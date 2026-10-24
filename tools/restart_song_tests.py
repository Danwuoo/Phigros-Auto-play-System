"""User-invoked cold-boot/fixture bootstrap. Import and --plan are device-free.

No game is launched or selected. All native fixture input requires a separate
affirmative GUI action, after boot validation. Original evidence is never edited.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

SCHEMA = "pas.song-test-restart.v1"
APK_SHA = "d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2"
SNAP_SHA = "1bee0d00ef2e69bbad846729ddd73e27b8c61b48cd61e4c03ffc2ba897f77ccb"
STREAM_SHA = "8974022af633ff37d5af048279393cc78cad2f48f438271e9596a6d617eb1f79"
PACKAGE = "org.pas.touchfixture.evidence.cpp"
SERIAL, AVD = "emulator-5554", "phigros"
REMOTE_SNAP = "/data/local/tmp/pas-evdev-slots-" + SNAP_SHA[:16]
REMOTE_STREAM = "/data/local/tmp/pas-evdev-stream-" + STREAM_SHA[:16]
UUID = re.compile(r"[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}")
TOOL_KEYS = ("adb", "emulator", "avd_ini", "avd_config", "python", "powershell",
             "probe", "probe_build_manifest", "probe_source", "fixture_apk", "snapshot_helper", "stream_helper", "retrieval_tool",
             "analyzer", "fixture_launcher", "bootstrap_script", "worker", "panel_config")
EMULATOR_ARGS = ["-avd", AVD, "-port", "5554", "-grpc", "8554", "-grpc-use-token",
                 "-no-snapshot-load", "-no-snapshot-save"]


def need(condition, message):
    if not condition:
        raise ValueError(message)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def ref(path):
    path = Path(path).resolve(strict=True)
    need(path.is_file(), "file required: " + str(path))
    return {"file": str(path), "sha256": sha(path)}


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def decode(raw):
    return json.loads(raw, object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite JSON")))


def read(path, maximum=8 * 1024 * 1024):
    raw = Path(path).read_bytes()
    need(len(raw) <= maximum, "JSON size bound exceeded")
    return decode(raw.decode("utf-8-sig"))


def write_new(path, value):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def checked_ref(value):
    need(type(value) is dict and set(value) == {"file", "sha256"}, "invalid pinned file reference")
    need(type(value["file"]) is str and Path(value["file"]).is_absolute(), "absolute file required")
    need(type(value["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", value["sha256"]), "invalid SHA256")
    need(Path(value["file"]).is_file() and sha(value["file"]) == value["sha256"], "pinned file changed: " + value["file"])
    return Path(value["file"])


def ini(path):
    return dict(line.split("=", 1) for line in Path(path).read_text(encoding="utf-8-sig").splitlines()
                if "=" in line and not line.startswith("#"))


def load_config(path, expected_sha, *, running=False):
    need(re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha or ""), "restart config SHA256 is required")
    need(sha(path) == expected_sha.lower(), "restart config hash mismatch")
    config = read(path)
    need(config.get("schema") == SCHEMA and config.get("serial") == SERIAL and config.get("avd") == AVD,
         "only the known explicit phigros AVD is supported")
    need(config.get("emulator_arguments") == EMULATOR_ARGS, "unexpected emulator arguments")
    for key in TOOL_KEYS:
        if running and key in ("avd_ini", "avd_config"):
            # Emulator-owned metadata may be rewritten at boot. Preserve the
            # original pin, then validate current identity/geometry separately.
            need(Path(config[key]["file"]).is_absolute() and Path(config[key]["file"]).is_file(), "AVD metadata missing")
        else:
            checked_ref(config[key])
    need(config["fixture_apk"]["sha256"] == APK_SHA and config["snapshot_helper"]["sha256"] == SNAP_SHA
         and config["stream_helper"]["sha256"] == STREAM_SHA, "unqualified fixture/helper artifact")
    deps = config.get("probe_dependencies")
    need(type(deps) is list and 1 <= len(deps) <= 32, "bounded pinned probe dependencies required")
    declared = {Path(config["probe"]["file"]).resolve()}
    for item in deps:
        p = checked_ref(item).resolve()
        need(p.parent == Path(config["probe"]["file"]).parent.resolve(), "probe DLL outside its archive")
        declared.add(p)
    for p in Path(config["probe"]["file"]).parent.iterdir():
        if p.suffix.lower() == ".dll":
            need(p.resolve() in declared, "unpinned probe DLL")
    probe_manifest_path = Path(config["probe_build_manifest"]["file"])
    probe_archive = probe_manifest_path.parent
    probe_manifest = read(probe_manifest_path)
    need(probe_manifest.get("schema") == "pas.adaptive-build-archive.v1" and
         type(probe_manifest.get("artifacts")) is list and len(probe_manifest["artifacts"]) <= 2048, "invalid frozen probe archive")
    archived = {str((probe_archive / item["file"]).resolve()).casefold(): item["sha256"]
                for item in probe_manifest["artifacts"]}
    for item in [config["probe"], config["probe_source"], *deps]:
        need(archived.get(str(Path(item["file"]).resolve()).casefold()) == item["sha256"], "probe binary/source/DLL differs from frozen archive")
    avd_ini, avd = ini(config["avd_ini"]["file"]), ini(config["avd_config"]["file"])
    need(Path(avd_ini.get("path", "")).resolve() == Path(config["avd_config"]["file"]).parent.resolve()
         and avd.get("AvdId") == AVD and avd.get("hw.lcd.width") == "720"
         and avd.get("hw.lcd.height") == "1280" and avd.get("abi.type") == "x86_64", "AVD identity/geometry changed")
    panel = read(config["panel_config"]["file"])
    need(panel.get("schema") == "pas.manual-song-panel-launch.v1" and panel.get("manual_selection") is True
         and panel.get("manual_start") is True, "manual panel config required")
    need(Path(config["output_parent"]).is_absolute() and Path(config["output_parent"]).is_dir(), "output parent unavailable")
    need(Path(panel["output_parent"]).resolve() == Path(config["output_parent"]).resolve(), "output parent differs from panel")
    return config


def make_config(panel_path, output):
    """Offline sealing helper; the caller separately pins this config's SHA."""
    repo = Path(__file__).resolve().parents[1]
    base = repo.parent.parent
    assets, work = repo.parent / "local-assets/phigros-4.0.1", base / "work"
    sdk = Path(os.environ["LOCALAPPDATA"]) / "Android/Sdk"
    pwsh = Path(os.environ["USERPROFILE"]) / ".cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe"
    probe = assets / "glaciaxion-adaptive-build-003/bin/touch_probe.exe"
    paths = dict(adb=sdk / "platform-tools/adb.exe", emulator=sdk / "emulator/emulator.exe",
                 avd_ini=Path(os.environ["USERPROFILE"]) / ".android/avd/phigros.ini",
                 avd_config=Path(os.environ["USERPROFILE"]) / ".android/avd/phigros.avd/config.ini",
                 python=Path(sys.executable), powershell=pwsh, probe=probe,
                 probe_build_manifest=probe.parent.parent / "manifest.json",
                 probe_source=probe.parent.parent / "source/apps/touch_probe/main.cpp",
                 fixture_apk=assets / "m6-touch-fixtures/pas-touch-evidence-v3.apk",
                 snapshot_helper=work / "android-touch-slots", stream_helper=work / "android-touch-slots-stream",
                 retrieval_tool=work / "pull_touch_evidence.py", analyzer=repo / "tools/analyze_touch_probe.py",
                 fixture_launcher=repo / "tools/start-touch-evidence.ps1", bootstrap_script=repo / "tools/restart-song-tests.ps1",
                 worker=Path(__file__), panel_config=Path(panel_path))
    panel = read(panel_path)
    config = dict(schema=SCHEMA, serial=SERIAL, avd=AVD, emulator_arguments=EMULATOR_ARGS,
                  output_parent=panel["output_parent"], fixture_capacity=5, automatic_game_launch=False,
                  manual_fixture_consent_required=True, manual_selection=True, manual_start=True,
                  probe_dependencies=[ref(p) for p in sorted(probe.parent.glob("*.dll"))],
                  **{key: ref(path) for key, path in paths.items()})
    desktop = Path(os.environ["USERPROFILE"]) / "Desktop/Phigros Test Panel.lnk"
    config["desktop_shortcut"] = ref(desktop) if desktop.is_file() else None
    write_new(output, config)
    load_config(output, sha(output))
    return ref(output)


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def select_new_recording(listing, previous, pid):
    names = listing.splitlines()
    need(len(names) <= 1024 and len(names) == len(set(names)), "invalid bounded fixture listing")
    candidates = [name for name in names if name not in previous and
                  re.fullmatch(r"touch-evidence-" + str(pid) + r"-[0-9]{1,19}\.jsonl", name)]
    need(len(candidates) == 1, "fresh current-PID recording is absent or ambiguous")
    return candidates[0]


