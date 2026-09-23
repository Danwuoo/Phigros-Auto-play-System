"""Reproducible capability and timing commands."""

import argparse
import binascii
import ctypes
import json
import platform
import struct
import sys
import threading
import time
import uuid
import zlib
from pathlib import Path

from .adb import AdbLauncher, AdbPngCapture, find_adb, list_devices, probe
from .capture import CaptureWorker
from .capture_grpc import EmulatorGrpcCapture, GrpcEndpoint, discover_endpoint
from .clock import HostClock
from .contracts import Frame, HitIntent
from .fixture import read_fixture_counter
from .input import FakeTouchBackend
from .scheduler import Scheduler
from .session import SessionController
from .synthetic import run_synthetic
from .telemetry import Telemetry, distribution


def _host() -> dict:
    return {"platform": platform.platform(), "python": platform.python_version(),
            "machine": platform.machine(), "clock": "time.monotonic_ns"}


def _write_rgb_png(path: str, frame: Frame) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF))

    stride = frame.width * 3
    raw = b"".join(b"\0" + frame.rgb[y * stride:(y + 1) * stride]
                   for y in range(frame.height))
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", frame.width, frame.height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 1)) + chunk(b"IEND", b""))
    Path(path).write_bytes(png)


def _rss_bytes() -> int | None:
    if sys.platform == "win32":
        class MemoryCounters(ctypes.Structure):
            _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong)] + [
                (name, ctypes.c_size_t) for name in (
                    "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                    "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
                    "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
        counters = MemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        ctypes.windll.kernel32.GetCurrentProcess.restype = ctypes.c_void_p
        ctypes.windll.psapi.GetProcessMemoryInfo.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(MemoryCounters), ctypes.c_ulong]
        if ctypes.windll.psapi.GetProcessMemoryInfo(
                ctypes.windll.kernel32.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            return counters.WorkingSetSize
        return None
    try:
        import resource
        usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        return usage * (1024 if sys.platform != "darwin" else 1)
    except ImportError:
        return None


def schedule_bench(samples: int, interval_ms: float, warmup: int,
                   load: bool, log: str | None) -> dict:
    if samples < 1 or warmup < 0 or interval_ms <= 0:
        raise ValueError("invalid sample, warmup or interval")
    clock = HostClock()
    stop = threading.Event()

    def spin() -> None:
        value = 0
        while not stop.is_set():
            value = (value * 3 + 1) & 65535

    worker = threading.Thread(target=spin, daemon=True) if load else None
    with Telemetry(log) as telemetry:
        backend = FakeTouchBackend(clock)
        scheduler = Scheduler(clock, backend, telemetry, max_late_ns=100_000_000)
        total = samples + warmup
        start = clock.now_ns() + 100_000_000
        interval_ns = round(interval_ms * 1e6)
        telemetry.record("run_config", mode="host_scheduler", host=_host(),
                         samples=samples, warmup=warmup, interval_ms=interval_ms,
                         load=load, touch_backend="fake_independent_contacts")
        for i in range(total):
            deadline = start + i * interval_ns
            intent = HitIntent(f"bench-{i}", i, i, 0, 0, deadline,
                               "synthetic_fixed_deadline", 1.0)
            scheduler.submit(intent)
        if worker:
            worker.start()
        try:
            while scheduler.next_due_ns() is not None:
                scheduler.run_next()
        finally:
            stop.set()
            if worker:
                worker.join(timeout=2)
        down = [r for r in backend.receipts if r.command.phase == "down" and
                int(r.command.key.split("-")[1]) >= warmup]
        summary = {"mode": "host_scheduler", "host": _host(), "samples": samples,
                   "warmup": warmup, "interval_ms": interval_ms, "load": load,
                   "touch_backend": "fake_independent_contacts",
                   "schedule_error_ms": distribution([(r.injection_start_ns - r.command.scheduled_ns) / 1e6 for r in down]),
                   "injection_duration_ms": distribution([(r.injection_return_ns - r.injection_start_ns) / 1e6 for r in down]),
                   "failed": sum(not r.success for r in down), "log_path": log}
        telemetry.record("summary", **summary)
    return summary


def _grpc_source(serial: str, clock: HostClock, telemetry: Telemetry,
                 endpoint: str | None, token_file: str | None,
                 image_format: str, width: int, height: int,
                 row_order: str, max_relative_lag_ms: float | None) -> EmulatorGrpcCapture:
    if bool(endpoint) != bool(token_file):
        raise ValueError("--grpc-endpoint and --grpc-token-file must be supplied together")
    if endpoint:
        token = Path(token_file).read_text(encoding="utf-8").strip()
        selected = GrpcEndpoint(endpoint, token, "explicit")
    else:
        selected = discover_endpoint(serial)
    return EmulatorGrpcCapture(serial, endpoint=selected, image_format=image_format,
                               width=width, height=height, row_order=row_order,
                               clock=clock, telemetry=telemetry,
                               max_relative_lag_ms=max_relative_lag_ms)


def grpc_capture_bench(serial: str, duration_s: float, warmup_s: float,
                       consumer_delay_ms: float, log: str | None,
                       endpoint: str | None = None, token_file: str | None = None,
                       image_format: str = "rgb888", width: int = 0,
                       height: int = 0, load: bool = False,
                       row_order: str = "top-down",
                       receiver_pause_ms: float = 0,
                       fixture_x: int = 0, fixture_y: int = 0,
                       fixture_scale: float = 1.0,
                       fixture_scale_y: float | None = None,
                       consumer_recover_after_s: float | None = None,
                       max_relative_lag_ms: float | None = None) -> dict:
    if duration_s <= 0 or warmup_s < 0 or consumer_delay_ms < 0 or receiver_pause_ms < 0:
        raise ValueError("invalid duration, warmup or consumer delay")
    if consumer_recover_after_s is not None and not 0 < consumer_recover_after_s < duration_s:
        raise ValueError("consumer recovery time must fall within measurement duration")
    adb = find_adb()
    if not adb or serial not in [d["serial"] for d in list_devices(adb) if d["state"] == "device"]:
        raise ValueError("explicit connected ADB serial required")
    run_id = time.strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex[:8]
    log = log or str(Path("measurements") / run_id / "capture.jsonl")
    if Path(log).exists():
        raise FileExistsError(f"log already exists: {log}")
    clock = HostClock()
    arrival_times: list[int] = []
    ready_times: list[int] = []
    conversion_ms: list[float] = []
    source_sequences: list[int] = []
    source_times_us: list[int] = []
    fixture_samples: list[tuple[int, int]] = []
    shapes: set[tuple[int, int, int | None]] = set()
    first_frame: Frame | None = None
    receiver_paused = False
    measurement_start = clock.now_ns() + round(warmup_s * 1e9)
    measurement_end = measurement_start + round(duration_s * 1e9)

    def collect(frame: Frame) -> None:
        nonlocal first_frame, receiver_paused
        if measurement_start <= frame.capture_complete_ns < measurement_end:
            arrival_times.append(frame.capture_complete_ns)
            ready_times.append(frame.pixels_ready_ns or frame.capture_complete_ns)
            conversion_ms.append(((frame.pixels_ready_ns or frame.capture_complete_ns) - frame.capture_complete_ns) / 1e6)
            if frame.source_sequence is not None:
                source_sequences.append(frame.source_sequence)
            if frame.source_timestamp_us is not None:
                source_times_us.append(frame.source_timestamp_us)
            fixture_counter = read_fixture_counter(frame, fixture_x, fixture_y,
                                                   fixture_scale, fixture_scale_y)
            if fixture_counter is not None:
                fixture_samples.append((frame.capture_complete_ns, fixture_counter))
                telemetry.record("fixture_counter", frame_sequence=frame.sequence,
                                 source_sequence=frame.source_sequence,
                                 counter=fixture_counter,
                                 capture_complete_ns=frame.capture_complete_ns)
            shapes.add((frame.width, frame.height, frame.source_rotation))
            if first_frame is None:
                first_frame = frame
            if receiver_pause_ms and not receiver_paused and frame.capture_complete_ns >= measurement_start + round(duration_s * 0.4 * 1e9):
                receiver_paused = True
                telemetry.record("receiver_pause", frame_sequence=frame.sequence,
                                 duration_ms=receiver_pause_ms,
                                 start_ns=clock.now_ns())
                clock.sleep_until_ns(clock.now_ns() + round(receiver_pause_ms * 1e6))

    stop_load = threading.Event()

    def spin() -> None:
        value = 0
        while not stop_load.is_set():
            value = (value * 3 + 1) & 65535

    load_thread = threading.Thread(target=spin) if load else None
    with Telemetry(log) as telemetry:
        source = _grpc_source(serial, clock, telemetry, endpoint, token_file,
                              image_format, width, height, row_order,
                              max_relative_lag_ms)
        telemetry.record("run_config", mode="emulator_grpc_stream", run_id=run_id,
                         host=_host(), serial=serial, endpoint=source.endpoint.target,
                         emulator_instance=source.endpoint.instance,
                         image_format=image_format, requested_width=width,
                         requested_height=height, row_order=row_order,
                         max_relative_lag_ms=max_relative_lag_ms,
                         duration_s=duration_s,
                         warmup_s=warmup_s, consumer_delay_ms=consumer_delay_ms,
                         consumer_recover_after_s=consumer_recover_after_s,
                         receiver_pause_ms=receiver_pause_ms,
                         fixture_offset=[fixture_x, fixture_y], fixture_scale=fixture_scale,
                         fixture_scale_y=fixture_scale_y,
                         load=load, clock="host_monotonic_ns",
                         source_timestamp_clock="unix_us_unmapped")
        worker = CaptureWorker(source, clock, telemetry, on_frame=collect)
        worker.start()
        if load_thread:
            load_thread.start()
        last = -1
        read_count = skipped = 0
        recovery_residency_ms = None
        recovery_sequence_skip = None
        host_residency_ms: list[float] = []
        rss_samples: list[int] = []
        cpu_start = time.process_time()
        try:
            while clock.now_ns() < measurement_end:
                frame = worker.latest.read_after(last, 0.1)
                if worker.error:
                    raise RuntimeError(f"capture failed: {worker.error}")
                if frame is None:
                    continue
                if clock.now_ns() < measurement_start:
                    last = frame.sequence
                    continue
                recovering = (consumer_recover_after_s is not None and
                              clock.now_ns() >= measurement_start + round(consumer_recover_after_s * 1e9))
                if recovering and recovery_residency_ms is None:
                    recovery_residency_ms = (clock.now_ns() - frame.capture_complete_ns) / 1e6
                    recovery_sequence_skip = max(0, frame.sequence - last - 1)
                    telemetry.record("consumer_recovered", frame_sequence=frame.sequence,
                                     source_sequence=frame.source_sequence,
                                     host_residency_ms=recovery_residency_ms,
                                     sequence_skip=recovery_sequence_skip)
                skipped += max(0, frame.sequence - last - 1)
                last = frame.sequence
                read_count += 1
                now_ns = clock.now_ns()
                host_residency_ms.append((now_ns - frame.capture_complete_ns) / 1e6)
                rss = _rss_bytes()
                if rss is not None:
                    rss_samples.append(rss)
                telemetry.record("frame_consumed", frame_sequence=frame.sequence,
                                 source_sequence=frame.source_sequence,
                                 consume_ns=now_ns)
                if consumer_delay_ms and not recovering:
                    clock.sleep_until_ns(now_ns + round(consumer_delay_ms * 1e6))
        finally:
            stop_load.set()
            if load_thread:
                load_thread.join(2)
            worker.stop(join_timeout_s=2)
        cpu_s = time.process_time() - cpu_start
        image_path = str(Path(log).with_suffix(".png"))
        if first_frame:
            _write_rgb_png(image_path, first_frame)
        summary = {"mode": "emulator_grpc_stream", "run_id": run_id,
                   "serial": serial, "endpoint": source.endpoint.target,
                   "emulator_instance": source.endpoint.instance,
                   "image_format": image_format, "duration_s": duration_s,
                   "row_order": row_order,
                   "warmup_s": warmup_s, "consumer_delay_ms": consumer_delay_ms,
                   "load": load, "frames_received": len(arrival_times),
                   "receiver_pause_ms": receiver_pause_ms,
                   "consumer_recover_after_s": consumer_recover_after_s,
                   "recovery_first_host_residency_ms": recovery_residency_ms,
                   "recovery_first_sequence_skip": recovery_sequence_skip,
                   "distinct_source_sequences": len(set(source_sequences)),
                   "fixture_samples": len(fixture_samples),
                   "fixture_distinct_counters": len({value for _, value in fixture_samples}),
                   "fixture_counter_span": (fixture_samples[-1][1] - fixture_samples[0][1]
                                            if len(fixture_samples) > 1 else None),
                   "fixture_callback_rate_estimate_hz": (
                       (fixture_samples[-1][1] - fixture_samples[0][1]) * 1e9 /
                       (fixture_samples[-1][0] - fixture_samples[0][0])
                       if len(fixture_samples) > 1 and fixture_samples[-1][0] > fixture_samples[0][0]
                       else None),
                   "dimensions_rotation": sorted([list(s) for s in shapes]),
                   "arrival_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(arrival_times, arrival_times[1:])]),
                   "source_timestamp_interval_ms": distribution([(b - a) / 1e3 for a, b in zip(source_times_us, source_times_us[1:])]),
                   "pixels_ready_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(ready_times, ready_times[1:])]),
                   "conversion_ms": distribution(conversion_ms),
                   "host_residency_ms": distribution(host_residency_ms),
                   "source_sequence_gaps": source.source_gaps,
                   "relative_stale_drops": source.relative_stale_drops,
                   "max_relative_lag_ms": max_relative_lag_ms,
                   "inactive_frames": source.inactive_frames,
                   "frames_overwritten_unconsumed": worker.latest.overwritten,
                   "frames_read": read_count, "consumer_sequence_skips": skipped,
                   "process_cpu_seconds": cpu_s, "capture_error": str(worker.error) if worker.error else None,
                   "process_cpu_one_core_percent": cpu_s / duration_s * 100,
                   "process_rss_mib": distribution([size / (1024 * 1024) for size in rss_samples]),
                   "source_frame_age": "unknown; Unix source timestamp not mapped to host monotonic",
                   "log_path": log, "sample_png_path": image_path if first_frame else None}
        telemetry.record("summary", **summary)
    return summary


def capture_bench(serial: str, samples: int, warmup: int, log: str | None) -> dict:
    adb = find_adb()
    if not adb:
        raise RuntimeError("adb unavailable; install Android SDK platform-tools")
    if samples < 1 or warmup < 0:
        raise ValueError("invalid sample or warmup")
    if serial not in [d["serial"] for d in list_devices(adb) if d["state"] == "device"]:
        raise ValueError("explicit serial is not connected")
    capture = AdbPngCapture(adb, serial)
    clock = HostClock()
    durations = []
    decode_durations = []
    complete_times = []
    ready_times = []
    shapes = set()
    with Telemetry(log) as telemetry:
        telemetry.record("run_config", mode="adb_screencap", host=_host(),
                         serial=serial, samples=samples, warmup=warmup,
                         capture_backend="adb exec-out screencap -p")
        for i in range(samples + warmup):
            frame, start_ns = capture.capture()
            decoded_ns = frame.pixels_ready_ns or clock.now_ns()
            telemetry.record("capture", sample=i, capture_start_ns=start_ns,
                             capture_complete_ns=frame.capture_complete_ns,
                             pixels_ready_ns=decoded_ns,
                             decode_complete_ns=decoded_ns,
                             width=frame.width, height=frame.height)
            if i >= warmup:
                durations.append((frame.capture_complete_ns - start_ns) / 1e6)
                decode_durations.append((decoded_ns - frame.capture_complete_ns) / 1e6)
                complete_times.append(frame.capture_complete_ns)
                ready_times.append(frame.pixels_ready_ns or decoded_ns)
                shapes.add((frame.width, frame.height))
        summary = {"mode": "adb_screencap", "host": _host(), "serial": serial,
                   "samples": samples, "warmup": warmup,
                   "dimensions": [list(shape) for shape in sorted(shapes)],
                   "capture_call_ms": distribution(durations),
                   "decode_ms": distribution(decode_durations),
                   "payload_arrival_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(complete_times, complete_times[1:])]),
                   "capture_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(complete_times, complete_times[1:])]),
                   "capture_interval_semantics": "legacy alias: adjacent PNG payload completion times",
                   "pixels_ready_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(ready_times, ready_times[1:])]),
                   "failed": 0, "log_path": log,
                   "source_frame_age": "unknown; screencap supplies no exposure timestamp"}
        telemetry.record("summary", **summary)
    return summary


def start_session(serial: str, package: str, duration_s: float, log: str | None,
                  backend: str = "adb-png", endpoint: str | None = None,
                  token_file: str | None = None, image_format: str = "rgb888",
                  width: int = 0, height: int = 0,
                  row_order: str = "top-down",
                  max_relative_lag_ms: float | None = None) -> dict:
    adb = find_adb()
    if not adb:
        raise RuntimeError("adb unavailable; install Android SDK platform-tools")
    if serial not in [d["serial"] for d in list_devices(adb) if d["state"] == "device"]:
        raise ValueError("explicit serial is not connected")
    if duration_s <= 0:
        raise ValueError("duration must be positive")
    if backend == "emulator-grpc" and log is None:
        run_id = time.strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex[:8]
        log = str(Path("measurements") / run_id / "session.jsonl")
    clock = HostClock()
    with Telemetry(log) as telemetry:
        telemetry.record("run_config", mode="capture_first_session", host=_host(),
                         serial=serial, package=package,
                         capture_backend=backend,
                         touch_backend="disabled")
        source = (AdbPngCapture(adb, serial, clock) if backend == "adb-png"
                  else _grpc_source(serial, clock, telemetry, endpoint, token_file,
                                    image_format, width, height, row_order,
                                    max_relative_lag_ms))
        worker = CaptureWorker(source, clock, telemetry)
        session = SessionController(worker, AdbLauncher(adb, serial, clock), clock, telemetry)
        try:
            session.start(package, readiness_frames=1 if backend == "emulator-grpc" else 3)
            session.monitor_for(duration_s)
        finally:
            session.stop()
        result = {"mode": "capture_first_session", "state": session.state,
                  "frames_published": worker.latest.published,
                  "frames_overwritten": worker.latest.overwritten,
                  "capture_error": str(worker.error) if worker.error else None,
                  "gameplay_touch_enabled": False, "log_path": log}
        telemetry.record("summary", **result)
    return result


def buffer_bench(duration_s: float, capture_interval_ms: float,
                 consumer_delay_ms: float, log: str | None) -> dict:
    if duration_s <= 0 or capture_interval_ms <= 0 or consumer_delay_ms < 0:
        raise ValueError("invalid buffer benchmark duration or interval")
    clock = HostClock()

    class Source:
        sequence = 0

        def capture(self):
            start_ns = clock.now_ns()
            clock.sleep_until_ns(start_ns + round(capture_interval_ms * 1e6))
            complete_ns = clock.now_ns()
            frame = Frame(self.sequence, 1, 1, b"\0\0\0", complete_ns, complete_ns)
            self.sequence += 1
            return frame, start_ns

    with Telemetry(log) as telemetry:
        telemetry.record("run_config", mode="latest_frame_buffer", host=_host(),
                         duration_s=duration_s,
                         capture_interval_ms=capture_interval_ms,
                         consumer_delay_ms=consumer_delay_ms)
        worker = CaptureWorker(Source(), clock, telemetry)
        worker.start()
        deadline = clock.now_ns() + round(duration_s * 1e9)
        last = -1
        read_count = 0
        skipped_sequences = 0
        ages = []
        try:
            while clock.now_ns() < deadline:
                frame = worker.latest.read_after(last, 0.1)
                if frame is None:
                    continue
                skipped_sequences += max(0, frame.sequence - last - 1)
                last = frame.sequence
                read_count += 1
                age = clock.now_ns() - frame.capture_complete_ns
                ages.append(age / 1e6)
                telemetry.record("frame_consumed", frame_sequence=frame.sequence,
                                 capture_complete_ns=frame.capture_complete_ns,
                                 consume_ns=clock.now_ns())
                clock.sleep_until_ns(clock.now_ns() + round(consumer_delay_ms * 1e6))
        finally:
            worker.stop()
        result = {"mode": "latest_frame_buffer", "host": _host(),
                  "duration_s": duration_s,
                  "capture_interval_ms": capture_interval_ms,
                  "consumer_delay_ms": consumer_delay_ms,
                  "frames_published": worker.latest.published,
                  "frames_read": read_count,
                  "frames_overwritten_unconsumed": worker.latest.overwritten,
                  "sequences_skipped_by_consumer": skipped_sequences,
                  "consumed_frame_age_ms": distribution(ages),
                  "log_path": log}
        telemetry.record("summary", **result)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="pas")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("probe", help="inventory ADB and optionally a selected device")
    p.add_argument("--serial")
    p = sub.add_parser("synthetic", help="deterministic pixels-to-touch-to-pixels test")
    p.add_argument("--count", type=int, default=30)
    p.add_argument("--fps", type=int, default=60)
    p.add_argument("--recognition-delay-ms", type=float, default=0)
    p.add_argument("--log")
    p = sub.add_parser("schedule-bench", help="host monotonic scheduling baseline")
    p.add_argument("--samples", type=int, default=100)
    p.add_argument("--warmup", type=int, default=10)
    p.add_argument("--interval-ms", type=float, default=10)
    p.add_argument("--load", action="store_true")
    p.add_argument("--log")
    p = sub.add_parser("capture-bench", help="selected ADB screencap candidate")
    p.add_argument("--serial", required=True)
    p.add_argument("--capture-backend", choices=("adb-png", "emulator-grpc"), default="adb-png")
    p.add_argument("--samples", type=int, default=100)
    p.add_argument("--warmup", type=int, default=10)
    p.add_argument("--duration-s", type=float, default=60)
    p.add_argument("--warmup-s", type=float, default=10)
    p.add_argument("--consumer-delay-ms", type=float, default=0)
    p.add_argument("--consumer-recover-after-s", type=float)
    p.add_argument("--load", action="store_true")
    p.add_argument("--receiver-pause-ms", type=float, default=0)
    p.add_argument("--fixture-x", type=int, default=0)
    p.add_argument("--fixture-y", type=int, default=0)
    p.add_argument("--fixture-scale", type=float, default=1.0)
    p.add_argument("--fixture-scale-y", type=float)
    p.add_argument("--grpc-endpoint")
    p.add_argument("--grpc-token-file")
    p.add_argument("--image-format", choices=("rgb888", "rgba8888"), default="rgb888")
    p.add_argument("--row-order", choices=("top-down", "bottom-up"), default="top-down")
    p.add_argument("--max-relative-lag-ms", type=float)
    p.add_argument("--width", type=int, default=0)
    p.add_argument("--height", type=int, default=0)
    p.add_argument("--log")
    p = sub.add_parser("start-session", help="capture-first app launch; gameplay touch disabled")
    p.add_argument("--serial", required=True)
    p.add_argument("--package", required=True)
    p.add_argument("--capture-backend", choices=("adb-png", "emulator-grpc"), default="adb-png")
    p.add_argument("--grpc-endpoint")
    p.add_argument("--grpc-token-file")
    p.add_argument("--image-format", choices=("rgb888", "rgba8888"), default="rgb888")
    p.add_argument("--row-order", choices=("top-down", "bottom-up"), default="top-down")
    p.add_argument("--max-relative-lag-ms", type=float)
    p.add_argument("--width", type=int, default=0)
    p.add_argument("--height", type=int, default=0)
    p.add_argument("--duration-s", type=float, default=30)
    p.add_argument("--log")
    p = sub.add_parser("buffer-bench", help="host producer/slow-consumer capacity-one test")
    p.add_argument("--duration-s", type=float, default=1)
    p.add_argument("--capture-interval-ms", type=float, default=2)
    p.add_argument("--consumer-delay-ms", type=float, default=20)
    p.add_argument("--log")
    args = parser.parse_args(argv)
    try:
        if args.command == "probe":
            adb = find_adb()
            result = probe(adb, args.serial) if adb else {
                "adb_path": None, "devices": [],
                "status": "adb unavailable; install Android SDK platform-tools",
                "emulator_integration_verified": False}
        elif args.command == "synthetic":
            result = run_synthetic(args.count, args.fps, args.recognition_delay_ms, args.log)
        elif args.command == "schedule-bench":
            result = schedule_bench(args.samples, args.interval_ms, args.warmup,
                                    args.load, args.log)
        elif args.command == "capture-bench":
            if args.capture_backend == "emulator-grpc":
                result = grpc_capture_bench(args.serial, args.duration_s, args.warmup_s,
                                            args.consumer_delay_ms, args.log,
                                            args.grpc_endpoint, args.grpc_token_file,
                                            args.image_format, args.width, args.height,
                                            args.load, args.row_order,
                                            args.receiver_pause_ms,
                                            args.fixture_x, args.fixture_y,
                                            args.fixture_scale,
                                            args.fixture_scale_y,
                                            args.consumer_recover_after_s,
                                            args.max_relative_lag_ms)
            else:
                result = capture_bench(args.serial, args.samples, args.warmup, args.log)
        elif args.command == "buffer-bench":
            result = buffer_bench(args.duration_s, args.capture_interval_ms,
                                  args.consumer_delay_ms, args.log)
        else:
            result = start_session(args.serial, args.package, args.duration_s, args.log,
                                   args.capture_backend, args.grpc_endpoint,
                                   args.grpc_token_file, args.image_format,
                                   args.width, args.height, args.row_order,
                                   args.max_relative_lag_ms)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (ValueError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
