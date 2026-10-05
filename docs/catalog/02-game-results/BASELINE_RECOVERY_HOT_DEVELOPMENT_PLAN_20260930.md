# 33／36版能力恢復與逐項熱開發計畫

日期：2026-09-30。執行者：新建 GPT-6 Sol／xhigh task。狀態：A36已核對並完成首批五輪手動實戰；候選開發中，尚未完成穩定恢復或更強驗收。

首批執行索引：`measurements/game-assist/2026-09-30-baseline-recovery/manifest.json`、`first-five-rounds.md`。A36原binary SHA符合凍結值，Android16／SDK36、1280×720 rotation1與五指preflight通過；manual session `manual-session-3293220942400` 完整結算 Glaciaxion HD6、Eradication Catastrophe HD7、Dlyrotz HD9、Dlyrotz IN13、光 IN12。使用者之後關閉emulator，session層因擷取中斷為FAULT，但五輪各自仍是`result_confirmed`且原始鏈已驗。使用者改為先根據這五輪離線開發，必要時單輪熱測，完成開發後再開多輪；並允許多項修正、局部架構改寫及選用49版機制。A36仍是正式候選起點，各主要改動保留獨立差異與回退點。

## 1. 本輪授權、目標與交接

使用者要求先制定 plan，建立新的 GPT-6 Sol／xhigh 專案 task，完成重現準備後開啟 emulator 與程式 STANDBY，直接進入熱開發。planner 只確認新 chat 已啟動，之後不監看、不輪詢，也不啟動第二個程序。後續由使用者直接在新 task 選曲、按 Play、回報跑完與討論結果。

這是恢復及改進33／36版實戰能力的熱開發授權，取代上一階段「本輪只冷開發」的限制；不需要再次詢問是否可啟動 emulator 或進行本計畫內修改。仍採 manual：使用者選曲／難度與按 Play；程序從即時HUD接手，結算保存證據並回STANDBY，不自動選曲、Play或無限重試。正式策略只用即時pixels與有界近期歷史，不按曲名調參、不讀譜／內部狀態、不引入學習模型。

「完成重現後待命」先指**核對並準備可追溯的舊版程式、設定和環境**；高分是否重現必須等使用者實際Play與完整結算，不能在離線準備後就宣稱恢復。先交付能測的36版STANDBY，不能為建立大型新測試框架再延後熱測。

首個里程碑：同期舊基線可核對；找到至少一項可證實的決策退步機制；只修該機制，四首無明顯退步且至少一個實際失效場景改善。歷史高分是恢復參照，AP／zero Miss為遠期目標，均非預設成果。

使用者同輪補充：**最新版本可參考部分，尤其是大幅開發的組合庫；基線仍以36版為主。** 因此正式候選從36版策略出發，新版作可挑選的實作、測試與組合資源，不整包放棄，也不以49版加上36版幾個參數冒充36基線。33版只作互補比較，不按曲切換預設。

## 2. 必讀與現況

依序讀 AGENTS.md、README.md、docs/ARCHITECTURE.md、docs/ROADMAP.md、docs/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md，以及：

- [獨立退步審查與高分基準修正](HOT_REGRESSION_REVIEW_20260930.md)。此處風險與推論不是已證實的全部Miss根因。
- [原四首熱調試](FOUR_SONG_HOT_TUNING_20260929.md)、[HD9來源](HD9_MANUAL_SESSION_20260927.md)、[33版結果](HD9_LEGACY_HD_RESULTS_20260928.md)、[36版結果](MAIN_LEGACY_HD_COMPARISON_20260928.md)。
- [判定線形式](../01-project/判定線形式.md)、[Note形式](../01-project/note形式.md)。只針對已定位退步取代表例，不再以完成所有組合作為開始熱測的前置条件。

