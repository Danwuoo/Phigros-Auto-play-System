# 四首 HD 熱調試（2026-09-29）

使用者已手動完成 Glaciaxion、Eradication Catastrophe、Credits、Dlyrotz 四首 HD，要求就此範圍使用 emulator 做小步、跨曲的共用調參。選曲與按 Play 仍由使用者操作；runtime 只用即時 pixels 與有界近期追蹤。四輪同源結算、事件、108 張短 RGB、分析與容量帳本見 `measurements/game-assist/2026-09-29-chapter-legacy-full/`。

main 固定基線是 `853a4db3e3c1c915780f7fccc5ace64d4a1ce970`、observer47／planner23／diagnostics11，Release SHA-256 `e7ed210f0d27637f58d93b5fa2bf2b57c632898781e5ff9ea964c05d7eb94b54`。其 binary 已複製到 `measurements/game-assist/2026-09-29-four-song-hot-tuning/` 供回退。五指 fingerprint 相符，擷取維持 gRPC payload fast／RGB888 top-down／256 KiB、1280×720 rotation1；來源絕對年齡未知。

| HD 曲目 | 基線分數 | P/G/B/M | 舊同曲參照分數／Miss | 基線 Miss 變化 |
|---|---:|---:|---:|---:|
| Glaciaxion 6 | 796794 | 338/4/0/51 | 868880／22（更早獨立 run） | +29 |
| Eradication Catastrophe 7 | 819350 | 177/2/0/21 | 818000／22（observer36／planner18） | −1 |
| Credits 10 | 623535 | 242/3/0/110 | 582690／128（observer36／planner18） | −18 |
| Dlyrotz 9 | 898472 | 444/0/0/14 | 961572／6（observer36／planner18） | +8 |

四張結算共 9 個 Good，均標為 Late；數量太少，不能歸因全部 Miss。Dlyrotz 的 Hold Down 由 observer36／planner18 的 43、observer37／planner19 的 28，降為本輪 17；Tap／Flick Down 接近舊量，但 Hold 像素候選沒有同比例下降。這提示先檢查預測與 owner 資格，尚不能證明任何單顆 Miss 根因。Dlyrotz 的 `confirmed_line_relation_conflict` 主要作用於 Flick，不支持把 Hold 下降直接歸因於該 guard。舊 Glaciaxion run 原始事件已不在清理後保留範圍。

## 試驗 A：共用 lead 35 → 40 ms

只改跨曲 `game.lead_ms`。原 manual-session 把 owner lead 硬寫為 35 ms，現改從經 30–45 ms 範圍驗證的 profile 傳入，manifest 列有效 lead／uncertainty。候選 profile 是 `configs/phigros-hd-assist-five-lead40.json`，策略標為 observer47／planner24。30 ms uncertainty、線／Note 策略、QPC、有界 buffer、單 owner、未知 Down 不重試均不變。Fake-clock 回歸檢查同一新鮮 Hold crossing 的 5 ms 排程差與正常釋放。

使用者在 emulator 完成四輪，observer47／planner24、lead40、commit `32d93f1d41ee02e0c1cf2b4a077d197c5d01f64b`、clean binary SHA-256 `e43d32e5f14c35b7c58cd1638e128bd26a441d56ce72f4cdbbaeb6e40c5e52f3`。同源結算圖及逐輪事件在 `measurements/game-assist/manual-session-16637397849200/`；四輪 `result_confirmed`，回到 STANDBY 後正常 STOPPED。事件分段和 36 組／108 張 RGB 均驗證 SHA；RPC unknown、runtime revoke、scheduler rejection 各 0。來源絕對年齡仍未知。

| HD 曲目 | lead35 score／P-G-B-M | lead40 score／P-G-B-M | 分數差／Miss 差 |
|---|---:|---:|---:|
| Glaciaxion 6 | 796794／338-4-0-51 | 806031／341-2-0-50 | +9237／−1 |
| Eradication Catastrophe 7 | 819350／177-2-0-21 | 827000／180-0-0-20 | +7650／−1 |
| Credits 10 | 623535／242-3-0-110 | 627479／244-3-0-108 | +3944／−2 |
| Dlyrotz 9 | 898472／444-0-0-14 | 903057／445-0-0-13 | +4585／−1 |

四首各只測一次，lead40 的小幅正向差異不能與自然波動分開；它遠未解決使用者觀察到的 lost。四輪 capture_complete→Down injection_start 的 n／p50／p95／p99／max（ms）分別為 337／9.95／28.94／33.92／44.28、175／10.68／30.00／38.47／46.79、212／9.74／28.24／40.21／46.57、459／10.60／28.94／36.36／45.51，含計畫等待，不是來源 render age 或遊戲命中延遲。

## 試驗 B：瞬現線與交叉線的共通判讀

在 Credits 保存的 round-3 clip-4 三張完整 RGB，前兩張有移動 Tap 而判定線不可見；第三張線與該 Tap 重疊。observer47 對它輸出 `insufficient_history`、沒有 Down 資格。這是能直接重播的判讀缺口，不從 Miss 總數反推。新 observer48 對**已追到至少三張、前一張不超過40ms、當前有高信心長線與高信心 Tap 真正重疊、且 Tap 朝線接近**的情況輸出 `current_tap_overlap`；planner25 用當前像素立即 Tap，不把它寫成預測 crossing。相同 RGB 離線重播該 Tap 由 `insufficient_history` 變為 `current_tap_overlap`；另有首見、靜止與遠離負例。此條件目前只適用 Tap。

Dlyrotz round-4 clip-4 與連續事件 frame 38080–38086 顯示水平判定線和掃過的斜直線同時存在；一顆原本朝水平線移動的 Tap，在第二條線接近時先得到 `multiple_line_association_unvalidated`、後得 `confirmed_line_relation_conflict`。四輪 target reason 出現次數分別為 line conflict 752／261／231／704、多線未確認 127／43／1784／87；它們是 frame-target 次數，不是 Miss 顆數。observer48 對**原線在當幀仍可見、已有確認關聯與90ms內測量、Note 到原線的相對距離仍在縮小**者保留原關聯。新 Note 等距多線仍 unknown；原線不可見或 Note 遠離原線也不強制延續。合成正反例與完整 Release 315／315 通過。這是能減少無謂失關聯的候選修正，尚未取得遊戲得分驗證。

Hold 的結算缺口仍不能從事件直接歸因：lead40 四輪 Hold Down 為 101／46／16／16，而 active Hold cancel 為 101／45／16／15；絕大多數接觸在目標消失後依60ms有界 grace 釋放，真正可見 tail 確認只有 0／1／0／1。目標消失可能是已判定結束、遮擋或漏辨，沒有逐 Note 遊戲真值，不應把全部 cancel 當漏接。下輪需比較新策略的四首結算、Hold接續及當前像素證據。
