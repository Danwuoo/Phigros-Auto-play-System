# Chapter Legacy 全曲 IN 解鎖與 zero miss 端到端驗收研究

研究日期：2026-10-05（UTC；歷史報告多使用 Asia/Taipei）。唯讀來源基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本文件及同目錄 `LEGACY_ACCEPTANCE_MANIFEST_TEMPLATE.json` 為研究交付，不是開發、裝置操作、解鎖、實戰或新增輪次授權。

## 1. 先說結論

1. **全 Chapter Legacy 的現行分母與目前逐曲 IN 解鎖仍未知；不能報完成百分比。** Repo 的 20 個歷史曲名可作盤點候選，不能直接升格為當前完整清單。
2. tracked 總帳可完整解析：89 份所選完整結算＝70 HD＋19 IN、33 sessions；所有 89 份均有 Miss。IN 僅涉及 Dlyrotz 11 輪、光 8 輪，完整 IN Miss=0 證據為 0。這不表示其他 18 曲鎖定，也不表示歷史所有嘗試都收齊。
3. 目標是「同一凍結通用策略、當前已核章節每曲完整 IN 結算 Miss=0」，照報 Perfect／Good／Bad／Miss、分數及失敗。**不加 AP、HD AP 或全部 HD S 的前置條件。** 先看 IN 是否已開；只有實際仍鎖定者才安排所需解鎖。
4. C36h tint1 是 behavioural／experimental baseline，Dlyrotz IN13 最近保存值為 795950、496/11/0/77；main50 是 comparison／donor、live0。歷史 Windows 完整 BVI harness 尚未編譯／完整冷 suite 未跑；**本次建置研究已原封編譯 BVI core，GCC 新 smoke probe 28/28 assertions 通過**，但無 owner／live 資格，X12 成本 gate 仍未過。見同目錄 `BUILD_TEST_AUDIT.md:7-11,36-43`；核心 smoke 不等於 zero miss 改善。
5. 本輪研究**不必索取整包 gitignored 資料**。若要驗目前解鎖，最小輸入是去識別的遊戲版本、全章節清單與逐曲 IN 畫面；若要核對歷史結果原圖，先取一輪的四份小證據即可。詳 §4。

依據：`docs/status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md:5-15,23-31,39-50`；`docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md:19-50,111-117`。

## 2. 本輪確實查了甚麼，仍缺甚麼

### 2.1 來源與查核層級

已讀 `AGENTS.md`、README、ARCHITECTURE、ROADMAP、跨曲研究及最新 10/5 現況。Checkout 無 `.agents/skills`。本驗收研究分支以 shell／jq 作文件維護級解析與交叉引用檢查；未跑產品／候選／測試／重播。平行建置分支的新 Linux 核心 smoke 見上節，兩種範圍分開。全案未啟動遊戲／模擬器／裝置、未讀遊戲內部或存檔、未訓練、未付費；本分支未修改正式 source／既有 tests／Git 分支或提交。

完整解析 `docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json`，本輪 SHA256：`610e7f613f2a5ce1e75ef05938a6207b067bfe9026bbba690ec63aa99ba0fe4a`。文件內 `head=f83c7ea...` 是 10/4 快照，不是本輪 checkout HEAD。10/5 搬移保留 JSON 原始 bytes，因此 JSON 中的 `docs/FOO.md` 是歷史路徑，須用 `docs/status/DOCUMENT_RELOCATION_20261005.json` 解析，不回寫原封存字串（`docs/status/DOCUMENT_RELOCATION_20261005.md:3-7`）。

本輪確認：

- 89 個 `runs[].id` 唯一；每個 `session_ref` 都可連到 33 個 sessions 之一。
- 20 曲的 `hd_run_ids`／`in_run_ids` 均可解析，逐曲數量與 runs 相符。
- 89 輪均為 `result_confirmed`；每輪 `P+G+B+M=judgments`；Miss=0 的 run 為 0。
- 20 曲的 `current_membership`、`current_unlock_state` 仍 unknown；`chapter_listing_verified=false`、分母 null。
- `inventory_exhaustive_all_repository=false`；5 筆 excluded／incomplete 與一筆已失 raw 的歷史結果另外保存。

**未做：** 此雲端 checkout 沒有 `measurements/` 或 `out/`。JSON 的 `exists:true`、先前 hash 檢查成功，只能解讀為歷史稽核紀錄，不能冒稱本輪持有／重新驗過圖、events、DLL、binary 或 freeze。`.gitignore:5-6` 明列這兩個根。本輪只親自重 hash tracked JSON；數字仍是歷史像素轉錄，未產生新的逐 Note 真值。

