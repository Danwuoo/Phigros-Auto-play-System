# Chapter Legacy開發盤點與4I-R2D總控驗收

2026-10-05，Asia/Taipei。依使用者最新指示，**先不繼續開發、不派送下一task；本輪僅獨立驗收與盤點。** 4I-R2D開發chat `01a109fb-e794-74b1-9b55-d19fb3fa66bf`已完成並idle。此頁的後續階段是規劃，不是新授權；原R1 runner repair剩0仍有效。

目前已有可運行的歷史自動遊玩系統，但新BVI改善方案還沒有可信建置／冷驗結果、沒有接正式owner，也沒有新實戰收益。**沿目前BVI路線，到首次候選實機測試前尚有5個必要工作階段。** 此數是總控整理的工作分段，不是固定task數、工期或通過保證。若BVI被核心反例否決，需重新選擇方向；不能承諾再跑5個包就可實機。

## 4I-R2D驗收結論

**恢復設計方向通過，保留為design-only；不授予runner實作／controls／configure或build資格。** 原[研究](../03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2D_REVIEW_20261005.md)与[交接](../03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2D_HANDOFF_20261005.md)保留。單一RootBinding、防跨shell繼續執行的attempt狀態、launch與cleanup誠實收據、共用contact assertion拒絕鏈，均直接對應R1已核問題，沒有要求改BVI閾值或重開OS／toolchain支線。

總控以新 [check-r2d.ps1](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/controller-review/check-r2d.ps1)獨立核對 **2134個不同檔案、2148項SHA／bytes引用，0不符**，含2120份原來源及review保護集合；[integrity.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/controller-review/integrity.json)保存結果及完整來源manifest的SHA錨點。本輪不複製大清單、不執行任何runner、control、configure、build、candidate或PNG audit。初次終端摘要因OrderedDictionary投影顯示null，保存的JSON及檢查斷言有效，總控另讀JSON確認；不是候選執行失敗。

