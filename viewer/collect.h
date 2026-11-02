// Карточки, осы-камеры и гэги уровня (пункт В5, уровень 1 на 100%): логика без рисования, общая для EBOOT (viewer/main.c)
// и ботов на Маке (tools/bots). Данные — пакет CLT1 collect-l<N>.shr (tools/collect_export.py, формат описан там), всё из
// диска: локаторы карточек (ActionEvent CollectorCard), точки ос (wasps.p3d + AddSpawnPointByLocatorScript/AddBehaviour в
// leveli.mfk), блоки GagBegin..GagEnd из level.mfk; модели и анимации — подсекция SPR1 (viewer/stateprops.c: карточка
// card_idle, оса beecamera со своей машиной состояний, сцены гэгов с синтезированными состояниями ждёт/играет/сыграл).
// Звук, частицы, HUD, ролик — у вызывающего по очереди событий collectPop. Чистый C99, без malloc в кадре.
//
// Правила порта (в данных нет, решено здесь; сверить с эталоном PS2):
//   карточка берётся, когда центр игрока (ступни + 0.9 м) ближе радиуса её TriggerVolume + 0.6 м пешком / 1.4 м машиной (как монеты:
//     досягаемость в машине больше); только в городе (карточек в комнатах нет)
//   оса: появляется ближайшая из неубитых, когда игрок ближе её дистанции появления (6-й аргумент), и живых меньше
//     PreallocateActors; дальше дистанции + 15 м — исчезает (не засчитана, появится снова); ATTRACTION: дальше max —
//     летит к игроку (не ниже земли + 1 м) со скоростью из данных, не дальше радиуса полёта
//     от своей точки; dist <= 0 (L2/L5 -1) использует roam для появления/усыпления как правило порта до замера PS2; ближе min — висит; у стоящих (-1) — не летает. Высота полёта: пешком — ступни + 2 м (достаёт
//     прыжок с пинком), в машине — низ колёс + 1 м (слой кузова: машина сбивает). Поворот к игроку — rotSpeed град/с.
//     ATTACK_PLAYER: игрок пешком ближе дальности и в дуге ±atkArc — заряд (s2 из данных), заряжена (s3), выстрел (s4);
//     выстрел в момент FIRE_ENERGY_BOLT данных: Гомер пешком в дальности x1.2 и в дуге ±15° — удар током (homerHurt,
//     монеты выпадают, как у game.c); в машине не стреляет. Уклонение (3-й аргумент) не делаем.
//     пинок/стомп Гомера и удар машины переводят осу в DESTROYED (у beecamera в данных только это событие)
//   dist <= 0 (L2/L5: -1) использует положительный roam для появления/усыпления как правило порта до замера PS2
//   щит осы (AddShield("beecamera", "beeshield") уровней 5–7; поведение щита — код игры, в данных только машина состояний
//     рисунка: s0 появление -> s1 держит, s2 удар (назад в s1), s3 пролом): щит на осе с её появления; пока он цел, пинок/стомп
//     и удар машиной (сближение от 2 м/с в шаре щита, раз в 0.3 с) достаются щиту — COLL_SHIELD_HP ударов: все, кроме последнего,
//     — s2, последний — s3 (пролом, через COLL_SHIELD_GONE_S щит гаснет); после пролома оса бьётся как обычно. Оса уснула — щит
//     снова цел при следующем появлении
//   гэг: touch — вход игрока в сферу триггера; action — △ пешком внутри сферы; random-гэги одной комнаты — при каждом
//     входе в комнату выбирается один (равновероятно, весов в L1 нет); single — сыграл и остался на последнем кадре,
//     больше не запускается; reset — после проигрыша снова ждёт; cycle — после запуска играет по кругу; persist —
//     состояние помнится между входами в комнату, иначе сбрасывается при входе. Монеты (GagSetCoins N, задержка) —
//     только за первый запуск: задержка < 0 — в конце проигрыша, иначе через столько секунд; в городе вылетают
//     из точки гэга, в комнате — сразу в кошелёк (монеты в комнатах не рисуются). Счёт гэгов уровня — гэги с монетами
#ifndef COLLECT_H
#define COLLECT_H
#include <stdint.h>
#include "stateprops.h"
#include "coins.h"
#include "homer.h"
#include "car.h"
#include "col.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WASP_NEAR_OLD
#define WASP_NEAR_OLD 0   // сверка g11b P15-07: подсказка про ос-камер — когда любая оса ближе 5 м к игроку, событие CE_WASP_NEAR (было: при появлении осы; 1 — как было)
#endif
#ifndef WASP_BOB_OLD
#define WASP_BOB_OLD 0   // сверка g11b P04 W11: качание осы ±0,5 м (1 Гц) и дрейф ±0,5 м (период 1,99 с); 1 — оса неподвижна в воздухе
#endif
#define COLL_LEVELS 8
#define COLL_MAXCARD 8
#define COLL_MAXWASP 32
#define COLL_MAXGAG 64
#define COLL_MAXBOLT 64           // port budget; preallocated, no allocation in frame
#define COLL_EVQ 32
#define COLL_CTEX0 0x5000          // номер текстуры SPR1 пакета = COLL_CTEX0 + номер в пакете (main.c texAt)

