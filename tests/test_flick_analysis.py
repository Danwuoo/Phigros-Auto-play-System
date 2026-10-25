import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


TOOL = Path(__file__).resolve().parents[1] / "tools/analyze_flick_misses.py"
SPEC = importlib.util.spec_from_file_location("analyze_flick_misses", TOOL)
analyzer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analyzer)
MS = 1_000_000
CHART = "a" * 64
PLAN = "b" * 64
SOURCE = "c" * 64


def fixture():
    def step(time, phase, x, y):
        return dict(song_ns=time * MS, phase=phase, point=[x / 1279, y / 719])

    def owner(contact, pointer, kind, time):
        return dict(contact_id=contact, source_note_id=CHART + "#" + pointer,
                    json_pointer=pointer, judgment_ns=time * MS, kind=kind,
                    projection_min_reference_pixels=0.0, projection_max_reference_pixels=0.0,
                    normal_projection_spans=[])

    # Deliberately different source-contact order and runtime birth order.
    contacts = [dict(id=3, steps=[step(970, 0, 640, 320), step(1000, 1, 640, 350),
                                 step(1030, 1, 640, 380), step(1054, 2, 640, 380)]),
                dict(id=9, steps=[step(900, 0, 800, 350), step(950, 1, 800, 350),
                                 step(1200, 2, 800, 350)])]
    plan = dict(schema="pas.experimental-whole-song-compiled.v1", cache_sha256=PLAN,
                compiler_version="phigros-contact-v2", chart_sha256=CHART,
                input_geometry=[1280, 720, 0], contacts=contacts,
                ownership=[owner(3, "/judgeLineList/0/notesAbove/1", 4, 1000),
                           owner(9, "/judgeLineList/1/notesBelow/0", 3, 900)],
                total_steps=7, peak_contacts=2, flick_lead_ns=30 * MS,
                flick_travel_ns=60 * MS, flick_release_ns=24 * MS,
                flick_distance_reference_pixels=60.0)
    manifest = dict(schema="pas.experimental-whole-song.v1", song_id="synthetic.song.0", difficulty="AT",
                    chart_sha256=CHART, expected_plan_sha256=PLAN, full_chart_notes=2,
                    full_compiled_steps=7, experimental_phase_ms=-80,
                    anchor=dict(origin_host_ns=10_000 * MS), receipt_rows=7,
                    source_sha256=SOURCE, status="complete", failure=None,
                    whole_song_sequence_complete=True, kernel_final_zero=True)
    for key in ("flick_lead_ns", "flick_travel_ns", "flick_release_ns", "flick_distance_reference_pixels"):
        manifest["experimental_" + key] = plan[key]
    receipts = []
    for contact in contacts:
        intent, pointer = (2, 1) if contact["id"] == 3 else (1, 0)
        for point in contact["steps"]:
            due = 9920 * MS + point["song_ns"]
            receipts.append(dict(event="rpc", phase=point["phase"], id=pointer,
                                 intent_id=intent, scheduled_ns=due,
                                 x=round(point["point"][0] * 1279), y=round(point["point"][1] * 719),
                                 host_start_ns=due + MS // 2, host_return_ns=due + MS // 2 + 100_000,
                                 success=True, reason="rpc_returned_effect_unverified",
                                 effect_verified_by_rpc=False, source_frame_sequence=1))
    receipts.sort(key=lambda row: (row["host_start_ns"], analyzer.PHASE_ORDER[row["phase"]], row["intent_id"]))
    return manifest, plan, receipts


class FlickAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run = self.root / "原始回合"
        self.run.mkdir()
        self.manifest, self.plan, self.receipts = fixture()

    def save(self, receipts=True):
        for name, value in (("manifest.json", self.manifest), ("compiled-plan.json", self.plan)):
            (self.run / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
        if receipts:
            (self.run / "receipts.jsonl").write_text("".join(json.dumps(row) + "\n" for row in self.receipts), encoding="utf-8")

    def analyze(self, **kwargs):
        self.save()
        return analyzer.analyze_run(self.run, **kwargs)

    def save_hud(self):
        samples = []
        for i, time in enumerate((950, 1000, 1050)):
            received = (9920 + time) * MS
            samples.append(dict(sequence=i + 1, epoch=1, generation=1, geometry_version=1,
                                received_ns=received, processed_ns=received + MS,
                                source_timestamp_us=None, raw_class=3, effective_HUD=True,
                                source_valid=True, byte_offset=i * 18720))
        pixels = b"\x7f" * (len(samples) * 18720)
        value = dict(schema="pas.experimental-hud-timeline.v1", clock_id="host-QPC-receipt-only",
                     source_sha256=SOURCE, pixels_file="hud-timeline.gray",
                     pixels_sha256=hashlib.sha256(pixels).hexdigest(), pixels_bytes=len(pixels),
                     sample_bytes=18720, samples=samples, frame_geometry=[1280, 720],
                     score_region_xywh=[1076, 20, 180, 40], combo_region_xywh=[560, 8, 160, 72],
                     stats=dict(samples=len(samples), capacity_drops=0, max_sample_gap_ns=50 * MS))
        (self.run / "hud-timeline.json").write_text(json.dumps(value), encoding="utf-8")
        (self.run / "hud-timeline.gray").write_bytes(pixels)
        return value

    def test_metadata_only_hud_keeps_receipt_neighbors_without_claiming_pixels(self):
        value = self.save_hud()
        (self.run / "hud-timeline.gray").unlink()
        value.update(schema="pas.experimental-hud-timeline-metadata.v1", pixels_retained=False,
                     pixels_file=None, pixels_sha256=None, pixels_bytes=0, allocated_pixel_bytes=0)
        for sample in value["samples"]:
            sample["byte_offset"] = None
        (self.run / "hud-timeline.json").write_text(json.dumps(value), encoding="utf-8")
        result = self.analyze()
        self.assertTrue(result["hud"]["available"])
        self.assertFalse(result["hud"]["pixel_hash_verified"])
        self.assertIn("hud_pixels_not_retained_by_policy", result["hud"]["gaps"])
        self.assertTrue(result["notes"][0]["hud_neighbors"])
        self.assertTrue(all(x["byte_offset"] is None for x in result["notes"][0]["hud_neighbors"]))
        value["pixels_file"] = "elsewhere.gray"
        (self.run / "hud-timeline.json").write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "retained image data"):
            self.analyze()

    def test_matches_birth_sorted_intent_and_reports_host_metrics_without_verdict(self):
        report = self.analyze()
        row = report["notes"][0]
        self.assertEqual((row["source_contact_id"], row["intent_id"], row["pointer_ids"]), (3, 2, [1]))
        self.assertEqual(row["planned_lead_ns"], 30 * MS)
        self.assertEqual(row["planned"]["path_length_pixels"], 60)
        self.assertEqual(row["planned"]["stroke_duration_ns"], 60 * MS)
        self.assertEqual(row["planned"]["release_dwell_ns"], 24 * MS)
        self.assertEqual(row["planned"]["max_segment_speed_pixels_per_second"], 1000)
        self.assertEqual(row["max_dispatch_lateness_ns"], MS // 2)
        self.assertEqual(row["matched_steps"], 4)
        self.assertEqual(row["planned_overlap"]["overlapping_intents"], [1])
        self.assertEqual(row["planned_overlap"]["peak_active_contacts"], 2)
        self.assertEqual(row["candidates"], [])
        self.assertTrue(row["all_planned_steps_have_successful_host_receipts"])
        self.assertFalse(row["game_miss_confirmed"])
        self.assertFalse(report["summary"]["AP_verified"])
        self.assertEqual(report["summary"]["game_misses_identified"], 0)

    def test_late_down_compresses_stroke_and_is_only_a_candidate(self):
        flick = [row for row in self.receipts if row["intent_id"] == 2]
        for row, delay in zip(flick[:2], (50 * MS, 21 * MS)):
            row["host_start_ns"] = row["scheduled_ns"] + delay
            row["host_return_ns"] = row["host_start_ns"] + 100_000
        row = self.analyze()["notes"][0]
        self.assertIn("dispatch_lateness_above_diagnostic_threshold", row["candidates"])
        self.assertIn("host_dispatch_stroke_duration_below_half_planned", row["candidates"])
        self.assertLess(row["dispatch_to_plan_duration_ratio"], .5)
        self.assertFalse(row["game_miss_confirmed"])

    def test_failed_rpc_is_separate_from_missing_receipt(self):
        next(row for row in self.receipts if row["intent_id"] == 2 and row["phase"] == 1)["success"] = False
        row = self.analyze()["notes"][0]
        self.assertEqual(row["matched_steps"], 4)
        self.assertIn("RPC_failure_or_unknown_effect", row["candidates"])
        self.assertNotIn("missing_or_mismatched_command_receipts", row["candidates"])
        self.assertFalse(row["all_planned_steps_have_successful_host_receipts"])
        self.assertTrue(row["host_dispatch_overlap"]["state_after_failed_RPC_uncertain"])

    def test_missing_and_mismatched_commands_do_not_become_misses(self):
        self.receipts = [row for row in self.receipts if not (row["intent_id"] == 2 and row["phase"] == 2)]
        next(row for row in self.receipts if row["intent_id"] == 2)["y"] += 1
        report = self.analyze()
        row = report["notes"][0]
        self.assertEqual(row["missing_or_mismatched_step_indices"], [0, 3])
        self.assertIn("unexpected_or_duplicate_command_receipts", row["candidates"])
        self.assertIn("manifest_receipt_count_mismatch", report["evidence_gaps"])
        self.assertEqual(report["summary"]["game_misses_identified"], 0)

    def test_pointer_collision_and_close_hold_are_distinct_candidates(self):
        for point in self.plan["contacts"][1]["steps"]:
            point["point"][0] = 640 / 1279
        for receipt in self.receipts:
            if receipt["intent_id"] == 1:
                receipt["x"] = 640
                receipt["id"] = 1
        row = self.analyze()["notes"][0]
        self.assertIn("pointer_reused_before_successful_UP", row["candidates"])
        self.assertIn("nearby_contact_interference_candidate", row["candidates"])
        self.assertEqual(row["planned_overlap"]["minimum_other_distance_pixels"], 0)
        self.assertEqual(row["host_dispatch_overlap"]["pointer_collisions"][0]["other_intent"], 1)

    def test_reusing_pointer_after_up_records_gap_without_collision(self):
        hold_times = (900, 920, 940)
        for step, time in zip(self.plan["contacts"][1]["steps"], hold_times):
            step["song_ns"] = time * MS
        for receipt, time in zip((row for row in self.receipts if row["intent_id"] == 1), hold_times):
            receipt["scheduled_ns"] = (9920 + time) * MS
            receipt["host_start_ns"] = receipt["scheduled_ns"] + MS // 2
            receipt["host_return_ns"] = receipt["host_start_ns"] + 100_000
        for receipt in self.receipts:
            receipt["id"] = 0
        self.receipts.sort(key=lambda row: row["host_start_ns"])
        row = self.analyze()["notes"][0]
        self.assertEqual(row["host_dispatch_overlap"]["pointer_reuse_gap_ns"], 30 * MS - 100_000)
        self.assertEqual(row["host_dispatch_overlap"]["pointer_collisions"], [])
        self.assertEqual(row["host_dispatch_overlap"]["pointer_changes"], [])
        self.assertEqual(row["planned_overlap"]["peak_active_contacts"], 1)

    def test_projection_small_screen_path_and_direction_reversal_are_visible(self):
        pixels = (350, 354, 352, 352)
        for step, y in zip(self.plan["contacts"][0]["steps"], pixels):
            step["point"][1] = y / 719
        for receipt, y in zip((row for row in self.receipts if row["intent_id"] == 2), pixels):
            receipt["y"] = y
        self.plan["ownership"][0]["projection_min_reference_pixels"] = -48.5
        row = self.analyze()["notes"][0]
        self.assertEqual(row["planned"]["path_length_pixels"], 6)
        self.assertEqual(row["planned"]["end_displacement_pixels"], 2)
        self.assertEqual(row["planned"]["direction_reversals"], 1)
        self.assertIn("screen_displacement_below_quarter_nominal_stroke", row["candidates"])
        self.assertIn("normal_projection_requires_geometry_review", row["candidates"])

    def test_hud_bytes_verified_and_indexed_without_combo_inference(self):
        self.save_hud()
        report = self.analyze()
        self.assertTrue(report["hud"]["pixel_hash_verified"])
        row = report["notes"][0]
        self.assertEqual([item["index"] for item in row["hud_neighbors"]], [0, 1])
        self.assertEqual(row["hud_neighbors"][1]["delta_from_hypothesis_judgment_ns"], 0)
        self.assertFalse(row["game_miss_confirmed"])
        self.assertIn("game_per_note_judgment_not_observed", row["evidence_gaps"])

    def test_tampered_hud_bytes_or_source_rejected(self):
        value = self.save_hud()
        (self.run / "hud-timeline.gray").write_bytes(b"\x80" * (3 * 18720))
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.analyze()
        value["source_sha256"] = "d" * 64
        (self.run / "hud-timeline.json").write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "different capture"):
            self.analyze()

    def test_missing_receipts_and_anchor_remain_unknown(self):
        del self.manifest["anchor"]
        self.save(receipts=False)
        report = analyzer.analyze_run(self.run)
        self.assertIn("arrival_anchor_missing", report["evidence_gaps"])
        self.assertIn("receipts_missing", report["evidence_gaps"])
        self.assertIsNone(report["notes"][0]["max_dispatch_lateness_ns"])
        self.assertEqual(report["notes"][0]["judgment_status"], "not_determined_by_tool")

    def test_manual_window_bound_to_exact_manifest_is_correlation_only(self):
        self.save()
        windows = dict(schema="pas.manual-miss-windows.v1", time_basis="song_ns", chart_sha256=CHART,
                       run_manifest_sha256=hashlib.sha256((self.run / "manifest.json").read_bytes()).hexdigest(),
                       windows=[dict(id="human-001", begin_ns=990 * MS, end_ns=1010 * MS,
                                     evidence="Human saw a lost combo in the local recording; exact note uncertain.")])
        path = self.root / "windows.json"
        path.write_text(json.dumps(windows), encoding="utf-8")
        row = analyzer.analyze_run(self.run, miss_windows_path=path)["notes"][0]
        self.assertIn("within_manually_annotated_miss_window", row["candidates"])
        self.assertFalse(row["game_miss_confirmed"])
        windows["run_manifest_sha256"] = "0" * 64
        path.write_text(json.dumps(windows), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "bind this manifest"):
            analyzer.analyze_run(self.run, miss_windows_path=path)

    def test_mismatched_plan_or_parameter_identity_rejected(self):
        original = copy.deepcopy(self.manifest)
        self.manifest["expected_plan_sha256"] = "d" * 64
        with self.assertRaisesRegex(ValueError, "identities"):
            self.analyze()
        self.manifest = original
        self.manifest["experimental_flick_lead_ns"] += MS
        with self.assertRaisesRegex(ValueError, "parameters differ"):
            self.analyze()

    def test_malformed_contact_types_and_bool_times_rejected(self):
        baseline = copy.deepcopy(self.plan)
        for replacement in (None, [], "contact"):
            self.plan = copy.deepcopy(baseline)
            self.plan["contacts"][0] = replacement
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                self.analyze()
        self.plan = baseline
        self.plan["contacts"][0]["steps"][0]["song_ns"] = True
        with self.assertRaises(ValueError):
            self.analyze()

    def test_duplicate_receipt_same_deadline_is_not_silently_matched(self):
        target = next(row for row in self.receipts if row["intent_id"] == 2)
        self.receipts.append(copy.deepcopy(target))
        self.receipts.sort(key=lambda row: row["host_start_ns"])
        row = self.analyze()["notes"][0]
        self.assertEqual(row["matched_steps"], 3)
        self.assertIn("unexpected_or_duplicate_command_receipts", row["candidates"])

    def test_readonly_cli_and_output_location_rules(self):
        self.save()
        before = {path.name: path.read_bytes() for path in self.run.iterdir()}
        output = self.root / "analysis.json"
        process = subprocess.run([sys.executable, str(TOOL), "--run-dir", str(self.run), "--output", str(output)],
                                 text=True, capture_output=True, check=False)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual({path.name: path.read_bytes() for path in self.run.iterdir()}, before)
        result = json.loads(output.read_text(encoding="utf-8"))
        self.assertFalse(result["input_enabled"])
        with self.assertRaises(ValueError):
            analyzer.write_report(result, self.run / "analysis.json", self.run)
        with self.assertRaises(FileExistsError):
            analyzer.write_report(result, output, self.run)


if __name__ == "__main__":
    unittest.main()
