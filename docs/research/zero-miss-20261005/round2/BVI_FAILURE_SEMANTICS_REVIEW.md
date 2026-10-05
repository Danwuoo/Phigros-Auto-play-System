# BVI frozen suite：54 項失敗的語義交叉審查

2026-10-05。審查基準 HEAD `ebe955a6286ca1bb767b631948714a5545b7c174`。本文件只讀既有 source、fixture、oracle 及既有執行報告；沒有重新編譯／執行 candidate、修改原 input/oracle、啟動遊戲／裝置、commit 或 push。

## 結論先行

**54 是失敗斷言數，不是 54 次 Miss，也不是 54 個獨立 production bug。** 它們涉及 19 個獨立 case、31 個 layer-case，可歸為七個最小機制群。所有 54 項仍保留為原 frozen oracle 的失敗，本分類不替它們改判通過。

- 最優先的實作問題：**5 項**來自 V01/V07 把白判定線當成雙側 rails，導致無 rails 的 body 取得假端點、錯誤 Move，並掩蓋 merge 歧義。
- **6 項**來自端點只看固定內側 -4px，恰好讀到白線；後續 independent/usable 失敗是同一觀測缺口，不是六個 temporal bugs。
- **7 項**揭露旋轉接觸的保守漏支持；固定 flank 採樣確實踩白線，因此另有「既定 probe 規則與 supported oracle 不一致」需要裁決，不能只怪 owner。
- **24 項**沒有在 RGB API 施加聲稱的超額條件；**8 項**已 fail-closed，只殘留 typed body payload；**2 項**是歧義 hit 的順序相依診斷；另 **2 項**是 alpha blending 後顏色不落在 frozen effect predicate。

這個 isolated candidate 是 synthetic ROI → descriptor → relation → fake constraints，沒有 observer/scheduler/touch backend，physical human gold=0。上述分類不能推論 C36h 或正式 main 在實景裡出現相同問題，更不能把任何 pass 換算為已減少 Miss。

## 1. 證據與計數

本文件的短代號：

