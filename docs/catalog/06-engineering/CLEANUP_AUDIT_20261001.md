# 整理與清理帳本（2026-10-01）

本輪在 main `9fea67e248411f7c2309f220daf989f652b27d15` 上研究及整理，**實際刪除211個可再生檔案：1,009,056,990 logical bytes（約1.01GB／0.94GiB）**。刪除清單的NTFS allocated bytes為1,009,578,464。原始實戰資料、凍結配套、失敗實驗與恢復worktree內容均保留；沒有清空整個out或measurements，也沒有啟動emulator／觸控／模型訓練。

所有明細保存在ignored的 `measurements/maintenance-20261001/`。研究新增輸出另在 `measurements/research-next-20261001/`，仍向原8GiB campaign加帳，不藉新目錄重設額度。研究結論見[現況報告](../01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md)。

## 1. 實際刪除與範圍

| 類別 | 限定路徑／來源 | 檔數 | logical bytes |
|---|---|---:|---:|
| 編譯中間產物 | main的out/debug-v145、out/asan-buildtools-v145、out/release-v145內`.obj`／`.pch`／`.idb` | 154 | 1,009,014,550 |
| 可再生CPU單元測試fixtures | out下9個vision-test-model／packet／rasterized目錄；來源為tests/vision_cpu_tests.cpp | 57 | 42,440 |
| 合計 | 僅manifest列出的檔案，之後移除空測試目錄 | 211 | 1,009,056,990 |

單元測試fixtures為整合CPU工具測試產生的臨時模型／資料包，與campaign中的模型checkpoint、proposal、失敗測試logs不同。舊build的exe／DLL／lib／PDB、CMakeCache、source、測試與建置logs全部保留；再次增量建置需要重新產生被刪的編譯中間檔。

刪除前將每個檔案的絕對路徑、大小、SHA256、NTFS file ID、link count與理由寫入[deletion-manifest.json](../../../measurements/maintenance-20261001/deletion-manifest.json)，SHA `cdd5e2fdac00d3f79911a1a56e0015af1d164c1cb81c97b18b384d1e05fe9021`。所有候選link count=1；對文件／scripts／provenance的引用搜索無命中。這只是輔助檢查，實際allowlist仍限制為上述可再生類別。

[delete-reviewed.ps1](../../../measurements/maintenance-20261001/delete-reviewed.ps1)在第一筆刪除前，核對所有resolved absolute path位於main/out、allowlist、大小與SHA、無reparse ancestor，並確認沒有PAS／有效建置程序。現有MSBuild的idle node reuse程序未被當作active build，也未終止它們。刪除使用同一PowerShell的`Remove-Item -LiteralPath`逐檔處理，沒有recursive delete；未知／新增檔案不在清單內就不動。

實際記錄見[deleted-files.json](../../../measurements/maintenance-20261001/deleted-files.json)與[修正後驗證結果](../../../measurements/maintenance-20261001/deletion-result-verified-v2.json)：211個路徑全部已不存在、記錄逐項與manifest相符。

## 2. 清理前後的保護驗證

使用本輪一次性的C++20／Win32 maintenance helper盤點main和恢復worktree，讀取SHA256、file ID、logical length、allocation及hard-link數；同file ID只hash一次。原始檔案逐項比較，不用「抽幾張PNG」代替整體保護驗證。helper source／exe／編譯產物留在audit/tool；未加入正式runtime或分析入口。

| 盤點範圍 | 清理前entries | 清理後entries | 前後同內容entries | 已刪 |
|---|---:|---:|---:|---:|
| main | 32,331 | 32,150 | 32,112 | 211 |
| baseline36-recovery | 10,410 | 10,410 | 10,410 | 0 |

main其餘差異是當時已編輯的5份文件與本輪起始為空、其後完成的3個新研究輸出；另新增30個輸出entries。`ARCHITECTURE.md`／`ROADMAP.md`的重整及新增報告在清理後inventory完成後寫入，因此盤點數字是**該次檔案快照，不是最終文件編輯後的工作區總量**。最終Git差異另外核對為文件，正式source／CMake／configs／tests未變。

