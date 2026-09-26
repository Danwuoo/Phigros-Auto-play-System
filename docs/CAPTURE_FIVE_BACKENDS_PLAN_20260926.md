# 五條擷取路徑開發與集中比較計畫

日期：2026-09-26。狀態：**本計畫已依縮短範圍結案，後續 gRPC 接收層優化亦已完成。** 現行終態見[最後驗收](CAPTURE_FINAL_ACCEPTANCE_20260926.md)：gRPC payload fast／256 KiB 為主用，正式備用暫缺，其他路徑保留候選／比較／淘汰／診斷結論。原規格與下節修訂均保留為歷史，原 96 批未完成、長期性能未驗證，不再自動續跑。Session 保護接線與 M3 簡單目標閉環轉入[路線圖](ROADMAP.md)下一階段。

## 最新使用者修訂：縮短剩餘測試

使用者在開發與多輪量測後表示測試耗時過長，要求改短。本節優先於下文原長測矩陣、readiness、比較報告草稿及其他任務中「不得以短測替代三十分鐘」等舊要求；歷史計畫與原始證據仍保留。

修訂時唯讀盤點：第四輪 `measurements/five_formal_20260926_r4/campaign-results.json` 為 `complete=false`，記錄完成 40 批、其中 3 批非零退出；當時未發現正在執行的 `pas.exe`。這是部分結果，不能改寫為原 96 批全部完成。

1. **不再重啟／補齊原 96 批，也取消剩餘 10 分鐘與主用／備用各 30 分鐘長測。** 已完成的長測照實保留；版本不同的結果分開呈現，不能當成本版驗收。
2. 先一次彙整目前凍結版本已有的正常／壓力／故障資料。足以回答的項目不再測；不能為符合新短窗口而把已完成的長窗口重跑或裁成更好看的片段。
3. 僅對缺乏可比較資料的候選补正常場景：固定目標 48 Hz，每配置最多兩批，**3 秒暖機＋20 秒正式窗口**；相同配對使用相同設定。40／57 Hz 已有結果沿用，未覆盖者列限制，不補完整頻率矩陣。控制／優化只在需要判斷優化收益且資料不足時配對補測。
4. 關鍵壓力只補未回答的 500 ms 接收暫停與慢消費後恢復，每候選每情境最多一批、3 秒暖機＋20 秒正式窗口；優先主用／備用建議候選。事件需在窗口內完成，恢復後留至少 5 秒觀察；若現有命令固定時程無法滿足，使用既有可配置單批 bench，不為此重寫整套 campaign。
5. 主用／備用建議候選各最多 **3 秒暖機＋120 秒**短期穩定性測試；已有同版足夠資料者免測。MMAP 無同步證據已可得 diagnostic-only 結論，不新增長測。
6. 新增實機量測含暖機合計上限 **15 分鐘**。開始前列出確切缺口、最小日程及預估時間；若日程超標，先刪低價值／重複項，保留正常對照與關鍵恢復。從開始新增實測起另設 **20 分鐘 wall-clock 上限**（包括就緒／重連及清理），為正常取消預留時間，不再啟動可能超時的批次；到限收尾並報告未測項，不自動續跑或擴大矩陣。
7. 最多一輪有明確原因的短配對補測，計入同一時間上限。失敗保留並列為限制；本輪不因新異常自動修碼、重建、重跑全矩陣。確實阻止任何有效結論的缺陷另提出具體下一步。
8. 優先使用既有凍結二進位的 `capture-bench` 與可用參數。不假設 `capture-five-campaign` 已提供 warmup／duration 選項；核對 help 後選最小可行方式，不以新增測試框架延長本輪。
9. 新資料獨立保存為 shortened follow-up，預寫日程、版本、設定與中止原因，不能覆寫舊 campaign-plan 或把被取消項算成功。修訂只降低本輪證據範圍，不降低像素一致性、幾何、生命週期及新鮮度硬條件。
10. 交付初步主用／備用建議與剩餘限制，明示短測樣本量與 p99 不確定性，**未完成本版長期穩定性驗證**，`performance_pass` 仍待數值決策。不要為收尾重新做不相關回歸、重新開發遊戲功能或建立自動續跑長測的排程。

