"""Authenticated screenshot stream, normalized to top-left-origin RGB24.

The installed Emulator 37.1.11 sends top-down rows despite its proto comment
claiming bottom-up; row order is configurable and must be checked per version.
"""

from dataclasses import dataclass, field
from pathlib import Path
import mmap
import os
import re
import tempfile
import threading

from .clock import Clock, HostClock
from .contracts import Frame
from .telemetry import Telemetry


@dataclass(frozen=True)
class GrpcEndpoint:
    target: str
    token: str = field(repr=False)
    instance: str


def discover_endpoint(serial: str, running_dir: Path | None = None) -> GrpcEndpoint:
    """Match an ADB emulator serial to one live emulator discovery file."""
    match = re.fullmatch(r"emulator-(\d+)", serial)
    if not match:
        raise ValueError("emulator gRPC requires an emulator-NNNN ADB serial")
    directory = running_dir or Path(tempfile.gettempdir()) / "avd" / "running"
    candidates = []
    for path in directory.glob("pid_*.ini"):
        fields = {}
        try:
            for line in path.read_text(encoding="utf-8").splitlines():
                if "=" in line:
                    key, value = line.split("=", 1)
                    fields[key] = value
        except OSError:
            continue
        if fields.get("port.serial") == match.group(1):
            candidates.append((path, fields))
    if len(candidates) != 1:
        raise RuntimeError(f"expected one emulator gRPC discovery file for {serial}; found {len(candidates)}")
    path, fields = candidates[0]
    port = fields.get("grpc.port", "")
    token = fields.get("grpc.token", "")
    if not port.isdecimal() or not 1 <= int(port) <= 65535 or not token:
        raise RuntimeError("emulator gRPC discovery file lacks a port or token")
    return GrpcEndpoint(f"127.0.0.1:{port}", token, path.stem)


def rgb24_from_image(image, requested_format: int,
                     row_order: str = "top-down", source_bytes: bytes | None = None) -> tuple[int, int, bytes]:
    """Validate raw pixels and normalize row order into top-down RGB24."""
    width, height = image.format.width, image.format.height
    if width <= 0 or height <= 0 or width * height > 20_000_000:
        raise ValueError("invalid screenshot dimensions")
    if image.format.format != requested_format or requested_format not in (1, 2):
        raise ValueError("unexpected screenshot pixel format")
    if row_order not in ("top-down", "bottom-up"):
        raise ValueError("invalid row order")
    channels = 3 if requested_format == 2 else 4
    source = image.image if source_bytes is None else source_bytes
    stride = width * channels
    if len(source) != stride * height:
        raise ValueError("invalid screenshot payload length")
    if channels == 3:
        if row_order == "top-down":
            return width, height, source
        return width, height, b"".join(source[y * stride:(y + 1) * stride]
                                       for y in range(height - 1, -1, -1))
    try:
        from PIL import Image as PilImage
    except ImportError:
        PilImage = None
    if PilImage is not None:
        picture = PilImage.frombytes("RGBA", (width, height), source)
        if row_order == "bottom-up":
            picture = picture.transpose(PilImage.Transpose.FLIP_TOP_BOTTOM)
        return width, height, picture.convert("RGB").tobytes()
    output = bytearray(width * height * 3)
    for y in range(height):
        source_y = y if row_order == "top-down" else height - y - 1
        row = source[source_y * stride:(source_y + 1) * stride]
        target = memoryview(output)[y * width * 3:(y + 1) * width * 3]
        target[0::3] = row[0::4]
        target[1::3] = row[1::4]
        target[2::3] = row[2::4]
    return width, height, bytes(output)


