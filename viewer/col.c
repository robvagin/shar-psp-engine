#ifndef SEAM_WALL_OLD
#define SEAM_WALL_OLD 0
#endif
// Разбивка №581 для бисекции (15.10, Sonnet-Стены): каждая часть по умолчанию = SEAM_WALL_OLD (поведение прежнее); -DSEAM_x_OLD=1 откатывает только её
#ifndef SEAM_NEEDLE_OLD
#define SEAM_NEEDLE_OLD SEAM_WALL_OLD   // игольчатые треугольники стыка мешей не берутся в слой кузова
#endif
#ifndef SEAM_TRILO_OLD
#define SEAM_TRILO_OLD SEAM_WALL_OLD    // низ слоя для треугольников на опоре выше на COL_RAMP_STEP
#endif
#ifndef SEAM_FENCE_OLD
#define SEAM_FENCE_OLD 1    // забор ниже опоры машины: не держать по опоре (gather, crossFence). ПО УМОЛЧАНИЮ ОТКАТ (1): эта часть №581 снимала защиту карманов (правка 339, tests/pockets: переехали по опоре 12 вместо 0), а стыки (seam_wall_gate) от неё не зависят. -DSEAM_FENCE_OLD=0 — как у Gemini
#endif
// Коллизия уровня по секции COL1 — см. col.h. Чистый C99, float, одна аллокация в colInit.
//
// Как устроено:
//  * земля (colGround): вертикальный луч в одной клетке сетки 4 м — треугольники с n.y > 0.5 (точка в треугольнике
//    по xz + высота плоскости), ящики (луч против OBB, берём грань входа, если она проходимая), цилиндры/капсулы/сферы;
//  * кузов (colPushCar): прямоугольник в xz и твёрдый слой [пол+lo, пол+hi] (пол — плоскость под колёсами, может
//    быть наклонной). Каждый примитив режется этим слоем, срез проецируется в xz: выпуклый набор точек (+ радиус
//    у капсул/сфер) и оси для SAT. Забор — толстый (FENCE_D) четырёхугольник за линией, SAT с допустимыми
//    направлениями: никогда внутрь (на изнанку), вдоль забора — только за выпуклый/свободный конец; стыки звеньев
//    без выпуклого излома продлены на FENCE_EXT и вдоль не толкают (иначе машина цеплялась бы за стыки).
//    Выталкиваем итерациями: каждый раз самый глубокий контакт, потом пересчёт всех.
#include "col.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef char col_phonex_80[sizeof(ColPhoneX) == 80 ? 1 : -1]; typedef char col_zone_80[sizeof(ColZone) == 80 ? 1 : -1]; typedef char col_pad_96[sizeof(ColPad) == 96 ? 1 : -1];   // формат JMP1
typedef char col_wrench_32[sizeof(ColWrench) == 32 ? 1 : -1];
#ifdef COL_DEBUG
#include <stdio.h>
int colDebug = 0;
long colStat[4];   // вызовы: gather, colGround, colCrossFence, итерации выталкивания
#define STAT(k) colStat[k]++
#else
#define STAT(k)
#endif

#define MAXSHAPE 128        // примитивов в узкой фазе за один запрос (обычно 0..20)
#define MAXP 32             // точек среза у одной фигуры (ящик: 8 углов + 24 пересечения рёбер с плоскостями)
#define MAXA 8              // собственных осей SAT у фигуры
#define GATHER_MARGIN 0.6f  // запас широкой фазы: кузов может сдвинуться за итерации
#define FENCE_EXT 2.0f      // продление забора за стык без выпуклого излома
#define FENCE_D 6.0f        // толщина забора «за линией» для SAT
#define PUSH_EPS 1e-4f
// Стеновая сторона горба (длинная лежачая капсула-не-куст, падение с террасы на откос): кузов выталкивается не рывком (до 0,39 м/кадр),
// а не больше HUMP_WALL_STEP за вызов; остаток глубины уходит на следующие кадры, нормаль и контакт прежние — скорость гасится как у стены.
// Изгороди №421 (pad[0]==1) и сам горб не трогаются. 1 — прежнее поведение.
#ifndef HUMP_WALL_SOFT_OLD
#define HUMP_WALL_SOFT_OLD 0
#endif
#define HUMP_WALL_STEP 0.10f
#define MAX_ITER 8
#define TRI_EDGE_EPS 2e-3f  // точка на ребре треугольника земли с запасом 2 мм (щели между соседними)
#define CAPSULE_TAPER_N 5    // правка 200: кругов в срезе сужающейся капсулы (<= 8: pr; осей 2·(N−1) <= MAXA)
#define CAPSULE_TAPER_EPS 0.02f
#define FENCE_BEHIND_FRESH 0.5f   // правка 200: «за забором» — центр за линией не глубже этого (только что пересёк)
#define GRAZE 0.02f        // примитив заходит в слой кузова меньше чем на 2 см сверху/снизу — не стена (иначе
                            // верх ящика вровень с низом слоя даёт тонкий срез на всю площадь и толчок вбок)

enum { SH_CONVEX = 0, SH_FENCE = 1 };
enum { SF_CVX_A = 1, SF_CVX_B = 2, SF_EXT_A = 4, SF_EXT_B = 8 };   // флаги концов заборов (строятся в colInit)
// Правка Роба 339 (05.10, L1 F4-8 у электростанции: «места, куда можно попасть и не выехать»). Внутренний забор (правка 47: верх =
// земля Intersect в полосе + до 3 м) ниже проезжей опоры, которая через него идёт: верх труб l1z6 (видимая земля и цилиндры, -4,3)
// над забором (-4,85) — машина по трубам переезжает линию оригинала и падает в щель между трубой и стеной (выезда нет: труба 3,6 м,
// край мира у стены). Замысел правки 47 — «по земле и подскоком держит, над видимым в полёте не держит» — теперь буквально: машине
// на опоре (CarBox.fsupOn) забор не ниже опоры + COL_FENCE_HOLD (кузов 1,6 м + подскок, как FENCE_MIN экспорта). Держит только
// подъезжающую с лица (кузов за линией не глубже COL_FENCE_SUP_PEN: толчок и запрет пересечь линию, colCrossFenceBox); стоящую поперёк
// линии (приземлилась, вытолкнуло) не держит — иначе каждый подшаг отменялся бы и машина замирала на линии; с изнанки опорный верх не
// держит. Полёт, изгороди (правка 173), границы карты — как были
#ifndef COL_FENCE_SUP
#define COL_FENCE_SUP 1
#endif
#define COL_FENCE_HOLD 2.5f
#define COL_FENCE_SUP_PEN 0.25f
enum { SF_SUP = 16 };   // флаг фигуры: забор попал в запрос только по опоре машины (правка 339)
int colFenceSupOff = !COL_FENCE_SUP;

#ifndef THIN_END_OLD
#define THIN_END_OLD 0   // №687: 1 — прежнее, конец отрезка-стены (ступенька шва ≥ 1 см) выталкивает машину назад по ходу вместо нормали стены
#endif
#ifndef THIN_END_W
#define THIN_END_W 16.f
#endif
typedef struct {
  unsigned char kind, ptype, np, na;
  uint32_t idx;
  float rad;
  float fa[2], fd[2], fn[2], L, t0, t1;   // забор: начало, направление, нормаль, длина, пределы вдоль
  unsigned char fflags;                   // SF_* концов
  float p[MAXP][2];                       // выпуклая фигура: точки среза (xz)
  float a[MAXA][2];                       // оси SAT (единичные)
  unsigned char nbv;                      // правка 200: у выпуклого конца (бит 0 — начало, 1 — конец) есть сосед: его точка и нормаль
  float nbp[2][2], nbn[2][2];
  unsigned char thin;                     // №687: срез — отрезок (вертикальный треугольник без толщины): оси борта машины в выборе выталкивания взвешены THIN_END_W
  unsigned char prv;                      // правка 200: 1 — у точек свой радиус pr (оболочка кругов), 0 — общий rad
  float pr[8];
  uint8_t rail;   // №1050 (Б): коробка — борт трамплина без торцов (railIs по миру, ставится при создании фигуры)
} Shape;

struct ColScratch {
  uint32_t rail1050[8]; unsigned nrail1050;   // №1050 (Б): борта трамплина этого мира (не глобально: комнаты тоже зовут colInit)
  uint16_t *stamp; uint16_t cur;
  uint32_t *humpAt; uint8_t *hump;   // горбы капсул: начало битов капсулы (~0 — не горб), по 2 бита на шаг оси (стороны)
  uint8_t *segFlags;
  uint32_t *segNb;          // правка 200: сосед по углу у каждого конца забора (2 на забор: начало, конец), ~0u — нет; нет памяти — NULL
  uint8_t *steep;           // бит на треугольник: n.y <= COL_WALK_NY (стена) — чтобы не читать землю в широкой фазе
  uint32_t *fol; uint32_t nfol;   // объёмы-кусты (тип << 29 | номер) и их число — для colFoliageNear без обхода сетки
  float *folBox;                  // их габариты в плане: x0, z0, x1, z1 на объём
  uint32_t *folSeg; uint32_t nfolSeg;   // правка 304: заборы-изгороди (y0 < COL_SEG_FOL_Y0) — для colFoliageCover
#if !COLPROBE_FAST_OLD
  uint8_t *cellWall;        // смещение первого препятствия (не земли) в клетке; 0 если есть отвесный треугольник
#endif
  int nshape;
  Shape shape[MAXSHAPE];
};

static float fminf_(float a, float b) { return a < b ? a : b; }
static float fmaxf_(float a, float b) { return a > b ? a : b; }

const char *colError(int code) {
  switch (code) {
  case 0: return "ok";
  case -1: return "COL1: section too small";
  case -2: return "COL1: section not 4-byte aligned";
  case -3: return "COL1: bad magic";
  case -4: return "COL1: bad grid";
  case -5: return "COL1: arrays exceed section size";
  case -6: return "COL1: cellStart broken";
  case -7: return "COL1: bad reference";
  case -8: return "COL1: out of memory";
  default: return "COL1: unknown error";
  }
}

// ---------------------------------------------------------------------------------------------- заборы: стыки
typedef struct { float x, z; uint32_t id; } EndPt;   // id = seg*2 + конец (0 = a, 1 = b)
static int cmpEnd(const void *a, const void *b) {
  float d = ((const EndPt *)a)->x - ((const EndPt *)b)->x; return d < 0 ? -1 : d > 0;
}
static int fenceFlags(ColWorld *w) {
  uint32_t n = w->nseg; uint8_t *fl = w->scr->segFlags;
  if (!n) return 0;
  EndPt *e = (EndPt *)malloc(sizeof(EndPt) * 2 * n);
  int32_t *nb = (int32_t *)malloc(sizeof(int32_t) * 2 * n);
  if (!e || !nb) { free(e); free(nb); return -8; }
  for (uint32_t i = 0; i < n; i++) {
    const ColSeg *s = &w->seg[i];
    e[2 * i] = (EndPt){s->a[0], s->a[1], 2 * i}; e[2 * i + 1] = (EndPt){s->b[0], s->b[1], 2 * i + 1};
  }
  qsort(e, 2 * n, sizeof(EndPt), cmpEnd);
  for (uint32_t i = 0; i < 2 * n; i++) nb[i] = -1;
  const float tol = 2e-3f;
  for (uint32_t i = 0; i < 2 * n; i++) {
    if (nb[e[i].id] >= 0) continue;
    for (uint32_t j = i + 1; j < 2 * n && e[j].x - e[i].x <= tol; j++) {
      if ((e[j].id >> 1) == (e[i].id >> 1) || nb[e[j].id] >= 0) continue;
      if (fabsf(e[j].z - e[i].z) <= tol) { nb[e[i].id] = (int32_t)e[j].id; nb[e[j].id] = (int32_t)e[i].id; break; }
    }
  }
  for (uint32_t i = 0; i < n; i++) fl[i] = 0;
  w->scr->segNb = (uint32_t *)malloc(sizeof(uint32_t) * 2 * n);   // правка 200: соседи по углам (нет памяти — без них, как раньше)
  if (w->scr->segNb) for (uint32_t id = 0; id < 2 * n; id++) w->scr->segNb[id] = nb[id] >= 0 ? (uint32_t)nb[id] >> 1 : ~0u;
  for (uint32_t id = 0; id < 2 * n; id++) {
    uint32_t si = id >> 1, end = id & 1;
    const ColSeg *s = &w->seg[si];
    float dx = s->b[0] - s->a[0], dz = s->b[1] - s->a[1], L = sqrtf(dx * dx + dz * dz);
    if (L < 1e-4f) continue;
    int32_t o = nb[id];
    uint8_t pt = end ? SF_CVX_B : SF_CVX_A, ext = end ? SF_EXT_B : SF_EXT_A;
    if (o < 0) { fl[si] |= pt; continue; }                      // свободный конец — угол
    uint32_t sj = (uint32_t)o >> 1, endj = (uint32_t)o & 1;
    const ColSeg *t = &w->seg[sj];
    float ex = t->b[0] - t->a[0], ez = t->b[1] - t->a[1], Lj = sqrtf(ex * ex + ez * ez);
    if (Lj < 1e-4f) { fl[si] |= pt; continue; }
    // направления от общей вершины внутрь каждого звена
    float uix = end ? -dx / L : dx / L, uiz = end ? -dz / L : dz / L;
    float ujx = endj ? -ex / Lj : ex / Lj, ujz = endj ? -ez / Lj : ez / Lj;
    float bx = uix + ujx, bz = uiz + ujz, bl = sqrtf(bx * bx + bz * bz);
    float nx = s->n[0] + t->n[0], nz = s->n[1] + t->n[1];
    if (bl < 0.02f) { fl[si] |= ext; continue; }                 // прямо: продлеваем, угла нет
    if (bx * nx + bz * nz <= 0) fl[si] |= pt;                   // выпуклый угол (непроезжее торчит к дороге)
    else fl[si] |= ext;                                          // вогнутый: полуплоскость продлеваем
  }
  free(e); free(nb);
  return 0;
}

// Правка Роба 421 (09.10): правка 173 была только про КУСТЫ; изгороди — как у оригинала. По данным COL1 листва (CollisionEffect eShrub)
// двух семей: лежачие капсулы изгородей (l1/l4/l7_hedgeunit: ось горизонтальна, 4,6 м, r 0,66–0,7; 105/105/102 шт.) и стоячие кусты
// (l2/l5_shrubsml: капсула + ящик, ось вверх). Сверка PS2 (docs/handoff/СТЕНЫ_СВЕРКА_PS2_2026-10-09.md): у капсулы изгороди машину
// выталкивает (fence_b001, fence_f008: 5–7 м), разгон в изгородь встаёт через 1,0–1,5 м (hedge000/004, fence_f006), J9-4 — за 2,8 м.
// Изгородь — твёрдая мягкая преграда (материал FOLIAGE: отклик куста правки 71), куст — насквозь (173). -DCOL_HEDGE_PASS_OLD=1 — прежнее
#ifndef COL_HEDGE_PASS_OLD
#define COL_HEDGE_PASS_OLD 0
#endif
#define COL_HEDGE_AXIS_Y 0.3f   // |ось.y| меньше — капсула лежит (изгородь), больше — стоит (куст)
// Капсулы изгородей пока пропускаются, как раньше (COL_HEDGE_CAPSULE_SOLID 0): твёрдая низкая капсула (верх 0,25–0,8 м над землёй)
// уходит под слой кузова, машина въезжает на неё днищем и зависает (tests/foliage/hedges.c cyl 63: назад 0,69 м), а выталкивание
// из капсулы, как у PS2 при постановке (fence_b001, fence_f008: 5–7 м), этим не получается — это отклик кузова на низкое препятствие
// (car.c, агент физики). Изгородь у оригинала держит забор Wall под ней — он теперь твёрдый (segFol), в лоб встаёт как PS2
#ifndef COL_HEDGE_CAPSULE_SOLID
#define COL_HEDGE_CAPSULE_SOLID 0
#endif
// №587 (16.10, humps D): латка J9-4 сделала зарытые капсулы 104/105 листвой (pad=1), а лист для колёс не опора: машина поперёк горба (2 м/с, правка 200) проваливалась
// у края террасы на 1,2 м и садилась днищем (7 из 36 заездов стоят). Лежачая длинная капсула-листва (как горб: ось не круче 0,3, длина вдоль земли не меньше COL_HUMP_LEN)
// остаётся опорой для колёс (материал листва: предел вертикальной скорости FOLIAGE_VY); тело машины сквозь неё проходит как прежде (colPassFol). HUMP_FOLIAGE_SUPPORT_OLD=1 — прежнее
#ifndef HUMP_FOLIAGE_SUPPORT_OLD
#define HUMP_FOLIAGE_SUPPORT_OLD 0
#endif
static int cylLowHump(const ColCyl *C) {
  float axl = hypotf(C->axis[0], C->axis[2]);
  return C->pad[0] == 1.f && fabsf(C->axis[1]) <= .3f && axl > 1e-4f && 2.f * C->halfLen * axl >= COL_HUMP_LEN;
}
static int cylBushPass(const ColCyl *C) {
  if (C->pad[0] != 1.f) return 0;
  return COL_HEDGE_PASS_OLD || !COL_HEDGE_CAPSULE_SOLID || fabsf(C->axis[1]) >= COL_HEDGE_AXIS_Y;
}
// Правка 414 (L5 K8-2 «фантом», решение Роба 10.10): капсула COL1 с материалом 2 (tools/psp_export.py col_phantom_pillars — невидимый столб
// посреди проезжей опоры) машину не держит ни боком, ни верхом; Гомер и пробы мира держат её как было. -DCOL_PHANTOM_OLD=1 — прежнее
#ifndef COL_PHANTOM_OLD
#define COL_PHANTOM_OLD 0
#endif
static int cylPhantom(const ColCyl *C) { return !COL_PHANTOM_OLD && C->pad[0] == 2.f; }
// Правка 475 (прозрачные стены, находки ботов): то же для коробки COL1 с материалом 2 (tools/psp_export.py col_phantom_volumes — вокруг
// объёма нет ни одного рисуемого треугольника и предмета). Машина её не держит ни боком, ни верхом, ни краем под кузовом; Гомер и
// пробы мира держат как было. -DCOL_PHANTOM_VOL_OLD=1 — прежнее (коробка 2 держит как обычная)
#ifndef COL_PHANTOM_VOL_OLD
#define COL_PHANTOM_VOL_OLD 0
#endif
static int boxPhantom(const ColBox *B) { return !COL_PHANTOM_VOL_OLD && B->pad == 2.f; }
// №525 (L1 J9-8, 11.10, голосом Роба: «должна быть горка, по которой въезжаешь»): тонкие наклонные плиты — скат детской горки и качелей у дома Гомера,
// такие же плиты на других уровнях (2 мм ... 0,3 м толщиной, верх положе 40° и круче 10°, не уже 0,4 м). Для кузова и колёс машины плита не стена
// (бок на 0,25 м вбок выталкивал вдоль носа и ставил машину намертво): остаётся опорой — верх даёт colGround, ребро под днищем — как было. Вертикальные
// стойки (длинная ось почти вертикальна, не плита) и толстые ящики — как прежде. Только шаги машины (colPassFol). -DSLOPE_PLATE_OLD=1 — прежнее
#ifndef SLOPE_PLATE_OLD
#define SLOPE_PLATE_OLD 0
#endif
#define SLOPE_PLATE_MAXT 0.3f
#define SLOPE_PLATE_NY_LO 0.766f
#define SLOPE_PLATE_NY_HI 0.985f

// №559 (L1 J10-1, 11.10, Роб: «скат крыши — если не перелетаешь, машина бьётся в середину ската как в стену, а по ней можно въехать как по горке»):
// расширение правила наклонных плит (№525) на скаты крыш:
// наклонные ящики (уклон 10°..53°, пролёт >= 0.8 м): их верх является проходимой опорой в colGround.
// Для колёс машины скат не является вертикальной стеной (colWheelWallContacts отсекает boxRoofSlope).
// В sliceBox отсечение трамплина (clipHalf) разрешено при smn < q->hi, позволяя въезжать на скат без удара о ступеньку.
// Флаг отката: -DROOF_RAMP_OLD=1
#ifndef ROOF_RAMP_OLD
#define ROOF_RAMP_OLD 0
#endif
#define ROOF_SLOPE_NY_LO 0.60f
#define ROOF_SLOPE_NY_HI 0.985f
#define ROOF_SLOPE_MIN_SPAN 0.8f
#define ROOF_SLOPE_MIN_BIG 2.0f
#ifndef ROOF_RAMP_SMALL_OLD
#define ROOF_RAMP_SMALL_OLD 0
#endif

static int boxRoofSlope(const ColBox *B) {
  if (ROOF_RAMP_OLD) return 0;
  int k = 0; float my = fabsf(B->ax[1]);
  if (fabsf(B->ay[1]) > my) { k = 1; my = fabsf(B->ay[1]); }
  if (fabsf(B->az[1]) > my) { k = 2; my = fabsf(B->az[1]); }
  if (my <= ROOF_SLOPE_NY_LO || my >= ROOF_SLOPE_NY_HI) return 0;
  for (int j = 0; j < 3; j++) if (j != k && 2.f * B->h[j] < ROOF_SLOPE_MIN_SPAN) return 0;
  // №187: у малого тела (цистерна J11-9, коробка ≈2×2×2 м) наклон — не скат крыши; иначе колесо не видит в нём стену и машина сползает с цистерны на 2,6 м. Крыша — хотя бы один пролёт ≥ 2·ROOF_SLOPE_MIN_BIG. ROOF_RAMP_SMALL_OLD=1 — как было
  if (!ROOF_RAMP_SMALL_OLD) { float big = 0.f; for (int j = 0; j < 3; j++) if (j != k && B->h[j] > big) big = B->h[j]; if (big < ROOF_SLOPE_MIN_BIG) return 0; }
  return 1;
}

