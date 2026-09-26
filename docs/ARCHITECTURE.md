# 架構與資料契約

## 主程式設計與開發接線（2026-09-27）

observer22 分開即時 HUD 分類與 motion history 的連續性。相同 generation／geometry／尺寸／rotation、sequence／QPC 嚴格遞增，且間隔≤250ms時，可以保留三幀分類計數；每張仍必須有當前 pause／score 像素、有效 source 與容量才能開 gate。epoch 改變或間隔>100ms立即清空所有 motion／rail anchor，新目標需重新累積跨度。缺 HUD、無效 source、容量失效、重播、>250ms間隔或幾何改變皆冷啟動分類。此上限只屬分類確認政策，不能刷新旧 evidence；scheduler／supervisor 的100ms期限及epoch撤銷不變。

observer21 的hit是實際頭部在當前可見線上的正交投影；擬合distance只估計crossing與uncertainty，不以殘差偏移hit。planner6可讓新鮮可靠、同位置≤2px且覆蓋時間重疊的Drag沿用已active的Drag plan，只延長up、保留原down及prefix。alias／primary仍包含於128 identity上限；仍有支持的成員可維持同一接觸，全部失效或gate撤銷則釋放。pending down、其他Note種類及不同位置不共用；兩指能力與100ms来源期限維持。`game_drag_coverage`最多128筆待取，分析區分physical down與有successful RPC歷史支持的本機共用；遊戲判定仍unknown。

observer20 的新Hold前緣兩排填色除7／9之外，兩側外部取樣群各須至少一點支持，避免一側完整body加另一側裝飾線擴大寬度。opt-in第二診斷槽僅保留近線association ambiguous或有≥30ms跨度、尚未到線的nonlinear mismatch；初始insufficient history及已到線的停止不單獨觸發。這是診斷訊號，不是逐Note判定真值。

observer19 的短期點保留最多六點／90ms，每身分10ms的QPC bucket只保留最新坐標；bucket起點固定直到新增下一點。速度外推／相對撞線擬合／接近rail搜尋均要求至少30ms跨度，沒有足夠跨度列等待，不用低殘差單獨接受短擷取突發。每幀的即時辨識與當前輪廓核對仍執行；`history_span_ns` 為schema2的可選診斷欄位，舊紀錄缺欄位不反推真實來源時間。

observer18 的 simultaneous highlight 過濾以黄色候選的法向厚度核對可見藍／紅芯寬度（上限為 max(24 px,25% core width)），並保留原位置、寬度與當前芯像素支持要求；獨立 Drag 不因高度變大而一律刪除。decision schema2 可附 `rails_geometry`／`head_on_line` 診斷欄位，舊紀錄缺欄位視為 unknown。離線 C++ conflict 分析最多保留64筆摘要、16個成功 RPC 本機接觸、128個最新 targets、每計畫16步；成功 receipt 與 owner reset 只能重建本機歷史，不證明遊戲效果或三押需求。

使用者改採 Glaciaxion HD 直接實戰研究、首次 AP 後進 IN；[主程式計畫](MAIN_PROGRAM_DEVELOPMENT_PLAN.md) 定義新 G0–G6 與資料契約，取代獨立 M3 Fixture 閉環前置順序，不改寫歷史結果。

單程序 C++20：既有 capture worker／LatestFrame → perception worker（UI、線／Note、追蹤、相對運動預測）→ 最新完整有界 DecisionSnapshot → 唯一 action／scheduler／touch owner。supervisor 可獨立撤銷；preview、journal、結果分析只讀，曲名／難度只作記錄。

遊戲開發版已分離 Note 身分、單調提交 intent ID 及 contact ID；未來意圖與近期可執行計畫分開，contact 在 down 時分配。active revision 保留已執行 prefix、進度與釋放責任。目標證據失效撤銷單目標；全局 UI／來源／輸入失效撤銷全部。共通 scheduler 尚未接多點 batch；相關實作與驗證界線見 [開發紀錄](GAME_RUNTIME_DEVELOPMENT_20260927.md)。

動態線預測使用 Note 與線的相對法向距離及局部運動，不固定螢幕 Y。以主機接收時間擬合的是表觀撞線估計；絕對 source age 仍 unknown，綜合提前量需實機校準且不能當作某段真實延遲。遊戲判定回饋無法唯一配對時標 unknown，不以結算反推逐 Note 時間真值。

`src/runtime.cpp` 已接 capture／perception／單一 action owner／supervisor；`src/game.cpp` 提供有界 UI／線／Note 候選、短期追蹤／預測與 planner。observe 不建立真實 input；auto-start 經能力指紋與即時 MENU 證據只發一次 PLAY down/up，曲中仍 dry。assist 經同一指紋核對使用共用 GrpcTouch，只有 action thread 可執行 UI 或 Note 計畫；離開 MENU 後只撤銷 UI scheduler 一次，避免其釋放曲中接觸。MENU／PLAYING 為開發分類，PAUSED／RESULT 等仍 UNKNOWN，UNKNOWN 撤銷 gate。完整契約、停止、容量及驗收方式以主程式計畫為準。

assist profile 的可選 `game` 物件嚴格限制 `enabled_types`（1–4 個不重複的 tap／hold／drag／flick）、`lead_ms`（整數 −60 至 60）及 `uncertainty_ms`（整數 1 至 60）；省略時預設 Tap／8 ms／30 ms。Hold 可由新 pixels 修改未執行的 move／up，已執行 prefix 不可改；`prefix_offset` 允許丟棄舊已執行步驟但必須保留最後一步，防止長 Hold 的 plan 無限增長。每次觸控 receipt 記錄 QPC scheduled／start／return、source frame、intent；RPC success 不代表遊戲 Perfect。