交接時工作區是 `codex/four-song-hot-tuning`，HEAD `25d464564c12acdf6945242c6dbd1b0c63a4c1ea`，有49/26正式source／tests與文件的未提交改動。另有本審查與計畫文件。不得reset、clean、覆寫、丟棄或不加區分地提交既有改動。先保存差異、untracked清單、必要source快照與SHA，再於隔離的候選checkout／建置目錄工作；需要worktree時先用app list_artifacts檢查可重用項，再用create_worktree。凍結binary可直接從原完整runtime目錄執行，禁止在它們的位置重建。

原實測task `01a0ed77-bcc3-77a0-9e56-7efd1c85d84c` 已停止測試；可讀其歷史，不能恢復它或讓兩個task同時持有touch owner。新task自行查目前process與鎖；發現仍有live程序時先辨識歸屬、在安全邊界正常停止，不能直接殺掉不明程序。

## 3. 凍結比較基線

| 名稱 | 檔案／來源 | 已知證據與限制 |
|---|---|---|
| A36，主要恢復起點 | `out/main-legacy-v145/Release/pas.exe`，source `cd0ec437f495ee73e92a1adaccd097a86eeb91ac` | observer36／planner18；SHA `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`；Dlyrotz HD9 961572、452/0/0/6，raw仍在 |
| A33，互補基準 | `out/hd9-session-release/Release/pas.exe` | observer33／planner13；SHA `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`；HD9策略＋manual生命周期，有跨曲raw |
| Glaciaxion歷史最高 | strategy source `1636519b9e3ddd472fad1d7494f5bc7525cc5fc1` | 868880、369/2/0/22；原run與原binary已不在，A33同策略但生命周期不同，不冒稱逐byte復現歷史run |
| 中間／負向對照 | 37/19凍結配套、47/23、49/26 runtime與source patch | 用於定位36→37→47→49分歧，不拿47作能力恢復終點 |

同一候選必須跨曲使用共同策略，不可因Dlyrotz選36、Glaciaxion選33而拼成不存在的通用高分版。33、36本來各有優劣，保留完整逐曲矩陣。

### 新版組合庫的保留與移植

先盤點新版已實作的組合機制、合成RGB／oracle生成器、fake-clock及混合Note回歸，沿用其可重現輸入與反例。參考冷計畫G1–T2、R1–R7與場景清單，保留多線／旋轉、雙側、縱連、多押、Hold＋Tap／Drag／Flick、同時Hold與多指爭用等資產；不要重新從零窮舉，也不為簡化回退刪掉它們。

每項標記為「可直接重用測試資料／工具」「需解耦後移植的動作機制」「尚無实戰證據的候選策略」。測試通過只證明自身契約；將near-line初現全拒絕等期待當作既定gold之前，必須核對是否符合使用者要求與實際畫面。正式移植以36版為base，一次一個通用機制，列依賴及前後動作差異，經相關冷回歸與四首熱測後決定保留或撤回。若有需多個元件配合的組合，先用最小adapter接合並分開驗證，不能藉依賴名義整包帶回新版關聯／planner。新版的資料工具／診斷可分開使用，效率優化也另批評估。

舊基線與初期候選固定 lead35ms、uncertainty30ms、五指，先移除lead40與結構改動混雜。若舊CLI不支援某新診斷參數，就使用其實際支援的流程；不可改舊策略只為套新工具，再稱原baseline。

## 4. 第一階段：先準備可測基線並啟動STANDBY

1. 核對A36與A33完整runtime依賴及exe SHA、profile、capability；列明原版／重建版區別。凍結檔案若匹配且可執行，不做不必要重建。需要建候選時使用獨立output，沿用既有MSVC v145與patched gRPC依賴。
2. 建新measurement campaign目錄，寫版本、設定、source／dirty patch、環境、容量帳本。保留所有舊raw與二進位，禁止覆蓋既有session。
3. 依現有啟動紀錄開啟或重用有效的phigros AVD；核對Android16／SDK36、guest5處理器、1280×720 rotation1、density320及五指mapping。固定gRPC payload fast／RGB888 top-down／256KiB，不重開擷取選型。
4. 優先用A36執行現有 `game-preflight` 與 `manual-session`，以實際CLI核對參數。設定 `configs/phigros-hd-assist-five-lead35.json`、capability `measurements/game-semantics-20260927/touch-five/summary.json`、`--no-preview`。若A36支援，沿用原有 `--pixel-clips`；不增加第二路capture。不要在首輪前加入新策略或新長片段recorder。
5. 以耐久背景程序保留stdout／stderr、PID及session索引，實際見到STANDBY且程序存活後向使用者報READY、版本SHA及目錄。離開task回覆不應關閉待命程序。啟動helper遵循隱藏窗口規則，emulator顯示供使用者操作。

