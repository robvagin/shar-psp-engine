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
// сверка g02-traffic, СВЕРКА_ГОМЕР 7.1 (строка 3): одновременно не больше 7 пешеходов (было 6). Память PSP: +1 TrPed (SkinPlayer), порядка
// сотен байт; TR_PED7_OLD=1 — прежние 6
#ifndef TR_PED7_OLD
#define TR_PED7_OLD 0
#endif
#define TR_MAXPED (TR_PED7_OLD ? 6 : 7)
#define TR_MAXMODEL 8
#define TR_MAXPEDMODEL 32
#define TR_MAXEXTRA 6             // машин миссий, которых видят водители трафика (viewer/msncar.h MC_MAX)
#define TR_MAXAWAKE 4             // стоящих машин с физикой одновременно (остальные ждут, правило порта: бюджет CPU)
#define TR_LOD_DIST 30.f
// Правка 139 (Роб 01.10: «слишком много припаркованных машин»): правило по данным — стоящая машина существует, только пока
// загружен файл её зоны (уличные загрузчики l<N>_terra, tools/traffic_export.py dyna_loaders); меняется только вне кадра.
// ЗА ФЛАГОМ, по умолчанию ВЫКЛЮЧЕНО — включать после замера на железе (строка чёрного ящика trf/park/ped) и слова Роба.
// круг 5 (P03-19): включено — это логика без цены для кадра (19 ворот и десяток мест раз в кадр); TRAFFIC_ZONE_PARK=0 — прежнее: места выбраны один раз на уровень
#ifndef TRAFFIC_ZONE_PARK
#define TRAFFIC_ZONE_PARK 1
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
  float bz[4][3], bzLen, bzTab[9], bzVmax, jamT;                            // Безье и таблица длины (9 точек)
  float waitT, v, vt, wheelAng, stillT, hitCool;
  int rest, restWreck; float restPose[4];       // №1159: лежит в покое — физика не считается (поза x, y, z, курс на момент сна; сдвиг или скорость — просыпается)
  float hitV, stunT; int ramStrong, honkDone;   // №479: сила удара (сближение, м/с), сильный ли он, сигнал уже дан
  float rearX, rearZ; int rearOk;   // №400: задняя ось тянется за началом машины (wheelR), 0 — не задана
  // ---- сверка g02-traffic (02.11.2026): состояния правил оригинала; все за выключателями TRF_SV_*_OLD (traffic.c, шапка «сверка»)
  float offT;                       // P03-21: сколько подряд вне кадра, с (снимается дольше 5 с)
  int relJ;                         // P16-01: перекрёсток+1, на котором этой машине уже дали зелёный (повторно у него не встаёт); 0 нет
  int chgLane; float chgK, chgDur, chgCool;   // P04 T5: смена полосы — целевая полоса, ход 0..1, длительность, пауза до следующего броска, с
  float dodgeT; int dodgeSide;      // 8.1/T4: уворот от машины игрока — сколько ещё идёт (с), сторона поворота руля ±1
  float flashT;                     // 8.1: дальний свет мигает (период 0,2 с), сколько ещё, с (рисует main: привязка фар — передано)
  float retryT;                     // P16-04 / Д2: нет места на полосах выезда — повтор выбора раз в 5 с
  float honkT;                      // 8.3: сигнал игроку/пешеходу — сколько до следующего, с
  float outLaneT; int noRejoin;     // P03-25: сколько с вне полосы; возврат запрещён навсегда (наклон, дальность, курс)
  float dmgSeen, dmgWin;            // P01-09: урон на прошлом кадре и окно без урона после «почти смерти», с
  int dodged, dodgeHb, honkedP, honkKind;   // круг 3: уворот был / ручник на один кадр после 0,8 с / сигналили игроку с ближней дистанции (на того же игрока не увёртывается) / кому сигналили (1 машина игрока, 2 Гомер)
  float vy0; int wasAir;            // круг 2, P09:30: вертикальная скорость на прошлом кадре и был ли в воздухе (событие приземления)
  Car car;                                                    // твёрдое тело (carHitCar; после удара — carUpdate)
} TrCar;
typedef struct { int state, wreck; float stillT; Car car; int gone, off; float dmgSeen, dmgWin; int sel; int lazy; } TrParked;   // lazy — Коллизия-96: поза по данным, на землю ещё не ставилась (carInitDeferred); оседает при включении места   // sel — круг 5 (P03-19): место выбрано при загрузке зоны   // dmgSeen/dmgWin — P01-09 (сверка g02-traffic)   // state: 0 спит, 1 физика; gone — вне зоны (правка 139)
enum { TRP_OFF, TRP_WALK, TRP_STAND, TRP_FLY, TRP_DOWN, TRP_GETUP, TRP_DODGE,
       TRP_PANIC, TRP_CRINGE, TRP_SIDE, TRP_FACE, TRP_TALK };   // сверка g02-traffic: паника (7.4), съёживание и прыжок вбок (7.5), стоп лицом к игроку (P02-37), разговор (7.8)   // TRP_DODGE — уклонился от машины (правка 180)
