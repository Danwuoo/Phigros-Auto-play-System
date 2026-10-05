# Round 2：C36h 結算原圖衍生副本與來源綁定覆核

研究日期：2026-10-05（UTC）。本輪 repo HEAD：`ebe955a6286ca1bb767b631948714a5545b7c174`；證據包記錄的本地 repo HEAD：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。兩者角色不同，並非執行版本矛盾。本報告只讀附件與 tracked 證據、撰寫研究文件；沒有產品執行、重播、裝置／觸控、正式碼或原證據改動。

## 1. 先前結論 → 新證據 → 修訂判定

| 先前結論 | 本輪新證據 | 修訂判定 |
|---|---|---|
| 首輪只持有 tracked 轉錄，未親自看 C36h 結算圖 | 已以 `view_image` 原解析度實際查看包內 `result.png`，逐欄獨立轉錄；重核四份 C4 檔與歷史總帳的 hash 身分 | **同一歷史 C36h Dlyrotz IN13 樣本的結算數字已直接圖面覆核，與舊轉錄一致。** 不是新實戰，不增加輪數 |
| 最近 C36h 結果為 795950／496-11-0-77，zero miss 未達 | 新圖直接顯示 Miss 77；四項判定合計 584 | 未達結論不變；不能由分數、Bad=0 或較大 combo 改判 |
| tracked 89 輪＝70 HD＋19 IN，IN 完整 Miss=0 證據為 0 | 本輪只補其中 `/runs/88` 的同一圖與 metadata | **89／70／19／0 的原分母不變。** 其餘 88 輪未新增圖面覆核；不把此包當全歷史重新驗證 |
| 無原 events，不能重算 lifecycle／receipt／時序根因 | 新 summary 可讀，列出 events 兩段 SHA，但 events、rounds index、session summary 未附 | 可核 summary 的歷史主張與綁定；不能升格為完整開始→結算→釋放鏈重驗 |
| source absolute age、現行章節分母與 IN 解鎖 unknown | manifest／summary 的 source age 仍為 null；沒有新章節／IN 選曲畫面 | 全部保持 unknown。歷史曾玩 Dlyrotz IN 不證明現在所有曲解鎖 |

首輪來源：[全曲驗收研究](../LEGACY_ACCEPTANCE_RESEARCH.md) §2、4、7、9，以及 [ignored 最小輸入](../IGNORED_INPUTS.md)。本輪更新覆核層級，不回寫歷史原 JSON 或其 `numbers_reference.review`。

## 2. 實際圖面轉錄

C4 是四份最小樣本中的 `round-1/result.png`。以下數字来自實際圖面閱讀，不是以包內 README 代讀。原解析度為 1280×720；右上帳號區有不透明黑色遮罩，曲名／难度／分數／判定區可讀。

| 圖面欄位 | 本輪讀值 |
|---|---:|
| 曲名 | Dlyrotz |
| 難度 | IN Lv.13 |
| Score | 顯示 `0795950`，數值 795950 |
| Perfect／Good／Bad／Miss | 496／11／0／77 |
| Max Combo | 120 |
| Accuracy | 86.16% |
| 其他可見欄位 | C；Early 8／Late 3 |

`496+11+0+77=584` 是本輪轉錄算術檢查，不是根據譜面／遊戲內部取得的 note 數。Accuracy 為圖面讀值，未由分數或自行假設的公式反推。`summary.result_numbers` 仍為 `unknown_until_result_pixels_reviewed`：summary 本身沒有已辨識 P/G/B/M，本報告為新增的離線圖面覆核，不修改原 summary。

既有總帳 `docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json` 的 `/runs/88` 在 song、difficulty、level、score、Perfect、Good、Bad、Miss、max_combo、judgments 上全部相符；Accuracy、Early／Late 與 C 不在該 run 的對應數字欄。Accuracy 另與 [C36h 歷史研究](../../../catalog/02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md) 的 86.16% 相符。

這支持「此圖呈現的單次 IN 結算為 M77」。它不能識別 77 個 Miss 的實際物件／時間／原因，也不能獨自證明當時每次觸控的執行者、來源或全程沒有人工介入。

