# 4I-R2D：BVI-1最小恢復契約審查（design-only）

2026-10-05，Asia/Taipei。完整授權依[4I-R1總控驗收](HOLD_OWNERSHIP_X10D_O_4I_R1_CONTROLLER_ACCEPTANCE_20261005.md)。**單一推薦：保留R1的BVI-1 candidate／oracle，在未來另授權的新來源及新輸出根，局部修補root binding、attempt gate、誠實收據與共用contact assertion。** 本包只研究與保存文件，不實作runner或candidate，不啟動任何controls／configure／build／tests。原runner repair已用1/1、剩0；本包native execution授權false，六項source修補仍未編譯／未驗。

設計可把本次錯root及失敗後第二控制變成機械拒絕，但尚未證明修補後可執行、可辨child、可編譯BVI或可滿足核心期待。沒有BVI演算法pass、family no-go或候選採用。本頁所有「須／通過条件／拒絕」描述都是**未跑的未來設計条件**。

## 1. 開工證據與靜態界線

已讀AGENTS指定README、ARCHITECTURE、ROADMAP、跨曲研究，完整最新總控授權、原4I授權、R1結果／交接／CONTRACT／PROTOCOL、runner及C++ source、fixtures／freeze／binding／既有controls與stop-sequence。未喚醒原R1 chat `01a1072a-52d4-7433-bd2c-a9a1b8c360ee`。

