// Гейт 1284 «посадка издалека пешком» (окно Гомер 107b): Гомер у пассажирской стороны, дверь водителя за машиной (путь к двери длиннее 4,5 м), irisOn = 1 (как в игре).
//   Новое: до двери идёт пешком, ирисовых посадок 0, прошёл ≥ 3 м. Старое (HOMERWALK1284_OLD=1): мгновенно на сиденье под ирисом (irisSeat 1), прошёл < 1 м.
//   cc ... -DSKIN_HOST [-DHOMERWALK1284_OLD=1] tests/homer107b/walk1284.c viewer/{homer,skin,col,car,props,stateprops,coins,watchdog}.c -lm  <level.shr> <homer.shr>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
static ColWorld world; static Car car;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static const float DT = 1.f / 60.f;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) return 2; fclose(f);
  SkinModel *m = skinLoad(argv[2]); if (!m) return 2;
  static Homer H; if (homerInit(&H, m)) return 2;
  carInit(&car, NULL, &world, 220.f, 3.5f, 172.f, 1.5708f); for (int i = 0; i < 90; i++) { CarInput ci = {0, 0, 0, 1}; carUpdate(&car, &world, &ci, DT); }
  HomerCar hc; homerCarFrom(&hc, &car, .62f);
  const CarBox *b = &hc.box; float rx = -b->fz, rz = b->fx;
  H.solid = 0; H.solidUser = 0; H.irisOn = 1;
  homerPlace(&H, &world, b->x + rx * (b->hw + .5f), hc.y + 2.f, b->z + rz * (b->hw + .5f), car.h);
  HomerInput in; memset(&in, 0, sizeof in);
  for (int i = 0; i < 30; i++) { homerCarFrom(&hc, &car, .62f); homerUpdate(&H, &world, &hc, &in, DT); homerPose(&H); }
  float x0 = H.x, z0 = H.z, walked = 0, px = H.x, pz = H.z; int irisBefore = H.irisSeat;
  int ok = homerEnterCar(&H, &hc, NULL);
  float t = 0; int inCar = 0;
  for (; t < 8.f; t += DT) {
    homerCarFrom(&hc, &car, .62f); homerUpdate(&H, &world, &hc, &in, DT); homerPose(&H);
    walked += hypotf(H.x - px, H.z - pz); px = H.x; pz = H.z;
    if (H.state == HS_IN_CAR) { inCar = 1; break; }
  }
  printf("enter_ok %d\nin_car %d\nseconds %.2f\nwalked_m %.2f\niris_seats %d\n", ok, inCar, t, walked, H.irisSeat - irisBefore); (void)x0; (void)z0;
  return 0;
}
