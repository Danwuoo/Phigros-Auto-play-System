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


def trim_derived_plan(folder):
    path=folder/'compiled-plan.json'
    if not path.exists():return
    if path.is_symlink() or path.resolve().parent!=folder.resolve():raise ValueError('Derived plan escapes new preparation output')
    data=read(path)
    write_new(folder/'compiled-plan-summary.json',dict(sha256=sha(path),bytes=path.stat().st_size,
        cache_sha256=data['cache_sha256'],contacts=len(data['contacts']),events=data['total_steps'],
        scope='Transient bulk offline plan; runtime regenerates and pins complete plan per actual run'))
    path.unlink()  # Only this newly generated derivative; never a run/original.


def cached_profile(cache,key,chart,source,policy):
    if cache is None:return None
    path=(Path(cache).resolve()/'songs'/key/'prepared/song-profile.json').resolve()
    if not path.is_file() or not path.is_relative_to(Path(cache).resolve()):return None
    try:
        p=read(path)
        if (p['song_id']==chart['track_id'] and p['difficulty']==chart['difficulty'] and
            p['chart_sha256']==chart['chart']['sha256'] and p['source_manifest_sha256']==sha(source) and
            p['hud_policy']==policy and p['expected_contacts']==chart['notes'] and 2<=p['expected_steps']<=16000):return path
    except (ValueError,KeyError,TypeError):pass
    return None


def prepare(library,identity,legacy,at_state,at_reference,executable,output,profile_cache=None):
    library,identity,legacy,at_state,at_reference,executable,output=map(lambda p:Path(p).resolve(),
        (library,identity,legacy,at_state,at_reference,executable,output))
    if output.exists():raise FileExistsError('Output must be new')
    catalog=read(library);proof=read(identity);config=read(legacy)
    if proof['library']!=reference(library):raise ValueError('Identity proof/library differ')
    old=read(checked(config['library']));old_jobs={(j['track_id'],j['difficulty']):j for j in old['songs']}
    identities={j['library_id']:j for j in proof['charts']}
    if set(identities)!={j['library_id'] for j in catalog['charts']}:raise ValueError('Identity proof is incomplete or duplicated')
    for pin in [config['kernel_profile'],config['capacity_evidence'],*map(reference,(at_state,at_reference))]:checked(pin)
    pins={str(p):sha(p) for p in (library,identity,legacy,at_state,at_reference,executable)}
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
            gameplay_qualified=False,result=None,live_status='NOT RUN')
        folder=output/'songs'/hashlib.sha256(chart['library_id'].encode()).hexdigest()[:24]
        folder.mkdir()
        try:
            item=identities[chart['library_id']]
            if item['status']!='verified_exact_installed_chart':raise ValueError('安裝版原譜與公共版本不同，暫跳過；保留兩份身份')
            source=checked(item['source_manifest']);job['source_manifest']=reference(source)
            previous=old_jobs.get((chart['track_id'],chart['difficulty']))
            if previous and previous['chart']['sha256']==chart['chart']['sha256']:
                for key in ('chart','profile','state_calibration','hud_reference','compiled_fingerprint','notes','events','peak'):
                    job[key]=previous[key]
                job['source_manifest']=source_for_legacy(previous)
                if '直線' in previous['display_name']:job['display_name']=previous['display_name']
            else:
                raw=read(chart_path)
                if raw.get('formatVersion')!=3:raise ValueError('格式 v'+str(raw.get('formatVersion'))+' 暫跳過')
                if raw.get('blockAreaList'):raise ValueError('blockAreaList 機制尚未支援，暫跳過')
                if chart['difficulty']=='AT':state,reference_path,policy=at_state,at_reference,'shared-difficulty-subpixel-prefix-score-v4'
                else:
                    baseline=next(j for j in old['songs'] if j['track_id']=='Glaciaxion.SunsetRay.0')
                    state,reference_path,policy=Path(baseline['state_calibration']['file']),Path(baseline['hud_reference']['file']),'shared-in-subpixel-prefix-score-v3'
                destination=folder/'prepared'
                cached=cached_profile(profile_cache,folder.name,chart,source,policy)
                if cached:
                    destination=cached.parent;write_new(folder/'reused-offline-profile.json',reference(cached))
                else:
                    args=[str(executable),'prepare-adaptive',str(chart_path),str(source),str(destination),
                        '--normal-projection-pixels','7000','--drag-duration-ms','32','--input-inset-pixels','64',
                        '--contact-capacity','5','--hud-policy',policy,'--import-policy','phigros-v3-source-priority-constant-empty-candidate-v1',
                        '--tight-projection-speed-bounds','1','--flick-lead-ms','30','--flick-travel-ms','60',
                        '--flick-distance-pixels','60','--flick-release-ms','24']
                    execution=run_bounded(args,60,folder,output_limit=2*1024*1024)
                    write_new(folder/'prepare-execution.json',dict(argv=args,**execution))
                    trim_derived_plan(destination)
                    if execution['returncode'] or execution['failure']:
                        reason=execution['stderr'].strip()
                        if (destination/'report.json').is_file():
                            report=read(destination/'report.json')
                            errors=[d['code'] for d in report['diagnostics'] if d['severity']==1]
                            reason=','.join(dict.fromkeys(errors)) or '完整計畫超出事件／時間／容量或首音模型預算'
                        raise ValueError(reason[:600] or '原生準備未通過')
                profile=read(destination/'song-profile.json')
                job.update(profile=reference(destination/'song-profile.json'),state_calibration=reference(state),hud_reference=reference(reference_path),
                    compiled_fingerprint=profile['expected_plan_sha256'],notes=profile['expected_contacts'],events=profile['expected_steps'],peak=profile['expected_peak'])
            args=[str(executable),'check-adaptive','emulator-5554','phigros',job['chart']['file'],job['state_calibration']['file'],
                config['kernel_profile']['file'],str(folder/'offline-check'),'-80','--experimental-whole-song','--song-profile',job['profile']['file'],
                '--experimental-hud-reference',job['hud_reference']['file'],'--capacity-evidence',config['capacity_evidence']['file']]
            execution=run_bounded(args,60,folder,output_limit=2*1024*1024)
            write_new(folder/'preflight-execution.json',dict(argv=args,**execution))
            if execution['returncode'] or execution['failure']:raise ValueError(execution['stderr'].strip()[:600] or '前檢失敗')
            response=json.loads(execution['stdout'])
            if response['input_enabled'] is not False or response['image_retention']!='none':raise ValueError('前檢／影像政策不相同')
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
        phase_ms=-80,image_retention='none',jobs=jobs,
        counts=dict(songs=catalog['counts']['song_count'],charts=len(jobs),ready=sum(j['ready_for_manual_preflight'] for j in jobs),
            ready_IN=sum(j['ready_for_manual_preflight'] and j['difficulty']=='IN' for j in jobs),
            ready_AT=sum(j['ready_for_manual_preflight'] and j['difficulty']=='AT' for j in jobs)),
        device_access=False,input_enabled=False,gameplay_qualified=False)
    write_new(output/'test-library.json',plan)
    return plan


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('library','identity','legacy','at-state','at-reference','executable','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--profile-cache',type=Path)
    a=p.parse_args();result=prepare(a.library,a.identity,a.legacy,a.at_state,a.at_reference,a.executable,a.output,a.profile_cache)
    print(json.dumps(result['counts']))
