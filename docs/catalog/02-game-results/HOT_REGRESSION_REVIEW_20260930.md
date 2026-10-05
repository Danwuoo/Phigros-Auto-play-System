# 熱測退步獨立審查（2026-09-30）

## 結論與範圍

使用者要求查看原測試 task 並判斷是否有重大開發問題。本輪只讀取 task、source diff、已保存的實測資料，使用現有 C++ analyzer 重算兩輪 Glaciaxion，未啟動 emulator／ADB／manual-session，未修改正式程式、設定或原始資料，未通知／重啟原 task。

結論：已足以將問題升級為**關聯／Hold 狀態契約及驗收有效性的重大問題**，不宜再以全域 lead 調整或測試數量增加判定改善。但尚未完成版本消融，也沒有逐 Note 遊戲真值，不能宣稱已找到造成全部退步的單一 bug，或將所有取消當作錯誤 Up。

目前分支為 `codex/four-song-hot-tuning`，HEAD `25d464564c12acdf6945242c6dbd1b0c63a4c1ea`，observer49／planner26 的修改尚未提交。main 的冷開發提交為 `853a4db3e3c1c915780f7fccc5ace64d4a1ce970`。本輪不合併候選，也不回退使用者工作區。

## 主比較基準修正：冷開發之前的高分版本

使用者指出前一版只比較47與49，基準太晚。以下改用冷開發前的歷史高分作主要對照；47→49保留為後段退步定位，不能當恢復能力的目標。各曲最佳版本不同，下表是明列來源的歷史高分參照，不是拼成一個不存在的「全曲最佳binary」，也不是配對實驗。

| 曲目 | 冷開發前高分版本 | 舊分數／P-G-B-M | 最新有完整結算的版本 | 新分數／P-G-B-M | 分數／Miss差 |
|---|---|---:|---|---:|---:|
| Glaciaxion HD6 | 33/13，1636519，第九輪 | 868880／369-2-0-22 | 49/26，lead40 | 720827／307-1-0-85 | −148053／+63 |
| Dlyrotz HD9 | 36/18，cd0ec43 | 961572／452-0-0-6 | 49/26，lead40 | 895819／444-1-0-13 | −65753／+7 |
| Eradication Catastrophe HD7 | 33/13，HD9 manual session | 828500／180-0-0-20 | 47/24，lead40 | 827000／180-0-0-20 | −1500／0 |
| Credits HD10 | 36/18，cd0ec43 | 582690／224-3-0-128 | 47/24，lead40 | 627479／244-3-0-108 | +44789／−20 |

49版只完成前兩首；後兩首不可填成49版。所有舊高分使用lead35，新近候選為lead40，亦有生命周期／時間環境差異。最高值本身有選取偏差，應作恢復能力的參照，不能充當相同環境A/B的因果證據。

Glaciaxion高分已由Git歷史 `5ad759e:docs/GOAL_PAUSE_SUMMARY_20260927.md`、現存HD9來源索引交叉核對：33/13的原始binary SHA為 `f25d4774…bbea0ca4`，歷史索引已記原binary未找到；`cpp-observe-17905107612608769` raw亦已在清理時刪除，本輪Test-Path為false。仍有source commit `1636519b9e3ddd472fad1d7494f5bc7525cc5fc1` 及策略未變、生命周期不同的HD9 manual binary可作後續重現，不能冒稱保有該高分原始事件或完全相同binary。

Dlyrotz高分raw仍在 `manual-session-108176133899800/round-3`。本輪重新執行C++ `analyze game-round`、核對分段SHA並驗result.png SHA為 `23e77365df90287b0785a91a5110c970f7b0f650bf1307ad0771b75b7e90039a`，輸出 `measurements/regression-review-20260930/highscore36-dlyrotz.json`。`out/main-legacy-v145/Release/pas.exe` SHA仍為歷史實測 `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`；HD9 manual凍結binary亦仍符合 `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`。本輪只hash檔案，未執行舊版live。

### 從高分版看，應追查的變更範圍

- Dlyrotz的時間線是36/18：961572分、6 Miss、Hold Down43；37/19：910044分、8 Miss、Hold Down28；47/23 lead35：898472分、14 Miss、Hold Down17；49/26 lead40：895819分、13 Miss、Hold Down19。下降在大型冷goal之前的判定線M1階段已開始，不能只把47→49或後來的40ms投影當作全部根因。
- 比對36版Git source與保留的37版 `measurements/cold-dev-20260929/source-before/src/game_tracking.cpp`：原本的方向篩選、近期同線優先／信心選線與單線fallback，改為距離／弱角度／近期ID減120分的評分、前兩候選差小於8則unknown，並移除單線fallback。M1另改線ID全域指派、旋轉Hold續接與分段運動擬合。這些是優先消融的變更組，不代表已證明應恢復舊同法向硬門檻。
- 冷goal再加入 `confirmed_line_id` 與換線衝突守門；48加入保留已確認線的覆寫，49才加短暫缺線投影。需分開36→37、37→47、47→49，不能一次把幾個階段都叫「冷開發」。
- **修正上一版因果解讀：`samples==0`立即取消接觸在高分36版就有。** 這是值得改善的接入／續按耦合，但它的存在不是新退步證據。應查後續觀測、線身份與關聯變更如何改變到達此分支的條件。36版Dlyrotz的43個Hold同樣全列active cancellation、tail確認0，卻只有6 Miss；所以cancel總數／沒有tail事件本身亦不能判定Hold遊戲失敗。
- Dlyrotz舊高分→最新：Tap Down200→200、Hold43→19、Drag166→146、Flick92→88。動作覆蓋變化比整體提前量更值得先追查；但Down不是獨立真值Note，一指覆蓋、多次誤接或假陽性會影響數量，不能把少24次Hold直接當24個Miss。

