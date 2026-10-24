# Chapter Legacy 資料入檔與 IN 一輪全測準備

## 封存結果優先

最新build006已由使用者實跑：18個回合、14輪完整輸入、8輪原生確認結算；正式Legacy有8首保存結算AP，另有Verrückt暖身AP。6個完成輸入未確認結算的回合及剩餘未測歌曲已列入[完整封存紀錄](SESSION_RECORDS_2026-10-24.md)。使用者要求先停止修正並commit／push；本次整理不新增實測。

以下保留各階段建立時的條件、NOT RUN與限制，不能當作目前仍未實跑；當前狀態以結果索引與MODEL_HANDOFF為準。

## 視窗操作與額外暖身

使用者新增Verruckt IN至曲庫，預設可在Legacy20前試跑，正式章節名單與順序不變。現在使用[視窗面板](SONG_TEST_PANEL.md)選取／勾選、開始、看READY、停止與開啟紀錄，無須在終端逐首Enter。005命令列仍留作診斷，優先使用桌面捷徑；新GUI與Verruckt完整實機尚未啟動。

## 最新入口005：修復首輪共用 HUD 誤拒絕

sweep004的Glaciaxion首輪已實跑，約62.7s因Drag接到第一個分數字，連續500ms score結構不通過而停止；當時287combo、288 DOWN／287 UP，最終kernel零。這不是譜面／首音失敗，亦未完成整首；原回合保留在chapter-legacy-IN-live-20261024-171208-966/round-01。

改用 [sweep005啟動入口](../../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-005/start.ps1)、build003、HUD shared-in-cell-score-v2及readiness004。相同20首原生順序與全部譜面／contact／opening身份不變，七字形與pause／prefix／freshness／STOP條件保留。20項新binary離線check、26/26整合CTest、4/4預設CTest通過；233筆RGB核對及邊界限制詳見 [共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)。新build實機NOT RUN，不自行啟動。

新版summary SHA e364bfff6d6a094e86d8ce59170c3ed6e5ad24ed731984ac99b77adb4380c6f9；plan SHA60bc2ad77b17d97b7af0b4aeb2ab571435592b25d5b1696a117765e2304708f7；binary SHA8a0de64b3a5c1cd9274ae013c27b5d633c2f3902d1f6742343ae52f361dfdb16。舊版004仍保留，但其公開script SHA已因修正更新，請使用005。

使用者以PowerShell執行Codex bundled pwsh的完整路徑，不依賴系統PATH。仍由使用者逐曲選曲→終端Enter→READY→Play，整輪同一腳本，實際失敗停止並保存原始紀錄。


更新：2026-10-24。Phigros 4.0.1／157；分支 `feature/chart-driven-multigame`。

**以下001歷史基準**：當時入檔完成，但只有1首通過原前檢，其餘19首阻擋；後續共通修正及最新入口以頁首章節為準。

## 原生來源與封存

Chapter Legacy 的 20 首曲序由安裝包 `GameInformation` 的 `MainStoryLegacy` 記錄確認，含原生 count 與下一章邊界。[章節來源證據](../../local-assets/phigros-4.0.1/chapter-legacy-metadata-001/membership.json) SHA256 為 `cfee68b06b0dbd7327eb7657a228f56729a6a04c3a770a4f702e978d808b9b39`。

- 20 首、62 份原生譜面、20 組原生／解碼音源，約 692 MB；素材全部留在 repo 外。
- 包含 20 EZ、20 HD、20 IN、Credits AT，以及 ENERGY SYNERGY MATRIX 的 `Chart_Legacy` 資產變體；後者的 UI 可選性尚未確認。
- 421 份原始產物與 20 份 WAV PCM 雜湊核對通過，來源 APK 在批次後再次核對；Dlyrotz／Glaciaxion 的 10 份既有基準檔案逐位元相同。
- Credits HD、ENERGY 的 Legacy 變體為 formatVersion 1，原始位元組完整封存；目前 v3 分析器不把它們標為解析通過。這不影響 20 份 IN 原件均已入檔的事實。

[驗證後索引](../../local-assets/phigros-4.0.1/chapter-legacy-archive-001/verified-index.json) SHA256：`99df7bf1de37cebca909578140ad7695e61cc38b3ca282749ffcae57ca2d599c`。原始 `chapter-index.json` 的 incomplete 狀態保留，衍生索引明確區分 raw_archive_complete=true 與 analysis_complete=false，沒有回寫或轉換原譜。

## 最新：共通限制補齊，20 首候選可進入人工全測