typedef struct {
  int mode, model, path, seg, dir; float s;
  float x, y, z, h, vx, vy, vz, t, speed, blockT, nearCool;   // nearCool — с прошлого уклонения (правка 180)
  int poseTick, test;                                         // test: служебный пешеход сценария (стоит, не удаляется)
  float noGroundT;                                              // №594: сколько подряд идёт без земли под ногами
  unsigned char gTick;                                          // PROP_FAST: счётчик кадров для проверки земли раз в 4 кадра (PED_GROUND_FAST_OLD)
  int avoidSide; float avoidT;                                 // №510: сторона обхода игрока/пешехода (±1) и сколько она ещё держится
  float waitT;                                                 // №692: сколько подряд путь перекрыт (стоит в ожидании), с
  float orbBest, orbStall, orbTx, orbTz;                       // №813: обход без продвижения к точке пути (лучшее расстояние, сколько с без продвижения, точка)
  // ---- сверка g02-traffic: общий таймер состояния, вне кадра, паника, разговор
  float stT, offT, pTurnT, pDirX, pDirZ, talkCool; int run, partner, speaker, sideDir, knocked, homeRun, retPath; float homeSpeed, detX, detZ, detT, detW; const Car *detCar; unsigned char talkChk;   // knocked: уже был сбит/пнут — повторный пинок события не даёт (3.4)   // stT — сколько в TRP_PANIC/FACE/TALK/CRINGE/SIDE; offT — вне кадра, с;
                                                              // круг 3 (7.3): homeSpeed/homeRun — рождённая скорость, retPath — идёт назад к тропе со скоростью 1,0
                                                              // pTurnT/pDir — паника: когда менять курс и куда; run — бежит (3 м/с); partner/speaker — разговор
  SkinMouth mouth; float mouthT;                               // круг 5 (P15-06): ход рта (skin.h SkinMouth) и сколько ещё звучит возглас, с
  SkinPlayer sp;
} TrPed;
enum { TR_EV_CAR_HIT = 1, TR_EV_CAR_WRECK, TR_EV_PED_HIT, TR_EV_PARK_HIT, TR_EV_PED_KICK, TR_EV_PED_NEAR, TR_EV_CAR_HONK,
       TR_EV_CAR_WAY, TR_EV_CAR_LAND };   // сверка g02-traffic круг 2: CAR_WAY — «пробка» (игрок загородил, голос водителя, P09:3, не нарушение); CAR_LAND — машина трафика приземлилась после полёта (подвеска, P09:30; amount — скорость вниз)   // CAR_HONK (№479) — не нарушение, только гудок   // PED_NEAR — не нарушение (шкалу не двигает)
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
  unsigned long long zoneIn;             // №1132: в каких воротах группы игрок уже стоит (бит на ворота) — группа меняется только при входе
  float scanT, pedScanT, time; unsigned seed; int fresh, parkRR;
  float drawDist, pedDrawDist, carDrawDist;   // carDrawDist (№1202): дальность рисования машин; drawDist остаётся правилом «вне кадра / дальше дальности» для рождения и возврата
  int noSolid;                          // служебное: отрицательный контроль (flag trfnosolid) — трафик не твёрдый
  const TrZone *loader; int nLoader; uint32_t zoneMask;   // правка 139: загрузчики зон и маска загруженных файлов (~0 — все)
  int forceLod;                         // служебное (flag trflod): все машины в дальней версии (снимок LOD вблизи)
  float solidPX, solidPZ; int solidHave; // trafficSolid: где Гомер был в прошлом вызове (шаг — для остановки о пешехода)
  TrEvent ev[16]; int nev;
  // ---- сверка g02-traffic
  float loadT;                          // с после загрузки/сброса (P03-12: первые 3 с круг 90 м и без снятия)
  int streetRace;                       // P03-13: уличная гонка идёт — пешеходов нет и новые не появляются (trafficStreetRace)
  struct { signed char q[8]; unsigned char n; float tm; } jq[256];   // P16-01: очередь дорог стоп-знака на перекрёстке (по номеру перекрёстка)
  struct { int car; float t; } honkQ[3]; int nhonk;                 // P03-15: машины, что ответят на гудок игрока (номер машины, через сколько с)
  unsigned zonePrev;                    // круг 5 (P03-19): набор загруженных файлов зон на прошлом проходе (изменился → «загрузка зоны»: выбор стоящих машин)
  float pedTryT; float runV[TR_MAXPEDMODEL];   // Д8: счётчик попыток рождения пешехода (кадры); скорость корня клипа бега
  unsigned pedFrame;
  float hornT, hornX, hornZ; int hornOn;  // гудок игрока: держится (P03-15/7.4)
  struct { short road[48]; int n; int goalRoad; float tx, tz; } nav;   // P16-06: путь навигации (дороги по порядку от дороги игрока до дороги цели)
  // статистика кадра (для HUD/отчёта)
  int activeCars, movingCars, drawnCars, lodCars, drawnPark, awakePark, activePeds, drawnPeds, drawnTris;
} Traffic;

