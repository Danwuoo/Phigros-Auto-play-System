import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import song_catalog as catalog
import inspect_catalog_native as native


def bytes_for(version):
    return catalog.json_bytes(dict(formatVersion=version, offset=0, judgeLineList=[dict(
        bpm=120, notesAbove=[dict(type=4, time=64, holdTime=0)], notesBelow=[])]))


class NativeFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "曲庫 with spaces"
        self.root.mkdir()
        self.config = dict(schema=catalog.SOURCE_SCHEMA, repository="Example/Resource", version="4.0.1.2",
                           commits=dict(info="1" * 40, chart="2" * 40, music="3" * 40),
                           expected=dict(song_count=1, IN=1, AT=1, variant_count=0, audio_count=1))
        self.raw = {
            "info": {"info.tsv": "歌曲.Composer\t歌曲\tComposer\tArtist\tEZ\tHD\tIN\tAT\n".encode(),
                     "difficulty.tsv": "歌曲.Composer\t1.0\t6.0\t12.0\t16.0\n".encode(), "version.txt": b"4.0.1.2\n"},
            "chart": {"歌曲.Composer.0/IN.json": bytes_for(1), "歌曲.Composer.0/AT.json": bytes_for(3)},
            "music": {"歌曲.Composer.ogg": b"synthetic audio"}}
        trees = {branch: dict(sha="4" * 40, truncated=False, tree=[dict(path=path, type="blob", mode="100644",
                 sha=catalog.git_blob_sha(raw), size=len(raw)) for path, raw in files.items()]) for branch, files in self.raw.items()}
        self.lock = catalog.build_lock(self.config, trees["info"], trees["chart"], trees["music"],
                                       self.raw["info"]["info.tsv"], self.raw["info"]["difficulty.tsv"], self.raw["info"]["version.txt"])
        for target in self.lock["targets"]:
            catalog.write_atomic(self.root, target["file"], self.raw[target["branch"]][target["source_path"]])
        catalog.write_atomic(self.root, "manifests/source-lock.json", catalog.json_bytes(self.lock))
        self.library = catalog.build_library(self.root, self.lock)
        self.library_path = self.root / "library.json"
        self.library_path.write_bytes(catalog.json_bytes(self.library))
        self.inspector = self.root / "test-inspector"
        self.inspector.write_bytes(b"synthetic pinned executable identity")
        self.output = self.root / "manifests/native-import-report.json"
        self.calls = []

    def runner(self, command, timeout, scratch):
        self.calls.append(command)
        raw = Path(command[1]).read_bytes()
        chart = json.loads(raw)
        if chart["formatVersion"] == 1:
            return dict(returncode=1, stdout="", stderr="chart-inspect: /formatVersion: only version 3 is supported\n", failure=None)
        return dict(returncode=0, stdout=json.dumps(dict(format_version=3, chart_sha256=command[2],
                    offset_seconds=0, lines=1, notes=1, events=0,
                    types=dict(tap=0, drag=0, hold=0, flick=1), first_note_ns=1000000000, last_note_end_ns=1000000000)),
                    stderr="", failure=None)

    def audit(self, **kwargs):
        return native.audit(self.library_path, self.inspector, self.output, runner=kwargs.pop("runner", self.runner), **kwargs)


