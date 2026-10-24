"""Explicit local scrcpy 5.0.1 observation-only recording; no game or input control."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from record_emulator import clock_anchor, private_directory

CREATE_FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
SUMMARY_RESERVE = 32768


def sha256(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def command(scrcpy, output, serial, duration, video_only=False):
    audio_args = ["--no-audio"] if video_only else [
        "--require-audio", "--audio-source=output", "--audio-codec=flac"]
    return [str(scrcpy), "--serial=" + serial, "--no-control", "--no-playback", "--no-window",
            *audio_args, "--video-codec=h264",
            "--video-bit-rate=20M", "--max-fps=60", "--time-limit=" + str(duration),
            "--record=" + str(output / "capture.mkv")]


def directory_bytes(path):
    return sum(entry.stat().st_size for entry in path.iterdir() if entry.is_file())


def budget_reason(path, max_bytes, min_free):
    if directory_bytes(path) + SUMMARY_RESERVE >= max_bytes:
        return "file_budget"
    if shutil.disk_usage(path).free < min_free + SUMMARY_RESERVE:
        return "free_disk_budget"
    return None


def stop_process(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def record(scrcpy, output, serial, duration, max_bytes, min_free, video_only=False):
    scrcpy, output = Path(scrcpy).resolve(), Path(output).resolve()
    if output.exists():
        raise FileExistsError("Output already exists")
    server, adb = scrcpy.parent / "scrcpy-server", scrcpy.parent / "adb.exe"
    if not all(path.is_file() for path in (scrcpy, server, adb)):
        raise FileNotFoundError("Use the complete scrcpy release directory")
    version = subprocess.run([str(scrcpy), "--version"], capture_output=True, text=True,
                             timeout=10, check=True, creationflags=CREATE_FLAGS).stdout.strip()
    if not re.match(r"^scrcpy 5\.0\.1(?:\s|$)", version):
        raise ValueError("Only the explicitly selected scrcpy 5.0.1 release is supported")
    if shutil.disk_usage(output.parent).free < min_free + SUMMARY_RESERVE:
        raise OSError("Insufficient free disk before capture")
    private_directory(output)
    args = command(scrcpy, output, serial, duration, video_only=video_only)
    environment = os.environ.copy()
    environment.update(SCRCPY_SERVER_PATH=str(server), ADB=str(adb))
    runtime = dict(version=version, executable=str(scrcpy), executable_sha256=sha256(scrcpy),
                   paired_server=str(server), paired_server_sha256=sha256(server))
    started, anchors, process, reason, error_type = time.monotonic(), [clock_anchor()], None, None, None
    try:
        with (output / "capture.log").open("xb") as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, shell=False,
                                       env=environment, creationflags=CREATE_FLAGS)
            while process.poll() is None:
                reason = budget_reason(output, max_bytes, min_free)
                if reason or time.monotonic() - started > duration + 15:
                    reason = reason or "timeout"
                    break
                time.sleep(0.1)
    except KeyboardInterrupt:
        reason = "user_interrupt"
    except Exception as exc:
        reason, error_type = "runtime_error", type(exc).__name__
    finally:
        if process is not None:
            try:
                stop_process(process)
            except Exception as exc:
                reason, error_type = "shutdown_error", type(exc).__name__
        anchors.append(clock_anchor())
    media = output / "capture.mkv"
    exit_code = process.returncode if process is not None else None
    process_exited = process is None or process.poll() is not None
    reason = reason or budget_reason(output, max_bytes, min_free)
    status = "recorded" if exit_code == 0 and process_exited and not reason and media.is_file() and media.stat().st_size > 0 else "failed"
    if reason == "user_interrupt":
        status = "interrupted"
    summary = dict(schema_version=1, status=status, observer_only=True, local_capture=True,
                   audio_requested=not video_only,
                   command=args, runtime=runtime, duration_requested_seconds=duration,
                   host_elapsed_seconds=time.monotonic() - started, host_clock_anchors=anchors,
                   exit_code=exit_code, process_exited=process_exited, stop_reason=reason,
                   error_type=error_type, capture_file="capture.mkv",
                   capture_bytes=media.stat().st_size if media.is_file() else 0,
                   max_bytes=max_bytes, min_free_bytes=min_free,
                   validation="Container streams, A/V phase and source completeness are unverified.",
                   budget_enforcement="Polled every 100ms; process termination may allow a small overshoot.")
    with (output / "summary.json").open("x", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--scrcpy", type=Path, required=True)
    parser.add_argument("--duration", type=int, required=True)
    parser.add_argument("--serial", default="emulator-5554")
    parser.add_argument("--max-mib", type=int, default=1024)
    parser.add_argument("--min-free-mib", type=int, default=1024)
    parser.add_argument("--video-only", action="store_true",
                        help="Do not capture/redirect Android audio; suitable alongside host loopback.")
    args = parser.parse_args(argv)
    if not 2 <= args.duration <= 180 or not 1 <= args.max_mib <= 8192 or args.min_free_mib < 0:
        parser.error("duration 2..180s; max-mib 1..8192; min-free-mib >= 0")
    summary = record(args.scrcpy, args.output, args.serial, args.duration,
                     args.max_mib * 1048576, args.min_free_mib * 1048576, video_only=args.video_only)
    print(json.dumps(summary))
    return 0 if summary["status"] == "recorded" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps(dict(status="failed", error_type=type(error).__name__)), file=sys.stderr)
        raise SystemExit(2)
