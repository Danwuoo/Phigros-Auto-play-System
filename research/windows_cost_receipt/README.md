# 唯讀 Windows 成本收據 CLI

只讀驗證 `pas.current-rails-cost-input.v2` 的單筆 gate input，必須由呼叫者提供
外部 SHA256、完整 source commit 及已凍結 analyzer 路徑。直接重用現有 C++20
`cost_analysis.cpp` 的 campaign／process／source／profile／raw audit／full＋measurement
predicates；沒有新演算法替換門檻。

exit0 表示完整性已驗證；`offline_cost_valid` 才是數值門檻結果。真實 NOT_READY
收據仍能驗證，失敗理由保留。任何綁定、程序身分、原始證據、SHA 或來源問題 exit2。
`live_ready=false` 固定，沒有 mutable token、live admission、device endpoint 或程序launch。
完整 AA/noise/ABBA、正式 production capture/RPC、目前 emulator/五指及 physical 仍分別必需。

```
pas_cost_receipt_verify --input <gate-input.json> --expected-sha256 <SHA256> --source-commit <40 hex> --analyzer <frozen cost_analysis.exe> --output <fresh verification.json>
```
