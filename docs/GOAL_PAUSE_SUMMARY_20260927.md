# 暫停紀錄：最佳三次嘗試與技術發現

2026-09-27 使用者要求暫停；goal 狀態已為 **paused**。不再自動開發、啟動遊戲或安排下一輪。最後一輪是第十八輪，已 STOPPED／exit0；目前沒有遊玩程序。HD 尚未 AP，IN 未開始，同版本連續三輪 AP 的穩定門檻未達成。

## 最佳三次完整嘗試

以 **遊戲結算分數由高至低** 排序，僅納入完整、未排除的 Glaciaxion HD Lv.6 測試；第二輪啟動晚於重試、第十三輪曲尾提前停止，排除比較。下表各輪均為393個結算判定、185秒 session、五指與35ms lead。

| 名次 | 輪次 | 分數 | Perfect／Good／Bad／Miss | Max Combo | 程式版本 |
|---|---|---:|---|---:|---|
| 1 | 第九輪 | **868,880** | **369／2／0／22** | 82 | source `1636519`，observer33／planner13 |
| 2 | 第七輪 | **862,583** | **359／7／0／27** | 118 | source `5f1c710`，observer31／planner12 |
| 3 | 第六輪 | **852,341** | **365／2／0／26** | 53 | source `5f1c710`，observer31／planner12 |

第六、七輪是同一 binary 的兩次測試，仍非AP；第九輪屬另一版本。這三次不能組成同版本穩定性驗收，也不能推論新版一定更好。若改按最低 Miss 排序，則為第九輪22、第六輪26、第十八輪26；26 Miss同分時，第六輪的結算分數較高。

| 輪次 | 辨識時間 n；p50／p95／p99／max（ms） | 被處理的遊玩畫面間隔 n；p50／p95／p99／max（ms） |
|---|---|---|
| 九 | 10811；4.5795／7.2929／8.7228／18.5836 | 9627；16.6027／32.05132／44.62404／76.1829 |
| 七 | 10278；5.08525／7.50676／9.445496／16.8118 | 9103；16.8445／35.77646／48.321038／90.1227 |
| 六 | 10640；4.6658／6.57259／7.87322／13.6668 | 9450；16.67975／33.32927／46.549711／90.4836 |

共同設定為Windows x64／C++20、gRPC payload fast-memcpy／256KiB、1280×720 RGB888 top-down／rotation1，觸控720×1280／rotation90，已驗證五個獨立接觸點。host QPC作共同時間域；畫面間隔統計來自已處理的playing decisions，包含跨epoch的真實間隔，不是全部capture callback的分布。不同輪的負載條件未完全相同，不能把分數差歸因於單一改動。

## 已確認的技術發現

1. **Hold 的可見外框與實際觸點需要分開追蹤。** 顏色或candidate ID改變不代表音符結束。唯一、當前雙側rails與body支持可維持同一手指；無效別名不能取消仍有支持的原contact。第七輪740–760的Hold前端確實往上離開判定線，舊方法固定在line附近，導致missing取消。moving front及body patch已實作，觸點使用當前可見body內部；灰色外框不能任意建立新Down。

2. **Hold 提早放開不只是一個顏色問題。** 第十輪查到pending Down改早時把舊Up一併平移，使最新frame只剩約40.72ms的Hold期限。planner15改成最新evidence+100ms；當前可見尾端連續兩次過線、至少10ms後才鎖定20ms正常release。尾端未知仍不能當已完成。source／target100ms、Hold missing60ms、anchor90ms保持；全曲提前Up仍未全部解決。

3. **Drag 應維持與移動接觸，而非只依賴離散點擊。** 連續黃鍵可在當前相容區域共享唯一活動手指；離開安全內部時用同contact ID送Move。planner16另加當前可靠line與黃鍵core的空間重疊Down，不必偽造線性crossing。沒有當前支持或身分競爭時仍按原40ms missing／100ms evidence釋放。此語義有合成與live執行證據，但Drag全曲不漏仍未驗收。

