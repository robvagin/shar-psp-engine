// №408: камеры зон города для машины — см. carzone.h. Чистый C99.
#include <math.h>
#include <string.h>
#include "carzone.h"

static int bodyIn(const RoomCam *c, const float (*body)[3], int nbody) {
  for (int k = 0; k < c->ntrig; k++)
    for (int i = 0; i < nbody; i++) if (roomTrigIn(&c->trig[k], body[i][0], body[i][1], body[i][2], 0.f)) return 1;
  return 0;
}
float carZoneSpeed(const float velocity[3]) {   // скорость входа в рельс: с вертикальной составляющей (native70 PS2: 19,847 по XZ, 20,067 по XYZ)
#ifndef CARZONE_SPEED_OLD
  return sqrtf(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
#else
  return hypotf(velocity[0], velocity[2]);   // CARZONE_SPEED_OLD: прежнее, только XZ
#endif
}
static float lagK(float lag, float dt) {   // как viewer/roomcam.c: доля пути за dt при запаздывании lag на кадр 1/60 с
  if (lag >= 1.f) return 1.f; if (lag <= 0.f) return 0.f;
  return 1.f - powf(1.f - lag, dt * 60.f);
}

// сверка g06 (02.11.2026, g06-camera:9,12,20,37; P08 Д2, Д5, P18-02): камера зоны для машины по данным оригинала. Допуск (биты данных, «только для машины» — скорость не меньше 20 м/с по
// скорости центра масс) проверяется КАЖДЫЙ кадр, пока кузов касается объёма; включившаяся камера держится, пока кузов касается объёма, даже если скорость упала. Касание кузова —
// сфера 1,5 м вокруг центра машины или любая из точек коробки кузова. Вход в объём включает камеру сразу, срез. Статичная: авто-зум по расстоянию до машины (10…25 м, 0,022), рельс — максимальный
// угол. CARZONE_SV_OLD=1 — прежнее правило (рельс только на входе, середина кузова, порог 17 м/с, угол из данных)
#ifndef CARZONE_SV_OLD
#define CARZONE_SV_OLD 0
#endif
#ifndef CARZONE_RAILN_OLD
#define CARZONE_RAILN_OLD 0
#endif
static int carZoneStepSv(const RoomCam *c, int n, CarZoneState *s, const float (*body)[3], int nbody, const float ref[3], float speed,
                         float dt, float eye[3], float look[3], float *fovY) {
  if (n > 32) n = 32;
  unsigned now = 0;
  for (int k = 0; k < n; k++) {
    int hit = bodyIn(&c[k], body, nbody);
    if (!hit && nbody > 0) for (int q = 0; q < c[k].ntrig && !hit; q++) hit = roomTrigIn(&c[k].trig[q], body[0][0], body[0][1], body[0][2], 1.5f);
    if (hit) now |= 1u << k;
  }
  unsigned entered = now & ~s->in; s->in = now;
  s->held &= now;
  unsigned ok = 0;
  for (int k = 0; k < n; k++) if ((now & (1u << k)) && ((s->held & (1u << k)) || roomCamAllows(&c[k], 1, speed))) ok |= 1u << k;
  int i = -1;
  for (int k = 0; k < n && i < 0; k++) if ((entered & ok & (1u << k)) && k != s->cur) i = k;   // вошла — берёт кадр сразу
  if (i < 0) {
    if (s->cur >= 0 && s->cur < n && (ok & (1u << s->cur))) i = s->cur;
    else for (int k = 0; k < n; k++) if (ok & (1u << k)) { i = k; break; }
  }
  if (i < 0) { s->cur = -1; s->held = 0; return -1; }
  s->held |= 1u << i;
  const RoomCam *m = &c[i];
  float e[3], l[3];
  if (m->type == 1) { s->t = roomRailParamDt(m, i == s->cur ? s->t : -1.f, ref, dt); roomRailPose(m, s->t, ref, e, l); }
  else { memcpy(e, m->pos, 12); if (m->follow) for (int k = 0; k < 3; k++) l[k] = ref[k] + m->target[k]; else memcpy(l, m->target, 12); }
  if (i != s->cur) { memcpy(s->eye, e, 12); memcpy(s->look, l, 12); s->zoomH = 0.f; }
  else { float kp = m->type == 1 ? lagK(m->posLag, dt) : 1.f, kt = lagK(m->targetLag, dt);
    for (int k = 0; k < 3; k++) { s->eye[k] += (e[k] - s->eye[k]) * kp; s->look[k] += (l[k] - s->look[k]) * kt; } }
  s->cur = i; memcpy(eye, s->eye, 12); memcpy(look, s->look, 12);
  float dx = s->eye[0] - ref[0], dy = s->eye[1] - ref[1], dz = s->eye[2] - ref[2];
  *fovY = roomCamFovCar(m, sqrtf(dx * dx + dy * dy + dz * dz), &s->zoomH, dt);
  return i;
}
int carZoneStep(const RoomCam *c, int n, CarZoneState *s, const float (*body)[3], int nbody, const float ref[3], float speed,
                float dt, float eye[3], float look[3], float *fovY) {
  if (!CARZONE_SV_OLD) return carZoneStepSv(c, n, s, body, nbody, ref, speed, dt, eye, look, fovY);
#ifdef CARZONE_OFF   // служебное: красный контроль tests/camera/carzone_ps2.py --old (камер зон у машины нет, как до №408)
  (void)c; (void)body; (void)nbody; (void)ref; (void)speed; (void)dt; (void)eye; (void)look; (void)fovY; s->cur = -1; return n < 0 ? n : -1;
#endif
  if (n > 32) n = 32;
  unsigned now = 0;
  for (int k = 0; k < n; k++) if (bodyIn(&c[k], body, nbody)) now |= 1u << k;
  unsigned entered = now & ~s->in;
  for (int k = 0; k < n; k++)   // рельс включается только на входе и только быстрой машиной (порог — carzone.h)
    if ((entered & (1u << k)) && c[k].type == 1 && speed < CARZONE_RAIL_MINV && (CARZONE_RAILN_OLD || ((int)(c[k].axisPlay[2] + .5f) & 1))) s->skip |= 1u << k;   // сверка g12-data круг 6 (P18-02): порог скорости только у рельса «только машина» (бит 1); нейтральный рельс включается на любой скорости. CARZONE_RAILN_OLD=1 — порог у всех
#if !CARZONE_PS2_OLD
  // №378: рельс держит, пока в объёме середина кузова; вошла и вышла — рельс выключен до выхода всего кузова (carzone.h, В95)
  unsigned mid = 0;
  for (int k = 0; k < n; k++)
    if (c[k].type == 1 && (now & (1u << k)) && bodyIn(&c[k], body, nbody > 0 ? 1 : 0)) mid |= 1u << k;
  s->mid = (s->mid | mid) & now;
  s->skip |= s->mid & ~mid;
#endif
  s->skip &= now; s->in = now;
  unsigned ok = now & ~s->skip;
  int i = -1;
#if !CARZONE_PS2_OLD
  // №378: камера, в чей объём кузов только что вошёл, берёт кадр у текущей (В103 80: срез на z1_housejump в 190; В95 70: 198)
  for (int k = 0; k < n && i < 0; k++) if ((entered & ok & (1u << k)) && k != s->cur) i = k;
  if (i < 0)
#endif
  {
    if (s->cur >= 0 && s->cur < n && (ok & (1u << s->cur))) i = s->cur;
    else for (int k = 0; k < n; k++) if (ok & (1u << k)) { i = k; break; }
  }
  if (i < 0) { s->cur = -1; return -1; }
  const RoomCam *m = &c[i];
  float e[3], l[3];
  if (m->type == 1) {
    s->t = roomRailParam(m, i == s->cur ? s->t : -1.f, ref);
    roomRailPose(m, s->t, ref, e, l);
  } else {
    memcpy(e, m->pos, 12);
    if (m->follow) for (int k = 0; k < 3; k++) l[k] = ref[k] + m->target[k];
    else memcpy(l, m->target, 12);
  }
  if (i != s->cur) { memcpy(s->eye, e, 12); memcpy(s->look, l, 12); }   // вход — срез (запись PS2)
  else {
    float kp = m->type == 1 ? lagK(m->posLag, dt) : 1.f, kt = lagK(m->targetLag, dt);
    for (int k = 0; k < 3; k++) { s->eye[k] += (e[k] - s->eye[k]) * kp; s->look[k] += (l[k] - s->look[k]) * kt; }
  }
  s->cur = i;
  // Historical 10m clamp measured radSoundHalListener, not the rendering camera.
  // Archived native tPointCamera +0xcc equals the authored StaticCam position.
#if CARZONE_LISTENER_OLD
  float d[3] = {s->eye[0] - ref[0], s->eye[1] - ref[1], s->eye[2] - ref[2]}, L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  float k = L > CARZONE_DIST ? CARZONE_DIST / L : 1.f;
  for (int j = 0; j < 3; j++) eye[j] = ref[j] + d[j] * k;
#else
  memcpy(eye, s->eye, 12);
#endif
  memcpy(look, s->look, 12); *fovY = roomCamFovY(m);
  return i;
}
