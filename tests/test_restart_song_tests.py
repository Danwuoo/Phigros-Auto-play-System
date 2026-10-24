"""Offline bootstrap proof/ownership tests: no real subprocess or device calls."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("restart_song_tests", Path(__file__).resolve().parents[1] / "tools/restart_song_tests.py")
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)
BOOT = "12345678-1234-1234-1234-123456789abc"


def snapshot():
    return dict(schema="pas.evdev-slot-snapshot.v1", boot_id=BOOT, path="/dev/input/event2",
                name="virtio_input_multi_touch_1", rdev=3394, slot_count=11, error_errno=0,
                tracking_ids=[-1]*11, active_count=0, input_enabled=False, grab_enabled=False,
                game_callback_verified=False, android_monotonic_before_ns=100, android_monotonic_after_ns=200)


def stream():
    row = snapshot()
    identity = {k: row[k] for k in ("path", "name", "rdev", "slot_count")}
    rows = [dict(type="header", schema="pas.evdev-slot-stream.v1", boot_id=BOOT, input_enabled=False,
                 grab_enabled=False, devices=[dict(**identity, initialization_errno=0)])]
    for sequence, count in enumerate((0, 5, 5, 0), 1):
        rows.append(dict(type="sample", sequence=sequence, boot_id=BOOT, **identity, error_errno=0,
                         android_monotonic_before_ns=sequence*100, android_monotonic_after_ns=sequence*100+50,
                         active_count=count, tracking_ids=list(range(count))+[-1]*(11-count)))
    rows.append(dict(type="footer", fault=False, partial_final_round=False, samples_written=4))
    return rows


def wire(rows):
    return ("\n".join(json.dumps(row) for row in rows)+"\n").encode()


def probe():
    return dict(schema="pas.touch-probe.v1", serial=tool.SERIAL, avd=tool.AVD, case="capacity", capacity_requested=5,
                fixture_package=tool.PACKAGE,fixture_apk_sha256=tool.APK_SHA,input_enabled=True,user_stop_requested=False,
                contacts_in_case=5,width=1280,height=720,
                status="fixture_visual_path_passed", path_verified=True, zero_active_observed=True,
                release_rpc_known=True, events_log_complete=True, failure="", final_evidence=dict(active=0, complete=True,error=""),
                release=dict(failed=[], unknown=[]), geometry=dict(normalize_ccw90=True, normalized_width=1280,
                normalized_height=720, raw_width=720,raw_height=1280,raw_rotation=0,touch_width=720,touch_height=1280,
                touch_rotation=90,profile="sensor-portrait-r0-ccw90"))


class BootstrapTests(unittest.TestCase):
    def test_snapshot_positive_is_readonly_zero(self):
        self.assertEqual(tool.validate_snapshot(snapshot(), BOOT)["active_count"], 0)

    def test_snapshot_cannot_relabel_old_boot(self):
        with self.assertRaises(ValueError):tool.validate_snapshot(snapshot(), "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa")

    def test_snapshot_rejects_active_tracking_even_if_count_claims_zero(self):
        for field, value in (("active_count", 1), ("tracking_ids", [9]+[-1]*10)):
            row=snapshot();row[field]=value
            with self.subTest(field=field), self.assertRaises(ValueError):tool.validate_snapshot(row,BOOT)

    def test_snapshot_rejects_readwrite_or_unqualified_capacity(self):
        for field,value in (("input_enabled",True),("grab_enabled",True),("game_callback_verified",True),
                            ("slot_count",4),("slot_count",65),("slot_count",True),("rdev",True),("error_errno",13),
                            ("path","/dev/input/../event2"),("name","other")):
            row=snapshot();row[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):tool.validate_snapshot(row,BOOT)

    def test_snapshot_route_change_is_not_old_profile(self):
        changed=snapshot();changed["rdev"]+=1
        with self.assertRaises(ValueError):tool.validate_snapshot(changed,BOOT,snapshot())

    def test_snapshot_rejects_bool_float_ids_and_clock_reversal(self):
        for value in (True,1.0,-2):
            row=snapshot();row["tracking_ids"][0]=value
            with self.subTest(value=value),self.assertRaises(ValueError):tool.validate_snapshot(row,BOOT)
        row=snapshot();row["android_monotonic_after_ns"]=1
        with self.assertRaises(ValueError):tool.validate_snapshot(row,BOOT)

    def test_stream_requires_actual_zero_five_zero(self):
        result=tool.validate_stream(wire(stream()),BOOT,snapshot())
        self.assertEqual(result["peak_contacts"],5)
        self.assertTrue(result["baseline_zero"] and result["final_zero"])

    def test_stream_rejects_missing_peak_even_if_footer_clean(self):
        rows=stream()
        for row in rows[1:-1]:row["active_count"]=0;row["tracking_ids"]=[-1]*11
        with self.assertRaises(ValueError):tool.validate_stream(wire(rows),BOOT,snapshot())

    def test_stream_rejects_unreleased_end_and_six_contacts(self):
        for index,count in ((-2,1),(2,6)):
            rows=stream();rows[index]["active_count"]=count;rows[index]["tracking_ids"]=list(range(count))+[-1]*(11-count)
            with self.subTest(index=index),self.assertRaises(ValueError):tool.validate_stream(wire(rows),BOOT,snapshot())

    def test_stream_rejects_other_boot_route_and_gap(self):
        for field,value in (("boot_id","aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"),("rdev",999),("sequence",44),
                            ("android_monotonic_before_ns",0),("error_errno",5)):
            rows=stream();rows[2][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):tool.validate_stream(wire(rows),BOOT,snapshot())

    def test_stream_rejects_truncation_footer_fault_or_count(self):
        raw=wire(stream())
        for candidate in (raw[:-1],wire(stream()[:-1])):
            with self.assertRaises(ValueError):tool.validate_stream(candidate,BOOT,snapshot())
        for field,value in (("fault",True),("partial_final_round",True),("samples_written",3)):
            rows=stream();rows[-1][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):tool.validate_stream(wire(rows),BOOT,snapshot())

    def test_recording_selection_handles_old_pid_reuse_without_deletion(self):
        old="touch-evidence-17-100.jsonl";new="touch-evidence-17-200.jsonl"
        self.assertEqual(tool.select_new_recording(old+"\n"+new,{old},17),new)

    def test_recording_selection_rejects_none_ambiguous_and_other_pid(self):
        for listing in ("touch-evidence-18-100.jsonl","touch-evidence-17-100.jsonl\ntouch-evidence-17-200.jsonl",
                        "touch-evidence-17-100.jsonl\ntouch-evidence-17-100.jsonl"):
            with self.subTest(listing=listing),self.assertRaises(ValueError):tool.select_new_recording(listing,set(),17)

    def test_probe_requires_complete_native_release_and_geometry(self):
        self.assertTrue(tool.validate_probe(probe())["zero_active_observed"])
        for field,value in (("path_verified",False),("zero_active_observed",False),("release_rpc_known",False),
                            ("events_log_complete",False),("failure","some_failure"),("capacity_requested",4)):
            row=probe();row[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):tool.validate_probe(row)
        row=probe();row["geometry"]["touch_rotation"]=0
        with self.assertRaises(ValueError):tool.validate_probe(row)

    def test_probe_rpc_ack_is_not_zero(self):
        row=probe();row["final_evidence"]["active"]=1
        with self.assertRaises(ValueError):tool.validate_probe(row)

    def test_analysis_requires_five_native_contacts_and_all_independent_path_facts(self):
        value=dict(schema="pas.touch-probe-analysis.v1",status="passed_observed_fixture_path",failures=[],incomplete_reasons=[],
                   android=dict(peak_contacts=5,stream_complete=True,expected_path_verified=True,pointer_lifecycle_valid=True,final_zero_observed=True))
        self.assertEqual(tool.validate_analysis(value)["peak_contacts"],5)
        for field in ("stream_complete","expected_path_verified","pointer_lifecycle_valid","final_zero_observed"):
            bad=copy.deepcopy(value);bad["android"][field]=False
            with self.subTest(field=field),self.assertRaises(ValueError):tool.validate_analysis(bad)
        bad=copy.deepcopy(value);bad["android"]["peak_contacts"]=4
        with self.assertRaises(ValueError):tool.validate_analysis(bad)

    def test_analysis_partial_or_failure_cannot_become_capacity_proof(self):
        for field,value in (("status","incomplete"),("failures",["path_mismatch"]),("incomplete_reasons",["missing_native"])):
            bad=dict(schema="pas.touch-probe-analysis.v1",status="passed_observed_fixture_path",failures=[],incomplete_reasons=[]);bad[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):tool.validate_analysis(bad)
        row=probe();row["release"]["unknown"]=[0]
        with self.assertRaises(ValueError):tool.validate_probe(row)

    def test_derived_panel_changes_only_expiring_refs(self):
        original=dict(schema="pas.manual-song-panel-launch.v1",manual_selection=True,manual_start=True,
                      kernel_profile={"old":1},capacity_evidence={"old":2},plan={"sha256":"unchanged"},library=[1,2])
        before=copy.deepcopy(original)
        derived=tool.derive_panel(original,{"fresh":1},{"fresh":2})
        self.assertEqual(original,before)
        self.assertEqual(derived["plan"],original["plan"])
        self.assertEqual(derived["library"],original["library"])
        self.assertEqual({k for k in original if derived[k]!=original[k]},{"kernel_profile","capacity_evidence"})

    def test_derived_panel_cannot_enable_autostart(self):
        for field in ("manual_selection","manual_start"):
            row=dict(schema="pas.manual-song-panel-launch.v1",manual_selection=True,manual_start=True);row[field]=False
            with self.subTest(field=field),self.assertRaises(ValueError):tool.derive_panel(row,{}, {})

    def test_strict_json_rejects_duplicates_and_nonfinite(self):
        for raw in ('{"a":1,"a":2}','{"a":NaN}'):
            with self.assertRaises(ValueError):tool.decode(raw)

    def test_pin_tampering_and_output_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/"p.json";tool.write_new(p,{"first":True});r=tool.ref(p)
            self.assertEqual(tool.checked_ref(r),p)
            with self.assertRaises(FileExistsError):tool.write_new(p,{"second":True})
            p.write_text("changed")
            with self.assertRaises(ValueError):tool.checked_ref(r)

    def test_consent_guard_precedes_every_subprocess(self):
        worker=object.__new__(tool.Bootstrap)
        with patch.object(tool.subprocess,"Popen",side_effect=AssertionError("subprocess not authorized")):
            with self.assertRaisesRegex(ValueError,"consent"):worker.fixture(False)

    def test_stop_is_sticky_and_does_not_create_probe_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp)
            (worker.root/"STOP").touch()
            with self.assertRaisesRegex(ValueError,"user_stop"):worker.stop_check()
            self.assertFalse((worker.root/"fixture-probe").exists())

    def test_input_owner_stop_is_forwarded_and_never_killed(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp)
            probe_dir=worker.root/"fixture-probe"
            class FakeOwner:
                pid=123;returncode=2
                def __init__(self,*args,**kwargs):self.polls=0
                def poll(self):
                    self.polls+=1
                    if self.polls==1:
                        probe_dir.mkdir();(worker.root/"STOP").touch();return None
                    return None if self.polls<3 else self.returncode
                def kill(self):raise AssertionError("must never kill input owner")
            with patch.object(tool.subprocess,"Popen",FakeOwner),patch.object(tool.time,"sleep",lambda _:None):
                with self.assertRaises(ValueError):worker.owned("fake-input",["not-executed"],probe_dir=probe_dir)
            self.assertTrue((probe_dir/"STOP").exists())

    def test_unexpected_clock_fault_after_start_still_drains_owner(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp);probe_dir=worker.root/"fixture-probe";probe_dir.mkdir()
            class FakeOwner:
                pid=123;returncode=0;polls=0
                def poll(self):self.polls+=1;return None if self.polls<4 else 0
                def kill(self):raise AssertionError("must never kill owner")
            owner=FakeOwner()
            with patch.object(tool.subprocess,"Popen",lambda *a,**k:owner),patch.object(tool.time,"monotonic",side_effect=OSError("clock fault")),patch.object(tool.time,"sleep",lambda _:None):
                with self.assertRaisesRegex(OSError,"clock fault"):worker.owned("fake-clock",["never-executed"],probe_dir=probe_dir)
            self.assertGreaterEqual(owner.polls,4)
            self.assertTrue((probe_dir/"STOP").exists())
            self.assertFalse(json.loads((worker.root/"fake-clock.process.json").read_text())["forced_termination"])

    def test_post_popen_metadata_failure_retains_owner_until_exit(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp);probe_dir=worker.root/"fixture-probe"
            class FakeOwner:
                pid=123;returncode=0;polls=0
                def __init__(self,*args,**kwargs):pass
                def poll(self):
                    self.polls+=1
                    if self.polls==2:probe_dir.mkdir()
                    return None if self.polls<5 else self.returncode
                def kill(self):raise AssertionError("must never kill input owner")
            owner=FakeOwner()
            with patch.object(tool.subprocess,"Popen",lambda *args,**kwargs:owner),patch.object(tool,"write_new",side_effect=OSError("disk full")),patch.object(tool.time,"sleep",lambda _:None):
                with self.assertRaisesRegex(OSError,"disk full"):worker.owned("fake-input",["never-executed"],probe_dir=probe_dir)
            self.assertGreaterEqual(owner.polls,5)
            self.assertTrue((probe_dir/"STOP").exists())

    def test_post_start_progress_and_stop_write_failures_are_retried(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp);probe_dir=worker.root/"fixture-probe"
            worker.state=lambda *args,**kwargs:(_ for _ in ()).throw(OSError("progress disk failure"))
            class FakeOwner:
                pid=123;returncode=2;polls=0
                def __init__(self,*args,**kwargs):pass
                def poll(self):
                    self.polls+=1
                    if self.polls==1:(worker.root/"STOP").touch()
                    if self.polls==3:probe_dir.mkdir()
                    return None if self.polls<8 else self.returncode
                def kill(self):raise AssertionError("must never kill input owner")
            owner=FakeOwner();original_touch=Path.touch;attempts=[]
            def touch(path,*args,**kwargs):
                if path==probe_dir/"STOP":
                    attempts.append(path)
                    if len(attempts)==1:raise OSError("first STOP write failed")
                return original_touch(path,*args,**kwargs)
            with patch.object(tool.subprocess,"Popen",lambda *args,**kwargs:owner),patch.object(Path,"touch",touch),patch.object(tool.time,"sleep",lambda _:None):
                with self.assertRaises(ValueError):worker.owned("fake-input",["never-executed"],probe_dir=probe_dir)
            self.assertGreaterEqual(owner.polls,8)
            self.assertGreaterEqual(len(attempts),2)
            self.assertTrue((probe_dir/"STOP").exists())

    def test_preboot_refuses_online_target_without_process_launch(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp)
            worker.command=lambda *args,**kwargs:"device"
            with patch.object(tool.subprocess,"Popen",side_effect=AssertionError("no new process")):
                with self.assertRaisesRegex(ValueError,"already online"):worker.preboot()
            self.assertFalse((worker.root/"preboot.json").exists())

    def test_readonly_command_failure_is_bounded_and_preserves_reply(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp);worker.counter=0;worker.c={"adb":{"file":"fake-adb"}}
            worker.runner=lambda *args:types.SimpleNamespace(stdout=b"partial",returncode=1,timed_out=True,output_limited=False,readers_closed=True)
            with self.assertRaisesRegex(ValueError,"device setup/read failed"):worker.command(["shell","cat","/read-only"])
            self.assertTrue((worker.root/"commands.jsonl").exists())
            self.assertEqual(len(list(worker.root.glob("*-adb.bin"))),1)

    def test_fake_boot_uses_fresh_identity_route_and_actual_zero(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp)
            worker.c={k:{"file":"not-executed-"+k,"sha256":"a"*64} for k in ("snapshot_helper","stream_helper","fixture_apk")}
            tool.write_new(worker.root/"preboot.json",{"prior_boot_id":"aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"})
            calls=[]
            def command(tail,**kwargs):
                calls.append(tail)
                if tail==["shell","getprop","sys.boot_completed"]:return "1"
                if tail==["emu","avd","name"]:return "phigros\nOK"
                if tail==["shell","cat","/proc/sys/kernel/random/boot_id"]:return BOOT
                if tail==["shell","getevent","-lp"]:
                    return 'add device 4: /dev/input/event2\n name: "virtio_input_multi_touch_1"\n ABS_MT_SLOT ABS_MT_TRACKING_ID ABS_MT_POSITION_X ABS_MT_POSITION_Y'
                if tail[:2]==["shell","sha256sum"]:
                    remote=tail[2];return (tool.APK_SHA if remote.endswith("base.apk") else "a"*64)+"  "+remote
                if tail[:2]==["shell",tool.REMOTE_SNAP]:return wire([snapshot()])
                if tail[:3]==["shell","pm","path"]:return "package:/data/app/fake/base.apk"
                if tail[0]=="push" or tail[:2]==["shell","chmod"]:return ""
                raise AssertionError(tail)
            worker.command=command
            with patch.object(tool.subprocess,"Popen",side_effect=AssertionError("fake boot must not execute subprocess")):
                worker.boot()
            profile=tool.read(worker.root/"kernel-profile.json")
            self.assertEqual(profile["boot_id"],BOOT)
            self.assertEqual(profile["path"],"/dev/input/event2")
            self.assertEqual(profile["rdev"],3394)
            self.assertEqual(tool.read(worker.root/"progress.json")["stage"],"fixture_consent_required")
            self.assertFalse(any("am" in call or "input" in call for call in calls))
            self.assertFalse((worker.root/"capacity-evidence.json").exists())

    def test_fake_boot_refuses_old_uuid_before_any_helper_or_fixture(self):
        with tempfile.TemporaryDirectory() as temp:
            worker=object.__new__(tool.Bootstrap);worker.root=Path(temp)
            tool.write_new(worker.root/"preboot.json",{"prior_boot_id":BOOT})
            calls=[]
            def command(tail,**kwargs):
                calls.append(tail)
                return "1" if tail[-1]=="sys.boot_completed" else "phigros\nOK" if tail[:2]==["emu","avd"] else BOOT
            worker.command=command
            with self.assertRaisesRegex(ValueError,"reused"):worker.boot()
            self.assertFalse(any(call[0]=="push" or "am" in call for call in calls))
            self.assertFalse((worker.root/"kernel-profile.json").exists())


if __name__=="__main__":unittest.main()
