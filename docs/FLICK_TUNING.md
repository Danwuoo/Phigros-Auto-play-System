# Flick 幅度與時機調整

本指南說明如何在既有 chart-driven 流程中，為單一曲目調整 Flick 的提前量、滑動時間、距離與末端停留，並用原始回合資料核對實際送出的命令。參數已接到原生 `prepare-adaptive`、song profile、compiler 與 `run-adaptive`；調整後必須重新產生計畫與 profile。

目前沒有證據能把各曲的 Flick Miss 歸結為同一個根因，也沒有宣稱本次修改已修好 AP。既有實測與缺錄項目仍以 [完整回合紀錄](SESSION_RECORDS_2026-10-24.md) 和 [驗證紀錄](VALIDATION.md) 為準；新工具的離線測試不增加遊戲實測成績。

## 1. 四個可調參數

| Profile 欄位 | `prepare-adaptive` 選項 | 缺欄位時的預設 | 接受範圍 |
|---|---|---:|---|
| `flick_lead_ms` | `--flick-lead-ms` | 30 ms | 整數 1–100 ms |
| `flick_travel_ms` | `--flick-travel-ms` | 60 ms | 整數 2–200 ms |
| `flick_distance_reference_pixels` | `--flick-distance-pixels` | 60 reference pixels | 有限數值 1–240，可有小數 |
| `flick_release_ms` | `--flick-release-ms` | 24 ms | 整數 1–100 ms |

額外要求 `flick_lead_ms < flick_travel_ms`。時間欄位不接受浮點數、布林值或數字字串；距離不接受布林值、NaN 或無限值。這些範圍是有界實驗控制，不是已量測的 Phigros 判定窗口。

以譜面判定時間 `J` 表示時序：

```text
DOWN = J − lead
滑動末端 MOVE = J − lead + travel
UP = J − lead + travel + release
名義滑動速度 = distance / (travel_ms / 1000)
```

預設 30／60／60／24 代表從 `J − 30 ms` 開始滑動，在 `J + 30 ms` 到達末端，再停留 24 ms，於 `J + 54 ms` 放開；名義速度為 1,000 reference pixels/s。

`lead` 改變整段滑動相對於判定時間的位置；`travel` 與 `distance` 共同影響名義速度；`release` 只延長末端停留。compiler 仍按原生譜面計算移動／旋轉判定線，並保留既有 normal projection。判定線運動、投影與像素取整都可能使實際螢幕命令軌跡不同於名義距離，因此不能只由 `distance / travel` 判斷遊戲是否接受了 Flick。

## 2. 參數如何進入真正執行路徑

[共用參數實作](../apps/chart_experiment/flick_policy.hpp) 負責驗證與轉換；[原生入口](../apps/chart_experiment/main.cpp) 的處理順序如下：

1. `prepare-adaptive` 解析四個選項，檢查範圍及 `lead < travel`，將毫秒轉為 compiler 使用的奈秒。
2. `song_compile_options` 把四個值傳給 `compile_phigros_contacts`。成功且符合原有資源限制時，輸出新的 `compiled-plan.json`、`song-profile.json` 與 `report.json`。
3. 新 profile 保存四個欄位；舊 profile 缺欄位時使用既有 30／60／60／24。profile schema 仍為 `pas.adaptive-song-profile.v1`。
4. `check-adaptive` 和 `run-adaptive` 讀取 profile，再以相同參數重新 compile，核對 `expected_plan_sha256`、contacts／steps／peak 和第一個 DOWN；opening model 也重新核對。
5. `check-adaptive` 輸出實際採用的 `flick_parameters`；執行回合的 manifest 記錄 `experimental_flick_*`，compiled plan 記錄相應 compiler 值。

以下只是完整 profile 中的參數片段，不能單獨作為可執行的 song profile：

```json
{
  "flick_lead_ms": 30,
  "flick_travel_ms": 60,
  "flick_distance_reference_pixels": 60.0,
  "flick_release_ms": 24
}
```

### 身分與第一個 DOWN

