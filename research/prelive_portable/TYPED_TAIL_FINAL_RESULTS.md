# Typed-tail final portable validation — 2026-10-06

**Release、Debug、ASan+UBSan 三配置全過；最終真圖3063張 prefix 亦通過獨立 raw audit。**
這是 Windows 前的軟體／證據驗收，成本、physical／遊戲採納及完整 IN Miss=0
仍未驗證；不把 synthetic 正常 Up 當成實戰改善。

## 實際凍結來源

| Source | SHA256 |
|---|---|
| `src/game.cpp` | `e6b8412e0c923321b19b8962df9159ca60d23c585f4a0943cc5a61c30e4a48f8` |
| `include/pas/game.hpp` | `5334b6d35696d09cb15d2a3482a3116ea3c737286a1bd16e7cd4ec2b48ee23c1` |
| `current.cpp` | `810c60301604770dbcde12d312a60784489ea0bedcf5db95bbb761761c486691` |
| `tail_current.hpp` | `45523b5c805569654721a53b3d2a24f016689f761bfa2f613c2f6d8419ac7d0e` |
| `runtime.hpp` | `d895b8f59fb844f0d0df361ad0355d39234c6d0312a8a0f1256066d5e5c8c7bd` |

完整 closure 與 test hashes 在每配置 `source-before.sha256`；執行後及交付前
再次核對一致。上述關鍵來源另保存於 `evidence/tail-final-source/`。

## 三配置完整分母

| Check group | 每配置 checks |
|---|---:|
| 原 owner113／front56／current68／own-prefix113 | 350 |
| Portable independent | 57 |
| Runtime first-release／RGB entry及新 round | 32 + 12 |
| Active-body／white-line tail guard | 35 + 15 |
| Workload diagnosis | 145 |
| Scheduler（13 cases） | 66 |
| 獨立 typed-tail owner／pixels reviewer | 130 + 57 |
| 原 v1／v2 family 獨立完整性 verifier | 104 + 104 |
| 新候選正常尾端目標 checker | 25 |
| **合計** | **1132** |

另每配置1個 overdue-Flick case，未虛構其 assertion 數。
三配置共3396次 check executions；不是3396個不同案例。

- Release：`-O2 -DNDEBUG`；Debug：`-O0 -g0 -D_GLIBCXX_ASSERTIONS`
- Sanitizers：`-O1 -g0 -fsanitize=address,undefined`，ASan/UBSan無 findings
- LeakSanitizer因容器ptrace不支援而關閉；早期LSan fatal保留，不宣稱leak-free
- 現有 PNG wrapper 的11個 controls 三配置通過、108個舊 WIC RGB samples
  byte-exact，為 decoder 分層證據，不重複灌入上表1132

主要結果在 `evidence/tail-final-{release,debug,sanitizers}/`；各 family verifier
及25項正常尾端目標在 `evidence/tail-final-family-verification/`。
獨立 reviewer 的130+57結果由主工作包 `review/evidence/tail-*independent*` 保存。

## 修補的有限正例與反例

凍結648幀 v2 family 的B組，三個 offset（-2／0／2）各8個不同 own Hold，
各有 **8次 typed confirmation + 8次 normal visible-tail completion**。
A baseline及absence／effect／no-tail-cap控制不取得正常尾端主張。25項 checker
逐格明確檢查此目標，不將「所有 input讀完／release安全」冒稱正常尾端完成。

獨立反例包含body／Move權限外溢、payload/tag或reason降級、重複／衝突證據、
future／stale／非遞增時間、未知或已完成 contact，以及已存在 alias繞過closing。
早期被發現的失敗及provisional archives保留，最終修補不改 frozen v3/front oracle。

## 最終真實3063張连续 prefix

- 使用隔離還原的原 PNG 與原 selection，3063/3063，逐張bytes／SHA／CRC核對
- 使用同一Runtime、同一連續archived source clock；不seed已知contact或cursor
- 自己的 **Down55／Move121／Up51**；verified final release、active0、ledger valid
- 9889個 raw events，27,594,746B，完整 rows/events/input-hashes全部保存
- 報告 SHA：`c35e755e0da8fcefd5fc40ced4c73bc9f2f7f4dfaff7b40bd19786f7f40c4c01`
- 獨立audit：integrity、metrics、plan join、coordinate join、complete prefix、
  all input均true，errors=[]
- **此段真圖有0個typed-tail targets／plans／confirmations，0次normal-tail Up。**
  它證明新候選沒有破壞這段已測安全prefix，不證明真實尾端問題改善。
  這與synthetic三個offset共24個正常完成嚴格分開。

Raw配套位於 `evidence/tail-final-prefix-3063/`，独立審查位於主工作包
`review/evidence/final-typed-tail-prefix/`。較早96ef125/d895b8 safety checkpoint
及中止prefix仍保留，沒有用本次結果覆寫或重貼舊收據。

## 可重跑與剩餘界線

`run_tail_candidate.sh PINNED_JSON_INCLUDE MODE FRESH_EVIDENCE_PATH` 建置完整Linux
closure；先建立ignored `out/`，使用新輸出路徑。`pixel_prefix.cpp` 連結同版archive
及原環境libpng1.6.48，以原selection／還原full-recording root／新report路徑執行。
所有官方header、既有libpng及dependency SHA見decoder與prefix來源manifest。

未執行原3938 frozen oracle、Windows GTest186、MSVC/native QPC／WIC、真capture／
gRPC／touch、完整live成本AA/ABBA或完整IN遊戲；未執行ZIP內任何Windows binary。
Windows CMake／run.ps1 recipe及來源closure由父工作包另交付，尚待當地實際驗證。

容量曾因並行產物跌破20GiB floor，當時未啟動final replay；停止新增大檔並完成
經授權的byte-exact物件／舊binary archive與hardlink dedup後才繼續。最終prefix
開始及結束均在floor以上，且逐幀保留16MiB安全寫入margin；所有原始資料、STOP及
失败收據未刪。較早Linuxbinary已封存者可從父工作包capacity receipts恢復原路徑／SHA。
