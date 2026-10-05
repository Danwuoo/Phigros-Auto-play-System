# X11-P-R1 交接

2026-10-03。**交付完成，待總控獨立驗收；not-ready，X12阻擋。** [結果](RUNTIME_X11_P_R1_RESULT_20261003.md)/[預先protocol](RUNTIME_X11_P_R1_PROTOCOL_20261003.md)與新根`runtime-x11-p-r1/final-summary.json`為入口。沒有傳訊總控、建chat/goal或自行後續實驗。

## 獨立驗收入口

1. 核old-binding-verified/old-artifacts-after、workspace前後dirty SHA、source-inverse-audit。57 src/include inverse僅pending8行，Wake class exact。共同5檔/metadata另列；source.tar不包含historical PNG junction。
2. 核source-binding-before-cost(305)/compiled-dependency-freeze(1580)/runtime-closure/runtime-metadata、另negative-tool-freeze；由tlogs確認manual-session與meter真的編譯相同Wake/events/archive。不要只把CMake source superset當實際編譯清單。
3. 看8套XML及collector-historical-regression。B0 original238/238，B1 237/1既定fail，pending8/5與13/13，新Release兩版10/10，ASan新10/10+pending13/13。新collector逐testcase，旧root skipped缺省卻有1skip的XML已回歸。原期待保留。
4. 每版512-input bridge比較原檔每byte/row/EOF。R1只保存小summary，原public-events直接引用；lead0 bridge與lead35 normal meter分開。無新full PNG replay。
5. 核成本前method/options/capacity、4stress→8AA順序與command/exits；noise四項不足及aa-owner-1 114/128 coverage fail。ABBA=0，剩餘16額度不自動使用。ASan stress receipts0是timing Unknown，不能包成dispatch/latency通過。
6. 逐raw frames/receipts/releases與archive event segments全分母、n/p50/p95/p99/max/jitter、queue/bytes/drop/fault/EOF，核release negatives12calls/2receipts及fake cleanup分類，再查全部artifact SHA/bytes與final容量。

完整commands/exit/log/XML/source snapshot保留。prepare/run/bind/freeze-dependencies/asan/collector/finalize都是`tools/*runtime-x11-p-r1*.ps1`；negative工具CMake source及configure/build-command JSON可重現，接既有frozen core。**既有根拒覆寫，不執行Build/AA/ABBA/Stress/Tests到同一輸出。** 若未來有新工程，另立有界root/protocol，不改此負結果或原X11 freeze。

純離線只讀查核（新logs另存）：

```powershell
./out/x11-p-r1/runtime-B0/pas.exe x11-provenance
./out/x11-p-r1/runtime-B1/pas.exe x11-provenance
./out/x11-p-r1/runtime-B0/pas.exe --help
./out/x11-p-r1/runtime-B1/pas.exe --help
```

## 準備路徑與下一決策

```text
B0 C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/x11-p-r1/runtime-B0/pas.exe
   SHA 42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09
B1 C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/x11-p-r1/runtime-B1/pas.exe
   SHA ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a
profile measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1/x12-profile-preview.json
capability measurements/game-semantics-20260927/touch-five/summary.json
future output measurements/game-assist/forward-live-20261003/sessions (未建立)
```

profile仍lead35/uncertainty30/fast RGB888 top-down/256KiB/1280×720 rotation1/五指mapping，與父profile僅log_dir差；static capability reference不等於新的device/APK/geometry preflight。新binary不同於原live及旧X11 B0/B1，不得使用版本37/19或舊live結果代替新SHA。

原≤6輪B0/B0＋B0/B1/B1/B0預案仍conditional；本輪不提供可執行live資格。journal容量準備已明確改native512MiB/round，六輪保守3,277,848,576B加future12GiB總根/reserve5GiB，不靠人工監看256MiB。沒有新full-recording、pixel-clips或真觸控授權的自動執行。

下一決策由總控獨立簽收本負結果後選擇：維持not-ready，或另立有明確樣本量/環境隔離依據的成本工程；本task不再找安靜樣本、不改pending或壓診斷救成本。原live未列compiled SHA不作無限阻塞條件，也不回補成已驗；14lost/77Miss與閉環採納只有未来合格有限比較才可能回答。X10d-O另案，X10b仍否決。
