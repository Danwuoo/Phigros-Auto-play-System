"""Real spawn and loopback tests. They never discover or contact an emulator."""

from concurrent.futures import ThreadPoolExecutor
import ctypes
from multiprocessing import get_context
from pathlib import Path
from tempfile import TemporaryDirectory
import mmap
import os
import struct
import subprocess
import sys
import threading
import time
import unittest
from urllib.parse import urlparse, unquote

from pas.capture_process import ProcessCaptureConfig, ProcessCaptureSource
from pas.capture_grpc import EmulatorGrpcCapture, GrpcEndpoint
from pas.cli import process_capture_bench


def _hold_lock(lock, ready):
    lock.acquire()
    ready.set()
    time.sleep(10)


class ProcessCaptureTests(unittest.TestCase):
    def test_spawn_rgb_snapshot_latest_and_monotonic(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake", fake_width=1280,
                                                           fake_height=720, fake_interval_ms=5))
        try:
            first, _ = source.capture()
            self.assertEqual((first.width, first.height), (1280, 720))
            self.assertEqual(len(first.rgb), 1280 * 720 * 3)
            original = first.rgb
            self.assertLessEqual(first.capture_complete_ns, first.pixels_ready_ns)
            self.assertLessEqual(first.pixels_ready_ns, time.monotonic_ns())
            time.sleep(0.1)
            newer, _ = source.capture()
            self.assertGreater(newer.sequence, first.sequence)
            self.assertEqual(first.rgb, original)
            self.assertEqual(source.shm.size, 256 + source.config.max_rgb_bytes)
            self.assertGreater(source.counters()["ipc_overwrites"], 0)
        finally:
            source.close()
        self.assertFalse(source.process.is_alive())

    def test_slow_consumer_recovers_to_latest(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake", fake_interval_ms=2))
        try:
            first, _ = source.capture()
            time.sleep(0.1)
            last, _ = source.capture()
            self.assertGreater(last.sequence - first.sequence, 10)
            self.assertLess(time.monotonic_ns() - last.capture_complete_ns, 80_000_000)
        finally:
            source.close()

    def test_child_crash_and_repeated_cleanup(self):
        for _ in range(2):
            source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake"))
            source.capture()
            source.process.terminate()
            source.process.join(1)
            with self.assertRaisesRegex(RuntimeError, "child"):
                source.capture()
            source.close()
            source.close()
            self.assertFalse(source.process.is_alive())

    def test_dead_lock_owner_is_bounded(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake"))
        helper = None
        try:
            source.capture()
            ctx = get_context("spawn")
            ready = ctx.Event()
            helper = ctx.Process(target=_hold_lock, args=(source.lock, ready))
            helper.start()
            self.assertTrue(ready.wait(2))
            helper.terminate()
            helper.join(1)
            started = time.monotonic()
            with self.assertRaises(TimeoutError):
                source.capture()
            self.assertLess(time.monotonic() - started, 1.5)
        finally:
            if helper is not None and helper.is_alive():
                helper.terminate()
                helper.join(1)
            source.close()

    def test_capacity_and_mmap_session_gate(self):
        with self.assertRaises(ValueError):
            ProcessCaptureConfig(kind="fake", fake_width=1280, fake_height=720,
                                 max_rgb_bytes=1024)
        with self.assertRaisesRegex(ValueError, "diagnostic"):
            ProcessCaptureSource(ProcessCaptureConfig(kind="loopback", serial="emulator-0000",
                                                      endpoint="127.0.0.1:12345", token_file="token",
                                                      transport="mmap"))

    def test_parent_python_busy_does_not_stop_child_producer(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake", fake_interval_ms=2))
        try:
            source.capture()
            before = source.counters()["ipc_published"]
            deadline = time.monotonic() + 0.15
            value = 0
            while time.monotonic() < deadline:
                value = (value * 3 + 1) & 65535
            after = source.counters()["ipc_published"]
            self.assertGreater(after, before)
        finally:
            source.close()

    def test_bounded_cross_process_health_probe(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake"))
        try:
            frame, _ = source.capture()
            self.assertEqual(source.probe_health(frame, timeout_s=0.5), "static")
            source.process.terminate()
            source.process.join(1)
            self.assertEqual(source.probe_health(frame, timeout_s=0.1), "failed")
        finally:
            source.close()

    def test_receiver_pause_and_recovery_are_timed(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake", fake_interval_ms=5))
        try:
            source.capture()
            source.request_receiver_pause(time.monotonic_ns() + 20_000_000, 80)
            deadline = time.monotonic() + 1
            while source.receiver_pause_times()["ended_ns"] is None and time.monotonic() < deadline:
                source.capture()
            times = source.receiver_pause_times()
            self.assertIsNotNone(times["started_ns"])
            self.assertIsNotNone(times["ended_ns"])
            self.assertGreaterEqual(times["ended_ns"] - times["started_ns"], 70_000_000)
            frame, _ = source.capture()
            self.assertLess(time.monotonic_ns() - frame.capture_complete_ns, 100_000_000)
        finally:
            source.close()

    def test_process_benchmark_window_pause_and_consumer_recovery(self):
        with TemporaryDirectory() as directory:
            log = str(Path(directory) / "run.jsonl")
            summary = process_capture_bench(
                ProcessCaptureConfig(kind="fake", fake_interval_ms=5),
                duration_s=0.45, warmup_s=0.1, consumer_delay_ms=50,
                log=log, receiver_pause_ms=80, consumer_recover_after_s=0.25)
            self.assertEqual(summary["status"], "complete")
            self.assertGreater(summary["frames_received"], summary["frames_read"])
            self.assertIsNotNone(summary["receiver_pause"]["ended_ns"])
            self.assertIsNotNone(summary["recovery_first_host_residency_ms"])
            self.assertGreater(summary["log_write_cost_ms"]["n"], 0)
            self.assertAlmostEqual(summary["measurement_actual_duration_s"], 0.45,
                                   delta=0.15)

    @unittest.skipUnless(sys.platform == "win32", "Windows parent death semantics")
    def test_abnormal_parent_exit_reaps_child(self):
        script = ("import os; from pas.capture_process import ProcessCaptureSource,"
                  "ProcessCaptureConfig; s=ProcessCaptureSource(ProcessCaptureConfig(kind='fake'));"
                  "s.capture(); print(s.process.pid,flush=True); os._exit(0)")
        env = dict(os.environ)
        env["PYTHONPATH"] = str(Path(__file__).resolve().parents[1] / "src")
        result = subprocess.run([sys.executable, "-c", script], env=env,
                                capture_output=True, text=True, timeout=5, check=True)
        pid = int(result.stdout.strip().splitlines()[-1])
        kernel = ctypes.windll.kernel32
        kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        kernel.WaitForSingleObject.restype = ctypes.c_ulong
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
        if not handle:
            return  # already exited
        try:
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and kernel.WaitForSingleObject(handle, 0) != 0:
                time.sleep(0.05)
            self.assertEqual(kernel.WaitForSingleObject(handle, 0), 0)
        finally:
            kernel.CloseHandle(handle)

    def test_corrupt_header_is_rejected(self):
        source = ProcessCaptureSource(ProcessCaptureConfig(kind="fake", fake_interval_ms=10))
        try:
            source.capture()
            source.stop_event.value = 1
            source.process.join(1)
            self.assertFalse(source.process.is_alive())
            with source.lock:
                source.shm.buf[0:4] = b"BAD!"
                struct.pack_into("<Q", source.shm.buf, 16, source._last_sequence + 1)
            with self.assertRaisesRegex(RuntimeError, "header"):
                source.capture()
        finally:
            source.close()


@unittest.skipUnless(__import__("importlib").util.find_spec("grpc"), "grpcio extra absent")
class MmapLoopbackTests(unittest.TestCase):
    def _server(self, writer, reply_bytes=b"", transport="mmap"):
        import grpc
        from pas.emulator_proto import emulator_controller_pb2 as pb
        server = grpc.server(ThreadPoolExecutor(max_workers=2))
        def stream(request, context):
            if dict(context.invocation_metadata()).get("authorization") != "Bearer test-secret":
                context.abort(grpc.StatusCode.UNAUTHENTICATED, "authentication required")
            if transport == "mmap":
                self.assertEqual(request.transport.channel, pb.ImageTransport.MMAP)
                self.assertTrue(request.transport.handle.startswith("file:///"))
                parsed = urlparse(request.transport.handle)
                path = unquote(parsed.path)
                if len(path) >= 3 and path[0] == "/" and path[2] == ":":
                    path = path[1:]
                with open(path, "r+b") as file:
                    with mmap.mmap(file.fileno(), 0) as mapped:
                        writer(mapped)
            else:
                self.assertEqual(request.transport.channel, 0)
            image = pb.Image(seq=0, timestampUs=123456)
            image.format.format = pb.ImageFormat.RGB888
            image.format.width = 2
            image.format.height = 2
            image.image = reply_bytes
            yield image
        method = grpc.unary_stream_rpc_method_handler(
            stream, request_deserializer=pb.ImageFormat.FromString,
            response_serializer=lambda value: value.SerializeToString())
        server.add_generic_rpc_handlers((grpc.method_handlers_generic_handler(
            "android.emulation.control.EmulatorController", {"streamScreenshot": method}),))
        port = server.add_insecure_port("127.0.0.1:0")
        server.start()
        return server, port

    def test_mmap_file_uri_copy_and_cleanup(self):
        server, port = self._server(lambda mapped: mapped.__setitem__(slice(0, 12), bytes(range(12))))
        source = EmulatorGrpcCapture("emulator-5554",
                                     endpoint=GrpcEndpoint(f"127.0.0.1:{port}", "test-secret", "fake"),
                                     transport="mmap", max_rgb_bytes=1024)
        try:
            frame, _ = source.capture()
            path = source._mmap_path
            self.assertEqual(frame.rgb, bytes(range(12)))
            self.assertEqual(source.consistency, "unverified")
            self.assertLessEqual(source.last_notification_ns, source.last_snapshot_copy_started_ns)
            self.assertLessEqual(source.last_snapshot_copy_started_ns,
                                 source.last_snapshot_copy_complete_ns)
        finally:
            source.close()
            server.stop(0).wait()
        self.assertFalse(Path(path).exists())

    def test_metadata_can_mismatch_overwritten_pixels(self):
        def writer(mapped):
            mapped[:12] = b"\x01" * 12  # nominal image 0
            mapped[:12] = b"\x02" * 12  # later write before image 0 notification
        server, port = self._server(writer)
        source = EmulatorGrpcCapture("emulator-5554",
                                     endpoint=GrpcEndpoint(f"127.0.0.1:{port}", "test-secret", "fake"),
                                     transport="mmap", max_rgb_bytes=1024)
        try:
            frame, _ = source.capture()
            self.assertEqual(frame.source_sequence, 0)
            self.assertEqual(frame.rgb, b"\x02" * 12)
        finally:
            source.close()
            server.stop(0).wait()

    def test_mmap_reply_payload_is_rejected(self):
        server, port = self._server(lambda mapped: mapped.__setitem__(slice(0, 12), b"a" * 12),
                                    reply_bytes=b"unexpected")
        source = EmulatorGrpcCapture("emulator-5554",
                                     endpoint=GrpcEndpoint(f"127.0.0.1:{port}", "test-secret", "fake"),
                                     transport="mmap", max_rgb_bytes=1024)
        try:
            with self.assertRaisesRegex(ValueError, "unexpectedly"):
                source.capture()
        finally:
            source.close()
            server.stop(0).wait()

    def test_spawned_payload_authenticates_and_delivers(self):
        server, port = self._server(lambda _: None, reply_bytes=bytes(range(12)),
                                    transport="payload")
        with TemporaryDirectory() as directory:
            token_file = Path(directory) / "token"
            token_file.write_text("test-secret", encoding="utf-8")
            source = ProcessCaptureSource(ProcessCaptureConfig(
                kind="loopback", serial="emulator-0000", endpoint=f"127.0.0.1:{port}",
                token_file=str(token_file), max_rgb_bytes=1024))
            try:
                frame, _ = source.capture()
                self.assertEqual(frame.rgb, bytes(range(12)))
                self.assertIsNotNone(frame.parent_snapshot_complete_ns)
                self.assertGreaterEqual(frame.parent_snapshot_complete_ns,
                                        frame.ipc_published_ns)
            finally:
                source.close()
                server.stop(0).wait()
