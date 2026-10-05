# Chapter Legacy 全曲 HD V／S 目標接續

日期：2026-09-30。狀態：接續點已核對；尚未完成全曲驗收。

## 授權與驗收

使用者本次要求所有 Chapter Legacy 曲目 HD 達到 V／S 以上並盡量提高實際分數。A36 恢復與四曲比較只是中間步驟。正式策略保持即時 pixels、有界近期追蹤、C++20、單一 touch owner；不讀譜、不按曲名調參、不重播歷史動作、不訓練模型。選曲與 Play 仍由使用者手動完成。

V／S 的精確數值門檻尚待遊戲說明核對。待核對的工作假設為 V 960000、S 920000；暫以較高的 960000 記錄差距，不宣稱門檻已驗。最終須同一套通用候選、完整結算分數與畫面評級、版本／環境／原始證據鏈；不能將不同版本的最佳分數拼成已通過版本。

## 真正接續點

- 主 checkout：`codex/four-song-hot-tuning`、HEAD `25d464564c12acdf6945242c6dbd1b0c63a4c1ea`，observer49／planner26 有既存未提交 source／tests／docs。本次檢查未修改或丟棄這些變更。
- 隔離 worktree：`C:/Users/wurre/.codex/worktrees/baseline36-recovery/Phigros-Auto-play-System`，分支 `codex/baseline36-recovery`，HEAD `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`；首次檢查為乾淨，其後出現其他執行工作正在修改與建置的 C36e（見下段）。
- A36 凍結 runtime：`out/main-legacy-v145/Release/pas.exe`，SHA-256 `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`，本次重新 hash 相符。
- C36c 最終候選：`measurements/game-assist/2026-09-30-baseline-recovery/candidate36c-pas.exe`，SHA-256 `2d4333d0d7ac80464fb53da2d5ef95be7f31409b2dc49fc3712b90ecd0fdef93`，本次重新 hash 相符。保留 row prescreen、受限相交線候選及單線 fallback 保護；C36d 唯一可用長線 fallback 已回退。
- 上一批 16／16 完整結算已完成，raw session 共 1710164243 bytes。候選尚未通過四曲同期驗收，不能升為預設。上一批記錄最終 Release 210／210；本次尚未新跑回歸。
- 檢查當時無 PAS／emulator／qemu／adb process；本次未啟動 live owner。後续每次啟動前仍須重新檢查。

詳見[原恢復計畫](BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md)、[16輪實测](../../../measurements/game-assist/2026-09-30-baseline-recovery/restart-live-test-report.md)、[既有組合庫盤點](../../../measurements/game-assist/2026-09-30-baseline-recovery/combination-bank-inventory.md)。

## 逐曲基準

下表十九首是 A36 2026-09-28 同一 binary 的歷史 HD 結算，加上另一批的 Glaciaxion。現行章節清單尚未由遊戲核對；既有 `roster.json` 也明列 `chapter_listing_verified=false`。不可稱本表已證實為完整現行 Chapter Legacy。分數來自既有同源結算報告，本次已目視重新核對 Dlyrotz 原圖為 HD9／961572／V／452-0-0-6，其他圖片尚未逐圖重新目視核對，亦無新增實戰分數。

