// Миссии в EBOOT (пункт 8, m0 «The Cola Caper»): логика — viewer/game_msn.c (общая с ботами), здесь то, что видит и
// нажимает игрок на PSP: пакет level01.msn рядом с EBOOT, интерфейс миссий из родных элементов Scrooby (ui.shr: текст
// задания, таймер, «этап пройден», MISSION COMPLETE, провал с повтором), значок цели на радаре, персонажи миссий
// (chars/<имя>.chr, tools/char_export.py) и маркеры цели/значки задания из данных диска (msnart.shr, tools/msnart_export.py).
//
// Порядок в main.c:  mvInit(каталог EBOOT) один раз после uiInit; mvNewGame(&G) — NEW GAME (старт уровня = m0);
//   кадр: act = △ && mvWantsAction() (тогда △ не идёт в посадку); ... gameAfterCar; mvTick(act, dt);
//   3D: mvDraw3D(глаз) после Гомера; mvDrawMarkers(глаз) после частиц; HUD: mvHud(dt) до uiDraw, mvOverlay() после;
//   провал: mvInput(&hit) забирает кнопки меню повтора. Комнаты (другой поток): mvSetInterior(имя | NULL) каждый кадр.
#ifndef MSN_VIEW_H
#define MSN_VIEW_H
#include "stateprops.h"
#include "game.h"
#include "mission/msn.h"   // MsnProgress (сохранения)
#include "ui.h"            // UiLevelStats (LEVEL PROGRESS)
#include "msncar.h"        // машины миссий с ИИ (рисует main.c)