四個參數都是 compiled fingerprint 的輸入。不能直接修改既有 profile 的參數，卻沿用舊 `expected_plan_sha256`；原生 preflight 會重新編譯並拒絕不一致的結果。`prepare-adaptive` 會建立新的 profile，而非把新軌跡掛在舊計畫身分下。

opening 使用實際編譯結果的第一個 DOWN。較晚的 Flick 可能因 lead 提前而先於最早原生 note 出手，例如首 Tap 在 2,000 ms、Flick 在 2,020 ms、lead 為 50 ms，則第一個 DOWN 在 1,970 ms。opening 仍觀測原生最早 note，但會將 1,970 ms 納入其目標時間及 model fingerprint。

離線契約已確認：缺欄位與明示相同預設產生相同 compiled fingerprint；原本相同的 first DOWN 不改變 opening fingerprint；每個參數改動都進入 compiler 並改變相應計畫身分，合成例中的非 Flick Tap 保持不變。

## 3. 產生單一參數候選

先用本次程式碼重新建置 `chart_experiment.exe`。舊 binary 不會自動取得新選項。以下路徑是本機範例，請替換為同一曲目的既有資產與完整 baseline profile：

```powershell
python .\tools\prepare_flick_trials.py `
  --baseline-profile C:\private\ready\song-profile.json `
  --chart C:\private\assets\Chart_IN.json `
  --source-manifest C:\private\assets\source-manifest.json `
  --executable C:\build\chart_experiment.exe `
  --output C:\private\flick-distance-001 `
  --vary flick_distance_reference_pixels `
  --value 48 --value 72
```

[候選產生工具](../tools/prepare_flick_trials.py) 會：

- 核對 baseline profile、chart、source manifest 的曲目／難度／hash 關係，記錄 executable hash。
- 保留 baseline 的 Drag duration、projection、inset、capacity、HUD、import policy 與其他 Flick 參數。
- 建立 baseline 和兩個只改距離的候選；重複數值或與 baseline 相同的數值不另建候選。一次最多提供 12 個候選值。
- 輸出 `trials.json` 和 `prepare-commands.ps1`。每個候選保存完整 native argv，初始 `preparation_status`、`gameplay_status` 都是 `not_run`，`winner` 為 `null`。

`--vary` 可指定表格中的四個 profile 欄位。時間候選必須填 JSON 整數，例如 `--vary flick_lead_ms --value 20 --value 40`。以上數值是比較方法的示例，沒有被判定為該曲的最佳參數。

產生命令不會執行 native 程式、不會尋找裝置，也不會觸控。確認 `trials.json` 後，在 Windows 本機執行：

```powershell
& 'C:\private\flick-distance-001\prepare-commands.ps1'
```

此腳本先核對 profile、source manifest、chart、executable 的已記錄 SHA-256，再依序呼叫原生 **離線 `prepare-adaptive`**；它不含 `run-adaptive`。每個候選使用不同的新輸出目錄。native preparation 失敗時腳本停止；若原生程式已寫出 `report.json`，可由其中檢查 diagnostics。不要覆寫失敗產物；修正後使用新的 trial 輸出目錄。

原生 preparation 的既有限制仍適用，包括已支援的 source revision、source manifest 旁的 `music.wav` 與 hash、compiler 容量／步數／時長限制。它目前仍要求 Phigros `4.0.1 / 157` 的本機來源身分；取得其他版本的譜面不代表可直接通過此原生實驗入口。IN 共用 HUD policy 的難度限制也沒有放寬，AT 不會自動繼承 IN 的 HUD 資格。

成功 preparation 後，逐一檢查 `report.json`、新 profile 及 compiled plan，再使用既有 [adaptive preflight 流程](ADAPTIVE_SONG_TESTS.md) 做 `check-adaptive`。`trials.json` 是產生當時的規劃檔，不會因手動執行腳本而自動更新為「已完成」；實際 preparation 結果以各候選目錄中的原生產物為準。

## 4. 單變量 B–A–B 本機比較

定義 **A 為原始 baseline，B 為某一個候選**。先保存 A 的既有完整回合，再針對同一個 B 採 B–A–B 順序比較。第二個 B 不再改參數；另一個候選另開比較組。

1. 固定同一 APK、chart SHA、歌曲難度、裝置／模擬器、解析度、phase、Tap／Drag、projection、inset、HUD 與 capacity。只變動選定的 Flick 欄位。
2. 每個回合使用新輸出目錄，保存 manifest、compiled plan、receipts、HUD timeline、結算圖及人工結果。實跑沿用既有 `run-adaptive` 操作、裝置證據與停止／釋放流程；候選工具不自動開始或重試遊戲。
3. 執行 B–A–B，核對兩次 B 是否出現一致方向的改變。可重複比較區塊並補 A，使 A／B 都有多次完整回合；一次 AP 或一次較低 Miss 不能單獨判定因果。
4. 把未跑完、late stop、缺結算或缺原件的回合一併列出。它們不是可以刪除的差成績，也不能補記為 AP／0 Miss。
5. 結算與人工窗口對照逐 Flick 診斷，再決定是否保留該曲的新 profile。證據不足時保留「待驗」，不由工具自動選優勝。

優先比較距離或 lead 的小幅變化；若需要再比較 travel 或 release，另開一組單變量實驗。若換用新 baseline，明確記錄其 profile／plan hash，使不同組的基準可以追溯。局部改善也不自動變成所有曲目的共用預設。

## 5. 唯讀逐 Flick 診斷

[診斷工具](../tools/analyze_flick_misses.py) 直接讀取原生 whole-song 回合目錄：

| 輸入 | 用途 |
|---|---|
| `manifest.json` | 曲目與計畫身分、phase、arrival anchor、runtime 狀態／收尾 |
| `compiled-plan.json` | 每個 note 的 ownership、judgment、編譯接點及命令軌跡 |
| `receipts.jsonl` | 每筆主機 RPC 的 intent／pointer／phase／座標／deadline／起訖時間／成功狀態；缺少時回報證據不足 |
| `hud-timeline.json`，可選 | 原生 receipt-time HUD 索引與取樣資訊 |
| 相鄰的 `hud-timeline.gray`，可選 | 核對已保存 HUD 像素的大小與 SHA-256 |

在已存在的分析資料夾中指定新的輸出檔：

```powershell
python .\tools\analyze_flick_misses.py `
  --run-dir C:\private\runs\song-live-001 `
  --output C:\private\analysis\song-live-001-flick.json
