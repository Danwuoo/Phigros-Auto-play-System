"""Opt-in, observation-only Android Emulator gRPC PNG/PCM recorder."""
import argparse
import csv
import hashlib
import importlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time
import wave


def private_directory(path):
    path.mkdir(mode=0o700)  # Existing paths, including empty directories, are refused.
    try:
        if os.name == "nt":
            options = dict(capture_output=True, check=True, creationflags=subprocess.CREATE_NO_WINDOW)
            sid = next(csv.reader(subprocess.run(
                ["whoami", "/user", "/fo", "csv", "/nh"], text=True, **options).stdout.splitlines()))[1]
            subprocess.run(["icacls", str(path), "/inheritance:r", "/grant:r",
                            f"*{sid}:(OI)(CI)F"], **options)
    except BaseException:
        path.rmdir()
        raise


class BudgetExceeded(RuntimeError):
    pass


class Budget:
    def __init__(self, root, max_bytes, max_frames, min_free_bytes):
        self.root, self.limit, self.frame_limit = root, max_bytes, max_frames
        self.min_free, self.bytes, self.frames = min_free_bytes, 0, 0
        self.lock = threading.Lock()

    def take(self, size, frame=False):
        with self.lock:
            if (self.bytes + size > self.limit or self.frames + int(frame) > self.frame_limit
                    or shutil.disk_usage(self.root).free - size < self.min_free):
                raise BudgetExceeded("Recording budget exhausted")
            self.bytes += size
            self.frames += int(frame)


def json_line(row):
    return (json.dumps(row, separators=(",", ":")) + "\n").encode("utf-8")


class Timeline:
    def __init__(self):
        self.count = self.backwards = self.missing_timestamps = self.sequence_gaps = self.sequence_regressions = 0
        self.first_source = self.last_source = self.first_arrival = self.last_arrival = self.last_sequence = None
        self.max_arrival_gap_ns = self.max_source_gap_us = self.audio_gap_us = self.last_samples = 0

    def add(self, source, arrival, sequence=None, samples=0):
        if self.count:
            delta = source - self.last_source
            self.backwards += int(delta < 0)
            self.max_source_gap_us = max(self.max_source_gap_us, delta)
            self.max_arrival_gap_ns = max(self.max_arrival_gap_ns, arrival - self.last_arrival)
            if samples:
                self.audio_gap_us += max(0, delta - self.last_samples * 1000000 / 48000)
            if sequence is not None:
                self.sequence_gaps += max(0, sequence - self.last_sequence - 1)
                self.sequence_regressions += int(sequence <= self.last_sequence)
        else:
            self.first_source, self.first_arrival = source, arrival
        self.count += 1
        self.missing_timestamps += int(source == 0)
        self.last_source, self.last_arrival, self.last_sequence, self.last_samples = source, arrival, sequence, samples

    def summary(self):
        return dict(messages=self.count, first_source_timestamp_us=self.first_source,
                    last_source_timestamp_us=self.last_source, first_arrival_ns=self.first_arrival,
                    last_arrival_ns=self.last_arrival, source_timestamp_regressions=self.backwards,
                    zero_source_timestamps=self.missing_timestamps, sequence_gaps=self.sequence_gaps,
                    sequence_regressions=self.sequence_regressions,
                    max_source_timestamp_gap_us=self.max_source_gap_us,
                    max_arrival_gap_ns=self.max_arrival_gap_ns, estimated_audio_gap_us=self.audio_gap_us)


