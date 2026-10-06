#!/usr/bin/env python3
"""Maintenance-only archive integrity audit. Never executes archived programs."""
import argparse, collections, hashlib, json, os, pathlib, re, shutil, stat, time, zipfile

EXPECTED_SHA = 'e59766f09879b927b26cd0947ce501cea05f16fd838cfb1cce9321a63a0d7530'
EXPECTED_BYTES = 1001668802
EXPECTED_COMMIT = 'f07dc408385aaf70e1885a2734fc51097d6ed840'
META = {'_package/' + n for n in ['README.txt', 'FILES.json', 'EXCLUDED.json', 'SOURCE_ARCHIVE.json', 'compact_prelive_package.ps1', 'PRELIVE_RESULTS.md', 'PRELIVE_REPRODUCTION.md', 'PRELIVE_OPERATION_PACKET.md', 'PRELIVE_RESULT_TEMPLATE.json']}
RESERVED = re.compile(r'^(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(?:\.|$)', re.I)

def sha_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''): h.update(b)
    return h.hexdigest()

def safe_name(name):
    p = pathlib.PurePosixPath(name)
    assert name and not p.is_absolute() and not name.endswith('/'), 'absolute/directory path: ' + name
    assert '\\' not in name and ':' not in name and '\x00' not in name, 'separator/ADS/NUL: ' + name
    assert p.as_posix() == name and all(c not in ('', '.', '..') for c in name.split('/')), 'noncanonical path: ' + name
    assert all(not c.endswith((' ', '.')) and not any(ord(x) < 32 or x in '<>"|?*' for x in c) for c in p.parts), 'Windows unsafe name: ' + name
    return p

