// Пешая навигация для ночных ботов (g04-homer, 04.11.2026): второй, независимый «пешеход» поверх настоящей коллизии уровня.
// Дополняет tools/bots/footpath.h (первый пешеход: сетка 0,5 м, A*, без прыжков, область ±40 м). Не трогает код игры (viewer/).
//
//   пешеход A  — footpath.h fpPlan как есть (сетка 0,5 м, тело Гомера, заборы, своя машина как препятствие, без прыжков)
//   пешеход B  — свой поиск по другим правилам: сетка 0,35 м (иные ячейки: проходит в щели и между столбами, где A мажет), тело 0,85 радиуса,
//                шаг вверх до FN_JUMP_UP (прыжок на уступ/ступень 0,6…1,6 м, как пешком выше HOMER_STEP), область ±FN_R (больше), опор 4 на клетку
//   fnReach    — A, затем B; результат с кодом причины и «кодом места» (уровень, квадрат 10 м, высота) для утреннего списка
//
//   int fnReach(cw, car, sx,sy,sz, tx,ty,tz, reach, FnResult *r, FpPt *path, cap) -> 1 путь есть (r->who 'A' или 'B'), 0 — нет, причина в r->why
//   const char *fnWhyName(int why)   текст причины
//   void fnPlaceCode(level, x, y, z, char out[40])   «L2_X-170_Z-020_Y007» — квадрат 10 м и высота в метрах
// Коды причин: FN_NO_PATH — ни один пешеход пути не нашёл; FN_START_BLOCKED — старт внутри твёрдого; FN_TOO_FAR — дальше области поиска;
//   FN_TARGET_HIGH — цель выше достижимого прыжка от любой опоры рядом (нужен подъёмник/лестница ANM)
#ifndef BOTS_FOOTNAV_H
#define BOTS_FOOTNAV_H
#include "footpath.h"

#define FN_CELL 0.35f
#define FN_R 70.f                  // м: полуразмер сетки B
#define FN_N 410                   // клеток: 2 * FN_R / FN_CELL + запас
#define FN_LAYERS 4
#define FN_JUMP_UP 1.6f            // м: шаг вверх прыжком (вершина прыжка Гомера 1,88 м, берём с запасом)
#define FN_JUMP_GAP 1.2f           // м по горизонтали для прыжка
#define FN_BODY_SCALE 0.85f

enum { FN_OK = 0, FN_NO_PATH, FN_START_BLOCKED, FN_TOO_FAR, FN_TARGET_HIGH };
typedef struct { int why; char who; float nearest; char place[40]; } FnResult;   // nearest — ближайшее расстояние до цели, до которого дошёл поиск

static const char *fnWhyName(int why) {
  static const char *n[] = {"дошёл", "пути нет", "старт в твёрдом", "цель вне области", "цель выше досягаемого"};
  return why >= 0 && why <= FN_TARGET_HIGH ? n[why] : "?";
}
static void fnPlaceCode(int level, float x, float y, float z, char out[40]) {
  snprintf(out, 40, "L%d_X%+04d_Z%+04d_Y%+04d", level, (int)floorf(x / 10.f) * 10, (int)floorf(z / 10.f) * 10, (int)floorf(y));
}

