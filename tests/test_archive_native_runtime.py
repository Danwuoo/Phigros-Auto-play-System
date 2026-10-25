"""Archive boundary tests with synthetic files; no native processes/devices."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("archive_native_runtime", Path(__file__).resolve().parents[1] / "tools/archive_native_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class RuntimeArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo, self.bin = self.root / "repo", self.root / "build"
        self.repo.mkdir()
        self.bin.mkdir()
        for name in ("apps/main.cpp", "tools/helper.ps1", "assets/private.json", "CMakeLists.txt"):
            path = self.repo / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"synthetic")
        (self.repo / "tools/untracked-private.json").write_bytes(b"private")
        for name in runtime.EXECUTABLES | {"dependency.dll"}:
            (self.bin / name).write_bytes(b"synthetic binary")
        self.log = self.root / "ctest.log"
        self.log.write_bytes(b"synthetic evidence, not a real test result")
        subprocess.run(["git", "init", "-q", str(self.repo)], check=True)
        subprocess.run(["git", "-C", str(self.repo), "add", "apps", "tools/helper.ps1", "assets", "CMakeLists.txt"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-qm", "fixture"], check=True)
        self.output = self.root / "archive"

    def seal(self, **kwargs):
        return runtime.archive(self.repo, self.bin, kwargs.get("output", self.output), kwargs.get("evidence", [self.log]))

    def test_copies_pinned_runtime_sources_and_evidence_excludes_private(self):
        result = self.seal()
        names = {item["file"] for item in result["artifacts"]}
        self.assertIn("bin/chart_experiment.exe", names)
        self.assertIn("bin/dependency.dll", names)
        self.assertIn("source/tools/helper.ps1", names)
        self.assertIn("evidence/ctest.log", names)
        self.assertFalse(any("private" in name or "assets/" in name for name in names))
        for item in result["artifacts"]:
            self.assertEqual(runtime.digest(self.output / item["file"]), item["sha256"])
        self.assertEqual(result["game_test"], "NOT RUN")

    def test_existing_output_is_untouched(self):
        self.output.mkdir()
        sentinel = self.output / "original"
        sentinel.write_bytes(b"keep")
        with self.assertRaises(FileExistsError):
            self.seal()
        self.assertEqual(sentinel.read_bytes(), b"keep")

    def test_output_cannot_overlap_source_or_build(self):
        for output in (self.repo / "archive", self.bin / "archive"):
            with self.assertRaisesRegex(ValueError, "independent"):
                self.seal(output=output)
            self.assertFalse(output.exists())

    def test_missing_binary_duplicate_evidence_and_budget_rejected_before_write(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.seal(evidence=[self.log, self.log])
        with patch.object(runtime, "MAX_BYTES", 1):
            with self.assertRaisesRegex(ValueError, "budget"):
                self.seal()
        (self.bin / "chart_experiment.exe").unlink()
        with self.assertRaisesRegex(ValueError, "not a file"):
            self.seal()
        self.assertFalse(self.output.exists())

    def test_changed_input_retains_partial_without_manifest(self):
        original_copy = runtime.shutil.copyfile
        def changed(src, target):
            original_copy(src, target)
            Path(src).write_bytes(b"changed during copy")
        with patch.object(runtime.shutil, "copyfile", side_effect=changed):
            with self.assertRaisesRegex(ValueError, "changed"):
                self.seal()
        self.assertTrue(self.output.exists())
        self.assertFalse((self.output / "manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
