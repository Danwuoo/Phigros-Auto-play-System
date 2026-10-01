# 單輪全錄與人工篩選契約

當前狀態：全錄、12 段篩選及使用者理由均已保存；`REASONS.md` 原文不改。理由已轉成 revision 3 研究索引，clip-09 已更正為無問題對照，pixel human gold 仍為 0。逐幀結果及後續 C36h 實戰見[分析報告](DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md)，整合後方向見[現況研究](PROJECT_STATUS_NEXT_STEPS_20261001.md)。下文「待填／尚未」描述匯出或啟動前的歷史狀態，不是目前未完成清單。

2026-10-01首輪完成：session manual-session-22885039263800，正常自動STOPPED、
PAS process已不存在。received pixels 7722/7722保存，約131.8293931秒、
PNG+index2385518593bytes；無writer fault，全部PNG／index SHA與ordinal／時間
已由C++離線匯出工具核對。MP47722張、89960755bytes，時間mapping與provenance完成。
capture最長間隔443.5487ms，沒有重建未收到來源幀。owner unknown receipts0，
release failed／unknown均空；這些不證明遊戲效果。結算pixels讀為Dlyrotz IN13
778185、485/16/6/77、maxcombo86、accuracy84.83%，不是廣泛改善驗收。
原12輪、C36g第一次驗收及凍結binary均保留，未開第二輪。

篩選入口為campaign/full-recording36g-01/review；`selection.json`引用來源與影片SHA，
整批理由與每片段時間／現象／選取理由／annotation request／hypothesis分開。
初次整理時clips空；使用者後續指定12段，已封裝原圖／日誌／小影片，當時原因待填。
逐Note Miss定位0、human gold0；proposed逐幀標註尚未開始。

## 2026-10-01 使用者選取12段

依全輪影片秒數：31–32、36–46、47–56、58–60、63–64、67–69、78–79、
85、90、93–95、99–101、102–113。85與90按時間點處理，指定點標記最近
實際原圖；每段前後各1秒並包含實際邊界支持影格，不抽樣或補造影格。
102–113只保留使用者原話「這裡問題很大」，沒有推定其原因或遊戲判定。

輸出 `review/selected-clips-v1`，12段MP4／mapping、3843個影格引用、3722張
不同PNG、專用副本1229237869bytes。各clip保留177、702、649、242、167、
241、180、123、121、238、239、764張連續原圖；同時匯出相關原始journal。
每段README入口及REASONS.md供使用者補原因，selection-r1/r2與本批snapshot保留。
初次匯出時原因、觀察、推測、annotation request 全留空，annotation_status=not_started；該 snapshot 保留，後填理由另見 revision 3。

新增離線C++20 `pas_recording_check select <session> <selection.json> <new-output>`。
先核對影片／來源索引／journal SHA、幾何、ordinal／時序與全部所選PNG。
原圖复制為本批唯讀專用副本，再以hard links供各clip共用；不連至全輪原圖，
不改原圖屬性／內容。index保留原始source_frame／recording_ordinal／QPC與SHA，
另列clip-local ordinal、全影片時間、clip時間及requested/context角色。
日誌按精確source_frame或文件明列的top-level host-QPC時間窗過濾，原row不改。
片段起始contact狀態unknown；長Hold的暖機仍可引用完整原錄影，不能靠剪片
自動恢復既有Down狀態。timing-gaps只記交付缺口，不當作Miss標籤。

上限32clips、每段40秒core與各最多5秒context、來源索引64MiB／36000rows、
每段4000frames、總12000張unique PNG／18000references、專用PNG 3GiB；
每個PNG4MiB、來源32journal segments×32MiB、row2MiB／總1M rows、每片段
100k rows／64MiB。native Media Foundation離線順序匯出12段，仍為單程序，
不建構emulator／touch owner，不回饋live決策。

實際匯出exit0；12段PNG／index／journal／MP4／mapping SHA與數量核對通過。
來源SHA錯、路徑逃逸id、既有output覆寫3個負例exit1拒絕，未新增output，
原package SHA不變。frozen C36g-rec1 PAS／舊recording工具保留，新selection工具
另凍結source／binary／DLL於selection-tools。沒有重跑策略回歸或新實戰。
campaign容量以hard-link每個入口都計長度的保守logical bytes核對仍低於8GiB；
帳本原12輪與獨立全錄1輪均不改。初次匯出時人工選片12、人工原因0、human gold0；後續已收到12段理由，並不等於像素gold。

使用者於 2026-09-30 要求「全錄，然後我來做篩選，保留連續影格給程序標註」
並另授權開 emulator／程式跑一輪。採用隔離 baseline36-recovery 工作樹；
原 12 輪已結束，原 C36g 首次驗收與原始資料全部保留。此授權不恢復 AP goal、
無限實戰或訓練。錄影版本為 C36g-rec1，observer36／planner18 策略不變。

## 即時資料流

`manual-session --full-recording --no-preview` 只由使用者選曲及按 Play。
同一 gRPC payload fast／RGB888 top-down／256KiB 擷取串流 publish latest 後，
另外把每張收到的有效 RGB 複製到診斷專用 slot。observer／owner 仍只讀 latest，
不讀錄影歷史。diagnostics 的故障可停止 session 並釋放觸控，不修改動作策略。
確認本輪結算或中止、owner 釋放及 archive 結束後，自動停止整個 session。