static int boxSlopePlate(const ColBox *B) {
  if (SLOPE_PLATE_OLD) return 0;
  int k = 0; float my = fabsf(B->ax[1]);
  if (fabsf(B->ay[1]) > my) { k = 1; my = fabsf(B->ay[1]); }
  if (fabsf(B->az[1]) > my) { k = 2; my = fabsf(B->az[1]); }
  if (my <= SLOPE_PLATE_NY_LO || my >= SLOPE_PLATE_NY_HI || 2.f * B->h[k] > SLOPE_PLATE_MAXT) return 0;
  for (int j = 0; j < 3; j++) if (j != k && B->h[j] < 0.2f) return 0;
  return 1;
}
static int colPassFol, colFolHits;   // правка 173 (col.h): машина сквозь листву на время шага car.c
void colSetFoliagePass(int on) { colPassFol = on; colFolHits = 0; }
int colFoliageHits(void) { return colFolHits; }
int colFoliageNear(const ColWorld *w, float x, float y, float z, float margin);
static int folIn(const ColWorld *w, uint32_t k, float x, float y, float z, float margin);
static int segFol(const ColWorld *w, uint32_t i, float x, float y, float z) {   // забор-изгородь: помечен экспортом или у куста
  const ColSeg *G = &w->seg[i];
  // правка 421: забор Wall под изгородью — забор данных (у оригинала машина в изгороди встаёт); пометка экспорта «изгородь» и «у куста»
  // по изгороди — правило порта 173, оно только про кусты. Забор у КУСТА (стоячая листва, L2/L5 shrubsml) — насквозь, как было
  if (G->y0 < COL_SEG_FOL_Y0 && COL_HEDGE_PASS_OLD) return 1;
#ifndef FOLIAGE_PREV
  float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], l2 = dx * dx + dz * dz;
  float t = l2 > 1e-8f ? ((x - G->a[0]) * dx + (z - G->a[1]) * dz) / l2 : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
  float px = G->a[0] + dx * t, pz = G->a[1] + dz * t;
  if (COL_HEDGE_PASS_OLD) { if (colFoliageNear(w, px, y, pz, COL_FOLIAGE_FENCE)) return 1; }
  else if (w->scr && w->scr->fol) {
    for (uint32_t k = 0; k < w->scr->nfol; k++) {
      uint32_t r = w->scr->fol[k];
#ifndef COL_HEDGE_CLASS_OLD
#define COL_HEDGE_CLASS_OLD 0
#endif
      if ((r >> 29) == COL_CYL) {
        const ColCyl *C=&w->cyl[r & 0x1fffffffu];
        // Класс листвы от оси диска, отдельно от включения коллизии низкой капсулы
        // Иначе deferred COL_HEDGE_CAPSULE_SOLID=0 превращал изгородь в «куст» и пропускал Wall
        int bush=COL_HEDGE_CLASS_OLD ? cylBushPass(C) : fabsf(C->axis[1])>=COL_HEDGE_AXIS_Y;
        if(!bush)continue;
      }
      if (folIn(w, k, px, y, pz, COL_FOLIAGE_FENCE)) return 1;
    }
  }
#endif
  return 0;
}

// Правка 304 (сверка tests/foliage/hedges.c после снятия ложной вязкости): забор у изгороди решается «изгородь или нет» по ближней к
// центру кузова точке забора (segFol) — у забора, лишь краем лежащего у куста (L1 157.0 293–295 у изгороди cyl 61), решение
// менялось, пока кузов стоял на нём верхом: только что пропущенный забор становился твёрдым и выталкивал кузов за кадр на 2–3 м
// вбок и на 1.7 м вверх (с прежней вязкостью — на 24 м/с, без неё — уже на 15 м/с). Забор-изгородь, пропущенный на прошлом подшаге,
// остаётся пропущенным, пока он в досягаемости кузова (широкая фаза gather); car.c передаёт список, как горбы (colHumpKeep).
// -DCOL_FOL_FENCE_KEEP=0 — прежнее (красный гейта tests/foliage/hedges.c)
#ifndef COL_FOL_FENCE_KEEP
#define COL_FOL_FENCE_KEEP 1
#endif
static uint32_t fkIn[COL_FOL_KEEP], fkOut[COL_FOL_KEEP]; static int fkInN, fkOutN;
void colFolKeep(const uint32_t *idx, int n) {
  fkInN = 0; fkOutN = 0;
  for (int k = 0; idx && k < n && k < COL_FOL_KEEP; k++) fkIn[fkInN++] = idx[k];
}
int colFolRiding(uint32_t *idx, int max) { int n = 0; for (int k = 0; k < fkOutN && n < max; k++) idx[n++] = fkOut[k]; return n; }
static int segFolKeep(const ColWorld *w, uint32_t i, float x, float y, float z) {   // segFol + память пропущенных (на шаг машины)
  int on = segFol(w, i, x, y, z);
#if COL_FOL_FENCE_KEEP
  for (int k = 0; k < fkInN && !on; k++) on = fkIn[k] == i;
#endif
  if (on) { int seen = 0; for (int k = 0; k < fkOutN; k++) seen |= fkOut[k] == i; if (!seen && fkOutN < COL_FOL_KEEP) fkOut[fkOutN++] = i; }
  return on;
}

// Забор с изнанки (col.h colSetFenceBack, сверка PS2 09.10, docs/handoff/СТЕНЫ_СВЕРКА_PS2_2026-10-09.md): в 10 пробах L1, где машина
// стоит за внутренним забором и едет на линию, PS2 переезжает её на лицо (центр −1,0…−5,2 м → +0,7…+6,8 м), ни одна не упёрлась;
// порт упирался носом в 8 из 10 (центр −2,5…−3,4 м). Правило — сторона центра кузова, не место.
#ifndef COL_FENCE_BACK_OLD
#define COL_FENCE_BACK_OLD 0
#endif
static int colPassBack;
static uint32_t bkIn[COL_BACK_KEEP], bkOut[COL_BACK_KEEP]; static int bkInN, bkOutN;
void colSetFenceBack(int on) { colPassBack = on && !COL_FENCE_BACK_OLD; }
void colBackKeep(const uint32_t *idx, int n) {
  bkInN = 0; bkOutN = 0;
  for (int k = 0; idx && k < n && k < COL_BACK_KEEP; k++) bkIn[bkInN++] = idx[k];
}
int colBackRiding(uint32_t *idx, int max) { int n = 0; for (int k = 0; k < bkOutN && n < max; k++) idx[n++] = bkOut[k]; return n; }
static int segBackKept(uint32_t i) { for (int k = 0; k < bkInN; k++) if (bkIn[k] == i) return 1; return 0; }
// забор i не держит кузов (центр cx, cz; re — выступ прямоугольника кузова поперёк линии): центр с изнанки или кузов, пришедший
// с изнанки, ещё стоит на линии (центр на лице ближе re)
#ifndef COL_BACK_BOUND
#define COL_BACK_BOUND 0   // 1 — граница карты по тому же правилу (PS2 playable007_Af выезжает с изнанки границы); выключено: census
                           // tests/pockets на L3/L6 начинает стоять на полосе крыши между забором 40 и границей 1072 (прежде старт там замерзал) — ждёт решения
#endif
#ifndef COL_BACK_HOPS
#define COL_BACK_HOPS 4   // -DCOL_BACK_HOPS=1 — только ближний сосед (красный tests/walls/back_chain.c)
#endif
static int segBack(const ColWorld *w, uint32_t i, float cx, float cz, float re) {
  if (!colPassBack || i >= w->nseg) return 0;
  const ColSeg *G = &w->seg[i];
  // только внутренние заборы (конечный верх): граница карты (за ней пустота) — как была. С изнанки границы PS2 тоже выезжает
  // (playable007_Af), но census tests/pockets на L3/L6 ставит машину поперёк границы на крыше — открытый вопрос, не в этой правке
  if (G->y1 > 1e5f && !COL_BACK_BOUND) return 0;
  float cu = (cx - G->a[0]) * G->n[0] + (cz - G->a[1]) * G->n[1];
  // за линией В ПРЕДЕЛАХ звена (как back в evalShape): у выпуклого угла центр перед соседом бывает «за» продолжением этой линии —
  // это не изнанка, и запоминать такое звено нельзя (иначе, въехав в его пролёт, кузов прошёл бы его с лица: таран L2 J6-8)
  float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], l2 = dx * dx + dz * dz;
  float L = sqrtf(l2), t = L > 1e-4f ? ((cx - G->a[0]) * dx + (cz - G->a[1]) * dz) / L : -1e9f;   // м вдоль звена
  uint8_t f = w->scr && w->scr->segFlags ? w->scr->segFlags[i] : 0;   // прямой стык с соседом — пролёт продлён, как в gather (t0..t1)
  float t0 = (f & SF_EXT_A) ? -FENCE_EXT : 0.f, t1 = L + ((f & SF_EXT_B) ? FENCE_EXT : 0.f);
  int on = (cu < 0.f && t >= t0 && t <= t1) || (segBackKept(i) && cu < re);
  // за продолжением линии у конца звена, а соседнее по этому концу звено держит центр с изнанки в своём пролёте — кузов за той же
  // ломаной (у излома L1 K2-1 забор 1628 за 1627): изнанка и здесь, иначе прежнее правило толкало бы кузов вглубь
  // Ломаная идёт дальше через несколько звеньев (изгородь и твёрдый забор одной линией у L1 J12-9: 1140 → 1141 → 1142) — идём по
  // соседям в ту же сторону до COL_BACK_HOPS звеньев; звено-изгородь (правка 173) в цепочке тоже линия
  if (!on && cu < 0.f && w->scr && w->scr->segNb) {
    uint32_t cur = i; int e = t < t0 ? 0 : 1;
    for (int hop = 0; hop < COL_BACK_HOPS && !on; hop++) {
      uint32_t j = w->scr->segNb[2 * cur + e];
      if (j == ~0u || j >= w->nseg || j == i) break;
      const ColSeg *N = &w->seg[j]; float nx = N->b[0] - N->a[0], nz = N->b[1] - N->a[1], nl = sqrtf(nx * nx + nz * nz);
      if ((N->y1 > 1e5f && !COL_BACK_BOUND) || nl < 1e-4f) break;
      float cn = (cx - N->a[0]) * N->n[0] + (cz - N->a[1]) * N->n[1], tn = ((cx - N->a[0]) * nx + (cz - N->a[1]) * nz) / nl;
      if (cn >= 0.f) break;
      if (tn >= 0.f && tn <= nl) { on = 1; break; }
      // дальше по цепочке: конец соседа, противоположный стыку с текущим звеном
      e = (fabsf(N->a[0] - (e ? G->b[0] : G->a[0])) + fabsf(N->a[1] - (e ? G->b[1] : G->a[1]))) < 1e-3f ? 1 : 0;
      cur = j; G = N;
    }
  }
  if (on) { int seen = 0; for (int k = 0; k < bkOutN; k++) seen |= bkOut[k] == i; if (!seen && bkOutN < COL_BACK_KEEP) bkOut[bkOutN++] = i; }
  return on;
}

static void folList(ColWorld *w) {   // список кустов один раз (на L1 ~100): colFoliageNear проверяет только их; нет памяти — кустов нет
  ColScratch *S = w->scr; uint32_t n = 0, ns = 0;
  for (uint32_t i = 0; i < w->nseg; i++) ns += w->seg[i].y0 < COL_SEG_FOL_Y0;   // правка 304: заборы-изгороди (на L1/L4/L7 64–67)
  if (ns && (S->folSeg = (uint32_t *)malloc(sizeof(uint32_t) * ns)) != 0)
    for (uint32_t i = 0; i < w->nseg; i++) if (w->seg[i].y0 < COL_SEG_FOL_Y0) S->folSeg[S->nfolSeg++] = i;
  for (uint32_t i = 0; i < w->nbox; i++) n += w->box[i].pad == 1.f;
  for (uint32_t i = 0; i < w->ncyl; i++) n += w->cyl[i].pad[0] == 1.f;
  if (!n) return;
  S->fol = (uint32_t *)malloc(sizeof(uint32_t) * n); S->folBox = (float *)malloc(sizeof(float) * 4 * n);
  if (!S->fol || !S->folBox) { free(S->fol); free(S->folBox); S->fol = 0; S->folBox = 0; return; }
  for (uint32_t i = 0; i < w->nbox; i++) if (w->box[i].pad == 1.f) {
    const ColBox *B = &w->box[i]; float *bb = S->folBox + 4 * S->nfol;
    float hx = fabsf(B->ax[0]) * B->h[0] + fabsf(B->ay[0]) * B->h[1] + fabsf(B->az[0]) * B->h[2];
    float hz = fabsf(B->ax[2]) * B->h[0] + fabsf(B->ay[2]) * B->h[1] + fabsf(B->az[2]) * B->h[2];
    bb[0] = B->c[0] - hx; bb[1] = B->c[2] - hz; bb[2] = B->c[0] + hx; bb[3] = B->c[2] + hz; S->fol[S->nfol++] = ((uint32_t)COL_BOX << 29) | i;
  }
  for (uint32_t i = 0; i < w->ncyl; i++) if (w->cyl[i].pad[0] == 1.f) {
    const ColCyl *C = &w->cyl[i]; float *bb = S->folBox + 4 * S->nfol;
    float hx = fabsf(C->axis[0]) * C->halfLen + C->r, hz = fabsf(C->axis[2]) * C->halfLen + C->r;
    bb[0] = C->c[0] - hx; bb[1] = C->c[2] - hz; bb[2] = C->c[0] + hx; bb[3] = C->c[2] + hz; S->fol[S->nfol++] = ((uint32_t)COL_CYL << 29) | i;
  }
}

static void humpFlags(ColWorld *w) {   // см. humpHere: один раз после сетки; нет памяти — горбов нет (прежнее поведение)
  ColScratch *S = w->scr; uint32_t bits = 0;
  S->humpAt = (uint32_t *)malloc(sizeof(uint32_t) * (w->ncyl ? w->ncyl : 1)); if (!S->humpAt) return;
  for (uint32_t i = 0; i < w->ncyl; i++) {
    const ColCyl *C = &w->cyl[i]; float axl = hypotf(C->axis[0], C->axis[2]);
    S->humpAt[i] = ~0u;
    if (C->halfLen <= 0.f || fabsf(C->axis[1]) > .3f || axl < 1e-4f) continue;   // стволы, шары — нет
#ifdef FOLIAGE_PREV
    if (C->pad[0] == 1.f) continue;
#endif
    // короткая капсула — предмет (труба, бревно), не рельеф: остаётся стеной; низкая изгородь (куст) любой длины — переезжается
    // правка 421: изгородь (лежачая листва) — не горб: у оригинала машина в ней встаёт (сверка PS2), «переезжается» было правилом порта
    if ((C->pad[0] != 1.f || (COL_HEDGE_CAPSULE_SOLID && !COL_HEDGE_PASS_OLD)) && 2.f * C->halfLen * axl < COL_HUMP_LEN) continue;
    S->humpAt[i] = bits; bits += 2u * ((uint32_t)ceilf(2.f * C->halfLen * axl / COL_HUMP_DS) + 1u);
  }
  S->hump = (uint8_t *)calloc(bits / 8 + 1, 1); if (!S->hump) { free(S->humpAt); S->humpAt = 0; return; }
  for (uint32_t i = 0; i < w->ncyl; i++) {
    if (S->humpAt[i] == ~0u) continue;
    const ColCyl *C = &w->cyl[i]; float axl = hypotf(C->axis[0], C->axis[2]), ux = C->axis[0] / axl, uz = C->axis[2] / axl, L = 2.f * C->halfLen * axl;
    int n = (int)ceilf(L / COL_HUMP_DS) + 1;
    for (int k = 0; k < n; k++) {
      float t = k * COL_HUMP_DS - L * .5f; if (t > L * .5f) t = L * .5f;
      float ax = C->c[0] + ux * t, az = C->c[2] + uz * t, top = C->c[1] + C->axis[1] * (t / axl) + C->r;
      for (int side = 0; side < 2; side++) {
        float sg = side ? 1.f : -1.f, x = ax - uz * sg * (C->r + .3f), z = az + ux * sg * (C->r + .3f), g;
        if (!colGround(w, x, z, top + 1.f, top - 6.f, &g, 0)) continue;
        float e = top - g;
        // куст мягкий: крутизна у земли не важна (правка 71, правило порта: низкую изгородь в оригинале переезжают)
        if (e <= COL_HUMP && ((C->pad[0] == 1.f && (COL_HEDGE_PASS_OLD || !COL_HEDGE_CAPSULE_SOLID)) || e <= C->r * (1.f - COL_WALK_NY))) { uint32_t bit = S->humpAt[i] + 2u * (uint32_t)k + (uint32_t)side; S->hump[bit >> 3] |= (uint8_t)(1u << (bit & 7)); }
      }
    }
  }
}

static void pocketInit(ColWorld *w);
static void zcReset(void);
static void railInit(const ColWorld *w); static int railIs(const ColWorld *w, uint32_t i);   // №1050 (Б): ниже, у evalShape
int colInit(ColWorld *w, const void *section, unsigned size) {
  memset(w, 0, sizeof *w); zcReset();
  if (!section || size < sizeof(ColHdr)) return -1;
  if ((uintptr_t)section & 3) return -2;
  const ColHdr *h = (const ColHdr *)section;
  if (memcmp(h->magic, "COL1", 4)) return -3;
  if (!(h->cell > 0.f) || h->W == 0 || h->H == 0 || h->W > 65536 || h->H > 65536) return -4;
  uint64_t cells = (uint64_t)h->W * h->H;
  uint64_t need = 48 + (uint64_t)h->ntri * 48 + (uint64_t)h->nbox * 64 + (uint64_t)h->ncyl * 48 + (uint64_t)h->nseg * 32 +
                  (cells + 1) * 4 + (uint64_t)h->nref * 4;
  if (need > size) return -5;
  const char *p = (const char *)section + 48;
  w->tri = (const ColTri *)p; p += (size_t)h->ntri * 48;
  w->box = (const ColBox *)p; p += (size_t)h->nbox * 64;
  w->cyl = (const ColCyl *)p; p += (size_t)h->ncyl * 48;
  w->seg = (const ColSeg *)p; p += (size_t)h->nseg * 32;
  w->cellStart = (const uint32_t *)p; p += (size_t)(cells + 1) * 4;
  w->refs = (const uint32_t *)p;
  w->ox = h->ox; w->oz = h->oz; w->cell = h->cell; w->inv = 1.f / h->cell; w->W = (int)h->W; w->H = (int)h->H;
  w->ntri = h->ntri; w->nbox = h->nbox; w->ncyl = h->ncyl; w->nseg = h->nseg; w->nref = h->nref;
  if (w->cellStart[0] != 0 || w->cellStart[cells] != h->nref) return -6;
  for (uint64_t i = 0; i < cells; i++) if (w->cellStart[i] > w->cellStart[i + 1]) return -6;
  uint32_t cnt[4] = {w->ntri, w->nbox, w->ncyl, w->nseg};
  for (uint32_t i = 0; i < w->nref; i++) {
    uint32_t t = w->refs[i] >> 29, k = w->refs[i] & 0x1fffffffu;
    if (t > 3 || k >= cnt[t]) return -7;
  }
  uint32_t nprim = w->ntri + w->nbox + w->ncyl + w->nseg;
  size_t sz = sizeof(ColScratch) + (size_t)nprim * 2 + w->nseg + (w->ntri + 7) / 8 + 16;
  ColScratch *s = (ColScratch *)malloc(sz);
  if (!s) return -8;
  memset(s, 0, sz);
  s->stamp = (uint16_t *)(s + 1); s->segFlags = (uint8_t *)(s->stamp + nprim); s->steep = s->segFlags + w->nseg; s->cur = 0;
  for (uint32_t i = 0; i < w->ntri; i++) if (w->tri[i].n[1] <= COL_WALK_NY) s->steep[i >> 3] |= (uint8_t)(1u << (i & 7));
  w->scr = s;
#if !COLPROBE_FAST_OLD
  uint64_t ncells = (uint64_t)w->W * w->H;
  if (ncells <= 65536 * 4) {
    uint8_t *cw = (uint8_t *)malloc((size_t)ncells);
    if (cw) {
      for (uint64_t c = 0; c < ncells; c++) {
        uint32_t k0 = w->cellStart[c], k1 = w->cellStart[c + 1];
        uint32_t tri_count = 0; int has_steep = 0;
        for (uint32_t k = k0; k < k1; k++) {
          uint32_t t = w->refs[k] >> 29, idx = w->refs[k] & 0x1fffffffu;
          if (t == COL_TRI) {
            tri_count++;
            if (s->steep[idx >> 3] & (1u << (idx & 7))) has_steep = 1;
          } else break;
        }
        cw[c] = has_steep ? 0 : (uint8_t)(tri_count > 255 ? 255 : tri_count);
      }
      s->cellWall = cw;
    }
  }
#endif
  int r = fenceFlags(w);
  if (r) { colFree(w); return r; }
  humpFlags(w);
#ifndef BUSH_SOFT_OLD
#define BUSH_SOFT_OLD 0
#endif
  // №587: L1 J9-4 кусты податливее для проезда (метка #4: ColBox 32, ColCyl 104, 105 живой изгороди common_gens51Shape).
  // Латка по координатам оставлена (15.10, Sonnet-Стены): общего признака в данных коллизии нет — «gens» это любая статика уровня (~100 на уровень),
  // у gens51 physProp 0 как у стен, визуального меша с таким именем в сценах l1* нет; геометрия «длинный низкий» даёт 74 ящика L1 и 51 L2 (заборы,
  // стены). Затрагивает ровно 1 ящик + 2 капсулы на L1/L4/L7 (перепись), других уровней нет. Общее правило — только в экспортёре: объём считается
  // листвой, если видимая геометрия внутри него вся с текстурой FOL_TEX_RE (как у заборов, правка 173), для этого нужен перевыпуск уровней.
  if (!BUSH_SOFT_OLD) {
    for (uint32_t i = 0; i < w->nbox; i++) {
      ColBox *B = (ColBox *)&w->box[i];
      if (B->pad == 0.f && B->c[0] > 125.f && B->c[0] < 155.f && B->c[2] > 145.f && B->c[2] < 175.f && B->h[0] > 10.f)
        B->pad = 1.f;
    }
    for (uint32_t i = 0; i < w->ncyl; i++) {
      ColCyl *C = (ColCyl *)&w->cyl[i];
      if (C->pad[0] == 0.f && C->c[0] > 125.f && C->c[0] < 155.f && C->c[2] > 140.f && C->c[2] < 175.f && C->halfLen > 10.f && C->r > 1.5f)
        C->pad[0] = 1.f;
    }
  }
  folList(w);
  // блок JMP1 (зоны прыжка, подкидывающие точки): смещение в flags; битый/за концом секции — уровень без него
  if (h->flags && h->flags >= need && ((uint64_t)h->flags + 32) <= size && !(h->flags & 3)) {
    const char *j = (const char *)section + h->flags; uint32_t v[6]; memcpy(v, j + 4, 24);   // версия, зон, точек, ключей, будок, DEATH
    if (!memcmp(j, "JMP1", 4) && v[0] == 1 && (uint64_t)h->flags + 32 + (uint64_t)v[1] * 80 + ((uint64_t)v[2] + v[5]) * 96 + ((uint64_t)v[3] + v[4]) * 32 <= size) {
      w->zone = (const ColZone *)(j + 32); w->nzone = v[1];
      w->pad = (const ColPad *)(j + 32 + (size_t)v[1] * 80); w->npad = v[2];
      w->wrench = (const ColWrench *)(j + 32 + (size_t)v[1] * 80 + (size_t)v[2] * 96); w->nwrench = v[3];
      w->phone = w->wrench + v[3]; w->nphone = v[4];   // правка 62: будки — после ключей (старые файлы: 0)
      w->death = (const ColPad *)(w->phone + v[4]); w->ndeath = v[5];   // правка 86: зоны DEATH — после будок
      uint32_t npx; memcpy(&npx, j + 28, 4);   // правка 166: будки+ (место машины CarStart, оси локатора) — после зон DEATH
      const char *px = (const char *)(w->death + v[5]);
      if (npx && npx == v[4] && (uint64_t)(px - (const char *)section) + (uint64_t)npx * sizeof(ColPhoneX) <= size) { w->phonex = (const ColPhoneX *)px; w->nphonex = npx; }
    }
  }
  pocketInit(w);
  railInit(w);
  return 0;
}

