"""Build a fresh manual panel from exact installed sources and real calibration.

No emulator discovery, application launch or input is performed. The native
commands are prepare-adaptive/check-adaptive only. No historical private files,
old boot IDs or gameplay qualifications are manufactured or imported.
"""
import argparse
import json
from pathlib import Path

import prepare_catalog_panel as bulk
from inspect_catalog_native import run_bounded
from verify_installed_catalog import reference, sha, write_new


SEED_TRACK = 'Glaciaxion.SunsetRay.0'
RGB_BYTES = 1280 * 720 * 3


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def checked(pin):
    if reference(pin['file'])['sha256'] != pin['sha256'].lower():
        raise ValueError('Pinned input changed: ' + pin['file'])
    return Path(pin['file']).resolve()


def validate_calibration(state, hud, difficulty):
    state, hud = Path(state).resolve(), Path(hud).resolve()
    value = read(state)
    if (value.get('schema') != 'pas.screen-state-dataset.v1' or
        value.get('width') != 1280 or value.get('height') != 720 or
        value.get('experimental_shared_IN_HUD') is not True):
        raise ValueError('Real normalized 1280x720 shared HUD calibration is required')
    if value.get('capture_geometry_profile') not in ('sensor-portrait-r0-ccw90', 'landscape-r1'):
        raise ValueError('Fresh local calibration must bind the actual supported capture geometry')
    policy = ('shared_difficulty_subpixel_prefix_score_v4' if difficulty == 'AT'
              else 'shared_in_subpixel_prefix_score_v3')
    if value.get('hud_glyph_policy') != policy or (difficulty == 'AT' and value.get('hud_difficulty') != 'AT'):
        raise ValueError('Calibration policy does not match requested difficulty')
    if hud.stat().st_size != RGB_BYTES:
        raise ValueError('HUD reference must contain one complete normalized RGB frame')
    hud_sha = sha(hud)
    templates = value.get('templates', [])
    if not 1 <= len(templates) <= 8 or not {'playing', 'paused', 'result'} <= {t['state'] for t in templates}:
        raise ValueError('Real playing, paused and result calibration templates are required')
    matched = False
    for template in templates:
        pin = template['rgb']; path = Path(pin['path'])
        if not path.is_absolute(): path = state.parent / path
        if path.stat().st_size != RGB_BYTES or sha(path) != pin['sha256']:
            raise ValueError('Calibration RGB pin or geometry differs')
        matched |= template['state'] == 'playing' and pin['sha256'] == hud_sha
    if not matched:
        raise ValueError('HUD reference must match an actual playing template RGB')
    return state, hud


