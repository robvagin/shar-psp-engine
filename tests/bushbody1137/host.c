// №1137/1154 (g11): кусты-стейтпропы без объёма получают тело. Хост на данных уровней: (а) сколько кустов (класс 2, высота 0,4–3 м, есть кусок) на уровнях остаются без тела; (б) на L7 у J9-5 куст (156,7; 159,9)
// машина встречает по правилу №1101: на 3 м/с он держит (не проехала), на 12 м/с продавливает (проехала, скорость в момент входа упала). Параметры: LEVEL.shr...  (L7 — последним для заезда)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps sp; static Car car; static Coins coins;
static int drive(unsigned i, float v) {
  float rx = sp.run[i].x, ry = sp.run[i].y, x = rx - 12.f, z = sp.run[i].z, g = 0; if (!colGround(&world, x, z, ry + 4.f, -50.f, &g, 0)) return -1;
  spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return -2;
  carInit(&car, 0, &world, x, g + .6f, z, 1.5708f); carSetSpeed(&car, v); int crossed = 0;
  for (int f = 0; f < 150; f++) { CarInput in = {0, 0, 0, 0}; float sp0 = hypotf(car.vel[0], car.vel[2]); static int touched; if (f == 0) touched = 0; if (sp.run[i].cool > 0.f) touched = 1; in.throttle = touched ? 1.f : (sp0 < v ? 1.f : 0.f); /* до касания держим скорость входа, после — полный газ */ carUpdate(&car, &world, &in, 1.f / 30.f); spHitCar(&sp, &world, &car, &coins); spUpdate(&sp, &world, &car, &coins, 1.f / 30.f); if (car.x > rx + 2.5f) crossed = 1; }
  return crossed;
}
int main(int argc, char **argv) {
  int bare = 0, bushes = 0;
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) continue; fclose(f);
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    spFree(&sp); memset(&props, 0, sizeof props); colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    for (unsigned i = 0; i < sp.ninst; i++) { const SpType *T = &sp.type[sp.inst[i].type]; if (T->cls != 2 || T->kind || !T->nPiece || 2.f * T->h[1] < .4f || 2.f * T->h[1] > 3.f || fmaxf(T->h[0], T->h[2]) > 2.5f) continue; bushes++; if (!T->nCol) bare++; }
    if (a == argc - 1) {   // последний файл — L7: куст у J9-5
      int found = -1; for (unsigned i = 0; i < sp.ninst; i++) if (fabsf(sp.inst[i].x - 156.7f) < .5f && fabsf(sp.inst[i].z - 159.9f) < .5f && sp.type[sp.inst[i].type].cls == 2) found = (int)i;
      int slow = found >= 0 ? drive((unsigned)found, 3.f) : -9, fast = found >= 0 ? drive((unsigned)found, 12.f) : -9;
      printf("bush_found %d\nslow_crossed %d\nfast_crossed %d\n", found, slow, fast);
    }
  }
  printf("bushes %d\nbare %d\n", bushes, bare);
  return 0; }
