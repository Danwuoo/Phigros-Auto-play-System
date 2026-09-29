# 追蹤關聯對照（2026-09-27）

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

實作 source `b32517d`，分支 `codex/tracking-segmentation-pilot`；不合併／推送。真實觸控仍採 T0 legacy／observer29／planner8、35ms、兩指。T1／T2 只供離線與單一 shadow worker；本輪未訓練或接入視覺模型。依據 開發計畫（TRACKING_SEGMENTATION_PLAN_20260927.md 已清理，歷史見 Git baf3d4f），完整 pipeline、獨立 line tracking 與正式替換仍 pending。

## 實作及資格

- `game_tracking` 抽出舊 greedy 與原撞線擬合。`CandidateBatch` schema1／extractor29／quality1，保留 source frame、整數 QPC、幾何、當前支持與歷史 hint 年齡。T0 共用同一份函式，沒有第二份 production observer。
- `byte_association` 先配對強候選，再以弱候選續接；`oc_observation` 加觀測方向與最後觀測重接，可用 `--oc-reupdate` 消融內部修正。兩者為本專案適配版，不是上游完整 tracker／MOT 分數重現。
- 實際 `dt` 的 Kalman 與 Hungarian assignment 均有界。最多128 tracks／128 candidates／16 lines，實测 fitter 仍是最多6點、90ms、10ms bucket、至少30ms跨度。ORU最多3個內部虛擬修正點，不進入 fitter 或 action evidence。
- 弱候選不建立新身份；弱 action support 另需當前可見 head、body、左右 rails、已到線幾何，以及≤90ms且來自之前frame的 anchor。純預測不出現在完整可執行 targets，不能刷新期限或避免missing。
- 相鄰／重疊競爭拒絕以 `association_ambiguous`／`birth_candidate_competition`／`birth_identity_competition` 輸出；出生競爭在追加 fitter 點前攔截。Tap／縮短Hold的幾何分流不吞掉薄Tap。
- Hold birth identity 另有最多128筆有界退休記憶。近距離近期重接，或舊到線位置的重生描述，綁回舊執行身份；source／capacity失效不清掉它。滿額時保守關閉動作資格，不能靜默忘記。新的獨立context才重置記憶，ID不重用。這是保守研究策略，可能拒絕新的同位置Hold，因此不可直接採為正式預設。
- line ID 是當前批次內幾何對應，**不是**已驗收的跨幀線身分。保持原 line selection，不宣稱已解決線抖動。

候選提取仍讀 T0 的 rail 歷史。真實 bank 明示 `baseline_guided`，因此只比較相同候選上的關聯；不能將其排名當作各方法完整 pixels pipeline 的排名。灰色Hold完全沒有候選時，tracker不能補回它。

## 有界離線與 shadow

離線入口只在 `pas_core` 建立 `FakeTouchBackend`，不呼叫 `GrpcTouch` 或讀取裝置。bank最多2048批／64MiB檔案；正確性只計唯一frame一次，未知標籤欄位為null。性能重播不當作獨立實戰樣本。

線上 opt-in `--tracking-shadow byte_association|oc_observation`，只有一個值型別 mailbox，容量1，落後覆蓋／計skip。它沒有 Frame lease、backend或owner引用，不等待action owner。正常decision發布後才複製；worker失敗只記shadow fault。另保留首批最多2048 CandidateBatch、16MiB預算，輸入停止後串流寫bank；達額度drop後續，不假稱全曲bank完整。

```powershell
out/release-v145/Release/pas.exe dataset tracking-challenge --output measurements/tracking-segmentation-20260927/challenge-final.json
out/release-v145/Release/pas.exe analyze tracking measurements/tracking-segmentation-20260927/challenge-final.json --methods legacy,byte_association,oc_observation --updates 10000
out/release-v145/Release/pas.exe analyze tracking '<run>/tracking-bank.json' --methods legacy,byte_association,oc_observation --updates 10000
out/release-v145/Release/pas.exe dataset copy-bench --updates 10000
```

`tracking-challenge` 是短小**幾何候選**回歸，不是假造1191–1196 pixels。另有實際生成 pixels 的T0序列與候選JSON roundtrip等價測試。12個幾何clips各12frame，包含弱支持、消失、鄰近、出生競爭、薄Tap、弱假候選、線量測噪聲／真實移動、441ms空窗、假垂直線、head不可見及錯類型；不將合成通過視為遊戲通過。

## 驗證及結果

新增25項：17追蹤、7資料、1CLI，總數138。source `b32517d` 的 Release **138／138、14.09秒**；Debug **138／138、22.70秒**；嚴格ASan **138／138、58.91秒**。不關閉ASAN檢查，不設定 `ASAN_OPTIONS`／suppressions，第三方vcpkg二進位仍未插樁。Release binary SHA256 `d6fb2f7f4ac620782bd4aa85085123386609616561705c26ba2e4c89b7a94ff1`。舊／新binary對v75兩張PNG的單圖分析JSON完全相同；這不是完整runtime逐clock等價驗收。

