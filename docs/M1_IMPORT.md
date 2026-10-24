# M1：Phigros v3 時間與語義 importer

2026-10-23 已完成嚴格 v3 importer、離線檢視入口與時間核對。輸入基準為 [M0](M0_ASSETS.md) 取得的 Dlyrotz IN 原生 JSON。M1 不輸出觸控，也沒有把它轉成接點計畫。

## 介面與格式邊界

- `include/rhythm/chart.hpp`：純 C++ SemanticChart／ChartLine／SemanticNote 與來源欄位，不暴露 JSON dependency。
- `src/rhythm/phigros_import.cpp`：nlohmann JSON 僅為 private implementation dependency。只接受已核對的 `formatVersion=3`，未知欄位、note type、重複 key、非有限數、容量／深度超限明確拒絕。
- `apps/chart_inspect/main.cpp`：只讀離線 JSON，輸出摘要或 `--notes-jsonl`；可指定預期 chart SHA256，與實際 bytes 不符即失敗。
- `tools/verify_chart_times.py`：以 Python Decimal／Fraction 獨立逐 note 比對 C++ 匯出時間、來源 pointer、type／side／raw tick；輸出須為新檔。

SHA256 對原始 JSON bytes 計算；stable IDs 是 `SHA256#JSON pointer`，不是假設原生格式提供了 note ID。保留 line／note 原始順序、above／below、BPM、raw ticks、positionX、speed、floorPosition，以及 move／rotation／opacity／speed events。空譜面在語義層合法。

真實譜面含 `-999999`／`1e9` 事件端點、螢幕外座標、opacity >1、負速度等；importer 保留這些值，沒有以畫面範圍裁切。時間區間倒序拒絕，零長事件保留供 M2 明訂求值規則。

## 時間契約

```text
song_ns = round((raw_tick × 1.875 / line_BPM + chart_offset_seconds) × 1e9)
hold_end_ns = round(((raw_tick + raw_hold_ticks) × 1.875 / line_BPM + chart_offset_seconds) × 1e9)
```

Offset 只加一次，正值表示事件相對音樂延後；最後取整到 ns，中點遠離零。此格式解讀交叉參考 [Phichain 格式說明](https://ivan-1f-phichain.mintlify.app/formats/official)、固定版本的 [Phira tick parser](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/parse/pgr.rs#L220) 與 [music_time − offset](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/scene/game.rs#L979)。這些是社群 primary implementation，不是 Pigeon Games 官方判定規格。

Chart offset、捕捉時鐘／音訊相位與輸入提前量各自分離。上述 ns 數值正確不代表遊戲輸入有 ns 精度。

## 建置與使用

預設仍維持無第三方、離線 3 項 CTest；選用 importer 時才要求 nlohmann_json（本機驗證 3.12.0）：

```powershell
cmake -S . -B build-import -A x64 -DRHYTHM_BUILD_CHART_IMPORT=ON -DCMAKE_PREFIX_PATH='C:/path/to/nlohmann-json-install'
cmake --build build-import --config Release
ctest --test-dir build-import -C Release --output-on-failure
./build-import/Release/chart_inspect.exe '../local-assets/phigros-4.0.1/dlyrotz-in13-verified/Chart_IN.json' aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90
```

Linux／單組態 generator 移除 `-A x64`，改用 build 目錄下的執行檔；本輪 M1 實際執行的是 Windows／MSVC 19.50，不冒稱已跑 Linux。

## 驗證

- **16 項 synthetic checks 通過**：SHA padding vectors、跨 BPM 同時音、正負 offset、非整數 tick／BPM、Hold end、空譜面、來源 hash／ID、四種 note、sentinel／原始語義、無效輸入及資源限制。
- Optional importer build 全部 **4/4 CTest 通過**；default build **3/3 通過**。
- 真實 chart SHA：`aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90`。
- 真實輸入：**24 lines、584 notes、31,369 line events**；首音 `12000000000 ns`，最後 note end `121200000000 ns`。
- C++ 輸出的 **584 個來源 pointer、1,168 個 start/end 時間**逐項與 Python exact Fraction 比對；此 fixture 的最大時間差 **0 ns**。不是只比對第一／最後音。

本機完整報告為 `../local-assets/phigros-4.0.1/m1-verified-note-times.json`，逐 note 匯出為 `m1-verified-notes.jsonl`；這些真實譜面衍生內容不提交 Git。合成時間測例要求與獨立期望值差不超過 1 ns，實際裝置相位與延遲仍由 M4／M6 驗證。
