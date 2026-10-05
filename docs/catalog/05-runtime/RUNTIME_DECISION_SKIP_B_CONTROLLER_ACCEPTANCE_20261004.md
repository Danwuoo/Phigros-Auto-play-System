# 工作包3：B契約總控驗收與X10d-O派送

2026-10-04 Asia/Taipei。**B設計審查驗收通過；保留為待實作提案，本輪不採行B插樁或八輪量測。** 依既定順序派送工作包4「X10d-O current-body ownership冷開發」，GPT-6.1 Sol／xhigh。A checker技術驗收仍未完成，X12仍not-ready，不因本頁改變。

B交付chat：`01a1067b-21fb-77f1-b15e-97bee70817af`「審查選項 B 最小量測契約」。[契約](RUNTIME_DECISION_SKIP_B_CONTRACT_REVIEW_20261004.md)、[交接](RUNTIME_DECISION_SKIP_B_HANDOFF_20261004.md)、[開發收據](../../../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/final-receipt.json)保留原封存狀態。

## 總控獨立核對

[機讀結果](../../../measurements/runtime-decision-skip-a/controller-review-b-controller-20261004/integrity.json)與[維護核對命令](../../../measurements/runtime-decision-skip-a/controller-review-b-controller-20261004/check-b.ps1)：重新hash／核bytes **741個不同檔案，0不符**。包含717份原保護檔、34份選定inputs、8份交付artifact，組別重疊不相加；8個小raw例列逐一對來源檔指定行的完整JSON及SHA相符。沒有重新解析五run／193列，也沒有build、guard、probe、classifier或fixture test執行。

HEAD仍`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，正式src/include/root CMake及index無diff，非預期Git路徑0。開發新資料 **232210B／1MiB**；交付時A總review **2430911B／16MiB**、batch含外部文件 **37185028B／64MiB**逐項重算一致。總控新增頁／維護結果另計入本次receipt，不改開發final-capacity。

另獨立讀R3 frozen main的process→recognition_end→lease reset→mutex publication，以及action mutex selection→JSON／兩enqueue→owner_start→accept→owner_end。核RGB observer自增decision sequence、保留source frame的源碼；這支持B區分兩類publication、S與owner_start、decision與source frame，不以欄名代替實際來源。

## 設計判定

- 同mutex內P／S各一對前後clock reads給操作界限，沒有冒稱clock sample等於線性化瞬間。使用原owner_start/end夾accept入口保留不確定性；const snapshot與metadata綁定、所有publication側錄及診斷單向要求合理。
- 全attempt、consumer skip、未selected、selected-but-fault、terminal與trace失效分開；原90%成本gate不變。短residency≤4ms及「嚴格多於50%」是預聲明的新描述命題，不能回填為原A分類。
- 總控手算full的9–11ns、partial的19–21ns、entry-unknown的8–10ns、long的69–71ns及membership的35–45ns均一致。同一P／replacement在A=50／60可為partial／full，足以展示不可識別性。同timestamp按physical量化保留Unknown合理。
- 比例界限`[K/(K+F+U),(K+U)/(K+F+U)]`是保守外界：將全部U納入分母並分別視為非full／full。已確認long仍列全skip帳；空判定域為null。1/3/0給25%、1/1/2給25–75%、2/2/0恰50%否證「嚴格多於」均成立。這是**手核設計**，不是已執行tests。
- 新P/S固定slots記憶體655360B，加控制≤256KiB仍低於1MiB；5120行×1024B＋metadata≤1MiB可納8MiB新trace/run。預算算術可核，不代表sizeof、RSS或observer effect已量。
- B能區分服務區間，不能把覆蓋說成JSON、lock、wake或OS的原因，也不能由此刪診斷或推稱Miss改善。四格D/T及至多8run僅未来screening提案，未獲本輪執行授權；它不代替noise／B0/B1成本資格。

未來真正實作前仍需將7個invalid標籤展開成具體輸入與独立oracle，完成時計／編譯順序／共享資料／容量與ON/OFF驗證；目前它們是契約草案。B「值得另案最小實作」的理由可成立，但**優先順序維持X10d-O**：產品的current-body歸屬缺口可獨立冷研究，無須先擴量測平台。沒有阻止本設計審查簽收的問題。

## 工作包4執行決策

沿[總帳第四包](../01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md#第四包x10d-o產品開發)、[X9](../04-offline-research/HOLD_CAUSAL_X9_20261003.md)、[X10c](../04-offline-research/HOLD_CASCADE_X10C_RESULT_20261003.md)與[X10d-P已驗界線](../04-offline-research/PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)，從**未加suppression與pending hook的frozen C36h**建立新隔離export，先grounding／反例／契約，至多一個預聲明通用ownership候選。main50只作donor背景，不整包移植；不混P、alias/grace調寬或復活X10b suppression family。

必須分清原held body、独立incoming front、內部fragment及head/body/tail；同column分離Hold、舊tail過線／新front接近、rotation／late alignment／neighbor／absence皆有負例。proposed不升gold，局部藍色連續不證physical ownership。當前支持不足輸出unknown；已執行／未知Down／completed不重播，同contact的Move仍需當前支持。不能靠少Down或H/K局部變好簽收。

允許本包必要C++20隔離實作、Release／Debug-ASan合成與fake-clock、原基準回歸及有界離線行為比較。先凍結新protocol、source／binary／DLL／manifest及全action分母後，**完整recording replay最多4次，失敗亦計次**；有可重現候選反例即停止該family，不湊滿次數、不調門檻。原recorded touch只作外部比較、不餵策略；固定pixels不證遊戲feedback。若只到可信負結果，也可交付，不能為完成硬採候選。

總控已實查campaign8196523814B＋prior45307809B＝**8241831623B**，距8GiB剩348102969B。O新batch含source/docs/失敗/log/report與總控預留上限**256MiB**（開發224MiB＋總控32MiB），新export/build上限**1GiB**；使用前再次核容量及free≥全部預留＋5GiB，不得以新root重置campaign或aggregate舊帳。256MiB保留後舊8GiB帳仍餘79667513B；不複製原PNG、不刪舊資料。新根建議campaign/hold-ownership-x10d-o與out/x10d-o，查時均未存在。

本包不改main正式src/include/root CMake及既有dirty/frozen；新工具與隔離export可新增，完整sources須可保存，不能只留下ignored臨時產物。舊A／B建置guard支線不重開，O所需一般冷建置／測試另凍結有界契約，不能把A checker成功變前置或借此修改原A失敗紀錄。若自身必要工具無法可信執行，保留已完成證據及具體原因，不擴無限平台。

**禁止**新runtime/cost/stress/R4/AA/ABBA、B插樁、emulator/ADB/真觸控、模型訓練、goal/automation、commit/push或自行續派。本包完成交總控獨立驗收；O冷通過也不自動進live。產品目標仍為所有Chapter Legacy曲目解鎖IN與完整IN Miss=0，目前未達。