def bootstrap(library, identity, in_state, in_reference, executable, build_manifest,
              kernel_profile, capacity_evidence, output, at_state=None,
              at_reference=None, contact_capacity=10, image_retention='result'):
    contact_capacity = bulk.checked_contact_capacity(contact_capacity)
    if image_retention != 'result':
        raise ValueError('This desktop launcher saves only the confirmed result image')
    if (at_state is None) != (at_reference is None):
        raise ValueError('AT state and reference must be supplied together')
    paths = list(map(lambda p: Path(p).resolve(), (library, identity, executable,
                     build_manifest, kernel_profile, capacity_evidence, output)))
    library, identity, executable, build_manifest, kernel_profile, capacity_evidence, output = paths
    if output.exists(): raise FileExistsError('Bootstrap output must be new')
    in_state, in_reference = validate_calibration(in_state, in_reference, 'IN')
    if at_state is not None:
        at_state, at_reference = validate_calibration(at_state, at_reference, 'AT')
    catalog, proof, archive = read(library), read(identity), read(build_manifest)
    if proof.get('schema') != 'pas.installed-catalog-identity.v1' or proof.get('library') != reference(library):
        raise ValueError('Installed identity map must bind this exact public catalog')
    if archive.get('schema') != 'pas.adaptive-build-archive.v1':
        raise ValueError('A fresh native runtime archive is required')
    archived = build_manifest.parent / 'bin' / 'chart_experiment.exe'
    artifacts = [a for a in archive['artifacts'] if a['file'].replace('\\', '/') == 'bin/chart_experiment.exe']
    if len(artifacts) != 1 or sha(executable) != artifacts[0]['sha256'] or sha(archived) != artifacts[0]['sha256']:
        raise ValueError('Native executable does not match the pinned runtime archive')
    candidates = [c for c in catalog['charts'] if c['track_id'] == SEED_TRACK and c['difficulty'] == 'IN']
    if len(candidates) != 1: raise ValueError('One exact Glaciaxion IN seed is required')
    chart = candidates[0]
    rows = [r for r in proof['charts'] if r['library_id'] == chart['library_id']]
    if len(rows) != 1 or rows[0]['status'] != 'verified_exact_installed_chart':
        raise ValueError('Seed chart has not been verified against the installed APK')
    source = checked(rows[0]['source_manifest']); source_value = read(source)
    chart_path = (library.parent / chart['chart']['file']).resolve()
    if (not chart_path.is_relative_to(library.parent) or sha(chart_path) != chart['chart']['sha256'] or
        source_value['track_id'] != SEED_TRACK or source_value['difficulty'] != 'IN' or
        source_value['chart']['sha256'] != chart['chart']['sha256']):
        raise ValueError('Seed chart/source identity differs')
    # Kernel/capacity files are pinned as supplied. The native live gate checks
    # actual current-boot evidence; bootstrap never rewrites their contents.
    input_paths = [library, identity, in_state, in_reference, executable, build_manifest,
                   kernel_profile, capacity_evidence, source]
    if at_state is not None: input_paths.extend((at_state, at_reference))
    pins = {str(p): sha(p) for p in input_paths}
    output.mkdir(parents=True); (output / 'runs').mkdir()
    seed_dir = output / 'seed'; seed_dir.mkdir(); prepared = seed_dir / 'prepared'
    args = [str(executable), 'prepare-adaptive', str(chart_path), str(source), str(prepared),
        '--normal-projection-pixels', '7000', '--drag-duration-ms', '32', '--input-inset-pixels', '64',
        '--contact-capacity', str(contact_capacity), '--hud-policy', 'shared-in-subpixel-prefix-score-v3',
        '--import-policy', 'phigros-v3-source-priority-constant-empty-candidate-v1',
        '--tight-projection-speed-bounds', '1', '--flick-lead-ms', '30', '--flick-travel-ms', '60',
        '--flick-distance-pixels', '72', '--flick-release-ms', '24',
        '--flick-path-policy', 'judgment-anchored-straight-v1']
    execution = run_bounded(args, 60, seed_dir, output_limit=2 * 1024 * 1024)
    write_new(seed_dir / 'prepare-execution.json', dict(argv=args, **execution))
    if execution['returncode'] or execution['failure']:
        raise ValueError('Fresh seed preparation failed: ' + execution['stderr'][:600])
    profile_path = prepared / 'song-profile.json'; profile = read(profile_path)
    if (profile.get('contact_capacity', 4) != contact_capacity or profile['chart_sha256'] != chart['chart']['sha256'] or
        profile['source_manifest_sha256'] != sha(source)):
        raise ValueError('Fresh seed profile does not match requested capacity/source')
    seed_job = dict(track_id=SEED_TRACK, difficulty='IN', display_name='Glaciaxion IN · 直線 Flick 72px',
        chart=reference(chart_path), profile=reference(profile_path), source_manifest=reference(source),
        state_calibration=reference(in_state), hud_reference=reference(in_reference),
        compiled_fingerprint=profile['expected_plan_sha256'], notes=profile['expected_contacts'],
        events=profile['expected_steps'], peak=profile['expected_peak'], contact_capacity=contact_capacity)
    seed_library = seed_dir / 'library.json'
    write_new(seed_library, dict(schema='pas.catalog-bootstrap-library.v1', songs=[seed_job],
        scope='Fresh offline seed only; no historical gameplay qualification'))
    seed_config = seed_dir / 'configuration.json'
    write_new(seed_config, dict(schema='pas.catalog-bootstrap-seed.v1', library=reference(seed_library),
        kernel_profile=reference(kernel_profile), capacity_evidence=reference(capacity_evidence)))
    bulk_dir = output / 'catalog'
    plan = bulk.prepare(library, identity, seed_config, at_state, at_reference, executable, bulk_dir,
                        contact_capacity=contact_capacity, image_retention=image_retention)
    repo = Path(__file__).resolve().parents[1]
    gui = repo / 'tools' / 'sweep-panel.ps1'
    scripts = [repo / 'tools' / name for name in ('sweep-panel.ps1', 'catalog-panel-core.ps1',
                                               'song-library-panel.ps1', 'start-song-library.ps1')]
    full_plan = bulk_dir / 'test-library.json'
    config = dict(schema='pas.manual-song-panel-launch.v2', manual_selection=True, manual_start=True,
        image_retention=image_retention, contact_capacity=contact_capacity,
        capture_geometry_profile=read(in_state)['capture_geometry_profile'], round_limit=len(plan['jobs']),
        plan=reference(full_plan), library=reference(full_plan), catalog=reference(library), identity=reference(identity),
        build_archive=str(build_manifest.parent), build_manifest=reference(build_manifest),
        kernel_profile=reference(kernel_profile), capacity_evidence=reference(capacity_evidence),
        gui_script=reference(gui), scripts=list(map(reference, scripts)), output_parent=str(output / 'runs'))
    if any(sha(path) != expected for path, expected in pins.items()):
        raise ValueError('Bootstrap inputs changed during preparation')
    write_new(output / 'launch-config.json', config)
    write_new(output / 'summary.json', dict(schema='pas.manual-panel-bootstrap.v1',
        launch_configuration=reference(output / 'launch-config.json'), counts=plan['counts'],
        image_retention=image_retention, contact_capacity=contact_capacity, device_access=False,
        input_enabled=False, gameplay_qualified=False, at_calibration_supplied=at_state is not None))
    return config


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('library', 'identity', 'in-state', 'in-reference', 'executable', 'build-manifest',
                'kernel-profile', 'capacity-evidence', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    for key in ('at-state', 'at-reference'): parser.add_argument('--' + key, type=Path)
    parser.add_argument('--contact-capacity', type=int, choices=range(1, 11), default=10)
    parser.add_argument('--image-retention', choices=('result',), default='result')
    values = vars(parser.parse_args())
    config = bootstrap(**values)
    print(json.dumps(dict(status='offline_panel_prepared', rounds=config['round_limit'],
                          image_retention=config['image_retention']), ensure_ascii=False))
