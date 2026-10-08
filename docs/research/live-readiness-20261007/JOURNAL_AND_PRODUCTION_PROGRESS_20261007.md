# 2026-10-07 接續開發：R2 STOP、production 稽核與序列化候選

本檔追加後續證據，不回寫 README 中較早 AA04 的失敗，也不續跑任何 stopped campaign。
使用者已要求繼續開發；裝置選 emulator，manual-session 仍由使用者選曲及按 Play。
目前 live 入口 UNQUALIFIED；當前 emulator serial/app/profile/五指與遊戲語義未核。
全曲完整 IN Miss=0 未完成，章節分母及逐曲 IN 解鎖仍 unknown。

## 已完成、可核的成果

| 項目 | source / 證據入口 | 結果與範圍 |
| --- | --- | --- |
| R2 Tap absence/reentry、owner/producer 控制 | `8e953c723efe5ed97af51184739673f15ed8af03`；`out/prelive-owner-wake-tap05/candidate-builds.json` | Release/Debug/ASan 各 169 控制，合計 507 通過 |
| 新版本完整 pixels 前綴 | `out/prelive-owner-wake-tap05/new-candidate-prefix-receipt.json` | 3063/3063，自己的 Down/Move/Up=55/121/51；獨立 raw audit 通過；沒有 physical gold |
| R2 成本 AA | `out/prelive-aa-tapaa01/status.json` | Tap 4、Hold 4 通過；dense1 必要 coverage/n 不通過即 STOP，餘 3 筆 dense/noise/ABBA 未跑 |
| 真實 R2 收據唯讀 CLI | `out/prelive-cost-receipt-r2-reader01/evaluation.json` | 9 項通過；保留真實 PASS/STOP，拒絕錯 SHA/commit、重複 key、無效 runner/身分/audit、覆寫輸出；不產生 live token |
| 當前 production 編譯及入口拒絕 | `out/prelive-production-evaluation-prodclosed02/build-binding.json`；`out/prelive-production-entry-evaluation-prodentry01/evaluation.json` | 真正 main 及五 libraries 編譯；8 項原 production admission 拒絕控制；main 未執行 |
| literal production 三執行緒 synthetic probe | `out/prelive-production-entry-evaluation-prodpipe03/evaluation.json` | 原 `manual_session.cpp`、Packet/Wake/高精度 timer/SessionPerception/owner/Archive；Tap 1000@20ms 完成；test-double capture/touch，未連裝置 |
| 獨立 production raw auditor | `out/prelive-production-entry-evaluation-prodaudit03/evaluation.json` | 全 attempts、packet/capture、plan/receipt、release、segment SHA 和分位數重算；smoke n 不夠 normal 資格，照報 NOT_READY |
| production auditor 控制 | `out/prelive-production-audit-controls-paudit03/evaluation.json` | 10 項通過：authentic 及 9 拒絕控制；前兩次維護腳本失敗保留 |

dense1 的完整 coverage=0.868359375；measurement n=1980/2304（0.859375），原門檻為
coverage≥0.9 且 measurement n≥2000。capture→owner p99=57.440522ms、max=75.9201ms；
producer/publication overruns=0、pool/writer drops=0、release verified。完整分母與失敗不縮減。
此結果阻止 noise/ABBA，不因另一個 smoke 的數值较低而升格。

literal production smoke 的 measurement 744/744；capture→owner_return p99=15.975994ms、
max=21.7365ms；observer p99=6.008612ms；phase lateness p99=6.978345ms。
raw 1005 render attempts / 1004 published：最後一筆是 synthetic result 完成後、mailbox
關閉時已啟動的 tail attempt，稽核只允許最多一筆、且必須晚於 round_stop；前 1000
正常 attempts 仍全部必需。不把這項 test-double 規則當成真 capture RPC 合格。

## 新序列化候選

`C:/Users/wurre/Desktop/PAS-journal-cost-20261007`，branch `codex/journal-cost-20261007`，
source `3f23faf8bb91a4a974c33463e7232d3507c1dcc1`。原來源 `8e953c7` 保持 frozen。

Journal 新增 opaque prepared record，保留原 clock/schema、critical/debug overflow 及 drain。
成本 host 只編碼一次後入列；原 262144B / 65536 rows / 1GiB 硬界及 128B 餘量保留。
SessionArchive ordinary event 在 mutex 外編碼一次，writer 直接寫 cached row，鎖內依 cached
bytes 解扣原 8192 rows / 16MiB；completion summary 仍保留 JSON 由 writer 加 SHA/result。
不修改 observer、association、ledger、scheduler、expiry、unknown Down 或 admission。

新編碼、UTF8、容量、debug drop/critical fault、drain、原 archive/lifecycle，加上原 owner /
producer / Tap / shared seam / finish / current/front/bridge/tail 控制，正在三配置驗證。
`journal01` Release 22 個 formal core object 已全由此 source 重編譯，後續 original session
test 因 standalone CMake 漏 `PAS_TEST_SOURCE_DIR` 而停止。`journal03` 的 Release 使用
**同一新 source** 已核的完整 core，current libraries/host/tests 全部重建；Debug/ASan
完整 core 重建。維護 wrapper 補與原根測試相同的資料路徑、ASan 傳至全部 test TU。
不借不同 source 的舊 core，也不改測試 oracle。

新 source 的 cost tools 已建置並通過 analyzer 42 項及 metrics 22 項。
`out/prelive-journal-cost-tools-evaluation-journal03/build-binding.json` 保留原維護 driver
誤讀 `pass` 為 `failed` 的停止，並由新增 completion recipe 核對原成功 native receipts。

`out/develop-journal03-all-configurations.ps1` 是目前三配置再建置/回歸入口；
`out/run-journal03-prefix.ps1` 與 `out/run-journal03-r2-aa.ps1` 已準備，必須等前者完整成功
及新完整 3063 prefix/audit，才允許新 AA。原 R2 corpus、cadence、n、warmup、cost/noise/RSS
門檻不變；所有新 run 為 fresh output，前一個 STOP 不恢復。

## 到 ready 仍缺的必要項

1. 新序列化 source 的三配置回歸、完整前綴及 production source 同源重建/稽核。
2. 新 AA12 全部必要成本通過，再凍结充分 noise，才跑 36 筆 ABBA；C5 改善另列。
3. 必要 fault/slow/writer/RPC/stress 1000、Tap/dense 10000、Debug/ASan 1000 等成本回歸。
4. 當前 emulator/app/profile/五指、真正 capture/RPC、physical Hold 接入/body/tail 和 release。
5. 完整 live receipt validator / typed qualified token / 啟用 CLI；cold/AA/comparison 不代替授權。
6. 由使用者選曲及按 Play 的有限遊戲驗收、逐曲 IN 解鎖及完整 IN Miss=0 原始結算。

量測資料與新建置保留原件。新維護 storage 宣告累積≤16GiB、每候選≤4GiB、
metadata≤64MiB、free≥20GiB、build/raw reserve=1.5/1GiB；不改程式 buffer 或成本門檻。
