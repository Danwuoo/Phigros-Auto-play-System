# Windows cost protocol R2：明確 Tap 消失／重入負載

2026-10-07，任何 R2 成本執行前預聲明。接續原
[PRELIVE_CURRENT_COST_PROTOCOL.md](PRELIVE_CURRENT_COST_PROTOCOL.md)，原文件、SHA、
所有停止批次及 raw 保留。R2 是新的 corpus／campaign，不能恢復或續接原 AA。

改動唯一的成本場景是 Tap，名稱 `tap-closed-reset-v1`。原 12 幀 Tap 在精確 cadence 下
由 note_id=2 從 frame2 持續到 frame2560；全程一次 Down／Up 在 warmup，measurement
無動作。已完成 note 不復活是必要安全行為，不能藉由放寬 ledger／重試 Down 補分母。

新 Tap 以 32 幀為 cycle：phase0..15 與 phase28..31 保留相同 HUD／line、沒有 note；
phase16..27 使用原 Tap 的 440+8k 像素位置，逐 cycle 依 -2／0／+2px 偏移循環。
16 幀前綴即使在 8ms stress 也為 128ms，超過既有 90ms track lifetime。
渲染只由離線 attempt 序號產生當前 RGB；scene／phase／offset 不傳入 Runtime，
不依 owner、receipt、target 或 plan 回饋，不重設 Runtime／owner／ledger／observer。
三偏移／兩 cadence 的 A/B FakeClock 控制必須證明每兩個完整 cycle 有兩個不同 note ID
及各自 Down／Up；無 note 負控制與明列 skip 控制分開報告。96 張固定 RGB 的 SHA 凍結。

Tap 使用單張 2,764,800B streaming render storage；量測所有 attempt 的 due、render_start／end、
publication return，render 與 publication deadline overrun 必須為零。獨立 auditor 逐項重算。
原 Hold `hold-closed-reset-v2` 的 81 phase／三偏移 corpus、dense 12 張 RGB 不變。

其餘所有原硬門檻及順序保持：Release 同機同版，Tap／Hold／dense 各 A 四次；
256 warmup＋2304 measurement＝2560，normal20ms、slow／10000 stress8ms；
full／measurement owner coverage≥.9、measurement owner_seen≥2000；全分母產生，pool／writer
drop0、fault0、backend failure0、verified release；capture→owner p99≤100ms/max≤250ms；
full／measurement phase lateness p99≤15ms/max≤100ms；Tap／Hold full／measurement 都需 own
Down／Up；RSS Release128MiB、Debug512MiB、ASan1GiB。不能刪 warmup 或失敗子集。

noise 僅用本次完整 AA12：每 load pairs(1,2)/(3,4)，d=max abs pair差，
T=max(floor,2d)，2d≤max(min四值×.30,floor) 才 adequate；observer／owner／capture→owner
p95／p99 floors .5／.25／3ms。全部必要 gate 及 noise 通過才凍結，否則 ABBA NOT_RUN。
各 load 三批 A/B/B/A＝36；同批兩 A／兩 B quantile 平均非退步≤T，
瓶頸改善>T 並傳到 capture→owner 才能聲稱改善。不得沿用舊 corpus 的 noise 或 PASS。

必要 gate 失敗即停止該批，後續明列 NOT_RUN，不安靜重跑。獨立 B slow／writer／rpc／fault
各1000、Tap／dense各10000、Debug／ASan Tap／Hold 各1000仍需完成及保存全分母。
所有 stage 仍使用 qualified owned Job／held identity／streams／deadline，沒有裝置 endpoint。
新 OUT≤12GiB、metadata≤64MiB、free≥20GiB，量測逐 stage 保留1GiB raw headroom。

endpoint-free recognition／owner 同 consumer，仍不能代替正式 production packet／action／wake／
capture／RPC。新的 workload 功能通过不表示成本通过，成本通过不表示 physical 或 live admission。
當前 emulator／app／profile／五指能力、physical qualification 與由使用者選曲／Play 的有限驗收
各自必需；全曲 IN 解鎖分母、完整結算 Miss=0 尚未完成。
