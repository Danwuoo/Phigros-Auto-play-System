"""Offline launcher contracts; SDK path is fake and every device call is mocked."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
LAUNCHER = REPO / "tools/desktop-start-runtime.ps1"
BOOTSTRAP = REPO / "tools/desktop-bootstrap.ps1"
PWSH = os.environ.get("PAS_TEST_PWSH") or shutil.which("pwsh")


@unittest.skipUnless(PWSH, "PowerShell 7.2+ required")
class DesktopStartupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pas-startup-offline-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        self.gui = self.root / "tools/sweep-panel.ps1"
        self.gui.write_text("# synthetic GUI; never executed\n", encoding="utf-8")
        self.config = self.root / "manual.json"
        self.data = dict(schema="pas.manual-song-panel-launch.v2", manual_selection=True,
                         manual_start=True, image_retention="result",
                         gui_script=self.pin(self.gui), capacity_policy="configured-ten-contact-v1")
        self.write_config()

    @staticmethod
    def pin(path):
        return dict(file=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())

    def write_config(self):
        self.config.write_text(json.dumps(self.data), encoding="utf-8")

    def invoke(self, code, *, data=None, expected=0):
        payload = self.root / "case.json"
        payload.write_text(json.dumps(data or {}), encoding="utf-8")
        driver = self.root / "driver.ps1"
        driver.write_text(
            "param($Launcher,$Repository,$Configuration,$Payload)\n"
            "$ErrorActionPreference='Stop'\n"
            "function Get-CimInstance {throw 'Real process discovery forbidden'}\n"
            "function Start-Process {throw 'Process launch forbidden'}\n"
            "$d=Get-Content -LiteralPath $Payload -Raw|ConvertFrom-Json\n" + code,
            encoding="utf-8")
        env = dict(os.environ, LOCALAPPDATA=str(self.root / "no-real-sdk"))
        result = subprocess.run([PWSH, "-NoProfile", "-NonInteractive", "-File", str(driver),
                                 "-Launcher", str(LAUNCHER), "-Repository", str(self.root),
                                 "-Configuration", str(self.config), "-Payload", str(payload)],
                                env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return json.loads(result.stdout) if expected == 0 else result.stderr

    def plan(self, backend=None):
        return self.invoke(
            "$args=@{Repository=$Repository;ManualConfiguration=$Configuration;PlanOnly=$true}\n"
            "if($d.PSObject.Properties['backend']){$args.EmulatorAudioBackend=$d.backend}\n"
            "& $Launcher @args\n", data={} if backend is None else dict(backend=backend))

    def test_default_none_plan_preserves_manual_and_cold_boot_contract(self):
        plan = self.plan()
        args = plan["emulator_arguments"]
        self.assertEqual(args[args.index("-audio") + 1], "none")
        self.assertIn("-no-snapshot-load", args)
        self.assertIn("-no-snapshot-save", args)
        self.assertEqual(args[args.index("-avd") + 1], "phigros")
        self.assertTrue(plan["audio_backend"]["no_host_speaker_output"])
        self.assertIsNone(plan["audio_backend"]["applied_backend"])
        self.assertEqual(plan["audio_backend"]["scope"], "planned_cold_start_only")
        self.assertFalse(plan["device_access"])
        self.assertFalse(plan["input_enabled"])
        self.assertFalse(plan["automatic_song_selection"])
        self.assertFalse(plan["automatic_play"])
        self.assertFalse(plan["fixture_verification_automatically_run"])
        self.assertFalse(plan["current_boot_capacity_verification_required"])
        self.assertEqual(plan["image_retention"], "result")
        self.assertEqual(plan["landscape_normalization_plan"]["rotation"], 1)
        self.assertEqual(plan["landscape_normalization_plan"]["maximum_clockwise_rotations"], 3)
        self.assertEqual(plan["landscape_normalization_plan"]["sensor_settle_delay_ms"], 500)

    def test_explicit_dsound_is_normalized_and_keeps_gui_configuration(self):
        plan = self.plan("DSOUND")
        self.assertEqual(plan["emulator_arguments"][-2:], ["-audio", "dsound"])
        self.assertFalse(plan["audio_backend"]["no_host_speaker_output"])
        self.assertEqual(plan["frontend"], "manual")
        self.assertEqual(plan["panel_script"], str(self.gui))
        self.assertEqual(plan["panel_arguments"][-2:], ["-Configuration", str(self.config)])

    def test_unknown_backend_rejected_before_any_device_work(self):
        error = self.invoke("& $Launcher -Repository $Repository -PlanOnly -EmulatorAudioBackend wasapi", expected=1)
        self.assertIn("ValidateSet", error)

    def test_bad_gui_pin_and_retention_still_reject_offline(self):
        for change in (dict(image_retention="diagnostic"), dict(gui_script=dict(file=str(self.gui), sha256="0"*64))):
            with self.subTest(change=change):
                original = dict(self.data)
                self.data.update(change); self.write_config()
                self.invoke("& $Launcher -Repository $Repository -ManualConfiguration $Configuration -PlanOnly", expected=1)
                self.data = original; self.write_config()

    def test_audio_status_does_not_claim_reused_instance_was_changed(self):
        result = self.invoke(". $Launcher -Repository $Repository -LoadCoreOnly\n"
                             "@('cold','reuse_existing')|ForEach-Object {Get-PhigrosAudioStartupInfo none $_}|ConvertTo-Json\n")
        self.assertEqual(result[0]["applied_backend"], "none")
        self.assertTrue(result[0]["no_host_speaker_output"])
        self.assertIsNone(result[1]["applied_backend"])
        self.assertIsNone(result[1]["no_host_speaker_output"])

    def test_rotation_only_changes_cold_instance_and_checks_actual_geometry(self):
        result = self.invoke(
            ". $Launcher -Repository $Repository -LoadCoreOnly\n"
            "$script:calls=[Collections.Generic.List[string]]::new()\n"
            "function Invoke-SelectedAdb {param([string[]]$Arguments,[int]$TimeoutMilliseconds)\n"
            " $s=$Arguments -join ' ';$script:calls.Add($s)\n"
            " if($s.EndsWith('emu sensor get acceleration')){return \"acceleration = 9.77631:0:.812`nOK\"}\n"
            " if($s.EndsWith('user-rotation lock 1')){return ''}\n"
            " if($s.EndsWith('user-rotation')){return 'lock 1'}\n"
            " if($s.EndsWith('dumpsys input')){return 'Viewport INTERNAL: displayId=0, orientation=1, logicalFrame=[0, 0, 1280, 720]'}\n"
            " throw 'Unexpected offline command'}\n"
            "$reuse=Initialize-PhigrosColdLandscape $false;$reuseCalls=$script:calls.Count\n"
            "$cold=Initialize-PhigrosColdLandscape $true\n"
            "@{reuse=$reuse;reuse_calls=$reuseCalls;cold=$cold;calls=@($script:calls)}|ConvertTo-Json -Depth 5\n")
        self.assertEqual(result["reuse_calls"], 0)
        self.assertIsNone(result["reuse"]["geometry_verified"])
        self.assertFalse(result["reuse"]["applied"])
        self.assertTrue(result["cold"]["geometry_verified"])
        self.assertEqual(result["cold"]["width"], 1280)
        self.assertTrue(result["cold"]["sensor_verified"])
        self.assertEqual(result["cold"]["clockwise_rotations"], 0)
        self.assertEqual(result["calls"], ["-s emulator-5554 emu sensor get acceleration",
                                           "-s emulator-5554 shell wm user-rotation lock 1",
                                           "-s emulator-5554 shell wm user-rotation",
                                           "-s emulator-5554 shell dumpsys input"])

    def test_cold_skin_rotates_only_until_positive_x_then_locks_wm(self):
        for initial_skin, expected_rotations in ((3, 0), (0, 3), (1, 2), (2, 1)):
            with self.subTest(initial_skin=initial_skin):
                result = self.invoke(
                    ". $Launcher -Repository $Repository -LoadCoreOnly\n"
                    "$script:skin=[int]$d.initial_skin;$script:calls=[Collections.Generic.List[string]]::new()\n"
                    "$script:sleeps=[Collections.Generic.List[int]]::new()\n"
                    "function Start-Sleep {param([int]$Milliseconds) $script:sleeps.Add($Milliseconds)}\n"
                    "function Invoke-SelectedAdb {param([string[]]$Arguments,[int]$TimeoutMilliseconds)\n"
                    " $s=$Arguments -join ' ';$script:calls.Add($s)\n"
                    " if($s.EndsWith('emu sensor get acceleration')){\n"
                    "  $v=@('0:9.77631:.812','-9.77631:0:.812','0:-9.77631:.812','9.77631:0:.812')[$script:skin];return \"acceleration = $v`nOK\"}\n"
                    " if($s.EndsWith('emu rotate')){$script:skin=($script:skin+1)%4;return 'OK'}\n"
                    " if($s.EndsWith('user-rotation lock 1')){return ''}\n"
                    " if($s.EndsWith('user-rotation')){return 'lock 1'}\n"
                    " if($s.EndsWith('dumpsys input')){return 'Viewport INTERNAL: displayId=0, orientation=1, logicalFrame=[0, 0, 1280, 720]'}\n"
                    " throw 'Unexpected offline command'}\n"
                    "$cold=Initialize-PhigrosColdLandscape $true\n"
                    "@{cold=$cold;calls=@($script:calls);sleeps=@($script:sleeps)}|ConvertTo-Json -Depth 6\n",
                    data=dict(initial_skin=initial_skin))
                self.assertEqual(result["cold"]["clockwise_rotations"], expected_rotations)
                self.assertEqual(result["cold"]["acceleration"]["skin_rotation"], 3)
                self.assertEqual(result["sleeps"], [500] * expected_rotations)
                self.assertEqual(sum(s.endswith("emu rotate") for s in result["calls"]), expected_rotations)
                self.assertEqual(result["calls"][-3:], ["-s emulator-5554 shell wm user-rotation lock 1",
                    "-s emulator-5554 shell wm user-rotation", "-s emulator-5554 shell dumpsys input"])

    def test_unknown_or_stale_sensor_fails_without_unbounded_rotation_or_wm_lock(self):
        for stale in (False, True):
            with self.subTest(stale=stale):
                result = self.invoke(
                    ". $Launcher -Repository $Repository -LoadCoreOnly\n"
                    "$script:calls=[Collections.Generic.List[string]]::new()\n"
                    "function Start-Sleep {param([int]$Milliseconds)}\n"
                    "function Invoke-SelectedAdb {param([string[]]$Arguments,[int]$TimeoutMilliseconds)\n"
                    " $s=$Arguments -join ' ';$script:calls.Add($s)\n"
                    " if($s.EndsWith('emu sensor get acceleration')){if($d.stale){return 'acceleration = 0:9.8:0'}else{return 'not a sensor reply'}}\n"
                    " if($s.EndsWith('emu rotate')){return 'OK'}\n"
                    " throw 'Unexpected offline command'}\n"
                    "$rejected=$false;try{$null=Initialize-PhigrosColdLandscape $true}catch{$rejected=$true}\n"
                    "@{rejected=$rejected;calls=@($script:calls)}|ConvertTo-Json\n", data=dict(stale=stale))
                self.assertTrue(result["rejected"])
                self.assertEqual(sum(s.endswith("emu rotate") for s in result["calls"]), int(stale))
                self.assertEqual(sum(s.endswith("emu sensor get acceleration") for s in result["calls"]), 4 if stale else 3)
                self.assertFalse(any("shell wm" in s for s in result["calls"]))

    def test_acceleration_parser_rejects_ambiguous_nonfinite_or_invalid_gravity(self):
        for text in ("acceleration = NaN:0:0", "acceleration = 20:0:0", "acceleration = 0:0:9.8",
                     "acceleration = 9.8:0:0\nacceleration = 9.8:0:0", "acceleration = 9.8:0:0\nKO: failure",
                     "acceleration = 9.8:0:0\nunknown status", "x" * 4097):
            with self.subTest(text=text[:80]):
                result = self.invoke(". $Launcher -Repository $Repository -LoadCoreOnly\n"
                                     "$rejected=$false;try{$null=Get-PhigrosAcceleration $d.text}catch{$rejected=$true}\n"
                                     "@{rejected=$rejected}|ConvertTo-Json", data=dict(text=text))
                self.assertTrue(result["rejected"])

    def test_surface_parser_rejects_portrait_wrong_display_and_conflicting_readback(self):
        for text in ("Viewport: displayId=0, orientation=0, logicalFrame=[0, 0, 720, 1280]",
                     "Viewport: displayId=1, orientation=1, logicalFrame=[0, 0, 1280, 720]",
                     "Viewport: displayId=0, orientation=1, logicalFrame=[0, 0, 1280, 720]\nSurfaceOrientation: 1\nSurfaceWidth: 720\nSurfaceHeight: 1280"):
            with self.subTest(text=text):
                result = self.invoke(". $Launcher -Repository $Repository -LoadCoreOnly\n"
                                     "$rejected=$false;try{$null=Get-PhigrosLogicalSurface $d.text}catch{$rejected=$true}\n"
                                     "@{rejected=$rejected}|ConvertTo-Json", data=dict(text=text))
                self.assertTrue(result["rejected"])

    def test_bootstrap_forwards_audio_option_to_new_runtime(self):
        shutil.copyfile(LAUNCHER, self.root / "tools/desktop-start-runtime.ps1")
        result = self.invoke("& $d.bootstrap -Repository $Repository -ManualConfiguration $Configuration -EmulatorAudioBackend dsound -PlanOnly",
                             data=dict(bootstrap=str(BOOTSTRAP)))
        self.assertEqual(result["emulator_arguments"][-2:], ["-audio", "dsound"])

    def test_bootstrap_does_not_force_new_option_into_old_installed_bundle(self):
        release = self.root / "local/desktop-app/releases/synthetic-old"
        (release / "tools").mkdir(parents=True)
        names = ("desktop-start-runtime.ps1", "sweep-panel.ps1", "catalog-panel-core.ps1",
                 "song-library-panel.ps1", "start-song-library.ps1")
        for name in names:
            text = "# synthetic installed member\n"
            if name == names[0]:
                text = ("param($Repository,$ManualConfiguration,$BootTimeoutSeconds,[switch]$PlanOnly,"
                        "[switch]$LoadCoreOnly,[switch]$InProcessPanel)\n"
                        "@{legacy=$true;device_access=$false}|ConvertTo-Json\n")
            (release / "tools" / name).write_text(text, encoding="utf-8")
        manifest = release / "manifest.json"
        manifest.write_text(json.dumps(dict(schema="pas.desktop-frontend-release.v1",
            runtime="tools/"+names[0], artifacts=[dict(file="tools/"+n,
                sha256=self.pin(release/"tools"/n)["sha256"], bytes=(release/"tools"/n).stat().st_size)
                for n in names])), encoding="utf-8")
        self.data.update(frontend_release=self.pin(manifest), gui_script=self.pin(release/"tools"/names[1]),
                         scripts=[self.pin(release/"tools"/name) for name in names])
        self.write_config()
        result = self.invoke("& $d.bootstrap -Repository $Repository -ManualConfiguration $Configuration -PlanOnly",
                             data=dict(bootstrap=str(BOOTSTRAP)))
        self.assertTrue(result["legacy"])
        error = self.invoke("& $d.bootstrap -Repository $Repository -ManualConfiguration $Configuration -EmulatorAudioBackend dsound -PlanOnly",
                            data=dict(bootstrap=str(BOOTSTRAP)), expected=1)
        self.assertIn("音訊選項", error)


if __name__ == "__main__":
    unittest.main()