[protection-verification-v2.json](../../../measurements/maintenance-20261001/protection-verification-v2.json)為valid=true、problems=[]。兩個root的before／after inventories皆errors=0。盤點排除`.git`內部、maintenance自身，遇reparse不跟隨；本批沒有reparse略過項。結果不含NTFS的MFT／其他filesystem metadata，也不冒稱是整顆磁碟精確使用量。

以下全部在保留且同SHA的範圍內：

- 兩輪跨曲原始sessions、能力報告、歷史observer36／18與33／13等獨立binary、舊main37／19比較配套，以及冷開發所有實驗／失敗記錄。
- 本campaign原12輪、恢復線各驗收、C36g-rec1全錄7722 PNG、index／journal／MP4／mapping、12段選片及使用者原文`REASONS.md`。選片3722份專用原圖與各clip hard links全部不改。
- C36h tint1 binary及freeze，原CPU工具／source freeze、packet-v1（superseded）／v2、proposals、三次合成訓練與final checkpoint、self-test／失敗logs。
- main整合與CPU build、原recovery build與LibTorch、全部已安裝vcpkg依賴。`out/vcpkg_installed`約11.40GB是已安裝依賴，本轮未當成可刪cache。
- 使用者的[判定線形式](../01-project/判定線形式.md)、[note形式](../01-project/note形式.md)，原始理由与raw均未編輯。

### 2.1 保留的帳目／工具修正記錄

本輪沒有隱藏維護工具的失敗與誤報：

1. inventory初版使用過大stack buffer而失敗，輸出為空；改heap buffer後產生`*-before-v2.jsonl`且errors=0，空的初版輸出保留，不拿它計數。
2. `deletion-result.json`的logical_bytes初次為null，原因是PowerShell對OrderedDictionary直接Measure-Object未取得屬性；詳細manifest與211筆deleted記錄完整。重新讀取序列化記錄後的確切總量見`deletion-result-verified-v2.json`，原摘要不覆寫。
3. 首次保護比較把本輪正在生成的frames／replay／log由空檔變成完整輸出列為異常。v2只明確辨識`research-next-20261001`的新產物，原campaign／其他檔案沒有放寬；最終無異常。首版report／script一併保留。

## 3. 容量與hard-link帳本

logical sum按每個目錄entry計長度；unique file bytes／allocated bytes按NTFS file ID去重。FileCompressionInfo的數值在未壓縮檔案常等於logical length，不能冒稱精確物理占用；下表使用FileStandardInfo的AllocationSize。目錄／MFT等metadata仍不在內。

| 範圍／快照 | logical bytes | unique file bytes | NTFS allocated bytes |
|---|---:|---:|---:|
| main清理前 | 36,848,956,130 | 35,581,405,189 | 35,636,306,896 |
| main清理後inventory | 35,885,193,872 | 34,617,642,931 | 34,672,067,760 |
| recovery前後相同 | 2,196,799,760 | 2,196,799,760 | 2,221,241,584 |
| 原campaign前後相同 | 7,631,728,996 | 6,364,178,055 | 6,392,368,792 |

main前後同時有新增重播輸出及文件修改，不能把兩列相減當刪除量。刪除量以manifest的1,009,056,990 bytes為準。磁碟free bytes差值會受其他桌面活動影響，保留drive-before／after原值但不用它宣稱精確回收。

campaign的18,385 entries對應14,542個file ID；多出的3,843個hard-link entries皆保留，沒有用它們的logical重複長度誇大清理收益。專用選片圖與全錄原圖是分開副本，不能假定共享同一file ID。

| 預算項目 | 實際／上限 |
|---|---:|
| 原campaign保守logical sum | 7,631,728,996 bytes |
| 本輪研究新增36 entries | 45,307,809 bytes |
| campaign總計（跨目錄加帳） | **7,677,036,805／8,589,934,592 bytes** |
| 剩餘campaign額度 | 912,897,787 bytes |
| 原CPU依賴＋原build＋main CPU build | 2,252,752,150／3,221,225,472 bytes |
| 本輪filesystem維護metadata／helper | 54,443,620／268,435,456 bytes（不含帳本自身）；見[final-ledger.json](../../../measurements/maintenance-20261001/final-ledger.json)，不能用它存遊戲raw或模型實驗 |

