"""Build a local manual-test library without changing native chapter membership."""
import argparse
import copy
import json
from pathlib import Path

from prepare_chapter_sweep import read_json, reference


def checked_job(raw):
    job = copy.deepcopy(raw)
    if job.get("difficulty") != "IN" or job.get("rounds") != 1:
        raise ValueError("Library entries require one IN round")
    track = job.get("track_id")
    if not isinstance(track, str) or not track or len(track) > 256:
        raise ValueError("Invalid track identity")
    for key in ("chart", "profile", "state_calibration", "hud_reference"):
        item = job.get(key)
        if not isinstance(item, dict):
            raise ValueError("Missing prepared library artifact: " + key)
        job[key] = reference(item["file"], item["sha256"])
    profile, _ = read_json(job["profile"]["file"], 32768)
    if (profile.get("schema") != "pas.adaptive-song-profile.v1"
            or profile.get("song_id") != track or profile.get("difficulty") != "IN"
            or profile.get("chart_sha256") != job["chart"]["sha256"]
            or profile.get("expected_plan_sha256") != job.get("compiled_fingerprint")):
        raise ValueError("Library profile/chart/plan identity mismatch")
    for key, field in (("notes", "expected_contacts"), ("events", "expected_steps"), ("peak", "expected_peak")):
        if job.get(key) != profile.get(field):
            raise ValueError("Library count differs from pinned profile: " + key)
    if (not 1 <= job["events"] <= 16000 or not 1 <= job["notes"] <= job["events"] // 2
            or not 1 <= job["peak"] <= 5):
        raise ValueError("Library entry exceeds native experiment resource bounds")
    if job.get("source_manifest"):
        item = job["source_manifest"]
        job["source_manifest"] = reference(item["file"], item["sha256"])
        source, source_sha = read_json(item["file"], 1024 * 1024)
        if (source.get("track_id") != track or source.get("difficulty") != "IN"
                or source.get("chart", {}).get("sha256") != job["chart"]["sha256"]
                or source.get("decoded_audio", {}).get("sha256") != profile.get("audio_sha256")
                or source_sha != profile.get("source_manifest_sha256")):
            raise ValueError("Additional song source identity mismatch")
    return job


def build_library(plan_path, extras, output):
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError("Library output must be new")
    plan_path = Path(plan_path).resolve()
    plan, _ = read_json(plan_path)
    if (plan.get("schema") != "pas.chapter-IN-sweep-plan.v1"
            or plan.get("chapter_id") != "MainStoryLegacy"
            or plan.get("song_count") != 20 or plan.get("round_limit") != 20
            or len(plan.get("jobs", [])) != 20):
        raise ValueError("The formal Chapter Legacy plan must retain exactly 20 native entries")
    songs, seen = [], set()

    def append(raw, display_name, group, role, native_order):
        job = checked_job(raw)
        identity = job["track_id"] + ":IN"
        if identity in seen:
            raise ValueError("Duplicate library song/difficulty")
        seen.add(identity)
        job.update(library_id=identity, display_name=display_name or job["track_id"],
                   group=group, role=role, native_order=native_order,
                   default_selected=bool(job.get("ready_for_manual_preflight") and not job.get("blockers")))
        songs.append(job)

    for path in extras:
        extra, _ = read_json(path)
        if extra.get("schema") != "pas.extra-song-library-entry.v1":
            raise ValueError("Unsupported additional-song descriptor")
        if not extra.get("job", {}).get("source_manifest"):
            raise ValueError("Additional song requires native source provenance")
        append(extra["job"], extra.get("display_name"), "Additional", "warmup", None)
    for ordinal, job in enumerate(plan["jobs"], 1):
        if job.get("order") != ordinal:
            raise ValueError("Native chapter order must not be rearranged")
        append(job, job.get("display_name", job["track_id"].split(".")[0]),
               "Chapter Legacy", "chapter", ordinal)
    if len(songs) > 64:
        raise ValueError("Library entry budget exceeded")
    result = dict(schema="pas.local-song-library.v1", source_plan=reference(plan_path),
                  song_count=len(songs), chapter_count=20, extra_count=len(songs) - 20,
                  phase_ms=-80, songs=songs, device_discovery=False, input_enabled=False,
                  gameplay_qualified=False,
                  scope="Finite operator-selected IN candidates; extras do not change native chapter membership")
    output.mkdir(parents=True, exist_ok=False)
    (output / "library.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--extra", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = build_library(args.plan, args.extra, args.output)
    print(json.dumps({key: result[key] for key in ("song_count", "chapter_count", "extra_count")}))


if __name__ == "__main__":
    main()
