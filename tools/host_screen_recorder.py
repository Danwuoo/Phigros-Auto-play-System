"""Host-side Windows window capture recorder using GDI PrintWindow and FFmpeg NVENC.

Captures Android Emulator window directly on Windows host with zero guest CPU overhead.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
kernel32 = ctypes.windll.kernel32

PW_RENDERFULLCONTENT = 2
DEFAULT_RUNS_DIR = Path(__file__).resolve().parents[1] / "local" / "manual-panel-001" / "runs"
ROUND_NAME_RE = re.compile(r"^round-\d+$")


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


def find_ffmpeg(explicit: str | None = None) -> str:
    """Find a functional ffmpeg binary supporting NVENC or x264."""
    candidates = [
        explicit,
        os.environ.get("FFMPEG"),
        r"C:\Users\wurre\AppData\Local\CapCut\Apps\8.3.0.3497\ffmpeg.exe",
        r"C:\Users\wurre\AppData\Local\CapCut\Apps\8.1.1.3417\ffmpeg.exe",
    ]
    # Check candidates
    for c in candidates:
        if c and Path(c).is_file():
            return str(Path(c).resolve())

    # Glob CapCut directories
    capcut_base = Path(r"C:\Users\wurre\AppData\Local\CapCut\Apps")
    if capcut_base.is_dir():
        for exe in sorted(capcut_base.glob("*/ffmpeg.exe"), reverse=True):
            if exe.is_file():
                return str(exe.resolve())

    # PATH fallback
    which = shutil.which("ffmpeg")
    if which:
        return which

    raise FileNotFoundError("FFmpeg executable not found.")


def ensure_desktop() -> int:
    """Ensure current thread is attached to interactive desktop WinSta0\\Default."""
    hdesk = user32.OpenDesktopW("Default", 0, False, 0x01FF)
    if hdesk:
        user32.SetThreadDesktop(hdesk)
    return hdesk


def ensure_android_protection(
    adb_path: str = r"C:\Users\wurre\AppData\Local\Android\Sdk\platform-tools\adb.exe",
    serial: str = "emulator-5554",
) -> None:
    """Ensure system gesture navigation is disabled and immersive mode is active."""
    try:
        if Path(adb_path).is_file():
            subprocess.run([adb_path, "-s", serial, "shell", "cmd", "overlay", "enable", "com.android.internal.systemui.navbar.threebutton"], capture_output=True, timeout=3)
            subprocess.run([adb_path, "-s", serial, "shell", "cmd", "overlay", "disable", "com.android.internal.systemui.navbar.gestural"], capture_output=True, timeout=3)
            subprocess.run([adb_path, "-s", serial, "shell", "settings", "put", "global", "policy_control", "immersive.full=*"], capture_output=True, timeout=3)
    except Exception:
        pass


def find_emulator_window(target_keyword: str = "emulator") -> tuple[int | None, str]:
    """Find top-level Android emulator display window (excluding toolbars) across desktops."""
    hw = user32.OpenWindowStationW("WinSta0", False, 0x037F)
    desktops: list[str] = []
    if hw:
        def enum_desk(name, _):
            desktops.append(name)
            return True
        DESKTOPENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.LPCWSTR, wintypes.LPARAM)
        user32.EnumDesktopsW(hw, DESKTOPENUMPROC(enum_desk), 0)

    if "Default" in desktops:
        desktops.remove("Default")
        desktops.insert(0, "Default")
    elif not desktops:
        desktops = ["Default"]

    best_hwnd = None
    best_area = 0
    best_title = ""

    for d in desktops:
        hd = user32.OpenDesktopW(d, 0, False, 0x01FF)
        if not hd:
            continue
        user32.SetThreadDesktop(hd)

        def enum_cb(hwnd, _):
            nonlocal best_hwnd, best_area, best_title
            if not user32.IsWindowVisible(hwnd):
                return True
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buff = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buff, length + 1)
                title = buff.value
                if target_keyword.lower() in title.lower():
                    r = RECT()
                    user32.GetClientRect(hwnd, ctypes.byref(r))
                    w = r.right - r.left
                    h = r.bottom - r.top
                    # Must be a substantial window (emulator screen, not 54px toolbar)
                    if w >= 200 and h >= 200:
                        area = w * h
                        if area > best_area:
                            best_area = area
                            best_hwnd = hwnd
                            best_title = title
            return True

        WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
        user32.EnumDesktopWindows(hd, WNDENUMPROC(enum_cb), 0)
        if best_hwnd:
            return best_hwnd, best_title

    return best_hwnd, best_title


class HostWindowRecorder:
    """Captures a Win32 window at specified FPS and pipes frames to FFmpeg."""

    def __init__(
        self,
        output_path: str | Path,
        hwnd: int | None = None,
        fps: int = 30,
        bitrate: str = "5M",
        ffmpeg_path: str | None = None,
    ):
        self.output_path = Path(output_path).resolve()
        self.hwnd = hwnd
        self.fps = fps
        self.bitrate = bitrate
        self.ffmpeg_path = find_ffmpeg(ffmpeg_path)
        self.stop_event = threading.Event()
        self.thread: threading.Thread | None = None
        self.frames_written = 0
        self.duration_seconds = 0.0
        self.video_width = 0
        self.video_height = 0
        self.encoder_used = "h264_nvenc"
        self.error: Exception | None = None

    def start(self) -> None:
        """Start asynchronous recording in a background thread."""
        self.stop_event.clear()
        self.frames_written = 0
        self.error = None
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self, timeout: float = 6.0) -> bool:
        """Signal recording to stop and wait for FFmpeg to finish cleanly."""
        self.stop_event.set()
        if self.thread:
            self.thread.join(timeout=timeout)
            return not self.thread.is_alive()
        return True

    def _run(self) -> None:
        hdc_screen = None
        hdc_mem = None
        hbm = None
        old_bm = None
        proc = None

        try:
            ensure_desktop()
            if not self.hwnd or not user32.IsWindow(self.hwnd):
                for _ in range(10):
                    self.hwnd, title = find_emulator_window()
                    if self.hwnd and user32.IsWindow(self.hwnd):
                        break
                    time.sleep(0.5)
                if not self.hwnd or not user32.IsWindow(self.hwnd):
                    raise RuntimeError("Emulator display window not found on host.")

            r = RECT()
            user32.GetClientRect(self.hwnd, ctypes.byref(r))
            w = r.right - r.left
            h = r.bottom - r.top

            # Ensure even dimensions for H.264 video encoders
            w = w - (w % 2)
            h = h - (h % 2)
            if w <= 0 or h <= 0:
                raise ValueError(f"Invalid window client dimensions: {w}x{h}")

            self.video_width = w
            self.video_height = h

            hdc_screen = user32.GetDC(0)
            hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
            hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
            old_bm = gdi32.SelectObject(hdc_mem, hbm)

            bmi = BITMAPINFOHEADER()
            bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
            bmi.biWidth = w
            bmi.biHeight = -h  # top-down DIB
            bmi.biPlanes = 1
            bmi.biBitCount = 32  # BGR0 (DWORD-aligned scanlines)
            bmi.biCompression = 0

            buf = ctypes.create_string_buffer(w * h * 4)
            self.output_path.parent.mkdir(parents=True, exist_ok=True)

            # Build ffmpeg command with NVENC hardware acceleration
            cmd = [
                self.ffmpeg_path,
                "-y",
                "-f", "rawvideo",
                "-vcodec", "rawvideo",
                "-s", f"{w}x{h}",
                "-pix_fmt", "bgr0",
                "-r", str(self.fps),
                "-i", "-",
                "-c:v", self.encoder_used,
                "-preset", "p4",
                "-b:v", self.bitrate,
                "-pix_fmt", "yuv420p",
                str(self.output_path),
            ]

            proc = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
            )

            frame_interval = 1.0 / self.fps
            start_wall = time.perf_counter()

            while not self.stop_event.is_set():
                t0 = time.perf_counter()

                # Capture window directly via GDI PrintWindow
                res = user32.PrintWindow(self.hwnd, hdc_mem, PW_RENDERFULLCONTENT)
                if res != 0:
                    gdi32.GetDIBits(hdc_mem, hbm, 0, h, buf, ctypes.byref(bmi), 0)
                    proc.stdin.write(buf.raw)
                    self.frames_written += 1

                elapsed = time.perf_counter() - t0
                sleep_needed = frame_interval - elapsed
                if sleep_needed > 0:
                    if self.stop_event.wait(timeout=sleep_needed):
                        break

            self.duration_seconds = round(time.perf_counter() - start_wall, 2)

        except Exception as exc:
            self.error = exc
        finally:
            if proc is not None:
                if proc.stdin:
                    try:
                        proc.stdin.close()
                    except Exception:
                        pass
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=2)

            # Clean up all GDI handles
            if hdc_mem and old_bm:
                gdi32.SelectObject(hdc_mem, old_bm)
            if hbm:
                gdi32.DeleteObject(hbm)
            if hdc_mem:
                gdi32.DeleteDC(hdc_mem)
            if hdc_screen:
                user32.ReleaseDC(0, hdc_screen)


def watch_and_record(
    runs_dir: Path,
    fps: int = 30,
    bitrate: str = "5M",
    ffmpeg_path: str | None = None,
) -> None:
    """Watch runs directory and record each round on the Windows host."""
    runs_dir = Path(runs_dir).resolve()
    print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Initializing...", flush=True)
    ensure_desktop()

    hwnd, title = find_emulator_window()
    if not hwnd:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] WARNING: Emulator window not found yet. Will probe dynamically.", flush=True)
    else:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Found Emulator Window: HWND={hwnd} '{title}'", flush=True)

    print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Monitoring {runs_dir} (FPS={fps}, Bitrate={bitrate})...", flush=True)
    known_rounds: set[str] = set()

    if runs_dir.exists():
        for run in runs_dir.iterdir():
            if run.is_dir() and run.name.startswith("catalog-manual-panel-"):
                for round_entry in run.iterdir():
                    if round_entry.is_dir() and ROUND_NAME_RE.match(round_entry.name):
                        known_rounds.add(str(round_entry.resolve()))

    print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] {len(known_rounds)} existing rounds skipped. Standing by for next round...", flush=True)

    active_round: Path | None = None
    recorder: HostWindowRecorder | None = None
    start_time: float = 0.0

    try:
        while True:
            time.sleep(0.4)
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
                                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] >>> New round detected: {round_entry.name} in {run.name}", flush=True)
                                ensure_android_protection()
                                active_round = round_entry
                                start_time = time.time()
                                out_mp4 = active_round / "gameplay.mp4"

                                meta_file = active_round / "record-meta.json"
                                meta = {
                                    "status": "recording",
                                    "engine": "host_gdi_nvenc",
                                    "round_dir": str(active_round),
                                    "target_fps": fps,
                                    "started_utc": datetime.now(timezone.utc).isoformat(),
                                }
                                try:
                                    meta_file.write_text(json.dumps(meta, indent=2), encoding="utf-8")
                                except Exception:
                                    pass

                                recorder = HostWindowRecorder(
                                    output_path=out_mp4,
                                    fps=fps,
                                    bitrate=bitrate,
                                    ffmpeg_path=ffmpeg_path,
                                )
                                recorder.start()
                                print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Recording started -> {out_mp4}", flush=True)
                                break
                    if active_round is not None:
                        break
            else:
                manifest_file = active_round / "manifest.json"
                result_file = active_round / "result.png"
                summary_file = active_round / "desktop-summary.json"
                stop_file = active_round / "STOP"
                elapsed = time.time() - start_time

                # Completion conditions
                if manifest_file.exists() or result_file.exists() or summary_file.exists() or stop_file.exists() or elapsed > 280:
                    reason = (
                        "result_screen" if result_file.exists()
                        else "manifest_ready" if manifest_file.exists()
                        else "summary_ready" if summary_file.exists()
                        else "stop_or_timeout"
                    )
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] <<< Round completed ({reason}, {elapsed:.1f}s): {active_round.name}", flush=True)
                    # Allow brief window for settlement frame
                    time.sleep(1.0)

                    if recorder:
                        recorder.stop()
                        out_mp4 = active_round / "gameplay.mp4"
                        success = out_mp4.is_file() and out_mp4.stat().st_size > 1024

                        file_sha256 = ""
                        if success:
                            with out_mp4.open("rb") as f:
                                file_sha256 = hashlib.file_digest(f, "sha256").hexdigest()

                        meta_file = active_round / "record-meta.json"
                        meta = {
                            "status": "completed" if success else "failed",
                            "engine": "host_gdi_nvenc",
                            "round_dir": str(active_round),
                            "duration_sec": recorder.duration_seconds,
                            "frames_written": recorder.frames_written,
                            "resolution": f"{recorder.video_width}x{recorder.video_height}",
                            "encoder": recorder.encoder_used,
                            "video_file": str(out_mp4) if success else None,
                            "video_bytes": out_mp4.stat().st_size if success else 0,
                            "video_sha256": file_sha256,
                            "completed_utc": datetime.now(timezone.utc).isoformat(),
                        }
                        try:
                            meta_file.write_text(json.dumps(meta, indent=2), encoding="utf-8")
                        except Exception:
                            pass

                        if success:
                            print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Saved round video: {out_mp4} ({out_mp4.stat().st_size} bytes, {recorder.frames_written} frames)", flush=True)
                        else:
                            print(f"[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Recording failed or empty file. Error: {recorder.error}", flush=True)

                    active_round = None
                    recorder = None

    except KeyboardInterrupt:
        print(f"\n[{datetime.now().strftime('%H:%M:%S')}] [Host Recorder] Stopped by user.")
        if recorder:
            recorder.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    watch_p = subparsers.add_parser("watch", help="Watch runs folder and record each round from host")
    watch_p.add_argument("--runs-dir", type=Path, default=DEFAULT_RUNS_DIR)
    watch_p.add_argument("--fps", type=int, default=30)
    watch_p.add_argument("--bitrate", default="5M")
    watch_p.add_argument("--ffmpeg", default=None)

    record_p = subparsers.add_parser("record", help="Record host window for explicit duration")
    record_p.add_argument("--output", type=Path, required=True)
    record_p.add_argument("--duration", type=float, default=5.0)
    record_p.add_argument("--fps", type=int, default=30)
    record_p.add_argument("--bitrate", default="5M")
    record_p.add_argument("--ffmpeg", default=None)

    test_p = subparsers.add_parser("test", help="Test emulator window detection and short capture")

    args = parser.parse_args()

    if args.command == "test":
        print("Testing host window capture...")
        hwnd, title = find_emulator_window()
        print(f"Detected HWND: {hwnd}, Title: '{title}'")
        if not hwnd:
            sys.exit(1)
        test_out = Path("local/test_capture.mp4")
        rec = HostWindowRecorder(test_out, hwnd=hwnd, fps=30)
        rec.start()
        time.sleep(2.0)
        rec.stop()
        if test_out.is_file() and test_out.stat().st_size > 1024:
            print(f"SUCCESS: Captured {rec.frames_written} frames -> {test_out} ({test_out.stat().st_size} bytes)")
            test_out.unlink()
        else:
            print(f"FAILED: {rec.error}")
            sys.exit(1)

    elif args.command == "record":
        rec = HostWindowRecorder(args.output, fps=args.fps, bitrate=args.bitrate, ffmpeg_path=args.ffmpeg)
        print(f"Recording {args.duration}s to {args.output}...")
        rec.start()
        time.sleep(args.duration)
        rec.stop()
        print(f"Done. Captured {rec.frames_written} frames ({rec.duration_seconds}s).")

    elif args.command == "watch":
        watch_and_record(args.runs_dir, fps=args.fps, bitrate=args.bitrate, ffmpeg_path=args.ffmpeg)


if __name__ == "__main__":
    main()
