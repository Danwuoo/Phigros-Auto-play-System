"""Recompute the campaign, source comparability and load, and write a Markdown table."""
import argparse
import json
from pathlib import Path

from pas.telemetry import distribution
from unified_capture_bench import dump, recompute, sha


def pause_observations(path):
    events=[json.loads(line) for line in path.open(encoding="utf-8")]
    pauses=[e for e in events if e["event"]=="receiver_pause"]
    if not pauses:return None
    pause=pauses[-1]
    start=pause.get("start_ns",pause.get("started_ns"))
    if start is None:return {"error":"missing pause start"}
    end=pause.get("ended_ns",start+round(pause.get("duration_ms",500)*1e6))
    frames=sorted((e for e in events if e["event"]=="capture" and e.get("source_timestamp_us") is not None),
                  key=lambda e:e["capture_complete_ns"])
    before=[e for e in frames if e["capture_complete_ns"]<start]
    if not before:return {"error":"missing anchor"}
    anchor=before[-1]
    after=[e for e in frames if end<=e["capture_complete_ns"]<end+1_000_000_000]
    points=[]
    for e in after:
        same_generation=e.get("stream_generation")==anchor.get("stream_generation")
        relative=((e["capture_complete_ns"]-anchor["capture_complete_ns"])/1e6-
                  (e["source_timestamp_us"]-anchor["source_timestamp_us"])/1e3) if same_generation else None
        points.append({"frame_sequence":e["frame_sequence"],"source_sequence":e.get("source_sequence"),
                       "after_pause_start_ms":(e["capture_complete_ns"]-start)/1e6,
                       "extra_relative_lag_ms":relative})
    return {"start_ns":start,"end_ns":end,
            "end_basis":"measured" if pause.get("ended_ns") else "requested duration; actual thread resume not logged",
            "first_second_after_end":points,
            "interpretation":"Difference of within-clock deltas relative to last pre-pause frame; not absolute age"}


