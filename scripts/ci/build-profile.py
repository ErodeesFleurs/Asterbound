#!/usr/bin/env python3
"""Build a CMake preset without intercepting its output, and save CI profiling artifacts."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time


INTERVAL = 0.25


class WindowsJob:
    """A gated launcher joins a kill-on-close job before it can spawn CMake."""

    def __init__(self):
        from ctypes import wintypes as w

        class IO(ctypes.Structure):
            _fields_ = [(name, ctypes.c_ulonglong) for name in (
                "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

        class BasicLimits(ctypes.Structure):
            _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                        ("PerJobUserTimeLimit", ctypes.c_longlong), ("LimitFlags", w.DWORD),
                        ("MinimumWorkingSetSize", ctypes.c_size_t), ("MaximumWorkingSetSize", ctypes.c_size_t),
                        ("ActiveProcessLimit", w.DWORD), ("Affinity", ctypes.c_size_t),
                        ("PriorityClass", w.DWORD), ("SchedulingClass", w.DWORD)]

        class Limits(ctypes.Structure):
            _fields_ = [("BasicLimitInformation", BasicLimits), ("IoInfo", IO),
                        ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                        ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

        class Accounting(ctypes.Structure):
            _fields_ = [(name, ctypes.c_longlong) for name in (
                "TotalUserTime", "TotalKernelTime", "ThisPeriodTotalUserTime", "ThisPeriodTotalKernelTime")]
            _fields_ += [(name, w.DWORD) for name in (
                "TotalPageFaultCount", "TotalProcesses", "ActiveProcesses", "TotalTerminatedProcesses")]

        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api.CreateJobObjectW.argtypes = [ctypes.c_void_p, w.LPCWSTR]
        self.api.CreateJobObjectW.restype = w.HANDLE
        self.api.SetInformationJobObject.argtypes = [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD]
        self.api.QueryInformationJobObject.argtypes = [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD, ctypes.c_void_p]
        self.api.AssignProcessToJobObject.argtypes = [w.HANDLE, w.HANDLE]
        self.api.TerminateJobObject.argtypes = [w.HANDLE, w.UINT]
        self.api.CloseHandle.argtypes = [w.HANDLE]
        self.handle = self.api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        self.limits_type, self.accounting_type = Limits, Accounting
        limits = Limits()
        limits.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        try:
            self.check(self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)))
        except BaseException:
            self.close()
            raise

    @staticmethod
    def check(result):
        if not result:
            raise ctypes.WinError(ctypes.get_last_error())

    def assign(self, process):
        self.check(self.api.AssignProcessToJobObject(self.handle, int(process._handle)))

    def sample(self):
        limits, accounting = self.limits_type(), self.accounting_type()
        for kind, value in ((9, limits), (1, accounting)):
            self.check(self.api.QueryInformationJobObject(
                self.handle, kind, ctypes.byref(value), ctypes.sizeof(value), None))
        return {"job_peak_private_committed_bytes": limits.PeakJobMemoryUsed,
                "processes": accounting.ActiveProcesses,
                "job_cpu_seconds": (accounting.TotalUserTime + accounting.TotalKernelTime) / 10000000}

    def terminate(self):
        self.check(self.api.TerminateJobObject(self.handle, 1))

    def close(self):
        if self.handle:
            self.api.CloseHandle(self.handle)
            self.handle = None


def process_tree_sample(root_pid):
    rows = {}
    if sys.platform.startswith("linux"):
        page_size = os.sysconf("SC_PAGE_SIZE")
        for entry in Path("/proc").iterdir():
            if not entry.name.isdecimal():
                continue
            try:
                # comm may itself contain spaces and ')'; fields after its last ')' are stable.
                fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
                rows[int(entry.name)] = (int(fields[1]), int(fields[21]) * page_size)
            except (OSError, ValueError, IndexError):
                continue  # Process exited between enumeration and sampling.
    elif sys.platform == "darwin":
        result = subprocess.run(["ps", "-axo", "pid=,ppid=,rss="], check=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5)
        for line in result.stdout.splitlines():
            pid, parent, rss = map(int, line.split())
            rows[pid] = (parent, rss * 1024)
    else:
        raise RuntimeError("process-tree memory sampling is unavailable on this platform")
    if root_pid not in rows:
        raise RuntimeError("build process exited before this sample")
    children = {}
    for pid, (parent, _) in rows.items():
        children.setdefault(parent, []).append(pid)
    pending, descendants = [root_pid], set()
    while pending:
        pid = pending.pop()
        if pid not in descendants:
            descendants.add(pid)
            pending.extend(children.get(pid, ()))
    return {"tree_rss_bytes": sum(rows[pid][1] for pid in descendants), "processes": len(descendants)}


def ninja_expand(text, variables):
    return re.sub(r"\$(?:([ $:])|\{([^}]+)\}|([\w.-]+))",
                  lambda match: match[1] if match[1] is not None
                  else variables.get(match[2] or match[3], ""), text)


def ninja_tokens(text, variables):
    """Tokenize paths, respecting Ninja's escaped spaces, colons and dollars."""
    tokens, word = [], []
    index = 0
    while index < len(text):
        char = text[index]
        if char == "$" and index + 1 < len(text):
            index += 1
            char = text[index]
            if char in " $:":
                word.append(char)
            else:
                match = re.match(r"\{([^}]+)\}|([\w.-]+)", text[index:])
                if match:
                    word.append(variables.get(match[1] or match[2], ""))
                    index += len(match[0]) - 1
                else:
                    word.append("$")
                    word.append(char)
        elif char.isspace() or char in ":|":
            if word:
                tokens.append("".join(word))
                word = []
            if char in ":|":
                tokens.append(char)
        else:
            word.append(char)
        index += 1
    if word:
        tokens.append("".join(word))
    return tokens


