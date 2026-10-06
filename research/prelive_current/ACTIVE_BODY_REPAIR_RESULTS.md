# 有界 active-body／tail-proof 修補與 workload 設計

2026-10-06，接續 [第一輪診斷](COST_WORKLOAD_DIAGNOSIS.md)。先前 STOP、false starts、
既有 generator、frozen v3/front、所有舊 reports/oracles 保留，不把 Linux 功能測試作 Windows cost
或 live permission。此輪解決兩個可冷證實的 current hook 問題，另保留正常 tail 完成的明確缺口。

## 1. 已修：own active Hold 缺 tail 時的當前 body

formal `observe_moving_held_front` 由當前 rails 量得 local-normal `height`，但刻意清掉 tail；
只有接近線且看到 transverse closure 才恢復尾座標。舊 hook 把 absent tail 當成 body_depth=0，
即使已有自己的成功 Down、當前 `held_body_evidence`、完整 RGB rails/body，也拒絕遠離 head 的交區。

最小修補只對 own ACTIVE、`held_body_evidence && rails_geometry && !held_body_patch && !tail`，
且 finite `0<height<=4095` 使用此當前 local-normal depth 作粗空間界限。方向仍是 head 後方，
不是 ±height；完整 current-line 競爭、line identity、當前 body_rows/rails/contrast 仍重新檢查。
不把 height 終點當 tail，不新增 Down，不改 owner、scheduler 或 frozen v3/front。

`active_body_tests` **before 34/35 → after 35/35**。正例必須先經真實 owner/scheduler/FakeTouch
自己的 Down，後續 no-tail 當前 body 更新同 contact 並實際 Move。反例涵蓋 unowned、completed、
缺 fill／rail／當前 hit-region fill、第二線、expiry、0／4096／NaN depth、缺 observation flags、
深度不足、反向 tangent/normal、line 在 alleged head 前方；全部拒絕。

## 2. 已修：judgment line 白色邊緣不能成為 typed tail

在已凍結 family-v2 frame62：generator tail=500，formal observer tail=501；immutable front donor
量到的 rear boundary 卻是 judgment line 上緣 y=498.5，五條 inside witnesses 都是 y499 的白色
線 pixels。frame62–65 都把這個 line edge 回寫 owner tail=498.5，並把 `s.tail` 設為 true。
owner 正確判斷未過 line500，但上游宣稱的 tail proof 本身不可靠。

最小安全 guard：rear 五條 lanes 的 inside/outside 任一 WHITE witness 落在任一完整當前線的
既有2.8px mask，rear proof 一律 unknown，清掉 tail／tail_crossing，不得 `s.tail=true`。
實際 cyan／black witnesses 仍是 pixels，即使落在 conservative geometric mask 也不由此否決；
這與 frozen v3 對真實 colored/black witness 的區分一致。不修改 immutable front sampler。

`tail_guard_tests` exact frozen-v2：**before 11/15 → after 15/15**，frame62–65 的 false proof
改為 unknown；同 corpus frame58/59 的遠離線真實 cap 保留。舊 current68 的 colored-tail 控制
不需改 oracle。初始較嚴 any-color mask prototype 另保留，最終只拒絕被 current-line mask 包住的
白色 witness。無新 typed tail-only release 動作路徑。

## 3. Workload 確实可冷改善的部分

先事前凍結 [family-v1](COST_WORKLOAD_FAMILY_PLAN.md)：closed transverse tail cap、慢速1px
head 接近、8 blank frames（160ms）讓 observer 自然淘汰舊 identity，禁止 seed actions。
三個預定 offsets{-2,0,+2}，A/B 每筆8cycles／520frames。

- 六個 positive runs 均520/520、8個 distinct own notes、8 Down／8 Up。
- 分開前256 warmup，後264 measurement 均4 Down／4 Up。空 phase 不再藏在 warmup。
- B 當前 active-body accepted rows：16/32/40 → 168/184/200。A 完全未改。
- 104個獨立 C++ checks 核完整 frames、phase/measurement 重算、own plan→receipt、五contact
  lifetime／release、8 identities 及 B body 增量，全部通過。

