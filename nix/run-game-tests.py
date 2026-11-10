#!@python@
"""Run packaged game_tests with private per-case runtime state and reports.

ASTERBOUND_TEST_JOBS defaults to 1: asset loading and sanitizer heaps are
large. Opt in to bounded parallelism (e.g. 4) only with sufficient memory.
Arguments select the existing single-process, streamed-output interface.
"""

import asyncio
import fcntl
import hashlib
import json
import os
import re
import signal
import sys
import tempfile
from contextlib import asynccontextmanager
from pathlib import Path
from urllib.parse import quote

GAME_TESTS = "@gameTests@"
BUNDLED_ASSETS = ("@bundledAssets@/asterbound", "@bundledAssets@/asterbound-compat")
# Only this test binds a fixed TCP port. TestUniverse/ServerTest leave TCP
# listening disabled; the other connection tests bind ephemeral port 0.
FIXED_PORT_CASE = "UniverseConnections.All"


def announce(message):
    print(message, flush=True)


def duration(value):
    match = re.fullmatch(r"([0-9]+(?:\.[0-9]+)?)([smhd]?)", value)
    if not match:
        raise ValueError("ASTERBOUND_TEST_TIMEOUT must be a nonnegative duration (s/m/h/d)")
    return float(match[1]) * {"": 1, "s": 1, "m": 60, "h": 3600, "d": 86400}[match[2]]


def selects_fixed_port(arguments):
    # GoogleTest filters use only '*'/'?' wildcards, ':' alternatives, and a
    # '-' exclusion suffix. The last command-line filter overrides GTEST_FILTER.
    # A flagfile may supply another filter; do not interpret/rewrite that input.
    if os.environ.get("GTEST_FLAGFILE") or any(arg.startswith("--gtest_flagfile=") for arg in arguments):
        return True
    selected = os.environ.get("GTEST_FILTER", "*")
    for argument in arguments:
        if argument.startswith("--gtest_filter="):
            selected = argument.split("=", 1)[1]
    positive, separator, negative = selected.partition("-")

    def matches(patterns):
        return any(re.fullmatch(re.escape(pattern).replace(r"\*", ".*").replace(r"\?", "."),
                                FIXED_PORT_CASE) is not None for pattern in patterns.split(":"))

    return matches(positive or ("*" if separator else "")) and not (separator and matches(negative))


@asynccontextmanager
async def fixed_port_lease(required):
    if not required:
        yield
        return
    # Stable across report directories and runner invocations of this uid. Do
    # not unlink: a waiter must never lock a different inode. This coordinates
    # runners, not unrelated programs already using port 55555 (which must fail).
    # Retain OpenStarbound's lock so both products still serialize this port.
    path = f"/tmp/openstarbound-game-tests-{os.getuid()}-port-55555.lock"
    fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                await asyncio.sleep(0.1)
        yield
    finally:
        os.close(fd)


async def stop_group(process):
    # Every child starts its own session. Never signal the caller's process
    # group, and also clean up descendants when the direct child exits first.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        await process.wait()
        return
    try:
        await asyncio.wait_for(process.wait(), 2)
    except TimeoutError:
        pass
    # The leader may have exited while a grandchild still owns its pipes.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    await process.wait()


async def run_process(arguments, log_path, timeout, environment, stream=False):
    process = None
    with log_path.open("wb") as log:
        try:
            # Shield spawning so a signal in create_subprocess_exec cannot lose
            # the handle to a child that has already been forked.
            spawning = asyncio.create_task(asyncio.create_subprocess_exec(
                GAME_TESTS, *arguments, stdout=asyncio.subprocess.PIPE if stream else log,
                stderr=asyncio.subprocess.STDOUT, env=environment, start_new_session=True,
            ))
            try:
                process = await asyncio.shield(spawning)
            except asyncio.CancelledError:
                process = await spawning
                raise

            async def finish():
                if stream:
                    while chunk := await process.stdout.read(65536):
                        log.write(chunk)
                        log.flush()
                        sys.stdout.buffer.write(chunk)
                        sys.stdout.buffer.flush()
                return await process.wait()

            try:
                status = await asyncio.wait_for(finish(), timeout) if timeout else await finish()
                return status if status >= 0 else 128 - status
            except TimeoutError:
                return 124
        except OSError as error:
            log.write(f"asterbound: could not run game_tests: {error}\n".encode())
            if stream:
                announce(f"asterbound: could not run game_tests: {error}")
            return 127
        finally:
            if process is not None:
                await stop_group(process)


