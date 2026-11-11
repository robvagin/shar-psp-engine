// Игра без экрана для ботов на Маке (пункт Б0, docs/БОТЫ-ИГРОКИ_2026-09-28.md): тот же C-код логики, что EBOOT
// (viewer/game.c + car, col, props, stateprops, coins, homer, skin), шаг строго 1/60 с, без часов реального времени.
// Мир целиком в BotWorld: save/load — копия структуры и изменяемых массивов модулей (пропы, монеты, стейтпропы).
// Отличия от EBOOT (перечислить, не прятать): нет рисования, звука, частиц, интерфейса; RESET CAR — по START (как без
// ui.shr); «вне вида» для возврата предметов, спавна и удаления трафика — конус камеры за машиной (как chase-камера, без
// точной пирамиды кадра). Живой город (Т1.2): трафик, пешеходы, стоящие машины — viewer/traffic.c (build/traffic.shr рядом
// с уровнем; BW_NO_TRAFFIC=1 — без него), кадр в том же порядке, что main.c; состояние — в снимке и отпечатке мира.
// Один трафик на процесс (как коллизия и комнаты): несколько BotWorld в одном процессе делят его.
#ifndef BOTS_WORLD_H
#define BOTS_WORLD_H
#include <stddef.h>
#include <stdint.h>
#include "game.h"
#include "game_msn.h"
#if __has_include("animobj.h")   // правка 116: объекты ANM1 шагают в мире ботов (в старой копии viewer/ для гейта Т1.4 их нет)
#include "animobj.h"
#define BW_HAVE_AO 1
#endif

#ifndef BW_DT   // сверка g01-cars №1126: шаг кадра можно подменить при сборке замерщика (tests/g01cars/streets.c, неровные кадры)
#define BW_DT (1.f / 60.f)
#endif
typedef struct {   // счётчики событий за всё время (для счёта ботов и сводок)
  unsigned propKicks, spChanges, coins, homerEvents, resets, frames, rooms;   // rooms — входов в комнаты
  unsigned trEvents;   // события трафика (удары машин, остовы, сбитые пешеходы, стоящие) -> шкала Hit & Run
  float maxWallV;
} BotStats;
typedef struct {
  Car car; Homer homer; Props props; StateProps sp; Coins coins;
  float hnrHeat, hnrCalm; int wasDestroyed, spStInit; unsigned char spSt[512];
  int haveCol, haveProps, haveSP, haveCoins, haveHomer; float lift;
  unsigned prevButtons; BotStats st;
  Game G;
  GameMsn msn; int haveMsn;   // миссии уровня (build/missions/level01.msn), если пакет дан
  int room, lastDoor, doorArmed, exitArmed;   // комната Гомера (-1 — город), последняя дверь, вход/выход взведены (пункт 11.2)
  Collect coll; int haveColl;   // В5: карточки, осы, гэги (collect-l<N>.shr рядом с level.shr; BW_NO_COLLECT=1 — без них)
  int haveTraffic;              // Т1.2: живой город (трафик, пешеходы, стоящие машины)
#ifdef BW_HAVE_AO
  // правка 116 (30.09, m2 ингалятор на крыше грузовика Duff): объекты уровня с действиями — подъёмник по △, двери, платформы АЭС —
  // шагают тем же viewer/animobj.c, что EBOOT (△ у выключателя, кадр клипа, живые объёмы COL1). BW_NO_AO=1 — как до правки
  // (стоят; двери AutomaticDoor открыты, правка 108). Живые объёмы лежат в общей коллизии процесса — снимок хранит их копию
  AnimObjs ao; int haveAo, aoAct;
#endif
} BotWorld;
typedef struct { BotWorld w; PropState *pst; Coin *coin; void *spmem, *collmem, *trmem, *aomem; size_t npst, ncoin, nspmem, ncollmem, ntrmem, naomem;
  float **spjm, **colljm; unsigned nspjm, ncolljm; } BotSnap;   // 04.11: jm0 обломков (№923) — отдельные выделения внутри sp.mem, снимок хранит свои копии

