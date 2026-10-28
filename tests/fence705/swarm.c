#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
// аргументы: level.shr зона [номер заезда для трассы T] [old — carJumpZoneGateOld = 1, №705 ч.2]
int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "old")) carJumpZoneGateOld = 1;
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2;
  for (unsigned i = 0; i < w.nzone; i++) printf("zone %u c=%.1f,%.1f,%.1f\n", i, w.zone[i].c[0], w.zone[i].c[1], w.zone[i].c[2]);
  const ColZone *Z = &w.zone[atoi(argv[2])]; const float vs[3] = {20, 25, 30}; int tr = argc > 3 && strcmp(argv[3], "old") ? atoi(argv[3]) : -1;
  for (int iv = 0; iv < 3; iv++) { int runs = 0, roof = 0; float bx = 0, by = 0, bz = 0;
    for (int ha = -4; ha <= 4; ha++) for (int lt = -2; lt <= 2; lt++) {
      float a = (270.f + ha * 2.f) * 0.0174533f, dx = sinf(a), dz = cosf(a), sx = Z->c[0] - dx * 40.f - dz * lt, sz = Z->c[2] - dz * 40.f + dx * lt, g;
      if (!colGround(&w, sx, sz, Z->c[1] + 30, Z->c[1] - 30, &g, 0)) continue;
      Car c; carInit(&c, NULL, &w, sx, g, sz, a); for (int i = 0; i < 20; i++) carUpdate(&c, &w, &(CarInput){0}, 1.f / 60); if (hypotf(c.x - sx, c.z - sz) > 1.f) continue;
      carSetSpeed(&c, vs[iv]); runs++; int onRoof = 0; float vmaxair = 0, apex = 0, xa = 0, vprev = 0, vlast = 0, xend = 0, yend = 0, zend = 0;
      for (int fr = 0; fr < 420; fr++) { CarInput in = {0}; in.throttle = c.v < vs[iv] ? 1.f : 0.f; carUpdate(&c, &w, &in, 1.f / 60);
        if (c.y > apex) { apex = c.y; xa = c.x; } if (c.grounded && c.y > 16.5f && c.x < 25.3f && c.x > 0.f) onRoof = 1; xend = c.x; yend = c.y; zend = c.z;
        if (ha == 0 && lt == 0 && iv == tr && fr % 4 == 0 && fr < 200) printf("T %d x=%.2f y=%.2f z=%.2f v=%.2f vy=%.2f gr=%d\n", fr, c.x, c.y, c.z, hypotf(c.vel[0], c.vel[2]), c.vel[1], c.grounded); }
      roof += onRoof; if (ha == 0 && lt == 0) { bx = xend; by = yend; bz = zend; printf("  v=%.0f центр: апекс y=%.1f на x=%.1f; конец x=%.1f y=%.1f z=%.1f roof=%d\n", vs[iv], apex, xa, xend, yend, zend, onRoof); } }
    printf("v=%.0f заездов %d, на крыше %d\n", vs[iv], runs, roof); }
  return 0; }
