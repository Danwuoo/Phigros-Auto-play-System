"""Bounded, offline data contracts for a versioned Phigros asset catalogue.

This module never connects to a device, invokes a player, or downloads anything.
Public-source identities remain separate from installed-source manifests and live
qualifications. File references in library.json are relative to its directory.
"""
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import tempfile
from urllib.parse import quote

SOURCE_SCHEMA = "pas.song-catalog-sources.v1"
LOCK_SCHEMA = "pas.song-catalog-lock.v1"
LIBRARY_SCHEMA = "pas.catalog-library.v1"
REPORT_SCHEMA = "pas.song-catalog-verification.v1"
MIB = 1024 * 1024
MAX_CHART_BYTES = 32 * MIB
MAX_ASSET_BYTES = 64 * MIB
MAX_MANIFEST_BYTES = 16 * MIB
MAX_TARGETS = 4096
LFS_HEADER = b"version https://git-lfs.github.com/spec/v1\n"
SHA1 = re.compile(r"[0-9a-f]{40}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
VERSION = re.compile(r"[0-9]+(?:\.[0-9]+){1,4}\Z")
REPOSITORY = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\Z")


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")


def reject_constant(value):
    raise ValueError("Non-finite JSON value: " + value)


def load_json(path, limit=MAX_MANIFEST_BYTES):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError("Missing, linked or oversized JSON file: " + str(path))
    return json.loads(path.read_text(encoding="utf-8-sig"), parse_constant=reject_constant)


def safe_relative(value):
    if not isinstance(value, str) or not value or len(value) > 700:
        raise ValueError("Invalid catalogue path")
    path = PurePosixPath(value)
    if (path.is_absolute() or value != path.as_posix() or any(part in (".", "..", "") for part in path.parts)
            or any(ord(c) < 32 or c in '\\:*?"<>|' for c in value)):
        raise ValueError("Unsafe catalogue path: " + repr(value))
    for part in path.parts:
        if part.endswith((".", " ")) or part.split(".", 1)[0].upper() in {
                "CON", "PRN", "AUX", "NUL", *("COM" + str(i) for i in range(1, 10)),
                *("LPT" + str(i) for i in range(1, 10))}:
            raise ValueError("Non-portable catalogue path")
    return value


def inside(root, relative):
    root = Path(root).absolute()
    if root.is_symlink():
        raise ValueError("Catalogue root must not be a symlink")
    relative = safe_relative(relative)
    path = root.joinpath(*PurePosixPath(relative).parts)
    cursor = path
    while cursor != root:
        if cursor.is_symlink():
            raise ValueError("Catalogue paths must not traverse symlinks")
        cursor = cursor.parent
    if root.resolve() not in path.resolve().parents:
        raise ValueError("Catalogue path escaped root")
    return path


def write_atomic(root, relative, data, immutable=False):
    path = inside(root, relative)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and immutable:
        if path.read_bytes() != data:
            raise ValueError("Existing immutable snapshot differs: " + relative)
        return
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        inside(root, relative)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def git_blob_sha(data):
    digest = hashlib.sha1()
    digest.update(b"blob " + str(len(data)).encode("ascii") + b"\0")
    digest.update(data)
    return digest.hexdigest()


def source_url(repository, commit, source_path):
    if not REPOSITORY.fullmatch(repository) or not SHA1.fullmatch(commit):
        raise ValueError("Source requires a repository and full immutable commit")
    safe_relative(source_path)
    return f"https://raw.githubusercontent.com/{repository}/{commit}/{quote(source_path, safe='/')}"


def validate_sources(config):
    if config.get("schema") != SOURCE_SCHEMA:
        raise ValueError("Unsupported source configuration")
    if not REPOSITORY.fullmatch(config.get("repository", "")) or not VERSION.fullmatch(config.get("version", "")):
        raise ValueError("Invalid source identity")
    commits = config.get("commits", {})
    if set(commits) != {"info", "chart", "music"} or not all(SHA1.fullmatch(x) for x in commits.values()):
        raise ValueError("All three source revisions must be full commit SHAs")
    expected = config.get("expected", {})
    if set(expected) != {"song_count", "IN", "AT", "variant_count", "audio_count"}:
        raise ValueError("Expected inventory must be explicit")
    if any(type(x) is not int or not 0 <= x <= MAX_TARGETS for x in expected.values()):
        raise ValueError("Invalid expected inventory budget")
    return config


def parse_metadata(info_raw, difficulty_raw):
    def records(raw):
        if len(raw) > 2 * MIB:
            raise ValueError("Metadata exceeds budget")
        lines = raw.decode("utf-8-sig").splitlines()
        result = {}
        for line in lines:
            if not line.strip():
                continue
            fields = line.split("\t")
            identity = fields[0]
            safe_relative(identity)
            if "/" in identity or identity in result or len(result) >= MAX_TARGETS:
                raise ValueError("Duplicate, unsafe or oversized song metadata")
            result[identity] = fields
        return result

    info, difficulty = records(info_raw), records(difficulty_raw)
    if set(info) != set(difficulty):
        raise ValueError("Song information and difficulty identities disagree")
    songs = []
    for identity, fields in info.items():
        values = difficulty[identity]
        if len(fields) < 7 or not 4 <= len(values) <= 5:
            raise ValueError("Missing metadata columns for " + identity)
        ratings = {}
        for level, raw in zip(("EZ", "HD", "IN", "AT"), values[1:]):
            number = float(raw)
            if not math.isfinite(number) or number <= 0 or number > 30:
                raise ValueError("Invalid difficulty rating for " + identity)
            ratings[level] = number
        songs.append(dict(song_id=identity, title=fields[1], composer=fields[2], illustrator=fields[3],
                          charters=dict(zip(("EZ", "HD", "IN", "AT"), fields[4:8])),
                          extra_metadata=fields[8:], ratings=ratings, chapter=None))
    return songs


def checked_tree(tree):
    if tree.get("truncated") is not False or not isinstance(tree.get("tree"), list):
        raise ValueError("Source tree is missing or truncated")
    if len(tree["tree"]) > 20000:
        raise ValueError("Source tree exceeds finite budget")
    files = {}
    for item in tree["tree"]:
        if item.get("type") != "blob":
            continue
        path = safe_relative(item["path"])
        if path in files or item.get("mode") != "100644" or not SHA1.fullmatch(item.get("sha", "")):
            raise ValueError("Unsupported or duplicate source tree file")
        size = item.get("size")
        if type(size) is not int or not 0 < size <= MAX_ASSET_BYTES:
            raise ValueError("Source file exceeds finite byte budget: " + path)
        files[path] = item
    return files


def build_lock(config, info_tree, chart_tree, music_tree, info_raw, difficulty_raw, version_raw):
    config = validate_sources(config)
    if version_raw.decode("utf-8-sig").strip() != config["version"]:
        raise ValueError("Metadata version differs from pinned source configuration")
    songs = parse_metadata(info_raw, difficulty_raw)
    song_map = {row["song_id"]: row for row in songs}
    trees = {"info": checked_tree(info_tree), "chart": checked_tree(chart_tree), "music": checked_tree(music_tree)}
    targets = []

    def append(kind, branch, source_path, relative, **labels):
        item = trees[branch].get(source_path)
        if item is None:
            raise ValueError("Source is missing a referenced file: " + source_path)
        targets.append(dict(kind=kind, branch=branch, source_path=source_path, file=safe_relative(relative),
                            bytes=item["size"], git_blob_sha=item["sha"],
                            raw_url=source_url(config["repository"], config["commits"][branch], source_path), **labels))

    metadata = {"info.tsv": info_raw, "difficulty.tsv": difficulty_raw, "version.txt": version_raw}
    for name, raw in metadata.items():
        append("metadata", "info", name, "metadata/" + name)
        target = targets[-1]
        if len(raw) != target["bytes"] or git_blob_sha(raw) != target["git_blob_sha"]:
            raise ValueError("Metadata differs from pinned tree: " + name)
    found = set()
    counts = Counter()
    for path in sorted(trees["chart"]):
        match = re.fullmatch(r"([^/]+)/((?:IN)|(?:AT))\.json", path)
        if not match:
            continue
        track, level = match.groups()
        canonical, separator, variant = track.rpartition(".")
        if not separator or not variant.isdecimal() or canonical not in song_map:
            raise ValueError("Chart has no exact song/variant metadata mapping: " + path)
        if level not in song_map[canonical]["ratings"]:
            raise ValueError("Chart difficulty is absent from metadata: " + path)
        if trees["chart"][path]["size"] > MAX_CHART_BYTES:
            raise ValueError("Chart exceeds byte budget: " + path)
        append("chart", "chart", path, "charts/" + path, song_id=canonical, track_id=track,
               variant=variant, difficulty=level)
        found.add((canonical, variant, level))
        counts[level] += 1
        counts["variant_count"] += variant != "0"
    for song in songs:
        identity = song["song_id"]
        for level in ("IN", "AT"):
            if level in song["ratings"] and (identity, "0", level) not in found:
                raise ValueError("Metadata references a missing basic chart: " + identity + "/" + level)
        append("audio", "music", identity + ".ogg", "music/" + identity + ".ogg", song_id=identity)
    actual = dict(song_count=len(songs), IN=counts["IN"], AT=counts["AT"],
                  variant_count=counts["variant_count"], audio_count=len(songs))
    if actual != config["expected"]:
        raise ValueError("Pinned catalogue inventory differs from configuration: " + str(actual))
    if len(targets) > MAX_TARGETS:
        raise ValueError("Too many catalogue targets")
    result = dict(schema=LOCK_SCHEMA, source=config, counts=actual, songs=songs,
                  targets=sorted(targets, key=lambda row: row["file"]),
                  source_trees={branch: dict(requested_commit=config["commits"][branch],
                                            reported_sha=tree["sha"], truncated=False)
                                for branch, tree in (("info", info_tree), ("chart", chart_tree), ("music", music_tree))})
    validate_lock(result)
    return result


def validate_lock(lock):
    if lock.get("schema") != LOCK_SCHEMA:
        raise ValueError("Unsupported catalogue lock")
    config = validate_sources(lock["source"])
    targets = lock.get("targets")
    if not isinstance(targets, list) or not 1 <= len(targets) <= MAX_TARGETS:
        raise ValueError("Invalid target inventory")
    seen = set()
    for item in targets:
        path = safe_relative(item["file"])
        if path.casefold() in seen:
            raise ValueError("Duplicate or case-colliding destination path")
        seen.add(path.casefold())
        kind, branch = item.get("kind"), item.get("branch")
        if {"chart": "chart", "audio": "music", "metadata": "info"}.get(kind) != branch:
            raise ValueError("Asset kind/source branch mismatch")
        if item.get("raw_url") != source_url(config["repository"], config["commits"][branch], item["source_path"]):
            raise ValueError("Asset URL differs from pinned repository and path")
        prefix = {"chart": "charts/", "audio": "music/", "metadata": "metadata/"}[kind]
        if path != prefix + item["source_path"]:
            raise ValueError("Destination path differs from source identity")
        maximum = MAX_CHART_BYTES if kind == "chart" else MAX_ASSET_BYTES
        if type(item.get("bytes")) is not int or not 0 < item["bytes"] <= maximum or not SHA1.fullmatch(item.get("git_blob_sha", "")):
            raise ValueError("Invalid asset fingerprint/size")
    return lock


def inspect_asset(root, target):
    path = inside(root, target["file"])
    base = {key: target[key] for key in ("file", "bytes", "git_blob_sha", "raw_url")}
    if not path.exists():
        return dict(base, status="missing", sha256=None)
    if not path.is_file():
        return dict(base, status="failed", sha256=None, error="destination_is_not_a_file")
    size = path.stat().st_size
    if size <= 1024:
        with path.open("rb") as stream:
            if stream.read(len(LFS_HEADER)) == LFS_HEADER:
                return dict(base, status="lfs_pointer", sha256=None, error="git_lfs_pull_required")
    if size != target["bytes"]:
        return dict(base, status="failed", sha256=None, actual_bytes=size, error="size_mismatch")
    digest = hashlib.sha256()
    git_digest = hashlib.sha1(b"blob " + str(size).encode("ascii") + b"\0")
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(MIB), b""):
            digest.update(chunk)
            git_digest.update(chunk)
    if git_digest.hexdigest() != target["git_blob_sha"]:
        return dict(base, status="failed", sha256=digest.hexdigest(), error="git_blob_mismatch")
    return dict(base, status="verified", sha256=digest.hexdigest())


