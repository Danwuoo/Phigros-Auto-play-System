# 唯讀 ABBA 成本核對器

保持原 `PRELIVE_CURRENT_COST_PROTOCOL.md` 的 A/A noise 及 A/B/B/A 比較公式。
程式重用 frozen `cost_analysis.cpp` 的 run validity、raw audit、source／profile 與
直接 producer Job 身分核對。每次核對先重驗完整未停批的 AA12，再重算 noise 並與
凍結收據完全匹配；不單靠 `comparison_allowed=true`。

新的 comparison campaign 仍採 producer 支援的 v2 schema；必須預先綁定 AA input、
AA campaign、noise 與本核對器 binary，36 個計畫依 Tap／Hold／dense 各三批 A/B/B/A。
固定 candidate／source／corpus／profile／guard／原 analyzer 與 auditor；只允許同機
新 environment 及 maintenance recipe。每筆 raw、PID＋creation、report 不能重用。

`--operation run` 是必要 validity；`compare` 要完整 36 筆、未停批及每批 p95／p99
不退步。`C5_improvement_three_batches` 另外表示至少一個 load 的三批都存在超過 T
的 observer 或 owner 改善，並傳遞到同 quantile 的 capture→owner。
`ready` 只表示 endpoint-free comparison 不退步，不能單獨聲稱 C5 優化收益或 live 資格。

```
pas_cost_comparison --operation run|compare --input INPUT --expected-sha256 SHA256 --source-commit COMMIT --analyzer FROZEN_ANALYZER --output FRESH
```

頂層 input 使用 `pas.windows-cost-comparison-input.v1`，以 path／bytes／sha256
綁定 `aa_input`、`noise`、`campaign`；單筆含 `plan_index`／`run`，完整比較含
`runs`／`normal_stopped`。錯誤／竄改 exit2，真實數值 NOT_READY exit0 並保留理由。
輸出固定 `live_ready=false`、`device_operations=0`，沒有裝置或 admission API。

只有完整 AA／noise 通過才啟動 comparison；工具開發和合成功能測試不代表已做 ABBA。
