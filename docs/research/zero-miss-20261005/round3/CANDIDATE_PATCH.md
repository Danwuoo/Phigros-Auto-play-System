# BVI cold v2：最小 RGB 側軌／端部修補

2026-10-05。研究基準 `acb27fdb095b82253ef9388eab52948a59663d02`。新候選只在 `research/bvi_cold_v2/bvi.cpp`、`bvi.hpp`，明示 derived from `research/x10d_o_bvi_build`。原 source、oracle、STOP、owner、scheduler、正式程式均未修改。**這是尚未整合正式鏈的 synthetic cold candidate；不是遊戲 Miss 改善或 live-ready。**

**最終凍結為 P1.1＋P2**，bvi.cpp SHA256 `232bb808e0e21b3b25e5e85995f8d0333597f446b8f867993535b84b40543e81`。下列 P1 各節保留第一次修補與反例沿革，不代表最終規則；最終差異見本文末節。P1 的 endpoint 幾何 mask veto 已撤回，真 cap union 的 merge 豁免亦已移除。

## P1 凍結及因果範圍

- donor bvi.cpp SHA256：`f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce`
- donor bvi.hpp SHA256：`6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f`
- P1 bvi.cpp SHA256：`550efdff2a96602d634cad7b4f5617ff4584c380569bb5d6aa1bc0ac665a9c46`
- P1 bvi.hpp SHA256：`1426a4b245988c8186f665444f0ddbc9f5f5f93c37d20e513fc77ad5c8bb043a`

[P1 core diff](evidence/candidate-dev/p1-core.diff)、[header diff](evidence/candidate-dev/p1-header.diff)。Header 只加來源註解，API／struct／metadata 不變。沒有另增 helper 檔；唯一 helper `pixel_center` 是 cpp 私有函式，對齊原 `llround` 讀像素規則。

### 1. 側軌白色不能由判定線代供

每側須在連續 depth offsets 中看到至少 **2 個不同實際 pixel centers**，各自滿足：

1. 當前 RGB white predicate。
2. 位於所有當前 measured-line masks 外，沿用法向 2.8px／線長範圍。
3. 同一 depth 的該側 interior probe 實際為 blue。

任一不合格 depth 重置短片段；連續 offsets 若 rounded 到同一 pixel，不增加 count。每側獨立見證，沿原 ROI 掃描維持有界。雙側任一缺失時不能供 Hold 的端點／Move；填滿但無 rails 的 union 仍可保留 body 事實。

數值 2 是測試前與契約分支明定的 synthetic proposal，用來排除孤立 white dot；不是 Phigros 真圖校準。正例包括剛好 2 點、長 rails、12px 短 body 及部分線遮擋。負例包括 1 點、兩點之間有 gap、只有 measured-line 交點、沒有對應藍 interior。

### 2. 端部在內外各 1..4px 找可見轉換

front／rear 各在內侧及外側 1、2、3、4px 探查，每側要有至少一組**同深度**雙邊 color witnesses：內側雙 blue、外側雙 black。見證均須在實際 pixel center 上落於全部 measured-line masks 外，並保留雙 rail 必要條件。

白線只會排除 witness，不能算 blue 或 black；不能用 renderer cap/rails flags、hidden physical labels 或過去像素補答案。內外側都搜尋是必要的：front=line+4 時固定內側 -4 踩線；front=line−4 時固定外側 +4 也踩線。完整內帶遮擋、單側 blue、沒有外部 black 仍拒絕 endpoint。

這只恢復當前端點量測。3 個 independent endpoints／至少 30ms、signature/RGB 雙去重及既有 Down 條件沒有放寬。

### 3. P1 不改 relation／constraint

P1 保留原 `contained>=2 && !front_end` 條件。V07 原無 rails union 先在 RGB 修正，不因單一 false cap 遮掉既有 merge ambiguity。`relate`、`constrain` 的實作與 donor 相同。

但真 rails union 有真 cap 時，原 cap 豁免仍可能把兩個既有分離 extents 當單一延續。本輪作者 probe 已記錄 `front_end=true / ambiguous=false / Move=true`，**未當 pass 或抹去**；獨立分支須先凍結反例、合法延續／分離／duplicate 控制，再由總控决定是否另立 P2。不能直接以拒絕更多 Move 當改善。

## 保留界限與成本

- 一張 borrowed current RGB；沒有 past RGB、新 hidden state、歌名／譜面／動作歷史輸入。
- `Candidate::metadata_bytes()` 仍 **185,904 bytes**；6 samples／90ms、40ms adjacent gap、100ms source、gate／plan deadline、60ms missing、128 ROI／16 lines／1MiB metadata／6,291,456 probe cap 均不變。
- 每個 Hold 的端點讀取從 8 到固定 32 pixel probes，新增 24；rail 排除只用既有讀取，不新增 RGB reads。最壞新增 128×24=3,072 probes。沒有把幾何 mask 計算包裝成已量過的 runtime 成本。
- `known_down` 仍用同 contact；unknown Down、completed Up、到期、歧義與 invalid 的動作邊界不變。沒有 owner／scheduler／backend。
- 旋轉 contact flank（原 V10 七失敗）、alpha effect predicate（原 V04 兩失敗）及 stationary descriptor dedup 另列未決，P1 不修改。
- 額外端點見證提升不是 physical identity 證明，也沒有 full note discovery／遊戲 feedback。

## 作者自驗，非獨立驗收