extern int bwPropsSolid;   // 1 — пропы твёрдые для Гомера пешком, как в игре (по умолчанию 0)
int bwData(const char *level, const char *homer, const char *msn);   // один раз: уровень (COL1, PRP2, CAR1), Гомер, пакет миссий (NULL — без); 0 = ок
const char *bwCarName(void);   // машина уровня из CAR1 v2 (famil_v, honor_v, …); без неё — famil_v
int bwCarSource(void);         // carParamsFromCar1: 1 — машина из CAR1 v2, 0 — CAR1 v1, < 0 — битая (famil_v)
int bwLevel(void);   // номер уровня (MSN1 @6, без миссий — BW_LEVEL, иначе 1): спутники collect-l<N>.shr, interiors-l<N>.bin рядом с level.shr
void bwMissionPacks(int *carGeo, int *audio);   // кузовов машин миссий из msncars-l<N>.shr рядом с уровнем; звук миссий (audio/) загружен
int bwNew(BotWorld *w);                                 // новая игра на спавне уровня, Гомер за рулём
void bwFree(BotWorld *w);
void bwStep(BotWorld *w, unsigned buttons, float ax, float ay);   // один кадр 1/60 с; кнопки — биты GP_* (как PSP_CTRL_*)
void bwSaveAlloc(const BotWorld *w, BotSnap *s);        // выделить снимок под этот мир (один раз)
void bwSave(const BotWorld *w, BotSnap *s);             // мир -> снимок
void bwLoad(BotWorld *w, const BotSnap *s);             // снимок -> мир (тот же мир, что был сохранён)
void bwSnapFree(BotSnap *s);
extern unsigned long long bwTotalFrames;   // кадров bwStep за процесс (BW_FRAMES=1 -> строка "F\t..." в конце stdout)
uint64_t bwHash(const BotWorld *w);                     // отпечаток состояния мира побайтно
const ColWorld *bwCol(void);
int bwRooms(void);
int bwRoomAt(float x, float y, float z);                // комната, в которой точка (-1 — город)
int bwRoomDoor(int room, float out[3]);                 // дверь комнаты в городе; 0 — нет
void bwRoomExit(int room, float out[3]);                // локатор выхода внутри комнаты                                      // комнат уровня загружено (interiors-l<N>.bin; BW_NO_ROOMS=1 — без них)
void bwSpawn(float *x, float *y, float *z, float *h);
void bwPlace(BotWorld *w, float x, float y, float z, float h);   // машина в точку (как goto сценария), скорость 0, Гомер за рулём
// Где точка на уровне (ключ кластера, Т1.1): опоры коллизии над XZ сверху вниз; слой = сколько опор ниже той, на которой
// точка стоит (опора — не дальше 2.5 м ниже и 0.6 м выше точки, под центром или под одной из 4 точек в 1.5 м: край крыши
// остаётся крышей; 0 — нижняя, земля; 1+ — крыша, эстакада, навес над ней; -1 — опоры рядом нет); obj — самый
// глубокий примитив в квадрате ±r на высоте 0.2..2 м над опорой: "b12" ящик, "c3" цилиндр, "t45" треугольник, "s6" забор,
// "-" — ничего. -> слой; *nlayers — опор над XZ всего. Этаж и объект разделяют кластеры в одной клетке 8 м
int bwWhere(float x, float y, float z, float r, int *nlayers, char obj[16]);
// Можно ли дотянуться от машины a до машины b (поимка догонялок, Т1.1): один этаж (перепад < 2 м) и между ними нет стены
// (столбик 0.3 м на высоте 0.5–1.5 м над линией через каждые 0.5 м, забор с любой стороны). 1 = да
#define BW_REACH_DY 2.f
int bwReachable(float ax, float ay, float az, float bx, float by, float bz);
// Живой город (Т1.2): трафик процесса (NULL — нет); машина-препятствие для гейта — стоящая машина из данных (место 0)
// переставлена в (x, y, z) курсом h; чужая машина бота (погоня догонялок) бьётся о трафик и стоящие тем же carHitCar
const void *bwTraffic(void);
int bwTrafficCounts(int *cars, int *peds, int *parked);   // сейчас едет машин, идёт пешеходов, стоящих мест; 0 — трафика нет
void bwPlantCar(float x, float y, float z, float h);
void bwHitTraffic(Car *c);
extern unsigned long long bwPhaseNs[8]; extern int bwProf;   // №984 стенд city984: время фаз bwStep, нс (при bwProf = 1)
extern int bwLastDeathZone;  // Observation only: actual gameDeath return in last bwStep, -1 otherwise.
#endif
