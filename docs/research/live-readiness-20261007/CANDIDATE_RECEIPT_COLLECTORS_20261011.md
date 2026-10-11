# Current candidate 收據工具與實機邊界

2026-10-11 本輪完成可冷完成的 software／controls／AA／noise／原未跑 ABBA，之後停在
emulator 邊界；不操作 emulator、ADB、Computer Use 或 touch，也不建立新 thread。
本文件記錄已接線的命令與收據契約，命令範例不是本輪裝置執行紀錄。
當前 device、capture/RPC、physical 及遊戲採納均未量測；冷 PASS 不升格為 live admission。
原75／production9、歷史負結果／STOP／source／raw／R2 hash 保留，48GiB retained／20GiB
minimum free、原 bounded metadata／native／runtime caps 不變。

## 冷診斷來源及正式接線

`out/prelive-pre-emulator-planning/admission-api-diagnostic-20261011/attempt06/diagnostic-result.json`
記錄 configure／compile native0、admission82／collector48 native0。其 source-snapshot 的
79個 bytes／SHA pins 在final review修補前與工作區逐檔核對一致；編譯紀錄包含實際
`src/candidate_admission.cpp`、`src/candidate_receipts.cpp`、`src/manual_session.cpp`、兩個
controls TU 及 `apps/pas/main.cpp` object，共6個自有 C++20 TU。此隔離診斷仍為 dirty source、
preserved main32 donor links，`candidate=false`／`qualification_issued=false`／`live_ready=false`。
CLI object 編譯不等於正式 `pas.exe` 已重新連結；正式 fresh build、affected controls 與
normal Tap／Hold／Dense完整 raw數值 gate 依 [CPU工作包](CPU_COMPONENT_REPAIR_20261011.md)
另行取得收據。舊通過項是保留前提，不冒稱新三配置或 whole qualification。

final review補強實際collector命令銜接：完整15個argv欄位、凍結collection input的path／SHA、
共同current source／base pins，以及role相應的device／capture Native8前置與physical
owner／review模式，都須吻合。新增29個冷控制後collector為77項，admission仍82項，
schema及6個診斷TU不變。原attempt06 PASS仍按修補前來源保留；新診斷另寫attempt07，
不得將舊dirtycomponent PASS套到較晚修補來源。

`attempt07/diagnostic-result.json`記錄新6 TU實際compile native0、admission82／collector77
完整unique checks與native0；79個current／copied source pins的before／after bytes／SHA全部
一致。含新actual collector TU及CLI object，仍明列dirty source、main32 donor links、
`candidate=false`／`qualification_issued=false`／`live_ready=false`；不提供正式production或
裝置資格。本輪沒有因final review而執行production／cost或裝置收集。

`CandidateColdReceiptValidation`、`CandidateCollectionInputs` 與 `CandidatePhysicalPermit`
沒有轉為 `CandidateLiveAdmission` 的介面。一般 current-candidate manual archive入口關閉；
normal score入口只接受實際 factory 建立、單次使用、同 executable／config／capability 的
score-only admission。physical collector 的單次 permit只供取得必要 physical觀察，不發normal
live admission。所有正式決策仍是即時pixels、有界近期追蹤、latest frame及單一touch owner。

## CLI

`pas.exe candidate-preflight` 必須給 `--workspace`、`--config`、`--capability`、
`--admission-input`、`--admission-sha256`。輸入 schema 為
`pas.current-candidate-admission-input.v1`；此命令只讀 pinned files、不 discovery／capture／
ADB／touch／寫 gameplay。即使保存收據全部有效，仍以 `CURRENT_PREFLIGHT_REQUIRED`、
`pass=false`、exit2 停在當前裝置 preflight邊界，不能發 admission。

三個 collector 同樣要求上述五項，另要求 `--output` 與 `--game-package`：

