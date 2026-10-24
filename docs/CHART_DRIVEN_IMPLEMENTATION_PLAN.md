# 譜面驅動自動演奏實作計畫

日期：2026-10-23

工作分支：`feature/chart-driven-multigame`

起點：`3be959e` 加上目前本機的建置修正、資產盤點與觀測錄製工具。

研究參考：phisap `stable@8ac9a67d584ed2c34faccebd9ad35194331ec9d5`。

本文件是接下來的實作順序與驗收依據；`TECHNICAL_DIRECTION.md` 保留架構研究，`VALIDATION.md` 記錄已完成驗證。下列「待做」均不是已實作能力，不改寫舊分數或資格。

**執行進度（2026-10-24）**：**Dlyrotz IN13 AP 已於 live011 達成一次：1,000,000分、584 Perfect／零 Good／Bad／Miss、Max Combo584、100.00%。** [獨立結算與完整audit](M7_WHOLE_SONG_EXPERIMENT.md)核對584 contacts／5,669events、peak4、failure null、最後kernel zero；[Build009](../../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json)通過25/25 CTest、default4/4。M0／M1完成，其他里程碑已有各自列出的離線與裝置證據；單次AP不自動完成跨輪穩定性、來源像素age、獨立input lead、其他曲目／遊戲或生產gate資格。[音訊線](M4_AUDIO_SYNC.md)仍依使用者要求暫緩，原始失敗與未測範圍保留。

## 1. 目標與優先順序

### 使用者新增目標：重現 Dlyrotz IN13 AP（live011 已達成一次）

使用者明確新增 **「重現 Dlyrotz IN13 AP」**，先前「第一版不設定 AP」的範圍由此取代；音訊線仍暫緩。離開期間曾授權代理操作電腦，現在已回來並要求直接完整譜面測試，選曲／開始採互動式 manual-mode。既有選曲／Play→HUD 確認→結算 latch 生命週期保留；UI 開始不等於取得歌曲時間或輸入資格。

目前核心已是譜面驅動：精確原生chart→IR→line pose→contact compiler→PAS scheduler，視覺負責首音觸發與UI監督。Live011以Flick lead30 ms／travel60 ms／位移60 reference pixels／末端停留24 ms、Tap／Drag32 ms、phase−80 ms取得指定曲目一次AP；480個非Flick contacts與010相同，全譜重新編譯為5,669events。這個成功組合已保存身份與原始結果，尚未量得獨立input lead或證明跨輪重現率。

AP 驗收只認指定 **Phigros 4.0.1／Dlyrotz IN13** 的實際結算：**584 Perfect、0 Good／Bad／Miss**。需保存來源譜面／compiled plan SHA、binary、固定視窗／幾何／backend capacity、Gameplay Offset、歌曲時間映射與 input lead 設定、逐次執行／停止／釋放紀錄及原始結果證據，讓同一設定可重跑。FakeTouch、CTest、完整 note 數或口頭 AP 都不能標為目標完成。

每次執行預先固定有限輪數與 watchdog／wall budget；新的全曲 CLI 為一輪、round watchdog **180 s**、session **240 s**，舊只讀入口的 600 s 預設另列。調參／重試用新的編號輸出目錄並明列有限批次上限。失敗、unknown／失鎖與釋放異常的原始證據保留；有界停止與生產 qualification gate 不因 AP 目標被略過。AP已由live011確認；成功設定與live009／010漏判歷史一併封存，後續跨輪重現率驗證仍須新編號、有界測試；首音視覺觸發後固定 arrival-origin／rate 1／−80 ms phase，仍明示為未取得資格的假設。

### 使用者最新調整：音訊線暫緩

使用者於互動測試後明確要求「先不做音訊線，繼續把其他線開發完成和 M0–M7」。因此停止新增音訊研究、演算法修改與錄製；已完成的兩輪 WASAPI／video-only 資料和未通過的結果保留。M4 的音訊資格維持暫緩，不能因停止開發而標為完成。

