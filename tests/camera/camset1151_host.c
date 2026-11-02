// №1151: настройки камеры игрока (viewer/camuser.h) — чистые законы на Маке.
//   cc -std=c11 -I viewer tests/camera/camset1151_host.c -lm -o /tmp/camset1151 && /tmp/camset1151      Красный контроль: -DCAMSET1151_OLD=1 (настроек нет — проверки падают)
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "camuser.h"
static int bad;
#define CHK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); bad++; } else printf("ok   %s\n", m); } while (0)
int main(void) {
  // пределы: любое значение безопасно
  float hs[] = {-5.f, 0.f, .2f, .5f, 1.f, 1.5f, 3.f, 1e9f, NAN, INFINITY, -INFINITY};
  int okH = 1, okD = 1, okT = 1, okF = 1;
  for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) { float h = camUserH(hs[i]), d = camUserD(hs[i]), t = camUserTiltWant(hs[i]), f = camUserFovWant(hs[i]);
    if (!(h >= CAMU_H_LO && h <= CAMU_H_HI)) okH = 0; if (!(d >= CAMU_D_LO && d <= CAMU_D_HI)) okD = 0; if (!(t >= -CAMU_TILT_MAX && t <= CAMU_TILT_MAX)) okT = 0; if (!(f >= CAMU_FOV_LO && f <= CAMU_FOV_HI)) okF = 0; }
  CHK(okH && okD && okT && okF, "любое значение (в том числе не число и бесконечность) зажимается в безопасные пределы");
  CHK(camUserH(1.f) == 1.f && camUserD(1.f) == 1.f && camUserTiltWant(0.f) == 0.f && camUserFovWant(1.f) == 1.f, "значения по умолчанию = нынешняя камера (1 / 1 / 0 / 1)");
  CHK(camUserH(.75f) == .75f && camUserD(1.15f) == 1.15f && camUserTiltWant(-5.f) == -5.f && camUserFovWant(.92f) == .92f, "ступени меню проходят без изменений");
  // плавность
  { float now = 0.f, mx = 0.f; int n = 0; for (; now < 10.f && n < 100; n++) { float nx = camUserEase(now, 10.f, CAMU_TILT_RATE, 1.f / 30.f, 0); if (nx - now > mx) mx = nx - now; now = nx; }
    CHK(mx <= CAMU_TILT_RATE / 30.f + 1e-4f && now == 10.f && n >= 7, "наклон 0 → 10° идёт не быстрее 40°/с (шаг ≤ 1,34°, 8 кадров), без рывка");
    CHK(camUserEase(0.f, 10.f, CAMU_TILT_RATE, 0.f, 0) == 10.f && camUserEase(0.f, 10.f, CAMU_TILT_RATE, 1.f / 30.f, 1) == 10.f, "под паузой (dt = 0) и по команде сброса — сразу на месте"); }
  { float now = 1.15f; int n = 0; float mx = 0.f; for (; now > .85f && n < 200; n++) { float nx = camUserEase(now, .85f, CAMU_FOV_RATE, 1.f / 30.f, 0); if (now - nx > mx) mx = now - nx; now = nx; }
    CHK(mx <= CAMU_FOV_RATE / 30.f + 1e-5f && n >= 14, "множитель угла 1,15 → 0,85 идёт не быстрее 0,6/с (0,02 за кадр)"); }
  // угол кадра
  { float base = 62.f; CHK(camUserFovY(base, CAMU_FOV_LO) >= CAMU_FOVY_MIN && camUserFovY(base, CAMU_FOV_HI) <= CAMU_FOVY_MAX && fabsf(camUserFovY(base, 1.f) - base) < 1e-4f, "угол кадра 62° при ×0,85…×1,15 — в 30…80°, ×1 даёт прежний");
    CHK(camUserFovY(70.f, 1.15f) == CAMU_FOVY_MAX && camUserFovY(35.f, .85f) == CAMU_FOVY_MIN, "крайний случай базы: предел 80° / 30°"); }
  // наклон взгляда
  { float eye[3] = {0.f, 3.f, 0.f}, look[3] = {0.f, 3.f, 10.f}, l0[3] = {0.f, 3.f, 10.f};
    camUserTilt(eye, look, 0.f); CHK(look[0] == l0[0] && look[1] == l0[1] && look[2] == l0[2], "наклон 0 — взгляд не меняется");
    camUserTilt(eye, look, 10.f); float L = sqrtf(look[0] * look[0] + (look[1] - 3.f) * (look[1] - 3.f) + look[2] * look[2]), el = atan2f(look[1] - 3.f, look[2]) * 57.2958f;
    CHK(fabsf(L - 10.f) < 1e-3f && fabsf(el - 10.f) < .01f, "наклон +10°: длина взгляда прежняя, подъём ровно 10°");
    float look2[3] = {0.f, 3.f, 10.f}; camUserTilt(eye, look2, -10.f); CHK(look2[1] < 3.f && fabsf(atan2f(3.f - look2[1], look2[2]) * 57.2958f - 10.f) < .01f, "наклон −10°: вниз на 10°");
    float up[3] = {0.f, 13.f, 0.f}, upn[3] = {0.f, 13.f, 0.f}; camUserTilt(eye, up, 10.f); CHK(up[0] == upn[0] && up[2] == upn[2], "почти вертикальный взгляд не трогаем"); }
  // глаз при смене высоты и дальности: положение глаза меняется только на величину множителя, шаг от настроек ограничен ступенями (меню даёт ≤ 0,25 за шаг по высоте)
  { float eyeH = 3.8f, tgtY = 1.4f; float lo = tgtY + (eyeH - tgtY) * camUserH(-100.f), hi = tgtY + (eyeH - tgtY) * camUserH(100.f);
    CHK(lo > tgtY + .5f && hi < tgtY + (eyeH - tgtY) * 1.5f + 1e-4f, "высота глаза famil_v (3,8 м над землёй, цель 1,4) при крайних настройках: 2,6…5,0 м, выше цели во всех случаях"); }
#if CAMSET1151_OLD
  puts("(режим CAMSET1151_OLD=1: настроек нет)");
  CHK(camUserCarHe(1.2f) == 1.2f && camUserCarDe(.8f) == .8f && camUserFootHe(1.3f) == 1.3f, "OLD: машина и пешком читают прежние homerCamUserH/D");
#else
  camUserCarH = 1.5f; camUserCarD = 1.3f; camUserFootH = .5f; camUserFootD = .7f;
  CHK(camUserCarHe(1.f) == 1.5f && camUserCarDe(1.f) == 1.3f && camUserFootHe(1.f) == .5f && camUserFootDe(1.f) == .7f, "машина и пешком читают свои значения, а не общие");
#endif
  puts(bad ? "FAIL" : "PASS"); return bad;
}