def runtime(directory, vanilla):
    directory.mkdir(parents=True, exist_ok=True)
    for name in ("storage", "logs", "tmp"):
        (directory / name).mkdir(exist_ok=True)
    boot = directory / "sbinit.config"
    boot.write_text(json.dumps({
        "assetSources": [str(vanilla), *BUNDLED_ASSETS],
        "storageDirectory": str(directory / "storage"),
        "logDirectory": str(directory / "logs"),
    }, indent=2) + "\n")
    # Native POSIX runner only; do not pass POSIX TEMP paths to Wine. Keep
    # HOME/XDG intact so the real configuration inputs do not change. Star's
    # native File helpers still use P_tmpdir (unique entropy-seeded names).
    temporary = str(directory / "tmp")
    environment = dict(os.environ, TMPDIR=temporary, TMP=temporary, TEMP=temporary)
    return boot, environment


def discover(text):
    suite = None
    cases = []
    for line in text.splitlines():
        name = line.split("#", 1)[0].rstrip()
        if not name:
            continue
        if not name[0].isspace():
            suite = name if name.endswith(".") and not any(c.isspace() for c in name) else None
        elif suite and name.startswith("  ") and len(name.split()) == 1:
            cases.append(suite + name.strip())
    if len(cases) != len(set(cases)):
        raise ValueError("game_tests discovery returned duplicate case names")
    return cases


async def sweep(out, vanilla, timeout, timeout_label, jobs):
    runtime_root = Path(tempfile.mkdtemp(prefix="run-", dir=out / "runtime"))
    boot, environment = runtime(runtime_root / "discovery", vanilla)
    discovery_log = out / "discovery.log"
    status = await run_process(["--gtest_list_tests", "-bootconfig", str(boot)],
                               discovery_log, timeout, environment)
    names = discover(discovery_log.read_text(errors="replace")) if status == 0 else []
    (out / "cases.list").write_text("".join(name + "\n" for name in names))
    if status or not names:
        (out / "cases.txt").write_text(f"{'case':<46} result\n")
        return 1, f"discovery failed (exit {status}, cases: {len(names)}); see discovery.log\n"

    # Remove only reports owned by this discovered sweep. A case that crashes
    # before producing JSON must not inherit a previous run's successful JSON.
    for name in names:
        (out / "cases" / (quote(name, safe="._-") + ".json")).unlink(missing_ok=True)
    results = [None] * len(names)
    pending = iter(enumerate(names))

    async def worker():
        for index, name in pending:
            filename = quote(name, safe="._-")
            boot, environment = runtime(runtime_root / filename, vanilla)
            async with fixed_port_lease(name == FIXED_PORT_CASE):
                announce(f"asterbound: start [{index + 1}/{len(names)}] {name}")
                status = await run_process([
                    "-bootconfig", str(boot), "--gtest_filter=" + name,
                    "--gtest_output=json:" + str(out / "cases" / (filename + ".json")),
                ], out / "cases" / (filename + ".log"), timeout, environment)
            verdict = ("pass" if status == 0 else f"timeout ({timeout_label}s)"
                       if status == 124 else f"fail (exit {status})")
            results[index] = (status, f"{name:<46} {verdict}\n")
            announce(f"asterbound: end   [{index + 1}/{len(names)}] {name}: {verdict}")

    workers = [asyncio.create_task(worker()) for _ in range(min(jobs, len(names)))]
    try:
        await asyncio.gather(*workers)
    finally:
        for worker_task in workers:
            worker_task.cancel()
        await asyncio.gather(*workers, return_exceptions=True)
        # Keep completed cases in discovery order even on interruption. Never
        # describe an unstarted/cancelled case as passed.
        lines = [f"{'case':<46} result\n"]
        lines.extend(result[1] if result else f"{name:<46} interrupted/not run\n"
                     for name, result in zip(names, results))
        (out / "cases.txt").write_text("".join(lines))
    passed = sum(result[0] == 0 for result in results)
    failed = len(names) - passed
    return int(failed != 0), (f"cases: {len(names)}, passed: {passed}, not passed: {failed}\n\n"
                              + "".join(lines))


