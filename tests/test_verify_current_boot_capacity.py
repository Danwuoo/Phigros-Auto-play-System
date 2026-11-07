"""Offline current-boot authorization/identity/geometry proof contracts."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS=Path(__file__).resolve().parents[1]/"tools"
sys.path.insert(0,str(TOOLS))
SPEC=importlib.util.spec_from_file_location("verify_current_boot_capacity",TOOLS/"verify_current_boot_capacity.py")
tool=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(tool)


def host(capacity=10,profile="landscape-r1"):
    return dict(schema="pas.touch-probe.v1",serial=tool.restart.SERIAL,avd=tool.restart.AVD,case="capacity",
        capacity_requested=capacity,contacts_in_case=capacity,fixture_package=tool.restart.PACKAGE,
        fixture_apk_sha256=tool.restart.APK_SHA,input_enabled=True,user_stop_requested=False,
        status="fixture_visual_path_passed",path_verified=True,zero_active_observed=True,
        release_rpc_known=True,events_log_complete=True,failure=None,image_retention="none",
        final_evidence=dict(complete=True,active=0,error=""),release=dict(failed=[],unknown=[]),
        geometry=dict(profile=profile,normalized_width=1280,normalized_height=720,
            touch_width=720,touch_height=1280,touch_rotation=90,**tool.PROFILES[profile]))


def native_raw(*,closed=True,change=None):
    header=dict(type="header",schema="pas.android.touch-evidence.v1",fixture="native-evidence-v3",
        start_monotonic_ns=12345,max_events=8192,max_bytes=tool.MAX_NATIVE,max_duration_ns=60_000_000_000)
    lines=[json.dumps(header)+"\n",json.dumps(dict(type="motion"))+"\n"]
    if closed:
        footer=dict(type="footer",fault=False,lifecycle_errors=0,first_unrecorded_events=0,
            active_after_last_observation=0,further_events_not_observed=True,
            release_observation="zero_active_in_observed_stream",events_written=1,
            bytes_before_footer=len("".join(lines).encode()))
        if change:footer.update(change)
        lines.append(json.dumps(footer)+"\n")
    return "".join(lines).encode()


class CurrentBootCapacityTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.archive=self.root/"archive"; (self.archive/"bin").mkdir(parents=True)
        (self.archive/"source/apps/touch_probe").mkdir(parents=True)
        self.probe=self.archive/"bin/touch_probe.exe";self.probe.write_bytes(b"offline synthetic executable")
        self.source=self.archive/"source/apps/touch_probe/main.cpp";self.source.write_text("--no-images synthetic source")
        self.dll=self.archive/"bin/dependency.dll";self.dll.write_bytes(b"offline synthetic DLL")
        self.manifest=self.archive/"manifest.json"
        self.seal()
        self.adb=self.root/"adb.exe";self.adb.write_bytes(b"never executed adb")

    def seal(self):
        tool.write_new(self.manifest,dict(schema="pas.adaptive-build-archive.v1",artifacts=[
            dict(file=str(p.relative_to(self.archive)),sha256=tool.sha(p)) for p in (self.probe,self.source,self.dll)]))

    def test_run_is_required_before_any_files_or_subprocess(self):
        target=self.root/"new-run"
        with patch.object(tool.subprocess,"Popen",side_effect=AssertionError("device call forbidden")):
            with self.assertRaisesRegex(ValueError,"--run"):
                tool.verify(self.probe,self.manifest,target,adb=self.adb)
        self.assertFalse(target.exists())

    def test_probe_source_binary_and_dll_are_bound_to_archive(self):
        result=tool.sealed_probe(self.probe,self.manifest)
        self.assertEqual(result["probe"]["sha256"],tool.sha(self.probe))
        self.assertEqual(result["probe_source"]["sha256"],tool.sha(self.source))
        self.assertEqual(result["probe_dependencies"],[tool.ref(self.dll)])
        self.dll.write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError,"frozen"):
            tool.sealed_probe(self.probe,self.manifest)

    def test_old_probe_without_no_images_is_refused_before_device_access(self):
        self.source.write_text("synthetic old source")
        self.manifest.unlink();self.seal()
        with patch.object(tool.subprocess,"Popen",side_effect=AssertionError("no device access")):
            with self.assertRaisesRegex(ValueError,"no-images"):
                tool.verify(self.probe,self.manifest,self.root/"new",adb=self.adb,run=True)

    def test_geometry_is_explicit_in_actual_probe_arguments(self):
        config=tool.sealed_probe(self.probe,self.manifest)
        for profile in tool.PROFILES:
            with self.subTest(profile=profile):
                arguments=tool.probe_arguments(config,self.root/"probe",10,profile)
                self.assertEqual(arguments[5:9],["capacity","10","1280","720"])
                self.assertEqual(arguments[-3:],[profile,"--fixture-input","--no-images"])
                inspection=tool.probe_arguments(config,self.root/"inspect",10,profile,inspect=True)
                self.assertEqual(inspection[-2:],["--inspect-only","--no-images"])

    def test_both_actual_geometries_validate_without_relabeling(self):
        for profile in tool.PROFILES:
            value=host(profile=profile);before=copy.deepcopy(value)
            self.assertIs(tool.validate_host(value,10,profile),value)
            self.assertEqual(value,before)
            other=next(p for p in tool.PROFILES if p!=profile)
            with self.assertRaisesRegex(ValueError,"geometry"):
                tool.validate_host(value,10,other)

    def test_ten_contact_host_cannot_use_five_contact_or_uncertain_release(self):
        with self.assertRaises(ValueError):tool.validate_host(host(5),10,"landscape-r1")
        for key,value in (("path_verified",False),("zero_active_observed",False),("image_retention","fixture_diagnostics"),
                          ("events_log_complete",False),("user_stop_requested",True)):
            candidate=host();candidate[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):tool.validate_host(candidate,10,"landscape-r1")
        candidate=host();candidate["release"]["unknown"]=[9]
        with self.assertRaises(ValueError):tool.validate_host(candidate,10,"landscape-r1")

    def test_eleven_and_noninteger_capacity_are_rejected(self):
        for capacity in (11,0,True,10.0):
            with self.subTest(capacity=capacity),self.assertRaises(ValueError):
                tool.probe_arguments({},self.root/"unused",capacity,"landscape-r1")

    def test_native_filename_header_and_terminal_footer_are_bound(self):
        result=tool.native_snapshot(native_raw(),"touch-evidence-17-12345.jsonl")
        self.assertTrue(result["snapshot_complete"])
        for name in ("touch-evidence-17-99999.jsonl","../touch-evidence-17-12345.jsonl"):
            with self.subTest(name=name),self.assertRaises(ValueError):tool.native_snapshot(native_raw(),name)
        partial=tool.native_snapshot(native_raw()[:-1],"touch-evidence-17-12345.jsonl")
        self.assertFalse(partial["snapshot_complete"])
        self.assertFalse(tool.native_snapshot(native_raw(closed=False),"touch-evidence-17-12345.jsonl")["snapshot_complete"])

    def test_native_footer_does_not_bless_lost_events_or_active_contacts(self):
        for key,value in (("fault",True),("active_after_last_observation",1),("lifecycle_errors",1),
                          ("first_unrecorded_events",1),("events_written",2),("bytes_before_footer",0)):
            with self.subTest(key=key),self.assertRaises(ValueError):
                tool.native_snapshot(native_raw(change={key:value}),"touch-evidence-17-12345.jsonl")

    def test_existing_output_is_preserved_and_no_worker_starts(self):
        output=self.root/"existing";output.mkdir();(output/"keep").write_bytes(b"preserve")
        with patch.object(tool.CurrentBootCapacity,"execute",side_effect=AssertionError("must not start")):
            with self.assertRaisesRegex(ValueError,"new directory"):
                tool.verify(self.probe,self.manifest,output,adb=self.adb,run=True)
        self.assertEqual((output/"keep").read_bytes(),b"preserve")

    def test_explicit_run_preserves_ten_and_profile_in_sealed_invocation(self):
        output=self.root/"new"
        observed=[]
        def fake_execute(worker):
            observed.append(worker.c)
            return dict(status="offline_fake_only",gameplay_qualified=False)
        with patch.object(tool.CurrentBootCapacity,"execute",fake_execute),\
             patch.object(tool.subprocess,"Popen",side_effect=AssertionError("offline test forbids subprocess")):
            result=tool.verify(self.probe,self.manifest,output,10,"sensor-portrait-r0-ccw90",self.adb,run=True)
        invocation=tool.read(output/"invocation.json")
        self.assertEqual(invocation["capacity"],10)
        self.assertEqual(observed[0]["geometry_profile"],"sensor-portrait-r0-ccw90")
        self.assertFalse(invocation["emulator_restarted"])
        self.assertEqual(invocation["image_retention"],"none")
        self.assertFalse(result["gameplay_qualified"])
        self.assertFalse((output/"capacity-evidence.json").exists())


if __name__=="__main__":unittest.main()
