import pathlib,json,hashlib,zipfile,os,subprocess,shutil
repo=pathlib.Path('/workspace/shared/phigros-live-readiness-20261006');old=pathlib.Path('/workspace/shared/phigros-producer-seam-final-20261006-02');new=pathlib.Path('/workspace/shared/phigros-producer-seam-final-20261006-04')
if new.exists():raise RuntimeError('fresh replay root')
if shutil.disk_usage(repo).free<20*1024**3+160*1024**2:raise RuntimeError('replay capacity reserve')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
changes=[]
for line in (old/'source-before.sha256').read_text().splitlines():
 h,p=line.split(None,1);p=p.strip();f=pathlib.Path(p);f=f if f.is_absolute() else repo/f
 if sha(f)!=h:changes.append({'path':p,'old':h,'new':sha(f)})
assert {x['path'] for x in changes}=={'research/prelive_current/CMakeLists.txt','research/prelive_current/run.ps1','research/prelive_current/owner_candidate_step.hpp'},changes
new.mkdir();receipt=json.loads((repo/'docs/research/live-readiness-20261006/capacity/final-cost-binary-lossless-archive.json').read_text());restored=[]
for x in receipt['rows']:
 p=pathlib.Path(x['path'])
 if p.name=='audit.o':continue
 assert p.parent.parent==old and p.name in ['seed','admission-review','input-closure-review','producer-review'] and not p.exists()
 z=pathlib.Path(x['archive_path']);assert sha(z)==x['archive_sha256']
 with zipfile.ZipFile(z) as f:
  assert f.testzip() is None;b=f.read(x['archive_entry'])
 assert len(b)==x['bytes'] and hashlib.sha256(b).hexdigest()==x['sha256'];p.write_bytes(b);p.chmod(0o700);restored.append({'path':str(p),'sha256':sha(p),'bytes':len(b),'archive_sha256':x['archive_sha256']})
(new/'binary-reuse.json').write_text(json.dumps({'schema':'pas.final-recipe-metadata-replay.v1','old_matrix':str(old),'only_changed_inputs':changes,'compile_scope':'The Linux g++ commands compiled C++ sources directly; CMake/PowerShell are unexecuted Windows recipes; owner_candidate_step.hpp is runtime input-binding data for these cost/audit control binaries, not included by their compiler translation units. It is rebuilt and functionally tested separately in the full runtime lane. All actual cost/audit control translation-unit/dependency hashes remain exact. Reuse only hash-verified locally compiled binaries; recreate all fixture evidence from current final source files.','restored_binaries':restored,'Windows_recipe_executed':False},indent=2)+'\n')
source=[]
for line in (old/'source-before.sha256').read_text().splitlines():
 _,p=line.split(None,1);p=p.strip();f=pathlib.Path(p);f=f if f.is_absolute() else repo/f;source.append((f,p,sha(f)))
(new/'source-before.sha256').write_text(''.join(f'{h}  {p}\n' for f,p,h in source))
shutil.copyfile(__file__,new/'replay-source.py');shutil.copyfile(old/'compiler.txt',new/'compiler.txt')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for mode in ['release','debug','sanitizers']:
 out=new/mode;out.mkdir();shutil.copyfile(old/mode/'flags.txt',out/'flags.txt')
 for name,exe,args in [('seed','seed',[str(out/'fixtures'),str(repo)]),('admission','admission-review',[str(out/'fixtures'),str(out/'admission-results')]),('input-closure','input-closure-review',[str(out/'fixtures'),str(out/'input-closure-results')]),('producer','producer-review',[str(out/'producer-results')])]:
  p=old/mode/exe
  with (out/(name+'.stdout')).open('wb') as stdout,(out/(name+'.stderr')).open('wb') as stderr:code=subprocess.run([str(p),*args],cwd=repo,env=env,stdout=stdout,stderr=stderr).returncode
  (out/(name+'.exit')).write_text(str(code)+'\n')
  if code:raise RuntimeError(f'{mode} {name} failed {code}')
 for f,p,h in source:assert sha(f)==h,p
 (out/'source-after-check.txt').write_text(''.join(f'{p}: OK\n' for f,p,h in source));(out/'matrix-exit.txt').write_text('0\n');print(mode+' complete',flush=True)
for x in restored:assert sha(pathlib.Path(x['path']))==x['sha256']
(new/'overall.exit').write_text('0\n');print('final recipe metadata replay all passed',flush=True)
