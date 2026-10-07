# ASan 模組掃描與維護接續

`journal06` Release/Debug 功能、encoding、原 session 歷史負控制、108幀精確對照及嚴格
schema compatibility 已完成；ASan configure成功，首批core在native編譯中觸及
**held-identity-cap64**，guard runner125，停止並保留原STOP。active_final=0、所有已held
handles signaled、streams complete；identity_trusted=false，不能作成功編譯收據。

stdout顯示要求5個object時，CMake的C++20模組dyndep先掃描全部22個cpp。
正式source沒有module/import宣告；新的fresh ASan root只將維護configure設為
`CMAKE_CXX_SCAN_FOR_MODULES=OFF`，其餘C++20、Debug/ASan flags及完整22個object不變。
未使用module的普通include dependencies仍由MSVC/Ninja追蹤；不改guard64、poll、
compiler/API、測試/oracle，不借failed ASan06的objects。

Release/Debug已完成的穩定source/build/test收據逐項驗SHA和原input closures後接續，
ASan07完整新build及原同等回歸。維護recipe另立fresh outputs，不覆寫任何舊結果。
本必要cost前置接續使用已預聲明32GiB累計留存、build reserve1.5GiB、free20GiB，
新candidate建置/功能根4GiB仍必需。原16GiB收據不回寫；正常cost/RSS/scenes/
cadence/denominator與所有buffer硬界不變；沒有裝置操作或live資格。
