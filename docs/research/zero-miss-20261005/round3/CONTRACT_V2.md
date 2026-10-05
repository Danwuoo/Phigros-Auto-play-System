# BVI cold v2：先凍結的有界修補與消費契約

2026-10-05；基準 `acb27fdb095b82253ef9388eab52948a59663d02`。初版規格於本輪 candidate 結果前凍結；後續明示修訂見下方，不能倒稱全部規格在全部實驗前已定。這是合成 current RGB／conditional ROI／fake constraints 的雲端冷測，不是正式 pixels→touch 整合；physical human gold=0。沒有 live、裝置、Windows、模型或 commit/push/PR。

規格沿革：初版 SHA256 `269b94f49feb003cb83e0d6c706bd1d8c8627d51b68372446574aaa9bc683c42`。`C-V2-ENDPOINT-r2` 只修正 endpoint 的過強幾何 mask veto：獨立 P05 共轉正例在實際 blue pixel 距線 2.7826px 時，被 ±2.8px 保守 mask 排除，雖然 renderer 白線只覆蓋 ±2px。保留該 P1 失敗與原正例 gold；P1.1 採實際 blue/black predicate，不讓近似 mask 抹掉已見顏色。rails 的 white 消歧仍需 mask 外。這不是 V10 contact 規則的放寬。§4 的安全目標則由 P2 單獨修補；P1/P1.1 的 relate 保持 donor，不能稱它們已修好真 cap union。

## 1. 不改的證據與分母

原 core/source、六份 inputs、oracle、STOP、第二輪 356 layer-cases／3938 assertions／54 fail 報告均保留。舊 suite 的 before/after 使用同一舊 oracle，結果另存；v2 新 suite 另列 case、assertion、pending decision 分母，不以新 suite pass 改判舊紅字。

`CONTRACT_CHANGE_MAP.json` 逐項列出 54 個原失敗的 case/layer/field/pointer、原 expected/actual、規格條款、新控制與是否仍未裁決。尤其 24 capacity、8 typed invalid-body、2 ambiguous-hit 均逐 assertion 保留，不只列總數。任何新預期都以以下規格或原規格為據，不由 candidate 輸出反推 gold。

## 2. C-V2-RAIL：current rails 的最小可見支持

原 color predicates、有限 ROI 掃描與 all-lines 輸入保留。每側 rail 須有沿連續整數 body-depth offsets 的最小短片段：至少 **2 個不同、實際 llround 採樣 pixel centers** 同時滿足：

1. rail pixel 符合原 white predicate；
2. 位於所有當前 measured-line masks 外（原 mask 的法向 ±2.8px、切向半長度界線；以實際採樣 pixel center 判斷）；
3. 同一 depth 的該側 interior probe（±0.35 width）符合原 blue predicate。

任一 depth 不合格即重置該側連續 run；重複 rounded pixel 不增加不同點計數。左右各有自己的見證，不要求相同 k。2 是明示的 synthetic proposal，未由真圖校準。白判定線、單白點、兩點間斷、白但內側非藍皆不能構成 rail。rails 狀態每幀重測，不延用歷史。沒有 rails 的 filled body 可保留原可見 body 診斷，但不得取得 cap、新 Down 或已知 contact 的 Move/refresh 資格。

必驗正例：原長 rails、剛好 2 點短片段、平移／region order；負例：無 rails＋白線交點、僅 1 點、2 點間斷、單側缺失、white-line 全遮住僅有候選 rail。

## 3. C-V2-ENDPOINT：4px 內的真轉換

front/rear 分別在各自內側 1..4px 搜尋至少一個同深度 bilateral blue pair；外側 1..4px 搜尋至少一個同深度 bilateral black pair。每個見證都讀實際 llround pixel center 的顏色，不以 ±2.8px 保守幾何 mask 否決已見 blue/black；仍需當前左右有效 rails。white／effect／clipping 不能代替 blue 或 black。兩侧不能取不同 depth 拼成同一 bilateral pair。內外距離可不同，但都在原 4px 界內；沒有完整 pair 就不確認端部。

固定最壞每端 16 pixel reads、兩端 32；讀取計入原總 probe budget。測試至少包括 front=line+4 與 front=line−4 的支持、整帶 white、單側blue、無外部black、無rails、opaque effect 與 frame clipping。此條實現原「within4」可見轉換，未降低 3 次獨立／30ms 新 Down 條件，未放寬 dedup。

## 4. C-V2-MERGE：端部不能消除已有多假設

本條是独立測試先凍結的安全目標；P1/P1.1 僅修 RGB，尚未達成 true-cap union。v2 fixture 首跑對此保留4個 failed assertions，不能標 pending 後從失敗分母移除。獨立 N09 及本契約的4個失敗、單一延續／兩個未合併 current ROI 正例構成 P2 最小修補依據；P2 僅移除原條件的 `!front_end`，另存 source 與各階段結果。

原 contains、width/angle/distance、nearest tie、有效 6 份／90ms 因果窗口不變。同一有效歷史 sample 若至少兩個 eligible prior front-endpoint 候選通過原 contains，就構成未解多假設，保留 ambiguous 與最多2 alternatives；單一 current cap 不自行裁決。保留 current 單一延續與 current 兩個未合併 ROI 的正例，另核有／無 current cap 的 union 負例。不新增矩形 projection 或分離閾值；不把兩個候選稱為已證兩個 physical 物件。duplicate ROI 的既有 nearest tie 過拒仍 deferred，不以新投影宣稱已修。