int colZoneContains(const ColZone *v, float x, float y, float z, float grow) {
  float d[3] = {x - v->c[0], y - v->c[1], z - v->c[2]};
  if (!v->shape) { float r = v->h[0] + grow; return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= r * r; }
  const float *A[3] = {v->ax, v->ay, v->az};
  for (int k = 0; k < 3; k++) if (fabsf(d[0] * A[k][0] + d[1] * A[k][1] + d[2] * A[k][2]) > v->h[k] + grow) return 0;
  return 1;
}
float colZoneTop(const ColZone *v) {
  if (!v->shape) return v->c[1] + v->h[0];
  return v->c[1] + fabsf(v->ax[1]) * v->h[0] + fabsf(v->ay[1]) * v->h[1] + fabsf(v->az[1]) * v->h[2];
}
int colJumpZoneAt(const ColWorld *w, float x, float y, float z) {
  for (uint32_t i = 0; i < w->nzone; i++) if (colZoneContains(&w->zone[i], x, y, z, 0.f)) return (int)i;
  return -1;
}
int colDeathFirst(const ColWorld *w, uint32_t i) {   // правка 349: тот же локатор — то же имя и та же точка возврата у предыдущей записи
  if (i == 0 || i >= w->ndeath) return 1;
  const ColPad *a = &w->death[i - 1], *b = &w->death[i];
  return strncmp(a->name, b->name, sizeof a->name) || memcmp(a->apex, b->apex, sizeof a->apex);
}
// №590 (K3-3 L1/L4/L7): «карман за забором покрышек» — полоса за забором, которая ограждает двор зоны DEATH R6D01 (горящие покрышки): ездить там
// нельзя нигде, сброс на точку возврата R6D01. Правило по данным, не по координатам: звенья забора с конечным верхом у ящиков зоны R6D01 (не дальше
// POCKET_NEAR по плану), сторона «за забором» — противоположная двору (по нормали звена); точка в полосе глубиной POCKET_DEPTH за звеном (вдоль звена
// с запасом POCKET_PAD) и по высоте от верха забора −POCKET_BELOW … +POCKET_ABOVE — карман. Уровни без R6D01 не затронуты. PLAYABLE_ZONE_OLD=1 — прежнего нет;
// POCKET_RECT_OLD=1 — прежнее Gemini (прямоугольник по координатам L1)
#ifndef POCKET_RECT_OLD
#define POCKET_RECT_OLD 0
#endif
#define POCKET_NEAR 8.f
#define POCKET_DEPTH 25.f
#define POCKET_PAD 2.f
#define POCKET_BELOW 8.f
#define POCKET_ABOVE 12.f
static void pocketInit(ColWorld *w) {
  w->npocket = 0;
  float bx0 = 1e30f, bx1 = -1e30f, bz0 = 1e30f, bz1 = -1e30f; int have = 0;
  for (uint32_t i = 0; i < w->ndeath; i++) {
    const ColPad *d = &w->death[i]; if (strncmp(d->name, "R6D01", sizeof d->name) || d->shape != 1) continue;
    float ex = fabsf(d->ax[0]) * d->h[0] + fabsf(d->ay[0]) * d->h[1] + fabsf(d->az[0]) * d->h[2];
    float ez = fabsf(d->ax[2]) * d->h[0] + fabsf(d->ay[2]) * d->h[1] + fabsf(d->az[2]) * d->h[2];
    bx0 = fminf(bx0, d->c[0] - ex); bx1 = fmaxf(bx1, d->c[0] + ex); bz0 = fminf(bz0, d->c[2] - ez); bz1 = fmaxf(bz1, d->c[2] + ez); have = 1;
  }
  if (!have) return;
  float cx = .5f * (bx0 + bx1), cz = .5f * (bz0 + bz1);
  for (uint32_t i = 0; i < w->nseg && w->npocket < 32; i++) {
    const ColSeg *G = &w->seg[i]; if (G->y1 > 1e5f) continue;
    float mx = .5f * (G->a[0] + G->b[0]), mz = .5f * (G->a[1] + G->b[1]);
    float dx = fmaxf(fmaxf(bx0 - mx, mx - bx1), 0.f), dz = fmaxf(fmaxf(bz0 - mz, mz - bz1), 0.f);
    if (dx * dx + dz * dz > POCKET_NEAR * POCKET_NEAR) continue;
    float yardSide = (cx - G->a[0]) * G->n[0] + (cz - G->a[1]) * G->n[1];   // двор (зона) по эту сторону нормали
    if (fabsf(yardSide) < 1.f) continue;
    w->pocketSeg[w->npocket] = i; w->pocketSide[w->npocket] = yardSide > 0.f ? -1.f : 1.f; w->npocket++;   // «за забором» — со знаком противоположным двору
  }
}
static int pocketAt(const ColWorld *w, float x, float y, float z) {
  for (uint32_t k = 0; k < w->npocket; k++) {
    const ColSeg *G = &w->seg[w->pocketSeg[k]];
    float ex = G->b[0] - G->a[0], ez = G->b[1] - G->a[1], L = sqrtf(ex * ex + ez * ez); if (L < 1e-3f) continue;
    float t = ((x - G->a[0]) * ex + (z - G->a[1]) * ez) / L;                    // вдоль звена
    float s = ((x - G->a[0]) * G->n[0] + (z - G->a[1]) * G->n[1]) * w->pocketSide[k];   // вглубь кармана: > 0 за забором (знак — от нормали)
    if (t < -POCKET_PAD || t > L + POCKET_PAD || s < 0.f || s > POCKET_DEPTH) continue;
    if (y < G->y1 - POCKET_BELOW || y > G->y1 + POCKET_ABOVE) continue;
    return 1;
  }
  return 0;
}
int colDeathAtEx(const ColWorld *w, float x, float y, float z, float grow, int first) {
#ifndef PLAYABLE_ZONE_OLD
#define PLAYABLE_ZONE_OLD 0
#endif
  if (!PLAYABLE_ZONE_OLD) {
    int hit = 0;
    if (POCKET_RECT_OLD) hit = w->ndeath > 11 && x > 250.f && x < 300.f && z > -487.8f && z < -465.f && y > 2.f && y < 20.f;
    else hit = pocketAt(w, x, y, z);
    if (hit) for (uint32_t i = 0; i < w->ndeath; i++) if (!strncmp(w->death[i].name, "R6D01", sizeof w->death[i].name)) return (int)i;
  }
  for (uint32_t i = 0; i < w->ndeath; i++)
    if ((!first || colDeathFirst(w, i)) && colZoneContains((const ColZone *)&w->death[i], x, y, z, grow)) return (int)i;
  return -1;
}
// есть ли под точкой (x, y, z) зона DEATH не дальше maxDrop метров вниз (проверка пробой точкой по высоте зоны: низ, середина, верх, 0.5 м шаг не нужен — зоны толщиной от 2 м)
int colDeathBelow(const ColWorld *w, float x, float y, float z, float maxDrop) {
  for (uint32_t i = 0; i < w->ndeath; i++) {
    const ColPad *d = &w->death[i]; float hy = d->h[1], cy = d->c[1];
    for (int k = 0; k < 5; k++) {
      float yy = cy + hy * (k - 2) * .45f;   // нижняя, средняя, верхняя части
      if (yy > y + .5f || yy < y - maxDrop) continue;
      if (colZoneContains((const ColZone *)d, x, yy, z, .5f)) return (int)i;
    }
  }
  return -1;
}
int colDeathAt(const ColWorld *w, float x, float y, float z, float grow) { return colDeathAtEx(w, x, y, z, grow, 0); }
int colDeathHeading(const ColWorld *w, uint32_t i, float *h) {
  if (i >= w->ndeath || !w->death[i].pad) return 0;
  float f; memcpy(&f, &w->death[i].pad, 4); f -= 16.f;
  if (!(f > -7.f && f < 7.f)) return 0;   // не курс (битые данные)
  *h = f; return 1;
}
int colBouncePadAt(const ColWorld *w, float x, float y, float z, float grow) {
  for (uint32_t i = 0; i < w->npad; i++) if (colZoneContains((const ColZone *)&w->pad[i], x, y, z, grow)) return (int)i;
  return -1;
}

void colFree(ColWorld *w) {
  zcReset();
  if (w->scr) {
    free(w->scr->segNb); free(w->scr->humpAt); free(w->scr->hump); free(w->scr->fol); free(w->scr->folBox); free(w->scr->folSeg);
#if !COLPROBE_FAST_OLD
    free(w->scr->cellWall);
#endif
    free(w->scr);
  }
  w->scr = 0;
}

static uint16_t nextStamp(ColScratch *s, uint32_t nprim) {
  if (++s->cur == 0) { memset(s->stamp, 0, (size_t)nprim * 2); s->cur = 1; }
  return s->cur;
}

// ---------------------------------------------------------------------------------------------- земля
int colGround(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3) {
  return colGroundMat(w, x, z, yTop, yBottom, outY, outN3, 0);
}

int colGroundMat(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3, int *outMat) {
  return colGroundMatId(w,x,z,yTop,yBottom,outY,outN3,outMat,0);
}

int colGroundMatId(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3, int *outMat, uint32_t *outRef) {
  STAT(1);
  float fx = (x - w->ox) * w->inv, fz = (z - w->oz) * w->inv;
  if (!(fx >= 0.f && fz >= 0.f)) return 0;
  int gx = (int)fx, gz = (int)fz;
  if (gx >= w->W || gz >= w->H) return 0;
  uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx;
  const uint32_t *r = w->refs + w->cellStart[c], *e = w->refs + w->cellStart[c + 1];
  float best = -3.0e38f, bn0 = 0, bn1 = 1, bn2 = 0; int found = 0, bmat = COL_MAT_HARD; uint32_t bref=~0u;
  for (; r < e; r++) {
    uint32_t t = *r >> 29, i = *r & 0x1fffffffu;
    if (t == COL_TRI) {
      const ColTri *T = &w->tri[i];
      if (T->n[1] <= COL_WALK_NY) continue;
      float ax = T->a[0], az = T->a[2], bx = T->b[0], bz = T->b[2], cx = T->c[0], cz = T->c[2];
      // n.y > 0  =>  cross2(b-a, c-a) < 0: внутри все три функции рёбер <= 0 (с запасом TRI_EDGE_EPS * длина ребра)
      float e0x = bx - ax, e0z = bz - az, w0 = e0x * (z - az) - e0z * (x - ax);
      if (w0 > 0 && w0 * w0 > TRI_EDGE_EPS * TRI_EDGE_EPS * (e0x * e0x + e0z * e0z)) continue;
      float e1x = cx - bx, e1z = cz - bz, w1 = e1x * (z - bz) - e1z * (x - bx);
      if (w1 > 0 && w1 * w1 > TRI_EDGE_EPS * TRI_EDGE_EPS * (e1x * e1x + e1z * e1z)) continue;
      float e2x = ax - cx, e2z = az - cz, w2 = e2x * (z - cz) - e2z * (x - cx);
      if (w2 > 0 && w2 * w2 > TRI_EDGE_EPS * TRI_EDGE_EPS * (e2x * e2x + e2z * e2z)) continue;
      float y = T->a[1] - (T->n[0] * (x - ax) + T->n[2] * (z - az)) / T->n[1];
      if (y <= yTop && y >= yBottom && y > best) { best = y; bn0 = T->n[0]; bn1 = T->n[1]; bn2 = T->n[2]; found = 1; bref=*r; bmat = COL_MAT_HARD; }
    } else if (t == COL_BOX) {
      const ColBox *B = &w->box[i];
      if (colPassFol && (B->pad == 1.f || boxPhantom(B))) continue;            // правка 173: машина на куст не встаёт — проходит сквозь (+475: невидимая коробка)
      const float *A[3] = {B->ax, B->ay, B->az};
      float dx = x - B->c[0], dy = yTop - B->c[1], dz = z - B->c[2];
      float tmin = -3.0e38f, tmax = 3.0e38f; int ek = -1; float es = 0;
      int miss = 0;
      for (int k = 0; k < 3; k++) {
        float lo = dx * A[k][0] + dy * A[k][1] + dz * A[k][2], dd = -A[k][1], hk = B->h[k];
        if (fabsf(dd) < 1e-7f) { if (lo < -hk || lo > hk) { miss = 1; break; } continue; }
        float t1 = (-hk - lo) / dd, t2 = (hk - lo) / dd, sg = -1.f;   // вход через грань -h (нормаль -ось), если dd > 0
        if (t1 > t2) { float q = t1; t1 = t2; t2 = q; sg = 1.f; }
        if (t1 > tmin) { tmin = t1; ek = k; es = sg; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) { miss = 1; break; }
      }
      if (miss || ek < 0 || tmin < 0.f) continue;         // мимо / верх выше yTop (начало луча внутри ящика)
      float ny = es * A[ek][1];
      if (ny <= COL_WALK_NY) continue;                      // первой встречается крутая грань — стоять нельзя
      float y = yTop - tmin;
      if (y >= yBottom && y > best) { best = y; bn0 = es * A[ek][0]; bn1 = ny; bn2 = es * A[ek][2]; found = 1; bref=*r;
                                      bmat = B->pad == 1.f ? COL_MAT_FOLIAGE : COL_MAT_HARD; }
    } else if (t == COL_CYL) {
      const ColCyl *C = &w->cyl[i];
      if (colPassFol && (C->pad[0] == 1.f || cylPhantom(C)) && (HUMP_FOLIAGE_SUPPORT_OLD || cylPhantom(C) || !cylLowHump(C))) continue;         // правка 173 (+414: невидимый столб): на листву (и куст, и изгородь) машина не встаёт — у PS2 из капсулы изгороди выталкивает вбок (421); №587/200: лежачая низкая листва-горб остаётся опорой колёс
      float rc = C->r, hl = C->halfLen, dx = x - C->c[0], dz = z - C->c[2];
      float axz = sqrtf(C->axis[0] * C->axis[0] + C->axis[2] * C->axis[2]), rr = rc + hl * axz;
      if (dx * dx + dz * dz > rr * rr) continue;
      float y, n0, n1, n2;
      if (fabsf(C->axis[1]) > 0.999f || hl <= 0.f) {         // вертикальный цилиндр/капсула или сфера
        float d2 = dx * dx + dz * dz; if (d2 > rc * rc) continue;
        float top = C->c[1] + hl * fabsf(C->axis[1]);
        if (C->flat && hl > 0.f) { y = top; n0 = 0; n1 = 1; n2 = 0; }
        else { float hh = sqrtf(rc * rc - d2); y = top + hh; n0 = dx / rc; n1 = hh / rc; n2 = dz / rc; }
      } else {                                               // наклонная капсула (наклонный плоский цилиндр — как капсула)
        float pax = C->c[0] - C->axis[0] * hl, pay = C->c[1] - C->axis[1] * hl, paz = C->c[2] - C->axis[2] * hl;
        float bax = 2 * hl * C->axis[0], bay = 2 * hl * C->axis[1], baz = 2 * hl * C->axis[2];
        float oax = x - pax, oay = yTop - pay, oaz = z - paz;   // луч: o + t * (0,-1,0)
        float baba = bax * bax + bay * bay + baz * baz, bard = -bay, baoa = bax * oax + bay * oay + baz * oaz;
        float rdoa = -oay, oaoa = oax * oax + oay * oay + oaz * oaz;
        float qa = baba - bard * bard, qb = baba * rdoa - baoa * bard, qc = baba * oaoa - baoa * baoa - rc * rc * baba;
        float hh = qb * qb - qa * qc; if (hh < 0.f || qa <= 0.f) continue;
        float tt = (-qb - sqrtf(hh)) / qa, yy = baoa + tt * bard;
        if (!(yy > 0.f && yy < baba)) {
          float ocx = yy <= 0.f ? oax : x - (pax + bax), ocy = yy <= 0.f ? oay : yTop - (pay + bay), ocz = yy <= 0.f ? oaz : z - (paz + baz);
          float b2 = -ocy, c2 = ocx * ocx + ocy * ocy + ocz * ocz - rc * rc, h2 = b2 * b2 - c2;
          if (h2 <= 0.f) continue;
          tt = -b2 - sqrtf(h2);
        }
        if (tt < 0.f) continue;
        float hx = x, hy = yTop - tt, hz = z;
        float u = ((hx - pax) * bax + (hy - pay) * bay + (hz - paz) * baz) / baba; u = u < 0 ? 0 : u > 1 ? 1 : u;
        n0 = (hx - (pax + bax * u)) / rc; n1 = (hy - (pay + bay * u)) / rc; n2 = (hz - (paz + baz * u)) / rc;
        if (n1 <= COL_WALK_NY) continue;
        y = hy;
      }
      if (n1 <= COL_WALK_NY) continue;
      if (y <= yTop && y >= yBottom && y > best) { best = y; bn0 = n0; bn1 = n1; bn2 = n2; found = 1; bref=*r;
                                                   bmat = C->pad[0] == 1.f ? COL_MAT_FOLIAGE : COL_MAT_HARD; }
    }
  }
  if (found) { if (outY) *outY = best; if (outN3) { outN3[0] = bn0; outN3[1] = bn1; outN3[2] = bn2; } if (outMat) *outMat = bmat; if(outRef)*outRef=bref; }
  return found;
}

// ---------------------------------------------------------------------------------------------- кузов: фигуры
typedef struct { float x, z, s; } P3;   // точка: xz + высота над полом (s)

static void addAxis(Shape *sh, float ux, float uz) {
  float l = ux * ux + uz * uz;
  if (l < 1e-10f || sh->na >= MAXA) return;
  l = 1.f / sqrtf(l); ux *= l; uz *= l;
  for (int k = 0; k < sh->na; k++) if (fabsf(sh->a[k][0] * uz - sh->a[k][1] * ux) < 1e-4f) return;   // параллельна
  sh->a[sh->na][0] = ux; sh->a[sh->na][1] = uz; sh->na++;
}
static int clipS(const P3 *in, int n, P3 *out, float bound, int keepAbove) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    const P3 *c = &in[i], *p = &in[(i + n - 1) % n];
    int ci = keepAbove ? c->s >= bound : c->s <= bound, pi = keepAbove ? p->s >= bound : p->s <= bound;
    if (ci != pi) {
      float t = (bound - p->s) / (c->s - p->s);
      out[m].x = p->x + (c->x - p->x) * t; out[m].z = p->z + (c->z - p->z) * t; out[m].s = bound; m++;
    }
    if (ci) out[m++] = *c;
  }
  return m;
}

typedef struct { float cx, cz, y, gx, gz, lo, hi, x0, z0, x1, z1, ylo, yhi, fx, fz, hl, hw; float fsup; int fsupOn; } Q;   // запрос: пол, слой, габарит, кузов (правка 200), опора (339)
static float slabS(const Q *q, float x, float y, float z) { return y - q->y - q->gx * (x - q->cx) - q->gz * (z - q->cz); }

static int sliceTri(const Q *q, const ColTri *T, Shape *sh) {
  if (!SEAM_NEEDLE_OLD) {
    float v1x = T->b[0] - T->a[0], v1y = T->b[1] - T->a[1], v1z = T->b[2] - T->a[2];
    float v2x = T->c[0] - T->a[0], v2y = T->c[1] - T->a[1], v2z = T->c[2] - T->a[2];
    float cx = v1y * v2z - v1z * v2y, cy = v1z * v2x - v1x * v2z, cz = v1x * v2y - v1y * v2x;
    float area4 = cx*cx + cy*cy + cz*cz; // 4 * Area^2
    float e1 = v1x*v1x + v1y*v1y + v1z*v1z;
    float e2 = v2x*v2x + v2y*v2y + v2z*v2z;
    float v3x = T->c[0] - T->b[0], v3y = T->c[1] - T->b[1], v3z = T->c[2] - T->b[2];
    float e3 = v3x*v3x + v3y*v3y + v3z*v3z;
    float emax = fmaxf_(e1, fmaxf_(e2, e3));
    // Игольчатый треугольник сетки на стыке мешей: высота < 3 см при длине ребра > 0.5 м
    if (emax > 0.25f && area4 < 0.0009f * emax) return 0;
  }
  P3 a[3] = {{T->a[0], T->a[2], slabS(q, T->a[0], T->a[1], T->a[2])},
             {T->b[0], T->b[2], slabS(q, T->b[0], T->b[1], T->b[2])},
             {T->c[0], T->c[2], slabS(q, T->c[0], T->c[1], T->c[2])}};
  P3 b[5], c[6];
  float smin = fminf_(a[0].s, fminf_(a[1].s, a[2].s)), smax = fmaxf_(a[0].s, fmaxf_(a[1].s, a[2].s));
  float triLo = (!SEAM_TRILO_OLD && q->fsupOn) ? (q->lo + COL_RAMP_STEP) : q->lo;
  if (smax < triLo + GRAZE || smin > q->hi - GRAZE) return 0;
  int n = clipS(a, 3, b, triLo, 1); if (!n) return 0;
  n = clipS(b, n, c, q->hi, 0); if (!n) return 0;
  sh->kind = SH_CONVEX; sh->rad = 0; sh->np = 0; sh->na = 0; sh->prv = 0; sh->thin = 0;
  for (int i = 0; i < n; i++) { sh->p[sh->np][0] = c[i].x; sh->p[sh->np][1] = c[i].z; sh->np++; }
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n; float ex = c[j].x - c[i].x, ez = c[j].z - c[i].z;
    if (ex * ex + ez * ez > 1e-8f) addAxis(sh, -ez, ex);
  }
  if (!sh->na) {   // срез выродился в точку по xz (вертикальное ребро) — ось по нормали треугольника
    addAxis(sh, T->n[0], T->n[2]); if (!sh->na) addAxis(sh, 1, 0);
  }
  if (!THIN_END_OLD && sh->np >= 2) {   // №687: все точки среза на одной прямой (ширина < 1 см) — отрезок
    float dx = sh->p[sh->np - 1][0] - sh->p[0][0], dz = sh->p[sh->np - 1][1] - sh->p[0][1], dl = sqrtf(dx * dx + dz * dz);
    if (dl > 0.05f) { float w_ = 0.f; for (int i = 1; i + 1 < sh->np; i++) { float cr = (sh->p[i][0] - sh->p[0][0]) * dz - (sh->p[i][1] - sh->p[0][1]) * dx; if (fabsf(cr) / dl > w_) w_ = fabsf(cr) / dl; }
      sh->thin = w_ < 0.01f; }
  }
  return 1;
}

