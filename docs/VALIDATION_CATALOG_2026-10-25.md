# 2026-10-25 全曲曲庫、使用端與 Flick 驗證

基線：`3e66ab7a26d64a40289573fe0c5ab3f247b1c1c2`，目標分支 `feature/chart-driven-multigame`。本文件區分本輪真實完成的離線檢查、遠端發布與尚待本機補驗的範圍。原本 2026-10-24 的 Windows 28／28 CTest、145／145 Python 與遊戲 AP 仍屬該輪原件，不借用為本輪通過證明。

## 來源與完整性

| 項目 | 本輪結果 |
| --- | --- |
| 固定上游 | `7aGiven/Phigros_Resource`，三個 commit 見 `configs/song-catalog-sources.json` |
| 歌曲／譜面／音源 | 327／389／327 |
| IN／AT／額外 IN 變體 | 333／56／6 |
| 完整下載 | 719／719（包含 3 metadata） |
| 重新驗證 | 719／719；0 缺項、0 來源 Git blob 或 bytes 不符 |
| 譜面格式結構分析失敗 | 0；5 份 v1 明確保留為不受 native v3 支援 |
| 遊戲可實測資格 | 0 新增；全部公共來源 chart 保持 `live_ready=false` |

原譜和音源逐檔依固定 Git blob SHA 與長度核對，再保存 SHA256。來源版本為 4.0.1.2，既有實測版本為 4.0.1／157；完整下載不能替代兩版本的同一性驗證。

| 固定產物 | SHA256 |
| --- | --- |
| `library.json` | `555bc99aecd340c993520c5270a948a91ecc251c7276fbde828118e4f4a090b3` |
| `manifests/source-lock.json` | `cb4743f4c8a7cd284760ecacbe869633e2708624b2a291e0199774a5bce99af4` |
| `manifests/native-import-report.json` | `5f1f57e0e32746666e4dd92b7bb4f323b4b42e479118a6baa3a29f85b0357cea` |
| 本輪 Linux `chart_inspect` | `20e714671069286178782e0e9779d83dd147312513ed18daa9d7fde6a730d8b1` |

## 全庫原生匯入

實際執行 `tools/inspect_catalog_native.py`，對全部 389 份原件呼叫上述 inspector，以 `phigros-v3-strict-v1` 稽核，包含執行前後原件／工具／library／來源鎖定雜湊核對。

| 結果 | 數量 |
| --- | ---: |
| passed | 356 |
| unsupported_format（v1） | 5 |
| rejected | 28 |
| timeout | 0 |
| integrity_error | 0 |
| inspector_error | 0 |

28 份拒絕包括 13 份 `reversed event interval` 與 15 份 `unsupported field in strict v3 dialect`。報告 `status=complete`、`all_passed=false`，完整列出每個 library ID、chart SHA、exit 與原生錯誤；來源 bytes 未改。

## 本輪建置與測試

環境：Linux x86_64，GNU C++ 13.3.0、CMake 4.4.4、Ninja 1.13.2、nlohmann_json 3.12.0、Python 3.12、PowerShell 7.6.8。CMake build 使用 `-Wall -Wextra -Wpedantic -Werror`。依賴安裝在任務隔離目錄，沒有提交工具鏈或 generated binary。

| 檢查 | 結果 | 範圍 |
| --- | --- | --- |
| 預設 portable CTest | **4／4 通過** | 無 native backend／裝置 |
| importer＋geometry／contacts＋visual＋supervisor CTest | **13／13 通過** | 含新 `flick_policy_contracts` |
| Windows MSVC portable CTest | **4／4 通過** | Visual Studio 18 2026／MSVC 19.51.36260.0 |
| Windows MSVC importer／Flick 整合 CTest | **13／13 通過** | SHA 釘住 nlohmann_json 3.12.0；含原 Windows fingerprint goldens 與新 Flick policy |
| 完整 Python unittest | **223／223 通過，0 skips** | 最終全 suite，包含 PowerShell、LZ4、相對路徑與六欄資料回歸 |
| Flick 原生 policy 檢查 | **61 checks 通過** | default、型別／範圍、compiler、fingerprint、首 DOWN／opening |
| 新 Flick Python 工具 | **21 tests 通過** | receipt 診斷與單參數 trials；包含在全 suite |
| 曲庫下載與索引 | **28 tests 通過** | 固定來源、續傳、錯誤原件、身份與路徑；包含在全 suite |
| 原生稽核工具 | **17 tests 通過** | timeout、hash 變動、輸出界線、v1 等；包含在全 suite |
| 曲庫 PowerShell 內建 SelfTest | **14 checks 通過** | headless 資料與狀態 |
| 曲庫 Python→PowerShell | **12 tests 通過** | 真正 PowerShell 資料層、runspace 相對路徑與精確六欄；包含在全 suite |
| build-local Portable helper | **實際執行成功，4／4 CTest** | 不只驗證印出的命令 |
| 新 PowerShell 入口 | parser／AST 通過 | 完整 GUI 實際版面另列 |

