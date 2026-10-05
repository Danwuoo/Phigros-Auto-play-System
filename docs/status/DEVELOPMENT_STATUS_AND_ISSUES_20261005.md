# Chapter Legacy 開發現況與問題總整理

更新日期：2026-10-05，Asia/Taipei。整理者：總控。

**目前開發與續派均暫停。** 使用者本輪只授權盤點及文件整理；本頁後續建議不是開工授權。最近的「修復 BVI 建置命令封裝並完成冷驗」task 已 completed／idle；R2F task 最新 turn 已 completed，查詢時為 notLoaded。沒有重啟它們、建立新開發 task 或執行 runner、configure、build、測試、模型、觸控與實戰。

## 1. 現況摘要

專案已有可以遊玩的歷史 C++ 系統；本輪研究的 **BVI 新候選尚未成功編譯，尚未執行候選冷測，更未取得新的實戰改善證據**。目前主要卡點在測試與建置工具的工程流程。

- 產品目標：Chapter Legacy 所有曲目解鎖 IN，逐曲完整 IN 結算 Miss=0。HD 用於解鎖／回歸，P/G/B 照報，沒有 AP 前置。
- 現行章節完整曲目分母、目前各曲 IN 解鎖仍為 unknown。歷史選入 89 份完整結算（70 HD＋19 IN、33 sessions）不能當作已驗全章節或當前解鎖清單；其中沒有完整 IN Miss=0 的驗收證據。
- main HEAD 為 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`；main50 的 observer50／planner27／diagnostics11 是 comparison／donor，live0。
- C36h tint1 是 behavioural／experimental baseline；最近保存的 Dlyrotz IN 結果仍 M77，不能以離線工具進展宣稱 Miss 改善。
- suppression OFF；X10d-P 沒有混入 BVI；X12 仍 not-ready。模型只供離線研究。

原始逐曲入口：[Legacy 總帳](../catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)、[逐曲 JSON](../catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。較早的 [R2D 盤點](../catalog/01-project/LEGACY_DEVELOPMENT_INVENTORY_AND_R2D_ACCEPTANCE_20261005.md)可看長期脈絡；其中當時暫停與派送範圍是歷史，當前暫停以本頁及使用者最新指示為準。

## 2. 已經開發了甚麼

| 層次 | 已有程式／證據 | 尚不能推論的能力 |
|---|---|---|
| 基礎遊玩閉環 | C++20、gRPC latest-frame、像素觀測、線／Note 追蹤、note→line 關聯、contact owner、可撤銷排程、觸控注入及 manual-session；有歷史 HD／IN 實戰 | main50 或 BVI 已通過現行實戰；全曲 IN 已解鎖或 zero miss |
| 幾何、Hold 與排程 | 歷史 M0／M1、C0–C6、旋轉與晚對齊、Hold body／tail、fake-clock 等實作與回歸 | 新 BVI 已通過同一批回歸，或合成效果等於遊戲採納 |
| 離線證據工具 | X1 fixed-pixels contact replay 已獨立驗收；X2–X10 系列有關聯、角色、Hold 與反例研究；保存 7722 張原始 PNG | 固定影像重播包含新觸控的遊戲回饋；proposed 或手述部位是人工 gold |
| X10d-P | pending-missing hook 的冷契約已驗收 | 已採用為實機策略；14 組機會損失利弊已明；成本已合格 |
| X10d-O／BCC-v1 | 有候選、adapter、實際反例及總控審查；同 RGB 的物理 ownership 期待矛盾使 BCC-v1 否決 | 所有像素方法均無效；BVI 自動繼承其測試成果 |
| 4R／BVI-1 | 可觀測分界與有限時序研究、52 圖選取研究；當前 body／端部、6 份／90ms 關聯、fake lifecycle、RGB／typed／e2e harness 已有 source | 可見 body 唯一決定物理 owner；BVI 實作正確、已編譯或已測 |
| BVI R1 修補 | 淘汰順序、獨立樣本去重／跨度、RGB→link→constraint、schema／coverage、contact 傳遞、長靜止支持與 loss 邊界等 source 修補保存 | 89×4=356 layer-cases、22 supplemental、4 原 controls 已執行；1128 coverage rows／2392 引用只是規劃及結構分母 |
| 收據與程序控制 | 共享檔案交易、精確輸出根、durable STOP、PID／creation／image／Job membership、同一期限清理及 identity 交接已有實作；R2F control 三真控制通過 | 任意新 wrapper、root 綁定、工具鏈或候選也已驗；全部 managed／system DLL closure 已採集 |
| 成本／A／B | X11-P/R1/R2/R3 有工程成果與負結果；A 有 reader／checker 工作，B 量測契約設計審查通過 | A checker 獨立驗收完成、193 skip 因果已知、B 已實作、X12 ready |

主要參考：[架構](../catalog/01-project/ARCHITECTURE.md)、[冷開發結果](../catalog/04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)、[X1 修補交接](../catalog/04-offline-research/C36H_CONTACT_REPLAY_REPAIR_20261002.md)、[P 總控驗收](../catalog/04-offline-research/PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)、[O 總控驗收](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_CONTROLLER_ACCEPTANCE_20261004.md)、[4R 驗收](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_4R_CONTROLLER_ACCEPTANCE_20261004.md)、[R1 驗收](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_4I_R1_CONTROLLER_ACCEPTANCE_20261005.md)、[B 設計驗收](../catalog/05-runtime/RUNTIME_DECISION_SKIP_B_CONTROLLER_ACCEPTANCE_20261004.md)。

## 3. 最近工作包時間線

此表的「通過」只對應該列所述層次，沒有把來源保全、mock、程序控制或設計通過升格為候選通過。

| 包 | 已到達的成果 | 停止點／交付分類 | BVI 編譯／候選測試 |
|---|---|---|---|
| 4I | 第一版 BVI／harness source | configure 程序收尾不合格；平台 partial | 未完成 |
| R1 | 六項 source／harness 修補與新 cases | runner 指錯舊根，收據錯放；核驗失敗後還跑第二控制，流程偏差已保存 | 0 |
| R2D | 最小執行恢復設計 | design-only 已審；未授予實作 pass | 0 |
| R2E | helper mock 25 筆；natural 程序正常退出 | writer 尚開啟時 Get-FileHash 分享衝突，verification1、STOP | 0 |
| R2F-01 | 部分 reader／binding source | 預留計算多 28,949B；另有失敗後新增兩檔偏差，已封存 | 0 |
| R2F resume-02 | 交易預驗第二輪32/32；natural、nonzero 通過 | owned-child parent 身份收據失敗，held wait0 兩錯；STOP | 0 |
| R2F control-03 | helper25/25、交易32/32、控制預驗39/39；真控制 0/0/0、7/7/0、0/125/0 | Release configure 首次 1/1/1，cmd 語法錯誤；後九命令未執行 | 0 |
| BVI build-01（最新） | 既有三預驗96/96；新增診斷預驗18/20 | D19/D20 測試 adapter 的 mixed-slash scratch 路徑遭 guard 拒絕；一次額度耗盡，native 前 STOP | 0 |

最新包沒有執行任何真控制、wrapper probe 或十項產品命令。原 configure 的 `The syntax of the command is incorrect.` **尚未定位到具體出錯行，也尚未修復**。wrapper-v0、runner placeholder 與診斷草稿不能視為已驗版本。

最近的真進展是 control-03 已驗共享 identity 發布與 held-handle 等待：parent 實際看見 pending→empty→ready；15.0122752s 後仍有可信 child／conhost，終止後全部 held handle signal、Job active0、streams 完成、errors0。這支持該次新版控制成立，不能倒推 resume-02 的精確歷史失敗原因。

原始結果及交接：

- [R2E](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2E_RESULT_20261005.md)／[R2F-01](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESULT_20261005.md)。
- [resume-02](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESUME_RESULT_20261005.md)／[control-03](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md)。
- [最新 BUILD 結果](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md)／[BUILD 交接](../catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md)。

## 4. 問題清單與判定

| 問題 | 證據與狀態 | 對進度的影響 |
|---|---|---|
| Windows 建置命令封裝 | control-03 的 cmd root 真 exit1、stderr41B，自然收尾可信；沒有新out／CMake log；vcvars、路徑、quoting 或 CMake 哪一步出錯仍 unknown | 當前尚未真正進入 BVI 編譯 |
| 新診斷 adapter 路徑錯誤 | BUILD D19/D20 以字串組合 `/scratch-diagnostic/D19`，與 transaction 的 `\scratch-` 字面 prefix 不符；在 NewTransaction 即被拒 | 還沒測到完整診斷交易，4 次真 probe 額度均未使用 |
| INIT state 欄位不完整 | 最新 BUILD 首次保存 STOP 時缺 `running` 屬性；只做保存修正後 STOP 成立，失敗保留 | 維護／狀態 schema 本身仍有一般作者錯誤，需避免錯誤處理再失敗 |
| 收據分享模式、身份交接、held 等待 | 後續交易／control-03 已取得局部成功證據 | 應保留已驗核心，避免每次重建整套工具；新綁定／改動仍須必要回歸 |
| 過度僵硬的前置額度 | 容量預估錯誤、預驗路徑錯誤在尚無真程序或產品執行時，因一次額度直接整包 STOP | 一般作者錯誤被升級為重新派包、複製來源與再建收據；總控派送設計也須負責 |
| 外層工具順序與失敗處理 | R1 核驗失敗後仍執行下一控制；R2F-01 exit1 後仍新增兩檔，均已有紀錄 | 必須維持相依工具 exit／isError 檢查，不能只依對話記憶停止 |
| 文件與保護資料膨脹 | 最新包新增約1.49MB但native0；累積多份同日結果／交接／ledger。原README/路線圖仍有「O未開始」的時點文字 | 找真正現况成本上升，需要新入口與明確日期；不能以文件數當開發進度 |
| 真圖合法幾何缺口 | 原52圖 packet 的合法 current ROI／all-lines 為null；既有 provenance-gap driver 只decode並輸出unknown，candidate未執行 | 未來需合法 bounded-current adapter；不能從 full-prefix bank／手述 parts 補成真辨識 |
| BVI 與實際 ownership 未驗 | Guard.attachment_query 是 fake 條件；新 source 沒有可信 binary、assertions、sizeof／probe 實測 | 尚無法判斷 BVI family 有效／無效，更不能接實機求 zero miss |
| 成本與產品驗收缺口 | X12 not-ready；當前逐曲解鎖／章節分母 unknown；新候選 IN 結算0 | 程序工具修好仍不等於可上機或達成產品目標 |

根目錄 guard、身份／cleanup、來源保護等硬限制有作用；需要區分「拒絕了不合法輸入」與「作者提供錯誤測試路徑」。後者在正式執行前應有合理的修正與回歸流程。此判斷是恢復開發前的流程建議，**沒有修改任何既有 STOP 或授予重跑權限**。

## 5. 容量是否為真正阻塞

目前沒有資料證明需要增加容量。R2F-01 的 40MiB gate 是 reserve 算術錯誤，並非磁碟耗盡；最新 BUILD 停在測試 adapter，不是容量。

最新開發收據（本次文件整理之前）：

| 項目 | Bytes |
|---|---:|
| 最新包新增 development | 1,485,130 |
| 累計 development／上限 | 10,473,385／58,720,256 |
| 原 R2F 40MiB 子額度剩餘 | 38,764,345 |
| aggregate／上限 | 8,287,386,342／8,589,934,592 |
| aggregate 剩餘 | 302,548,250 |
| controller 已用／上限 | 1,299,900／8,388,608 |
| 最新包 newout／舊 out | 0／374,705 |

來源：[BUILD final receipt](../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-build/final-receipt.json)。這是保留資料與預算的帳，不是磁碟 free 或效能量測。本次新文件及維護資料另計 controller 與 aggregate，見[本次整理收據](../../measurements/documentation-organization-20261005/receipt.json)，不回寫舊數字。

## 6. 距離測試還差甚麼

| 里程碑 | 現況與未完成項 |
|---|---|
| 啟動離線 BVI 冷測 | control-03 三程序控制曾通過；仍需修正診斷 adapter、定位並修復建置 wrapper、取得成功 configure／build，才能執行候選 assertions |
| 候選觀測驗證 | 合成／fake-clock／完整分層冷契約，再完成合法 current ROI／all-lines 的有界真圖觀測；PNG額度0/2仍保留 |
| owner 接入與行為回歸 | 同contact Move、head/body/tail、unknown Down、completed、不誤重播、全action分母與機會損失；不能只看少Down |
| runtime／成本資格 | 同一 exact 候選、有效全分母／noise與比較／可信收尾，不沿用其他候選成本冒簽 |
| 有限實機 pilot | exact source/binary/DLL/profile、裝置與五指指紋、容量／停止規則、preflight及總控驗收 |
| 全章節產品驗收 | 核当前章節清單與IN解鎖，必要HD解鎖／回歸，再同版逐曲完整IN Miss=0；pilot不是全曲完成 |

先前三控制與建置恢復被放在「第一階段」；現在應分開看已驗控制與未完成wrapper，不說一切回到零。這些是工作階段，不能換算固定task數、完成百分比或實機日期。A全部193因果、B完整插樁、模型訓練與大型標註平台不是自動新增前置。

## 7. 恢復開發時的建議（目前不執行）

1. 將一般作者錯誤的可修正預驗與真正不可繼續的程序／來源故障分開；在原容量與有界工時內保存失敗後修正，不再把每次未launch的adapter typo變成整包重開。
2. 最小修補 canonical scratch 路徑與完整初始state schema；保留root guard及既有成功交易／身份／清理核心。沿相同真helper驗完整診斷交易，而不是更多獨立boolean案例。
3. 用已規劃的有界wrapper probe取得具體offending step與實際argv，通過後連續configure→build→wrong-contact負例→完整Release suite→可用Debug／ASan。正式執行異常仍分類保全，不能隱藏反例或修改oracle追pass。
4. 控制額度與驗收層次按目的設置：工具可靠性與候選效果分開報，成功證據綁source／binary SHA；不把source存在、Add-Type或預驗綠燈當BVI能力。
5. 新包盡量引用已凍結資料，避免反覆抄整套manifest與失敗史；計帳不能因此漏列新增資料。優先讓工程工作到達候選編譯，而非持續擴充恢復平台。

使用者恢復授權後才可具體派送。這份整理不新增attempt、試跑額度或live資格。

## 8. 文件整理方式與證據限制

本次盤點前 `/docs` 共141檔：根目錄120檔、第三方授權21檔。前次只建立八個分類索引；本次依使用者要求把原141份文件實際搬入分類資料夾，另有[統一入口](../README.md)。Markdown相對連結同步更新；歷史收據中的path／bytes／SHA保留當時值，新舊路徑與本次SHA另列[搬移核驗](DOCUMENT_RELOCATION_20261005.md)，不將文件搬移當作產品重新驗收。

較早文件中的「最新」「已授權」「續派」「O未開始」依文件時點解讀；本頁是停止開發後的現況入口，不把歷史文件改寫成新狀態。分類索引不是重新技術驗收。總控前一輪獨立核最新包4134檔／4193引用0不符；本輪另保護原631個Git可見檔及141份docs，核新索引覆蓋與連結，詳[整理收據](../../measurements/documentation-organization-20261005/receipt.json)。沒有重新執行任何候選、正式程序控制或全量raw分析。
