# 視覺輔助與時間追蹤研究（2026-09-27）

後續決策：使用者已授權先做 ByteTrack／OC-SORT 思路追蹤對照，並同步準備小型分割資料。完整可執行規格、固定上游版本、採樣／標註與驗收見[開發計畫](TRACKING_SEGMENTATION_PLAN_20260927.md)。這是下一輪授權，不代表新 tracker 或模型已實作。

狀態：研究與下一步規格，尚未接入新的追蹤器、光流或學習模型。正式程式仍為 observer29／planner8／diagnostics2。使用者在 v75 實戰中指出判定線抖動等情況容易斷 Hold，希望加入輔助技術；本文件依當輪 pixels、日誌及現有實作區分已知證據與待驗假設。

## 當輪證據

桌面 main `81f29b0`，同一 Release binary SHA256 `cbb6685cbb11eac8967f9f52f7925f0d4417a331da9bb0ab396f9792afeaca96`。固定 35ms lead／兩指／185秒；run `measurements/game-assist/cpp-observe-17904849394349323` STOPPED／exit0。使用者選曲，C++ pixels 按 PLAY；曲中沒有 build/test、第二擷取或 CU 擷取。結算人工看圖核對為 Glaciaxion HD Lv.6：833,295分，356 Perfect／1 Good／0 Bad／36 Miss，MaxCombo65，Accuracy90.75%，Early0／Late1，尚未 AP。比 v73 的 790,229分、328／16／0／49 有改善，但本輪擷取分布也改變，不能把全部收益歸因於單項修正。

本輪 10,796消費frames／996 gameplay commands。capture interval n=10,795，p50／p95／p99／max=16.5381／32.85916／45.603072／441.9336ms；recognition n=10,796，4.2574／6.1051／7.80529／124.009ms。排程 lateness n=996，0.0627／0.598548／1.054419／1.737069ms；RPC n=996，0.6875／0.96495／1.223025／1.951ms。這些是本輪實测，不代表未來模型的延遲。

量測環境：Windows host、Emulator37.1.11.0的既有phigros AVD、gRPC payload fast／RGB888／top-down／256KiB、1280×720／rotation1；觸控720×1280／rotation90°／兩contacts。當前preflight核對guest processors=5、MemTotal=8,130,820kB；AVD設定8192MiB與host GPU，主機RTX3050 6GB／Intel Arc。配置、裝置指紋與guest量測原文在同一run manifest，後續模型比較須另記當時環境，不能只沿用本段。

兩張曲中圖由同一 capture 的有界診斷保留，input 停止後才編碼。結算圖另於停止後由一秒無輸入 C++ capture 保存。離線資料只供分析，不接真實觸控。

| 窗口 | 可核對的現象 | 界線 |
| --- | --- | --- |
| frame1022–1028 | 主線為 y=576，Note61 在1025消失、1026恢复；PNG1025仍可見到線 Hold 與命中特效 | intent7沒有在1025立刻 Up，故單幀消失不是這個接觸中斷的證據 |
| frame1191–1196 | 中央位置同時／交替描述為 Note87與92；1193主線從576偏到574.43687、次幀恢復576；1194起額外出現 x715垂直線候選 | 約1.56px主線估計變化與額外線候選存在；沒有逐幀原圖，不能斷言線實際抖動或兩個Note必定為同一物件 |
| intent11／Note87 | 最後接受frame1192；Down後約29.06ms Up；最後證據到Up約62.32ms。該ID之後在1196恢復，但已超過missing60ms | 時間符合missing取消路徑；原100ms source尚未到期。completed intent不可重播 |
| intent12／Note92 | 最後接受1195；其Down距intent11 Down約8.23ms，均在(639.5,576)，使用不同contacts；最後證據到Up約63.24ms | 須調查是否重複描述／身份切換或真實重疊，不以兩次RPC成功當作兩次命中 |
| PNG1208 | 灰色長Hold與特效可見，主線y=576.02083；現行單張 `analyze game-image` 找不到任何target | 明確存在可見物體與現行規則输出的落差；灰色、combo消失不提供逐Note判定真值 |

因此本輪支持優先改善物件辨識／身份連續性，同時量測判定線追蹤。不能把36 Miss全部歸因於線抖動，也不能只對線做低通濾波後宣稱已解決。現有程式已使用最多六個近期點的90ms相對距離擬合，並非完全逐幀點擊；缺口在觀測物件與線的穩定支持。

## 推進方向

### 1. 先建立可量測的觀測融合與身份追蹤

