# 判定線 M1：D1–D4 實作與離線驗證

2026-09-28。此文件記錄現行開發 checkout 的正式 C++20 修正；兩輪手動實戰與 M0 probe 的原始結果保留在原資料根，沒有以新 binary 覆寫。使用者提供的[判定線形式](判定線形式.md)保持原文。此輪沒有啟動模擬器、觸控、模型訓練或兩秒採樣 ring。

## 實作邊界

| 項目 | 現行程式 | 保守界線 |
|---|---|---|
| D1 線身分 | `GameLineTracker` 對至多16條當前線／16條track做有界全域最小成本配對；排除已選邊再求一次完整可行解，差距≤3列歧義。未分派且和已占用前身差距≤3者不即刻出生；可分離近鄰連續三幀才建立新lineage，出生幀仍invalid。 | exact重合不可分辨；歧義可見線不授新Down；候選暫存≤16，context／100ms gap清除。滿容量的未分派線輸出ID 0／invalid，不製造未保存的新ID。 |
| D2 Note→line | 多線用局部法向距離、沿線支持、相對接近趨勢（近期note位置對該線局部幾何）、confidence、弱外觀角度及近期線ID評分；相近分數保持unknown。外觀切向、實際運動與線法向不混成一個硬門檻；Hold body續接不得無支持換線。 | 趨勢僅作有界軟分數，歷史／插補不變成當前證據。有限候選只建立關係；需另有量測段與時序、當前pixels及owner gate才可Down。距離更近但關係錯的干擾線由近期身分排除；首次遠處誤配仍需人工關係真值檢驗。 |
| D3 旋轉Hold | 三個當前灰body／rails函式不再用舊anchor與線的`.95`角度硬拒絕，仍逐像素檢查雙側rails、interior、front／tail。observer 多線續接要求原線ID；owner只在當前rails／body支持且形狀連續時同指Move，同線旋轉容許切向變化。tail必須當前有效同ID線與兩份相隔≥10ms的可見過線證據。 | head接入仍由預測與當前支持決定；body不能建立新Down；Move位置來自當前body，沒有盲目旋轉舊觸點；掉幀後仍受原60／100ms期限，沒有加長。tail像素不完整時按既有失效釋放。 |
| D4 相對運動 | 使用每時刻note到每時刻線的有號法向距離，最多6點／90ms。反轉或跳變斷開舊擬合；新段仍需≥3點／30ms才輸出單一短期root。近線首見Tap／Hold／Flick明示`near_line_appearance_unqualified`；owner遇`motion_discontinuity`撤銷未執行Down。 | 斜向與線追Note可由相對距離表示；多root、幀間完整掠過／返回及遊戲判定窗尚無足夠觀測，不製造確定root。已執行或未知Down仍不重播。 |

### D1 策略選擇的證據

原[M0紀錄](LINE_IDENTITY_M0_20260928.md)使用改動前production tracker：單線→一幀兩候選→30幀單線，33個輸出出現33個不同ID；近鄰歧義仍出生是充分失效機制。新增獨立[`line_assignment_compare.cpp`](../research/line_assignment_compare.cpp)在**靜態水平線**範圍忠實重現舊cost／greedy配對，再只改「contested不出生」作B組；C組直接編譯現行production `game_motion.cpp`。[`assignment-ablation.json`](../measurements/line-policy-m1-20260928/assignment-ablation.json)保存輸入類型、source hash及以下結果：

| 固定序列 | A 舊production／M0 | B 最小出生修正 | C 全域可行配對＋有界出生 |
|---|---|---|---|
| 單線→一幀雙候選→30幀單線 | 33個不同ID，valid 1／invalid 32 | 1個ID，valid 31／unknown 2 | 1個ID，valid 31／unknown 2 |
| 起初兩條相距2px、共32幀 | 64個不同ID，valid 2／invalid 62 | 12個ID，valid 12／unknown 52 | 2個ID，valid 64／unknown 0 |
| 先單線，後4幀新增2px近線 | 未另測A | 1個ID，valid 1／unknown 8 | 2個ID，valid 3／unknown 6；第三幀才出生且當幀invalid |

B雖打斷自我重生，卻使真雙線反覆到期重生，近線新生也持續unknown，故採C。C在真雙線的最佳成本0、交換成本4，能維持兩個ID；一條既有線旁偶發2px候選仍為歧義，返回單線時不再循環出生；近鄰若連續三幀有可分離幾何則允許出生。這比「全部近線合併」或永久拒絕有可測的新線召回。B只涵蓋此靜態horizontal成本特例，未冒稱完整舊production重建或實戰。C對持續三幀的重複偵測仍可能誤生，需下節實際pixels標註；交叉重合時身分可暫時unknown，不以較低IDSW單獨宣稱正確。

## 驗證與可重現輸出

