# 新 R2 成本量測的獨立容量宣告

2026-10-07，所有舊12/16GiB receipt、STOP、編譯及 raw 保留，不改原件。
本宣告在新 source3f 的 AA/ABBA/stress 啟動前凍結，不授予任何成本或 live 資格。

目前全部 retained prelive 約13.75GB；新三配置核心/測試仍在建置。
舊 tapaa01 的單筆 Tap journal≈11.22MB、Hold≈13MB、dense≈414.67MB。
完整 AA12 至少需4個dense；ABBA36含12個dense及其他全量事件與各stage收據。
原累積16GiB不足以同時保留舊負結果、新三配置和這兩個完整批次。

只為新的必要離線成本階段，預先固定：

- retained prelive 累積硬上限 **32GiB**，C槽 free≥20GiB，raw/native-stage reserve仍1GiB。
- 新候選建置/功能根的 **4GiB** 硬上限照舊；AA/ABBA/stress各為獨立新成本根。
- 新成本原始資料合計≤12GiB；任一raw run的1GiB / 65536 rows / 262144B record 硬界不變。
- 每一 native-stage metadata≤64MiB；整批 metadata AA≤64MiB、ABBA≤128MiB；全新成本階段metadata≤256MiB。
- 原4MiB process stdout/stderr cap、64 held-identity cap、input closure8192 files及所有程序生命週期資格不變。

若必要測試超過這些預聲明上限，停止並留下capacity負證據，不刪除舊raw或縮分母。
此容量只控制落盤留存；程式pool/mailbox/RSS、cadence、coverage、latency、phase、noise、
nonregression、SCENE和正常批次停止条件全不修改。建置16GiB收據不升格為32GiB收據。
真正emulator/RPC/physical及完整IN錄製要另有當次raw容量帳本；本宣告不自動啟動裝置。
