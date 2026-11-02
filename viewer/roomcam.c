// Камеры комнат (пункт 11.2): см. roomcam.h. Чистый C99.
#include <string.h>
#include <math.h>
#include "roomcam.h"
#include "camlaw.h"

static unsigned u32at(const unsigned char *b) { unsigned v; memcpy(&v, b, 4); return v; }
static int finite3(const float *v) { return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]); }

int roomTrigRead(RoomTrig *t, const unsigned char *b) {
  unsigned k = u32at(b); memset(t, 0, sizeof *t);
  if (k == 0xFFFFFFFFu) { t->kind = -1; return 0; }
  if (k > 1) return -1;
  t->kind = (int)k; memcpy(t->c, b + 4, 12); memcpy(t->half, b + 16, 12); memcpy(t->ax, b + 28, 36);
  if (!finite3(t->c) || !finite3(t->half) || !finite3(t->ax[0]) || !finite3(t->ax[1]) || !finite3(t->ax[2])) return -1;
  if (t->half[0] < 0.f || t->half[1] < 0.f || t->half[2] < 0.f) return -1;
  return 0;
}
int roomTrigIn(const RoomTrig *t, float x, float y, float z, float r) {   // как msn_loc_contains (viewer/mission/msn_file.c) + радиус тела
  float d[3] = {x - t->c[0], y - t->c[1], z - t->c[2]};
  if (t->kind == 0) { float R = t->half[0] + r; return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= R * R; }
  if (t->kind != 1) return 0;
  float e2 = 0.f;   // сколько сфера r выступает за коробку по её осям
  for (int k = 0; k < 3; k++) { float e = fabsf(d[0] * t->ax[k][0] + d[1] * t->ax[k][1] + d[2] * t->ax[k][2]) - t->half[k]; if (e > 0.f) e2 += e * e; }
  return e2 <= r * r;
}

int roomCamsParse(RoomCam *c, int max, const unsigned char *b, unsigned size) {
  if (size < 128) return -1;
  unsigned off = u32at(b + 120), n = u32at(b + 124);
  if (!n) return 0;
  if (n > (unsigned)max || off < 128 || (off & 3) || off > size || n > (size - off) / ROOM_CAM_REC) return -1;
  for (unsigned i = 0; i < n; i++) {
    const unsigned char *r = b + off + (size_t)i * ROOM_CAM_REC; RoomCam *m = &c[i];
    memset(m, 0, sizeof *m); memcpy(m->name, r, 32); m->name[31] = 0;
    m->type = (int)u32at(r + 32); m->ntrig = (int)u32at(r + 36); m->npts = (int)u32at(r + 40); m->follow = (int)u32at(r + 44);
    memcpy(m->pos, r + 48, 12); memcpy(m->target, r + 60, 12);
    memcpy(&m->fov, r + 72, 4); memcpy(&m->targetLag, r + 76, 4); memcpy(&m->posLag, r + 80, 4);
    memcpy(&m->minR, r + 84, 4); memcpy(&m->maxR, r + 88, 4); memcpy(&m->trackDist, r + 92, 4);
    m->trackRail = (int)u32at(r + 96); m->reverse = (int)u32at(r + 100); m->behaviour = (int)u32at(r + 104);
    memcpy(m->offset, r + 108, 12); m->cut = (int)u32at(r + 120); m->data = (int)u32at(r + 124);
    if (m->type < 0 || m->type > 1 || m->ntrig < 0 || m->ntrig > ROOM_CAM_TRIGS || m->npts < 0 || m->npts > ROOM_CAM_PTS_MAX) return -1;
    if (m->type == 1 && m->npts < 1) return -1;
    unsigned xoff = u32at(r + 504);   // 0 — точки на месте (до 9), иначе смещение массива всех точек от начала блоба
    if (!xoff) { if (m->npts > ROOM_CAM_PTS) return -1; m->pts = (const float (*)[3])(r + 384); }
    else { if ((xoff & 3) || xoff < 128 || xoff > size || (unsigned)m->npts > (size - xoff) / 12) return -1; m->pts = (const float (*)[3])(b + xoff); }
    if (!finite3(m->pos) || !finite3(m->target) || !finite3(m->offset) || !isfinite(m->fov) || m->fov <= 1.f || m->fov >= 170.f) return -1;
    if (!isfinite(m->targetLag) || !isfinite(m->posLag)) return -1;
    for (int k = 0; k < m->ntrig; k++) if (roomTrigRead(&m->trig[k], r + 128 + 64 * k)) return -1;
    for (int k = 0; k < m->npts; k++) if (!finite3(m->pts[k])) return -1;
    memcpy(m->axisPlay, r + 492, 12);
  }
  return (int)n;
}