M3 全首校正候選、M5 整首 FakeTouch、M6 source build／所測 fixture／STOP 釋放，以及兩次 M7 只讀一輪起訖已具備；live011另完成指定曲目一次AP；有界歌曲時鐘與獨立遊戲輸入校正資格仍分開列管。M4 的非音訊狀態 supervisor 繼續整合。若 M7 使用手動起點或視覺校正，須獨立標明其時間證據與校正範圍，不繼承尚未取得的音訊資格，也不得以 polling tick 重造新 evidence。

### 使用者最新調整：開始與結束沿用 manual-mode

沿用 `origin/main@2d479e17fa1566c089dce019cbb11eb883ba47fd` 的手動選曲／Play、待機、HUD 確認、結算 latch、人工下一輪與 Esc／Ctrl+C 停止語義。開始需三個不同送達、未過期的 HUD 觀測且跨至少 20 ms；結束需三個不同送達的結算觀測且跨至少 60 ms。送達／stream 序號不證明不同渲染影格，來源像素 age 仍未取得資格，見 [capture-clock 界線](M4_CAPTURE_CLOCK.md)。暫停、黑畫面與暫時失去 HUD 只暫停處理，不宣稱歌曲完成；watchdog 到期屬中止。

以獨立 `ManualLifecycle` 接上新畫面狀態分類器，先提供 `chart_manual shadow ... --observe-only` 的有界只讀入口；不把舊 `GameObserver` 或純視覺 `GamePlanOwner` 搬回。舊的六個結算標籤門檻與新的本機模板各自保存驗證身份，不能混稱同一分類器。預設一輪、單輪 180 秒、整個觀測 600 秒，多輪須明確指定且上限 20 輪。

這個入口已完整 build／link，非音訊整合先後 **14/14→15/15→16/16 CTest**，各 binary 與日誌分開保存。兩次實際只讀 run 各一開始／一確認結算；002 先保留舊 result latch，人工 retry 後才開新回合。非同步 foreground 檢查的 002 consumer skips 為 0、直接 PLAYING→STARTING 回退 1 次，001 則為 718／97；這不是受控 benchmark，未知 UI 與最大間隔仍保留。[M7 報告](M7_MANUAL_MODE.md) 含兩輪原件／獨立摘要與 [build002 archive](../../local-assets/phigros-4.0.1/m7-manual-build-002/manifest.json)。之後的 watchdog 逾期覆寫邊界修正在 build003 離線驗證，沒有新增第三首裝置 run。同程序多輪、來源像素 age、連續 playing 及實際遊戲輸入仍待取得資格。

manual-mode 決定一輪何時開始／結束，不產生歌曲時間、SyncLease 或輸入資格。畫面 arrival-only 時鐘明示僅作 UI 生命週期；恢復／下一輪接回輸入前仍需新 epoch、實際同步觀測與先前接點的釋放確認。詳見 [manual-mode 接續](M7_MANUAL_MODE.md)。

第一個目標是讓 **現存 `phigros` AVD／Phigros 4.0.1 的 Dlyrotz IN13**，能由精確譜面產生動作，持續觀測歌曲時間，再經既有 PAS 核心執行。先完成一首可追溯、可停止、可重跑的端到端流程，再擴充曲目。

主要決策來源改為原生譜面；音訊估計歌曲位置，視覺確認播放狀態、幾何與局部錨點。錄影作為核對與回歸資料，不把每幀辨識所有音符當作必要的即時主線。

世界計畫保留獨立 importer／adapter 的位置，待第一首 Phigros 流程穩定後再實作。共用歌曲時鐘、接點生命週期、排程與輸入，兩款遊戲各自保留 note 語義與幾何。

AP結果條件已由live011的584 Perfect／零Good／Bad／Miss與完整身份、事件、釋放證據達成一次。固定設定已封存供重跑；同步誤差、獨立input lead與跨輪重現率仍未取得資格，不把AP結果當作這些條件的替代證據。

## 2. 現況與可使用資產

