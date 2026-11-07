"""Refresh only previously blocked blockAreaList jobs in a pinned desktop plan offline."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path

from prepare_catalog_panel import (checked, compile_policy, import_policy_for, matches_block_area_phase,
                                   preparation_diagnostics, preparation_failure, read,
                                   runtime_budget, trim_derived_plan)
from inspect_catalog_native import run_bounded
from verify_installed_catalog import reference, write_new


CAPACITY_POLICY = 'configured-ten-contact-v1'
CATALOG_BLOCKER = 'unsupported_block_area'
STRATEGY = 'block-area-offline-detour-candidate'
SCOPE = 'offline artifact/config validation; current boot, foreground, contacts and game judgment NOT RUN'
FLICK = dict(flick_path_policy='judgment-anchored-diagonal-sampled-v2',
             flick_distance_reference_pixels=80.0, flick_lead_ms=8, flick_travel_ms=38, flick_release_ms=40)
PROJECTION = dict(hold_tail_policy_id='phira-tail-grace-candidate-v1',
                  offscreen_policy_id='bounded-judgment-rectangle-candidate-v1',
                  max_tangent_error_reference_pixels=151.2, max_normal_projection_reference_pixels=7000,
                  input_inset_pixels=64, drag_duration_ns=32_000_000, tight_projection_speed_bounds=True)
HUD_POLICIES = {'IN': 'shared-in-subpixel-prefix-score-v3', 'AT': 'shared-difficulty-subpixel-prefix-score-v4'}


def require_fields(value, expected, label):
    for key, target in expected.items():
        actual = value[key]
        if actual != target or (type(target) in (int, bool) and type(actual) is not type(target)):
            raise ValueError(f'{label} {key} differs from its pinned value')


def indexed(rows):
    result = {row['library_id']: row for row in rows}
    if not 1 <= len(result) == len(rows) <= 10000:
        raise ValueError('Bounded, unique library rows are required')
    return result


def prepare_job(job, catalog, identity, hud, pins, output, phase, allow_temporal_fallback=False):
    folder = output / 'songs' / hashlib.sha256(job['library_id'].encode('utf-8')).hexdigest()[:24]
    folder.mkdir()
    prepared = folder / 'prepared'
    result = dict(library_id=job['library_id'], track_id=job['track_id'], song=job['display_name'],
                  difficulty=job['difficulty'], notes=job['notes'], ready=False, reason=None,
                  catalog_blocker=CATALOG_BLOCKER, declared_regions=None)
    row_pins = dict(chart=job['chart'], state_calibration=hud['state_calibration'], hud_reference=hud['hud_reference'])
    details = None
    try:
        diff = job['difficulty']
        job_identity = {key: job[key] for key in ('library_id', 'track_id', 'difficulty')}
        digest = job['chart']['sha256']
        require_fields(catalog, dict(job_identity, notes=job['notes']), 'Catalog')
        require_fields(identity, dict(job_identity, chart_sha256=digest, status='verified_exact_installed_chart'), 'Identity')
        catalog_chart = (Path(pins['catalog']['file']).parent / catalog['chart']['file']).resolve()
        chart = checked(job['chart'])
        if catalog_chart != chart or catalog['chart']['sha256'] != digest:
            raise ValueError('Catalog and desktop original chart pins differ')
        row_pins['source_manifest'] = identity['source_manifest']
        source_path = checked(row_pins['source_manifest'])
        source = read(source_path)
        require_fields(source, dict(schema='pas.installed-catalog-song-source.v1', track_id=job['track_id'],
                                    difficulty=diff, exact_chart_bytes_verified=True, input_enabled=False,
                                    device_access=False), 'Installed source')
        require_fields(source['chart'], dict(sha256=digest, byte_transform='none'), 'Installed chart')
        require_fields(source['audio_identity'], dict(sha256=identity['native_audio_sha256'], format='native-fsb5'), 'Installed audio')
        raw = read(chart)
        if 'blockAreaList' not in raw:
            raise ValueError('Previously blocked chart has no exact root blockAreaList')
        areas = raw['blockAreaList']
        if areas is not None and not isinstance(areas, list):
            raise ValueError('/blockAreaList: expected an array or null')
        result['declared_regions'] = area_count = len(areas or [])
        result['phase_ms'] = phase
        projection = dict(PROJECTION, input_inset_pixels=12 if area_count else 64)
        temporal = bool(area_count and allow_temporal_fallback)
        import_policy = import_policy_for(raw)
        # Prove that the derived plan contains an owner for every raw source note.
        source_notes = {f'{digest}#/judgeLineList/{i}/{side}/{j}'
                        for i, line in enumerate(raw['judgeLineList'])
                        for side in ('notesAbove', 'notesBelow') for j in range(len(line[side]))}
        if len(source_notes) != job['notes']:
            raise ValueError('Original note count differs from the pinned desktop row')
        args = [pins['executable']['file'], 'prepare-adaptive', str(chart), str(source_path), str(prepared),
                '--normal-projection-pixels', '7000', '--drag-duration-ms', '32',
                '--input-inset-pixels', str(projection['input_inset_pixels']),
                '--contact-capacity', '10', '--hud-policy', HUD_POLICIES[diff], '--import-policy', import_policy,
                '--tight-projection-speed-bounds', '1', '--flick-path-policy', FLICK['flick_path_policy'],
                '--flick-distance-pixels', '80', '--flick-lead-ms', '8', '--flick-travel-ms', '38', '--flick-release-ms', '40',
                '--hold-tail-policy', PROJECTION['hold_tail_policy_id'], '--offscreen-policy', PROJECTION['offscreen_policy_id'],
                '--max-tangent-error-pixels', '151.2']
        if area_count:
            args += ['--block-area-phase-ms', str(phase)]
        if temporal:
            args += ['--allow-block-area-temporal-fallback', '1']
        execution = run_bounded(args, 180, folder, output_limit=2 * 1024 * 1024)
        write_new(folder / 'prepare-execution.json', dict(argv=args, **execution))
        if execution['returncode'] or execution['failure']:
            trim_derived_plan(prepared)
            details = preparation_diagnostics(prepared)
            reason = preparation_failure(execution, details)
            if 'block_area_no_safe_path' in details['error_counts']:
                reason += '。封鎖區域內找不到符合判定與觸控限制的安全路徑。'
            raise ValueError(reason)
        report = read(prepared / 'report.json')
        require_fields(report, dict(schema='pas.adaptive-song-prepare.v1', offline_only=True, device_discovery=False,
                                    compiled=True, within_live_experiment_bounds=True, source_notes=job['notes'],
                                    chart_sha256=digest, audio_sha256=identity['native_audio_sha256'],
                                    source_manifest_sha256=row_pins['source_manifest']['sha256'], flick_parameters=FLICK), 'Prepare')
        profile_path = prepared / 'song-profile.json'
        profile = read(profile_path)
        require_fields(profile, dict(projection, **FLICK, schema='pas.adaptive-song-profile.v1',
                                     song_id=job['track_id'], difficulty=diff, chart_sha256=digest,
                                     audio_sha256=identity['native_audio_sha256'], gameplay_qualified=False,
                                     source_manifest_sha256=row_pins['source_manifest']['sha256'], expected_contacts=job['notes'],
                                     import_policy_id=import_policy, hud_policy=HUD_POLICIES[diff], contact_capacity=10), 'Profile')
        budget = runtime_budget(profile)
        if not matches_block_area_phase(profile, phase if area_count else None):
            raise ValueError('Profile block area phase differs from the effective desktop phase')
        if profile.get('allow_block_area_temporal_fallback', False) is not temporal:
            raise ValueError('Profile block area temporal fallback differs from the explicit offline request')
        plan = read(prepared / 'compiled-plan.json')
        try:
            require_fields(plan, dict(chart_sha256=digest, cache_sha256=profile['expected_plan_sha256'],
                                      total_steps=profile['expected_steps'], peak_contacts=profile['expected_peak']), 'Plan')
            owners = plan['ownership']
            contact_ids = {contact['id'] for contact in plan['contacts']}
            if (len(contact_ids) != len(plan['contacts']) or len(owners) != len(source_notes) or
                    {owner['source_note_id'] for owner in owners} != source_notes or
                    {owner['contact_id'] for owner in owners} != contact_ids):
                raise ValueError('Plan must retain one unique owner and contact per original source note')
            if plan.get('block_area') != report.get('block_area'):
                raise ValueError('Plan and prepare report block area evidence differ')
        finally:
            trim_derived_plan(prepared, plan)
        details = preparation_diagnostics(prepared)
        block = report.get('block_area')
        if area_count:
            if (not block or block['phase_ns'] != phase * 1_000_000 or not block['policy_id'] or
                    type(block['active_regions']) is not int or not 0 <= block['active_regions'] <= area_count or
                    type(block['detoured_contacts']) is not int or not 0 <= block['detoured_contacts'] <= job['notes']):
                raise ValueError('Prepare block area proof differs from the declared regions and phase')
            temporal_contacts = block['temporal_fallback_contacts']
            if (type(temporal_contacts) is not int or not 0 <= temporal_contacts <= block['detoured_contacts'] or
                    block['temporal_fallback_enabled'] is not temporal or (not temporal and temporal_contacts != 0)):
                raise ValueError('Prepare temporal fallback evidence differs from the explicit offline request')
            if sum(owner.get('block_area_temporal_fallback') is True for owner in owners) != temporal_contacts:
                raise ValueError('Prepare temporal fallback count differs from the source note ownership')
        elif block is not None:
            raise ValueError('Empty block area list unexpectedly has a routing proof')
        args = [pins['executable']['file'], 'check-adaptive', 'emulator-5554', 'phigros', str(chart),
                row_pins['state_calibration']['file'], pins['kernel_profile']['file'], str(folder / 'offline-check'),
                str(phase), '--experimental-whole-song', '--song-profile', str(profile_path),
                '--experimental-hud-reference', row_pins['hud_reference']['file'],
                '--capacity-policy', CAPACITY_POLICY, '--image-retention', 'result']
        execution = run_bounded(args, 180, folder, output_limit=2 * 1024 * 1024)
        write_new(folder / 'preflight-execution.json', dict(argv=args, **execution))
        if execution['returncode'] or execution['failure']:
            raise ValueError(execution['failure'] or execution['stderr'].strip()[:600] or 'Native offline preflight failed')
        response = json.loads(execution['stdout'])
        require_fields(response, dict(status='offline_preflight_passed', device_discovery=False, input_enabled=False,
                                      executable_sha256=pins['executable']['sha256'], scope=SCOPE,
                                      song_id=job['track_id'], difficulty=diff, chart_sha256=digest,
                                      compiled_sha256=profile['expected_plan_sha256'], contacts=job['notes'],
                                      events=budget['expected_steps'], peak=budget['expected_peak'], contact_capacity=10,
                                      runtime_step_budget=budget['runtime_step_budget'], runtime_duration_limit_ns=budget['runtime_duration_limit_ns'],
                                      last_up_song_ns=budget['last_up_song_ns'], capacity_policy=CAPACITY_POLICY,
                                      capacity_evidence_required_for_live=False, capacity_test_performed=False,
                                      image_retention='result', HUD_policy=HUD_POLICIES[diff], flick_parameters=FLICK,
                                      screen_calibration_sha256=row_pins['state_calibration']['sha256'],
                                      HUD_reference_sha256=row_pins['hud_reference']['sha256'], kernel_profile_sha256=pins['kernel_profile']['sha256'],
                                      **{key: PROJECTION[key] for key in ('hold_tail_policy_id', 'offscreen_policy_id', 'max_tangent_error_reference_pixels')}),
                       'Preflight')
        require_fields(response['import'], dict(policy_id=import_policy, source_chart_sha256=digest), 'Preflight import')
        if response.get('block_area') != block or (folder / 'offline-check').exists():
            raise ValueError('Preflight block area proof or offline-only output differs')
        if block is not None:
            details['block_area'] = response['block_area']
        result.update(ready=True, block_area=block, contacts=job['notes'], events=budget['expected_steps'],
                      peak=budget['expected_peak'], compiled_fingerprint=profile['expected_plan_sha256'])
        job.update(profile=reference(profile_path), source_manifest=row_pins['source_manifest'],
                   state_calibration=row_pins['state_calibration'], hud_reference=row_pins['hud_reference'],
                   compiled_fingerprint=profile['expected_plan_sha256'], ready_for_manual_preflight=True,
                   blockers=[], events=budget['expected_steps'], peak=budget['expected_peak'], runtime_budget=budget,
                   compile_policy=dict(compile_policy(profile), **FLICK), preparation_strategy=STRATEGY)
        if area_count:
            job['compile_policy']['block_area_phase_ms'] = phase
        if temporal:
            job['compile_policy']['allow_block_area_temporal_fallback'] = True
    except (KeyError, TypeError, ValueError, OSError, RuntimeError) as error:
        result['reason'] = str(error)
        job['blockers'] = [str(error)]
    # Failed prepare/check attempts still retain the original chart and source pin and native diagnostics.
    if (prepared / 'compiled-plan.json').exists():
        trim_derived_plan(prepared)
    if details is None:
        details = preparation_diagnostics(prepared)
    job['preparation'] = details
    result['error_counts'] = details['error_counts']
    if details['error_examples']:
        result['error_examples'] = details['error_examples']
    result['inputs'] = row_pins
    write_new(folder / 'status.json', result)
    print(json.dumps({key: result[key] for key in ('song', 'difficulty', 'ready', 'reason')}, ensure_ascii=False), flush=True)
    return result


def refresh(configuration, executable, output, allow_temporal_fallback=False):
    config_ref = reference(configuration)
    config = read(config_ref['file'])
    require_fields(config, dict(schema='pas.manual-song-panel-launch.v2', manual_selection=True, manual_start=True,
                                image_retention='result', contact_capacity=10, capacity_policy=CAPACITY_POLICY), 'Configuration')
    if config['plan'] != config['library']:
        raise ValueError('Desktop plan and library pins differ')
    pins = {key: config[key] for key in ('plan', 'catalog', 'identity', 'kernel_profile')}
    pins.update(configuration=config_ref, executable=reference(executable))
    for pin in pins.values():
        checked(pin)
    plan, catalog, proof = (read(pins[key]['file']) for key in ('plan', 'catalog', 'identity'))
    require_fields(plan, dict(schema='pas.catalog-test-plan.v1', image_retention='result', contact_capacity=10,
                              capacity_policy=CAPACITY_POLICY, catalog=pins['catalog'], identity=pins['identity']), 'Plan')
    require_fields(proof, dict(schema='pas.installed-catalog-identity.v1', library=pins['catalog']), 'Identity proof')
    charts, identities, jobs = (indexed(rows) for rows in (catalog['charts'], proof['charts'], plan['jobs']))
    if charts.keys() != identities.keys() or charts.keys() != jobs.keys():
        raise ValueError('Desktop plan, catalogue and exact identity proof rows differ')
    hud = {}
    for diff, policy in HUD_POLICIES.items():
        baseline = next((job for job in jobs.values() if job['difficulty'] == diff and job['ready_for_manual_preflight']), None)
        if baseline is None or read(checked(baseline['profile']))['hud_policy'] != policy:
            raise ValueError(f'No first ready row with the pinned shared {diff} HUD policy')
        hud[diff] = {key: baseline[key] for key in ('profile', 'state_calibration', 'hud_reference')}
        for pin in hud[diff].values():
            checked(pin)
    targets = [job for job in jobs.values() if not job['ready_for_manual_preflight'] and
               any(block['code'] == CATALOG_BLOCKER for block in charts[job['library_id']]['blockers'])]
    if not targets:
        raise ValueError('The pinned desktop plan has no previous blockAreaList blockers')
    for job in targets:
        phase = job.get('phase_ms', plan['phase_ms'])
        if type(phase) is not int or not -100 <= phase <= 100:
            raise ValueError('Effective desktop phase must be an integer between -100 and 100 ms')
    output = Path(output).resolve()
    output.mkdir(parents=True)
    (output / 'songs').mkdir()
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = [pool.submit(prepare_job, job, charts[job['library_id']], identities[job['library_id']],
                               hud[job['difficulty']], pins, output, job.get('phase_ms', plan['phase_ms']),
                               allow_temporal_fallback) for job in targets]
        results = [future.result() for future in pending]
    all_pins = list(pins.values()) + [pin for refs in hud.values() for pin in refs.values()]
    all_pins.extend(pin for row in results for pin in row['inputs'].values())
    for pin in {pin['file']: pin for pin in all_pins}.values():
        checked(pin)
    previous_ready = plan['counts']['ready']
    ready_jobs = [job for job in jobs.values() if job['ready_for_manual_preflight']]
    plan['counts'].update(ready=len(ready_jobs), ready_IN=sum(job['difficulty'] == 'IN' for job in ready_jobs),
                          ready_AT=sum(job['difficulty'] == 'AT' for job in ready_jobs))
    output_plan = output / 'test-library.json'
    write_new(output_plan, plan)
    report = dict(schema='pas.block-area-offline-refresh.v1', inputs=pins, shared_hud=hud, plan=reference(output_plan),
                  allow_temporal_fallback=allow_temporal_fallback,
                  counts=dict(plan['counts'], previous_ready=previous_ready, targeted=len(targets),
                              targeted_ready=sum(row['ready'] for row in results),
                              untouched_rows=len(jobs) - len(targets)), rows=results,
                  device_discovery=False, input_enabled=False, gameplay_qualified=False, scope=SCOPE)
    write_new(output / 'refresh-report.json', report)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('configuration', 'executable', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--allow-temporal-fallback', action='store_true')
    arguments = parser.parse_args()
    print(json.dumps(refresh(arguments.configuration, arguments.executable, arguments.output,
                             arguments.allow_temporal_fallback)['counts']))