// выпуклая оболочка точек фигуры (xz) и срез полуплоскостью K + ux*x + uz*z > 0; 0 = ничего не осталось
static int clipHalf(Shape *sh, float K, float ux, float uz) {
  int n = sh->np;
  float ul = sqrtf(ux * ux + uz * uz);
  if (ul < 1e-5f) return K > 0.f;                                  // верх параллелен полу: весь или ничего
  float P[MAXP][2], H[2 * MAXP + 2][2], O[2 * MAXP + 4][2];
  for (int i = 0; i < n; i++) { P[i][0] = sh->p[i][0]; P[i][1] = sh->p[i][1]; }
  for (int i = 1; i < n; i++) {                                     // сортировка по x, затем z (точек <= 32)
    float x = P[i][0], z = P[i][1]; int j = i - 1;
    while (j >= 0 && (P[j][0] > x || (P[j][0] == x && P[j][1] > z))) { P[j + 1][0] = P[j][0]; P[j + 1][1] = P[j][1]; j--; }
    P[j + 1][0] = x; P[j + 1][1] = z;
  }
  int m = 0;                                                        // монотонная цепь
#define CR(o, a, b) (((a)[0] - (o)[0]) * ((b)[1] - (o)[1]) - ((a)[1] - (o)[1]) * ((b)[0] - (o)[0]))
  for (int i = 0; i < n; i++) { while (m >= 2 && CR(H[m - 2], H[m - 1], P[i]) <= 0) m--; H[m][0] = P[i][0]; H[m][1] = P[i][1]; m++; }
  for (int i = n - 2, t = m + 1; i >= 0; i--) { while (m >= t && CR(H[m - 2], H[m - 1], P[i]) <= 0) m--; H[m][0] = P[i][0]; H[m][1] = P[i][1]; m++; }
#undef CR
  if (m > 1) m--;                                                   // последняя = первая
  int k = 0;
  for (int i = 0; i < m; i++) {
    const float *c = H[i], *p = H[(i + m - 1) % m];
    float dc = K + ux * c[0] + uz * c[1], dp = K + ux * p[0] + uz * p[1];
    if ((dc > 0.f) != (dp > 0.f)) { float t = dp / (dp - dc); O[k][0] = p[0] + (c[0] - p[0]) * t; O[k][1] = p[1] + (c[1] - p[1]) * t; k++; }
    if (dc > 0.f) { O[k][0] = c[0]; O[k][1] = c[1]; k++; }
  }
  if (!k) return 0;
  if (k > MAXP) k = MAXP;
  for (int i = 0; i < k; i++) { sh->p[i][0] = O[i][0]; sh->p[i][1] = O[i][1]; }
  sh->np = (unsigned char)k;
  addAxis(sh, ux, uz);                                              // новая сторона — линия среза
  return 1;
}

static int sliceBox(const Q *q, const ColBox *B, Shape *sh) {
  float P[8][3], S[8];
  for (int i = 0; i < 8; i++) {
    float sx = (i & 1) ? B->h[0] : -B->h[0], sy = (i & 2) ? B->h[1] : -B->h[1], sz = (i & 4) ? B->h[2] : -B->h[2];
    for (int k = 0; k < 3; k++) P[i][k] = B->c[k] + B->ax[k] * sx + B->ay[k] * sy + B->az[k] * sz;
    S[i] = slabS(q, P[i][0], P[i][1], P[i][2]);
  }
  float smin = S[0], smax = S[0];
  for (int i = 1; i < 8; i++) { smin = fminf_(smin, S[i]); smax = fmaxf_(smax, S[i]); }
  if (smax < q->lo + GRAZE || smin > q->hi - GRAZE) return 0;
  sh->kind = SH_CONVEX; sh->rad = 0; sh->np = 0; sh->na = 0; sh->prv = 0; sh->thin = 0;
  for (int i = 0; i < 8; i++)
    if (S[i] >= q->lo && S[i] <= q->hi) { sh->p[sh->np][0] = P[i][0]; sh->p[sh->np][1] = P[i][2]; sh->np++; }
  for (int bit = 1; bit < 8; bit <<= 1)
    for (int i = 0; i < 8; i++) {
      if (i & bit) continue;
      int j = i | bit;
      for (int pl = 0; pl < 2; pl++) {
        float b = pl ? q->hi : q->lo, di = S[i] - b, dj = S[j] - b;
        if ((di < 0) == (dj < 0)) continue;
        float t = di / (di - dj);
        if (sh->np < MAXP) {
          sh->p[sh->np][0] = P[i][0] + (P[j][0] - P[i][0]) * t; sh->p[sh->np][1] = P[i][2] + (P[j][2] - P[i][2]) * t; sh->np++;
        }
      }
    }
  if (!sh->np) return 0;
  const float *A[3] = {B->ax, B->ay, B->az};
  for (int k = 0; k < 3; k++) {
    const float *e = A[k];
    addAxis(sh, -e[2], e[0]);                                    // рёбра ящика вдоль e
    float cx = -e[1] * q->gz - e[2], cz = e[0] + e[1] * q->gx;   // линия грани (нормаль e) на плоскости слоя
    addAxis(sh, -cz, cx);
  }
  if (!sh->na) addAxis(sh, 1, 0);
  // Трамплин: наклонный ящик с проходимым верхом, который выходит из-под слоя (нижний край зарыт, как лестница
  // пожарной машины у Kwik-E-Mart). Его верх — земля (colGround, колёса и днище въезжают), стена — только там, где
  // верх выше пола больше чем на lo + COL_RAMP_STEP. Иначе кромка клина толкает машину назад, как стена.
  {
    int k = 0; float my = fabsf(A[0][1]);
    if (fabsf(A[1][1]) > my) { k = 1; my = fabsf(A[1][1]); }
    if (fabsf(A[2][1]) > my) { k = 2; my = fabsf(A[2][1]); }
    if (my > COL_WALK_NY) {
      float sg = A[k][1] > 0 ? 1.f : -1.f, N[3] = {sg * A[k][0], sg * A[k][1], sg * A[k][2]}, T[3];
      for (int i = 0; i < 3; i++) T[i] = B->c[i] + N[i] * B->h[k];     // центр верхней грани
      int i1 = (k + 1) % 3, i2 = (k + 2) % 3; float smn = 3.0e38f;
      for (int cn = 0; cn < 4; cn++) {
        float a1 = (cn & 1) ? B->h[i1] : -B->h[i1], a2 = (cn & 2) ? B->h[i2] : -B->h[i2];
        float s = slabS(q, T[0] + A[i1][0] * a1 + A[i2][0] * a2, T[1] + A[i1][1] * a1 + A[i2][1] * a2, T[2] + A[i1][2] * a1 + A[i2][2] * a2);
        if (s < smn) smn = s;
      }
      if (smn < (ROOF_RAMP_OLD ? q->lo : q->hi)) {
        // высота верха над полом: s(x, z) = K + ux * x + uz * z; стена — где s > lo + шаг
        float ux = -N[0] / N[1] - q->gx, uz = -N[2] / N[1] - q->gz;
        float K = T[1] + (N[0] * T[0] + N[2] * T[2]) / N[1] - q->y + q->gx * q->cx + q->gz * q->cz - (q->lo + COL_RAMP_STEP);
        if (!clipHalf(sh, K, ux, uz)) return 0;
#if !COL_RAMP_RIDGE_OLD
        // Правки Роба 247/353 (09.10, конёк дома J10 у jumpzone17): ящик, повёрнутый так, что вверх смотрят ДВЕ проходимые грани
        // (скат крыши 40° и его продолжение за коньком 50°, ящики COL1 17 и 18), — сам конёк: его верх — нижняя огибающая обеих
        // плоскостей, а не продолженная за ребро плоскость одной. Прежнее правило брало только самую «верхнюю» грань: за коньком её
        // продолжение уходит вверх, и машина, долетевшая до конька на 90 км/ч, била ребро как отвесную стену (24,5 → 0,2 м/с;
        // у PS2 ps2_v103_house90_gas переползает конёк и садится за домом на ~23 м). Стена — где ОБЕ плоскости выше пола на lo + шаг.
        // Геометрия ящика с диска, не подгонка. Прежнее: -DCOL_RAMP_RIDGE_OLD=1
        for (int j = 0; j < 3; j++) {
          // только скаты-плоскости (обе грани не уже COL_RIDGE_MIN, col.h): мелкие повёрнутые детали — по-прежнему (J11-9, опоры 927/928)
          if (j == k || fabsf(A[j][1]) <= COL_WALK_NY || 2.f * B->h[j] < COL_RIDGE_MIN || 2.f * B->h[k] < COL_RIDGE_MIN) continue;
          float sj = A[j][1] > 0 ? 1.f : -1.f, N2[3] = {sj * A[j][0], sj * A[j][1], sj * A[j][2]}, T2[3];
          for (int i = 0; i < 3; i++) T2[i] = B->c[i] + N2[i] * B->h[j];
          float ux2 = -N2[0] / N2[1] - q->gx, uz2 = -N2[2] / N2[1] - q->gz;
          float K2 = T2[1] + (N2[0] * T2[0] + N2[2] * T2[2]) / N2[1] - q->y + q->gx * q->cx + q->gz * q->cz - (q->lo + COL_RAMP_STEP);
          if (!clipHalf(sh, K2, ux2, uz2)) return 0;
        }
#endif
      }
    }
  }
  return 1;
}

// Низкая изгородь (лежачая капсула-куст, правка 71, 01.10): торчит из земли с этой стороны не больше COL_HUMP — тоже горб,
// любой длины и крутизны у земли: кузов её не задевает, колёса и днище переезжают по её верху (опора-куст, car.c FOLIAGE_VY).
// Горб: длинная (от COL_HUMP_LEN) лежачая капсула-не-куст, чей верх торчит из земли рядом не больше COL_HUMP (как трамплин:
// lo 0.35 + шаг 0.4) и на линии земли ещё проходим (n.y ≥ COL_WALK_NY): это рельеф (кромка насыпи) — колёса въезжают по colGround,
// стеной для кузова не считать. Короткие (трубы culvert 4 м) остаются стенами до сверки с эталоном PCSX2: их открытие давало
// рост проникновений в рое (24 серии, 11 -> 27, машины заклинивало в канаве у трубы 81 -320).
// Земля рядом замеряется один раз в colInit через каждый метр оси, отдельно с каждой стороны (common_gens51 поперёк дороги к
// Kwik-E-Mart: с одной стороны горб на 0.3–0.5 м, с другой — откос на 2–3 м, там стена остаётся).
static int humpSide(const ColScratch *S, const ColCyl *C, uint32_t i, float cx, float cz);
static int humpHere(const ColScratch *S, const ColCyl *C, uint32_t i, const Q *q) {
  float cx = q->cx, cz = q->cz, cy = q->y;
  if (!S->humpAt || S->humpAt[i] == ~0u) return 0;
  float axl = hypotf(C->axis[0], C->axis[2]); if (axl < 1e-4f) return 0;
  float ux = C->axis[0] / axl, uz = C->axis[2] / axl, L = 2.f * C->halfLen * axl;
  float t = (cx - (C->c[0] - ux * L * .5f)) * ux + (cz - (C->c[2] - uz * L * .5f)) * uz;
#if !defined(HUMP_SIDE_ONLY) || !HUMP_SIDE_ONLY
  // правка Роба 200 (03.10, «через арку к стеклу медленно — кусты выталкивают в разные стороны»): сторона горба бралась по центру
  // машины — перевалил центр через ось капсулы, и та же капсула под днищем становилась стеной, срез сбоку толкал кузов вдоль неё
  // (до 0,9 м за кадр). Машина, чей низ уже на высоте верха капсулы (не ниже порога горба), переезжает её с любой стороны:
  // капсула под ней, не перед ней. Оригинал (близнец arch.py, 3 м/с) переезжает эту изгородь и бьёт стекло, без сноса вбок
  { float tc = t < 0.f ? 0.f : t > L ? L : t, top = C->c[1] + C->axis[1] * ((tc - L * .5f) / axl) + C->r;
    float thr = C->pad[0] == 1.f ? COL_HUMP : fminf_(COL_HUMP, C->r * (1.f - COL_WALK_NY));
    // ось под кузовом (машина съезжает с изгороди, зад ещё над ней) — высота днища над осью по наклону кузова, иначе центр
    float axx = C->c[0] + ux * (tc - L * .5f), axz = C->c[2] + uz * (tc - L * .5f), ox = axx - cx, oz = axz - cz;
    float lf = ox * q->fx + oz * q->fz, lr = -ox * q->fz + oz * q->fx;
    if (fabsf(lf) <= q->hl && fabsf(lr) <= q->hw) cy += q->gx * ox + q->gz * oz;
    if (cy >= top - thr) return 1; }
#else
  (void)cy;
#endif
  return humpSide(S, C, i, cx, cz);
}
static int humpSide(const ColScratch *S, const ColCyl *C, uint32_t i, float cx, float cz) {   // горбатая ли сторона капсулы у центра кузова
  if (!S->humpAt || S->humpAt[i] == ~0u) return 0;
  float axl = hypotf(C->axis[0], C->axis[2]); if (axl < 1e-4f) return 0;
  float ux = C->axis[0] / axl, uz = C->axis[2] / axl, L = 2.f * C->halfLen * axl;
  float t = (cx - (C->c[0] - ux * L * .5f)) * ux + (cz - (C->c[2] - uz * L * .5f)) * uz;
  int n = (int)ceilf(L / COL_HUMP_DS) + 1, k = (int)floorf(t / COL_HUMP_DS + .5f); if (k < 0) k = 0; if (k >= n) k = n - 1;
  uint32_t bit = S->humpAt[i] + 2u * (uint32_t)k + (uint32_t)(((cx - C->c[0]) * -uz + (cz - C->c[2]) * ux) > 0.f);
  return (S->hump[bit >> 3] >> (bit & 7)) & 1;
}

// правка 200, остаток сверки (col.h colHumpKeep): капсула-горб, на которой кузов стоит, не перескакивает в стену под ним. Решение
// humpHere берётся по одной точке пола и порогу — у кузова с креном (одной стороной на изгороди, другой у края ящика, L2 common306;
// съезд с изгороди наискось, L1 common_gens51, bushesShape1) оно от сотых долей метра менялось на «стена», и срез капсулы, уже
// глубоко под кузовом, выталкивал его за кадр вдоль изгороди до 1 м. Горб, под которым кузов был на прошлом подшаге, остаётся горбом,
// пока ось капсулы ближе её радиуса к прямоугольнику кузова; съехал — решает humpHere, как раньше (снизу, с откоса — стена)
static uint32_t hkIn[COL_HUMP_KEEP], hkOut[COL_HUMP_KEEP]; static int hkInN, hkOutN;
void colHumpKeep(const uint32_t *idx, int n) {
  hkInN = 0; hkOutN = 0;
  for (int k = 0; idx && k < n && k < COL_HUMP_KEEP; k++) hkIn[hkInN++] = idx[k];
}
int colHumpRiding(uint32_t *idx, int max) { int n = 0; for (int k = 0; k < hkOutN && n < max; k++) idx[n++] = hkOut[k]; return n; }
static float segRectDist(float ax, float az, float bx, float bz, const Q *q) {   // отрезок (xz) до прямоугольника кузова: 0 — пересекает
  float fx = q->fx, fz = q->fz, rx = -fz, rz = fx, hl = q->hl, hw = q->hw;
  float a0 = (ax - q->cx) * fx + (az - q->cz) * fz, a1 = (ax - q->cx) * rx + (az - q->cz) * rz;   // в осях кузова
  float b0 = (bx - q->cx) * fx + (bz - q->cz) * fz, b1 = (bx - q->cx) * rx + (bz - q->cz) * rz;
  float t0 = 0.f, t1 = 1.f, d0 = b0 - a0, d1 = b1 - a1, lo[2] = {-hl, -hw}, hi[2] = {hl, hw}, o[2] = {a0, a1}, d[2] = {d0, d1};
  int hit = 1;
  for (int k = 0; k < 2 && hit; k++) {   // отрезок против прямоугольника (плиты)
    if (fabsf(d[k]) < 1e-9f) { if (o[k] < lo[k] || o[k] > hi[k]) hit = 0; continue; }
    float u0 = (lo[k] - o[k]) / d[k], u1 = (hi[k] - o[k]) / d[k]; if (u0 > u1) { float t = u0; u0 = u1; u1 = t; }
    t0 = fmaxf_(t0, u0); t1 = fminf_(t1, u1);
    if (t0 > t1) hit = 0;
  }
  if (hit) return 0.f;
  float best = 3.0e38f;
  for (int e = 0; e < 2; e++) {           // концы отрезка до прямоугольника
    float p0 = e ? b0 : a0, p1 = e ? b1 : a1, c0 = p0 < -hl ? -hl : p0 > hl ? hl : p0, c1 = p1 < -hw ? -hw : p1 > hw ? hw : p1;
    float dd = (p0 - c0) * (p0 - c0) + (p1 - c1) * (p1 - c1); if (dd < best) best = dd;
  }
  float l2 = d0 * d0 + d1 * d1;
  for (int c = 0; c < 4 && l2 > 1e-12f; c++) {   // углы прямоугольника до отрезка
    float p0 = (c & 1) ? hl : -hl, p1 = (c & 2) ? hw : -hw, t = ((p0 - a0) * d0 + (p1 - a1) * d1) / l2; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    float e0 = a0 + d0 * t - p0, e1 = a1 + d1 * t - p1, dd = e0 * e0 + e1 * e1; if (dd < best) best = dd;
  }
  return sqrtf(best);
}
// горб с памятью подшага; кузов над горбом — в новый список. Впервые в память — только с горбатой стороны (бит humpFlags у центра
// кузова): снизу, со стороны-стены, мгновенное «горб» по полу задравшегося на склон кузова памяти не даёт (иначе стена переезжалась)
static int humpRide(const ColScratch *S, const ColCyl *C, uint32_t i, const Q *q, int hump) {
  float hl = C->halfLen;
  int over = segRectDist(C->c[0] - C->axis[0] * hl, C->c[2] - C->axis[2] * hl, C->c[0] + C->axis[0] * hl, C->c[2] + C->axis[2] * hl, q) < C->r;
  if (!over) return hump;
  int kept = 0; for (int k = 0; k < hkInN; k++) if (hkIn[k] == i) { kept = 1; break; }
  if (kept) hump = 1;
  if (hump && (kept || humpSide(S, C, i, q->cx, q->cz))) {
    int seen = 0; for (int k = 0; k < hkOutN; k++) seen |= hkOut[k] == i;
    if (!seen && hkOutN < COL_HUMP_KEEP) hkOut[hkOutN++] = i;
  }
  return hump;
}

static int sliceCyl(const Q *q, const ColCyl *C, Shape *sh) {
  float hl = C->halfLen, r = C->r;
  float p0x = C->c[0] - C->axis[0] * hl, p0y = C->c[1] - C->axis[1] * hl, p0z = C->c[2] - C->axis[2] * hl;
  float p1x = C->c[0] + C->axis[0] * hl, p1y = C->c[1] + C->axis[1] * hl, p1z = C->c[2] + C->axis[2] * hl;
  float s0 = slabS(q, p0x, p0y, p0z), s1 = slabS(q, p1x, p1y, p1z);
  float g2 = q->gx * q->gx + q->gz * q->gz, kN = sqrtf(1.f + g2), gs = sqrtf(g2);
  float smin = fminf_(s0, s1), smax = fmaxf_(s0, s1), L, U, rad;
  if (C->flat && hl > 0.f && fabsf(C->axis[1]) > 0.99f) {       // вертикальный цилиндр с плоскими торцами
    L = q->lo - r * gs; U = q->hi + r * gs;
    if (smax < L + GRAZE || smin > U - GRAZE) return 0;
    rad = r;
  } else {                                                       // капсула / сфера / наклонный (как капсула)
    float gap = fmaxf_(0.f, fmaxf_(q->lo - smax, smin - q->hi)) / kN;
    if (gap >= r - GRAZE) return 0;
    rad = sqrtf(r * r - gap * gap);
    L = q->lo - r * kN; U = q->hi + r * kN;
  }
  float ta = 0.f, tb = 1.f, ds = s1 - s0;
  if (fabsf(ds) > 1e-6f) {
    float u0 = (L - s0) / ds, u1 = (U - s0) / ds;
    if (u0 > u1) { float t = u0; u0 = u1; u1 = t; }
    ta = fmaxf_(0.f, u0); tb = fminf_(1.f, u1);
    if (ta > tb) return 0;
  }
  sh->kind = SH_CONVEX; sh->rad = rad; sh->np = 0; sh->na = 0; sh->prv = 0; sh->thin = 0;
#if !defined(CAPSULE_STADIUM) || !CAPSULE_STADIUM
  // правка Роба 200 (03.10): капсула, наклонённая к слою кузова (машина на склоне у изгороди, обрезана краем слоя), режется
  // клином — у края слоя срез сужается до нуля. «Стадион» одного наибольшего радиуса давал у обрезанного конца толстый скруглённый
  // торец — призрачное касание, и кузов толкало вдоль изгороди (до 0,6 м за кадр). Срез — оболочка кругов по оси, у каждого
  // свой радиус среза; оси SAT — общие касательные соседних кругов (обе стороны). Ровный срез (без сужения) — как было
  if (!(C->flat && hl > 0.f && fabsf(C->axis[1]) > 0.99f)) {
    float ga = fmaxf_(0.f, fmaxf_(q->lo - (s0 + ds * ta), (s0 + ds * ta) - q->hi)) / kN;
    float gb = fmaxf_(0.f, fmaxf_(q->lo - (s0 + ds * tb), (s0 + ds * tb) - q->hi)) / kN;
    float ra = sqrtf(fmaxf_(0.f, r * r - ga * ga)), rb = sqrtf(fmaxf_(0.f, r * r - gb * gb));
    if (rad - ra > CAPSULE_TAPER_EPS || rad - rb > CAPSULE_TAPER_EPS) {
      for (int k = 0; k < CAPSULE_TAPER_N; k++) {
        float t = ta + (tb - ta) * (float)k / (CAPSULE_TAPER_N - 1), sk = s0 + ds * t;
        float gk = fmaxf_(0.f, fmaxf_(q->lo - sk, sk - q->hi)) / kN;
        sh->p[k][0] = p0x + (p1x - p0x) * t; sh->p[k][1] = p0z + (p1z - p0z) * t; sh->pr[k] = sqrtf(fmaxf_(0.f, r * r - gk * gk));
      }
      sh->np = CAPSULE_TAPER_N; sh->prv = 1;
      for (int k = 0; k + 1 < CAPSULE_TAPER_N; k++) {
        float dx = sh->p[k + 1][0] - sh->p[k][0], dz = sh->p[k + 1][1] - sh->p[k][1], D = sqrtf(dx * dx + dz * dz);
        if (D < 1e-4f) continue;
        float ux = dx / D, uz = dz / D, b = (sh->pr[k] - sh->pr[k + 1]) / D, a;
        if (b > 1.f) b = 1.f; if (b < -1.f) b = -1.f; a = sqrtf(1.f - b * b);
        addAxis(sh, -a * uz + b * ux, a * ux + b * uz); addAxis(sh, a * uz + b * ux, -a * ux + b * uz);
      }
      return 1;
    }
  }
#endif
  float ax_ = p0x + (p1x - p0x) * ta, az_ = p0z + (p1z - p0z) * ta, bx_ = p0x + (p1x - p0x) * tb, bz_ = p0z + (p1z - p0z) * tb;
  sh->p[0][0] = ax_; sh->p[0][1] = az_; sh->np = 1;
  float ex = bx_ - ax_, ez = bz_ - az_;
  if (ex * ex + ez * ez > 1e-8f) { sh->p[1][0] = bx_; sh->p[1][1] = bz_; sh->np = 2; addAxis(sh, -ez, ex); }
  return 1;
}

