# 結算確認與影像保存政策

2026-10-25 依使用者要求，在下一輪實測前修正結算辨識，並停止一般回合的影像落盤。此變更不回寫歷史回合，也不自動複核分數／AP。

## 結算標籤 v2

Cipher、FULi、Winter 的舊原始日誌顯示五個結算 caption 均有充分筆畫支持，但 `Miss` 在較亮背景下的絕對 128 灰階切割將抗鋸齒邊緣算成額外筆畫，超過 v1 的 ink-ratio 上限。候選只有短暫成立，未達原本三個不同實際觀測／至少 60 ms 的確認條件，原生最後仍失敗或 watchdog 結束。

新增明確的 `shared-result-local-contrast-v2` dataset policy。每個固定 caption 以 20th／99th 灰階百分位估計背景與前景，兩者至少差 40，再以其中點切割 reference 與當下影格；保留五個標籤、共同小幅位移、0.98 支持、0.05 額外筆畫、0.7–1.4 筆畫比例及原有對比條件。沒有以當下畫面建立新模板。

只有全曲接點完成、來源有效、原分類為 unknown 且沒有已匹配或歧義狀態時，才可使用此 terminal-only fallback。原 playing／paused／result 判斷優先；manual lifecycle、100 ms anchor、500 ms UI、20 ms lateness、180 秒 round watchdog、停止及 release-all 未改。

舊 `shared-result-labels-v1` 與其絕對閾值預設保持相容；新的本機 dataset 才明確選用 v2。新資料集引用原 RGB bytes，不複製／改寫校正原件。

離線驗證：8 張既有結算圖全部接受，92 張既有遊玩圖全部拒絕，**100/100**；包含 v1 未接受的上述三張歷史結算。原圖只讀，檢查時使用單一暫存 RGB buffer，未另外保存一批圖。另有合成亮度變化、缺標籤、平坦／棋盤／低對比、越界位移及原 terminal gate 負例。這些是 frozen-pixel 結果，**不是新的實機回合或已證明跨輪改善**。

## 新 binary 預設不存圖片

一般 `run`／`run-adaptive` 預設 `--image-retention none`：

- 不保存 initial、anchor、last、result、judgment、acquisition、unknown 或 learned-reference PNG。
- 不配置或保存 HUD 灰階像素時間線；原 67,410,720 bytes 的像素預配置降為 0，48 個開場診斷 RGB buffers 也不配置。
- 保留實際 receipts、輸入計畫、停止／釋放／kernel 證據、事件與 HUD receipt metadata。
- 結算已確認時，在 manifest 保存實際影格序號、receipt 時間、RGB SHA256 與 policy；`image_saved=false`。這些欄位不是分數／判定 OCR，`AP_verified` 仍為 false，等待獨立複核。

明確需要圖片時，可在 native 命令最後加：

```text
--image-retention result
--image-retention diagnostic
```

`result` 只保存確認結算的一張 `result.png`，不保存 HUD 灰階或其他圖；`diagnostic` 才明確開啟既有有界影像診斷。PowerShell wrapper 對應 `-ImageRetention result`／`diagnostic`；不指定時使用新 binary 的 none。舊封存 binary 保持原樣，不把新版預設套稱為舊版行為。

metadata-only HUD 使用獨立 schema `pas.experimental-hud-timeline-metadata.v1`，沒有 pixel filename／SHA／offset。Flick 分析器仍可提供真實 receipt-time 鄰近索引，明示 `hud_pixels_not_retained_by_policy`，不假稱已核對像素或遊戲逐 note 判定。面板顯示「依設定未保存圖片」，不把刻意不存圖誤報為圖片遺失。

舊私人譜面、音源、APK、校正、AP 圖片與每輪原始封存均保留，沒有為節省空間刪除原件。初次完成此變更時，遊戲實測依使用者要求暫停。

## 其後恢復的三輪實機確認

Glaciaxion IN的72→60→72 px三輪均在原template未匹配、來源有效的情況下由terminal-only v2 fallback確認結算；候選分別5／3／4個不同實際receipt，跨越71.436／69.730／64.454 ms，最後kernel zero。三輪實際圖片均0張，HUD pixels bytes均0；數值判定由使用者文字回報，沒有OCR或新AP。這是Glaciaxion的三輪確認，Cipher／FULi／Winter仍只有既有像素回放，未補稱新實機成功。三輪raw文字記錄共133.65 MiB，記錄仍占容量。詳見[完整比較](FLICK_BAB_2026-10-25.md)。

本機建置與封存見 [接續紀錄](LOCAL_NATIVE_FLICK_2026-10-25.md)。
