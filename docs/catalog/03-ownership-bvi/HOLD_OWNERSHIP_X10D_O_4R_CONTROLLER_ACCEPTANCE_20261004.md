# 4R 總控獨立驗收與 4I 冷實作契約

2026-10-04，Asia/Taipei。**4R 研究／設計方向驗收通過，授權接續一個 BVI-1 隔離冷實作包4I。** 通過的是有真圖依據、能被反例否決的觀測介面方向；20組vectors仍為未執行設計，需先具體化為一致的測試輸入，沒有演算法或觸控候選已通過。BCC-v1仍否決，原測試／反例封存不改。X12仍not-ready，無live授權；C36h tint1 baseline、main50 donor/live0、suppression OFF及P不混入不變。

4R chat `01a106c0-c171-73b1-87ea-6c35a1711b80`「審查 X10d-O 時序可觀測性與契約」。原 [研究](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_20261004.md)、[設計JSON](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_DESIGN_20261004.json)、[交接](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_HANDOFF_20261004.md)與原batch保留。本頁接續 [O總控驗收](HOLD_OWNERSHIP_X10D_O_CONTROLLER_ACCEPTANCE_20261004.md)，並明列設計轉實作前的必要澄清；不回寫原交付來掩蓋差異。

## 獨立核對

總控以新維護 [check-4r.ps1](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability/controller-review/check-4r.ps1) 重 hash／核bytes，未執行開發的finalize／audit，也未跑任何產品binary、build、tests、ASan、full replay、runtime、成本、emulator或觸控。

[integrity.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability/controller-review/integrity.json) 記 **1779個不同檔案、1843項hash引用核對，0不符**：包含1762份受保護檔、4R交付與13個source references等重疊組別。原450個Git路徑保留，只有預期3份新docs/JSON，共453；正式src/include/root CMake無diff，HEAD與index一致。沒有宣稱全面再驗所有ignored raw或SDK。

52份選圖皆與原O packet及原7722列recording index的ordinal/source frame/兩個host clocks/source timestamp/PNG SHA相符，來源檔hash亦相符。原交付容量重算 **660941B**、aggregate **8275074850B**，與修正後final receipt一致；初版漏計外部三檔的收據保留但不採用。總控新增資料另計4MiB預留，不修改開發final receipt；settled數字見 [controller receipt](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability/controller-review/controller-receipt.json)。

總控本輪直接目視14張：H2468–2471、2477–2479及K2879–2885。H由前緣近線到小角度變化，外rails與body仍可見、下端疊effect；K前緣由約503移向576線，2880–2883仍有間隔，2884到線。支持4R的可見描述及「上方body不能當原接觸區的當前支持」，不提供521/539/682/684的physical gold。另沿用上輪直接看過的2865分離body作背景；本次沒有冒稱重新目視全部52張。新增human gold0。

獨立讀12個X10c原trace ordinal的 [精確欄位摘錄](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability/controller-review/trace-excerpts.json)：H2470的521/156為cursor3、prefix1及contact1位置(278.8094,599.1934)；2478另有contact0，2479該contact消失而contact1持續。K2883的682/194更新至(781.5,559)，2884回576；2885另出現684/197及contact2。2882 consumed=false、scene=null；其中仍保存owner/contact舊狀態，不能把它當該幀新observer結果。2478原capture23126393917400與replay43357925700、2881原23133341705500與replay50305713800均一致，原點不同，不作延遲相減。

**總控更正一處用語：** 4R研究§3稱2887「該身份消失」，精確事實是contact2不再活動、684/197的cursor為null；owner.identities仍保留684/197紀錄。這不改變新front／歸屬仍unknown的研究結論，但禁止後續把它當identity刪除、完成或可重播Down的證據。

## 設計判定與必要澄清