| 項目 | 已確認狀態 | 接下來的用途 |
| --- | --- | --- |
| PAS clock、ContactScheduler、觸控抽象、PixelCoordinateMap | 原來源保留；Windows core／ADB library 已重建 | 保留為執行核心 |
| `include/rhythm/chart_memory.hpp` | 已有 resolved-contact memory、ClockMap、SyncLease 的離線契約 | 作為編譯後記憶與同步介面起點；不是原生譜面格式 |
| 現存 AVD／遊戲 | `phigros` 可開機；Phigros 4.0.1／versionCode 157；獨立 fixture 已觀測四指 | 第一個實際目標；遊戲輸入時序／input lead 仍待校正，fixture 未量最大容量 |
| 安裝資產 | 已取得 base／UnityDataAssetPack／UnityStreamingAssetsPack／arm64 split 的 SHA | 盤點精確譜面與音源的入口；尚未證明能解析其內容 |
| 新錄製 `dlyrotz-in13-20261023-02` | 開場至結算；4,789 張可解碼畫面；48 kHz stereo、179.456 秒音訊 | 離線對照、視覺錨點與音訊研究 |
| 新錄製的限制 | H264 有損，最大影格 PTS 間距 143 ms；WAV 含整份錄製 | 不能作毫秒命中精度 gold，也不是已裁好的歌曲音源 |
| 舊 7,722 PNG／12 段選片 | Git 留有文件、來源指紋及工具；使用者確認舊本機原件刪除 | 保留歷史發現；不再等待原件，不把舊 ordinal 對到新影格 |
| patched C++ gRPC、原生 importer、同步器、PAS bridge | gRPC source build／runtime 與所測 fixture 路徑通過；importer／bridge 與整首 FakeTouch 已完成；真實同步尚未取得資格 | 分別保留 build、離線與裝置驗證範圍 |

本機影音與 `session.json` 在 repo 外的同層 `../recordings/dlyrotz-in13-20261023-02/`，分析資料在 `../recordings/dlyrotz-in13-20261023-02-analysis/`。這些是本機相對位置，不保證其他 clone 存在；檔案身份以 session／analysis 的 hash 為準。

## 3. 從 phisap 採納的設計

| 採納 | 在本專案中的落地方式 |
| --- | --- |
| 安裝資產 → 原生 JSON 譜面 | 先釐清 4.0.1 分包結構，取得一份可驗證的 chart；不假設舊 APK／OBB 路徑仍成立 |
| 在判定時刻直接計算判定線 pose | 將時間、線動畫與裝置座標分開，建立獨立 evaluator |
| Note 不等於 contact | 建立接點編譯器，處理 Drag／Flick 重用與 Tap／Hold 干擾 |
| 先離線規劃整首再快取 | 保存不可變歌曲計畫；執行時只滾動提交未來短段 |
| 避免逐 note 執行 ADB shell tap | 使用既有 persistent backend；ADB 留給設定、盤點與明確的選單操作 |

不直接沿用 phisap 的整曲 wall-clock busy loop、固定 10 ms 補償、硬編碼 10 指、未帶來源身份的 `.ans.json`，或中途停止缺少 release-all 的行為。其 `chart.offset` 被讀入但未參與排程，必須在新 importer 補正；1 ms 操作採樣也不代表實際 1 ms 輸入精度。

phisap 的手寫 scrcpy 控制器依賴 2.0；目前錄製工具使用完整配對的 5.0.1 release，PAS 則有自己的 patched gRPC。三條路徑分開驗證，不混用 server／協定或繼承對方的測試結果。若移植程式碼，記錄固定來源 commit 與修改；不搬用其專用圖標。

## 4. 目標資料流與契約

```text
本機安裝資產／明確版本的原生 JSON
  → Asset manifest + chart/audio identity
  → Phigros importer → Semantic Chart IR
  → Line pose／hit region evaluator
  → Contact compiler → 不可變歌曲計畫
  → Rolling planner／PAS bridge → ContactScheduler → TouchBackend
                         ↑
實際音訊觀測＋視覺狀態 → Song clock／SyncLease／Supervisor
                         └→ 失鎖、暫停、重開、幾何變更：撤銷與 release-all
```

資料分成三層，避免把譜面身份與即時觀測混在一起：

1. **Semantic Chart IR**：來源 note／line ID（若有）、類型、above/below、判定與結束時間、線動畫、來源版本與 hash。來源只有陣列時，以 chart hash＋line index＋side＋note index 產生穩定 ID，保留 JSON 來源定位，不冒稱原生 ID。保留原生時間單位及轉換記錄，尚未分配 pointer。
2. **編譯後歌曲計畫**：contact ID、DOWN／MOVE／UP、歌曲時間、正規化座標／軌跡、note→contact 對應、接點峰值、編譯器與幾何版本。由現有 `Memory` 契約擴充，必要變更須有遷移測試。
3. **執行上下文**：song↔host monotonic 映射、相位、rate、uncertainty、觀測時間、租約期限、epoch、geometry version、backend 能力與輸入提前量。可變且有期限，不寫回原譜面。

