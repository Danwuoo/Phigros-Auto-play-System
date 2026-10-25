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
| 完整 Python unittest | **222／222 通過，0 skips** | 當輪全 suite，包含 PowerShell 與 LZ4 可用路徑 |
| Flick 原生 policy 檢查 | **61 checks 通過** | default、型別／範圍、compiler、fingerprint、首 DOWN／opening |
| 新 Flick Python 工具 | **21 tests 通過** | receipt 診斷與單參數 trials；包含在全 suite |
| 曲庫下載與索引 | **28 tests 通過** | 固定來源、續傳、錯誤原件、身份與路徑；包含在全 suite |
| 原生稽核工具 | **17 tests 通過** | timeout、hash 變動、輸出界線、v1 等；包含在全 suite |
| 曲庫 PowerShell 內建 SelfTest | **14 checks 通過** | headless 資料與狀態 |
| 曲庫 Python→PowerShell | **11 tests 通過** | 真正 PowerShell 資料層與 runspace 相對路徑；包含在全 suite |
| build-local Portable helper | **實際執行成功，4／4 CTest** | 不只驗證印出的命令 |
| 新 PowerShell 入口 | parser／AST 通過 | 完整 GUI 實際版面另列 |

### 本輪修正的跨平台測試問題

1. `supervisor_tests.cpp` 的匿名 fixture `sync()` 與 POSIX 同名函式衝突，改為 `sync_observation()`；只有測試名稱變更。
2. 既有 v2 fingerprint 將標準函式庫 `hexfloat` 字串納入 hash。MSVC 對 1.0 輸出 `0x1.0000000000000p+0`，libstdc++ 輸出 `0x1p+0`，造成原本只釘 Windows golden 的測試在 Linux 失敗。實際擷取未改的序列化 transcript，將 GNU mantissa 補成 13 位後逐一重現原 Windows 的 strict／dynamic hashes。測試保留兩組已重現 golden，依字串格式選擇；production fingerprint v2 與 Windows 舊封存 hash 都沒有改。

`docs/EXTRACTION.json` 的 14 個保留 blob 中，13 個程式／設定檔仍逐 Git blob 相符；`.gitattributes` 僅新增本輪 LFS 與新腳本規則，因此不再宣稱該檔原 blob 相同。`proto/`、`third_party/` 與既有 `tools/sweep-panel.ps1` 均未改。沒有修改 preserved PAS core、transport 或 contact compiler。

## 使用端實際驗證

使用新 GUI 同一背景資料路徑讀取真實 327 首／389 譜面、套用相符的 356／5／28 native 報告，並對一組真實原譜／音源完成 SHA256 核對。失配／過期報告會拒絕套用，LFS pointer 不當成可開啟的原件。

Windows WinForms 兩種尺寸的 render 與 portable build 交由本輪 Actions 的 Windows job；初始程式碼提交時標記為 **PENDING**，遠端完成後在本頁補記實際 run／commit。人工點擊、實際螢幕 DPI／多螢幕與 native 遊戲流程另需本機核對。

## Git LFS 與分支發布狀態

初始程式碼提交會帶入已完成的 native report，接著由 `catalog-lfs.yml` 重建相同來源曲庫、核對 report 綁定的 library／source lock SHA，再上傳 716 個原譜／音源 LFS 物件，最後正常推送資產 commit。Windows job 釘住 archive job 實際發布的 commit SHA，不追蹤浮動分支。

**初始程式碼提交時：PENDING 遠端 LFS 傳輸與下載回驗。** 只完成本地完整下載不算遠端入庫成功；後續需記錄 Actions run、資產 commit、LFS 物件存在與代表性下載 SHA。

## 明確未驗證／未完成

- 新 Windows native `chart_experiment` 主程式與 patched gRPC 全套整合建置：本輪雲端 **NOT RUN**。
- 既有本機私有 source manifest／WAV、calibration、kernel／capacity、archive 的 `check-adaptive`：**NOT RUN**。
- 新 Flick 候選的實際遊戲結算與 AP：**NOT RUN**；沒有證明共同漏判根因。
- 公共 4.0.1.2 與安裝 4.0.1／157 的逐曲身份對應、AT 共用 HUD、Random 變體選曲綁定：未完成。
- v1、新欄位、strict 拒絕及超出目前接點／時間預算的譜面：保留原件與阻擋，未假裝可執行。
- 舊結算漏記、20 ms lateness、缺錄與未測曲目：保留原本狀態，未宣稱修好。

本機後續使用 [HANDOFF_LOCAL_6_1_SOL_XHIGH.md](HANDOFF_LOCAL_6_1_SOL_XHIGH.md)。
