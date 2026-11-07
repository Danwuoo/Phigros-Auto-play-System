"""Headless full-panel identity/filter contracts. No device or native process."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

PANEL=Path(__file__).resolve().parents[1]/'tools/sweep-panel.ps1'
PWSH=os.environ.get('PAS_TEST_PWSH') or shutil.which('pwsh')

@unittest.skipUnless(PWSH,'PowerShell required')
class CatalogPanelTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.profile=dict(schema='pas.adaptive-song-profile.v1',song_id='synthetic.0',difficulty='AT',chart_sha256='a'*64,
                          expected_plan_sha256='b'*64,expected_contacts=2001,expected_steps=4002,expected_peak=1,
                          hud_policy='shared-difficulty-subpixel-prefix-score-v4')
        self.job=dict(library_id='v/synthetic.0/AT/hash',track_id='synthetic.0',display_name='Literal [*]',difficulty='AT',
                      variant='0',rounds=1,default_selected=False,ready_for_manual_preflight=True,blockers=[],
                      chart=dict(sha256='a'*64),profile=dict(file=str(self.root/'profile.json')),
                      notes=2001,events=4002,peak=1,compiled_fingerprint='b'*64)
        self.chart=dict(library_id=self.job['library_id'],track_id='synthetic.0',difficulty='AT',chart=dict(sha256='a'*64))

    def invoke(self,code):
        (self.root/'profile.json').write_text(json.dumps(self.profile),encoding='utf-8')
        (self.root/'data.json').write_text(json.dumps(dict(job=self.job,chart=self.chart)),encoding='utf-8')
        driver=self.root/'driver.ps1'
        driver.write_text("param($Panel,$Data)\n$ErrorActionPreference='Stop'\n. $Panel -LoadCoreOnly\n$d=Get-Content $Data -Raw|ConvertFrom-Json\n"+code,encoding='utf-8')
        return subprocess.run([PWSH,'-NoProfile','-NonInteractive','-File',str(driver),str(PANEL),str(self.root/'data.json')],capture_output=True,text=True,encoding='utf-8',timeout=40)

    def test_more_than2000_and_explicit_at_profile_are_accepted(self):
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart')
        self.assertEqual(r.returncode,0,r.stderr)

    def test_both_actual_archive_consumers_accept4096_reject4097_and_keep_path_and_hash_checks(self):
        r=self.invoke(r'''
$tools=Split-Path -Parent $Panel
foreach($name in @('catalog-panel-core.ps1','sweep-panel.ps1')){
    $tokens=$null;$parseErrors=$null
    $tree=[Management.Automation.Language.Parser]::ParseFile((Join-Path $tools $name),[ref]$tokens,[ref]$parseErrors)
    if($parseErrors.Count){throw 'Panel source did not parse'}
    $guards=@($tree.FindAll({param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and $node.Extent.Text -match '^if\s*\(\$build\.schema\b'
    },$true))
    if($guards.Count -ne 1){throw 'Expected the exact production archive-count/schema guard'}
    $guard=[scriptblock]::Create($guards[0].Extent.Text)
    foreach($artifactCount in @(2048,2049,4096,4097)){
        $build=[pscustomobject]@{schema='pas.adaptive-build-archive.v1';artifacts=[object[]]::new($artifactCount)}
        $rejected=$false;try{& $guard}catch{$rejected=$true}
        if($rejected -ne ($artifactCount -gt 4096)){throw "Incorrect artifact boundary: $name / $artifactCount"}
    }
    $build=[pscustomobject]@{schema='invalid.archive.schema';artifacts=@()}
    $rejected=$false;try{& $guard}catch{$rejected=$true};if(-not $rejected){throw 'Schema guard was lost'}
    $loops=@($tree.FindAll({param($node)
        $node -is [Management.Automation.Language.ForEachStatementAst] -and $node.Variable.VariablePath.UserPath -eq 'artifact' -and
        $node.Condition.Extent.Text -eq '$build.artifacts'
    },$true))
    if($loops.Count -ne 1){throw 'Expected the exact production archive path/hash loop'}
    $loop=[scriptblock]::Create($loops[0].Extent.Text)
    $archive=Join-Path (Split-Path -Parent $Data) $name
    $bin=Join-Path $archive 'bin';$null=New-Item -ItemType Directory -Path $bin
    $exe=Join-Path $bin 'chart_experiment.exe';[IO.File]::WriteAllText($exe,'SYNTHETIC FILE, NEVER EXECUTE')
    $digest=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    foreach($case in @('valid','path-escape','wrong-hash')){
        $pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
        $declared=@{};$binPrefix=$bin+[IO.Path]::DirectorySeparatorChar
        $relative=if($case -eq 'path-escape'){'../outside.exe'}else{'bin/chart_experiment.exe'}
        $expected=if($case -eq 'wrong-hash'){'0'*64}else{$digest}
        $build=[pscustomobject]@{artifacts=@([pscustomobject]@{file=$relative;sha256=$expected})}
        $rejected=$false;try{& $loop;Test-PanelPins ([pscustomobject]@{Pins=$pins})}catch{$rejected=$true}
        if($rejected -ne ($case -ne 'valid')){throw "Archive path/hash guard drift: $name / $case"}
    }
}
''')
        self.assertEqual(r.returncode,0,r.stderr)

    def test_both_actual_total_pin_guards_accept8192_reject8193(self):
        r=self.invoke(r'''
$tools=Split-Path -Parent $Panel
foreach($name in @('catalog-panel-core.ps1','sweep-panel.ps1')){
    $tokens=$null;$parseErrors=$null
    $tree=[Management.Automation.Language.Parser]::ParseFile((Join-Path $tools $name),[ref]$tokens,[ref]$parseErrors)
    if($parseErrors.Count){throw 'Panel source did not parse'}
    $guards=@($tree.FindAll({param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and $node.Extent.Text -match '^if\s*\(\$pins\.Count\s+-gt\s+'
    },$true))
    if($guards.Count -ne 1){throw 'Expected the exact production total-pin guard'}
    $guard=[scriptblock]::Create($guards[0].Extent.Text)
    $configurationPins=@($tree.FindAll({param($node)
        $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Add-PanelPin' -and
        $node.Extent.Text.Contains('Get-FileHash -LiteralPath $Path')
    },$true))
    if($configurationPins.Count -ne 1 -or $configurationPins[0].Extent.StartOffset -gt $guards[0].Extent.StartOffset){
        throw 'The total-pin budget must include the final configuration pin'
    }
    foreach($pinCount in @(4096,8192,8193)){
        $pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
        for($number=0;$number -lt $pinCount;$number++){$pins.Add(('synthetic-'+$number),('a'*64))}
        $rejected=$false;try{& $guard}catch{$rejected=$true}
        if($rejected -ne ($pinCount -gt 8192)){throw "Incorrect total-pin boundary: $name / $pinCount"}
    }
}
''')
        self.assertEqual(r.returncode,0,r.stderr)

    def test_in_policy_cannot_be_relabelled_as_at(self):
        self.profile['hud_policy']='shared-in-subpixel-prefix-score-v3'
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_ten_pointer_profile_is_accepted_but_not_understated_or_unbounded_capacity(self):
        for capacity in range(6,11):
            self.profile.update(contact_capacity=capacity,expected_peak=capacity)
            self.job.update(contact_capacity=capacity,peak=capacity)
            r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart')
            self.assertEqual(r.returncode,0,r.stderr)
        self.profile['contact_capacity']=9
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.profile.update(contact_capacity=11,expected_peak=11)
        self.job.update(contact_capacity=11,peak=11)
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_capacity_blocker_does_not_claim_fixed_five_pointer_device_limit(self):
        self.job['blockers']=['capacity_exceeded']
        r=self.invoke("Get-PanelBlockerText $d.job")
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('10',r.stdout)
        self.assertNotIn('5指',r.stdout)

    def test_changed_chart_identity_is_rejected(self):
        self.chart['chart']['sha256']='c'*64
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_skipped_chart_needs_reason_and_cannot_be_preselected(self):
        self.job.update(ready_for_manual_preflight=False,blockers=['unsupported'],default_selected=False)
        self.assertEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.job['default_selected']=True
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_literal_filter_preserves_difficulty_variant_and_readiness(self):
        r=self.invoke("$rows=@(Select-PanelCatalogJobs @($d.job) '[*]' 'AT' $true);if($rows.Count -ne 1){throw 'literal mismatch'};if(@(Select-PanelCatalogJobs @($d.job) 'missing' 'AT').Count){throw 'false match'};if(@(Select-PanelCatalogJobs @($d.job) '' 'IN').Count){throw 'difficulty mixed'}")
        self.assertEqual(r.returncode,0,r.stderr)

    def test_configured_ten_mode_does_not_read_or_pin_historical_fixture_evidence(self):
        r=self.invoke("""
