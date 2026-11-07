"""Prepare every catalog row offline, skipping incompatible rows explicitly.

Uses exact installed-APK identity evidence and a real difficulty-bound AT HUD.
Never launches a game, discovers a device, runs run-adaptive, or sends input.
Per-game-run records are unaffected. Bulk offline plans are transient derived
outputs: retain profile/report/logs and plan hash/counts, not hundreds of copies.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path

from inspect_catalog_native import run_bounded
from verify_installed_catalog import reference, sha, write_new


def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def checked(ref):
    if reference(ref['file'])['sha256'] != ref['sha256'].lower():
        raise ValueError('Pinned input changed: '+ref['file'])
    return Path(ref['file']).resolve()


def source_for_legacy(job):
    profile=read(job['profile']['file']);base=Path(job['chart']['file']).parent
    candidates=[Path(job['source_manifest']['file'])] if job.get('source_manifest') else [base/'source-manifest-IN.json',base/'source-manifest.json']
    matches=[p for p in candidates if p.is_file() and sha(p)==profile['source_manifest_sha256']]
    if len(matches)!=1:raise ValueError('Unique exact existing source manifest required')
    return reference(matches[0])


def trim_derived_plan(folder, data=None):
    path=folder/'compiled-plan.json'
    if not path.exists():return
    if path.is_symlink() or path.resolve().parent!=folder.resolve():raise ValueError('Derived plan escapes new preparation output')
    if data is None:data=read(path)
    last_up=max((step['song_ns'] for contact in data['contacts'] for step in contact.get('steps',[])),default=None)
    write_new(folder/'compiled-plan-summary.json',dict(sha256=sha(path),bytes=path.stat().st_size,
        cache_sha256=data['cache_sha256'],contacts=len(data['contacts']),events=data['total_steps'],
        peak_contacts=data.get('peak_contacts'),last_up_song_ns=last_up,
        scope='Transient bulk offline plan; runtime regenerates and pins complete plan per actual run'))
    path.unlink()  # Only this newly generated derivative; never a run/original.


def checked_contact_capacity(value):
    if type(value) is not int or not 1<=value<=10:raise ValueError('Contact capacity must be an integer 1..10')
    return value


def runtime_budget(profile,last_up_song_ns=None):
    """A larger budget must be explicit in the immutable native profile."""
    steps=profile.get('runtime_step_budget',16000)
    duration=profile.get('runtime_duration_limit_ns',180_000_000_000)
    if type(steps) is not int or not 2<=steps<=65536:raise ValueError('Runtime step budget must be an integer 2..65536')
    if type(duration) is not int or not 1_000_000_000<=duration<=600_000_000_000:
        raise ValueError('Runtime duration limit must be an integer 1..600 seconds in ns')
    events,contacts=profile['expected_steps'],profile['expected_contacts']
    if type(events) is not int or not 2<=events<=steps:raise ValueError(f'Runtime event budget exceeded: {events}/{steps}')
    if type(contacts) is not int or not 1<=contacts<=events//2:raise ValueError('Runtime contact/closed-event count differs')
    peak=profile['expected_peak'];capacity=checked_contact_capacity(profile.get('contact_capacity',4))
    if type(peak) is not int or not 1<=peak<=capacity:raise ValueError('Runtime peak exceeds declared contact capacity')
    last=profile.get('last_up_song_ns',last_up_song_ns)
    if last is not None and (type(last) is not int or not 0<=last<duration):
        raise ValueError(f'Runtime duration budget exceeded: last_up={last}, limit={duration}')
    return dict(runtime_step_budget=steps,runtime_duration_limit_ns=duration,last_up_song_ns=last,
        expected_contacts=contacts,expected_steps=events,expected_peak=peak)


def compile_policy(profile):
    return dict(import_policy_id=profile.get('import_policy_id','phigros-v3-strict-v1'),
        flick_path_policy=profile.get('flick_path_policy','dynamic-line-v1'),
        hold_tail_policy_id=profile.get('hold_tail_policy_id','full-pose-v1'),
        offscreen_policy_id=profile.get('offscreen_policy_id','bounded-normal-intersection-candidate-v1'),
        max_tangent_error_reference_pixels=profile.get('max_tangent_error_reference_pixels',0),
        max_normal_projection_reference_pixels=profile.get('max_normal_projection_reference_pixels',7000),
        input_inset_pixels=profile.get('input_inset_pixels',64),drag_duration_ns=profile.get('drag_duration_ns',32_000_000),
        max_prejudge_ns=profile.get('max_prejudge_ns',0),advance_above_drags=profile.get('advance_above_drags',False),
        max_hold_reacquire_gap_ns=profile.get('max_hold_reacquire_gap_ns',0))


def checked_candidate_limits(normal=None,inset=None,drag_ms=None,prejudge_ms=0,advance_above_drags=False,hold_gap_ms=0):
    normal=7000 if normal is None else normal;inset=64 if inset is None else inset;drag_ms=32 if drag_ms is None else drag_ms
    for name,value,low,high in (('normal projection',normal,1,1_000_000),('input inset',inset,0,100),
        ('Drag duration',drag_ms,1,32),('pre-judge duration',prejudge_ms,0,30),('Hold reacquisition gap',hold_gap_ms,0,20)):
        if type(value) is not int or not low<=value<=high:raise ValueError(f'{name} must be an integer {low}..{high}')
    if type(advance_above_drags) is not bool or (advance_above_drags and prejudge_ms==0):
        raise ValueError('Advancing above Drags requires an explicit positive pre-judge window')
    return dict(max_normal_projection_reference_pixels=normal,input_inset_pixels=inset,drag_duration_ns=drag_ms*1_000_000,
        max_prejudge_ns=prejudge_ms*1_000_000,advance_above_drags=advance_above_drags,max_hold_reacquire_gap_ns=hold_gap_ms*1_000_000)


def checked_projection_policy(policy=None,tangent_error=None):
    if policy is None and tangent_error is None:return None
    if policy!='bounded-judgment-rectangle-candidate-v1' or type(tangent_error) not in (int,float) or not math.isfinite(tangent_error) or not 0<=tangent_error<=151.2:
        raise ValueError('Rectangle candidate requires an explicit finite tangent error 0..151.2 reference pixels')
    return dict(offscreen_policy_id=policy,max_tangent_error_reference_pixels=tangent_error)


def import_policy_for(raw,v1_policy=None):
    version=raw.get('formatVersion')
    if version not in (1,3):raise ValueError('格式 v'+str(version)+' 暫跳過')
    if v1_policy is not None and v1_policy not in ('phigros-v1-source-priority-constant-empty-candidate-v1','phigros-v1-source-priority-empty-interval-candidate-v1'):
        raise ValueError('Unsupported explicit native v1 import policy')
    if version==1 and v1_policy is not None:return v1_policy
    return f'phigros-v{version}-source-priority-constant-empty-candidate-v1'


def preparation_diagnostics(folder):
    report_path=folder/'report.json';summary_path=folder/'compiled-plan-summary.json'
    report=read(report_path) if report_path.is_file() else {}
    summary=read(summary_path) if summary_path.is_file() else {}
    diagnostics=report.get('diagnostics',[])
    return dict(report=reference(report_path) if report_path.is_file() else None,
        compiled=report.get('compiled'),within_runtime_bounds=report.get('within_live_experiment_bounds'),
        contacts=summary.get('contacts',report.get('source_notes')),events=summary.get('events'),
        peak=summary.get('peak_contacts'),last_up_song_ns=report.get('last_up_song_ns',summary.get('last_up_song_ns')),
        runtime_step_budget=report.get('runtime_step_budget',16000),
        runtime_duration_limit_ns=report.get('runtime_duration_limit_ns',180_000_000_000),
        error_counts=dict(Counter(d['code'] for d in diagnostics if d.get('severity')==1)),
        error_examples=[d for d in diagnostics if d.get('severity')==1][:8],
        diagnostic_count=len(diagnostics),import_anomaly_count=len(report.get('import',{}).get('anomalies',[])))


def preparation_failure(execution,details):
    if execution.get('failure'):
        return '原生準備程序 '+execution['failure']+'；完整原因保留於執行與report紀錄'
    if details['error_counts']:return ','.join(details['error_counts'])
    reasons=[]
    if details['events'] is not None and details['events']>details['runtime_step_budget']:
        reasons.append(f"事件預算超出：{details['events']}/{details['runtime_step_budget']}")
    if details['last_up_song_ns'] is not None and details['last_up_song_ns']>=details['runtime_duration_limit_ns']:
        reasons.append(f"時長預算超出：{details['last_up_song_ns']/1e9:.3f}/{details['runtime_duration_limit_ns']/1e9:g}秒")
    return '；'.join(reasons) or execution.get('stderr','').strip()[:600] or '原生準備未通過；請檢視完整report'


def matches_block_area_phase(profile, phase):
    if phase is None:return 'block_area_phase_ms' not in profile
    value=profile.get('block_area_phase_ms')
    return type(value) is int and value==phase


def cached_profile(cache,key,chart,source,policy,contact_capacity=5,import_policy=None,projection_policy=None,block_area_phase=None):
    contact_capacity=checked_contact_capacity(contact_capacity)
    if cache is None:return None
    path=(Path(cache).resolve()/'songs'/key/'prepared/song-profile.json').resolve()
    if not path.is_file() or not path.is_relative_to(Path(cache).resolve()):return None
    try:
        p=read(path)
        if not matches_block_area_phase(p,block_area_phase):return None
        runtime_budget(p)
        actual_policy=compile_policy(p)
        if projection_policy is not None and any(actual_policy[key]!=value for key,value in projection_policy.items()):return None
        if import_policy is not None:
            if actual_policy['import_policy_id']!=import_policy:return None
            pair=(actual_policy['flick_path_policy'],actual_policy['hold_tail_policy_id'])
            if pair not in (('dynamic-line-v1','full-pose-v1'),('judgment-anchored-straight-v1','full-pose-v1'),
                            ('judgment-anchored-screen-fit-v1','phira-tail-grace-candidate-v1')):return None
        if (p['song_id']==chart['track_id'] and p['difficulty']==chart['difficulty'] and
            p['chart_sha256']==chart['chart']['sha256'] and p['source_manifest_sha256']==sha(source) and
            p['hud_policy']==policy and p.get('contact_capacity',4)==contact_capacity and
            p['expected_contacts']==chart['notes']):return path
    except (ValueError,KeyError,TypeError):pass
    return None


def prepare(library,identity,legacy,at_state,at_reference,executable,output,profile_cache=None,contact_capacity=5,image_retention='none',capacity_policy=None,offscreen_policy=None,max_tangent_error_pixels=None,v1_import_policy=None,normal_projection_pixels=None,input_inset_pixels=None,drag_duration_ms=None,max_prejudge_ms=0,advance_above_drags=False,max_hold_gap_ms=0):
    contact_capacity=checked_contact_capacity(contact_capacity)
    projection_policy=checked_projection_policy(offscreen_policy,max_tangent_error_pixels)
    requested_limits=checked_candidate_limits(normal_projection_pixels,input_inset_pixels,drag_duration_ms,max_prejudge_ms,advance_above_drags,max_hold_gap_ms)
    if max_hold_gap_ms and projection_policy is None:raise ValueError('Hold reacquisition gap requires explicit rectangle and Hold candidate policies')
    requested_policy=dict(requested_limits,**(projection_policy or {}))
    custom_limits=any(value is not None for value in (normal_projection_pixels,input_inset_pixels,drag_duration_ms)) or max_prejudge_ms!=0 or advance_above_drags or max_hold_gap_ms!=0
    if image_retention not in ('none','result'):raise ValueError('Manual preparation allows only none or result image retention')
    if (at_state is None)!=(at_reference is None):raise ValueError('AT state and reference must be supplied together')
    library,identity,legacy,executable,output=map(lambda p:Path(p).resolve(),(library,identity,legacy,executable,output))
    if at_state is not None:at_state,at_reference=map(lambda p:Path(p).resolve(),(at_state,at_reference))
    if output.exists():raise FileExistsError('Output must be new')
    catalog=read(library);proof=read(identity);config=read(legacy)
    capacity_policy=capacity_policy or config.get('capacity_policy','current-boot-fixture-v1')
    if capacity_policy not in ('current-boot-fixture-v1','configured-ten-contact-v1'):raise ValueError('Unsupported capacity policy')
    if capacity_policy=='configured-ten-contact-v1' and contact_capacity!=10:raise ValueError('Configured ten-contact policy requires capacity 10')
    if proof['library']!=reference(library):raise ValueError('Identity proof/library differ')
    old=read(checked(config['library']));old_jobs={(j['track_id'],j['difficulty']):j for j in old['songs']}
    identities={j['library_id']:j for j in proof['charts']}
    if set(identities)!={j['library_id'] for j in catalog['charts']}:raise ValueError('Identity proof is incomplete or duplicated')
    at_inputs=() if at_state is None else (at_state,at_reference)
    required_pins=[config['kernel_profile'],*map(reference,at_inputs)]
    if capacity_policy!='configured-ten-contact-v1':required_pins.append(config['capacity_evidence'])
    for pin in required_pins:checked(pin)
    pins={str(p):sha(p) for p in (library,identity,legacy,*at_inputs,executable)}
    output.mkdir(parents=True);(output/'songs').mkdir()
    rows={};progress=[]
    # Diagnose the actual AT reference first; final list remains catalog order.
    charts=sorted(catalog['charts'],key=lambda c:(c['track_id']!='Credits.Frums.0' or c['difficulty']!='AT',c['library_id']))
    def prepare_one(chart):
        chart_path=(library.parent/chart['chart']['file']).resolve();checked(dict(file=str(chart_path),sha256=chart['chart']['sha256']))
        job=dict(library_id=chart['library_id'],track_id=chart['track_id'],display_name=chart['title'],difficulty=chart['difficulty'],
            variant=chart['variant'],role='catalog',rounds=1,default_selected=False,chart=reference(chart_path),
            ready_for_manual_preflight=False,blockers=[],notes=chart['notes'],events=0,peak=0,
            profile=None,source_manifest=None,state_calibration=None,hud_reference=None,compiled_fingerprint=None,
            contact_capacity=contact_capacity,capacity_policy=capacity_policy,runtime_budget=None,preparation=None,
            compile_policy=None,preparation_strategy=None,
            source_kind='public-exact-installed',source_public_id=chart.get('upstream_library_id',chart['library_id']),
            source_public_chart=chart.get('public_chart'),source_public_match=chart.get('public_match',True),
            gameplay_qualified=False,result=None,live_status='NOT RUN')
        if not job['source_public_match']:job['source_kind']='installed-local-variant'
        folder=output/'songs'/hashlib.sha256(chart['library_id'].encode()).hexdigest()[:24]
        folder.mkdir()
        try:
            raw=read(chart_path)
            import_policy=import_policy_for(raw,v1_import_policy)
            block_areas=raw.get('blockAreaList')
            block_area_phase=-80 if isinstance(block_areas,list) and block_areas else None
            item=identities[chart['library_id']]
            if item['status']!='verified_exact_installed_chart':raise ValueError('安裝版原譜與公共版本不同，暫跳過；保留兩份身份')
            source=checked(item['source_manifest']);job['source_manifest']=reference(source)
            if chart['difficulty']=='AT' and at_state is None:
                raise ValueError('AT 尚待真實開場HUD與獨立難度校正，準備完成前目前無法執行')
            previous=old_jobs.get((chart['track_id'],chart['difficulty']))
            previous_profile=None
            if (previous and previous.get('profile') and previous['chart']['sha256']==chart['chart']['sha256'] and
                projection_policy is None and not custom_limits and (raw['formatVersion']!=1 or v1_import_policy is None)):
                previous_profile=read(checked(previous['profile']))
            if (previous_profile and previous_profile.get('contact_capacity',4)==contact_capacity and
                matches_block_area_phase(previous_profile,block_area_phase)):
                for key in ('chart','profile','state_calibration','hud_reference','compiled_fingerprint','notes','events','peak'):
                    job[key]=previous[key]
                job['source_manifest']=source_for_legacy(previous)
                job['preparation_strategy']='retained-exact-profile'
                if '直線' in previous['display_name']:job['display_name']=previous['display_name']
            else:
                if chart['difficulty']=='AT':state,reference_path,policy=at_state,at_reference,'shared-difficulty-subpixel-prefix-score-v4'
                else:
                    baseline=next(j for j in old['songs'] if j['track_id']=='Glaciaxion.SunsetRay.0')
                    state,reference_path,policy=Path(baseline['state_calibration']['file']),Path(baseline['hud_reference']['file']),'shared-in-subpixel-prefix-score-v3'
                destination=folder/'prepared'
                cached=cached_profile(profile_cache,folder.name,chart,source,policy,contact_capacity,import_policy,requested_policy,block_area_phase)
                if cached:
                    destination=cached.parent;write_new(folder/'reused-offline-profile.json',reference(cached))
                    job['preparation_strategy']='retained-exact-profile'
                else:
                    args=[str(executable),'prepare-adaptive',str(chart_path),str(source),str(destination),
                        '--normal-projection-pixels',str(requested_limits['max_normal_projection_reference_pixels']),
                        '--drag-duration-ms',str(requested_limits['drag_duration_ns']//1_000_000),
                        '--input-inset-pixels',str(requested_limits['input_inset_pixels']),
                        '--contact-capacity',str(contact_capacity),'--hud-policy',policy,'--import-policy',import_policy,
                        '--tight-projection-speed-bounds','1','--flick-lead-ms','30','--flick-travel-ms','60',
                        '--flick-distance-pixels','60','--flick-release-ms','24',
                        '--flick-path-policy','judgment-anchored-screen-fit-v1',
                        '--hold-tail-policy','phira-tail-grace-candidate-v1']
                    if block_area_phase is not None:args+=['--block-area-phase-ms',str(block_area_phase)]
                    job['preparation_strategy']='screen-fit-phira-tail-candidate'
                    if projection_policy is not None:
                        args+=['--offscreen-policy',offscreen_policy,'--max-tangent-error-pixels',str(max_tangent_error_pixels)]
                        job['preparation_strategy']='bounded-judgment-rectangle-phira-tail-candidate'
                    if max_prejudge_ms:
                        args+=['--max-prejudge-ms',str(max_prejudge_ms),'--advance-above-drags',str(int(advance_above_drags))]
                        job['preparation_strategy']+='-prejudge-candidate'
                    if max_hold_gap_ms:
                        args+=['--max-hold-gap-ms',str(max_hold_gap_ms)]
                        job['preparation_strategy']+='-bounded-hold-gap-candidate'
                    execution=run_bounded(args,60,folder,output_limit=2*1024*1024)
                    write_new(folder/'prepare-execution.json',dict(argv=args,**execution))
                    trim_derived_plan(destination)
                    job['preparation']=preparation_diagnostics(destination)
                    if execution['returncode'] or execution['failure']:
                        raise ValueError(preparation_failure(execution,job['preparation']))
                profile=read(destination/'song-profile.json')
                job.update(profile=reference(destination/'song-profile.json'),state_calibration=reference(state),hud_reference=reference(reference_path),
                    compiled_fingerprint=profile['expected_plan_sha256'],notes=profile['expected_contacts'],events=profile['expected_steps'],peak=profile['expected_peak'])
            profile=read(job['profile']['file'])
            if not matches_block_area_phase(profile,block_area_phase):
                raise ValueError('Prepared block area phase differs from the -80 ms preflight phase')
            job['compile_policy']=compile_policy(profile)
            if profile['expected_contacts']!=chart['notes']:
                raise ValueError('Native preparation must retain every source note exactly once')
            if job['compile_policy']['import_policy_id']!=import_policy:
                raise ValueError('Prepared import policy differs from actual native chart format')
            if job['preparation_strategy']!='retained-exact-profile' and (
                job['compile_policy']['flick_path_policy']!='judgment-anchored-screen-fit-v1' or
                job['compile_policy']['hold_tail_policy_id']!='phira-tail-grace-candidate-v1'):
                raise ValueError('Native preparation did not retain explicit screen-fit/Hold-tail candidate policies')
            if any(job['compile_policy'][key]!=value for key,value in requested_policy.items()):
                raise ValueError('Native preparation did not retain explicit geometry, Drag or pre-judge candidate parameters')
            if profile.get('contact_capacity',4)!=contact_capacity:
                raise ValueError('Prepared profile capacity differs from explicit offline request')
            job['runtime_budget']=runtime_budget(profile)
            args=[str(executable),'check-adaptive','emulator-5554','phigros',job['chart']['file'],job['state_calibration']['file'],
                config['kernel_profile']['file'],str(folder/'offline-check'),'-80','--experimental-whole-song','--song-profile',job['profile']['file'],
                '--experimental-hud-reference',job['hud_reference']['file']]
            if capacity_policy=='configured-ten-contact-v1':args+=['--capacity-policy',capacity_policy]
            args+=['--image-retention',image_retention]
            execution=run_bounded(args,60,folder,output_limit=2*1024*1024)
            write_new(folder/'preflight-execution.json',dict(argv=args,**execution))
            if execution['returncode'] or execution['failure']:raise ValueError(execution['stderr'].strip()[:600] or '前檢失敗')
            response=json.loads(execution['stdout'])
            if response['input_enabled'] is not False or response['image_retention']!=image_retention:raise ValueError('前檢／影像政策不相同')
            if capacity_policy=='configured-ten-contact-v1' and response.get('capacity_policy')!=capacity_policy:
                raise ValueError('前檢未使用明確設定的十指模式')
            job['runtime_budget']=runtime_budget(profile,response.get('last_up_song_ns'))
            job['ready_for_manual_preflight']=True
            job['default_selected']=chart['track_id']=='Glaciaxion.SunsetRay.0' and chart['difficulty']=='IN'
        except (ValueError,KeyError,TypeError,OSError,RuntimeError) as error:
            job['blockers']=[str(error)];job['default_selected']=False
        rows[chart['library_id']]=job
        row_status=dict(library_id=job['library_id'],ready=job['ready_for_manual_preflight'],blockers=job['blockers'])
        progress.append(row_status);write_new(folder/'status.json',row_status)
        print(json.dumps(dict(done=len(rows),track=chart['track_id'],difficulty=chart['difficulty'],ready=job['ready_for_manual_preflight'],reason=job['blockers']),ensure_ascii=False),flush=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(prepare_one,charts))
    if any(sha(path)!=expected for path,expected in pins.items()):raise ValueError('Preparation input changed')
    jobs=[rows[c['library_id']] for c in catalog['charts']]
    plan=dict(schema='pas.catalog-test-plan.v1',catalog=reference(library),identity=reference(identity),round_limit=len(jobs),
        phase_ms=-80,image_retention=image_retention,contact_capacity=contact_capacity,capacity_policy=capacity_policy,
        capacity_scope=('Configured ten-contact desktop mode; no repeated current-boot fixture is requested or claimed' if capacity_policy=='configured-ten-contact-v1' else 'Legacy current-boot fixture evidence policy'),jobs=jobs,
        counts=dict(songs=catalog['counts']['song_count'],charts=len(jobs),ready=sum(j['ready_for_manual_preflight'] for j in jobs),
            ready_IN=sum(j['ready_for_manual_preflight'] and j['difficulty']=='IN' for j in jobs),
            ready_AT=sum(j['ready_for_manual_preflight'] and j['difficulty']=='AT' for j in jobs)),
        device_access=False,input_enabled=False,gameplay_qualified=False)
    write_new(output/'test-library.json',plan)
    return plan


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('library','identity','legacy','executable','output'):p.add_argument('--'+key,type=Path,required=True)
    for key in ('at-state','at-reference'):p.add_argument('--'+key,type=Path)
    p.add_argument('--profile-cache',type=Path)
    p.add_argument('--contact-capacity',type=int,choices=range(1,11),default=5,
        help='Explicit offline candidate capacity; does not qualify actual device capacity (default: 5)')
    p.add_argument('--image-retention',choices=('none','result'),default='none')
    p.add_argument('--capacity-policy',choices=('current-boot-fixture-v1','configured-ten-contact-v1'))
    p.add_argument('--offscreen-policy',choices=('bounded-judgment-rectangle-candidate-v1',))
    p.add_argument('--max-tangent-error-pixels',type=float)
    p.add_argument('--v1-import-policy',choices=('phigros-v1-source-priority-constant-empty-candidate-v1','phigros-v1-source-priority-empty-interval-candidate-v1'))
    p.add_argument('--normal-projection-pixels',type=int)
    p.add_argument('--input-inset-pixels',type=int)
    p.add_argument('--drag-duration-ms',type=int)
    p.add_argument('--max-prejudge-ms',type=int,default=0)
    p.add_argument('--advance-above-drags',type=int,choices=(0,1),default=0)
    p.add_argument('--max-hold-gap-ms',type=int,default=0)
    a=p.parse_args();result=prepare(a.library,a.identity,a.legacy,a.at_state,a.at_reference,a.executable,a.output,a.profile_cache,a.contact_capacity,a.image_retention,a.capacity_policy,a.offscreen_policy,a.max_tangent_error_pixels,a.v1_import_policy,a.normal_projection_pixels,a.input_inset_pixels,a.drag_duration_ms,a.max_prejudge_ms,bool(a.advance_above_drags),a.max_hold_gap_ms)
    print(json.dumps(result['counts']))
