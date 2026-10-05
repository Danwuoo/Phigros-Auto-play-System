# 4I-R2D交接：只读恢复设计已保存，等待总控独立验收

2026-10-05，Asia/Taipei。完整授权为[4I-R1总控验收](HOLD_OWNERSHIP_X10D_O_4I_R1_CONTROLLER_ACCEPTANCE_20261005.md)，本包结果为[单一最小恢复契约](HOLD_OWNERSHIP_X10D_O_BVI_R2D_REVIEW_20261005.md)。**只交design-only；R1六项source修补未编译／未验，原runner repair剩0，本R2D native execution授权false、runner repair授权false。** 不签算法pass或family no-go，不自行实现／configure或续派。

推荐保留R1 candidate与原69＋22、20新增cases、4 controls、1128coverage rows／2392引用。未来新授权仅局部处理共用RootBinding与跨shell attempt gate、launch／cleanup诚实事实、共用contact assertion→aggregate拒绝链；详细P01–P09／S01–S07／E01–E05／C01–C04均未执行。新r2e三根只是命名提案，未创建、未授权，不是可执行入口。

本次错4I root、natural收据缺失verification exit1后仍跑第二nonzero，是具体机械拒绝反例。历史两个root自然active0只支持原收据；owned-child未跑，原4I两个残留member仍unknown。8份已保全收据不改不重跑，原R1 chat `01a1072a-52d4-7433-bd2c-a9a1b8c360ee`未唤醒。

开工及收尾使用新batch中[check-protection.ps1](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/check-protection.ps1)做必要维护：只读文件／SHA／bytes／JSON、Git read与保存本包compact保护收据。2112源manifest、2039原保护、R1 final receipt、controller-review全部6檔、最新总控页合2120不同文件无不符；原503 Git paths/dirty、HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、index260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1保持。原大manifest仅引用SHA，不复制。最终容量、修正后再核保护与当前ledger见[final-receipt-corrected.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/final-receipt-corrected.json)。

本包首次settlement有一项维护错误：OrderedDictionary的Measure-Object返回null，漏计external docs32391B；独立核验exit1发现。原final-receipt／artifact-ledger／settle-design不覆写，两份新docs旧版本保存于failed-settlement，全部失败计入本包容量。[failure.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/failed-settlement/failure.json)保存分类；新settle-corrected逐项加bytes并完整重核。不计作runner repair或candidate验证，不改变R1 repair剩0。

| 本包允许且实际完成 | 本包受禁、实际0 |
|---|---|
| 必要维护shell read/hash/JSON/Git read、官方Microsoft技术文件只读、保存两份新docs及小型设计／保护／容量收据 | 既有run.ps1／Add-Type runner、process controls／child probes、compiler/configure/build/test／产品binary |
| 新check-protection、失败settle-design与新settle-corrected仅维护自身新文件与容量，无process runner | 原source/oracle/receipt修改、旧settle执行、OS/registry/toolchain安装或门槛变更 |
| 完整manifest逐檔hash（包含PNG引用），无图像内容审查 | PNG audit／复制、full replay、runtime/cost/stress、owner/backend、emulator/ADB/触控、模型、goal/automation、commit/push、续派 |

新证据根 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/`。所有本包entry（两份external docs、JSON、维护脚本、失败及失败快照等）共用原development56MiB，包内≤1MiB、out新增0；全carry以R1 controller receipt完结值为准：development6826223B／剩51894033B、controller1223753B／剩7164855B、aggregate8283663033B／剩306271559B、out374705B。campaign已含controller-review，不重复加。最终新增与扣后余量以本包final-receipt-corrected为准，self hash明示排除，可由独立验收核；原final-receipt仅保留为失败证据。

下一动作仅总控独立验收本设计。任何未来repair／controls／configure必须另立明确root、attempt及额度，不重置R1 1/1；本包native0、configure0、build0、cold tests0、Debug0、ASan0、PNG0、live0。维护shell不计作BVI或process controls通过。

Chapter Legacy全部解锁IN及完整IN Miss=0目标不变，章节分母／当前逐曲解锁unknown；C36h tint1 baseline、main50 comparison/donor live0、suppression OFF、P excluded、X12 not-ready，未取得新增游戏改善或IN zero-miss证据。交总控后停止。
