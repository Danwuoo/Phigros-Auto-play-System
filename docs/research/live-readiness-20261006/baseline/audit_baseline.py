#!/usr/bin/env python3
"""Maintenance-only SHA/receipt audit. Does not replay product analysis or run binaries."""
import argparse, collections, hashlib, json, pathlib, subprocess

BASE='e9e305e3aba13f05a786c74540419428610b2f76'
PARENT='f07dc408385aaf70e1885a2734fc51097d6ed840'
WIN='docs/research/zero-miss-20261006/windows'
PRE=WIN+'/prelive-evidence-01'
EXPECTED_FINAL='f059078eba9fdf06ed66e9fe08176ce7531081ef44334f5eda2c227cd2518445'

def digest(b):return hashlib.sha256(b).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return digest(p.read_bytes())
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args])
def winrel(path):
    p=path.replace('\\','/')
    return p.split('/PAS-zero-miss-r4-20261006/',1)[1] if '/PAS-zero-miss-r4-20261006/' in p else None

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--repo',type=pathlib.Path,default=pathlib.Path('.'));ap.add_argument('--restored',type=pathlib.Path);ap.add_argument('--output',type=pathlib.Path,required=True);args=ap.parse_args()
    repo=args.repo.resolve(); root=repo/WIN; pre=repo/PRE; checks=[]
    def check(name,ok,**details):checks.append({'id':name,'status':'pass' if ok else 'fail',**details});return ok
    def record(p):return {'path':p.relative_to(repo).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)}
    def audit_index(p,expected_count,expected_sha=None):
        d=read(p);entries=d.get('entries',d.get('files'));names=[];bad=[]
        for e in entries:
            rel=e.get('relative',e.get('path'));names.append(rel);f=p.parent/rel
            if not f.is_relative_to(p.parent) or '..' in pathlib.PurePosixPath(rel).parts or not f.is_file():bad.append({'path':rel,'problem':'missing/unsafe'});continue
            b=f.read_bytes()
            if len(b)!=e['bytes'] or digest(b)!=e['sha256']:bad.append({'path':rel,'problem':'SHA/bytes'})
        check('seal:'+p.parent.name+':'+p.name,len(entries)==expected_count and len(set(names))==len(names) and not bad and (expected_sha is None or sha(p)==expected_sha),index=record(p),expected_entries=expected_count,checked_entries=len(entries),failures=bad)
        return entries
    parents=git(repo,'rev-list','--parents','-n','1',BASE).decode().split()[1:]
    changes=git(repo,'diff','--name-only',PARENT,BASE).decode().splitlines()
    allowed=['README.md',WIN+'/MAIN_MERGE_20261006.json',WIN+'/PRELIVE_COMPACT_PACKAGE.json',WIN+'/PRELIVE_IGNORED_PACKAGE.md','research/compact_prelive_package.ps1']
    check('packaging_commit_immediate_parent',parents==[PARENT],commit=BASE,parents=parents)
    check('packaging_only_delta',set(changes)==set(allowed),changed_paths=changes,product_source_changed=False if set(changes)==set(allowed) else None)
    final_entries=audit_index(pre/'SHA256_INDEX_FINAL.json',4104,EXPECTED_FINAL)
    first=read(pre/'SHA256_INDEX.json');audit_index(pre/'SHA256_INDEX.json',len(first['entries']),'3c8ecef50639858ecf71f2ed88e4a4653aaae2071125cc07c625cc23db9394bd')
    final=read(pre/'closure-final.json');closure=read(pre/'closure.json');candidate=read(pre/'candidate-manifest.json')
    for s in final['historical_seals']:
        audit_index(root/s['name']/s['index']['path'].split('\\')[-1],s['checked'],s['index']['sha256'])
    for key in ('preserved_external_index','preserved_first_index'):
        e=final[key];p=pre/e['path'].split('\\')[-1];check('closure_binding:'+key,p.stat().st_size==e['bytes'] and sha(p)==e['sha256'],file=record(p))
    # Map exact frozen Windows source bytes to immutable Git blobs; never hide CRLF changes.
    by_sha=collections.defaultdict(list)
    for e in final_entries:by_sha[e['sha256']].append(e['relative'])
    source_map=[]
    for role,entries in [('candidate',candidate['source']),('frozen_v3',final['v3_core_rechecked'])]:
        for e in entries:
            rel=winrel(e['path']);b=git(repo,'show',BASE+':'+rel);normal=b.replace(b'\r\n',b'\n');windows=normal.replace(b'\n',b'\r\n')
            exact=digest(b)==e['sha256'] and len(b)==e['bytes'];converted=digest(windows)==e['sha256'] and len(windows)==e['bytes']
            w=repo/rel;current=w.read_bytes() if w.is_file() else None
            item={'role':role,'path':rel,'accepted_windows_sha256':e['sha256'],'accepted_windows_bytes':e['bytes'],'git_ref':BASE,'git_blob_sha256':digest(b),'git_blob_bytes':len(b),'mapping':'exact_bytes' if exact else 'LF_to_CRLF_only' if converted else 'MISMATCH','exact_sealed_snapshot_paths':[PRE+'/'+p for p in by_sha[e['sha256']] if 'source-' in p],'working_tree_sha256':digest(current) if current is not None else None,'working_tree_matches_baseline_blob':current==b}
            source_map.append(item);check('source_binding:'+rel,exact or converted,**item)
    # Verify archived process receipt contents and their embedded per-file hashes.
    stage_checks=[]
    for s in closure['stages']:
        stage=s['stage'];base=pre/'receipts'/stage
        if s['status']=='preflight_retained':
            stage_checks.append({'stage':stage,'status':'preflight_retained','native_launched':False});continue
        receipt=base/(stage+'-verification.json');state=read(base/'state.json');v=read(receipt);r=read(base/(stage+'-result.json'));e=s['receipt'];bad=[]
        if sha(receipt)!=e['sha256'] or receipt.stat().st_size!=e['bytes']:bad.append('closure_receipt_hash')
        exits={'native':v.get('native_exit',r['facts'].get('exit_code')),'runner':v.get('runner_exit',r['facts'].get('runner_exit')),'verification':v['verification_exit']}
        for k in exits:
            if s[k]!=exits[k]:bad.append(k)
        if state!=s['state']:bad.append('closure_state')
        for ref in v.get('entries',[]):
            f=base/ref['path'].replace('\\','/').split('/')[-1]
            if not f.is_file() or f.stat().st_size!=ref['bytes'] or sha(f)!=ref['sha256']:bad.append('receipt_entry:'+f.name)
        stage_checks.append({'stage':stage,'status':s['status'],'native':exits['native'],'runner':exits['runner'],'verification':exits['verification'],'state_status':state['status'],'state_reason':state.get('reason'),'result_reason':r['facts'].get('reason'),'receipt':record(receipt),'binding_errors':bad})
    check('closure_stage_receipt_bindings',not any(s.get('binding_errors') for s in stage_checks),stage_count=len(stage_checks),status_counts=dict(collections.Counter(s['status'] for s in stage_checks)))
    # Use actual assertion rows and GTest results, not README claims.
    regressions=[]
    for mode,attempt in [('release','14'),('debug','05'),('asan','05')]:
        for suite,stage_type,count in [('prefix_preservation','prefix-preservation',113),('front_preservation','front-preservation',56),('own_prefix','own-prefix',113),('current_tests','current-tests',68),('formal_affected','formal-affected',186)]:
            p=pre/'package'/f'{suite}-{mode}-{attempt}.json';d=read(p);stage=f'{stage_type}-prelive-{mode}-{attempt}';s=next(x for x in stage_checks if x['stage']==stage)
            if suite=='formal_affected':
                rows=[r for group in d['testsuites'] for r in group['testsuite']];passed=sum(r.get('status')=='RUN' and r.get('result')=='COMPLETED' and not r.get('failures') for r in rows);ok=d['tests']==count and len(rows)==count and passed==count and d['failures']==d['errors']==d['disabled']==0
            else:
                rows=d['checks'];passed=sum(r['pass'] is True for r in rows);ok=d['assertions']==count and len(rows)==count and passed==count
            fr=read(pre/'receipts'/stage/'freeze.json')['files'];expected_source={e['path']:(e['bytes'],e['sha256']) for e in candidate['source']};freeze_source_matches=sum((e['bytes'],e['sha256'])==expected_source[e['path']] for e in fr if e['path'] in expected_source)
            params=read(pre/'package'/(stage+'-params.json'));executable=params['arguments']['Executable'];binding=next((e for e in fr if e['path'].lower()==executable.lower()),None)
            binary=next(b for b in candidate['binaries'] if b['mode']==mode);manifest_executable=next((e for e in binary['files'] if e['path'].lower()==executable.lower()),None)
            bound=bool(binding and manifest_executable and binding==manifest_executable)
            process_ok=s['native']==s['runner']==s['verification']==0 and s['state_status']=='COMPLETE' and not s['binding_errors']
            item={'mode':mode,'suite':suite,'expected':count,'row_count':len(rows),'passed':passed,'result':record(p),'stage':stage,'process_receipt_valid':process_ok,'candidate_binary_freeze_bound':bound,'frozen_candidate_source_matches':freeze_source_matches,'executable':binding}
            regressions.append(item);check('accepted_regression:'+mode+':'+suite,ok and process_ok and bound and freeze_source_matches>=13,**item)
    p=pre/'package/pixels-release-prefix-03.json';prefix=read(p);audit=read(pre/'package/audit-pixels-prefix.json');selection=read(pre/'package/full-prefix-selection.json')
    expected={'frames':3063,'processed':3063,'raw_proposals':10314,'accepted':2771,'own_down':55,'own_move':121,'own_up':51,'current_bodies':49,'current_terminals':361,'unknown':1773,'event_count':9889,'release_verified':True,'device_endpoints':0,'game_adoption':'unknown','physical_gold':0,'stop':''}
    check('accepted_prefix_summary',all(prefix.get(k)==v for k,v in expected.items()),file=record(p),expected=expected,observed={k:prefix.get(k) for k in expected})
    check('accepted_prefix_archived_independent_audit',audit['phases']==[55,121,51] and audit['receipts']==227 and audit['integrity'] is True and audit['complete_prefix_evidence'] is True and not audit['errors'] and audit['event_rows']==9889,file=record(pre/'package/audit-pixels-prefix.json'),observed=audit,note='Archived C++ audit receipt verified as evidence; fresh raw C++ recomputation is separate.')
    frames=selection['frames'];check('prefix_selection_contiguous',len(frames)==3063 and [e['index']['ordinal'] for e in frames]==list(range(1,3064)) and all(e['sha256']==e['index']['png_sha256'] for e in frames),count=len(frames),selection=record(pre/'package/full-prefix-selection.json'))
    # Every preserved STOP has its own immutable identity; do not reclassify native-zero STOPs.
    stops=[]
    for area in ['evidence','withdrawal-acceptance-evidence-01','front-candidate-evidence-01','front-controller-acceptance-evidence-01','prelive-evidence-01']:
        for p in sorted((root/area).rglob('state.json')):
            d=read(p)
            if d.get('status')=='STOP':stops.append({**record(p),'reason':d.get('reason'),'attempt':d.get('attempt'),'receipt_count':len(d.get('receipts',[]))})
    legacy=[]
    for p in sorted((root/'evidence').rglob('legacy-summary.json')):
        d=read(p);legacy.append({**record(p),'aggregate_pass':d['aggregate_pass'],'failed_assertions':d['failed_assertions'],'failed_rows':len(d['failed_rows']),'failed_rows_sha256':digest(json.dumps(d['failed_rows'],sort_keys=True,separators=(',',':')).encode()),'assertions':d['assertions'],'layer_cases':d['layer_cases'],'new_case_failed_assertions':d['new_case_failed_assertions'],'suite_native_exit':d['suite_native_exit']})
    check('three_legacy_36_fail_results_preserved',len(legacy)==3 and len({x['failed_rows_sha256'] for x in legacy})==1 and all(x['failed_assertions']==x['failed_rows']==36 and x['assertions']==3938 and x['layer_cases']==356 and x['aggregate_pass'] is False for x in legacy),results=legacy)
    aa=read(pre/'package/analysis-release-aa-freeze.json');ab=read(pre/'package/ab-disposition.json');hold=read(pre/'package/pipeline-release-aa-hold-1.json');writer=read(pre/'package/audit-writer-final.json');negative=read(pre/'package/audit-negative-count.json');failedprefix=read(pre/'package/pixels-release-prefix-01.json')
    check('AA_stopped_AB_unrun',len(aa['normal_runs'])==5 and aa['comparison_allowed'] is False and aa['ready'] is False and aa['candidate_results_used']==0 and hold['own_down']==hold['own_up']==hold['all_phase_lateness_ms']['n']==0 and ab['comparison_allowed'] is False and ab['planned_runs']==36 and ab['status'].startswith('not_run'),aa=record(pre/'package/analysis-release-aa-freeze.json'),ab=record(pre/'package/ab-disposition.json'))
    check('writer_loss_remains_incomplete',writer['integrity'] is True and writer['declared_writer_loss'] is True and writer['complete_prefix_evidence'] is False and final['writer_complete_prefix_evidence'] is False,receipt=record(pre/'package/audit-writer-final.json'))
    check('negative_count_rejected',negative['integrity'] is False and 'receipt_phase_counts' in negative['errors'],receipt=record(pre/'package/audit-negative-count.json'))
    check('failed_2742_prefix_remains_failed',failedprefix['processed']==2742 and failedprefix['release_verified'] is False and failedprefix['stop']!='',receipt=record(pre/'package/pixels-release-prefix-01.json'))
    check('gates_remain_NOT_READY',final['normal_cost_gate']==candidate['normal_cost_gate']=='NOT_READY' and final['physical_adoption']=='unknown' and not final['product_goal_complete'] and not candidate['live_cli_hook'] and candidate['endpoint_count']==0,normal_cost_gate=final['normal_cost_gate'],physical_adoption=final['physical_adoption'],live_cli_hook=candidate['live_cli_hook'],product_goal_complete=final['product_goal_complete'])
    original_refs=read(root/'evidence/external-originals.json')['original_stop_states']
    historical_stop_inventory=[{**e,'payload_status':'not_supplied'} for e in original_refs]
    check('71_historical_STOP_identities_indexed',len(original_refs)==71 and len({e['path'] for e in original_refs})==71,index=record(root/'evidence/external-originals.json'),identity_count=len(original_refs),raw_bytes_verified=0)
    external={'status':'not_supplied','raw_reexecution_performed':False}
    if args.restored:
        restored=args.restored.resolve();files=read(restored/'_package/FILES.json')['entries'];excluded=read(restored/'_package/EXCLUDED.json')['entries'];by_source={e['source'].replace('\\','/').casefold():e for e in files};excluded_source={e['source'].replace('\\','/').casefold():e for e in excluded}
        for e in historical_stop_inventory:
            packaged=by_source.get(e['path'].replace('\\','/').casefold())
            if packaged:
                f=restored/packaged['entry'];e['payload_status']='verified' if f.is_file() and sha(f)==e['sha256'] and f.stat().st_size==e['bytes'] else 'mismatch'
        refs=read(pre/'external-new-out.json')['files']+final['appended_external_files'];counts=collections.Counter();external_bad=[]
        for ref in refs:
            key=ref['path'].replace('\\','/').casefold()
            e=by_source.get(key)
            if e:
                f=restored/e['entry'];ok=e['sha256']==ref['sha256'] and e['bytes']==ref['bytes'] and f.is_file() and f.stat().st_size==ref['bytes'] and sha(f)==ref['sha256'];counts['retained_verified' if ok else 'mismatch']+=1
                if not ok:external_bad.append(ref)
            elif key in excluded_source:
                e=excluded_source[key];ok=e['sha256']==ref['sha256'] and e['bytes']==ref['bytes'];counts['declared_omitted_matching_identity' if ok else 'omitted_identity_mismatch']+=1
                if not ok:external_bad.append(ref)
            else:counts['unaccounted']+=1;external_bad.append(ref)
        check('external_closure_retained_or_explicitly_omitted',not external_bad,counts=dict(counts),unaccounted_or_mismatch=external_bad)
        png_bad=[]
        for e in frames:
            packaged=by_source.get(e['path'].replace('\\','/').casefold());p=restored/packaged['entry'] if packaged else None
            if not packaged or packaged['sha256']!=e['sha256'] or not p.is_file() or p.stat().st_size!=e['bytes'] or sha(p)!=e['sha256']:png_bad.append(e['path'])
        check('3063_original_PNG_bytes',not png_bad,checked=len(frames),failures=png_bad)
        binary_rows=[]
        for b in candidate['binaries']:
            for e in b['files']:
                key=e['path'].replace('\\','/').casefold();packaged=by_source.get(key);omitted=excluded_source.get(key);p=restored/packaged['entry'] if packaged else None
                status='verified' if packaged and p.is_file() and sha(p)==e['sha256'] and p.stat().st_size==e['bytes'] else 'deliberately_omitted' if omitted and omitted['sha256']==e['sha256'] and omitted['bytes']==e['bytes'] else 'unaccounted_or_mismatch'
                binary_rows.append({'mode':b['mode'],'path':e['path'],'sha256':e['sha256'],'status':status})
        check('final_binary_payload_bindings',all(x['status']!='unaccounted_or_mismatch' for x in binary_rows),files=binary_rows)
        external={'status':'compact_retained_payload_verified','closure_references':dict(counts),'binary_files':binary_rows,'png_verified':3063 if not png_bad else 0,'raw_reexecution_performed':False,'full_rebuild_environment':False,'host_system_DLL_bytes':'not_supplied; recorded backing-file SHA is not mapped-image measurement'}
    failed=[c['id'] for c in checks if c['status']=='fail']
    report={'schema':'pas.live-readiness.baseline-provenance.v1','baseline_commit':BASE,'package_parent_commit':PARENT,'method':'Independent bytes/SHA/JSON-row/receipt-binding checks; no Windows executable or live/device invocation. Maintenance Python only.','verification_pass':not failed,'failed_checks':failed,'checks':checks,'source_mapping':source_map,'regressions':regressions,'closure_stages':stage_checks,'preserved_STOP_states':stops,'preserved_STOP_counts':dict(collections.Counter(p['path'].split('/windows/')[1].split('/')[0] for p in stops)),'historical_original_STOP_inventory':historical_stop_inventory,'historical_original_STOP_summary':{'indexed_identities':len(historical_stop_inventory),'payload_status_counts':dict(collections.Counter(e['payload_status'] for e in historical_stop_inventory)),'note':'The 71 original STOP identities are hashed in evidence/external-originals.json; their original bytes are absent from the compact package. The 41 observable recent STOP states are a separate set. Missing historical raw is not promoted to independently verified preservation.'},'external_payload':external,'readiness':{'evidence_provenance':'verified' if not failed else 'failed','normal_cost_gate':'NOT_READY','physical_adoption':'unknown','live_cli_hook':False,'product_goal_complete':False,'fresh_raw_CXX_audit':'separate work; not claimed by this maintenance audit'}}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print(json.dumps({'verification_pass':not failed,'checks':len(checks),'failed_checks':failed,'STOP_states':len(stops),'external':external['status']}))
    return 0 if not failed else 1
if not __debug__: raise RuntimeError('Audit checks require Python without -O optimization')
if __name__=='__main__':raise SystemExit(main())