任務需同步更新 readiness／比較報告的「目前執行中、必須三十分鐘」等現行狀態，將舊要求標為被本節取代。對已完成樣本先分析再決定是否還需要實測；能用既有資料收尾就直接交付。

## 1. 使用者決策與任務範圍

使用者已確認 C++ 遷移完成，接續研究五條擷取路徑。本次採「先統一交付五條線，再一併測試」，以降低跨開發時期環境差異對比較的影響。

- 指派新 **GPT-6 Sol / xhigh** 任務，直接在 `C:/Users/wurre/Desktop/Phigros-Auto-play-System` 的 **main** 開發。**不建立 worktree，不建立其他開發分支。** 此決策取代舊遷移計畫 D10 的隔離 worktree 要求。
- 自有正式邏輯維持 C++20、單程序、多專用執行緒；第三方原版依賴可保留原語言。
- 正常來源更新率研究範圍改為 **40–59 Hz**。這是正常來源條件，不是每條路徑最低交付率或性能通過門檻；範圍外樣本保留、分類及解釋，不自動刪除。
- 五條線依序開發：gRPC payload 優化、WGC、DXGI Desktop Duplication、scrcpy、Emulator MMAP 可行性收尾。
- 五條線全部達到第 6 節的共同就緒門檻，才執行第 7 節的集中正式測試。集中是同一凍結版本、同一環境與同一 campaign 下串行比較，**不是同時開啟五個擷取後端**。
- 新任務應持續完成開發、共同就緒檢查、可執行的集中測試與選型建議，不在 scaffold 或第一條可取圖時結束。不需在每個工作包後重新請求開工許可。
- 最終性能門檻仍待使用者依新數據決定。未確認前可以完成全部資料蒐集與提出主用／備用建議，不宣稱性能正式通過。
- 簡單移動目標真實觸控閉環是選型後的下一階段；本任務不實作新的 M3、Phigros 辨識、選曲、自動遊玩或 assist。

開始先讀 `AGENTS.md`、`README.md`、`docs/ARCHITECTURE.md`、`docs/ROADMAP.md`、`docs/CPP_MIGRATION_PLAN.md`、`docs/CPP_MIGRATION_AUDIT.md`、`docs/CPP_MERGE_REVIEW_20260925.md` 及本文件。本文件覆蓋舊計畫的任務範圍、40–57 Hz、worktree 與逐候選正式測試順序；其餘 pixels-only、時間、所有權及證據規則沿用。

## 2. 現有基礎與第一個工作包

目前主要程式位於 `src/emulator.cpp`、`src/bench.cpp`、`src/analysis.cpp`、`src/core.cpp`，公開介面位於 `include/pas/`，CLI 位於 `apps/pas/main.cpp`。不是舊計畫示意的多層目錄；開發前應以實際程式核對，不按示意圖盲目重寫。

現有 C++ 能力包括 gRPC payload、ADB PNG 診斷、MMAP 診斷、LatestFrame、有界 Journal、capture-bench／capture-campaign／analyze、Native Fixture 及 CTest。MMAP 尚無生產端一致性保證，不得進 Session。

W0：盤點與保護。

1. 確認 cwd、main、HEAD 與工作目錄狀態；保留本計畫及任何既有修改。不自動 stash、reset、clean、切分支或刪除量測。
2. 核對已有 Frame、CLI、配置、分析 schema 及能力，記錄哪些可直接擴充、哪些需小幅拆分。
3. 確認工具鏈與第三方鎖定版本；以目前 Release 建置及 CTest 建立開發前正確性基準。編譯最多四個並行工作，避免不必要資源飽和。
4. 只做唯讀環境盤點：AVD、ADB、視窗是否嵌在 Android Studio、DPI／螢幕／GPU、SDK、FFmpeg／scrcpy 可用性。不得在此階段變更 AVD 顯示或啟動形式。

