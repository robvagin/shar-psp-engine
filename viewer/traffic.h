// Трафик, стоящие машины и пешеходы (этап 5): данные build/traffic.shr (tools/traffic_export.py, формат TRF1), чистый C99,
// без GE-состояний, кроме skinDraw у пешеходов. Машины рисует main.c своим drawPieceM через колбэк (текстуры TRF1 дописываются
// в tex[] main.c), пешеходы — viewer/skin.c (скиннинг на GE, общий блоб CHR1 для всех 25 моделей).
//
// Что делает (по данным уровня; числа оригинала, где известны, иначе «правило порта» — в данных нет):
//   * пул движущихся машин = группа трафика 0 из leveli.mfk (ур. 1: minivanA x2, glastruc, schoolbu, pickupA = 5);
//     спавн на кольце 65 м вокруг точки «игрок + 40 м вперёд», при сбросе — от 20 до 100 м (как TRAFFIC_RULES
//     hit-and-run-web/src/traffic.ts, MIT), только вне кадра или дальше дальности отрисовки; удаление > 75 м от той же
//     точки вне кадра (правило порта: и > 130 м в любом случае). Правка 328: места — точки по длине полосы, проверка в самой
//     точке рождения; вне кадра камеры и камеры «за игроком» (после разговора/переноса), впереди не ближе 50 м, при сбросе — тоже;
//     через 1,5 с машина ещё в круге удаления (traffic.c spawnCars; прежнее — trfSpawnOld, «flag trfspawnold 1»);
//   * езда: полосы дорог (Road/RoadSegment), скорость дороги из данных (ур. 1: 50 км/ч), в повороте не больше
//     sqrt(3 м/с2 * R); чистое преследование точки в 4 м впереди; через перекрёсток — кубическая Безье до полосы
//     следующей дороги (выход случайный, без разворота, если есть другой); дистанция до машины/игрока/Гомера/пешехода
//     впереди (тормоз до 6 м/с2, стоп за 3 м); перекрёсток с trafficBehaviour 1 — остановка у края на 1 с и проезд,
//     когда в перекрёстке никого (правило порта, значение behaviour не описано); 0/3 — без остановки, но ждут занятый
//     перекрёсток; ждёт дольше 4 с — едет (правило порта против взаимной блокировки);
//   * удар игроком — carHitCar (импульс, урон по HitPoints .con), дальше машина — твёрдое тело (carUpdate с тормозом) до
//     остановки, стоит 1.5 с и возвращается на ближнюю полосу (до 4 м, курс ±60°), иначе стоит до удаления; урон 100% —
//     остов huskA (level.mfk);
//   * стоящие машины: CarStart-локаторы с флагом 1 (ур. 1: 93 места), модели с третьим аргументом AddTrafficModel = 1
//     (glastruc, schoolbu); правило порта: занято столько мест, сколько моделей пула с флагом стоянки (L1 — 2, как у оригинала; TRAFFIC_PARK_FEW_OLD=1 — заняты все), модель по FNV-1a имени места (экспортёр, trafficParkModel).
//     Спят (без физики), пока их не толкнули: удар -> carUpdate с тормозом до покоя 1 с -> снова спят. Сдвинутая дальше
//     1 м и вне кадра дальше 100 м от игрока возвращается на место (правило порта, как возврат пропов);
//   * твёрдые: машины (движущиеся и стоящие) — carHitCar с машиной игрока; для Гомера — trafficSolid (HomerSolidFn:
//     кузов-прямоугольник, крыша — опора); пешеходы — круг 0.35 м до роста модели (правило порта); пешеход ждёт, пока
//     Гомер стоит у него на пути, и сам сквозь Гомера не идёт;
//   * пешеходы: до TR_MAXPED, по замкнутым путям (Path), модели из активной группы (ворота-зоны kind 13, старт — 0),
//     ходьба с playback по скорости корня клипа; машина игрока быстрее 3 м/с — полёт (npd_flail), приземление
//     (fall_land), лежит 1 с, get_up, идёт к пути; медленнее — отталкивает; пинок Гомера — тот же полёт, слабее;
//   * Hit & Run: шкала у game.c (0..1); здесь только события с долей (удар машины 0.05, остов 0.25, пешеход 0.10,
//     пинок 0.13 — числа gameplay.md §6 / 100), main.c прибавляет их к hnrHeat.
// Бюджет (аудит шаг 9): LOD <= 400 треуг. дальше TR_LOD_DIST (30 м), машины дальше drawDist (90 м) не рисуются,
//   пешеходы — до pedDrawDist (60 м), позы дальше 25 м — через кадр.
#ifndef SHAR_TRAFFIC_H
#define SHAR_TRAFFIC_H
#include <stdint.h>
#include <math.h>
#include "col.h"
#include "car.h"
#include "skin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TR_MAXCAR 5
#define TR_MAXPED 6
#define TR_MAXMODEL 8
#define TR_MAXPEDMODEL 32
#define TR_MAXEXTRA 6             // машин миссий, которых видят водители трафика (viewer/msncar.h MC_MAX)
#define TR_MAXAWAKE 4             // стоящих машин с физикой одновременно (остальные ждут, правило порта: бюджет CPU)
#define TR_LOD_DIST 30.f
// Правка 139 (Роб 01.10: «слишком много припаркованных машин»): правило по данным — стоящая машина существует, только пока
// загружен файл её зоны (уличные загрузчики l<N>_terra, tools/traffic_export.py dyna_loaders); меняется только вне кадра.
// ЗА ФЛАГОМ, по умолчанию ВЫКЛЮЧЕНО — включать после замера на железе (строка чёрного ящика trf/park/ped) и слова Роба.
#ifndef TRAFFIC_ZONE_PARK
#define TRAFFIC_ZONE_PARK 0
#endif
#define TR_PED_R .35f             // круг пешехода (правило порта)

