"""Spawned capture producer with a capacity-one, locked shared-pixel exchange.

The lock protects the *child-to-parent* snapshot only. It says nothing about
the emulator's independent MMAP writer.
"""

from dataclasses import dataclass
from multiprocessing import get_context, parent_process, shared_memory
from pathlib import Path
import os
import struct
import tempfile
import threading
import time
import zlib

from .clock import HostClock
from .contracts import Frame

MAX_RGB_BYTES = 16 * 1024 * 1024
HEADER_BYTES = 256
SCHEMA = 2
_HEADER = struct.Struct("<4sIQQQIIIIQQQQqqII")
_EXTRA = struct.Struct("<qq")
_MAGIC = b"PAS1"
# Header: magic, schema, generation, sequence, source sequence, width, height,
# rotation, payload length, capture complete, pixels ready, child publish,
# notification, source timestamp, produced (always -1), crc32, row order.
_STATES = {0: "STARTING", 1: "READY", 2: "STOPPING", 3: "STOPPED", 4: "FAILED",
           5: "FORCED_STOPPED"}


@dataclass(frozen=True)
class ProcessCaptureConfig:
    kind: str = "grpc"
    serial: str = ""
    endpoint: str | None = None
    token_file: str | None = None
    image_format: str = "rgb888"
    width: int = 0
    height: int = 0
    row_order: str = "top-down"
    max_relative_lag_ms: float | None = None
    transport: str = "payload"
    max_rgb_bytes: int = MAX_RGB_BYTES
    fake_interval_ms: float = 10
    fake_width: int = 16
    fake_height: int = 16
    child_cpu_load: bool = False

    def __post_init__(self):
        if self.kind not in ("grpc", "fake", "loopback") or self.transport not in ("payload", "mmap"):
            raise ValueError("unsupported process capture configuration")
        if self.max_rgb_bytes <= 0 or self.max_rgb_bytes > MAX_RGB_BYTES:
            raise ValueError("invalid IPC capacity")
        if (self.image_format not in ("rgb888", "rgba8888") or
                self.row_order not in ("top-down", "bottom-up") or
                self.width < 0 or self.height < 0 or
                (self.max_relative_lag_ms is not None and self.max_relative_lag_ms <= 0)):
            raise ValueError("invalid source image configuration")
        if self.kind == "grpc" and bool(self.endpoint) != bool(self.token_file):
            raise ValueError("endpoint and token file must be supplied together")
        if self.kind == "loopback" and (not self.endpoint or not self.token_file):
            raise ValueError("loopback source requires an explicit endpoint and token file")
        if self.kind == "fake" and self.transport != "payload":
            raise ValueError("fake pixel source supports payload only; use loopback gRPC for MMAP")
        if self.kind == "fake" and (self.fake_width <= 0 or self.fake_height <= 0 or
                                    self.fake_width * self.fake_height * 3 > self.max_rgb_bytes or
                                    self.fake_interval_ms <= 0):
            raise ValueError("invalid fake source dimensions or interval")


class _FakeSource:
    event_driven = True

    def __init__(self, config):
        self.config = config
        self.sequence = 0
        self.closed = False
        self.inactive = False

    def capture(self):
        start = time.monotonic_ns()
        time.sleep(self.config.fake_interval_ms / 1000)
        if self.closed:
            raise RuntimeError("source closed")
        now = time.monotonic_ns()
        value = self.sequence % 256
        rgb = bytes((value, value ^ 255, 33)) * (self.config.fake_width * self.config.fake_height)
        frame = Frame(self.sequence, self.config.fake_width, self.config.fake_height,
                      rgb, now, pixels_ready_ns=now, source_sequence=self.sequence,
                      source_timestamp_us=None, source_rotation=0)
        self.sequence += 1
        return frame, start

    def close(self):
        self.closed = True

    def counters(self):
        return {"source_gaps": 0, "relative_stale_drops": 0,
                "inactive_frames": 0, "invalid_frames": 0}


def _make_source(config, mmap_directory=None):
    if config.kind == "fake":
        return _FakeSource(config)
    from pathlib import Path
    from .capture_grpc import EmulatorGrpcCapture, GrpcEndpoint, discover_endpoint
    selected = (GrpcEndpoint(config.endpoint, Path(config.token_file).read_text(encoding="utf-8").strip(),
                             "explicit") if config.endpoint else discover_endpoint(config.serial))
    return EmulatorGrpcCapture(config.serial, endpoint=selected,
                               image_format=config.image_format, width=config.width,
                               height=config.height, row_order=config.row_order,
                               max_relative_lag_ms=config.max_relative_lag_ms,
                               transport=config.transport, max_rgb_bytes=config.max_rgb_bytes,
                               mmap_directory=mmap_directory)


