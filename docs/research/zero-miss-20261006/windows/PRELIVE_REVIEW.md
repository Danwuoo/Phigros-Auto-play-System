# current-rails-v1 原圖語義裁決

圖卡工具只讀原 PNG，產生三倍 nearest-neighbour 原樣 crop 與另存標記版；
不補像素、不作學習標註，也不回饋策略。來源／crop／RGB samples／SHA 在
`out/prelive-20261006/review-cards-02/cards.json`，原圖均保留在原 checkout。
初次 WIC encoder 格式失敗完整保留；新 attempt 明示 RGB→BGR 編碼，沒有變更顏色。

|圖卡|可核當前支持|裁決界線與最小缺件|
|---|---|---|
|3030 visible-front|矩形漸層下緣與雙 rails；既有使用者約(495,237)確認 head 前緣|只確認可見語義；精確合法接觸點、實際判定仍 unknown|
|3028 prior-current|同位置附近較早的實際外觀，含當前 body／background 對比|不是已完成 Down、contact 或合法 entry gold|
|3030 interior|body 內部取樣點(495,214)，其白 rails 不證明該點是 head|保留 body 與 head 的區別；沒有新增人工標註|
|1519 effect|當前藍色物件與黃色光斑／背景幾何重疊|最小外部問題：候選4/5是兩個可按物件、同一物件拆分，還是效果？無人工回答，不給物理 identity gold|

三個 cyan variants、neutral body 與 inside/outside 對比是事前通用假說，
新冷控制支持其機械契約；沒有以3030單張或合成 oracle 證明各曲遊戲語義。
完整3063輸入從原錄影起點、候選自己的 lifecycle round reset 開始；每個窗口不 seed
舊 touches。短窗／完整前綴的 accepted 仍是候選准入次數，不是物理 opportunity 分母。

完整前綴首輪在2742幀發現 Drag/Flick 已取消 identity 經原 owner 重新建立 intent，
新 Down 後 ledger 才拒絕。這是可冷修的整合缺陷，不能以「外部 unknown」忽略。
修補在 A、B 共用的 dispatch 前明確拒絕自身 completed／cancelled／unknown／retired
identity；原原則與 oracle 不放寬。首輪 source、raw events、退出1與失敗驗證保留。
後續結果見 PRELIVE_RESULTS，物理 adoption 仍須目前版本遊戲／裝置才能回答。
