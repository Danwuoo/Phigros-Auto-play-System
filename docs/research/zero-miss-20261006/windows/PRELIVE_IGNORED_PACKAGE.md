# 2026-10-06 近期 ignored 開發封包

使用者後續要求合併main並縮小封包；開發分支已快轉合併至main並push到 `f07dc40`。
main的AUX修改SHA保持原樣；Windows保留名稱造成的索引缺項已就指定兩個path修正，
修正後index tree精確等於HEAD tree，沒有改持久Git／系統安全設定。

精簡封包為原 main root 的 `phigros-prelive-compact-20261006-f07dc40-01.zip`，精確大小、
SHA與縮小比例見 [PRELIVE_COMPACT_PACKAGE.json](PRELIVE_COMPACT_PACKAGE.json)。
實際1,001,668,802B（約1.00GB），比完整包5,867,867,149B縮小82.93%；
13,670個保留payload逐項解壓SHA核對通過。ZIP SHA
`e59766f09879b927b26cd0947ce501cea05f16fd838cfb1cce9321a63a0d7530`。
它保留全部本輪raw／收據／STOP、3063張原PNG、60段正式clips與最終三配置EXE／DLL／map；
省略舊build roots、`.obj`／`.lib`／`.pdb`、old objects ZIP及isolated MSVC compiler binaries，
保留runtime DLL、headers、tools、licenses與metadata。ZIP內有保留／省略清單及逐項SHA。
因此它適合證據審查、離線候選執行與沿既有環境接續研究，**不是完整獨立重建環境**；
CMake／vcpkg metadata仍描述原完整安裝，重建需既有相符依賴或下述完整封包。
原資料與5.87GB完整封包保持原件。精簡不刪raw列、不把STOP／失敗改成pass。

下列為第一版完整封包的範圍與建立收據。

使用者另行授權本機 commit／push，並將近期開發需要的 ignored 檔案整理成 ZIP，
放在原 main root。開發資料 checkpoint 為 `55738325eb1cbcd83fe18e18c7d0320dc8eedcd4`，
原推送分支為 `codex/prelive-preparation-20261006`，後續已整合main。
本包不新增 runtime／遊戲測試或裝置權限。

封包：原 main root 的 `phigros-prelive-ignored-20261006-5573832-01.zip`；
旁有 `.zip.sha256`。精確 bytes／SHA／逐項核對數量／容量及 main 狀態見
[PRELIVE_IGNORED_PACKAGE.json](PRELIVE_IGNORED_PACKAGE.json)。ZIP 是 Git source／封存文件的
補充資料，保持本機檔案；source、recipe、收據透過上述分支交付。

收录范围：

- 開發根全部 `out/prelive*`：所有成功／失敗 attempt、raw timings／events／Journal、
  Release／Debug／ASan 建置、同配置 core donor、maps／PDB／DLL／libs，以及既有無損 `.obj` archive。
- 開發根 `out/windows-handoff`：process qualification、MSVC snapshot、JSON／IO adapters、
  全部收據與 STOP；另附 formal build 使用的五份 generated gRPC 檔案。
- 原 main 的 `out/vcpkg_installed/x64-windows` 與 vcpkg status／info：Release／Debug
  第三方庫、headers、DLL／PDB、tools、CMake metadata 與 licenses，省略舊 build／download staging。
- 本輪實際使用的3063張原PNG、原始 recording index／summary／progress、60段正式回歸 clips，
  以及 historical runtime-x11-p reference evidence。

ZIP內 `_package/FILES.json` 逐檔列來源、entry、bytes、SHA256、用途與 ignored 核查；
`_package/README.txt` 列還原對照，另附 `.gitignore` 快照、封包 recipe 及結果／重跑／操作包。
建立時對本輪 external seal 和原PNG expected SHA 核對，再從ZIP串流解壓核對每一項 payload。
封包不刪除或覆寫原始資料、frozen receipt、STOP或使用者 AUX 修改。

先解壓到空的審查目錄。兩個頂層 payload prefix 對應原 main 與開發 checkout 的目錄名稱；
核 SHA 後選擇性還原，不能整包覆寫現有 checkout。原index涵蓋7722幀，但本包只帶實際使用的
1–3063幀PNG；其餘4659幀及其他歷史資料未收錄，不宣稱全部歷史raw可由本包重算。

MSVC／Windows SDK／CMake／Ninja／PowerShell仍是host prerequisite。cache及歷史收據保留
原absolute path；換機不能直接沿用舊 qualification，須fresh roots、依賴綁定及process核驗。
資料上限32GiB、ZIP上限16GiB、最低free20GiB，這是本次封包額度，獨立於先前冷測容量帳。

目前情況維持：三配置回歸及自身原圖前綴已通過；A/A在Hold零動作停止、ABBA未跑，成本與
physical gate仍NOT_READY。沒有候選live CLI或遊戲採納證據，Chapter Legacy分母／IN解鎖
仍unknown，完整IN Miss=0目標未完成，沒有AP前置。
