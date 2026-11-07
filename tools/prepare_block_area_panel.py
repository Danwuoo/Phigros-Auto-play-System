"""Refresh only previously blocked blockAreaList jobs in a pinned desktop plan offline."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re

from prepare_catalog_panel import (checked, compile_policy, import_policy_for, matches_block_area_phase,
                                   preparation_diagnostics, preparation_failure, read,
                                   runtime_budget, trim_derived_plan)
from inspect_catalog_native import run_bounded
from verify_installed_catalog import reference, write_new


CAPACITY_POLICY = 'configured-ten-contact-v1'
CATALOG_BLOCKER = 'unsupported_block_area'
STRATEGY = 'block-area-offline-detour-candidate'
SCOPE = 'offline artifact/config validation; current boot, foreground, contacts and game judgment NOT RUN'
OFFSCREEN_HOLD_IDS = 'block_area_offscreen_hold_source_ids'
HOLD_HANDOFFS = 'allow_block_area_hold_handoffs'
HOLD_HANDOFF_OVERLAP_NS = 24_000_000
MAX_HOLD_HANDOFFS_PER_SOURCE = 16
SOURCE_NOTE_ID = re.compile(r'[0-9a-f]{64}#/judgeLineList/[0-9]{1,9}/notes(?:Above|Below)/[0-9]{1,9}')
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


def unique_object(pairs):
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError('Song overrides contain a duplicate JSON key')
    return result


def read_song_overrides(path, targets):
    path = Path(path).resolve(strict=True)
    with path.open('rb') as stream:
        raw = stream.read(65537)
    if len(raw) > 65536:
        raise ValueError('Song override file must be at most 64 KiB')
    overrides = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=unique_object)
    if not isinstance(overrides, dict) or not overrides or not overrides.keys() <= targets:
        raise ValueError('Song overrides must be keyed by currently blocked blockAreaList library IDs')
    for library_id, values in overrides.items():
        if (not isinstance(values, dict) or not values or
                not values.keys() <= {'phase_ms', 'drag_duration_ms', OFFSCREEN_HOLD_IDS, HOLD_HANDOFFS}):
            raise ValueError(f'Song override {library_id} has unsupported options')
        if 'phase_ms' in values and (type(values['phase_ms']) is not int or not -100 <= values['phase_ms'] <= 100):
            raise ValueError(f'Song override {library_id} phase_ms must be an integer between -100 and 100 ms')
        if 'drag_duration_ms' in values and (type(values['drag_duration_ms']) is not int or not 1 <= values['drag_duration_ms'] <= 32):
            raise ValueError(f'Song override {library_id} drag_duration_ms must be an integer between 1 and 32 ms')
        if HOLD_HANDOFFS in values and values[HOLD_HANDOFFS] is not True:
            raise ValueError(f'Song override {library_id} {HOLD_HANDOFFS} must be true')
        if OFFSCREEN_HOLD_IDS in values:
            source_ids = values[OFFSCREEN_HOLD_IDS]
            if (not isinstance(source_ids, list) or not 1 <= len(source_ids) <= 32 or
                    not all(isinstance(source_id, str) and SOURCE_NOTE_ID.fullmatch(source_id) for source_id in source_ids) or
                    len(set(source_ids)) != len(source_ids)):
                raise ValueError(f'Song override {library_id} must name 1 to 32 unique exact Hold source IDs')
            values[OFFSCREEN_HOLD_IDS] = sorted(source_ids)
    return overrides, dict(file=str(path), sha256=hashlib.sha256(raw).hexdigest())


def contact_lifetime(contact):
    steps = contact['steps']
    times = [step['song_ns'] for step in steps]
    phases = [step['phase'] for step in steps]
    if (len(steps) < 2 or phases[0] != 0 or phases[-1] != 2 or any(phase != 1 for phase in phases[1:-1]) or
            any(type(phase) is not int for phase in phases) or any(type(time) is not int for time in times) or
            any(right <= left for left, right in zip(times, times[1:]))):
        raise ValueError('Hold handoff contact must have a strictly ordered DOWN/MOVE/UP lifetime')
    return times[0], times[-1]


def validate_contact_ownership(plan, raw, source_notes, physical_contacts, allow_handoffs):
    owners = plan['ownership']
    contacts = {contact['id']: contact for contact in plan['contacts']}
    if (len(contacts) != len(plan['contacts']) or len(contacts) != physical_contacts or
            len(owners) != len(source_notes) or {owner['source_note_id'] for owner in owners} != source_notes.keys()):
        raise ValueError('Plan must retain a unique owner for every original source note and all physical contacts')
    assigned = set()
    handoff_contacts = 0
    for owner in owners:
        extras = owner.get('hold_handoff_contact_ids', [])
        if not isinstance(extras, list) or len(extras) > MAX_HOLD_HANDOFFS_PER_SOURCE:
            raise ValueError('Plan Hold handoff contacts must be an ordered list of at most 16 IDs per source')
        group = [owner['contact_id'], *extras]
        for contact_id in group:
            if type(contact_id) is not int or contact_id not in contacts or contact_id in assigned:
                raise ValueError('Plan must assign every physical contact to exactly one source note')
            assigned.add(contact_id)
        if not extras:
            continue
        note, bpm = source_notes[owner['source_note_id']]
        if (not allow_handoffs or type(note['type']) is not int or note['type'] != 3 or
                type(owner['kind']) is not int or owner['kind'] != 3 or
                owner.get('block_area_temporal_fallback') is not True):
            raise ValueError('Only an enabled temporal BlockArea Hold can own handoff contacts')
        windows = [contact_lifetime(contacts[contact_id]) for contact_id in group]
        source_head_ns = (float(note['time']) * 1.875 / bpm + raw['offset']) * 1_000_000_000
        source_tail_ns = ((float(note['time']) + float(note['holdTime'])) * 1.875 / bpm + raw['offset']) * 1_000_000_000
        if (type(owner['judgment_ns']) is not int or windows[0][0] != owner['judgment_ns'] or
                not abs(windows[0][0] - source_head_ns) <= 1 or
                not abs(windows[-1][1] - source_tail_ns) <= 1):
            raise ValueError('Plan Hold handoff must preserve the original chart DOWN and UP times')
        for index, (down, up) in enumerate(windows[1:], 1):
            previous_down, previous_up = windows[index - 1]
            if (down <= previous_down or up <= previous_up or previous_up - down != HOLD_HANDOFF_OVERLAP_NS or
                    (index >= 2 and down < windows[index - 2][1])):
                raise ValueError('Plan Hold handoff must maintain ordered two-contact overlap of 24 ms')
        handoff_contacts += len(extras)
    if assigned != contacts.keys():
        raise ValueError('Plan must assign every physical contact to exactly one source note')
    return owners, handoff_contacts


def prepare_job(job, catalog, identity, hud, pins, output, phase, allow_temporal_fallback=False, override=None):
    folder = output / 'songs' / hashlib.sha256(job['library_id'].encode('utf-8')).hexdigest()[:24]
    folder.mkdir()
    prepared = folder / 'prepared'
    result = dict(library_id=job['library_id'], track_id=job['track_id'], song=job['display_name'],
                  difficulty=job['difficulty'], notes=job['notes'], ready=False, reason=None,
                  catalog_blocker=CATALOG_BLOCKER, declared_regions=None)
    override = override or {}
    offscreen_holds = override.get(OFFSCREEN_HOLD_IDS, [])
    hold_handoffs = override.get(HOLD_HANDOFFS, False)
    drag_ms = override.get('drag_duration_ms', PROJECTION['drag_duration_ns'] // 1_000_000)
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
        if override and not area_count:
            raise ValueError('Song overrides require a nonempty original blockAreaList')
        projection = dict(PROJECTION, input_inset_pixels=12 if area_count else 64, drag_duration_ns=drag_ms * 1_000_000)
        temporal = bool(area_count and allow_temporal_fallback)
        import_policy = import_policy_for(raw)
        # Prove that the derived plan contains an owner for every raw source note.
        source_notes = {f'{digest}#/judgeLineList/{i}/{side}/{j}': (note, line['bpm'])
                        for i, line in enumerate(raw['judgeLineList'])
                        for side in ('notesAbove', 'notesBelow') for j, note in enumerate(line[side])}
        if len(source_notes) != job['notes']:
            raise ValueError('Original note count differs from the pinned desktop row')
        if any(source_id not in source_notes or type(source_notes[source_id][0]['type']) is not int or
               source_notes[source_id][0]['type'] != 3 for source_id in offscreen_holds):
            raise ValueError('Offscreen Hold overrides must match Hold notes in the exact original chart')
        args = [pins['executable']['file'], 'prepare-adaptive', str(chart), str(source_path), str(prepared),
                '--normal-projection-pixels', '7000', '--drag-duration-ms', str(drag_ms),
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
        if offscreen_holds:
            args += ['--block-area-offscreen-hold-source-ids', json.dumps(offscreen_holds, separators=(',', ':'))]
        if hold_handoffs:
            args += ['--allow-block-area-hold-handoffs', '1']
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
                                     source_manifest_sha256=row_pins['source_manifest']['sha256'],
                                     import_policy_id=import_policy, hud_policy=HUD_POLICIES[diff], contact_capacity=10), 'Profile')
        budget = runtime_budget(profile)
        physical_contacts = budget['expected_contacts']
        extra_contacts = physical_contacts - job['notes']
        if extra_contacts < 0 or (extra_contacts and not hold_handoffs):
            raise ValueError('Profile physical contact count differs from the explicit Hold handoff policy')
        require_fields({'source_notes': profile.get('expected_source_notes', None if extra_contacts else job['notes'])},
                       {'source_notes': job['notes']}, 'Profile')
        if not matches_block_area_phase(profile, phase if area_count else None):
            raise ValueError('Profile block area phase differs from the effective desktop phase')
        if profile.get('allow_block_area_temporal_fallback', False) is not temporal:
            raise ValueError('Profile block area temporal fallback differs from the explicit offline request')
        if profile.get(OFFSCREEN_HOLD_IDS, []) != offscreen_holds:
            raise ValueError('Profile offscreen Hold fallback differs from the explicit offline request')
        if profile.get(HOLD_HANDOFFS, False) is not hold_handoffs:
            raise ValueError('Profile Hold handoff policy differs from the explicit offline request')
        plan = read(prepared / 'compiled-plan.json')
        try:
            require_fields(plan, dict(chart_sha256=digest, cache_sha256=profile['expected_plan_sha256'],
                                      total_steps=profile['expected_steps'], peak_contacts=profile['expected_peak'],
                                      drag_duration_ns=projection['drag_duration_ns']), 'Plan')
            owners, handoff_contacts = validate_contact_ownership(plan, raw, source_notes, physical_contacts, hold_handoffs)
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
            temporal_owners = {owner['source_note_id'] for owner in owners
                               if owner.get('block_area_temporal_fallback') is True}
            if len(temporal_owners) != temporal_contacts:
                raise ValueError('Prepare temporal fallback count differs from the source note ownership')
            hold_contacts = block.get('offscreen_hold_fallback_contacts', 0)
            used_offscreen_holds = {owner['source_note_id'] for owner in owners
                                   if owner.get('block_area_offscreen_hold_fallback') is True}
            if (type(hold_contacts) is not int or hold_contacts != len(offscreen_holds) or
                    used_offscreen_holds != set(offscreen_holds) or not used_offscreen_holds <= temporal_owners):
                raise ValueError('Prepare offscreen Hold fallback evidence differs from the explicit Hold source IDs')
            require_fields({'hold_handoff_contacts': block.get('hold_handoff_contacts', 0)},
                           {'hold_handoff_contacts': extra_contacts}, 'Prepare block area')
            if hold_handoffs:
                require_fields(block, {'hold_handoff_overlap_ns': HOLD_HANDOFF_OVERLAP_NS}, 'Prepare block area')
            if handoff_contacts != extra_contacts:
                raise ValueError('Prepare Hold handoff count differs from source note ownership')
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
                                      compiled_sha256=profile['expected_plan_sha256'], contacts=physical_contacts,
                                      events=budget['expected_steps'], peak=budget['expected_peak'], contact_capacity=10,
                                      runtime_step_budget=budget['runtime_step_budget'], runtime_duration_limit_ns=budget['runtime_duration_limit_ns'],
                                      last_up_song_ns=budget['last_up_song_ns'], capacity_policy=CAPACITY_POLICY,
                                      capacity_evidence_required_for_live=False, capacity_test_performed=False,
                                      image_retention='result', HUD_policy=HUD_POLICIES[diff], flick_parameters=FLICK,
                                      screen_calibration_sha256=row_pins['state_calibration']['sha256'],
                                      HUD_reference_sha256=row_pins['hud_reference']['sha256'], kernel_profile_sha256=pins['kernel_profile']['sha256'],
                                      **{key: PROJECTION[key] for key in ('hold_tail_policy_id', 'offscreen_policy_id', 'max_tangent_error_reference_pixels')}),
                       'Preflight')
        require_fields({'source_notes': response.get('source_notes', None if extra_contacts else job['notes'])},
                       {'source_notes': job['notes']}, 'Preflight')
        require_fields(response['import'], dict(policy_id=import_policy, source_chart_sha256=digest), 'Preflight import')
        if response.get('block_area') != block or (folder / 'offline-check').exists():
            raise ValueError('Preflight block area proof or offline-only output differs')
        if block is not None:
            details['block_area'] = response['block_area']
        profile_pin = reference(profile_path)
        policy = dict(compile_policy(profile), **FLICK)
        if area_count:
            policy['block_area_phase_ms'] = phase
        if temporal:
            policy['allow_block_area_temporal_fallback'] = True
        if offscreen_holds:
            policy[OFFSCREEN_HOLD_IDS] = offscreen_holds
        if hold_handoffs:
            policy[HOLD_HANDOFFS] = True
        job.update(profile=profile_pin, source_manifest=row_pins['source_manifest'],
                   state_calibration=row_pins['state_calibration'], hud_reference=row_pins['hud_reference'],
                   compiled_fingerprint=profile['expected_plan_sha256'], ready_for_manual_preflight=True,
                   blockers=[], events=budget['expected_steps'], peak=budget['expected_peak'], runtime_budget=budget,
                   compile_policy=policy, preparation_strategy=STRATEGY)
        if 'phase_ms' in override:
            job['phase_ms'] = phase
        if extra_contacts:
            job['physical_contacts'] = physical_contacts
        result.update(ready=True, block_area=block, contacts=physical_contacts, events=budget['expected_steps'],
                      peak=budget['expected_peak'], compiled_fingerprint=profile['expected_plan_sha256'])
        if 'drag_duration_ms' in override:
            result['drag_duration_ms'] = drag_ms
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


def refresh(configuration, executable, output, allow_temporal_fallback=False, song_overrides=None):
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
    overrides = {}
    if song_overrides is not None:
        overrides, pins['song_overrides'] = read_song_overrides(song_overrides, {job['library_id'] for job in targets})
        if not allow_temporal_fallback and any({OFFSCREEN_HOLD_IDS, HOLD_HANDOFFS} & values.keys() for values in overrides.values()):
            raise ValueError('Offscreen Hold and Hold handoff overrides require --allow-temporal-fallback')
    phases = {}
    for job in targets:
        library_id = job['library_id']
        phase = overrides.get(library_id, {}).get('phase_ms', job.get('phase_ms', plan['phase_ms']))
        if type(phase) is not int or not -100 <= phase <= 100:
            raise ValueError('Effective desktop phase must be an integer between -100 and 100 ms')
        phases[library_id] = phase
    output = Path(output).resolve()
    output.mkdir(parents=True)
    (output / 'songs').mkdir()
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = [pool.submit(prepare_job, job, charts[job['library_id']], identities[job['library_id']],
                               hud[job['difficulty']], pins, output, phases[job['library_id']],
                               allow_temporal_fallback, overrides.get(job['library_id'])) for job in targets]
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
    if overrides:
        report['song_overrides'] = overrides
    write_new(output / 'refresh-report.json', report)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('configuration', 'executable', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--allow-temporal-fallback', action='store_true')
    parser.add_argument('--song-overrides', type=Path,
                        help=f'JSON keyed by blocked library IDs with phase_ms, drag_duration_ms, {OFFSCREEN_HOLD_IDS}, or {HOLD_HANDOFFS}')
    arguments = parser.parse_args()
    print(json.dumps(refresh(arguments.configuration, arguments.executable, arguments.output,
                             arguments.allow_temporal_fallback, arguments.song_overrides)['counts']))
