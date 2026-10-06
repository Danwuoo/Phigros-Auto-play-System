# 2026-10-06 Windows 接手與離線橋接里程碑

最新本地冷準備交付見 [PRELIVE_RESULTS](PRELIVE_RESULTS.md)：current-rails-v1 typed hook
已接正式離線 owner／scheduler，三配置113／56／113／68／186項均通過，3063幀完整自身
receipt前綴有效。A/A在第5筆Hold無動作時停止，A/B未跑，成本與physical gate仍NOT_READY；
沒有裝置操作，當前Chapter Legacy分母／IN解鎖與完整Miss=0仍unknown／未完成。
收據入口為 [最終SHA索引](prelive-evidence-01/SHA256_INDEX_FINAL.json)，另附
[重跑方式](PRELIVE_REPRODUCTION.md)、[待授權操作包](PRELIVE_OPERATION_PACKET.md)與
[逐曲採證模板](PRELIVE_RESULT_TEMPLATE.json)。候選只有offline entry，未接live CLI。
本輪後續push與原main root的近期ignored ZIP另見
[封包範圍／還原方式／收據](PRELIVE_IGNORED_PACKAGE.md)。

以下為先前Windows交接／sidecar驗收的歷史里程碑，封存與負結果維持原件。

**產品目標未完成；實機與成本 gate 仍 NOT_READY。** 本輪完成交接核驗、Windows
離線建置，以及隔離研究候選的 current ROI／all-lines → 正式 owner／scheduler
橋接。凍結 v3 的七份 core source、正式 runtime、原測試與 oracle 均保留。
沒有裝置讀取、Fixture／遊戲操作、模型工作、push、PR、merge 或新長期 Goal。

前一包總控獨立驗收見 [FRONT_CONTROLLER_ACCEPTANCE](FRONT_CONTROLLER_ACCEPTANCE.md)：
全新 Release 重建，三配置重跑56項量測／113項前綴，256幀原圖audit與三個竄改拒絕；
簽收隔離離線幾何量測。總控新增1 STOP保留並以新attempt成功重跑；eligible仍0，
正式採納／完整成本及實機未驗收。交付封存與原source均未覆寫。

隔離 Hold 終端候選交付見 [FRONT_CANDIDATE](FRONT_CANDIDATE.md)：
source `e26231ec5acef9634df007770c1f06031d5326d4`，三配置56項新量測、113項未改前綴與
256幀獨立原圖audit；191個可見終端仍proposed，完整成本及physical採納尚未驗收。
三個新STOP、全部attempt與封存SHA保留。

前一包自行開發與派送撤回驗收見 [WITHDRAWAL_ACCEPTANCE](WITHDRAWAL_ACCEPTANCE.md)：
source `774f3e7020818e94c508cc1481dd0986ebe417a0`，三配置113項前綴、32項診斷、
256幀／1,665原圖取樣獨立核驗與兩個竄改拒絕控制；初次程序 STOP 保留。
frame3030已有一筆使用者確認的可見 head 前緣語義，1519身分及接觸採納仍 unknown。
下列是原接手里程碑的歷史結果，舊封存不覆寫。

原橋接 source commit：`29e3ed09ae176a93a159f3b7993c8047505e9482`。
前一個 checkpoint `0f81a387676ec67b470f537812113cbe33df8fa0` 的 binary、108 項前綴、
真圖與 source SHA 原件保留；最後修補只明確拒絕凍結 profile 以外的 rotation。
配套實作：[research/bvi_windows](../../../../research/bvi_windows/README.md)。
本文件與證據另以本地文件 commit 留存；產品狀態見
[目標](../../../goals/LEGACY_IN_ZERO_MISS_GOAL.md)。

## 交接與 checkout

- 實際原根為 `C:\Users\wurre\Desktop\Phigros-Auto-play-System`；原 main HEAD
  `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`，使用者修改的
  `apps/runtime_x11_p/aux/CMakeLists.txt` 及未追蹤 ZIP 留在原處。
- ZIP 5,187,688 bytes，SHA256
  `18040114639c560c0ed33d6d7949affd1a4fb1ce2be807adf1f53b948656ee9b`。
  1,461 entries 在解壓前通過路徑包含、大小／數量、Windows 保留名、大小寫重複與
  symlink／reparse 檢查；新解壓根為同層的
  `Phigros-handoff-round4-9dc99d6-20261006`。1,374 個 SHA256SUMS 項目全部匹配。
- 先完整讀 CODEX_HANDOFF_PROMPT，再讀同輪 README／VALIDATION／READINESS、
  docs/goals，以及本機 AGENTS／README／架構／路線圖／跨曲研究。
