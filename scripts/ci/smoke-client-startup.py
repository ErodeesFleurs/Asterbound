#!/usr/bin/env python3
"""Run the real client's no-assets -version path with present/absent output streams."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


TIMEOUT_SECONDS = 30
CASES = (
    ("redirected", ()),
    ("absent-stdout", (1,)),
    ("absent-stderr", (2,)),
    ("absent-both", (1, 2)),
)


def launch_posix(client, cwd, stdout, stderr, absent):
    # Popen installs the explicit destinations first; close only the requested
    # descriptors immediately before exec, not in the smoke runner itself.
    def close_outputs():
        for descriptor in absent:
            os.close(descriptor)

    process = subprocess.Popen(
        [str(client), "-version"], cwd=cwd, stdin=subprocess.DEVNULL,
        stdout=stdout, stderr=stderr, preexec_fn=close_outputs,
    )
    try:
        return process.wait(timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
        raise RuntimeError(f"timed out after {TIMEOUT_SECONDS}s (child killed and reaped)") from None


def launch_windows(client, cwd, stdout, stderr, absent):
    import ctypes
    from ctypes import wintypes
    import msvcrt

    class StartupInfo(ctypes.Structure):
        _fields_ = [
            ("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
            ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
            ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
            ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
            ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
            ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
            ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
            ("lpReserved2", ctypes.POINTER(wintypes.BYTE)),
            ("hStdInput", wintypes.HANDLE), ("hStdOutput", wintypes.HANDLE),
            ("hStdError", wintypes.HANDLE),
        ]

    class ProcessInfo(ctypes.Structure):
        _fields_ = [
            ("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
            ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD),
        ]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateProcessW.argtypes = [
        wintypes.LPCWSTR, wintypes.LPWSTR, ctypes.c_void_p, ctypes.c_void_p,
        wintypes.BOOL, wintypes.DWORD, ctypes.c_void_p, wintypes.LPCWSTR,
        ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInfo),
    ]
    kernel.CreateProcessW.restype = wintypes.BOOL
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetExitCodeProcess.restype = wintypes.BOOL
    kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.TerminateProcess.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL

    startup = StartupInfo()
    startup.cb = ctypes.sizeof(startup)
    startup.dwFlags = 0x100  # STARTF_USESTDHANDLES
    process = ProcessInfo()
    command = ctypes.create_unicode_buffer(subprocess.list2cmdline([str(client), "-version"]))
    with open(os.devnull, "rb") as stdin:
        handles = [msvcrt.get_osfhandle(stream.fileno()) for stream in (stdin, stdout, stderr)]
        startup.hStdInput = handles[0]
        startup.hStdOutput = None if 1 in absent else handles[1]
        startup.hStdError = None if 2 in absent else handles[2]
        # subprocess may replace NULL outputs with inherited handles. Native
        # DETACHED_PROCESS + STARTF_USESTDHANDLES passes these exact NULLs to the GUI.
        try:
            for handle in handles:
                os.set_handle_inheritable(handle, True)
            if not kernel.CreateProcessW(
                str(client), command, None, None, True, 0x8,  # DETACHED_PROCESS
                None, str(cwd), ctypes.byref(startup), ctypes.byref(process),
            ):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            for handle in handles:
                os.set_handle_inheritable(handle, False)

    kernel.CloseHandle(process.hThread)
    exited = False
    try:
        status = kernel.WaitForSingleObject(process.hProcess, TIMEOUT_SECONDS * 1000)
        if status == 0x102:  # WAIT_TIMEOUT
            raise RuntimeError(f"timed out after {TIMEOUT_SECONDS}s")
        if status != 0:  # WAIT_OBJECT_0
            raise ctypes.WinError(ctypes.get_last_error())
        exited = True
        code = wintypes.DWORD()
        if not kernel.GetExitCodeProcess(process.hProcess, ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        return code.value
    finally:
        try:
            if not exited:
                if not kernel.TerminateProcess(process.hProcess, 1):
                    raise ctypes.WinError(ctypes.get_last_error())
                if kernel.WaitForSingleObject(process.hProcess, 5000) != 0:
                    raise RuntimeError("could not reap timed-out client within 5s")
        finally:
            kernel.CloseHandle(process.hProcess)


def show_output(case, channel, contents):
    print(f"--- {case} {channel} ---", flush=True)
    print(contents.decode("utf-8", errors="replace") or "<empty>", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path, help="path to the built graphical client")
    args = parser.parse_args()
    client = args.client.resolve(strict=True)
    launch = launch_windows if os.name == "nt" else launch_posix
    failed = False
    with tempfile.TemporaryDirectory(prefix="asterbound-client-startup-") as directory:
        cwd = Path(directory)
        for case, absent in CASES:
            error = None
            with (cwd / f"{case}.stdout").open("w+b") as stdout, (cwd / f"{case}.stderr").open("w+b") as stderr:
                try:
                    code = launch(client, cwd, stdout, stderr, absent)
                    if code != 0:
                        error = f"exit {code} (0x{code & 0xffffffff:08x}), expected 0"
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as exception:
                    error = str(exception)
                stdout.seek(0)
                stderr.seek(0)
                output, diagnostic = stdout.read(), stderr.read()
            if case == "redirected" and error is None and not output.strip():
                error = "stdout was empty with redirected output"
            # Keep the baseline's actual output in CI logs, even on success.
            if case == "redirected" or error is not None:
                show_output(case, "stdout", output)
                show_output(case, "stderr", diagnostic)
            if error is not None:
                print(f"FAIL client startup [{case}]: {error}", file=sys.stderr, flush=True)
                failed = True
            else:
                print(f"PASS client startup [{case}]", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"Client startup smoke failed: {error}", file=sys.stderr)
        sys.exit(1)
