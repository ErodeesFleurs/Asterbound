#!/usr/bin/env python3
"""Keep recent native cache snapshots only after observing a new successful save.

Only content-addressed compiler/ThinLTO snapshots are eligible. Cache versions,
SDKs, presets, compiler revisions and refs never share a retention budget.
Dry-run is the default; --apply deletes the printed cache IDs, not key prefixes.
"""
import argparse
from collections import defaultdict
from datetime import datetime
import ipaddress
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request


HEX = r"[0-9a-f]{64}"
SNAPSHOT = re.compile(
    rf"(?P<namespace>(?:sccache0\.17-(?:v1|win-clang-rsp-{HEX})|thinlto-v1)-"
    r"(?P<os>Windows|Linux|macOS)-(?P<arch>X64|ARM64)-"
    r"(?P<preset>windows-release|linux-release|linux-arm-release|macos-arm-release)-"
    rf"xclang[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+-{HEX})-{HEX}"
)
NATIVE_TARGETS = {
    ("Windows", "X64", "windows-release"),
    ("Linux", "X64", "linux-release"),
    ("Linux", "ARM64", "linux-arm-release"),
    ("macOS", "ARM64", "macos-arm-release"),
}


def timestamp(value):
    if not isinstance(value, str):
        raise ValueError("expected an ISO-8601 timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        raise ValueError("expected an ISO-8601 timestamp") from None
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("timestamp must include a timezone")
    return parsed


class NoRedirect(urllib.request.HTTPRedirectHandler):
    # Never forward the Authorization header to a redirect target.
    def redirect_request(self, request, response, code, message, headers, url):
        return None


class CacheAPI:
    def __init__(self, repository, token):
        base = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")
        parts = urllib.parse.urlsplit(base)
        try:
            loopback = parts.hostname == "localhost" or ipaddress.ip_address(parts.hostname).is_loopback
        except ValueError:
            loopback = False
        if (parts.scheme != "https" and not (parts.scheme == "http" and loopback)
                or not parts.hostname or parts.username or parts.password or parts.query or parts.fragment):
            raise ValueError("GITHUB_API_URL must be HTTPS (or a loopback HTTP fixture) without credentials")
        self.url = f"{base}/repos/{repository}/actions/caches"
        self.token = token
        self.opener = urllib.request.build_opener(NoRedirect())

    def request(self, method, suffix):
        request = urllib.request.Request(self.url + suffix, method=method, headers={
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "asterbound-native-cache-retention",
        })
        try:
            with self.opener.open(request, timeout=30) as response:
                if method == "GET":
                    return json.load(response)
        except urllib.error.HTTPError as error:
            if method == "DELETE" and error.code == 404:
                return False
            # Do not print request headers, response bodies or exception URLs.
            raise RuntimeError(f"cache API {method} failed with HTTP {error.code}") from None
        except (OSError, urllib.error.URLError):
            raise RuntimeError(f"cache API {method} failed (network error)") from None
        return True

    def caches(self):
        caches = {}
        page = 1
        while True:
            payload = self.request("GET", f"?per_page=100&page={page}")
            if not isinstance(payload, dict) or not isinstance(payload.get("actions_caches"), list):
                raise ValueError("cache API returned an invalid cache list")
            entries = payload["actions_caches"]
            for entry in entries:
                if (not isinstance(entry, dict) or type(entry.get("id")) is not int
                        or entry["id"] <= 0):
                    raise ValueError("cache API returned an invalid cache ID")
                if entry["id"] in caches:
                    # A moving page boundary must not turn a partial inventory into a deletion plan.
                    raise ValueError("cache API pagination repeated a cache ID; refusing to prune")
                caches[entry["id"]] = entry
            if len(entries) < 100:
                return list(caches.values())
            page += 1


def plan(caches, ref, since, until, keep):
    groups = defaultdict(list)
    ignored = {"other_ref": 0, "other_key": 0, "invalid_metadata": 0}
    for cache in caches:
        if cache.get("ref") != ref:
            ignored["other_ref"] += 1
            continue
        key = cache.get("key")
        match = SNAPSHOT.fullmatch(key) if isinstance(key, str) else None
        if (not match or (match["os"], match["arch"], match["preset"]) not in NATIVE_TARGETS
                or (key.startswith("sccache0.17-win-clang-rsp-") and match["os"] != "Windows")):
            ignored["other_key"] += 1
            continue
        try:
            created = timestamp(cache.get("created_at"))
            version = cache.get("version")
            size = cache.get("size_in_bytes")
            if not isinstance(version, str) or not version or type(size) is not int or size < 0:
                raise ValueError("invalid cache metadata")
        except ValueError:
            ignored["invalid_metadata"] += 1
            continue
        groups[(ref, version, match["namespace"])].append((created, cache))

    deletions = []
    for (group_ref, version, namespace), snapshots in sorted(groups.items()):
        snapshots.sort(key=lambda item: (item[0], item[1]["id"]), reverse=True)
        saved = [cache["id"] for created, cache in snapshots
                 if since <= created <= until and cache["size_in_bytes"] > 0]
        eligible = bool(saved)
        print(json.dumps({"ref": group_ref, "version": version, "namespace": namespace,
                          "new_save_ids": saved,
                          "policy": f"keep newest {keep}" if eligible else "keep all: no new nonempty save"}))
        for index, (_, cache) in enumerate(snapshots):
            delete = eligible and index >= keep
            print(json.dumps({"action": "delete" if delete else "keep", "id": cache["id"],
                              "size_in_bytes": cache["size_in_bytes"]}))
            if delete:
                deletions.append(cache)
    print(json.dumps({"protected_outside_retention": ignored, "recognized_namespaces": len(groups),
                      "delete_count": len(deletions),
                      "delete_bytes": sum(cache["size_in_bytes"] for cache in deletions)}))
    return deletions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, help="owner/repository")
    parser.add_argument("--ref", required=True, help="exact cache ref, for example refs/heads/main")
    parser.add_argument("--since", required=True, type=timestamp, help="triggering run's run_started_at")
    parser.add_argument("--until", required=True, type=timestamp, help="triggering run's updated_at")
    parser.add_argument("--keep", type=int, default=2)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        parser.error("--repository must be owner/repository")
    if not args.ref.startswith("refs/heads/") or any(ord(char) < 32 for char in args.ref):
        parser.error("--ref must be a full branch ref")
    if args.keep < 1:
        parser.error("--keep must be at least 1")
    if args.since > args.until:
        parser.error("--since must not be after --until")
    token = os.environ.get("GH_TOKEN")
    if not token:
        parser.error("GH_TOKEN is required")
    if any(ord(char) < 33 or ord(char) > 126 for char in token):
        parser.error("GH_TOKEN contains invalid header characters")
    api = CacheAPI(args.repository, token)
    deletions = plan(api.caches(), args.ref, args.since, args.until, args.keep)
    if not args.apply:
        print("Dry run: no caches deleted. Pass --apply to execute this plan.")
        return
    for cache in deletions:
        deleted = api.request("DELETE", f"/{cache['id']}")
        print(json.dumps({"action": "deleted" if deleted else "already_absent",
                          "id": cache["id"], "size_in_bytes": cache["size_in_bytes"]}))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError) as error:
        print(f"Cache retention failed: {error}", file=sys.stderr)
        sys.exit(1)
