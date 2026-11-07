"""Offline preparation cache/derivative boundaries, with synthetic files only."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

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
            import_policy_id='phigros-v3-source-priority-constant-empty-candidate-v1',
            expected_contacts=2001,expected_steps=4002,expected_peak=1,contact_capacity=5)
        self.save()

    def save(self):self.path.write_text(json.dumps(self.profile),encoding='utf-8')

    def lookup(self):return prep.cached_profile(self.cache,'key',self.chart,self.source,'shared-difficulty-subpixel-prefix-score-v4')

    def test_cached_profile_requires_exact_identity(self):
        self.assertEqual(self.lookup(),self.path)
        self.profile['difficulty']='IN';self.save();self.assertIsNone(self.lookup())

    def test_changed_source_never_reuses_old_profile(self):
        self.source.write_text('different',encoding='utf-8')
        self.assertIsNone(self.lookup())

    def test_cache_policy_preserves_known_old_profile_but_cannot_relabel_partial_or_wrong_import_policy(self):
        policy='phigros-v3-source-priority-constant-empty-candidate-v1'
        lookup=lambda:prep.cached_profile(self.cache,'key',self.chart,self.source,
            'shared-difficulty-subpixel-prefix-score-v4',5,policy)
        self.assertEqual(lookup(),self.path)
        self.profile['flick_path_policy']='judgment-anchored-screen-fit-v1';self.save()
        self.assertIsNone(lookup())
        self.profile['hold_tail_policy_id']='phira-tail-grace-candidate-v1';self.save()
        self.assertEqual(lookup(),self.path)
        self.profile['import_policy_id']='phigros-v1-source-priority-constant-empty-candidate-v1';self.save()
        self.assertIsNone(lookup())

    def test_native_v1_import_is_explicit_and_other_formats_are_rejected(self):
        self.assertEqual(prep.import_policy_for(dict(formatVersion=1)),
            'phigros-v1-source-priority-constant-empty-candidate-v1')
        self.assertEqual(prep.import_policy_for(dict(formatVersion=3)),
            'phigros-v3-source-priority-constant-empty-candidate-v1')
        explicit='phigros-v1-source-priority-empty-interval-candidate-v1'
        self.assertEqual(prep.import_policy_for(dict(formatVersion=1),explicit),explicit)
        self.assertEqual(prep.import_policy_for(dict(formatVersion=3),explicit),
            'phigros-v3-source-priority-constant-empty-candidate-v1')
        with self.assertRaises(ValueError):prep.import_policy_for(dict(formatVersion=1),'unknown')
        with self.assertRaises(ValueError):prep.import_policy_for(dict(formatVersion=2))

    def test_rectangle_candidate_is_explicit_bounded_and_cannot_reuse_normal_cache(self):
        self.assertIsNone(prep.checked_projection_policy())
        requested=prep.checked_projection_policy('bounded-judgment-rectangle-candidate-v1',151.2)
        self.assertEqual(requested['max_tangent_error_reference_pixels'],151.2)
        for policy,tangent in ((None,151.2),('bounded-judgment-rectangle-candidate-v1',None),
            ('unknown',151.2),('bounded-judgment-rectangle-candidate-v1',float('nan')),
            ('bounded-judgment-rectangle-candidate-v1',151.2001),('bounded-judgment-rectangle-candidate-v1',True)):
            with self.assertRaises(ValueError):prep.checked_projection_policy(policy,tangent)
        lookup=lambda:prep.cached_profile(self.cache,'key',self.chart,self.source,
            'shared-difficulty-subpixel-prefix-score-v4',5,projection_policy=requested)
        self.assertIsNone(lookup())
        self.profile.update(requested);self.save();self.assertEqual(lookup(),self.path)

    def test_candidate_limits_are_finite_explicit_and_cache_must_match_every_override(self):
        default=prep.checked_candidate_limits()
        self.assertEqual(default,dict(max_normal_projection_reference_pixels=7000,input_inset_pixels=64,
            drag_duration_ns=32_000_000,max_prejudge_ns=0,advance_above_drags=False,max_hold_reacquire_gap_ns=0))
        requested=prep.checked_candidate_limits(100000,0,16,30,True)
        self.assertEqual(requested['max_prejudge_ns'],30_000_000)
        for kwargs in (dict(normal=1_000_001),dict(normal=True),dict(inset=-1),dict(inset=101),dict(drag_ms=0),
            dict(drag_ms=33),dict(prejudge_ms=31),dict(prejudge_ms=0,advance_above_drags=True),dict(advance_above_drags=1),dict(hold_gap_ms=21)):
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):prep.checked_candidate_limits(**kwargs)
        lookup=lambda:prep.cached_profile(self.cache,'key',self.chart,self.source,
            'shared-difficulty-subpixel-prefix-score-v4',5,projection_policy=requested)
        self.assertIsNone(lookup());self.profile.update(requested);self.save();self.assertEqual(lookup(),self.path)
        self.profile['max_prejudge_ns']=29_000_000;self.save();self.assertIsNone(lookup())

    def test_event_budget_still_applies_after_removing_note_cap(self):
        self.profile['expected_steps']=16001;self.save();self.assertIsNone(self.lookup())

    def test_explicit_bounded_runtime_profile_supports_longer_or_more_detailed_chart(self):
        self.profile.update(runtime_step_budget=65536,runtime_duration_limit_ns=230_000_000_000,
            last_up_song_ns=199_761_900_000,expected_steps=27255)
        self.save();self.assertEqual(self.lookup(),self.path)
        budget=prep.runtime_budget(self.profile)
        self.assertEqual(budget['expected_steps'],27255)
        self.assertEqual(budget['last_up_song_ns'],199_761_900_000)
        self.profile['runtime_step_budget']=65537;self.save();self.assertIsNone(self.lookup())

    def test_runtime_budget_rejects_unbounded_malformed_and_inconsistent_values(self):
        for update in (dict(runtime_step_budget=True),dict(runtime_step_budget=65537),
            dict(runtime_duration_limit_ns=600_000_000_001),dict(runtime_duration_limit_ns=180.0),
            dict(last_up_song_ns=180_000_000_000),dict(expected_peak=6),dict(expected_contacts=3000)):
            with self.subTest(update=update),self.assertRaises((ValueError,KeyError)):
                prep.runtime_budget(dict(self.profile,**update))

    def test_output_limit_failure_is_reported_as_transport_limit_not_false_chart_budget(self):
        details=dict(error_counts={},events=3701,runtime_step_budget=16000,last_up_song_ns=159_800_000_000,
            runtime_duration_limit_ns=180_000_000_000,within_runtime_bounds=True)
        reason=prep.preparation_failure(dict(returncode=1,failure='output_limit',stderr=''),details)
        self.assertIn('output_limit',reason)
        self.assertNotIn('容量',reason)

    def test_failure_reports_actual_declared_budget_instead_of_generic_rejection(self):
        details=dict(error_counts={},events=19477,runtime_step_budget=16000,last_up_song_ns=191_432_400_000,
            runtime_duration_limit_ns=180_000_000_000)
        reason=prep.preparation_failure(dict(returncode=2,failure=None,stderr=''),details)
        self.assertIn('19477/16000',reason)
        self.assertIn('191.432/180',reason)

    def test_ten_pointer_request_cannot_reuse_old_five_pointer_profile(self):
        policy='shared-difficulty-subpixel-prefix-score-v4'
        self.assertIsNone(prep.cached_profile(self.cache,'key',self.chart,self.source,policy,10))
        self.profile['contact_capacity']=10;self.save()
        self.assertEqual(prep.cached_profile(self.cache,'key',self.chart,self.source,policy,10),self.path)
        self.assertIsNone(self.lookup())

    def test_only_integer_capacities_one_through_ten_are_allowed(self):
        for value in range(1,11):self.assertEqual(prep.checked_contact_capacity(value),value)
        for value in (-1,0,11,True,5.5,'10'):
            with self.assertRaises(ValueError):prep.checked_contact_capacity(value)

    def test_ten_pointer_offline_preparation_does_not_relabel_old_five_pointer_device_proof(self):
        def write(name,value):
            path=self.root/name;path.write_text(json.dumps(value),encoding='utf-8');return path
        chart=write('AT.json',dict(formatVersion=3))
        library=write('library.json',dict(counts=dict(song_count=1),charts=[dict(
            library_id='v/synthetic.0/AT/hash',track_id='synthetic.0',title='Synthetic',difficulty='AT',variant='0',notes=6,
            chart=dict(file=chart.name,sha256=prep.sha(chart)))]))
        identity=write('identity.json',dict(library=prep.reference(library),charts=[dict(
            library_id='v/synthetic.0/AT/hash',status='verified_exact_installed_chart',source_manifest=prep.reference(self.source))]))
        old_library=write('old-library.json',dict(songs=[]))
        kernel=write('kernel.json',dict(boot_id='old-boot'))
        old_capacity=write('old-capacity.json',dict(capacity=5,boot_id='old-boot'))
        legacy=write('legacy.json',dict(library=prep.reference(old_library),kernel_profile=prep.reference(kernel),
            capacity_evidence=prep.reference(old_capacity)))
        state=write('state.json',dict(synthetic=True));hud=write('hud.rgb',dict(synthetic=True))
        executable=write('synthetic-native.exe',dict(synthetic=True));calls=[]
        def native(args,*unused,**kwargs):
            calls.append(args)
            if args[1]=='prepare-adaptive':
                destination=Path(args[4]);destination.mkdir()
                profile=dict(self.profile,chart_sha256=prep.sha(chart),expected_contacts=6,expected_steps=12,
                    expected_peak=6,expected_plan_sha256='b'*64,contact_capacity=10,
                    flick_path_policy='judgment-anchored-screen-fit-v1',hold_tail_policy_id='phira-tail-grace-candidate-v1')
                (destination/'song-profile.json').write_text(json.dumps(profile),encoding='utf-8')
                return dict(returncode=0,failure=None,stdout='{}',stderr='')
            self.assertEqual(args[1],'check-adaptive')
            self.assertNotIn('--capacity-evidence',args)
            return dict(returncode=0,failure=None,stdout=json.dumps(dict(input_enabled=False,image_retention='none')),stderr='')
        before=old_capacity.read_bytes()
        with patch.object(prep,'run_bounded',side_effect=native):
            result=prep.prepare(library,identity,legacy,state,hud,executable,self.root/'prepared',contact_capacity=10)
        self.assertEqual(len(calls),2)
        self.assertEqual(calls[0][calls[0].index('--contact-capacity')+1],'10')
        self.assertEqual(result['contact_capacity'],10)
        self.assertTrue(result['jobs'][0]['ready_for_manual_preflight'])
        self.assertFalse(result['gameplay_qualified'])
        self.assertEqual(old_capacity.read_bytes(),before)

    def test_only_new_derived_plan_is_trimmed_and_identity_summary_kept(self):
        original=self.root/'private-original.bin';original.write_bytes(b'keep exactly')
        plan=self.path.parent/'compiled-plan.json'
        value=dict(cache_sha256='b'*64,contacts=[dict(id=1)],total_steps=2)
        plan.write_text(json.dumps(value),encoding='utf-8');before=hashlib.sha256(plan.read_bytes()).hexdigest()
        prep.trim_derived_plan(self.path.parent)
        self.assertFalse(plan.exists());self.assertEqual(original.read_bytes(),b'keep exactly')
        summary=json.loads((self.path.parent/'compiled-plan-summary.json').read_text())
        self.assertEqual(summary['sha256'],before);self.assertEqual(summary['events'],2)

    def test_configured_prepare_accepts_native_v1_and_explicit_budget_without_fixture_files(self):
        def write(name,value):
            path=self.root/name;path.write_text(json.dumps(value),encoding='utf-8');return path
        chart=write('local-AT.json',dict(formatVersion=1))
        public=write('public-AT.json',dict(formatVersion=1,revision='public-different'))
        chart_row=dict(library_id='installed/v157/synthetic/AT/hash',upstream_library_id='public/synthetic/AT/hash',
            public_chart=prep.reference(public),public_match=False,track_id='synthetic.0',title='Synthetic',
            difficulty='AT',variant='0',notes=6,chart=dict(file=str(chart),sha256=prep.sha(chart)))
        library=write('effective-library.json',dict(counts=dict(song_count=1),charts=[chart_row]))
        identity=write('effective-identity.json',dict(library=prep.reference(library),charts=[dict(
            library_id=chart_row['library_id'],status='verified_exact_installed_chart',source_manifest=prep.reference(self.source))]))
        old_library=write('empty-seed.json',dict(songs=[]));kernel=write('route-template.json',dict(boot_id='historical-template'))
        legacy=write('configured-seed.json',dict(library=prep.reference(old_library),kernel_profile=prep.reference(kernel),
            capacity_policy='configured-ten-contact-v1'))
        state=write('actual-state-fixture.json',dict(synthetic=True));hud=write('actual-hud-fixture.rgb',dict(synthetic=True))
        executable=write('fake-native.exe',dict(synthetic=True));calls=[]
        def native(args,*unused,**kwargs):
            calls.append(args)
            if args[1]=='prepare-adaptive':
                self.assertEqual(args[args.index('--import-policy')+1],'phigros-v1-source-priority-constant-empty-candidate-v1')
                self.assertEqual(args[args.index('--flick-path-policy')+1],'judgment-anchored-screen-fit-v1')
                self.assertEqual(args[args.index('--hold-tail-policy')+1],'phira-tail-grace-candidate-v1')
                destination=Path(args[4]);destination.mkdir()
                profile=dict(self.profile,chart_sha256=prep.sha(chart),expected_contacts=6,expected_steps=12,
                    expected_peak=6,expected_plan_sha256='b'*64,contact_capacity=10,
                    import_policy_id='phigros-v1-source-priority-constant-empty-candidate-v1',
                    flick_path_policy='judgment-anchored-screen-fit-v1',hold_tail_policy_id='phira-tail-grace-candidate-v1',
                    runtime_step_budget=65536,runtime_duration_limit_ns=230_000_000_000,last_up_song_ns=200_000_000_000)
                if '--offscreen-policy' in args:
                    self.assertEqual(len(args),35+(4 if '--max-prejudge-ms' in args else 0)+(2 if '--max-hold-gap-ms' in args else 0))
                    self.assertEqual(args[args.index('--offscreen-policy')+1],'bounded-judgment-rectangle-candidate-v1')
                    self.assertEqual(float(args[args.index('--max-tangent-error-pixels')+1]),151.2)
                    profile.update(offscreen_policy_id='bounded-judgment-rectangle-candidate-v1',max_tangent_error_reference_pixels=151.2)
                if '--max-prejudge-ms' in args:
                    self.assertEqual(args[args.index('--max-prejudge-ms')+1],'30')
                    self.assertEqual(args[args.index('--advance-above-drags')+1],'1')
                    profile.update(max_prejudge_ns=30_000_000,advance_above_drags=True,
                        max_normal_projection_reference_pixels=int(args[args.index('--normal-projection-pixels')+1]),
                        input_inset_pixels=int(args[args.index('--input-inset-pixels')+1]),
                        drag_duration_ns=int(args[args.index('--drag-duration-ms')+1])*1_000_000)
                if '--max-hold-gap-ms' in args:
                    self.assertEqual(args[args.index('--max-hold-gap-ms')+1],'20')
                    profile['max_hold_reacquire_gap_ns']=20_000_000
                (destination/'song-profile.json').write_text(json.dumps(profile),encoding='utf-8')
                report=dict(compiled=True,within_live_experiment_bounds=True,source_notes=6,diagnostics=[],
                    runtime_step_budget=65536,runtime_duration_limit_ns=230_000_000_000,last_up_song_ns=200_000_000_000)
                (destination/'report.json').write_text(json.dumps(report),encoding='utf-8')
                return dict(returncode=0,failure=None,stdout=json.dumps(dict(schema='pas.adaptive-song-prepare-summary.v1',report_file='report.json')),stderr='')
            self.assertEqual(args[1],'check-adaptive')
            self.assertNotIn('--capacity-evidence',args)
            self.assertEqual(args[args.index('--capacity-policy')+1],'configured-ten-contact-v1')
            return dict(returncode=0,failure=None,stdout=json.dumps(dict(input_enabled=False,image_retention='result',
                capacity_policy='configured-ten-contact-v1',last_up_song_ns=200_000_000_000)),stderr='')
        with patch.object(prep,'run_bounded',side_effect=native):
            result=prep.prepare(library,identity,legacy,state,hud,executable,self.root/'configured-output',
                contact_capacity=10,image_retention='result')
        job=result['jobs'][0]
        self.assertEqual(len(calls),2);self.assertTrue(job['ready_for_manual_preflight'])
        self.assertEqual(job['runtime_budget']['runtime_step_budget'],65536)
        self.assertEqual(job['runtime_budget']['last_up_song_ns'],200_000_000_000)
        self.assertEqual(job['source_kind'],'installed-local-variant')
        self.assertEqual(job['preparation_strategy'],'screen-fit-phira-tail-candidate')
        self.assertEqual(job['compile_policy']['hold_tail_policy_id'],'phira-tail-grace-candidate-v1')
        self.assertEqual(job['source_public_id'],chart_row['upstream_library_id'])
        self.assertEqual(job['source_public_chart'],prep.reference(public))
        self.assertNotEqual(job['chart']['sha256'],job['source_public_chart']['sha256'])
        self.assertFalse(result['gameplay_qualified'])
        calls.clear()
        with patch.object(prep,'run_bounded',side_effect=native):
            rectangle=prep.prepare(library,identity,legacy,state,hud,executable,self.root/'rectangle-output',
                contact_capacity=10,image_retention='result',offscreen_policy='bounded-judgment-rectangle-candidate-v1',max_tangent_error_pixels=151.2)
        self.assertEqual(len(calls),2);self.assertTrue(rectangle['jobs'][0]['ready_for_manual_preflight'])
        self.assertEqual(rectangle['jobs'][0]['preparation_strategy'],'bounded-judgment-rectangle-phira-tail-candidate')
        self.assertEqual(rectangle['jobs'][0]['compile_policy']['max_tangent_error_reference_pixels'],151.2)
        calls.clear()
        with patch.object(prep,'run_bounded',side_effect=native):
            early=prep.prepare(library,identity,legacy,state,hud,executable,self.root/'prejudge-output',
                contact_capacity=10,image_retention='result',offscreen_policy='bounded-judgment-rectangle-candidate-v1',max_tangent_error_pixels=151.2,
                normal_projection_pixels=100000,input_inset_pixels=0,drag_duration_ms=16,max_prejudge_ms=30,advance_above_drags=True,max_hold_gap_ms=20)
        self.assertEqual(len(calls),2);self.assertTrue(early['jobs'][0]['ready_for_manual_preflight'])
        self.assertEqual(early['jobs'][0]['compile_policy']['max_prejudge_ns'],30_000_000)
        self.assertTrue(early['jobs'][0]['compile_policy']['advance_above_drags'])
        self.assertEqual(early['jobs'][0]['compile_policy']['max_hold_reacquire_gap_ns'],20_000_000)
        with self.assertRaises(ValueError):
            prep.prepare(library,identity,legacy,state,hud,executable,self.root/'invalid-gap-output',contact_capacity=10,max_hold_gap_ms=20)

if __name__=='__main__':unittest.main()
