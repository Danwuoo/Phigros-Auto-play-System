# 交付結果：冷工程與測量工具已驗；實機仍 NOT_READY

本輪到達 **COLD_VERIFIED_FOR_WINDOWS_QUALIFICATION**，整體 **NOT_READY_FOR_LIVE**。
已接上預設不可啟用的 production C++ API 接縫；Windows 編譯／成本、正式啟用 validator
及 physical／device 許可各自仍有 gate，不能直接開遊戲。
起點 main `e9e305e3aba13f05a786c74540419428610b2f76`；所有新提交只在本分支，未 push。
獨立 checkout 與 IRIS／原研究根分離，沒有裝置、emulator、Fixture、真 touch、模型或付費運算。

## 實際修復

1. **false READY 證據漏洞**：拒絕同一 Tap 重包十二筆 A/A、偽造場景／延遲摘要、錯誤
   plan/source/cursor/contact/coordinates、unknown release 被後來成功覆蓋。完整 raw 分母與
   quantiles 重算，source/binary/profile/campaign、直接 owned native process 與全部 receipts 綁定。
2. **當前 body 粗界限**：自身 ACTIVE Hold 在沒有可見 tail 時，允許使用當前 observer 已量的
   有限 rail depth 作粗空間界限；仍重新驗當前 RGB body/rails、所有 line 競爭。不借歷史 body、
   不新增 Down、不把 depth 終點當 tail。
3. **正常尾端通道**：新的 line-aware current tail 量測先排除判定線的白色偽邊緣，再做單一
   terminal/rails/connected-fill 檢驗。typed terminal-only 只可結束同一 own-ACTIVE Hold；兩份
   fresh 證據跨≥10ms 才按既有+20ms規則安排 Up，保留原 evidence/expiry/contact/point。
   已確認後不可復活 Move、續 lease、alias 或 Down。重複／降級／alias／未知注入反例已修。
4. **第一次 uncertain release**：Runtime 保存原 unknown/failed report 與事件，阻止重試及
   覆寫；verified round 才能進下一 round。原 throw-before-drain 失敗已重現並保留。
5. **有據的固定 renderer**：保留原錯誤 Hold generator；新81相位 closed-reset family有慢接入、
   顯式空窗、可見 closure。新 `hold-closed-reset-v2` cost scene只重用一張2,764,800B RGB，
   243種相位／offset bytes與固定參考完全一致，沒有648幀快取或按歌調參。
   render／publication-return／owner分段與全 attempt 都記錄；schedule anchor、deadline與
   raw join 防止慢 producer 把 coverage做得看似更容易。沒有執行新 Windows cost campaign。
6. **真 production 接縫、仍硬閉鎖**：抽出 sole-owner action step，研究 Runtime 與
   manual-session candidate C++ route 共用；原 donor CLI 不變，沒有 enabling flag。新入口
  只有 UNQUALIFIED 型別，在 config/preflight/discovery/backend 前拒絕。same-frame immutable
   RGB lease＋完整 batch、唯一 backend 的有界 receipt decorator、implicit release 與第一份
   uncertain release 都已接入；沒有另建第二個 observer／owner。
7. **到期先於續 lease**：既有 family 證實 old evidence1620ms、due1720ms 時，capture1640ms／
   now1720ms 的 delayed packet 原可在 production accept-before-poll 續命。共用 accept 現先
   poll＋sync；兩路均先release、active0、Down1。原反例保留，新增6項 expiry checks，
   原91項不變。
8. **64KiB freeze 的實際容量**：原 ASan native freeze 已近硬上限；未放寬 cap，改用
   bounded root/leaves＋前後hash receipt，綁原 core COMPLETE/state/verifier。缺closure、
   STOP、漏source、錯root／count都拒絕。Windows source migration 以 old/new canonical
   blobs 與確切LF/CRLF hashes銜接；不跳過舊source guards或手造舊收據。
9. **證據不覆寫**：所有舊 STOP／失敗／oracle保留；原報告或任一 raw sidecar存在即拒絕，
   persistent atomic reservation亦拒絕同report的同時啟動。

## 自主驗證與獨立反例

三配置是 Linux GCC Release、Debug、ASan+UBSan；不是重做／冒稱 MSVC 三配置。

| 驗證 | 每配置結果 | 證據與範圍 |
|---|---:|---|
| 正式 owner/front/current/own-prefix 與新 body/release/diagnostic | 646 checks pass | 113/56/68/113/57/145/35/15/32/12，各來源前後SHA相同 |
| scheduler | 66 checks pass，另1個Flick case | 既有不變時間契約 |
| typed tail獨立review | 130 owner +57 pixel pass | closing、duplicate、downgrade、alias、unknown、rotation等固定反例 |
| 兩份完整 family＋normal-tail核對 | 104 +104 +25 pass | 全12格分母及每筆phase/receipt，不挑成功subset |
| disabled API／packet／receipt／expiry | 47 +63 +97 pass | 原runtime91加6固定expiry反例；production入口spy為0、硬UNQUALIFIED |
| cost admission獨立matrix | 36 +152 +35 +14 pass | 152含123來源＋29語義；35含31closure負例；14用真auditor讀合成raw，無Windows cost READY |
| streaming pixels／producer arithmetic／output reservation | 252／15／9 pass | 分別為固定pixels、raw時序及檔案競態；不把它們加總成不同實戰案例 |
| PNG decoder | 11 controls pass | 官方libpng headers與既有同版library；108個原WIC RGB取樣全部相同 |

主 runtime 合計每配置 **1339 checks＋1 Flick case**；三配置4017次check executions，
不是4017種不同案例。PNG/WIC／cost／helper分層計數，不混作實戰分母。