def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('archive',type=pathlib.Path); ap.add_argument('--output',type=pathlib.Path,required=True); ap.add_argument('--extract',type=pathlib.Path)
    args=ap.parse_args(); start=time.monotonic()
    assert args.archive.stat().st_size == EXPECTED_BYTES, 'archive byte count'
    actual_sha=sha_file(args.archive); assert actual_sha == EXPECTED_SHA, 'archive SHA'
    report={'schema':'pas.live-readiness.compact-archive-audit.v1','archive_bytes':EXPECTED_BYTES,'archive_sha256':actual_sha,'expected_merged_main_commit':EXPECTED_COMMIT,'binary_execution':False,'payload_verified':False,'extraction_verified':False}
    with zipfile.ZipFile(args.archive) as z:
        infos=z.infolist(); names={}; folded={}; windows_reserved=[]
        assert len(infos)==13679, 'entry count'
        for i in infos:
            safe_name(i.filename)
            if any(RESERVED.match(c) for c in pathlib.PurePosixPath(i.filename).parts): windows_reserved.append(i.filename)
            assert i.filename not in names and i.filename.casefold() not in folded, 'duplicate/case collision: '+i.filename
            mode=(i.external_attr >> 16)&0xffff
            assert not stat.S_ISLNK(mode) and stat.S_IFMT(mode) in (0,stat.S_IFREG), 'symlink/special entry: '+i.filename
            assert not (i.flag_bits & 1), 'encrypted entry'
            assert i.compress_type in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED), 'unsupported compression'
            assert i.file_size<=1024**3 and i.file_size/max(1,i.compress_size)<=100, 'size/ratio bound'
            names[i.filename]=i;folded[i.filename.casefold()]=i.filename
        assert sum(i.file_size for i in infos)<=6*1024**3, 'total expanded bound'
        # Reject file/directory aliases as well as duplicate leaf paths.
        for name in names:
            for p in pathlib.PurePosixPath(name).parents:
                assert p.as_posix().casefold() not in folded, 'file/directory conflict: '+name
        manifest=json.loads(z.read('_package/FILES.json'))
        excluded=json.loads(z.read('_package/EXCLUDED.json'))
        assert manifest['merged_main_commit']==EXPECTED_COMMIT, 'manifest source commit'
        entries=manifest['entries']; assert len(entries)==13670, 'payload count'
        declared={}
        for e in entries:
            safe_name(e['entry']);assert e['entry'] not in declared,'manifest duplicate'
            assert e['entry'] in names and names[e['entry']].file_size==e['bytes'], 'manifest bytes/missing'
            assert re.fullmatch('[0-9a-f]{64}', e['sha256']), 'SHA format'
            declared[e['entry']]=e
        assert set(names)==set(declared)|META and not(set(declared)&META), 'unlisted/missing entries'
        excluded_entries={e['entry']:e for e in excluded['entries']}
        assert len(excluded_entries)==len(excluded['entries']) and not(set(excluded_entries)&set(declared)), 'retained/excluded overlap'
        report.update(structure_verified=True,posix_quarantine_extractable=True,windows_direct_extraction_safe=not windows_reserved,windows_reserved_paths=windows_reserved,entry_count=len(infos),payload_count=len(entries),metadata_count=len(META),expanded_bytes=sum(i.file_size for i in infos),compressed_entry_bytes=sum(i.compress_size for i in infos),maximum_expansion_ratio=max(i.file_size/max(1,i.compress_size) for i in infos),excluded_count=len(excluded_entries),case_duplicates=0,unsafe_paths=0,symlink_or_special_entries=0,encrypted_entries=0,categories=dict(collections.Counter(e['category'] for e in entries)))
        save(args.output,report);print('STRUCTURE_VERIFIED '+json.dumps({k:report[k] for k in ['entry_count','payload_count','expanded_bytes','maximum_expansion_ratio']}),flush=True)
        observed={}; next_notice=time.monotonic()+15
        for n,i in enumerate(infos,1):
            h=hashlib.sha256(); count=0
            # Reading to EOF verifies each ZIP CRC independently of SHA.
            with z.open(i) as f:
                for b in iter(lambda:f.read(1024*1024),b''):count+=len(b);h.update(b)
            assert count==i.file_size, 'stream size'
            digest=h.hexdigest()
            if i.filename in declared: assert digest==declared[i.filename]['sha256'], 'payload SHA: '+i.filename
            observed[i.filename]={'bytes':count,'sha256':digest}
            if time.monotonic()>=next_notice: print('STREAM_SHA_VERIFIED '+str(n)+'/'+str(len(infos)),flush=True);next_notice=time.monotonic()+15
        report.update(payload_verified=True,payload_sha_verified=len(entries),crc_verified=len(infos),metadata_sha256={n:observed[n]['sha256'] for n in sorted(META)},payload_bytes=sum(e['bytes'] for e in entries),elapsed_seconds=round(time.monotonic()-start,3))
        save(args.output,report);print('PAYLOAD_VERIFIED '+str(len(entries)),flush=True)
        if args.extract:
            assert os.name=='posix', 'quarantine extraction requires POSIX; Windows reserved-name hazard is retained'
            assert not args.extract.exists(), 'fresh extraction root required'
            assert shutil.disk_usage(args.extract.parent).free>report['expanded_bytes']+5*1024**3, 'free disk safety reserve'
            args.extract.mkdir(mode=0o700)
            root=args.extract.resolve()
            # Extract only after the entire archive has passed structure/CRC/SHA checks.
            for n,i in enumerate(infos,1):
                dst=root.joinpath(*safe_name(i.filename).parts)
                assert dst.is_relative_to(root), 'containment'
                dst.parent.mkdir(parents=True,exist_ok=True)
                assert not dst.parent.is_symlink(), 'unexpected symlink parent'
                h=hashlib.sha256();count=0
                with z.open(i) as src, open(dst,'xb') as out:
                    for b in iter(lambda:src.read(1024*1024),b''):out.write(b);h.update(b);count+=len(b)
                dst.chmod(0o600) # Treat even executables as nonexecutable data.
                assert {'bytes':count,'sha256':h.hexdigest()}==observed[i.filename], 'extraction SHA: '+i.filename
            report.update(extraction_verified=True,extracted_entries=len(infos),extraction_root=str(root),elapsed_seconds=round(time.monotonic()-start,3))
            save(args.output,report);print('EXTRACTION_VERIFIED '+str(root),flush=True)

if not __debug__: raise RuntimeError('Audit checks require Python without -O optimization')
if __name__=='__main__': main()
