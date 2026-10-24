# M5：有限窗口 PAS bridge 與離線接點回放

日期：2026-10-23。狀態：bridge 契約與真實譜面前 60 秒的 FakeTouch 回放通過；整首、即時音訊、實際輸入尚未驗收。保留 PAS source 未改動。

## 執行契約

PasBridge 持有不可變 Memory，以實際 ExecutionObservation 的 chart/audio/layout identity、ClockMap、SyncLease、sequence 建立執行上下文。規劃 tick 不能更新觀測時間。預設保留 2 contacts、64 plans、16 steps、2 秒 admission horizon、150 ms evidence age、30 ms max lateness；只提前 100 ms 提交新接點。

長 Hold 保留原接點和真實最後 UP。每次 revision 最多包含 16 steps，壓縮已執行前綴時保留最後一個已執行 step，後接有限未來 MOVE 與真實 UP。未收進窗口的下一個 MOVE 形成 coverage deadline；先檢查舊 deadline，再更新窗口，不能以新窗口掩蓋已錯過的軌跡。

已執行前綴及 10 ms commit window 內已提交 deadline 保持不動；微小時鐘修正只影響其餘後綴。core 要求新 intent 的 birth ID 遞增，bridge 依實際 contact 起始時間產生 ID，不借用原始 note 陣列編號。相同時刻仍依 UP、MOVE、DOWN，再依 admission birth ID 排序；這是序列派送，沒有宣稱 backend atomic batch。

到期、identity/epoch/geometry 變更、序號倒退、過期 MOVE、transport failure、coverage 過期、host clock 倒退均撤銷並 release-all。新觀測不能接續已過期的舊租約；需要顯式新 epoch。使用者 stop 是不可自動重置的 latch。release 的 failed/unknown IDs 保留，不能當作成功。

next_wake_ns() 同時考慮派送、下一筆 admission、租約／證據期限與軌跡更新需求；runtime 不能只依 retained scheduler 的下一個事件睡眠。owner thread 執行 begin/update/tick/stop，只有 request_stop 允許 supervisor thread 呼叫。

## 驗證

tests/pas_bridge_tests.cpp 的 14 個具名群組通過，涵蓋長 Hold 多次 prefix compaction、固定 pointer、有限 admission、相位修正、真實觀測期限、identity/epoch/geometry、trajectory coverage、late MOVE、backward clock、backend error/unknown release、sticky stop、容量 2/5/10、同時相位順序與慢 transport 中途失鎖。

另有重要的非整數毫秒 deadline 回歸：真實譜面的 UP 可在 owner tick 前不足 1 ms 到期，保留 core 不接受「最後 UP 已在過去」的 active revision。bridge 此時在原 plan 仍有效且 lateness 合格時直接排出原 UP，不改 deadline、不提前釋放、不延長 evidence。

chart_compile 是離線匯出／回放工具。固定來源 SHA，保留 source pointer、note→contact、編譯器與 geometry identity、內容 cache SHA、診斷和未校正狀態。音源 SHA 在這個 CLI 只是呼叫者提供的 identity；音源 bytes 由 M0 manifest 核對。--segment-ns 明確選取判定到 Hold 結尾都在範圍內的 note，產物列出整體與選取數量；不稱為全譜。Flick lead／release 可能延伸到所選 judgment 範圍外。

本機 Dlyrotz 結果：[前 60 秒 FakeTouch](../../local-assets/phigros-4.0.1/m5-first60s-fake-v3.json)。

| 項目 | 結果 |
| --- | --- |
| 完整來源 | 584 notes |
| 明確選取範圍 | song [0, 60) 秒，281 notes |
| 編譯動作 | 2,114 steps |
| 聲明容量／實際 FakeTouch 峰值 | 2／2 |
| 同時 pending plan 峰值 | 4 |
| 執行 | 281 contacts 完成，2,114 receipts |
| 比對 | deadline、phase、pixel、birth ID、每 contact pointer ownership 全部吻合 |
| 結束 | 無 active contact，release 成功 |
| 時鐘 | synthetic exact mapping；每 40 ms 模擬一筆新觀測，1 ms owner tick |

這些數據不測聲音辨識、Windows 排程 jitter、Android delivery 或遊戲判定。原始 m5-first60s-fake.json 保存 past-UP revision 失敗；-v2 已完整執行，但驗證器誤用 source-ID 同時排序而報失敗；-v3 修正為 core 的 birth-ID tie rule，逐 contact ownership 仍獨立核對。沒有刪除負結果。

整首 strict compile 仍因 6 個含畫面外位置／路徑的 notes 拒絕，見 [M3](M3_CONTACTS.md)。沒有跳過這些 notes 宣稱全首成功。M7 arming 必須取得完整 CompiledContactPlan 診斷及校正證據，不能只抽出沒有 qualification 欄位的舊 Memory。

## 離線重跑

### 全曲候選的新增驗證

後續有界法向交集候選保留全部 584 notes／4,911 steps；容量 4 時，裸 bridge 與 M7 supervisor 整合的 FakeTouch 回放均逐筆通過，pending 峰值 8、active 峰值 4、所有接點完成及釋放，M7 末尾 explicit known-release ACK 成功。容量 2 的同一候選正確拒絕並回報 required_peak_contacts=4；strict policy 仍拒絕那 6 個畫面外 notes。

報告位於 repo 外 `../local-assets/phigros-4.0.1/m7-whole-fake-001/`。全曲候選保留 requires_calibration，法向修正上限為 250 reference pixels；這輪仍使用 synthetic exact-clock 每 40 ms 新觀測，沒有輸入裝置或新的音訊工作。它補上完整計畫的離線執行驗證，不取代實際判定區、四指能力與 input lead 校正。

在下列 build 另開啟 RHYTHM_BUILD_SUPERVISOR 與 RHYTHM_BUILD_SESSION_RUNNER，可使用全曲入口：

```powershell
./build-bridge/Release/chart_compile.exe CHART_JSON CHART_SHA AUDIO_SHA Dlyrotz.Likey.0 IN 4.0.1 reference1280x720 4 --normal-intersection-pixels 250 pending-calibration-id --session-replay
```

提供已安裝的 nlohmann_json prefix；建置不啟動裝置或擷取音訊：

```powershell
cmake -S . -B build-bridge -DRHYTHM_BUILD_CHART_IMPORT=ON -DRHYTHM_BUILD_PAS_BRIDGE=ON -DCMAKE_PREFIX_PATH=C:/path/to/json-install
cmake --build build-bridge --config Release
ctest --test-dir build-bridge -C Release --output-on-failure
./build-bridge/Release/chart_compile.exe CHART_JSON CHART_SHA AUDIO_SHA Dlyrotz.Likey.0 IN 4.0.1 reference1280x720 2 --segment-ns 0 60000000000 --fake-replay
```

chart_compile exit 0 表示離線編譯（以及有要求時的 FakeTouch）成功，2 表示編譯拒絕，3 表示回放核對失敗，1 表示參數／讀取錯誤。輸出一律 live_qualified=false。
