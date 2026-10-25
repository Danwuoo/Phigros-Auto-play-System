"""Read existing whole-song evidence and report per-Flick diagnostic candidates.

Inputs inside --run-dir: manifest.json, compiled-plan.json, receipts.jsonl;
optional hud-timeline.json and hud-timeline.gray. All formats are emitted by
apps/chart_experiment/main.cpp. Host RPC receipts do not prove Android/game
delivery. HUD pixels are verified and indexed, never OCR'd into a note verdict.
No source is modified; --output must name a new file outside the run directory.
"""

import argparse
import bisect
import hashlib
import json
import math
from pathlib import Path
import re


NS = 1_000_000_000
MAX_STEPS = 20000
MAX_CONTACTS = MAX_STEPS // 2
MAX_PAIRS = 1000000
PHASE_ORDER = {2: 0, 1: 1, 0: 2}  # Native release, move, down tie order.
SOURCE_POINTER = re.compile(r"^/judgeLineList/(\d+)/notes(Above|Below)/(\d+)$")


def integer(value, name, lower=0, upper=2**63 - 1):
    if type(value) is not int or not lower <= value <= upper:
        raise ValueError(name + " must be a bounded integer")
    return value


def number(value, name, lower=-math.inf, upper=math.inf):
    if type(value) not in (int, float) or not math.isfinite(value) or not lower <= value <= upper:
        raise ValueError(name + " must be a bounded finite number")
    return float(value)


def boolean(value, name):
    if type(value) is not bool:
        raise ValueError(name + " must be bool")
    return value


def sha(value, name):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise ValueError(name + " must be lowercase SHA-256")
    return value


def object_value(value, name):
    if not isinstance(value, dict):
        raise ValueError(name + " must be a JSON object")
    return value


def hash_file(path, limit):
    path = Path(path)
    size = path.stat().st_size
    if size > limit:
        raise ValueError("File exceeds diagnostic budget: " + str(path))
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return dict(file=str(path.resolve()), sha256=digest.hexdigest(), bytes=size)


def read_json(path, limit):
    ref = hash_file(path, limit)
    raw = Path(path).read_bytes()
    if hashlib.sha256(raw).hexdigest() != ref["sha256"]:
        raise ValueError("Input changed while reading")
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError("JSON input must be an object: " + str(path))
    return value, ref