- 判定線使用獨立有界track與身份，記錄當前像素支持、中心／角度、innovation、候選拒絕及Note實際配線。以真實QPC間隔更新運動估計，區分量測噪聲與真正的移動／旋轉；不將真实動畫一律平滑掉。
- Hold使用頭部、尾部、兩側輪廓及當前body支持，而非單靠飽和顏色。內部分片、灰色body與被特效遮擋的頭部须在同一組實例假設中比較；不能因相近而合併相鄰Hold或吞掉重疊Tap。
- 一個沒有當前像素支持的預測不能單獨延長Hold。anchor90ms、source／target100ms、missing60ms及已完成intent不重播規則保留。增加的是有效觀測證據，不是放寬取消期限。
- 在同一capture的最新frame上運行。光流如需前幀，最多保存一張縮小灰階圖或有界ROI，與一張當前圖配對；不保留歷史frame佇列。物件／特徵數、ROI數及診斷圖數均設定固定上限，epoch／geometry／過期時清空。

OpenCV C++的KalmanFilter及稀疏Lucas–Kanade光流可作評估工具，沒有證實適用本遊戲。細線存在沿線方向的觀測歧義、大片純色缺少特徵、特效會污染光流；須以當前邊緣／輪廓支持與前後向一致性排除錯誤，光流不能代替物件分類。[官方tracking API](https://docs.opencv.org/4.10.0/dc/d6b/group__video__track.html)

### 2. 加入小型視覺模型的觀測候選

建議研究目標為小型像素分割模型：判定線、四類Note、Hold頭／body／tail與命中特效。模型輸出幾何與可信度，由時間追蹤及現有C++ owner／scheduler決定操作。通用預訓練物件模型沒有經過本遊戲類別驗收，不能直接當作可靠Note辨識；需先建立含灰色Hold、暖色特效、真實重疊、鄰近Hold、薄Tap與線移動的標註像素資料。

初期以shadow mode比較規則與模型輸出，不建立新觸控。評估使用同一像素輸入、同一上限與同一裝置；比較觀測缺失、身份切換、重複候選、假線、誤併及後處理p95／p99／max。若模型較穩定且端到端尾端延遲通過，再接入真實assist。

模型推論可透過ONNX Runtime C++留在單程序多執行緒；GPU provider與CPU的選擇須在RTX3050／Intel Arc／模擬器共同負載下實測，不預設GPU較快。[C++ API](https://onnxruntime.ai/docs/get-started/with-cpp.html)、[profiling](https://onnxruntime.ai/docs/performance/tune-performance/profiling-tools.html)

只有兩張異常PNG不足以訓練或驗收模型。下一個實作點是有界、指定失效窗口的診斷：最多保留兩個事件、每事件四張縮小ROI及配對時間／線／身份／intent摘要，單輪上限八張；從同一capture複製，在input停止後編碼。此為待實作的新診斷契約，當前diagnostics2仍只保存兩張單幀圖。先定位1191–1196這類身份切換的原始pixels，再決定追蹤回歸與模型標註；不以歌名、歌曲時間或Note序號觸發注入。

## 驗收與決策

1. 保留規則版作對照，先以pixels／fake QPC重現新的身份切換與灰色遮擋窗口。必須含真實線移動／旋轉、線噪聲、相鄰Hold、重疊Tap、來源停頓與停止釋放負例。
2. 追蹤／觀測接口記錄當前來源frame及證據年齡，不將純預測冒充當前觀測。身份上限與期限在短回歸可核對。
3. 標註資料只來自畫面，不含譜面／記憶體／音訊／預錄按鍵。訓練與驗證按獨立場景或run切分，不能把相鄰frame分到兩邊後宣稱泛化。
4. 新模型／tracker通過合成及離線驗證、三配置必要回歸後，才重新做固定35ms／兩指HD；實戰結算仍是驗收依據。首次可靠HD AP才進IN。
5. 擷取固定gRPC payload fast／256KiB。較好辨識不能抵銷不可控尾端延遲；每個候選另保存環境、樣本數與分布，不重開五擷取選型。

## 本機證據

- `measurements/hold-transition-20260927/live-v75-analysis.json`，raw SHA256 `d9533f1c244a78448994c9f60d48e8ed4cee40aed7908bd25149bb1a4201345b`。
- `v75-hold-windows.json`、`v75-first-combo-context.json`、`v75-hold-owner-windows.json`、`v75-combo-image-analysis.json`。
- 結算PNG `assist-v75-result/diagnostic.png` SHA256 `f02143e97cbc8ca7c90e3cbe067c5df310706598fe20d5e50ec2edeb946eaa6f`。
- run內Hold PNG SHA256 `71a08f064f7fed821dcc8f456755129dd7f285f4914cb426a381d4a31a85cb29`；combo PNG SHA256 `79bc66feba6d77489f158990886541bc41e3ae637c724ca5623cd17a0d6a33c7`。

這些ignored量測檔已保存在桌面工作區；Git文件不是量測檔備份。舊4c3c的程式快照1b7b05a已在main祖先中，本輪未恢復checkout；此核對不表示舊ignored實戰檔已恢復。
