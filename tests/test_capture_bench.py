"""Window and failure tests for the continuous capture benchmark."""

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import threading
import time
import unittest
from unittest.mock import patch

from pas.cli import _window_event_counts, grpc_capture_bench
from pas.contracts import Frame
from pas.telemetry import Telemetry


class Source:
    def __init__(self, initial_delay=0, one_frame=False, pause_at=None, pause_s=0):
        self.endpoint = type("Endpoint", (), {"target": "127.0.0.1:1", "instance": "fake"})()
        self.initial_delay = initial_delay
        self.one_frame = one_frame
        self.pause_at, self.pause_s = pause_at, pause_s
        self.closed = threading.Event()
        self.sequence = 0
        self.source_gaps = 0

    def capture(self):
        if self.sequence == self.pause_at:
            self.closed.wait(self.pause_s)
        elif self.sequence == 0 and self.initial_delay:
            self.closed.wait(self.initial_delay)
        else:
            self.closed.wait(0.005 if not self.one_frame else 1)
        if self.closed.is_set():
            raise RuntimeError("closed")
        if self.one_frame and self.sequence:
            raise RuntimeError("one frame only")
        now = time.monotonic_ns()
        sequence = self.sequence
        self.sequence += 1
        if sequence == 2:
            self.source_gaps += 4  # Deliberately in warmup.
        return Frame(sequence, 1, 1, b"\0\0\0", now,
                     source_sequence=sequence), now

    def counters(self):
        return {"inactive_frames": 0, "invalid_frames": 0,
                "source_gaps": self.source_gaps, "relative_stale_drops": 0}

    def close(self):
        self.closed.set()