## 3. 原始 SHA 與衍生 SHA 分開核

相對根縮寫 `C/`：

`measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/`

本輪重算包內四檔實際 bytes／SHA，全部符合 package manifest 的 `derived`；其中 summary 未改 bytes。`original` 欄是原件歷史身分，對發生去識別的檔案，不能用來校驗本輪 bytes。

| 檔案 | original bytes → derived bytes | original SHA256 | 本輪 derived SHA256 |
|---|---:|---|---|
| `C/manifest.json` | 10293 → 8688 | `bf515cc46a728936643f4c4a0f69933c95caf9ad2aea2eae2bf0ea73b6bed996` | `3bad0f5aef266cd432113e1aee4dbe718f4ac7091488fc9102b9027d0e65163d` |
| `C/round-1/manifest.json` | 10311 → 8705 | `df84edcd4b50fb57ca91311705443b589238f2406dcc90945b11f8430867868c` | `45d5cd48dbbb7b968bf25705a839d177fc5b0bf968b073c8b7c3f12dfd8c5f79` |
| `C/round-1/summary.json` | 2477 → 2477 | `56e2f733ace6c47cbe2f4e2a218ec59318c8efe0058fa8ab3a6f34458c6f4446` | `56e2f733ace6c47cbe2f4e2a218ec59318c8efe0058fa8ab3a6f34458c6f4446` |
| `C/round-1/result.png` | 614091 → 824386 | `d85240867fa6386538076fb556c2f995f82e1e887d7675d64ab789a69c4271af` | `d5ecd02c4979c8a78bee1e080abc54a67f0eb5a9ba5e0f00a124e2c869b7796e` |

核對鏈如下：

1. tracked `/runs/88/result` 的路徑、原圖 bytes／SHA → package entry 的 `original` → `derived` → 本輪實際 PNG bytes／SHA。三段一致。
2. tracked `/runs/88/summary` 同法核對；原始 SHA 與本輪 SHA 相同。
3. summary 的 `result_image_sha256=d852…71af` 指向原圖，透過 package mapping 對應本輪 `d5ec…796e`；summary 的 `manifest_sha256=df84…868c` 同法對應本輪 round manifest `45d5…5f79`。兩個 binding 均吻合。
4. tracked `/sessions/2/manifest` 的 original bytes／SHA 與 package entry 相符。解析後的 round manifest 恰等於 session manifest 加上 `round_id:1`；round manifest 與 summary 的 round ID 相符。

### 3.1 去識別聲明的可驗與不可驗

本輪能驗：新 PNG SHA／bytes、1280×720 幾何、實際可見的遮罩及結算數字；新 JSON 實際內容與原歷史 SHA 引用映射。包內聲明遮罩為 `(x=1040,y=0,width=240,height=112)`，遮罩不蓋住上述轉錄欄位。

**本輪沒有原始未遮罩 PNG。** 「遮罩外 894720 像素完全相同、不同像素 0」「原件未變」「去識別前後 JSON 非字串／結構完全不变」「未使用生成式重畫」是打包者提供的本地驗證聲明；雲端不能自行重做兩份原／衍生像素差分或去識別前後全量比較。hash mapping 有助追溯，不能單憑 mapping 證明衍生檔一定是由指定原件生成。此限制不妨礙直接讀取本輪圖面上的 M77。

打包者所留 manifest 本輪 SHA256：`ddc738c400ad29be11ffdcf9015b4acd31aed770b6bbcea60b55b08aab0178b3`。tracked 總帳本輪 SHA256：`610e7f613f2a5ce1e75ef05938a6207b067bfe9026bbba690ec63aa99ba0fe4a`。ZIP SHA 由父研究的收件核驗給出，本分支不把它寫成本分支親自重 hash 的結果。

## 4. 執行身分配對成立到哪一層

新 session／round manifests 與 tracked `/sessions/2` 的以下記載吻合：

