# 桌面48px短斜Flick三首隨機實測與HUD修復

2026-10-30（Asia/Taipei），桌面native013在同一份48px曲庫與設定下，經Computer於遊戲All Songs按Random，完成第二批連續三首：Demiurge IN11、Get Ready!! IN13、Re: birth IN15。三輪均由真正的桌面程式READY，再於遊戲按Play；各自完成全曲、確認結算、釋放後kernel zero，13項輸入／生命週期稽核與6項capture身份／時鐘稽核全通過。Demiurge取得這一輪AP；其餘兩輪有Bad／Miss，未宣稱全曲庫或跨輪AP保證。

第一批第二首Xep+ion因HUD觀測到期而中止，整批重置。補上部分合併分數的當前觀測路徑後，先以Xep+ion指定回歸確認全曲，再重新抽取三首；指定回歸及第一批成功輪均不挪入第二批計數。原失敗紀錄保留。

## 最終第二批三首

| 抽數 | 曲目／難度 | 完整接點／steps | Computer結算 | Perfect／Good／Bad／Miss | Max Combo |
| --- | --- | --- | --- | --- | --- |
| 1 | Demiurge IN11 | 520／4723 | 1,000,000 · AP · 100.00% | 520／0／0／0 | 520 |
| 2 | Get Ready!! IN13 | 819／3921 | 937,393 · S · 98.44% | 803／5／10／1 | 421 |
| 3 | Re: birth IN15 | 939／4722 | 965,021 · V · 99.36% | 931／3／0／5 | 665 |

合計2278接點／13366steps，三個獨立實際回合依序使連續計數1→2→3；沒有跳過抽取。每輪各保存一張確認結算圖，沒有遊玩／開場／HUD圖片落盤。分數、判定與AP由Computer結算觀察另行記錄，不從RPC接受或manifest的全曲旗標推導，也不改寫原生manifest的歷史AP欄位。

三輪都使用核心SHA `cf47b209b2602e3cb1b238aaaffa901b365133dcfaa79f001eba3779b14c67fb`、曲庫SHA `a52780d06c3f39fa8023989c4e291d4e9b459d9f46b17f34fbd46c5729aefd0b`、batch02設定快照SHA `0e926e9bc3decef153a627aa202afce4eadd926ec728eb19c71efdca83575645`。最終進度原件 `work/three-random48-progress.json` 的 `active_batch=2`、`completed_streak=3`，最後更新時間02:10:57 +08:00；SHA為 `a28b8b0709c84f4aa38dd0a4752ac59ea027b5301247e179d9a758697111c902`。

最終獨立只讀複核 `work/three-random48-b02-final-independent-review.json`（SHA `5f7c88c35983c402176c7b26284cfc832cbcfc0fd362773bbd2e4bc0a5f9a38a`）確認三輪原件／auditor身份、全部13＋6項及目前安裝設定一致；合計11988個訊息／3262個stream，Write／WritesDone／Finish均成功。主機實際Write開始最大遲到3.999578ms、准入所用當前UI最大age86.3617ms；這些是主機時鐘紀錄，不是遊戲輸入延遲或GPU像素age量測。

## 實際回合與稽核索引

以下連結指向被Git忽略的本機原件；本文件只記錄身份，不把私人譜面、音源、校正、圖片或完整實跑資料加入Git。外部 `work/` 位於本次Codex工作區。

| 抽數 | 本機原件 | 外部合併稽核與SHA256 |
| --- | --- | --- |
| 1 | [Demiurge manifest](../local/manual-panel-001/runs/catalog-manual-panel-20261030-015713-755-c33d8f/round-01/manifest.json)／[ledger](../local/manual-panel-001/runs/catalog-manual-panel-20261030-015713-755-c33d8f/ledger.json) | `work/three-random48-b02-d01-audit.json` · `43dfb89d28bc9994841413fe2d0afc18aa4fab09154c4345c1765a5b235ed37b` |
| 2 | [Get Ready!! manifest](../local/manual-panel-001/runs/catalog-manual-panel-20261030-020230-123-0e1a8a/round-01/manifest.json)／[ledger](../local/manual-panel-001/runs/catalog-manual-panel-20261030-020230-123-0e1a8a/ledger.json) | `work/three-random48-b02-d02-audit.json` · `0c9320178ce60652226209a51f0b281a2ada691dbfc4bb981484219f4c43c44c` |
| 3 | [Re: birth manifest](../local/manual-panel-001/runs/catalog-manual-panel-20261030-020720-089-c55869/round-01/manifest.json)／[ledger](../local/manual-panel-001/runs/catalog-manual-panel-20261030-020720-089-c55869/ledger.json) | `work/three-random48-b02-d03-audit.json` · `b5061e09564d3a58b63fe9812339de339e2ac6e095d0d6da368309789f58786c` |