class EmulatorGrpcCapture:
    event_driven = True

    def __init__(self, serial: str, *, endpoint: GrpcEndpoint | None = None,
                 image_format: str = "rgb888", width: int = 0, height: int = 0,
                 row_order: str = "top-down", clock: Clock | None = None,
                 telemetry: Telemetry | None = None,
                 max_relative_lag_ms: float | None = None,
                 transport: str = "payload", max_rgb_bytes: int = 16 * 1024 * 1024):
        if image_format not in ("rgb888", "rgba8888"):
            raise ValueError("image format must be rgb888 or rgba8888")
        if width < 0 or height < 0:
            raise ValueError("requested dimensions must be nonnegative")
        if row_order not in ("top-down", "bottom-up"):
            raise ValueError("row order must be top-down or bottom-up")
        if max_relative_lag_ms is not None and max_relative_lag_ms <= 0:
            raise ValueError("relative lag limit must be positive")
        if transport not in ("payload", "mmap"):
            raise ValueError("invalid gRPC transport")
        if not 0 < max_rgb_bytes <= 16 * 1024 * 1024:
            raise ValueError("invalid maximum RGB capacity")
        self.endpoint = endpoint or discover_endpoint(serial)
        if not re.fullmatch(r"(?:127\.0\.0\.1|localhost):\d+", self.endpoint.target):
            raise ValueError("gRPC endpoint must be local loopback")
        if not self.endpoint.token:
            raise ValueError("gRPC authentication token required")
        self.clock = clock or HostClock()
        self.telemetry = telemetry
        self.image_format = image_format
        self.row_order = row_order
        self.transport = transport
        self.consistency = "unverified" if transport == "mmap" else "payload"
        self.max_rgb_bytes = max_rgb_bytes
        self._mmap_file = None
        self._mmap_path = None
        self._mmap = None
        self.last_notification_ns = 0
        self.last_snapshot_copy_started_ns = 0
        self.last_snapshot_copy_complete_ns = 0
        self.width, self.height = width, height
        self.max_relative_lag_ns = (round(max_relative_lag_ms * 1e6)
                                    if max_relative_lag_ms is not None else None)
        self._lock = threading.Lock()
        self._stats_lock = threading.Lock()
        self._channel = None
        self._call = None
        self._closed = False
        self._sequence = 0
        self._last_source_sequence: int | None = None
        self._last_shape: tuple[int, int, int] | None = None
        self._lag_anchor: tuple[int, int] | None = None
        self._last_source_time_us: int | None = None
        self._last_source_host_ns: int | None = None
        self._last_published_ns: int | None = None
        self._frozen_timestamps = 0
        self._drop_since_ns: int | None = None
        self._drop_timer: threading.Timer | None = None
        self.inactive_frames = 0
        self.inactive = False
        self.source_gaps = 0
        self.relative_stale_drops = 0
        self.invalid_frames = 0

    def counters(self) -> dict[str, int]:
        with self._stats_lock:
            return {"inactive_frames": self.inactive_frames,
                    "invalid_frames": self.invalid_frames,
                    "source_gaps": self.source_gaps,
                    "relative_stale_drops": self.relative_stale_drops}

    def _open(self):
        try:
            import grpc
            from .emulator_proto import emulator_controller_pb2 as pb
        except ImportError as error:
            raise RuntimeError("gRPC capture requires pip install '.[emulator-grpc]'") from error
        channel = grpc.insecure_channel(self.endpoint.target,
                                        options=[("grpc.max_receive_message_length", 80 * 1024 * 1024)])
        request = pb.ImageFormat(format=pb.ImageFormat.RGB888 if self.image_format == "rgb888"
                                 else pb.ImageFormat.RGBA8888,
                                 width=self.width, height=self.height)
        if self.transport == "mmap":
            # A real file URI is required by the emulator; multiprocessing
            # SharedMemory names are not emulator handles. The file is private
            # to this source and never resized while the stream may write it.
            raw_capacity = min(32 * 1024 * 1024, (self.max_rgb_bytes * 4 + 2) // 3)
            fd, path = tempfile.mkstemp(prefix="pas-emulator-mmap-", suffix=".bin")
            file = None
            try:
                os.fchmod(fd, 0o600) if hasattr(os, "fchmod") else None
                os.ftruncate(fd, raw_capacity)
                file = os.fdopen(fd, "r+b", buffering=0)
                mapping = mmap.mmap(file.fileno(), raw_capacity, access=mmap.ACCESS_READ)
            except Exception:
                if file is not None:
                    file.close()
                else:
                    os.close(fd)
                os.unlink(path)
                raise
            self._mmap_file, self._mmap_path, self._mmap = file, path, mapping
            request.transport.channel = pb.ImageTransport.MMAP
            request.transport.handle = Path(path).as_uri()
        rpc = channel.unary_stream(
            "/android.emulation.control.EmulatorController/streamScreenshot",
            request_serializer=lambda message: message.SerializeToString(),
            response_deserializer=pb.Image.FromString,
        )
        call = rpc(request, metadata=(("authorization", f"Bearer {self.endpoint.token}"),))
        with self._lock:
            if self._closed:
                call.cancel()
                channel.close()
                raise RuntimeError("capture source closed")
            self._channel, self._call = channel, call
        return call

    def capture(self) -> tuple[Frame, int]:
        with self._lock:
            if self._closed:
                raise RuntimeError("capture source closed")
            call = self._call
        if call is None:
            call = self._open()
        try:
            import grpc
            while True:
                wait_start_ns = self.clock.now_ns()
                try:
                    image = next(call)
                except grpc.RpcError as error:
                    raise RuntimeError(f"gRPC screenshot stream failed: {error.code().name}") from None
                except StopIteration:
                    raise RuntimeError("gRPC screenshot stream ended") from None
                complete_ns = self.clock.now_ns()
                self.last_notification_ns = complete_ns
                if image.format.width == 0 and image.format.height == 0:
                    with self._stats_lock:
                        self.inactive_frames += 1
                    self.inactive = True
                    if self.telemetry:
                        self.telemetry.record("capture_inactive", monotonic_ns=complete_ns,
                                              source_sequence=image.seq)
                    continue
                requested = 2 if self.image_format == "rgb888" else 1
                try:
                    if self.transport == "mmap":
                        if image.image:
                            raise ValueError("MMAP reply unexpectedly contained payload bytes")
                        raw_size = image.format.width * image.format.height * (3 if requested == 2 else 4)
                        if (raw_size <= 0 or self._mmap is None or raw_size > len(self._mmap) or
                                image.format.width * image.format.height * 3 > self.max_rgb_bytes):
                            raise ValueError("MMAP image exceeds mapped capacity")
                        self.last_snapshot_copy_started_ns = self.clock.now_ns()
                        source_bytes = self._mmap[:raw_size]
                        self.last_snapshot_copy_complete_ns = self.clock.now_ns()
                        width, height, rgb = rgb24_from_image(image, requested, self.row_order,
                                                              source_bytes)
                    else:
                        if len(image.image) > self.max_rgb_bytes * 4 // 3 + 4:
                            raise ValueError("payload exceeds configured capacity")
                        width, height, rgb = rgb24_from_image(image, requested, self.row_order)
                except ValueError as error:
                    with self._stats_lock:
                        self.invalid_frames += 1
                    if self.telemetry:
                        self.telemetry.record("capture_invalid", reason=str(error),
                                              monotonic_ns=complete_ns,
                                              source_sequence=image.seq)
                    raise
                if self.transport == "mmap":
                    complete_ns = self.last_snapshot_copy_complete_ns
                ready_ns = self.clock.now_ns()
                self.inactive = False
                source_sequence = int(image.seq)
                previous = self._last_source_sequence
                if previous is not None and source_sequence > previous + 1:
                    gap = source_sequence - previous - 1
                    with self._stats_lock:
                        self.source_gaps += gap
                    if self.telemetry:
                        self.telemetry.record("capture_source_gap", monotonic_ns=complete_ns,
                                              previous_source_sequence=previous,
                                              source_sequence=source_sequence, gap=gap)
                if previous is not None and source_sequence <= previous:
                    raise RuntimeError("emulator source sequence reset; restart capture source")
                self._last_source_sequence = source_sequence
                source_timestamp_us = int(image.timestampUs) or None
                if self.max_relative_lag_ns is not None:
                    if source_timestamp_us is None:
                        raise RuntimeError("source Unix timestamp missing; relative lag guard untrusted")
                    previous_us = self._last_source_time_us
                    previous_host_ns = self._last_source_host_ns
                    if previous_us is not None:
                        source_delta_ns = (source_timestamp_us - previous_us) * 1000
                        host_delta_ns = complete_ns - previous_host_ns
                        if source_delta_ns < 0:
                            raise RuntimeError("source Unix timestamp moved backwards; restart capture source")
                        if source_delta_ns == 0:
                            self._frozen_timestamps += 1
                            if self._frozen_timestamps >= 3 or host_delta_ns >= 250_000_000:
                                raise RuntimeError("source Unix timestamp stopped; restart capture source")
                        else:
                            self._frozen_timestamps = 0
                        if source_delta_ns > max(1_000_000_000, host_delta_ns + 500_000_000):
                            raise RuntimeError("source Unix timestamp jumped forward; restart capture source")
                    self._last_source_time_us = source_timestamp_us
                    self._last_source_host_ns = complete_ns
                    if self._lag_anchor is None:
                        self._lag_anchor = (complete_ns, source_timestamp_us)
                    anchor_host_ns, anchor_source_us = self._lag_anchor
                    relative_lag_ns = ((complete_ns - anchor_host_ns) -
                                       (source_timestamp_us - anchor_source_us) * 1000)
                    if relative_lag_ns > self.max_relative_lag_ns:
                        with self._stats_lock:
                            self.relative_stale_drops += 1
                        with self._lock:
                            if self._drop_since_ns is None and not self._closed:
                                self._drop_since_ns = complete_ns
                                self._drop_timer = threading.Timer(1.0, self._cancel_stalled_drops)
                                self._drop_timer.daemon = True
                                self._drop_timer.start()
                        if self.telemetry:
                            self.telemetry.record("capture_relative_stale_drop",
                                                  frame_sequence=self._sequence,
                                                  source_sequence=source_sequence,
                                                  relative_lag_ms=relative_lag_ns / 1e6,
                                                  monotonic_ns=complete_ns)
                        if (self._last_published_ns is not None and
                                complete_ns - self._last_published_ns > 1_000_000_000):
                            raise RuntimeError("relative lag guard made no progress for 1 second")
                        continue
                shape = (width, height, int(image.format.rotation.rotation))
                if self._last_shape is not None and shape != self._last_shape and self.telemetry:
                    self.telemetry.record("capture_geometry_changed", previous=self._last_shape,
                                          current=shape, monotonic_ns=ready_ns)
                self._last_shape = shape
                frame = Frame(self._sequence, width, height, rgb, complete_ns,
                              pixels_ready_ns=ready_ns, source_sequence=source_sequence,
                              stream_generation=0,
                              source_timestamp_us=source_timestamp_us,
                              source_rotation=shape[2],
                              notification_received_ns=self.last_notification_ns,
                              snapshot_copy_started_ns=(self.last_snapshot_copy_started_ns
                                                        if self.transport == "mmap" else None),
                              snapshot_copy_complete_ns=(self.last_snapshot_copy_complete_ns
                                                         if self.transport == "mmap" else None))
                self._sequence += 1
                self._last_published_ns = ready_ns
                with self._lock:
                    self._drop_since_ns = None
                    if self._drop_timer is not None:
                        self._drop_timer.cancel()
                        self._drop_timer = None
                return frame, wait_start_ns
        except Exception:
            raise

    def close(self) -> None:
        with self._lock:
            self._closed = True
            call, channel = self._call, self._channel
            self._call = self._channel = None
            if self._drop_timer is not None:
                self._drop_timer.cancel()
                self._drop_timer = None
        if call is not None:
            call.cancel()
        if channel is not None:
            channel.close()
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None
        if self._mmap_file is not None:
            self._mmap_file.close()
            self._mmap_file = None
        if self._mmap_path is not None:
            Path(self._mmap_path).unlink(missing_ok=True)
            self._mmap_path = None

    def _cancel_stalled_drops(self) -> None:
        with self._lock:
            if self._closed or self._drop_since_ns is None:
                return
            remaining_ns = 1_000_000_000 - (self.clock.now_ns() - self._drop_since_ns)
            if remaining_ns > 0:
                self._drop_timer = threading.Timer(remaining_ns / 1e9, self._cancel_stalled_drops)
                self._drop_timer.daemon = True
                self._drop_timer.start()
                return
            if self._call is not None:
                self._call.cancel()

    def probe_health(self, last_frame: Frame, timeout_s: float = 0.5) -> str:
        """Check transport and visible pixels without publishing or dating an old frame."""
        from .emulator_proto import emulator_controller_pb2 as pb
        with self._lock:
            channel = self._channel
            if self._closed or channel is None:
                return "failed"
        request = pb.ImageFormat(format=pb.ImageFormat.RGB888 if self.image_format == "rgb888"
                                 else pb.ImageFormat.RGBA8888,
                                 width=self.width, height=self.height)
        rpc = channel.unary_unary(
            "/android.emulation.control.EmulatorController/getScreenshot",
            request_serializer=lambda message: message.SerializeToString(),
            response_deserializer=pb.Image.FromString)
        try:
            image = rpc(request, timeout=timeout_s,
                        metadata=(("authorization", f"Bearer {self.endpoint.token}"),))
            width, height, rgb = rgb24_from_image(image, request.format, self.row_order)
        except Exception:
            return "failed"
        if width != last_frame.width or height != last_frame.height or rgb != last_frame.rgb:
            return "changed"
        return "static"