def link_edges(build_dir):
    edges, variables, visited = {}, {}, set()

    def load(path):
        path = path.resolve()
        if path in visited:
            return
        visited.add(path)
        text = path.read_text(encoding="utf-8", errors="replace")
        # Odd trailing dollars continue a logical line; $$ is a literal dollar.
        text = re.sub(r"(?<!\$)(?:\$\$)*\$\r?\n[ \t]*",
                      lambda m: m[0].split("\n")[0].rstrip("\r")[:-1], text)
        for line in text.splitlines():
            if not line or line[0].isspace() or line.startswith("#"):
                continue
            if line.startswith(("include ", "subninja ")):
                names = ninja_tokens(line.split(" ", 1)[1], variables)
                if names:
                    # Ninja resolves includes relative to its working directory.
                    load(build_dir / names[0])
            elif line.startswith("build "):
                tokens = ninja_tokens(line[6:], variables)
                if ":" not in tokens:
                    continue
                separator = tokens.index(":")
                outputs, rule = tokens[:separator], tokens[separator + 1]
                if "_LINKER" not in rule and "_ARCHIVER" not in rule:
                    continue
                kind = "archive" if "STATIC_LIBRARY" in rule or "_ARCHIVER" in rule else "link"
                outputs = [name for name in outputs if name != "|"]
                edge = {"output": outputs[0], "rule": rule, "kind": kind}
                for name in outputs:
                    edges[os.path.normcase(os.path.normpath(str(build_dir / name)))] = edge
            elif "=" in line:
                name, value = line.split("=", 1)
                variables[name.strip()] = ninja_expand(value.strip(), variables)

    load(build_dir / "build.ninja")
    return edges


def read_log(path):
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return b""