def validate_snapshot(row, boot, expected=None, zero=True):
    need(row.get("schema") == "pas.evdev-slot-snapshot.v1" and row.get("boot_id") == boot,
         "snapshot schema/boot mismatch")
    need(re.fullmatch(r"/dev/input/event[0-9]{1,3}", row.get("path", "")) and
         row.get("name") == "virtio_input_multi_touch_1" and row.get("error_errno") == 0, "unexpected touch route")
    need(type(row.get("rdev")) is int and row["rdev"] > 0 and type(row.get("slot_count")) is int
         and 5 <= row["slot_count"] <= 64, "unqualified slot metadata")
    ids = row.get("tracking_ids")
    need(type(ids) is list and len(ids) == row["slot_count"] and all(type(i) is int and i >= -1 for i in ids), "invalid slots")
    need(type(row.get("active_count")) is int and row["active_count"] == sum(i >= 0 for i in ids), "slot count disagreement")
    if zero:
        need(row["active_count"] == 0 and all(i == -1 for i in ids), "kernel slots are not zero")
    need(row.get("input_enabled") is False and row.get("grab_enabled") is False and
         row.get("game_callback_verified") is False, "helper must remain read-only")
    begin, end = row.get("android_monotonic_before_ns"), row.get("android_monotonic_after_ns")
    need(type(begin) is int and type(end) is int and 0 < begin <= end, "invalid Android interval")
    if expected:
        need(all(row.get(key) == expected[key] for key in ("path", "name", "rdev", "slot_count")), "kernel route changed")
    return row


