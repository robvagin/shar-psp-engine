// Правка 436 (находка Кадров 07.11: L2, rocke_v на автопилоте «газ в пол» кувыркается): повтор на реальных данных L2. Машина (BW_CARNAME) ставится за 8 м до стены (180.3, 26.2) по курсу 1.17,
// разгоняется до V м/с и держит газ в пол 5 с (руль ARG3 или 0). Печатает наклон/подъём. Выход: наибольший наклон, подъём центра.
//   climb LEVEL PLAYER MSN V [STEER]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
float bwDtNow = 1.f / 60.f;
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 5 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: climb LEVEL PLAYER MSN V [STEER]\n"); return 2; }
  float V = (float)atof(argv[4]), st = argc > 5 ? (float)atof(argv[5]) : 0.f; bwNew(&W);
  float h = getenv("CH") ? (float)atof(getenv("CH")) : 1.17f, wx = 180.3f, wz = 26.2f, px = wx - 8.f * sinf(h), pz = wz - 8.f * cosf(h);
  float gy; colGround(bwCol(), px, pz, 60.f, -1e6f, &gy, 0); float h0 = getenv("H0") ? (float)atof(getenv("H0")) : .3f; bwPlace(&W, px, gy + h0, pz, h);
  bwDtNow = 1.f / 30.f; for (int f = 0; f < (h0 > 1.f ? 0 : 60); f++) bwStep(&W, 0, 0.f, 0.f);
  if (getenv("BOXES")) { const ColWorld *cw = bwCol(); for (unsigned i = 0; i < cw->nbox; i++) { const ColBox *b = &cw->box[i]; if (fabsf(b->c[0] - wx) < 8.f && fabsf(b->c[2] - wz) < 8.f) printf("  коробка %u: c (%.1f,%.1f,%.1f) h (%.2f,%.2f,%.2f) ax (%.2f,%.2f,%.2f) az (%.2f,%.2f,%.2f) верх %.2f\n", i, b->c[0], b->c[1], b->c[2], b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2], b->az[0], b->az[1], b->az[2], b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2]); }
    printf("  земля под стеной: "); float g2; colGround(cw, wx, wz, 60.f, -1e6f, &g2, 0); printf("%.2f; step машины %.2f\n", g2, W.car.p.step); }
  carSetSpeed(&W.car, V); float tmax = 0.f, rise = 0.f, y0 = W.car.y; int verbose = getenv("V") != 0;
  for (int f = 0; f < 150; f++) {
    bwStep(&W, GP_R, st, 0.f); const Car *c = &W.car;
    float upy = 1.f - 2.f * (c->q[1] * c->q[1] + c->q[3] * c->q[3]); float tilt = acosf(fmaxf(-1.f, fminf(1.f, upy))) * 57.29578f;
    if (tilt > tmax) tmax = tilt; if (c->y - y0 > rise) rise = c->y - y0;
    if (verbose && f % 5 == 0) printf("  f%3d v %5.1f x %.1f z %.1f y %.2f тангаж %4.0f крен %4.0f колёс %d нзем %d конт %d тип %d\n", f, c->v, c->x, c->z, c->y - y0, c->pitch * 57.29578f, c->roll * 57.29578f, c->wheels, c->grounded, c->contacts, c->ctype[0]);
  }
  printf("%s V=%.0f руль %.1f: макс наклон %.0f°, макс подъём %.2f м\n", bwCarName(), V, st, tmax, rise); return 0;
}
