# X11-P-R3 有界長窗口成本資格結果

2026-10-03 Asia/Taipei。**交付完成，待總控獨立驗收；not-ready。** 本包完成 bounded collector 工程及一次有限成本資格嘗試。四筆 RGB B0 A/A 通過，第一筆 owner B0 A/A 的完整 owner 覆蓋率不足，依凍結停止線結束；AA=5、ABBA=0、新 stress=0。沒有候選性能差異結論，X12 仍阻擋。

## 決定性負結果與門檻來源

`aa-owner-1`：attempts=2560、published=2560、consumed=2475、owner_seen=2282。原 R1 的完整 attempt 分母要求 consumed/owned 各≥90%，owner 必須≥2304，實際少22，2282/2560=89.140625%。85 個 consumer skips 與193 個 decision skips 均保留，2560−85−193=2282。這個原門檻已足以獨立否決 normal 資格；並非只看已成功 owner 樣本。

measurement 為固定 attempts257–2560：recognition n=2231、owner/capture→owner n=2052，都≥2000。warmup consumed244／owned230；四個576-attempt blocks 的 consumed/owned 為559/516、553/508、559/512、560/516。沒有刪 warmup 或挑最佳 block。

方法差異明列：R3 在成本之前另外凍結了 **measurement 90%** 覆蓋率要求，並在 measurement capture→owner 保留100/250ms界限。R1 僅有完整 run 的90%要求；protocol 的「保留R1…及measurement90%」不應解讀為第二條也源自 R1。本包新增要求較嚴，measurement owned2052/2304=89.0625%亦未達；交付不回改 frozen protocol/code。即使不使用這個新增要求，原完整90%仍 fail，負結果不依賴新增門檻。未曾放寬門檻、減負載、安靜重跑或修復後追 pass。

owner 全 run capture→owner p99/max=11.5068/17.5312ms，all-phase lateness p99/max=5.8618/8.6554ms，通過原100/250及15/100ms硬界限。time order、failed/unknown receipt/release、contacts exit、worker/archive fault、raw完整性均通過；45,203,157B run files <80MiB。因此本次分類為 **正常負載 owner 全分母覆蓋率不足**；容量與主樣本數已足夠，但成本資格未成立。不能歸因於B1或宣稱候選退步。

必要 A/A 未完成，owner2–4未執行，沒有 `noise-frozen.json`、`cost-evaluation.json`，noise 未評估而非已量得不穩。`qualification.json.noise_gate=false` 表示未取得進候選比較的資格。原 `.30` noise adequacy、floor、T、ABBA quantile-mean差算法都保留，未以四筆RGB單独結果啟動候選比較。

## 執行與完整分母

每筆 attempts/published=2560/2560，pool drops=0。全部14組 metrics 的 n/p50/p95/p99/max/p95−p5，以及 all/warmup/measurement/四blocks、完整 phase/failure/late/release 分母在各 `summary.json`、獨立 raw audit 和 `final-summary.json` 保留。

| B0 run | consumed | owned | consumer/decision skips | measurement recognition/owner n | receipts Down/Move/Up | release calls/requested IDs | normal/raw integrity |
|---|---:|---:|---:|---:|---:|---:|---|
| aa-rgb-1 | 2545 | 2544 | 15/1 | 2292/2291 | 1600：480/640/480 | 6/0 | pass/pass |
| aa-rgb-2 | 2545 | 2545 | 15/0 | 2291/2291 | 1599：480/640/479 | 6/1 | pass/pass |
| aa-rgb-3 | 2543 | 2543 | 17/0 | 2289/2289 | 1600：480/640/480 | 6/0 | pass/pass |
| aa-rgb-4 | 2553 | 2553 | 7/0 | 2297/2297 | 1600：480/640/480 | 6/0 | pass/pass |
| aa-owner-1 | 2475 | 2282 | 85/193 | 2231/2052 | 1515：525/466/524 | 5/1 | **fail**/pass |

五筆 receipt failed/unknown=0，release failed/unknown IDs=0、contacts_exit=0。RGB1 有2筆lateness>15ms，最大16.525798ms；保留於分母，p99=2.160205ms、max<100ms，原normal gate未要求所有receipt都≤15ms。其餘4run late_over_15ms=0。owner accepted plans15063、coverage0、canceled524（全部 current_object_missing_or_region_lost）、rejected4437（contact_conflict4436、gate_evidence_expired1）完整保留；不能把拒絕數當Miss或成功觸控數。

下面是固定 measurement 主指標，單位ms；nearest rank為ceil(p×n)，jitter=p95−p5。各run獨立，不pool、不相加stage p99。

