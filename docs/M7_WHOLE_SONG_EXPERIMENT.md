# Dlyrotz IN13 全曲譜面實驗

更新：2026-10-24。**Live011 已取得一次 Dlyrotz IN13 AP：1,000,000分、584 Perfect／0 Good／0 Bad／0 Miss、Max Combo584、100.00%、Early/Late0。** 原生譜面由首音gRPC觀測觸發，完整584 contacts／5,669events完成，manual-mode確認結算、failure null、最後kernel zero。結果由獨立sidecar核對，原manifest保持unreviewed；單輪AP不提升其他時間／生產資格，音訊仍暫緩。

## 執行範圍

`apps/chart_experiment/main.cpp` 的 `chart_experiment` 是明確 opt-in 的實驗入口，固定 Phigros **4.0.1／157**、Dlyrotz IN13、`emulator-5554`／`phigros`、1280×720 畫面與既有 landscape touch mapping。原生 chart SHA 為 `aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90`。每次由原譜重新編譯並核對完整 **584 contacts／peak 4**；Build001–008為4,911steps，成功AP的Build009為 **5,669steps**，不裁成開場片段。

6 個畫面外目標／路徑保留 D250 bounded-normal-intersection 候選。這些接點仍需實際遊戲校正，`requires_calibration=true` 保持不變；固定 4 點來自先前 fixture 所測能力，不代表 backend 最大容量。

Build001 的開場由原始視覺候選與三個實際 evidence IDs 取得 provisional origin。原始 receipt 與固定 rate 1、顯式 phase（當時 **−80 ms** 假設）產生全曲 deadline；後續 UI receipt 只授予短期輸入許可，不能更新歌曲 origin、冒充 capture timestamp、獨立 input lead 或 SyncLease。生產 `SessionRunner` 的 live gate 未變更。下面的 Build001 測試與裝置紀錄保留原範圍；新的首音觸發設計另列，不能繼承為已實測。

**Build001–005 的歷史實作**沿用既有 PAS 的 rolling-prefix 方法：

- 100 ms lookahead 逐 contact 納入，保留 2 秒 scheduler admission horizon；不是把整首一次送進 scheduler。
- 每份 contact plan 最多 32 steps，保留最後已執行步驟、下一段 source steps 與真正的最終 UP；`prefix_offset` 維持原始步驟序號。scheduler 的 2 秒門檻作用在 plan 第一個 deadline，長 Hold 的真實 UP 可在其後。
- `coverage_until` 先檢查舊視窗是否漏掉 MOVE，再允許 revision；Build001 在缺少 coverage、UI unknown／paused／100 ms expiry、前景失效、晚於 20 ms 或 transport fault 時停止並釋放，不能靠新觀測抹掉已過期區間。新的實驗 UI watchdog 與 anchor acquisition 分開設定，見下節。
- 通用實驗 runtime 有 180 秒歌曲長度、2,000 contacts／20,000 steps 的上限；CLI 再固定為本曲 584／4911／4。首 DOWN 前需當次 read-only kernel zero；正常與異常路徑都另做 release／final-slot 核對。

一輪 watchdog 180 秒、整個 CLI session 240 秒；Esc／Ctrl+C／輸出目錄的 `STOP` 可停止。這些是 cooperative deadlines；保留 ADB 的 process termination／reader join 仍沒有硬性 wall-time 保證。完成 RPC sequence 後等待 manual-mode 結算，分開記錄輸入序列是否完成與結果畫面是否取得。RPC、kernel zero 或 hit effect 都不能直接當 Perfect。

**Build006 起的 runtime** 改為逐 contact 保留完整未執行 tail，更新實際 UI receipt 時只移除已成功執行的前綴，仍留下最後一個已執行 step。全曲總預算維持 **20,000 steps**，同時作為單份 plan 的硬上限；仍在 DOWN 前 **100 ms** 才 admission，保留 **2 s first-step horizon**，沒有一次 admission 整首。所有原生 MOVE、座標與 deadline 完整保留；逐 step **20 ms lateness**、100 ms anchor age、500 ms actual-UI watchdog 不變。這取代舊 32-step 人工 coverage 界線，使同一次 `run_due()` 內的非零 RPC 耗時不會耗盡預截斷視窗。

**Build007 的實驗選項**只把 Tap／Drag 排定 DOWN→UP 從 16 ms 改成 **32 ms**，compiler 預設保持 16 ms，Flick／Hold 的全路徑與全部 DOWN 不變；phase 仍為 −80 ms。另預配最多 67,410,720 bytes 保存 50 ms 以上 receipt 間距的 score／combo 灰階 ROI，採樣不刷新 UI permission，釋放後才寫檔。Live009 是這個版本的一輪完整觀察，不能單憑分數差推論因果。

