# 跨曲目接續：從原生首音建立各曲目的開場候選

## 最新實測結果索引

最新build006已由使用者實跑：18個回合、14輪完整輸入、8輪原生確認結算；正式Legacy有8首保存結算AP，另有Verrückt暖身AP。6個完成輸入未確認結算的回合及剩餘未測歌曲已列入[完整封存紀錄](SESSION_RECORDS_2026-10-24.md)。使用者要求先停止修正並commit／push；本次整理不新增實測。

以下保留各階段建立時的條件、NOT RUN與限制，不能當作目前仍未實跑；當前狀態以結果索引與MODEL_HANDOFF為準。

最新：Chapter Legacy 20首IN已建立共通匯入／首音候選與完整profile，通過封存binary全20離線check；資料／清單／壓力測試未通過項目見 [全測準備](CHAPTER_LEGACY_TEST_PLAN.md)。本頁下列Glaciaxion實機結果仍屬原build；共通首音不是依遊戲判定自動調整Flick／Drag參數的回饋器。

日期：2026-10-24。分支：`feature/chart-driven-multigame`。

Glaciaxion IN12 的 **live005 已用 Tap／Drag32、五指、inset64 完整送出 729 contacts／6,350 events**，manual-mode 已確認結算、failure null、最後 kernel zero。[獨立結算複核](../../local-assets/phigros-4.0.1/glaciaxion-result-review-005-001/result-observed-001.json)為 **932,353 分、722 Perfect／1 Good／3 Bad／3 Miss**，不是 AP。先前 live003 的 725 Perfect／0 Good／1 Bad／3 Miss、結算辨識失敗與其他負例保留，不由兩輪總分差推論參數因果。

Dlyrotz IN13 的 live011 AP 與 Build009 封存保持原身份，不延伸為 Glaciaxion 成績或一般曲庫支援。

使用者接續要求減少每曲人工特調，研究依原生幾何／可用接點與實測回饋選擇候選；其後明確優先完成 Chapter Legacy 全部資料入檔，再準備 **IN 每首一輪**。目前不新增歌曲實跑，自適應調參的分析保留，沒有將尚未實作的回饋策略標為完成。

## 每首曲目的身份與開場

`prepare-adaptive` 從已授權本機資產的 source manifest 核對譜面／解碼 WAV SHA，保留精確 song、difficulty、版本與原生 note JSON pointer。它重新編譯整份譜面，輸出 `compiled-plan.json`、`report.json`；完整計畫符合有界入口時，才輸出 `song-profile.json`。

Profile 固定 chart／audio identity、全曲 contacts／steps／peak、compiled fingerprint、接點參數及 opening-model SHA。它不保存可續用的同步租約；每次 `run-adaptive` 都重算並比對計畫與開場模型。換譜、換參數或模型改變時重新準備，不直接修改舊 profile 的 fingerprint。

開場由**全譜最早的原生音符**推導；目前接受唯一的 above Tap／Drag、水平判定線與有界仿射移動／原生 speed piece。Tap 用 cyan、Drag 用 yellow 的有界 run detector。旋轉／below 或其他尚不支援的開頭明確拒絕，不能跳過早期音符改抓後面的 Tap。

Native note position、line move 及 speed floor 共同建立 x／y 時間候選，保留多分支與一對一配對歧義；不以接近預期秒數挑唯一答案。實際兩幀必須有不同送達／證據與可區分的向下移動，首音配對、時間一致性、freshness 與至少 200 ms 派送提前量均通過後才單次觸發。這仍依賴明示的 rendering／可見性假設。

歌曲時間維持 `actual receipt − candidate midpoint`、rate 1、顯式 phase 的實驗假設；phase −80 ms 沿用為起始候選，尚未分離 input lead。新 UI 觀測只續認播放許可，不刷新既有首音時間證據。

## Glaciaxion IN 的離線候選