| run | metric | n | p50 | p95 | p99 | max | jitter |
|---|---|---:|---:|---:|---:|---:|---:|
| aa-rgb-1 | recognition | 2292 | 4.494 | 8.6531 | 10.2414 | 39.156 | 6.1554 |
| aa-rgb-1 | owner | 2291 | 0.0156 | 0.0326 | 0.0672 | 2.9793 | 0.0238 |
| aa-rgb-1 | capture_to_owner | 2291 | 5.7249 | 10.881 | 15.8986 | 39.5709 | 7.6206 |
| aa-rgb-2 | recognition | 2291 | 4.6069 | 8.4481 | 9.5994 | 15.1728 | 5.9297 |
| aa-rgb-2 | owner | 2291 | 0.0158 | 0.0335 | 0.0658 | 1.0261 | 0.0244 |
| aa-rgb-2 | capture_to_owner | 2291 | 5.8626 | 10.7871 | 15.7688 | 21.7022 | 7.5223 |
| aa-rgb-3 | recognition | 2289 | 4.7866 | 8.5609 | 9.5781 | 21.0995 | 5.7096 |
| aa-rgb-3 | owner | 2289 | 0.016 | 0.0322 | 0.0608 | 1.4228 | 0.0228 |
| aa-rgb-3 | capture_to_owner | 2289 | 6.0966 | 10.6872 | 15.7343 | 22.3871 | 7.0307 |
| aa-rgb-4 | recognition | 2297 | 5.8038 | 8.9401 | 9.9974 | 16.5589 | 5.8035 |
| aa-rgb-4 | owner | 2297 | 0.0166 | 0.0382 | 0.0968 | 0.4893 | 0.0285 |
| aa-rgb-4 | capture_to_owner | 2297 | 7.1524 | 11.5977 | 17.2119 | 23.0244 | 7.6692 |
| aa-owner-1 | recognition | 2231 | 0.0239 | 0.1389 | 0.3502 | 2.3412 | 0.1365 |
| aa-owner-1 | owner | 2052 | 0.0837 | 0.2768 | 0.587 | 1.9225 | 0.2405 |
| aa-owner-1 | capture_to_owner | 2052 | 2.5774 | 7.6817 | 11.6251 | 17.5312 | 7.1559 |

每個sample來自原attempt/receipt，missing/empty/nonfinite不能pass。phase或release窗口n=0時輸出null，不能作零成本。frames/receipts按source attempt；event enqueue/serialize/write及release按最後accepted attempt錨定，非實際服務時刻窗口。全run accepted/cancel/reject及完整archive event rows另核；blocks僅描述。本包n≥2000不代表有效獨立樣本數，未建立信賴區間。

## Collector、來源與共同成本

C36h tint1 behavioral baseline、main50 donor/control，B1仍只含pending-only八行known absolute cursor0 hook，suppression OFF。R1兩版core未改未重編；57個src/include inverse核對。新C++20 standalone meter/tests直接連入對稱的SessionArchive object，僅三處16384→65536（兩reserve、一fail limit），header/class/ABI不變。六張link maps均證明新的archive object被選中、原pas_core archive object未抽取；正式src/include/root CMake未改。source/input/options/environment凍結於首cost之前：51個來源/binary/method bindings、575 compiled dependency entries，另2個input freeze項；成本後rebuild/repair/retry=0。

256warmup＋2304measurement、RGB16ms／owner8ms、原16RGB SHA `1a50b115a612d3528de2de9c225482d329b0c6a3baf10d3506ac6e47d9f082ee`、原owner32-target、lead35ms/uncertainty30ms/enabled15、jitter{-2,+1,+2,-1}ms保持。shared high-resolution Wake、SessionGameOwner及full production JSON的SessionArchive保持。owner adapter並非RGB recognition，不能將兩負載指標視為同一pipeline的替代量測。

共同instrumentation包括event admission/envelope dump、enqueue clock、queue accounting、writer serialize/write clock及逐筆raw輸出，全部保留其負擔。正式default runtime沒有meter clock/delay；舊R1 instrumented sample上限16384，本包改65536且預留記憶體較大。owner RSS initial/final=59,547,648/66,699,264B，是兩端快照非peak；不聲稱instrumentation無影響。未量測整個manual-session、真capture/RPC、HUD/result/supervisor或遊戲採納。

環境：Windows11 Pro10.0.26200／Core Ultra5 125H（14core/18logical）／RAM34,037,383,168B／Balanced；QPC10,000,000Hz、MSVCv145 Release。未改affinity/priority/power，背景負載未控制。五run elapsed依序41,971.2972、42,403.9328、42,207.0358、42,712.1962、24,707.0129ms；沒有以理想cadence乘attempts冒稱實際時間。

