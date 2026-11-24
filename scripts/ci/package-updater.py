"""Opt in an assembled native client to the maintainer's offline signing key."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BIN_DIRS = {"linux": "linux", "windows": "win", "darwin": "osx/Asterbound.app/Contents/MacOS"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tree", type=Path)
    parser.add_argument("--combined", action="store_true", help="Windows client/server installer tree")
    args = parser.parse_args()
    key = os.environ.get("ASTERBOUND_UPDATE_PUBLIC_KEY", "")
    if not key:
        print("No release public key configured; packaging without self-update.")
        return
    if not re.fullmatch(r"[0-9a-fA-F]{64}", key):
        parser.error("ASTERBOUND_UPDATE_PUBLIC_KEY must be a hex-encoded Ed25519 public key (32 bytes)")
    module = ROOT / "scripts/updater"
    env = {**os.environ, "CGO_ENABLED": "0"}
    target, arch = subprocess.check_output(["go", "env", "GOOS", "GOARCH"], env=env, text=True).split()
    tree = args.tree.resolve(strict=True)
    exe = tree / BIN_DIRS[target] / ("asterbound-updater.exe" if target == "windows" else "asterbound-updater")
    subprocess.run(["go", "build", "-trimpath", "-buildvcs=false", "-ldflags", f"-s -w -X main.publicKeyHex={key}",
                    "-o", str(exe), "."], cwd=module, env=env, check=True)
    version = json.loads((ROOT / "version.json").read_text())["version"]
    subprocess.run(["go", "run", "./cmd/release", "pack", version, target, arch, str(tree), str(Path.cwd()),
                    *(["combined"] if args.combined else [])], cwd=module, env=env, check=True)


if __name__ == "__main__":
    main()