來源為 Phigros 4.0.1／157，`Glaciaxion.SunsetRay.0`／IN；[原生資產與來源鏈](../../local-assets/phigros-4.0.1/glaciaxion-in-verified-001/source-manifest.json)、[重抽比較](../../local-assets/phigros-4.0.1/glaciaxion-in-reextract-002/reextract-comparison.json)另存本機，沒有提交 chart／audio。20 lines／729 notes：216 Tap、389 Drag、73 Hold、51 Flick；1,458 個 start／end ns 與獨立 Fraction 計算完全相同。

首音為 `/judgeLineList/0/notesAbove/0` 的 Drag，1,714,285,714 ns；判定時中心 `(640,360)`。開場需處理 native x 抖動；第一個 Tap 在 3.429 s，不能用它作完整歌曲的觸發點。

| 項目 | 本曲候選 |
| --- | --- |
| Tap／Drag dwell | 32 ms／32 ms，依使用者要求恢復 |
| Flick | lead 30 ms、travel 60 ms、60 reference pixels、末端停留 24 ms |
| 容量／幾何 | 顯式 5 點，1280×720 compiler surface；720×1280／rotation90 觸控路徑另有本 boot fixture 證據 |
| 全曲計畫 | 729 contacts／6,350 events／peak 5，未刪除 note |
| Input inset | 64 reference pixels；目標矩形 x64–1215、y64–655 |
| Normal projection | 顯式 D7000 ceiling；19 contacts，3,358 certificates，實際最大 6,544 reference pixels |
| Fingerprint | `05ab01a0662eb16cb11215c6a66c14645dc1d95bce3143f6020459d81a666188` |

[最新逐來源／座標核對](../../local-assets/phigros-4.0.1/glaciaxion-inset64-cap5-drag32-replay-001/independent-plan-verification.json)確認全部 729 個原生 pointer 恰好一次、6,350 steps 均在 inset 內（實際 x208–1061、y64–643）。相較 live003，全部 DOWN 時刻不變；340 個非 Drag contacts（含全部 51 Flick）的完整 steps 相同，389 個 Drag 僅 UP 延後 16 ms。Flick 的 lead／travel／位移／末端停留沒有改動，亦未由這次參數恢復證明漏判原因。

D7000 限定的是來源幾何的法向修正距離；inset 也只產生顯式幾何候選，沒有擴大輸入畫面或宣告全距離判定已驗收。舊 D250、D7000／Drag16／inset0 與 inset64／capacity4 的候選、fingerprints 及失敗均保留。

仍有 440 個 fresh-DOWN、74 個 nearby Tap／Hold interference、19 個 unverified-projection warnings。這些候選保留來源與 certificate，沒有以成功編譯消除其實際判定不確定性。

## 保留的拒絕與回放負例

- [原 D250／Drag32](../../local-assets/phigros-4.0.1/glaciaxion-compile-replay-001/report.json)因 19 個 `projection_unavailable` 拒絕；[幾何診斷](../../local-assets/phigros-4.0.1/glaciaxion-compile-geometry-002/report.json)確認部分線移至 y −720／−6480，並非刪 note 可以解決的情況。
- [D7000／Drag32](../../local-assets/phigros-4.0.1/glaciaxion-compile-D7000-003/report.json)解除幾何拒絕後仍需 peak 5。63.398571429 s 的兩個 Drag 尾端與兩個 Flick DOWN、另一條 Hold 重疊；[獨立 sweep](../../local-assets/phigros-4.0.1/glaciaxion-compile-geometry-002/independent-contact-peaks.json)顯示 Drag16 降回 peak 4，沒有假定裝置已支援第 5 點。
- [完整 FakeTouch 回放封存](../../local-assets/phigros-4.0.1/glaciaxion-D7000-drag16-replay-006/archive-manifest.json)以 live009／010／011 的 RPC durations 循環，理想獨立 20 ms UI receipts、next-wake owner 三組均完成全部 6,350 events；最大 send lateness 分別 14.934190／13.652590／19.568008 ms。
- 同封存的 live011 pattern＋固定 1 ms owner 在 5,670 receipts 後按 `compiled_step_lateness_limit_exceeded` 停止；負例與最後 FakeTouch 釋放保留。上述模型未包含實際分類／主機額外成本，19.568008 ms 亦不提供 live 時序餘裕保證。
- [Inset64／capacity4／Drag16 回放](../../local-assets/phigros-4.0.1/glaciaxion-inset64-replay-001/archive-manifest.json)保留其 `4cf00b…` 計畫；[最新 capacity5／Drag32 回放](../../local-assets/phigros-4.0.1/glaciaxion-inset64-cap5-drag32-replay-001/archive-manifest.json)以 Dlyrotz009／010／011、Glaciaxion002／003 的五組 RPC durations 循環，next-wake 全數完成 6,350 events，實際 FakeTouch pointer lifecycle peak5、使用 IDs0–4，最後全釋放。
- 最新固定 1 ms owner 的舊 live011 pattern 仍在 5,670 receipts 後因 lateness 停止；原負例未被容量擴充掩蓋。其他四組固定 1 ms 回放完成。這些是理想 UI／owner 成本模型，不是實機吞吐、裝置 5 點能力或遊戲判定證明。