| 命令 | collector輸入追加前提 | 產出 schema／作用 |
|---|---|---|
| `candidate-collect-device` | 完整cold／current production bundle | `pas.current-device-profile-receipt.v1`；只讀當前device／profile／APK／mapping metadata，`real_input_created=false` |
| `candidate-collect-capture-rpc` | 當前device pin及其Native8 proof | `pas.current-capture-rpc-qualification.v1`；固定capture窗口，無touch |
| `candidate-collect-physical`，owner模式 | 當前device及capture/RPC pins／Native8 proofs | `pas.current-physical-owner-observation.v1`；Fixture mapping後，使用current owner有限None單輪；`pass=false`，等待獨立覆核 |
| `candidate-collect-physical`，review模式 | 成對的owner observation／physical review pins，以及owner Native8 proof | `pas.current-physical-qualification.v1`；重核當前device，驗證先前完整owner與其後独立review；此步不再跑遊戲或重新觸控 |

collector輸入 schema為 `pas.current-candidate-collection-input.v1`，`evidence_kind=actual`；
共同要求 `source_commit` 及下列完整 pins：`candidate_binary`、`source_manifest`、
`production_build`、`cold_preflight_input`、`runtime_profile`、`capability`、`production_cost`、
`authorization`。`game_package` 若在input內，必須與CLI一致。每個pin有絕對 `path`、整數
`bytes`、64-hex `sha256`；input本身由外部 `--admission-sha256` pin綁定。
只接受實際執行的 clean compiled source／binary／profile／capability，完整current Release
object/library closure、七項affected controls及原完整 normal3 production／raw auditor joins。
完整AA／noise／ABBA由原readonly reader重核，缺項／NOT_READY在任何裝置呼叫前拒絕。

`--output` 必須是既有目錄中的fresh檔案。collector metadata單檔≤256KiB，`CREATE_NEW`
寫入，不覆寫既有收據。實際collector開始後的失敗會盡力保存同一bounded `NOT_READY`
envelope，再拋出錯誤；缺cold input或非法fresh output在begin階段拒絕，不產生裝置收據。

以下是完成裝置前置後的直接CLI格式；每個 `$...` 必須先由實際pinned inputs取得，
不得填入synthetic fixture或歷史donor值：

```powershell
& $CandidateExe candidate-collect-device `
  --admission-input $CollectionInput --admission-sha256 $CollectionInputSha256 `
  --workspace $Workspace --config $Profile --capability $Capability `
  --output $FreshDeviceReceipt --game-package $InstalledGamePackage
```

實際qualification還須透過 `research/pre_emulator/device_collection_invoke.ps1` 的OwnedV3
transaction取得native收據，直接CLI成功輸出本身不足以成為admission證據：

```powershell
pwsh -NoProfile -File research/pre_emulator/device_collection_invoke.ps1 `
  -Specification $CollectionRequest -SpecificationSha256 $CollectionRequestSha256 `
  -GateReceipt $OriginalProcessGate -GateReceiptSha256 $OriginalProcessGateSha256
