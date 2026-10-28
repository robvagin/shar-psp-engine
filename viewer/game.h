// Логика кадра игры без рисования (пункт Б0): машина, Гомер, пропы, стейтпропы, монеты, шкала Hit & Run. Один код для
// EBOOT (viewer/main.c) и ботов на Маке (tools/bots): порядок вызовов в кадре — как в main.c, числа те же.
// Состояние живёт у вызывающего (main.c — свои static, tools/bots — одна структура мира для save/load), здесь указатели.
// Звук, частицы и интерфейс — через GameHooks (на Маке NULL). Чистый C99 без PSP.
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "col.h"
#include "car.h"
#include "props.h"
#include "stateprops.h"
#include "coins.h"
#include "homer.h"
#include "collect.h"

// кнопки как в pspctrl.h (PSP_CTRL_*), чтобы бот и PSP говорили на одном языке
enum { GP_SELECT = 0x1, GP_START = 0x8, GP_UP = 0x10, GP_RIGHT = 0x20, GP_DOWN = 0x40, GP_LEFT = 0x80, GP_L = 0x100, GP_R = 0x200,
       GP_TRIANGLE = 0x1000, GP_CIRCLE = 0x2000, GP_CROSS = 0x4000, GP_SQUARE = 0x8000 };
typedef struct { unsigned buttons, hit; float ax, ay; } GamePad;   // hit — нажаты в этом кадре; стик −1..1, мёртвая зона уже снята

typedef struct {   // всё, что не влияет на игру: звук, частицы, подсказки HUD (NULL — пропустить)
  void (*propKick)(void *u, unsigned i, const float pos[3], float carV, float dx, float dz);   // сбит предмет PRP1: звук, частицы ATC
  void (*spChange)(void *u, unsigned i, float speed);   // стейтпроп сменил состояние: звук
  void (*spFx)(void *u, const SpFxOut *o);              // частица стейтпропа (очередь всё равно разбирается)
  void (*homerEvent)(void *u, const HomerEvent *e);     // звук, музыка, пыль событий Гомера
  void (*afterHomer)(void *u, const HomerCar *hc);      // подсказка △ в HUD
  void (*coin)(void *u);                                // монета подобрана
  void (*door)(void *u, float open);                    // дверь водителя (шарнир CAR1)
  void (*reset)(void *u, int wasOnFoot);                // RESET CAR: музыка
  void *u;
  void (*wrench)(void *u, const float pos[3], int repaired);   // гаечный ключ взят (пункт 4): звук wrench_collect, частицы; repaired — машина была разбита
} GameHooks;

typedef struct {
  ColWorld *pWorld; Car *pCar; Props *pProps; StateProps *pSp; Coins *pCoins; Homer *pHomer;
  int *pHaveCol, *pHaveProps, *pHaveSP, *pHaveCoins, *pHaveHomer;
  float *pHnrHeat, *pHnrCalm; int *pWasDestroyed;
  unsigned char *pSpSt; int *pSpStInit;   // прошлые состояния стейтпропов (512) и «уже запомнены»
  const float *pLift;                     // CAR1 lift: где сидит Гомер (CarVis.lift)
  GameHooks hk;
  const ColWorld *pHomerWorld;   // в конце: позиционные инициализаторы оставляют NULL; Гомер в комнате (пункт 11.2): её коллизия; NULL — город (машина и предметы всегда в городе)
  Collect *pColl;                // В5: карточки, осы, гэги (viewer/collect.c); NULL — без пакета collect-l<N>.shr
  int collRoom, collAction;      // комната Гомера для гэгов: collectRoomId + 1 (0 — город, как у нулевой инициализации); △ этого кадра отдан гэгу (сбрасывает шаг)
  int noEject;                   // служебное (flag noeject, отрицательный контроль): взрыв не выбрасывает Гомера
  int footPadOld;                // служебное (flag oldfootpad, отрицательный контроль tests/controls): прежняя раскладка
                                 // порта пешком — стик сразу бег 8 м/с, ○ — стомп в воздухе (до 29.09)
  // гаечные ключи (пункт 4; точки — JMP1 COL1, модель — тип wrench SPR1 уровня): взят (бит k), сколько с «вне вида» для
  // возврата, экземпляр SPR1 модели ключа k (-1 — модели нет), счётчики
  uint32_t wrenchTaken; float wrenchAway[32]; short wrenchInst[32]; int wrenchMapped; unsigned wrenches, wrenchRepairs;
  // Hit & Run (правка 121, правила исполняемого файла PAL — game.c gameHnrOffense): погоня защёлкнута (шкала дошла до 100, держится
  // до спада ниже 5), выключено сценарием (DisableHitAndRun), игрок в комнате, идёт арест (с), машина, в которую игрок сел последней
  int hnrLatched, hnrOff, hnrRoom; float hnrBusted; char hnrCar[16]; int hnrCarSet;
  int exitReq;   // правка 168: △ на ходу — выход заказан: машина тормозит сама, на малой скорости Гомер выходит (△ ещё раз — отмена)
  int coinBoomArmed; // №441: wallet penalty only on a new boom, not loading a husk
} Game;
// нарушения Hit & Run: удар машины, разбитая машина, предмет, пешеход, пинок пешехода (шкала 100: 5 / 25 / 7.5 / 10 / 13)
enum { HNR_CAR_HIT, HNR_CAR_WRECK, HNR_PROP, HNR_PED_HIT, HNR_PED_KICK };

