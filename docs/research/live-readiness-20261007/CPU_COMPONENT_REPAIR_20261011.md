# CPU component 修補與啟動前軟體工作

使用者最新指示仍要求完成所有可冷完成的開發及測試；修補後不重跑原75組及
production9 PASS，完成後不建立新 thread。本輪不啟動 emulator／device／Computer Use。

main45／`a15986f` fresh32單一build transaction觸及原held-identity-cap64而STOP，
runner125／native exit未知，31／32 target objects及部分link產品保存；held及streams完成、
active0。完整945 source copies與原STOP見[負結果](MAIN45_BUILD_NEGATIVE_20261011.json)。
下一候選將CPU build拆為core／bridge及主harness／其餘controls三個owned transactions；
正式production拆core／action／emulator／bench／entry／pipeline六個transactions。
總32／52 TUs、十個CPU targets、原64 held／time／stream caps與所有門檻不變；
每批有真實Native8、來源及before／after closure，後批封存先批已完成object／library。
不沿用main45部分產品，不將拆批或組件PASS當整體資格。

main46／`7b898c4` configure native0及完整before／after closure通過，第一build在
InputClosure正規化時拒絕empty path而PRELAUNCH STOP，沒有build Native啟動或target
objects。原因為初始空prior receipts／closures／processes的`.path`展開產生null；
main47用逐entry產生path array，空prior明確為零條，保留generic empty path拒絕。
原945 source snapshot及configure Native8見[main46負結果](MAIN46_BUILD_PRELAUNCH_NEGATIVE_20261011.json)。

main47／`87af239` CPU32三owned build groups及七controls通過，reader2／81 controls
通過；實際readonly核base／prefix／faults／原production及current narrow source／native
joins通過，只因AA／noise／ABBA尚缺而NOT_READY。production在第一次Native前載入
兩組owner control source array時STOP：PS未加括號的字串串接吞入逗號array，六項變成
一個長字串。main48明列第一項括號及逐source manifest lookup；修後隔離readonly loader
完成不授新資格。原945 snapshot、CPU／reader／readonly及負結果見[main47記錄](MAIN47_PRODUCTION_LOAD_NEGATIVE_20261011.json)。

main44（source `79f8f37`）AA12 validity 全通過，producer／publication 全部零超期；
2ms active tail 仍未使 noise 合格（12不足、owner6 adequate），ABBA 未跑。
實際 readonly reader 核對前置、候選、AA及18條公式，結果仍為 NOT_READY。
完整負結果與12個 lossless journal relocation proof 見
[MAIN44 negative](MAIN44_NOISE_NEGATIVE_20261011.json)，不重跑該 binary 挑選有利結果。

下一候選保留 full RGB FNV seed、逐 byte 值與順序，使用固定512KiB純算法常數表
加速實際讀到的 uniform-byte8 區塊；非uniform區塊保持原 FNV8。每列checkpoint、
probe分母、padding排除及中止位置保持。observer 只對固定8個抽樣像素覆蓋的48
RGB bytes全部≤165區塊直接填零mask，其餘照原分類；BFS、累加順序及動作策略保持。
兩項改動須與保留scalar／donor oracle逐位元／逐mask及完整decision對照；效益待測。

來源契約新增明確 CPU delta 分支：fresh `pas_core`完整22 TU及 `own_bridge` 2 TU；
其餘 `current_rails`、`current_front`、`exact_bvi_v3` 三原library沿用exact pins。
五個原library只保留為歷史前提。新的 `linked_static_inputs` 必須是三原library＋
兩fresh library，`rebuilt_static_inputs` 只兩fresh library；不得將原core／bridge
偷偷連入。四harness TU＋四affected control TU，合計32 fresh TU／10 targets。
實際Ninja compile／archive／link graph、object、library、native before／after closure、
完整source及control report／binary均須銜接。原75／prefix3063／fault4／production9
只是保留前提，不能寫成新正式source的新全配置qualification。

validator 的收據讀取、完整核對、current source／executable／profile joins、逐理由拒絕、
CLI接線與score-only入口屬可冷完成軟體，不因缺實機收據而延後。synthetic fixtures
只能驗證軟體契約，不能建立live admission。最後還須fresh正式action library與`pas.exe`
及generated runtime provenance，跑受影響的拒絕／score-only／release控制；正式source／
action／入口已修改，因此僅fresh normal Tap／Hold／Dense三項原數值gate，沿既有pipeline
v3／auditor v5完整raw與native收據核對。不重跑原production9的stress／Debug／ASan項。
研究
`current_chain.exe`的成本通過不能直接授權舊production binary。

所有待測候選目前均未取得新cost或live資格。R2 cadence／分母／noise公式／ABBA門檻、
48GiB retained與20GiB free等界限保持；完整noise合格後才跑原未執行的ABBA36。
真正裝置／profile／capture／physical收據須在啟動裝置邊界後取得，不能由冷fixture代替。

可冷完成的收據 producer 也納入本輪：正式 `pas_emulator` 6 TU，含 current admission
與 receipt collectors，CLI 綁定当前 binary／source／profile／capability。新正式 build
連同七項受影響的 control executions 共52 fresh TU；collector 有77項純冷控制，
admission 有82項、actual closed entry 13項，None lifecycle 保留 invalid／Tap／Hold／Dense。
裝置收集採既有 OwnedV3 process／transaction／wait 控制，啟動前封存實際 device scope；
舊 offline 收據與 process guard 不改寫。此處只實作工具及冷拒絕控制，不執行裝置收集。

Capture collector 沿原15秒 ready budget、10秒 warmup、60秒 measurement窗口及完整
notification 分母。既有 residence／no-frame／coverage 警告保持 diagnostic，不能把
production2560／Tap-Hold-Down-Up 或三曲 formula 套到 menu capture 形成循環門檻。
Physical 的 mapping／rotation／release Fixture 與實際 current owner 的有限 None session
分開；Fixture不升格為 Phigros gold。獨立 pixel review 只能對自然收尾後的 current owner
結果作判定，缺失或 unknown 維持 NOT_READY。資格 metadata 有界，不保存 gameplay
pixels、錄屏、journal 或 trace；後續 Legacy IN 一轮的使用者紀錄仍只有分數。

已完成的 component-only memory benchmark 只是診斷。固定 scalar／donor oracle 的
逐mask與decision控制359項及 uniform FNV19項通過；全RGB200的fallback診斷反而較慢
（median 1.3353→2.1827ms），原負結果保留，不能以 uniform 場景的改善代替正式成本收據。
