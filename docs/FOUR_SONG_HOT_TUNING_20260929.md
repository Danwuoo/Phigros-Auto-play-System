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

這是時間假說試驗，不是已驗收修正。建置與針對性回歸後，由使用者在 emulator 以 manual-session 逐首重測四首；每輪保留完整 P/G/B/M、score、各類 Down／取消、故障、capture_complete→injection_start 的 n/p50/p95/p99/max、結算及有界 RGB。逐曲比較得失；若最差曲惡化或無一致收益，回退 35 ms。單輪四首不能證明泛化、AP 或每顆 Note 的遊戲命中。
