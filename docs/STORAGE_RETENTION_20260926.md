# 畫面保留與暫存清理

使用者要求清理測試暫存，並避免處理過的畫面持續累積。

## 空間盤點與清理範圍

本次盤點工作區約 42,221 MiB 位於 `out/vcpkg_installed`；圖片共 309 檔、15,753,526 bytes。主要空間來自依賴的建置中間產物和打包 staging，不是逐幀錄影。

清理前驗證目標絕對路徑位於此工作區，沒有 reparse point，沒有進行中的 PAS／建置程序。僅刪除：

- `out/vcpkg_installed/vcpkg/blds`：43,351 檔，21,418,051,165 bytes。
- `out/vcpkg_installed/vcpkg/pkgs`：2,191 檔，11,426,816,221 bytes。
- Windows TEMP 下 17 個超過一天、可獨占開啟的 `pas-emulator-mmap-*.bin`：380,283,574 bytes；沒有掃描刪除其他 TEMP 檔案。

前兩項合計約 30.59 GiB 邏輯大小，但部分打包檔為 hard link，因此刪除後當下 C 槽實際增加約 20.00 GiB；不能把邏輯總量稱為實際釋放量。前後磁碟值與刪除清單保存在 `measurements/storage_cleanup_20260926/`。之後建置、系統活動會使剩餘空間再變動。

保留已安裝的 `x64-windows` headers／libraries／tools、vcpkg status／info、Release 二進位、所有舊驗收 raw／PNG／hash 索引與凍結版本。沒有移動或刪除 legacy／使用者文件。建置暫存可以由鎖定依賴重建；首次重建可能較慢。

README 的安裝命令及 CMake presets 已加入 `--clean-buildtrees-after-build`、`--clean-packages-after-build`，讓未來經這兩個入口安裝依賴後自動清掉建置與打包暫存；已安裝依賴與下載快取仍保留。直接繞過这些入口的外部安裝命令不受此設定控制。

## 新畫面生命週期

- `run` 原本就不逐幀存圖；像素只在有界 Frame pool 與當前 reader 中保留。消費完釋放引用，buffer 可重用，不累積成歷史影像佇列。
- 三個 capture bench／campaign 命令預設不再寫 `diagnostic.png`，連 READY 圖也不落盤。這避免先寫檔再刪除的 I/O 與中斷殘留。
- `--keep-diagnostic-image` 才保留每次 run 一張診斷 PNG；不自動保存每張 frame。用於人工校準或重現時明確啟用。
- READY 圖完成處理後立即 `reset()` reader lease，不占用整個量測窗口的一個 pool slot。
- manifest／summary 記 `diagnostic_image_retention`；不存圖的 PNG hash 是 null。分析區分「明確不存圖」和「原本應保留卻遺失」，不能把遺失證據默認通過。
- 不自動刪除歷史驗收圖；不清除逐幀數值日誌。數值日誌、summary 與版本是可重算研究的證據，不是畫面暫存。

本次改動需以新二進位驗證儲存行為；歷史擷取性能繼續引用原凍結二進位，不把新的預設診斷政策冒充舊測法。

## 驗證結果

- Release 建置成功，31 項測試全部通過。
- 兩次各一秒的真實 Emulator smoke test：預設不產生 PNG、hash 為 null；明確 keep 時產生一張 PNG 並核對 hash。驗證用 PNG 核對後已刪除，另留刪除紀錄。
- 四種離線分析相容性案例符合預期：舊版保留圖、新版不存圖通過；政策不一致、應保留卻缺圖均拒絕。重播用的暫存副本已清除。
- 歷史驗收 810 個證據檔案逐一核對 hash，全部完整。
- 清理後重新執行 CMake configure 成功，既有依賴無須重建，新的清理選項已套用。

測試紀錄位於 `measurements/storage_cleanup_20260926/`。gRPC 傳輸延遲的後續研究另見 [gRPC 延遲研究](GRPC_TRANSPORT_LATENCY_NOTES_20260926.md)。
