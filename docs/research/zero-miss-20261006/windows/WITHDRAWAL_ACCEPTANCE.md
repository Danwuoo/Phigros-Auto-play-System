# 撤回派送、檔案驗收與自行開發（2026-10-06）

**派送已撤回，由本 task 自行開發；本里程碑沒有取得實機或全曲 zero miss 資格。**
原 task「Phigros 真圖橋接與離線開發」`01a10f9b-5ada-7a33-8347-2d9be79405f8`
的撤回回合已完成、狀態 idle，停止後續編輯／建置／commit。其已產生的檔案、收據、
binary 與 source `57b00d566db805d4f62bc63f054b3b7a268e7734` 全部保留。
沒有再派送其他 agent 或 task。

## 驗收結論與修補

派送 source 只有 dispatch 後的兩幀診斷，未改動動作策略。四個 Release stage 的
native／runner／verification 均0、held handles 已 signaled、active_final=0；
113項前綴與256張 PNG 的原結果可驗。獨立驗收重核762 frozen entries、2,111份
舊封存 metadata、256張原 PNG，以及 child binary SHA。

實際取樣是每 query 37×5＝185點；source 註解誤寫925。trace cap 實際32MiB，
原稿誤寫16MiB。原 metadata 匯出使用裸 `false`，並在撤回時中斷；
`real-pixels-review-evidence-01` **是保留的 partial，不是當時完成的封存**。
本輪另建立新索引記錄其現有 bytes，不回溯聲稱原匯出成功。

自行修補 commit：`774f3e7020818e94c508cc1481dd0986ebe417a0`。

- 新增有限座標、影像尺寸、stride、完整 storage 與取樣數量的防護，越界不取像素。
- 抽出 C++20 診斷模組，明示185點及32MiB上限，仍只在 dispatch 後單向輸出。
- 新增32項有意義的診斷契約，色彩邊界與原封 v3 extractor 比對。
- 新增獨立 C++ audit，重新解碼兩張原 PNG，逐點核座標／RGB／拒絕原因，並比較
  全部256幀決策欄位。只排除五個主機時間及三個新增診斷欄位。

正式 `src`／`include`／root測試、七份 v3 core、oracle 及程序安全核心沒有修改。
沒有 production runtime hook 或新動作候選。

## 本輪實際執行

三種配置各有全新 build root：`out/bridge-win-withdraw-{release,debug,asan}-01`。
使用已核工具鏈與既有離線依賴，沒有安裝依賴或裝置命令。

| 查核 | Release | Debug | MSVC ASan |
|---|---:|---:|---:|
| 建置與程序收據 | 通過 | 通過 | 通過 |
| 正式 owner／scheduler 自身 receipt 前綴 | 113/113 | 113/113 | 113/113 |
| 診斷契約 | 32/32 | 32/32 | 32/32 |
| 派送 trace 對舊基準決策／原 PNG audit | 256幀／1,665點 | 同左 | 同左 |
| 自行修補 trace 對舊基準決策／原 PNG audit | 256幀／1,665點 | 同左 | 同左 |
| 所選 PNG 前後 SHA 與完整 replay | 256/256 | 256/256 | 256/256 |
| eligible／fake Down、Move、Up | 全為0 | 全為0 | 全為0 |

1,665是9 queries×185的 query-local 取樣，不是1,665個獨立物件或人工 gold。
Release 另做兩個注入控制：只改副本 `allowed_targets`，audit native=1 拒絕
`audit decision field changed`；只改副本一個 RGB，native=1 拒絕
`audit raw PNG pixel mismatch`。两者 runner=1／verification=0，原 PNG／oracle 未改。

Release 初次 `prefix-bridge-withdraw-release-01` 的 native 斷言通過，但 root 退出時
一個 member image 查詢回傳5，身分 unknown；runner=125／verification=1，安全 STOP
保留。owned cleanup 核實 final0／held signaled。沒有忽略此失敗；使用相同 binary、
fresh `-02` root 通過，安全核心未放寬，舊 STOP 未重開。

