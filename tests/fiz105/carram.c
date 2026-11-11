// fiz105: наезд игрока на стоящую машину трафика под 15°/45°/90° к её оси на 40 и 80 км/ч, плоский мир. Тот же carHitCar и carUpdate, что в игре (трафик: просыпается и катится сам).
// Печать: потеря скорости игрока за 0,5 с после удара, боковое смещение стоявшей за 0,5 с и за 1 с (м), её скорость сразу после удара, полный отлёт, расстояние между центрами (мин.), кто сильнее сдвинулся.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static uint32_t colBuf[1024];
static int flat(ColWorld *W) {
  unsigned char *b = (unsigned char *)colBuf; memset(colBuf, 0, sizeof colBuf);
  ColHdr h; memset(&h, 0, sizeof h); memcpy(h.magic, "COL1", 4); h.ox = -300.f; h.oz = -300.f; h.cell = 600.f; h.W = h.H = 1; h.ntri = 2; h.nref = 2;
  memcpy(b, &h, 48);
  ColTri t[2] = {{{-300, 0, -300}, {-300, 0, 300}, {300, 0, 300}, {0, 1, 0}}, {{-300, 0, -300}, {300, 0, 300}, {300, 0, -300}, {0, 1, 0}}};
  memcpy(b + 48, t, sizeof t); uint32_t cs[2] = {0, 2}, refs[2] = {0, 1}; memcpy(b + 48 + 96, cs, 8); memcpy(b + 48 + 96 + 8, refs, 8);
  return colInit(W, b, 48 + 96 + 16);
}
int main(int argc, char **argv) {
  const char *tn = argc > 1 ? argv[1] : "minivana"; float DT = (argc > 2 ? (float)atof(argv[2]) : 16.667f) / 1000.f; if (DT > .05f) DT = .05f; /* длина кадра, мс (игра режет кадр до 50 мс) */ static ColWorld W; if (flat(&W)) { puts("мир"); return 2; }
  static const float ang[] = {15, 45, 90}, kmh[] = {40, 80};
  for (int ki = 0; ki < 2; ki++) for (int ai = 0; ai < 3; ai++) {
    float a = ang[ai] * 0.0174533f, v0 = kmh[ki] / 3.6f;
    CarParams pt; if (!carParamsFor(tn, &pt)) carParamsFor("famil_v", &pt);
    Car t, p; carInit(&t, &pt, &W, 0.f, 0.f, 0.f, 0.f);
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 1.f}; carUpdate(&t, &W, &in, DT); }   // ручник
    carInit(&p, NULL, &W, -14.f * sinf(a), 0.f, -14.f * cosf(a), a); p.huskDrive = 1;
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 0}; carUpdate(&p, &W, &in, DT); }
    p.vel[0] = sinf(a) * v0; p.vel[2] = cosf(a) * v0; p.v = v0;
    float tx0 = t.x, tz0 = t.z, dmin = 1e9f, vt_hit = 0, vp_hit = 0, vp05 = -1, dl05 = -1, dl1 = -1, dfin = 0; int hit = -1;
    int NF = (int)(6.f / DT), n05 = (int)(.5f / DT + .5f), n1 = (int)(1.f / DT + .5f); float jumpT = 0, jumpP = 0;
    for (int f = 0; f < NF; f++) {
      CarInput ip = {hit >= 0 && f < hit + n1 ? 1.f : 0.f, 0.f, 0, 0}, it = {0, 0, 0, 1.f};
      float vpb = hypotf(p.vel[0], p.vel[2]);
      carUpdate(&p, &W, &ip, DT); carUpdate(&t, &W, &it, DT);
      float tx_ = t.x, tz_ = t.z, px_ = p.x, pz_ = p.z; (void)tx_; (void)tz_; (void)px_; (void)pz_;
      float dx = p.x - t.x, dz = p.z - t.z; if (hypotf(dx, dz) < dmin) dmin = hypotf(dx, dz);
      float bx = t.x, bz = t.z, qx = p.x, qz = p.z; int hh = carHitCar(&p, &t, &W); if (hh) { float jt = hypotf(t.x - bx, t.z - bz), jp = hypotf(p.x - qx, p.z - qz); if (jt > jumpT) jumpT = jt; if (jp > jumpP) jumpP = jp; }
      if (hh && hit < 0) { hit = f; vp_hit = vpb * 3.6f; vt_hit = hypotf(t.vel[0], t.vel[2]) * 3.6f; }
      if (hit >= 0) { float lat = hypotf(t.x - tx0, t.z - tz0); if (f == hit + n05) { dl05 = lat; vp05 = hypotf(p.vel[0], p.vel[2]) * 3.6f; } if (f == hit + n1) dl1 = lat; dfin = lat; }
    }
    printf("[%.0f мс] %2.0f° %2.0f км/ч: скачок за кадр: стоявшая %.2f м, игрок %.2f м; удар на %.0f км/ч, игрок через 0.5 с %.0f км/ч (-%.0f%%), стоявшая: сразу %.0f км/ч, смещена за 0.5 с %.1f м, за 1 с %.1f м, всего %.1f м, мин. расстояние %.1f м\n", DT * 1000.f, ang[ai], kmh[ki], jumpT, jumpP, vp_hit, vp05, vp_hit > 0 ? 100.f * (1.f - vp05 / vp_hit) : 0.f, vt_hit, dl05, dl1, dfin, dmin);
  }
  return 0;
}
