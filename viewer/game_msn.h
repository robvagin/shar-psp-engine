// Миссии поверх логики кадра (пункт Б0): рантайм viewer/mission (msn_game) с настоящим игроком из viewer/game.c.
// Сценарий сам ставит игрока по своим локаторам (старт миссии, повтор — как в оригинале: SetMissionResetPlayer*,
// PlacePlayerCar, PutMFPlayerInCar); цели засчитываются только по настоящему положению машины/Гомера — никакого
// перемещения игрока «в цель». Общий код EBOOT (viewer/msn_view.c рисует) и ботов на Маке (tools/bots/world.c).
//
// Смысл SetMissionResetPlayerInCar/OutCar (donut: место игрока при старте/перезапуске миссии): ставим игрока только
// при старте миссии (уровень, MISSION SELECT, бонус), повторе и прерывании. Обычный переход вступление -> основная часть
// (m0: после разговора с Мардж) игрока не переносит: «сядь в машину» — действие самого игрока, как в оригинале.
//
// Персонажи миссий (AddNPC, AddAmbientCharacter): имя, место, маршрут (Add*NPCWaypoint, шаг ходьбы), интерьер —
// здесь, чтобы разговор (talkto) проверялся по живому положению одинаково на PSP и у ботов; модели рисует msn_view.c.
// Кнопка действия одна: gameMsnWantsAction() = 1, если △ сейчас нужен миссии (разговор рядом, точка с действием,
// бонус-персонаж) — тогда △ не уходит в посадку в машину (как в оригинале: одно нажатие — одно действие).
// Звук реплик и музыка миссий — msn_audio.c (gameMsnSetAudio); без него диалоги завершаются сами (MsnGame.auto_dialogue_s).
// Машины миссий с ИИ (AddStageVehicle, ActivateVehicle, AddStageWaypoint, догонялки, SetVehicleAIParams) — viewer/msncar.c:
// здесь, в логике (гонка, погоня, таран, груз проверяются по живым машинам одинаково на PSP и у ботов), рисует msn_view.c.
// Дороги для их пути — полосы трафика уровня (gameMsnSetRoads: traffic.shr). Ролики — main.c (цель fmv).
#ifndef GAME_MSN_H
#define GAME_MSN_H
#include <stddef.h>
#include "game.h"
#include "mission/msn_game.h"
#include "msn_audio.h"
#include "shop.h"
#include "msncar.h"
#include "traffic.h"   // сверка g07b круг 2: TrNavArrow

