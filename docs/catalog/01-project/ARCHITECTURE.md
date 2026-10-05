# 現行架構與資料契約

2026-10-04，main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，**observer50／planner27／diagnostics11、live0，是comparison／donor**；behavioural／experimental baseline仍是另有frozen來源的C36h tint1。本文按責任描述main50正式程式，不把其owner規則套到C36h。產品目標、逐曲證據、最新研究狀態與工作包見[10/4總帳](LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)；[10/1狀態](PROJECT_STATUS_NEXT_STEPS_20261001.md)按J5–J32接續，早期段落為歷史。來源沿革見[整合交接](../06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)、[冷開發](../04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)及[熱調試](../02-game-results/FOUR_SONG_HOT_TUNING_20260929.md)。模型只離線；main50或研究工具通過不授予live-ready。

## 即時閉環

```text
gRPC capture → LatestFrame → SessionPerception / GameObserver
                              → 最新完整 DecisionSnapshot
                              → SessionGameOwner / GamePlanOwner
                              → ContactScheduler → GrpcTouch
supervisor ───────────────────→ revoke / stop / release
只讀旁路：preview、journal、結果圖、pixel clips、full-recording
```

正式邏輯為單程序多執行緒 C++20。capture、perception、action owner、supervisor、診斷 writer 分工；只有 owner 可修改 scheduler／發送觸控。runtime 不讀譜、遊戲內部狀態、音訊節拍、曲名／進度、離線標籤或歷史按鍵。模型不連到正式閉環，`pas`／`pas_core` 不連結 LibTorch。

## 固定擷取、時間與所有權

- 固定 gRPC payload fast、RGB888 top-down、1280×720、source rotation 1、read chunk 256 KiB。profile 相對 lag guard 250ms，不是來源絕對年齡。
- 邏輯 latest-frame 容量 1，固定三個物理 buffer；reader 持有時不可覆寫，耗盡 drop 並計數。慢消費者跳過舊圖，不無限排隊。
- host 統一 QPC nanoseconds。Frame 保留 sequence、epoch、generation、geometry、capture_complete、pixels_ready、published；辨識起訖、owner 接受、預定觸控、injection_start／return 分開記錄。
- source Unix／Android／WGC／PTS 保持獨立 domain；未校準 source age，不把 receive time 改稱 render time。
- frame context、尺寸／方向改變或來源失效撤銷資格。WGC／DXGI／scrcpy 僅 bench，MMAP diagnostic-only，無已驗正式自動備援。
- 五指 capability 必須匹配裝置／APK／幾何／mapping 指紋；Fixture 多指成功不等於完整 Phigros 動作語義。

## 當前像素、線與 Note 身分

`GameObserver` 由当幀 RGB 提取 HUD、線、色芯與 Hold 外框候選，`GameLineTracker` 維護線 ID／有界 pose，Note identity、intent identity、contact ID 各自獨立。線至多 16、note／track 至多 128，近期 pose 至多 6 點／90ms；跨 epoch／geometry、非遞增時間或過大 gap 清理。

當前線包括長 run、局部連通區線段及 C36h 正交 ridge。局部線段分割避免整塊 X／V／框／放射白區只剩單一 PCA 方向；染色 ridge 仍需當前純白種子與局部對比，不能由全暖色特效或歷史白線續租。Hold 相鄰 body／側軌排除、最多 16 線等界限保留。Flick 僅在當前中央箭頭支持、局部切向／厚度／gap 合格時合併核心，沒有箭頭的相鄰雙 Flick 不合併。

線用至多 16×16 全域可行 assignment；競爭未分派不即刻出生，近鄰可分離候選需連續三幀才出生，暫存至多 16 筆。歧義線可保留歷史 ID 作診斷，不能更新 measured lifetime／motion fit 或取得動作資格。`lines` 只列當前量測，不把投影充作可見線。

Hold 在同一組當前側軌上優先找既有 held body，再處理前景片段；已用 rail pair 不能被新候選重複占用。每幀仍須重新支持 body／rails，不能複用上一幀的 boolean。這是候選提取與身分續接，不等於 owner 已有 Down。

## Note→line 與撞線預測

有限候選以線局部距離、沿線範圍、近期相對接近、原 line ID 及弱外觀方向評分；拒絕 association_invalid 線。遠處 Note 朝向不等於運動方向，不能要求當前同法向才配對。最佳與次佳分數差不足時 unknown，不建立 root。

至少三次、跨 30ms 的實測可確認 Note→line 關聯。原線仍當前可見時有保留已確認關聯的分支。**現行程式並非嚴格單調接近**：`game_tracking.cpp` 的 `preserve_confirmed` 接受「當前絕對距離 ≤ 前次距離 + max(12px, Note寬×0.10)」。小幅逐幀遠離也可能持續通過；這是需反例約束的既存風險，不能沿用舊文字「只在持续接近」當保證。

