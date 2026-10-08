# main 的 emulator 啟動前工作包 — 2026-10-08

使用者授權繼續實作，**到需要啟動 emulator 就停止**。本包只執行 endpoint-free
Windows 建置、fixed pixels、R2 AA/noise/ABBA 和 literal production cold stages；
不啟動 emulator、ADB、遊戲、真 capture/RPC 或 touch，不新建 goal／worktree。

所有新來源、binary、native Job／input closure、raw 分母與失敗均按現有 gate
保存。R2 cadence、256/2304/2560、noise 公式、coverage／lateness／RSS 門檻不改，
失敗停止該批，後續 NOT_RUN；有具體修補才能建立新的 campaign，不能挑成功重測。

新的 unified CMake project freshly 編譯正式 22 TUs 及完整 production branch，
和 endpoint-free cold tests 共用同一 pas_core；pas.exe 只建置不執行。literal
manual_session 包含原 translation unit，替換的只有 capture/preflight/touch 邊界。
原 main07 三配置 compiler outputs 壓縮保存，每個 entry SHA 讀回核對及 originals
再次核對後才刪原 build roots；原始 raw、source、失敗與 receipts 保留原樣。
48 GiB retained／6 GiB candidate／12 GiB cost raw／20 GiB free 等界限不變。

完整 live reader／qualified token 必須以真實成功 production、cost 及當前
device/profile/physical 收據建立 schema；沿用交接的信任契約，不先創造可啟用
格式。本包在需要 emulator 的 profile/physical/capture/RPC 階段停下，入口保持
UNQUALIFIED，cold/comparison 的 PASS 不授予實機資格。
