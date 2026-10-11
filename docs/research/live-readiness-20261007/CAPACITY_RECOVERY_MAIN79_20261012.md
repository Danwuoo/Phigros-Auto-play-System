# main78 capacity STOP 與必要續作

main78 source `43359f80c772acd56aab8af5a12c631d49a46be0` 的 configure／build Native、runner、verification 皆0，14個宣告object全部存在，held／streams完整、active0。第一個affinity control尚未launch，因retained `50029647183` bytes＋沿用build reserve `1610612736` 超過原48GiB `51539607552`。原capacity-before為false、after為true，未改寫STOP。完整原source／objects／兩Native8及prelaunch closure／spec見[保存JSON](MAIN78_COMPILE_PASS_CAPACITY_STOP_20261012.json)。26回歸／Normal3／AA12／noise18／ABBA36皆NOT_RUN，不能由編譯PASS授成本或live資格。

必要維護修正僅在build與output seal核完後設`$Operation='tests'`，讓8個control Native沿用既有tests reserve1GiB。Configure／build仍使用原build reserve1.5GiB。48GiB、minimum-free20GiB、metadata及全部runtime／R2門檻保持；未調高上限。依現有source／manifest／compiled identity契約，新tracked HEAD仍須fresh14，不冒稱原objects是新source編譯，亦不重跑原75／production9。

為騰出空間，main76／main77共24份inactive AA journal的原payload `4033204157` bytes保存為 `113645634` byte ZIP，逐entry解壓SHA與原件再次核對皆通過後，只刪除原未壓縮副本。原reports／attempts／負結果／STOP不變；歷史完整重讀須restore原path，payload本身可完全恢復。

封存位於`out/retained/consolidation-20261008/inactive-noise-raw-archive79-aa-history/original-journals.zip`，SHA `a3d1529d49270c2885327a2498a4200e1ab2f7fcb82b7278e997da4f6fe7b32f`；同目錄`proof.json` SHA `68087a34d971c41e475edc10a57245de1bb1f77b2a961f1e3f2055e6f3fe23cc`。維護recipe `out/planning/main79-archive-inactive-aa-history.ps1` SHA `ee474fbd8aba880c6871135d82ec3430931105272f97168ab268c24e095f97ef`，未授candidate／cost／live資格。

若尚需歷史archive壓縮，只能保存原whole ZIP bytes及原SHA，另有解碼核對及原件再次核對索引後刪除已驗未壓縮副本；原ZIP內entry proof不改，重新讀原historical build須先restore原ZIP及原output paths。任何未驗／失敗都保留原件，不調容量界限，不碰current／base32／base55必要輸入。這是待必要容量作業，尚未宣稱已完成。

後續仍為fresh14／原controls及26 regression → pre-cost → current Normal3 → AA12／noise18 → 原未跑ABBA36 → full readonly → collection preflight。全部成功後停在current-device收據邊界；本輪不開emulator、device、CU、新thread或ETW recording。只冷完成的結果與actual physical資格分開。