decision schema 2 明列 Note anchor 語義（Tap／Flick 芯中心、Hold leading edge）及 color core／近期 Hold 與當幀 parallel rails 的觀測依據。後者只在新鮮 PLAYING gate、最近 100 ms 身分、當幀支持充分的線與兩側輪廓同時成立時續接；裁切 tail 仍 unknown，不能由舊 body 長度計時維持。observer 11 的 rails 一般須在 projected head 12 px 內開始支持，且延續至少 16 px；前一張已確認 rails 且當前 head 仍有藍／灰 body 填色時，才容許最多 32 px 的短邊框遮擋。成立時優先維持完整 Hold 身分並抑制其當前 body 內厚核心碎片，獨立薄 Tap 與其他位置的候選保留。已確認 rail 幾何／QPC 另有每身分一份、90 ms 上限的 anchor；當前核心碎裂不能覆寫它，只有當前 rails 再驗證成功才恢復原身分，不以歷史 anchor 單獨延長觸控。歷史 schema 1 可由 C++ 分析器重算，不能回填新觀測依據。計畫記錄 `accepted_ns`，分析分開 future-at-accept 與 already-past-at-accept 的 down lateness；缺此舊欄位時列為 unclassified。

少量候選漏辨的容忍仍受像素證據期限約束：Tap／Drag 40 ms、Hold 60 ms、Flick 75 ms；超過時取消個別意圖，UNKNOWN UI／source 失效則立即取消全部。不由 tombstone 重啟已完成意圖。離線分析另保留最多 512 個近期觀測身分，按是否形成近線預測、計畫接受及實際 down 列結果；像素身分可能碎裂，這些數量不能當作真實譜面個數或逐 Note 判定。

observer 17 增加無歷史前提的當幀 Hold 外輪廓重建，schema 2 的 `observation_basis=hold_current_parallel_rails_and_fill` 與近期身分續接分開。只在充分可見的跨畫面主線存在時，從當幀藍色 body 碎片搜尋兩條窄中性白輪廓；最多 128 個種子、每截面 16 個窄帶、兩個截面、129 個前緣探針。兩側最多容許 32 px 短遮擋，仍至少延續 `max(24 px, width×0.25)` 且末端相差不超過 20 px，前緣兩排各九點至少七點有藍／灰填色，前方一排須已離開完整填色以排除內部假 head；尾端另受當幀較淡 body 填色約束，避免輪廓接上裝飾線。當前完整前緣不再由舊位置另擬合一次；到線的既有 Hold 仍用原近期身分路徑。辨識可在 UI gate 成立前提供幾何，觸控仍必須通過獨立三幀 PLAYING／新鮮來源門控。傾斜薄 Tap 的正常方向厚度受到保護；無 body、單側輪廓及短閃光不形成新 Hold。

到線的已確認 rail anchor 在 90 ms 內可排除其近期 body 中的新假 head（以完整候選寬度是否落在 body 半寬+16 px 內判斷），但這項排除不產生新的觸控證據；既有 Hold 續接仍驗證當前雙側輪廓。當前未到線的破碎候選不得覆寫到線 anchor。新頭部的前緣仍必須有藍／灰填色；尾端及前方是否延續的檢查另外識別當前黃色覆蓋，避免內部特效造成假 body 邊界。黃色本身不能建立新 Hold。

observer 16 將薄 Tap 的法向厚度保護同步套到近期 Hold 續接路徑，避免傾斜包圍盒較高時刪掉獨立 Tap。當幀重建已具兩排完整藍／灰前緣時，雙側輪廓可接受黃色覆蓋；到線續接則另要求 90 ms rail anchor 與當前 head 填色支持，才開啟黃色輪廓容忍。只有黃色／白色線條或無當前 body 填色時，不能因此續接。

planner version 5 以預估 crossing 到現在不超過 40 ms 為晚預測補接下限，與校準提前量分開；尚未執行的 down 可改為立即排程。原 100 ms 證據與 UI gate 仍必要，已執行 down 不重播。`ContactPlan.predicted_down_ns` 保存原始預測下壓期限，scheduler 只依 `steps[].due_ns` 執行。分析分開原預測期限的晚到量、實际排程晚到量與刻意 clamp 次數；舊 log 缺預測下壓期限列 unknown，不將立即补接解釋成新計時性能改善。Drag 名義接觸 90 ms，實际接觸亦可能因證據／gate／漏辨撤銷提前結束。

assist 的可選 `--keep-diagnostic-anomalies` 僅保留最多兩張與決策配對的原始 frame（近線窄 Hold 對、近線 history／association／fit 失敗）。兩個固定 slot 各只寫一次，copy 在 decision 發布後產生，不佔 capture pool lease；input owner 停止並釋放後才 PNG 編碼／hash，記來源 frame 與保存結果。這些副本與檔案不回饋遊玩；預設不保留任何 frame。開啟診斷 copy 的 run 不作為無診斷的性能驗收。

action owner 以 QPC deadline 計算相對等待時間，用 Win32 高解析度 waitable timer 與 auto-reset wake event 等待；最新決策、撤銷或停止可立即喚醒，無跨時域絕對定時。效能需看各 run 實測分布。Console Ctrl-C／Break 只設定停止旗標，由 supervisor 通知 owner 釋放接觸，不在 OS handler 中呼叫 RPC。

## gRPC Windows 接收區塊

正式 dependency 由 vcpkg manifest 的本地 overlay 固定為 gRPC 1.81.1、port revision 2。補丁在 Connect 呼叫時複製 channel argument `pas.grpc.windows_read_chunk_bytes` 的整數值，交由 Windows EventEngine 非同步建立的接收端使用，不延後存取設定參照。`GrpcCapture` 預設 256 KiB；profile 的 `capture.grpc_read_chunk_kib` 與 capture bench 的 `--grpc-read-chunk-kib` 僅允許 8／64／256。未指定的 channel（包含 touch）維持 8 KiB；HTTP/2、BDP 及 frame pool 策略未改。

