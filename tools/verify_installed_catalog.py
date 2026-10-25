"""Offline exact installed APK/public-chart identity mapping, including IN/AT.

Reads authorized local APKs once, retains bounded per-track bundles, and writes
only provenance JSON. Native FSB5 is hashed in memory; no duplicate audio/WAV,
chart, image, network access, device discovery or touch input is produced.
"""
import argparse
import hashlib
import json
from pathlib import Path

from extract_phigros_target import AssetExtractionSession, textasset_bytes, unique


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def reference(path):
    path = Path(path).resolve()
    return dict(file=str(path), sha256=sha(path))


def write_new(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def bind_chart(row, script):
    if row['difficulty'] not in ('IN', 'AT'):
        raise ValueError('Exact IN/AT difficulty required')
    actual = hashlib.sha256(script).hexdigest()
    if actual != row['chart']['sha256']:
        raise ValueError('Installed/public chart bytes differ; no identity alias allowed')
    return actual


def audio_identity(session, track):
    from UnityPy.helpers.ResourceReader import get_resource_data
    key = 'Assets/Tracks/' + track + '/music.wav'
    reader, obj, raw, origin, env = session.select(key, 'AudioClip')
    resource = obj.m_Resource
    if obj.m_AudioData or resource is None or not 0 < resource.m_Size <= 64*1024*1024 or resource.m_Offset < 0:
        raise ValueError('Unsupported bounded streamed audio resource')
    cab = env.get_cab(resource.m_Source.rsplit('/', 1)[-1])
    if cab is None or resource.m_Offset + resource.m_Size > cab.Length:
        raise ValueError('Audio resource outside selected bundle')
    data = get_resource_data(resource.m_Source, reader.assets_file, resource.m_Offset, resource.m_Size)
    if len(data) != resource.m_Size or not data.startswith(b'FSB5'):
        raise ValueError('Exact native FSB5 resource required')
    return dict(format='native-fsb5', sha256=hashlib.sha256(data).hexdigest(), bytes=len(data),
                catalog_key=key, source=origin,
                resource=dict(source=resource.m_Source, offset=resource.m_Offset, bytes=resource.m_Size),
                sample_rate=obj.m_Frequency, channels=obj.m_Channels,
                duration_metadata_seconds=obj.m_Length,
                scope='Exact installed native resource identity; not a decoded WAV or audio synchronization qualification')


def verify(library, installed_source, apks, output):
    library, output = Path(library).resolve(), Path(output).resolve()
    if output.exists():
        raise FileExistsError('Output must be new')
    data = json.loads(library.read_text(encoding='utf-8-sig'))
    if data.get('schema') != 'pas.catalog-library.v1' or not 1 <= len(data['charts']) <= 10000:
        raise ValueError('Bounded catalog library required')
    pinned_library = reference(library)
    session = AssetExtractionSession(apks, installed_source)
    output.mkdir(parents=True)
    source_dir = output/'sources';source_dir.mkdir()
    rows = []
    by_track = {}
    for row in data['charts']:
        by_track.setdefault(row['track_id'], []).append(row)
    for track, charts in by_track.items():
        # Native bundles and environments never accumulate across tracks.
        session.bundle_cache.clear();session.cached_bundle_bytes = 0
        audio = None
        for row in charts:
            result = dict(library_id=row['library_id'], track_id=track, difficulty=row['difficulty'],
                          chart_sha256=row['chart']['sha256'], status='not_verified', source_manifest=None)
            try:
                public = (library.parent/row['chart']['file']).resolve()
                if not public.is_relative_to(library.parent) or sha(public) != row['chart']['sha256']:
                    raise ValueError('Public chart path/hash mismatch')
                key = 'Assets/Tracks/'+track+'/Chart_'+row['difficulty']+'.json'
                reader, obj, raw, origin, env = session.select(key, 'TextAsset')
                name, script, offset = textasset_bytes(raw, reader.reader.endian)
                if name != obj.m_Name or script != obj.m_Script.encode('utf-8', 'surrogateescape'):
                    raise ValueError('Raw TextAsset/typed reader mismatch')
                bind_chart(row, script)
                if audio is None:
                    audio = audio_identity(session, track)
                source = dict(schema='pas.installed-catalog-song-source.v1',
                    package=session.source['package'], version_name=session.source['version_name'],
                    version_code=session.source['version_code'], track_id=track, difficulty=row['difficulty'],
                    catalog=session.catalog_origin, installed_source=reference(installed_source),
                    packages=[dict(file=p['path'],sha256=p['sha256'],bytes=p['bytes']) for p in session.inventory['packages']],
                    chart=dict(file=str(public),sha256=row['chart']['sha256'],source=origin,
                               textasset_script_byte_offset=offset,byte_transform='none'),
                    audio_identity=audio, public_library=pinned_library, public_library_id=row['library_id'],
                    exact_chart_bytes_verified=True, input_enabled=False, device_access=False)
                name = hashlib.sha256(row['library_id'].encode()).hexdigest()[:24]+'.json'
                path = source_dir/name;write_new(path,source)
                result.update(status='verified_exact_installed_chart', source_manifest=reference(path),
                              native_audio_sha256=audio['sha256'])
            except (ValueError, KeyError, TypeError, RuntimeError) as error:
                result.update(status='rejected', reason=str(error))
            rows.append(result)
        print(json.dumps(dict(track=track,checked=len(rows)),ensure_ascii=False),flush=True)
    session.verify_sources_unchanged()
    if reference(library) != pinned_library:
        raise ValueError('Library changed during verification')
    report=dict(schema='pas.installed-catalog-identity.v1',library=pinned_library,
        installed_source=reference(installed_source),packages=[dict(file=p['path'],sha256=p['sha256']) for p in session.inventory['packages']],
        total=len(rows),verified=sum(r['status']=='verified_exact_installed_chart' for r in rows),charts=rows,
        input_enabled=False,device_access=False,gameplay_qualified=False,
        scope='Exact native chart and audio-resource identity only; profile/HUD/format/resource preparation is separate')
    write_new(output/'identity-map.json',report)
    return report


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library',type=Path,required=True)
    parser.add_argument('--installed-source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('apks',nargs='+',type=Path)
    args=parser.parse_args()
    result=verify(args.library,args.installed_source,args.apks,args.output)
    print(json.dumps(dict(total=result['total'],verified=result['verified'])))
