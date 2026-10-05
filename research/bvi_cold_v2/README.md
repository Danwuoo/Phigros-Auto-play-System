# BVI cold v2（隔離候選，未整合正式runtime）

本目錄從 `research/x10d_o_bvi_build/bvi.cpp/.hpp` 衍生，保留原source與oracle不動。核准範圍是當前pixels支持的rails／endpoint最小修補及cap不掩蓋merge歧義；不是主程式owner或真觸控策略。

結果／契約：[第三階段研究](../../docs/research/zero-miss-20261005/round3/README.md)、[CONTRACT_V2](../../docs/research/zero-miss-20261005/round3/CONTRACT_V2.md)。

## 檔案

- `bvi.cpp/.hpp`：新cold candidate，保持原API和bounded metadata
- `contract_fixture_v2.cpp`：48-case新契約suite，與舊3938斷言分母分開
- `independent_tests.cpp`：獨立renderer的35-case審查，仍保留duplicate ROI既有2fail
- `compare_legacy_results.cpp`：C++逐3938斷言及原54項失敗before/after，不更改期望
- `run_regression.sh`：全新輸出根、輸入SHA鎖定、source snapshots、native/aggregate exits與三配置

只需要Linux GCC C++20、bash/jq、原repo、已核小證據包及官方pin的JSON header；不下載或安裝依賴、不啟動裝置。JSON依賴重建見[第二輪文件](../../docs/research/zero-miss-20261005/round2/BVI_FROZEN_SUITE_UPDATE.md)。

```sh
E="$PWD/docs/research/zero-miss-20261005/round3/evidence"
bash research/bvi_cold_v2/run_regression.sh "$PWD" \
  /path/to/unpacked-evidence /path/to/json-include \
  "$E/my-new-release" release candidate all
```

第五參數可用`release`／`debug`／`sanitizers`；第六為`baseline`／`candidate`；第七可用`legacy`／`contract`／`independent`／`all`。既有output拒絕覆寫。`all`目前應非零：原suite仍43fail，獨立suite仍2fail；新contract suite才是339 assertions全過。不得以runner完成收集當作所有能力通過。

`sanitizers`使用ASan＋UBSan、明示關閉LSan（既有ptrace環境限制），不宣稱leak-free。舊process-control與Windows wrappers完全未動；本目錄沒有network、observer或touch backend。
