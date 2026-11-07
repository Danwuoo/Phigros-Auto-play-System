"""Explicit operator-invoked fixture proof for an already running phigros AVD.

Import and validation helpers are offline. --run authorizes opening only the
private fixture and sending its bounded probe; no cold boot or game launch.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time
from types import SimpleNamespace

import analyze_touch_probe
import restart_song_tests as restart

MAX_NATIVE = 16 * 1024 * 1024
PROFILES = {
    "landscape-r1": dict(raw_width=1280, raw_height=720, raw_rotation=1, normalize_ccw90=False),
    "sensor-portrait-r0-ccw90": dict(raw_width=720, raw_height=1280, raw_rotation=0, normalize_ccw90=True),
}
need, read, write_new, ref, sha = restart.need, restart.read, restart.write_new, restart.ref, restart.sha


def bounded_readonly(argv, limit, timeout):
    """Bound host memory and wall time; this runner never owns Android input."""
    need(type(limit) is int and 1 <= limit <= 32 * 1024 * 1024 and 0 < timeout <= 90,
         "invalid read-only command budget")
    process = subprocess.Popen([str(a) for a in argv], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    buffers = [bytearray(), bytearray()]
    overflow = threading.Event()
    def consume(stream, index):
        try:
            while True:
                chunk = stream.read(65536)
                if not chunk: break
                available = limit - len(buffers[index])
                buffers[index].extend(chunk[:max(0, available)])
                if len(chunk) > available: overflow.set(); break
        finally:
            stream.close()
    readers = [threading.Thread(target=consume, args=(stream, i), daemon=True)
               for i, stream in enumerate((process.stdout, process.stderr))]
    for reader in readers: reader.start()
    deadline = time.monotonic() + timeout
    timed_out = False
    while process.poll() is None:
        if overflow.is_set() or time.monotonic() >= deadline:
            timed_out = not overflow.is_set()
            process.kill()  # ADB read/setup only; never a native input owner.
            break
        time.sleep(.02)
    process.wait(timeout=5)
    for reader in readers: reader.join(timeout=5)
    return SimpleNamespace(stdout=bytes(buffers[0]), stderr=bytes(buffers[1]), returncode=process.returncode,
        timed_out=timed_out, output_limited=overflow.is_set(), readers_closed=not any(r.is_alive() for r in readers))


def sealed_probe(probe, manifest):
    probe, manifest = Path(probe).resolve(strict=True), Path(manifest).resolve(strict=True)
    value = read(manifest)
    need(value.get("schema") == "pas.adaptive-build-archive.v1" and
         type(value.get("artifacts")) is list and 1 <= len(value["artifacts"]) <= 2048,
         "a frozen adaptive build archive is required")
    archive = manifest.parent
    indexed = {}
    for item in value["artifacts"]:
        relative = Path(item["file"])
        need(not relative.is_absolute() and ".." not in relative.parts, "archive artifact escapes root")
        path = (archive / relative).resolve()
        need(path.is_relative_to(archive) and str(path).casefold() not in indexed, "duplicate/escaping archive artifact")
        indexed[str(path).casefold()] = item["sha256"]
    source = archive / "source/apps/touch_probe/main.cpp"
    need(probe.name.lower() == "touch_probe.exe" and probe.parent == archive / "bin",
         "probe must be the frozen archive bin/touch_probe.exe")
    dependencies = sorted(p for p in probe.parent.iterdir() if p.suffix.lower() == ".dll")
    need(len(dependencies) <= 32, "probe dependency budget exceeded")
    pins = [ref(p) for p in (manifest, probe, source, *dependencies)]
    for pin in pins[1:]:
        need(indexed.get(str(Path(pin["file"]).resolve()).casefold()) == pin["sha256"],
             "probe/source/DLL does not match frozen build archive")
    need("--no-images" in source.read_text(encoding="utf-8-sig"), "frozen probe lacks required no-images option")
    return dict(probe=pins[1], probe_source=pins[2], probe_build_manifest=pins[0], probe_dependencies=pins[3:])


def probe_arguments(config, output, capacity, geometry_profile, *, inspect=False):
    restart.checked_capacity(capacity)
    need(geometry_profile in PROFILES, "unsupported explicit fixture geometry profile")
    return [config["probe"]["file"], "run", restart.SERIAL, restart.AVD, restart.APK_SHA,
        "capacity", str(capacity), "1280", "720", str(output), geometry_profile,
        "--inspect-only" if inspect else "--fixture-input", "--no-images"]


def native_snapshot(raw, name):
    """Read a bounded snapshot without blessing an incomplete append as closed."""
    need(0 < len(raw) <= MAX_NATIVE, "native recorder byte budget exceeded")
    match = re.fullmatch(r"touch-evidence-([1-9][0-9]{0,9})-([0-9]{1,19})\.jsonl", name)
    need(match is not None, "unsafe native recorder filename")
    lines = raw.splitlines(keepends=True)
    if not raw.endswith(b"\n"):
        return dict(snapshot_complete=False, issues=["partial_tail"], parsed_records=0, header=None)
    need(len(lines) <= 8194, "native recorder record budget exceeded")
    rows = [restart.decode(line) for line in lines]
    header = rows[0]
    need(header.get("type") == "header" and header.get("schema") == "pas.android.touch-evidence.v1" and
         header.get("fixture") == "native-evidence-v3" and
         type(header.get("start_monotonic_ns")) is int and header["start_monotonic_ns"] == int(match[2]) and
         header.get("max_events") == 8192 and header.get("max_bytes") == MAX_NATIVE and
         header.get("max_duration_ns") == 60_000_000_000, "fresh recorder header identity/limits mismatch")
    footers = [i for i, row in enumerate(rows) if row.get("type") == "footer"]
    if not footers:
        need(all(row.get("type") == "motion" for row in rows[1:]), "unexpected native recorder row")
        return dict(snapshot_complete=False, issues=["missing_terminal_footer"], parsed_records=len(rows), header=header)
    need(footers == [len(rows) - 1], "native recorder footer is ambiguous/nonterminal")
    footer = rows[-1]
    need(footer.get("fault") is False and footer.get("lifecycle_errors") == 0 and
         footer.get("first_unrecorded_events") == 0 and footer.get("active_after_last_observation") == 0 and
         footer.get("further_events_not_observed") is True and
         footer.get("release_observation") == "zero_active_in_observed_stream" and
         footer.get("events_written") == len(rows) - 2 and
         footer.get("bytes_before_footer") == sum(map(len, lines[:-1])), "native recorder did not close completely at zero")
    return dict(snapshot_complete=True, issues=[], parsed_records=len(rows), header=header, footer=footer)


def validate_host(host, capacity, profile):
    restart.checked_capacity(capacity)
    need(profile in PROFILES, "unsupported geometry profile")
    need(host.get("schema") == "pas.touch-probe.v1" and host.get("serial") == restart.SERIAL and
         host.get("avd") == restart.AVD and host.get("case") == "capacity" and
         type(host.get("capacity_requested")) is int and host["capacity_requested"] == capacity and
         type(host.get("contacts_in_case")) is int and host["contacts_in_case"] == capacity,
         "host probe target/capacity mismatch")
    need(host.get("fixture_package") == restart.PACKAGE and host.get("fixture_apk_sha256") == restart.APK_SHA and
         host.get("input_enabled") is True and host.get("user_stop_requested") is False and
         host.get("status") == "fixture_visual_path_passed" and host.get("path_verified") is True and
         host.get("zero_active_observed") is True and host.get("release_rpc_known") is True and
         host.get("events_log_complete") is True and host.get("failure") in (None, "") and
         host.get("image_retention") == "none", "probe path/release/no-images proof failed")
    final = host.get("final_evidence", {})
    need(final.get("complete") is True and type(final.get("active")) is int and final["active"] == 0 and
         final.get("error") == "" and host.get("release", {}).get("failed") == [] and
         host.get("release", {}).get("unknown") == [], "probe final release is not proven zero")
    geometry = dict(profile=profile, normalized_width=1280, normalized_height=720,
                    touch_width=720, touch_height=1280, touch_rotation=90, **PROFILES[profile])
    need(all(type(host.get("geometry", {}).get(k)) is type(v) and host["geometry"][k] == v
             for k, v in geometry.items()), "actual host geometry differs from explicit profile")
    return host


class CurrentBootCapacity(restart.Bootstrap):
    def __init__(self, config, output, runner=bounded_readonly):
        self.c, self.root, self.runner = config, Path(output), runner
        self.counter = 0

    def state(self, stage, detail, **extra):
        value = dict(schema="pas.current-boot-capacity-progress.v1", stage=stage, detail=detail,
                     host_qpc_ns=time.perf_counter_ns(), **extra)
        temporary = self.root / "progress.tmp"
        temporary.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
        os.replace(temporary, self.root / "progress.json")
        print(json.dumps(value, ensure_ascii=False), flush=True)

    def package_hash(self):
        installed = self.command(["shell", "pm", "path", restart.PACKAGE], limit=8192)
        need(re.fullmatch(r"package:/data/app/[A-Za-z0-9_/.=+~\-]+/base\.apk", installed),
             "expected exactly one installed v3 fixture APK")
        path = installed.removeprefix("package:")
        need(self.command(["shell", "sha256sum", path], limit=1024).split() == [restart.APK_SHA, path],
             "installed fixture APK hash differs")
        return restart.APK_SHA

    def fixture_identity(self):
        boot = self.boot_id()
        pid = self.command(["shell", "pidof", restart.PACKAGE], limit=1024)
        need(re.fullmatch(r"[1-9][0-9]{0,9}", pid), "fixture PID absent or ambiguous")
        stat = self.command(["exec-out", "run-as", restart.PACKAGE, "cat", "/proc/" + pid + "/stat"], limit=8192)
        need(stat.startswith(pid + " (") and ") " in stat, "fixture process identity unavailable")
        fields = stat.rsplit(") ", 1)[1].split()
        need(len(fields) >= 20 and fields[19].isdigit(), "fixture process start time unavailable")
        return dict(boot_id=boot, pid=int(pid), process_start_ticks=int(fields[19]),
                    installed_apk_sha256=self.package_hash(), package=restart.PACKAGE)

    def execute(self):
        capacity, geometry = self.c["fixture_capacity"], self.c["geometry_profile"]
        self.state("checking", "核對目前已開機的 phigros、fixture 與唯讀 helpers")
        need(self.command(["get-state"], limit=1024) == "device", "explicit emulator is not online")
        need(self.command(["shell", "getprop", "sys.boot_completed"], limit=256) == "1", "Android boot incomplete")
        need(self.command(["emu", "avd", "name"], limit=1024).splitlines()[0] == restart.AVD, "AVD mismatch")
        version = self.command(["shell", "dumpsys", "package", "com.PigeonGames.Phigros"])
        need(re.search(r"\bversionName=4\.0\.1(?:\s|$)", version) and
             re.search(r"\bversionCode=157(?:\s|$)", version), "installed game must be 4.0.1 / 157")
        boot = self.boot_id(); self.package_hash()
        helpers = []
        for remote, expected, name in ((restart.REMOTE_SNAP, restart.SNAP_SHA, "snapshot-helper"),
                                        (restart.REMOTE_STREAM, restart.STREAM_SHA, "stream-helper")):
            need(self.command(["shell", "sha256sum", remote], limit=1024).split() == [expected, remote], "remote helper hash differs")
            raw = self.command(["exec-out", "cat", remote], limit=8*1024*1024, raw=True)
            need(hashlib.sha256(raw).hexdigest() == expected, "downloaded helper bytes/hash differ")
            path = self.root / name
            with path.open("xb") as stream: stream.write(raw)
            helpers.append(dict(remote=remote, downloaded=ref(path)))
        routes = self.command(["shell", "getevent", "-lp"], limit=256*1024)
        matches = []
        for block in re.split(r"(?=add device \d+:)", routes):
            found = re.search(r"add device \d+:\s*(/dev/input/event[0-9]{1,3})", block)
            if found and re.search(r'name:\s*"virtio_input_multi_touch_1"', block): matches.append(found[1])
        need(matches == ["/dev/input/event2"], "expected unique native kernel route unavailable")
        raw = self.command(["shell", restart.REMOTE_SNAP, matches[0]], limit=8192, raw=True)
        need(len(raw.splitlines()) == 1, "one kernel snapshot required")
        snap = restart.validate_snapshot(restart.decode(raw), boot, capacity=capacity)
        need(snap["rdev"] == 3394 and snap["slot_count"] == 11, "current kernel route is unsupported by native runtime")
        profile = dict(schema="pas.first-tap-kernel-profile.v1", boot_id=boot,
            **{k: snap[k] for k in ("path", "name", "rdev", "slot_count")},
            helper=restart.REMOTE_SNAP, helper_sha256=restart.SNAP_SHA,
            evidence="actual existing current boot; fresh route and zero snapshot; no cold-boot claim")
        write_new(self.root/"kernel-profile.json", profile)
        self.snapshot(profile, "kernel-before.json")
        previous = set(self.listing(optional=True).splitlines())
        self.command(["shell", "setprop", "debug.pas.touch_evidence", "1"], limit=1024)
        need(self.command(["shell", "getprop", "debug.pas.touch_evidence"], limit=1024) == "1", "fixture recorder not enabled")
        self.command(["shell", "am", "force-stop", restart.PACKAGE], limit=1024)
        self.command(["shell", "am", "start", "-W", "-n", restart.PACKAGE+"/android.app.NativeActivity"], timeout=20)
        identity = self.fixture_identity()
        need(identity["boot_id"] == boot, "boot changed during fixture launch")
        deadline = time.monotonic()+5
        while True:
            listing = self.listing()
            candidates = [n for n in listing.splitlines() if n not in previous and
                re.fullmatch(r"touch-evidence-"+str(identity["pid"])+r"-[0-9]{1,19}\.jsonl", n)]
            if candidates:
                name = restart.select_new_recording(listing, previous, identity["pid"]); break
            need(time.monotonic()<deadline, "new native recorder did not appear")
            time.sleep(.1)
        native_args = ["exec-out", "run-as", restart.PACKAGE, "cat", "files/"+name]
        deadline = time.monotonic()+2
        while True:
            before = self.command(native_args, limit=MAX_NATIVE, raw=True)
            if before and before.endswith(b"\n"):
                initial = native_snapshot(before, name)
                break
            need(time.monotonic()<deadline,"new recorder header did not finish writing; no input permitted")
            time.sleep(.05)
        need(initial["parsed_records"] == 1 and initial["header"] is not None and not initial["snapshot_complete"],
             "fresh fixture recorder is not header-only; no input permitted")
        inspection = self.root/"fixture-inspect"
        self.owned("fixture-inspect", probe_arguments(self.c, inspection, capacity, geometry, inspect=True), timeout=15)
        inspected = read(inspection/"manifest.json")
        need(inspected.get("input_enabled") is False and inspected.get("image_retention") == "none" and
             inspected.get("status") == "fixture_capture_signature_checked_no_input", "fixture readiness inspection failed")
        self.state("fixture", f"開始本次開機的 {capacity} 指 fixture；不進入遊戲、不存圖片")
        stream_path = self.root/"kernel-stream.jsonl"
        with stream_path.open("xb") as out, (self.root/"kernel-stream.stderr.log").open("xb") as err:
            observer = subprocess.Popen([self.c["adb"]["file"], "-s", restart.SERIAL, "shell", restart.REMOTE_STREAM,
                "--interval-ms", "40", "--duration-ms", "20000", "--max-rounds", "512", profile["path"]],
                stdout=out, stderr=err, creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
            try:
                deadline = time.monotonic()+5
                while stream_path.read_bytes().count(b"\n")<2:
                    self.stop_check()
                    need(observer.poll() is None and time.monotonic()<deadline, "read-only kernel observer not ready")
                    time.sleep(.02)
                first = [restart.decode(line) for line in stream_path.read_bytes().splitlines()[:2]]
                need(first[0].get("boot_id") == boot and first[1].get("active_count") == 0, "kernel observer baseline not zero")
                probe_dir = self.root/"fixture-probe"
                self.owned("capacity-probe", probe_arguments(self.c, probe_dir, capacity, geometry), probe_dir=probe_dir, timeout=25)
            finally:
                try: observer.wait(timeout=25)
                except subprocess.TimeoutExpired:
                    observer.kill(); observer.wait(timeout=3)
                    raise ValueError("read-only kernel observer timeout")
        need(observer.returncode==0, "read-only kernel observer failed")
        route = restart.validate_stream(stream_path.read_bytes(), boot, profile, capacity)
        write_new(self.root/"route-observation.json", route)
        host = validate_host(read(probe_dir/"manifest.json"), capacity, geometry)
        self.snapshot(profile,"kernel-after-probe.json")
        self.state("closing", "觸控已釋放，等待原生 recorder 約 60 秒安全封口")
        deadline = time.monotonic()+70
        while True:
            raw = self.command(native_args, limit=MAX_NATIVE, raw=True)
            status = native_snapshot(raw, name)
            if status["snapshot_complete"]: break
            need(time.monotonic()<deadline, "native recorder did not close within its bound")
            time.sleep(1)
        after = self.fixture_identity()
        need(after==identity and self.boot_id()==boot and raw.startswith(before), "native PID/starttime/boot/header association changed")
        native = self.root/name
        with native.open("xb") as stream: stream.write(raw)
        retrieval = dict(schema="pas.touch-evidence-retrieval.v1", status="retrieved_closed_snapshot",
            serial=restart.SERIAL, avd=restart.AVD, package=restart.PACKAGE, installed_apk_sha256=restart.APK_SHA,
            source_file=name, retrieved_bytes=len(raw), sha256=sha(native), identity_before=identity, identity_after=after,
            association_stable=True, snapshot=status, selection_policy="new filename + current PID/starttime + header clock + unchanged current boot",
            touch_sent=False, input_latency_ns=None, game_input_lead_ns=None)
        write_new(self.root/"retrieval-report.json",retrieval)
        analysis = analyze_touch_probe.analyze_probe(probe_dir/"manifest.json",probe_dir/"events.jsonl",native)
        android = restart.validate_analysis(analysis,capacity)
        write_new(self.root/"analysis.json",analysis)
        self.snapshot(profile,"kernel-final-zero.json")
        need(self.boot_id()==boot,"boot changed before final proof")
        for pin in self.c["pins"]: restart.checked_ref(pin)
        for helper in helpers: restart.checked_ref(helper["downloaded"])
        evidence = dict(schema="pas.fixture-capacity-evidence.v1", boot_id=boot, capacity=capacity,
            normal_path_valid=True, final_zero=True, fixture_apk_sha256=restart.APK_SHA,
            analysis=ref(self.root/"analysis.json"),host_manifest=ref(probe_dir/"manifest.json"),
            retrieval_report=ref(self.root/"retrieval-report.json"),
            artifacts=[ref(p) for p in (probe_dir/"manifest.json",probe_dir/"events.jsonl",native,
                self.root/"analysis.json",self.root/"retrieval-report.json",self.root/"kernel-final-zero.json",
                stream_path,self.root/"route-observation.json")],geometry=host["geometry"],
            native_peak_contacts=android["peak_contacts"],current_kernel_zero=True,
            input_latency_ns=None,game_input_lead_ns=None,live_game_qualified=False,
            boot_policy="existing-current-boot; emulator not restarted", image_retention="none",
            probe_build_manifest=self.c["probe_build_manifest"],probe_source=self.c["probe_source"],
            probe_binary=self.c["probe"],probe_dependencies=self.c["probe_dependencies"],helper_downloads=helpers,
            tool_pins=self.c["tool_pins"],
            scope="actual requested-capacity fixture DOWN/MOVE/UP and final zero on this existing boot; no game/latency qualification")
        need(len(json.dumps(evidence,ensure_ascii=False).encode("utf-8"))<65536,
             "capacity proof exceeds native finite metadata budget")
        write_new(self.root/"capacity-evidence.json",evidence)
        result = dict(schema="pas.current-boot-capacity-result.v1",status="current_boot_fixture_verified",
            actual_capacity=android["peak_contacts"],boot_id=boot,geometry_profile=geometry,
            kernel_profile=ref(self.root/"kernel-profile.json"),capacity_evidence=ref(self.root/"capacity-evidence.json"),
            emulator_restarted=False,game_launched=False,image_retention="none",gameplay_qualified=False)
        write_new(self.root/"result.json",result)
        self.state("ready", f"本次開機 {capacity} 指與最後零接觸已驗證；請自行返回 Phigros",
            result=ref(self.root/"result.json"),actual_capacity=android["peak_contacts"],boot_id=boot)
        return result


def verify(probe, build_manifest, output, capacity=10, geometry_profile="landscape-r1", adb=None, sdk=None, *, run=False):
    need(run is True, "explicit --run is required; no device or UI action was performed")
    restart.checked_capacity(capacity)
    need(geometry_profile in PROFILES,"unsupported explicit geometry profile")
    sealed = sealed_probe(probe,build_manifest)
    if adb is None:
        sdk = Path(sdk) if sdk else Path(os.environ.get("LOCALAPPDATA",""))/"Android/Sdk"
        adb = sdk/"platform-tools/adb.exe"
    adb_pin = ref(adb)
    output = Path(output).resolve()
    need(not output.exists() and output.parent.is_dir(),"output must be a new directory under an existing parent")
    tools = [ref(Path(__file__)),ref(Path(restart.__file__)),ref(Path(analyze_touch_probe.__file__))]
    config = dict(sealed,adb=adb_pin,fixture_capacity=capacity,geometry_profile=geometry_profile,tool_pins=tools)
    config["pins"] = [sealed["probe"],sealed["probe_source"],sealed["probe_build_manifest"],
        *sealed["probe_dependencies"],adb_pin,*tools]
    output.mkdir()
    write_new(output/"invocation.json",dict(schema="pas.current-boot-capacity-invocation.v1",explicit_run=True,
        capacity=capacity,geometry_profile=geometry_profile,emulator_restarted=False,image_retention="none",pins=config["pins"]))
    worker = CurrentBootCapacity(config,output)
    try: return worker.execute()
    except BaseException as error:
        worker.state("failed",str(error),failure_type=type(error).__name__,gameplay_qualified=False)
        write_new(output/"failure.json",dict(status="failed",error=str(error),error_type=type(error).__name__,
            capacity_evidence_created=(output/"capacity-evidence.json").is_file(),gameplay_qualified=False))
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run",action="store_true")
    for key in ("probe","build-manifest","output"):parser.add_argument("--"+key,type=Path,required=True)
    parser.add_argument("--capacity",type=int,choices=range(1,11),default=10)
    parser.add_argument("--geometry-profile",choices=PROFILES,default="landscape-r1")
    parser.add_argument("--adb",type=Path);parser.add_argument("--sdk",type=Path)
    args=parser.parse_args()
    result=verify(args.probe,args.build_manifest,args.output,args.capacity,args.geometry_profile,args.adb,args.sdk,run=args.run)
    print(json.dumps(result,ensure_ascii=False))
    return 0


if __name__=="__main__":
    try:raise SystemExit(main())
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        print(type(error).__name__+": "+str(error),file=__import__("sys").stderr)
        raise SystemExit(2)