[本包protection-before](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/protection-before.json)用完整既有manifest引用重核：2112份來源、2039份原保護集合，另核R1 final receipt、controller-review全部6檔及最新總控頁；去重2120檔、0 SHA／bytes不符。2326是原總控hash引用數，本包不拿它作新測試分母。compact收據引用原585722B manifest的SHA，不複製完整大清單。PNG只作manifest逐檔hash，PNG內容audit0、複本0。

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`、原503 Git paths與dirty指紋一致；src/include/root CMake無diff。原收據、settle、source、oracle、freeze均不改不跑。開工campaign8237966326B，aggregate8283663033B，development6826223B、controller1223753B、out374705B重算吻合。

| 已知歷史 | 支持的結論 | 保留未知 |
|---|---|---|
| natural native0；PID36100／creation134355963840706648／完整pwsh image；歷史natural active0、streams完成 | 一個root的既有身分／自然退出收據 | 非本輪重新清理、非child控制 |
| natural後讀R1 receipt不存在，verification exit1，明確stop；其後仍執行nonzero native7、外層工具exit1 | 收據錯root及停止違規有工具歷史證據 | 不用外層1否定已記native7，也不把native7當controls aggregate成功 |
| nonzero PID18652／creation134355964021246632，歷史natural active0 | 第二個root的歷史收據 | 無法辨認原4I兩個殘留member；owned-child未跑 |
| 8份新增誤放資料已核SHA移回，原保護檔未改、誤放path不存在 | 證據保全成立 | 不倒推當時輸出邊界正確 |

歷史分類保持「runner工程／停止流程partial」，六項source與BVI結果另列未驗。

## 2. 一次解析的canonical root契約

未來controller須明確另授權一個attempt與唯一allowlist。推薦命名提案為 `research/x10d_o_bvi_r2e/`、`measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/`、`out/x10d-o-bvi-r2e/`；**此三根未建立、未授權，不能把本頁當啟動入口。** controller可採此提案；若改名，須先重新凍結精確binding，不能執行時任意換leaf。

只接受新controller核定的contract SHA及attempt ID，contract含固定repository絕對根、唯一evidence_root絕對值、source/out相對路徑、完整禁止寫入的舊封存根／檔manifest、容量carry SHA、stage→exe／argv／預期結果／輸出名稱。入口只接受stage與attempt ID；禁止任意Name／Exe／Arguments／output override。一次 `ResolveContract` 產生不可變RootBinding，所有Save、stdout/stderr、child身分檔、harness report、CMake source/out與容量枚舉都用同一物件，不另算batchPath，也不使用Replace或docs wildcard取代ledger。

順序固定在任何native launch、log／receipt建立之前：

1. 先只讀核controller契約SHA、HEAD/index、來源／oracle／carry，固定base解析source/evidence/out。拒絕原始輸入含`..` segment，即使正規化後回到allowlist亦拒絕；拒絕drive-relative、UNC/device、ADS、尾端dot／space等未允許拼法。
2. 對canonical完整路徑做Windows ordinal case-insensitive精確allowlist相等，並用分隔符邊界核repo範圍；不能只測字串prefix（`repo-other`不得通過）。寫入集合不得與任何原4I/R1/controller／本R2D封存根或保護檔相交。只讀input可在舊根，寫入API只持新allowlist；不改ACL／registry來實現此限制。
3. 新evidence/out根在initialize前必須不存在；source只能是已凍結的新source。對既有ancestor逐級拒絕reparse point／junction／symbolic link，不追隨未知alias。新目錄建立後重新核實際路徑與attributes；任何alias／競爭／無法確認即STOP。這是局部工作目錄契約，不聲稱抵抗惡意OS並行竄改。
4. 路徑全過才CreateNew建立attempt lock/state；預先CreateNew並持有本stage的command／result／verification保留檔及stdout/stderr輸出handle，寫入／flush／readback測試成功後才允許CreateProcess。reservation檔是`pending`，不能被下次誤讀為結果。Pump改用已驗handle，禁止在Task裡首次開未知path。report先在新根保留；保存失敗不觸發重跑。
5. 任一既有output或reservation、權限／磁碟／receipt建立失敗，保留失敗殘片並終止attempt；不刪檔重試、不回落舊root。根無效時不能往該根寫拒絕receipt；錯誤回傳外層，而合法attempt只能保持不可前進的INIT_PENDING／STOP。缺state或初始化未完成也是STOP等效。

`Path.GetFullPath(path, fixedBase)`能避免依賴變動cwd，但不代替上面的repo／allowlist／alias核對。[Microsoft Path.GetFullPath](https://learn.microsoft.com/en-us/dotnet/api/system.io.path.getfullpath?view=net-10.0) handle的final path可反映symbolic link解析後位置；本提案選擇拒絕reparse ancestor，縮小處理面。[Microsoft GetFinalPathNameByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew) CreateNew才會在既有檔案時拒絕，不能以Test-Path後WriteAllText保證不覆寫。[Microsoft FileMode](https://learn.microsoft.com/en-us/dotnet/api/system.io.filemode?view=net-10.0)

### 必拒絕的根／保存案例（全部design-only）

| ID | 輸入／故障 | 必要可觀察結果 |
|---|---|---|
| P01 | 本次R1 run.ps1第4行仍是原4I root | 精確allowlist拒絕，created/resumed native0，原4I任何log／receipt無新增 |
| P02 | 改用R1舊root或本R2D封存root | archived-write拒絕，不能因位於repo內就通過 |
| P03 | `../`越repo，或先出再返回允許leaf | raw traversal拒絕，正規化相等也不豁免 |
| P04 | `Phigros-Auto-play-System-other`／同leaf不同parent | repo boundary及完整相等拒絕 |
| P05 | 允許root ancestor被junction指到舊封存／repo外 | reparse拒絕，禁止alias寫入 |
| P06 | 新out已存在，或command／exit／stdout任一已存在 | existing-output拒絕，原bytes/SHA保持 |
| P07 | result／verification reservation無法CreateNew或flush/readback | receipt-not-writable拒絕，CreateProcess未呼叫 |
| P08 | caller傳不同evidence_root或child／harness仍帶硬編舊root | binding拒絕，容量及所有consumer取同一RootBinding |
| P09 | 完成preflight後path/contract/source SHA變動 | 在stage開始再核時STOP；若已created則誠實記launch已發生，僅做owned cleanup |

## 3. 小型機械狀態及跨shell停止

推薦在唯一新evidence_root下保存一個有界attempt state（≤64KiB）與固定stage receipts，不建服務／資料庫／通用平台。state含schema、attempt ID、controller/source/input/root-binding SHA、revision、已核receipt SHA、下一個stage及terminal原因。一次只持一個排他FileShare.None lock；第二shell拿不到lock即拒絕，不啟動任何native。

stage啟動前，先讀最後有效state及全部前序receipt/readback SHA；缺失、pending、JSON損毀、錯attempt／stage／root、SHA不符、重複呼叫或非白名單命令皆STOP。runtime入口没有bootstrap：它永不因state不存在而initialize；只读设置阶段依单次controller设置许可在已核canonical根建立INIT_PENDING。设置失败即交付停止，许可不得由同一agent重用。无有效初始化state的任意后续shell均拒绝native；新设置许可只能由controller另发。把唯一state先持久化為`RUNNING(stage)`並consume該stage一次額度，flush/readback成功才呼叫native。完成後由**同一入口內的verifier**核三種結果並保存verification，才轉下一state。外層shell的exit code不能直接轉state。

state更新採同目錄有界暫存＋替换並保留revision校驗；不完整／多義state一律拒絕。若寫STOP失敗，最後有效state仍是RUNNING／INIT_PENDING，所有後續shell同樣拒絕；不能把「還有舊pass state」當新launch資格。入口中任何launch必在state durable RUNNING之後；若此持久化失敗就是not launched。舊attempt不得reset或重新initialize，STOP不可轉回pass，新attempt只由controller另授權且不得自動生成。

| 當前state | 唯一允許操作 | 完整核驗後state | 失敗／未知 |
|---|---|---|---|
| INIT_PENDING | preflight roots／hash／capacity／reservations，native禁止 | PREFLIGHT_VERIFIED | STOP或保持不可前進pending |
| PREFLIGHT_VERIFIED | RUNNING(natural)，僅natural控制一次 | NATURAL_VERIFIED | STOP |
| NATURAL_VERIFIED | RUNNING(nonzero)，僅nonzero控制一次 | NONZERO_VERIFIED | STOP |
| NONZERO_VERIFIED | RUNNING(owned-child)，僅owned-child負例一次 | OWNED_CHILD_VERIFIED | STOP |
| OWNED_CHILD_VERIFIED | 只讀再核binding／capacity及三份verification，arm一次 | CONFIGURE_ARMED | STOP |
| CONFIGURE_ARMED | RUNNING(configure-release)，consume最多一次configure | CONFIGURE_VERIFIED或交付終止 | STOP；build拒絕 |
| RUNNING任一stage／STOP／缺state／損壞state | 只許保全資料及已有owned handle的有界cleanup | 不授予後續native | 所有controls、configure、build、tests命令拒絕 |

configure可信後才可能依新controller列出的固定stage順序build／完整cold tests；Debug／ASan亦需明確範圍、可用且同gate可信。任何未聲明stage直接拒絕。預期非零7及owned-child預期wrapper125是**負控制predicate核驗成功**，不是略過失敗規則。cleanup未知／缺receipt仍失敗。

本次具體反例：若natural完成後R1 exit receipt找不到，verifier記missing-receipt並STOP；即使agent另開shell要求nonzero，入口讀STOP而不呼叫Run。若verifier存STOP失敗，RUNNING(natural)也拒絕nonzero。這使「natural control failed; stop」不再只靠agent記得遵守。不能保留任何可繞過state的原run.ps1作新attempt入口。

未跑狀態反例S01–S07：missing exit receipt、native7被當自然0、receipt SHA被改、錯attempt ID、同stage重跑、STOP後跨shell啟動nonzero、wrapper崩潰遺留RUNNING；全部應拒絕後續native，詳細機讀設計見[recovery-design.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/recovery-design.json)。

## 4. 誠實receipt：已知進程事實不能被exception抹掉

最小修補保留現有suspended CreateProcess→owned kill-on-close job→核membership→resume及固定15秒自然quiescence、5秒cleanup界線。`Result`在呼叫Run前由caller建立並傳入，不能只在Run return後拿到；各API前後立即更新這個共享事實容器。外層catch只增加tool／storage error，不覆寫launch／exit／snapshot。布林型未知欄改nullable或明確enum；exit_code、active_final、identity未觀察不得默認0／true。

| 維度 | 值與證據条件 | 不能混用 |
|---|---|---|
| launch | `not_launched`：CreateProcess確定未呼叫或明確return false；`possibly_launched`：durable launch-intent後wrapper中断，return未知；`launched`：CreateProcess return true，root created（可仍suspended） | launched不等於resume成功；possibly不等於native0次 |
| execution | create_attempted／created／assigned／resume_attempted／resumed各自有值／unknown；PID＋creation＋image／QPC僅記實際量得 | suspended child清理亦須記root created=1 |
| root exit | `root_exited`須Wait/GetExitCode成功且native_exit非null，另記觀測QPC／UTC | root0不表示descendants0；API失敗不填exit0 |
| descendants | `unknown`／`observed_nonzero`／`natural_zero`，依完整owned job accounting／PID-list及時點 | 最後active0不改写此前15秒nonzero或missing identities |
| cleanup | `not_needed_verified_zero`／`cleanup_verified`／`cleanup_unknown`；verified須owned termination結果、完整final query/list0與有界stream drain成功 | CloseHandle kill-on-close意圖不等於cleanup已量得；unknown仍STOP |

durable command receipt先記`not_launched`；進入CreateProcess前flush launch-intent為`possibly_launched`，return true後寫launch-known checkpoint為`launched`，再到root-exited、membership／cleanup checkpoints。每command最多8份≤16KiB checkpoint，配合原≤8 snapshots／≤64 members；不輪詢產生無界記錄。checkpoints由預保留handle写入，以不同stage名保留，不反向覆寫先前已知事實。若某checkpoint保存失敗，記storage error、STOP／owned cleanup；不得繼續resume以賭下一次保存成功。

### 未跑保存／cleanup故障（design-only）

| ID | 故障位置 | 必須保存或可推出的最小事實 |
|---|---|---|
| E01 | launch前reservation／Add-Type失败 | 沒到CreateProcess可記not_launched；本包不執行Add-Type，未來若授權其初始化须在native前完成 |
| E02 | CreateProcess return true，assign／identity失败 | launched、resumed=false／unknown，保留已知PID；只終止本handle建立的suspended root或owned job，不能掃同名程序 |
| E03 | Run完成后Save exit失败 | 共享Result及已有checkpoints保留launched/root-exited與native_exit；storage failed，STOP；不能固定native_launch_possible=false |
| E04 | cleanup Observe／Active／Terminate／Task.WaitAll之一throw | 每操作隔離Try-record，追加stage／Win32／exception，繼續剩餘必要owned cleanup；主失敗與次失敗皆保留，cleanup未知不轉pass |
| E05 | finally Active／Observe／stream drain再throw，或wrapper被中斷 | finally每項有界、不讓單一异常跳過後续handle close；共享Result不丢；持久資料只剩launch-intent則possibly_launched／descendants-unknown，RUNNING拒絕任何下條命令 |

cleanup/finally改為不拋出遮蔽原失敗的bounded收集（errors≤16，超過明記overflow並STOP）。所有opened handles單獨best-effort close，记录return/error；查不到active_final填null。未assign的created root只用本次PI process handle清理並核退出；已assign只用本次job handle。自然zero與強制cleanup zero分别报告；streams的seen/written、overflow、fault／完成时间保留。Pump exception及Task.WaitAll exception不能直接逃出finally：官方API允许fault/cancellation产生AggregateException。[Microsoft Task.WaitAll](https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.waitall?view=net-10.0)

這不能保證磁碟／程序崩潰時一定保存所有事实。設計保證保守结论：若known checkpoint缺失，launch count是区间`[0,1]`／unknown，不是假称0；任何不完整state不放行。原R1兩次自然控制沒有证据发生E03/E04/E05，這些是静態风险与未跑故障设计。

## 5. 三個controls的機讀充分条件

共用必需条件：contract/attempt/stage/binding匹配；command/result/verification完整且SHA正确；root身份PID＋creation FILETIME＋完整image及member前后membership可信；owned PID-list在每次成功query仍须核`listed==assigned<=64`、active/list一致或明确失败，截断／查询失败／PID复用／inaccessible均不作trusted；QPC ticks与frequency、UTC分别保存；stdout/stderr在限制内且最后drain完成。cap／timeout／secondary cleanup/storage error均拒絕。官方PID-list若返回数小于assigned就是不完整；不允许无限扩buffer，超64拒絕。[Microsoft PID list](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_process_id_list) Query失败须保留GetLastError。[Microsoft QueryInformationJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-queryinformationjobobject)

| 控制 | 必须成立的predicate | 命令／verification区别 |
|---|---|---|
| natural0 | launched/resumed、root_exited、native_exit=0、runner_exit=0、reason空、完整natural15s内zero、无forced cleanup、identity trusted、streams complete、final zero | verification_exit=0才写NATURAL_VERIFIED；任何tool/storage失败STOP |
| expected nonzero7 | 相同自然收尾条件；native_exit=7、runner_exit=7、无tool error、reason空 | runner7是预期native值；stage entry在predicate/readback全过后verification_exit=0。外层工具若封装为1须另列raw tool exit，不能以1当runner7或验证成功；receipt不能建立就STOP |
| owned-child rejection | parent/root native0；新binding传入parent/child，不留硬编R1 path；child和parent各自身分receipt一致并含attempt；同owned job root-exit及15s末均观测到预期child PID/creation/image、membership前后真；natural=false、primary reason=descendants-nonquiescent、runner_exit=125；仅owned cleanup，5s内final完整zero、child handle退出、streams最后complete，无次错误 | 命令本身必须被runner拒绝；负控制verifier只有确认这种拒绝及cleanup才exit0→OWNED_CHILD_VERIFIED。125因timeout/log-cap/identity未知不是预期拒绝通过 |

维持原15秒gate，不以child提前自然退出作成功负例。child sleep25s、parent最多3s核到身分仍是原设计；未来须核parent exit至15s末child确实存活，root deadline≤3s、15s quiescence、≤5s cleanup、≤5s drain及有界余量合计native控制≤30s，逾时STOP、不缩短15s。有query身份竞速就失败，不猜child。

只控制本次owned job；默认CreateProcess children通常入job并不足以证明此child实际归属，仍要member证据，不启用breakaway。[Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects) TerminateJobObject作用于该job关联程序，API成功仍由后续query核cleanup，不凭意图签zero。[Microsoft TerminateJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-terminatejobobject)

两个历史自然root收据不可直接充任新attempt的三项controls。新控制即使通过，也不追认原4I两个unknown members；原历史保留unknown。

## 6. contact负控制必须经过真实aggregate拒绝链

R1 `driver.inc` 的wrong-contact-output分支只算fixture `guard_contact_id != actual_contact_id`，再check「reject=true」。這证明了数字不等，没证明逐帧contact assertion、failed_assertions、status、return及消费者拒绝。

推荐在未来新harness局部抽出当前`check`／`evaluate`／aggregate finalization，正例与负例共用同一AssertionAccumulator（本包不写C++）。wrong-contact fixture保留原4/1：将可控实际输出的contact_id置1、expected guard4，调用**同一个逐帧contact_id_transfer assertion**；其row必记expected4/actual1/pass=false，failures增1，aggregate产生cold_fail_requires_classification、程序exit1、consumer不得採用。另在1→4变形的renaming_output_contact共用assertion处注入错误ID，必须同样拒绝；不得删除ID后normalized相等。

负控制在独立有界aggregate作用域／独立report保存，原正例的失败不能被「这是负例」清零。为核真正的程序return及消费者，未来Release build可信后以同一编译harness的预声明wrong-contact-only模式执行一次：在正常output→逐帧assertion边界注入原fixture的错误ID，沿共用finalization实际return1、自然quiescence，单独保存≤64KiB report。调用方先读真实report核失败row／计数／status／native1、采用false，才签negative verification0；随后完整正例test仍必须单独pass。缺report、无关失败、假exit0或忽略失败均STOP。這是一次candidate/harness负测试，不是第4个process control，且本包不执行；不能以纯内存exit映射声称已经核过实际return链。Debug/ASan的完整分层仍保存负控制报告，不因Release负例而豁免其正例。

未跑的最小机械证据：C01正确ID4对guard4正例；C02错ID1对guard4触发共用row→aggregate→exit1→consumer拒绝；C03在1→4 mapping输出仍1被相同机制拒绝；C04假aggregate(pass0/exit0)、缺row或消费者忽略失败均使负控制checker失败。保留另3个schema controls及metadata／必要coverage拒绝；不称当前4!=1已验整链。

## 7. 六项修补及20新增cases的保留决定

以下均静态审查，**全部C++行为／sizeof／实际probe／assertion分母仍未验**。只保留为冷验来源，不逐项签pass。原RGB predicates、matching、6份含current／90ms、3独立端部／30ms新Down、100ms期限／60ms missing不变；physical owner未知，不接正式owner/backend。

| 项目 | 可以直接保留作来源 | 尚需最小更动／未来核验 |
|---|---|---|
| F1先淘汰再relation | bvi.cpp reserving5 past＋current、age淘汰；R01–R04及without-oldest共8 cases | candidate无需因runner失败而重写；冷验逐step输出及同current对照，不能只核retains |
| F2去重与独立span | window-wide RGB/descriptor任一见过不增确认；first/last/span/freshness；R05 ABA、R06 duplicate-span、R07 30ms正例共3 | 阈值不改；冷验RGB/e2e与typed分列，hash equality仍近似 |
| F3实际RGB→link→fake | execute中的e2e及原分层、同一render/extract结果、relation/usable/Move/Down | 保留driver；核实际执行coverage与完整分母，不能用typed成功替代RGB |
| F4 schema/contact | expected handler／适用层／metadata白名单、输出contact_id、逐帧transfer、1→4mapping | 只局部改wrong-contact负控制接共用assertion/aggregate；新差异及预期拒绝先冻结，原4 controls fixture保留不覆写 |
| F5长静止当前支持 | known一致fake receipt下current双rail/body/unique line支持Move/refresh，新Down仍3/30；R08/R09共2、R10 loss/ambiguity共2、R11 unknown/completed/plan/gate/source共5 | 共9 cases逐步核last_contact/prefix/ID；支持掉失121ms最后refresh→181ms满60ms释放。attachment_query仍外部fake，不证明物理身份或防错note跳接 |
| F6 effect/provenance | 原V04 effect=true不改；effect只RGB/e2e，typed独立left/right/rails_provenance | 不把typed声明称RGB识别；原typed新增三栏之外保持原值，实际适用性与执行coverage再核 |

20新增cases完整保留：8淘汰对照＋3独立证据＋9静止序列。无已证输入作者错误，不改r1-cases.json、typed-r1.json、expected-coverage或原oracle。原20顶层／69 cases＋22 supplemental、4层×89＝356规划layer-case、4schema/contact controls、1128coverage rows／2392展开引用均保留，实际执行0。未来局部负控制adapter可另存小型差异说明与新source SHA；不抄469436B r1-cases或224663B coverage到本R2D。

未来最小source改动清单限于：新root resolver／state-verifier共用入口；run的Result共享、nullable事实与nonthrow bounded cleanup／预开Pump handles；parent/child和configure/build/report消费同RootBinding；driver共用contact负控制拒绝链。原repair-runner.ps1、run.ps1、所有旧configure/batch保持冻结，不执行旧Replace脚本。不另造调度平台、不换compiler或family。若新冷验发现额外普通编码问题，保留失败、停止并交controller，不自行续修；若同合法输入核心观测期待被可信反例否决，才交核心反例，不能拿流程错误判family no-go。

## 8. 未来一次attempt的先决、停止及容量（提案，未授权）

先决是controller独立验收本R2D并**另发**新source／root／repair额度／可执行stage／唯一attempt授权；本页不会重置R1 1/1。新契约／机械拒绝案例／negative assertions差异先冻结，之后才局部实现，再冻结source/dependencies。保全原2120已核来源、503旧Git paths/dirty；工具链只核既有可用指纹，完整compiler/SDK/CRT/Add-Type closure不足仍明列，不能安装或修OS。

未来先做无产品native的局部path／state／故障模拟核验（亦须新授权），保存P/S/E/C所有实际结果，未跑不升格。然后三项process controls严格顺序、一项一次；预期owned-child拒绝已核才CONFIGURE_ARMED。最多一次新Release configure，可信后才standalone最小build及全部cold cases；Debug/ASan只在另列stage且可用／capacity／同gate成立时。并行≤2、每产品command≤300s、原stream绝对上限4MiB不增；52PNG audit、full replay、runtime/cost/stress、owner/backend、live等仍无授权。

hard stop：任何绑定／保护／容量／free／状态／receipt／SHA／identity／cleanup／streams未知或失败；controls不合predicate；原4I同型configure再次15s不自然zero；configure失败；build/test失败或可信核心反例。终止本attempt、保留已知与uncertain、后续native命令拒绝，不重试configure、不换root、不删失败、不第二次runner修补、不按成功子集报告。

容量以[总控controller receipt](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/controller-review/controller-receipt.json)的完整carry为基线。本R2D总新增≤1048576B（docs／设计JSON／维护脚本／收据／失败全算），从development56MiB剩51894033B扣，不是新额度。campaign含R1/controller-review，**不能把1223753B或607742B再加一次**。aggregate公式为当前完整campaign＋prior45307809＋O external72115/9546＋4R54056/11851＋4I104033/12289＋R1 external114127/10881＋R2D external实际bytes；controller新0、out新0。

未来建议预算（须总控核定）只在收尾余额内预留：development总新增≤40MiB（41943040B）、新out≤128MiB（134217728B），预留后仍保留原controller全部剩7164855B。合计最多183325623B，小于R2D前aggregate剩306271559B；本R2D即使满1MiB仍有121897360B余量。不能分别把development51894033B及out剩268060751B都用满，因为总和超过aggregate余量。

40MiB示意分配：3控制每stream≤64KiB合393216B；Release及可选Debug/ASan最多10命令（含一次Release wrong-contact-only）每stream≤1MiB合20971520B；三份report各≤4MiB合12582912B；必要新source/最小freeze与差异≤4MiB；全部state/receipts/负例≤64KiB report/failed artifacts≤1MiB；合39190528B，余2752512B仍计在40MiB内。report或source超过分配就STOP，不能偷偷复制旧大batch。这些是收紧保存预算，不放宽原4MiB／300s／15s条件；binary/dependency/probe output亦在128MiB内，全失败都计账。free仍须≥5704253440B，实际启动前再核，不拿本包快照保证未来空闲。

本包最终entry bytes、完整carry／剩余、当前artifact ledger与post保护见[final-receipt-corrected](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/final-receipt-corrected.json)。首次维护settlement漏计两份external docs32391B：PowerShell对OrderedDictionary用Measure-Object bytes返回null，转long成为0；独立最终核验exit1发现。原final-receipt／artifact-ledger／settle-design保持为失败版本，两份新docs当时版本亦保存于failed-settlement；完整失败分类和SHA见[maintenance failure](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/failed-settlement/failure.json)。修正维护改逐项加bytes并重算全部失败与external，不把原错误收据当最终容量。這未触及runner或原R1资料。维护核验失败不是BVI反例，设计案例执行仍0。

本包新增out0、旧数据删除0、PNG复制0；无commit/push／goal／automation／续派。完成交controller独立验收即停。

产品状态不变：Chapter Legacy全曲解锁IN与完整IN结算Miss=0尚未验收，章節分母／逐曲解锁unknown；HD解锁／回归，P/G/B照报，无AP前置。C36h tint1 baseline、main50 donor50/27/11/live0、suppression OFF、P excluded、X12 not-ready；无新增游戏改善证据。