def validate_stream(raw, boot, profile):
    need(0 < len(raw) <= 16 * 1024 * 1024 and raw.endswith(b"\n"), "bounded complete kernel stream required")
    rows = [decode(line) for line in raw.splitlines()]
    need(3 <= len(rows) <= 514, "kernel stream record count exceeded")
    header, footer = rows[0], rows[-1]
    need(header.get("type") == "header" and header.get("schema") == "pas.evdev-slot-stream.v1"
         and header.get("boot_id") == boot and header.get("input_enabled") is False
         and header.get("grab_enabled") is False, "kernel stream identity invalid")
    devices = header.get("devices")
    need(type(devices) is list and len(devices) == 1 and devices[0].get("initialization_errno") == 0
         and all(devices[0].get(k) == profile[k] for k in ("path", "name", "rdev", "slot_count")), "stream route mismatch")
    need(footer.get("type") == "footer" and footer.get("fault") is False
         and footer.get("partial_final_round") is False and footer.get("samples_written") == len(rows) - 2,
         "kernel stream incomplete")
    previous, active = 0, []
    for index, sample in enumerate(rows[1:-1], 1):
        need(sample.get("type") == "sample" and sample.get("sequence") == index
             and sample.get("boot_id") == boot and sample.get("error_errno") == 0
             and all(sample.get(k) == profile[k] for k in ("path", "name", "rdev", "slot_count")), "kernel sample identity invalid")
        begin, end = sample.get("android_monotonic_before_ns"), sample.get("android_monotonic_after_ns")
        need(type(begin) is int and type(end) is int and previous <= begin <= end, "kernel stream time invalid")
        previous = end
        ids = sample.get("tracking_ids")
        need(type(ids) is list and len(ids) == profile["slot_count"] and all(type(i) is int and i >= -1 for i in ids), "kernel sample slots invalid")
        count = sum(i >= 0 for i in ids)
        need(type(sample.get("active_count")) is int and sample["active_count"] == count and count <= 5, "kernel sample count invalid")
        active.append(count)
    need(active[0] == 0 and active[-1] == 0 and max(active) == 5, "actual fresh route did not observe zero/five/zero")
    return dict(schema="pas.restart-route-observation.v1", boot_id=boot, path=profile["path"],
                samples=len(active), peak_contacts=max(active), baseline_zero=True, final_zero=True,
                scope="actual simultaneous kernel slots; no host/Android clock map or game judgment qualification")