## 5 點容量必須有本 boot 的原生證據

Runtime 的 `contact_capacity` **預設 4、僅允許 1–5**；compiled metadata、selected memory 與 scheduler 都使用明確上限。原預設拒絕 peak5，容量 6 也拒絕；新離線測例另驗五個不同 pointer 的 DOWN／UP，以及 STOP、UI expiry、epoch 改變時全部釋放。FakeTouch 不能取代真實 backend 驗證。

[本機 5 點證據](../../local-assets/phigros-4.0.1/glaciaxion-five-evidence-004/capacity-evidence.json)綁定 boot `756c4d2b-04ba-433d-805b-38a14370b12c`、已核對的 fixture APK、實際 720×1280／rotation90 路徑；[native analysis](../../local-assets/phigros-4.0.1/glaciaxion-five-evidence-004/analysis.json)確認 peak5、IDs0–4、完整 DOWN／MOVE／UP 與 final zero。本次 proof SHA256 為 `1d5aea8cf1809344101d0f1f1b2c383b654dd3d5c59b8be004e9fd1e6869a2cd`。這只證明本次路徑所測的 5 點，不是裝置最大容量或 input latency 資格；前面的幾何失敗嘗試另存原狀。

使用 5 點 profile 前，CLI 必須核對 capacity proof 及其引用原件 SHA、native stream／pointer lifecycle／final-zero analysis、host manifest、retrieval association、boot 與 720×1280／rotation90 幾何；缺證據或不符即拒絕。Live 另以 kernel helper 核對目前 boot／evdev route／零接點，不能只修改 proof 的 boot 字串。Emulator 重啟或輸入路徑改變後重新取得對應證據。

## 本機準備與明確啟動

以下從 repo 根目錄執行；[Build003 封存](../../local-assets/phigros-4.0.1/glaciaxion-adaptive-build-003/manifest.json)的 binary SHA256 為 `21a0a475789e89eaa056882ddf3df938bb6e0319afbf28972fddc154cc20ae6a`，通用 launcher 逐檔核對 binary／DLL hashes。[最新 source profile](../../local-assets/phigros-4.0.1/glaciaxion-prepared-004/song-profile.json)為 capacity5／Drag32／inset64；[state calibration002](../../local-assets/phigros-4.0.1/glaciaxion-state-calibration-002/dataset.json)更新本曲結算參考，playing HUD 沿用 calibration001。`NEW` 目錄須不存在，舊輪次不覆寫。

