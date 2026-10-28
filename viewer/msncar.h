// Машины миссий с ИИ (29.09): соперники гонок, цели погонь и тарана, преследователи. Один код для EBOOT (viewer/game_msn.c ->
// msn_view.c рисует) и ботов на Маке (tools/bots/world.c). Чистый C99, без PSP и без malloc; всё состояние — в MsnCars
// (без указателей: снимок мира ботов копирует структуру целиком).
//
// Что из данных диска (скрипты миссий, .con, дороги уровня), что — правило порта (в данных нет, сверить с эталоном PS2):
//   * AddStageVehicle(имя, локатор, ИИ, "<путь>.con", водитель): машина на локаторе с курсом локатора; физика — .con водителя
//     (carParamsForAi: масса, газ, TopSpeedKmh, руль, сцепление, HitPoints), кузов и колёса — модель машины (пакет
//     msncars-l<N>.shr, tools/msncar_export.py; нет — модель трафика того же имени; нет — famil_v, отмечается geo = 0).
//     Повторный AddStageVehicle того же имени — новая машина на новом месте (L2 m7: четыре «cCellA» подряд).
//   * ИИ: "race" / "target" / "evade" / "waypoint" — едут по точкам этапа AddStageWaypoint (общий список этапа, как у
//     hit-and-run-web: точки этапа получают все машины с ИИ), SetRaceLaps — кругов столько же, что у игрока; "chase" —
//     к игроку и таран (цель losetail); "NULL" — стоит (до ActivateVehicle(имя, локатор|NULL, ИИ)).
//   * Езда — по дорожной сети уровня (полосы TRF1 traffic.shr: Road/RoadSegment диска, дорога односторонняя): путь до
//     следующей точки — кратчайший по полосам (Дейкстра по дорогам), дорога-срез (shortcut) — по SetVehicleAIParams(имя,
//     мин, макс): ПРАВИЛО ПОРТА — срезы разрешены, если макс > 0 (данные: -10/-9 у целей тарана, 50/51 у соперников гонок).
//     Руль — преследование точки на пути впереди (упреждение 5 м + 0.45 с), газ/тормоз — к целевой скорости: TopSpeed
//     .con x множитель догонялки, в повороте не больше sqrt(7 м/с2 * R) с торможением заранее (правило порта).
//   * Догонялка гонки SetStageAIRaceCatchupParams(имя, D, медленно, норма, быстро): отрыв = остаток пути игрока − остаток
//     машины (по точкам этапа); впереди больше D — «медленно», позади больше D — «быстро», между — линейно через «норму».
//     Множитель — к TopSpeed машины (правило порта: смысл полей по именам команды, данных о форме кривой нет).
//   * Догонялка цели SetStageAITargetCatchupParams(имя, мин, макс): игрок ближе «мин» — полный ход, дальше «макс» — ползёт
//     (0.2 TopSpeed), между — линейно (правило порта). "evade": игрок ближе 15 м — +15% (уходит; правило порта).
//   * Удары: машина игрока, машины миссии, трафик и стоящие — carHitCar (импульс, урон по HitPoints .con); урон 1 — разбита
//     (цель destroy засчитана). Удар игрока по машине миссии -> MSN_IN_HIT_VEHICLE (dump: выпадает предмет).
//   * Застряла (газ, скорость < 1 м/с 1.5 с) — назад с обратным рулём 1.2 с; застряла 5 раз подряд или перевёрнута, а
//     игрок дальше 60 м — на ближнюю точку своего пути (правило порта, как возврат пропов вне вида).
//   * Во время отсчёта 3-2-1 машины миссии стоят на ручнике, как игрок.
#ifndef SHAR_MSNCAR_H
#define SHAR_MSNCAR_H
#include "col.h"
#include "car.h"
#include "traffic.h"
// Правка 121, L1_ФИНИШ п.3 (30.09 22:3x): поведение полиции по трассам оракула (6e62872: появление через 0.75 с, стоп в 30 м от стоящего
// игрока, заново через 8–11 с, после ареста машина в точку дороги) снято по хирургии шкалы — до естественного опыта П2 в игре прежнее
// поведение; новое включается сборкой с -DHNR_ORACLE_POLICE=1
#ifndef HNR_ORACLE_POLICE
#define HNR_ORACLE_POLICE 1   // 01.10: включено — близнец/оракул подтвердил (knowledge/facts/ps2-hnr-chase.yaml, ступень 4): погоня, без штрафа до ареста
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MC_MAX 6          // машин миссии одновременно (данные: не больше 4 в одной миссии, tools/missions: AddStageVehicle)
#define MC_WP 16          // точек пути этапа (данные: до 14 AddStageWaypoint в этапе)
#define MC_ROUTE 64       // точек маршрута по полосам до следующей точки пути (дальше — перепланировка у конца)
#define MC_AVOID 4        // дорог в объезд (где машина застревала)
enum { MC_PARKED = 0, MC_RACE, MC_TARGET, MC_EVADE, MC_CHASE, MC_WAYPOINT };
extern const char *const mcModeName[6];

