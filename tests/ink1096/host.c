// №1096 (g11): контур предмета мультика не мигает. Хост: L1, предмет у дороги. Трафик задевает предмет — inkTouch 0; игрок задевает — 1, и после засыпания 1;
// регулятор кадра контур предметов не отключает (propsInkGov(1) == 0). Ключи для accept.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
static Props props; static ColWorld world; static Car player, traf;
static unsigned char *data;
static int load(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return 1; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  if (!data) data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 1; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); return 0;
}
// едем машиной car на предмет i с 14 м, 10 м/с; viaPlayer: 1 — это машина игрока (через propsUpdate), 0 — трафик (propsHitCar, игрок стоит в стороне)
static void run(unsigned i, int viaPlayer, int frames) {
  const PropState *st = &props.st[i]; float ax = st->x, az = st->z;
  float ang = 0.9f, bx = ax - sinf(ang) * 14.f, bz = az - cosf(ang) * 14.f, g = 0;
  if (!colGround(&world, bx, bz, 40, -50, &g, 0)) g = st->y;
  Car *c = viaPlayer ? &player : &traf;
  carInit(c, 0, &world, bx, g + .6f, bz, ang);
  if (!viaPlayer) { float gy = 0; colGround(&world, bx + 30.f, bz, 40, -50, &gy, 0); carInit(&player, 0, &world, bx + 30.f, gy + .6f, bz, 0.f); }
  for (int f = 0; f < frames; f++) {
    CarInput in = {f < 60 ? 1.f : 0.f, f >= 60 ? .5f : 0.f, 0, 0}; if (f == 0) carSetSpeed(c, 10.f);
    carUpdate(c, &world, &in, 1.f / 30.f);
    propsUpdate(&props, &world, &player, 1.f / 30.f);
    if (!viaPlayer) propsHitCar(&props, &world, &traf, NULL);
  }
}
int main(int argc, char **argv) {
  if (argc < 2 || load(argv[1])) return 2;
  // кандидаты: малые предметы у спавна (ящик, гидрант, урна), по возрастанию расстояния
  unsigned cand[12], nc = 0;
  for (int pass = 0; pass < 12; pass++) {
    float best = 1e9f; int bi = -1;
    for (unsigned i = 0; i < props.ninst; i++) {
      const PropType *ty = &props.types[props.def[i].type]; if (2.f * ty->halfH > 2.f || ty->radius > 1.f) continue;
      int used = 0; for (unsigned k = 0; k < nc; k++) if (cand[k] == i) used = 1; if (used) continue;
      float dx = props.st[i].x - 220.f, dz = props.st[i].z - 172.f, d = dx * dx + dz * dz; if (d > 15.f * 15.f && d < best) { best = d; bi = (int)i; }
    }
    if (bi < 0) break; cand[nc++] = (unsigned)bi;
  }
  int chosen = -1, trafKicked = 0, trafInk = 1, plKicked = 0, plInk = 0, plInkAsleep = 0, plAsleep = 0;
  for (unsigned k = 0; k < nc && chosen < 0; k++) {
    unsigned i = cand[k]; propsFree(&props); load(argv[1]);
    run(i, 0, 150); int tk = props.st[i].kicked, ti = props.st[i].inkTouch;
    if (!tk) continue;
    propsFree(&props); load(argv[1]);
    run(i, 1, 90); int pk = props.st[i].kicked, pi = props.st[i].inkTouch;
    if (!pk) continue;
    // дальше предмет засыпает: машина игрока стоит
    for (int f = 0; f < 600; f++) { CarInput z = {0, 1.f, 0, 0}; carUpdate(&player, &world, &z, 1.f / 30.f); propsUpdate(&props, &world, &player, 1.f / 30.f); }
    chosen = (int)i; trafKicked = tk; trafInk = ti; plKicked = pk; plInk = pi; plAsleep = props.st[i].asleep; plInkAsleep = props.st[i].inkTouch;
  }
  printf("chosen %d\ntraf_kicked %d\ntraf_ink %d\nplayer_kicked %d\nplayer_ink %d\nplayer_asleep %d\nplayer_ink_after_sleep %d\ngov_off %d\n", chosen, trafKicked, trafInk, plKicked, plInk, plAsleep, plInkAsleep, !propsInkGov(1));
  return 0;
}
