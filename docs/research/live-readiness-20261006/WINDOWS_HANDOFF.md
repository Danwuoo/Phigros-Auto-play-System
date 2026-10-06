# 下一階段 Windows 接手 prompt（待使用者自行移交；不含實機授權）

請在使用者指定的 Windows C++ 環境接手這份 Phigros 雲端候選。先讀本目錄 RESULTS／
READINESS、SHA 清單及 PLAN，再讀 AGENTS、現行 README 和原 10/6 PRELIVE 結果／成本規則。
本 prompt 只是可移交的工作內容；本雲端 task 沒有建立本機 task、連裝置或啟動遊戲。

## 起點與保護

- base 是完整 `e9e305e3aba13f05a786c74540419428610b2f76`；交付最新 commit／tree 見包內
  HANDOFF_MANIFEST.json。只套 incremental patch 或匯入 Git bundle 擇一，不重套舊 v3 patch。
- 獨立新 worktree／checkout，不覆寫使用者的 main、AUX 修改、舊 attempt、source、oracle、
  原 36 fail、STOP 或 ignored 原始資料。不自行 push／PR／merge。
- 原 ignored ZIP SHA
  `e59766f09879b927b26cd0947ce501cea05f16fd838cfb1cce9321a63a0d7530`，1,001,668,802 bytes。
  compact 缺舊 libraries／compiler snapshot 等，不是完整獨立 toolchain。核 manifest 與原有
  依賴，不能用殘留 exe 代表新 source。保留的 AUX 歷史 source path 不適合直接普通解壓至 Windows。
- baseline metadata、3063 PNG、全部 retained payload 已重新核 SHA；舊 71 STOP 原始檔不在
  compact，只有其 sealed identities。不要聲稱那些 bytes 已由本包重驗。

## 此候選與現況

1. 正式決策 JSON 新增 typed current_contact 並升 schema4，防止與 reason-only 證據混淆。
   最終共用 OwnerCandidateStep 已在所有 caller 的 Hook 前 poll/sync；100ms舊lease先
   到期再處理delayed fresh packet，不可續命復活。固定expiry反例已驗兩路active0／Down1。
2. current hook 僅對自身已 ACTIVE、當前 rails/body 支持的無可見 tail Hold 使用有限實測 depth；
   不新增 Down、不創造 tail、不改 frozen v3/front。未知／completed identity 不復活。
3. Runtime finalization 保存第一次 unknown/failed release 與事件，阻止重試／覆寫，verified
   round 結束才允許後續新 round。缺失／未知 release 保持 fail-closed。
4. 成本 auditor v2 直接重算 raw quantile／分母、prefix／contact／phase，並驗測量段是否實際
   有動作；analyzer 把 campaign、source、binary、profile、raw 與直接 native Job 身分綁定。
   舊報告可做 forensic 核對，不能重新封裝為 v2 資格。
5. production manual-session 的 C++ candidate API 已接共用單一owner、immutable同幀
   lease/batch及唯一backend的bounded receipt stream；API硬UNQUALIFIED，任何I/O前拒絕。
   原donor CLI未改，沒有新enable flag。真正live admission validator仍未實作，需根據
   實際Windows資格收據另驗；AA ready不能解鎖。
6. 所有 Linux 合成／FakeClock／decoder 回歸仍不是 Windows 成本或 Phigros 採納。
   共享 measurement workload 與 tail 結果以最終 RESULTS 為準，不借舊 generator 的空動作過關。

## 必要順序：先冷工程，實機另授權

A. 先核全包 SHA、精確來源／dirty／依賴與原件未變。Windows 上現有 compiler／SDK／vcpkg
   符合時，以包內SOURCE_MIGRATION.json＋canonical old/new blobs核原freeze及LF/CRLF
   精確bytes，再fresh Release／Debug／MSVC ASan roots重建affected source。不能借不符 ABI、
   模式、flags 或 source 的 formal_core.lib。ASan 不包含第三方全面插樁／UBSan／LSan 宣稱。
   research CMake是endpoint-free chain，**不編譯整個Win32 production manual-session**；
   另做新root production build，兩種build成功不能互代。

B. 跑原 prefix113、front56、own-prefix113、current68、受影響正式 Game／Tracking186（含
   原60 clips），以及本輪新 active-body、Runtime release、serializer、cost/auditor counterexamples。
   再跑既有independent packet63／runtime97（原91＋6expiry）與closure35；
   按 RESULTS 的 exact final source 再核全部退出。保留原 timeout、skip、oracle36fail。
   架構、source 不變的舊套件只引用已核收據，不要求無關重跑。

C. 新 source 的 3063 因果前綴從 recording ordinal1 開始，streaming 一張 current RGB，保持
   自己的 ledger／owner／receipts，不 seed 歷史 Down，不截成功窗。所有 processed／unknown／
   accepted／Down／Move／Up／release／fail／raw event 分母照報；完整 audit 不等於物理 gold。

D. measurement design 若尚不足，先依精確缺口做有界、可否證的冷證明；不得直接跑 A/A
   消耗測量或悄悄改門檻。固定 v3 A 的 body 能力與新 B 不等價時，分列 shared entry/release、
   B-only sustained-body/tail 等實際路徑，不冒稱等效 full-Hold 比較。新設計必須在成本前凍結。

