"""Archive a proven native Phigros chapter from authorized local APKs, offline.

One sequential reader, one verified catalog, all exact Chart_* addresses, one
native/decoded audio set per song. Existing outputs are never overwritten.
Native bytes stay outside Git; this inventory grants no gameplay qualification.
"""
import argparse
import gc
import importlib.metadata
import json
from pathlib import Path, PurePosixPath
import platform
import re
import struct

from extract_phigros_target import (AssetExtractionSession, MIB, asset_report,
    extract_audio_asset, extract_chart_asset, hash_file, json_bytes, sha256, unique, write_new)
from unity_bundle_guard import preflight_unityfs

SCHEMA = "pas.phigros-chapter-archive.v1"
MEMBERSHIP_SCHEMA = "pas.phigros-chapter-membership.v1"
STANDARD_DIFFICULTIES = ("EZ", "HD", "IN", "AT")


def safe_leaf(value):
    """Windows-safe exact Unicode leaf; preserve native spelling and bytes."""
    if (not isinstance(value, str) or not value or len(value) > 180 or value in (".", "..")
            or value[-1] in " ." or any(c in '<>:"/\\|?*' or ord(c) < 32 or ord(c) == 127 for c in value)
            or re.match(r"(?i)^(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])(?:\.|$)", value)):
        raise ValueError("Unsafe native file/directory leaf")
    value.encode("utf-8")
    return value


def aligned_string(raw, offset):
    if offset < 0 or offset + 4 > len(raw):
        raise ValueError("Truncated native chapter string length")
    length = struct.unpack_from("<i", raw, offset)[0]
    if not 0 <= length <= 4096 or offset + 4 + length > len(raw):
        raise ValueError("Invalid native chapter string length")
    end = offset + 4 + length
    aligned_end = (end + 3) & ~3
    if aligned_end > len(raw) or any(raw[end:aligned_end]):
        raise ValueError("Invalid native chapter string alignment")
    return raw[offset + 4:end].decode("utf-8"), aligned_end


def validate_native_record(membership, files):
    provenance = membership["native_provenance"]
    objects = [data for name, data in files.items() if sha256(data) == provenance["object_sha256"]]
    raw = unique(objects, "native chapter object proof")
    if len(raw) != provenance["object_bytes"]:
        raise ValueError("Native object proof byte count mismatch")
    begin, end = provenance["chapter_record_range_in_object"]
    if not 0 <= begin < end <= len(raw) or sha256(raw[begin:end]) != provenance["chapter_record_sha256"]:
        raise ValueError("Native chapter record bounds/hash mismatch")
    cursor = begin
    for expected in provenance["chapter_identity_fields"]:
        if cursor != expected["length_offset"]:
            raise ValueError("Native chapter identity offset mismatch")
        text, cursor = aligned_string(raw, cursor)
        if text != expected["value"]:
            raise ValueError("Native chapter identity does not match its proof")
    if provenance["chapter_identity_fields"][0]["value"] != membership["chapter_id"]:
        raise ValueError("Native chapter identity differs from requested chapter")
    if cursor != provenance["native_song_count_offset"] or cursor + 4 > end:
        raise ValueError("Native chapter song-count offset mismatch")
    count = struct.unpack_from("<i", raw, cursor)[0]
    cursor += 4
    if count != membership["song_count"] or count != len(membership["songs"]) or not 1 <= count <= 64:
        raise ValueError("Native chapter count differs from requested membership")
    for order, song in enumerate(membership["songs"], 1):
        if song["order"] != order or song["source_index"] != order - 1:
            raise ValueError("Native chapter song order changed")
        source = song["source_metadata"]
        if cursor != source["track_length_offset"]:
            raise ValueError("Native track record offset mismatch")
        track, cursor = aligned_string(raw, cursor)
        if track != song["track_id"] or cursor + 16 > end:
            raise ValueError("Native track identity differs from requested membership")
        fields = list(struct.unpack_from("<4i", raw, cursor))
        cursor += 16
        if fields != source["native_trailing_i32_fields"] or cursor != source["entry_end_offset"]:
            raise ValueError("Native track trailing fields/order changed")
    if cursor != end:
        raise ValueError("Native chapter record has unconsumed bytes")
    next_identity = provenance.get("next_chapter_identity")
    if next_identity and aligned_string(raw, next_identity["length_offset"])[0] != next_identity["value"]:
        raise ValueError("Next native chapter boundary changed")
    return raw


