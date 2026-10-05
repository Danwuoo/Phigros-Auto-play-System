# X11-P runtime／成本結果

2026-10-03 Asia/Taipei。**工程交付完成，待總控獨立驗收；freeze 狀態 not-ready，沒有 X12 live 資格。** 成本 gate failed，沒有調門檻、追加 run 或混第二策略救結果。C36h tint1 仍 behavioral/experimental baseline；main50 仍 donor/control live0；X10b suppression OFF 且否決，X10d-O 未啟動。

## 交付與來源

預先 [protocol](RUNTIME_X11_P_PROTOCOL_20261003.md) 在 export/build/量測前保存，實際 snapshot 為 `measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/protocol-before.md`。完整 [batch](../measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p) 保存每次命令、exit、logs、XML、raw frames/receipts、所有成功與負結果；[final-summary](../measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/final-summary.json) 是容量及 SHA 入口。

| binary | 路徑 | SHA-256 |
|---|---|---|
| 原 tint1 live，沒有重建 | `acceptance36h-01/candidate36h-tint1-runtime/pas.exe` | `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9` |
| 新完整 B0 | `out/x11-p/runtime-B0/pas.exe` | `81cb1cf57939e0020dabb503aba5b43cee1eb1503a27f7a4f34d6cab08939c14` |
| 新完整 B1 | `out/x11-p/runtime-B1/pas.exe` | `0b93f561c3ed4011db8f3c252e3f97781d321468a7d9501e87944d531da3112f` |

三者不同，不以 observer37/planner19 號碼混同。新 metadata 標籤為 `C36h-tint1-X11-P-B0/B1`。保存提交 `98169a575bd7c3b7503849ceb4b91a934d50ce0f` 的完整 archive 加 tint1 原 snapshot 建出 B0；B1 唯一 decision semantic 差異是已驗 8 行 pending-missing hook。src/include 全檔 inverse patch 還原 B0，兩版 metadata 改動對稱。沒有借 bench 搬 main50 observer、association、claim、alias 或 threshold。

26 個 freeze snapshot SHA 全符；保存提交 normalized content 23 符、CMake／分析文件2項不同、舊 generated provenance1項不作 Git 比較。採用 frozen CMake 再做明列的共用 build metadata 改動；不複製原 generated provenance。完整 parent archive 共83個 src/include/apps/proto/cmake/tests輸入條目。**原 live freeze 沒列的編譯輸入（例如 runtime.cpp、grpc_transport.cpp、bench與額外 headers）缺原 compiled SHA，不能回溯宣稱全部與原 live binary 相同。** saved commit、原 recovery patch、listed snapshots 支持可重建 lineage；新 binaries 的完整編譯輸入另凍結，不把歷史缺口填成已驗。

runtime `pas.exe x11-provenance` 在 CLI dispatch 前純離線輸出 parent/variant/actual source hashes，逐項重新 hash 符合；manual-session manifest 也有新 variant。兩版完整 pas.exe、core/emulator/bench/proto、DLL closure、CMake cache/vcxproj flags、1490 個實際 CL/link dependencies＋compiler/proto tools SHA 已保存。VS2026 v145／MSVC19.51.36256.0，Windows11 10.0.26200，Core Ultra5 125H 14C/18T，約32GiB RAM。沿用既有 patched gRPC1.81.1 與 out/vcpkg_installed，沒有安裝或替換依賴。遞迴 PE imports 核對到同目錄 closure 或本機 OS/API sets，無 Torch/c10；OS 本身不是可攜打包的一部分。

generated `compiled_source_sha256` 是export source superset（亦列未連入pas的離線CPU工具原始碼），不是聲稱每檔皆編譯；真正compiled/header/link輸入以實際tlogs的dependency freeze為準。未連模型仍由runtime import/link closure確認。

正式 checkout src/include/root CMake 對 HEAD 無差異；main f83c7ea、唯一 registered worktree，原 dirty 保存。舊 X10d-P source/export/binary bindings 重新核對，原 tint1 live binary 也核 SHA；沒有覆寫 frozen out/x1..x10d-p、刪 raw、commit/push、開新 chat/goal。