// ---------------------------------------------------------------- файл TRF1 (little-endian, смещения от начала файла)
typedef struct {                        // 256 Б
  char magic[4];                        // "TRF1"
  uint32_t version, size;
  uint32_t nJunc, offJunc, nRoad, offRoad, nPt, offPt, nOut, offOut;
  uint32_t nModel, offModel, nPiece, offPiece, vertOff, vertBytes;
  uint32_t nTex, offTex, texBytes, nSlot, offSlot;
  uint32_t nPath, offPath, nPathPt, offPathPt, nZone, offZone, nGroup, offGroup;
  uint32_t nPed, offPed, pedOff, pedBytes, huskModel, nPark, offPark;
  uint32_t nLoader, offLoader;          // правка 139: ворота уличных загрузчиков зон (TrZone, group = маска файлов); 0 — нет
  uint32_t offLight;                    // @156 (29.09, свет уровней): блок LGT1 (viewer/levlight.h) — свет уровня, которым экспорт
                                        // запёк светотень машин трафика; 0 — файл без блока. Движку не нужен (персонажам свет — из CAR1)
  uint32_t offSeat;                     // @160 (№666): nModel x {f32 x, y, z} — сустав dl (сиденье водителя) в осях модели, м; y = 99 — у модели нет; 0 — старый файл без блока
  uint32_t pad[23];
} TrHeader;
typedef struct { float x, y, z, r; uint32_t behaviour; uint16_t firstOut, nOut, nIn, pad16; uint32_t pad; } TrJunc;   // 32 Б
typedef struct {                        // 32 Б; дорога односторонняя: полоса k — pt[firstPt + k*nPt .. +nPt)
  uint16_t startJ, endJ, rev, lanes;    // rev — дорога обратно (0xFFFF нет)
  uint8_t flags, maxCars, speed, pad8;  // flags 1 shortcut, 2 без трафика; speed км/ч
  uint32_t firstPt; uint16_t nPt, pad16; float length, width, pad;
} TrRoad;
typedef struct { float x, y, z, s; } TrPt;                   // s — путь от начала полосы, м
#ifndef TRF_SEAT_OLD
#define TRF_SEAT_OLD 0   // №666: 1 — блок сидений TRF1 не читается (угнанная машина сажает героя по запасному famil_v)
#endif
typedef struct {                        // 128 Б; модель смотрит в -Z, низ колёс на -lift
  char name[16];
  uint16_t firstPiece, nPiece, firstLod, nLod;
  float lift, wheelRad, halfW, halfL, bodyOfs, wheelX, wheelF, wheelR, bodyLo, bodyHi;
  float wheel[4][3];                    // центры колёс w0..w3 в осях модели
  float camElev; uint32_t tris, lodTris; float camMag;   // правка 198: FollowCameraData модели (ближний вариант); 0 — нет (старый пакет)
} TrModel;
#ifndef TRF_LOD_DIST_OLD
#define TRF_LOD_DIST_OLD 0
#endif
static inline float trLodDist(const TrModel *M) {
#if TRF_LOD_DIST_OLD
  (void)M;
  return TR_LOD_DIST;
#else
  // №558: адаптивная дистанция переключения LOD.
  // Базовая дистанция 42 м (вместо 30 м) для компактных машин.
  // Для больших машин (автобус, грузовики) масштабируется
  // пропорционально размеру (до 65 м), чтобы переключение на подробную версию
  // происходило заблаговременно вдалеке, а не перед самым носом (на 30 м).
  float sz = fmaxf(M->halfL, M->halfW);
  float dist = fmaxf(42.f, 42.f * (sz / 2.2f));
  return dist > 65.f ? 65.f : dist;
#endif
}
typedef struct {                        // 80 Б = Rec level.shr (main.c memcpy в Piece.r): tex — номер текстуры TRF1 (-1 нет)
  float bb[6]; int tex, flags, nv, ni, owner; float ox, oy, oz, sc; uint32_t off, nf, cell, wheel, lod;   // wheel: 0 кузов, 1..4 колесо
} TrPiece;
typedef struct { uint16_t model, park; } TrSlot;             // park = третий аргумент AddTrafficModel (1 — модель для стоянки)
typedef struct { uint32_t first, n; float length, cx, cz, rad; uint32_t pad[2]; } TrPath;   // замкнутая ломаная
typedef struct { float c[3], ax[3], az[3], h[3]; uint32_t group, pad[3]; } TrZone;         // ворота группы пешеходов
typedef struct { uint8_t n, pad[3], model[4], count[4], pad2[4]; } TrGroup;
typedef struct { char name[16]; uint32_t hdrOff; float height; uint32_t pad[2]; } TrPedModel;
typedef struct { float x, y, z, h; char name[12]; uint16_t model, zmask; } TrPark;        // 32 Б: место стоящей машины;
                                                                                           // zmask — файлы зон места (0 — всегда)

