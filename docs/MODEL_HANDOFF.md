# 模型交接：2026-10-24 測試已封存

使用者要求先停止修正，整理所有紀錄並commit／push目前分支。不要自行恢復實測、選曲、觸控或重啟emulator。下次接續先讀[完整結果與回合表](SESSION_RECORDS_2026-10-24.md)及[機器索引](SESSION_RECORDS_2026-10-24.json)；本文件取代先前多段「最新」交接的狀態解讀。

## 本輪結果

- Chapter Legacy20首IN及62份原譜已入檔；額外暖身Verrückt不計入20首。原譜／音源／本機校正／APK／binary／PNG／UI與RPC原始證據均留在repo外，不提交Git。
- 最新build006有18個原生回合：14輪完整送完接點，8輪manual-mode確認結算，6輪完成輸入但未確認結算；18輪最後kernel均為零。
- 正式章節14首建立過原生回合（光為0接點停止），12首至少一次完整輸入；保存結算圖確認8首AP：Eradication Catastrophe、Credits、Dlyrotz、Winter ↑cube↓、FULi AUTO SHOOTER、[PRAW]、Cereris、Pixel Rebelz。另有暖身Verrückt AP。
- Glaciaxion為727 Perfect／2 Miss、943,759分；Cipher為1040 Perfect／1 Miss、980,980分。Engine x Start!!與Non-Melodic Ragez雖已完整輸入，缺本輪完整結果圖，成績未驗。
- 6個未確認結算回合：Engine兩輪、Cipher、Non-Melodic、FULi、Winter。FULi與Winter的AP由保存PNG補記於衍生複核表；原native failure／manual_result_confirmed=false保留。選曲頁歷史最佳分不能作為本輪成績。
- 6首尚未實跑：混乱-Confusion、HumaN、Class Memories、SURREALISM、Bonus Time、ENERGY SYNERGY MATRIX。光需重驗首音，Sultan Rage中途遲到停止；不得宣稱20首全測完成。

## 版本與本機來源

分支為`feature/chart-driven-multigame`，main維持抽取基準。確切封存commit以`git log -1`為準；本機各bundle的9472c44標記是建立時的父commit／未提交source snapshot，不改寫成後來commit。

- 最新程式：`chapter-legacy-adaptive-build-006/bin/chart_experiment.exe`，SHA256 `07ede2edf228fa9805305c4ea3603f2235075f477000165937b42bb2d242edb7`。
- 搭配：panel004／library004／state005／readiness007／sweep008。GUI SHA `5efe63c0632c1dbb0ca548bf30771d80c61364e5148f29cc6bb8cbbd3ce1e3f1`；learner SHA `8899f5f4a2afeb6d19949e9e6db16036e65797abacac926da96a4e7816383838`。
- 已封存的重啟入口為`local-assets/phigros-4.0.1/song-test-restart-001/restart.cmd`（repo的同層local-assets目錄）。只有使用者執行後才冷開機／確認五指fixture輸入／建立新boot config；既有已知桌面捷徑核對後才更新。
- 使用者本輪已完成`20261024-222415-179-56858ea7`重啟／實際五指驗證，新的derived config在該restart目錄。後續再重開emulator仍必須重驗，不能直接更改boot字串或把舊capacity proof移植到新boot。
- 本次總整理：`session-records-2026-10-24-001/`，索引原有280個本機目錄、1448份原件SHA、26個面板回合及6個較早控制回合。先前逐段交接原文另存`session-records-doc-history-001/`，避免把當時NOT RUN誤當為截至目前尚未實跑。

## 實作邊界

來源相容匯入採明示政策，嚴格預設保留；所有原始track宣告／json pointer／異常來源保留，不能另存「修正過」的原譜。完整contact計畫、投影速度界限與opening模型各自綁定身份，Prepared只移轉一次。詳見[共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)。

HUD主路徑是shared-IN v3；learned-title-redundancy-v1從兩份開場clean實際觀測建立本輪title／完整IN等級／pause參考。只在當下分數受到邊界交疊且仍保留部分字形、所有當前glyph與否決優先條件通過時續用實際receipt。不在armed後訓練、不把參考圖或planner tick當作新觀測。它是額外像素假設，既有六位數＋邊界線合成案例的接受變化已明列，不宣稱所有舊合成負例仍拒絕。

48個開場RGB與64MiB journal緩衝預配置於READY前；最多2個診斷immutable lease與pool5，保留capture-ready／publish／owner／snapshot／journal階段時戳。100ms anchor／500ms UI／20ms lateness、pause/result／foreground／epoch／幾何／實際容量／release-all與STOP不放寬。保留14個PAS檔案byte／Git blob身份不變。

## 未完成與接續順序

1. 結果擷取、保存、辨識與離開畫面仍有缺口；優先用Cipher／FULi／Winter新原件補齊，並避免Engine／Non-Melodic完成後只留下選曲頁。本輪依使用者要求只封存，沒有繼續改辨識。
2. build006仍有Verrückt／Sultan Rage的20ms遲到。預配置不是已證明消除OS／RPC／排程抖動；利用新階段日誌分析，不延長限制。
3. 全測剩餘6首，重驗光開場、Glaciaxion等Flick／Drag漏判及逐曲重現率。實際AP只屬指定回合／chart／binary／設定，不能轉移成其他版本或20首資格。
4. 音訊同步仍暫緩；獨立input lead／source pixel age、長期穩定性與跨重啟重現率尚未取得。Project Sekai importer／裝置整合仍不是已完成支援。

## 驗證與歷史基準

本輪來源已有Windows非音訊整合28/28 CTest、portable4/4、21/21原生離線預檢，bootstrap Python31／PowerShell16及凍結入口PlanOnly。完整Python145例及LZ4驗證詳見[驗證紀錄](VALIDATION.md)。本次封存整理不執行新的裝置／ADB／遊戲測試；表中實測由使用者已完成的原件複核。

Dlyrotz原始成功基準仍是9472c44之前的Build009／live011：584Perfect／零GoodBadMiss／1M、584contacts／5669events，原封存與`tools/run-dlyrotz-ap.ps1`保留。新build006的Dlyrotz AP另有不同binary／plan身份，兩者不可混同。舊Glaciaxion live005的722P／1G／3B／3M亦保留，不覆蓋成新727P／2M。

所有完整回合、failure、缺錄、影格hash與本機來源見[SESSION_RECORDS](SESSION_RECORDS_2026-10-24.md)；來源提取與成本歷史見[章節計畫](CHAPTER_LEGACY_TEST_PLAN.md)、[每曲實驗](ADAPTIVE_SONG_TESTS.md)、[面板](SONG_TEST_PANEL.md)。
