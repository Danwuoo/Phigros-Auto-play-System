# 2026-10-25 Windows native 接續與 Flick 比較

接續雲端 `d89af4c64c945230ecb7c7184fd7871ce3e9a4be`，以 fast-forward 從本機 `3e66ab7` 更新。本機起始工作樹無未提交變更；main 未修改。公開曲庫沿用固定上游與既有 LFS 物件，沒有重建曲庫或重新執行上游 sync。

## 追加要求：結算 v2／不存圖（目前使用版本）

使用者暫停歌曲測試後要求先改善結算與容量。已完成 [結算與影像保存政策](RESULT_RECOGNITION_AND_STORAGE.md)，另建 `catalog-native-build-003`（195 份固定產物），不修改下列 002 封存。

- 目前 `chart_experiment.exe` SHA256：`58c4ecd4ca86ee35113b80da499f036af19ad1c7f210ad93b81078db48c17228`。
- native **29/29**、portable configure/build/CTest **4/4**；完整 Python **229 項，228 通過、1 symlink 權限 skip**；面板離線 SelfTest **52 checks**。
- v2 frozen-pixel **100/100**：8 個結算正例、92 個 playing 負例；三個舊漏記結果重新分類成功。歷史 failure／原圖不改，沒有補稱為本次實機成功。
- `state-result-v2/dataset.json` 以原 RGB bytes 的絕對引用建立新 policy；`panel-v2-{48,60,72}` 及 `fresh-boot-v2-distance-{48,60,72}.json` 綁定 003，三組完整 GUI pins 及 **23/23 native offline preflights** 通過，全部回報 `image_retention=none`／新 result policy。`result`／`diagnostic` 兩個明確 opt-in suffix 另有兩次離線前檢通過。
- `restart-v2-config.json` 已做 PlanOnly；新 binary 的歌曲／裝置整合仍 **NOT RUN**。本次五指證據屬於下列先完成的 002 probe 與其實際 boot，不把它改寫成 003 歌曲資格。
- 一般回合不落盤圖片，改保留實際結算 frame metadata／SHA；Flick receipt 與 metadata-only HUD 分析仍可使用，沒有數字 OCR 或自動 AP 判定。

以下保留新增結算／保存要求之前的建置與準備紀錄。

## 建置與封存

- 新 build：任務外部工作區 `work/catalog-native-build-001`，Visual Studio 18 2026／MSVC 19.50.35729，Release、x64、4 jobs。
- 使用既有 `work/phigros-vcpkg-install/x64-windows` 與原 vcpkg toolchain；`VCPKG_MANIFEST_INSTALL=OFF`。patched gRPC header 與實際 patch-symbol smoke 通過，第三方依賴未重新建置。音訊兩個選項保持 OFF。
- native **29/29 CTest 通過**；portable `work/catalog-portable-build-001` **4/4 CTest 通過**。完整測試名稱／輸出保存在新封存 evidence。
- Python unittest：**228 項，227 通過、1 skipped**。使用 `PYTHONUTF8=1`、`PYTHONIOENCODING=utf-8`、原 task-local LZ4 與實際 PowerShell；唯一 skip 為 Windows 不允許建立 symlink。初次非 UTF-8 環境的失敗日誌另存，未刪除或算為通過。
- 修正 `test_catalog_native.py` 的 timeout fixture，以 `Path(...).name` 判斷 `IN.json`，避免 Windows 路徑分隔符使 timeout 分支未被測到。production importer 不變。
- 新工具 `tools/archive_native_runtime.py` 只封存 Git 已追蹤的程式來源、明確選定的 runtime EXE／DLL 與指定建置證據；排除 assets／未追蹤私人檔案。5 個合成測試驗證既有輸出保留、source/build 邊界、缺檔／重名／預算拒絕及中途變動不得產生成功 manifest。
- 正式新封存：專案外 `local-assets/phigros-4.0.1/catalog-native-build-002`，schema `pas.adaptive-build-archive.v1`，192 份固定 SHA256 產物。封存建立時 HEAD 為 `d89af4c` 並記錄 staged source snapshot；不把後續提交冒充當時 build HEAD。較早 `catalog-native-build-001` 亦保留，正式設定使用 002。
- 新 `chart_experiment.exe` SHA256：`0e9f8e7c31ff906f56c339df699f25a312ab262c515a4789207a1afe576e3310`。舊 build006 與 Dlyrotz AP binary／manifest 均不覆寫。