// ---------------------------------------------------------------- состояние
enum { TRM_OFF, TRM_DRIVE, TRM_PHYS, TRM_WRECK };
typedef struct {
  int mode, model;
  // маршрут: elem 0 — полоса дороги (road, lane, s), 1 — кривая через перекрёсток к (nroad, nlane); next — выбран ли выезд
  int elem, road, lane, nroad, nlane, next, junc; float s;
  float bz[4][3], bzLen, bzTab[9];                            // Безье и таблица длины (9 точек)
  float waitT, v, vt, wheelAng, stillT, hitCool;
  float hitV, stunT; int ramStrong, honkDone;   // №479: сила удара (сближение, м/с), сильный ли он, сигнал уже дан
  float rearX, rearZ; int rearOk;   // №400: задняя ось тянется за началом машины (wheelR), 0 — не задана
  Car car;                                                    // твёрдое тело (carHitCar; после удара — carUpdate)
} TrCar;
typedef struct { int state, wreck; float stillT; Car car; int gone, off; } TrParked;   // state: 0 спит, 1 физика; gone — вне зоны (правка 139)
enum { TRP_OFF, TRP_WALK, TRP_STAND, TRP_FLY, TRP_DOWN, TRP_GETUP, TRP_DODGE };   // TRP_DODGE — уклонился от машины (правка 180)
typedef struct {
  int mode, model, path, seg, dir; float s;
  float x, y, z, h, vx, vy, vz, t, speed, blockT, nearCool;   // nearCool — с прошлого уклонения (правка 180)
  int poseTick, test;                                         // test: служебный пешеход сценария (стоит, не удаляется)
  float noGroundT;                                              // №594: сколько подряд идёт без земли под ногами
  unsigned char gTick;                                          // PROP_FAST: счётчик кадров для проверки земли раз в 4 кадра (PED_GROUND_FAST_OLD)
  int avoidSide; float avoidT;                                 // №510: сторона обхода игрока/пешехода (±1) и сколько она ещё держится
  float waitT;                                                 // №692: сколько подряд путь перекрыт (стоит в ожидании), с
  float orbBest, orbStall, orbTx, orbTz;                       // №813: обход без продвижения к точке пути (лучшее расстояние, сколько с без продвижения, точка)
  SkinPlayer sp;
} TrPed;
enum { TR_EV_CAR_HIT = 1, TR_EV_CAR_WRECK, TR_EV_PED_HIT, TR_EV_PARK_HIT, TR_EV_PED_KICK, TR_EV_PED_NEAR, TR_EV_CAR_HONK };   // CAR_HONK (№479) — не нарушение, только гудок   // PED_NEAR — не нарушение (шкалу не двигает)
// amount — доля шкалы Hit & Run (0..1); who — правка 240: имя модели человека для голоса (прохожий: «boy3», персонаж миссии:
// «grandpa», viewer/sound.c sndPedVoice), "" — у события нет человека. index < 0 — персонаж миссии (game_msn.c: -1 - номер)
typedef struct { int type, index; float x, y, z, amount; char who[16]; } TrEvent;
enum { TRC_WALK, TRC_RUN, TRC_IDLE, TRC_FLAIL, TRC_GETUP, TRC_DODGE, TRC_CRINGE, TRC_LAND, TRC_N };