歷史性能只能作背景。合併紀錄指出舊 C++ 正常三批暖機只有 2 秒，不能冒充本輪 10 秒暖機規格的候選比較數據。

## 3. W1：共同擷取與量測契約

先整理共同介面，再接入候選；避免每条後端各自實作不可比較的計時器和報表。

### Frame 與生命週期

- 沿用單個邏輯最新 Frame、有界物理 buffer pool 與 reader lease。讀者持有期間禁止改寫；pool 耗盡時依契約丟棄並計數，不無限配置。
- 統一記錄 sequence、epoch、generation、geometry version、尺寸、stride、像素格式、方向、裁切／缩放轉換及來源有效性。
- 共同比較終點是「完整 CPU RGB24 像素可供辨識」。同時保留後端原始像素取得、GPU readback、解碼／轉換及發布的分段成本。
- 來源原格式可留在內部；不得以 GPU texture 到手與另一條完整 CPU RGB24 到手直接排名。ROI／降解析度必須是明確的新 profile。
- Session 有明確後端能力與有效性門控；MMAP diagnostic-only 不可因共用介面而被意外放行。新後端未驗證前僅供 bench／診斷使用。

### 時間與計數

- 所有主機時間使用既有 QPC HostClock；保留 capture_complete、pixels_ready、published、consume 等共同時間點，逐後端明確定義 capture_complete 語義。
- 增加必要的 receive／parse／copy／decode／readback 量測點；API 無法直接觀察的階段標 unknown 或用明示的受控微基準，不偽造拆分精度。
- Windows capture QPC、Android 時間、來源 Unix metadata、codec PTS 分別標示時域／單位。即使 Windows 時間可映射，也不是 Android render time。
- 未校準時絕對來源年齡維持 unknown；相對 lag 與主機駐留不能改名成絕對端到端延遲。
- 分開記錄 callback、來源可見 counter、不同來源畫面、來源跳號、發布丟棄、消費跳過、重複畫面、無新圖空窗及恢復事件。counter 有 wrap／缺失／解码失敗語義。
- Fixture 真值與任何來源統計只供測試／離線分析；執行時遊戲決策僅來自即時 pixels。

### CLI、campaign 與分析

- 擴充現有 capture-bench、capture-campaign、analyze，提供五條路徑的顯式選擇及每條必要參數。ADB PNG 留診斷，不算第六候選。
- CLI 拼法由實作決定並同步 help／文件；不可把本計畫的後端名稱當成已存在的 flag。
- schema 版本化，保留既有日誌重算能力；缺值為 null，有分母與時間窗口，報表可由 raw JSONL 重算。
- campaign 支援預先寫入 manifest 的候選表、環境指紋、執行顺序、warmup／正式窗口、故障情境及每項退出狀態。不可把未執行項當成功。
- 日誌與資源採樣有界並移出 capture callback；所有比較使用相同採樣設定，原始 CPU 取樣前後 QPC brackets 保留。

## 4. W2–W6：五條路徑依序開發

### W2：gRPC payload 優化

- 保留目前可用實作作同一 campaign 的控制配置，避免以舊日期結果當優化前基準。
- 先釐清接收、protobuf、配置、RGB888／RGBA8888 轉換、複製與發布成本，再移除已證實多餘的工作；不以 zero-copy 名稱犧牲資料所有權。
- 維持來源時戳檢查、相對積壓保護、可取消阻塞 RPC、認證去敏、靜態／inactive／fault 語義。
- 正式核心維持 thread。thread／process 只可作有明確假設的可選診斷；如有必要，使用同 C++ 邏輯與相同來源，IPC 成本及所有程序 CPU 合計，且同樣等共同就緒後才正式測量。不把 Python GIL 結論套到 C++，也不以此阻擋五候選交付。

### W3：Windows Graphics Capture

