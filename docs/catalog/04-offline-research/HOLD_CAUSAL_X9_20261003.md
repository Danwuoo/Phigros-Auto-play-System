# X9：2479 Hold 不是「原 contact 因無 root 掉線」（2026-10-03）

**Verified：C36h 的原 contact 持續；另一個候選新增 Down 後因 identity ambiguity 取消。** 這是可重現的 action-semantic 差異，不能稱已找到一個遊戲 Miss。**Strong inference：**原圖較支持同一可見 Hold 的內部 fragment 被當成另一個接近前緣，造成額外接觸。Main50 在同窗沒有該額外 Down，值得拆出其 current-rail claim 機制研究；不因此升格 main50。

## 輸入與方法

重用已驗收 X1 R1/R2 frozen tools，零 source/binary 改動。新 manifest 只把 D 窗換成 H2460–2495（anchor2479），另四窗仍 A3493–3504、B4979–4990、C5516–5524、E5281–5293。共82 trace frames／role；從原32張preroll開始完整跑7722張PNG、7715次perception，owner cadence、frame-first、zero fake recognition/RPC、五指成功receipts。原recorded actions只另存比較，不餵策略。

兩次新 full contact replay（C36h、main50各1），每次7722 PNG SHA核對，結尾contacts0、無truncation。每個role的**全prefix semantic digest及events.jsonl均與原frozen X1相同**，不是只比選段；證明此次更換診斷窗沒改該輸入的全程行為。未重新實跑原全測試，也未聲稱新一輪獨立agent驗收。

| role | frozen reader SHA | 新窗口外仍相同的全semantic SHA |
|---|---|---|
| C36h | `5919a4d1316b4e7a8a6bc4d44a416f606e5108a1e97a95f7255dfb1fff5902b0` | `54ce21932b25eabf5d552f49b3020fa31856ba9c0ffe1c3d981a0b7bb79ec4ae` |
| main50 | `e19a618269d61cad0e99fb93c9012df20bef3c3de646b3d3c7dd5d651b3853b6` | `c58428a4d4ecfa72253fb38f15de90771907a28bf3101cf0cf384225b5b28119` |

新資料 [hold-causal-x9](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-causal-x9)，入口 [hold-causal-x9.ps1](../../../tools/hold-causal-x9.ps1)。128MiB batch（既有writer127MiB streams＋reserve）、8GiB campaign+prior；無新build。初PowerShell字串 `$Lineage:` parse error於建batch前修正，沒有執行不明replay。精確用量／SHA另見final-summary。

## C36h 因果鏈

以下 ID 僅同一run內join；不可當physical gold或跨版對應。

| ordinal | current observation / identity / relation | owner / FakeTouch |
|---|---|---|
| 2469 | 已有Hold521接近line2 | intent156、contact1成功Down，後續維持 |
| 2474 | 同窗新增539，`color_core`，center(277.189,513.612)、width139.593；521 current rails/front在(278.415,591.618)、width154 | 539短歷史，沒有Down；521有Move |
| 2476 | 539仍在body內、root_past；521保持on-line current rails | 原接觸仍在 |
| **2477** | C36h539改為`current_rails_recent_anchor`，擬前緣y535.826，得到未到期root；main50對應局部541保持current color位置／無該新Down。這是窗口中需消融的recovery分歧，不冒稱全程第一個版本差異 | 521有Move，尚未增加539 contact |
| **2478** | 539重建y572.648、line2、4 samples、root43380005364；candidate bank為weak_current、`action_support=false`、head_on_line=false | legacy正式路徑仍accept；intent160/contact0在43359740900成功Down，(280.675,588.478)。原521/contact1仍約(278.324,589.315) |
| **2479** | 539 identity_ambiguous→association_ambiguous、samples0/line0；521 current rails／line2／4 samples，root null只是relative_velocity_small | 539/contact0 Up＋identity_ambiguous cancel；521/contact1同幀Move(278.255,587.298)，沒有因此失去原contact |
| 2483/2487/2488 | 521 current續接 | 原contact仍有Move |

**Verified source boundary：** `make_candidate_batch`的quality/action_support是候選診斷／其他tracking入口的契約；C36h實際仍走`track_legacy_batch`，不以該布林直接拒新Down。不能只見action_support=false就宣稱owner違反已接線的gate。`held_support`要求head_on_line或held_body_evidence；539不符，owner alias沒有接管它。到2478 compatible_body可為true，但原521已claimed且539非held_support，仍不符合現有alias前提。放寬alias或取消identity guard不是已證實修法。

**Strong inference pixel review：**本輪看2474、2478、2479原PNG，連續藍body與rails可見，內部有黃色effect；未看到可靠的新独立leading edge。Human gold=0。較合理待驗問題是「為何內部fragment被重建成 approaching front並产生新root」，而非「samples0是否應給原Hold延長grace」。

## Main50 donor 與反例要求

Source比較找到main50而C36h沒有的兩段：

1. `hold_recovery_order`：最多128個anchor，先處理held_body、再on-line、再其他，ID只作穩定tie。
2. fallback重建後再次查`claimed_outlines`：width差<20及line切向中心差≤20，就skip fallback。C36h僅outer分支有較嚴的claim檢查，fallback沒有。

**Hypothesis：**第2項可單獨阻止539借用已被521占用的current rail pair；第1項可能讓結果不依track順序。尚未single-mechanism介入，不能以兩版相關性定因果。

**必要negative：**同寬、同切向位置但在法向完全分離的兩個Hold；不同線、不同朝向；真獨立前緣／thin Tap；一側rails缺失；無current body；unknown Down／completed身份。原donor第2項只看寬與切向距離，沒有normal interval／rail-pair ownership／tangent match，可能誤壓下一個真Hold。先拆一個機制做C36h隔離消融與反例；不能整包搬main50或直接放大nearby去重範圍。

## 下一步及 unknown

進 X10 的**單一fallback-claim donor消融**；source export從C36h frozen lineage複製到新out、保留trace on/off及原C36h全測試。先問能否去掉2478額外Down且保留521、A/B/C/E及D6214，再測同column分離Hold。若donor反例失敗，拒採原donor，另以current pixel rail continuity／body extent提出有界修法，不改測試期待救結果。優先處理這項具體uncertainty，X7角色推理暫停。

Unknown：兩描述是否同一physical Hold的human gold、額外觸控是否造成遊戲傷害、77 Miss中的關聯、真recognition/RPC與feedback、跨曲效果。沒有正式候選可freeze，未使用emulator。使用者本輪live授權仍有效，在冷驗證完成或確有新資料需求時按有限計畫執行。
