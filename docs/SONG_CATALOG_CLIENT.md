# 全曲譜面曲庫使用指南

新入口是 [tools/start-song-library.ps1](../tools/start-song-library.ps1)。它開啟 Windows 桌面曲庫，可搜尋、篩選、檢視來源，以及核對並開啟譜面與音源。瀏覽曲庫不需要 native build、模擬器或本機觸控設定。

這份固定公共來源快照包含 **327 首歌曲、389 份 IN／AT 譜面、327 份音源**，其中 IN 333 份、AT 56 份。IN 數量包含原生變體，因此不等於歌曲數量。來源標示為 **4.0.1.2**；既有本機實測基準是 **4.0.1／157**，兩者尚未逐項核對，不會因為已入庫而自動取得本機實測資格。

## 1. 本機需求

- Windows。
- **PowerShell 7.2 或更新版本**，執行檔為 `pwsh.exe`。Windows PowerShell 5.1 不適用。
- 開啟視窗時使用 **`-STA`**。
- Git 與 Git LFS，用於取得分支及原件。

以下命令都從專案根目錄執行。如果 `pwsh` 沒有加入 PATH，可改成 `& '實際完整路徑\pwsh.exe'`。GUI 不需要先安裝 Visual Studio；C++ 原生實測程式的建置是另一個步驟。

## 2. 取得分支與 LFS 原件

在已存在的 checkout 中更新研究分支：

```powershell
git switch feature/chart-driven-multigame
git pull --ff-only
git lfs install --local
git lfs pull
```

譜面與音源使用 Git LFS 保存。`library.json`、來源索引及稽核報告是一般 Git 檔案，因此即使尚未下載全部原件，也能開啟曲庫查看清單與來源。

若只先取得譜面，可選擇性下載：

```powershell
git lfs pull --include="assets/phigros/4.0.1.2/charts/**" --exclude=""
```

之後再取得全部音源：

```powershell
git lfs pull --include="assets/phigros/4.0.1.2/music/**" --exclude=""
```

也可以只取得一首。例如 Credits 的 IN／AT 原件及音源：

```powershell
git lfs pull --include="assets/phigros/4.0.1.2/charts/Credits.Frums.0/**,assets/phigros/4.0.1.2/music/Credits.Frums.ogg" --exclude=""
```

下載完成後，在曲庫按 **F5** 或「重新整理」。重新整理會重讀索引及本機檔案狀態，不會自行下載檔案。

## 3. 開啟曲庫

使用預設的固定快照：

```powershell
pwsh -NoProfile -STA -File tools/start-song-library.ps1
```

也可明確指定索引：

```powershell
pwsh -NoProfile -STA -File tools/start-song-library.ps1 `
  -Library assets/phigros/4.0.1.2/library.json
