# M6：唯讀 evdev slot 路由觀察

2026-10-23，本次四指 fixture 輸入可與 `/dev/input/event2` 的 type-B slot
狀態對應。這是該 boot／裝置／fixture session 的路由觀察，尚未提升為遊戲
release observer，也不證明遊戲 callback、判定時序或 AP。稽核只讀保存檔案，
沒有再次操作裝置。

## 原件與獨立核對

原始 [evdev JSONL](../../local-assets/phigros-4.0.1/m6-evdev-fixture-002/evdev.jsonl)
及 [外層報告](../../local-assets/phigros-4.0.1/m6-evdev-fixture-002/report.json)，
配合 [capacity004 主機紀錄](../../local-assets/phigros-4.0.1/m6-live-capacity-004/events.jsonl)、
[fixture 原件取回報告](../../local-assets/phigros-4.0.1/m6-capacity-evidence-004/report.json)
和 [原 analyzer 結果](../../local-assets/phigros-4.0.1/m6-live-capacity-004/analysis.json)。
獨立稽核保存在 repo 外的
[m6-evdev-fixture-audit-002/report.json](../../local-assets/phigros-4.0.1/m6-evdev-fixture-audit-002/report.json)，
含可重跑的唯讀 Python 稽核副本及原 analyzer 重跑結果。重跑與原結果逐欄相同。
稽核 report SHA-256：`e0c18c0c5bbd5500cb55319c75df8b8cb8c6305a362350d2c83852a35e9a1bb0`。

- Boot UUID `543848d2-d515-4ec5-942c-1edf8944d780`；fixture PID 32158。
- 11 個明確指定的 type-B 裝置，每個 486 次查詢，共 5,346 samples；序號、
  round／device 順序、boot、metadata、tracking ID 與時間順序均一致。
  Header／footer 完整、ioctl error 0、footer fault false、20 秒 duration limit。
- 僅 `event2 / virtio_input_multi_touch_1`（rdev 3394、11 slots）非零。
  其 19 個非零樣本分別為 1／2／3 點各 2 次、4 點 13 次；其餘 10 個裝置
  的 486 次樣本均為零。這些零是各次查詢的瞬間狀態。
- slot 0、1、2、3 先後出現 tracking ID 0、1、2、3，觀測狀態為
  `0→1→2→3→4→0`。Android fixture device_id 為 4，9 個 MotionEvents
  的 active-after 為 `1,2,3,4,4,3,2,1,0`。四次分開的 DOWN 及座標／順序
  支持本次對應；ID 數字相等不構成通用映射規則。
- Native 記錄為 4 DOWN、1 個四指 snapshot MOVE、3 POINTER_UP＋1 UP；
  history_size 全為 0、無 lifecycle error／drop，footer 因 app_pause 關閉且
  final active 0。靜止指 1–3 相對自身 DOWN 的最大漂移為 0。
  主機有 9 個成功 RPC，但 RPC 自身仍標示 effect unverified；效果由 native
  及 fixture visual path 另行核對，不由成功回傳推定。

## 時間與輪詢界線

以下是 Android monotonic 域的查詢前後界限；event-time 的比較依 fixture
記載的 NDK 時鐘契約與相同 boot 關聯，沒有擬合 Windows QPC 的偏移或 rate。

| event2 量測 | 中位數 | p95 | 最大值 |
| --- | ---: | ---: | ---: |
| 單次 revalidation＋ioctl bracket | 12.263 µs | 19.460 µs | 418.222 µs |
| 相鄰查詢間未觀測 gap | 40.992 ms | 42.154 ms | 69.530 ms |
| 同一 round 的 11 裝置查詢跨度 | 0.206 ms | 0.863 ms | 3.253 ms |

每個 DOWN 後、下一個 native event 前，皆有相符 count 的完整 evdev
bracket；第 4 次 DOWN 後 8 次、MOVE 後 5 次四指樣本。四次 UP 的 event-time
只跨 1.494 ms，落在輪詢間隔內，evdev 未取得下降時的 3／2／1 點樣本。
最後非零 seq1057 bracket 為 `[10428661971970,10428661982085]` ns；
首個零 seq1068 為 `[10428702352696,10428702363625]` ns。該首零早於最後
callback arrival `10428716848095` ns；callback 後的 seq1079
`[10428744202829,10428744213980]` ns 也為零，之後尚有 387 個零樣本。
首個 DOWN 前另有 78 次零查詢。

這些樣本支持觀察到先有接點、後有零狀態，不能證明兩次查詢間始終為零。
11 個裝置是順序查詢，不能組成同一瞬間的全裝置原子 snapshot。evdev
tracking slot 也沒有 app callback 或畫面／遊戲判定資訊。

Raw ADB stdout 保存為 5,348 個 CRLF lines，SHA 依原 bytes 計算。Helper
footer 的 `bytes_before_footer=2193959` 計的是裝置端 LF；主機 raw pre-footer
為 2,199,306 bytes，減去該段 5,347 個 CR 恰好相符，沒有為對齊而改原件。

## 工具與未成功案例

Helper 目前只在 `work/android_touch_slots_stream.cpp`／
`work/android-touch-slots-stream`，未升為 repo 工具或 runtime 依賴。
NDK 30、x86_64 Android API29、C++20、`-Wall -Wextra -Werror` 編譯通過；
MSVC `/W4 /WX` 的 13 個參數／escaping 檢查、CTest 1/1 通過，僅是 offline
argument／format 測試，未模擬 Android ioctl／hotplug。ELF SHA-256
`8974022af633ff37d5af048279393cc78cad2f48f438271e9596a6d617eb1f79`，
source SHA-256 `1e3f3e32f709d655a073d61537a1415626b02fcbf439e650c127fe6509bbbd5a`。

Helper 只讀明確列出的 fd，不 grab／寫入／注入；16 devices、64 slots、
512 rounds、20 秒 acquisition、16 MiB output 有界。開啟一次後，每次重新驗
path/fd identity、slot bounds 與 boot；不支援或失敗輸出 null，不能當零。
每行立即輸出，stdout backpressure 最多重試 100 ms；kernel syscall stall
與 regular-file disk I/O 仍無硬即時保證，外層 process deadline 也需保留。

[readonly001](../../local-assets/phigros-4.0.1/m6-evdev-readonly-001/report.json)
的三輪 11-device snapshot 均為零；各輪跨度約 146.629–172.618 ms，因逐個
開啟／查 metadata。當時沒有正向輸入，不能推定 gRPC route。
[outer harness001](../../local-assets/phigros-4.0.1/m6-evdev-fixture-001/report.json)
保持 failed/incomplete：內層 capacity003 的
[native path 核對](../../local-assets/phigros-4.0.1/m6-live-capacity-003/analysis.json)
雖通過，外層只等 future 2 秒，未保存 raw evdev。更正等待為 28 秒後，新的
002／capacity004 才取得上述原件；沒有回填或改寫 001。

後續若做 request/reply release observation，仍須每次真的讀取新 slot、
驗證裝置／boot、保留 host request→reply 因果界限與失敗狀態。Fixture route
可作該 adapter 的來源證據，不能直接取得遊戲 callback 或遊戲釋放資格。
