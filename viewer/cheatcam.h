// №1104/№1119, код Unlock All Cameras (сверено с оригиналом): только при включённом коде в цикл SELECT в машине попадают дополнительные виды, а у следящей и бамперной камер угол 92°
// по горизонтали. Без кода всё по правилу №650 (наше). Чистый C99: тест tests/cheats/host.c.
// Виды оригинала (список камер езды, в этом порядке): 3 comedy, 4 wreckless, 5 chase, 6 kull, 7 tracker, 8 debug. Здесь все шесть.
// Что делает каждый (своими словами, числа оригинала, где они известны; отличия помечены «наше»):
//   comedy    — «пешая» камера на машине: облёт цели на расстоянии 4,26–9 м (середина 6,63), высота 1,36 рад от вертикали, цель сглаживается с долей 0,044 за кадр, расстояние догоняет с долей 0,08;
//               поворот оригинал берёт со стика камеры, у нас — аналог (см. ввод); скорость вращения 1 рад/с — наше
//   wreckless — камера в точке последней «смены камеры» уровня (локаторы, файл camcut-l<N>.shr; у уровней 4–7 их нет), смотрит на цель; нет точки — на 10 м над целью (смещена на 0,3 м назад — наше,
//               чтобы взгляд вниз не вырождался); угол по расстоянию: 10 м — 90°, 35 м — 28,6° (рад 1,5707 … 0,5), догоняет с долей 0,08; при новой смене угол сбрасывается на широкий
//   chase     — штанга (0; 50; −15) в осях курса цели, положение и точка взгляда догоняют кубической кривой (доли 0,01 и 0,089 за кадр 1/60 с), угол 0,3487…1,7508 рад по скорости камеры
//   kull      — облёт цели на 5 м (начало: сбоку, горизонтально), стик камеры крутит (1° за кадр) и поднимает; с зажатой кнопкой «ближе/дальше» — приближает (0,1 м за кадр); не ближе 2 м; угол 78°
//   tracker   — камера стоит там, где была прошлым видом, и держит цель: относительное положение сохраняется, стик крутит (1° за кадр) и поднимает (0,05…90°), с кнопкой — приближает (1 м за кадр); угол по
//               расстоянию от 10 до 25 м: 100° … 20° (рад 1,75079 … 0,3487), доля 0,05
//   debug     — свободный полёт: камера стартует с положения и направления прошлого вида, стик вращает (1° за кадр), с кнопкой — летит вперёд/назад на 1 м за кадр; угол 78°
// «Второй геймпад» оригинала (его стик и кнопка L3/R1 ведут kull, tracker и debug) у PSP переложен: пока вид comedy, kull, tracker или debug, **аналоговый стик ведёт камеру** (руль — только крестовина ← →),
// **↓ — кнопка «ближе/дальше»** (вид назад ↓ в этих видах не работает). ↑ гудок, ✕/R газ, ○/L тормоз, □ ручник, △ выход, SELECT цикл видов — как были. Оси сферы (оригинал: «поворот, подъём») — обычные:
// x = sin(подъём)·cos(поворот), z = sin(подъём)·sin(поворот), y = cos(подъём) — наше допущение. Шаги «за кадр» оригинала пересчитаны на секунды: кадр оригинала 1/60 с.
#ifndef SV_CHEATCAM_H
#define SV_CHEATCAM_H
#include <math.h>
#include <string.h>
#ifndef CHEATS1104_OLD
#define CHEATS1104_OLD 0
#endif
#ifndef CHEATCAM_OLD
#define CHEATCAM_OLD CHEATS1104_OLD   // 1 — дополнительных видов и широкого угла нет
#endif
#ifndef CHEATCAM1119_OLD
#define CHEATCAM1119_OLD 0   // №1119: 1 — только chase и kull (как после №1104), без comedy, wreckless, tracker, debug и без управления стиком
#endif
enum { CC_VIEW_COMEDY = 3, CC_VIEW_WRECK = 4, CC_VIEW_CHASE = 5, CC_VIEW_KULL = 6, CC_VIEW_TRACK = 7, CC_VIEW_DEBUG = 8 };   // chaseView (main_camera.h): 0 дальний, 1 ближний, 2 бампер
#define CC_FOV_H 1.608495f   // 92,2° по горизонтали (оригинал, при включённом коде: следящая и бамперная камеры)
#define CC_PI 3.14159265f
static inline int ccStickView(int v) { return !CHEATCAM_OLD && !CHEATCAM1119_OLD && (v == CC_VIEW_COMEDY || v == CC_VIEW_KULL || v == CC_VIEW_TRACK || v == CC_VIEW_DEBUG); }   // виды, которые ведёт стик
static inline int ccViewNext(int cur, int dune, int unlocked, int (*base)(int, int, int)) {   // цикл SELECT: …→ дальний(1) → 3 → 4 → 5 → 6 → 7 → 8 → ближний(0)
  if (CHEATCAM_OLD || !unlocked) return base(cur > 2 ? 0 : cur, dune, 0);
  if (CHEATCAM1119_OLD) { if (cur == 1) return CC_VIEW_CHASE; if (cur == CC_VIEW_CHASE) return CC_VIEW_KULL; if (cur == CC_VIEW_KULL) return 0; return base(cur, dune, 0); }
  if (cur == 1) return CC_VIEW_COMEDY;
  if (cur >= CC_VIEW_COMEDY && cur < CC_VIEW_DEBUG) return cur + 1;
  if (cur == CC_VIEW_DEBUG) return 0;
  return base(cur, dune, 0);
}
static inline float ccFovYDeg(float aspect) { return 2.f * atanf(tanf(CC_FOV_H * .5f) / aspect) * (180.f / CC_PI); }   // вертикальный угол в градусах для нашего экрана
static inline float ccFovYFromH(float fovH, float aspect) { return 2.f * atanf(tanf(fovH * .5f) / aspect) * (180.f / CC_PI); }
static inline void ccCubic(float *pos, float *rate, float want, float interval) {   // кубическое догоняние «положение, скорость → желаемое» за долю interval
  float a = *rate + (*pos - want) * 2.f, b = (want - *pos) * 3.f - *rate * 2.f, i2 = interval * interval, i3 = i2 * interval;
  *pos = a * i3 + b * i2 + *rate * interval + *pos; *rate = a * i2 * 3.f + b * interval * 2.f + *rate;
}
static inline float ccClamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
static inline void ccSph(float r, float rot, float el, float o[3]) { o[0] = r * sinf(el) * cosf(rot); o[2] = r * sinf(el) * sinf(rot); o[1] = r * cosf(el); }
static inline void ccUnsph(const float v[3], float *r, float *rot, float *el) { *r = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); *el = *r > 1e-6f ? acosf(v[1] / *r) : 1.5f; *rot = atan2f(v[2], v[0]); }
// уменьшение угла по расстоянию: расстояние minD → maxFov, maxD → minFov, линейно; догоняет с долей lag·k
static inline void ccZoom(float dist, float minD, float maxD, float minFov, float maxFov, float *fov, float *fovd, float lag, float k) {
  float p = ccClamp01((dist - minD) / (maxD - minD)), want = maxFov - (maxFov - minFov) * p, l = lag * k; if (l > 1.f) l = 1.f;
  ccCubic(fov, fovd, want, l);
}
// ---- состояние видов и вход кадра
typedef struct { float p[3], pd[3], t[3], td[3], fov, fovd; int init; } CcChase;
typedef struct {
  int mode, init;
  CcChase chase;
  float kRot, kEl, kMag;                         // kull
  float eye[3], fov, fovd, rot, el;              // tracker (eye — относительное положение, fov), debug (eye, rot, el)
  float cMag, cMagD, cRot, cEl, ct[3], ctd[3];   // comedy: расстояние, поворот, подъём, сглаженная цель
  int hasCut; float cutPos[3];                   // wreckless: место последней смены камеры
} CcState;
typedef struct {
  float t[3], hx, hz, dt;                        // цель (машина), единичный курс, секунды кадра
  float sx, sy, toggle;                          // стик камеры: x вправо +, y вверх +; кнопка «ближе/дальше» (↓): 0 или 1
  float prevEye[3], prevLook[3];                 // камера прошлого кадра (вид стартует с неё)
  int cutEnter; float cutPos[3];                 // машина вошла в объём смены камеры в этом кадре; место камеры
} CcIn;
static inline void ccResetState(CcState *s) { memset(s, 0, sizeof *s); }
// Один кадр вида view (3..8). Выход: глаз, взгляд, горизонтальный угол кадра (рад).
static inline void ccView(CcState *s, int view, const CcIn *in, float eye[3], float look[3], float *fovH) {
  float k = in->dt * 60.f, t[3] = {in->t[0], in->t[1], in->t[2]}; int fresh = s->mode != view || !s->init;
  if (fresh) { CcState keep = *s; ccResetState(s); s->hasCut = keep.hasCut; memcpy(s->cutPos, keep.cutPos, 12); s->mode = view; s->init = 1; }
  float xa = in->sx, ya = in->sy, za = 0.f;
  if (in->toggle > .5f) { za = ya; ya = 0.f; }
  switch (view) {
    case CC_VIEW_COMEDY: {
      if (fresh) { float rod[3] = {in->prevEye[0] - t[0], in->prevEye[1] - t[1], in->prevEye[2] - t[2]}, m; ccUnsph(rod, &m, &s->cRot, &s->cEl);
                   s->cMag = m < 4.26f ? 4.26f : m > 9.f ? 9.f : m; s->cEl = 1.36f; memcpy(s->ct, t, 12); }
      s->cRot += xa * 1.f * in->dt;   // скорость вращения — наше
      float tl = .044f * k; if (tl > 1.f) tl = 1.f;
      for (int i = 0; i < 3; i++) ccCubic(&s->ct[i], &s->ctd[i], t[i], tl);
      float ml = .08f * k; if (ml > 1.f) ml = 1.f; ccCubic(&s->cMag, &s->cMagD, 6.63f, ml);
      float rod[3]; ccSph(s->cMag, s->cRot, s->cEl, rod);
      float tc[3] = {s->ct[0], s->ct[1] + 1.f, s->ct[2]};
      for (int i = 0; i < 3; i++) { eye[i] = tc[i] + rod[i]; look[i] = tc[i]; }
      *fovH = 1.363451f; break; }
    case CC_VIEW_WRECK: {
      if (in->cutEnter) { s->hasCut = 1; memcpy(s->cutPos, in->cutPos, 12); s->fov = 1.5707f; s->fovd = 0.f; }
      if (fresh) { s->fov = 1.5707f; s->fovd = 0.f; }
      if (s->hasCut) memcpy(eye, s->cutPos, 12); else { eye[0] = t[0] - in->hx * .3f; eye[1] = t[1] + 10.f; eye[2] = t[2] - in->hz * .3f; }
      memcpy(look, t, 12);
      float d[3] = {eye[0] - t[0], eye[1] - t[1], eye[2] - t[2]}, dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      ccZoom(dist, 10.f, 35.f, .5f, 1.5707f, &s->fov, &s->fovd, .08f, k); *fovH = s->fov; break; }
    case CC_VIEW_CHASE: {
      float want[3] = {t[0] - in->hx * 15.f, t[1] + 50.f, t[2] - in->hz * 15.f}; CcChase *h = &s->chase;
      if (!h->init) { memcpy(h->p, want, 12); memcpy(h->pd, want, 12); memcpy(h->t, t, 12); memcpy(h->td, t, 12); h->fov = 1.363451f; h->fovd = 0.f; h->init = 1; }
      float old[3]; memcpy(old, h->p, 12);
      float pl = .01f * k, tl = .089f * k, fl = .05f * k; if (pl > 1.f) pl = 1.f; if (tl > 1.f) tl = 1.f; if (fl > 1.f) fl = 1.f;
      for (int i = 0; i < 3; i++) { ccCubic(&h->p[i], &h->pd[i], want[i], pl); ccCubic(&h->t[i], &h->td[i], t[i], tl); }
      float dx = h->p[0] - old[0], dy = h->p[1] - old[1], dz = h->p[2] - old[2], sp = sqrtf(dx * dx + dy * dy + dz * dz); if (in->dt > 0.f) sp *= (1.f / 60.f) / in->dt;
      if (sp > .233f) sp = .233f;
      ccCubic(&h->fov, &h->fovd, 0.3487f + (1.75079f - 0.3487f) * sp / .233f, fl);
      memcpy(eye, h->p, 12); memcpy(look, h->t, 12); *fovH = h->fov; break; }
    case CC_VIEW_KULL: {
      if (fresh) { s->kRot = 0.f; s->kEl = CC_PI * .5f; s->kMag = 5.f; }
      s->kRot += xa * .0174f * k; s->kEl -= ya * .0174f * k; s->kMag -= za * .1f * k;
      if (s->kEl < .001f) s->kEl = .001f;
      if (s->kEl > CC_PI - .05f) s->kEl = CC_PI - .05f;
      if (s->kMag < 2.f) s->kMag = 2.f;
      float rod[3]; ccSph(s->kMag, s->kRot, s->kEl, rod);
      for (int i = 0; i < 3; i++) { eye[i] = t[i] + rod[i]; look[i] = t[i]; }
      *fovH = 1.363451f; break; }
    case CC_VIEW_TRACK: {
      if (fresh) { for (int i = 0; i < 3; i++) s->eye[i] = in->prevEye[i] - t[i]; s->fov = 1.363451f; s->fovd = 0.f; }
      float m, rot, el; ccUnsph(s->eye, &m, &rot, &el);
      rot += xa * .0174f * k; el -= ya * .0174f * k; m -= za * 1.f * k;
      if (el < .05f) el = .05f;
      if (el > CC_PI * .5f) el = CC_PI * .5f;
      if (m < 2.f) m = 2.f;
      ccSph(m, rot, el, s->eye);
      for (int i = 0; i < 3; i++) { eye[i] = t[i] + s->eye[i]; look[i] = t[i]; }
      ccZoom(m, 10.f, 25.f, .3487f, 1.75079f, &s->fov, &s->fovd, .05f, k); *fovH = s->fov; break; }
    case CC_VIEW_DEBUG: {
      if (fresh) { memcpy(s->eye, in->prevEye, 12); float d[3] = {in->prevLook[0] - in->prevEye[0], in->prevLook[1] - in->prevEye[1], in->prevLook[2] - in->prevEye[2]}, m; ccUnsph(d, &m, &s->rot, &s->el); }
      s->rot -= xa * .0174f * k; s->el -= ya * .0174f * k;
      if (s->el < .01f) s->el = .01f;   // пределы подъёма — наше (у оригинала их нет, на полюсе картинка переворачивается)
      if (s->el > CC_PI - .01f) s->el = CC_PI - .01f;
      float f[3]; ccSph(1.f, s->rot, s->el, f);
      for (int i = 0; i < 3; i++) s->eye[i] += f[i] * za * 1.f * k;
      for (int i = 0; i < 3; i++) { eye[i] = s->eye[i]; look[i] = s->eye[i] + f[i]; }
      *fovH = 1.363451f; break; }
    default: memcpy(eye, in->prevEye, 12); memcpy(look, in->prevLook, 12); *fovH = 1.363451f; break;
  }
}
// ---- камеры-вырезы уровня (camcut-l<N>.shr): вошла ли точка p в объём какой-либо смены камеры
typedef struct { float pos[3]; unsigned ntrig; struct { unsigned shape; float c[3], h[3], ax[9]; } tr[3]; } CcCut;   // запись файла, 208 Б
static inline int ccInVolume(const float c[3], const float h[3], const float ax[9], unsigned shape, const float p[3]) {
  float d[3] = {p[0] - c[0], p[1] - c[1], p[2] - c[2]};
  if (shape == 0) return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < h[0] * h[0];
  for (int i = 0; i < 3; i++) { float l = d[0] * ax[3 * i] + d[1] * ax[3 * i + 1] + d[2] * ax[3 * i + 2]; if (fabsf(l) > h[i]) return 0; }
  return 1;
}
static inline int ccCutHit(const CcCut *cuts, int n, const float p[3]) {   // номер смены, в чьём объёме точка; -1 — нигде
  for (int i = 0; i < n; i++) for (unsigned k = 0; k < cuts[i].ntrig && k < 3; k++) if (ccInVolume(cuts[i].tr[k].c, cuts[i].tr[k].h, cuts[i].tr[k].ax, cuts[i].tr[k].shape, p)) return i;
  return -1;
}
// ---- проекция точки на экран на процессоре (код Show Tree, viewer/showtree.h): f, r, u — единичные оси камеры (вперёд, вправо, вверх); -> 1, если точка перед камерой (ближе 0,2 м — нет)
static inline int ccProject(const float e[3], const float f[3], const float r[3], const float u[3], float tanH, float tanV, float sw, float sh, const float p[3], float o[3]) {
  float d[3] = {p[0] - e[0], p[1] - e[1], p[2] - e[2]};
  float z = d[0] * f[0] + d[1] * f[1] + d[2] * f[2]; if (z < .2f) return 0;
  o[0] = sw * .5f + (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / z / tanH * sw * .5f;
  o[1] = sh * .5f - (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / z / tanV * sh * .5f; o[2] = 0.f; return 1;
}
#endif
