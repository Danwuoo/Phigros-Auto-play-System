# 第三階段：冷測契約釐清與最小側軌／端部修補

2026-10-05。修補起點：`acb27fdb095b82253ef9388eab52948a59663d02`。本輪已獲准在雲端修改冷測候選、相關schema／fixture契約與測試；**修補完成於獨立research候選，尚未整合正式pixels→touch鏈，也未取得live資格。**

## 1. 結果先說

- 原frozen **356 layer-cases／3938 assertions**，同一oracle的failed assertions **54→43**；修復11項、沒有新增原suite失敗。原54項逐列before/after全部保留。
- 新v2契約 **48 cases／339 assertions／0 fail**，包含真正容量API、raw與eligible的正反例、rails／端部／merge控制。這個新綠燈不改判原43個紅字。
- 獨立renderer原31例中，27個可裁定case全過；合法action輸出 **8/9→9/9**，安全組不當Move **7/14→0/14**。4個decision-only不計pass。追加3個merge正控制全過；duplicate ROI的既存過拒仍有2項fail，所以完整獨立 **35 cases／79 assertions仍exit1**。
- Release、Debug、ASan＋UBSan各組结果一致；LSan明示關閉。沒有將合成輸出換算成實戰Miss下降。

最終候選位於 [`research/bvi_cold_v2`](../../../../research/bvi_cold_v2/README.md)。`bvi.cpp` SHA256：

`232bb808e0e21b3b25e5e85995f8d0333597f446b8f867993535b84b40543e81`

原donor、六份frozen inputs、原oracle、Windows工具與STOP不變。沒有改正式owner、scheduler、擷取、模型或遊戲操作。原章節分母／當前解鎖仍unknown；本輪沒有新增任何IN結算。

## 2. 實際改了哪些程式

| 最小改動 | 規則與理由 | 沒有做的擴張 |
|---|---|---|
| 當前側軌見證 | 每側至少兩個連續depth、不同實際pixel center的white＋同深度內側blue，且white在當前measured-line mask外；失效depth重置run | 不讀renderer的rails flag、不按case／歌名特判，不從過去幀借rails |
| 有界端部轉換 | front/rear內外各搜尋1..4px，分別要真正bilateral blue／black與有效雙rails；white不能補成blue或black | 不降低3獨立／30ms，不改dedup、不把完全遮住的端部補成可見 |
| 多候選merge歧義 | 原`contained>=2`不再被current front_end豁免 | contains、距離／角度／寬度、6份／90ms、nearest tie與constraints不變；沒有新增矩形投影或物理owner判定 |

新目錄保留同API與bounded metadata；舊研究source不覆寫。完整diff、每階段source snapshot和作者控制見 [CANDIDATE_PATCH.md](CANDIDATE_PATCH.md)。

### 2.1 修補中找到的新過拒，沒有藏起來

P1原先也要求endpoint的blue／black見證在2.8px幾何mask外。獨立共轉.25案例P05卻找到實際blue像素距line=2.7826466，因mask餘裕被排掉，front_end=false。這是本輪新增的過拒；初次失敗仍保留。

P1.1只取消endpoint對真blue／black的幾何mask veto。blue／black與white predicate互斥，因此不用把實際可見顏色當白線；**white rails仍排除line mask**。獨立P05原gold未改，最終通過。

P1已修好原V07無rails union，但獨立N09的兩段有真rails Hold合為帶真cap union時仍錯Move。凍結反例及「單一延續／兩物件仍分離／僅含一個prior」正控制支持P2，再只去掉cap豁免。三個正控制均保留Move，不以全面拒絕求安全。

初版31-case摘要曾少計P05；完整原始報告是3 fail（union兩項＋P05一項），加duplicate等controls後是5 fail。最終完整35例仍留下原本就有的duplicate2 fail；未將早期報告改成綠結果。

## 3. 契約版本化，原43個fail不被新名詞洗掉

[CONTRACT_V2.md](CONTRACT_V2.md)記錄先凍結規則及有證據支持的endpoint-r2修訂；[CONTRACT_CHANGE_MAP.json](CONTRACT_CHANGE_MAP.json)逐一保留原54個expected／actual／pointer與新條款。

