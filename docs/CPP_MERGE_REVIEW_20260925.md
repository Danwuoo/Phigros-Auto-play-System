# C++ 遷移合併驗收（2026-09-25）

審查起點：`codex/cpp-migration-t0-t5` 的 `cb09cfb`，目標 `main` 的 `b5da5c8`。本次結論為 **C++ 遷移主線可合併**；不代表性能數值已通過、五候選已完成，或遊戲 assist 可啟用。

## 合併前修正

- `offline-capture-bench` 接受 1×1／2×1 畫面，原本卻固定寫入八個 counter bytes，超出三／六 byte RGB buffer。改成按實際容量寫入，加入 CLI 回歸並在 ASan 下執行。正式消費窗口同時排除 warmup frame 與結束邊界之後的 frame。
- `synthetic` 原本用 Fixture 真值標記 `effects_seen`，並以生成器的目標索引指定意圖身分。改以紅色效果 pixels 的出現邊界計數，意圖身分由視覺 tracker／predictor 產生；真值只留在場景生成與離線評分。
- 25 ms 辨識延遲回歸揭露虛擬時間會跳過 scheduler deadline。合成模型現在於辨識期間繼續執行到期排程，符合專用排程 owner 的架構；其日誌 clock domain 改為 `synthetic_virtual_ns`，不再標成主機 QPC。
- 補上已安裝依賴版本及原始授權副本，見 [第三方紀錄](THIRD_PARTY_NOTICES.md)。

## 本輪實際驗證

- 修正後重新建置 Release、Debug、ASan，每次最多四個並行工作；各 **21/21 CTest 通過**。包含原有 frame lease、排程三項 P1、真 gRPC loopback、錯誤 payload／source reset、設定與分析測試，以及新增的兩項 CLI 回歸。
- 合成閉環在 0／5／25 ms 辨識延遲下，各 30/30 命中、30 次可見效果、零誤觸；這是虛擬時間正確性，不是主機性能結果。
- 用 C++ 重新分析正常三批及壓力／穩定性七批，十份 raw SHA-256 全部吻合，兩個 campaign 的 `all_valid` 與 `environment_consistent` 均為 true，`performance_pass` 仍為 null。
- 移除 Python／venv／conda PATH 後，Release 的 fake observe 成功執行，未建立 input backend。
- 來源 manifest 所列 54 個程式、測試、設定與 Fixture 檔，在 legacy 中都有 SHA-256 完全相同的副本。桌面原工作目錄與來源快照只差已知的四個 recovered override；沒有發現額外未交接的開發修改。
- 核對既有 native Touch Fixture 報告為逐指路徑 180/180、取消 30/30，APK hash 相符。本輪 `probe --serial emulator-5554` 回報裝置未連線，**沒有重新執行實機擷取／觸控**；實機結果沿用並保存前次原始證據，沒有把離線測試冒充新實機測試。

## 證據保存與合併保護

工作樹內 108 個量測／Fixture 檔案（41,097,229 bytes，包含失敗紀錄）已逐檔複製、核對 SHA-256，保存在桌面專案 `measurements/` 的原相對路徑。複本清單為 `measurements/cpp_merge_review_20260925/preserved-evidence.json`。這些原始資料仍被 Git 忽略，不隨程式提交，也不刪除工作樹中的原檔。

主目錄原有已修改／未追蹤檔案在合併前以具名 Git stash 保存，另將 stash ID 寫入上述 review 資料夾。這批來源已納入 legacy／文件，合併後不直接套回舊根目錄，以免重新引入正式 Python 路徑；備份保留供還原。

## 保留的限制

- 性能 p95／p99、掉幀、空窗等數值門檻仍由使用者決定。既有正常批次使用 **2 秒暖機**，與計畫的 10 秒不同；它們可作探索基線，後續正式候選比較須使用預先固定的 10 秒暖機＋60 秒正式窗口，不能視為已符合該比較規格。
- ASan 的預編譯第三方二進位未受插樁，現有 preset 也保留兩版 MSVC／ASan 元件搭配的限制；詳見 [開發驗收紀錄](CPP_ACCEPTANCE_20260925.md)。
- 真外部網路中斷／RPC timeout、預覽外觀人工 QA，以及矩陣中標明待補的細項，不因本次合併變成已驗證。現階段不啟用遊戲觸控；新擷取候選另開階段。