int  mvInit(const char *ebootPath, int level);   // 0 = пакет миссий level0<level>.msn прочитан (нет файла — игра без миссий)
void mvSetLog(void (*log)(const char *line)); // служебные строки MSN ... (smoke.txt)
int  mvNewGame(Game *g);                     // старт уровня: m0 (Гомер выходит из дома к Мардж); 0 = ок
int  mvNewGameAt(Game *g, const MsnProgress *resume);   // то же с загрузкой сохранения (NULL — mvNewGame), viewer/savegame.h
extern int (*mvHeapRoom)(unsigned);
extern int (*mvViewBusy)(void);   // №632: 1 — камера будки/лавки: чтение NPC не начинать   // ревизия 1.1: хватает ли кучи под чтение впрок (запас стриминга сохраняется); NULL — не проверять
extern void (*mvCarPrefetch)(const char *);   // №629: чтение впрок блока машины миссии (ставит main)
extern int mvProfOn;   // тест-апка: разбивка mvHud
int mvProfTake(char *out, int n);   // строка MVHUD или 0
extern int (*mvFlatGlow)(const char *name, const float *pos, float ex, float ey, float ez);   // №793: пятно <рисунок>_shadow векторно (ставит main); 0 — рисовать картинкой
extern void (*mvPump)(void);   // правка 244: кадр экрана загрузки во время чтения моделей (NULL — нет)
int  mvProgress(MsnProgress *p);             // прогресс миссий для записи; 0 — миссий нет
// пауза во время миссии (PauseMission.scr, 29.09): 1 — идёт сама миссия (основная часть, не вступление «sd» и не свободная
// езда) — START открывает паузу миссии; RESTART MISSION — миссия заново с первого этапа основной части (игрок — на точку
// сброса миссии), CANCEL MISSION — отмена, как Cancel брифинга (назад к вступлению миссии)
int  mvInMission(void);
int  mvModels(void);                          // правка 103: моделей персонажей в памяти сейчас (строка чёрного ящика)
int  mvGiverNear(float x, float z, float r);   // значок бонус-миссии / гонки / продавца ближе r м: его UI_R_* (-1 — нет); подсказки Барта
int  mvGiverAt(int type, int k, float *xz);   // служебное (flag bonusgo): k-й значок типа type (UI_R_*) — x, z; 0 — нет
int  mvTalkNpcAt(float *p);   // служебное (flag bonusgo -1): собеседник цели — x, y, z и высота значка над ним; 0 — нет
int  mvTutorialMode(void);                   // EnableTutorialMode(1 | true) в leveli.mfk уровня (подсказки Барта, viewer/tutorial.h)
// LEVEL PROGRESS / конец уровня (LevelStats.pag, 29.09): строки миссий уровня (сюжет, бонус, уличные гонки — по флагам миссий,
// сделано = rt.completed), машины и одежда (BindReward уровня: своя по умолчанию, машина бонус-миссии и гонок — после них, прочие —
// если куплены: host.owns), всего гэгов (SetTotalGags). Карточки, осы, найденные гэги заполняет main.c (collectCount)
void mvLevelStats(UiLevelStats *s);
// конец уровня: вместо вспышки LEVEL COMPLETE — экран LevelEnd.scr (LevelStats.pag + Accept), мир стоит; X — дальше (mvLevelDone)
int  mvLevelEnd(void);
void mvLevelEndInput(unsigned hit);
void mvPauseRestart(void);
void mvPauseAbort(void);
int  mvFreeRoam(void);                       // 1 — свободная езда (или миссий нет): SAVE GAME доступен, как в PauseSunday.pag
void mvServiceMark(int mission);             // служебное (гейт сохранений, flag svmark): отметить миссию пройденной
void mvShutdown(void);                       // выход в главное меню: миссии выгружаются (модели, маркеры)
void mvFree(void);                           // смена уровня: mvShutdown + пакет миссий + msnart.shr уровня
void mvFreePack(void);                       // только пакет миссий (mvInit сам зовёт перед чтением нового)
void mvIconState(char *o, unsigned n);          // №958б: значок цели в углу одной строкой: имя/поле/картинка/уровень цели/текст (чёрный ящик)
int  mvPackLevel(void);                      // номер уровня в заголовке пакета MSN1 (0 — пакета нет): сверка набора уровня
int  mvLevelDone(void);                      // 1 — уровень пройден, 2 — игра пройдена (MSN_EV_LEVEL_COMPLETE после вспышки), один раз
const char *mvFmvPending(void);              // цель «fmv»: имя ролика ("fmv3.rmv"), пока main.c его не сыграл; NULL — нет
void mvFmvDone(void);                        // ролик сыгран (или его нет) — цель засчитана (MSN_IN_FMV_DONE)
void mvServiceFade(int out, float t);        // служебное: затемнение (flag fadeout/fadein)
float mvFadeAlpha(void);                    // правка 122: затемнение миссии 0..1 (SetFadeOut / StayInBlack), main.c рисует поверх мира
int  mvMsgBox(const char **text);            // правка 122: окно сообщения ждёт показа (1 — text); мир стоит, main.c рисует uiDrawMessageBox
void mvMsgBoxInput(unsigned hit);             // ✕ / ○ — закрыть окно сообщения
void mvServiceFlash(int bonus);              // служебное: вспышка MISSION COMPLETE (bonus — BONUS MISSION), flag msnflash
void mvServiceUnlock(const char *name);        // служебное: как награда name (окно сообщения), flag unlockmsg
void mvBusted(void);
void mvMusicResume(void);   // правка 235: музыка миссии снова (её событие _start), например после магазина                         // правка 121: арест Hit & Run — идущая миссия провалена («Hit & Run: You got busted!»)
void mvServiceWin(void);
void mvMarkStoryDone(void);   // №860: финал — все сюжетные миссии уровня пройдены (без событий)
void mvServiceFail(void);                       // служебное (flag msnfail): экран провала миссии (№572)
const char *mvDynaRoom(void);                // интерьер последнего SetDynaLoadData («…@», "SimpsonsHouse"); "" — город                     // служебное (flag lvwin): все сюжетные миссии пройдены -> конец уровня
int  mvActive(void);
int  mvKick(float x, float y, float z, float r, float dx, float dz);   // №843: пинок Гомера по персонажам миссии (game_msn.c gameMsnKick)
int  mvFrozen(void);                         // 1 — отсчёт 3-2-1 этапа: игрок стоит, управление — на GO
void mvFreezeCar(CarInput *in);              // во время отсчёта: газ/тормоз/руль 0, ручник
int  mvWantsAction(void);                    // △ этого кадра — миссии (разговор, точка с действием)
void mvTick(int action, float dt);
int mvNpcPosition(const char *name,float point[3]);
const MsnCars *mvCars(void);                 // машины миссии с ИИ (viewer/msncar.h) — рисует main.c; NULL — миссий нет
int mvDriversDrawn(void);                    // аудит 09.10 п. 20: водителей машин миссии нарисовано в последнем кадре
void mvServiceSellers(void);                  // служебное (flag sellerlog): места разговора продавцов и сами продавцы (правка 288)
void mvServiceMain(int mission);
void mvMarkerShadowOld(int on);                // 04.10: 1 — маркер цели без пятна <рисунок>_shadow (флаг mkshadowold, красный контроль)
void mvServiceUfo(void);                       // №967: строка MSNUFO (тарелка L7, заряд луча)
void mvServiceMainAt(int code);                // служебное (flag msnmainat M*100+S): миссия M с этапа S основной части             // служебное: основная часть миссии с её чекпоинта (flag msnmain N)
int mvDriverShown(int i);                   // №666: у машины миссии i рисуется водитель-персонаж (запечённого в кузов не рисовать)
const char *mvForcedCar(void);               // SetForcedCar: игрок на машине миссии (модель пакета msncars); NULL — машина уровня
void mvCarjack(const char *model, const CarParams *P, float lift, float x, float y, float z, float h);   // правка 63: угон машины трафика
float mvHnrDecay(void);
int mvIsPolice(const char *name);
void mvPoliceHold(void);                      // №627: полиция после ареста стоит на месте (исчезает через mvPolice(0) по концу таймера)
void mvPolice(int on);                        // правка 64: погоня Hit & Run — полицейская машина уровня едет за игроком / уходит
void mvWorldCar(const char *car, const float p[3], float h);   // правка 53: машина мира (ракетная у локатора rocke)
int  mvWorldCarNear(float x, float z, float r, float out[4]);    // -> номер или -1
int  mvTakeWorldCar(int i);                                       // 1 — игрок пересел в неё
int  mvHnrOff(void);                         // перепроверка 96: миссия выключила Hit & Run (DisableHitAndRun)
int mvPhoneCar(const char *car);             // правка 94: машина из галереи будки (1 — сменилась, 0 — машина уровня / нет модели)
int mvSummon(void);                           // правка 62: будка — своя машина (0 — на машине миссии нельзя)
void mvSetInterior(const char *name);        // КРЮЧОК КОМНАТ: имя интерьера как в скриптах ("KwikEMart"), NULL — город
void mvSetRoomAt(const char *(*roomAt)(float x, float y, float z)); // №376: комната по настоящему полу SHI1, а не высоте города
int  mvTeleported(float pos[3]);             // 1 — сценарий переставил игрока (старт/повтор/выбор миссии): камера, экран загрузки
void mvSetGround(int (*f)(float x, float z, float yHi, float yLo, float *out));   // №606: высота коллизии под точкой (main: colGround); без вызова стрелка стоит как раньше
void mvSetGov(int on);   // L3 причал: кадр в перегрузке (frameGov) — дальние персонажи миссии не рисуются дальше NPC_GOV_DIST
void mvSetVis(int (*f)(const float bb[6]));   // правка 500: отсечение персонажей по кадру (trVis из main); без вызова — рисуются все
void mvDraw3D(float ex, float ey, float ez, float dt);   // персонажи миссий (скиннинг GE); после — resetState() в main.c
int mvNpcFeet(float (*out)[4], int max, float ex, float ey, float ez, float r);   // №748: персонажи миссий, которые рисуются (ближе r м к глазу, своя комната): x, y (ноги), z, рост модели — векторные тени (spritelight.h)
extern float mvViewDir[3];                     // №967: взгляд камеры (единичный), ставит main перед mvDrawMarkers
void mvDrawMarkers(float ex, float ey, float ez, float t);
void mvStatePropSync(StateProps *sp);   // №314: бочки L7 — включить экземпляр SPR1 на цели «подобрать предмет», спрятать после
int mvStatePropWanted(const float p[3]);   // маркер цели (billboard-ы/меши msnart.shr)
void mvHud(float dt);                        // поля и метки HUD, значки радара (до uiDraw)
void mvOverlay(void);                        // меню повтора после провала (после uiDraw)
int  mvInput(unsigned *hit);                 // 1 — кнопки забрало меню провала (*hit обнулён)
int  mvFailedScreen(int *sel);               // 1 — провал: экран MissionOver (uiDrawMissionOver(sel) вместо uiDraw), мир стоит
int  mvMissionSelect(int on, unsigned hit);  // пауза -> MISSION SELECT: список миссий уровня; 1 открыт, 2 миссия запущена, 0 закрыт
void mvDrawMissionSelect(void);
int  mvMissionSelectOn(void);                 // 1 — список MISSION SELECT открыт
int  mvMissionSelectAvail(int rows);          // пункт ДОБИВКИ 26: 0 — на MISSION SELECT нечего выбрать (пункт паузы серый; PAUSE_MS_DIM_OLD=1 — всегда 1);
                                              // rows: 1 — список откроется строками MissionSelect.scr (pextra.shr), 0 — запасной список
