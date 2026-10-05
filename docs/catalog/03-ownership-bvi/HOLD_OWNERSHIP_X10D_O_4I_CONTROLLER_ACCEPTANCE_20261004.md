# 4I總控獨立驗收：簽收平台partial，實作未通過；續派4I-R1

2026-10-04，Asia/Taipei。**簽收4I的停止、保護與平台partial交付；不簽演算法pass、core no-go或候選採用。** BVI-1尚未可信build，候選測試、Debug／ASan及52PNG審核皆0次。總控靜態閱讀另找到需要修補的有限history與harness問題，授權下一個同族冷修補包4I-R1；不是owner整合或live階段。

開發chat `01a106f2-157d-7dc3-8053-e9bfe95457ed`「實作 BVI-1 觀測與時序關聯冷契約」。原[結果](HOLD_OWNERSHIP_X10D_O_BVI_RESULT_20261004.md)、[交接](HOLD_OWNERSHIP_X10D_O_BVI_HANDOFF_20261004.md)、[4R驗收與原4I授權](HOLD_OWNERSHIP_X10D_O_4R_CONTROLLER_ACCEPTANCE_20261004.md)、source、inputs、oracle、out與失敗收據皆封存不回寫。本頁是續派入口，較早README／總帳的「O未開始」屬當時狀態。

## 獨立核對與判定範圍