- `S`：`evidence/bvi-frozen-suite/typed-singleton-release-verified/suite.json`（相對本目錄；canonical 明文）。本審查最初讀取的 `typed-singleton-release/suite.json` 後續無損收成同目錄 `suite.json.gz`，其解壓內容與 S 逐 byte 同 SHA；不是刪除或改寫失敗。
- `C`：`research/x10d_o_bvi_build/bvi.cpp`；`M`：同目錄 `main.cpp`；`D`：同目錄 `driver.inc`（相對 repo）
- `N`：補件唯讀根下 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/normalized-execution.json`
- `O`：同上目錄 `oracle.json`
- `T`：相鄰 `hold-ownership-x10d-o-bvi-r1/typed-r1.json`
- `A`：repo `research/x10d_o_bvi/normalize.ps1`，只作 authoring 機制佐證，實際 inputs 以 N/O/T 為準

補件根為 `/workspace/shared/phigros-evidence-round2/unpacked`；原包 README 的 historical SHA 與 derived bytes 必須分讀。逐斷言 JSON pointer、原值、分類與 SHA 見配套 [BVI_FAILURE_SEMANTICS_CLASSIFICATION.json](BVI_FAILURE_SEMANTICS_CLASSIFICATION.json)。

| 層 | layer-cases | assertions | failed |
|---|---:|---:|---:|
| RGB | 89 | 490 | 13 |
| typed | 89 | 589 | 9 |
| lifecycle | 89 | 1,331 | 1 |
| e2e | 89 | 1,495 | 31 |
| 四層小計 | 356 | 3,905 | 54 |
| 22 supplemental、4 schema negatives、4 contact controls、3 renderer controls | 另列 | 33 | 0 |
| aggregate | 不重算為 389 cases | 3,938 | 54 |

20 個新 R1 case 在四層全部通過；失敗都屬原 69 cases。刻意錯 contact 的子報告有 2 個預期失敗且 consumer 正確拒絕，不額外加進這 54。`unverified_oracle_fields=[]`、schema errors 為空。`S` 的 status 仍為 `cold_fail_requires_classification`、adopted=false。

| 群 | affected assertions / layer-cases | cases | 主分類 | 信心 |
|---|---:|---|---|---|
| C1 白線污染 rails／merge | 5 / 3 | V01-whole、V01-touching、V07-merge | candidate 行為缺陷，unsafe fake Move | 高 |
| C2 端點固定 probe 踩白線 | 6 / 6 | V02-continuation、V02-stationary-current、V17-gap40、V17-duplicate-rgb、V17-same-descriptor-new-background | candidate 觀測假陰性，衍生 confirmation 不足 | 高 |
| C3 半透明 effect 顏色 | 2 / 2 | V04-contained-effect | frozen predicate／oracle 語義不相容 | 高：算術；中：應修改哪項設計 |
| C4 旋轉 contact flank | 7 / 2 | V10-rotation | 保守漏支持，固定 probe 契約與 oracle 有張力 | 高：採樣機制；中：契約歸責 |
| C5 宣告容量未施加 | 24 / 8 | V16-regions/lines/metadata/probes-overflow | fixture/API 條件缺失，不證明容量防線失效 | 高 |
| C6 typed invalid payload | 8 / 8 | V16-source、四個 overflow、short-rgb、nan、infinity | 原 typed adapter／輸出語義不一致，動作已拒絕 | 高 |
| C7 歧義 hit 輸入順序 | 2 / 2 | V18-V08-line-order | 非動作 payload 的決定性問題 | 高 |

各群 layer-case 互斥，合計 31；C5/C6 有相同 case 名但不同 layer，因此 unique case 合計須去重為 19。

## 2. C1：白判定線冒充側軌，並抑制 merge 歧義

**5 項 = V01-whole/e2e move + V01-touching/e2e move + V07-merge/e2e alternatives、move、relation。**

- N 的 V01 cases（name 行 180、356；`/cases/2`、`/cases/3`）全部 shapes.rails=false。front=(320,500)、width=140，line y=500。V07（行2032；`/cases/11`）末幀也是 rails=false、front=(320,500)、depth=180。O 對应行 50、64、201。
- renderer 在 body 後畫白判定線（M:22–24），所以左右 rail probe `(250,500)`、`(390,500)` 都是白色；它們不是獨立側軌證據。
- C:34–35 把整條深度任一 `ra.white()/rb.white()` OR 進 railLeft/right，未排除當前 measured line。C:40 因此輸出 d.left/right=true。
- C:44 的 before=(x±49,496) 為藍、after=(x±49,504) 為黑；加上被污染的左右 rails，使無 rails 輸入得到 front_end=true。
- V01 在一致 known_down/prefix1/attachment_query0 fake receipt 下，C:91–94 所有 Move 條件成立。S 的 Move/refresh=true、Down=false。typed 明列 rails=false，因此 typed/lifecycle 的 Move=false。
- V07 原本有兩個可被當前 union 包含的歷史 extents；C:77–78 的 merge 歧義還要求 `!d.front_end`。假 front_end=true 使這條安全分支不成立，最近前端距離又非 tie，得到 alternatives=1、continued、Move=true。typed 的 front_end=false 則得到 alternatives=2、ambiguous、Move=false。

**最早失效層是 RGB rail measurement，不是抽象的 physical owner unknown。** fake known_down 收據原本就允許在合法當前支持下 Move；不能一概用 physical=unknown 否決所有既有接觸。此處是規定需要的雙側 rails 被同一條白線假造，且錯誤傳到動作資格。

下一步：在另行授權的 candidate 修訂中，先保留本原失敗，對 rail 支持加入能區別 measured-line 白交點與真正沿 body 側軌的有界證據；同時獨立驗 merge 分支，避免單一端點標誌覆蓋已存在的多歷史 extents。不要讀 renderer 的 rails flag，也不要以 case name 特判。先驗這一群，再談放寬漏支持。**zero-miss 優先序：P1 安全／錯續接。**

## 3. C2：端點漏讀造成六個下游斷言失敗

**6 項：** V02-continuation cap（RGB/e2e，2）、V02-stationary-current independent（e2e，1）、V17-gap40 usable（e2e，1）、V17-duplicate-rgb independent（e2e，1）、V17-same-descriptor-new-background independent（e2e，1）。

N name 行565、741、8915、9994、10171；O 行85、101、629、661、669。authoring 幾何見 A:14、25–27、53–60。

- 共同關鍵是 front y=504、line y=500。M:24 在 integer pixel center 上畫 `abs(y-500)<=2` 白帶。
- C:44 固定讀 endpoint 內側 -4px，恰為 y500 白色，所以 front_end=false；外側 y508 是黑。實際 y503 是當前藍色、y505 是黑，雙側可見端部轉換仍存在於 4px 範圍內。這不是 fixture 完全遮住端部。
- V02 的 y490、y500 兩幀可取得端點，y504 及其 stationary repeat 不可，因此 independent=2 而非3。V17 三組都以 y504 作前端，因 C:70 先拒絕所有非端點，independent=0；gap40 的 usable=false 也是因此。
- 40ms 邊界 C:61 使用 `gap>40000000`，S 中 V17-gap40 仍有3 samples、60ms window。這份反例没有證明 gap 邊界或 hash dedup 寫錯；typed 有端點時這些斷言通過。

這與首輪已獨立保存的「靜止 Tap＋移動白線」新接入反例不同。首輪 typed 與 RGB 對照都有可用端點，但 descriptor不變使 independent停在1；本 full suite V17 是 front_end先成false，使 independent為0，尚未進入該 dedup 因果問題。兩組機制與分母不可合併。首輪入口見 [研究總報告](../README.md) 的靜止端部 typed／RGB 對照段（行105–107）。

下一步：以同一 frozen renderer/input 檢查 endpoint 的有界當前像素轉換與白線遮擋判別，避免只換一個剛好通過的固定 offset；新 Down 仍維持3獨立／30ms，不能降低門檻補漏。分別保留端點輸出與後續 confirmation，防止把這六項當六個修補。**P2 漏機會／接入能力，高信心，但此 suite 未證明真遊戲漏按。**

## 4. C3：effect=true 與 frozen 顏色規則衝突

**2 項：V04-contained-effect 的 RGB/e2e effect。** N name行1209（`/cases/7`）；O:139；A:29–30。

末幀 effect polygon x=289..327、y=460..480 完全位於藍 body 內，RGBA=(255,225,80,89)。M:25 使用整數 alpha 合成，對 blue=(40,190,255) 得到：

`floor((src*89 + dst*166 + 127)/255) = (115,202,194)`。

C:13 及原 CONTRACT.md:9 的 yellow 要求 R>=180、G>=120、B<=180；合成色 R=115、B=194 不符合。a/b 位於中心±49，還在這個窄 effect polygon 之外；mid 讀到合成色，也不符合 yellow。C:35 因此 effect=false。這不是 typed 沒測 effect：原修訂契約明定 effect 只適用 RGB/e2e（R1 CONTRACT.md:10），兩個必要斷言確實執行並失敗。

實際 body/contact/Move 全符合原 oracle，沒有顯示動作缺陷。**可證的是「存在透明疊層」不等於「滿足 frozen yellow predicate」**；僅憑此結果不能宣稱候選漏掉了符合其既定 predicate 的黃色像素。

下一步：先由獨立契約審查裁決 effect 是「yellow predicate 可見證據」還是「此種半透明色差也須識別」。保存原失敗及算式；若需要新色差能力，要明確另立、凍結可否證規格，不能默改原 oracle 或放寬 threshold 求綠。**P4 契約釐清；未顯示當下 zero-miss 動作影響。**

## 5. C4：旋轉時 flank 踩白線，七項其實同一漏支持

**7 項 = RGB contact/hit_body_required/hit_line_error_max（3）＋e2e 同三項與 move（4）。** N name行2895（`/cases/15`）；O:262；A:38–39。

末幀 front=(316,505)、body angle=.25、width140、depth200；line center=(320,500)、angle=.4。由 C:48–49 的幾何，body 軸與 line 交點為 `(317.542060924,498.960800027)`，body normal offset=-6.232967849，確實在 body 幾何內。

C:52 先沿 line normal 位移±4/±6，再沿 body tangent 位移±49。依 frozen renderer 與 C:14 的 nearest integer sampling：

| line-normal sign/delta | body-tangent flank | pixel | 對 line 的法向距離 | 實際顏色 |
|---|---|---|---:|---|
| -1 / 6 | -49 | (272,481) | +1.191921545 | 白判定線 |
| +1 / 6 | +49 | (363,517) | -1.086951821 | 白判定線 |

其餘6個 flank probes為藍色。兩個 sign 都因自己的 delta6 白點而無法滿足「4與6全部雙側 blue」，故 contact=absent。C:53 不寫合格 hit/line_id，D:40–41 才輸出 hit_line_error=1e9、hit_body=false；這兩個不是另外兩個幾何計算 bug。C:94 因 contact 不支持拒絕 Move，沒有向錯誤 hit 執行 Move。

這是實際、可追溯的旋轉支持能力缺口；但原 CONTRACT.md:11 明定±4/±6雙 flank 支持，且 exact line-colored pixels alone 不合格。**按目前 literal predicate，這個 fixture 沒有提供每一個規定 probe 的藍色支持；oracle 卻期待 supported。** 不應只說「實作不遵守既定規則」，也不能因保守拒絕就把旋轉能力宣告通過。typed 以幾何合格代替 RGB flank，通過不解決這個問題。

下一步：獨立定義旋轉下的當前可见 flank/line-mask 幾何證据，而非沿 body tangent 擴開後仍假設離 line 固定距離；在保持 rails、line唯一、當前像素及有界成本的前提下審查是否屬原契約可修实现，或需要明列新規格。不得把白線當藍 body，更不得直接採 typed contact=true。**P2 持續 Hold 漏支持；安全拒絕不等於 zero miss，遊戲影響仍未驗。**

## 6. C5：V16 declared_usage 不是 RGB 端實際超額

**24 項：四個 overflow 各 RGB current_body_preserved/qualification_invalid（2）及 e2e 加 move/refresh（4）。** N name行5930、6109、6288、6467（`/cases/31..34`）；O:457/469/481/493；A:48–52。

原 fixture 只在最後一幀加入 `declared_usage`：regions129、lines17、metadata1048577、probes6291457。四例仍各有1 query、1 line、640×640 RGB，沒有擴大任何傳給 extractor 的實際集合。

- D:31 的 RGB 路徑只傳 pixels/view、queries、lines；`declared_usage` 不在 Candidate::extract API（bvi.hpp:23）中。
- C:25–26 檢查實際來源/bytes、queries.size、lines.size 及實際 metadata_bytes；C:38/55 檢查實際 probes。這四例 actual metadata=185,904 bytes、末幀 probes=410,621，均沒有超限。
- `declared_usage` 只在 M:41 typed adapter 讀取，令 typed invalid。原 oracle 跨層套用後自然產生24個不一致。
- supplemental 的 actual129-query-API、actual17-lines-API 實際傳入超額並 invalid=true；actual128-query-16line-API 正常。這是已存在報告，非本審查新增執行（D:133–134）。capacity helper 的 metadata/probes +1 也通過，但它們不是 extractor 真正耗尽 probes 的動態壓力測試。

下一步：保留原四例與紅字，新增、另凍結真正施加容量的 cases，分清 input-container boundary、固定編譯結構上限與動態 probe budget。不應讓 RGB candidate 偷讀 authoring-only `declared_usage`；不可藉改 expected 宣稱原容量涵蓋已通過。實際動態 probe 耗盡尚未由這24項驗證。**P3 測試有效性；沒有從這組證明容量防線被突破。**

## 7. C6：typed invalid 已阻止動作，但 body payload 未作廢

**8 項，全是 typed/current_body_preserved：** V16-source、regions-overflow、lines-overflow、metadata-overflow、probes-overflow、short-rgb、nan、infinity。

T name行4325、5487、5655、5823、5991、6159、6325、6823；O 對應期待 body不保留。M:35 先複製 author-declared body=true，M:39–42 後設 `o.invalid=true`，不把 d.body/contact 改成 invalid；相較 C:24 的 RGB invalid helper 會覆寫 body/contact=invalid。

這八例 S 一致為：invalid=true、lifecycle_invalid=true、relation=invalid、samples=0、usable=false、Down=false、Move=false、refresh=false、release=true，但 parts[0].body=true。D:52 的 current_body_preserved 只讀 body 布林，沒有用 invalid 包住，因此按原 oracle 失敗。

**安全條件確實 fail-closed；這八項不能稱為失效後仍在續 Hold。** 但 invalid 輸出與當前有效 body 事實的界面仍不一致，future consumer 如果忽略 invalid 會有風險。它來自原 M:34–43 typed builder；此次 singleton-lines parser adapter 沒有改 body/invalid 邏輯。

下一步：釐清並落實 invalid descriptor payload 的契約，與只清 temporal qualification 的 context/clock reset 區分（後者依原契約可保留當前body）。另行修正時應讓 typed invalid 輸出遵循與 RGB 同一可見性／有效性規則，再用未改 oracle 驗證所有8例。不要刪 body assertion，也不要只看 release=true 就忽略界面。**P3 typed診斷／消費者安全，非已觀測的錯動作。**

## 8. C7：ambiguous 下 hit 隨最後一條 line 改變

**2 項：V18-V08-line-order 的 typed/lifecycle equivalent_to。** N name行13136（`/cases/65`）；T:12255；O:767；A:35、61–64。

typed M:36–37 對每條 eligible line 都覆寫 d.hit/line_id，最後 n=2，所以 association=ambiguous。交換 line順序後，一側 hit=(308,500)，另一側=(308,499.5197438360538)，差0.4802561639462px。這是兩條不同幾何線的確定性差異，不是隨機浮點誤差。M:45 normalized 去除 line_id，保留 hit；D:107–116 因整個 normalized object 不同判 fail。

兩側 Move=false、refresh=false、Down=false、contact_id=1、physical=unknown，其餘 normalized欄相同。C:91 的 line_unique=false 阻止 Move。這份 evidence 沒有錯選 line 進行動作；它否決的是歧義輸出仍保留任一線 hit 的順序不變性。

下一步：使可供消費的 hit 僅在唯一合格時有效；對 ambiguous 保留明確未資格化的候選集合或穩定的無值表達。保留原 whole-output equivalence oracle，不只從 comparator 刪掉 hit 求pass。**P4 輸出決定性，安全 gate 已有效；不能把 payload 任意點當可 Move 點。**

## 9. 對 zero-miss 的真正優先順序與停止邊界

1. **先擋錯續接（C1）。** 將白線與 rails 的證據分離、保留 merge 多假設；不能用放寬支持來掩蓋不安全 Move。
2. **再恢復可見的接入／持續支持（C2、C4）。** 區分 front endpoint confirmation 與已Down body contact，保留旋轉、近線、端點被部分覆蓋的反例。C4須先把 predicate/oracle 張力明列裁決。
3. **修測試的施加能力與 typed界面（C5、C6）。** 讓「被測條件真的送進API」可核對；動態 probes 覆蓋不冒稱已取得。
4. **解決 effect/ambiguous payload 契約（C3、C7）。** 不把低層診斷紅字包裝成已知Miss根因，也不因暫無動作差就永遠忽略。
5. 最後才是 exact source/binary 的 Windows build／成本／實景因果驗證與完整IN結算；本審查不授權後續執行。C36h M77、當前章節清單／解鎖、逐note physical gold、source絕對age等缺口沒有被這個 suite 消除。

不可採取的捷徑：修改原 oracle／threshold 求全部綠、把 typed support 宣稱為 RGB 已辨識、把少 Move 當 safety以外的全面成功、把 body=true當作已有效支持、把54 fail除以3938換算遊戲Miss率。

## 10. 可追溯性／本審查驗證

- S SHA256：`8fbc37e8db4afe1d8736b2f89c17f25dc91d2213581d241279c6ab2dbda38611`
- C SHA256：`f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce`
- D SHA256：`ba0328c622bcd44974e4d7e66414546d1e8de71cfd28f379d796096f2e67f0b3`
- 執行 binary SHA256（既有收據）：`a106b1e8454067397b1785a88d2e93a48916f1b01e30696e6f5863539b89758e`
- isolated adapter.diff SHA256：`f460afa63433da2a4d2e2d1e00595d909fef668caeb99bbf27cacf676327c2d1`

原未適配 runner 的 R1 type305/exit2 仍是獨立失敗證據。singleton parser只把156/159個新R1 typed frame的單Line object讀成一個Line；對應RGB幾何逐值相同，並未重寫任何input。這次54項全在original cases，不能說是改parser修好了candidate，也不能忽略原harness不接受其frozen輸入形狀的問題。

本次驗證限於 read-only source/data 對帳、顏色與幾何靜態算術、分類 JSON 完整性／計數及 `git diff --check`。未重新執行任何candidate；所有執行結果取自 S。數值 witness 是由 frozen幾何／renderer公式推導，用以解釋已保存輸出，不冒充新的實跑軌跡。