### 本輪修正的跨平台測試問題

1. `supervisor_tests.cpp` 的匿名 fixture `sync()` 與 POSIX 同名函式衝突，改為 `sync_observation()`；只有測試名稱變更。
2. 既有 v2 fingerprint 將標準函式庫 `hexfloat` 字串納入 hash。MSVC 對 1.0 輸出 `0x1.0000000000000p+0`，libstdc++ 輸出 `0x1p+0`，造成原本只釘 Windows golden 的測試在 Linux 失敗。實際擷取未改的序列化 transcript，將 GNU mantissa 補成 13 位後逐一重現原 Windows 的 strict／dynamic hashes。測試保留兩組已重現 golden，依字串格式選擇；production fingerprint v2 與 Windows 舊封存 hash 都沒有改。

`docs/EXTRACTION.json` 的 14 個保留 blob 中，13 個程式／設定檔仍逐 Git blob 相符；`.gitattributes` 僅新增本輪 LFS 與新腳本規則，因此不再宣稱該檔原 blob 相同。`proto/`、`third_party/` 與既有 `tools/sweep-panel.ps1` 均未改。沒有修改 preserved PAS core、transport 或 contact compiler。

## 使用端實際驗證

使用新 GUI 同一背景資料路徑讀取真實 327 首／389 譜面、套用相符的 356／5／28 native 報告，並對一組真實原譜／音源完成 SHA256 核對。失配／過期報告會拒絕套用，LFS pointer 不當成可開啟的原件。

Windows WinForms 兩種尺寸的 render 與 portable build 交由本輪 Actions 的 Windows job。首次 Windows job 的 10 項測試通過，1 項 fixture 路徑比較因 `RUNNER~1` 與 `runneradmin` 指向同一個目錄而誤拒絕；production resolver 正常。提交 `21307ae7f4d93b29d61ce052949a37154d770d07` 改以 `os.path.samefile` 比較既存檔案身份，仍核對尚未建立的 preview 尾段與 root，並新增獨立 Windows workflow，重驗不再重抓全庫。

Windows run `40166716343` 的 11 項使用端測試、14 checks SelfTest 與 MSVC portable 4／4 CTest 通過，並產生兩張圖。但人工看圖發現 PowerShell 陣列加法優先序造成列表欄位位移，頂層 Form 又受 runner 桌面大小壓縮，導致詳情與按鈕裁切，這版未通過視覺 QA。

修正提交 `f32e9887e4142cfb58c804d510a2328179fb372c` 明確產生六格資料、加入逐格回歸、約束 TableLayout／FlowLayout 可用寬度並按實際寬度調整 splitter。預覽使用同一組正式控制項的非頂層畫布，記錄要求／實際尺寸與主要控制項邊界，超出範圍即失敗。

Windows run `40167315208` 的 12 項使用端測試、14 checks SelfTest、MSVC portable 4／4 及 importer／Flick 13／13 CTest 全數通過。列表欄位與所有操作區域的邊界也已正確；預覽尺寸檢查另抓到 child Form 仍被系統最大視窗尺寸限制，故該 run 整體維持失敗，預覽畫布需再修正。人工點擊、實際螢幕 DPI／多螢幕與 native 遊戲流程另需本機核對。