def load_membership(path):
    path = Path(path).resolve()
    if path.stat().st_size > 16 * MIB:
        raise ValueError("Chapter membership proof exceeds finite budget")
    data = path.read_bytes()
    proof = json.loads(data.decode("utf-8-sig"))
    if (proof.get("schema") != MEMBERSHIP_SCHEMA or proof.get("package") != "com.PigeonGames.Phigros"
            or proof.get("version_name") != "4.0.1" or proof.get("version_code") != 157):
        raise ValueError("Unsupported chapter membership/revision proof")
    safe_leaf(proof["chapter_id"])
    songs = proof["songs"]
    if not isinstance(songs, list) or not 1 <= len(songs) <= 64 or len(songs) != proof["song_count"]:
        raise ValueError("Invalid chapter song budget/count")
    track_ids = [safe_leaf(song["track_id"]) for song in songs]
    if len({track.casefold() for track in track_ids}) != len(track_ids):
        raise ValueError("Duplicate or case-colliding native chapter track IDs")
    sources = proof["proof_sources"]
    if not isinstance(sources, list) or not 1 <= len(sources) <= 16:
        raise ValueError("Missing or oversized chapter proof source set")
    files, total = {}, 0
    for item in sources:
        relative = PurePosixPath(item["file"])
        if relative.is_absolute() or ".." in relative.parts or "\\" in item["file"] or not relative.parts:
            raise ValueError("Proof source must be relative within its own directory")
        source = path.parent.joinpath(*relative.parts).resolve()
        if not source.is_relative_to(path.parent):
            raise ValueError("Proof source escaped membership directory")
        if (type(item.get("bytes")) is not int or not 0 < item["bytes"] <= 64 * MIB
                or source.stat().st_size != item["bytes"] or total + item["bytes"] > 64 * MIB):
            raise ValueError("Chapter proof source declared size or total budget invalid")
        raw = source.read_bytes()
        total += len(raw)
        if len(raw) != item["bytes"] or sha256(raw) != item["sha256"] or total > 64 * MIB:
            raise ValueError("Chapter proof source size/hash/budget mismatch")
        if item["file"] in files:
            raise ValueError("Duplicate chapter proof source")
        files[item["file"]] = raw
    validate_native_record(proof, files)
    return proof, data, files


def catalog_charts(catalog, track_id):
    safe_leaf(track_id)
    prefix = f"Assets/Tracks/{track_id}/"
    charts = []
    for key in catalog.keys:
        if key.kind != "string" or not isinstance(key.value, str) or not key.value.startswith(prefix):
            continue
        tail = key.value[len(prefix):]
        if not tail.startswith("Chart_") or not tail.endswith(".json"):
            continue
        difficulty = safe_leaf(tail[6:-5])
        if len(difficulty) > 24 or "/" in tail:
            raise ValueError("Unsupported native chart address suffix")
        entry = unique(catalog.lookup(key.value), f"native chart address {key.value}")
        if entry.resource_type.get("m_ClassName") != "UnityEngine.TextAsset":
            raise ValueError("Native chart address is not a TextAsset")
        charts.append(dict(difficulty=difficulty, catalog_key=key.value, catalog_entry=entry.index,
                           classification="difficulty" if difficulty in STANDARD_DIFFICULTIES else "asset_variant",
                           ui_selectability="unverified"))
    if not charts or len(charts) > 8 or len({x["difficulty"].casefold() for x in charts}) != len(charts):
        raise ValueError("Missing, oversized or case-colliding chart difficulty inventory")
    return sorted(charts, key=lambda item: (STANDARD_DIFFICULTIES.index(item["difficulty"])
        if item["difficulty"] in STANDARD_DIFFICULTIES else 99, item["difficulty"]))


