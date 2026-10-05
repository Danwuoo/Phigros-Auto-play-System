# Cold v3：獨立 current-RGB 行為審查

2026-10-05；研究基準 `a53a9b7021bcc900f498047f5cc017b42e4711b0`。隔離 cold candidate；不是 physical ownership、遊戲接納、裝置觸控或 Miss=0 驗收。

## 結論

最終44-case／101-assertion suite三配置全過：合法17/17、過拒0/17、安全誤放0/22。實作審查另找到3個非pixel Query noise錯Down，留下失敗並促成最小修補；追加5-case三配置也全過，2/2合法、0/3誤放。3個不可辨decision-only仍不計pass；產品與實機門檻未完成。

## 先凍結的輸入與期待

在讀取任何 v3 implementation cpp／policy helper 或取得 v3 輸出前，先依 AGENTS、goal、round3 的正例／失敗／decision-only 和公開 baseline header，凍結 [39-case 期待](evidence/independent/expectations-frozen.json)與[完整自有測試](evidence/independent/pre-candidate-tests.cpp)。

- 原期待 SHA256：`6d894b7a13837a156e0818f1e56577d3cbf1a91e6d61df319458502af076eff2`
- 原 source SHA256：`cb3e5420b317936192b4e2641d2b8995da44070b689f386b62ea0c0a1f59deb4`
- 公開接口通知後只預留有條件的 `observe_relation_context` 呼叫，留存[接口版本](evidence/independent/pre-candidate-interface-tests.cpp)。正式獨立 runner 不定義該 macro；v3 extract 自行註記 relation context，測試不依賴雙重 annotation。

先跑未修改 v2 baseline 後發現，原 active-alias 兩例僅有不變像素，不能單獨證明 qualified new Down 被抑制。因此在尚未讀 v3 implementation 或得到其結果前另凍結 [5-case addendum](evidence/independent/addendum-expectations-frozen.json)。原39例的邏輯與 gold 保留，不因 donor 輸出修改。新增 qualified alias、moving-endpoint duplicate、切向線運動、遠離線與 source-invalid，總計44例。

- Addendum 期待 SHA256：`f3af8682c3ce81b53579130b3ff02e5f60eb1bc4a5d186b00f6f7660ba69edc0`
- 最終預先凍結 source SHA256：`c302f8d280df58feff84f60d6456fc43d184474b46bab946ec44f4b07267b664`；[快照](evidence/independent/pre-candidate-addendum-tests.cpp)

Renderer 直接用 C++20 畫 640×640 整數 pixel centers：blue body、3px 白 rails、4px 白 measured line，並可生成可見缺損。未 include canonical renderer、typed adapter、candidate 內部 helper 或原 oracle。只有合法 RGB、ROI、當前 measured lines、時間鍵與一致 fake receipt 傳入候選。render flags／case ID／期待值均不傳入。線與 pixel pose 一致；沒有以錯誤 metadata 暗測不同層。

## 覆蓋與分母

- 17 個合法動作機會：4 個 stationary 相對接近、exact prior/current duplicate 與非canonical attachment、兩個分離可見 notes／Holds、4 個斜交 contact、qualified active alias、moving-endpoint duplicate。
- 22 個安全拒絕：僅背景／無關線／same-source replay／非pixel微小幾何 noise／epoch／future／stale／兩當前線／換 line ID／unknown／completed／two-Hold union／near-distinct prior extents／white-only／缺 rails／parallel／畫面邊界／2px body／缺側支持／切向／遠離／source-invalid。
- 2 個 permutation；3 個不計 pass 的 decision-only：不可觀測 physical owner、同一可見 body 的非exact近重複 ROI、6px body。
- 共44 cases／101 assertions。合法動作成功與 overreject 依 Down 數及 Move 欄位另計，不用完整 case assertion pass 冒充動作成功。安全組若有任何 final Down／Move 即計 unsafe false positive。

Current duplicate 要求恰1個 Down；qualified active alias 要求仍用 contact3、Move／refresh並且整組0個 Down。R08 與 A02 共用可形成新Down資格的 stationary Hold＋approaching line，以防因缺 confirmation 而假過。