typedef struct {   // геометрия модели машины (пакет msncars-l<N>.shr или TRF1): как TrModel
  char name[16]; float lift, wheelRad, halfW, halfL, bodyOfs, wheelX, wheelF, wheelR, bodyLo, bodyHi;
  unsigned char info[128]; int hasInfo;   // MCR1 v2: блок машины CAR1 v2 (car.h carParamsFromInfo) — машина игрока SetForcedCar
} McGeo;
// каталог пакета msncars (MCR1): hdr — заголовок 32 Б, dir — байты файла со смещения каталога hdr[4] (каталог и за ним таблица
// машин v2), bytes — сколько их. -> моделей в out (0 — не MCR1 / битый)
int mcGeoFromPack(const unsigned char *hdr, const unsigned char *dir, unsigned bytes, McGeo *out, int cap);
const McGeo *mcGeoFind(const char *name);   // из таблицы mcSetGeo; NULL — нет в пакете

typedef struct {
  char name[16], con[48], driver[16];
  int on, mode, conOk, geoOk;       // conOk: .con водителя найден; geoOk: кузов из модели (0 — famil_v)
  Car car;
  float lift;                       // низ колёс под началом модели (рисование)
  int wp, passed, finished;         // следующая точка пути (с кругами), последняя пройденная (-1), доехал до конца
  float route[MC_ROUTE][3]; int nroute, seg, routeWp, routeCut, offroad;   // маршрут к точке routeWp; routeCut — обрезан
  float replanT, stuckT, backT, backSteer, still, progRef[3], progT; int progWp;
  float subBase, subFullT, subD, subCalmT; int subTier;   // подшаг физики по расстоянию до игрока (msncar.c aiSubstep): исходный, сколько ещё держим полный после контакта, ступень
  short avoid[MC_AVOID]; int navoid;   // дороги, где застревала (объезд)
  int holdSkip;                    // стоящие на отсчёте: счётчик кадров (RACE_START_LIGHT_OLD)
  int stuckN, asleep, turn3;       // turn3: задний ход — часть разворота в три приёма (путь не меняется)
  float rc[4]; int hasRc;           // SetStageAIRaceCatchupParams: D, медленно, норма, быстро
  float rs[3];                      // 03.10: где машина начала путь этапа (первый отрезок доли гонки для догонялки)
  int rk, rAhead; float rDist;      // правка 326: пройдено точек гонки игрока, впереди игрока (-1 — ещё не считали), до следующей, м
  float tc[2]; int hasTc;           // SetStageAITargetCatchupParams: мин, макс
  int skLo, skHi, hasSk;            // SetVehicleAIParams
  float baseTop, baseGas, mult;     // TopSpeed и газ .con; множитель этого кадра
  float wheelAng, vt;               // колёса (рисование); целевая скорость этого кадра
  CarInput lastIn;                  // ввод водителя последнего кадра (следы шин skid.c, правка 249)
  float vcap;                       // аудит 09.10 п. 10: потолок цели по догонялке (множитель × скорость игрока); 0/1e9 — нет
  float offLane;                    // отход от оси полос этого кадра, м (гейт)
  float shiftF;                     // объезд машины впереди: сдвиг точки руля вбок, м (> 0 — влево), сглаженный
  float pushT, pushSide;            // упёрлась в стоящую и сдала назад: столько секунд объезд до упора в эту сторону (+1 влево)
  unsigned resets, replans;
  float goalAt[3]; char ignore[16];
  int careful;                      // бот-игрок (часть race 30.09): машине игрока бьют все удары (урон car.c, порог 2 м/с) — к машине
                                    // впереди (трафик, стоящие, соперники) не толкаясь, 1.5 м/с; у машин миссии 0 — как было
  int hnrWreck;                     // правка 121: разбитая полицейская уже засчитана в Hit & Run (25) — второй раз не считается
  int police;                       // правка 121 (оракул PS2): полиция Hit & Run — к стоящему игроку ближе POLICE_STANDOFF не подъезжает
  float dbgGap, dbgSwerve; int dbgKind;   // служебное: зазоры этого кадра и чья машина впереди (гейт, журнал)   // бот-игрок (mcBotInput): точка, к которой проложен путь; машина-цель тарана
} McCar;

