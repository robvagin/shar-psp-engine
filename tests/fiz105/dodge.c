// fiz105: уворот NPC (svDodge, traffic.c): те же команды, что даёт трафик: 0,8 с полный газ + руль в сторону + предел колёс 120°, потом ручник на кадр, потом катится.
// Печать по NPC на 35/50 км/ч: боковое смещение (м) и поворот курса (°) к концу уворота, через 1,5 и 3 с; макс. угол между курсом кузова и вектором скорости (°); макс. боковая скорость (м/с); остановилась ли.
// Параметры: dodge MODEL [WHEEL_OVR_RAD]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static uint32_t colBuf[1024];
static int flat(ColWorld *W) {
  unsigned char *b = (unsigned char *)colBuf; memset(colBuf, 0, sizeof colBuf);
  ColHdr h; memset(&h, 0, sizeof h); memcpy(h.magic, "COL1", 4); h.ox = -400.f; h.oz = -400.f; h.cell = 800.f; h.W = h.H = 1; h.ntri = 2; h.nref = 2;
  memcpy(b, &h, 48);
  ColTri t[2] = {{{-400, 0, -400}, {-400, 0, 400}, {400, 0, 400}, {0, 1, 0}}, {{-400, 0, -400}, {400, 0, 400}, {400, 0, -400}, {0, 1, 0}}};
  memcpy(b + 48, t, sizeof t); uint32_t cs[2] = {0, 2}, refs[2] = {0, 1}; memcpy(b + 48 + 96, cs, 8); memcpy(b + 48 + 96 + 8, refs, 8);
  return colInit(W, b, 48 + 96 + 16);
}
static float wrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
int main(int argc, char **argv) {
  const char *mn = argc > 1 ? argv[1] : "famil_v"; float ovr = argc > 2 ? (float)atof(argv[2]) : 2.0944f; static ColWorld W; if (flat(&W)) return 2;
  static const float kmh[] = {35, 50}; const float DT = 1.f / 60.f;
  for (int ki = 0; ki < 2; ki++) for (int side = -1; side <= 1; side += 2) {
    CarParams p; if (!carParamsFor(mn, &p)) carParamsFor("famil_v", &p);
    Car c; carInit(&c, &p, &W, 0.f, 0.f, 0.f, 0.f);
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 1.f}; carUpdate(&c, &W, &in, DT); }
    float v0 = kmh[ki] / 3.6f; c.vel[2] = v0; c.v = v0;
    float t = 0, dodgeT = .8f, ang = 0, vlat = 0, x08 = 0, h08 = 0, x15 = 0, h15 = 0, x3 = 0, h3 = 0, z08 = 0; int hb = 0;
    for (int f = 0; f < 60 * 3 + 1; f++) {
      CarInput in = {0.f, 0.f, 0.f, 1.f};
      if (dodgeT > 0.f) { in.throttle = 1.f; in.handbrake = 0.f; in.steer = (float)side * 1.f; c.wheelMaxOvr = ovr; if ((dodgeT -= DT) <= 0.f) hb = 1; }
      else { c.wheelMaxOvr = 0.f; if (hb) { in.handbrake = 1.f; hb = 0; } else in.handbrake = 0.f; }
      carUpdate(&c, &W, &in, DT); t += DT;
      float sp = hypotf(c.vel[0], c.vel[2]);
      if (sp > 1.f) { float a = fabsf(wrap(atan2f(c.vel[0], c.vel[2]) - c.h)) * 57.2958f; if (a > ang) ang = a; float sl = fabsf(c.vel[0] * cosf(c.h) - c.vel[2] * sinf(c.h)); if (sl > vlat) vlat = sl; }
      if (f == 47) { x08 = c.x; h08 = wrap(c.h) * 57.2958f; z08 = c.z; }
      if (f == 89) { x15 = c.x; h15 = wrap(c.h) * 57.2958f; }
      if (f == 179) { x3 = c.x; h3 = wrap(c.h) * 57.2958f; }
    }
    printf("%s %2.0f км/ч руль %+d (предел колёс %.0f°): к 0,8 с боковой уход %+.1f м (вперёд %.1f), курс %+.0f°; к 1,5 с %+.1f м, %+.0f°; к 3 с %+.1f м, %+.0f°; макс угол кузов/скорость %.0f°, макс боковая скорость %.1f м/с, крен %.0f°\n", mn, kmh[ki], side, ovr * 57.2958f, x08, z08, h08, x15, h15, x3, h3, ang, vlat, fabsf(c.roll) * 57.2958f);
  }
  return 0;
}