v1仍只有一筆接近 line 的 tail。再事前凍結唯一 [family-v2](COST_WORKLOAD_FAMILY_V2_PLAN.md)
修正：tail651..669改1px/frame，不再做 raster 搜尋；每cycle81frames，共648frames，仍測所有
A/B／offsets及 absence/effect/no-tail-cap controls。每個 positive 都8 Down／8 Up；後256的
measurement392frames有5 Down／5 Up，B每offset有280個 active-body accepted rows。

這解決了「沒有共用 action workload」及「重複圖樣 completed identity 不生新機會」的設計問題。
它不表示兩版執行同等 active work：A 的 no-tail adapter 仍提早安全釋放，B 持續接觸到尾附近。
固定接觸點不需要 Move；Move 路徑由上面的獨立同contact平移正例實際驗證。

## 4. 正常 tail completion 仍未成立，原因是軟體契約 seam

兩個 family 的 `game_hold_tail_confirmed`／`visible_tail_completed` 都為0，不把安全 Up 當正常完成。
v2 並非缺少 sampled frames：frame58–65 多次可見 rear；masked-line proof 修補後僅真實 unmasked
部分可保留。當真實 tail 離開線 mask，body-intersection gate 隨之拒絕該 target；目前 tail 驗證又
放在 body gate 之後。因此 owner 的原兩份當前 tail-passed、跨度>=10ms 規則，拿不到有效輸入。
A 還有獨立的 no-tail query 拒絕。Windows 更快的 timing 不能修正這個 state/typed-proof 缺口。

同 frozen-v2 offset0 frame69 的實際 Up 是 `game_contact_cancelled`，reason
`current_object_missing_or_region_lost`。frame62–65 raw/filtered tail、真實 RGB lane samples、
接受的 body plan 及後續取消已完整保留在 `workload_family_evidence/tail-seam.json`。

下一個最小設計為「own ACTIVE 的 terminal-only current proof」：以同幀 cap/rails、完整 current-line
遮罩競爭、身份/context/capture freshness 驗證尾，僅傳給既有兩份 fresh tail release 規則；不准
new Down、Move、借 body flags 或修改幾何交區門檻。這需要明確 typed seam/owner 接收契約與
paired positive/negative pixels，不在本有界修補中偷偷加入。真正 Phigros tail-role 仍需 targeted
同 Hold、已有 own Down 的 approaching/on/past-line 圖片，標清 trailing terminal 是尾或 hit effect；
只有合成 cap 不能替代 physical gold。

## 5. 診斷分類修正，不改 frozen oracle

舊144-check診斷把「shared entry」定成12frames內 Down且Up。body修補後B在窗口末仍持有自己
的 contact，原 assertion 因此在三配置各失敗一次（`safety-*` 原件保留）。現在 shared entry 只由
ownDown定義；另完整列 `shared_down_up_within12frames`（本結果為空），並新增 assertion：503正例
B窗口末仍有1個 own active contact、沒有提前Up。所有 phase receipt、final verified release、
no retry gates 不變；新診斷為145checks。原3938／113／front oracle 不動。

## 6. 交付與分層

- 軟體修補：active-only body粗界限與白色 line-edge tail proof拒絕，paired冷反例已解。
- Measurement設計：closed-reset family 有完整 warmup/measurement自身动作，shared entry/release
  與 B-only extended body 成本必須分開說明；沒有同等full-Hold成本或正常tail採納主張。
- Windows qualification：未跑新A/A、ABBA、cost campaign；舊noise／STOP仍保留。
- Live permission／physical採納：沒有裝置或真touch行為，未取得、未推論任何新live授權。

若未來明確採納一個新cost scene，建議名稱 `hold-closed-reset-v2`，單一 producer-owned可重用
RGB buffer＝2,764,800B，81frame cycle；不预存648張圖。需在新campaign事前明列render開始/結束
及是否計入完整producer時間，保留三槽latest pool／全部attempts／RSS與Journal硬界限。
目前沒有將它接進 qualifying pipeline；原 hold generator byte-for-byte 保留。

所有 before/after、負結果、源碼/plan/binary bindings 在 `workload_family_evidence/`；
最後三配置的源碼閉包與結果在 `../prelive_portable/evidence/`。獨立測試通過不代表功能未知已消失。

Scoped commit 時，final local Release 的15／68／145皆通過；完整三配置`final-safety-*`
及3063原PNG閉包由總控／portable lane繼續完成，這份文件不先簽 pending 結果。