def validate_probe(value):
    need(value.get("schema") == "pas.touch-probe.v1" and value.get("serial") == SERIAL and value.get("avd") == AVD
         and value.get("case") == "capacity" and value.get("capacity_requested") == 5, "probe identity mismatch")
    need(value.get("fixture_package") == PACKAGE and value.get("fixture_apk_sha256") == APK_SHA
         and value.get("input_enabled") is True and value.get("user_stop_requested") is False
         and value.get("contacts_in_case") == 5 and value.get("width") == 1280 and value.get("height") == 720,
         "probe fixture/input geometry identity mismatch")
    need(value.get("status") == "fixture_visual_path_passed" and value.get("path_verified") is True
         and value.get("zero_active_observed") is True and value.get("release_rpc_known") is True
         and value.get("events_log_complete") is True and value.get("failure") in (None, ""), "fixture probe/release not verified")
    final = value.get("final_evidence", {})
    need(type(final.get("active")) is int and final["active"] == 0 and final.get("complete") is True
         and final.get("error") == "", "final fixture state is not zero")
    need(value.get("release", {}).get("failed") == [] and value.get("release", {}).get("unknown") == [], "uncertain fixture release")
    expected = dict(normalize_ccw90=True, normalized_width=1280, normalized_height=720,
                    raw_width=720, raw_height=1280, raw_rotation=0, touch_width=720,
                    touch_height=1280, touch_rotation=90, profile="sensor-portrait-r0-ccw90")
    need(all(type(value.get("geometry", {}).get(k)) is type(v) and value["geometry"][k] == v for k, v in expected.items()), "fixture geometry mismatch")
    return value


def validate_analysis(value):
    need(value.get("schema") == "pas.touch-probe-analysis.v1" and value.get("status") == "passed_observed_fixture_path"
         and value.get("failures") == [] and value.get("incomplete_reasons") == [], "actual native five-contact path analysis failed")
    android = value.get("android", {})
    need(type(android.get("peak_contacts")) is int and android["peak_contacts"] == 5 and all(android.get(k) is True for k in
         ("stream_complete", "expected_path_verified", "pointer_lifecycle_valid", "final_zero_observed")), "actual Android capacity evidence incomplete")
    return android


def derive_panel(template, kernel_ref, capacity_ref):
    need(template.get("schema") == "pas.manual-song-panel-launch.v1" and
         template.get("manual_selection") is True and template.get("manual_start") is True, "manual panel required")
    result = copy.deepcopy(template)
    result["kernel_profile"], result["capacity_evidence"] = kernel_ref, capacity_ref
    return result