| 原群 | 原failed | 本輪原oracle結果 | 新契約怎樣處理 |
|---|---:|---|---|
| C1 白線假rails／merge | 5 | **5修復** | 真rails、無rails＋白線、單白點／間斷／單側、union與合法延續皆有正反例 |
| C2 固定端部探針 | 6 | **6修復** | 4px內真色轉換；共同旋轉正例檢出並修正P1過拒 |
| C3 透明effect | 2 | **仍失敗** | 原yellow predicate未放寬；RGBA89合成色語義仍需裁決 |
| C4 斜交旋轉contact | 7 | **仍失敗** | 固定flank contact未改；共同旋轉通過不替代body .25／line .4斜交能力 |
| C5 容量只宣告未施加 | 24 | **仍失敗，原fixture／oracle保留** | 另驗真129 ROI／17 lines API拒絕與128／16正邊界；helper、ABI與動態probe分母分開 |
| C6 invalid-body payload | 8 | **仍失敗，原typed adapter未改** | 新cold readout分raw診斷與eligible支持，invalid動作全拒；合法正例必須有真hit／Move，不能永遠false/null |
| C7 ambiguous hit順序 | 2 | **仍失敗，原whole-output比較未改** | 新eligible hit在歧義時null、action不變；raw hit仍留debug，沒有宣稱原診斷決定性已修 |
| 合計 | 54 | **11修復／43保留／0新增原suite失敗** | 新suite另列48／339，沒有替舊紅字改gold |

動態probe真正耗盡未測；metadata/probes helper邊界通過不冒領動態壓力覆蓋。新eligibility projection只是冷測消費視圖，沒有接上正式owner。

## 4. 全回歸的分母與逐階段結果

### 4.1 原frozen suite

六份inputs、driver與oracle保持相同bytes，沿第二輪已明示的typed-singleton IO adapter執行；輸出改用全新round3身份，不重開舊attempt。

| 階段 | 原assertions | failed | 變更範圍 |
|---|---:|---:|---|
| Donor重新執行 | 3938 | 54 | 完整報告SHA與第二輪相同 |
| P1 rails＋endpoint初版 | 3938 | 43 | 原11項修復；獨立新正例另找到mask過拒 |
| P1.1 | 未另跑此中間點 | 不填推測值 | 新v2契約有獨立中間點，不假稱完整legacy也跑過 |
| 最終P1.1＋P2，Release | 3938 | 43 | 11原fail修復、0新增fail |
| 最終Debug | 3938 | 43 | 與Release逐byte相同 |
| 最終ASan＋UBSan | 3938 | 43 | 與Release逐byte相同，無sanitizer診斷；LSan關閉 |

最終各層：RGB 12／490、typed 9／589、lifecycle 1／1331、e2e 21／1495 failed。22 supplemental、4 schema negatives、3 renderer及4contact controls仍通過；wrong-contact預期native1且consumer確實拒絕。整體原suite native／aggregate仍1，不能標全綠。

最終原suite完整JSON SHA：

`a3479413c03dcb82170793dce707133dbd04e90693dcbd798fea7e69eeec9136`

[逐54項結果映射](evidence/legacy-final-delta.json)保留before／after actual、舊expected、仍失敗／修復；新增fail清單空。原literal expected全相同。14個metamorphic assertions的報告`expected`是driver即時計算的reference-case輸出，因此隨候選修補改變；其**oracle參照規則未改，兩側仍通過**，詳列在同一delta檔，不把這種materialization說成改gold。

### 4.2 新v2契約suite

最終同一 **48-case／339-assertion** 分母：donor23 fail → P1 5 → P1.1 4 → final0。原47／321的第一版結果也保留，不跨不同分母作百分比比較。

