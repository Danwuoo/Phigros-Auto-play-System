"""M0 observe runtime: isolated process capture, bounded diagnostics, no input."""

from collections import deque
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import re
import subprocess
import threading
import time
import uuid

from .adb import find_adb, probe
from .capture import CaptureWorker
from .capture_process import ProcessCaptureConfig, ProcessCaptureSource
from .clock import HostClock
from .config import RuntimeConfig
from .telemetry import distribution


class BoundedJournal:
    """Never holds an unbounded collection or file lock on a capture thread."""

    def __init__(self, path: Path, capacity=512, max_bytes=128*1024*1024):
        self.queue = queue.Queue(maxsize=capacity)
        self.path = path
        self.max_bytes = max_bytes
        self.dropped_debug = 0
        self.fault = None
        self._thread = threading.Thread(target=self._write, name="runtime-journal")
        self._thread.start()

    def record(self, event: str, **fields):
        try:
            self.queue.put_nowait({"event":event,**fields})
        except queue.Full:
            if event in ("capture", "capture_overwrite"):
                self.dropped_debug += 1
            else:
                self.fault = "journal_capacity_exhausted"

    def _write(self):
        try:
            with self.path.open("w",encoding="utf-8") as stream:
                written=0
                while True:
                    item = self.queue.get()
                    try:
                        if item is None:
                            break
                        line=json.dumps(item,ensure_ascii=False)+"\n"
                        size=len(line.encode("utf-8"))
                        if written+size > self.max_bytes:
                            self.fault="journal_disk_limit"
                        elif self.fault is None:
                            stream.write(line)
                            written+=size
                    finally:
                        self.queue.task_done()
        except Exception as error:
            self.fault = f"journal_write_failed:{error}"

    def close(self, timeout_s=3):
        try:
            self.queue.put(None,timeout=timeout_s)
        except queue.Full:
            self.fault = "journal_close_queue_full"
            return
        self._thread.join(timeout_s)
        if self._thread.is_alive():
            self.fault = "journal_flush_timeout"


def _revision():
    try:
        head = subprocess.run(["git","rev-parse","HEAD"],capture_output=True,text=True,
                              timeout=2,check=True).stdout.strip()
        dirty = bool(subprocess.run(["git","status","--porcelain"],capture_output=True,
                                    text=True,timeout=2,check=True).stdout.strip())
        return {"head":head,"dirty":dirty}
    except (OSError,subprocess.SubprocessError):
        return {"head":None,"dirty":None}


def display_refresh_hz(display_info: str):
    match=re.search(r"renderFrameRate\s+([0-9.]+)",display_info)
    return float(match.group(1)) if match else None


def validate_frame_geometry(frame, config: RuntimeConfig):
    actual = (frame.width, frame.height, frame.source_rotation)
    expected = (config.width, config.height, config.source_rotation)
    if actual != expected:
        raise RuntimeError(f"frame geometry changed; new epoch/profile required: expected {expected}, got {actual}")


def _source(config: RuntimeConfig):
    if config.capture_kind == "fake":
        settings = ProcessCaptureConfig(kind="fake",fake_width=config.width,
                                        fake_height=config.height,fake_interval_ms=25)
    else:
        settings = ProcessCaptureConfig(kind="grpc",serial=config.serial,
                                        endpoint=config.endpoint,token_file=config.token_file,
                                        width=config.width,height=config.height,
                                        image_format="rgb888",row_order="top-down",transport="payload")
    return ProcessCaptureSource(settings)


