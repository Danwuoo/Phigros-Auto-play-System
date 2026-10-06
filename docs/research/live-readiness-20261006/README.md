# 2026-10-06 雲端實機前 readiness 續作

本分支從使用者指定 main `e9e305e3aba13f05a786c74540419428610b2f76` 開始，
獨立 checkout；沒有 push／PR／merge、Windows／emulator／遊戲／裝置操作。
產品 Chapter Legacy 全曲 IN 解鎖與同一候選完整 IN Miss=0 仍未完成。

## 閱讀順序

1. [工作與 gate 依賴](PLAN.md)
2. [accepted baseline／ignored ZIP 完整核验](baseline/README.md)
3. [成本／owner 證據的獨立反例](review/INDEPENDENT_READINESS_AUDIT.md)
4. [成本 admission 的獨立三配置審查](baseline/analyzer_review/README.md)
5. [最終 typed-tail 三配置及真實 3063 幀](../../../research/prelive_portable/EXPIRED_FIRST_FINAL_RESULTS.md)
   - [活動 body 中間里程碑](../../../research/prelive_current/ACTIVE_BODY_REPAIR_RESULTS.md)保留歷史結果；最終狀態以前項為準
6. [可移交 Windows prompt](WINDOWS_HANDOFF.md)

最終結果見 [RESULTS](RESULTS.md) 與 [機讀readiness](READINESS.json)。
交付檔的完整commit/tree/SHA列於包內HANDOFF_MANIFEST；NOT_READY不可解讀為允許live。

## 已分開的四種 readiness

- **Software readiness**：當前 pixels、typed transport、sole owner／scheduler、自己的 receipts、
  unknown release、terminal identity 與 regression；合成通過不等於遊戲採納。
- **Measurement design**：shared entry/release、持續 body、normal tail 各有實際完整分母；
  baseline A 和候選 B 的工作量不同時如實分列，不用非空 Down/Up 冒稱等效 full Hold。
- **Same-host Windows qualification**：exact source/build/flags/profile/input、qualified native process、
  有效新 A/A/noise→必要 A/B 比較、完整 raw／diagnostics／RSS／jitter；Linux不替代。
- **Live integration／permission／physical gate**：目前裝置/app/mapping、合法接觸與遊戲採納、候選 live entry，
  加上使用者對具體 device/touch 步驟的授權。production C++ API接縫已接上且硬UNQUALIFIED；live CLI／綁真收據的啟用validator
  仍未實作，不能把它們叫純硬體測量；cost通過也不自動啟動。

舊 36 fail、AA STOP、noise inadequate、timeouts/skips、unknown、缺原始bytes全部保留。
新工具拒絕 false READY 並不代表成本已合格；新 renderer/typed proof 也不製造人工 gold。