// забор с конечным верхом (внутренний, правка 47): слой кузова — по наклонному полу на той части забора, что лежит в
// габарите запроса (запас наклона gather для всего кузова сразу пропускал бы машину, чей низ над забором, только
// над дальним углом). 0 — весь слой кузова над верхом забора (или под низом) на этом участке. Бесконечные — как было
static int fenceBand(const Q *q, const ColSeg *G) {
  if (G->y1 > 1e5f && G->y0 < -1e5f) return 1;
  float ax = G->a[0], az = G->a[1], dx = G->b[0] - ax, dz = G->b[1] - az, t0 = 0.f, t1 = 1.f;
  float lo[2] = {q->x0, q->z0}, hi[2] = {q->x1, q->z1}, o[2] = {ax, az}, d[2] = {dx, dz};
  for (int k = 0; k < 2; k++) {                              // отрезок забора, обрезанный габаритом запроса (xz)
    if (fabsf(d[k]) < 1e-9f) { if (o[k] < lo[k] || o[k] > hi[k]) return 0; continue; }
    float u0 = (lo[k] - o[k]) / d[k], u1 = (hi[k] - o[k]) / d[k]; if (u0 > u1) { float t = u0; u0 = u1; u1 = t; }
    if (u0 > t0) t0 = u0; if (u1 < t1) t1 = u1;
  }
  if (t0 > t1) return 0;
  float f0 = q->y + q->gx * (ax + dx * t0 - q->cx) + q->gz * (az + dz * t0 - q->cz);
  float f1 = q->y + q->gx * (ax + dx * t1 - q->cx) + q->gz * (az + dz * t1 - q->cz);
  float bl = fminf_(f0, f1) + q->lo, bh = fmaxf_(f0, f1) + q->hi;
  return !(G->y1 < bl || G->y0 > bh);
}

// широкая фаза + срезы: собрать фигуры всего, что может задеть кузов
static const ColSeg *xSeg; static uint32_t nxSeg;   // заборы трассы гонки (colExtraFences)
void colExtraFences(const ColSeg *segs, unsigned n) { xSeg = segs; nxSeg = segs ? n : 0; }
// №984 (наряд ГОРОД L3–L7, столкновение кузова в подшаге): широкая фаза на свободной дороге каждый вызов (4 подшага × толчок и проба) обходит ячейки сетки и зря проверяет коробки ничего не нашедших.
// Кэш «пусто»: если в области запроса (x/z/y) НИ ОДИН примитив не пересекает её по габариту (тот же проход, что в gather до нарезки фигур — побочных эффектов у него нет:
// горбы, память заборов, листва считаются только у прошедших габарит), то для любой вложенной области результат тот же — 0 фигур, побитно. Область расширяется при втором «пусто» подряд
// на ZC_PADXZ / ZC_PADY (ещё один проход без нарезки), чтобы покрыть остальные подшаги кадра; нашёлся кандидат — расширять не пробуем ZC_SKIP вызовов. Мир после colInit не меняется,
// кроме colExtraFences (в ключе). COL_GATHERCACHE984_OLD=1 — как было; COL_ZC_VERIFY=1 — при попадании всё равно собрать и сверить (отладка).
#ifdef COL_ZC_VERIFY
#include <stdio.h>
#include <stdlib.h>
#endif
#ifndef COL_GATHERCACHE984_OLD
#define COL_GATHERCACHE984_OLD 0
#endif
int colGatherCacheOld = COL_GATHERCACHE984_OLD;
#ifndef ZC_PADXZ
#define ZC_PADXZ 1.2f
#endif
#ifndef ZC_PADY
#define ZC_PADY .5f
#endif
#define ZC_SKIP 8
#ifdef COL_ZC_COUNT
unsigned long colCnt[4];   // 0 ячеек, 1 ссылок пройдено, 2 вызовов gather, 3 ячеек и ссылок в расширяющем проходе
#define ZCNT(i, n) (colCnt[i] += (n))
#else
#define ZCNT(i, n) ((void)0)
#endif
static struct { const ColScratch *s; const ColSeg *xs; uint32_t nx; float x0, x1, z0, z1, ylo, yhi; int ok, streak, skip; unsigned long hit, miss, scan; } zc;
unsigned long colZcHits(void) { return zc.hit; }
unsigned long colZcCalls(void) { return zc.hit + zc.miss; }
static void zcReset(void) { memset(&zc, 0, sizeof zc); }
// есть ли хоть один примитив, чей габарит пересекает область (тот же отсев, что в gather до нарезки); 0 — пусто
static int zcAnyCand(const ColWorld *w, float x0, float x1, float z0, float z1, float ylo, float yhi) {
  ColScratch *S = w->scr;
#ifdef COL_ZC_BREAK
  return 0;   // красный контроль гейта: «пусто» без проверки
#endif
  int gx0 = (int)floorf((x0 - w->ox) * w->inv), gx1 = (int)floorf((x1 - w->ox) * w->inv);
  int gz0 = (int)floorf((z0 - w->oz) * w->inv), gz1 = (int)floorf((z1 - w->oz) * w->inv);
  if (gx0 < 0) gx0 = 0;
  if (gz0 < 0) gz0 = 0;
  if (gx1 >= w->W) gx1 = w->W - 1;
  if (gz1 >= w->H) gz1 = w->H - 1;
  for (uint32_t k = 0; k < nxSeg; k++) {   // заборы трассы гонки: габарит по плану (без полосы — запас)
    const ColSeg *G = &xSeg[k];
    float mnx = fminf_(G->a[0], G->b[0]), mxx = fmaxf_(G->a[0], G->b[0]), mnz = fminf_(G->a[1], G->b[1]), mxz = fmaxf_(G->a[1], G->b[1]);
    if (!(mxx < x0 || mnx > x1 || mxz < z0 || mnz > z1)) return 1;
  }
  if (gx0 > gx1 || gz0 > gz1) return 0;
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx; ZCNT(3, 1);
      uint32_t kstart = w->cellStart[c];
      if (S->cellWall) kstart += S->cellWall[c];
      for (uint32_t k = kstart; k < w->cellStart[c + 1]; k++) { ZCNT(3, 1);
        uint32_t t = w->refs[k] >> 29, i = w->refs[k] & 0x1fffffffu;
        if (t == COL_TRI) {
          if (!(S->steep[i >> 3] & (1u << (i & 7)))) continue;
          const ColTri *T = &w->tri[i];
          float mnx = fminf_(T->a[0], fminf_(T->b[0], T->c[0])), mxx = fmaxf_(T->a[0], fmaxf_(T->b[0], T->c[0]));
          float mnz = fminf_(T->a[2], fminf_(T->b[2], T->c[2])), mxz = fmaxf_(T->a[2], fmaxf_(T->b[2], T->c[2]));
          float mny = fminf_(T->a[1], fminf_(T->b[1], T->c[1])), mxy = fmaxf_(T->a[1], fmaxf_(T->b[1], T->c[1]));
          if (mxx < x0 || mnx > x1 || mxz < z0 || mnz > z1 || mxy < ylo || mny > yhi) continue;
          return 1;
        } else if (t == COL_BOX) {
          const ColBox *B = &w->box[i];
          float hx = fabsf(B->ax[0]) * B->h[0] + fabsf(B->ay[0]) * B->h[1] + fabsf(B->az[0]) * B->h[2];
          float hy = fabsf(B->ax[1]) * B->h[0] + fabsf(B->ay[1]) * B->h[1] + fabsf(B->az[1]) * B->h[2];
          float hz = fabsf(B->ax[2]) * B->h[0] + fabsf(B->ay[2]) * B->h[1] + fabsf(B->az[2]) * B->h[2];
          if (B->c[0] + hx < x0 || B->c[0] - hx > x1 || B->c[2] + hz < z0 || B->c[2] - hz > z1 || B->c[1] + hy < ylo || B->c[1] - hy > yhi) continue;
          return 1;
        } else if (t == COL_CYL) {
          const ColCyl *C = &w->cyl[i];
          float hx = fabsf(C->axis[0]) * C->halfLen + C->r, hy = fabsf(C->axis[1]) * C->halfLen + C->r, hz = fabsf(C->axis[2]) * C->halfLen + C->r;
          if (C->c[0] + hx < x0 || C->c[0] - hx > x1 || C->c[2] + hz < z0 || C->c[2] - hz > z1 || C->c[1] + hy < ylo || C->c[1] - hy > yhi) continue;
          return 1;
        } else {
          const ColSeg *G = &w->seg[i];
          float mnx = fminf_(G->a[0], G->b[0]), mxx = fmaxf_(G->a[0], G->b[0]), mnz = fminf_(G->a[1], G->b[1]), mxz = fmaxf_(G->a[1], G->b[1]);
          if (mxx < x0 || mnx > x1 || mxz < z0 || mnz > z1) continue;
          return 1;
        }
      }
    }
  return 0;
}
static int gatherFull(const ColWorld *w, const CarBox *b, Q *q);
static int gather(const ColWorld *w, const CarBox *b, Q *q) {
  ColScratch *S = w->scr; STAT(0);
#if !COLPROBE_FAST_OLD
  if (!colGatherCacheOld) {
    float fx = b->fx, fz = b->fz, rx = -fz, rz = fx;
    float ex = fabsf(fx) * b->hl + fabsf(rx) * b->hw + GATHER_MARGIN, ez = fabsf(fz) * b->hl + fabsf(rz) * b->hw + GATHER_MARGIN;
    float sl = fabsf(b->gx) * ex + fabsf(b->gz) * ez;
    float x0 = b->x - ex, x1 = b->x + ex, z0 = b->z - ez, z1 = b->z + ez, ylo = b->y + b->lo - sl, yhi = b->y + b->hi + sl;
    if (zc.ok && zc.s == S && zc.xs == xSeg && zc.nx == nxSeg && x0 >= zc.x0 && x1 <= zc.x1 && z0 >= zc.z0 && z1 <= zc.z1 && ylo >= zc.ylo && yhi <= zc.yhi) {
      zc.hit++;
#ifdef COL_ZC_VERIFY
      { int nn = gatherFull(w, b, q); if (nn != 0) { fprintf(stderr, "COL_ZC_VERIFY: кэш «пусто», а gather нашёл %d фигур\n", nn); abort(); } }
#endif
      q->cx = b->x; q->cz = b->z; q->y = b->y; q->gx = b->gx; q->gz = b->gz; q->lo = b->lo; q->hi = b->hi; q->fx = b->fx; q->fz = b->fz; q->hl = b->hl; q->hw = b->hw;
      q->fsupOn = b->fsupOn && !colFenceSupOff; q->fsup = b->fsup; q->x0 = x0; q->x1 = x1; q->z0 = z0; q->z1 = z1; q->ylo = ylo; q->yhi = yhi;
      S->nshape = 0; return 0;
    }
    zc.miss++;
    int n = gatherFull(w, b, q);
    if (n == 0 && zc.skip <= 0) {   // фигур нет — проверить, пусто ли и по габаритам (нарезка могла отбросить кандидата), и расширить область
      {
        float ax0 = x0 - ZC_PADXZ, ax1 = x1 + ZC_PADXZ, az0 = z0 - ZC_PADXZ, az1 = z1 + ZC_PADXZ, aylo = ylo - ZC_PADY, ayhi = yhi + ZC_PADY; zc.scan++;
        if (!zcAnyCand(w, ax0, ax1, az0, az1, aylo, ayhi)) { zc.s = S; zc.xs = xSeg; zc.nx = nxSeg; zc.x0 = ax0; zc.x1 = ax1; zc.z0 = az0; zc.z1 = az1; zc.ylo = aylo; zc.yhi = ayhi; zc.ok = 1; zc.streak++; }
        else { zc.ok = 0; zc.streak = 0; zc.skip = ZC_SKIP; }
      }
    } else { if (n) zc.streak = 0; if (zc.skip > 0) zc.skip--; zc.ok = 0; }
    return n;
  }
#endif
  return gatherFull(w, b, q);
}
static int gatherFull(const ColWorld *w, const CarBox *b, Q *q) {
  ColScratch *S = w->scr; ZCNT(2, 1);
  float fx = b->fx, fz = b->fz, rx = -fz, rz = fx;
  float ex = fabsf(fx) * b->hl + fabsf(rx) * b->hw + GATHER_MARGIN, ez = fabsf(fz) * b->hl + fabsf(rz) * b->hw + GATHER_MARGIN;
  q->cx = b->x; q->cz = b->z; q->y = b->y; q->gx = b->gx; q->gz = b->gz; q->lo = b->lo; q->hi = b->hi; q->fx = b->fx; q->fz = b->fz; q->hl = b->hl; q->hw = b->hw;
  q->fsupOn = b->fsupOn && !colFenceSupOff; q->fsup = b->fsup;
  q->x0 = b->x - ex; q->x1 = b->x + ex; q->z0 = b->z - ez; q->z1 = b->z + ez;
  float sl = fabsf(b->gx) * ex + fabsf(b->gz) * ez;
  q->ylo = b->y + b->lo - sl; q->yhi = b->y + b->hi + sl;
  int gx0 = (int)floorf((q->x0 - w->ox) * w->inv), gx1 = (int)floorf((q->x1 - w->ox) * w->inv);
  int gz0 = (int)floorf((q->z0 - w->oz) * w->inv), gz1 = (int)floorf((q->z1 - w->oz) * w->inv);
  if (gx0 < 0) gx0 = 0;
  if (gz0 < 0) gz0 = 0;
  if (gx1 >= w->W) gx1 = w->W - 1;
  if (gz1 >= w->H) gz1 = w->H - 1;
  S->nshape = 0;
  if (gx0 > gx1 || gz0 > gz1) return 0;
  uint32_t o1 = w->ntri, o2 = o1 + w->nbox, o3 = o2 + w->ncyl, nprim = o3 + w->nseg;
  uint16_t st = nextStamp(S, nprim);
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx; ZCNT(0, 1);
#if !COLPROBE_FAST_OLD
      uint32_t kstart = w->cellStart[c];
      if (S->cellWall) kstart += S->cellWall[c];
      for (uint32_t k = kstart; k < w->cellStart[c + 1]; k++) { ZCNT(1, 1);
#else
      for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; k++) {
#endif
        uint32_t t = w->refs[k] >> 29, i = w->refs[k] & 0x1fffffffu;
#if COLPROBE_FAST_OLD
        uint32_t gid = t == 0 ? i : t == 1 ? o1 + i : t == 2 ? o2 + i : o3 + i;
#endif
        if (t == COL_TRI) {                         // проходимую землю не берём вовсе (по биту, не читая треугольник)
          if (!(S->steep[i >> 3] & (1u << (i & 7)))) continue;
#if !COLPROBE_FAST_OLD
          uint32_t gid = i;
#endif
          const ColTri *T = &w->tri[i];
          if (S->stamp[gid] == st) continue;
          S->stamp[gid] = st;
          float mnx = fminf_(T->a[0], fminf_(T->b[0], T->c[0])), mxx = fmaxf_(T->a[0], fmaxf_(T->b[0], T->c[0]));
          float mnz = fminf_(T->a[2], fminf_(T->b[2], T->c[2])), mxz = fmaxf_(T->a[2], fmaxf_(T->b[2], T->c[2]));
          float mny = fminf_(T->a[1], fminf_(T->b[1], T->c[1])), mxy = fmaxf_(T->a[1], fmaxf_(T->b[1], T->c[1]));
          if (mxx < q->x0 || mnx > q->x1 || mxz < q->z0 || mnz > q->z1 || mxy < q->ylo || mny > q->yhi) continue;
          if (S->nshape >= MAXSHAPE) continue;
          Shape *sh = &S->shape[S->nshape];
          if (sliceTri(q, T, sh)) { sh->ptype = COL_TRI; sh->idx = i; S->nshape++; }
        } else {
#if !COLPROBE_FAST_OLD
          uint32_t gid = t == 1 ? o1 + i : t == 2 ? o2 + i : o3 + i;
#endif
          if (S->stamp[gid] == st) continue;
          S->stamp[gid] = st;
          if (S->nshape >= MAXSHAPE) continue;
          Shape *sh = &S->shape[S->nshape];
          if (t == COL_BOX) {
            const ColBox *B = &w->box[i];
            float hx = fabsf(B->ax[0]) * B->h[0] + fabsf(B->ay[0]) * B->h[1] + fabsf(B->az[0]) * B->h[2];
            float hy = fabsf(B->ax[1]) * B->h[0] + fabsf(B->ay[1]) * B->h[1] + fabsf(B->az[1]) * B->h[2];
            float hz = fabsf(B->ax[2]) * B->h[0] + fabsf(B->ay[2]) * B->h[1] + fabsf(B->az[2]) * B->h[2];
            if (B->c[0] + hx < q->x0 || B->c[0] - hx > q->x1 || B->c[2] + hz < q->z0 || B->c[2] - hz > q->z1 ||
                B->c[1] + hy < q->ylo || B->c[1] - hy > q->yhi) continue;
            if (colPassFol && boxPhantom(B)) continue;   // правка 475: невидимая коробка — насквозь, без вязкости листвы
            if (colPassFol && boxSlopePlate(B)) continue;   // №525: тонкий скат — не стена для кузова (опора остаётся)
            if (colPassFol && B->pad == 1.f) { colFolHits++; continue; }   // правка 173: куст — насквозь
            if (sliceBox(q, B, sh)) { sh->ptype = COL_BOX; sh->idx = i; sh->rail = (uint8_t)railIs(w, i); S->nshape++; }
          } else if (t == COL_CYL) {
            const ColCyl *C = &w->cyl[i];
            float hx = fabsf(C->axis[0]) * C->halfLen + C->r, hy = fabsf(C->axis[1]) * C->halfLen + C->r;
            float hz = fabsf(C->axis[2]) * C->halfLen + C->r;
            if (C->c[0] + hx < q->x0 || C->c[0] - hx > q->x1 || C->c[2] + hz < q->z0 || C->c[2] - hz > q->z1 ||
                C->c[1] + hy < q->ylo || C->c[1] - hy > q->yhi) continue;
            if (colPassFol && cylPhantom(C)) continue;   // правка 414: невидимый столб — насквозь, без вязкости листвы
            if (colPassFol && cylBushPass(C)) { colFolHits++; continue; }   // правка 173 (кусты; изгороди — нет, правка 421)
            int hump = humpHere(S, C, i, q);
#if !defined(HUMP_NO_KEEP) || !HUMP_NO_KEEP
            if (S->humpAt && S->humpAt[i] != ~0u) hump = humpRide(S, C, i, q, hump);   // правка 200: горб под кузовом не перескакивает в стену
#endif
            if (!hump && sliceCyl(q, C, sh)) { sh->ptype = COL_CYL; sh->idx = i; S->nshape++; }
          } else {
            const ColSeg *G = &w->seg[i];
#if !COLPROBE_FAST_OLD
            float mnx = fminf_(G->a[0], G->b[0]), mxx = fmaxf_(G->a[0], G->b[0]);
            float mnz = fminf_(G->a[1], G->b[1]), mxz = fmaxf_(G->a[1], G->b[1]);
            if (mxx < q->x0 || mnx > q->x1 || mxz < q->z0 || mnz > q->z1) continue;
#endif
            int bySup = 0;
            if (G->y1 < q->ylo || G->y0 > q->yhi || !fenceBand(q, G)) {   // правка 339: по своему верху не задевает — а по опоре машины?
              if (!q->fsupOn || G->y1 > 1e5f || G->y0 < COL_SEG_FOL_Y0 || (!SEAM_FENCE_OLD && G->y1 < q->fsup)) continue;   // в полёте; бесконечный; изгородь (правка 173)
              ColSeg Gs = *G; Gs.y1 = fmaxf_(G->y1, q->fsup + COL_FENCE_HOLD);
              if (Gs.y1 < q->ylo || Gs.y0 > q->yhi || !fenceBand(q, &Gs)) continue;
              bySup = 1;
            }
#if COLPROBE_FAST_OLD
            float mnx = fminf_(G->a[0], G->b[0]), mxx = fmaxf_(G->a[0], G->b[0]);
            float mnz = fminf_(G->a[1], G->b[1]), mxz = fmaxf_(G->a[1], G->b[1]);
            if (mxx < q->x0 || mnx > q->x1 || mxz < q->z0 || mnz > q->z1) continue;
#endif
            float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], L = sqrtf(dx * dx + dz * dz);
            if (L < 1e-4f) continue;
            if (colPassFol && segFolKeep(w, i, q->cx, q->y + q->lo, q->cz)) { colFolHits++; continue; }   // правка 173: изгородь (304: с памятью)
            if (colPassBack) {   // сверка PS2 09.10: с изнанки забор не держит (выступ кузова поперёк линии — для памяти)
              float nx = G->n[0], nz = G->n[1];
              float re = q->hl * fabsf(q->fx * nx + q->fz * nz) + q->hw * fabsf(-q->fz * nx + q->fx * nz);
              if (segBack(w, i, q->cx, q->cz, re)) continue;
            }
            uint8_t f = S->segFlags[i];
            sh->kind = SH_FENCE; sh->ptype = COL_SEG; sh->idx = i; sh->np = sh->na = 0; sh->rad = 0; sh->prv = 0;
            sh->fa[0] = G->a[0]; sh->fa[1] = G->a[1]; sh->fd[0] = dx / L; sh->fd[1] = dz / L;
            sh->fn[0] = G->n[0]; sh->fn[1] = G->n[1]; sh->L = L;
            sh->t0 = (f & SF_EXT_A) ? -FENCE_EXT : 0.f; sh->t1 = L + ((f & SF_EXT_B) ? FENCE_EXT : 0.f);
            sh->fflags = f | (bySup ? SF_SUP : 0); sh->nbv = 0;
            for (int e = 0; e < 2 && S->segNb; e++) {   // правка 200: соседи у выпуклых концов (угол, а не свободный конец)
              uint32_t j = S->segNb[2 * i + e];
              if (j == ~0u || !(f & (e ? SF_CVX_B : SF_CVX_A))) continue;
              const ColSeg *N = &w->seg[j];
              sh->nbp[e][0] = N->a[0]; sh->nbp[e][1] = N->a[1]; sh->nbn[e][0] = N->n[0]; sh->nbn[e][1] = N->n[1]; sh->nbv |= (unsigned char)(1u << e);
            }
            S->nshape++;
          }
        }
      }
    }
  // правка 122 (сверка 30.09): заборы трассы уличной гонки (StreetRacePropsLoad: FenceWrapper файла l1_srNp.p3d) — поверх мира,
  // пока идёт гонка (colExtraFences). Как заборы уровня, без продолжений концов; номер в контакте — COL_XFENCE + k
  for (uint32_t k = 0; k < nxSeg && S->nshape < MAXSHAPE; k++) {
    const ColSeg *G = &xSeg[k];
    if (G->y1 < q->ylo || G->y0 > q->yhi || !fenceBand(q, G)) continue;
    float mnx = fminf_(G->a[0], G->b[0]), mxx = fmaxf_(G->a[0], G->b[0]), mnz = fminf_(G->a[1], G->b[1]), mxz = fmaxf_(G->a[1], G->b[1]);
    if (mxx < q->x0 || mnx > q->x1 || mxz < q->z0 || mnz > q->z1) continue;
    float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], L = sqrtf(dx * dx + dz * dz); if (L < 1e-4f) continue;
    Shape *sh = &S->shape[S->nshape];
    sh->kind = SH_FENCE; sh->ptype = COL_SEG; sh->idx = COL_XFENCE + k; sh->np = sh->na = 0; sh->rad = 0; sh->prv = 0;
    sh->fa[0] = G->a[0]; sh->fa[1] = G->a[1]; sh->fd[0] = dx / L; sh->fd[1] = dz / L;
    sh->fn[0] = G->n[0]; sh->fn[1] = G->n[1]; sh->L = L; sh->t0 = 0.f; sh->t1 = L; sh->fflags = 0; sh->nbv = 0;
    S->nshape++;
  }
  return S->nshape;
}