**成功Build009設定**：Flick lead30 ms／travel60 ms／位移60 reference pixels／末端停留24 ms，Tap／Drag32 ms、phase−80 ms。480個非Flick contacts與010相同，104個Flick重編譯後增加758steps；完整fingerprint固定為 `74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5`。

## 已通過的核對

| 範圍 | 結果與界線 |
| --- | --- |
| Build001 runtime 合成契約（歷史版本） | **14 組／11,973 checks**；完整 584／4911／peak 4、1,002-step 長 Hold、immutable prefix、漏 MOVE coverage-stop、舊 UI expiry、unknown injection／release 等。沒有裝置操作。 |
| Build001 原生 Dlyrotz 全曲 FakeTouch（歷史版本） | **19,650 checks**；584 contacts 完成、4911 events 的 deadline／phase／source／pixel 完整吻合；active 峰值 4、pending 峰值 8、6 個投影候選保留，最後 UP **121.216 s**，最後接點為空。 |
| Build001 非音訊完整建置（歷史版本） | **22/22 CTest**；包括 retained PAS、visual／opening、experimental runtime 與 anchor monitor 等各自契約。不是遊戲成績，也不是新首音觸發入口的整合測試。 |
| 新 UI-watchdog options 的 runtime scratch 驗證 | **18 組／11,998 checks**、MSVC `/W4 /WX /permissive-` build、**1/1 CTest** 通過；新增 500 ms exact-boundary／實際 receipt renewal、anchor 仍限 100 ms、11.8 s 的二證據 anchor、立即 pause release 與選項上限。只驗 runtime 合成契約，首音辨識／整合／live 不在此結果內。 |
| Build003 首音觸發整合 | 非音訊完整 **23/23 CTest**、default **4/4 CTest**；實際首音原生 positive-approach／向下移動證據與 500 ms UI watchdog 接回完整譜面 runtime。這份建置通過不包含後續 live005 暴露的 immutable-prefix revision 問題已修復之聲明。 |
| Build004 revision 修正 | 新版 runtime **20 組／12,087 checks**、非音訊完整 **23/23 CTest**、default **4/4 CTest**；針對 live005 的 owner preemption／真 UP 邊界回歸通過。Live006 使用此 binary 越過舊停止位置；deferred-tail counter 為 0，這輪未實際觸發修正分支。 |
| Build006 完整 tail／owner wait | Runtime **23 組／12,582 checks**、scratch **1/1 CTest**，完整非音訊 **24/24 CTest**、default **4/4 CTest** 通過。包含 live007 非零 RPC 時序、owner 跨舊視窗、超過 20 ms 必停與 live005 UP 邊界回歸。Live008 已完整送完並取得結算，結果另列；該輪AP未達成。 |
| [Build007 Tap／Drag 32 ms 與 HUD timeline](../../local-assets/phigros-4.0.1/m7-whole-song-build-007/manifest.json) | 完整非音訊 **25/25 CTest**、default **4/4**；診斷採樣的固定預算、實際來源 metadata、無配置與不改觀測狀態有離線檢查。Live009 使用此版本完成整曲，但有 5 Miss，未取得 AP。 |
| [Build008 Flick 末端停留 24 ms](../../local-assets/phigros-4.0.1/m7-whole-song-build-008/manifest.json) | **25/25 CTest、default 4/4**；只有 104 個 Flick UP 延後 20 ms，其餘 4,807 events 不變。Live010 完整完成，結算 580 Perfect／4 Miss。 |
| [Build009 60 ms Flick軌跡](../../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json) | **25/25 CTest、default4/4**；完整584／5669／peak4及固定fingerprint。Live011獨立結算為 **584 Perfect／零Good／Bad／Miss，AP**。 |

私譜回放使用合成精確 clock、20 ms UI observations 與 1 ms scheduler ticks，不把這些數值宣稱為實機精度。相同 timestamp／phase 的跨接點 tie order 在 scheduler 採 birth intent、compiler 採 source ID；驗證以 time／phase／source canonical 排序核對完整多重集合，所有原始事件仍需存在且座標不變。

[Build001 manifest](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/manifest.json) 保存來源、binary、DLL 與日誌身份；[22/22 CTest](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/whole-song-ctest-001.log) 及[私譜回放紀錄](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/experimental-whole-private-replay-003.log)可分別核對。`chart_experiment.exe` SHA256：`6d013855a95053dba77385cb04f4a00f0ccf44a975c7f002df73c77c6d580726`。這些本機原件位於 repo 外，不隨其他 clone 提供。

