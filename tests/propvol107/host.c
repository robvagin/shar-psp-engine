// 107в П4: контакт машины с предметом по объёмам из данных диска (PVL1). Хост, L1 (level.shr с таблицей PVL1): для каждого типа предмета, у которого есть объёмы (до 10 типов), один экземпляр; машина проезжает на 6 м/с
// мимо него с боковым смещением от -3 до +3 м (шаг 0,25 м) под двумя курсами. Эталон — независимая выборка точек кузова вдоль пути против тех же объёмов (ящики и описанные ящики цилиндров): «точно задел» — с запасом -0,10 м,
// «точно мимо» — с запасом +0,10 м. Нарушение: должен был задеть, а не задел; или точно мимо, а предмет тронут. Печатает число заездов и нарушений. Параметр: LEVEL.shr [old]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world;
typedef struct { CarBox b; } Fr;
static unsigned char *load(const char *p, unsigned *n) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (s + 15) & ~15); if (fread(d, 1, s, f) != (size_t)s) return NULL; fclose(f); *n = (unsigned)s; return d; }
static void init(unsigned char *d, unsigned n) { unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = n; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv; colInit(&world, d + co, ce - co); propsInit(&props, d + po, *(unsigned *)(d + po + 40)); }
// истина: хоть одна точка кузова (сетка 0,1 м, 3 высоты) внутри хоть одного объёма с запасом m (m < 0 — объём уже на |m|)
static int truth(unsigned inst, const CarBox *fr, int nfr, float m) {
  const PropState *st = &props.st[inst]; unsigned t = props.def[inst].type, first = props.volIdx[2 * t], n = props.volIdx[2 * t + 1];
  float R[3][3]; const float *q = st->q; float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0][0] = 1 - 2 * (y * y + z * z); R[0][1] = 2 * (x * y - w * z); R[0][2] = 2 * (x * z + w * y); R[1][0] = 2 * (x * y + w * z); R[1][1] = 1 - 2 * (x * x + z * z); R[1][2] = 2 * (y * z - w * x);
  R[2][0] = 2 * (x * z - w * y); R[2][1] = 2 * (y * z + w * x); R[2][2] = 1 - 2 * (x * x + y * y);
  float org[3], cv[3] = {st->cx, st->cy, st->cz}; for (int r = 0; r < 3; r++) org[r] = (r == 0 ? st->x : r == 1 ? st->y : st->z) - (R[r][0] * cv[0] + R[r][1] * cv[1] + R[r][2] * cv[2]);
  for (int f = 0; f < nfr; f++) { const CarBox *b = &fr[f];
    for (float lf = -b->hl; lf <= b->hl + 1e-3f; lf += .1f) for (float lr = -b->hw; lr <= b->hw + 1e-3f; lr += .1f) for (int hz = 0; hz < 3; hz++) {
      float yy = b->y + b->lo + (b->hi - b->lo) * (hz * .5f), px = b->x + b->fx * lf + (-b->fz) * lr, pz = b->z + b->fz * lf + b->fx * lr;
      for (unsigned v = first; v < first + n; v++) { const PropVol *V = &props.vol[v]; float C[3], d[3] = {px, yy, pz}; int in = 1;
        for (int r = 0; r < 3; r++) C[r] = org[r] + R[r][0] * V->c[0] + R[r][1] * V->c[1] + R[r][2] * V->c[2];
        for (int ax = 0; ax < 3 && in; ax++) { float wv[3]; for (int r = 0; r < 3; r++) wv[r] = R[r][0] * V->a[3 * ax] + R[r][1] * V->a[3 * ax + 1] + R[r][2] * V->a[3 * ax + 2];
          float s = (d[0] - C[0]) * wv[0] + (d[1] - C[1]) * wv[1] + (d[2] - C[2]) * wv[2]; if (fabsf(s) > V->h[ax] + m) in = 0; }
        if (in) return 1; } } }
  return 0;
}
int main(int argc, char **argv) {
  unsigned n; unsigned char *d = load(argv[1], &n); if (!d) return 2; if (argc > 2) propsVol107Old = atoi(argv[2]);
  init(d, n); if (!props.vol) { printf("нет PVL1\n"); return 3; }
  unsigned used[64], nu = 0; int runs = 0, viol = 0, missed = 0, ghost = 0;
  for (unsigned i = 0; i < props.ninst && nu < 10; i++) {
    unsigned t = props.def[i].type; if (!props.volIdx[2 * t + 1] || props.types[t].mass <= 0.f) continue; int seen = 0; for (unsigned k = 0; k < nu; k++) if (used[k] == t) seen = 1; if (seen) continue; used[nu++] = t;
    for (int hd = 0; hd < 2; hd++) for (int off = -12; off <= 12; off++) {
      propsFree(&props); init(d, n); const PropState *st0 = &props.st[i]; float ang = hd ? 0.9f : 2.4f, fx = sinf(ang), fz = cosf(ang), rx = -fz, rz = fx, lat = off * .25f;
      float px = st0->x + rx * lat - fx * 12.f, pz = st0->z + rz * lat - fz * 12.f, g = 0; if (!colGround(&world, px, pz, st0->y + 3.f, st0->y - 6.f, &g, 0)) continue;
      Car c; Fr tr[150]; int nf = 0;
      carInit(&c, 0, &world, px, g + .6f, pz, ang);
      for (int f = 0; f < 150; f++) { CarInput in = {0, 0, 0, 0}; carSetSpeed(&c, 6.f); carUpdate(&c, &world, &in, 1.f / 30.f); carBox(&c, &tr[nf].b); nf++; }   // прогон 1: траектория без предмета
      CarBox arr[150]; for (int f = 0; f < nf; f++) arr[f] = tr[f].b;
      int tHit = truth(i, arr, nf, -.10f), tMiss = !truth(i, arr, nf, +.10f);
      propsFree(&props); init(d, n); carInit(&c, 0, &world, px, g + .6f, pz, ang);
      for (int f = 0; f < 150; f++) { CarInput in = {0, 0, 0, 0}; carSetSpeed(&c, 6.f); carUpdate(&c, &world, &in, 1.f / 30.f); propsUpdate(&props, &world, &c, 1.f / 30.f); if (props.st[i].kicked) break; }
      int kicked = props.st[i].kicked; runs++;
      if (tHit && !kicked) { viol++; missed++; } else if (tMiss && kicked) { viol++; ghost++; } } }
  printf("types %u\nruns %d\nviol %d\nmissed %d\nghost %d\n", nu, runs, viol, missed, ghost);
  return 0;
}
