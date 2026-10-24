import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("prepare_chapter_sweep", Path(__file__).parents[1] / "tools/prepare_chapter_sweep.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SweepPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "中文"
        self.archive, self.ready, self.output = self.root / "archive", self.root / "readiness", self.root / "sweep"
        self.archive.mkdir(parents=True)
        self.ready.mkdir()
        self.members, self.rows = [], []
        for ordinal, track in enumerate(("first.synthetic.0", "光.synthetic.0"), 1):
            chart = self.archive / f"song-{ordinal}.json"
            chart.write_bytes(b"synthetic raw chart " + str(ordinal).encode())
            chart_sha = hashlib.sha256(chart.read_bytes()).hexdigest()
            plan_sha = hashlib.sha256(f"synthetic-plan-{ordinal}".encode()).hexdigest()
            profile = self.ready / f"profile-{ordinal}.json"
            profile.write_text(json.dumps(dict(song_id=track, difficulty="IN", chart_sha256=chart_sha,
                                              expected_plan_sha256=plan_sha)), encoding="utf-8")
            self.members.append(dict(track_id=track, charts=[dict(difficulty="IN", chart=dict(file=chart.name, sha256=chart_sha))]))
            self.rows.append(dict(track_id=track, order=ordinal, relative_chart_path=chart.name,
                                  profile_path=profile.name, compiled_fingerprint=plan_sha,
                                  blockers=[], notes=1, events=2, peak=1, opening_model_status="synthetic"))
        self.write_inputs()

    def write_inputs(self):
        index = self.archive / "chapter-index.json"
        index.write_text(json.dumps(dict(schema="pas.phigros-chapter-archive.v1", songs=self.members,
                                         status="archived_with_analysis_gaps")), encoding="utf-8")
        report = dict(schema="pas.chapter-legacy-IN-readiness-batch.v1", songs=self.rows,
                      chapter_index_sha256=hashlib.sha256(index.read_bytes()).hexdigest())
        (self.ready / "summary.json").write_text(json.dumps(report), encoding="utf-8")

    def prepare(self):
        return MODULE.prepare(self.archive, self.ready, self.output)

    def test_preserves_all_members_and_missing_ui_as_blockers(self):
        self.rows[1]["blockers"] = [dict(category="native_scope", reason="synthetic unsupported opening")]
        self.write_inputs()
        result = self.prepare()
        self.assertEqual([job["track_id"] for job in result["jobs"]], [row["track_id"] for row in self.rows])
        self.assertEqual(result["round_limit"], 2)
        self.assertEqual(result["blocked_count"], 2)
        self.assertFalse(result["device_discovery"])
        self.assertFalse(result["input_enabled"])
        self.assertFalse(result["policy"]["automatic_retry"])
        self.assertIn("native_scope", [item["category"] for item in result["jobs"][1]["blockers"]])

    def test_candidate_readiness_retains_failed_cost_qualification_and_ui_scope(self):
        for row in self.rows:
            row["ui_binding"] = dict(present=True, state_file="shared.json", hud_file="shared.rgb",
                state_sha256=hashlib.sha256(b"synthetic shared policy").hexdigest(),
                hud_sha256=hashlib.sha256(b"synthetic RGB").hexdigest(),
                evidence_scope="explicit shared candidate, pixels NOT RUN", actual_per_song_pixel_validation_qualified=False)
        self.root.joinpath("shared.json").write_bytes(b"synthetic shared policy")
        self.root.joinpath("shared.rgb").write_bytes(b"synthetic RGB")
        warning = dict(category="measured_cost_replay_failed", reason="serial second DOWN exceeds unchanged 20ms guard")
        self.rows[1]["qualification_warnings"] = [warning]
        self.rows[1]["all_measured_cost_nextwake_replays_complete"] = False
        self.write_inputs()
        result = MODULE.prepare(self.archive, self.ready, self.output, self.root)
        self.assertEqual(result["ready_count"], 2)
        self.assertEqual(result["jobs"][1]["qualification_warnings"], [warning])
        self.assertFalse(result["jobs"][1]["measured_cost_replays_qualified"])
        self.assertFalse(result["jobs"][1]["actual_per_song_pixel_validation_qualified"])
        self.assertFalse(result["gameplay_qualified"])
        self.assertIn("no complete-song", result["ready_scope"])

    def test_reordered_or_missing_rows_rejected(self):
        self.rows.reverse()
        self.write_inputs()
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_duplicate_native_member_rejected(self):
        self.members[1]["track_id"] = self.members[0]["track_id"]
        self.write_inputs()
        with self.assertRaises(ValueError):
            self.prepare()

    def test_changed_raw_chart_rejected(self):
        (self.archive / self.rows[0]["relative_chart_path"]).write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "SHA256"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_changed_profile_identity_rejected(self):
        path = self.ready / self.rows[0]["profile_path"]
        value = json.loads(path.read_text(encoding="utf-8"))
        value["expected_plan_sha256"] = "0" * 64
        path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "profile"):
            self.prepare()

    def test_path_escape_rejected(self):
        self.rows[0]["relative_chart_path"] = "../private.json"
        self.members[0]["charts"][0]["chart"]["file"] = "../private.json"
        self.write_inputs()
        with self.assertRaisesRegex(ValueError, "escapes"):
            self.prepare()

    def test_readiness_index_binding_rejected(self):
        path = self.ready / "summary.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        value["chapter_index_sha256"] = "0" * 64
        path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "different chapter"):
            self.prepare()

    def test_verified_derivative_index_is_explicitly_bound(self):
        path = self.archive / "verified-index.json"
        value = json.loads((self.archive / "chapter-index.json").read_text(encoding="utf-8"))
        value["schema"] = "pas.phigros-chapter-verified-index.v1"
        value["raw_archive_complete"] = True
        path.write_text(json.dumps(value), encoding="utf-8")
        report_path = self.ready / "summary.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        report["chapter_index_file"] = str(path)
        report["chapter_index_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        report_path.write_text(json.dumps(report), encoding="utf-8")
        self.assertEqual(self.prepare()["chapter_index"]["file"], str(path.resolve()))

    def test_index_outside_archive_rejected(self):
        report_path = self.ready / "summary.json"
        value = json.loads(report_path.read_text(encoding="utf-8"))
        value["chapter_index_file"] = str(self.root / "private-index.json")
        report_path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "escapes archive"):
            self.prepare()

    def test_existing_output_preserved(self):
        self.output.mkdir()
        marker = self.output / "keep.txt"
        marker.write_bytes(b"keep")
        with self.assertRaises(FileExistsError):
            self.prepare()
        self.assertEqual(marker.read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()
