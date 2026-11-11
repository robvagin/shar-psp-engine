// Правка 1230 (Роб, L5 E3-5: Гомер бежит по косой вдоль перил, перила останавливают намертво; должен скользить).
//   rail_host LEVEL PLAYER --seg I | --box I | --auto-seg N | --auto-box N [-v] [--old]
// Стена = звено забора (ColSeg) или тонкий ящик COL1 (поручни лестниц: наклонные тонкие ящики). Гомер пешком в 3 м от стены (с обеих сторон),
// бежит на стену под углом 10/30/60/85° к её плоскости, стик вперёд. Строка R: стена, сторона, угол, кадр контакта, скорость вдоль стены после контакта
// (м/с) против идеальной v·cos(угол), прошёл ли сквозь. Красный/зелёный: --old = HOMER_SLIDE1230_OLD (флаг в viewer/homer.c).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "col.h"
extern int homerRunSlide1230;
static BotWorld W; static float gDist = 3.f; static unsigned gBtn = GP_CIRCLE; static float gV = 8.f;   // бег (○ зажат) — как у Роба; --walk: шагом
static int onFoot(void) { return gameOnFoot(&W.G); }
typedef struct { float px, pz, tx, tz, nx, nz, half, len, yRef, ylo, yhi; } Wall;   // плоскость стены: точка, касательная, нормаль в сторону Гомера, полутолщина, длина по t, высота ориентира
static int runOne(const Wall *S, float aDeg, int verbose, float *slideOut, int *crossed, float *ctlOut) {
  float sx = S->px + S->nx * (S->half + gDist) - S->tx * S->len * .3f, sz = S->pz + S->nz * (S->half + gDist) - S->tz * S->len * .3f;
  float g, n3[3];
  if (!colGround(bwCol(), sx, sz, S->yRef + 2.5f, S->yRef - 5.f, &g, n3) || n3[1] < .85f) return -2;
  memset(&W, 0, sizeof W); if (bwNew(&W)) return -3;
  bwPlace(&W, sx, g, sz, 0.f); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  for (int f = 0; f < 120 && !onFoot(); f++) bwStep(&W, (f % 20) == 0 ? GP_TRIANGLE : 0, 0, 0);
  if (!onFoot()) return -4;
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  W.homer.x = sx; W.homer.z = sz; W.homer.y = g; W.homer.vx = W.homer.vz = W.homer.vy = 0.f; W.homer.grounded = 1;
  for (int f = 0; f < 20; f++) bwStep(&W, 0, 0, 0);
  float a = aDeg * 3.14159265f / 180.f;
  float dx = S->tx * cosf(a) - S->nx * sinf(a), dz = S->tz * cosf(a) - S->nz * sinf(a);   // вдоль стены и на неё
  float r = atan2f(dx, dz) - W.homer.camYaw;
  W.homer.h = atan2f(dx, dz);   // лицо уже в сторону бега (иначе бег с ○ сначала поворачивает лицо со темпом 2/с — это не касание стены)
  float lastx = W.homer.x, lastz = W.homer.z; int contact = -1; float slideSum = 0.f; int slideN = 0;
  float ctlSum = 0.f; int ctlN = 0;
  float d0 = (W.homer.x - S->px) * S->nx + (W.homer.z - S->pz) * S->nz; *crossed = 0;
  const int NF = 210;
  for (int f = 0; f < NF; f++) {
    bwStep(&W, gBtn, -sinf(r), -cosf(r));
    float d = (W.homer.x - S->px) * S->nx + (W.homer.z - S->pz) * S->nz - S->half;
    float mvx = W.homer.x - lastx, mvz = W.homer.z - lastz; lastx = W.homer.x; lastz = W.homer.z;
    float along = (mvx * S->tx + mvz * S->tz) * 60.f, pos = (W.homer.x - S->px) * S->tx + (W.homer.z - S->pz) * S->tz;
    if (f >= 20 && f < 150) { ctlSum += along; ctlN++; }   // контроль коридора (угол 0): свободный бег вдоль стены
    if (contact < 0 && d < .5f) contact = f;
    if (contact >= 0 && f >= contact + 8 && f < contact + 38) { slideSum += along; slideN++; }
    { float tp = (W.homer.x - S->px) * S->tx + (W.homer.z - S->pz) * S->tz;   // сквозь стену — только в пределах её длины (обход вокруг торца не считается)
      if ((d + S->half) * d0 < 0.f && fabsf(d + S->half) > .2f && fabsf(tp) < S->len * .5f - 1.0f && f > 3) *crossed = 1; }
    if (verbose && f % 15 == 0) printf("  f%3d x %.2f z %.2f y %.2f d %.2f along %.2f m/s\n", f, W.homer.x, W.homer.z, W.homer.y, d, along);
  }
  *slideOut = slideN ? slideSum / slideN : 0.f; *ctlOut = ctlN ? ctlSum / ctlN : 0.f;
  return contact < 0 ? 9999 : contact;
}
static int segWall(const ColSeg *s, int side, Wall *o) {
  float ex = s->b[0] - s->a[0], ez = s->b[1] - s->a[1], L = hypotf(ex, ez); if (L < 6.f) return 0;
  o->tx = ex / L; o->tz = ez / L; o->nx = -o->tz * (side ? -1.f : 1.f); o->nz = o->tx * (side ? -1.f : 1.f);
  o->px = (s->a[0] + s->b[0]) * .5f; o->pz = (s->a[1] + s->b[1]) * .5f; o->half = 0.f; o->len = L;
  o->yRef = s->y1 < 1e5f ? s->y1 - 3.f : 0.f; o->ylo = s->y0; o->yhi = s->y1;
  return s->y1 < 1e5f;   // высоту ориентира для бесконечных не знаем (берётся ниже)
}
static int boxWall(const ColBox *b, int side, Wall *o) {   // тонкий ящик: длинная ось и самая тонкая; горизонтальная нормаль тонкой оси
  int lo = 0, hi = 0; for (int k = 1; k < 3; k++) { if (b->h[k] < b->h[lo]) lo = k; if (b->h[k] > b->h[hi]) hi = k; }
  const float *ax[3] = {b->ax, b->ay, b->az}; const float *T = ax[hi], *N = ax[lo];
  float tl = hypotf(T[0], T[2]), nl = hypotf(N[0], N[2]); if (tl < .5f || nl < .5f || b->h[hi] < 1.5f || b->h[lo] > .45f) return 0;
  o->tx = T[0] / tl; o->tz = T[2] / tl; o->nx = N[0] / nl * (side ? -1.f : 1.f); o->nz = N[2] / nl * (side ? -1.f : 1.f);
  if (fabsf(o->tx * o->nx + o->tz * o->nz) > .2f) return 0;
  o->px = b->c[0]; o->pz = b->c[2]; o->half = b->h[lo] / nl; o->len = 2.f * b->h[hi] * tl; o->yRef = b->c[1]; o->ylo = o->yhi = 0.f;
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: rail_host LEVEL PLAYER --seg I | --box I | --auto-seg N | --auto-box N [-v]\n"); return 2; }
  int idx = -1, isBox = 0, autoN = 0, verbose = 0;
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--seg") && i + 1 < argc) { idx = atoi(argv[++i]); isBox = 0; }
    else if (!strcmp(argv[i], "--box") && i + 1 < argc) { idx = atoi(argv[++i]); isBox = 1; }
    else if (!strcmp(argv[i], "--auto-seg") && i + 1 < argc) { autoN = atoi(argv[++i]); isBox = 0; }
    else if (!strcmp(argv[i], "--auto-box") && i + 1 < argc) { autoN = atoi(argv[++i]); isBox = 1; }
    else if (!strcmp(argv[i], "--dist") && i + 1 < argc) gDist = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--old")) homerRunSlide1230 = 0;   // красный контроль: как было до №1230
    else if (!strcmp(argv[i], "--walk")) { gBtn = 0; gV = 4.f; }
    else if (!strcmp(argv[i], "-v")) verbose = 1;
  }
  const ColWorld *w = bwCol(); const float ang[5] = {10.f, 30.f, 60.f, 85.f, 90.f};
  unsigned n = isBox ? w->nbox : w->nseg; int done = 0;
  for (unsigned i = 0; i < n; i++) {
    if (idx >= 0 && (int)i != idx) continue;
    if (idx < 0 && done >= autoN) break;
    int did = 0;
    for (int side = 0; side < 2; side++) {
      Wall S0; if (!(isBox ? boxWall(&w->box[i], side, &S0) : segWall(&w->seg[i], side, &S0))) { if (idx >= 0) printf("R %s %u: не подходит как стена\n", isBox ? "box" : "seg", i); goto next; }
      float sl0 = 0.f, ctl = 0.f; int cr0 = 0; int c0 = runOne(&S0, 0.f, 0, &sl0, &cr0, &ctl);
      if (c0 < 0) { if (idx >= 0) printf("R %s %u side %d: нет старта (%d)\n", isBox ? "box" : "seg", i, side, c0); continue; }
      if (ctl < 0.8f * gV) { if (idx >= 0) printf("R %s %u side %d: коридор занят (бег вдоль стены %.2f м/с)\n", isBox ? "box" : "seg", i, side, ctl); continue; }   // стенд, не игра
      for (int k = 0; k < 5; k++) {
        if (getenv("RAIL_ONLY") && (atoi(getenv("RAIL_ONLY")) != (int)ang[k] || atoi(strchr(getenv("RAIL_ONLY"), ',') + 1) != side)) continue;
        float sl = 0.f, c_ = 0.f; int cr = 0; int c = runOne(&S0, ang[k], verbose, &sl, &cr, &c_);
        if (c < 0) continue;
        did = 1;
        printf("R\t%s %u\tside %d\tangle %2.0f\tcontact_f %d\tslide %.2f\tideal %.2f\tcrossed %d\n", isBox ? "box" : "seg", i, side, ang[k], c, sl, gV * cosf(ang[k] * 3.14159265f / 180.f), cr);
      }
    }
    if (did) done++;
    next:;
  }
  return 0;
}
