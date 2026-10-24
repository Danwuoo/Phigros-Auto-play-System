# 新技術方向：譜面驅動、同步閉迴路、多遊戲 adapter

研究日期：2026-10-23。下列區分為原碼觀察、設計決策與尚未驗證事項。研究範圍是本機／授權環境的輸入與時序工程；不包含反偵測、完整性繞過、排行或多人模式自動化。

## 1. 結論與既有瓶頸

不要把下一輪主要投入放在更大的逐幀辨識模型。首選是 **已知譜面或經校正的操作記憶 + 可觀測的歌曲時鐘 + 小範圍視覺校正**。這是架構建議，不是已證明的升分結果。

基準原碼實際已有可複用的 clock、frame buffer、ContactScheduler、TouchBackend、PixelCoordinateMap、GrpcCapture/GrpcTouch，無須全部重寫。[R1]

具體限制：

- `ContactScheduler` 預設 2 contacts、64 plans、16 steps、2 秒 admission horizon、150 ms evidence age、30 ms max lateness。這些是舊預設，不是兩遊戲的判定窗口或新架構最佳值。constructor 容許最多 10 contacts，仍不等於裝置已證明支援 10 點。[R1]
- `ContactPlan` 和 gate 都要求新鮮 evidence；把整首曲目一次 submit，或每個 planner tick 偽造 `evidence_ns=now`，都不是正確的記憶式整合。150 ms 不可直接擴成整首歌來規避檢查。[R1]
- 舊 `pas_core` 建置納入大量 game/research 模組，runtime 依賴 research provenance；舊實跑流程重用本機 binary/objects，不能將 source clone 等同原 binary 重建。[R2]
- `grpc_transport.cpp` 明確依賴自訂 Windows read patch，不能換 stock gRPC 後仍報相同能力。[R3]
- 已保留的 20 首資料只有曲名、等級、分數與簡註，沒有逐幀、audio 或 touch trace，不能從這些分數還原譜面記憶。[R2]

上述 score-only 限制只適用於那批 20 首。基準歷史另有 Dlyrotz 全輪 7,722 張原始 PNG、QPC／source-frame 索引、12 段使用者選片及日誌，還有較早的 ROI dataset。這些可作離線記憶候選、幾何與視覺時間錨點的起點；原始像素目前仍需找回並核對，不能把歷史文件當成已取得資產。詳見 [資產接續](ASSET_RECOVERY.md)；先前 extraction 並未盤點這條資料路徑。

## 2. 三種記憶來源，優先順序不同

| 來源 | 用途 | 邊界 |
| --- | --- | --- |
| 使用者合法提供的精確譜面 | 優先；建立 deterministic timing、note semantics 與幾何 | 必須對到 game、區服、曲目、難度、版本、音源及 chart hash；不能假設已有資產 |
| 授權錄影／成功操作記錄 + 離線標註 | 無原生譜面時的替代路徑；多次觀察、人工修正後固化記憶 | 多幀可以降低單幀歧義，但遮擋／漏鍵不能保證恢復；需人工確認低信心段落 |
| 即時視覺 | 確認 playing/paused/result、畫面映射、局部時間錨點、異常停止 | 不是每個 note 都必須即時重建的主控制器；失鎖不盲目續播 |

先用明確可驗證的一首譜面建立閉迴路。不要先做全曲庫下載，也不要把 YOLO/VLM、強化學習或 process-memory hook 當作必要起點。沒有原生譜面時，離線多幀標註比追求當幀完全辨識更值得先驗證。

使用者已確認舊本機原件刪除，因此本機接續改為重錄 Dlyrotz 影音基準。保留錄影 pre-roll、host QPC、round-start 與 song-time 的不同時間原點；原播放器 0 秒不是歌曲 0 秒。舊操作含失誤，不能直接重播為理想譜面。歷史理由仍有研究價值，但不可假稱已對回新影格；新錄製、來源時間索引與音訊品質檢查見 [RECORDING.md](RECORDING.md)。

## 3. 分層架構

```text
授權原生譜面 / 校正後錄製記憶
       ↓ importer + schema/version validation
語義 Chart IR（note、時間、判定區域、路徑、來源身份）
       ↓ Phigros / Project Sekai adapter + calibrated geometry
Resolved-contact memory / rolling contact planner
       ↓ song→host clock + input latency compensation
PAS ContactPlan bridge → bounded ContactScheduler → TouchBackend
       ↑                     ↑
音訊對齊 / 視覺錨點 → SyncLease / supervisor / emergency release
```

