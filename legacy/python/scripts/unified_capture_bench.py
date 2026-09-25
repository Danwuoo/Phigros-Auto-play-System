"""Paired real-AVD capture campaign; raw JSONL is the common measurement source.

Run from the repository root with PYTHONPATH=src. Never injects touch. The caller
must put org.pas.capturefixture in the foreground first. No automatic retries or
selection of the best runs. MMAP is explicitly diagnostic, not a usable backend.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import threading
import time

from pas.adb import adb_call, find_adb, probe
from pas.telemetry import distribution


def dump(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def recompute(path):
    """Apply identical [start,end) timestamp filters to both execution modes."""
    with path.open(encoding="utf-8") as stream:
        events = [json.loads(line) for line in stream]
    phases = {e["phase"]: e["monotonic_ns"] for e in events if e["event"] == "bench_phase"}
    start, end = phases["MEASURING"], phases["STOPPING"]
    if end <= start:
        raise ValueError("invalid measurement window")
    duration = (end-start)/1e9
    def select(kind, timestamp):
        return sorted((e for e in events if e["event"] == kind and start <= e[timestamp] < end),
                      key=lambda e:e[timestamp])
    frames = select("capture", "capture_complete_ns")
    consumed = select("frame_consumed", "consume_ns")
    capture_times = {e["frame_sequence"]:e["capture_complete_ns"] for e in events if e["event"]=="capture"}
    for event in consumed:
        if "capture_complete_ns" not in event:
            event["capture_complete_ns"] = capture_times[event["frame_sequence"]]
    counters = select("fixture_counter", "capture_complete_ns")
    intervals = [(b["capture_complete_ns"]-a["capture_complete_ns"])/1e6
                 for a,b in zip(frames,frames[1:])]
    increments = [(b["counter"]-a["counter"]) % (1 << 24) for a,b in zip(counters,counters[1:])]
    counter_span_s = ((counters[-1]["capture_complete_ns"]-counters[0]["capture_complete_ns"])/1e9
                      if len(counters)>1 else 0)
    valid_counter_order = all(v < (1 << 23) for v in increments)
    source_hz = sum(increments)/counter_span_s if counter_span_s and valid_counter_order else None
    geometry = sorted({(e["width"],e["height"],e["source_rotation"]) for e in frames})
    def segment(lo,hi):
        selected=[e for e in consumed if start+lo*1e9 <= e["consume_ns"] < min(end,start+hi*1e9)]
        return distribution([(e["consume_ns"]-e["capture_complete_ns"])/1e6 for e in selected])
    return {"measurement_start_ns":start,"measurement_end_ns":end,"window_s":duration,
            "capture_events":len(frames),"consumer_events":len(consumed),
            "received_hz":len(frames)/duration,"fixture_samples":len(counters),
            "fixture_distinct":len({e["counter"] for e in counters}),
            "fixture_distinct_hz":len({e["counter"] for e in counters})/duration,
            "fixture_counter_span_hz":source_hz,"fixture_counter_order_valid":valid_counter_order,
            "fixture_decoded_fraction":len(counters)/len(frames) if frames else 0,
            "geometry":geometry,"geometry_valid":geometry==[(1280,720,1)],
            "arrival_interval_ms":distribution(intervals),
            "capture_to_pixels_ready_ms":distribution([(e["pixels_ready_ns"]-e["capture_complete_ns"])/1e6 for e in frames]),
            "host_residency_ms":distribution([(e["consume_ns"]-e["capture_complete_ns"])/1e6 for e in consumed]),
            "consumer_sequence_skips":sum(e["sequence_skip"] for e in consumed),
            "consumer_cross_boundary_frames":sum(e["capture_complete_ns"]<start for e in consumed),
            "host_residency_first15s_ms":segment(0,15),"host_residency_after15s_ms":segment(15,1e9),
            "raw_sha256":sha(path),"source_absolute_age":"unknown"}


def host_sample():
    idle,kernel,user = wintypes.FILETIME(),wintypes.FILETIME(),wintypes.FILETIME()
    if not ctypes.windll.kernel32.GetSystemTimes(ctypes.byref(idle),ctypes.byref(kernel),ctypes.byref(user)):
        raise ctypes.WinError()
    def integer(t): return (t.dwHighDateTime<<32)|t.dwLowDateTime
    return time.monotonic_ns(),integer(idle),integer(kernel)+integer(user)


def monitor(path, stop):
    before=host_sample()
    with path.open("w",encoding="utf-8") as stream:
        while not stop.wait(1):
            after=host_sample()
            dt=after[2]-before[2]
            stream.write(json.dumps({"start_ns":before[0],"end_ns":after[0],
                "host_busy_percent":100*(1-(after[1]-before[1])/dt) if dt else None})+"\n")
            stream.flush()
            before=after


def foreground(adb,serial):
    lines=adb_call(adb,["-s",serial,"shell","dumpsys","window"],timeout_s=15).decode(errors="replace").splitlines()
    focus=[line.strip() for line in lines if "mCurrentFocus=" in line]
    if not focus or not all("org.pas.capturefixture" in line for line in focus):
        raise RuntimeError(f"capture Fixture is not foreground: {focus}")
    return focus


def campaign(output,serial,include_mmap):
    output.mkdir(parents=True,exist_ok=False)
    adb=find_adb()
    before_focus=foreground(adb,serial)
    local_apk=Path("measurements/fixture_android/pas-capture-fixture.apk")
    package=adb_call(adb,["-s",serial,"shell","pm","path","org.pas.capturefixture"]).decode().strip()
    installed=output/"installed-capture-fixture.apk"
    adb_call(adb,["-s",serial,"pull",package.removeprefix("package:"),str(installed)],timeout_s=30)
    src_hashes={str(p).replace("\\","/"):sha(p) for folder in ("src/pas","fixtures/capture_android")
                for p in sorted(Path(folder).rglob("*")) if p.suffix in (".py",".java",".xml")}
    src_hashes["scripts/unified_capture_bench.py"]=sha(Path(__file__))
    names={"T":("thread","payload"),"P":("process","payload"),"M":("process","mmap")}
    normal_order=list("TPMPMTMTP") if include_mmap else list("TPPTTP")
    cases=[{"name":f"normal-{i+1}-{key}","key":key,"condition":"normal","duration_s":60,"extra":[]}
           for i,key in enumerate(normal_order)]
    stress=[("slow50",["--consumer-delay-ms","50"]),
            ("recover100",["--consumer-delay-ms","100","--consumer-recover-after-s","15"]),
            ("pause500",["--receiver-pause-ms","500"]),
            ("pause500-guard100",["--receiver-pause-ms","500","--max-relative-lag-ms","100"]),
            ("parent-gil",["--load"])]
    for i,(condition,extra) in enumerate(stress):
        for key in ("TP" if i%2==0 else "PT"):
            cases.append({"name":f"{condition}-{key}","key":key,"condition":condition,"duration_s":30,"extra":extra})
    manifest={"protocol_version":1,"created_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),
              "host_os":platform.platform(),"python":sys.version,"logical_cpus":os.cpu_count(),
              "git_head":subprocess.check_output(["git","rev-parse","HEAD"],text=True).strip(),
              "git_status":subprocess.check_output(["git","status","--porcelain"],text=True),
              "environment":probe(adb,serial),"foreground_before":before_focus,
              "installed_fixture_sha256":sha(installed),
              "local_fixture_sha256":sha(local_apk) if local_apk.exists() else None,
              "code_sha256":src_hashes,"cases":cases,"configurations":names,
              "fixed":{"warmup_s":10,"width":1280,"height":720,"source_rotation":1,
                       "format":"rgb888","row_order":"top-down","fixture_scale":1,
                       "ready_timeout_s":15,"preview":False,"instrument_log_cost":False,
                       "thread_per_frame_rss":False},
              "predeclared_comparability":{"normal_source_max_min_ratio_max":1.05,
                                           "near60_source_hz_range":[57,63],
                                           "all_frames_fixture_decodable":True},
              "research_gates":{"distinct_frames_hz_min":55,"arrival_p95_ms_max":33.4,"arrival_p99_ms_max":50},
              "limitations":["Host background processes remain user-controlled; GetSystemTimes sampled at 1 Hz",
                "MMAP is diagnostic only: counter decode cannot prove atomic full-frame snapshot",
                "Existing CLI telemetry differs between paths; compares instrumented pipelines, not transport alone",
                "Process capture_complete timestamps are from child; only frames delivered to parent appear in capture JSONL",
                "Process child ipc_published counter is a resource-window estimate, not an exact frame-window raw event count",
                "CPU summaries have backend-specific resource windows; not ranked or merged",
                "No absolute source frame age or end-to-end touch latency measured"]}
    dump(output/"manifest.json",manifest)
    stop=threading.Event()
    thread=threading.Thread(target=monitor,args=(output/"host_load.jsonl",stop),daemon=True)
    thread.start()
    results=[]
    try:
        for case in cases:
            directory=output/case["name"]
            directory.mkdir()
            focus_before=foreground(adb,serial)
            execution,transport=names[case["key"]]
            command=[sys.executable,"-m","pas.cli","capture-bench","--serial",serial,
                     "--capture-backend","emulator-grpc","--capture-execution",execution,
                     "--grpc-transport",transport,"--image-format","rgb888","--row-order","top-down",
                     "--width","1280","--height","720","--fixture-scale","1","--fixture-x","0","--fixture-y","0",
                     "--duration-s",str(case["duration_s"]),"--warmup-s","10","--ready-timeout-s","15",
                     "--no-log-cost","--log",str(directory/"capture.jsonl"),*case["extra"]]
            if transport=="mmap":command.append("--diagnostic-mmap")
            if execution=="thread":command.append("--no-consumer-rss")
            dump(directory/"invocation.json",{"command":command,"foreground_before":focus_before})
            print(f"START {case['name']}",flush=True)
            with (directory/"stdout.json").open("w",encoding="utf-8") as out, (directory/"stderr.txt").open("w",encoding="utf-8") as err:
                completed=subprocess.run(command,stdout=out,stderr=err,timeout=case["duration_s"]+80)
            result={**case,"returncode":completed.returncode,"foreground_after":foreground(adb,serial)}
            try:result["common"]=recompute(directory/"capture.jsonl")
            except Exception as error:result["analysis_error"]=str(error)
            results.append(result)
            dump(output/"results.json",results)
            common=result.get("common",{})
            print(f"END {case['name']} rc={completed.returncode} frames={common.get('capture_events')} source_hz={common.get('fixture_counter_span_hz')}",flush=True)
            if completed.returncode or not common.get("geometry_valid") or common.get("fixture_decoded_fraction")!=1:
                raise RuntimeError(f"invalid batch retained; campaign stopped: {case['name']}")
    finally:
        stop.set()
        thread.join(3)
        dump(output/"results.json",results)
    print(str(output.resolve()),flush=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",type=Path)
    parser.add_argument("--serial",default="emulator-5554")
    parser.add_argument("--include-diagnostic-mmap",action="store_true")
    parser.add_argument("--recompute",type=Path,nargs="+")
    args=parser.parse_args()
    if args.recompute:
        for path in args.recompute:print(json.dumps({"path":str(path),**recompute(path)},indent=2))
    elif args.output:campaign(args.output,args.serial,args.include_diagnostic_mmap)
    else:parser.error("--output or --recompute required")