```powershell
$assets = '../local-assets/phigros-4.0.1'
$settings = @{
    BuildArchive = "$assets/glaciaxion-adaptive-build-003"
    Chart = "$assets/glaciaxion-in-verified-001/Chart_IN.json"
    SongProfile = "$assets/glaciaxion-prepared-004/song-profile.json"
    StateCalibration = "$assets/glaciaxion-state-calibration-002/dataset.json"
    HudReference = "$assets/glaciaxion-state-calibration-001/playing.rgb"
    KernelProfile = "$assets/glaciaxion-boot-validation-001/kernel-profile.json"
    CapacityEvidence = "$assets/glaciaxion-five-evidence-004/capacity-evidence.json"
    OutputDirectory = "$assets/glaciaxion-live-NEW"
    PhaseMs = -80
}
& .\tools\run-adaptive-song.ps1 @settings -CheckOnly
```

`prepare-adaptive` 與 `check-adaptive`／launcher `-CheckOnly` 均為離線入口；後者核對檔案、fresh compile、opening-model、HUD／state 及提供的 capacity evidence，不連裝置、不建立 run 目錄，也不證明 profile 符合當前裝置 boot。5 點 profile **必須提供 `-CapacityEvidence`**。

若需從同一資產重建新 profile，使用同一封存 binary 與明確參數；本步不連裝置：

```powershell
$exe = Join-Path $settings.BuildArchive 'bin/chart_experiment.exe'
& $exe prepare-adaptive $settings.Chart "$assets/glaciaxion-in-verified-001/source-manifest.json" "$assets/glaciaxion-prepared-NEW" --normal-projection-pixels 7000 --drag-duration-ms 32 --input-inset-pixels 64 --contact-capacity 5
```

Emulator 重啟後重新核對 AVD／版本、boot、evdev route、helper SHA 與 kernel 零接點；使用新 profile，不能只換 boot 字串。操作者停在 Glaciaxion IN 選曲／結算頁，再明確執行一輪：

```powershell
& .\tools\run-adaptive-song.ps1 @settings
```

等 `READY` 後由使用者按 Play／Retry。入口不選曲、不按開始、不自動重試；Esc／Ctrl+C／run 目錄中的 `STOP` 提供停止介面，Ctrl+C 裝置路徑仍未取得實測資格。每輪另存原始 manifest、plan、UI、RPC、釋放與結算圖；遊戲判定須由獨立 sidecar 核對，不改寫原 `unreviewed`。

## HUD、時間與來源界線

各曲目使用自己的 playing RGB／state calibration；adaptive HUD reference 必須等於 playing template 的 RGB hash。標題 ROI 可依曲名配置，pause／result 的同幀 denied-state 優先，unknown 不自行刷新許可。現有樣本核對不等於全曲背景、遮擋、顏色與所有 UI 狀態皆已取得資格。

100 ms anchor age、500 ms 實際 UI watchdog、20 ms send-lateness、180 s round、240 s session、epoch／geometry invalidation、完整來源／fingerprint、release-all、使用者停止與有限記憶體保持。容量預設4，顯式可選1–5；投影預設 D250、inset0，anchor domain 預設 Dlyrotz 的 10.8–11.95 s，其他歌曲須明確提供其上限與 native domain。

Capture source-pixel age、獨立 input lead／jitter、rate／歌曲時鐘、每項接點參數因果及跨輪重現率均未取得資格；音訊依使用者要求暫緩。Live003 是一輪實際結果，不能將 D7000、inset64 或新 5 點設定一併視為全曲判定資格。離線回放與輸入 RPC 完成均不代表遊戲 AP。

此次改動包含新 contact compiler 的明確 inset 選項、`apps/chart_experiment/` 實驗層、launcher 與相應測例，不更改 production SessionRunner qualification gate。Inset0 保留舊 fingerprints；Dlyrotz 584／5669 的成功計畫已離線核對完全相同。`EXTRACTION.json` 所列原 PAS sources 保持 byte-for-byte，沒有以本輪結果改寫來源或歷史資格；main 分支未動。Private chart、音源、校正、boot profile 與原始證據繼續保存在 repo 外。

## 初版實測與系統手勢邊緣