E. 方法成立後才建立全新 campaign，原 AA第5筆 STOP、Tap noise inadequate 和 ABBA未跑
   永久保留。禁止重開 research/prelive_batch.ps1 的舊 campaign。
   v2 schema／direct-native invocation 見 research/prelive_current/COST_ANALYSIS_V2.md。
   原 process safety core 不改；fresh qualified gate→direct candidate Job root→complete state／
   freeze／contract／command／verification／result 鏈，以及bounded input_closure的
   before/after＋core/root/leaf證據。原64KiB native freeze cap不改；core COMPLETE卻缺
   closure PASS仍拒絕。cmd.exe root不能代替candidate身分。
   新 campaign 必須完整 bind source（含 compiler dependency closure）、build／flags、依賴／
   profile、binary、generator、host/background、每個事前 planned run 及全部結果。

F. 同機 Release A/A 至多12筆，256 warmup＋2304 measurement，舊 coverage／lateness／RSS／
   noise 門檻全部保留。必要 gate 失敗即停止新批，未跑明列；不能補安靜重測、刪診斷、
   挑成功 subset 或借 Debug/ASan 速度取得資格。僅新 A/A 全通過，才凍結T並再評估有限36筆ABBA。
   新 OUT／metadata 上限、保留餘量及至少20GiB free 先獨立記帳；不靠刪歷史失敗換空間。

G. 即使冷成本通過，production額外perception→packet→action hop／Win32 wake／pool
   contention與其full journal也需資格；endpoint-free producer不替代此鏈。
   目前C++入口故意UNQUALIFIED，須實作綁真收據的validator且独立驗收才能啟用，
   不能改成caller bool或把AA ready當live許可。live CLI、真 capture/RPC、目前 serial／app/profile/capability、五指mapping、
   physical Hold 接入／body／tail 與1519 identity 仍各有 gate。先向使用者列具體裝置、命令、
   目的、風險、有限額度和停止／釋放條件；取得相應授權後才進裝置階段。這份 prompt 不准
   安装/啟動 emulator、Fixture、遊戲或發 touch。舊 donor pas.exe 不等於 current 候選。

### 為何 admission 啟用仍是未完成的程式工作

目前缺的不是可填一個true的開關，而是新候選完整Win32 build／flags／依賴與binary closure、
該binary的fresh ToolSnapshot／GateReceipt及owned-process輸出、實際perception→packet→
action鏈的成本／全attempt journal，以及當前device/app/profile/capability與mapping指紋。
原endpoint-free cost receipt只證冷比較，沒有資格聲明以上production鏈；歷史Windows
receipts也不對應本輪來源。尚未有可接受的真實qualification範例，因此不預先編造可啟用
格式或信任規則。最小下一步是先完成A–F與production-chain qualification，保留一份
完整成功樣本及必要STOP/不匹配樣本，再實作明確schema與完整source/build/profile/process/
device綁定的reader；重做冷負例與早期拒絕spy，維持live CLI關閉直到独立驗收與授權。
這段validator／CLI工作明列未完成，不歸為單純硬體gate。

產品 Goal 仍是當前完整 Chapter Legacy roster／各曲 IN 已解鎖，以及同一凍結候選逐曲完整
IN 結算 Miss=0；P/G/B／score 照報，不新增 AP 或每曲三連勝門檻。不得以此交接或全綠冷測
宣告目標完成。

## 精確命令與交付導入

完整可執行的offline qualification/build/tests/family/prefix/audit/analysis命令、必要
原件路徑、SourceMigration格式與closure schema：
[WINDOWS_COMMANDS](../../../research/prelive_current/windows_recipe_evidence/WINDOWS_COMMANDS.md)。
包內亦有獨立副本。這些commands只完成source/static檢查，沒有在雲端Linux冒充執行PowerShell。

先核包內HANDOFF_MANIFEST/SHA256SUMS與完整base/target/tree。優先匯入incremental.bundle
到獨立review ref，再以一次性git -c core.autocrlf=false worktree add --detach建立全新
目錄，保留canonical LF及sealed evidence bytes；不改全域Git設定。若改用另提供的patch，
先在乾淨exact base用git apply --check／--index，不與bundle重复套用。

SOURCE_MIGRATION.json與migration-blobs必須放一起，保持原始bytes，取manifest列出的SHA
作SourceMigrationSha256，取target_commit作CandidateCommit；不可手改JSON成新路徑。
old_freezes.path是新repo-relative固定位置，blob path是migration檔案相對路徑；
previous.path保留歷史Windows身分。依賴／toolchain缺件時停止並列精確缺項，不造receipt。

當前source的Linux最終lane共1339 checks/config＋Flick，latest3063 report SHA
1c41c6a2842284d6c1bc5e28ad92fa2fb41dab55059003aaaffcd3db830d4683。
實際tail機會分母unknown，typed-tail與normal-tail Up均0；不能據此宣稱沒有錯過尾端或已改善Miss。