第一次 launch 在 endpoint discovery 階段以 `emulator discovery lacks valid port or token` 拒絕；[stderr 原件](../../../work/whole-song-live-001.stderr.log)保留。沒有建立 live 輸出目錄或送出音符輸入，不能計為全曲演奏。auth 啟動已修正，當次 boot／route 重新盤點，使用 `full-song-kernel-profile-002.json`（boot `07be8914-d124-44e8-bb8b-904b8fe4541d`）；不能沿用舊 boot 的 kernel profile。

| 全曲嘗試 | 實際記錄 |
| --- | --- |
| [Live001](../../local-assets/phigros-4.0.1/m7-whole-song-live-001/manifest.json) | 起始黑畫面尚未 ready 時由 owner 寫 STOP，`user_stop`；**0 DOWN／0 UP、release RPC known、final kernel zero**。 |
| [Live002](../../local-assets/phigros-4.0.1/m7-whole-song-live-002/manifest.json) | 成功取得原始三證據 anchor（song interval 約 **11.110–11.124 s**）並載入全部 584／4911；其後 UI receipt 過期，記錄 `actual_UI_or_foreground_invalid`，在第一筆輸入前停止。**0 DOWN／0 UP、release RPC known、final kernel zero**。 |
| [Live003](../../local-assets/phigros-4.0.1/m7-whole-song-live-003/manifest.json) | 未建立 anchor；候選觀測中斷造成 `visual_epoch_invalidated: observation gap`，在輸入前停止。**0 DOWN／0 UP、release RPC known、final kernel zero**。 |
| [Live004 launch failure](../../local-assets/phigros-4.0.1/m7-whole-song-live-004/launch-failure.json) | 首音 trigger constructor 在 transport／READY 前以 `earlier positive native approach remains ambiguous` 拒絕；把不在可見 approach band 的較早正速段也列為歧義。**0 DOWN／0 UP**；這次沒有一般 run manifest，也不能宣稱取得當次 final kernel 觀測。 |
| [Live005](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/manifest.json) | Build003 首音觸發成功後實際送出 **53 DOWN／64 MOVE／51 UP，共 168 筆成功 RPC 回執**；在 `immutable_prefix_revision_rejected` 停止。runtime 回報完成 51 contacts，停止時另釋放 IDs **0／1**，release RPC known，最後 pinned `/dev/input/event2` 為 zero。後續獨立觀察結算為 **53 Perfect／0 Good／0 Bad／531 Miss**。 |
| [Live006](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/manifest.json) | Build004 實際送出 **150 DOWN／874 MOVE／150 UP，共 1,174 筆成功 RPC 回執**，完成 150 contacts，之後因 `actual_UI_receipt_expired` 停止。停止時沒有待釋放 ID，release known、最後 pinned kernel zero。後續獨立結算為 **150 Perfect／0 Good／0 Bad／434 Miss**。 |
| [Live007](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/manifest.json) | Build005 的 HUD fallback 用於 **226 個實際來源畫面**，越過先前約 39 s 的停止區域。送出 **196 DOWN／1,514 MOVE／195 UP，共 1,905 筆成功 RPC**，在約 song **47.25 s** 因 `compiled_trajectory_coverage_expired` 中止；release known、最後 pinned kernel zero。獨立結算為 **196 Perfect／0 Good／0 Bad／388 Miss**。 |
| [Live008](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/manifest.json) | Build006 完成 **584 DOWN／3,743 MOVE／584 UP，共 4,911 筆成功 RPC**，全部 contacts 完成；manual-mode 確認結算，failure null、release known、最後 pinned kernel zero。獨立目視 **581 Perfect／0 Good／0 Bad／3 Miss**，score **963,014**、Max Combo **395**、Accuracy **99.49%**、Early/Late 0。 |
| [Live009](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/manifest.json) | Build007 完成 **584 DOWN／3,743 MOVE／584 UP、4,911 成功 RPC**，failure null、manual-mode 確認結算、release known、最後 kernel zero。獨立目視 **579 Perfect／0 Good／0 Bad／5 Miss、score 936,815、Max Combo 260、Accuracy 99.14%、Early/Late 0**；使用者回報 Miss 都是 Flick，逐 note 對應另待核對。 |
| [Live010](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/manifest.json) | Build008 完成 **584／4911**，failure null、manual-mode 結算、release known、final kernel zero；獨立目視 **580 Perfect／0 Good／0 Bad／4 Miss、929,110 分、Max Combo 206、99.32%、Early/Late 0**。使用者仍回報 Miss 都是 Flick；逐 note 配對另行分析。 |
| [Live011](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/manifest.json) | Build009完成 **584 DOWN／4,501 MOVE／584 UP＝5,669events**，failure null、manual-mode結算、release known、final kernel zero。獨立目視 **1,000,000分、584 Perfect／零Good／Bad／Miss、MaxCombo584、100.00%，AP**。 |

