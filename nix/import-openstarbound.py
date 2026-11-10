#!@python@
"""Explicitly copy an old runtime into an empty, independent Asterbound runtime."""

import argparse
import os
from pathlib import Path
import re
import shutil
import stat
import sys


DIRECTORIES = ("assets", "mods", "storage")


def default_target():
    data_home = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")
    return os.environ.get("ASTERBOUND_RUNTIME_DIR") or str(data_home / "asterbound")


def excluded(name):
    return name in ("sbinit.config", "logs") or re.fullmatch(r".*\.log(?:\.[0-9]+)?", name)


def import_runtime(source, target):
    source = source.expanduser().resolve(strict=True)
    target = target.expanduser().absolute()
    if target.is_symlink():
        raise ValueError("target must not be a symbolic link")
    target = target.resolve()
    if not source.is_dir():
        raise ValueError("source must be an OpenStarbound runtime directory")
    if source == target or source in target.parents or target in source.parents:
        raise ValueError("source and target must be distinct, non-nested directories")
    if target.exists() and (not target.is_dir() or any(target.iterdir())):
        raise ValueError(f"refusing nonempty or non-directory target: {target}")

    # Resolve and inspect every entry before creating anything. Following source
    # links copies their bytes (including linked packed.pak), never shared saves.
    # Detect cycles and links into the destination before they can recurse into
    # the data being imported. Storage includes client/server user configuration.
    entries = []

    def collect(path, relative, ancestors):
        resolved = path.resolve(strict=True)
        if resolved == target or target in resolved.parents:
            raise ValueError(f"source entry points into target: {path}")
        mode = resolved.stat().st_mode
        if stat.S_ISDIR(mode):
            if resolved in ancestors:
                raise ValueError(f"source contains a directory link cycle: {path}")
            entries.append((resolved, relative, True))
            for child in resolved.iterdir():
                if not excluded(child.name):
                    collect(child, relative / child.name, ancestors | {resolved})
        elif stat.S_ISREG(mode):
            entries.append((resolved, relative, False))
        else:
            raise ValueError(f"source contains a non-regular entry: {path}")

    for name in DIRECTORIES:
        path = source / name
        if path.exists() or path.is_symlink():
            if not path.is_dir():
                raise ValueError(f"expected a source directory: {path}")
            collect(path, Path(name), {source})
    if not entries:
        raise ValueError("source has no assets, mods or storage directories to import")

    # No existing file or child directory is ever reused. Exclusive creation also
    # refuses a destination populated after the initial empty-directory check.
    target.mkdir(parents=True, exist_ok=True)
    for path, relative, directory in entries:
        destination = target / relative
        if directory:
            destination.mkdir()
        else:
            with path.open("rb") as original, destination.open("xb") as copied:
                shutil.copyfileobj(original, copied)
            shutil.copystat(path, destination)
    return target


def main():
    parser = argparse.ArgumentParser(
        prog="asterbound-import-openstarbound",
        description="Copy OpenStarbound data without modifying the source or overwriting the target.",
        epilog=("Stop both games and servers first. Copies assets/mods/storage, including storage's "
                "user configuration; source symlinks are copied as independent data. Does not copy "
                "sbinit.config, logs or log rotations. A failed copy may leave a partial target; "
                "it is never automatically deleted. No import is performed by the game launcher."),
    )
    parser.add_argument("source", type=Path, help="old OpenStarbound runtime root (explicitly required)")
    parser.add_argument("target", type=Path, nargs="?", default=Path(default_target()),
                        help="new runtime, absent or completely empty (default: ASTERBOUND_RUNTIME_DIR "
                             "or $XDG_DATA_HOME/asterbound, falling back to ~/.local/share/asterbound)")
    arguments = parser.parse_args()
    try:
        target = import_runtime(arguments.source, arguments.target)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"asterbound-import-openstarbound: {error}; source unchanged", file=sys.stderr)
        return 1
    print(f"Asterbound data copied to {target}; OpenStarbound source unchanged.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