class NativeAuditTests(NativeFixture):
    def test_real_sha_arguments_and_expected_v1_refusal_separate(self):
        original_library = self.library_path.read_bytes()
        result = self.audit(jobs=2)
        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["counts"]["passed"], 1)
        self.assertEqual(result["counts"]["unsupported_format"], 1)
        self.assertFalse(result["all_passed"])
        self.assertFalse(result["gameplay_qualified"])
        self.assertEqual(len(self.calls), 2)
        for command in self.calls:
            self.assertEqual(len(command), 3)
            self.assertEqual(command[2], hashlib.sha256(Path(command[1]).read_bytes()).hexdigest())
        self.assertEqual(self.library_path.read_bytes(), original_library)
        self.assertEqual(result["inspector"]["sha256"], hashlib.sha256(self.inspector.read_bytes()).hexdigest())

    def test_new_report_required_and_library_protected(self):
        self.output.write_bytes(b"preserve me")
        with self.assertRaisesRegex(ValueError, "new independent"):
            self.audit()
        self.assertEqual(self.output.read_bytes(), b"preserve me")
        with self.assertRaisesRegex(ValueError, "new independent"):
            native.audit(self.library_path, self.inspector, self.library_path, replace=True, runner=self.runner)

    def test_source_asset_cannot_be_report(self):
        chart = self.root / self.library["charts"][0]["chart"]["file"]
        before = chart.read_bytes()
        with self.assertRaisesRegex(ValueError, "cannot overwrite"):
            native.audit(self.library_path, self.inspector, chart, replace=True, runner=self.runner)
        self.assertEqual(chart.read_bytes(), before)

    def test_parent_path_alias_cannot_overwrite_library(self):
        alias = self.root / "manifests" / ".." / "library.json"
        original = self.library_path.read_bytes()
        with self.assertRaisesRegex(ValueError, "new independent"):
            native.audit(self.library_path, self.inspector, alias, replace=True, runner=self.runner)
        self.assertEqual(self.library_path.read_bytes(), original)

    def test_source_lock_mutation_refused_before_launch(self):
        path = self.root / "manifests/source-lock.json"
        path.write_bytes(path.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "fingerprint differs"):
            self.audit()
        self.assertEqual(self.calls, [])

    def test_library_identity_mismatch_refused_before_launch(self):
        self.library["charts"][0]["difficulty"] = "EZ"
        self.library_path.write_bytes(catalog.json_bytes(self.library))
        with self.assertRaisesRegex(ValueError, "identity differs"):
            self.audit()
        self.assertEqual(self.calls, [])

    def test_changed_chart_gets_integrity_error_without_inspection(self):
        target = self.root / self.library["charts"][0]["chart"]["file"]
        target.write_bytes(b"changed")
        result = self.audit()
        self.assertEqual(result["status"], "incomplete")
        self.assertEqual(result["counts"]["integrity_error"], 1)
        self.assertEqual(len(self.calls), 1)

    def test_lfs_pointer_is_not_sent_to_inspector(self):
        target = self.root / self.library["charts"][0]["chart"]["file"]
        target.write_bytes(catalog.LFS_HEADER + b"oid sha256:" + b"a" * 64 + b"\nsize 100\n")
        result = self.audit()
        self.assertEqual(result["counts"]["integrity_error"], 1)
        self.assertEqual(len(self.calls), 1)

    def test_timeout_and_invalid_summary_are_execution_failures(self):
        def runner(command, timeout, scratch):
            if Path(command[1]).name == "IN.json":
                return dict(returncode=-9, stdout="", stderr="", failure="timeout")
            return dict(returncode=0, stdout="{\"chart_sha256\":\"wrong\"}", stderr="", failure=None)
        result = self.audit(runner=runner)
        self.assertEqual(result["status"], "incomplete")
        self.assertEqual(result["counts"]["timeout"], 1)
        self.assertEqual(result["counts"]["inspector_error"], 1)

    def test_semantic_rejection_is_not_format_rejection(self):
        def runner(command, timeout, scratch):
            return dict(returncode=1, stdout="", stderr="chart-inspect: /judgeLineList/0: interval reversed", failure=None)
        result = self.audit(runner=runner)
        self.assertEqual(result["counts"]["rejected"], 2)
        self.assertEqual(result["counts"]["unsupported_format"], 0)
        self.assertEqual(result["status"], "complete")

    def test_cannot_open_file_is_inspector_error(self):
        result = self.audit(runner=lambda c, t, s: dict(returncode=1, stdout="", stderr="chart-inspect: cannot open chart", failure=None))
        self.assertEqual(result["counts"]["inspector_error"], 2)

    def test_mutation_during_inspection_invalidates_source(self):
        def runner(command, timeout, scratch):
            result = self.runner(command, timeout, scratch)
            Path(command[1]).write_bytes(b"mutated during inspection")
            return result
        result = self.audit(runner=runner)
        self.assertEqual(result["counts"]["integrity_error"], 2)

    def test_binary_mutation_invalidates_whole_audit(self):
        def runner(command, timeout, scratch):
            result = self.runner(command, timeout, scratch)
            self.inspector.write_bytes(b"different executable")
            return result
        result = self.audit(runner=runner)
        self.assertEqual(result["status"], "incomplete")
        self.assertIn("inspector_changed", result["input_changes"])
        self.assertFalse(result["all_passed"])

    def test_report_replace_requires_explicit_flag(self):
        self.audit()
        result = self.audit(replace=True)
        self.assertEqual(result["total"], 2)


class SubprocessBudgetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)

    def test_subprocess_unicode_and_spaces_use_argument_list(self):
        result = native.run_bounded([sys.executable, "-c", "import sys; print(sys.argv[1])", "曲名 with spaces"], 3, self.temp.name)
        self.assertEqual(result["returncode"], 0)
        self.assertIsNone(result["failure"])
        self.assertEqual(result["stdout"].strip(), "曲名 with spaces")

    def test_subprocess_timeout_kills_and_reaps(self):
        result = native.run_bounded([sys.executable, "-c", "import time; time.sleep(20)"], 0.08, self.temp.name)
        self.assertEqual(result["failure"], "timeout")
        self.assertIsInstance(result["returncode"], int)
        self.assertEqual(list(Path(self.temp.name).iterdir()), [])

    def test_subprocess_output_limit(self):
        result = native.run_bounded([sys.executable, "-c", "import sys; sys.stdout.write('x'*20000)"], 3, self.temp.name, output_limit=1000)
        self.assertEqual(result["failure"], "output_limit")
        self.assertLessEqual(len(result["stdout"]), 1000)
        self.assertEqual(list(Path(self.temp.name).iterdir()), [])


if __name__ == "__main__":
    unittest.main()
