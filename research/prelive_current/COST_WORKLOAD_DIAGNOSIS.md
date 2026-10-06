# A/A Hold zero-Down：有界冷診斷，仍 NOT_READY

2026-10-06；起點 `e9e305e3aba13f05a786c74540419428610b2f76`。
只新增獨立 C++20 RGB workload 診斷／回歸，未修改正式決策、原 v3/front、cost thresholds、
舊 scene、舊 corpus、任何 STOP 或既有收據。沒有新增或啟用 cost scene。
Linux FakeClock 結果不是 Windows live／成本資格。

## 已定位的原因

舊 `chain.cpp::stimulus("hold")` 產生 fill `(155,233,255)`、外部 `(30,30,30)`，
每 12 幀的 head 為 `440+8k`、body 長 100、判定線 y=500。

- 未改 A/v3 的 `Pixel::blue()` 要求 R=20..100、G=130..230、B>=G+20；舊 fill 的
  R=155/G=233 不符。body、rail+inside、contact 不成立。
- v3 的 `endpoint()` 另需距 endpoint 1..4px 的雙側黑色外部，RGB 每通道 <=12。
  dark30 不符。只修 fill 仍然沒有 front／relation／Down；只修外部也無效。
- 獨立四組、每組 12 幀 query probe 證明兩個阻塞彼此獨立；同時改為 blue
  `(40,190,255)` 與 black0 才有 4 個 v3 constraint Down-eligible frame。
  此分母是合成 query eligibility，不能當正式 owner 動作或物理機會。
- 實際 RGB→observer→lifecycle→同幀 hook→sole owner/scheduler→自身 FakeTouch ledger
  的 120 幀重現中，舊 Hold 的 A 和 B 都是 Down/Move/Up=0/0/0，並非僅 A 不支持。
  B 在靠近線時選到白色 judgment-line 邊緣，或因 formal observer tail 缺失而拒絕。
  current hook 正確拒絕白色 terminal 作 cyan head，沒有放寬這項 guard。

## 為何沒有把另一個合成點改成「合格 Hold」

保留三個未量成本的探索版本：head-at-frame5=500 時兩版都 0；502 時只有 B Down；
503 時兩版都有 Down/Up。但後續 Up 發生在 Down+60ms、raster tail 過線前，
是缺證據後的安全釋放，不是正常 Hold tail 完成。三個 source snapshot 和全部結果保留。

隨後事前固定完整 `490..510` 整數 phase sweep，A/B 各 21 筆，每筆 12 幀；
再加 503 的 stationary-body／leave／absence／effect 各 A/B 一筆，共 50 runs／600 attempts：

- 同一輸入兩版都能 entry+Up 只有 503 一點，沒有連續、多像素共同有效區間。
- A 在 phase 490..493、503..509 有 action；B 在 501..503 有 action。前者的
  490..493 是較後 frame 的接入，不表示在 y490 當下觸控。
- leave／absence／effect 全部僅一次自身 Down，隨後安全 Up，無重試；全部 final release 有效。
- stationary-body 下 A 提早 Up；B 維持同 contact 到 final release，沒有注入 Up。
  兩版 continuation 不等價；沒有 Move 或正常 tail 完成的證據。

因此停止參數搜尋；撤回 prototype 的 `hold-shared-v1` 名稱與提案。
`entry_release_probe()` 只留在獨立診斷 header，cost CLI／controller 沒有新 scene。
診斷 assertions 通過只表示預期的正／負契約被保留，不是把原零動作失敗改綠。

## 成本分母與仍未解除的阻塞

舊協議／收據保持原狀：normal 每筆 2560 attempts = 256 warmup + 2304 measurement；
完整及 measurement owner coverage >=90%，measurement owner_seen >=2000。
consume/publish/owned 分開；capture→owner 使用實際 owned，measurement 只取 attempt>=257。
phase／injection 以全部收據（含失敗）為分母；source capture join 不足不得冒稱真 source age。

