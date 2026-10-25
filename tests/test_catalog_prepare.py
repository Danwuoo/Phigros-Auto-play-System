"""Offline preparation cache/derivative boundaries, with synthetic files only."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import prepare_catalog_panel as prep


class CatalogPreparationTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.cache=self.root/'cache';self.source=self.root/'source.json'
        self.source.write_text('{"synthetic":true}',encoding='utf-8')
        self.chart=dict(track_id='synthetic.0',difficulty='AT',notes=2001,chart=dict(sha256='a'*64))
        self.path=self.cache/'songs/key/prepared/song-profile.json';self.path.parent.mkdir(parents=True)
        self.profile=dict(song_id='synthetic.0',difficulty='AT',chart_sha256='a'*64,
            source_manifest_sha256=prep.sha(self.source),hud_policy='shared-difficulty-subpixel-prefix-score-v4',
            expected_contacts=2001,expected_steps=4002)
        self.save()

    def save(self):self.path.write_text(json.dumps(self.profile),encoding='utf-8')

    def lookup(self):return prep.cached_profile(self.cache,'key',self.chart,self.source,'shared-difficulty-subpixel-prefix-score-v4')

    def test_cached_profile_requires_exact_identity(self):
        self.assertEqual(self.lookup(),self.path)
        self.profile['difficulty']='IN';self.save();self.assertIsNone(self.lookup())

    def test_changed_source_never_reuses_old_profile(self):
        self.source.write_text('different',encoding='utf-8')
        self.assertIsNone(self.lookup())

    def test_event_budget_still_applies_after_removing_note_cap(self):
        self.profile['expected_steps']=16001;self.save();self.assertIsNone(self.lookup())

    def test_only_new_derived_plan_is_trimmed_and_identity_summary_kept(self):
        original=self.root/'private-original.bin';original.write_bytes(b'keep exactly')
        plan=self.path.parent/'compiled-plan.json'
        value=dict(cache_sha256='b'*64,contacts=[dict(id=1)],total_steps=2)
        plan.write_text(json.dumps(value),encoding='utf-8');before=hashlib.sha256(plan.read_bytes()).hexdigest()
        prep.trim_derived_plan(self.path.parent)
        self.assertFalse(plan.exists());self.assertEqual(original.read_bytes(),b'keep exactly')
        summary=json.loads((self.path.parent/'compiled-plan-summary.json').read_text())
        self.assertEqual(summary['sha256'],before);self.assertEqual(summary['events'],2)

if __name__=='__main__':unittest.main()
