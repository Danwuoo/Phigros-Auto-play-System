#!/usr/bin/env python3
"""Audit every catalogue chart with a pinned native chart_inspect executable.

This is an offline strict-v3 importer audit. It never changes library readiness,
rewrites chart bytes, compiles touch plans, or starts a device/player. A complete
audit may contain rejected charts; only per-chart status=passed is an import pass.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time

from song_catalog import (LIBRARY_SCHEMA, MAX_MANIFEST_BYTES, SHA256, inside,
                          inspect_asset, json_bytes, load_json, reject_constant,
                          validate_lock, write_atomic)

SCHEMA = "pas.catalog-native-import-report.v1"
POLICY = "phigros-v3-strict-v1"
OUTPUT_LIMIT = 256 * 1024
ERROR_STATUSES = {"timeout", "integrity_error", "inspector_error"}


def file_sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run_bounded(command, timeout_seconds, scratch, output_limit=OUTPUT_LIMIT):
    """No shell, no inherited stdin, finite timeout and combined output budget."""
    with tempfile.TemporaryDirectory(prefix="native-inspect-", dir=scratch) as temporary:
        output_path, error_path = Path(temporary) / "stdout", Path(temporary) / "stderr"
        with output_path.open("w+b") as output, error_path.open("w+b") as error:
            start = time.monotonic()
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output, stderr=error,
                                       shell=False, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            reason = None
            try:
                while process.poll() is None:
                    if time.monotonic() - start >= timeout_seconds:
                        reason = "timeout"
                        break
                    if os.fstat(output.fileno()).st_size + os.fstat(error.fileno()).st_size > output_limit:
                        reason = "output_limit"
                        break
                    time.sleep(0.01)
                if reason:
                    process.kill()
                process.wait(timeout=5)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
            output.seek(0)
            error.seek(0)
            stdout, stderr = output.read(output_limit + 1), error.read(output_limit + 1)
            if len(stdout) + len(stderr) > output_limit:
                reason = reason or "output_limit"
            return dict(returncode=process.returncode, stdout=stdout[:output_limit].decode("utf-8", "replace"),
                        stderr=stderr[:output_limit].decode("utf-8", "replace"),
                        failure=reason)


def parse_summary(text, expected_sha, row):
    def no_duplicates(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate native summary field")
            result[key] = value
        return result

    result = json.loads(text, parse_constant=reject_constant, object_pairs_hook=no_duplicates)
    if (not isinstance(result, dict) or type(result.get("format_version")) is not int
            or result.get("format_version") != 3 or result.get("chart_sha256") != expected_sha):
        raise ValueError("Native summary chart identity/format mismatch")
    for key, maximum in (("lines", 4096), ("notes", 200000), ("events", 2000000)):
        if type(result.get(key)) is not int or not 0 <= result[key] <= maximum:
            raise ValueError("Invalid native summary count: " + key)
    types = result.get("types")
    if (not isinstance(types, dict) or set(types) != {"tap", "drag", "hold", "flick"}
            or any(type(x) is not int or x < 0 for x in types.values()) or sum(types.values()) != result["notes"]):
        raise ValueError("Native summary note counts disagree")
    for key in ("first_note_ns", "last_note_end_ns"):
        if result.get(key) is not None and type(result[key]) is not int:
            raise ValueError("Native summary time is not integer nanoseconds")
    offset = result.get("offset_seconds")
    if type(offset) not in (int, float) or not math.isfinite(offset):
        raise ValueError("Native summary offset is invalid")
    if row.get("notes") is not None and result["notes"] != row["notes"]:
        raise ValueError("Native note count differs from catalogue structural count")
    if row.get("line_count") is not None and result["lines"] != row["line_count"]:
        raise ValueError("Native line count differs from catalogue structural count")
    return result


def inspect_one(root, row, target, inspector, timeout_seconds, scratch, runner=run_bounded):
    result = {key: row.get(key) for key in ("library_id", "song_id", "track_id", "difficulty", "variant", "title")}
    result.update(chart_file=target["file"], chart_sha256=row["chart"].get("sha256"),
                  source_format_version=row.get("format_version"), policy=POLICY,
                  gameplay_qualified=False)
    try:
        before = inspect_asset(root, target)
        expected = row["chart"].get("sha256")
        if before["status"] != "verified" or not isinstance(expected, str) or not SHA256.fullmatch(expected) or before["sha256"] != expected:
            return dict(result, status="integrity_error", reason="Chart bytes are missing, LFS-only, changed, or differ from the catalogue SHA256")
        path = inside(root, target["file"])
        execution = runner([str(inspector), str(path), expected], timeout_seconds, scratch)
        after = inspect_asset(root, target)
        if after["status"] != "verified" or after.get("sha256") != expected:
            return dict(result, status="integrity_error", reason="Chart bytes changed during native inspection")
        result["returncode"] = execution["returncode"]
        if execution.get("failure"):
            status = "timeout" if execution["failure"] == "timeout" else "inspector_error"
            return dict(result, status=status, reason=execution["failure"], stderr=execution["stderr"][:2000])
        if execution["returncode"] != 0:
            reason = execution["stderr"].strip()[:4000] or "Native inspector returned nonzero without a diagnostic"
            if execution["returncode"] != 1 or not reason.startswith("chart-inspect:"):
                status = "inspector_error"
            elif row.get("format_version") != 3 and "/formatVersion" in reason and "only version 3 is supported" in reason:
                status = "unsupported_format"
            elif any(text in reason for text in ("cannot open chart", "chart read failed", "usage: chart_inspect", "unknown or duplicate chart_inspect argument")):
                status = "inspector_error"
            else:
                status = "rejected"
            return dict(result, status=status, reason=reason)
        summary = parse_summary(execution["stdout"], expected, row)
        return dict(result, status="passed", summary=summary,
                    stderr=execution["stderr"].strip()[:2000])
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        return dict(result, status="inspector_error", reason=type(error).__name__ + ": " + str(error)[:1000])


def load_inputs(library_path):
    library_path = Path(library_path).absolute()
    library = load_json(library_path)
    library_sha = file_sha(library_path)
    if library.get("schema") != LIBRARY_SCHEMA:
        raise ValueError("Unsupported catalogue library schema")
    root = library_path.parent
    reference = library.get("source_lock", {})
    lock_path = inside(root, reference.get("file", ""))
    if file_sha(lock_path) != reference.get("sha256"):
        raise ValueError("Source lock fingerprint differs from library")
    lock = validate_lock(load_json(lock_path))
    targets = {x["file"]: x for x in lock["targets"] if x["kind"] == "chart"}
    rows = library.get("charts")
    if not isinstance(rows, list) or len(rows) != len(targets) or not 1 <= len(rows) <= 4096:
        raise ValueError("Library chart inventory differs from source lock")
    seen = set()
    for row in rows:
        path = row.get("chart", {}).get("file")
        target = targets.get(path)
        if target is None or path in seen:
            raise ValueError("Duplicate or unknown library chart")
        seen.add(path)
        for key in ("song_id", "track_id", "difficulty", "variant"):
            if row.get(key) != target.get(key):
                raise ValueError("Library chart identity differs from source lock: " + key)
        expected_id = "/".join((lock["source"]["version"], target["track_id"], target["difficulty"], target["git_blob_sha"]))
        if row.get("library_id") != expected_id:
            raise ValueError("Library stable identity differs from source lock")
    return root, library, library_sha, lock, rows, targets


def audit(library_path, inspector, output, *, timeout_seconds=20, jobs=1, replace=False, runner=run_bounded, progress=None):
    if not 0.05 <= timeout_seconds <= 120 or not 1 <= jobs <= 4:
        raise ValueError("Timeout/worker budget outside supported range")
    library_path, inspector, output = Path(library_path).absolute(), Path(inspector).absolute(), Path(output).absolute()
    if inspector.is_symlink() or not inspector.is_file() or not 0 < inspector.stat().st_size <= 512 * 1024 * 1024:
        raise ValueError("Missing, linked or oversized native inspector")
    if output.is_symlink():
        raise ValueError("Report must be a new independent file; linked outputs are not supported")
    # Normalize '..' aliases before testing protected inputs, even with --replace.
    library_path, inspector, output = library_path.resolve(), inspector.resolve(), output.resolve()
    if output in (library_path, inspector) or (output.exists() and not replace):
        raise ValueError("Report must be a new independent file; --replace explicitly permits replacing a report")
    root, library, library_sha, lock, rows, targets = load_inputs(library_path)
    protected = {inside(root, x["file"]) for x in lock["targets"]}
    protected.add(inside(root, library["source_lock"]["file"]))
    if output in protected:
        raise ValueError("Report cannot overwrite a source asset or lock")
    output.parent.mkdir(parents=True, exist_ok=True)
    inspector_sha = file_sha(inspector)
    results = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(inspect_one, root, row, targets[row["chart"]["file"]], inspector,
                               timeout_seconds, output.parent, runner): row for row in rows}
        for index, future in enumerate(as_completed(futures), 1):
            result = future.result()
            results.append(result)
            if progress:
                progress(dict(event="native_import", completed=index, total=len(rows),
                              track_id=result["track_id"], difficulty=result["difficulty"], status=result["status"]))
    results.sort(key=lambda x: x["library_id"])
    counts = Counter(x["status"] for x in results)
    changed = []
    if file_sha(inspector) != inspector_sha:
        changed.append("inspector_changed")
    if file_sha(library_path) != library_sha:
        changed.append("library_changed")
    if file_sha(inside(root, library["source_lock"]["file"])) != library["source_lock"]["sha256"]:
        changed.append("source_lock_changed")
    report = dict(schema=SCHEMA, status="complete" if not changed and not any(counts[x] for x in ERROR_STATUSES) else "incomplete",
                  policy=POLICY, source=lock["source"],
                  library=dict(file=library_path.name, sha256=library_sha), source_lock=library["source_lock"],
                  inspector=dict(file=inspector.name, sha256=inspector_sha, bytes=inspector.stat().st_size,
                                 platform=platform.system(), machine=platform.machine(),
                                 command_contract="chart_inspect <chart.json> <expected-sha256>"),
                  total=len(rows), counts={key: counts[key] for key in ("passed", "unsupported_format", "rejected", "timeout", "integrity_error", "inspector_error")},
                  all_passed=counts["passed"] == len(rows) and not changed,
                  input_changes=changed, timeout_seconds=timeout_seconds, workers=jobs, charts=results,
                  device_discovery=False, input_enabled=False, gameplay_qualified=False,
                  scope="Strict native importer audit only; no geometry, touch-plan, HUD, synchronization or gameplay qualification.")
    write_atomic(output.parent, output.name, json_bytes(report))
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--inspector", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=20)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--require-all-pass", action="store_true", help="Exit nonzero for expected semantic/format rejections too")
    args = parser.parse_args(argv)
    try:
        report = audit(args.library, args.inspector, args.output, timeout_seconds=args.timeout_seconds,
                       jobs=args.jobs, replace=args.replace,
                       progress=lambda value: print(json.dumps(value, ensure_ascii=False), flush=True))
        print(json.dumps(dict(event="complete", status=report["status"], total=report["total"],
                              counts=report["counts"], all_passed=report["all_passed"]), ensure_ascii=False))
        return 0 if report["status"] == "complete" and (report["all_passed"] or not args.require_all_pass) else 2
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps(dict(event="error", reason=str(error)), ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
