// Бонус-игра главного меню (мини-игра «Super Sprint», правка Роба 420): правила гонки на трассах b01..b07.
// Чистый C99, без PSP-заголовков и без malloc: один код для EBOOT (viewer/bonus_play.h) и полигона ботов на Маке
// (tools/bonus/race_host.c). Данные трассы — файл BNS1 (tools/bonus/export.py), коллизия — COL1 трассы (b0N.shr).
//
// Что из данных диска, что — правило порта (сверка: docs/handoff/БОНУС_ИГРА_2026-10-09.md, вопросы — knowledge/questions.tsv):
//   ДАННЫЕ  финиш и чекпойнты (Event 2 b0Ndata, номер по порядку — второе слово, объём TriggerVolume): круг засчитан, когда
//           машина вошла в объём финиша, пройдя все чекпойнты по порядку; старты car1..car4 с курсом из данных; точки пути ИИ
//           WPR (по ходу) / WPL (обратный ход); ходовые соперников bonus/bgT_K.con; камера сверху sprint_camShape (направление,
//           угол кадра); число кругов на выбор «x 0 … x 9», смена направления (CW/CCW) — страница MiniMenu.pag;
//           таблица итогов: место, победы, очки, общее время, лучший круг (MiniSummary.pag); строка GO! (SLES_518.97)
//   PS2     default3/range1..5, count3/2/1=2.96/1/1s; b01 native tPointCamera eye/lens (observer facts).
//   ПОРТ    обратный ход — машины на тех же стартах, развёрнуты на π, чекпойнты в обратном
//           порядке; очки за финиш4/3/2/1, DNF0; соперники доезжают до всех финишей или BR_DNF_S от первого победителя,
//           иначе DNF; место в гонке — по кругам, пройденным чекпойнтам и расстоянию до следующего; застрявший соперник
//           (нет продвижения BR_STUCK_S) — на последнюю пройденную точку пути; путь ИИ — точки WPR/WPL в порядке обхода
//           (tools/bonus/track.py lap_cycle) + финиш последней точкой круга.
#ifndef SHAR_BONUS_H
#define SHAR_BONUS_H
#include <stdint.h>
#include "col.h"
#include "car.h"
#include "animobj.h"   // ловушки трасс: объекты ANM1 (pistons, spikes), живые объёмы COL1

