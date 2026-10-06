# 新 task：完成實機測試前可本地完成的準備

使用者於2026-10-06明確要求派送新的GPT-6.1 Sol／xhigh task，接續下一步，完成
需要實機測試前的所有準備。本次授權必要本地程式整合、離線runtime／cost／stress、
回歸及本地commit；未授權任何裝置讀取、Fixture觸控、啟動emulator或遊戲操作。
沿用「可以不監控」：不建立automation、heartbeat或持續輪詢，不另派task／agent。

## 起点與工作位置

- project原根：`C:\Users\wurre\Desktop\Phigros-Auto-play-System`。main仍為
  `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`，有使用者AUX修改及未追蹤交接ZIP。
  此根僅核對／就地讀既有證據；不可reset、clean、刪除或覆寫。
- 唯一開發根：`C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`。
  已建立新分支`codex/prelive-preparation-20261006`，由已簽收
  `a55bf942e7d9fd6e63b715105544d1d6d065b469`分出；本派工文件另以commit保存。
  先核實cwd、branch、HEAD ancestry、dirty與所有適用AGENTS，不強制checkout抹進度。
- 先讀AGENTS要求的README／ARCHITECTURE／ROADMAP／跨曲研究，再完整讀
  `FRONT_CONTROLLER_ACCEPTANCE.md`、`FRONT_CANDIDATE.md`、`WITHDRAWAL_ACCEPTANCE.md`、
  Windows README、`docs/goals/`與round4 `CODEX_HANDOFF_PROMPT.md`、`VALIDATION.md`、
  `READINESS_AND_HANDOFF.md`、`COST_AND_LIMITS.md`。舊10/4「僅整理、不跑cost」是歷史
  範圍，本次使用者已授權新的必要離線開發及成本工作；裝置界線仍有效。

## 已驗收與仍缺項

原封v3 core在`c2dda1db9fe95b46ff103300d43583857130cd8f`，原356cases／3938assertions
仍有36fail，原54修18、新增原套件失敗0。不得刪測試、改oracle、用WILL_FAIL或放寬
期待來全綠；舊根349的timeout／skip、所有STOP與frozen inputs保留。

Windows正式observer→same-frame ROI／all-lines→v3→單owner／scheduler離線橋接已有
113項前綴契約。`e26231ec5acef9634df007770c1f06031d5326d4`終端量測56項三配置通過，
仍只是dispatch後的no-action sidecar，沒有接入正式動作流程。256張原PNG含859個
raw proposals、336個Hold：191個visible terminal proposed、145 unknown；eligible=0、
fake Down／Move／Up=0。原藍色predicate與真色有落差，需有證據的新契約。

frame3030 candidate2的當前邊界y=[236,237]、代表點(495.5,236.5)；使用者已確認約
(495,237)是可見Hold head前緣。該標註只在外部review，不能餵策略，也不是精確觸點
或合法Down gold。1519 Tap physical identity、合法機會分母及遊戲採納仍unknown。
前後封存index／原PNG與metadata SHA、linked source/binary／配置閉包先核；舊成本
數字只作定位，沒有有效最新完整鏈A/A→A/B／stress資格。

## 必須推進的工作包

先建立能逐項裁決的readiness矩陣，每項列依賴、方法、已跑／失敗／未跑、收據及是否
只能由裝置回答。持續完成以下所有能在現有本地資料／權限內完成的工作，不在一個小
sidecar、文件清單或一筆unknown出現時結束整包。

1. **當前pixels契約與物理語義缺口。** 就地核既有原PNG與有界連續窗，以線局部
   座標、RGB／rails／inside-outside／遮擋／effect證據研究可解釋的通用front／body
   契約。先定假說、真圖正反支持與停止條件；不按歌／ordinal調參，不以一張3030
   過擬合所有色彩，不重置舊調參預算。需要改predicate／ownership时另立新命名候選
   和source freeze，原v3、front候選與原oracle保持可重跑。無法裁決者保留unknown，
   製作最小review圖卡／問題，不以更多合成宣稱解决，也不先要求整包錄影重傳。

2. **必要typed整合與完整自身接觸前綴。** 在新候選支線實作需要的正式pipeline
   adapter／runtime hook，預設不准真注入，離線必須可證無裝置endpoint。清楚分開
   current measurement、身份、note→line、prediction、intent與injection；front
   sidecar的proposed不自動取得Down資格。原圖前綴从round reset或可核checkpoint
   開始，由候選自己的owner receipts建立contact，不seed舊按鍵、不手填known_down。
   分開驗Hold接入／body續接Move／tail結束，旋轉且按住、接近才對齊、相鄰／duplicate、
   線消失／重接、多線、五指上限、completed不復活、unknown Down不重試、超時與
   釋放失敗。Fake-clock／合成正反例可補契約組合，實際遊戲採納仍另列未驗。

