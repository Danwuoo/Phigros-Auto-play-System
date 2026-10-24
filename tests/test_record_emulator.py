"""Offline recorder contracts; never connects to an emulator."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import threading
import time
from types import SimpleNamespace as NS
import unittest
import wave
import zlib

from PIL import Image

SPEC = importlib.util.spec_from_file_location(
    "record_emulator", Path(__file__).parents[1] / "tools/record_emulator.py")
recorder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recorder)


def screenshot():
    buffer = io.BytesIO()
    Image.new("RGB", (2, 1), "red").save(buffer, format="PNG")
    return NS(format=NS(width=2, height=1, format=0,
                        rotation=NS(rotation=1, xAxis=0, yAxis=0, zAxis=90)),
              width=0, height=0, image=buffer.getvalue(), seq=7, timestampUs=1234567890123456)


def audio():
    return NS(format=NS(samplingRate=48000, channels=1, format=1),
              audio=b"\x01\x00\x02\x00" * 2, timestamp=1234567890123000)


class Cancelled(Exception):
    def code(self):
        return NS(name="CANCELLED")


class Stream:
    def __init__(self, packets, fail=False):
        self.packets, self.fail, self.cancelled = packets, fail, threading.Event()

    def __iter__(self):
        yield from self.packets
        if self.fail:
            raise OSError("sensitive diagnostic must not be saved")
        self.cancelled.wait()
        raise Cancelled()

    def cancel(self):
        self.cancelled.set()


class RecorderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "new-recording"

    def recording(self):
        recording = recorder.Recording(self.path, 1048576, 10, 0)
        self.addCleanup(recording.close)
        return recording

    def test_timestamps_png_and_pcm_offsets_are_preserved(self):
        recording = self.recording()
        packet = screenshot()
        recording.frame(packet, 101)
        recording.audio(audio(), 102)
        recording.audio(audio(), 103)
        recording.close()
        frame = json.loads((self.path / "frame-index.jsonl").read_text())
        rows = [json.loads(row) for row in (self.path / "audio-index.jsonl").read_text().splitlines()]
        self.assertEqual((frame["source_timestamp_us"], frame["source_sequence"], frame["arrival_ns"]),
                         (packet.timestampUs, 7, 101))
        self.assertEqual((self.path / frame["file"]).read_bytes(), packet.image)
        self.assertEqual((rows[0]["source_timestamp_us"], rows[0]["arrival_ns"]),
                         (audio().timestamp, 102))
        self.assertEqual([row["sample_frame_offset"] for row in rows], [0, 2])
        with wave.open(str(self.path / "audio.wav")) as wav:
            self.assertEqual((wav.getframerate(), wav.getnchannels(), wav.getsampwidth(), wav.getnframes()),
                             (48000, 2, 2, 4))

    def test_existing_output_is_refused_without_changes(self):
        self.path.mkdir()
        (self.path / "sentinel").write_text("keep")
        with self.assertRaises(FileExistsError):
            recorder.Recording(self.path, 1048576, 10, 0)
        self.assertEqual((self.path / "sentinel").read_text(), "keep")

    def test_frame_budget_refuses_additional_frame(self):
        recording = self.recording()
        recording.budget.frame_limit = 1
        recording.frame(screenshot(), 1)
        with self.assertRaises(recorder.BudgetExceeded):
            recording.frame(screenshot(), 2)
        self.assertEqual(recording.frames, 1)
        self.assertFalse((self.path / "frames/frame-000001.png").exists())

    def test_byte_budget_refuses_payload_before_writing(self):
        recording = self.recording()
        recording.budget.limit = recording.budget.bytes + 1
        with self.assertRaises(recorder.BudgetExceeded):
            recording.audio(audio(), 1)
        self.assertEqual(recording.sample_frames, 0)

    def test_worker_fault_cancels_both_streams_and_marks_failure(self):
        recording = self.recording()
        video, sound = Stream([], fail=True), Stream([])
        summary = recorder.capture(recording, video, sound, 0.05)
        self.assertEqual(summary["status"], "failed")
        self.assertTrue(video.cancelled.is_set() and sound.cancelled.is_set())
        self.assertEqual(summary["faults"][0]["code"], "OSError")
        self.assertNotIn("sensitive", (self.path / "summary.json").read_text())

    def test_deadline_closes_wave_and_keeps_clock_domains_distinct(self):
        recording = self.recording()
        video, sound = Stream([screenshot()]), Stream([audio()])
        summary = recorder.capture(recording, video, sound, 0.05)
        self.assertEqual(summary["status"], "recorded")
        self.assertEqual((summary["frames"], summary["audio_sample_frames"]), (1, 2))
        self.assertIn("separate clock domain", summary["arrival_timestamps"])
        self.assertIn("unverified", summary["coverage"])
        self.assertEqual(len(summary["clock_anchors"]), 2)

    def test_rgb888_bottom_up_pixels_are_encoded_without_new_frames(self):
        recording = self.recording()
        packet = screenshot()
        packet.format.format, packet.format.width, packet.format.height = 2, 1, 2
        recording.rgb_row_order = "bottom-up"
        packet.image = bytes([0, 0, 255, 255, 0, 0])  # Bottom blue, top red.
        recording.frame(packet, 99)
        recording.close()
        with Image.open(self.path / "frames/frame-000000.png") as image:
            self.assertEqual((image.getpixel((0, 0)), image.getpixel((0, 1))),
                             ((255, 0, 0), (0, 0, 255)))
        self.assertEqual(recording.frames, 1)

    def test_missing_audio_samples_never_claims_complete(self):
        recording = self.recording()
        summary = recorder.capture(recording, Stream([screenshot()]), Stream([]), 0.05)
        self.assertEqual(summary["status"], "incomplete")

    def test_discovery_auth_requires_exact_port_and_nonempty_token(self):
        discovery = Path(self.temp.name) / "pid_1.ini"
        discovery.write_text("grpc.port=8554\ngrpc.token=synthetic-test-token\n")
        self.assertEqual(recorder.read_token(discovery_file=discovery), "synthetic-test-token")
        discovery.write_text("grpc.port=8555\ngrpc.token=synthetic-test-token\n")
        with self.assertRaises(ValueError):
            recorder.read_token(discovery_file=discovery)

    def test_crc_correct_but_invalid_deflate_png_is_rejected(self):
        recording = self.recording()
        packet = screenshot()
        original, rebuilt, offset = packet.image, bytearray(packet.image[:8]), 8
        while offset < len(original):
            length = int.from_bytes(original[offset:offset + 4], "big")
            kind = original[offset + 4:offset + 8]
            data = b"invalid zlib data" if kind == b"IDAT" else original[offset + 8:offset + 8 + length]
            rebuilt.extend(len(data).to_bytes(4, "big") + kind + data +
                           zlib.crc32(kind + data).to_bytes(4, "big"))
            offset += length + 12
        packet.image = bytes(rebuilt)
        with Image.open(io.BytesIO(packet.image)) as image:
            image.verify()  # CRC verification alone accepts this invalid image.
        with self.assertRaises(OSError):
            recording.frame(packet, 1)
        self.assertEqual(recording.frames, 0)

    def test_cancel_error_still_cancels_other_stream_and_closes_files(self):
        class CancelError(Stream):
            def cancel(self):
                super().cancel()
                raise OSError("do not save this diagnostic")
        recording = self.recording()
        video, sound = CancelError([screenshot()]), Stream([audio()])
        summary = recorder.capture(recording, video, sound, 0.05)
        self.assertEqual(summary["status"], "failed")
        self.assertTrue(summary["files_closed"] and sound.cancelled.is_set())
        self.assertIn({"stream": "cancel", "code": "OSError"}, summary["faults"])

    def test_stalled_stream_join_is_bounded_and_does_not_close_active_writer(self):
        class Stalled(Stream):
            def __init__(self):
                super().__init__([])
                self.release = threading.Event()

            def __iter__(self):
                self.release.wait()
                return
                yield  # This is an intentionally stalled iterator.
        recording = self.recording()
        video, sound = Stream([screenshot()]), Stalled()
        started = time.monotonic()
        try:
            summary = recorder.capture(recording, video, sound, 0.02, shutdown_timeout=0.03)
            self.assertLess(time.monotonic() - started, 0.5)
            self.assertEqual(summary["status"], "failed")
            self.assertFalse(summary["files_closed"])
            self.assertFalse(recording.audio_index.closed)
        finally:
            sound.release.set()

    def test_audio_sparse_pcm_and_source_sequence_gaps_are_reported(self):
        recording = self.recording()
        recording.video_timing.add(1000000, 100, sequence=0)
        recording.video_timing.add(1100000, 100000100, sequence=3)
        recording.audio_timing.add(1000000, 100, samples=2)
        recording.audio_timing.add(2000000, 1000000100, samples=2)
        recording.frames, recording.sample_frames = 2, 4
        summary = recorder.capture(recording, Stream([]), Stream([]), 0.01)
        self.assertEqual(summary["video_timing"]["sequence_gaps"], 2)
        self.assertLess(summary["audio_pcm_source_span_ratio"], 0.001)
        self.assertTrue(any("PCM sample duration" in warning for warning in summary["quality_warnings"]))


if __name__ == "__main__":
    unittest.main()