[probe source](evidence/candidate-dev/candidate_probe.cpp) 的相同 88 個 assertions：

| binary | failed | native exit | 最大 probes |
|---|---:|---:|---:|
| 原 donor | 21 | 1 | 410,629 |
| P1 | 0 | 0 | 410,653 |

完整 [donor 結果](evidence/candidate-dev/donor-results.json)、[P1 結果](evidence/candidate-dev/candidate-results.json)。另有 1 個不計入 88 assertions 的真 cap union decision row，兩版仍允許 Move，明列 unresolved-relation-policy。

正例覆蓋：front=line−4／line／line+4 的 cap 和 Move；兩點最小 rail；12px 短 body；3 個不同端點跨 40ms 的合法新 Down；重複 RGB／同 descriptor 改背景仍可保持當前 Move且不增加獨立次數。反例覆蓋：沒有 rail、孤立點、有 gap 的兩點、整帶 opaque effect、單側藍、外部沒有黑；unknown／completed 不重發 Down，到期釋放，原無 rail union 不 Move。這同時計合法機會与拒絕，不能只報 Move 降低。

自驗命令（repo root；可輸出至全新檔名重跑）：

```sh
g++ -std=c++20 -O2 -I research/bvi_cold_v2 \
  -I /tmp/phigros-bvi-round2-deps \
  docs/research/zero-miss-20261005/round3/evidence/candidate-dev/candidate_probe.cpp \
  research/bvi_cold_v2/bvi.cpp -o /tmp/bvi-cold-v2-author-probe
/tmp/bvi-cold-v2-author-probe
```

完整原 356 layer-cases／3,938 assertions、新凍結契約 cases、獨立挑戰與 sanitizer 由各責任分支另報；本作者 probe 不取代那些分母。未執行 Windows、遊戲、裝置、模型、付費、push／PR／commit。

## 最終 P1.1＋P2：獨立反例後的兩個可重現步驟

總控在獨立 P05／N09 及合法單一延續／仍分離／只包含一個 prior controls 後批准如下兩步。沒有加入新矩形投影、extent 間隔規則或 association 重構。

### P1.1：保留實際 blue／black，避免 mask 餘裕吞掉端部

獨立 P05 的共同 angle=.25 例，內側 -1 的 flank pixels `(273,491)`／`(368,515)` 皆為實際 blue；對 line 距離分別為 3.0321／2.7826px。第二點被 2.8px 幾何 mask 餘裕排掉，但實際 line renderer 的 white 半厚僅 2px。這是 P1 新增的過拒，必須保留首次失敗，不能把正例改成負例求通過。

因此 final endpoint 的 1..4px bilateral blue／black 見證**不再被純幾何 mask 否決**；實際 white 本來就不符合 blue 或 black，仍不能代供證據。rails 是 white/white 歧義，故 rail mask 排除維持。

- [P1 完整源碼快照](evidence/candidate-dev/p1-source/bvi.cpp) 及同目錄 header，SHA 與上方 P1 相符。
- [P1→P1.1 diff](evidence/candidate-dev/p1-to-p1_1.diff)。
- [P1.1 完整源碼快照](evidence/candidate-dev/p1_1-source/bvi.cpp)，SHA `ed9f9c05e00ee5e8d122f1b4232ad6d0b2c7b64db7fc3d89c44260b5ad367a1d`。

此改動是本輪端部 bounded search 的取樣語義修正，**沒有修改原 V10 的 contact flank**，不把兩種旋轉問題合併計功。

### P2：current cap 不清掉多個 prior endpoint 假設

僅將 `contained>=2 && !d.front_end` 改成 `contained>=2`。原 `contains`、候選計數、nearest tie、128px／width4／angle.6、6 samples／90ms 全部不動；不添加 oracle／hidden identity 資料。獨立 N09 與作者 probe 都先證明真 rails union 的 current cap 原會掩蓋兩個 prior extents，此步才另行授權。

完全重複及近重複 ROI 仍可能觸發既有 nearest tie 的保守過拒。這不是 final 新修好的 dedup 能力，也不以它當作两個 physical notes；原有不足另列 deferred，不藉本輪擴 association 設計。

- [P1.1→P2 diff](evidence/candidate-dev/p1_1-to-p2.diff)。
- [最終源碼 SHA](evidence/candidate-dev/final-source.sha256)。header 始終為 `1426a4b245988c8186f665444f0ddbc9f5f5f93c37d20e513fc77ad5c8bb043a`。
- 除上述單一 relation predicate 外，final 的 constrain、contact、history／dedup／期限／容量規則與 donor 不變。

### 最終作者回歸及證據分層

同一未修改作者 probe 的 **88 assertions 仍 0 fail／native0**，見 [final-results.json](evidence/candidate-dev/final-results.json)。既有不計入斷言分母的真 cap union row 現為 `front_end=true / ambiguous=true / Move=false`；該 row 的 `unresolved-relation-policy` 文字標籤刻意保留其原 probe bytes，不能當作最終決策狀態。正式 N09 正反裁決以獨立 suite 為準。

P1 自驗另跑 ASan＋UBSan：build0／run0、88 assertions 0fail、stderr0，結果與 O2 逐 byte 相同；LSan 關閉，不宣稱 leak-free。最終 sanitizer／全 frozen suite／v2 fixture 的驗證由責任分支獨立報告，本文件不預報尚未完成的結果。所有 P1、P1.1、final source hash 及逐步 diff 可重建，原 donor hash 再核未變。
