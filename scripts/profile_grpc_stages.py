"""Controlled gRPC stage costs on the pixel-only Android fixture.

This is a diagnostic receiver, not a CaptureSource or gameplay input. Each run
opens one authenticated stream and retains only bounded timing samples.
"""

import argparse
from collections import deque
import json
from itertools import pairwise
from pathlib import Path
import time

import grpc

from pas.capture_grpc import discover_endpoint, rgb24_from_image
from pas.contracts import Frame
from pas.emulator_proto import emulator_controller_pb2 as pb
from pas.fixture import read_fixture_counter
from pas.telemetry import distribution


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--mode", choices=("payload", "protobuf", "rgb", "fixture"), required=True)
    parser.add_argument("--warmup-s", type=float, default=2)
    parser.add_argument("--duration-s", type=float, default=10)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    if args.duration_s <= 0 or args.warmup_s < 0 or args.log.exists():
        parser.error("positive duration, nonnegative warmup, and a new log are required")
    endpoint = discover_endpoint(args.serial)
    channel = grpc.insecure_channel(endpoint.target,
                                    options=[("grpc.max_receive_message_length", 80 * 1024 * 1024)])
    request = pb.ImageFormat(format=pb.ImageFormat.RGB888)
    decoder = (lambda data: data) if args.mode == "payload" else pb.Image.FromString
    rpc = channel.unary_stream(
        "/android.emulation.control.EmulatorController/streamScreenshot",
        request_serializer=lambda message: message.SerializeToString(),
        response_deserializer=decoder)
    call = rpc(request, metadata=(("authorization", f"Bearer {endpoint.token}"),))
    arrivals = deque(maxlen=100_000)
    conversion = deque(maxlen=100_000)
    first_counter = last_counter = None
    first_counter_ns = last_counter_ns = None
    different = 0
    count = total_bytes = 0
    try:
        next(call)  # Readiness is outside warmup and formal window.
        warmup_end = time.monotonic_ns() + round(args.warmup_s * 1e9)
        while time.monotonic_ns() < warmup_end:
            next(call)
        start_ns = time.monotonic_ns()
        cpu_start_ns = time.process_time_ns()
        deadline_ns = start_ns + round(args.duration_s * 1e9)
        while time.monotonic_ns() < deadline_ns:
            image = next(call)
            complete_ns = time.monotonic_ns()
            if complete_ns >= deadline_ns:
                break
            if args.mode == "payload":
                total_bytes += len(image)
            else:
                total_bytes += len(image.image)
                if args.mode in ("rgb", "fixture"):
                    width, height, rgb = rgb24_from_image(image, 2)
                    if args.mode == "fixture":
                        frame = Frame(count, width, height, rgb, complete_ns)
                        visible = read_fixture_counter(frame)
                        if visible is not None:
                            if first_counter is None:
                                first_counter, first_counter_ns = visible, complete_ns
                            if last_counter is not None and visible != last_counter:
                                different += 1
                            last_counter, last_counter_ns = visible, complete_ns
            conversion.append((time.monotonic_ns() - complete_ns) / 1e6)
            arrivals.append(complete_ns)
            count += 1
        cpu_end_ns = time.process_time_ns()
        end_ns = time.monotonic_ns()
    finally:
        call.cancel()
        channel.close()
    summary = {
        "mode": args.mode, "serial": args.serial, "endpoint": endpoint.target,
        "warmup_s": args.warmup_s, "duration_requested_s": args.duration_s,
        "start_ns": start_ns, "end_ns": end_ns,
        "duration_actual_s": (end_ns - start_ns) / 1e9,
        "frames_received": count, "payload_bytes": total_bytes,
        "frames_per_wall_second": count * 1e9 / (end_ns - start_ns),
        "arrival_interval_ms": distribution([(b - a) / 1e6 for a, b in pairwise(arrivals)]),
        "post_receive_work_ms": distribution(conversion),
        "cpu_process_seconds": (cpu_end_ns - cpu_start_ns) / 1e9,
        "cpu_one_core_percent": (cpu_end_ns - cpu_start_ns) / (end_ns - start_ns) * 100,
        "fixture_counter_changes": different,
        "fixture_counter_rate_hz": ((last_counter - first_counter) * 1e9 /
                                    (last_counter_ns - first_counter_ns)
                                    if first_counter is not None and last_counter_ns > first_counter_ns
                                    else None),
        "source_frame_age": "unknown; Unix source timestamp not mapped to host monotonic",
        "distribution_scope": "latest 100000 samples",
    }
    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
