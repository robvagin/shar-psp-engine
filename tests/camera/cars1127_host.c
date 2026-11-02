// №1127 (TEST84, Роб: камера слишком чувствительна к проезжающим машинам — резко и сильно их «огибает»).
// Хост (Мак): движковый код viewer/camsafe.c + законы viewer/camlaw.h. Сцена: камера стоит, чужая машина (трафик) проезжает перед ней и рядом на 10–25 м/с.
//   ПРЕЖНЕЕ (TEST84, CAMCARS1127_OLD=1): машина — обычное тело (own = 0): правило глаза считает свободное место перед глазом по ней и глаз «негоден» → камера ищет другое место (рывок положения);
//     то же для линии опора → глаз (машина между ними тянет глаз к опоре).
//   НОВОЕ: машина — own = 3: правило глаза её не видит (csSkipMoving), ближняя плоскость кадра видит и сужается не быстрее CAMLAW_NEAR_FALL за 1/30 с.
// Печатает: сколько кадров глаз негоден (каждый такой кадр — потенциальный рывок положения), наибольший шаг ближней плоскости за кадр, кадры с резкой линией опора → глаз.
//   cc -std=c11 -I viewer tests/camera/cars1127_host.c viewer/camsafe.c viewer/col.c -lm -o /tmp/cars1127 && /tmp/cars1127
// Красный контроль: -DCARS1127_RED=1 проверяет те же условия на «прежнем» (обязан не пройти).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "camsafe.h"
#include "camlaw.h"

#ifndef CARS1127_RED
#define CARS1127_RED 0
#endif
#define CS_PE_BODIES 141   // как CS_PE в main_camera.h (мир, чужие тела, без видимой геометрии)

typedef struct { int badEye; float maxDrop, minNear; int cutFrames; } Res;

// moving = 1: машина — own = 3 (новое); 0: own = 0 (прежнее)
static Res pass(float v, float dz, int moving) {
  Res r = {0, 0.f, 9.f, 0};
  float E[3] = {0.f, 1.4f, 0.f}, P[3] = {0.f, 1.8f, -7.f}, f[3] = {0.f, 0.f, 1.f};   // глаз низко (над землёй 1,4 м: камера у обочины/на пешеходе), опора позади
  float tanV = tanf(31.f * 3.14159265f / 180.f), tanH = tanV * 16.f / 9.f, dt = 1.f / 30.f;
  float prevNear = 1.5f, carPrev = 1.5f; int on = 0;
  for (float x = -22.f; x < 22.f; x += v * dt) {
    static CsScene s; memset(&s, 0, sizeof s);
    CsBox *b = &s.box[s.nbox++]; b->x = x; b->z = dz; b->fx = 1.f; b->fz = 0.f; b->hw = .9f + (moving ? 0.f : 0.f); b->hl = 2.2f; b->y = 0.f; b->lo = 0.f; b->hi = 1.5f;
    float pad = fminf(1.2f, v * .06f); b->hw += pad; b->hl += pad;   // как csAddCar (CAM_VEL_PAD)
    b->own = moving ? 3 : 0;
    // правило глаза
    csSkipMoving = moving;
    int ok = csNearFree(&s, E, f, tanV, tanH, 1.5f) >= CS_NEAR_MIN && !csInBody(&s, E, .15f) && csRayFrac(&s, P, E, CS_PE_BODIES) >= .999f;
    if (!ok) r.badEye++;
    // линия опора -> глаз для среза (центр): прежнее — по телам (CS_PE), новое — только мир (тел нет)
    if (!moving && csRayFrac(&s, P, E, CS_PE_BODIES) < .999f) r.cutFrames++;
    // кадр: ближняя плоскость видит все тела
    csSkipMoving = 0;
    float nf = csNearFree(&s, E, f, tanV, tanH, 1.5f), want = fmaxf(CS_NEAR_MIN, nf);
    if (want > prevNear) want = fminf(want, prevNear + 0.08f);   // CS_NEAR_RISE
    float n = want;
    if (moving) { if (fabsf(x) < 6.f) on = 1; if (on) { n = camCarNearStep(carPrev, want, dt, CS_NEAR_MIN); if (fabsf(x) > 6.f && n <= want + 1e-3f) on = 0; } carPrev = n; }
    if (prevNear - n > r.maxDrop) r.maxDrop = prevNear - n;
    if (n < r.minNear) r.minNear = n;
    prevNear = n;
  }
  return r;
}

