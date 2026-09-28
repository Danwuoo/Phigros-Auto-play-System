"""Audit the completed main-strategy Legacy session and its manually read result PNGs."""
from __future__ import annotations

import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SESSION = ROOT / "measurements/game-assist/manual-session-108176133899800"
PREVIOUS = ROOT / "docs/HD9_LEGACY_HD_RESULTS_EVIDENCE_20260928.json"
OUTPUT = ROOT / "docs/MAIN_LEGACY_HD_RESULTS_EVIDENCE_20260928.json"

# Direct, offline readings of the 21 same-capture result PNGs, in round order.
# (song, difficulty, level, score, Perfect, Good, Bad, Miss, Max Combo, ACC)
ROWS = [
    ("Eradication Catastrophe", "HD", 7, 818000, 178, 0, 0, 22, 34, 89.00),
    ("Credits", "HD", 10, 582690, 224, 3, 0, 128, 35, 63.65),
    ("Dlyrotz", "HD", 9, 961572, 452, 0, 0, 6, 336, 98.69),
    ("Dlyrotz", "IN", 13, 634461, 395, 5, 6, 178, 121, 68.19),
    ("Engine x Start!! (melody mix)", "HD", 8, 919792, 187, 0, 0, 5, 83, 97.40),
    ("光", "HD", 7, 943762, 312, 1, 0, 2, 159, 99.25),
    ("光", "IN", 12, 799603, 443, 7, 0, 67, 106, 86.57),
    ("Winter ↑ cube ↓", "HD", 8, 865601, 419, 3, 0, 27, 98, 93.75),
    ("混乱-Confusion", "HD", 10, 743692, 376, 1, 0, 101, 165, 78.80),
    ("Cipher", "HD", 10, 879884, 445, 1, 0, 27, 151, 94.22),
    ("FULL AUTO SHOOTER", "HD", 9, 704846, 295, 1, 0, 93, 81, 76.00),
    ("HumaN", "HD", 8, 903153, 212, 0, 0, 10, 97, 95.50),
    ("[PRAW]", "HD", 10, 895722, 548, 4, 0, 23, 195, 95.76),
    ("Cereris", "HD", 10, 855164, 656, 2, 0, 43, 79, 93.77),
    ("Pixel Rebelz", "HD", 9, 751257, 412, 1, 0, 92, 80, 81.71),
    ("Non-Melodic Ragez (MUG Edit)", "HD", 11, 887993, 642, 7, 3, 23, 175, 95.79),
    ("Sultan Rage", "HD", 7, 817320, 292, 1, 0, 41, 96, 87.62),
    ("Class Memories", "HD", 10, 897218, 743, 3, 0, 25, 213, 96.62),
    ("-SURREALISM-", "HD", 9, 764396, 465, 2, 0, 104, 168, 81.66),
    ("Bonus Time", "HD", 9, 909078, 392, 4, 0, 16, 194, 95.78),
    ("ENERGY SYNERGY MATRIX", "HD", None, 865043, 545, 5, 0, 30, 83, 94.53),
]


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def main() -> None:
    root_manifest = json.loads((SESSION / "manifest.json").read_text(encoding="utf-8"))
    root_summary = json.loads((SESSION / "summary.json").read_text(encoding="utf-8"))
    assert root_manifest["game_observer_version"] == 36
    assert root_manifest["game_planner_version"] == 18
    assert root_manifest["source_build_dirty"] is False
    assert root_manifest["pixel_clip_sampling"] is True
    assert root_manifest["capability_preflight"]["fingerprint_matches"] is True
    assert root_summary["state"] == "STOPPED"
    assert root_summary["pool_drops"] == 0
    assert len(ROWS) == 21

    index = [json.loads(line) for line in (SESSION / "rounds.jsonl").read_text(encoding="utf-8").splitlines()]
    assert [entry["round_id"] for entry in index] == list(range(1, 22))
    rounds = []
    for number, (song, difficulty, level, score, perfect, good, bad, miss, combo, accuracy) in enumerate(ROWS, 1):
        folder = SESSION / f"round-{number}"
        summary_path = folder / "summary.json"
        manifest_path = folder / "manifest.json"
        result_path = folder / "result.png"
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        assert index[number - 1]["summary_sha256"] == sha(summary_path)
        assert summary["manifest_sha256"] == sha(manifest_path)
        assert summary["result_image_sha256"] == sha(result_path)
        assert summary["status"] == "result_confirmed"
        assert summary["release_failed_ids"] == summary["release_unknown_ids"] == []
        for segment in summary["event_segments"]:
            assert segment["sha256"] == sha(folder / segment["path"])
        judgments = perfect + good + bad + miss
        rounds.append({
            "round_id": number, "song": song, "difficulty": difficulty, "level": level,
            "score": score, "perfect": perfect, "good": good, "bad": bad, "miss": miss,
            "judgments": judgments, "perfect_percent": round(100 * perfect / judgments, 2),
            "max_combo": combo, "accuracy_display_percent": accuracy,
            "result_path": relative(result_path), "result_sha256": summary["result_image_sha256"],
            "summary_path": relative(summary_path), "summary_sha256": sha(summary_path),
            "duration_s": round((summary["round_stop_ns"] - summary["round_start_ns"]) / 1e9, 3),
            "decisions": summary["decisions"], "commands": summary["commands"],
            "timing": {key: {metric: summary[key][metric] for metric in ("n", "p50", "p95", "p99", "max")}
                       for key in ("playing_capture_interval_ms", "recognition_ms", "host_residency_ms")},
        })

    assert all(round["miss"] > 0 for round in rounds)
    assert [r["round_id"] for r in rounds if r["difficulty"] == "IN"] == [4, 7]
    assert rounds[2]["song"] == rounds[3]["song"]
    assert rounds[5]["song"] == rounds[6]["song"]
    hd = [r for r in rounds if r["difficulty"] == "HD"]
    assert len(hd) == len({r["song"] for r in hd}) == 19

    old = json.loads(PREVIOUS.read_text(encoding="utf-8"))
    old_hd = {r["song"]: r for r in old["rounds"] if r["status"] == "result_confirmed"}
    assert len(old_hd) == 19
    comparisons = []
    for current in hd:
        prior = old_hd[current["song"]]
        assert prior["difficulty"] == "HD"
        assert current["judgments"] == prior["judgments"]
        comparisons.append({
            "song": current["song"], "new_round_id": current["round_id"],
            "old_session": prior["session"], "old_round_id": prior["round_id"],
            "old_score": prior["score"], "new_score": current["score"],
            "score_delta": current["score"] - prior["score"],
            "old_miss": prior["miss"], "new_miss": current["miss"],
            "miss_delta": current["miss"] - prior["miss"],
        })
    assert next(c for c in comparisons if c["song"] == "Credits")["old_round_id"] == 14

    clips_root = SESSION / "pixel-clips"
    clips_index = clips_root / "index.jsonl"
    samples = [json.loads(line) for line in clips_index.read_text(encoding="utf-8").splitlines()]
    clip_counts = Counter()
    trigger_counts = Counter()
    raw_bytes = 0
    for sample in samples:
        path = clips_root / sample["path"]
        assert sample["round_id"] <= 20
        assert sample["width"] == 1280 and sample["height"] == 720 and sample["stride"] == 3840
        assert sample["layout"] == "top_down_rgb888"
        assert path.stat().st_size == 2_764_800
        assert sha(path) == sample["sha256"]
        clip_counts[sample["round_id"]] += 1
        trigger_counts[sample["clip_trigger"]] += 1
        raw_bytes += path.stat().st_size
    assert len(samples) == root_summary["pixel_clips"]["frames_written"] == 558
    assert root_summary["pixel_clips"]["frames_dropped_queue_or_error"] == 0
    assert raw_bytes <= root_summary["pixel_clips"]["max_raw_total_bytes"]

    evidence = {
        "schema": 1, "date": "2026-09-28",
        "source": "offline manual reading of same-capture result PNGs and exhaustive archive/clip hash audit",
        "session_path": relative(SESSION), "session_manifest_sha256": sha(SESSION / "manifest.json"),
        "session_summary_sha256": sha(SESSION / "summary.json"),
        "rounds_index_sha256": sha(SESSION / "rounds.jsonl"),
        "executable_sha256": root_manifest["executable_sha256"],
        "source_build_commit": root_manifest["source_build_commit"],
        "source_build_dirty": root_manifest["source_build_dirty"],
        "observer": 36, "planner": 18, "capture_geometry": root_manifest["config"]["capture"],
        "clock_domain": "host_qpc_ns", "session_state": root_summary["state"],
        "hash_audit": {"all_round_manifests_events_summaries_results_verified": True,
                       "all_pixel_clip_frames_verified": True},
        "counts": {"confirmed_results": 21, "hd_results": 19, "in_results": 2,
                   "distinct_song_labels": 19, "all_perfect_results": 0,
                   "capture_pool_drops": root_summary["pool_drops"],
                   "consumer_skips": root_summary["consumer_skips"]},
        "in_rounds": [{"hd_round": 3, "in_round": 4, "song": "Dlyrotz"},
                      {"hd_round": 6, "in_round": 7, "song": "光"}],
        "rounds": rounds,
        "hd9_comparison_policy": "same song and HD difficulty; Credits uses second confirmed prior HD result B14",
        "hd9_comparisons": comparisons,
        "pixel_clips": {"index_path": relative(clips_index), "index_sha256": sha(clips_index),
                        "frames": len(samples), "rounds_with_samples": len(clip_counts),
                        "frames_per_round": dict(sorted(clip_counts.items())),
                        "trigger_frame_counts": dict(trigger_counts), "raw_bytes": raw_bytes,
                        "round_21_sampling": "not collected; fixed 20-round cap",
                        "copy_distribution_ms": root_summary["pixel_clips"]["copy_distribution_ms"],
                        "queue_peak": root_summary["pixel_clips"]["peak_queue"],
                        "queue_drops": root_summary["pixel_clips"]["frames_dropped_queue_or_error"]},
        "limits": {"score_source": "result PNG only; no runtime OCR or chart access",
                   "same_song_cross_version": "descriptive comparison, not causal attribution",
                   "source_absolute_age": None,
                   "round_21_has_result_but_no_pixel_clips": True,
                   "energy_synergy_matrix_level": "last digit obscured by artwork; unknown"},
    }
    OUTPUT.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"results": len(rounds), "hd": len(hd), "in": 2,
                      "improved_score": sum(c["score_delta"] > 0 for c in comparisons),
                      "reduced_miss": sum(c["miss_delta"] < 0 for c in comparisons),
                      "clips": len(samples), "raw_bytes": raw_bytes,
                      "output": relative(OUTPUT)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
