# X3：線角色、有效切向與重新關聯的逐物件證據（2026-10-02）

**2026-10-02總控獨立驗收通過，X3作為離線診斷工具與研究交付結案，詳[status J9](../01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md#j9-x3總控獨立驗收通過離線研究與工具結案2026-10-02)。** 下文自驗與容量保留開發交付當時紀錄，最新驗收見§10。本輪交付有界、唯讀的 C++20 causal-feature report；沒有新增 shadow rule、重新 replay 或 production 策略。現有訊號能區分幾種 guard 子因，但不足以把幾何接近／交疊升格為 judgment role，亦不足以凍結新的 live candidate。

主要結論：D 的初配、舊線仍被觀測時的 competition、缺線後的幾何 continuation，以及 current-overlap Down 是不同問題。水平線自己移動也會讓距離縮短；completed contact 更不能因新的 relation 而復活。C36h 仍是主要 behavioural／experimental baseline，main50 為 control／mechanism donor／可能 regression；X2 winner-only variant 不升格為 baseline。

## 1. 工作區、來源與交付位置

- HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，main、唯一 registered worktree；候選 observer50／planner27／diagnostics11，live rounds=0。沒有 fetch、commit、push、切換分支或新 worktree。
- 開始即有未提交 X1／X2 工具與文件；原 `apps/frame_review/main.cpp` 的修改保留。本輪增加 X3 檔案及 offline CMake 的可選 target，另更新本交接與 status 的頂部／J8。`src/`、`include/`、根 CMake 無改動。
- 按專案準則閱讀 README、架構、路線圖、主程式／跨曲研究、status J7、清理與整合交接、X1／修補／校正後 X2 交接；比對 C36h／main50 association、motion、owner／scheduler 及既有 crossing tests。沒有恢復歷史流程。
- 新 evidence 全部保存在 [line-role-x3](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3)；硬上限 **33,554,432B（32MiB）**，失敗 attempts、logs、manifest、source snapshot、帳本均計入。沒有複製原 PNG、trace、export，沒有另 root 分攤同批 evidence。

父來源是已獨立驗收的 X2：

| 綁定 | SHA256 |
|---|---|
| X2 input manifest | `e1704c0c6ab0cc6bb9def4aad31a8e49416556947ba319d9487dbbd451577cbc` |
| X2 acceptance-summary | `5436b5f6d981df40554dcce9bbbcb41e5aa15b7a2650ffb5a075883de8349dc8` |
| C36h replay binary | `bf78aed01795802ee0d0736101dc423d2fd0744e8144a5263bfde38c2c61f550` |
| main50 control／variant 共用 ablation-capable binary | `6981f88d91e404f8d5b2fe1c9ced34154af6d07dbcf8da6298bc94e12c02ecdb` |
| C36h tracking source | `01cd9fba1b70d79217a93a5d0a5193e1a1ef9a259bf937b019e75c723cf03b22` |
| main50 X2 tracking source | `6050c8bcb778368f7d0ef1df05a4f1736d71572ec94024d66ee0dce7ed6c1aee` |

[X3 input-manifest.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/input-manifest.json) 明列三角色，分別固定至 X2 的 `c36h-reference-on-1`、`main50-control-on-1`、`main50-no-override-on-1`。每個 run 的 summary／trace／first-intervention／events 實際 SHA 同時綁定 query 與父 frozen ledger；再驗 source provenance、variant、歷史 binary 實體 SHA、index SHA、frame join 與所用 PNG SHA。不是以 observer 版本或 run 順序替代來源證明。

**新 full replay=0、live=0、training=0、shadow rule=0。** 本輪讀既有固定 C36g pixels counterfactual traces；沒有執行 emulator、manual-session、真觸控、模型、goal、自動化或下一 task，也未訊息其他 chat。

## 2. 最小 C++ 交付與重現契約

檔案：[x3_features.hpp](../../../apps/frame_review/x3_features.hpp)、[x3_report.cpp](../../../apps/frame_review/x3_report.cpp)、[x3_report.hpp](../../../apps/frame_review/x3_report.hpp)、[x3_main.cpp](../../../apps/frame_review/x3_main.cpp)、[x3_features_tests.cpp](../../../tests/x3_features_tests.cpp)、[offline/CMakeLists.txt](../../../apps/frame_review/offline/CMakeLists.txt)、[line-role-x3.ps1](../../../tools/line-role-x3.ps1)。分析由 C++20 執行；PowerShell 僅維護、建置、執行、hash／容量與人工研究註記。

`X3_REPORT=ON` 只加入 report／test targets，沿用 frozen `out/x1/main50-v2` 的 core 來連結 SHA／PNG utilities；report 不呼叫 observer replay、planner 或 owner 的動作 API。沒有修改既有 X1／X2 策略 target。新的 source／binary／DLL、重用 export hash 與 source 副本見 [tool-freezeverified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/tool-freezeverified.json)。

重算入口（必須指定不存在的新 output；先保留 6MiB headroom）：

```powershell
& .\out\x3\build\Release\x3_report.exe `
  .\measurements\game-assist\2026-09-30-m0-manual-continue\line-role-x3\input-manifest.json `
  .\measurements\game-assist\2026-09-30-m0-manual-continue\line-role-x3\independent-features.json
```

保存的執行命令、exit、logs 可直接覆核；維護腳本 `Reports` 已使用的 attempt 名不可重用。獨立驗收新增量仍須記在原 batch，避免覆寫任何 frozen output。

報告功能／邊界：

| 項目 | 實作與限制 |
|---|---|
| 時間 | 原 QPC capture_complete／pixels_ready 與 X2 fake monotonic 分欄；fake capture 必須等於 index 時間平移。來源年齡、真 recognition／RPC delay 仍 Unknown |
| current evidence | current target／candidate、candidate→runtime ID assignment、current line pose、association_valid、observed time、age、score／gate；runtime ID 不作 physical gold |
| 線局部座標 | 切向 canonical sign、法向、signed distance、沿線位置、appearance dot、core 是否包含投影交點；外觀朝向、Note 移動、線法向分開 |
| 距離趨勢 | 最多6個已處理 frame、90ms、每 frame16 lines；Note prior time 與該 line prior observed time 必須相等，ID 必須相同、association 必須有效。讀後才更新 pose bank，沒有 future row feature |
| measured secant | `(d_now-d_prior)/dt` 包含實際量測的線平移／旋轉；另列 `(|d_now|-|d_prior|)/dt`、Note 平移在当前法／切向的分量。不是 instantaneous velocity、root 或下一幀預測 |
| motion validity | `motion_valid=false` 時 model velocity／angular velocity／model relative-normal velocity 為 null；有兩次有效 current pose 仍可有 measured secant，兩者不能混稱 |
| 無效／缺資料 | 初見、不同 line ID、缺 prior pose、identity ambiguous、過時、非當前 pose 皆不猜速度；null 與 reason 保留，不能把零向量作有效 motion |
| confirmed | 主 main50 trace 的 pre-selection confirmed ID；沒有明確建立 event。第一次看到 confirmed 只記 bracket；在局部完整窗口符合原3 samples／30ms predicate時列 proposed 建立時間，不能冒稱完整內部 event。C36h 未輸出該 ID／時間則維持 Unknown |
| continuation | 從最近 exported accepted sample 重建原 old-visible／90ms／dot≥.97／prior-hit gap≤36 的幾何子判定，grade為 Strong inference；replacement count／first time 未輸出，保持 null。40px current support另列，沒有繞過前置幾何 guard |
| owner | 當前 owner identity、contact、prefix Down／last receipt、scheduler／contacts／lifecycle 分欄。未提交、started、unknown receipt、retired、completed分開；不從幾何返回推出新 Down 資格 |
| actions annex | 讀完整既有 events 後列 selected runtime IDs 的 receipts；此 annex 可含窗口後事件，但絕不回饋逐幀 feature。success 是 fake receipt，game adoption Unknown |
| 有界 I/O | query／行串流沿用既有有界 reader；trace109 rows≤16MiB、first context≤5 rows／2MiB、index≤36,000 rows，events≤100,000 rows，receipt annex≤4096／role。Note state≤128；單 report≤6MiB；batch32MiB、campaign＋prior8GiB。容量不足拒寫、不刪舊檔、不 silent truncate |

入口還拒絕 reparse 路徑，output 必須是固定 batch 的直接子檔，使用 CREATE_NEW；writer 使用原 shared budget mutex。這是凍結來源下的研究工具，不聲稱已對任意偽造 trace 做全面鑑證，也未用填滿磁碟來實測全部容量邊界。

## 3. 實際分母與可重現性

主輸出：[causal-features-1verified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/causal-features-1verified.json)，第二份同內容。兩份各 **3,034,295B**、byte-identical，SHA256 **`8fce0da93a9455c9762182ce16bda855ac0600ae61385bbc92251746a359c777`**。既有 output 負例後再次核對 SHA 不變。

| 角色 | trace frames | selected object occurrences | valid measured line pairs | unknown pairs | full events read | selected receipts |
|---|---:|---:|---:|---:|---:|---:|
| C36h reference | 109 | 132 | 315 | 74 | 6115 | 45 |
| main50 control | 114 | 137 | 323 | 76 | 5964 | 36 |
| main50 winner-only variant | 114 | 137 | 323 | 76 | 6083 | 42 |

合計337個 role frames；**114個 unique PNG references**實際 hash核對，沒有新增7722張全組掃描。C36h 沒有 X2首次介入 context，不補造 G 的 C36h五幀。109是既有五窗，main50另有 G1533–1537的5幀；不是337個獨立案例或 gameplay分母。沒有以可算 pair 子集掩蓋 unknown；報告均 `input_truncated=false`。

假說在首次 report 前保存於 [hypotheses-before-report.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/hypotheses-before-report.json)，含 H1–H4 與 falsifier。workspace-before 是開始整理時的 snapshot，當時新 `x3_features.hpp` 已存在；不冒稱它是動筆前的全部原始工作區。沒有為了成立假說修改原 gold。

## 4. D6158–6220：先分開初配與後續兩種 conflict

以下數字均來自 report／父驗收的凍結 traces；RGB 目視只為 proposed grounding。主 Drag 在 main50 的 runtime **1649→1650** 與 C36h **1622** 不能當跨角色 physical ID 對照；6174有重疊紅色物件，身份續接仍 proposed。

### 4.1 6160初配是 distance／role scoring 問題

main50 control／variant 都初選水平 **316**，score **26.596113**；次佳是水平 **305=289.121193**，垂直 **315=917.604857**。confirmed=0／preserve=false，不能把這次錯配歸因 confirmed preserve。原 RGB 的黃色 core 約 `(21,287)`，附近水平線約y273；vertical距離遠。C36h 以 `!normal_approach && alignment<.95` 的 `role_alignment` gate 排除水平358／343、選垂直356。

6160沒有 prior motion，所以 measured pair 無效。到6161，main50同 runtime1649／同測量line315可算距離從917.005縮至900.530px，absolute-distance rate **−3916.238px/s**；Note 在垂直線法向的分量 **−5936.824px/s**、切向237.156。水平316的距離反而由14.000增至32.998px，rate **+4516.044px/s**，Note大部分沿其切向5936.824移動。這是該短4.2067ms pair的 secant，不能把如此短區間速度当穩定長期預測。

這支持「第二次current量測才有可辨的相對運動」；**不支持把C36h遠處appearance hard gate全域移植**。Note 接近時才旋轉對齊、線追 Note、旋轉 Hold均有反例；外觀和移動分量必須分開研究。

main50 runtime1649在6164首次符合局部3 samples／30ms建立 predicate，6165 pre-selection confirmed316；6170／71為ambiguity，6172／73為conflict。6174換成1650，6175重新累積水平305，6177符合predicate，6178 pre-selection confirmed305。這些建立時間是 **proposed predicate**，不是未輸出的明確事件；並未說同一 confirmed316 一直延續到6194。

### 4.2 6194是舊305仍被觀測的 competition

variant保留best水平317，score157.6，比confirmed305的171.985435低；原 `confirmed_line_relation_conflict` 仍清成line0／samples0。舊305存在於 current valid bank，第一幾何 guard為 **old_line_currently_visible**；它不是6214的缺線正交子因。

此幀 vertical315 distance403.5px、closing rate **−526.368px/s**，Note normal **−526.368**／tangent0；水平317 distance−154px，closing rate反而 **−1052.735px/s**，但Note normal0／tangent526.368，且 `motion_valid=false`。因此「closing越快越合理」會選到移動水平線；不能只看 `motion_valid=false` 就丟棄兩次當前量測的距離趨勢，也不能填零 model velocity後當成靜止線。

### 4.3 6199–6205是 history expiry→current overlap→完成

6199原90ms history自然到期，confirmed清0，水平317重新累積。6200 samples2、fitted root null，水平 projected hit在core內、distance約 **−8px**，既有 current-drag-overlap 路徑產生Down：contact0、intent520、fake delivery6200。6201–6204是Moves，6205是Up，共6個命令；主 Drag先前Down前綴0→1。

6200水平 closing rate **−609.824px/s**，Note normal **−31.002**／tangent478.854；vertical315 closing **−478.854px/s**。RGB直接支持水平線與側向core當前交疊；**judgment role、原遊戲是否採納、是否少一個Miss仍Unknown**。此新增Down不能稱改善，也不能因沒有root否定A的持續body contact。

6201局部predicate開始confirmed317。6211其最後有效current量測時，intent520已在6205完成；之後 owner的prefix Up是完成證據，不能藉role重配、缺線續接或幾何返回重播。

### 4.4 6212–6214是舊線缺失、正交不續接

6212起317從 current candidate bank消失；6214 current只有垂直318與水平305。variant best318 score50.099419，但confirmed317仍在有界近期內，最後量測6211距今 **61.8127ms**；舊水平／新垂直切向abs dot **0.0000582343347**，不達原0.97。prior-hit normal gap約101.508px亦不達36。第一 guard是 **tangent_not_local_continuation**，其後replacement count／12ms不能繞過前置條件；report列為未輸出。

318在6214 `motion_valid=false`，但6213→6214兩次current pose與Note identity非ambiguous，distance rate仍可算 **−1001.979px/s**；model velocity／angular velocity仍null。這就是 motion model與current measured geometry要分欄的實際例子。

control仍preserve水平305；C36h採垂直360，在6215有root／plan／Down。這是不同自然state路徑的固定pixels比較，不能把variant清conflict後必然等於C36h，亦不能復活已完成520。

### 4.5 「trace缺線」不等於 RGB 沒線

目視原6211–6214，畫面上方灰色水平線仍有支持。C++研究annex在手動限定的y96–160、x96–864九個等距點選最亮row，輸出原RGB與下方8px背景，**不進feature／action**：

| ordinal | 最亮sampled row | 九點mean RGB | 通過原white predicate點數 |
|---|---:|---:|---:|
| 6211 | 138 | 195.740741 | 3/9 |
| 6212 | 132 | 190.222222 | 0/9 |
| 6213 | 124 | 178.222222 | 0/9 |
| 6214 | 120 | 171.666667 | 0/9 |

原white classification需要每通道>195且channel差<35；6212起這九點都不達。它們仍顯著高於下8px背景。原horizontal scan範圍12%–95%高度，row120在範圍內，**沒有ROI離開的證據**。

**Strong inference：**亮度下降與當前line bank缺失相符；physical317的延續僅proposed。九點不是全行classification／component trace，不能由此宣稱已證明唯一提取子因、已找到閾值修法或本輪應放寬white detector。已確認的語義是「missing from current observations」，不是「原pixels不存在」。

## 5. G1533–1537與A/B/C/E controls

父X2全prefix首個 preserve eligible／override可介入點是 **804**，首個 different winner／semantic state是 **1535**，首個全receipt action差為variant **1547** vscontrol **1552**；本輪没有重掃全prefix來改寫這些父驗收結果。

1535 runtime184：best垂直3 score **48.229426**，confirmed水平2 score **178.459826**。control override→2；variant保留3後原conflict清0。舊2仍current，第一guard是old-visible。3首次量測，prior line pose unknown／model motion false；2有26.4307ms measured pair，absolute-distance closing **−603.472px/s**，Note normal **716.081**／tangent−163.990。水平仍有正在接近的測量支持，較近垂直線不能自動偷走confirmed role。

G只有5幀局部context；confirmed2在1533已出現，exact establishment Unknown。原 RGB 支持垂直線1535突然出現在下降橫向Note附近；judgment role仍unknown，不把runtime line3的出現宣稱成同一physical線換ID。

| control | 本輪原RGB／trace grounding | 保留的結論／限制 |
|---|---|---|
| A3498 | 主Hold949在3488已有Down；3498 body／rails仍current、owner started且有Move，當前root null／motion_discontinuity。附近963另列，不能混成949 | 無root不等於該Up；持續body support與新接入分開。A窗selected cohort的父X2state／action相同，未新驗遊戲採納 |
| B4986 | 當前Hold body已缺，runtime1547列於absent owner，前綴Down4963、Up4986 | absence release保留；control／variant此前Down約0.539547ms及位置微差仍存在，不稱全部control完全不變。absence本身不是完成gold |
| C5520 | Flick1592 current；Down5516、Up5520，merge control | 保留Flick／完成狀態；不拿相邻線／特效冒充同一Note身份 |
| E5287 | 正常control的Hold／Tap仍分列，prediction／not-submitted可能是尚未到排程時刻 | 沿用使用者「正常」定位，不能因root／owner某欄為null重標失敗 |

[manual-rgb-review.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/manual-rgb-review.json)保存20張已目視原PNG的ordinal／source_frame／path／SHA。AI目視、跨runtime continuity與role假說全部是proposed，不新增human gold。

## 6. 可證偽反例矩陣與本輪決策

沒有 shadow classifier，因此不報造出的TP／FP／FN、門檻命中率、gameplay rate或跨曲安全率。以下是量測／既有guard對照，以及會推翻簡化規則的反例：

| 情境／檢驗 | 可用訊號與結果 | 簡化規則的誤放／誤拒風險 |
|---|---|---|
| 正確初配、右移→固定vertical（synthetic） | 2 current poses後vertical close−1000，水平normal0／tangent1000；初見motion unknown | 只取最近距離會先配水平；第二幀相對運動可辨，初見不能猜 |
| G／原crossing保護 | approaching confirmed horizontal仍close；competitor首次未知或零normal | 直接取best／刪preserve會破壞原gold；X2已保留唯一失敗 |
| D錯誤confirmed水平 | 6194移動水平也close，比vertical更快；Note主要沿horizontal切向 | close-only會誤放；只用Note normal排除會誤拒「線追Note」 |
| 6200 incidental current overlap | current投影在core、rootnull仍可plan；physical role／adoption未知 | overlap-only不能證明role；禁止所有no-root contact會傷A |
| 同線幾何、不同fragment ID | synthetic same orientation、gap2px可通continuation；跨ID measured motion unknown | 同ID當必要條件會誤拒fragment；把新ID當已知速度會誤放 |
| 多線同方向neighbor | synthetic parallel但gap50px拒continuation | 單看tangent dot會誤放另一條線 |
| 舊線可見 vs缺線正交 | 相同orthogonal geometry分別先old-visible與tangent guard | 相同reason不能合併子因；放寬續接不能處理normal competition |
| Hold按住時線旋轉 | stationary body、線30°／20ms旋轉，measured d-rate−250、Note速度0；tangent sign翻轉結果相同 | 只算平移normal會漏掉旋轉；盲目跟線Move亦沒有支持，本輪無owner變更 |
| 遠處不對齊、接近才對齊 | feature API不以appearance gate，量測位置仍可close | 全域復用C36h distant alignment gate可能誤拒；此synthetic僅驗feature independence，未驗所有tracking語義 |
| motion model invalid／evidence缺失 | D水平317／vertical318有measured pair但model null；缺prior／過時／ambiguous保持Unknown | 不可填零當有效model；不可把known secant當prediction |
| A body／B absence | A維持started contact；B已Up，absence另列 | 無root或缺line不直接代表新Down／Up資格 |
| completed／unknown Down／返回 | synthetic receipt狀態保守；D520在6205已Up，6214仍completed | 幾何返回、relation修正不得復活intent；unknownDown不能retry |

13項新test包括上述量測、rotation、late alignment、fragment／neighbor、invalid model／time、unknown／completed、malformed geometry及CLI missing input。解析期幾何拒絕不作遊戲判斷規則。既有C36h／main50 control suites提供另外的tracking／owner保護；合成的truth只在其明示幾何設定成立。

**決策：尚不值得凍結新候選。** 可辨「Note沿哪個法／切向動、線是否在動、舊線是否仍在bank、現有guard先擋哪裡」；不能僅由這些量推出唯一judgment role。初配錯誤與錯誤confirmed的自洽支持需要分開，不靠刪conflict、縮90ms expiry、放寬lease、重試Down或加入新模型解決。

## 7. 唯一下一個 discriminating experiment（提案，未執行）

**做一個D的「pre-confirmation proposed-role oracle」離線因果介入，判別初配／fragment再初配是否足以導致後續鏈。** C36h frozen reference與main50 frozen control重用；只在新的main50 diagnostic variant、D6158–6220所選core且confirmed=0時，把pre-selection choice指定為當幀current bank中已用原RGB／幾何註明的vertical候選。6160 prior motion unknown保持；6161才有兩次current measurement，6174換runtime時亦只用當前候選／proposed cohort。不得把窗口後資訊輸入feature，不能以completed owner重新觸控。

這是明示研究oracle，不是expert label、human role gold或可部署rule；只變 provisional choice 一個機制，score計算／identity／preserve／conflict／history／motion／owner／clock保持。介入後的歷史與runtime ID自然變化按causal consequences記錄，不再硬對同ID。已confirmed的competition與continuation仍照原策略，故能辨別修初配後是否仍被錯誤保留、fragment或owner擋住。

驗證從原完整prefix起跑，僅保留D／G／A/B/C/E的有界trace與canonical全prefixdigest／events；至多2個variant重算確認trace開關不改結果，新batch預先保留≤24MiB（含失敗、metadata及ledger），先重新核容量。必須保留原crossing gold、rotation／late alignment、neighbor、unknown／completed負例；不能改期待或以更多Down報成功。

可證偽標準：若兩段未confirmed初配指定後，vertical不能持續到fresh root／plan／Down，報第一新guard與自然state；若能恢復鏈且無completed重播，僅支持「早期role choice足以改變這條固定輸入鏈」。兩種結果都不證明遊戲採納、role真值或跨曲泛化。這比再移除另一層conflict更能區分初配原因與後續保護；真正規則仍須独立role證據與相反運動場景。

本chat在交付後停止；上述提案不構成自動啟動下一task、replay或live的授權。

## 8. 自驗、失敗分母與保護

| 本轮實跑 | 結果／邊界 |
|---|---|
| X3 Release synthetic／CLI tests | **13/13 pass，0 fail／skip**；最新 `testsverified`，CLI明確排除mutex busy假通過 |
| 既有C36h frozen tests | **213/213 pass，0 fail／skip**，27張原opt-in RGB fixture |
| 既有main50 control frozen tests | **244/244 pass，0 fail／skip**，相同fixture；兩套共用cases不合算457個獨立場景 |
| 兩次actual report CLI | exit0、byte-identical、337 role frames／114 unique PNG；無新增replay |
| actual CLI negatives | **9/9預期拒絕**：missing role、swapped role root、錯traceSHA、缺file binding、錯parentSHA、擅改budget、float schema、existing output、missing manifest；非existing output皆無新report |
| variant gold failure | 未重跑X2 variant suite；父X2的243/244與原crossing非零exit／gold原樣保護，不稱本輪通過 |
| 其他 | 未做ASan／Debug／全repository重跑，不提出report效能提升；既有fixture timing logs保存，但不作本輪live benchmark |

最初3次build失敗（JSON比較／宣告型別、insert overload、同一行GTest macro）與後續修正log全部保留；首次report因C36h無optional projection欄位而失敗，未產output，改用當幀current valid selected ID／observed time，沒有補造schema。初次CLI missing test被外層mutex搶先拒絕的覆蓋缺陷已修正，verified重新跑13項；原初次log保留。兩次只用於顯示資料的PowerShell／rg查詢錯誤另記註記，沒有改輸入。首次Finalize因status路徑分隔符排除失效而誤列已授權文件；J1–J7歷史比對當時已通過。原source／preservation／ledger／summary保留為失敗attempt；修正後`verified`清單0 mismatch、exit0，沒有重建binary或重跑report掩蓋。見 [finalization-notesverified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/finalization-notesverified.json)。其他attempt詳 [attempt-notes.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/attempt-notes.json) 及所有 `*-command.json`／logs。

保護核對：繼承X2開始前的X1 protected **883次檔案檢查**、X2 **25 binary／DLL、159 export、23 source snapshot**均相符；另本輪開始snapshot的formal／既有工作區／X2artifact核對見 [preservationverified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/preservationverified.json)。唯一有意修改的既有工具檔是offline CMake可選target，before／after SHA列出。status從C36h baseline段落到J7的原文逐字保留，允許top update／J8 append。

這些是檔案核對次數，不是獨立證據數。原X1 **130,356,292B**、X2 **52,804,100B**不變；所有失敗／父驗收／未知分母保留。没有刪raw、移出batch、覆寫父ledger／frozen binary或擴充原容量上限。

## 9. 最終容量與验收狀態

預估兩份6MiB report＋4MiB logs／metadata，共16MiB，低於32MiB。原 campaign **7,814,889,388B**＋prior research **45,307,809B**＝**7,860,197,197B**，原剩729,737,395B；新增batch開始前不存在。

最終量測：X3 **6,950,299／33,554,432B**，剩 **26,604,133B**；campaign＋prior **7,867,147,496／8,589,934,592B**，剩 **722,787,096B**；`out/x3` build **120,638,741B**另列。帳本自身25,089B、final-summary自身377B已包含；新增export=0。完整item bytes／SHA、ledger與final-summary自身長度見 [capacity-ledgerverified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/capacity-ledgerverified.json)、[final-summaryverified.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/final-summaryverified.json)；`out/x3` build另列，無新export，原PNG／trace未複製。

**交付狀態：開發自驗通過、待總控獨立驗收。** 未解：physical role／ID gold、未輸出的exact confirmation／replacement events、original perception cadence／早期state、source age、真recognition／RPC、game adoption、77 Miss逐Note因果与跨曲效果。本輪完成證據研究與最小工具，不宣告Miss改善或production驗收。

## 10. 總控獨立驗收（2026-10-02）

**通過，無阻擋項；驗收範圍為唯讀離線report與研究證據。** [驗收summary](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/acceptance-20261002/acceptance-summary.json) SHA256 `75ae973b9003432493c0096b5c2d281669a27953b2a4e708f5e5b054d4c88fd6`；完整檢查與邊界見status J9。

總控實跑X3 13/13、C36h 213/213、main50 control 244/244，fail0／skip0；獨立重算兩份report，各3,034,295B，與§3交付SHA逐byte相同；9個CLI負例均按預期拒絕且原輸出不變。審查實際source、原tracking／owner分支，重新核對來源／binary／frozen artifacts，另目視原6160、6174、6211、6214。沒有重建、重跑full replay／ASan／variant或live，沒有需要校正的交接事實。

本次確認灰色水平線在RGB仍有支持，但沒有證明唯一detector失敗分支；measured secant不等於有效motion model，continuation重建也不等於未輸出的內部event。§7仍是下一個單機制研究提案，尚未派送或實作；不得同时提供identity oracle、解除conflict或復活completed contact。

總控新增6,500,739B後，X3為13,451,038／33,554,432B；campaign＋prior為7,873,648,235／8,589,934,592B。§9原帳本保持原樣，新summary包含驗收資料及自身長度。C36h baseline／main50 donor定位、§9 Unknown及非gameplay驗收邊界不變。