總控以新維護腳本 [check-4i.ps1](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/controller-review/check-4i.ps1) 核SHA／bytes、案例映射、Git狀態與容量，未執行開發runner、configure、candidate、build、test、full replay、runtime/cost、emulator或觸控。[integrity.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/controller-review/integrity.json) 記 **2031個不同檔案、2089項hash引用、0不符**，包含1788份保護來源及交付、freeze、依賴引用的重疊組別；不是整個SDK或全部ignored raw重驗。

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`一致。原454個Git路徑保留，4I新增24個external路徑，共478；本頁另加1個，收尾共479。正式src/include/root CMake無diff；原apps等既有dirty以保護SHA核對，未被清理或重置。

20組頂層vector展開69 cases，normalized-execution／oracle／typed-execution的名稱及順序完全對應；另22 supplemental。這是清單核對，不是207個layer-case或22個case已執行。原oracle SHA `90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425`未改。首版規格／oracle先於candidate，最終輸入作者修訂與typed實體化在candidate作者期間、首次configure之前；不得升格為最終所有輸入都先於candidate凍結。

開發容量獨立重算 **4144966B**（batch4040933B＋external104033B），舊out374705B，交付時aggregate8279758023B；與原收據一致。本總控review及本頁另占原8MiB reserve，實際settled值見 [controller-receipt.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/controller-review/controller-receipt.json)。原開發收據不改。

## 平台partial的確切意義

本包3次native命令包括兩次runner selfcheck與一次Release configure。第一個selfcheck為cmd參數語法錯誤exit1，第二個plain argv為exit0；兩者自然quiescence通過，均非candidate test。configure使用Community MSVC14.50.35717（native19.50.35729.0）、CMake4.2.1與Ninja。native log完成configure/generate、root exit0；owned job仍有2個程序，15秒內未自然歸零，wrapper125、`descendants-nonquiescent`。收據為root PID6120、creation FILETIME134355935842315767；terminate owned job後記active_final0／cleanup_zero=true，streams_completed=false。

runner source支持先suspended launch、assign owned kill-on-close job再resume，以及有界等待／只清理owned job的路徑。**總控核對的是source與既有收據，沒有重現歷史清理。** 現有收據只有root身分與active數，缺兩個殘留member的PID／creation／image；不可猜它們是vctip，也不可拿exit0或CompilerId／ABI probe EXE當BVI建置通過。當前只讀搜尋的Community LLVM目錄只有clang-format／clang-tidy，沒有找到可用clang-cl／clang++；不據此假設替代compiler可立即使用。

原15秒gate不放寬；A原2秒gate亦未動。4I執行工具repair記0，下一包只可用原授權剩餘的一次工程修補，補足owned-member證據及負控制；不重啟A guard、vctip、OS／registry／安裝工具鏈支線。

## 靜態發現：需要修補或補足驗證

以下是source與fixture閱讀所得，**不是已執行失敗**；本輪未編譯任何新反例，亦不據此判BVI整個family no-go。

1. **P1：第7份樣本可影響當前輸出。** [bvi.cpp:60–74](../../../research/x10d_o_bvi/bvi.cpp)在6份history上先計算current relation，最後才evict再存current。故當前輸出可能受6份past＋current影響，雖然回報samples≤6。原V17-samples7只驗事後witness已移除，沒有證明該witness已不影響這次relation。R1須先定義含current的6份因果窗口，加入「僅即將淘汰最舊樣本提供歧義或確認」的輸出對照，再修正。
2. **P1：独立證據去重與span可能誤計。** 同檔69／71行只和上一個接受的RGB／descriptor signature比較；合法A/B/A序列可計3次，實際僅兩份不同觀測。72行span用current time減first，即使current是duplicate；例如不同端部A@0、B@10、C@20ms加C@40ms，3份獨立端部只跨20ms，source路徑卻可報40ms而達30ms門檻。須在有界有效窗口內定義去重與真正獨立證據的首末時間，補A/B/A及duplicate延長span負例；新鮮current支持仍與獨立確認數分列。
3. **P1：缺RGB到fake約束的完整斷言。** [main.cpp:54–80](../../../research/x10d_o_bvi/main.cpp)雖對RGB執行extract→relate→constrain，但61行RGB只核measured／geometry等，relation／independent／usable／Move／Down主要由另寫typed descriptors驗。三層都綠仍不足以證RGB提取出的端部和signature能產生預期約束。須保留分層報告，再加入同一RGB序列實際經過三段的端到端冷斷言；不是接正式owner。
4. **P2：oracle欄位漏檢可被靜默跳過，ID變形未被驗證。** main.cpp61行在所有layer篩掉未知欄，75行unverified列表抓不到它，94行就可能容許PNG gate。`permutation`是已知metadata，但拼錯的必要斷言也可能全層跳過。需在執行前列出每個expected欄位的層／斷言覆蓋，明列metadata白名單，未知或必要但未核欄位使aggregate fail。另30／45行未把Constraints.contact_id輸出及做renaming對照；V18 contact-id變形目前不能抓錯contact。須按改名映射核contact傳遞，不是刪掉ID後說通過。
5. **P2／待完整序列核實：靜止持續支持只驗一幀。** V02-stationary-current原追加70ms重複樣本，前3份仍在窗口。constrain82行要求attached也有重新計算的r.usable；若新鮮合法靜止畫面持續至既有3份獨立端部全離開6份／90ms窗口，source路徑將不再Move／refresh。R1須先凍結跨越兩種淘汰邊界的已接觸靜止正例，逐步更新fake receipt／last-contact，並配支持消失、歧義、過期／unknown／completed負例。澄清新Down確認與既有contact當前支持不同，不用延長grace、永久鎖owner或保留無界歷史救正例。
6. **P2：typed effect期待無輸入來源。** V04-contained-effect期待effect=true；main.cpp35–37行typed adapter沒有賦值effect，typed-execution該descriptor也沒有此欄，但59／61行把effect當typed必驗measured欄。預設false會造成harness假失敗。須明列欄位與layer適用性：若typed要驗effect，補獨立宣告且可追溯的fixture；否則明示由RGB層覆蓋並禁止宣稱typed驗了它。不修改原oracle期待以遷就實作。

修改涉及明文化契約、實作或harness須分開說明；新契約可澄清4R已有的有限history／stationary原則，不能把不利原反例改掉或把physical unknown升格。普通coding錯誤與不可滿足的核心觀測claim分開分類，不能讓任何harness欄位錯誤自動成為family否決。

## 續派4I-R1：同族冷修補與一次有證據的執行嘗試

依使用者順序派送授權，用 **GPT-6.1 Sol／xhigh**，只開一個開發chat。總控不做產品實作。先讀本頁及既有必讀專案文件，將以下全部落成可審查的protocol與oracle差異映射後再修candidate：

- 新source `research/x10d_o_bvi_r1/`；新batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/`；新out `out/x10d-o-bvi-r1/`。只複製所需最小source／fixture，不複製整個舊batch或PNG。原4I及以前所有source／docs／measurements／out、controller收據、HEAD/index/dirty保持SHA／bytes不變。新結果／交接docs使用R1新檔。
- 先保存六項發現的最小正負案例、oracle與欄位覆蓋表，再修唯一BVI-1 family；原20／69＋22保留在分母，新增案例另列，不刪失敗、不調像素／matching門檻，不搜索新family。原oracle不可回寫；若有真正輸入作者錯誤，保留原版，另存可核對差異及獨立理由。無法滿足的同輸入核心期待要停下交反例。
- 在新runner完成一次有界工程修補，保留原版與diff。只補owned-job member的PID、process creation、image及觀測時點／缺失理由，核member歸屬；PID再用或已退出不能假稱相同程序。身份／收尾控制最多3個預先聲明的小型commands（自然結束、非零exit、故意留owned child的負例），每個≤30秒、15秒quiescence、只有owned job可終止、每stream≤4MiB。負例必須證明能識別預期child並因不歸零被拒，cleanup記active0；控制不過就停。控制工具只限必要maintenance，正式BVI與harness仍C++20。
- **最多一次新的Release configure。** 只有已凍結修補source、oracle、protocol，身份／收尾controls通過後才執行；同原15秒quiescence，native exit／自然收尾／cleanup分報。與舊包同型失敗時，交新member證據並停止，不再configure挑成功、不做工具第二修補、不加OS／toolchain探索。原exit0／failed gate仍失敗。這次的新增價值是可辨識殘留member，不是無資訊重試。
- configure可信通過才可最小standalone Release build及全部冷測試；編譯並行≤2、每command≤300秒、每stream≤4MiB；可用且同gate可信時再Debug／ASan。任何不可可信owned-process收尾立即停。全部實際命令、case／assertion分母、失敗、sizeof／實際有界metadata／probe計數、source／binary／dependency closure與覆蓋限制照報。未執行填未驗，不能因exe存在而算通過。
- **R1本包先不做52PNG audit。** 先取得可靠冷契約與執行結論，再由總控驗收決定是否動用原至多兩次PNG額度；原已用0次。這縮小本包，避免靜態漏洞未關閉就向真圖推進。