```

輸出必須位於原始 run 目錄之外，且不能已存在。不指定 `--output` 時將完整 JSON 印到 stdout。所有輸入唯讀，不修改原始 manifest 或補寫歷史結果。

### 每個 Flick 可看到什麼

- 原始 chart SHA＋JSON pointer、line／side／note index、compiler contact ID、runtime intent ID 與實體 pointer ID。intent 依原生 runtime 的「出生時間、contact ID」排序還原，不把它當成 note index 或 pointer ID。
- judgment、planned lead、逐 step 預期 host deadline，以及 receipt 的 phase／pixel／deadline 匹配、發送 lateness、RPC duration、success。
- 編譯與 host command 軌跡的距離、滑動時間、分段速度、方向反轉、末端停留、派送時間相對計畫的壓縮比例。
- 同時接點峰值、重疊／接近的 intent、最小接點距離、UP 前 pointer 被重用、中途 pointer 改變，以及成功 UP 後的重用間隔。
- normal projection 範圍、需要幾何覆核的候選、人工窗口與相鄰 HUD sample 的 index／byte offset。

報告會將缺／錯／重複 receipt、RPC failure、lateness、壓縮派送、pointer collision、短位移、projection、近距離重疊等列為**待查候選**。預設診斷門檻為 lateness 20 ms、接點距離 48 pixels；另標記端點位移低於名義距離四分之一、完整主機滑動時間低於計畫一半的情況。可用 `--lateness-ms`、`--nearby-pixels` 改變前兩項分析門檻，這不會修改觸控計畫。這些分類門檻不是遊戲判定窗口或建議觸控參數。

### 證據界線

主機 deadline 使用 `anchor.origin_host_ns + experimental_phase_ms × 1,000,000 + song_ns`，仍是 arrival-origin、rate=1 的實驗假設，不是已校正的遊戲時鐘或端到端輸入延遲。

命令軌跡／速度是送往 transport 的座標與主機時間，不是 Android 或遊戲內實際運動量測。RPC 成功不證明遊戲已接受；缺 step 也可能發生於回合停止之後。工具不重建判定線的 native pose，而是檢查已編譯的 screen-space 軌跡。

HUD 只核對 schema、source、幾何、取樣數與像素 SHA，再提供相鄰 receipt-time 索引，沒有 OCR。少 combo、少 receipt 或人工窗口都不會自動確定某個 Flick Miss。每個 note 維持：

```json
{
  "judgment_status": "not_determined_by_tool",
  "game_miss_confirmed": false
}
```

summary 的 `game_misses_identified` 為 0、`AP_verified` 為 false，表示工具沒有作出遊戲判定，**不是實測零 Miss**。報告的 `evidence_gaps` 列出沒有逐 note 遊戲判定、Android callback、完整 receipts 或可驗 HUD 像素等缺口。

manifest 與 compiled plan 的原生身分欄位會交叉核對，讀入檔案另記錄 SHA-256。現有 compiled JSON export 沒有完整 native fingerprint 重算所需的全部欄位，因此工具不宣稱已獨立重算並證明該 fingerprint。

## 6. 人工標註疑似 Miss 時間窗口

先執行診斷，取得 `chart_sha256` 和 `sources.manifest.sha256`，建立獨立的新標註檔。時間基準必須明示為 `song_ns`；若從錄影換算，應在 `evidence` 說明對齊方法與不確定範圍。

```json
{
  "schema": "pas.manual-miss-windows.v1",
  "time_basis": "song_ns",
  "chart_sha256": "替換成該譜面的64字元SHA256",
  "run_manifest_sha256": "替換成該回合manifest檔案的64字元SHA256",
  "windows": [
    {
      "id": "human-001",
      "begin_ns": 41000000000,
      "end_ns": 41200000000,
      "evidence": "人工覆看該回合錄影，此區間出現斷combo；尚未確定單一note。"
    }
  ]
}
```

每個窗口可選填 `note_ids`，值為該 compiled plan 中完整的 `source_note_id` 字串陣列，以限縮候選。沒有填時，工具對照 judgment 落在窗口內的 Flick；即使指定 note，也仍視為人工相關註記。

```powershell
python .\tools\analyze_flick_misses.py `
  --run-dir C:\private\runs\song-live-001 `
  --miss-windows C:\private\analysis\song-live-001-windows.json `
  --output C:\private\analysis\song-live-001-flick-annotated.json
```

