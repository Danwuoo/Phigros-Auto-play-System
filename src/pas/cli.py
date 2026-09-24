"""Reproducible capability and timing commands."""

import argparse
import binascii
from collections import deque
from itertools import pairwise
import ctypes
import json
import os
import platform
import struct
import subprocess
import sys
import threading
import time
import uuid
import zlib
from pathlib import Path

from .adb import AdbLauncher, AdbPngCapture, find_adb, list_devices, probe
from .capture import CaptureWorker
from .capture_process import ProcessCaptureConfig, ProcessCaptureSource
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


def _source_revision() -> dict:
    try:
        head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                              text=True, check=True, timeout=2).stdout.strip()
        dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True,
                                    text=True, check=True, timeout=2).stdout.strip())
        return {"git_head": head, "git_dirty": dirty}
    except (OSError, subprocess.SubprocessError):
        return {"git_head": None, "git_dirty": None}


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


def _window_event_counts(path: str, start_ns: int, end_ns: int) -> dict[str, int]:
    """Recount exactly by event timestamp in the half-open measurement window."""
    counts = {key: 0 for key in ("inactive_frames", "invalid_frames", "source_gaps",
                                  "relative_stale_drops", "latest_published",
                                  "latest_overwritten", "consumer_reads", "consumer_skips")}
    events = {
        "capture_inactive": ("inactive_frames", "monotonic_ns", None),
        "capture_invalid": ("invalid_frames", "monotonic_ns", None),
        "capture_source_gap": ("source_gaps", "monotonic_ns", "gap"),
        "capture_relative_stale_drop": ("relative_stale_drops", "monotonic_ns", None),
        "capture": ("latest_published", "published_ns", None),
        "capture_overwrite": ("latest_overwritten", "monotonic_ns", None),
        "frame_consumed": ("consumer_reads", "consume_ns", None),
    }
    with Path(path).open(encoding="utf-8") as stream:
        for line in stream:
            item = json.loads(line)
            spec = events.get(item["event"])
            if spec is None:
                continue
            key, timestamp_key, value_key = spec
            timestamp = item.get(timestamp_key)
            if timestamp is not None and start_ns <= timestamp < end_ns:
                counts[key] += item[value_key] if value_key else 1
                if item["event"] == "frame_consumed":
                    counts["consumer_skips"] += item.get("sequence_skip", 0)
    return counts


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


def _child_cpu_seconds(pid: int | None) -> float | None:
    if pid is None:
        return None
    try:
        import psutil
        times = psutil.Process(pid).cpu_times()
        return times.user + times.system
    except (ImportError, OSError):
        pass
    if os.name != "nt":
        return None
    import ctypes
    import ctypes.wintypes as wt
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    kernel.OpenProcess.restype = wt.HANDLE
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        return None
    try:
        creation, exit_time, kernel_time, user_time = (wt.FILETIME() for _ in range(4))
        kernel.GetProcessTimes.argtypes = [wt.HANDLE, ctypes.POINTER(wt.FILETIME),
                                           ctypes.POINTER(wt.FILETIME), ctypes.POINTER(wt.FILETIME),
                                           ctypes.POINTER(wt.FILETIME)]
        if not kernel.GetProcessTimes(handle, ctypes.byref(creation), ctypes.byref(exit_time),
                                      ctypes.byref(kernel_time), ctypes.byref(user_time)):
            return None
        def ticks(value):
            return (value.dwHighDateTime << 32) | value.dwLowDateTime
        return (ticks(kernel_time) + ticks(user_time)) / 10_000_000
    finally:
        kernel.CloseHandle(handle)