def link_summary(build_dir, before, after):
    notes = []
    if not after:
        return [], ["No .ninja_log exists; no successful Ninja edge timings are available."]
    if after.startswith(before):
        current = after[len(before):].splitlines()
    else:
        old = set(before.splitlines())
        current = [line for line in after.splitlines() if line not in old]
        notes.append("Ninja rewrote/recompacted its log; current edges identified by full-record difference.")
    try:
        edges = link_edges(build_dir)
    except (OSError, ValueError, IndexError) as error:
        return [], notes + [f"Ninja link metadata unavailable: {error}"]
    timings, seen = [], set()
    for line in current:
        fields = line.decode("utf-8", errors="replace").split("\t")
        if len(fields) < 5 or line.startswith(b"#"):
            continue
        try:
            start, end = int(fields[0]), int(fields[1])
        except ValueError:
            continue
        key = os.path.normcase(os.path.normpath(str(build_dir / fields[3])))
        edge = edges.get(key)
        if not edge:
            continue
        identity = (edge["output"], start, end)
        if identity in seen:
            continue  # Ninja logs each output of a multi-output edge separately.
        seen.add(identity)
        timings.append({**edge, "start_seconds": start / 1000, "end_seconds": end / 1000,
                        "elapsed_seconds": (end - start) / 1000})
    notes.append("Only successful edges logged during this invocation are timed; failed/interrupted links are absent.")
    return sorted(timings, key=lambda row: row["elapsed_seconds"], reverse=True), notes


def sccache_build(command, output_dir):
    """Keep the server and its compiler children inside the profiled group/job."""
    env = os.environ.copy()
    for name in ("SCCACHE_START_SERVER", "SCCACHE_STARTUP_NOTIFY", "SCCACHE_SERVER_UDS", "SCCACHE_SERVER_PORT"):
        env.pop(name, None)
    with tempfile.TemporaryDirectory(prefix="ast-scc-", dir=None if os.name == "nt" else "/tmp") as temporary:
        if os.name == "nt":
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                address = ("127.0.0.1", reservation.getsockname()[1])
            env["SCCACHE_SERVER_PORT"] = str(address[1])
        else:
            address = str(Path(temporary) / "server.sock")
            env["SCCACHE_SERVER_UDS"] = address
        # v0.17.0 cmdline.rs selects InternalStartServer with START_SERVER=1
        # and no arguments. util.rs skips daemonization only for NO_DAEMON=1.
        env.update(SCCACHE_NO_DAEMON="1", SCCACHE_IDLE_TIMEOUT="0")
        server_env = {**env, "SCCACHE_START_SERVER": "1"}
        server_env.pop("SCCACHE_CLIENT_SIDE", None)
        server = subprocess.Popen(["sccache"], env=server_env)
        build = None
        state = {"server_pid": server.pid, "address": address, "state": "starting",
                 "temporary_dir": temporary, "stats_available": False}
        state_path = output_dir / "sccache-server.json"
        state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        (output_dir / "sccache.json").unlink(missing_ok=True)
        try:
            deadline = time.monotonic() + 10
            while True:
                if server.poll() is not None:
                    raise RuntimeError(f"Foreground sccache exited during startup: {server.returncode}")
                try:
                    family = socket.AF_INET if os.name == "nt" else socket.AF_UNIX
                    with socket.socket(family, socket.SOCK_STREAM) as probe:
                        probe.settimeout(0.1)
                        probe.connect(address)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Foreground sccache did not become ready within 10 seconds")
                    time.sleep(0.05)
            state["state"] = "running"
            state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
            build = subprocess.Popen(command, env=env)
            while build.poll() is None:
                if server.poll() is not None:
                    raise RuntimeError(f"Foreground sccache exited during the build: {server.returncode}")
                time.sleep(0.1)
            state["build_returncode"] = build.returncode
            return build.returncode
        finally:
            # The outer profiler kills the entire group/job on cancellation or
            # worker exit, including compilers which outlive a crashed server.
            if build is not None and build.poll() is None:
                build.terminate()
            try:
                if server.poll() is None:
                    stats = subprocess.run(["sccache", "--show-stats", "--stats-format=json"],
                                           env=env, stdout=subprocess.PIPE, text=True, timeout=3)
                    if stats.returncode == 0:
                        (output_dir / "sccache.json").write_text(stats.stdout, encoding="utf-8")
                        print(f"sccache statistics: {stats.stdout}", flush=True)
                        state["stats_available"] = True
                    subprocess.run(["sccache", "--stop-server"], env=env, timeout=3)
                    server.wait(timeout=3)
            except (OSError, subprocess.SubprocessError) as error:
                state["shutdown_note"] = str(error)
            finally:
                if server.poll() is None:
                    server.kill()
                server.wait()
                state["state"] = "stopped"
                state["server_returncode"] = server.returncode
                state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")


