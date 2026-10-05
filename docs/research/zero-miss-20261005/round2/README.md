# 補件後更新：BVI完整冷契約已可執行，但尚未通過

2026-10-05。接續研究 commit `ebe955a6286ca1bb767b631948714a5545b7c174`，正式產品／frozen source 基準為 `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。

**這次新增的結論不是「再確認尚未成功」。小證據包已讓我們跑到真正的完整 BVI 候選 assertions：修正隔離 IO 的單線形狀相容性後，356 layer-cases 全數執行，3938 assertions 中54失敗。Release、Debug、ASan＋UBSan與另一輪獨立Release結果逐byte一致。BVI仍未通過冷契約；失敗中又必須區分測試輸入／診斷欄位與真正候選行為，不可把54當作54個遊戲Miss或54個產品bug。**

本輪原封保留core、driver、oracle、六份frozen inputs、正式產品碼、既有tests及舊STOP。只新增研究文件、可重跑驗證及明示的隔離IO adapter；沒有遊戲／模擬器／真觸控、訓練、使用使用者電腦、push、PR或merge。這是補件後的研究收束，不是zero miss定案。

## 1. 先前結論 → 新證據 → 更新判定

| 首輪結論／未知 | 補件與本輪實驗 | 更新判定 |
|---|---|---|
| BVI只能編core、28個新smoke；原frozen inputs缺席 | 五個ignored runner inputs已齊且原bytes/SHA保持，第六supplemental在Git；使用相同pin的官方JSON版本與port patches | **完整suite的資料阻塞解除**。不再需要為此索取整包out、SDK或全部PNG |
| 不知原完整harness是否可跑 | 只改新輸出reservation身份後，原harness在新增R1第一例拋JSON type305、exit2 | 找到新的**fixture/IO schema不相容**，不是candidate assertion失敗，也不是Windows quoting造成 |
| 僅知道20個新R1案例source存在 | 159個新typed frames中156處lines為單一object；均與對應RGB的單一line逐值相同。隔離parser把合法單線object讀成一條Line，原檔不變 | 加明示shape adapter後可完整執行；**不是未適配的原suite原封通過** |
| 原356 layer-cases從未實跑，無法判候選冷契約 | 89×4=356全執行；3938 assertions、54fail；四種編譯／重跑同報告SHA | **候選尚未滿足其既有冷契約**，不只剩建置封裝。原69例中19例涉及失敗，新20例無失敗 |
| 全部紅燈的因果不明 | 獨立逐列source＋fixture語義審查 | 24項是RGB並未實際施加宣告容量、8項typed invalid的body欄位、2項ambiguous hit順序；其餘還有color／geometry規則與oracle張力，不能盲改54項 |
| Dlyrotz IN M77只持有tracked轉錄 | 實際看去識別結果圖，並核同輪manifest／summary的original→derived映射 | 此**同一歷史樣本**數字已直接覆核：795950／P496-G11-B0-M77，未新增一輪，也未全驗其餘88輪 |
| Windows configure exit1只依歷史文件 | A9原樣result／verification／stdout／stderr可核，三exit=1/1/1、root launched且乾淨收尾 | 程序確有執行並失敗；**具體offending step仍未知**，不能把D19/D20斜線當cmd根因 |
| D19/D20 path失敗僅源碼推斷 | D6完整oracle/pretest配對18/20，D19/D20=`transaction-root`、native0 | 第一失敗層已證實；另發現若修入口後走到FinishDiagnostic，仍可能讀錯全域state根 |
| 全曲分母、現在逐曲IN解鎖unknown | 包內沒有當前完整章節／版本／IN可選畫面 | **維持unknown**。本包不證明任何新解鎖或zero miss |

證據入口：[包完整性與邊界](INPUT_AUDIT.md)、[完整suite與重現](BVI_FROZEN_SUITE_UPDATE.md)、[獨立失敗語義分類](BVI_FAILURE_SEMANTICS_REVIEW.md)、[Windows收據](WINDOWS_RECEIPTS_UPDATE.md)、[圖面與provenance](RESULT_PROVENANCE_UPDATE.md)、[獨立重跑與交付核驗](VALIDATION.md)。

## 2. 本次執行結果，不能只報一個綠／紅燈

### 2.1 原harness的新增阻塞

Linux已成功編譯原 `bvi.cpp`／`driver.inc`及只換新輸出身份的 `main.cpp`。wrong-contact負例實際native exit1、兩個expected4/actual1的failed rows，consumer拒絕成立。

完整suite卻在 `/69/typed_frames/0/lines/angle` 以type305中止：parser以為lines是array，實際拿到object，迭代到angle數字再索引字串欄位。原69例的217個typed frames都是array；新增20例159frames中156個單line被序列化為object，其餘為array。這不是傳檔損壞，五份輸入均與frozen SHA一致。

隔離shape adapter只在具有descriptors、且lines恰有center/id/angle/length的單一object時讀成一條Line；array路徑不變，不改parsed JSON、oracle、核心或driver。156個object均與對應RGB的單線相同。原exit2保留；原PowerShell generator／正式main並未被本輪改動。

### 2.2 適配後完整分母

| 層 | cases | assertions | failed |
|---|---:|---:|---:|
| RGB | 89 | 490 | 13 |
| typed | 89 | 589 | 9 |
| lifecycle | 89 | 1331 | 1 |
| e2e（此harness內的RGB到fake constraints） | 89 | 1495 | 31 |
| 四層合計 | 356 | 3905 | 54 |
| supplemental、schema、renderer、contact adapter controls合計 | 另列control分母 | 33 | 0 |
| 全部assertions | 不混成曲目數 | 3938 | 54 |

- 22 supplemental／4 schema negatives通過；三組renderer等價control與四個contact adapter checks也通過。
- 1128 coverage rows／2392 layer references中，`unverified_oracle_fields=0`，`schema_errors=0`；這表示宣告要檢查的欄位均有執行，**不表示各fixture真正施加了想測條件或行為正確**。
- 54fail涉及19個原案例、31個layer-cases；新20個R1案例四層都沒有fail。不能只報新修補例全綠而漏掉原例回歸。
- GCC ABI metadata=185904 bytes，suite內最大probe counter=555648。這不是Windows ABI、最壞場景成本或正式即時鏈latency。
- 各模式的suite native exit都是1；最終研究runner aggregate exit也為1，不把成功收集失敗報告寫成suite pass。

完整suite JSON在Release、Debug、ASan＋UBSan（LSan明示關閉）及獨立Release中均為：

`8fbc37e8db4afe1d8736b2f89c17f25dc91d2213581d241279c6ab2dbda38611`

Sanitizer執行未出現Address/UB診斷，但契約仍54fail。**不是sanitizer版測試全過，也沒有LSan證據。** 編譯模式、依賴pin、每層原exit、原始錯誤及adapter diff都在[BVI更新](BVI_FROZEN_SUITE_UPDATE.md)。

## 3. 54個fail該怎麼用，哪些結論必須修正

### 3.1 至少34項不應直接改動作策略

- **24項V16 RGB/e2e**：fixture只在`declared_usage`宣告超容量，實際仍傳1 query／1 line且未達metadata/probe界限；extract看真輸入，不看那個宣告。這些case沒有測到想測的容量拒絕。既有supplemental中的actual-capacity控制另有通過，不能從此倒推core容量guard失效。
- **8項V16 typed**：adapter把observation標invalid，fake constraint確實不Move／refresh且release；只是原body payload未清掉，`current_body_preserved`期待false。應裁定「可見診斷payload」與「可用資格」的欄位語義，不以刪光payload或放行動作來追綠。
- **2項V18 line-order**：ambiguous兩線的診斷hit會因最後遍歷line不同而改變；兩側均不Move。可修診斷順序不變性，但不是已驗的錯觸。

### 3.2 真正值得優先隔離的視覺鏈

**V01／V07，共5項：白判定線污染Hold側軌與端部資格。** fixture最後畫面明列rails=false，判定線卻穿過左右rail採樣位置；目前跨depth的white OR把該線計成railLeft/right，再產生不該有的front_end。V01因此允許Move；V07的merge歧義又被假的front_end抑制。這是一個可定位的最早失效層，值得做最小當前像素支持修補／反例，而不是先改owner grace或學習模型。仍不能從合成案例宣稱已定位真實77個Miss。

**固定端部／旋轉探針和判定線的交疊。** V02／V17有6項不符先發生在front-end觀測：固定-4px探針落在白線，導致cap不存在，後續independent／usable不成立。這不能作為40ms gap或dedup本身壞掉的證據。V10旋轉也有固定雙側探針踩白線而拒contact的情況；literal bilateral-blue規則與oracle期待supported存在張力，應先明定遮擋／可辨支持，而非擅改oracle或一律把白色當blue。

**V04效果判定。** alpha混合後像素並不符合frozen yellow閾值；兩個effect fail不等於「程式漏掉已符合規格的yellow」。這是fixture／規則／oracle須裁決的一部分。完整逐項數量與source座標見[獨立語義審查](BVI_FAILURE_SEMANTICS_REVIEW.md)。

### 3.3 首輪哪些假說保留，哪些不再能拿來解釋新結果

- 首輪「線追靜止Tap時，內容去重使新Down無法取得資格」是另一組已跑typed／RGB的獨立反例，仍成立於它的條件；**不可把本次V17的cap漏辨拿來加強同一dedup歸因**。
- source age未校準、串行五指尾延遲、Flick backlog仍是已重現的機制假說。本包没有逐次RPC／原events／render-time校準，沒有把它們證成實战主因，也沒有反駁它們。
- 新C4的playing interval n=7284、p99≈43.9468ms、max433.1117ms，是owner消費latest decisions的aggregate間隔，含skip；不是直接的render/capture callback分布。不得因host residency較小就把未知source age設為0。
- 仍不能由同一張結果圖把19份IN變成19首IN、把20個歷史曲名變成完整當前分母，或把一次M77當現行版本／帳號的全部狀態。

## 4. 更新後的最短路線

下列是下一個有界實作／驗證包建議，本輪未改正式碼；不是自動恢復live或整套開發。

| 優先 | 最小下一步 | 可驗收的停點／依賴 |
|---|---|---|
| P0a | 把新R1 typed-lines的schema不變量放在input產生／載入入口，明確保留array；另保留本輪compat adapter作研究 | 原69＋新20完整枚舉；錯型別在candidate前有清楚case/path；原frozen檔與這次exit2不覆寫 |
| P0b | 逐一裁決24容量、8invalid-body、2ambiguous-hit以及color/旋轉的契約差異 | 每case說明實际施加條件、觀測／資格欄位和oracle版本；新版本可新增真正負例，不刪舊fail追pass |
| P1a | 先處理V01/V07最早可見支持失效，再測端部與旋轉交疊；一次一個通用機制 | 真rails、無rails＋判定線、分離兩Hold→union、line/region順序、rotation、unknown/completed等均有正負例；同時報合法機會與錯放行 |
| P1b | 對照首輪stationary新需求、pending/capacity/expiry、Flick串行假說，僅在能區分首次失效的位置補測 | 不按曲名調參；不從窗中body捏造已Down；不把少Down/少cancel等同Miss改善 |
| 並行P0 | Windows最小修復：canonical scratch、transaction-local state readback、完整INIT；保留成功process-control核心，再用marked wrapper定位cmd失敗步驟 | 新attempt／exact source，舊STOP不重開；D19/D20完整交易可達且負例仍拒；真正Windows configure/build才能另簽 |
| 其後 | 真圖合法current ROI/all-lines橋接、完整owner回歸、同候選有效成本、exact freeze／五指preflight | 此次fake constraints與RGB renderer不提供真owner／遊戲採納；cold未過不能直接live |
| 產品驗收 | 凍結當前遊戲版本／完整Chapter Legacy N與逐曲IN狀態，必要才HD解鎖，再同版逐曲完整IN M0 | 目前N/解鎖仍unknown。基礎每曲一次M0，額外3次／跨2sessions穩定性提案仍待凍結；不加AP |

這條路線比「先把Windows wrapper修好就直接上機」更準確：Linux已可提早檢出候選契約與fixture問題，不需等待Windows才能判讀它們；Windows的工程與runtime資格則仍不能被Linux結果取代。

## 5. 還需什麼資料，現在不必再提供什麼

**已不再缺：** 本次完整BVI runner的五份ignored inputs、A9失敗收據、D6診斷收據與這輪C36h結果圖。不要再重複要求這包。

**針對後續問題才需要：**

- Windows唯一syntax根因：未去識別原件在本地的有界marked probe／實際commandline／各step exits，需另外授權的Windows工作；不要求傳整個SDK或私密環境。
- M77逐Note／owner因果：選定問題所需的pixels＋完整接觸prefix／events；本包未附31MiB事件或X1，不能假定近期90ms就恢復長Hold。
- 真正全曲產品分母：當前遊戲版本畫面、完整章節列表及每曲IN可選／鎖定提示；不需帳號、存檔或憑證。

目前可以做的冷修補／契約裁決不應被全部raw、所有193skip歸因、大型標註或模型訓練綁住。反過來，也不能因雲端fixture可跑，就省掉真圖、owner、成本和逐曲完整IN驗收。

## 6. 重現與交付界線

- 原證據ZIP／解包檔只在repo外使用；新commit不納入原PNG、原收據包、JSON單header、binary或build cache。
- [BVI執行脚本](evidence/bvi-frozen-suite/run_frozen_suite.sh)要求原包解壓根、repo與明列JSON依賴；先核全部SHA，使用新輸出根，拒絕覆寫。完整suite目前應得到**非零**，不是安裝後應全綠。
- [Windows收據核驗](evidence/windows-receipts/audit_receipts.sh)只做bash＋jq的file/hash/receipt維護，不執行提供的cmd。獨立重跑結果與交付JSON逐byte一致。
- 第一輪研究文檔與原始結論保留為歷史；本輪在新子目錄增補，不重寫舊raw、舊測試或舊STOP。查核清單與result-provenance摘要均明列known/unknown。
- 本輪結果足以更新工程研究判定，**尚不足以宣布BVI可採用、zero miss路線已定案、全曲已解鎖或產品完成**。
