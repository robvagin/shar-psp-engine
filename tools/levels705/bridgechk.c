// №905 (Глюки): звенья «моста» забора (в прежнем файле верх +1e6, в новом конечный): bridgechk NEW.shr OLD.shr
// по каждому: полёт (9 м над землёй, 18 м/с, курс на звено с лица, удары типа 3 > 3 м/с) и езда по земле 8/15/22 м/с (пересекла линию звена?) на новом и на прежнем файле
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "car.h"
static int NLAT = 1;
static ColWorld ld(const char *p) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) exit(2); fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) exit(2); return w; }
static int air(ColWorld *w, float mx, float mz, float nx, float nz) {   // удары в полёте
  float sx = mx + nx * 8.f, sz = mz + nz * 8.f, g, a = atan2f(-nx, -nz); if (!colGround(w, mx, mz, 400.f, -400.f, &g, 0)) return -1;
  Car c; carInit(&c, NULL, w, sx, g, sz, a); for (int i = 0; i < 20; i++) carUpdate(&c, w, &(CarInput){0}, 1.f / 60); c.y = g + 9.f; c.vy = 0; carSetSpeed(&c, 18.f); int hits = 0;
  for (int fr = 0; fr < 120; fr++) { carUpdate(&c, w, &(CarInput){0}, 1.f / 60); if (c.grounded) break; float dn = (c.x - mx) * nx + (c.z - mz) * nz; if (dn > 4.5f || dn < -1.f) continue;   // удар засчитывается этому звену, только когда центр машины в 4,5 м перед его линией
    for (int k = 0; k < c.contacts; k++) if (c.ctype[k] == 3 && c.hitSpeed > 3.f) { hits++; break; } }
  return hits; }
static int ground(ColWorld *w, float mx, float mz, float nx, float nz, float v, float lt) {   // пересекла линию; lt — боковой сдвиг старта вдоль линии
  float sx = mx + nx * 10.f - nz * lt, sz = mz + nz * 10.f + nx * lt, g, a = atan2f(-nx, -nz); if (!colGround(w, sx, sz, 400.f, -400.f, &g, 0)) return -1;
  Car c; carInit(&c, NULL, w, sx, g, sz, a); for (int i = 0; i < 20; i++) carUpdate(&c, w, &(CarInput){0}, 1.f / 60); if (hypotf(c.x - sx, c.z - sz) > .8f) return -1;
  carSetSpeed(&c, v); for (int fr = 0; fr < 240; fr++) { CarInput in = {0}; in.throttle = c.v < v ? 1.f : 0.f; carUpdate(&c, w, &in, 1.f / 60); if ((c.x - mx) * -nx + (c.z - mz) * -nz > 2.f) return 1; }
  return 0; }
int main(int argc, char **argv) {
  if (getenv("LAT")) NLAT = 5; ColWorld n = ld(argv[1]), o = ld(argv[2]); int links = 0, airOld = 0, airNew = 0, airRuns = 0, gOldC = 0, gNewC = 0, gRuns = 0, newOnly = 0;
  for (unsigned i = 0; i < n.nseg; i++) { const ColSeg *s = &n.seg[i]; if (s->y1 > 1e5f) continue; float mx = (s->a[0] + s->b[0]) / 2, mz = (s->a[1] + s->b[1]) / 2, nx = s->n[0], nz = s->n[1], old1 = -1e9f; int found = 0;
    for (unsigned j = 0; j < o.nseg; j++) { const ColSeg *q = &o.seg[j]; if (q->n[0] * nx + q->n[1] * nz < .98f) continue; float dx = q->b[0] - q->a[0], dz = q->b[1] - q->a[1], L2 = dx * dx + dz * dz; if (L2 < 1e-6f) continue;
      float t = ((mx - q->a[0]) * dx + (mz - q->a[1]) * dz) / L2; if (t < 0 || t > 1) continue; float px = q->a[0] + t * dx - mx, pz = q->a[1] + t * dz - mz; if (px * px + pz * pz > .05f * .05f) continue; found = 1; if (q->y1 > old1) old1 = q->y1; }
    if (!found || old1 < 1e5f) continue; links++;   // было бесконечным, стало конечным: звено моста
    int an = air(&n, mx, mz, nx, nz), ao = air(&o, mx, mz, nx, nz); if (an >= 0 && ao >= 0) { airRuns++; airNew += an > 0; if (an > 0 && getenv("V")) { float g0 = 0; colGround(&n, mx + nx * 8.f, mz + nz * 8.f, 400.f, -400.f, &g0, 0); printf("  полёт упёрся на новом: звено (%.1f,%.1f) n(%.2f,%.2f) верх звена %.1f, земля с лица %.1f, верх над землёй с лица %.1f м (полёт на 9 м)\n", mx, mz, nx, nz, s->y1, g0, s->y1 - g0); } airOld += ao > 0; }
    for (int iv = 0; iv < 3 * NLAT; iv++) { float v = (float[]){8, 15, 22}[iv % 3], lt = (float[]){0, -1.5f, 1.5f, -.75f, .75f}[iv / 3]; int gn = ground(&n, mx, mz, nx, nz, v, lt), go = ground(&o, mx, mz, nx, nz, v, lt); if (gn >= 0 && go >= 0 && getenv("V") && (gn || go)) { float gl = 0, gf = 0, gbk = 0; colGround(&n, mx, mz, 400.f, -400.f, &gl, 0); colGround(&n, mx + nx * 3.f, mz + nz * 3.f, 400.f, -400.f, &gf, 0); colGround(&n, mx - nx * 3.f, mz - nz * 3.f, 400.f, -400.f, &gbk, 0); printf("  пересечение (%.1f,%.1f) %.0f м/с: новый %d, прежний %d; верх нового %.1f; земля: в линии %.1f, с лица %.1f, за %.1f\n", mx, mz, v, gn, go, s->y1, gl, gf, gbk); }
      if (gn >= 0 && go >= 0) { gRuns++; gNewC += gn; gOldC += go; if (gn && !go) { newOnly++; if (getenv("V")) printf("  новая проезжает: звено (%.1f,%.1f) n(%.2f,%.2f) y1 %.1f (прежний %.1f) %.0f м/с\n", mx, mz, nx, nz, s->y1, old1, v); } } } }
  printf("звеньев моста %d; полёт: заездов %d, упёрлись на прежнем %d, на новом %d; по земле: заездов %d, пересекли на прежнем %d, на новом %d (только на новом %d)\n", links, airRuns, airOld, airNew, gRuns, gOldC, gNewC, newOnly);
  return 0; }