前四次沒有送出音符 DOWN；live005–007 均在部分輸入後中止；live008 首次完整送完並取得結算，live009／010亦完成全曲，分別有5／4 Miss；live011取得一次AP。各次結果只對應當次 binary，不能互相繼承資格。

## 首音觸發接續與 live005–011

Build003 已整合由 gRPC 畫面辨認第一顆原生 Tap 的 unique positive approach 與兩次實際向下移動，再依固定 origin／rate 1／phase 執行完整譜面；live005 已走過這條觸發路徑。明示的 **500 ms UI receipt watchdog** 與歌曲對時分開：runtime 的舊預設仍為 100 ms，首音模式可選 500 ms。只有實際 playing 觀測能更新 permission receipt；遇 unknown 保留最後實際 playing receipt 並讓它自然到期，不能把 unknown 或 polling 時間偽裝成 playing 新證據。明確 paused／result 立即撤銷並釋放。

Anchor acquisition 仍固定 **100 ms**，不因 UI watchdog 變長而延長；允許明示 song interval 至 **11.95 s**、2–3 個不同實際 evidence IDs。Step lateness 預設及上限仍為 **20 ms**。全譜候選與 `requires_calibration=true` 保留，不產生 SyncLease 資格。[Build003 archive](../../local-assets/phigros-4.0.1/m7-whole-song-build-003/manifest.json) 保存 **11,998-check runtime 紀錄**、[23/23 完整 CTest](../../local-assets/phigros-4.0.1/m7-whole-song-build-003/first-note-full-ctest-002.log)與 default 4/4。`chart_experiment.exe` SHA256 為 `db56c232051c5356819dd254121d0992ba8a9d9d7885c4840db36df23363a81c`，與 live005 manifest 一致；archive 的 `not_run_at_archive_time` 保留原意，不回寫後續結果。

既有首音離線回放原件已逐檔複製至 [first-note replay archive](../../local-assets/phigros-4.0.1/m7-first-note-trigger-replay-001/manifest.json)，本次沒有重跑。影片 [video-report](../../local-assets/phigros-4.0.1/m7-first-note-trigger-replay-001/video-report.json) 把 PTS 明示當合成 receipt，在 PTS 15.712 s 以兩筆向下證據觸發，預留約 339.003 ms；這不是實際 host clock。舊 gRPC PNG 的 [actual-report](../../local-assets/phigros-4.0.1/m7-first-note-trigger-replay-001/actual-report.json) 使用原 receipt、假設 UI healthy，因 `first_note_dispatch_lead_below_200ms` 拒絕，沒有 trigger。成功與拒絕都保留，不宣稱端到端時間資格。

Live005 anchor 使用 frame2095／2100，最新 song interval 為 **11.549411273–11.573038307 s**；−80 ms 仍是顯式 combined phase，沒有量得獨立 input lead。結算 [PNG 原件](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/result-observed-001.png)與[後續觀察 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/result-observed-001.json)保存 **score 90,753（顯示 `0090753`）、Max Combo 53、Accuracy 09.08%、53 Perfect／0 Good／0 Bad／531 Miss、Early 0／Late 0**。PNG SHA256 為 `2ee71cfc61538262eb66e566d86bbcad119439d7724b34d7676e1327e4c26014`。這是程序停止後另行擷取並目視核對的結果；原 manifest 的 `game_judgment=unreviewed`、`manual_result_confirmed=false` 保持原樣，不偽裝成 CLI 自動確認結算。這份結算沒有逐 note 判定，因此不把每筆 RPC 和 Perfect 一一配對。

[Saved-artifact audit](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/analysis/live005-opening-audit.md) 將 53 筆 DOWN 核對至固定 origin＋phase＋原生事件；send-after-due median **4.469233 ms**、p95 **11.682661 ms**、max **12.384647 ms**。首筆 RPC 為 **18.0224 ms**，其餘多數較短；ACK 並非遊戲判定時間。下一輪保留 **−80 ms phase**，不單憑 RPC latency 重調相位。Audit JSON 較早的 visual scope 與其後更新的 Markdown 都原樣保存，後續判定由 result sidecar 補充。

