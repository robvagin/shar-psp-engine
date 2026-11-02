// №1086 + круг 6 (g11): цена проверки «машина задела предмет» — число записей предметов, которые просматривает кадр.
// L1 (475 предметов), игрок едет 900 кадров по 30 Гц, рядом 14 чужих машин (9 в радиусе 60 м, 5 дальше 100 м, половина стоит, остальные едут).
// Печатает: проходы шага и списка на кадр (propsPassIter), обходы проверки касания на кадр (propsHitIter), число тронутых предметов и отпечаток
// всех положений предметов в конце (одинаковый при одинаковом поведении). Параметры: LEVEL.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "col.h"
#include "props.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
extern unsigned propsHitIter, propsPassIter;
static Props props; static ColWorld world; static Car player, oc[14];
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40));
  float gy = 0; colGround(&world, 220, 172, 20, -50, &gy, 0);
  carInit(&player, 0, &world, 220, gy + 1, 172, 1.57f);
  for (int i = 0; i < 14; i++) {
    float a = i * 0.9f, r = i < 9 ? 15.f + 5.f * i : 110.f + 10.f * i, x = 220 + cosf(a) * r, z = 172 + sinf(a) * r, g = 0;
    if (!colGround(&world, x, z, 40, -50, &g, 0)) g = gy;
    carInit(&oc[i], 0, &world, x, g + 1, z, a);
  }
  const int frames = 900; unsigned pass = 0, hit = 0;
  for (int fr = 0; fr < frames; fr++) {
    CarInput in = {1.f, 0, 0.3f * sinf(fr * 0.02f), 0}; if (fr % 300 == 0) carSetSpeed(&player, 12.f);
    carUpdate(&player, &world, &in, 1.f / 30.f);
    unsigned p0 = propsPassIter, h0 = propsHitIter;
    propsUpdate(&props, &world, &player, 1.f / 30.f);
    for (int i = 0; i < 14; i++) {
      if (i % 2) { carSetSpeed(&oc[i], 8.f); oc[i].x += 0.27f * cosf(i); oc[i].z += 0.27f * sinf(i); }
      propsHitCar(&props, &world, &oc[i], NULL);
    }
    pass += propsPassIter - p0; hit += propsHitIter - h0;
  }
  unsigned kicked = 0; uint32_t h = 2166136261u;
  for (unsigned i = 0; i < props.ninst; i++) {
    kicked += props.st[i].kicked;
    int32_t v[3] = {(int32_t)lrintf(props.st[i].x * 100.f), (int32_t)lrintf(props.st[i].y * 100.f), (int32_t)lrintf(props.st[i].z * 100.f)};
    for (int k = 0; k < 3; k++) { h ^= (uint32_t)v[k]; h *= 16777619u; }
  }
  printf("предметов %u\npass_per_frame %.1f\nhit_per_frame %.1f\nkicked %u\nhash %08x\n", props.ninst, (double)pass / frames, (double)hit / frames, kicked, h);
  return 0;
}
