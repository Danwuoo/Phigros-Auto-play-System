# 4I-R2E：合併執行恢復、BVI冷驗與有界真圖審核

2026-10-05 Asia/Taipei。使用者最新指示「派送新task進行下一步，盡量一次做完多步」恢復開發與派送，取代[上一輪盤點](LEGACY_DEVELOPMENT_INVENTORY_AND_R2D_ACCEPTANCE_20261005.md)的暫停。本包使用GPT-6.1 Sol／xhigh，一個開發chat，總控只派送與獨立驗收。**本頁明確新授權一次R2D局部恢復實作及同一包內按gate連續推進；不回填R1 repair1/1為剩1，不改原失敗紀錄。**

依[已審R2D設計](HOLD_OWNERSHIP_X10D_O_BVI_R2D_REVIEW_20261005.md)，成功完成一個stage後應自行接續本頁已授權stage，不僅交計畫或停在文件完成。本包涵蓋盤點第1階段及第2階段；最後可附第3階段ownership整合的具體接口／證據缺口建議，但不自行接正式owner。hard stop仍有效，不能為做多步而忽略核心反例、保護失敗或不可信程序收尾。

## 新範圍及不變來源

新source `research/x10d_o_bvi_r2e/`；新batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/`；新out `out/x10d-o-bvi-r2e/`。唯一production execution attempt ID `bvi-r2e-20261005-01`，不能自行換根或重置attempt。總控派送證據另在同campaign的 `hold-ownership-x10d-o-bvi-r2e-dispatch/`，只供保護及容量carry。以上三個開發根在派送前不存在。

先讀AGENTS指定四文件、上一輪盤點、4I/R1/R2D契約與controller receipts，再核原保護集合。R2D controller integrity的2134份查核來源以source_anchors指向完整manifest／ledger重建；加其delivery_receipt、controller review所有檔、盤點頁及本派送頁／receipt。原506 Git paths加本派送頁為507；原HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`、dirty／原source／oracle／receipts／out保持。不可執行舊settle或舊runner，不回寫封存。

新正式BVI／renderer／harness／PNG分析仍為C++20。必要維護shell／owned-job interop可用；沿已安裝最小工具鏈與PNG讀取能力，不安裝／重裝、不改OS／registry／服務，不重開A或vctip支線。候選仍唯一BVI-1；不混P、suppression、調matching／像素門檻或alias／grace，不換family救結果。

## 同一包內的連續stage

**A：凍結契約並完成局部實作。** 先落成唯一RootBinding、精確stage allowlist、attempt狀態表、獨立P/S/E/C故障oracle、完整容量與來源binding。採R2D的精確新根／舊根拒寫、reparse／traversal拒絕、預先開啟輸出handle、durable RUNNING與同入口verifier、跨shell STOP、共享Result／nullable事實及有界nonthrow cleanup。入口僅允許預先凍結stage，不能繞過成任意Exe/Name/Arguments；第一次initialize單次许可不重用。wrong-contact負例改走共用assertion→aggregate→實際return→consumer拒絕鏈。

保留R1 candidate與全部原69＋新增20 cases、22 supplemental、4 schema/contact controls及coverage分母；沒有已證fixture錯誤不得更改原oracle。只複製最小必要source到新根，引用原fixtures，不複製整個batch／PNG。C++普通作者錯誤及局部實作可在首次正式execution freeze前修正，保留重要失敗。此次新授權是一個局部實作版本；正式controls開始後不另修runner再試，不建立第二attempt。

**B：先驗路徑／state／故障拒絕，再三項程序控制。** 允許R2D P01–P09、S01–S07、E01–E05設計的有界局部維護測試，以新batch下封閉scratch及mock native介面驗證；不得在舊根實際寫檔製造錯root案例，不使用未完成的runner啟動產品。預期拒絕須由oracle核驗，不等於attempt控制失敗；sandbox子測試狀態與唯一正式attempt分開命名且不能授予launch權限。這些測試不是child身份／真owned cleanup驗收。

局部拒絕核验全部成立、source凍結後，依序只執行natural0、expected nonzero7、owned-child拒絕各一次。全新root／PID/creation/image／membership／QPC、native exit／runner exit／verification exit分列；所有stage皆需receipt readback/SHA／自然收尾或預期負控制清理通過。原15秒quiescence不變，控制每項總≤30秒（含cleanup／drain），各stream≤64KiB。owned-child必须在15秒末仍可辨且被拒，僅owned job清理active0、streams完成；意外timeout、identity未知或cleanup未知不能算負控制成功。第一個非預期控制失敗即durable STOP，禁止後續任何native stage。

**C：最小standalone Release及完整冷驗。** 三控制可信通過才arm一次Release configure，至多一次。可信configure後build，先單獨跑wrong-contact-only負模式，驗實際report/failed row/status/native1與consumer拒絕；負模式預期拒絕通過才跑完整正例及原schema控制。假exit0、缺report或無關失敗均STOP。完整分層報RGB、typed、fake lifecycle、e2e，列每個case及assertion，不能拿typed代RGB或把未核欄位當pass。保留3組same-RGB countermodel、淘汰／去重／independent-span、長靜止支持與loss/ambiguity/unknown/completed/deadline、旋轉／late alignment及原邊界；不能全unknown換綠燈。