// загрузка файла целиком (одно чтение); 0 = ок, <0 — ошибка. Память: размер файла (1.35 МБ ур. 1) + стоящие машины
int trafficLoad(Traffic *t, const char *path);
int trafficInitMem(Traffic *t, void *data, unsigned size, int own);   // то же из памяти (64-выравнивание); own — free при trafficFree
void trafficFree(Traffic *t);
// текстуры TRF1 в формате level.shr: w, h, число уровней, CLUT (256 x 8888), уровни (swizzle, высота кратна 8)
int trafficTexCount(const Traffic *t);
// круг 5 (P17-27 / P18-10): «сложность» дороги-среза — байт, который tools/traffic_export.py кладёт в TrRoad.pad8 (диск: второй байт слова дороги). ИИ миссий берёт срез,
// если сложность не больше уровня. 0 — не задана или нет дороги. Читается из файла как есть
static inline int trafficRoadDifficulty(const Traffic *t, int road) { return (t && t->hd && road >= 0 && road < (int)t->hd->nRoad) ? (int)t->road[road].pad8 : 0; }
static inline int trafficRoadIsShortcut(const Traffic *t, int road) { return (t && t->hd && road >= 0 && road < (int)t->hd->nRoad) ? (t->road[road].flags & 1) : 0; }
int trafficStopCount(void);   // №875: концов полос со стоп-линией у зебры (stop875.bin)
void trafficTexInfo(const Traffic *t, int i, int *w, int *h, int *levels, int *pre, void **pal, void *lv[8]);
// всё убрать и поставить стоящие машины на места (старт, NEW GAME, телепорт); движущиеся и пешеходы появятся вокруг игрока
void trafficReset(Traffic *t, const ColWorld *w);
// UsePedGroup из MFK: номер из TRF1, −1 снимает выбор миссии; неверный номер не меняет мир
int trafficPedGroup(Traffic *t, int group);
void trafficRoutePoint(const Traffic *t, const TrCar *c, float ds, float p[3]);   // точка маршрута машины на ds м вперёд (служебное, №400)
// кадр: после физики игрока и Гомера. player — машина игрока (удары меняют и её); driving 0 = игрок пешком
// (ppos — ступни Гомера: машины тормозят, пешеходы ждут). eye/fwd — камера (спавн и удаление вне кадра).
extern void (*trafficLogLine)(const char *line);   // P0.2/A2: журнал PEDAIR (main_loop.h ставит smkWrite при смоуке)
void trafficUpdate(Traffic *t, const ColWorld *w, Car *player, int driving, const float ppos[3],
                   const float eye[3], const float fwd[3], float dt);