#ifdef __cplusplus
extern "C" {
#endif

// файлы рядом с EBOOT (tools/bonus/export.py, правила tools/ship_rules.txt)
#define BONUS_SHR "bonus/b%02d.shr"
#define BONUS_BNS "bonus/b%02d.bns"
#define BONUS_UI_PAK "bonus/mini.shr"      // родные экраны мини-игры (tools/bonus/screens_export.py, viewer/bonus_play.h)
#define BONUS_FLYBY_CAM "bonus/flyby.cam"   // облёт minigamecam.p3d перед заездом (tools/bonus/flyby_export.py)
#define BONUS_CARS "bonus/msncars-b.shr"
#define BNS_MAXCP 16
#define BNS_MAXWP 15
#define BR_MAX 4
#ifndef BONUS_SETUP_PS2_OLD
#define BONUS_SETUP_PS2_OLD 0
#endif
#ifndef BR_LAPS_DEF
#define BR_LAPS_DEF 3        // observer ps2-bonus-minimenu-native: default 3
#endif
#define BR_LAPS_MIN 1        // «x 0» — пункт есть в данных, но гонки без кругов нет: порт начинает с 1
#define BR_LAPS_MAX (BONUS_SETUP_PS2_OLD ? 9 : 5) // measured native range 1..5
#ifndef BONUS_COUNT_PS2_OLD
#define BONUS_COUNT_PS2_OLD 0
#endif
#ifndef BR_COUNT_S
#define BR_COUNT_S (BONUS_COUNT_PS2_OLD ? 3.f : 4.96f) // observer native: 3=2.96s, 2/1=1s each
#endif
static inline int brCountdownDigit(float remaining) {
  return remaining > 2.f ? 3 : remaining > 1.f ? 2 : remaining > 0.f ? 1 : 0;
}
#ifndef BR_DNF_S
#define BR_DNF_S 20.f        // observer native: ~20s from first winner, or earlier if all finish
#endif
#ifndef BONUS_ROADS_OLD
#define BONUS_ROADS_OLD 0    // №1116: 1 — соперники по плотному пути порта (nav.py), как до правки; 0 — по дорогам трассы (хвост BRD1 файла .bns)
#endif
#ifndef AIAI1147_OLD
#define AIAI1147_OLD 0       // №1147: 1 — соперники проходят сквозь друг друга (e.noNpcCollide, прежнее решение g07b); 0 — удары машина-машина между всеми, как у игрока
#endif
#ifndef BR_STUCK_MOVE
#define BR_STUCK_MOVE 15.f   // №1147: «застряла» — нет продвижения BR_STUCK_S с И сдвиг меньше стольких метров; едущая по извилистой дороге (прямая до чекпойнта растёт) не застряла
#endif
#ifndef BR_STUCK_LONG
#define BR_STUCK_LONG 30.f   // №1147: ... но без продвижения столько секунд, даже едучи (кружит по кругу, не берёт точку) — тоже возврат
#endif
#ifndef BR_STUCK_S
#define BR_STUCK_S 6.f       // ПРАВИЛО ПОРТА: соперник без продвижения по гонке столько секунд — на свой путь впереди
#endif
#define BR_STUCK_PROG 0.15f  // продвижение: доля отрезка между чекпойнтами (мера места BrRacer.prog)

typedef struct {               // 128 Б
  char magic[4]; uint32_t ver, track, ncp, nstart, nwpf, nwpr, flags;
  float fov, aspect, znear, zfar, pos[3], look[3], up[3], bbox[6];
  uint32_t npathf, npathr;     // точек пути ИИ (tools/bonus/nav.py, правило порта) по ходу / обратно
  uint32_t npick, nhaz, ntrapobj;   // версия 2 (09.10): подборов, зон (ловушки, B04G, свет), объектов ловушек (spikes/pistons); в версии 1 — 0
} BnsHdr;
typedef struct { float c[3], ax[3][3], h[3]; uint32_t rect, order; char name[16]; uint32_t pad[3]; } BnsCp;   // 96 Б
typedef struct { float p[3], h; char name[16]; } BnsStart;   // 32 Б
typedef struct { float p[3], r; } BnsWp;                     // 16 Б

typedef struct { float p[3], tag; } BnsPath;               // 16 Б: точка пути ИИ, tag — номер остановки круга у точки или -1
#define BNS_MAXPATH 256

// ---- подборы, ловушки, свет (BNS версии 2, tools/bonus/export.py; правка 420, часть «подборы, ловушки, огонь, DEATH»)
// ДАННЫЕ  подбор — ActionEvent b0Ndata с действием Nitro / Wrench (repair* — тоже Wrench), сфера TriggerVolume 5 м; модель —
//         составной рисунок wrench (art/wrench.p3d) / nitro (art/missions/generic/nitro.p3d, его грузит ss.mfk) — тип SPR1 трассы;
//         ловушка — Event 66 trap* (b04, b07): объёмы-датчики; объект ловушки — AnimCollision "pistons" (b04) / "spikes" (b07)
//         (имена — строки SLES_518.97 у таблицы мини-игры), клип без цикла: поршни/шипы выходят из пола, их объёмы COL1 «живые»;
//         B04G01..03 — Event 63 (b04): сфера r 3 м ровно на каждом поршне, второе слово 20;
//         fire — Event 65 (b07): зона света, данные те же, что tirefirelight уровней 1/4/7 (0xBBFF9A3F, ARGB);
//         DEATH — Event 4 (b03, b06, b07) в JMP1 трассы: объём и точка возврата с курсом (как у уровней, правки 86/349)
//   ПОРТ  что делает подбор (в данных — только имя действия): Wrench чинит машину целиком (как ключ уровня, viewer/game.c);
//         Nitro — разгон BR_NITRO_ACC × газ вдоль курса BR_NITRO_S с, пока колёса на земле (ускорение — как замеренный у оригинала
//         разгон зоны «jump boost», car.c JUMP_ZONE_ACC; длительность — правило порта); взятый подбор пропадает и возвращается
//         через BR_PICK_RESPAWN_S (как ключ уровня WRENCH_RESPAWN_T); ловушка: машина в объёме датчика — клип объекта вперёд до
//         конца, BR_TRAP_HOLD_S наверху, назад к кадру 0, снова готова; B04G: машина в сфере, пока поршни выходят, получает
//         вертикальную скорость не меньше второго слова (20, м/с — так прочитано: пик скорости поршня в клипе 21 м/с; вопрос В135);
//         свет: машина в зоне рисуется с цветом зоны (lerp(белый, RGB, альфа)); DEATH — как у уровней: на точку возврата
//         данных курсом локатора, развёрнутым по ходу гонки, если смотрит назад (вопрос В136).
// BONUS_PICKUPS_OLD=1 — как до этой части: подборы, ловушки, свет и DEATH трасс не действуют, SPR1 трассы не рисуется
#ifndef BONUS_PICKUPS_OLD
#define BONUS_PICKUPS_OLD 0
#endif
typedef struct { float p[3], r; uint32_t kind; char name[12]; } BnsPick;   // 32 Б: kind 1 nitro, 2 wrench
enum { BNS_PK_NITRO = 1, BNS_PK_WRENCH = 2 };
typedef struct { uint32_t kind, arg, rect, pad; float c[3], ax[3][3], h[3], loc[3]; char name[16], obj[16]; uint32_t pad2[2]; } BnsHaz;   // 128 Б
enum { BNS_HZ_TRAP = 1, BNS_HZ_LAUNCH = 2, BNS_HZ_LIGHT = 3 };
#define BNS_MAXPICK 16
#define BNS_MAXHAZ 16
#define BR_MAXTRAP 2
#ifndef BR_DEATH_AI_N
#define BR_DEATH_AI_N 1          // соперник ИИ: с N-го DEATH подряд без нового чекпойнта — на свой путь вперёд (ПРАВИЛО ПОРТА, brDeath;
                                 // 1 — всегда: полигон b06, 3 — точка данных дважды, гейт tests/bonus/race.sh (2) b06 падает)
#endif
#ifndef BR_NITRO_ACC
#define BR_NITRO_ACC 9.f          // м/с² сверх тяги (ПРАВИЛО ПОРТА: как JUMP_ZONE_ACC car.c — замер разгона зоны оригинала)
#endif
#ifndef BR_NITRO_S
#define BR_NITRO_S 2.f            // с (ПРАВИЛО ПОРТА)
#endif
#ifndef BR_PICK_RESPAWN_S
#define BR_PICK_RESPAWN_S 5.f     // с (ПРАВИЛО ПОРТА: как ключ уровня WRENCH_RESPAWN_T)
#endif
#ifndef BR_TRAP_HOLD_S
#define BR_TRAP_HOLD_S 2.f        // с наверху (ПРАВИЛО ПОРТА)
#endif

// ---- сверка с исходником (g07b-msnview, 02.11.2026; строки P17-14/15/17/18/20, P04 B1, P07 17/18, P17 Д3). Каждое — под своим выключателем (=1 — прежнее поведение порта)
//   нитро: у каждой машины 3 заряда в начале заезда; подбор «нитро» даёт +1 заряд; гудок тратит заряд и даёт толчок +10 м/с вдоль курса, повтор не раньше чем через 0,5 с
//   (кнопка человека — ↑ крестовины, как гудок в игре; ИИ жмёт сам не чаще раза в 10 с, если путь впереди в пределах 10° от курса; на «GO!» часть ИИ тратит заряд сразу)
//   конец заезда: отсчёт 10 с начинается, когда доехал предпоследний или доехали все люди; последний финишёр обрывает его
//   подборы возвращаются: нитро через 15 с, ключ через 30 с; ловушки переключает только лидер (первый датчик — вперёд, второй — назад); событие 63 (B04G) ничего не делает
#ifndef BR_NITRO3_OLD
#define BR_NITRO3_OLD 0
#endif
#ifndef BR_DNF10_OLD
#define BR_DNF10_OLD 0
#endif
#ifndef BR_RESPAWN_KIND_OLD
#define BR_RESPAWN_KIND_OLD 0
#endif
#ifndef BR_TRAP_LEADER_OLD
#define BR_TRAP_LEADER_OLD 0
#endif
#ifndef BR_LAUNCH_OLD
#define BR_LAUNCH_OLD 0
#endif
// ---- правка Роба 1109 (сверка с оригиналом, круг 7, g03-aicars; supersprintmanager). Каждое — под своим выключателем (=1 — прежнее поведение)
//   BR_SEQ_OLD: список чекпойнтов круга — как у оригинала, с повторами (b02 и b07: 2,3,4,5,6,3,8,9,1; b05: …,10,7,3,11,1; обратный ход — свои списки), финиш последним
//   BR_CHEAT_OLD: чекпойнт засчитывается при ВХОДЕ в объём любого чекпойнта по его номеру; не тот номер (и не предыдущий) — «читер»: возврат на предыдущий шаг, круг без
//     финиша не засчитывается, пока не пройдёт нужный; прежнее — только объём следующего чекпойнта, без возврата
//   BR_POSDIST_OLD: место при равных кругах и шаге — по расстоянию до следующего чекпойнта ПО ПУТИ (плотный путь хода), меньше — впереди; прежнее — доля прямой между чекпойнтами
#ifndef BR_SEQ_OLD
#define BR_SEQ_OLD 0
#endif
#ifndef BR_CHEAT_OLD
#define BR_CHEAT_OLD 0
#endif
#ifndef BR_CHEAT_AI
#define BR_CHEAT_AI 0   // 1 — правило «читера» и у соперников ИИ (их путь заходит в чужие объёмы: возвраты)
#endif
#ifndef BR_POSDIST_OLD
#define BR_POSDIST_OLD 0
#endif
#define BR_NITRO_CHARGES 3
#define BR_HORN_KICK 10.f        // м/с вдоль курса за одно нажатие
#define BR_HORN_GAP_S 0.5f
#ifndef BR_AIHORN3_OLD
#define BR_AIHORN3_OLD 0   // круг 3 (проверка g07b): 1 — ИИ может гудеть с первой секунды и решает каждый кадр (прежнее)
#endif
#ifndef BR_LAPLIST_OLD
#define BR_LAPLIST_OLD 0   // круг 7 (P17-16): 1 — прежний порядок чекпойнтов (по возрастанию, без повторов b02, b05, b07)
#endif
int brLapList(int tr, int reverse, int *out, int cap);
#ifndef BR_GONITRO1_OLD
#define BR_GONITRO1_OLD 0  // круг 6 (P17-20): на «GO!» гудят до (ИИ − 1) машин; при одном ИИ — никто. 1 — прежнее (при одном ИИ он гудел с шансом 3/7)
#endif
#define BR_AI_HORN_GAP_S 10.f
#define BR_AI_HORN_CHECK_S 2.f
#define BR_DNF10_S 10.f
#define BR_RESPAWN_NITRO_S 15.f
#define BR_RESPAWN_WRENCH_S 30.f
// круг 6 (P04 B3): ИИ бонус-гонки, перевёрнутый или разбитый, ставится на колёса на месте через 1 с. «Перевёрнутый» = вектор «вверх» кузова отклонён от вертикали больше чем на 75°,
// то есть его вертикальная составляющая не выше cos75° = 0,258819; скорость не учитывается. Кватернион q = (w, x, y, z); вертикальная составляющая оси «вверх» = 1 − 2(x² + z²).
#ifndef BR_AIFLIP_UPY
#define BR_AIFLIP_UPY 0.258819f
#endif
#ifndef BR_AIFLIP_S
#define BR_AIFLIP_S 1.f
#endif
static inline float brUpY(const float q[4]) { return 1.f - 2.f * (q[1] * q[1] + q[3] * q[3]); }
// один кадр счётчика: t — секунды наклона (или с момента разрушения); -> 1, когда пора ставить на колёса. Вышла из наклона и не разбита — счёт с нуля
static inline int brAiFlipStep(float upY, int destroyed, float *t, float dt) {
  if (upY <= BR_AIFLIP_UPY || destroyed) *t += dt; else *t = 0.f;
  if (*t >= BR_AIFLIP_S) { *t = 0.f; return 1; }
  return 0;
}
typedef struct { int state, obj; float f, hold, last, fps; } BrTrap;   // ловушка: BR_TRAP_*, объект ANM1, кадр, выдержка

typedef struct {
  const BnsHdr *h; const BnsCp *cp; const BnsStart *st; const BnsWp *wpf, *wpr; const BnsPath *pf, *pr;
  const BnsPick *pk; const BnsHaz *hz; unsigned npick, nhaz;   // версия 2; в версии 1 — 0
  // №1116 (сверка g03-aicars): дороги трассы — хвост файла (блок TRF1: перекрёстки, дороги, полосы; tools/bonus/roads.py), u32 размер, 'BRD1'.
  // Соперники ведутся по ним от точки WPR/WPL к следующей, как у оригинала (по дорогам файла трассы). NULL — хвоста нет (старый файл)
  const void *rd; unsigned rdSize;
} BnsTrack;
// разбор файла BNS1 (данные живут у вызывающего). 0 — ок, < 0 — ошибка (-1 магия/версия, -2 размер, -3 числа вне пределов)
int bnsParse(BnsTrack *t, const void *data, unsigned size);
// p внутри объёма чекпойнта (ящик — по осям, сфера — радиус h[0]); grow — запас, м
int bnsInside(const BnsCp *c, const float p[3], float grow);

enum { BR_COUNT = 0, BR_RACE, BR_DONE };
typedef struct {
  int next;            // номер (в порядке круга) следующего чекпойнта; 0 — финиш
  int lap;             // кругов пройдено
  int finished, dnf, place;
  float lapT, bestLap, totalT, finishT;
  float prog;          // мера места: круги * ncp + пройдено + доля пути к следующему
  int cps;             // всего пройдено чекпойнтов (наблюдение)
  float stuckT, stuckLong, anchor[3], lastPos[3], bestProg; int respawnN, respawnCp;   // возвратов подряд у того же чекпойнта
  int deathN, deathCp;   // DEATH подряд без нового чекпойнта (соперник ИИ, BR_DEATH_AI_N, brDeath)
  int cheated; unsigned inMask;   // 1109: отправлен назад за чужой чекпойнт; маска объёмов чекпойнтов, в которых машина была в прошлый кадр (вход = переход 0 -> 1)
  int step; float dist;           // 1109: пройдено шагов круга (для места) и путь до следующего чекпойнта, м
} BrRacer;

typedef struct {
  BnsTrack t;
  int laps, reverse, n;            // кругов, обратный ход, машин
  int order[BNS_MAXCP], ncp;       // порядок чекпойнтов хода: order[0] — финиш
  BrRacer r[BR_MAX];
  int state; float time, count, sinceFirst;
  int finishedN, human;            // сколько доехало; номер машины игрока (-1 — только боты)
  float wp[BNS_MAXWP + 1][3]; float wpr[BNS_MAXWP + 1]; int nwp;   // путь ИИ этого хода (+ финиш последней точкой)
  const BnsPath *path; int npath, stopAt[BNS_MAXWP + 1];          // плотный путь ИИ хода; номер точки пути у остановки k
  float grow[BR_MAX];   // запас объёмов чекпойнтов на машину, м: кузов машины входит в объём (полуширина кузова; 0 — точка)
  // водитель трассы (brDrive): профиль скорости по плотному пути на машину и состояние
  float vprof[BR_MAX][BNS_MAXPATH]; unsigned char jump[BNS_MAXPATH];
  struct { int idx; float stuckT, backT, backSteer, vt; } drv[BR_MAX];
  // подборы, ловушки, свет, DEATH (часть «подборы»; данные — BNS2, ловушки — ANM1 трассы через brTrapBind)
  uint32_t pickTaken; float pickAway[BNS_MAXPICK];       // взятые подборы (маска) и секунды до возврата
  float boostT[BR_MAX];                                   // осталось нитро у машины, с
  BrTrap trap[BR_MAXTRAP]; int ntrap;                    // объект ловушки ANM1: состояние и кадр клипа
  unsigned launched;                                      // машины, подброшенные поршнями в этом выходе
  unsigned nNitro, nWrench, nDeath, nTrap, nLaunch, nLight;   // счётчики (журнал, гейты)
  int nitro[BR_MAX]; float nitroGap[BR_MAX], aiHornT[BR_MAX], aiChkT[BR_MAX]; uint32_t rng; unsigned nHorn;   // сверка g07b: заряды нитро, паузы гудка, зерно выбора ИИ, счётчик гудков
  float dnfT; int dnfOn;                       // сверка g07b: отсчёт 10 с до DNF (идёт, когда dnfOn)
  int trapIn[BR_MAXTRAP + 2];                  // сверка g07b: лидер был в датчике k прошлый кадр (границы входа)
  const Car *peer[BR_MAX]; int npeer;          // №1147: машины гонки (индекс — место в гонке: 0 игрок, 1.. соперники), ставит вызывающий brSetPeers каждый кадр; нужны, чтобы не ставить машину на другую
} BonusRace;
enum { BR_TRAP_IDLE = 0, BR_TRAP_RISE, BR_TRAP_HOLD, BR_TRAP_LOWER };

// старт гонки: машины ставит вызывающий по brStart (курс с учётом обратного хода)
void brInit(BonusRace *b, const BnsTrack *t, int laps, int reverse, int n, int human);
void brStart(const BonusRace *b, int i, float pos[3], float *h);
// кадр: pos[i] — положения машин. Возврат: битовая маска машин, прошедших круг в этом кадре
unsigned brUpdate(BonusRace *b, const float (*pos)[3], float dt);
int brPlaceOf(const BonusRace *b, int i);
// очки завершившего гонку: native PS2 4/3/2/1; DNF отдельно даёт 0
int brPoints(int place);
#ifndef BONUS_CAMERA_PS2_OLD
#define BONUS_CAMERA_PS2_OLD 0
#endif
// Measured b01 lens; vertical field preserved on 16:9 PSP. Other tracks retain authored-fit policy.
#ifndef BONUSTOP_SV_OLD
#define BONUSTOP_SV_OLD 0   // №1109 (камера): 1 — по таблице оригинала только трасса 1, остальные трассы камерой по рамке трассы (правило порта), как в TEST84
#endif
int brTopIsPs2(const BnsTrack *t);   // вид сверху берётся из таблицы оригинала (трасса 1; с №1109 — все семь)
void brTopLens(const BnsTrack *t, float *fovyDeg, float *nearZ, float *farZ);
// камера сверху: глаз и взгляд по направлению sprint_camShape, расстояние — чтобы прямоугольник трассы (bbox) вошёл в кадр
// с вертикальным углом fovyDeg и соотношением aspect (ПРАВИЛО ПОРТА: дальность sprint_camShape 2212 м при zfar 1000 —
// в кадр так трасса не попадает, значит игра ставит камеру сама; берём направление и угол из данных)
void brTopCam(const BnsTrack *t, float fovyDeg, float aspect, float eye[3], float at[3], float up[3], float *dist);
// маршрут ИИ к остановке goal (номер в b->wp, с кругами — по модулю nwp) от точки pos по плотному пути хода -> точек в out
// (формат маршрута viewer/msncar.c: out[0] — первая точка, последняя — остановка). 0 — плотного пути нет (прямо к точке)
int brPlan(const BonusRace *b, int goal, const float pos[3], float (*out)[3], int cap);
// ПРАВИЛО ПОРТА: в бонус-гонке машина не разбивается (урон не выше BR_DMG_MAX: дым есть, взрыва нет) — разбитая машина выбыла бы из
// гонки навсегда (вопрос В133). С 09.10 ключи трасс (brPick) чинят машину; у b01 ключей нет, поэтому правило остаётся
#ifndef BR_DMG_MAX
#define BR_DMG_MAX 0.9f
#endif
void brKeepAlive(Car *k);
// застрявший соперник — куда его поставить: точка плотного пути на BR_RESPAWN_AHEAD точек впереди ближней к машине (к
// остановке goal), курс — вдоль пути (ПРАВИЛО ПОРТА, как возврат машин миссий на свой путь). 0 — пути нет
#ifndef BR_RESPAWN_AHEAD
#define BR_RESPAWN_AHEAD 2
#endif
// цель ИИ машины i — остановка у её следующего чекпойнта с кругами (номер для McCar.wp: круг * nwp + остановка). Ведёт ИИ
// правило гонки, а не счёт точек водителя: пропущенный чекпойнт — машина возвращается к нему (ПРАВИЛО ПОРТА)
int brGoal(const BonusRace *b, int i);
// третий возврат подряд у того же чекпойнта — прямо на остановку в его объёме (ПРАВИЛО ПОРТА: соперник не застревает навсегда)
int brRespawn(BonusRace *b, int i, const float pos[3], float out[3], float *h);
// №1147 (правка Роба 1147: удары между соперниками, пересечений кузовов 0): возврат на путь и DEATH не ставят машину на другую — точка сдвигается вперёд по пути (до BR_CLEAR_TRY точек),
// пока кузов не освободится; машины гонки — brSetPeers. AIAI1147_OLD = 1 — ставит как было
void brSetPeers(BonusRace *b, const Car *const *cars, int n);
int brSpotFree(const BonusRace *b, int self, const float p[3], float h);   // 1 — кузов машины self в точке p курсом h не задевает чужих (зазор BR_CLEAR_GAP)
#define BR_CLEAR_TRY 10
#define BR_CLEAR_GAP .6f
// Водитель трассы (09.10, вторая версия ИИ бонус-игры; прежний — водитель машин миссий viewer/msncar.c). ПРАВИЛО ПОРТА целиком:
//   * едет по плотному пути круга (tools/bonus/nav.py) целиком, а не до следующей остановки: видит повороты за чекпойнтом;
//   * профиль скорости на точку пути: поворот — sqrt(a_бок * R) по радиусу через ±2 точки (8 м), a_бок как у водителя машин миссий
//     (7 м/с² × TireGrip/2.4, до 11); гребень (путь выпуклый вверх) — sqrt(g / кривизна), чтобы колёса не отрывались; в зоне прыжка
//     данных (Event 48, блок JMP1) — полный ход (трамплину нужен разгон); торможение заранее — обратный проход с замедлением .con;
//   * руль — догон точки пути впереди (4 м + 0,35 с × скорость), выбор ближней точки — с высотой (ярусы развязок не путаются);
//   * в воздухе руль прямо; упёрлась (газ есть, скорости нет 1,5 с) — назад с обратным рулём 1,2 с; дальше — brStuck/brRespawn.
// brDriverInit — после brInit, на каждую машину её ходовыми; brDrive — ввод машины i этого кадра
void brDriverInit(BonusRace *b, int i, const CarParams *p, const ColWorld *w);
void brDrive(BonusRace *b, int i, const Car *k, float dt, CarInput *in);
// удары машин гонки между собой (carHitCar, как машины миссий друг с другом); 1 бит на машину — был удар
unsigned brCollide(Car *const *cars, int n, const ColWorld *w);
// застрявший соперник: мера пути (prog) не росла BR_STUCK_S — вернуть на путь (1 — вернуть; куда — brRespawn)
int brStuck(BonusRace *b, int i, const float pos[3], float dt);

// ---- подборы, ловушки, свет, DEATH (что из данных, что правило порта — выше, у BnsPick)
// машина i (кузов k) у подбора: взять (эффект сразу: ключ — ремонт, нитро — BR_NITRO_S разгона). -> номер подбора или -1
int brPick(BonusRace *b, int i, Car *k);
void brPickTick(BonusRace *b, float dt);                  // раз в кадр: взятые подборы возвращаются через BR_PICK_RESPAWN_S
int brPickShown(const BonusRace *b, int p);               // 1 — подбор p стоит (рисовать модель)
// нитро машины i: разгон вдоль курса (thr — газ 0..1; водитель ИИ — 1), пока колёса на земле; 1 — действует
int brBoost(BonusRace *b, int i, Car *k, float thr, float dt);
// сверка g07b-msnview: гудок = толчок нитро. brHorn — человек (1 — сработало); brAiHorn — раз в кадр для ИИ (решает сам); brGoNitro — на «GO!» часть ИИ тратит заряд сразу
int brHorn(BonusRace *b, int i, Car *k);
int brAiHorn(BonusRace *b, int i, Car *k, float dt);
int brGoNitro(BonusRace *b, Car *const *cars);
// DEATH трассы (JMP1): машина в объёме — на точку возврата данных, курсом локатора; ПРАВИЛО ПОРТА: курс, смотрящий против хода
// гонки у точки возврата (по плотному пути хода), развёрнут на π (b06 death/death1 по ходу смотрят назад; обратный ход). 1 — вернули.
// ПРАВИЛО ПОРТА для соперников ИИ (не игрока b->human): с BR_DEATH_AI_N-го DEATH подряд без нового чекпойнта — не на точку данных,
// а на свой путь вперёд (brRespawn, как возврат застрявшего), счёт «застрял» (brStuck) — заново. Почему: водитель ИИ порта не
// берёт трамплины b06 и падает под эстакаду; с точки данных (до трамплина) он падал снова (полигон: возвратов 10–12 за 3 круга)
int brDeath(BonusRace *b, int i, Car *k, const ColWorld *w);
// ловушки: pos/grow — машины (как brUpdate). Машина в объёме датчика — выход объекта (если стоит внизу). -> маска сработавших
unsigned brTrapStep(BonusRace *b, const float (*pos)[3], int n, float dt);
// B04G: машина i в сфере поршня, пока поршни выходят, — вертикальная скорость не меньше данных. 1 — подбросило сейчас
int brLaunch(BonusRace *b, int i, Car *k);
// свет зоны в точке p (ARGB данных, 0 — вне зон); brLightMul — множитель цвета lerp(белый, RGB, альфа) как ABGR для GE
uint32_t brLight(const BonusRace *b, const float p[3]);
uint32_t brLightMul(uint32_t argb);
// объекты ловушек трассы в ANM1: объекты с живыми объёмами, без действия, клип больше кадра (b04 — pistons, b07 — spikes;
// экспорт проверяет, что такой объект на трассе один, BnsHdr.ntrapobj). -> сколько связано
int brTrapBind(BonusRace *b, const AnimObjs *a);
// кадр клипа ловушек -> позы экземпляров SPR1 и живые объёмы COL1 (aoPlace)
void brTrapApply(const BonusRace *b, AnimObjs *a, StateProps *sp, ColWorld *w);

#ifdef __cplusplus
}
#endif
#endif