相同分析器重算的Dlyrotz recognition時間（ms）為36版n7741、p50/p95/p99/max＝4.487/8.265/10.647/19.805；49版既有分析n7788、1.818/4.836/7.298/20.458。新版此段較快卻沒有保住高分，不能用冷測速度提升替代策略正確性。這不是完整render-to-touch或環境控制實驗。

## 補充：47→49後段退步

| 曲目 | observer47／planner24，lead40 | observer49／planner26，lead40 |
|---|---:|---:|
| Glaciaxion HD6 分數 | 806031 | 720827 |
| Perfect / Good / Bad / Miss | 341 / 2 / 0 / 50 | 307 / 1 / 0 / 85 |
| Dlyrotz HD9 分數 | 903057 | 895819 |
| Perfect / Good / Bad / Miss | 445 / 0 / 0 / 13 | 444 / 1 / 0 / 13 |

每版每曲只有一次完整結果。Glaciaxion 多35 Miss，Miss率增加35/393＝8.91個百分點；不是可以由平均分掩蓋的退步。Dlyrotz 最新兩輪 Miss 相同，但仍未恢復歷史 observer36／planner18 的6 Miss。另一方面，最初 observer47 四首中 Eradication Catastrophe／Credits 相對36版分別22→21、128→110 Miss，不能把所有曲目概括為一致退步。

兩輪 Glaciaxion 的結算 PNG 已獨立目視核對；最新兩首 PNG 的 SHA 與結果索引相符。C++ `analyze game-round` 重新驗證兩輪 Glaciaxion 分段雜湊並重算，輸出保存在 `measurements/regression-review-20260930/`。其餘曲目的診斷引用原 task 保存的分析，沒有假稱本輪全部重跑。

比較 session：`manual-session-16637397849200`（47/24）與 `manual-session-21040034439800`（49/26）。同為 Android16／SDK36 x86_64、guest5處理器、1280×720 rotation1、density320、五指指紋通過、固定 gRPC payload RGB888 top-down 256KiB、lead40／uncertainty30。前者 binary SHA `e43d32e5f14c35b7c58cd1638e128bd26a441d56ce72f4cdbbaeb6e40c5e52f3`，後者 `cd5d45567c8435ca7378a0c9c7b8d5d5eae844d043b429da0a8e0adffee8e110`。後者為 dirty source，須連同 `candidate49-source.patch`／manifest source hashes 追溯，不能只 checkout HEAD。

## 1. Hold 接入與續按仍共用失效條件：有實際事件證據

`src/game.cpp` 的已提交 intent 路徑，在處理 active Hold 寬限之前，先以 `t.samples==0` 呼叫 `cancel_contact(...,"current_geometry_unsupported")`。`src/game_tracking.cpp` 在線關聯衝突／找不到可選線時可以輸出目前 Note、但 samples=0。這使「同一 Note 還存在、線關聯無法建立」與「Note 完全不見」走不同失效路徑：前者可能立即取消，後者才走 Hold 的60ms missing grace。

最新 Glaciaxion 的兩個具體案例（均在 round-1/events-1.jsonl）：

| note / intent | decision frame | target reason | 已開始接觸 | 最後有效 evidence 到 cancel |
|---|---:|---|---|---:|
| 887 / 205 | 10854 | confirmed_line_relation_conflict | true | 26.8496ms |
| 894 / 208 | 10944 | confirmed_line_relation_conflict | true | 16.0738ms |

兩者 samples=0，取消原因為 current_geometry_unsupported。intent208 原始 journal 同時存在 Down 成功 RPC 回執、後續續約、取消時 Up 成功 RPC 回執；成功仍只代表 RPC 返回，不代表遊戲採納。此分支在 `853a4db` 已存在，不是49版才新增。

這推翻「已開始的 Hold 一律等缺證據60ms才放手」的簡化解釋。**它證明實際取消機制，尚不證明錯誤取消或某顆 Miss**：兩個 target 均無 held_body_evidence，第二個連 rails_geometry 也為 false；現有 RGB index 沒有覆蓋這兩個時點，无法證明當時仍有合法接觸支持。修正時須區分接入資格、當前 body 接觸支持、線關聯 unknown／矛盾與 tail 完成，不能直接刪除安全條件或盲目延長按住。