// Гомер: HomerSolidFn (homer.h) — вытолкнуть круг из кузовов машин и пешеходов; крыша машины — опора
int trafficSolid(void *traffic, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
// пинок Гомера (сфера x,y,z,r, направление dx,dz): пешеходы в сфере летят; число задетых
// правка 355: неподвижная машина k (оставленная своя): машины трафика и стоянки, задевшие её, отталкиваются (k не сдвигается)
int trafficHitStill(Traffic *t, const ColWorld *w, const Car *k);
#ifndef OLDCARHIT1241_OLD
#define OLDCARHIT1241_OLD 0   // правка 1241: игрок в чужой машине таранит оставленную свою как обычную стоящую (1 — проезд насквозь, как было)
#endif
int trafficHitPlayerStill(const ColWorld *w, Car *player, Car *own, int *awake, float *still, float dt);   // 1 — был удар; обе машины сдвинуты, толчок и урон по правилам стоящей машины трафика
// сверка g11b-collect круг 7 (P04 W5): снаряд осы по отрезку a -> b (dir — единичное направление): -> доля пути до первого пешехода на ногах или машины трафика (больше 1 — нет); apply = 1 — удар:
// пешеход летит 10 м/с под 45° по dir (пешеходы — бросок через pedFly), машина получает урон 0,1 (не выше 0,999: один удар не разбивает). Игрока и ос не касается
float trafficBoltHit(Traffic *t, const float a[3], const float b[3], const float dir[3], int apply);
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
extern void (*trfFadeHook)(float a); extern int trfFadeOn; extern unsigned trfFadeN; extern float trfFadeMin, trfFadeTest;   // №1202: растворение появляющейся машины: main ставит смешивание с постоянной прозрачностью a (1 — снять)
typedef struct { unsigned char pass, born; unsigned short i; float d, R; } TrPop;   // №1202: журнал появления машин в кадре (только смоук, traffic.c trfPopMark)
extern TrPop trfPop[48]; extern unsigned trfPopN;
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
// сверка g02-traffic, СВЕРКА_ГОМЕР 7.6 (строка 8): оригинал сбивает пешехода при скорости больше 1 м/с (3,6 км/ч), было 3 м/с. TR_PED_HIT_V_OLD=1 — 3 м/с
#ifndef TR_PED_HIT_V_OLD
#define TR_PED_HIT_V_OLD 0
#endif
#define TR_PED_HIT_V (TR_PED_HIT_V_OLD ? 3.f : 1.f)          // машина быстрее — сбивает, медленнее — отталкивает
#ifndef TR_PED_LIE_OLD
#define TR_PED_LIE_OLD 0   // 1 — прежнее: лежит 1 с
#endif
// Лежание сбитого (сверка g02-traffic, находка ps2-twin по СВЕРКА_ГОМЕР 7.7 и чтение оригинала): в оригинале время не задано числом — тело кувыркается в физике, пока сглаженная
// (15 отсчётов) линейная скорость не упадёт ниже 0,5 м/с и угловая ниже 0,7 рад/с (и пока касается машины, или стоит под игроковой машиной, или выше 1 м над землёй, его подталкивают
// снова), потом играет клип get_up. Запись PS2 (удар на 10 м/с, один чистый случай): с удара до подъёма ~570 кадров вертикальной развёртки ≈ 11,4 с. Зависимость от скорости удара не снята
// (запись на 5 и 15 м/с нужна). Берём 11,4 с от удара: минус типичный отлёт ~0,6 с = 10,8 с лежания после приземления. PEDLIE_PS2_OLD=1 — прежние 3 с (видео L1: ~4 с)
#ifndef PEDLIE_PS2_OLD
#define PEDLIE_PS2_OLD 0
#endif
#define TR_PED_DOWN (PEDLIE_PS2_OLD ? (TR_PED_LIE_OLD ? 1.f : 3.f) : 10.8f)   // сбитый лежит, с
#define TR_HIT_PED .13f           // доля шкалы Hit & Run за сбитого человека (аудит 09.10: PS2 +13, tw_v14_multi30; было .10 по gameplay.md §6)
int trafficCarHitsBody(const Car *k, float *x, float y, float *z, float r, float v[3]);
int trafficBodyFly(const ColWorld *w, float p[3], float v[3], float *t, float dt);
float trafficGetUpLen(const Traffic *t);
void trafficPushEvent(Traffic *t, int type, int idx, float x, float y, float z, float amount, const char *who);
// правка 122: стрелка навигации к цели (px, pz, курс ph) -> (tx, tz) по дорогам трафика: 1 — стрелка в p с курсом *h (перекрёсток ближе maxd)
int trafficRouteArrow(const Traffic *t, float px, float pz, float ph, float tx, float tz, float maxd, float p[3], float *h);

// ---- сверка g02-traffic (02.11.2026): точки вызова для других окон (подробно в docs/handoff/СВЕРКА_ВНЕДРЕНИЕ/g02-traffic_журнал.md)
// гудок игрока (P03-15 / СВЕРКА_ГОМЕР 7.4): звать каждый кадр, пока гудок зажат (on = 1), x, z — машина игрока. Трафик в 25 м отвечает (шанс 10 % на машину),
// пешеходы в 12 м пугаются. Точка вызова: main_loop.h рядом с sndHorn
void trafficPlayerHorn(Traffic *t, int on, float x, float z);
// пинок/наезд тоже пугают пешеходов в 12 м от игрока (7.4): звать из места, где игрок ударил
void trafficScare(Traffic *t, float x, float z);
// уличная гонка идёт (P03-13): все пешеходы снимаются, новые не появляются, пока on = 1. Вызывает game_msn.c при загрузке/выгрузке реквизита гонки (передано g-миссии)
void trafficStreetRace(Traffic *t, int on);
// P16-06: путь навигации целиком. trafficNavBuild — один раз при старте цели (позиция игрока px, pz, курс ph; цель tx, tz); trafficNavArrows — каждый кадр:
// заполняет стрелки (позиция, курс, red = «не туда»), возвращает число (до max). Рисует msn_view.c (передано g-вид: набор darrow на каждую)
typedef struct { float p[3], h; int red; } TrNavArrow;
int trafficNavBuild(Traffic *t, float px, float pz, float ph, float tx, float tz);
int trafficNavArrows(const Traffic *t, float px, float pz, TrNavArrow *out, int max);
// сверка g02-traffic круг 2 (g11 P11-02): машины трафика, что не стоят и не OFF (едущие, сбитые, проснувшиеся стоящие), ближе r к (x, z); до max указателей. Зовёт main_loop.h
// и передаёт каждую в propsHitCar: предметы ломает любая машина, не только игрока
int trafficCarsNear(Traffic *t, float x, float z, float r, Car **out, int max);
// Слой «персонаж ↔ машина» (g07 → g02): в оригинале любая машина толкает физика, касание = персонаж стоит 3,0 с. Здесь — какие машины считаются твёрдыми для пеших: трафик, стоящие места,
// машины миссий (t->extra) и, если передана, машина игрока (extraCar), СТОЯЩИЕ или едущие медленнее 0,5 м/с (быстрые сбивают — pedHitBy / npcKnock). Цена: только машины в радиусе кузова + r
// (быстрый отсев по расстоянию), для прохожих ≤ 7 × (5 + 6 + стоящие) проверок расстояния за кадр. PEDCAR_ALL_OLD=1 — как в TEST84 (только машина игрока)
// №1132 (сверка g02-traffic, правка Роба: у АЭС (L1 E4-8) ходят простые пешеходы, а в городе — работники АЭС): в оригинале группа моделей прохожих —
// ОДНА текущая; её меняют только вход игрока в ворота группы (раз при входе) и UsePedGroup миссии (раз при старте миссии, дальше ворота снова главные).
// У нас UsePedGroup держал группу поверх ворот до конца миссии, а ворота срабатывали на каждом кадре внутри. PEDZONE1132_OLD=1 — как в TEST85.
// №1159 (Роб, ночь 03.11, регресс скорости тура TEST83→85; замер g01-cars: машин трафика в полной физике за кадр 0,18 → 0,47 на уровне 5, подшагов 0,53 → 1,18). Съехавшая и
// остановившаяся машина трафика остаётся твёрдой до удаления (круги 6–7: запрет возврата на полосу в перекрёстке и у конца дороги) и каждый кадр считала подшаги физики на месте.
// Теперь: лежит без движения дольше TR_REST_T (на земле, не горит) — это неподвижное твёрдое тело: подшаги не считаем, остальное (возврат на полосу, таймеры, удары по ней, удаление) как было;
// просыпается от любого сдвига, скорости, удара. Пары двух лежащих (две съехавшие, съехавшая и стоящая спящая) друг с другом не проверяются. TRF_REST1159_OLD=1 — как в TEST85
#ifndef TRF_REST1159_OLD
#define TRF_REST1159_OLD 0
#endif
#define TR_REST_T 1.0f    // с покоя до сна (у стоящих машин порта — тот же 1 с, traffic.c)
#ifndef PEDZONE1132_OLD
#define PEDZONE1132_OLD 0
#endif
#ifndef PEDCAR_ALL_OLD
#define PEDCAR_ALL_OLD 0
#endif
const Car *trafficSolidCarAt(const Traffic *t, float x, float z, float y, float r, const Car *extraCar);   // первая твёрдая машина, чей кузов касается круга (x, z, r) на высоте y; 0 — нет
int trafficCarsSolid(const Traffic *t, float x, float z, float r, const Car **out, int max);          // твёрдые машины в радиусе r от (x, z) — для обхода по углам кузова
extern int trfOneTapDeath;   // P16-17 «One Tap Traffic Death»: 1 — машина трафика, ударенная машиной игрока, гибнет с одного удара (выставит меню читов, когда оно будет)

#ifdef __cplusplus
}
#endif
#endif
extern int trfCost1001b;   // №1001б: 1 — pathGap отбрасывает далёкие препятствия заранее (побитно то же); TRFCOST1001B_OLD=1 / flag trfcost1001b 0 — прежний обход
extern unsigned trfProfUs[8]; extern int trfProfOn;   // №1001б: разбивка trafficUpdate по частям (диагностика, строка TRFSUB)