最終提交 `c918b8aa4cb5d9d8185f180ace9f29e4e7aab28f` 改以普通 Panel 擷取同一組正式控制項，離屏 Form 保留原生 ComboBox 文字繪製所需的可見祖先。後續 [Windows run 40167707422／job 121092036624](https://github.com/Danwuoo/Phigros-Auto-play-System/actions/runs/40167707422/job/121092036624) **整體成功**：12 項使用端測試、14 checks SelfTest、portable 4／4、importer／Flick 13／13 及兩尺寸預覽全部通過。

下載 artifact `12588014853`，ZIP SHA256 為 `533e04b0f5d268467d7d896df4cc6838f60a2b60db61cd37ebef428d96d84acc`。兩張 PNG 均已人工檢視：要求與實際尺寸一致、每張 13 個主要控制項邊界全部在可見範圍，難度／變體／狀態選單文字正常，六欄資料與音符數正確，詳情可捲動、五個動作及底部兩個面板按鈕完整顯示。

| 圖片／證據 | SHA256 |
| --- | --- |
| [1440×900 曲庫](images/catalogue-client/catalog-wide.png) | `1d49510196893b70eb904be1bb6347fcca13d89468d85a5937e271f2d95aa301` |
| [1080×720 曲庫](images/catalogue-client/catalog-compact.png) | `dc0009595558c959e1515fbff0263d0225480aa70101247d2b3cccf3e67e1b28` |
| [preview-manifest.json](images/catalogue-client/preview-manifest.json) | `28eed8ec99c29e57ce2f74e588034d25847ed150ea23f19d07210829c0bb116d` |

CI checkout 刻意不 hydrate LFS，所以圖中顯示「待取回 LFS 原件」；遠端原件已另行驗證。這些是實際 Windows 控制項的離線畫面，不代表已執行原生遊戲流程。最終變動僅涉及預覽容器，完整 Python 223 項在此前版本通過，隨後另重跑本地 12 項使用端測試及上述 Windows 全部檢查。

## Git LFS 與分支發布狀態

程式碼提交帶入已完成的 native report，接著由 `catalog-lfs.yml` 重建相同來源曲庫、核對 report 綁定的 library／source lock SHA，再上傳 716 個原譜／音源 LFS 物件，最後正常推送資產 commit。Windows job 釘住 archive job 實際發布的 commit SHA，不追蹤浮動分支。

**LFS 已發布並完成遠端回驗。** 資產 commit：[`9ad6700678cc8ddbb259a5611a9be74cd9f6c5a2`](https://github.com/Danwuoo/Phigros-Auto-play-System/commit/9ad6700678cc8ddbb259a5611a9be74cd9f6c5a2)。成功的 archive job：[run 40166307374／job 121087962656](https://github.com/Danwuoo/Phigros-Auto-play-System/actions/runs/40166307374/job/121087962656)。該 run 後續 Windows fixture 比較失敗另行修正，不能把整個 run 說成全部通過。

- 真實 `git lfs fsck`、`git lfs push origin HEAD` 與 branch push 均成功。
- 重新 fetch 資產 commit，716 份 remote Git pointers 的 OID／size 全部與本地已核對原件相符，0 mismatch；remote `library.json` 與本地逐 bytes 相同。
- 使用匿名 LFS download batch 對 716 個物件逐一實際 Range GET，全部可讀且長度相符，0 errors；沒有借用 credential store 或存取 token。
- 額外完整下載 FULi IN、Credits AT、Random `.1` IN、EnginexStartmelodymix OGG，4／4 大小與 SHA256 相符，共 4,503,523 bytes。這四個是完整 hash 回驗；其餘 712 個是可讀／長度回驗，不冒充整庫再次完整下載。
- 完整逐物件證據見 [remote-lfs-verification.json](../assets/phigros/4.0.1.2/manifests/remote-lfs-verification.json)，SHA256 `4fe5cfe80703e5deb2900c38ae7e4c91de2a332bc99a2f806c3643a6df87b301`。其輸入的本地核對報告另存 `manifests/source-verification-local.json`；Actions 產生的 `verification.json` 保留其自身時間與記錄。

首次程式碼提交為 `af428081bf4c6c61bf18a5579fc52c836424b630`。Actions run `40166096046` 已完成 719 份下載／重驗與 native report SHA 綁定，但 `git lfs push origin HEAD` 因 SHA checkout 沒有任何本地 branch，回傳 `Error getting local refs`。此時尚未上傳 LFS／推送資產 ref，沒有留下懸空 pointers。

修正提交 `5895de79a70ffd239cef8d756e778d884236520b` 在固定 SHA checkout 後建立僅供 runner 使用的本地 `catalogue-lfs-transfer` branch，仍保留原本遠端 SHA lease、物件先上傳與非 force push。以獨立本地 fixture 實際重現 detached 狀態 exit 2，建立本地 branch 後相同 LFS dry-run exit 0；修正後由新 workflow run 重做發布。

## 明確未驗證／未完成

- 新 Windows native `chart_experiment` 主程式與 patched gRPC 全套整合建置：本輪雲端 **NOT RUN**。
- 既有本機私有 source manifest／WAV、calibration、kernel／capacity、archive 的 `check-adaptive`：**NOT RUN**。
- 新 Flick 候選的實際遊戲結算與 AP：**NOT RUN**；沒有證明共同漏判根因。
- 公共 4.0.1.2 與安裝 4.0.1／157 的逐曲身份對應、AT 共用 HUD、Random 變體選曲綁定：未完成。
- v1、新欄位、strict 拒絕及超出目前接點／時間預算的譜面：保留原件與阻擋，未假裝可執行。
- 舊結算漏記、20 ms lateness、缺錄與未測曲目：保留原本狀態，未宣稱修好。

本機後續使用 [HANDOFF_LOCAL_6_1_SOL_XHIGH.md](HANDOFF_LOCAL_6_1_SOL_XHIGH.md)。
