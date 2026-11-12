#!/usr/bin/env python3
"""Exercise the Nix cache tar exclusions on an unprivileged Linux filesystem."""
import contextlib
import importlib.util
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile


spec = importlib.util.spec_from_file_location(
    "cache_state", Path(__file__).resolve().with_name("cache-state.py"))
cache_state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache_state)


def main():
    if os.name != "posix" or os.geteuid() == 0:
        raise RuntimeError("This permission smoke requires an unprivileged POSIX user")
    with tempfile.TemporaryDirectory(prefix="asterbound-cache-smoke-") as temporary:
        root = Path(temporary)
        store = root / "nix/store"
        store.mkdir(parents=True)
        complete = store / ("c" * 32 + "-complete")
        complete.mkdir()
        (complete / "data").write_bytes(b"completed output\x00payload")
        registered_lock = store / ("b" * 32 + "-Cargo.lock")
        registered_lock.write_bytes(b"valid registered lockfile output")
        registered_lock.chmod(0o444)
        neighbor = store / ("e" * 32 + "-lookalikeX.lock")
        neighbor.write_bytes(b"also registered")
        locks = [store / ("a" * 32 + "-failed.lock"),
                 store / ("e" * 32 + "-lookalike?.lock")]
        for lock in locks:
            lock.write_bytes(b"transient lock")
            lock.chmod(0)
        work = root / "work/owner/repository"
        work.mkdir(parents=True)
        excludes = root / "locks.exclude"
        action_excludes = root / "action.exclude"
        action_excludes.write_text("")
        manifest = root / "manifest.txt"
        manifest.write_text(os.path.relpath(store.parent, work) + "\n")
        archive = root / "cache.tar"
        command = ["tar", "--posix", "-cf", str(archive), "--exclude", archive.name,
                   "-P", "-C", str(work), "--exclude-from", str(action_excludes),
                   "--files-from", str(manifest)]
        environment = dict(os.environ)
        environment.pop("TAR_OPTIONS", None)

        def pack():
            return subprocess.run(command, cwd=work, env=environment,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        assert pack().returncode != 0, "Unreadable locks unexpectedly archived"
        with contextlib.chdir(work):
            cache_state.nix_lock_excludes(
                store, {str(path) for path in (complete, registered_lock, neighbor)}, excludes)
        environment["TAR_OPTIONS"] = f"--exclude-from={excludes}"
        result = pack()
        if result.returncode:
            raise RuntimeError(result.stderr.decode())
        with tarfile.open(archive) as saved:
            members = {Path(member.name).name: member for member in saved.getmembers()}
            assert all(lock.name not in members for lock in locks)
            for path in (complete / "data", registered_lock, neighbor):
                assert saved.extractfile(members[path.name]).read() == path.read_bytes()
            assert members[registered_lock.name].mode == 0o444
        unreadable = complete / "unreadable"
        unreadable.write_bytes(b"a real store output must not be ignored")
        unreadable.chmod(0)
        assert pack().returncode != 0, "Unrelated unreadable store data was silently ignored"
    print("PASS: transient locks excluded; registered .lock outputs and bytes/modes preserved; other read errors remain fatal")


if __name__ == "__main__":
    main()