```

載入完成後，上方顯示歌曲、譜面、IN 及 AT 總數。左側是可排序的譜面列表，右側顯示目前選取項目的身份、來源與尚待完成的工作。

### 搜尋與篩選

搜尋範圍包括曲名、作者、原生 track ID、歌曲 ID 與完整譜面識別。多個詞以空白分開，必須全部符合；搜尋採字面比對，因此 `[*]` 等文字不會被當作萬用字元。

可搭配三組篩選：

| 篩選 | 用途 |
| --- | --- |
| 難度 | 全部、IN、AT |
| 變體 | 全部或來源提供的 `.0`、`.1` 等原生變體 |
| 準備狀態 | 待本機核對、格式待支援、原生匯入拒絕、原件未齊備、LFS 尚未取回、取得／分析失敗 |

列表的每一列代表一份譜面。同一首的 IN、AT 或不同變體分別保留，不能用其中一份覆蓋另一份。沒有符合條件的項目時，按「清除篩選」即可回到完整清單。

### 常用操作

| 操作 | 行為 |
| --- | --- |
| 核對並開啟原譜 | 背景檢查 LFS pointer 及 SHA256；符合後，用本機預設程式開啟 JSON |
| 核對並開啟音源 | 背景核對來源音檔 SHA256；符合後，用本機預設程式開啟音源 |
| 所在目錄 | 開啟目前譜面所在目錄；該目錄尚未建立時，開啟曲庫目錄 |
| 來源網頁 | 開啟該項固定 GitHub 來源網址 |
| 複製識別 | 複製此版本／曲目／難度／原始 Git blob 的完整識別 |
| 重新整理 | 重讀索引、原生匯入報告及本機原件存在狀態 |

索引讀取、原件雜湊及本機面板入口核對都在背景執行。檔案核對期間仍可檢視其他項目；同時間只處理一項背景核對，不會同時建立多個讀取工作。

### 鍵盤操作

| 快捷鍵 | 用途 |
| --- | --- |
| Ctrl+F | 移至搜尋欄並選取現有文字 |
| F5 | 重新整理 |
| Esc | 清除搜尋與篩選 |
| 方向鍵 | 在列表選擇譜面，右側詳情隨之更新 |
| Enter | 列表取得焦點時，核對並開啟選取原譜 |
| Ctrl+C | 列表取得焦點時，複製完整識別 |
| Tab／Shift+Tab | 在搜尋、篩選、列表與操作按鈕間移動 |

雙擊譜面列，也會先核對原件再開啟。

## 4. 如何判讀狀態

**原件入庫、原生匯入、接點編譯及實機演奏是不同階段。** 此曲庫中的 `live_ready` 全部為 `false`；讀取器也會拒絕在公共曲庫中直接宣告實測資格。

| 顯示狀態 | 代表的範圍與接續動作 |
| --- | --- |
| 已入庫・待本機核對 | 原件已列入固定來源索引；仍須核對本機版本、譜面及設定 |
| 匯入通過・待本機核對 | 已有身份吻合的嚴格原生匯入報告；接點、HUD、時序及實機結果尚待驗證 |
| 已入庫・格式待支援 | 原件完整保留，但現行原生匯入器尚未支援該格式 |
| 已入庫・原生匯入拒絕 | 嚴格匯入未通過；右側會顯示繁中原因及原始技術細節 |
| 本機原件尚未取得 | 索引存在，但這個 checkout 尚未取得對應原件 |
| 待取回 LFS 原件 | 本機目前只有 Git LFS pointer；先執行相應的 `git lfs pull` |
| 本機原件無法讀取 | 檔案存在但無法開啟；檢查路徑、權限或其他程式的占用 |
| 原件取得失敗／已入庫・分析待處理 | 來源同步或分析留有失敗紀錄；查看阻擋原因後處理 |

本機檔案缺失或仍為 LFS pointer 時，列表會優先顯示這個可直接處理的狀態。已封存的匯入報告仍可在詳情中查看；「原生匯入拒絕」與「格式待支援」篩選也能找到這些項目。

### 嚴格原生匯入報告

目前固定報告位於：

```text
assets/phigros/4.0.1.2/manifests/native-import-report.json
```

此輪結果為：

| 結果 | 譜面數 |
| --- | ---: |
| 通過嚴格原生匯入 | 356 |
| 格式未支援 | 5 |
| 嚴格匯入拒絕 | 28 |
| 合計 | 389 |

已知拒絕原因包含起訖倒序的判定線事件，以及目前嚴格 v3 匯入器未支援的 `blockAreaList` 欄位。原件保持不變，沒有刪除事件或改寫譜面來取得通過。

GUI 只有在報告的 **library SHA256、每筆 library ID、chart SHA256 及相對路徑** 都吻合時才採用結果。報告缺少、已過期或身份不符時，仍可瀏覽曲庫，但詳情會明示沒有套用該報告。

## 5. 連結既有本機實測面板

若本機已有可用的 legacy `launch-config.json`，可以啟動時指定：

```powershell
pwsh -NoProfile -STA -File tools/start-song-library.ps1 `
  -Library assets/phigros/4.0.1.2/library.json `
  -LegacyConfiguration 'C:\你的本機封存目錄\launch-config.json'