N04 的當前 line y500.10／500.11／500.12 產生相同線 raster，背景 pixel 分別改變；不得把 whole-frame RGB 變動和 subpixel pose noise 拼成三份有效相對量測。R06 使用 width32、centers299／341 的可見分離 Tap；N13 使用兩個15px Hold extents且間隔5px，後續 union不能冒充 exact duplicate。

## 未修改 v2 donor

[44-case Release](evidence/independent/baseline-release/summary.json)：44 cases／101 assertions／23 failed，native及runner exit1。合法 action **2/17**、overreject **15/17**，安全誤放 **0/22**；2個permutation過，3個decision-only不計pass。原39例的[首跑](evidence/independent/baseline-results.json)為89 assertions／20 failed，完整保留。

這組刻意針對 v2 已知缺口。不能據此說 v2 只有2/17一般能力，也不能把0/22誤放延伸成全面安全證明。

## 獨立新挑戰：非像素 Query noise 仍會錯 Down

第一個 v3 freeze在原44/101三配置全過後，實作審查發現 moving-note 路徑仍把任意 double Query／signature差異當新端部。這是**source-review後另立反例**，不是改動原44例，也不冒稱全部都是盲測。

先凍結 [N23期待](evidence/independent/query-noise-expectation.json)及[獨立probe](evidence/independent/query-noise-probe.cpp)，再取得[首輪失敗](evidence/independent/query-noise-initial-result.json)：Tap x320.10／320.11／320.12、y500；移除背景tag後三張note＋line RGB逐byte相同，只改一個無關背景pixel，卻independent3／40ms／Down1。

隨後另凍結[4-case matrix](evidence/independent/query-noise-matrix-expectation.json)：x、width、angle非pixel微變三個拒絕，以及真正1px可見note平移正控制。三個noise輸入都驗證移除背景後RGB逐byte相同。初版v3均錯Down；未修改[v2 donor重跑](evidence/independent/supplemental-v2-donor/results.json)也同樣3個錯Down。因此這是新找到的既存缺口，不把它誤稱v3新引入的回歸。

修補前另凍結[0.5px累積正例](evidence/independent/query-accumulation-expectation.json)：x318→318.5→319→319.5→320，20ms間隔、80ms span。需要比較最後已計數witness，不應因每次只走0.5px而永遠過拒。

最終只在保留原RGB／signature／same-Query去重後，增加front相對最後已計數witness至少1px Euclidean位移。stationary line 路徑不改。最終noise拒絕 **3/3**、unsafe false positives **0/3**；1px與0.5累積兩合法控制 **2/2**，過拒 **0/2**。累積例最終independent3／80ms／Down1。完整[初版結果](evidence/independent/supplemental-initial-release/summary-recovered.json)與[最終Release結果](evidence/independent/supplemental-final-release-rerun/summary.json)並存。

這五個追加case分開報，不將單獨N23診斷probe再次計數。純width／depth／angle變動但front不移，現在不能單獨新增Down資格；已Down的current contact旋轉Move不變。這是明列的front-only新接入限制，不宣稱解決所有晚對齊／純旋轉接入。

## 最終三配置結果

第二次freeze的原 **44 cases／101 assertions，0 failed**；合法action **17/17**、過拒 **0/17**，安全誤放 **0/22**。2個permutation過、3個decision-only仍不計pass。

- [Release](evidence/independent/final-release/summary.json)、[Debug](evidence/independent/final-debug/summary.json)、[ASan＋UBSan](evidence/independent/final-sanitizers/summary.json)：build0／native0，結果逐byte一致。
- 追加5例的[Release](evidence/independent/supplemental-final-release-rerun/summary.json)、[Debug](evidence/independent/supplemental-final-debug/summary.json)、[ASan＋UBSan](evidence/independent/supplemental-final-sanitizers/summary.json)：全部native0，兩份probe結果各自逐byte一致，2/2合法及0/3誤放。
- 全部最終stdout／stderr為0bytes；[完整比較](evidence/independent/final-second-freeze-comparison.txt)。LSan使用`detect_leaks=0`，不宣稱leak-free。
- 最大observed probes為411,229，包含全圖signature；只是這組合成輸入，不是worst-case或p99成本。
- 初版44例結果保留於candidate-release/debug/sanitizers；與最終44例輸出也完全相同。額外反例不被原44例全綠掩蓋。