初輪smoke benchmark與pilot編碼曾重疊，只用於功能檢查；正式分布使用建置／測試／編碼結束後的串行重播，輪換T0/T1/T2，3批、每方法每批10,000更新、排除500次暖機。逐批分布、工具鏈／QPC／binary hash存JSON。若沒有困難實戰標註，ID switch／fragment／誤合併／錯誤續接為unknown；不以cancel變少排名。

唯一正確性分母為144 frames／12 clips：T0／T1／T2 identity switch各2、fragment各2、false merge各0、duplicate Down各0；T0已知負例續接12次／負例Down1次，新兩者均0。T0 fake Down12（11已標實例＋1負例），新兩者10（10實例）；未知／未觀測不能当成更高recall。出生競爭T0拒絕22次但先建立身份，新方法拒絕24次且不出生。ORU開啟版本的這組身份／誤續接結果相同，未取得採用證據。固定線合成crossing error n71／62／62均0，由線性軌跡構造，不包含線抖動／真實移動的未知oracle。

正式合成tracker成本，每列 n=10,000，單位ms（p50／p95／p99／max）；三批輪換順序，500暖機，沒有其他build/test／編碼並行：

| 方法 | batch0 | batch1 | batch2 |
| --- | --- | --- | --- |
| T0 | .0014／.0064／.0116／.0964 | .0011／.0019／.0021／.0294 | .0011／.0020／.0028／.1916 |
| T1 | .0062／.0107／.0173／.1501 | .0067／.0128／.0598／1.1412 | .0068／.0109／.0215／.1535 |
| T2 | .0069／.0109／.0173／.0784 | .0068／.0104／.0199／.1396 | .0065／.0106／.0162／.1556 |

實戰第一輪 `cpp-observe-17904941035800460` 的首2048批約40.35秒（含非PLAY context），全部truth unknown／baseline_guided；fake Down T0=46、T1／T2=43，只是輸出差异，不是命中／miss改善。identity switch／fragment／merge／錯誤續接／duplicate均null。成本仍3批×10,000更新，詳細分布見 `live-tracking-comparison.json`；T0 p99=.0046／.0220／.0169ms，T1=.0330／.0422／.1439ms，T2=.0764／.1414／.0881ms；max分別1.5141／1.6070／1.5456ms。重播數不能作2048以外的實戰樣本。

第一輪shadow n10,018／skip0／fault空、無backend／lease；update p50／p95／p99／max=.0384／.0785／.128483／.7414ms，capture-to-shadow=7.0662／11.22371／13.716335／23.1015ms。7300餘prediction-only為診斷輸出，沒有送觸控。真實T0成績345／6／0／42，尚未AP；不同來源空窗不支持方法收益推論。

第二／第三輪由使用者逐輪就緒後完成，仍採同binary／T0真實觸控／T1shadow。run `cpp-observe-17904949475779425`／`cpp-observe-17904951892485298`，10,151／9,620 shadow批、skip0／fault空；update p50／p95／p99／max=.0394／.0826／.13295／.6061ms及.0405／.081805／.131581／1.1127ms。分別保留首2048 bank／drop8103、7572。三輪共6144已留唯一批，truth全unknown／baseline_guided，不能以同曲重打或性能重播充當獨立標註。

第二bank fake Down T0／T1／T2=45／41／41；第三=51／43／43。身份switch／fragment／merge／錯誤續接／duplicate皆null，減少Down不等於減少Miss。離線仍每bank三批輪換、每方法每批10,000更新：第二T0 p99=.0060／.0069／.0093、T1=.061102／.0656／.052601、T2=.083807／.075302／.060406ms；第三T0=.0048／.0044／.0062、T1=.035601／.033901／.0368、T2=.051902／.038001／.0310ms。逐批p50／p95／max與bank hash見metadata及 `live-tracking-comparison2.json`／`3.json`，沒有同時build/test或編碼。

保留T0。新方法擋住合成弱假候選，但身份switch／fragment沒有改善；三輪原生clips已補上，仍缺獨立實戰連續真值，不足以採用。ORU、獨立line與T3未因本輪開發自動啟用。`performance_pass=null`；可行方向是單worker shadow，live update p99約.13ms，但不是已決定的正式性能門檻。正式替換仍須困難集ID改善、無新增危險續接及完整pipeline證據。三輪結算及跨run近似去重QA見資料報告；此批遊玩已停止。

## 證據位置與限制

本機根：`C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/tracking-segmentation-20260927/`。含P0索引、舊v75 binary、建置／短測／完整回歸log、environment、challenge及pilot；關鍵檔案hash與實測結果索引見[提交的metadata](TRACKING_SEGMENTATION_EVIDENCE_20260927.json)，不含raw images。Windows11 Pro 10.0.26200、Core Ultra5 125H（14 cores／18 logical）、約32GiB RAM；RTX3050 6GB／Intel Arc，完整driver版本見 environment。MSVC19.51／C++20／VS18／v145；測性能只用Release。原live擷取固定gRPC payload fast／256KiB、既有phigros AVD。

原v75兩張圖只能測觀測覆蓋，不能量測身份切換／重接。逐Note遊戲judgment仍unknown；HD尚未AP。採樣與模型資料狀態見 [資料報告](VISION_DATASET_20260927.md)。ignored證據仍是本機單份保存，Git提交不是圖像備份。
