# 本包重跑與收據定位

開發根 `C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`，本地branch
`codex/prelive-preparation-20261006`。原Phigros checkout保持唯讀。
先讀派工、PLAN、RESULTS、未改事前COST_PROTOCOL與操作包，再核metadata SHA index與external inputs。
所有指令只離線；腳本拒絕原有stage/root/report，STOP不能重開。不要再跑已停止normal cost來補資格。

下列PowerShell例使用新的attempt名稱；工作目錄必須是開發根。既有qualified gate與isolated
compiler snapshot、vcpkg依賴及source-before/audit都須存在且SHA相符；不install dependencies。

```powershell
# fresh complete closure；manifest install OFF、Torch absent。
pwsh -NoProfile -File research/prelive_current/run.ps1 -Operation build -Mode release -BuildAttempt review01
pwsh -NoProfile -File research/prelive_current/run.ps1 -Operation tests -Mode release -BuildAttempt review01 -Attempt review01
# Debug／ASan以各自Mode和fresh BuildAttempt重建，不借不同模式的lib。

# 沒有seed舊touch，從原錄影起點開始；就地SHA核PNG，不複製。
pwsh -NoProfile -File research/prelive_current/run.ps1 -Operation pixels -Mode release -BuildAttempt review01 -Attempt prefix-review01 -Selection C:/Users/wurre/Desktop/PAS-zero-miss-r4-20261006/out/prelive-20261006/full-prefix-selection.json
```

本包最終roots為Release13、Debug05、ASan04；formal core donor分別Release11、Debug04、ASan03。
可選 `-CoreDonor <same-mode-root>` 只用於src／headers／編譯配置與ABI完全相符的未改formal core；
事前核其verified build、source/header及deps manifest，before/after SHA檢查，lib／cache／Ninja
納入新freeze。source差、不同flags/mode或新正式header必須fresh完整重編。
ASan沿既有14.50.35717 support，完整自有code／core插樁；不安裝、無UBSan/leak宣稱。

本輪cost-controller `research/prelive_batch.ps1` 的AA固定12筆上限、逐筆gate止步，normal本輪已STOP；
不可在相同 campaign 重跑controller以改寫 noise。獨立負載case的手動入口示例：

```powershell
pwsh -NoProfile -File research/prelive_current/run.ps1 -Operation pipeline -Mode release -BuildAttempt review01 -Attempt fault-review01 -Variant B -Scene fault -Frames 1000
pwsh -NoProfile -File research/prelive_audit/run.ps1 -Report C:/absolute/fresh-report.json -Attempt fault-review01
```

case均由C++生成12張RGB，n=1000–10000；沒有endpoint參數。A/B只改同圖鏈內的hook，其他正式
owner／ledger／latest／FakeTouch／Journal與instrumentation相同，來源age unknown。
RGB generator/source hash與binary SHA固定輸入定義，不讀歌曲／譜／舊按鍵。
保留1GiB raw餘量、全分母、全部失敗及critical writer停止；不放寬coverage／lateness／RSS門檻。

metadata入口 `prelive-evidence-01/SHA256_INDEX.json` → `closure.json` →
`candidate-manifest.json`／`external-new-out.json`。`package/`含結果、capacity、原PNG selection
及source snapshots；`receipts/`含每stage params、spec、freeze、command、stdout/stderr、
native/runner/verifier、state/checkpoints。大Journal／attempts／events／PNG／binary／dependencies
保留external path/bytes/SHA，不在metadata中重複複製。loaded modules是backing-file SHA，不是memory image。

早期本包`.obj`在`out/prelive-20261006/compiler-object-archive-01/objects.zip`無損保存；
`object-bindings.json`列原absolute path、entry、bytes/SHA。重建早期中間物件時先核ZIP與逐entry SHA，
只還原於指定同名研究root，不能覆寫現有檔案。EXE/lib/DLL/map、所有measurement與STOP仍原位。
source08只是一筆事後hash receipt，不能由短commit／版本號聲稱其當時source可完全重建。

獨立raw auditor使用C++20檢查完整attempt排序、published/owned全分母、time order、Journal rows與
writer join、plan/receipt、五contact及release。故意增加own_down的外部負控制應得到
`integrity=false; receipt_phase_counts`，native0本身不代表science pass。writer壓力有明示診斷缺失，
不冒稱完整prefix evidence。原source/oracle未由負控制修改。