| 抽數 | manifest SHA256 | result.png SHA256 |
| --- | --- | --- |
| 1 | `16f693d1c2a5c0a6568fc26a3cecd2d715d48aa8eb5b4b8dcd3427151887f3f9` | `7812178582b2c5ce921cc39ae113abb4db4c258f17c0c91121a83bdbfe1b73bb` |
| 2 | `fe850044473606b40cbf2cb2e8f9c21e064d9cf4f87d0e9d1c38b43072b32d18` | `8148d13fcdb24d3e43e8eccd00fd0ee6b98d8104c15fc3c737a203f0bdd0e1e5` |
| 3 | `2d50fe61840ce2889810355de78066206df646db082158251014c875f8ecae2a` | `066c95c972cbc482e435460bec6ed47e941d6ac0ff3a1b5bf7ffedcb381b5f1a` |

## 第一批失敗與修復

| 歷史批次／抽數 | 曲目／核心 | 實際結果 | 計數處理 |
| --- | --- | --- | --- |
| batch01／1 | FULi AUTO SHOOTER IN14，native012 | 全843接點／2092steps；948,191 · S · 98.61%；815P／25G／2B／1M，Max Combo512；13＋6項PASS | 該輪成功；下一抽失敗後不計入第二批 |
| batch01／2 | Xep+ion IN14，native012 | 390／698接點後 `actual_UI_receipt_expired`；未全曲／未確認結算，最後kernel zero | 保留失敗，整批連續計數歸零 |

Xep+ion原回合持續收到新影格，最後29個被拒影格跨約481.8ms，送達至處理僅約1.86–4.53ms；實際觸控Write最大遲到約1.91ms，沒有stream Finish失敗。這次是失去可接受HUD觀測後按既有500ms guard停止，不能寫成20ms觸控遲到或單純斷流。

既有合併分數路徑要求單一連通區塊跨足多個已學得窗口，沒有涵蓋「部分數字合併、其餘數字仍清楚」的情況。native013仍從**當前RGB**獨立量測完整pause、難度與等級、曲名三個學得錨點，新增由當前邊界片段與清楚數字共同供應分數紋理的路徑。最多8個片段與7個窗口；清楚連通區塊只能投給唯一最大重疊窗口。至少4個分布窗口有當前垂直紋理，且至少1個由邊界片段提供，才接受這次候選。沒有重建缺失的七位分數，也不將舊觀測改時戳續用。

離線合成測例包含單／雙片段加清楚數字正例，以及空白、實心、單線、僅三窗口、缺錨點、過期、暫停與超預算等負例。500ms當前HUD期限、20ms觸控遲到、兩幀學習、epoch／幾何失效、release-all及使用者STOP條件未放寬。`main.cpp`只增記有界片段／窗口metadata，未新增中途圖片保存。

舊FULi稽核 `work/three-random48-b01-d01-audit.json` SHA為 `13a7f8b3870957bf2c05bf41f8e7e78eb6329847435326dd299987b31359d409`；[失敗Xep manifest](../local/manual-panel-001/runs/catalog-manual-panel-20261030-013625-979-6e40a9/round-01/manifest.json) SHA為 `379d8b32101e8ac646261e28c703e73ef1566cd02736f47edf5f64bb3e43d44a`。只讀分析在 `work/xepion-hud-stop-forensics-20261030-001.json` 與 `work/xepion-hud-tail-metadata-forensics-001.json`。原輪未保存遊玩RGB，metadata不能重建其像素，沒有以新程式回放通過來改寫舊結果。

## Xep+ion指定回歸與新路徑實際使用

新核心先以Xep+ion IN14指定回歸：完整698接點／1950steps，含69 Flick；952,736 · S · 99.27%，689P／6G／2B／1M，Max Combo414。原生確認結算、全10點釋放後kernel zero，13＋6項稽核通過。這輪不計入Random三首。

當前partial-boundary路徑實際產生45個候選影格，其中36個達到fresh HUD確認、9個仍在確認等待。45個候選的playing receipt均是當前影格原received時間，候選不直接等於觸控許可。實際11筆Write准入引用其中6個已確認的當前影格，且都allowed／playing；另有19個當前負例被拒絕。這證明該路徑在本輪被實際使用，並非只在離線測例存在；沒有從這些數字推導每筆輸入的遊戲效果。