## 功能與橋接

| 驗證 | 結果／限制 |
|---|---|
| B0 完整原 suite | 238 distinct passed；首次237 pass/1 historical path skip，補只讀 junction 後單項1/1，原 skip log保留 |
| B1 完整原 suite | 237 pass/1 預先聲明契約 fail，0 skip/error/disabled |
| B0 新 pending contracts | 8 pass/5 預期 red，與原 X10d-P 分類相同 |
| B1 新 pending contracts | 13/13 pass，0 skip/error/disabled |
| 原 RGB | 27張 opt-in 已啟用，另含既有完整原 suite 的 RGB／session／capacity／loopback tests；未連 emulator |
| 研究核心→新核心 public bridge | 每版512固定 synthetic inputs/FakeClock；每版 events bytes 全相同、contacts0。B0 receipts177、B1 receipts127 |
| 新 concurrency／memory 風險 | 新 uninstrumented B1 core＋新 harness Debug-ASan，256 attempts stress，0 sanitizer report/worker error/failed/unknown，contacts0 |
| 安全 CLI／拒絕 | 兩版 help/provenance 純離線成功；無 subcommand／錯 workload／existing output 拒絕，0 publish、舊 output SHA不變 |

唯一原 suite fail 仍為 `GameOwner.SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt` 的兩個 pending_count=1 舊期待，candidate為0；原期待沒有改或過濾。新13項測 pending／active／unknown／completed／missing cursor、fresh-return、due前／同時／後、capture-ready交錯、gate/reset/context、旋轉 rootless Hold、alias及shared Drag。移除研究 getter 後，兩個 introspection assertions 改為 public contact/receipt 檢查，原 test source、轉換後 source 及差異都保留；absolute prefix 算術仍由既有已驗ASan test和未改 scheduler source支持，不能說新test重新讀到了private alias表。

bridge source 直接連各 frozen research library 和新完整 pas_core，沒有跨版 ABI 或 decoder instrumentation。去除差異限3個診斷 cpp、2個 const getter header，manual_session則是共用 metadata；diff逐檔保存。**沒有新 full-prefix PNG replay**（配額0/2）；X10d-P的7722幀/2165→2131 actions屬原凍結研究 binary，不能重標為本次新runtime實跑。source inverse＋同input/clock bridge支持冷策略保持，未證明新runtime所有實景非同步路徑或原live byte identity。

## 成本與排程判定

QPC頻率10MHz。24個 Release cost runs全部按預先8 A/A＋16 ABBA順序；每run512 attempts。4 stress各256 attempts，B1 owner那筆用Debug-ASan、佔其第二個stress額度，不當Release效能樣本。合計13312 publish attempts全部published，pool_drop0、consumed12709、owner_seen12420、consumer skip603、decision skip289、fake failed/unknown0、所有run contacts_at_exit0。634筆 writer debug rejection均在stress，沒有丟掉它們。

輸入 RGB SHA `1a50b115a612d3528de2de9c225482d329b0c6a3baf10d3506ac6e47d9f082ee`；16種pose與 owner adapter code SHA在 source binding。RGB是真 GameObserver；owner負載是128個synthetic typed targets，不是 RGB extraction 或 physical gold。128 pending/5 contacts容量達到；旋轉活動 Hold 正確續接另由13項契約test獨立驗證，不以密集負載contact總數保證每個fixture Hold的完整生命週期。

量測每次 capture_complete/pixels_ready/published/recognition_start/end/owner_start/end、全部phase scheduled/injection_start/return，raw完整留存。published取 immutable lease，producer publish_cost只在join後讀，沒有重引入9/29 published race。recognition／owner cost及capture→owner是同frame直接差；百分位不相加。owner accept本身不含diagnostic drain，但 end-to-end受前次drain/enqueue負載影響；完整accepted/coverage/cancel/notices全排空計數，writer只寫等價compact records，不是正式archive完整JSON序列化。

**A/A noise gate failed，在第一筆B1 cost之前凍結：**

| 指標 | 2×最大pair差 ms | 預先noise上限 ms | 結果 |
|---|---:|---:|---|
| RGB recognition p99 | 1.7936 | 1.60878 | failed |
| dense owner accept p99 | .2980 | .2500 | failed |

