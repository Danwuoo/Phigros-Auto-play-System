# R2F：修復完整收據交易並連續完成可到達的BVI冷驗

2026-10-05 Asia/Taipei。使用者已授權總控驗收後派新task、盡量一次多步；GPT-6.1 Sol／xhigh。前置[R2E獨立驗收](HOLD_OWNERSHIP_X10D_O_BVI_R2E_ACCEPTANCE_20261005.md)只接受工程partial封存。**本頁新增一次局部恢復實作／唯一新attempt授權，絕不續跑R2E STOP、不修改封存或重置R1 repair。** 本task做到所有可到達stage或第一個hard stop，再交總控獨立驗收；不自行派下一task。

## 根、來源與保護

唯一attempt `bvi-r2f-20261005-01`；新source `research/x10d_o_bvi_r2f/`，新batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/`，新out `out/x10d-o-bvi-r2f/`。唯一新增外部文件為docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESULT_20261005.md與HANDOFF同前綴；新工具在新source/batch內。不得替換字串後誤指舊根，精確RootBinding及stage allowlist先落地。

先讀AGENTS四入口、當前Legacy總帳／逐曲JSON、原4I/R1/R2D契約、R2E dispatch/result/handoff及本頁。以R2E controller-review/check-r2e.ps1的只讀重建步驟為參考，自行重建2145舊保護集合，再核R2E ledger全部entry、final receipt、freeze/contract inputs/dependencies及本次controller ledger；不得執行舊script回寫。總控integrity的2214是含當時依賴等檔的去重數；新receipt加controller資料後分母另報，不硬湊2214。HEAD/index/既有dirty保持，現在Git分母以controller receipt為準。原R2E來源、結果、attempt STOP、收據、失敗、PNG、out均不可寫。

## A：先修完整I/O交易，不用真native試錯

保留R1 BVI候選／門檻／oracle及全部fixture；允許新runner的必要維護shell／owned-job interop與最小harness adapter。候選／分析仍C++20。先靜態核整條執行鏈與每個讀寫API，再在新batch封閉scratch測真檔案交易。修復範圍含command/result/log/checkpoint/report/identity/verification的writer ownership、flush、SHA、JSON readback、receipt引用及durable state；不得只修首次Get-FileHash後留下下一個reader sharing錯誤。

選擇有明確ownership的既有handle hash或相容reader／完成寫入後關閉再hash等實作；記錄為何不容許旁路writer、如何保證最終資料穩定、如何保留log pump完成與輸出reservation。不得為綠燈全域改成可任意寫入或讀過期hash。原15s quiescence、PID/creation/image/membership、單owned job、bounded cleanup、QPC與nullable共享Result均不放寬。

將正式run使用的storage/state transaction抽成共用路徑，以fake native facts注入；scratch入口無真CreateProcess權限，不能授予正式launch。保留原P9/S7/E5相關拒絕檢查，再測完整成功交易：預開writer→command→RUNNING→fake launched/exit/active0/streamdone→result/report→verification SHA/readback→READY→下一stage CheckState；writer持有與釋放後均獨立核bytes/SHA，包含零長log、非零log、reserved report與identity使用的分享模式。使用真正reader API及真檔案，不能只有helper boolean通過。測missing/pending、closed writer、SHA突變、native已退出後保存故障、state寫入失敗後RUNNING/STOP拒絕及跨shell讀STOP；注入必須走正式共用交易／consumer。預先凍結oracle，所有重要失敗保留並分類。

預正式開發可修作者錯誤與跑有界scratch，最多兩輪完整交易預驗，每輪最多40個declared cases，每命令≤60s；原helper組最多再跑一次。第一次預驗失敗可在這個明列額度內修一次，不新增第三輪或換根。第二輪仍失敗／容量保護或舊根改動則停止，不進正式controls。零native mock不可冒充真程序身份／cleanup驗收。正式來源freeze前一次列全source、inputs、stage argv、預期exit、budget及可用配置；成功預驗所用共用交易與freeze版本SHA對應。

## B→C：成功後自行連續推進，不停在單一控制

正式controls依序natural0、nonzero7、owned-child負控制各一次。所有來源／容量／收據readback核驗成立才下一stage；每項總≤30s含cleanup/drain、各stream≤64KiB、15s quiescence不變。負控制須證明完整15s末owned child身份仍可辨、runner預期125且cleanup active0、streams完成；任何無關異常或unknown不算負例pass。完成前native結果不能寫成verification pass。

三控制通過才一次Release configure→build（parallel≤2）→wrong-contact-only真native1且共用failed-row/aggregate/consumer拒絕→完整cold suite。保留原89cases×4層356、22 supplemental、4原controls、1128 coverage rows／2392引用，以及新增contact adapter檢查；分層assertions/未驗欄位/sizeof/metadata/probes照實報。配置、編譯或必要測試任何非預期失敗立即STOP並分類，不修改oracle、候選門檻或以全unknown追pass。

Release成功即接read-only事先確認可用的Debug與ASan，每配置一次configure/build/full suite。事先確認不支援可標未驗；啟動後失敗不得倒填skip。產品命令各≤300s、stream≤512KiB、並行≤2，最多11條（Release四條＋Debug三條＋ASan三條，剩一條不代表可重試；精確allowlist只列實際所需）。正式control第一launch起不可再修runner／source重試，第一非預期失敗durable STOP，全後續native禁止；本attempt不重啟。

## D：有限上游來源設計與交付

本包不執行R2E provenance-gap PNG driver；52圖審核已用0/2保持。利用已存在原packet schema、既有C++ observer介面及已凍結geometry-sources做有限只讀來源對照（不重放、不開圖作新內容審核），列出每個ROI/front/depth/width/angle與全部line所需資料：是否當前pixels證據、是否含full-prefix追蹤、可否在6份/90ms空history segment下取得、具體程式接口與未實作項。不能從4R手述parts/oracle/physical label或舊actions填值，也不寫新辨識平台。可在任何STOP後完成這份純文件交接，不執行candidate。缺幾何不阻塞先完成冷測。

交source/binary/input與實際依賴／DLL closure及缺口；Add-Type不是BVI build，存在不是loaded closure。保存每stage三exit、所有failed/unknown及未跑數、STOP/COMPLETE事實、來源和容量帳、候選仍experimental；成功全部冷驗只能判cold contract，不宣稱真圖/owner/成本/live通過。附下一包合法幾何與owner接入需要的具體接口/驗收差距；不接正式owner。

## 容量與禁區

開發carry7294690B／56MiB；controller carry與aggregate依本次controller-review/controller-receipt.json（含本派送頁與驗收頁）讀取，不沿用未加review數字。R2F新增development≤40MiB41943040B，含全部source/docs/batch/mock/failures；newout≤128MiB134217728B，舊out374705＋new仍≤256MiB。aggregate＝最新carry＋全部新development＋newout，≤8GiB；保留原controller8MiB剩餘；每phase free≥5704253440B。每維護報告≤64KiB、全部maintenance logs≤2MiB；source/freeze、logs和各suite報告預留且拒絕超額，不刪失敗／移到帳外。

不改formal src/include/root CMake/apps/tests或舊source，不install/重裝/OS registry服務修改，不重開A/vctip、P/suppression、B插樁、runtime/cost/stress、full replay、模型、emulator/ADB/觸控/manual-session、goal/automation、commit/push。保留C36h tint1、main50 donor/live0、suppression OFF、P excluded、X12 not-ready與Chapter Legacy全曲IN Miss=0目標。完成或hard stop後交總控，不自行續派。
