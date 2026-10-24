# Screenshot 的 source timestamp 與主機時鐘：來源核對及量測設計

**目前能確認的是 server 讀鐘點，尚不能確認像素內容的生成時間。** 官方實作在 `getScreenshot` 讀像素之前寫入主機 Unix 時間；`streamScreenshot` 也呼叫同一條路徑。即使 QPC↔UTC 映射非常精密，仍不能據此宣稱影像 capture age、遊戲狀態 age 或相位誤差低於 20 ms。

本輪只讀官方程式碼與本機 SDK 檔案，沒有呼叫裝置 RPC、啟動 emulator、擷取新畫面、輸入或音訊處理。production timestamp 來源未修改。實際量測由另外明確授權的 probe 執行；本文件沒有虛構量測結果。

## 本機身份與官方來源界線

本機 `emulator/source.properties` 記載：`Pkg.Revision=37.1.11`、`Pkg.BuildId=15917651`。該檔 SHA256 為 `ee589ca350515c29dc3bad30dd5ebe8efb11d1a50c17c8815125d3c4bd12a3a8`。隨 SDK 提供的 `lib/emulator_controller.proto` SHA256 為 `1d62c6bcad5f06621f90ec2bf26c661ba769ccd0f1416b5314d25a68e04eee5f`，其 `Image.timestampUs` 第 1460–1464 行說明 Unix µs、估計 frame generated、在複製／轉換之前。欄位單位不是誤差保證。

官方來源固定如下，避免未來 master 改變後無法重現：

| Repository | 本輪固定 revision | 用途 |
| --- | --- | --- |
| `platform/external/qemu` | `ae9d18d2b6261179fbd57fffec720a04f7bfb053` | RPC、System 與 ScreenshotUtils |
| `platform/hardware/google/aemu` | `3c1ced8a369417db591eb7cd083af5bb2c317975` | Windows POSIX time compatibility |
| `platform/hardware/google/gfxstream` | `d047a57228332d995d36600792fa9ccc26cf8ae6` | renderer／color-buffer readback |
| `microsoft/STL` | `b6f01e438b68ee497f9c08313e49d12bb2f0d8e0` | MSVC system_clock 的 Windows API |

尚未取得將本機 **37.1.11／15917651 prebuilt** 精確連到以上四個 revisions 的完整 build manifest；它們是已取得的官方 source snapshots，不是已證明的 binary reproduction。開發 snapshot 的完整 proto 與本機 proto 也不相同，僅 timestamp 欄位說明相符。因此下述為已查源碼的語義，對本機 binary 的一致性仍須 probe 驗證。不能僅以版本號或同名函式取代這個界線。

原始 source 副本、逐 repository revision／URL／SHA、本機 launcher／qemu／gfxstream／proto 指紋與 artifact hashes 保存在 repo 外 `../local-assets/phigros-4.0.1/m4-capture-clock-source-001/`。獨立唯讀覆核確認 server-stamp 的程式順序與缺失的 pixel-age 界；default 離線 CTest 4/4 仍通過。這些檢查沒有執行 capture-clock 實測。

## timestamp 實際在哪裡產生