- Release：`cmake --build out/release-v145 --config Release --parallel 4`、`ctest --test-dir out/release-v145 -C Release --output-on-failure`；**221／221通過**，見 [`release-ctest-final2.txt`](../measurements/line-policy-m1-20260928/release-ctest-final2.txt)。原有手動策略golden仍通過，沒有改寫golden。新增案例涵蓋一幀重複、真雙近線、遲出生新線、順序交換、16線容量、89／90／100ms邊界、遠處角度不合與干擾線、相對接近趨勢、關係歧義、近線突現、跳變／往返、旋轉rails、同指Move、無body支持不得Move、同時五Hold、旋轉tail及fake-clock取消未執行Down。原「跳變必報`nonlinear_or_mismatch`」測試改驗`motion_discontinuity`且歷史段清空；五Hold tail fixture補上當前線ID與觀測時間，未遮掉實際失敗。
- Debug：最終來源 `ctest --test-dir out/debug-v145 -C Debug --output-on-failure`，**221／221通過**，見[`debug-ctest-final2.txt`](../measurements/line-policy-m1-20260928/debug-ctest-final2.txt)。ASan Debug：相關的GameMotion／GameTracking／GameOwner **80／80通過**，見[`asan-relevant-ctest-final.txt`](../measurements/line-policy-m1-20260928/asan-relevant-ctest-final.txt)；ASan未對其他專案測試宣稱完整通過，第三方DLL也非全面插樁。
- 合成容量／耗時：Windows 10.0.26200、Core Ultra 5 125H、MSVC 19.51.36256、Release `/O2`、`steady_clock`／Windows QPC；固定輸入順序交替，每場景暖身50、計1000次，沒有Capture／Touch。由production `game_motion.cpp`編譯的[`line_policy_bench.cpp`](../research/line_policy_bench.cpp)輸出[`assignment-timing.json`](../measurements/line-policy-m1-20260928/assignment-timing.json)：1線p50／p95／p99／max為1.5／1.5／1.6／24.7µs；2條2px近線為3.9／13.5／18.5／376.8µs；16線為101.7／203.4／458.2／2807µs。三組失敗0、invalid觀測0；p99−p50執行分散分別0.1／14.6／356.5µs。這是演算法單段，不是capture→touch尾端或排程jitter；實際密集畫面成本仍待量測。
- 實際pixels：保存的Pixel Rebelz round15／clip6、source frames 238007–238009，三個RGB檔與index SHA-256逐一相符。使用[`pixel_clip_probe.cpp`](../research/pixel_clip_probe.cpp)從空observer狀態重播，輸出[`pixel-rebelz-clip6-replay-final2.json`](../measurements/line-policy-m1-20260928/pixel-rebelz-clip6-replay-final2.json)：每幀辨1條同ID斜線及4個Hold候選；原事件當時發布1條線與4個target。重播首兩幀未通過三幀HUD gate，第三幀才有短期預測。這**不能**還原片段前的private tracks、原當場7603→7605錯誤ID序列或遊戲判定結果，僅核對這三張像素的當前觀測。

重跑獨立研究工具：

```powershell
cmake -S research -B out/line-policy-research -G 'Visual Studio 18 2026' -A x64 -T v145 `
  '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275'
cmake --build out/line-policy-research --config Release --target pas_line_policy_bench pas_line_assignment_compare pas_pixel_clip_probe --parallel 4
# 工具拒絕覆寫既有輸出；使用新的目的檔名。
out/line-policy-research/Release/pas_line_policy_bench.exe measurements/line-policy-m1-20260928/assignment-timing-rerun.json
out/line-policy-research/Release/pas_line_assignment_compare.exe measurements/line-policy-m1-20260928/assignment-ablation-rerun.json
out/line-policy-research/Release/pas_pixel_clip_probe.exe `
  measurements/game-assist/manual-session-108176133899800/pixel-clips/index.jsonl 15 6 `
  measurements/line-policy-m1-20260928/pixel-rebelz-clip6-rerun.json
```

## 仍未知與下一批人工標註

1. Pixel Rebelz round15／clip6前至少90ms的當前線及重複候選：每幀標出線實例、可見長度／切向、真正雙線或單線的`unknown`、跨幀同ID／遮擋／新生。現有三幀只能見故障後狀態，不能補出最初private assignment。
2. 遠處未對齊→近處才對齊的旋轉線：逐幀標note head／外觀切向、線切向、運動位移、note→line `confirmed/proposed/unknown`；同場加入外觀方向更像的干擾線。需要確認首次關聯錯配率與關係切換，而非把AI提議作人工真值。
3. 已按住且持續旋轉的Hold：標當前雙rails、body interior、front、tail、線ID、可支持的contact區及遮擋；分開記Down接入、同指Move、可見tail兩幀與真實Up／判定回饋。含兩個並行Hold與掉幀；未取得遊戲回饋時contact有效性標`unknown`。
4. 快速掠過／返回與近線突現：記每個實際可見frame、QPC間隔、note／線局部`d,s`、前後是否可能有未見root、已執行／未知Down及遊戲結果。幀間不可辨者保持`unknown`，不用舊動作當專家標註。

所有新增資料在`measurements/line-policy-m1-20260928/`，14檔共342,811 bytes（檔案Length合計，含中途與最終回歸紀錄）。`git diff --check`退出碼0，見[`diff-check.txt`](../measurements/line-policy-m1-20260928/diff-check.txt)；換行格式警告不代表diff錯誤。凍結的第一輪／第二輪實戰binary SHA-256仍分別為`483cb838…713c2534b76`與`de220228…e08dc601`；沒有覆寫M0 `synthetic.json`、原兩輪raw或凍結binary。沒有跨曲人工真值、Phigros旋轉Hold接觸區／tail判定驗收或新實戰版本成績；快速角度跳變／掉幀後同一線ID的能力也需實際關係真值核對。M1的兩秒ring負載實驗仍另列後續。
