# 第三方依賴與授權紀錄

這份清單取自本機已安裝的 vcpkg manifest 套件及 NDK 原始 NOTICE，保留原文。vcpkg baseline 為 `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb`；正式自有邏輯仍為 C++20。套件包含建置／測試用依賴，並非全部都是 runtime DLL。

| 套件 | 已驗證版本 | 原始授權／copyright |
| --- | --- | --- |
| abseil | 20260107.1 | [abseil](third_party/abseil.txt) |
| c-ares | 1.34.8 | [c-ares](third_party/c-ares.txt) |
| cli11 | 2.7.2 | [cli11](third_party/cli11.txt) |
| FFmpeg | 9.0.2，vcpkg `avcodec`／`swscale`，動態連結 | [FFmpeg 原版授權彙整](third_party/ffmpeg.txt) |
| grpc | 1.81.1 | [grpc](third_party/grpc.txt) |
| gtest | 1.18.0 | [gtest](third_party/gtest.txt) |
| nlohmann-json | 3.12.0 | [nlohmann-json](third_party/nlohmann-json.txt) |
| openssl | 3.6.4 | [openssl](third_party/openssl.txt) |
| protobuf | 6.33.4 | [protobuf](third_party/protobuf.txt) |
| re2 | 2025-11-05 | [re2](third_party/re2.txt) |
| utf8-range | 6.33.4 | [utf8-range](third_party/utf8-range.txt) |
| vcpkg-cmake | 2025-08-07 | [vcpkg-cmake](third_party/vcpkg-cmake.txt) |
| vcpkg-cmake-config | 2026-07-21 | [vcpkg-cmake-config](third_party/vcpkg-cmake-config.txt) |
| vcpkg-cmake-get-vars | 2025-05-29 | [vcpkg-cmake-get-vars](third_party/vcpkg-cmake-get-vars.txt) |
| zlib | 1.3.2 | [zlib](third_party/zlib.txt) |
| scrcpy server | v4.1 tag `49c9501fb26f456bbf4a341dd68879f670c67452`, SHA-256 `deacb991ed2509715160ffdc7907e47b4160eb30d1566217e9047fd5b8850cae` | [scrcpy](third_party/scrcpy.txt) |
| Android native_app_glue | NDK 30.0.16248370 | [AOSP NOTICE](third_party/android-native-app-glue.txt) |

NDK glue 的 Apache 2.0 完整授權亦見 [Abseil 授權副本](third_party/abseil.txt)。Windows／MSVC／SDK／JBR／NDK 工具本身由既有安裝提供；此倉庫沒有重新散布它們。未來製作正式二進位發行包時，須隨實際打包內容一併提供相應 notices。

scrcpy server 原版檔案僅存於忽略的本機量測依賴目錄，程式啟動時驗證上述 SHA-256；倉庫沒有納入 server 二進位。FFmpeg 9.0.2 由 vcpkg baseline 建置 `avcodec`、`avutil`、`swscale` 動態庫；本次配置停用外部 x264／x265、GPL 與非自由元件。附上的 vcpkg 原版 `copyright` 保留 LGPL 2.1 授權全文與檔案授權說明。二進位交付時須隨實際 DLL 與 codec 配置再核對授權。