首輪由使用者選Glaciaxion HD6、Eradication Catastrophe HD7、Credits HD10、Dlyrotz HD9。可以逐曲完整結算後分析，或依使用者要求等整批跑完再看；不在歌曲期間建置、做大型離線分析或額外抓屏。結算回STANDBY後向使用者清楚標示是否仍待命；更換binary時先正常停止並確認釋放。

## 5. 第二階段：重現實戰與定位首次退步

先收A36同期四首，再以A33作相同四首比較，使用者可調整順序／先後範圍。不得因仍缺一首而阻止分析已完成結果。單次資料只作screening；影響版本選擇的異常或小幅差異，先安排有限A/A或同曲重複，不用大量重跑追最高分，也不宣稱一次重現等於穩定恢復。

如果舊版在当前環境也同樣退步，暫不改策略，先檢查來源／消費圖間隔、RPC、UI門控、解析度方向、負載及診斷差異；不得盲目延長期限。若舊版恢復而新版本退步，優先做下列獨立變更組的消融：

| 優先序 | 變更組 | 要回答的問題 |
|---|---|---|
| 1 | 36→37的note→line評分及單線fallback改動 | 哪顆原可按Note首次選錯線或變unknown？遠處弱角度、近期ID減120分、前兩候選差8等因素分別有何效果？不能直接恢復全場同法向硬限制 |
| 2 | 36→37的線ID全域指派／出生、Hold續接與分段擬合 | 是否因線身分無效、ID更換或root重建，使当前仍可用的Note／body失去動作資格？幾何、ID與owner分開比較 |
| 3 | 37→47的confirmed_line與重接守門及觀測擴充 | 錯線防護是否讓有效關係無法恢復？新增候選是否增加歧義？不把缺乏真值的線數增加視為改善 |
| 4 | 48／49交叉原線覆寫與短缺線投影 | 只有前段原因仍無法解釋時才納入；不能把近期修補當作最早退步來源 |

每個候選必須記明base source、唯一主要行為變更、預期首次決策差異、正反例、對應raw與rollback版本。不整包cherry-pick後才猜根因；必要依賴一併引入時逐項列出，先證明依賴本身無動作語義差異。效能優化另批處理，不與關聯或lead同時修改。

## 6. Hold修正的契約與證據

接入、已開始接觸的續接／Move、tail正常結束、失效釋放須為可獨立檢驗的狀態。缺少新crossing不自動等同body消失；但保留觸控／Move必須有當前body支持或明確有界的既有接觸寬限，不得把预测線當目前像素、盲目跟線旋轉或取消所有安全門檻。

`samples==0`立即取消分支在高分36版已存在；新49版有note887/intent205、note894/intent208的26.85ms／16.07ms取消事件，但無當時RGB真值，不能直接判定為錯誤Up。36版Dlyrotz43個Hold也全走active cancellation、tail確認0，仍只有6 Miss；所以不能用取消數、tail事件數或Down數替代遊戲結果。應先確認原本有效的條件為何在新觀測／關聯後消失。

針對每個已找到的實際反例，建立精確的synthetic／fake-clock回歸，特別是任何時間預測與排程變更；同時保留錯線續Hold、重複Down、未知Down重試、當前支持消失與多指爭用負例。測試期待應來自可解釋的幾何／觸控契約，不能把不按一切當成功，也不能為恢復分數重播歷史觸控。

## 7. 診斷與有限長窗