class BenchTests(unittest.TestCase):
    def test_event_counters_use_half_open_boundaries(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            events = [
                {"event": "capture_inactive", "monotonic_ns": 99},
                {"event": "capture_inactive", "monotonic_ns": 100},
                {"event": "capture_source_gap", "monotonic_ns": 150, "gap": 3},
                {"event": "capture", "published_ns": 150},
                {"event": "frame_consumed", "consume_ns": 199, "sequence_skip": 2},
                {"event": "capture_overwrite", "monotonic_ns": 200},
            ]
            path.write_text("\n".join(json.dumps(event) for event in events) + "\n")
            counts = _window_event_counts(str(path), 100, 200)
            self.assertEqual(counts["inactive_frames"], 1)
            self.assertEqual(counts["source_gaps"], 3)
            self.assertEqual(counts["latest_published"], 1)
            self.assertEqual(counts["consumer_reads"], 1)
            self.assertEqual(counts["consumer_skips"], 2)
            self.assertEqual(counts["latest_overwritten"], 0)

    def run_bench(self, source, path, **kwargs):
        factory = source if callable(source) else lambda *args: source
        with (patch("pas.cli.find_adb", return_value="adb"),
              patch("pas.cli.list_devices", return_value=[{"serial": "emulator-5554", "state": "device"}]),
              patch("pas.cli._grpc_source", side_effect=factory),
              patch("pas.cli.read_fixture_counter", side_effect=lambda frame, *args: frame.source_sequence)):
            return grpc_capture_bench("emulator-5554", kwargs.pop("duration_s", 0.08),
                                      kwargs.pop("warmup_s", 0.04), 0, str(path), **kwargs)

    def test_delayed_readiness_still_gets_full_warmup_and_measurement(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            result = self.run_bench(Source(initial_delay=0.08), path)
            self.assertEqual(result["status"], "complete")
            self.assertGreaterEqual(result["measurement_actual_duration_s"], 0.08)
            self.assertGreaterEqual((result["warmup_end_ns"] - result["ready_ns"]) / 1e9, 0.04)
            self.assertGreater(result["connecting_start_ns"], 0)
            self.assertGreaterEqual(result["window_counters_start"]["source_gaps"], 4)
            self.assertEqual(result["window_counters_delta"]["source_gaps"], 0)
            self.assertAlmostEqual(result["process_cpu_one_core_percent"],
                                   result["process_cpu_seconds"] /
                                   result["measurement_actual_duration_s"] * 100)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual([event["phase"] for event in events if event["event"] == "bench_phase"],
                             ["CONNECTING", "WARMUP", "MEASURING", "STOPPING"])

    def test_readiness_timeout_is_failure_not_zero_sample_success(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            with self.assertRaisesRegex(TimeoutError, "readiness"):
                self.run_bench(Source(initial_delay=0.2), path, ready_timeout_s=0.02)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual(events[-1]["event"], "bench_failed")
            self.assertEqual(events[-1]["phase"], "CONNECTING")

    def test_initialization_counts_toward_readiness_deadline(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"

            def delayed_source(*args):
                time.sleep(0.04)
                return Source()

            with self.assertRaisesRegex(TimeoutError, "initialization"):
                self.run_bench(delayed_source, path, ready_timeout_s=0.01)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual(events[-1]["event"], "bench_failed")

    def test_connection_failure_is_logged_and_fails(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"

            def failed_source(*args):
                raise RuntimeError("authentication failed")

            with self.assertRaisesRegex(RuntimeError, "authentication failed"):
                self.run_bench(failed_source, path)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual(events[-1]["event"], "bench_failed")
            self.assertEqual(events[-1]["phase"], "CONNECTING")

    def test_all_invalid_frames_fail_before_warmup(self):
        class InvalidSource(Source):
            def capture(self):
                raise ValueError("invalid screenshot payload length")

        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            with self.assertRaisesRegex(RuntimeError, "invalid screenshot"):
                self.run_bench(InvalidSource(), path)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual(events[-1]["event"], "bench_failed")
            self.assertEqual(events[-1]["phase"], "CONNECTING")

    def test_one_frame_is_not_dynamic_success(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            with self.assertRaises(RuntimeError):
                self.run_bench(Source(one_frame=True), path)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            summary = next(event for event in events if event["event"] == "summary")
            self.assertEqual(summary["status"], "incomplete_dynamic_unverified")
            self.assertIsNotNone(summary["capture_error"])
            self.assertEqual(events[-1]["event"], "bench_failed")

    def test_cpu_uses_only_two_measurement_boundary_snapshots(self):
        with TemporaryDirectory() as directory:
            with patch("pas.cli.time.process_time_ns", side_effect=[2_000_000_000, 2_010_000_000]) as cpu:
                result = self.run_bench(Source(initial_delay=0.04),
                                        Path(directory) / "capture.jsonl", warmup_s=0.03)
            self.assertEqual(cpu.call_count, 2)
            self.assertEqual(result["process_cpu_seconds"], 0.01)
            self.assertEqual(result["process_cpu_start_ns"], 2_000_000_000)
            self.assertEqual(result["process_cpu_end_ns"], 2_010_000_000)

    def test_measurement_stall_remains_in_wall_clock_denominator(self):
        with TemporaryDirectory() as directory:
            result = self.run_bench(Source(pause_at=5, pause_s=0.05),
                                    Path(directory) / "capture.jsonl",
                                    warmup_s=0.01, duration_s=0.12)
            self.assertGreaterEqual(result["measurement_actual_duration_s"], 0.12)
            self.assertGreater(result["longest_interarrival_ms"], 40)
            self.assertGreater(result["no_frame_gap_ms"]["max"], 40)

    def test_first_formal_skip_excludes_warmup_frames(self):
        class DelayedMeasurementTelemetry(Telemetry):
            def record(self, event, **fields):
                super().record(event, **fields)
                if event == "bench_phase" and fields.get("phase") == "MEASURING":
                    time.sleep(0.025)

        with TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            with patch("pas.cli.Telemetry", DelayedMeasurementTelemetry):
                result = self.run_bench(Source(), path, warmup_s=0.04, duration_s=0.1)
            events = [json.loads(line) for line in path.read_text().splitlines()]
            start = result["measurement_start_ns"]
            warmup = [e["frame_sequence"] for e in events
                      if e["event"] == "capture" and e["capture_complete_ns"] < start]
            first = next(e for e in events if e["event"] == "frame_consumed")
            self.assertGreater(len(warmup), 2)
            self.assertGreater(first["sequence_skip"], 0)
            self.assertEqual(first["sequence_skip"], first["frame_sequence"] - max(warmup) - 1)
            self.assertEqual(result["last_pre_window_sequence_at_start"], max(warmup))


if __name__ == "__main__":
    unittest.main()
