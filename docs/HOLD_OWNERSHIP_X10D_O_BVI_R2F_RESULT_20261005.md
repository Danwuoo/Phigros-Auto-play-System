# R2F結果：開工預留算術被容量gate拒絕，STOP；BVI未驗

2026-10-05 Asia/Taipei。唯一attempt `bvi-r2f-20261005-01`，依[R2F派送](HOLD_OWNERSHIP_X10D_O_BVI_R2F_DISPATCH_20261005.md)封存為工程partial。**預初始化容量保護拒絕後即停止可執行開發；三控制、完整交易預驗、C++ configure/build/tests全部0。** 不是BVI核心反例，不是磁碟實際耗盡，也不宣稱execution recovery／cold contract通過。R2E STOP與R1 repair保持。

## 失敗與停止事實

在新source複製最小候選／harness／maintenance來源後，開工命令依序完成Binding、InitializeRoots只讀不存在檢查、Protection，再呼叫 `Capacity 41900000 134000000`。當時source71989B，development請求為41900000＋71989＝41971989B，超過R2F子上限41943040B **28949B**。development carry加此請求仍在56MiB、aggregate保留controller後仍在8GiB，out請求在128MiB、free亦充足；這次拒絕是新增development預留算術錯誤，不是實際資料超額。

這是開發者可避免的錯誤：reserve參數是「尚待新增」，不能在已有來源後仍請求接近整個子上限。派送A明列「第二輪仍失敗／容量保護或舊根改動則停止，不進正式controls」，因此未縮小參數重新放行、不執行initialize、不重用唯一attempt，也沒有進行任何真native試錯。[failure-classification](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/failure-classification.json)保存失敗請求、當時bytes、工具exit1及分類。

另有明確流程偏差：同一functions.exec在exec_command返回exit1後，仍執行後面的apply_patch，新增CMakeLists.txt與initialize.ps1共1981B。這兩檔未執行，保留而不刪，source由71989變73970B。這是外層工具編排未依回傳exit停止，不能把本包描述成「完全沒有停止後source變動」。識別拒絕後未再修改source、修runner或launch；此偏差亦列失敗收據。

新batch只為保全建立STOP state：revision1、native_stages_consumed0、native_launches_known0、receipts空、contract/freeze null、next null。另一個工具shell用新common的CheckState讀STOP，實際拒絕`state-blocked`，見[STOP-readback](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/STOP-readback.json)。未建attempt.lock或正式run入口；新根已存在且state為STOP，未呼叫初始化。後續零預留的容量枚舉僅供收尾計帳，沒有授予launch。

## 實作、來源與未驗分母

新source含R1原樣bvi.cpp/hpp、R2E main/driver contact adapter及owned interop／控制腳本的R2F binding副本。bvi.cpp/hpp逐bytes／SHA與R1、R2E一致，沒有修改候選門檻、oracle、fixture、grace或suppression。common初步改SHA及JSON reader為FileAccess.Read／FileShare.ReadWrite，使它們設計上容納原FileShare.Read writer；writer本身未全域放寬。Protection已延伸重建R2E ledger／freeze／inputs／dependencies與最新controller carry，固定R2F三根。

**完整共用storage/state transaction尚未實作、未測。** ReadStream變更只有來源，未驗writer仍開啟、flush穩定、log pump完成、report/identity writer ownership、封口後SHA及JSON鏈；不能稱已修R2E交易。沒有run.ps1、prepare.ps1、formal contract.json或execution freeze。source-manifest是封存來源清單，不是執行freeze；mock.ps1只複製，並未執行。CMake與initialize亦未執行，PNG source/target沒有複製。

| 授權項目 | 實際 | 未驗 |
|---|---:|---|
| helper組 | 0次 | 原P9/S7/E5等全部 |
| 完整交易預驗 | 0/2輪、0cases | 每輪≤40case，writer持有／關閉及fake facts鏈全部 |
| natural／nonzero／owned-child | 0/3 | native／runner／verification exits、15s child身分／cleanup全部 |
| Release configure/build/負控制/suite | 0/4 | wrong-contact實際native1及consumer、完整356 layer-cases |
| Debug／ASan | 0 | 未做本包可用配置preflight，未啟動，不倒填skip或pass |
| 原89×4、22 supplemental、4 schema controls | 0 | assertion0；1128 coverage rows／2392引用保持原來源，未執行 |
| sizeof／metadata／probes | null | 尚無C++ binary或實測 |
| PNG | 0/2，剩2 | 本包不執行已知provenance-gap driver |