class Recording:
    def __init__(self, root, max_bytes, max_frames, min_free_bytes, rgb_row_order=None):
        self.root = Path(root)
        private_directory(self.root)
        self.budget = Budget(self.root, max_bytes, max_frames, min_free_bytes)
        self.frames = self.audio_packets = self.sample_frames = 0
        self.video_timing, self.audio_timing, self.rgb_row_order = Timeline(), Timeline(), rgb_row_order
        self.handles = []
        try:
            self.budget.take(44 + 16384)  # WAV header and bounded final summary reserve.
            (self.root / "frames").mkdir(mode=0o700)
            self.frame_index = self.open("frame-index.jsonl")
            self.audio_index = self.open("audio-index.jsonl")
            self.wav = wave.open(self.open("audio.wav"), "wb")
            self.wav.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        except BaseException:
            self.close()
            raise

    def open(self, name):
        handle = (self.root / name).open("xb")
        self.handles.append(handle)
        return handle

    def frame(self, packet, arrival_ns):
        from PIL import Image
        width, height = packet.format.width or packet.width, packet.format.height or packet.height
        if not width or not height or width * height > 16777216:
            raise ValueError("Invalid screenshot dimensions")
        payload = bytes(packet.image)
        if packet.format.format == 0:  # Server-encoded PNG: preserve original bytes.
            with Image.open(io.BytesIO(payload)) as image:
                if image.format != "PNG" or image.size != (width, height):
                    raise ValueError("Screenshot PNG metadata mismatch")
                image.verify()
            with Image.open(io.BytesIO(payload)) as image:
                image.load()  # verify() alone does not validate IDAT decompression.
            encoding = "server_png"
        elif packet.format.format == 2 and len(payload) == width * height * 3:
            if self.rgb_row_order not in ("top-down", "bottom-up"):
                raise ValueError("RGB888 requires explicit validated row order")
            image = Image.frombytes("RGB", (width, height), payload, "raw", "RGB", 0,
                                    1 if self.rgb_row_order == "top-down" else -1)
            buffer = io.BytesIO()
            image.save(buffer, format="PNG", compress_level=1)
            payload, encoding = buffer.getvalue(), "rgb888_" + self.rgb_row_order + "_to_png"
        else:
            raise ValueError("Unsupported screenshot payload")
        name = f"frames/frame-{self.frames:06d}.png"
        rotation = packet.format.rotation
        row = dict(ordinal=self.frames, file=name, source_timestamp_us=int(packet.timestampUs),
                   source_sequence=int(packet.seq), arrival_ns=arrival_ns, width=width, height=height,
                   rotation=dict(value=int(rotation.rotation), x=rotation.xAxis,
                                 y=rotation.yAxis, z=rotation.zAxis), encoding=encoding,
                   bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest())
        line = json_line(row)
        self.budget.take(len(payload) + len(line), frame=True)
        with (self.root / name).open("xb") as output:
            output.write(payload)
        self.frame_index.write(line)
        self.video_timing.add(packet.timestampUs, arrival_ns, sequence=packet.seq)
        self.frames += 1

    def audio(self, packet, arrival_ns):
        fmt, payload = packet.format, bytes(packet.audio)
        if (fmt.samplingRate, fmt.channels, fmt.format) != (48000, 1, 1) or len(payload) % 4:
            raise ValueError("Audio must be 48000 Hz stereo little-endian S16")
        if len(payload) > 1048576:
            raise ValueError("Oversized audio packet")
        row = dict(ordinal=self.audio_packets, source_timestamp_us=int(packet.timestamp),
                   arrival_ns=arrival_ns, sample_frame_offset=self.sample_frames,
                   sample_frames=len(payload) // 4, bytes=len(payload),
                   sha256=hashlib.sha256(payload).hexdigest())
        line = json_line(row)
        self.budget.take(len(payload) + len(line))
        self.wav.writeframesraw(payload)
        self.audio_index.write(line)
        self.audio_timing.add(packet.timestamp, arrival_ns, samples=len(payload) // 4)
        self.sample_frames += len(payload) // 4
        self.audio_packets += 1

    def close(self):
        error = None
        for handle in [getattr(self, "wav", None)] + list(reversed(self.handles)):
            if handle is not None:
                try:
                    handle.close()
                except Exception as exc:
                    error = error or exc
        if error:
            raise error


def clock_anchor():
    before = time.perf_counter_ns()
    wall = time.time_ns()
    return dict(perf_counter_before_ns=before, unix_time_ns=wall, perf_counter_after_ns=time.perf_counter_ns())


def capture(recording, screenshot_stream, audio_stream, duration, shutdown_timeout=5):
    stop, faults, lock = threading.Event(), [], threading.Lock()
    started = time.perf_counter_ns()
    start_anchor = clock_anchor()

    def fault(label, exc):
        # Never persist exception text: transport diagnostics may contain credentials.
        code = exc.code().name if callable(getattr(exc, "code", None)) else type(exc).__name__
        with lock:
            faults.append(dict(stream=label, code=code))
        stop.set()

    def worker(label, stream, write):
        try:
            for packet in stream:
                arrival = time.perf_counter_ns()
                if stop.is_set():
                    break
                write(packet, arrival)
            if not stop.is_set():
                fault(label, RuntimeError("Stream ended before deadline"))
        except Exception as exc:
            code = exc.code().name if callable(getattr(exc, "code", None)) else ""
            if not (stop.is_set() and code in ("CANCELLED", "DEADLINE_EXCEEDED")):
                fault(label, exc)

    threads = [threading.Thread(target=worker, args=args, daemon=True) for args in
               [("screenshots", screenshot_stream, recording.frame), ("audio", audio_stream, recording.audio)]]
    deadline, interrupted = False, False
    try:
        for thread in threads:
            thread.start()
        deadline = not stop.wait(duration)
    except KeyboardInterrupt:
        interrupted = True
    except Exception as exc:
        fault("startup", exc)
    finally:
        stop.set()
        for stream in (screenshot_stream, audio_stream):
            try:
                stream.cancel()
            except Exception as exc:
                fault("cancel", exc)
        join_deadline = time.monotonic() + shutdown_timeout
        for thread in threads:
            if thread.ident is not None:
                thread.join(max(0, join_deadline - time.monotonic()))
        files_closed = not any(thread.is_alive() for thread in threads)
        if files_closed:
            try:
                recording.close()
            except Exception as exc:
                files_closed = False
                fault("disk_close", exc)
        else:
            fault("worker_shutdown", TimeoutError())
    status = "recorded" if deadline and not faults else ("interrupted" if interrupted and not faults else "failed")
    if not recording.frames or not recording.sample_frames:
        status = "incomplete" if status == "recorded" else status
    video, sound = recording.video_timing, recording.audio_timing
    pcm_seconds = recording.sample_frames / 48000
    audio_span = ((sound.last_source - sound.first_source) / 1000000 + sound.last_samples / 48000) if sound.count else 0
    arrival_span = ((sound.last_arrival - sound.first_arrival) / 1000000000) if sound.count else 0
    audio_ratio = pcm_seconds / audio_span if audio_span > 0 else None
    frame_rate = (video.count - 1) * 1000000000 / (video.last_arrival - video.first_arrival) if video.count > 1 and video.last_arrival > video.first_arrival else 0
    warnings = ["Source completeness and audio/video phase have not been verified."]
    if audio_ratio is not None and not 0.9 <= audio_ratio <= 1.1:
        warnings.append("PCM sample duration differs from source timestamp span by more than 10%.")
    if frame_rate < 30:
        warnings.append("Received screenshot rate is below 30 frames per second.")
    if video.sequence_gaps or video.sequence_regressions or video.backwards or sound.backwards:
        warnings.append("Source sequence gaps or timestamp/sequence regressions observed.")
    if video.missing_timestamps or sound.missing_timestamps:
        warnings.append("Zero source timestamps observed; timing estimates may be unavailable.")
    ended = time.perf_counter_ns()
    summary = dict(schema_version=1, status=status, duration_requested_seconds=duration,
                   capture_only=True, started_arrival_clock_ns=started, ended_arrival_clock_ns=ended,
                   capture_elapsed_seconds=(ended - started) / 1000000000,
                   frames=recording.frames, audio_packets=recording.audio_packets,
                   audio_sample_frames=recording.sample_frames, audio_format="48000Hz stereo S16LE",
                   files_closed=files_closed, clock_anchors=[start_anchor, clock_anchor()],
                   video_timing=video.summary(), audio_timing=sound.summary(),
                   received_frame_rate=frame_rate, audio_pcm_seconds=pcm_seconds,
                   audio_source_span_seconds=audio_span, audio_arrival_span_seconds=arrival_span,
                   audio_pcm_source_span_ratio=audio_ratio, quality_warnings=warnings,
                   audio_pcm_arrival_span_ratio=pcm_seconds / arrival_span if arrival_span > 0 else None,
                   audio_quality="sparse_received_pcm" if audio_ratio is not None and audio_ratio < 0.9 else "unverified",
                   wav_layout="received PCM concatenated; gaps not filled; consult audio-index.jsonl",
                   budget_accounted_bytes=recording.budget.bytes, faults=faults,
                   proto_sha256=hashlib.sha256((Path(__file__).parents[1] / "proto/emulator_controller.proto").read_bytes()).hexdigest(),
                   source_timestamps="emulator Unix epoch microseconds; preserved unmodified",
                   arrival_timestamps="host perf_counter nanoseconds; separate clock domain",
                   coverage="received RPC messages only; source completeness and A/V phase unverified")
    with (recording.root / "summary.json").open("xb") as output:
        output.write(json_line(summary))
    return summary


def read_token(token_file=None, discovery_file=None):
    if token_file:
        token = Path(token_file).read_text(encoding="utf-8-sig").strip()
    else:
        candidates = [Path(discovery_file)] if discovery_file else list(
            (Path(os.environ.get("TEMP", ".")) / "avd/running").glob("pid_*.ini"))
        tokens = []
        for path in candidates:
            values = dict(line.strip().split("=", 1) for line in path.read_text().splitlines()
                          if "=" in line and not line.lstrip().startswith(("#", ";")))
            if values.get("grpc.port") == "8554" and values.get("grpc.token"):
                tokens.append(values["grpc.token"])
        if len(tokens) != 1:
            raise ValueError("Supply one token file or matching emulator discovery file")
        token = tokens[0]
    if not token or "\n" in token or "\r" in token:
        raise ValueError("Invalid token file")
    return token


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--duration", type=float, required=True)
    parser.add_argument("--bindings-dir", type=Path, required=True)
    parser.add_argument("--address", default="127.0.0.1:8554", choices=["127.0.0.1:8554"])
    auth = parser.add_mutually_exclusive_group()
    auth.add_argument("--token-file", type=Path)
    auth.add_argument("--discovery-file", type=Path)
    auth.add_argument("--allow-unauthenticated", action="store_true")
    parser.add_argument("--image-format", choices=["PNG", "RGB888"], default="PNG")
    parser.add_argument("--rgb-row-order", choices=["top-down", "bottom-up"])
    parser.add_argument("--max-mib", type=int, default=1024)
    parser.add_argument("--max-frames", type=int, default=12000)
    parser.add_argument("--min-free-mib", type=int, default=128)
    args = parser.parse_args(argv)
    if not 2 <= args.duration <= 180 or not 1 <= args.max_mib <= 8192 or not 1 <= args.max_frames <= 36000 or args.min_free_mib < 0:
        parser.error("duration 2..180s; max-mib 1..8192; max-frames 1..36000; min-free-mib >= 0")
    if args.image_format == "RGB888" and not args.rgb_row_order:
        parser.error("RGB888 requires --rgb-row-order after checking actual emulator output")
    import grpc
    sys.path.insert(0, str(args.bindings_dir.resolve()))
    pb = importlib.import_module("emulator_controller_pb2")
    service = importlib.import_module("emulator_controller_pb2_grpc")
    metadata = [] if args.allow_unauthenticated else [("authorization", "Bearer " + read_token(args.token_file, args.discovery_file))]
    with grpc.insecure_channel(args.address, options=[("grpc.max_receive_message_length", 33554432)]) as channel:
        stub = service.EmulatorControllerStub(channel)
        recording = Recording(args.output, args.max_mib * 1048576, args.max_frames, args.min_free_mib * 1048576, args.rgb_row_order)
        screenshots = stub.streamScreenshot(pb.ImageFormat(format=getattr(pb.ImageFormat, args.image_format)), metadata=metadata, timeout=args.duration + 5)
        audio = stub.streamAudio(pb.AudioFormat(samplingRate=48000, channels=1, format=1), metadata=metadata, timeout=args.duration + 5)
        summary = capture(recording, screenshots, audio, args.duration)
    print(json.dumps(summary))
    return 0 if summary["status"] == "recorded" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps({"status": "failed", "error_type": type(error).__name__}), file=sys.stderr)
        raise SystemExit(2)
