// fiz105: наезд на столб (ColCyl r 0,25) / ствол (r 0,6) на 20/40/80 км/ч, лоб и скользом (смещение 0,5 м), при длине кадра DT мс. Печать: потеря скорости за 0,5 с, боковой уход игрока за 0,5 с, макс. скачок позиции за кадр (м), прошёл ли насквозь.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static uint32_t colBuf[2048];
static int world(ColWorld *W, float r) {
  unsigned char *b = (unsigned char *)colBuf; memset(colBuf, 0, sizeof colBuf);
  ColHdr h; memset(&h, 0, sizeof h); memcpy(h.magic, "COL1", 4); h.ox = -300.f; h.oz = -300.f; h.cell = 600.f; h.W = h.H = 1; h.ntri = 2; h.ncyl = 1; h.nref = 3;
  memcpy(b, &h, 48);
  ColTri t[2] = {{{-300, 0, -300}, {-300, 0, 300}, {300, 0, 300}, {0, 1, 0}}, {{-300, 0, -300}, {300, 0, 300}, {300, 0, -300}, {0, 1, 0}}};
  ColCyl cy; memset(&cy, 0, sizeof cy); cy.c[0] = 0; cy.c[1] = 2.f; cy.c[2] = 0; cy.axis[1] = 1.f; cy.r = r; cy.halfLen = 2.f;
  memcpy(b + 48, t, sizeof t); memcpy(b + 48 + sizeof t, &cy, sizeof cy);
  uint32_t cs[2] = {0, 3}, refs[3] = {0, 1, (2u << 29) | 0u};   // тип в старших 3 битах: 0 треугольник, 1 ящик, 2 цилиндр, 3 отрезок
  memcpy(b + 48 + sizeof t + sizeof cy, cs, 8); memcpy(b + 48 + sizeof t + sizeof cy + 8, refs, 12);
  return colInit(W, b, 48 + sizeof t + sizeof cy + 20);
}
int main(int argc, char **argv) {
  float DT = (argc > 1 ? (float)atof(argv[1]) : 16.667f) / 1000.f; if (DT > .05f) DT = .05f;
  static const float rr[] = {0.25f, 0.6f}, kmh[] = {20, 40, 80}, off[] = {0.f, 0.5f};
  for (int ri = 0; ri < 2; ri++) for (int ki = 0; ki < 3; ki++) for (int oi = 0; oi < 2; oi++) {
    static ColWorld W; int rc_ = world(&W, rr[ri]); if (rc_) { printf("мир %d\n", rc_); return 2; }
    Car c; carInit(&c, NULL, &W, off[oi], 0.f, -14.f, 0.f); c.huskDrive = 1;
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 0}; carUpdate(&c, &W, &in, DT); }
    float v0 = kmh[ki] / 3.6f; c.vel[2] = v0; c.v = v0;
    int NF = (int)(5.f / DT), n05 = (int)(.5f / DT + .5f), hit = -1; float vh = 0, vprev = kmh[ki], v05 = -1, xhit = 0, x05 = 0, jump = 0, zmax = -99;
    for (int f = 0; f < NF; f++) {
      float px = c.x, pz = c.z; CarInput in = {1.f, 0, 0, 0}; carUpdate(&c, &W, &in, DT);
      float sp = hypotf(c.vel[0], c.vel[2]) * 3.6f;
      if (hit < 0 && c.z > -3.4f) { hit = f; vh = vprev; xhit = c.x; }
      vprev = sp;
      if (hit >= 0) { float j = hypotf(c.x - px, c.z - pz) - hypotf(c.vel[0], c.vel[2]) * DT; if (fabsf(j) > jump) jump = fabsf(j); if (f == hit + n05) { v05 = sp; x05 = c.x - xhit; } }
      if (c.z > zmax) zmax = c.z;
    }
    printf("[%.0f мс] r=%.2f %2.0f км/ч смещение %.1f: удар %.0f км/ч, через 0.5 с %.0f км/ч, боковой уход %+.2f м, скачок позиции за кадр %.2f м, %s\n", DT * 1000.f, rr[ri], kmh[ki], off[oi], vh, v05, x05, jump, zmax > 2.2f ? "ПРОШЁЛ НАСКВОЗЬ" : "держит");
  }
  return 0;
}