之後 ABBA 維持原容忍。兩批 owner p99 非退步也 failed：

| dense owner batch | B0兩次run的p99平均 ms | B1兩次run的p99平均 ms | B1−B0 | T |
|---|---:|---:|---:|---:|
| 1 | .88725 | 1.35380 | +.46655 | .29800 |
| 2 | .98895 | 1.44580 | +.45685 | .29800 |

這些是「run percentile 的算術平均」，不是 pooled p99或跨曲平均。其餘22個預先ABBA metric/quantile比較通過，不能覆蓋兩個fail。另 baseline `ab-owner-1-4-B0` receipt lateness p99=15.0906ms，越15ms normal hard timing gate；其餘normal所有消費／owner分母均≥90%，capture→owner p99/max未越100/250ms。沒有重跑挑安靜樣本，也没有因少Down宣稱改善。密集B1每run1183次hook cancel，B0不走此hook；它是刻意有動作／生命週期差的壓力測試，不能強求動作數相同。

完整每run的每項 n/p50/p95/p99/max/p95−p5 jitter、取消/拒絕/late及原樣summary見 `all-cost-runs.json` 和各run raw。RGB ABBA B0 Down各96，B1為95/94/95/96，Move均128；owner Down受排程與hook各為55或59／B1均55，不是遊戲效果證據。

| stress | consumed /256 | owner_seen | consumer / decision skip | writer drop | pending/contact peak | between-frame receipts |
|---|---:|---:|---|---:|---|---:|
| RGB B0 | 67 | 65 | 189 /2 | 23 | 3 /3 | 18 |
| RGB B1 | 67 | 61 | 189 /6 | 20 | 3 /3 | 17 |
| owner B0 | 240 | 181 | 16 /59 | 303 | 128 /4 | 32 |
| owner B1 ASan | 234 | 174 | 22 /60 | 288 | 128 /3 | 0 |

所有stress硬容量/收尾gate通過；ASan與Release timing不比較。慢consumer24ms、writer5ms、假RPC3ms均實際造成負載，假RPC返回受OS排程可大於3ms。stress lateness p99約32–38ms，完整reject/late留在summary，不包裝成一般正常時序通過。writer峰8 rows且byte峰有記，physical buffers3/latest1/decision1；sample257或513、receipt32768、writer row≤64KiB/總16MiB硬限。runtime原本的SessionArchive8192/16MiB mailbox、32×16MiB輪journal、16線/128note/6點近期history、scheduler128/16steps/5contacts由source及原regression核對，沒有把它們冒稱本harness量到的RSS峰。

## 保留的失敗及未知

首次 B0 harness compile 發現舊Journal無3參數delay constructor；工程修一次，改用harness-only bounded writer而不移植runtime。protocol-before不回寫，修正記 `meter-repair-before-cost.json`，cost attempts當時0。ASan首次link因14.50整個lib目錄搶到舊STL、缺14.51 vectorized symbols；改只copy ASan libs，失敗log保留。dependency inventory首次遇static target沒有Link項，補null guard。首次historical data path skip及維護查詢的shell錯誤也列入negative/admin ledger，不宣稱所有歷次操作全成功。

**Unknown/缺口：**正式 manual-session 的高解析Wake與SessionArchive完整序列化/磁碟競爭沒有被此1ms condition-variable harness等價量測；未量真gRPC、source render age、裝置狀態、觸控採納、physical identity或逐note判定。fake release_all固定成功且清空map；本meter未保存每次release-call分母，不把zero contacts推論為真RPC釋放驗收。來源未列原livecompiled SHA的缺口仍在。既有14組lost opportunity的遊戲利弊、77 Miss下降、跨曲泛化與human gold仍Unknown。

freeze為研究結果，不是產品baseline、live-ready或AP完成。下一步由總控獨立核對後決定是否另立有界成本／正式archive-wake驗證；原24 cost/4 stress額度已用完。X12只準備[操作預案與交接](RUNTIME_X11_P_HANDOFF_20261003.md)，本task不執行。