- 使用 Windows 原生 WGC／D3D11 及背景 frame pool；明確選擇目標視窗，不依賴固定桌面座標或脆弱的標題唯一性假設。
- 完成 texture lease、GPU copy／同步、staging readback、row pitch、像素轉換及最新 Frame 發布。
- 實作 ContentSize／resize、DPI、黑邊／工具列裁切、方向、關閉與 device lost 的失效／重建契約。
- 遮擋和最小化是否繼續更新必須實測；取到舊圖不能刷新新鮮度。嵌入視窗是否能直接擷取先確認，不暗中改啟動形式。

### W4：DXGI Desktop Duplication

- 明確選 monitor／adapter，實作 Acquire／Release 生命周期、逾時、access lost、重建與裁切座標變換。
- 涵蓋桌面原點、多螢幕、旋轉、DPI、視窗移動及跨螢幕；不支援的組合要明確拒絕及保存理由。
- desktop dirty rect、游標與其他視窗變動不能視為模擬器更新。以 Fixture 可見 counter 核對目標更新。
- 制定可測的遮擋有效性政策：若无法可靠證明目標區域可用，就撤銷有效性，或將其限定於已驗證的可見前景使用條件；不可把污染画面標作有效遊戲 frame。
- 共用 WGC 可重用的 D3D11 readback／轉換元件，但保留各自來源時間與失效語義。

### W5：scrcpy 影像串流

- 固定官方 scrcpy server tag／commit、檔案 hash 與授權；使用原版第三方 server，自有接收與分析邏輯為 C++。
- 關閉 audio／control，直接接收編碼串流並以固定版本 FFmpeg 解碼。先完成 H.264 單一可重現配置，再考慮其他 codec；不得先擴大組合數。
- 明確記錄 Android encoder、主機 decoder、硬體／軟體路徑、bitrate、尺寸、幀率限制、PTS 與 buffer 策略。raw stream 不表示原始 RGB pixels。
- 封包／parser／decoder／decoded-frame 各層有界。不可任意丟棄 inter-frame 參考封包；解碼後發布最新圖，積壓不可控時採有期限、可驗證的重同步／重連並標示 gap。
- 若用硬體解碼，將 CPU readback 算入共同終點。保留軟體配置的可重現性與失效說明，不預設 AVD 有便宜可用的硬體編碼器。
- Fixture 應能檢查細線、低對比／彩色邊緣、移動位置與壓縮後 counter 可讀性。若須擴充 Fixture，所有後端正式測試都使用同一新版 APK。

### W6：Emulator MMAP 可行性收尾

- 以安裝版 Emulator／proto 及對應官方來源追蹤 producer 的寫入、通知、metadata、buffer 重用、ownership／fence／ack。
- 記錄相符版本、來源定位與證據鏈；找不到相符來源或無同步保證就是具體限制，不以大量成功取圖推定安全。
- 保留現有診斷取圖、四區 identity／tearing 檢查、取消、映射回收與容量限制。reader lock、CRC 或連讀相同只能診斷，不能修復未同步 writer。
- 若沒有足夠一致性證據，交付 diagnostic-only 結論及診斷工具，依安全性安排集中診斷測量；不能進 Session。
- fork／重建整套 Emulator 不在本次默認範圍。

## 5. 開發期間允許的驗證

先統一交付不表示把程式錯誤留到最後才發現。每個工作包可執行編譯、單元／整合測試、合成來源、loopback、格式／幾何與取消驗證，以及必要的短時間實機取圖 smoke。

這些輸出須標 `development_smoke` 或同等類別，不算正式 campaign、不發布性能排名、不據此提前選型。短 smoke 不應演變成只替先完成候選跑長測。開發期間可以用 profiler 確認成本來源，所有性能結論仍待凍結後重測。

有意義的回歸包括：無效尺寸／stride、lease 期間改寫防護、取消與回收、pool／decoder 壓力、timestamp reset、幾何變更後旧 frame 失效、counter 解析／壓縮失敗、分析重算與半開窗口邊界。沿用 GoogleTest／CTest；需要 GPU／視窗／AVD 的項目單列，不用 fake 宣稱實機通過。

## 6. W7：共同就緒門檻與版本凍結

正式集中測試開始前，五條線每條必須交付：