def load_plan(value):
    if value.get("schema") != "pas.experimental-whole-song-compiled.v1":
        raise ValueError("Expected native compiled-plan.json whole-song schema")
    chart_sha = sha(value.get("chart_sha256"), "plan chart hash")
    sha(value.get("cache_sha256"), "compiled cache hash")
    if value.get("input_geometry") != [1280, 720, 0]:
        raise ValueError("Only the native 1280x720 unrotated compiler surface is supported")
    contacts, ownership = value.get("contacts"), value.get("ownership")
    if (not isinstance(contacts, list) or not 1 <= len(contacts) <= MAX_CONTACTS
            or not isinstance(ownership, list) or len(ownership) != len(contacts)):
        raise ValueError("Invalid contact/ownership count")
    owners, source_ids = {}, set()
    for owner in ownership:
        object_value(owner, "note ownership")
        contact_id = integer(owner.get("contact_id"), "source contact", 1)
        pointer = owner.get("json_pointer")
        if not isinstance(pointer, str) or not SOURCE_POINTER.fullmatch(pointer):
            raise ValueError("Invalid native note JSON pointer")
        note_id = owner.get("source_note_id")
        if note_id != chart_sha + "#" + pointer or note_id in source_ids or contact_id in owners:
            raise ValueError("Duplicate/mismatched source note identity")
        source_ids.add(note_id)
        integer(owner.get("kind"), "note kind", 1, 4)
        integer(owner.get("judgment_ns"), "judgment time")
        owners[contact_id] = owner
    parsed, seen, total = [], set(), 0
    for raw in contacts:
        object_value(raw, "contact")
        contact_id = integer(raw.get("id"), "contact id", 1)
        if contact_id in seen or contact_id not in owners:
            raise ValueError("Duplicate contact or missing ownership")
        seen.add(contact_id)
        steps = raw.get("steps")
        if not isinstance(steps, list) or not 2 <= len(steps) <= MAX_STEPS - total:
            raise ValueError("Invalid/budgeted contact steps")
        points = []
        previous = -1
        for index, step in enumerate(steps):
            object_value(step, "contact step")
            time = integer(step.get("song_ns"), "step time")
            phase = integer(step.get("phase"), "step phase", 0, 2)
            expected_phase = 0 if index == 0 else 2 if index == len(steps) - 1 else 1
            if phase != expected_phase or time <= previous:
                raise ValueError("Invalid closed contact lifecycle/order")
            point = step.get("point")
            if not isinstance(point, list) or len(point) != 2:
                raise ValueError("Invalid normalized point")
            # C++ pixel_point uses lround for nonnegative normalized values.
            x = math.floor(number(point[0], "normalized x", 0, 1) * 1279 + 0.5)
            y = math.floor(number(point[1], "normalized y", 0, 1) * 719 + 0.5)
            points.append(dict(song_ns=time, phase=phase, x=x, y=y, step_index=index))
            previous = time
        total += len(steps)
        parsed.append(dict(contact_id=contact_id, owner=owners[contact_id], steps=points))
    if total != integer(value.get("total_steps"), "declared steps", 2, MAX_STEPS):
        raise ValueError("Compiled total_steps mismatch")
    # experimental_runtime.hpp assigns intent IDs by sorted birth, not source
    # note index, not physical pointer ID, and not compiler contact order.
    parsed.sort(key=lambda contact: (contact["steps"][0]["song_ns"], contact["contact_id"]))
    for intent, contact in enumerate(parsed, 1):
        contact["intent_id"] = intent
    return parsed


def load_receipts(path):
    if not path.is_file():
        return [], None
    ref = hash_file(path, 32 * 1024 * 1024)
    rows = []
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for line_number, raw in enumerate(stream, 1):
            digest.update(raw)
            if len(raw) > 16384 or len(rows) >= MAX_STEPS:
                raise ValueError("Receipt row/line budget exceeded")
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict) or row.get("event") != "rpc":
                raise ValueError("receipts.jsonl must contain native rpc rows")
            phase = integer(row.get("phase"), "receipt phase", 0, 2)
            pointer = integer(row.get("id"), "physical pointer", 0, 9)
            intent = integer(row.get("intent_id"), "receipt intent", 1)
            scheduled = integer(row.get("scheduled_ns"), "scheduled time")
            sent = integer(row.get("host_start_ns"), "dispatch start")
            returned = integer(row.get("host_return_ns"), "dispatch return", sent)
            if rows and sent < rows[-1]["host_start_ns"]:
                raise ValueError("Receipt dispatch clock moves backwards")
            rows.append(dict(row=line_number, phase=phase, pointer_id=pointer, intent_id=intent,
                             scheduled_ns=scheduled, host_start_ns=sent, host_return_ns=returned,
                             x=number(row.get("x"), "receipt x", 0, 1279),
                             y=number(row.get("y"), "receipt y", 0, 719),
                             success=boolean(row.get("success"), "RPC success"),
                             reason=str(row.get("reason", ""))[:1024]))
    if digest.hexdigest() != ref["sha256"]:
        raise ValueError("Receipts changed while reading")
    return rows, ref


