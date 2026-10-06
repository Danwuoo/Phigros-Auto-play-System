# Expired-first disabled live seam — final portable validation

**2026-10-06：Release、Debug、ASan+UBSan 同一最終來源全過；原圖3063張新 prefix 亦通過完整獨立 raw audit。**
每配置1339次 check executions（原1132＋focused47＋獨立63＋97），另有1個 Flick case，
PNG11／原 WIC108 decoder checks。這是冷軟體契約／證據驗收，Windows cost、physical、
game adoption 與完整 Chapter Legacy IN Miss=0 仍 NOT_READY。

## 真正最終的來源身份

三配置 `source-before.sha256` 完全相同：
`c4e3b5cc29522b5887ae6b7c0d051c5a802920f929dd227033ea4a39c688aa38`。
最終交付 seal 再核 source、archive、compiler-derived dependencies、全部新 test binaries、
獨立 reviewer 的 source/input/binary，全數一致。

| 關鍵來源 | SHA256 |
|---|---|
| `owner_candidate_step.hpp` | `d4c6b3cdc26369fbce7e7e3c772942e745555ca5e8b99810282902a14f9e9573` |
| `live_seam_runtime_review_v2.cpp` | `6ea212284a2ffc8d170c261bc8d9316834f9e6cff5a7f8616cfac6ff46ad812d` |
| `run_live_seam_incremental_final.sh` | `a0f83100cbacb9e25912755763a0d1b2dbbc0ab55ceb1bb4fa2da0d5c9c6a4f2` |

完整 closure 包含 production manual-session／root CMake、candidate admission／packet、
共享 owner step／receipt stream／Runtime、原13個 translation units、所有測試、PNG依賴、
原 selection／WIC cards及完整 source snapshot。Linux 未編譯或執行 Windows endpoint branch。

## 到期優先修補與未改動的反例

此前完整矩陣通過的來源仍有獨立 blocking 反例：production `accept` 在100ms lease邊界
先接受新 scene，可讓舊 active body 續命；既有 `Runtime::dispatch` 先 poll則會釋放。
該來源／三配置／prefix保留為 [checkpoint](LIVE_SEAM_CHECKPOINT_RESULTS.md)，不冒充本次最後候選。

最終共享 `OwnerCandidateStep::accept` 在 Hook/filter前先 `poll`＋`sync`，舊期限先失效。
獨立 runtime reviewer 原91項未刪減，另加6個既有 corpus 的固定 expiry controls，
共97/97三配置通過。paired production／Runtime路徑均 active0、Down1；未改 renderer／gold。
更早 reviewer v1 的90/91 fixture期望失敗及全部舊 STOP／負結果亦保持原件。

## 有據 incremental build，不重貼舊 binary 收據

修補只有共享 inline header；每配置都重新執行 compiler `-MM`，取得13個 library TU的
完整非system dependency closure。68個逐TU dependency entries全部與先前封存 source manifest
的實際 bytes／SHA一致，changed／unsealed=0；compiler／ar SHA、所有flags、13個member names
及每個member bytes／SHA也完全相同。因此只hardlink復用完整同配置 `.a`。

**所有測試與 prefix executable 全部重新編譯並重新執行，包括 inline header 消費者。**
沒有復用舊測試執行結果或漏跑「未受影響」subset。若dependency不能證明一致，runner會
重編該TU並另核容量。`PROOF_SCOPE.txt` 明列非system dependency證明及read-only system includes；
未虛構歷史每個system header的SHA。

| Mode／flags | 同配置完整 archive SHA256 |
|---|---|
| Release：`-O2 -DNDEBUG` | `0bbc3d07358396d571832e0500348ebca43d3d1c2cca9ada436f9509b9423c8f` |
| Debug：`-O0 -g0 -D_GLIBCXX_ASSERTIONS` | `91a31cb9855ea469c209d359c95fd3b043f773d5aa4b147004d1b9751ce3a5bc` |
| ASan+UBSan：`-O1 -g0 -fno-omit-frame-pointer -fsanitize=address,undefined` | `7009c670658d0bdd836a5089acbcb32e1d703daa3f0ec6d270f609bf543997ae` |

## 三配置完整分母