3. **選定exact候選的Windows全鏈成本與安全。** 先凍結功能／source／binary／
   編譯flags／profile／依賴／輸入與全分母；不同source要freshly build相符閉包，
   不因舊lib SHA相符便與新header／正式邏輯混用。沿既有正式latest-frame mailbox、
   observer、typed bridge、owner、FakeTouch／transport stub與Journal，實量主機QPC
   complete chain，不拿leaf／雲端耗時或stage p99相加代替。覆蓋慢消費、skip／drop、
   不規則dt、過期／revision／取消、slow／failed／unknown RPC、writer滿載、
   16線／128候選容量、shutdown／釋放與fault。真RPC可先用明示offline stub，真driver／
   capture成本標待實機，禁止偷偷連裝置補值。
   依當前適用C5／R1–R3 gate追來源與threshold，不能複製不同source舊容忍。
   同機同corpus先baseline A/A至少三批，候選結果前凍結噪聲，再平衡A/B／B/A至少三批；
   冷成本每必要場景／mode至少1000attempts，長跑至少10000frames或10分鐘fake業務
   時間（另報wall time），有界RSS／pool／queue／writer帳。只做對裁決必要的批次，
   不無限跑。列全分母n、p50／p95／p99／max、jitter、失敗／reject／unknown／
   late／expiry／drop；丟難例、全拒絕或縮覆蓋換成本不算通過。未達門檻如實NOT_READY。

4. **回歸與交付閉包。** 全新build roots完成變更涉及的Release／Debug／MSVC ASan，
   使用原獨立tests／oracle與必要新控件；根／BVI原套件依實際受影響閉包重跑，未改
   source且已核同配置證據者可以引用但須明示未重跑。所有原36fail逐類保留，若有修正
   要在同oracle完整差異下解釋，新增退化不得藏。保存before／after SHA、binary／DLL／
   lib bindings、freeze、params、stdout/stderr、native／runner／verifier、完整結果与
   source版本。做有界證據封存與重跑入口，更新readiness及repo狀態，做本地commit。

5. **實機前可審查操作包。** 準備exact候選manifest、profile/capability離線指紋核對、
   五contact與停止／釋放checklist、有限測試的目的與結果格式、Chapter Legacy逐曲
   分母／IN解鎖採證表，以及待授權命令。指定裝置serial若目前只有historical記錄要
   明標歷史未再驗；未知則填明示待指定，不能猜。每條裝置讀取／Fixture／manual-session
   命令列目的、風險、停止條件、source/binary/環境前提與容量，預設不執行。遊戲由使用者
   選曲及按Play，一次一輪，無auto-start／選曲／連打／重試。不得承諾過了冷測就實機ready。

## 安全、資源與完成界線

所有自有正式分析／策略／統計為C++20，維護shell／CMake／JSON可用；不讀譜、memory／
內部狀態、存檔、音訊節拍、歌曲身分／進度或future frames決策，diagnostics單向。
單owner、五contact、bounded recent state、QPC與latest-only保持；unknown注入不重試
Down，fault／unknown release保存並停止／核釋放。

沿`tools/zero_miss_windows`的qualified process-control核心；native exit0或有JSON
不是pass。每job用新stage／root／report，核Job assignment、身份／held handles、
active_final=0、streams、deadline與完整verification。遇member-image:5先按收據
核收尾，保留STOP，僅另開fresh attempt；不taskkill猜PID、不跳guard、不重開舊STOP。
不要為完成研究建立通用無限恢復平台。

先核當前Windows工具鏈、既有vcpkg／ASan support及磁碟，再凍結本包容量：新OUT
≤12GiB、metadata≤64MiB、最低free20GiB；逐階段核實，歷史分配分開保留。優先複用
已核且source／配置相符的依賴／靜態閉包，manifest install OFF、Torch OFF，沒有付費
運算或系統安全變更。原PNG／binary／依賴主要列external hashes，不任意全量複製。

不push、PR、merge，不啟動裝置、emulator、Fixture、遊戲或模型訓練，不建立系統goal／
automation，不另派agent／task或擅自message其他thread。小修補／離線測試／本地commit
持續做，不每步再索許可；新的裝置權限留待使用者。

**完成標準：所有現有證據／權限下可完成的實作、必要回歸、冷成本／安全與操作準備
均有可核結果。** 真圖或裝置才能回答的剩餘項目須逐項列最小缺件及為何不能離線裁決，
其餘獨立工作繼續完成。全拒絕、合成gold、冷測全綠、只列TODO或打包文件不算整合成功；
成本失敗如實保留，不宣稱所有gate已通過。

首次以繁體中文回報實際cwd／HEAD／branch／dirty、SHA核對、readiness矩陣与先做三步。
之後以可核里程碑回報，最終列本地commits／source與binary SHA／收據入口，已跑、失敗、
未跑及精確授權／語義門檻。完成後在此新task回報供總控獨立驗收，不等待裝置連線。
最終Chapter Legacy同一凍結候選全曲IN解鎖與完整Miss=0目标仍未完成；不用AP、不同
版本最佳成績拼湊或新增「每曲三連勝」門檻。
