"""Install a versioned desktop frontend without starting the app or a device.

This updates only the frontend references. Native binaries, chart/profile pins,
calibration and all playback settings remain exactly as configured.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import uuid


MEMBERS = (
    'tools/desktop-start-runtime.ps1',
    'tools/sweep-panel.ps1',
    'tools/catalog-panel-core.ps1',
    'tools/song-library-panel.ps1',
    'tools/start-song-library.ps1',
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


def install(repository: Path) -> dict:
    repository = repository.resolve(strict=True)
    configuration = repository / 'local/manual-launch.json'
    launcher = repository / 'local/start-phigros.ps1'
    previous_config = configuration.read_bytes()
    previous_launcher = launcher.read_bytes()
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
        'schema': 'pas.desktop-frontend-release.v1',
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
        'build_archive_unchanged': config['build_archive'],
        'build_manifest_unchanged': config['build_manifest'],
        'plan_unchanged': config['plan'],
        'gui_started': False,
        'device_access': False,
        'tests': 'Installer does not execute tests; separate validation receipts retain their own scope.',
        'previous_five_random_task': 'Still paused; not resumed by this frontend repair.',
    }
    report_path = history / (stamp + '-frontend-install-report.json')
    write_new(report_path, encoded(report))
    return {'configuration': str(configuration), 'release': str(release), 'report': str(report_path),
            'gui_started': False, 'device_access': False, 'tests': report['tests']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    print(json.dumps(install(args.repository), ensure_ascii=False, indent=2))