## 2. 交叉線修正降低拒絕次數，卻沒有證明關聯正確

同曲 Glaciaxion、兩版相同 lead40：

| 診斷 | 47/24 | 49/26 |
|---|---:|---:|
| confirmed_line_relation_conflict（target-frame） | 752 | 155 |
| multiple_line_association_unvalidated（target-frame） | 127 | 27 |
| 實際 Down RPC 數 | 337 | 290 |
| Hold Down | 101 | 76 |
| root_past（target-frame） | 1766 | 2175 |
| Hold 可見 tail 確認 | 0 | 0 |

這些計數不是獨立 Note 真值；少按可能包含消除假陽性，不能直接換算漏按顆數。但與結算一起看，足以否定「衝突變少＝遊戲能力改善」的推論。

新增 `preserve_confirmed` 的註解／文件說只保留仍在接近原線的 Note，實際条件是 `current_distance <= prior_distance + max(12px, width*0.1)`。128px寬的 Note 即使每幀遠離10px仍符合；每次又更新 prior，並無累積遠離上限。靜止也符合。這會覆寫原評分選出的線並略過多線歧義，且適用於 Hold／Drag，不只 Tap。現有 receding 負例一次退49px，沒有驗到這種逐幀小幅遠離。這是可由 source 確認的契約落差／錯線鎖定風險，**尚未證明它導致新增35 Miss**；應優先做有界歷史重播的單項消融，而非再次擴大鎖定條件。

## 3. 突現與消線覆蓋仍不完整

常規 root 至少需要3個樣本、30ms跨度；新 `current_tap_overlap` 只在先前已追蹤至少兩次 Note 且本次首次建立線樣本時生效，第一眼就出現在判定區的 Note 仍可能得到 `near_line_appearance_unqualified`。短暫缺線投影又只允許 Tap／Flick，不能處理 Hold／Drag 新接入或 active contact 續接。

因此「有線速度追蹤、增加突現測試」並不等於所有需要的動作路徑已接通。這是現有能力範圍的缺口；不主張所有 first-appearance 都應無條件按下，也不將 unknown 自動升格為動作證據。最新兩輪投影真正 Down 僅1／0次，沒有證據支持這條狹窄路徑已解決廣泛 lost。

## 4. 目前不支持單一全域延遲崩潰的解釋

Glaciaxion 原始事件重算；單位ms，各列為獨立分布，不相加p99。

| 指標／版本 | n | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|---:|
| recognition 47/24 | 10178 | 2.306 | 5.191 | 7.305 | 17.083 |
| recognition 49/26 | 10112 | 2.314 | 5.306 | 8.130 | 24.031 |
| PLAYING消費圖間隔 47/24 | 9893 | 16.471 | 29.319 | 41.283 | 93.030 |
| PLAYING消費圖間隔 49/26 | 9827 | 16.475 | 31.014 | 43.155 | 117.984 |
| capture_complete→Down start 47/24 | 337 | 9.951 | 28.945 | 33.924 | 44.281 |
| capture_complete→Down start 49/26 | 290 | 9.052 | 28.356 | 36.952 | 42.558 |

消費圖間隔 jitter p95−p5 為25.683→28.103ms，局部長間隔仍值得查，不能排除它造成個別失誤。Down分布只覆蓋真的Down，不能替未按出的Note證明延遲良好；來源render age仍unknown。未知觸控receipt、runtime revoke與scheduler rejection在這兩輪均0，不等於命中或判讀正確。

## 5. 驗收盲點與下一步

`src/game_clip_replay.cpp` 每個三幀clip都重新建 GameObserver。753張RGB比對是短窗冷啟動相容性證據；無法重現長時間錯線確認、Hold body持續接觸與多次失效累積。合成回歸主要檢查已明定條件，也可能把保守拒絕寫成成功期待，並未驗證遊戲可接受的召回。沒有人工gold亦不能由候選數無變化推斷準確率沒退步。先前合併報告有列明限制，但把這樣的覆蓋当作繼續大幅策略演進的主要門檻仍不夠。

建議暫停擴功能及全域調參，保存所有版本。以33/13與36/18的實戰高分為恢復參照，先隔離36→37的線ID／Note關聯／Hold改動，再查37→47與47→49；不能只回退到已退步的47。必要的實戰由使用者另行安排。用足夠長且有上限的真實像素窗口或具來源的 observation 歷史重播首次決策分歧，缺的遊戲語義明列，不能編造長片段。修正要同時報動作覆蓋、錯關聯、錯續Hold與逐曲結算，不只報拒絕計數降低或測試通過。先恢復可靠基線，再逐項證明改進；目前沒有證據要求重寫整套系統或導入學習。

本輪未定位全部Miss，未測環境A/A，未執行新合成反例或消融，未修改上述問題；將靜態風險、軟體事件與遊戲因果分開保留。