- 新 worktree `C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`，分支
  `codex/zero-miss-round4-windows-20261006`。只套
  `cumulative-from-74e5443.patch`；沒有再套 incremental。
  重建 delivery commit `18965192f43e47ce9d5f6a2d8ad372b4bdb0507b` 的 tree 精確等於
  `6a4a9950423f0775992780b65b96ce8dcf2bb98e`；不靠短 commit ID 認定相同。
- `aux` 為 Windows 保留名。新 sparse checkout 只省略該工作目錄，Git tree 仍完整。
  索引初始化曾使用單次 `git -c core.protectNTFS=false read-tree`；沒有改持久 Git
  或系統安全設定。原檔透過 extended path 核 SHA，仍為
  `e5c78a1d90a5e430c4f7c5328c7e9a8b3571800b0b8fc698d082b1cbe16d5501`。
- 新匯入研究檔採交付 LF bytes，避免 autocrlf 改變凍結 core SHA。舊 Windows driver／
  main 保留既有 CRLF，只在獨立 IO 副本使用交付 typed-singleton patch。
  第六份 supplemental 從不可變 Git blob 匯出成雲端 LF SHA；原 CRLF 檔沒有覆寫。

完整 delivery／source／原 dirty 收據見
[handoff audit](evidence/receipts/handoff-audit-01/verification.json)。

## 本機環境及程序門檻

Windows 11 Pro 10.0.26200，Core Ultra 5 125H（14 核／18 logical）、約 31.7 GiB RAM。
MSVC 19.51.36256／tools 14.51.36231、SDK 10.0.26100.0、CMake 4.2.1、Ninja。
BuildTools 安裝器登錄仍標 incomplete；實際 C++20 compile/link 已核。
ASan 支援來自既有 Community 14.50.35717，來源／副本逐檔核 SHA。

VCPKG_ROOT 未設定，明示使用原 checkout 的 `out/vcpkg_installed`，manifest install OFF。
baseline `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb` 與 gRPC 1.81.1／256 KiB
read-chunk overlay 保留；JSON 3.12.0、protobuf 6.33.4／protoc 33.4、CLI11 2.7.2、
GTest 1.18、FFmpeg 9.0.2、zlib 1.3.2、OpenSSL 3.6.4 使用本機既有依賴。
獨立 BVI 使用交付兩份 JSON patch，最終單檔 SHA 為
`f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`；
正式 core 仍用既有 modular JSON，不混稱同一檔案。

沿用原封 owned／identity／transaction 安全核心。新 file-only pretest 20/20，
actual CreateProcess=0；native 0／7、owned-child 125 負控制及 wrapper／工具鏈 probe
共四控制收據通過。舊 STOP 未重開。
VS generator 的初次 configure 因 vctip／mspdb 子程序不自然退出而 STOP；改用
逐檔相同的 compiler snapshot（省略 vctip.exe）與 Ninja／Embedded `/Z7`。
原安裝樹未變，未知子程序身分仍立即失敗，沒有忽略 conhost／access-denied。

## 已跑與保留失敗

| 套件 | Release | Debug | ASan | 限制 |
|---|---|---|---|---|
| 正式 root CTest 349 | 347 pass、2 skip | 346 pass、2 skip、1 timeout | 346 pass、2 skip、1 timeout | 不把 skip 當 pass |
| 1,000 dense frames 專項 | — | 原同 binary 單例 pass，128.24 s | 原同 binary 單例 pass，345.45 s | 原 CTest timeout 仍保留 |
| v3 八個 contract／independent targets | 8/8 | 8/8 | 8/8 | 339／79／74／167／16／101，另 5 controls |
| 原 356 layer-cases／3938 assertions | 36 fail | 36 fail | 36 fail | failed_rows 與雲端精確相同，新增原套件失敗 0 |
| 正式 owner／scheduler 前綴 | 113/113 | 113/113 | 113/113 | 候選自己建立 fake receipts；不是遊戲採納 |
| 同一組真圖短窗 | 256/256 | 256/256 | 256/256 | 全部零 eligible targets／零 own fake Down |

凍結舊套件維持 native=1、runner=1、verification=0，`aggregate_pass=false`；
wrong-contact 消費者負控制也維持拒絕。36 fail 包含 effect 2、capacity 24、
raw-body 8、raw-hit-order 2，解釋仍沿用
[round4 VALIDATION](../../zero-miss-20261005/round4/VALIDATION.md)。
新合法橋接能拒絕完整超限 batch，不代表把舊 adapter 的 capacity failures 修成通過。