def chart_summary(path):
    chart = load_json(path, MAX_CHART_BYTES)
    if not isinstance(chart, dict):
        raise ValueError("Chart root must be an object")
    version = chart.get("formatVersion")
    if type(version) is not int:
        raise ValueError("Missing chart formatVersion")
    lines = chart.get("judgeLineList")
    if not isinstance(lines, list) or len(lines) > 4096:
        raise ValueError("Missing or oversized judgeLineList")
    offset = chart.get("offset", 0)
    if type(offset) not in (int, float) or not math.isfinite(offset):
        raise ValueError("Invalid chart offset")
    counts, times = Counter(), []
    for line in lines:
        bpm = line.get("bpm")
        if type(bpm) not in (int, float) or not math.isfinite(bpm) or bpm <= 0:
            raise ValueError("Invalid line BPM")
        for side in ("notesAbove", "notesBelow"):
            notes = line.get(side)
            if not isinstance(notes, list):
                raise ValueError("Missing note-side array")
            for note in notes:
                kind, tick, hold = note.get("type"), note.get("time"), note.get("holdTime", 0)
                if type(kind) is not int or kind not in (1, 2, 3, 4):
                    raise ValueError("Unsupported note type")
                if any(type(x) not in (int, float) or not math.isfinite(x) for x in (tick, hold)) or hold < 0:
                    raise ValueError("Invalid note time")
                counts[kind] += 1
                if sum(counts.values()) > 200000:
                    raise ValueError("Chart note budget exceeded")
                times.append((tick * 1.875 / bpm + offset, (tick + hold) * 1.875 / bpm + offset))
    return dict(format_version=version, line_count=len(lines), note_count=sum(counts.values()),
                note_types={str(k): counts[k] for k in (1, 2, 3, 4)}, offset=offset,
                first_note_seconds=min((t[0] for t in times), default=None),
                last_note_seconds=max((t[1] for t in times), default=None),
                semantics="Structural inventory only; time formula is provisional for unsupported formats.")


