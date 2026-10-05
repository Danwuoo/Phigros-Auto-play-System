# 第四階段：有界 v3 冷修補完成，停在真圖／Windows 交接門檻

2026-10-05 UTC。產品 Goal 仍未完成；本次雲端委託完成必要冷回歸後，準備並留存交接，由使用者自行移交。未啟動 Windows、裝置、遊戲或新 Codex task；本地 commit 不代表發布。

## 實際改進

隔離候選在 `research/bvi_cold_v3/`；源自 v2 `a53a9b7021bcc900f498047f5cc017b42e4711b0`。正式 `src/include`、owner、模型、擷取、process-control、舊 frozen inputs／oracle／STOP 沒有改動。

| 先前結論 | 新實作／證據 | 更新判定 |
|---|---|---|
| stationary note 的固定 signature 不能湊新Down確認 | 真正 current measured-line 相對法向接近、同線、至少3個獨立RGB／30ms；背景／ID／時間不算獨立 | 合成合法機會可取得；line來源可信度仍需真圖adapter證明 |
| exact duplicate ROI 使同一量測假歧義，舊獨立79斷言有2fail | 僅完整相同measurement作canonical alias，保留原索引；任何alias已attachment都封鎖該組新Down | 原79不改oracle全部通過；near/distinct物件不被合併 |
| 斜交線的±4/6 probes採錯flank位置 | 每側自己的line交點，再沿line normal讀真blue，body/frame/有限線段有界 | 原C4共7項修好；Tap維持原分支，已Down／unknown／completed控制保留 |
| 小suite全綠可掩蓋跨模組問題 | 首次完整legacy抓到V08六項新退步；独立source review另抓到ROI微抖＋背景的假Down | 都先凍結反例、保存失敗、做最小修補，最終重跑全部，不以第一份全綠結束 |

兩個收尾修補：①藍色probe被另一條白線遮住不能讓該線退出Hold的all-lines競爭，真body範圍內多條量測線仍保持歧義；②moving-note新確認要距上次**已計數**front至少1px，0.5px中間幀可以累積，純width/depth/angle微動不製造資格。已Down旋轉Move仍保留；front不移的純旋轉新接入是未證能力，不能宣稱全判定線形式已解。

## 最終結果，分母分列

| 測試 | before → 最終 failed assertions | 分母／限制 |
|---|---:|---|
| 原 frozen 全suite | 原54 → v2 43 → v3 36 | 89 cases ×4 layers＝356 layer-cases；3938 assertions；最終新增失敗0 |
| 原54逐項分類 | 修18、仍36、新0 | C1 5/5、C2 6/6、C4 7/7已修；其餘不抹掉 |
| v2版contract | 0 → 0 | 48 cases／339 assertions，原fixtures／期望未變 |
| v2独立suite | 2 → 0 | 35 cases／79 assertions，原期望未變 |
| v3 relation | v2 27 → 0 | 37 cases／74 assertions，含source-review前先凍結的depth novelty反例 |
| v3 contact整合 | 最終0 | 52 cases／167 assertions；含Tap parity與6 action gate controls |
| V08追加正反例 | 4 → 0 | 4 cases／16 assertions；不能加在原356分母裡 |
| v3独立suite | v2 23 → 0 | 44 cases／101 assertions；17/17合法機會、0/22錯放；3個decision-only不算成功 |
| 独立review後追加 | noise 3/3錯放 → 0/3；2/2正例保留 | 另5個controls，沒有回寫原44例；詳獨立報告 |
| 既有scheduler保全 | 全過 | fake-clock 13 cases／66 assertions＋Flick腳本；Linux舊clock shim，不是Windows／owner整合 |

以上最終正式suite／新contract／獨立皆重跑Release、Debug、AddressSanitizer+UBSan。完整legacy三份結果逐byte相同，SHA256 `33172758b42d8c3e360777e914d3ea0ec21877d275cab842e2d91c658a1d240d`。LSan按已知ptrace環境限制關閉，沒有leak-free宣稱。三配置不是三份獨立遊戲樣本。

剩餘36項：effect混色定義2、capacity fixture只有declared usage卻無實際溢出24、typed invalid仍保留raw body診斷8、ambiguous raw hit依line順序2。原報告維持fail／exit1；新版本contract的eligible-action projection與真實容量邊界另測，不能以新339全過覆蓋旧紅字。effect不擴色閾值；真實混色語義需看真圖。

## 成本及正式整合邊界

Linux／GCC14.2 Release有界函数測量：每場景n256，含所有拒絕，另warmup16；完整raw samples、p50/p95/p99/max/jitter在 [COST_AND_LIMITS.md](COST_AND_LIMITS.md)。v3 128-ROI／16-line场景p99約7.88–12.52ms，max8.12–16.75ms；只是此雲端合成函數，不是主鏈／Windows／RPC成本gate。metadata 236216B，前版185904B；16lines/128ROIs/6samples/90ms硬界仍在。

正式鏈沒有BVI消費seam。現在直接接入會把screen-Y height誤作旋轉Hold depth，body patch誤作front，遺失line association可信旗標，或把每幀ROI index誤當owner contact。這些必須用真實同幀RGB／ROI／all-lines及完整contact prefix辨別，繼續擴合成不能代答。因此此處停止，不冒稱pixels→touch／live-ready。

## 接手入口

- [VALIDATION.md](VALIDATION.md)：精確source、依賴、命令、最終／中間結果、所有失敗
- [RELATION_POLICY.md](RELATION_POLICY.md)、[CONTACT_POLICY.md](CONTACT_POLICY.md)：契約與修補理由
- [INDEPENDENT_REVIEW.md](INDEPENDENT_REVIEW.md)：獨立正反例、實作審查與能力限制
- [READINESS_AND_HANDOFF.md](READINESS_AND_HANDOFF.md)：Windows分層恢復、最小真圖／owner橋接、設備／風險／停止規則
- [CODEX_HANDOFF_PROMPT.md](CODEX_HANDOFF_PROMPT.md)：使用者之後手動交接的自足prompt
- [原54完整狀態映射](evidence/final-original54-classified.json)、[v2→v3差異](evidence/final-v2-to-v3.json)
- [Goal](../../../goals/LEGACY_IN_ZERO_MISS_GOAL.md)：當前Chapter Legacy N、各曲IN解鎖、同候選全曲完整M0實機仍unknown／未完成

既有去識別小包已驗SHA與manifest，不重新索取。下一個最小缺件是真圖短窗＋當幀全候選／線＋足夠contact prefix；不是帳密、整包cache/build、模型或存檔。真機先完成當前版本／曲目分母／解鎖核對，再按安全與成本gate有限手動Play，不能以合成或某曲最佳成績完成Goal。
