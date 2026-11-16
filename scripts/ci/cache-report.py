#!/usr/bin/env python3
"""Summarize native CI cache restores and measured build diagnostics."""
import argparse
import html
import json
import math
import os
from pathlib import Path


def escape(value):
    # Entities also keep table delimiters, code fences and injected headings inert.
    text = str(value).replace("\r\n", "\n").replace("\r", "\n")
    return "".join(html.escape(char, quote=True) if char.isalnum() or char == " "
                   else f"&#{ord(char)};" for char in text)


def number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def measured(value, unit=""):
    if not number(value):
        return "unavailable"
    return f"{value:,.3f}{unit}" if isinstance(value, float) else f"{value:,}{unit}"


def load(path, notes):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        notes.append(f"{path.name} unavailable: {error}")
        return None


def counter(value):
    if number(value) and value >= 0:
        return value
    if isinstance(value, dict) and isinstance(value.get("counts"), dict):
        counts = value["counts"]
        if all(number(item) and item >= 0 for item in counts.values()):
            return sum(counts.values())
    return None


def baseline(value):
    if not isinstance(value, list) or not all(
            isinstance(row, list) and len(row) == 2 and isinstance(row[0], str)
            and number(row[1]) and row[1] >= 0 for row in value):
        return "unavailable (missing or invalid inventory)"
    return f"{len(value):,} entries; {sum(row[1] for row in value):,} bytes"


def busy_seconds(rows):
    if not isinstance(rows, list):
        return None
    intervals = []
    for row in rows:
        if not isinstance(row, dict):
            return None
        start, end = row.get("start_seconds"), row.get("end_seconds")
        if not number(start) or not number(end) or start < 0 or end < start:
            return None
        intervals.append((start, end))
    total, last = 0.0, None
    for start, end in sorted(intervals):
        total += max(0, end - max(start, last if last is not None else start))
        last = max(end, last if last is not None else end)
    return total


