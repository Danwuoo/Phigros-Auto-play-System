# Disabled live seam portable checkpoint — 2026-10-06

**本次凍結來源的 Release、Debug、ASan+UBSan 全矩陣及原圖 3063 prefix 都通過。這是已被新反例攔住的 checkpoint，不能當作最終 software readiness。**

獨立 reviewer 在完整矩陣之外，用未改動的既有 family-v2 pixels 證實：
production `OwnerCandidateStep::accept` 在 100ms lease 到期邊界，若先 accept 才 poll，
可讓舊 active body 錯誤續命；`Runtime::dispatch` 先 poll 的路徑則會釋放。
父工作包已將這個反例列為 blocking，準備共用 `accept` 的 expired-first 修補與新來源重驗。
本報告保留修補前全部成功與失敗證據，不變更既有 corpus、gold、原 oracle 或任何舊報告。

## 同一凍結來源與建置

三配置 `source-before.sha256` 完全相同，SHA256：
`147cc6d9a4225393e5c9c14c181dd53764f507b22db6610c2fc80ae0ba8fb056`。
每配置完整 source snapshot、編譯器／ar SHA、flags、每個測試的 source-before-run、
source-after 與父端 13-file implementation seal 全數保存。當時所有前後檢查一致；
後續修補的當前工作樹不能冒稱仍是這份 checkpoint。

- Release：`-O2 -DNDEBUG`
- Debug：`-O0 -g0 -D_GLIBCXX_ASSERTIONS`
- ASan+UBSan：`-O1 -g0 -fno-omit-frame-pointer -fsanitize=address,undefined`
- ASan/UBSan 無 findings；容器 ptrace 不支援 LeakSanitizer，明確使用 `detect_leaks=0`，不宣稱 leak-free
- 13 個原 production translation units 全數重新編譯，不重用舊 checkpoint 的 ELF／archive
- Windows `src/manual_session.cpp`／root CMake／candidate admission 與 packet 等都在 SHA closure；Linux 未編譯或執行 Windows endpoint branch

| 模式 | 新 archive SHA256 | 全矩陣 exit |
|---|---|---:|
| Release | `0bbc3d07358396d571832e0500348ebca43d3d1c2cca9ada436f9509b9423c8f` | 0 |
| Debug | `91a31cb9855ea469c209d359c95fd3b043f773d5aa4b147004d1b9751ce3a5bc` | 0 |
| ASan+UBSan | `7009c670658d0bdd836a5089acbcb32e1d703daa3f0ec6d270f609bf543997ae` | 0 |

## 完整分母，分開列報

每配置原矩陣 **1132/1132**，分組與舊 typed-tail checkpoint 相同：
owner/front/current/own-prefix 350、portable independent 57、runtime 32+12、
active-body/tail guard 35+15、workload 145、scheduler 66、typed-tail reviewer 130+57、
family verifiers 104+104、normal-tail goal 25。`full-denominators.tsv` 逐組檢查實際數字、
checks 長度及每個 pass，未只看 exit code。

另每配置：

- focused disabled-seam contract **47/47**，live admission=UNQUALIFIED、device endpoints=0
- 獨立新 seam reviewer packet **63/63**、runtime v2 **91/91**，由 reviewer 連結上述同配置 archive
- 1 個 overdue-Flick case；沒有虛構 assertion 數
- PNG wrapper **11/11** controls；原 WIC samples **108/108** byte-exact，屬 decoder 分層證據

原矩陣三配置共3396 check executions；新 focused141與獨立 reviewer462另列，
不是3999個不同案例。獨立 reviewer v1 的舊90/91負結果未覆寫；v2只把
`no_tail_cap` 控制恢復成已封存 corpus 的原0/0/0期望，renderer／gold沒有變動。

主要證據：

- `evidence/live-seam-final-{release,debug,sanitizers}-01/`
- `../../docs/research/live-readiness-20261006/review/live-seam/final-{release,debug,sanitizers}-01/`
- 新 blocking 反例：`../../docs/research/live-readiness-20261006/review/live-seam/sequencing-check/probe.json`

## 同一 Release Runtime 的原圖 3063 continuous prefix

- 原 selection SHA `a2f91ae9e121a713283a0017e7fef012f307cd8557b80c1b71f4f7e1d2aabe44`
- 隔離還原 root 的 **3063/3063** PNG，逐張 bytes／SHA／CRC/profile 核對；沒有複製另一份 PNG
- 單一連續 archived source clock、自己的 ledger；沒有 seed 舊 contact、cursor 或 action
- 自己的 **Down55／Move121／Up51**；verified release、active0、ledger valid
- 9889 raw events，27,594,746 bytes；完整 rows、events、PNG/RGB digests 全保存
- 新 report SHA `c004a1d02c266bc92be69d60a3c12d1668617021ea70940544af546a4177fecc`
- 未改動的獨立 raw auditor：integrity、all input、plan join、coordinate join、complete prefix、metrics 均 true，errors=[]
- 227 injection receipts、140 release calls，無 failed receipts／release uncertainty
- typed-tail targets／plans／confirmations／normal-tail Up **全部0**；仍是 safety prefix，不證明真實 tail 改善

Raw 在 `evidence/live-seam-final-release-01/prefix-3063/`；獨立審查在
`../../docs/research/live-readiness-20261006/review/live-seam/raw-audit-3063/`。

## 容量與不可外推的門檻

全程在20GiB floor以上；每配置起跑 reserve 為 Release64MiB、Debug128MiB、
ASan640MiB，另加16MiB write margin；每次 compile／run 也檢查 reserve。
原圖 prefix 逐幀保留16MiB margin。所有 capacity rows、STOP／失敗與原始 receipts 保留。
成功的 transient `.o` 先逐個核對 archive member bytes／SHA，再移除；可從 archive 恢復。
完成後自建 ELF 有獨立 ZIP CRC／逐 entry SHA recoverable archive；三個 `.a` 暫留供後續修補。

没有執行原3938 frozen oracle、Windows/MSVC/native QPC/WIC/GTest、真 capture／gRPC／touch、
AA/ABBA live cost 或完整 IN 遊戲，也未執行 supplied unknown binaries。既有 donor CLI 未變；
production candidate C++ API 的 admission 仍 hard-disabled。Windows cost、physical、
game adoption 及 Chapter Legacy 全曲 IN Miss=0 繼續 NOT_READY。