#ifndef ROOM_FOOT_R
#define ROOM_FOOT_R 0.f
#endif
#ifndef ROOMCAM_FLAGS_SV_OLD
#define ROOMCAM_FLAGS_SV_OLD 0
#endif
int roomCamAllows(const RoomCam *c, int inCar, float speed) {   // сверка g06 (g06-camera:1,2,9,37): допуск по битам данных
#if ROOMCAM_FLAGS_SV_OLD
  (void)c; (void)inCar; (void)speed; return 1;
#else
  if (!strcmp(c->name, "z3_ufocam")) return 1;   // сверка g06 круг 2 (P08 Д9): зона z3_ufocam включается без всех проверок (скорость, машина или пешком, тип камеры)
  int zoneCarSv, zoneFootSv;
  if (c->type == 0) { int d = c->data; if (d == 3) return 0; zoneCarSv = d & 1; zoneFootSv = d & 2; }
  else { int z = (int)(c->axisPlay[2] + .5f); zoneCarSv = z & 1; zoneFootSv = z & 8; }
  if (zoneCarSv && zoneFootSv) return 0;
  if (zoneCarSv) return inCar && speed >= ROOMCAM_CAR_V;
  if (zoneFootSv) return !inCar;
  return 1;
#endif
}
int roomCamCut(const RoomCam *c) {
  if (c->type == 0) return c->cut != 0 || c->posLag >= .999f;
  return ((int)(c->axisPlay[2] + .5f) & 2) != 0 || c->axisPlay[0] >= .999f;
}
float roomCamTransRate(const RoomCam *c) {
  float r = c->type == 0 ? c->posLag : (c->axisPlay[0] > .001f ? c->axisPlay[0] : .02f);   // по данным камеры; у оригинала время перехода идёт со скоростью 30 × скорость, 1000 мс лимит
  return r < .001f ? .001f : r;
}
float roomCamFovFromH(float hdeg) { return 2.f * atanf(tanf(hdeg * (3.14159265f / 360.f)) * .75f) * (180.f / 3.14159265f); }
float roomCamFovCar(const RoomCam *c, float dist, float *cur, float dt) {
  float mx = c->fov > 1.f ? c->fov : 90.f, mn = 20.f;
  if (mn > mx) mn = mx;
  // круг 6 (g06-camera:12, СВЕРКА_КАМЕРА 5.1): у машины статичная и рельсовая камеры держат ПОЛНЫЙ угол данных; зум по расстоянию 10–25 м есть только у одной из чит-камер. ROOMCAM_FOVCAR_SV_OLD=1 — зум
  float want = ROOMCAM_FOVCAR_SV_OLD ? (c->type == 1 || dist <= 10.f ? mx : dist >= 25.f ? mn : mx + (mn - mx) * (dist - 10.f) / 15.f) : mx;
  if (!(*cur > 0.f) || c->type == 1) *cur = want;
  else *cur += (want - *cur) * (1.f - powf(1.f - .022f, dt * 60.f));
  return roomCamFovFromH(*cur);
}
static int camHas(const RoomCam *c, const float p[3]) {   // ноги Гомера — точкой (камеры делят пол комнаты триггерами)
  if (!roomCamAllows(c, 0, 0.f)) return 0;   // сверка g06: пешком не показываем «только для машины» (и «никому»)
  for (int k = 0; k < c->ntrig; k++) if (roomTrigIn(&c->trig[k], p[0], p[1], p[2], ROOM_FOOT_R)) return 1;
  return 0;
}
int roomCamPick(const RoomCam *c, int n, int cur, const float p[3]) {
  if (cur >= 0 && cur < n && camHas(&c[cur], p)) return cur;
  for (int i = 0; i < n; i++) if (camHas(&c[i], p)) return i;
  return -1;
}
// рельс — ломаная по точкам сплайна, параметр — длина дуги от первой точки
float roomRailLen(const RoomCam *c) {
  float L = 0.f;
  for (int k = 0; k + 1 < c->npts; k++) { const float *a = c->pts[k], *b = c->pts[k + 1]; L += sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2])); }
  return L;
}
void roomRailAt(const RoomCam *c, float t, float o[3]) {
  memcpy(o, c->pts[0], 12);
  for (int k = 0; k + 1 < c->npts; k++) {
    const float *a = c->pts[k], *b = c->pts[k + 1];
    float l = sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2]));
    if (t <= l || k + 2 == c->npts) { float u = l > 1e-6f ? t / l : 0.f; if (u < 0.f) u = 0.f; if (u > 1.f) u = 1.f;
      for (int i = 0; i < 3; i++) o[i] = a[i] + (b[i] - a[i]) * u; return; }
    t -= l;
  }
}
float roomRailClosest(const RoomCam *c, const float p[3]) {   // параметр ближайшей к p точки рельса
  float best = 1e30f, bt = 0.f, acc = 0.f;
  for (int k = 0; k + 1 < c->npts; k++) {
    const float *a = c->pts[k], *b = c->pts[k + 1];
    float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, l2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2], u = 0.f, l = sqrtf(l2);
    if (l2 > 1e-12f) { u = ((p[0] - a[0]) * ab[0] + (p[1] - a[1]) * ab[1] + (p[2] - a[2]) * ab[2]) / l2; if (u < 0.f) u = 0.f; if (u > 1.f) u = 1.f; }
    float dx = a[0] + ab[0] * u - p[0], dy = a[1] + ab[1] * u - p[1], dz = a[2] + ab[2] * u - p[2], d = dx * dx + dy * dy + dz * dz;
    if (d < best) { best = d; bt = acc + u * l; }
    acc += l;
  }
  return bt;
}
static float hdist(const RoomCam *c, float t, const float p[3]) { float q[3]; roomRailAt(c, t, q); return hypotf(q[0] - p[0], q[2] - p[2]); }
static float solve(const RoomCam *c, float a, float b, const float p[3], float want) {   // точка на [a, b], где расстояние = want (a — одна сторона, b — другая)
  float fa = hdist(c, a, p) - want;
  for (int i = 0; i < 24; i++) { float m = .5f * (a + b), fm = hdist(c, m, p) - want; if ((fm > 0.f) == (fa > 0.f)) { a = m; fa = fm; } else b = m; }
  return .5f * (a + b);
}
// поведение Distance (Behaviour 1 в данных): камера едет по рельсу, только чтобы горизонтальное расстояние до Гомера было
// в кольце [MinRadius, MaxRadius]; дальше MaxRadius — подъезжает, ближе MinRadius — отъезжает (сначала — назад по
// рельсу, к его началу; ReverseSense — к концу): в обоих случаях до MaxRadius. t — прошлый параметр или < 0 (включилась)
// Правка 186 (близнец 03.10, v17_house_cam): камера ездит только между ВНУТРЕННИМИ точками сплайна (вторая … предпоследняя) —
// крайние точки у оригинала — направляющие кривой: в гостиной камера int_sh_path1 стоит во второй точке (501,40), пока Гомер
// правее, и едет только влево; int_sh_path2 — между второй и третьей. Рельс из 2–3 точек — весь. -DROOMCAM_RAIL_FULL — прежнее
static void railSpan(const RoomCam *c, float *a, float *b) {
  float L = roomRailLen(c); *a = 0.f; *b = L;
#ifndef ROOMCAM_RAIL_FULL
  if (c->npts >= 4) {
    const float *p0 = c->pts[0], *p1 = c->pts[1], *q0 = c->pts[c->npts - 2], *q1 = c->pts[c->npts - 1];
    *a = sqrtf((p1[0] - p0[0]) * (p1[0] - p0[0]) + (p1[1] - p0[1]) * (p1[1] - p0[1]) + (p1[2] - p0[2]) * (p1[2] - p0[2]));
    *b = L - sqrtf((q1[0] - q0[0]) * (q1[0] - q0[0]) + (q1[1] - q0[1]) * (q1[1] - q0[1]) + (q1[2] - q0[2]) * (q1[2] - q0[2]));
  }
#endif
}
float roomRailParam(const RoomCam *c, float t, const float p[3]) {
  float A, B; railSpan(c, &A, &B);
  float tc = roomRailClosest(c, p); if (tc < A) tc = A; if (tc > B) tc = B;
#ifndef ROOMCAM_RAIL_FULL
  if (t < 0.f) {   // правка 186: включилась — сразу на MaxRadius от Гомера назад по рельсу (ReverseSense — вперёд), как у оригинала
    float end = c->reverse ? B : A;                // (int_sh_path2: глаз в 3,21 м от Гомера в кадре включения, MaxRadius 3,2)
    return hdist(c, tc, p) >= c->maxR ? tc : (hdist(c, end, p) <= c->maxR ? end : solve(c, tc, end, p, c->maxR));
  }
#endif
  if (t < A || t > B) t = tc;
  float d = hdist(c, t, p);
  if (d > c->maxR) t = hdist(c, tc, p) >= c->maxR ? tc : solve(c, t, tc, p, c->maxR);
  else if (d < c->minR) {
    int dir = fabsf(t - tc) > 1e-4f ? (t > tc ? 1 : -1) : (c->reverse ? 1 : -1);
    float end = dir > 0 ? B : A;
    t = hdist(c, end, p) <= c->maxR ? end : solve(c, t, end, p, c->maxR);   // слишком близко — отъезд на MaxRadius
  }
  return t;
}
// сверка g06 (g06-camera:22; P08 Д7): поведение Distance — камера стоит там, где горизонтальное расстояние до цели ровно MaxRadius (из двух пересечений — ближайшее к прежней позиции;
// не пересекает — ближайшая к цели точка рельса). MinRadius камеру не двигает (это ближняя плоскость). Ход по рельсу не быстрее 0,25 отрезка сплайна за обновление (1/60 с),
// поэтому при скачке цели камера едет. ROOMCAM_RAILSV_OLD=1 — прежнее кольцо [MinRadius, MaxRadius]
#ifndef ROOMCAM_RAILSV_OLD
#define ROOMCAM_RAILSV_OLD 0
#endif
float roomRailParamDt(const RoomCam *c, float t, const float p[3], float dt) {
#if ROOMCAM_RAILSV_OLD
  (void)dt; return roomRailParam(c, t, p);
#else
  float A, B; railSpan(c, &A, &B);
  float ref = t >= 0.f ? t : (c->reverse ? B : A), R = c->maxR, best = -1.f, bd = 1e30f, acc = 0.f;
  for (int k = 0; k + 1 < c->npts; k++) {
    const float *a = c->pts[k], *b = c->pts[k + 1];
    float dx = b[0] - a[0], dz = b[2] - a[2], ey = b[1] - a[1], l = sqrtf(dx * dx + ey * ey + dz * dz);
    float al = dx * dx + dz * dz, be = 2.f * (dx * (a[0] - p[0]) + dz * (a[2] - p[2])), ga = (a[0] - p[0]) * (a[0] - p[0]) + (a[2] - p[2]) * (a[2] - p[2]) - R * R;
    if (al > 1e-10f) {
      float disc = be * be - 4.f * al * ga;
      if (disc >= 0.f) { float sq = sqrtf(disc);
        for (int r = 0; r < 2; r++) { float u = (-be + (r ? sq : -sq)) / (2.f * al), tt = acc + u * l;
          if (u >= 0.f && u <= 1.f && tt >= A - 1e-4f && tt <= B + 1e-4f && fabsf(tt - ref) < bd) { bd = fabsf(tt - ref); best = tt; } } }
    }
    acc += l;
  }
  if (best < 0.f) { best = roomRailClosest(c, p); if (best < A) best = A; if (best > B) best = B; }
  if (t < 0.f) return best;
  float seg = 1.f, a2 = 0.f;   // длина отрезка сплайна под прежним положением
  for (int k = 0; k + 1 < c->npts; k++) { const float *a = c->pts[k], *b = c->pts[k + 1]; float l = sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2])); seg = l; if (t <= a2 + l) break; a2 += l; }
  float lim = .25f * seg * (dt > 0.f ? dt * 60.f : 1.f), d = best - t;
  return t + (d > lim ? lim : d < -lim ? -lim : d);