沒有新pas.exe。引用未改R1 B0 runtime SHA `42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09`、B1 `ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a`，profile SHA `d9644832d3aa39c608f95aa22562803cf5516de0f5f99c1286c067870c186228`。重建baseline不是原C36h live binary；原live未列compiled SHA仍Unknown。

## 工程驗證與保留

新collector最終B0 Release/B1 Release/B1 ASan各10/10，fail/error/skipped/disabled=0。ASan涵蓋新collector/tests/archive object與已插樁B1 core；第三方未全面插樁，沒有本輪OS concurrency/stress。exact65536/over限archive樣本、receipt/release/event admission界限、bounded reader長度/count/空row/malformed/truncated、空metric/缺樣本、warmup硬fail、full archive deterministic及缺row拒絕均驗。初版三套9/9、兩次failed build及修復source/log亦保留；一次pre-cost工程修復週期，首次cost前全部重新freeze。

兩版512input streaming bridge與原reference逐byte/row/EOF相同：B0 14,668,947B/7931rows/177receipts，B1 15,043,186B/9562rows/127receipts，contacts_exit均0。這是deterministic public events一致，不是完整live性能等價。

原11套XML僅核SHA/結果，未重跑原238suite、pending13或R1 suites；B1原suite歷史1fail、B0 pending歷史5fail按既有契約保留，不混入本輪10/10。R2既有三筆active OS並行與ASan結果仅引用不重跑：same-contact body Move/release已驗，但非本包32-target長窗active聯合性能或tail覆蓋。

新五筆C++raw audit重算全部14metric families／七窗口，full frame/receipt/release/event timing/archive row及segment SHA/count/EOF一致，raw integrity=pass。owner archive logical41,082,188B、CR extra26,108B、physical41,108,296B，40MiB admission剩860,852B；未fault亦未debug drop/discard。full JSON保留、不壓縮、不採樣。

| run | archive logical B | event rows | run全部file bytes |
|---|---:|---:|---:|
| aa-rgb-1 | 10,805,650 | 10,060 | 13,217,990 |
| aa-rgb-2 | 10,820,928 | 10,071 | 13,234,094 |
| aa-rgb-3 | 10,807,610 | 10,071 | 13,220,896 |
| aa-rgb-4 | 10,859,662 | 10,130 | 13,280,118 |
| aa-owner-1 | 41,082,188 | 26,108 | 45,203,157 |

normal raw總98,156,255B，各run<80MiB。全包bound73,905,664B/run、24×80MiB＋96MiB工程＋32MiB驗收=2GiB，在成本前凍結；未用sample壓縮換資格。最終精確batch/engineering/ledger bytes由[final-summary](../measurements/runtime-cost-x11-p-r3/final-summary.json)記錄，含所有失敗、初版bridges/source/log/XML/文件與自身檔長；out=278,012,952B<3GiB。舊campaign+prior前後皆8,241,831,623B<8GiB；新根<2GiB、兩帳<10GiB、controller32MiB及free5GiB保留。artifact-ledger逐檔SHA、file length、LF/CRLF與LF-normalized bytes；physical_file_bytes指檔長，NTFS allocation Unknown。

舊X11 source274/deps1490、R1 source305/deps1580/negative376/artifacts2059、R2 source38/deps562/artifacts707成本前後逐項核。R2兩個mutable文件只用總控原pre-review copies核原SHA，沒有別項例外。原86正式檔、既有dirty及oldraw均保留，status/forward只追加且old byte prefix核對。HEAD/branch/worktree保持main／`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`／唯一worktree。

## 證據分級與交接終點

- **Verified**：bounded collector、三套10/10、兩版bridge一致、五筆全raw分母/metrics完整、長窗樣本及容量足夠、原normal覆蓋率fail且立即停止；來源/compiled dependencies/map/inverse/old SHA保護。
- **Strong inference**：本共同archive插樁在既有512 deterministic input下保留public策略行為；不能外推所有future inputs或live效能。
- **Hypothesis**：diagnostic/背景OS競爭可能影響owner skip與尾變異；沒有因果trace，不從queue/write max推斷193 decision skips成因。
- **Unknown**：effective independent n、完整AA noise及B1差異、原live exact compiled inputs、真capture/RPC/device adoption、14 lost opportunity利弊、77Miss改善、跨曲。

本工程包完成不等於成本gate、正式採用或live-ready。下一步是總控独立簽收負結果；若將來另定有界工程，先用既有全raw研究owner覆蓋率缺口，不原樣重跑、不自動R4/R5/X12、不放寬90%或減少diagnostics。[交接與獨立驗收](RUNTIME_X11_P_R3_HANDOFF_20261003.md)含精確入口。