typedef struct {
  unsigned char *base; unsigned size; int own;
  const TrHeader *hd; const TrJunc *junc; const TrRoad *road; const TrPt *pt; const uint16_t *out;
  const TrModel *model; const TrPiece *piece; const unsigned char *vert; const TrSlot *slot;
  const TrPath *path; const float *pathPt; const TrZone *zone; const TrGroup *grp; const TrPedModel *pedm; const TrPark *parkd;
  SkinModel peds[TR_MAXPEDMODEL]; int npeds; signed char clip[TR_MAXPEDMODEL][TRC_N]; float walkV[TR_MAXPEDMODEL];
  CarParams params[TR_MAXMODEL];
  TrCar car[TR_MAXCAR]; int maxCars;
  TrParked *park; int npark;
  const Car *extra[TR_MAXEXTRA]; int nextra;   // машины миссий этого кадра (viewer/msncar.c): водители трафика тормозят перед ними,
                                              // как перед машиной игрока; ставит game_msn.c каждый кадр (не часть снимка мира)
  TrPed ped[TR_MAXPED]; int maxPeds;
  int group;                            // активная группа пешеходов
  int zoneGroup, missionPedGroup;        // группа ворот и UsePedGroup; −1 возвращает выбор ворот
  float scanT, pedScanT, time; unsigned seed; int fresh, parkRR;
  float drawDist, pedDrawDist;
  int noSolid;                          // служебное: отрицательный контроль (flag trfnosolid) — трафик не твёрдый
  const TrZone *loader; int nLoader; uint32_t zoneMask;   // правка 139: загрузчики зон и маска загруженных файлов (~0 — все)
  int forceLod;                         // служебное (flag trflod): все машины в дальней версии (снимок LOD вблизи)
  float solidPX, solidPZ; int solidHave; // trafficSolid: где Гомер был в прошлом вызове (шаг — для остановки о пешехода)
  TrEvent ev[16]; int nev;
  // статистика кадра (для HUD/отчёта)
  int activeCars, movingCars, drawnCars, lodCars, drawnPark, awakePark, activePeds, drawnPeds, drawnTris;
} Traffic;

