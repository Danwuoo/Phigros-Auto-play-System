"""Install a versioned desktop frontend without starting the app or a device.

The existing native archive is kept unless a new logging-capable archive
and its manifest SHA-256 are explicitly supplied.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid


MEMBERS = (
    'tools/desktop-start-runtime.ps1',
    'tools/sweep-panel.ps1',
    'tools/catalog-panel-core.ps1',
    'tools/song-library-panel.ps1',
    'tools/start-song-library.ps1',
    'tools/round_history.py',
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def encoded(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def pin(path: Path, data: bytes | None = None) -> dict:
    return {'file': str(path), 'sha256': digest(path.read_bytes() if data is None else data)}


def write_new(path: Path, data: bytes) -> None:
    with path.open('xb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def replace_if_unchanged(path: Path, previous: bytes, following: bytes) -> None:
    temporary = path.with_name(path.name + '.next-' + uuid.uuid4().hex)
    write_new(temporary, following)
    if path.read_bytes() != previous:
        raise RuntimeError(f'Installation interrupted: file changed concurrently: {path}')
    os.replace(temporary, path)


def native_pin(archive: Path, expected_sha: str) -> tuple[dict, str]:
    manifest_path = archive / 'manifest.json'
    manifest_data = manifest_path.read_bytes()
    if len(manifest_data) > 1048576 or digest(manifest_data) != expected_sha:
        raise RuntimeError('New native archive manifest does not match its supplied SHA-256')
    manifest = json.loads(manifest_data)
    if manifest.get('schema') != 'pas.adaptive-build-archive.v1':
        raise RuntimeError('New native archive has the wrong schema')
    artifacts = manifest['artifacts']
    if not 1 <= len(artifacts) <= 4096:
        raise RuntimeError('New native archive exceeds the artifact limit')
    paths = {}
    total = 0
    for item in artifacts:
        relative = Path(item['file'])
        path = archive / relative
        if (relative.is_absolute() or '..' in relative.parts or not path.is_file()
                or path.resolve() != path or relative.as_posix().casefold() in paths):
            raise RuntimeError(f'Invalid native archive member: {relative}')
        size = path.stat().st_size
        total += size
        if size != item['bytes'] or total > 1073741824:
            raise RuntimeError(f'Native archive size differs: {relative}')
        with path.open('rb') as stream:
            sha = hashlib.file_digest(stream, 'sha256').hexdigest()
        if sha != item['sha256']:
            raise RuntimeError(f'Native archive member differs: {relative}')
        paths[relative.as_posix().casefold()] = sha
    executable_sha = paths.get('bin/chart_experiment.exe')
    if not executable_sha or any(
        f'bin/{path.name.casefold()}' not in paths
        for path in (archive / 'bin').iterdir()
        if path.suffix.casefold() in {'.exe', '.dll'}
    ):
        raise RuntimeError('Native executable or DLL pins are incomplete')
    return pin(manifest_path, manifest_data), executable_sha


def prepared_plan_pin(path: Path, expected_sha: str, config: dict, executable_sha: str) -> tuple[dict, dict]:
    path = path.resolve(strict=True)
    raw = path.read_bytes()
    if len(raw) > 8 * 1024 * 1024 or digest(raw) != expected_sha:
        raise RuntimeError('Prepared plan does not match its supplied SHA-256')
    before = config['plan']
    old_raw = Path(before['file']).read_bytes()
    if before != config['library'] or digest(old_raw) != before['sha256']:
        raise RuntimeError('Existing desktop plan identity changed')
    old, new = json.loads(old_raw), json.loads(raw)
    for key in ('schema', 'catalog', 'identity', 'round_limit', 'phase_ms', 'image_retention',
                'capacity_policy', 'contact_capacity'):
        if old[key] != new[key]:
            raise RuntimeError(f'Prepared plan changed the existing {key}')
    if len(old['jobs']) != len(new['jobs']):
        raise RuntimeError('Prepared plan changed the catalog size')
    reference = pin(path, raw)
    report_path = path.with_name('refresh-report.json')
    if report_path.stat().st_size > 2 * 1024 * 1024:
        raise RuntimeError('Prepared plan refresh report exceeds the size limit')
    report = json.loads(report_path.read_bytes())
    if (report.get('schema') != 'pas.block-area-offline-refresh.v1' or report.get('plan') != reference
            or report['inputs']['plan'] != before or report['inputs']['executable']['sha256'] != executable_sha
            or report.get('device_discovery') is not False or report.get('input_enabled') is not False
            or report.get('gameplay_qualified') is not False):
        raise RuntimeError('Prepared plan evidence must match the archived engine and existing plan')
    overrides = report.get('song_overrides', {})
    receipts = {row['library_id']: row for row in report['rows']}
    for previous, following in zip(old['jobs'], new['jobs'], strict=True):
        if any(previous.get(key) != following.get(key) for key in (
                'library_id', 'track_id', 'difficulty', 'chart', 'notes')):
            raise RuntimeError('Prepared plan changed a chart identity')
        if previous['ready_for_manual_preflight'] and previous != following:
            raise RuntimeError('Prepared plan changed an existing playable profile')
        phase = following.get('phase_ms')
        if previous.get('phase_ms') != phase:
            library_id = following['library_id']
            receipt = receipts.get(library_id, {})
            if (type(phase) is not int or not -100 <= phase <= 100
                    or previous['ready_for_manual_preflight'] is not False
                    or following['ready_for_manual_preflight'] is not True
                    or overrides.get(library_id, {}).get('phase_ms') != phase
                    or receipt.get('catalog_blocker') != 'unsupported_block_area'
                    or receipt.get('ready') is not True or receipt.get('phase_ms') != phase
                    or receipt.get('compiled_fingerprint') != following['compiled_fingerprint']
                    or receipt.get('block_area') != following['preparation']['block_area']
                    or receipt['block_area']['phase_ns'] != phase * 1_000_000
                    or following['compile_policy']['block_area_phase_ms'] != phase):
                raise RuntimeError('Prepared song phase changed without matching BlockArea preflight')
            profile_pin = following['profile']
            profile_bytes = Path(profile_pin['file']).read_bytes()
            if len(profile_bytes) > 32768 or digest(profile_bytes) != profile_pin['sha256']:
                raise RuntimeError('Prepared song phase profile pin differs')
            profile = json.loads(profile_bytes)
            if (profile['block_area_phase_ms'] != phase
                    or profile['expected_plan_sha256'] != following['compiled_fingerprint']):
                raise RuntimeError('Prepared song phase differs from the native profile')
        if following['ready_for_manual_preflight'] is True and following.get('gameplay_qualified') is not False:
            raise RuntimeError('An offline prepared profile cannot claim gameplay qualification')
    if any(new['counts'][key] != expected for key, expected in {
            'songs': old['counts']['songs'], 'charts': len(new['jobs']),
            'ready': sum(row['ready_for_manual_preflight'] is True for row in new['jobs']),
            **{f'ready_{difficulty}': sum(row['ready_for_manual_preflight'] is True and row['difficulty'] == difficulty
                                          for row in new['jobs']) for difficulty in ('IN', 'AT')},
    }.items()):
        raise RuntimeError('Prepared plan counts differ from the actual rows')
    return reference, new['counts']


def install(repository: Path, expected_config_sha: str | None = None,
            native_archive: Path | None = None, native_manifest_sha: str | None = None,
            prepared_plan: Path | None = None, prepared_plan_sha: str | None = None) -> dict:
    repository = repository.resolve(strict=True)
    configuration = repository / 'local/manual-launch.json'
    launcher = repository / 'local/start-phigros.ps1'
    previous_config = configuration.read_bytes()
    previous_launcher = launcher.read_bytes()
    if expected_config_sha is not None and digest(previous_config) != expected_config_sha:
        raise RuntimeError('Existing configuration does not match the expected installation base')
    if (native_archive is None) != (native_manifest_sha is None):
        raise RuntimeError('Provide the new native archive and its manifest SHA-256 together')
    if (prepared_plan is None) != (prepared_plan_sha is None) or (prepared_plan is not None and native_archive is None):
        raise RuntimeError('A prepared plan requires its SHA-256 and the corresponding new native archive')
    config = json.loads(previous_config.decode('utf-8-sig'))
    if (len(previous_config) > 1048576 or config.get('schema') != 'pas.manual-song-panel-launch.v2'
            or config.get('manual_selection') is not True or config.get('manual_start') is not True
            or config.get('image_retention') != 'result'
            or config.get('capacity_policy') != 'configured-ten-contact-v1'
            or config.get('contact_capacity') != 10):
        raise RuntimeError('Expected the existing manual, result-only, ten-contact desktop configuration')

    # Read the existing engine identity, without running it or changing its archive.
    manifest_pin = config['build_manifest']
    engine_manifest = Path(manifest_pin['file'])
    if digest(engine_manifest.read_bytes()) != manifest_pin['sha256']:
        raise RuntimeError('Existing native archive manifest does not match the configuration')
    previous_archive = config['build_archive']
    previous_plan = config['plan']
    if native_archive is not None:
        archive = native_archive.resolve(strict=True)
        reference, executable_sha = native_pin(archive, native_manifest_sha)
        if prepared_plan is not None:
            plan, counts = prepared_plan_pin(prepared_plan, prepared_plan_sha, config, executable_sha)
            config['plan'] = config['library'] = plan
            config['library_counts'] = dict(config['library_counts'], **counts)
        config['build_archive'] = str(archive)
        config['build_manifest'] = reference
        config['desktop_round_summary'] = 'pas.desktop-round-summary.v1'
        config['manual_runtime_deployment'] = {
            'schema': 'pas.desktop-records-deployment.v1',
            'engine_sha256': executable_sha,
            'build_manifest': reference,
            'library': config['library'],
            'previous_configuration_sha256': digest(previous_config),
            'scope': 'desktop runtime and offline library; no gameplay validation',
        }
    bootstrap = (repository / 'tools/desktop-bootstrap.ps1').read_bytes()
    sources = {name: (repository / name).read_bytes() for name in MEMBERS}
    if any(not data or len(data) > 524288 for data in sources.values()) or sum(map(len, sources.values())) > 2097152:
        raise RuntimeError('Frontend source bundle exceeds its fixed size budget')

    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f') + '-' + uuid.uuid4().hex[:8]
    release = repository / 'local/desktop-app/releases' / stamp
    release.mkdir(parents=True, exist_ok=False)
    (release / 'tools').mkdir()
    artifacts = []
    for name, data in sources.items():
        target = release / name
        write_new(target, data)
        if target.read_bytes() != data:
            raise RuntimeError(f'Frontend copy was incomplete: {target}')
        artifacts.append({'file': name, 'sha256': digest(data), 'bytes': len(data),
                          'source': pin(repository / name, data)})
    manifest = {
        'schema': 'pas.desktop-frontend-release.v2',
        'version': stamp,
        'created_utc': datetime.now(timezone.utc).isoformat(),
        'runtime': MEMBERS[0],
        'artifacts': artifacts,
        'policy': 'Execute this complete snapshot; never refresh hashes automatically at startup.',
    }
    release_manifest = release / 'manifest.json'
    write_new(release_manifest, encoded(manifest))
    config['gui_script'] = pin(release / 'tools/sweep-panel.ps1')
    config['scripts'] = [pin(release / name, sources[name]) for name in MEMBERS]
    config['frontend_release'] = pin(release_manifest)
    config['frontend_policy'] = 'immutable-versioned-bundle-v1'
    config['history_python'] = str(Path(sys.executable).absolute())

    history = repository / 'local/manual-config-history'
    history.mkdir(exist_ok=True)
    config_backup = history / (stamp + '-before-frontend-install.json')
    launcher_backup = history / (stamp + '-before-frontend-install.ps1')
    write_new(config_backup, previous_config)
    write_new(launcher_backup, previous_launcher)
    following = encoded(config)
    if configuration.read_bytes() != previous_config or launcher.read_bytes() != previous_launcher:
        raise RuntimeError('Launch files changed during installation; the new release was not activated')
    replace_if_unchanged(launcher, previous_launcher, bootstrap)
    try:
        replace_if_unchanged(configuration, previous_config, following)
    except BaseException:
        # Keep the previous configuration and bootstrap as a consistent pair.
        replace_if_unchanged(launcher, bootstrap, previous_launcher)
        raise

    report = {
        'schema': 'pas.desktop-frontend-install.v1',
        'installed_utc': datetime.now(timezone.utc).isoformat(),
        'configuration': pin(configuration, following),
        'previous_configuration': pin(config_backup, previous_config),
        'bootstrap': pin(launcher, bootstrap),
        'previous_bootstrap': pin(launcher_backup, previous_launcher),
        'frontend_release': pin(release_manifest),
        'frontend_files': len(artifacts),
        'build_archive': config['build_archive'],
        'previous_build_archive': previous_archive,
        'build_manifest': config['build_manifest'],
        'plan': config['plan'],
        'previous_plan': previous_plan,
        'gui_started': False,
        'device_access': False,
        'tests': 'Installer does not execute tests; separate validation receipts retain their own scope.',
        'previous_five_random_task': 'Still paused; not resumed by this frontend repair.',
    }
    if previous_plan == config['plan']:
        report['plan_unchanged'] = config['plan']
    report_path = history / (stamp + '-frontend-install-report.json')
    write_new(report_path, encoded(report))
    return {'configuration': str(configuration), 'release': str(release), 'report': str(report_path),
            'gui_started': False, 'device_access': False, 'tests': report['tests']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--expected-config-sha256')
    parser.add_argument('--native-archive', type=Path)
    parser.add_argument('--native-manifest-sha256')
    parser.add_argument('--prepared-plan', type=Path)
    parser.add_argument('--prepared-plan-sha256')
    args = parser.parse_args()
    print(json.dumps(install(args.repository, args.expected_config_sha256, args.native_archive,
                             args.native_manifest_sha256, args.prepared_plan,
                             args.prepared_plan_sha256), ensure_ascii=False, indent=2))
