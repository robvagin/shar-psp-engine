// №1140/1153 (g11): водопропускная труба l7z7_culvert (лежачий цилиндр r 1,13 м, 3,5 м, верх у земли) у телефонной будки L7 H4-9 не должна останавливать машину как стена.
// Заезд с востока по z = -317 на 12 м/с с газом: со вторым полётом сквозь место трубы (x 80…83). Печатает минимум скорости за трубой и наибольший жёсткий удар. Параметр: L7 level.shr
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "car.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {}
static ColWorld world; static Car car;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (size + 15) & ~15); if (fread(d, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv; colInit(&world, d + co, ce - co);
  int runs = 0, stopped = 0; float hardMax = 0, vAfter = 1e9f;
  for (int dz = -3; dz <= 3; dz++) for (int sp = 0; sp < 2; sp++) {   // 7 смещений вбок × 2 скорости
    float z = -317.f + dz * .6f, v0 = sp ? 15.f : 10.f, g = 0; if (!colGround(&world, 95.f, z, 40.f, -50.f, &g, 0)) continue;
    carInit(&car, 0, &world, 95.f, g + .6f, z, 4.71239f); carSetSpeed(&car, v0); float minv = 1e9f, hv = 0;
    for (int fr = 0; fr < 60; fr++) { CarInput in = {1.f, 0, 0, 0}; carUpdate(&car, &world, &in, 1.f / 30.f); if (car.x < 80.f && car.x > 76.f) { float v = hypotf(car.vel[0], car.vel[2]); if (v < minv) minv = v; } if (car.x > 78.5f && car.hardV > hv) hv = car.hardV; }   // удар считаем до западного края трубы (дальше — забор l07_fencelines, он по данным стена)
    runs++; if (hv > hardMax) hardMax = hv; if (minv < 3.f || car.x > 79.f) stopped++; if (minv < vAfter) vAfter = minv;
  }
  printf("runs %d\nstopped %d\nhard_max %.1f\nv_after_min %.1f\n", runs, stopped, hardMax, vAfter);
  return 0; }