```

request schema `pas.current-device-collection-native-request.v1` 的欄位為 `command`、`stage`、
`stage_root`、`exe`、`input`、`workspace`、`config`、`capability`、`output`、`game_package`、
`total_s`、`stream_cap`；`input`／`config`／`capability` 為pins。stage以對應
`candidate-collection-device-`／`capture-rpc-`／`physical-`開始，root為fresh
`out/windows-handoff/<stage>`；output另位於retained prelive或windows-handoff容量根。
wrapper依既有time／stream caps、before／after closure、owned identity／held handles／
snapshots／drain／exits封存明列device scope，不改寫原offline process guard或舊收據。

## 欄位與順序

collector共同envelope含 `source_commit`、`candidate_binary_sha256`、`profile_sha256`、
`capability_sha256`、`producer_binary` pin、`clock_domain=host_qpc_ns`、
`source_absolute_age=null`、`qualification_issued=false`、`live_ready=false`、
`gameplay_files_created=0`。單項receipt的 `pass` 只表示該項契約，不表示overall live-ready。

`production_build`含current source manifest、fresh22 core plan、`files`、configure／build
receipts及七affected `controls`。input的 `native_processes` 保存完整bounded catalogue；
`qualification_processes` 的 `device`、`capture_rpc`、`physical_owner`、`physical` 各保存恰好
八pins：`specification`、`input_closure`、`result`、`verification`、`freeze`、`state`、
`contract`、`command`。C++核對completed owned process、image／exits／scope、完整15欄collector
argv與凍結input SHA、freeze／
ordered root-leaves分母、before／after closure及current build／source／profile／capability
pins。後續receipt還須join當前device／capture pins；physical finalizer須join原owner及
subsequent review pins。final admission改用admission schema，加入 `device`、`capture_rpc`、
`physical` pins及以上qualified processes；缺任何必要來源仍拒絕。

device receipt保存serial、game package／installed game APK SHA、fixture APK SHA、public
device fingerprint、1280×720／source rotation1與四corner mapping。capture receipt固定
15s ready budget、10s warmup、60s measurement，完整notification分母拆為accepted／inactive／
invalid／relative stale drops，另列source gaps。full及measurement的RPC-read／copy／host
residence分布有n／p50／p95／p99／max，`performance_pass=null`；原host residence／capture
gap／coverage診斷警告不升格為新gate，不套用production2560或三曲公式。

physical Fixture必須已在前景；collector不安裝或自動啟動Fixture。四corner、Hold Move、
five contacts六case以當前Fixture pixels核對，未知Down不重試，首個release結果固定。
Fixture成功不表示Phigros遊戲採納。其後owner僅在RAM保留aggregate，None mode不建立
archive writer／目錄／summary／image／clips／recording或逐frame／touch trace檔案；首個
release的unknown／failure不由其後成功或catch路徑升格。None result依 `ended && RESULT`
確認，不依賴被禁用的result image；Escape／Ctrl+C、watchdog或source fault維持ABORTED。

owner aggregate只報frame／receipt總數、owner unknown／failed injection、首個release／result
狀態及六個 `owner_opportunities` 計數；`physical_gold=false`。獨立review schema為
`pas.current-physical-independent-review.v1`，須joinowner pin及四current identities／device／
capture pins，`reviewed_after_owner=true`、`independent_of_candidate=true`、
`candidate_predictions_used_as_gold=false`、`review_basis=current_live_pixels`，並列reviewer。
六phase依序為tap、hold_entry、hold_body、hold_tail、held_rotation、late_alignment；各phase
保存獨立 `opportunities`／`passed`／`failed`／`unknown`，有非零可判分母、全passed及零
failed／unknown才可通過。candidate的機會計數不能當semantic gold。`identity_1519`保存
當前identity／association覆核與相同分母欄位，`historical_evidence_unchanged=true`；
不要求歷史ordinal、曲名、contact ID或重播歷史按鍵。缺機會或unknown保持NOT_READY。

`pas.exe score-session`使用admission五項必填options，factory先完成全部pinned收據檢查，
再取當前read-only preflight，消耗admission時再次檢查；同binary／profile／capability才進
固定None單輪。authorization schema `pas.current-score-only-authorization.v1` 必須綁定
current四identities，scope為 `chapter_legacy_available_in_single_pass_scores_only`，
`persistence=none`、`one_round`／`no_unlock`／`no_retry`／`stop_after_pass`／
`device_operations_authorized`均true。這份使用者scope與真正資格分開核對。

後續實機授權沿用：盤點Chapter Legacy所有現行IN，可用IN各一次，只保存曲名、IN等級、
分數與簡短狀態；未解鎖／不可用如實記。physical owner的自然一輪也計入該single pass。
不加HD解鎖、AP／Miss0／高分前置、額外重試或持久化遊玩證據；一輪整理後停止休息。
本輪只完成冷軟體與收據接線，真正裝置邊界數值／physical gold仍未取得。