```

也可以開啟曲庫後按「連結本機面板設定…」，選擇該檔案。入口核對完成後，再按 **「開啟既有實測面板」**。

這個串接會：

1. 確認設定屬於既有人工選曲／人工 Play 面板。
2. 核對設定指定的 `sweep-panel.ps1` SHA256。
3. 以新的 PowerShell STA 程序開啟原面板。
4. 由原面板繼續核對其曲庫、native binary、來源、校正及目前開機的容量證據。

曲庫目前選取的公共譜面**不會直接傳給原面板，也不會自動開始播放**。原面板仍使用原有 Legacy20＋Verrückt 候選、人工選曲／Play、READY、STOP 及釋放流程。原面板也不會因為連接了全曲瀏覽器而自動取得其他 IN 或 AT 的執行設定。

要把公共 4.0.1.2 譜面用於既有 4.0.1／157 環境，仍須先逐項比對本機原件，建立來源相符的 profile／接點計畫與 UI 綁定。AT 還需要對應難度的 HUD 驗證；現有 `shared-in-*` 政策不能直接套用 AT。原 native 的容量、時間、停止及釋放限制保持原有行為。

已有面板的操作與歷史證據見 [曲庫與視窗測試面板](SONG_TEST_PANEL.md)；最新本機封存組合見 [MODEL_HANDOFF](MODEL_HANDOFF.md)。

## 6. 離線自測與畫面預覽

### 純資料自測

使用便捷入口：

```powershell
pwsh -NoProfile -File tools/start-song-library.ps1 -SelfTest
```

或直接測試前端腳本：

```powershell
pwsh -NoProfile -File tools/song-library-panel.ps1 -SelfTest
```

此模式不載入 WinForms、不顯示視窗、不連裝置，也不啟動原生程序。它使用合成檔案驗證搜尋、身份、SHA、LFS pointer 與路徑限制；不是對全部遊戲原件重新做實測。

Python 契約測試可明確指定 PowerShell：

```powershell
$env:PAS_TEST_PWSH = (Get-Command pwsh).Source
python -m unittest discover -s tests -p test_song_library_panel.py -v
```

### Windows 畫面預覽

預覽模式直接呼叫前端，輸出目錄必須尚未存在：

```powershell
pwsh -NoProfile -STA -File tools/song-library-panel.ps1 `
  -Library assets/phigros/4.0.1.2/library.json `
  -PreviewDirectory outputs/song-catalog-preview-001
```

它會在背景載入曲庫，最多等待 40 秒，再建立離屏 WinForms 視窗並輸出：

- `catalog-wide.png`：1440×900 client 尺寸。
- `catalog-compact.png`：1080×720 client 尺寸。
- `preview-manifest.json`：實際圖片尺寸、SHA256、資料數量與驗證範圍。

PNG 會包含視窗邊框等繪製區域，所以實際圖片尺寸以 manifest 為準。預覽不能同時指定 `-LegacyConfiguration`，不啟動 native、模擬器或觸控。

## 7. 已完成與待驗證範圍

| 項目 | 此輪狀態 |
| --- | --- |
| Python → PowerShell 契約測試 | 11/11 通過 |
| 前端內建 headless 自測 | 14 checks 通過 |
| PowerShell 語法／AST | 通過 |
| 真實總庫背景讀取 | 327 首／389 譜面，IN333／AT56 相符 |
| 真實原生匯入報告綁定與篩選 | 356／5／28 相符 |
| 一組真實譜面及音源 SHA256 核對 | 通過 |
| Windows WinForms 實際畫面繪製與版面 QA | 待 Windows workflow 驗證 |
| Windows 人工點擊、高 DPI、既有面板串接的完整操作 | 尚未實測 |
| 新曲目或 AT 的實機演奏與成績 | 尚未實測 |

本指南的驗證結果對應新曲庫前端及此次固定來源資料。原有 AP、失敗回合、結算漏記與時序待修項仍按原始身份保存，不會因為曲庫更新而改寫。
