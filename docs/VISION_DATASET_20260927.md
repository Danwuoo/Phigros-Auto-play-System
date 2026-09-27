# 視覺分割資料準備（2026-09-27）

軟體 source `b32517d`。本輪交付原生ROI採樣、標註／mask rasterizer、validator、export與v75格式pilot；不啟動模型訓練、下載權重或上傳圖片。**training_ready=false**；200–400張經核對ROI／至少3獨立run仍是目標，不是已完成數量。

## 同源採樣

`--keep-diagnostic-anomalies` 升級 diagnostics3：最多2事件×4原生ROI＝8張，前1／current／後2；沒有前幀便減少張數，不能補成額外post。

獨立 `--keep-vision-dataset` 額度為16 clips×8張＝128 ROI，前2／current／後5，最長250ms，固定crop、640×384最大、1:1 RGB888。8難例、4正常、4不依賴detector命中的背景分層窗口。單時刻最多1 active clip，其他trigger drop計數。epoch／geometry／順序失效截斷，原始間隔與不足張數保留。

decision發布後從同一已消費Frame复制；2張前置full-frame是独立診斷副本，不供runtime決策，不保存capture lease。診斷與dataset共用預先配置arena；1280×720兩種opt-in共105,799,680 raw bytes。含16MiB候選bank、shadow與metadata allowance的runtime檢查仍≤128MiB。超過geometry預算拒絕配置，不自動擴容。

所有PNG／JSON編碼都在action stop／release與worker join後。每run256MiB、全資料根2GiB、最多10runs／1280 images；不自動刪舊檔。15秒flush deadline，partial manifest及truncated clips分開記；已有檔不覆寫。逐張RGB／WIC明示buffer上界與1ms process working-set採樣另記，sub-ms WIC暫態峰值仍unknown，不把RSS成長誤稱精確WIC allocation。

copy成本有獨立純記憶體A/B `dataset copy-bench`，3批、每種10,000更新、排除500暖機；synthetic uniform pixels只測複製，沒有遊戲真值。開啟sampling的p50／p95／p99／max（ms）逐批為 .12175／.263905／.623603／3.3665、.1065／.196805／.305910／.7997、.1342／.2002／.325111／.7517；關閉時p50為.0001／0／0，p95/p99各.0001，max .0769／.0001／.0001。不能直接以這個純memory差值歸因遊戲分數。實戰sampling與純性能run分開解讀，deadline與missing100／90／60ms不變。

```powershell
out/release-v145/Release/pas.exe run --config configs/phigros-hd-assist-lead35.json --mode assist --capability measurements/touch-cpp-full-run3/summary.json --duration-s 185 --no-preview --keep-diagnostic-anomalies --keep-vision-dataset --tracking-shadow byte_association
```

使用者手動選好Glaciaxion HD保留PLAY；C++只按當前pixels確認的PLAY。這個設定中真實遊戲仍由T0，不讓shadow輸出接到input。run summary列實際dataset path、copy／publication分布、shadow skip、bank retention。圖片資料根為 `measurements/vision-dataset-20260927/<run-id>/`。

## 標註與驗證

每sample：`image.png`、`annotation.json`；標註後有8-bit `semantic.png`、16-bit `instance.png`與C++ `overlay.png`。

| semantic | 類別 | instance |
| --- | --- | --- |
| 0 | 已確認background | 0 |
| 1 | visible judge line | 獨立line ID，附ROI centerline endpoints |
| 2 | tap | Note ID |
| 3／4／5 | Hold head／body／tail | 同Hold共用ID |
| 6／7 | drag／flick | Note ID |
| 8 | visible hit effect | 0 |
| 255 | 未標註／遮擋／不確定 | 65535 unknown |

polygon按pixel center rasterize，後polygon覆蓋可見上層，未標註預設ignore。每圖最多256 polygons、每polygon最多256 vertices。visible-only，不補未知head／tail，不做amodal mask。物件有visibility／truncated／type uncertainty，runtime identity不作ground truth。source／ROI PNG hash與run／clip／frame QPC／source geometry／1:1 crop及binary／config provenance保留。

validator檢查schema、來源PNG hash、shape／depth、語義與instance contract、objects對應、Hold部位類型、line endpoints，並**逐pixel重建polygon確認兩張mask**。run／clip不得跨split，crop／epoch／QPC順序一致；exact PNG hash与跨run dHash近似重複檢查，近似跨split先拒絕等待group review。dHash對低紋理可能過度保守，不當成標註真值；最多輸出256對細節與完整count。

`reviewed_human`需要reviewer；本task的視覺建議都是proposed，不冒稱人工gold。export只產生本機可查hash的index，不發送圖片、不啟動訓練。

