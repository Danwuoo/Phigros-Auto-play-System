# 哪些 gitignored 檔案值得補？

日期：2026-10-05。基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。

## 結論

**本輪原始碼盤點、合成研究與方案交付不必等補檔。** Git 中的正式原始碼、測試、BVI 快照與逐曲 JSON 已足夠；本次雲端 checkout 沒有 `measurements/` 或 `out/`。這只表示它們未隨 Git 傳來，不表示使用者電腦上的資料已遺失。

要重驗原始遊戲證據、重跑完整既有 BVI cases 或定位 Windows 失敗，則需要下列精選輸入。先提供小型 metadata／收據，再由其引用挑必要 raw，不要求整個 ignored 目錄。原圖 SHA、來源 manifest 和逐輪配對應一併保留，不能只給壓縮後的聊天截圖。

`.gitignore` 排除了 `measurements/`、`out/`、`__pycache__/`、`*.py[cod]`、`.venv/`、`*.egg-info/`。本輪未變更 ignore 規則；研究報告和小型合成 probe 放在 Git 可追蹤的新文件目錄。

## 最小清單，依要回答的問題索取

以下 `M/` 只是本文件的縮寫，代表 repo 相對根：

`measurements/game-assist/2026-09-30-m0-manual-continue/`

| 問題／優先性 | 最小輸入 | 可以建立的證據；仍不能建立的證據 |
|---|---|---|
| Windows configure 到底哪一層失敗？下一輪修 wrapper 前優先 | `M/hold-ownership-x10d-o-bvi-r2f-control/` 中 `configure-release-command.json`、`configure-release-result.json`、`configure-release-verification.json`、`configure-release.stderr.log`、`configure-release.stdout.log`、`contract.json`、`freeze.json`、`state.json` | 可核原始 argv、三 exit、當次來源與停止點；若未記逐步 marker，仍不能倒推具體 offending step，需要新的有限 Windows probe |
| D19/D20 是已定位的 adapter 問題嗎？有助獨立重核 | `M/hold-ownership-x10d-o-bvi-build/` 中 `diagnostic-pretest.json`、`diagnostic-oracle.json`、`failure-classification.json`、`state.json`、`input-manifest.json`、`final-receipt.json` | 可重核 18/20、native 前拒絕與 STOP；原始 source 的 slash／prefix 矛盾本輪已可直接讀到，毋須等這些檔才能研究 |
| 完整既有 BVI 356 layer-cases 是否成立？既有 suite 重跑必需 | `M/hold-ownership-x10d-o-bvi/{normalized-execution.json,oracle.json}`；`M/hold-ownership-x10d-o-bvi-r1/{typed-r1.json,r1-cases.json,expected-coverage.json,specification-freeze.json}`，以及這些 manifest 實際引用而未內嵌的輸入 | 能準備重跑既有 frozen cases，而非自造小 probe 取代原 suite；Git 內 `research/x10d_o_bvi/supplemental.json` 已有，不必重傳 |
| 最近的 Dlyrotz IN M77 原結算可信嗎？先取一輪即可 | `M/acceptance36h-01/sessions/manual-session-16174738262800/manifest.json`，同根 `round-1/{manifest.json,summary.json,result.png}` | 可核該輪圖／數字／難度／版本配對；只證明該輪，不能替其他曲、目前解鎖或全曲驗收背書。若核完整開始→結算／釋放，再加同 session `rounds.jsonl` 與該輪 `events-*.jsonl` |
| 某一視覺／線關聯假說在真圖中是否出現？選一個事件 | 一段既选 clip 的原生 PNG/RGB、matching `index.jsonl`、事件窗 journal/events，以及 run manifest／有效 profile／source、binary SHA。需涵蓋事件前至少 90ms 的真觀測、事件及必要事後幀；具體 ordinal／引用由原索引核對 | 可隔離局部 pixels／association／時間問題。90ms 只是近期 tracking 暖機下限，**不恢復更早已 Down 的 Hold 或完成 identity**；無初始狀態不能宣稱完整 owner replay |
| 既有 X1 contact replay 的可信範圍？先讀小型輸出 | `M/contact-replay-x1/{input-manifest.json,input-source-audit.json,tool-freeze-verified.json,five-cases-verified.json,prefix-first-divergence-verified.json}`、`source-v2/*-source-provenance.json`，以及被 manifest 明確引用的 `c36h-verified-on-1`／`main50-verified-on-1` summary、窗口 trace、prefix events | 可先審既有重播的證據鏈。單給 109 幀案例窗不夠；活動接觸可能在窗前建立。必須有可核完整 prefix 或等價且可重建的 owner/scheduler checkpoint，不能從 body 假造 Down |
| 要用現有工具重新做全輪 X1 pixels→owner replay？較大、非本輪必需 | 現有工具要求 `M/full-recording36g-01/sessions/manual-session-22885039263800/` 的完整 recording/index、所需兩個 journal segments、profile／manifest，按原 manifest 從最早 preroll 到 EOF，歷史記錄為 7722 PNG | 這是現工具的完整模式，不承諾任選小 clip 等價。若先前小型收據已足夠回答問題，不索取此大包；若改成 checkpoint/window replay，應另外設計與驗證工具，而非默認已支援 |
| 現在到底有幾首、哪些 IN 能選？產品驗收前必需，但不是舊 raw 可解 | 當前遊戲版本畫面；Chapter Legacy 從首到尾的去識別連續列表（相鄰畫面重疊）；各曲 IN 可選／鎖定與畫面提示 | 才能凍結目前章節分母與解鎖前提；歷史 HD 分數、曾玩過某個 IN 都不能替代目前帳號狀態。無須提供帳號或存檔檔案 |

## 不需要提供

- 整個 `out/`、vcpkg cache、SDK、IDE cache、Python virtualenv、臨時編譯檔。
- 整個 emulator image、遊戲 APK、遊戲內部存檔、登入憑證、token、密碼或個人帳號頁。
- 模型權重、LibTorch runtime 或所有歷史錄影。模型訓練與即時模型不在本轮範圍。
- 為了讓報告「看起來完整」而補無法配對 manifest／SHA 的截圖或重製紀錄。

## 分享及重現注意事項

1. 只取已命名的最小檔案；若 log 有 username／device serial／私人路徑，另做去識別分享副本，原封存證據不覆寫。變更 bytes 的副本應標記衍生檔及新 SHA，不能宣稱仍匹配原 hash。
2. 不需要取消 `.gitignore`、把 raw 永久 commit 或公開 push。來源可以是使用者選定的附件或授權可讀位置；本輪不自行切換到使用者電腦找檔。
3. `null`、`unknown`、找不到引用應留在清單。沒有 raw 的研究結論標為 source 檢查／歷史轉錄，不能升格為原始畫面重驗。
4. 正式 replay 還依賴 exact binary／source、有效 profile／geometry／mapping、clock/cadence 契約；僅有 PNG 不表示可重現當時觸控結果。

更多細節：[建置與測試](BUILD_TEST_AUDIT.md)、[視覺與時間](VISION_TIMING_RESEARCH.md)、[觸控排程](TOUCH_SCHEDULER_RESEARCH.md)、[全曲驗收](LEGACY_ACCEPTANCE_RESEARCH.md)。