#define GAME_NPC_MAX 24       // персонажи уровня 1: 11 фоновых + 2 продавца машин + 5 бонус-миссий + миссионные
// Правка 140 (Роб 01.10 «некоторых продавцов машин нет вообще»): модели фоновых и продавцов (не миссионных) — ближайшим к
// Гомеру: до max персонажей своего мира (город / комната Гомера) в пределах GAME_AMB_NEAR, по расстоянию. Вид (msn_view.c)
// держит модель до GAME_AMB_FAR (гистерезис) и отдаёт слот, только когда ближний без модели. Правило порта: на PS2 модели
// всех персонажей уровня в памяти, у PSP 2 слота (NMODEL_AMB). -DGAME_AMB_SCRIPT_ORDER — прежнее «по порядку скрипта» (красный).
#define GAME_AMB_NEAR 60.f
#define GAME_AMB_FAR 90.f
#define GAME_TALK_CAR_NEAR 100.f   // место машины разговора дальше этого от игрока — не переносить (game_msn.c talkBegin)
#define GAME_BGIVER_MAX 8      // правка 180: персонажей бонус-миссий на уровне (у всех семи по 5)
#define GAME_NPC_WP 6          // точек маршрута на персонажа (больше всего у Неда m2: 5)
#define GAME_NPC_WALK 0.87f    // м/с: скорость корня клипа loco_walk (skin.h, skinClipSpeed) — шаг без скольжения ног
#define GAME_NPC_RADIUS HOMER_RADIUS   // капсула персонажа по xz — как у Гомера (homer.h): персонажи твёрдые друг для друга
// В85 (ДОБИВКА 21, 09.10). (1) Мардж на старте m0 L1 (вступление m0sd, цель talkto marge) на своём маршруте останавливается, не
// доходя до Гомера, на расстоянии покоя PS2: факт ps2-marge-distance-cancel-window.yaml (запись ps2_v85_marge_rtti_bound, 10 с без
// ввода): Мардж стоит в 1.572168 м по горизонтали от корня Гомера (покачивание Гомера даёт 1.473..1.572), 500 кадров на одном месте
// 220.300/−176.982; порт до правки — 0.6 м (общая граница столкновения GAME_NPC_RADIUS + HOMER_RADIUS). Число — только из этой
// записи, поэтому правило действует только там, где оно записано: таблица npcTalkStops (game_msn.c), сейчас одна строка m0sd/marge.
// Другие собеседники talkto (Лиза m1 и др.) и все остальные персонажи — прежняя граница 0.6 м. ПРАВИЛО ПОРТА: шаг ближе stop
// запрещён, остановилась — маршрут окончен, ждёт разговора на месте (что она делает на PS2, если Гомер отойдёт, не записано: в
// атласе build/АТЛАС_МИССИЙ_L1/ps2/m0d у Мардж нет колонок, снимков между кадрами 10 и 103 нет).
// GAME_NPC_TALK_STOP_OLD=1 — прежнее (красный контроль tests/missions/marge_v85.sh)
#ifndef GAME_NPC_TALK_STOP_OLD
#define GAME_NPC_TALK_STOP_OLD 0
#endif
#define GAME_NPC_TALK_STOP 1.572168f
// (2) Cancel на карточке «Start Mission / Cancel» (LetterBox) — тот же факт (записи ps2_v85_cancel_triangle_base, _retalk_8..32):
//  места после карточки на PS2: Гомер 219.832/−176.218 = его место в кадре △ атласа m0d (кадр 90: 219.833/−176.218), Мардж
//  220.097/−177.042 (не место разговора 219.288, не место появления, не место покоя 220.300), 0.866 м; Мардж стоит 140 кадров.
//  Те же места и после ✕ Start Mission (снимок атласа m0d snap_00188, eeMemory a24a70/a62210). Где Мардж стояла перед △ на PS2 —
//  не записано, поэтому возврат собеседника на его место до разговора — ПРАВИЛО ПОРТА (для Гомера совпадает с записью). У порта
//  Мардж после Cancel там, где остановилась по (1): 220.245/177.040, 0.920 м — на 0.148 м от PS2; расхождение открыто, его ловит
//  часть D гейта tests/missions/marge_v85.sh. Порт до правки: Гомер оставался на месте разговора, Мардж появлялась на m1_marge_sd.
//  Повторное △ после Cancel: через 8/16/24 кадра PAL отклонено, через 26/28/32 — принято; граница (24, 26] кадров 50 Гц.
//  Окно порта — 26 кадров PAL = 0.52 с от нажатия Cancel (кадр 25 не измерен — берём принятый край); нажатие в окне
//  пропадает, позже не срабатывает (как у PS2). GAME_TALK_CANCEL_OLD=1 — прежнее (красный контроль того же гейта)
#ifndef GAME_TALK_CANCEL_OLD
#define GAME_TALK_CANCEL_OLD 0
#endif
#define GAME_TALK_RETALK (26.f / 50.f)
// Правка 240 (Роб 03.10 14:51: «не сбиваются люди вообще никакие»): персонажи сбиваются машиной, как прохожие трафика (game_msn.c
// npcKnock). 0 — прежнее: машина проходит сквозь персонажей (красный контроль tests/npc/knock.sh)
#ifndef GAME_NPC_KNOCK
#define GAME_NPC_KNOCK 1
#endif
typedef struct {
  char name[24], interior[24];   // имя как в скриптах ("marge"), интерьер ("" — город)
  MsnVec3 pos; float h;          // где стоит, курс как у main.c (вперёд = (sin h, cos h))
  MsnVec3 wp[GAME_NPC_WP]; int nwp, iwp;   // маршрут и следующая точка; iwp == nwp — дошёл
  int kind;                      // MSN_NPC_MISSION / MSN_NPC_AMBIENT / MSN_NPC_PLACE (продавец машин, правка 140)
  float radius;                  // зона разговора фонового персонажа (AddAmbientCharacter)
  int walking;                   // 1 — идёт к wp[iwp]
  int bonus;                     // правка 180: персонаж бонус-миссии на своём месте (AddNPCCharacterBonusMission) — 1 + номер миссии, 0 — нет
  // правка 240: сбит машиной — тот же механизм, что у прохожих трафика (traffic.h trafficCarHitsBody / trafficBodyFly):
  // knock 0 — стоит/идёт, TRP_FLY (летит, клип flail) / TRP_DOWN (лежит, fall_land) / TRP_GETUP (встаёт, get_up); kv, kt —
  // скорость полёта и время в состоянии; homing — встал и идёт к home, где стоял до удара (правило порта): 1 — идёт, 2 — путь
  // занят (машина на месте, Гомер на пути: 1 с ни с места) — ждёт 3 с и пробует снова; homeP/homeT — где и сколько стоит на пути
  int knock, homing; float kv[3], kt, homeT; MsnVec3 home, homeP;
  int blocked, avoidSide; float avoidT;   // №510: путь перекрыт игроком/персонажем и обойти нельзя — стоит (клип покоя); сторона обхода (±1) и время, пока она держится
  float pcWaitT;   // №831: сколько подряд стоит перед игроком (обычное поведение: ждёт, не обходит)
  float carStallT, carStallD;   // сверка g07 №1098 (Фринк): с без продвижения к точке маршрута у машины и лучшее расстояние до точки
  float carWaitT, carFree;   // сверка g02-traffic (слой персонаж ↔ машина): с, сколько стоит, коснувшись машины (3,0 с); потом столько секунд обходит по углам кузова
  int avoidGive; float avoidBest, avoidStall, avoidTx, avoidTz;   // №813: обход без продвижения к точке (avoidStall с при лучшем расстоянии avoidBest до точки avoidTx/Tz) — бросить обход (avoidGive)
} GameNpc;