// правка 348: уровни экрана MISSION SELECT (◄ ►). levelOpen(n) — уровень n открыт (набор рядом с EBOOT, открыт в сохранении); dev — DEV SAVE.
// mvMissionSelect -> 3: выбрана миссия другого уровня — mvMissionSelectWant (уровень, строка 0..6), после загрузки уровня —
// mvMissionSelectStartRow(строка). mvMissionSelectOld — служебное «uimsselold» (как до 348: pextra.shr только на уровне 1, без ◄ ►)
void mvMissionSelectSetup(int (*levelOpen)(int), int dev);
// облако 05.10 (карьера): строки другого уровня — по записи прогресса (savegame.h SV_EXT_LV): levelRows(n) -> байт строк (бит 7
// SV_LV_ROWS_OK, биты 0..6 — строка пройдена), 0 — неизвестно (как DEV: сюжетные, кроме последней, пройдены). NULL — как прежде
void mvMissionSelectRows(unsigned (*levelRows)(int));
// облако 05.10: строки MISSION SELECT текущего уровня (бит k — строка k пройдена) | 0x80 (SV_LV_ROWS_OK); 0 — миссий нет
unsigned mvMissionRows(void);
int  mvMissionSelectWant(int *level, int *row);
int  mvMissionSelectStartRow(int row);
int  mvMissionSelectOld(void);
void mvMsLR1058Set(int old);   // №1058: 1 — уровень в Mission Select только ◄ ► (как в TEST73); 0 — ещё L R
int  mvBriefLoading(void);                  // №626: 1 — брифинг ещё под газетой LOADING (карточки нет): время для прогрева мира
int  mvBriefing(void);                       // 1 — брифинг миссии на экране (мир стоит, как пауза): uiDrawBriefing вместо uiDraw
void mvBriefingInput(unsigned hit);          // X — продолжить (после LOADING, правка 52); △ на карточке ничего не делает
void mvBriefOld(int on);                     // отрицательный контроль правки 52: прежняя карточка (сразу Continue и Cancel)
#include "shop.h"
Shop *mvShop(void);                          // лавка продавцов уровня (viewer/shop.h) для экрана покупки (shop_view.c); NULL — миссий нет
// Правка Роба 40 — камеры из данных миссий:
// камера старта фазы (viewer/game_msn.h gameMsnStartCam): 1 — заявка (имя фазы SelectMission и камеры), одна на фазу
int mvStartCam(const char **phase, const char **cam);
// №1047 (Роб: «сначала чёрные полоски, наезд, и ПОТОМ пузырь с текстом»): цель этапа (пузырь) не показывается, пока идёт наезд и полосы на экране; mvCamBusy ставит main_loop.h
// (1 — наезд играет или полосы ещё не ушли), mvBubblePump — каждый кадр: отложенную цель показать, когда полос нет. Без наезда — как раньше
extern int (*mvCamBusy)(void); void mvBubblePump(void);
// камера разговора (viewer/convcam.h): 1 — идёт разговор, план реплики из данных (SetConversationCam, SetCamBestSide) или
// по правилу порта: глаз, взгляд, вертикальный FOV. prevEye — глаз камеры до разговора (сторона без SetCamBestSide);
// blocked(от, до) — луч упирается в коллизию (план за стеной не берётся: другой план того же говорящего, иначе прежний,
// иначе камера разговора не включается); NULL — не проверять. *nearZ — ближняя плоскость, не режущая собеседников
// (convNear). Вызывать каждый кадр (в комнате — нет: там камеры комнаты)
typedef int (*MvRayFn)(const float from[3], const float to[3]);
int mvTalkCam(const float prevEye[3], MvRayFn blocked, float eye[3], float look[3], float *fovY, float *nearZ);
typedef struct { int line, pcSpeaks, shot, side, fromData, hasBest; float pc[3], npc[3], pcHead, npcHead, pcH, npcH, best[3]; char npc_name[24], conv[24]; } MvTalkCamInfo;
int mvTalkCamInfo(MvTalkCamInfo *out);
// экран разговора (Scrooby LetterBox.scr, viewer/ui.h uiDrawLetterbox): идёт ли разговор, какие подписи (UI_LB_SKIP — «✕ SKIP»,
// UI_LB_START — «✕ Start Mission / △ Cancel»: разговор — последний этап вступления, дальше брифинг) и кнопки разговора:
// X — пропустить (перед миссией — начать её), △ — отменить миссию (только перед миссией). 1 — кнопки забраны разговором
int mvTalkActive(void);
int mvTalkPrompt(void);
int mvTalkInput(unsigned hit);   // служебное (гейт tests/camera/convcam_emu): 1 — камера разговора включена
void mvTalkCamPlant(int how);            // служебное, отрицательный контроль: 1 — план на слушающем, 2 — камера на оси за слушающим;
                                         // 3 — ближняя плоскость города (без convNear)