Live005 的 revision 邊界修正已有[舊版失敗回歸](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/analysis/live005-runtime-regression-before.log)與[新版 20 組／12,087 checks](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/analysis/live005-runtime-regression-after-002.log)，scratch **1/1 CTest** 通過：owner 被搶占而跨過真 UP deadline 時保留既有 immutable tail，仍不延長 20 ms lateness。[Build004](../../local-assets/phigros-4.0.1/m7-whole-song-build-004/manifest.json) 隨後完成 **23/23 CTest、default 4/4**；binary SHA256 為 `64fabcc14b59a4e4a434f6ce03af7a8601243db9d0bdf84592a61cb98a8e123e`，與 live006 一致。Live006 越過舊停止位置，但 `overdue_tail_revisions_deferred=0`，不能宣稱這輪實測觸發了 fallback；live005 的失敗狀態保持原樣。

Live006 的[結算 PNG](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/result-observed-001.png)與[獨立觀察 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/result-observed-001.json)確認 **score 256,849（顯示 `0256849`）、Max Combo 150、Accuracy 25.68%、150 Perfect／0 Good／0 Bad／434 Miss、Early 0／Late 0**。PNG SHA256 為 `7ef63619f6b45e60ac45c63715ffc46f6e6c96184a17721ed25a783248023f92`，sidecar 另有 [SHA256 檔](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/result-observed-001.json.sha256)。原 manifest 的 `unreviewed`／`manual_result_confirmed=false` 未改；這是後續目視的總計，沒有逐 note 的 RPC／判定配對。

這次停止由 UI 分類空窗造成：[events 原件](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/events.jsonl)的序號 **4755–4782** 連續 28 筆均 `source_valid=true`，卻分類為 unknown；含前後邊界的最大 receipt 間距 **31.3076 ms**，所以這段並非來源停止送幀。序號 4783 再辨認到 HUD 時，距最後實際 playing receipt 已 **506.6265 ms**，超過 500 ms watchdog，runtime 正確拒絕以新觀測補回過期區間。接續改善 HUD 可辨識範圍，保留實際 receipt、立即 pause／result 撤銷與原時間門檻；[HUD Build005](../../local-assets/phigros-4.0.1/m7-whole-song-build-005/manifest.json) 已通過 **24/24 CTest、default 4/4** 與 **8 組／81 HUD checks**。其 [V3 樣本核對](../../local-assets/phigros-4.0.1/m7-hud-glyph-reference-001/FEATURE_API_V3.md)只涵蓋 11 個選定保存／合成樣本；4755–4782 的 unknown 原圖未保存，不能證明可消除當時空窗。

Live007 的[結果 PNG](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/result-observed-001.png)與[獨立 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/result-observed-001.json)確認 **score 335,616（顯示 `0335616`）、Max Combo 196、Accuracy 33.56%、196 Perfect／0 Good／0 Bad／388 Miss、Early 0／Late 0**。原 manifest 的 `unreviewed`／`manual_result_confirmed=false` 保留；總計不提供逐 note 判定配對。HUD fallback 已實際使用，500 ms watchdog 未放寬；新停止原因是 `compiled_trajectory_coverage_expired`，約 47.25 s 為固定 origin／rate 1 的預測位置。

Live007 的 intent 196 有 **40 steps／36 ms**。Owner 更新舊 32-step 視窗時已完成 6 steps；後續同步 RPC 把時鐘推近 first-omitted step，最後成功回執距該 coverage 僅 **3.157 µs**，但真正待送的下一個 MOVE 當時約晚 **13.7 ms**，尚未超過 20 ms。完整 tail 修正不改時間，只移除這個人為缺口；[舊版失敗回歸](../../../work/live007-runtime-regression-before.log)及[新版 12,582 checks](../../../work/live007-runtime-regression-after-002.log)保留，最小 fixture 使用實際 RPC 區間與 owner gap，未觀測尾段則明示採 0.6656 ms 模型，不冒充裝置回執。

