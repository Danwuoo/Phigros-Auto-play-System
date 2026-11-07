"""Fresh panel bootstrap contracts using synthetic files and mocked offline CLI."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import bootstrap_manual_panel as bootstrap


class FreshManualPanelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.chart = self.write('IN.json', {'formatVersion': 3, 'synthetic': True})
        self.source = self.write('source.json', dict(track_id=bootstrap.SEED_TRACK,
            difficulty='IN', chart=bootstrap.reference(self.chart)))
        chart_row = dict(library_id='v/Glaciaxion/IN/hash', track_id=bootstrap.SEED_TRACK,
            difficulty='IN', chart=dict(file=self.chart.name, sha256=bootstrap.sha(self.chart)))
        self.library = self.write('library.json', dict(charts=[chart_row]))
        self.identity = self.write('identity.json', dict(schema='pas.installed-catalog-identity.v1',
            library=bootstrap.reference(self.library), charts=[dict(library_id=chart_row['library_id'],
            status='verified_exact_installed_chart', source_manifest=bootstrap.reference(self.source))]))
        self.hud = self.root / 'actual-test-fixture.rgb'
        self.hud.write_bytes(bytes(bootstrap.RGB_BYTES))
        self.state = self.write('state.json', dict(schema='pas.screen-state-dataset.v1', width=1280, height=720,
            capture_geometry_profile='sensor-portrait-r0-ccw90',
            experimental_shared_IN_HUD=True, hud_glyph_policy='shared_in_subpixel_prefix_score_v3',
            templates=[dict(state=state, rgb=dict(path=str(self.hud), sha256=bootstrap.sha(self.hud)))
                       for state in ('playing', 'paused', 'result')]))
        self.archive = self.root / 'archive'; (self.archive / 'bin').mkdir(parents=True)
        self.exe = self.archive / 'bin' / 'chart_experiment.exe'; self.exe.write_bytes(b'synthetic binary')
        self.manifest = self.write('archive/manifest.json', dict(schema='pas.adaptive-build-archive.v1',
            artifacts=[dict(file='bin/chart_experiment.exe', sha256=bootstrap.sha(self.exe))]))
        self.kernel = self.write('kernel.json', dict(boot_id='actual-supplied-boot'))
        self.capacity = self.write('capacity.json', dict(capacity=10, boot_id='actual-supplied-boot'))
        self.calls = []

    def write(self, name, value):
        path = self.root / name
        path.write_text(json.dumps(value), encoding='utf-8')
        return path

    def native(self, args, *unused, **kwargs):
        self.calls.append(args)
        self.assertEqual(args[1], 'prepare-adaptive')
        self.assertEqual(args[args.index('--contact-capacity') + 1], '10')
        self.assertEqual(args[args.index('--flick-path-policy') + 1], 'judgment-anchored-straight-v1')
        folder = Path(args[4]); folder.mkdir()
        (folder / 'song-profile.json').write_text(json.dumps(dict(contact_capacity=10,
            chart_sha256=bootstrap.sha(self.chart), source_manifest_sha256=bootstrap.sha(self.source),
            expected_plan_sha256='a'*64, expected_contacts=6, expected_steps=12, expected_peak=6)), encoding='utf-8')
        return dict(returncode=0, failure=None, stdout='{}', stderr='')

    def bulk(self, library, identity, seed, at_state, at_reference, executable, output, **kwargs):
        self.assertIsNone(at_state); self.assertIsNone(at_reference)
        self.assertEqual(kwargs, dict(contact_capacity=10, image_retention='result'))
        seed_config = bootstrap.read(seed)
        self.assertEqual(seed_config['schema'], 'pas.catalog-bootstrap-seed.v1')
        seed_library = bootstrap.read(seed_config['library']['file'])
        self.assertEqual(seed_library['schema'], 'pas.catalog-bootstrap-library.v1')
        self.assertEqual(seed_library['songs'][0]['contact_capacity'], 10)
        output.mkdir()
        value = dict(jobs=[dict(library_id='synthetic')], counts=dict(ready=1, charts=1))
        bootstrap.write_new(output / 'test-library.json', value)
        return value

    def call(self, **kwargs):
        args = dict(library=self.library, identity=self.identity, in_state=self.state,
            in_reference=self.hud, executable=self.exe, build_manifest=self.manifest,
            kernel_profile=self.kernel, capacity_evidence=self.capacity, output=self.root/'new-panel')
        args.update(kwargs)
        return bootstrap.bootstrap(**args)

    def test_fresh_in_only_panel_pins_result_only_policy_and_preserves_boot_proofs(self):
        before = self.kernel.read_bytes(), self.capacity.read_bytes()
        with patch.object(bootstrap, 'run_bounded', side_effect=self.native), patch.object(bootstrap.bulk, 'prepare', side_effect=self.bulk):
            config = self.call()
        self.assertEqual(len(self.calls), 1)
        self.assertEqual(config['image_retention'], 'result')
        self.assertTrue(config['manual_selection']); self.assertTrue(config['manual_start'])
        self.assertEqual(config['contact_capacity'], 10)
        self.assertEqual(config['library'], config['plan'])
        self.assertEqual(config['kernel_profile'], bootstrap.reference(self.kernel))
        self.assertEqual(config['capacity_evidence'], bootstrap.reference(self.capacity))
        self.assertEqual((self.kernel.read_bytes(), self.capacity.read_bytes()), before)
        summary = bootstrap.read(self.root / 'new-panel' / 'summary.json')
        self.assertFalse(summary['device_access']); self.assertFalse(summary['input_enabled'])
        self.assertFalse(summary['gameplay_qualified']); self.assertFalse(summary['at_calibration_supplied'])

    def test_wrong_seed_source_does_not_launch_native_preparation(self):
        value = bootstrap.read(self.source); value['difficulty']='AT'
        self.source.write_text(json.dumps(value), encoding='utf-8')
        with patch.object(bootstrap, 'run_bounded') as native:
            with self.assertRaises(ValueError): self.call()
            native.assert_not_called()

    def test_missing_playing_calibration_is_rejected(self):
        value = bootstrap.read(self.state); value['templates'] = value['templates'][1:]
        self.state.write_text(json.dumps(value), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'playing, paused and result'): self.call()

    def test_unarchived_native_binary_and_diagnostic_images_are_rejected(self):
        other = self.root / 'other.exe'; other.write_bytes(b'different binary')
        with self.assertRaisesRegex(ValueError, 'pinned runtime'): self.call(executable=other)
        with self.assertRaisesRegex(ValueError, 'only the confirmed result'): self.call(image_retention='diagnostic')

    def test_calibration_accepts_actual_landscape_profile_but_rejects_unrecognized_geometry(self):
        value=bootstrap.read(self.state);value['capture_geometry_profile']='landscape-r1'
        self.state.write_text(json.dumps(value),encoding='utf-8')
        self.assertEqual(bootstrap.validate_calibration(self.state,self.hud,'IN'),(self.state,self.hud))
        value['capture_geometry_profile']='invented-geometry'
        self.state.write_text(json.dumps(value),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'actual supported capture'):bootstrap.validate_calibration(self.state,self.hud,'IN')


if __name__ == '__main__': unittest.main()
