# 模型交接：Dlyrotz IN13 已取得 AP

更新：2026-10-24。分支：`feature/chart-driven-multigame`。使用者要求封存、總結並 commit／push，接著更換模型；本次不再啟動歌曲或更改成功參數。

使用者已表示要關閉 emulator；後續不要假設裝置仍在線，也不要自行重開。

## 已確認結果

Phigros **4.0.1／157**、`phigros` AVD、Dlyrotz **IN13** 的 **live011** 實際結算：**1,000,000 分、584 Perfect、0 Good／Bad／Miss、Max Combo 584、100.00% Accuracy**。原始結算圖與另一位代理的獨立複核一致；先前重複漏判的雙 Flick 在 HUD 時間線清楚從 259 接到 261。

- [原始 live011 manifest](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/manifest.json)
- [原始結算圖](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result.png)
- [人工複核 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result-observed-001.json)
- [獨立 HUD 複核](../../local-assets/phigros-4.0.1/m7-hud-ap-review-011-001/README.md)
- [成功 binary／source／測試封存](../../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json)

這些本機證據位於 repo 外的 `../local-assets/`，不隨 Git 發布。原執行器 manifest 的 `AP_verified=false` 與 `game_judgment=unreviewed` 保持原樣；經目視核對的 AP 存在獨立 sidecar，不能回寫原始紀錄。

## 成功設定與執行方法

| 項目 | 已固定設定 |
| --- | --- |
| Binary | Build009，SHA256 `2d080e1234c4ec105087f092b4785409d6befd2e999aa9ff1ad40d63a6435ad7` |
| 譜面 SHA256 | `aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90` |
| Compiled fingerprint | `74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5` |
| 計畫 | 584 contacts、5,669 events、peak 4；保留全曲及 6 個 D250 投影候選 |
| Tap／Drag | 按住 32 ms |
| Flick | 判定前 30 ms 開始、滑動 60 ms、60 reference pixels、末端停留 24 ms；仍追蹤動態判定線 |
| 時間 | 首個原生 Tap 的視覺錨點、固定 arrival-origin／rate 1、phase −80 ms；遊戲 Offset 0 ms（使用者確認） |
| 幾何／輸入 | 1280×720；gRPC 觸控映射至 720×1280、rotation 90；4 接點 |
| 起訖 | 互動 manual-mode；人工選曲／開始，一輪結束後停止，不自動重試 |
| 限制 | anchor 100 ms、UI watchdog 500 ms、dispatch lateness 20 ms、round 180 s、session 240 s |

從 repo 根目錄執行：

```powershell
# 純離線核對本機封存、DLL、譜面、畫面模板與 HUD reference。
& .\tools\run-dlyrotz-ap.ps1 -CheckOnly

# 先讓遊戲停在 Dlyrotz IN13 選曲／結算畫面，等 READY 後人工開始。
& .\tools\run-dlyrotz-ap.ps1
```

腳本固定成功 binary，輸出到新的時間戳目錄，保留先前各輪。預設 kernel profile 綁定目前 Android boot `07be8914-d124-44e8-bb8b-904b8fe4541d`；emulator 重啟後須重新核對 boot／evdev route／helper，再以 `-KernelProfile` 明確傳入新的 profile。不要只改 boot 字串跳過核對。CLI 會再次驗 AVD、遊戲版本、前景與接點零狀態。

## 開發成果

- **M0／M1**：盤點 AVD 的 4.0.1 分包與 Unity assets，提取並驗證精確原生譜面／音源；嚴格 Phigros v3 parser、note／line 來源身份及時間轉換。
- **M2／M3**：判定線 pose／座標 evaluator、離線疊圖、Tap／Hold／Drag／Flick contact compiler、路徑誤差界線、容量／干擾檢查、離屏投影候選與完整 fingerprint。
- **M4 非音訊部分**：screen-state classifier、manual lifecycle、foreground watchdog、視覺時間候選、首音 trigger、限定條件的 HUD glyph 補充。音訊程式骨架與失敗紀錄保留，依使用者要求暫緩。
- **M5**：rolling planner／PAS bridge、FakeTouch、完整原生譜面回放。實驗 runtime 對已接納接點保留完整未執行路徑，修正 past-UP revision race 與 32-step window 中斷；保留 evidence／stop／release 機制。
- **M6**：重建自訂 patched gRPC、Android touch evidence fixture、四指／release／STOP、host／Android clock interval 與 kernel slot 核對。原 PAS 保留來源不以新實驗資格覆蓋。
- **M7**：有界全曲 CLI、Windows 高精度 owner wait、完整 receipts／plan／source 對照、50 ms 預配置 score／combo 灰階時間線、首個實際 AP。

非音訊整合 **25/25 CTest**、default offline **4/4 CTest** 通過；私譜 60 ms Flick 候選使用 live009／010 實測 RPC 耗時的四次完整 FakeTouch 回放通過。live011 的 5,669 receipts 逐一核對 source／phase／pixel／deadline，最後 release known、kernel zero。

交接前另跑 Python **84 個離線測例**：初次 83 通過、1 項因缺少 lz4 跳過，隨後使用 task-local asset-python 補跑 bundle guard **8/8**，包含該壓縮 metadata 測例，全部通過。啟動腳本 `-CheckOnly` 已通過；此模式不連裝置、不輸入。

## 保留的界線與後續工作

AP 是指定版本、裝置及設定的一輪實證，尚未證明跨重啟或長期穩定。音訊同步依使用者指示暫緩；精確 source pixel age、遊戲 input lead／jitter 及正式 SyncLease 尚未取得資格，不能把 RPC 回傳或 AP 當作這些測量。M2／M3 的遊戲判定範圍及 6 個投影仍是候選，保留其 metadata。

M6 的 STOP／release fixture 已驗，但 Ctrl+C 的裝置停止路徑尚未實測；不得把它寫成所有中斷方式已驗收。整體 goal 的完整原始 M0–M7 資格仍有上述缺項，這次完成的是指定曲目的非音訊實驗流程與實際 AP。

下一個模型應先讀此文件、[實作計畫](CHART_DRIVEN_IMPLEMENTATION_PLAN.md)、[最新驗證](VALIDATION.md)、[M7 全曲紀錄](M7_WHOLE_SONG_EXPERIMENT.md)。保留成功設定為基線；如要驗穩定性，使用獨立編號／有界整首 run。世界計畫 importer／adapter 與跨曲目擴充留待後續，未因 Dlyrotz AP 宣稱完成。

使用者目前可操作電腦：選曲／開始採互動式，先等程式 READY 再請使用者按開始；不要自行恢復無人操作模式。暫不繼續音訊線、不推翻已有效的原生譜面主導架構，也不把原來失敗的 live001–010 刪除或改成成功。