`chart offset` 在 importer 正規化一次；音訊相位與 input lead 在校正／執行層分開處理。所有模型明列時間原點：譜面 tick、歌曲時間、影片 PTS、音訊 sample offset、裝置時鐘與主機 monotonic 不互相冒充。

## 5. 分階段工作與驗收

### M0：取得並固定第一份真實譜面

**工作**：唯讀盤點目前 4.0.1 的 base／split APK 及實際資產位置，辨識 catalog、bundle、TextAsset 或其他格式。以 phisap 作線索，依實際格式選擇資產 reader，不預先重寫整套 Unity parser。保留直接載入已取得 JSON 的入口。

**交付**：本機 `asset-report.json`、Dlyrotz IN13 原生 chart、對應音源或有明確來源的參考音訊，以及來源 manifest。列出包 SHA、資產路徑、遊戲版本、chart dialect/version、曲目／難度、chart hash、audio hash。

**驗收**：能重複定位同一份 chart，核對 note／line／類型數量與時間範圍。錄影結算的 584 個判定項目只作交叉檢查，不硬改 parser 湊數，也不能單憑數量相同確認身份。

**未通過時**：輸出具體缺失或不支援格式；改用同版本、來源明確的 JSON 輸入。錄影標註可作另一種記憶來源，但須標為 inferred，不能冒稱已取得精確原生譜面。

### M1：原生時間與語義 importer

**工作**：只接受已核對的 dialect/version。明訂每線時間基準、BPM、offset 單位／正負、Hold 結束時間與同時事件；保留 speed／floor／opacity 等原始語義，但不靠 approach speed 推估判定時間。未知版本、note type、非有限數值與不合理範圍明確拒絕。

**交付**：Semantic Chart IR、`chart-inspect` 離線入口、時間正規化報告、synthetic golden fixtures 與本機真實 chart 摘要。

**驗收**：首音、跨線同時音、正負 offset、非整數 tick／BPM、Hold 結尾、空譜面與錯誤輸入均有測例。固定解析測例對獨立期望值比對，納秒取整誤差不超過 1 ns；這是數值轉換門檻，不是裝置命中精度。

### M2：判定線 pose 與裝置幾何

**工作**：求任意歌曲時間的線位置／旋轉與 note 判定位置，再由獨立 geometry mapping 轉成輸入座標。覆蓋事件邊界、座標原點、角度方向、viewport、旋轉及 letterbox。phisap 的 1280×720／positionX×72 只在確認相同 dialect 後採用。

**交付**：pose evaluator、離線 hit-region／軌跡匯出、錄影疊圖預覽與幾何 manifest。

**驗收**：靜止／移動／旋轉線、線上不同 x、above/below、長 Hold、缺事件、零長事件、螢幕外位置、NaN 與邊界像素有 golden cases；合成座標誤差 ≤1e-6 normalized。新錄影只作可見幾何核對，143 ms gap 內的精確命中時間維持未知。螢幕外落點不能直接套用「法向無限判定」假設。

### M3：Note → contact 編譯

**工作**：先以可追溯、可診斷的 ownership 策略實作 Tap／Drag／Hold／Flick，再增加接點重用。每個候選動作保留來源 note；測 Drag／Flick 的新 DOWN 是否會干擾鄰近 Tap／Hold。先確認單接點持續性，再評估多音共享；不直接照搬 algo2 的 Hold 合併假設。

**交付**：整首不可變接點計畫、事件瀏覽／dry-run、note→contact 對應、峰值接點數與衝突／拒絕報告、帶身份的快取。

**驗收**：生命週期閉合、無重複 DOWN／失主 MOVE／UP、同時事件順序固定；Hold 不任意換 pointer，Flick 有可觀察位移。參數化測試 2／5／10 點容量，超限拒絕；這些是編譯器測例，不代表裝置已支援。長軌跡壓縮須有誤差上界與 note 關鍵時刻保留，不能用無限 1 ms MOVE 撐過限制。

