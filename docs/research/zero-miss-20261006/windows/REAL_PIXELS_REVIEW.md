# 10/6 兩張當前真圖覆核

承接 `b100df82321461e79f213e64f39e609b94bd9c4d`，在既有 worktree 建立
`codex/zero-miss-real-pixels-20261006`。本包只本地程式／離線核對及 commit，
沒有裝置命令。原 v3、正式策略、oracle、36 fail 與舊 evidence 索引保留。

## 可觀測缺口與裁決界線

已直接閱覽原 frame3030、1519，未生成或改寫原圖。派送 task 的原稿與 source
保留在撤回驗收證據內。本文件已依自行驗收結果更正；可重算的座標／RGB、
人工單筆語義與完整遊戲採納分開。最新結果見 [撤回驗收與自行開發](WITHDRAWAL_ACCEPTANCE.md)。

- 3030：query 對應 candidate2，中心 `(495.5,228)`、tail `(495.5,150)`、
  width152、depth78，origin=`current_reconstructed_front`。唯一當前量測線為
  ID2、中心 `(640,576)`、水平 length1280、association_valid=true。
  該 query 遠離線，因此即使修 body 色彩也不能在此幀證明合法 Down。
  原取樣內側 `[192,226,237]`／`[193,227,238]` 不符合 R 上限及 B−G≥20；
  white rails 本身不能替代 blue interior。front+3 `[156,233,255]` 不符合
  R/G 上限，且非 black。圖上 proposal y228 位於漸層內，下方平直可見邊約 y237；
  使用者已明確確認約 `(495,237)` 的矩形下緣是 Hold head 前緣，回答與原圖 SHA
  留在 [人工覆核](HUMAN_REVIEW_20261006.json)。這是一筆近似語義標註，不能將
  observer y228 視為精確 head，也沒有確認已／未接入、合法觸點、body/tail 或採納。
- 1519：線 ID2 約 y576。candidate4 `(746,575)` width54 與 candidate5
  `(821,575)` width56 均有 contact supported、line_unique=true，但 front_end=false、
  relation_usable=false。兩段在 effect 覆蓋的同一條可見藍條範圍內；是否為同一
  Tap 尚待人工裁決，不能由 effect 推定已執行 Down。其 normal±3 中有 RGB
  `[10,195,255]`，v3 拒絕的是 R 下限20；一般 Tap candidate1、3 亦可見同類色芯。

因此零 eligible 並非單一可安全放寬的 threshold 問題：存在色芯範圍、Hold gradient、
front 座標／near-black 外側假設、effect 下分割及當前 relation 多層缺口。
只放寬顏色不能建立獨立物件、接入與完整 body/tail ownership 真值。

## 已實作的最小工具

`pixel_chain.cpp` 在 owner dispatch 後追加 `post_dispatch_witness_review`，
僅輸出 ordinal1519、3030。每 query 37 個法向 offset（−16…20）×5 切向位置
（兩 rail、兩 interior、中心），即185取樣點；至多128 queries／幀，最多兩幀。
整份 trace 的實際硬上限為32MiB；原稿的16MiB是文件錯誤，本包 metadata 封存上限
仍是16MiB。單張借用 current RGB，沒有新增歷史圖或 policy input。
每點列 rounded pixel、RGB、v3 blue 拒絕項、white、black。duplicate integer pixels
仍列各自 query-local 位置，不冒稱独立像素證據。這是診斷擴充，沒有新動作候選。

## 本包容量與下一個最小門檻

起點 C槽 free197,267,894,272 bytes；本包 out≤2GiB、metadata≤16MiB、
保留≥20GiB空間。資料另存 review roots，原11.31GB out／45.55MB metadata
及原12GiB／64MiB上限不改；不複製7722 PNG，不刪 STOP／binary。

3030 可見 head 前緣已有使用者確認，其他接觸語義仍 unknown；1519 candidate4/5
是否為同一 Tap 仍待裁決。凍結 selection 的 human_gold=0 不改，獨立 annotation
只新增一筆語義。後續 typed front 必須由當前 pixels 推導，不可手填 y237、加固定9px，
也不可將 full body depth78的一半當作 head 厚度。色彩候選與完整接觸資格另須凍結驗證。

派送 task 的四個 Release stage 收據已獨立驗收；其 metadata 匯出因 PowerShell
裸 `false` 及撤回中斷而未完成，原 partial 目錄保留。自行開發 source `774f3e7`
新增座標／storage 邊界防護、32項診斷契約及獨立原 PNG audit；三配置均核對256幀
決策與1,665取樣點，兩個竄改控制被拒絕。Release 初次程序查核 STOP 亦保留。

完整 latest-frame/mailbox/journal/RPC 成本未跑，NOT_READY；人工合法機會分母及
完整真實 Down/Move/Up 採納仍 unknown。全曲IN zero miss未完成。
