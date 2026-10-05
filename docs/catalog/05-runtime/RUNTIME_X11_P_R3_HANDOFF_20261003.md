# X11-P-R3 交接：工程完成，成本仍not-ready

2026-10-03 Asia/Taipei。**交付完成，待總控獨立驗收。** [結果](RUNTIME_X11_P_R3_RESULT_20261003.md)／[protocol](RUNTIME_X11_P_R3_PROTOCOL_20261003.md)／[機器總結](../../../measurements/runtime-cost-x11-p-r3/final-summary.json)。主入口 `measurements/runtime-cost-x11-p-r3`，build `out/x11-p-r3`。沒有未完成或仍執行中的cost/stress。

## 要簽收的結果

AA5（RGB4＋owner1）、ABBA0、新stress0。owner1完整owned2282/2560=89.140625%<90%，原normal gate立即fail；recognition/owner measurement n2231/2052≥2000、full latency hard gate與raw完整性pass、run45,203,157B<80MiB。成本前另凍結measurement90%屬新增較嚴門檻，亦fail；原全run門檻本身已足以停止，不能將新增條誤歸R1或事後回改。全部AA不足，noise未評估、候選差異Unknown，X12仍blocked。

三版新collector10/10、兩版512input full streaming bridge一致、五筆C++raw audit pass；原XML僅驗SHA/結果，R2 active coverage僅引用，非本輪重跑。ASan scope及歷史已聲明fail見result。没有emulator/ADB/真觸控/模型/X10d-O/X12/新goal/chat/automation/commit/push，也不恢復過期timer。

## 來源與不可改區

main／HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`／唯一worktree，原86正式檔與prior dirty保留。C36h tint1 behavioral baseline、main50 donor/control、suppression OFF；57個R1 src/include逆移除唯一pending8line核對。core不重編；新shared SessionArchive object只三處16384→65536共同插樁上限。六張maps證明未抽取old library archive object。

`source-binding-before-cost.json`51bindings、`compiled-dependency-freeze.json`575實際CL/link/compiler/library依賴、`input-binding-before-cost.json`2inputs、`source-snapshot/`、`protocol-before-cost.md`與old before/after保留。cost前所有source/method/environment/commands已freeze；freeze後無rebuild/repair/retry。`generate-runtime-x11-p-r3-source.ps1`為初始bootstrap，後續實際source有工程修復/admission完善，**不可重跑bootstrap或build覆蓋frozen artefacts**，實際source snapshot才是最終來源。

沒有新runtime pas.exe。R1 runtime B0 SHA `42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09`、B1 `ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a`，profile SHA `d9644832d3aa39c608f95aa22562803cf5516de0f5f99c1286c067870c186228`僅引用未改舊檔；不是原C36h live binary。正式採用未發生。

## 總控獨立驗收命令

工作目錄是repo根。以下只核凍結與既有raw／有界test，**不啟動新cost、stress、replay、build或live**。開一個不存在的新ReviewRoot，保留32MiB配額。不要rerun已結束的`run-runtime-x11-p-r3.ps1`。

```powershell
Set-Location 'C:\Users\wurre\Desktop\Phigros-Auto-play-System'
./tools/accept-runtime-x11-p-r3.ps1 -ReviewRoot 'C:\Users\wurre\Desktop\Phigros-Auto-play-System\measurements\runtime-cost-x11-p-r3\controller-review'
```

此script核artifact ledger root/全部逐檔SHA與檔長、source/dependencies；B0/B1各重跑8個light collector tests（明確排除兩個大型archive fixtures），ASan完整10個；五筆raw audits與開發audit逐byte一致。32MiB上限核對。它只給verification，不自簽總控接受或live資格。原238/pending13/R1/R2 suites不是script重跑項；開發XML結果可逐項核`tests-final-collected.json`。如需原suite另立配額，不把此驗收冒稱全suite重跑。

normal gate負結果可對既有run無輸出檔重算：

```powershell
./out/x11-p-r3/build-B0/Release/r3_gate.exe check ./measurements/runtime-cost-x11-p-r3/aa-owner-1
# 預期 exit2 / normal_gate:false；不是工具異常
```

若需單筆audit，必須用新output，不能覆蓋原audit：

```powershell
./out/x11-p-r3/build-B0/Release/r3_audit.exe ./measurements/runtime-cost-x11-p-r3/aa-owner-1 ./measurements/runtime-cost-x11-p-r3/controller-review/owner-extra-audit.json
```

没有完整8AA，所以不可執行noise/evaluate後填補owner2–4或推論gate成功。總控應獨立查owner1 attempts/owned與90%失敗、qualification五run與stop reason、無ABBA directories/commands、各run所有warmup/blocks/full denominators/14metrics/原raw segment SHA。

旧binding另核：`old-freeze-verified-before.json`與`old-freeze-verified-after.json`含逐manifest SHA/entries及57個inverse file對；驗收script驗new ledger已綁其bytes，但不代替重新hash舊manifest每條input。若總控要重新跑完整old檔核對，使用一個全新的phase字串，避免覆寫before/after：

```powershell
./tools/check-runtime-x11-p-r3-frozen.ps1 -Phase controller-independent
```

此最後命令在batch根新增一份約29KiB維護核對輸出，計入controller32MiB；不会回寫old ledger/raw/source。先執行accept檢查凍結ledger，再執行新增核對。R2歷史status/forward僅用總控pre-review快照匹配原SHA，其他entries不得忽略。若需傳送或新chat，應由使用者／總控另行決定，本chat不傳訊。

## 帳本與閱讀順序

最終精確byte數在`final-summary.json.capacity`，含ledger及自身檔長。normal raw98,156,255B，out278,012,952B，舊campaign+prior8,241,831,623B前後不變；新2GiB、build3GiB、旧8GiB、兩帳10GiB、free5GiB和controller32MiB均遵守。工程96MiB內含failed builds/初版full bridges/fixtures/log/source/doc/XML；normal24×80MiB占额與controller reserve独立，unused slots不代表授權重跑。逐檔physical指file length，NTFS allocation Unknown，CRLF extra/LF-normalized另列。

1. final-summary、qualification及本result：讀判定、分母、容量與限制。
2. immutable protocol/input/environment、source/dependency freezes：查成本前方法與實際binary。
3. tests XML/commands/exits與bridge-final兩summary：查實跑與引用界線。
4. 五個run完整frames/receipts/releases/event-timings/archive，以及五個audit：獨立重算。
5. artifact ledger、workspace-after、old-freeze-after、documents-delivery：查保留及status/forward追加。

封存doc副本綁在batch `documents-delivery`；兩個mutable workspace進度文件不納入new ledger直接SHA，避免下輪合法追加破壞本輪freeze。開發只簽「交付完成，待總控獨立驗收」，無接受或live-ready宣告。

## 下一决策

本task已達預先停止線，不留待補owner2–4。總控先獨立簽收工程與negative。若另授權下一包，問題應是既有full diagnostics負載下owner覆蓋率缺口及量測有效性；先分析已有raw，明定可被否證的工程hypothesis，再決定是否值得另立bounded方法。不得原樣重跑追90%、縮完整JSON、只報成功subset、改AA noise公式或靠候選結果救baseline。14lost opportunity、77Miss、真閉環與跨曲仍Unknown；本包不自動開R4/R5/X12。
