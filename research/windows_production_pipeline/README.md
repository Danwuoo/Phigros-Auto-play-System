# 正式管線的 endpoint-free 成本工具

這個 C++20 工具逐字 include frozen `src/manual_session.cpp`，連結同源 production
core／current／proto。`pas_emulator.lib` 不連結；discovery、preflight、capture、touch
只有本 executable 的測試替身，拒绝任何 endpoint／token。沒有修改正式 admission。
先调用正式 public 函式，必须在全部替身调用前拒绝 UNQUALIFIED，才可由本 TU 内部
测试 wrapper 执行正式实现。这个 wrapper 不存在于 production executable。

實際執行原有三執行緒、Packet 最新槽、三個 pool buffer、Windows event／high-resolution
timer、SessionPerception、唯一 action owner、ReceiptStream、SessionArchive 與 supervisor。
Tap／Hold 用已凍結 R2 renderer；dense 與原負載同像素。全 attempt QPC、完整正式 journal、
owner／packet／hook／phase quantile 與 OS peak RSS 都保留。256 warmup 不刪除；末尾最多
64 張明列 synthetic result pixels 使 one_round 正常結束，不以 timeout 冒稱曲尾。

工具執行成功不授予成本、capture／RPC、目前 emulator、五指、Hold 語義或 live 資格。
journal 原正式欄位的 `real_input` 是被測原程式內容；在此工具一律測試替身，不能當裝置證據。
成本門檻須另由獨立 auditor 全分母重算，不能拿本工具自己的 report 當資格核准。
