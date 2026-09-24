"""Deterministic late-warmup delivery and deliberately slow CPU sampling."""

import json
import os
from pathlib import Path
from tempfile import TemporaryDirectory
import threading
import time
import unittest
from unittest.mock import patch

from pas.cli import _cpu_sample_window, process_capture_bench
from pas.capture_process import ProcessCaptureConfig
from pas.contracts import Frame
from pas.telemetry import Telemetry


class BoundarySource:
    def __init__(self, formal_started, skipped=0):
        self.formal_started = formal_started
        self.skipped = skipped
        self.closed = threading.Event()
        self.sequence = 0
        self.process = type("Process", (), {"pid": os.getpid()})()

    def capture(self):
        sequence = self.sequence
        self.sequence += 1
        timestamp = time.monotonic_ns()
        if sequence == 1:
            # Capture is before the boundary; delivery is released by the
            # benchmark's MEASURING event, not a timing-dependent sleep.
            if not self.formal_started.wait(3):
                raise TimeoutError("missing measurement phase")
        elif sequence >= 3:
            self.closed.wait(3)
            raise RuntimeError("source closed")
        actual_sequence = sequence + (self.skipped if sequence >= 2 else 0)
        return Frame(actual_sequence, 1, 1, b"abc", timestamp), timestamp

    def counters(self):
        return {"ipc_published": self.sequence}

    def close(self):
        self.closed.set()
        self.formal_started.set()


class ProcessBenchBoundaryTests(unittest.TestCase):
    def run_boundary(self, skipped=0, child_sample=None, parent_sample=None):
        formal_started = threading.Event()
        source = BoundarySource(formal_started, skipped)

        class BoundaryTelemetry(Telemetry):
            def record(self, event, **fields):
                super().record(event, **fields)
                if event == "bench_phase" and fields["phase"] == "MEASURING":
                    formal_started.set()

        with TemporaryDirectory() as directory:
            log = Path(directory) / "capture.jsonl"
            with (patch("pas.cli.ProcessCaptureSource", return_value=source),
                  patch("pas.cli.Telemetry", BoundaryTelemetry),
                  patch("pas.cli._child_cpu_seconds", side_effect=child_sample or (lambda _: 0)),
                  patch("pas.cli._child_rss_bytes", return_value=0),
                  patch("pas.cli.time.process_time_ns", side_effect=parent_sample or (lambda: 0))):
                summary = process_capture_bench(ProcessCaptureConfig(kind="fake"),
                    duration_s=0.06, warmup_s=0.02, consumer_delay_ms=0, log=str(log))
            events = [json.loads(line) for line in log.read_text().splitlines()]
        return summary, events

    def test_delayed_warmup_is_excluded_but_formal_skips_are_preserved(self):
        for skipped in (0, 2):
            with self.subTest(skipped=skipped):
                summary, events = self.run_boundary(skipped)
                self.assertEqual(summary["consumer_skips"], skipped)
                consumed = [e for e in events if e["event"] == "frame_consumed"]
                self.assertEqual(len(consumed), 1)
                self.assertEqual(consumed[0]["sequence_skip"], skipped)
                self.assertEqual(consumed[0]["frame_sequence"], 2 + skipped)
                warm = [e for e in events if e["event"] == "capture" and
                        e["capture_complete_ns"] < summary["measurement_start_ns"]]
                self.assertEqual([e["frame_sequence"] for e in warm], [0, 1])

    def test_slow_child_sampling_is_bracketed_and_excluded_from_parent_cpu(self):
        cpu_ns = 0
        child_reads = 0

        def child_sample(_pid):
            nonlocal cpu_ns, child_reads
            cpu_ns += 80_000_000  # injected cost of the child-counter accessor
            child_reads += 1
            time.sleep(0.04)
            return child_reads * 0.01

        summary, events = self.run_boundary(child_sample=child_sample, parent_sample=lambda: cpu_ns)
        self.assertEqual(summary["parent_cpu_seconds"], 0)
        self.assertGreaterEqual(summary["snapshot_start_offset_ms"], 40)
        self.assertGreaterEqual(summary["snapshot_end_offset_ms"], 40)
        window = summary["resource_windows"]["child_cpu"]
        for sample in (window["start"], window["end"]):
            self.assertGreaterEqual(sample["after_ns"] - sample["before_ns"], 40_000_000)
        self.assertGreater(window["duration_s"], summary["measurement_actual_duration_s"])
        self.assertAlmostEqual(summary["child_cpu_one_core_percent"],
                               summary["child_cpu_seconds"] / window["duration_s"] * 100)
        phases = {e["phase"]: e for e in events if e["event"] == "bench_phase"}
        self.assertEqual(phases["MEASURING"]["resource_samples"]["child_cpu"], window["start"])
        self.assertEqual(phases["STOPPING"]["resource_samples"]["child_cpu"], window["end"])

    def test_unknown_cpu_is_unknown_and_duration_bounds_are_explicit(self):
        result = _cpu_sample_window(
            {"before_ns": 10, "after_ns": 30, "value": None},
            {"before_ns": 110, "after_ns": 150, "value": None}, 1, 40, 100)
        self.assertIsNone(result["cpu_seconds"])
        self.assertIsNone(result["one_core_percent"])
        self.assertAlmostEqual(result["duration_min_s"], 80 / 1e9)
        self.assertAlmostEqual(result["duration_max_s"], 140 / 1e9)
        self.assertAlmostEqual(result["duration_s"], 110 / 1e9)


if __name__ == "__main__":
    unittest.main()
