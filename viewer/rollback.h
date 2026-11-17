// 107б (генплан Fable 13.11, п.7): один файл rollback.txt рядом с EBOOT возвращает старый путь любой правки 107 без пересборки. Строка = имя выключателя (как в коде), пустые строки и «#…» пропускаются.
// Игра читает файл при старте (rollbackLoad, main_loop.h), для каждого названного пишет в журнал/ящик «ROLLBACK имя applied …» (неизвестное имя — «ROLLBACK имя unknown»), ничего не названо — ничего не меняется.
// Имена (все выключатели 107, поставленные против 106; Т = переключается файлом, К = нужна пересборка):
//   Т BOOTHSND1266_OLD BRIEFPIC1269_OLD BUBBLE1264A_OLD BUBBLE1264B_OLD BUBBLE1264C_OLD BUBBLE1264D_OLD BUBBLE1264E_OLD DAL1272_OLD DAL72_CEIL_OLD DALBELT_OLD GAME_A12_DUMPDMG_OLD
//   Т MC_ENDLEAVE_OLD MC_PARKED_INVULN_OLD MC_REPLAYPARK_OLD MC_SPAWNEVICT_OLD MC_STAGEEND_OLD MCRFIX1265_OLD MOVIETV1271_OLD MOVIETV1271B_OLD MSN_BARRELLOST_OLD MSN_CARREACH_OLD MSN_COSM1264_OLD
//   Т MSN_RESETCOUNT_OLD MSN_RESETPT1264_OLD MSN_SPAWNCLEAR_OLD MSN_TRAFFICHOLD_OLD MSNARTIO108_OLD MSNARTIO107B_OLD MVGETINARROW_OLD RACEFENCE1264_OLD STREAMHALF_OLD TRF_RACEHOLD1264_OLD
//   Т P0 (сон): SBHEAL107B_OLD MEWAKE107B_OLD SHARME_STALE107B_OLD (модуль — нужен sharme_old.prx рядом)
//   Т 1319: ZONENEAR1319_OLD, ZONENEARCELL_OLD, ZONENEARMASK1354_OLD
//   Т 1362: MCRPREPOLL_OLD
//   Т 1320: ROOMRELOAD1320_OLD
//   Т 1345: TRF_REJOIN1345_OLD
//   Т 1359: WCARPOL1359_OLD, STAGEENDNAME1359_OLD, PARKDMGNAME1359_OLD, MSNDONEPOL1359_OLD (зонтик HNRPOLNAME1350_OLD включает пп. 1–3)
//   Т 1356: HNRBUSTNAME1356_OLD
//   Т 1350: HNRPOLNAME1350_OLD
//   Т 1337: MSNIOPRIO1337_OLD, MSNBRIEFBB1337_OLD
//   Т 1333: CARSTACK1333_OLD
//   Т 1330: SV_FLIPEMA1330_OLD, SV_LEANREST1330_OLD
//   Т 1323: WEEBLE1323_OLD
//   Т 1325: MC_AIRESET1325_OLD, MC_AIRESET1325B_OLD
//   Т 1312: NAVARROW1312_OLD
//   Т слой 108: COLLPREP108_OLD COLRAYK1_OLD FXFIND108_OLD MISSGRID_OLD MSAFIND108_OLD P1PAGES_OLD PBVER108_OLD PROPHOT108_OLD SKIDDENSE108_OLD SLSCAN108_OLD SPIDLE108_OLD
//   Т 107в (Бабл): COLLCANCEL107_OLD PROPVOL107_OLD PROPGHOST107_OLD PROPLOSS107_OLD PROPSTOMP107_OLD PROPBOOM107_OLD CARDMGPROP107_OLD POLE1276_OLD SPOTHER107_OLD SPBUSH1284_OLD SPZONE1284_OLD PWR107_OLD PWRCOLL107_OLD COLLHEAL107_OLD QUAR107_OLD RIDE1274_OLD RIDEINH1274_OLD
//   Т 109 (запекание): PBIDX109_OLD LATBAKE_OLD SKYBAKE_OLD
//   Т 1298: MSNARTCULL1298_OLD MSNARTGROUP1298_OLD
//   Т 1361: NPD1361_OLD
//   Т 1363: MSNARTSHARE1363_OLD
//   Э 1328 (экспорт данных, не движок; файлом не переключается): VEND1328_OLD=1 в окружении tools/prp2_export.py — автоматы L2 (первый из двух блоков машины состояний) и L3 (вид по имени) как до 1328; нужен перевыпуск level.shr L2 и L3
//   (STREAMCHUNK_OLD: по умолчанию уже прежний, строка ничего не меняет)
//   (BOOT_BUILDNUM1270_OLD — теперь тоже Т: число сборки на загрузочном экране; CAR_FENCEANG1279_OLD — когда car107b будет в своде)
// Файлы «streamhalfold», «dal1272old» и прочие *old рядом с EBOOT работают как раньше; rollback.txt — общий список поверх них.
#ifndef ROLLBACK_H
#define ROLLBACK_H
extern int collChain1364Old;   // 1364: coll_stream.c, цепочка фоновых заданий гэгов
extern int stallGrace1320Old;   // 1320б: запас сторожа на чистой перезагрузке (mem1060.h)
extern int mvNpd1361;   // 1361 (msn_view.c)
extern int msnartShare1363;   // 1363 (msnart.c)
extern int roomReload1320Old;   // 1320: чистая перезагрузка при входе в помещение в миссии (main_rooms.h)
extern int hnrStagePause1360Old;   // 1360: см. main_frontend.h
extern int zoneNear1319Old, zoneNearCellOld, zoneNearMask1354Old;   // 1319: регионы клеток вокруг игрока в наборе ворот (main_rooms.h)
extern int mcrPrePollOld;   // 1362: main_render.h
extern int zoneNear1319Old, zoneNearCellOld, zoneNearMask1354Old, zoneNearTight1360Old;   // 1319: регионы клеток вокруг игрока в наборе ворот (main_rooms.h)
extern int trfNavArrow1312Old;   // 1312: стрелки пути красятся по пути за стрелкой (traffic.c)
extern int vehDestroyed1315Old;   // 1315 К7: строки VEHDESTROYED (main_loop.h)
extern int shopSnd1329Old;   // 1329: shop_view.c
extern int homerOwnRoof1332Old, homerDecks1332Old;   // 1332: homer.c
extern int svDlgPad1339Old;   // 1339: savemenu.c
extern int camWall1316Old, vehDestroyed1315Old;   // 1315 К7: строки VEHDESTROYED (main_loop.h)
extern int mcChaseCurve1309Old, mcChaseHead1309Old;   // 1309: погоня по дорогам (msncar.c)
extern int mcChaseCurve1309Old, mcChaseHead1309Old, mcSlipDecay1309Old;   // 1309: погоня по дорогам (msncar.c)
extern int mvFadeDir1349Old;
extern int loadWait1362Old; extern int mvFadeClk1362bOld;
extern int mvWorldHold1326Old, mvSkipCount1331Old, mvMcrWait1337Old;
extern int cfxDefault1340Old, cardGlint1342Old;
extern int msnLockIcon1321Old, msnTimerFloor1324Old, msnTimerHold1324Old;
extern int msnLockHint1327Old;
extern int msnartChain1359Old, inputHints1357Old, mvPauseBetween1348Old, mvBubble1264Old, uiBub1264Old, msnCosm1264Old, msnResetPt1264Old, msnartIo108Old, msnartIo107bOld, mvRaceFence1264Old, trfRaceHold1264Old;
extern int rb_BOOTHSND1266_OLD, rb_BRIEFPIC1269_OLD, rb_DAL72_CEIL_OLD, rb_GAME_A12_DUMPDMG_OLD, rb_MC_ENDLEAVE_OLD, rb_MC_PARKED_INVULN_OLD, rb_MC_REPLAYPARK_OLD, rb_MC_STAGEEND_OLD,
  rb_MSN_RESETCOUNT_OLD, rb_MSN_SPAWNCLEAR_OLD, rb_MSN_TRAFFICHOLD_OLD, rb_MC_SPAWNEVICT_OLD, rb_MSN_BARRELLOST_OLD, rb_MSN_CARREACH_OLD, rb_MVGETINARROW_OLD;