凍結 v3 與正式 source 未變，本次診斷修補沒有重跑整份349項 root CTest或3938斷言。
前一里程碑的原套件36 fail（effect2、capacity24、raw-body8、raw-hit-order2）、
Debug／ASan 各1個 CTest timeout、三配置各2 skip，繼續保留；不能稱 aggregate 全綠。

## 人工語義與成本限制

使用者回答「可確認是 head 前緣」：只確認 frame3030 約 `(495,237)` 的矩形下緣是
該 Hold 可見 head 前緣。原圖 SHA、reviewer、問題與答案見
[單筆人工標註](HUMAN_REVIEW_20261006.json)。原 observer front為 `(495.5,228)`；
這個落差尚未修成策略。近似座標不能作精確觸點；full body depth78不能作 head 厚度。
1519 candidate4／5的同一物件身分、實際 contact／completed 狀態、合法機會分母及
body/tail採納仍 unknown。所有 frozen selection／oracle 的舊標籤保留。

Release全部256幀 current-host QPC compute，p50／p95／p99／max為
8.30385／25.964575／32.64545／36.7595ms，jitter p95−p5=20.029125ms，失敗0。
PNG IO加compute為18.23905／43.453075／50.443275／58.9101ms。
原29e3ed0的7.2014／12.631025／15.43637／19.5908ms亦保留，不挑最快結果。
這是順序離線短窗；compute不含dispatch後診斷寫檔及最終釋放，business clock為
離線FakeClock，零 eligible；不是 A/A→A/B、Windows 即時完整鏈或正式 owner 成本資格。
Debug／ASan數字另存，ASan replay與 audit有執行重疊，不拿其時間作效能比較。

latest-frame producer／RPC／journal壓力、production hook、裝置當前指紋、
Fixture真觸控、遊戲採納及Chapter Legacy分母／IN解鎖均未跑／未核，成本 NOT_READY。

## 可重驗證的封存與後續門檻

新證據：[SHA256索引](withdrawal-acceptance-evidence-01/SHA256_INDEX.json)、
[程序／source／容量／外部原件核驗](withdrawal-acceptance-evidence-01/verification-and-provenance.json)。
含所有本輪成功、負控制與 STOP 收據、freeze、命令、報告、trace、兩個控制副本，
原 child source／原稿快照及774f3e7 source。binary只記外部路徑／SHA，不複製原圖、
binary、依賴。新 out上限2GiB、metadata16MiB、free至少20GiB；舊封存與容量帳本不改。
本次封存535檔（534檔由索引列出、索引排除自身），12,324,539 bytes；三個新
build roots共1,620,143,812 bytes。重核5,346個 freeze項目／356份獨立凍結檔、
2,111份舊metadata與71份原 STOP，256張 PNG／manifest／index仍相符；原main
HEAD與AUX使用者修改SHA不變。24個 native stages均已退出並核實held handles／final0，
包含上述程序 STOP 與兩個預期拒絕，沒有把 native0單獨當驗收。

接下來的三個具體門檻：

1. 從當前像素推导有界 typed head boundary，區分 reconstructed front、head及body；
   不把人工近似點、固定9px或舊按鍵輸入策略。
2. 裁定1519物件身分，保留 unknown／proposed；有可裁定的合法物件與接觸前綴才
   凍結新的色彩／front候選，避免只放寬 predicate。
3. 以 exact 新候選完成完整 owner／scheduler、自身 Down/Move/Up、latest-frame及
   成本／失敗情境門檻，再列明裝置、命令、風險與停止條件取得裝置授權。

本地程式與離線測試的授權持續有效。沒有 push、PR、merge、付費運算、系統安全變更、
裝置讀取或遊戲操作。此里程碑完成撤回驗收與必要診斷修補；產品目標仍未完成。