typedef struct {
  McCar c[MC_MAX]; int n;
  float wp[MC_WP][3]; int nwp, laps;
  unsigned hits;                    // ударов игрока по машинам миссии (счёт)
} MsnCars;

typedef struct {   // окружение кадра
  const ColWorld *w;
  Traffic *tr;                      // дороги и машины трафика (NULL — нет: машины едут прямо к точкам)
  Car *player;                      // машина игрока (удары меняют и её)
  int driving;                      // игрок за рулём
  float ppos[3];                    // где игрок (машина или Гомер)
  int hold;                         // отсчёт 3-2-1: стоять
  int haveFrac; float pfrac;        // 03.10: доля пройденного пути гонки игрока по длине (gameMsnRaceFracLen); haveFrac 0 — прежняя мера
  int noNpcCollide;                 // бонус-гонка: соперники не сталкиваются друг с другом; контакт игрок↔соперники остаётся
} McEnv;

// Курс машины на локаторе CarStart (AddStageVehicle, ActivateVehicle, PlacePlayerCar, InitLevelPlayerVehicle, SetMissionReset*
// с машиной, места разговора): курс пакета миссий (MsnLoc.heading = −угол файла — так стоят персонажи: пары собеседников
// m2_barney/m2_homer_barney, bm1_cletus_sd/bm1_player, m7 Бёрнс/Гомер смотрят друг на друга) + π. Проверено данными диска:
// старт гонки (PlacePlayerCar, AddStageVehicle) смотрит на первую точку своего пути 12 из 12 на уровне 1 и 73 из 73 на 2–7
// (с курсом пакета — назад), машины на оси полосы (≤ 1 м: m1_skinner_place, m7_smithers_carstart, sr2_AI_carstart2–4 …) —
// по ходу полосы; то же правило у старта уровня (tools/level_sweep.find_spawn). Гейт — tests/aicar (сборка -DMC_BREAK_HEADING=1)
float mcCarHeading(float locHeading);
// модели машин уровня (пакет msncars): геометрия по имени. Таблица живёт у вызывающего (static), n = 0 — нет
void mcSetGeo(const McGeo *g, int n);
void mcInit(MsnCars *m);
McCar *mcFind(MsnCars *m, const char *name);
// AddStageVehicle; h/hasH — курс машины (mcCarHeading от курса локатора). -> машина или NULL (мест нет)
McCar *mcSpawn(MsnCars *m, const ColWorld *w, const Traffic *tr, const char *name, const float pos[3], float h, int hasH,
               const char *ai, const char *con, const char *driver);