| 曲目 | A36 歷史 HD 分數 | 距暫定 960000 | 原始 round |
| --- | ---: | ---: | --- |
| Eradication Catastrophe | 818000 | 142000 | 108176133899800/1 |
| Credits | 582690 | 377310 | 108176133899800/2 |
| Dlyrotz | 961572 | 0 | 108176133899800/3 |
| Engine x Start!! (melody mix) | 919792 | 40208 | 108176133899800/5 |
| 光 | 943762 | 16238 | 108176133899800/6 |
| Winter ↑ cube ↓ | 865601 | 94399 | 108176133899800/8 |
| 混乱-Confusion | 743692 | 216308 | 108176133899800/9 |
| Cipher | 879884 | 80116 | 108176133899800/10 |
| FULL AUTO SHOOTER | 704846 | 255154 | 108176133899800/11 |
| HumaN | 903153 | 56847 | 108176133899800/12 |
| [PRAW] | 895722 | 64278 | 108176133899800/13 |
| Cereris | 855164 | 104836 | 108176133899800/14 |
| Pixel Rebelz | 751257 | 208743 | 108176133899800/15 |
| Non-Melodic Ragez (MUG Edit) | 887993 | 72007 | 108176133899800/16 |
| Sultan Rage | 817320 | 142680 | 108176133899800/17 |
| Class Memories | 897218 | 62782 | 108176133899800/18 |
| -SURREALISM- | 764396 | 195604 | 108176133899800/19 |
| Bonus Time | 909078 | 50922 | 108176133899800/20 |
| ENERGY SYNERGY MATRIX | 865043 | 94957 | 108176133899800/21 |
| Glaciaxion | 885458 | 74542 | 3293220942400/1 |

路徑格式為 `measurements/game-assist/manual-session-<session>/round-<round>/result.png`。完整 P／G／B／M、Max Combo、PNG／summary SHA 與版本請由[十九首結構化證據](MAIN_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)及上述恢復 campaign 读取。Glaciaxion 同批 A36 後續為867239、781209；不可只用885458宣稱穩定。

## 下一步與資料界線

1. 保留 A36 和 C36c 及新版 G1–T2／R1–R7 資產；先用現存 RGB 核對線／Note 關聯反例。少量人工 gold 尚缺，不將 proposed 升格；以可重現幾何契約建立回歸後才移植一項動作機制。
2. 新實戰另列有限批次與容量帳本，保存結果、事件、版本、QPC 分布及失敗；不續寫已滿16輪的舊 campaign，不無限重試。Vision dataset 已達20-run retention cap；不刪旧資料、不換 root 繞上限。預備新結果批次時保持 dataset 和新增 pixel-clips 關閉，除非另有必要且明確授權的容量政策。
3. 重新檢查 process、啟動 emulator、核對 profile／五指 fingerprint，A36 lead35／uncertainty30 進 STANDBY；由使用者選曲／Play。優先補尚缺的 Credits HD 同期對照，以及能區分候選行為的 HD 反例，結算後分析。
4. 目前執行工具間歇回報 `exec-server transport disconnected`／initialize handshake timeout；Computer Use 所需 `node_repl` 未在本會話工具中提供。需恢復可靠本機執行連線才可建置與啟動，不假稱 READY 或已測試。

## 接續時發現的並行工作

後續讀取發現同一 worktree 新增未提交 `src/game.cpp`、`src/game_tracking.cpp`、`src/manual_session.cpp`、`src/runtime.cpp`、`tests/game_tests.cpp`，manifest 稱 `C36e-current-tap-overlap`。這些修改由另一個執行工作產生，本會話未寫入。當時有 MSBuild 在建置；因此未啟動第二次建置、emulator 或 touch owner，已請使用者確認正式開發／實測由哪個工作負責。

該候選只擬處理已有至少兩次近期 Note 觀測、先前無 line sample、當前 Tap 與當前可見長線重合且已有接近運動的路徑，owner 再核對当前線、幾何與 Note 類型後立即 Down／18ms Up。這是 source 意圖，尚未完成本會話的實作審查或回歸，不宣稱解決所有突現 Note。接手前需重新取得 dirty patch、binary SHA、測試結果與程序狀態，不能把初次讀取的 e23b3f9 clean 狀態當成最新候選來源，也不能稱該行為是原凍結 A36。

使用者已回覆由正在建置 C36e 的工作繼續；本會話交付基準與接續記錄後停止重疊開發，未啟動任何 capture／touch owner。全 Chapter Legacy HD 目標仍待該工作逐曲完成，不能將交接視為目標完成。
