"""Offline tests for the history reader, using temporary synthetic runs."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "round_history.py"
sys.path.insert(0, str(ROOT / "tools"))
import round_history as history


def write_json(path, value):
    raw = json.dumps(value).encode("utf-8")
    path.write_bytes(raw)
    return hashlib.sha256(raw).hexdigest()


class RoundHistoryTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="pas-history-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve() / "runs"
        self.root.mkdir()

    def panel(self, name, track="Synthetic.Track.0", level="IN", planned=4,
              receipts=0, failure=None, kernel_zero=True, with_manifest=True):
        run = self.root / name
        round_dir = run / "round-01"
        round_dir.mkdir(parents=True)
        manifest = {
            "schema": history.MANIFEST_SCHEMA, "song_id": track, "difficulty": level,
            "failure": failure, "full_chart_notes": planned, "full_compiled_steps": planned,
            "down_attempts": receipts, "up_attempts": receipts, "receipt_rows": receipts,
            "executable_sha256": history.NOV03_EXECUTABLE, "experimental_phase_ms": -70,
            "touch_stream": {"events": [
                {"command": {"intent_id": 1}, "message_write_ok": True} for _ in range(receipts)
            ]},
        }
        sha = write_json(round_dir / "manifest.json", manifest) if with_manifest else None
        image = b"presence-only synthetic fixture"
        if receipts:
            (round_dir / "result.png").write_bytes(image)
        ledger = {
            "schema": "pas.manual-catalog-panel-ledger.v1", "status": "completed" if receipts else "failed",
            "started_utc": "2026-11-03T07:13:20.6360973Z",
            "configuration_sha256": history.NOV03_CONFIG,
            "library": {"sha256": history.NOV03_LIBRARY},
            "rounds": [{
                "order": 1, "track_id": track, "library_id": f"example/synthetic/revision/{track}/{level}/hash",
                "status": "sent_result_unreviewed" if receipts else "failed_or_stopped",
                "manifest_sha256": sha, "manifest_identity_matched": with_manifest,
                "exit_code": 0 if receipts else 2, "failure": failure, "runtime_failure": failure,
                "execution_state": "completed" if receipts else "incomplete",
                "result_state": "confirmed_by_native" if receipts else "not_confirmed",
                "release_state": "kernel_zero_confirmed" if kernel_zero else "kernel_zero_not_confirmed",
                "judgment": "unreviewed",
                "evidence_files": [{"file": r"D:\old\round-01\result.png", "sha256": hashlib.sha256(image).hexdigest()}] if receipts else [],
            }],
        }
        write_json(run / "ledger.json", ledger)
        return run, ledger

    def test_separates_plan_attempt_transport_result_and_judgment(self):
        run, _ = self.panel("catalog-manual-panel-20261103-153120-179-669b50", receipts=4)
        row = history.history(self.root)["rows"][0]
        self.assertEqual(row["started_utc"], "2026-11-03T07:13:20.6360973Z")
        self.assertEqual(row["started_local_unqualified"], "2026-11-03T15:31:20.179")
        self.assertEqual(row["origin"], "desktop_panel")
        self.assertEqual(row["difficulty"], "IN")
        self.assertEqual(row["planned_steps"], 4)
        self.assertEqual(row["transport_written_steps"], 4)
        self.assertEqual(row["receipt_rows"], 4)
        self.assertEqual(row["result_state"], "confirmed_by_native")
        self.assertEqual(row["judgment_state"], "unreviewed")
        self.assertEqual(row["result_file"], str(run / "round-01" / "result.png"))

    def test_legacy_gate_is_ambiguous_and_cleanup_is_separate(self):
        raw = ("first_note_trigger_cancelled: first_note_dispatch_lead_below_200ms; "
               "kernel_final: ADB read-only kernel observer snapshot: ADB timed out")
        self.panel("catalog-manual-panel-20261103-153120-179-669b50", failure=raw, kernel_zero=False)
        row = history.history(self.root)["rows"][0]
        self.assertEqual(row["failure_stage"], "first_note")
        self.assertEqual(row["failure_code"], history.LEGACY_GATE)
        self.assertTrue(row["legacy_first_note_gate_ambiguous"])
        self.assertEqual(row["planned_steps"], 4)
        self.assertEqual(row["transport_written_steps"], 0)
        self.assertEqual(row["release_state"], "kernel_zero_not_confirmed")
        self.assertEqual(row["cleanup_errors"], [
            {"stage": "kernel_final", "detail": "ADB read-only kernel observer snapshot: ADB timed out"}
        ])

    def test_missing_or_mismatched_manifest_does_not_confirm_result(self):
        run, _ = self.panel("catalog-manual-panel-20261103-153120-179-669b50", receipts=4)
        manifest = run / "round-01" / "manifest.json"
        with manifest.open("ab") as stream:
            stream.write(b"\n")
        altered = history.history(self.root)["rows"][0]
        self.assertEqual(altered["manifest_state"], "hash_mismatch")
        manifest.unlink()
        absent = history.history(self.root)["rows"][0]
        self.assertEqual(absent["manifest_state"], "missing")
        for row in (altered, absent):
            self.assertEqual(row["execution_state"], "unknown")
            self.assertEqual(row["result_state"], "unknown")
            self.assertEqual(row["release_state"], "unknown")
            self.assertEqual(row["judgment_state"], "unknown")
            self.assertIsNone(row["transport_written_steps"])
            self.assertIsNotNone(row["result_file"])

    def test_preflight_only_and_corrupt_ledgers_remain_visible(self):
        prefix = "catalog-manual-panel-20261103-"
        run = self.root / (prefix + "151320-613-7d95f4")
        run.mkdir()
        write_json(run / "ledger.json", {
            "schema": "pas.manual-catalog-panel-ledger.v1", "status": "failed",
            "selected_ids": ["example/synthetic/revision/Sample.Track.0/AT/hash"],
            "failure": "preflight exited", "rounds": [],
        })
        broken = self.root / (prefix + "151741-169-8f2b29")
        broken.mkdir()
        (broken / "ledger.json").write_text("{ truncated", encoding="utf-8")
        corrupt, preflight = history.history(self.root)["rows"]
        self.assertEqual(corrupt["execution_state"], "unknown")
        self.assertIn("ledger.json:unreadable", corrupt["issues"])
        self.assertEqual(preflight["track_id"], "Sample.Track.0")
        self.assertEqual(preflight["difficulty"], "AT")
        self.assertEqual(preflight["execution_state"], "not_started")
        self.assertIsNone(preflight["round_id"])
        self.assertEqual(preflight["failure_detail"], "preflight exited")

    def test_filters_origin_and_sorts_wall_time_without_assuming_offset(self):
        self.panel("catalog-manual-panel-20261103-153120-179-669b50", track="SyntheticSong.Track.0", level="AT")
        control = self.root / "computer-hypothesis-20261103-153121-a1b2c3"
        control.mkdir()
        write_json(control / "intent.json", {
            "schema": history.CONTROL_SCHEMA, "track_id": "SyntheticSong.Track.0", "difficulty": "IN",
        })
        write_json(control / "completion.json", {"exit_code": 2})
        filtered = history.history(self.root, "synthetic song", limit=1)
        self.assertEqual(filtered["total_matching"], 2)
        self.assertEqual(len(filtered["rows"]), 1)
        row = filtered["rows"][0]
        self.assertEqual(row["origin"], "computer_control")
        self.assertEqual(row["exit_code"], 2)
        self.assertIsNone(row["started_utc"])
        self.assertEqual(row["manifest_state"], "missing")
        self.assertEqual(history.history(self.root, level="AT")["rows"][0]["origin"], "desktop_panel")

    def test_new_ledger_fields_are_preserved_with_precise_gate(self):
        run, ledger = self.panel("catalog-manual-panel-20261103-153120-179-669b50")
        ledger["launch_id"] = "launch-identifier"
        ledger["frontend_release"] = {"file": "local/release.json", "sha256": "a" * 64}
        ledger["rounds"][0].update({
            "display_name": "曲名", "attempt_id": "attempt-identifier", "failure_stage": "first_note",
            "failure_code": "dispatch_lead_insufficient", "failure_detail": "insufficient measured lead",
            "first_note_gate": {"lead_ns": 100000000, "minimum_ns": 200000000},
            "milestones": {"ready": "2026-11-03T07:13:21Z"},
        })
        write_json(run / "ledger.json", ledger)
        row = history.history(self.root, track="曲名")["rows"][0]
        self.assertEqual(row["launch_id"], "launch-identifier")
        self.assertEqual(row["attempt_id"], "attempt-identifier")
        self.assertEqual(row["frontend_release_sha256"], "a" * 64)
        self.assertEqual(row["first_note_gate"]["lead_ns"], 100000000)
        self.assertFalse(row["legacy_first_note_gate_ambiguous"])

    def test_acceptance_cli_on_synthetic_mirrors_and_missing_record(self):
        for suffix, (track, level, planned, receipts, failure, zero) in history.NOV03.items():
            primary = None
            if failure == history.LEGACY_GATE:
                primary = "first_note_trigger_cancelled: first_note_dispatch_lead_below_200ms"
            if failure == "source_timestamp_jump":
                primary = ("source timestamp jumped beyond 10 seconds; "
                           "kernel_final: ADB read-only kernel observer snapshot: ADB timed out")
            self.panel(history.NOV03_PREFIX + suffix, track, level, planned, receipts, primary, zero)
        command = [sys.executable, str(TOOL), "--runs-dir", str(self.root), "--accept-nov03", "--format", "json"]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        report = json.loads(result.stdout)
        self.assertTrue(report["passed"])
        self.assertEqual(len(report["checks"]), 6)
        missing = self.root / (history.NOV03_PREFIX + next(iter(history.NOV03)))
        missing.rename(self.root / "excluded-fixture")
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(json.loads(result.stdout)["checks"][0]["errors"], ["expected one round, found 0"])

    def test_manual_review_keeps_raw_record_and_uses_pinned_result(self):
        name = "catalog-manual-panel-20261103-153120-179-669b50"
        run, _ = self.panel(name, receipts=4)
        original = {path: path.read_bytes() for path in run.rglob("*") if path.is_file()}
        command = [sys.executable, str(TOOL), "--runs-dir", str(self.root), "--format", "json",
                   "--review", name + "/round-01", "--perfect", "2", "--good", "1", "--bad", "1", "--miss", "0"]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        report = json.loads(result.stdout)
        sidecar = Path(report["review_file"])
        self.assertEqual(sidecar.parent, self.root.parent / "score-reviews")
        self.assertEqual(sorted(sidecar.parent.iterdir()), [sidecar])
        self.assertEqual(original, {path: path.read_bytes() for path in run.rglob("*") if path.is_file()})
        row = history.history(self.root)["rows"][0]
        self.assertEqual(row["native_judgment_state"], "unreviewed")
        self.assertEqual(row["judgment_state"], "manually_reported")
        self.assertEqual(row["manual_judgments"], {"perfect": 2, "good": 1, "bad": 1, "miss": 0})
        self.assertNotIn("AP_verified", row)
        (run / "round-01" / "result.png").write_bytes(b"altered")
        row = history.history(self.root)["rows"][0]
        self.assertEqual(row["judgment_state"], "unreviewed")
        self.assertIn("manual_review:invalid_or_stale", row["issues"])

    def test_manual_review_rejects_wrong_sum_and_missing_pin(self):
        name = "catalog-manual-panel-20261103-153120-179-669b50"
        run, ledger = self.panel(name, receipts=4)
        with self.assertRaisesRegex(ValueError, "sum to"):
            history.record_review(self.root, name + "/round-01", dict(perfect=2, good=0, bad=0, miss=0))
        ledger["rounds"][0]["evidence_files"] = []
        write_json(run / "ledger.json", ledger)
        with self.assertRaisesRegex(ValueError, "pinned result"):
            history.record_review(self.root, name + "/round-01", dict(perfect=4, good=0, bad=0, miss=0))
        self.assertFalse((self.root.parent / "score-reviews").exists())

    def test_completed_archive_cli_freezes_the_utc_window_and_preserves_originals(self):
        selected = "catalog-manual-panel-20261107-171938-242-4e1c74"
        run, ledger = self.panel(selected, receipts=4)
        ledger["rounds"][0]["started_utc"] = "2026-11-07T09:20:09Z"
        write_json(run / "ledger.json", ledger)
        failed, ledger = self.panel("catalog-manual-panel-20261107-172328-301-45efca", failure="user_stop")
        ledger["rounds"][0]["started_utc"] = "2026-11-07T09:23:28Z"
        write_json(failed / "ledger.json", ledger)
        self.panel("catalog-manual-panel-20261103-153120-179-669b50", receipts=4)
        original = {path: path.read_bytes() for path in self.root.rglob("*") if path.is_file()}
        command = [sys.executable, str(TOOL), "--runs-dir", str(self.root), "--format", "json"]
        archive_args = ["--archive-completed", "nov07-evening", "--since-utc", "2026-11-07T09:00:00Z",
                        "--until-utc", "2026-11-07T11:13:00Z"]
        report = json.loads(subprocess.run(command + archive_args, capture_output=True, text=True, check=True).stdout)
        repeated = json.loads(subprocess.run(command + archive_args, capture_output=True, text=True, check=True).stdout)
        self.assertEqual(report, repeated)
        self.assertEqual(report["record_count"], 1)
        self.assertEqual(report["rounds"][0]["run_id"], selected)
        self.assertEqual(report["rounds"][0]["native_judgment_state"], "unreviewed")
        self.assertEqual(original, {path: path.read_bytes() for path in self.root.rglob("*") if path.is_file()})
        filtered = json.loads(subprocess.run(command + ["--archive", "nov07-evening"], capture_output=True,
                                             text=True, check=True).stdout)
        self.assertEqual(filtered["total_matching"], 1)
        self.assertEqual(filtered["rows"][0]["archive_ids"], ["nov07-evening"])
        self.assertIsNone(filtered["rows"][0]["manual_judgments"])
        with self.assertRaisesRegex(ValueError, "different contents"):
            history.archive_completed(self.root, "nov07-evening", "2026-11-07T09:00:00Z", "2026-11-07T11:14:00Z")

    def test_completed_archive_requires_intact_result_and_manifest(self):
        run, _ = self.panel("catalog-manual-panel-20261103-153120-179-669b50", receipts=4)
        bounds = ("2026-11-03T00:00:00Z", "2026-11-03T23:59:59Z")
        history.archive_completed(self.root, "intact", *bounds)
        self.assertEqual(history.history(self.root, archived_only=True)["total_matching"], 1)
        (run / "round-01" / "result.png").write_bytes(b"altered")
        with self.assertRaisesRegex(ValueError, "archived SHA256"):
            history.archive_completed(self.root, "altered", *bounds)
        self.assertFalse((self.root.parent / "completed-archives" / "altered.json").exists())
        self.assertEqual(history.history(self.root, archived_only=True)["total_matching"], 0)
        row = history.history(self.root)["rows"][0]
        self.assertEqual(row["archive_ids"], [])
        self.assertIn("completed_archive:evidence_unavailable", row["issues"])


if __name__ == "__main__":
    unittest.main()
