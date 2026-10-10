# 2026-10-11 噪聲修補與縮小重驗範圍

使用者原文：「修復完不用全盤重跑，跑噪聲檢查就好，趕快完成」；其後補充
「但是你還是要完成到實機 ready 前的所有測試與開發」。已通過的三配置75組、
完整source/build圖3、3063 prefix、fault4、production9不全盤重跑。
修補僅限必要component建置／控制、新AA12／原R2噪聲檢查，再完成原未跑ABBA36；
成功至需要emulator的current profile／capture／RPC／physical階段停下。

使用者另指示：「然後我要離開一下，所以妳完成後先不要開新的 thread」。
本task不得建立新thread或啟動emulator。既有Chapter Legacy IN一輪只記分數授權保留，
不是本輪冷測操作；沒有新goal、automation、訓練、push或worktree授權。

原source `aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de`（main32）三配置各25組、
完整圖3、prefix3063、fault4及production9 integrity通過；normal Tap／Hold／Dense
原numerical gate通過，其餘stress／instrumented normal cost false保留。AA12 validity
全通過，但18 noise欄中observer及capture-to-owner的p95／p99共12欄不adequate，
owner六欄adequate。noise producer/native/runner/verification均0，原數值NOT_READY，
ABBA36 NOT_RUN。這不是producer deadline failure或程序身分失敗。

原913份source snapshot、原raw與STOP見
`out/prelive-pre-emulator-planning/main32-stage-inventory.json`；37個native stages完整
296小ProcessEvidence見`main32-stage-native-completeness.json`。原件不回寫。
Debug／ASan舊compiler outputs逐entry ZIP讀回SHA後保存，Release保持原bytes。
還原舊output path才能重新讀原完整build圖；不把較晚文件HEAD冒稱原binary來源。

18個inactive歷史／main32失敗噪聲journal逐entry解壓SHA與原件再次核對後，
4539082365 bytes保存於125609008-byte ZIP：
`out/retained/consolidation-20261008/inactive-noise-raw-archive33/original-journals.zip`，
同目錄`proof.json`保存原path／bytes／SHA／entry索引。只刪除核對後的未壓縮副本；
原payload可恢復，原報告、負數值及STOP不變。原main32完整raw讀取須先恢復原path。
inactive main05與main32 Debug／ASan compiler ZIP及各自verified proof亦保留；
Release static libraries和binary仍在原path，新修補不依賴上述封存ZIP。

有界B dense1000診斷（cost qualification false）測得741個measurement owner frames，
observer wall總4874.582ms／thread CPU總4265.625ms，hook wall總3695.217ms／CPU總
3078.125ms。CPU採樣量化未知、counter與QPC界線不同；不能把差值當精確off-CPU，
不能認定特定背景程序／core migration／thermal根因。原診斷與首次summary prelaunch
capacity STOP均保留；回收inactive main05 outputs後只讀原診斷，未重跑producer。

新的cold consumer調度候選維持NORMAL process，consumer scoped ABOVE_NORMAL，
query／set／readback／同thread explicit restore逐項驗證；A／B同policy。
producer原ABOVE_NORMAL、writer、frame cadence、QPC epoch、workload與原R2全部數值
界限不改。這是待量測候選，不宣稱已證實noise根因或實機成本改善。
正式`src/`／`include/`與production `pas.exe`不改。新chain只重新編譯必要TU，
沿用逐檔SHA核對的main32 Release static libraries；收據明列base/delta identity、
原25組只是原來源prerequisite，不能把重用說成新三配置fresh建置或whole qualification。

必要噪聲重驗須有新source／binary／campaign／environment及原完整AA12，不能挑成功
重測、重排配對或改原noise公式／coverage／deadline／RSS門檻。失敗停止該批並照報。
通過後原未跑ABBA36仍須完成；noise或ABBA通過都不發live admission。
保留retained48GiB／free20GiB及原stage、candidate、raw、metadata caps。

main33／`c3a51ca` configure native／runner／verification皆0。後續graph檢查原以
POSIX分隔符及`/MD`辨識，Windows Ninja實際使用反斜線及等價MSVC `-MD`，因而冷STOP；
沒有compile／controls／AA量測。保留原configure與STOP，修正路徑正規化及相同runtime
flag辨識後用新attempt，不放寬兩TU、Release flags或原測量契約。

main34／`626d32d`兩個target、controls36及全部native／closure通過。AA loader的
script binding欄位與caller同名PowerShell參數碰撞，將path變成hashtable，AA campaign
封存前冷STOP；沒有producer／噪聲量測。改為獨立`noiseRepairBindingPin`，原必要建置
收據及prelaunch負結果保留；新attempt不重跑75組或production。

main35／`42a450c`必要兩TU及controls36通過。首筆Tap完整2560張、producer／publication
overrun0、consumer readback／restore及完整raw auditor通過；cost analyzer native2，
stderr `hash file bound/type`。原native guard identity／held／streams與active0正常，
原status保留PRODUCED_BUT_UNQUALIFIED／後11筆NOT_RUN，未有noise或ABBA數值。
新loader誤把全部已核歷史依賴加入每次測量的actual inputs，六個不供該冷runtime使用的
gRPC Release／Debug library為655596174至2515648002 bytes，超過原256MiB單檔界限。
修正為明列當次runtime／source／static／receipt metadata，完整依賴仍於loader核對，
不提高原hash、freeze或closure上限。純維護修正沿用已通過相同C++ component bytes、
compile source與controls收據，另列新execution source／maintenance amendment；
不重新編相同TU，也不冒稱新source fresh compile或重用舊producer作新AA測量。

main36 執行來源 `505ae43`、compiled source `42a450c`，沿用 main35 chain SHA
`fc7c040c4673899b4a237a5917eb734e5c13c088923843a65d49de0ddf9d5a89`。
全12筆新 AA 的 producer／auditor／gate validity通過，producer／publication overrun0，
consumer priority 12/12 readback與explicit restore通過；noise native／runner／verification0，
80.103942秒自然退出。原公式18欄獨立重算一致：observer與capture-to-owner p95／p99
仍12欄inadequate，owner六欄adequate。noise READY=false、comparison=false，ABBA NOT_RUN；
不把單一priority變更當已解決。全915 codefiles先逐bytes複製／SHA核對保存於
`out/prelive-pre-emulator-planning/source-505ae43-noise-snapshot/snapshot.json`。

main36失敗noise的12份journal共2013879133 bytes，逐entry讀回SHA及原件再次核對後，
保存於56923317-byte ZIP：
`out/retained/consolidation-20261008/inactive-noise-raw-archive36/original-journals.zip`；
同目錄proof保存原path／bytes／SHA。原report、attempts、audit、noise數值與STOP保持；
完整舊raw reader需先還原原path。保留48GiB上限，未刪除payload。

下一個有界診斷只以A／B各一次dense1000補原九段wall／thread CPU／cycles及
GetCurrentProcessorNumberEx begin/end；每process一次64KiB／256 LP上限的
RelationProcessorCore topology保存原group／number／EfficiencyClass。
只fresh編譯chain與summary兩TU，五個原library不持有Profile並逐SHA核對重用。
端點相同不證明中途未遷移，wall−CPU不是精確off-CPU，cycles不換算QPC；
不猜P/E編號，不改affinity／CPU sets。此診斷cost qualification=false，不能代替AA／ABBA。
最新使用者再確認須完成所有啟動實機前可完成的測試／開發；仍不開新thread或emulator。
