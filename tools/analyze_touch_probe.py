"""Bounded, offline validation of PAS touch-probe and Android fixture evidence."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics

MAX_FILE = 18 * 1024 * 1024
MAX_LINE = 256 * 1024
MAX_RECORDS = 9000
PHASE_DOWN, PHASE_MOVE, PHASE_UP = 0, 1, 2


class Findings:
    def __init__(self):
        self.failed, self.incomplete = [], []

    def fail(self, code):
        if code not in self.failed and len(self.failed) < 64:
            self.failed.append(code)

    def missing(self, code):
        if code not in self.incomplete and len(self.incomplete) < 64:
            self.incomplete.append(code)


def _pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def _json(raw):
    def invalid(_):
        raise ValueError("nonfinite JSON number")
    value = json.loads(raw.decode("utf-8"), object_pairs_hook=_pairs, parse_constant=invalid)
    if not isinstance(value, dict):
        raise ValueError("expected JSON object")
    return value


def integer(value, minimum=0, maximum=(1 << 63) - 1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError("invalid bounded integer")
    return value


def number(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError("invalid finite number")
    return float(value)


def distribution(values):
    if not values:
        return {"count": 0, "minimum": None, "median": None, "p95": None, "maximum": None}
    values = sorted(values)
    return dict(count=len(values), minimum=values[0], median=statistics.median(values),
                p95=values[math.ceil(.95 * len(values)) - 1], maximum=values[-1])


def visual_evidence(value):
    """Validate the reported visual structure, without claiming to decode PNG pixels."""
    for key in ("sequence", "downs", "ups", "moves", "cancels", "visual_ring_overwrites"):
        integer(value[key], 0, 0xFFFFFF)
    integer(value["active"], 0, 32)
    if type(value["complete"]) is not bool or not isinstance(value["error"], str):
        raise ValueError("invalid visual flags")
    if not isinstance(value["samples"], list) or len(value["samples"]) > 64:
        raise ValueError("visual ring bounds")
    for sample in value["samples"]:
        integer(sample["sequence"], 0, 0xFFFFFF)
        integer(sample["phase"], 1, 4)
        integer(sample["android_id"], 0, 31)
        integer(sample["x"], 0, 0xFFFFFF)
        integer(sample["y"], 0, 0xFFFFFF)
    return value


def read_file(path, findings, label, jsonl=False):
    if path is None or not Path(path).is_file():
        findings.missing(label + ":missing_file")
        return [], None
    path = Path(path)
    if path.stat().st_size > (MAX_FILE if jsonl else 512 * 1024):
        findings.fail(label + ":file_budget")
        return [], None
    rows, digest, offset = [], hashlib.sha256(), 0
    with path.open("rb") as stream:
        while raw := stream.readline(MAX_LINE + 1):
            begin = offset
            digest.update(raw)
            offset += len(raw)
            if len(raw) > MAX_LINE or offset > MAX_FILE or len(rows) >= MAX_RECORDS:
                findings.fail(label + ":record_budget")
                break
            if not raw.endswith(b"\n"):
                findings.missing(label + ":partial_tail")
            try:
                rows.append((_json(raw), begin))
            except (ValueError, UnicodeError, json.JSONDecodeError, RecursionError):
                (findings.fail if raw.endswith(b"\n") else findings.missing)(label + ":invalid_json")
                break
            if not jsonl:
                # Manifest may be pretty printed: read it as one bounded object.
                break
    total = path.stat().st_size
    return rows, dict(path=str(path.resolve()), bytes=offset, file_bytes=total, sha256=digest.hexdigest(),
                     sha256_scope="entire_file" if total == offset else "read_prefix")


def read_manifest(path, findings):
    if not Path(path).is_file():
        findings.missing("host:missing_manifest")
        return {}, None
    with Path(path).open("rb") as stream:
        raw = stream.read(512 * 1024 + 1)
    if len(raw) > 512 * 1024:
        findings.fail("host:manifest_budget")
        return {}, None
    try:
        value = _json(raw)
    except (ValueError, UnicodeError, json.JSONDecodeError, RecursionError):
        findings.fail("host:invalid_manifest")
        value = {}
    return value, dict(path=str(Path(path).resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def analyze_host(manifest, rows, findings):
    result = dict(rpc_effect_verified=False, terminal_status=manifest.get("status") if isinstance(manifest.get("status"), str) else None,
                  rpc_duration_ns=distribution([]), dispatch_lateness_ns=distribution([]))
    if not manifest:
        return result, []
    try:
        if manifest["schema"] != "pas.touch-probe.v1" or manifest["fixture_package"] != "org.pas.touchfixture.evidence.cpp":
            raise ValueError("unsupported host schema")
        apk_hash = manifest["fixture_apk_sha256"]
        if not isinstance(apk_hash, str) or len(apk_hash) != 64 or any(c not in "0123456789abcdefABCDEF" for c in apk_hash):
            raise ValueError("invalid claimed APK identity")
        kind = manifest["case"]
        if kind not in ("single", "capacity", "batch", "cancel", "transport-loss"):
            raise ValueError("unsupported case")
        count = integer(manifest["contacts_in_case"], 1, 10)
        capacity = integer(manifest["capacity_requested"], 1, 10)
        width, height = integer(manifest["width"], 2, 4096), integer(manifest["height"], 2, 4096)
        if count > capacity:
            findings.fail("host:capacity_claim")
        if count != (1 if kind == "single" else capacity):
            findings.fail("host:case_capacity_mismatch")
        actions = manifest["normal_actions"]
        if not isinstance(actions, list) or len(actions) != count * 2 + 1:
            raise ValueError("bounded expected actions")
        expected = []
        for action in actions:
            phase, contact = integer(action["phase"], 0, 2), integer(action["contact_id"], 0, 9)
            x, y = number(action["x"]), number(action["y"])
            if not 0 <= x < width or not 0 <= y < height:
                raise ValueError("expected coordinate")
            expected.append(dict(phase=phase, id=contact, x=x, y=y))
        downs = [a for a in expected if a["phase"] == PHASE_DOWN]
        moves = [a for a in expected if a["phase"] == PHASE_MOVE]
        ups = [a for a in expected if a["phase"] == PHASE_UP]
        if len(downs) != count or len(moves) != 1 or len(ups) != count or len({a["id"] for a in downs}) != count:
            raise ValueError("expected lifecycle")
        if {a["id"] for a in ups} != {a["id"] for a in downs} or moves[0]["id"] not in {a["id"] for a in downs}:
            raise ValueError("expected identities")
        if [a["phase"] for a in expected] != [0]*count+[1]+[2]*count:
            raise ValueError("probe stage order")
        result.update(case=kind, contacts_requested=count, capacity_declared=capacity,
                      normalized_width=width, normalized_height=height,
                      claimed_fixture_apk_sha256=manifest.get("fixture_apk_sha256"))
        if manifest.get("input_enabled") is not True:
            findings.missing("host:input_not_exercised")
            return result, expected
        start, end = integer(manifest["host_start_ns"]), integer(manifest["host_end_ns"])
        run_limit = integer(manifest["max_session_ns"], 1, 15_000_000_000)
        input_deadline = start + run_limit
        if end < start:
            findings.fail("host:session_clock")
        if manifest.get("events_log_complete") is not True:
            findings.missing("host:events_not_finalized")
        if manifest.get("status") != "fixture_visual_path_passed" or manifest.get("failure") or manifest.get("user_stop_requested"):
            findings.fail("host:failed_or_stopped")
        for key in ("down_attempted", "path_verified", "zero_active_observed", "release_rpc_known"):
            if manifest.get(key) is not True:
                findings.fail("host:" + key)
        release = manifest["release"]
        if release.get("failed") or release.get("unknown"):
            findings.fail("host:release_rpc_unknown")
        rs, rr = integer(release["host_start_ns"]), integer(release["host_return_ns"])
        if not start <= rs <= rr <= end:
            findings.fail("host:release_clock")
        result["release_rpc_duration_ns"] = rr - rs
        result["host_session_duration_ns"] = end-start
        result["input_command_deadline_ns"] = input_deadline
        baseline = visual_evidence(manifest["baseline"])
        if baseline["complete"] is not True or baseline["active"] != 0 or baseline["error"]:
            findings.fail("host:invalid_visual_baseline")
        receipts, durations, lateness, stages, previous = [], [], [], [], start
        previous_visual, final_snapshot = baseline, None
        fault_seen, last_rpc = False, None
        for row, _ in rows:
            event = row.get("event")
            if event == "rpc":
                phase, contact = integer(row["phase"], 0, 2), integer(row["id"], 0, 9)
                scheduled, sent, returned = (integer(row[k]) for k in ("host_scheduled_ns", "host_start_ns", "host_return_ns"))
                if not start <= scheduled <= sent <= returned <= end or sent < previous:
                    findings.fail("host:rpc_clock")
                if sent > input_deadline:
                    findings.fail("host:dispatch_after_run_limit")
                if last_rpc and sent < last_rpc[1] and not (kind == "batch" and (sent,returned) == last_rpc):
                    findings.fail("host:overlapping_nonbatch_rpc")
                last_rpc = sent,returned
                previous = sent
                if row.get("success") is not True or row.get("effect_verified_by_rpc") is not False:
                    findings.fail("host:rpc_result_or_effect_claim")
                receipts.append(dict(phase=phase, id=contact, x=number(row["x"]), y=number(row["y"])))
                durations.append(returned-sent)
                lateness.append(sent-scheduled)
            elif event == "fixture_snapshot":
                stamp = integer(row["host_arrival_ns"])
                if not start <= stamp <= end or stamp < previous or (last_rpc and stamp < last_rpc[1]):
                    findings.fail("host:snapshot_clock")
                previous = stamp
                evidence = visual_evidence(row["evidence"])
                if evidence["complete"] is not True or evidence["error"]:
                    findings.missing("host:visual_snapshot_incomplete")
                stage = row["stage"]
                wanted_stages = ("held", "moved", "after_release")
                if len(stages) >= 3 or stage != wanted_stages[len(stages)]:
                    findings.fail("host:visual_stage_order_or_duplicate")
                stages.append(stage)
                if evidence["active"] != (0 if stage == "after_release" else count):
                    findings.fail("host:visual_stage_active")
                for key in ("sequence", "downs", "ups", "moves", "cancels", "visual_ring_overwrites"):
                    if evidence[key] < previous_visual[key]:
                        findings.fail("host:visual_counter_regressed")
                if evidence["downs"]-baseline["downs"] != count or evidence["cancels"] != baseline["cancels"] or evidence["ups"]-baseline["ups"] != (count if stage == "after_release" else 0):
                    findings.fail("host:visual_stage_counts")
                previous_visual = evidence
                if stage == "after_release":
                    final_snapshot = evidence
            elif event == "simulated_client_transport_loss":
                fault_seen = row.get("rpc_success") is False
                if not fault_seen:
                    findings.fail("host:simulated_loss_not_observed")
            elif event == "faulted_channel_release":
                if not row.get("report", {}).get("unknown"):
                    findings.fail("host:fault_release_missing_unknown")
            else:
                findings.fail("host:unknown_event")
        wanted = expected if kind not in ("cancel", "transport-loss") else [a for a in expected if a["phase"] != PHASE_UP]
        if receipts != wanted:
            findings.fail("host:commands_do_not_match_plan")
        if stages != ["held", "moved", "after_release"]:
            findings.missing("host:missing_visual_stage")
        if kind == "transport-loss" and (not fault_seen or manifest.get("transport_fault_observed") is not True):
            findings.fail("host:transport_loss_case_unverified")
        result.update(rpc_commands=len(receipts), rpc_duration_ns=distribution(durations),
                      dispatch_lateness_ns=distribution(lateness),
                      intentional_loss_scope="client channel switched to an invalid target" if kind == "transport-loss" else None)
        final = visual_evidence(manifest["final_evidence"])
        if final.get("complete") is not True or final.get("active") != 0:
            findings.fail("host:final_visual_not_zero")
        if final_snapshot is not None and final_snapshot != final:
            findings.fail("host:terminal_snapshot_manifest_mismatch")
        result["visual_stages"] = stages
    except (KeyError, TypeError, ValueError, OverflowError):
        findings.fail("host:invalid_schema_or_value")
        return result, []
    return result, expected


def analyze_android(rows, source, host, expected, findings, tolerance):
    result = dict(stream_complete=False, expected_path_verified=False, final_zero_observed=False,
                  historical_samples_expanded=False, clock_mapping_to_host=None,
                  pointer_lifecycle_valid=False,
                  pointer_lifecycle_scope="complete recorder stream only; false when reconstruction is incomplete")
    if not rows:
        return result
    own = Findings()
    try:
        header, _ = rows[0]
        if header.get("type") != "header" or header.get("schema") != "pas.android.touch-evidence.v1" or header.get("fixture") != "native-evidence-v3":
            raise ValueError("unsupported Android header")
        start = integer(header["start_monotonic_ns"])
        if integer(header["max_events"], 1, 8192) != 8192 or integer(header["max_bytes"], 1, MAX_FILE) != 16*1024*1024 or integer(header["max_duration_ns"], 1) != 60000000000:
            raise ValueError("unsupported recorder limits")
        if source and source["file_bytes"] > header["max_bytes"]:
            own.fail("android:declared_byte_budget")
        verify_path = bool(expected)
        if not verify_path:
            own.missing("android:missing_expected_plan")
        active, mapping, completed = {}, {}, set()
        motions, history, peak, last_event, last_arrival, gesture_down = 0, 0, 0, -1, start, None
        intervals, arrivals, callback_ages = [], [], []
        downs = {a["id"]: a for a in expected if a["phase"] == PHASE_DOWN}
        move = next((a for a in expected if a["phase"] == PHASE_MOVE), None)
        ups = {a["id"]: a for a in expected if a["phase"] == PHASE_UP}
        targets_seen, move_seen, progress, max_stationary, saw_down = set(), False, 0., 0., False
        footer, last_surface, devices = None, None, set()
        for position, (row, offset) in enumerate(rows[1:], 1):
            if row.get("type") == "footer":
                if footer is not None or position != len(rows)-1:
                    own.fail("android:footer_not_terminal")
                footer = row
                if integer(row["events_written"]) != motions or integer(row["bytes_before_footer"]) != offset:
                    own.fail("android:footer_count_or_bytes")
                if integer(row["stop_monotonic_ns"]) < last_arrival:
                    own.fail("android:footer_clock")
                if row.get("fault") is not False:
                    own.missing("android:recorder_fault")
                if integer(row["lifecycle_errors"]) != 0:
                    own.fail("android:footer_lifecycle_error")
                if integer(row["first_unrecorded_events"]) or row.get("stop_reason") in ("event_limit", "byte_limit"):
                    own.missing("android:recorder_limit")
                if row.get("further_events_not_observed") is not True:
                    own.fail("android:invalid_footer_scope")
                if integer(row["active_after_last_observation"], 0, 32) != len(active):
                    own.fail("android:footer_active_mismatch")
                continue
            if footer is not None or row.get("type") != "motion":
                own.fail("android:unexpected_record")
                continue
            motions += 1
            if motions > header["max_events"]:
                own.fail("android:declared_event_budget")
            if integer(row["sequence"], 1) != motions:
                own.fail("android:sequence_gap_or_duplicate")
            arrival, event, down = (integer(row[k]) for k in ("arrival_monotonic_ns", "android_event_ns", "android_down_ns"))
            if arrival < last_arrival or event < last_event or down > event or event > arrival or arrival-start >= header["max_duration_ns"]:
                own.fail("android:timestamp_anomaly")
            if last_event >= 0:
                intervals.append(event-last_event)
                arrivals.append(arrival-last_arrival)
            callback_ages.append(arrival-event)
            last_event, last_arrival = event, arrival
            history += integer(row["history_size"], 0, 8192)
            width, height = integer(row["surface_width"], 1, 16384), integer(row["surface_height"], 1, 16384)
            if last_surface and last_surface != (width, height):
                own.fail("android:surface_changed")
            last_surface = width, height
            if host.get("normalized_width") is not None and (width, height) != (host.get("normalized_width"), host.get("normalized_height")):
                own.missing("android:unmapped_surface_geometry")
                verify_path = False
            devices.add((integer(row["device_id"], -(1 << 31), (1 << 31)-1), integer(row["source"], 0, (1 << 31)-1)))
            raw, action, index = integer(row["action_raw"], 0, 65535), integer(row["action"], 0, 255), integer(row["action_index"], 0, 31)
            if action != raw & 255 or index != (raw >> 8) & 255:
                own.fail("android:action_encoding")
            pointers = row["pointers"]
            if not isinstance(pointers, list) or not 1 <= len(pointers) <= 32 or index >= len(pointers):
                raise ValueError("invalid pointer array")
            present = {}
            for pointer in pointers:
                pid = integer(pointer["id"], 0, 31)
                x, y, pressure = number(pointer["x"]), number(pointer["y"]), number(pointer["pressure"])
                integer(pointer["tool_type"], 0, 255)
                if pid in present or not 0 <= x < width or not 0 <= y < height or pressure < 0:
                    own.fail("android:invalid_pointer")
                present[pid] = dict(x=x, y=y)
            changed = integer(pointers[index]["id"], 0, 31)
            before = set(active)
            if action in (0, 5):
                saw_down = True
                if (action == 0 and (before or len(present) != 1)) or (action == 5 and (not before or changed in before)) or set(present) != before | {changed}:
                    own.fail("android:illegal_down_snapshot")
                if action == 0:
                    gesture_down = down
                candidates = [logical for logical, target in downs.items() if logical not in targets_seen and
                              math.hypot(present[changed]["x"]-target["x"], present[changed]["y"]-target["y"]) <= tolerance]
                if verify_path and (len(candidates) != 1 or changed in mapping):
                    own.fail("android:ambiguous_or_unexpected_down")
                elif verify_path:
                    mapping[changed] = candidates[0]
                    targets_seen.add(candidates[0])
                active = present.copy()
            elif action in (1, 6):
                if changed not in before or set(present) != before or (action == 1 and len(before) != 1) or (action == 6 and len(before) <= 1):
                    own.fail("android:illegal_up_snapshot")
                logical = mapping.get(changed)
                if verify_path and (logical not in ups or logical in completed or not move_seen or len(targets_seen) != len(downs)):
                    own.fail("android:unexpected_or_premature_up")
                elif verify_path and host.get("case") != "transport-loss" and math.hypot(present[changed]["x"]-ups[logical]["x"], present[changed]["y"]-ups[logical]["y"]) > tolerance:
                    own.fail("android:up_coordinate")
                completed.add(logical)
                active = {pid: point for pid, point in present.items() if pid != changed}
            elif action == 2:
                if not before or set(present) != before:
                    own.fail("android:illegal_move_snapshot")
                active = present.copy()
            elif action == 3:
                if set(present) != before:
                    own.fail("android:illegal_cancel_snapshot")
                own.fail("android:unexpected_cancel")
                active = {}
            else:
                own.fail("android:unsupported_action")
            if gesture_down is None or down != gesture_down:
                own.fail("android:gesture_down_time_changed")
            if not active:
                gesture_down = None
            recorded_active = row["active_after"]
            if not isinstance(recorded_active, list) or len(recorded_active) > 32:
                raise ValueError("invalid active ID list")
            recorded_ids = [integer(pid, 0, 31) for pid in recorded_active]
            if len(set(recorded_ids)) != len(recorded_ids) or set(recorded_ids) != set(active):
                own.fail("android:active_after_mismatch")
            if integer(row["lifecycle_errors"]) != 0:
                own.fail("android:recorded_lifecycle_error")
            peak = max(peak, len(active))
            if len(active) > host.get("capacity_declared", 32):
                own.fail("android:capacity_exceeded")
            for pid, point in present.items():
                logical = mapping.get(pid)
                if logical not in downs or not move:
                    continue
                # Recovery UP is intentionally sent at center; it is release
                # evidence, not a calibrated endpoint for that changed pointer.
                if host.get("case") == "transport-loss" and action in (1, 6) and pid == changed:
                    continue
                anchor = downs[logical]
                if logical != move["id"]:
                    drift = math.hypot(point["x"]-anchor["x"], point["y"]-anchor["y"])
                    max_stationary = max(max_stationary, drift)
                    if drift > tolerance:
                        own.fail("android:stationary_pointer_drift")
                else:
                    dx, dy = move["x"]-anchor["x"], move["y"]-anchor["y"]
                    length = math.hypot(dx, dy)
                    if length == 0:
                        own.fail("host:zero_length_move")
                        continue
                    px, py = point["x"]-anchor["x"], point["y"]-anchor["y"]
                    current = (px*dx+py*dy)/(length*length)
                    if verify_path and current > progress+tolerance/length and len(present) != len(downs):
                        own.fail("android:move_before_full_capacity")
                    if action in (0, 5) and math.hypot(px, py) > tolerance:
                        own.fail("android:primary_moved_during_down_stage")
                    if abs(px*dy-py*dx)/length > tolerance or current < progress-tolerance/length or current > 1+tolerance/length:
                        own.fail("android:off_path_or_reversed_move")
                    progress = max(progress, current)
                    if action == 2 and len(active) == len(downs) and math.hypot(point["x"]-move["x"], point["y"]-move["y"]) <= tolerance:
                        move_seen = True
        if footer is None:
            own.missing("android:missing_footer")
        else:
            if footer.get("stop_reason") not in ("app_pause", "app_destroy", "duration_limit", "event_limit", "byte_limit", "clock_fault", "invalid_event", "invalid_pointer"):
                own.fail("android:unknown_stop_reason")
            if footer.get("stop_reason") in ("clock_fault", "invalid_event", "invalid_pointer"):
                own.missing("android:terminal_recorder_fault")
                if footer.get("fault") is not True or (footer["stop_reason"] != "clock_fault" and footer["first_unrecorded_events"] < 1):
                    own.fail("android:terminal_fault_flag_mismatch")
            if footer.get("stop_reason") in ("event_limit", "byte_limit") and footer["first_unrecorded_events"] < 1:
                own.fail("android:limit_missing_first_unrecorded")
            if active or footer.get("release_observation") != "zero_active_in_observed_stream":
                (own.missing if own.incomplete else own.fail)("android:final_release_not_zero")
            release_scope = ("unknown_due_incomplete_evidence" if footer.get("fault") or footer["first_unrecorded_events"] or footer["lifecycle_errors"] else
                             "no_pointer_observations" if not saw_down else
                             "active_at_close" if active else "zero_active_in_observed_stream")
            if footer.get("release_observation") != release_scope:
                own.fail("android:release_scope_mismatch")
            if verify_path and (not move_seen or completed != set(downs) or peak != host.get("contacts_requested")):
                own.fail("android:expected_path_missing")
        if len(devices) != 1:
            own.fail("android:mixed_input_sources")
        result.update(motion_events=motions, peak_contacts=peak, logical_to_android_id={str(logical): pid for pid, logical in mapping.items()},
                      stationary_max_drift_pixels=max_stationary, expected_move_target_observed=move_seen,
                      history_samples_counted=history, all_historical_motion_samples_available=history == 0,
                      release_coordinates_checked=host.get("case") != "transport-loss",
                      terminal_reason=footer.get("stop_reason") if footer else None,
                      final_zero_observed=bool(footer and saw_down and not active and footer.get("release_observation") == "zero_active_in_observed_stream"),
                      event_interval_ns=distribution(intervals), arrival_interval_ns=distribution(arrivals),
                      callback_age_in_android_clock_ns=distribution(callback_ages),
                      release_scope="zero at the last observation in this app stream; later events unobserved")
    except (KeyError, TypeError, ValueError, OverflowError):
        own.fail("android:invalid_schema_or_value")
    effect_codes = {"android:ambiguous_or_unexpected_down", "android:unexpected_or_premature_up",
                    "android:up_coordinate", "android:unexpected_cancel", "android:capacity_exceeded",
                    "android:stationary_pointer_drift", "android:off_path_or_reversed_move",
                    "android:move_before_full_capacity",
                    "android:primary_moved_during_down_stage",
                    "android:final_release_not_zero", "android:expected_path_missing", "host:zero_length_move",
                    "android:unmapped_surface_geometry", "android:missing_expected_plan"}
    inherited = [c for c in findings.failed + findings.incomplete if c.startswith("android:")]
    result["stream_complete"] = not inherited and not any(c not in effect_codes for c in own.failed + own.incomplete)
    result["expected_path_verified"] = result["stream_complete"] and not own.failed and not own.incomplete and bool(expected)
    result["pointer_lifecycle_valid"] = result["stream_complete"] and not any("snapshot" in c or "lifecycle" in c or "active_" in c or "invalid_pointer" in c or "unsupported_action" in c for c in own.failed)
    result["pointer_lifecycle_scope"] = "complete recorder stream only; false when schema, sequence, timing or finalization prevents reconstruction"
    result["final_zero_observed"] = result["final_zero_observed"] and result["pointer_lifecycle_valid"]
    for code in own.failed:
        findings.fail(code)
    for code in own.incomplete:
        findings.missing(code)
    return result


def analyze_probe(manifest_path, events_path, android_path=None, *, tolerance_pixels=3.):
    if not math.isfinite(tolerance_pixels) or not 0 <= tolerance_pixels <= 20:
        raise ValueError("tolerance must be finite and bounded 0..20 pixels")
    findings = Findings()
    manifest, manifest_source = read_manifest(manifest_path, findings)
    events, events_source = read_file(events_path, findings, "host", True)
    host, expected = analyze_host(manifest, events, findings)
    native, native_source = read_file(android_path, findings, "android", True)
    android = analyze_android(native, native_source, host, expected, findings, tolerance_pixels)
    return dict(schema="pas.touch-probe-analysis.v1",
                status="failed" if findings.failed else "incomplete" if findings.incomplete else "passed_observed_fixture_path",
                failures=findings.failed, incomplete_reasons=findings.incomplete,
                sources=dict(manifest=manifest_source, host_events=events_source, android_events=native_source),
                tolerance_pixels=tolerance_pixels, host=host, android=android,
                host_to_android_clock_mapping=None, input_latency_ns=None, game_input_lead_ns=None,
                association="one-to-one DOWN coordinate mapping and ordered path, not sequence-number or cross-clock matching",
                device_identity_authenticated=False, live_game_qualified=False,
                limitations=["RPC return does not verify effect; Android IDs are learned from DOWN coordinates.",
                             "Host QPC and Android time domains are not mapped; no cross-domain subtraction is performed.",
                             "MotionEvent sequence, per-pointer visual sequence and per-command RPC logs are different streams.",
                             "Only captured event snapshots are checked; batched historical samples are counted, not expanded.",
                             "Zero refers to the final app observation; APK/session association is supplied, not authenticated here."])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--events", type=Path, required=True)
    parser.add_argument("--android", type=Path)
    parser.add_argument("--tolerance-pixels", type=float, default=3.)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        if args.output and args.output.exists():
            raise ValueError("refuse to overwrite output")
        result = analyze_probe(args.manifest, args.events, args.android, tolerance_pixels=args.tolerance_pixels)
        text = json.dumps(result, indent=2, allow_nan=False) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8") as stream:
                stream.write(text)
        else:
            print(text, end="")
        return {"failed": 3, "incomplete": 2}.get(result["status"], 0)
    except (OSError, ValueError):
        parser.exit(1, "analysis failed: unreadable input, invalid option, or existing output\n")


if __name__ == "__main__":
    raise SystemExit(main())