1. 可建置、可執行的後端／診斷入口，或有版本證據及重現方法的不可行結論。
2. 完整配置與 help、資料／時間契約、所有權與停止語義、能力和限制。
3. 開發期間的必要正確性驗證，以及集中測試適用情境表。

不可用候選可用 `unsupported-with-evidence` 或 `failed-with-reproduction` 關閉研究項目；MMAP 可用 `diagnostic-only`。不可用空殼、「之後再做」、無證據猜測代替交付。`waiting-user-decision` 是未完成阻礙，需列出具體待決事項，不假稱五條就緒。

共同交付還包括：固定的 Fixture APK、統一 bench／analyzer、可重現 campaign 配置、測試矩陣、preflight／環境指紋與診斷圖像檢查。

凍結 source revision／dirty diff hash、Release 二進位 hash、依賴、APK hash、配置與分析版本。正常比较前停止開發建置與高負載分析，確認相關 workload 已穩定。修正影響比較的程式或配置後，建立新 campaign revision，保留舊資料並重跑受影響的比較組，不能混成單一排名。

## 7. W8：集中正式測試

### 環境與頻率

- 同一主機、AVD、Android／Emulator 版本、GPU mode、解析度、方向、縮放、視窗形式與固定 Capture Fixture。核對 5 vCPU／8192 MiB 及 guest 實際值。
- 記錄 Windows、驅動、電源、monitor refresh／DPI、背景負載與所有後端配置；不為不同後端偷偷調整源端工作量。
- 先沿用 Fixture 40／48／57 Hz 目標場景；59 Hz 只有 Fixture 明確支援並完成共同凍結才加。沒有必要為正常範圍上界強迫實際更新達到 59 Hz。
- 目標頻率與實際來源可見更新率分別報告；設定 57 或 59 不代表實際達成。低於 40 的樣本保留，尤其要檢查是否由擷取／編碼負擔造成。
- 無預覽為主比較，預覽另設配對場景。各後端輸出同一目標畫面區域與 CPU 像素尺寸，縮放不可逆差異要揭露。

### 預先固定順序與樣本

- 每個可行候選、每個正常頻率場景至少三批，每批 **10 秒暖機＋60 秒正式窗口**；正式窗口完整、從 READY 後計算暖機。
- 用固定 seed／輪換順序分散先後效應，campaign 開始前把完整日程寫入 manifest，串行執行。gRPC 原配置與優化配置也納入相同凍結版本的配對比較。
- 設定每批前的一致穩定條件及有上限的等待；未達条件保留失敗，不無限重試挑數字。
- 40–59 Hz 是有效研究範圍，不足以證明兩批來源條件相同。來源差異大時保留端到端負載影響，並限制「純擷取成本排名」解讀。
- 首輪前預先允許至多一輪補充配對重測，必須寫明觸發原因；不得任意反覆挑最好結果。

### 壓力、故障與長測

- 共通：50 ms 慢消費者、100 ms 慢消費後恢復、500 ms 接收暫停、來源靜態、停止／反覆啟動、適用的斷線／重連、受控 CPU／記憶體負載。GPU 負載獨立成場景。
- 固定慢消費／暫停發生的階段、時刻和持續時間；各後端暫停位置差異明列。計算停止積壓後恢復到最新可見 counter 的時間，不只看恢復後瞬間交付率。
- WGC／DXGI：resize、DPI／縮放、方向、遮擋、最小化、移動／跨螢幕及可重現的 capture device／access loss。
- scrcpy：buffer／解碼追趕、串流中斷、重同步與邊緣位置品質；MMAP：metadata／四區 identity、tearing 診斷與回收。
- 每個通過基本正確性的可行候選做 10 分鐘穩定性；依预定規則選出的主用／備用建議候選再做 30 分鐘。僅診斷後端的長測結果獨立列出，不給正式資格。
- 未能真實重現的 fault 明列，fake／loopback 只證明對應程式行為。短取圖成功不能代替這些情境。

### 指標與報告