原503 Git paths／dirty保持，R2D新增2份docs共505，本頁另新增1。HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`未變，src/include/root CMake無diff。原R1六項修補的編譯／行為仍未驗。

修正後R2D新增 **148404B**＝batch114741B＋external33663B，交付時aggregate8283811437B、剩306123155B；獨立重算一致。首次漏計external docs32391B的舊收據與文件版本保留於failed-settlement，僅修正後final-receipt-corrected及artifact-ledger-corrected作當前容量入口。本輪review與本頁另計原controller8MiB，settled值見 [controller-receipt](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/controller-review/controller-receipt.json)。

設計驗收的限制：P01–P09、S01–S07、E01–E05、C01–C04全部未跑。未來實作仍須具體凍結stage allowlist、三項控制的端到端時限、durable RUNNING／失敗STOP、writer/readback故障與非零exit的機讀判定；設計本身不證明Windows執行可靠。提議40MiB開發／128MiB out只是未授權預算，不因本頁驗收自动生效。

## 已經開發了甚麼

| 層次 | 已有實作／證據 | 現行判定 |
|---|---|---|
| 基礎遊玩閉環 | C++20擷取、latest-frame、像素觀測、線／Note追蹤、contact owner、可撤銷排程及觸控注入；已有HD／IN歷史實戰 | 系統不是從零開始。C36h tint1是behavioural baseline，Dlyrotz IN最近保存結果M77，未證改善；main50 50/27/11只作comparison/donor、live0 |
| 冷契約及離線驗證 | 9/29 D1–D4／C0–C6可冷範圍、旋轉／Hold／排程等合成fake-clock；X1完整fixed-pixels contact replay工具；既有owner controls | 相關歷史冷驗已成立，可復用。固定pixels不含新觸控造成的遊戲feedback，不代表新BVI已驗 |
| 證據與逐曲總帳 | 選入89份完整結算＝70HD＋19IN、33sessions，版本／來源分開；7722張原始PNG與局部研究packet保留 | 目前章節完整分母／逐曲IN解鎖unknown；89份所選結算皆有Miss，完整IN Miss=0驗收證據0 |
| X10d-P pending-missing候選 | pending取消hook已隔離實作，冷契約獨立驗收 | 沒有採為新實機策略；14組少Down機會利弊unknown，成本仍not-ready；不自動和BVI混合 |
| X10d-O／BCC-v1 | ownership候選、reference adapter、原基準／owner controls及圖像reader曾實際建置／執行，BCC反例獨立重現 | BCC-v1已否決，停止該family；不是所有ownership研究已完成，suppression仍OFF |
| BVI-1／4I-R1 | 當前body／端部／contact觀測、6份／90ms有限關聯、fake lifecycle constraints、RGB renderer、typed及e2e harness；原69＋新增20 cases、22 supplemental、4 schema/contact controls | source與修補已凍結；本線尚無可信candidate build／test，356 layer-case僅規劃分母；未接正式owner，尚不能稱產品能力完成 |
| X11成本工具、A／B研究 | runtime/shared Wake/archive、bounded collector、active coverage及reader已有工程成果；A checker未完成獨立執行；B publication/selection契約已審 | 量測與負結果有價值，但有效成本資格仍未過。193 skip因果與B0/B1成本差仍unknown；B未實作、不強制先擴此支線 |
| 4I-R2D恢復設計 | 已提出固定輸出根、持久停止狀態、程序／收据失敗分層、共用assertion拒絕鏈 | 本輪通過設計審查，尚未實作／執行；runner舊repair額度不重置 |

較早README／路線圖「O未開始」是10/4整理包時點，不代表最新狀態；本頁及O→4R→4I→R1→R2D總控驗收鏈為後續沿革。為保留封存SHA，本輪不改寫舊入口文件。

最近4I、R1、R2D累積的是新觀測小工具、harness修補與可靠執行設計，**不是三次遊戲策略效果提升**。兩類阻塞分開：近端是如何可信build／run／保存證據；產品端是BVI是否真的可辨識、如何接owner、完整行為是否改善以及成本是否合格。修好runner只移除第一類阻塞，不會自動解決77Miss。

## 到首次候選實機測試的5個階段

| 階段 | 要完成的工作 | 可前進的最低證據 |
|---|---|---|
| 1. 恢復可信執行 | 另行核准後，局部實作R2D固定根／機械STOP／誠實收據及contact拒絕鏈，驗證必要工程控制 | 錯路徑、缺收據、失敗後跨shell續跑被拒；三項程序控制及owned cleanup可信，才允許一次configure |
| 2. 驗證BVI觀測候選 | standalone Release及授權／可用的Debug／ASan冷驗，保留69＋20／22及所有負例；合成成立後再做有界原52PNG只讀審核 | RGB→link→fake完整斷言可核、沒有核心矛盾；真圖能產生可用且有限制的觀測，不靠typed或手述部位冒充辨識 |
| 3. 接入完整行為並離線回歸 | 設計／實作到正式ownership決策邊界的隔離候選，驗同contact Move、incoming、head/body/tail、unknown/completed守門；完整recording行為比较 | 完整action分母／first divergence／機會損失與回歸成立；不只H/K局部變好或少Down，仍不把固定pixels當真遊戲收益 |
| 4. 取得有效runtime／成本資格 | 對準擬上機的exact候選做必要負載、coverage、noise与baseline／candidate比較 | 有效全分母與A/A、候選比較及安全收尾過現行gate；不得直接借P候選舊量測替BVI背書或改門檻追pass |
| 5. 凍結及實機放行 | exact source／binary／DLL／profile、裝置／擷取／方向／五指mapping指紋、容量／停止條件、bounded pilot與總控驗收 | preflight與安全前置成立，使用者恢復開發且總控在當時授權內續派後，才進首次候選實機pilot |

階段2把合成與52PNG合併為一個「候選觀測驗證」階段；若以獨立工作包拆開，可能多於5個task。階段3、4也可能因結果需要額外修補，因此不報尚無依據的工期或完成百分比。這不是要求所有歷史A／B／模型研究先完成：A的193完整因果歸屬、B所有插樁方案、模型訓練及完整人工作圖平台均不自動新增為實機前置。

「可做首次實機pilot」與「全Chapter Legacy完成」是不同里程碑。pilot後仍須核当前章節清單／逐曲IN解鎖，必要時HD解鎖／回歸，再以同一凍結通用策略完成逐曲IN並保留完整P/G/B/M結算；每曲Miss=0才通過，不能拼接不同版本最佳值。舊X12最多6attempt只是pilot上限，不是全章節所需轮次或全部驗收承諾。後續全曲計畫與容量需另定有界範圍。

**本輪執行結論：** 4I-R2D設計簽收；開發與派送暫停。新runner／configure／build／test／PNG audit／emulator／觸控／goal／automation均0。C36h tint1、main50 donor/live0、suppression OFF、P excluded、X12 not-ready及「Chapter Legacy全曲IN Miss=0尚未達成」不變。