// загрузка файла целиком (одно чтение); 0 = ок, <0 — ошибка. Память: размер файла (1.35 МБ ур. 1) + стоящие машины
int trafficLoad(Traffic *t, const char *path);
int trafficInitMem(Traffic *t, void *data, unsigned size, int own);   // то же из памяти (64-выравнивание); own — free при trafficFree
void trafficFree(Traffic *t);
// текстуры TRF1 в формате level.shr: w, h, число уровней, CLUT (256 x 8888), уровни (swizzle, высота кратна 8)
int trafficTexCount(const Traffic *t);
int trafficStopCount(void);   // №875: концов полос со стоп-линией у зебры (stop875.bin)
void trafficTexInfo(const Traffic *t, int i, int *w, int *h, int *levels, int *pre, void **pal, void *lv[8]);
// всё убрать и поставить стоящие машины на места (старт, NEW GAME, телепорт); движущиеся и пешеходы появятся вокруг игрока
void trafficReset(Traffic *t, const ColWorld *w);
// UsePedGroup из MFK: номер из TRF1, −1 снимает выбор миссии; неверный номер не меняет мир
int trafficPedGroup(Traffic *t, int group);
void trafficRoutePoint(const Traffic *t, const TrCar *c, float ds, float p[3]);   // точка маршрута машины на ds м вперёд (служебное, №400)
// кадр: после физики игрока и Гомера. player — машина игрока (удары меняют и её); driving 0 = игрок пешком
// (ppos — ступни Гомера: машины тормозят, пешеходы ждут). eye/fwd — камера (спавн и удаление вне кадра).
void trafficUpdate(Traffic *t, const ColWorld *w, Car *player, int driving, const float ppos[3],
                   const float eye[3], const float fwd[3], float dt);
