"""Decode an existing media capture offline; never infer source completeness or song zero."""
import argparse
import array
from contextlib import ExitStack
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys
import wave

from record_emulator import private_directory


def line(value):
    return json.dumps(value, separators=(",", ":"), allow_nan=False) + "\n"


def timestamp(frame):
    tb = frame.time_base
    return dict(pts=frame.pts, time_base=None if tb is None else
                dict(numerator=tb.numerator, denominator=tb.denominator),
                pts_seconds=float(frame.pts * tb) if frame.pts is not None and tb is not None else None)


def audio_samples(frame):
    """Read actual decoded samples, excluding AV plane padding; no NumPy conversion."""
    formats = {"u8": ("B", 128), "s16": ("h", 0), "s32": ("i", 0),
               "s64": ("q", 0), "flt": ("f", 0), "dbl": ("d", 0)}
    base = frame.format.name.rstrip("p")
    if base not in formats:
        return dict(status="unsupported_format", format=frame.format.name,
                    samples_analyzed=0, nonzero_samples=0, nonfinite_samples=0)
    code, silence = formats[base]
    channels = len(frame.layout.channels)
    expected_planes = channels if frame.format.is_planar else 1
    if len(frame.planes) != expected_planes:
        raise ValueError("Unexpected audio plane count")
    total = nonzero = nonfinite = 0
    for plane in frame.planes:
        count = frame.samples if frame.format.is_planar else frame.samples * channels
        size = count * array.array(code).itemsize
        payload = bytes(plane)
        if len(payload) < size:
            raise ValueError("Truncated decoded audio plane")
        values = array.array(code)
        values.frombytes(payload[:size])  # Ignore FFmpeg's allocation padding.
        total += len(values)
        if code in ("f", "d"):
            nonfinite += sum(not math.isfinite(value) for value in values)
        nonzero += sum(value != silence for value in values)
    return dict(status="exact_decoded_samples", format=frame.format.name,
                samples_analyzed=total, nonzero_samples=nonzero, nonfinite_samples=nonfinite)


def save_png(frame, path, av):
    """Use FFmpeg's PNG encoder, keeping Pillow out of the media requirements."""
    encoder = av.CodecContext.create("png", "w")
    encoder.width, encoder.height, encoder.pix_fmt = frame.width, frame.height, "rgb24"
    encoder.time_base = Fraction(1, 1)
    rgb = frame.reformat(format="rgb24")
    rgb.pts, rgb.time_base = 0, Fraction(1, 1)
    packets = [*encoder.encode(rgb), *encoder.encode(None)]
    if not packets:
        raise ValueError("PNG encoder produced no packets")
    with path.open("xb") as handle:
        for packet in packets:
            handle.write(bytes(packet))


def stereo_s16le(frame):
    """Pack existing signed 16-bit stereo samples; never resample or fill gaps."""
    if (frame.sample_rate != 48000 or len(frame.layout.channels) != 2 or
            frame.layout.name != "stereo" or frame.format.name not in ("s16", "s16p")):
        raise ValueError("WAV extraction requires 48000 Hz stereo s16 or s16p frames")
    if frame.format.name == "s16":
        payload = bytes(frame.planes[0])[:frame.samples * 4]
        if len(payload) != frame.samples * 4:
            raise ValueError("Truncated signed 16-bit audio plane")
        values = array.array("h")
        values.frombytes(payload)
    else:
        channels = []
        for plane in frame.planes:
            payload = bytes(plane)[:frame.samples * 2]
            if len(payload) != frame.samples * 2:
                raise ValueError("Truncated signed 16-bit audio plane")
            channel = array.array("h")
            channel.frombytes(payload)
            channels.append(channel)
        if len(channels) != 2:
            raise ValueError("Unexpected planar stereo channel count")
        values = array.array("h", [0]) * (frame.samples * 2)
        values[0::2], values[1::2] = channels
    if sys.byteorder != "little":
        values.byteswap()
    return values.tobytes()


class Timing:
    def __init__(self):
        self.count = self.missing = self.regressions = self.duplicates = self.corrupt = 0
        self.times, self.ends = [], []

    def add(self, row, duration):
        self.count += 1
        self.corrupt += int(row["is_corrupt"])
        value = row["pts_seconds"]
        if value is None:
            self.missing += 1
            return
        if self.times:
            self.regressions += int(value < self.times[-1])
            self.duplicates += int(value == self.times[-1])
        self.times.append(value)
        self.ends.append(value + duration)

    def summary(self):
        gaps = [b - a for a, b in zip(self.times, self.times[1:])]
        return dict(decoded_frames=self.count, missing_pts=self.missing,
                    pts_regressions=self.regressions, pts_duplicates=self.duplicates,
                    pts_strictly_increasing=bool(self.times) and not (self.missing or self.regressions or self.duplicates),
                    corrupt_frames=self.corrupt, first_pts_seconds=self.times[0] if self.times else None,
                    last_pts_seconds=self.times[-1] if self.times else None,
                    source_span_seconds=max(self.ends) - min(self.times) if self.times else None,
                    median_pts_gap_seconds=statistics.median(gaps) if gaps else None,
                    max_pts_gap_seconds=max(gaps) if gaps else None)


