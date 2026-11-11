// Правка 1240 (Роб, PSP): причёска Мардж за рулём. Клип сидения ndr кладёт hair_1..3 назад (под закрытую кабину); в открытой машине (HomerCar.openTop,
// бит 16 CAR1 по tools/open_cars.txt) homerPose возвращает причёску в покой. Настоящий homer.c/skin.c на хосте.
//   tests/homer/marge_hair.sh l4_player.shr [l1_player.shr]     красный контроль: -DMARGE_HAIR_OLD=1 даёт FAIL у открытой
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "skin.h"
#include "car.h"
static Homer O, C;   // O — открытая машина, C — закрытая
static void setup(Homer *H, SkinModel *m, int open) {
  if (homerInit(H, m)) { printf("homerInit failed\n"); exit(1); }
  H->rootMode = 1; H->car.ok = 1; H->car.openTop = open && !MARGE_HAIR_OLD; H->clip = H->cInCar; H->clipLoop = 1; H->clipT = 0.f; H->fade = H->fadeLen = 0.f;
  H->x = 10.f; H->y = 1.f; H->z = 5.f; H->h = 0.7f;
}
static float qdeg(const float *a, const float *b) { float d = fabsf(a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]); if (d > 1.f) d = 1.f; return 2.f * acosf(d) * 57.29578f; }
int main(int argc, char **argv) {
  int bad = 0;
  // список открытых машин (tools/open_cars.txt -> carparams.h) и путь homerCarFrom: открытая famil_v / закрытая sedana
  { const char *op[] = {"famil_v", "lisa_v", "smith_v", "witchcar"}, *cl[] = {"sedana", "marge_v", "schoolbu", "carmor", "huska", "nope"};
    for (int i = 0; i < 4; i++) if (!carOpenFor(op[i]) && !MARGE_HAIR_OLD) { printf("FAIL: %s must be open\n", op[i]); bad = 1; }
    for (int i = 0; i < 6; i++) if (carOpenFor(cl[i])) { printf("FAIL: %s must be closed\n", cl[i]); bad = 1; }
    Car k; memset(&k, 0, sizeof k); carParamsFor("famil_v", &k.p); HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &k, 0.f);
    printf("homerCarFrom famil_v: openTop %d (want %d)\n", hc.openTop, !MARGE_HAIR_OLD);
    if (hc.openTop != !MARGE_HAIR_OLD) { printf("FAIL: homerCarFrom does not set openTop for famil_v\n"); bad = 1; }
    carParamsFor("sedana", &k.p); homerCarFrom(&hc, &k, 0.f); if (hc.openTop) { printf("FAIL: sedana openTop\n"); bad = 1; } }
  for (int a = 1; a < argc; a++) {
    SkinModel *m = skinLoad(argv[a]); if (!m) { printf("no %s\n", argv[a]); return 1; }
    setup(&O, m, 1); setup(&C, m, 0); int nj = (int)m->h->nJoints;
    homerPose(&O); homerPose(&C);
    float openMax = 0.f, closedMin = 1e9f, restDiff = 0.f; int other = 0;
    for (int i = 0; i < O.nHair; i++) { int j = O.hair[i]; openMax = fmaxf(openMax, qdeg(O.xf[j].q, m->j[j].q)); closedMin = fminf(closedMin, qdeg(C.xf[j].q, m->j[j].q)); }
    for (int j = 0, h; j < nj; j++) { for (h = 0; h < O.nHair && O.hair[h] != j; h++); if (h < O.nHair) continue;   // не причёска: открытая == закрытая побитно
      if (memcmp(&O.xf[j], &C.xf[j], sizeof(SkinXf))) other++; }
    (void)restDiff;
    printf("%s: hair joints %d; open car: max angle from rest %.2f deg, closed car: min angle from rest %.1f deg; other joints different: %d\n", argv[a], O.nHair, openMax, O.nHair ? closedMin : 0.f, other);
    if (other) { printf("FAIL: non-hair joints differ between open and closed\n"); bad = 1; }
    if (O.nHair) {
      if (openMax > 0.5f) { printf("FAIL: open car — hair not in rest pose\n"); bad = 1; }
      if (closedMin < 5.f) { printf("FAIL: closed car — hair no longer bent like the original clip\n"); bad = 1; }
    }
  }
  if (!bad) printf("PASS\n"); return bad;
}