// Разговор (цель dialogue) — правки Роба 26 и 38 (видео с PSP 3:07–3:30: «Гомер и Мардж стоят друг в друге, она не
// разворачивается ко мне, глаза на меня, анимация разговора»). Данные этапа диска:
//   SetDialoguePositions(место 1-го, место 2-го из SetDialogueInfo, машина[, флаг]) — где стоят собеседники и машина;
//   AddAmbientNpcAnimation / AddAmbientPcAnimation — жест собеседника / игрока на реплику N (порядок строк = номер реплики;
//   "none" — без жеста), AmbientAnimationRandomize(кто 0 игрок / 1 собеседник, 1 — случайно), ClearAmbientAnimations.
// На старте разговора (MSN_EV_DIALOGUE): оба на своих местах (нет SetDialoguePositions — оба стоят, где стояли, как в оригинале),
// лицом друг к другу; собеседник останавливает маршрут; игрок стоит до конца разговора
// (gameMsnFrozen). Жесты и рот — вид (msn_view.c) по строке и говорящему (gameMsnTalkLine).
#define GAME_TALK_ANIMS 8
#define GAME_TALK_CAMS 8   // SetConversationCam: реплик с планом в данных уровня 1 — до 6 (m7i, bm1)
typedef struct {
  int on;                                  // идёт разговор: стоят, лицом друг к другу
  char npc[24];                            // собеседник (не игрок), как в скриптах; "" — нет в списке персонажей
  int hasPos, hasCar; MsnPlace at[2], car; // SetDialoguePositions этапа (at[i] — место i-го из SetDialogueInfo)
  const char *anim[2][GAME_TALK_ANIMS]; int nAnim[2], rnd[2];   // жесты: [0] игрок, [1] собеседник
  const char *cam[GAME_TALK_CAMS];         // правка 40: SetConversationCam(реплика, план) этапа — план на реплику (viewer/convcam.h)
  int hasBest; MsnVec3 best;               // SetCamBestSide(локатор): сторона оси, где стоит камера разговора
  int pcFirst;                             // игрок — первый в SetDialogueInfo
  float dist;                              // на каком расстоянии поставлены (служебно, журнал / гейт)
  unsigned starts;                         // сколько разговоров поставлено (служебно)
  int off;                                 // служебно: 1 — разговор не ставится (отрицательный контроль гейта tests/ui/rob_rows_check.py)
  int noReturn;                            // сверка g07 (P06-16 / P07-14): SetDialoguePositions, 4-й аргумент 1 — после реплик персонажей на прежние места не возвращать
  MsnVec3 baseN; int shifted;              // сверка g07 круг 7 (КАМЕРА 4.3): место собеседника без сдвига крупного плана игрока; 1 — сейчас сдвинут назад
  char npc2[24];                           // сверка g07 (P15-14): вторая из пары Пэтти/Сельма — тоже лицом к игроку (пустая — нет)
} GameTalk;
typedef struct { int on; char npc[24]; MsnVec3 hp, np; float hh, nh; int hCar; } GamePreTalk;   // hCar (круг 3): разговор начат за рулём — Гомера на прежнее место не возвращать   // В85: Гомер и собеседник до разговора

// AddSafeZone(локатор, радиус) этапа (уровень 2, бонус-миссия bm1: у Мо, у Squidport, у дедушки — радиус 30): пока игрок ближе
// радиуса по плоскости к локатору, шкала Hit & Run обнуляется и не копится (правило порта: машин полиции в порте нет — шкала и
// «погоня» на HUD; смысл «полиция здесь не преследует» — из реестра команд, эталон PS2 не сверен). Зона живёт до конца этапа.
#define GAME_SAFE_MAX 4
typedef struct { MsnVec3 c; float r; } GameSafeZone;

