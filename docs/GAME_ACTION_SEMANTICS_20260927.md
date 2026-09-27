# 五指容量與持續接觸語義（2026-09-27）

使用者補充：Phigros可使用四／五指；Hold須維持到整條結束；連續黃色Drag可用持續接觸覆蓋。這是本輪動作策略的依據，不把既有兩指設定當成遊戲限制。實作 `0897e21`、最終source `59c92bf32992e02d0cb61c97dec5e96b66f0e0f2`，planner9；observer29／主用擷取不變。四／五指後端核對及三配置回歸已通過，遊戲效果待實戰。機器可讀證據見 [metadata](GAME_ACTION_EVIDENCE_20260927.json)。

## 實作

- 新增 `configs/phigros-hd-assist-five-lead35.json`（5 contacts／35ms）與 `configs/avd-touch-five.json`。舊兩指profile及歷史三輪原始資料保留，不改寫其結果。
- `touch-bench --kinds ... four five` 在當前Native Touch Fixture核對四／五指同時可見、輪流移動一指而其他指不漂移、逐指錯開Up、最後active=0；五指profile的取消案例另核對全部五指active與release。只從Fixture pixels的per-pointer trace及計數認定能力，不從RPC成功推論遊戲命中。
- capability增加 `max_contacts_verified`，從實際通過的多指案例取得，不能只採profile宣告的max_contacts。旧schema2報告無此欄時最多沿用2；因此把舊兩指報告的profile數字改成5也不能通過新五指門控。geometry／mapping／device／APK hash仍須相符。
- Hold尾端擬合 `tail_crossing_ns` 保留診斷，不能提前安排正常Up。當前同一frame可見tail／rails、線方向與hit一致、tail確實過線才鎖定正常release（當前evidence＋原20ms保守緩衝）；後續frame不再把這個terminal deadline往後推。未知／裁切tail下，持續有效觀測只更新有界接觸期限。新出生但tail已過線的Hold拒絕補Down。
- 連續Drag可共用一個已active接觸：下一個當前Drag的保守區域須覆蓋該接觸，沿線距離≤30%note width、最多48px（無寬度資料保留原2px），法向≤2px，預測窗口重疊及當前evidence／類型／不確定度有效。這是研究範圍，不宣稱可見寬度等於遊戲精確hitbox。
- 新鮮後繼Drag在missing判定前匹配，避免原ID消失先釋放再Down。只有一個可覆蓋的active Drag leader才共用；多個leader都符合時不選第一個、不合併；Tap／Hold／Flick仍獨立。純預測、舊frame或下一個過遠／跨線／不重疊候選不能延長接觸。

100ms source／target、90ms anchor、Hold missing60ms、Drag missing40ms、UI／context／unknown input撤銷及stop release均未放寬。正常完成與證據失效取消分開；灰色Hold或特效造成的漏辨仍可能走取消路徑，本輪沒有宣稱已解決視覺缺口或AP。

新增四個fake-clock回歸覆蓋五Hold獨立contact與誤尾預測、實測尾端正常結束且terminal不延後、未知tail的原100ms到期、跨多個Drag ID／位置小幅變動的持續接觸、法向不覆蓋／多leader競爭負例；擴充capability門控回歸。初輪23項相關回歸發現舊Drag成員會取消新續接的leader，修正後23／23通過。覆核另發現兩次讀monotonic clock之間窗口可能跨界，後繼匹配改為保留同一個optional結果，避免重取後空值解參考；最終三配置由59c92bf重跑，早一版三配置142／142 logs另保留。尚未進遊戲。

## 驗證與命令

最終source的同一Release executable SHA256 `8e1d3ce7dd74a49ab893640a7b432880e83686f73a1292cc7845e8e5e18fb669`。Release／Debug／嚴格ASan各 **142／142** 通過，CTest總時間10.57／27.52／68.70秒，logs為本機root下 `test-frozen-windows-{release,debug,asan}.log`。ASAN_OPTIONS／suppressions未設定，第三方DLL未插樁；測試時間不是遊戲延遲。

當前Native Touch Fixture：八類各30次、**240／240** 通過，包括four30／30、five30／30；每個ID均輪流作唯一移動指，其他指固定，逐指Up、final active0。另 **30／30** 取消案例各五指同時可見、Down／Up delta各5、release IDs0–4且final active0。`capability_verified=true`、`max_contacts_verified=5`，報告 `touch-five/summary.json` SHA256 `73dc5c94f1418a1e75bf4488871d27c849e0d6105adc79ec03c167ff672dbcd3`。APK本機與安裝hash相同：`b3bfaf4e4e876e7833c9af1bd6149b40cdc9fa46b67646d52ea045dde6705dc6`。

五指遊戲profile的當前preflight `fingerprint_matches=true`／mismatches空；舊兩指capability配五指profile則false、`touch_mapping_or_capacity`，沒有建立input。兩指bench profile要求five也在建立後端前拒絕。首次bench因Fixture尚未到前景而安全退出；待`am start -W`確認後重試才得到上述通過報告。完成後已將原Phigros activity帶回前景，未按PLAY或開始新局。

環境：Windows11 Pro26200、Core Ultra5 125H／18 logical processors、約32GiB RAM、RTX3050 6GB／Intel Arc；MSVC19.51／VS18。既有phigros AVD37.1.11、5vCPU、8192MiB、host GPU、window scale0.8、Android16／SDK36／x86_64／density320；emulator-5554、gRPC loopback8554。擷取1280×720／rotation1／payload fast256KiB，觸控720×1280／rotation90。bench期間沒有build／test／遊戲或第二capture並行。

| Fixture bench指標（ms） | n | p50 | p95 | p99 | max |
| --- | ---: | ---: | ---: | ---: | ---: |
| RPC call | 1290 | 0.846850 | 1.358500 | 1.652889 | 6.050500 |
| schedule error | 1290 | 7.619050 | 15.396430 | 17.809045 | 29.756600 |

時間使用host QPC monotonic。上述schedule error包含Fixture bench輪詢／像素核對節奏，不能套用到遊戲owner高精度排程或宣稱命中；本輪不作遊戲時序／AP性能結論。完整分布、環境與檔案hash保存在metadata／raw。

```powershell
out/release-v145/Release/pas.exe touch-bench --config configs/avd-touch-five.json --repetitions 30 --kinds tap hold move flick pair simultaneous four five --fixture-apk measurements/fixture_cpp_v2/pas-touch-fixture-v2.apk --output-dir measurements/game-semantics-20260927/touch-five
out/release-v145/Release/pas.exe game-preflight --config configs/phigros-hd-assist-five-lead35.json --capability measurements/game-semantics-20260927/touch-five/summary.json
out/release-v145/Release/pas.exe run --config configs/phigros-hd-assist-five-lead35.json --mode assist --capability measurements/game-semantics-20260927/touch-five/summary.json --duration-s 185 --no-preview
```

實戰必須使用當前匹配、實際已驗證五指的報告，且使用者手動選曲PLAY已就緒。本輪軟體與能力核對完成，尚未啟動新遊戲；不能把Fixture或合成成功當作Hold／Drag遊戲動作已驗收。

本機root：`C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/game-semantics-20260927/`。舊三輪binary另存 `pas-before-semantics.exe`；新source的build／related／完整回歸logs及能力raw已分開保存。沒有模型訓練／weights、merge／push或跨task回報。