// Гомер: HomerSolidFn (homer.h) — вытолкнуть круг из кузовов машин и пешеходов; крыша машины — опора
int trafficSolid(void *traffic, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
// пинок Гомера (сфера x,y,z,r, направление dx,dz): пешеходы в сфере летят; число задетых
// правка 355: неподвижная машина k (оставленная своя): машины трафика и стоянки, задевшие её, отталкиваются (k не сдвигается)
int trafficHitStill(Traffic *t, const ColWorld *w, const Car *k);
int trafficKick(Traffic *t, float x, float y, float z, float r, float dx, float dz);
// служебное (сценарий, тесты): поставить стоящего пешехода модели m на землю в (x, z) курсом h; номер или -1
int trafficPedPlace(Traffic *t, const ColWorld *w, int m, float x, float z, float h);
// служебное (сценарий): машину трафика 0 — на ближнюю к (x, z) полосу с курсом ±60° от h (до 40 м), едет; 0 = ок
int trafficCarPlace(Traffic *t, const ColWorld *w, float x, float z, float h);
// правка 117: ближайшая к (x, z) точка любой полосы не дальше maxd — p и курс по ходу полосы *h (вызов машины у будки); 1 — нашлась
int trafficLaneSpot(const Traffic *t, float x, float z, float maxd, float p[3], float *h);
// угон (правка 63): ближайшая целая машина трафика к x, z (до кузова <= r), её Car и модель; trafficTake — забрал игрок
extern int trfSpawnOld;     // правка 328: служебное
extern int trfNearRadial;   // правка 285: 1 — прежняя мера (центр − полуширина), красный контроль
int trafficNearest(const Traffic *t, float x, float z, float r, int *park, float *dist);
const Car *trafficCarAt(const Traffic *t, int i, int park);
int trafficCarModelAt(const Traffic *t, int i, int park);
void trafficTake(Traffic *t, int i, int park);
// модель стоящей машины по правилу порта (FNV-1a имени места по моделям с park = 1); -1 — моделей нет
int trafficParkModel(const Traffic *t, const char *name);
// отрисовка машин: для каждого куска fn(user, кусок, вершины, матрица модели 4x4 по столбцам) — main.c строит Piece и
// зовёт drawPieceM (стекло уходит в его очередь полупрозрачного). vis(bb[6]) — видимость ящика (NULL = всё видно).
typedef void (*TrDrawFn)(void *user, const TrPiece *p, const void *verts, const float m[16]);
void trafficDrawCars(Traffic *t, const float eye[3], int (*vis)(const float bb[6]), TrDrawFn fn, void *user);
// одна машина по модели формата TRF1 (трафика или пакета машин миссий msncars-l<N>.shr): куски piece и вершины vert — у
// вызывающего; lod — дальняя версия; *tris += треугольников (NULL — не считать)
extern int trHideDriver;   // №666: 1 — trafficDrawModel не рисует куски водителя (owner 4): машина игрока (угнанная / SetForcedCar), водителя выбросили. 0 — рисует (ИИ-трафик)
void trafficDrawModel(const TrModel *M, const TrPiece *piece, const unsigned char *vert, const Car *k, float wheelAng, int lod,
                      TrDrawFn fn, void *user, int *tris);
#ifndef SKIN_HOST
// пешеходы: skinDraw (свет/текстура/кости — см. skin.h), после — main.c resetState()
void trafficDrawPeds(Traffic *t, const float eye[3], int (*vis)(const float bb[6]));
#endif
// события (удары, остов, пешеход) — для звука/HUD/Hit & Run; 0 = очередь пуста
int trafficPopEvent(Traffic *t, TrEvent *e);
// Правка 240 (Роб 03.10 14:51: «не сбиваются люди вообще никакие»): удар машины по человеку — ОДИН механизм для прохожих трафика и
// персонажей миссий (viewer/game_msn.c: фоновые, продавцы, бонусные, персонаж миссии вне разговора).
//   trafficCarHitsBody: машина k против круга человека (*x, *z, радиус r) на высоте y. 2 — сбит: человек вынесен из кузова,
//     v — скорость отлёта; 1 — машина медленнее TR_PED_HIT_V: человек вытолкнут из кузова; 0 — не касается.
//     Отлёт (правило порта, грубая оценка по записи PS2 7:11,1–7:11,3 и 9:55,2–9:55,7: сбитый улетает от кузова вбок-вперёд и
//     виден рядом с машиной, под машину не уходит): по нормали удара — (1 + TR_KNOCK_E) скорости сближения + TR_KNOCK_MIN,
//     вдоль — TR_KNOCK_T касательной скорости машины, вверх — 3 + 0.15 скорости (не больше 8). Прежнее (-DTR_KNOCK_OLD=1,
//     красный контроль tests/npc/knock.sh): 0.8 скорости машины + 2 м/с по нормали — машина догоняет сбитого и проходит сквозь
//   trafficBodyFly: шаг полёта сбитого (тяжесть, приземление на опору коллизии; стена дома, ящик, столб на пути — дальше вбок не
//     летит, падает у стены; заборы COL1 не держат, как машину с изнанки): 1 — приземлился (p[1] — земля, v = 0), -1 — летит дольше
//     4 с (улетел за край мира), 0 — летит; *t — время
//   trafficGetUpLen: длина клипа get_up рига прохожих (npd), с; нет клипа — 1
//   trafficPushEvent: событие в очередь трафика (main.c / боты: Hit & Run, голос who, удар по телу)
#define TR_PED_HIT_V 3.f          // машина быстрее — сбивает, медленнее — отталкивает
#ifndef TR_PED_LIE_OLD
#define TR_PED_LIE_OLD 0   // 1 — прежнее: лежит 1 с
#endif
#define TR_PED_DOWN (TR_PED_LIE_OLD ? 1.f : 3.f)   // сбитый лежит, с (видео PS2 L1, 15:37–15:41: лежит ~4 с не вставая; 3 с + клип get_up ≈ 1 с)
#define TR_HIT_PED .13f           // доля шкалы Hit & Run за сбитого человека (аудит 09.10: PS2 +13, tw_v14_multi30; было .10 по gameplay.md §6)
int trafficCarHitsBody(const Car *k, float *x, float y, float *z, float r, float v[3]);
int trafficBodyFly(const ColWorld *w, float p[3], float v[3], float *t, float dt);
float trafficGetUpLen(const Traffic *t);
void trafficPushEvent(Traffic *t, int type, int idx, float x, float y, float z, float amount, const char *who);
// правка 122: стрелка навигации к цели (px, pz, курс ph) -> (tx, tz) по дорогам трафика: 1 — стрелка в p с курсом *h (перекрёсток ближе maxd)
int trafficRouteArrow(const Traffic *t, float px, float pz, float ph, float tx, float tz, float maxd, float p[3], float *h);

#ifdef __cplusplus
}
#endif
#endif
extern int trfCost1001b;   // №1001б: 1 — pathGap отбрасывает далёкие препятствия заранее (побитно то же); TRFCOST1001B_OLD=1 / flag trfcost1001b 0 — прежний обход
extern unsigned trfProfUs[8]; extern int trfProfOn;   // №1001б: разбивка trafficUpdate по частям (диагностика, строка TRFSUB)