// ---- записи пакета (размеры сверяются static_assert в collect.c и struct.pack в tools/collect_export.py)
typedef struct { float x, y, z, r; uint16_t id, title, inst, pad; uint8_t pad2[8]; } CollCard;            // 32 Б
typedef struct { char name[16]; float x, y, z, roam, dist, attrMin, attrMax, attrSpeed, atkRange, atkArc, atkEvade, rot;
                 uint16_t inst, flags, shield, pad; float boltSpeed, boltLife; } CollWasp;                                      // 80 Б
enum { COLL_WASP_STATIC = 1, COLL_WASP_SHIELD = 2 };   // flags; shield — экземпляр SPR1 щита (AddShield, уровни 5–7)
typedef struct { char name[16], sound[16], fmv[16]; int16_t inst, skin0; uint16_t nskin; int8_t room;
                 uint8_t cycle, random, trig, sparkle, persist, animCol, coins; uint16_t pad; float coinDelay;
                 float x, y, z, tx, ty, tz, tr, len, shake[3], intro, outro, pad2[2]; } CollGag;                 // 128 Б
enum { COLL_SINGLE = 0, COLL_CYCLE = 1, COLL_RESET = 2 };
enum { COLL_TOUCH = 0, COLL_ACTION = 1 };

// ---- события для звука, частиц, HUD и ролика
enum { CE_NONE, CE_CARD, CE_CARDS_DONE, CE_WASP_SPAWN, CE_WASP_CHARGE, CE_WASP_FIRE, CE_WASP_ZAP, CE_WASP_DEAD,
       CE_WASPS_DONE, CE_GAG_START, CE_GAG_END, CE_GAG_FOUND, CE_GAGS_DONE, CE_WASP_MISS, CE_TICKET,
       CE_WASP_NEAR };   // CE_WASP_NEAR (сверка g11b, P15-07): любая оса ближе 5 м к игроку — подсказка про ос-камер
typedef struct { int type, idx; float x, y, z; } CollEvent;   // idx: карточка / оса / гэг в пакете

// игрок на этот кадр (вызывающий уже решил, кому △: миссии, гэгу или машине)
typedef struct { float x, y, z; int onFoot, room, action; } CollPlayer;   // x y z — ступни Гомера / низ колёс; room: -1 город, иначе collectRoomId

typedef struct {   // изменяемое состояние (боты: копия структуры + sp.mem)
  uint8_t ticketFlags; // conditional gag state from ticket.c (not an ordinary gag completion)
  uint8_t cardGot[COLL_MAXCARD];
  uint8_t waspSt[COLL_MAXWASP];             // 0 спит, 1 живая, 2 убита (засчитана)
  float waspYaw[COLL_MAXWASP]; uint8_t waspShot[COLL_MAXWASP];
  uint8_t shieldHp[COLL_MAXWASP];           // щит осы (AddShield): ударов до пролома, 0 — щита нет/пробит
  float shieldT[COLL_MAXWASP];              // с после пролома (щит гаснет через COLL_SHIELD_GONE_S) / охлаждение удара машиной
  uint8_t gagSt[COLL_MAXGAG];               // 0 ждёт, 1 играет, 2 сыграл (single)
  uint8_t gagFound[COLL_MAXGAG], gagIn[COLL_MAXGAG], gagCoin[COLL_MAXGAG], gagPick[COLL_MAXGAG];
  float gagT[COLL_MAXGAG];
  int lastRoom; unsigned rng;
  float px, py, pz; int pOnFoot, pRoom;     // игрок прошлого шага (collectWantsAction)
  uint8_t cards[COLL_LEVELS]; uint32_t wasps[COLL_LEVELS]; uint64_t gags[COLL_LEVELS];   // прогресс по уровням (для сохранения)
  CollEvent evq[COLL_EVQ]; unsigned nev, evDropped;
  unsigned char shockOn;
  unsigned char boltOn[COLL_MAXBOLT], boltOwner[COLL_MAXBOLT];
  float boltV[COLL_MAXBOLT][3], boltT[COLL_MAXBOLT];
  unsigned boltFired, boltMissed, boltFull;
  unsigned zaps, kills, gagRuns;            // счётчики (отчёты, гейты)
  unsigned shieldHits, shieldBreaks;        // удары по щиту осы, проломы (гейт tests/levelmech/shield.sh)
  // сверка g11b (02.11.2026, P04 W3–W10, Д7, СВЕРКА_ГОМЕР 8.x): агрессия, перелёты и путь снаряда осы; старое поведение — WASP_AGGR_OLD=1 и др. в collect.c
  uint8_t waspAggr[COLL_MAXWASP];           // 0 наблюдает (ATTRACTION), 1 агрессия до конца жизни осы
  uint8_t waspFly[COLL_MAXWASP];            // 0 стоит, 1 перелёт в позицию атаки, 2 уход от игрока, 3 отлёт на 5 м
  float waspTo[COLL_MAXWASP][3];            // точка перелёта
  float waspLastFly[COLL_MAXWASP];          // wclock конца прошлого перелёта; -1e9 — ещё не летала
  float wclock;                             // время осиной логики, с
  float boltRun[COLL_MAXBOLT], boltEnd[COLL_MAXBOLT];   // пройдено и длина пути до проверенной при выстреле стены (0 — стены нет)
  unsigned boltPoolFull;                    // выстрелов не было: пул снарядов занят (W1)
  float waspBob[COLL_MAXWASP][3];           // текущее качание осы (W11): снимается в начале шага, добавляется в конце
  int gagHold;                              // гэг g+1, чей звук ещё идёт (ставит main_collect.h каждый кадр): конец гэга ждёт и анимацию, и звук (P16-14)
  int inMission;                            // 1 — идёт миссия (ставит main_collect.h): гэги с роликом и проверочные в миссии не срабатывают (P16 Д7)
  float pJump;                              // высота игрока над землёй в прыжке, м (0 — на земле или нет Гомера): цель по высоте и «прыгающего снаряд пропускает» (W4/W5, 8.5)
  uint8_t waspNear[COLL_MAXWASP];           // оса ближе 5 м к игроку уже отмечена событием CE_WASP_NEAR (P15-07)
} CollState;

