# 離線架構研究

此目錄保存可重現的 C++ 小實驗，不接 Capture／Touch。舊 `pas_line_identity_probe` 對應 M0 未修改的原始 production source 與舊輸出，**不要用現行修改後的source重跑並覆寫M0 `synthetic.json`**。M1另有靜態線配對消融、production線配對耗時與保存RGB三幀離線重播；它們不注入觸控，也不是遊戲動作語義驗收。

```powershell
cmake -S research -B out/line-policy-research -G 'Visual Studio 18 2026' -A x64 -T v145 `
  '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275'
cmake --build out/line-policy-research --config Release --target pas_line_identity_probe --parallel 4
New-Item -ItemType Directory -Force measurements/line-policy-m1-20260928 | Out-Null
$roundRoot='measurements/game-assist/manual-session-108176133899800/round-15'
$summary=Get-Content "$roundRoot/summary.json" -Raw | ConvertFrom-Json
$segments=@($summary.event_segments | ForEach-Object { Join-Path $roundRoot $_.path })
& out/line-policy-research/Release/pas_line_identity_probe.exe `
  measurements/line-policy-m1-20260928/m0-probe-current-source-rerun.json @segments
```

換機須修改 VS instance 與已安裝依賴 include 路徑（`PAS_DEPENDENCY_INCLUDE`）；本工具不下載新依賴。省略 event segment 參數只跑合成案例。可選摘錄針對已記錄 Pixel Rebelz round15／frame238007，沒有把曲名或歷史動作交給正式 runtime。

報告自帶 motion source／header／probe hash；來源改變會觸發 CMake 重新產生 provenance。上方命令使用**現行修改後source**，輸出不能取代M0原版結果。原M0七項輸出案例中，一項為前一案例追加空觀測的延續，不能當七個獨立實驗。參見 [M0 研究](../docs/catalog/04-offline-research/LINE_IDENTITY_M0_20260928.md)。

## M1 新工具

從同一research CMake來源另建`out/line-policy-research`（不要使用凍結binary目錄），只建新目標：

```powershell
cmake -S research -B out/line-policy-research -G 'Visual Studio 18 2026' -A x64 -T v145 `
  '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275'
cmake --build out/line-policy-research --config Release `
  --target pas_line_assignment_compare pas_line_policy_bench pas_pixel_clip_probe --parallel 4
```

`pas_line_assignment_compare`在靜態水平線固定序列中比較最小出生修正和現行production全域指派；`pas_line_policy_bench`用production `game_motion.cpp`做1／2／16線單段耗時分布。兩者只接受未存在的輸出JSON路徑。`pas_pixel_clip_probe`以當前`out/release-v145/Release/pas_core.lib`從空observer狀態重播指定的三張保存RGB，同樣拒絕覆寫輸出；需先重建當前Release library。其輸入index需仍有原RGB，不能只靠Git恢復已刪ignored資料。具體命令、source／結果hash、限制與新容量帳本見[判定線M1實作紀錄](../docs/catalog/04-offline-research/JUDGMENT_LINE_M1_IMPLEMENTATION_20260928.md)。