### M4：音訊 shadow 同步與狀態 supervisor

**工作**：先從錄音與已知偏移的合成資料驗證特徵／相關比對，再接即時音訊。初版優先驗證 Windows WASAPI 的 sample／device position／QPC；裝置端音訊若作主來源，必須證明其 timestamp 語義、與 host clock 的映射及不確定度。gRPC server-read timestamp 只能作粗觀測。

**交付**：參考音訊衍生 manifest、即時 audio adapter、SongClock／SyncLease producer、無輸入的 shadow CLI，以及時鐘／phase／rate／信心時間線。native song hash 與錄製 WAV hash 分開；若從錄影裁參考段，保存原 SHA、sample 範圍與對歌曲時間的錨點。

**驗收**：測靜音、音量 0、重複樂段、錯音源、暫停／恢復、重開／seek、漂移、資料延遲與時鐘跳變。指定負例集不得假鎖；已知延遲／漂移測例回報 phase error p50／p95／p99／max、鎖定時間與失鎖時間。真實誤差未知時標 unknown，不以視訊 gap 或 callback arrival 製造精度結論。

進入 live 前，須將可接受 uncertainty、觀測 age、lost-lock timeout 與延遲裕量填成明確設定並附測量依據；它們不是通用 Perfect 判定窗口。視覺只提供 playing／paused／result、幾何及可驗證錨點，不阻塞排程 owner。

### M5：Rolling planner 與 PAS bridge，先用 FakeTouch

**工作**：把歌曲計畫分成有限 lookahead，映射為 PAS intent／revision／prefix。依實際同步與幾何觀測提交；歌曲記憶可長期保存，執行證據須持續有效。

必須處理現有預設 **2 秒 admission horizon、150 ms evidence age、16 steps／plan、64 plans**。長 Hold／Flick 需要已執行前綴保留、revision／compaction 或有回歸測試的介面調整；不得每 tick 設 `evidence_ns=now`，也不得直接把有效期放大成整首歌。

**交付**：bridge、bounded queue／接點佔用報告、fake clock／FakeTouch 的整首 replay，以及 supervisor 的撤銷／release-all 路徑。

**驗收**：時鐘修正只影響未執行後綴；跨多次提交的 Hold 保留 ownership；重開建立新 epoch，微小 phase 修正不任意取消現有接點。租約到期、錯曲、幾何變更、過期事件、backend error 均停止派送並釋放。測同時 UP／MOVE／DOWN、late policy、批次派送能力與上限，不能由離線排序推定 backend 原子性。

M4 與 M5 的獨立程式可平行開發；兩者契約與測試都通過後才接真輸入。

### M6：Backend 重建與輸入校正

**工作**：完整重建現有 patched C++ gRPC backend，確認與 emulator 服務版本／ABI 的契約。建立明確選擇 serial／AVD 的 live 入口、人工停止路徑、裝置能力 fixture、release-all 與限時結束。

**交付**：build／runtime 指紋、實測容量、接點連續性／同時性／釋放報告、input lead 與 jitter 測量。後續若增加 scrcpy touch backend，獨立固定 client/server 與協定，不替換既有 gRPC 的測試身份。

**驗收**：先在有界接點 fixture 測單指與多指，不直接從整首歌推測 capacity。host dispatch、可取得的 Android input observation、RPC receipt 與遊戲判定分開記錄；只有 RTT 時不得宣稱已量到遊戲輸入延遲。每條停止路徑都須核對實際接點已釋放，unknown／failure 不當作成功。

**目前結果**：[M6 裝置報告](M6_DEVICE_VALIDATION.md) 已核對 SINGLE002、capacity002（四指）、batch001、cancel001（release-all 而非 ACTION_CANCEL）、client transport-loss001 及 STOP002 cleanup。STOP002 在已按下兩指時中止並完整釋放，正常 expected-path 分析維持 failed；缺 native、verifier 誤拒與黑畫面的舊失敗均保留。新 probe build 15/15 CTest／default 4/4 通過。callback clock join 只給條件區間；遊戲 input lead／jitter、Ctrl+C 裝置路徑、其他停止時機與最大容量仍待驗；實際單 Tap 判定的獨立校正見 M7，不由 fixture 資格推定。