campaign實測比整合交接當時的7,631,712,403 bytes多16,593；本輪以完整盤點7,631,728,996為起點，原campaign前後無變動，不回寫歷史帳本。CPU容量沿用已驗整合capacity，各相關檔案本輪同SHA且未清理；LibTorch原archive196,059,242及展開1,154,297,400 bytes均保留。本輪研究輸出低於128MiB；後續新增artifact仍須另記、不得把目前剩餘額度當無限預算。

## 4. 為何保留恢復worktree

`C:/Users/wurre/.codex/worktrees/baseline36-recovery/Phigros-Auto-play-System`維持原branch／內容，沒有archive或刪除。`learning-cpu-01/tool-freeze-v1/freeze.json`的11個DLL使用該worktree的`out/vision-cpu-v145/Release`絕對路徑；freeze不是一份包含所有DLL的可獨立搬走目錄。main的`out/main-integration-cpu-v145/CMakeCache.txt`中Torch_DIR也仍引用recovery的`out/dependencies/libtorch-cpu-2.7.0/libtorch/share/cmake/Torch`。

11個DLL逐個SHA符合freeze，見[cpu-frozen-dll-reference-check.json](../../../measurements/maintenance-20261001/cpu-frozen-dll-reference-check.json)。將該DLL目錄只對測試程序的PATH暫時加入，執行原frozen工具的read-only packet audit後還原PATH：[結果](../../../measurements/maintenance-20261001/frozen-cpu-audit.json)valid=true、18 samples、human0、development_training_ready=false、cross_song_validation_ready=false、errors=[]。沒有訓練或載入正式即時模型。

若將來要刪該worktree，須先複製必要ignored依賴到明確位置、核SHA、更新新的引用manifest／main build設定並重驗frozen工具；歷史manifest保留。tracked合併不完成這項搬移，本輪也未假裝已完成。

## 5. 測試、hash與文件整理

清理後直接使用已驗整合`out/main-integration-v145/Release/pas_tests.exe`，篩選`GameOwner.*:GameMotion.*:GameTracking.*:ManualIntegration.*:FullRecording.*`：**117／117通過、0skip／0fail，1181ms**。沒有重建／覆寫凍結binary。詳見[完整log](../../../measurements/maintenance-20261001/post-cleanup-tests.log)與[XML](../../../measurements/maintenance-20261001/post-cleanup-tests.xml)。沒有正式碼變更，因此未重複執行整合全套349或歷史Debug／ASan。

| 受保護內容 | SHA256 |
|---|---|
| main整合pas.exe | `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb` |
| main整合pas_tests.exe | `b8a0da76ffbb1231d9795caad10cd02346bdcb9d3d383c74531b0213a14ae7da` |
| C36h tint1 | `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9` |
| 原CPU tool | `cf6c5dac20979224cf5b9cfd8c203cec47045ac0c38f3064b1811d6a92fb00bb` |
| final checkpoint | `151e47c730e11aa3c06a75a6801650c0832faab6a8d02a80495ce8256d17e8cc` |
| 全錄index | `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d` |
| 使用者REASONS.md | `a01ebaf9e989ab09537882f8bf41dae1e6d48bba315be0857c3c6ad00fa73ee9` |

文件整理把README／AGENTS／架構／路線圖的「目前」統一為50／27／11，補main50的新差異、owner重播缺口及可執行下一步；舊研究中即時學習式觀測改標歷史提案。全錄／逐幀報告的「理由待填、尚未結算」加上時間範圍，沒有重寫原實驗結果。原始研究與失敗證據保留，未為版面清爽刪歷史。

更早清理造成的ignored raw不可回復範圍仍見[9/28紀錄](CLEANUP_AUDIT_20260928.md)；本輪沒有恢復它們，也不聲稱那些舊結果仍可全部重算。`measurements`與`out`不受Git備份，這份Markdown只記錄位置／界限，不取代實體原檔。
