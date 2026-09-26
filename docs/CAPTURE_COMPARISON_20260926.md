# 五擷取路徑集中比較（2026-09-26）

狀態：依使用者最新縮短規格收尾。第四版原 96 批日程停在 40 批已記錄（37 成功／3 失敗），另有一批被中斷；原日程未完成。取消剩餘完整矩陣、十分鐘與三十分鐘長測，只補必要的 20 秒恢復與 120 秒短期穩定性。`performance_pass=null`；結論為初步選型，未完成本版長期穩定性驗證。最新規格見 [計畫修訂](CAPTURE_FIVE_BACKENDS_PLAN_20260926.md#最新使用者修訂縮短剩餘測試)。

## 固定版本與環境

第四版 source revision `2d43dd74811aab75c3968a2c21aaea2dc4b28dbd`，dirty diff 為空。Release EXE SHA-256 `2447a4143a4b35f045f22e9345b462b4ed176bee20db9a937bfc38ab7ecc0d68`，EXE／DLL 副本及全部 hash 在 `measurements/frozen_2d43dd7_release/`。正式程序從此副本執行。

| 項目 | 固定配置／證據 |
| --- | --- |
| 主機 | Windows 10.0.26200，18 logical processors，約 32 GiB RAM，AC power |
| 顯示 | 1920×1080、60 Hz、DPI 120；NVIDIA RTX 3050 6GB Laptop 的 DISPLAY5，monitor 0；Intel Arc 沒有附加 output |
| AVD | phigros／emulator-5554；Emulator 37.1.11.0 build 15917651；host GPU、5 vCPU／8192 MiB |
| Guest 實際 | Android 16／API 36／x86_64；5 processors；MemTotal 8,130,816 KiB；自然解析度 720×1280，density 320 |
| 視窗 profile | standalone、fixed-scale、window.scale 0.8；client 1280×720、初始 origin (125,163)、DPI 120；HWND 0x240bc4；環境介入後及短補測 origin (159,235) |
| RGB endpoint | CPU、top-down、RGB24、1280×720；gRPC source rotation 1，其他 backend 輸出 rotation 0 |
| 裁切 | WGC top-level crop (1,38)；DXGI client crop (0,0)，monitor 0 |
| Fixture | native-v2 四區 identity＋binary X 真值＋低對比／彩色邊緣＋可控靜止；APK SHA-256 `195b6a0673c5d2182fe9b9face60cbfae8457dcf4c46bb82a873ad99d35fe964` |
| H.264 | 官方 scrcpy v4.1；c2.android.avc.encoder 軟體編碼，8 Mb/s、max 60 fps，audio/control off；FFmpeg 9.0.2 單解碼 thread |
| 積壓 | gRPC 控制／優化與 WGC 相對 lag 上限 250 ms；scrcpy 相對 PTS 上限 250 ms、最多一次重連；MMAP 只供診斷 |

完整 manifest 與預寫日程見 [第四版 campaign](../measurements/five_formal_20260926_r4/campaign-plan.json)。同候選不同批次的實際來源率仍須比較，不能把目標頻率當成實際值。GPU 動態樣本位於 `resource_sample.gpu_engines`；static host probe 的 `gpu_engine_samples=null` 不替代動態取樣。正式程序 PID 20400 的獨立盤點與 QPC bracket 存於 `campaign-process.json`，用於離線對應 GPU engine instance。

全部原始資料、失敗／取消分類、凍結輸入與 SHA-256 見 [證據索引](CAPTURE_EVIDENCE_INDEX_20260926.md)。

## 方法與時間定義

原計畫為 96 批：三個目標 40／48／57 Hz、每配置各三批，10 秒暖機＋60 秒正常窗口；每配置九項壓力／診斷、五個十分鐘及 MMAP 診斷。實際完成的正常批次按原窗口完整保留，沒有裁短或挑選片段；尚未進入該版壓力與長測。每批前十秒期限內，以 ADB pixels 讀到兩個不同四區 counter 才 READY。失敗記退出碼、原因及部分 raw，不無限重試。

正常順序 `(rank + round + 2*hz_index) mod 5` 預先固定。原先計畫的壓力包括 slow50、recover100（15 秒後解除）、pause500、CPU thread、256 MiB memory、static3、獨立 GPU load、preview off/on；r4 尚未執行這些完整場景。GPU load 與預覽的開發證據另列於就緒紀錄。

縮短補測的 [預寫日程](../measurements/shortened_followup_20260926_01/followup-plan.json) 固定 48 Hz、3 秒暖機；gRPC fast／WGC 各一次 20 秒 pause500（正式一秒後暫停）和 recover100（正式十秒後解除，餘十秒觀察），各一次 120 秒短期穩定性，無預覽／額外負載。六批含暖機計畫 338 秒；實測預算 900 秒，從開始到清理另限 1200 秒並保留 60 秒清理餘量。沒有新增正常批次或執行原配對補測一整輪，既有 48 Hz 每配置至少兩個完整正常窗口已足夠。

| 後端 | capture_complete | 後續共同成本 |
| --- | --- | --- |
| gRPC payload | 完整 payload 到達；protobuf 的內部 parse 無獨立 API 邊界 | RGB 複製／轉換、LatestFrame 發布、消費 |
| WGC | callback 取得最新可用 frame；先做相對時戳門控 | D3D11 staging readback、BGRA→RGB、pixels fingerprint、發布、消費 |
| DXGI | AcquireNextFrame 返回，裁切有效性擷取前後皆檢查 | D3D11 staging readback、BGRA→RGB、pixels fingerprint、發布、消費 |
| scrcpy | 完整編碼 packet 收到，另記 header parse | FFmpeg 軟體 decode、RGB conversion、發布、消費 |
| MMAP | 通知後 snapshot 複製完成 | 正規化、發布、消費；producer 同步未知 |

所有主機時間為 QPC nanoseconds。Android Unix metadata、codec PTS、WGC SystemRelativeTime 100 ns 與 DXGI QPC ticks 分別保留。主機駐留从上述 capture_complete 到 consume；不能當 Android render 到 host 的絕對端到端延遲。相對 lag 只看 host/source elapsed 差；source absolute age 仍 unknown。

來源 counter span 估計繪圖推進，distinct/秒表示實際消費的不同來源圖；跟隨比例以兩者相除，有消費取樣／遮擋／來源間隔限制。正常與壓力場景不能合併為單一平均排名。所有分布皆列 n、p50／p95／p99／max；超過分析固定樣本容量會明示 sample_truncated。

暫停後另讀獨立 ADB pixels reference，記呼叫前後 QPC bracket；恢復指標是第一張已消費 counter 達到／超過 reference 的圖，含診斷等待成本。static3 的兩個獨立 reference 應相同，恢復 reference 應更新。這是可重算的像素恢復證據，不是精確瞬時 age 校準。

CPU core-equivalent = process CPU elapsed／其 QPC bracket midpoint elapsed，1.0 為一個 core；PAS 與可識別 qemu/emulator/studio 程序分列，可再加總同一區間。RSS 為一秒取樣峰值、起／終值及分布，短尖峰可能漏取。PDH GPU Engine percent 各 engine 獨立歸一，原始 instance 包含 PID、adapter LUID、engine type；不同引擎的加總不能稱為整卡使用率。不同程序的 encoder 成本不可只看 PAS CPU。

PDH 使用 [語系無關 counter](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw) 與 [wildcard formatted array](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw)，每次先詢問容量，再在四 MiB／4096 instance 上限內讀取；失敗或缺值明示 missing。WGC [TryGetNextFrame](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.trygetnextframe) 回傳下一張、空 pool 為 null；本程式額外限制最多兩次取得與相對積壓檢查，API 本身不保證已排隊圖等於最新 Android 畫面。

## 已保留的失敗與修正

| 日程 | 實際退出狀態 | 本輪處置 |
| --- | --- | --- |
| r1／3f5671e | 前兩批 gRPC 完成；第一批 WGC 停止時崩潰，73 批未執行 | WER `GraphicsCapture.dll_unloaded`／c0000005；共享回呼 closing／drain，加上 System32 模組 pin；同程序 21/21 stop/start 回歸 |
| r2／6fa6dbde | 76 批全部執行，69 成功／7 失敗 | DXGI 兩批前景失效；scrcpy 四批正常及十分鐘因相對 PTS 積壓終止；全數保留 |
| r2 暫停稽核 | WGC 第一張相對額外 lag 500.7144 ms；gRPC control/fast 494.3589／493.7612 ms | WGC 最新 pool／相對 guard；gRPC 正式日程固定 guard；新增 Fixture 真值及資源採樣，第三版整組重测 |
| r3／4913edc | 完成 11/96 後主動終止，剩餘或中斷 85；scrcpy 一批 counter order 失敗 | 色彩 identity frame 655 為 484957，前後為 23903／23905；新增 binary cross-check 與合成對抗圖回歸，第四版整組重測 |

r1／r2／r3 不能與 r4 混成單一排名；修正與短測明細見 [就緒紀錄](CAPTURE_READINESS_20260926.md)。r3 的失敗圖完整 RGB 未保存，沒有改寫其 counter 或聲稱已從原圖重新解碼；新合成回歸是解析器契約證據。性能補測規則首輪前已固定：至多一輪、限環境／品質異常或來源率差超過 10%，保留原結果；版本修正則重跑全部受影響配置。

r4 `normal-40-r2-scrcpy` 的 frame 599 實際又出現 exact colour identity 484957／binary counter 23904；新版發布的解析 counter 為 23904，schema `native_v2_lossy_validated`、max colour error 9，整批 counter order 有效。原始事件明列兩個獨立讀值；這是新版保護的實機證據，並不補寫 r3 缺失的原圖。

## 正式結果與適用性

第四版部分資料已由同一凍結 C++ analyzer 重算：[partial-analysis.json](../measurements/five_formal_20260926_r4/partial-analysis.json)。37 個成功批次全部 `valid=true`，raw／PNG hash、APK、binary、環境、pixels／幾何門控一致；全日程 `campaign_complete=false`、`all_valid=false`、`formal_candidates_valid=false`。這些旗標保留原意，不能因縮短要求而改成原矩陣通過。

| 路徑 | 本輪結案狀態 | 初步選型與條件 |
| --- | --- | --- |
| gRPC payload fast | implemented-and-tested（已測範圍） | **初步主用**；SDK gRPC 認證可用、橫向 RGB888 payload、顯式 250 ms 相對 guard；仍有 owned RGB 複製 |
| gRPC legacy-rows | implemented-and-tested（控制配置） | 配對基線；不因名稱叫「優化」就宣稱 fast 必然較快，收益須依同版各批分布解讀 |
| WGC | implemented-and-tested（已測範圍） | **有條件備用**；明示 top-level HWND／crop、視窗存活且持續 render，可補 gRPC 認證／傳輸路徑問題；依賴同 AVD 與 Windows compositor；低頻來源跟隨仍有限制 |
| DXGI | implemented-and-tested（可見前景條件） | 原版兩次實際遮擋／前景失效拒絕；需完整位於同一未旋轉 output，無上層視窗相交。使用電腦時會撤銷有效性，適用条件較嚴格 |
| scrcpy v4.1 軟體 H.264 | failed-with-reproduction（固定配置） | 57 Hz 第二批因相對 PTS 積壓、至多一次重連後終止；成功批次保留。AVD encoder 成本與有損品質使本配置不列主／備 |
| Emulator MMAP | diagnostic-only | producer 同步未建立；不給 Session／主備資格，沒有為新版再補長測 |

### 正常場景（r4 原 60 秒完整窗口）

表中區間為不同批次的最小／最大值，p99 欄是各批 p99 的區間，沒有合併成 pooled p99。有效批数分母為原預定每組三批；失敗、取消與中斷另列。逐批 p50／p95／p99／max、樣本數、分段與資源見 [run-metrics.md](../measurements/shortened_followup_20260926_01/run-metrics.md) 及 [report-data.json](../measurements/shortened_followup_20260926_01/report-data.json)。

| 配置／目標 Hz | 有效／預定 | 消費樣本 n | 可見來源 Hz | 不同圖 Hz | 跟隨比例 % | 主機駐留 p99 ms | 最長無新圖 ms |
| --- | --- | ---: | --- | --- | --- | --- | ---: |
| gRPC control／40 | 3/3 | 7,180 | 39.91–39.99 | 39.81–39.91 | 99.55–99.94 | 2.28–2.48 | 82.60 |
| gRPC fast／40 | 3/3 | 7,166 | 39.56–40.00 | 39.47–39.99 | 99.76–99.99 | 2.20–4.24 | 88.75 |
| WGC／40 | 3/3 | 5,595 | 39.95–40.00 | 26.65–35.40 | 66.70–88.50 | 13.75–15.05 | 69.34 |
| DXGI／40 | 3/3 | 5,587 | 40.00–40.01 | 26.33–38.93 | 65.80–97.33 | 15.46–17.46 | 66.70 |
| scrcpy／40 | 3/3 | 6,852 | 39.04–39.77 | 37.12–38.68 | 95.09–98.18 | 3.48–4.68 | 123.86 |
| gRPC control／48 | 3/3 | 8,346 | 46.02–46.68 | 45.98–46.59 | 99.78–99.90 | 2.94–4.13 | 89.52 |
| gRPC fast／48 | 3/3 | 8,370 | 46.54–46.63 | 46.43–46.55 | 99.75–99.83 | 2.99–3.26 | 94.02 |
| WGC／48 | 3/3 | 8,172 | 47.25–47.58 | 44.79–46.10 | 94.80–96.90 | 16.54–19.67 | 69.57 |
| DXGI／48 | 2/3 | 5,584 | 47.71–47.73 | 46.32–46.74 | 97.08–97.93 | 17.21–19.13 | 51.06 |
| scrcpy／48 | 3/3 | 6,577 | 38.64–40.06 | 34.59–37.58 | 89.53–94.95 | 5.23–6.25 | 2,148.32 |
| gRPC control／57 | 2/3 | 5,827 | 45.46–51.89 | 45.24–51.77 | 99.54–99.76 | 4.05–5.29 | 96.20 |
| gRPC fast／57 | 2/3 | 6,351 | 52.90–53.11 | 52.77–53.02 | 99.75–99.83 | 2.67–3.21 | 77.80 |
| WGC／57 | 2/3 | 6,393 | 55.70–56.22 | 52.14–54.39 | 93.61–96.75 | 14.51 | 66.96 |
| DXGI／57 | 1/3 | 3,321 | 56.45 | 55.35 | 98.05 | 14.22 | 53.12 |
| scrcpy／57 | 1/3 | 2,152 | 38.81 | 35.85 | 92.36 | 6.00 | 135.14 |

40 Hz 目標的略低於 40 Hz 值和 scrcpy 48／57 的低來源率全部保留；正常研究範圍不是交付率通過線。57 Hz 控制配置第二批來源只有 45.46 Hz，不能把其較差尾端完全歸為複製策略。scrcpy 48 Hz 第一批重同步一次且出現 2.148 秒空窗；第二批 57 Hz 連一次重連後仍失敗。本軟體 encoder 配置不能只以成功批次的短主機駐留排名。

### 成本、GPU 與像素品質

48 Hz 原正常窗口的各批範圍如下。CPU 為 core-equivalent；RSS 為一秒採樣峰值的各批範圍，PAS 單程序串行重用含 module／allocator cache，不能解讀為後端純 allocation。

| 配置 | 批數 | PAS CPU | qemu CPU | PAS RSS 峰 MiB | qemu RSS 峰 MiB | RGB copy p50／p99 ms | readback／decode p99 ms |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| gRPC control | 3 | 0.478–0.512 | 1.505–1.566 | 63.94–64.64 | 3325.93–3328.17 | 0.871–0.893／2.349–2.708 | 不適用／不適用 |
| gRPC fast | 3 | 0.499–0.529 | 1.534–1.606 | 63.94–65.07 | 3325.99–3328.30 | 0.814–0.826／1.996–2.375 | 不適用／不適用 |
| WGC | 3 | 0.409–0.481 | 0.899–1.017 | 82.52–84.61 | 3319.37–3323.75 | 1.646–1.697／3.607–4.544 | 6.571–6.717／不適用 |
| DXGI | 2 | 0.621–0.725 | 0.788–0.980 | 77.72–77.88 | 3319.25–3321.48 | 1.662–1.666／3.431–4.104 | 9.768–9.858／不適用 |
| scrcpy | 3 | 0.106–0.124 | 3.145–3.271 | 61.88–64.17 | 3315.69–3318.71 | 1.112–1.141／2.976–3.964 | 不適用／1.826–2.378 |

fast 在 48 Hz 複製階段較低，CPU 和各頻率尾端沒有一致改善；資料支持減少逐列工作，尚不足以宣稱全面或顯著的端到端加速。scrcpy 的成本大部分在 qemu；低 PAS CPU 不代表低整體 CPU。gRPC 主用建議依完整像素、較高來源跟隨、短尾端和使用條件，並非只看優化名稱。

所有 37 批每秒 GPU 動態採樣可用／缺失為 60／0；原 PID PAS=20400、qemu=11368。下表為 adapter LUID `0x00000000_0x00010673`、physical 0、engine 0 的 **3D 引擎 p95 % 各批範圍**，各引擎独立解讀；其餘 Copy instance 的 n／完整分布保存在 report-data 和 raw，不加總為整卡使用率。PAS gRPC／scrcpy 的該 engine 無明顯活動，表格不以缺少 instance 代填零。

| 配置 | PAS 3D p95 % | qemu 3D p95 % |
| --- | --- | --- |
| gRPC control | 無 >0.01% p95 的樣本 instance | 4.46–4.68 |
| gRPC fast | 無 >0.01% p95 的樣本 instance | 4.60–4.64 |
| WGC | 4.43–5.87 | 4.06–4.73 |
| DXGI | 5.06–5.92 | 3.90–4.34 |
| scrcpy | 無 >0.01% p95 的樣本 instance | 7.33–8.12 |

37 批共有 **93,473/93,473** 消費圖可解碼，位置真值無缺值、方向／幾何及 counter order 有效；這是消費取樣，並不涵蓋所有 source callback。gRPC／WGC／DXGI 的最大 colour、左右移動邊緣與細線位移皆 0 px；細線寬 1 px、低對比 delta 8。scrcpy 15,581 張的最大 colour error 24、左邊緣誤差最大 1 px、右邊緣與細線位移 0 px，低對比 delta 7–8，細線仍 1 px；一次 colour／binary 不一致已被交叉核對處理。這個 Fixture 不能證明所有 Phigros 圖樣在 H.264 下都能識別。

### 縮短補測與短期穩定性

六批 **6/6 成功**，另以凍結 C++ analyzer 驗 raw hash、幾何、order、位置真值和 source_valid；12,102/12,102 消費圖可解碼。含暖機原定 338 秒；實測至清理 QPC wall 為 **606.27 秒**，低於 1200 秒上限，無 PAS 留在背景、Fixture freeze=0。結果見 [followup-results.json](../measurements/shortened_followup_20260926_01/followup-results.json)。

| 批次 | 正式秒 | 主機駐留 n | p50／p95／p99／max ms | 暫停後 stale drops | 指定來源 counter 匹配時間 ms |
| --- | ---: | ---: | --- | ---: | ---: |
| gRPC fast pause500 | 20.028 | 915 | 1.009／1.650／2.187／23.158 | 0 | 158.448（實測 resume 起） |
| WGC pause500 | 20.005 | 782 | 7.641／10.932／12.066／55.911 | 1 | 166.450（實測 resume 起） |
| gRPC fast recover100 | 約 20 | 554 | 1.421／16.956／32.552／65.503 | 0 | 166.846（排程 threshold 起） |
| WGC recover100 | 約 20 | 422 | 10.877／35.047／47.801／60.967 | 0 | 190.244（排程 threshold 起） |
| gRPC fast stability120 | 120.001 | 5,323 | 1.701／2.774／4.614／15.515 | 0 | 不適用 |
| WGC stability120 | 約 120 | 4,106 | 8.635／11.442／13.684／20.517 | 0 | 不適用 |

pause 第一張發布圖在 resume 後 14.356／16.695 ms（gRPC／WGC），相對額外 lag 為 -8.013／-0.075 ms；resume 後第一秒的最大額外 lag 為 21.124／3.425 ms。WGC 先丟棄一張相對舊圖。不能把負值當成負的絕對 render latency。原 receiver 暫停約 507 ms、包含預期暫停的無圖空窗約 523／533 ms。

來源 counter 匹配指標包含 ADB reference 呼叫等待。consumer 恢復沒有獨立 journal 實際解除時刻，原 summary `recovery_from_resume_ms=null` 保留；[另列推導](../measurements/shortened_followup_20260926_01/consumer-recovery-derived.json) 以 `measurement_start_ns + 10s` 和匹配圖的 `capture_complete_ns` 算差，沒有回填原 raw 或假稱精確消費者解除延遲。原 analyzer 的 first15s／after15s 固定分段也不適用此次十秒解除，不拿來當恢復前／後分布。

120 秒 gRPC 可見來源 44.60 Hz、不同圖 44.32 Hz、跟隨 99.36%、最大無新圖 94.87 ms；WGC 為 47.87／34.21 Hz、跟隨 **71.47%**、最大空窗 66.55 ms。WGC 記 1490 重複 pixels callback、consumer frame-sequence skips 1498，而 LatestFrame 全生命期 overwrite 為 8、physical pool drop=0；sequence skip 包含未發布的 duplicate，不能全部歸為消費者積壓。跟隨差異原因未定位，保留結果且不再重測挑數字。

PAS 的 120 秒 RSS 起／終／一秒採樣峰為 gRPC 37.46／45.24／45.58 MiB、WGC 60.51／66.70／66.82 MiB；CPU 為 PAS／qemu 0.603／1.996 與 0.430／0.981 core。各有 120 個 GPU 樣本、缺失 0。此長度只能支持短期可運行，不證明長期無 leak／jitter 或穩定交付率。第一個 gRPC 暫停短測的 PID 盤點晚於程序退出，因此 own GPU PID attribution 缺失；原始 GPU instance 採樣仍保留，沒有補造 PID。

### 建議門檻與選型邊界

**初步主用 gRPC fast；WGC 僅為有條件的備援候選，未宣稱性能合格備援。** WGC 像素／幾何與此次恢復正確，但 40 Hz 正常跟隨 66.70–88.50%、最新 120 秒跟隨 71.47%，不能套用 gRPC 的交付表現。需要容忍較低不同圖率，並在沒有新圖／profile 無效時撤銷觸控有效性；下一階段先量簡單目標閉環。

待使用者決定的基線預警建議：主用正常無額外負載時主機駐留 p99 >10 ms、單次空窗 >150 ms 或來源跟隨 <98% 就記異常；WGC 駐留 p99 >25 ms、空窗 >150 ms 或跟隨 <90% 記異常。依據是上述各批與 120 秒觀察，WGC 部分實測已觸發跟隨預警，這不作通過宣告。pixels 解碼／幾何／時域／ownership／過期圖撤銷仍為硬條件；250 ms relative guard 是積壓上限配置，不能等同遊戲延遲容許。正式 SLO、負載下容許和長期故障率仍未決定，所有 `performance_pass=null`。

### 故障、取消與覆蓋限制

r4 `normal-48-r3-dxgi` 在 worker 偵測到上層 Emulator 工具列相交 `[1802,192,1807,825]` 後拒絕；`normal-57-r2-dxgi` 因目標不再前景拒絕；`normal-57-r2-scrcpy` 因 PTS 相對積壓終止。三批均未完成正式窗口，保留錯誤及 partial raw，不納入完整正常分布。Windows 操作介入見 [environment-intervention-01.json](../measurements/five_formal_20260926_r4/environment-intervention-01.json)，其與 `normal-57-r1-scrcpy` 重疊；該成功批次保留並明示此條件，不能當完全無介入的成本樣本。

原 plan 96 批中，40 批有結果；第 41 批 `normal-57-r3-grpc-fast` 只有 partial raw 且末行截斷、沒有正式退出紀錄，其終止原因未建立；另 55 批未開始。依新修訂全部未完成項收尾取消，原 `campaign-results.json` 不改寫。原先預記的整輪 supplement 及 11 項 manual-fault plan 未執行，按最新縮短要求取消。

本版尚未正式覆蓋 CPU／memory／GPU 負載、preview 配對、static3、受控視窗遮擋、最小化、resize／DPI／方向變更、跨 monitor、driver device/access loss、實機 scrcpy server 中斷／AVD 關閉重連。先前開發／舊版本的相關實測和單元測試僅證明各自範圍。主機只有一個附加、未旋轉 output；negative-origin／cross-output helper 測試不等於真實多螢幕測試。沒有本版十分鐘或三十分鐘長期穩定性資料，短測 p99 不給可靠性或遊戲判定窗保證。

MMAP 已有版本對應的 proto hash 與明示 tearing 契約，沒有建立 producer fence／ownership 證據；大量正常 counter 不能補足此缺口。詳見 [MMAP 稽核](MMAP_FEASIBILITY_20260926.md)。旋轉／跨 monitor、未實際重現的 GPU device loss／DPI fault 會明列限制，不能用單元或 loopback 冒充真實 driver fault。

## 下一階段交接

本輪選型不授予遊戲自動觸控資格。M3 先沿選定 capture profile 與 Frame 時域／ownership，統一座標轉換，對無新圖、相對積壓、幾何變更與斷線撤銷有效性。只依 pixels 建立 observation／track／predicted collision，記辨識完成、預定觸控、注入開始／完成、再次觀察到效果各 QPC 時點；先用簡單目標完成閉環誤差分布，再進 Phigros 專用辨識。

原生 backend 的 `source_rotation=0` 描述已是橫向的 Windows raster，不能直接推定 Android 自然方向或觸控方向也為零。接入 native Frame 到觸控前，需明示 Android display／input profile 及 game-area→input 的座標映射，驗證四角與各 contact。WGC 的 window-border crop、DXGI 的 screen-origin mapping 都屬擷取座標，不可把 Windows screen coordinates 直接送到 Android。gRPC 的 bench 相對 lag 選項目前也未自動改動既有 Session 配置；M3 接線時應顯式攜帶已驗證的保護與 epoch/generation 撤銷契約。