[QEMU `EmulatorService.cpp`](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/android-grpc/services/emulator-controller/server/src/android/emulation/control/EmulatorService.cpp#908) 的第 908 行設定 `Image.timestampUs`，呼叫 `System::get()->getUnixTimeUs()`。之後才取得旋轉、尺寸、buffer 等資訊。PNG 在第 1001 行讀圖；RGB/RGBA 在第 1011 行先查 buffer 大小，第 1075 行才做實際像素 readback。timestamp 沒有從該 color buffer 的 producer timestamp 傳入，也沒有扣除量測到的 frame age。

[同檔 `streamScreenshot`](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/android-grpc/services/emulator-controller/server/src/android/emulation/control/EmulatorService.cpp#733) 第 733–810 行：

1. 等待 framebuffer event；125 ms 是檢查 client 的等待設定，不是 capture latency 上界。
2. sensor 改變或第一次回覆，也可觸發送圖，沒有要求一定新 render。
3. 第 784 行呼叫上述 `getScreenshot`，第 800 行以累計 events 設定 seq，再 `Write`。

因此 stream 的 seq 是 event／stream 進度資訊，可能跳號；sensor-only 更新也可能重用 seq。它不是與 returned pixels 原子綁定的 Android frame ID。unary `getScreenshot` 的 seq 按 proto 為 0，更不能拿 timestamp 或每次 RPC 次數宣稱新 framebuffer。RPC 重連／sequence rollback 應建立新的 capture generation。

## Windows 時鐘來源

[QEMU `HostSystem::getUnixTimeUs`](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/android-emu-base/android/base/system/System.cpp#1855) 第 1855–1858 行透過 `gettimeofday` 取得 Unix 秒及 µs；`System::get` 在非 testing override 時選 host system。它不讀 Android uptime、guest wall clock 或 guest frame clock。

[QEMU Windows compatibility CMake](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/msvc-posix-compat/CMakeLists.txt) 指到 aemu 的 Windows 層。[aemu `windows/src/time.cpp`](https://android.googlesource.com/platform/hardware/google/aemu/+/3c1ced8a369417db591eb7cd083af5bb2c317975/windows/src/time.cpp#116) 第 116–127 行的 `gettimeofday` 使用 `std::chrono::system_clock::now()`，截取整數 µs。[MSVC `chrono`](https://github.com/microsoft/STL/blob/b6f01e438b68ee497f9c08313e49d12bb2f0d8e0/stl/inc/chrono#L81) 及 [`xtime.cpp`](https://github.com/microsoft/STL/blob/b6f01e438b68ee497f9c08313e49d12bb2f0d8e0/stl/src/xtime.cpp#L48) 的對應實作使用 `GetSystemTimePreciseAsFileTime`。

所以這條 Windows source 路徑讀的是 **emulator 主機的 wall clock**。µs 欄位有截斷誤差；wall clock 可校時、跳變，並非 monotonic。MSVC source API 的精度也不代表整個 screenshot pipeline 有相同精度。因本機 binary／依賴 revision 尚未完全核對，不把上述 MSVC 實作當作已驗證的 prebuilt ABI 證據。

Microsoft 明確區分 [QPC 的 interval clock 與 UTC time-of-day clock](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)，並建議 UTC stamp 使用 [GetSystemTimePreciseAsFileTime](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemtimepreciseasfiletime)。同機兩個 process 可以比較 QPC，但不可直接將 Unix µs 當 QPC ns；wall-clock 校正也不應重設 scheduler 的 monotonic 時間。

## 像素與 stamp 的關聯仍缺哪一段

[`ScreenshotUtils.cpp`](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/android-grpc/services/emulator-controller/server/src/android/emulation/control/utils/ScreenshotUtils.cpp#154) 對 RGB/RGBA 轉交 renderer；其他情況走 `takeScreenshot`。沒有回傳 producer frame time。

[gfxstream `RendererImpl::getScreenshot`](https://android.googlesource.com/platform/hardware/google/gfxstream/+/d047a57228332d995d36600792fa9ccc26cf8ae6/host/RendererImpl.cpp#425) 轉交 `FrameBuffer`。[`FrameBuffer.cpp`](https://android.googlesource.com/platform/hardware/google/gfxstream/+/d047a57228332d995d36600792fa9ccc26cf8ae6/host/FrameBuffer.cpp#2018) 第 2047–2051 行對 display 0 選 `m_lastPostedColorBuffer`；第 2137–2140 行排 screenshot worker 並等待完成；[`PostWorkerGl`](https://android.googlesource.com/platform/hardware/google/gfxstream/+/d047a57228332d995d36600792fa9ccc26cf8ae6/host/PostWorkerGl.cpp#61) 再讀出 buffer bytes。這些步驟沒有把該 frame 的生成／present timestamp 填回 RPC。

由此可得的限制：

- 若 guest 沒更新畫面，最後 posted 的內容可以早於 stamp 很久；新的 metadata stamp 不會使舊像素變新。
- 在 stamp 到 buffer selection 之間，也可能有更新；不能一律假定畫面一定早於 stamp，或把 stamp 當單向上界。
- event 喚醒到 server 讀鐘的排程延遲、readback／縮放／PNG、RPC buffering／傳輸／client decode 都沒有這份 API 的 hard latency bound。
- `client_receive - mapped_server_stamp` 衡量的是 **stamp→接收** 路徑；不是 **frame generation→接收** 的 age。RTT 也不能取半後充當 capture time。

因此 source stamp 映射成功，仍只能標記 `server_wall_stamp_mapped` 這類獨立 metadata。現有 `mapped_capture` 的使用需要真正的像素生成／觀測時間資格；在此資格未取得前，production 仍須保留 `arrival_only` 或明確 unknown，不能用新的欄位名稱偷偷縮小 uncertainty。

## QPC↔UTC 可實測的有界映射

可先量測 **讀鐘點的映射**，它本身有價值，也不需要碰遊戲 UI。每筆 anchor 在同一 thread 依序讀：

```text
q_before = QueryPerformanceCounter()
u        = GetSystemTimePreciseAsFileTime()
q_after  = QueryPerformanceCounter()
```

保留 raw QPC ticks、frequency、FILETIME 100 ns 與 `[q_before,q_after]`，不要只保存 midpoint。FILETIME 轉 Unix 時須扣除 `116444736000000000` 個 100 ns；計算用整數／有向取整，避免巨大 epoch 的浮點消去或乘法 overflow。若 scheduler 的 host ns 有不同 epoch，還須另量測／核對到該 clock，不能只因同樣叫 ns 就直接拼接。

在一段已驗證無 wall-clock step、具有明確 rate/error allowance 的局部區間內，令 `q` 先換成同單位時間，`u=q+b`，UTC API 與源頭實作的合計誤差為 `epsilon`。一筆夾取給出：

```text
b ∈ [u - q_after - epsilon, u - q_before + epsilon]
source Unix µs s 對應的 server 讀鐘 QPC interval：
q_stamp ∈ [1000*s - b_max - epsilon_source,
           1000*(s+1) - b_min + epsilon_source]
```

最後一式包含 µs 截斷；實作應避免重複計入同一 epsilon。這是條件式 interval，不是把 midpoint 擬合殘差當作保證。`epsilon_source`、局部 rate allowance 與相鄰 anchors 間的 wall-clock 行為目前尚未量測，不能直接填 0。共同使用同機 wall clock 時不需要宣稱對外部真 UTC 的絕對準確度，但仍需核對兩 API／兩 process 的一致性。

取樣前後都收 anchors，以 source stamp 所在區段作局部映射；記錄 bracket width 分布與最壞值。發現 QPC rollback、wall-clock step、映射集合互斥或過期 anchor，立即作廢該 mapping generation 及相關時間證據，不以平均或強行 fit 掩蓋。

前後 anchors 一致只是有限觀察，不能邏輯上排除取樣之間的短暫 wall-clock 調整。尚無可信 `epsilon_source` 或局部 rate／step 界時，只能輸出帶假設的 clock-mapping candidate，不能標為已證明的有限 capture uncertainty。

## 建議的第一輪只讀 probe

1. 記錄 SDK revision/build ID、launcher／實際 qemu binary／gfxstream DLL／proto SHA、process／capture generation、實際要求的 display/format/geometry；不保存 token。
2. probe 前後及期間收上述 QPC–UTC 夾取；同時保存 API 名稱、QPC frequency、scheduler host-clock 身份，所有原值可重算。
3. 有界 unary 截圖：保存 call-begin/end QPC、server timestampUs、seq、format／尺寸、bytes、received/decode-complete QPC、內容 SHA。將 mapped **server stamp interval** 與完整 RPC bracket 作一致性檢查；不將 RPC bracket 稱為 pixel generation interval。
4. 若另外測 stream，保留 server seq、client receipt sequence、timestamp、每筆 arrival QPC 及 queue/drop counters；相同 seq、跳號、重連及未消耗的舊訊息分開報告。不能把每次 reader callback 當新 render。
5. 報告 mapping width、stamp→arrival 的 interval 分布、timestamp regression、source/API 不一致與完整最壞值；明確保留 `pixel_source_age=unknown`、`capture_clock_qualified=false`。readback 格式或尺寸改變屬新的量測條件，不能承接較快格式的數字。

這輪即使得到很小的 clock-mapping interval，也只能完成讀鐘映射資格的其中一部分，不能直接將 supervisor 的 20 ms 工程預算填成已通過。

## 最小尚缺的證據

要把結果提升成捕獲／視覺 song clock，至少還缺：

- 本機 prebuilt 的 source／runtime 一致性，或足夠明確且可保留的實測 clock contract。
- **與返回 pixels 原子綁定**的 producer frame identity 及 host-monotonic post/compose/capture timestamp，或獨立驗證、附誤差界的 framebuffer age 量測。僅 event counter 不足。
- 若借助 Android fixture 內的 monotonic marker，還必須量測 guest clock↔host QPC interval，以及 marker 取時到實際 frame 內容的語義；frame callback 也不等於顯示器 present。不能假設 guest/host clock offset 為零或單向延遲等於 RTT/2。
- 對本遊戲 render-state／chart-state 相位及輸入延遲各自的證據；capture clock 完成也不會自動解決這兩項。

可在未來固定版本 server／fixture 上增加上述來源觀測點；本輪沒有做此改動，也沒有承諾 API 現有欄位已提供它們。現階段最小可靠產物是 **可重算的 server-wall-stamp→QPC interval＋仍未知的 pixel age**。