原線消失或候選換 ID 時，不能直接跳到不連續的新線。若新 ID 在上一接觸區延續原線，需局部切向 mod π 點積至少 .97、法向差不超過 36px；一般路徑需兩份間隔至少 12ms 的新鮮影像，近線且當前支持的 Drag／held body 可立即重接。重接清掉舊線速度擬合，不轉移舊 root。這些條件仍可能過度保留錯誤關聯，實例見現況報告的 ordinal6214。

root 由 Note 與線的局部法向距離及近期相對運動取得。反轉／跳變斷開舊擬合，通常至少三點／30ms 新量測才輸出 root；空間殘差與時間 uncertainty 分開。基準 uncertainty 30ms、lead 35ms；lead 不是已量得的固定 gRPC 延遲。當前 Tap 重疊路徑有獨立像素／短期追蹤資格，basis 為 `live_pixels_current_tap_overlap`，不宣稱有舊 root。

Tap／Flick 有嚴格有界的短缺線投影：當前 Note 仍可見、同線曾有至少三次／30ms 實測、最後 pose 運動擬合有效、缺線 ≤40ms、相對接近無反轉且誤差合格、落點仍在原／投影線段、局部無衝突新線。投影不寫入實測歷史，以 `line_projection_only`／`last_line_observed_ns` 區別。超過 90ms 清掉舊確認 ID；新線需重新取得資格。basis `live_note_recent_confirmed_line_projection` 只支援未執行 Down 的有限修訂，不使活動 contact Move 或續命；Hold／Drag 新 Down 仍需當前支持。

斜向、旋轉、線追 Note、晚對齊、反轉、突現與往返以[判定線形式](判定線形式.md)作覆蓋清單。幾何過線不等於音符完成；返回不授權重播已執行／未知 Down。

## 動作與 Hold 接觸契約

| 責任 | 當前行為與界限 |
|---|---|
| 門控／到期 | gate 與每個 plan 獨立核對，now ≥ deadline 即到期；source／target 證據上限 100ms。UNKNOWN UI／context／input fault 撤銷全部 |
| 未執行 Down | 新完整快照目標消失即取消；motion discontinuity 等新矛盾也撤銷。僅確認 cursor=0 才可由後續新鮮像素建立新 intent |
| 已開始 Hold | 當前 body／rails 支持續接同指；接觸位置由當前支持修訂 Move，不固定初次 Down，不盲隨線旋轉。無 root 本身不等於活動 Hold 失效 |
| patch/front 切換 | 同一 Note ID、同非零 line ID、兩份當前 rails/body、宽度／方向及 body 覆蓋合格才放寬法向位移至 128px；此放寬不給新 alias |
| contact alias | 主線已有獨立的 current body alias 規則與有界表；須保留已執行 cursor，已 Up／completed identity 不復活。不能把「新增 alias」重複列為未實作需求 |
| 明確矛盾 | 已提交目標若 samples=0、關聯歧義、證據倒退或到期，可在 body grace 分支之前直接取消；60ms 不是所有失效的共同緩衝 |
| 有限 missing grace | 活動接觸按既有規則：Tap／Drag 40ms、Hold 60ms、Flick 75ms；Hold body 不支持時不更新證據、不 Move，超時釋放。不以 grace 掩蓋所有錯配 |
| tail 結束 | 與當前線／時間一致的可見 tail，需兩份新鮮支持才正常 Up；無 tail 仍可因失效安全釋放。tail 預測不能單獨續命 |
| 注入結果 | RPC success 只代表呼叫返回。未知 Down 不重試；失效撤銷並保留釋放責任。遊戲是否採納仍需獨立證據 |

旋轉 Hold 的 head 接入、body 接觸與 tail 結束分開驗證。已存在旋轉／平移 36 幀 RGB、混合雙 Hold＋Flick、兩指容量負例及獨立 oracle／fake-clock 回歸；這些只支持軟體契約，沒有建立實景完整判定真值。

## 多輪生命週期

manual-session：STANDBY → STARTING → PLAYING → RESULT → STANDBY。使用者選曲及按 Play；當前 HUD／observer gate 才可觸控。六個英文結算文字須三個不同新鮮 frame、跨度至少 60ms；第一份結算證據即關閉新 Down。空白、無候選、HUD 消失或暫停不算結算。

真正新 round 重設 observer／owner；曲中 HUD／source 撤銷不清除已完成 identity。geometry／generation 改變、未知注入／釋放結果為 FAULT；finalize 保存首份 release report。watchdog 只管停止，不當歌曲時鐘。`--one-round` 在一輪完成／中止、釋放與 archive 完成後停止；`--full-recording` 包含此單輪模式。

