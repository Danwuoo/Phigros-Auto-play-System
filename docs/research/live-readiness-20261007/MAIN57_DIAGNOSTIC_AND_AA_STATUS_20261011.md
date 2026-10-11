# main57 診斷與 base55 AA 實際結果

實際 diagnostic compiled source 為 `5dce75d12c63ed51de5d996cedcd44e29d729841`，重用 library base source 為 `12ec0550e882e3b06751d67fa67c43dda3e9312b`。獨立三個 TU 與 16 項控制通過；以下三場各執行一次，只屬 diagnostic component。原 base55 正式 Normal Tap STOP 保留，不授 Normal3、cost、whole candidate、physical 或 live 資格。

| 場景 | 完整 raw auditor | producer／callback 超期 | 原數值 | 最大相鄰 QPC 間隔 |
|---|---|---:|---|---:|
| Tap | PASS，11 檔／3,455,536 B | 0／0 | true | 14.4936 ms |
| Hold | PASS，10 檔／3,059,223 B | 0／0 | true | 26.1576 ms |
| Dense | PASS，53 檔／27,272,967 B | 2／2 | true | 16.0759 ms |

Dense 兩次超期發生於 render（23.9395／29.1035 ms），照報，沿用原 Dense gate。QPC 間隔超過 20 ms 本身不等於原 due-relative deadline gate 失敗。這些結果沒有重現原 base55 Tap 的 168.9509 ms active-wait 事件，亦未證明已修復或確立 OS 根因。

[完整保存索引](MAIN57_DIAGNOSTIC_PRESERVATION_20261011.json)包含九個實際 Native stages／72 個 Native8 pins、950 份 source snapshot 與 780 份有界 metadata 的前後 SHA 核對。原 main55／main56 負結果不覆蓋。

另外獨立執行原本 NOT_RUN 的 base55 AA12；compiled source／binary 保持 base55，execution source 為 main57。r01 在 Native 啟動前因 fileless PowerShell body 的 maintenance path 為空而停止；r02 的首個 producer Native2／runner2／wrapper1，C++ canonical 拒絕 264 字元來源快照路徑，沒有 frame。兩次原件均保留。

r03 將原 939 份 source 的 exact bytes 全量複製到 main 內 `out/s55aa03`，原件與副本逐 SHA 核對，最長 205 字元；原 946 份 snapshot 保存不動。新 Native active inputs 使用短來源副本及原七份方向 metadata，舊 Native leaves 僅 pin metadata 本身，避免再次 canonical 原長 payload 路徑。R2、12 筆 A 固定順序、workload、公式、上限及 fail-STOP 不變。此修正排除了來源路徑啟動錯誤，沒有修復排程時間問題。

r03 首筆 Tap 的 producer／auditor／gate 三個 Native 均0、runner／verification 均0，完整 raw integrity 通過；2560 張 published、2381 consumed、measurement owner_seen 2125／2304、pool drops0、writer drops0／fault=false、release verified=true。原數值 producer／publication deadline overruns40／40，gate `tap_streamed_workload_and_cadence` 失敗，首筆 NOT_READY、後11筆 NOT_RUN，沒有 noise／ABBA 測量。

主間隔在 measurement attempt2462：前 publication return 至 render start 跨787.7784 ms，schedule lateness768.03 ms，render .2996 ms、render 至 publication return .4184 ms；後續沿原 absolute epoch 追趕，不能把40次超期當作40個已確認獨立 stall。原 AA binary 沒有相鄰 QPC telemetry，不能把該整段間隔直接稱為 wait、OS或guard根因。只讀程式核對未找到固定約50秒觸發動作或可證明的 producer bug。

[r03 原始負結果](MAIN57_BASE55_AA_NEGATIVE_20261011.json)包含三個 actual Native／24 pins、84 closure leaves、typed before／after=true 與180份 metadata 的前後 SHA；attempts／journal 共12,164,478 B 的 SHA沿用實際完整 auditor binding，保存時只核長度，不冒稱重新 hash raw。Native0 不代表數值 PASS。

latest completed noise 仍為 main44 的6／18 adequate、12不足，ABBA0／36。原 ABBA36 runner 已準備 PENDING 範本，沒有填入預期 PASS 或 SHA；必須取得實際 AA12／noise18 完整合格收據才可執行。current Normal3 仍是既有冷 gate，不能用本 diagnostic 三場替代。必要 validator／collector／score-only 入口與冷控制已實作，真正 device／profile／capture-RPC／physical 收據需在 emulator 邊界後取得。

原 main32 的75／graphs3／prefix3063／fault4／production9 PASS 保留、不全盤重跑。cost NOT_READY、live UNQUALIFIED；沒有啟動 emulator、裝置、Computer Use 或新 thread。本文件是实际盤點，不能把較晚文件提交當作 compiled source。
