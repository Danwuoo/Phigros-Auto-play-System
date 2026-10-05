# Windows 收據更新：configure 失敗已直接核到，offending step 仍未知

2026-10-05 UTC。當前研究 checkout：`ebe955a6286ca1bb767b631948714a5545b7c174`；補件包註明的 repo 基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本報告只讀新包 A9（含 `.cmd`）及 D6、比對 Git source，未修改正式 source、既有工具、原 oracle 或 STOP；未啟動 Windows、PowerShell、裝置、遊戲或產品程式。

## 1. 本輪究竟改變了什麼

| 先前假說／缺口 | 新證據 | 更新判定 |
|---|---|---|
| configure 的 41-byte syntax error、exit1 只來自作者報告 | A9 的 result、verification、stdout、stderr 四檔與 manifest 所列 original bytes/SHA 完全相同；彼此一致 | **已直接核收據**：實際 cmd root native/runner/verifier 為 **1/1/1**，程序自然收尾完整；不是 CreateProcess 沒有建立 root |
| 斜線、vcvars 或 CRT/cmd quoting 可能造成 configure 錯誤 | `.cmd` 與 frozen argv 可讀；没有實際 `%cmdcmdline%`、entry、vcvars end 或 CMake entry；未附 `argv-readback.json` | **仍不能指定 offending step 或唯一根因**；D19/D20 的斜線證據不能跨層當作這次 cmd 的根因 |
| D19/D20 的 mixed-separator 只是 source＋作者分類 | D6 的 oracle/result 配對完整：20 cases、18 pass、2 fail；兩項都是 `transaction-root`。classification 保留 actual path／required prefix | **失敗種類與第一失敗位置已直接核到**；不是完整交易跑完後 rejected，也沒有 native wrapper 執行 |
| 「改成 Join-Path」可能足以完成診斷預驗 | 此次續讀同 SHA 的 `diagnostic.ps1`，发现 `FinishDiagnostic` 寫 `tx.dir/state.json` 後卻讀全域 `Evidence/state.json` | **另有後續 source-level 阻塞**：在原 D19/D20 前提下，若走到該行，全域 state 仍為 INIT_PENDING，會被 READY guard 拒絕。這不是舊收據已觀測的第二錯誤，也未實跑 |
| frozen source 與目前 Git 的身份只按版本敘述 | A9 freeze 23檔：17檔原 bytes/SHA exact；6份 CMD 的 Git LF bytes 在記憶體轉 CRLF 後才對上 historical SHA。D6 shared source 3檔 exact | 可精確連結現行 source 與收據；**不能把整個工作樹或去識別 CMD 說成原執行 bytes** |

結論仍是工程分層恢復：保留已驗程序所有權／清理核心，修正新測試入口的真實交易邊界，再對新允許範圍做有界 Windows wrapper 分步診斷。此包不提供 Windows build/suite 成功、候選資格或 zero miss 證據。

## 2. 證據身份與去識別界線

