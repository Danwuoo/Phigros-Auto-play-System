# 4I BVI-1：平台 partial，尚無候選驗證結果

2026-10-04，Asia/Taipei。**本包停止於 Release configure 的程序收尾門檻；BVI-1 未完成可信建置，所有候選測試／Debug／ASan／真圖審核未執行。** 這是平台 partial，既不是 BVI 觀測成功，也不是演算法已被反例否決。交總控獨立驗收後停止，不自行接續。

授權為 [4R總控驗收與4I契約](HOLD_OWNERSHIP_X10D_O_4R_CONTROLLER_ACCEPTANCE_20261004.md)。新source位於 `research/x10d_o_bvi/`；證據根 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/`，new out `out/x10d-o-bvi/`。C36h tint1仍baseline；main50 donor/live0、suppression OFF、P不混入、X12 not-ready。Chapter Legacy章節分母／目前IN解鎖unknown，完整IN Miss=0沒有新證據。

## 已保存的交付與未執行分母

原4R controller receipt／ledger與保護來源在開工重核1788個不同檔、0不符，含原454個Git路徑及O/4R/out封存；不是所有ignored raw或整套SDK的重hash。HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、index SHA260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1與既有dirty保留，收尾核對見本根 protection-after.json。

先存 PROTOCOL.md、CONTRACT.md、normalized.json、獨立oracle.json與 specification-binding.json，再寫唯一BVI-1來源。20個頂層vector展開69個cases：V00/01各2、V02有continuation及stationary、V03–05各1、V06兩variant、V07一、V08兩line order、V09–11各1、V12兩、V13一、V14三state、V15一、V16二十、V17八、V18十五、V19兩。另有22個supplemental精確邊界cases。

| 層 | 預定分母 | 本包實際執行 | 判定 |
|---|---:|---:|---|
| RGB→descriptor（給合法測得的ROI／line幾何） | 69 case，3組full-byte same-RGB controls | 0 | 未驗；不是完整Note discovery |
| typed link／context | 69 case | 0 | 未驗；typed支持不算RGB辨識 |
| 新fake lifecycle constraints | 69 case | 0 | 未驗；沒有接既有owner/scheduler |
| supplemental capacity／source／deadline／receipt | 22 case | 0 | 未驗 |
| Release candidate build/tests | 最小standalone target | 0 | configure gate失敗，未build |
| Debug／ASan | 可用時同源冷驗 | 0 | 因停止線未啟動，未驗 |
| 原4R只讀52PNG audit | 至多2次 | 0 | 前置不成立，未凍結或執行driver／packet |

所有cases與mutations均保留；未執行不能填pass、fail、skip=0以冒稱完成。sizeof、實際metadata、probe分母、candidate性能、real accuracy／Miss均未量測。constexpr/static_assert是尚未編譯的source限制，不能當量測。

## 契約具體化與來源限制

RGB extractor API僅接RGB／FrameKey／合法ROI幾何與全列current lines，不接 expected support/cap、renderer layers/private labels或手述4R部位。RGBA整數合成、sampling、draw order、裁切、line遮body與contact flanks明列。body事實與line/action資格分開；≤6／90ms history與≤2 alternatives、獨立確認及當前freshness分開。過去RGB slots為0，以當前FNV64及descriptor signatures判重，碰撞／範圍限制明示，不能聲稱精確past RGB byte比較。same-RGB controls只在獨立renderer測試中用完整bytes比較。

V12只有一致execution enum／unknown receipt，另以prefix0矛盾輸入作invalid負例。V16只改current context而保留前兩份history；V17完整寫出90ms／40ms／6samples與+1序列。V03/09/11仍是incoming／Tap機會，非physical新Note或注入成功；V15只驗rear492/508/512與兩份幾何past-line，normal owner Up／遊戲tail未驗。2887原事實是contact2消失，identity684/197保留且cursor=null。

規格作者工具兩次初版錯誤（dictionary重複key及Copy alias）均在任何normalized/oracle寫出之前，失敗source／記錄保留。typed fixtures的實體化及補充邊界在candidate source作者期間、所有build/run之前保存。source inspection另發現V00應保留原line y575／sequence2、unknown state receipt、region-order contact索引重編及typed signature不能含private object-list差；修正輸入另存 normalized-execution.json／typed-execution.json，初版保留，oracle期待／SHA沒有改。

**排序限制如實列明：** 第一份normalized/oracle／renderer與input contract在candidate source之前，最後上述輸入作者修訂則在source作者期間、第一次configure之前。不能宣稱首次規格已完全無需澄清，總控須獨立審查最終可執行契約。所有C++source尚未compile，無正結果可據以採用。

## 命令與停止收據

新 run.ps1使用suspended native launch→owned kill-on-close Windows job→resume，capped pipe每stream4MiB、timeout≤300s、build並行≤2，native exit與15s自然quiescence分別記錄；只終止owned job，不掃／殺同名OS程序。執行工具工程修補0次，沒有展開A guard、vctip、OS／registry支線。

1. runner-selfcheck：cmd對帶空白的shell command argv回syntax error，native exit1，job歸零與streams完成；不是candidate test。原命令／失敗log保留。
2. runner-selfcheck-plain-argv：明列 `/d /c exit 0`，native exit0、自然quiescence及cleanup均通過。
3. configure-release：安裝的Community MSVC14.50.35717（native識別19.50.35729.0）、CMake4.2.1與Ninja。native log顯示configure/generate完成，root exit0，但 **active_at_exit=2，15s內沒有natural_quiescence**；wrapper exit125，reason=`descendants-nonquiescent`。owned root PID6120／creation FILETIME134355935842315767。Terminated owned job後 **active_final=0、cleanup_zero=true**，elapsed20.2350393s。

因此config產物封存為failed-gate/unverified；CompilerId及ABI探針EXE不是BVI binary。沒有build bvi_tests、沒有test command、沒有Debug/ASan configure。沒有拉長quiescence或retry configure挑成功，沒有觸控backend、full replay、runtime/cost/stress、emulator/ADB、模型、goal、automation、commit/push或自行派送。

## Freeze、容量與交接

freeze包含实际source/tests/tools/contracts／初版與修正input／configure產物、配置實際tool fingerprints與最小nlohmann header kit。沒有candidate binary或其DLL，因為未build；不能編造compiled-input closure。MSVC/SDK/STL/system DLL完整closure及候選依賴執行未驗，已用及必需來源路徑/hash與覆蓋限制另列 dependency-manifest.json。回存source kit不等於在別台已可獨立build成功。

容量在final-receipt.json／artifact-ledger.json逐檔settle：開發≤56MiB（含外部新source/docs/tools、全部失敗與snapshots）、總控保留8MiB、新out≤256MiB、aggregate≤8GiB。保留prior45307809、O外部72115、O總控9546、4R外部54056、4R總控11851，再加本包全部外部檔；不只算campaign，也不新root歸零。entry lengths是保守帳，不是NTFS allocated bytes。原資料刪除0／PNG複本0。

最終交 [4I交接](HOLD_OWNERSHIP_X10D_O_BVI_HANDOFF_20261004.md)。需要總控簽收平台partial與來源保護；不把未編譯source簽為觀測候選，不自行修平台／接owner／轉live或續派。
