#!/usr/bin/env python3
"""Exercise native snapshot deletion boundaries through an isolated REST fixture."""
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit


REF = "refs/heads/main"
SINCE = "2026-11-16T10:00:00Z"
UNTIL = "2026-11-16T11:00:00Z"
OLD = "2026-11-16T09:00:00Z"
AFTER = "2026-11-16T12:00:00Z"
SDK = "a" * 64
COMPILER = f"sccache0.17-v1-Linux-X64-linux-release-xclang23.1.5.2-{SDK}"
LINK = f"thinlto-v1-Linux-ARM64-linux-arm-release-xclang23.1.5.2-{SDK}"
WINDOWS = (f"sccache0.17-win-clang-rsp-{'b' * 64}-Windows-X64-"
           f"windows-release-xclang23.1.5.2-{SDK}")


def cache(number, namespace, created=OLD, *, ref=REF, version="fixture-v1", size=100):
    return {"id": number, "key": f"{namespace}-{number:064x}", "ref": ref,
            "version": version, "created_at": created, "size_in_bytes": size}


def fixtures():
    entries = [
        # The newest snapshot is outside the triggering window but must stay.
        cache(1, COMPILER), cache(2, COMPILER), cache(3, COMPILER, SINCE),
        cache(4, COMPILER, UNTIL), cache(5, COMPILER, AFTER),
        # Independently prove that each inclusive boundary permits retention.
        cache(10, LINK), cache(11, LINK), cache(12, LINK, SINCE),
        cache(20, WINDOWS), cache(21, WINDOWS), cache(22, WINDOWS, UNTIL),
    ]
    macos = f"thinlto-v1-macOS-ARM64-macos-arm-release-xclang23.1.5.2-{SDK}"
    entries += [cache(30, macos), cache(31, macos), cache(32, macos, AFTER)]
    empty = f"thinlto-v1-Linux-X64-linux-release-xclang23.1.5.2-{SDK}"
    entries += [cache(40, empty), cache(41, empty), cache(42, empty, SINCE, size=0)]
    other_sdk = COMPILER.removesuffix(SDK) + "c" * 64
    entries += [cache(number, other_sdk) for number in range(50, 53)]
    entries += [cache(number, COMPILER, version="other-version") for number in range(60, 63)]
    entries += [cache(number, COMPILER, SINCE, ref="refs/heads/other") for number in range(70, 73)]
    entries += [cache(number, COMPILER, SINCE, ref="refs/pull/1/merge") for number in range(73, 76)]
    # Dependency, executable, Nix, lookup and almost-matching keys are not snapshots.
    for number, namespace in enumerate([
        "vcpkg-v2-Linux-X64-linux-release-xclang23.1.5.2-" + SDK,
        "sccache0.17.0-win-clang-rsp-X64-image-version-" + SDK,
        "nix-nightly-store", COMPILER + "-lookup", COMPILER + "-extra",
        f"sccache0.17-v1-Linux-X64-windows-cross-xclang23.1.5.2-{SDK}",
    ], 80):
        entries.append(cache(number, namespace, SINCE))
    # Invalid recognized metadata must not become a deletion candidate.
    entries.append(cache(90, COMPILER, SINCE, size="not-a-size"))
    # More than one API page, with ignored keys on both sides of the boundary.
    entries += [cache(number, "unrelated-fixture", SINCE) for number in range(1000, 1105)]
    return {entry["id"]: entry for entry in entries}


def main():
    caches = fixtures()
    original_ids = set(caches)
    deleted = []
    pages = []
    errors = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            pass

        def respond(self, status, payload=None):
            body = json.dumps(payload).encode() if payload is not None else b""
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def check_auth(self):
            if self.headers.get("Authorization") != "Bearer fixture-token":
                errors.append("missing fixture authorization")
                self.respond(403)
                return False
            return True

        def do_GET(self):
            if not self.check_auth():
                return
            url = urlsplit(self.path)
            query = parse_qs(url.query)
            try:
                page = int(query.get("page", ["0"])[0])
                count = int(query.get("per_page", ["0"])[0])
                assert url.path == "/repos/fixture/cache-retention/actions/caches"
                assert page >= 1 and count == 100
            except (ValueError, AssertionError):
                errors.append("invalid cache listing request")
                self.respond(400)
                return
            pages.append(page)
            ordered = [caches[number] for number in sorted(caches)]
            start = (page - 1) * count
            self.respond(200, {"total_count": len(ordered), "actions_caches": ordered[start:start + count]})

        def do_DELETE(self):
            if not self.check_auth():
                return
            prefix = "/repos/fixture/cache-retention/actions/caches/"
            try:
                assert self.path.startswith(prefix)
                number = int(self.path[len(prefix):])
            except (ValueError, AssertionError):
                errors.append("invalid cache deletion request")
                self.respond(400)
                return
            if number not in caches:
                errors.append(f"deleting absent cache {number}")
                self.respond(404)
                return
            deleted.append(number)
            del caches[number]
            self.respond(204)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    environment = dict(os.environ, GH_TOKEN="fixture-token",
                       GITHUB_API_URL=f"http://127.0.0.1:{server.server_port}",
                       NO_PROXY="127.0.0.1", no_proxy="127.0.0.1")
    script = Path(__file__).with_name("prune-build-caches.py")

    def run(*extra, since=SINCE, until=UNTIL):
        pages.clear()
        result = subprocess.run([
            sys.executable, str(script), "--repository", "fixture/cache-retention",
            "--ref", REF, "--since", since, "--until", until, *extra,
        ], env=environment, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stdout + result.stderr
        assert not errors, errors
        assert pages == [1, 2], pages

    try:
        # Even --apply cannot prune if the completed run saved nothing visible.
        run("--apply", since="2026-11-17T10:00:00Z", until="2026-11-17T11:00:00Z")
        assert not deleted and set(caches) == original_ids
        run()  # CLI defaults must be dry-run and keep=2.
        assert not deleted and set(caches) == original_ids
        run("--apply")
        expected = {1, 2, 3, 10, 20}
        assert set(deleted) == expected and len(deleted) == len(expected), deleted
        assert set(caches) == original_ids - expected
        # A second application is idempotent, without relying on output wording.
        run("--apply")
        assert set(deleted) == expected and len(deleted) == len(expected), deleted
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    print("Native cache retention smoke passed (loopback fixture only).")


if __name__ == "__main__":
    main()
