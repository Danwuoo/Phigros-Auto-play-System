# C-V3-CONTACT-1：有界斜交 Hold 當前支持

契約初次凍結：2026-10-05，基線 `a53a9b7021bcc900f498047f5cc017b42e4711b0`。這是新隔離候選契約，不回寫 round2／round3、原 RGB oracle 或 typed renderer。修補目的限於有真實可見 blue 見證的斜交 Hold，並非遊戲接納或 physical owner 證據。

## 1. 原缺口與新契約

原 V10 的 body angle=.25、line angle=.4、width=140、front=(316,505)，中心線交點約 (317.542060924,498.960800027)。舊式先將中心 hit 沿 line normal 移 ±4／6，再沿 body tangent 移 ±49，導致兩個 sign 各有一個 delta6 點踩白線。其餘可見 blue 不能满足舊 literal probe 規則，所以原7項失敗是真的規格／機會缺口，不能改稱只是安全拒絕。

新規格只改 Hold：

1. 接受單一已計算的中心 hit、當前 View、Query、Line；hit 必須有限、位於 frame、body rectangle、body 中軸及當前有限 line segment，並在該線上。外部依舊負責 body 支持、當前 rails、line 數量／唯一性與動作資格。
2. 在 body 局部切向 ±.35×width 處，各取一條沿 body normal 的 flank 直線；分别計算它與 line 的交點。交點只作幾何定位，不是像素支持。它可微越 body 端部，但必須在 frame 與有限 line segment 內。
3. 每個 flank 交點各沿 line normal 移 sign×4、sign×6。對同一 sign，四個實際像素見證全部 blue 才 supported。兩個 sign 任一成立即可。不得把左右不同 sign 或不同 delta 拼成一組。
4. 每個採樣的連續座標及 llround 後像素中心都必須在 frame、Query body rectangle，且沿線投影落在當前 line segment。這保留了4／6的 line-normal距離與固定最多8次讀取，沒有沿 body 軸除以 cos 放大局部步長。
5. blue／yellow 閾值原封不動。white、black、裁切、少一 flank、少一 delta 都不能代替 blue。yellow 只表示在合法當前採樣位置符合原 predicate；它不是透明 layer 存在的同義詞。
6. abs(dot(body-normal,line-normal))<1e-6 拒絕。更近於 parallel 的 geometry 若把 flank 交點移出 frame／body 支持／line segment 也拒絕；不將所有非同向 Hold 一律拒絕，也不以 typed geometric contact=true 代替像素。

此安全局部契約並未證明4／6、.35width或最小body支持行數适用所有真圖。極短 body、裁切及完全遮蔽仍可能沒有可辨見證；它們是能力限制，不宣稱 zero-miss 通過。

## 2. 接口與容量

`ContactSample { bool supported; bool yellow; } sample_current_contact(const View&, const Query&, const Line&, Point hit, std::uint64_t& probes)`；無持續 state，無配置、typed資料、history、owner、注入或學習介面。Tap 保持舊 ±3／3、兩側 flank 的逐點取樣語義，不加入 Hold 的 body 長方形限制。

每個 query／line 配對最多8次 pixel probe，最多128 queries／16 lines，所以 contact 子項最多16384次，另由既有 extract 全域6291456 budget檢查；不新增動態 geometry 搜尋。frame 仍至多1280×720，Query width4..4096、depth(0,4095]。函式對不合法 View／非有限幾何 fail-closed。probe達上限後記 budget+1，供 caller 的原 overflow invalid 分支辨識，且不再讀取；不發生整數wrap。

## 3. 預先凍結的可否證覆蓋

正例：同向body／line、共同旋轉、原V10、反向斜交、整數平移、較深處斜交、可見16px短body。正例分母独立計數，禁止以全部拒絕通過。

負例：黑圖、白圖、opaque yellow、單flank、左右不同sign、欠一delta、2px短body、parallel／near-parallel、frame裁切、center hit不在body／中軸／line、短line未涵蓋flanks、幾何或frame無效、probe耗盡。即使全圖blue，幾何限制也不能繞過。

整合覆蓋：雙條各自可支持line必須line_unique=false，Move／refresh=false；line順序置換不改資格。唯一可見斜交line有合法known-down Move。未知Down、已完成、無rails保持拒絕。Tap在同樣RGB及geometry下必須與原子predicate相等。

測試來源與expected凍結、baseline／candidate獨立報告、source/binary hash、native exit分開保存於 `evidence/contact/`；後續若修正測試或契約必須另留revision，不刪初稿或失敗。

## 4. V04 effect明確保留

原V04 RGBA=(255,225,80,89)在blue=(40,190,255)上整數混色為(115,202,194)。原yellow規則為R>=180、G>=120、B<=180，故false；它同樣不滿原blue predicate。可觀測名稱為 `yellow_predicate_visible_v1`；「renderer有alpha layer」是authoring資訊，不是這個布林值。

V04原RGB/e2e effect=true共2項紅字保留，原oracle不改。body／contact／Move原本符合oracle，尚無此診斷差異造成動作缺陷的證據。本輪不加透明色差偵測、不擴色閾值、不加模型，也不把新名字當成修掉原斷言。下一步只有在真圖與明確動作需求顯示必須區分某類透明effect時，才另立可觀測分類與獨立正反例。

## 5. 下一必要 gate

本輪僅完成上述有限合成反證與整合回歸。要決定當前判線／ROI在真Phigros斜交、近parallel、短body與遮蔽下能否產生這些合法見證，需要原始真圖、合法current ROI／all-lines來源及相符的已Down前綴；不能用手填geometry或typed oracle填補。交接應包含成功／失敗probe座標與RGB、body／line支持、唯一線資格、exact source身份及容量，不先以新增無限合成矩陣延長雲端開發。
