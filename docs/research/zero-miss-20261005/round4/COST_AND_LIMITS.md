# 有界 Linux 函數成本與不可代簽的門檻

來源：`evidence/cost/final/`；可重跑`research/bvi_cold_v3/run_cost_probe.sh`。GCC14.2、C++20、Release `-O2 -DNDEBUG`、Linux x86_64，當時getconf列9 logical processors。使用steady_clock，不是Windows QPC。共享雲端，無CPU affinity／頻率鎖定或負载隔離，其他研究工作可同時存在。

每版每場景warmup16、正式n256；單調時鐘量同一次`extract→relate→constrain`總區間，保留每次raw ns、不相加stage p99。預渲染與配置在計時外；每次source/key更新在計時外。固定ABBA各跑一次，不等於歷史A/A noise gate已取得資格，不以比值宣稱產品加速。

## 完整分母

四場景都含全部256次，這套固定RGB／fake never-executed場景所有迭代皆拒絕action。沒有排除失敗／拒絕子集，也沒有取得accepted-action／owner／writer的耗時證據。`invalid_source`是最早拒絕路徑。沒有touch backend、實機、RPC、journal或frame lease adapter。完整raw結果讓接手者能區分這些限制。

以下毫秒，兩輪分列，勿把它們當不同真實場景樣本：

| 版本/輪次 | 場景 | p50 | p95 | p99 | max |
|---|---|---:|---:|---:|---:|
| v2 A1 | single Hold | 3.215 | 4.224 | 5.864 | 6.286 |
| v3 B1 | single Hold | 3.212 | 3.677 | 5.125 | 6.653 |
| v3 B2 | single Hold | 3.164 | 3.768 | 4.181 | 4.376 |
| v2 A2 | single Hold | 3.137 | 4.102 | 4.724 | 5.326 |
| v2 A1 | 128 exact aliases/16 lines | 11.639 | 13.404 | 14.544 | 20.500 |
| v3 B1 | 128 exact aliases/16 lines | 9.633 | 11.112 | 11.770 | 13.271 |
| v3 B2 | 128 exact aliases/16 lines | 9.481 | 10.779 | 12.518 | 16.752 |
| v2 A2 | 128 exact aliases/16 lines | 11.236 | 12.689 | 14.323 | 15.701 |
| v2 A1 | 128 distinct/16 lines | 7.077 | 8.395 | 13.491 | 19.332 |
| v3 B1 | 128 distinct/16 lines | 6.856 | 8.140 | 9.015 | 12.103 |
| v3 B2 | 128 distinct/16 lines | 6.686 | 7.535 | 7.875 | 8.117 |
| v2 A2 | 128 distinct/16 lines | 6.308 | 7.265 | 8.430 | 10.981 |

invalid_source v3 p50約0.00157/0.00158ms、max0.015664/0.019099ms；v2 p50約0.000962/0.000961ms。microsecond小數不能推論Windows優劣。每份JSON也含max−p50 jitter、query/line數、全部拒絕數與probes。

## 硬界與風險

- metadata236216B（v2 185904B），GCC ABI靜態檢查<1MiB。128 candidates、16 lines、history6／90ms；新canonical aliases及scratch arrays固定容量。
- 1280×720 frame有921600 pixels；原候選每幀hash全RGB。本probe含這條路徑；`Observation.probes`包含921600計數但不是CPU指令／memory-byte上界。複雜三角函數、line遮罩與alias比較成本不能只靠probe個數推得。
- 本批最大counted probes：v3 aliases1033600／distinct958144，均低於6291456硬budget。這不是窮舉worst-case。
- shared host scheduling尾端噪音、深度/姿態/線數分布、真實ROI重複率、資料快取、adapter複製、正式owner及診斷writer仍未量；不把本批全部拒絕path時間套給正式action總延遲。
- 原X11/R3 AA資格不足與X12 not-ready維持不變。必須在Windows exact source/binary/依賴/profile與完整closure凍結後，按有效新method取得A/A、全分母和安全資格，再做A/B。不要只壓小suite成本或修改90%門檻求過。

结論：此數據足以交代候選的函數級量級及有界資源，不能簽live-ready；下一階段需真圖adapter與Windows主鏈成本，暫停合成擴展。
