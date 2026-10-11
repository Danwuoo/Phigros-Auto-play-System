# 像素 ridge 探查短路：必要 source delta

2026-10-12 使用者離開期間仍要求完成所有實機啟動前工作。完成至 emulator 邊界後停止，先不建立新 thread、不操作裝置；Chapter Legacy 可用 IN 各一次、只記分數的後續授權保留。這份契約是必要修補計畫，不能當作已通過的結果。

main77 實際 compiled source 為 `fce23e7946a4bae9a83f5294539078df0a890f11`。10 個 fresh cold TUs、9 個 build/control Native stages、actual pre-cost 必要來源核對、Normal Tap/Hold/Dense 的完整 raw 與原數值、AA12 的完整 raw 與單筆 validity 全部通過。完整 noise 是 13/18 adequate；ABBA NOT_RUN。全部 source、copies、raw、STOP 與 53 個 Native8 索引保留於 [main77 原結果](MAIN77_NORMAL_AA_PASS_NOISE13_STOP_20261012.json)。noise Native0、154.9724451s，workflow1 是原「noise 不足不跑 ABBA」停止。

五項不足是 Tap observer p95，以及 Hold、Dense 的 observer p99/capture-to-owner p99。main76 曾為 14/18；main77 的 CPU eligible partition 沒有改善全部穩定性，不認定已修好或已支持任何 OS/driver/其他程序根因。QPC wall 與已知空窗不等於 CPU compute。

## 唯一正式 source 變更

只修改 `src/game_motion.cpp` 的純讀 `ridge`。原本無條件讀中心 `v` 與兩侧 `a/b` 的灰階，再判斷所有條件；候選依原 `v → a → b` 順序，在 `!(v>=80)` 或 `!(a>=0 && v-a>=18)` 時提早回 false，最後才讀 `b`。有效 Frame、有限合法像素幾何下，所有布林結果、gray 計算、80/18 門檻、越界 -1、pixel 來源和遊戲決策策略保持。

不宣稱任意無效 NaN/Inf/超出 lround 範圍座標的浮點例外或 errno 副作用一致。現行這些 helper 不觀察 fenv/errno；提早略過無用純讀是一個效能候選，效益仍待 actual 成本判定。

原 source 是 CRLF、33,455 B、SHA256 `04b67571a1631f7036d1f12235ac30d2e7d744bf1b1dce2dcfb746d0221e7093`。新 source 用 LF/no BOM。宣告必須驗證 CRLF→LF 正規化後只有這一段 literal 函數替換；反向替換並恢復原 CRLF 須重現原 bytes/SHA。其他正式像素／動作來源、ABI headers、manual/grpc/app 不變。

既有 fresh verification TU `src/candidate_admission.cpp` 與 reader 同步更新必要收據契約，接受明列的 verification+motion 分支並驗證完整 overlay；這些是驗證邏輯的改動。原歷史 verification-only 分支保留，不把這項銜接當作新的遊戲策略或新的 runtime TU。

本次已編輯的新 source 是32,961 B、SHA256 `84d38d14cc886db75cff3c580c521d21a36a69999131e772a24ad07dc53b0f32`；唯一替換與逆向 byte 核對、26個完整 case 名稱保留於 `out/planning/game-motion-ridge-short-circuit-source-proof.json`（9,991 B，SHA256 `ccb55a348d4bcca5030acb3932a5395d3d3941bf0254717e193d248b09c57380`）。這是 source proof，尚不是實際編譯／測試收據。

## 建置与驗證身分

使用既有 producer-eligibility overlay，加入明列的 `pas.cold-motion-source-delta.v1`。一個 fresh `game_motion_delta` object 在 current_chain、production_pipeline_cold、score_only_controls、pas、candidate_admission_controls 五個 linker object lists 各出現一次，位於原 archives 之前。兩套原 base55 core archives 仍各22 members，原 source/object/archive SHA 保留；不能稱為重新建置了22個正式 TUs。

必要回歸重用 `GameMotion.*:ColdRgbH1.*:ColdRgbR1.*:ColdRgbR6.*` 的26個既有案例（21+3+1+1）。除了共享 motion object，fresh 編譯 `tests/game_tests.cpp`、`tests/cold_scenario_tests.cpp` 與由原 `src/runtime.cpp` 前綴生成的 pure runtime_contract。GTest Release/config/header/link 輸入取自原完整 pinned dependency manifest；驗證 actual JSON 的精確案例集合、結果及 Native8。

新分支共14個 fresh compile occurrences、11個 selected targets、9個 executables；原9個 configure/build/control Native 外增加一次這項必要回歸。原 affinity65、placement27、reader81、admission82及各12、score-only8/2/4的分母和數值門檻不變。歷史無 motion 的10-TU分支維持原契約。新分支明列一個 fresh formal motion TU、原正式 libraries 未重建；不再以 verification-only 字樣隱藏正式 source 變更。

source、short copy、new object、兩 original archive members、五 consumer images、回歸來源/runtime 前綴/26-case report與實際程序收據須銜接。只有這個驗證完整的同名 source/object 替換可例外處理 effective imported-source equality；不得放寬成任意正式變更。

## 後續順序

保留 v3 eligible masks、priority/QoS/readback/restore、writer-before-affinity、2560/20ms、全部 raw/metadata/RSS/容量與 R2 原門檻。configure/build/final 完整 dependency 核對及原 control compile-only 大型 installed archive 排除規則保持；不追加 ETW recording。

必要新 build/controls/26-case 回歸後依序 actual pre-cost → current Normal3 → AA12/noise18 → 原未跑 ABBA36 → final readonly → actual collection-preflight。原75與production9不全盤重跑，原 PASS 來源仍獨立保留。沒有完成以上必要成本與核對前，cost NOT_READY、live UNQUALIFIED。成功後僅停在需要 current device/profile/capture/RPC/physical 收據的邊界，不以 cold PASS 代替真正装置資格。
