# X11-P-R1 總控獨立驗收

2026-10-03。**簽收共用量測路徑、冷契約與負成本證據；維持 not-ready，不進 X12。** 一項「512MiB實體硬限」說法經反例否證，已修訂 forward plan 的磁碟保留帳；原開發文件與freeze保持歷史原樣。來源：[R1結果](RUNTIME_X11_P_R1_RESULT_20261003.md)、[protocol](RUNTIME_X11_P_R1_PROTOCOL_20261003.md)、[handoff](RUNTIME_X11_P_R1_HANDOFF_20261003.md)。

## 狀態與簽收範圍

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一registered worktree，原dirty保留；正式src/include/root CMake無改動，main策略仍50/27/11。本次沒有emulator/ADB/manual-session/真觸控/模型、完整PNG replay、成本run或stress run，也沒有重build任何frozen source/binary。新增的總控C++20工具只重算原raw，並用未改的frozen B1 core作deterministic archive容量反例。

C36h tint1仍behavioral baseline，main50 donor/control。R1 B0 `42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09`、B1 `ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a`是新binary，不能繼承原live結果或原compiled SHA未知部分。B1唯一策略差仍pending missing八行hook；suppression OFF，X10b仍否決，X10d-O未混入。

## 實際独立驗證

讀取R1完整meter、archive、gate、test、event/Wake helpers、prepare/bind/run/collector/ASan/negative scripts，核manual-session提取diff與SessionGameOwner accept/poll/finish呼叫。Wake class保持原字串，plan/receipt/release payload欄位保持；plan accepted_ns現在於建立steps JSON前取樣，屬共用診斷時間點差異，不宣稱全部diagnostic timestamps與舊版等價。Archive統計本身也增加共用成本，已測公共策略等價不代表整個manual-session時序等價。

| 總控實际操作 | 結果 |
|---|---|
| SHA及length逐項重核 | 305 source/binary/profile、1580 compiled dependencies、376 negative-tool inputs、2059 artifact entries全部符合 |
| src/include inverse | 57檔去掉B1唯一pending hook還原B0 |
| runtime provenance重跑 | B0/B1離線exit0，embedded source SHA逐項符合export |
| 完整原suite重跑 | B0 238/238；B1 237pass/1預先聲明pending-grace fail，未改期待 |
| pending契約重跑 | B0 8pass/5預期red；B1 Release/ASan各13/13 |
| 新R1 tests重跑 | B0/B1 Release與B1 ASan各10/10；所有八套suite skip/error/disabled0 |
| skip collector重跑 | 原X11首次B0 XML正確讀為238tests/237pass/1skip，root skipped缺省不漏計 |
| 兩版512 synthetic/FakeClock bridge重跑 | streaming summary bytes與R1 freeze相同；B0/B1 receipts177/127、contacts0；lead0橋接不冒充lead35成本 |
| failed/unknown release probe重跑 | raw及summary bytes與freeze相同，共12calls/2receipts；cleanup明列為fake |
| A/A gate重算 | 複製八份summary至新根，noise exit2，JSON bytes與freeze相同；不執行ABBA |
| 獨立C++ raw audit | 全12run的frame/receipt/release分母及11組可由raw導出的n/p50/p95/p99/max/jitter重算符合；全部receipt/release與archive對應JSON逐筆相同 |

raw audit亦核10294完整archive事件的各類分母，logical bytes=15,821,841、physical bytes=15,832,135，差10294B恰為每行多一個CR。writer/event enqueue成本沒有逐sample raw，僅能核其原summary、source及freeze，**不宣稱本次獨立重算這三組分布**。未重跑ASan並行stress；重跑的是ASan deterministic tests。

入口為 `measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1-controller/final-summary.json`，下含八套XML、bridges、release negatives、noise、raw-audit/audit.json與SHA ledger；重現腳本 `tools/accept-runtime-x11-p-r1.ps1`，獨立分析源碼 `tools/runtime_x11_p_r1_audit/`。新batch≤16MiB，獨立audit build≤64MiB，campaign+prior≤8GiB，精確bytes見final-summary。