```powershell
out/release-v145/Release/pas.exe dataset pilot-v75 measurements/game-assist/cpp-observe-17904849394349323 --output measurements/tracking-segmentation-20260927/v75-pilot
out/release-v145/Release/pas.exe dataset rasterize '<sample-folder>'
out/release-v145/Release/pas.exe dataset validate '<dataset-root>'
out/release-v145/Release/pas.exe dataset export '<dataset-root>' --output '<new-local-index.json>'
```

## v75 pilot實際資料

兩張既有原圖hash與原context核對後，各切640×384 ROI。**2 samples／1 run，非連續clip**；visible实例5（3個Hold body、2個line segment），人工覆核0。35157 labeled pixels、456363 ignore pixels；class0=15250、class1=636、class4=18891、class8=380。沒有Head／Tail／Tap／Drag／Flick覆蓋，不能當成完整訓練集。

overlay已視覺檢查，warm effect與未知邊界保留ignore；首圖body polygon縮小，避免吃到左側命中特效。QA valid、warnings為非獨立人工覆核；training_ready=false，兩張全部development。contact-sheet、mask及export index在桌面 `measurements/tracking-segmentation-20260927/v75-pilot/` 與 `v75-pilot-index.json`。

## 第一輪連續實戰資料

source／binary與三配置回歸見追蹤報告。`cpp-observe-17904941035800460` 使用已核對capability、5vCPU／8192MiB AVD、1280×720 rotation1／720×1280 touch90°，35ms／兩指／185秒。STOPPED／exit0，10,018消費frames、1,015遊戲commands；停止後result圖核對811,985分、345 Perfect／6 Good／0 Bad／42 Miss、ACC88.78%、MaxCombo51、Early0／Late6。未AP，也沒有把shadow接進真實input。曲中無build/test／第二capture。

18連續clips／136 ROI＝診斷2×4＋hard8×8＋normal4×8＋detector-independent背景4×8，1實戰run。背景分層是採樣來源，不宣稱每張像素全是background。trigger drop3、rejected frames212、truncated0、partial=false、sampling fault空。輸入停止後flush2.115秒、written8,971,945 bytes；raw arena105,799,680 bytes，working-set採樣210點／1ms，起140,541,952 bytes、sampled peak144,887,808／growth4,345,856；explicit encode scratch≤1,474,560 bytes，metadata allowance另列，不宣稱瞬時WIC精確峰值。

copy n10,018，p50／p95／p99／max=.2205／.309415／.401715／.9171ms；shadow publication=.0072／.0125／.0261／.2100ms，bank copy=.0003／.0011／.0018／.0381ms。capture n10,017=16.9658／37.1968／50.56524／795.0122ms；recognition n10,018=5.17925／7.343094／9.937592／17.0635ms；scheduler n1015=.062887／.58818／1.210101／4.1061ms；RPC n1015=.7257／1.03211／1.311566／2.2548ms。5次source expiry revoke與較長空窗存在，不能把分數差全歸因額外複製或tracker。

clip0 frame367–370有連續原圖：367的Hold仍清楚可見，368暖色效果遮到線；bank368零候選，369出現兩個Hold描述，370另有effect附近Drag候選與rails Hold。這證明候選覆蓋仍有缺口，但不提供逐Note judgment或所有描述的獨立真值。只標四張可確定body／可見水平line／小background，另標clip15首圖的可見gray body；頭尾及特效混合像素ignore。同clip proposed Hold ID1由可見連續rails指定，與runtimeID無關。C++ rasterize、逐pixel QA及overlay目視檢查通過。5 proposed samples、10逐sample實例、140430 labeled／1088370 ignore（僅已標5圖的分母），class0=50400／1=4120／4=85910；其餘131 unannotated，human review0，training_ready=false。已有gray／warm／截斷body格式例，尚缺完整Head／Tail及四類Note有效標註覆蓋。

原圖／sidecar／mask根：`C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/vision-dataset-20260927/cpp-observe-17904941035800460/`。QA、bank對照與停止後結算圖位於 `measurements/tracking-segmentation-20260927/`。PNG hash是lossless ROI檔案hash，不是假稱已保存整張source frame或另一個decoded RGB hash。

## 後續缺口

使用者已允許再取兩輪HD，待各輪PLAY就緒。現有第一輪連續原生ROI仍不足200–400經核對／至少3run目標；不假造1191–1196原圖。資料準備工具通過不等於灰色Hold已補辨，也不等於模型可訓練／AP。下一階段先獨立覆核新片段、補各類與負例、以run分組及去重，再決定模型候選與訓練條件。圖像目前本機單份保存，沒有宣稱舊4c3c ignored資料已恢復。