static int fnBody(const ColWorld *cw, float x, float y, float z) {   // тело (меньше радиус) помещается
  CarBox q; memset(&q, 0, sizeof q); q.x = x; q.z = z; q.fx = 0.f; q.fz = 1.f; q.hw = q.hl = (HOMER_RADIUS + .05f) * FN_BODY_SCALE; q.y = y; q.lo = HOMER_STEP; q.hi = HOMER_HEIGHT;
  colProbeCar(cw, &q); return q.maxDepth <= .05f;
}
// шаг из (ax,ay,az) в (bx,bz): опора под ногами, шаг вверх до up (обычный — HOMER_STEP, прыжок — FN_JUMP_UP), спуск до FP_DROP, забор, тело
static int fnStep(const ColWorld *cw, const Car *car, float ax, float ay, float az, float bx, float bz, float up, float *by) {
  float g;
  if (!colGround(cw, bx, bz, ay + up, ay - FP_DROP, &g, 0)) return 0;
  if (colCrossFence(cw, ax, az, bx, bz, 0, 0) || colCrossFence(cw, bx, bz, ax, az, 0, 0)) return 0;
  if (!fnBody(cw, bx, g, bz) || fpInCar(car, bx, g, bz)) return 0;
  *by = g; return 1;
}
typedef struct { float y[FN_LAYERS]; float gc[FN_LAYERS]; int par[FN_LAYERS]; unsigned char n, closed; } FnCell;
static FnCell *fnGrid; static FpHeapE *fnHeap; static int fnHeapN, fnHeapCap;
static void fnPush(int s, float k) {
  if (fnHeapN >= fnHeapCap) return;
  int i = fnHeapN++;
  while (i > 0) { int p = (i - 1) / 2; if (fnHeap[p].k <= k) break; fnHeap[i] = fnHeap[p]; i = p; }
  fnHeap[i].k = k; fnHeap[i].s = s;
}
static int fnPop(void) {
  int top = fnHeap[0].s; FpHeapE e = fnHeap[--fnHeapN]; int i = 0;
  for (;;) { int c = 2 * i + 1; if (c >= fnHeapN) break; if (c + 1 < fnHeapN && fnHeap[c + 1].k < fnHeap[c].k) c++;
    if (fnHeap[c].k >= e.k) break; fnHeap[i] = fnHeap[c]; i = c; }
  if (fnHeapN) fnHeap[i] = e;
  return top;
}
// пешеход B. Возвращает число точек пути, 0 — нет; *nearest — ближайшее расстояние до цели среди раскрытых клеток
static int fnPlanB(const ColWorld *cw, const Car *car, float sx, float sy, float sz, float tx, float ty, float tz, float reach, FpPt *out, int cap, float *nearest) {
  if (!fnGrid) { fnGrid = malloc(sizeof(FnCell) * FN_N * FN_N); fnHeapCap = FN_N * FN_N * FN_LAYERS * 3; fnHeap = malloc(sizeof(FpHeapE) * (size_t)fnHeapCap); }
  *nearest = hypotf(tx - sx, tz - sz);
  if (*nearest > 2.f * FN_R - 4.f) return 0;
  float ox = .5f * (sx + tx) - FN_R, oz = .5f * (sz + tz) - FN_R;
  memset(fnGrid, 0, sizeof(FnCell) * FN_N * FN_N); fnHeapN = 0;
  int ci = (int)((sx - ox) / FN_CELL), cj = (int)((sz - oz) / FN_CELL);
  if (ci < 0 || cj < 0 || ci >= FN_N || cj >= FN_N) return 0;
  FnCell *c0 = &fnGrid[cj * FN_N + ci]; c0->n = 1; c0->y[0] = sy; c0->gc[0] = 0.f; c0->par[0] = -1;
  int s0 = (cj * FN_N + ci) * FN_LAYERS; fnPush(s0, *nearest);
  static const int DI[16] = {1, -1, 0, 0, 1, 1, -1, -1, 2, -2, 0, 0, 2, 2, -2, -2}, DJ[16] = {0, 0, 1, -1, 1, -1, 1, -1, 0, 0, 2, -2, 1, -1, 1, -1};
  int goal = -1, iters = 0;
  while (fnHeapN && iters < FN_N * FN_N * 3) {
    int s = fnPop(), cell = s / FN_LAYERS, L = s % FN_LAYERS, i = cell % FN_N, j = cell / FN_N; iters++;
    FnCell *C = &fnGrid[cell]; if (C->closed & (1 << L)) continue; C->closed |= (unsigned char)(1 << L);
    float x = i == ci && j == cj && L == 0 ? sx : ox + (i + .5f) * FN_CELL, z = i == ci && j == cj && L == 0 ? sz : oz + (j + .5f) * FN_CELL, y = C->y[L];
    float dt = hypotf(x - tx, z - tz); if (dt < *nearest) *nearest = dt;
    if (dt < reach && (isnan(ty) || fabsf(y - ty) < 2.f)) { goal = s; break; }
    for (int k = 0; k < 8; k++) {   // обычные шаги по 8 направлениям, вверх до ступеньки
      int ni = i + DI[k], nj = j + DJ[k]; if (ni < 0 || nj < 0 || ni >= FN_N || nj >= FN_N) continue;
      float nx = ox + (ni + .5f) * FN_CELL, nz = oz + (nj + .5f) * FN_CELL, ny, my;
      if (!fnStep(cw, car, x, y, z, .5f * (x + nx), .5f * (z + nz), HOMER_STEP, &my) || !fnStep(cw, car, .5f * (x + nx), my, .5f * (z + nz), nx, nz, HOMER_STEP, &ny)) continue;
      FnCell *N = &fnGrid[nj * FN_N + ni]; int nl = -1;
      for (int q = 0; q < N->n; q++) if (fabsf(N->y[q] - ny) < .4f) { nl = q; break; }
      if (nl < 0) { if (N->n >= FN_LAYERS) continue; nl = N->n++; N->y[nl] = ny; N->gc[nl] = 1e30f; }
      if (N->closed & (1 << nl)) continue;
      float g = C->gc[L] + (k < 4 ? 1.f : 1.41421356f) * FN_CELL + fmaxf(0.f, ny - y) * .5f;
      if (g < N->gc[nl]) { N->gc[nl] = g; N->par[nl] = s; fnPush((nj * FN_N + ni) * FN_LAYERS + nl, g + hypotf(tx - nx, tz - nz)); }
    }
    for (int k = 8; k < 16; k++) {   // прыжок на уступ: на 2 клетки (0,7 м) вперёд, вверх до FN_JUMP_UP; только если обычный шаг туда не проходит по высоте
      int ni = i + DI[k], nj = j + DJ[k]; if (ni < 0 || nj < 0 || ni >= FN_N || nj >= FN_N) continue;
      float nx = ox + (ni + .5f) * FN_CELL, nz = oz + (nj + .5f) * FN_CELL, ny;
      if (hypotf(nx - x, nz - z) > FN_JUMP_GAP) continue;
      if (!fnStep(cw, car, x, y, z, nx, nz, FN_JUMP_UP, &ny) || ny - y <= HOMER_STEP + .02f) continue;
      FnCell *N = &fnGrid[nj * FN_N + ni]; int nl = -1;
      for (int q = 0; q < N->n; q++) if (fabsf(N->y[q] - ny) < .4f) { nl = q; break; }
      if (nl < 0) { if (N->n >= FN_LAYERS) continue; nl = N->n++; N->y[nl] = ny; N->gc[nl] = 1e30f; }
      if (N->closed & (1 << nl)) continue;
      float g = C->gc[L] + 2.f + (ny - y) * 1.5f;   // прыжок дорог, чтобы идти по ступеням и тротуару, где можно
      if (g < N->gc[nl]) { N->gc[nl] = g; N->par[nl] = s; fnPush((nj * FN_N + ni) * FN_LAYERS + nl, g + hypotf(tx - nx, tz - nz)); }
    }
    // прыжок через тонкую преграду (штакетник, низкая стенка до 1,5 м, шириной до 1,7 м): 4 направления на 3…5 клеток, поверхность на пути не выше y+1,55
    // (в L7 забор двора Неда: стена 0,5 м, высотой 1,5 м, бегущий Гомер перелетает её, замерено на физике и в эмуляторе); приземление на тот же уровень ±ступенька
    for (int k = 0; k < 4; k++) for (int w = 3; w <= 5; w++) {
      int ni = i + DI[k] * w, nj = j + DJ[k] * w; if (ni < 0 || nj < 0 || ni >= FN_N || nj >= FN_N) continue;
      float nx = ox + (ni + .5f) * FN_CELL, nz = oz + (nj + .5f) * FN_CELL, ny, gt; int okh = 1;
      for (int q = 1; q < w && okh; q++) { float mx = x + (nx - x) * q / w, mz = z + (nz - z) * q / w;
        if (!colGround(cw, mx, mz, y + 4.f, y - 2.f, &gt, 0) || gt > y + 1.55f) okh = 0; }
      if (!okh) continue;
      float g0; if (!colGround(cw, nx, nz, y + HOMER_STEP, y - FP_DROP, &g0, 0)) continue;
      if (!fnBody(cw, nx, g0, nz) || fpInCar(car, nx, g0, nz)) continue; ny = g0;
      FnCell *N = &fnGrid[nj * FN_N + ni]; int nl = -1;
      for (int q = 0; q < N->n; q++) if (fabsf(N->y[q] - ny) < .4f) { nl = q; break; }
      if (nl < 0) { if (N->n >= FN_LAYERS) continue; nl = N->n++; N->y[nl] = ny; N->gc[nl] = 1e30f; }
      if (N->closed & (1 << nl)) continue;
      float g = C->gc[L] + 6.f + w * FN_CELL;   // самый дорогой ход: сначала обычные пути
      if (g < N->gc[nl]) { N->gc[nl] = g; N->par[nl] = s; fnPush((nj * FN_N + ni) * FN_LAYERS + nl, g + hypotf(tx - nx, tz - nz)); }
    }
  }
  if (goal < 0) return 0;
  int n = 0; for (int s = goal; s >= 0 && s != s0; s = fnGrid[s / FN_LAYERS].par[s % FN_LAYERS]) n++;
  if (n > cap) n = cap;
  int k = n; for (int s = goal; s >= 0 && s != s0 && k > 0; s = fnGrid[s / FN_LAYERS].par[s % FN_LAYERS]) {
    int cell = s / FN_LAYERS; k--; out[k] = (FpPt){ox + (cell % FN_N + .5f) * FN_CELL, fnGrid[cell].y[s % FN_LAYERS], oz + (cell / FN_N + .5f) * FN_CELL}; }
  return n;
}
static int fnReachM(const ColWorld *cw, const Car *car, int level, float sx, float sy, float sz, float tx, float ty, float tz, float reach, FnResult *r, FpPt *path, int cap, int bOnly) {
  memset(r, 0, sizeof *r); r->nearest = hypotf(tx - sx, tz - sz); fnPlaceCode(level, tx, ty, tz, r->place);
  if (!fnBody(cw, sx, sy, sz) && !fpBody(cw, sx, sy, sz)) { r->why = FN_START_BLOCKED; return 0; }
  if (r->nearest > 2.f * FN_R - 4.f) { r->why = FN_TOO_FAR; return 0; }
  int n = bOnly ? 0 : fpPlan(cw, car, sx, sy, sz, tx, ty, tz, reach, path, cap);
  if (n > 0) { r->who = 'A'; r->why = FN_OK; r->nearest = 0.f; return n; }
  float near_ = 0.f; n = fnPlanB(cw, car, sx, sy, sz, tx, ty, tz, reach, path, cap, &near_);
  r->nearest = near_;
  if (n > 0) { r->who = 'B'; r->why = FN_OK; r->nearest = 0.f; return n; }
  r->why = (!isnan(ty) && ty - sy > FN_JUMP_UP + 2.f) ? FN_TARGET_HIGH : FN_NO_PATH;
  return 0;
}
static int fnReach(const ColWorld *cw, const Car *car, int level, float sx, float sy, float sz, float tx, float ty, float tz, float reach, FnResult *r, FpPt *path, int cap) {
  return fnReachM(cw, car, level, sx, sy, sz, tx, ty, tz, reach, r, path, cap, 0);
}
// ---- далёкая цель (сотни метров): ломаная крупных опорных точек (у бота — navPlan по графу дорог, у теста — свой граф) режется на прыжки до 55 м;
// каждый прыжок строит пешеход A, а если не вышло — пешеход B; цель ближе 55 м берётся сразу. Опорные точки — P3-подобные {x,y,z}.
//   int fnLongPlan(cw, car, level, sx,sy,sz, tx,ty,tz, reach, const FpPt *coarse, nc, bOnly, FpPt *out, cap, FnResult *r) -> точек пути, 0 — застрял (r->why, r->place: где)
#define FN_HOP 55.f
static int fnLongPlan(const ColWorld *cw, const Car *car, int level, float sx, float sy, float sz, float tx, float ty, float tz, float reach, const FpPt *coarse0, int nc0, int bOnly, FpPt *out, int cap, FnResult *r) {
  static FpPt dense[4000]; int nc = 0; const FpPt *coarse = dense;   // опорные точки дорог редки (до 60 м между узлами): добавляем промежуточные каждые 8 м
  for (int i = 0; i < nc0 && nc < 3990; i++) {
    if (i) { float dx = coarse0[i].x - coarse0[i - 1].x, dz = coarse0[i].z - coarse0[i - 1].z, L = hypotf(dx, dz); int k = (int)(L / 8.f);
      for (int q = 1; q <= k && nc < 3990; q++) { float t = (float)q / (float)(k + 1); dense[nc++] = (FpPt){coarse0[i - 1].x + dx * t, coarse0[i - 1].y + (coarse0[i].y - coarse0[i - 1].y) * t, coarse0[i - 1].z + dz * t}; } }
    dense[nc++] = coarse0[i]; }
  static FpPt leg[700]; int n = 0, cur = -1; float px = sx, py = sy, pz = sz; FnResult lr;
  memset(r, 0, sizeof *r);
  for (int it = 0; it < 60; it++) {
    float dT = hypotf(tx - px, tz - pz);
    if (dT < FN_HOP + 20.f || cur >= nc - 1) {   // цель в пределах прыжка: последний отрезок
      int m = fnReachM(cw, car, level, px, py, pz, tx, ty, tz, reach, &lr, leg, 700, bOnly);
      if (m <= 0) { r->why = lr.why; r->who = 0; r->nearest = lr.nearest; fnPlaceCode(level, px, py, pz, r->place); return 0; }
      if (n + m > cap) { r->why = FN_NO_PATH; fnPlaceCode(level, px, py, pz, r->place); return 0; }
      memcpy(out + n, leg, sizeof(FpPt) * (size_t)m); n += m; r->who = lr.who ? lr.who : r->who; r->why = FN_OK; fnPlaceCode(level, tx, ty, tz, r->place); return n; }
    if (cur < 0) { float bd = 1e9f; for (int j = 0; j < nc; j++) { float d = hypotf(coarse[j].x - px, coarse[j].z - pz); if (d < bd) { bd = d; cur = j; } } }
    int moved = 0;
    for (int j = nc - 1; j > cur; j--) {   // самая дальняя опорная точка в пределах прыжка, до которой пешком проходят
      float d = hypotf(coarse[j].x - px, coarse[j].z - pz); if (d > FN_HOP || d < 3.f) continue;
      int m = fnReachM(cw, car, level, px, py, pz, coarse[j].x, coarse[j].y, coarse[j].z, 1.5f, &lr, leg, 700, bOnly);
      if (m <= 0) continue;
      if (n + m > cap) break;
      memcpy(out + n, leg, sizeof(FpPt) * (size_t)m); n += m; px = leg[m - 1].x; py = leg[m - 1].y; pz = leg[m - 1].z; cur = j; moved = 1; if (lr.who == 'B') r->who = 'B'; else if (!r->who) r->who = 'A'; break; }
    if (!moved) { r->why = FN_NO_PATH; r->nearest = dT; fnPlaceCode(level, px, py, pz, r->place); return 0; }
  }
  r->why = FN_NO_PATH; fnPlaceCode(level, px, py, pz, r->place); return 0;
}
// ---- ходок: идёт по пути на настоящей физике Гомера (homerUpdate), с лестницей выручки: упёрся → прыжок ✕ → шаг вбок → пересчёт пешеходом B → код места
//   int fnWalk(Homer *H, cw, car, level, tx,ty,tz, reach, path, n, secs, FnWalkOut *o) -> 1 дошёл, 0 застрял (o->code — место, jumps/sides/replans — какие ступени понадобились)
typedef struct { float x, y, z; char code[40]; int jumps, sides, replans; float t; } FnWalkOut;
static int fnWalkLeg(Homer *H, const ColWorld *cw, FpPt *path, int n, float tx, float tz, float reach, float *t, float limit, int jumpOK, FnWalkOut *o) {
  int seg = 0; float stillT = 0.f, fx = H->x, fz = H->z, sideT = 0.f, jumpT = 0.f; const float dt = 1.f / 60.f; int sideDir = 1;
  while (*t < limit) {
    if (hypotf(H->x - tx, H->z - tz) < reach) return 1;
    // круг поворота на бегу (~2 м): точка сбоку ближе радиуса разворота обходится по кругу, поэтому точка пути засчитывается с запасом
    // 1,3 м, а к точке, лежащей по ходу дальше в пределах 2 м, можно идти сразу; при большом угле к цели вблизи бег снимается (шагом круг мал)
    while (seg < n - 1 && hypotf(path[seg].x - H->x, path[seg].z - H->z) < 1.3f) seg++;
    for (int j = n - 1; j > seg && j <= seg + 8; j--) if (hypotf(path[j].x - H->x, path[j].z - H->z) < 2.0f) { seg = j; break; }
    float px = n ? path[seg].x : tx, pz = n ? path[seg].z : tz, a = atan2f(px - H->x, pz - H->z);
    if (sideT > 0.f) { a += sideDir * 1.5708f; sideT -= dt; }
    HomerInput in; memset(&in, 0, sizeof in); in.camYaw = 0.f; in.mx = -sinf(a); in.my = cosf(a); in.dash = 1;
    { float sp2 = hypotf(H->vx, H->vz), dd = hypotf(px - H->x, pz - H->z);
      if (sp2 > 1.f && dd < 6.f) { float c = (H->vx * (px - H->x) + H->vz * (pz - H->z)) / (sp2 * (dd + 1e-4f)); if (c < .75f) in.dash = 0; } }
    if (jumpT > 0.f) { in.jump = jumpT > .98f; jumpT -= dt; }
    homerUpdate(H, cw, NULL, &in, dt); homerPose(H); *t += dt;
    if (getenv("FN_TRACE") && ((int)(*t * 60.f + .5f)) % 120 == 0) fprintf(stderr, "  t=%.0f Гомер %.2f %.2f %.2f seg %d/%d -> %.1f %.1f state %d sp %.1f\n", *t, H->x, H->y, H->z, seg, n, px, pz, H->state, H->speed);
    if (hypotf(H->x - fx, H->z - fz) > .4f) { fx = H->x; fz = H->z; stillT = 0.f; }
    else if ((stillT += dt) > 1.0f) {   // упёрся: прыжок (до 3 раз), потом шаг вбок (до 4 раз), потом «не смог»
      if (getenv("FN_TRACE")) fprintf(stderr, "  упёрся t=%.1f jumps %d sides %d\n", *t, o->jumps, o->sides);
      if (jumpOK && o->jumps < 3) { o->jumps++; jumpT = 1.f; stillT = 0.f; }
      else if (o->sides < 4) { o->sides++; sideT = .8f; sideDir = -sideDir; stillT = 0.f; }
      else return 0; }
  }
  return 0;
}
static int fnWalk(Homer *H, const ColWorld *cw, const Car *car, int level, float tx, float ty, float tz, float reach, const FpPt *coarse, int nc, FpPt *path, int n, float secs, FnWalkOut *o) {
  memset(o, 0, sizeof *o); float t = 0.f;
  if (fnWalkLeg(H, cw, path, n, tx, tz, reach, &t, secs, 1, o)) { o->t = t; return 1; }
  static FpPt p2[3000]; FnResult r;   // путь не довёл: второй пешеход (только B) строит путь заново от места остановки
  o->replans++; int m = fnLongPlan(cw, car, level, H->x, H->y, H->z, tx, ty, tz, reach, coarse, nc, 1, p2, 3000, &r);
  if (m > 0 && fnWalkLeg(H, cw, p2, m, tx, tz, reach, &t, secs, 1, o)) { o->t = t; return 1; }
  o->x = H->x; o->y = H->y; o->z = H->z; o->t = t; fnPlaceCode(level, H->x, H->y, H->z, o->code);
  return 0;
}
#endif
