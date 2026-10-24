# 2026-10-24 測試封存與後續交接

本輪依使用者要求停止修正並整理、commit／push `feature/chart-driven-multigame`；main 不動。原生譜面、音源、校正、APK、binary、PNG、RPC／UI原始紀錄保留repo外。此處是既有證據摘要，沒有啟動emulator、ADB或新增遊戲輸入。

## 最新 build006 的實際範圍

- 18個原生回合，14個完整送完接點、8個由manual-mode確認結算；6個完整輸入卻未確認結算。18輪最後kernel均為零。
- 正式Legacy20首中，14首建立過原生回合（光為0接點停止），12首至少一次完整輸入，8首由保存結算圖確認AP；其餘6首尚未實跑。Verrückt是額外暖身，另有本binary的AP，不計入20首。
- 原始native manifest的failure／manual_result_confirmed／AP旗標不改寫。保存結果圖的人工像素複核與執行完成分列；沒有結果圖時不以RPC成功或選曲頁歷史最佳分推斷本輪成績。

## 20首IN逐曲結果（本build）

| # | 曲目 | 回合／完整輸入 | 結算圖複核 | 原生結算確認 | 狀態 |
| --- | --- | ---: | --- | --- | --- |
| 1 | Glaciaxion | 1／1 | 943,759；727P／0G／0B／2M | 1輪 | 非AP |
| 2 | Eradication Catastrophe | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 3 | Credits | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 4 | Dlyrotz | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 5 | Engine x Start!! (melody mix) | 3／2 | 未保存本輪完整結果 | 0輪 | 完成輸入，成績未驗 |
| 6 | 光 | 1／0 | — | 0輪 | 中途停止／未輸入 |
| 7 | Winter ↑cube↓ | 1／1 | AP，1,000,000 | 0輪 | AP像素證據保留 |
| 8 | 混乱-Confusion | 0／0 | — | 0輪 | 未測 |
| 9 | Cipher | 1／1 | 980,980；1040P／0G／0B／1M | 0輪 | 非AP |
| 10 | FULi AUTO SHOOTER | 1／1 | AP，1,000,000 | 0輪 | AP像素證據保留 |
| 11 | HumaN | 0／0 | — | 0輪 | 未測 |
| 12 | [PRAW] | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 13 | Cereris | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 14 | Pixel Rebelz | 1／1 | AP，1,000,000 | 1輪 | AP像素證據保留 |
| 15 | Non-Melodic Ragez (MUG Edit) | 1／1 | 未保存本輪完整結果 | 0輪 | 完成輸入，成績未驗 |
| 16 | Sultan Rage | 1／0 | — | 0輪 | 中途停止／未輸入 |
| 17 | Class Memories | 0／0 | — | 0輪 | 未測 |
| 18 | SURREALISM | 0／0 | — | 0輪 | 未測 |
| 19 | Bonus Time | 0／0 | — | 0輪 | 未測 |
| 20 | ENERGY SYNERGY MATRIX | 0／0 | — | 0輪 | 未測 |

額外暖身Verrückt：本build一輪756接點時20ms遲到停止，另一輪1285 Perfect／零Good、Bad、Miss／100萬分，manual-mode確認結算。舊build004／005的暖身AP另有原件，不能轉移成新版本的通用資格。

## 已完成歌曲但未成功記錄（保留待修）

| 回合 | 曲目 | 原生 failure | 已保存證據 |
| --- | --- | --- | --- |
| `chapter-IN-panel-20261024-222944-158-63248a/round-06` | EnginexStartmelodymix.CrossingSound.0 | `user_stop` | 最後僅選曲頁；本輪完整成績無法補證 |
| `chapter-IN-panel-20261024-224938-792-17be8d/round-01` | Cipher20.TetrajectoryfeatCtymax.0 | `manual_round_ended_without_result_confirmation` | 980,980，1040P／1M，保存結算圖 |
| `chapter-IN-panel-20261024-225510-019-1dc6ea/round-01` | NonMelodicRagezMUGEdit.Normal1zer.0 | `user_stop` | 最後僅選曲頁；本輪完整成績無法補證 |
| `chapter-IN-panel-20261024-230148-998-497f6e/round-01` | FULiAUTOSHOOTER.MYUKKE.0 | `manual_round_ended_without_result_confirmation` | AP，保存結算圖 |
| `chapter-IN-panel-20261024-231754-197-209c3e/round-01` | Wintercube.CtymaxfeatNceS.0 | `manual_round_ended_without_result_confirmation` | AP，保存結算圖 |
| `chapter-IN-panel-20261024-232852-424-b55024/round-01` | EnginexStartmelodymix.CrossingSound.0 | `user_stop` | 最後僅選曲頁；本輪完整成績無法補證 |