初次 extraction 只實作 **resolved-contact memory 與時間／租約契約的離線原型**。本機接續另有 opt-in AVD 盤點、gRPC 觀測短錄、scrcpy 影音錄製及離線解碼／PTS 索引工具；它們尚未連入新的即時 runtime。完整語義 Chart IR、譜面 importer、同步 estimator、game adapter 與 PAS bridge 仍未實作。game enum 與合成範例不能被稱為實際雙遊戲支援。

長期記憶保存 chart/audio/layout identity；即時層保存 song position、clock rate、uncertainty、epoch、geometry version、觀測時間、有效期限與暫停狀態。cache 必須在區服／譜面／音源／layout 不一致時失效。

## 4. 音訊同步先做簡單可量測的方法

現有系統以 Windows emulator 為主，第一個音源方案是 WASAPI loopback；可行時使用 process-loopback 限縮 emulator process tree，避免桌面其他音效污染。Microsoft 提供 endpoint loopback 與 process-loopback sample；後者有 Windows build 條件，須先核對本機版本。[R4][R5]

本機接續的離線基準優先使用 scrcpy 裝置端影音 PTS 與 FLAC 音軌，避免混入其他桌面音訊。gRPC 音訊也已測通，但其 timestamp 不具 sample-position 契約；WASAPI QPC／device-position loopback 可作後續即時同步的另一條來源。三者都仍須量測歌曲判定相位，不能由有錄音推論已完成同步。

同一段原始混音／同一 cover 的音訊，先做降採樣特徵與 normalized cross-correlation 的粗對齊，再在預測位置附近做短窗相關與小幅 phase/rate 更新。cross-correlation 可求 lag，但不是直接等於遊戲判定時間。[R6]

要求 peak-to-sidelobe/次高峰比、最低能量、時間連續性及至少數個相容錨點；重複段落、靜音、音效、不同 cover、不同裁切均可能假鎖。不得只取最高峰就宣告 locked。原始 PCM 在不同混音下不穩時，才評估 onset/chroma/CQT 與受限 online DTW；CQT/DTW 文獻是演算法參考，不是本案延遲保證。[R7]

時間契約：

```text
host_hit = host_anchor + rate * (song_time - song_anchor) + calibrated_phase
host_dispatch = host_hit - measured_input_lead
```

chart offset 只在 importer 正規化一次。audio buffer 的時間戳應來自 sample/device position 與 host counter 關聯，不是 callback 抵達時刻；IAudioClock::GetPosition 提供相關 position/QPC 資訊，QPC 輸出單位須依 API 文件轉換。[R8]

必須分開估計 audio-output delay、capture/decode age、audio-to-judgment phase、input delivery latency。RPC return 不代表遊戲已判定；同一首歌鎖上音訊也不證明輸入 offset 正確。首次校正需要畫面錨點或可觀測的輸入／判定測試。時鐘更新只修訂未執行 future suffix，不回放已完成事件。

停止與重新獲鎖：pause、seek、restart、orientation/layout 改變、曲目身份不符、時間跳變或不確定度超限，都應 disarm、release-all、清空待執行，取得新 epoch 後才允許重新 armed。視覺/音訊處理不能阻塞 scheduler owner thread。

## 5. 兩個遊戲共用核心，但不共用判定語義

### Phigros adapter

先鎖定一種已取得且有測試資料的 JSON dialect/version。保留每條線的時間基準、移動、旋轉、透明度／速度事件、above/below 與 tap/drag/hold/flick 類型。社群 chart tooling 顯示這些事件可由譜面表示；它們不是遊戲廠商對第三方工具的保證。[R9]

在判定線局部座標求目標，再轉為目前 input surface。需要分開判定時間與視覺 approach speed，不能再將固定 y-crossing 當作普遍成立。Hold 是否需隨線移動、Flick 的移動門檻與 release 規則，必須以目標版本測試，不能把模擬器 rendering 當成原遊戲判定器。

### 世界計畫 adapter

以合法提供的 SUS/USC 或其他已辨識來源作 adapter 輸入，保留 BPM/拍號變化、offset、lane span、critical、slide 節點／曲線／連接語義、flick 方向及已知版本的其他 note 類型。lane 是有效區間，不只是單一中心點；slide 與普通 hold 不能混為一種固定長按。

舊 `sonolus-pjsekai-engine` 有 SUS→USC 轉換參考，但已明示停止維護並指向 Next SEKAI；兩者都是社群引擎，不是官方 parser 或判定規格。Next SEKAI 的擴充類型也不能自動視為官方遊戲一一對應。[R10][R11]

