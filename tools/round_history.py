"""Read local desktop and standalone run metadata without opening a device."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile


SCHEMA = "pas.round-history.v1"
LEDGER_SCHEMAS = {"pas.manual-catalog-panel-ledger.v1", "pas.manual-IN-panel-ledger.v1"}
CONTROL_SCHEMA = "pas.computer-manual-backend-run.v1"
MANIFEST_SCHEMA = "pas.experimental-whole-song.v1"
DEFAULT_RUNS_DIR = Path(__file__).resolve().parents[1] / "local" / "manual-panel-001" / "runs"
METADATA_CAP = 1024 * 1024
MANIFEST_CAP = 128 * 1024 * 1024
RESULT_CAP = 16 * 1024 * 1024
REVIEW_SCHEMA = "pas.manual-judgment-review.v1"
ARCHIVE_SCHEMA = "pas.completed-round-archive.v1"
ARCHIVE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,79}")
ROUND_ID = re.compile(r"round-[0-9]{2,4}")
JUDGMENTS = ("perfect", "good", "bad", "miss")
SHA256 = re.compile(r"[0-9a-f]{64}")
LOCAL_STAMP = re.compile(r"-(\d{8}-\d{6})(?:-(\d{3}))?-[0-9a-f]+$")
CLEANUP_STAGES = re.compile(r"(?:^|; )(runtime_stop|release|recovery|pending_kernel|kernel_final|kernel_after_recovery): ")
LEGACY_GATE = "first_note_dispatch_lead_below_200ms"
UNKNOWN = "unknown"


def obj(value):
    return value if isinstance(value, dict) else {}


def string(value):
    return value if isinstance(value, str) and value else None


def count(value):
    return value if type(value) is int and value >= 0 else None


def state(value, allowed):
    return value if isinstance(value, str) and value in allowed else UNKNOWN


def sha256_pin(value):
    return value if isinstance(value, str) and SHA256.fullmatch(value) else None


def difficulty(value):
    return state(value, {"IN", "AT", "EZ", "HD", "SP"})


def local_file(folder, name):
    path = folder / name
    try:
        if path.resolve(strict=True) == path and path.is_file():
            return path
    except (OSError, RuntimeError):
        pass
    return None


def read_object(path, cap=METADATA_CAP):
    with path.open("rb") as stream:
        raw = stream.read(cap + 1)
    if len(raw) > cap:
        raise ValueError(f"{path.name} exceeds {cap} bytes")
    value = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"{path.name} is not a JSON object")
    return value, raw


def read_metadata(folder, name, issues):
    path = local_file(folder, name)
    if path is not None:
        try:
            return read_object(path)[0]
        except (OSError, ValueError, RecursionError):
            issues.append(f"{name}:unreadable")
    return {}


def time_fields(folder_name, started):
    """Use legacy folder wall time for ordering without inventing an offset."""
    utc = None
    if isinstance(started, str):
        try:
            parsed = datetime.fromisoformat(started.replace("Z", "+00:00"))
            if parsed.tzinfo is not None and parsed.utcoffset().total_seconds() == 0:
                utc = started
        except ValueError:
            pass
    matched = LOCAL_STAMP.search(folder_name)
    local = None
    if matched:
        try:
            stamp = datetime.strptime(matched.group(1), "%Y%m%d-%H%M%S")
            local = stamp.isoformat() + ("." + matched.group(2) if matched.group(2) else "")
        except ValueError:
            pass
    return utc, local


def split_legacy_failure(failure):
    if not failure:
        return None, []
    segments = list(CLEANUP_STAGES.finditer(failure))
    if not segments:
        return failure, []
    primary = failure[:segments[0].start()] or None
    cleanup = []
    for index, segment in enumerate(segments):
        end = segments[index + 1].start() if index + 1 < len(segments) else len(failure)
        cleanup.append({"stage": segment.group(1), "detail": failure[segment.end():end]})
    return primary, cleanup


def classify_failure(primary, stage, code):
    if primary and LEGACY_GATE in primary and (not code or code == LEGACY_GATE):
        return stage or "first_note", LEGACY_GATE, True
    if code:
        return stage or UNKNOWN, code, False
    if primary == "source timestamp jumped beyond 10 seconds":
        return "capture", "source_timestamp_jump", False
    if primary and primary.startswith("first_note_trigger_cancelled:"):
        return "first_note", "first_note_trigger_cancelled", False
    return stage or (UNKNOWN if primary else None), None, False


def manifest_info(round_dir, track_id, level, entry, issues):
    path = local_file(round_dir, "manifest.json")
    if path is None:
        return None, "missing", {}
    try:
        data, raw = read_object(path, MANIFEST_CAP)
    except (OSError, ValueError, RecursionError):
        issues.append("manifest.json:unreadable")
        return str(path), "unreadable", {}
    expected_hash = string(entry.get("manifest_sha256"))
    if expected_hash and hashlib.sha256(raw).hexdigest() != expected_hash:
        issues.append("manifest.json:hash_mismatch")
        return str(path), "hash_mismatch", {}
    if (data.get("schema") != MANIFEST_SCHEMA or not track_id or data.get("song_id") != track_id
            or level == UNKNOWN or data.get("difficulty") != level or entry.get("manifest_identity_matched") is False):
        issues.append("manifest.json:identity_mismatch")
        return str(path), "identity_mismatch", {}
    return str(path), "read", data


def boolean_state(value, true_state, false_state):
    if value is True:
        return true_state
    return false_state if value is False else UNKNOWN


def transport_written_steps(manifest):
    events = obj(manifest.get("touch_stream")).get("events")
    if not isinstance(events, list):
        return None
    return sum(obj(event).get("message_write_ok") is True and
               (count(obj(obj(event).get("command")).get("intent_id")) or 0) > 0 for event in events)


def read_run_source(folder):
    """Load only small ledger/intent documents during filtering and ordering."""
    issues = []
    if folder.name.startswith("catalog-manual-panel-") or local_file(folder, "ledger.json"):
        ledger = read_metadata(folder, "ledger.json", issues)
        if string(ledger.get("schema")) not in LEDGER_SCHEMAS:
            issues.append("ledger.json:missing_or_unknown_schema")
            return "desktop_panel", {}, [{}], issues
        rounds = ledger.get("rounds")
        if not isinstance(rounds, list) or any(not isinstance(entry, dict) for entry in rounds):
            issues.append("ledger.json:invalid_rounds")
            rounds = []
        if not rounds:
            selected = ledger.get("selected_ids")
            if isinstance(selected, list) and len(selected) == 1 and isinstance(selected[0], str):
                return "desktop_panel", ledger, [{"library_id": selected[0]}], issues
        return "desktop_panel", ledger, rounds or [{}], issues
    if folder.name.startswith("computer-") or local_file(folder, "intent.json"):
        intent = read_metadata(folder, "intent.json", issues)
        if intent.get("schema") != CONTROL_SCHEMA:
            issues.append("intent.json:missing_or_unknown_schema")
            return "computer_control", {}, [{}], issues
        return "computer_control", intent, [dict(intent, order=1)], issues
    return None


def identity(entry):
    library_id = string(entry.get("library_id"))
    parts = library_id.rsplit("/", 3) if library_id else []
    track_id = string(entry.get("track_id")) or (parts[-3] if len(parts) == 4 else None)
    level = entry.get("difficulty") or (parts[-2] if len(parts) == 4 else None)
    return library_id, track_id, difficulty(level)


def result_pin(entry):
    evidence = entry.get("evidence_files")
    if not isinstance(evidence, list):
        return None
    # The legacy file can be absolute and contain either path separator. Use
    # its basename as a label only; read exclusively the local round child.
    pins = [sha256_pin(item.get("sha256")) for item in evidence if isinstance(item, dict)
            and isinstance(item.get("file"), str) and re.split(r"[/\\]", item["file"])[-1] == "result.png"]
    return pins[0] if len(pins) == 1 else None


def build_row(folder, origin, source, entry, issues):
    issues = list(issues)
    order = count(entry.get("order"))
    round_id = f"round-{order:02d}" if order and order <= 9999 else None
    round_dir = folder / round_id if round_id else None
    library_id, track_id, level = identity(entry)
    utc, local = time_fields(folder.name, entry.get("started_utc") or source.get("started_utc"))
    path, manifest_state, manifest = (manifest_info(round_dir, track_id, level, entry, issues)
                                      if round_dir else (None, "missing", {}))
    primary, cleanup = split_legacy_failure(string(entry.get("runtime_failure")) or string(manifest.get("failure")))
    raw_failure = string(entry.get("failure"))
    if not round_id:
        raw_failure = raw_failure or string(source.get("failure"))
    controller_failure = string(entry.get("controller_failure"))
    structured_cleanup = entry.get("cleanup_errors")
    if isinstance(structured_cleanup, list):
        cleanup = [item for item in structured_cleanup if isinstance(item, (str, dict))]
    primary = string(entry.get("failure_detail")) or primary
    if primary is None and not cleanup:
        primary = raw_failure
    failure_stage, failure_code, legacy_ambiguous = classify_failure(
        primary, string(entry.get("failure_stage")), string(entry.get("failure_code")))
    execution = state(entry.get("execution_state"), {"not_started", "in_progress", "incomplete", "completed"})
    result = state(entry.get("result_state"), {"not_confirmed", "waiting_for_confirmation", "confirmed_by_native"})
    release = state(entry.get("release_state"), {"kernel_zero_confirmed", "kernel_zero_not_confirmed"})
    if manifest_state != "read":
        # A ledger can survive a missing or changed manifest. Show active and
        # preflight status but never resurrect terminal evidence from it.
        if execution in {"completed", "incomplete"}:
            execution = UNKNOWN
        if result == "confirmed_by_native":
            result = UNKNOWN
        release = UNKNOWN
    elif origin == "computer_control":
        execution = boolean_state(manifest.get("whole_song_sequence_complete"), "completed", "incomplete")
        result = boolean_state(manifest.get("manual_result_confirmed"), "confirmed_by_native", "not_confirmed")
        release = boolean_state(manifest.get("kernel_final_zero"), "kernel_zero_confirmed", "kernel_zero_not_confirmed")
    completion = read_metadata(folder, "completion.json", issues) if origin == "computer_control" else {}
    exit_code = (completion if origin == "computer_control" else entry).get("exit_code")
    if not round_id and source and not issues:
        execution = "not_started"
    result_file = local_file(round_dir, "result.png") if round_dir else None
    judgment = "unreviewed" if manifest_state == "read" else UNKNOWN
    return {
        "run_id": folder.name, "round_id": round_id,
        "attempt_id": string(entry.get("attempt_id")), "origin": origin, "launch_id": string(source.get("launch_id")),
        "started_utc": utc, "started_local_unqualified": local,
        "track_id": track_id, "title": string(entry.get("title")) or string(entry.get("display_name")),
        "difficulty": level, "library_id": library_id,
        "run_status": string(source.get("status")), "round_status": string(entry.get("status")),
        "exit_code": exit_code if type(exit_code) is int else None,
        "stop_requested": entry.get("stop_requested") if type(entry.get("stop_requested")) is bool else None,
        "execution_state": execution, "result_state": result, "release_state": release,
        "judgment_state": judgment, "native_judgment_state": judgment, "manual_judgments": None,
        "planned_contacts": count(manifest.get("full_chart_notes")),
        "planned_steps": count(manifest.get("full_compiled_steps")),
        "down_attempts": count(manifest.get("down_attempts")), "up_attempts": count(manifest.get("up_attempts")),
        "receipt_rows": count(manifest.get("receipt_rows")),
        "transport_written_steps": transport_written_steps(manifest),
        "configuration_sha256": string(source.get("configuration_sha256")),
        "library_sha256": string(obj(source.get("library")).get("sha256")),
        "frontend_release_sha256": string(obj(source.get("frontend_release")).get("sha256")),
        "executable_sha256": string(manifest.get("executable_sha256")),
        "phase_ms": manifest.get("experimental_phase_ms") if type(manifest.get("experimental_phase_ms")) is int else None,
        "boot_id": string(manifest.get("runtime_kernel_boot_id")),
        "failure_stage": failure_stage, "failure_code": failure_code, "failure_detail": primary,
        "controller_failure": controller_failure, "cleanup_errors": cleanup,
        "legacy_first_note_gate_ambiguous": legacy_ambiguous,
        "first_note_gate": obj(entry.get("first_note_gate")) or None,
        "milestones": obj(entry.get("milestones")), "timing": obj(entry.get("timing")),
        "run_directory": str(folder), "manifest_file": path, "manifest_state": manifest_state,
        "manifest_sha256": sha256_pin(entry.get("manifest_sha256")) if manifest_state == "read" else None,
        "result_file": str(result_file) if result_file is not None else None,
        "result_sha256": result_pin(entry),
        "issues": issues,
    }


def alnum(value):
    return "".join(char for char in value.casefold() if char.isalnum())


def review_name(row):
    return f"{row['run_id']}__{row['round_id']}.json"


def valid_judgments(values, row):
    if set(values) != set(JUDGMENTS) or any(count(value) is None or value > 2_000_000 for value in values.values()):
        return False
    return row["planned_contacts"] is None or sum(values.values()) == row["planned_contacts"]


def review_identity(row):
    if (row["origin"] != "desktop_panel" or row["execution_state"] != "completed"
            or row["result_state"] != "confirmed_by_native" or not row["result_file"]
            or not row["result_sha256"] or not row["manifest_sha256"]):
        raise ValueError("A complete desktop result with pinned result.png and manifest is required")
    path = Path(row["result_file"])
    if local_file(path.parent, "result.png") is None:
        raise ValueError("Pinned result.png is missing")
    with path.open("rb") as stream:
        raw = stream.read(RESULT_CAP + 1)
    if not raw or len(raw) > RESULT_CAP or hashlib.sha256(raw).hexdigest() != row["result_sha256"]:
        raise ValueError("result.png does not match the archived SHA256")
    return {key: row[key] for key in ("run_id", "round_id", "manifest_sha256", "result_sha256")}


def add_review(row, reviews_dir):
    if not row["round_id"]:
        return
    path = local_file(reviews_dir, review_name(row))
    if path is None:
        return
    try:
        review, _ = read_object(path)
        values = obj(review.get("judgments"))
        if (review.get("schema") != REVIEW_SCHEMA or review.get("source") != "manual"
                or not valid_judgments(values, row)
                or any(review.get(key) != value for key, value in review_identity(row).items())):
            raise ValueError("Review identity or counts do not match")
    except (OSError, ValueError, RecursionError):
        row["issues"].append("manual_review:invalid_or_stale")
        return
    row["judgment_state"] = "manually_reported"
    row["manual_judgments"] = values


def record_review(runs_dir, selector, values):
    run_id, separator, round_id = selector.partition("/")
    if not separator or not run_id or not ROUND_ID.fullmatch(round_id):
        raise ValueError("Review selector must be RUN_ID/round-01")
    root = runs_dir.resolve(strict=True)
    indexed = history(root, run_ids={run_id}, include_reviews=False)
    rows = [row for row in indexed["rows"] if row["round_id"] == round_id]
    if len(rows) != 1:
        raise ValueError("Expected one matching round")
    row = rows[0]
    if not valid_judgments(values, row):
        raise ValueError("Counts must be nonnegative and sum to the planned chart notes when known")
    review = {"schema": REVIEW_SCHEMA, "source": "manual", "reviewed_utc": datetime.now(timezone.utc).isoformat(),
              **review_identity(row), "judgments": values}
    reviews_dir = root.parent / "score-reviews"
    reviews_dir.mkdir(exist_ok=True)
    if reviews_dir.resolve(strict=True) != reviews_dir:
        raise ValueError("score-reviews must be a local directory")
    destination = reviews_dir / review_name(row)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=reviews_dir,
                                         prefix=".review-", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(review, stream, indent=2)
            stream.write("\n")
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return {**review, "review_file": str(destination)}


def utc_datetime(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset().total_seconds() != 0:
        raise ValueError("Archive boundaries must be UTC timestamps")
    return parsed


def archive_index(root, archive_id=None):
    directory = root.parent / "completed-archives"
    if archive_id is not None and not ARCHIVE_ID.fullmatch(archive_id):
        raise ValueError("Invalid completed archive ID")
    if not directory.exists():
        if archive_id is not None:
            raise ValueError(f"Completed archive not found: {archive_id}")
        return {}
    if not directory.is_dir() or directory.resolve(strict=True) != directory:
        raise ValueError("completed-archives must be a local directory")
    names = [archive_id + ".json"] if archive_id else sorted(path.name for path in directory.glob("*.json"))
    index = {}
    for name in names:
        path = local_file(directory, name)
        if path is None:
            if archive_id is not None:
                raise ValueError(f"Completed archive not found: {archive_id}")
            continue
        try:
            data, _ = read_object(path)
            if (data.get("schema") != ARCHIVE_SCHEMA or data.get("archive_id") != path.stem
                    or not ARCHIVE_ID.fullmatch(path.stem) or not isinstance(data.get("rounds"), list)
                    or count(data.get("record_count")) != len(data["rounds"])):
                raise ValueError(f"Invalid completed archive: {name}")
            keys = set()
            for record in data["rounds"]:
                item = obj(record)
                key = (string(item.get("run_id")), string(item.get("round_id")))
                if (not all(key) or not ROUND_ID.fullmatch(key[1]) or not sha256_pin(item.get("manifest_sha256"))
                        or not sha256_pin(item.get("result_sha256")) or key in keys):
                    raise ValueError(f"Invalid round pin in completed archive: {name}")
                keys.add(key)
            for item in data["rounds"]:
                index.setdefault((item["run_id"], item["round_id"]), {})[path.stem] = item
        except (OSError, ValueError, RecursionError):
            if archive_id is not None:
                raise
    return index


def completed_round_identity(row):
    if (row["round_status"] != "sent_result_unreviewed" or row["exit_code"] != 0
            or row["release_state"] != "kernel_zero_confirmed" or row["failure_detail"]
            or row["cleanup_errors"] or row["controller_failure"] or not row["planned_steps"]
            or row["transport_written_steps"] != row["planned_steps"]
            or row["receipt_rows"] != row["planned_steps"]):
        raise ValueError(f"Completed round has inconsistent evidence: {row['run_id']}/{row['round_id']}")
    return review_identity(row)


def mark_archives(row, index):
    row["archive_ids"] = []
    matches = index.get((row["run_id"], row["round_id"]))
    if not matches:
        return
    try:
        pins = completed_round_identity(row)
    except (OSError, ValueError):
        row["issues"].append("completed_archive:evidence_unavailable")
        return
    for archive_id, record in matches.items():
        if all(record.get(key) == value for key, value in pins.items()):
            row["archive_ids"].append(archive_id)
        else:
            row["issues"].append(f"completed_archive:pin_mismatch:{archive_id}")


def history(runs_dir, track=None, level=None, limit=None, run_ids=None, include_reviews=True,
            archive_id=None, archived_only=False, utc_window=None):
    root = runs_dir.resolve(strict=True)
    archives = archive_index(root, archive_id)
    archived_only = archived_only or archive_id is not None
    candidates = []
    for folder in root.iterdir():
        if run_ids is not None and folder.name not in run_ids:
            continue
        if not folder.is_dir() or folder.resolve() != folder:
            continue
        run_source = read_run_source(folder)
        if run_source is None:
            continue
        origin, source, entries, issues = run_source
        for entry in entries:
            order = count(entry.get("order"))
            if archived_only and (not order or (folder.name, f"round-{order:02d}") not in archives):
                continue
            _, track_id, entry_level = identity(entry)
            if level and entry_level != level:
                continue
            title = string(entry.get("title")) or string(entry.get("display_name"))
            haystack = (track_id or "") + " " + (title or "")
            if track and track.casefold() not in haystack.casefold():
                normalized = alnum(track)
                if not normalized or normalized not in alnum(haystack):
                    continue
            utc, local = time_fields(folder.name, entry.get("started_utc") or source.get("started_utc"))
            if utc_window is not None and (not utc or not utc_window[0] <= utc_datetime(utc) <= utc_window[1]):
                continue
            key = (local or utc or "", folder.name, order or 0)
            candidates.append((key, folder, origin, source, entry, issues))
    candidates.sort(key=lambda item: item[0], reverse=True)
    rows = []
    for candidate in candidates if archived_only else candidates[:limit]:
        row = build_row(*candidate[1:])
        mark_archives(row, archives)
        if archived_only and not row["archive_ids"]:
            continue
        if include_reviews:
            add_review(row, root.parent / "score-reviews")
        rows.append(row)
    return {"schema": SCHEMA, "total_matching": len(rows) if archived_only else len(candidates),
            "rows": rows[:limit] if archived_only else rows}


def archive_completed(runs_dir, archive_id, since_utc, until_utc):
    if not ARCHIVE_ID.fullmatch(archive_id):
        raise ValueError("Invalid completed archive ID")
    start, end = utc_datetime(since_utc), utc_datetime(until_utc)
    if start > end:
        raise ValueError("Archive end precedes its start")
    root = runs_dir.resolve(strict=True)
    rows = history(root, include_reviews=False, utc_window=(start, end))["rows"]
    records = []
    keys = set()
    for row in rows:
        if row["origin"] != "desktop_panel" or row["round_status"] != "sent_result_unreviewed":
            continue
        record = completed_round_identity(row)
        key = (record["run_id"], record["round_id"])
        if key in keys:
            raise ValueError(f"Duplicate completed round: {key[0]}/{key[1]}")
        keys.add(key)
        record.update({key: row[key] for key in (
            "started_utc", "track_id", "title", "difficulty", "planned_contacts", "planned_steps",
            "transport_written_steps", "execution_state", "result_state", "release_state", "native_judgment_state",
            "configuration_sha256", "library_sha256", "executable_sha256",
        )})
        records.append(record)
    if not records:
        raise ValueError("No completed desktop rounds in the requested window")
    directory = root.parent / "completed-archives"
    directory.mkdir(exist_ok=True)
    if directory.resolve(strict=True) != directory:
        raise ValueError("completed-archives must be a local directory")
    destination = directory / f"{archive_id}.json"
    data = {"schema": ARCHIVE_SCHEMA, "archive_id": archive_id, "since_utc": start.isoformat(),
            "until_utc": end.isoformat(), "record_count": len(records), "rounds": records}
    encoded = (json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
    if len(encoded) > METADATA_CAP:
        raise ValueError("Completed archive exceeds the metadata limit")
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=directory, prefix=".archive-", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        try:
            # Publish the completed file without ever replacing an existing batch.
            os.link(temporary, destination)
        except FileExistsError:
            existing = local_file(directory, destination.name)
            if existing is None or read_object(existing)[1] != encoded:
                raise ValueError(f"Completed archive already exists with different contents: {archive_id}") from None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return {**data, "archive_file": str(destination)}


# Frozen expectations derived from the six preserved desktop records. This
# acceptance compares the index with the old records, never replays a song.
NOV03 = {
    "151320-613-7d95f4": ("BANGINGSTRIKE.DewPleiades.0", "AT", 7960, 0, "source_timestamp_jump", False),
    "151741-169-8f2b29": ("BANGINGSTRIKE.DewPleiades.0", "AT", 7960, 0, LEGACY_GATE, True),
    "152137-625-7d1616": ("ERABYECONNEC10N.かめりあ.0", "IN", 10044, 10044, None, True),
    "152601-504-690b53": ("ERABYECONNEC10N.かめりあ.0", "AT", 5527, 5527, None, True),
    "153027-331-0eedab": ("GOODRAGE.EBIMAYO.0", "IN", 3395, 0, LEGACY_GATE, True),
    "153120-179-669b50": ("GOODRAGE.EBIMAYO.0", "IN", 3395, 3395, None, True),
}
NOV03_PREFIX = "catalog-manual-panel-20261103-"
NOV03_CONFIG = "c80b55f9cc36dcac3c93f830aaa608615eade293030ba87e54e037564895b0e3"
NOV03_LIBRARY = "c84c40107d1b727d92bf55f2bc728bdf27d4f5adc5fd7cf9d448f55d1b53b642"
NOV03_EXECUTABLE = "efb08c976e68dab64cf188c0f3614784986efe608e29388b35f21a415c3df5cb"


def accept_nov03(runs_dir):
    expected_ids = {NOV03_PREFIX + suffix for suffix in NOV03}
    indexed = history(runs_dir, run_ids=expected_ids, include_reviews=False)
    grouped = {}
    for row in indexed["rows"]:
        grouped.setdefault(row["run_id"], []).append(row)
    checks = []
    for suffix, expected in NOV03.items():
        track, level, planned, receipts, failure, kernel_zero = expected
        run_id = NOV03_PREFIX + suffix
        candidates = grouped.get(run_id, [])
        errors = []
        if len(candidates) != 1:
            errors.append(f"expected one round, found {len(candidates)}")
        row = candidates[0] if candidates else None
        if row:
            fields = {
                "origin": "desktop_panel", "round_id": "round-01", "manifest_state": "read",
                "configuration_sha256": NOV03_CONFIG, "library_sha256": NOV03_LIBRARY,
                "executable_sha256": NOV03_EXECUTABLE, "phase_ms": -70,
                "track_id": track, "difficulty": level, "planned_steps": planned,
                "receipt_rows": receipts, "transport_written_steps": receipts, "failure_code": failure,
                "exit_code": 0 if receipts else 2, "execution_state": "completed" if receipts else "incomplete",
                "result_state": "confirmed_by_native" if receipts else "not_confirmed",
                "release_state": "kernel_zero_confirmed" if kernel_zero else "kernel_zero_not_confirmed",
                "judgment_state": "unreviewed", "legacy_first_note_gate_ambiguous": failure == LEGACY_GATE,
            }
            if not receipts:
                fields.update(down_attempts=0, up_attempts=0)
            for key, value in fields.items():
                if row[key] != value:
                    errors.append(f"{key}: expected {value!r}, found {row[key]!r}")
            if bool(row["result_file"]) != bool(receipts):
                errors.append("unexpected result.png presence")
            if failure == "source_timestamp_jump" and row["cleanup_errors"] != [{
                "stage": "kernel_final", "detail": "ADB read-only kernel observer snapshot: ADB timed out",
            }]:
                errors.append("expected separate kernel-final ADB timeout")
        observed = {key: row[key] for key in (
            "planned_steps", "transport_written_steps", "execution_state", "result_state", "release_state",
            "judgment_state", "failure_code", "cleanup_errors", "legacy_first_note_gate_ambiguous",
        )} if row else None
        checks.append({"run_id": run_id, "track_id": track, "difficulty": level, "passed": not errors,
                       "observed": observed, "errors": errors})
    return {"schema": "pas.round-history-acceptance.v1", "passed": all(check["passed"] for check in checks), "checks": checks}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs-dir", type=Path, default=DEFAULT_RUNS_DIR)
    parser.add_argument("--track", help="Case-insensitive track ID/title substring")
    parser.add_argument("--difficulty", choices=("IN", "AT", "EZ", "HD", "SP"))
    parser.add_argument("--limit", type=int)
    parser.add_argument("--format", choices=("text", "json"), default="text")
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--accept-nov03", action="store_true", help="Check the six archived Nov 3 records offline")
    actions.add_argument("--review", metavar="RUN_ID/ROUND_ID", help="Save a manual judgment review beside the run tree")
    actions.add_argument("--archive-completed", metavar="BATCH_ID", help="Pin completed desktop records in a UTC window")
    filters = parser.add_mutually_exclusive_group()
    filters.add_argument("--archive", metavar="BATCH_ID", help="Show verified rounds in the named completed archive")
    filters.add_argument("--archived-only", action="store_true", help="Show verified rounds from all completed archives")
    parser.add_argument("--since-utc", help="Inclusive UTC start of a completed archive")
    parser.add_argument("--until-utc", help="Inclusive UTC end of a completed archive")
    for judgment in JUDGMENTS:
        parser.add_argument(f"--{judgment}", type=int)
    args = parser.parse_args(argv)
    if args.limit is not None and args.limit < 1:
        parser.error("--limit must be positive")
    if (args.accept_nov03 or args.review or args.archive_completed is not None) and (
            args.archived_only or any(value is not None for value in (args.track, args.difficulty, args.limit, args.archive))):
        parser.error("History filters cannot be combined with an acceptance, review, or archive action")
    if args.archive_completed is not None:
        if args.since_utc is None or args.until_utc is None:
            parser.error("--archive-completed requires --since-utc and --until-utc")
    elif args.since_utc is not None or args.until_utc is not None:
        parser.error("Archive boundaries require --archive-completed")
    values = {judgment: getattr(args, judgment) for judgment in JUDGMENTS}
    if args.review and any(value is None for value in values.values()):
        parser.error("--review requires --perfect, --good, --bad and --miss")
    if not args.review and any(value is not None for value in values.values()):
        parser.error("Judgment counts require --review")
    try:
        if args.review:
            report = record_review(args.runs_dir, args.review, values)
        elif args.archive_completed is not None:
            report = archive_completed(args.runs_dir, args.archive_completed, args.since_utc, args.until_utc)
        elif args.accept_nov03:
            report = accept_nov03(args.runs_dir)
        else:
            report = history(args.runs_dir, args.track, args.difficulty, args.limit,
                             archive_id=args.archive, archived_only=args.archived_only)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"History unavailable: {error}", file=sys.stderr)
        return 2
    if args.format == "json":
        print(json.dumps(report, allow_nan=False))
    elif args.review:
        print(f"Manual judgment review saved: {report['review_file']}")
    elif args.archive_completed is not None:
        print(f"{report['record_count']} completed rounds archived at {report['archive_file']}")
    elif args.accept_nov03:
        for check in report["checks"]:
            verdict = "PASS" if check["passed"] else "FAIL"
            print(f"{verdict}  {check['track_id']} {check['difficulty']}  {check['run_id']}")
            observed = check["observed"]
            if observed:
                print(f"  transport writes {observed['transport_written_steps']}/{observed['planned_steps']}  "
                      f"{observed['result_state']} / {observed['release_state']} / {observed['judgment_state']}")
                if observed["failure_code"]:
                    note = " (ambiguous legacy gate)" if observed["legacy_first_note_gate_ambiguous"] else ""
                    print(f"  {observed['failure_code']}{note}")
                for cleanup in observed["cleanup_errors"]:
                    print(f"  cleanup: {cleanup}")
            for error in check["errors"]:
                print(f"  {error}")
        print(f"{sum(check['passed'] for check in report['checks'])}/6 archived records matched")
    else:
        for row in report["rows"]:
            started = row["started_utc"] or row["started_local_unqualified"] or "?"
            failure = row["failure_code"] or row["failure_detail"] or ""
            print(f"{started}  {row['track_id'] or '?'} {row['difficulty']}  {row['origin']}  "
                  f"{row['execution_state']} / {row['result_state']} / {row['release_state']} / {row['judgment_state']}  {failure}")
        print(f"{len(report['rows'])}/{report['total_matching']} matching rounds")
    return 1 if args.accept_nov03 and not report["passed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
