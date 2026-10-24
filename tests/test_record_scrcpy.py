"""Offline wrapper checks; never starts scrcpy, ADB, or an emulator."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location("record_scrcpy", TOOLS / "record_scrcpy.py")
recorder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recorder)


class ScrcpyRecorderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_observer_flags_and_paths_are_separate_list_arguments(self):
        args = recorder.command(self.root / "space dir/scrcpy.exe", self.root / "new output",
                                "emulator-5554", 180)
        for flag in ("--no-control", "--no-playback", "--no-window", "--require-audio",
                     "--audio-source=output", "--audio-codec=flac", "--video-codec=h264",
                     "--video-bit-rate=20M", "--max-fps=60", "--time-limit=180"):
            self.assertIn(flag, args)
        self.assertEqual(args[-1], "--record=" + str(self.root / "new output/capture.mkv"))

    def test_existing_output_is_rejected_before_any_process(self):
        output = self.root / "existing"
        output.mkdir()
        sentinel = output / "keep"
        sentinel.write_text("same")
        with mock.patch.object(recorder.subprocess, "run") as run:
            with self.assertRaises(FileExistsError):
                recorder.record(self.root / "absent.exe", output, "emulator-5554", 5, 1048576, 0)
        run.assert_not_called()
        self.assertEqual(sentinel.read_text(), "same")

    def test_video_only_leaves_host_audio_available(self):
        args = recorder.command(self.root / "scrcpy.exe", self.root / "out", "emulator-5554", 180, video_only=True)
        self.assertIn("--no-audio", args)
        self.assertIn("--no-control", args)
        self.assertFalse(any(arg.startswith("--audio-") or arg == "--require-audio" for arg in args))

    def test_file_and_free_disk_budgets_are_independent(self):
        (self.root / "capture.mkv").write_bytes(b"x" * 100)
        self.assertEqual(recorder.budget_reason(self.root, 100 + recorder.SUMMARY_RESERVE, 0), "file_budget")
        with mock.patch.object(recorder.shutil, "disk_usage", return_value=mock.Mock(free=10)):
            self.assertEqual(recorder.budget_reason(self.root, 1048576, 20), "free_disk_budget")

    def test_cli_duration_rejects_before_starting_process(self):
        with mock.patch.object(recorder, "record") as record, mock.patch("sys.stderr"):
            with self.assertRaises(SystemExit) as error:
                recorder.main([str(self.root / "new"), "--scrcpy", "unused.exe", "--duration", "181"])
        self.assertEqual(error.exception.code, 2)
        record.assert_not_called()

    def test_shutdown_escalates_with_bounded_waits(self):
        process = mock.Mock()
        process.poll.return_value = None
        process.wait.side_effect = [recorder.subprocess.TimeoutExpired("offline-fixture", 5), 0]
        recorder.stop_process(process)
        process.terminate.assert_called_once_with()
        process.kill.assert_called_once_with()
        self.assertEqual(process.wait.call_args_list, [mock.call(timeout=5), mock.call(timeout=5)])


if __name__ == "__main__":
    unittest.main()
