# 第三階段驗證、退出碼與重跑

2026-10-05；起點 `acb27fdb095b82253ef9388eab52948a59663d02`。

## 完整執行而非只檢查報告

主控用 [`run_regression.sh`](../../../../research/bvi_cold_v2/run_regression.sh)重新編譯donor、P1及最終candidate，直接跑同一六份frozen inputs和原driver。每次使用新的round3輸出根，temporary source快照與前後SHA、binary SHA、原生exit、stdout/stderr、IO adapter diff均保存。

| 執行 | 原生suite／runner | 已核結果 |
|---|---|---|
| [donor Release](evidence/legacy-baseline-release/run.log) | 1／1 | 3938／54fail，完整JSON SHA等於第二輪 |
| [P1 Release](evidence/legacy-p1-release/run.log) | 1／1 | 3938／43fail；獨立新正例另外發現mask過拒 |
| [final Release](evidence/legacy-final-release/run.log) | 1／1 | 3938／43fail |
| [final Debug](evidence/legacy-final-debug/run.log) | 1／1 | 與Release逐byte相同 |
| [final ASan＋UBSan](evidence/legacy-final-sanitizers/run.log) | 1／1 | 同43fail、無sanitizer額外診斷；LSan關閉 |
| [主控v2契約另跑](evidence/controller-contract-release/contract.json) | 0／0 | 48cases／339assertions／0fail，與契約分支三配置結果相同 |
| [主控獨立case另跑](evidence/controller-independent-release/independent.json) | 1／1 | 35cases／79assertions／2既存fail，與獨立分支三配置結果相同 |

三組結果不混合分母，也不因collector完成就標測試通過。wrong-contact獨立native1屬預期拒絕，consumer已核兩條完整failed rows；full legacy native1則仍是未滿足原契約。

## 54項映射與oracle完整性

[`compare_legacy_results.cpp`](../../../../research/bvi_cold_v2/compare_legacy_results.cpp)列出全部3938個原assertion位置，核身份與literal expected保持、原54fail每項必有after，另列所有原pass→fail。最終[legacy-final-delta.json](evidence/legacy-final-delta.json)：11修復、43保留、0新增原suite失敗。

比較工具最初把所有report.expected都當literal，因14個metamorphic reference輸出改變而拒絕輸出。校正只限`equal_to`／`equivalent_to`的**計算後reference值**：其frozen oracle定義是「與某reference case相等」，不是reference case的舊輸出常量。14處before/after materialization及pass狀態全部明列；六份輸入的oracle定義SHA一直不變。其餘literal expected或assertion身份變動仍立即拒絕，沒有放行改gold。

v2 [CONTRACT_CHANGE_MAP.json](CONTRACT_CHANGE_MAP.json)另保留54項原expected／actual／pointer、契約對應與未解決項；不是把原8+2診斷紅字改名後簽修復。新projection尚未接正式owner。

## 可重跑命令

```sh
E="$PWD/docs/research/zero-miss-20261005/round3/evidence"
bash research/bvi_cold_v2/run_regression.sh "$PWD" \
  /path/to/unpacked-evidence /path/to/json-include \
  "$E/new-final-release" release candidate all
```

輸出根必須不存在；`all`目前應exit1，因legacy43fail及independent2fail仍保留。模式可選debug／sanitizers；suite可選legacy／contract／independent。官方JSON header的pin、兩個upstream/vcpkg patches及來源沿[第二輪說明](../round2/BVI_FROZEN_SUITE_UPDATE.md)；腳本不自行下載或系統安裝。

ASan/UBSan使用`detect_leaks=0`與`halt_on_error=1`。既有ptrace環境限制沿用，沒有把LSan未跑寫成通過。沒有執行Windows、正式root CMake、遊戲、裝置、模型或真觸控。

## 交付與保全

- 原source、oracle、frozen inputs、Windows process-control及STOP保持；第一／二輪SHA清單仍能驗過。本輪只新增`research/bvi_cold_v2/`與round3文件／小型控制／衍生結果，更新兩份導覽README。
- P1、P1.1、最終source及逐步patch保存，失敗與spec修訂有沿革；原31例與追加4控制分開凍結。
- 所有JSON完整性、相對連結、shell語法、SHA、core/API來源與新source scope均檢查。完整原run log／patch的空白保持bytes，不為格式check重寫已封存結果。
- 沒有原證據包、PNG、私密metadata、編譯產物、JSON單header或cache進commit。資料都是本輪必要的冷測衍生證據；tmp binary不放交付包。
- 最終fixture最高probe計數與ABI bytes是已測輸入的結果，非runtime p99或worst-case CPU保證；成本與實機gate仍未過。

詳見 `delivery-checks.txt` 與本輪 `research-files.sha256`。這個階段完成不表示候選全契約通過、已整合主程式或全曲zero miss。