| 類別 | cases／assertions | 最終failed |
|---|---:|---:|
| 真API容量 | 5／34 | 0 |
| 容量helper，非動態耗盡 | 4／4 | 0 |
| ABI static bound | 1／1 | 0 |
| typed invalid payload | 8／112 | 0 |
| eligible正例、reset／receipt | 8／74 | 0 |
| ambiguous projection正反例 | 3／27 | 0 |
| rails／endpoints | 15／70 | 0 |
| merge correspondence | 4／17 | 0 |

Release／Debug／ASan+UBSan均exit0，JSON逐byte相同：

`8ed705e47ae4bd701187e39288995df0f9079bb91d789fb0cd4913a553dc223f`

主控亦以新runner另編譯／執行Release核對。新suite不讀候選輸出來產生gold；typed eligibility投影不回饋策略。細項：[contract-v2驗證](evidence/contract-v2/validation.json)。

### 4.3 獨立正反例

先凍結31例，再讀candidate；追加4個merge controls在看到P1之後另凍結，沒有冒称全部35例都是盲測。兩批case及expectation hashes都有保留。

最終：合法action 9/9、安全拒絕14/14、3 permutation、1幾何正例；4decision-only不计pass。追加3merge正控制通過，duplicate ROI既有2fail保留。原31例71assertions的評分項通過，完整35／79仍exit1。

**合法動作欄位是8/9→9/9，不是1/9→9/9。** donor只有1/9「整個case所有診斷斷言」全過，另8個有些只是cap等欄位失敗，不能用這個數誇大動作改善。安全組7個不當Move降到0，亦只限合成fake constraints。

三配置JSON SHA：`bca8d2322f9c32ab01b8b049c4265f1acc5b43b040e5ad57dd090945c64eee59`。主控另編譯Release的結果亦相同。完整解析：[INDEPENDENT_REVIEW.md](INDEPENDENT_REVIEW.md)。

## 5. 成本、安全與仍未跨過的門檻

- metadata仍185904 bytes，沒有改資料結構或歷史容量。
- endpoint從每Hold8個pixel reads增至32個，最多增加24；本suite最高probe counter由555648到558720（+3072＝128×24）。這是**合成輸入計數**，不是p99 runtime或最壞CPU時間；rail mask的有界幾何計算成本也尚未做正式負載量測。
- 新／舊source、oracle、case覆蓋、unknown/completed及期限控制均分開保存；constraints、owner、scheduler與真注入沒被重寫。
- P2仍沿用nearest tie，duplicate／near-duplicate ROI可能過拒；完整獨立報告保留2fail。
- stationary Tap＋移動line的新需求仍只有independent1，Down未取得；本輪沒有降低確認條件。
- V10斜交contact、V04效果定義、極短body與dynamic probe耗盡都還有明列限制。相同可觀測輸入不提供physical owner gold。
- 缺真圖合法ROI/all-lines橋接、完整owner／FakeTouch接入回歸、有效候選成本、Windows build／preflight和遊戲回饋，因此**不能採用到正式live，更不能稱Chapter Legacy全曲zero miss**。

## 6. 此階段完成與下一個有界門檻

本輪按核准範圍停止：已有版本化契約、最小code patch、修補前後證據、獨立正反例與完整回歸。没有繼續為全綠擴張dedup、旋轉contact、effect、owner或process-control。

推薦下一個門檻分兩條，不混成無限重構：

1. **先處理仍阻礙合法新需求的可見性／關聯契約**：stationary相對運動、duplicate ROI、斜交contact；一次選一組，先凍結正反例與機會／安全分母，再改code。V04效果定義另裁決，不能擅自放寬color threshold。
2. **對已修局部機制補真圖證據與成本／owner接入**：只需能回答rails／端部的精選原圖、合法ROI/all-lines來源與必要contact前綴；不要先要求全量錄影或大模型。通過exact候選的行為／成本及Windows gate後，才另行授權有限live。

目前已提供的小包不需重傳；原章節N與逐曲IN解鎖依然要當前畫面核對。HD只用必要解鎖／回歸，完整IN Miss=0才是產品結果，沒有AP前置。

重跑命令、返回值與交付校驗見 [VALIDATION.md](VALIDATION.md)。本地commit與ZIP不等於push／PR／merge；未執行裝置、遊戲或付費運算。