Release完整契約成立後，自行接續已安裝／可用且容量與相同收尾gate可成立的Debug、ASan，每配置各最多一次configure/build/完整cold suite。事先read-only確認不支援的配置可明列不可用，不假裝pass；已啟動後失敗則STOP，不用skip逃過失敗。每個產品命令≤300秒、編譯並行≤2，各stream≤512KiB；native超限／truncation不能丟失而當成功。編譯／必要冷驗任何非預期失敗交具體分類及完整失敗，不擅改oracle、不跑第二輪追pass。普通coding bug不等於family no-go，但本正式attempt仍終止。

**D：合成通過才接原52PNG有界審核。** 本頁重新授權原4I的至多兩次只讀額度，原已用0/2；每次完整audit（包含失敗）計次。先凍結driver／52圖packet／當前合法ROI及全列line幾何來源／clock/context／輸出schema與來源SHA，driver只能從当前pixels及有界近期觀測取得candidate資訊。必要最小PNG driver及target應在C階段configure之前納入新source並凍結；不得因此額外configure，不連觸控backend。沒有合法上游幾何時列缺口，不偷用4R手述部位／oracle／physical label或舊動作。

前置為Release及本次已執行配置的冷契約全部成立，無未結束的測試／平台故障。全52圖列supported／invalid／unknown及原因，segment空history、6份／90ms、來源時間domain分清；不是full-prefix／full recording replay，不能由結果推Miss收益／ownership準確率。第一輪若有core counterexample或程序／資料完整性失敗立即停，不使用第二次；第二輪只可用于事先聲明的另一個觀察模式或驗證問題，有明確新資訊，不重跑選漂亮結果。兩次累計沿用原額度，不能再聲稱剩2次。

**E：一次交付整包結論與下一步接口建議。** 每成功stage接下一stage，不因做完控制／build／一組test就提前交「待續派」。交付可執行source／binary／input／實際依賴／DLL closure及限制、完整分母與全部失敗、精確stage receipts／STOP狀態、SHA／容量帳、52PNG已用次數；分類為工程partial、冷契約失敗需分類、核心反例no-go、或有界觀測支持。不自簽產品採用。若所有授權步驟成功，附下一包owner接口、unknown／completion／機會損失需要的具體驗收項；只寫交接，不接owner、不full replay。

## 限額、停止與容量

本次最多3個真process controls＋14個compiler／candidate／PNG native命令，具體stage/argv及每次預期exit在A先凍結。純文件維護、mock測試另有≤60秒／次、每報告≤64KiB、總維護記錄≤2MiB上限，不藉此啟動產品／child或繞过STOP。stage的預期非零7／125／contact1只有在predicate與收據全核後才是verification成功；其他非零、缺資料、identity不可信、失效容量或非自然15秒收尾皆停止。R2D例示檢查是設計，實際結果須新報，不沿用歷史root控制。

新開發資料最多40MiB＝41943040B，含全部external source/docs/tools、最小freeze、logs、reports、失敗；從原development56MiB剩51745629B扣，不新root歸零。建議包內預算：產品logs14MiB、control logs384KiB、三配置冷報告12MiB、兩次PNG報告共4MiB、source/freeze6MiB、state/maintenance/失敗2MiB，餘額約1.6MiB；任何一項不足要在launch前拒絕，不刪失敗或移到未計帳root。總控原8MiB餘額亦保留。新out≤128MiB＝134217728B；兩包舊out374705B與新out合計仍≤256MiB。

派送前anchor容量：aggregate8283838427B／8GiB，剩306096165B；development6974627B、controller1250743B、out374705B；campaign8238099150B。新派送資料與本頁先加入controller carry，詳dispatch receipt。保守預留40MiB＋128MiB＋原controller剩7137865B後尚餘122797532B（未扣本派送實際資料）；足夠不等於可放寬每項cap。為避免out與campaign帳分開造成誤用，本R2E aggregate額外納入新out實際bytes；不重算／改寫舊out歷史收據。公式為dispatch aggregate carry＋R2E新batch＋全部新external＋R2E新out，總控後續新review另計；舊資料不重複加。free開工及每階段維持至少原64MiB＋256MiB＋5GiB＝5704253440B。

不允許修改formal src/include/root CMake或原apps/tests、不重放全recording、不做runtime/cost/stress／B插樁、模型、emulator/ADB／真觸控／manual-session、goal／automation、commit/push、另派chat。Chapter Legacy全曲IN Miss=0仍是產品目標；HD解鎖／回歸、P/G/B照報。C36h tint1 baseline、main50 donor/live0、suppression OFF、P excluded、X12 not-ready不變。完成或hard stop交總控獨立驗收。
