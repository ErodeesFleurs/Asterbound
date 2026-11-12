#!/usr/bin/env python3
"""Fingerprint the native SDK or snapshot content-addressed CI cache entries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def emit(values):
    text = "".join(f"{key}={value}\n" for key, value in values.items())
    print(text, end="")
    if output := os.environ.get("GITHUB_OUTPUT"):
        with open(output, "a", encoding="utf-8") as stream:
            stream.write(text)


def sdk_identity(compiler_root):
    root = Path(compiler_root)
    architecture = "aarch64" if platform.machine().lower() in ("aarch64", "arm64") else "x86_64"
    system = platform.system()
    target = {
        "Linux": f"{architecture}-unknown-linux-gnu",
        "Windows": f"{architecture}-w64-windows-gnu",
        "Darwin": f"{architecture}-apple-darwin",
    }[system]
    # Linux's generated driver config includes hashes of the actual host C SDK.
    # Other targets use xclang's pinned bundle plus the native macOS SDK below.
    config = root / "bin" / f"{target}.cfg"
    identity = {"system": system, "target": target,
                "driver_config": hashlib.sha256(config.read_bytes()).hexdigest(),
                "runner_image": {key: os.environ[key] for key in ("ImageOS", "ImageVersion")
                                 if key in os.environ}}
    if system == "Darwin":
        sdk_path = subprocess.check_output(
            ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
        identity["sdk_settings"] = hashlib.sha256(
            (Path(sdk_path) / "SDKSettings.json").read_bytes()).hexdigest()
        identity["xcode"] = subprocess.check_output(["xcodebuild", "-version"], text=True)
        # Use exactly the SDK fingerprinted here for the following configure.
        if output := os.environ.get("GITHUB_ENV"):
            with open(output, "a", encoding="utf-8") as stream:
                stream.write(f"SDKROOT={sdk_path}\n")
    digest = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    emit({"sdk": digest})
    print(json.dumps(identity, indent=2))


def record(entries, output, compare):
    serialized = json.dumps(entries, separators=(",", ":"))
    before = Path(compare).read_text(encoding="utf-8") if compare else serialized
    if output:
        Path(output).parent.mkdir(parents=True, exist_ok=True)
        Path(output).write_text(serialized, encoding="utf-8")
    emit({"inventory": hashlib.sha256(serialized.encode()).hexdigest(),
          "changed": str(serialized != before).lower(), "entries": len(entries)})


def snapshot(directory, output, compare):
    root = Path(directory)
    root.mkdir(parents=True, exist_ok=True)
    # vcpkg and LLVM use content-derived filenames. Timestamps are not entries.
    entries = sorted((path.relative_to(root).as_posix(), path.stat().st_size)
                     for path in root.rglob("*")
                     if path.is_file() and path.name != "llvmcache.timestamp")
    record(entries, output, compare)


def nix_snapshot(output, compare):
    # Store paths already carry their immutable identity; no store files are read.
    paths = subprocess.check_output(["nix", "path-info", "--all"], text=True)
    record(sorted(paths.splitlines()), output, compare)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    sdk = commands.add_parser("sdk")
    sdk.add_argument("--compiler-root", required=True)
    cache = commands.add_parser("snapshot")
    cache.add_argument("--directory", required=True)
    cache.add_argument("--output")
    cache.add_argument("--compare")
    nix_cache = commands.add_parser("nix-snapshot")
    nix_cache.add_argument("--output")
    nix_cache.add_argument("--compare")
    args = parser.parse_args()
    if args.command == "sdk":
        sdk_identity(args.compiler_root)
    elif args.command == "snapshot":
        snapshot(args.directory, args.output, args.compare)
    else:
        nix_snapshot(args.output, args.compare)


if __name__ == "__main__":
    main()