Owner 等待同步改為非阻塞讀取新幀與高精度 waitable timer，醒後照常執行 freshness／stop／lateness guard。[純主機測量](../../local-assets/phigros-4.0.1/m7-host-wait-001/REPORT.md)中，未被通知的 `condition_variable::wait_for(1ms)` 中位數 **15.5933 ms**，高精度 1 ms timer 為 **1.5305 ms**；[實際 header 的 100 次 smoke](../../local-assets/phigros-4.0.1/m7-host-wait-001/header-smoke-result.json)無 QPC 回退或 handle 增加。這些數據不含 gRPC／遊戲，不能當端到端延遲保證。[Build006 archive](../../local-assets/phigros-4.0.1/m7-whole-song-build-006/manifest.json)保存整合版本與 **24/24、default 4/4 CTest**，binary SHA256 為 `08ebf4372f008576dc59bcadfaf6636ec8c15b68129523439332bcbc37ae5e22`；與 live008 manifest 的 binary SHA 相符。

私有全譜的[非零耗時 FakeTouch 封存報告](../../local-assets/phigros-4.0.1/m7-whole-song-latency-replay-001/README.md)重複使用 live007 的 1,905 個 RPC durations，完整完成 **584 contacts／4,911 events**；next-wake（最長 1 ms）與固定 1 ms owner 模型的最大 send lateness 分別為 **18.407515／18.520386 ms**。同一 duration 循環的舊 32-step／1 ms owner baseline 在 chart schedule **52.8164001 s** 因 coverage 停止。固定 0.5 ms 亦完成；新版固定 1 ms 在 **110.425 s**、1.2 ms 在 **56.945086 s／47.3508 s** 按真實 20 ms guard 停止。所有已送 source／deadline／pixel 與各 contact 前綴均核對；[manifest](../../local-assets/phigros-4.0.1/m7-whole-song-latency-replay-001/manifest.json)保留新舊報告與 traces。額外三個 owner-stall 場景因 phase 放置錯位落在事件空隙，明列不能驗證密集區壓力。這是合成 clock／UI、排除其他主機開銷的模型結果，不代表實機後段可完成。

## Live008 全曲完成與 3 Miss 待定位

[結果 PNG](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/result.png)與[獨立 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/result-observed-001.json)確認 **581 Perfect／0 Good／0 Bad／3 Miss、score 963,014（顯示 `0963014`）、Max Combo 395、Accuracy 99.49%、Early 0／Late 0**。本輪 CLI 的 `whole_song_sequence_complete=true`、`manual_result_confirmed=true`、`failure=null`；原 manifest 的 `game_judgment=unreviewed` 保持不變，sidecar 記錄後續獨立目視。

[逐筆時序稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/analysis/live008-timing-audit.md)重新核對 **584 contacts／4,911 events** 的完整步數、來源、phase、pixel 與固定 origin＋phase deadline，全部吻合且 RPC success；HUD fallback 使用 **359** 個實際來源畫面，500 ms watchdog 不變。最後接點為空、release known、pinned kernel route zero。輸入完整與遊戲 **581 Perfect／3 Miss** 分開成立；現有資料沒有逐 note 判定，不能直接定位 Miss。

DOWN send-after-due median／p95／max 為 **0.473719／1.590705／3.603505 ms**；全 phase 最大 **9.436977 ms** 在約 song 63.787 s 的雙 Flick MOVE。首 DOWN RPC **15.6878 ms**，其後最大 **3.9554 ms**；最密 **110.384–110.420 s** 的 **60 events** 全部完成，最大 send lateness **6.897876 ms**。按歌曲段落分組未見持續向曲末惡化的主機 DOWN lateness，沒有漏步或超過 20 ms；這不證明遊戲時鐘無漂移，也不能把 3 Miss 歸因於 RPC、投影或 phase。下一輪補 score／combo 變化證據，與幾何候選逐項核對；−80 ms仍是顯式實驗phase，live008當次未達成AP。

[補充 source-action 稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/analysis/live008-source-action-audit-001.md)保存六個投影 Hold 的來源與路徑、251 個 Tap 的實際 host DOWN→UP 窗口及 6,082 筆 active-period UI cadence。Tap start→start 中位數 **15.9596 ms**，139 筆低於排定 16 ms；這不是 Unity 實際接點持續時間。Max Combo 395 只能提出帶前提的 ordinal 396 候選，尚未指認 Miss；phisap stable 的 pointer recycle 也不是固定 32 ms dwell 的依據。

## Live009：32 ms Tap／Drag、579 Perfect／5 Miss

[結果 PNG](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/result.png)與[獨立 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/result-observed-001.json)確認 **579 Perfect／0 Good／0 Bad／5 Miss、score 936,815（`0936815`）、Max Combo 260、Accuracy 99.14%、Early/Late 0**。原 manifest 的 `game_judgment=unreviewed` 保留，`whole_song_sequence_complete` 與 `manual_result_confirmed` 均 true，failure null、release known、最後 kernel zero；binary SHA `14310fe6a016ffddbb05597106c5aa68fb81c92b0a0964f8691e7077828c0a48` 與 Build007 一致。