標註必須綁定同一份 manifest／chart；換回合或變更原始 manifest 後，原標註的 hash 不相符會被拒絕。窗口不等於因果證據，也不會變更 `game_miss_confirmed`。

## 7. 本次驗證範圍

- Linux focused native test：[flick_policy_tests.cpp](../tests/flick_policy_tests.cpp) **61 checks 通過**，涵蓋 profile／CLI 參數契約、範圍／型別拒絕、compiler 實際採用參數、fingerprint、非 Flick Tap 不變與 first DOWN／opening 身分。
- Python focused tests：[診斷測試](../tests/test_flick_analysis.py) 與 [候選測試](../tests/test_flick_trials.py) 合計 **21 tests 通過**，涵蓋時間／座標配對、遲到壓縮、RPC failure、缺／重複 receipts、pointer 碰撞與正常重用、投影短軌跡、HUD hash、人工窗口綁定、唯讀輸出及單變量命令。
- 本次 Windows 原生 `chart_experiment.exe` 建置／執行、PowerShell 腳本實際執行、gRPC、裝置與遊戲 A/B：**NOT RUN**。

本次交付讓幅度與時機可以明確設定、重新編譯並逐回合對照。是否改善特定曲目的 Flick Miss，仍需上述本機受控回合與完整結算證據確認。