def blocker(code, message):
    return dict(code=code, message=message)


def build_library(root, lock, observations=None):
    """Build a deterministic browse-only index. No public asset becomes live-ready."""
    validate_lock(lock)
    source = lock["source"]
    metadata = {row["song_id"]: row for row in lock["songs"]}
    audio_targets = {row["song_id"]: row for row in lock["targets"] if row["kind"] == "audio"}
    observations = {} if observations is None else observations
    assets = {}
    for target in lock["targets"]:
        observed = observations.get(target["file"]) or inspect_asset(root, target)
        # Transfer attempts and timestamps belong in the report, not the index.
        canonical = {key: target[key] for key in ("file", "bytes", "git_blob_sha", "raw_url")}
        canonical.update({key: observed[key] for key in ("status", "sha256", "error", "actual_bytes") if key in observed})
        assets[target["file"]] = canonical
    charts = []
    for target in lock["targets"]:
        if target["kind"] != "chart":
            continue
        song = metadata[target["song_id"]]
        asset = assets[target["file"]]
        audio = assets[audio_targets[song["song_id"]]["file"]]
        summary, analysis, blocks = {}, "unavailable", []
        if asset["status"] == "verified":
            try:
                summary = chart_summary(inside(root, asset["file"]))
                analysis = "structural_only" if summary["format_version"] == 3 else "unsupported_format"
                if analysis == "unsupported_format":
                    blocks.append(blocker("unsupported_format", f"原件已封存；目前原生 importer 尚未支援格式 v{summary['format_version']}。"))
                if summary["note_count"] * 2 > 16000:
                    blocks.append(blocker("step_budget", "完整閉合接點至少需要每音符兩個事件，超過目前16,000事件預算；不刪減音符。"))
                if summary.get("last_note_seconds") is not None and summary["last_note_seconds"] >= 180:
                    blocks.append(blocker("duration_budget", "最後音符時間達 180 秒；需另外核對接點計畫的時長限制。"))
            except (ValueError, TypeError, KeyError, OverflowError, RecursionError) as error:
                analysis = "failed"
                summary = {"error": str(error)[:400]}
                blocks.append(blocker("structural_analysis_failed", "原件已封存，結構分析失敗：" + str(error)[:240]))
        elif asset["status"] == "lfs_pointer":
            blocks.append(blocker("lfs_pull_required", "目前只有 Git LFS 指標；請先執行 git lfs pull。"))
        else:
            blocks.append(blocker("asset_unavailable", "譜面尚未完整取得或來源核對失敗。"))
        if audio["status"] != "verified":
            blocks.append(blocker("audio_unavailable", "對應音源尚未完整取得或未完成核對。"))
        blocks.extend([
            blocker("installed_version_unverified", "公開來源版本尚未與本機 APK／遊戲版本逐項核對。"),
            blocker("local_profile_required", "需要本機接點編譯、首音／HUD 校正及執行前檢。")])
        if target["difficulty"] == "AT":
            blocks.append(blocker("at_hud_required", "目前共用 IN HUD 政策不能直接套用 AT；需對應畫面證據。"))
        if target["variant"] != "0":
            blocks.append(blocker("variant_binding_required", "譜面變體已獨立保存；實測前需確認遊戲中的對應變體。"))
        identity = "/".join((source["version"], target["track_id"], target["difficulty"], target["git_blob_sha"]))
        charts.append(dict(library_id=identity, song_id=song["song_id"], track_id=target["track_id"],
                           title=song["title"], display_name=song["title"], composer=song["composer"], artist=song["composer"],
                           difficulty=target["difficulty"], variant=target["variant"], variant_id=target["variant"],
                           rating=song["ratings"][target["difficulty"]], version=source["version"], chapter=None,
                           chart=asset, audio=audio, summary=summary,
                           format_version=summary.get("format_version"), notes=summary.get("note_count"),
                           line_count=summary.get("line_count"),
                           source=dict(repository=source["repository"], revision=source["commits"]["chart"],
                                       game_version=source["version"], game_version_exact=False, url=target["raw_url"]),
                           archive_status=asset["status"], asset_status=asset["status"], analysis_status=analysis,
                           prepare_status="blocked", readiness="blocked", live_ready=False, blockers=blocks,
                           legacy_library_id=None))
    counts = dict(lock["counts"], chart_count=len(charts),
                  verified_chart_count=sum(x["archive_status"] == "verified" for x in charts),
                  verified_audio_count=sum(assets[x["file"]]["status"] == "verified" for x in audio_targets.values()),
                  failure_count=sum(x["status"] == "failed" for x in assets.values()),
                  missing_count=sum(x["status"] in ("missing", "lfs_pointer") for x in assets.values()),
                  analysis_failed_count=sum(x["analysis_status"] == "failed" for x in charts),
                  unsupported_format_count=sum(x["analysis_status"] == "unsupported_format" for x in charts),
                  live_ready_count=0)
    complete = all(x["status"] == "verified" for x in assets.values())
    return dict(schema=LIBRARY_SCHEMA, source=source, counts=counts, status="complete" if complete else "incomplete",
                songs=lock["songs"], charts=charts, device_discovery=False, input_enabled=False,
                source_lock=dict(file="manifests/source-lock.json", sha256=hashlib.sha256(json_bytes(lock)).hexdigest()),
                qualification="Asset integrity and structural summaries only; no installed-game or playback qualification.")
