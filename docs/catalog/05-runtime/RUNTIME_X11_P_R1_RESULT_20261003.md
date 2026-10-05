# X11-P-R1 開發與自驗結果

2026-10-03 Asia/Taipei。**交付完成，待總控獨立驗收；not-ready，不進X12。** A/A未過，ABBA=0；沒有再調策略、門檻或追加成本樣本。C36h tint1仍behavioral/experimental baseline，main50 donor/live0，X10b suppression OFF/否決，X10d-O未混入。

預先[protocol](RUNTIME_X11_P_R1_PROTOCOL_20261003.md)與成本前`method-options-capacity-before-cost.json`分別保留原規格及容量收緊。完整證據根為`measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1`，入口為`final-summary.json`、`all-cost-runs.json`及artifact ledger。不得重跑既有根或把舊X11資料賦給本輪binary。

## 來源與正式路徑

HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c/main/唯一worktree；正式src/include/root CMake未改，原dirty逐SHA保留。先重核舊274 source/binary＋1490 compiled dependency綁定，收尾再重核原1923 artifact ledger；原live SHA61ec…仍保留，未列compiled SHA仍Unknown。

新兩export源自frozen X11 B0/B1。57個src/include逐檔inverse，B1去掉8行known absolute executed cursor0 missing hook即還原B0；其他共用核心差為manual_session.cpp、SessionArchive cpp/header、新Wake和event helpers兩header共5檔。沒有observer/association/ownership/grace/lease策略變更。共同metadata改名R1；完整新source/compiled-input/DLL/profile單獨freeze。

| 新runtime | SHA256 |
|---|---|
| out/x11-p-r1/runtime-B0/pas.exe | 42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09 |
| out/x11-p-r1/runtime-B1/pas.exe | ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a |

305個source/binary/profile綁定、1580個實際CL/link/header/compiler/proto依賴另保存。generated provenance列65個source superset，實際編譯依賴以tlogs為準。runtime遞迴PE closure逐SHA及本機OS/API sets核對，無Torch/c10，不安裝依賴。環境Windows11 10.0.26200/Core Ultra5 125H/14C18T/32GB級/MSVC v145 19.51.36256/QPC10MHz；電源、priority/affinity未覆寫，OS背景負載未隔離。

原manual-session Wake class逐字抽取為`pas/action_wake.hpp`，正式caller與meter都調同一high-resolution timer/event與due delay函式。完整plan steps/receipt/release JSON抽為`session_events.hpp`；manual-session和meter共用，離線明記real_input=false。meter實際調SessionGameOwner(start/accept/poll/finish)→GamePlanOwner/ContactScheduler，兩版options{15,35ms,30ms}匹配live lead35；event/complete/close實際進同一compiled SessionArchive。沒有再用compact writer宣稱archive等價。

Archive只增加可選離線clock/delay/scaled mailbox測試參數及有界統計；default queue8192/16MiB、32×16MiB journal保留。統計計數本身有共用成本；normal/stress開啟QPC旁路計時，另有meter admission dump，正式manual-session不開這些clock/delay/scaled參數。**實測是正式共用路徑的離線負载，尚非整個manual-session性能等價。** 真capture/gRPC、SessionPerception HUD/result state、preflight/supervisor、result PNG及遊戲採納不涵蓋。

## 冷契約與memory驗證

| 新binary實際跑 | 結果 |
|---|---|
| B0完整原suite | 238/238，含27張RGB opt-in及historical UI evidence |
| B1完整原suite | 237pass/1預先聲明pending-grace fail |
| pending13 B0/B1 | 8pass/5預期red；13/13 |
| 新meter/負例 B0/B1 Release | 各10/10 |
| B1 Debug-ASan自有core＋新tests/pending | 10/10＋13/13，無ASan報告 |
| 每版舊512-input公共事件bridge | 每byte/row/EOF一致，contacts0，B0 receipts177/B1 127 |

