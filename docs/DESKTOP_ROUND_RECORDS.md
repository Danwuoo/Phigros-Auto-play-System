# 桌面回合紀錄

2026-11-07 已安裝 `20261107-native-records-026` 與前端 `20261107-025703-617872-7fef4705`。原有曲庫、校正與相位設定沿用部署前的參照。舊啟動設定與前端入口的原始備份在 `local/manual-config-history/`，部署報告使用同一前端版本字首。

## 查閱紀錄

桌面面板的「開啟紀錄」可在待機時查詢過去回合，以曲名、ID 或難度篩選。選取一輪會顯示主要停止原因、清理錯誤和釋放狀態，並可開啟原始資料夾或既有結算圖。

從專案根目錄執行下列指令，可離線開啟同一個歷史視窗。

```powershell
$configPath = (Resolve-Path -LiteralPath .\local\manual-launch.json).ProviderPath
$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
pwsh -NoProfile -STA -File $config.gui_script.file -Configuration $configPath -HistoryOnly
```

命令列也可直接查詢。

```powershell
.\.venv\Scripts\python.exe -B -X utf8 .\tools\round_history.py --track GOODRAGE --difficulty IN --limit 10
```

在已確認結算且原始 manifest、結算圖指紋均可驗證的桌面回合，可手動補登四項判定。將下例的回合 ID 及數值換成結算頁上讀到的資料。

```powershell
.\.venv\Scripts\python.exe -B -X utf8 .\tools\round_history.py --review '<RUN_ID>/round-01' --perfect 500 --good 5 --bad 1 --miss 2
```

人工判定另存於 `local/manual-panel-001/score-reviews/`。歷史保留原生判定狀態、人工來源與相對應的原始指紋。

## 六輪離線驗收

由操作者在專案根目錄執行。指令只讀 2026-11-03 的六輪原始資料，逐輪列出 PASS 或差異。六輪都通過時 exit code 為 0。

```powershell
.\.venv\Scripts\python.exe -B -X utf8 .\tools\round_history.py --accept-nov03
```

| 當地時間 | 曲目 | 預期可查閱的狀態 |
| --- | --- | --- |
| 15:13 | BANGING STRIKE AT | 0 步，來源時戳跳變，另有收尾 ADB 逾時，釋放未確認 |
| 15:17 | BANGING STRIKE AT | 0 步，舊首音拒絕原因有歧義，已確認歸零 |
| 15:21 | ERABYECONNEC10N IN | 10,044 步，已結算、已確認歸零，判定待複核 |
| 15:26 | ERABYECONNEC10N AT | 5,527 步，已結算、已確認歸零，判定待複核 |
| 15:30 | GOODRAGE IN | 0 步，舊首音拒絕原因有歧義，已確認歸零 |
| 15:31 | GOODRAGE IN | 3,395 步，已結算、已確認歸零，判定待複核 |

## 新紀錄的時間與證據

- 啟動狀態另存於各次 `local/startup/<launch_id>/status.json`。面板帳本關聯啟動、佇列和回合 ID，記錄 READY、HUD、首音、輸入完成及停止等 UTC 觀察時間。
- 原生 `desktop-summary.json` 分列主要錯誤、清理錯誤與實際首音閘門條件。其時間屬於主機 QPC。首音 `evaluated_host_ns` 是判斷當時使用的時刻，`lead_ns` 是該時刻距預定觸控時間的差值。完整 manifest 保留接點、串流寫入與釋放證據。
- 桌面原生 stdout 保存精簡摘要，省去重複的完整 manifest。前檢與一般原生 CLI 保持舊格式。新桌面回合的影像只保存確認後的 `result.png`。

程式檢查已通過可攜 CTest 6 項、Windows 原生 CTest 38 項、面板自測 107 項，以及歷史合成資料 9 項。安裝後已核對離線啟動計畫及 389 譜面設定。真實六輪離線驗收留給操作者執行。桌面視窗和模擬器遊玩未測。

## 本機磁碟盤點

確認可重建的最大暫存是 `build-local-native/vcpkg_installed/vcpkg/pkgs`，共 4,534,299,584 bytes。它是 vcpkg 套件 staging，與仍在使用的同層套件狀態及 `vcpkg_installed/x64-windows` 安裝前綴分開。自動執行環境拒絕遞迴刪除，所以磁碟尚未釋放該空間。舊遊玩、前檢、校正及回退資料保留原件。