`grpc_transport` manifest 保存 client version、patch version、實際 read bytes 與 argument 名稱；campaign plan 鎖定該物件，分析拒絕新批次與計畫不符的傳輸設定。舊 evidence 沒有此欄位時仍使用舊契約。profile 缺少新欄位時採 256 KiB，public config 會補上實際值。補丁的版本 symbol 必須由連結的 library 提供，防止 stock gRPC 靜默忽略私有 argument。詳細驗證見 [接入紀錄](GRPC_TRANSPORT_INTEGRATION_20260926.md)。

## 畫面保留政策

使用者要求已處理畫面不持續占用儲存空間。正式擷取仍只使用有界 Frame／reader lease，消費完畢釋放引用後重用 buffer，不逐幀落盤，也不逐幀向 OS 釋放再重配置。擷取 bench／兩種 campaign 預設不寫 `diagnostic.png`；明確指定 `--keep-diagnostic-image` 才保存一張 READY 圖，隨即釋放 READY lease。所有成功／失敗路徑的預設模式都沒有圖檔可累積。

新 manifest／summary／campaign-plan 記錄 `diagnostic_image_retention=none|keep`。none 要求 PNG 不存在且 hash=null；keep 驗證檔案及 hash。schema-3 舊 plan 沒有 retention 字段時仍依原規則必須有 PNG，不能以檔案缺失當成已清理；舊 schema-2 保留其原驗證語義。raw JSONL 與必要配置／數值證據繼續保留，不因畫面不落盤而失去可重算性。歷史圖檔不由新程序掃描／刪除。

## 最後選型決定

依[擷取器終態與最後驗收](CAPTURE_FINAL_ACCEPTANCE_20260926.md)，本輪五路徑研究與 gRPC 接收層正式化已完成。主用為 gRPC payload fast（RGB888、top-down、顯式尺寸／方向、256 KiB 接收區塊）。WGC 僅為 bench 備援候選，正式備用暫缺；DXGI 留作受限比較、scrcpy 本次軟體 H.264 配置不列主／備、MMAP 維持診斷，沒有自動切換。

gRPC revision 2 的歷史 Release 34／34 回歸與 18 批短測、正式擷取／暫停恢復／observe 已完成。2026-09-27 新增 profile 的 `capture.max_relative_lag_ms`（1–1000，預設 250），`runtime_capture_options()` 真正傳遞至 gRPC，public config／manifest 記錄有效值；相對 lag drop 撤銷當前 epoch。遊戲閉環依 G1–G4 推進；絕對 source age、長期性能及遊戲時序尚未驗收。舊 raw 性能與完整矩陣通過旗標保持原值；不再自動續跑取消的矩陣／長測。

## T6 擷取候選開發契約（2026-09-26）