def summarize(root):
    manifest=json.loads((root/"manifest.json").read_text(encoding="utf-8"))
    runs=json.loads((root/"results.json").read_text(encoding="utf-8"))
    loads=[json.loads(line) for line in (root/"host_load.jsonl").open(encoding="utf-8")]
    for run in runs:
        directory=root/run["name"]
        common=recompute(directory/"capture.jsonl")
        run["common"]=common
        samples=[s["host_busy_percent"] for s in loads if s["host_busy_percent"] is not None and
                 common["measurement_start_ns"]<=s["start_ns"] and s["end_ns"]<=common["measurement_end_ns"]]
        run["host_busy_percent"]=distribution(samples)
        run["cli_summary"]=json.loads((directory/"stdout.json").read_text(encoding="utf-8"))
        run["pause_observations"]=pause_observations(directory/"capture.jsonl")
        run["png_sha256"]=sha(directory/"capture.png")
        common["research_thresholds_met"]=(common["fixture_distinct_hz"]>=55 and
               common["arrival_interval_ms"]["p95"]<=33.4 and common["arrival_interval_ms"]["p99"]<=50)
    normal=[r for r in runs if r["condition"]=="normal"]
    source_rates=[r["common"]["fixture_counter_span_hz"] for r in normal]
    ratio=max(source_rates)/min(source_rates) if source_rates and min(source_rates)>0 else None
    unchanged={name:Path(name).exists() and sha(Path(name))==digest for name,digest in manifest["code_sha256"].items()}
    payload_normal=[r for r in normal if r["key"] in ("T","P")]
    diagnostic_normal=[r for r in normal if r["key"]=="M"]
    complete=(len(runs)==len(manifest["cases"]) and
              all(r.get("returncode")==0 and r["common"]["geometry_valid"] and
                  r["common"]["fixture_decoded_fraction"]==1 for r in runs))
    summary={"complete":complete,"normal_count":len(normal),
             "normal_source_max_min_ratio":ratio,"source_rate_comparable":ratio is not None and ratio<=1.05,
             "all_normal_sources_near60":bool(source_rates) and all(57<=r<=63 for r in source_rates),
             "normal_research_pass_count":sum(r["common"]["research_thresholds_met"] for r in normal),
             "payload_normal_numeric_pass_count":sum(r["common"]["research_thresholds_met"] for r in payload_normal),
             "diagnostic_mmap_numeric_pass_count":sum(r["common"]["research_thresholds_met"] for r in diagnostic_normal),
             "payload_normal_source_near60_count":sum(57<=r["common"]["fixture_counter_span_hz"]<=63 for r in payload_normal),
             "source_code_unchanged":all(unchanged.values()),
             "changed_files":[name for name,same in unchanged.items() if not same],
             "manifest_sha256":sha(root/"manifest.json"),"host_load_sha256":sha(root/"host_load.jsonl"),
             "runs":runs}
    dump(root/"analysis.json",summary)
    def f(value):return f"{value:.2f}" if value is not None else "unknown"
    def dist(d):return " / ".join(f(d[k]) for k in ("p50","p95","p99","max")) if d["n"] else "n=0"
    lines=["# 統一擷取重測數據（2026-09-25）","",
       f"完整批次：{summary['complete']}；程式雜湊未變：{summary['source_code_unchanged']}。",
       f"正常來源 max/min={f(ratio)}，近似同來源率（≤1.05）：{summary['source_rate_comparable']}；所有批次接近 60 Hz：{summary['all_normal_sources_near60']}。","",
       f"原研究數值門檻：payload {summary['payload_normal_numeric_pass_count']}/{len(payload_normal)}；MMAP 診斷 {summary['diagnostic_mmap_numeric_pass_count']}/{len(diagnostic_normal)}。payload 接近 60 Hz 的批數 {summary['payload_normal_source_near60_count']}/{len(payload_normal)}；MMAP 即使數值通過也不具 Session 像素一致性資格。","",
       "T=thread payload；P=process payload；M=process MMAP，僅診斷。主機負載為全部 logical CPUs 正規化 busy%，每秒採樣，只選完整落在正式窗口的樣本。","",
       "| 批次 | 秒數 | 收到 / 消費 / 不同 counter | 來源跨度 Hz | 不同 counter/s | 到達間隔 n；p50 / p95 / p99 / max ms | 主機 busy n；p50 / p95 / p99 / max % |",
       "| --- | ---: | ---: | ---: | ---: | --- | --- |"]
    for r in runs:
        c=r["common"];a=c["arrival_interval_ms"];h=r["host_busy_percent"]
        lines.append(f"| {r['name']} | {f(c['window_s'])} | {c['capture_events']} / {c['consumer_events']} / {c['fixture_distinct']} | {f(c['fixture_counter_span_hz'])} | {f(c['fixture_distinct_hz'])} | {a['n']}; {dist(a)} | {h['n']}; {dist(h)} |")
    lines.extend(["","主機駐留 = consume_ns − capture_complete_ns，不是來源絕對年齡。process 的 capture 時間由 child 記錄，包含後續 IPC／parent 等待。","",
       "| 批次 | 駐留 n；p50 / p95 / p99 / max ms | consumer skip | IPC 覆蓋 | buffer 未讀覆蓋 | capture 錯誤 / child 回收 |",
       "| --- | --- | ---: | ---: | ---: | --- |"])
    for r in runs:
        c=r["common"];h=c["host_residency_ms"];s=r["cli_summary"]
        delta=s.get("window_counter_delta",s.get("window_counters_delta",{}))
        shutdown=s.get("capture_shutdown",{})
        lines.append(f"| {r['name']} | {h['n']}; {dist(h)} | {c['consumer_sequence_skips']} | {delta.get('ipc_overwrites','N/A')} | {s.get('frames_overwritten_unconsumed',delta.get('latest_overwritten','N/A'))} | {s.get('capture_error',s.get('error'))} / {shutdown.get('child_reaped','N/A')} |")
    lines.extend(["","停頓後相對落後：以停頓前最後一張為錨點，主機時間差減來源時間差。不是兩種 clock 的絕對相減，也不是來源絕對年齡。首張時間自實測恢復時間起算；舊日誌若無 ended_ns 則標示推算。","",
        "| 批次 | 恢復依據；停頓實際 ms | 恢復後首張 ms / 額外相對落後 ms | 後 1 秒內相對落後 n；p50 / p95 / p99 / max ms |",
        "| --- | --- | --- | --- |"])
    for r in runs:
        p=r["pause_observations"]
        if not p or not p.get("first_second_after_end"):continue
        points=p["first_second_after_end"];first=points[0]
        d=distribution([v["extra_relative_lag_ms"] for v in points if v["extra_relative_lag_ms"] is not None])
        actual_ms=(p['end_ns']-p['start_ns'])/1e6
        after_resume_ms=(first['after_pause_start_ms']-actual_ms)
        lines.append(f"| {r['name']} | {p['end_basis']}; {f(actual_ms)} | {f(after_resume_ms)} / {f(first['extra_relative_lag_ms'])} | {d['n']}; {dist(d)} |")
    lines.extend(["","## 原始 JSONL SHA-256","","| 批次 | SHA-256 |","| --- | --- |"])
    for r in runs:lines.append(f"| {r['name']} | `{r['common']['raw_sha256']}` |")
    lines.extend(["",f"manifest SHA-256：`{summary['manifest_sha256']}`。",
                  f"host_load JSONL SHA-256：`{summary['host_load_sha256']}`。",""])
    (root/"tables.md").write_text("\n".join(lines),encoding="utf-8")
    print(json.dumps({k:v for k,v in summary.items() if k!="runs"},indent=2))


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("root",type=Path)
    args=parser.parse_args()
    summarize(args.root)
