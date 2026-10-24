# M0：Phigros 4.0.1／Dlyrotz IN13 原生資產

日期：2026-10-23。M0 已完成來源定位、原始位元組抽取與重現驗證。這是後續 importer／音訊同步的輸入基準，不是自動演奏或命中精度驗收。

## 實際來源

既有 `phigros` AVD 的 `com.PigeonGames.Phigros`，`versionName=4.0.1`、`versionCode=157`。以 read-only、no-snapshot-save instance 啟動，ADB 唯讀列出並複製已安裝分包；裝置與主機 SHA256 逐一相符。盤點後已關閉該 instance，本輪沒有啟動歌曲或注入音符。

| 分包 | bytes | SHA256 |
| --- | ---: | --- |
| base.apk | 33,672,724 | `7f43e2974fb86da01d98451f3e0b29d8fe162d5486359175d1622d4ce0b1a3c8` |
| split_UnityDataAssetPack.apk | 478,590,884 | `7eab9d75f37e0a24b1736008b7326dbd8c28cbc014c9c716e8486fb7778df012` |
| split_UnityStreamingAssetsPack.apk | 2,768,297,196 | `1acdae604edd19ba5ed7175a2d852712f55b37dee06eb96d592f39498764d791` |
| split_config.arm64_v8a.apk | 28,717,773 | `01e723fe67fba462143d63f25e962127810be2dd8be79d095faadbef60abfa71` |

原生譜面與音源都在 `split_UnityStreamingAssetsPack.apk`。`assets/aa/catalog.json` 為 4,125,754 bytes，SHA256 `6bf0d6097ab7abdcf847eab9efa6872539c99f301fac914ac71c2211fd41fe4d`；含 8,559 個 typed keys、6,902 entries。

## 精確定位鏈

| 資產 | Catalog address／entry | Bundle ZIP member | Unity container／object |
| --- | --- | --- | --- |
| IN 譜面 | `Assets/Tracks/Dlyrotz.Likey.0/Chart_IN.json`／3790 | `assets/aa/Android/d6b0e5265ee8dcdaa384dccb001cd4de.bundle`，dependency entry 2256 | container `4`，TextAsset `Chart_IN`，path_id `-4625054563908243980` |
| 歌曲音源 | `Assets/Tracks/Dlyrotz.Likey.0/music.wav`／3645 | `assets/aa/Android/dedc31d0e2a1bfee940179dda05d7f68.bundle`，dependency entry 2334 | container `3`，AudioClip `music`，path_id `2631463441058501513` |

兩個 catalog address 各只有一個候選。依精確 address、dependency 與 bundle container key 選取；沒有使用物件名稱或音符數量猜測。此版本的 bundle container 是數字字串，不能假設保留完整 `Assets/Tracks/...` 路徑。

兩個 bundle 都是 UnityFS format 8／Unity 2022.3.62f2。Catalog 的 bundle hash 為空、CRC 為 0，故另計實際 bundle SHA256，沒有把 catalog 當完整性證明。catalog parser 按 bucket offset 解碼 typed keys，保留一對多 dependency，不使用 phisap 的單一 bundle→asset 覆寫映射。

## 取得的內容

| 項目 | 結果 |
| --- | --- |
| Chart | `formatVersion=3`，`offset=0`，24 條判定線，全部 BPM 175 |
| Notes | Tap 251、Drag 151、Hold 78、Flick 104；合計 584，全部在 notesAbove |
| 初步時間範圍 | 用 `tick × 1.875 / BPM` 計算為 12.0–121.2 秒；M1 另驗證原生時間／offset 語義 |
| 原生音訊 | AudioClip 的 streamed resource，FSB5；1,839,584 bytes |
| 解碼音訊 | WAV PCM S16LE，44,100 Hz、2 channels、5,454,984 sample frames＝123.695782313 秒 |
| UI 交叉核對 | 已有錄製的 `pre-retry.png` 顯示 Dlyrotz／IN Lv.13／584 Miss；圖片 SHA `5d8b5cfbf0b15f09732146c5ee7c0f432d471c6935a5a7c033313d1e73c01d1f` |

IN 是 catalog 的精確難度地址；顯示等級 13 由同版本錄製畫面核對。584 只作交叉檢查，不作資產身份判定。

