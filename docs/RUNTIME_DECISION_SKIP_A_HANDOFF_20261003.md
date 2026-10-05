# 選項 A 交接：有限 raw 診斷完成

2026-10-03 Asia/Taipei。**選項A交付完成，待總控獨立驗收；成本資格仍not-ready。** 讀[protocol](RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md)、[result](RUNTIME_DECISION_SKIP_A_RESULT_20261003.md)與`measurements/runtime-decision-skip-a/final-summary.json`。HEAD/main f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c／唯一worktree；正式86檔不變，既有dirty/untracked保留。

最重要驗收點：193owner decision skip／85consumer skip各有row；51份publication下界在前accept結束後、136份上界在前accept開始前，餘6份對該publication邊界不能定位。6份exploratory整個residency可界定在前selection→accept；所有causal attribution仍Unknown。191/193次下一recognition≤4ms，但owned966/2281亦群聚；185skip edges全部按先驗匹配，skip enqueue p50=1.0035ms/control1.0447ms，沒有更高成本的因果證明。

RGB event/release raw attempt實為decision sequence，與source frame在consumer skip後不同；`source-anchor-map.jsonl`明列映射。owner stimulus兩者相同，所以193題不受影響。新reader解析archive sequence/frame_sequence，原R3 windows/gate/ledger不回寫、不重評資格。

## 機读入口及凍結

- `input-manifest-v2.json`94精確reference（原92保留）；含五run每個檔、完整archive summaries/manifests/segments、兩套ledger及source/input/method/environment/qualification/controller證據，raw未複製。`initial-binding-verification.json`／`after-analysis-verification.json`分開核原R3與controller ledger。R1 source305另核。
- `source-binary-binding-v2.json`／`compiled-dependency-freeze-v2.json`為最終新工具來源、binary/map、396實際CL/link/compiler inputs；`source-snapshot-v2`是固定副本。初版freeze29只抓到link/部分paths，v2補CL前綴；不拿初版當完整依賴核對。沒有pas_core link或新runtime。
- `deterministic-final-1`與`deterministic-final-2`全部16檔一致：`report.json`七固定windows／phase／quantiles；`decision-skips.jsonl`194份（owner193＋RGB1）；`consumer-skips.jsonl`139份（owner85＋RGB54）；全`owned-edges`、`consumed-arrivals`、`matched-controls`、`source-anchor-map`；8SVG。兩種skip不能混算，零unknown raw欄轉null。
- `tests-final-release.json`／`tests-final-asan.json`各17pass/0fail；`verify-asan.json`另核五run完整reader/joins/metrics。所有commands/exits/logs與已聲明工程失敗留存。原R3/R1/R2大suite、bridge、runtime/cost/stress只引用，沒有本次重跑。
- `artifact-ledger.json`凍結新batch所有文件，排自身及final-summary，兩者hash由final-summary/交付輸出另記；`documents-delivery`綁文件快照，mutable workspace status/forward只核原byte prefix，不當永久immutable。

## 獨立驗收建議（不用build或新runtime）

先逐檔核input/output/工具freeze與兩套R3 ledger SHA/長度，再讀全部193owner rows的bound與Unknown，以及先驗／exploratory界線。所有現有logs、初版analysis及失敗不可覆寫。總控新ReviewRoot限制≤16MiB，保留另free5GiB，不進原R3 normal slots。

```powershell
# 從repo根，只使用新不存在的路徑。此為重現入口，不是追加cost。
./out/decision-skip-a/release/Release/decision_skip_a.exe self-test ./measurements/runtime-decision-skip-a/controller-review/tests.json
./out/decision-skip-a/release/Release/decision_skip_a.exe verify-input ./measurements/runtime-decision-skip-a/input-manifest-v2.json ./measurements/runtime-decision-skip-a/controller-review/raw-verification.json
# verify-input解析完整raw與bounds，不新建另份約12MiB全row報告。
```

先建立controller-review目錄並計算配額；上兩命令預期exit0。若要逐byte重現整份analysis，用`analyze INPUT_MANIFEST NEW_OUTPUT`，單份約12MiB，須先核16MiB剩餘額度；不重build frozen工具、不覆寫任何既有output。ASan驗收沿最終`out/decision-skip-a/asan/RelWithDebInfo/decision_skip_a.exe`，只用新output；不是新OS並行成本run。

## 總控下一決策

建議選項B僅做有界量測契約審查：最小publication／selection時刻能否補193題的actor邊界？RGB anchor命名是否需要澄清？只提案，未改插樁、cadence、90%、noise、diagnostics或production。若不補這兩時刻，本raw識別性已到停點，考慮停止此線、另案X10d-O；不得自動R4、補owner2–4、用少skip/Down說Miss改善。X12仍無資格，14lost機會/77Miss/真閉環/跨曲Unknown。此chat完成後停止、不另開task、不傳訊、不goal/automation/emulator/ADB/live/model/commit/push。
