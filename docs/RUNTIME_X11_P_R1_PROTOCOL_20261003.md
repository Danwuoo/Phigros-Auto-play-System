# X11-P-R1 預先工程與成本 protocol

2026-10-03 Asia/Taipei。只離線開發、自驗並交「待總控獨立驗收」。authoritative範圍為X11-P總控驗收的R1工作包；不進X12，不啟動emulator/ADB/manual-session/真觸控/模型，不commit/push或新chat/goal。main f83c7ea/唯一worktree/dirty保留；正式src/include/root CMake不修改。舊X1–X11 output/raw不重建或覆寫。

## 新方法及來源

新out/x11-p-r1從已核SHA的out/x11-p/B0、B1完整source複製；共同改動只包括metadata、原Wake逐字抽取共用header、manual-session的完整plan/receipt/release JSON抽取共用函式，以及SessionArchive可選離線量測統計與delay。默认archive路徑不開量測/delay。B1唯一策略差仍8行known absolute cursor0 pending missing hook；inverse逐檔核對。完整runtime/core/tests重新建於新根，保存compiled dependencies、DLL closure/profile/SHA。原live未列compiled SHA仍Unknown。

meter連新pas_core，用原正式high-resolution waitable timer Wake，SessionGameOwner(start/accept/poll/finish)與SessionArchive(event/complete/close)；原decision_json、全部accepted plan steps/cancel/coverage/notice/receipt和每次backend release都完整序列化。真capture/RPC、session HUD/result lifecycle、preflight/supervisor/render age/result PNG不涵蓋；RGB負載用GameObserver，owner負載用明示synthetic typed snapshot，不能稱完整manual-session等價或遊戲命中。owner options兩版{15,35ms,30ms}，與live lead35相符；lead0只用舊512-input bridge以核提取前後公共行為。

樣本硬限：latest1/physical3/decision1，normal RGB256 attempts/cadence16ms、owner128 attempts/cadence8ms/32 targets；stress各64 attempts/cadence8ms，owner128 targets、RGBconsumer24ms，writer5ms、fakeRPC3ms。四相jitter{-2,+1,+2,-1}ms；16合成pose沿用舊SHA，不產生PNG副本。縮短全JSON負載是事前容量選擇，不能外推原512×128 dense成本。stress保留128pending容量驗證；active rotation與完整語義以獨立契約測試驗。

每run frames≤257/receipts≤4096/release calls≤4096/archive計時samples≤16384/serialized row≤256KiB，archive既有queue8192與16MiB雙界維持，meter總event admission≤3MiB(normal)/6MiB(stress)，超限FAULT而不抽樣。全run raw與metadata≤4MiB(normal)/7MiB(stress)；預留24×4+4×7=124MiB，因此bridge不保留第二套巨大events檔：串流比較既有原bridge，保存hash與第一不同/EOF分母，bridge輸出上限各64KiB。tests/source/logs/ledger另預留4MiB，總batch≤128MiB；首次payload上界估算須在成本前保存，超估算不得追加成本。out≤3GiB、campaign+prior≤8GiB；實測開始8,217,563,371B，disk free207,621,382,144B（快照）；reserve3GiB+128MiB+5GiB。全bytes重算，失敗亦計額。

## 契約、負例及先验gate

保留原238 suite及pending13：B0預期8pass/5red，B1全13；B1原suite唯一預期fail仍SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt，不改期待。新增archive full event/missing row/open-write failure/mailbox/segment/closed-before-completion/after-close負例；逐testcase skip collector用舊B0 historical skip XML回歸，root缺省skipped不得漏計。normal gate不接受n0/null/nonfinite/denominator mismatch；只有預先no-action workload可將lateness標not-applicable，不稱通過時序驗收。

FakeClock驗due前/同時/後、通知/timeout驅動、gate/reset/收尾、failed/unknown release及unknown Down不重試/completed不復活。FakeWake是測試driver，與真OS Wake各自分類；OS checks驗pre-notify/timeout/wait0，QPC值只描述環境，不拿fakeclock報latency。共享提取與新meter用ASan（自有core/測試插樁，第三方不全面插樁），stress4額度中B1 owner一筆為ASan，不作Release成本。

X12容量選擇：保留C36h原32×16MiB=536,870,912B的可測硬限；明確修訂forward plan該pending-only六輪預案為512MiB/round。六輪journal最多3,221,225,472B，加六份result RGB/PNG保守各4MiB、manifest/summary每輪1MiB、standby4MiB/session×6，合計3,277,848,576B；另立future live根12GiB/reserve5GiB，錄影配額仍不自動授權。scaled32×小segment負例證明第33段拒絕；沒有把256MiB寫成已強制。本task不建立live根。

## 成本配額與停止

最多8 A/A(B0各load4run)、16 ABBA(各load2batch B0/B1/B1/B0)、4 stress；所有失敗計額。成本前freeze exact sources/binaries/options/environment/method/payload estimate。先8 A/A，再freeze noise；任何normal hard gate或noise adequacy不足立即停，ABBA=0，不用ABBA救結果。最多一次量測工程修復（失敗保留）；不改策略/門檻、不追安靜樣本、不另開profiling。沒有額外cost分解實驗：serialization/event enqueue/writer全分布是同一run的旁路統計。

gate沿原公式：前三metric recognition/owner/capture→owner，p95/p99 pair d=max(|1−2|,|3−4|)，T=max(floor,2d)，noise adequacy 2d≤max(min四run metric×.30,floor)，floor .5/.25/3ms。ABBA每batch兩B0/twoB1的run quantile算術平均差≤T，全批全metric須過。n/p50/p95/p99/max/p95−p5 jitter全部保留；不加stage p99。

normal全分母：published+pooldrop=attempts、consumed≤published、owner≤consumed且各≥.90attempts；failed/unknown receipts/release IDs=0、contacts_exit0、worker/archivefault0、完整raw/serialized row counts匹配、queue/bytes/sample/capacity合格。capture≤ready≤published≤recognition_start/end≤owner_start/end；capture→owner p99≤100/max≤250ms，all-phase scheduled→start p99≤15/max≤100ms。每phase另外報，缺某phase標unknown、不單獨當fail；all-phase正常應有樣本。stress允许跳幀但不允許silent loss/archivefault/未知release/殘留contact或無界累積。寫檔失敗或缺row必失敗。

## 交付

新protocol/result/handoff，機讀summary、命令/exit/XML/ASan/source inverse/new runtime SHA/compiled dependency/closure/profile/raw/全部失敗與容量；status下一節與forward plan寫「交付完成，待總控獨立驗收」。Verified/Strong inference/Hypothesis/Unknown分列。not-ready有效，不自簽總控或自進X12。14lost opportunity/77Miss/跨曲遊戲效果仍Unknown。
