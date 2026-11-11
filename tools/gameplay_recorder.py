"""Record Android Emulator gameplay per round via Host Window Capture (default) or ADB screenrecord."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from host_screen_recorder import HostWindowRecorder, watch_and_record as host_watch_and_record

DEFAULT_SERIAL = "emulator-5554"
DEFAULT_ADB = r"C:\Users\wurre\AppData\Local\Android\Sdk\platform-tools\adb.exe"
DEFAULT_FFMPEG = r"C:\Users\wurre\AppData\Local\CapCut\Apps\8.3.0.3497\ffmpeg.exe"
DEFAULT_RUNS_DIR = Path(__file__).resolve().parents[1] / "local" / "manual-panel-001" / "runs"
REMOTE_MP4 = "/sdcard/gameplay.mp4"
ROUND_NAME_RE = re.compile(r"^round-\d+$")


def find_adb(explicit: str | None = None) -> str:
    candidates = [
        explicit,
        os.environ.get("ADB"),
        DEFAULT_ADB,
        "adb",
    ]
    for c in candidates:
        if c and Path(c).is_file():
            return str(Path(c).resolve())
    return "adb"


def start_adb_recording(adb_path: str, serial: str = DEFAULT_SERIAL) -> subprocess.Popen:
    """Start adb shell screenrecord in background (legacy mode)."""
    subprocess.run(
        [adb_path, "-s", serial, "shell", "pkill", "-2", "screenrecord"],
        capture_output=True,
    )
    subprocess.run(
        [adb_path, "-s", serial, "shell", "rm", "-f", REMOTE_MP4],
        capture_output=True,
    )
    time.sleep(0.3)

    cmd = [
        adb_path, "-s", serial, "shell",
        "screenrecord", "--size", "1280x720", "--bit-rate", "8000000",
        "--time-limit", "300", REMOTE_MP4,
    ]
    print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Recorder] Starting screenrecord on {serial} -> {REMOTE_MP4}...")
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.3)
    return proc


def stop_adb_recording(adb_path: str, destination: Path, serial: str = DEFAULT_SERIAL) -> bool:
    """Stop screenrecord on device, pull MP4 to destination, and clean up (legacy mode)."""
    destination = Path(destination).resolve()
    print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Recorder] Stopping screenrecord on {serial}...")
    subprocess.run(
        [adb_path, "-s", serial, "shell", "pkill", "-2", "screenrecord"],
        capture_output=True,
    )
    time.sleep(2.0)

    print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Recorder] Pulling {REMOTE_MP4} -> {destination}...")
    destination.parent.mkdir(parents=True, exist_ok=True)
    res = subprocess.run(
        [adb_path, "-s", serial, "pull", REMOTE_MP4, str(destination)],
        capture_output=True, text=True,
    )
    subprocess.run(
        [adb_path, "-s", serial, "shell", "rm", "-f", REMOTE_MP4],
        capture_output=True,
    )

    if destination.is_file() and destination.stat().st_size > 1024:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Recorder] Saved round video: {destination} ({destination.stat().st_size} bytes)")
        return True
    else:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Recorder] Pull failed or file empty: {res.stderr.strip()}")
        return False


def watch_adb(runs_dir: Path, adb_path: str, serial: str = DEFAULT_SERIAL) -> None:
    """Watch runs directory and record via ADB screenrecord."""
    runs_dir = Path(runs_dir).resolve()
    print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Watcher] Monitoring {runs_dir} on {serial}...")
    known_rounds: set[str] = set()

    if runs_dir.exists():
        for run in runs_dir.iterdir():
            if run.is_dir():
                for round_entry in run.iterdir():
                    if round_entry.is_dir() and ROUND_NAME_RE.match(round_entry.name):
                        known_rounds.add(str(round_entry.resolve()))

    active_round: Path | None = None
    active_proc: subprocess.Popen | None = None
    start_time: float = 0.0

    try:
        while True:
            time.sleep(0.5)
            if active_round is None:
                if not runs_dir.exists():
                    continue
                candidate_runs = sorted(
                    [r for r in runs_dir.iterdir() if r.is_dir() and r.name.startswith("catalog-manual-panel-")],
                    key=lambda r: r.stat().st_mtime,
                    reverse=True,
                )
                for run in candidate_runs:
                    for round_entry in sorted(run.iterdir(), key=lambda r: r.name):
                        if not round_entry.is_dir() or not ROUND_NAME_RE.match(round_entry.name):
                            continue
                        r_path = str(round_entry.resolve())
                        if r_path not in known_rounds:
                            known_rounds.add(r_path)
                            manifest_file = round_entry / "manifest.json"
                            result_file = round_entry / "result.png"
                            if not manifest_file.exists() and not result_file.exists():
                                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] [ADB Watcher] >>> New round detected: {round_entry.name} in {run.name}")
                                active_round = round_entry
                                start_time = time.time()
                                meta_file = active_round / "record-meta.json"
                                meta = {
                                    "status": "recording",
                                    "engine": "adb_screenrecord",
                                    "round_dir": str(active_round),
                                    "started_utc": datetime.now(timezone.utc).isoformat(),
                                }
                                try:
                                    meta_file.write_text(json.dumps(meta, indent=2), encoding="utf-8")
                                except Exception:
                                    pass
                                active_proc = start_adb_recording(adb_path, serial)
                                break
                    if active_round is not None:
                        break
            else:
                manifest_file = active_round / "manifest.json"
                result_file = active_round / "result.png"
                stop_file = active_round / "STOP"
                elapsed = time.time() - start_time

                if manifest_file.exists() or result_file.exists() or stop_file.exists() or elapsed > 280:
                    reason = "result_screen" if result_file.exists() else ("manifest_ready" if manifest_file.exists() else "stopped_or_timeout")
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] [ADB Watcher] <<< Round completed ({reason}, {elapsed:.1f}s): {active_round.name}")
                    time.sleep(1.2)
                    out_mp4 = active_round / "gameplay.mp4"
                    success = stop_adb_recording(adb_path, out_mp4, serial)

                    meta_file = active_round / "record-meta.json"
                    meta = {
                        "status": "completed" if success else "failed",
                        "engine": "adb_screenrecord",
                        "round_dir": str(active_round),
                        "duration_sec": round(time.time() - start_time, 2),
                        "video_file": str(out_mp4) if success else None,
                        "video_bytes": out_mp4.stat().st_size if success and out_mp4.is_file() else 0,
                        "completed_utc": datetime.now(timezone.utc).isoformat(),
                    }
                    try:
                        meta_file.write_text(json.dumps(meta, indent=2), encoding="utf-8")
                    except Exception:
                        pass

                    active_round = None
                    active_proc = None

    except KeyboardInterrupt:
        print(f"\n[{datetime.now().strftime('%H:%M:%S')}] [ADB Watcher] Interrupted by user.")
        if active_round is not None:
            stop_adb_recording(adb_path, active_round / "gameplay.mp4", serial)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", choices=["host", "adb"], default="host", help="Capture engine (default: host)")
    subparsers = parser.add_subparsers(dest="command", required=True)

    watch_p = subparsers.add_parser("watch", help="Watch runs folder and auto-record rounds")
    watch_p.add_argument("--runs-dir", type=Path, default=DEFAULT_RUNS_DIR)
    watch_p.add_argument("--fps", type=int, default=30)
    watch_p.add_argument("--bitrate", default="5M")
    watch_p.add_argument("--serial", default=DEFAULT_SERIAL)
    watch_p.add_argument("--adb", default=DEFAULT_ADB)

    start_p = subparsers.add_parser("start", help="Start background recording")
    start_p.add_argument("--serial", default=DEFAULT_SERIAL)
    start_p.add_argument("--adb", default=DEFAULT_ADB)

    stop_p = subparsers.add_parser("stop", help="Stop recording and pull video")
    stop_p.add_argument("--serial", default=DEFAULT_SERIAL)
    stop_p.add_argument("--adb", default=DEFAULT_ADB)
    stop_p.add_argument("--output", type=Path, required=True)

    args = parser.parse_args()

    if args.engine == "host":
        if args.command == "watch":
            host_watch_and_record(args.runs_dir, fps=args.fps, bitrate=args.bitrate)
        elif args.command == "start":
            print("Host recording operates via 'watch' or 'host_screen_recorder.py record --output <path>'.")
        elif args.command == "stop":
            print("Host recording stops automatically when round completes.")
    else:
        adb = find_adb(args.adb)
        if args.command == "start":
            start_adb_recording(adb, args.serial)
            print("ADB Recording started. Run 'stop' to finish.")
        elif args.command == "stop":
            stop_adb_recording(adb, args.output, args.serial)
        elif args.command == "watch":
            watch_adb(args.runs_dir, adb, args.serial)


if __name__ == "__main__":
    main()