## Profile、設定與證據邊界

以新 binary 重新產生 Legacy20＋Verrückt 共 21 份 baseline profiles。每份 contacts／steps／peak、plan fingerprint、opening fingerprint 與 first DOWN 均與其舊 profile 相符；缺少 Flick 欄位時的舊預設相容性已實際核對。

Glaciaxion IN 使用授權本機 4.0.1／157 原件 `source-manifest-IN.json`；章節的聚合 `source-manifest.json` 不是該 profile 綁定的難度 manifest。最初誤用聚合檔的準備被 identity gate 拒絕，未產生可執行 profile；之後依 SHA 找到正確來源重新準備。公開 4.0.1.2 原件沒有代替本機來源。

三組新 plan／library／launch config 均另存專案外；正式 Legacy20 順序與額外暖身分類保留，只有 Glaciaxion 預先勾選。21 首 baseline 加兩個距離候選，共 **23/23 offline check-adaptive 通過**，三組 GUI 的完整資產 pins 與 cold-boot PlanOnly 亦通過。離線前檢沒有 discovery 或輸入，也不證明現在的 boot 或遊戲判定。

本機 Windows 正式 WinForms 控制項的 1440×900、1080×720 預覽皆已產生並檢視，控制項位於畫布內、六欄與底部動作可見。曲庫程序以「Phigros 譜面曲庫」視窗啟動。Computer Use 未列出 PowerShell 承載的 GUI，故不將預覽說成完整的互動點擊／DPI 驗證。

## 距離比較控制

固定 phase −80 ms、lead 30 ms、travel 60 ms、release 24 ms、Drag 32 ms、inset 64、projection D7000、capacity 5、原 APK／chart／HUD／import policy。

| 距離 reference pixels | 接點 | events | peak | 與 60 px 相同的非 Flick 接點 |
| --- | ---: | ---: | ---: | ---: |
| 60 baseline | 729 | 6080 | 5 | 678 |
| 48 | 729 | 5863 | 5 | 678 |
| 72 | 729 | 6277 | 5 | 678 |

51 個 Flick 的 DOWN 時間、travel 與末端停留保持不變；距離改變會改變名義速度與細分步數。移動判定線與投影也會使螢幕端點位移不隨名義距離單調增加：例如同一個約 103.714 s 的 Flick，48／60／72 候選的端點位移約 22／10／2 pixels。這是編譯命令座標，不是 Android 實際運動或遊戲接受結果。

歷史 build006 Glaciaxion 完整回合的 51 個 Flick receipts 與 HUD 像素已用新分析工具唯讀稽核：全部有匹配的成功主機回執，最大 Flick step dispatch lateness 約 5.145404 ms；4 個端點位移偏短、3 個近距離干擾候選。沒有獨立逐 note 遊戲判定，不能把它們直接標成該輪的 2 Miss，也不能由 RPC 成功推導 AP。

## 本次開機與遊戲比較

使用者操作新 restart 入口，完成新的 `fresh_fixture_verified`：Android 實際 peak 5、正常 DOWN／MOVE／UP 路徑、最後 kernel zero。新 probe binary／source／DLL 均綁定 002 封存，未移植舊 boot 字串或放寬容量檢查。每次重啟仍須重新驗證。

使用者隨後要求先完成下載、稍後再測。72 px 面板已關閉，關閉時沒有 native owner，**本次歌曲比較 0 輪／NOT RUN**；沒有結算、winner、改善或新 AP。五指 fixture 是獨立裝置驗證，不計入歌曲回合。預定的 72→60→72 及 48 候選保留，待使用者明確恢復；若其間重啟 emulator，原容量證據不得直接沿用。

原本結算漏記、20 ms lateness、音訊／input lead／source pixel age 資格均保留原範圍。

## 原件保留

更新前建立 8,798 份私人原檔的存在／大小／mtime 清單與 369 份 manifest／設定／結果 SHA256。更新與離線準備後均無變動。保留 PAS 的 13 個非 `.gitattributes` blobs 相符；`.gitattributes` 的差異來自已同步的雲端 LFS 規則。保留 PAS／proto／third_party 本次沒有修改。

完整新工作證據位於專案外 `catalog-native-local-20261025-145855/`，所有新舊原件與失敗嘗試保留；私人譜面、WAV、APK、校正、boot／容量原件及遊戲紀錄不加入 Git。