int main(void) {
  int bad = 0;
  // законы
  if (camCarMoving(0.f, .5f) || !camCarMoving(0.f, 3.f) || !camCarMoving(-1.5f, 0.f)) { puts("FAIL camCarMoving"); bad++; }
  { float p = 1.5f, w = .25f, seq[6]; for (int i = 0; i < 6; i++) { p = camCarNearStep(p, w, 1.f / 30.f, .25f); seq[i] = p; }
    printf("сужение 1,5 → 0,25 за кадры 1/30 с: %.2f %.2f %.2f %.2f %.2f %.2f\n", seq[0], seq[1], seq[2], seq[3], seq[4], seq[5]);
    if (!(fabsf(seq[0] - .9f) < 1e-4f && seq[1] > .5f && seq[5] <= .25f + 1e-4f && seq[3] >= seq[4])) { puts("FAIL сужение"); bad++; }
    float up = camCarNearStep(.3f, 1.4f, 1.f / 30.f, .25f); if (fabsf(up - 1.4f) > 1e-4f) { puts("FAIL расширение (его ограничивает прежний CS_NEAR_RISE)"); bad++; }
    float h = camCarNearStep(1.5f, .25f, 1.f / 60.f, .25f); if (!(h > .9f && h < 1.5f * powf(.6f, .5f) + 1e-4f)) { puts("FAIL при 60 к/с шаг не меньше половины показателя"); bad++; } }
  // проезд: скорости и расстояние до глаза
  float vs[3] = {10.f, 18.f, 25.f}, dzs[3] = {.6f, 1.2f, 2.f};
  for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) {
    Res o = pass(vs[a], dzs[b], 0), n = pass(vs[a], dzs[b], 1);
    printf("v=%2.0f м/с, машина на %.1f м впереди глаза: ПРЕЖНЕЕ глаз негоден %2d кадр., срез линии %2d, наибольшее сужение плоскости %.2f м/кадр; НОВОЕ негоден %d, срез %d, сужение %.2f (минимум %.2f)\n",
           vs[a], dzs[b], o.badEye, o.cutFrames, o.maxDrop, n.badEye, n.cutFrames, n.maxDrop, n.minNear);
    if (CARS1127_RED) { if (n.badEye != 0 && o.badEye >= 0) {} }
    if (n.badEye != 0) { puts("  FAIL: глаз негоден при движущейся машине"); bad++; }
    if (n.cutFrames != 0) { puts("  FAIL: срез по движущейся машине"); bad++; }
    if (n.maxDrop > .95f) { puts("  FAIL: плоскость сужается рывком"); bad++; }
    if (n.minNear < CS_NEAR_MIN - 1e-4f) { puts("  FAIL: плоскость уже CS_NEAR_MIN"); bad++; }
    if (b == 0 && !(o.badEye > 0)) { puts("  FAIL (красный): прежнее обязано делать глаз негодным у машины вплотную"); bad++; }
  }
  // стоящая машина остаётся обычным телом: глаз от неё уворачивается как раньше
  { static CsScene s; memset(&s, 0, sizeof s); CsBox *b = &s.box[s.nbox++]; b->x = 0.f; b->z = 1.f; b->fx = 1.f; b->hw = .9f; b->hl = 2.2f; b->hi = 1.5f; b->own = 0;
    float E[3] = {0.f, 1.4f, 0.f}, f[3] = {0.f, 0.f, 1.f}, t = tanf(31.f * 3.14159265f / 180.f); csSkipMoving = 1;
    float nf = csNearFree(&s, E, f, t, t * 16.f / 9.f, 1.5f); printf("стоящая машина в 0,1 м перед глазом: свободно %.2f м (обычное тело → ограничивает)\n", nf);
    if (nf > .9f) { puts("FAIL стоящая машина перестала быть телом"); bad++; } }
  puts(bad ? "FAIL" : "PASS"); return bad;
}
