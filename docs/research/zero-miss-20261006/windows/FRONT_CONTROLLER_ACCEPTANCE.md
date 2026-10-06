# Hold 當前像素終端候選：總控獨立驗收（2026-10-06）

**通過隔離、離線幾何量測里程碑；正式觸控、完整成本與全曲 zero miss 仍未驗收。**
本 task 依使用者「驗收」指示直接 review、重建與重跑，未另派 task、監控、建立
automation 或執行裝置命令。交付來源為 GPT-6.1 Sol／xhigh task
`01a10fdd-1f46-7e71-a0ad-ed5b808388c3`，實作 commit
`e26231ec5acef9634df007770c1f06031d5326d4`、交付
`055ec799d53ccc842ee7a2fa3033369ccc389f8d`。

驗收位於 `C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`，另立本地分支
`codex/hold-front-acceptance-20261006`。原 worker 分支、初版 `28493f1`、歷史失敗與
封存保留；未修改候選程式以追綠。交付說明见 [FRONT_CANDIDATE](FRONT_CANDIDATE.md)。

## 獨立核對與 review

前後均核 SHA256 與 Git blob：本次 worker 封存 749 entries／750 files、前兩包
2,645 entries；34 個既有 binary、12 個 linked libraries、三份負控制完整 trace、
256 張原 PNG、原 index／manifest，以及三配置共 764 筆 build-input freeze 相符。
最終九份 source snapshot 與 working tree／Git 精確相符；初版七份 source snapshot
也與 `28493f1` Git blobs 相符。詳見
[前後核對](front-controller-acceptance-evidence-01/controller/audit-after-02.json)。

`src/include`、根 CMake、原 tests/oracle、`research/bvi_cold_v3`、原 Windows bridge、
qualified process wrappers 相對父基準 `4fe3884` 無差異；七份 frozen-v3 core SHA 相符。
原 checkout main 仍 `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`，AUX 修改與 ZIP 的
原 SHA 及 dirty 列表不變。

review 核實：幾何、storage、context、容量與 pixel 索引先檢查；body patch 不能投影
head；tail proposal 只定法向符號；邊界需五組相鄰 inside／outside RGB 與 rails 支持，
不是把 rail 最末點直接當 head。typed consumer 在 same current frame 重新量測並比對
完整 proof；結果仍 `head_role_confirmed=false`、`action_authorized=false`。
新 front sidecar 位於原 owner dispatch 後，只進 fresh-v3 extract-only shadow；
annotation、ordinal、舊動作不進 producer，shadow 不接 relate／guard／owner。
未發現阻擋此隔離量測範圍簽收的程式問題。

本 audit 以獨立 WIC 解碼核所有正結果的當前原像素支持與完整 offline context，
並比較原決策欄；它不為 unknown 逐筆提供人工物理分類，也不证明每個拒絕都没有
合法遊戲機會。proposed 數量與 physical objects／legal opportunities 的分母不同。

## 總控實際重跑

沿未改 qualified wrappers，在全新 `out/hold-front-release-controller-01` 完成
Release configure／build。Debug／ASan 使用已逐檔核 SHA 的交付 binary，另開新測試
收據，沒有用新 Release 代替 sanitized owner。

| 配置 | 新量測契約 | 未改 owner／scheduler 前綴 | 本次原圖重跑 |
|---|---:|---:|---|
| 新建 Release | 56/56 | 113/113 | 256/256，獨立原圖 audit 通過 |
| 交付 Debug | 56/56 | 113/113 | 未重跑；交付原件與逐幀欄位已獨立核對 |
| 交付 MSVC ASan | 56/56 | 113/113 | 未重跑；交付原件與逐幀欄位已獨立核對 |

所有 256 列原決策及完整新 sidecar geometry，與 worker Release／Debug／ASan
以及本次首輪 STOP 的 trace 相同；只排除五個原 host 時間欄及兩個新 sidecar 時間欄。
見 [跨配置比較](front-controller-acceptance-evidence-01/controller/trace-comparison.json)。
Release 新 binary SHA 不要求與另一 build root 相同；本次 `pixel_chain.exe` SHA256 為
`5c317f6398862ccfb560d333b7bf22eaf32588d3d56756a4bb3dbc5f424102ef`，source／四個
linked libraries 的來源、配置、freeze 與內容均已核實，綁定見
[source／binary manifest](front-controller-acceptance-evidence-01/controller/source-and-binaries-external.json)。