量測範圍依使用者最新 [縮短修訂](CAPTURE_FIVE_BACKENDS_PLAN_20260926.md#最新使用者修訂縮短剩餘測試) 收尾：本版原 96 批未完成，正常資料與必要恢復／120 秒補測提供初步選型，沒有長期穩定性或完整故障／負載資格。結論與適用條件見 [比較報告](CAPTURE_COMPARISON_20260926.md)。下列資料所有權、pixels-only、時域和有效性硬條件仍適用；新後端接入 Session／M3 需另做顯式門控。

Native Fixture counter 驗證需要色彩四區與獨立黑白 binary 交叉核對；相同的壓縮色差可能碰巧保留 XOR，不能只憑四區 byte 一致就接受。raw 保留兩個解碼值；lossy 分支只供 scrcpy 的正式 schema，其他後端仍要求 exact 格式。這些真值只供測試／離線分析，沒有進入遊戲決策。DXGI 整個 crop 必須位於 client 內，並位於選定 monitor 內，不能以九個內部可見點代替完整邊界檢查。

WGC frame pool 容量二，每次回呼最多取兩張並 Close 被替換圖；以主機／SystemRelativeTime 的 elapsed 差監控相對積壓，預設 250 ms 上限，舊圖先丟棄再進 GPU readback。時戳倒退終止來源；stream 重建時另建 guard。該時間不是 Android render age。停止時 shared callback state 先 closing 並排空正在執行的回呼，後撤銷事件、Close session/pool；本機 Windows 的 `GraphicsCapture.dll_unloaded` 崩潰以僅常駐 System32 模組修正，session/frame/GPU 資源照常回收。證據與完整重測見 [就緒紀錄](CAPTURE_READINESS_20260926.md)。

bench 的來源靜止、GPU 負載和預覽都是診斷選項。靜止只操作原生擷取 Fixture 的 property，控制 thread 確保恢復；ADB reference 的讀取前後都記 QPC。GPU 負載同程序、單 dispatch 在途、二秒完成期限；資源採樣收錄 PDH GPU Engine 的獨立 instance 百分比，不假設加總等於整卡百分比。預覽用 CPU RGB24 做 D3D11 上傳，common capture endpoint 保持相同；明示的本機 preview placement 不得當跨機器通用座標。所有新後端仍只在 bench，pixels-only 的遊戲觀察契約不變。

五條擷取路徑沿用同一 `LatestFrame` 和有界 Journal；比較終點是完整 CPU RGB24。`Frame` 新增 backend、原始格式／stride、裁切、來源有效性，以及 receive、copy、GPU readback、decode 的可觀察時間點。未能直接觀察的階段維持 null，來源 Unix、Windows WGC system-relative 100 ns、DXGI QPC ticks、scrcpy PTS 微秒分開保存；`capture_complete_ns` 依後端分別是完整 gRPC payload 到達、WGC frame 取得、DXGI `AcquireNextFrame` 返回、完整編碼封包到達，MMAP 診斷則是通知後 snapshot 複製完成。絕對 Android 來源年齡仍 unknown。

Emulator gRPC 的幾何由實際顯示 profile 決定。先前內嵌直向視窗回傳 720×1280、rotation 0，bench 可明示 `--grpc-rotate-ccw` 正規化 -90°；raw dimensions、rotation metadata 與正規化角度分別記錄。控制配置先逐列複製再旋轉，優化配置融合旋轉直接寫入輸出 buffer。獨立橫向視窗已驗證直接回傳 1280×720、rotation 1，正式 campaign 使用該 profile：控制逐列複製、優化單次 `memcpy`。Session 既有 profile 不受 bench 選項影響。

DXGI 在取得 frame 前及 RGB readback 後再次確認目標前景、client 尺寸／DPI、monitor 裁切位置與遮擋；若擷取期間位置變更即丟棄並 fault。上層視窗以 DWM extended frame bounds 檢查，排除 cloaked 視窗；GetWindowRect 包含不可見 resize border，僅在 DWM 查詢不可用時保守回退。未知範圍即拒絕，沒有按視窗標題豁免。WGC 的 ContentSize／DPI 變動也撤銷 crop profile。

WGC 擷取明示的 HWND，使用 free-threaded frame pool、D3D11 staging readback 與 BGRA→RGB24；DXGI 擷取明示的 monitor，將 HWND client crop 轉到桌面像素，限制目標前景、可見且無上層視窗相交並通過受檢點，access lost 後重建 duplication。原生後端遇到 GPU device lost 最多重建兩次並記錄事件；尺寸、旋轉或視窗有效性不符即 fault，必須重新校準 profile。scrcpy 使用官方 v4.1 server，H.264 packet/session 解析及固定 FFmpeg 軟體解碼，音訊／控制關閉；目前 AVD 清單確認 `c2.android.avc.encoder` 為軟體 H.264 編碼器，正式配置顯式指定該名稱且禁止編碼錯誤時自動降解析度。封包至多 4 MiB，PTS 相對積壓超限最多重連一次並記錄 gap。新候選現階段只供 bench，尚未授予 Session 資格。MMAP distributed proto 明示可能 tearing，保持 diagnostic-only，見 [可行性報告](MMAP_FEASIBILITY_20260926.md)。

`capture-five-campaign` 預先列出 40／48／57 Hz 的三輪輪換日程及壓力／長測，再逐案串行執行。分析從原始 JSONL 重算半開窗口、階段耗時、可見 counter、資源 brackets，驗證原始資料與診斷 PNG hash；正式性能數值門檻仍是 null。本節描述開發契約，不等同五候選已通過實機測試或完成選型。

## C++20 現行架構（T0–T5）

正式核心為單程序、多專用執行緒。擷取 worker 接收 Emulator gRPC payload 或診斷用 ADB PNG，驗證幾何／RGB24 後只發布到 `LatestFrame`；三個預先配置的物理 buffer 提供一個邏輯最新 frame，仍被 reader 持有的 slot 不會改寫，耗盡時丟棄新輸入並計數。主執行緒消費最新 frame、執行健康探測及 Win32/D3D11 降頻預覽；預覽不在擷取 callback 內。量測用有界 Journal 另有 writer 執行緒，重要事件無法保存時視為 fault。

```text
Emulator gRPC / diagnostic ADB → capture worker → fixed-slot LatestFrame
                                             ↓
                           observe consumer / optional D3D11 preview
                                             ↓
                         bounded JSONL Journal / offline C++ analyzer

simple target pixels → detector → tracker → line-crossing predictor
                    → ContactScheduler owner → FakeTouch / Fixture gRPC touch
```

`Frame` 保留 `sequence`、`epoch`、`generation`、`geometry_version`、width／height／stride、RGB24、`capture_complete_ns`、`pixels_ready_ns`、`published_ns`，以及分離時域的 `source_sequence`／`source_timestamp_us`。`consume_ns`、辨識完成、預定觸控、注入開始／返回在各自階段記錄。所有 host 時差只用 `HostClock` 的 QPC nanoseconds；來源 Unix／Android 時戳未校準，不能拿來算絕對來源年齡或觸控排程。

擷取量測的程序 CPU／RSS 使用 Win32 `GetProcessTimes`／`GetProcessMemoryInfo`，每筆資源讀取在 QPC `before_ns`／`after_ns` 間取樣。CPU core-equivalent 的分母採起訖讀取中點之差，並保留括號供重算；不把取樣值聲稱為與影格邊界原子同步。每秒 RSS 樣本與兩端 CPU／RSS 原始值均寫進 JSONL。

`ContactScheduler` 由單一 owner 依 monotonic deadline dispatch。gate 與各 plan 的證據分開到期；`now >= deadline` 即撤銷並釋放接觸點。每個 epoch 使用單調 birth ID watermark 防止完成意圖被晚到 revision 復活；已送出的 down 保留 contact ID 與釋放責任。`request_stop()` 對注入臨界區線性化。RPC 返回仍不證明 Android 已執行觸控，因此能力報告需核對 native Touch Fixture v2 的逐指像素事件；未知結果使 input faulted，後續 move 不執行。

2026-09-25 遷移驗收時，observe／Session 不建立遊戲觸控後端，`assist` 明確拒絕，觸控測試僅限前景 `org.pas.touchfixture.cpp` 加可見 schema 雙檢查；後續遊戲 assist 的現行路徑見本文開頭。Native Capture Fixture v2 的四區可見 identity 供來源新鮮度與 tearing 診斷；目標 40／48／57 Hz 與實際可見更新分開記錄。Emulator MMAP 未有 producer 同步證據，仍只可診斷。AVD、ABI、解析度、方向、實際核心／記憶體、Fixture APK hash 及工具鏈記入 manifest／驗收報告。詳細逐項狀態見 [遷移矩陣](CPP_PARITY_MATRIX.md)。

首批 5 vCPU／8 GB AVD 的三批 60 秒 Release 基線採 gRPC RGB888 payload，實際來源 43.5–44.6 Hz、來源跟隨約 99.9%；其可見 freshness 與分布比診斷 ADB PNG 的短測更適合作為目前 observe 擷取基線。RGBA payload 可運作但多出轉換成本；MMAP 在相同 Fixture 上可取得畫面，仍因 producer 同步未證明而限診斷。這是現階段的測試選擇，不代表已證明絕對來源年齡或遊戲端到端延遲。原始窗口、樣本數、尾端分布、觸控與工具鏈限制見 [C++ 驗收紀錄](CPP_ACCEPTANCE_20260925.md)。

## 歷史 Python 架構與研究紀錄

以下舊段落描述凍結在 `legacy/` 的實作及當時驗收，不能作為 C++20 新結果。

> 2026-09-25 使用者新決策：目標架構為全自有 C++20、單程序多專用執行緒，CLI＋Win32／D3D11 預覽。這取代下文 Python process runtime 的未來架構選擇，但不改寫其歷史實作／驗證結果。新契約、遷移範圍與待決項見 [C++ 遷移計畫](CPP_MIGRATION_PLAN.md)。

> 2026-09-25 獨立驗收修正：目前 scheduler 的 freshness 維持、逐 plan dispatch 證據與 completed intent 去重尚有缺口；多指獨立移動／Flick 反向的可見驗證也不足。詳見 [驗收報告](ACCEPTANCE_20260925.md)。下文「已落地」描述實作存在，不代表上述契約已通過。

主程式契約、資源所有權與落地順序見 [完整開發計畫](MAIN_PROGRAM_DEVELOPMENT_PLAN.md)。M0–M2 的已實作範圍在下節；M3 以後及下文帶舊日期的未實作敘述仍屬規劃／歷史基線。

## 2026-09-25 M0–M2 落地狀態

- `RuntimeConfig` 嚴格驗證 JSON profile；observe runtime 只建立原有 `ProcessCaptureSource`／`CaptureWorker`，固定 gRPC payload、RGB888、top-down、容量 1。profile 鎖定擷取尺寸與來源方向；不符時停止並要求新 epoch/profile，避免同尺寸倒轉畫面繼續流入視覺與座標映射。fake source 可離線驗證；任何 MMAP profile 在啟動前被拒絕。run 目錄保存去敏 config、SHA-256、Git dirty／環境 manifest、有限 journal 與摘要。`observe` 沒有輸入後端；`assist` 仍禁用。
- `ContactPlan` 的 note／intent key 與有限 contact ID 分離；`ContactScheduler` 擁有 plans、revision、epoch、gate、證據期限、遲到期限及接觸釋放責任。live `SchedulerOwner` 是唯一修改者與注入者；mailbox、plan 數與步數均有上限，反覆 revision 不累積舊 heap。停止與注入用同一 guard 排序。Windows owner 運作時請求 1 ms timer resolution，停止時還原；這只改善主機量測尾端，不構成硬即時保證。
- `EmulatorGrpcTouch` 使用與擷取獨立的認證 channel；唯一 `PixelCoordinateMap` 將 1280×720 已定向 frame 轉至本機 720×1280 觸控座標（90°）。`sendTouch` 的多指事件與普通 down／move／up 已經隔離的 Android Fixture 可見回饋驗證。RPC 成功仍只代表呼叫返回；RPC 失敗／逾時使後端鎖定，釋放請求保留 `effect_unverified_ids`。重新建立後端前須確認 Fixture 可見零接觸。真實 channel 斷線的一次緊急全 ID 釋放已驗；真實 deadline 逾時未重現。
- `CapabilityReport` 保存 serial、frame／touch 尺寸、旋轉、後端、APK 雜湊、逐能力樣本與失敗數，`matches()` 拒絕指紋不合的 profile；報告明列 `gameplay_enabled=false`。Fixture 量測報告不作自動 arm 的捷徑。
- `DiagnosticPreview` 是最多 10 Hz 的主機視窗，只讀取最新 frame；靜態畫面不增加新 frame 或更新來源時間。來源絕對年齡仍未知，UI 維持 `UNKNOWN`。Fixture 的固定動作只在 `touch-bench` 前景 package 和像素簽名雙重檢查下執行，不接入 Phigros 決策。原有合成 `Scheduler`、CLI 與擷取測試的契約未替換。

本地實測與未完成門檻見 [量測紀錄](MEASUREMENTS.md#主程式-m0m2-觸控-fixture2026-09-25)。下文帶有 2026-09-23／24 日期的「尚無觸控後端」等敘述是當時基線。

## 閉環模組

| 模組 | 輸入 | 輸出 | 責任 |
| --- | --- | --- | --- |
| Capture | 模擬器畫面 | `Frame` | 取得畫面、轉為統一像素格式，只發布最新 frame |
| Perception | `Frame` | `Observation` | 偵測可見目標、Note 與判定線及其畫面座標 |
| Tracker | `Observation` | `Track` | 跨 frame 對應物件，估計位置、速度與不確定度 |
| Predictor | `Track` | `HitIntent` | 估算 Note 到達判定線的時間與觸控位置 |
| Scheduler | `HitIntent` | `TouchCommand` | 安排動作時間、處理過期與衝突動作 |
| Input backend | `TouchCommand` | `TouchReceipt` | 送出 Tap / Hold / Move / Flick / 多點觸控並回報送出狀態 |
| Telemetry | 各模組事件 | 結構化日誌 | 建立同一條時間線，計算延遲與 jitter |

第一個閉環測試可以用簡單目標偵測器取代 Note 追蹤與預測，但仍經過相同的排程器和觸控後端。

2026-09-24 遊戲機制研究提出的下一版需求見 [Phigros 機制研究](PHIGROS_MECHANICS_RESEARCH.md)：預測需分離時間與有效觸控區域，Note ID 與 contact ID 分離，並在預測與排程間增加處理 Hold／Drag／Flick 與接觸衝突的動作規劃層。這是待實作設計，不代表現有單點／Tap 契約已支援上述能力；遊戲機制仍須在觸控與簡單目標閉環驗收後實測。

## 啟動協調與介面狀態

Session controller 是遊戲外圍的控制元件，負責一鍵啟動與安全切換，不從遊戲內部資料推斷操作時機：

1. 選定並記錄唯一的模擬器序號、解析度、方向、縮放與後端設定；確認 ADB 連線及遊戲已安裝。啟動遊戲可使用 Android 的公開啟動介面，但套件資訊只用於啟動與診斷，不作遊戲決策。
2. 先啟動 Telemetry、Capture 與最新 frame 緩衝區，確認持續收到有效畫面後，再要求模擬器開啟遊戲；記錄啟動命令與畫面首次出現的 monotonic 時間戳。啟動命令返回不代表遊戲已載入。
3. 介面辨識器只根據最新 frame 輸出 `UiObservation`：`MENU`、`LOADING`、`PLAYING`、`PAUSED`、`RESULT` 或 `UNKNOWN`，並附來源 frame、辨識完成時間與信心。畫面未穩定或信心不足時維持 `UNKNOWN`，不因固定等待秒數直接進入遊玩狀態。
4. `PLAYING` 連續通過可設定的確認條件後才啟用 Predictor / Scheduler。離開 `PLAYING`、畫面逾時、ADB 中斷或使用者停止時立即停用排程，取消未送出的觸控並釋放仍按住的接觸點。重新進入遊玩畫面需重新確認；Tracker 不沿用上一局的狀態。
5. 第一版在 `MENU` 等非遊玩狀態交由使用者手動操作。若後續加入自動導覽，應由獨立的 UI navigator 根據當前可見按鈕產生觸控意圖，每次操作後重新觀察畫面與確認轉移；不得用固定座標、延遲或預錄序列盲目走完整個選單。UI navigator 與譜面 Predictor 互斥使用觸控後端。

Session controller 的狀態可為 `DISCONNECTED → CAPTURING → LAUNCHING → NAVIGATING → ARMED → PLAYING → RESULT`，任何階段皆可轉至 `STOPPED` 或 `ERROR`。`ARMED` 表示已確認遊玩畫面、準備處理新 frame；是否有實際音符仍由 Perception / Predictor 判斷。所有狀態轉移記錄原因、來源 frame 與 monotonic 時間戳。此流程中的「同步啟動」是可驗證的就緒交接，而非假設兩個程序在同一瞬間啟動。

## 時間與座標

- 主機端時間戳統一使用 `time.monotonic_ns()` 或語義相同的 monotonic clock，以奈秒儲存。wall clock 只用於辨識日誌檔案，不參與時差計算。
- 每個 `Frame` 包含本機遞增序號、RGB24 像素、尺寸、`capture_complete_ns`（本機收到完整 payload）、`pixels_ready_ns`（可供視覺模組使用）和發布前記錄的 `published_ns`。gRPC 另記 `source_sequence`、`stream_generation`、`source_rotation` 與 `source_timestamp_us`（Unix 微秒原值）。未校準時 `produced_ns=None`；絕不可直接用 Unix 時戳與主機 monotonic 相減。ADB 的 `capture_complete_ns` 是 PNG bytes 收齊，`pixels_ready_ns` 是解碼完成。
- 每個 `Observation` / `Track` 保留來源 frame 序號。每個 `HitIntent` 包含來源、預測撞線時間、畫面座標、動作類型與信心或不確定度。
- 每個 `TouchCommand` 至少記錄 `scheduled_ns`、接觸點 ID、畫面座標與動作階段（按下、移動、放開）。`TouchReceipt` 記錄注入呼叫開始與返回時間、結果；呼叫返回不等於畫面已反映觸控。
- UI 狀態切換與啟動命令同樣使用主機 monotonic clock。`UiObservation` 及 Session controller 狀態轉移須可連回來源 frame；任何 `UNKNOWN` 狀態不得送出遊玩或導覽觸控。
- 所有視覺座標以原始 frame 像素座標為基準，透過單一明確變換映射到模擬器觸控座標。旋轉、裁切、縮放與黑邊都要納入變換。

## 最新 frame 規則

### 獨立程序交接（已做 Fixture 實機測試）

`ProcessCaptureSource` 是既有 `CaptureSource`／`CaptureWorker` 的相容 adapter。Windows `spawn` 子程序擁有 gRPC channel、token 讀取、stream 和來源 MMAP；父程序擁有固定共享像素區與子程序生命週期。IPC schema 2 的 256-byte header 含 magic、schema、隨機 generation、發布／來源序號、尺寸、RGB24 長度、row order、rotation、CRC32，以及來源通知、來源快照複製、像素就緒、IPC 發布等 monotonic 時間。實體像素只有一個容量受限的 slot，沒有 FIFO；父子使用有期限的跨程序 lock 防止這一段半寫半讀，父程序在 lock 內複製出 immutable `bytes`。CRC 是損壞偵測，lock 才是本段同步依據。死鎖超時後回報錯誤並回收本次子程序，不接受半張圖。輪詢共享序號最多等待 5 ms，避免子程序崩潰時 Windows Event 內部鎖留在被殺程序而造成停止卡死。

`capture_complete_ns` 在 payload 仍是完整 gRPC bytes 到達；MMAP 診斷路徑代表映射區的**一次複製完成**，不等於已證實一致快照。`notification_received_ns`、`snapshot_copy_started_ns`、`snapshot_copy_complete_ns`、`ipc_published_ns`、`parent_snapshot_complete_ns` 分別保留，`produced_ns` 仍為 `None`。所有主機時戳使用同機 `time.monotonic_ns()`；來源 Unix 微秒保持未映射，不作主機時差。Session 拒絕 MMAP，因為來源端寫入與通知並無 reader acknowledgment、fence 或其他足以排除 tearing／下一張覆寫的證據。程序 heartbeat 只證明 child alive，不刷新舊 frame。程序模式對靜態串流回報 `DEGRADED`，不將未知的新鮮度作為日後觸控資格。

資源 owner：父程序在 spawn 前建立來源 MMAP 私有目錄，子程序在該目錄建立映射檔並持有映射與 gRPC。獨立取消執行緒每 20 ms 檢查停止旗標及 parent alive，呼叫只取消 RPC、不釋放像素的 `cancel()`；擷取返回後才在 finally 解除映射、關閉檔案。父程序 join 後回收目錄中本來源的殘留映射檔，Windows server 尚未解除映射時最多重試 0.75 秒；不掃描其他 task 的檔案。2 秒 cooperative 期限後才 terminate，再以各 1 秒 join/kill 為備援；強制停止標 `FORCED_STOPPED`，清理失敗標 `FAILED` 並報錯。`capture_process_shutdown` 事件保留 forced、child_reaped、exitcode、mmap_cleanup_complete；正常取消不累計 child_failures。父程序異常退出時 child 自行取消、解除映射及清理已知私有目錄。IPC SharedMemory 仍由父程序建立／unlink。上述清理不構成生產端像素同步證據，MMAP Session 門控維持。

程序 benchmark 的影格窗口仍使用 `[measurement_start_ns, measurement_end_ns)`，晚完成的暖機 callback 與 consumer 收到的暖機圖都會更新序號基準。資源讀取與影格邊界不可能由這個 API 原子化：`resource_windows.parent_cpu/child_cpu` 保存每次讀取的 before_ns、after_ns、value，以起訖讀取中點之差估計分母，另附最短／最長可能時長及相對影格邊界的 offset bounds。父 CPU 起點在慢速 child 取樣之後、終點在末次 child 取樣之前，排除這兩次量測成本；child CPU 則明確屬於自己的取樣窗口。`snapshot_start_ns` 是整批開始讀取前、`snapshot_end_ns` 是整批結束後，不再漏算取樣耗時。計數差值也是帶有時間括號的資源快照差，不能聲稱精確落在影格窗口；原始取樣記入 phase 事件供重算，缺測仍為 null。

2026-09-24 本機 Emulator 37.1.11／1280×720 動態 Fixture 實測：process payload 三批完整 60 秒、Session 5 秒均能取得有效畫面；MMAP 診斷使用預設 `width=height=0` 時無影格，明確指定 `1280×720` 後能取圖，CLI 因此要求 MMAP 診斷指定正尺寸。MMAP 三批正式窗口及 payload/thread 對照見 [量測紀錄](MEASUREMENTS.md)。MMAP 子程序 CPU 較低，但來源一致性尚未證明；來源可見更新率在批次間下降，無法據此選定低延遲遊戲擷取後端。正常 Session 仍只准 payload，預設執行方式維持 thread。

Capture 只維護容量為 1 的共享緩衝區。新 frame 覆蓋尚未處理的舊 frame；Perception 取得 frame 後才開始計算。追蹤器可以保留少量歷史狀態以估計運動，但不得要求逐一處理所有過去的 frame。落後或超過有效期限的 `HitIntent` 由 Scheduler 丟棄並記錄原因。

## 計時與量測

至少量測相鄰 payload 到達間隔、相鄰像素就緒間隔、主機駐留時間（消費開始減 `capture_complete_ns`）、辨識耗時、排程誤差（注入開始時間減 `scheduled_ns`）及注入呼叫耗時。主機駐留時間不是來源影格年齡；來源時鐘未映射時來源影格年齡未知。簡單目標測試應另外記錄目標首次被觀察到、首次送出觸控，以及畫面首次觀察到觸控效果的時間；後者包含顯示與再次擷取的延遲，應與注入呼叫耗時分開報告。受控測試畫面可額外提供目標出現的真值時間供離線評估，但執行時決策仍只能使用 pixels。

報告樣本數、p50 / p95 / p99、最大值和誤差分布。此專案的 jitter 指相對預定動作時間的實際送出誤差分散程度，不能只以平均延遲代替。若要宣稱足以應付遊戲判定窗，須先有判定窗與端到端誤差的可比量測。

## 尚待實測的選型

擷取後端需評估輸出幀率、畫面年齡、CPU 使用量與掉幀情況。觸控後端需逐項證明 Tap、長按、連續移動、快速 Flick 以及獨立多指的能力，並量測排程誤差。後端應可替換；選型完成時在本文件補充實測環境、結果與限制。

目前擷取路徑的量測、下一批候選及 Phigros 視覺架構見 [畫面擷取與視覺決策技術研究](CAPTURE_AND_VISION_RESEARCH.md)。候選優先序不是後端選定結論；實際採用仍以同一 AVD 上的功能、像素正確性和尾端延遲量測為準。

## 目前實作與選型狀態（2026-09-23）

- `pas.contracts` 定義 `Frame`（RGB24、`capture_complete_ns` 與可選且不推定的 `produced_ns`）、`Observation`、`Track`、`HitIntent`、`TouchCommand`、`TouchReceipt`。追蹤速度以 frame 的擷取完成時間計算，不以辨識耗時推動物體。`HitIntent` 保留速度、殘差和判定線的文字依據；不確定度是像素量化的啟發式估計，尚未校準為機率區間。
- `CaptureWorker` 由可替換的 `CaptureSource` 持續取圖並寫入 `LatestFrame`，其共享緩衝區容量固定為 1。下游以本機序號取得最新 frame；覆蓋數只計尚未消費的 frame。gRPC 來源在停止時取消阻塞 RPC，再回收非 daemon thread 與 channel。來源跳號、應用覆蓋、消費者跳過與 inactive／invalid 各自記錄。執行記錄採 JSONL 串流，不在有日誌檔時把所有影像或事件累積在記憶體。
- `AdbPngCapture` 保留為診斷路徑：`adb exec-out screencap -p`，支援非交錯 8-bit RGB/RGBA PNG。`capture_complete_ns` 是 PNG bytes 收齊，`pixels_ready_ns` 是解碼完成。歷史 AVD 基線非常慢，原始 JSONL 目前缺失，不能重算其百分位數。
- `EmulatorGrpcCapture` 使用安裝版 proto 的 `streamScreenshot`、一般 gRPC transport 和本機 discovery 檔的端點／權杖。先請求 RGB888，可選 RGBA8888，驗證長度後輸出 RGB24；0×0 inactive 影格不發布。串流斷線或來源序號重置會停止並要求重啟，不回放舊圖。`Image.seq` 與本機 `Frame.sequence` 分離；`stream_generation` 在每個新來源物件中為 0，未在同一 worker 內自動重連。動態 fixture 實測發現接收端暫停 500 ms 後，gRPC 暫時交付 3 張較舊畫面；可選的相對落後上限可丟棄該批舊畫面，但不是絕對來源年齡證明。
- 可選 `max_relative_lag_ms` 只比較串流內來源 Unix 時戳差與主機 monotonic 到達時間差，丟棄接收端停頓後相對落後的上游圖；它是不可信的相對診斷，並非絕對影格年齡。基準預設停用。啟用時固定首張錨點；時間缺失、倒退、明顯前跳、凍結或超過 1 秒無有效發布會明確失敗，要求重建來源，不能重設錨點洗掉積壓。來源序號重置同樣失敗；`stream_generation=0` 只代表這次未重連的來源物件。
- 現有 Tracker／Predictor 仍以 `capture_complete_ns` 作時間基準；它沒有補償來源產生到本機接收的未知延遲。原生 1280×720 動態畫面先前三批約 35.4–35.9 張不同畫面／秒、p95 49–51 ms；後續約 59 Hz Fixture 條件下三批約 58.5–59.3 張不同畫面／秒、p95 29.6–32.5 ms。後一條件符合擷取到達研究門檻，繪製率改變的原因未證實。建立可信來源時鐘映射、量測端到端誤差之前，不把 gRPC 影格直接視為可滿足遊戲判定窗的預測輸入。
- 安裝版 Emulator 37.1.11 的 RGB888／RGBA8888 原始行序經 ADB 同場景截圖驗證為 top-down，雖然 proto 註解宣稱 bottom-up。後端提供 `row_order` 明確設定。像素以已定向的原始 frame 座標輸出，保留 `source_rotation` metadata；尺寸或方向變化記錄事件，不在 Capture 加入遊戲座標。
- `SessionController` 已提供擷取先就緒、再發送公開應用啟動命令的交接；事件式 gRPC 以一張有效圖像就緒。`frame_fresh` 僅在最近有效串流影格未逾期時為真。逾期後獨立 `getScreenshot` 探測返回時，在同一狀態鎖內重新檢查最新串流序號／年齡、worker 錯誤、inactive 與停止狀態。探測期間若已有未逾期的新有效串流影格，恢復 `NAVIGATING`，而非用舊探測結果誤報 `ERROR`；新圖若已逾期則維持 `DEGRADED`。相同靜態 pixels 僅標 `DEGRADED` 並撤銷新鮮度；探測失敗或看到變化但串流仍未送達才轉 `ERROR`。協定無法單靠相同 pixels 證明事件式串流仍會送出新畫面，因此 `DEGRADED` 不可作遊玩門控。inactive 短暫轉 `DEGRADED`，逾期報錯，新有效影格可恢復 `NAVIGATING`。可逆的 AVD 客戶端斷線／重連已驗證；螢幕關閉及 Android Studio 最小化均未產生真實 `0×0` inactive，裝置斷線亦未測。目前沒有遊戲畫面分類器或 `PLAYING` 接手機制，也不注入觸控。
- `capture-bench` 的 `CONNECTING → WARMUP → MEASURING → STOPPING` 以主機 monotonic 標記。首張有效影格前的連線／認證不計入暖機與正式窗口；暖機後 CPU process time 與累積計數在兩端各取快照。正式計數再由 JSONL 事件時間的半開區間 `[measurement_start_ns, measurement_end_ns)` 重算，來源接收、發布與消費各依自身事件時間歸屬，可在邊界不同而不強求相等。首次正式消費前以最後一個暖機影格序號作 skip 基準；晚完成的暖機 callback 也會更新該基準，不把暖機影格算成正式 consumer skips。窗口內無影格停頓保留在牆鐘分母；摘要百分位數是至多最近 100,000 筆的線性插值，完整事件保留在串流 JSONL。來源 Unix metadata 不參與窗口或 CPU 計算。
- `GreenTargetDetector`、`VelocityTracker`、`LineCrossingPredictor` 是簡單目標研究用，從當下 RGB pixels 取得位置，使用至多 8 筆追蹤狀態線性估速，計算判定線交會時間。`Scheduler` 可更新未送出的預測、拒絕過期／重複意圖、取消排程，依 monotonic 截止時間注入並記錄收據。
- `FakeTouchBackend` 驗證獨立接觸點的 down／move／up 狀態機和取消釋放；它不證明 Android 多指能力。`CoordinateTransform` 統一處理裁切、黑邊映射及四種直角旋轉。尚無任何已通過模擬器實測的觸控注入後端；`adb shell input tap` 僅列為單點能力候選，未宣稱支援 Hold／Move／Flick／多指。
- 合成 world 的目標出現及撞線真值只供測試 fixture 和離線摘要。執行中的 detector、tracker、predictor、scheduler 不讀真值。合成閉環與主機排程實際分布、限制見 [量測紀錄](MEASUREMENTS.md)。