[逐筆稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/analysis/live009-timing-audit.md)核對全部 584／4911 的來源、phase、pixel、deadline、完整步數。和 live008 比對：**104 Flick／78 Hold 的全部 steps 相同**，僅 402 個 Tap／Drag 的 UP 延後 16 ms，全部 DOWN 不變。DOWN send lateness median／p95／max **0.4898635／1.655571／2.848685 ms**，全 phase 最大 **7.771330 ms**，最密 60 events 完整送出。沒有漏步或超過 20 ms；這不是遊戲判定時間或 input lead。

HUD timeline 已保存 **1,904 samples／35,642,880 bytes**、capacity drops 0，來源 receipt／raw timestamp 與灰階 pixels 各可追溯；其分析獨立於輸入許可。使用者觀察這輪 Miss 都是 Flick，先作為使用者回報保存，接續以 score／combo 變化定位 source notes。兩輪 anchor、運行狀況與診斷也不同，不能從 3→5 Miss 推論 32 ms 的因果，亦不能宣稱 Flick 已調整或修復。

## Live010：Flick 末端停留 24 ms、580 Perfect／4 Miss

[結果 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/result-observed-001.json)獨立確認 **580 Perfect／0 Good／0 Bad／4 Miss、score 929,110（`0929110`）、Max Combo 206、Accuracy 99.32%、Early/Late 0**。完整 584／4911 送完，failure null、manual-mode 確認結算、release known、最後 kernel zero；HUD timeline 保存 **1,918 samples**、capacity drops 0。

[輕量時序稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/analysis/live010-timing-audit.md)確認相較 live009 只有 **104 Flick UP 延後 20 ms**，其餘 **4,807 events** 完全不變。原 lead 16 ms／travel 32 ms／位移 48 reference pixels、Tap／Drag 32 ms、phase −80 ms 保持。DOWN lateness median／p95／max **0.479325／1.526718／2.804889 ms**，全部 events 最大 **3.544504 ms**；沒有漏步或超過 20 ms。使用者仍觀察 Miss 都是 Flick，接續比對兩輪 timeline；不能由 5→4 Miss 推論末端停留的因果或宣稱已修復。

## Live011：AP已確認，原始manifest保持不變

[結果PNG](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result.png)與[獨立sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result-observed-001.json)確認 **1,000,000分、584 Perfect／零Good／Bad／Miss、MaxCombo584、100.00%、Early/Late0**。PNG SHA256為 `17b1123df99e8e92b33be1236b6eff660e14b4d483dab9d6f3fb6c9a77a31748`；sidecar的 `AP_verified=true` 限於這份已觀察結果，原manifest的 `AP_verified=false`／`game_judgment=unreviewed` 未改。

[完整source／timing audit](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/analysis/live011-timing-audit.md)逐項核對 **584 contacts／5,669events** 的來源、完整步數、phase、pixel及deadline，全吻合；RPC全部成功，host接點生命週期peak4／final0，Android最後kernel slots另有zero證據。Binary SHA256為 `2d080e1234c4ec105087f092b4785409d6befd2e999aa9ff1ad40d63a6435ad7`，與Build009相符；HUD timeline保存 **1,893samples**、capacity drops0。

DOWN send lateness median／p95／max **0.471353／1.660581／3.896696 ms**，全events最大 **5.955067 ms**。首DOWN RPC為20.8513 ms、ACK晚21.989167 ms；20 ms guard限制的是發送遲到，不是RPC返回耗時，首筆發送晚1.137867 ms。AP由遊戲結果獨立確認，不由回執推導；單次成功尚不證明跨輪重現率、各Flick參數因果、其他曲目或獨立input lead。

## 歷史高密度輸入分析

對 [live003 原生 compiled plan](../../local-assets/phigros-4.0.1/m7-whole-song-live-003/compiled-plan.json) 的唯讀盤點顯示：4,911 events 中有 3,743 MOVE；第一個雙 Tap 在 **22.971 s**，第一個 Flick 在 **23.298–23.334 s** 有 **24 steps**，最短間距約 **0.206 ms**；最密段 **110.384–110.420 s** 有 **60 events／36 ms**。舊單 Tap 的首 DOWN RPC 約 12–20 ms，三 Tap 暖機後的 RPC 約 0.6–1.4 ms，不能認定每筆都需 10–20 ms。

