# 2026-10-07 Windows readiness 實際結果：NOT_READY

三配置冷回歸、3063 個原始 WIC frames 及冷編譯收據 CLI 已完成。Windows A/A 實際產生三筆 Tap，前兩筆完整通過；第三筆分析程序在 root-exit 無法確認一個 Job 成員的 image identity，必要程序門檻 STOP。**AA12／noise／ABBA 未取得資格，live 入口保持 UNQUALIFIED；本輪零 emulator 啟動、零裝置命令、零觸控。全曲完整 IN Miss=0 未完成。**

完整實際路徑、SHA、各 stage、原 STOP、容量及三筆 raw 分布見 [result-index.json](result-index.json)。已執行 recipe／GUI helper 的逐 byte source 副本見 [recorded-recipe-copies.json](recorded-recipe-copies.json)；實際測試輸入仍保留原 OUT 路徑。這些副本不代表新的執行或新的成本資格。

使用者本輪明確要求接手 source-only ZIP，完成 Windows 實際成本、資格驗證及實機驗收。這是新的工作授權；交接文件內「沒有 device 授權」記錄的是上一輪範圍，不是本輪使用者要求。但新授權不能代替成本、裝置、physical 與解鎖門檻的證據。

原包 SHA256 `515b26d075ddfbf8a3f74a1925c0fb7ae2290afc5b42787c7aaf024e5b621ef1`，候選 commit `ecae2c51246bf8e962a21afad7532f73a01e5001`。主 checkout 未修改；本輪位於 `C:/Users/wurre/Desktop/PAS-windows-readiness-20261007` 的 `codex/windows-readiness-20261007` 分支，未 push。

## 身分及保留

- 原包所有 manifest／SHA256SUMS 已核對。舊 compact ZIP 13,670 個保留 payload 全部逐流核 SHA 和 bytes；不將 ZIP CRC 當 source 身分。
- 舊 Windows R4 checkout 已不存在。只恢復原 freeze 必需、且 SHA 完全匹配的 compiler snapshot／歷史 source／generated protobuf／receipt，未恢復被刪 raw 或執行舊 STOP。原 36 oracle failures 保留。
- fresh file-only 20/20 與 native 四控制通過，原 Job／identity／held／streams／期限／64 KiB freeze core 未修改。
- 第一輪 direct functional root 與早期 CMD envelope 在 child image identity 不可確認時 STOP。改用獨立 C++20 GUI subsystem root 執行 endpoint-free 功能子程序，保留實際 child exit、原 Job／identity／held／streams 檢查與完整 closure。helper 自身 fresh configure／build 及 nonzero 控制通過。GUI root 沒有自己的 console；舊 failure 可能涉及 console-host 生命週期，但失敗 PID 的 image 未能確認，不能將此推測當成已知身分。此 envelope **只供功能與唯讀驗證，不替換 direct cost producer**。

## Windows 發現及修補

1. `ByteVariants` 的 culture-sensitive `StartsWith(BOM)` 把沒有 BOM 的 UTF-8 當 BOM。改為 Ordinal；65 個實際 LF／CRLF／BOM／invalid UTF-8 控制通過，原 119 個 recipe static checks 通過。
2. 原 analyzer unit fixture generator 使用 text-mode `ofstream`，Windows 寫 CRLF；scoped independent review 使用 binary-mode，重建 LF 後與原 auditor SHA 不符。修成固定 binary-mode LF，保留原 failure／fixture 和 attack oracle，另建立新的 source migration 與 fresh 三配置驗證。

初輪 Windows Release／Debug／ASan 全部 fresh build 通過。完整 Win32 root 的 `pas` 與 disabled-live-seam target 亦編譯成功，包含 `manual_session.cpp`；當時 source dirty 已照報，未執行 production binary，不能作成本或 live 資格。

## 唯讀資格 CLI

`research/windows_qualification` 提供獨立 C++20 `pas_qualification_verify`：

```powershell
pas_qualification_verify.exe --build-binding <current-build.json> --expected-sha256 <externally-pinned-SHA> --source-commit <40-hex-commit>
```

