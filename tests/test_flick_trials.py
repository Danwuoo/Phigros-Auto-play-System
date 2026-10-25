import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


TOOL = Path(__file__).resolve().parents[1] / "tools/prepare_flick_trials.py"
SPEC = importlib.util.spec_from_file_location("prepare_flick_trials", TOOL)
trials = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(trials)


class FlickTrialPreparationTests(unittest.TestCase):
    def test_straight_policy_is_preserved_in_every_trial_command(self):
        self.profile_value["flick_path_policy"] = "judgment-anchored-straight-v1"
        self.save_profile()
        result = self.prepare()
        for trial in result["trials"]:
            self.assertEqual(trial["parameters"]["flick_path_policy"], "judgment-anchored-straight-v1")
            args = trial["native_prepare_argv"]
            self.assertEqual(args[args.index("--flick-path-policy") + 1], "judgment-anchored-straight-v1")

    def test_invalid_path_policy_is_rejected(self):
        for invalid in (None, True, 1, "unknown"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                trials.parameters({"flick_path_policy": invalid})

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "譜面 John's $song"
        self.root.mkdir()
        self.chart = self.root / "chart.json"
        self.chart.write_bytes(b"synthetic chart; never executed")
        self.exe = self.root / "chart_experiment.exe"
        self.exe.write_bytes(b"synthetic executable; must never execute")
        self.source = self.root / "source-manifest.json"
        self.profile = self.root / "song-profile.json"
        self.output = self.root / "trials"
        chart_sha = hashlib.sha256(self.chart.read_bytes()).hexdigest()
        source = dict(track_id="synthetic.song.0", difficulty="IN",
                      chart=dict(sha256=chart_sha), decoded_audio=dict(sha256="a" * 64))
        self.source.write_text(json.dumps(source), encoding="utf-8")
        self.profile_value = dict(schema="pas.adaptive-song-profile.v1", song_id="synthetic.song.0", difficulty="IN",
                                  chart_sha256=chart_sha, audio_sha256="a" * 64,
                                  source_manifest_sha256=hashlib.sha256(self.source.read_bytes()).hexdigest(),
                                  drag_duration_ns=32_000_000, max_normal_projection_reference_pixels=7000,
                                  input_inset_pixels=64, contact_capacity=5,
                                  hud_policy="shared-in-subpixel-prefix-score-v3",
                                  import_policy_id="phigros-v3-strict-v1", tight_projection_speed_bounds=True)
        self.save_profile()

    def save_profile(self):
        self.profile.write_text(json.dumps(self.profile_value), encoding="utf-8")

    def prepare(self, vary="flick_distance_reference_pixels", values=(48, 72)):
        return trials.prepare_trials(self.profile, self.chart, self.source, self.exe, self.output, vary, values)

    def test_single_parameter_variants_keep_other_options_and_only_prepare(self):
        before = {path.name: path.read_bytes() for path in self.root.iterdir()}
        result = self.prepare()
        self.assertEqual(len(result["trials"]), 3)
        self.assertEqual(result["trials"][0]["parameters"], trials.DEFAULTS)
        self.assertFalse(result["execution_performed"])
        self.assertFalse(result["input_enabled"])
        self.assertIsNone(result["winner"])
        for index, trial in enumerate(result["trials"]):
            self.assertEqual(trial["changed_parameters"], ["flick_distance_reference_pixels"] if index else [])
            argv = trial["native_prepare_argv"]
            self.assertEqual(argv[1], "prepare-adaptive")
            options = dict(zip(argv[5::2], argv[6::2]))
            self.assertEqual(options["--contact-capacity"], "5")
            self.assertEqual(options["--input-inset-pixels"], "64")
            self.assertEqual(options["--normal-projection-pixels"], "7000")
            self.assertEqual(options["--flick-lead-ms"], "30")
            self.assertEqual(options["--flick-release-ms"], "24")
            self.assertEqual(options["--flick-travel-ms"], "60")
            self.assertFalse(Path(trial["output_directory"]).exists())
        script = (self.output / "prepare-commands.ps1").read_text(encoding="utf-8-sig")
        self.assertEqual(script.count("'prepare-adaptive'"), 3)
        self.assertIn("John''s $song", script)
        self.assertEqual(script.count("Get-FileHash -LiteralPath"), 4)
        self.assertNotIn("'run-adaptive'", script)
        self.assertEqual({path.name: path.read_bytes() for path in self.root.iterdir() if path.is_file()}, before)

    def test_numeric_duplicate_and_unchanged_candidates_removed(self):
        result = self.prepare(values=(60, 60.0, 48, 48.0))
        self.assertEqual(len(result["trials"]), 2)
        self.assertEqual(result["trials"][1]["changed_parameters"], ["flick_distance_reference_pixels"])

    def test_nondefault_baseline_preserved_and_timing_candidate_reaches_command(self):
        self.profile_value.update(flick_lead_ms=40, flick_travel_ms=80,
                                  flick_distance_reference_pixels=72, flick_release_ms=32)
        self.save_profile()
        result = self.prepare(vary="flick_lead_ms", values=(30, 50))
        for trial in result["trials"]:
            self.assertEqual(trial["parameters"]["flick_travel_ms"], 80)
            self.assertEqual(trial["parameters"]["flick_distance_reference_pixels"], 72)
            self.assertEqual(trial["parameters"]["flick_release_ms"], 32)
            argv = trial["native_prepare_argv"]
            self.assertEqual(argv[argv.index("--flick-lead-ms") + 1], str(trial["parameters"]["flick_lead_ms"]))

    def test_invalid_relationship_values_and_unchanged_only_rejected_before_output(self):
        for vary, values in (("flick_lead_ms", (60,)), ("flick_lead_ms", (True,)),
                             ("flick_travel_ms", (29,)), ("flick_release_ms", (0,)),
                             ("flick_distance_reference_pixels", (241,)),
                             ("flick_distance_reference_pixels", (float("nan"),)),
                             ("flick_distance_reference_pixels", (60, 60.0))):
            with self.subTest(vary=vary, values=values), self.assertRaises(ValueError):
                self.prepare(vary, values)
            self.assertFalse(self.output.exists())

    def test_source_chart_or_profile_mismatch_rejected_before_output(self):
        self.chart.write_bytes(b"changed synthetic chart")
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_no_overwrite_and_newline_arguments_rejected(self):
        self.output.mkdir()
        with self.assertRaises(FileExistsError):
            self.prepare()
        self.assertEqual(trials.ps_literal("x' $(no) `none`"), "'x'' $(no) `none`'")
        with self.assertRaises(ValueError):
            trials.ps_literal("first\nsecond")


if __name__ == "__main__":
    unittest.main()
