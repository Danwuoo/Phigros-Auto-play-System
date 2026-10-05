# BVI cold v2：獨立正反例審查

2026-10-05。固定 donor：`acb27fdb095b82253ef9388eab52948a59663d02`。

## 結論

最終候選在獨立 renderer 的原 **31 cases／71 assertions** 中，27 個可裁定 case 全通過；另 4 個不可直接裁成動作正誤的 case 保留 decision-only，不加 pass。合法動作機會 **9/9**、安全拒絕 **14/14**、3 個 permutation 與 1 個幾何正例分列。

追加的 3 個 merge 正控制全過；1 個重複 ROI 表示控制保留 **2 個既存失敗**。因此完整獨立報告仍為 **35 cases／79 assertions／2 failed，native exit 1**，不是全綠。Release、Debug、ASan＋UBSan 三份結果逐 byte 相同，sanitizer stderr 為空；LSan 關閉，不宣稱 leak-free。

- 觀察到的安全拒絕組錯 Move 從 donor 的 **7/14** 降至候選 **0/14**；這是合成 fake-constraint 輸出，不是 7 次真機錯按或 Miss。
- 合法 action 輸出從 donor **8/9** 到候選 **9/9**。多數舊失敗原本是端部欄位／confirmation 不符，不能誤稱原本只有 1/9 能 Move。
- 本組未發現新增的 unsafe action 放行，也沒有 9 個合法機會或 3 個 merge 正控制的新過拒；這不等於所有可能 pixels 已證安全。
- 靜止 Tap＋移動判定線的新接入、斜交 contact、極短 body、重複 ROI 去重依然是明列限制。沒有更動 owner、scheduler 或正式程式。

## 1. 獨立性與凍結

先讀既有 AGENTS、架構與 round2 語義分類，**尚未閱讀 v2 candidate source** 時寫下 [原 31-case 期待](evidence/independent/expectations-frozen.json)，SHA256：

`b8691561d1c638af017f301c4b8460c094d1ec0a2e8e6b0b4b4159ccdfe86253`

獨立 C++20 [測試／renderer](../../../../research/bvi_cold_v2/independent_tests.cpp)不 include canonical renderer、driver、typed adapter 或其 oracle。它直接在 640×640 整數 pixel centers 畫藍 body、3px 白側軌及 4px 白線，然後只向 candidate 傳合法 RGB、ROI、measured lines、時間鍵。rails 開關、case 名與期待值不進 candidate。

首次 candidate review 前的 31-case source 已留存為 [pre-candidate-tests.cpp](evidence/independent/pre-candidate-tests.cpp)，SHA256 `1db10375b6e5b968c4ed2013d6eb65c5a8fab785a3587fe1d89871d5930a19bb`。追加控制後，原 31-case 邏輯可逐 byte 還原此 SHA；未就候選結果改 gold。

發現真 rails union 仍錯 Move 後，依主控要求另凍結 [4 個 merge controls](evidence/independent/merge-controls-frozen.json)，與原分母分開。這批控制在已看過初版修補之後設計，故不冒稱全部 35 例都是完全盲測。[coverage-audit.json](evidence/independent/coverage-audit.json)核對原 31＋追加 4 的 ID、分類、缺漏及重複，沒有漏 case。

所有 active／unknown／completed 狀態來自一致的 fake receipt；沒有 physical owner gold。unknown/completed 控制也使用三幀可形成新 Down 幾何資格的序列，拒絕不是靠只有一幀不足 confirmation 取得。

## 2. 分母與結果

| 組別 | cases | donor 完整 case 通過 | 最終候選完整 case 通過 |
|---|---:|---:|---:|
| 合法動作機會 | 9 | 1 | 9 |
| 安全拒絕 | 14 | 4 | 14 |
| line／region permutation | 3 | 3 | 3 |
| 無遮蔽端部幾何 | 1 | 1 | 1 |
| decision-only | 4 | 不計 pass | 不計 pass |
| 追加 merge 動作正控制 | 3 | 3 | 3 |
| 追加 duplicate ROI 表示控制 | 1 | 0 | 0 |

「完整 case 通過」包含 rails、端部、independent 等斷言，**不等於 action 有無成功輸出**。[action-comparison.json](evidence/independent/action-comparison.json)另列真正的動作欄位差異：

- 9 個合法機會中，donor 原已有 8 個正確 Move；S04 三個不同端部的 never-executed Down 被漏掉。最終候選全部 9 個輸出符合期待。
- 14 個安全組中，donor N01/N02/N04/N05/N09/N10/N11 不當 Move；最終全部拒絕，沒有新 unsafe 放行。
- 三個 merge 正控制分別是單一延續、两 Hold 仍保持分離、目前 body 只包含其中一個舊端部；均維持 Move。

覆蓋真 rails、無 rails＋一／兩白線、孤立雙白點、單邊 rail、部分／完整白線遮蔽、16px 短 body、兩 Hold→union、整數平移、共同旋轉 .25/.60、region order、唯一／歧義 line order、unknown／completed、stationary known-down、stationary never-executed、只變背景、三個不同端部。

## 3. 獨立找出的兩個修補問題

### 3.1 真 rails union 仍遮掉 merge 歧義

N09 前幀是 front y410／y500、depth60 的兩個真側軌 Hold，中間有 30px 黑色間隔。今幀改為 front y500、depth150 的真側軌 union，判定線 y500。舊片段的分離是可見像素事實，不靠私有 physical identity 標籤。

初版 rails/endpoints 修補 `550efdff…` 仍輸出 front_end=true、alternatives=1、ambiguous=false、Move/refresh=true。其 `contained>=2 && !d.front_end` 讓當前可見端部掩蓋多個舊端部。

