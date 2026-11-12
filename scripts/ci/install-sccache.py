#!/usr/bin/env python3
"""Build the pinned, disk-cache-only Windows sccache with our Clang spawn patch."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request


SOURCE_URL = "https://codeload.github.com/mozilla/sccache/tar.gz/refs/tags/v0.17.0"
SOURCE_SHA256 = "49949ad1cf175c49da126dbb0c2e6a56bd9d1f626e8cc0be17b9668b914145c6"
RUST_TOOLCHAIN = "1.90.0-x86_64-pc-windows-msvc"
TARGET = "x86_64-pc-windows-msvc"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-dir", required=True, type=Path)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("this installer is only for native Windows x64 CI")
    patch = Path(__file__).with_name("sccache-windows-clang-response.patch").resolve()
    destination = args.install_dir.resolve()
    environment = os.environ.copy()
    # Build the cache itself directly, never through an inherited compiler wrapper.
    for variable in ("RUSTC_WRAPPER", "RUSTC_WORKSPACE_WRAPPER"):
        environment.pop(variable, None)
    with tempfile.TemporaryDirectory(prefix="asterbound-sccache-") as temporary:
        work = Path(temporary)
        archive = work / "sccache.tar.gz"
        with urllib.request.urlopen(SOURCE_URL, timeout=120) as response, archive.open("wb") as output:
            shutil.copyfileobj(response, output)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != SOURCE_SHA256:
            raise RuntimeError(f"sccache source SHA-256 mismatch: {digest}")
        with tarfile.open(archive) as source:
            source.extractall(work, filter="data")
        source = work / "sccache-0.17.0"
        subprocess.run(["git", "apply", "--whitespace=error-all", str(patch)],
                       cwd=source, env=environment, check=True)
        subprocess.run(["rustup", "toolchain", "install", RUST_TOOLCHAIN,
                        "--profile", "minimal"], env=environment, check=True)
        environment["CARGO_TARGET_DIR"] = str(work / "target")
        subprocess.run(["cargo", f"+{RUST_TOOLCHAIN}", "build", "--release", "--locked",
                        "--no-default-features", "--bin", "sccache", "--target", TARGET],
                       cwd=source, env=environment, check=True)
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copy2(work / "target" / TARGET / "release" / "sccache.exe", destination)
        shutil.copy2(source / "LICENSE", destination / "sccache-LICENSE")


if __name__ == "__main__":
    main()