對未識別的格式／note type 必須拒絕，不默默降級成 tap。多指譜先分析 peak overlap，再以 backend fixture 驗證 contact capacity、同時性與 pointer continuity；不能沿用預設 2 點就宣稱所有難度支援。

## 6. 如何接回保留的 PAS 核心

使用短 lookahead 的 rolling planner，而不是整首塞進舊 scheduler。先把 memory contact id 轉成依 admission 順序遞增的 `intent_id`；它不是可以任意排序的原生 note id。持續動作使用同一 intent 的 revision/prefix_offset，保持已執行前綴與 contact ownership。[R1]

`ContactPlan.evidence_ns` 應指向真實同步／幾何觀測，並由 supervisor 續租；`generation` 不應每次 phase 微調都增加，否則舊 set_context 會取消接點。epoch/geometry 改變才作適當撤銷。現有 16 steps、150 ms freshness 可能需要 prefix compaction 或有證據的界面重構，不可只放寬限制。

記憶原型的同時事件規則是 release→move→down；舊 scheduler 的實際 tie ordering / batch dispatch 必須另測，不能以離線排序推定已有原子多點注入。保留的 GrpcTouch 有 batch 入口，也不等於目前 ContactScheduler 已用它。[R1]

實機 Android 的第二 backend 可評估 scrcpy 的音訊、畫面與控制通道；官方文件要求 client/server 精確版本相符，protocol 沒有跨版本相容保證。這次不改舊 scrcpy 4.1 capture，也不把新版本 server 塞給舊程式。[R12]

若測量證實 host→device jitter 才是主瓶頸，再考慮裝置端小型 deadline queue；在此之前先測 current gRPC path，不預先引入另一套 daemon。不能把平均 RTT 當做 p99 deadline error。

## 7. 實作順序與完成門檻

| 階段 | 交付與驗證 | 本輪狀態 |
| --- | --- | --- |
| 0 核心分離 | 精選原碼 exact blobs、無 research 的離線 default build、來源清單 | 已完成；Windows optional build 未測 |
| 1 譜面正規化 | 每遊戲一個合法 fixture；時間、offset、座標、未知類型拒絕與 golden events | 待做；只有合成 resolved-memory |
| 2 同步 shadow | 實際 audio capture、start offset / drift / silence / pause / 錯音源測試；不輸入 | 觀測擷取與離線解碼已做；自動對齊與 shadow estimator 待做 |
| 3 接點編譯 | adapter、rolling planner、PAS bridge、tap/hold/slide/drag/flick、容量與停止測試 | 待做 |
| 4 有界實機驗證 | 使用者確認測試環境後，每遊戲先一首；保存允許的結果與 end-to-end timing | 未啟動 |

每層分開報：chart import correctness、sync phase error p50/p95/p99、dispatch lateness、input observed latency、lost/duplicate contacts、release failure、各 note 類型結果及總分。先用 measured judgment budget 訂安全裕量，不杜撰通用 Perfect 窗口或 AP 保證。原 cost NOT_READY / live UNQUALIFIED 與歷史負結果保持，不能繼承為新系統通過。

## 來源（查閱於 2026-10-23）

- R1: [基準 core.hpp](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/include/pas/core.hpp)、[core.cpp](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/src/core.cpp)、[emulator.hpp](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/include/pas/emulator.hpp)。
- R2: [基準 README](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/README.md)、[CMakeLists](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/CMakeLists.txt)。
- R3: [基準 gRPC transport](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/src/grpc_transport.cpp)。
- R4: [Microsoft Loopback Recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)。
- R5: [Microsoft Application Loopback sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/)。
- R6: [SciPy correlation_lags](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.correlation_lags.html)。
- R7: [Lee, Musical Score Following and Audio Alignment, 2022](https://arxiv.org/abs/2205.03247)（本輪查閱摘要）。
- R8: [Microsoft IAudioClock::GetPosition](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition)。
- R9: [Phichain format documentation](https://ivan-1f-phichain.mintlify.app/formats/official)（社群文件；精確時間單位及座標慣例必須再與實際 parser/fixture 核對；文件所列 Rust 檔案路徑本輪未能取得）。
- R10: [舊 Sonolus PJSekai engine](https://github.com/NonSpicyBurrito/sonolus-pjsekai-engine)。
- R11: [Next SEKAI README](https://github.com/Next-SEKAI/sonolus-next-sekai-engine)（透過 repository default branch 讀取 README）。
- R12: [scrcpy developer documentation](https://github.com/Genymobile/scrcpy/blob/master/doc/develop.md)。