CLI 核對現有 cold build／native gate／tool snapshot／migration target／configure-build input closure／COMPLETE receipt 的實際 SHA、links 與 scope。JSON 有容量、深度和重複 key 限制；完整 closure 最多 8192 files／12 GiB，驗證結束再查 bytes。它沒有 device API／process launcher，不呼叫 admission continuation、不產生可用 live token；成功也固定 `live_ready=false`。真實 Windows build 正向及 14 個拒絕控制已通過。

這是 cold 收據驗證器的完成部分。完整 production packet/action/wake/capture/RPC 與 physical 收據尚不能由 cold build 或 AA 代替，`CandidateLiveAdmission` 保持 UNQUALIFIED。不能為了測成本先走實機入口。

## 順序與界限

先完成同源三配置 affected tests、positive-before-attacks controls、family，以及原 3063 WIC prefix 和獨立 raw audit，再 freeze 一次 AA12。AA 使用原 20ms cadence、256 warmup＋2304 measurement、原硬門檻及 noise 規則。第一個必要 validity／process gate 失敗就停止，未跑列 NOT_RUN，不更換 corpus／threshold 或補安靜重測。

所有 stage 保留 raw 與失敗；輸出仍受原 12 GiB／metadata 64 MiB／free 20 GiB 上限。Windows ASan 與 Linux ASan+UBSan 的能力不同，不能聲稱 MSVC 覆蓋 UBSan。

實機驗收要先有指定裝置、當前 app／profile／五指映射與 physical qualification；manual-session 仍由使用者選曲及按 Play。Chapter Legacy 目前分母／逐曲 IN 解鎖 unknown，全曲完整 IN Miss=0 **未完成**，HD 不推定 IN，AP 不作前置。

## 同源冷驗證完成

實際凍結 commit 是 `cd21144440b1d02c11eafb3f332e63d20c0079cb`，local migration SHA `5e696cdf6bbcca1bdc8cff54336763ff93c4a3bb8272c1e76f4328f5d650c5a2`。後續提交只保留 recipe source 及實際結果，不能把較晚 HEAD 冒稱這次 binary 的編譯來源。

|配置|fresh build|正式 affected|成本 unit／拒絕控制|family v1／v2／正常 tail|
|---|---|---|---|---|
|Release|win04，COMPLETE|186／fail0／disabled0|36；152／35／14 全過|104／104／25 全過|
|Debug|win03，COMPLETE|186／fail0／disabled0|36；152／35／14 全過|104／104／25 全過|
|MSVC ASan|win03，COMPLETE|186／fail0／disabled0|36；152／35／14 全過|104／104／25 全過|

每配置 22 個 tests／helpers／controls stage，加 5 個 family stage；另加 WIC prefix 與 raw audit，合計 **83 個 COMPLETE stage**。每個 native／runner／verifier exit=0，identity trusted、held signaled、streams complete、active_final=0，輸入 closure before／after 均通過。MSVC ASan 不冒稱 UBSan。

Debug family-v2 的較早 270s 時限 STOP 保留，fresh 900s 冷 stage 通過。ASan 較早 cost-workload 時限 STOP 也保留，新的冷 900s stage 通過；最後兩個 ASan family 實際為 867.520s／864.858s，均在既定時限內完成。另預備的 1800s ASan 接續沒有執行。這些冷時限調整不改 normal cost cadence／門檻。

原完整 WIC prefix 3063／3063、own Down／Move／Up=55／121／51、9889 events、27,595,109 bytes。獨立 auditor 驗證完整前綴、raw metrics、plan／coordinates joins 及 227 receipts；release 已核，uncertainty=false。它不是 physical gold 或遊戲採納。

冷收據 CLI 的 genuine Windows Release seal 正向＋14 個拒絕控制通過，verified files=1031。可只驗證以下已凍結收據：

```powershell
& .\out\windows-qualification-release-reader03\pas_qualification_verify.exe `
  --build-binding .\out\prelive-20261006\current-build-release-win04.json `
  --expected-sha256 5a5bede090a3fdc0d2ae3b497099937886d0d432f333e0b091d0db57d27f2b2f `
  --source-commit cd21144440b1d02c11eafb3f332e63d20c0079cb