先使用既有原始事件及有界三幀clips開始熱測。遇到無法重現的warm-state問題，再增加**獨立診斷改動**：同一frame來源單向輸出、有界較長窗口，列明實際保存的前置狀態與缺口，不補造已刪除的RGB。可用帶來源的觀測序列隔離tracker／owner；它不能代替pixels重播驗證observer。

新長窗若確有必要，建議初始硬上限為單窗2秒、最多120張1280×720 RGB、ring≤384MiB、writer mailbox≤4張、每round最多2窗、每次campaign最多16窗；丟棄／截斷必記且不能阻塞action。正式啟用前核對實際記憶體與速度，必要時縮小而非解除界限。此為待實作規格，不宣稱當前已有功能。單獨測採樣off/on的負載與動作影響，不能默認與舊baseline等价。

新campaign raw／clips／logs總預算16GiB，衍生報告／bundle另外4GiB；啟動前依可用磁碟與現有round上限核算，容量不足停止新增高量診斷並告知，不刪舊證據或另開root繞額度。單次暫定最多16完整測試回合，含重跑及aborted皆記帳；達範圍後報告下一批必要比較，由使用者指示延續，不自動無限測試。

## 8. 接受、回退與報告

- 每輪記曲名／難度、P/G/B/M、score、maxcombo、完整result與SHA、所有event segments、版本／binary／source／設定與環境。中止另列，不能當完成成績；HD／IN不混算。
- 按可取得資料報全鏈各階段n、p50／p95／p99／max、jitter、drop／skip／取消／故障。實際Down的延遲只是成功進入動作子集，另報無計畫／未Down分母，來源render age未知則明列。
- 用同曲同期基線逐曲比較；任一曲Miss或非Perfect惡化先標記退步，不以其他曲平均抵銷，也不直接稱自然波動。未解釋退步的候選不升為通用預設；必要的有限重測由使用者按Play完成。
- 重複／未知Down重試、錯接Hold或失效不釋放等有實證的危險行為，立即停止該候選並保存證據；修復前回到可用凍結版本。一般分數退步於結算後停止該候選推廣並診斷，不默默換參數繼續。
- 修改先跑相關回歸；提出可比較候選前跑適用完整Release檢查。Debug／ASan用於記憶體／生命週期等風險修改，避免每個可逆參數小改都阻塞熱測。所有建置／回歸須與live歌曲分開。
- 完成首里程碑需至少一項可重現機制修正、四曲完整候選結算與同期對照，沒有未解釋的明顯退步。單輪只可稱初步通過；稱穩定恢復／更強前須有限重複確認，且不能只挑最佳輪。zero Miss／AP另以完整結算認定。

持續更新本plan的執行狀態與campaign索引；另寫結果報告，區分已證、推論、未知、保留／回退的候選。不自動merge main、push或恢復舊AP goal。planner本輪不跟進監看，所有READY、問題與測試安排直接在新chat交付使用者。

## 9. 2026-09-30 本批執行結果

本批已達 16/16 完整結算上限，新增輪次都在 `STANDBY` 結果邊界更換或停止
binary；最終沒有 PAS touch owner 執行中。原始證據、全 16 輪版本／結算、
近時 A36 對照、重啟 preflight、容量與分析見
[`restart-live-test-report.md`](../../../measurements/game-assist/2026-09-30-baseline-recovery/restart-live-test-report.md)。

隔離 `codex/baseline36-recovery` 分支保留行掃描 prescreen、受限相交線候選
與單線 fallback 保護；其後的唯一可用長線關聯試驗已回退。prescreen 的
離線決策等價與辨識時延收益已有證據，但 Glaciaxion HD6 多輪分數退步，
光 IN12 大幅波動；重啟後 A36 亦同幅退步。候選未滿足 §8 的四曲同期驗收，
不升為通用預設、不合併 main，也不以本批結算推論已修好 Note→line 語義。
下一步沿研究 M0，先補少量人工線 ID／Note 關聯標註及可量測的擷取來源年齡；
若要再開實戰，須另立有限範圍與容量帳本。
