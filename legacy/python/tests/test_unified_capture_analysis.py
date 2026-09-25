"""Window and counter semantics for the standardized offline measurement report."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


spec=importlib.util.spec_from_file_location("unified_capture_bench",
    Path(__file__).resolve().parents[1]/"scripts"/"unified_capture_bench.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def capture(sequence,when):
    return {"event":"capture","frame_sequence":sequence,"capture_complete_ns":when,
            "pixels_ready_ns":when+10,"width":1280,"height":720,"source_rotation":1}


class UnifiedAnalysisTests(unittest.TestCase):
    def compute(self,events):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"capture.jsonl"
            path.write_text("".join(json.dumps(event)+"\n" for event in events),encoding="utf-8")
            return module.recompute(path)

    def test_distinct_capture_and_consume_windows_with_thread_join(self):
        events=[{"event":"bench_phase","phase":"MEASURING","monotonic_ns":1000},
                {"event":"bench_phase","phase":"STOPPING","monotonic_ns":3000},
                capture(0,900),capture(1,1000),capture(2,2000),capture(3,3000),
                {"event":"frame_consumed","frame_sequence":0,"consume_ns":950,"sequence_skip":99},
                {"event":"frame_consumed","frame_sequence":0,"consume_ns":1001,"sequence_skip":0},
                {"event":"frame_consumed","frame_sequence":1,"consume_ns":1100,"sequence_skip":0},
                {"event":"frame_consumed","frame_sequence":2,"consume_ns":2500,"sequence_skip":1},
                {"event":"frame_consumed","frame_sequence":3,"consume_ns":3000,"sequence_skip":99}]
        result=self.compute(events)
        self.assertEqual(result["capture_events"],2)
        self.assertEqual(result["consumer_events"],3)
        self.assertEqual(result["consumer_sequence_skips"],1)
        self.assertEqual(result["consumer_cross_boundary_frames"],1)
        self.assertEqual(result["arrival_interval_ms"]["n"],1)
        self.assertEqual(result["host_residency_ms"]["max"],500/1e6)

    def test_visible_counter_wrap_is_not_negative_source_rate(self):
        start=1_000_000_000
        events=[{"event":"bench_phase","phase":"MEASURING","monotonic_ns":start},
                {"event":"bench_phase","phase":"STOPPING","monotonic_ns":start+50_000_000}]
        for seq,counter in enumerate(((1<<24)-1,0,1)):
            when=start+seq*20_000_000
            events.extend([capture(seq,when),{"event":"fixture_counter","frame_sequence":seq,
                "capture_complete_ns":when,"counter":counter}])
        result=self.compute(events)
        self.assertTrue(result["fixture_counter_order_valid"])
        self.assertEqual(result["fixture_counter_span_hz"],50)
        self.assertEqual(result["fixture_decoded_fraction"],1)
        self.assertTrue(result["geometry_valid"])

    def test_missing_measurement_phase_is_not_silently_accepted(self):
        with self.assertRaises(KeyError):self.compute([capture(0,1000)])

    def test_regressing_counter_is_marked_invalid(self):
        events=[{"event":"bench_phase","phase":"MEASURING","monotonic_ns":1000},
                {"event":"bench_phase","phase":"STOPPING","monotonic_ns":3000}]
        for seq,counter in enumerate((10,9)):
            when=1000+seq*1000
            events.extend([capture(seq,when),{"event":"fixture_counter","frame_sequence":seq,
                "capture_complete_ns":when,"counter":counter}])
        result=self.compute(events)
        self.assertFalse(result["fixture_counter_order_valid"])
        self.assertIsNone(result["fixture_counter_span_hz"])


if __name__=="__main__":unittest.main()