補充runner第一次在彙總JSON時有jq括號錯誤，native tests與source穩定核對已完成；原runner、native結果與錯誤脈絡[保留](evidence/independent/supplemental-runner-recovery.txt)。修正僅改彙總括號，Release於新目錄完整重跑，不把wrapper exit2寫成成功。沒有改測試或gold。

## 實作獨立覆核

所有期待先凍結後才閱讀v3 cpp。最終[source SHA清單](evidence/independent/final-reviewed-source.sha256)：

- bvi.cpp `866ad2440d088ec1caf360eb5d4a9c796fdfcbcf2e15aff92374baf2291ea824`
- bvi.hpp `565e7c058328d01b11b75e7bf05a96a96dcdd378b42f000a422c000957671d5f`
- relation_policy.cpp `37a23c696654b3935a51d422991f260213094a48d1488dc45e74334b29ac1efc`
- contact_policy.cpp `b08520d87591b914fd357205e506a5080abb690dc4efe08c6e44e1225aea1c6f`
- constraint_policy.cpp `693cb7a1fa177035e1542c772dd2e26e63cd280fcdc34067aea4f3959297c821`

覆核重點：

1. Exact aliases比較完整Query、Key、當前support／measured geometry及line witness；沒有tolerance去合併near-distinct物件。original indices保留；歷史nearest／contained計數只取representatives。active attachment在任何alias均封鎖整組newDown，Move仍用原索引及contact。
2. Stationary novelty是獨立路徑：同Query、同唯一current line ID、真RGB不重播、逐次normal接近及3/30ms；換號／遠離／missing／multiple重設。whole-frame RGB僅作去重，不能單獨支持novelty。
3. 新front位移gate比較最後已計數witness；中間未計數點不移動此基準。seen／RGB暫存最多6項，保留6samples／90ms／40ms gap／source deadline與unknown/completed邊界。這不是無限history或physical owner重構。
4. Hold contact按每flank自己的line交點取同sign的4/6px，兩側／兩delta均須真blue；continuous點及rounded pixel均在frame、body、finite segment內。white不代供blue，Tap沿用舊取樣；read上限保留。
5. 主控的原V08回歸另找到：一條線遮住另一條線的藍probe時，不能因只有一條supported就聲稱唯一。最終[core diff](evidence/independent/final-core-second-fix.diff)保留真body範圍內所有幾何可行當前線參與ambiguity；這個幾何競爭只否決唯一性，不創造contact像素支持。原V08及主控追加4/16結果由主控另報，此分支不冒領其独立oracle。

沒有發現上述新增路徑在這組合法current輸入內仍可繞過已測邊界。這不是普遍安全證明：extract仍信任上游current ROI／all-lines測量的真實性；未驗證任意極端有限double座標等非法上游值。不能把fake attachment當正式owner身分。

## 未解項與交接界線

D01 identical observables在一致known-down receipt下Move=true，但世界裡是幾個physical objects仍unknown。D02非exact近重複同body仍ambiguous／不Move；沒有把保守拒絕算安全能力通過。D03 6px body有端部但body／contact absent，不Move；仍是未解可见性界限。

本分支不新增owner／scheduler／Windows/runtime bridge，不讀譜、不加模型，不以合成結果完成Chapter Legacy全曲IN Miss=0。下一步須用真圖合法ROI/all-lines及已Down前綴驗證目前可觀測契約，連同正式owner／Windows門檻交由使用者移交，不以無界增加合成矩陣延長雲端工作。

未執行Windows、device、emulator、game、paid action、push、PR或commit；未更動candidate/header/policy、旧tests/oracle、owner或scheduler。原round3 35/79、contract339與frozen3938由主控另報，不和此分母混加。

在repo root重跑主suite：`bash docs/research/zero-miss-20261005/round4/evidence/independent/run-independent.sh "$PWD" /tmp/phigros-bvi-round2-deps candidate release NEW_OUT`。補充：`bash docs/research/zero-miss-20261005/round4/evidence/independent/run-supplemental.sh "$PWD" /tmp/phigros-bvi-round2-deps research/bvi_cold_v3 release NEW_SUPPLEMENT_OUT`。mode也可用debug或sanitizers。所有runner拒絕覆寫，驗凍結source／期待與前後source一致。
