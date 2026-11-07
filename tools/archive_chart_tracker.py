"""Archive immutable IN/AT originals from MEMZ-Chrome/phigros-chart-tracker.

Only the selected current-tree blobs are fetched. Raw charts are never converted,
and every public chart remains blocked pending local identity and runtime work.
No audio, device discovery, input, upstream code execution, or Git push is used.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import quote

REPOSITORY = "MEMZ-Chrome/phigros-chart-tracker"
REMOTE = "https://github.com/" + REPOSITORY + ".git"
SCHEMA = "pas.chart-tracker-archive.v1"
MAX_FILE_BYTES = 32 * 1024 * 1024
MAX_TOTAL_BYTES = 4 * 1024 * 1024 * 1024
MAX_CHARTS = 4096
SHA1 = re.compile(r"[0-9a-f]{40}\Z")
CHART = re.compile(r"charts/([^/]+)/Chart_(IN|AT)\.json\Z")
SOURCE_ERROR = re.compile(r"charts/([^/]+)/Chart_(IN|AT)_Error\.json\Z")
LFS_HEADER = b"version https://git-lfs.github.com/spec/v1"
BLOCK_MESSAGE = "原譜含 blockAreaList，已歸檔；目前無法完成。"


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")


def load_json(raw):
    def reject(value):
        raise ValueError("non-finite JSON: " + value)
    return json.loads(raw.decode("utf-8-sig"), parse_constant=reject)


def sha256(raw):
    return hashlib.sha256(raw).hexdigest()


def blob_sha(raw):
    return hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()


def safe_path(root, relative):
    root = Path(root).absolute()
    parts = PurePosixPath(relative)
    if (not relative or parts.is_absolute() or relative != parts.as_posix()
            or any(x in ("", ".", "..") for x in parts.parts)
            or any(ord(c) < 32 or c in '\\:*?"<>|' for c in relative)):
        raise ValueError("unsafe archive path: " + repr(relative))
    path = root.joinpath(*parts.parts)
    cursor = path
    while True:
        if cursor.is_symlink():
            raise ValueError("archive paths must not traverse symlinks")
        if cursor == root:
            break
        cursor = cursor.parent
    if root.resolve() not in path.resolve().parents:
        raise ValueError("archive path escapes root")
    return path


def write_immutable(root, relative, raw):
    path = safe_path(root, relative)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if not path.is_file() or path.read_bytes() != raw:
            raise ValueError("existing immutable archive differs: " + relative)
        return
    with path.open("xb") as stream:
        stream.write(raw)


def git(checkout, *args, timeout=1200, input_data=None):
    return subprocess.run(["git", "-C", str(checkout), *args], check=True,
                          input=input_data, stdout=subprocess.PIPE,
                          timeout=timeout).stdout


def source_tree(checkout, commit):
    if not SHA1.fullmatch(commit):
        raise ValueError("a full immutable commit SHA is required")
    if git(checkout, "rev-parse", "HEAD").decode().strip() != commit:
        raise ValueError("checkout HEAD differs from the requested commit")
    records = []
    for raw in git(checkout, "ls-tree", "-r", "-z", commit).split(b"\0"):
        if not raw:
            continue
        header, path_bytes = raw.split(b"\t", 1)
        mode, kind, oid = header.decode("ascii").split()
        path = path_bytes.decode("utf-8")
        if CHART.fullmatch(path) or SOURCE_ERROR.fullmatch(path) or path in ("README.md", "version.json"):
            if mode != "100644" or kind != "blob" or not SHA1.fullmatch(oid):
                raise ValueError("unsupported selected source tree entry")
            records.append(dict(path=path, git_blob_sha=oid, mode=mode))
    charts = [x for x in records if CHART.fullmatch(x["path"])]
    if not charts or len(charts) > MAX_CHARTS:
        raise ValueError("missing or oversized selected IN/AT inventory")
    if sum(x["path"] == "version.json" for x in records) != 1:
        raise ValueError("source version.json is required")
    return sorted(records, key=lambda x: x["path"])


def fetch_source(checkout, commit):
    checkout = Path(checkout).absolute()
    if not SHA1.fullmatch(commit):
        raise ValueError("a full immutable commit SHA is required")
    if not checkout.exists():
        subprocess.run(["git", "clone", "--depth", "1", "--filter=blob:none",
                        "--no-checkout", "--single-branch", REMOTE, str(checkout)],
                       check=True, timeout=1200)
    origin = git(checkout, "remote", "get-url", "origin").decode().strip()
    if origin.removesuffix(".git").rstrip("/") != REMOTE.removesuffix(".git"):
        raise ValueError("source checkout origin does not match the expected repository")
    git(checkout, "fetch", "--depth", "1", "--filter=blob:none", "origin", commit)
    git(checkout, "sparse-checkout", "init", "--no-cone")
    patterns = b"/README.md\n/version.json\n/charts/*/Chart_IN.json\n/charts/*/Chart_AT.json\n/charts/*/Chart_IN_Error.json\n/charts/*/Chart_AT_Error.json\n"
    git(checkout, "sparse-checkout", "set", "--no-cone", "--stdin", input_data=patterns)
    git(checkout, "checkout", "--detach", commit)


def block_area_fields(value):
    """Find every spelling of blockAreaList, with JSON-pointer provenance."""
    found, pending, visited = [], [(value, "")], 0
    while pending:
        current, path = pending.pop()
        visited += 1
        if visited > 4_000_000:
            raise ValueError("chart JSON traversal exceeds the finite node budget")
        if isinstance(current, dict):
            for key, child in current.items():
                child_path = path + "/" + key.replace("~", "~0").replace("/", "~1")
                if re.sub(r"[^a-z0-9]", "", key.lower()) == "blockarealist":
                    state = ("nonempty" if child else "empty") if isinstance(child, list) else "invalid"
                    found.append(dict(path=child_path, state=state,
                                      value_type=type(child).__name__,
                                      entries=len(child) if isinstance(child, list) else None))
                if isinstance(child, (dict, list)):
                    pending.append((child, child_path))
        elif isinstance(current, list):
            pending.extend((child, path + "/" + str(i)) for i, child in enumerate(current)
                           if isinstance(child, (dict, list)))
    return sorted(found, key=lambda x: x["path"])


def analyze_chart(raw):
    try:
        value = load_json(raw)
        if not isinstance(value, dict):
            raise ValueError("chart root is not an object")
        fields = block_area_fields(value)
        lines = value.get("judgeLineList")
        version = value.get("formatVersion")
        notes = None
        if isinstance(lines, list) and all(isinstance(x, dict) for x in lines):
            arrays = [line.get(side) for line in lines for side in ("notesAbove", "notesBelow")]
            if all(isinstance(x, list) for x in arrays):
                notes = sum(len(x) for x in arrays)
        summary = dict(format_version=version, line_count=len(lines) if isinstance(lines, list) else None,
                       note_count=notes, block_area_fields=fields,
                       block_area_status="present" if fields else "absent",
                       semantics="Raw structural inventory only; no chart conversion or gameplay qualification.")
        if fields:
            return summary, "unsupported_block_area", [dict(code="unsupported_block_area", message=BLOCK_MESSAGE)]
        if version != 3:
            return summary, "unsupported_format", [dict(code="unsupported_format", message=f"原譜格式 v{version} 已歸檔；目前格式尚未支援。")]
        if not isinstance(lines, list) or notes is None:
            raise ValueError("missing native v3 line or note-side arrays")
        return summary, "structural_only", []
    except (ValueError, TypeError, KeyError, RecursionError, OverflowError) as error:
        return dict(error=str(error)[:400], block_area_status="unknown"), "failed", [
            dict(code="structural_analysis_failed", message="原件已歸檔；結構分析失敗：" + str(error)[:240])]


def archive(checkout, commit, output, metadata_library=None):
    checkout, output = Path(checkout).absolute(), Path(output).absolute()
    records = source_tree(checkout, commit)
    files, rows, source_errors, total = [], [], [], 0

    def store(relative, raw, *, source=None, kind="provenance"):
        nonlocal total
        if len(raw) > MAX_FILE_BYTES or total + len(raw) > MAX_TOTAL_BYTES:
            raise ValueError("archive file or total byte budget exceeded")
        if kind == "chart" and raw.startswith(LFS_HEADER):
            raise ValueError("LFS pointer is not an archived chart original")
        if source and blob_sha(raw) != source["git_blob_sha"]:
            raise ValueError("source Git blob hash mismatch: " + source["path"])
        write_immutable(output, relative, raw)
        total += len(raw)
        item = dict(file=relative, bytes=len(raw), sha256=sha256(raw), kind=kind)
        if source:
            item.update(source_path=source["path"], git_blob_sha=source["git_blob_sha"],
                        source_url=f"https://raw.githubusercontent.com/{REPOSITORY}/{commit}/{quote(source['path'], safe='/')}")
        files.append(item)
        return item

    metadata, comparison_catalog, metadata_provenance = {}, {}, None
    if metadata_library:
        raw = Path(metadata_library).read_bytes()
        value = load_json(raw)
        metadata = {x["song_id"]: x for x in value.get("songs", [])}
        comparison_catalog = {(x["track_id"], x["difficulty"]): x for x in value.get("charts", [])}
        metadata_provenance = store("provenance/title-library.json", raw, kind="supplemental_title_metadata")
        metadata_provenance["source"] = value.get("source")
        metadata_provenance["scope"] = "Display labels only, matched by exact song_id; never substitutes chart identity."

    version_record = next(x for x in records if x["path"] == "version.json")
    version_raw = git(checkout, "cat-file", "blob", version_record["git_blob_sha"])
    version_metadata = load_json(version_raw)
    version = version_metadata.get("version")
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]+(?:\.[0-9]+){1,4}", version):
        raise ValueError("invalid upstream version identity")
    for record in records:
        source_path = record["path"]
        match, error_match = CHART.fullmatch(source_path), SOURCE_ERROR.fullmatch(source_path)
        # Git blobs preserve original LF/BOM bytes even when Windows checkout
        # applies core.autocrlf or another working-tree filter.
        size = int(git(checkout, "cat-file", "-s", record["git_blob_sha"]).decode().strip())
        if size > MAX_FILE_BYTES:
            raise ValueError("source file exceeds the finite byte budget")
        raw = git(checkout, "cat-file", "blob", record["git_blob_sha"])
        if not match and not error_match:
            store("provenance/" + source_path, raw, source=record)
            continue
        if error_match:
            stored = store("source-errors/" + source_path, raw, source=record, kind="source_error")
            source_errors.append(dict(stored, track_id=error_match[1], difficulty=error_match[2],
                                      status="source_error", reason="Upstream explicitly named this *_Error.json; not a playable chart."))
            continue
        asset = store(source_path, raw, source=record, kind="chart")
        track_id, difficulty = match.groups()
        song_id, separator, variant = track_id.rpartition(".")
        if not separator or not variant.isdecimal():
            song_id, variant = track_id, "unknown"
        song = metadata.get(song_id, {})
        title = song.get("title") or song_id
        summary, analysis, blockers = analyze_chart(raw)
        blockers.extend([
            dict(code="audio_not_archived", message="此來源僅提供譜面；本次未歸檔音源。"),
            dict(code="installed_version_unverified", message="來源原譜尚未與本機 APK／遊戲版本逐項核對。"),
            dict(code="local_profile_required", message="尚需本機接點編譯、首音／HUD 校正及執行前檢。")])
        rows.append(dict(library_id="/".join((REPOSITORY, commit, track_id, difficulty, asset["sha256"])),
                         song_id=song_id, track_id=track_id, title=title, display_name=title,
                         title_source="provenance/title-library.json" if song else "upstream_track_identifier",
                         composer=song.get("composer", ""), artist=song.get("composer", ""),
                         difficulty=difficulty, variant=variant, variant_id=variant,
                         rating=song.get("ratings", {}).get(difficulty), version=version, chapter=None,
                         chart=dict(asset, status="verified"), audio=None, summary=summary,
                         format_version=summary.get("format_version"), notes=summary.get("note_count"),
                         line_count=summary.get("line_count"), source=dict(repository=REPOSITORY,
                         revision=commit, game_version=version, game_version_exact=False, url=asset["source_url"]),
                         archive_status="verified", asset_status="verified", analysis_status=analysis,
                         prepare_status="blocked", readiness="blocked", live_ready=False, blockers=blockers,
                         legacy_library_id=None))
        if len(rows) % 25 == 0:
            print(json.dumps(dict(progress_charts=len(rows), bytes=total)), flush=True)
    comparisons = []
    for row in rows:
        reference = comparison_catalog.get((row["track_id"], row["difficulty"]), {}).get("chart", {})
        if not reference:
            status = "no_reference"
        elif (row["chart"]["sha256"] == reference.get("sha256")
              and row["chart"]["git_blob_sha"] == reference.get("git_blob_sha")
              and row["chart"]["bytes"] == reference.get("bytes")):
            status = "exact_byte_identity"
        else:
            status = "different_byte_identity"
        comparisons.append(dict(track_id=row["track_id"], difficulty=row["difficulty"], status=status,
                                tracker_file=row["chart"]["file"], tracker_sha256=row["chart"]["sha256"],
                                tracker_git_blob_sha=row["chart"]["git_blob_sha"], tracker_bytes=row["chart"]["bytes"],
                                reference_file=reference.get("file"), reference_sha256=reference.get("sha256"),
                                reference_git_blob_sha=reference.get("git_blob_sha"), reference_bytes=reference.get("bytes")))
    counts = dict(song_count=len({x["song_id"] for x in rows}), chart_count=len(rows),
                  IN=sum(x["difficulty"] == "IN" for x in rows), AT=sum(x["difficulty"] == "AT" for x in rows),
                  verified_chart_count=len(rows), verified_audio_count=0, audio_count=0,
                  source_error_count=len(source_errors), live_ready_count=0,
                  unsupported_block_area_count=sum(x["analysis_status"] == "unsupported_block_area" for x in rows),
                  unsupported_format_count=sum(x["analysis_status"] == "unsupported_format" for x in rows),
                  analysis_failed_count=sum(x["analysis_status"] == "failed" for x in rows))
    field_states = Counter(field["state"] for row in rows for field in row["summary"].get("block_area_fields", []))
    counts["block_area_field_states"] = dict(field_states)
    counts["catalog_byte_comparison"] = dict(Counter(x["status"] for x in comparisons))
    source = dict(repository=REPOSITORY, revision=commit, version=version,
                  version_code=version_metadata.get("versioncode"),
                  url=f"https://github.com/{REPOSITORY}/tree/{commit}",
                  scope="Current pinned tree: IN/AT raw charts and explicitly named IN/AT source errors only; audio absent.")
    library = dict(schema="pas.catalog-library.v1", source=source, counts=counts, status="charts_archived_audio_absent",
                   charts=rows, songs=[metadata[x] for x in sorted({row["song_id"] for row in rows}) if x in metadata],
                   device_discovery=False, input_enabled=False,
                   qualification="Raw asset integrity and structural inventory only; every entry remains blocked.")
    store("library.json", json_bytes(library), kind="browse_index")
    inventory = io.StringIO(newline="")
    writer = csv.writer(inventory, lineterminator="\n")
    writer.writerow(("title", "track_id", "difficulty", "version", "file", "bytes", "sha256", "git_blob_sha", "analysis_status", "block_area_status", "block_area_fields", "reason"))
    for row in rows:
        writer.writerow((row["title"], row["track_id"], row["difficulty"], version, row["chart"]["file"], row["chart"]["bytes"],
                         row["chart"]["sha256"], row["chart"]["git_blob_sha"], row["analysis_status"],
                         row["summary"].get("block_area_status", "unknown"),
                         json.dumps(row["summary"].get("block_area_fields", []), ensure_ascii=False),
                         " | ".join(x["message"] for x in row["blockers"])))
    store("inventory.csv", b"\xef\xbb\xbf" + inventory.getvalue().encode("utf-8"), kind="inventory")
    store("provenance/source-tree.json", json_bytes(dict(commit=commit, selected_entries=records)), kind="source_inventory")
    store("provenance/catalog-byte-comparison.json", json_bytes(dict(
        scope="New raw bytes compared with the supplemental catalog's declared SHA256, Git blob SHA, and byte size; not a claim that reference files exist locally.",
        reference="provenance/title-library.json" if metadata_library else None, charts=comparisons)), kind="source_comparison")
    manifest = dict(schema=SCHEMA, source=source, counts=counts, chart_bytes=sum(x["chart"]["bytes"] for x in rows),
                    files=files, total_file_bytes=total, source_errors=source_errors,
                    supplemental_title_metadata=metadata_provenance,
                    audio_archived=False, raw_bytes_preserved=True, input_enabled=False,
                    status="complete_selected_chart_archive", gameplay_qualified=False)
    # Recheck every written file before publishing a successful manifest.
    verify_files(output, manifest)
    raw_manifest = json_bytes(manifest)
    write_immutable(output, "manifest.json", raw_manifest)
    write_immutable(output, "manifest.sha256", (sha256(raw_manifest) + "  manifest.json\n").encode("ascii"))
    return manifest


def verify_files(root, manifest):
    if manifest.get("schema") != SCHEMA:
        raise ValueError("unsupported archive manifest")
    files = manifest.get("files")
    if not isinstance(files, list) or not files or len(files) > MAX_CHARTS + 32:
        raise ValueError("invalid archive file inventory")
    seen, total, chart_count = set(), 0, 0
    for item in files:
        if item["file"] in seen:
            raise ValueError("duplicate archive path")
        seen.add(item["file"])
        path = safe_path(root, item["file"])
        if not path.is_file() or path.stat().st_size != item["bytes"] or item["bytes"] > MAX_FILE_BYTES:
            raise ValueError("archive file size mismatch: " + item["file"])
        raw = path.read_bytes()
        if sha256(raw) != item["sha256"] or (item.get("git_blob_sha") and blob_sha(raw) != item["git_blob_sha"]):
            raise ValueError("archive file hash mismatch: " + item["file"])
        if item["kind"] == "chart":
            chart_count += 1
            if raw.startswith(LFS_HEADER):
                raise ValueError("archive contains a chart LFS pointer")
        total += len(raw)
        if total > MAX_TOTAL_BYTES:
            raise ValueError("archive total byte budget exceeded")
    if total != manifest.get("total_file_bytes") or chart_count != manifest["counts"]["chart_count"]:
        raise ValueError("archive total inventory mismatch")
    return dict(status="verified", files=len(files), chart_count=chart_count, bytes=total,
                audio_archived=False, gameplay_qualified=False)


def verify(root):
    raw = safe_path(root, "manifest.json").read_bytes()
    recorded = safe_path(root, "manifest.sha256").read_text(encoding="ascii").strip()
    if recorded != sha256(raw) + "  manifest.json":
        raise ValueError("manifest checksum mismatch")
    return verify_files(root, load_json(raw))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    fetch = commands.add_parser("fetch-source")
    fetch.add_argument("--checkout", type=Path, required=True)
    fetch.add_argument("--commit", required=True)
    run = commands.add_parser("archive")
    run.add_argument("--checkout", type=Path, required=True)
    run.add_argument("--commit", required=True)
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--metadata-library", type=Path)
    check = commands.add_parser("verify")
    check.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "fetch-source":
        fetch_source(args.checkout, args.commit)
        result = dict(status="selected_source_checked_out", commit=args.commit)
    elif args.command == "archive":
        result = archive(args.checkout, args.commit, args.output, args.metadata_library)
        result = dict(status=result["status"], counts=result["counts"], chart_bytes=result["chart_bytes"], output=str(args.output.absolute()))
    else:
        result = verify(args.output)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