def verify_native_source(session, membership, files):
    native = membership["native_provenance"]
    data, origin = session.read_entry(native["zip_member"])
    if origin["apk_sha256"] != native["apk_sha256"] or sha256(data) != native["bundle_sha256"]:
        raise ValueError("Native chapter proof does not bind to these authorized APKs")
    preflight_unityfs(data)
    environment = session.unity.load(data)
    readers = list(environment.objects)
    if len(readers) > 100000:
        raise ValueError("Native chapter bundle object budget exceeded")
    reader = unique([r for r in readers if r.type.name == "MonoBehaviour" and r.path_id == native["object_path_id"]
                     and r.assets_file.name == native["serialized_node"]], "native chapter MonoBehaviour")
    raw = reader.get_raw_data()
    if sha256(raw) != native["object_sha256"] or len(raw) != native["object_bytes"] or reader.byte_start != native["object_byte_offset_in_node"]:
        raise ValueError("Native chapter object bytes/source offset differ from proof")
    script = native["script_pointer"]
    # The stripped custom type tree describes only the 32-byte MonoBehaviour
    # prefix. Read its script pointer explicitly; the complete raw object hash
    # and the separately bounded chapter-record reader validate the custom tail.
    object_script = reader.parse_as_object(check_read=False).m_Script
    if object_script.m_FileID != script["file_id"] or object_script.m_PathID != script["path_id"]:
        raise ValueError("Native chapter MonoBehaviour points to a different script")
    script_reader = unique([r for r in readers if r.type.name == "MonoScript" and r.path_id == script["path_id"]
                            and r.assets_file.name == script["serialized_file"]], "native GameInformation script")
    parsed = script_reader.parse_as_object()
    if (parsed.m_ClassName != "GameInformation" or parsed.m_Namespace != script["parsed_namespace"]
            or parsed.m_AssemblyName != script["parsed_assembly_name"]):
        raise ValueError("Native chapter object script identity changed")
    if sha256(raw) not in {sha256(blob) for blob in files.values()}:
        raise ValueError("Native chapter object is absent from proof files")


class ArchiveWriter:
    def __init__(self, root, max_bytes=4096 * MIB):
        self.root = Path(root).resolve()
        if not 1 <= max_bytes <= 8192 * MIB:
            raise ValueError("Archive output budget must be within 1..8192 MiB")
        self.max_bytes, self.bytes, self.records = max_bytes, 0, {}

    def write(self, path, data):
        path = Path(path).resolve()
        if not path.is_relative_to(self.root) or len(data) > self.max_bytes - self.bytes:
            raise ValueError("Archive path or total output byte budget exceeded")
        relative = path.relative_to(self.root).as_posix()
        if relative in self.records:
            raise FileExistsError("Archive artifact already written")
        path.parent.mkdir(parents=True, exist_ok=True)
        artifact = write_new(path, data)
        self.bytes += len(data)
        self.records[relative] = dict(artifact, file=relative)
        return artifact

    def reference(self, path):
        return dict(self.records[Path(path).resolve().relative_to(self.root).as_posix()])