async def main(arguments):
    timeout_label = os.environ.get("ASTERBOUND_TEST_TIMEOUT") or "300"
    timeout = duration(timeout_label)
    jobs = int(os.environ.get("ASTERBOUND_TEST_JOBS") or "1")
    if jobs < 1:
        raise ValueError("ASTERBOUND_TEST_JOBS must be a positive integer")
    runtime_root = Path(os.environ.get("ASTERBOUND_RUNTIME_DIR") or
                        str(Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")
                            / "asterbound"))
    assets = Path(os.environ.get("ASTERBOUND_ASSETS") or str(runtime_root / "assets")).absolute()
    vanilla = assets / "packed.pak" if (assets / "packed.pak").is_file() else assets
    if not assets.is_dir():
        announce(f"asterbound: no Starbound assets in {assets}")
        announce("asterbound: point ASTERBOUND_ASSETS at a Starbound assets directory")
        return 2
    out = Path(os.environ.get("ASTERBOUND_TEST_OUT") or "game-tests-report").absolute()
    for directory in (out, out / "cases", out / "runtime"):
        directory.mkdir(parents=True, exist_ok=True)
    asset_identity = str(vanilla)
    if vanilla.is_file():
        digest = hashlib.sha256()
        with vanilla.open("rb") as pak:
            while chunk := pak.read(1024 * 1024):
                digest.update(chunk)
        asset_identity += f" ({digest.hexdigest()[:16]}…)"
    header_arguments = " ".join(arguments) if arguments else "<all cases, one process each>"
    header = (f"assets: {asset_identity}\narguments: {header_arguments}\n"
              f"timeout: {timeout_label}s\n")
    announce("asterbound: game_tests")
    announce(f"  assets : {asset_identity}\n  report : {out}\n  timeout: {timeout_label}s per run")
    if not arguments:
        announce(f"  jobs   : {jobs}")
    status, body = 1, "runner did not finish\n"
    try:
        if arguments:
            boot, environment = runtime(out, vanilla)
            (out / "game-tests.json").unlink(missing_ok=True)
            # Reserve the fixed-port resource only when the caller's unmodified
            # GTest filter can actually select that test.
            async with fixed_port_lease(selects_fixed_port(arguments)):
                status = await run_process([
                    "-bootconfig", str(boot), "--gtest_output=json:" + str(out / "game-tests.json"),
                    *arguments,
                ], out / "game-tests.log", timeout, environment, stream=True)
            (out / "exit-status").write_text(str(status) + "\n")
            log = (out / "game-tests.log").read_text(errors="replace")
            lines = log.splitlines(keepends=True)
            start = next((i for i, line in enumerate(lines) if line.startswith("[  PASSED  ]")), len(lines))
            body = "".join(lines[start:])
            if status == 124:
                announce(f"asterbound: stopped after {timeout_label}s")
        else:
            status, body = await sweep(out, vanilla, timeout, timeout_label, jobs)
    except asyncio.CancelledError:
        status = 128 + interrupted_signal
        body = "interrupted; see cases.txt and per-process logs\n"
        if arguments:
            (out / "exit-status").write_text(str(status) + "\n")
        raise
    finally:
        summary = header + f"exit status: {status}\n\n" + body
        (out / "summary.txt").write_text(summary)
        announce(f"asterbound: summary in {out / 'summary.txt'}")
        announce(summary.rstrip())
    return status


interrupted_signal = 0


async def entry(arguments):
    loop = asyncio.get_running_loop()
    task = asyncio.current_task()

    def interrupt(signum):
        global interrupted_signal
        if not interrupted_signal:
            interrupted_signal = signum
            task.cancel()

    for signum in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(signum, interrupt, signum)
    try:
        return await main(arguments)
    except asyncio.CancelledError:
        return 128 + interrupted_signal


if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(entry(sys.argv[1:])))
    except (OSError, ValueError) as error:
        print(f"asterbound: {error}", file=sys.stderr, flush=True)
        sys.exit(2)
