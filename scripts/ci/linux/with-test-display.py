#!/usr/bin/env python3
"""Run a command on a fresh authenticated Xvfb, never the invoking desktop."""

import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile


def run(command, root):
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET", "NIRI_SOCKET",
                "ASTERBOUND_RENDER_TEST_DISPLAY", "DBUS_STARTER_ADDRESS", "DBUS_STARTER_BUS_TYPE"):
        env.pop(key, None)
    for key in ("HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "XDG_RUNTIME_DIR"):
        directory = root / key
        directory.mkdir(mode=0o700)
        env[key] = str(directory)
    # Do not autolaunch a bus: service activation can invoke desktop hooks.
    env.update(DBUS_SESSION_BUS_ADDRESS="unix:path=/dev/null", DBUS_SYSTEM_BUS_ADDRESS="unix:path=/dev/null")

    authority = root / "Xauthority"
    # Xauthority FamilyWild, empty address and display number: Xvfb chooses
    # the number itself. The random cookie is valid only on this server.
    fields = (b"", b"", b"MIT-MAGIC-COOKIE-1", os.urandom(16))
    authority.write_bytes(b"\xff\xff" + b"".join(len(f).to_bytes(2, "big") + f for f in fields))
    env["XAUTHORITY"] = str(authority)
    server = subprocess.Popen(["Xvfb", "-displayfd", "1", "-screen", "0", "1400x1000x24",
                               "-noreset", "-nolisten", "tcp", "-auth", authority],
                              env=env, stdout=subprocess.PIPE)
    try:
        if not select.select([server.stdout], [], [], 30)[0]:
            raise SystemExit("Xvfb did not publish a display within 30 seconds")
        number = server.stdout.readline().strip()
        if not number.isdigit():
            raise SystemExit("Xvfb failed to allocate a display")
        display = ":" + number.decode("ascii")
        lease = root / "display"
        lease.write_text(display + "\n", encoding="ascii")
        env.update(DISPLAY=display, ASTERBOUND_RENDER_TEST_DISPLAY=str(lease))
        result = subprocess.run(command, env=env).returncode
        return result if result >= 0 else 128 - result
    finally:
        server.stdout.close()
        server.terminate()
        server.wait()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: with-test-display.py COMMAND [ARG ...]")
    def terminate(signum, _frame):
        raise SystemExit(128 + signum)
    signal.signal(signal.SIGTERM, terminate)
    with tempfile.TemporaryDirectory(prefix="asterbound-xvfb-") as temporary:
        sys.exit(run(sys.argv[1:], Path(temporary).resolve()))
