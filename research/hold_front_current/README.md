# Current-pixel Hold 終端候選（離線、2026-10-06）

只新增隔離 C++20 producer、typed consumer 與 dispatch 後 sidecar。正式 src/include、
既有 bridge、七份 v3 core、色彩/effect/ownership 契約和 oracle 不改。
完整交付入口為 [FRONT_CANDIDATE](../../docs/research/zero-miss-20261006/windows/FRONT_CANDIDATE.md)。

`front.hpp` 把 observer reconstructed front 與 current RGB visible terminal 分開。
終端的 head role 保留 proposed；tail proposal 只用於局部法向符號，tail boundary、
physical ownership、合法觸點及採納全部 unknown。`Projection` 明示
head_role_confirmed=false、action_authorized=false，沒有 owner 或注入 API。

固定 geometry search：最多128完整候選／16條線，width16–512px；每 Hold 沿局部
法向 ±32px，5個 interior lanes與每側5個rail取樣，共975 probes。終端搜尋 ±20px，
每lane的 inside→outside正亮度跳變需總channel差≥60且最大差≥40，容許法向1px取樣
差。終端前4–12px的9列，各側至少6列white rails（沿用≥240定義）；外側2–4px
最多1列rail。只接受單一終端cluster。輸出各lane相鄰pixel/RGB與最小最大offset區間，
不是 rails 最末像素，也不是固定9px或 body depth/2。

非有限／越界／不完整storage、context不一致、真正容量溢出、無法向、裁切、多端部、
rails/contrast不足都 abstain。Body patch 明確 body_interior，不能投影head。
consumer 以完整 same-frame context、原ROI及 current RGB重新核typed proof；測量不保留
歷史frame，不讀ordinal、歌名、annotation、舊按鍵或未來圖。duplicate只保留frame-local
證據，不升格物理同一物件；neighbor不自動合併。

`front_chain.cpp` 是凍結 bridge pixel_chain 的另存隔離 host，舊策略照跑；新量測在
owner dispatch後輸出。新邊界只進 fresh v3 extract-only shadow，不呼叫relate/guard/owner。
`front_audit.cpp` 另解全部原PNG，独立核完整offline context、inside/outside RGB、法向、
區間與rails，並比較256幀舊決策；不呼叫新measure算法作為其判準。人工annotation
僅audit末端外部比對，不進producer。

三配置借用已驗774f3e7的相符formal/v3/current_bridge/pixel_diagnostics libraries，
`run.ps1` 逐核舊build freeze、收據及 sealed external binary SHA，CMake再核四個lib SHA。
新sources/targets在全新roots編譯；MSVC ASan也使用原ASan完整closure，沒有unsanitized
owner替代。維護腳本沿未改qualified process wrappers；未知身分仍STOP。重跑使用新
BuildAttempt/Attempt；先建立新的容量帳，保持out≤2GiB、metadata≤16MiB、free≥20GiB。

```powershell
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation build -BuildAttempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation tests -BuildAttempt review-01 -Attempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation pixels -BuildAttempt review-01 -Attempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation audit -BuildAttempt review-01 -Attempt review-01
```

debug/asan同法。舊attempt／STOP不能重開。無root349／3938重跑，因策略與原閉包不變。
原36fail、timeouts、skips仍保留。原鏈compute不包含新sidecar；新measure/consume/shadow及
JSON構造另記全256幀QPC分布，兩者皆不是完整即時成本資格，不相加各stage p99。