全部suite skip/error/disabled0。唯一原suite失敗仍`GameOwner.SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`兩個pending_count舊期待1、實際0；没有改期待／filter。GTest新collector逐testcase計skip：舊首次B0 XML測出238 tests/237pass/1skip，即使root skipped缺省；原XML SHA bbb235a0…保留。

新增10tests驗空/缺/n不符/null metric拒絕、明示no-action只標not-applicable、完整JSON與缺row、events檔open/write故障、partial close、close後拒收、scaled mailbox雙界和第33段拒絕、OS Wake與FakeWake、lead35 due前/同時/後、gate/reset/finish、failed/unknown release及unknown/completed不重試。pending13另外保留active rootless旋轉Hold/body、alias/shared Drag、absolute cursor退休、fresh-return及capture/ready排序。

FakeWake是FakeClock測試driver，不冒稱OS timer模擬。OS pre-notify/2ms timeout/wait≤0用真QPC，property存XML；它們只是有限OS契約檢查，非OS latency統計驗收。ASan插樁自有core/新meter/pending/newtests；第三方與完整原suite/real PNG replay未全面ASan插樁或重跑。

bridge比較14,668,947B/7931rows及15,043,186B/9562rows，直接串流讀原檔，不產生另30MB public-events副本；SHA/bytes/EOF結果存summary。仍是舊lead0的512 synthetic/FakeClock保持證據，沒有新7722PNG replay或live byte identity證明。

## 成本全分母與停止

事前為完整JSON預算將normal改RGB256 attempts/16ms、owner128 attempts/8ms/32 targets；stress64 attempts/8ms、owner128 targets，RGBconsumer24ms、writer5ms/fakeRPC3ms。16固定pose/RGB SHA1a50b115…保持。normal receipt/release各512、frame257、archive samples16384；event admission3MiB/6MiB、單row256KiB，normal raw+metadata≤4MiB/stress≤7MiB，越界fault而不抽樣。全部failure/tests/source/logs/bridge計額。

先完成4個有界functional/concurrency stress（B1 owner為ASan，不當Release速度樣本），再8個B0 A/A。此順序成本前寫入method/options檔；noise JSON的`frozen_before_candidate_cost`僅指候選normal gate cost，stress已先跑。**AA凍結後沒有ABBA、追加stress或成本修復。** 縮短且較疏的normal owner負載不能外推舊512×128 dense成本。

12run共1792 publish attempts全部published、pooldrop0、consumed1684、owner1574、consumer skip108/decision skip110。全phase receipts1000=Down333/Move370/Up297；release calls99/requested IDs36、failed/unknown IDs0；contacts exit全0，worker/archive faults0，debug drop/discard0。完整序列化10,294事件/15,821,841B（stats不含Windows額外CR；實體磁碟長度另在ledger），每種event全分母與queue/byte peaks均有保存。每一backend release含空集合皆記call_index、start/return、requested/failed/unknown IDs。

每run `frames.jsonl`有全部attempt；`receipts.jsonl`有所有phase scheduled/start/return；`releases.jsonl`有逐call；archive包含完整decision/plan steps/cancel/coverage/notice/receipt/release/UI rows，完成後按實際segments逐row核對event全分母，缺row不pass。成本分布包括recognition、owner accept、同frame capture→owner、各phase/all-phase lateness、fake injection/release、event enqueue、writer final JSON dump/write及capture→all-phase injection；完整n/p50/p95/p99/max/p95−p5在每run summary/all-cost-runs保存。

owner accept不含之前/之後event serialization，但同framecapture→owner含前置decision JSON及前次archive負載影響。event_enqueue含meter admission dump及archive push dump；writer_serialize只量最後row dump，queue扣byte的另一dump沒有獨立分布；writer_write可含開段/換段/hash，最後close/flush/hash/summary落盤進elapsed_total，未拆成每事件latency。不能相加這些stage p99或稱每段磁碟fsync已量得。完整shared writer持續運行造成的競爭由本run直接端到端量測承受。