def path_metrics(points, time_key):
    stroke = [point for point in points if point["phase"] != 2]
    segments, total, reversals, last_vector = [], 0.0, 0, None
    for a, b in zip(stroke, stroke[1:]):
        elapsed = b[time_key] - a[time_key]
        dx, dy = b["x"] - a["x"], b["y"] - a["y"]
        distance = math.hypot(dx, dy)
        speed = distance * NS / elapsed if elapsed > 0 else None
        if distance:
            if last_vector is not None and dx * last_vector[0] + dy * last_vector[1] < 0:
                reversals += 1
            last_vector = (dx, dy)
        total += distance
        segments.append(dict(begin_ns=a[time_key], end_ns=b[time_key],
                             distance_pixels=distance, speed_pixels_per_second=speed))
    duration = stroke[-1][time_key] - stroke[0][time_key] if len(stroke) >= 2 else None
    displacement = math.hypot(stroke[-1]["x"] - stroke[0]["x"], stroke[-1]["y"] - stroke[0]["y"]) if stroke else None
    dwell = (points[-1][time_key] - stroke[-1][time_key]
             if stroke and points and points[-1]["phase"] == 2 else None)
    speeds = [segment["speed_pixels_per_second"] for segment in segments if segment["speed_pixels_per_second"] is not None]
    return dict(scope="command coordinates and timestamps only; not measured Android/game motion",
                stroke_duration_ns=duration, end_displacement_pixels=displacement,
                path_length_pixels=total, release_dwell_ns=dwell,
                max_segment_speed_pixels_per_second=max(speeds, default=None),
                min_segment_speed_pixels_per_second=min(speeds, default=None),
                zero_interval_segments=sum(segment["begin_ns"] == segment["end_ns"] for segment in segments),
                direction_reversals=reversals, segments=segments)


def overlap_metrics(events, flicks, proximity, *, actual=False):
    result = {intent: dict(peak_active_contacts=0, overlapping_intents=set(),
                          nearby_intents=set(), minimum_other_distance_pixels=None,
                          pointer_collisions=[], pointer_changes=[], pointer_reuse_gap_ns=None)
              for intent in flicks}
    active, pointer_owner, first_pointer, last_release = {}, {}, {}, {}
    pair_checks, uncertain = 0, False
    for event in events:
        intent, phase = event["intent_id"], event["phase"]
        if actual:
            pointer, time = event["pointer_id"], event["host_start_ns"]
            if not event["success"]:
                uncertain = True
                continue
            previous = pointer_owner.get(pointer)
            if phase == 0:
                if previous is not None and previous != intent:
                    for target in (intent, previous):
                        if target in result:
                            result[target]["pointer_collisions"].append(dict(row=event["row"], pointer_id=pointer, other_intent=previous if target == intent else intent))
                if intent in first_pointer and intent in result:
                    result[intent]["pointer_changes"].append(dict(row=event["row"], reason="duplicate_DOWN"))
                first_pointer[intent] = pointer
                if intent in result and pointer in last_release:
                    result[intent]["pointer_reuse_gap_ns"] = time - last_release[pointer]
                pointer_owner[pointer] = intent
            elif first_pointer.get(intent) != pointer or previous != intent:
                if intent in result:
                    result[intent]["pointer_changes"].append(dict(row=event["row"], reason="MOVE_or_UP_has_different_or_unowned_pointer"))
            if phase == 2 and previous == intent:
                del pointer_owner[pointer]
                # A new command can only follow the synchronous RPC return.
                last_release[pointer] = event["host_return_ns"]
        if phase == 2:
            active.pop(intent, None)
        elif phase == 0 or intent in active:
            active[intent] = (event["x"], event["y"])
        for target, point in active.items():
            if target not in result:
                continue
            record = result[target]
            record["peak_active_contacts"] = max(record["peak_active_contacts"], len(active))
            for other, other_point in active.items():
                if other == target:
                    continue
                pair_checks += 1
                if pair_checks > MAX_PAIRS:
                    raise ValueError("Overlap analysis work budget exceeded")
                record["overlapping_intents"].add(other)
                distance = math.hypot(point[0] - other_point[0], point[1] - other_point[1])
                minimum = record["minimum_other_distance_pixels"]
                record["minimum_other_distance_pixels"] = distance if minimum is None else min(distance, minimum)
                if distance <= proximity:
                    record["nearby_intents"].add(other)
    for record in result.values():
        record["overlapping_intents"] = sorted(record["overlapping_intents"])
        record["nearby_intents"] = sorted(record["nearby_intents"])
        record["scope"] = "successful host RPC command state; Android/game state unverified" if actual else "zero-order held compiled positions; UP before MOVE before DOWN"
        record["state_after_failed_RPC_uncertain"] = uncertain if actual else False
    return result