不接正式observer／owner／scheduler／actuator，不啟動full replay、runtime/cost/stress/AA/ABBA、B插樁、emulator/ADB/真觸控、模型、goal、automation、commit/push或自行派下一包；觸控backend不得載入。到停止點或完成即交總控獨立驗收。若仍平台partial，應帶來source修補與具體程序證據，不得冒稱演算法驗證成功。

## 共用原額度，不因R1新root歸零

R1沿用原4I總64MiB＝development56MiB＋controller8MiB，**不是新增64MiB**。原development已4144966B，R1開發剩54575290B，R1 source/docs/tools、snapshots、failed logs與新batch均列入；controller本次與R1後續review合計仍≤8MiB。兩包out合計≤256MiB，原374705B亦carry。逐檔logical entry length計帳；舊資料刪除0，PNG複本0。

aggregate沿用8GiB：完整campaign＋prior45307809B＋O外部72115B＋O總控9546B＋4R外部54056B＋4R總控11851B＋4I外部104033B＋本頁bytes＋R1所有外部檔；controller review已在campaign，不能再重加或漏掉。本頁／review完結值以controller receipt為開工基線；新R1不得重跑旧settle覆寫receipt。開工仍核free≥原64MiB＋256MiB＋5GiB，並在保留其他已占用資料下保證aggregate不超8GiB。

產品驗收仍為Chapter Legacy全曲解鎖IN及完整IN結算Miss=0。現行章節分母與各曲目前解鎖unknown；C36h tint1 baseline、main50 comparison/donor 50/27/11/live0、suppression OFF、P excluded不變。**X12仍not-ready，沒有新的IN zero-miss或遊戲改善證據。**