def report(preset, directory):
    notes = []
    profile = load(directory / "build-profile.json", notes)
    profile = profile if isinstance(profile, dict) else {}
    cache = load(directory / "sccache.json", notes)
    cache = cache if isinstance(cache, dict) else {}
    stats = cache.get("stats")
    stats = stats if isinstance(stats, dict) else {}
    try:
        restores = json.loads(os.environ.get("CACHE_RESTORES", "null"))
    except ValueError as error:
        restores = None
        notes.append(f"CACHE_RESTORES unavailable: {error}")
    restores = restores if isinstance(restores, dict) else {}
    lines = [f"## Native cache report: {escape(preset)}", "", "### Restore results", "",
             "| Cache | Restore | Matched key | Baseline inventory |",
             "| --- | --- | --- | --- |"]
    for name, filename in (("vcpkg", "vcpkg-before.json"),
                           ("compiler", "sccache-before.json"),
                           ("thinlto", "thinlto-before.json"),
                           ("sccache-executable", None)):
        entry = restores.get(name)
        entry = entry if isinstance(entry, dict) else {}
        key, exact = entry.get("matched_key"), entry.get("exact_hit")
        outcome = entry.get("outcome")
        if isinstance(key, str) and key:
            status = "exact hit" if exact is True or exact == "true" else "matched prefix hit"
        elif outcome in ("skipped", "cancelled", "failure", "not-run", "not-applicable", ""):
            status = f"unavailable (restore outcome: {escape(outcome)})"
        elif outcome == "success" or exact is False or exact == "false":
            status = "miss (no matched restore key)"
        else:
            status = "unavailable (no matched restore key reported)"
        inventory = baseline(load(directory / filename, notes)) if filename else "not collected"
        lines.append(f"| {escape(name)} | {status} | {escape(key) if key else 'unavailable'} | {inventory} |")
    lines += ["", "A false exact-hit flag is not a restore failure: a matched prefix key is a restored cache.",
              f"Configured compiler cache budget: {escape(os.environ.get('COMPILER_CACHE_SIZE', 'unavailable'))}. "
              f"Configured ThinLTO cache budget: {escape(os.environ.get('THINLTO_CACHE_SIZE', 'unavailable'))}. "
              "These are intended limits, not measured occupancy or eviction counts.",
              "", "### Compiler cache measurements", "", "| Metric | Value |", "| --- | --- |"]
    for label, key in (("Hits", "cache_hits"), ("Misses", "cache_misses"),
                       ("Cache errors", "cache_errors"), ("Read errors", "cache_read_errors"),
                       ("Write errors", "cache_write_errors"), ("Timeouts", "cache_timeouts"),
                       ("Compile requests", "compile_requests"),
                       ("Requests executed", "requests_executed"),
                       ("Non-cacheable requests", "requests_not_cacheable"),
                       ("Unsupported compiler requests", "requests_unsupported_compiler"),
                       ("Non-cacheable compilations", "non_cacheable_compilations")):
        lines.append(f"| {label} | {measured(counter(stats.get(key)))} |")
    hits, misses = counter(stats.get("cache_hits")), counter(stats.get("cache_misses"))
    rate = f"{100 * hits / (hits + misses):.2f}%" if hits is not None and misses is not None and hits + misses > 0 else "unavailable (no measured hits + misses denominator)"
    lines.append(f"| Hit rate (hits / (hits + misses)) | {rate} |")
    size, maximum = cache.get("cache_size"), cache.get("max_cache_size")
    lines += [f"| Cache size | {measured(size, ' bytes')} |",
              f"| Maximum cache size | {measured(maximum, ' bytes')} |"]
    occupancy = size / maximum if number(size) and size >= 0 and number(maximum) and maximum > 0 else None
    lines.append(f"| Occupancy | {f'{occupancy:.2%}' if occupancy is not None else 'unavailable'} |")
    if occupancy is not None and occupancy >= 0.95:
        lines += ["", "**Warning:** compiler cache occupancy is at least 95%; capacity pressure is possible. Occupancy does not measure eviction counts."]
    reasons = stats.get("not_cached")
    lines += ["", "Non-cacheable reasons:"]
    if isinstance(reasons, dict):
        lines += [f"- {escape(key)}: {measured(counter(value))}" for key, value in sorted(reasons.items())]
        if not reasons:
            lines.append("- None recorded (empty measured reasons map).")
    else:
        lines.append("- unavailable")
    configure = os.environ.get("CONFIGURE_SECONDS")
    try:
        configure = float(configure) if configure is not None else None
    except ValueError:
        configure = None
    rows = profile.get("link_timings")
    lines += ["", "### Build timing and resources", "",
              f"- Configure wall time: {measured(configure, ' seconds')}",
              f"- Build wall time: {measured(profile.get('elapsed_seconds'), ' seconds')}",
              f"- Completed link/archive busy interval union: {measured(busy_seconds(rows), ' seconds')} (overlaps counted once; not a sum of edge durations).",
              f"- Build return code: {measured(profile.get('returncode'))}",
              f"- Cancellation signal: {escape(profile.get('cancelled_signal', 'unavailable'))}",
              "", "Only completed link/archive edges are timed; failed or interrupted edges are absent. Missing diagnostics are unavailable, not zero. Interrupted or failed builds and interrupted cache collection may leave partial statistics.",
              "", "#### Longest completed links/archives", "",
              "| Seconds | Kind | Output | Rule |", "| --- | --- | --- | --- |"]
    valid_rows = [row for row in rows if isinstance(row, dict) and number(row.get("elapsed_seconds"))] if isinstance(rows, list) else []
    for row in sorted(valid_rows, key=lambda row: row["elapsed_seconds"], reverse=True)[:10]:
        lines.append(f"| {measured(row['elapsed_seconds'])} | {escape(row.get('kind', 'unavailable'))} | {escape(row.get('output', 'unavailable'))} | {escape(row.get('rule', 'unavailable'))} |")
    if not valid_rows:
        lines.append("| unavailable | No completed edge timings available | | |")
    lines += ["", "#### Resource measurements and notes", "",
              f"- Resource samples: {measured(profile.get('resource_samples'))}"]
    peaks = profile.get("sampled_peaks")
    if isinstance(peaks, dict) and peaks:
        lines += [f"- {escape(key)}: {measured(value)}" for key, value in sorted(peaks.items())]
    else:
        lines.append("- Sampled peaks: unavailable")
    semantics = profile.get("resource_semantics")
    if isinstance(semantics, dict):
        lines += [f"- {escape(key)}: {escape(value)}" for key, value in sorted(semantics.items())]
    else:
        lines.append("- Sampled RSS is not an exact high-water mark; shared pages can be counted repeatedly and short-lived processes can be missed. Private committed memory is not RSS.")
    for key in ("resource_notes", "link_timing_notes"):
        values = profile.get(key)
        if isinstance(values, list):
            lines += [f"- {escape(value)}" for value in values]
    server = profile.get("sccache_server")
    if isinstance(server, dict):
        lines.append(f"- sccache collection state: {escape(json.dumps(server, sort_keys=True))}")
    if notes:
        lines += ["", "#### Unavailable diagnostics"] + [f"- {escape(note)}" for note in notes]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    text = report(args.preset, args.output_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "cache-summary.md").write_text(text, encoding="utf-8")
    print(text, end="")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as stream:
            stream.write("\n" + text)


if __name__ == "__main__":
    main()
