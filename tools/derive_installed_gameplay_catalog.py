"""Derive three distinct local chart identities from already pinned installed APKs.

Offline only: preserves public originals/IDs, never launches a device or game,
and archives native TextAsset bytes without conversion. Native audio is hashed
in memory for the existing source contract; no audio copy or WAV is produced.
"""
from __future__ import annotations

import argparse
from collections import Counter
import copy
import hashlib
import json
from pathlib import Path

from archive_chart_tracker import analyze_chart, blob_sha
from extract_phigros_target import AssetExtractionSession, json_bytes, textasset_bytes
from verify_installed_catalog import audio_identity, reference, sha

TARGETS = (("Archidoxen.Se_IRA.0", "AT"),
           ("ExoplanetaryMirage.かめりあ.0", "AT"),
           ("Reimei.影虎.0", "IN"))
SOURCE_KIND = "installed-local-variant"


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_bytes(path, raw):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as stream:
        stream.write(raw)
    return dict(file=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def write_json(path, value):
    return write_bytes(path, json_bytes(value))


def new_library_id(track, difficulty, actual_sha):
    if difficulty not in ("IN", "AT") or len(actual_sha) != 64 or any(c not in "0123456789abcdef" for c in actual_sha):
        raise ValueError("Explicit IN/AT and actual SHA256 required")
    return f"installed/4.0.1-157/{track}/{difficulty}/{actual_sha}"


def public_chart_pin(row, public_root):
    path = (Path(public_root) / row["chart"]["file"]).resolve()
    if not path.is_relative_to(Path(public_root).resolve()) or sha(path) != row["chart"]["sha256"]:
        raise ValueError("Public chart escaped its pinned archive or changed bytes")
    return dict(copy.deepcopy(row["chart"]), file=str(path))


def compose_catalog(public, variants, public_root, public_reference, installed_reference):
    """Keep all non-target IDs and hashes; give actual differing bytes new IDs."""
    if public.get("schema") != "pas.catalog-library.v1":
        raise ValueError("Expected pinned public catalogue schema")
    if set(variants) != set(TARGETS):
        raise ValueError("Exactly the three requested local variants are required")
    result = copy.deepcopy(public)
    seen, changed = set(), []
    for row in result["charts"]:
        key = (row["track_id"], row["difficulty"])
        if key in seen:
            raise ValueError("Duplicate track/difficulty identity")
        seen.add(key)
        original_id, original_sha = row["library_id"], row["chart"]["sha256"]
        original = copy.deepcopy(row["chart"])
        original["file"] = str((Path(public_root) / original["file"]).resolve())
        # Explicit absolute pins are supported by the manual preparation/panel
        # path, avoiding duplicate copies of the other 386 public originals.
        row["chart"]["file"] = original["file"]
        if key not in variants:
            continue
        variant = variants[key]
        actual = variant["chart"]
        if actual["sha256"] == original_sha:
            raise ValueError("Local variant must not alias identical public bytes")
        actual_id = new_library_id(*key, actual["sha256"])
        if actual_id == original_id:
            raise ValueError("Different local bytes cannot retain the public ID")
        summary, analysis, blocks = variant["summary"], variant["analysis_status"], variant["blockers"]
        row.update(library_id=actual_id, upstream_library_id=original_id, public_chart=original,
                   source_public_id=original_id, source_public_chart=original, source_kind=SOURCE_KIND,
                   chart=copy.deepcopy(actual), summary=copy.deepcopy(summary),
                   notes=summary.get("note_count"), line_count=summary.get("line_count"),
                   format_version=summary.get("format_version"), analysis_status=analysis,
                   version="4.0.1", version_code=157, public_version=public["source"].get("version"),
                   exact_local=True, public_match=False, archive_status="verified", asset_status="verified",
                   source=dict(kind=SOURCE_KIND, package="com.PigeonGames.Phigros", game_version="4.0.1",
                               version_code=157, game_version_exact=True, exact_local=True, public_match=False,
                               installed_source=installed_reference, source_manifest=variant["source_manifest"]),
                   source_manifest=variant["source_manifest"], audio_identity=variant["audio_identity"],
                   prepare_status="blocked", readiness="blocked", live_ready=False,
                   blockers=[*copy.deepcopy(blocks), dict(code="local_profile_required",
                       message="本機安裝版，與公開版不同；需以此原件重新建立接點／首音及人工前檢。")])
        changed.append(dict(track_id=key[0], difficulty=key[1], upstream_library_id=original_id,
                            library_id=actual_id, public_sha256=original_sha, installed_sha256=actual["sha256"],
                            analysis_status=analysis, block_area_fields=summary.get("block_area_fields", [])))
    if not set(TARGETS).issubset(seen) or len(changed) != 3:
        raise ValueError("Requested target missing from the public catalogue")
    ids = [x["library_id"] for x in result["charts"]]
    if len(ids) != len(set(ids)):
        raise ValueError("Effective catalogue has duplicate IDs")
    result["schema"] = "pas.catalog-library.v1"
    result["source"] = dict(schema="pas.effective-gameplay-catalog-source.v1", version="4.0.1/157",
        package="com.PigeonGames.Phigros", version_name="4.0.1", version_code=157,
        public_catalog=public_reference, installed_source=installed_reference,
        scope="386 public byte identities retained; three differing installed originals have distinct local IDs.")
    result["derived_from"] = public_reference
    result["source_variants"] = changed
    result["counts"].update(chart_count=len(ids), installed_local_variant_count=3,
                            retained_public_id_count=len(ids)-3, live_ready_count=0,
                            unsupported_block_area_count=sum(x["analysis_status"]=="unsupported_block_area" for x in result["charts"]),
                            unsupported_format_count=sum(x["analysis_status"]=="unsupported_format" for x in result["charts"]),
                            analysis_failed_count=sum(x["analysis_status"]=="failed" for x in result["charts"]))
    result["counts"]["block_area_field_states"] = dict(Counter(field["state"] for row in result["charts"]
        for field in row["summary"].get("block_area_fields", [])))
    result.update(status="effective_installed_catalog_requires_offline_preparation", input_enabled=False,
                  device_discovery=False, gameplay_qualified=False,
                  qualification="Exact local source identities only; runtime preparation and game judgment remain separate.")
    return result


def compose_identity(previous, effective_catalog_reference, variants):
    result = copy.deepcopy(previous)
    result["library"] = effective_catalog_reference
    result["derived_from_identity_scope"] = "Original exact identities retained; three mismatching public IDs replaced with distinct installed IDs."
    found = set()
    for row in result["charts"]:
        key = (row["track_id"], row["difficulty"])
        if key in variants:
            variant = variants[key]
            row.update(library_id=new_library_id(*key, variant["chart"]["sha256"]),
                       upstream_library_id=row["library_id"], public_chart=variant["public_chart"],
                       chart_sha256=variant["chart"]["sha256"], status="verified_exact_installed_chart",
                       source_manifest=variant["source_manifest"], native_audio_sha256=variant["audio_identity"]["sha256"],
                       exact_local=True, public_match=False, source_kind=SOURCE_KIND)
            row.pop("reason", None)
            found.add(key)
    if found != set(TARGETS):
        raise ValueError("Identity map does not cover the three requested source variants")
    if any(row["status"] != "verified_exact_installed_chart" for row in result["charts"]):
        raise ValueError("Unresolved installed identities remain in the effective catalogue")
    result.update(total=len(result["charts"]), verified=len(result["charts"]),
                  installed_local_variant_count=3, input_enabled=False, device_access=False, gameplay_qualified=False)
    return result


def derive(library, identity, installed_source, apks, output):
    library, identity, installed_source, output = map(lambda x: Path(x).resolve(),
                                                     (library, identity, installed_source, output))
    if output.exists():
        raise FileExistsError("Derivation output must be a new directory")
    public, previous = read(library), read(identity)
    public_ref, identity_ref, installed_ref = reference(library), reference(identity), reference(installed_source)
    if previous.get("library") != public_ref or previous.get("installed_source") != installed_ref:
        raise ValueError("Original identity map input pins do not match the requested inputs")
    public_rows = {(x["track_id"], x["difficulty"]): x for x in public["charts"]}
    if len(public_rows) != len(public["charts"]) or not set(TARGETS).issubset(public_rows):
        raise ValueError("Public catalogue lacks a unique requested target")
    session = AssetExtractionSession(apks, installed_source)
    output.mkdir(parents=True, exist_ok=False)
    variants, artifacts = {}, []
    packages = [dict(file=p["path"], sha256=p["sha256"], bytes=p["bytes"]) for p in session.inventory["packages"]]
    for track, difficulty in TARGETS:
        session.bundle_cache.clear(); session.cached_bundle_bytes = 0
        row = public_rows[(track, difficulty)]
        original_chart = public_chart_pin(row, library.parent)
        key = f"Assets/Tracks/{track}/Chart_{difficulty}.json"
        reader, obj, raw, origin, env = session.select(key, "TextAsset")
        name, script, offset = textasset_bytes(raw, reader.reader.endian)
        if name != obj.m_Name or script != obj.m_Script.encode("utf-8", "surrogateescape"):
            raise ValueError("Raw TextAsset slice and typed reader disagree")
        if hashlib.sha256(script).hexdigest() == row["chart"]["sha256"]:
            raise ValueError("Requested differing local variant unexpectedly equals the public chart")
        folder = output / "charts" / track
        actual = write_bytes(folder / f"Chart_{difficulty}.json", script)
        raw_pin = write_bytes(folder / f"Chart_{difficulty}.textasset.bin", raw)
        artifacts.extend((actual, raw_pin))
        actual.update(status="verified", git_blob_sha=blob_sha(script),
                      git_blob_scope="Computed blob identity of local bytes, not an upstream commit object")
        summary, analysis, blocks = analyze_chart(script)
        audio = audio_identity(session, track)
        actual_id = new_library_id(track, difficulty, actual["sha256"])
        source = dict(schema="pas.installed-catalog-song-source.v1", package=session.source["package"],
            version_name=session.source["version_name"], version_code=session.source["version_code"],
            track_id=track, difficulty=difficulty, catalog=session.catalog_origin,
            installed_source=installed_ref, packages=packages,
            chart=dict(file=actual["file"], sha256=actual["sha256"], bytes=actual["bytes"], source=origin,
                       textasset_script_byte_offset=offset, byte_transform="none", raw_textasset=raw_pin),
            audio_identity=audio, public_library=public_ref, public_library_id=row["library_id"],
            upstream_library_id=row["library_id"], effective_library_id=actual_id, public_chart=original_chart,
            exact_chart_bytes_verified=True, exact_local=True, public_match=False, source_kind=SOURCE_KIND,
            input_enabled=False, device_access=False,
            scope="Exact local installed m_Script bytes; public bytes preserved separately; no identity alias or gameplay qualification.")
        source_pin = write_json(output / "sources" / (hashlib.sha256(actual_id.encode()).hexdigest()[:24]+".json"), source)
        artifacts.append(source_pin)
        variants[(track, difficulty)] = dict(chart=actual, public_chart=original_chart,
            source_manifest=source_pin, audio_identity=audio, summary=summary, analysis_status=analysis, blockers=blocks)
        print(json.dumps(dict(track_id=track, difficulty=difficulty, public_sha256=row["chart"]["sha256"],
                              installed_sha256=actual["sha256"], bytes=len(script),
                              notes=summary.get("note_count"), analysis_status=analysis)), flush=True)
    effective = compose_catalog(public, variants, library.parent, public_ref, installed_ref)
    catalog_pin = write_json(output / "catalog.json", effective); artifacts.append(catalog_pin)
    effective_identity = compose_identity(previous, dict(file=catalog_pin["file"], sha256=catalog_pin["sha256"]), variants)
    catalogue_hashes = {row["library_id"]: row["chart"]["sha256"] for row in effective["charts"]}
    identity_hashes = {row["library_id"]: row["chart_sha256"] for row in effective_identity["charts"]}
    if len(identity_hashes) != len(effective_identity["charts"]) or identity_hashes != catalogue_hashes:
        raise ValueError("Effective identity coverage does not exactly match the derived catalogue")
    effective_identity["derived_from"] = identity_ref
    identity_pin = write_json(output / "identity-map.json", effective_identity); artifacts.append(identity_pin)
    session.verify_sources_unchanged()
    for path, expected in ((library, public_ref), (identity, identity_ref), (installed_source, installed_ref)):
        if reference(path) != expected:
            raise ValueError("Pinned derivation input changed")
    # Every effective chart, including the 386 retained public identities, must
    # exist as complete raw bytes before this derivative is published.
    public_by_id = {x["library_id"]: x for x in public["charts"]}
    for row in effective["charts"]:
        if sha(row["chart"]["file"]) != row["chart"]["sha256"]:
            raise ValueError("Effective chart file/hash mismatch")
        if (row["track_id"], row["difficulty"]) not in variants:
            if row["library_id"] not in public_by_id or row["chart"]["sha256"] != public_by_id[row["library_id"]]["chart"]["sha256"]:
                raise ValueError("Non-target public identity was changed")
    manifest = dict(schema="pas.installed-effective-catalog-derivation.v1", status="verified_derivation",
        public_catalog=public_ref, original_identity=identity_ref, installed_source=installed_ref,
        effective_catalog=catalog_pin, effective_identity=identity_pin, packages=packages,
        total=len(effective["charts"]), replaced_local_variants=3, retained_public_ids=len(effective["charts"])-3,
        verified_installed_identities=effective_identity["verified"], variants=effective["source_variants"],
        artifacts=artifacts, raw_chart_byte_transform="none", audio_saved=False,
        device_access=False, input_enabled=False, gameplay_qualified=False)
    manifest_pin = write_json(output / "manifest.json", manifest)
    write_bytes(output / "manifest.sha256", (manifest_pin["sha256"]+"  manifest.json\n").encode("ascii"))
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--identity", type=Path, required=True)
    parser.add_argument("--installed-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("apks", type=Path, nargs="+")
    args = parser.parse_args()
    manifest = derive(args.library, args.identity, args.installed_source, args.apks, args.output)
    print(json.dumps({key: manifest[key] for key in ("status", "effective_catalog", "effective_identity", "total", "replaced_local_variants", "retained_public_ids", "verified_installed_identities")}, indent=2))
