"""Bounded loopback-only transport comparison; never discovers an emulator.

Run from the repository root with PYTHONPATH=src. JSONL is the recomputation
source; samples represent harness cost, not emulator latency or source age.
"""

import argparse
from collections import deque
from concurrent.futures import ThreadPoolExecutor
import json
import mmap
from pathlib import Path
import tempfile
import threading
import time
from urllib.parse import unquote, urlparse

from pas.capture_grpc import EmulatorGrpcCapture, GrpcEndpoint
from pas.capture_process import ProcessCaptureConfig
from pas.cli import process_capture_bench
from pas.telemetry import distribution


def _path_from_file_uri(uri):
    parsed = urlparse(uri)
    if parsed.scheme != "file":
        raise ValueError("expected file URI")
    path = unquote(parsed.path)
    return path[1:] if len(path) >= 3 and path[0] == "/" and path[2] == ":" else path


def start_server(width, height, interval_ms, token):
    import grpc
    from pas.emulator_proto import emulator_controller_pb2 as pb
    server = grpc.server(ThreadPoolExecutor(max_workers=4),
                         options=[("grpc.max_send_message_length", 80 * 1024 * 1024)])

    def stream(request, context):
        if dict(context.invocation_metadata()).get("authorization") != "Bearer " + token:
            context.abort(grpc.StatusCode.UNAUTHENTICATED, "invalid authentication")
        if request.format != pb.ImageFormat.RGB888:
            context.abort(grpc.StatusCode.INVALID_ARGUMENT, "RGB only")
        mapping = file = None
        try:
            if request.transport.channel == pb.ImageTransport.MMAP:
                file = open(_path_from_file_uri(request.transport.handle), "r+b")
                mapping = mmap.mmap(file.fileno(), 0)
            for sequence in range(100000):
                if not context.is_active():
                    break
                value = sequence % 256
                pixels = bytes((value, value ^ 255, 33)) * (width * height)
                image = pb.Image(seq=sequence, timestampUs=time.time_ns() // 1000)
                image.format.width = width
                image.format.height = height
                image.format.format = pb.ImageFormat.RGB888
                if mapping is not None:
                    mapping[:len(pixels)] = pixels
                else:
                    image.image = pixels
                yield image
                time.sleep(interval_ms / 1000)
        finally:
            if mapping is not None:
                mapping.close()
            if file is not None:
                file.close()

    handler = grpc.unary_stream_rpc_method_handler(
        stream, request_deserializer=pb.ImageFormat.FromString,
        response_serializer=lambda value: value.SerializeToString())
    server.add_generic_rpc_handlers((grpc.method_handlers_generic_handler(
        "android.emulation.control.EmulatorController", {"streamScreenshot": handler}),))
    port = server.add_insecure_port("127.0.0.1:0")
    server.start()
    return server, f"127.0.0.1:{port}"


def thread_run(endpoint, token, duration_s, warmup_s, consumer_delay_ms, log,
               parent_load=False, instrument_log_cost=True):
    from pas.capture import CaptureWorker
    from pas.clock import HostClock
    from pas.telemetry import Telemetry
    source = EmulatorGrpcCapture("emulator-0000", endpoint=GrpcEndpoint(endpoint, token, "loopback"))
    clock = HostClock()
    arrivals = []
    load_stop = threading.Event()
    def spin():
        value = 0
        while not load_stop.is_set():
            value = (value * 3 + 1) & 65535
    load_thread = threading.Thread(target=spin, daemon=True) if parent_load else None
    with Telemetry(log, instrument_write_cost=instrument_log_cost) as telemetry:
        telemetry.record("run_config", source_kind="loopback", execution="thread",
                         transport="payload", consistency="payload", duration_s=duration_s,
                         warmup_s=warmup_s, consumer_delay_ms=consumer_delay_ms,
                         parent_load=parent_load)
        worker = CaptureWorker(source, clock, telemetry)
        worker.start()
        try:
            last = worker.wait_for_valid_frames(1, 10)
            if load_thread:
                load_thread.start()
            time.sleep(warmup_s)
            boundary = worker.latest.peek()
            last = boundary.sequence if boundary else last
            start_ns = clock.now_ns()
            end_ns = start_ns + round(duration_s * 1e9)
            telemetry.record("bench_phase", phase="MEASURING", monotonic_ns=start_ns)
            while clock.now_ns() < end_ns:
                frame = worker.latest.read_after(last, 0.05)
                if worker.error:
                    raise RuntimeError("thread loopback failed")
                if frame is None or frame.capture_complete_ns < start_ns:
                    continue
                last = frame.sequence
                arrivals.append(frame.capture_complete_ns)
                if consumer_delay_ms:
                    time.sleep(consumer_delay_ms / 1000)
            actual_end = clock.now_ns()
            telemetry.record("bench_phase", phase="STOPPING", monotonic_ns=actual_end)
        finally:
            load_stop.set()
            if load_thread and load_thread.ident is not None:
                load_thread.join(0.5)
            worker.stop(join_timeout_s=2)
        telemetry.flush()
        produced_times = deque(maxlen=100_000)
        produced_count = 0
        for line in Path(log).open(encoding="utf-8"):
            event = json.loads(line)
            if event["event"] == "capture" and start_ns <= event["capture_complete_ns"] < actual_end:
                produced_count += 1
                produced_times.append(event["capture_complete_ns"])
        produced_times = list(produced_times)
        summary = {"execution": "thread", "transport": "payload", "source_kind": "loopback",
                   "parent_load": parent_load,
                   "frames_received": produced_count, "frames_read": len(arrivals),
                   "measurement_start_ns": start_ns,
                   "measurement_end_ns": actual_end,
                   "arrival_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(produced_times, produced_times[1:])]),
                   "consumer_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(arrivals, arrivals[1:])]),
                   "log_write_cost_ms": telemetry.write_cost_distribution(start_ns, actual_end),
                   "source_frame_age": "unknown", "log_path": str(log)}
        telemetry.record("summary", **summary)
        return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration-s", type=float, default=2)
    parser.add_argument("--warmup-s", type=float, default=0.5)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--interval-ms", type=float, default=16.67)
    parser.add_argument("--consumer-delay-ms", type=float, default=0)
    parser.add_argument("--parent-load", action="store_true")
    parser.add_argument("--child-load", action="store_true")
    parser.add_argument("--no-log-cost", action="store_true")
    parser.add_argument("--output-dir", default="measurements/offline_capture_comparison")
    args = parser.parse_args()
    if args.width <= 0 or args.height <= 0 or args.width * args.height * 3 > 16 * 1024 * 1024:
        parser.error("dimensions exceed bounded IPC capacity")
    output = Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=False)
    token = "loopback-test-only"
    server, endpoint = start_server(args.width, args.height, args.interval_ms, token)
    try:
        with tempfile.TemporaryDirectory() as temp:
            token_file = Path(temp) / "token"
            token_file.write_text(token, encoding="utf-8")
            results = [thread_run(endpoint, token, args.duration_s, args.warmup_s,
                                  args.consumer_delay_ms, output / "thread-payload.jsonl",
                                  args.parent_load, not args.no_log_cost)]
            for transport in ("payload", "mmap"):
                config = ProcessCaptureConfig(kind="loopback", serial="emulator-0000",
                                              endpoint=endpoint, token_file=str(token_file),
                                              transport=transport, width=args.width, height=args.height,
                                              child_cpu_load=args.child_load)
                results.append(process_capture_bench(
                    config, args.duration_s, args.warmup_s, args.consumer_delay_ms,
                    str(output / f"process-{transport}.jsonl"),
                    diagnostic_mmap=transport == "mmap", parent_load=args.parent_load,
                    instrument_log_cost=not args.no_log_cost))
            (output / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
            print(json.dumps({"output_dir": str(output), "results": results}, indent=2))
    finally:
        server.stop(0).wait()


if __name__ == "__main__":
    main()