// ---------------------------------------------------------------------------------------------- кузов: SAT
typedef struct { float cx, cz, fx, fz, rx, rz, hw, hl; } Rect;

// глубина проникновения фигуры в прямоугольник и направление, куда толкать машину (0 = не пересекаются)
// №1050 (Роб: «въехал в самый стык трамплина L5, полностью остановился, дальше поехал без скорости»; разбор: канал входа на большой трамплин L5 шириной 4,5 м между двумя стенками-коробками 2 м × 0,4 м,
// машина со смещением от оси ≥ 1,8 м упирается передним углом в ТОРЕЦ стенки и встаёт с 25 м/с в ноль, а по оси взлетает на 30 м/с): удар передним углом в торец ТОНКОЙ стенки (толщина ≤ 0,5 м, длина ≥ 2 м,
// заход в пределах 0,4 м от конца) при курсе почти вдоль стенки (≤ 35° к оси) раньше выталкивал машину назад по торцу и гасил скорость. Теперь выталкивание — вбок, к ближней стороне стенки (в проём),
// если нужный сдвиг не больше 1,3 м; лобовой удар в плоскость стенки, в торец под большим углом и по оси стенки (сдвиг больше 1,3 м) — как раньше. RAMP1050_OLD=1 / файл ramp1050old / flag ramp1050 0 — как было
#ifndef RAMP1050_OLD
#define RAMP1050_OLD 0
#endif
int colRamp1050 = 0;   // общее правило (А) выключено по умолчанию (Роб выбрал точечный вариант Б, flag ramp1050 1 включает)
#define RAMP1050_THICK 0.5f
#define RAMP1050_LEN 2.0f
#define RAMP1050_END 1.0f   // проникновение в торец к моменту первого контакта: до скорость × шаг (25 м/с × 1/30 с ≈ 0,8 м), не 0,4 м
#define RAMP1050_COS 0.819f
#define RAMP1050_LAT 1.3f
static int wallEndSlide(const Shape *s, const Rect *R, float *dx, float *dz, float *depth) {
  if (s->ptype != COL_BOX || s->prv || s->rad != 0.f || s->np < 3 || s->np > 8) return 0;
  // точки среза не обязаны идти по кругу (8 точек у ящика стенки): ось толщины — из осей фигуры s->a (нормали рёбер) с наименьшей протяжённостью точек
  float lx = 0.f, lz = 0.f, T = 3.0e38f, L = 0.f, ex = 0.f, ez = 0.f; int have = 0;
  for (int a = 0; a < s->na; a++) {
    float ux = s->a[a][0], uz = s->a[a][1], ul = sqrtf(ux * ux + uz * uz); if (ul < 1e-5f) continue; ux /= ul; uz /= ul;
    float mn = 3.0e38f, mx = -3.0e38f; for (int k = 0; k < s->np; k++) { float t = s->p[k][0] * ux + s->p[k][1] * uz; if (t < mn) mn = t; if (t > mx) mx = t; }
    if (mx - mn < T) { T = mx - mn; lx = ux; lz = uz; have = 1; }
  }
  if (!have || T > RAMP1050_THICK) return 0;
  ex = lz; ez = -lx;
  { float mn = 3.0e38f, mx = -3.0e38f; for (int k = 0; k < s->np; k++) { float t = s->p[k][0] * ex + s->p[k][1] * ez; if (t < mn) mn = t; if (t > mx) mx = t; } L = mx - mn; }
  if (L < RAMP1050_LEN) return 0;
  if (fabsf(R->fx * ex + R->fz * ez) < RAMP1050_COS) return 0;
  float oA[2], oL[2];
  for (int a = 0; a < 2; a++) {
    float ux = a ? lx : ex, uz = a ? lz : ez;
    float ru = R->hl * fabsf(R->fx * ux + R->fz * uz) + R->hw * fabsf(R->rx * ux + R->rz * uz), mn = 3.0e38f, mx = -3.0e38f;
    for (int k = 0; k < s->np; k++) { float d = (s->p[k][0] - R->cx) * ux + (s->p[k][1] - R->cz) * uz; if (d < mn) mn = d; if (d > mx) mx = d; }
    float o1 = mx + ru, o2 = ru - mn; if (o1 <= 0.f || o2 <= 0.f) return 0;
    if (a == 0) { oA[0] = o1; oA[1] = o2; } else { oL[0] = o1; oL[1] = o2; }
  }
  float oEnd = fminf(oA[0], oA[1]), oLat = fminf(oL[0], oL[1]);
  if (oEnd > RAMP1050_END || oLat > RAMP1050_LAT) return 0;
  float sg = oL[0] < oL[1] ? 1.f : -1.f;
  *dx = sg * lx; *dz = sg * lz; *depth = oLat;
  return 1;
}
// №1050 (Б, Роб: «торцы бортиков не держат», большой трамплин L5 к стеклянной трубе): четыре борта у въезда на скат — коробки COL1 867/868 (на ровной части перед скатом) и 869/870 (вдоль ската) — торцами
// останавливали машину со смещением от оси: упёршись передним углом в торец, она теряла всю скорость. Теперь у этих бортов нет торцов: кузов, зашедший на торец, выталкивается только ВБОК (к ближней стороне),
// боковые грани остаются стенками (с трамплина не выпасть). Борта опознаются по форме и месту (сигнатуры центров коробок), не по номеру: у уровня с тем же рельсом (L2 в тех же координатах) то же.
// RAMPRAIL1050_OLD=1 / файл ramprail1050old / flag ramprail1050 0 — как было
#ifndef RAMPRAIL1050_OLD
#define RAMPRAIL1050_OLD 0
#endif
int colRampRail1050 = !RAMPRAIL1050_OLD;
static void railInit(const ColWorld *w) {
  static const float sig[4][3] = {{-90.7f, 29.4f, -430.6f}, {-90.7f, 29.5f, -435.5f}, {-104.0f, 33.2f, -430.6f}, {-104.0f, 33.2f, -435.5f}};
  if (!w->scr) return;
  w->scr->nrail1050 = 0;
  for (uint32_t i = 0; i < w->nbox && w->scr->nrail1050 < 8; i++) {
    const ColBox *b = &w->box[i];
    for (int k = 0; k < 4; k++)
      if (fabsf(b->c[0] - sig[k][0]) < .35f && fabsf(b->c[1] - sig[k][1]) < .35f && fabsf(b->c[2] - sig[k][2]) < .35f && fminf(b->h[0], fminf(b->h[1], b->h[2])) < .25f && fmaxf(b->h[0], fmaxf(b->h[1], b->h[2])) > 3.f) { w->scr->rail1050[w->scr->nrail1050++] = i; break; }
  }
}
static int railIs(const ColWorld *w, uint32_t i) { if (!w->scr) return 0; for (unsigned k = 0; k < w->scr->nrail1050; k++) if (w->scr->rail1050[k] == i) return 1; return 0; }
static int railLateral(const Shape *s, const Rect *R, float *dx, float *dz, float *depth) {
  float lx = 0.f, lz = 0.f, T = 3.0e38f; int have = 0;
  for (int a = 0; a < s->na; a++) {
    float ux = s->a[a][0], uz = s->a[a][1], ul = sqrtf(ux * ux + uz * uz); if (ul < 1e-5f) continue; ux /= ul; uz /= ul;
    float mn = 3.0e38f, mx = -3.0e38f; for (int k = 0; k < s->np; k++) { float t = s->p[k][0] * ux + s->p[k][1] * uz; if (t < mn) mn = t; if (t > mx) mx = t; }
    if (mx - mn < T) { T = mx - mn; lx = ux; lz = uz; have = 1; }
  }
  if (!have) return 0;
  float ex = lz, ez = -lx, o[2][2];
  for (int a = 0; a < 2; a++) {
    float ux = a ? lx : ex, uz = a ? lz : ez;
    float ru = R->hl * fabsf(R->fx * ux + R->fz * uz) + R->hw * fabsf(R->rx * ux + R->rz * uz), mn = 3.0e38f, mx = -3.0e38f;
    for (int k = 0; k < s->np; k++) { float d = (s->p[k][0] - R->cx) * ux + (s->p[k][1] - R->cz) * uz; if (d < mn) mn = d; if (d > mx) mx = d; }
    o[a][0] = mx + ru; o[a][1] = ru - mn; if (o[a][0] <= 0.f || o[a][1] <= 0.f) return 0;
  }
  float oL0 = o[1][0], oL1 = o[1][1];
  *dx = (oL0 < oL1 ? 1.f : -1.f) * lx; *dz = (oL0 < oL1 ? 1.f : -1.f) * lz; *depth = fminf(oL0, oL1);
  return 1;
}
static float evalShape(const Shape *s, const Rect *R, float *dx, float *dz, int *behind) {
  *behind = 0;
  if (colRampRail1050 && s->kind != SH_FENCE && s->ptype == COL_BOX && s->rail) { float sd; if (railLateral(s, R, dx, dz, &sd)) return sd; return 0.f; }
  if (colRamp1050 && s->kind != SH_FENCE) { float sd; if (wallEndSlide(s, R, dx, dz, &sd)) return sd; }
  if (s->kind == SH_FENCE) {
    // координаты забора: t вдоль (fd), u по нормали (fn). Забор — прямоугольник t в [t0, t1], u в [-FENCE_D, 0]
    // (центр кузова с изнанки в пределах отрезка — зеркально: u в [0, FENCE_D], толкаем дальше на изнанку)
    float ox = R->cx - s->fa[0], oz = R->cz - s->fa[1];
    float ct = ox * s->fd[0] + oz * s->fd[1], cu = ox * s->fn[0] + oz * s->fn[1];
    float ft = R->fx * s->fd[0] + R->fz * s->fd[1], fu = R->fx * s->fn[0] + R->fz * s->fn[1];
    float rt = R->rx * s->fd[0] + R->rz * s->fd[1], ru = R->rx * s->fn[0] + R->rz * s->fn[1];
    if ((s->fflags & SF_SUP) && cu < 0.f) return 0.f;   // правка 339: опорный верх с изнанки не держит (выехать назад можно, как было)
    // изнанка — центр за линией в пределах звена с продлением на прямых стыках (t0..t1): у соседних звеньев одной прямой
    // (стык без излома) сторона одна и та же — иначе продление соседа толкало бы кузов сквозь линию на лицо
    int back = cu < 0.f && ct >= s->t0 && ct <= s->t1, side = back;
#if !defined(FENCE_BEYOND_FRONT) || !FENCE_BEYOND_FRONT
    // правка Роба 200 (остаток сверки tests/humps/sweep.c, 03.10): центр кузова за линией, но за концом звена — машина обогнула
    // выпуклый угол (у острого угла толстая полоса звена выходит из-под твёрдого клина на лицо соседа) или стоит на опоре выше
    // соседнего низкого звена (внутренний забор правки 47). Раньше такой кузов считался «на лице» и полоса FENCE_D за линией
    // выталкивала его сквозь твёрдое — вдоль забора за угол или поперёк, до 6 м за кадр. Кузов на изнанке толкается только обратно
    // на изнанку (если залез за линию в пределах звена); флага «за забором» нет — отмены подшага нет, как и раньше вне звена
    if (!back && (ct < s->t0 || ct > s->t1)) {
      int e = ct < s->t0 ? 0 : 1;
      // у выпуклого угла кузов на лице соседа — огибает угол: твёрдое у угла держит сосед, это звено не толкает (у острого угла его
      // полоса за линией лежит на лице соседа — фантом). Центр перед этим звеном — весь кузов перед соседом; за ним — центр
      if (s->nbv >> e & 1) {
        float nx = s->nbn[e][0], nz = s->nbn[e][1], cn = (R->cx - s->nbp[e][0]) * nx + (R->cz - s->nbp[e][1]) * nz;
        float en = R->hl * fabsf(R->fx * nx + R->fz * nz) + R->hw * fabsf(R->rx * nx + R->rz * nz);
        if (cn >= (cu < 0.f ? 0.f : en)) return 0.f;
      }
      if (cu < 0.f) side = 1;
    }
#endif
    float sn = side ? -1.f : 1.f, u0 = side ? 0.f : -FENCE_D, u1 = side ? FENCE_D : 0.f;
    float qt = (s->t0 + s->t1) * .5f, qu = (u0 + u1) * .5f, ht = (s->t1 - s->t0) * .5f, hu = FENCE_D * .5f;
    float ax[4][2] = {{0.f, 1.f}, {1.f, 0.f}, {ft, fu}, {rt, ru}};
    float best = 3.0e38f, pt_ = 0, pu_ = 0;
    for (int k = 0; k < 4; k++) {
      float at = ax[k][0], au = ax[k][1];
      float qc = qt * at + qu * au, qe = ht * fabsf(at) + hu * fabsf(au);
      float rc = ct * at + cu * au, re = R->hl * fabsf(ft * at + fu * au) + R->hw * fabsf(rt * at + ru * au);
      float o1 = (qc + qe) - (rc - re), o2 = (rc + re) - (qc - qe);   // o1: толкать +ось, o2: -ось
      if (o1 <= 0.f || o2 <= 0.f) return 0.f;
      for (int sg = 0; sg < 2; sg++) {
        float o = sg ? o2 : o1, dt_ = sg ? -at : at, du_ = sg ? -au : au;
        if (o >= best) continue;
        if (du_ * sn < -0.01f) continue;                                  // на изнанку — нельзя
        if (dt_ < -0.01f && !(s->fflags & SF_CVX_A)) continue;            // за начало — только если там выпуклый угол
        if (dt_ > 0.01f && !(s->fflags & SF_CVX_B)) continue;             // за конец — так же
        best = o; pt_ = dt_; pu_ = du_;
      }
    }
    if (best > 1e30f) { best = 0.f; }
    if ((s->fflags & SF_SUP) && best > COL_FENCE_SUP_PEN) return 0.f;   // правка 339: приземлилась поперёк линии — не выталкиваем
    *dx = pt_ * s->fd[0] + pu_ * s->fn[0]; *dz = pt_ * s->fd[1] + pu_ * s->fn[1];
#if !defined(FENCE_BEYOND_FRONT) || !FENCE_BEYOND_FRONT
    // «за забором» (car.c отменяет подшаг) — только что пересёкший линию: центр за ней не глубже FENCE_BEHIND_FRESH (за подшаг центр
    // проходит меньше 0,35 м; путь сквозь забор ловит ещё colCrossFenceBox). Кузов, стоящий за внутренним забором глубже (заехал по
    // низкому звену, прилетел с трамплина), упирается в него как в стену: отмена каждого подшага его замораживала (рой L1, L3)
    *behind = back && cu > -FENCE_BEHIND_FRESH;
#else
    *behind = back;
#endif
    return best;
  }
  // выпуклая фигура (+ радиус): SAT по осям прямоугольника, фигуры и (для скруглённых) от угла прямоугольника к точке
  float best = 3.0e38f, bestS = 3.0e38f, bx = 0, bz = 0;
  const float rad = s->rad;
#define SAT_AXIS(UX, UZ, WGT) do { \
    float ux_ = (UX), uz_ = (UZ); \
    float ru_ = R->hl * fabsf(R->fx * ux_ + R->fz * uz_) + R->hw * fabsf(R->rx * ux_ + R->rz * uz_); \
    float mn_ = 3.0e38f, mx_ = -3.0e38f; \
    for (int k_ = 0; k_ < s->np; k_++) { \
      float d_ = (s->p[k_][0] - R->cx) * ux_ + (s->p[k_][1] - R->cz) * uz_, r_ = s->prv ? s->pr[k_] : rad; \
      mn_ = d_ - r_ < mn_ ? d_ - r_ : mn_; mx_ = d_ + r_ > mx_ ? d_ + r_ : mx_; } \
    float o1_ = mx_ + ru_; if (o1_ <= 0.f) return 0.f; \
    float o2_ = ru_ - mn_; if (o2_ <= 0.f) return 0.f; \
    float ow_ = (WGT); \
    if (o1_ < o2_) { if (o1_ * ow_ < bestS) { bestS = o1_ * ow_; best = o1_; bx = ux_; bz = uz_; } } \
    else if (o2_ * ow_ < bestS) { bestS = o2_ * ow_; best = o2_; bx = -ux_; bz = -uz_; } \
  } while (0)
  const float carW = (!THIN_END_OLD && s->thin) ? THIN_END_W : 1.f;
  SAT_AXIS(R->fx, R->fz, carW);
  SAT_AXIS(R->rx, R->rz, carW);
  for (int k = 0; k < s->na; k++) SAT_AXIS(s->a[k][0], s->a[k][1], 1.f);
  if (rad > 0.f)
    for (int k = 0; k < s->np; k++) {
      if (s->prv && !(s->pr[k] > 0.f)) continue;            // точка без радиуса — вершина, её стороны уже в осях фигуры
      float px = s->p[k][0] - R->cx, pz = s->p[k][1] - R->cz;
      float lf = px * R->fx + pz * R->fz, lr = px * R->rx + pz * R->rz;
      float cf = lf < -R->hl ? -R->hl : lf > R->hl ? R->hl : lf, cr = lr < -R->hw ? -R->hw : lr > R->hw ? R->hw : lr;
      if (cf == lf && cr == lr) continue;                // точка внутри — хватит других осей
      float vx = px - (R->fx * cf + R->rx * cr), vz = pz - (R->fz * cf + R->rz * cr), l = vx * vx + vz * vz;
      if (l < 1e-12f) continue;
      l = 1.f / sqrtf(l);
      SAT_AXIS(vx * l, vz * l, 1.f);
    }
#undef SAT_AXIS
  // o1 = mx + ru: фигура ниже по оси — машину толкаем по +u; o2 = ru - mn: фигура выше — по -u. (bx, bz) — куда толкать
  *dx = bx; *dz = bz;
  return best;
}

static void rectOf(const CarBox *b, Rect *R) {
  R->cx = b->x; R->cz = b->z; R->fx = b->fx; R->fz = b->fz; R->rx = -b->fz; R->rz = b->fx; R->hw = b->hw; R->hl = b->hl;
}

static void addContact(CarBox *b, const Shape *s, float nx, float nz, float d) {
  for (int k = 0; k < b->n; k++)
    if (b->type[k] == s->ptype && b->index[k] == s->idx && b->nx[k] == nx && b->nz[k] == nz) { b->depth[k] += d; return; }
  if (b->n < COL_MAX_CONTACTS) { b->nx[b->n] = nx; b->nz[b->n] = nz; b->depth[b->n] = d; b->type[b->n] = s->ptype; b->index[b->n] = s->idx; b->n++; }
  else {   // места нет — заменить самый мелкий
    int m = 0; for (int k = 1; k < b->n; k++) if (b->depth[k] < b->depth[m]) m = k;
    if (d > b->depth[m]) { b->nx[m] = nx; b->nz[m] = nz; b->depth[m] = d; b->type[m] = s->ptype; b->index[m] = s->idx; }
  }
}

static int folIn(const ColWorld *w, uint32_t k, float x, float y, float z, float margin) {   // точка в объёме-кусте k списка (+ запас)
  const ColScratch *S = w->scr; const float *bb = S->folBox + 4 * k;
  if (x < bb[0] - margin || x > bb[2] + margin || z < bb[1] - margin || z > bb[3] + margin) return 0;
  uint32_t t = S->fol[k] >> 29, i = S->fol[k] & 0x1fffffffu;
  if (t == COL_CYL) {
    const ColCyl *C = &w->cyl[i]; float px = x - C->c[0], py = y - C->c[1], pz = z - C->c[2];
    float u = px * C->axis[0] + py * C->axis[1] + pz * C->axis[2];
    if (u > C->halfLen) u = C->halfLen; if (u < -C->halfLen) u = -C->halfLen;
    px -= C->axis[0] * u; py -= C->axis[1] * u; pz -= C->axis[2] * u;
    return px * px + py * py + pz * pz <= (C->r + margin) * (C->r + margin);
  }
  const ColBox *B = &w->box[i]; float px = x - B->c[0], py = y - B->c[1], pz = z - B->c[2];
  return fabsf(px * B->ax[0] + py * B->ax[1] + pz * B->ax[2]) <= B->h[0] + margin &&
         fabsf(px * B->ay[0] + py * B->ay[1] + pz * B->ay[2]) <= B->h[1] + margin &&
         fabsf(px * B->az[0] + py * B->az[1] + pz * B->az[2]) <= B->h[2] + margin;
}
int colFoliageNear(const ColWorld *w, float x, float y, float z, float margin) {
  if (!w || !w->scr || !w->scr->fol) return 0;
  for (uint32_t k = 0; k < w->scr->nfol; k++) if (folIn(w, k, x, y, z, margin)) return 1;
  return 0;
}

// Правка Роба 304 (04.10, L1 H10-5: «кусты почему-то тормозят машину — прям затормаживают»): доля кузова в листве. Сетка 5 × 3
// точек прямоугольника кузова на двух высотах (у колёс COL_FOL_WHEEL_Y над полом и середина слоя кузова); точка в листве — внутри объёма FOLIAGE или ближе
// COL_FOL_FENCE_HALF к забору-изгороди (по высоте ниже его верха). car.c вязнет пропорционально этой доле: проезд изгороди
// толщиной T на скорости v теряет e^(−FOLIAGE_DRAG·T/v) скорости, как задумано в правке 173, а езда вдоль куста без касания —
// ничего. Прежде вязкость включалась пересечением габаритных коробок (широкая фаза gather + 0.6 м запаса) в полную силу
float colFoliageCover(const ColWorld *w, const CarBox *b) {
  const ColScratch *S = w ? w->scr : 0;
  if (!S || (!S->nfol && !S->nfolSeg)) return 0.f;
  float rx = -b->fz, rz = b->fx, ext = b->hl + b->hw + COL_FOL_FENCE_HALF;
  uint32_t cand[16], vol[16]; int nc = 0, nv = 0;   // заборы-изгороди и объёмы-кусты рядом с кузовом (обычно 0–3)
  for (uint32_t k = 0; k < S->nfolSeg && nc < 16; k++) {
    const ColSeg *G = &w->seg[S->folSeg[k]];
    if (fminf_(G->a[0], G->b[0]) > b->x + ext || fmaxf_(G->a[0], G->b[0]) < b->x - ext ||
        fminf_(G->a[1], G->b[1]) > b->z + ext || fmaxf_(G->a[1], G->b[1]) < b->z - ext) continue;
    cand[nc++] = S->folSeg[k];
  }
  for (uint32_t k = 0; k < S->nfol && nv < 16; k++) {
    const float *bb = S->folBox + 4 * k;
    if (bb[0] > b->x + ext || bb[2] < b->x - ext || bb[1] > b->z + ext || bb[3] < b->z - ext) continue;
    vol[nv++] = k;
  }
  if (!nc && !nv) return 0.f;
  int in = 0;
  for (int i = 0; i < 5; i++) for (int j = 0; j < 3; j++) {
    float a = (i - 2) * .5f * b->hl, c = (j - 1) * b->hw;
    float x = b->x + b->fx * a + rx * c, z = b->z + b->fz * a + rz * c, y0 = b->y + b->gx * (x - b->x) + b->gz * (z - b->z);
    float ya = y0 + COL_FOL_WHEEL_Y, yb = y0 + (b->lo + b->hi) * .5f; int hit = 0;
    for (int k = 0; k < nv && !hit; k++) hit = folIn(w, vol[k], x, ya, z, 0.f) || folIn(w, vol[k], x, yb, z, 0.f);
    for (int k = 0; k < nc && !hit; k++) {
      const ColSeg *G = &w->seg[cand[k]];
      if (ya > G->y1) continue;
      float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], l2 = dx * dx + dz * dz;
      float t = l2 > 1e-8f ? ((x - G->a[0]) * dx + (z - G->a[1]) * dz) / l2 : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
      float ex = x - (G->a[0] + dx * t), ez = z - (G->a[1] + dz * t);
      hit = ex * ex + ez * ez <= COL_FOL_FENCE_HALF * COL_FOL_FENCE_HALF;
    }
    in += hit;
  }
  return in / 15.f;
}

