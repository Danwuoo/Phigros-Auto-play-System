# Safety-fix checkpoint: current96ef125 / Runtime d895b8

這是完整保留的中途安全修補 checkpoint；後續 typed-tail 候選需要另外驗證，
不能把本表轉貼成其結果。Windows/MSVC、成本、physical 與產品目標均未升格。

## Runtime release 不確定性

原 `Runtime::finish()` 在 `SessionGameOwner::finish()` 拋出 release fault 時，
尚未賦值 `final_release` 也未排出自己的 backend release event。
32-check 新 fixture 以自己的 typed accepted Down／receipt 證明 **8 fail**，
原 header 與結果保存在 `evidence/runtime-finish-before/`。

修補只位於 Runtime：第一筆 finalization latch；擷取原 scheduler release report；
即使 owner finish 拋錯也排出原事件並 fail closed；不得第二次 release、重試或
unknown→success 覆寫。未知釋放後的 dispatch／新 round 被拒；已驗釋放才在
真正 new-round owner 建立時重設 latch。

結果：32/32 release controls + 12/12 RGB-entry／跨 round controls。
後者的 typed lifecycle result/menu fixture 與真正 RGB observer/owner 入口分列；
最初錯把永久 supervisor stop 當 round result 的 fixture 及其2 fail亦保留。

## 三配置

`evidence/final-safety-{release,debug,sanitizers}/` 全部 exit 0，來源 before/after
一致。為容量採 `-g0`／strip-debug；Debug 仍為 `-O0 -D_GLIBCXX_ASSERTIONS`，
sanitizer 仍為完整此 closure 的 ASan+UBSan。LeakSanitizer 因 ptrace 不支援而關閉。

每配置：

- 原四組 113 + 56 + 68 + 113 = 350 checks
- Portable independent 57
- Runtime finish 32、RGB/round 12
- Active-body 35、white-line tail guard 15
- 新 workload diagnosis 145
- Scheduler 66（13 cases），另1個 Flick case
- 合計712 checks，另1 Flick case；三配置同組重跑
- 同配置輸出的 v1 family 經獨立 checker 各104/104，故此層合計816 checks/config

第一次含 body repair 的 `safety-*` 保留每配置1項舊 characterization failure：
12幀內共同 Down+Up 的集合由 {503} 變空。145項新診斷區分「有 Down」及
「在短窗內已有 Up」，並明列 B 在短窗尾仍持有自己的 contact；未修改 frozen oracle。

## 真實3063張连续 prefix

最終本 checkpoint replay：**3063/3063、Down55／Move121／Up51、final release
verified、active0、ledger valid、無 stop**。每張 PNG 對原 selection 的 bytes
及 SHA、RGB24 profile、所有 chunk CRC 均驗證；單一 Runtime 累積自己的 receipts，
不灌入歷史 contact、cursor 或輸入事件。

- 原 selection SHA：`a2f91ae9e121a713283a0017e7fef012f307cd8557b80c1b71f4f7e1d2aabe44`
- 報告 SHA：`11818c7e60013b32e09ac8497c7f94a251c42c168db0c8ae2ecfcf7834e97faa`
- `evidence/final-prefix-3063/report.json` 及其 rows/events/input-sha256 配套
- 9889個 raw events，27,241,451 bytes；Up數不含 `release_all` 安全釋放
- 獨立 audit v2：integrity、metrics、plan join、coordinate join、complete prefix
  全 true、errors=[]；審查者證據另在主工作包 `review/evidence/checkpoint-prefix/`
- 計時是 Linux steady-clock 診斷，不是 Windows／RPC／physical 成本

初次 prefix 在容量提醒及再次 source-refreeze 時 Ctrl-C（exit130）；部分 rows/events
仍在 ignored `out/portable-prefix-3063`，中止紀錄在 `evidence/pixel-prefix-3063/`，
不計入3063通過分母。

## PNG decoder 與 WIC 對照

沒有自製 pixel decoder。取官方 libpng v1.6.48 的3份 header，commit
`ea127968204cc5d10f3fc9250c306b9e8cbd9b80`，直接連環境已存在的
`libpng16.so.16.48.0`，無安裝／下載執行檔。`portable_png.hpp` 僅做 signature、
大小、geometry、RGB24、CRC 與 color-metadata 邊界，再呼叫 libpng。

11個 decoder-wrapper controls 三配置全過；原 Windows WIC review-card 的
**108個 RGB sample 全部 byte-exact**。不把原系統 libpng 本身說成已 ASan 插樁。
最多一份4MiB compressed PNG、一份1280×720 RGB，另正式3-slot latest-frame pool；
不缓存8GiB整段 decoded frames。原圖來自既有隔離還原根目錄。

## 容量與保留

- 3063份本 lane 誤重複展開的 PNG 逐份 SHA 比對原圖與 selection 後，只刪掉
  自己的 duplicate，回收931,373,644B；原件全部保留，逐份 receipt 已保存。
- 自己的169個 `.o` 中介共321,707,136B 封成41,700,888B ZIP；全部 entry CRC
  與逐 entry streaming SHA驗證後才移除中介，exe、library、raw、source不刪。
- 45個 byte-identical 自產 Linux executable/archive duplicate 依批准原子替換為
  hardlink；全部原路徑與SHA保留，回收293,099,892B，old/new inode receipts保存。
- 完成後可用容量22,150,979,584B，始終保留20GiB floor。後續建置需再核容量。

本 checkpoint 不保證後续候選、Windows 建置、成本或遊戲結果；native CMake/MSVC
新增 recipe 僅提供，未在本環境執行。