// слой 108 (одна сборка, слово Роба): выключатели Бабла, Стены, Дали
// 107б: сон, Бабл, Стена, Гомер (13 имён по замечанию Fable)
extern int homerRide107bOld, homerSlope107bOld, homerSp107bOld, propsGhost107Old, propsLoss107Old, msnartPwr107Old, collPwr107Old, collHeal107Old, msnartQuar107Old, aoRide1274Old, aoRideInh107Old, spOther107Old, carDbgDiag108, trfPedObs108Old;
// сон Дали, Бабл П6/П10/П11/П12, Гомер П9, Стена: уворот, точный уворот
extern int homerFar1367Old;
extern int rb_BOOT_BUILDNUM1270_OLD, rb_SPINVIS1248_OLD, spInvis1248OldV, rb_TRFSMOOTH1092_OLD, trfSmooth1092OldV, rb_TRFDRAW108_OLD, trfDraw108OldV, trfDodgeOffV, rb_MC_RIVAL_ROAD108_OLD, mcRivalRoad108OldV, homerWalk1284Old, npcP0;
extern int carDmgProp107Old, homerP9107Old, propsPoleKick1276Old, propsBoom107Old, propsStomp107Old, trfDodgeExact108Old, rb_TRF_SV_DODGE_OLD;
extern int collPrep108Old, colRayK1Old, fxFind108Old, pbVer108Old, propsHot108Old, skidDense108Old, spIdle108Old, msaFind108Old, rb_P1PAGES_OLD;
extern int mvLoadWait109Old, mvPhaseCars109Old, gameHnrStage109Old;
extern int mvProxBar1318Old, mvFinLine1317Old, mvBubVis1310Old, mvBubCam1315Old, mvLoadQ1303Old, mvChr109Old, mvReady109Old, mvList109Old, msnartEnd109Old, collCancelOld107, propsVol107Old, propsGhost107Old, propsLoss107Old, propsStomp107Old, propsBoom107Old, carDmgProp107Old, propsPoleKick1276Old, spOther107Old, spBush1284Old, spZone1284Old, msnartPwr107Old, collPwr107Old, collHeal107Old, msnartQuar107Old, aoRide1274Old, aoRideInh107Old;   // 107в: физика и сон (Бабл)
extern int mvRsPend1304Old, mvFadeGuard1316Old;   // 1304 «yes» на экране провала, 1316 затемнение/отметка перезапуска/сторож
extern int mvReady109Old, mvList109Old, msnartEnd109Old, collCancelOld107, propsVol107Old, propsGhost107Old, propsLoss107Old, propsStomp107Old, propsBoom107Old, carDmgProp107Old, propsPoleKick1276Old, spOther107Old, spBush1284Old, spZone1284Old, msnartPwr107Old, collPwr107Old, collHeal107Old, msnartQuar107Old, aoRide1274Old, aoRideInh107Old;   // 107в: физика и сон (Бабл)
// сборка с -DИМЯ_OLD=1 (старые гейты, красный контроль): переменная rb_ИМЯ не определяется (имя — константа), ссылка слабая, запись пропускается
#pragma weak rb_BOOTHSND1266_OLD
#pragma weak rb_BOOT_BUILDNUM1270_OLD
#pragma weak rb_BRIEFPIC1269_OLD
#pragma weak rb_DAL72_CEIL_OLD
#pragma weak rb_GAME_A12_DUMPDMG_OLD
#pragma weak rb_HOMERHIT109_OLD
#pragma weak rb_MC_ENDLEAVE_OLD
#pragma weak rb_MC_PARKED_INVULN_OLD
#pragma weak rb_MC_REPLAYPARK_OLD
#pragma weak rb_MC_RIVAL_ROAD108_OLD
#pragma weak rb_MC_SPAWNEVICT_OLD
#pragma weak rb_MC_STAGEEND_OLD
#pragma weak rb_MSN_BARRELLOST_OLD
#pragma weak rb_MSN_CARREACH_OLD
#pragma weak rb_MSN_RESETCOUNT_OLD
#pragma weak rb_MSN_SPAWNCLEAR_OLD
#pragma weak rb_MSN_TRAFFICHOLD_OLD
#pragma weak rb_MVGETINARROW_OLD
#pragma weak rb_P1PAGES_OLD
#pragma weak rb_SPINVIS1248_OLD
#pragma weak rb_TRFDRAW108_OLD
#pragma weak rb_TRFSMOOTH1092_OLD
#pragma weak rb_TRF_SV_DODGE_OLD

