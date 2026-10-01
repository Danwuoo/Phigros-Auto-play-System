# C36g 疑似 Miss 影格與 Hold 標註小試

使用者於2026-09-30首輪C36g完成後表示「問題還是沒有被大幅改善，可能要
特別把 miss 的禎找出來標註」。本次先停止PAS、封存結果、離線整理已有pixels；
沒有追加遊戲輪次或更改動作策略；只對已完成PAS送出正常停止訊號。

## 本輪結果與抽樣缺口

Dlyrotz IN13：796284分、P/G/B/M=491/18/4/71，共584判定；max combo126、
accuracy86.08%、Early/Late15/3。C36f兩次均81Miss；本次少10，但只有一次、
不是受控因果A/B，也不作「大幅改善」或跨曲穩定驗收。

原資料根為`measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36g-01`；
session=`manual-session-16106368637700`。PAS正常Ctrl+C exit0、STOPPED；兩段
events、manifest、result及27RGB的SHA核對相符。這是新一輪驗收；原12/12封存不修改。

現有採樣只在uniform及complex_line_event取三幀，沒有Miss觸發或事前ring。
9組共27張，首尾跨度相加231.1412ms，分散在約130.837秒round內，不能當連續影片，
也不能由71Miss反推出缺失影格。7781決策能定位診斷事件，但未保存的pixels不可重建。

## 離線覆核工具與產物

新增C++20 `pas_miss_review`，只讀round與pixels，沒有emulator／touch backend。
正式C36g runtime與策略不變。依summary驗證segments，依index驗證RGB SHA及bytes；
按source_frame＋capture_complete_ns對上當時journal，沒有三幀冷重跑取代實戰狀態。

```powershell
pas_miss_review.exe <round-root> <pixel-clips-root> <new-output-dir>
```

上限：32段×32MiB、1M rows、100k decisions、每幀16lines／128targets、64個
saved frames及2048個事件。每次只載一張RGB，summary／index各2MiB，journal
row2MiB。越限／SHA錯／路徑逃逸拒絕；output不得覆寫。

- `miss-review-v2/review.html`：27張全解析度原圖與當時偵測疊圖；保留原圖不畫框。
- `contact-sheet.png`：三欄九列原图縮圖，順序與`clips.json`一致。
- `incidents.json`：228個候選，189次已開始接觸撤銷、39次Combo glyph消失；
  17次Down前撤銷排除。glyph只辨形狀，沒有OCR或Miss分類。
- 228候選中只有7個在±500ms內有任何稀疏原圖，221個缺圖；僅1個事件的
  source_frame真的被保存。500ms僅是搜索半徑，不能用來判同一個Note或Miss。
- `annotation-template.json`：27個精確journal join，線角色、Note部位／關係／
  結果都unknown；journal只作candidate hint。額外漏檢實例可另填，不被候選器限制。
- `case-118-proposed.json`／`case-118-review.md`：助理看原pixels的第一組標註提議。
- `case-118-annotation/review.html`：C++驗證PNG／RGB provenance後，以SVG框出
  body／rail／tail／水平線的proposed範圍，保留可點開的未標註原圖。`miss-review`
  初版亦保留；v2把綠叉明列為候選投影、不是實際觸控，line0不画無效落點。

actual Miss定位0、human gold0。自動候選、助理proposed和人工確認分開；任何標籤
都不回饋live決策，不以歌曲身分調參。結果圖的71Miss是round aggregate，不能
變成228個事件的逐Note標籤。

Release工具建置／實際資料匯出通過；27個精確join及unknown欄位另核對。
四項負例：路徑逃逸、RGB SHA錯、既有output覆寫與proposal擅升human gold，
皆exit1拒絕，前三個新輸出案例沒有產物；覆寫案例原輸出保留。記錄在
`miss-review-checks/verification.json`，其中bad-gold-proposal只是假資料拒絕測試。
凍結C36g binary SHA未變；工具source／binary／DLL及離線analyzer SHA在
`miss-review-tool-provenance.json`。本次只新增離線工具，沒有重跑或變更已凍結
的227項正式回歸結果。

資料帳本最後量得campaign1740563189bytes，含舊12輪、cold候選與本次
180259374bytes驗收／覆核產物；保守另留1MiB為1741611765，低於8GiB。

## 第一個有原圖的 Hold 事件

事件118，開局後60049.0675ms，frame27449，intent280／note1052，已執行4步，
reason=`identity_ambiguous`。原圖可見垂直Hold兩側rail和body到水平y576白線；
中心下方被黃色特效部分遮蔽。line138當幀valid且保持水平，沒有全線失效。

frame27448的原note1052仍有line138；27449卻變`association_ambiguous`、line0、
rails=false。同幀note1063有當前rails、line138，因新ID僅2samples而不能接入。
27450只見1063；27451原1052返回，但另有1063歧義及新1067。這支持研究
「同一可見body被前景片段／rails切成重複身分」的假說；1052／1063／1067是
runtime候選ID，不是人工物理Note真值。

三張原圖可提出body／rail／tail位置及同body關聯，head受遮擋保持unknown。
三幀COMBO仍18，沒有可見Miss結果；不能證明事件118導致71Miss中的一個。
前兩張27447／27448只有journal、没有raw；後續也缺連續pixels。不能只因觸控
被撤銷就放寬identity／重試Down；要先以足夠事前暖機的pixels與fake-clock重現。

## 下一次資料補採設計（尚未實作／未啟動）

2026-10-01補記：使用者已改為授權全錄一輪並自行篩選。下列20fps事件窗設計
保留為歷史提案，現行採用[單輪全錄契約](FULL_ROUND_RECORDING_20261001.md)。

優先改診斷採樣，再安排有限manual實驗；現有三幀不足以驗收長Hold。
建議用硬上限的診斷ring，保存觸發前後約1秒的實際最新pixels，記錄dt／skip／
drop及QPC。Combo可見性消失、已Down Hold撤銷／證據到期都只作疑似失誤觸發；
同時保留uniform成功／無事件對照，避免只標失敗、無法量誤觸發。

例如20fps、2秒事件窗約40張=110592000bytes；32張事前ring约84.375MiB。
每輪最多6窗、全run最多12窗，另設writer mailbox／匯出佇列與磁碟硬額度，
時間窗重疊合併、滿額拒錄並記分母。這些是待驗設計數值，沒有已接入兩秒採樣器。
採樣只單向流出，不把ring供observer／owner讀取；先量copy／writer成本與慢writer
退化，不能讓診斷拖住latest-frame或改觸控排程。舊原始證據全留。

標註至少含：線角色、當前Note／head-body-tail、clip-local物理身分、Note→line、
當前可接觸區、遮擋／unknown、當時Down／Move／Up收據及獨立遊戲判定證據。
幾何過線與程式Up都不是完成真值；無判定證據保持unknown，不要求補採阻塞其他冷研究。
