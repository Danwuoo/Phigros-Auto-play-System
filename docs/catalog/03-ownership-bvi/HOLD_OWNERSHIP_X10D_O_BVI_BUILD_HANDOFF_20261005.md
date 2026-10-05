# BVI build包交接：新診斷測試adapter錯誤，native前STOP

唯一run `bvi-build-20261005-01`；詳[結果](HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md)。**Windows建置封裝沒有修好或定位原始語法失敗，BVI建置／冷驗未執行。** 不續跑此STOP、不借舊控制pass放行。

- 入口為新batch `hold-ownership-x10d-o-bvi-build/final-receipt.json`、artifact-ledger index/shards、protected-index/shards、source/input/dependency/binary/stage manifests、scope、failure-classification、diagnostic-oracle/pretest、STOP-readback及state。新source `research/x10d_o_bvi_build/`；新out未建立。只有兩份BUILD外部docs。
- helper25/25、transaction32/32、control39/39各一次、原oracle保持、cross-shell5拒絕；fake CreateProcess0。新增diagnostic20-case一次18/20：D19/D20的mixed-slash scratch路徑不匹配既有canonical `scratch-` prefix，在NewTransaction即拒絕，未測到完整診斷拒絕／positive交易。source/oracle/results/兩份scratch state保留，無第二輪或修oracle。
- 初次STOP保存缺INIT_PENDING nullable running欄位，exit1及舊source保留於failures；僅補欄位保存STOP，不修／再跑預驗。最終STOP revision1、consumed0、known launch0，17個controls/probes/products入口於新shell均state-blocked。所有三exit欄均null，非pass/skip。
- 兩次freeze／正式contract均未到達；run仍placeholder且未取得launch資格。wrapper-v0七份ASCII/CRLF/NUL0草稿及純maintenance argv fixture未執行，probe0/4、wrapper repair0/2。原cmd syntax offending step、vcvars/CMake target入口均unknown。不要將未執行的backslash候選當已證修復。
- 真controls0/3、產品0/10、candidate binary/build0、layercases0/356、supplemental0/22、schema0/4、sizeof/metadata/probes null。原1128rows/2392refs、R1候選及contact adapter SHA保持。Debug/ASan未啟動也未在本包完成可用性確認；compiled/loaded C++ closure未驗，Add-Type只managed預驗interop，完整managed DLL closure未採集。
- 保護3609舊unique/4010refs，再加scope為3610/4011、0不符；原594 Git paths、HEAD/index/dirty保持。carry8285901212/8988255/controller1299900及剩7088708保留，新dev<=40249475/newout<=134217728。source/docs/all scratch/failures/protection shards/receipts/self bytes全計，實際容量看final receipt；newout0、PNG0/2、未刪舊資料。
- 既有geometry接口按SHA引用，合法current ROI/all-lines、physical owner、owner regression、cost/runtime/live仍後續另包。C36h tint1/main50 donor live0/suppression OFF/P excluded/X12 not-ready與Chapter Legacy IN Miss=0保持。

下一包若另獲授權，最小修補是diagnostic-pretest以`Join-Path`產生canonical scratch及case根，並檢查INIT/READY的nullable state欄位，保留嚴格NewTransaction根guard。先使新的完整diagnostic交易預驗成立，再做不可變runner/allowlist freeze及三真控制；之後才能用有界marked wrapper probes定位Windows命令封裝，再作產品freeze與連續configure/build/cold suite。這只是具體交接，不授權重跑本包或新增run，本chat不另派task／message。