[Build001](../../local-assets/phigros-4.0.1/glaciaxion-adaptive-build-001/manifest.json) 為新的跨曲目 binary，SHA256 `f9967d9e76e041bc0f5d4af7f7ba7d83c1740512c0167e94c1f080807fe2a3ec`；26/26 整合 CTest、4/4 default CTest、85/85 Python unittest 通過。新的 `check` 模式另核對 Dlyrotz 584/5669 的原 fingerprint 不變；這不是重跑原 AP。

- [Live001](../../local-assets/phigros-4.0.1/glaciaxion-whole-song-live-001/manifest.json)：選曲待機中收到 `user_stop`，0 DOWN/UP，release known、final kernel zero；沒有遊戲判定。
- [Live002](../../local-assets/phigros-4.0.1/glaciaxion-whole-song-live-002/manifest.json)：原生首 Drag 的雙影格觸發成功，anchor interval 約 1.238–1.248 秒，完整 729/6350 計畫已 armed；94 DOWN/93 UP 後 `actual_UI_receipt_expired`，剩餘一指 release 成功，最後 11 個 kernel slots 全為 −1。沒有整曲結算或 AP。

[獨立停止稽核](../../local-assets/phigros-4.0.1/glaciaxion-hud-watchdog-audit-002/report.json)將 input/source/畫面逐項對照：兩個 Hold（line1 notesAbove13/14）在 32.571 秒從 `(451,0)`／`(829,0)` DOWN，32.786 秒向內滑至 y576。保存畫面出現 Android status/navigation bars，HUD 字形同時變暗；使用者也觀察到觸控滑入 bar。最後有效 playing 後 500.6777 ms 按原 watchdog 停止。HUD 原件的拒絕可離線重現，沒有放寬亮度／freshness 或將系統列視為 playing。

## Live003 完整輸入與獨立結果

[Live003 原始 manifest](../../local-assets/phigros-4.0.1/glaciaxion-whole-song-live-003/manifest.json)保持 `whole_song_sequence_complete=true`、729 DOWN／729 UP、6,350 receipts、release known、final kernel zero；Inset64 計畫完成全曲，沒有重現 live002 的系統列停止。這是一輪實測，尚未證明所有系統手勢情況或跨輪穩定性。

[獨立結果 sidecar](../../local-assets/phigros-4.0.1/glaciaxion-result-review-003-001/result-observed-001.json)目視確認 **725 Perfect／0 Good／1 Bad／3 Miss、951,989 分、Max Combo415、99.45%**；使用者回報 Flick 漏判，逐 note 歸因尚未驗證。原始 `unreviewed`／`AP_verified=false` 不回寫。

[結算辨識稽核](../../local-assets/phigros-4.0.1/glaciaxion-result-review-003-001/ui-result-failure-analysis.json)確認借用的 Dlyrotz accuracy-label 區域受本曲背景影響，未通過既有 threshold8；RPC 已完成，manual lifecycle 卻未確認結算，直到 180.005 s 的 round watchdog 中止，原 failure 為 `manual_round_ended_before_whole_song_complete`。新 calibration002 換用本曲結果參考並保留原 threshold，不能把原 run 改為成功 lifecycle；新結果樣本只有 self-check，獨立 heldout result：NOT RUN。

最新跨曲目建置通過 **26/26 整合 CTest、4/4 portable CTest、85/85 Python 測例**；runtime 子測例另有 27 組／12,630 checks，包含 default4 拒絕5及第五接點的釋放。第 004 輪在首音錨點過期後、任何輸入前停止，0 DOWN/UP、最後 kernel zero；第 005 輪依使用者要求重新待機後成功觸發，729 DOWN／4,892 MOVE／729 UP 完成，peak5，manual result confirmed、failure null、release known、final kernel zero。結果為上述 722 Perfect／1 Good／3 Bad／3 Miss；使用者觀察漏判多為 Flick、另有部分 Drag，逐 note 類型仍待回饋定位。原 manifest 的 `AP_verified=false`／`unreviewed` 保持原樣。