def _cleanup_mmap_directory(directory, timeout_s=0.75):
    """Remove only this source's private directory, after its child has exited."""
    if directory is None:
        return
    deadline = time.monotonic() + timeout_s
    path = Path(directory)
    while path.exists():
        try:
            for child in path.glob("pas-emulator-mmap-*.bin"):
                child.unlink(missing_ok=True)
            path.rmdir()
            return
        except OSError:
            # On Windows the server may still be releasing its mapping after
            # cancellation/disconnect. Never scan other sources' temp files.
            if time.monotonic() >= deadline:
                raise RuntimeError("owned MMAP directory could not be cleaned") from None
            time.sleep(0.02)


def _child_main(config, shm_name, lock, stop, state, generation, counters, probe, pause,
                mmap_directory):
    """Module-level spawn entry. No channel, lock closure, or Frame is pickled."""
    clock = HostClock()
    shm = shared_memory.SharedMemory(name=shm_name)
    source = None
    sequence = 0
    latest_frame = None
    stats_thread = None
    cancel_thread = None
    load_thread = None
    parent = parent_process()
    try:
        source = _make_source(config, mmap_directory)
        def watch_stop():
            # Independent of capture(), IPC locks, and potentially slow probes.
            while not stop.value:
                if parent is not None and not parent.is_alive():
                    stop.value = 1
                    break
                time.sleep(0.02)
            try:
                getattr(source, "cancel", source.close)()
            except Exception:
                pass  # Parent still enforces a bounded join/terminate deadline.
        cancel_thread = threading.Thread(target=watch_stop, name="capture-child-cancel", daemon=True)
        cancel_thread.start()
        def spin():
            value = 0
            while not stop.value:
                value = (value * 3 + 1) & 65535
        def sample_status():
            while not stop.value:
                time.sleep(0.05)
                if stop.value:
                    break
                counts = source.counters()
                counters[6] = counts.get("source_gaps", 0)
                counters[7] = counts.get("relative_stale_drops", 0)
                counters[8] = counts.get("inactive_frames", 0)
                counters[9] = counts.get("invalid_frames", 0)
                counters[10] = clock.now_ns()
                counters[11] = int(getattr(source, "inactive", False))
                if probe[0] > probe[1]:
                    requested = probe[0]
                    check = getattr(source, "probe_health", None)
                    if check is None or latest_frame is None:
                        result = "static" if latest_frame is not None else "failed"
                    else:
                        try:
                            result = check(latest_frame, timeout_s=0.5)
                        except Exception:
                            result = "failed"
                    probe[2] = {"static": 1, "changed": 2}.get(result, 3)
                    probe[1] = requested
        stats_thread = threading.Thread(target=sample_status, name="capture-child-status", daemon=True)
        stats_thread.start()
        while not stop.value:
            frame, _ = source.capture()
            if stop.value:
                break
            latest_frame = frame
            size = len(frame.rgb)
            if size > config.max_rgb_bytes:
                raise ValueError("frame exceeds configured IPC capacity; restart with larger capacity")
            if not lock.acquire(timeout=0.2):
                counters[5] += 1
                continue
            try:
                # This section is short and bounded by max_rgb_bytes. A dead
                # writer is detected by the parent's timed acquire.
                old_sequence = struct.unpack_from("<Q", shm.buf, 16)[0]
                if old_sequence != 0 and old_sequence > counters[4]:
                    counters[0] += 1  # overwritten before parent read
                shm.buf[HEADER_BYTES:HEADER_BYTES + size] = frame.rgb
                publish_ns = clock.now_ns()
                _HEADER.pack_into(shm.buf, 0, _MAGIC, SCHEMA, generation, sequence + 1,
                                  frame.source_sequence if frame.source_sequence is not None else 0,
                                  frame.width, frame.height, frame.source_rotation or 0,
                                  size, frame.capture_complete_ns,
                                  frame.pixels_ready_ns or frame.capture_complete_ns,
                                  publish_ns, getattr(source, "last_notification_ns", 0),
                                  frame.source_timestamp_us or -1, -1,
                                  zlib.crc32(frame.rgb), 0)  # IPC pixels are normalized top-down
                _EXTRA.pack_into(shm.buf, _HEADER.size,
                                 frame.snapshot_copy_started_ns or -1,
                                 frame.snapshot_copy_complete_ns or -1)
                sequence += 1
                counters[1] = sequence
                state.value = 1
            finally:
                lock.release()
            if config.child_cpu_load and load_thread is None:
                # Establish a valid stream before applying controlled GIL load.
                load_thread = threading.Thread(target=spin, name="capture-child-load", daemon=True)
                load_thread.start()
            if pause[0] and not pause[2] and clock.now_ns() >= pause[0]:
                pause[2] = clock.now_ns()
                time.sleep(pause[1] / 1e9)
                pause[3] = clock.now_ns()
    except Exception:
        # Never send an exception string across the boundary: gRPC failures
        # or a future source may embed credentials in one.
        if not stop.value:
            state.value = 4
            counters[3] += 1
    finally:
        stop.value = 1
        if cancel_thread is not None:
            cancel_thread.join(0.5)
        if stats_thread is not None:
            stats_thread.join(0.2)
        if load_thread is not None:
            load_thread.join(0.2)
        if source is not None:
            try:
                source.close()
            except Exception:
                pass
        if state.value != 4:
            state.value = 3
        shm.close()
        if parent is not None and not parent.is_alive():
            # After abnormal parent exit there is no supervisor left to clean.
            _cleanup_mmap_directory(mmap_directory)