void mcActivate(MsnCars *m, const ColWorld *w, const char *name, const float *pos, float h, int hasH, const char *ai);
void mcWaypoint(MsnCars *m, int index, const float p[3], int laps);   // AddStageWaypoint: index 0 — новый путь этапа
void mcParams(MsnCars *m, const char *name, int lo, int hi);           // SetVehicleAIParams
void mcCatchup(MsnCars *m, const char *name, int kind, const float *p, int n);   // 0 — гонка (4 числа), 1 — цель (2)
void mcClear(MsnCars *m);
void mcRemove(MsnCars *m, const char *name);   // машина миссии уходит (игрок сел в машину этого имени: SetForcedCar)                                             // конец миссии / повтор: машины уходят
int mcMode(const char *ai);
// кадр: после машины игрока и Гомера, до трафика. hit(u, имя) — игрок ударил машину миссии (скорость удара > 1 м/с)
extern unsigned (*mcClockUs)(void); extern float mcPartMs[3];   // служебное: подфазы mcUpdate (смоук)
void mcUpdate(MsnCars *m, const McEnv *e, float dt, void (*hit)(void *u, const char *name), void *u);
// бот-игрок (tools/bots/missioner): водитель машин миссий ведёт машину игрока к goal по полосам; c — тень водителя бота
// (c->car = копия машины игрока этого кадра). in — ввод машины (газ/тормоз 0..1 — бот переводит в нажатия R/L)
void mcBotInput(const MsnCars *m, McCar *c, const McEnv *e, const float goal[3], float dt, CarInput *in);
// то же по готовым точкам пути (бот за машиной-целью её же следом: pts — след, последняя — цель); без планировщика полос
void mcBotInputPath(const MsnCars *m, McCar *c, const McEnv *e, const float (*pts)[3], int n, float dt, CarInput *in);
// правка 420 (бонус-игра): свой планировщик маршрута ИИ вместо полос трафика — (user, машина, цель, out, cap) -> точек маршрута
// (out[0] — первая, последняя — цель; 0 — прямо к цели). NULL (по умолчанию) — как было: mcPlan по дорогам e->tr
typedef int (*McPlanHook)(void *u, const McCar *c, const float g[3], float (*out)[3], int cap);
extern McPlanHook mcPlanHook; extern void *mcPlanUser;
// водители трафика видят машины миссии (тормозят перед ними, как перед игроком): до trafficUpdate каждого кадра; m = NULL —
// машин нет
void mcTrafficSees(const MsnCars *m, Traffic *t);
// Гомер пешком (HomerSolidFn): кузова машин миссии твёрдые, крыша — опора
int mcSolid(const MsnCars *m, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
// гонка: остаток пути до финиша по точкам этапа, м (машина; игрок — от точки p); место игрока среди машин с ИИ (1 — первый)
float mcRemaining(const MsnCars *m, const McCar *c);
float mcRemainingFrom(const MsnCars *m, const float p[3]);
int mcRacePosition(const MsnCars *m, const float p[3], int *racers);
// место в гонке считает game_msn.c: доля пройденного пути — у машины по её точкам этапа (с кругами), у игрока — по его точкам
// гонки (AddCollectible рантайма миссий), сравнение долей (пути игрока и ИИ — разные списки точек)
float mcRaceFrac(const MsnCars *m, const McCar *c);
// правка 300 (04.10): место игрока — по остатку пути до финиша в метрах. Игрок: по прямой до своей следующей точки гонки
// (pts — AddCollectible этапа, next — номер с кругами) и отрезки между оставшимися точками (с кругами) — mcPathRemaining;
// соперник — то же по его точкам этапа (mcRemaining). Место = 1 + соперники, кому до финиша меньше (mcRacePlace)
float mcPathRemaining(const float (*pts)[3], int n, int laps, int next, const float p[3]);
int mcRacePlace(const MsnCars *m, float prem, int *racers);
// прежняя мера (правка 178, красный контроль): доли по ЧИСЛУ точек — игрок (next + доля отрезка от sx,sz для первой) / n·laps
// против mcRaceFrac соперника. mcRacePosOld = 1 — game_msn.c считает так (сценарий: flag raceposold 1)
extern int mcRacePosOld;
float mcPathFracCount(const float (*pts)[3], int n, int laps, int next, float sx, float sz, const float p[3]);
int mcRacePlaceOld(const MsnCars *m, float fp, int *racers);
// правка 326 (Роб 04.10: «гонка — прям беда: ехал рядом со Скиннером — не понимает, где кто первый, не в реальном времени»;
// указание Роба: «траекторий много, но просто: кто ближе к точке — тот и первый»). ПРАВИЛО ПОРТА по слову Роба: одна мера для
// всех — точки гонки ИГРОКА (AddCollectible этапа, круги SetRaceLaps). Соперник впереди, если прошёл больше этих точек, а при
// равенстве — ближе (по прямой, xz) к той же следующей точке; каждый кадр; гистерезис MC_RACE_HYST (полоса 1 м: ±0,5) только
// против дрожания. Точки соперника: c->rk — пройдено точек гонки игрока (in — внутри точки так же, как игрок, NULL — ближе WP_R;
// одну обойдённую точку засчитывает вместе со следующей: путь соперника — свои точки этапа); финиш (последняя точка) — только
// когда соперник доехал до конца своего пути (c->finished: кто выиграл, решает условие миссии race).
// Что показывает оригинал (запись PS2, без формулы): knowledge/facts/ps2-race-position.yaml
#define MC_RACE_HYST 1.f
typedef int (*McInPt)(void *u, int k, const float p[3]);   // p внутри точки гонки k (с кругами)?
int mcRaceAhead(int rk, float dr, int pk, float dp, int prev);   // соперник впереди игрока? prev: 1 / 0 — прежнее, -1 — нет
// pk — точек, пройденных игроком (с кругами), p — игрок; *dp — до его следующей точки, м. -> место (1 — первый; 0 — соперников нет)
int mcRacePlaceRob(MsnCars *m, const float (*pts)[3], int n, int laps, int pk, const float p[3], McInPt in, void *u, float *dp,
                   int *racers);
extern int mcRacePos300;   // 1 — место по остатку метров (правка 300; красный контроль 326: flag racepos300 1 / -DRACEPOS_300=1)
extern int mcLedgeOld;   // правка 347: 1 — путь ИИ и вверх по уступам срезов (прежнее; flag ledgeold 1 / -DMC_LEDGE_OLD=1)
extern int mcLedgeFwdOld;   // правка 486: 1 — по ходу дороги уступ не судить (прежнее правило 347; -DMC_LEDGE_FWD_OLD=1)
extern int mcBotPassOld;   // правка 347: 1 — бот careful не обгоняет соперника (прежнее; MISSIONER_PASS_OLD=1)
extern int mcBotRamOld;   // правка 347: 1 — бот-таран тормозит и под машины ДАЛЬШЕ цели в коридоре (прежнее; MISSIONER_RAM_OLD=1)
// служебное (гейт tests/aicar): маршрут по полосам от (x, z, курс h) до точки g -> точек (0 — нет дорог), out — точки,
// *offroad = 1, если точка g дальше 30 м от полос (едет к ней напрямую)
void mcPlanAvoid(const short *r, int n);   // служебное (гейт tests/aicar/roadplan.c)
extern int mcRoadPlanOld;
extern int mcPolice723Old;   // №723: 1 — полиция подъезжает к стоящему игроку вплотную (как до правки); 0 — не ближе 27 м, как PS2
extern int mcAiSubstepOld;   // 1 — подшаги ИИ-машин всегда полные (AI_SUBSTEP_OLD)   // 1 — прежний поиск пути planRoadsOld (ROADPLAN_OLD)
int mcPlan(const Traffic *tr, float x, float y, float z, float h, const float g[3], int shortcuts, float out[][3], int cap, int *offroad);
int mcResetPoint(const Traffic *t, float x, float y, float z, float h, float vx, float vz, float out[4]);   // RESET CAR по дорогам: точка и курс на оси ближней полосы по ходу; 0 — нет
float mcLaneDist(const Traffic *tr, float x, float z);   // до оси ближней полосы (любой дороги), м; 1e9 — дорог нет
// дороги, чью ось перекрывает коллизия уровня (путь ИИ — в объезд): проверить все сразу при загрузке уровня (иначе — по требованию
// в первом пути через дорогу: на PSP до 70 мс за кадр). -> сколько перекрыто
int mcProbeRoads(const Traffic *tr, const ColWorld *w);
extern void (*mcProbePump)(void);   // правка 244: подкачка кадра экрана загрузки во время mcProbeRoads (NULL — нет)
// Загрузка 2, пункт 1: запечённый результат mcProbeRoads (mcpr.shr, секция MCP1; tools/mcp_bake.c)
#define MCP_FILE_BYTES (32 + 256 + 256 + 512 * 4 + 512 * 4)
unsigned mcProbeHash(const void *p, unsigned n);
unsigned mcProbeBakeBuild(const Traffic *t, unsigned colHash, unsigned char *out);   // после mcProbeRoads; out >= MCP_FILE_BYTES
int mcProbeBakedLoad(const Traffic *t, unsigned colHash, const void *blob, unsigned size);   // -> перекрытых дорог, -1 — не подошло
int mcProbeStateSame(const unsigned char *blob);

#ifdef __cplusplus
}
#endif
#endif