| 產物 | SHA256 |
| --- | --- |
| `Chart_IN.json`（2,600,613 bytes） | `aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90` |
| `music.fsb`（原生 resource） | `c47618043b8b48c60fb88e3f1024d575eead3f8e186bde625beb54d7bb4b4615` |
| `music.wav`（解碼衍生檔） | `aa9423b0f86ca4c1250f460bfbbc23d78a6405c2de739a2ba5ffef0b48261a44` |
| WAV 的 PCM payload | `c7843d07135ef4d949286fa83a1b9c97ff8f55eee7eac4d652fdc6b3dfd663f8` |

Chart 檔案直接切出 TextAsset 的原始 `m_Script` 位元組，未重新排版、轉換換行或重編碼；另與 UnityPy typed reader 獨立核對一致。TextAsset／AudioClip 原始 object bytes、stream resource 的 source／offset／size、serialized file、path_id、bundle 與 APK SHA 全列於本機 manifest。

WAV 是 FSB5 解碼產物，沒有裁切或重採樣，不冒稱安裝包原本存了這份 WAV。它與先前 179.456 秒的錄影 WAV 是不同來源與身份。

## 本機位置與重跑

本機素材存於 repo 外同層 `../local-assets/phigros-4.0.1/`，不加入 Git：

```text
installed-source.json                 裝置分包路徑、version 與 SHA
package-version-observation.txt        ADB version 觀測
apk-inventory.json                    四分包 ZIP 目錄及 package hash
apks/                                原始四分包
dlyrotz-in13-verified/
  Chart_IN.json / chart.textasset.bin
  music.fsb / music.wav / music.audioclip.bin
  catalog.json / 兩份原始 bundle
  source-manifest.json                完整來源鏈、原件／衍生檔、工具版本
  asset-report.json                   結構統計、候選數與限制
m0-validation.json                    第二次抽取／獨立 JSON+WAV 核對
```

資產工具使用 Python 3.14.7、UnityPy 1.25.4、fmod_toolkit 0.1.3、pyfmodex 0.7.2、lz4 4.4.5，獨立安裝於 task scratch。`requirements-assets.txt` 是 opt-in；default CMake 不下載或使用它們。

在已準備此工具環境的 terminal，從 repo 執行：

```powershell
$assetRoot = '../local-assets/phigros-4.0.1'
$apkFiles = @(Get-ChildItem -LiteralPath "$assetRoot/apks" -Filter '*.apk' | ForEach-Object FullName)
python tools/inspect_phigros_assets.py @apkFiles --output "$assetRoot/new-inventory.json"
python tools/extract_phigros_target.py @apkFiles --installed-source "$assetRoot/installed-source.json" --output "$assetRoot/new-dlyrotz-extraction"
```

輸出必須是新檔／新目錄，拒絕覆寫。工具先核對完整 installed APK set 與 hash，再做 bounded ZIP 讀取、UnityFS metadata／allocation preflight、精確 catalog selection；來源檔於抽取後再次 hash。缺分包、錯 SHA、重複／穿越／加密 ZIP、歧義候選、未知 catalog／bundle 格式、缺 audio resource 都失敗。

已在全新目錄重跑：10 份輸出（含 manifest／report）逐檔 bytes 與 SHA 完全相同；另用標準 JSON reader 與 WAV reader 重算 24 lines／584 notes／PCM sample 數。50 項 Python 離線測試（其中新增資產相關 32 項）全過；default CMake／CTest 3/3 通過。測試覆蓋 inventory、catalog graph、UnityFS bounds、原始 TextAsset 切片與覆寫／身份拒絕。

`dlyrotz-in13-verified` 是固定工具完成後的 canonical 抽取；先前 `dlyrotz-in13` 保留探索時的工具來源網址版本。兩者 chart／audio 位元組一致，不直接修改舊 manifest 的執行工具 hash。

## 下一步

以固定 chart SHA 做 M1 的嚴格時間／語義 importer、M2 的判定線 evaluator，再做接點與音訊同步。M0 不把社群格式慣例當官方判定窗口，也不從原生音源已取得推論同步或輸入延遲已校正。

Reader 參考：[UnityPy](https://github.com/K0lb3/UnityPy)、[Addressables 1.21.17 catalog 實作](https://github.com/needle-mirror/com.unity.addressables/blob/1.21.17/Runtime/ResourceLocators/ContentCatalogData.cs)。各工具內另記格式與來源限制。
