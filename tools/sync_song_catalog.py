#!/usr/bin/env python3
"""Acquire and verify a fixed public Phigros IN/AT catalogue; never launch a game.

Examples (from the repository root):
  python tools/sync_song_catalog.py sync --output assets/phigros/4.0.1.2
  python tools/sync_song_catalog.py verify --output assets/phigros/4.0.1.2

All network access is explicit in snapshot/sync. verify/index are offline. The
catalogue remains browse-only until separate local native qualification exists.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import datetime
import hashlib
import json
import os
from pathlib import Path
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

from song_catalog import (LOCK_SCHEMA, MAX_MANIFEST_BYTES, MIB, REPORT_SCHEMA, build_library,
                          build_lock, git_blob_sha, inside, inspect_asset, json_bytes,
                          load_json, source_url, validate_lock, validate_sources, write_atomic)

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = REPOSITORY_ROOT / "configs/song-catalog-sources.json"


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def open_url(url, start=0, timeout=45):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or parsed.hostname not in ("api.github.com", "raw.githubusercontent.com"):
        raise ValueError("Only the pinned GitHub source endpoints are allowed")
    headers = {"User-Agent": "Phigros-Auto-play-System-Catalog/1", "Accept-Encoding": "identity"}
    if start:
        headers["Range"] = f"bytes={start}-"
    if parsed.hostname == "api.github.com":
        headers["Accept"] = "application/vnd.github+json"
        # Optional normal GitHub API authentication; never sent to the raw host.
        token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
        if token:
            headers["Authorization"] = "Bearer " + token
    response = urlopen(Request(url, headers=headers), timeout=timeout)
    final = urlsplit(response.geturl())
    if final.scheme != "https" or final.hostname != parsed.hostname:
        response.close()
        raise ValueError("Unexpected source redirect")
    return response


def fetch_small(url, limit=MAX_MANIFEST_BYTES, opener=open_url):
    with opener(url) as response:
        raw = response.read(limit + 1)
    if len(raw) > limit:
        raise ValueError("Source response exceeds byte budget")
    return raw


def create_snapshot(output, config_path=DEFAULT_CONFIG, opener=open_url):
    output = Path(output)
    config = validate_sources(load_json(config_path))
    lock_path = inside(output, "manifests/source-lock.json")
    if lock_path.exists():
        lock = validate_lock(load_json(lock_path))
        if lock["source"] != config:
            raise ValueError("Existing snapshot has different source revisions; use a new output directory")
        return lock
    trees = {}
    for branch, commit in config["commits"].items():
        url = f"https://api.github.com/repos/{config['repository']}/git/trees/{commit}?recursive=1"
        trees[branch] = json.loads(fetch_small(url, opener=opener))
    metadata = {name: fetch_small(source_url(config["repository"], config["commits"]["info"], name),
                                 limit=2 * MIB, opener=opener)
                for name in ("info.tsv", "difficulty.tsv", "version.txt")}
    lock = build_lock(config, trees["info"], trees["chart"], trees["music"],
                      metadata["info.tsv"], metadata["difficulty.tsv"], metadata["version.txt"])
    output.mkdir(parents=True, exist_ok=True)
    for name, raw in metadata.items():
        write_atomic(output, "metadata/" + name, raw, immutable=True)
    for branch, tree in trees.items():
        write_atomic(output, "manifests/source-tree-" + branch + ".json", json_bytes(tree), immutable=True)
    write_atomic(output, "manifests/source-lock.json", json_bytes(lock), immutable=True)
    return lock


def download_asset(root, target, *, retries=3, repair=False, opener=open_url, sleep=time.sleep):
    """Stream one bounded target, resume its .part, verify before atomic promotion."""
    observed = inspect_asset(root, target)
    if observed["status"] == "verified":
        return dict(observed, action="reused")
    if observed["status"] == "failed" and not repair:
        return dict(observed, action="refused", error=observed.get("error", "invalid_existing_file") + "; use --repair to quarantine and replace")
    path = inside(root, target["file"])
    path.parent.mkdir(parents=True, exist_ok=True)
    part_relative = target["file"] + ".part"
    partial = inside(root, part_relative)
    last_error = "download_failed"
    for attempt in range(1, retries + 1):
        try:
            inside(root, target["file"])
            inside(root, part_relative)
            start = partial.stat().st_size if partial.is_file() else 0
            if partial.exists() and not partial.is_file():
                raise ValueError("Partial destination is not a file")
            if start > target["bytes"]:
                partial.unlink()
                start = 0
            if start < target["bytes"]:
                with opener(target["raw_url"], start=start) as response:
                    status = getattr(response, "status", 200)
                    if start and status == 206:
                        expected = f"bytes {start}-"
                        if not response.headers.get("Content-Range", "").startswith(expected):
                            raise ValueError("Invalid Content-Range for partial response")
                        mode = "ab"
                    elif status == 200:
                        start, mode = 0, "wb"
                    else:
                        raise ValueError("Unexpected HTTP response status: " + str(status))
                    announced = response.headers.get("Content-Length")
                    if announced is not None and int(announced) != target["bytes"] - start:
                        raise ValueError("Source Content-Length differs from locked size")
                    with partial.open(mode) as stream:
                        total = start
                        while True:
                            block = response.read(256 * 1024)
                            if not block:
                                break
                            total += len(block)
                            if total > target["bytes"]:
                                raise ValueError("Source response exceeds locked size")
                            stream.write(block)
                        stream.flush()
                        os.fsync(stream.fileno())
                    if total != target["bytes"]:
                        raise OSError("Incomplete source response; partial retained")
            check = inspect_asset(root, dict(target, file=part_relative))
            if check["status"] != "verified":
                partial.unlink(missing_ok=True)
                raise ValueError("Downloaded bytes do not match locked Git blob")
            inside(root, target["file"])
            if path.exists() and observed["status"] == "failed":
                quarantine = target["file"] + ".corrupt-" + hashlib.sha256(path.read_bytes()).hexdigest()[:16]
                destination = inside(root, quarantine)
                if destination.exists():
                    raise ValueError("Quarantine destination already exists")
                os.replace(path, destination)
            os.replace(partial, path)
            return dict(check, file=target["file"], action="downloaded", attempts=attempt)
        except (OSError, ValueError, HTTPError, URLError) as error:
            # Do not log response bodies, request headers, or tokens.
            last_error = (type(error).__name__ + ": " + str(error))[:500]
            if attempt < retries:
                sleep(min(4, 0.5 * 2 ** (attempt - 1)))
    return dict(observed, status="failed", action="download_failed", error=last_error, attempts=retries)


def verify_catalog(output, lock, charts_only=False):
    results = {}
    for target in lock["targets"]:
        if charts_only and target["kind"] == "audio":
            continue
        try:
            observed = inspect_asset(output, target)
        except (ValueError, OSError) as error:
            observed = dict(file=target["file"], status="failed", sha256=None, error=str(error)[:500])
        results[target["file"]] = observed
    return results


def save_outputs(output, lock, observations, scope):
    library = build_library(output, lock, observations)
    write_atomic(output, "library.json", json_bytes(library))
    entries = [observations[key] for key in sorted(observations)]
    report = dict(schema=REPORT_SCHEMA, checked_at=utc_now(), scope=scope,
                  source_lock_sha256=hashlib.sha256(json_bytes(lock)).hexdigest(),
                  total=len(entries), verified=sum(x["status"] == "verified" for x in entries),
                  failed=sum(x["status"] != "verified" for x in entries), assets=entries,
                  library_counts=library["counts"], offline_gameplay_qualification=False)
    write_atomic(output, "manifests/verification.json", json_bytes(report))
    return library, report


def sync_catalog(output, lock, jobs=4, retries=3, charts_only=False, repair=False, progress=None):
    targets = [t for t in lock["targets"] if not (charts_only and t["kind"] == "audio")]
    results = {}
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(download_asset, output, t, retries=retries, repair=repair): t for t in targets}
        for index, future in enumerate(as_completed(futures), 1):
            target = futures[future]
            try:
                result = future.result()
            except (ValueError, OSError) as error:
                result = dict(file=target["file"], status="failed", sha256=None, error=str(error)[:500])
            results[target["file"]] = result
            if progress:
                progress(dict(event="asset", completed=index, total=len(targets), file=target["file"],
                              status=result["status"], action=result.get("action"), error=result.get("error")))
    return save_outputs(output, lock, results, "charts-only" if charts_only else "full")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("snapshot", "sync", "verify", "index"))
    parser.add_argument("--output", type=Path, required=True, help="Versioned catalogue directory")
    parser.add_argument("--sources", type=Path, default=DEFAULT_CONFIG, help="Pinned source configuration")
    parser.add_argument("--jobs", type=int, default=4, help="Bounded download workers, 1–8")
    parser.add_argument("--retries", type=int, default=3, help="Attempts per asset, 1–5")
    parser.add_argument("--charts-only", action="store_true", help="Acquire/check charts and metadata; audio remains visibly missing")
    parser.add_argument("--repair", action="store_true", help="Quarantine corrupt existing assets before replacement")
    parser.add_argument("--max-download-mib", type=int, default=4096, help="Total selected source-size budget")
    args = parser.parse_args(argv)
    if not 1 <= args.jobs <= 8 or not 1 <= args.retries <= 5 or not 1 <= args.max_download_mib <= 8192:
        parser.error("Worker/retry/download budget is outside supported bounds")
    try:
        if args.command in ("snapshot", "sync"):
            lock = create_snapshot(args.output, args.sources)
        else:
            lock = validate_lock(load_json(inside(args.output, "manifests/source-lock.json")))
        selected = [x for x in lock["targets"] if not (args.charts_only and x["kind"] == "audio")]
        if sum(x["bytes"] for x in selected) > args.max_download_mib * MIB:
            raise ValueError("Selected catalogue exceeds --max-download-mib")
        if args.command == "snapshot":
            print(json.dumps(dict(event="snapshot", counts=lock["counts"], targets=len(selected),
                                  bytes=sum(x["bytes"] for x in selected)), ensure_ascii=False))
            return 0
        if args.command == "sync":
            library, report = sync_catalog(args.output, lock, args.jobs, args.retries, args.charts_only,
                                           args.repair, progress=lambda value: print(json.dumps(value, ensure_ascii=False), flush=True))
        else:
            observed = verify_catalog(args.output, lock, args.charts_only)
            library, report = save_outputs(args.output, lock, observed, "charts-only" if args.charts_only else "full")
        print(json.dumps(dict(event="complete", counts=library["counts"], verified=report["verified"],
                              failed=report["failed"], library=str(args.output / "library.json")), ensure_ascii=False))
        return 0 if report["failed"] == 0 else 2
    except (ValueError, OSError, KeyError, TypeError, HTTPError, URLError) as error:
        print(json.dumps(dict(event="error", error=type(error).__name__ + ": " + str(error)), ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