- variant：`C36h-orthogonal-ridge-patch-recovery`；observer37／planner19。
- executable SHA：`61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`。
- source build commit：`e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`；dirty=true；21 項 compiled-source SHA map 完全一致。
- profile 原始 SHA：`4201679acc8ded39a394852d5f8d7f11fd62e736c32f10aa77657fc8fdbef18b`；嵌入 config 除已聲明去識別的 log_dir、serial 兩字段，與 tracked profile 逐值相同。
- gRPC payload／RGB888 top-down／1280×720、source rotation1；五指、touch geometry720×1280／rotation90；lead35ms／uncertainty30ms；read chunk262144 bytes。
- host_qpc_ns，QPC frequency10000000；manual-session、one-round limit1、full_recording=false、pixel_clip_sampling=true。

這證明**收到的記錄之間身分一致**，不等於本轮持有／重 hash 過歷史 exe、DLL、21 個 frozen source 原檔或完整 dirty build closure。不能僅用相同 37/19 號碼把它拼入歷史 main37/19，亦不能以當前 repo HEAD 代替此 binary 的來源。

裝置 serial 已替換為 `DEVICE_001`，Android／SDK／ABI／model／guest resources 為 REDACTED。`fingerprint_matches=true`、`mismatches=[]`、`real_input_created=false` 是歷史 preflight 記錄；不代表匿名化後的裝置重新做過 preflight，也不能建立當前裝置／帳號連續性。`automatic_play_enabled=false` 與 `manual_PLAY_only_gated_gameplay` 描述手動選曲按 Play 的策略，不能誤讀為歷史整輪完全沒有自動觸控。

## 5. summary 新增可讀時序，沒有解決 source-age 或逐次 RPC 缺口

summary 明列 `status=result_confirmed`、`stop_reason=six_current_result_labels_confirmed_over_60ms`、decisions7753、commands1770；release_requested／failed／unknown 三個 ID 陣列皆空。這些是保存的摘要值，本輪未由事件重算。

| summary 欄位（ms） | n | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|---:|
| capture_interval_ms | 7752 | 16.54785 | 30.84456 | 42.242408 | 360.9101 |
| playing_capture_interval_ms | 7284 | 16.57355 | 31.123365 | 43.946792 | 433.1117 |
| recognition_ms | 7753 | 2.867 | 6.3021 | 9.443968 | 18.4328 |
| host_residency_ms | 7753 | 4.0907 | 8.04692 | 11.612704 | 24.6779 |

關鍵 scope：`interval_scope = latest decisions consumed by action owner; includes skipped frames; first 100000 samples`。因此前兩列雖叫 capture_interval，**指 action owner 消費 latest decisions 的 interval，含跳幀**；不能改寫成遊戲 render cadence、capture callback 全分布或每幀來源新鮮度。playing 的 max433.1117ms 不能被另一較窄 scope 的 max77.3507ms 取代。

session、round、summary 的 `source_absolute_age` 全為 null。包內沒有逐次 injection_start／return、RPC receipts 或事件時序。因此：

- 不把 host residency／recognition 改稱 render age，不相加各欄 p99 估計 end-to-end p99。
- 1770 commands 不是 1770 個遊戲判定成功，也不等於可由此推出每個 Miss。
- 空 release 陣列不能替代完整停止／釋放事件鏈與所有執行中 unknown receipt 的逐項核對。
- 本輪既不能證实，也不能排除首輪提出的 source-age、序列多指／Flick backlog 或 BVI 靜止端部新 Down 等機制假說；它只給一輪 aggregate 結果及有 scope 的歷史摘要。

summary 引用的兩段 events SHA 與 tracked `/runs/88/event_segments` 相符，實際檔案未附：

- `events-0.jsonl`：`161f2f5230c007b51959f2d4b50a2ed7511984c81fd9904066702766a81ce741`
- `events-1.jsonl`：`31fa3efbe63d5b56bcad5424de46805dff02e3e3f51fe1d2667b9dfc16c41d19`

## 6. 全包引用映射的独立核對與最小閉包界線

本分支另外用 shell／jq／sha256sum 作文件維護級核對，不執行證據中的命令或候選程式：