typedef struct { const char *car, *con; int count; float decay; } GameHnrConfig;
#ifndef HNR_LEVEL_C3_OLD
#define HNR_LEVEL_C3_OLD 0
#endif
#ifndef HNR_POOL_C3_OLD
#define HNR_POOL_C3_OLD HNR_LEVEL_C3_OLD
#endif

typedef struct {
  GameHnrConfig hnr;
  MsnFile f; MsnRuntime rt; MsnGame mg;   // f указывает в буфер пакета .msn (живёт у вызывающего)
  GameSafeZone safe[GAME_SAFE_MAX]; int nsafe; unsigned safeFrames;   // зоны этапа; кадров, когда шкала погашена зоной (служебно)
  Game *g;
  const char *vehicle;                    // имя машины игрока для MsnWorld
  unsigned places, resets;                // сколько раз сценарий ставил игрока (для счёта ботов)
  int markerOn; MsnMarker marker;         // текущий маркер цели, как его видит игрок (стрелка/сфера); marker.loc — триггер
  int flashes, fails;                     // «MISSION COMPLETE!» и провалы (счёт ботов)
  const char *interior;                   // комната, где Гомер (имя, как в скриптах: KwikEMart), NULL — город; ставит мир
  int startArmed, inSetup;                // старт/повтор миссии (можно ставить игрока) и окно подготовки фазы
  int keepPlayer;                         // №369: отмена Retry убирает миссию, не переставляет игрока
  unsigned skipped;                       // SetMissionReset* на переходе вступление -> основная часть (не перенос)
  unsigned teleports;                     // сколько раз сценарий переносил игрока/машину (виду: экран загрузки, камера)
  GameNpc npc[GAME_NPC_MAX]; int nnpc;
  int lastNpc;                            // правка 180: кого сценарий поставил последним (маршрут Add*NPCWaypoint — ему)
  struct { char name[24]; MsnPlace at; int mission; } bgiver[GAME_BGIVER_MAX]; int nbgiver;   // правка 180: бонусные и их места
  HomerSolidFn prevSolid; void *prevSolidUser;   // твёрдые тела Гомера до миссий (стейтпропы): персонажи добавляются к ним
  MsnGameApi view;                        // PSP (msn_view.c): интерфейс, модели, загрузки; зовётся ПОСЛЕ логики, user = GameMsn*. Боты: нули
  MsnAudio audio;                         // голос разговоров и музыка миссий (msn_audio.c); без звука — автозавершение 1 с
  MsnCars cars;                           // машины миссии с ИИ (viewer/msncar.h): состояние мира, без указателей
  int forced; char forcedName[16];        // SetForcedCar: игрок на машине миссии (game_msn.c forcedOn)
  int jacked; char jackedName[16];        // правка 63: игрок угнал машину трафика (gameMsnCarjack); машина уровня — defCar
  int trafficMax;                         // SetMaxTraffic / NoTrafficForStage этапа: машин трафика сейчас (служебно)
  CarParams defCar; float defLift;        // машина уровня на время машины миссии
  GameTalk talk;                          // разговор: места, лицом друг к другу, жесты (правки 26, 38)
  GamePreTalk preTalk;                    // В85: где стояли до разговора (talkBegin) — сюда возвращает Cancel (gameMsnTalkCancel)
  float retalkT;                          // В85: сколько ещё после Cancel △ миссии не принимается (с)
  struct {                                // правка 40: камера старта фазы (см. gameMsnStartCam)
    const char *cam, *phase;              // SetMissionStartCameraName / SetAnimatedCameraName фазы, имя фазы (SelectMission)
    int arm, placed, stages, req;         // окно старта фазы, сценарий поставил игрока, этапов с начала фазы, заявка виду
    int quiet;                            // следующая фаза — после отмены игроком (gameMsnAbort): без камеры старта
    unsigned count;                       // сколько камер заявлено (служебно)
  } scam;
  // кошелёк и продавцы (viewer/shop.h; пункты 5–6 ведущего 29.09): MsnShopApi рантайма (ставки гонок, награды, «нужен Plow King»),
  // лавка продавца (△ пешком у Барни/Гила, AddPurchaseCarReward) — её рисует вид экраном PurchaseRewards (msn_view.c)
  Shop shop; unsigned shopOpens, shopDenied;
  // П1 (L1_ФИНИШ 30.09): предметы «сломать» (m4 щитки PP_powerbox1..9, стейтпропы с тегом N): включены этапом, уже засчитаны
  uint32_t tagsOn, tagsSent;
  int raceOffRoad;   // presentation only, preserves actual race ranking/VO
  int racePlace, raceRacers; float raceDp;   // правка 326: место игрока в гонке этого кадра (gameMsnTick -> raceTrack), до его точки, м
  // №967: тарелка «Planet Express Ship» L7 (AddFlyingActorByLocator leveli.mfk, место — локатор l7_spaceship) и её луч; заряд — секунды, что машина простояла в луче
  float ufoPos[3], ufoCharge; int ufoOn, ufoSent, ufoKey;
  // сверка g07 (02.11.2026): состояние мира, что ставит main (переходы) и что считает миссия
  int transition, arrested; char entering[24];   // идёт вход/выход из помещения; арест; интерьер, в который входит игрок (начало перехода)
  int hudHidden;                                 // P06-21: ShowHUD("false")
  char hideName[24];                             // P06-21: SetCharacterToHide этапа — на выходе из этапа персонаж убирается
  int safeIn;                                    // P06-13: игрок в машине внутри зоны AddSafeZone (полиция стоит)
  int trafficStage;                              // P03-Д5: лимит трафика этапа (-1 — как на уровне); пешком набирается до полного
  float ufoY, ufoT; int ufoGrab; char ufoCar[8]; // UFO_BEAM: машина игрока тянется лучом (grab 1 — тянется, 2 — забрана)
  int carHidden;                                 // круг 5, P06-15
  int buyTried;                                  // круг 5, P06-24: купленная машина уже ставилась игроку на этом этапе
  float missionClock;                            // №1114: секунды основной части текущей миссии (лучшее время в записи, savegame.h SvMrec)
  float hitTgtT; int aiWaitSeen[16];             // круг 2: окно после удара игрока по цели; счётчик waitEvt по машинам миссии
  float navTx, navTz; int navOn;                 // круг 2, g02 P16-06: куда строился путь навигации
  float ufoHome[4]; int missionDone;             // круг 3: где стояла машина до луча; миссия завершена (для правки «выданная машина»)
  float forcedPt[3];                             // круг 3: точка выданной машины
  int animStale;                                 // круг 2, P15-15: жесты прошлого этапа живут, пока новый этап не задаст свои
  float dmgHold; int dmgHoldOn;                  // круг 2, P07 Д6: неуязвимость машины игрока на время цели dump с несколькими предметами
  unsigned aiArrived;                            // g03 A10: счёт кадров с событием «ИИ добрался» (служебно, пока включено MC_G03_FIELDS)
  float forcedStart[3]; int forcedStartOn;       // P (5.2): где стоял Гомер, когда миссия выдала машину
} GameMsn;