### M7：單首有限 live 與擴充

**工作**：shadow 與開場輸入紀錄保留；依使用者最新要求，接續直接做完整 Dlyrotz IN13 譜面實驗。每一輪固定 chart／audio identity／layout／build／校正身份與時間上限，audio capture 仍停用。停止條件包含失鎖、過期、容量不足、幾何改變、transport 失敗及使用者停止；不自動無限 retry。

**交付**：每類 note 的可核對結果、分數／判定數、同步與派送誤差、接點遺失／重複／釋放結果、完整起訖與失敗分類。

**驗收**：先證明事件與身份正確、全流程停止可靠，再比較分數；單次高分、合成 dry-run 或正常 RPC 不替代端到端驗證。保留失敗紀錄；若來源無逐 note 判定，只能回報可觀察範圍。

**歷史開場 Tap 結果**：[校正試驗](M7_FIRST_TAP_CALIBRATION.md) 的 live002／003 在 DOWN 前失效停止，未放寬 age。live004（0 ms）實際結算 **0 Perfect／1 Good Late／583 Miss**，live005 與 live007（−80 ms）各 **1 Perfect／0 Good／0 Bad／583 Miss**，同設定 live006 為 **0 Perfect／1 Good Late／583 Miss**。三 Tap live001 完成三組 DOWN／UP 與最後 kernel zero，但沒有最終結算原件，判定未驗證。這些紀錄不證明穩定相位、分離 input lead 或 AP。

**目前有界範圍**：[chart_experiment 全曲入口](M7_WHOLE_SONG_EXPERIMENT.md) 目前保留完整 584／5669／4 與 6 個 D250 投影候選（Build001–008為4911steps）；目前逐 contact 保留完整未執行 tail，總預算 20,000 steps，保留真 UP、2 s first-step admission horizon、100 ms lookahead 與 20 ms lateness；舊 Build001–005 的 32-step prefix 屬歷史實作。原生私譜 FakeTouch **19,650 checks** 與 Build001 **22/22 CTest** 為歷史離線結果；首音觸發 [Build003](../../local-assets/phigros-4.0.1/m7-whole-song-build-003/manifest.json) 有 **23/23 CTest、default 4/4**。Live001–003 均在輸入前停止；live004 在 READY 前因 trigger constructor 拒絕、零輸入。Live005 首音觸發後實際送出 **53 DOWN／64 MOVE／51 UP、168 筆成功 RPC**，於 `immutable_prefix_revision_rejected` 停止；runtime 另釋放 IDs 0／1、最後 pinned kernel zero。其[後續結果觀察](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/result-observed-001.json)為 **53 Perfect／0 Good／0 Bad／531 Miss、score 90,753、max combo 53、accuracy 09.08%**，原失敗 manifest 不改。

**歷史中止（live007）**：[Live007](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/manifest.json) 使用 HUD Build005，226 幀 fallback 讓本輪越過先前 UI 空窗；196 DOWN／1,514 MOVE／195 UP 後因 trajectory coverage 過期停止。結算為 **196 Perfect／388 Miss**，release known、最後 kernel zero；原件保留，不代表目前仍停在此進度。

**最新完整全曲（live011，AP）**：[獨立sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result-observed-001.json)確認 **584 Perfect／零Good／Bad／Miss、1,000,000分、MaxCombo584、100.00%**。完整584／5669已送完、manual-mode確認結算、failure null、release known、最後kernel zero；原manifest的unreviewed／AP_verified=false不回寫，後續sidecar明示本輪AP_verified=true。Live008／009／010的581P3Miss／579P5Miss／580P4Miss原件保留。

**成功設定與後續資格**：[Build009](../../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json)已通過 **25/25 CTest、default4/4** 並完成live011 AP；封存固定chart、compiled fingerprint、binary、設定、5,669receipts與結果SHA，供模型交接與後續重跑。一次AP不證明Flick各參數因果或跨輪穩定；音訊仍暫緩。HUD denied-state優先、−80 ms phase、100 ms anchor、500 ms UI watchdog、20 ms send lateness保持，生產qualification gate未放寬。歷史[主機等待量測](../../local-assets/phigros-4.0.1/m7-host-wait-001/REPORT.md)與[非零RPC私譜封存](../../local-assets/phigros-4.0.1/m7-whole-song-latency-replay-001/README.md)繼續保留原範圍。

