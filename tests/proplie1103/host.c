// №1103 (g11): сбитые скамейки, урны и т.п. лежат до выгрузки зоны (как в оригинале). Хост на L1: предмет у дороги сбивает машина игрока; потом
// (а) игрок отходит на 100 м и 300 м на 120 с без взгляда камеры — предмет лежит там же, не спрятан, не вернулся домой (прежние TEST81: домой через 5 с дальше 60 м);
// (б) игрок уходит дальше PROP_ZONE_R (зона выгружена) — предмет возвращается домой; (в) машина игрока проезжает по лежащему 3 раза — не застревает (скорость не падает ниже 70 %)
// и не сдвигает его (лежащий машине не мешает). Ключи для accept.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
static Props props; static ColWorld world; static Car player;
static unsigned char *data;
static int load(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return 1; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  if (!data) data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 1; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); return 0;
}
static int never(void *u, float x, float y, float z, float r) { (void)u; (void)x; (void)y; (void)z; (void)r; return 0; }
static float minSpeed;
static void drive(unsigned i, int frames, float speed) {   // машина игрока едет на предмет i с 14 м
  const PropState *st = &props.st[i]; float ang = 0.9f, bx = st->x - sinf(ang) * 14.f, bz = st->z - cosf(ang) * 14.f, g = 0;
  if (!colGround(&world, bx, bz, 40, -50, &g, 0)) g = st->y;
  carInit(&player, 0, &world, bx, g + .6f, bz, ang); minSpeed = 1e9f; float closest = 1e9f;
  for (int f = 0; f < frames; f++) {
    CarInput in = {f < 90 ? 1.f : 0.f, f >= 90 ? .5f : 0.f, 0, 0}; if (f == 0) carSetSpeed(&player, speed);
    carUpdate(&player, &world, &in, 1.f / 30.f); propsUpdate(&props, &world, &player, 1.f / 30.f);
    float v = hypotf(player.vel[0], player.vel[2]), d = hypotf(player.x - st->x, player.z - st->z);   // скорость в момент наибольшего сближения с предметом
    if (d < closest) { closest = d; minSpeed = v; }
  }
}
static float dist2d(const PropState *s, const float *a) { return hypotf(s->x - a[0], s->z - a[1]); }
int main(int argc, char **argv) {
  if (argc < 2 || load(argv[1])) return 2;
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
  int chosen = -1, kicked = 0, away = 0, hidden100 = 1, hidden300 = 1, home100 = 0, home300 = 0, homeFar = 0, still = 0; float vmin = 0, moved = 0;
  for (unsigned k = 0; k < nc && chosen < 0; k++) {
    unsigned i = cand[k]; propsFree(&props); load(argv[1]);
    drive(i, 150, 10.f); if (!props.st[i].kicked) continue;
    for (int f = 0; f < 600; f++) propsUpdate(&props, &world, &player, 1.f / 30.f);   // лежит и спит
    PropState *st = &props.st[i]; if (!st->asleep || st->fresh) continue;
    chosen = (int)i; kicked = 1; away = dist2d(st, (float[]){st->home[0], st->home[2]}) > .5f;
    float rest[3] = {st->x, st->y, st->z};
    float ex[3] = {100.f, 300.f, 450.f};
    for (int q = 0; q < 3; q++) {
      float px = st->home[0] + ex[q], pz = st->home[2];
      for (int f = 0; f < 120 * 30; f++) propsRespawnStep(&props, px, pz, 1.f / 30.f, never, NULL);
      int back = dist2d(st, (float[]){st->home[0], st->home[2]}) < .5f, hid = st->hidden;
      if (q == 0) { home100 = back; hidden100 = hid; still = fabsf(st->x - rest[0]) + fabsf(st->z - rest[2]) < .05f; }
      if (q == 1) { home300 = back; hidden300 = hid; }
      if (q == 2) homeFar = back && !hid;
      if (q < 2 && (back || hid)) break;
    }
    if (!homeFar) continue;
    // (в) проезд по лежащему: возвращаем в лежачее состояние перезагрузкой? — предмет после (б) дома; сбиваем заново и проверяем проезд поверх
    propsFree(&props); load(argv[1]); drive(i, 150, 10.f); for (int f = 0; f < 600; f++) propsUpdate(&props, &world, &player, 1.f / 30.f);
    st = &props.st[i]; float r2[2] = {st->x, st->z}; vmin = 1e9f; float v0 = 10.f;
    for (int pass = 0; pass < 3; pass++) { drive(i, 120, v0); if (minSpeed < vmin) vmin = minSpeed; }
    moved = dist2d(&props.st[i], r2);
    vmin = vmin / v0;
  }
  printf("chosen %d\nkicked %d\nlies_away %d\nstill_100m %d\nhidden_100m %d\nhome_100m %d\nhome_300m %d\nhidden_300m %d\nhome_beyond_zone %d\nspeed_ratio %.2f\nmoved_by_cars %.2f\n",
         chosen, kicked, away, still, hidden100, home100, home300, hidden300, homeFar, vmin, moved);
  return 0;
}