def _child_rss_bytes(pid: int | None) -> int | None:
    if pid is None:
        return None
    try:
        import psutil
        return psutil.Process(pid).memory_info().rss
    except (ImportError, OSError):
        pass
    if os.name != "nt":
        return None
    class MemoryCounters(ctypes.Structure):
        _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong)] + [
            (name, ctypes.c_size_t) for name in (
                "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
                "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
    kernel = ctypes.windll.kernel32
    kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    kernel.OpenProcess.restype = ctypes.c_void_p
    handle = kernel.OpenProcess(0x1000 | 0x0400, False, pid)
    if not handle:
        return None
    try:
        counters = MemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        ctypes.windll.psapi.GetProcessMemoryInfo.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
        if ctypes.windll.psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
            return counters.WorkingSetSize
        return None
    finally:
        kernel.CloseHandle(handle)


def process_capture_bench(config: ProcessCaptureConfig, duration_s: float, warmup_s: float,
                          consumer_delay_ms: float, log: str | None = None,
                          ready_timeout_s: float = 10,
                          fixture_x: int = 0, fixture_y: int = 0,
                          fixture_scale: float = 1, fixture_scale_y: float | None = None,
                          diagnostic_mmap: bool = False, parent_load: bool = False,
                          receiver_pause_ms: float = 0,
                          consumer_recover_after_s: float | None = None,
                          instrument_log_cost: bool = True) -> dict:
    """Full-window offline or emulator benchmark; MMAP results stay diagnostic."""
    if duration_s <= 0 or warmup_s < 0 or ready_timeout_s <= 0 or consumer_delay_ms < 0:
        raise ValueError("invalid benchmark duration")
    if receiver_pause_ms < 0 or receiver_pause_ms > 10_000:
        raise ValueError("invalid receiver pause")
    if consumer_recover_after_s is not None and not 0 < consumer_recover_after_s < duration_s:
        raise ValueError("consumer recovery must fall in measurement window")
    if config.transport == "mmap" and not diagnostic_mmap:
        raise ValueError("MMAP requires explicit diagnostic opt-in")
    run_id = time.strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex[:8]
    log = log or str(Path("measurements") / run_id / "capture-process.jsonl")
    if Path(log).exists():
        raise FileExistsError(log)
    clock = HostClock()
    arrival = deque(maxlen=100_000)
    residency = deque(maxlen=100_000)
    ipc_handoff = deque(maxlen=100_000)
    fixture_values = deque(maxlen=100_000)
    frame_count = 0
    first_frame = None
    collecting_start = None
    collecting_end = None
    collection_lock = threading.Lock()
    load_stop = threading.Event()
    def spin_parent():
        value = 0
        while not load_stop.is_set():
            value = (value * 3 + 1) & 65535
    load_thread = threading.Thread(target=spin_parent, daemon=True) if parent_load else None
    with Telemetry(log, instrument_write_cost=instrument_log_cost) as telemetry:
        source = ProcessCaptureSource(config, diagnostic_mmap=diagnostic_mmap)
        def collect(frame):
            nonlocal frame_count, first_frame
            with collection_lock:
                if collecting_start is None or frame.capture_complete_ns < collecting_start:
                    return
                if collecting_end is not None and frame.capture_complete_ns >= collecting_end:
                    return
                frame_count += 1
                arrival.append(frame.capture_complete_ns)
                if frame.parent_snapshot_complete_ns is not None and frame.ipc_published_ns is not None:
                    ipc_handoff.append((frame.parent_snapshot_complete_ns - frame.ipc_published_ns) / 1e6)
                if first_frame is None:
                    first_frame = frame
                value = read_fixture_counter(frame, fixture_x, fixture_y,
                                             fixture_scale, fixture_scale_y)
                if value is not None:
                    fixture_values.append((frame.capture_complete_ns, value))
                    telemetry.record("fixture_counter", frame_sequence=frame.sequence,
                                     counter=value, capture_complete_ns=frame.capture_complete_ns)
        worker = CaptureWorker(source, clock, telemetry, on_frame=collect)
        begin = clock.now_ns()
        telemetry.record("run_config", mode="process_capture_bench", host=_host(),
                         source_revision=_source_revision(), run_id=run_id,
                         requested_execution="process", effective_execution="process",
                         requested_transport=config.transport, effective_transport=config.transport,
                         consistency="unverified" if config.transport == "mmap" else "payload",
                         source_kind=config.kind, serial=config.serial,
                         image_format=config.image_format, row_order=config.row_order,
                         max_rgb_bytes=config.max_rgb_bytes, duration_s=duration_s,
                         warmup_s=warmup_s, ready_timeout_s=ready_timeout_s,
                         consumer_delay_ms=consumer_delay_ms, diagnostic_mmap=diagnostic_mmap,
                         receiver_pause_ms=receiver_pause_ms,
                         consumer_recover_after_s=consumer_recover_after_s,
                         parent_load=parent_load, child_load=config.child_cpu_load,
                         instrument_log_cost=instrument_log_cost,
                         clock="time.monotonic_ns, same Windows host domain")
        telemetry.record("bench_phase", phase="CONNECTING", monotonic_ns=begin)
        worker.start()
        read_count = skip_count = 0
        recovery_first_residency_ms = recovery_first_sequence_skip = None
        start_ns = end_ns = None
        cpu_parent_start = cpu_parent_end = None
        cpu_child_start = cpu_child_end = None
        counters_start = counters_end = None
        snapshot_start_ns = snapshot_end_ns = None
        parent_rss = child_rss = None
        try:
            last = worker.wait_for_valid_frames(1, ready_timeout_s)
            ready_ns = clock.now_ns()
            telemetry.record("bench_phase", phase="WARMUP", monotonic_ns=ready_ns,
                             first_frame_sequence=last)
            if load_thread:
                load_thread.start()
            warmup_end = ready_ns + round(warmup_s * 1e9)
            while clock.now_ns() < warmup_end:
                if worker.error:
                    raise RuntimeError("capture failed during warmup")
                clock.sleep_until_ns(min(warmup_end, clock.now_ns() + 50_000_000))
            cpu_parent_start = time.process_time_ns()
            cpu_child_start = _child_cpu_seconds(source.process.pid)
            counters_start = source.counters()
            snapshot_start_ns = clock.now_ns()
            with collection_lock:
                start_ns = clock.now_ns()
                collecting_start = start_ns
                boundary_frame = worker.latest.peek()
                if boundary_frame is not None and boundary_frame.capture_complete_ns < start_ns:
                    last = max(last, boundary_frame.sequence)
            planned_end = start_ns + round(duration_s * 1e9)
            if receiver_pause_ms:
                source.request_receiver_pause(start_ns + round(duration_s * 0.4 * 1e9),
                                              receiver_pause_ms)
            telemetry.record("bench_phase", phase="MEASURING", monotonic_ns=start_ns,
                             planned_end_ns=planned_end, counters=counters_start,
                             parent_cpu_ns=cpu_parent_start, child_cpu_s=cpu_child_start,
                             snapshot_ns=snapshot_start_ns)
            while clock.now_ns() < planned_end:
                frame = worker.latest.read_after(last, min(0.05, max(0, (planned_end - clock.now_ns()) / 1e9)))
                if worker.error:
                    raise RuntimeError("capture failed during measurement")
                if frame is None or frame.capture_complete_ns < start_ns:
                    continue
                recovering = (consumer_recover_after_s is not None and
                              clock.now_ns() >= start_ns + round(consumer_recover_after_s * 1e9))
                if recovering and recovery_first_residency_ms is None:
                    recovery_first_residency_ms = (clock.now_ns() - frame.capture_complete_ns) / 1e6
                    recovery_first_sequence_skip = max(0, frame.sequence - last - 1)
                    telemetry.record("consumer_recovered", frame_sequence=frame.sequence,
                                     host_residency_ms=recovery_first_residency_ms,
                                     sequence_skip=recovery_first_sequence_skip)
                skip_count += max(0, frame.sequence - last - 1)
                last = frame.sequence
                read_count += 1
                consume_ns = clock.now_ns()
                residency.append((consume_ns - frame.capture_complete_ns) / 1e6)
                telemetry.record("frame_consumed", frame_sequence=frame.sequence,
                                 consume_ns=consume_ns, capture_complete_ns=frame.capture_complete_ns)
                if consumer_delay_ms and not recovering:
                    clock.sleep_until_ns(consume_ns + round(consumer_delay_ms * 1e6))
            with collection_lock:
                end_ns = clock.now_ns()
                collecting_end = end_ns
            snapshot_end_ns = clock.now_ns()
            counters_end = source.counters()
            cpu_parent_end = time.process_time_ns()
            cpu_child_end = _child_cpu_seconds(source.process.pid)
            parent_rss = _rss_bytes()
            child_rss = _child_rss_bytes(source.process.pid)
            telemetry.record("bench_phase", phase="STOPPING", monotonic_ns=end_ns,
                             counters=counters_end, parent_cpu_ns=cpu_parent_end,
                             child_cpu_s=cpu_child_end, snapshot_ns=snapshot_end_ns)
            if receiver_pause_ms:
                telemetry.record("receiver_pause", **source.receiver_pause_times())
        except Exception as error:
            telemetry.record("bench_failed", reason=type(error).__name__,
                             monotonic_ns=clock.now_ns(), frames_received=frame_count,
                             child_state=source.state, child_alive=(source.process.is_alive()
                                                                   if source.process else False),
                             counters=source.counters())
            raise
        finally:
            load_stop.set()
            if load_thread and load_thread.ident is not None:
                load_thread.join(0.5)
            worker.stop(join_timeout_s=2)
        telemetry.flush()
        image_path = str(Path(log).with_suffix(".png"))
        if first_frame is not None:
            _write_rgb_png(image_path, first_frame)
        window_s = (end_ns - start_ns) / 1e9
        values = list(arrival)
        gaps = [(b - a) / 1e6 for a, b in pairwise(values)]
        child_cpu = (cpu_child_end - cpu_child_start if cpu_child_start is not None and
                     cpu_child_end is not None else None)
        dynamic_verified = (config.kind != "grpc" or len({v for _, v in fixture_values}) > 1)
        summary = {"mode": "process_capture_bench", "status": "complete" if frame_count and dynamic_verified else "incomplete",
                   "run_id": run_id, "execution": "process", "transport": config.transport,
                   "consistency": "unverified" if config.transport == "mmap" else "payload",
                   "diagnostic_only": config.transport == "mmap",
                   "source_kind": config.kind, "ready_ns": ready_ns,
                   "parent_load": parent_load, "child_load": config.child_cpu_load,
                   "receiver_pause": source.receiver_pause_times() if receiver_pause_ms else None,
                   "consumer_recover_after_s": consumer_recover_after_s,
                   "recovery_first_host_residency_ms": recovery_first_residency_ms,
                   "recovery_first_sequence_skip": recovery_first_sequence_skip,
                   "measurement_start_ns": start_ns, "measurement_end_ns": end_ns,
                   "measurement_actual_duration_s": window_s,
                   "snapshot_start_ns": snapshot_start_ns, "snapshot_end_ns": snapshot_end_ns,
                   "snapshot_start_offset_ms": (start_ns - snapshot_start_ns) / 1e6,
                   "snapshot_end_offset_ms": (snapshot_end_ns - end_ns) / 1e6,
                   "frames_received": frame_count, "frames_read": read_count,
                   "consumer_skips": skip_count,
                   "arrival_interval_ms": distribution(gaps),
                   "host_residency_ms": distribution(residency),
                   "ipc_publish_to_parent_snapshot_ms": distribution(ipc_handoff),
                   "fixture_samples": len(fixture_values),
                   "fixture_distinct_counters": len({v for _, v in fixture_values}),
                   "window_counters_start": counters_start, "window_counters_end": counters_end,
                   "window_counter_delta": {key: counters_end[key] - counters_start[key]
                                            for key in counters_start},
                   "telemetry_drops": 0,
                   "telemetry_policy": "synchronous bounded-memory JSONL; write failure fails capture",
                   "parent_cpu_seconds": (cpu_parent_end - cpu_parent_start) / 1e9,
                   "child_cpu_seconds": child_cpu,
                   "child_cpu_one_core_percent": child_cpu / window_s * 100 if child_cpu is not None else None,
                   "parent_cpu_one_core_percent": (cpu_parent_end - cpu_parent_start) / 1e7 / window_s,
                   "parent_rss_bytes_at_end": parent_rss, "child_rss_bytes_at_end": child_rss,
                   "log_write_cost_ms": telemetry.write_cost_distribution(start_ns, end_ns),
                   "source_frame_age": "unknown; Unix source timestamp not mapped to host monotonic",
                   "log_path": log, "sample_png_path": image_path if first_frame else None}
        telemetry.record("summary", **summary)
        if frame_count == 0 or not dynamic_verified:
            raise RuntimeError("benchmark had no valid dynamic frames in measurement window")
        return summary


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
                       max_relative_lag_ms: float | None = None,
                       ready_timeout_s: float = 10) -> dict:
    if (duration_s <= 0 or warmup_s < 0 or consumer_delay_ms < 0 or
            receiver_pause_ms < 0 or ready_timeout_s <= 0):
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
    # Keep a bounded suffix for in-process percentiles; JSONL retains all events.
    arrival_times = deque(maxlen=100_000)
    ready_times = deque(maxlen=100_000)
    conversion_ms = deque(maxlen=100_000)
    source_sequences = deque(maxlen=100_000)
    source_times_us = deque(maxlen=100_000)
    fixture_samples = deque(maxlen=100_000)
    frames_received_total = fixture_samples_total = counter_changes = 0
    last_fixture_counter = None
    first_arrival_ns = last_arrival_ns = None
    longest_interarrival_ns = 0
    shapes: set[tuple[int, int, int | None]] = set()
    first_frame: Frame | None = None
    receiver_paused = False
    measurement_start: int | None = None
    measurement_end: int | None = None
    last_pre_window_sequence = -1
    collection_lock = threading.Lock()

    def collect(frame: Frame) -> None:
        nonlocal first_frame, receiver_paused, frames_received_total
        nonlocal fixture_samples_total, counter_changes, last_fixture_counter
        nonlocal first_arrival_ns, last_arrival_ns, longest_interarrival_ns
        nonlocal last_pre_window_sequence
        with collection_lock:
            if measurement_start is None or frame.capture_complete_ns < measurement_start:
                last_pre_window_sequence = frame.sequence
                return
            if measurement_end is not None and frame.capture_complete_ns >= measurement_end:
                return
            arrival_times.append(frame.capture_complete_ns)
            if first_arrival_ns is None:
                first_arrival_ns = frame.capture_complete_ns
            if last_arrival_ns is not None:
                longest_interarrival_ns = max(longest_interarrival_ns,
                                              frame.capture_complete_ns - last_arrival_ns)
            last_arrival_ns = frame.capture_complete_ns
            frames_received_total += 1
            ready_times.append(frame.pixels_ready_ns or frame.capture_complete_ns)
            conversion_ms.append(((frame.pixels_ready_ns or frame.capture_complete_ns) - frame.capture_complete_ns) / 1e6)
            if frame.source_sequence is not None:
                source_sequences.append(frame.source_sequence)
            if frame.source_timestamp_us is not None:
                source_times_us.append(frame.source_timestamp_us)
            fixture_counter = read_fixture_counter(frame, fixture_x, fixture_y,
                                                   fixture_scale, fixture_scale_y)
            if fixture_counter is not None:
                fixture_samples_total += 1
                if last_fixture_counter is not None and fixture_counter != last_fixture_counter:
                    counter_changes += 1
                last_fixture_counter = fixture_counter
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
        connecting_start_ns = clock.now_ns()
        ready_deadline_ns = connecting_start_ns + round(ready_timeout_s * 1e9)
        telemetry.record("bench_phase", phase="CONNECTING", monotonic_ns=connecting_start_ns)
        try:
            source = _grpc_source(serial, clock, telemetry, endpoint, token_file,
                                  image_format, width, height, row_order,
                                  max_relative_lag_ms)
            source_initialized_ns = clock.now_ns()
            if source_initialized_ns >= ready_deadline_ns:
                raise TimeoutError("capture readiness timed out during initialization")
        except Exception as error:
            telemetry.record("bench_failed", phase="CONNECTING", reason=str(error),
                             monotonic_ns=clock.now_ns(), frames_received=0)
            raise
        telemetry.record("run_config", mode="emulator_grpc_stream", run_id=run_id,
                         host=_host(), source_revision=_source_revision(),
                         command_argv=sys.argv, serial=serial, endpoint=source.endpoint.target,
                         emulator_instance=source.endpoint.instance,
                         image_format=image_format, requested_width=width,
                         requested_height=height, row_order=row_order,
                         max_relative_lag_ms=max_relative_lag_ms,
                         duration_s=duration_s,
                         warmup_s=warmup_s, ready_timeout_s=ready_timeout_s,
                         consumer_delay_ms=consumer_delay_ms,
                         consumer_recover_after_s=consumer_recover_after_s,
                         receiver_pause_ms=receiver_pause_ms,
                         fixture_offset=[fixture_x, fixture_y], fixture_scale=fixture_scale,
                         fixture_scale_y=fixture_scale_y,
                         load=load, clock="host_monotonic_ns",
                         source_timestamp_clock="unix_us_unmapped")
        worker = CaptureWorker(source, clock, telemetry, on_frame=collect)
        worker.start()
        last = -1
        read_count = skipped = 0
        recovery_residency_ms = None
        recovery_sequence_skip = None
        host_residency_ms = deque(maxlen=100_000)
        rss_samples = deque(maxlen=100_000)
        cpu_start_ns = cpu_end_ns = None
        start_counts = end_counts = None
        ready_ns = warmup_start_ns = warmup_end_ns = None
        actual_end_ns = None
        try:
            remaining_ready_s = (ready_deadline_ns - clock.now_ns()) / 1e9
            if remaining_ready_s <= 0:
                raise TimeoutError("capture readiness timed out before first frame")
            last = worker.wait_for_valid_frames(1, remaining_ready_s)
            ready_ns = clock.now_ns()
            telemetry.record("bench_phase", phase="WARMUP", monotonic_ns=ready_ns,
                             first_frame_sequence=last)
            if load_thread:
                load_thread.start()
                telemetry.record("load_started", monotonic_ns=clock.now_ns())
            warmup_start_ns = clock.now_ns()
            warmup_deadline_ns = warmup_start_ns + round(warmup_s * 1e9)
            while clock.now_ns() < warmup_deadline_ns:
                if worker.error:
                    raise RuntimeError(f"capture failed during warmup: {worker.error}")
                clock.sleep_until_ns(min(warmup_deadline_ns, clock.now_ns() + 50_000_000))
            warmup_end_ns = clock.now_ns()
            start_counts = {**source.counters(), **{f"latest_{k}": v for k, v in worker.latest.counters().items()},
                            "consumer_reads": read_count, "consumer_skips": skipped}
            cpu_start_ns = time.process_time_ns()
            with collection_lock:
                measurement_start = clock.now_ns()
                last = max(last, last_pre_window_sequence)
                boundary_sequence = last_pre_window_sequence
            planned_end_ns = measurement_start + round(duration_s * 1e9)
            telemetry.record("bench_phase", phase="MEASURING", monotonic_ns=measurement_start,
                             cpu_process_ns=cpu_start_ns, counters=start_counts,
                             last_pre_window_sequence=boundary_sequence)
            while clock.now_ns() < planned_end_ns:
                frame = worker.latest.read_after(last, min(0.05, max(0, (planned_end_ns - clock.now_ns()) / 1e9)))
                if worker.error:
                    raise RuntimeError(f"capture failed: {worker.error}")
                if frame is None:
                    continue
                if frame.capture_complete_ns < measurement_start:
                    last = max(last, frame.sequence)
                    continue
                # A pre-window frame can finish its callback after the boundary.
                with collection_lock:
                    last = max(last, last_pre_window_sequence)
                recovering = (consumer_recover_after_s is not None and
                              clock.now_ns() >= measurement_start + round(consumer_recover_after_s * 1e9))
                if recovering and recovery_residency_ms is None:
                    recovery_residency_ms = (clock.now_ns() - frame.capture_complete_ns) / 1e6
                    recovery_sequence_skip = max(0, frame.sequence - last - 1)
                    telemetry.record("consumer_recovered", frame_sequence=frame.sequence,
                                     source_sequence=frame.source_sequence,
                                     host_residency_ms=recovery_residency_ms,
                                     sequence_skip=recovery_sequence_skip)
                sequence_skip = max(0, frame.sequence - last - 1)
                skipped += sequence_skip
                last = frame.sequence
                read_count += 1
                now_ns = clock.now_ns()
                host_residency_ms.append((now_ns - frame.capture_complete_ns) / 1e6)
                rss = _rss_bytes()
                if rss is not None:
                    rss_samples.append(rss)
                telemetry.record("frame_consumed", frame_sequence=frame.sequence,
                                 source_sequence=frame.source_sequence,
                                 consume_ns=now_ns, sequence_skip=sequence_skip)
                if consumer_delay_ms and not recovering:
                    clock.sleep_until_ns(now_ns + round(consumer_delay_ms * 1e6))
            with collection_lock:
                actual_end_ns = clock.now_ns()
                measurement_end = actual_end_ns
            cpu_end_ns = time.process_time_ns()
            end_counts = {**source.counters(), **{f"latest_{k}": v for k, v in worker.latest.counters().items()},
                          "consumer_reads": read_count, "consumer_skips": skipped}
            telemetry.record("bench_phase", phase="STOPPING", monotonic_ns=actual_end_ns,
                             cpu_process_ns=cpu_end_ns, counters=end_counts)
        except Exception as error:
            telemetry.record("bench_failed", phase=("CONNECTING" if ready_ns is None else
                                                     "WARMUP" if measurement_start is None else "MEASURING"),
                             reason=str(error), monotonic_ns=clock.now_ns(),
                             frames_received=frames_received_total)
            raise
        finally:
            stop_load.set()
            if load_thread and load_thread.ident is not None:
                load_thread.join(2)
            worker.stop(join_timeout_s=2)
        cpu_s = (cpu_end_ns - cpu_start_ns) / 1e9
        telemetry.flush()
        image_path = str(Path(log).with_suffix(".png"))
        if first_frame:
            _write_rgb_png(image_path, first_frame)
        window_s = (actual_end_ns - measurement_start) / 1e9
        snapshot_delta = {key: end_counts[key] - start_counts[key] for key in start_counts}
        window_counts = _window_event_counts(log, measurement_start, actual_end_ns)
        frame_span_s = ((last_arrival_ns - first_arrival_ns) / 1e9
                        if frames_received_total >= 2 else None)
        no_frame_gap_ms = distribution([(b - a) / 1e6 for a, b in zip(
            [measurement_start if frames_received_total <= 100_000 else arrival_times[0],
             *arrival_times], [*arrival_times, actual_end_ns])])
        dynamic_verified = counter_changes > 0
        failure_reason = None if dynamic_verified else "dynamic fixture not verified in measurement window"
        summary = {"mode": "emulator_grpc_stream", "run_id": run_id,
                   "status": "complete" if dynamic_verified else "incomplete_dynamic_unverified",
                   "serial": serial, "endpoint": source.endpoint.target,
                   "emulator_instance": source.endpoint.instance,
                   "image_format": image_format, "duration_s": duration_s,
                   "ready_timeout_s": ready_timeout_s,
                   "connecting_start_ns": connecting_start_ns,
                   "source_initialized_ns": source_initialized_ns,
                   "ready_ns": ready_ns, "warmup_start_ns": warmup_start_ns,
                   "warmup_end_ns": warmup_end_ns,
                   "measurement_start_ns": measurement_start,
                   "last_pre_window_sequence_at_start": boundary_sequence,
                   "measurement_end_ns": actual_end_ns,
                   "measurement_actual_duration_s": window_s,
                   "first_last_frame_span_s": frame_span_s,
                   "no_frame_gap_ms": no_frame_gap_ms,
                   "longest_interarrival_ms": longest_interarrival_ns / 1e6,
                   "window_counters_start": start_counts,
                   "window_counters_end": end_counts,
                   "window_snapshot_delta": snapshot_delta,
                   "window_counters_delta": window_counts,
                   "window_counter_method": "JSONL event timestamps in [measurement_start_ns, measurement_end_ns)",
                   "row_order": row_order,
                   "warmup_s": warmup_s, "consumer_delay_ms": consumer_delay_ms,
                   "load": load, "frames_received": frames_received_total,
                   "distribution_scope": "bounded latest 100000 samples; raw JSONL has full window",
                   "receiver_pause_ms": receiver_pause_ms,
                   "consumer_recover_after_s": consumer_recover_after_s,
                   "recovery_first_host_residency_ms": recovery_residency_ms,
                   "recovery_first_sequence_skip": recovery_sequence_skip,
                   "distinct_source_sequences": frames_received_total,
                   "fixture_samples": fixture_samples_total,
                   "fixture_distinct_counters": len({value for _, value in fixture_samples}),
                   "fixture_counter_span": (fixture_samples[-1][1] - fixture_samples[0][1]
                                            if len(fixture_samples) > 1 else None),
                   "fixture_callback_rate_estimate_hz": (
                       (fixture_samples[-1][1] - fixture_samples[0][1]) * 1e9 /
                       (fixture_samples[-1][0] - fixture_samples[0][0])
                       if len(fixture_samples) > 1 and fixture_samples[-1][0] > fixture_samples[0][0]
                       else None),
                   "dimensions_rotation": sorted([list(s) for s in shapes]),
                   "arrival_interval_ms": distribution([(b - a) / 1e6 for a, b in pairwise(arrival_times)]),
                   "source_timestamp_interval_ms": distribution([(b - a) / 1e3 for a, b in pairwise(source_times_us)]),
                   "pixels_ready_interval_ms": distribution([(b - a) / 1e6 for a, b in pairwise(ready_times)]),
                   "conversion_ms": distribution(conversion_ms),
                   "host_residency_ms": distribution(host_residency_ms),
                   "source_sequence_gaps": window_counts["source_gaps"],
                   "relative_stale_drops": window_counts["relative_stale_drops"],
                   "max_relative_lag_ms": max_relative_lag_ms,
                   "inactive_frames": window_counts["inactive_frames"],
                   "invalid_frames": window_counts["invalid_frames"],
                   "frames_overwritten_unconsumed": window_counts["latest_overwritten"],
                   "frames_read": read_count, "consumer_sequence_skips": skipped,
                   "process_cpu_seconds": cpu_s,
                   "capture_error": str(worker.error) if worker.error else failure_reason,
                   "process_cpu_start_ns": cpu_start_ns,
                   "process_cpu_end_ns": cpu_end_ns,
                   "process_cpu_one_core_percent": cpu_s / window_s * 100,
                   "process_rss_mib": distribution([size / (1024 * 1024) for size in rss_samples]),
                   "source_frame_age": "unknown; Unix source timestamp not mapped to host monotonic",
                   "log_path": log, "sample_png_path": image_path if first_frame else None}
        telemetry.record("summary", **summary)
        if not dynamic_verified:
            telemetry.record("bench_failed", phase="MEASURING", reason=failure_reason,
                             monotonic_ns=clock.now_ns(), frames_received=frames_received_total)
            raise RuntimeError(f"benchmark incomplete: {failure_reason}")
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
                  max_relative_lag_ms: float | None = None,
                  capture_execution: str = "thread", grpc_transport: str = "payload",
                  max_rgb_bytes: int = 16 * 1024 * 1024) -> dict:
    if grpc_transport != "payload":
        raise ValueError("unverified MMAP cannot enter Session")
    if capture_execution == "process" and backend != "emulator-grpc":
        raise ValueError("process execution currently requires emulator-grpc")
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
                         capture_backend=backend, capture_execution=capture_execution,
                         grpc_transport=grpc_transport, consistency="payload",
                         touch_backend="disabled")
        if backend == "adb-png":
            source = AdbPngCapture(adb, serial, clock)
        elif capture_execution == "process":
            source = ProcessCaptureSource(ProcessCaptureConfig(
                serial=serial, endpoint=endpoint, token_file=token_file,
                image_format=image_format, width=width, height=height,
                row_order=row_order, max_relative_lag_ms=max_relative_lag_ms,
                max_rgb_bytes=max_rgb_bytes))
        else:
            source = _grpc_source(serial, clock, telemetry, endpoint, token_file,
                                  image_format, width, height, row_order,
                                  max_relative_lag_ms)
        worker = CaptureWorker(source, clock, telemetry)
        session = SessionController(worker, AdbLauncher(adb, serial, clock), clock, telemetry)
        monitor_state = None
        frame_fresh_at_monitor_end = False
        try:
            session.start(package, readiness_frames=1 if backend == "emulator-grpc" else 3)
            session.monitor_for(duration_s)
            monitor_state = session.state
            frame_fresh_at_monitor_end = session.frame_fresh
        finally:
            session.stop()
        result = {"mode": "capture_first_session", "state": session.state,
                  "monitor_state": monitor_state,
                  "frame_fresh_at_monitor_end": frame_fresh_at_monitor_end,
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
    p.add_argument("--capture-execution", choices=("thread", "process"), default="thread")
    p.add_argument("--grpc-transport", choices=("payload", "mmap"), default="payload")
    p.add_argument("--diagnostic-mmap", action="store_true")
    p.add_argument("--no-log-cost", action="store_true")
    p.add_argument("--max-rgb-bytes", type=int, default=16 * 1024 * 1024)
    p.add_argument("--samples", type=int, default=100)
    p.add_argument("--warmup", type=int, default=10)
    p.add_argument("--duration-s", type=float, default=60)
    p.add_argument("--warmup-s", type=float, default=10)
    p.add_argument("--ready-timeout-s", type=float, default=10)
    p.add_argument("--consumer-delay-ms", type=float, default=0)
    p.add_argument("--consumer-recover-after-s", type=float)
    p.add_argument("--load", action="store_true")
    p.add_argument("--child-load", action="store_true")
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
    p.add_argument("--capture-execution", choices=("thread", "process"), default="thread")
    p.add_argument("--grpc-transport", choices=("payload", "mmap"), default="payload")
    p.add_argument("--max-rgb-bytes", type=int, default=16 * 1024 * 1024)
    p.add_argument("--grpc-endpoint")
    p.add_argument("--grpc-token-file")
    p.add_argument("--image-format", choices=("rgb888", "rgba8888"), default="rgb888")
    p.add_argument("--row-order", choices=("top-down", "bottom-up"), default="top-down")
    p.add_argument("--max-relative-lag-ms", type=float)
    p.add_argument("--width", type=int, default=0)
    p.add_argument("--height", type=int, default=0)
    p.add_argument("--duration-s", type=float, default=30)
    p.add_argument("--log")
    p = sub.add_parser("offline-capture-bench", help="spawn fake pixels; never discovers an emulator")
    p.add_argument("--duration-s", type=float, default=3)
    p.add_argument("--warmup-s", type=float, default=1)
    p.add_argument("--fake-interval-ms", type=float, default=16.67)
    p.add_argument("--width", type=int, default=1280)
    p.add_argument("--height", type=int, default=720)
    p.add_argument("--consumer-delay-ms", type=float, default=0)
    p.add_argument("--load", action="store_true")
    p.add_argument("--child-load", action="store_true")
    p.add_argument("--no-log-cost", action="store_true")
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
                if args.capture_execution == "process":
                    if args.grpc_transport == "mmap" and not args.diagnostic_mmap:
                        raise ValueError("MMAP requires --diagnostic-mmap")
                    adb = find_adb()
                    if not adb or args.serial not in [d["serial"] for d in list_devices(adb)
                                                   if d["state"] == "device"]:
                        raise ValueError("explicit connected ADB serial required")
                    config = ProcessCaptureConfig(
                        serial=args.serial, endpoint=args.grpc_endpoint,
                        token_file=args.grpc_token_file, image_format=args.image_format,
                        width=args.width, height=args.height, row_order=args.row_order,
                        max_relative_lag_ms=args.max_relative_lag_ms,
                        transport=args.grpc_transport, max_rgb_bytes=args.max_rgb_bytes,
                        child_cpu_load=args.child_load)
                    result = process_capture_bench(config, args.duration_s, args.warmup_s,
                                                   args.consumer_delay_ms, args.log,
                                                   args.ready_timeout_s, args.fixture_x,
                                                   args.fixture_y, args.fixture_scale,
                                                   args.fixture_scale_y, args.diagnostic_mmap,
                                                   args.load, args.receiver_pause_ms,
                                                   args.consumer_recover_after_s,
                                                   not args.no_log_cost)
                elif args.grpc_transport != "payload":
                    raise ValueError("MMAP benchmark requires process execution")
                else:
                    if args.child_load:
                        raise ValueError("--child-load requires process execution")
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
                                            args.max_relative_lag_ms,
                                            args.ready_timeout_s)
            else:
                if (args.capture_execution != "thread" or args.grpc_transport != "payload" or
                        args.child_load or args.diagnostic_mmap):
                    raise ValueError("ADB benchmark supports only thread execution")
                result = capture_bench(args.serial, args.samples, args.warmup, args.log)
        elif args.command == "offline-capture-bench":
            config = ProcessCaptureConfig(kind="fake", fake_interval_ms=args.fake_interval_ms,
                                          fake_width=args.width, fake_height=args.height,
                                          child_cpu_load=args.child_load)
            result = process_capture_bench(config, args.duration_s, args.warmup_s,
                                           args.consumer_delay_ms, args.log,
                                           parent_load=args.load,
                                           instrument_log_cost=not args.no_log_cost)
        elif args.command == "buffer-bench":
            result = buffer_bench(args.duration_s, args.capture_interval_ms,
                                  args.consumer_delay_ms, args.log)
        else:
            result = start_session(args.serial, args.package, args.duration_s, args.log,
                                   args.capture_backend, args.grpc_endpoint,
                                   args.grpc_token_file, args.image_format,
                                   args.width, args.height, args.row_order,
                                   args.max_relative_lag_ms, args.capture_execution,
                                   args.grpc_transport, args.max_rgb_bytes)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (ValueError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
