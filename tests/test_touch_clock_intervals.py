import dataclasses
import importlib.util
from pathlib import Path
import sys
import unittest

SPEC = importlib.util.spec_from_file_location(
    "touch_clock_intervals", Path(__file__).resolve().parents[1] / "tools/touch_clock_intervals.py")
clock = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = clock
SPEC.loader.exec_module(clock)


class ClockIntervalsTests(unittest.TestCase):
    def setUp(self):
        self.identity = clock.ClockAssociation("emulator-test", "12345678-1234-1234-1234-123456789abc", "synthetic-host-boot/QPC")

    def sample(self, sequence, android, before, after, association=None):
        return clock.ClockSample(sequence, android, before, after, association or self.identity)

    def event(self, time, association=None):
        return clock.AssociatedEvent(time, association or self.identity, "fixture-event-17")

    def test_inside_uses_outer_causal_bounds_without_assuming_rate(self):
        mapper = clock.OrderOnlyClockMap([
            self.sample(1, 100, 1000, 1100), self.sample(3, 200, 1_000_000, 1_000_100),
            self.sample(9, 300, 1_000_200, 1_000_300)])
        interval = mapper.map_event(self.event(101))
        self.assertEqual((interval.lower_ns, interval.upper_ns), (1000, 1_000_100))
        self.assertEqual(interval.support_sequences, (1, 3))
        self.assertFalse(interval.exact_sample)
        self.assertEqual(interval.width_ns, 999_100)
        self.assertEqual(interval.event_evidence_id, "fixture-event-17")
        self.assertIn("no equal-rate", interval.scope)
        self.assertFalse(hasattr(interval, "midpoint_ns"))
        later = mapper.map_event(self.event(250))
        self.assertEqual((later.lower_ns, later.upper_ns), (1_000_000, 1_000_300))

    def test_exact_sample_uses_own_bracket_including_range_endpoints(self):
        mapper = clock.OrderOnlyClockMap([self.sample(2, 0, 50, 90), self.sample(4, 10, 100, 200)])
        for android, expected, sequence in [(0, (50, 90), 2), (10, (100, 200), 4)]:
            value = mapper.map_event(self.event(android))
            self.assertTrue(value.exact_sample)
            self.assertEqual((value.lower_ns, value.upper_ns), expected)
            self.assertEqual(value.support_sequences, (sequence,))
        one = clock.OrderOnlyClockMap([self.sample(1, 7, 300, 300)])
        self.assertEqual(one.map_event(self.event(7)).width_ns, 0)

    def test_no_extrapolation_or_empty_map(self):
        mapper = clock.OrderOnlyClockMap([self.sample(1, 10, 100, 200), self.sample(2, 20, 300, 400)])
        for time in [0, 9, 21, clock.MAX_NS]:
            with self.subTest(time=time), self.assertRaises(ValueError):
                mapper.map_event(self.event(time))
        with self.assertRaises(ValueError):
            clock.OrderOnlyClockMap([])

    def test_valid_overlapping_brackets_are_not_narrowed_to_reply_request_gap(self):
        mapper = clock.OrderOnlyClockMap([self.sample(1, 10, 100, 500), self.sample(2, 20, 200, 600)])
        interval = mapper.map_event(self.event(15))
        self.assertEqual((interval.lower_ns, interval.upper_ns), (100, 600))

    def test_rollback_duplicate_or_invalid_sample_rejects_entire_input(self):
        first = self.sample(1, 10, 100, 200)
        for second in [self.sample(1, 20, 300, 400), self.sample(2, 10, 300, 400),
                       self.sample(2, 9, 300, 400), self.sample(2, 20, 99, 400),
                       self.sample(2, 20, 150, 199), None]:
            with self.subTest(second=second), self.assertRaises(ValueError):
                clock.OrderOnlyClockMap([first, second])

    def test_association_required_and_mixed_device_boot_host_rejected(self):
        identities = [dataclasses.replace(self.identity, device_id="other-device"),
                      dataclasses.replace(self.identity, boot_id="12345678-1234-1234-1234-123456789abd"),
                      dataclasses.replace(self.identity, host_clock_id="other-host-boot/QPC")]
        mapper = clock.OrderOnlyClockMap([self.sample(1, 10, 100, 200)])
        for identity in identities:
            with self.assertRaises(ValueError):
                clock.OrderOnlyClockMap([self.sample(1, 10, 100, 200), self.sample(2, 20, 300, 400, identity)])
            with self.assertRaises(ValueError):
                mapper.map_event(self.event(10, identity))
        for factory in [lambda: clock.ClockSample(1, 1, 1, 1, None),
                        lambda: clock.AssociatedEvent(1, None, "e"),
                        lambda: clock.ClockAssociation("", self.identity.boot_id, "host"),
                        lambda: clock.ClockAssociation("dev", "", "host"),
                        lambda: clock.ClockAssociation("dev", self.identity.boot_id, "")]:
            with self.assertRaises(ValueError):
                factory()

    def test_integer_fields_reject_boolean_float_negative_and_overflow(self):
        for bad in [True, False, 1.0, "1", None, -1, clock.MAX_NS + 1]:
            for field in ["sequence", "android_monotonic_ns", "host_before_ns", "host_after_ns"]:
                valid = dict(sequence=1, android_monotonic_ns=10, host_before_ns=100, host_after_ns=200, association=self.identity)
                valid[field] = bad
                with self.subTest(field=field, bad=bad), self.assertRaises(ValueError):
                    clock.ClockSample(**valid)
            with self.assertRaises(ValueError):
                self.event(bad)
        with self.assertRaises(ValueError):
            self.sample(0, 10, 100, 200)
        with self.assertRaises(ValueError):
            self.sample(1, 10, 200, 100)
        for evidence in ["", "x" * 513, 123]:
            with self.assertRaises(ValueError):
                clock.AssociatedEvent(10, self.identity, evidence)

    def test_bounded_input_no_silent_discard_and_immutable_records(self):
        def samples(count):
            for sequence in range(1, count + 1):
                yield self.sample(sequence, sequence * 10, sequence * 100, sequence * 100 + 50)
        mapper = clock.OrderOnlyClockMap(samples(clock.MAX_SAMPLES))
        self.assertEqual(mapper.sample_count, clock.MAX_SAMPLES)
        self.assertEqual(mapper.map_event(self.event(clock.MAX_SAMPLES * 10)).support_sequences, (clock.MAX_SAMPLES,))
        with self.assertRaises(ValueError):
            clock.OrderOnlyClockMap(samples(clock.MAX_SAMPLES + 1))
        with self.assertRaises(dataclasses.FrozenInstanceError):
            mapper.map_event(self.event(10)).lower_ns = 1
        with self.assertRaises(AttributeError):
            mapper.association = dataclasses.replace(self.identity, device_id="different")


if __name__ == "__main__":
    unittest.main()