## 5. C-V2-CAPACITY：真 API 條件與 helper 分開

RGB API 仍只收 View、queries、lines。不可增加或讀取 authoring-only `declared_usage`，也不可讀 renderer 私有 rails/cap/effect labels、case 名、oracle 或 world owner。

- 實際 129 queries／17 lines 分別呼叫 extract；必須 invalid、無動作資格，report 記實際容器 sizes。另測 128 queries、16 lines 及 128＋16 合法邊界；黑圖通過容量僅代表沒有 capacity invalid，不代表 contact 成立。
- 原四個 overflow fixture 每個 RGB 2 與 e2e 4 共24 fail 仍是原輸入／oracle 張力；它們只有 declared_usage，實際1 ROI／1 line。v2 不是在原RGB預期改成通過，而是另立真的 API 負例。
- metadata 1MiB／+1、probes 6291456／+1 只測 `capacity_valid` helper，明列 helper 分母。編譯 `metadata_bytes()` 是 ABI/static bound，並非動態實際耗用。
- 動態 probe 耗盡尚未驗證；沒有真正送 API 且使計數超額的軌跡，不可把 helper 或 declared_usage 當作 dynamic coverage。保留 decision，不降低既定 budget。

## 6. C-V2-ELIGIBILITY：raw payload 不等於可消費支持

v2 readout 是新版本 **冷測消費視圖**，尚未接正式 owner，且不回饋 candidate。原 raw descriptor／Observation 及原 comparator 保留。新視圖至少分開：

- `raw_body_claim`、`raw_contact`、`raw_hit`：原 payload 的診斷轉錄，invalid 時不宣稱是有效當前量測；
- `visible_body_valid`：Observation 非 invalid 且 body supported。單純 context/clock reset 可保留當前有效可見 body 事實；
- `action_eligible_body`：Observation 非 invalid／context_invalid、relation 非 invalid／ambiguous，body supported 且雙 rails（Tap依原獨立規則）；
- `eligible_hit`：上述資格成立、contact supported 且 line_unique 時才是 hit，否則 null。保留 raw hit，不以 null 消除原診斷缺陷；
- 實際 down/move/refresh/release 仍來自原 `constrain`，不能用投影自寫預期動作。

body/hit 的這組 eligibility 是觀測層前置資格，不是單獨的動作授權；plan/gate deadline、unknown/completed receipt 等仍必須通過 fake constraint。報告同时保留 constraint_invalid、實際四種動作及 receipt控制，不能只看非null hit就動作。

對八個 typed invalid-body 原例另建 raw body=true 的明示 typed/schema negatives，逐一核 invalid reason、raw claim 保留、有效支持／eligible_hit 為false/null、history 清空、Down/Move/refresh=false、release=true。regions/lines 使用真正容器數；metadata/probes 是 typed validation helper 宣告，明列非RGB動態壓力。來源／byte-count／NaN／Inf各自施加有效性負例。這不能把原8個 `current_body_preserved=false` fail 改判已修；原 legacy typed adapter 仍未改。

另必含有效 current body/contact/hit 與 Move 的正例，防止一律 false/null 假通過；context change、future、nonmonotonic 的 raw body可見但 **所有動作資格都false**。普通 gap>40ms 只重設 temporal history，當前新鮮 body/contact 合格的 known_down 可繼續 Move；不得把 gap 與 context_invalid 混同。plan/gate expiry與unknown/completed仍按原 constraint，未重放 Down。

## 7. C-V2-AMBIGUOUS：沒有合格 hit 不等於錯誤 Move

原 V18-V08-line-order typed/lifecycle 共2 whole-output equivalence fail 保留。v2 用前後兩種實際 all-lines 順序另核：raw hit 診斷可不同、line_unique=false、eligible_hit=null、Down/Move/refresh皆false且contact_id一致；同時唯一line正例須有非null hit及合法Move。新比較僅說明 action 語義不變，不宣稱 raw output deterministic 或原2 fail修好。不得把0.480256px診斷差報成 action retreat。

## 8. C-V2-PENDING：不順手改 gold 的決策

- V04：原 yellow predicate不變；RGBA89 疊在 blue 得(115,202,194)，不符合predicate。保留2個RGB/e2e effect=true fail；「擴成透明色差效果識別」仍pending，未採用。
- V10：原旋轉固定 flank ±4/±6 會被白線覆蓋，原7個RGB/e2e expected supported/hit/move保留。line-aware每側幾何採樣只是後續提案，本輪未實作、未改oracle；保守拒絕不等於zero-miss能力通過。旋轉的安全負例與合法可見正例若有新增，均另列，不取代這7項。
- stationary新需求／same-descriptor去重：本輪不修，不降低3獨立／30ms。
- 真圖、full owner、有效成本、Windows ABI/build、全曲IN完整Miss=0、目前章節分母／解鎖皆未因冷測取得驗收。

## 9. 執行及停止

`research/bvi_cold_v2/contract_fixture_v2.cpp` 是獨立 C++20 fixture/驗證器；不使用 Python、不讀 candidate 結果生成 expected。輸出包含逐case assertions、各類分母、raw/eligible/constraints、未測量項；輸出根須新建，拒絕覆寫。原 suite 與 v2 各自報退出碼及hash；新assertion失敗不能被pending標籤自動忽略。任何不滿足上述可否證規格的反例保留並停止升格，必要修補後重跑；不宣告產品完成。