def describe(stream):
    codec = stream.codec_context
    info = dict(index=stream.index, type=stream.type, codec=codec.name,
                time_base=None if stream.time_base is None else str(stream.time_base),
                declared_duration_ticks=stream.duration, declared_frames=stream.frames)
    if stream.type == "video":
        info.update(width=codec.width, height=codec.height,
                    pixel_format=codec.format.name if codec.format else None)
    elif stream.type == "audio":
        info.update(sample_rate=codec.sample_rate, channels=len(codec.layout.channels),
                    sample_format=codec.format.name if codec.format else None)
    return info


def analyze(source, output, max_frames=1000000, extract_audio_wav=False):
    import av
    source, output = Path(source).resolve(strict=True), Path(output)
    if not source.is_file():
        raise ValueError("Input must be a file")
    source_before = source.stat()
    digest = hashlib.sha256()
    with source.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1048576), b""):
            digest.update(chunk)
    private_directory(output)
    timings, audio_stats, streams, faults = {}, {}, [], []
    geometries = {}
    wav, wav_frames, wav_stream = None, 0, None
    summary = dict(schema_version=1, status="failed", input_sha256=digest.hexdigest(),
                   input_bytes=source_before.st_size, pyav_version=av.__version__,
                   coverage="All demuxed audio/video packets are decoded; source completeness and actual song zero are unverified.",
                   preview_note="PNG previews are decoded pixels from the input codec; H.264 input remains lossy.")
    try:
        with ExitStack() as stack, av.open(str(source)) as container, (output / "frame-index.jsonl").open("x", encoding="utf-8") as video_index, (output / "audio-frame-index.jsonl").open("x", encoding="utf-8") as audio_index:
            streams = [describe(stream) for stream in container.streams if stream.type in ("video", "audio")]
            if not streams:
                raise ValueError("Input has no audio or video streams")
            for stream in container.streams:
                if stream.type in ("video", "audio"):
                    stream.codec_context.thread_count = 2
                    timings[stream.index] = Timing()
                    if stream.type == "audio":
                        audio_stats[stream.index] = dict(sample_frames=0, pcm_seconds=0.0, samples_analyzed=0,
                            nonzero_samples=0, nonfinite_samples=0, unsupported_format_frames=0, sample_rates=set(), formats=set())
            if extract_audio_wav:
                audio_streams = [stream for stream in streams if stream["type"] == "audio"]
                if (len(audio_streams) != 1 or audio_streams[0]["sample_rate"] != 48000 or
                        audio_streams[0]["channels"] != 2):
                    raise ValueError("WAV extraction requires one 48000 Hz stereo audio stream")
                wav_stream = audio_streams[0]["index"]
                raw_wav = stack.enter_context((output / "decoded-audio.wav").open("xb"))
                wav = stack.enter_context(wave.open(raw_wav, "wb"))
                wav.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            total = 0
            for packet in container.demux():
                stream = packet.stream
                if stream.type not in ("video", "audio"):
                    continue
                for frame in packet.decode():
                    total += 1
                    if total > max_frames:
                        raise ValueError("Decoded frame budget exceeded")
                    timing = timings[stream.index]
                    row = dict(stream_index=stream.index, ordinal=timing.count,
                               is_corrupt=bool(getattr(frame, "is_corrupt", False)), **timestamp(frame))
                    if stream.type == "video":
                        duration = float(frame.duration * frame.time_base) if getattr(frame, "duration", 0) and frame.time_base else 0
                        row.update(width=frame.width, height=frame.height, pixel_format=frame.format.name,
                                   duration_ticks=getattr(frame, "duration", None))
                        geometries.setdefault(stream.index, set()).add((frame.width, frame.height))
                        video_index.write(line(row))
                    else:
                        if not frame.sample_rate:
                            raise ValueError("Audio frame has no sample rate")
                        duration = frame.samples / frame.sample_rate
                        analysis = audio_samples(frame)
                        stats = audio_stats[stream.index]
                        row.update(sample_frames=frame.samples, sample_rate=frame.sample_rate,
                                   channels=len(frame.layout.channels), sample_format=frame.format.name,
                                   sample_frame_offset=stats["sample_frames"],
                                   pcm_seconds=duration, sample_analysis=analysis)
                        if wav is not None:
                            wav.writeframesraw(stereo_s16le(frame))
                            wav_frames += frame.samples
                        stats["sample_frames"] += frame.samples
                        stats["pcm_seconds"] += duration
                        stats["sample_rates"].add(frame.sample_rate)
                        stats["formats"].add(frame.format.name)
                        for key in ("samples_analyzed", "nonzero_samples", "nonfinite_samples"):
                            stats[key] += analysis[key]
                        stats["unsupported_format_frames"] += int(analysis["status"] == "unsupported_format")
                        audio_index.write(line(row))
                    timing.add(row, duration)
        summary["status"] = "decoded"
    except Exception as exc:
        faults.append(dict(stage="decode", code=type(exc).__name__))
    summary["streams"] = streams
    summary["video"] = []
    summary["audio"] = []
    for stream in streams:
        index = stream["index"]
        result = dict(stream_index=index, **timings[index].summary())
        if stream["type"] == "video":
            result["decoded_geometries"] = sorted(geometries.get(index, set()))
            summary["video"].append(result)
        else:
            stats = audio_stats[index]
            stats["sample_rates"] = sorted(stats["sample_rates"])
            stats["formats"] = sorted(stats["formats"])
            result.update(stats)
            result["nonzero_sample_ratio"] = stats["nonzero_samples"] / stats["samples_analyzed"] if stats["samples_analyzed"] else None
            result["pcm_state"] = ("missing_pcm" if not stats["sample_frames"] else "unsupported_sample_format" if stats["unsupported_format_frames"] else "nonzero_pcm" if stats["nonzero_samples"] else "silent_pcm")
            result["sample_analysis"] = "Exact decoded native samples, excluding plane padding; u8 silence is 128, other supported formats use zero."
            summary["audio"].append(result)
            if stats["nonfinite_samples"]:
                faults.append(dict(stage="decode", code="NonfiniteAudioSamples", stream_index=index))
        if result["corrupt_frames"]:
            faults.append(dict(stage="decode", code="CorruptFrames", stream_index=index))
    if faults:
        summary["status"] = "failed"
    if summary["status"] == "decoded" and summary["video"]:
        selected = summary["video"][0]
        targets = [(0, "preview-first.png"), (selected["decoded_frames"] // 2, "preview-middle.png"),
                   (selected["decoded_frames"] - 1, "preview-last.png")]
        previews = []
        try:
            with av.open(str(source)) as container:
                stream = container.streams[selected["stream_index"]]
                for ordinal, frame in enumerate(container.decode(stream)):
                    for target, name in targets:
                        if ordinal == target:
                            metadata = timestamp(frame)
                            save_png(frame, output / name, av)
                            previews.append(dict(file=name, ordinal=ordinal, stream_index=stream.index, **metadata))
                    if ordinal >= max(target for target, _ in targets):
                        break
            summary["previews"] = previews
        except Exception as exc:
            faults.append(dict(stage="preview", code=type(exc).__name__))
    summary["faults"] = faults
    source_after = source.stat()
    if (source_after.st_size, source_after.st_mtime_ns) != (source_before.st_size, source_before.st_mtime_ns):
        faults.append(dict(stage="input", code="SourceChangedDuringAnalysis"))
    summary["audio_presence"] = "present" if summary["audio"] else "no_audio_stream"
    summary["video_presence"] = "present" if summary["video"] else "no_video_stream"
    summary["audio_wav"] = dict(requested=extract_audio_wav)
    wav_path = output / "decoded-audio.wav"
    if extract_audio_wav and wav_path.is_file():
        with wav_path.open("rb") as handle:
            wav_sha256 = hashlib.file_digest(handle, "sha256").hexdigest()
        summary["audio_wav"].update(file=wav_path.name, sha256=wav_sha256, bytes=wav_path.stat().st_size,
            stream_index=wav_stream, sample_frames=wav_frames, sample_rate=48000, channels=2,
            sample_format="s16le", pcm_seconds=wav_frames / 48000,
            scope="Received decoded PCM from the entire capture; not a song-only reference. Concatenated in decode order without gap filling, cropping, or resampling; consult audio-frame-index.jsonl PTS and sample_frame_offset.")
    if faults:
        summary["status"] = "failed"
    with (output / "summary.json").open("x", encoding="utf-8") as handle:
        handle.write(line(summary))
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputfile", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--extract-audio-wav", action="store_true",
                        help="Extract one 48 kHz stereo s16/s16p decoded stream without gap filling or cropping")
    args = parser.parse_args(argv)
    summary = analyze(args.inputfile, args.output, extract_audio_wav=args.extract_audio_wav)
    print(line(summary), end="")
    return 0 if summary["status"] == "decoded" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(line(dict(status="failed", error_type=type(exc).__name__)), file=sys.stderr, end="")
        raise SystemExit(2)