使用 [共通候選政策](CHAPTER_LEGACY_COMMON_POLICIES.md) 後，**20/20 IN** 均完成相容匯入、原生首音模型、完整接點編譯、設定檔與封存執行檔的離線啟動檢查。共 **15,817 notes／74,302 events**；所有曲目沿用 capacity5、Tap／Drag32 ms、Flick30/60/60/release24、inset64、D7000、phase−80 ms，沒有逐曲人工調參或刪音符。六首同時首音、光的 Hold 開頭、Cereris／Bonus Time 的 offset，以及原生旋轉／below／signed-speed 同色別名由共通模型處理。

[最新前置報告003](../../local-assets/phigros-4.0.1/chapter-legacy-in-readiness-003/summary.json) SHA256 `ddf0ad9d7b8ad8a0caaa3b3e9bda8cbdb6927e6c44b0785167de70356f93a27a`；[獨立核對](../../local-assets/phigros-4.0.1/chapter-legacy-in-readiness-003/independent-audit.json)驗證421份舊產物／62份原譜完全不變，31,634個音符起訖時間以 Fraction 核對最大差0 ns，15,817來源恰好一次，全部輸入位於內縮範圍。原始宣告與相容求值另有[匯入稽核](../../local-assets/phigros-4.0.1/chapter-import-compatibility-validation-001/independent-verification.json)；四首原先拒絕的 IN 有10個常數反向宣告，相關10條 tracks 的665份宣告全部保留為來源證據，並非665個錯誤。

**候選準備不等於整首遊戲資格。** 80組歷史循環 RPC 成本回放為57完成／23按原 guard停止，涉及下表9首；失敗明列 qualification_warnings，不轉成通過。UI使用明確的共通 IN HUD候選，18首的新像素與全部20首的新執行檔實機測試仍為 NOT RUN。慢RPC、視覺歧義或UI失效時仍停止，沒有放寬100 ms錨點／500 ms UI／20 ms lateness／release／STOP。

| # | 原生曲目 ID | Notes | Events | Peak | 歷史成本回放完成 |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | Glaciaxion.SunsetRay.0 | 729 | 6080 | 5 | 4/4 |
| 2 | EradicationCatastrophe.NceS.0 | 593 | 3455 | 3 | 4/4 |
| 3 | Credits.Frums.0 | 535 | 1780 | 2 | 4/4 |
| 4 | Dlyrotz.Likey.0 | 584 | 5361 | 4 | 4/4 |
| 5 | EnginexStartmelodymix.CrossingSound.0 | 433 | 2183 | 2 | 4/4 |
| 6 | 光.姜米條.0 | 517 | 2589 | 2 | 2/4 |
| 7 | Wintercube.CtymaxfeatNceS.0 | 733 | 3302 | 2 | 4/4 |
| 8 | 混乱Confusion.OnlyMyBlackScore.0 | 978 | 2395 | 2 | 4/4 |
| 9 | Cipher20.TetrajectoryfeatCtymax.0 | 1041 | 5248 | 3 | 0/4 |
| 10 | FULiAUTOSHOOTER.MYUKKE.0 | 843 | 2455 | 4 | 2/4 |
| 11 | HumaN.SOTUI.0 | 554 | 2719 | 2 | 4/4 |
| 12 | PRAW.Bluewind.0 | 1171 | 6247 | 3 | 4/4 |
| 13 | Cereris.SunsetRayNceS.0 | 898 | 4460 | 4 | 4/4 |
| 14 | PixelRebelz.Normal1zer.0 | 941 | 5087 | 3 | 3/4 |
| 15 | NonMelodicRagezMUGEdit.Normal1zer.0 | 1235 | 4099 | 2 | 2/4 |
| 16 | SultanRage.MonstDeath.0 | 485 | 2449 | 4 | 0/4 |
| 17 | ClassMemories.AntistarfeatCtymax.0 | 1194 | 3282 | 2 | 4/4 |
| 18 | SURREALISM.Itsuki.0 | 914 | 5199 | 2 | 0/4 |
| 19 | BonusTime.MegaloPaleWhite.0 | 689 | 2424 | 2 | 2/4 |
| 20 | ENERGYSYNERGYMATRIX.Tanchiky.0 | 750 | 3488 | 3 | 2/4 |

[新全測清單004](../../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-004/sweep-plan.json) SHA256 `f38c4fc4835f708764bd1f662673db3908ce52c655dfd72214c0d35f98197329`，20項、每首一輪、0個結構阻擋。舊成本失敗仍隨清單呈現；實際失敗會停止整輪，不跳過或自動重試。先前清單002／003與前檢查001／002保留為歷史；v2只驗證中心fixture，PixelRebelz斜向取點缺口已在v3補齊。