// Звук миссий (msn_audio.h): поставить ДО gameMsnOpen; NULL — без звука (разговоры кончаются сами, как раньше)
void gameMsnSetAudio(const MsnAudioBackend *b);
// дороги и машины трафика уровня (traffic.shr) для машин миссий: путь по полосам, удары о трафик; NULL — едут прямо к точкам
void gameMsnSetRoads(Traffic *t);
extern int gameMsnDeferStart;   // №943: см. game_msn.c
void gameMsnResumeStart(int on);   // RESUME/LOAD: первый старт миссии пешком ставит машину рядом с Гомером (game_msn.c carNearHomer)
// машины миссии: место игрока в гонке (1 — первый; 0 — не гонка с соперниками), всего соперников
int gameMsnRacePos(const GameMsn *m, int *racers);
float gameMsnRaceFracLen(const GameMsn *m);   // 03.10: доля пройденного пути гонки игрока по длине (догонялка соперников); < 0 — не гонка
// машина миссии у игрока (SetForcedCar: L1 m6 marge_v) — имя модели пакета msncars; NULL — машина уровня
void gameMsnCarjack(GameMsn *m, const char *model, const CarParams *P, float lift, float x, float y, float z, float h);   // правка 63
GameHnrConfig gameMsnHnrConfig(const MsnFile *f);
void gameMsnHnrPolice(GameMsn *m, int on);
void gameMsnHnrPoliceAdd(GameMsn *m);   // сверка g09b-menu круг 5 (P03-08): ставит ОДНУ машину погони (проход раз в 0,7 с)
int gameMsnHnrPoliceLimit(const GameMsn *m);   // сколько машин погони положено уровню (SetNumChaseCars)
void gameMsnHnrPoliceFar(GameMsn *m, float px, float pz, float R);   // снимает по одной машины погони дальше R м от игрока
void gameMsnHnrPoliceDrop(GameMsn *m, int i);
int gameMsnHnrPoliceSuspend(GameMsn *m);   // круг 7: машины погони уходят в запас с позой; -> сколько
void gameMsnHnrPoliceResume(GameMsn *m);   // и возвращаются на те же места   // круг 7: снять одну машину погони по номеру
void gameMsnHnrPoliceHold(GameMsn *m);   // №627: полиция после ареста стоит на месте
void gameMsnPolice(GameMsn *m, int on, const char *car, const char *con, int n);   // правка 64: погоня Hit & Run
int gameMsnSummon(GameMsn *m);   // правка 62: будка — назад на машину уровня (0 — машина миссии, нельзя)
int gameMsnPhoneCar(GameMsn *m, const char *car);   // правка 94: машина из галереи будки (1 — сменилась)
void gameMsnWorldCar(GameMsn *m, const char *car, const float p[3], float h);   // правка 53: машина мира (ракетная)
void gameMsnWorldCarsReset(void);
void gameMsnWorldCarsStep(GameMsn *m, float px, float pz);
// №1114: лучшее время и попытки миссий (как у оригинала). Слот записи: сюжетные 0..7 по номеру, уличные гонки 8..10, бонус-миссия 11, гонка на ставку 12.
// Хук ставит игра (main_loop.h: svAttemptNote / svBestNote из savegame.c), в ботах и тестах он свой или пуст. kind 0 — миссия начата, 1 — пройдена (sec — время основной части)
extern void (*gameMsnRecordHook)(int level, int slot, int kind, int sec);
int  gameMsnBestSlot(const GameMsn *m, int cur);           // -1 — миссия не ведёт записи
void gameMsnRecord(GameMsn *m, int kind);   // сверка g10b-loop круг 5 (P03-20): свободная машина уходит и возвращается с зоной (200 м, 10 м от машины миссии)
int  gameMsnWorldCarNear(GameMsn *m, float x, float z, float r, float out[4]);   // -> номер или -1; out: x y z h
int  gameMsnTakeWorld(GameMsn *m, int i);                                      // 1 — игрок пересел
int  gameMsnHnrOff(void);   // перепроверка 96: DisableHitAndRun миссии
// ---- сверка g07 (02.11.2026): точки для других модулей (подробно — game_msn.c и docs/handoff/СВЕРКА_ВНЕДРЕНИЕ/g07-missions_журнал.md)
void gameMsnSetState(GameMsn *m, int transition, int arrested, const char *entering);   // main: идёт переход в интерьер / арест / интерьер, в который входят (начало перехода)
int  gameMsnShopAllowed(const GameMsn *m);    // лавка и гардероб (main_loop.h:774) — только в вступлении и вне миссий
void gameMsnTalkNpcBack(GameMsn *m, int on);   // сверка g07 круг 7 (КАМЕРА 4.3): на крупном плане игрока (pc_near) собеседник стоит на 1,8 м дальше от игрока; on — план pc_near
const float *gameMsnTalkNpcBase(const GameMsn *m);   // место собеседника без сдвига (камера разговора считает по нему) или NULL
int  gameMsnDumpHitStrong(float J);   // круг 7: 1 — удар достаточно сильный, чтобы цель dump уронила предмет (импульс не меньше 5000)
int  gameMsnAbortAllowed(const GameMsn *m);   // 0 — AllowMissionAbort("false"): Accept/Cancel вступления не показывать
int  gameMsnTalking(const GameMsn *m);        // идёт разговор: шкала H&R и счёт стоят
int  gameMsnHudHidden(const GameMsn *m);      // ShowHUD("false")
void gameMsnSetSkinNameFn(const char *(*fn)(void));   // main: имя надетого костюма (замок "locked" "skin")
extern int gameMsnCarHide;                             // круг 5: 1 — машину игрока не рисовать (диалог бонус-миссии)
int gameMsnBoothBlocked(const GameMsn *m);              // круг 5: будка закрыта (миссия провалена, арест)
int gameMsnBoothsOff(const GameMsn *m);   // ps2-twin P05:25: будки выключены (миссия с принудительной машиной в ходе); viewer/phonebusy.h
extern void (*gameMsnSafeHook)(int active);           // полиция: 1 — игрок в машине внутри AddSafeZone, погоню остановить; 0 — продолжить
const char *gameMsnForcedCar(const GameMsn *m);
// машина-цель этапа (SetObjTargetVehicle: destroy / dump / follow / losetail / гонка против неё), NULL — нет
const McCar *gameMsnTargetCar(const GameMsn *m);
int gameMsnRouteArrow(GameMsn *m, const float tgt[3], float p[3], float *h);
// сверка g07b-msnview круг 2 (P16-06): путь навигации целиком по дорогам трафика (traffic.h trafficNavBuild/Arrows); roadsT приватный — две обёртки. Build — один раз при старте цели (0 — пешком/нет дорог)
int gameMsnNavBuild(GameMsn *m, float tx, float tz);
int gameMsnGroundY(GameMsn *m, float x, float z, float *y);   // правка 122: высота земли (заборы трассы гонки)   // правка 122: стрелка на дороге к цели (1 — есть)

