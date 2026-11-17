// К1 (108, окно Стена): colRayFrac по клеткам вдоль отрезка == прежний обход всего прямоугольника, бит в бит. Мак, CPU, настоящая коллизия level.shr (COL1).
// Случайные лучи: короткие (1–20 м), средние (20–150 м), длинные (150–700 м, как «опора -> глаз» в F_L7_WALK), оба режима foliage 0/1.
// Сборка:  cc -std=c11 -O2 -I viewer tests/stena108/rayk1_host.c viewer/col.c -lm -o /tmp/rayk1_host && /tmp/rayk1_host level.shr [число_лучей]
// Красный контроль: -DK1_PLANT=1 (запас клеток 0 вместо 0,1 м и обход только узкой полосы) — расхождения должны появиться
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "col.h"
static ColWorld world;
static unsigned rs = 777u; static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.f / 16777216.f); }
static float rr(float a, float b) { return a + (b - a) * rnd(); }
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "level.shr"; long N = argc > 2 ? atol(argv[2]) : 100000;
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("нет %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("не уровень\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 не читается\n"); return 2; }
  float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
  for (unsigned i = 0; i < world.ntri; i += 7) { const ColTri *T = &world.tri[i]; for (int k = 0; k < 3; k++) { if (T->a[k] < lo[k]) lo[k] = T->a[k]; if (T->a[k] > hi[k]) hi[k] = T->a[k]; } }
  long bad = 0, hits = 0, tot = 0; double tn = 0, to = 0, tnL = 0, toL = 0; long nL = 0;
  static const float len[3][2] = {{1, 20}, {20, 150}, {150, 700}};
  for (long it = 0; it < N; it++) {
    const ColTri *T = &world.tri[(unsigned)(rnd() * world.ntri) % world.ntri];
    float a[3] = {(T->a[0] + T->b[0] + T->c[0]) / 3 + rr(-6, 6), (T->a[1] + T->b[1] + T->c[1]) / 3 + rr(.2f, 8.f), (T->a[2] + T->b[2] + T->c[2]) / 3 + rr(-6, 6)}, b[3];
    int kind = (int)(rnd() * 3.f) % 3; float L = rr(len[kind][0], len[kind][1]), az = rr(0, 6.2831853f), el = rr(-.5f, .5f);
    if (it % 11 == 0) { b[0] = rr(lo[0], hi[0]); b[1] = rr(lo[1], hi[1]); b[2] = rr(lo[2], hi[2]); }   // до случайной точки уровня
    else { b[0] = a[0] + L * cosf(el) * sinf(az); b[1] = a[1] + L * sinf(el); b[2] = a[2] + L * cosf(el) * cosf(az); }
    if (it % 13 == 0) b[2] = a[2];   // ровно вдоль X: ряд один
    if (it % 17 == 0) b[0] = a[0];   // ровно вдоль Z
    int fol = (int)(it & 1);
    colRayK1Old = 1; float o = colRayFrac(&world, a, b, fol); colRayK1Old = 0; float n = colRayFrac(&world, a, b, fol);
    tot++; if (o < 1.f) hits++; if (memcmp(&o, &n, 4)) { bad++; if (bad < 6) printf("  РАСХОЖДЕНИЕ старый %.9g новый %.9g  a=%.2f,%.2f,%.2f b=%.2f,%.2f,%.2f\n", o, n, a[0], a[1], a[2], b[0], b[1], b[2]); }
    if (kind == 2 && it % 7 == 0) { colRayK1Old = 1; double t0 = now(); float x = colRayFrac(&world, a, b, 0); double t1 = now(); colRayK1Old = 0; float y = colRayFrac(&world, a, b, 0); double t2 = now(); toL += t1 - t0; tnL += t2 - t1; nL++; if (x < -5.f || y < -5.f) puts("?"); }
  }
  printf("К1 %s: лучей %ld (задели %ld), расхождений %ld; длинный луч (150–700 м): прежний %.1f мкс, новый %.1f мкс (%ld лучей)\n", lvl, tot, hits, bad, nL ? toL / nL * 1e6 : 0., nL ? tnL / nL * 1e6 : 0., nL);
  (void)tn; (void)to;
  return bad != 0;
}