最後 expiry 修補是 inline header：實際 compiler dependency、flags/toolchain及13個archive
member bytes證明既有library未受影響；只復用這些已驗lib，**全部test/prefix executable重新
編譯、完整矩陣與3063重新執行**。不是虛構三次fresh全library compile。成本控制binary也有
15份actual compiler dependency proof；Windows recipe與runtime header對那些工具只是
source-identity輸入，所有fixtures依最終bytes重造並完整重放，沒有重貼舊收據。

ASan/UBSan無最後失敗；LSan因容器ptrace不可用，初次fatal留存，後續明示關閉leak scanner。
最後 CMake/run.ps1 的測試 target／Inputs／scene 整合只完成靜態審查；其最後靜態收據與
完整cost控制另有source seal，不借Linux矩陣宣稱recipe已跑。research CMake只build
endpoint-free chain/pure runtime prefix，**不會編譯完整 production Win32 manual-session**；
後者仍須新Windows root build。CMake/MSVC/新Win32 producer entry未在Linux執行；原Windows186項與3938 frozen斷言只按其
歷史收據分讀，本輪沒有把原36 fail、timeouts/skips改綠。

完整入口：[portable results](../../../research/prelive_portable/EXPIRED_FIRST_FINAL_RESULTS.md)、
[最終shared seam review](review/live-seam/RESULTS.md)、
[producer admission matrix](baseline/final_candidate_cost_review/matrix_report.json)。

## 3063原PNG：同一最終來源完整重播

所有3063PNG按frozen selection核SHA，從起點開始、自己建立owner/receipts，沒有seed舊按鍵。
Release完整處理3063/3063；自身 **Down55／Move121／Up51**，ledger有效、最後release verified、
active=0、stop為空。9889 events／27,594,746 bytes全部保留。最新獨立auditor確認完整
source/plan/cursor/contact/coordinate joins與所有輸入，errors=[]。

report SHA256：`1c41c6a2842284d6c1bc5e28ad92fa2fb41dab55059003aaaffcd3db830d4683`。
獨立原始報告見 [final prefix audit](review/live-seam/expired-first-raw-audit-3063/audit.json)。

**重要：這份真圖重播的 typed-tail targets、tail-confirmed及正常tail Up都是0。**
固定合成family的B每offset8次（共24）normal-tail完成，不能換算成真圖／遊戲成功。
實際前綴的合法尾端機會分母仍 unknown，不能由0事件推斷「沒有機會」。中性灰gradient body能續接，不代表目前需六列cyan的terminal producer也具資格；實际灰尾／
line occlusion／tail角色與遊戲採納仍需定點驗證。[完整計數](review/live-seam/expired-first-raw-audit-3063/tail-counts.json)
獨立列出，phasecounts與前一安全checkpoint相同，沒有新Miss改善主張。

## 原件、容量與失敗

4,104個最新seal、3,715個歷史seal、13,670個ZIP payload與全部CRC已核。f07dc40是e9的直接父提交，
僅包裝差異。3063原PNG已核；舊71個STOP只有sealed identity，compact不含原始bytes，未假稱重驗。
Windows來源LF/CRLF差異有精確映射。AUX保留名只在隔離Linux還原，沒有覆原工作根。

保留全部新反例／編譯錯誤／source-race／LSan限制／手動容量中止與舊失敗。共享空間曾跌破20GiB
底線，所有新stage立即暫停；以SHA核對移除自己重複PNG、hardlink byte-identical副本、無損封存
自建.o及較早Linux測試binary/lib後恢復。沒有丟失原件、raw、STOP、source或binary bytes。
舊binary若已unmaterialized，按[還原bindings](capacity/old-binary-lossless-archive.json)從verified ZIP恢復；
部分已完checkpoint與final的自建Linux executable亦無損封存；其source、全部raw與
原始payload未移除。還原表應依最新capacity與portable preservation receipts，不能把舊路徑
仍有可執行bytes視為前提。這些cloud原生binary封存不構成Windows執行環境。

## 最終readiness

- Software cold verification：PASS，含same-source完整prefix及獨立反例。
- Measurement design：固定pixels與streaming／admission工具可交接；A只entry/safety release、B多持續body/normal tail，工作量差異照報。
- Same-host Windows qualification：NOT_RUN。新MSVC build、fresh process qualification、全分母A/A/noise→必要ABBA、real capture/RPC仍待驗。
- Candidate production C++ API：source-integrated、cold shared-step已驗；完整Win32 branch未編譯／執行，live admission validator未實作且硬閉鎖。
- Live CLI／physical／permission：NOT_READY。原donor CLI不變；目前裝置/app/mapping、灰尾與line occlusion採納未核，本輪device/touch未授權。
- Chapter Legacy目前完整分母／IN解鎖／同版全曲完整Miss0：仍unknown／未完成。

外部gate是具體新source的Windows/toolchain/Win32執行證據，並非宣稱剩下問題只有硬體。
啟用validator須根據真實qualification收據實作；不得以AA ready或任意bool開啟。

下一步是[Windows接手prompt](WINDOWS_HANDOFF.md)，先冷重建與成本；任何live步驟另需明確授權。

## Source-only 交付對應

封裝檢查發現早期獨立review提交曾包含兩個自建Linux ELF（共794,296B）。
已保留原bytes、SHA與Git歷史，只從目前source tree移除並ignore；全部raw/source
不變。交付使用以e9為parent、與最終開發tree完全相同的獨立snapshot commit，
避免增量bundle帶入這兩個歷史binary objects。HANDOFF_MANIFEST明列development
commit、delivery commit與相同tree；這是封裝修正，沒有新的程式／測試來源變更。
