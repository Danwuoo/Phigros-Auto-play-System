# Windows 離線 BVI 接手入口（2026-10-06）

本目錄只建立離線研究 target；正式 runtime、observer、owner、scheduler 與
`research/bvi_cold_v3` 的七份 core source 沒有改動。完整結果與限制見
[Windows 里程碑](../../docs/research/zero-miss-20261006/windows/README.md)。

## Target 與來源

- 本層 CMake：凍結 v3 的原契約、新契約、獨立 controls；另建立原 356
  layer-cases／3938 assertions 的 `legacy` 與獨立 `audit_io`。36 項失敗保留，
  原套件不註冊成可被 `WILL_FAIL` 掩蓋的全綠 CTest。
- `bridge/` CMake：重新編譯 root `pas_core` 的正式來源閉包與原封 v3；
  `bridge_tests` 使用單一正式 owner／scheduler 與有界 fake backend；
  `pixel_chain` 使用既有 PNG、當幀正式 GameObserver ROI／完整量測線及候選自己的 receipts。
- 沒有 capture、裝置連線或真實注入入口；沒有用歷史按鍵 seed contact。

## 維護入口與重新執行

`tools/zero_miss_windows` 的 PowerShell 只負責本機建置、SHA、程序交易及收據。
所有新的像素、候選、接觸前綴分析仍是 C++20。工具路徑記錄本工作站的
CMake／Ninja／VS／SDK；搬至另一工作站先重新核實，不能把已產生的 gate 當成可攜授權。

順序是新 root 的 `pretest.ps1` → `qualify.ps1` → `cmake-stage.ps1` →
`native-stage.ps1`／`legacy-stage.ps1`／`pixel-stage.ps1`。
前三者的具體 argv 與 hash 在 Windows 里程碑的 receipts 內。
每個 stage、build root、report 必須使用新名稱；拒絕覆寫是預期保護。
需要重新編譯時，可以使用自己建立的新 build root；不可清理歷史 STOP 或使用者 checkout。

`pixel-stage.ps1` 保持原安全核心的 64 KiB freeze 限額。
256 張 PNG 另以有界葉節點 manifest 在 native stage 前後逐一核 SHA；
native freeze 包含該 manifest、完整 build freeze 與 selection。
單靠 native exit0 或 JSON 存在不表示整個查核通過。

## 尚未取得的資格

Body patch 被拒絕，不能冒稱 seen front；跨 epoch／generation／geometry 的
attachment 必須停止並驗證釋放，另建立新 owner context。
未知注入、completed、取消且 cursor 缺失的身分不能復活。
沒有 cursor 不轉成零；完整 cursor／prefix_offset 保留 uint64。

真圖的安全拒絕不等於合法機會完整性。短窗沒有人工物件分母，fake receipt
不代表遊戲採納；順序 PNG 重播也沒有 latest-frame producer、RPC、journal
壓力或 A/A→A/B 成本資格。本目錄的通過不能開啟實機或完成全曲 zero miss。