### 2.2 版本不能以 observer／planner 號碼拼接

| 身分 | source build／dirty（歷史 session） | binary SHA256 | 可用結論 |
|---|---|---|---|
| HD9 33/13 | `5b3375dc57734da72a048dfdb6054001be9ce868`／true | `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76` | 舊 19 曲 HD 比較組，Credits 有完整重跑 |
| A36 36/18 | `cd0ec437f495ee73e92a1adaccd097a86eeb91ac`／false | `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601` | 新 19 曲 HD＋兩首 IN；後續相同 binary 仍分 session／profile |
| 歷史 main37/19 | `baf3d4fcbaac56ab085e19b9fba8a5ef6615d3bc`／true | `d9fa50034d1935b87ee4a1ab1f73ec762fa63b0e0f3835ffe4778f3e0fd8e780` | strategy 名稱37/19，manifest 數字36/18；兩者原樣留存 |
| C36h tint1 37/19 | `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`／true | `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9` | behavioural baseline，不能與上列同號合併；Dlyrotz M77 |
| main50 50/27/11 | 報告當時 HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`；此列不補猜完整 dirty closure | `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb` | comparison／donor，live0，不能拿此 binary 接續宣稱通過 |

來源：總帳 JSON `sessions`（例如 `:5556-5604,7819-7821,7929-7931,8039-8041`），基準文件 `:9-13`。C36h profile SHA256 為 `4201679acc8ded39a394852d5f8d7f11fd62e736c32f10aa77657fc8fdbef18b`。其餘 IN 每輪的完整 binary／build／dirty／profile 身分在配套機讀模板的 `historical_in_runs`；這些是歷史索引，不是本輪執行紀錄。

Commit 不是 dirty binary 的完整重建配方。未來 acceptance freeze 至少綁完整 source closure／dirty patch、exe、載入 DLL、編譯工具鏈、profile、能力／裝置指紋及遊戲版本；不能只填 37/19 或「main」。同一策略的參數也不能按歌名切換（`AGENTS.md:21-35`；`src/manual_session.cpp:79-103`）。

### 2.3 現有結果能說的程度

兩批相同 19 曲 HD：Miss 815→818，非 Perfect 957→860。平均分或 Perfect 變好，沒有證明 zero miss 目標改善；混乱、FULL AUTO SHOOTER、Pixel Rebelz 的 Miss 分別增加54、32、18。每曲通常只有一輪、時間／負載／版本共同變化，不是單機制因果比較。Credits 舊組採最後完整 B14，不是挑最佳，A2仍保留。來源：`MAIN_LEGACY_HD_COMPARISON_20260928.md:43-69`（位於 `docs/catalog/02-game-results/`）。

完整 19 份 IN 轉錄如下；M列全部非零。session尾碼／round僅為易讀縮寫，正規化 repo 相對 ID、圖 SHA、session、profile／binary 在機讀檔，各列 source_json_pointer 可追溯原 JSON。各列都是獨立實戰，**不能取不同版本最低 Miss 拼成驗收**。

| 曲目 | variant | session尾碼／round | 分數 | P/G/B/M |
|---|---|---|---:|---|
| Dlyrotz IN13 | A36 | 108176133899800／4 | 634461 | 395/5/6/178 |
| 光 IN12 | A36 | 108176133899800／7 | 799603 | 443/7/0/67 |
| 光 IN12 | 歷史main37/19 | 140156097343000／3 | 756170 | 427/4/3/83 |
| Dlyrotz IN13 | A36 | 3293220942400／4 | 627680 | 388/9/7/180 |
| 光 IN12 | A36 | 3293220942400／5 | 732292 | 405/7/1/104 |
| 光 IN12 | C36b | 1950777953300／1 | 749458 | 410/22/0/85 |
| 光 IN12 | A36 | 2214451448600／1 | 676983 | 358/40/0/119 |
| 光 IN12 | C36c | 2781923266400／1 | 661122 | 350/40/2/125 |
| 光 IN12 | C36d | 3285261084800／1 | 647476 | 343/37/1/136 |
| 光 IN12 | C36c-restart | 3906621440800／1 | 716344 | 386/30/1/100 |
| Dlyrotz IN13 | C36e | 5321553975700／1 | 534289 | 330/5/6/243 |
| Dlyrotz IN13 | A36 | 6112164289400／1 | 747192 | 461/16/5/102 |
| Dlyrotz IN13 | A36 | 8695119652800／1 | 549623 | 338/8/7/231 |
| Dlyrotz IN13 | C36f | 9522730305100／1 | 783793 | 488/11/4/81 |
| Dlyrotz IN13 | C36f | 10677475166400／1 | 777560 | 489/7/7/81 |
| Dlyrotz IN13 | A36 | 11517576270600／1 | 665206 | 413/8/5/158 |
| Dlyrotz IN13 | C36g | 16106368637700／1 | 796284 | 491/18/4/71 |
| Dlyrotz IN13 | C36g-rec1 | 22885039263800／1 | 778185 | 485/16/6/77 |
| Dlyrotz IN13 | C36h tint1 | 16174738262800／1 | 795950 | 496/11/0/77 |

原 JSON IN欄位行：1772、1931、2850、3840、3893、4164、4217、4270、4323、4376、4859、4912、4965、5018、5071、5124、5177、5230、5283。Dlyrotz 歷史總判定584，光517，只作已存同難度一致性檢查，**不是未來遊戲版本譜面的預填答案**。

## 3. 候選逐曲分母與公開背景

### 3.1 候選清單：20個 historical labels，全部 unverified

下表順序沿歷史 JSON，並非保證當前 UI 排序。每列的「目前章節成員」「目前 IN 解鎖」「目前 IN 等級」均為 unknown；每列產品驗收均為 not_accepted。歷史已有 IN13／IN12不回填為目前等級。原始來源：總帳 JSON `songs:34-527`；基準文件 `:25-50`。

| 候選曲名（保留歷史字串） | 保存HD／IN | 曾可玩IN證據 | 當前需核對 |
|---|---:|---|---|
| Eradication Catastrophe | 7／0 | unknown | chapter＋IN |
| Credits | 5／0 | unknown | chapter＋IN；勿選AT |
| Dlyrotz | 9／11 | 歷史完整IN結算 | chapter＋目前IN |
| Engine x Start!! (melody mix) | 2／0 | unknown | chapter＋IN；勿選舊Legacy譜 |
| 光 | 3／8 | 歷史完整IN結算 | chapter＋目前IN |
| Winter ↑ cube ↓ | 2／0 | unknown | title空格／箭頭＋chapter＋IN |
| 混乱-Confusion | 3／0 | unknown | chapter＋IN |
| Cipher | 2／0 | unknown | 畫面花體完整名稱／別名＋IN |
| FULL AUTO SHOOTER | 3／0 | unknown | 與公開FULi名稱的對應＋IN |
| HumaN | 2／0 | unknown | chapter＋IN |
| [PRAW] | 2／0 | unknown | chapter＋IN |
| Cereris | 2／0 | unknown | chapter＋IN |
| Pixel Rebelz | 3／0 | unknown | chapter＋IN |
| Non-Melodic Ragez (MUG Edit) | 2／0 | unknown | chapter＋IN |
| Sultan Rage | 2／0 | unknown | chapter＋IN |
| Class Memories | 2／0 | unknown | chapter＋IN |
| -SURREALISM- | 3／0 | unknown | chapter＋IN |
| Bonus Time | 2／0 | unknown | chapter＋IN |
| ENERGY SYNERGY MATRIX | 2／0 | unknown | chapter＋IN；歷史HD等級末位仍null |
| Glaciaxion | 12／0 | unknown | chapter＋IN；不併入歷史19HD比較 |

如果實查多了歌，新增候選；少了歌，保存差異及證據，不能為湊20刪除／補齊。無法判定的標題保持待核對，不能 fuzzy join 後自動算已通過。

### 3.2 公開資料只提供解鎖假說，不認證使用者存檔

2026-10-05 以公開網頁檢索讀取、非使用者本機瀏覽器。日本社群 Wiki 的章節表自標 ver3.19.4，Legacy 普通常設表列20曲，說明1.5.0合併前三章、3.0.0併第四章。它另列期間限定內容及 Legacy 舊譜欄。這可交叉檢查遺漏／別名，**不證明使用者安裝版本等於3.19.4，也不替代當前清單**。[社群章節表](https://wikiwiki.jp/phigros/%E6%A5%BD%E6%9B%B2%E4%B8%80%E8%A6%A7#M_L)

同站玩法頁指出 Legacy 曲目開放與 IN 難度開放不同；通常同曲 HD 達 S（920000）可開 IN，RKS達11亦可能使多數IN直接開放。一般「前一曲IN A」規則不應未经核實套到 Legacy；先讀當前鎖定提示。來源為社群整理、並非官方保證，版本未獨立鎖定。[玩法與解鎖](https://wikiwiki.jp/phigros/howtoplay#song_unlock)

因此最短解鎖路徑是：**盤點 → 已開者直接列 IN 測試 → 鎖定者依畫面要求補HD或其他明示條件 → 重查IN可選**。不為達RKS11擴大到別章刷分，不先要求全HD AP，不讀／改存檔繞解鎖。帳號、雲端同步狀態、RKS、安裝來源／版本與是否重置，repo都沒有當前證據；不用要求登入憑證或帳號私資。

Chapter Legacy 是章節名稱，與某些曲目的「Legacy 舊譜」是不同範圍。預設驗收一般 IN，AT／SP／隱藏舊譜另列非目標；若使用者想包含它們，須另立清单及驗收，不默默擴分母。公開表的 FULi 與repo的 FULL只記待核別名，不回寫歷史轉錄。

## 4. gitignored 檔案：分三層要，避免整包索取

| 用途 | 本輪是否必需 | 最小資料 | 可支持／不能支持 |
|---|---|---|---|
| 本輪路線與驗收設計 | 不需補檔 | 已有tracked docs／JSON／source | 可完成本報告；不能聲稱本輪看過原圖 |
| 目前章節／解鎖驗證 | 要做該驗證時才需 | 遊戲版本畫面1張、Legacy全清單首到尾連續重疊圖、每曲IN可選或鎖定條件圖 | 證明該時點該本機狀態；舊roster／網路表不能替代 |
| 歷史結果原圖小樣本稽核 | 要核原圖時才需 | 同輪result.png、summary.json、round manifest.json、session manifest.json | 可核P/G/B/M、圖與manifest綁定；4份不足證明全歷史無漏attempt |
| 完整端到端／完整attempt分母 | 驗收執行時需 | 上列＋session rounds.jsonl／summary、該輪events分段及SHA索引、停止／釋放紀錄、預先attempt ledger | 核開始→結算→安全收尾與遺漏／aborted；無事件不能證明全過程 |
| exact候選來源／binary重現 | 建置／live放行時需，由工程分支具體選取 | freeze／compiled-source／DLL manifest與相應小範圍source，必要時唯一候選exe／DLL | commit／版本號不夠；不先要整個out |
| 全7722PNG、所有measurement、訓練權重 | 本輪不需 | 只有定位具體pixels缺口時再要指定片段與index | 大包不會自動證明現在解鎖或全曲成功 |

最小歷史樣本建議 C36h Dlyrotz IN13。根目錄：

`measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/`

- `manifest.json`
- `round-1/manifest.json`
- `round-1/summary.json`
- `round-1/result.png`

原總帳期望 result SHA256：`d85240867fa6386538076fb556c2f995f82e1e887d7675d64ab789a69c4271af`；summary：`56e2f733ace6c47cbe2f4e2a218ec59318c8efe0058fa8ab3a6f34458c6f4446`（JSON `:5277-5331`）。需要 lifecycle時加 `rounds.jsonl`、session `summary.json`、`round-1/events-0.jsonl`、`events-1.jsonl`。進一步查來源才取 `acceptance36h-01/candidate36h-tint1-freeze.json`、`profile.json`；不用整包 runtime作結果閱讀的前置。

`measurements/game-assist/2026-09-29-chapter-legacy-full/roster.json` 可核對歷史候選来源，但它本來就未驗清單，**不是目前解鎖的缺失答案**。所有要求均可用去識別副本；若裁切／遮罩，保存原檔本地，另記衍生檔SHA與遮罩範圍，不能把衍生SHA冒作原歷史SHA。不要提供遊戲存檔、帳號token、密碼、整個AVD、APK譜面assets或全磁碟。

## 5. 最小畫面盤點與狀態更新

本節是下一次獲准後的盤點規格；本輪未操作裝置。

1. **固定時點與版本**：記時間／時區、裝置自訂代號、遊戲畫面可見版本及模式；不需帳號名稱。若遊戲未顯版本，可在後續獲准時取OS公開安裝版本資訊，仍不讀遊戲內部。無法讀到保持null。
2. **章節邊界**：保留 Chapter Legacy 標題、第一項與最後一項；連續向下的圖保留至少一個重複鄰項，避免滑動漏曲。記篩選／排序／搜尋狀態。章節總數只作交叉檢查，不取代逐項清單。
3. **逐曲IN**：每首同一張圖盡量同時含曲名、IN標示／等級、Play可用狀態或鎖定文字。僅看到HD分數、IN文字灰色與否不夠判斷時列unknown，不從顏色猜。已可明確選IN不必重打HD。
4. **解鎖後再核**：對仍鎖定者先抄當前條件；後續获准完成所需HD後，保存HD結果及「IN已可選」的after畫面。HD達標只記解鎖動作完成，未見after則解鎖仍未驗。
5. **對帳**：建立 `observed_display_title → historical_label` 的人工核對表。每个候選有畫面引用，額外／缺失／別名歧義單列；確定無漏項才把 `chapter_listing_verified` 改true，N改實際數字。
6. **狀態失效條件**：換安裝／遊戲版本、切換或重置存檔、重裝、恢復／同步後重新盘點。用使用者自訂的非識別 `local_save_context_label` 區別狀態，無需蒐集私有save ID。單張選曲畫面證明當時可選，不保證跨重啟保存；若需驗持久性，於另行允許重啟後抽查，再據差異決定是否全查。

## 6. 解鎖、回歸與IN的建議順序

### 6.1 先決條件與不必要前置

目前停止開發／續派仍有效；舊文件中的「本輪已授權」不是本輪啟動依據。未來進入新candidate live前要：可信build／冷驗 → 接正式owner後完整行為／安全回歸 → exact候選有效runtime成本 → freeze／裝置preflight／容量／停止條件 → 使用者與總控在當時範圍放行。BVI不能借pending-only成本或fake-touch結果背書（`LEGACY_DEVELOPMENT_INVENTORY_AND_R2D_ACCEPTANCE_20261005.md:34-48`；最新狀態優先）。

不把模型訓練、所有193 skips精確歸因、大型人工標註平台、全曲HD AP加成新前置。工程工作包通過和遊戲收益各自驗。

### 6.2 有界執行順序提案

| 階段 | 最小動作／輸出 | 停點 |
|---|---|---|
| A 先核當前UI | §5完整清單與解鎖表，可先讀使用者提供截圖 | 無清單N就不宣稱全曲；不因缺圖啟動未授權裝置 |
| B exact候選pilot | 沿既有X12形式：baseline A/A同譜兩輪，再候選與baseline交錯A/B各最多兩輪，最多6 attempts；目前仍blocked | A/A不穩，不作因果改善結論；輸入／釋放unknown等立即停止 |
| C 回歸哨兵 | 放行擴大範圍後：先Dlyrotz／光已有IN脈絡；HD可用Glaciaxion及歷史退步三曲作小型通用回歸 | 這是風險排序，不是按歌曲特調，也不取代全部IN |
| D 必要解鎖 | 只處理當前仍鎖的曲；明示HD要求才打HD，達條件後重驗IN可選 | 既有HD歷史S不直接簽當前解鎖；不無限刷至成功 |
| E 全曲單輪掃描 | 候選／參數固定、依已核UI清單逐曲完整IN；保存每次attempt | 每曲至少一個完整M0才有基礎產品達成；任一曲缺證就不叫全曲完成 |
| F 穩定性重複 | 建議每曲預先固定連續3個驗證attempt、至少2個session；相同freeze，三次均M0才標repeatability_pass | 不從很多次挑3次；失敗不能擦除，重新驗需另立明確batch |

來源：`C36H_FORWARD_EXECUTION_PLAN_20261003.md:23,38,58,96`；總帳 `:111-117`。§C-D-E-F需要新有界計畫與容量，不把X12的6輪擴成全章節額度。人工仍選曲／按Play；程式只用即時pixels及有界近期追蹤接手，不以本清單或歌名控制觸控（`README.md:36-48`；`src/manual_session.cpp:92-103,209`）。

HD回歸順序的理由：Glaciaxion已有12份HD，適合檢查基本生命週期；混乱／FULL／Pixel三曲曾退步，可抓平均分掩蓋的回歸。這些都是已見開發家族，不是未知曲泛化驗收。Dlyrotz與光相同binary的歷史IN波動很大，先A/A重要，不以單輪新高分認證改善。

### 6.3 重複數與容量：明確的建議，不改既定產品標準

- **基礎達成**仍是每曲至少一次完整IN M0，所有曲須同一最終freeze。3次重複是建議的穩定性層級，不是偷偷加AP或把現行目標改為3次。
- 建議在看結果前選定連續3次驗證批次；三次有一次M>0、aborted或證據unknown，該曲穩定性未過。連續指該曲在已凍結批次中的3個連續attempt，不能從更長序列事後截取成功尾段。完整M0仍可保留為「已觀察成功」，不能把失敗排出attempt率。
- 若策略／profile有行為改動，新的最終版本必須再覆蓋全N曲；不能用舊版某歌M0抵新freeze。純文件更動要有可核binary/profile未變證明。
- 不主張3/3意味任意遊玩皆不會Miss；它只是最小重現性訊號。N未知時輪次寫作N或3N。若實查剛好N=20，單輪20、三輪60只是情境估算，尚不含解鎖、pilot、失敗及重試。
- 歷史X12保守每attempt journal預留1GiB physical，加standby／metadata／result；6輪總6,530,531,328 bytes且原future根12GiB、另free5GiB。本數不包含full recording／pixel clips，不能直接用於新BVI或60輪；全章節應按新候選實測與硬上限逐批重算，不索取／建立無界根。

## 7. 每個完整IN結果的證據包

### 7.1 最低可接受鏈

1. **範圍身分**：已核chapter snapshot、當前遊戲版本／模式、曲名與IN難度畫面；不得把HD／AT／Legacy舊譜／Challenge／Mirror混到預定normal-IN scope。模式不同另列。
2. **執行身分**：attempt ID、run/session/round、exact candidate freeze ID、source/build/dirty closure、exe／DLL／profile／capability SHA、geometry／rotation／mapping、OS／emulator／遊戲版本、診斷開關、QPC domain、開始及終止時間。unknown source absolute age保留null。
3. **完整生命週期**：正向PLAYING→確認RESULT→釋放→封存；UI不明、空白、watchdog或user stop不是結算。不能只看score有值即complete。
4. **同源結果圖與轉錄**：保存原result.png SHA；從該圖離線讀標題／IN／score／P/G/B/M／max combo／可見ACC，無法辨識記null且unknown。自動OCR僅proposal；至少一次獨立圖面覆核，M=0或標題歧義建議第二次覆核。
5. **完整事件與核驗**：session manifest、round manifest、summary、rounds index、全部event_segments路徑／bytes／SHA、首尾／source frame引用；核summary绑定manifest及result。保存release requested/failed/unknown與runtime/input/archive fault，不用RPC success宣稱遊戲接受了每個音符。
6. **review分層**：`result_rule_pass`只回答完整IN且M0；`evidence_complete`回答可追溯性；`safety_gate_pass`回答已知故障／釋放；`accepted_for_product`要求上述、scope及freeze均合格。不以單一pass旗標混合。

原程式已寫round manifest、event分段、result與summary SHA、rounds索引，且明寫result數字須像素覆核：`src/session_archive.cpp:73-95`。中止保存partial／無result：`:108-111`。session原有executable／config／source／preflight欄位：`src/manual_session.cpp:79-103`；完成／釋放：`:195-206`；lifecycle／receipt：`:187-193,220-276`。這些是現行main source能力，不代表C36h或未建BVI全有相同欄位，實際包需依原binary契約查缺。

**完整結果證據包不等於每輪都必須全錄PNG。** 現有same-stream結算＋完整事件／身分足以建立aggregate成果鏈；逐note根因研究才追加有界圖段。反之，僅一張結算圖可以證明畫面上的M值，但不能獨自認證該binary、從頭到尾自動遊玩或沒有其他嘗試。

### 7.2 P/G/B/M與zero miss的精確用法

P=Perfect、G=Good、B=Bad、M=Miss。產品規則只要求完整IN的M=0，G或B不設為0門檻，但必須照報；不能把「zero miss」寫成AP或FC。社群玩法資料區分Good持續combo、Bad打斷combo及AP全Perfect，因此M0但B>0不自動等於FC。[判定說明](https://wikiwiki.jp/phigros/howtoplay#x2b2c6fa)

保存 `N_judgments=P+G+B+M`；未知欄位不能當0。分數、ACC、max combo是補充，不由分數倒推Miss，不用V／S徽章取代當次明確M值。舊best數字也不是本輪結果。對零Miss主張，全曲每首逐列核M，比總平均更重要。

## 8. 失敗、重試、aborted與unknown不消失

attempt ledger應在launch前建立唯一ID，记錄planned→launched→終態；planned但未launch不算遊戲attempt，仍作工程阻塞。每次手動Play／重新開始若實際launch了新遊玩就新增attempt，即使程序沒有成功建立round。abort不能借用上輪圖。

終態至少區分 `result_confirmed`、`aborted_user`、`aborted_runtime`、`aborted_input`、`aborted_archive`、`unknown_outcome`、`not_launched`。完成但M>0記結果失敗；圖缺失／數字不辨識記evidence_unknown；不是一律塞M=0或M=曲目總note數。retry另列 `retry_of_attempt_id`、原因、是否换版本／環境；舊attempt不可覆蓋。

本次原JSON保留的非完整項：

- `manual-session-54180603650300/round-7`：aborted、曲名unknown，不能指認為-SURREALISM-。
- `manual-session-99031291652700`：standby_only，不算完成歌曲。
- `manual-session-18848730136100`：aborted_no_result。
- `manual-session-12501552442800`：deleted_by_user_reset，不能重建不存在的成績。
- `.../sessions/manual-session-3371898928300`：disconnected_before_recorded_start。
- 更早Glaciaxion `cpp-observe-17905107612608769`：HD868880、369/2/0/22為historical_only，raw已失，不升格可重算。

來源：總帳 JSON `excluded_or_incomplete`、`historical_only`；`HD9_LEGACY_HD_RESULTS_20260928.md:36-47`。這份名單也不自稱窮盡全部歷史嘗試。

每批報：planned、launched、complete、aborted、unknown、同版有效M0、每曲成功／全部launched分母、重試次數。`complete_result_success_rate`與`all_attempt_success_rate`分開，章節覆蓋率僅在N已核後計算，unknown留在章節未完成數中。保留全部P/G/B/M分布、最差曲與失敗原因，不能只報成功子集的延遲；時序n/p50/p95/p99/max及jitter仍由對應工程分支量測。

## 9. 全曲驗收門檻與可驗收實驗

### 9.1 全章節簽收條件

- `chapter_listing_verified=true`，N為當前明確normal-IN目標清單，包含所有已核成員，別名與缺項無未解歧義。
- 每曲當前IN已核可選；至少一個同一最終freeze的完整IN結果满足M=0且P/G/B/M可讀。
- 每份計入結果的scope／source／binary／profile／環境／same-stream圖／完整生命周期與釋放可追溯，已知安全失敗無未處理。
- 所有attempt含failed／aborted／unknown／retry均登錄；不能跨binary／profile拼最佳、不能因鎖定或難度高移出N。
- 分開簽「基礎全曲M0達成」與「建議3次／曲穩定性通過」；後者未做不能冒稱可靠性已證。
- 若遊戲版本／存檔切換或候選行為改變，重新確認適用範圍；對新版本不沿用舊版全曲通過。

### 9.2 最小能改變決策的實驗提案

| 實驗 | 可驗收輸入／輸出 | 通過／否證與停止 | 依賴／價值 |
|---|---|---|---|
| E0 證據鏈小樣本 | §4四份C36h檔＋歷史SHA；圖面逐欄轉錄與binding報告 | hash相符且數字一致才確認該歷史樣本；不符保留原值與差異 | 只讀原檔；低成本驗資料轉移／讀圖能力，不新增live |
| E1 分母與解鎖盤點 | §5一套完整去識別畫面；逐曲表與SHA | 首尾／重疊／每項足夠才核N；任何缺頁則分母仍unknown | 提供截圖即可先做，立即回答還需解鎖多少 |
| E2 一個鎖定曲的解鎖轉換 | before提示＋必要HD結果＋after IN可選圖 | 不以HD分數單獨判unlock；無after保持unknown | 需後續裝置／遊玩授權與live gate；驗最小操作流程，之後才批次 |
| E3 exact候選6attempt pilot | 原A/A＋交錯A/B，全部freeze／attempt包 | 故障即停；只報pilot效果，不能宣稱全曲或顯著改善 | 有效成本／安全資格＋容量先成立，現時blocked |
| E4 全N曲固定版掃描 | 清單N＋每曲一輪IN完整包，明列失敗 | 全部M0才基礎達成；有失敗就回定位，不無限刷 | 新有界輪次／容量；直接驗產品，而非代理指標 |
| E5 3N穩定性 | 預先列每曲連續3次，跨至少2 session、相同freeze | 三次全M0才該曲repeatability_pass；全部失敗留存 | 使用者選擇較強驗收強度；不聲稱未見曲泛化 |

### 9.3 風險、效益與必要決策

- **最大資料風險：** 20歷史標籤被當成已核完整現行章節，或 `exists:true`被誤讀成雲端持有原檔。以E0／E1最小資料解決，不索取全部測量。
- **最大產品風險：** build／mock／冷契約通過被換算成Miss改善；固定pixels不含新觸控feedback。用同版完整IN結果分層驗收。
- **主要偏差：** 挑最低Miss／漏記中止／只比較平均分；以預先凍結attempt清單、所有結果與逐曲worst-case對帳降低。
- **版本／模式風險：** UI列表、譜面、解鎖與數字可隨版本改變；外部規則只作待核假說，使用者當前畫面優先。
- **資料／操作成本：** 三次／曲至少3N次完整IN，解鎖與失敗另計；只做一輪較便宜但重現性較弱。診斷全錄不是預設前置，避免額外I/O改變效果。
- **現在唯一需要補充的資料決策：** 是否先提供E0小樣本與E1畫面？不必提供帳號或存檔。本輪研究可在沒有它們時交付，但不能越界宣布當前状態已驗。
- **將來才需的執行決策：** 何時恢復裝置／live，選哪個已合格freeze，採基礎一次或另加3次穩定性，及分批attempt／磁碟上限。不能由本報告代為批准；無需重問既定pixels-only／C++20／五指／無AP前置。

## 10. 配套機讀模板與查核限制

`LEGACY_ACCEPTANCE_MANIFEST_TEMPLATE.json` 提供：20個unverified候選、全19份歷史IN的引用／版本摘要、歷史excluded清單、目前盤點null欄、freeze模板、未啟動的attempt欄位規格、驗收規則與停止條件。`attempts=[]`，不以模板null當實測0；`execution_authorized=false`。此檔是研究schema範本，不是已上線validator／自動選曲配置，也不得進runtime策略輸入。

新模板中的 `run_id_normalized`、`session_ref_normalized` 及結果／summary路徑為 repo 相對形式；移除歷史絕對路徑的本機使用者目錄前綴，不重複不必要個資。它們不是原字串。每列 `source_json_pointer` 指向原 tracked JSON 的 `/runs/<index>`，原JSON及原SHA不改；例如歷史main37的光IN對應原JSON `:2844-2877`。路徑正規化不表示原始raw在本checkout存在。

本輪僅驗證JSON可解析、候選數20、historical_IN19、attempts0、所有當前成員／解鎖unknown、分母null及前述源JSON引用一致性。未執行任何新遊戲驗收。若未來要自動判定pass，需以C++實作獨立validator並加入缺圖／錯SHA／跨binary／HD冒IN／M未知／中止冒結算／漏attempt等負例；目前只有可審查規則，不能宣稱validator已完成。

公開資料存取日均為2026-10-05。資料位階：當前使用者畫面／同輪原始證據 > 可追溯歷史轉錄 > 社群一般背景；任一來源未知均不猜補。未下載譜面／APK或接觸帳號。

### 10.1 可重現文件一致性檢查

在 repo 根執行以下唯讀維護檢查；不會跑產品、載入pixels或啟動裝置。這是 JSON 結構／轉錄算術核對，不是遊戲效果測試。

```sh
sha256sum docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json
jq '. as $d | {
  runs: (.runs|length), sessions: (.sessions|length), songs: (.songs|length),
  HD: ([.runs[]|select(.difficulty=="HD")]|length),
  IN: ([.runs[]|select(.difficulty=="IN")]|length),
  miss0: ([.runs[]|select(.miss==0)]|length),
  all_statuses: ([.runs[].status]|unique),
  run_ids_unique: ((.runs|map(.id)|unique|length)==(.runs|length)),
  session_refs_resolve: all(.runs[];
    .session_ref as $ref|any($d.sessions[];.path==$ref)),
  song_run_refs_resolve: all(.songs[];
    all((.hd_run_ids+.in_run_ids)[];
      . as $ref|any($d.runs[];.id==$ref))),
  per_song_counts_match: all(.songs[];. as $s|
    (([$d.runs[]|select(.song==$s.song and .difficulty=="HD")]|length)
      ==($s.hd_run_ids|length)) and
    (([$d.runs[]|select(.song==$s.song and .difficulty=="IN")]|length)
      ==($s.in_run_ids|length))),
  judgment_sums_consistent: all(.runs[];
    .judgments==(.perfect+.good+.bad+.miss)),
  chapter_listing_verified,
  current_chapter_denominator
}' docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json
```

本輪實際結果（exit0）：

```json
{
  "runs": 89, "sessions": 33, "songs": 20, "HD": 70, "IN": 19,
  "miss0": 0, "all_statuses": ["result_confirmed"],
  "run_ids_unique": true, "session_refs_resolve": true,
  "song_run_refs_resolve": true, "per_song_counts_match": true,
  "judgment_sums_consistent": true,
  "chapter_listing_verified": false, "current_chapter_denominator": null
}
```

SHA256 輸出為 §2.1 所列 `610e7f...ba0fe4a`。對模板的最小拒絕誤填檢查如下，實際結果 `true`／exit0：

```sh
jq -e '
  (.current_inventory.candidates|length)==20 and
  (.historical_in_runs|length)==19 and (.attempts|length)==0 and
  .execution_authorized==false and
  .current_inventory.chapter_listing_verified==false and
  .current_inventory.current_chapter_denominator==null and
  all(.current_inventory.candidates[];
    .current_membership=="unknown" and
    .current_in_unlock_state=="unknown" and .current_in_level==null) and
  all(.historical_in_runs[];.miss>0)
' docs/research/zero-miss-20261005/LEGACY_ACCEPTANCE_MANIFEST_TEMPLATE.json
```