FULi AUTO SHOOTER與Winter ↑cube↓的原始程式仍以未確認結算結束，AP由本次保存PNG複核補記在衍生表，不將原始failure改成成功。Cipher同樣保留980,980／1040Perfect／1Miss。Engine x Start!!兩輪與Non-Melodic Ragez的最後影格已是選曲頁，不能把頁上歷史最佳分當成該輪結果。

## 全部面板回合索引

共21個session、26個已留下原生manifest的回合。最後233128 session僅等待選曲，rounds空，未計為已開打或成功。下列路徑皆相對本機 `local-assets/phigros-4.0.1/`，實際來源hash及完整欄位見[機器索引](SESSION_RECORDS_2026-10-24.json)。

| 回合 | 曲目 | DOWN／UP | 全曲／結算 | failure |
| --- | --- | ---: | --- | --- |
| `chapter-IN-panel-20261024-182004-079-fe1ade/round-01` | Verruckt.Raimukun.0 | 0／0 | 否／否 | user_stop |
| `chapter-IN-panel-20261024-182106-677-1694e4/round-01` | Verruckt.Raimukun.0 | 0／0 | 否／否 | user_stop |
| `chapter-IN-panel-20261024-190641-385-bb3ebb/round-01` | Verruckt.Raimukun.0 | 1285／1285 | 是／否 | manual_round_ended_without_result_confirmation |
| `chapter-IN-panel-20261024-205450-978-b42027/round-01` | Verruckt.Raimukun.0 | 1285／1285 | 是／是 | — |
| `chapter-IN-panel-20261024-205450-978-b42027/round-02` | Glaciaxion.SunsetRay.0 | 360／360 | 否／否 | actual_UI_receipt_expired |
| `chapter-IN-panel-20261024-210148-825-bd5b2f/round-01` | Glaciaxion.SunsetRay.0 | 5／4 | 否／否 | compiled_step_lateness_limit_exceeded |
| `chapter-IN-panel-20261024-210249-128-99dc95/round-01` | Glaciaxion.SunsetRay.0 | 360／360 | 否／否 | actual_UI_receipt_expired |
| `chapter-IN-panel-20261024-210546-700-5e911f/round-01` | EradicationCatastrophe.NceS.0 | 226／225 | 否／否 | actual_UI_receipt_expired |
| `chapter-IN-panel-20261024-222704-866-f0c178/round-01` | Verruckt.Raimukun.0 | 756／755 | 否／否 | compiled_step_lateness_limit_exceeded |
| `chapter-IN-panel-20261024-222944-158-63248a/round-01` | Verruckt.Raimukun.0 | 1285／1285 | 是／是 | — |
| `chapter-IN-panel-20261024-222944-158-63248a/round-02` | Glaciaxion.SunsetRay.0 | 729／729 | 是／是 | — |
| `chapter-IN-panel-20261024-222944-158-63248a/round-03` | EradicationCatastrophe.NceS.0 | 593／593 | 是／是 | — |
| `chapter-IN-panel-20261024-222944-158-63248a/round-04` | Credits.Frums.0 | 535／535 | 是／是 | — |
| `chapter-IN-panel-20261024-222944-158-63248a/round-05` | Dlyrotz.Likey.0 | 584／584 | 是／是 | — |
| `chapter-IN-panel-20261024-222944-158-63248a/round-06` | EnginexStartmelodymix.CrossingSound.0 | 433／433 | 是／否 | user_stop |
| `chapter-IN-panel-20261024-224813-304-1f291e/round-01` | EnginexStartmelodymix.CrossingSound.0 | 107／107 | 否／否 | user_stop |
| `chapter-IN-panel-20261024-224938-792-17be8d/round-01` | Cipher20.TetrajectoryfeatCtymax.0 | 1041／1041 | 是／否 | manual_round_ended_without_result_confirmation |
| `chapter-IN-panel-20261024-225342-058-b359c1/round-01` | 光.姜米條.0 | 0／0 | 否／否 | user_stop |
| `chapter-IN-panel-20261024-225510-019-1dc6ea/round-01` | NonMelodicRagezMUGEdit.Normal1zer.0 | 1235／1235 | 是／否 | user_stop |
| `chapter-IN-panel-20261024-225819-868-841aab/round-01` | PRAW.Bluewind.0 | 1171／1171 | 是／是 | — |
| `chapter-IN-panel-20261024-230148-998-497f6e/round-01` | FULiAUTOSHOOTER.MYUKKE.0 | 843／843 | 是／否 | manual_round_ended_without_result_confirmation |
| `chapter-IN-panel-20261024-230526-324-bc53e3/round-01` | Cereris.SunsetRayNceS.0 | 898／898 | 是／是 | — |
| `chapter-IN-panel-20261024-230859-108-0a2d23/round-01` | PixelRebelz.Normal1zer.0 | 941／941 | 是／是 | — |
| `chapter-IN-panel-20261024-231524-705-4a51ab/round-01` | SultanRage.MonstDeath.0 | 247／245 | 否／否 | compiled_step_lateness_limit_exceeded |
| `chapter-IN-panel-20261024-231754-197-209c3e/round-01` | Wintercube.CtymaxfeatNceS.0 | 733／733 | 是／否 | manual_round_ended_without_result_confirmation |
| `chapter-IN-panel-20261024-232852-424-b55024/round-01` | EnginexStartmelodymix.CrossingSound.0 | 433／433 | 是／否 | user_stop |

