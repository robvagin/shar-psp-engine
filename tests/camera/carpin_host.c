// Аудит 04.11 (g06): камера пешком, своя машина за спиной героя. Глаз (дальность 6,45 м, как после переноса на старте этапа) дальше кузова и вне его, мира на луче нет, луч у нижнего края задевает крышу.
// Прежнее (homerCarPinOld = 1): упор в кузов считался стеной, глаз подтягивался к голове до 1,0 м. Теперь глаз остаётся на дальности, ближе MIN_OK м не подходит. Красный контроль: -DCARPIN_OLD=1.
//   cc -std=c99 -O2 -DSKIN_HOST -I viewer tests/camera/carpin_host.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o /tmp/carpin && /tmp/carpin build/level.shr build/homer.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
static ColWorld world; static Car car; static HomerCar hc;
static float rayCar(const float a[3], const float b[3]) {   // доля луча до кузова (проба 400 точек, запас 0,1 м); 1 — свободно
  for (int i = 0; i <= 400; i++) { float t = i / 400.f, x = a[0] + (b[0] - a[0]) * t, y = a[1] + (b[1] - a[1]) * t, z = a[2] + (b[2] - a[2]) * t;
    const CarBox *k = &hc.box; float dx = x - k->x, dz = z - k->z, u = fabsf(dx * k->fx + dz * k->fz) - k->hl, v = fabsf(-dx * k->fz + dz * k->fx) - k->hw;
    if (u < .1f && v < .1f && y < k->y + k->hi + .1f && y > k->y + k->lo - .1f) { homerCamCarTop = k->y + k->hi; return t; } }
  return 1.f;
}
int main(int argc, char **argv) {
  if (argc < 3) return 2; FILE *f = fopen(argv[1], "rb"); if (!f) return 2; unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96); fseek(f, 0, SEEK_END); unsigned fsize = ftell(f), end = fsize;
  if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = aligned_alloc(16, (end - colOff + 15) & ~15u); fseek(f, colOff, SEEK_SET); if (fread(cm, 1, end - colOff, f) != end - colOff) return 2; fclose(f);
  if (colInit(&world, cm, end - colOff)) return 1;
  SkinModel *m = skinLoad(argv[2]); static Homer H; if (homerInit(&H, m)) return 1;
  float x0 = 220.f, z0 = 172.f, hd = 1.5708f;   // старт L1 (дорога), герой лицом к +x, камера хочет встать на -x
  carInit(&car, NULL, &world, x0 - 2.6f, 3.5f, z0 + 0.6f, 0.4f); homerCarFrom(&hc, &car, 0.62f);
  homerPlace(&H, &world, x0, 3.5f, z0, hd); homerCamDist = 6.45f; homerCamRayExtra = rayCar;
#if CARPIN_OLD
  homerCarPinOld = 1;
#endif
  HomerInput in; memset(&in, 0, sizeof in); in.camYaw = hd; float dt = 1.f / 30, e[3], l[3];
  for (int i = 0; i < 150; i++) { homerUpdate(&H, &world, &hc, &in, dt); homerPose(&H); homerCamera(&H, &world, dt, e, l); }
  float head[3] = {H.x, H.y + 1.5f, H.z}, d = sqrtf((e[0] - head[0]) * (e[0] - head[0]) + (e[1] - head[1]) * (e[1] - head[1]) + (e[2] - head[2]) * (e[2] - head[2]));
  const float MIN_OK = 3.0f; printf("глаз от головы %.2f м (дальность кадра %.2f, по лучам %.2f), ждём ≥ %.1f\n", d, H.camDist, homerCamDbg[4], MIN_OK);
  printf("%s\n", d >= MIN_OK ? "PASS" : "FAIL"); return d >= MIN_OK ? 0 : 1;
}