兩個新增資訊有明確區別：A量當前端部／背景分界與contact-zone支持，B只保存≤6份／90ms的已觀測分界延續。它們比原anchor幾何多了可否證的像素來源；V00/01/19保留同輸入不可能推唯一physical世界的限制。這足以進一次小型冷實作，不能保證真H/K修復、避免所有重複Down或降低Miss。

原11項逐項解讀正確，尤其Gap只要求depth上界、原rotation各幀anchor同pose、late alignment原case僅一幀。新契約保留支持／Move／incoming／Tap正例及unknown/completed/return負例，不能全unknown換綠燈。BVI的physical欄只能proposed/unknown；其contact欄在本次接續中只作測試約束，沒有注入權限。

總控手核幾何與容量：V03三段gap相對y500為[-50,60]、[-20,80]、[0,120]；V10三個候選點位於body局部切向±70、法向[-200,0]內，距當前line法向誤差小於0.0004px，僅支持幾何一致。V15 rear=front−depth得492/508/512，能描述幾何尾端兩次過線，不證真實tail語義。宣告metadata各項合计293376B，距1MiB尚755200B容納其他結構；128×4096×12＝6291456 probes。這是設計算術，不是sizeof／RSS／可接受frame成本。

以下須在4I實作前另存明確規格與oracle，屬本次通過的邊界：

1. **輸入與oracle分離。** `measured_contact_regions`、`current_support_points`、`association_unique`、`feature_independent_cap`等欄須逐一標為合法的上游測量、test-only oracle或renderer私有資料。不能把預期的支持／端部直接餵給pixels extractor，再宣稱由pixels識別成功。typed link／lifecycle測試与RGB→descriptor測試分列；renderer知道的layer、physical label及遮擋底圖不可洩漏給candidate。
2. **渲染、可見性與oracle具體化。** 冷合成先凍結rail/cap/line寬度、RGBA合成／整數取樣、draw order、裁切、ROI、接觸区及endpoint/gap的測量定義。line畫在body前會遮住交點顏色，不能讓「幾何在body內」自動變成當前可見pixel支持。opaque遮住底圖時底下body未知；occluded只能是有當前外觀證據的遮擋候選，不能讀renderer標籤猜真相。保留同觀測的countermodel。
3. **時間與mutation無歧義。** V12繼承V03時，原`unknown_down:false`與新state/receipt unknown的優先順序未完整展開，須建立單一一致execution-state表示或明確拒絕矛盾輸入；不可因booleans碰巧讀取順序重試Down。V16逐一寫清「當前frame改context、history仍舊context」，不是把整組合法新epoch改名就期待invalid。V17補完整timestamp/sample序列以真正到达90ms／40ms／6samples及+1邊界。plan與gate期限、now、source frame均展開。
4. **不同層的失效不得混為一談。** line關聯歧義可使Move／新需求資格失效，同幀body是否可見仍可量；V08不得因第一條line失效而把支持全部清空。沒有新端部確認不等於沒有當前支持：duplicate RGB／相同descriptor的去重定義與freshness分列，靜止Hold仍可有當前支持，但重複資料不能湊足3份獨立分界證據。若不用past RGB ring，須說清以何種有界signature／來源key判重及碰撞限制，不冒稱精確全RGB去重。
5. **保留機會與語義缺口。** V03/09/11是observed incoming/Tap機會，不能由cap直接升格physical新Note；V15兩份rear過線本包只驗幾何觀測，正常Up及遊戲tail語義仍待owner整合契約。不能借本設計刪raw/direct front、吞獨立incoming、放寬grace/alias，或復活suppression。4px、12px等是預声明合成界限，非已校準遊戲參數。

20個頂層vectors中含多個mutations／metamorphic cases，不等於「20 tests已通過」。上述具體化另存4I映射，保留4R原文；如發現不可滿足的同輸入期待，停止相關claim并交反例，不改期待迎合candidate。

## 授權工作包4I：BVI-1 觀測介面隔離冷實作