$candidateConfig=[pscustomobject]@{capacity_policy='configured-ten-contact-v1';contact_capacity=10;capacity_evidence=[pscustomobject]@{file='C:\\deleted-history\\proof.json';sha256=('0'*64)}}
function Read-PanelJson {throw 'Historical proof must not be read'}
$pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
Add-PanelCapacityEvidencePins $pins $candidateConfig
if($pins.Count){throw 'Historical proof entered READY dependency pins'}
""")
        self.assertEqual(r.returncode,0,r.stderr)

    def test_configured_ten_arguments_need_no_capacity_proof_for_check_or_run(self):
        r=self.invoke("""
$context=[pscustomobject]@{Configuration=[pscustomobject]@{capacity_policy='configured-ten-contact-v1';contact_capacity=10;image_retention='result';kernel_profile=[pscustomobject]@{file='kernel.json'}}}
$job=[pscustomobject]@{chart=[pscustomobject]@{file='chart.json'};state_calibration=[pscustomobject]@{file='state.json'};profile=[pscustomobject]@{file='profile.json'};hud_reference=[pscustomobject]@{file='hud.rgb'}}
foreach($check in @($false,$true)){
    $arguments=New-PanelArguments $context $job 'new-run' -CheckOnly:$check
    if($arguments[13] -cne '--capacity-policy' -or $arguments[14] -cne 'configured-ten-contact-v1' -or
        $arguments -contains '--capacity-evidence' -or $arguments[-2] -cne '--image-retention' -or $arguments[-1] -cne 'result'){
        throw 'Configured policy or result-only suffix lost'
    }
}
""")
        self.assertEqual(r.returncode,0,r.stderr)

    def test_unknown_capacity_policy_rejected_and_old_configs_keep_explicit_legacy_mode(self):
        r=self.invoke("""