int gameOnFoot(const Game *g);
int gamePropKick(void *g, unsigned i, float dx, float dz);   // props.onKick (kickUser = Game*): шкала, обломок SPR1
void gameTriangle(Game *g);                                 // △: выйти на малой скорости / сесть
extern int gameResetRoadOld;                                  // 1 (-DRESET_ROAD_OLD=1): RESET CAR в последнее безопасное место, как до 11.10
extern int (*gameResetRoad)(const Car *c, float out[4]);       // ближняя полоса по ходу, ставит game_msn.c
void gameReset(Game *g);                                    // RESET CAR: машина рядом на колёса, остов чинится, Гомер за рулём
#define GAME_DEATH 1   // есть gameDeath (мир ботов зовёт её только тогда: откат правки гейтом Т1.4 собирается)
int gameDeath(Game *g);   // правка 86: зона DEATH оригинала под игроком (машина или Гомер пешком) -> в её точку; номер зоны, -1 — нет
// правка 349: то же по шагам (main.c: проверка -> затемнение -> перенос): все объёмы локатора, курс машины — курс точки возврата
int gameDeathAt(Game *g);            // номер записи DEATH под игроком, -1 — нет
void gameDeathPut(Game *g, int k);   // перенос в точку записи k (машина и Гомер — курсом локатора, скорость 0; камера Гомера — с лица, В86)
void gameDeathOld(int on);           // 1 — как до 349: первый объём локатора, курс прежний (main.c и мир ботов: красный контроль)
#define GAME_DEATH_FOOT 1   // есть gameDeathFootOld (мир ботов из старой копии viewer/ собирается и без неё)
void gameDeathFootOld(int on);       // ДОБИВКА 18: 1 — пешком курс прежний и камера за спиной (как до В86; красный контроль)
int gameDeathIsOld(void);
void gameCarInput(const Game *g, const GamePad *p, CarInput *in);   // R/X газ, L тормоз, □ ручник, стик/← → руль
void gameCarLimits(const Game *g, CarInput *in);
void gameExitAssist(Game *g, CarInput *in);   // правка 168: заказанный выход — тормоз, потом выход            // остов стоит, пешком водителя нет
void gameCarStep(Game *g, const CarInput *in, float dt);    // carUpdate
void gameAfterCar(Game *g, const GamePad *p, float dt);     // Гомер и его удары, пропы, стейтпропы, монеты
void gameRespawn(Game *g, float ex, float ez, float dt, int (*seen)(void *, float, float, float, float), void *u);   // возврат вне вида
void gameHnr(Game *g, float dt);                            // спад шкалы Hit & Run (SetHitAndRunDecay L1 3.0 ед./с на шкале 100)
extern int gameHnrChasePauseOld;                           // №723: 1 — в погоне пауза спада 3 с как до правки
void gameHnrRate(Game *g, float dt, float rate);            // то же с заданной скоростью: погоня 6/с, комната 10/с (docs/gameplay.md §6)
int gameHnrOffense(Game *g, int type);                      // нарушение HNR_*: 1 — засчитано (шкала, пауза спада)
void gameHnrVehicle(Game *g, const char *name);             // игрок сел в машину name (NULL — машина уровня): другая, чем прошлая, — шкала в ноль
// Арест Hit & Run (правка 121; №430 — сверка с 7 арестами PS2, game.c ARREST_PS2_OLD). Порог близости и скорости — правила
// исполняемого файла PAL (ps2-hnr-police): ближе 10 м, медленнее 30 км/ч, 0,75 с подряд. Записями подтверждены 10 м (сбросы
// 0x6121dc на 9,93–10,06 м) и 0,75 с (716–770 мс перед арестом); сброса по скорости в записях нет. Пешком 1,5 с (замера нет,
// правило порта). main.c hnrPursuit и гейт tests/hnr/arrest_ps2.sh зовут одни и те же функции
#define HNR_BUST_R 10.f
#define HNR_BUST_V (30.f / 3.6f)
#define HNR_BUST_CAR .75f
#define HNR_BUST_FOOT 1.5f
void gameHnrChaseStart(float *spawnT, float dt);   // кадр защёлки погони: таймер появления первой полицейской машины
int gameHnrSpawnDue(float *t, float dt);   // таймер появления полиции: 1 — пора ставить машину в этом кадре
// счётчик близости: armed — погоня с полицией, не в комнате, не идёт арест; dist — до ближайшей полицейской, м; speed — игрока, м/с.
// 1 — арест в этом кадре (счётчик обнулён)
int gameHnrCatch(float *nearT, float dt, int armed, float dist, float speed, int onFoot);
float gameHnrBustTime(void);       // таймер ареста, с: G.hnrBusted в кадре ареста
int gameHnrBustTick(Game *g, float dt);   // ход таймера ареста: 1 — кончился в этом кадре (перенос машины на дорогу)
// Гаечный ключ (пункт 4): игрок — машина или Гомер пешком — у ключа (сфера ActionEvent + запас) — ключ исчезает, машина игрока
// чинится целиком, в том числе остов (библия: «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A PHONEBOOTH, TO REPAIR YOUR
// VEHICLE»); возвращается, как предметы, через 5 с вне вида и дальше 60 м (gameRespawn). В gameAfterCar; -> ключей взято за кадр
int gameWrenchStep(Game *g);
#endif