原R2E來源、收據、out及STOP沒有回寫。Protection先前及STOP後均重建原R2D2134、R2E原2145，納入R2E交付／依賴／controller後去重**2222檔／2401引用／0不符**；數字隨新增保護範圍如實報，不硬湊歷史2214。原532 Git paths、HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`、原dirty指紋保持；src/include/root CMake及原apps/tests未改。[protection-final](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/protection-final.json)為最終入口。

開工／只讀檢索有兩組rg路徑診斷：Windows wildcard literal與兩個不存在的source名稱、之後一個不存在的header名稱；查核已改用存在的固定檔。這些是維護檢索錯誤，與容量hard stop、BVI及遊戲分開列在maintenance-findings；沒有啟動原settle/check script或source。

## 合法幾何與owner交接（只讀設計）

以下對照只讀既有source/schema，不開PNG、不執行observer/candidate/full replay。SHA索引及逐欄結論見[geometry-interface-review](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/geometry-interface-review.json)。R2E png-packet52列的legal_current_rois與legal_current_lines均null；O rgb-packet明記full-prefix selected trace，不能補成bounded-current輸入。

| BVI輸入 | 既有具體接口 | 當前pixels／history與6份90ms可達性 | 缺口 |
|---|---|---|---|
| RGB／ROI範圍與kind | pas::Frame；GameObserver::process；candidate_batch().candidates、TrackingCandidate.note.kind | 新segment可從原圖重建Frame、observer.reset後取得可見候選；本包未執行。core bounding box不是獨立Hold front | 沒有ROI→BVI Query adapter、current支持理由與容量consumer |
| front | NoteCandidate.center、held_body_patch、TrackingCandidate.head_visible；src/game.cpp的current rails/front提取；make_candidate_batch | 真可見leading edge有當前pixels；recent_identity與held patch含近期anchor。空history可能不產生部分held候選 | patch是接觸區不能冒稱front；head_visible為既有標記，須驗端部證據及anchor語義 |
| width | NoteCandidate.width、rails_geometry、direct_rails_evidence | current rail pair或color component可量，部分重新構造借近期anchor | width來源類型及兩側當前支持需輸出；不得把假fixture rails當真圖支持 |
| depth | NoteCandidate.height、tail；visible_hold_rails返回HoldRails.depth | 真rail span為當前掃描；普通color height可能是包圍盒，held patch可無tail | height不能無條件直接當BVI法向depth；需可見前後界限、normal符號與截斷/unknown |
| angle | NoteCandidate.tangent；candidate_batch_json.note.tangent | current component/rails方向；部分held recovery使用近期朝向。可在重設segment量當前可見方向，未驗 | atan2只轉數值，modπ兩向不決定Hold後方；禁止借line法向/運動向代Note朝向 |
| 全部lines | DecisionSnapshot.lines→CandidateBatch.lines；LineCandidate.center/tangent/length/track_id/observed_ns/association_valid；GameLineTracker.update | s.lines為當前量測；ID/motion来自bounded tracking。reset可避免full-prefix身份，但改變出生與競爭；未驗 | 全列line幾何／歧義／上限須保留；不能只拿target.line_id、projected hit或drop歧義線造成偽unique |
| context／clock／history | Frame sequence/epoch/generation/geometry/source_rotation/capture_complete/pixels_ready/source_valid→BVI Key；observer.reset、Candidate重新建立 | PNG metadata有host QPC與獨立source timestamp；6/90ms需segment adapter先淘汰、限制傳入，非僅取full-prefix bank末6列 | 來源valid及domain映射、frame gap／context重設、無舊anchor暖機的接口驗收尚缺；source age不能冒充量測 |

GameObserver.process在src/game.cpp:1308先make_candidate_batch，後track_legacy_batch；其傳入的tracks_可影響held recovery與origin/hint，historical bank即使每row current也不等於整個因果來源≤6份。src/game_tracking.cpp:527–583的CandidateBatch schema有center/tangent/width/height/tail及全lines，history_source預設baseline_guided；現在僅是可用接口來源，沒有BVI合法provenance adapter。decision_json的targets則是追蹤/預測/動作資格結果，不應反向補Query。

下一包幾何驗收需明確可見front、當前雙rail與depth、遮蔽patch unknown、完整line競爭、rotation與late alignment、空history及6/90ms淘汰/重設；禁止从4R手述parts、oracle、physical labels、舊actions或full-prefix bank填值。這是具體缺口清單，沒有開發新辨識平台。

owner接入仍另需隔離的adapter與FakeTouch回歸：真執行cursor/prefix、未知Down不重試、同contact責任、completed不復活、ambiguity/loss不續證據、60ms邊界及visible tail完成，報全部拒絕與機會損失。constrain的Guard.attachment_query是外部fake條件，現有BVI冷測即使後續成功也不建立physical ownership。本包不接GamePlanOwner。

## closure、容量與交付

source/input/dependency/binary manifests及artifact-ledger按實際path/bytes/SHA保存。BVI binary0、新out0、compiled include/link/CRT/DLL closure未驗；owned.cs未Add-Type，不算build。maintenance PowerShell image與版本有fingerprint；不把R2E依賴清單或檔存在当成loaded/compiled closure，也不宣稱Debug/ASan可用。全授權stage均在stage-summary列unexecuted。

沿controller carry aggregate8284207647B、development7294690B、controller1299900B，原out374705B。新development＝新batch＋source＋兩份RESULT/HANDOFF；newout另加aggregate。cap41943040B／out134217728B，aggregate≤8589934592B、development總≤58720256B、controller剩7088708B保留、每phase free≥5704253440B。精確交付bytes／SHA／self exclusions以[final-receipt](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/final-receipt.json)為準；不刪舊資料/失敗、不複製PNG。維護reports各≤64KiB、maintenance logs累計≤2MiB。

完成允許的STOP保全及只讀接口交接後交總控獨立驗收即止；不自行續派。C36h tint1 baseline、main50 donor50/27/11 live0、suppression OFF、P excluded、X12 not-ready及Chapter Legacy全曲IN Miss=0目標保持。未啟動emulator/ADB/觸控/manual-session、模型、runtime/cost/stress/full replay、goal/automation、commit/push。沒有新增IN產品證據。