def hud_context(run_dir, manifest):
    path = run_dir / "hud-timeline.json"
    if not path.is_file():
        return dict(available=False, gaps=["hud_timeline_missing"]), []
    value, ref = read_json(path, 4 * 1024 * 1024)
    metadata_only = value.get("schema") == "pas.experimental-hud-timeline-metadata.v1"
    if value.get("schema") not in ("pas.experimental-hud-timeline.v1", "pas.experimental-hud-timeline-metadata.v1") or value.get("clock_id") != "host-QPC-receipt-only":
        raise ValueError("HUD schema/clock is not the native diagnostic timeline")
    if value.get("source_sha256") != manifest.get("source_sha256"):
        raise ValueError("HUD timeline belongs to a different capture source")
    samples = value.get("samples")
    if not isinstance(samples, list) or len(samples) > 3601 or value.get("sample_bytes") != 18720:
        raise ValueError("HUD sample layout/budget mismatch")
    if (value.get("frame_geometry") != [1280, 720]
            or value.get("score_region_xywh") != [1076, 20, 180, 40]
            or value.get("combo_region_xywh") != [560, 8, 160, 72]):
        raise ValueError("HUD geometry differs from native timeline")
    previous_sequence, previous_received = 0, -1
    for index, row in enumerate(samples):
        object_value(row, "HUD sample")
        sequence = integer(row.get("sequence"), "HUD sequence", previous_sequence + 1)
        received = integer(row.get("received_ns"), "HUD received", previous_received + 1)
        integer(row.get("processed_ns"), "HUD processed", received)
        for field in ("epoch", "generation", "geometry_version"):
            integer(row.get(field), "HUD " + field, 1)
        boolean(row.get("effective_HUD"), "HUD classification")
        if not boolean(row.get("source_valid"), "HUD source validity"):
            raise ValueError("Native timeline cannot contain an invalid source sample")
        if metadata_only:
            if row.get("byte_offset") is not None:
                raise ValueError("Metadata-only HUD must not claim a pixel offset")
        elif integer(row.get("byte_offset"), "HUD byte offset") != index * 18720:
            raise ValueError("HUD byte offset does not match native sample layout")
        previous_sequence, previous_received = sequence, received
    expected_bytes = 0 if metadata_only else len(samples) * 18720
    stats = object_value(value.get("stats"), "HUD stats")
    if value.get("pixels_bytes") != expected_bytes or stats.get("samples") != len(samples):
        raise ValueError("HUD declared sample counts differ")
    if metadata_only:
        if (value.get("pixels_retained") is not False or value.get("pixels_file") is not None
                or value.get("pixels_sha256") is not None or value.get("allocated_pixel_bytes") != 0):
            raise ValueError("Metadata-only HUD claims retained image data")
        return dict(available=True, index=ref, pixels=None, pixel_hash_verified=False,
                    sample_count=len(samples), gaps=["hud_pixels_not_retained_by_policy"], stats=stats,
                    scope="actual receipt-time metadata only; image retention disabled; no OCR or note verdict"), samples
    # Only the writer's fixed adjacent filename is accepted; do not follow
    # arbitrary paths embedded in imported evidence.
    if value.get("pixels_file") != "hud-timeline.gray":
        raise ValueError("Unexpected HUD pixel filename")
    pixels = run_dir / "hud-timeline.gray"
    gaps, pixel_ref = [], None
    if pixels.is_file():
        pixel_ref = hash_file(pixels, 3601 * 18720)
        if pixel_ref["bytes"] != expected_bytes or pixel_ref["sha256"] != sha(value.get("pixels_sha256"), "HUD pixels SHA"):
            raise ValueError("HUD pixel bytes/hash mismatch")
    else:
        gaps.append("hud_pixel_bytes_missing")
    return dict(available=True, index=ref, pixels=pixel_ref, pixel_hash_verified=pixel_ref is not None,
                sample_count=len(samples), gaps=gaps, stats=value.get("stats", {}),
                scope="receipt-time neighborhoods only; no OCR, note verdict or capture-age qualification"), samples


