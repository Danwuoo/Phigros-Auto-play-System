# X11-P 總控獨立驗收

2026-10-03。**簽收完整 runtime 重建、冷契約與負成本結果；維持 not-ready，不進 X12。** 不代表性能 gate 通過或 pending-only 候選正式採用。原[開發結果](RUNTIME_X11_P_RESULT_20261003.md)／[protocol](RUNTIME_X11_P_PROTOCOL_20261003.md)／[交接](RUNTIME_X11_P_HANDOFF_20261003.md)保留交付時狀態，最新判定以本頁及 status J24 為準。

## 實際獨立驗證

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一 registered worktree；既有 dirty 保留。正式 src/include/root CMake 對 HEAD 無差異，main metadata仍50/27/11。重讀三份X11文件、export/build/binding/dependency/finalization scripts、完整 harness/gate、pending assertion diff、C36h manual-session Wake/owner/archive呼叫及source inverse。沒有 rebuild frozen outputs、emulator/ADB/live/模型、commit/push。

新證據位於 `measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-controller`；[final-summary](../../../measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-controller/final-summary.json)、verification及final-artifact-ledger保存命令／exit／XML／bridge／SHA。

| 獨立操作 | 結果 |
|---|---|
| 重hash／bytes | 274 source/binary binding、1490 compiled dependency、1923 artifact ledger條目全部符合；原 tint1 live SHA亦符合 |
| 重新inverse hook | 55個src/include檔還原B0；唯一策略差仍8行pending cursor==0 missing hook，suppression OFF |
| 兩版runtime `x11-provenance` | 離線exit0，embedded source SHA逐項符合export |
| 重跑B0原suite | 238/238 pass，此次無historical path skip |
| 重跑B1原suite | 237pass／1預期fail，仍僅 `GameOwner.SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt` |
| 重跑新pending contracts | B0 8pass／5預期red；B1 13/13。失敗名稱符合封存，四suite全部skip/error/disabled0 |
| 重跑兩版bridge | 各512 synthetic/FakeClock inputs，public-events bytes與原freeze相同；B0/B1 receipts177/127、contacts0 |
| 重算成本gate | 複製28份summary至新根，noise/evaluate各exit2，兩份JSON bytes與原freeze完全相同 |

沒有重跑24 cost／4 stress／ASan／完整7722 PNG replay。hash核對和ASan log檢視不稱為重編譯或ASan重跑。synthetic/public-output bridge不能取代完整實景非同步replay；原X10d-P的14 lost opportunity仍只屬原研究binary。

## 判定與量測缺口

**Verified：** A/A RGB recognition p99及dense owner p99噪聲adequacy failed；ABBA owner p99兩批增加`.46655/.45685 ms`，超凍結`T=.298 ms`；baseline `ab-owner-1-4-B0` lateness p99=`15.0906 ms`超15ms。其餘22個比較通過不能抵銷失敗。原noise檔建立時間早於首B1 cost命令，與腳本順序一致；本機檔案時間不是外部不可竄改時間證明。

**Unknown：** A/A已不穩，不能將差異定性為正式runtime已確認性能退步。harness使用1ms condition-variable polling、compact writer、owner options `{15,0,30ms}`；X12 profile lead35，正式manual-session用high-resolution Wake及SessionArchive完整事件，兩者成本不等價。每run1183次hook cancel及額外診斷工作可作成本歸因假說，尚無分解消融證明；不因此先改owner契約或壓掉診斷。

**來源範圍：** 原live `61ec…`、重建B0 `81cb…`、B1 `0b93…`是不同binary。26 listed snapshots支持C36h lineage，新完整compiled-input freeze支持新B0/B1來源。原live未保存的compiled SHA不可回補成fact。後續可明確以「可重建B0」作新配對基準並保留歷史未知；不以尋找不存在的證明無限阻塞，也不把新B0 tests推論成原live全來源一致。

額外確認三個collector契約缺口，下一工程包處理：

- `finalize-runtime-x11-p.ps1`只讀GTest root `skipped`，漏掉原B0首輪testcase skip；開發prose已披露及補跑。總控本次逐testcase確認真正0 skip，後續collector須同樣解析。
- gate `bounded()`把`n==0`當通過。須區分預先允許無動作workload與應有样本卻缺資料，不能把空lateness當時序驗收成功。
- fake `release_all`固定成功／清map，未保存逐call denominator。zero contacts不等於真RPC釋放驗收；下一fake meter須保存call/result及失敗注入。

## 下一工程包：X11-P-R1（規劃，尚未派送）

目的：補正式路徑離線量測契約，**不先改pending規則，不啟動emulator**。C36h仍behavioral baseline，main50 donor，X10b否決，X10d-O另案。

1. **量測正確性。** 新隔離export/output，使用可核source的正式Wake／SessionArchive共用路徑搭synthetic pixels/FakeTouch。明列與manual-session尚不同的capture/session/RPC邊界；兩版採相同owner options，核對live lead35。量actual serialized events、queue/drop/fault、逐release及全phase lateness；missing row、寫檔失敗、空樣本與skip不得靜默通過。不要為方便量測改策略。
2. **先驗契約及容量。** FakeClock覆盖due前/同時/後、wake通知/timeout、gate/reset/收尾；保留pending13項及原suite刻意fail分類。任何共用路徑提取須證明B0/B1策略差仍單hook。補testcase skip解析並用舊skip XML驗證。X12 journal原32×16MiB=512MiB與計畫256MiB的差異，先選可測硬限制或有計算依據的明確容量計畫修訂；不能靠人工盯檔案大小。兩版對稱，不無聲更改live條件。
3. **另立有界性能protocol。** 舊24+4耗盡且不重開。新方法先freeze source/input/options/環境/診斷範圍，最多8 A/A＋16 ABBA、4 stress，所有失敗計額。先8 A/A，noise不足即停，不啟動ABBA；只准一次量測工程修復且保留失敗，不調門檻／無限安靜重跑。沒有實際量測方法改善就不值得重跑原批次；成本分解若必要須預先計入同一預算，不混為gate資料。
4. **交付停止。** 建議新batch≤128MiB、build≤3GiB、campaign+prior仍≤8GiB，先實測reserve；PNG引用、原freeze保留。交付source inverse/provenance/全分母/negative logs待總控驗收。若仍not-ready，交負結果停止，不能自動X12。原live未知不得由新實驗補記「已驗」。

量測契約、冷功能、成本都通過後，才可驗收X12的≤6次閉環預案。觸控採納、14組lost opportunity利弊、77 Miss改善及跨曲效果仍 **Unknown**，最終需要有限實機；重播相同離線資料不能回答。

## 總控容量記錄

初估16MiB不足：兩份bridge public-events合計29,729,626B，tests/bridge完成後容量check失敗。保留所有輸出及原script/ledger，`controller-capacity-failure.json`記錄失敗；未追加任何實驗。只完成保留封存，另列32MiB retention ceiling，不追改16MiB gate為passed。final-summary前batch30,096,176B、campaign+prior8,217,562,555B，另預留16KiB給summary，仍在8GiB總上限。工具32MiB預檢修正供新根重現，不覆寫原script證據。
