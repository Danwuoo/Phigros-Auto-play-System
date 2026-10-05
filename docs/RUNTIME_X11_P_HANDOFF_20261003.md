# X11-P 交接及 X12 review 預案

2026-10-03。**待總控獨立驗收；not-ready，X12阻擋。** 詳[結果](RUNTIME_X11_P_RESULT_20261003.md)、[預先protocol](RUNTIME_X11_P_PROTOCOL_20261003.md)，機讀入口 `runtime-x11-p/final-summary.json`。交付完成後本chat停止；沒有另外派chat、傳訊、goal、X10d-O或live。

## 獨立驗收入口

1. 核 `source-binding-before-cost.json`／`compiled-dependency-freeze.json`／`source-inverse-audit.json`；B1 inverse hook恢復B0、共用metadata對稱、舊frozen綁定與正式checkout保留。1490實際CL/link/compiler/proto依賴與closure已列SHA；generated provenance新生成，parent不誤記main。
2. 看四個完整原/新contracts XML，B0 historical補跑XML；B1原suite唯一允許的fail仍原pending-grace test。新test移除兩個private introspection的diff另外保存，不把public輸出當完整private-state檢查。
3. 每版新核心 bridge和frozen研究核心 bridge的 `public-events.jsonl` SHA/bytes相同。是512 synthetic/FakeClock橋接，沒有新的7722PNG重跑；原X10d-P的14lost/5fresh-return、全observer相同證據只屬其原binary。
4. 先核 A/A命令、noise-frozen的時序與SHA，再看ABBA。noise不充分且dense owner兩批p99非退步fail，baseline一筆lateness p99也fail。不能只挑capture→owner通過值、少Down或ASan pass改ready。
5. 讀每run summary及raw frames/receipts、取消/拒絕分母和capacity；再核全部source/DLL/profile/failed logs及final帳本。沒有重建或刪舊raw。

純離線binary查核（可重跑，但新logs另存；以下不連裝置）：

```powershell
./out/x11-p/runtime-B0/pas.exe x11-provenance
./out/x11-p/runtime-B1/pas.exe x11-provenance
./out/x11-p/runtime-B0/pas.exe --help
./out/x11-p/runtime-B1/pas.exe --help
```

重現工程入口是 tools/prepare-runtime-x11-p.ps1、run-runtime-x11-p.ps1、bind-runtime-x11-p.ps1、audit-runtime-x11-p-sources.ps1、freeze-runtime-x11-p-dependencies.ps1。它們對既有frozen根拒覆寫；不要直接再跑Build/AA/ABBA/Stress。原run額度24＋4已用盡。完整CMakeCache/vcxproj/command JSON在batch或out/x11-p保存，重建要新protocol/容量/目錄與新binarySHA，不能重新賦予這份測量。

## X12 exact paths（準備，不執行）

```text
B0 C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/x11-p/runtime-B0/pas.exe
   SHA 81cb1cf57939e0020dabb503aba5b43cee1eb1503a27f7a4f34d6cab08939c14
B1 C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/x11-p/runtime-B1/pas.exe
   SHA 0b93f561c3ed4011db8f3c252e3f97781d321468a7d9501e87944d531da3112f
profile measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/x12-profile-preview.json
capability measurements/game-semantics-20260927/touch-five/summary.json
output（尚未建立）measurements/game-assist/forward-live-20261003/sessions
```

preview profile只改父profile的log_dir，其他欄位逐一相同：lead35/uncertainty30、gRPC payload fast/RGB888 top-down/256KiB、1280×720/source rotation1、五指720×1280/rotation90、128plans/16steps/350ms/100ms evidence。父profileSHA `4201679acc8ded39a394852d5f8d7f11fd62e736c32f10aa77657fc8fdbef18b`；preview及capability的SHA另列finalfreeze。歷史fingerprint只能靜態核對，X12必重新device/installed APK/geometry/mapping preflight；X11無ADB probe。

conditional X12的每輪確切CLI形式如下；**目前cost gate failed，禁止執行此預案**：

```powershell
./out/x11-p/runtime-B0/pas.exe manual-session --config measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/x12-profile-preview.json --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview --one-round --round-watchdog-s 300
# B1只替換exe路徑為runtime-B1/pas.exe，其他完全相同。
```

先兩輪同譜 B0/B0估A/A波動，再B0/B1/B1/B0，共最多6個attempt（中止也計），全部journal/result/latency/score/P/G/B/M/combo保存；選曲和Play由使用者/已授權UI操作，歌曲身分不得進策略。同一profile/lead/capturebackend/裝置設定。A/A不穩就停止效果因果宣稱；unknown注入、釋放failed/unknown、mapping/geometry變更立即停止，Ctrl+C/Escape後確認release report，不重試Down、不挑最佳輪。

本預案不開full-recording或pixel-clips；六輪source原硬上限journal512MiB/round約3GiB，另預留小型result/manifest，future live根≤12GiB、disk reserve另5GiB。**forward plan另要求每輪journal≤256MiB，但原C36h runtime沒有CLI可把32×16MiB改成256MiB。** 尚缺可驗的外部停止guard或另案容量設定，不能只靠口頭監看冒稱已強制256MiB；本次未改runtime archive契約，這也是X12操作尚未可執行的缺口。沒有自動建立live根或啟動任何裝置。

本輪已完成兩個完整runtime來源/DLL/metadata與冷驗工具；下一工作是總控驗收研究結果並選擇是否修成本證據缺口。不能順帶開X10d-O、恢復X10b、混另一策略或用本負結果宣稱遊戲退步；原14lost opportunity及77 Miss仍需未來合格有限閉環比較才能回答。