def manual_windows(path, manifest_ref, chart_sha, note_ids):
    if path is None:
        return [], None
    value, ref = read_json(path, 512 * 1024)
    if (value.get("schema") != "pas.manual-miss-windows.v1" or value.get("time_basis") != "song_ns"
            or value.get("run_manifest_sha256") != manifest_ref["sha256"] or value.get("chart_sha256") != chart_sha):
        raise ValueError("Manual windows must bind this manifest/chart and declare song_ns")
    rows = value.get("windows")
    if not isinstance(rows, list) or len(rows) > 256:
        raise ValueError("Manual window budget")
    seen = set()
    for row in rows:
        object_value(row, "manual window")
        identifier = row.get("id")
        if not isinstance(identifier, str) or not 1 <= len(identifier) <= 128 or identifier in seen:
            raise ValueError("Manual window ID must be unique and bounded")
        seen.add(identifier)
        begin = integer(row.get("begin_ns"), "manual begin")
        integer(row.get("end_ns"), "manual end", begin)
        evidence = row.get("evidence")
        if not isinstance(evidence, str) or not 1 <= len(evidence) <= 2048:
            raise ValueError("Manual window requires an evidence description")
        selected = row.get("note_ids", [])
        if (not isinstance(selected, list) or len(selected) > MAX_CONTACTS
                or any(not isinstance(note, str) or note not in note_ids for note in selected)):
            raise ValueError("Manual window references an unknown note")
    return rows, ref