profile 共用 `game.lead_ms` 容許 30–45ms，manifest 記有效值；35ms／40ms 的歷史結果需按版本分讀。`run --manual-play` 是另一有限等待入口：首次 pixels 確认 playing 才開始 duration，重複 playing 不重設。操作入口不代表本輪獲准啟動。

## 診斷容量與資料用途

| 元件 | 現行上限／用途 |
|---|---|
| SessionArchive | mailbox 8192 事件且 16MiB；每輪 16MiB×32 段，超額 FAULT；最多一張待編碼結算圖 |
| 待命 journal | 1MiB×4 輪替；不累積全歷史 round vector |
| 每輪統計 | 四個 vector 各 100,000 樣本；完整 raw 與有界統計範圍分開 |
| Pixel clips | opt-in；20輪×每輪最多30張全 RGB；writer mailbox 4；採樣失敗不回饋 owner |
| Full recording | opt-in；32 preroll＋64 pending＋3 encoder＝99 RGB slots；每輪 PNG＋index ≤5GiB／36000張／600秒，越限或溢出明列 incomplete_fault 並停止 |
| Full-recording 選片 | ≤32 clips、每段 core ≤40秒／context各≤5秒；unique ≤12000 PNG／references≤18000；專用 PNG≤3GiB，其餘 index／journal 界限見全錄契約 |
| 預覽／候選 bank | 只讀、有界；proposed 不算人工 gold，不作觸控決策來源 |

診斷單向流出。full-recording 故障可停止 session 並釋放，不能改變動作策略或偷降採樣；錄到所有 received pixels 不等於遊戲來源完全無漏幀。原圖與 QPC／source domain、ordinal／source_frame／SHA 映射分開保存。完整界限見[全錄契約](../06-engineering/FULL_ROUND_RECORDING_20261001.md)。舊研究的兩秒 ring／512MiB arena 是另一未落地提案，不當作現行 full-recording 容量。

## 離線工具與驗證邊界

- `pas_frame_review`原observer-only模式：核對全錄SHA、原cadence＋最多32preroll暖機observer、join原journal；原summary cancellations屬journal。這個模式與後來X1完整接觸模式分開，不將舊模式的能力限制套到整個工具。
- `game-clip-replay`：短 clips／triples 的冷重播，不恢復片段開始前的 contact。
- `analyze game-cold-pipeline`：有界預產合成 RGB、正式 LatestFrame／observer／owner／scheduler＋FakeTouch、Journal 三執行緒跑 QPC。fake-clock 時序回歸與主機成本分開；原發布時間 race 已修正，perception 讀 lease 內已同步的 published_ns。
- `pas_vision_cpu`：CMake `PAS_ENABLE_CPU_VISION` 預設 OFF，獨立 C++ LibTorch CPU exe。4377參數合成小試、18 native ROI proposal、gold gate／序列化已存在；人工 pixel gold=0，無跨曲準確率或 runtime 模型。只允許輔助離線標记和通用程式優化。
- **X1已完成／獨立驗收**：fixed-pixels的SessionPerception生命週期／reset／gate、owner／scheduler／FakeTouch、幀間due及診斷排空，R1/R2來源與CLI修補見[交接](../04-offline-research/C36H_CONTACT_REPLAY_REPAIR_20261002.md)及status J5。Replay只回答其明列clock/cadence/fake-delay契約，不含新touch feedback、真recognition/RPC成本或physical gold。
- **後續採用界線**：X10d-P隔離C36h候選只帶pending-missing hook，冷契約已獨立通過；X10b suppression仍否決／OFF，X10d-O未開始。X11-P/R1/R2/R3成本仍not-ready；A reader重現與新checker獨立驗收分層。未經完整行為／有效成本／遊戲結果證據，不移植為產品策略。

## 量測與維護

量測必報環境、n／p50／p95／p99／max、失敗與 jitter，保留全部 publish 嘗試、skip／drop／late 分母；不把各階段 p99 相加，不平均各曲 p99。主機 RPC 時间、owner lateness、預測偏差與遊戲可辨效果各自獨立。六區塊線掃描预檢的既有冷 ABBA／三執行緒 gate 見冷結果；密集場景額外逐批加速未過，不能只報成功子集。

原始跨曲證據、frozen binary、失敗實驗與必要輸入的歷史保留／已刪範圍見[10/1清理](../06-engineering/CLEANUP_AUDIT_20261001.md)及[9/28清理](../06-engineering/CLEANUP_AUDIT_20260928.md)。10/4現查：舊recovery目錄未registered，其中LibTorch／CPU runtime路徑不存在；main CPU cache仍引用該歷史絕對路徑，不能默認可build，未重裝。新維護資料容量／引用檢查見10/4總帳。修改正式契約須同步文件與可重現回歸，原frozen來源不回寫。
