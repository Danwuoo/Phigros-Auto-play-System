# 視覺／時間小試收據

2026-10-05，cloud Linux；基準commit `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本資料夾只含隔離研究檔，不改正式 source/tests。

## 1. 公式probe

命令：

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  docs/research/zero-miss-20261005/evidence/vision-timing/formula_probe.cpp \
  -o /tmp/phigros-vision-timing-formula-probe
/tmp/phigros-vision-timing-formula-probe
```

編譯／執行exit0。stdout保存於`formula_probe.csv`，包含時間fit與preserve gate兩段CSV表頭；不當單一schema數據集。使用inspect後自行撰寫的算術模型，不是編譯正式`game_tracking.cpp`，亦不是observer RGB test、runtime latency量測或遊戲效果。OLS、90ms history裁切、error/|velocity|、preserve max(12,width×.1)對應正式`game_tracking.cpp:299–300,390–464`。案例true times/delay由本probe作者設定；不從遊戲讀譜或內部clock取值。

3個時間案例：age0、age20ms、age20+0.5×source_ms；各6份sample，其中過90ms者同公式移除。無小於10ms arrival間隔，所以沒有bucket替換。preserve例有4種增量×6步；只probe該predicate、假設其餘guard皆滿足，不宣稱整鏈actions。

## 2. 原樣BVI typed core probe

命令：

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  -Iresearch/x10d_o_bvi_r1 \
  docs/research/zero-miss-20261005/evidence/vision-timing/bvi_stationary_probe.cpp \
  research/x10d_o_bvi_r1/bvi.cpp \
  -o /tmp/phigros-bvi-stationary-probe
/tmp/phigros-bvi-stationary-probe
```

編譯／執行exit0。編譯警告保存`bvi_compile.log`；stdout為`bvi_stationary_probe.csv`。原碼SHA：
- R1 `bvi.cpp`: `f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce`
- R1 `bvi.hpp`: `6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f`

本輪核最新`research/x10d_o_bvi_build/bvi.cpp`及R2E/R2F/control/resume的core均為相同cpp SHA；最新build header另核與R1相同。本probe因此適用這份相同核心，但沒有跑Windowswrapper或完整product target。

2個typed情境×4幀，對每幀各呼叫never_executed及known_down的constrain，assert已Down皆可Move；靜止descriptor皆只有1份independent／無新opportunity；moving descriptor第3/4幀有新opportunity。RGB與descriptor signatures是typed輸入，合法性僅限該conditional介面；不是extract計算結果。hit隨假設line移動，核心relation看的是part descriptor。這是qualification差異，沒有physical身份gold、true-game輸入、backend、touch、owner整合、模型或任何網路。

## 3. 邊界

- GCC：`g++ (Debian 14.2.0-19) 14.2.0`。
- 可執行檔僅位於`/tmp`，不作交付binary或frozen Windows runtime。
- 沒有performance claim；編译/執行的elapsed不是產品成本。
- 原raw `measurements/`與`out/`缺席；本輪沒有重新檢查歷史PNG、SHA、timing histograms或full-prefix replay。
- 其他研究分支的測試另有報告，不相加成本分母或當此probe覆蓋。

## 4. 新增最小 synthetic RGB 對照

使用最新build相同核心，命令：

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  -Iresearch/x10d_o_bvi_build \
  docs/research/zero-miss-20261005/evidence/vision-timing/bvi_rgb_stationary_probe.cpp \
  research/x10d_o_bvi_build/bvi.cpp \
  -o /tmp/phigros-bvi-rgb-stationary-probe
/tmp/phigros-bvi-rgb-stationary-probe
```

編譯／執行exit0，8個frame-case。源碼含獨立小型raster與事前assertion：79×9藍Tap固定(320,500)、白線y520→510→504→500；對照只把Tap每幀橫移1px。4幀為10/30/50/70ms。實跑原extract→relate→constrain，hash由extract實算，不手填signature。第3/4幀兩組current contact均支持，靜止組independent1、新opportunity0；移動組independent3/4、新opportunity1。沒有完整GameObserver、真圖、正式owner或注入；query/all-lines是外部宣告current幾何，所以只算conditional RGB契約。

SHA256：
- `bvi_rgb_stationary_probe.cpp`: `79cd178b75ab26b790b9020e7b208f71d865d21bf02b48df90489c4fc784bfd4`
- `bvi_rgb_stationary_probe.csv`: `236be8683cff810ef482ca29283cc3e711367d68885af63dae457d4c35ec07f6`
- `bvi_rgb_compile.log`: `b2e634dc718db0243cb4103b9a57946f80db6cc22f2fea634623dea3df8ea5b4`

公式與typed輸出另以同binary第二次執行byte-compare一致。新增RGB檢查以本節原始輸出為準，未增加大量回合或做性能統計。