def run_observe(config: RuntimeConfig, *, duration_s: float, no_preview=False):
    if not 0 < duration_s <= 86400:
        raise ValueError("invalid observe duration")
    device = None
    if config.capture_kind == "emulator-grpc":
        adb = find_adb()
        if not adb:
            raise RuntimeError("ADB unavailable")
        details = probe(adb,config.serial)
        device = {key:details[key] for key in ("selected_serial","android_release","android_sdk",
                                               "model","cpu_abi","wm_size","wm_density")}
        device["display_refresh_hz_setting"]=display_refresh_hz(details["display_info"])
    run_dir = Path(config.log_dir)/(datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")+"-"+uuid.uuid4().hex[:8])
    run_dir.mkdir(parents=True,exist_ok=False)
    public = config.public()
    public_hash = hashlib.sha256(json.dumps(public,sort_keys=True).encode()).hexdigest()
    (run_dir/"config.json").write_text(json.dumps(public,indent=2),encoding="utf-8")
    manifest = {"schema":1,"mode":"observe","run_id":run_dir.name,
                "config_sha256":public_hash,"revision":_revision(),
                "environment":{"platform":platform.platform(),"python":platform.python_version(),
                               "device":device,"clock":"time.monotonic_ns"},
                "capture":{"execution":"process","transport":"payload","format":"RGB888",
                           "row_order":"top-down","frame_slot_capacity":1},
                "touch":{"created":False,"capability":"unverified"},
                "source_age":"unknown","ui_classifier":"unavailable"}
    (run_dir/"manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    journal = BoundedJournal(run_dir/"events.jsonl")
    source = None
    worker = None
    preview = None
    error = None
    seen = 0
    shape = None
    intervals = deque(maxlen=100000)
    residences = deque(maxlen=100000)
    last_complete = None
    last_frame = None
    try:
        source = _source(config)
        worker = CaptureWorker(source, HostClock(),journal)
        worker.start()
        worker.wait_for_valid_frames(1,15)
        if config.preview_hz and not no_preview:
            from .preview import DiagnosticPreview
            preview = DiagnosticPreview(config.preview_hz)
        end_ns = time.monotonic_ns()+round(duration_s*1e9)
        sequence = -1
        while time.monotonic_ns() < end_ns:
            if worker.error:
                raise RuntimeError(f"capture failed: {worker.error}")
            if journal.fault:
                raise RuntimeError(journal.fault)
            frame = worker.latest.read_after(sequence,0.05)
            if frame is None:
                if preview and last_frame:
                    preview.update(last_frame,epoch=0,ui="UNKNOWN",
                                   reason="no new frame; freshness unconfirmed")
                if preview and preview.closed:
                    break
                continue
            sequence = frame.sequence
            last_frame = frame
            validate_frame_geometry(frame,config)
            shape = [frame.width,frame.height,frame.source_rotation]
            now = time.monotonic_ns()
            seen += 1
            residences.append((now-frame.capture_complete_ns)/1e6)
            if last_complete is not None:
                intervals.append((frame.capture_complete_ns-last_complete)/1e6)
            last_complete=frame.capture_complete_ns
            if preview:
                preview.update(frame,epoch=0,ui="UNKNOWN",reason="observe only",now_ns=now)
        if preview and preview.closed:
            journal.record("preview_closed",monotonic_ns=time.monotonic_ns())
    except KeyboardInterrupt:
        journal.record("user_stop",monotonic_ns=time.monotonic_ns())
    except Exception as exc:
        error = str(exc)
        journal.record("runtime_error",reason=error,monotonic_ns=time.monotonic_ns())
    finally:
        if preview:
            preview.close()
        if worker:
            try:
                worker.stop()
            except Exception as exc:
                error = f"capture_cleanup:{exc}"
        elif source:
            source.close()
        journal.close()
    summary = {"mode":"observe","run_dir":str(run_dir),"frames_consumed":seen,
               "capture_interval_ms":distribution(list(intervals)),
               "host_residence_ms":distribution(list(residences)),
               "shape_rotation":shape,"frame_overwrites":worker.latest.counters()["overwritten"] if worker else 0,
               "journal_debug_drops":journal.dropped_debug,"journal_fault":journal.fault,
               "capture_shutdown":source.shutdown_report if source else None,
               "last_frame_host_residence_ms_at_stop":((time.monotonic_ns()-last_frame.capture_complete_ns)/1e6
                                                       if last_frame else None),
               "error":error,"input_created":False,"ui_state":"UNKNOWN"}
    (run_dir/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    manifest["observed_frame_shape_rotation"]=shape
    manifest["result"]={"frames_consumed":seen,"error":error or journal.fault,
                        "capture_shutdown":source.shutdown_report if source else None}
    (run_dir/"manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    if error or journal.fault:
        raise RuntimeError(error or journal.fault)
    return summary