if((Get-PanelCapacityPolicy ([pscustomobject]@{})) -cne 'current-boot-fixture-v1'){throw 'Legacy mode changed silently'}
$rejected=$false
try{$null=Get-PanelCapacityPolicy ([pscustomobject]@{capacity_policy='unknown-policy'})}catch{$rejected=$true}
if(-not $rejected){throw 'Unknown capacity policy accepted'}
""")
        self.assertEqual(r.returncode,0,r.stderr)

    def test_configured_preflight_cannot_accept_engine_using_legacy_policy(self):
        r=self.invoke("""
$job=[pscustomobject]@{track_id='synthetic';difficulty='IN';chart=[pscustomobject]@{sha256='chart'};compiled_fingerprint='plan';state_calibration=[pscustomobject]@{sha256='state'};hud_reference=[pscustomobject]@{sha256='hud'}}
$context=[pscustomobject]@{Executable='fake.exe';Pins=@{'fake.exe'='binary'};Configuration=[pscustomobject]@{capacity_policy='configured-ten-contact-v1';image_retention='result';kernel_profile=[pscustomobject]@{sha256='kernel'}}}
$response=[ordered]@{status='offline_preflight_passed';device_discovery=$false;input_enabled=$false;song_id='synthetic';difficulty='IN';chart_sha256='chart';compiled_sha256='plan';executable_sha256='binary';screen_calibration_sha256='state';HUD_reference_sha256='hud';kernel_profile_sha256='kernel';image_retention='result';capacity_policy='current-boot-fixture-v1'}
$path=Join-Path (Split-Path -Parent $Data) 'preflight.json'
$response|ConvertTo-Json|Set-Content -LiteralPath $path -Encoding utf8
$rejected=$false
try{Assert-PanelPreflight $path $job $context}catch{$rejected=$true}
if(-not $rejected){throw 'Legacy engine preflight accepted under configured policy'}
$response.capacity_policy='configured-ten-contact-v1'
$response|ConvertTo-Json|Set-Content -LiteralPath $path -Encoding utf8
Assert-PanelPreflight $path $job $context
""")
        self.assertEqual(r.returncode,0,r.stderr)

    def test_large_chart_needs_explicit_profile_budget_and_matching_display_metadata(self):
        self.profile.update(expected_steps=27255,runtime_step_budget=65536,
            runtime_duration_limit_ns=230_000_000_000,last_up_song_ns=199_761_900_000)
        self.job.update(events=27255,runtime_budget=dict(runtime_step_budget=65536,
            runtime_duration_limit_ns=230_000_000_000,last_up_song_ns=199_761_900_000,
            expected_contacts=2001,expected_steps=27255,expected_peak=1))
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart; Get-PanelBudgetText $d.job')
        self.assertEqual(r.returncode,0,r.stderr);self.assertIn('27255/65536',r.stdout)
        self.job['runtime_budget']['runtime_step_budget']=32768
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.job['runtime_budget']['runtime_step_budget']=65536
        self.profile['runtime_step_budget']=65537
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_legacy_budget_is_not_silently_raised_and_tail_cannot_escape_explicit_limit(self):
        self.profile['expected_steps']=16001;self.job['events']=16001
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.profile.update(runtime_step_budget=65536,runtime_duration_limit_ns=230_000_000_000,
            last_up_song_ns=230_000_000_000)
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_installed_variant_keeps_distinct_public_identity_and_clear_source_text(self):
        public=dict(file=str(self.root/'public.json'),sha256='c'*64)
        self.chart.update(upstream_library_id='public/original/AT/id',public_chart=public)
        self.job.update(source_public_id='public/original/AT/id',source_public_chart=public,
            source_public_match=False,source_kind='installed-local-variant')
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart; Get-PanelSourceText $d.job')
        self.assertEqual(r.returncode,0,r.stderr);self.assertIn('本機安裝版',r.stdout)
        self.job['source_public_match']=True
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.job['source_public_match']=False
        self.job['source_public_chart']=dict(public,sha256='a'*64)
        self.chart['public_chart']=self.job['source_public_chart']
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_profile_policy_and_complete_native_note_count_cannot_be_relabelled(self):
        self.profile.update(import_policy_id='phigros-v1-source-priority-constant-empty-candidate-v1',
            flick_path_policy='judgment-anchored-screen-fit-v1',hold_tail_policy_id='phira-tail-grace-candidate-v1')
        self.job['compile_policy']={key:self.profile[key] for key in
            ('import_policy_id','flick_path_policy','hold_tail_policy_id')}
        self.chart['notes']=2001
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart')
        self.assertEqual(r.returncode,0,r.stderr)
        self.job['compile_policy']['hold_tail_policy_id']='full-pose-v1'
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.job['compile_policy']['hold_tail_policy_id']=self.profile['hold_tail_policy_id']
        self.chart['notes']=2002
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.chart['notes']=2001
        self.profile.update(offscreen_policy_id='bounded-judgment-rectangle-candidate-v1',max_tangent_error_reference_pixels=151.2)
        self.job['compile_policy'].update(offscreen_policy_id='bounded-judgment-rectangle-candidate-v1',max_tangent_error_reference_pixels=151.2)
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart');self.assertEqual(r.returncode,0,r.stderr)
        self.job['compile_policy']['max_tangent_error_reference_pixels']=151.21
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)
        self.job['compile_policy']['max_tangent_error_reference_pixels']=151.2
        limits=dict(max_normal_projection_reference_pixels=100000,input_inset_pixels=0,drag_duration_ns=16_000_000,
            max_prejudge_ns=30_000_000,advance_above_drags=True)
        self.profile.update(limits);self.job['compile_policy'].update(limits)
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart');self.assertEqual(r.returncode,0,r.stderr)
        self.job['compile_policy']['max_prejudge_ns']=29_000_000
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

    def test_no_native_output_prioritizes_real_startup_stderr_and_keeps_release_unknown(self):
        r=self.invoke("""