## 為何仍not-ready

**Verified：** A/A仍有四項noise adequacy fail：RGB recognition p95、RGB capture→owner p99、owner accept p99、owner capture→owner p95；`aa-owner-1` owner114/128=89.0625%，低於90%。A/A後停止、ABBA0符合protocol。沒有新B1 normal成本比較，因此不能說pending hook性能已改善或已確認退步。

**Verified覆蓋限制：** 原B1 ASan owner stress receipts0、pending/contact peak0。它涵蓋部分並行與archive記憶體路徑，沒有active dispatch壓力覆蓋；不以hard_gate=true冒稱lateness通過。重跑的ASan FakeClock active/unknown/release契約通過亦不能替代並行active路徑。

**Hypothesis，未驗因果：** normal owner僅128attempt、114–128 owner samples，nearest-rank p99主要依靠第二大sample；短窗、啟動效應、OS排程與完整JSON競爭可能影響重現性。這支持先研究量測設計，不能据此排除尾值、改noise門檻或直接重跑找安靜批次。工作負載縮為32targets與舊128targets不同，禁止跨批宣稱提速。

**Unknown：** 整個manual-session、真capture/session/RPC/裝置採納、原live未列compiled SHA、14 lost opportunity利弊、77Miss改善及跨曲效果。這些不由offline測試數推導。

## 容量反例與已做的文件修正

**Verified反例：** 總控工具連既有R1 B1 pas_core，構造32個各256 logical bytes的event，SessionArchive segment limit=256；正常complete、fault=false。實際32段檔案總8224B，大於32×256=8192B。原因是Windows text-mode ofstream把LF寫為CRLF；原archive以dump字串長度累計。這不是新R1引入的策略regression，但否證「32×16MiB是實體檔案512MiB硬限」。原scaled第33段拒絕測試只證明段數，未驗實體byte上限。

不為容量改正式runtime。最新pending-only X12計畫保持native **512MiB logical journal限**，以Windows文字轉換最多2倍byte作保守磁碟reserve：journal每attempt **1GiB physical reserve**、standby8MiB、manifest/summary2MiB、result PNG4MiB；最多6attempt共 **6,530,531,328B**。此為邏輯上限加保守實體保留推導，不是新1GiB runtime cutoff；12GiB future live根、另5GiB free reserve、no recording/clips保持。manifest/result等原保留假設需在未來preflight核對，沒有claim OS allocation-unit overhead精確相等。原R1的3,277,848,576B帳本保留為被本節取代的準備估算。

## 下一步：X11-P-R2量測有效性，尚未派送

不再重做已完成的Wake/archive提取，也不先調pending策略。下一包先用既有raw量化sample不足、startup/steady-state、owner輸入負載與diagnostic工作邊界，提出一份可否證的成本資格設計；不得僅把8 A/A原封不動再跑一次。

- 先定樣本量與尾端分布的解讀、workload契約、同輸入/clock/options、全分母與環境記錄。若擴大窗長，先設bounded raw/統計與容量，不能用無界journal換樣本。warmup資料另列、保留，不能事後挑段刪尾值；instrumentation成本及正式default路徑差異明列。
- 修並行stress的coverage契約：事前設計能建立active contact的固定stimulus，驗asserted fixture確實走active/release路徑；未命中即coverage unknown/fail。不能把零receipt memory run當action timing pass，也不能用runtime ID或舊touch充當真值。
- 新method/protocol與noise解釋先保存，再決定是否值得新有界A/A。原R1停止結果不重開；若設計不能提高可辨識性，就停止成本重跑、交具體缺口。新批配額、修復次數及停止線須先定，禁止多批試到pass。A/A仍不足則不做候選ABBA、不進X12。

這是新的量測問題，非證明遊戲改善的替代品。即使未來成本通過，仍需總控驗收後才進已授權≤6attempt有限live；本次不建task、不監看、不啟動live。