def run_build(command, cwd, sample_file, managed_sccache=False):
    started = time.monotonic()
    process, job, cancellation, first_cancel, sent_signal = None, None, None, None, False
    samples, peaks, errors = 0, {}, set()
    old_handlers = {}

    def cancel(signum, _frame):
        nonlocal cancellation, first_cancel
        cancellation = signum
        if first_cancel is None:
            first_cancel = time.monotonic()
        else:
            first_cancel = 0  # A second signal skips the grace period.

    for signum in (signal.SIGINT, signal.SIGTERM):
        old_handlers[signum] = signal.signal(signum, cancel)
    try:
        if os.name == "nt":
            job = WindowsJob()
            gate = ("import os,subprocess,sys; "
                    "token=sys.stdin.buffer.read(1); "
                    "code=subprocess.call(sys.argv[1:]) if token == b'1' else 1; "
                    "os._exit(code if code < 2147483648 else code - 4294967296)")
            process = subprocess.Popen([sys.executable, "-c", gate, *command], cwd=cwd,
                                       stdin=subprocess.PIPE, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            job.assign(process)
            process.stdin.write(b"1")
            process.stdin.close()
        else:
            process = subprocess.Popen(command, cwd=cwd, start_new_session=True)
        while True:
            try:
                sample = job.sample() if job else process_tree_sample(process.pid)
                samples += 1
                for metric, value in sample.items():
                    peaks[metric] = max(peaks.get(metric, 0), value)
                sample_file.write(json.dumps({"elapsed_seconds": time.monotonic() - started, **sample}) + "\n")
                sample_file.flush()
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
                errors.add(str(error))
            if cancellation:
                if job:
                    job.terminate()
                else:
                    try:
                        if not sent_signal:
                            os.killpg(process.pid, cancellation)
                        elif time.monotonic() - first_cancel >= 5:
                            os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                sent_signal = True
            status = process.poll()
            if status is not None:
                break
            time.sleep(INTERVAL)
        return {"returncode": status, "cancelled_signal": cancellation,
                "elapsed_seconds": time.monotonic() - started, "sample_interval_seconds": INTERVAL,
                "resource_samples": samples, "sampled_peaks": peaks or None,
                "resource_notes": sorted(errors)}
    finally:
        if process is not None:
            # Also clean up on profiler exceptions, and kill descendants if the leader
            # exited before all of its children acknowledged cancellation.
            if job:
                job.close()
            elif process.poll() is None or cancellation or managed_sccache:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if process.stdin is not None and not process.stdin.closed:
                process.stdin.close()
            if process.poll() is None:
                process.kill()
            process.wait()
        elif job:
            job.close()
        for signum, handler in old_handlers.items():
            signal.signal(signum, handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", required=True)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--sccache", action="store_true",
                        help="profile a private foreground sccache server and its compiler children")
    args = parser.parse_args()
    build_dir, output_dir = args.build_dir.resolve(), args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    # CMake presets live in source/, not the repository root or caller's cwd.
    cwd = Path(__file__).resolve().parents[2] / "source"
    command = ["cmake", "--build", "--preset", args.preset]
    profiled_command = command
    if args.sccache:
        profiled_command = [sys.executable, str(Path(__file__).resolve()), "--_sccache-worker",
                            json.dumps(command), str(output_dir)]
    before = read_log(build_dir / ".ninja_log")
    (output_dir / "ninja-before.log").write_bytes(before)
    print(f"Build profile: {command!r}, cwd={cwd}", flush=True)
    with (output_dir / "resource-samples.jsonl").open("w", encoding="utf-8") as samples:
        result = run_build(profiled_command, cwd, samples, managed_sccache=args.sccache)
    if args.sccache:
        state_path = output_dir / "sccache-server.json"
        if state_path.exists():
            state = json.loads(state_path.read_text(encoding="utf-8"))
            if state["state"] != "stopped":
                state["state"] = "terminated-with-profile-group"
                state["stats_available"] = (output_dir / "sccache.json").exists()
                state["stats_note"] = "Worker interrupted; any collected statistics may be partial."
                try:
                    if os.name != "nt":
                        Path(state["address"]).unlink(missing_ok=True)
                    Path(state["temporary_dir"]).rmdir()
                except FileNotFoundError:
                    pass
                except OSError as error:
                    state["cleanup_note"] = str(error)
                state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
            result["sccache_server"] = state
    after = read_log(build_dir / ".ninja_log")
    # An empty snapshot also replaces stale artifacts if an output directory is reused.
    (output_dir / "ninja.log").write_bytes(after)
    if (build_dir / "build.ninja").exists():
        (output_dir / "build.ninja").write_bytes((build_dir / "build.ninja").read_bytes())
    else:
        (output_dir / "build.ninja").unlink(missing_ok=True)
    timings, notes = link_summary(build_dir, before, after)
    result.update({"command": command, "cwd": str(cwd), "build_dir": str(build_dir),
                   "sccache_managed": args.sccache,
                   "link_timings": timings, "link_timing_notes": notes,
                   "resource_semantics": {
                       "tree_rss_bytes": "Sampled sum of live descendant RSS, including CMake; shared pages can be counted more than once. Not an exact high-water mark.",
                       "job_peak_private_committed_bytes": "Windows Job Object peak aggregate private committed memory, including the gated launcher; not RSS.",
                       "job_cpu_seconds": "Windows job cumulative user plus kernel CPU time, including exited descendants.",
                       "unavailable": "Missing metrics are unavailable, never zero; short-lived processes between samples can be missed. No CPU metric is collected on POSIX."}})
    (output_dir / "build-profile.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [f"Build elapsed: {result['elapsed_seconds']:.3f}s; return code: {result['returncode']}",
             f"Resource samples: {result['resource_samples']}; peaks: {result['sampled_peaks']}",
             "Link/archive timings for this invocation (descending; concurrent durations are not additive wall time):"]
    lines += [f"{row['elapsed_seconds']:10.3f}s  {row['kind']:7s}  {row['output']}  [{row['rule']}]" for row in timings]
    if not timings:
        lines.append("No completed link/archive edges recorded for this invocation.")
    lines += notes
    lines += [f"Resource note: {note}" for note in result["resource_notes"]]
    summary = "\n".join(lines) + "\n"
    (output_dir / "link-timings.txt").write_text(summary, encoding="utf-8")
    print(summary, flush=True)
    signum = result["cancelled_signal"]
    if signum is None and result["returncode"] < 0 and os.name != "nt":
        signum = -result["returncode"]
    if signum is not None:
        if os.name != "nt":
            signal.signal(signum, signal.SIG_DFL)
            os.kill(os.getpid(), signum)
        return 128 + signum
    return result["returncode"]


if __name__ == "__main__":
    try:
        if len(sys.argv) > 1 and sys.argv[1] == "--_sccache-worker":
            exit_code = sccache_build(json.loads(sys.argv[2]), Path(sys.argv[3]))
        else:
            exit_code = main()
        if exit_code < 0 and os.name != "nt":
            signal.signal(-exit_code, signal.SIG_DFL)
            os.kill(os.getpid(), -exit_code)
        # Windows exception/status codes occupy the full unsigned DWORD range.
        sys.exit(exit_code - 4294967296 if os.name == "nt" and exit_code > 2147483647 else exit_code)
    except (OSError, RuntimeError) as error:
        print(f"Build profiler failed: {error}", file=sys.stderr)
        sys.exit(2)