```

## Windows A/A 實量與停止

環境：Windows NT 10.0.26200.0、Intel Core Ultra 5 125H（14 cores／18 logical）、RAM 34,037,383,168 bytes。Release、相同 root／flags／DLL 閉包；背景程序摘要保留在 `out/prelive-aa-aa01/environment.json`。748 個 source rows 包含 747 實際 Git 檔案及 1 個 Linux `aux/CMakeLists.txt` Git blob 的安全檔名副本，原 Git 路徑另列；沒有省略 required formal source。

Campaign `windows-aa-aa01` SHA `db1830e59551cb3a4ca9ee6d9dcaaaa64031525ff84ebbdfe0169a0938c31ef0`。Tap1–4 → Hold closed-reset-v2 1–4 → dense1–4，固定每筆 2560 attempts、256 warmup、2304 measurement、20ms。

下表是 **measurement capture→owner** 毫秒；jitter=p95−p5。raw 中另保留 full 分母、observer／hook／owner、全部 phase lateness、injection、producer、writer／fault／release，不相加各階段 p99。

|run|owner-seen full／measurement|n|p50|p95|p99|max|jitter|peak RSS MiB|資格|
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
|Tap1|2494／2242|2242|22.683|35.945|42.595|53.491|18.465|54.742|PASS|
|Tap2|2394／2143|2143|27.065|40.821|48.584|53.069|22.187|55.391|PASS|
|Tap3|2424／2171|2171|25.285|39.365|46.230|51.622|21.469|54.895|PRODUCED_BUT_UNQUALIFIED|

各筆全 2560 attempts 產生、pool drop0、writer drop/fault0、injection failure0、release verified。全 Down／Move／Up 分別 32／0／32、32／0／32、28／0／28；measurement phase 分母另由 raw auditor核對。Full all-phase lateness 的 n／p99／max：64／6.928／7.481ms、64／11.738／13.682ms、56／10.185／10.223ms。來源 absolute age 仍 unknown，沒有真 capture／RPC 成本宣稱。

第一筆 producer 已 COMPLETE，但 AST-imported `Native` 的 `$PSCommandPath` 沒有檔案來源，audit 準備前產生空路徑錯誤。原 `out/prelive-aa-aa01/status.json` 保留；維護修補明確綁定 caller source，對既有 Tap1 做獨立 raw audit 與原 C++ numeric gate，沒有再產生 Tap1。維護 review01 的 PowerShell case-insensitive Campaign/campaign 物件覆蓋錯誤也保留（native launches=0）；fresh review02 通過。

只有在 Tap1 必要門檻確實通過後，才執行原凍結剩餘順序；沒有重開 STOP native stage，來源、corpus、cadence、門檻及已產生資料都未改。接續紀錄位於 `out/prelive-aa-aa01-continuation01/scope.json`／`status.json`。

**實際硬停止**在 `gate-prelive-release-aa01-tap3`：native exit=0、runner=125、identity_trusted=false、reason=`member-identity-untrusted:root-exit`。root-exit 列出 PID 21012，image=null、`member-image:5`；後續 active_final=0、held/streams=true，不能彌補失去的身分證據。第三筆 numeric JSON pass=true 也不能替代失敗的程序收據。

因此 Tap4、Hold4、dense4 共 **9 筆 NOT_RUN**；noise 未凍結、ABBA36 未跑。沒有再次執行這個失敗 gate 或以 GUI 功能 envelope 改寫它為 PASS。完整來源／raw／attempts／journal／STOP 及原 gate JSON 全保留。

## 尚未取得資格的部分

`CandidateLiveAdmission` 仍只有 UNQUALIFIED；完整 live receipt validator／qualified constructor／啟用 CLI 尚未實作。這輪新 CLI 完成的是 **冷編譯收據驗證**。Windows full production root 的初輪實際編譯已通過，但 source dirty 照報，沒有執行或升格成 production cost。

後續需先處理可驗證的程序生命週期／身分收據，依新版本重新凍結和驗收必要成本；不重開本次 STOP、不借不同版本 noise。之後仍需 production packet/action/wake／capture/RPC 成本、當前 emulator serial／app／profile／五指 mapping 與 physical Hold 接入/body/tail 語義收據，才能實作並核准相應入口。歷史 `emulator-5554` 未再驗；使用者已選 emulator、選曲並自行按 Play。

本輪没有 emulator／device／Fixture／touch／manual-session／模型／長期 goal 操作。完整 IN 結算 Miss=0、當前全曲／Chapter Legacy 分母與 IN 解鎖狀態仍 unknown／未完成，HD 不推 IN，沒有新增 AP 前置。