class ProcessCaptureSource:
    """CaptureSource adapter; immutable Frame bytes are copied under a timed lock."""

    event_driven = True

    def __init__(self, config: ProcessCaptureConfig, *, diagnostic_mmap: bool = False):
        if config.transport == "mmap" and not diagnostic_mmap:
            raise ValueError("unverified MMAP is diagnostic-only and cannot publish valid Session frames")
        self.config = config
        self.ctx = get_context("spawn")
        self.shm = shared_memory.SharedMemory(create=True, size=HEADER_BYTES + config.max_rgb_bytes)
        self.shm.buf[:HEADER_BYTES] = bytes(HEADER_BYTES)
        self.lock = self.ctx.Lock()
        self.stop_event = self.ctx.Value("i", 0, lock=False)
        self.state_value = self.ctx.Value("i", 0, lock=False)
        self.counters_value = self.ctx.Array("Q", 12, lock=False)
        self.probe_value = self.ctx.Array("Q", 3, lock=False)
        self.pause_value = self.ctx.Array("Q", 4, lock=False)
        self.generation = int.from_bytes(os.urandom(8), "little") or 1
        self.process = None
        self._last_sequence = 0
        self._closed = False
        self._guard = threading.Lock()
        # Known before spawning, so even a forcibly killed writer is recoverable.
        self._mmap_directory = (tempfile.mkdtemp(prefix="pas-capture-mmap-")
                                if config.transport == "mmap" else None)
        self.shutdown_report = None

    @property
    def inactive(self):
        return bool(self.counters_value[11])

    @property
    def state(self):
        return _STATES.get(self.state_value.value, "FAILED")

    def _start(self):
        with self._guard:
            if self._closed:
                raise RuntimeError("capture source closed")
            if self.process is None:
                self.process = self.ctx.Process(target=_child_main,
                                                args=(self.config, self.shm.name, self.lock,
                                                      self.stop_event,
                                                      self.state_value, self.generation,
                                                      self.counters_value, self.probe_value,
                                                      self.pause_value, self._mmap_directory),
                                                name="pas-capture-child")
                self.process.start()

    def capture(self):
        self._start()
        start_ns = time.monotonic_ns()
        lock_wait_since = None
        while True:
            if self._closed:
                raise RuntimeError("capture source closed")
            if not self.lock.acquire(timeout=0.1):
                self.counters_value[2] += 1
                if lock_wait_since is None:
                    lock_wait_since = time.monotonic_ns()
                if self.process is not None and not self.process.is_alive():
                    raise RuntimeError("capture child stopped while IPC lock was held")
                if time.monotonic_ns() - lock_wait_since > 500_000_000:
                    raise TimeoutError("IPC lock unavailable beyond deadline")
                continue
            lock_wait_since = None
            try:
                header = _HEADER.unpack_from(self.shm.buf)
                (magic, schema, generation, sequence, source_sequence, width, height,
                 rotation, size, complete_ns, ready_ns, publish_ns, notification_ns,
                 source_us, produced_ns, crc, row_order) = header
                if sequence > self._last_sequence:
                    snapshot_start_ns, snapshot_end_ns = _EXTRA.unpack_from(self.shm.buf, _HEADER.size)
                    if (magic != _MAGIC or schema != SCHEMA or generation != self.generation or
                            width <= 0 or height <= 0 or size != width * height * 3 or
                            size > self.config.max_rgb_bytes or row_order != 0 or
                            produced_ns != -1 or not 0 < complete_ns <= ready_ns <= publish_ns or
                            (notification_ns and notification_ns > complete_ns) or
                            ((snapshot_start_ns == -1) != (snapshot_end_ns == -1)) or
                            (snapshot_start_ns != -1 and not
                             0 < snapshot_start_ns <= snapshot_end_ns == complete_ns)):
                        self.counters_value[2] += 1
                        raise RuntimeError("invalid IPC frame header")
                    rgb = bytes(self.shm.buf[HEADER_BYTES:HEADER_BYTES + size])
                    if zlib.crc32(rgb) != crc:
                        self.counters_value[2] += 1
                        raise RuntimeError("IPC pixel checksum mismatch")
                    self.counters_value[4] = sequence
                    self._last_sequence = sequence
                    parent_snapshot_ns = time.monotonic_ns()
                    frame = Frame(sequence - 1, width, height, rgb, complete_ns,
                                  pixels_ready_ns=ready_ns, source_sequence=source_sequence,
                                  stream_generation=generation,
                                  source_timestamp_us=None if source_us == -1 else source_us,
                                  source_rotation=rotation,
                                  notification_received_ns=notification_ns or None,
                                  snapshot_copy_started_ns=(snapshot_start_ns if snapshot_start_ns > 0 else None),
                                  snapshot_copy_complete_ns=(snapshot_end_ns if snapshot_end_ns > 0 else None),
                                  ipc_published_ns=publish_ns,
                                  parent_snapshot_complete_ns=parent_snapshot_ns)
                    return frame, start_ns
            finally:
                self.lock.release()
            if self.state_value.value == 4 or (self.process is not None and not self.process.is_alive()):
                raise RuntimeError("capture child failed or exited")
            # Poll the published sequence with a finite interval. A killed
            # process cannot strand a multiprocessing.Event condition lock.
            if self.counters_value[1] > self._last_sequence:
                continue
            time.sleep(0.005)

    def counters(self):
        names = ("ipc_overwrites", "ipc_published", "ipc_read_rejections", "child_failures",
                 "ipc_read_sequence", "ipc_publish_lock_timeout", "source_gaps",
                 "relative_stale_drops", "inactive_frames", "invalid_frames",
                 "child_heartbeat_ns", "source_inactive")
        return dict(zip(names, list(self.counters_value)))

    def probe_health(self, last_frame, timeout_s=0.5):
        if self.process is None or not self.process.is_alive():
            return "failed"
        request = self.probe_value[0] + 1
        self.probe_value[0] = request
        deadline = time.monotonic() + timeout_s + 0.1
        while time.monotonic() < deadline:
            if self.probe_value[1] >= request:
                return {1: "static", 2: "changed"}.get(self.probe_value[2], "failed")
            if not self.process.is_alive():
                return "failed"
            time.sleep(0.005)
        return "failed"

    def request_receiver_pause(self, after_ns: int, duration_ms: float):
        if after_ns <= 0 or duration_ms <= 0 or duration_ms > 10_000:
            raise ValueError("invalid receiver pause")
        if self.pause_value[0]:
            raise RuntimeError("receiver pause already requested")
        self.pause_value[1] = round(duration_ms * 1e6)
        self.pause_value[0] = after_ns

    def receiver_pause_times(self):
        return {"requested_after_ns": self.pause_value[0] or None,
                "started_ns": self.pause_value[2] or None,
                "ended_ns": self.pause_value[3] or None}

    def close(self, join_timeout_s=2):
        with self._guard:
            if self._closed:
                return
            self._closed = True
            if self.state_value.value != 4:
                self.state_value.value = 2
            self.stop_event.value = 1
            process = self.process
        forced = False
        reaped = process is None or process.pid is None
        cleanup_error = None
        try:
            if process is not None and process.pid is not None:
                process.join(join_timeout_s)
                if process.is_alive():
                    forced = True
                    process.terminate()
                    process.join(1)
                if process.is_alive():
                    process.kill()
                    process.join(1)
                if process.is_alive():
                    raise TimeoutError("capture child could not be reaped")
                reaped = True
        finally:
            self.shm.close()
            self.shm.unlink()
            if reaped:
                try:
                    _cleanup_mmap_directory(self._mmap_directory)
                except RuntimeError as error:
                    cleanup_error = error
            self.shutdown_report = {"forced": forced, "child_reaped": reaped,
                                    "exitcode": process.exitcode if process else None,
                                    "mmap_cleanup_complete": reaped and cleanup_error is None}
            if not reaped or cleanup_error or (process and process.exitcode not in (None, 0) and not forced):
                self.state_value.value = 4
            elif forced:
                self.state_value.value = 5
            elif self.state_value.value != 4:
                self.state_value.value = 3
            if cleanup_error:
                raise cleanup_error
