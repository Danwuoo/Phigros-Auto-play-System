"""Protocol conversion and cancellation tests with no AVD dependency."""

from pathlib import Path
from tempfile import TemporaryDirectory
import threading
import time
import unittest

from pas.capture import CaptureWorker
from pas.capture_grpc import EmulatorGrpcCapture, GrpcEndpoint, discover_endpoint, rgb24_from_image
from pas.clock import HostClock
from pas.contracts import Frame
from pas.fixture import read_fixture_counter
from pas.session import SessionController
from pas.telemetry import Telemetry


class Format:
    def __init__(self, width=2, height=2, pixel_format=2, rotation=1):
        self.width, self.height, self.format = width, height, pixel_format
        self.rotation = type("Rotation", (), {"rotation": rotation})()


class Image:
    def __init__(self, data, *, sequence=0, width=2, height=2, pixel_format=2):
        self.format = Format(width, height, pixel_format)
        self.image = data
        self.seq = sequence
        self.timestampUs = 123456


class CaptureGrpcTests(unittest.TestCase):
    def test_event_stream_session_does_not_duplicate_static_frame(self):
        class Source:
            event_driven = True

            def __init__(self):
                self.calls = 0
                self.stopped = threading.Event()

            def capture(self):
                self.calls += 1
                if self.calls == 1:
                    return Frame(0, 1, 1, b"\x00\x00\x00", time.monotonic_ns()), 0
                self.stopped.wait(2)
                raise RuntimeError("closed")

            def close(self):
                self.stopped.set()

        class Launcher:
            def launch(self, package):
                now = time.monotonic_ns()
                return now, now

        clock = HostClock()
        worker = CaptureWorker(Source(), clock, Telemetry())
        session = SessionController(worker, Launcher(), clock, Telemetry())
        session.start("example.game", readiness_frames=1)
        session.monitor_for(0.06, max_frame_age_s=0.005)
        self.assertEqual(worker.latest.published, 1)
        session.stop()

    def test_fixture_counter_decoding(self):
        width, height = 250, 150
        data = bytearray(width * height * 3)

        def paint(x, y, rgb):
            at = (y * width + x) * 3
            data[at:at + 3] = rgb

        expected = 0xA55A01
        for row in range(2):
            y = 96 + row * 26
            paint(16, y, b"\xff\x00\x00")
            paint(206, y, b"\xff\x00\x00")
            for bit in range(12):
                paint(36 + bit * 14, y,
                      b"\xff\xff\xff" if expected & (1 << (row * 12 + bit)) else b"\x00\x00\x00")
        frame = Frame(0, width, height, bytes(data), 0)
        self.assertEqual(read_fixture_counter(frame), expected)
        self.assertIsNone(read_fixture_counter(frame, 1, 0))

    def test_row_order_and_rgba_conversion(self):
        # Payload order: bottom row blue/white, top row red/green.
        rgb = bytes([0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 0])
        expected = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])
        self.assertEqual(rgb24_from_image(Image(rgb), 2, "bottom-up"), (2, 2, expected))
        self.assertEqual(rgb24_from_image(Image(rgb), 2), (2, 2, rgb))
        rgba = bytes(value for i in range(0, len(rgb), 3) for value in (*rgb[i:i + 3], 7))
        self.assertEqual(rgb24_from_image(Image(rgba, pixel_format=1), 1, "bottom-up"), (2, 2, expected))

    def test_rejects_invalid_payload_and_format(self):
        with self.assertRaisesRegex(ValueError, "length"):
            rgb24_from_image(Image(b"short"), 2)
        with self.assertRaisesRegex(ValueError, "format"):
            rgb24_from_image(Image(bytes(12), pixel_format=1), 2)
        with self.assertRaisesRegex(ValueError, "dimensions"):
            rgb24_from_image(Image(b"", width=0), 2)

    def test_discovery_requires_exact_serial_match_and_token(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "pid_10.ini").write_text(
                "port.serial=5554\ngrpc.port=8554\ngrpc.token=secret\n", encoding="utf-8")
            endpoint = discover_endpoint("emulator-5554", root)
            self.assertEqual(endpoint.target, "127.0.0.1:8554")
            self.assertEqual(endpoint.token, "secret")
            with self.assertRaises(RuntimeError):
                discover_endpoint("emulator-5556", root)
            (root / "pid_11.ini").write_text(
                "port.serial=5554\ngrpc.port=8555\ngrpc.token=secret\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                discover_endpoint("emulator-5554", root)

    @unittest.skipUnless(__import__("importlib").util.find_spec("grpc"), "grpcio extra not installed")
    def test_stream_sequence_gap_and_blocking_cancel(self):
        class BlockingCall:
            def __init__(self):
                self.ready = threading.Event()
                self.cancelled = threading.Event()
                self.count = 0

            def __iter__(self):
                return self

            def __next__(self):
                if self.count < 2:
                    seq = (0, 3)[self.count]
                    self.count += 1
                    return Image(bytes(12), sequence=seq)
                self.ready.set()
                self.cancelled.wait(5)
                raise StopIteration

            def cancel(self):
                self.cancelled.set()

        call = BlockingCall()

        class FakeSource(EmulatorGrpcCapture):
            def _open(self):
                with self._lock:
                    self._call = call
                return call

        source = FakeSource("emulator-5554", endpoint=GrpcEndpoint("127.0.0.1:8554", "x", "fake"))
        worker = CaptureWorker(source, HostClock(), Telemetry())
        worker.start()
        self.assertTrue(call.ready.wait(1))
        current = worker.latest.read_after(-1, 0)
        self.assertEqual(current.sequence, 1)
        self.assertEqual(current.source_sequence, 3)
        self.assertEqual(source.source_gaps, 2)
        started = time.monotonic()
        worker.stop(join_timeout_s=2)
        worker.stop(join_timeout_s=2)
        self.assertLess(time.monotonic() - started, 2)
        self.assertIsNone(worker.error)

    @unittest.skipUnless(__import__("importlib").util.find_spec("grpc"), "grpcio extra not installed")
    def test_inactive_and_bad_payload_are_not_published(self):
        class Call:
            def __init__(self):
                self.images = iter([Image(b"", width=0, height=0), Image(b"bad", sequence=1)])

            def __next__(self):
                return next(self.images)

            def cancel(self):
                pass

        class FakeSource(EmulatorGrpcCapture):
            def _open(self):
                self._call = Call()
                return self._call

        log = Telemetry()
        source = FakeSource("emulator-5554", endpoint=GrpcEndpoint("127.0.0.1:8554", "x", "fake"),
                            telemetry=log)
        with self.assertRaisesRegex(ValueError, "length"):
            source.capture()
        self.assertEqual(source.inactive_frames, 1)
        self.assertIn("capture_invalid", [event["event"] for event in log.events])
        source.close()

    @unittest.skipUnless(__import__("importlib").util.find_spec("grpc"), "grpcio extra not installed")
    def test_geometry_change_and_source_reset_stop_stream(self):
        class Call:
            def __init__(self):
                self.images = iter([Image(bytes(12), sequence=8),
                                    Image(bytes(6), sequence=9, width=1),
                                    Image(bytes(6), sequence=0, width=1)])

            def __next__(self):
                return next(self.images)

            def cancel(self):
                pass

        class FakeSource(EmulatorGrpcCapture):
            def _open(self):
                self._call = Call()
                return self._call

        log = Telemetry()
        source = FakeSource("emulator-5554", endpoint=GrpcEndpoint("127.0.0.1:8554", "x", "fake"),
                            telemetry=log)
        first, _ = source.capture()
        second, _ = source.capture()
        self.assertEqual((first.sequence, second.sequence), (0, 1))
        self.assertEqual(second.width, 1)
        self.assertIn("capture_geometry_changed", [event["event"] for event in log.events])
        with self.assertRaisesRegex(RuntimeError, "sequence reset"):
            source.capture()
        source.close()

    @unittest.skipUnless(__import__("importlib").util.find_spec("grpc"), "grpcio extra not installed")
    def test_relative_lag_guard_drops_queued_old_frame(self):
        class Clock:
            value = 0

            def now_ns(self):
                self.value += 10_000_000
                return self.value

        class Call:
            def __init__(self):
                images = [Image(bytes(12), sequence=i) for i in range(3)]
                for image, timestamp in zip(images, (1_000_000, 1_001_000, 1_060_000)):
                    image.timestampUs = timestamp
                self.images = iter(images)

            def __next__(self):
                return next(self.images)

            def cancel(self):
                pass

        class FakeSource(EmulatorGrpcCapture):
            def _open(self):
                self._call = Call()
                return self._call

        source = FakeSource("emulator-5554", endpoint=GrpcEndpoint("127.0.0.1:8554", "x", "fake"),
                            clock=Clock(), max_relative_lag_ms=5)
        first, _ = source.capture()
        second, _ = source.capture()
        self.assertEqual((first.sequence, first.source_sequence), (0, 0))
        self.assertEqual((second.sequence, second.source_sequence), (1, 2))
        self.assertEqual(source.relative_stale_drops, 1)
        self.assertEqual(source.source_gaps, 0)
        source.close()


if __name__ == "__main__":
    unittest.main()