待命只保留最新 32 張事前原圖；新 round STARTING 時全部入列，之後不抽樣。
最多64張未提交 copies、3個 encoder，加事前 ring 共99個預配置RGB slots，
1280×720 下為273715200 bytes。三個 encoder 平行處理，每個只持有一張 PNG，
依 ordinal 串行提交磁碟，原順序不變；SUB filter／既有 zlib level1 Z_RLE
產出非交錯 RGB8 PNG，單張編碼上限 4MiB。encoder 的 filtered／compressed
buffers 與 output vector 均受 1280×720／4MiB 上限約束，不持有 latest pool lease。
copy／encode／ordered commit wait／disk+hash+index／capture dt 統計各最多36000 samples。

每輪 PNG+index 上限 5GiB、36000 張、600 秒；任一越限或 queue 溢出皆明列
incomplete_fault 並停止，不悄悄降採樣。round watchdog 另為 360 秒，僅失效
保護，並非結算真值。待命沒有 timeout，事前 ring 不累積到磁碟。

`full-recording/frames/frame-NNNNNN.png` 是全解析度、未畫框的無損原圖。
`index.jsonl` 保留 ordinal、source_frame、capture_complete／pixels_ready QPC、
source sequence／timestamp/domain、capture/source gap、dt、pre_roll、PNG SHA，
以及 copy／encode 起訖。每 60 張 flush 並輸出 bounded progress。
`summary.json` 僅在正常 close、已觀察 round end、accepted=written、沒有 fault 時
標為 continuous_received_pixels_complete。來源絕對年齡仍 unknown；未收到的
遊戲來源幀無法重建，連續收到不等於遊戲來源完全無漏幀。

## 離線覆核與篩選

`pas_recording_check bench <舊RGB根目錄> <新輸出目錄>` 用既有 27 張 RGB 反覆
10 次，以 60Hz 交付 270 張；逐張用獨立 WIC decode 比較所有 RGB bytes。
量出 n、p50/p95/p99/max、queue peak、失敗及 dt；它不涵蓋 capture+owner 實戰效能。
首次 FFmpeg PNG 嘗試因既有套件沒有 encoder 而測試失敗，已改成原生 PNG
chunks 加既有 zlib，不加新下載依賴。單 encoder 的60Hz測試另確認 queue overflow，
完整保存失敗資料；改用三個有界 encoder 及順序提交，再驗證同一270張。
格式依據：[W3C PNG](https://www.w3.org/TR/png-3/)。

`pas_recording_check video <錄影根目錄> <新MP4>` 先核對全部 PNG/index SHA，
使用 Windows Media Foundation H264/NV12 匯出 6Mbps、1280×720 的覆核影片，
QPC 差值換為媒體時間，不合成中間圖。影片是有損 preview，標註依據仍為 PNG。
影片旁的 `.jsonl` 對應影片時間到原始 ordinal／source_frame，`.json` 記錄 provenance。
MP4 512MiB額度另留，journal/archive 512MiB，連同已存 campaign 資料及 runtime／
bench 必須低於原 8GiB 總額；每次啟動前重算容量帳本。

使用者篩選時間窗後保留連續原圖與當時 journal，程序提出線角色、body／rail／
tail、clip-local 身分／Note→line／可接觸區等標註。未知遮擋或無遊戲判定證據
保持 unknown；proposed 不升人工 gold，取消觸控與幾何過線不當作 Miss／完成真值。

## 驗證狀態

錄影單元回歸、270 張實際 RGB 壓力／還原與 MP4 匯出須全部通過後才啟動。
原 C36g 策略檔 SHA 與凍結 baseline 核對；新 binary/source/DLL/profile/環境
另建 freeze。live 完成後再核對 completeness、gap、原圖、結果與 owner 收據。
此為啟動前 gate；其後全錄已完成，結果見頁首，沒有主要問題改善驗收。

2026-10-01啟動前驗證：Release231 passed／2 opt-in skipped；三encoder正式
bench270/270逐RGB bytes一致、exit0、queue peak4、PNG+index80166178bytes。
先前首次平行版曾在成功匯出後因離線工具增量編譯未更新class layout而退出
0xc0000409；強制重新編譯全部SessionRecording消費者後，最終bench與完整回歸
正常退出，失敗產物保留，不作驗收成功。MP4270張及時間mapping匯出exit0。

環境Windows11 Pro10.0.26200／Core Ultra5 125H／MSVC v145 Release，emulator
同時停在選曲；此bench沒有capture或owner，不是live排程性能。

|阶段|n|p50 ms|p95 ms|p99 ms|max ms|
|---|---:|---:|---:|---:|---:|
|copy|270|0.3052|0.472485|0.644003|1.1357|
|PNG encode|270|16.22705|24.798005|26.952977|32.9351|
|disk+hash+index|270|2.36095|4.379045|6.945133|15.6407|
|交付間隔|269|15.6525|30.6508|31.226196|31.775|

60Hz為producer目標；sleep喚醒交付有jitter，間隔p95-p5=26.52188ms，完整分布
保留benchmark.json，不把此合成交付間隔冒稱實戰來源年齡，也不相加各階段p99。
