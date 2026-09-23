import binascii
import struct
import time
import unittest
import zlib

from pas.adb import decode_png_rgb
from pas.clock import VirtualClock
from pas.capture import CaptureWorker
from pas.contracts import Frame, HitIntent, TouchCommand
from pas.input import CoordinateTransform, FakeTouchBackend
from pas.latest import LatestFrame
from pas.scheduler import Scheduler
from pas.session import SessionController
from pas.synthetic import run_synthetic
from pas.telemetry import Telemetry


def frame(sequence: int) -> Frame:
    return Frame(sequence, 1, 1, b"\0\0\0", sequence)


def intent(key: str, deadline: int, frame_sequence: int = 1) -> HitIntent:
    return HitIntent(key, 1, frame_sequence, 10, 10, deadline, "test", 1.0)


class PipelineTests(unittest.TestCase):
    def test_capture_worker_drops_old_frames_with_slow_consumer(self):
        class FastSource:
            sequence = 0

            def capture(self):
                time.sleep(0.001)
                now = time.monotonic_ns()
                result = Frame(self.sequence, 1, 1, b"\0\0\0", now)
                self.sequence += 1
                return result, now

        from pas.clock import HostClock
        worker = CaptureWorker(FastSource(), HostClock(), Telemetry())
        worker.start()
        first = worker.latest.read_after(-1, 1)
        self.assertIsNotNone(first)
        time.sleep(0.03)
        second = worker.latest.read_after(first.sequence, 1)
        worker.stop()
        self.assertGreater(second.sequence - first.sequence, 1)
        self.assertGreater(worker.latest.overwritten, 0)

    def test_session_launches_only_after_capture_ready(self):
        from pas.clock import HostClock
        clock = HostClock()
        log = Telemetry()

        class Source:
            sequence = 0

            def capture(self):
                time.sleep(0.001)
                now = clock.now_ns()
                result = Frame(self.sequence, 1, 1, b"\0\0\0", now)
                self.sequence += 1
                return result, now

        worker = CaptureWorker(Source(), clock, log)

        class Launcher:
            def launch(self, package):
                self.was_ready = worker.latest.published >= 3
                now = clock.now_ns()
                return now, now

        launcher = Launcher()
        session = SessionController(worker, launcher, clock, log)
        session.start("example.game")
        self.assertTrue(launcher.was_ready)
        self.assertEqual(session.state, "NAVIGATING")
        worker.stop()
        time.sleep(0.01)
        with self.assertRaises(RuntimeError):
            session.monitor_for(0.02, max_frame_age_s=0.005)
        self.assertEqual(session.state, "ERROR")
        session.stop()
        self.assertEqual(session.state, "STOPPED")

    def test_latest_frame_overwrites_unconsumed_frames(self):
        buffer = LatestFrame()
        buffer.publish(frame(0))
        buffer.publish(frame(1))
        buffer.publish(frame(2))
        self.assertEqual(buffer.overwritten, 2)
        self.assertEqual(buffer.read_after(-1, 0).sequence, 2)
        buffer.publish(frame(3))
        self.assertEqual(buffer.overwritten, 2)
        self.assertEqual(buffer.read_after(2, 0).sequence, 3)
        self.assertIsNone(buffer.read_after(3, 0))

    def test_scheduler_replaces_prediction_and_rejects_late_dispatch(self):
        clock = VirtualClock(100_000_000)
        backend = FakeTouchBackend(clock)
        log = Telemetry()
        scheduler = Scheduler(clock, backend, log, max_late_ns=5_000_000)
        scheduler.submit(intent("a", 130_000_000, 1))
        scheduler.submit(intent("a", 140_000_000, 2))
        clock.sleep_until_ns(140_000_000)
        scheduler.run_due()
        self.assertEqual(len(backend.receipts), 1)
        self.assertEqual(backend.receipts[0].command.source_frame_sequence, 2)
        self.assertFalse(scheduler.submit(intent("a", 150_000_000)))
        scheduler.run_next()
        self.assertFalse(backend.contacts)
        scheduler.submit(intent("b", 160_000_000))
        clock.sleep_until_ns(170_000_000)
        scheduler.run_due()
        self.assertEqual(len(backend.receipts), 2)
        self.assertIn("late_at_dispatch", [event.get("reason") for event in log.events])

    def test_independent_contacts_move_release_and_cancel(self):
        clock = VirtualClock()
        backend = FakeTouchBackend(clock)
        for cid in (1, 2):
            self.assertTrue(backend.inject(TouchCommand(str(cid), cid, "down", cid, 1, 0, 0)).success)
        backend.inject(TouchCommand("1", 1, "move", 8, 9, 1, 0))
        self.assertEqual(backend.contacts, {1: (8, 9), 2: (2, 1)})
        backend.inject(TouchCommand("2", 2, "up", 2, 1, 2, 0))
        self.assertEqual(list(backend.contacts), [1])
        backend.release_all()
        self.assertFalse(backend.contacts)

    def test_coordinate_transform_rotation_crop_and_black_border(self):
        transform = CoordinateTransform((10, 20, 100, 200), (5, 7, 200, 100), 90)
        self.assertEqual(transform.map(10, 20), (205, 7))
        self.assertEqual(transform.map(110, 220), (5, 107))
        with self.assertRaises(ValueError):
            transform.map(0, 0)

    def test_synthetic_closed_loop_observes_effect(self):
        result = run_synthetic(count=12, fps=60, recognition_delay_ms=5)
        self.assertEqual(result["hits"], 12)
        self.assertEqual(result["effects_seen"], 12)
        self.assertEqual(result["false_touches"], 0)
        self.assertEqual(result["frames_overwritten"], 0)
        self.assertEqual(result["schedule_error_ms"]["n"], 12)

    def test_png_decoder_accepts_rgba_screencap(self):
        def chunk(kind: bytes, data: bytes) -> bytes:
            return (struct.pack(">I", len(data)) + kind + data +
                    struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF))
        png = (b"\x89PNG\r\n\x1a\n" +
               chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)) +
               chunk(b"IDAT", zlib.compress(b"\x00\x01\x02\x03\xff")) +
               chunk(b"IEND", b""))
        self.assertEqual(decode_png_rgb(png), (1, 1, b"\x01\x02\x03"))


if __name__ == "__main__":
    unittest.main()
