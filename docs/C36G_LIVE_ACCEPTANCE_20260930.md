# C36g 首輪 manual 驗收啟動

2026-09-30，使用者於12輪封存及C36g冷優化後明確要求「開始實驗驗收，
幫我開啟 emulator，使用 manual-mode」，並確認已在選曲頁面待命。
本次授權為新驗收階段，不重設／追加前一批12/12結果、不恢復無限AP。
先記一輪初次C36g驗收；之後依結算／故障和使用者指示決定續作。

資料在原8GiB容量範圍內的`acceptance36g-01`，獨立acceptance.json；原
capacity-ledger.json另列live_acceptance，原12輪attempts不修改。profile
只換log_dir，lead35ms／uncertainty30ms／五指／gRPC payload fast256KiB維持。

emulator AVD phigros，serial emulator-5554，port5554／gRPC8554、token認證、
no-snapshot-load。啟動後直向首頁preflight因405×720／rotation0失敗，沒有
建立PAS touch owner。Phigros在頂層時送出明確launcher intent，未選曲／按Play；
之後1280×720／rotation1 preflight fingerprint_matches=true、mismatches=[]。
此read-only核對不是遊戲動作語義驗收。

PAS使用已凍結`C36g-current-note-line-support`，SHA
`1e2e79b2a4887fd97abbcdd07d4cf52f780a2902bd4577c4d2a34d1de6e10452`。
12個DLL SHA、profile／capability SHA與18個live manifest compiled source SHA
已核對。source e23b3f9+dirty，不使用Desktop的49/26建置、不重編譯凍結版。

入口為`manual-session --no-preview --pixel-clips --round-watchdog-s 360`。
automatic_play_enabled=false；先以STANDBY等待使用者選曲／Play。watchdog只
辨識異常停止，不把360秒當曲尾。初始STANDBY已核對：PID25156、session
manual-session-16106368637700。建議同Dlyrotz IN13與凍結C36f的兩次81Miss比較；
曲名只作人工比較metadata，不回饋策略。

初始啟動本身不算開局，當時attempts_started=0、gameplay_validated=false。
實際STARTING／PLAYING或中止須另記分母。完成後保存原始結算及segments／RGB
SHA，核對P/G/B/M、Hold／線關聯、觸控／釋放與時序，再正常停止本次初輪PAS。
不自動選曲／重試，不挑最高成績；未知Down不重試，錯接／失效不釋放或來源
故障即停止並保存證據。結果／幾何差異不是逐Note人工gold。

## 首輪完成：轉向疑似 Miss 影格標註

使用者自行選Dlyrotz IN13／Play，session完成STARTING→PLAYING→RESULT→STANDBY。
結果796284分、491/18/4/71，584判定，max combo126、accuracy86.08%、Early/Late15/3。
比C36f兩次81Miss少10，但仍不支持大幅改善或穩定泛化；使用者亦表示沒有大幅改善。
PAS已正常Ctrl+C exit0、session STOPPED，沒有追加輪次或重新啟動emulator。
attempts_started=1、completed=1、active_launch=null，原12輪封存不變。

result／manifest／兩段events SHA核對相符，27RGB SHA由C++工具核對；unknown
contact receipts0、release failed／unknown皆空。主機來源絕對age仍unknown。
7781 decisions、1781 touch receipts；playing capture interval n7316，
p50/p95/p99/max=16.57215/31.26030/45.151185/369.763ms，jitter p95−p5=28.682325ms；
recognition n7781，1.8397/4.617/7.11386/16.3963ms，jitter p95−p5=3.5753ms。
host residency n7781，3.0336/6.015/8.73658/26.8657ms，jitter p95−p5=3.8815ms。
完整分布及觸控排程／RPC成本在analysis.json與原summary，不把子段p99相加。

72次已開始Hold撤銷、2次Down前Hold撤銷、visible tail confirmations0；是診斷
分母，不等於72個Hold Miss。line-history有線而全無效僅2個單幀、derived geometry
association差異0；仍不能將所有Miss歸因給線ID或時序。

離線整理9組／27張RGB：228疑似事件中僅7個±500ms內有圖，221個缺圖，僅
一個事件source_frame保存。第一個例為frame27449的Hold身分歧義撤銷，當前可見
body仍在水平線，原ID1052與新rails ID1063切分；proposed而非Miss gold。
標註產物與資料缺口見[MISS_FRAME_ANNOTATION_20260930.md](MISS_FRAME_ANNOTATION_20260930.md)。