全分母仍為 859 raw proposals：336 Hold 中 191 個 visible terminal proposed、
145 個 unknown（89 tail normal 不足、54 no terminal、2 multiple terminals），
另外 523 個 Hold-only unsupported。audit 核 1,910 個新相鄰取樣與 1,665 個舊 witness
取樣。frame3030 candidate2 的區間為 y=[236,237]、代表點=(495.5,236.5)，與使用者
約 (495,237) 的 head 前緣語義確認相容；標註仍外部保存，沒有變成精確觸點或合法 Down
gold。1519 的 Tap physical identity 未裁決。

三份負輸入獨立比對全部 256 列，確認各只改3030candidate2一個指定欄位。新建
Release audit 分別拒絕 epoch、geometry 與 inside RGB 竄改：native=1／runner=1／
verifier=0；負報告的 `passed=false` 是預期拒絕，未改寫成正測試成功。
完整鏈仍 eligible=0、自建 fake Down／Move／Up=0、final release verified。

## 失敗、STOP、成本與未跑

本次共 14 native stages：13 COMPLETE（含三個預期拒絕控制）、1 STOP。首輪
`pixels-hold-front-release-controller-01` 因 `member-image:5` 得
native=0／runner=125／verifier=1，雖產出256列仍不計通過。其 active_final=0、
held_all_signaled=true、streams_completed=true；原 STOP 不重開，另建 controller-02
取得完整成功收據。worker 原33 stages 的30 COMPLETE／3 STOP也逐份核實保留。
本次所有stage均收尾，沒有猜 PID 終止或修改安全設定。

總控 file-only 維護核對亦保留五份早期錯誤報告：舊索引選擇、external manifest
結構、PowerShell布林寫法、STOP收據分欄、初版source檔案數；修正後前後核對通過。
封存維護腳本的保留變數衝突另有錯誤紀錄及原腳本副本。這些沒有改候選或 oracle，
也沒有把維護失敗刪掉。

本機環境沿交付 Windows11／MSVC19.51／CMake4.2.1／既有離線依賴；本次順序重跑
實際 host QPC（全部256幀，ms）：

| 範圍 | n | p50 | p95 | p99 | max | jitter p95−p5 |
|---|---:|---:|---:|---:|---:|---:|
| 原鏈 compute | 256 | 17.035500 | 26.047975 | 29.647005 | 34.257200 | 13.258550 |
| 新 front sidecar compute | 256 | 0.167150 | 12.838525 | 13.232885 | 13.624000 | 12.799475 |

sidecar 時間涵蓋 producer、consumer重核、fresh-v3 shadow與JSON構造；二者不含
dump／flush、final release、latest-frame producer、RPC或live injection，不能作
完整鏈成本資格或相加stage p99。producer上限124,800 probes／batch，consumer重核
另有最多124,800；兩者合計上限249,600，shadow另計。沒有有效 A/A→A/B／stress。

本次未重跑不變的原3938／root349；原36 fail、Debug／ASan timeout、skip 均保留，
沒有宣稱 aggregate 全綠。未跑完整接觸採納前綴、Hold接入／body持續／tail結束、
正式runtime hook、latest-frame／RPC／journal全鏈、實機指紋、遊戲版本／Chapter
Legacy完整分母、IN解鎖與逐曲IN Miss=0。沒有 Windows UBSan／leak 資格。

下一個必要工作仍是物理語義／合法機會分母與當前色彩契約，再凍結接觸候選、驗自身
完整前綴及全鏈安全／成本。到需要裝置讀取或操作時，按原規定另取指定裝置授權。
本驗收不啟動後續遊戲或自動續派。

## 封存

新驗收封存322 files／321 indexed entries、5,235,402 bytes，index SHA256：
`1dd569bb51f2ee7337a199e90a70240f198db3ae7b6ff6d50d893735ffbd9a07`。
[驗收機讀結果](front-controller-acceptance-evidence-01/controller/acceptance.json)與
[SHA index](front-controller-acceptance-evidence-01/SHA256_INDEX.json)包含新／失敗收據、
原圖trace、正負報告、維護錯誤、建置maps及source／binary外部SHA。未複製原PNG、
binary或依賴。新OUT≤512MiB、metadata≤16MiB、free≥20GiB，獨立容量帳保留；
worker與歷史配置／容量／seal均未覆寫。封存後322份原始位元組另與staged Git blob
逐檔核實；收據保留原CRLF，格式檢查限本次編輯的文件，不為格式檢查改寫凍結證據。
