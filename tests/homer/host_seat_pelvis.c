// Правка 1242: где таз героя за рулём относительно сиденья машины (сустав dl) — настоящий homer.c. Один и тот же клип сидения ndr, якорь посадки как в игре
// (seatA машины, высота = −lift + seatLift). Печатает таз в системе кузова: x вбок, y над землёй, z вперёд. Сравнить Гомера (l1) и остальных.
//   tests/homer/seat_pelvis.sh l1_player.shr l4_player.shr ...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "skin.h"
#include "car.h"
static Homer H;
int main(int argc, char **argv) {
  for (int a = 1; a < argc; a++) {
    SkinModel *m = skinLoad(argv[a]); if (!m) { printf("no %s\n", argv[a]); return 1; }
    memset(&H, 0, sizeof H); if (homerInit(&H, m)) { printf("homerInit failed\n"); return 1; }
    Car k; memset(&k, 0, sizeof k); carParamsFor("famil_v", &k.p); k.p.seatData = 1;   // сиденье famil_v: dl (−0,437; −0,186; 0,174), lift 0,685
    HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &k, 0.685f);
    float M[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; memcpy(hc.m, M, sizeof M);
    homerSetInCar(&H, &hc); H.fade = H.fadeLen = 0.f;
    homerPose(&H);
    int pj = skinFindJoint(m, "Pelvis"); const float *P = (const float *)&H.place; const ScePspFVector4 *q = &H.world[pj].w;
    float x = P[0]*q->x + P[4]*q->y + P[8]*q->z + P[12], y = P[1]*q->x + P[5]*q->y + P[9]*q->z + P[13], z = P[2]*q->x + P[6]*q->y + P[10]*q->z + P[14];
    printf("%s: pelvis in car frame x %.3f y %.3f z %.3f (seat dl x %.3f y %.3f z %.3f)\n", argv[a], x, y, z, k.p.seat[0], k.p.seat[1] + 0.685f, k.p.seat[2]);
  }
  return 0;
}
