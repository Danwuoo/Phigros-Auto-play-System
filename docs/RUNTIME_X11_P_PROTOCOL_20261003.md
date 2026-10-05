# X11-P runtime 重建與冷成本預先 protocol

2026-10-03 Asia/Taipei。待總控獨立驗收。只準備 X12；emulator、ADB probe、真觸控、manual-session/live、模型、commit/push 均不執行。main HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c，保留全部 dirty。

## 來源與功能門檻（量測前固定）

B0 從 saved commit 98169a575bd7c3b7503849ceb4b91a934d50ce0f 完整 archive；以 tint1 freeze 所列 source snapshots 核對 SHA/normalized content並採用原 CMake。B1 的唯一 decision semantic change 是 main50 已驗 8 行 pending missing hook。兩版共用實驗 metadata、離線 harness 與建置設定；不帶研究 instrumentation、suppression、第二策略或 LibTorch。原 live binary、重建 B0、B1 分別列 SHA；未列 live 編譯輸入的來源證明不足要明列，不能說 byte-identical。

完整 pas.exe/DLL closure、compiler/flags/proto/dependency/profile freeze；新的 compiled provenance 必須列 saved parent、實驗 variant 及實際 source hash，禁止 git rev-parse 向上找到 main。metadata 離線輸出不得連裝置。正式 checkout src/include/root CMake 不修改。

新完整 suite 保留 baseline 的 pending grace test 期待；B1 唯一容許失敗是 SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt。其他 unexpected failure/skip 要說明並阻止 ready。重用 X10d-P 的 13 契約 tests，只把 introspection assertion 換成 public cursor/receipt 等價驗證，差異另保存；prefix_offset/unknown/completed 保持原證據並增加無 instrumentation 橋接。研究 core 与新 core 的同 input/FakeClock/public events bridge 必須一致；完整 replay 最多兩次，必要才執行，不默認原 replay 屬於新 runtime。

## 固定負載與 run 配額

自有 C++20 離線 harness，連結各完整 build 的 uninstrumented pas_core。使用 HostClock/QPC、LatestFrame 三 physical buffers/一 latest、單 owner、同成本 bounded Journal 與 fake backend。immutable lease 的 published_ns 是 consumer 的唯一發布時間。各 run 前生成固定 stimulus，不把生成/PNG decode 算入 recognition。來源 render age、真 capture/RPC、遊戲採納未知。

1. RGB：1280×720，16 種固定合成 pose，playing HUD、長白線、接近 Tap/Drag/Flick 與 Hold rails，短缺 current note。512 publish attempts，cadence16ms，固定四相 jitter {-2,+1,+2,-1}ms。pixels 真正進原 GameObserver，原 C36h constructor defaults。
2. owner：相同 latest-frame transport，直接產生明示 synthetic DecisionSnapshot（不是 observer accuracy）；128 targets 上限，混合四 kind pending 反覆 missing/fresh-return、同 ID rootless rotating active Hold/body 支持。512 attempts，cadence8ms，同 jitter。這是 owner 壓力及 cancellation 成本，不能說 RGB 全鏈成本或實景語義。

所有診斷 drain/enqueue 同邊界，完整 receipts（非只 Down）與全部 capture attempts 記錄。per-frame sample、receipt 數、writer mailbox/row size/file bytes、core scheduler/plans/contacts 都有硬限。

Cost 總額 **24 runs**：每 workload 先 B0 A/A 4 runs（8 total），凍結噪聲；之後每 workload 2 批 ABBA（16 total），顺序 B0/B1/B1/B0。只按這個順序，A/A 完成前不看 B1 cost。所有 run≤512 attempts≤2000；沒有 long stability run。額外 stress 每版最多2 runs（4 total），各256 attempts：RGB 慢 consumer24ms/cadence8ms/writer delay5ms/mailbox8/fake RPC3ms；owner 同延遲但 consumer0，檢查 capacity/幀間 due。stress 不作速度改善 gate。量測器失敗保留，最多一次工程修復，失敗也佔24配額；不另找安靜样本。

## A/A 冻结公式及 A/B gate

每 workload 的 A/A 為順序 pairs (1,2),(3,4)。metric：recognition cost（owner stream 是 adapter assembly）、owner accept、同 frame capture→owner、receipt scheduled→injection_start，報 n/p50/p95/p99/max、p95−p5 jitter。對前三 metric 的 p95/p99 各取 pair 最大絕對差 d；容忍 T=max(固定 floor,2d)。floor：recognition0.5ms、owner0.25ms、capture→owner3ms。噪聲 adequacy：2d≤max(各4run中最小metric×0.30, floor)。失敗即 cost gate unknown/not-ready，不再放寬。A/A 原數據及 T 在 B1 首 run 前獨立保存。

每批 ABBA 比較兩次 B0 metric 的算術平均与兩次 B1 的算術平均（不是 pooled percentile）；各 workload 每批三 metric/p95,p99 的 B1−B0≤T。所有批必须通過，不要求提速或 action 數相同。非預期動作差靠 causal contract 判斷；已知14 lost opportunity仍未知。

normal hard gate：failed/unknown receipt=0，contacts_at_exit=0，worker error=0，writer critical fault=0，frame時間順序capture≤ready≤published≤recognition_end≤owner_end；published+pool_drop=attempts；consumed≤published、owner≤consumed，normal consumed及owner≥attempts×0.90。p99 capture→owner≤100ms、max≤250ms；scheduled lateness p99≤15ms、max≤100ms。超界不得用排除失敗/成功Down子集掩蓋。stress 允許skip/drop/debug writer loss，禁止殘留contact/unknown/無界積累；晚觸控完整報。

## 容量與安全

新 measurements 根 runtime-x11-p ≤128MiB，campaign+research-next仍≤8GiB；起點量得8,116,471,003B。原PNG只引用。out/x11-p 完整兩份 export/build/closure 加 harness≤3GiB；disk free 保留3GiB+128MiB+5GiB。全部 logs/failed artifacts/source/protocol/input/SHA計額。freeze 後不重build；舊 out/x1..x10d-p 不變，不刪 object 或 raw。

通過來源、橋接、regression、成本噪聲及 capacity gates 才標 ready-for-controlled-live；若缺口不能冷解，完整凍結研究結果為 not-ready。X12 預案最多6輪 B0/B0/B0/B1/B1/B0（同profile/lead35/capture256KiB，前兩輪A/A，後四輪ABBA）；現場重新preflight指紋、unknown/無法釋放立即停止，所有中止保留。X11不執行、不承諾77 Miss下降，完成停止待總控獨立驗收。
