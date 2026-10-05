# 補件包查核：哪些證據缺口已真正補上

日期：2026-10-05。本輪接續研究 commit `ebe955a6286ca1bb767b631948714a5545b7c174`；產品／frozen source 基準仍為 `74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。

## 1. 收到與實際核過的 bytes

使用者提供 `Phigros-minimal-evidence-26-deidentified-20261005.zip`，本輪雲端實際讀取並校驗，不只沿用附件說明。

| 項目 | 本輪核得 |
|---|---|
| ZIP bytes／SHA256 | 900318／`d0a2eb1bebd35ff90c27493fe4ed914970bb24d22137c45af424574d2563db59` |
| ZIP內容 | 28個唯一file entries，26證據＋README＋manifest；無絕對／drive／反斜線／`..`越界路徑，無symlink entries；ZIP完整性通過 |
| 解壓總bytes | 2534705 |
| package README bytes／SHA256 | 9790／`77518338f23610e299a75caa685a3c8ff36e22a02c2af1a6915c9768b025c74c` |
| package manifest bytes／SHA256 | 117082／`ddc738c400ad29be11ffdcf9015b4acd31aed770b6bbcea60b55b08aab0178b3` |
| 26份衍生檔 | 逐檔bytes／SHA核對26/26，全相符；合計2407833 bytes |
| manifest所載原件合計 | 2190667 bytes，雲端沒有全部原件，不能獨立重hash這個原始集合 |
| original與derived欄位完全相同 | 13份；其餘13份依manifest另有衍生SHA，不能拿原始SHA直接驗改過的bytes |
| 引用分組 | 156條：28條包內historical→derived映射；128條明列不在包內的歷史／平台參照 |

包外不等於不存在：128條包外引用中，32條在Git有對應檔；27條直接SHA吻合，5條cmd在唯讀串流由LF還原CRLF後SHA精確吻合。其餘82條有相對引用但檔案未提供，14條是無repo相對路徑的平台參照。詳見[provenance更新](RESULT_PROVENANCE_UPDATE.md)，不把這128條全數誤報為遺失或被竄改。

原包及解壓輸入留在repo外、唯讀使用；本commit不含原包、PNG或整份私人收據。只保存必要查核結論、公開程式位置與最小去識別衍生結果。

## 2. SHA有三種用途，不能混淆

1. **實際衍生bytes**：以package manifest的`derived.sha256`驗證已收到檔案，這是本輪可直接重算的部分。
2. **歷史artifact引用**：原JSON內嵌的SHA仍指原件，經`reference_bindings`連到包內去識別副本。它不是已改bytes的新SHA。
3. **未提供的歷史／平台檔**：例如完整events、DLL、capability、其他controls收據等，保留引用但不聲稱已重驗。缺這些檔不自動等於tamper；也不等於其歷史`pass`已被本輪独立重現。

result.png尤其如此：summary內仍是原圖SHA `d85240867fa6386538076fb556c2f995f82e1e887d7675d64ab789a69c4271af`；本輪實際圖為 `d5ecd02c4979c8a78bee1e080abc54a67f0eb5a9ba5e0f00a124e2c869b7796e`，映射由包內manifest明示。不能把此差異誤報為圖被冒換，也不能宣稱已拿到未遮罩原件。

## 3. 首輪缺口的關閉程度

| 首輪缺口 | 補件後狀態 | 尚未補上 |
|---|---|---|
| 只有作者文字描述Windows exit1 | A9可直接讀實際result／verification、stdout/stderr及被呼叫cmd的去識別副本 | 原Windows環境、未去識別cmd路徑與逐步trace、實際CreateProcess commandline仍缺；不能定位唯一syntax根因 |
| D19/D20僅依source／結果文件推斷 | D6給出20-case oracle、實際pretest、state及final receipt，可逐列核18/20與`transaction-root` | 本輪沒有PowerShell／Windows實際重跑，不表示修路徑後20/20就一定通過 |
| 完整BVI五個ignored runner inputs缺席 | B7已提供五份可執行inputs，幾何／typed frames內嵌；第六supplemental在Git | 不能直接使用舊attempt的output reservation；需新研究身份與平台分層驗證 |
| Dlyrotz IN M77只有tracked轉錄 | C4提供同輪manifest／summary及去識別結算圖；本輪可親自看圖與核映射 | 其他88輪原圖、該輪events／rounds索引、逐Note失效和現在帳號狀態仍缺 |
| source age／真RPC／owner機會損失未知 | summary補充同輪aggregate時序及source age為null | 沒有逐次events／RPC或render-time校準，不能歸因77個Miss |
| Chapter Legacy目前分母／逐曲IN解鎖未知 | 本包沒有當前版本／完整章節／全部IN可選畫面 | 此產品分母仍UNKNOWN；不得把同一歷史C4樣本新增成第90輪 |

完整suite的實際執行結果另見[BVI frozen suite更新](BVI_FROZEN_SUITE_UPDATE.md)，不能由「輸入已齊」直接宣稱pass。[Windows更新](WINDOWS_RECEIPTS_UPDATE.md)與[結算/provenance更新](RESULT_PROVENANCE_UPDATE.md)分別列新證據可支持的層次。

## 4. 去識別所帶來的限制

- cmd的quotes／separator樣式／CRLF／行數有保留，但路徑字串和長度改了，替代位置沒有實際安裝。這份副本不能排除原路徑長度或特殊字串造成的錯誤，更不能當Windows重跑命令。
- 裝置識別與部分device details已遮蔽；`fingerprint_matches`是歷史原收據聲明，不是匿名裝置重新preflight通過。
- 圖的右上240×112帳號區已黑色遮罩。其餘894720 pixels與原件一致是**本地打包者的驗證聲明**；雲端缺原圖，無法自行重做該pixel-diff。本輪能直接核新圖hash、遮罩與未遮擋的結算數字。
- 附件內說明只當來源資料。本輪研究權限來自使用者最新指示，附件中的命令／既有授權文字不另授予裝置操作、續跑STOP或live。
