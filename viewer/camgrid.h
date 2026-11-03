// №620 дешёвая версия (CAM_GEOM_FAST, по умолчанию 1): грубая сетка занятости клетки по ВИДИМЫМ треугольникам города (воксель CG_VX м). Строится один раз
// при загрузке клетки (loadCell), в кадре — только шаги луча опора -> глаз по сетке (~0,1 мс), без выборки вершин. Горизонтальные грани (земля, плоские
// крыши, |ny| > CG_NY) не метятся: земля и крыши — дело коллизии, иначе луч вдоль дороги цеплял бы воксель земли. CAM_GEOM_FAST=0 — прежний путь по
// треугольникам (раз за кадр собираются в сцену). Файл зависит от Piece/Cell (main_state.h).
#ifndef SHAR_CAMGRID_H
#define SHAR_CAMGRID_H
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef CAM_GEOM_FAST
#define CAM_GEOM_FAST 1
#endif
#define CG_VX .75f
// №1141 (TEST84, L5, старт миссии: камера сразу проходит сквозь вывеску дома): сетка занятости строится по одной клетке, не больше CG_US2 мкс за кадр, — после переноса героя (старт миссии, сброс машины)
// клетки у новой точки стояли без сетки, и пока она не достроится (десятки кадров), глаз и ближняя плоскость геометрию города не видели. Теперь при переносе опоры или загрузке уровня
// клетки в CAM_START_R м от глаза и опоры достраиваются сразу, целиком (cgBuildNear, camGeomCollect), и список клеток луча обновляется при каждой достроенной клетке.
// STARTCAM1141_OLD=1 — как в TEST84 (порционно, список клеток луча раз в 8 кадров)
#ifndef STARTCAM1141_OLD
#define STARTCAM1141_OLD 0
#endif
#ifndef CG_PROF
#define CG_PROF 0   // 1 — считать мкс в cgStep/cgRayFrac (строка CAMIN gusray/gusstep); сам таймер дорог
#endif
#define CG_TIME() (CG_PROF ? sceKernelGetSystemTimeLow() : 0u)
#ifndef CG_SCAN_OLD
#define CG_SCAN_OLD 0
#endif
#define CG_NY .8f
#define CG_MAXN 110
static inline int cgIdx(const Cell *c, int ix, int iy, int iz) { return (iz * c->cgn[1] + iy) * c->cgn[0] + ix; }
// CAMPERF (ускорение камеры, дирижёр 23.10): после битов вокселей в той же памяти — грубая сетка блоков 4 × 4 × 4 вокселя (3 м): бит блока ставится вместе с битом вокселя.
// Луч, чья коробка не касается ни одного занятого блока, не может задеть воксель — отвечает «свободно» без шагов (cgRayFrac2); ответ тот же. CG_COARSE_OLD=1 / flag cgcoarseold 1 — шаги как было
#ifndef CG_COARSE_OLD
#define CG_COARSE_OLD 0
#endif
static int cgCoarseOld = CG_COARSE_OLD;
static inline unsigned cgFineBytes(const Cell *c) { return ((unsigned)c->cgn[0] * c->cgn[1] * c->cgn[2] + 7u) >> 3; }
static inline unsigned cgCoarseBytes(const unsigned char n[3]) { return ((((unsigned)n[0] + 3u) >> 2) * (((unsigned)n[1] + 3u) >> 2) * (((unsigned)n[2] + 3u) >> 2) + 7u) >> 3; }
static inline void cgMark(Cell *c, const float p[3]) {
  int ix = (int)((p[0] - c->bb[0]) / CG_VX), iy = (int)((p[1] - c->bb[1]) / CG_VX), iz = (int)((p[2] - c->bb[2]) / CG_VX);
  if (ix < 0 || iy < 0 || iz < 0 || ix >= c->cgn[0] || iy >= c->cgn[1] || iz >= c->cgn[2]) return;
  int i = cgIdx(c, ix, iy, iz); ((unsigned char *)c->cg)[i >> 3] |= (unsigned char)(1u << (i & 7));
  int j = (((iz >> 2) * ((c->cgn[1] + 3) >> 2) + (iy >> 2)) * ((c->cgn[0] + 3) >> 2)) + (ix >> 2); ((unsigned char *)c->cg)[cgFineBytes(c) + (j >> 3)] |= (unsigned char)(1u << (j & 7));
}
// коробка lo..hi касается занятого блока клетки? (индексы вокселей считаются тем же выражением, что в cgOcc: усечение к нулю не нарушает порядок)
static int cgCoarseHit(const Cell *c, const float lo[3], const float hi[3]) {
  int i0[3], i1[3];
  for (int k = 0; k < 3; k++) if (hi[k] < c->bb[k] - CG_VX - 1e-3f || lo[k] > c->bb[k] + (float)c->cgn[k] * CG_VX + 1e-3f) return 0;   // коробка луча вне объёма сетки (с запасом: усечение к нулю захватывает воксель 0 ниже bb) — без делений
  for (int k = 0; k < 3; k++) { i0[k] = (int)((lo[k] - c->bb[k]) / CG_VX); i1[k] = (int)((hi[k] - c->bb[k]) / CG_VX);
    if (i0[k] < 0) i0[k] = 0; if (i1[k] >= c->cgn[k]) i1[k] = c->cgn[k] - 1; if (i0[k] > i1[k]) return 0; }
  const unsigned char *cb = (const unsigned char *)c->cg + cgFineBytes(c); int n0 = (c->cgn[0] + 3) >> 2, n1 = (c->cgn[1] + 3) >> 2;
  for (int z = i0[2] >> 2; z <= i1[2] >> 2; z++) for (int y = i0[1] >> 2; y <= i1[1] >> 2; y++) for (int x = i0[0] >> 2; x <= i1[0] >> 2; x++) {
    int j = (z * n1 + y) * n0 + x; if ((cb[j >> 3] >> (j & 7)) & 1) return 1; }
  return 0;
}
#ifndef CG_DIRTY_OLD
#define CG_DIRTY_OLD 0
#endif
#ifndef CG_PEND_OLD
#define CG_PEND_OLD 0
#endif
#ifndef CG_PEND_RED
#define CG_PEND_RED 0   // красный контроль гейта camf924: быстрый путь берёт самую дальнюю недостроенную клетку, а не ближайшую — выбор стройки расходится
#endif
#define CG_PEND_N 1024   // в езде недостроенных клеток 210–250 (L4, L7): список в 96 переполнялся всегда и быстрый путь не работал ни разу
static int cgPendOld = CG_PEND_OLD, cgPendN = -1, cgPendK = -1; static unsigned short cgPend[CG_PEND_N]; static float cgPendX[CG_PEND_N], cgPendZ[CG_PEND_N];   // №924: список недостроенных клеток; cgPendK — место выбранной клетки (вычёркивается, когда сетка достроена)
static unsigned cgPendSeen, cgPendGen, cgPendNc; static const void *cgPendCl;   // счётчик загрузок, поколение клеток, число и массив клеток на момент сбора списка: смена уровня/выгрузка/загрузка — список недействителен
static unsigned cgPendBad, cgPendHit, cgPendFull, cgPendOver, cgPendMax, cgIdleSkip;   // служебное (CAMIN pend=): быстрый список / полный перебор / переполнение списка / наибольшее число недостроенных / пропуски в покое
static void cgPendDrop(void) {   // сетка клетки достроена — клетка выходит из списка (порядок остальных тот же, как у полного перебора по индексу)
  int k = cgPendK; cgPendK = -1; if (k < 0 || cgPendN <= 0 || k >= cgPendN) return;
  memmove(cgPend + k, cgPend + k + 1, (size_t)(cgPendN - k - 1) * sizeof cgPend[0]); memmove(cgPendX + k, cgPendX + k + 1, (size_t)(cgPendN - k - 1) * sizeof cgPendX[0]); memmove(cgPendZ + k, cgPendZ + k + 1, (size_t)(cgPendN - k - 1) * sizeof cgPendZ[0]); cgPendN--;
}
static int cgDirtyOld = CG_DIRTY_OLD; static volatile unsigned cgDirty; static unsigned cgDirtySeen;   // №924: счётчик загрузок клеток (main_stream.h) и последнее увиденное перебором значение
static unsigned cgWork;   // метки, поставленные за кадр (бюджет работы построения)
// CAMPERF: разметка треугольника по листьям ставила 4 метки на лист (3 вершины + центр, каждая метка — 3 деления), а вершина общая у соседних листьев. Метки те же (вершины листьев —
// углы треугольника и середины делений; середина ставится при делении), листьев и cgWork столько же, меток втрое меньше. CG_TRI_OLD=1 / flag cgtriold 1 — по листьям
#ifndef CG_TRI_OLD
#define CG_TRI_OLD 0
#endif
static int cgTriOld = CG_TRI_OLD;
static void cgTriOldF(Cell *c, const float a[3], const float b[3], const float d[3], int depth);
static void cgTriNew(Cell *c, const float a[3], const float b[3], const float d[3], int depth) {
  float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, bd[3] = {d[0] - b[0], d[1] - b[1], d[2] - b[2]}, da[3] = {a[0] - d[0], a[1] - d[1], a[2] - d[2]};
  float l0 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2], l1 = bd[0] * bd[0] + bd[1] * bd[1] + bd[2] * bd[2], l2 = da[0] * da[0] + da[1] * da[1] + da[2] * da[2];
  float lm = l0 > l1 ? (l0 > l2 ? l0 : l2) : (l1 > l2 ? l1 : l2);
  if (lm <= CG_VX * CG_VX * .6f || depth >= 6) { cgWork++; float m[3] = {(a[0] + b[0] + d[0]) / 3, (a[1] + b[1] + d[1]) / 3, (a[2] + b[2] + d[2]) / 3}; cgMark(c, m); return; }
  float mid[3];
  if (lm == l0) { for (int k = 0; k < 3; k++) mid[k] = (a[k] + b[k]) * .5f; cgMark(c, mid); cgTriNew(c, a, mid, d, depth + 1); cgTriNew(c, mid, b, d, depth + 1); }
  else if (lm == l1) { for (int k = 0; k < 3; k++) mid[k] = (b[k] + d[k]) * .5f; cgMark(c, mid); cgTriNew(c, a, b, mid, depth + 1); cgTriNew(c, a, mid, d, depth + 1); }
  else { for (int k = 0; k < 3; k++) mid[k] = (d[k] + a[k]) * .5f; cgMark(c, mid); cgTriNew(c, a, b, mid, depth + 1); cgTriNew(c, mid, b, d, depth + 1); }
}
static void cgTri(Cell *c, const float a[3], const float b[3], const float d[3], int depth) {
  if (!cgTriOld) { cgMark(c, a); cgMark(c, b); cgMark(c, d); cgTriNew(c, a, b, d, depth); return; }
  cgTriOldF(c, a, b, d, depth);
}
static void cgTriOldF(Cell *c, const float a[3], const float b[3], const float d[3], int depth) {
  float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, bd[3] = {d[0] - b[0], d[1] - b[1], d[2] - b[2]}, da[3] = {a[0] - d[0], a[1] - d[1], a[2] - d[2]};
  float l0 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2], l1 = bd[0] * bd[0] + bd[1] * bd[1] + bd[2] * bd[2], l2 = da[0] * da[0] + da[1] * da[1] + da[2] * da[2];
  float lm = l0 > l1 ? (l0 > l2 ? l0 : l2) : (l1 > l2 ? l1 : l2);
  if (lm <= CG_VX * CG_VX * .6f || depth >= 6) { cgWork++; float m[3] = {(a[0] + b[0] + d[0]) / 3, (a[1] + b[1] + d[1]) / 3, (a[2] + b[2] + d[2]) / 3}; cgMark(c, a); cgMark(c, b); cgMark(c, d); cgMark(c, m); return; }
  float mid[3];
  if (lm == l0) { for (int k = 0; k < 3; k++) mid[k] = (a[k] + b[k]) * .5f; cgTriOldF(c, a, mid, d, depth + 1); cgTriOldF(c, mid, b, d, depth + 1); }
  else if (lm == l1) { for (int k = 0; k < 3; k++) mid[k] = (b[k] + d[k]) * .5f; cgTriOldF(c, a, b, mid, depth + 1); cgTriOldF(c, a, mid, d, depth + 1); }
  else { for (int k = 0; k < 3; k++) mid[k] = (d[k] + a[k]) * .5f; cgTriOldF(c, a, b, mid, depth + 1); cgTriOldF(c, mid, b, d, depth + 1); }
}
// Порционно (№620: после телепорта грузится много клеток разом — строить всё сразу нельзя): cgStep зовётся раз в кадр из главного потока, строит сетку
// одной ближней клетки не больше CG_BUDGET треугольников за кадр; пока сетка не готова (cgs != 2), клетка в луче не участвует
#define CG_US 100u   // мкс на кадр: время, не число треугольников (глубина разбиения у больших стен разная)
static unsigned cgDone, cgVox, cgDbg, cgFrame, cgReuse, cgUsStep, cgUsRay, cgStepN;
static float cgS[16]; static unsigned cgTotTri, cgSkipNy;
static void cgStep2(float ex, float ez);
static Cell *cgForceCell;   // №1141: клетка, которую cgStep2 достраивает целиком, без бюджета кадра
// TEST24 (цена, ph6): стройка сетки съедала ~320 мкс на кадр при бюджете 100 (время сверялось через треугольник, крупные — до 96 меток). Теперь бюджет CG_US2 мкс, проверка каждый треугольник, до 32 меток.
// CG_BUDGET_OLD=1 / flag cgbudgetold 1 — как было
#ifndef CG_US2
#define CG_US2 40
#endif
#ifndef CG_BUDGET_OLD
#define CG_BUDGET_OLD 0
#endif
static int cgBudgetOld = CG_BUDGET_OLD;
// служебное (flag camdet 1, только смоук): шаг кадра ровно 1/30 с и бюджет стройки в числе треугольников, а не во времени — прогоны старого и нового кода идут кадр в кадр (гейт tests/camera/camperf)
static int camDet; 
#define CG_DET_TRIS 12
static unsigned cgPf[4]; static float cgAhX = -1e9f, cgAhZ;
// №1159 (TEST85: фаза камеры «cam» в 2–3 раза дороже, CAMSUB.safe +0,7 мс на PSP; в эмуляторе STARTCAM1141_OLD=1 возвращает 1,4 → 0,6 мс): окно старта камеры №1141 (camGeomCollect: 120 вызовов с достройкой клеток)
// взводилось по смене cgGen, а cgGen растёт при каждой выгрузке клетки города (main_stream.h) — то есть при езде, постоянно: камера всё время строила сетки клеток рядом целиком. Окно должно взводиться только
// переносом опоры больше CAM_START_JUMP и загрузкой уровня: для неё свой счётчик camLevelGen (растёт в main_levels.h вместе с кэшами камеры). CAMSTARTGEN1159_OLD=1 — по cgGen, как в TEST85
#ifndef CAMSTARTGEN1159_OLD
#define CAMSTARTGEN1159_OLD 0
#endif
static unsigned camLevelGen = 1;
static int cgProf; static unsigned cgGen = 1;   // cgProf — таймеры камеры только под smoke flag camin; cgGen — поколение клеток (выгрузка/смена уровня сбрасывает кэш клеток луча)
static unsigned cgBuildN, cgBuildUs, cgScanN, cgScanUs;   // служебное (CAMIN gb/gs): кадры со стройкой сетки (> 150 мкс) и обычные
static void cgStep(float ex, float ez) { unsigned t0 = cgProf ? sceKernelGetSystemTimeLow() : 0; cgFrame++; cgStep2(ex, ez); if (!cgProf) { cgStepN++; return; } unsigned d_ = sceKernelGetSystemTimeLow() - t0; if (CG_PROF) cgUsStep += d_; cgStepN++; if (d_ > 150) { cgBuildN++; cgBuildUs += d_; } else { cgScanN++; cgScanUs += d_; } }
static void cgStep2(float ex, float ez) {
  if (!CAM_GEOM_FAST || !cl) return;
  Cell *forced = cgForceCell; cgForceCell = 0;
  // Аудит FPS: перебор всех клеток (1100+) каждый кадр ради «есть ли недостроенная» — впустую, когда всё построено. Нашли пусто — следующий перебор через 8 кадров
  // (клетка догружается — сетка начнёт строиться не позже чем через 8 кадров). CG_SCAN_OLD=1 — перебор каждый кадр
  // №924 (цена камеры в езде, тур TEST58 F_L4_CR, F_L7_CR: перебор клеток раз в 8 кадров в покое — 120 мкс на Маке, 27 мс на 540 кадров): клетка загрузилась — загрузчик (main_stream.h) двигает cgDirty,
  // в покое перебор только после этого (или раз в 64 кадра на всякий случай); стройка новой клетки начинается не позже, чем раньше. CG_DIRTY_OLD=1 / flag cgdirtyold 1 — раз в 8 кадров
  static int cgIdle; if (!forced && !CG_SCAN_OLD && cgIdle && (cgDirtyOld ? (cgFrame & 7u) : (cgDirty == cgDirtySeen && (cgFrame & 63u)))) { cgIdleSkip++; return; }
  cgDirtySeen = cgDirty;
  unsigned tA_ = cgProf ? sceKernelGetSystemTimeLow() : 0; Cell *c = 0; float bd = 1e30f;
  // №924: пока идёт стройка, недостроенные клетки — короткий список (≤ CG_PEND_N), полный перебор 1100+ клеток раз в 16 кадров и при каждой загрузке клетки (cgDirty); выбор ближайшей из списка — то же правило, что у полного перебора.
  // Список переполнен (старт уровня) — полный перебор каждый кадр, как было. CG_PEND_OLD=1 / flag cgpendold 1 — полный перебор каждый вызов
  unsigned dirty_ = cgDirty; int fast_ = !cgPendOld && !CG_SCAN_OLD && cgPendN >= 0 && cgPendSeen == dirty_ && cgPendGen == cgGen && cgPendNc == ncell && cgPendCl == (const void *)cl && (cgFrame & 15u);
  cgPendK = -1;
  if (forced) { c = forced; }
  else if (fast_) { cgPendHit++;   // список — только недостроенные клетки (достроенная вычёркивается), центры в плотных массивах: цикл не трогает клетки
    for (int k = 0; k < cgPendN; k++) {
      float dx = cgPendX[k] - ex, dz = cgPendZ[k] - ez, d = dx * dx + dz * dz;
      if (cgAhX > -1e8f) { float ax = cgPendX[k] - cgAhX, az = cgPendZ[k] - cgAhZ, da = ax * ax + az * az + 900.f; if (da < d) d = da; }
#if CG_PEND_RED
      d = 1e9f - d;
#endif
      if (d < bd) { bd = d; cgPendK = k; } }
    if (cgPendK >= 0) { c = &cl[cgPend[cgPendK]]; if (c->state != 2 || c->cgs == 2) { cgPendN = -1; cgPendK = -1; c = 0; cgPendBad++; return; } }   // страховка: список разошёлся с клетками — полный перебор в следующий вызов
  } else {
    int np_ = 0;
    for (unsigned i = 0; i < ncell; i++) { Cell *q = &cl[i]; if (q->state != 2 || q->cgs == 2) continue;
      if (np_ < CG_PEND_N) { cgPend[np_] = (unsigned short)i; cgPendX[np_] = q->cx; cgPendZ[np_] = q->cz; } np_++;
      float dx = q->cx - ex, dz = q->cz - ez, d = dx * dx + dz * dz;
      if (cgAhX > -1e8f) { float ax = q->cx - cgAhX, az = q->cz - cgAhZ, da = ax * ax + az * az + 900.f; if (da < d) d = da; }   // клетки по ходу глаза — тоже в очередь (поправка 30 м: клетка под глазом строится первой)
      if (d < bd) { bd = d; c = q; cgPendK = np_ - 1; } }
    cgPendN = np_ <= CG_PEND_N ? np_ : -1; cgPendSeen = dirty_; cgPendGen = cgGen; cgPendNc = ncell; cgPendCl = cl; cgPendFull++; if (np_ > CG_PEND_N) { cgPendOver++; cgPendK = -1; } if ((unsigned)np_ > cgPendMax) cgPendMax = (unsigned)np_;
  }
  if (cgProf) cgPf[0] += sceKernelGetSystemTimeLow() - tA_;
  if (!c) { cgIdle = 1; return; }
  if (smkOn && camDet) { char l_[64]; snprintf(l_, sizeof l_, "CGSEL f=%u i=%d\n", cgFrame, (int)(c - cl)); smkWrite(l_); }   // служебное (гейт camf924): какую клетку строит этот кадр, кадр в кадр при camdet 1
  cgIdle = 0; unsigned tB_ = cgProf ? sceKernelGetSystemTimeLow() : 0;
  if (c->cgs == 0) {
    int nx = (int)ceilf((c->bb[3] - c->bb[0]) / CG_VX) + 1, ny = (int)ceilf((c->bb[4] - c->bb[1]) / CG_VX) + 1, nz = (int)ceilf((c->bb[5] - c->bb[2]) / CG_VX) + 1;
    if (nx > CG_MAXN || ny > CG_MAXN || nz > CG_MAXN || nx < 1 || ny < 1 || nz < 1) { c->cgs = 2; c->cg = 0; cgPendDrop(); return; }
    size_t bytes = ((size_t)nx * ny * nz + 7) >> 3; { unsigned char n_[3] = {(unsigned char)nx, (unsigned char)ny, (unsigned char)nz}; bytes += cgCoarseBytes(n_); } unsigned char *g = calloc(bytes, 1);   // CAMPERF: + грубая сетка блоков if (!g) { c->cgs = 2; return; }
    c->cgn[0] = (unsigned char)nx; c->cgn[1] = (unsigned char)ny; c->cgn[2] = (unsigned char)nz; c->cgbytes = (unsigned)bytes; c->cgk = c->first; c->cgi = 0; c->cg = g; c->cgs = 1;
  }
  if (cgProf) cgPf[1] += sceKernelGetSystemTimeLow() - tB_;
  cgWork = 0; int budget = 1 << 30, tick = 0; unsigned t0 = sceKernelGetSystemTimeLow();
  while (c->cgk < c->first + c->n && budget > 0) {
    const Piece *d = &pc[c->cgk]; const Rec *r = &d->r;
    if (r->owner != 0 || (r->flags & 4) || !d->v || r->ni < 3 || r->sc <= 0.f || (!(r->flags & F_NOIDX) && !d->idx)) { c->cgk++; c->cgi = 0; continue; }
    int v12 = (r->flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8, noidx = (r->flags & F_NOIDX) != 0, n = r->ni - r->ni % 3;
    const char *vbase = (const char *)d->v;
    if (r->flags & F_STRIP) { unsigned nl_; vbase = (const char *)pieceListT(d, 0, &nl_); if (!vbase) { c->cgk++; c->cgi = 0; continue; } n = (int)(nl_ - nl_ % 3); }   // К4: полосы -> список (слот 0)
    while (c->cgi < n && budget > 0) {
      int i = c->cgi; c->cgi += 3;
      if (!forced && (cgWork > (cgBudgetOld ? 96 : 32) || (camDet ? ++tick > CG_DET_TRIS : ((++tick & (cgBudgetOld ? 1 : 0)) == 0 && sceKernelGetSystemTimeLow() - t0 > (cgBudgetOld ? CG_US : CG_US2))))) budget = 0;   // крупный треугольник = много вокселей: ещё и по числу меток
      float w[3][3];
      for (int j = 0; j < 3; j++) {
        unsigned vi = noidx ? (unsigned)(i + j) : d->idx[i + j];
        const short *xy = (const short *)(vbase + vi * vs + xo);
        w[j][0] = r->ox + (r->sc * (1.f / 32768.f)) * xy[0]; w[j][1] = r->oy + (r->sc * (1.f / 32768.f)) * xy[1]; w[j][2] = r->oz + (r->sc * (1.f / 32768.f)) * xy[2];
      }
      float e1[3] = {w[1][0] - w[0][0], w[1][1] - w[0][1], w[1][2] - w[0][2]}, e2[3] = {w[2][0] - w[0][0], w[2][1] - w[0][1], w[2][2] - w[0][2]};
      float nxv = e1[1] * e2[2] - e1[2] * e2[1], nyv = e1[2] * e2[0] - e1[0] * e2[2], nzv = e1[0] * e2[1] - e1[1] * e2[0], nl = sqrtf(nxv * nxv + nyv * nyv + nzv * nzv);
      cgTotTri++; if (cgDbg < 3 && i == 0) { cgDbg++; memcpy(cgS, w[0], 12); memcpy(cgS + 3, c->bb, 24); cgS[9] = r->ox; cgS[10] = r->sc; cgS[11] = (float)c->n; }
      if (nl < 1e-6f || fabsf(nyv) / nl > CG_NY) { cgSkipNy++; continue; }
      cgTri(c, w[0], w[1], w[2], 0);
    }
    if (c->cgi >= n) { c->cgk++; c->cgi = 0; }
  }
  if (c->cgk >= c->first + c->n) { c->cgs = 2; cgDone++; cgVox += c->cgbytes; cgPendDrop(); }   // gvox = байт сеток (без подсчёта занятых: перебор 100+ КБ стоил мс)
}
// №1141: достроить сразу, целиком, сетки клеток, чей ящик ближе R м к точке a или b (не больше maxCells за вызов). Зовётся после переноса опоры / загрузки уровня, окном в несколько кадров
static float cgBoxDist(const Cell *c, const float p[3]) {
  float d[3]; for (int k = 0; k < 3; k++) d[k] = fmaxf(fmaxf(c->bb[k] - p[k], p[k] - c->bb[k + 3]), 0.f);
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
// №1159 (тур TEST85: город +0,2 мс в эмуляторе после каждого переноса опоры, окно CAM_START_WIN вызовов): перебор клеток считал корень (расстояние до ящика) дважды на каждую недостроенную клетку уровня — ~1000 клеток × 120 вызовов.
// Теперь сравниваются квадраты (тот же результат без корня). STARTCAM1159_OLD=1 — как в TEST85
#ifndef STARTCAM1159_OLD
#define STARTCAM1159_OLD 0
#endif
static float cgBoxD2(const Cell *c, const float p[3]) {
  float d[3]; for (int k = 0; k < 3; k++) d[k] = fmaxf(fmaxf(c->bb[k] - p[k], p[k] - c->bb[k + 3]), 0.f);
  return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
}
static int cgBuildNear(const float a[3], const float b[3], float R, int maxCells) {
  if (!CAM_GEOM_FAST || !cl) return 0;
  int n = 0; const float R2 = R * R;
  for (unsigned i = 0; i < ncell && n < maxCells; i++) { Cell *q = &cl[i]; if (q->state != 2 || q->cgs == 2) continue;
    if (STARTCAM1159_OLD ? (cgBoxDist(q, a) > R && cgBoxDist(q, b) > R) : (cgBoxD2(q, a) > R2 && cgBoxD2(q, b) > R2)) continue;
    cgForceCell = q; cgStep2(a[0], a[2]); n++; }
  cgForceCell = 0; return n;
}
static inline int cgOcc(const Cell *c, const float p[3]) {
  int ix = (int)((p[0] - c->bb[0]) / CG_VX), iy = (int)((p[1] - c->bb[1]) / CG_VX), iz = (int)((p[2] - c->bb[2]) / CG_VX);
  if (ix < 0 || iy < 0 || iz < 0 || ix >= c->cgn[0] || iy >= c->cgn[1] || iz >= c->cgn[2]) return 0;
  int i = cgIdx(c, ix, iy, iz); return (((const unsigned char *)c->cg)[i >> 3] >> (i & 7)) & 1;
}
// доля пути a -> b до первого занятого вокселя (шаг 0,5 м; первые CG_SKIP м от опоры не в счёт: над крышей машины, под навесом)
#define CG_SKIP .8f
static unsigned cgCalls, cgHits, cgCellsOcc;
// Кэш (№620, «проверять при сдвиге глаза > 0,2 м или раз в 2 кадра»): тот же луч (оба конца сдвинулись меньше CG_REUSE м) в следующем кадре берёт прошлый
// ответ, не позже чем через кадр пересчитывается. CAM_GEOM_CACHE_OLD=1 — каждый кадр
#define CG_REUSE .2f
#ifndef CAM_GEOM_CACHE_OLD
#define CAM_GEOM_CACHE_OLD 0
#endif
static int cgCacheOld = CAM_GEOM_CACHE_OLD;
#if 0
#endif

// Рывок глаза у двора Симпсонов (L1, x 215, z 187; наряд после №916): плоская панель без коллизии (6 × 4 м), сквозь которую Гомер проходит, оказывалась между ним и глазом — глаз
// «за стеной», камера прыгала на 7 м к голове Гомера (и держалась у затылка 1,5 с). Поверхность, через которую опора (Гомер, машина) прошла без коллизии, камера может пройти следом:
// камера помнит до CG_CROSS_N таких мест (точка первого занятого вокселя на луче «опора → глаз» в кадре, когда в прошлом кадре луч был свободен, а коллизия и тела по-прежнему не мешают),
// воксели в CG_CROSS_R от них луч «опора → глаз» не считает занятыми. Лучи ближней плоскости видят всё (ничего не режется у края кадра). CAM_CROSS_OLD=1 / flag camcrossold 1 — как было
#ifndef CAM_CROSS_OLD
#define CAM_CROSS_OLD 0
#endif
#define CG_CROSS_N 4
#define CG_CROSS_R 1.2f
static int camCrossOld = CAM_CROSS_OLD, cgCrossN, cgCrossOn, cgCrossNew; static float cgCross[CG_CROSS_N][3];
static inline int cgCrossHit(const float p[3]) { for (int i = 0; i < cgCrossN; i++) { float dx = p[0] - cgCross[i][0], dy = p[1] - cgCross[i][1], dz = p[2] - cgCross[i][2]; if (dx * dx + dy * dy + dz * dz < CG_CROSS_R * CG_CROSS_R) return 1; } return 0; }
static struct { float a[3], b[3], r; unsigned fr; int cross; } cgCache[8]; static int cgCacheN;
static float cgRayFrac2(const float a[3], const float b[3], float skip);
static float cgRayFrac(const float a[3], const float b[3], float skip) {
  unsigned t0 = CG_TIME(); float rr;
  if (cgCacheOld) { rr = cgRayFrac2(a, b, skip); if (CG_PROF) cgUsRay += sceKernelGetSystemTimeLow() - t0; return rr; }
  for (int i = 0; i < 8; i++) { if (cgCache[i].fr == 0 || cgFrame - cgCache[i].fr != 1 || cgCache[i].cross != cgCrossOn) continue;
    float da = 0.f, db = 0.f; for (int k = 0; k < 3; k++) { float x = a[k] - cgCache[i].a[k], y = b[k] - cgCache[i].b[k]; da += x * x; db += y * y; }
    if (da < CG_REUSE * CG_REUSE && db < CG_REUSE * CG_REUSE) { cgReuse++; if (CG_PROF) cgUsRay += sceKernelGetSystemTimeLow() - t0; return cgCache[i].r; } }
  float r = cgRayFrac2(a, b, skip);
  int j = cgCacheN++ & 7; memcpy(cgCache[j].a, a, 12); memcpy(cgCache[j].b, b, 12); cgCache[j].r = r; cgCache[j].fr = cgFrame; cgCache[j].cross = cgCrossOn;
  if (CG_PROF) cgUsRay += sceKernelGetSystemTimeLow() - t0;
  return r;
}
static float cgRayFrac2(const float a[3], const float b[3], float skip) {
  float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (L < 1e-3f) return 1.f;
  float lo[3], hi[3]; for (int k = 0; k < 3; k++) { lo[k] = fminf(a[k], b[k]); hi[k] = fmaxf(a[k], b[k]); }
  // список клеток у луча — из кэша, пока луч внутри коробки кэша (± CG_CL_PAD м) и не прошло 8 кадров: перебор всех клеток на каждый из ~15 лучей кадра стоил больше самого шага
  static const Cell *cc[8]; static int ccN; static float ccLo[3], ccHi[3]; static unsigned ccFr = 1u << 31, ccNcell, ccGen, ccDone;   // ccDone — №1141: сколько сеток было достроено при сборе списка
  const Cell **near = cc; int nn;
  if (cgFrame - ccFr < 8u && ccNcell == ncell && ccGen == cgGen && (STARTCAM1141_OLD || ccDone == cgDone) && lo[0] >= ccLo[0] && lo[1] >= ccLo[1] && lo[2] >= ccLo[2] && hi[0] <= ccHi[0] && hi[1] <= ccHi[1] && hi[2] <= ccHi[2]) nn = ccN;
  else {
    nn = 0; for (int k = 0; k < 3; k++) { ccLo[k] = lo[k] - 4.f; ccHi[k] = hi[k] + 4.f; }
    for (unsigned i = 0; i < ncell && nn < 8; i++) { const Cell *c = &cl[i];
      if (c->state == 2 && c->cg && c->cgs == 2 && c->bb[0] <= ccHi[0] && c->bb[3] >= ccLo[0] && c->bb[1] <= ccHi[1] && c->bb[4] >= ccLo[1] && c->bb[2] <= ccHi[2] && c->bb[5] >= ccLo[2]) cc[nn++] = c; }
    ccN = nn; ccFr = cgFrame; ccNcell = ncell; ccGen = cgGen; ccDone = cgDone; }
  cgCalls++;
  if (!nn) return 1.f;
  int may[8]; for (int i = 0; i < 8; i++) may[i] = 1;
  if (!cgCoarseOld) {   // CAMPERF: клетки, в блоках которых (на коробке луча ± 1 мм) нет ни одного занятого вокселя, шагов не стоят
    float plo[3] = {lo[0] - 1e-3f, lo[1] - 1e-3f, lo[2] - 1e-3f}, phi[3] = {hi[0] + 1e-3f, hi[1] + 1e-3f, hi[2] + 1e-3f}; int any = 0;
    for (int i = 0; i < nn; i++) { may[i] = near[i]->cg ? cgCoarseHit(near[i], plo, phi) : 0; any |= may[i]; }
    if (!any) return 1.f;
  }
  for (float t = skip; t < L; t += .35f) {
    float p[3] = {a[0] + d[0] * (t / L), a[1] + d[1] * (t / L), a[2] + d[2] * (t / L)};
    for (int i = 0; i < nn; i++) if (may[i] && near[i]->cg && cgOcc(near[i], p) && !(cgCrossOn && cgCrossHit(p))) { cgHits++; return t / L; }
  }
  return 1.f;
}
// Вход камеры игры (csScene.geomRay): луч «опора → глаз» (skip > 0) обходит места, пройденные опорой; лучи ближней плоскости (skip = 0) — нет
static float cgRayFracCam(const float a[3], const float b[3], float skip) { cgCrossOn = skip > 0.f && cgCrossN > 0 && !camCrossOld; if (cgCrossNew) { memset(cgCache, 0, sizeof cgCache); cgCrossNew = 0; } float r = cgRayFrac(a, b, skip); cgCrossOn = 0; return r; }
#endif