- 26 檔實際 derived bytes／SHA 全符，共 2407833 bytes；22 份證據 JSON 可解析，共有 217 個 64-hex 字串 leaves。217 是欄位出現次數，不是 217 個獨立 artifacts。
- 156 個 `reference_bindings` 的來源 field 實際值全部等於宣告 historical SHA。
- 28 個包內引用逐一核「target path唯一 → entry.original.sha → entry.derived.sha → 實際 target bytes」，全部通過。
- 128 個 `not_in_package_historical_or_platform_reference` 不等於 128 個遺失檔案；這是引用邊數，可能重複指同一檔。

| 128 個包外引用在本輪 checkout 的狀態 | 引用邊數 |
|---|---:|
| Git 同相對路徑存在，實際 SHA 直接相同 | 27 |
| Git 同相對路徑存在，LF bytes 不同；唯讀串流改 CRLF 後精確等於歷史 SHA | 5 |
| 目前該相對路徑不存在 | 82 |
| 無相對路徑的平台／系統 reference | 14 |

上述五個換行案例位於 `research/x10d_o_bvi_r2f_control/`：`build-asan.cmd`、`build-debug.cmd`、`build-release.cmd`、`configure-asan.cmd`、`configure-debug.cmd`。本輪只對 stdout 串流做 LF→CRLF 後取 SHA，沒有改檔、更沒有執行 cmd。這精確解釋 hash 差異；不是將原 historical hash 當成 LF bytes 的 hash，也不是泛稱其他差異均可忽略。

82 個缺相對路徑引用對應 74 個 unique targets：36 條引用在 measurements 歷史 receipts／protection／build metadata／C36h events，46 條在原 out/vcpkg_installed 的 nlohmann headers。它們未包含是此最小包的已知邊界，不能一概列為 tamper 或強制索取整棵歷史樹。Git 已存在的 runner／protocol／supplemental 可按需使用，不必重傳；此報告未將它們視為「本輪已執行」。

156 個 bindings 不是全部 217 個 hash leaves 的完整信任圖；例如 C36h compiled-source／binary／profile hashes 是歷史 artifact 身分，當前四檔不提供所有對應 bytes。全部包內映射通過只表示此有限轉移包內部一致，不能推出完整原環境、歷史控制授權或所有缺席 receipt 均驗證。

## 7. 仍缺的資料與最小下一步

1. **只問這一輪結算數字：已收斂，不需再索取大包。** 保留此圖讀數與原→衍生映射；E0 的小樣本結算／identity-binding 檢查已達本包範圍，完整 lifecycle 部分另列未驗。
2. **要獨立重驗開始→結算→安全收尾：** 最少再提供同 session 的 `rounds.jsonl`、session `summary.json`，及 summary 已指定的 `round-1/events-0.jsonl`／`events-1.jsonl`，用原始 SHA 或另有去識別 original→derived map 配對；按原 segment 次序核事件。這不是要求7722 PNG或所有89輪，也不自動授權 replay／裝置操作。
3. **要重建 exact 歷史 binary：** 另按問題取候選 freeze／compiled-source／DLL manifest 與必要 bytes。現有21個SHA與dirty=true不足以重建；不把本地私人路徑／匿名serial当作可執行環境。
4. **要回答現在全章節還差哪些 IN：** 需要當前版本、Chapter Legacy 首尾及連續重疊清單、逐曲 IN 可選／鎖定提示的去識別畫面。舊 raw 不能補此答案；現在章節分母 null、各曲解鎖 unknown 保持不變，不索取帳號、存檔或憑證。
5. **要定位某一 Miss 或 source-age／串行延遲：** 先選能回答指定假說的事件窗與 matching pixels／index／journal，保留必要暖機或 owner 初始狀態；source age 需來源校準證據，RPC 需逐次起訖。結算圖與 aggregate summary 不足，不能從 M77反推機制或認證 proposed 修正。

配套小型查核摘要：[result-provenance-summary.json](result-provenance-summary.json)。只保存去識別的數字、hash 與核對結果，不複製原圖、raw、整包或原私人值入 repo。本報告不授予新 live／全曲驗收資格，也不把改進目標加成 AP 前置。
