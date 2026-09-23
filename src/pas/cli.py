"""Reproducible capability and timing commands."""

import argparse
import json
import platform
import sys
import threading

from .adb import AdbLauncher, AdbPngCapture, find_adb, list_devices, probe
from .capture import CaptureWorker
from .clock import HostClock
from .contracts import Frame, HitIntent
from .input import FakeTouchBackend
from .scheduler import Scheduler
from .session import SessionController
from .synthetic import run_synthetic
from .telemetry import Telemetry, distribution


def _host() -> dict:
    return {"platform": platform.platform(), "python": platform.python_version(),
            "machine": platform.machine(), "clock": "time.monotonic_ns"}


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
    shapes = set()
    with Telemetry(log) as telemetry:
        telemetry.record("run_config", mode="adb_screencap", host=_host(),
                         serial=serial, samples=samples, warmup=warmup,
                         capture_backend="adb exec-out screencap -p")
        for i in range(samples + warmup):
            frame, start_ns = capture.capture()
            decoded_ns = clock.now_ns()
            telemetry.record("capture", sample=i, capture_start_ns=start_ns,
                             capture_complete_ns=frame.capture_complete_ns,
                             decode_complete_ns=decoded_ns,
                             width=frame.width, height=frame.height)
            if i >= warmup:
                durations.append((frame.capture_complete_ns - start_ns) / 1e6)
                decode_durations.append((decoded_ns - frame.capture_complete_ns) / 1e6)
                complete_times.append(frame.capture_complete_ns)
                shapes.add((frame.width, frame.height))
        summary = {"mode": "adb_screencap", "host": _host(), "serial": serial,
                   "samples": samples, "warmup": warmup,
                   "dimensions": [list(shape) for shape in sorted(shapes)],
                   "capture_call_ms": distribution(durations),
                   "decode_ms": distribution(decode_durations),
                   "capture_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(complete_times, complete_times[1:])]),
                   "failed": 0, "log_path": log,
                   "source_frame_age": "unknown; screencap supplies no exposure timestamp"}
        telemetry.record("summary", **summary)
    return summary


def start_session(serial: str, package: str, duration_s: float, log: str | None) -> dict:
    adb = find_adb()
    if not adb:
        raise RuntimeError("adb unavailable; install Android SDK platform-tools")
    if serial not in [d["serial"] for d in list_devices(adb) if d["state"] == "device"]:
        raise ValueError("explicit serial is not connected")
    if duration_s <= 0:
        raise ValueError("duration must be positive")
    clock = HostClock()
    with Telemetry(log) as telemetry:
        telemetry.record("run_config", mode="capture_first_session", host=_host(),
                         serial=serial, package=package,
                         capture_backend="adb exec-out screencap -p",
                         touch_backend="disabled")
        worker = CaptureWorker(AdbPngCapture(adb, serial, clock), clock, telemetry)
        session = SessionController(worker, AdbLauncher(adb, serial, clock), clock, telemetry)
        try:
            session.start(package)
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
    p.add_argument("--samples", type=int, default=100)
    p.add_argument("--warmup", type=int, default=10)
    p.add_argument("--log")
    p = sub.add_parser("start-session", help="capture-first app launch; gameplay touch disabled")
    p.add_argument("--serial", required=True)
    p.add_argument("--package", required=True)
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
            result = capture_bench(args.serial, args.samples, args.warmup, args.log)
        elif args.command == "buffer-bench":
            result = buffer_bench(args.duration_s, args.capture_interval_ms,
                                  args.consumer_delay_ms, args.log)
        else:
            result = start_session(args.serial, args.package, args.duration_s, args.log)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (ValueError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