extern int rb_HOMERHIT109_OLD;
extern int lbDuckAll1294Old, msnBonusTwin1296Old, npcScene1293On, mvRsPend1304Old;
extern int msnartCap109Old;   // 109 (Бабл): мест под записи msnart 32
typedef struct { const char *name; int *var; int val, orMask; const char *what; } RbEnt;   // val — что записать (orMask: добавить биты), what — слова журнала
static const RbEnt rbTab[] = {
  {"BOOTHSND1266_OLD", &rb_BOOTHSND1266_OLD, 1, 0, "щелчки будки на шине эффектов"},
  {"BRIEFPIC1269_OLD", &rb_BRIEFPIC1269_OLD, 1, 0, "картинка карточки ждёт 8 с и 3 повтора"},
  {"BUBBLE1264A_OLD", &mvBubble1264Old, 1, 1, "этап без текста гасит бабл"},
  {"BUBBLE1264B_OLD", &mvBubble1264Old, 2, 1, "ожидание значка бабла 3 с"},
  {"BUBBLE1264C_OLD", &mvBubble1264Old, 4, 1, "cleanup не снимает ждущий текст"},
  {"BUBBLE1264D_OLD", &uiBub1264Old, 8, 1, "бабл С4 как до 1264"},
  {"BUBBLE1264E_OLD", &uiBub1264Old, 16, 1, "бабл С5 как до 1264"},
  {"DAL1272_OLD", &dal72On, 0, 0, "дальность и стрим как в 106 (Даль 1272 выключена)"},
  {"DAL72_CEIL_OLD", &rb_DAL72_CEIL_OLD, 1, 0, "потолок регулятора без 200 м"},
  {"DALBELT_OLD", &beltOn, 0, 0, "шар пояса 150–200 м выключен"},
  {"GAME_A12_DUMPDMG_OLD", &rb_GAME_A12_DUMPDMG_OLD, 1, 0, "неуязвимость цели dump как в 106"},
  {"MC_ENDLEAVE_OLD", &rb_MC_ENDLEAVE_OLD, 1, 0, "машины миссии уходят сразу после конца"},
  {"MC_PARKED_INVULN_OLD", &rb_MC_PARKED_INVULN_OLD, 1, 0, "стоящие машины миссии уязвимы"},
  {"MC_REPLAYPARK_OLD", &rb_MC_REPLAYPARK_OLD, 1, 0, "повтор команд ставит машину с прежним режимом"},
  {"MC_SPAWNEVICT_OLD", &rb_MC_SPAWNEVICT_OLD, 1, 0, "новая машина миссии не вытесняет уходящую"},
  {"MC_STAGEEND_OLD", &rb_MC_STAGEEND_OLD, 1, 0, "машины этапа не уходят по концу этапа"},
  {"MCRFIX1265_OLD", &mcrFix1265, 0, 0, "машина этапа без модели невидима"},
  {"MOVIETV1271_OLD", &movieTv1271, 0, 0, "ролик гэга/билета без новой рамки"},
  {"MOVIETV1271B_OLD", &movieTv1271b, 0, 0, "рамка роликов открывается одним заходом"},
  {"MSN_BARRELLOST_OLD", &rb_MSN_BARRELLOST_OLD, 1, 0, "бочка разбита не сообщается"},
  {"MSN_CARREACH_OLD", &rb_MSN_CARREACH_OLD, 1, 0, "машина входит в объём центром"},
  {"MSN_COSM1264_OLD", &msnCosm1264Old, 7, 1, "A1.8/A1.11/A1.12 как до 1264"},
  {"MSN_RESETCOUNT_OLD", &rb_MSN_RESETCOUNT_OLD, 1, 0, "RESET CAR на отсчёте работает"},
  {"MCRWAIT1337_OLD", &mvMcrWait1337Old, 1, 0, "1337: этап с целью-машиной без модели идёт сразу, метка на пустом месте, как до 1337"},
  {"SKIPCOUNT1331_OLD", &mvSkipCount1331Old, 1, 0, "1331: Yes на экране провала и Restart паузы считают попытку ещё раз сверх карточки (Skip на 4-м провале), как до 1331"},
  {"FADECLK1362B_OLD", &mvFadeClk1362bOld, 1, 0, "1362Б: часы затемнения под карточкой и паузой идут по игровому dt (= 0), как до 1362Б"},
  {"LOADWAIT1362_OLD", &loadWait1362Old, 1, 0, "1362: газета не ждёт клетки вокруг старта дольше потолка от начала показа (клетки собираются после раскрытия) и не дочитывает машины всей миссии"},
  {"FADEDIR1349_OLD", &mvFadeDir1349Old, 1, 0, "1349: выход из чёрного при перезапуске отменяется в кадре с dt = 0 (чёрный до 5 с), как до 1349"},
  {"WORLDHOLD1326_OLD", &mvWorldHold1326Old, 1, 0, "1326: после ✕ карточки интерфейс виден и игра идёт под чёрным слоем или пустым миром, как до 1326"},
  {"TIMERFLOOR1324_OLD", &msnTimerFloor1324Old, 1, 0, "1324: число на таймере обратного отсчёта округляется вверх, как до 1324"},
  {"TIMERHOLD1324_OLD", &msnTimerHold1324Old, 1, 0, "1324: отсчёт и таймер этапа идут, не дожидаясь готового мира под игроком, как до 1324"},
  {"MSN_LOCKICON1321_OLD", &msnLockIcon1321Old, 1, 0, "1321: после запертого этапа значок следующего этапа не обновляется (остаётся прежний), как до 1321"},
  {"LOCKHINT1327_OLD", &msnLockHint1327Old, 1, 0, "1327: после запертого этапа (нет нужной машины или костюма) нет окна подсказки с Continue «что купить, где и за сколько», только фанфара, как до 1327"},
  {"CFX_DEFAULT1340_OLD", &cfxDefault1340Old, 1, 0, "1340: предмет миссии без SetCollectibleEffect (доставка, сброс) при подборе без блеска mission_col, как до 1340"},
  {"CARDGLINT1342_OLD", &cardGlint1342Old, 1, 0, "1342: у карточки в мире нет фиолетового блика-звёзд как у ключа и вспышки при подборе, только белое пятно, как до 1342"},
  {"MSN_RESETPT1264_OLD", &msnResetPt1264Old, 1, 0, "точка перезапуска как до 1264"},
  {"MSN_SPAWNCLEAR_OLD", &rb_MSN_SPAWNCLEAR_OLD, 1, 0, "старт миссии без очистки места"},
  {"MSN_TRAFFICHOLD_OLD", &rb_MSN_TRAFFICHOLD_OLD, 1, 0, "трафик миссии как был"},
  {"MSNARTIO108_OLD", &msnartIo108Old, 1, 0, "значок задания без переоткрытия"},
  {"MSNARTIO107B_OLD", &msnartIo107bOld, 1, 0, "картинки карточки без переоткрытия, afd −1 молчит"},
  {"MVGETINARROW_OLD", &rb_MVGETINARROW_OLD, 1, 0, "стрелка getin выключена"},
  {"RACEFENCE1264_OLD", &mvRaceFence1264Old, 1, 0, "заборы гонки как до 1264"},
  {"RSPEND1304_OLD", &mvRsPend1304Old, 1, 0, "1304: «yes» на экране провала как было (затемнение обрывается, перезапуск ждёт чёрного)"},
  {"FADEGUARD1316_OLD", &mvFadeGuard1316Old, 1, 0, "1316: затемнение игровым dt, отметка перезапуска без привязки к миссии, без сторожа fade-stuck"},
  {"STREAMCHUNK_OLD", &streamChunkOn, 0, 0, "кусками стрим (уже выключен по умолчанию)"},
  {"STREAMHALF_OLD", &streamHalfOn, 0, 0, "stream() каждый кадр"},
  {"TRF_RACEHOLD1264_OLD", &trfRaceHold1264Old, 1, 0, "признак гонки у трафика как до 1264"},
  {"SPINVIS1248_OLD", &rb_SPINVIS1248_OLD, 1, 0, "невидимый стейтпроп твёрдый (закон 1284, сборка 97); умолчание 0 — правило №1248е"},
  {"SPINVIS1248_OLD", &spInvis1248OldV, 1, 0, "(зеркало CFG108)"},
  {"TRFSMOOTH1092_OLD", &rb_TRFSMOOTH1092_OLD, 0, 0, "1284: сглаживание поворотов трафика №1092 возвращено"},
  {"TRFSMOOTH1092_OLD", &trfSmooth1092OldV, 0, 0, "(зеркало CFG108)"},
  {"ROOMREADY1344_OLD", &roomReady1344Old, 1, 0, "1344: комната под закрытым экраном ждёт персонажей не больше 1,5 с (Дедушка встаёт на глазах), как до 1344"},
  {"STALLGRACE1320_OLD", &stallGrace1320Old, 1, 0, "1320б: сторож на чистой перезагрузке уровня ждёт 5 с, а не 25 (STALL2 на штатной 14-секундной загрузке), как было"},
  {"ROOMRELOAD1320_OLD", &roomReload1320Old, 1, 0, "1320: вход в помещение в миссии при отказе памяти без чистой перезагрузки (PLEASE WAIT), как было"},
  {"ZONENEAR1319_OLD", &zoneNear1319Old, 1, 0, "1319: регионы клеток города в 40 м от игрока не добавляются в набор ворот (после переноса города вокруг нет)"},
  {"ZONENEARCELL_OLD", &zoneNearCellOld, 1, 0, "1319б: регион клеток рядом целиком в наборе ворот (стрим грузит и считает дырами его дальние клетки); по умолчанию только клетки ближе 80/50 м"},
  {"ZONENEARTIGHT1360_OLD", &zoneNearTight1360Old, 1, 0, "1360: при тесной куче регион «рядом» берёт регионы клеток ближе 50 м (дыры), а не пустое пересечение с прежним набором — «нет текстур города после миссии/переноса» не возвращается на PSP; прежнее: при memTight новых регионов не берёт"},
  {"HNRSTAGEPAUSE1360_OLD", &hnrStagePause1360Old, 1, 0, "1360: шкала Hit & Run и полиция не сбрасываются на смене этапа внутри миссии (окно между этапами остаётся «в миссии»), только на старте и победе миссии — прежнее: каждый конец этапа читался как победа, начало как новый старт"},
  {"ZONENEARMASK1354_OLD", &zoneNearMask1354Old, 1, 0, "1354: регион «рядом» берётся только из маски REG1 клетки игрока (без «декораций издали» чужих районов, L4 F10-4: листва в кадре в полёте)"},
  {"MCRPREPOLL_OLD", &mcrPrePollOld, 1, 0, "1362: итог предчтения модели машины опрашивается дважды, готовое предчтение выбрасывается и машина читается синхронно в кадре (47–65 мс)"},
  {"NAVARROW1312_OLD", &trfNavArrow1312Old, 1, 0, "1312: стрелки пути красятся по расстоянию от центра перекрёстка, как в 106"},
  {"MC_CHASECURVE1309_OLD", &mcChaseCurve1309Old, 1, 0, "1309: погоня по дорогам без торможения по кривизне маршрута"},
  {"MC_SLIPDECAY1309_OLD", &mcSlipDecay1309Old, 1, 0, "1309: потеря управления не убывает в паузе после тарана (после тарана стоящей машины ИИ стоит минутами)"},
  {"MC_CHASEHEAD1309_OLD", &mcChaseHead1309Old, 1, 0, "1309: погоня без торможения при носе, отвёрнутом от точки руля"},
  {"TRFDRAW108_OLD", &rb_TRFDRAW108_OLD, 1, 0, "1284: трафик рисуется до 125 м, как в 106"},
  {"TRFDRAW108_OLD", &trfDraw108OldV, 1, 0, "(зеркало CFG108)"},
  {"MC_RIVAL_ROAD108_OLD", &rb_MC_RIVAL_ROAD108_OLD, 1, 0, "1284: Скиннер L1 m1 только по дороге"},
  {"MC_RIVAL_ROAD108_OLD", &mcRivalRoad108OldV, 1, 0, "(зеркало CFG108)"},
  {"HOMERWALK1284_OLD", &homerWalk1284Old, 1, 0, "1284: посадка пешком и автосброс как до 1284"},
  {"HOMERFAR1367_OLD", &homerFar1367Old, 1, 0, "1367: посадка издалека без ириса для долгого обхода (>4,5 м) и упора (0,6 с) у всех героев, пешком до двери, как после 1284"},
  {"P0NPC_OLD", &npcP0, 0, 0, "P0: персонажи миссии не лечатся после сна"},
  {"P0ROOM_OLD", &p0RoomOn, 0, 0, "P0: комнаты после сна не лечатся (rgBad навсегда)"},
  {"WSP0_OLD", &wsP0, 0, 0, "P0: спрайты ключей и будки после сна не лечатся"},
  {"BOOT_BUILDNUM1270_OLD", &rb_BOOT_BUILDNUM1270_OLD, 1, 0, "№1270: на загрузочном экране номер сборки 102"},
  {"BELTFAR_OLD", &beltFarOn, 0, 0, "шар пояса: дальняя версия клеток выключена"},
  {"CARDMGPROP107_OLD", &carDmgProp107Old, 1, 0, "П10: урон машине от удара о предмет как до 107б"},
  {"HOMERP9107_OLD", &homerP9107Old, 1, 0, "П9: отреагировавший стейтпроп проходим для Гомера — как до 107б"},
  {"P0COLRING_OLD", &colRingP0, 0, 0, "P0: colring.shr после сна не переоткрывается"},
  {"P0LEVEL_OLD", &loaderP0, 0, 0, "P0: файл уровня после сна не лечится (предел 16 переоткрытий)"},
  {"POLE1276_OLD", &propsPoleKick1276Old, 1, 0, "П6: столб валится с толчком бампера как до 1276б"},
  {"PROPBOOM107_OLD", &propsBoom107Old, 1, 0, "П11: взрыв бьёт всех предметов рядом, без предела 20"},
  {"PROPSTOMP107_OLD", &propsStomp107Old, 1, 0, "П12: стомп как до 107б"},
  {"RINGFIX1272_OLD", &ringFixOn, 0, 0, "Даль: кольцо роста не ограничено радиусом загрузки"},
  {"TRF_DODGE_EXACT108_OLD", &trfDodgeExact108Old, 1, 0, "Т1: уворот трафика не по правилу оригинала (точный)"},
  {"TRF_SV_DODGE_OLD", &rb_TRF_SV_DODGE_OLD, 1, 0, "уворот трафика выключен, как в 105"},
  {"TRF_SV_DODGE_OLD", &trfDodgeOffV, 1, 0, "(зеркало CFG108)"},
  {"COLLHEAL107_OLD", &collHeal107Old, 1, 0, "P0: дескрипторы гэгов после сна не лечатся"},
  {"HOMERRIDE107B_OLD", &homerRide107bOld, 1, 0, "Г4/Г5: перенос Гомера крышей машины как до 107б"},
  {"HOMERSLOPE107B_OLD", &homerSlope107bOld, 1, 0, "В1: предел наклона 30° на всём, в том числе на рельефе"},
  {"HOMERSP107B_OLD", &homerSp107bOld, 1, 0, "невидимое пешком: объём стейтпропа как до 107б"},
  {"MCRBAD108_OLD", &mcrBad108Old, 1, 0, "P0: машина миссии без модели — пометка «битая» навсегда"},
  {"PROPGHOST107_OLD", &propsGhost107Old, 1, 0, "П1/П2: лежащий обычный предмет — призрак для машин"},
  {"PROPLOSS107_OLD", &propsLoss107Old, 1, 0, "потеря скорости на предмете как до 107б"},
  {"PWR107_OLD", &msnartPwr107Old, 1, 0, "P0: msnart и гэги не сверяются с эпохой питания"},
  {"PWR107_OLD", &collPwr107Old, 1, 0, "P0: msnart и гэги не сверяются с эпохой питания (гэги)"},
  {"QUAR107_OLD", &msnartQuar107Old, 1, 0, "P0: брошенный буфер чтения msnart без карантина"},
  {"RIDE1274_OLD", &aoRide1274Old, 1, 0, "1274: Гомер не едет с платформой"},
  {"RIDEINH1274_OLD", &aoRideInh107Old, 1, 0, "Г1: прыжок с платформы не наследует скорость"},
  {"SPOTHER107_OLD", &spOther107Old, 1, 0, "П3: чужая машина не бьёт стейтпропы"},
  {"STUCKLOG108_OLD", &carDbgDiag108, 0, 0, "строки ящика STUCK/NEAR выключены"},
  {"TRFPEDOBS108_OLD", &trfPedObs108Old, 1, 0, "Т1: трафик тормозит перед любым пешеходом"},
  {"COLLPREP108_OLD", &collPrep108Old, 1, 0, "О16: набор гэгов пересчитывается каждый кадр"},
  {"COLRAYK1_OLD", &colRayK1Old, 1, 0, "К1: луч коллизии обходит весь прямоугольник клеток"},
  {"FXFIND108_OLD", &fxFind108Old, 1, 0, "О17: картинка эффекта ищется перебором каждый раз"},
  {"MISSGRID_OLD", &missGridOn, 0, 0, "О8: streamMiss обходит все клетки"},
  {"MSAFIND108_OLD", &msaFind108Old, 1, 0, "О17: рисунок метки ищется перебором каждый раз"},
  {"P1PAGES_OLD", &rb_P1PAGES_OLD, 1, 0, "П1: плотная карта высот и стен в блоке уровня"},
  {"PBIDX109_OLD", &pbIdx109Old, 1, 0, "З2: пересборка листа предметов обходит все экземпляры, как до 109"},
  {"LATBAKE_OLD", &latBakeOld, 1, 0, "З8: признак решётки считается обходом пикселей, не из texbake.shr"},
  {"SKYBAKE_OLD", &skyBakeOld, 1, 0, "З8: цвет дали и горизонта считается при загрузке, не из texbake.shr"},
  {"MSNARTGROUP1298_OLD", &msnartGroup1298, 0, 0, "1298: непрозрачные меши реквизита гонки по одному, порядок прежний (без групп по текстуре)"},
  {"NPD1361_OLD", &mvNpd1361, 0, 0, "1361: риг без кожи (npd) снова «читается», экран загрузки ждёт 3 с (L5)"},
  {"MSNARTSHARE1363_OLD", &msnartShare1363, 0, 0, "1363: общие записи миссий уровня (cards, wrench, missgen, значки) читаются на каждом старте миссии"},
  {"MSNARTCULL1298_OLD", &msnartCull1298, 0, 0, "1298: реквизит гонки без отсечения по кадру и сетки (рисуются все меши круга 70 м)"},
  {"PBVER108_OLD", &pbVer108Old, 1, 0, "О7: подписи запечённых предметов опрашиваются каждый раз"},
  {"PROPHOT108_OLD", &propsHot108Old, 1, 0, "О6/О9: прежние обходы предметов"},
  {"SKIDDENSE108_OLD", &skidDense108Old, 1, 0, "О13: два обхода кольца следов"},
  {"SLSCAN108_OLD", &slScan108Old, 1, 0, "О12: свет обходит все клетки каждый вызов"},
  {"SPIDLE108_OLD", &spIdle108Old, 1, 0, "О11: шаг стейтпропов без раннего выхода"},
  {"COLLCANCEL107_OLD", &collCancelOld107, 1, 0, "P0: отмена чтения гэга без предела по времени"},
  {"MSNARTCAP109_OLD", &msnartCap109Old, 1, 0, "109: 24 места под записи msnart вместо 32"},
  {"CAMWALL1316_OLD", &camWall1316Old, 1, 0, "1316 К6: полосы разговора и дуга камеры идут по dt с потолком 0,05 с, как раньше"},
  {"VEHDESTROYED1315_OLD", &vehDestroyed1315Old, 1, 0, "1315 К7: строки VEHDESTROYED (разбита / взрыв: показано или почему нет) не пишутся"},
  {"PROXBAR1318_OLD", &mvProxBar1318Old, 1, 0, "1318: шкала близости заполняется расстоянием и серая (прежнее); оригинал: 1 − расстояние/предел, красная у «оторваться», зелёная у «не отставай»"},
  {"FINLINE1317_OLD", &mvFinLine1317Old, 1, 0, "1317: финишная лента гонки без поворота локатора (вдоль осей модели)"},
  {"BUBVIS1310_OLD", &mvBubVis1310Old, 1, 0, "1310: текст этапа показывается под карточкой/ирисом (срок идёт невидимым)"},
  {"BUBCAM1315_OLD", &mvBubCam1315Old, 1, 0, "1315: пузырь ждёт «камера занята» без предела"},
  {"LOADQ1303_OLD", &mvLoadQ1303Old, 1, 0, "1303: ✕ на LOADING запоминается и закрывает карточку при готовности (прежнее)"},
  {"LOADCHR109_OLD", &mvChr109Old, 1, 0, "109/1293: карточка не заказывает модели персонажей миссии (читаются по ходу этапов)"},
  {"LOADLIST109_OLD", &mvList109Old, 1, 0, "109: карточка не заказывает и не ждёт записи миссии по списку msnres"},
  {"LOADREADY109_OLD", &mvReady109Old, 1, 0, "109: карточка не ждёт пустую очередь msnart и модели фазы"},
  {"LOADEND109_OLD", &msnartEnd109Old, 1, 0, "109: записи миссии снимаются на старте следующей, не на конце"},
  {"PROPVOL107_OLD", &propsVol107Old, 1, 0, "П4: контакт предмета по стволу/кругу, не по объёмам из данных"},
  {"PROPGHOST107_OLD", &propsGhost107Old, 1, 0, "П1/П2: лежащий задетый предмет — призрак для машин у всех"},
  {"PROPLOSS107_OLD", &propsLoss107Old, 1, 0, "П5: две формулы потери скорости о фонарь"},
  {"PROPSTOMP107_OLD", &propsStomp107Old, 1, 0, "П12: стомп по простому предмету 400/масса"},
  {"PROPBOOM107_OLD", &propsBoom107Old, 1, 0, "П11: взрыв задевает все предметы радиуса"},
  {"CARDMGPROP107_OLD", &carDmgProp107Old, 1, 0, "П10: урона машине от предметов нет"},
  {"POLE1276_OLD", &propsPoleKick1276Old, 1, 0, "П6: толчок бампера 1046, столб ложится за 0,5 с"},
  {"SPOTHER107_OLD", &spOther107Old, 1, 0, "П3: чужие машины едут сквозь стейтпропы"},
  {"SPBUSH1284_OLD", &spBush1284Old, 1, 0, "1284: круглые кусты насквозь"},
  {"SPZONE1284_OLD", &spZone1284Old, 1, 0, "1284: сбитые стейтпропы возвращаются через 5 с дальше 60 м"},
  {"PWR107_OLD", &msnartPwr107Old, 1, 0, "P0: msnart не сбрасывает пометки по пробуждению"},
  {"PWRCOLL107_OLD", &collPwr107Old, 1, 0, "P0: гэги не сбрасывают пометки по пробуждению"},
  {"COLLHEAL107_OLD", &collHeal107Old, 1, 0, "P0: пометки отказов гэгов не снимаются"},
  {"QUAR107_OLD", &msnartQuar107Old, 1, 0, "P0: брошенный буфер чтения в утечку"},
  {"RIDE1274_OLD", &aoRide1274Old, 1, 0, "1274: платформа не несёт Гомера"},
  {"RIDEINH1274_OLD", &aoRideInh107Old, 1, 0, "1274: прыжок с платформы без её скорости"},
  {"MSN_LOADWAIT109_OLD", &mvLoadWait109Old, 1, 0, "109 (г): этап поездки стартует сразу, не ждёт модели следующего этапа"},
  {"MSN_PHASECARS109_OLD", &mvPhaseCars109Old, 1, 0, "109 (в): чтение вперёд моделей машин только следующего этапа"},
  {"ZONE_BODY_OLD", &zoneBodyOn, 0, 0, "Б.10: ворота района срабатывают по точке машины, не по кузову (центр и четыре угла)"},
  {"CAR_WHEELDRAW108_OLD", &carWheelDraw108Old, 1, 0, "108: колёса на экране по сжатию comp, не по ходу подвески"},
  {"HOMERHIT109_OLD", &rb_HOMERHIT109_OLD, 1, 0, "С-г1: Гомера пешком сбивает и бок проезжающей машины, и крыша (как было)"},
  {"MCRHOLD109_OLD", &mcrHold109Old, 1, 0, "109 (в): машины миссии держатся 90 с и читаются заново, чтение вперёд без лестницы памяти car-ahead"},
  {"COLRINGRETRY_OLD", &colRingRetryOld, 1, 0, "108: colring после сна — прежнее сравнение таймера повтора"},
  {"COINSPIN1299_OLD", &hudCoinSpin1299Old, 1, 0, "1299: монета у счёта по игровым часам (в паузе и меню стоит)"},
  {"CARDICON1302_OLD", &uiCardIcon1302Old, 1, 0, "1302: значки ✕ и △ карточки брифинга без сдвига вниз"},
  {"MSNARTCHAIN1359_OLD", &msnartChain1359Old, 1, 0, "1359: насос записей миссии берёт следующее чтение только в следующем кадре"},
  {"COLLCHAIN1364_OLD", &collChain1364Old, 1, 0, "1364: гэги берут одно фоновое задание за кадр, без цепочки заданий в кадре"},
  {"INPUTHINTS1357_OLD", &inputHints1357Old, 1, 0, "1357: ✕/△ на разговоре и карточке миссии принимаются и до появления подсказок кнопок"},
  {"PAUSEBETWEEN1348_OLD", &mvPauseBetween1348Old, 1, 0, "1348: в окне между этапами пауза свободная (MISSION SELECT), а не паузой миссии"},
  {"LOADMARK1366_OLD", &uiLoadMark1366Old, 1, 0, "1366: знак загрузки и на экранах карточки, гардероба, газеты, бонус-игры, как в 1360 (по умолчанию только на экране ириса)"},
  {"LOADMARK1360_OLD", &uiLoadMark1360Old, 1, 0, "1360/1366: знак загрузки только на чёрном экране ириса (transit.c); 1 — прежняя надпись LOADING по центру внизу"},
  {"PAUSEOBJPOS1350_OLD", &uiPauseObjPos1350Old, 1, 0, "1350: рамка задания на паузе стоит на месте HUD и заходит на рамку меню"},
  {"PAUSEOBJ1347_OLD", &uiPauseObj1347Old, 1, 0, "1347: на паузе нет окна задания с текстом этапа"},
  {"LBSKIP1322_OLD", &uiLbSkip1322Old, 1, 0, "1322: значок ✕ у SKIP экрана разговора без сдвига вниз"},
  {"SVDLGPAD1339_OLD", &svDlgPad1339Old, 1, 0, "1339: после окна сохранения PSP пульт не пересчитывается (✕ «Да» отмены открывает окно снова)"},
  {"GLOW1341_OLD", &glow1341Old, 1, 0, "1341: середина свечения входов в дома и помещения густая (заливка от границы ядра)"},
  {"SHOPSND1329_OLD", &shopSnd1329Old, 1, 0, "1329: звуки экрана покупки машины на шине эффектов (под режимом Store молчат)"},
  {"HOMER_DECKS1332_OLD", &homerDecks1332Old, 1, 0, "1332 (Б): этажи верха высоких машин (cardecks.txt) выключены — одна плоская крыша на bodyHi"},
  {"HOMER_OWNROOF1332_OLD", &homerOwnRoof1332Old, 1, 0, "1332: своя машина выталкивает Гомера с крыши до крыши минус 0,15 (автобус и Mr. Plow не взять)"},
  {"MSN_BONUSTWIN1296_OLD", &msnBonusTwin1296Old, 1, 0, "1296/1300: бонусный двойник персонажа не скрыт, △ у собеседника миссии открывает гонку (как было)"},
  {"LEVELINTROCAR1294_OLD", &levelIntroCarOn, 0, 0, "1294: вступление уровня не играет в машине (сразу музыка езды), как было"},
  {"NPCSCENE1293_OLD", &npcScene1293On, 0, 0, "1293: готовность сцены персонажа миссии ждёт все клетки (клетки вне набора ворот и вне высоты ног считались), как было"},
  {"RSPEND1304_OLD", &mvRsPend1304Old, 1, 0, "1304: перезапуск миссии по Yes без принудительного старта через 1,5 с"},
  {"LBDUCK1294_OLD", &lbDuckAll1294Old, 1, 0, "1294: Letterbox-приглушение музыки на любые полосы (облёт старта), как было"},
  {"NPCROOM1314_OLD", &mvNpcRoom1314, 0, 0, "1314: персонаж с тегом интерьера, не совпавшим с комнатой, скрыт весь этап (Мо в таверне), как было"},
  {"OVBAR1308_OLD", &uiOvBar1308Old, 1, 0, "1308: шкалы урона и близости вне стопки оверлеев миссии (таймер на шкале), как было"},
  {"STAGEENDNAME1359_OLD", &stageEndName1359Old, 1, 0, "1359 п.1: машина миссии с именем погони (cPolice: L4 m7, L5 m6, где она гонится за игроком) после своего этапа паркуется и уходит, как остальные — прежнее: по имени пропускалась и гналась до конца миссии"},
  {"PARKDMGNAME1359_OLD", &parkDmgName1359Old, 1, 0, "1359 п.2: припаркованная cPolice миссии (L4 m1 у Lard Lad) неуязвима до выдачи ей ИИ, как остальные машины миссий — прежнее: по имени исключена, её можно было разбить"},
  {"MSNDONEPOL1359_OLD", &msnDonePol1359Old, 1, 0, "1359 п.3: в конце миссии погоня H&R снимается по флагу и указателю, машина миссии cPolice уходит через очередь — прежнее: по имени, машина миссии пропадала на глазах (а при другом порядке слотов оставалась погоня)"},
  {"WCARPOL1359_OLD", &wcarPol1359Old, 1, 0, "1359: свободная машина мира (rocke_v, atv_v, hype_v…) не уходит со своего места от проезжающего копа погони — прежнее: любая машина в 10 м считалась «машиной миссии» и убирала её"},
  {"HNRBUSTNAME1356_OLD", &hnrBustName1356Old, 1, 0, "1356: арест и погоня Hit & Run считают полицией только машины погони (флаг police), а не стоящую рядом машину миссии с именем cPolice (L4 m1 у Lard Lad: «штраф, штраф, штраф», L4 m7, L5 m6) — прежнее: по имени модели, арест от машины миссии"},
  {"NPCSLOT1360_OLD", &npcSlot1360Old, 1, 0, "1360: персонаж миссии с именем фонового (Сельма L4 sr3, Апу, Мо, Пэтти, Лиза…) получает свой слот и не стирает фонового; после миссии фоновый на месте — прежнее: слот фонового перезаписывался и пропадал до перезагрузки уровня"},
  {"HNRPOLNAME1350_OLD", &hnrPolName1350Old, 1, 0, "1350: снятие погони Hit & Run убирает только полицию H&R, а не машину миссии с тем же именем модели (cPolice, L4 m1 «Follow the police» — цель слежки пропадала на старте этапа 1.1.0) — прежнее: по имени, цель слежки удалялась"},
  {"TRF_RETHB1360_OLD", &trfRetHb1360Old, 1, 0, "1360: возврат ударенной машины трафика на полосу идёт с отпущенным ручником (прежде физический шаг оставлял ручник затянутым: ползла, не успевала вернуться за отведённое время)"},
  {"TRF_REJOIN1345_OLD", &trfRejoin1345Old, 1, 0, "1345: ударенная машина трафика стоит телом 10–15 с, потом едет на свою полосу сама (до 3,5 м/с, с поворотом), переноса в кадре нет; вне кадра перенос как раньше — прежнее: через 3 с перенос на полосу в кадре («чпонг», до 8 м, курс до 90°)"},
  {"MSNBRIEFBB1337_OLD", &mvBriefBb1337Old, 1, 0, "1337: ящик пишет, на чём ждёт карточка миссии (brief-wait q c l base) и сколько ждала (brief-ready) — прежнее: строк нет (только в журнале смоука)"},
  {"MSNIOPRIO1337_OLD", &msnIoPrio1337Old, 1, 0, "1337: на старте этапа картинки брифинга, модели персонажей и машин миссии читаются раньше догрузки клеток города (загрузчик уступает карту памяти, не дольше 2 с подряд) — прежнее: клетки вперёд, картинки 4 с и больше в очереди"},
  {"CARSTACK1333_OLD", &carStack1333Old, 1, 0, "1333: удар машина-машина: высота одной над другой — вдоль средней оси «вверх» только у двух машин на одном склоне (оси расходятся < 10°); накренённая цель — по мировой вертикали — прежнее: любая средняя ось, цель, накренённая тараном (автобус в борт седана на 75–90 км/ч), считалась «выше» и вставала на крышу тарана"},
  {"SV_FLIPEMA1330_OLD", &carFlipEma1330Old, 1, 0, "1330: автосброс «вверх колёсами без касания» по сглаженным скорости и вращению (среднее 1/15 за кадр, как у оригинала) — прежнее: по мгновенным, сбрасывало и в верхней точке кувырка"},
  {"SV_LEANREST1330_OLD", &carLeanRest1330Old, 1, 0, "1330: «лежит» и при подъёме кузова < 0,5, если на земле не больше двух колёс (грузовик на двух бортовых колёсах под 64° лежал бесконечно) — прежнее: только подъём < 0,2"},
  {"WEEBLE1323_OLD", &carWeeble1323Old, 1, 0, "1323: «неваляшка» по контакту колёс, бортовые пары (диагональ только famil_v), в полёте момент есть — прежнее: пары по comp <= 0, в полёте нет"},
  {"MC_AIRESET1325B_OLD", &mcAiReset1325BOld, 1, 0, "1325б: сброс ИИ (переворот, 7-е застревание): соперник гонки — на последнюю точку оси полосы под ним, в точке убирается трафик в двух колёсных базах — прежнее: на ближайшую полосу, трафик остаётся"},
  {"MC_AIRESET1325_OLD", &mcAiReset1325Old, 1, 0, "1325: машина ИИ миссии в зоне DEATH (вода, овраг) сразу в точку возврата зоны, перевёрнутая (75°, 1 с) — на ближайшую дорогу — прежнее: зон для ИИ нет, встаёт на месте"},
  {"CAR_SUSP1278_OLD", &carSusp1278Old, 1, 0, "108: подвеска как была (ход вниз 0,13 м, пружина по доле веса оси)"},
  {"CAR_SPR109_OLD", &carSpr109Old, 1, 0, "108: жёсткость пружин по доле веса оси, не одна 27 047"},
  {"CAR_COG109_OLD", &carCog109Old, 1, 0, "108: центр масс из данных диска, как было"},
  {"HNRSTAGE109_OLD", &gameHnrStage109Old, 1, 0, "Ф2-7: запрет Hit & Run держится до конца миссии, не этапа"},
  {"RFDL108_OLD", &rfOld, 2, 1, "О3: dlCity читает кусок для каждого вызова клетки (RF_DL выключен)"},
  {"CITY109_OLD", &city109On, 0, 0, "О3 шаг 2: дальность клетки считается по куску, не раз на клетку"},
  {"CAR_FENCEANG1279_OLD", &carFenceAng1279Old, 1, 0, "1279: забор тормозит правилом CAR_CRASH_ANG, как раньше"},
  {"CARHITFRIC1279_OLD", &carHitFric1279Old, 1, 0, "109 1279: удар машина-машина без касательного трения, как раньше"},
  {"NPCPROP1301B_OLD", &gameNpcProp1301bOld, 1, 0, "1301b: проверка предметов у каждого шага каждого персонажа миссий (в 108 до удешевления)"},
  {"GAME_NPC_PROPSOLID1301_OLD", &gameNpcPropSolid1301Old, 1, 0, "1301: персонажи миссий идут сквозь предметы и стейтпропы, как до 108"},
  {"SPONCEOTHER1305_OLD", &spOnceOther1305Old, 1, 0, "1305: разовые предметы, разбитые чужой машиной, пишутся в сохранение и не возвращаются, как в 107"},
  {"CARD_CORNER108_OLD", &carCardCorner108Old, 1, 0, "1297: карточка и гэг из машины: точка низ + 0,9 и углы кузова с запасом 1,5 м, как раньше"},
};
static void rbLog(const char *l) { if (smkOn) smkWrite(l); bbNote(l); }
// text — содержимое rollback.txt; возвращает число применённых строк
static int rollbackApply(const char *text) {
  int applied = 0; char name[64]; const char *p = text;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p) break;
    int n = 0; while (p[n] && p[n] != '\n' && p[n] != '\r') n++;
    int k = 0; if (*p != '#') { while (k < n && k < (int)sizeof name - 1 && p[k] != ' ' && p[k] != '\t' && p[k] != '#') { name[k] = p[k]; k++; } }
    name[k] = 0; p += n;
    if (!k) continue;
    int found = 0; char l[160];
    for (unsigned i = 0; i < sizeof rbTab / sizeof rbTab[0]; i++) if (!strcmp(name, rbTab[i].name)) {
      if (rbTab[i].var) { if (rbTab[i].orMask) *rbTab[i].var |= rbTab[i].val; else *rbTab[i].var = rbTab[i].val; }
      if (!found) { snprintf(l, sizeof l, "ROLLBACK %s applied: %s\n", name, rbTab[i].what); rbLog(l); applied++; }   // у имени может быть несколько переменных (PWR107_OLD: msnart и гэги) — строка и счёт один раз
      found = 1; }
    if (!found) {   // откаты, которые не переменная: вызов (звук) или решение при запуске ME (модуль)
      extern void audioSbHealOld(int); extern void audioMeWakeOld(int);
      if (!strcmp(name, "SBHEAL107B_OLD")) { audioSbHealOld(1); found = 1; snprintf(l, sizeof l, "ROLLBACK %s applied: sfx.sab после сна не лечится\n", name); }
      else if (!strcmp(name, "MEWAKE107B_OLD")) { audioMeWakeOld(1); found = 1; snprintf(l, sizeof l, "ROLLBACK %s applied: ME после сна не возвращается\n", name); }
      else if (!strcmp(name, "SHARME_STALE107B_OLD")) { found = 1; snprintf(l, sizeof l, "ROLLBACK %s: модуль sharme_old.prx (читается при запуске ME, нужен файл рядом с EBOOT)\n", name); }
      if (found) { rbLog(l); applied++; }
    }
    if (!found) { snprintf(l, sizeof l, "ROLLBACK %s unknown\n", name); rbLog(l); }
  }
  return applied;
}
static void rollbackLoad(void) {
  char fn[256]; sibling(fn, "rollback.txt"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  static char buf[8192]; int n = sceIoRead(f, buf, sizeof buf - 1); sceIoClose(f); if (n <= 0) return; buf[n] = 0;
  if (n >= (int)sizeof buf - 1) { rbLog("ROLLBACK file truncated: больше 8 КБ, хвост не прочитан\n"); }   // молча не обрезаем
  int a = rollbackApply(buf); char l[80]; snprintf(l, sizeof l, "ROLLBACK file applied=%d\n", a); rbLog(l);
}
#endif
