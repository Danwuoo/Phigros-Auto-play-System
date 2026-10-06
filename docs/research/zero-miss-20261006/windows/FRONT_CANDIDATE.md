# Current-pixel Hold 可見終端候選交付（2026-10-06）

本包完成隔離 C++20 current-pixel Hold 終端量測、typed producer/consumer seam，以及
同一組原 PNG 的 Release／Debug／MSVC ASan 離線驗證，供原任務獨立驗收。
head role 保留 proposed；physical ownership、合法 Down、body/tail 接觸與遊戲採納仍
unknown。產品目標未完成，實機與完整成本 gate 仍 `NOT_READY`。

## 來源與隔離範圍

工作目錄為 `C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`，新分支
`codex/hold-visible-front-20261006`，起點
`4fe388428c7ccca4ef15d18a1d8e44cddd5198c3`。本次兩個實作 commit：

- `28493f12e45e2110e74b4d5b0ab99a03f6755cf6`：初版有界當前像素終端量測。
- `e26231ec5acef9634df007770c1f06031d5326d4`：audit 核完整宣告 context，並分開原鏈與新 sidecar 時間。

本文件與證據以後續本地文件 commit 保存；以上最後實作 SHA 才是三配置最終 binary 的來源。
實作入口為 [research/hold_front_current](../../../../research/hold_front_current/README.md)。
正式 `src/include`、舊 bridge、原測試與 oracle、七份 v3 core、色彩／effect／ownership
契約及 qualified process wrappers 均未修改。formal/v3/current_bridge/pixel_diagnostics
libraries 借用已驗 source `774f3e7020818e94c508cc1481dd0986ebe417a0` 的相符配置閉包；
v3 frozen source 為 `c2dda1db9fe95b46ff103300d43583857130cd8f`。
每次建置先核舊收據、230 份原 build-input freeze 與 sealed binary SHA，CMake 再核四個 lib
的配置、ASan 與 SHA。新 sources／targets 在新 roots 編譯，沒有替換原 libraries。

`front_chain.cpp` 是既有 pixel_chain 的隔離副本。原 owner dispatch 後才執行新
measure／consume；新邊界只進 fresh frozen-v3 extract-only shadow，不接 relate／guard／
owner。新 sidecar 沒有 owner 或 injection API，`Projection` 明示
`head_role_confirmed=false`、`action_authorized=false`。observer reconstructed front
與 current RGB visible terminal 分欄保存。