以「精確 wakeup、零 owner 開銷、每次 RPC 固定耗時」作理想串列排隊模型：逐筆固定 3–12 ms 會在第一個 Flick 超過 20 ms lateness；即使固定 1 ms，後段密集雙 Flick 也會超限。同 deadline／同 phase／不同 pointer 的真實 batch 可把 4,911 events 合成 4,588 RPC；同一理想模型在 1 ms 時最壞 lateness 約 **13.515 ms**，1.2 ms 時約 **19.515 ms**，1.5 ms 則仍超限。**這是可行性模型，不是實測吞吐、時序保證或已完成的 batch 實作。**

現有 scheduler 同步等待每筆 `inject()` 回執，不能只在 app backend 暫存後先回成功來拼 batch；那會製造尚未執行的 ACK。最小合理後續設計是實驗 scheduler 的明確批次派送：同 deadline、同 phase、不同 pointer 最多四筆呼叫既有 `GrpcTouch::inject_batch()`，真 RPC 返回後才提交進度，各筆保留共同的真實 start/end 與 effect-unverified 語義；UP 批先執行，避免同 pointer 的 UP 與新 DOWN 混入同批。此方法不能合併同接點不同時刻的 Flick MOVE，也不藉擴大 lateness 或丟棄譜面步驟宣稱完成。

## 開場結果與下一輪驗收

先前 [首 Tap live007](../../local-assets/phigros-4.0.1/m7-first-tap-live-007/manifest.json) 在 −80 ms 的[結算原件](../../local-assets/phigros-4.0.1/m7-first-tap-result-007/result-1.png)為 **1 Perfect／0 Good／0 Bad／583 Miss**。同設定 live005 的 Perfect 與 live006 的 Good Late 都保留，不能挑選成功輪次宣稱穩定相位。[三 Tap live001](../../local-assets/phigros-4.0.1/m7-three-tap-live-001/manifest.json)有三組成功 DOWN／UP 回執、最後 pinned kernel route zero，但缺最終結算原件；**未驗證 3 Perfect**。詳見[開場試驗歷史](M7_FIRST_TAP_CALIBRATION.md)。

Live011已滿足指定曲目 **584 Perfect／零Good／Bad／Miss** 的實際AP結果條件，成功設定與原生chart／compiled plan／binary／profile SHA、原始UI與觸控紀錄、釋放證據及結果圖已封存。歷史失敗不改寫；後續若驗重現率，使用新的有界編號run。來源像素age、獨立input lead、精確歌曲時鐘與生產qualification仍未取得，音訊繼續暫緩。

## 明確啟動入口

Build003 保留以下啟動形式，首音觸發與 500 ms watchdog 由該 build／manifest 明示；不要將不同 build 的相同旗標視為相同實驗。沿用 `RHYTHM_BUILD_CALIBRATION_TRIAL=ON` 及其 `CHART_IMPORT`、`VISUAL_CANDIDATES`、`SUPERVISOR`、`PAS_GRPC` 依賴建置 `chart_experiment`；default build 不啟動裝置。先由操作者確認 Dlyrotz IN13 選曲或結算畫面，等 CLI 顯示 READY 後人工開始／重試一次。

```text
chart_experiment run emulator-5554 phigros CHART.json STATE.json KERNEL.json NEW_DIR -80 --experimental-whole-song
```

Build005 可在上述命令末尾追加 `--experimental-hud-reference CLEAN.rgb`。指定已核對的 [clean reference](../../local-assets/phigros-4.0.1/m7-hud-glyph-reference-001/clean-reference.rgb)，只在已 armed、有效來源、unknown 且 `no_calibrated_state_match` 時使用三個 HUD 字形區域；同幀 paused／result score match 優先，actual receipt 的 500 ms 界線不變。此 binary SHA256 為 `00d75a26930b146573cd92a4497805d7e3ecc74d88c475517c0cfd16abcfd895`；實際 live 另存新目錄。

`NEW_DIR` 必須不存在。`-80` 是本輪顯式相位假設，不是量得的 input lead；`KERNEL.json` 必須符合這次 emulator boot 與 route。入口不自行選曲或按 Play，不在失敗後無限重試。

本機這次使用以下啟動參數，避開已失敗的舊 snapshot 載入，並啟用既有 backend 所需的本機 gRPC token。這不清除 userdata。已有 `phigros` instance 時不要重複執行；每次重開後需重新盤點 boot ID、helper 與 kernel route，不能沿用舊 profile。

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\emulator\emulator.exe" -avd phigros -port 5554 -grpc 8554 -grpc-use-token -no-snapshot-load -no-snapshot-save
```