依使用者既有順序派送授權，使用 **GPT-6.1 Sol／xhigh**，一個開發chat。總控負責本驗收與派送，不實作產品。4I目標是驗證「新的像素部位觀測與有限時序摘要能否做到設計所稱的區分」，不是宣稱可靠ownership已解決。

**先規格、再唯一候選。** 先保存normalized vectors／oracle及4R差異對照、來源／dirty／容量與bounded command protocol；完成上述五點，才實作唯一BVI-1 family。允許新 `research/x10d_o_bvi/` 自有C++20觀測／有限link實作、獨立renderer/oracle及fake-clock harness，必要維護工具另存新路徑。formal src/include/root CMake、原apps/tests、O/4R封存與out不修改；不抄原candidate的exclusive-owner输出，不接observer/owner/scheduler/actuator，不做P或B混合。優先最小standalone target，只連必要已安裝依賴，不重建整個pas或複製整個C36h export。

**驗證交付。** 分別報RGB→descriptor、typed link/context、lifecycle constraint harness，不以typed case取代pixels辨識；fake lifecycle是新測試harness，不叫既有owner整合通過。先保存獨立正負oracle，含原same-input反例、完整rotation／late-alignment序列、supported stationary Hold、遮擋／同觀測反例、incoming/Tap機會、兩line歧義與order/ID invariance、所有clock/capacity精確邊界。至多一個預声明算法族；明確反例足以否決核心claim時停止，不做門檻／家族搜索。普通coding錯誤可修但保留失败，不調oracle救結果。

必要Release及可用的Debug/ASan在新out冷建置／測試；全部命令有時限、輸出／容量上限、exit及owned process身份與收尾證據，先凍結契約。編譯並行最多2，每命令最長300s，單stream log≤4MiB，保留失敗。執行工具至多一次工程修補；再遇不可可信收尾便交partial，不重啟A guard/vctip/OS平台支線、不對原gate改門檻。若ASan不能可信完成，明列未驗，不以EXE存在當通過。完整source／test／tool／binary／實際依賴／DLL及snapshot需可供獨立驗收；依賴覆蓋限制照報。

若合成契約支持繼續，可另做**至多兩次**有界只讀原52PNG觀測審核，失敗亦計次，事先凍結driver/packet／輸出schema。用既有最小PNG讀取能力，不新建資料平台；所有52張全列成功／invalid／unknown理由，不挑成功子集。不得把4R手述部位當candidate輸入或gold；使用當前合法測得的候選／line幾何要記來源及其依賴，不能沿O第一條line捷徑。每段可明列空history起點與≤90ms因果限制，**不是full-prefix重播或新觸控因果實驗**；不推遊戲正確率、Miss或移除Down收益。失敗與無新線索亦可交negative。

本包不允許full recording replay、runtime/cost/stress/AA/ABBA、B插樁、emulator/ADB/真觸控、模型、goal、automation、commit/push或自行續派。synthetic/fake-clock及PNG只讀工具不得連接觸控backend。完成或到停止點即交總控，候選不自採用；真正owner接線／完整行為／成本與live另案。

**容量。** 新batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi`，总64MiB＝開發56MiB（含外部新source/docs/tools/snapshots、失敗／log）＋總控8MiB。新out `out/x10d-o-bvi` ≤256MiB，不覆寫原out，不複製PNG或刪旧資料。開工重核free≥64MiB＋256MiB＋5GiB，及原8GiB aggregate。4R交付时aggregate8275074850B、剩314859742B，本總控頁／review追加另見receipt；新64MiB須連同原prior45307809B、O外部72115B、O總控頁9546B、4R外部54056B及本次外部頁一起carry，不能新root歸零。所有數字是保守entry length帳，非NTFS allocated bytes。

全曲產品狀態不變：現行Chapter Legacy分母／各曲當前解鎖unknown，完整IN Miss=0仍無新證據。本包要取得下一個可審查的觀測實作結論；研究／冷驗證不替代最終遊戲驗收。
