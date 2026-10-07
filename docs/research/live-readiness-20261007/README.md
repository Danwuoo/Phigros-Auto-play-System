# 2026-10-07 Windows readiness 接續

使用者本輪明確要求接手 source-only ZIP，完成 Windows 實際成本、資格驗證及實機驗收。這是新的工作授權；交接文件內「沒有 device 授權」記錄的是上一輪範圍，不是本輪使用者要求。但新授權不能代替成本、裝置、physical 與解鎖門檻的證據。

原包 SHA256 `515b26d075ddfbf8a3f74a1925c0fb7ae2290afc5b42787c7aaf024e5b621ef1`，候選 commit `ecae2c51246bf8e962a21afad7532f73a01e5001`。主 checkout 未修改；本輪位於 `C:/Users/wurre/Desktop/PAS-windows-readiness-20261007` 的 `codex/windows-readiness-20261007` 分支，未 push。

## 身分及保留

- 原包所有 manifest／SHA256SUMS 已核對。舊 compact ZIP 13,670 個保留 payload 全部逐流核 SHA 和 bytes；不將 ZIP CRC 當 source 身分。
- 舊 Windows R4 checkout 已不存在。只恢復原 freeze 必需、且 SHA 完全匹配的 compiler snapshot／歷史 source／generated protobuf／receipt，未恢復被刪 raw 或執行舊 STOP。原 36 oracle failures 保留。
- fresh file-only 20/20 與 native 四控制通過，原 Job／identity／held／streams／期限／64 KiB freeze core 未修改。
- 第一輪 direct functional root 在 child image identity 不可確認時 STOP。功能測試可採與既有 CMake wrapper 相同的 command envelope，完整保留及驗證所有 Job 成員、實際 test exit。這只提供功能回歸，不能作 direct-producer 成本資格。

## Windows 發現及修補

1. `ByteVariants` 的 culture-sensitive `StartsWith(BOM)` 把沒有 BOM 的 UTF-8 當 BOM。改為 Ordinal；65 個實際 LF／CRLF／BOM／invalid UTF-8 控制通過，原 119 個 recipe static checks 通過。
2. 原 analyzer unit fixture generator 使用 text-mode `ofstream`，Windows 寫 CRLF；scoped independent review 使用 binary-mode，重建 LF 後與原 auditor SHA 不符。修成固定 binary-mode LF，保留原 failure／fixture 和 attack oracle，另建立新的 source migration 與 fresh 三配置驗證。

初輪 Windows Release／Debug／ASan 全部 fresh build 通過。完整 Win32 root 的 `pas` 與 disabled-live-seam target 亦編譯成功，包含 `manual_session.cpp`；當時 source dirty 已照報，未執行 production binary，不能作成本或 live 資格。

## 唯讀資格 CLI

`research/windows_qualification` 提供獨立 C++20 `pas_qualification_verify`：

```powershell
pas_qualification_verify.exe --build-binding <current-build.json> --expected-sha256 <externally-pinned-SHA> --source-commit <40-hex-commit>
```

CLI 核對現有 cold build／native gate／tool snapshot／migration target／configure-build input closure／COMPLETE receipt 的實際 SHA、links 與 scope。JSON 有容量、深度和重複 key 限制；完整 closure 最多 8192 files／12 GiB，驗證結束再查 bytes。它沒有 device API／process launcher，不呼叫 admission continuation、不產生可用 live token；成功也固定 `live_ready=false`。真實 Windows build 正向及 14 個拒絕控制已通過。

這是 cold 收據驗證器的完成部分。完整 production packet/action/wake/capture/RPC 與 physical 收據尚不能由 cold build 或 AA 代替，`CandidateLiveAdmission` 保持 UNQUALIFIED。不能為了測成本先走實機入口。

## 順序與界限

先完成同源三配置 affected tests、positive-before-attacks controls、family，以及原 3063 WIC prefix 和獨立 raw audit，再 freeze 一次 AA12。AA 使用原 20ms cadence、256 warmup＋2304 measurement、原硬門檻及 noise 規則。第一個必要 validity／process gate 失敗就停止，未跑列 NOT_RUN，不更換 corpus／threshold 或補安靜重測。

所有 stage 保留 raw 與失敗；輸出仍受原 12 GiB／metadata 64 MiB／free 20 GiB 上限。Windows ASan 與 Linux ASan+UBSan 的能力不同，不能聲稱 MSVC 覆蓋 UBSan。

實機驗收要先有指定裝置、當前 app／profile／五指映射與 physical qualification；manual-session 仍由使用者選曲及按 Play。Chapter Legacy 目前分母／逐曲 IN 解鎖 unknown，全曲完整 IN Miss=0 **未完成**，HD 不推定 IN，AP 不作前置。

實際最新結果、STOP 與產物索引在本入口後續附加；測試設計和本文件的「順序」不等於已完成資格。