| 原件／證據 | SHA256 |
| --- | --- |
| [指定回歸manifest](../local/manual-panel-001/runs/catalog-manual-panel-20261030-015047-586-aa7fbc/round-01/manifest.json) | `30668368b9afb7db776b16210e3f8ff1d74e0f546ff6b63310b9d212a2247a7c` |
| `work/xepion-native013-regression-audit-001.json` | `05629e5a92b98ca925f3a2b871fb17ffe3a5663c4700c2044603ea349c5b88d9` |
| `work/xepion-native013-partial-route-evidence-001.json` | `0ad13469aae919431aa899610ce45e951d8a75d5843fc118598b51d0391fc474` |

## 新建置、封存與安裝身份

Windows native全目標Release build完成後，最終 `partial-score-native-ctest-002.log` **33／33通過**；portable configure／build及CTest **4／4通過**。較早native CTest001雖也顯示33項通過，但與最後app連結時間重疊，只保留為歷史，不用作最終核心驗證。離線測試中的合成Phigros／Sekai dry run不是遊戲實測。

| 身份／新證據 | SHA256 |
| --- | --- |
| `Phigros Runtime/20261030-native-013/bin/chart_experiment.exe` | `cf47b209b2602e3cb1b238aaaffa901b365133dcfaa79f001eba3779b14c67fb` |
| native013 manifest | `bba841d4db22e8e501bb4d80043eaddc5fd8c4ca660eca46ca70539f27cbb41f` |
| `work/partial-score-native-build-tests-receipt-001.json` | `27ec71d7100e7932b6b07956724a52304c7849ec5ddf4a5484afeaa00f280186` |
| `work/partial-score-native-ctest-002.log` | `bacca47e76808ee294b0756c085ca0cd1c05e9cc03186aeb6ed116f1dd12a4da` |
| `work/partial-score-portable-ctest-001.log` | `b9803914718ccbd73adc6e3e9ab8637a865e16ae42e76f70cd2884103176867c` |
| `local/library-desktop-flick48-002/test-library.json` | `a52780d06c3f39fa8023989c4e291d4e9b459d9f46b17f34fbd46c5729aefd0b` |
| 固定前端 `local/desktop-app/releases/20261029-152742-243564-22612340/manifest.json` | `ef0b71fb4b60e11fc299d506e415d32f5a90c5718110e6568e3952a649cc4523` |

封裝核對20個關鍵來源的建置前／後SHA、執行檔、DLL與這輪新建置／測試receipt，未沿用舊核心測試資格。封存222件、33,410,905bytes，含202份來源、10件執行檔／DLL及明列metadata evidence。20項清單是明列關鍵輸入，不宣稱已獨立證明所有遞迴編譯依賴可重現。

安裝只切換runtime封存參照與部署metadata；舊設定逐位元組備份，完整寫入及並行變更核對後原子取代。389份IN／AT與變體、374份已準備資料、15份blockAreaList未支援項目、全部原譜／profile／fingerprint、48px／8／20／6ms、十指、−40ms相位與固定前端保持原身份。374份可準備不代表374份均已逐曲實測。

相對native012，這次partial-score修正只改實驗 `learned_hud_redundancy.hpp`、`main.cpp`的有界metadata及對應測例；PAS傳輸、contact compiler與開場模型沒有因這個修正再變更，未合併main分支。私人原件不加入Git。第三方gRPC依賴從零重建本輪NOT RUN，不與本機app建置或上述實際emulator回合混為一談。

## 結果邊界

13項稽核核對全譜命令、實際Write准入時鐘、stream Finish、全10點釋放及其後kernel zero、實際結算與result-only身份；6項另外核對capture原時鐘、單調發布及RPC／UI身份。queue／Write accepted不等於每個音符已被遊戲正確判定，成績另看Computer結算觀察。

capture來源時間仍是伺服器請求時間而非GPU呈現PTS，未取得來源像素age資格。本輪沒有32／48px受控A/B，不能把分數差全部歸因於Flick幅度。Demiurge一次AP不保證重現，Get Ready!!與Re: birth仍有Miss；三首完整完成不等於全庫穩定、零Miss或所有未支援譜面可用。較早Thrash force餘留一觸點的原輪失敗與較早未完成五首紀錄均保留，不回溯改判。