- 每批列環境、有效時長、樣本數、p50／p95／p99／最大值、不同來源圖率、重複／丟棄／跳過比例、最長無新圖空窗、恢復時間與失敗數。
- 分段成本、主機駐留和已知時域的延遲分開報告；來源絕對年齡 unknown 必須保留。來源率由可見 counter 跨度推估時揭露限制。
- 資源同時看 PAS、可識別的 Emulator 相關程序與主機 GPU／編解碼引擎。記錄採樣窗口、CPU core-equivalent／百分比定義、RSS 峰值／趋势。取得不到的指標標 missing，不憑印象補值。
- 若成本從 PAS 移到 Emulator，或來源被拖慢，必須顯示在整體結果。少量樣本的 p99 不宣稱穩定尾端保證。
- 品質報告包括幾何正確、方向／色彩、counter 讀取、一致性異常與細線／移動邊緣位置偏差，不只展示「看起來正常」的截圖。

## 8. W9：研究結論、選型與下一階段

每條路徑最終狀態只能是 `implemented-and-tested`、`diagnostic-only`、`unsupported-with-evidence`、`failed-with-reproduction` 或明確未完成的 `waiting-user-decision`。每項都有證據路徑、適用條件及限制。

選型依序檢查：

1. 像素／幾何／生命週期正確、不持續積壓、不把未知／過期圖當新圖、可取消和回收、結果可重算。
2. 正常與負載下尾端、空窗、恢復、來源跟隨、CPU／GPU 成本和影像品質。
3. 主用的使用條件，以及備用能補足哪些失效情境；備用不必是平均耗時第二名。

報告提出具體建議門檻及其樣本依據，交使用者決定；確認前 `performance_pass` 維持 null／pending。若只有一條符合硬條件，明說備用尚未合格，不勉強選兩條。提出建議不等於開啟遊戲觸控。

交付清單：

- 五候選實作／診斷工具／明確淘汰證據，以及共同 CLI、配置、測試與分析。
- 候選 readiness／能力矩陣、正式測試日程與環境 manifest、所有成功及失敗原始資料索引及 hash。
- 集中比較 Markdown 報告，含逐批分布、異常解釋、限制、建議主用／備用與待確認門檻。
- 更新 README、ARCHITECTURE、ROADMAP 與第三方授權／版本紀錄；現行說明使用 40–59 Hz，歷史報告保留當時條件。
- 後續簡單目標閉環交接：選定 Frame 契約、座標變換、有效性撤銷、像素觀察→預測→排程→注入→再次觀察的量測點。該閉環另階段執行。

## 9. 開發操作與需要使用者決策的邊界

直接 main 開發已獲授權，不再詢問是否建立分支或 worktree。使用者既有修改與 ignored measurements 必須保留；可分工作包做範圍明確的本機提交，不 push、不覆蓋未知改動。此交接文件是已授權輸入，不應當成衝突刪除。

五條線與共同測試工具可先持續開發。只有具體影響既定環境或範圍時才提出決策：WGC／DXGI 所需 Emulator 啟動形式變更、GPU／顯示等固定設定變更、正式單程序架構變更、MMAP Emulator 重建，以及新性能數值門檻。先把可獨立完成的實作、測試命令、配置差異與可逆方案備妥，再詢問具體選項；不因可自行處理的類別命名、模組拆分或日常錯誤修正而停工。

若 AVD 未連線或目前環境不適合集中測試，完成五條線可獨立的開發及離線驗證、交付可直接執行的 campaign，列出真正缺少的條件。不得把未執行實測寫成成功，也不得用此理由只交第一條線。

## 10. 技術參考

- [Windows Graphics Capture](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture)
- [DXGI Desktop Duplication](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api)
- [scrcpy 官方開發文件](https://github.com/Genymobile/scrcpy/blob/master/doc/develop.md)：此為入口，實作時改用已鎖定 tag／commit 的文件及 server。
- [gRPC C++](https://grpc.io/docs/languages/cpp/)
- 本機 `proto/emulator_controller.proto`、既有 C++ 原始碼、遷移／合併驗收紀錄；不可拿不相符版本的網路 proto 覆寫。