#endif
}
// цель рельса (правка 138, Роб 01.10 «ракурсы камер внутри помещений — как в оригинале»): TargetOffset в осях РЕЛЬСА в точке
// камеры t — x вбок (side = fw × up в координатах движка), y вверх, z вдоль рельса (к следующей точке); fw — горизонтальная
// касательная ломаной. Экспорт (tools/interior_export.py) пишет смещение с отражённым z (как точки мира), здесь z
// возвращается: вдоль = -offset[2]. Толкование порта по данным и записи PS2 (кода оригинала нет): «грудь Гомера в кадре»
// во всех точках пола трёх комнат уровня 1 (tests/rooms/frame_check.sh), пары-зеркала int_kwik_camera06/12 (±15 по x)
// смотрят вглубь магазина. Прежнее толкование (смещение в осях мира) — сборкой с -DROOMCAM_OFFSET_WORLD (красный гейта).
// Правка 186 (близнец 03.10, камера PS2 по кадрам в гостиной Симпсонов — build/twins/o4/v17_house_cam/walk.tsv): у рельса с
// TrackRail 1 оригинал смотрит НЕ на Гомера — направление взгляда постоянно, куда бы Гомер ни шёл (камера только едет по рельсу).
// Цель — точка рельса на TrackDist впереди (в долях отрезка сплайна: 1 — целый отрезок между соседними точками; за концом —
// по касательной) + TargetOffset в осях рельса. Сверено числами: int_sh_path1 (смещение 1,0/−0,3, TrackDist 0,01, отрезок 2,66 м)
// — fwd (−0,026; −0,287; −0,958) у PS2 и у формулы; int_sh_path2 (0,25/−0,125, 0,1, отрезок 4,8 м) — (0,127; −0,225; −0,966).
// TrackRail 0 (школа: камеры 03/04, смещение (0; 1,25; 0) — голова) — цель Гомер + смещение, как раньше. Прежнее толкование для
// всех рельсов — сборкой с -DROOMCAM_TRACK_OLD (красный гейта tests/rooms/housecam_twins.py)
static void railAhead(const RoomCam *c, float t, float dpar, float o[3]) {   // точка рельса на dpar отрезков сплайна впереди t
  float acc = 0.f; int k = 0; float l = 0.f;
  for (; k + 1 < c->npts; k++) {
    const float *a = c->pts[k], *b = c->pts[k + 1];
    l = sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2]));
    if (t <= acc + l || k + 2 == c->npts) break;
    acc += l;
  }
  if (c->npts < 2) { memcpy(o, c->pts[0], 12); return; }
  float U = (float)k + (l > 1e-6f ? (t - acc) / l : 0.f) + dpar, last = (float)(c->npts - 1);
  int s = U < 0.f ? 0 : (U >= last ? c->npts - 2 : (int)U);   // отрезок цели; за концами — продолжение крайнего отрезка
  float u = U - (float)s; const float *a = c->pts[s], *b = c->pts[s + 1];
  for (int i = 0; i < 3; i++) o[i] = a[i] + (b[i] - a[i]) * u;
}
static void railLook(const RoomCam *c, float t, const float p[3], float look[3]) {
#ifdef ROOMCAM_OFFSET_WORLD
  (void)t; for (int i = 0; i < 3; i++) look[i] = p[i] + c->offset[i];
#else
  float a[3], b[3], L = roomRailLen(c);
  roomRailAt(c, t > .05f ? t - .05f : 0.f, a); roomRailAt(c, t + .05f < L ? t + .05f : L, b);
  float fx = b[0] - a[0], fz = b[2] - a[2], l = sqrtf(fx * fx + fz * fz);
  if (l < 1e-4f) { const float *q = c->pts[0], *r = c->pts[c->npts - 1]; fx = r[0] - q[0]; fz = r[2] - q[2]; l = sqrtf(fx * fx + fz * fz); }
  if (l < 1e-4f) { fx = 0.f; fz = 1.f; l = 1.f; }   // рельс-точка: оси мира
  fx /= l; fz /= l;
  float sx = -fz, sz = fx, oz = -c->offset[2];     // side = fw × (0,1,0)
  float base[3] = {p[0], p[1], p[2]};
#ifndef ROOMCAM_TRACK_OLD
  if (c->trackRail) railAhead(c, t, c->trackDist, base);   // правка 186: от точки рельса впереди, не от Гомера
#endif
  look[0] = base[0] + c->offset[0] * sx + oz * fx; look[1] = base[1] + c->offset[1]; look[2] = base[2] + c->offset[0] * sz + oz * fz;
#endif
}
void roomRailPose(const RoomCam *c, float t, const float p[3], float eye[3], float look[3]) {   // №408: камеры зон машины (carzone.c)
  roomRailAt(c, t, eye); railLook(c, t, p, look);
}
void roomCamIdeal(const RoomCam *c, const float p[3], float eye[3], float look[3]) {
  if (c->type == 0) {
    memcpy(eye, c->pos, 12);
    if (c->follow) { look[0] = p[0] + c->target[0]; look[1] = p[1] + c->target[1]; look[2] = p[2] + c->target[2]; }
    else memcpy(look, c->target, 12);
  } else {
    float t = roomRailParam(c, -1.f, p); roomRailAt(c, t, eye); railLook(c, t, p, look);
  }
}
float roomCamFovY(const RoomCam *c) { return 2.f * atanf(tanf(c->fov * (3.14159265f / 360.f)) * .75f) * (180.f / 3.14159265f); }
static float lagK(float lag, float dt) {   // доля пути за dt при запаздывании lag на кадр 1/60 с
  if (lag >= 1.f) return 1.f; if (lag <= 0.f) return 0.f;
  return 1.f - powf(1.f - lag, dt * 60.f);
}
static float trigVol(const RoomTrig *t) {
  if (t->kind == 0) return 4.18879f * t->half[0] * t->half[0] * t->half[0];
  return 8.f * t->half[0] * t->half[1] * t->half[2];
}
static float camVolAt(const RoomCam *c, const float p[3]) {   // объём того триггера камеры, в котором стоит Гомер (меньше — «точнее» камера)
  float v = 1e30f;
  for (int k = 0; k < c->ntrig; k++) if (roomTrigIn(&c->trig[k], p[0], p[1], p[2], ROOM_FOOT_R)) { float q = trigVol(&c->trig[k]); if (q < v) v = q; }
  return v;
}
static int roomCamStepOld(const RoomCam *c, int n, RoomCamState *s, const float p[3], float dt, float eye[3], float look[3], float *fovY) {
  int i;
  if (s->newWins && n <= ROOM_CAMS && !s->switchOld) {
    // №138/186/229 (близнецы 17.10: школа, DMV, Moe's, дом L4, комната Барта, Observatory, Android's): выбор камеры по триггерам.
    //  1) среди камер, в чьих триггерах стоит Гомер, оригинал держит ту, у которой триггер МЕНЬШЕ (школа camera04 ≪ path1, дом path2 < path1,
    //     DMV 01 < 02, Барт 02/03 ≪ 01, Observatory 01 < 10); если текущая ещё держит Гомера, её сменит только меньшая — и сразу;
    //  2) любая другая камера берёт кадр, когда Гомер непрерывно стоит в её триггере ROOM_SWITCH_DELAY (~1 с, от ВХОДА в триггер: Kwik 02→06,
    //     07→01, школа camera04→path1, Observatory 01→10); пока ждём, прежняя камера стоит как была, даже если Гомер уже вышел из её триггера;
    //  3) ни в одном триггере — камера ходьбы (-1).
    int cur = (s->cur >= 0 && s->cur < n) ? s->cur : -1, best = -1, any = 0; float bv = 1e30f, cv = cur >= 0 && camHas(&c[cur], p) ? camVolAt(&c[cur], p) : -1.f;
    for (int k = 0; k < n; k++) { if (camHas(&c[k], p)) s->inT[k] += dt; else s->inT[k] = 0.f; }
    for (int k = 0; k < n; k++) {
      if (!camHas(&c[k], p)) continue;
      any = 1; float v = camVolAt(&c[k], p);
      int elig = k == cur || cur < 0 || s->inT[k] >= ROOM_SWITCH_DELAY || c[k].posLag >= .999f || (cv >= 0.f && v < cv * ROOM_NEST_RATIO && !c[k].trackRail);
      if (elig && (v < bv - 1e-4f || (best >= 0 && fabsf(v - bv) <= 1e-4f && k == cur))) { bv = v; best = k; }
    }
    if (!any) { s->cur = -1; return -1; }
    if (best < 0) { memcpy(eye, s->eye, 12); memcpy(look, s->look, 12); *fovY = roomCamFovY(&c[cur]); return cur; }   // ждём: прежняя камера стоит
    i = best; s->in = 0;
    if (i != cur) goto switched;
    goto plain;
  } else if (s->newWins && n <= 32) {   // правка 186: вошёл в триггер другой камеры — она и снимает
    unsigned now = 0; for (int k = 0; k < n; k++) if (camHas(&c[k], p)) now |= 1u << k;
    unsigned entered = now & ~s->in; s->in = now; i = -1;
    if (entered && !(s->cur >= 0 && s->cur < n && (entered & (1u << s->cur)) && entered == (1u << s->cur)))
      for (int k = 0; k < n; k++) if (entered & (1u << k)) { i = k; break; }
    if (i < 0) i = roomCamPick(c, n, s->cur, p);
  } else i = roomCamPick(c, n, s->cur, p);
#ifndef ROOMCAM_SWITCH_NOW
  // Правка 172 (близнец v11_kwik_fwd + v17_house_cam, камера PS2 по кадрам): в комнате оригинал переходит на камеру с запаздыванием
  // (рельс — PositionLag < 1, статичная — TransitionTargetRate < 1) не сразу, а через ~1 с, пока Гомер в её триггере: Kwik-E-Mart
  // 02 → 06 на 50-м кадре (1,0 с), 07 → 01 на 48-м (0,96 с); на камеру без запаздывания (дом, int_sh_path2, 1/1) — сразу (2 кадра).
  // Смена по-прежнему срезом. Только комнаты (newWins); камеры зон города — как раньше
  if (s->switchOld) {
  if (s->newWins && s->cur >= 0 && s->cur < n && i >= 0 && i != s->cur && c[i].posLag < .999f) {
    if (s->want != i) { s->want = i; s->wantT = 0.f; }
    s->wantT += dt;
    if (s->wantT < ROOM_SWITCH_DELAY) {   // ждём: прежняя камера стоит как была (у оригинала 02 не ехала за Гомером, кадры 3064–3112)
      if (!camHas(&c[s->cur], p)) { memcpy(eye, s->eye, 12); memcpy(look, s->look, 12); *fovY = roomCamFovY(&c[s->cur]); return s->cur; }
      i = s->cur;
    }
  } else { s->want = -1; s->wantT = 0.f; }
  }
#endif
plain:
switched:
  if (i < 0) { s->cur = -1; return -1; }
  float e[3], l[3]; roomCamIdeal(&c[i], p, e, l);
  if (c[i].type == 1) { s->t = roomRailParam(&c[i], i == s->cur ? s->t : -1.f, p); roomRailAt(&c[i], s->t, e); railLook(&c[i], s->t, p, l); }   // рельс: параметр живёт между кадрами
  if (i != s->cur) { memcpy(s->eye, e, 12); memcpy(s->look, l, 12); }   // смена камеры — срез
  else {
    float kp = c[i].type == 1 ? lagK(c[i].posLag, dt) : 1.f, kt = lagK(c[i].targetLag, dt);
    for (int k = 0; k < 3; k++) { s->eye[k] += (e[k] - s->eye[k]) * kp; s->look[k] += (l[k] - s->look[k]) * kt; }
  }
  s->cur = i; memcpy(eye, s->eye, 12); memcpy(look, s->look, 12); *fovY = roomCamFovY(&c[i]);
  return i;
}

