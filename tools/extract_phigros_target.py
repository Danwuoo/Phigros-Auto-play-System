"""Extract one explicitly selected chart/audio pair from fingerprinted local APKs.

Offline, opt-in only. Catalog address -> dependency -> Unity container key is
authoritative; object names and note counts never select a target. UnityPy 1.25.4
is used only after bounded UnityFS preflight. No game assets belong in Git.
"""
import argparse
from collections import Counter
import hashlib
import importlib.metadata
import io
import json
import math
from pathlib import Path
import platform
import struct
import wave
import zipfile

from inspect_phigros_assets import inventory_apks
from unity_catalog import Catalog
from unity_bundle_guard import preflight_unityfs

MIB = 1024 * 1024
RUNTIME_PATH = "{UnityEngine.AddressableAssets.Addressables.RuntimePath}"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def hash_file(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_new(path, data):
    with Path(path).open("xb") as stream:
        stream.write(data)
    return dict(file=Path(path).name, bytes=len(data), sha256=sha256(data))


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")


def unique(values, label):
    if len(values) != 1:
        raise ValueError(f"{label}: expected exactly one candidate, got {len(values)}")
    return values[0]


def textasset_bytes(raw, endian="<"):
    """Extract raw m_Script bytes from the verified release TextAsset layout.

    Unity 2022 release TextAsset = aligned length-prefixed name, then script.
    No string roundtrip is used for the saved chart. Caller independently checks
    the name and script against UnityPy's typed parse and rejects other layouts.
    """
    if endian not in ("<", ">") or len(raw) < 8:
        raise ValueError("Unsupported TextAsset layout")
    name_size = struct.unpack_from(endian + "i", raw)[0]
    if not 0 <= name_size <= 4096 or 4 + name_size > len(raw):
        raise ValueError("Invalid TextAsset name length")
    name = raw[4:4 + name_size].decode("utf-8")
    at = (4 + name_size + 3) & ~3
    if at + 4 > len(raw):
        raise ValueError("Missing TextAsset script length")
    size = struct.unpack_from(endian + "i", raw, at)[0]
    end = at + 4 + size
    if size < 0 or end > len(raw) or len(raw) != ((end + 3) & ~3):
        raise ValueError("Invalid TextAsset script length or unsupported trailing fields")
    if any(raw[end:]):
        raise ValueError("Unsupported TextAsset trailing data")
    return name, raw[at + 4:end], at + 4


def chart_summary(raw):
    """M0 raw structural inventory, NOT an M1 semantic importer."""
    chart = json.loads(raw.decode("utf-8"), parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
    if chart.get("formatVersion") != 3:
        raise ValueError("M0 target summary only supports verified formatVersion 3")
    lines = chart["judgeLineList"]
    if not isinstance(lines, list) or len(lines) > 4096:
        raise ValueError("Invalid/oversized judgeLineList")
    counts, side_counts, per_line, provisional = Counter(), Counter(), [], []
    for index, line in enumerate(lines):
        bpm = line["bpm"]
        if isinstance(bpm, bool) or not isinstance(bpm, (int, float)) or not math.isfinite(bpm) or bpm <= 0:
            raise ValueError("Invalid BPM")
        notes = []
        for side in ("notesAbove", "notesBelow"):
            values = line[side]
            side_counts[side] += len(values)
            notes += values
            for note in values:
                if type(note["type"]) is not int or note["type"] not in (1, 2, 3, 4):
                    raise ValueError("Unsupported note type")
                counts[note["type"]] += 1
                if sum(counts.values()) > 200000:
                    raise ValueError("Note budget exceeded")
                # Kept explicitly provisional until the M1 time-contract checks.
                provisional.append((note["time"] * 1.875 / bpm,
                                    (note["time"] + note["holdTime"]) * 1.875 / bpm))
        ticks = [n["time"] for n in notes]
        per_line.append(dict(index=index, bpm=bpm, notes=len(notes),
                             note_tick_min=min(ticks) if ticks else None,
                             note_tick_max=max(ticks) if ticks else None,
                             events={key: len(line[key]) for key in ("speedEvents", "judgeLineMoveEvents",
                                     "judgeLineRotateEvents", "judgeLineDisappearEvents")}))
    return dict(dialect="phigros-official-json", format_version=chart["formatVersion"],
                offset_raw=chart["offset"], line_count=len(lines), note_count=sum(counts.values()),
                note_types={str(k): counts[k] for k in sorted(counts)}, sides=dict(side_counts),
                per_line=per_line, provisional_note_seconds_before_offset={
                    "first": min((x[0] for x in provisional), default=None),
                    "last_start": max((x[0] for x in provisional), default=None),
                    "last_end": max((x[1] for x in provisional), default=None)},
                validation="M0 structural counts only; native time/offset semantics are verified separately in M1.")


def verify_installed_source(source, inventory):
    if source.get("package") != "com.PigeonGames.Phigros" or source.get("version_name") != "4.0.1" or source.get("version_code") != 157:
        raise ValueError("This extraction target requires observed Phigros 4.0.1 / 157")
    expected = {entry["file"]: entry for entry in source["apk"]}
    actual = {entry["name"]: entry for entry in inventory["packages"]}
    if len(expected) != len(source["apk"]) or len(actual) != len(inventory["packages"]) or actual.keys() != expected.keys():
        raise ValueError("Missing, duplicate, or additional installed APK")
    for name, record in actual.items():
        if (record["bytes"], record["sha256"]) != (expected[name]["bytes"], expected[name]["sha256"]):
            raise ValueError(f"Installed APK fingerprint mismatch: {name}")


def asset_report(summary, chart_key, audio_key, *, chart_candidates, audio_candidates,
                 catalog_entries, scanned_apk_entries, selected_bundles, difficulty="IN"):
    """Describe the selected native target without inheriting another song's UI evidence."""
    return dict(schema_version=1, status="extracted", chart_summary=summary,
                target=dict(track_id=chart_key.split("/")[-2], difficulty=difficulty,
                            chart_catalog_key=chart_key, audio_catalog_key=audio_key,
                            displayed_level=None),
                exact_key_candidates=dict(chart=chart_candidates, audio=audio_candidates),
                catalog_entries=catalog_entries, scanned_apk_entries=scanned_apk_entries,
                selected_bundles=selected_bundles, failures=[], unsupported=[],
                limitations=["M0 does not validate time/geometry/gameplay semantics.",
                             f"Difficulty {difficulty} comes from the exact Chart_{difficulty} address; displayed level requires independent UI evidence.",
                             "The reported note count is a structural cross-check, not the asset selection rule."])


class AssetExtractionSession:
    """One verified offline APK/catalog session, reused for explicit targets.

    APKs are fingerprinted once at entry and once after the entire batch. Bundle
    payloads are bounded and cached; Unity environments are not retained between
    selections. This does not discover tracks, choose a chapter or access devices.
    """
    def __init__(self, apks, installed_source, *, max_cached_bundle_bytes=512 * MIB):
        if not 1 <= max_cached_bundle_bytes <= 1024 * MIB:
            raise ValueError("Bundle cache budget must be within 1..1024 MiB")
        self.installed_source = Path(installed_source)
        if self.installed_source.stat().st_size > MIB:
            raise ValueError("Installed source observation exceeds finite metadata budget")
        source_bytes = self.installed_source.read_bytes()
        self.installed_source_sha256 = sha256(source_bytes)
        self.source = json.loads(source_bytes.decode("utf-8-sig"))
        self.inventory = inventory_apks(apks)
        verify_installed_source(self.source, self.inventory)
        import UnityPy
        if UnityPy.__version__ != "1.25.4":
            raise ValueError("Use the verified UnityPy 1.25.4 reader")
        self.unity = UnityPy
        self.members, self.bundle_cache = {}, {}
        self.cached_bundle_bytes = 0
        self.max_cached_bundle_bytes = max_cached_bundle_bytes
        for package in self.inventory["packages"]:
            for entry in package["entries"]:
                if entry["type"] == "file":
                    self.members.setdefault(entry["path"], []).append((package, entry))
        self.catalog_bytes, self.catalog_origin = self.read_entry("assets/aa/catalog.json", 16 * MIB)
        self.catalog = Catalog.from_bytes(self.catalog_bytes)

    def read_entry(self, member, max_bytes=64 * MIB):
        package, info = unique(self.members.get(member, []), f"APK member {member}")
        if info["bytes"] > max_bytes:
            raise ValueError("Selected ZIP member exceeds extraction budget")
        with zipfile.ZipFile(package["path"]) as archive:
            with archive.open(member) as stream:
                data = stream.read(max_bytes + 1)
        if len(data) != info["bytes"] or len(data) > max_bytes:
            raise ValueError("Selected ZIP member changed size or exceeds budget")
        return data, dict(apk=package["name"], apk_sha256=package["sha256"], zip_entry=member,
                          bytes=len(data), sha256=sha256(data), zip_crc_checked=True)

    def select(self, key, expected_type):
        entry = unique(self.catalog.lookup(key), f"catalog key {key}")
        if entry.resource_type.get("m_ClassName") != "UnityEngine." + expected_type:
            raise ValueError("Catalog type does not match requested asset")
        if entry.provider != "UnityEngine.ResourceManagement.ResourceProviders.BundledAssetProvider":
            raise ValueError("Unsupported asset provider")
        bundles = []
        for dependency in self.catalog.dependency_closure(entry):
            if dependency.provider != "UnityEngine.ResourceManagement.ResourceProviders.AssetBundleProvider":
                raise ValueError("Unsupported non-bundle dependency")
            internal = dependency.expanded_internal_id
            if not internal.startswith(RUNTIME_PATH + "/Android/"):
                raise ValueError("Only local Android RuntimePath bundles are supported")
            member = internal.replace(RUNTIME_PATH, "assets/aa", 1)
            if member not in self.bundle_cache:
                data, origin = self.read_entry(member)
                guard = preflight_unityfs(data)
                if self.cached_bundle_bytes + len(data) > self.max_cached_bundle_bytes:
                    raise ValueError("Selected native bundle cache exceeds finite budget")
                self.cached_bundle_bytes += len(data)
                self.bundle_cache[member] = (data, origin, guard)
            bundles.append((dependency, member, *self.bundle_cache[member]))
        if not bundles:
            raise ValueError("Asset has no local bundle dependency")
        matches = []
        for dependency, member, data, origin, guard in bundles:
            environment = self.unity.load(data)
            for container_key, pointer in environment.container.items():
                if container_key == entry.expanded_internal_id:
                    reader = pointer.deref() if hasattr(pointer, "deref") else pointer
                    if reader.type.name == expected_type:
                        matches.append((reader, origin, guard, dependency, container_key, environment))
        reader, origin, guard, dependency, container_key, environment = unique(matches, f"Unity container {key}")
        parsed = reader.parse_as_object()
        raw = reader.get_raw_data()
        origin = dict(origin)
        origin.update(catalog_entry=entry.index, catalog_key=key,
                      catalog_internal_id=entry.expanded_internal_id,
                      catalog_dependency_entries=[d.index for d in self.catalog.dependency_closure(entry)],
                      bundle_dependency_entry=dependency.index, container_key=container_key,
                      serialized_file=reader.assets_file.name, unity_version=reader.assets_file.unity_version,
                      path_id=str(reader.path_id), object_type=reader.type.name, object_name=parsed.m_Name,
                      object_byte_offset=reader.byte_start, object_bytes=len(raw), object_sha256=sha256(raw),
                      bundle_preflight=guard)
        return reader, parsed, raw, origin, environment

    def verify_sources_unchanged(self):
        for package in self.inventory["packages"]:
            if hash_file(package["path"]) != package["sha256"]:
                raise ValueError("APK changed during extraction")
        if hash_file(self.installed_source) != self.installed_source_sha256:
            raise ValueError("Installed source observation changed during extraction")


def extract_chart_asset(session, chart_key, output, *, difficulty="IN", raw_name="chart.textasset.bin",
                        writer=write_new, allow_unsupported_summary=False):
    """Write exact raw TextAsset bytes and a structural cross-check, never semantics."""
    if chart_key.rsplit("/", 1)[-1] != f"Chart_{difficulty}.json":
        raise ValueError("Chart catalog suffix and requested difficulty must match exactly")
    chart_reader, chart_obj, chart_raw, chart_origin, chart_env = session.select(chart_key, "TextAsset")
    name, script, script_offset = textasset_bytes(chart_raw, chart_reader.reader.endian)
    if name != chart_obj.m_Name or script != chart_obj.m_Script.encode("utf-8", "surrogateescape"):
        raise ValueError("Raw TextAsset slice and UnityPy typed reader disagree")
    try:
        summary = chart_summary(script)
    except (ValueError, KeyError, TypeError) as error:
        if not allow_unsupported_summary:
            raise
        summary = dict(status="unsupported_structural_summary", reason=str(error),
                       validation="Native bytes archived exactly; no parser, timing, geometry or gameplay support claimed.")
    artifact = writer(Path(output) / f"Chart_{difficulty}.json", script)
    artifact.update(classification="native", encoding="utf-8", byte_transform="none; exact raw m_Script slice",
                    textasset_script_byte_offset=script_offset, source=chart_origin)
    writer(Path(output) / raw_name, chart_raw)
    return artifact, summary


def extract_audio_asset(session, audio_key, output, *, writer=write_new):
    """One exact FSB5 resource and one uncropped decoded WAV for a selected song."""
    from UnityPy.helpers.ResourceReader import get_resource_data
    audio_reader, audio_obj, audio_raw, audio_origin, audio_env = session.select(audio_key, "AudioClip")
    if (not math.isfinite(audio_obj.m_Length) or not 0 < audio_obj.m_Length <= 600
            or not 1 <= audio_obj.m_Channels <= 8 or not 8000 <= audio_obj.m_Frequency <= 192000
            or audio_obj.m_Length * audio_obj.m_Channels * audio_obj.m_Frequency * 4 > 512 * MIB):
        raise ValueError("Audio metadata exceeds decode budget")
    resource = audio_obj.m_Resource
    if audio_obj.m_AudioData or resource is None:
        raise ValueError("Expected verified streamed AudioClip resource layout")
    if resource.m_Offset < 0 or not 0 < resource.m_Size <= 64 * MIB:
        raise ValueError("Audio resource bounds invalid")
    resource_name = resource.m_Source.rsplit("/", 1)[-1]
    resource_reader = audio_env.get_cab(resource_name)
    if resource_reader is None or resource.m_Offset + resource.m_Size > resource_reader.Length:
        raise ValueError("Audio resource missing or truncated in selected bundle")
    native_audio = get_resource_data(resource.m_Source, audio_reader.assets_file, resource.m_Offset, resource.m_Size)
    if not native_audio.startswith(b"FSB5") or len(native_audio) != resource.m_Size:
        raise ValueError("Expected exact FSB5 audio resource")
    artifact = writer(Path(output) / "music.fsb", native_audio)
    artifact.update(classification="native", source=audio_origin,
                    resource=dict(source=resource.m_Source, offset=resource.m_Offset, bytes=resource.m_Size),
                    frequency=audio_obj.m_Frequency, channels=audio_obj.m_Channels,
                    length_metadata_seconds=audio_obj.m_Length, compression_format=audio_obj.m_CompressionFormat)
    writer(Path(output) / "music.audioclip.bin", audio_raw)
    decoded_name, decoded = unique(list(audio_obj.samples.items()), "decoded audio sample")
    if len(decoded) > 512 * MIB:
        raise ValueError("Decoded WAV exceeds budget")
    with wave.open(io.BytesIO(decoded)) as wav:
        if wav.getcomptype() != "NONE" or wav.getnchannels() != audio_obj.m_Channels or wav.getframerate() != audio_obj.m_Frequency:
            raise ValueError("Decoded WAV format disagrees with AudioClip")
        frames = wav.getnframes()
        pcm = wav.readframes(frames)
        if len(pcm) != frames * wav.getnchannels() * wav.getsampwidth():
            raise ValueError("Decoded WAV is truncated")
        decoded_artifact = dict(classification="derived", operation="UnityPy AudioClip.samples / FSB5 decode; no crop or resample",
                               source_sha256=sha256(native_audio), sample_rate=wav.getframerate(),
                               channels=wav.getnchannels(), sample_width_bytes=wav.getsampwidth(), sample_frames=frames,
                               duration_seconds=frames / wav.getframerate(), pcm_sha256=sha256(pcm),
                               pcm_layout="interleaved signed little-endian integer", reader_output_name=decoded_name)
    decoded_artifact.update(writer(Path(output) / "music.wav", decoded))
    return artifact, decoded_artifact


def extract(apks, installed_source, output, chart_key, audio_key):
    output = Path(output)
    if output.exists():
        raise FileExistsError("Extraction output must be a new directory")
    if (not chart_key.startswith("Assets/Tracks/") or not chart_key.endswith("/Chart_IN.json")
            or audio_key != chart_key.rsplit("/", 1)[0] + "/music.wav"):
        raise ValueError("Require IN chart and music addresses from exactly the same track")
    session = AssetExtractionSession(apks, installed_source)
    source, inventory, catalog = session.source, session.inventory, session.catalog
    output.mkdir(parents=True, exist_ok=False)
    write_new(output / "catalog.json", session.catalog_bytes)
    chart_artifact, summary = extract_chart_asset(session, chart_key, output)
    audio_artifact, audio_decode = extract_audio_asset(session, audio_key, output)
    for member, (data, origin, guard) in session.bundle_cache.items():
        write_new(output / Path(member).name, data)
    session.verify_sources_unchanged()
    manifest = dict(schema_version=1, package=source["package"], version_name=source["version_name"],
                    version_code=source["version_code"], track_id=chart_key.split("/")[-2], difficulty="IN",
                    catalog=session.catalog_origin, packages=[{k: p[k] for k in ("name", "bytes", "sha256")} for p in inventory["packages"]],
                    installed_source_sha256=session.installed_source_sha256, chart=chart_artifact,
                    native_audio=audio_artifact, decoded_audio=audio_decode,
                    tools=dict(python=platform.python_version(), dependencies={name: importlib.metadata.version(name)
                               for name in ("UnityPy", "fmod_toolkit", "pyfmodex", "lz4")},
                               scripts={name: hash_file(Path(__file__).with_name(name)) for name in
                                        ("extract_phigros_target.py", "inspect_phigros_assets.py", "unity_catalog.py", "unity_bundle_guard.py")}),
                    scope="Local authorized installed assets; no upload or in-game input; native media excluded from Git.")
    report = asset_report(summary, chart_key, audio_key,
                          chart_candidates=len(catalog.lookup(chart_key)),
                          audio_candidates=len(catalog.lookup(audio_key)),
                          catalog_entries=len(catalog.entries),
                          scanned_apk_entries=inventory["total_entries"], selected_bundles=len(session.bundle_cache))
    write_new(output / "source-manifest.json", json_bytes(manifest))
    write_new(output / "asset-report.json", json_bytes(report))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apks", nargs="+", type=Path)
    parser.add_argument("--installed-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--chart-key", default="Assets/Tracks/Dlyrotz.Likey.0/Chart_IN.json")
    parser.add_argument("--audio-key", default="Assets/Tracks/Dlyrotz.Likey.0/music.wav")
    args = parser.parse_args()
    result = extract(args.apks, args.installed_source, args.output, args.chart_key, args.audio_key)
    print(json.dumps(dict(status=result["status"], chart=result["chart_summary"]), ensure_ascii=False))


if __name__ == "__main__":
    main()
