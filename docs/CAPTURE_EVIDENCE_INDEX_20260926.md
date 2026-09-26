# 五擷取研究證據索引（2026-09-26）

本輪依最新縮短要求收尾。原 r4 96 批日程未完成，縮短六批 6/6 完成；性能數值維持 null。完整結果見 [比較報告](CAPTURE_COMPARISON_20260926.md)。

## 原始資料與結案狀態

| 資料 | 狀態與用途 |
| --- | --- |
| [r1](../measurements/five_formal_20260926_r1/) | source 3f5671e；兩批 gRPC 後 WGC 關閉崩潰，WER／partial raw 保留 |
| [r2](../measurements/five_formal_20260926_r2/) | source 6fa6dbde；76 批、69 成功／7 失敗；舊長測不代替新版；另有暫停舊圖缺陷 |
| [r3](../measurements/five_formal_20260926_r3/) | source 4913edc；完成 11/96 後停止，色彩 counter 異常與中斷保留 |
| [r4 原計畫](../measurements/five_formal_20260926_r4/campaign-plan.json)／[原結果](../measurements/five_formal_20260926_r4/campaign-results.json) | source 2d43dd7；40 批已記錄（37 成功／3 失敗），原 complete=false 不改寫 |
| [原日程結案分類](../measurements/shortened_followup_20260926_01/original-campaign-closure.json) | 列明 3 個失敗、1 個末行截斷的 partial、55 個未開始，以及原 supplement／manual-fault 取消 |
| [r4 凍結 C++ 重算](../measurements/five_formal_20260926_r4/partial-analysis.json) | 成功批次 hash／APK／binary／環境／品質有效；原全矩陣仍 invalid/incomplete |
| [短補測預寫日程](../measurements/shortened_followup_20260926_01/followup-plan.json)／[結果](../measurements/shortened_followup_20260926_01/followup-results.json) | 48 Hz、3 秒暖機、四批 20 秒恢復、兩批 120 秒短期穩定性；6/6 成功，raw 獨立重算一致 |
| [時限／清理](../measurements/shortened_followup_20260926_01/cleanup.json) | 實測至清理 QPC wall 606.27 秒；無 PAS 殘留，freeze=0；沒有建立長測排程 |
| [逐批分布](../measurements/shortened_followup_20260926_01/run-metrics.md)／[詳細資料](../measurements/shortened_followup_20260926_01/report-data.json) | 每批完整窗口 n、p50／p95／p99／max、來源跟隨、stage、CPU／RSS／GPU、品質與丟棄／跳過 |
| [consumer 恢復推導](../measurements/shortened_followup_20260926_01/consumer-recovery-derived.json) | 原 summary null 保留；另列 scheduled threshold→匹配 counter 圖的 QPC 差，含 ADB 等待 |
| [凍結 EXE／DLL](../measurements/frozen_2d43dd7_release/hashes.json) | 正式／短補測均從同一副本執行；舊版副本分別保留 |
| [CTest 紀錄](../measurements/r4_ctest_verified.log) | Release 29/29；此次縮短收尾沒有重新建置或重跑無關測試 |

## Hash catalogue

[capture_research_evidence_index_20260926.json](../measurements/capture_research_evidence_index_20260926.json) 列出 **810 個檔案**的相對路徑、bytes 與 SHA-256，涵蓋各版成功／失敗／partial raw、manifest、PNG、summary、WER／退出紀錄、凍結檔、相關 smoke、目前分析與短補測記錄。不存在的 summary 不補造。這些資料位於 ignored `measurements/`，需保留該目錄才可離線重算。

| 固定輸入 | SHA-256 |
| --- | --- |
| r4 Release EXE | `2447a4143a4b35f045f22e9345b462b4ed176bee20db9a937bfc38ab7ecc0d68` |
| 位置／靜止新版 Fixture APK | `195b6a0673c5d2182fe9b9face60cbfae8457dcf4c46bb82a873ad99d35fe964` |
| 官方 scrcpy v4.1 server | `deacb991ed2509715160ffdc7907e47b4160eb30d1566217e9047fd5b8850cae` |
| 完整 evidence index | `8c22ce24040b8698253091ba4e843058f5d3dfacb0ba6ec5daa8c94a84bcdc70` |

短補測時 source/code 和二進位未變，工作目錄只有本輪文件修訂。各批原始 manifest 與 analyzer 輸出保持原值；本索引不將不同版本、不同時長或不完整窗口混算。