| Group | 每配置 assertions |
|---|---:|
| 原 owner/front/current/own-prefix | 350 |
| Portable independent | 57 |
| Runtime first-release／RGB rounds | 32＋12 |
| Active-body／tail guard | 35＋15 |
| Workload diagnosis | 145 |
| Scheduler：13 cases | 66 |
| 既有獨立 typed-tail owner／pixels | 130＋57 |
| 原 v1／v2 family完整性 verifiers | 104＋104 |
| 正常尾端目標 checker | 25 |
| **原固定矩陣** | **1132** |
| Focused disabled seam | 47 |
| 新獨立 packet／runtime（含6 expiry controls） | 63＋97 |
| **軟體 checks 合計** | **1339** |

另每配置1個 overdue-Flick case，未虛構 assertion數；PNG wrapper11 controls及原WIC108
samples byte-exact為分開的decoder分母。三配置4017次 software check executions，
不是4017個不同案例。每組實際assertions／checks長度／每個pass均被明確核對。

ASan／UBSan無findings。容器ptrace不支援LeakSanitizer，明確 `detect_leaks=0`，不宣稱leak-free；
原LSan fatal保留。三配置 `overall.exit=0`，個別compile／stdout／stderr／exit／binary SHA全保存。

## 最新同來源原圖3063 continuous prefix

- 原 selection SHA：`a2f91ae9e121a713283a0017e7fef012f307cd8557b80c1b71f4f7e1d2aabe44`
- 使用原隔離還原root，**3063/3063** PNG逐張bytes／SHA／CRC／profile核對；未重複複製PNG
- 同一最終 Release Runtime、單一連續archived source clock、自有ledger；沒有舊contact／cursor／action seed
- 自己的 **Down55／Move121／Up51**；verified release、active0、ledger valid
- 9889 raw events，27,594,746B；全部rows/events/PNG及RGB digests保存
- 最新 report SHA：`1c41c6a2842284d6c1bc5e28ad92fa2fb41dab55059003aaaffcd3db830d4683`
- 未改動的獨立auditor：integrity、all input、plan join、coordinate join、complete prefix、metrics全true，errors=[]
- 227 injection receipts、140 release calls；無failed receipts或release uncertainty
- typed-tail targets／plans／confirmations／normal-tail Up **全0**，是已測safety prefix；不證明真實尾端改善

Synthetic B正例仍是三offset各8次 typed confirmation＋normal visible-tail completion，
與此段真圖0個已確認 typed-tail 事件嚴格分開；合法尾端機會分母仍 unknown。

## 證據與容量

- 本輪：`evidence/live-seam-expired-first-{release,debug,sanitizers}-01/`
- 交付前獨立seal：`evidence/live-seam-expired-first-final-seal-01/`
- 最新prefix：`evidence/live-seam-expired-first-release-01/prefix-3063/`
- 獨立reviewer：`../../docs/research/live-readiness-20261006/review/live-seam/expired-first-final-{release,debug,sanitizers}-01/`
- 最新raw audit／tail counts：`../../docs/research/live-readiness-20261006/review/live-seam/expired-first-raw-audit-3063/`

Capacity stage guards保持20GiB floor＋16MiB write margin；Release／Debug預留64／128MiB，
已證明庫可復用的ASan預留512MiB（若TU需重編則640MiB）。prefix亦逐幀檢查floor＋16MiB。
ASan結束available=21,571,112,960B，高於floor96,276,480B。一次exec transport斷線後，
由新read-only shell查明Debug已正常exit0；沒有重送或重複執行。

自建ELF可recoverably封存以留交付空間；每個gzip member的CRC／ISIZE及uncompressed SHA
均須核對後才unlink。archive receipts／原路徑／SHA在 `evidence/expired-first-final-ELF-archive-01/`；
所有source、原PNG、raw、report與三個完整 `.a`保留。

## 仍未做／未取得的資格

未重跑原3938 frozen oracle，未執行Windows/MSVC/native QPC/WIC/GTest、真capture／gRPC／touch、
live AA/ABBA成本或完整IN遊戲，未執行supplied unknown binaries。既有donor CLI不變；
candidate production C++ API admission仍hard-disabled UNQUALIFIED。這份portable結果
不能授予Windows cost、physical／遊戲採納或Chapter Legacy全曲完整IN Miss=0資格。
