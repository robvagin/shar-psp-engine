// Гейт Г4/Г5 «крыша машины» (окно Гомер 107b; сверка Fable-2): homer.c на реальном мире L1, реальной машине (car.c) и реальном трафике (traffic.c trafficRide/trafficSolid).
//   Г5: Гомер на крыше СВОЕЙ стоящей машины, стик вперёд 1 с: путь по крыше. Новое: идёт обычным шагом (≥ 2,0 м). Старое (HOMERRIDE107B_OLD=1): vx = vz = 0 каждый кадр, ползёт (< 1 м).
//   Г4: Гомер на крыше машины ТРАФИКА, которая разгоняется до 5 м/с и поворачивает 0,25 рад/с, 2,5 с без ввода. Новое: остаётся на крыше (в кузове по плану, высота крыши).
//       Старое: машина уезжает из-под ног, он падает на землю (ушёл от центра больше 3 м или ниже крыши).
//   cc ... -DSKIN_HOST [-DHOMERRIDE107B_OLD=1] tests/homer107b/roof.c viewer/{homer,skin,col,car,props,stateprops,coins,traffic,msncar,watchdog}.c viewer/mission/{msn_file,msn_ops}.c -lm  <level.shr> <homer.shr>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "traffic.h"
static ColWorld world; static Car car; static Traffic T;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static const float DT = 1.f / 60.f;
static HomerInput in0(float camYaw) { HomerInput i; memset(&i, 0, sizeof i); i.camYaw = camYaw; return i; }
static int solidT(void *u, float *x, float *z, float y, float r, float *top, float *nx, float *nz) { return trafficSolid(u, x, z, y, r, top, nx, nz); }
static int rideT(void *u, int id, float x, float y, float z, HomerRide *o) { return trafficRide(u, id, x, y, z, o); }
int main(int argc, char **argv) {
  const char *lvl = argv[1], *hom = argv[2];
  FILE *f = fopen(lvl, "rb"); if (!f) return 2;
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) return 2; fclose(f);
  SkinModel *m = skinLoad(hom); if (!m) return 2;
  static Homer H; if (homerInit(&H, m)) return 2;
  const float sx = 220.f, sy = 3.5f, sz = 172.f, sh = 1.5708f;
  float res5 = -1, res4 = -1, res4y = -99, res4a = 99;
  // ---------------- Г5: своя стоящая машина
  {
    carInit(&car, NULL, &world, sx, sy, sz, sh); for (int i = 0; i < 90; i++) { CarInput ci = {0, 0, 0, 1}; carUpdate(&car, &world, &ci, DT); }
    HomerCar hc; homerCarFrom(&hc, &car, .62f);
    float fx = sinf(car.h), fz = cosf(car.h);
    H.solid = 0; H.solidUser = 0; homerRideFn = 0;
    homerPlace(&H, &world, car.x - fx * 1.6f, hc.roof + .05f, car.z - fz * 1.6f, car.h); H.y = hc.roof + .1f; H.grounded = 0; H.vy = 0.f; H.safe[1] = H.y;   // на крышу у заднего края (homerPlace прижимает к земле)
    HomerInput in = in0(car.h);
    for (int i = 0; i < 60; i++) { homerCarFrom(&hc, &car, .62f); homerUpdate(&H, &world, &hc, &in, DT); homerPose(&H); { CarInput ci = {0, 0, 0, 1}; carUpdate(&car, &world, &ci, DT); } }
    int onCar = H.onCar; float x0 = H.x, z0 = H.z; in.my = 1.f;
    for (int i = 0; i < 60; i++) { homerCarFrom(&hc, &car, .62f); homerUpdate(&H, &world, &hc, &in, DT); homerPose(&H); { CarInput ci = {0, 0, 0, 1}; carUpdate(&car, &world, &ci, DT); } }
    res5 = hypotf(H.x - x0, H.z - z0); printf("g5_onCar %d\n", onCar);
  }
  // ---------------- Г4: машина трафика едет и поворачивает, Гомер на её крыше без ввода
  {
    memset(&T, 0, sizeof T); static TrHeader dummy; T.hd = &dummy; T.maxCars = 1; T.car[0].mode = TRM_DRIVE; T.npark = 0;
    Car *k = &T.car[0].car; carInit(k, NULL, &world, sx + 20.f, sy, sz, sh); for (int i = 0; i < 90; i++) { CarInput ci = {0, 0, 0, 1}; carUpdate(k, &world, &ci, DT); }
    HomerCar none; memset(&none, 0, sizeof none);
    float fx = sinf(k->h), fz = cosf(k->h), roof = k->y + k->p.bodyHi * cosf(k->pitch) * cosf(k->roll);
    H.solid = solidT; H.solidUser = &T; homerRideFn = rideT; homerRideUser = &T;
    homerPlace(&H, &world, k->x - fx * .5f, roof + .05f, k->z - fz * .5f, k->h); H.y = roof + .1f; H.grounded = 0; H.vy = 0.f; H.safe[1] = H.y;
    HomerInput in = in0(k->h);
    for (int i = 0; i < 60; i++) { homerUpdate(&H, &world, NULL, &in, DT); homerPose(&H); }
    int riding = fabsf(H.y - roof) < .1f; printf("g4_standing_on_roof %d\n", riding);
    float v = 0.f, h = k->h; CarBox b0; carBox(k, &b0); float a0 = (H.x - b0.x) * b0.fx + (H.z - b0.z) * b0.fz;
    for (int i = 0; i < 150; i++) {   // 2,5 с: разгон до 5 м/с и поворот 0,25 рад/с, машина двигается сама (трафик)
      v = fminf(5.f, v + 3.f * DT); h += .25f * DT; k->h = h; k->x += sinf(h) * v * DT; k->z += cosf(h) * v * DT;
      homerUpdate(&H, &world, NULL, &in, DT); homerPose(&H);
    }
    CarBox b; carBox(k, &b); roof = k->y + k->p.bodyHi * cosf(k->pitch) * cosf(k->roll);
    float dx = H.x - b.x, dz = H.z - b.z, a = dx * b.fx + dz * b.fz, s = -dx * b.fz + dz * b.fx;
    res4a = hypotf(a - a0, s); res4y = H.y - roof; res4 = (fabsf(H.y - roof) < .15f && res4a < .5f) ? 1.f : 0.f;
    printf("g4_car_frame_drift_m %.3f\ng4_y_minus_roof %.3f\n", res4a, res4y);
  }
  printf("g5_path_m %.3f\ng4_stays_on_roof %d\n", res5, (int)res4);
  return 0;
}
