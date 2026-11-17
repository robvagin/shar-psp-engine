// 107б П11: взрыв задевает не больше 20 ближайших предметов (оригинал). Хост, L1: центр взрыва — точка с наибольшим числом предметов в 20 м; взрыв игрока (byPlayer 0 — разлёт, без обломков); считаем сдвинутых/проснувшихся.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "car.h"
#include "sverka_shared.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = aligned_alloc(16, (size + 15) & ~15); if (fread(d, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, d + co, ce - co); propsInit(&props, d + po, *(unsigned *)(d + po + 40));
  unsigned best = 0, bi = 0;
  for (unsigned i = 0; i < props.ninst; i++) { unsigned n = 0; for (unsigned j = 0; j < props.ninst; j++) { float dx = props.st[j].x - props.st[i].x, dz = props.st[j].z - props.st[i].z; n += dx * dx + dz * dz < 18.f * 18.f; } if (n > best) { best = n; bi = i; } }
  svBoom.seq++; svBoom.radius = 20.f; svBoom.x = props.st[bi].x; svBoom.y = props.st[bi].y; svBoom.z = props.st[bi].z; svBoom.force = 6000.f; svBoom.byPlayer = 0;
  Car c; float g = 0; colGround(&world, svBoom.x + 30.f, svBoom.z, svBoom.y + 5.f, svBoom.y - 10.f, &g, 0); carInit(&c, 0, &world, svBoom.x + 60.f, g + 1.f, svBoom.z, 0.f);
  unsigned inR = 0; for (unsigned j = 0; j < props.ninst; j++) { float dx = props.st[j].x - svBoom.x, dz = props.st[j].z - svBoom.z; inR += dx * dx + dz * dz < 20.f * 20.f; }
  propsUpdate(&props, &world, &c, 1.f / 30.f);
  unsigned moved = 0; for (unsigned j = 0; j < props.ninst; j++) moved += !props.st[j].asleep || props.st[j].hidden;
  printf("in_radius %u\nmoved %u\n", inR, moved);
  return 0;
}
