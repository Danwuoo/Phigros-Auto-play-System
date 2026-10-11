# 冷 producer CPU eligibility 修補

2026-10-12，本輪仍完成實機啟動前所有必要冷開發／測試，到需要 emulator 的邊界停止。
使用者暫離，不依賴新增互動點選；不建立新 thread、不操作裝置、不追加 ETW 錄製。

同一原 r03 ETL 的 main68／`1941cf4` reader-only 一 TU／40 controls／3 Native8 通過；
离線讀取 Native0／runner0，完整 EOF、781 buffers、1,264,141 callbacks、file reported-loss 全0。
最大相鄰 producer QPC 空窗 36.553ms，target TID27284／CPU14 的 Ready 區間 36.5492ms，
兩端 unknown 共3.8µs。當窗保留652個 CSwitch v5（上限4096），CPU14三個 switch edges：
27284(priority10)→20256(16)→18404(13)→27284(10)。
18404 的相鄰匹配 scheduler residence 是36.5474ms，20256為1.8µs。
沒有 PROC／lifetime 證據，不能命名所屬程序或認定 stall cause／DPC／ISR。
原 controller final statistics UNKNOWN、原 STOP／normal cost=false 保留。
完整小收據見 [main68保存](MAIN68_SCHEDULER_CPU_TIMELINE_20261012.json)。

候選只改冷 producer eligibility：以有界 Windows topology 的最高原始 EfficiencyClass，
每個 physical core 選其最低 logical processor，排除原 consumer 整個 physical core（含 SMT siblings）。
原 last-two 選定的 consumer LP保持，原 producer LP只稱 anchor；不得聲稱 thread 實際固定在 anchor。
若原 consumer／anchor 不在 process mask、拓樸未知、mask/readback/restore 不一致，明確拒絕；
不退回較低 class 或另一 SMT sibling。Writer在 affinity scope 前建立。
AA A/B/normal採同一 producer policy，normal/manual consumer仍reserved、未施加新affinity。

process NORMAL(32)、producer／AA consumer HIGHEST(2)、own-process high execution-speed QoS、
full_period_active_qpc、absolute epoch、2560／20ms／warm256／measure2304維持。
不改正式遊戲決策、manual session、writer、數值成本、coverage、noise18、ABBA36、容量或R2門檻。

建置需明列 base55正式 libraries／source、main32保留資格、新 cold harness delta、
reader delta，以及必要 actual factory verification-only delta。不能改寫原base55.files、
session_build_provenance或宣稱少量重編為fresh32／52／三配置75。新factory binary需實際relink、
compiled verifier identity與Native/link proofs，原formal manual SHA仍核對。
新增eligibility正反控制另列，原65／27／82等分母及历史PASS保存。

`candidate-preflight` 可核對外部 SHA 綁定的 collection-input；成功核對所有冷資料後，
明列 `cold_collection_inputs_valid=true`，仍以 `CURRENT_DEVICE_COLLECTION_REQUIRED`、
`pass=false`／exit2 停在裝置邊界，不啟動 discovery、capture、ADB 或 touch。
原 admission-input 的完整嚴格檢查保持。後續 device adapter 的 explicit delta branch
另核當前 clean execution source 與保留正式 source，不把新 executable 塞回原 build.files。

實際cost尚待新Normal3、AA12／noise18、原未跑ABBA36、完整readonly。
latest completed noise仍main44 6／18 adequate、ABBA0／36；cost NOT_READY、live UNQUALIFIED。
這個修補是由Ready區間支持的通用候選；成功效果必須由新的完整工作負載結果確認。

main69／`62cc0c8` 的 configure Native0／runner0／5.938s 通過，pre-build graph核對在
三個無 imported formal library 的控制 target STOP；尚無 compile、controls 或量測。
原 metadata／source copies／configure Native8 見 [原STOP](MAIN69_ELIGIBILITY_PREBUILD_STOP_20261012.json)。
PowerShell function boundary 把空 pipeline 轉為 null，原 `@($required).Count` 在 function
內誤算為1。新 recipe先正規化成真正空 array，五個有 imported roles 的 target仍保持
exact path／role／unique／complete 檢查；同樣十個 fresh compile occurrences另以新來源建置。