[本輪執行檔封存](../../local-assets/phigros-4.0.1/chapter-legacy-adaptive-build-002/manifest.json)：binary SHA256 `baf45968e45da26133b7b6220637deb6608d71615155b3070e61289208f711db`。26/26非音訊整合CTest、4/4預設CTest、107/107 Python通過；封存binary的20項 check-adaptive全部通過，含兩個原生中文路徑。只做離線準備，沒有啟動歌曲或送觸控。

[v3首音稽核](../../local-assets/phigros-4.0.1/chapter-legacy-opening-audit-003/README.md)以全部20首原生角度的簡化head像素測試通過20/20，85次觀察沒有刪掉真實時間；這是合成head，不是18首新實機像素資格。新的旋轉detector保留Dlyrotz／Glaciaxion既有實際anchor中心；原影片PTS回放在25 ms取樣與25 ms模擬處理下仍分別有686.681／333.209 ms送出前置時間。完整計畫／模型初始化已移至READY前，不會耗掉首音receipt新鮮度。

使用者指定**同一腳本連續執行、逐首人工選曲與Play**。單一啟動入口為 [start.ps1](../../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-004/start.ps1)：預設執行20輪、每輪Enter→READY→Play，輸出新的時間戳目錄與ledger；`-CheckOnly`純離線。已用不含裝置介面的檔案logger驗證20輪完整接續，以及第3輪故障只執行3輪便停止；替身驗證不是遊戲支援。**本輪沒有啟動實機測試，等使用者自行執行。**

從repo根目錄做整輪離線檢查：

```powershell
& tools/run-chapter-sweep.ps1 `
  -Plan ../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-004/sweep-plan.json `
  -CheckOnly `
  -BuildArchive ../local-assets/phigros-4.0.1/chapter-legacy-adaptive-build-002 `
  -KernelProfile ../local-assets/phigros-4.0.1/glaciaxion-boot-validation-001/kernel-profile.json `
  -CapacityEvidence ../local-assets/phigros-4.0.1/glaciaxion-five-evidence-004/capacity-evidence.json
