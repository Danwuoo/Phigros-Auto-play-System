"""Bind an offline chapter archive/readiness report to a finite manual IN test queue."""
import argparse
import hashlib
import json
from pathlib import Path


def read_json(path, limit=16 * 1024 * 1024):
    path = Path(path)
    if path.stat().st_size > limit:
        raise ValueError("JSON byte budget exceeded")
    raw = path.read_bytes()
    return json.loads(raw.decode("utf-8-sig")), hashlib.sha256(raw).hexdigest()


def digest(path, limit=64 * 1024 * 1024):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError("Missing file or artifact byte budget exceeded")
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def contained(root, relative):
    root = Path(root).resolve()
    value = Path(relative)
    if value.is_absolute():
        raise ValueError("Expected a relative archive artifact")
    path = (root / value).resolve()
    if not path.is_relative_to(root):
        raise ValueError("Artifact escapes its archive")
    return path


def reference(path, expected=None):
    path = Path(path).resolve()
    actual = digest(path)
    if expected is not None and actual != expected:
        raise ValueError("Artifact SHA256 mismatch")
    return dict(file=str(path), sha256=actual)


def prepare(archive, readiness, output, asset_root=None):
    archive, readiness, output = map(lambda p: Path(p).resolve(), (archive, readiness, output))
    if output.exists():
        raise FileExistsError("Sweep output must be new")
    asset_root = Path(asset_root).resolve() if asset_root else archive.parent
    report, report_sha = read_json(readiness / "summary.json")
    reported_index = Path(report.get("chapter_index_file", str(archive / "chapter-index.json")))
    index_path = reported_index.resolve() if reported_index.is_absolute() else contained(archive, reported_index)
    if not index_path.is_relative_to(archive):
        raise ValueError("Chapter index escapes archive")
    index, index_sha = read_json(index_path)
    if index.get("schema") not in ("pas.phigros-chapter-archive.v1", "pas.phigros-chapter-verified-index.v1") or report.get("schema") != "pas.chapter-legacy-IN-readiness-batch.v1":
        raise ValueError("Unsupported archive/readiness schema")
    if report["chapter_index_sha256"] != index_sha:
        raise ValueError("Readiness belongs to a different chapter index")
    members, observations = index["songs"], report["songs"]
    if not 1 <= len(members) <= 64 or len(members) != len(observations):
        raise ValueError("Readiness must contain every chapter member exactly once")
    expected = [song["track_id"] for song in members]
    if len(set(expected)) != len(expected) or [row["track_id"] for row in observations] != expected:
        raise ValueError("Duplicate, missing or reordered chapter member")
    jobs = []
    for ordinal, (member, row) in enumerate(zip(members, observations), 1):
        choices = [chart for chart in member["charts"] if chart["difficulty"] == "IN"]
        if len(choices) != 1 or row.get("order") != ordinal:
            raise ValueError("Exactly one IN source and native chapter order are required")
        selected = choices[0]
        if row["relative_chart_path"] != selected["chart"]["file"]:
            raise ValueError("Readiness chart path differs from archive")
        chart = reference(contained(archive, selected["chart"]["file"]), selected["chart"]["sha256"])
        profile = None
        if row.get("profile_path"):
            profile_path = contained(readiness, row["profile_path"])
            candidate, _ = read_json(profile_path)
            if (candidate["song_id"] != member["track_id"] or candidate["difficulty"] != "IN"
                    or candidate["chart_sha256"] != chart["sha256"]
                    or candidate["expected_plan_sha256"] != row["compiled_fingerprint"]):
                raise ValueError("Prepared profile does not bind the same native chart/plan")
            profile = reference(profile_path)
        binding = row.get("ui_binding", row.get("UI_binding", {}))
        state, hud = None, None
        if binding.get("present"):
            state = reference(contained(asset_root, binding["state_file"]), binding["state_sha256"])
            hud = reference(contained(asset_root, binding["hud_file"]), binding["hud_sha256"])
        blockers = list(row.get("blockers", []))
        if profile is None and not any(item.get("category") == "missing_profile" for item in blockers):
            blockers.append(dict(category="missing_profile", reason="No bounded native opening/full-plan profile was produced"))
        if (state is None or hud is None) and not any(item.get("category") == "missing_UI_binding" for item in blockers):
            blockers.append(dict(category="missing_UI_binding", reason="A validated or explicitly experimental common IN UI binding is required"))
        jobs.append(dict(order=ordinal, track_id=member["track_id"], difficulty="IN", rounds=1,
                         chart=chart, profile=profile, state_calibration=state, hud_reference=hud,
                         ready_for_manual_preflight=not blockers, blockers=blockers,
                         qualification_warnings=list(row.get("qualification_warnings", [])),
                         measured_cost_replays_qualified=row.get("all_measured_cost_nextwake_replays_complete"),
                         ui_evidence_scope=binding.get("evidence_scope"),
                         actual_per_song_pixel_validation_qualified=binding.get("actual_per_song_pixel_validation_qualified", False),
                         notes=row.get("notes"), events=row.get("events"), peak=row.get("peak"),
                         compiled_fingerprint=row.get("compiled_fingerprint"),
                         duration_seconds=row.get("duration_seconds"),
                         opening_model_status=row.get("opening_model_status"),
                         result=None, live_status="NOT RUN"))
    ready_count = sum(job["ready_for_manual_preflight"] for job in jobs)
    plan = dict(schema="pas.chapter-IN-sweep-plan.v1", chapter_id=index.get("chapter_id", "MainStoryLegacy"),
                difficulty="IN", song_count=len(jobs), round_limit=len(jobs),
                status="prepared" if ready_count == len(jobs) else "prepared_with_blockers",
                ready_count=ready_count, blocked_count=len(jobs) - ready_count,
                chapter_index=dict(file=str(index_path), sha256=index_sha),
                readiness_summary=dict(file=str(readiness / "summary.json"), sha256=report_sha),
                archive_reported_status=index.get("status"),
                policy=dict(manual_selection=True, manual_start=True, automatic_retry=False,
                            stop_on_failure=True, score_submission=False, experimental_phase_ms=-80,
                            require_current_boot_kernel_profile=True, require_current_boot_five_contact_evidence=True),
                ready_scope="bounded artifact/model candidate for one controlled manual trial; runtime guards remain active; no complete-song performance or gameplay qualification",
                device_discovery=False, input_enabled=False, gameplay_qualified=False,
                jobs=jobs)
    output.mkdir(parents=True, exist_ok=False)
    (output / "sweep-plan.json").write_text(json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--readiness", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--asset-root", type=Path)
    args = parser.parse_args()
    result = prepare(args.archive, args.readiness, args.output, args.asset_root)
    print(json.dumps({key: result[key] for key in ("status", "song_count", "ready_count", "blocked_count")}))


if __name__ == "__main__":
    main()
