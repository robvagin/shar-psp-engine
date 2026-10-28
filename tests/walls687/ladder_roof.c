// №705 (L5, пожарная лестница-трамплин, зона 4 у (45,3; 11,5; 4,9) — по просьбе дирижёра код H8-2): рой заездов на лестницу, считаем, садится ли машина на крышу здания
// (на земле, y > 16,5, x < 25,3 после перелёта) и сколько заездов гасит скорость в воздухе о забор над крышей (скорость падает больше чем на 15 м/с за 3 кадра при y > 14).
// Старт за 40 м до центра зоны по курсу, курсы 270° ± 8° шаг 2°, смещения −2…2 м шаг 1, скорости 20/25/30 м/с, 7 с.
// cc -std=c11 -O2 -Iviewer tests/walls687/ladder_roof.c viewer/car.c viewer/col.c -lm ; ./a.out level.shr [zone]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "car.h"
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2; const ColZone *Z = &w.zone[argc > 2 ? atoi(argv[2]) : 4];
  int runs = 0, roof = 0, crash = 0; const float vs[3] = {20, 25, 30};
  for (int ha = -4; ha <= 4; ha++) for (int lt = -2; lt <= 2; lt++) for (int iv = 0; iv < 3; iv++) {
    float a = (270.f + ha * 2.f) * 0.0174533f, dx = sinf(a), dz = cosf(a), sx = Z->c[0] - dx * 40.f - dz * lt, sz = Z->c[2] - dz * 40.f + dx * lt, g;
    if (!colGround(&w, sx, sz, Z->c[1] + 30, Z->c[1] - 30, &g, 0)) continue;
    Car c; carInit(&c, NULL, &w, sx, g, sz, a); for (int i = 0; i < 20; i++) carUpdate(&c, &w, &(CarInput){0}, 1.f / 60); if (hypotf(c.x - sx, c.z - sz) > 1.f) continue;
    carSetSpeed(&c, vs[iv]); runs++; int onRoof = 0, hit = 0; float vh[3] = {0, 0, 0};
    for (int fr = 0; fr < 420; fr++) { CarInput in = {0}; in.throttle = c.v < vs[iv] ? 1.f : 0.f; carUpdate(&c, &w, &in, 1.f / 60);
      float v = hypotf(c.vel[0], c.vel[2]); if (c.y > 14.f && c.x < 60.f && vh[0] - v > 15.f) hit = 1; vh[0] = vh[1]; vh[1] = vh[2]; vh[2] = v; vh[0] = vh[0];
      if (c.grounded && c.y > 16.5f && c.x < 25.3f && c.x > 0.f) onRoof = 1; }
    roof += onRoof; crash += hit; }
  printf("заездов %d: садятся на крышу %d, удар в воздухе (падение > 15 м/с, y > 14) %d\n", runs, roof, crash); return 0; }