**A/A四項noise adequacy未過：**

| workload/metric | 2×最大pair差ms | 預先上限ms |
|---|---:|---:|
| RGB recognition p95 | 2.8452 | 1.79199 |
| RGB capture→owner p99 | 4.0574 | 3.06654 |
| owner accept p99 | .3156 | .25 |
| owner capture→owner p95 | 3.0190 | 3 |

另`aa-owner-1` owner114/128=89.0625%未達90%全分母消費gate；其他7 normal throughputs通過。所有normal all-phase lateness有73–160個樣本，p99≤1.606ms/max≤3.288975ms；這些通過值不能抵銷noise/coverage fail，也不能與舊meter直接稱提速。

stress RGB B0/B1 consumed18/18、owner15/17；owner B0 consumed64/owner45、128pending/4contacts，lateness p99=52.1182ms。B1 owner ASan consumed58/owner5，但receipt n0、pending/contact peak0；capture→owner p99=267.436ms。它有併發archive/最新frame的memory覆蓋，**沒有活動dispatch時序覆蓋，空lateness是Unknown，不稱通過**；活動接觸/failed release由ASan FakeClock契約另驗，未以額外第5個stress補救。RGB stress lateness p99約28.2374/31.031607ms；全部原樣保留。

failed/unknown release負例另有獨立C++20 deterministic probe連既有frozen B1 core，無重建meter或額外latency/stress run。每case保存6release calls（5在測試/1明示fake cleanup）、requested3IDs、failed2或unknown2、receipt1；fault及finish拒絕後無新Down。fake cleanup及成功RPC不等於真裝置釋放驗證。

## 修復、容量與分類

成本前唯一meter工程修復：receipt抽取初版停在++commands分號造成重複，修正後reconfigure/rebuild並核provenance；export複製曾展開historical junction，先逐SHA確認130,898,392B全是原檔副本，再只移除新根誤副本並恢復junction。清單/原SHA/初log保留，舊原檔及frozen根未移除。維護指令的早期缺檔讀取及freeze-script字串插值錯誤另列；negative helper DLL copy先用不存在zlib1.dll，於negative執行前失敗，改既有z.dll。收尾Get-Item曾把舊產物檔名中的方括號解作萬用字元而誤報長度不符，改LiteralPath後1923筆SHA/bytes全相同，另存finalization-admin-repair。這些不是第二次meter成本修復或重跑挑樣本。

開始campaign+prior實測8,217,563,371B；最終確切batch/out/campaign/free與SHA見final-summary（包含ledger自身，不照抄近似KB）。batch≤128MiB/out≤3GiB/campaign+prior≤8GiB/reserve5GiB均核對；原PNG只引用，誤展開副本已核SHA移除。本輪沒有刪原raw騰空間。

X12容量已選明確修訂：保留原native32×16MiB=536,870,912B/round硬限，scaled segment regression驗第33段拒絕；pending-only≤6輪計畫改512MiB/round。6輪journal3,221,225,472B＋6×(result4MiB+metadata1MiB+standby4MiB)=3,277,848,576B，future live根仍12GiB且reserve5GiB。沒有口頭256MiB guard或新capacity策略，沒有建立live根。

**Verified：**來源inverse/shared Wake/archive呼叫與編譯綁定、完整JSON/逐release分母、collector/空metric負例、冷契約/限定ASan/bridge、native容量計算、AA負結果及ABBA0。**Strong inference：**抽取與共同diagnostic變化保持已測公共策略行為，不需第二owner機制解釋。**Hypothesis：**完整JSON競爭、OS排程與短窗樣本可能影響尾端，沒有額外profile消融證明，不改門檻。**Unknown：**正式整個manual-session性能／pending候選normal成本差、原live未列compiled SHA、真capture/session/RPC/裝置採納、14lost opportunity利弊、77Miss與跨曲效果。交付候選待独立驗收，不自簽總控，不自進X12。