最終只去掉 `!d.front_end`，未更改 contains、matching 閾值、tie、history 或 owner。N09 現在 alternatives=2、ambiguous=true，拒絕 Move。三個追加正控制都仍可 Move。

### 3.2 幾何 mask 把真藍像素排除

P05 共同旋轉 .25：front=(320,504)，line center=front−4×normal。inside −1px 的兩 flank 落在 `(273,491)`／`(368,515)`，皆是實際 blue `(40,190,255)`；line normal distance 分別 3.0321246／2.7826466。後者落入 2.8px 保守幾何 mask，但它不是白線像素。

初版 endpoint 要求 `!masked`，所以把可見 blue witness 排除，front_end=false。這是獨立新正例找出的過拒，不是把原 negative 改為 positive。[逐點證據](evidence/independent/endpoint-witness.json)與 [C++ probe](evidence/independent/endpoint-witness.cpp)留存。

最終端部只採實際 bilateral blue／black；白色自然不滿兩種 predicate。這保持 4px 界限、雙側及真 rails 要求，同時修掉 mask veto。

初版 31-case 報告原有 3 fail：N09 兩項＋P05 一項；加入 controls 後為 5 fail。兩份報告保留，沒有覆寫成最終綠結果。

## 4. 尚未解決，不能借拒絕算通過

1. **Duplicate ROI 過拒，2 assertions 保留。** G04 同一幀有兩個完全相同 Query，後續只有一個幾何 continuation。兩者不是兩份 distinct pixel endpoint 證據，API 也未宣告 ROI 必唯一。但 donor 與最終 candidate 都因 nearest-match tie 產生 ambiguous=true、Move=false。這是既存表示／關聯缺口，非此 patch 新增。不要拿 G04 證明「兩個可見分離 Hold」；不要為全綠偷改 oracle。±3px near-duplicate 的 same-boundary 歧義亦未專門驗收，應隨 dedup 契約另立 controls。
2. **Stationary Tap＋移動線的新接入未修。** D01 同一 Tap，線從 y470→485→500；最終 front_end=true、contact supported、line unique，但 independent=1、Down=false。舊 descriptor novelty 規則仍擋這個新需求，與本輪端部原本不可見不同。列 decision-only，不能稱安全拒絕成功或已解 stationary acquisition。
3. **6px body 有端部但 body/contact absent。** D02 最終 front_end=true、雙側 rails true，卻不足舊至少 8 個 body support rows。Move=false；不把此保守輸出當能力通過，也不直接拿短 body 的私有 render 參數宣稱 owner 應 Move。
4. **斜交 contact 仍漏支持。** D03 body angle .25、line .4，body／端部支持但 contact absent、Move=false。固定 flank 問題與 round2 C4 同型，沒有放寬 contact 規格解決它。
5. **不可觀測 physical owner 仍 unknown。** D04 相同 legal RGB/ROI/receipt 必然得到相同輸出；即使 fake known-down 取得 Move，也沒有因此證明世界裡到底是一個或多個實體 Hold。既不強迫拒絕刷 pass，也不把 Move 說成 physical correctness。
6. 歧義兩線輸入換序的 action eligibility 不變，但 hit 診斷仍隨順序而變。M02 action 控制通過不代表診斷 canonicalization 已修。

最小建議：保留已凍結的 rails/endpoints＋cap 不遮 merge 修補；另以明確新契約處理 stationary relative-motion acquisition、duplicate/near-duplicate ROI、oblique current support。不要在本輪追加 owner 重構或縮低 confirmation 門檻。

## 5. 重跑與建置證據

最終 source：

- `bvi.cpp` SHA256 `232bb808e0e21b3b25e5e85995f8d0333597f446b8f867993535b84b40543e81`
- `bvi.hpp` SHA256 `1426a4b245988c8186f665444f0ddbc9f5f5f93c37d20e513fc77ad5c8bb043a`

[run-independent.sh](evidence/independent/run-independent.sh)拒絕覆寫既存輸出，核 frozen expectations SHA、記 compile command/source/header/dependency/binary SHA，核 run 前後 source 未改、native exit 與 report fail count一致。g++ C++20；依賴沿用 `/tmp/phigros-bvi-round2-deps` 官方 pinned／patched nlohmann-json header，未更換 JSON 實作。

```sh
E=docs/research/zero-miss-20261005/round3/evidence/independent
bash "$E/run-independent.sh" "$PWD" /tmp/phigros-bvi-round2-deps candidate release "$E/new-release"
# 第四參數也可用 debug 或 sanitizers；有 G04 已知失敗時預期 native/runner exit 1。
```

[Release](evidence/independent/final-release/summary.json)、[Debug](evidence/independent/final-debug/summary.json)、[ASan＋UBSan](evidence/independent/final-sanitizers/summary.json)均 79 assertions／2 failed。[逐 byte／streams 核對](evidence/independent/final-comparison.txt)完整保存。三配置 compile exit0；native exit1 僅反映 G04 retained failures。ASan 用 `detect_leaks=0`，UBSan 用 `halt_on_error=1`；stderr 0 bytes，不作 LSan、Windows 或無 UB 的普遍證明。

本組最大 observed probes 410,690（包括 full-frame signature）；只是所測合成輸入，非 worst-case bound 或 p99 成本測量。原 misleading-indentation warning 保留，未順手修改與此範圍無關的 constrain。

原 356 layer-cases／3,938 assertions／54 fail 的 frozen suite 由主控另行重跑；本報告不把它們與 35 個自建 case 相加，也不取代其 original oracle／trace。沒有執行 Windows、裝置、emulator、live、模型、訓練、paid action、push 或 PR；沒有修改原 candidate、原 contract／oracle、正式 owner，也沒有 commit。