4. **歪斜與移動線要採局部座標及獨立line身分。** observer35用最多6點／90ms／10ms bucket的實測線方程估法向速度與角速度，只作下一幀關聯初值；目前幾何不平滑、不外推成虛擬觀測。當前分布式ridge及片段支持提升斜線可靠性；斜Drag採component實際像素投影邊界。PCA variance假定均勻填滿曾把U形高亮寬度從148膨脹到178.47px，造成既有回歸失敗，已改實測extrema並保留近水平去重語義。近似line ID連續性改善不等於遊戲Miss修復。

5. **固定像素殘差不等於固定時間誤差。** 第十五輪兩個Tap有pending計畫，約9px殘差超過舊8px門檻，取消後直到過線都沒有Down。observer36改用有界空間誤差及誤差／相對速度換算的時間不確定性，owner原30ms保持；最新點偏離fit也須受限。合成測試通過，但第十六輪28 Miss沒有改善，所以沒有實戰收益證明。

6. **取消未執行的計畫，不能永久封鎖返回的有效像素。** 第十六輪Tap1082在6496被missing取消，executed_steps=0，6501已有有效新預測卻因submitted狀態無法重建。planner18只對已知cursor=0解除提交標記，之後由新當前證據建立新intent；已Down、完成或未知注入結果不能重播。第十七輪3次、第十八輪2次重建均實際成功Down；這證明生命週期修正執行，不代表救回5個遊戲Miss。

7. **畫面間隔與尾端過期是重要變因，原因尚未確定。** 完全相同binary的第十七／十八輪，Miss56→26，playing間隔p99 87.536522→59.803561ms，source過期撤銷48→9。第十七輪published6503／consumed6490、pool drops0、host residency p99約17.35ms，不支持辨識端大量積壓的解釋；但沒有其他應用CPU、完整來源callback或實際前景證據，不能確定為負載、渲染或傳輸原因。不能靠延長證據期限掩蓋。

8. **單元回歸、診斷候選與真實遊戲判定必須分開。** 最新Release／Debug／嚴格ASan各190／190，只表示測試通過。取消數、未處理近線candidate、ID變化、Down成功都不是逐音符Perfect／Miss真值。光流及模型未接入；現有ROI未完成獨立人工標註，不能宣稱模型資料就緒。

## 最後狀態與證據

第十八輪完整結算：**348／19／0／26、839,224分**，maxcombo55、ACC91.69%、Early2／Late17，未AP。與第十七輪同一source `609600f`／observer36／planner18，binary `d7ce474576a0283711b046b82720f9c10e8de0bb92203eb999a6c8decc157162`；未自動還原成最佳第九輪。最新三配置各190／190；本次暂停整理沒有新建置或測試。

資料集已保留20個run，第十八輪136原生ROI、partial=false／failure空，達到既定20run上限。沒有刪除證據、改root繞過額度、上傳資料或建立自動化。Computer Use第十八輪截到遮擋視窗，故以停止後gRPC原生结算核對；實際emulator前景未證明。後續任何測試需使用者明確恢復goal，舊的PLAY準備／負載問題不會自行觸發測試。

| 輪次 | 原始run與SHA256 |
|---|---|
| 九 | `cpp-observe-17905107612608769`；`fad0a66fad967173b1dd91650208391a46d40ce5d312684e9b99c958fc45a5cf` |
| 七 | `cpp-observe-17905082616018716`；`5090389ba59b8e6d1ed6211347daaa005d545fba93550a92f0e7d63c483263e3` |
| 六 | `cpp-observe-17905079704324840`；`796399baa4104e6b4142aa1cc4d14c09e70bdf95bee65d23c1bc5b2a3a74535c` |
| 十八 | `cpp-observe-17905210508063699`；`bdf80a15785b58ce1579655deeb588df32c1f4acd4ddac64cf1cbb537e4c0b46` |

三個最佳run的raw hash本次重新核對一致；manifest版本亦已核對。原始JSONL／manifest／summary位於 `measurements/game-assist/<run-id>/`，保留於本機ignored目錄。詳細修正、失敗與分布見[完整開發紀錄](OUTLINE_CONTACT_TRACKING_PLAN_20260927.md)，結構化證據見[索引](OUTLINE_CONTACT_EVIDENCE_20260927.json)。