hard gates：全部 attempts、chain safe、verified release、pool drop0、writer drop/fault0、
input failure0、capture→owner p99/max <=100/250ms、非 dense phase lateness <=15/100ms、
Release RSS <=128MiB。dense 是容量計算負載，不是 Hold／合法機會驗收。

舊 A/A 需每個 tap/hold/dense 四筆，共 12；(1,2)/(3,4) 的最大差 d 形成 T=max(floor,2d)，
noise adequacy 為 `2d <= max(0.3*min(metric),floor)`；observer/owner/chain floor=.5/.25/3ms。
Tap noise 已經獨立失敗；修 Hold 不能恢復舊 campaign。AA 第5筆 STOP、餘7筆未跑、ABBA36未跑。
writer 故障 case 的 raw integrity 與 complete-prefix 分層也不能省略。

原 validator 只要求全 run own Down/Up>0，未要求 measurement 中非空 phase；同一 deterministic
Tap 12-frame loop 重複 120 幀僅一次 Down/Up，completed identity 可跨幾何循環保留。
未來新協議必須明列 warmup／measurement 的活動覆蓋和空分布，不能由全 run 一筆动作推論
後半段 action-cost 分母有效，也不能修改舊收據門檻來補資格。

## 新 Windows campaign 的前置配方（本包不執行）

目前沒有足够的共用 Hold workload，故沒有 eligible new normal campaign。

1. 先解決或界定觀測／持續 body／tail 缺口，用目前同圖 A/B 的完整自身 receipt 證明
   declared trajectory 範圍、正常完成與安全釋放各自的分母；保留原 unsupported Hold。
2. 若改 generator 或 strategy，另立新 campaign ID、完整源碼／binary／依賴／flags／profile／
   RGB hashes／host／背景負載 freeze；不得重開舊 AA、借既有結果設定 noise 或改原 oracle。
3. 新 root/report/stage 先做 qualified Windows process／identity／held／streams 驗證及容量核對。
   Release A/A 12 筆先行，raw 獨立重算所有分母／measurement metrics；必要 gate 失敗立即 STOP。
4. 只有新的全部 A/A validity/noise 通過、容忍事前機讀 freeze 後，才可依新授權做 36 筆 ABBA。
   未經 Windows exact-source 驗證，portable suite 不給 cost qualification。
5. true capture/RPC、五指指紋、physical Hold identity/contact/adoption 與 Chapter Legacy 全曲
   IN 完整 Miss=0 另有外部 gate；合成／成本綠燈也不替代它們。

## 重現与保留資料

- `cost_workload.hpp`：literal historical RGB reference＋有界 diagnostic raster，沒有 endpoint API。
- `cost_workload_tests.cpp`：四組 v3 witness 控制、舊 Hold/Tap A/B 全 owner 控制、完整 phase sweep、
  四類後續正反控制；每個 attempts/phase/receipt、owner events 與 fake lateness 都保留。
- [工作證據](workload_evidence/README.md)：探索失敗、全部 sweep 與 SHA index。
- [portable lane](../prelive_portable/)：最終 Release／Debug／ASan+UBSan fresh reports；
  結果與實際 command/source/binary/dependency SHA 由該 lane 的 evidence 保存。

最終每配置 **144/144 assertions 通過**，各自完整處理 1,080 個 Runtime frames＋48 個
v3 query frames；原 Hold 零動作及共同 phase 過窄作為已知負結果明列。
實際報告為 [Release](../prelive_portable/evidence/workload-release/report.json)、
[Debug](../prelive_portable/evidence/workload-debug/report.json)、
[ASan+UBSan](../prelive_portable/evidence/workload-sanitizers/report.json)。
ASan 的 leak detection 因執行環境限制關閉；不聲稱 LSan 或 Windows native 驗證。

原 source 與 reports 的狀態不可由本頁反向覆寫；任何合成結果保持 physical_gold=0、
windows_cost_qualification=false、normal_hold_tail_completion_proved=false。