def archive_chapter(apks, installed_source, membership_path, output, *, progress=None,
                    session_factory=AssetExtractionSession, max_output_bytes=4096 * MIB):
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError("Chapter archive output must be a new directory")
    membership, membership_bytes, proof_files = load_membership(membership_path)
    session = session_factory(apks, installed_source)
    if (membership["catalog_sha256"] != sha256(session.catalog_bytes)
            or membership["installed_source_sha256"] != session.installed_source_sha256):
        raise ValueError("Chapter membership/catalog/installed-source identities differ")
    verify_native_source(session, membership, proof_files)
    gc.collect()
    targets = []
    for song in membership["songs"]:
        charts = catalog_charts(session.catalog, song["track_id"])
        if {c["catalog_key"] for c in charts} != {c["catalog_key"] for c in song["charts"]}:
            raise ValueError("Available native chart inventory differs from membership proof")
        audio_key = f"Assets/Tracks/{song['track_id']}/music.wav"
        audio = unique(session.catalog.lookup(audio_key), f"native audio {audio_key}")
        if audio.resource_type.get("m_ClassName") != "UnityEngine.AudioClip" or song["audio"]["catalog_key"] != audio_key:
            raise ValueError("Native chapter song audio identity/type differs")
        targets.append((song, charts, audio_key))
    writer = ArchiveWriter(output, max_output_bytes)
    failures, songs = [], []
    tools = dict(python=platform.python_version(), dependencies={name: importlib.metadata.version(name)
                 for name in ("UnityPy", "fmod_toolkit", "pyfmodex", "lz4")},
                 scripts={name: hash_file(Path(__file__).with_name(name)) for name in
                    ("archive_phigros_chapter.py", "extract_phigros_target.py", "inspect_phigros_assets.py", "unity_catalog.py", "unity_bundle_guard.py")})
    output.mkdir(parents=True, exist_ok=False)
    try:
        writer.write(output / "membership-proof.json", membership_bytes)
        for name, raw in proof_files.items():
            writer.write(output / "proof" / name, raw)
        writer.write(output / "catalog.json", session.catalog_bytes)
        writer.write(output / "apk-inventory.json", json_bytes(session.inventory))
        packages = [{k: p[k] for k in ("name", "bytes", "sha256")} for p in session.inventory["packages"]]
        for song, charts, audio_key in targets:
            track = song["track_id"]
            folder = output / "songs" / track
            folder.mkdir(parents=True, exist_ok=False)
            native_audio, decoded_audio = extract_audio_asset(session, audio_key, folder, writer=writer.write)
            common = dict(schema_version=1, package=session.source["package"], version_name=session.source["version_name"],
                          version_code=session.source["version_code"], track_id=track, catalog=session.catalog_origin,
                          packages=packages, installed_source_sha256=session.installed_source_sha256,
                          native_audio=native_audio, decoded_audio=decoded_audio, tools=tools,
                          scope="Authorized local native assets, exact chart bytes and one audio set per song; no gameplay qualification or input.")
            chart_rows = []
            for target in charts:
                difficulty = target["difficulty"]
                artifact, summary = extract_chart_asset(session, target["catalog_key"], folder,
                    difficulty=difficulty, raw_name=f"chart-{difficulty}.textasset.bin", writer=writer.write,
                    allow_unsupported_summary=True)
                if summary.get("status") == "unsupported_structural_summary":
                    failures.append(dict(code="unsupported_chart_structure", track_id=track, difficulty=difficulty, reason=summary["reason"]))
                report = asset_report(summary, target["catalog_key"], audio_key, chart_candidates=1, audio_candidates=1,
                    catalog_entries=len(session.catalog.entries), scanned_apk_entries=session.inventory["total_entries"],
                    selected_bundles=len(session.bundle_cache), difficulty=difficulty)
                report["target"].update(classification=target["classification"], ui_selectability="unverified")
                source_path = folder / f"source-manifest-{difficulty}.json"
                writer.write(source_path, json_bytes(dict(common, difficulty=difficulty, chart=artifact,
                    chapter_id=membership["chapter_id"], chapter_order=song["order"],
                    asset_classification=target["classification"], ui_selectability="unverified")))
                report_path = folder / f"asset-report-{difficulty}.json"
                writer.write(report_path, json_bytes(report))
                chart_rows.append(dict(target, chart=writer.reference(folder / artifact["file"]),
                    source_manifest=writer.reference(source_path), asset_report=writer.reference(report_path), chart_summary=summary))
            song_manifest = folder / "source-manifest.json"
            writer.write(song_manifest, json_bytes(dict(common, schema="pas.phigros-song-archive.v1",
                chapter_id=membership["chapter_id"], chapter_order=song["order"], charts=chart_rows,
                membership_proof_sha256=sha256(membership_bytes))))
            songs.append(dict(track_id=track, order=song["order"], display_name=song.get("display_name"),
                available_difficulties=[c["difficulty"] for c in charts], charts=chart_rows,
                native_audio=writer.reference(folder / "music.fsb"), decoded_audio=writer.reference(folder / "music.wav"),
                source_manifest=writer.reference(song_manifest)))
            if progress:
                progress(dict(event="song_archived", order=song["order"], total=len(targets), track_id=track,
                              difficulties=[c["difficulty"] for c in charts], decoded_audio_sha256=decoded_audio["sha256"]))
            gc.collect()
        bundles = []
        for member, (data, origin, guard) in session.bundle_cache.items():
            bundle = output / "bundles" / safe_leaf(Path(member).name)
            writer.write(bundle, data)
            bundles.append(dict(writer.reference(bundle), source=origin, preflight=guard))
        session.verify_sources_unchanged()
        index = dict(schema=SCHEMA, status="complete" if not failures else "incomplete", offline_only=True,
            chapter_id=membership["chapter_id"], chapter_name=membership["chapter_name"], package=session.source["package"],
            version_name=session.source["version_name"], version_code=session.source["version_code"],
            membership_proof=writer.reference(output / "membership-proof.json"), membership_native_bytes_verified=True,
            catalog=writer.reference(output / "catalog.json"), catalog_origin=session.catalog_origin,
            catalog_difficulty_inventory_complete=True, audio_sets_per_song=1, extraction_workers=1,
            source_packages_rehashed_after_batch=True, installed_source_sha256=session.installed_source_sha256,
            song_count=len(songs), chart_count=sum(len(s["charts"]) for s in songs), songs=songs, bundles=bundles,
            failures=failures, tools=tools, limits=dict(output_bytes=max_output_bytes, cached_bundle_bytes=session.max_cached_bundle_bytes),
            scope="All exact native Chart_* catalog assets for the proven chapter; nonstandard suffixes are archival variants, UI selectability unverified; no gameplay support, device discovery, input, upload or score submission.")
        writer.write(output / "chapter-index.json", json_bytes(index))
        manifest = dict(schema="pas.phigros-chapter-artifacts.v1", status=index["status"],
            chapter_index=writer.reference(output / "chapter-index.json"), artifacts=list(writer.records.values()),
            artifact_index_excludes=["archive-manifest.json", "archive-manifest.json.sha256"], total_bytes_before_manifest=writer.bytes)
        artifact = writer.write(output / "archive-manifest.json", json_bytes(manifest))
        writer.write(output / "archive-manifest.json.sha256", (artifact["sha256"] + "  archive-manifest.json\n").encode("ascii"))
        return index
    except Exception as error:
        failure = dict(schema=SCHEMA, status="failed", complete=False, reason=f"{type(error).__name__}: {error}",
                       completed_songs=[s["track_id"] for s in songs], artifacts=list(writer.records.values()),
                       scope="Partial archive preserved; never a completed chapter or gameplay result.")
        write_new(output / "failure-manifest.json", json_bytes(failure))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apks", nargs="+", type=Path)
    parser.add_argument("--installed-source", required=True, type=Path)
    parser.add_argument("--membership-proof", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--max-output-mib", type=int, default=4096)
    args = parser.parse_args()
    result = archive_chapter(args.apks, args.installed_source, args.membership_proof, args.output,
        max_output_bytes=args.max_output_mib * MIB,
        progress=lambda event: print(json.dumps(event, ensure_ascii=False), flush=True))
    print(json.dumps(dict(status=result["status"], chapter_id=result["chapter_id"], songs=result["song_count"], charts=result["chart_count"]), ensure_ascii=False))
    return 0 if result["status"] == "complete" else 2


if __name__ == "__main__":
    raise SystemExit(main())