原 checkout `C:\Users\wurre\Desktop\Phigros-Auto-play-System` 的 main HEAD 仍為
`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。使用者 AUX 修改與未追蹤 ZIP 的
SHA 均核實未變，見 [最終核對](front-candidate-evidence-01/package/final-verification.json)。
本輪 device commands、派送新 agent/chat、goal、automation、push／PR／merge 都為 0。

## 量測與拒絕契約

輸入只含 same-frame RGB888、current candidate 與完整 context。每 batch 最多128候選、
16條線；origin 最多64字元，輸出固定128個 result。leaf frame 上限1280×720、stride≤3840，
storage 必須精確完整；既有離線 host 仍固定1280×720／stride3840／rotation=1。
每 candidate 保存 frame-local ID、原 observer front、局部 tangent／normal、原始 pixel／RGB
與證據區間，沒有歷史 frame 或物理身分持久化。

Hold width 只接受16–512px；tail proposal 只决定局部法向符號，不宣稱量到 tail boundary。
搜尋沿法向±32px共65列，每列5個 interior lanes（width比例−.30／−.15／0／.15／.30）
與每側5個 rail probes，共最多975 probes／candidate、124,800／batch。
terminal 搜尋限±20px，各 lane 必須支持相鄰 inside→outside 正亮度跳變：
正 channel 差總和≥60，最大差≥40，法向取樣容差1px。
終端前4–12px的9列，每側至少6列 white rail（RGB各≥240）；外側2–4px最多1列 rail。
只接受單一 terminal cluster，輸出各 lane 的 last-inside／first-outside 及其原 RGB，
以最小／最大 offset 表示區間、median last-inside+0.5 表示邊界代表點。
head 厚度不取固定9px、畫面高度、rails最末點或完整 body depth/2。

非有限或過大 geometry、不完整 storage、context 不一致、真正容量溢出、不可見／已拒絕
head proposal、missing/incompatible tail normal、裁切、低對比、rails不足、無端部與多端部
均拒絕或 abstain。body patch 明列 `body_interior`，不投影 head。consumer 對完整
same-frame context、原 ROI 與當前 RGB 重量測，typed proof 必須完全相等。
duplicate 保留分別的 frame-local 證據，不因此合併鄰居或宣稱同一 physical object。
ordinal、歌名、annotation、舊動作及未来圖均不進 producer。

`front_audit.cpp` 另以 WIC 解原 PNG，不呼叫 `measure` 或 `front.cpp` 作其判準。
它核原 ROI 的局部法向、邊界區間、相鄰 RGB、rails，以及 epoch／generation／geometry=1、
width1280／height720／rotation1；frame／capture／pixels_ready 逐筆與 selection/index 相接。
舊決策欄完整比較，僅排除五項當前 host timing、原診斷附加欄及另行核驗的新 sidecar。
這是新 offline context，沒有冒稱當前裝置指紋。

## 原圖與完整分母

沿用凍結 selection：ordinal480／1500／3000／5000各64張，共256張原 PNG；
原錄影分母7,722張未擴選。index SHA256：
`0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`。
每配置執行前後逐張核原 PNG SHA，audit 再解全部256張。
selection `human_gold=0`、physical legal opportunity gold=0 都未升格。

| 全256幀的候選 | Release | Debug | ASan |
|---|---:|---:|---:|
| raw current proposals | 859 | 859 | 859 |
| Hold proposals | 336 | 336 | 336 |
| visible_terminal_proposed_head | 191 | 191 | 191 |
| unknown Hold | 145 | 145 | 145 |
| 其他 Hold-only unsupported | 523 | 523 | 523 |
| independently verified 新相鄰像素 | 1,910 | 1,910 | 1,910 |
| independently verified 舊 witness 像素 | 1,665 | 1,665 | 1,665 |

145個 unknown 分為89個 missing/incompatible tail normal、54個 no supported terminal、
2個 multiple supported terminals。1,910=191×5×2，是 query-local 支持點數，不是獨立
physical objects 或人工 gold。完整分母包含全部拒絕、unknown 及 unsupported。
舊鏈604個 BVI queries、eligible=0、自建 fake Down／Move／Up=0、最終 release verified
與基準相同；這不代表遊戲中不存在合法機會。
結果及逐列 trace 見 [Release audit](front-candidate-evidence-01/package/audit-release-03.json)、
[Release trace](front-candidate-evidence-01/package/pixels-release-03.json.rows.jsonl)，
Debug／ASan 同名 `*-02` 原件均封存。

3030 candidate2 原 observer front=(495.5,228)、width152、tail proposal=(495.5,150)，
舊 depth78。新量測 normal=(0,1)、offset interval=[8,9]，因此當前可見終端
y區間=[236,237]、代表點=(495.5,236.5)。兩側各9/9 rail rows，support anchor offset7，
975 probes，typed seam verified。五組相鄰像素如下：

| x | inside y236 RGB | outside y237 RGB |
|---:|---|---|
| 450 | 155,233,255 | 67,63,75 |
| 473 | 155,233,255 | 68,65,75 |
| 496 | 155,233,255 | 69,66,76 |
| 518 | 155,233,255 | 70,67,76 |
| 541 | 155,233,255 | 70,68,77 |

3030原 PNG SHA：`5d0b0c9d8bac8d034bbd2cebfab04cf95cb6df96944293e62bbc753ee4226c15`。
使用者另外確認的約(495,237) head前緣語義，僅在 audit 最後外部比較；代表點差
(+0.5,−0.5)，語義相容，沒有把近似標註改成精確觸點／合法 Down gold。
[人工標註原件](front-candidate-evidence-01/HUMAN_REVIEW_20261006.json)與 selection分開保存。
新邊界進 frozen-v3 shadow 後仍 front_end=false、body=absent、contact=absent、invalid=false，
所以沒有藉此放寬既有色彩或 ownership。3030其他候選仍 proposed／unknown。
1519六個 sidecar candidates 皆 Hold-only unsupported；candidate4/5 Tap physical identity
仍 unknown，沒有用 effect 或舊按鍵決定身份。

## 建置、回歸、控制與 STOP

Windows 11 Pro10.0.26200／Core Ultra5 125H（14核、18 logical）／約31.7GiB RAM；
MSVC19.51.36256／tools14.51.36231／SDK10.0.26100.0、CMake4.2.1／Ninja。
借用既有 vcpkg、manifest install OFF、Torch OFF。MSVC ASan 使用已驗相符 ASan
libraries 與既有14.50.35717 support，沒有 unsanitized owner 替代。獨立 host stack8MiB，
正式 runtime stack 未改。

| 最終新 build root | front checks | 未改 prefix checks | 原 PNG／獨立 audit |
|---|---:|---:|---:|
| out/hold-front-release-03 | 56/56 | 113/113 | 256/256 |
| out/hold-front-debug-02 | 56/56 | 113/113 | 256/256 |
| out/hold-front-asan-02 | 56/56 | 113/113 | 256/256 |

56項覆蓋不同端部 offset、旋轉／法向符號、context及proof竄改、非有限／極大geometry、
storage、裁切、無tail／無rails／無terminal／多terminal／遮擋、body、鄰居／duplicate／
permutation與128容量及溢出。113項沿未改 bridge_tests，以自己的完整 fake receipts
驗五contact、unknown Down不重試、completed不復活、uint64 prefix／cursor與旋轉等；
沒有手寫 known_down 或舊動作前綴。

三個成功負控制只各改3030candidate2 trace一欄，原PNG未改，皆 native=1／runner=1／
verification=0：epoch1→2、geometry1→2 皆拒絕完整offline context mismatch；
inside第一點R155→156拒絕 raw RGB mismatch。mutation、SHA、產生腳本與原收據封存；
完整三份負trace留在OUT並列 external SHA，未為控制複製PNG。

初版Release01已跑build／56／113／256／audit，但audit完整context與generic時間標籤
不足；原source-v1、binary SHA與結果保留，不拿它簽最後契約。
Debug01只建置。所有較早attempt、錯誤與程序STOP都保留。

| 新程序 STOP | native / runner / verifier | 結果 |
|---|---|---|
| configure-hold-front-asan-01 | 0 / 125 / 1 | member-image:5，identity不可信 |
| build-hold-front-release-02 | 0 / 125 / 1 | member-image:5，identity不可信 |
| audit-hold-front-release-neg-front-rgb | 1 / 125 / 1 | 語義拒絕但程序完整性失敗，未計成功控制 |

之後以新attempt取得完整成功收據，未重開STOP或忽略身份檢查。全部33 native stages
（30 COMPLETE、3 STOP）皆 active_final=0、held_all_signaled=true、streams_completed=true。
job suspended／assign／resume、完整身份、owned handles/member、deadline、bounded streams
沿未改qualified wrappers；沒有猜PID終止或修改安全設定。
舊兩份sealed archive共2,645個entries全部再核SHA；當前OUT與archive中各20份可直接解析
的STOP狀態也核實未变。歷史文件71個STOP的紀錄原件保留，本次不冒稱重新解析71份。

## 時間與容量

修正後原鏈時間明列 `unchanged_baseline_chain_compute_ms`，timestamp在新sidecar之前；
新 `post_dispatch_front_sidecar_compute_ms` 另量measure、consumer重核、fresh-v3 extract-only
shadow及JSON構造。二者各以全部256幀、當前host QPC計，包含拒絕；業務clock沿原offline
FakeClock，來源絕對年齡unknown。二者均不含dump／flush、final release、latest-frame
producer、RPC或live injection，不相加stage p99。

| 組態／範圍（ms，n=256） | p50 | p95 | p99 | max | jitter p95−p5 |
|---|---:|---:|---:|---:|---:|
| Release 原鏈compute | 8.205150 | 28.714825 | 32.748370 | 39.912900 | 22.707525 |
| Release 新sidecar compute | 0.088500 | 11.960775 | 13.903510 | 17.646900 | 11.920175 |
| Debug 原鏈compute | 33.194400 | 59.306100 | 83.890370 | 133.408000 | 31.228800 |
| Debug 新sidecar compute | 0.826150 | 18.997450 | 22.761770 | 25.414000 | 18.910300 |
| ASan 原鏈compute | 179.204950 | 266.365200 | 338.116680 | 515.862300 | 196.848950 |
| ASan 新sidecar compute | 10.868700 | 171.417000 | 195.320230 | 204.833000 | 170.476525 |

這是固定PNG順序離線觀察，沒有有效A/A noise／A/B／stress成本資格；Debug／ASan部分
stage與其他stage重疊，不比較組態效能，也不把此表當完整候選成本。
所有stage失敗分母及舊版本時間原件保留。v1原鏈計時也排除了新sidecar，不能回稱其時間
已包含新算法。

新容量帳為OUT≤2GiB、metadata≤16MiB、free≥20GiB；舊12GiB／64MiB及前包分配分開保留。
新build roots656,640,617 bytes、stage directories2,383,564 bytes；package封存前最後報告
之前15,044,429 bytes，free196,213,014,528 bytes。
新sealed archive共750 files／12,567,463 bytes（約11.99MiB），749個entries逐檔核SHA；
index排除自身，SHA256 `03bdc52737bc2e0afb6a0ab4c41ede24e3ab3ea63db480d253ce949e095d4fb3`。
封存包含全部attempt收據／stdout／stderr／params／freeze／maps、source-v1及
source-e26231e、四份positive traces、三個negative mutation／reports與最後核對。
未複製原PNG、binary或依賴，未删除失敗root。

原source／binary／linked closure及OUT負trace的精確SHA與路徑見
[external manifest](front-candidate-evidence-01/package/binaries-and-controls-external.json)，
完整可核封存見 [SHA256 index](front-candidate-evidence-01/SHA256_INDEX.json)。
Release03 pixel_chain.exe SHA為
`1b5306317caea1c5014ccd387b5c68724e97a3a33966f0d169a5eca7dfb912bb`；
Debug02為`243c4d00ccf2df98fb46b4205f03215e0bc950d0f04869cb9efdeb88c8a1a043`；
ASan02為`fa71d9fa2428062126d13d7af2dca4b695c49778af8253657a4721b44d8cdea7`。
全部source與輸入凍結值以manifest為準。

## 重跑入口與尚未驗收

先核sealed index與external本機輸入，再建立新的容量帳。使用
[run.ps1](../../../../research/hold_front_current/run.ps1) 的全新BuildAttempt／Attempt，
不能重開本包舊stage。既有本機closure、gate及tool snapshot必須仍可核；缺失即停止。

```powershell
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation build -BuildAttempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation tests -BuildAttempt review-01 -Attempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation pixels -BuildAttempt review-01 -Attempt review-01
& $pwsh -NoProfile -File research/hold_front_current/run.ps1 -Mode release -Operation audit -BuildAttempt review-01 -Attempt review-01
```

debug／asan同法。exact negative mutation與finalization腳本均封存；finalizer採CreateNew，
不能覆寫舊封存。這些命令不啟動裝置或觸控。

本輪未重跑不變的root349／v3 3938，原36fail、Debug／ASan timeout及skip原件保留；
沒有Windows UBSan／leak check。本輪未做模型、runtime hook、完整replay、latest-frame／
RPC／journal成本與壓力、physical合法機會分母、真實完整contact prefix、Hold接入／
body持續／tail結束、當前profile／capability、Chapter Legacy完整分母、各曲IN解鎖與
完整IN Miss=0驗收。後續接觸策略或predicate變更需另凍結候選、保留本包與原oracle，
沿既有gate及總控續派；此包不授權live。
