import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "tools/analyze_touch_probe.py"
SPEC = importlib.util.spec_from_file_location("analyze_touch_probe", TOOL)
analyzer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analyzer)


def fixture():
    start = 1_000_000_000_000
    actions = [dict(phase=0, contact_id=0, x=426, y=540), dict(phase=0, contact_id=1, x=853, y=540),
               dict(phase=1, contact_id=0, x=426, y=480), dict(phase=2, contact_id=0, x=426, y=480),
               dict(phase=2, contact_id=1, x=853, y=540)]
    visual = dict(complete=True, active=0, downs=2, ups=2, moves=4, cancels=0, sequence=8,
                  samples=[], error="", visual_ring_overwrites=0)
    manifest = dict(schema="pas.touch-probe.v1", fixture_package="org.pas.touchfixture.evidence.cpp", case="capacity",
                    contacts_in_case=2, capacity_requested=2, width=1280, height=720, normal_actions=actions,
                    input_enabled=True, events_log_complete=True, fixture_apk_sha256="a"*64,
                    host_start_ns=start, host_end_ns=start+1_000_000_000, max_session_ns=15_000_000_000,
                    status="fixture_visual_path_passed", failure="", user_stop_requested=False,
                    down_attempted=True, path_verified=True, zero_active_observed=True, release_rpc_known=True,
                    release=dict(requested=[], failed=[], unknown=[], host_start_ns=start+203_000_000,
                                 host_return_ns=start+203_100_000, effect_verified_by_rpc=False),
                    baseline=dict(complete=True, active=0, downs=0, ups=0, moves=0, cancels=0,
                                  sequence=0, error="", samples=[], visual_ring_overwrites=0),
                    final_evidence=visual)
    events = []
    times = [1, 2, 101, 201, 202]
    for index, action in enumerate(actions):
        stamp = start + times[index]*1_000_000
        events.append(dict(event="rpc", phase=action["phase"], id=action["contact_id"], x=action["x"], y=action["y"],
                           host_scheduled_ns=stamp-1000, host_start_ns=stamp, host_return_ns=stamp+100_000,
                           success=True, reason="rpc_returned_effect_unverified", effect_verified_by_rpc=False))
        if index in (1, 2, 4):
            stage, delta, active = {1:("held",100,2), 2:("moved",200,2), 4:("after_release",300,0)}[index]
            evidence = dict(visual)
            evidence.update(active=active, ups=2 if stage == "after_release" else 0,
                            moves=0 if stage == "held" else 4, sequence={"held":2,"moved":6,"after_release":8}[stage])
            events.append(dict(event="fixture_snapshot", stage=stage, host_arrival_ns=start+delta*1_000_000,
                               source_timestamp_us=1_700_000_000_000_000+delta,
                               evidence=evidence))
    header = dict(type="header", schema="pas.android.touch-evidence.v1", fixture="native-evidence-v3",
                  start_monotonic_ns=99_000_000_000, max_events=8192, max_bytes=16*1024*1024,
                  max_duration_ns=60_000_000_000, host_clock_mapping="unknown", history_samples="count only; not expanded")

    def point(pid, x, y):
        return dict(id=pid, x=x, y=y, pressure=1., tool_type=1)
    snapshots = [(0,0,[point(7,426,540)],[7]), (5,1,[point(7,426,540),point(2,853,540)],[2,7]),
                 (2,0,[point(7,426,510),point(2,853,540)],[2,7]),
                 (2,0,[point(7,426,480),point(2,853,540)],[2,7]),
                 (6,0,[point(7,426,480),point(2,853,540)],[2]), (1,0,[point(2,853,540)],[])]
    motions = []
    for index, (action, changed, pointers, active) in enumerate(snapshots):
        stamp = 100_000_000_000 + index*10_000_000
        motions.append(dict(type="motion", sequence=index+1, arrival_monotonic_ns=stamp+1_000_000,
                            android_event_ns=stamp, android_down_ns=100_000_000_000,
                            action=action, action_index=changed, action_raw=action|(changed<<8),
                            source=0x1002, device_id=6, surface_width=1280, surface_height=720,
                            history_size=0, pointers=pointers, active_after=active, lifecycle_errors=0))
    footer = dict(type="footer", stop_reason="app_pause", stop_monotonic_ns=101_000_000_000,
                  events_written=len(motions), bytes_before_footer=0, first_unrecorded_events=0,
                  further_events_not_observed=True, lifecycle_errors=0, fault=False,
                  active_after_last_observation=0, release_observation="zero_active_in_observed_stream")
    return manifest, events, [header]+motions+[footer]


class ProbeAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.manifest, self.events, self.native = fixture()

    def tearDown(self):
        self.temp.cleanup()

    def save(self, native=True):
        manifest, events, android = self.root/"manifest.json", self.root/"events.jsonl", self.root/"android.jsonl"
        manifest.write_text(json.dumps(self.manifest, indent=2), encoding="utf-8")
        events.write_text("".join(json.dumps(row)+"\n" for row in self.events), encoding="utf-8")
        if native:
            content = b""
            for row in self.native:
                if row.get("type") == "footer":
                    row["bytes_before_footer"] = len(content)
                content += (json.dumps(row)+"\n").encode("utf-8")
            android.write_bytes(content)
        return manifest, events, android

    def analyze(self, native=True):
        return analyzer.analyze_probe(*self.save(native), tolerance_pixels=3)

    def test_complete_path_learns_android_ids_and_keeps_clock_domains_separate(self):
        report = self.analyze()
        self.assertEqual(report["status"], "passed_observed_fixture_path")
        self.assertEqual(report["android"]["logical_to_android_id"], {"0":7,"1":2})
        self.assertTrue(report["android"]["stream_complete"])
        self.assertTrue(report["android"]["expected_path_verified"])
        self.assertEqual(report["android"]["peak_contacts"], 2)
        self.assertEqual(report["host"]["rpc_duration_ns"]["median"], 100000)
        self.assertEqual(report["android"]["callback_age_in_android_clock_ns"]["median"], 1000000)
        self.assertIsNone(report["input_latency_ns"])
        self.assertIsNone(report["host_to_android_clock_mapping"])
        self.assertFalse(report["live_game_qualified"])

    def test_rpc_and_visual_success_without_device_stream_is_incomplete(self):
        report = self.analyze(False)
        self.assertEqual(report["status"], "incomplete")
        self.assertIn("android:missing_file", report["incomplete_reasons"])
        self.assertFalse(report["android"]["expected_path_verified"])

    def test_missing_footer_and_partial_tail_never_establish_release(self):
        self.native.pop()
        report = self.analyze()
        self.assertEqual(report["status"], "incomplete")
        self.assertFalse(report["android"]["final_zero_observed"])
        paths = self.save()
        with paths[2].open("ab") as stream:
            stream.write(b'{"type":"footer"')
        report = analyzer.analyze_probe(*paths)
        self.assertEqual(report["status"], "incomplete")
        self.assertFalse(report["android"]["stream_complete"])

    def test_duplicate_and_gap_sequences_fail_independently_of_footer_count(self):
        for value in (1, 9):
            with self.subTest(value=value):
                self.native = fixture()[2]
                self.native[2]["sequence"] = value
                report = self.analyze()
                self.assertIn("android:sequence_gap_or_duplicate", report["failures"])
                self.assertFalse(report["android"]["pointer_lifecycle_valid"])
                self.assertFalse(report["android"]["final_zero_observed"])

    def test_complete_pointer_snapshot_cannot_replace_existing_active_id(self):
        self.native[2]["pointers"][0]["id"] = 15
        report = self.analyze()
        self.assertEqual(report["status"], "failed")
        self.assertIn("android:illegal_down_snapshot", report["failures"])
        self.assertFalse(report["android"]["pointer_lifecycle_valid"])

    def test_duplicate_pointer_ids_and_mismatched_action_index_fail(self):
        self.native[2]["pointers"][1]["id"] = 7
        self.assertIn("android:invalid_pointer", self.analyze()["failures"])
        self.native = fixture()[2]
        self.native[2]["action_raw"] = 5
        self.assertIn("android:action_encoding", self.analyze()["failures"])

    def test_active_after_requires_integer_ids_not_float_or_boolean_aliases(self):
        for value in (7., True):
            with self.subTest(value=value):
                self.native = fixture()[2]
                if value is True:
                    # Use valid Android ID1 so set([True]) would otherwise equal {1}.
                    for motion in self.native[1:-1]:
                        for pointer in motion["pointers"]:
                            if pointer["id"] == 7:
                                pointer["id"] = 1
                        motion["active_after"] = [1 if pid == 7 else pid for pid in motion["active_after"]]
                self.native[1]["active_after"] = [value]
                report = self.analyze()
                self.assertIn("android:invalid_schema_or_value", report["failures"])
                self.assertFalse(report["android"]["pointer_lifecycle_valid"])

    def test_incomplete_or_invalid_schema_cannot_claim_valid_pointer_lifecycle(self):
        self.native.pop()
        self.assertFalse(self.analyze()["android"]["pointer_lifecycle_valid"])
        self.native = fixture()[2]
        del self.native[2]["pointers"]
        report = self.analyze()
        self.assertIn("android:invalid_schema_or_value", report["failures"])
        self.assertFalse(report["android"]["pointer_lifecycle_valid"])
        self.assertFalse(report["android"]["final_zero_observed"])

    def test_footer_byte_count_and_no_pointer_zero_claim_are_checked(self):
        paths = self.save()
        raw = paths[2].read_bytes()
        value = self.native[-1]["bytes_before_footer"]
        paths[2].write_bytes(raw.replace(f'"bytes_before_footer": {value}'.encode(),b'"bytes_before_footer": 0'))
        report = analyzer.analyze_probe(*paths)
        self.assertIn("android:footer_count_or_bytes", report["failures"])
        self.assertFalse(report["android"]["final_zero_observed"])
        self.native = [fixture()[2][0], fixture()[2][-1]]
        self.native[-1]["events_written"] = 0
        report = self.analyze()
        self.assertIn("android:release_scope_mismatch", report["failures"])
        self.assertFalse(report["android"]["final_zero_observed"])

    def test_missing_up_cannot_be_erased_by_claimed_zero_footer(self):
        self.native.pop(-2)
        self.native[-1]["events_written"] -= 1
        report = self.analyze()
        self.assertIn("android:footer_active_mismatch", report["failures"])
        self.assertIn("android:final_release_not_zero", report["failures"])
        self.assertFalse(report["android"]["final_zero_observed"])

    def test_other_finger_drift_and_off_path_move_are_detected(self):
        self.native[3]["pointers"][1]["x"] += 10
        self.assertIn("android:stationary_pointer_drift", self.analyze()["failures"])
        self.native = fixture()[2]
        self.native[3]["pointers"][0]["x"] += 10
        self.assertIn("android:off_path_or_reversed_move", self.analyze()["failures"])

    def test_primary_path_is_checked_in_pointer_down_and_pointer_up_snapshots(self):
        self.native[2]["pointers"][0]["x"] += 50
        self.assertIn("android:off_path_or_reversed_move", self.analyze()["failures"])
        self.native = fixture()[2]
        # Release stationary Android ID2 first, while the primary ID7 remains.
        self.native[-3].update(action_index=1, action_raw=6|(1<<8), active_after=[7])
        self.native[-3]["pointers"][0]["x"] += 50
        self.native[-2]["pointers"] = [dict(id=7,x=426,y=480,pressure=1,tool_type=1)]
        self.assertIn("android:off_path_or_reversed_move", self.analyze()["failures"])
        self.native = fixture()[2]
        self.native[2]["pointers"][0]["y"] -= 30
        self.assertIn("android:primary_moved_during_down_stage", self.analyze()["failures"])

    def test_history_is_counted_without_claiming_expanded_samples(self):
        self.native[3]["history_size"] = 5
        report = self.analyze()
        self.assertEqual(report["status"], "passed_observed_fixture_path")
        self.assertEqual(report["android"]["history_samples_counted"], 5)
        self.assertFalse(report["android"]["all_historical_motion_samples_available"])
        self.assertFalse(report["android"]["historical_samples_expanded"])

    def test_recorder_limit_and_fault_are_incomplete_not_known_release(self):
        self.native[-1].update(stop_reason="event_limit", first_unrecorded_events=1,
                               release_observation="unknown_due_incomplete_evidence")
        report = self.analyze()
        self.assertEqual(report["status"], "incomplete")
        self.assertFalse(report["android"]["expected_path_verified"])
        self.assertFalse(report["android"]["final_zero_observed"])

    def test_terminal_fault_reason_cannot_be_laundered_by_false_fault_flag(self):
        for reason in ("clock_fault", "invalid_event", "invalid_pointer"):
            with self.subTest(reason=reason):
                self.native = fixture()[2]
                self.native[-1]["stop_reason"] = reason
                report = self.analyze()
                self.assertIn("android:terminal_fault_flag_mismatch", report["failures"])
                self.assertFalse(report["android"]["pointer_lifecycle_valid"])
                self.native[-1].update(fault=True, first_unrecorded_events=0 if reason == "clock_fault" else 1,
                                       release_observation="unknown_due_incomplete_evidence")
                report = self.analyze()
                self.assertEqual(report["status"], "incomplete")
                self.assertFalse(report["android"]["final_zero_observed"])

    def test_android_timestamp_anomaly_is_not_clamped_or_cross_subtracted(self):
        self.native[3]["android_event_ns"] = self.native[3]["arrival_monotonic_ns"] + 10
        report = self.analyze()
        self.assertIn("android:timestamp_anomaly", report["failures"])
        self.assertLess(report["android"]["callback_age_in_android_clock_ns"]["minimum"], 0)
        self.assertIsNone(report["input_latency_ns"])

    def test_gesture_down_time_is_not_redefined_for_pointer_down(self):
        self.native[2]["android_down_ns"] = self.native[2]["android_event_ns"]
        self.assertIn("android:gesture_down_time_changed", self.analyze()["failures"])

    def test_host_rpc_plan_failure_and_log_tail_are_checked(self):
        self.events[0]["x"] += 1
        self.assertIn("host:commands_do_not_match_plan", self.analyze()["failures"])
        self.events = fixture()[1]
        self.events.pop()
        report = self.analyze()
        self.assertIn("host:missing_visual_stage", report["incomplete_reasons"])

    def test_host_terminal_snapshot_and_manifest_cannot_contradict(self):
        self.events[-1]["evidence"]["active"] = 1
        report = self.analyze()
        self.assertIn("host:visual_stage_active", report["failures"])
        self.assertIn("host:terminal_snapshot_manifest_mismatch", report["failures"])
        self.events = fixture()[1]
        self.events.insert(3, copy.deepcopy(self.events[2]))
        self.assertIn("host:visual_stage_order_or_duplicate", self.analyze()["failures"])

    def test_host_clock_order_and_input_deadline_are_checked(self):
        self.events[1]["host_start_ns"] = self.events[0]["host_start_ns"]
        self.events[1]["host_scheduled_ns"] = self.events[0]["host_scheduled_ns"]
        self.events[1]["host_return_ns"] = self.events[0]["host_return_ns"]
        self.assertIn("host:overlapping_nonbatch_rpc", self.analyze()["failures"])
        self.manifest["case"] = "batch"
        self.assertEqual(self.analyze()["status"], "passed_observed_fixture_path")
        self.events = fixture()[1]
        self.manifest["max_session_ns"] = 1_000_000
        self.assertIn("host:dispatch_after_run_limit", self.analyze()["failures"])

    def test_unknown_surface_mapping_does_not_invent_coordinate_conversion(self):
        for motion in self.native[1:-1]:
            motion["surface_width"] = 1279
        report = self.analyze()
        self.assertEqual(report["status"], "incomplete")
        self.assertTrue(report["android"]["pointer_lifecycle_valid"])
        self.assertFalse(report["android"]["expected_path_verified"])
        self.assertIn("android:unmapped_surface_geometry", report["incomplete_reasons"])

    def test_transport_loss_center_release_is_separate_from_path_calibration(self):
        self.manifest.update(case="transport-loss", transport_fault_observed=True)
        self.events = [r for r in self.events if not (r["event"] == "rpc" and r["phase"] == 2)]
        self.events.insert(-1, dict(event="simulated_client_transport_loss", rpc_success=False,
                                    host_start_ns=1_000_202_000_000, host_return_ns=1_000_202_100_000))
        self.events.insert(-1, dict(event="faulted_channel_release", report=dict(unknown=[0,1], failed=[0,1])))
        self.native[-3]["pointers"][0].update(x=640,y=360)
        self.native[-2]["pointers"][0].update(x=640,y=360)
        report = self.analyze()
        self.assertEqual(report["status"], "passed_observed_fixture_path")
        self.assertFalse(report["android"]["release_coordinates_checked"])

    def test_batch_android_pointer_down_order_can_differ_from_logical_ids(self):
        self.manifest["case"] = "batch"
        # Native callback first sees logical1/Android2, then logical0/Android7.
        self.native[1]["pointers"] = [dict(id=2,x=853,y=540,pressure=1,tool_type=1)]
        self.native[1]["active_after"] = [2]
        self.native[2]["pointers"].reverse()
        report = self.analyze()
        self.assertEqual(report["status"], "passed_observed_fixture_path")
        self.assertEqual(report["android"]["logical_to_android_id"], {"1":2,"0":7})

    def test_cli_writes_new_report_and_duplicate_json_keys_fail(self):
        manifest, events, native = self.save()
        output = self.root/"new-report.json"
        result = subprocess.run([sys.executable,str(TOOL),"--manifest",str(manifest),"--events",str(events),
                                 "--android",str(native),"--output",str(output)],capture_output=True,text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(output.read_text())["status"], "passed_observed_fixture_path")
        with native.open("wb") as stream:
            stream.write(b'{"type":"header","type":"footer"}\n')
        self.assertIn("android:invalid_json", analyzer.analyze_probe(manifest,events,native)["failures"])

    def test_output_refuses_overwrite_and_resource_limits_are_bounded(self):
        manifest, events, native = self.save()
        output = self.root/"existing.json"
        output.write_text("keep", encoding="utf-8")
        result = subprocess.run([sys.executable,str(TOOL),"--manifest",str(manifest),"--events",str(events),
                                 "--android",str(native),"--output",str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(output.read_text(), "keep")
        with self.assertRaises(ValueError):
            analyzer.analyze_probe(manifest, events, native, tolerance_pixels=float("nan"))
        native.write_bytes(b"x"*(analyzer.MAX_LINE+1)+b"\n")
        self.assertIn("android:record_budget", analyzer.analyze_probe(manifest,events,native)["failures"])


if __name__ == "__main__":
    unittest.main()