$folder=Join-Path (Split-Path -Parent $Data) 'never-created-native-round'
$result=Get-PanelRoundResult $folder 1 'ADB foreground request failed: device offline'
if($result.Success -or -not $result.StartupFailure -or $result.KernelFinalZero -ne $null -or
    $result.ReleaseState -cne 'unknown' -or $result.ExecutionState -cne 'not_started'){
    throw 'Missing output invented completion, input or release evidence'
}
if(-not $result.Failure.StartsWith('ADB foreground request failed: device offline') -or
    $result.Failure.Contains($folder) -or $result.Failure -notmatch '啟動尚未READY'){
    throw 'Expected missing manifest path hid the real startup failure'
}
$presentation=Get-PanelRoundPresentation $result
if($presentation.Title -cne '啟動尚未READY'){throw 'Startup failure was displayed as gameplay failure'}
$null=New-Item -ItemType Directory -Path $folder
$unknown=Get-PanelRoundResult $folder 1 'native transport failed'
if($unknown.StartupFailure -or $unknown.ReleaseState -cne 'unknown' -or $unknown.Failure -match '未送出觸控'){
    throw 'Created but incomplete native round falsely claims no input'
}
""")
        self.assertEqual(r.returncode,0,r.stderr)

if __name__=='__main__':unittest.main()