最初 Windows 1 MiB stack 的 Debug／ASan 各有兩 fixture 失敗；ASan 明報 stack overflow。
另開 8 MiB stack 的**獨立冷 fixture host**後八 target 通過。這沒有修改正式 runtime stack。
MSVC 預設 stack 說明見 [Microsoft /STACK 文件](https://learn.microsoft.com/en-us/cpp/build/reference/stack-stack-allocations?view=msvc-170)。
沒有宣稱 Windows UBSan 或 leak check 通過。

Debug／ASan root 的 timeout 來自本次 runner 的 60 s 牆鐘限制，不是原 oracle。
兩個專項以 900 s owned deadline 跑各自同一 GTest filter，原 1,000 幀、全部斷言與 fake-clock
業務期限沒有更改；原 349 項 CTest 的 timeout 不抹除、不改成 aggregate green。
三配置的兩個 skip 為 opt-in 的 recorded RGB clip 回歸（本次未設定 `PAS_RGB_CLIP_ROOT`），
及 fresh build root 未帶入 ignored 舊圖的 `ManualUiPixels.ExistingResultAndMenuEvidenceOfflineOnly`。
兩者都列未跑；另做的 256 張新橋接回歸不冒稱補跑這兩個原套件案例。

其他失敗亦全部留存：pretest 初次 sharing violation；建置期間修改 invoke.ps1
導致的 source-input-sha STOP；初版前綴 fixture 的 held-support 宣告缺失；拒絕覆寫舊
report；legacy／ASan 的 timeout 與退出時身分不明。ASan root configure 的 argv
組裝錯誤、project() 前 runtime 庫尚未複製及 CMake flags 反斜線問題均使用新 root
定位，原錯誤收據不覆寫。偶發退出身分競態尚未修安全核心，依然 fail closed。

## 新橋接及真圖缺口

借用同一 RGB frame，嚴核 frame／batch／scene context、geometry、時效與容量。
最多 128 queries／16 current lines；所有量測線包含 association-invalid 競爭者，
不挑線製造唯一性。Hold depth 由當前 tail 沿 Note normal 投影，沒有 tail 不冒稱已見尾。
每幀 ROI ID 只作側表，經唯一當前 geometry 配對 note_id，再接正式 intent／revision／contact。
body patch 不升格成 seen front。v3 只供 Tap／Hold current 支持，單一正式 owner 負責注入。

ExecutionLedger 的 known_down 只來自自己已接受 plan 與成功 Down receipt。
cursor／prefix_offset 保留 uint64／optional，缺 cursor 不變零；unknown Down／Move／Up
停止、completed 不復活、五 contact、旋轉 Move、prefix compaction／截短拒絕、expiry、
錯 contact、未知釋放及跨 context 失效都有原正式 owner／scheduler 的實跑控制。
極大 uint64 是轉型控制，不是宣稱執行數十億 Move。不同 context 保持拒絕，直到
另建立經釋放核驗的新 owner／ledger；取消 pending 的 rebirth 尚保守拒絕。
橋接明確限定 1280×720／RGB888／stride 3840／rotation=1。最後 5 項新增回歸
驗證一致但未支援的 rotation 仍拒絕，以及 active contact 不可沿用 Down、立即核實
釋放、不再執行。三配置都在全新 `bridge-win-profile-*-01` 建置及核驗；沒有覆寫
舊 binary。正式來源與 v3 未變，本次只重跑有關橋接／真圖，未重跑不變的原套件。

本地 C36g Dlyrotz IN full-recording 原 index SHA
`0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`，分母 7,722 PNG。
按事先指定 ordinal 480／1500／3000／5000 各連續 64 張，256 張共 76,971,667 bytes
在 native stage 前後逐張核 SHA。只保有一張 current RGB，短窗間核實釋放後重置；
沒有歷史按鍵 seed，也不宣稱還原目標窗開始前已有的 physical Hold。
候選、正式 observer 的短期狀態及自己的 receipts 由每個窗起點重新建立。

真圖結果：859 個 current proposals、604 個 BVI queries（Tap 357／Hold 247）、
848 個原 tracker targets；255 unsupported、12 unmapped、244/256 幀允許 playing gate。
peak lines=6、ROI=7、probes=924,527、BVI metadata=236,216 bytes；invalid frames=0。
Tap 的 body=absent 是既有設計；247 個 Hold 皆沒有 body 支持。
只有 2 個 contact supported，兩者 relation 不 usable。合法物件人工分母 unknown，
eligible=0 不代表遊戲中沒有合法機會，也不能簽全接觸前綴或 active-owner 成本。

診斷側表保留全 current proposals、origin／quality／head／body／hint 與所有量測線旗標。
frame 3030 的 query 2（front 495.5,228，depth 78）內側實測 RGB `[192,226,237]`／
`[193,227,238]`，兩 rail 為 `[255,255,255]`；front 外側 +3 仍為 `[156,233,255]`。
凍結 blue predicate 要求 R=20–100、G=130–230、B=180–255 且 B≥G+20。
這是可核顏色／前端語義落差，**不是人工 gold，也未改 predicate**。
frame 1519 的兩個 Tap proposal 位於同一 effect 覆蓋區，只見 contact、不見 front；
不由舊成功按鍵或畫面 effect 推定候選可以 Down。

歷史 five-contact capability 原件也在本機，SHA
`73dc5c94f1418a1e75bf4488871d27c849e0d6105adc79ec03c167ff672dbcd3` 與 recording manifest 相符。
其中 max_contacts_verified=5、serial=emulator-5554、APK SHA
`b3bfaf4e4e876e7833c9af1bd6149b40cdc9fa46b67646d52ea045dde6705dc6`；
這只核歷史來源，不代表當前裝置／遊戲指紋已核。

## Windows 成本與下一個必要門檻

最後 source／binary 的 Release 順序 PNG replay，所有 256 幀（含拒絕）之完整
observer＋bridge＋owner／scheduler／receipts 計算：p50 7.2014、p95 12.631025、
p99 15.43637、max 19.5908 ms，jitter(p95−p5)=6.589525 ms。
QPC 為當前 host；業務 clock 另列為 archived QPC 的 offline FakeClock，來源絕對年齡 unknown。
診斷序列化／flush、最終 release、PNG IO 另列，不把 stage p99 相加。
前一 source 的 p99 13.055215 ms，及更早 attempt 的 14.31204／31.47459 ms
與全部原收據仍保留；不同 adapter／診斷
source 與負載不能拼作 A/A 或只挑較快結果。ASan／Debug timing 不作即時成本資格。
loaded module paths 有界列出，SHA 是**事後 backing file**，不冒稱 mapped image hash。

未跑／未驗收：正式 BVI runtime hook、latest-frame producer／RPC／journal 壓力、
有效 A/A noise／A/B／stress、同候選真實完整 contact prefix／Hold body continuation／
tail 採納、當前 profile／capability／版本／Chapter Legacy 完整分母／IN 解鎖與逐曲結果。
offline harness 用 Tap／Hold、lead=0 ms；沒有冒稱正式 lead35／全 note types profile。

下一步先針對本地 frame 3030 的實際 Hold front／gradient／rails 與 frame 1519 的
effect／Tap proposal 做可追溯語義 review，保留 proposed／unknown；不先索取整包錄影。
要修 predicate／typed front 表示時另 freeze 候選與保留原 oracle／v3 失敗。
合法機會與完整接觸語義成立後，才凍結含 latest-frame／journal 的完整成本方法。
需要裝置讀取或操作時另列指定裝置、命令、目的、風險與停止條件取得授權；
目前沒有提出可開遊戲的候選。最終全曲驗收仍要求當前分母、各 IN 已解鎖、
同一凍結候選逐曲完整 IN Miss=0；P/G/B／score 照報，不加 AP。

原始結果、失敗、STOP、source／binary／容量／SHA 索引隨本目錄 evidence 留存。
入口：[最終 SHA 索引](evidence/SHA256_INDEX_FINAL.json)、
[原 checkpoint 全部 attempt](evidence/stage-index.json)、
[最終 attempt／編譯身分](evidence/final-profile-01/source-and-binary-sha.json)、
[最終核對與容量](evidence/final-profile-01/final-verification.json)、
[最終 Release 真圖](evidence/final-profile-01/receipts/pixels-profile-release-01.json)、
[113 前綴](evidence/final-profile-01/receipts/prefix-profile-release-01.json)、
[原 36 fail](evidence/receipts/legacy-release-r3-01-inputs/legacy-summary.json)。
原 [SHA 索引](evidence/SHA256_INDEX.json) 仍原封保存；最終索引涵蓋原 checkpoint
及本次 rotation 修補的追加收據。封存不含 PNG、binary 或依賴。
最後核對保留原 71 個 STOP、123 個 checkpoint binary、15 份舊 source snapshot；
2,111 個 metadata 檔列入最終 SHA 索引，封存約 45.55 MB（硬上限 64 MiB）。
本次新增 out 累積約 11.31 GB（硬上限 12 GiB），沒有複製原 7,722 張錄影或清除失敗 root。
PNG、binary、依賴留在原本機位置，沒有納入 Git 或另複製整包。
