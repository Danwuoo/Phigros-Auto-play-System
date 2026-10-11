# Cold producer 相鄰 QPC 間隔診斷

2026-10-11 main57／`5dce75d` actual 三TU／16控制及三場完整raw auditor通過。Tap／Hold零超期；Dense兩次render超期照報，三場原數值true，但只屬diagnostic，不替代Normal3、不宣告fix或資格。原compiled55獨立AA r03首Tap完整raw通過、producer／publication超期40／40而原gate STOP，noise／ABBA未跑。詳見[實際結果与完整保存索引](MAIN57_DIAGNOSTIC_AND_AA_STATUS_20261011.md)。以下main56 STOP及診斷proposed為原保存記錄。

main56／`78b9186` configure Native0；三TU build Native2，compiler C1083：新 controls target 未加入 pipeline include 目錄而找不到 `poll_gap_diagnostic.hpp`。main與原transport編譯完成，控制／測量皆NOT_RUN；guard identity／held／streams／active0正常。原source snapshot、compiler stdout及負收據保留於 [編譯負結果](MAIN56_DIAGNOSTIC_COMPONENT_COMPILE_STOP_20261011.json)。修補只補該target的include目錄，以新獨立attempt取得實際收據，不覆蓋原件、不重跑75／production9。

main55 source `12ec0550e882e3b06751d67fa67c43dda3e9312b` 的必要 CPU32／原584、wait43／diagnostic20／active12／QoS24、reader2／原81／active-policy12／QoS-policy12、production52／七控制通過。Normal Tap 的 producer／auditor Native 均0，完整 raw11／3,358,832 bytes integrity通過；pool drops0，producer／callback deadline overruns7，成本STOP。Hold／Dense／AA／noise／ABBA未跑。完整原負結果見 [main55記錄](MAIN55_PRODUCTION_TAP_NEGATIVE_20261011.json)，原source、raw、STOP及Native收據保留。

252 warmup attempt 的 active wait跨168.9509 ms／93059 polls，lateness149.1784 ms。後253–258沿原absolute due追趕，構成另外六次超時；render／callback分別只有 .1733／.0991 ms。1973另有35.5134 ms等待，但未超過原20 ms超時門檻。正常18–22 ms等待的poll p50為301224，正常樣本亦存在thread CPU accounting零差；計數的粒度未知，不能從零差認定精確off-CPU，或把OS、guard、core／frequency當已知原因。

下一步只建立獨立diagnostic component，重用逐檔SHA核對的main55 Release libs、generated/manual provenance及runtime來源；新編 production main、原grpc_transport及新poll-gap控制，共三個compile occurrences。原main55 build不回寫，不使用新component替代Normal3／AA／noise／ABBA或admission所需current資格。base55與compiled diagnostic delta source、binary、輸入及Native收據分列；所有whole、cost、physical及live資格false，device0。

診斷只在既有cold producer診斷分支包裝原Clock：每次仍讀原Clock一次，保存固定scalar的最大相鄰QPC間隔、兩端、序號及sample數，沒有逐poll持久化、分配或額外OS API。原wait的negative／backward／throw及due／cadence／poll cap行為保留；同值max gap保留最早間隔。首次讀取加原2^24 polls對應sample cap2^24+1及interval cap2^24。原raw行的16欄與其數值保留，新欄與schema明列；原1536-byte row／4MiB file界限不提高。wrapper的額外轉呼叫及scalar更新成本尚未量測，結果只屬診斷。

最大gap可確認是否出現支配性的相鄰讀取空窗；小max gap不能證明所有間隔均勻，任何gap亦不能獨立判定OS根因。根據新證據選擇修補後，才完成必要整合、Normal3、AA12／noise18、未跑ABBA36及完整readonly。原main32的75／production9 PASS保持，不全盤重跑。latest completed noise仍main44的6／18 adequate，ABBA0／36。本輪不啟動emulator、裝置或新thread，成功至emulator邊界停止。