int  gameMsnNavArrows(GameMsn *m, TrNavArrow *out, int max);   // круг 2, g02 P16-06: стрелки пути по дорогам (зелёные впереди, красные пройденные) для вида

// 0 = ок; msn_level_start (первая сюжетная миссия уровня — m0). view может быть NULL (боты)
int gameMsnOpen(GameMsn *m, Game *g, const void *pack, size_t size, const MsnGameApi *view, char *err, int errlen);
// то же с загрузкой сохранения: resume — прогресс из записи (NULL — как gameMsnOpen). Пройденные миссии восстанавливаются,
// текущая сюжетная — с чекпоинта; запись не этого уровня / вне границ пакета — старт уровня (без падения)
int gameMsnOpenAt(GameMsn *m, Game *g, const void *pack, size_t size, const MsnGameApi *view, const MsnProgress *resume,
                  char *err, int errlen);
// Правка Роба 335 (05.10): сохранение разработчика — resume->story = GAME_MSN_STORY_DEV (в записи не хранится, ставит main.c при
// загрузке DEV SAVE): пройдены все сюжетные миссии уровня, кроме последней (её даёт персонаж — свободная езда, как между миссиями);
// переход к следующему уровню (m8 L2/L5/L6/L7) не тронут; бонусы/гонки пройдены, в DEV доступен повтор у персонажа. Правило порта
#define GAME_MSN_STORY_DEV 0xFEu
extern int gameLvProgOld;   // облако 05.10 (карьера): 1 — прогресс миссий только текущего уровня (прежнее; flag lvprogold 1)
uint32_t gameMsnDevMask(const MsnFile *f);   // маска «всё, кроме последней сюжетной» для пакета уровня
extern unsigned (*gameMsnClockUs)(void);
extern float gameMsnPartMs[5];   // служебное: подфазы gameMsnTick, мс (сумма)
void gameMsnTick(GameMsn *m, const GamePad *p, float dt);   // MsnWorld из игры -> msn_game_tick; действие = △ этого кадра
int gameMsnWantsAction(GameMsn *m);
const char *gameMsnActionLabel(GameMsn *m, char *buf, int n);   // правка 206: подпись к △ миссии        // 1 — △ этого кадра принадлежит миссии (не посадке в машину)
int gameMsnWantsMissionAction(GameMsn *m);   // то же без продавца: △ нужен самой миссии (разговор, точка, бонус) — боты
void gameMsnWorld(const GameMsn *m, MsnWorld *w);   // MsnWorld из игры (без кнопки)
const GameNpc *gameMsnNpc(const GameMsn *m, const char *name);
extern int gameMsnNpc984Old, gameMsnNpcChk, gameMsnNpcTime; extern unsigned gameMsnNpcCalls, gameMsnNpcMiss, gameMsnNpcBad, gameMsnNpcUs;   // №984: кэш поиска персонажа по имени (game_msn.c)
// правка 140: кому из не миссионных (фоновые, продавцы) дать модель — индексы m->npc, ближние к (x, z) первыми; -> сколько
int gameMsnAmbWant(const GameMsn *m, float x, float y, float z, int max, int *want);
// №444: same world/nearest policy with a predictive loading radius; legacy callers retain 60 m
int gameMsnAmbWantRange(const GameMsn *m, float x, float y, float z, float range, int max, int *want);
// твёрдые персонажи (homer.solid): стейтпропы прежним обработчиком + капсулы персонажей миссий на том же уровне
int gameMsnSolid(void *m, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
// Отсчёт 3-2-1 этапа (AddToCountdownSequence / StartCountdown скрипта): игрок стоит, как в оригинале — ввод движения
// не проходит, машина на ручнике; управление возвращается на GO. Frozen: 1 — сейчас идёт отсчёт
int gameMsnKick(GameMsn *m, float x, float y, float z, float r, float dx, float dz);   // №843: пинок Гомера по персонажам миссии -> сколько сбито
int gameMsnFrozen(const GameMsn *m);   // и во время разговора (игрок стоит лицом к собеседнику, как в оригинале)
// разговор: номер звучащей реплики (0..) или -1; *pcSpeaks — говорит игрок; *anim — жест говорящего/слушающего этой
// реплики по данным (AddAmbient*Animation: NULL/"none" — без жеста), who: 0 игрок, 1 собеседник
int gameMsnTalkLine(const GameMsn *m, int *pcSpeaks);
const char *gameMsnTalkAnim(const GameMsn *m, int who, int line);
// Камера старта фазы (правка Роба 40, «наезды»): фаза задаёт камеру (SetMissionStartCameraName в mNsdi, SetAnimatedCameraName
// в srNi / grNi), и сценарий на её старте ставит игрока (SetMissionReset*, SetInitialWalk, PlacePlayerCar — старт уровня,
// MISSION SELECT, загрузка, старт гонки). Конец каждой камеры данных — в 3–10 м от этого места и смотрит на него
// (tests/camera/msncam_check.py), а в записи PS2 при обычном переходе к следующей миссии (игрок никуда не переносится —
// 3:20, 6:10, 12:45) облёта нет, есть только на старте уровня (0:58) — отсюда правило. Заявка одна на фазу:
// 1 — есть (имя фазы и камеры), дальше 0 до следующей фазы
int gameMsnStartCam(GameMsn *m, const char **phase, const char **cam);
// отмена миссии игроком (брифинг Cancel, разговор перед миссией △ Cancel): msn_abort — назад к вступлению миссии; камеру
// старта этот перезапуск не играет (игрок сам отказался, облёта в ответ на отказ нет)
void gameMsnAbort(GameMsn *m);
// В85: Cancel карточки разговора перед миссией (msn_view.c mvTalkInput): gameMsnAbort + оба на свои места до разговора, собеседник
// стоит; △ миссии не принимается GAME_TALK_RETALK с. GAME_TALK_CANCEL_OLD=1 — только gameMsnAbort (прежнее)
void gameMsnTalkCancel(GameMsn *m);
int gameMsnInSafeZone(const GameMsn *m, float x, float z);   // 1 — точка в зоне AddSafeZone текущего этапа
void gameMsnFreezePad(const GameMsn *m, GamePad *p);    // во время отсчёта: кнопки и стик — только START (пауза)
// Лавка продавца (правило порта: пешком, △ у продавца в радиусе AddPurchaseCarReward + SHOP_REACH): открыта — номер продавца
// в m->shop.seller, иначе -1. Пока открыта, игрок стоит (gameMsnFrozen), кнопки — лавке: ◄ ► выбор (m->shop.sel — k-й
// предмет каталога продавца, shopSellerItem), X — Purchase (монеты списываются, предмет принадлежит; мало монет или уже
// куплено — отказ, m->shopDenied++), ○ / △ — Cancel
int gameShopOpen(const GameMsn *m);
void gameMsnSetSkinTarget(int (*fn)(float px, float py, float pz, float out[3]));   // метка этапа buyskin: ближайший гардероб/дверь (NULL — метки нет)
void gameShopInput(GameMsn *m, const GamePad *p);   // кнопки лавки (вид, пока экран лавки поверх мира): ◄ ► X ○ △
void gameMsnFreezeCar(const GameMsn *m, CarInput *in);  // во время отсчёта: газ, тормоз, руль 0, ручник
extern int gameMsnCarNearOld;   // №1073: 1 — машина миссии к герою с любого расстояния (как до правки)
extern void (*gameMsnNote)(const char *line);   // №1073: строка в журнал (msn_view.c ставит logf_), NULL — молча
extern int gameMsnResetAlways;           // правка 61: отрицательный контроль (1 — переносить и после пройденной миссии)
extern int gamePoliceDmgOld;   // правка 280: 1 — полицейская получает полный урон машин миссий (прежнее)
extern int gameTalkCarOld;   // правка 305: 1 — машина переносится на место разговора из данных (прежнее, красный)
extern int gameShopFirstOld;   // правка 306: 1 — лавка перехватывает △ у собеседника этапа (прежнее, красный контроль)
// №967 (отчёт ФИНАЛ_БОСС_L7): цель «destroyboss» (m3, m5, m6, m7 L7: «въехать на машине в луч тарелки и выйти») ждала MSN_IN_BOSS_HIT, а слать его было некому — этап кончался «Timed out».
// Теперь тарелка и луч стоят в точке локатора AddFlyingActorByLocator (l7_spaceship), а тик gameMsnTick шлёт событие, когда машина простояла в луче BOSS967_CHARGE секунд и игрок вышел.
// Числа (радиус луча 11 м, заряд 3 с) — «правило порта, не PS2»: ориентир hit-and-run-web/src/campaign/runtime.ts (MIT), записей PS2 по L7 нет. Провала за долгое сидение в машине нет:
// в данных миссии его нет (условия damage, keepbarrel, timeout). BOSS967_OLD=1 / «flag boss967old 1» / файл boss967old рядом с EBOOT — как было (тарелки нет, событие не шлётся)
#ifndef BOSS967_OLD
#define BOSS967_OLD 0
#endif
#define BOSS967_R 11.f
#define BOSS967_CHARGE 3.f
extern int gameBoss967Old;
extern int gameSellerAtLoc;   // правка 288: 1 — место разговора продавца на его локаторе (прежнее, красный контроль)
#endif