## 面板之前的控制回合

Glaciaxion初期5輪及sweep004首輪另列，既有Dlyrotz AP／單Tap／manual-mode／裝置fixture報告保持原身份。

| 來源 | DOWN／UP | 全曲／結算 | failure |
| --- | ---: | --- | --- |
| `chapter-legacy-IN-live-20261024-171208-966/round-01` | 288／287 | 否／否 | actual_UI_receipt_expired |
| `glaciaxion-whole-song-live-001` | 0／0 | 否／否 | user_stop |
| `glaciaxion-whole-song-live-002` | 94／93 | 否／否 | actual_UI_receipt_expired |
| `glaciaxion-whole-song-live-003` | 729／729 | 是／否 | manual_round_ended_before_whole_song_complete |
| `glaciaxion-whole-song-live-004` | 0／0 | 否／否 | anchor_not_fresh_or_first_contact_not_future |
| `glaciaxion-whole-song-live-005` | 729／729 | 是／是 | — |

## 原件與版本

- 本機整理目錄：`session-records-2026-10-24-001/`；`local-catalogue.json`盤點原有280個目錄，`original-artifacts.json`固定1448份原件SHA；`visual-reviews.json`保存逐圖複核來源。它們不提交Git。
- 使用者已實際完成 `song-test-restart-20261024-222415-179-56858ea7` 的fresh_fixture_verified流程，並以新boot執行build006；這是一次實際重啟／五指驗證，不代表跨重啟或延遲穩定性合格。
- build006 binary SHA256：`07ede2edf228fa9805305c4ea3603f2235075f477000165937b42bb2d242edb7`；搭配panel004／library004／state005／readiness007／sweep008；全部原譜、profile與contact／opening身份保留。舊各版資料均未刪除。
- 20首／62原譜／20組音源的原生章節入檔，Verrückt額外3難度原件，以及歷次HUD、首音、成本、容量、結果稽核，以各原manifest與本機全目錄索引接續。

## 下次接續

1. 先處理結果擷取／保存／確認與畫面離開的生命週期，尤其Cipher、FULi、Winter新原件；使用者本輪要求先封存，沒有在本次整理修改辨識邏輯。
2. build006仍出現Verrückt／Sultan Rage的20ms遲到；參考capture-ready／publish／owner／snapshot／journal各階段時間，保持原限制。不要把HUD修正說成全曲時序已合格。
3. 完成剩餘6首，並重測光的首音取得；逐note Flick／Drag漏判、自適應選參及其他未取得的資格仍待驗。
4. 音訊同步維持暫緩，獨立input lead／source pixel age及長期／跨重啟重現率未驗；只在使用者再要求時恢復實測。

測試驗證與來源變更見[VALIDATION](VALIDATION.md)、[共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)、[面板](SONG_TEST_PANEL.md)與[交接](MODEL_HANDOFF.md)。
