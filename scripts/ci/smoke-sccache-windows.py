#!/usr/bin/env python3
"""Exercise oversized incoming Clang rsp commands and real sccache hits on Windows."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time


def gnu_quote(argument):
    return '"' + argument.replace("\\", "\\\\").replace('"', '\\"') + '"'


def run(command, *, cwd, env):
    return subprocess.run(command, cwd=cwd, env=env, check=True, timeout=120,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, encoding="utf-8", errors="replace")


def stats(sccache, work, env):
    result = run([sccache, "--show-stats", "--stats-format=json"], cwd=work, env=env)
    data = json.loads(result.stdout)["stats"]
    return (sum(data["cache_hits"]["counts"].values()),
            sum(data["cache_misses"]["counts"].values()))


def exercise(sccache, compiler, work, env):
    include = work / "include space café"
    include.mkdir()
    (include / "from-path.h").write_text("#define FROM_PATH 17\n", encoding="utf-8")
    (include / "from-env.h").write_text("#define FROM_ENV 19\n", encoding="utf-8")
    env["CPLUS_INCLUDE_PATH"] = str(include)
    literal = r'"space \"quote\" slash \\ café \\"'
    source = work / "source café.cpp"
    source.write_text(
        '#include "from-path.h"\n#include "from-env.h"\n'
        'constexpr char actual[] = SMOKE_TEXT;\n'
        f'constexpr char expected[] = {literal};\n'
        'constexpr bool equal() {\n'
        '  if (sizeof(actual) != sizeof(expected)) return false;\n'
        '  for (unsigned i = 0; i < sizeof(actual); ++i)\n'
        '    if (actual[i] != expected[i]) return false;\n'
        '  return true;\n}\n'
        'static_assert(equal());\n'
        'static_assert(FROM_PATH == 17 && FROM_ENV == 19);\n'
        'int main() { return SMOKE_VALUE; }\n', encoding="utf-8")
    object_file = work / "result space.o"
    executable = work / "result.exe"
    response = work / "incoming café.rsp"
    arguments = ["--target=x86_64-w64-mingw32", "-std=c++23", "-fno-exceptions", "-fno-rtti",
                 "-c", source.name, "-o", object_file.name, "-I" + str(include) + "\\",
                 "-DSMOKE_TEXT=" + literal]
    arguments += [f"-DSMOKE_PADDING_{i:04d}=" + "abcdefghij" * 7 for i in range(650)]
    if len(subprocess.list2cmdline([compiler, *arguments])) <= 32767:
        raise RuntimeError("smoke arguments did not exceed the Windows spawn limit")

    def compile_value(value):
        response.write_text("\n".join(gnu_quote(arg) for arg in
                            [*arguments, f"-DSMOKE_VALUE={value}"]) + "\n", encoding="utf-8")
        run([sccache, compiler, "@" + response.name], cwd=work, env=env)
        if not object_file.is_file():
            raise RuntimeError("Clang did not produce the requested object")
        return hashlib.sha256(object_file.read_bytes()).hexdigest()

    def execute_value(value):
        run([compiler, "--target=x86_64-w64-mingw32", str(object_file), "-o", str(executable)],
            cwd=work, env=env)
        result = subprocess.run([str(executable)], cwd=work, env=env, timeout=30)
        if result.returncode != value:
            raise RuntimeError(f"compiled program returned {result.returncode}, expected {value}")

    first = compile_value(1)
    if stats(sccache, work, env) != (0, 1):
        raise RuntimeError("first oversized compilation was not a genuine cache miss")
    execute_value(1)
    object_file.unlink()
    restored = compile_value(1)
    if restored != first or stats(sccache, work, env) != (1, 1):
        raise RuntimeError("deleted object was not restored byte-for-byte by a genuine cache hit")
    execute_value(1)
    object_file.unlink()
    changed = compile_value(2)
    if changed == first or stats(sccache, work, env) != (1, 2):
        raise RuntimeError("changed macro did not invalidate the cached compilation")
    execute_value(2)
    print("Windows Clang >32K rsp: compile, quoted Unicode arguments, cache restoration and macro invalidation passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("this runtime smoke requires native Windows")
    compiler = str(args.compiler.resolve(strict=True))
    sccache = shutil.which("sccache")
    if sccache is None:
        raise RuntimeError("sccache is not on PATH")
    with tempfile.TemporaryDirectory(prefix="asterbound-sccache-smoke-") as temporary:
        work = Path(temporary)
        env = os.environ.copy()
        for name in ("SCCACHE_START_SERVER", "SCCACHE_STARTUP_NOTIFY", "SCCACHE_SERVER_UDS",
                     "SCCACHE_SERVER_PORT", "SCCACHE_CLIENT_SIDE"):
            env.pop(name, None)
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            address = ("127.0.0.1", reservation.getsockname()[1])
        configuration = work / "sccache.conf"
        configuration.write_text("", encoding="utf-8")
        env.update(SCCACHE_SERVER_PORT=str(address[1]), SCCACHE_DIR=str(work / "cache"),
                   SCCACHE_CONF=str(configuration), SCCACHE_NO_DAEMON="1",
                   SCCACHE_IDLE_TIMEOUT="0", SCCACHE_DIRECT="false", SCCACHE_CACHE_SIZE="32M")
        # Match build-profile.py's private foreground server, never the default daemon.
        server = subprocess.Popen([sccache], env={**env, "SCCACHE_START_SERVER": "1"})
        try:
            deadline = time.monotonic() + 10
            while True:
                if server.poll() is not None:
                    raise RuntimeError(f"private sccache exited during startup: {server.returncode}")
                try:
                    with socket.create_connection(address, timeout=0.1):
                        break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("private sccache did not become ready within 10 seconds")
                    time.sleep(0.05)
            exercise(sccache, compiler, work, env)
        finally:
            if server.poll() is None:
                try:
                    run([sccache, "--stop-server"], cwd=work, env=env)
                    server.wait(timeout=10)
                finally:
                    if server.poll() is None:
                        server.kill()
                        server.wait(timeout=10)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stdout or "", end="")
        print(error.stderr or "", end="")
        raise
