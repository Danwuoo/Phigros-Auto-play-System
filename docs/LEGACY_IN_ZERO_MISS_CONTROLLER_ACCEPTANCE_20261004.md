# 工作包1：Chapter Legacy IN／zero miss 基準總控驗收

2026-10-04 Asia/Taipei。**總控獨立驗收通過，可以派送工作包2。** 驗收範圍是基準文件、逐曲證據索引、入口同步與既有資料保護；不代表遊戲策略、成本或全章節產品驗收通過。總控未執行產品開發。

交付 chat：`01a10602-f0fe-71d3-9dc9-3685957d509c`（整理 Chapter Legacy IN 與 zero miss 基準）。驗收時 HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`。工作包1的[最終交付收據](../measurements/controller-baseline-20261004/final-delivery.json)保留不動；舊 `final-summary.json` 是開發者已揭露的封存失敗，沒有用它取代最終收據。

## 驗收證據

| 核對層 | 實際執行與結果 |
|---|---|
| 獨立完整性核對 | 總控重新雜湊609個不同檔案；覆蓋8個交付路徑、33個交付artifact、358個存在的來源binding、311項有期望SHA來源及220個未授權改動的原有檔案。各組計數有重疊，不相加當檔案總數。無不符。 |
| 既有資料保留 | status原160614 bytes及forward原23874 bytes逐byte前綴完全保留；原dirty與其他既有untracked未被改寫。正式src/include/root CMake及index對HEAD無diff，HEAD不變，沒有非預期修改路徑。 |
| 獨立資料鏈核對 | 89個canonical round無重複、33個session無重複。逐輪重算result及round manifest SHA對summary綁定；核round/session binary、source commit/dirty/compiled-source、config/profile、版本、capture與clock一致；核總帳session欄位及event段引用。89/89、33/33通過。 |
| 結果與分母 | 70HD＋19IN，20個歷史曲名；P/G/B/M加總及max-combo範圍合理。十九首HD使用既定Credits B14，舊815／新818 Miss一致；不挑不同輪最佳。所有89份Miss大於0。 |
| 開發自驗重現 | 先讀維護腳本，再執行 `validate.ps1 -OutputName validation-controller-replay.json`，exit0，14項通過、0失敗；包含全部選入score/P/G/B/M/曲名/難度/等級對原數字來源。此層明列為開發自驗重現，不充作另寫的獨立checker。 |
| 視覺抽核 | 總控讀C36h tint1的same-stream result PNG：Dlyrotz IN Lv.13，795950分，P496/G11/B0/M77，Max Combo120，與總帳一致。僅此一張另行目視，不宣稱重讀全部89張。 |
| 文件與契約 | 已讀新總帳、JSON、handoff、入口diff及第二至第五包契約。C36h experimental baseline、main50 donor/live0、X1已驗、X10d-P冷契約、suppression OFF、X10d-O未開始、成本not-ready及歷史/現況區分符合可追溯證據。 |

可重查：[完整性結果](../measurements/controller-baseline-20261004/controller-review/integrity.json)、[總控資料鏈檢查](../measurements/controller-baseline-20261004/controller-review/data-checks.json)、[總控資料鏈維護腳本](../measurements/controller-baseline-20261004/controller-review/check-data.ps1)、[自驗重跑結果](../measurements/controller-baseline-20261004/validation-controller-replay.json)。完整性原命令另保存於同review目錄；只作本次執行紀錄，不保證新增總控檔案後原先的「僅8路徑」檢查仍可直接重跑。原交付保存954682 bytes；新增總控證據後的容量另列review receipt，不回寫開發封存。

本次只讀JSON/文件/既有圖片與SHA，另執行有界PowerShell維護驗收。**沒有build、emulator、ADB、觸控、runtime、cost、full replay、模型訓練或commit。** 原events完整內容、全部PNG、歷史DLL/compiler閉包及X1–R3歷史suite沒有重新驗證。歷史數字轉錄不升格為逐Note人工gold。

## 保留的限制

- Chapter Legacy目前完整清單分母及逐曲目前IN解鎖仍unknown；20曲名是所選保存證據的索引，不是已核章節全集。
- Dlyrotz及光的19份IN結果只能證明那些歷史時段可玩IN；目前IN Miss=0產品驗收未通過。HD亦不代推當前解鎖。
- C36h實驗基準未改善77 Miss；main50未有新live。14組少Down的利弊、193 decision skip因果、有效AA noise及B1成本差仍unknown。
- 工作包1已明列兩個維護失敗並保留最終收據。總控資料鏈脚本的console摘要曾因OrderedDictionary/Select-Object投影顯示null；保存JSON及exit0正常，隨後從JSON重讀確認passed=true/failures=[]，未當成新的產品失敗。
- 第二包接手前補查：002 receipt記的外部歷史checker來源 `C:/Users/wurre/Desktop/dots-control/incoming/PHIGROS-M1-INDEPENDENT-20261004-02` 目前不存在。工作包1稱它為歷史位置，未承諾當前可重建；本項不否決基準包，但下一包不能假稱已有該source。identity-003根也沒有名為admission.json的檔案，以其實存configure-identity-receipt為準。

## 下一包派送決策

依使用者「驗收確認後，派送下一階段task」授權，以 **GPT-6.1 Sol／xhigh** 建立本地工作包2，完成選項A獨立離線核對。以[總帳第二包契約](LEGACY_IN_ZERO_MISS_BASELINE_20261004.md#第二包收尾選項a獨立驗收)為範圍，先核來源、dirty及剩餘容量，保留舊0419/002/003失敗與原reader。

若可找回exact-SHA既有checker來源則先核再用；找不到可在新隔離目錄重新實作C++20獨立checker並明列新來源。不得把缺失source冒稱復原、直接照搬原reader後宣稱獨立，或另根重置容量額度。一次預先凍結的有界建置方案，至多一次工程修補；不得放寬原兩秒quiescence gate或改runtime/collector/90%資格門檻。

工作包2只交來源、建置/合成/逐列raw驗證及分層結論給總控，不自簽產品採用、不接第三包。第二包完成後再由總控驗收；其後B僅審查最小量測契約。量測線若到停點，不阻塞獨立X10d-O冷研究。全章節解鎖與IN zero miss仍是最終目標。

