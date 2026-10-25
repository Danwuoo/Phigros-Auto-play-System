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
        return subprocess.run([PWSH,'-NoProfile','-NonInteractive','-File',str(driver),str(PANEL),str(self.root/'data.json')],capture_output=True,text=True,timeout=40)

    def test_more_than2000_and_explicit_at_profile_are_accepted(self):
        r=self.invoke('Assert-CatalogPanelJob $d.job $d.chart')
        self.assertEqual(r.returncode,0,r.stderr)

    def test_in_policy_cannot_be_relabelled_as_at(self):
        self.profile['hud_policy']='shared-in-subpixel-prefix-score-v3'
        self.assertNotEqual(self.invoke('Assert-CatalogPanelJob $d.job $d.chart').returncode,0)

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

if __name__=='__main__':unittest.main()