// ---- сверка g06 (02.11.2026): выбор камеры и переходы как у оригинала (g06-camera:16,19,25,36; P08 Д4, 14) ----
// Выбор по объёмам (Д4): вход Гомера в объём камеры включает её сразу, последняя вошедшая побеждает (прежняя запоминается в стеке); выход из объёма активной камеры возвращает ту, что была
// до неё, если она ещё держит Гомера, иначе камеру ходьбы. Без «меньший объём» и без задержки 1 с (правки 138/172/186/229 — прежнее, ROOMCAM_SEL_SV_OLD=1).
// Переход к камере (P08 14, P18-03): лимит 1,0 с при времени ×0,6 от реального (≈1,67 с), кривая 2t − t² от позы начала к движущейся цели; срез при CutInOut / скорости перехода 1,0,
// у самой первой камеры (первый вход после загрузки уровня или комнаты) и когда позу ходьбы не передали. Выход: возврат к камере ходьбы обычным переходом (экспонента τ 0,15 с, около 0,45 с
// до 95%), срез при CutInOut. Не сделано: ускорение перехода при беге быстрее максимальной скорости (скорость героя в функцию не передаётся)
#ifndef ROOMCAM_SEL_SV_OLD
#define ROOMCAM_SEL_SV_OLD 0
#endif
#ifndef ROOMCAM_HYPE_SV_OLD
#define ROOMCAM_HYPE_SV_OLD 0   // 1 — зона hypecam не держит камеру (круг 2)
#endif
#ifndef ROOMCAM_TRANS_SV_OLD
#define ROOMCAM_TRANS_SV_OLD 0
#endif
void roomCamRestart(RoomCamState *s) {   // №1085: стек, переход, выход и признак «первая камера» — заново; поза прошлого кадра не нужна
  s->cur = -1; s->in = 0; s->nstk = 0; s->firstDone = 0; s->trOn = 0; s->exiting = 0; s->want = -1; s->wantT = 0.f; s->hasP = 0;
}
// Круг 6 (СВЕРКА_КАМЕРА 3.5, P08:14): выход из последней камеры зоны в камеру ходьбы идёт не рывком и не e^(-t/0,15), а так: до 7 с, шаг за кадр — доля ((1 − 0,02)·t/7 + 0,02), положение камеры
// движется кубическим шагом по трём числам вокруг точки взгляда камеры ходьбы (поворот, наклон, дальность), точка взгляда — по трём осям; заканчивается через 7 с или когда обе точки совпали
// (< 0,03 м). Камера ходьбы идёт своим ходом, поэтому места берутся заново каждый кадр
#define ROOMCAM_EXIT_S 7.f
#define ROOMCAM_PI 3.14159265f
static void exitSph(const float v[3], float *m, float *rot, float *el) { *m = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); *rot = atan2f(v[0], v[2]); float c = *m > 1e-5f ? v[1] / *m : 1.f; *el = acosf(c > 1.f ? 1.f : c < -1.f ? -1.f : c); }
static int exitStep(RoomCamState *s, const float bE[3], const float bL[3], float dt, float eye[3], float look[3]) {
  if (!s->xInit) { memcpy(s->xE, s->exitE, 12); memcpy(s->xL, s->exitL, 12); s->xR[0] = s->xR[1] = s->xR[2] = 0.f; s->xLv[0] = s->xLv[1] = s->xLv[2] = 0.f; s->xInit = 1; s->exitT = 0.f; }
  s->exitT += dt;
  float I = (.98f * (s->exitT / ROOMCAM_EXIT_S) + .02f) * (dt / .016f); if (I > 1.f) I = 1.f; if (I < 0.f) I = 0.f;
  float a[3] = {s->xE[0] - bL[0], s->xE[1] - bL[1], s->xE[2] - bL[2]}, b[3] = {bE[0] - bL[0], bE[1] - bL[1], bE[2] - bL[2]}, am, ar, ae, bm, br, be;
  exitSph(a, &am, &ar, &ae); exitSph(b, &bm, &br, &be);
  while (br - ar > ROOMCAM_PI) br -= 2.f * ROOMCAM_PI; while (br - ar < -ROOMCAM_PI) br += 2.f * ROOMCAM_PI;
  float nr = camHermite(ar, &s->xR[0], br, I), ne = camHermite(ae, &s->xR[1], be, I), nm = camHermite(am, &s->xR[2], bm, I);
  s->xE[0] = bL[0] + nm * sinf(ne) * sinf(nr); s->xE[1] = bL[1] + nm * cosf(ne); s->xE[2] = bL[2] + nm * sinf(ne) * cosf(nr);
  for (int k = 0; k < 3; k++) s->xL[k] = camHermite(s->xL[k], &s->xLv[k], bL[k], I);
  float d1 = 0.f, d2 = 0.f; for (int k = 0; k < 3; k++) { d1 += (s->xE[k] - bE[k]) * (s->xE[k] - bE[k]); d2 += (s->xL[k] - bL[k]) * (s->xL[k] - bL[k]); }
  memcpy(eye, s->xE, 12); memcpy(look, s->xL, 12);
  if (s->exitT >= ROOMCAM_EXIT_S || (d1 < .03f * .03f && d2 < .03f * .03f)) { s->exiting = 0; s->xInit = 0; return 0; }
  return 1;
}
int roomCamStep(const RoomCam *c, int n, RoomCamState *s, const float p[3], float dt, float eye[3], float look[3], float *fovY) {
  return roomCamStepFrom(c, n, s, p, dt, eye, look, fovY, 0, 0);
}
int roomCamStepFrom(const RoomCam *c, int n, RoomCamState *s, const float p[3], float dt, float eye[3], float look[3], float *fovY, const float baseE[3], const float baseL[3]) {
  if (ROOMCAM_SEL_SV_OLD || n > 32 || n <= 0) return roomCamStepOld(c, n, s, p, dt, eye, look, fovY);
  // №1085 (Роб: «при загрузке в миссию внутри дома камера сначала стоит позади героя, как на улице, потом едет вбок на комнатную»). Оригинал включает камеру срезом, если она первая после
  // загрузки уровня или миссии, при входе в помещение и выходе, и когда миссия переставила героя; плавный переход 1 с — только на ходу между камерами. Признак «первая» у нас firstDone
  // сбрасывался только при создании состояния, поэтому со второй загрузки всё шло переходом. Теперь: герой ушёл за кадр дальше 3 м (загрузка, перезапуск, перенос, дверь) —
  // состояние начинается заново, первая камера срезом. CUT1085_OLD=1 — как в TEST82
  if (!CUT1085_OLD) { if (s->hasP) { float dx = p[0] - s->lastP[0], dy = p[1] - s->lastP[1], dz = p[2] - s->lastP[2]; if (dx * dx + dy * dy + dz * dz > 9.f) roomCamRestart(s); }
    memcpy(s->lastP, p, 12); s->hasP = 1; }
  unsigned now = 0; for (int k = 0; k < n; k++) if (camHas(&c[k], p)) now |= 1u << k;
  unsigned entered = now & ~s->in; s->in = now;
  for (int pass = 0; pass < 32; pass++) {   // вошедшие в этот кадр — в стек; одновременно вошедшие: больший объём раньше, меньший сверху
    int k = -1; float bv = -1.f;
    for (int j = 0; j < n; j++) if ((entered & (1u << j)) && camVolAt(&c[j], p) > bv) { bv = camVolAt(&c[j], p); k = j; }
    if (k < 0) break;
    entered &= ~(1u << k);
    int w = 0; for (int q = 0; q < s->nstk; q++) if (s->stk[q] != k) s->stk[w++] = s->stk[q];
    s->nstk = w; if (s->nstk < 8) s->stk[s->nstk++] = k; else { for (int q = 1; q < 8; q++) s->stk[q - 1] = s->stk[q]; s->stk[7] = k; }
  }
  { int w = 0; for (int q = 0; q < s->nstk; q++) if (now & (1u << s->stk[q])) s->stk[w++] = s->stk[q]; s->nstk = w; }
  int i = s->nstk > 0 ? s->stk[s->nstk - 1] : -1;
  for (int q = 0; q < s->nstk && !ROOMCAM_HYPE_SV_OLD; q++) if (!strcmp(c[s->stk[q]].name, "hypecam")) i = s->stk[q];   // сверка g06 круг 2 (P08 Д9): пока Гомер в зоне hypecam, другую камеру выбрать нельзя
  if (i < 0) {
    if (s->cur >= 0) {   // вышел из последней камеры: срез при CutInOut, иначе плавный возврат к камере ходьбы
      int cutOut = roomCamCut(&c[s->cur]) || !baseE || ROOMCAM_TRANS_SV_OLD;
      s->cur = -1; s->trOn = 0;
      if (!cutOut) { s->exiting = 1; s->exitT = 0.f; s->xInit = 0; memcpy(s->exitE, s->outE, 12); memcpy(s->exitL, s->outL, 12); } else s->exiting = 0;
    }
    if (s->exiting && baseE && !ROOMCAM_EXIT_SV_OLD) { if (exitStep(s, baseE, baseL, dt, eye, look)) { *fovY = 0.f; return -2; } return -1; }
    if (s->exiting && baseE) {
      s->exitT += dt; float w = expf(-s->exitT / .15f);
      if (w < .02f) s->exiting = 0;
      else { for (int k = 0; k < 3; k++) { eye[k] = baseE[k] + (s->exitE[k] - baseE[k]) * w; look[k] = baseL[k] + (s->exitL[k] - baseL[k]) * w; } *fovY = 0.f; return -2; }
    }
    return -1;
  }
  float e[3], l[3]; roomCamIdeal(&c[i], p, e, l);
  int sw = i != s->cur;
  if (c[i].type == 1) { s->t = roomRailParamDt(&c[i], !sw ? s->t : -1.f, p, dt); roomRailAt(&c[i], s->t, e); railLook(&c[i], s->t, p, l); }
  if (sw) {
    float fromE[3], fromL[3]; int have = 0;
    if (s->exiting && !ROOMCAM_EXIT_SV_OLD) { if (s->xInit) { memcpy(fromE, s->xE, 12); memcpy(fromL, s->xL, 12); have = 1; } else if (baseE) { memcpy(fromE, s->exitE, 12); memcpy(fromL, s->exitL, 12); have = 1; } s->exiting = 0; s->xInit = 0; }
    else if (s->exiting) { float w = expf(-s->exitT / .15f); if (baseE) { for (int k = 0; k < 3; k++) { fromE[k] = baseE[k] + (s->exitE[k] - baseE[k]) * w; fromL[k] = baseL[k] + (s->exitL[k] - baseL[k]) * w; } have = 1; } s->exiting = 0; }
    else if (s->cur >= 0) { memcpy(fromE, s->outE, 12); memcpy(fromL, s->outL, 12); have = 1; }
    else if (baseE) { memcpy(fromE, baseE, 12); memcpy(fromL, baseL, 12); have = 1; }
    int cut = ROOMCAM_TRANS_SV_OLD || !have || !s->firstDone || roomCamCut(&c[i]);
    s->firstDone = 1; memcpy(s->eye, e, 12); memcpy(s->look, l, 12);
    s->trRate = ROOMCAM_RATE_SV_OLD ? .02f : roomCamTransRate(&c[i]);
    if (cut) s->trOn = 0; else { s->trOn = 1; s->trT = 0.f; memcpy(s->trFromE, fromE, 12); memcpy(s->trFromL, fromL, 12); }
  } else {
    float kp = c[i].type == 1 ? lagK(c[i].posLag, dt) : 1.f, kt = lagK(c[i].targetLag, dt);
    for (int k = 0; k < 3; k++) { s->eye[k] += (e[k] - s->eye[k]) * kp; s->look[k] += (l[k] - s->look[k]) * kt; }
  }
  memcpy(eye, s->eye, 12); memcpy(look, s->look, 12);
  if (s->trOn) {
    s->trT += dt * 30.f * (s->trRate > .001f ? s->trRate : .02f); /* круг 7: 0,6 только при скорости 0,02; по данным камеры — своя */ float u = s->trT >= 1.f ? 1.f : s->trT, w = 2.f * u - u * u, d2 = 0.f;
    for (int k = 0; k < 3; k++) { eye[k] = s->trFromE[k] + (s->eye[k] - s->trFromE[k]) * w; look[k] = s->trFromL[k] + (s->look[k] - s->trFromL[k]) * w; d2 += (eye[k] - s->eye[k]) * (eye[k] - s->eye[k]); }
    if (u >= 1.f || d2 < .03f * .03f) s->trOn = 0;
  }
  memcpy(s->outE, eye, 12); memcpy(s->outL, look, 12);
  s->cur = i; *fovY = roomCamFovY(&c[i]);
  return i;
}
