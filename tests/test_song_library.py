import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location("build_song_library", TOOLS / "build_song_library.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SongLibraryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "本機曲庫"
        self.root.mkdir()
        self.jobs = [self.job("synthetic-" + str(i), i) for i in range(1, 21)]
        self.plan = self.root / "plan.json"
        self.write_plan()
        self.extra = self.root / "extra.json"
        self.extra_value = dict(schema="pas.extra-song-library-entry.v1", display_name="Extra synthetic",
                                job=self.job("extra.synthetic", 0, extra=True))
        self.write_extra()
        self.output = self.root / "new-library"

    def file(self, name, value):
        path = self.root / name
        path.write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode("utf-8"))
        return dict(file=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())

    def job(self, track, order, extra=False):
        chart = self.file(track + ".chart", b"synthetic native bytes " + track.encode())
        state = self.file(track + ".state", b"synthetic state")
        hud = self.file(track + ".rgb", b"synthetic pixels")
        plan_sha = hashlib.sha256(track.encode()).hexdigest()
        profile = dict(schema="pas.adaptive-song-profile.v1", song_id=track, difficulty="IN",
                       chart_sha256=chart["sha256"], expected_plan_sha256=plan_sha,
                       expected_contacts=1, expected_steps=2, expected_peak=1, audio_sha256="a" * 64)
        source = None
        if extra:
            source = self.file(track + ".source.json", dict(track_id=track, difficulty="IN",
                chart=dict(sha256=chart["sha256"]), decoded_audio=dict(sha256="a" * 64)))
            profile["source_manifest_sha256"] = source["sha256"]
        result = dict(order=order, track_id=track, difficulty="IN", rounds=1, notes=1, events=2, peak=1,
                      compiled_fingerprint=plan_sha, chart=chart, profile=self.file(track + ".profile", profile),
                      state_calibration=state, hud_reference=hud, ready_for_manual_preflight=True,
                      blockers=[], qualification_warnings=[])
        if source:
            result["source_manifest"] = source
        return result

    def write_plan(self):
        self.plan.write_text(json.dumps(dict(schema="pas.chapter-IN-sweep-plan.v1", chapter_id="MainStoryLegacy",
            song_count=20, round_limit=20, jobs=self.jobs)), encoding="utf-8")

    def write_extra(self):
        self.extra.write_text(json.dumps(self.extra_value), encoding="utf-8")

    def build(self):
        return MODULE.build_library(self.plan, [self.extra], self.output)

    def test_warmup_is_separate_and_native_twenty_remain_in_order(self):
        original = self.plan.read_bytes()
        value = self.build()
        self.assertEqual((value["song_count"], value["chapter_count"], value["extra_count"]), (21, 20, 1))
        self.assertEqual(value["songs"][0]["role"], "warmup")
        self.assertIsNone(value["songs"][0]["native_order"])
        self.assertEqual([row["track_id"] for row in value["songs"][1:]], [row["track_id"] for row in self.jobs])
        self.assertEqual([row["native_order"] for row in value["songs"][1:]], list(range(1, 21)))
        self.assertEqual(self.plan.read_bytes(), original)
        self.assertFalse(value["input_enabled"])
        self.assertFalse(value["device_discovery"])

    def test_blocked_candidate_is_visible_but_not_selected(self):
        self.extra_value["job"]["blockers"] = [dict(category="synthetic", reason="requires observation")]
        self.extra_value["job"]["ready_for_manual_preflight"] = False
        self.write_extra()
        row = self.build()["songs"][0]
        self.assertFalse(row["default_selected"])
        self.assertEqual(row["blockers"], self.extra_value["job"]["blockers"])

    def test_changed_profile_bytes_fail_before_output(self):
        path = Path(self.extra_value["job"]["profile"]["file"])
        path.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "SHA256"):
            self.build()
        self.assertFalse(self.output.exists())

    def test_source_identity_mismatch_rejected(self):
        job = self.extra_value["job"]
        source = dict(track_id="wrong", difficulty="IN", chart=dict(sha256=job["chart"]["sha256"]))
        job["source_manifest"] = self.file("forged-source.json", source)
        self.write_extra()
        with self.assertRaisesRegex(ValueError, "source identity"):
            self.build()

    def test_extra_cannot_duplicate_native_membership(self):
        self.extra_value["job"] = copy.deepcopy(self.jobs[0])
        job = self.extra_value["job"]
        job["source_manifest"] = self.file("native-source.json", dict(track_id=job["track_id"],
            difficulty="IN", chart=dict(sha256=job["chart"]["sha256"]), decoded_audio=dict(sha256="a" * 64)))
        profile = json.loads(Path(job["profile"]["file"]).read_text(encoding="utf-8"))
        profile["source_manifest_sha256"] = job["source_manifest"]["sha256"]
        job["profile"] = self.file("extra-native.profile", profile)
        self.write_extra()
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.build()

    def test_native_order_and_member_count_are_not_editable(self):
        self.jobs.reverse()
        self.write_plan()
        with self.assertRaisesRegex(ValueError, "order"):
            self.build()
        self.jobs.pop()
        self.write_plan()
        with self.assertRaisesRegex(ValueError, "exactly 20"):
            self.build()

    def test_existing_library_remains_untouched(self):
        self.output.mkdir()
        marker = self.output / "keep"
        marker.write_bytes(b"preserve")
        with self.assertRaises(FileExistsError):
            self.build()
        self.assertEqual(marker.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