typedef struct Collect {
  int level; unsigned maxAlive;
  const unsigned char *pak; unsigned size;
  const CollCard *card; unsigned ncard;
  const CollWasp *wasp; unsigned nwasp;
  const CollGag *gag; unsigned ngag;
  const unsigned char *tex; unsigned ntex;  // текстуры пакета (формат SHR7), main.c строит по ним Tex
  StateProps sp; int haveSp;
  int kickFx, shockFx, bolt0; unsigned nbolt;
  CollState st;
} Collect;

// pak — весь файл, выровнен на 16, живёт, пока жив Collect (не копируется). 0 = ок, <0 — битый пакет
int  collectInit(Collect *c, const void *pak, unsigned size);
int collectInitResident(Collect *c, const void *pak, unsigned size, int detachedKeys);
void collectFree(Collect *c);                         // spFree; сам pak освобождает вызывающий
const char *collectError(int code);
void collectNewGame(Collect *c);                      // новая игра: прогресс и мир уровня с нуля
int  collectRoomId(const char *interior);             // имя комнаты скриптов -> номер (как tools/interior_export.py), NULL/нет -> -1
// кадр после Гомера/машины/монет (viewer/game.c gameAfterCar): осы, карточки, гэги, SPR1 пакета. homer/car/world/coins — NULL можно
void collectStep(Collect *c, const CollPlayer *p, Homer *homer, Car *car, const ColWorld *world, Coins *coins, float dt);
void collectGagPlay(Collect *c, unsigned g);   // №869 служебно: запустить гэг g (как триггер)
void collectFootKick(Collect *c, const Homer *homer);
void collectHomerHit(Collect *c, const HomerEvent *e);   // HIT / STOMP Гомера: осы в досягаемости -> DESTROYED
int  collectWantsAction(const Collect *c);            // action-гэг под Гомером (по игроку прошлого шага): номер или -1
int  collectPop(Collect *c, CollEvent *e);            // 0 — пусто
int  collectInstRoom(const Collect *c, unsigned inst); // экземпляр SPR1 пакета: -1 город, иначе комната (рисовать там)
int  collectGagOf(const Collect *c, unsigned inst);   // экземпляр -> гэг (-1 — карточка/оса)
int  collectSparkle(const Collect *c, unsigned g);    // 1 — над гэгом блёстки (не найден, GagSetSparkle)

// ---- прогресс уровня (для сохранения и VIEW CARDS / LEVEL PROGRESS): маски и счёт. what: 0 карточки, 1 осы, 2 гэги
enum { COLL_CARDS, COLL_WASPS, COLL_GAGS };
unsigned collectCards(const Collect *c, int level);            // бит k-1 — карточка cardLk
uint32_t collectWasps(const Collect *c, int level);            // бит i — оса i пакета (порядок AddSpawnPointByLocatorScript)
uint64_t collectGags(const Collect *c, int level);             // бит g — гэг g пакета (засчитываются гэги с монетами)
int  collectCount(const Collect *c, int level, int what, int *total);   // собрано; total — всего на уровне (для своего уровня)
void collectSetProgress(Collect *c, int level, unsigned cards, uint32_t wasps, uint64_t gags);   // загрузка сохранения

#ifdef __cplusplus
}
#endif
#endif