int colContactMaterial(const ColWorld *w, const CarBox *b, int k) {
  if (k < 0 || k >= b->n) return COL_MAT_HARD;
  unsigned i = b->index[k];
  if (b->type[k] == COL_BOX && i < w->nbox && w->box[i].pad == 1.f) return COL_MAT_FOLIAGE;
  if (b->type[k] == COL_CYL && i < w->ncyl && w->cyl[i].pad[0] == 1.f) return COL_MAT_FOLIAGE;
#ifndef FOLIAGE_PREV
  // забор мира, проложенный по изгороди (в данных l1_terra заборы идут вдоль живых изгородей дворов): машина видит и
  // задевает куст — удар куста (правило порта, правка 71). Точка — ближайшая точка забора к центру кузова на высоте низа слоя
  if (b->type[k] == COL_SEG && i < w->nseg && w->seg[i].y0 < COL_SEG_FOL_Y0) return COL_MAT_FOLIAGE;   // правка 173
  if (b->type[k] == COL_SEG && i < w->nseg) {
    const ColSeg *G = &w->seg[i]; float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], l2 = dx * dx + dz * dz;
    float t = l2 > 1e-8f ? ((b->x - G->a[0]) * dx + (b->z - G->a[1]) * dz) / l2 : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    if (colFoliageNear(w, G->a[0] + dx * t, b->y + b->lo, G->a[1] + dz * t, COL_FOLIAGE_FENCE)) return COL_MAT_FOLIAGE;
  }
#endif
  return COL_MAT_HARD;
}

// Closest points, in world space; no allocations or model-specific geometry.
static float sdot(const float*a,const float*b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void ssub(float*o,const float*a,const float*b){for(int k=0;k<3;k++)o[k]=a[k]-b[k];}
static void smadd(float*o,const float*a,const float*b,float t){for(int k=0;k<3;k++)o[k]=a[k]+b[k]*t;}
#ifndef COL_CYL_AS_CAPSULE_OLD
#define COL_CYL_AS_CAPSULE_OLD 0
#endif
#ifndef COL_WHEEL_GROUND_EDGE_OLD
#define COL_WHEEL_GROUND_EDGE_OLD 0
#endif
#ifndef COL_WHEEL_GROUND_FACE_OLD
#define COL_WHEEL_GROUND_FACE_OLD 0   // 1: e9558e0, every walkable triangle skipped, faces included
#endif
#ifndef COL_WHEEL_BOX_TOP_OLD
#define COL_WHEEL_BOX_TOP_OLD 0
#endif
static void sphereTriPoint(const float*p,const ColTri*T,float*q){
 float ab[3],ac[3],ap[3],bp[3],cp[3];ssub(ab,T->b,T->a);ssub(ac,T->c,T->a);ssub(ap,p,T->a);
 float aa=sdot(ab,ab),cc=sdot(ac,ac),ad=sdot(ab,ac);
 if(aa*cc-ad*ad<=1e-7f*aa*cc){
  const float *v[3]={T->a,T->b,T->c};float best=3e38f;
  for(int edge=0;edge<3;edge++){float direction[3],delta[3],point[3];ssub(direction,v[(edge+1)%3],v[edge]);ssub(delta,p,v[edge]);float d=sdot(direction,direction),t=d>1e-12f?fminf_(1.f,fmaxf_(0.f,sdot(delta,direction)/d)):0.f;smadd(point,v[edge],direction,t);ssub(delta,p,point);d=sdot(delta,delta);if(d<best){best=d;memcpy(q,point,12);}}
  return;
 }
 float d1=sdot(ab,ap),d2=sdot(ac,ap);if(d1<=0&&d2<=0){memcpy(q,T->a,12);return;}
 ssub(bp,p,T->b);float d3=sdot(ab,bp),d4=sdot(ac,bp);if(d3>=0&&d4<=d3){memcpy(q,T->b,12);return;}
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){smadd(q,T->a,ab,d1/(d1-d3));return;}
 ssub(cp,p,T->c);float d5=sdot(ab,cp),d6=sdot(ac,cp);if(d6>=0&&d5<=d6){memcpy(q,T->c,12);return;}
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){smadd(q,T->a,ac,d2/(d2-d6));return;}
 float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){float bc[3];ssub(bc,T->c,T->b);smadd(q,T->b,bc,(d4-d3)/((d4-d3)+(d5-d6)));return;}
 float inv=1/(va+vb+vc);for(int k=0;k<3;k++)q[k]=T->a[k]+ab[k]*vb*inv+ac[k]*vc*inv;
}
#ifndef COL_WHEEL_PRE_OLD
#define COL_WHEEL_PRE_OLD 0   // 1: без предотсева (каждый треугольник и ящик в клетках — полный расчёт), как до 11.10
#endif
#ifndef COL_WHEEL_PRE_BREAK
#define COL_WHEEL_PRE_BREAK 0.f   // красный контроль гейта: сдвиг порога предотсева вверх (отсекает настоящие контакты)
#endif
int colWheelPreOld = COL_WHEEL_PRE_OLD;
unsigned long long colWheelPreSeen, colWheelPreSkip;   // счётчики гейта: сколько рассмотрено, сколько отсеяно
int colWheelWallContacts(const ColWorld*w,const float p[3],float radius,ColSphereContact*out,int max){
 if(!w->scr||radius<=0||max<=0)return 0;ColScratch*S=w->scr;
 int x0=(int)floorf((p[0]-radius-w->ox)*w->inv),x1=(int)floorf((p[0]+radius-w->ox)*w->inv);
 int z0=(int)floorf((p[2]-radius-w->oz)*w->inv),z1=(int)floorf((p[2]+radius-w->oz)*w->inv);
 if(x0<0)x0=0;if(z0<0)z0=0;if(x1>=w->W)x1=w->W-1;if(z1>=w->H)z1=w->H-1;
 unsigned o1=w->ntri,o2=o1+w->nbox,o3=o2+w->ncyl;uint16_t st=nextStamp(S,o3+w->nseg);int count=0;
 for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
  unsigned cell=z*w->W+x;for(unsigned k=w->cellStart[cell];k<w->cellStart[cell+1];k++){
   unsigned ref=w->refs[k],t=ref>>29,i=ref&0x1fffffffu;if(t>COL_CYL)continue;unsigned gid=t==COL_TRI?i:t==COL_BOX?o1+i:o2+i;
   if(S->stamp[gid]==st)continue;S->stamp[gid]=st;float q[3],n[3],depth=0,delta[3];
   if(t==COL_TRI){const ColTri*T=&w->tri[i];
    // A sphere next to a ground edge can get a lateral closest-point normal
    // although the face is walkable; that edge must not become a wall and
    // push the car sideways on a ramp (G, cyl570: ny .469 -> -.206). A
    // contact whose own normal is walkable is still support: the face of a
    // steep walkable slope and open seam edges under the tyre (J11-9,
    // ny .83-.99) stop the car where the original stops.
    if(!COL_WHEEL_GROUND_EDGE_OLD && COL_WHEEL_GROUND_FACE_OLD && T->n[1]>COL_WALK_NY)continue;
    if(!colWheelPreOld){   // предотсев без изменения результата: контакт ставится только при d < radius и n.y = (p.y - q.y)/d <= COL_WALK_NY,
      // то есть точка q на треугольнике не ниже p.y - COL_WALK_NY * radius; а d >= расстояния от p до плоскости
      float my=T->a[1]>T->b[1]?T->a[1]:T->b[1];my=my>T->c[1]?my:T->c[1];
      float pd=(p[0]-T->a[0])*T->n[0]+(p[1]-T->a[1])*T->n[1]+(p[2]-T->a[2])*T->n[2],nn=sdot(T->n,T->n);
      colWheelPreSeen++;
      if(my<p[1]-COL_WALK_NY*radius-1e-3f+COL_WHEEL_PRE_BREAK||(nn>.99f&&nn<1.01f&&fabsf(pd)>=radius+1e-3f-COL_WHEEL_PRE_BREAK)){colWheelPreSkip++;continue;}}
    sphereTriPoint(p,T,q);ssub(delta,p,q);float d=sqrtf(sdot(delta,delta));if(d>=radius||d<1e-7f||(T->n[1]>COL_WALK_NY&&sdot(delta,T->n)<0.f))continue;for(int j=0;j<3;j++)n[j]=delta[j]/d;depth=radius-d;
    if(!COL_WHEEL_GROUND_EDGE_OLD && T->n[1]>COL_WALK_NY && n[1]<0.f)continue;}
   else if(t==COL_BOX){const ColBox*B=&w->box[i];if(colPassFol&&(B->pad==1||boxPhantom(B)||boxSlopePlate(B)||boxRoofSlope(B)))continue;if(!colWheelPreOld&&B->c[1]+fabsf(B->ax[1])*B->h[0]+fabsf(B->ay[1])*B->h[1]+fabsf(B->az[1])*B->h[2]<p[1]-COL_WALK_NY*radius-1e-3f+COL_WHEEL_PRE_BREAK){colWheelPreSeen++;colWheelPreSkip++;continue;}const float*A[3]={B->ax,B->ay,B->az};float d[3],local[3],clipped[3];ssub(d,p,B->c);int inside=1;float gap=1e30f;int face=0;
    for(int j=0;j<3;j++){local[j]=sdot(d,A[j]);clipped[j]=fminf_(B->h[j],fmaxf_(-B->h[j],local[j]));inside&=fabsf(local[j])<=B->h[j];float g=B->h[j]-fabsf(local[j]);if(g<gap){gap=g;face=j;}}
    memcpy(q,B->c,12);for(int j=0;j<3;j++)for(int b=0;b<3;b++)q[b]+=A[j][b]*clipped[j];
    // Регресс 388/377 (гейт tests/pockets L7, забор 503): центр колеса внутри тонкой плиты (ящик 1148, толщина 0,78 м) у края её
    // проходимого верха — ближайшей гранью оказывалась боковая (3,5 см против 3,45 см до верха), и сфера выталкивалась вбок на
    // радиус + зазор: 0,41 м за кадр при скорости 0,5 м/с. Проходимый верх — дело подвески (как у треугольников: колесо под
    // проходимой гранью — не стена): пока центр ближе радиуса к проходимому верху ящика, боковой контакт не ставится.
    // Прежнее: -DCOL_WHEEL_BOX_TOP_OLD
    if(inside&&!COL_WHEEL_BOX_TOP_OLD){int top=0;for(int j=0;j<3;j++)if(fabsf(A[j][1])>COL_WALK_NY){float sgt=A[j][1]>0.f?1.f:-1.f;top|=B->h[j]-sgt*local[j]<radius;}if(top)continue;}
    if(inside){float sg=local[face]>=0?1.f:-1.f;for(int j=0;j<3;j++)n[j]=sg*A[face][j];depth=radius+gap;for(int j=0;j<3;j++)q[j]=p[j]+gap*n[j];}
    else{ssub(delta,p,q);float dist=sqrtf(sdot(delta,delta));if(dist>=radius||dist<1e-7f)continue;for(int j=0;j<3;j++)n[j]=delta[j]/dist;depth=radius-dist;}
   }else{const ColCyl*C=&w->cyl[i];if(C->r<=0.f||(colPassFol&&(C->pad[0]==1||cylPhantom(C))))continue;int kept=0;for(int j=0;j<hkInN;j++)kept|=hkIn[j]==i;if(kept)continue;
    float d[3];ssub(d,p,C->c);float axial=sdot(d,C->axis),along=fminf_(C->halfLen,fmaxf_(-C->halfLen,axial));
    smadd(q,C->c,C->axis,along);
    if(C->flat && C->halfLen>0.f && !COL_CYL_AS_CAPSULE_OLD) {
      float radial[3];for(int j=0;j<3;j++)radial[j]=d[j]-C->axis[j]*axial;
      float rho=sqrtf(sdot(radial,radial));float scale=rho>0.f?fminf_(1.f,C->r/rho):0.f;
      for(int j=0;j<3;j++)q[j]+=radial[j]*scale;
      if(fabsf(axial)<=C->halfLen && rho<=C->r) {
        float side=C->r-rho,end=C->halfLen-fabsf(axial);
        if(end<side){float sg=axial>=0.f?1.f:-1.f;for(int j=0;j<3;j++)n[j]=sg*C->axis[j];depth=radius+end;}
        else{
          if(rho>=1e-7f)for(int j=0;j<3;j++)n[j]=radial[j]/rho;
          else{
            // On the axis every radial direction is equally near. Choose
            // a deterministic perpendicular, rather than a farther end.
            int k=0;for(int j=1;j<3;j++)if(fabsf(C->axis[j])<fabsf(C->axis[k]))k=j;
            for(int j=0;j<3;j++)n[j]=(j==k?1.f:0.f)-C->axis[j]*C->axis[k];
            float inv=1.f/sqrtf(sdot(n,n));for(int j=0;j<3;j++)n[j]*=inv;
          }
          depth=radius+side;
        }
        for(int j=0;j<3;j++)q[j]=p[j]+(depth-radius)*n[j];
      }else{ssub(delta,p,q);float dist=sqrtf(sdot(delta,delta));if(dist>=radius||dist<1e-7f)continue;for(int j=0;j<3;j++)n[j]=delta[j]/dist;depth=radius-dist;}
    }else {
      ssub(delta,p,q);float dist=sqrtf(sdot(delta,delta));if(dist>=radius+C->r||dist<1e-7f)continue;
      for(int j=0;j<3;j++){n[j]=delta[j]/dist;q[j]+=n[j]*C->r;}depth=radius+C->r-dist;
    }
   }
   // Walkable crowns belong to the suspension. This query supplies the
   // lateral tyre contacts omitted by the vertical suspension rays.
   if(n[1]>COL_WALK_NY||depth<=PUSH_EPS)continue;
   int slot=count;if(slot>=max){slot=0;for(int j=1;j<max;j++)if(out[j].depth<out[slot].depth)slot=j;if(depth<=out[slot].depth)continue;}else count++;
   memcpy(out[slot].p,q,12);memcpy(out[slot].n,n,12);out[slot].depth=depth;out[slot].ref=ref;
  }
 }
 return count;
}
int colBoxEdgesUnderPlane(const ColWorld*w,const float o[3],const float u[3],const float f[3],const float l[3],float hl,float hw,float dmax,float maxWidth,ColSphereContact*out,int max){
 if(!w->scr||max<=0)return 0;ColScratch*S=w->scr;
 float ex=fabsf(f[0])*hl+fabsf(l[0])*hw+0.1f,ez=fabsf(f[2])*hl+fabsf(l[2])*hw+0.1f;
 int x0=(int)floorf((o[0]-ex-w->ox)*w->inv),x1=(int)floorf((o[0]+ex-w->ox)*w->inv);
 int z0=(int)floorf((o[2]-ez-w->oz)*w->inv),z1=(int)floorf((o[2]+ez-w->oz)*w->inv);
 if(x0<0)x0=0;if(z0<0)z0=0;if(x1>=w->W)x1=w->W-1;if(z1>=w->H)z1=w->H-1;
 unsigned o1=w->ntri,o2=o1+w->nbox,o3=o2+w->ncyl;uint16_t st=nextStamp(S,o3+w->nseg);int count=0;
 for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
  unsigned cell=z*w->W+x;for(unsigned k=w->cellStart[cell];k<w->cellStart[cell+1];k++){
   unsigned ref=w->refs[k];if((ref>>29)!=COL_BOX)continue;unsigned i=ref&0x1fffffffu,gid=o1+i;
   if(S->stamp[gid]==st)continue;S->stamp[gid]=st;
   const ColBox*B=&w->box[i];if(B->pad==1.f||(colPassFol&&boxPhantom(B)))continue;
   const float*A[3]={B->ax,B->ay,B->az};int top=-1;float sg=1.f,best=COL_WALK_NY;
   for(int j=0;j<3;j++){if(A[j][1]>best){best=A[j][1];top=j;sg=1.f;}if(-A[j][1]>best){best=-A[j][1];top=j;sg=-1.f;}}
   if(top<0)continue;
   int a=(top+1)%3,b=(top+2)%3;float c0[3];smadd(c0,B->c,A[top],sg*B->h[top]);
   int ridge=-1;   // конёк: вторая проходимая грань, смотрящая вверх (ящик повёрнут на ребро — скат крыши и его продолжение)
#if !COL_RIDGE_EDGE_OLD
   // только скаты-плоскости: обе грани не уже COL_RIDGE_MIN (крыша дома J10 — 2,1 и 5,5 м; мелкие повёрнутые ящики-детали вроде
   // опор цистерны J11-9 (927/928, 1,0–1,4 м) остаются точками днища, как решено в правке 388)
   for(int j=0;j<3;j++)if(j!=top&&fabsf(A[j][1])>COL_WALK_NY&&2.f*B->h[j]>=COL_RIDGE_MIN&&2.f*B->h[top]>=COL_RIDGE_MIN)ridge=j;
#endif
   if(ridge<0&&(2.f*fminf_(B->h[a],B->h[b])>=maxWidth||best<0.9f))continue;   // только узкая почти ровная грань (балка, парапет)
   for(int e=0;e<(ridge<0?4:1);e++){   // четыре ребра верхней грани; у конька — одно, общее ребро двух верхних граней
    float p0[3],p1[3];
    if(ridge>=0){int t3=3-top-ridge;smadd(p0,c0,A[ridge],(A[ridge][1]>0.f?1.f:-1.f)*B->h[ridge]);smadd(p1,p0,A[t3],B->h[t3]);smadd(p0,p0,A[t3],-B->h[t3]);}
    else if(e<2){float s=e?1.f:-1.f;smadd(p0,c0,A[b],s*B->h[b]);smadd(p1,p0,A[a],B->h[a]);smadd(p0,p0,A[a],-B->h[a]);}
    else{float s=e==3?1.f:-1.f;smadd(p0,c0,A[a],s*B->h[a]);smadd(p1,p0,A[b],B->h[b]);smadd(p0,p0,A[b],-B->h[b]);}
    float d0[3],d1[3];ssub(d0,p0,o);ssub(d1,p1,o);
    float h0=sdot(d0,u),h1=sdot(d1,u);if(h0<=0.f&&h1<=0.f)continue;
    float sf0=sdot(d0,f),sf1=sdot(d1,f),sl0=sdot(d0,l),sl1=sdot(d1,l),t0=0.f,t1=1.f;int ok=1;   // обрезка прямоугольником (Liang–Barsky)
    float P[4]={-(sf1-sf0),sf1-sf0,-(sl1-sl0),sl1-sl0},Qd[4]={sf0+hl,hl-sf0,sl0+hw,hw-sl0};
    for(int j=0;j<4&&ok;j++){if(fabsf(P[j])<1e-9f){if(Qd[j]<0.f)ok=0;}else{float t=Qd[j]/P[j];if(P[j]<0.f){if(t>t0)t0=t;}else if(t<t1)t1=t;}}
    if(!ok||t0>t1)continue;
    float hA=h0+(h1-h0)*t0,hB=h0+(h1-h0)*t1,t=hA>=hB?t0:t1,hm=hA>=hB?hA:hB;
    if(hm<=PUSH_EPS||hm>dmax)continue;
    float p[3];for(int j=0;j<3;j++)p[j]=p0[j]+(p1[j]-p0[j])*t;
    int slot=count;if(slot>=max){slot=0;for(int j=1;j<max;j++)if(out[j].depth<out[slot].depth)slot=j;if(hm<=out[slot].depth)continue;}else count++;
    memcpy(out[slot].p,p,12);memcpy(out[slot].n,u,12);out[slot].depth=hm;out[slot].ref=ref;
   }
  }
 }
 return count;
}

int colPushCar(const ColWorld *w, CarBox *b, float *mtvX, float *mtvZ, float *nX, float *nZ) {
  Q q; int ns = gather(w, b, &q);
  const Shape *sh = w->scr->shape;
  float x0 = b->x, z0 = b->z, bestD = 0, bnx = 0, bnz = 0;
  b->n = 0; b->behindFence = 0; b->maxDepth = 0; b->nshapes = ns;
  int it = 0, clean = 0; float humpPushed = 0.f;
  float minD = 3.0e38f, minX = b->x, minZ = b->z;        // положение с наименьшим худшим проникновением
  for (; it < MAX_ITER && ns; it++) {
    Rect R; rectOf(b, &R); STAT(3);
    int bi = -1, bbeh = 0; float bd = PUSH_EPS, bdx = 0, bdz = 0;
    for (int i = 0; i < ns; i++) {
      float dx, dz; int beh;
      float d = evalShape(&sh[i], &R, &dx, &dz, &beh);
      if (d > bd) { bd = d; bi = i; bdx = dx; bdz = dz; bbeh = beh; }
    }
    if (bd < minD) { minD = bd; minX = b->x; minZ = b->z; }
#ifdef COL_DEBUG
    if (colDebug) {
      printf("iter %d: centre %.4f %.4f\n", it, b->x, b->z);
      for (int i = 0; i < ns; i++) { float dx, dz; int beh; float d = evalShape(&sh[i], &R, &dx, &dz, &beh);
        printf("   shape %2d kind %d type %d idx %u np %d na %d rad %.2f: depth %.4f dir %+.3f %+.3f%s", i, sh[i].kind, sh[i].ptype, sh[i].idx,
               sh[i].np, sh[i].na, sh[i].rad, d, dx, dz, i == bi ? "  <== push" : "");
        for (int k = 0; k < sh[i].np && k < 4; k++) printf(" p%d %.2f %.2f", k, sh[i].p[k][0], sh[i].p[k][1]);
        printf(" | rect c %.2f %.2f f %.3f %.3f hl %.2f hw %.2f\n", R.cx, R.cz, R.fx, R.fz, R.hl, R.hw); }
    }
#endif
    if (bi < 0) { clean = 1; break; }
    if (!HUMP_WALL_SOFT_OLD && sh[bi].ptype == COL_CYL && sh[bi].idx < w->ncyl && w->cyl[sh[bi].idx].pad[0] != 1.f && w->scr->humpAt[sh[bi].idx] != ~0u) {
      float room = HUMP_WALL_STEP - humpPushed; if (room < 0.f) room = 0.f;
      if (bd > room) bd = room;
      humpPushed += bd;
      if (bd <= PUSH_EPS) { addContact(b, &sh[bi], bdx, bdz, bd); if (bd > bestD) { bestD = bd; bnx = bdx; bnz = bdz; } break; }
    }
    if (it >= 3) bd *= .5f;   // зажат с двух сторон — полшага: сходимся к равным проникновениям, а не качаемся
    b->x += bdx * bd; b->z += bdz * bd;
    if (bbeh) b->behindFence = 1;
    addContact(b, &sh[bi], bdx, bdz, bd);
    if (bd > bestD) { bestD = bd; bnx = bdx; bnz = bdz; }
  }
  if (!clean && ns) {   // итерации кончились (зажат с двух сторон): лучшее из пройденных положений
    Rect R; rectOf(b, &R);
    float d = 0.f;
    for (int i = 0; i < ns; i++) {
      float dx, dz; int beh; float e = evalShape(&sh[i], &R, &dx, &dz, &beh);
      if (e > d) d = e;
    }
    if (minD < d) { b->x = minX; b->z = minZ; d = minD; }
    b->maxDepth = d;
  }
  if (mtvX) *mtvX = b->x - x0;
  if (mtvZ) *mtvZ = b->z - z0;
  if (nX) *nX = bnx;
  if (nZ) *nZ = bnz;
  return b->n;
}