之後增加具代表性的旋轉線、長 Hold、Flick／Drag 密集與多指曲目，重跑跨曲目回歸。世界計畫另取得真實 fixture，實作 lane／slide／flick 語義，不直接套用 Phigros 的幾何或接點合併規則。

## 6. 預計程式與產物位置

下列為建議拆分，名稱可依實作調整；不是已存在檔案清單。

| 路徑／模組 | 職責 |
| --- | --- |
| `tools/inspect_phigros_assets.py` | 唯讀資產盤點與來源 manifest；reader 依 M0 實際格式選定 |
| `include/rhythm/chart.hpp` | Semantic Chart IR、時間／身份欄位 |
| `src/rhythm/phigros_import.cpp`、`phigros_pose.cpp` | 已核對 dialect 的 importer 與 pose evaluator |
| `src/rhythm/contact_compiler.cpp` | note→contact、ownership／重用與衝突檢查 |
| `src/rhythm/audio_sync.cpp` | 特徵對齊、SongClock、uncertainty 與失鎖 |
| `src/rhythm/pas_bridge.cpp` | lookahead、intent／prefix revision、租約與撤銷 |
| `apps/chart_inspect/`、`apps/chart_shadow/` | 明確分開離線檢視、無輸入 shadow 與 opt-in live 入口 |
| `tests/fixtures/` | 可提交的合成 golden；真實 chart／audio 用本機 manifest 引用 |
| 忽略的 `local/`、`charts/private/`、`recordings/`、`calibration/local/` | 私有 chart、音訊、錄影、快取與本機校正 |

新增 JSON／音訊套件透過明確 CMake option／vcpkg feature 或獨立工具環境提供。現有 default offline build 不因資產工具而啟動 ADB、emulator、網路下載或輸入。保留的 PAS 原碼若確需修改，逐檔記錄來源與回歸，不能悄悄覆蓋 provenance。

## 7. 驗證分層與工作順序

驗證各自報告，不能互相替代：

- **來源**：實際 bytes、身份與解包結果可重現。
- **時間／幾何**：獨立期望值的 golden；畫面疊圖只作輔助。
- **接點**：生命週期、干擾、容量、軌跡與 ownership。
- **同步**：已知偏移測例、實際 clock 對應、不確定度與假鎖負例。
- **排程／輸入**：FakeTouch、實際 backend fixture、age／lateness／停止結果。
- **遊戲**：有界單首可觀察結果，最後才是分數比較。

本計畫用依賴與驗收點排順序，不先承諾日期：曲中精確 clock、實際多指／釋放與端到端判定仍待驗。每個里程碑結束更新 `VALIDATION.md`，只把已有證據的項目標成完成。

**目前 M4–M7 的驗收界線**：Live011完成指定Phigros4.0.1／Dlyrotz IN13一次AP，完整584／5669與最後釋放均可核對。跨輪重現率、其他曲目／遊戲、來源像素age、精確歌曲時鐘、獨立input lead／jitter及生產live gate仍未取得資格；音訊依使用者要求暫緩。固定arrival-origin／rate1／−80 ms phase與Gameplay Offset0 ms保留為本次設定，不冒充獨立延遲量測。

## 8. 參考與既有文件

- [phisap 資產提取入口](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/main.py#L25-L79)、[Chart parser](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/chart.py)、[時間／判定線座標](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/judge_line.py#L118-L160)。
- [phisap algo1](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/algo/algo1.py)、[algo2](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/algo/algo2.py)、[接點規劃說明](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/QA.md)。
- [phisap 手動／延時同步](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/main.py#L361-L479)、[control／audio=false](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/control.py#L43-L79)、[版本與使用說明](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/README.md)。
- 本專案：[技術方向](TECHNICAL_DIRECTION.md)、[驗證紀錄](VALIDATION.md)、[來源抽取](EXTRACTION.json)、[歷史資產接續](ASSET_RECOVERY.md)、[新錄製流程](RECORDING.md)。