以下 `C/` 代表包內 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-control/`，`D/` 代表同 campaign 下的 `hold-ownership-x10d-o-bvi-build/`。

- Package manifest SHA-256：`ddc738c400ad29be11ffdcf9015b4acd31aed770b6bbcea60b55b08aab0178b3`。
- 本分支重算 A9＋D6 **15/15 檔的 derived bytes/SHA**，全部與 manifest 一致。這是本輪做的完整性核對，並非重新執行历史測試。
- Package 的 `original` 和 JSON 內嵌 hash 是 historical source 身份；`derived` 才是提供副本的 bytes 身份。原私人根被替换後，舊 hash 不能直接驗副本。原件未變等敘述是製包者聲明；本輪只驗收到的 bytes 和已附對應。
- A9 result 2078B、verification 2126B、stderr 41B、stdout 0B 沒有被去識別改寫。D6 oracle 1429B、STOP state 339B 也沒有改寫。其餘 relevant 副本要經 original→derived 對應讀。
- 提供的 `configure-release.cmd`：**700B**，SHA `94e5bdc42b9a59fbb5afa892edd12bcf0266aff85a983db5ef5cb41286c8e7e4`；manifest original：**676B**，SHA `97417c2ad42fa25e5377def41939c78309b1d89bf2d69f4c1b29d144f3f6fc7f`。副本有8個 CRLF、10個雙引號、33個 `/`、12個 `\`、NUL=0。這不等於兩者執行等價。
- Git 的同名 CMD 是 **668B／LF**，SHA `23c84b13c7e3bfeaf11caa2ede5bd6d2dc422582e53cd2e26ff7648b5f67aad0`。只在記憶體把 Git bytes 的 LF 轉 CRLF，可得到676B及 historical SHA；沒有寫出、替换或執行這個重編碼檔。此比對獨立於匿名副本，且不證明當時的 Windows 環境。
- D6 final receipt 內 `self_bytes=5681` 仍是原收據資料；收到的 derived 檔是6431B。不能因兩數不等說原收據毀損，也不能用5681B當目前副本大小。

完整逐檔結果與可重跑只讀程序：[`receipt-audit.json`](evidence/windows-receipts/receipt-audit.json)、[`audit_receipts.sh`](evidence/windows-receipts/audit_receipts.sh)。未把原始大包複製進 Git。

## 3. A9：實際 launch、exit 與 cleanup 的精確讀法

### 3.1 可從收據直接成立

`C/configure-release-command.json.spec` 與 `C/contract.json` 的同名 stage 完全相同：

- executable spec 為 `C:/Windows/System32/cmd.exe`；argv 為 `/d`、`/s`、`/c`、單一 wrapper 路徑；expected native/runner 為0/0。
- `result.facts.launch=launched`，`create_attempted/created/assigned/resumed/root_exited=true`；實際 root image 為 `C:\Windows\System32\cmd.exe`。
- `exit_code=1`、`runner_exit=1`；verification 的 `reason=exit-predicate`、`verification_exit=1`。result 與 verification 內全部 facts 相等。
- `elapsed_s=0.1338654`；stdout seen/written/實體檔都是0；stderr三者都是41；原訊息為 `The syntax of the command is incorrect.` 加 CRLF。
- `identity_trusted=true`；assigned-suspended snapshot 的 PID／creation／image 與 root 相同，membership before/after 均true。
- `active_at_exit=0`、`active_final=0`、`natural_quiescence=true`、`held_all_signaled=true`、`streams_completed=true`、errors空、無overflow；唯一 held wait 在同一 deadline 前 signal。
- `cleanup=not_needed_verified_zero`、`termination_returned=null`。沒有把強制終止的125或未知退出替换為1；按該 runner 的欄位來源，這個1是 `GetExitCodeProcess` 取得的 **cmd root exit**。它不是獨立量得的 vcvars 或 CMake exit。
- 最後 state 是 STOP／revision9／consumed4／known4，三個成功控制 receipt 的引用保留，失敗 configure 沒有變成成功收據。

兩個容易誤讀的欄位：command 收據的 `launch=not_launched` 是 `BeginTransaction` 在 native 呼叫前保存的狀態，不否定 result 的 launched；STOP 的 `running=configure-release` 是未清掉的失敗stage欄位，不是程序仍活著的證據。

這份最小包没有三個成功 control receipt 的 bytes、完整 ledger/protection 或 checkpoint 檔。可核它們的引用，不能宣稱本輪重驗了其全部控制或整棵閉包。其歷史成功仍與本輪 configure receipt 核查分列。

### 3.2 為什麼仍不能定罪 vcvars、斜線或 CMake

舊 CMD 的 line5 呼叫 vcvars 並把 stdout 導到 nul；line6 把任何 `errorlevel >= 1` 映成 wrapper exit1；line7 才是 CMake；line8 傳遞其 `%errorlevel%`。所以同一 root exit1 可由不同路徑產生。stdout0 不等於 wrapper 未進、vcvars 未進或 vcvars 成功。0.134秒只說明很早結束，不能替代階段trace。

`owned.cs:99,106–108` 的 source 確實以 CRT 式 Quote 組 commandline、再呼叫 `CreateProcess(exe,cmd,...)`。目前匿名 spec 的 wrapper 路徑無空白／雙引號，因此該 Quote 分支不會額外包它；但這只是 **source 對匿名輸入的分析**。舊收據沒有 `commandline_to_CreateProcess`，也沒有 actual `%cmdcmdline%`，不能冒稱取回完整實際命令列。外層 argv、cmd 的 `/c` 字串處理和 batch 內部 `call` 是不同邊界；Microsoft 也對 `/s` 的 quote 處理另作定義。[cmd 文件](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/cmd)

三個候選原因的地位：

1. **forward-slash executable／batch 路徑**：仍可設計受控比較，但不是既成根因。CMD 的 vcvars/CMake/ninja 路徑與 `-B` 的 mixed separator 分別由不同消費者處理，不能泛稱「所有正斜線都錯」。
2. **vcvars 初始化／繼承環境**：contract 只給 vcvars64.bat 的39B／SHA和工具依賴pin，沒有該batch內容、下游vcvarsall/VsDevCmd、展開後環境或分步退出碼。MSVC官方說明這些命令檔及環境與安裝選項相關，不能從版本pin推成初始化已完成。[MSVC 命令列建置文件](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170)
3. **CMake 已被呼叫並失敗**：沒有 entry marker、CMake log或被觀測的CMake root，所以不能成立；也不能以此斷言它絕對沒被短暫啟動。收據的 Job snapshots 是離散時點，assigned snapshot 在root尚未resume時、後續在root exit後，並非完整子程序建立紀錄。

原 `CreateProcess` API 的成功只表示建立程序，不替該程序的初始化／工作結果簽字；本次的actual root exit與收尾要由result另讀。[CreateProcessW 文件](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)

## 4. D6：18/20 是哪一層，斜線實際證明什麼

### 4.1 直接核到的失敗

- `diagnostic-oracle.json` 與 `diagnostic-pretest.json` 的20組 `(id,mode,expected)` 完全相等，重算18 pass／2 fail，與 `failed=2` 一致。
- D19 `full-rejection-transaction`、D20 `full-positive-transaction` 都預期pass，實際都是 `error=transaction-root`、`facts={}`、pass=false。
- oracle 為一次、最多20案例，`actual_CreateProcess_allowed=false`；result 的 `actual_CreateProcess=0`、`fake_facts_not_native_verification=true`。
- D11–D13 中名字為 `native_exit` 的0或1是 **FakeFacts 的測試欄位**，不是程序退出；D12/D13 的case pass只代表預期reject分類成立，不代表wrapper pass。
- final與STOP state一致：真controls0、wrapper probes0、product commands0、candidate layer cases0、native consumed0／known0、diagnostic slots0、out_new0、product_adopted=false。不是「configure失敗後重試兩次」，是尚未到達真native的前測失敗。

### 4.2 literal prefix 證據及不能跨越的邊界

failure-classification 保留的 actual path 在 build evidence 根後用 `/scratch-diagnostic/D19`；required prefix 用 `\scratch-`。對這些ASCII字串進行大小寫無關的 literal startswith：false；只做 slash-normalization後為true。此小型比較沒有執行 PowerShell、Windows canonicalization、IO或security guard，**只驗證可見字串不相等**。

這與 source 完全對應：`diagnostic-pretest.ps1:7,25–27` 拼mixed路徑，`transaction.ps1:12–13` 先做字面prefix，失敗就拋 `transaction-root`，尚未 `NoAlias`、`BeginTransaction`、output reservation 或完整診斷交易。D19的分類有實際字串；D20的原 `actual_path` 未另收在classification中，但其來源同一拼接分支、result同錯誤，足以把兩者第一失敗層與cmd問題分開。

新入口宜 canonical `Join-Path` 組成合法scratch根，保留原 exact-root／reparse／舊根限制與反例。把比較改成寬鬆substring或關掉guard不是本研究建議。

### 4.3 新發現：修斜線後仍應檢查交易 readback 的根

這是 **條件式 SOURCE 發現，不是本輪已執行的修復或新的歷史失敗**：

1. pretest line4要求全域 `Evidence/state.json.status=INIT_PENDING`；D01–D18 沒有把它轉成READY。
2. D19/D20 line25另建假的 `$s=State`；line27將它寫入case `$dir/state.json`，`tx.dir`也指向case根。
3. `FinishDiagnostic` line37 正確呼叫 `DurableState $tx.dir $tx.state`。
4. 但 line38 若交易READY，卻以 `CheckState (Json "$Evidence/state.json") ...` 讀全域主state，而不是剛保存的tx根。`base.ps1:101–103` 第一個guard要求READY。
5. 因而即使解掉入口slash，若執行已走到這行、其他依賴齊備，仍會把未變的INIT_PENDING判 `state-blocked`。原D19/D20在NewTransaction已中斷，**舊收據沒有走到這裡**。

新測試根的修補應覆核整條 file-only transaction readback，包括主要根与case根、reject/positive兩條路徑；不應承諾「只換Join-Path就20/20」。原 `transaction.ps1`／owned／identity 核心不需因此重寫。另有既有INIT schema缺欄位與未seal contract placeholder問題仍在，見 [第一輪建置稽核](../BUILD_TEST_AUDIT.md#42-init-state初始化-schema-本身仍不完整)；D6的最終STOP保存成功沒有把initializer變成合格。

## 5. Source 身份：哪些可以繼續引用

- A9 freeze：23來源中17檔exact，包括 `owned.cs`、`identity.ps1`、`transaction.ps1`、`run.ps1`、BVI core／driver／harness。六個CMD只在LF→CRLF的有說明比較下對上原hash。
- D6 pretest.shared_source 的 `diagnostic.ps1`、`common.ps1`、`transaction.ps1` 三檔均 exact；所以§4.3分析的是收據聲明使用的同版diagnostic實作，不是後人另一份相似草稿。
- `owned.cs`／`identity.ps1`／`transaction.ps1` 在 build 與 control 目錄亦保持相同 SHA。A9成功收尾、D6 fake前測與未來新wrapper的資格仍各自獨立。
- A9 state→contract／freeze historical hash與包內manifest original對應一致；D6 final→state／failure／pretest／oracle／inputs同理。這不自動補齊包外控制、dependency、ledger或Windows安裝樹。
- 新 `wrappers-v0` 的 stage markers以及 `run.ps1.commandline_to_CreateProcess` 只屬新的未執行草稿；D6明示probes0，不可拿它們反填A9的空trace。

## 6. 下一個有界驗證應回答的問題（未執行）

1. 在新授權的獨立Windows測試根，補完整INIT schema和 canonical case-root，把D19/D20推到完整reserve→RUNNING→result→receipt→readback；加入「實際readback是tx根」的正負例。保留舊STOP與失敗bytes，不改oracle取pass。
2. 固定同一未變的owned/identity/transaction核心。純argv/version probe收：明確`lpApplicationName`、真正送入CreateProcess的命令字串、`%cmdcmdline%`、wrapper entry、vcvars start/end及即時保存的exit、fixture argv讀回和預期0/7返回。仍完整核native/runner/verifier、Job/held/streams/cleanup。
3. 第一份trace決定下一個最小對照：無entry先看呼叫邊界；有vcvars start但未成功end，查batch／環境；vcvars成功而target未進，查該命令；target實際進入才討論其工具錯誤。不要一次重寫所有quoting或同時更換compiler／CMake版本。
4. 只有新wrapper診斷陽性、source/contract另凍結後，才有理由按授權做configure/build/wrong-contact/full suite。新Linux冷suite結果即使另處通過，也不能當Windows關卡。

本輪到這裡只建立「兩個工程失敗層分開、已讀到實際收據、下一個測試能定位哪個邊界」；沒有把未授權的Windows行動實作或啟動。

## 7. 重跑本輪只讀核查

從repo root，對已提供且已安全解包的目錄執行：

```sh
bash docs/research/zero-miss-20261005/round2/evidence/windows-receipts/audit_receipts.sh \
  /path/to/unpacked-evidence .
```

輸出只包含整理過的匿名收據事實、bytes/SHA對應與source比較，不載入或執行包內腳本。此腳本的assertions是本次資料一致性核對，**不計入20診斷case、349正式回歸或356 BVI layer-cases**。

方法更正：早期曾用暫時Python脚本核對收據metadata；依專案C++20／必要維護shell邊界，該腳本已移除。現在保留並重新執行的是bash＋jq的檔案／hash／JSON引用維護核驗；其主要收據欄位、15檔hash與source配對逐項對比早期輸出相同。沒有用Python或shell執行候選策略、Windows命令或原預驗。