int colProbeCar(const ColWorld *w, CarBox *b) {
  Q q; int ns = gather(w, b, &q);
  const Shape *sh = w->scr->shape;
  Rect R; rectOf(b, &R);
  b->n = 0; b->behindFence = 0; b->maxDepth = 0; b->nshapes = ns;
  for (int i = 0; i < ns; i++) {
    float dx, dz; int beh; float d = evalShape(&sh[i], &R, &dx, &dz, &beh);
    if (d > PUSH_EPS) {
      addContact(b, &sh[i], dx, dz, d);
      if (d > b->maxDepth) b->maxDepth = d;
      if (beh) b->behindFence = 1;
    }
  }
  return b->n;
}

void colBoxBand(const CarBox *b, float *ylo, float *yhi) {   // слой кузова по высоте — как в gather
  float fx = b->fx, fz = b->fz, rx = -fz, rz = fx;
  float ex = fabsf(fx) * b->hl + fabsf(rx) * b->hw + GATHER_MARGIN, ez = fabsf(fz) * b->hl + fabsf(rz) * b->hw + GATHER_MARGIN;
  float sl = fabsf(b->gx) * ex + fabsf(b->gz) * ez;
  *ylo = b->y + b->lo - sl; *yhi = b->y + b->hi + sl;
}

int colCrossFence(const ColWorld *w, float x0, float z0, float x1, float z1, float *nX, float *nZ) {
  return colCrossFenceY(w, x0, z0, x1, z1, -1e30f, 1e30f, nX, nZ);
}

static int crossFence(const ColWorld *w, float x0, float z0, float x1, float z1, float ylo, float yhi, const CarBox *bx, float *nX, float *nZ);
int colCrossFenceY(const ColWorld *w, float x0, float z0, float x1, float z1, float ylo, float yhi, float *nX, float *nZ) {
  return crossFence(w, x0, z0, x1, z1, ylo, yhi, 0, nX, nZ);
}
int colCrossFenceBox(const ColWorld *w, float x0, float z0, float x1, float z1, const CarBox *b, float *nX, float *nZ) {
  return crossFence(w, x0, z0, x1, z1, -1e30f, 1e30f, b, nX, nZ);
}
static int crossFence(const ColWorld *w, float x0, float z0, float x1, float z1, float ylo, float yhi, const CarBox *bx, float *nX, float *nZ) {
  STAT(2);
  ColScratch *S = w->scr;
  int gx0 = (int)floorf((fminf_(x0, x1) - 0.1f - w->ox) * w->inv), gx1 = (int)floorf((fmaxf_(x0, x1) + 0.1f - w->ox) * w->inv);
  int gz0 = (int)floorf((fminf_(z0, z1) - 0.1f - w->oz) * w->inv), gz1 = (int)floorf((fmaxf_(z0, z1) + 0.1f - w->oz) * w->inv);
  if (gx0 < 0) gx0 = 0;
  if (gz0 < 0) gz0 = 0;
  if (gx1 >= w->W) gx1 = w->W - 1;
  if (gz1 >= w->H) gz1 = w->H - 1;
  if (gx0 > gx1 || gz0 > gz1) return 0;
  uint32_t o3 = w->ntri + w->nbox + w->ncyl, nprim = o3 + w->nseg;
  uint16_t st = nextStamp(S, nprim);
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx;
      uint32_t k = w->cellStart[c + 1];
      while (k > w->cellStart[c] && (w->refs[k - 1] >> 29) == COL_SEG) k--;   // заборы — в конце списка клетки
      for (; k < w->cellStart[c + 1]; k++) {
        uint32_t i = w->refs[k] & 0x1fffffffu;
        if (S->stamp[o3 + i] == st) continue;
        S->stamp[o3 + i] = st;
        const ColSeg *G = &w->seg[i];
        if (colPassFol && segFolKeep(w, i, (x0 + x1) * .5f, bx ? bx->y + bx->lo : ylo, (z0 + z1) * .5f)) continue;   // правка 173: изгородь (304: с памятью)
        if (colPassBack && segBackKept(i)) continue;   // сверка PS2 09.10: кузов, пришедший с изнанки, ещё на линии — назад можно
        if (G->y1 < ylo || G->y0 > yhi) continue;   // внутренний забор с конечным верхом (tools/psp_export.py col_fence_tops): путь над ним
        float s0 = (x0 - G->a[0]) * G->n[0] + (z0 - G->a[1]) * G->n[1];
        float s1 = (x1 - G->a[0]) * G->n[0] + (z1 - G->a[1]) * G->n[1];
        if (!(s0 >= 0.f && s1 < 0.f)) continue;
        float t = s0 / (s0 - s1), px = x0 + (x1 - x0) * t - G->a[0], pz = z0 + (z1 - z0) * t - G->a[1];
        float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], L2 = dx * dx + dz * dz;
        float u = px * dx + pz * dz;
        if (u < 0.f || u > L2) continue;
        if (bx && !(G->y1 > 1e5f && G->y0 < -1e5f)) {           // конечный забор: слой кузова по полу в точке пересечения
          float fy = bx->y + bx->gx * (px + G->a[0] - bx->x) + bx->gz * (pz + G->a[1] - bx->z), top = G->y1;
          // правка 339: на опоре — не ниже опоры + COL_FENCE_HOLD, если до шага кузов был с лица (заходил за линию не глубже COL_FENCE_SUP_PEN).
          // Кузов, уже стоящий поперёк линии (приземлился, вытолкнуло), не держим: иначе каждый подшаг отменялся бы — машина замирала на линии
          if (bx->fsupOn && !colFenceSupOff && G->y0 >= COL_SEG_FOL_Y0) {
            float re = bx->hl * fabsf(bx->fx * G->n[0] + bx->fz * G->n[1]) + bx->hw * fabsf(-bx->fz * G->n[0] + bx->fx * G->n[1]);
            if (s0 >= re - COL_FENCE_SUP_PEN && (!SEAM_FENCE_OLD ? G->y1 > bx->fsup : 1)) top = fmaxf_(top, bx->fsup + COL_FENCE_HOLD);
          }
          if (top < fy + bx->lo || G->y0 > fy + bx->hi) continue;
        }
        if (nX) *nX = G->n[0];
        if (nZ) *nZ = G->n[1];
        return 1;
      }
    }
  return 0;
}

// ---------------------------------------------------------------------------------------------- луч (камера)
// Правки Роба 40 и 27 (28.09): камера пешком и за машиной не заходит в стены и геометрию — луч от точки взгляда к глазу.
static float rayTri(const ColTri *T, const float o[3], const float d[3]) {   // Мёллер–Трумбор, с обеих сторон; t в долях d
  float e1[3] = {T->b[0] - T->a[0], T->b[1] - T->a[1], T->b[2] - T->a[2]}, e2[3] = {T->c[0] - T->a[0], T->c[1] - T->a[1], T->c[2] - T->a[2]};
  float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
  float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
  if (fabsf(det) < 1e-12f) return 2.f;
  float inv = 1.f / det, s[3] = {o[0] - T->a[0], o[1] - T->a[1], o[2] - T->a[2]};
  float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv; if (u < 0.f || u > 1.f) return 2.f;
  float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
  float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0.f || u + v > 1.f) return 2.f;
  float t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
  return t >= 0.f ? t : 2.f;
}
static float rayBox(const ColBox *B, const float o[3], const float d[3]) {   // ящик: плиты по осям; начало внутри — не мешает
  const float *A[3] = {B->ax, B->ay, B->az}; float tmin = 0.f, tmax = 1.f; int inside = 1;
  for (int k = 0; k < 3; k++) {
    float lo = (o[0] - B->c[0]) * A[k][0] + (o[1] - B->c[1]) * A[k][1] + (o[2] - B->c[2]) * A[k][2];
    float dd = d[0] * A[k][0] + d[1] * A[k][1] + d[2] * A[k][2], h = B->h[k];
    if (lo < -h || lo > h) inside = 0;
    if (fabsf(dd) < 1e-9f) { if (lo < -h || lo > h) return 2.f; continue; }
    float t1 = (-h - lo) / dd, t2 = (h - lo) / dd; if (t1 > t2) { float q = t1; t1 = t2; t2 = q; }
    if (t1 > tmin) tmin = t1; if (t2 < tmax) tmax = t2;
    if (tmin > tmax) return 2.f;
  }
  return inside ? 2.f : tmin;
}
static float raySphere(const float c[3], float r, const float o[3], const float d[3]) {
  float oc[3] = {o[0] - c[0], o[1] - c[1], o[2] - c[2]}, a = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
  float b = oc[0] * d[0] + oc[1] * d[1] + oc[2] * d[2], cc = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - r * r;
  if (cc <= 0.f) return 2.f;                   // начало внутри — не мешает
  float h = b * b - a * cc; if (h < 0.f || a <= 0.f) return 2.f;
  float t = (-b - sqrtf(h)) / a; return t >= 0.f ? t : 2.f;
}
#ifndef COL_CAMERA_FLAT_CAPSULE_OLD
#define COL_CAMERA_FLAT_CAPSULE_OLD 0
#endif
static float rayCylinder(const ColCyl *C,const float o[3],const float d[3]) {
  float oc[3],op[3],dp[3],along=0.f,speed=0.f;
  for(int k=0;k<3;k++){oc[k]=o[k]-C->c[k];along+=oc[k]*C->axis[k];speed+=d[k]*C->axis[k];}
  float aa=0.f,bb=0.f,cc=-C->r*C->r;
  for(int k=0;k<3;k++) {
    op[k]=oc[k]-along*C->axis[k];dp[k]=d[k]-speed*C->axis[k];
    aa+=dp[k]*dp[k];bb+=op[k]*dp[k];cc+=op[k]*op[k];
  }
  if(cc<=0.f && fabsf(along)<=C->halfLen)return 2.f; // same inside-origin convention as other shapes
  float enter=0.f,exit=1.f;
  if(aa>0.f) {
    float disc=bb*bb-aa*cc;if(disc<0.f)return 2.f;
    float q=-bb-copysignf(sqrtf(disc),bb);
    float u=q/aa,v=q!=0.f?cc/q:u;
    enter=fmaxf(enter,fminf(u,v));exit=fminf(exit,fmaxf(u,v));
  } else if(cc>0.f)return 2.f;
  if(speed!=0.f) {
    float u=(-C->halfLen-along)/speed,v=(C->halfLen-along)/speed;
    enter=fmaxf(enter,fminf(u,v));exit=fminf(exit,fmaxf(u,v));
  } else if(fabsf(along)>C->halfLen)return 2.f;
  return enter<=exit?enter:2.f;
}
static float rayCapsule(const ColCyl *C, const float o[3], const float d[3]) {
  float hl = C->halfLen, r = C->r;
  if(r<=0.f)return 2.f;
  if (hl <= 0.f) return raySphere(C->c, r, o, d);
  if(C->flat && !COL_CAMERA_FLAT_CAPSULE_OLD)return rayCylinder(C,o,d);
  float pa[3] = {C->c[0] - C->axis[0] * hl, C->c[1] - C->axis[1] * hl, C->c[2] - C->axis[2] * hl};
  float ba[3] = {2 * hl * C->axis[0], 2 * hl * C->axis[1], 2 * hl * C->axis[2]}, oa[3] = {o[0] - pa[0], o[1] - pa[1], o[2] - pa[2]};
  float baba = ba[0] * ba[0] + ba[1] * ba[1] + ba[2] * ba[2], bard = ba[0] * d[0] + ba[1] * d[1] + ba[2] * d[2];
  float baoa = ba[0] * oa[0] + ba[1] * oa[1] + ba[2] * oa[2], rdoa = d[0] * oa[0] + d[1] * oa[1] + d[2] * oa[2];
  float rdrd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2], oaoa = oa[0] * oa[0] + oa[1] * oa[1] + oa[2] * oa[2];
  { float u = baoa / baba; u = u < 0 ? 0 : u > 1 ? 1 : u;   // начало внутри — не мешает
    float qx = oa[0] - ba[0] * u, qy = oa[1] - ba[1] * u, qz = oa[2] - ba[2] * u; if (qx * qx + qy * qy + qz * qz <= r * r) return 2.f; }
  float a = baba * rdrd - bard * bard, b = baba * rdoa - baoa * bard, c = baba * oaoa - baoa * baoa - r * r * baba;
  float h = b * b - a * c, best = 2.f;
  if (h >= 0.f && a > 1e-12f) {
    float t = (-b - sqrtf(h)) / a, y = baoa + t * bard;
    if (t >= 0.f && y > 0.f && y < baba) best = t;
  }
  float e0 = raySphere(pa, r, o, d); if (e0 < best) best = e0;
  float pb[3] = {pa[0] + ba[0], pa[1] + ba[1], pa[2] + ba[2]}; float e1 = raySphere(pb, r, o, d); if (e1 < best) best = e1;
  return best;
}
// №922 (Роб, TEST58: «при появлении в машине перед домом камера в гараже, всё коричневое»): стена фасада гаража в коллизии кончается на 6,36 м, а крыша гаража есть только в картинке (без
// коллизии); глаз камеры за машиной (9,5 м назад, 4,5 вверх) проходил над стеной и оказывался под видимой крышей. Для луча «опора -> глаз» (маска 128 в csRayFrac) высокие стены (верх не ниже
// COL_TALL, то есть здания) продолжены вверх на colSegUp. Пешеходам и машинам — как было (colSegUp = 0 вне луча камеры). CAM_WALLUP_OLD=1 / flag camwallupold 1 — как было
#define COL_TALL 4.f
float colSegUp;
static float raySeg(const ColSeg *G, const float o[3], const float d[3]) {   // забор: вертикальная стена по отрезку, обе стороны
  float s0 = (o[0] - G->a[0]) * G->n[0] + (o[2] - G->a[1]) * G->n[1], sd = d[0] * G->n[0] + d[2] * G->n[1];
  if (fabsf(sd) < 1e-9f) return 2.f;
  float t = -s0 / sd; if (t < 0.f || t > 1.f) return 2.f;
  float y = o[1] + d[1] * t; if (y < G->y0 || y > G->y1 + (G->y1 >= COL_TALL ? colSegUp : 0.f)) return 2.f;
  float px = o[0] + d[0] * t - G->a[0], pz = o[2] + d[2] * t - G->a[1], dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1];
  float u = px * dx + pz * dz; return (u < 0.f || u > dx * dx + dz * dz) ? 2.f : t;
}
// CAMPERF (ускорение камеры, дирижёр 23.10): 21 луч ближней плоскости камеры (csNearFree) лежат в одной пирамиде в 1–3 м. colRayFrac гнал каждый по своим клеткам
// и считал rayTri (деление) для каждого треугольника клетки — ~10 мкс на луч. Пакет: клетки коробки всех лучей обходятся один раз (штамп — как в colRayFrac), треугольник
// проверяется ящиком против коробки пакета и только потом лучами, чьи коробки его касаются (запас COL_RAYS_EPS на округление). Ответ каждого луча тот же, что у
// colRayFrac: тот же набор задетых кусков (кусок лежит во всех клетках, которые покрывает), те же rayTri/rayBox/rayCapsule/raySeg. CAM_RAYS_OLD=1 / flag camraysold 1 — по одному лучу
#ifndef COL_RAYS_EPS
#define COL_RAYS_EPS 1e-3f   // красный контроль tests/camera/camperf_host.c: -DCOL_RAYS_EPS=-0.5f (ящик слишком тесный — лучи теряют куски)
#endif
void colRaysFrac(const ColWorld *w, int n, const float (*a)[3], const float (*b)[3], float *out) {
  for (int i = 0; i < n; i++) out[i] = 1.f;
  if (!w || !w->scr || n <= 0 || n > COL_RAYS_MAX) return;
  ColScratch *S = w->scr; float d[COL_RAYS_MAX][3], lo[COL_RAYS_MAX][3], hi[COL_RAYS_MAX][3], ulo[3], uhi[3];
  int gx0 = 1 << 30, gx1 = -(1 << 30), gz0 = 1 << 30, gz1 = -(1 << 30);
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < 3; k++) { d[i][k] = b[i][k] - a[i][k]; lo[i][k] = fminf_(a[i][k], b[i][k]) - COL_RAYS_EPS; hi[i][k] = fmaxf_(a[i][k], b[i][k]) + COL_RAYS_EPS;
      if (i == 0 || lo[i][k] < ulo[k]) ulo[k] = lo[i][k]; if (i == 0 || hi[i][k] > uhi[k]) uhi[k] = hi[i][k]; }
    int x0 = (int)floorf((fminf_(a[i][0], b[i][0]) - 0.1f - w->ox) * w->inv), x1 = (int)floorf((fmaxf_(a[i][0], b[i][0]) + 0.1f - w->ox) * w->inv);
    int z0 = (int)floorf((fminf_(a[i][2], b[i][2]) - 0.1f - w->oz) * w->inv), z1 = (int)floorf((fmaxf_(a[i][2], b[i][2]) + 0.1f - w->oz) * w->inv);
    if (x0 < gx0) gx0 = x0; if (x1 > gx1) gx1 = x1; if (z0 < gz0) gz0 = z0; if (z1 > gz1) gz1 = z1;
  }
  if (gx0 < 0) gx0 = 0; if (gz0 < 0) gz0 = 0; if (gx1 >= w->W) gx1 = w->W - 1; if (gz1 >= w->H) gz1 = w->H - 1;
  if (gx0 > gx1 || gz0 > gz1) return;
  uint32_t o1 = w->ntri, o2 = o1 + w->nbox, o3 = o2 + w->ncyl, nprim = o3 + w->nseg;
  uint16_t st = nextStamp(S, nprim);
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx;
      for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; k++) {
        uint32_t t = w->refs[k] >> 29, i = w->refs[k] & 0x1fffffffu, gid = t == COL_TRI ? i : t == COL_BOX ? o1 + i : t == COL_CYL ? o2 + i : o3 + i;
        if (S->stamp[gid] == st) continue;
        S->stamp[gid] = st;
        if (t == COL_TRI) {
          const ColTri *T = &w->tri[i]; float tl[3], th[3];
          for (int q = 0; q < 3; q++) { tl[q] = fminf_(T->a[q], fminf_(T->b[q], T->c[q])); th[q] = fmaxf_(T->a[q], fmaxf_(T->b[q], T->c[q])); }
          if (tl[0] > uhi[0] || th[0] < ulo[0] || tl[1] > uhi[1] || th[1] < ulo[1] || tl[2] > uhi[2] || th[2] < ulo[2]) continue;
          for (int r = 0; r < n; r++) {
            if (tl[0] > hi[r][0] || th[0] < lo[r][0] || tl[1] > hi[r][1] || th[1] < lo[r][1] || tl[2] > hi[r][2] || th[2] < lo[r][2]) continue;
            float h = rayTri(T, a[r], d[r]); if (h < out[r]) out[r] = h; }
        } else for (int r = 0; r < n; r++) {
          float h = 2.f;
          if (t == COL_BOX) { if (w->box[i].pad != 1.f) h = rayBox(&w->box[i], a[r], d[r]); }
          else if (t == COL_CYL) { if (w->cyl[i].pad[0] != 1.f) h = rayCapsule(&w->cyl[i], a[r], d[r]); }
          else h = raySeg(&w->seg[i], a[r], d[r]);
          if (h < out[r]) out[r] = h; }
      }
    }
}
float colRayFrac(const ColWorld *w, const float a[3], const float b[3], int foliage) {
  if (!w || !w->scr) return 1.f;
  ColScratch *S = w->scr;
  float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, best = 1.f;
  int gx0 = (int)floorf((fminf_(a[0], b[0]) - 0.1f - w->ox) * w->inv), gx1 = (int)floorf((fmaxf_(a[0], b[0]) + 0.1f - w->ox) * w->inv);
  int gz0 = (int)floorf((fminf_(a[2], b[2]) - 0.1f - w->oz) * w->inv), gz1 = (int)floorf((fmaxf_(a[2], b[2]) + 0.1f - w->oz) * w->inv);
  if (gx0 < 0) gx0 = 0; if (gz0 < 0) gz0 = 0; if (gx1 >= w->W) gx1 = w->W - 1; if (gz1 >= w->H) gz1 = w->H - 1;
  if (gx0 > gx1 || gz0 > gz1) return 1.f;
  uint32_t o1 = w->ntri, o2 = o1 + w->nbox, o3 = o2 + w->ncyl, nprim = o3 + w->nseg;
  uint16_t st = nextStamp(S, nprim);
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      uint32_t c = (uint32_t)gz * (uint32_t)w->W + (uint32_t)gx;
      for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; k++) {
        uint32_t t = w->refs[k] >> 29, i = w->refs[k] & 0x1fffffffu, gid = t == COL_TRI ? i : t == COL_BOX ? o1 + i : t == COL_CYL ? o2 + i : o3 + i;
        if (S->stamp[gid] == st) continue;
        S->stamp[gid] = st;
        float h = 2.f;
        if (t == COL_TRI) h = rayTri(&w->tri[i], a, d);
        else if (t == COL_BOX) { if (foliage || w->box[i].pad != 1.f) h = rayBox(&w->box[i], a, d); }
        else if (t == COL_CYL) { if (foliage || w->cyl[i].pad[0] != 1.f) h = rayCapsule(&w->cyl[i], a, d); }
        else h = raySeg(&w->seg[i], a, d);
        if (h < best) best = h;
      }
    }
  return best;
}