// служебное (сценарий смоука): msngo — игрок к текущей цели как «goto» (посадка — к двери машины; комнаты — нет);
// far > 0 — отрицательный контроль: встать на far м дальше от цели (цель обязана НЕ засчитаться)
void mvServiceGo(float far);
int  mvServiceBot(void);   // flag msnbot: 1 — этап ждёт △ пешком, 2 — этап машинный, а Гомер пешком (0 — ничего)
void mvServiceSwitchOld(const char *what, int on);   // №943: msnswitchold / mscursorold — прежняя смена миссии / курсор MISSION SELECT (красный контроль)
void mvServiceHud(void);                     // №943: строка MSNHUD (цель рантайма, цель HUD, маркер, пузырь); служебное flag msnhud
void mvStartFirstIfIdle(void);               // №943: миссий нет (стартовая цель уровня отложена, выбранная строка не стартовала) — стартовать отложенную
void mvServiceSelect(int mission);          // служебное: миссия по номеру пакета (бонус — как разговор с персонажем)
void mvServiceUi(const char *flag, float a);  // служебное: uistack N, uiov (рамки оверлеев), uifit / uifitplant (тексты в рамке)
#endif
void mvForceArrow(int on, float x, float y, float z, float h);   // №1025 служебно: набор стрелок в точке без миссии
extern unsigned mvMkUs[5];   // №849 служебно: части mvDrawMarkers (мкс)
extern unsigned mvRouteCalcUs, mvRouteDrawUs;   // №849 служебно