class Bootstrap:
    def __init__(self, config, session, *, readonly_runner=None):
        self.c, self.root = config, Path(session).resolve()
        self.pull = module(config["retrieval_tool"]["file"], "pas_restart_retrieval")
        self.runner = readonly_runner or self.pull.run_bounded
        self.counter = 0
        self.root.mkdir(exist_ok=True)

    def state(self, stage, detail, **extra):
        value = dict(schema="pas.song-test-restart-progress.v1", stage=stage, detail=detail,
                     host_qpc_ns=time.perf_counter_ns(), **extra)
        temp = self.root / "progress.tmp"
        temp.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
        os.replace(temp, self.root / "progress.json")

    def stop_check(self):
        need(not (self.root / "STOP").exists(), "user_stop")

    def command(self, tail, *, limit=131072, timeout=10, allowed=(0,), raw=False):
        self.stop_check()
        argv = [self.c["adb"]["file"], "-s", SERIAL, *tail]
        start = time.perf_counter_ns()
        result = self.runner(argv, limit, timeout)
        end = time.perf_counter_ns()
        self.counter += 1
        log = self.root / (f"{time.time_ns()}-{self.counter:03d}-adb.bin")
        log.write_bytes(result.stdout)
        with (self.root / "commands.jsonl").open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(dict(argv=argv, host_before_ns=start, host_after_ns=end,
                          returncode=result.returncode, timed_out=result.timed_out,
                          output_limited=result.output_limited, readers_closed=result.readers_closed,
                          stdout=ref(log))) + "\n")
        need(not result.timed_out and not result.output_limited and result.readers_closed
             and result.returncode in allowed, "device setup/read failed: " + " ".join(tail[:3]))
        return result.stdout if raw else result.stdout.decode("utf-8", errors="strict").strip()

    def boot_id(self):
        boot = self.command(["shell", "cat", "/proc/sys/kernel/random/boot_id"], limit=256)
        need(UUID.fullmatch(boot), "invalid fresh Android boot UUID")
        return boot

    def snapshot(self, profile, label):
        start = time.perf_counter_ns()
        raw = self.command(["shell", REMOTE_SNAP, profile["path"]], limit=8192, raw=True)
        end = time.perf_counter_ns()
        lines = raw.splitlines()
        need(len(lines) == 1, "one explicit kernel snapshot required")
        row = validate_snapshot(decode(lines[0]), profile["boot_id"], profile)
        row.update(host_request_begin_ns=start, host_reply_end_ns=end, causal_host_interval_valid=True,
                   helper_sha256=SNAP_SHA, reply_sha256=hashlib.sha256(raw).hexdigest(),
                   scope="read-only current kernel snapshot; no cross-clock latency qualification")
        write_new(self.root / label, row)
        return row

    def preboot(self):
        self.state("checking", "核對封存與模擬器關閉狀態")
        # Explicit target query only: no device enumeration or selection.
        status = self.command(["get-state"], limit=1024, allowed=(0, 1))
        need(status != "device", "emulator-5554 is already online; close the emulator before cold boot")
        need(not (self.root / "preboot.json").exists(), "session was already started")
        old = read(self.c["panel_config"]["file"])
        old_kernel = read(checked_ref(old["kernel_profile"]))
        write_new(self.root / "preboot.json", dict(schema="pas.restart-preboot.v1", prior_boot_id=old_kernel["boot_id"],
                   restart_config=self.c["_config_ref"], emulator_arguments=EMULATOR_ARGS, user_initiated=True))
        self.state("cold_boot_allowed", "準備以冷開機啟動 phigros AVD")

    def boot(self):
        pre = read(self.root / "preboot.json")
        self.state("booting", "等待 Android 開機（最多 180 秒）")
        deadline = time.monotonic() + 180
        while True:
            self.stop_check()
            try:
                ready = self.command(["shell", "getprop", "sys.boot_completed"], limit=256, timeout=3)
                if ready == "1":
                    break
            except ValueError:
                pass
            need(time.monotonic() < deadline, "Android cold boot did not finish in 180 seconds")
            time.sleep(1)
        avd = [x.strip() for x in self.command(["emu", "avd", "name"], limit=1024).splitlines() if x.strip()]
        need(avd in ([AVD], [AVD, "OK"]), "actual AVD mismatch")
        boot = self.boot_id()
        need(boot != pre["prior_boot_id"], "boot UUID was reused; fresh boot evidence required")
        self.state("route", "核對新的觸控路徑、唯讀 helper 與零接觸")
        routes = self.command(["shell", "getevent", "-lp"], limit=256 * 1024)
        (self.root / "route.txt").write_text(routes, encoding="utf-8")
        blocks = re.split(r"(?=add device \d+:)", routes)
        matches = []
        for block in blocks:
            found = re.search(r"add device \d+:\s*(/dev/input/event[0-9]{1,3})", block)
            if found and re.search(r'name:\s*"virtio_input_multi_touch_1"', block):
                need(all(key in block for key in ("ABS_MT_SLOT", "ABS_MT_TRACKING_ID", "ABS_MT_POSITION_X", "ABS_MT_POSITION_Y")), "native MT capabilities missing")
                matches.append(found[1])
        need(len(matches) == 1, "touch route is absent or ambiguous")
        for key, remote in (("snapshot_helper", REMOTE_SNAP), ("stream_helper", REMOTE_STREAM)):
            self.command(["push", self.c[key]["file"], remote], limit=8192, timeout=15)
            self.command(["shell", "chmod", "700", remote], limit=1024)
            digest = self.command(["shell", "sha256sum", remote], limit=1024).split()
            need(digest == [self.c[key]["sha256"], remote], "remote helper hash mismatch")
        raw = self.command(["shell", REMOTE_SNAP, matches[0]], limit=8192, raw=True)
        lines = raw.splitlines()
        need(len(lines) == 1, "ambiguous route snapshot")
        snap = validate_snapshot(decode(lines[0]), boot)
        profile = dict(schema="pas.first-tap-kernel-profile.v1", boot_id=boot,
                       **{key: snap[key] for key in ("path", "name", "rdev", "slot_count")},
                       helper=REMOTE_SNAP, helper_sha256=SNAP_SHA,
                       evidence="fresh user-invoked cold boot; actual route + helper hash + zero snapshot; five-contact qualification follows separately")
        write_new(self.root / "kernel-profile.json", profile)
        self.snapshot(profile, "kernel-before.json")
        installed = self.command(["shell", "pm", "path", PACKAGE], limit=8192, allowed=(0, 1))
        if not installed:
            self.command(["install", "--no-streaming", self.c["fixture_apk"]["file"]], limit=8192, timeout=45)
            installed = self.command(["shell", "pm", "path", PACKAGE], limit=8192)
        need(re.fullmatch(r"package:/data/app/[A-Za-z0-9_/.=+~\-]+/base\.apk", installed), "fixture package split/path invalid")
        apk = installed.removeprefix("package:")
        need(self.command(["shell", "sha256sum", apk], limit=1024).split() == [APK_SHA, apk], "installed fixture APK mismatch")
        need(self.boot_id() == boot, "boot changed during setup")
        self.state("fixture_consent_required", "開機與零接觸檢查完成；等待您確認五指測試")

    def owned(self, label, argv, *, probe_dir=None, timeout=30):
        """Never kill an input owner; STOP is sticky and release is its responsibility."""
        self.stop_check()
        for value in getattr(self, "c", {}).values():
            if type(value) is dict and value.get("file") == str(argv[0]):
                checked_ref(value)
        if getattr(self, "c", {}).get("probe", {}).get("file") == str(argv[0]):
            for dependency in self.c["probe_dependencies"]:
                checked_ref(dependency)
        stdout, stderr = self.root / (label + ".stdout.log"), self.root / (label + ".stderr.log")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            process = subprocess.Popen([str(a) for a in argv], stdout=out, stderr=err,
                                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            try:
                write_new(self.root / (label + ".process.json"), dict(argv=[str(a) for a in argv], pid=process.pid,
                          host_started_ns=time.perf_counter_ns(), forced_termination=False))
                deadline = time.monotonic() + timeout
                while process.poll() is None:
                    if (self.root / "STOP").exists() or time.monotonic() > deadline:
                        raise ValueError(label + " STOP/deadline requested")
                    time.sleep(.05)
                need(process.returncode == 0, label + " failed; inspect saved logs")
            finally:
                # Covers EVERY post-Popen exception (metadata, clock, progress,
                # KeyboardInterrupt, etc.), not just known I/O operations. The
                # original error escapes only after the input owner has exited.
                self.drain_input_owner(process, probe_dir)
        self.stop_check()

    def drain_input_owner(self, process, probe_dir):
        while True:
            try:
                if process.poll() is not None:
                    return
            except BaseException:
                pass  # An unreadable handle is never proof of owner exit.
            try:
                # This is deliberately retried: output may be created AFTER the
                # stop/fault, or the first STOP write may itself fail.
                if probe_dir and probe_dir.is_dir():
                    (probe_dir / "STOP").touch(exist_ok=True)
            except BaseException:
                pass
            try:
                self.state("releasing", "停止已要求；等待原生程式釋放觸控（不強制關閉）", owner_pid=process.pid)
            except BaseException:
                pass
            try:
                time.sleep(.05)
            except BaseException:
                pass

    def listing(self, optional=False):
        # A newly installed fixture may not have created files/ yet. Only this
        # prelaunch listing may be absent; the postlaunch recording must exist.
        return self.command(["exec-out", "run-as", PACKAGE, "ls", "-1", "files"],
                            limit=128 * 1024, allowed=(0, 1) if optional else (0,))

    def fixture(self, consent):
        need(consent, "explicit affirmative fixture consent required")
        profile = read(self.root / "kernel-profile.json")
        need(self.boot_id() == profile["boot_id"], "boot changed before fixture consent")
        self.snapshot(profile, "kernel-pre-fixture.json")
        previous = set(self.listing(optional=True).splitlines())
        self.state("fixture", "開始五指 DOWN／MOVE／UP；請勿碰觸模擬器")
        self.owned("fixture-launch", [self.c["powershell"]["file"], "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                   "-File", self.c["fixture_launcher"]["file"], "-Serial", SERIAL, "-Avd", AVD, "-Adb", self.c["adb"]["file"]], timeout=25)
        call = lambda args, limit, timeout: self.runner([self.c["adb"]["file"], "-s", SERIAL, *args], limit, timeout)
        identity = self.pull.current_identity(call)
        need(identity["boot_id"] == profile["boot_id"], "fixture boot changed")
        recording_deadline = time.monotonic() + 5
        while True:
            listing = self.listing()
            new_names = [n for n in listing.splitlines() if n not in previous and
                         re.fullmatch(r"touch-evidence-" + str(identity["pid"]) + r"-[0-9]{1,19}\.jsonl", n)]
            if new_names:
                name = select_new_recording(listing, previous, identity["pid"])
                break
            need(time.monotonic() < recording_deadline, "fresh fixture recorder did not start")
            time.sleep(.1)
        before_raw = self.command(["exec-out", "run-as", PACKAGE, "cat", "files/" + name], limit=self.pull.MAX_RAW_BYTES, raw=True)
        before_status = self.pull.snapshot_status(before_raw, name)
        need(before_status["parsed_records"] == 1 and before_status["header"] is not None
             and before_status["issues"] == ["missing_ambiguous_or_nonterminal_footer"],
             "new recorder is not clean/header-only; no fixture input")
        # Run the read-only observer before any fixture DOWN. Its complete stream
        # independently ties the newly selected evdev route to five actual slots.
        stream_path = self.root / "kernel-stream.jsonl"
        stream_err = self.root / "kernel-stream.stderr.log"
        with stream_path.open("xb") as out, stream_err.open("xb") as err:
            observer = subprocess.Popen([self.c["adb"]["file"], "-s", SERIAL, "shell", REMOTE_STREAM,
                     "--interval-ms", "40", "--duration-ms", "20000", "--max-rounds", "512", profile["path"]],
                     stdout=out, stderr=err, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            try:
                deadline = time.monotonic() + 5
                while True:
                    self.stop_check()
                    raw = stream_path.read_bytes()
                    if raw.count(b"\n") >= 2:
                        break
                    need(observer.poll() is None and time.monotonic() < deadline, "kernel observer not ready; no input")
                    time.sleep(.02)
                first_rows = [decode(line) for line in raw.splitlines()[:2]]
                need(first_rows[0].get("boot_id") == profile["boot_id"] and first_rows[1].get("active_count") == 0,
                     "observer has no clean baseline; no input")
                probe = self.root / "fixture-probe"
                self.owned("five-contact-probe", [self.c["probe"]["file"], "run", SERIAL, AVD, APK_SHA,
                    "capacity", "5", "1280", "720", probe, "sensor-portrait-r0-ccw90", "--fixture-input"],
                    probe_dir=probe, timeout=25)
            finally:
                # Observer is bounded to 20 seconds. It does not own input and is
                # never mistaken for a release acknowledgement.
                try:
                    observer.wait(timeout=25)
                except subprocess.TimeoutExpired:
                    observer.kill()
                    observer.wait(timeout=3)
                    raise ValueError("read-only observer exceeded its own bound")
        need(observer.returncode == 0, "kernel observer failed")
        route = validate_stream(stream_path.read_bytes(), profile["boot_id"], profile)
        write_new(self.root / "route-observation.json", route)
        probe = self.root / "fixture-probe"
        host = validate_probe(read(probe / "manifest.json"))
        self.snapshot(profile, "kernel-after-probe.json")
        self.state("recording", "五指已釋放；等待 60 秒記錄安全封口")
        deadline = time.monotonic() + 70
        while True:
            self.stop_check()
            raw = self.command(["exec-out", "run-as", PACKAGE, "cat", "files/" + name], limit=self.pull.MAX_RAW_BYTES, raw=True)
            status = self.pull.snapshot_status(raw, name)
            if status["snapshot_complete"]:
                break
            need(status["issues"] == ["missing_ambiguous_or_nonterminal_footer"] and time.monotonic() < deadline,
                 "fixture recording did not close cleanly")
            time.sleep(1)
        after = self.pull.current_identity(call)
        need(after == identity and self.boot_id() == profile["boot_id"], "fixture PID or boot changed")
        need(raw.startswith(before_raw), "closed recording no longer has its original header prefix")
        native = self.root / name
        native.write_bytes(raw)
        retrieval = dict(schema="pas.touch-evidence-retrieval.v1", status="retrieved_closed_snapshot",
            serial=SERIAL, avd=AVD, package=PACKAGE, installed_apk_sha256=APK_SHA, source_file=name,
            retrieved_bytes=len(raw), sha256=sha(native), identity_before=identity, identity_after=after,
            association_stable=True, snapshot=status, transport=dict(returncode=0, timed_out=False, output_limited=False, readers_closed=True),
            selection_policy="new-filename-since-before-launch-and-current-PID.v1", prior_filenames=sorted(previous),
            touch_sent=False, input_latency_ns=None, game_input_lead_ns=None)
        write_new(self.root / "retrieval-report.json", retrieval)
        checked_ref(self.c["analyzer"])
        analyzer = module(self.c["analyzer"]["file"], "pas_restart_analyzer")
        analysis = analyzer.analyze_probe(probe / "manifest.json", probe / "events.jsonl", native)
        android = validate_analysis(analysis)
        write_new(self.root / "analysis.json", analysis)
        self.snapshot(profile, "kernel-final-zero.json")
        evidence = dict(schema="pas.fixture-capacity-evidence.v1", boot_id=profile["boot_id"], capacity=5,
            normal_path_valid=True, final_zero=True, fixture_apk_sha256=APK_SHA,
            analysis=ref(self.root / "analysis.json"), host_manifest=ref(probe / "manifest.json"),
            retrieval_report=ref(self.root / "retrieval-report.json"),
            artifacts=[ref(p) for p in (probe / "manifest.json", probe / "events.jsonl", native,
                self.root / "analysis.json", self.root / "retrieval-report.json", self.root / "kernel-final-zero.json",
                stream_path, self.root / "route-observation.json")], geometry=host["geometry"], native_peak_contacts=5,
            logical_to_android_id=android.get("logical_to_android_id"), native_motion_events=android.get("motion_events"),
            current_kernel_zero=True, input_latency_ns=None, game_input_lead_ns=None, live_game_qualified=False,
            scope="new cold-boot fixture DOWN/MOVE/UP, simultaneous five contacts and final zero; no game judgment or latency qualification")
        write_new(self.root / "capacity-evidence.json", evidence)
        derived = derive_panel(read(self.c["panel_config"]["file"]), ref(self.root / "kernel-profile.json"), ref(self.root / "capacity-evidence.json"))
        write_new(self.root / "launch-config.json", derived)
        # Rehash tools; record mutable emulator metadata separately, keeping the
        # sealed preboot identity rather than replacing its original hashes.
        for key in TOOL_KEYS:
            if key not in ("avd_ini", "avd_config"):
                checked_ref(self.c[key])
        current_avd = ini(self.c["avd_config"]["file"])
        need(current_avd.get("AvdId") == AVD and current_avd.get("hw.lcd.width") == "720" and
             current_avd.get("hw.lcd.height") == "1280", "AVD geometry changed during boot")
        write_new(self.root / "current-avd-metadata.json", dict(original_avd_ini=self.c["avd_ini"],
                   original_avd_config=self.c["avd_config"], current_avd_ini=ref(self.c["avd_ini"]["file"]),
                   current_avd_config=ref(self.c["avd_config"]["file"]), identity_geometry_rechecked=True))
        self.stop_check()
        write_new(self.root / "result.json", dict(schema="pas.song-test-restart-result.v1", status="fresh_fixture_verified",
            prior_panel_config=self.c["panel_config"], launch_config=ref(self.root / "launch-config.json"),
            kernel_profile=ref(self.root / "kernel-profile.json"), capacity_evidence=ref(self.root / "capacity-evidence.json"),
            boot_id=profile["boot_id"], actual_capacity=5, game_launched=False, game_selection=False, game_play=False,
            original_evidence_modified=False, input_latency_ns=None, game_input_lead_ns=None))
        self.state("ready", "新的五指與零接觸驗證完成；可以開啟測試面板", launch_config=str(self.root / "launch-config.json"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--make-config", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--configuration", type=Path)
    parser.add_argument("--configuration-sha256")
    parser.add_argument("--mode", choices=("plan", "preboot", "boot", "fixture"), default="plan")
    parser.add_argument("--session", type=Path)
    parser.add_argument("--fixture-consent", action="store_true")
    args = parser.parse_args()
    if args.make_config:
        need(args.output is not None, "--output required")
        print(json.dumps(make_config(args.make_config, args.output)))
        return 0
    need(args.configuration is not None, "--configuration required")
    config = load_config(args.configuration, args.configuration_sha256, running=args.mode in ("boot", "fixture"))
    config["_config_ref"] = ref(args.configuration)
    if args.mode == "plan":
        print(json.dumps(dict(schema="pas.song-test-restart-plan.v1", status="offline_pins_verified",
              configuration=ref(args.configuration), emulator=config["emulator"], emulator_arguments=EMULATOR_ARGS,
              panel_config=config["panel_config"], fixture_capacity=5, device_access=False, native_processes_started=0)))
        return 0
    need(args.session is not None and args.session.is_absolute() and args.session.parent.resolve() == Path(config["output_parent"]).resolve(), "session must be a new direct child of output_parent")
    worker = Bootstrap(config, args.session)
    try:
        if args.mode == "preboot":
            worker.preboot()
        elif args.mode == "boot":
            worker.boot()
        else:
            worker.fixture(args.fixture_consent)
    except BaseException as error:
        worker.state("failed", str(error), failure_type=type(error).__name__, live_game_started=False)
        raise
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(type(error).__name__ + ": " + str(error), file=sys.stderr)
        raise SystemExit(2)