def analyze_run(run_directory, *, miss_windows_path=None, proximity_pixels=48.0,
                lateness_ns=20_000_000):
    run_dir = Path(run_directory).resolve()
    proximity = number(proximity_pixels, "proximity threshold", 0, 500)
    lateness_limit = integer(lateness_ns, "diagnostic lateness threshold", 0, NS)
    manifest, manifest_ref = read_json(run_dir / "manifest.json", 2 * 1024 * 1024)
    plan, plan_ref = read_json(run_dir / "compiled-plan.json", 64 * 1024 * 1024)
    if manifest.get("schema") != "pas.experimental-whole-song.v1":
        raise ValueError("Expected whole-song runtime manifest")
    contacts = load_plan(plan)
    if (manifest.get("chart_sha256") != plan["chart_sha256"]
            or manifest.get("expected_plan_sha256") != plan["cache_sha256"]
            or manifest.get("full_chart_notes") != len(contacts)
            or manifest.get("full_compiled_steps") != plan["total_steps"]):
        raise ValueError("Manifest and compiled plan identities/counts differ")
    for key in ("flick_lead_ns", "flick_travel_ns", "flick_release_ns"):
        integer(plan.get(key), "compiled " + key, 1, NS)
    nominal = number(plan.get("flick_distance_reference_pixels"), "nominal Flick distance", 0.0001, 10000)
    for key in ("flick_lead_ns", "flick_travel_ns", "flick_release_ns", "flick_distance_reference_pixels"):
        if "experimental_" + key in manifest and manifest["experimental_" + key] != plan[key]:
            raise ValueError("Manifest and compiled Flick parameters differ")
    path_policy = plan.get("flick_path_policy", "dynamic-line-v1")
    if (path_policy not in ("dynamic-line-v1", "judgment-anchored-straight-v1")
            or manifest.get("experimental_flick_path_policy", "dynamic-line-v1") != path_policy):
        raise ValueError("Unknown or mismatched Flick path policy")
    receipts, receipt_ref = load_receipts(run_dir / "receipts.jsonl")
    gaps = ["game_per_note_judgment_not_observed", "Android_input_callbacks_not_observed",
            "moving_or_rotating_line_pose_not_reconstructed_from_compiled_steps"]
    if receipt_ref is None:
        gaps.append("receipts_missing")
    if manifest.get("receipt_rows") is not None and manifest["receipt_rows"] != len(receipts):
        gaps.append("manifest_receipt_count_mismatch")
    phase = integer(manifest.get("experimental_phase_ms"), "experimental phase", -100, 100)
    anchor = manifest.get("anchor")
    offset = None
    if isinstance(anchor, dict) and anchor.get("origin_host_ns") is not None:
        offset = integer(anchor["origin_host_ns"], "arrival origin") + phase * 1_000_000
    else:
        gaps.append("arrival_anchor_missing")
    all_intents = {contact["intent_id"]: contact for contact in contacts}
    flicks = {intent for intent, contact in all_intents.items() if contact["owner"]["kind"] == 4}
    receipt_groups = {intent: [] for intent in all_intents}
    unknown_rows = []
    for row in receipts:
        if row["intent_id"] in receipt_groups:
            receipt_groups[row["intent_id"]].append(row)
        else:
            unknown_rows.append(row["row"])
    if unknown_rows:
        gaps.append("receipts_with_unknown_intents")
    planned_events = [dict(step, intent_id=contact["intent_id"], source_contact_id=contact["contact_id"])
                      for contact in contacts for step in contact["steps"]]
    planned_events.sort(key=lambda row: (row["song_ns"], PHASE_ORDER[row["phase"]], row["source_contact_id"]))
    planned_overlap = overlap_metrics(planned_events, flicks, proximity)
    dispatched_overlap = overlap_metrics(receipts, flicks, proximity, actual=True)
    hud, samples = hud_context(run_dir, manifest)
    received_times = [sample["received_ns"] for sample in samples]
    windows, windows_ref = manual_windows(miss_windows_path, manifest_ref, plan["chart_sha256"],
                                         {contact["owner"]["source_note_id"] for contact in contacts})
    rows = []
    for contact in contacts:
        owner, intent, steps = contact["owner"], contact["intent_id"], contact["steps"]
        if intent not in flicks:
            continue
        sent_rows = receipt_groups[intent]
        by_time = {}
        for row in sent_rows:
            by_time.setdefault(row["scheduled_ns"], []).append(row)
        step_rows, candidates, missing = [], [], []
        matched_rows = set()
        for step in steps:
            due = step["song_ns"] + offset if offset is not None else None
            candidates_at_time = by_time.get(due, []) if due is not None else []
            receipt = candidates_at_time[0] if len(candidates_at_time) == 1 else None
            matches = bool(receipt and receipt["phase"] == step["phase"] and
                           receipt["x"] == step["x"] and receipt["y"] == step["y"])
            if matches:
                matched_rows.add(receipt["row"])
            elif due is not None:
                missing.append(step["step_index"])
            step_rows.append(dict(**step, expected_host_ns=due, matched_receipt_row=receipt["row"] if matches else None,
                                  host_start_ns=receipt["host_start_ns"] if matches else None,
                                  host_return_ns=receipt["host_return_ns"] if matches else None,
                                  dispatch_lateness_ns=receipt["host_start_ns"] - due if matches else None,
                                  rpc_duration_ns=receipt["host_return_ns"] - receipt["host_start_ns"] if matches else None,
                                  rpc_success=receipt["success"] if matches else None))
        unexpected = [row["row"] for row in sent_rows if row["row"] not in matched_rows]
        planned = path_metrics(steps, "song_ns")
        dispatch = path_metrics(sent_rows, "host_start_ns")
        late = [row["dispatch_lateness_ns"] for row in step_rows if row["dispatch_lateness_ns"] is not None]
        if missing:
            candidates.append("missing_or_mismatched_command_receipts")
        if unexpected:
            candidates.append("unexpected_or_duplicate_command_receipts")
        if any(not row["success"] for row in sent_rows):
            candidates.append("RPC_failure_or_unknown_effect")
        if late and max(late) > lateness_limit:
            candidates.append("dispatch_lateness_above_diagnostic_threshold")
        if late and min(late) < 0:
            candidates.append("dispatch_before_pinned_deadline")
        if dispatched_overlap[intent]["pointer_collisions"]:
            candidates.append("pointer_reused_before_successful_UP")
        if dispatched_overlap[intent]["pointer_changes"]:
            candidates.append("pointer_continuity_mismatch")
        if planned["end_displacement_pixels"] is not None and planned["end_displacement_pixels"] < nominal * .25:
            candidates.append("screen_displacement_below_quarter_nominal_stroke")
        full_match = len(matched_rows) == len(steps) == len(sent_rows) and all(row["success"] for row in sent_rows)
        compression_ratio = None
        if full_match and planned["stroke_duration_ns"] and dispatch["stroke_duration_ns"] is not None:
            compression_ratio = dispatch["stroke_duration_ns"] / planned["stroke_duration_ns"]
            if compression_ratio < .5:
                candidates.append("host_dispatch_stroke_duration_below_half_planned")
        if dispatch["zero_interval_segments"]:
            candidates.append("multiple_moves_share_host_dispatch_timestamp")
        projected = (number(owner.get("projection_min_reference_pixels", 0), "projection min") != 0 or
                     number(owner.get("projection_max_reference_pixels", 0), "projection max") != 0)
        if projected:
            candidates.append("normal_projection_requires_geometry_review")
        if planned_overlap[intent]["nearby_intents"] or dispatched_overlap[intent]["nearby_intents"]:
            candidates.append("nearby_contact_interference_candidate")
        judgment = owner["judgment_ns"]
        applicable = [window for window in windows if window["begin_ns"] <= judgment <= window["end_ns"]
                      and (not window.get("note_ids") or owner["source_note_id"] in window["note_ids"])]
        if applicable:
            candidates.append("within_manually_annotated_miss_window")
        host_judgment = judgment + offset if offset is not None else None
        neighbors = []
        if host_judgment is not None and samples:
            index = bisect.bisect_left(received_times, host_judgment)
            for sample_index in sorted({max(0, index - 1), min(len(samples) - 1, index)}):
                sample = samples[sample_index]
                neighbors.append(dict(index=sample_index, sequence=sample["sequence"], received_ns=sample["received_ns"],
                                      delta_from_hypothesis_judgment_ns=sample["received_ns"] - host_judgment,
                                      byte_offset=sample["byte_offset"]))
        pointer = SOURCE_POINTER.fullmatch(owner["json_pointer"])
        note_gaps = list(gaps)
        if not hud.get("pixel_hash_verified"):
            note_gaps.append("verified_HUD_pixels_unavailable")
        if not full_match:
            note_gaps.append("complete_successful_host_command_sequence_not_established")
        rows.append(dict(source_note_id=owner["source_note_id"], source_json_pointer=owner["json_pointer"],
                         line_index=int(pointer.group(1)), side=pointer.group(2).lower(), note_index=int(pointer.group(3)),
                         source_contact_id=contact["contact_id"], intent_id=intent,
                         pointer_ids=sorted({row["pointer_id"] for row in sent_rows}),
                         judgment_song_ns=judgment, hypothesis_judgment_host_ns=host_judgment,
                         planned_lead_ns=judgment - steps[0]["song_ns"], planned=planned,
                         host_dispatch=dispatch, dispatch_to_plan_duration_ratio=compression_ratio,
                         max_dispatch_lateness_ns=max(late, default=None), min_dispatch_lateness_ns=min(late, default=None),
                         matched_steps=len(matched_rows), expected_steps=len(steps), receipt_rows=len(sent_rows),
                         missing_or_mismatched_step_indices=missing, unexpected_receipt_rows=unexpected,
                         all_planned_steps_have_successful_host_receipts=full_match,
                         planned_overlap=planned_overlap[intent], host_dispatch_overlap=dispatched_overlap[intent],
                         projection=dict(min_reference_pixels=owner.get("projection_min_reference_pixels"),
                                         max_reference_pixels=owner.get("projection_max_reference_pixels")),
                         hud_neighbors=neighbors, manual_windows=applicable,
                         candidates=candidates, evidence_gaps=note_gaps, steps=step_rows,
                         judgment_status="not_determined_by_tool", game_miss_confirmed=False))
    counts = {}
    for row in rows:
        for candidate in row["candidates"]:
            counts[candidate] = counts.get(candidate, 0) + 1
    return dict(schema="pas.flick-diagnostics.v1", song_id=manifest.get("song_id"), difficulty=manifest.get("difficulty"),
                chart_sha256=plan["chart_sha256"], compiled_fingerprint=plan["cache_sha256"],
                sources=dict(manifest=manifest_ref, compiled_plan=plan_ref, receipts=receipt_ref, manual_windows=windows_ref),
                parameters={**{key: plan.get(key) for key in ("flick_lead_ns", "flick_travel_ns", "flick_release_ns", "flick_distance_reference_pixels")},
                            "flick_path_policy": path_policy},
                experimental_phase_ms=phase, clock_model="manifest arrival-origin + explicit phase + song_ns; rate=1 hypothesis only",
                thresholds=dict(dispatch_lateness_ns=lateness_limit, nearby_contact_pixels=proximity,
                                screen_displacement_fraction=.25, host_stroke_duration_fraction=.5,
                                scope="diagnostic candidate thresholds, not Phigros judgment windows or recommended tuning"),
                summary=dict(flick_count=len(rows), candidate_counts=counts, unknown_intent_receipt_rows=unknown_rows,
                             game_misses_identified=0, AP_verified=False),
                runtime=dict(status=manifest.get("status"), failure=manifest.get("failure"),
                             sequence_complete=manifest.get("whole_song_sequence_complete"),
                             final_kernel_zero=manifest.get("kernel_final_zero")),
                hud=hud, evidence_gaps=gaps, notes=rows, input_enabled=False, source_files_modified=False,
                identity_scope="Native manifest/plan identity fields are cross-checked; input bytes are SHA-256 pinned. This JSON export lacks fields needed to independently recompute the native compiled fingerprint.",
                scope="Screen-space command trajectory and host transport diagnostics only. No game acceptance or per-note Miss is inferred from RPC success, HUD combo or missing receipts. Missing steps may follow a stopped round, rather than a game rejection.")


def write_report(result, path, run_directory):
    path = Path(path).resolve()
    root = Path(run_directory).resolve()
    if path == root or root in path.parents:
        raise ValueError("Diagnostic output must be outside the original run directory")
    with path.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--miss-windows", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--nearby-pixels", type=float, default=48.0)
    parser.add_argument("--lateness-ms", type=int, default=20)
    args = parser.parse_args()
    try:
        result = analyze_run(args.run_dir, miss_windows_path=args.miss_windows,
                             proximity_pixels=args.nearby_pixels, lateness_ns=args.lateness_ms * 1_000_000)
        if args.output:
            write_report(result, args.output, args.run_dir)
            print(json.dumps(result["summary"], ensure_ascii=False, indent=2))
        else:
            print(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, str(error) + "\n")


if __name__ == "__main__":
    main()
