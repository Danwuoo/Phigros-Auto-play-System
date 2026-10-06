import pathlib,hashlib,zipfile,json,os,stat,time
repo=pathlib.Path('/workspace/shared/phigros-live-readiness-20261006')
roots=[pathlib.Path('/workspace/shared/phigros-producer-seam-final-20261006-02')/n for n in ['release','debug','sanitizers']]
archive=repo/'out/portable-binary-preservation'; receipt=repo/'docs/research/live-readiness-20261006/capacity/final-cost-binary-lossless-archive.json'
if receipt.exists(): raise RuntimeError('fresh archive receipt required')
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(1024*1024):h.update(b)
 return h.hexdigest()
def free():s=os.statvfs(repo);return s.f_bavail*s.f_frsize
before=free();groups={};rows=[]
for root in roots:
 for p in sorted(root.iterdir()):
  if p.is_symlink() or not p.is_file():continue
  with p.open('rb') as f:magic=f.read(8)
  if not (magic.startswith(b'\x7fELF') or magic==b'!<arch>\n'):continue
  st=p.stat();digest=sha(p);entry={'path':str(p),'bytes':st.st_size,'sha256':digest,'inode_before':st.st_ino,'device':st.st_dev,'links_before':st.st_nlink,'mtime_ns':st.st_mtime_ns}
  groups.setdefault(digest,[]).append(entry)
for digest,entries in sorted(groups.items(),key=lambda kv:-sum(x['bytes'] for x in kv[1])):
 zpath=archive/(digest+'.zip')
 source=pathlib.Path(entries[0]['path']);payload='payload.bin'
 if not zpath.exists():
  with zipfile.ZipFile(zpath,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:z.write(source,payload)
 with zipfile.ZipFile(zpath) as z:
  info=z.getinfo(payload)
  if info.file_size!=entries[0]['bytes'] or z.testzip() is not None:raise RuntimeError('archive size/CRC mismatch')
  h=hashlib.sha256()
  with z.open(payload) as f:
   while b:=f.read(1024*1024):h.update(b)
  if h.hexdigest()!=digest:raise RuntimeError('archive SHA mismatch')
 zhash=sha(zpath)
 for entry in entries:
  p=pathlib.Path(entry['path']);st=p.stat()
  if st.st_ino!=entry['inode_before'] or st.st_mtime_ns!=entry['mtime_ns'] or st.st_size!=entry['bytes'] or sha(p)!=digest:raise RuntimeError('source changed')
  entry.update(archive_path=str(zpath),archive_entry=payload,archive_sha256=zhash,archive_bytes=zpath.stat().st_size,verified_before_dematerialization=True)
 # All original bytes remain recoverable from a verified ZIP; only this task's
 # old materializations are removed. Current archives/binaries and all raw stay.
 for entry in entries:pathlib.Path(entry['path']).unlink();entry['materialized']=False;rows.append(entry)
 # Durable incremental receipt; an interruption never loses the restore map.
 data={'schema':'pas.own-linux-binary-preservation.v1','scope':'older completed generated Linux fixtures only; no user/original Windows/IRIS/raw/STOP/source removal','free_before':before,'free_now':free(),'minimum_free':20*1024**3,'restoration':'Verify archive SHA; extract payload.bin to its exact recorded original path only when that path is absent; verify bytes/SHA. Do not execute to restore.','rows':rows}
 tmp=receipt.with_suffix('.tmp');tmp.write_text(json.dumps(data,indent=2)+'\n');tmp.replace(receipt)
print(json.dumps({'archived_paths':len(rows),'unique_payloads':len(groups),'free_before':before,'free_after':free(),'reclaimed':free()-before,'receipt':str(receipt)}))