```

開始實測時改為 `-Run` 並指定新的 `-OutputDirectory`。操作員逐曲選到IN、停在選曲頁，按終端Enter啟動該輪，再等READY後按遊戲Play。Emulator重啟後先重驗kernel／五指證據；檔案離線相符不能證明當前boot仍有效。

## 歷史001：原限制下的前置結果

依使用者指定，第一輪只測 **IN，每首各一輪**。共 15,817 個原生 notes；16 首完整編譯，64 組完整 FakeTouch 成本回放中 49 組完成、15 組按原 guard 停止。16 份計畫的來源指標、接點起訖、座標與 Fraction 時間獨立核對通過，時間最大差 0 ns。這些是離線檢查，沒有新增歌曲實跑或遊戲成績。

目前統一候選為 Tap／Drag32 ms、Flick lead30／travel60／distance60／末端停留24、capacity5、inset64；投影先用 D7000，若需要只依原生幾何求有界候選，上限 D10000。所有候選保留 requires_calibration。

| 曲序 | 原生 track ID | IN notes | 編譯 | 峰值 | 主要阻擋 |
| ---: | --- | ---: | --- | ---: | --- |
| 1 | `Glaciaxion.SunsetRay.0` | 729 | 通過 | 5 | 可進入人工前檢 |
| 2 | `EradicationCatastrophe.NceS.0` | 593 | 通過 | 3 | UI 參考 |
| 3 | `Credits.Frums.0` | 535 | 通過 | 2 | UI 參考、首音語義／模型 |
| 4 | `Dlyrotz.Likey.0` | 584 | 通過 | 4 | UI 參考、首音旋轉模型 |
| 5 | `EnginexStartmelodymix.CrossingSound.0` | 433 | 通過 | 2 | UI 參考、首音語義／模型 |
| 6 | `光.姜米條.0` | 517 | 通過 | 2 | UI 參考、同時首音模型、成本回放停止 |
| 7 | `Wintercube.CtymaxfeatNceS.0` | 733 | 通過 | 2 | UI 參考、首音語義／模型 |
| 8 | `混乱Confusion.OnlyMyBlackScore.0` | 978 | 通過 | 2 | UI 參考、首音旋轉模型 |
| 9 | `Cipher20.TetrajectoryfeatCtymax.0` | 1041 | 拒絕 | — | UI 參考、倒置事件區間 |
| 10 | `FULiAUTOSHOOTER.MYUKKE.0` | 843 | 拒絕 | — | UI 參考、倒置事件區間 |
| 11 | `HumaN.SOTUI.0` | 554 | 通過 | 2 | UI 參考、首音語義／模型 |
| 12 | `PRAW.Bluewind.0` | 1171 | 通過 | 3 | UI 參考、首音語義／模型、成本回放停止 |
| 13 | `Cereris.SunsetRayNceS.0` | 898 | 通過 | 4 | UI 參考、首音語義／模型、首音 offset 模型 |
| 14 | `PixelRebelz.Normal1zer.0` | 941 | 通過 | 3 | UI 參考、首音語義／模型、成本回放停止 |
| 15 | `NonMelodicRagezMUGEdit.Normal1zer.0` | 1235 | 通過 | 2 | UI 參考、同時首音模型、成本回放停止 |
| 16 | `SultanRage.MonstDeath.0` | 485 | 拒絕 | — | UI 參考、倒置事件區間 |
| 17 | `ClassMemories.AntistarfeatCtymax.0` | 1194 | 通過 | 2 | UI 參考、首音語義／模型 |
| 18 | `SURREALISM.Itsuki.0` | 914 | 通過 | 2 | UI 參考、同時首音模型、成本回放停止 |
| 19 | `BonusTime.MegaloPaleWhite.0` | 689 | 通過 | 2 | UI 參考、同時首音模型、成本回放停止、首音 offset 模型 |
| 20 | `ENERGYSYNERGYMATRIX.Tanchiky.0` | 750 | 拒絕 | — | UI 參考、倒置事件區間 |

四份 IN 匯入拒絕分別為 Cipher20 的 disappear/68、FULi AUTO SHOOTER 的 rotate/1、Sultan Rage 的 disappear/83，以及 ENERGY SYNERGY MATRIX 的 disappear/77 原生事件區間倒置。沒有為了跑完而交換端點、丟棄事件或改寫 chart hash。Cereris／Bonus Time 的正 offset 已由 importer 正規化一次，目前首音模型仍明示不接受非零 offset。

Dlyrotz 的舊 Build009 AP 封存與重跑入口保持原身份。本表是統一新版 candidate／首音模型的結果；它對 Dlyrotz 的模型／HUD 配對拒絕不回寫舊 AP 成績，也不把舊成績轉成新版通過。

[完整前置報告](../../local-assets/phigros-4.0.1/chapter-legacy-in-readiness-001/summary.json) SHA256：`7863233c51aa71d07bfd0e31784ee399597351f3039b659cb0517805ddf4355e`；[獨立核對](../../local-assets/phigros-4.0.1/chapter-legacy-in-readiness-001/independent-audit.json)。

## 已準備的測試清單與入口

[20 首 IN 測試清單](../../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-002/sweep-plan.json) 每首一輪、總上限20輪、manual selection／start、不自動 retry、失敗停止。Immutable chart/profile/hash 與目前 boot 的 kernel／五指證據分開；重開 emulator 需重新驗證後者。

```powershell
# 預設只核對檔案與列出全部阻擋，不連裝置、不輸入。
& ./tools/run-chapter-sweep.ps1 -Plan ../local-assets/phigros-4.0.1/chapter-legacy-IN-sweep-002/sweep-plan.json
```

只有全清單的 blocker 都解除後，才可用 `-Run` 加上明確 BuildArchive、KernelProfile、CapacityEvidence 與新 OutputDirectory 開始；入口在任何一首仍 blocked 時會拒絕啟動整輪，不自動略過。每首選曲後人工確認，等待該程式 READY 再按 Play。

可用 `tools/archive_phigros_chapter.py` 重建新的本機素材封存，`tools/prepare_chapter_sweep.py` 以索引／報告雜湊產生新的有限清單。所有工具的預設／單元測試保持離線。

## 與自適應調參的關係

Glaciaxion 已有兩輪完整輸入與獨立非 AP 結算，見[跨曲目實驗](ADAPTIVE_SONG_TESTS.md)。自適應回饋與候選選擇尚在分析階段；使用者要求先完成本章入檔與全測準備，因此沒有新增無人重試或把失敗輪改為成功。

共用 IN HUD 有明示 opt-in 的離線原型，保留兩曲正例與選單／暫停／結算／系統列負例，以及分數區被判定線穿越造成 unknown 的限制；它沒有使另外18首自動取得 UI 或連續輸入資格，也尚未解除本輪的 UI blocker。

原 PAS sources 的 extraction blobs 保持不變；main 未合併。圖表、音源、校正與原始遊戲證據不加入 Git。
