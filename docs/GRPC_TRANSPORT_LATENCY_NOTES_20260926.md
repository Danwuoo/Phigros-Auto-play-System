# gRPC 本身延遲：下一步研究方向

本方向已完成研究及[正式接入](GRPC_TRANSPORT_INTEGRATION_20260926.md)，現行擷取 channel 預設為 256 KiB，擷取器終態见[最後驗收](CAPTURE_FINAL_ACCEPTANCE_20260926.md)。以下保留為實驗前假設；「未變更傳輸參數」等敘述僅指當時儲存清理階段，不代表目前程式，亦不是待自動執行的研究清單。

使用者澄清希望降低 gRPC 路徑取得畫面的延遲，不是只降低收到 payload 之後的 RGB 複製成本。本文件是重新界定量測與候選實驗；本次儲存清理開發没有變更傳輸參數，也沒有取得新的低延遲性能結論。

## 目前數據尚不能回答的問題

目前 `capture_complete_ns` 在同步 `Read(Image*)` 返回後取得，包含完整訊息接收及 typed protobuf decode；`receive_wait_ms` 同時包含「等 Emulator 下一張圖」與「資料傳輸／排隊」，不能當成純 gRPC transport latency。主機駐留 p99 4.61 ms 則發生在收到畫面之後，也不是 gRPC 傳輸延遲。

應區分：Android 畫面產生 → Emulator readback／像素轉換 → server 寫入／等待流控 → HTTP/2 與 localhost 接收 → protobuf 解析 → 應用發布。源端 Unix timestamp 尚未校準，不能與主機 QPC 相減估造延遲。

本機 proto 說明 `streamScreenshot` 隨新 frame／sensor 更新送圖；因此 40–59 Hz 對應的等待下一幀時間，不會因單純增加 gRPC threads 而消失。Emulator 與主機 client 的版本也不同，不能僅凭 client 1.81.1 推定 server 同版本／相同預設。

## 優先研究

1. **先量傳輸段。** 用可重現 C++ loopback server、同一主機 QPC、2,764,800-byte RGB payload、40／48／59 Hz paced producer，記錄 message ready、Write 呼叫／完成、Read 完成及 sequence。message-ready→Read 完成量包含排隊／serialization／傳輸／decode；仍需分段追蹤才可細分。Write 返回只表示 gRPC 接受或完成相應 API 操作，不保證對端已收到。loopback 結果不冒充 Emulator 端到端結果。
2. **讓接收持續取走資料。** 現在同一讀取迴圈還執行 normalize／發布／bench JSON 建構，下一次 Read 要等這些完成。若 trace 顯示此處使對端等待或緩衝堆積，再比較持續接收 owner＋容量一的最新完整訊息交接。物理訊息 slots 也須有上限；不是把上游 FIFO 搬進自己程序。這能改善 transport 回壓／舊圖滯留，與只減少收到後複製不同。
3. **依證據檢查 HTTP/2 流控與讀取緩衝。** 每張約 2.76 MB，需排查分段、WINDOW_UPDATE 等待與 burst。本機 headers 有 `grpc.http2.lookahead_bytes`、`grpc.http2.bdp_probe`，但讀取預取上限不等於單一萬用 flow-control window。先核對鎖定版本實作，再以單變數短 A/B 評估；更大的緩衝可能提高吞吐卻加重舊圖堆積。
4. **核對 server 取圖／send batching。** 若本機同大小 loopback 很快，而 Emulator 仍慢，查匹配 Emulator 版本的畫面讀回、顏色轉換、縮放及 server 發送排程。client 設定不能保證改變 server 的 buffering。`grpc.http2.write_buffer_size` 是寫入側相關選項，不能把 client 調值直接解釋成控制 screenshot server 發送。必要 server fork／重建仍另列範圍決策。

已經使用長連線 streaming 及重用 stub／channel；這些既有做法不能再包裝成新優化。Keepalive 主要幫助閒置連線，不會提高持續活躍串流每一幀的產生速度。改 async／增加 threads、壓縮、降解析度、修改預設流控，都需有對應瓶頸和資料，不預設一定較低延遲。

實驗先控制在短、小矩陣，不重新啟動五候選長測。應以 transport 分段 p50／p95／p99、最新圖相對落後、CPU 及掉圖共同判斷；只有吞吐變高不算成功。若需要量真正畫面產生→主機到達，另設可校準的 pixels-only 測試或來源版本匹配的診斷時間證據，且真值僅供離線研究。

## 一手參考

- [gRPC Flow Control](https://grpc.io/docs/guides/flow-control/)：stream 的寫入、緩衝與接收流控。
- [gRPC Performance Best Practices](https://grpc.io/docs/guides/performance/)：channel／stub 重用及長期串流。
- [gRPC C++ Performance Notes](https://grpc.github.io/grpc/cpp/md_doc_cpp_perf_notes.html)：寫入 batching 與 C++ 性能注意事項；實作仍須核對本機鎖定版本。
- 本機 `out/vcpkg_installed/x64-windows/include/grpc/impl/channel_arg_names.h`、`proto/emulator_controller.proto` 與 `src/emulator.cpp`。
