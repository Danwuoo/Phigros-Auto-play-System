# 完整曲庫面板、AT 與接點預算

2026-10-25 依使用者要求整合library全部歌曲、處理身份／AT、解除獨立2000 notes上限；其後明確允許先跳過格式不相容的項目。這是逐譜離線準備與人工實測入口，沒有宣稱全庫皆可實跑或AP。

## 完整清單

| 範圍 | 數量 |
| --- | ---: |
| 歌曲／譜面 | 327／389 |
| IN／AT（含獨立變體） | 333／56 |
| 最終binary離線前檢通過 | **263** |
| 其中IN／AT | **241／22** |
| 列明原因、保留在列表並跳過 | **126** |

同一操作面板可搜尋曲名／ID、篩選IN／AT與可前檢／跳過；變體分列，跳過列不能勾選。「全選可測」勾選目前顯示且備妥的列；原有勾選可用「清除」重設。要單獨試曲，選取該列再按「只測本首」。選曲、READY後Play、下一首仍由使用者操作，不自動選曲、retry或提交分數。

126份跳過原因：46份超過5指、24份無法投影、22份Flick沒有可辨識位移、3份有兩種路徑錯誤、11份完整事件／時間／首音等預算未通過、10份blockAreaList未支援、5份v1及5份安裝版／公共版本不同。沒有刪音符、忽略判定區域或放寬容量來湊通過。Credits AT（638 notes）在22份可前檢AT之中；可前檢不等於遊戲判定已驗證。

## 身份與AT

使用既有授權4.0.1／157四APK，按精確Addressables地址、container及原始TextAsset切片核對，再與UnityPy typed reader及公共chart SHA交叉核對。389份都有安裝版地址，**384份原譜bytes相同**，原本21首亦全數相同。5份不同SHA為Desultory Signals AT、Petrichor AT、What do you want more than a Happy ending AT、ハテ IN／AT，保留兩份身份並跳過，不強制別名。

新`pas.installed-catalog-song-source.v1`記錄APK／catalog／container／TextAsset及native FSB5 resource SHA。音源只在記憶體中讀取／雜湊，沒有為327首再製WAV／FSB副本；hash明確是FSB5身份，不冒充解碼WAV或PCM。profile綁定source manifest SHA與檔案，面板固定APK、來源與profile；原21首保留各自原WAV身份與設定。公共曲庫、LFS原件與canonical library SHA未改或重抓。

目前AVD另作唯讀package觀測：4.0.1／157。本機四APK在離線核對前後hash相符。本輪額外重讀實機整個大分包因時限未完成，base／UnityData已完成部分hash相符；partial digest不作成功或變更證據。全庫比對以先前授權並核對的本機APK快照為來源，不將不完整重讀稱為四分包重新驗證成功。

使用者手動選Credits AT並Play，只讀gRPC取得實際Credits／AT Lv.15／0000000畫面，沒有觸控。一次專用RGB校正參考SHA：`15a3ed0e9fb7b191f52d42cafab9495cc59776f6f80c79602fa9d266d2a43fa5`，留在repo外，之後一般回合不存圖。

新增`shared-difficulty-subpixel-prefix-score-v4`，dataset必須宣告相同難度。AT的A從x1147開始，原IN裁切會切到筆畫；新policy採[1144,673,81,26]，保留AT／Lv.五個固定筆畫群，結束於可變等級數字前。原IN區域、white support、負空間、位移族、七個score glyph與freshness門檻不變。title redundancy亦支援獨立AT teacher及完整難度裁切，仍要求兩個不同實際開場觀測／60ms，不能從planner tick取得觀測資格。

實際AT參考正例、IN／result負例與AT單參考建立通過離線檢查；單次frozen-frame建立不等於已完成實機兩觀測學習或AT整曲驗證。

## 解除2000 notes門檻

profile、prepare、首音模型、runtime、library與Flick分析不再以2000音符直接拒絕。新增**2001完整接點／4002事件**的compiler、opening與runtime回歸，沒有截斷音符。

仍保留有限預算：app最多16000完整事件／receipts，core runtime最多20000 steps，閉合接點至少需要DOWN／UP兩事件，因此接點數由事件預算推導；首音方程／工作量、180秒、5指與路徑限制不變。本批沒有超過2000 notes的真實譜面同時通過其他全部預算，不能由合成回歸宣稱大型真譜已實跑。

Glaciaxion保留直線72px profile與時機；原21首其餘設定不變。其他新候選採原dynamic Flick60px、30／60／24ms、phase −80ms、Tap／Drag32ms、inset64、D7000、capacity5，沒有未經比較就將直線候選設為全曲共用預設。

## 封存與驗證

- 程式提交：`a50229d960e9031c77c62514c3191c2b6f12fba8`。
- 新封存：repo外`catalog-native-build-005/`，201份固定產物、乾淨HEAD／source snapshot；003／004／舊build006／AP原件保留。
- binary SHA256：`7fd7e04399b3cb62bb06afe1653d0434dfe8ce104d1a05eb94e2f1b26ee7c979`。
- 本機根目錄：`local-assets/phigros-4.0.1/full-library-local-20261025-174833/`。正式清單`prepared-002/test-library.json`、`launch-config.json`、入口`open-full-library-panel.cmd`。identity、逐譜report、失敗／重試、263份最終前檢及GUI pins分別保留。
- launch／plan／ledger schema為`pas.manual-song-panel-launch.v2`／`pas.catalog-test-plan.v1`／`pas.manual-catalog-panel-ledger.v1`。原Legacy v1仍保留20首章節與額外暖身的嚴格身份驗證。
- Windows patched-gRPC native **29/29 CTest**，Flick／來源／opening **207 checks**、learned HUD **161 checks**、HUD glyph **158 checks**；Python **244項：243通過、1個Windows symlink權限skip**；portable configure／Release build／CTest **4/4**，面板SelfTest **52 checks**。
- 最終binary **263/263**離線前檢，全數input=false／image_retention=none；實際完整v2 context **1166個pins**核對通過。另產生並檢視389列正式控制項的離線預覽，未宣稱實際點擊、DPI／多螢幕互動QA。

每輪仍使用新目錄保存完整compiled plan、events、receipts、metadata-only HUD、結算frame metadata與停止／釋放紀錄，不保存每輪圖片；結果數字由使用者文字提供，不自動判AP。批量離線準備的可重建計畫只留SHA／大小／接點／事件摘要及profile／報告／日誌，避免數百份大計畫副本；實際遊玩回合的計畫不刪。初次AT裁切拒絕、中止的prepared-001、C++建置／舊2001拒絕測例失敗及後續成功重跑保留，舊manifest不改寫。

保留PAS／proto／third_party與main不變。使用既有patched gRPC，第三方fresh source重建NOT RUN。新增AT只讀觀測與離線候選不等於新005整曲輸入／遊戲判定；**新005遊戲測試NOT RUN，沒有新AP或改善宣稱**。
