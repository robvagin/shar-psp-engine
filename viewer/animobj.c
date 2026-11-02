// Анимированные объекты уровня (см. animobj.h, данные — tools/animobj_export.py).
#include "animobj.h"
#include <string.h>
#include <math.h>

#ifndef ANIM_COL_POSE_OLD
#define ANIM_COL_POSE_OLD 0
#endif

_Static_assert(sizeof(AoObj) == 80, "AoObj 80 B (tools/animobj_export.py)");
_Static_assert(sizeof(AoVol) == 72, "AoVol 72 B (tools/animobj_export.py)");

int aoInit(AnimObjs *a, const void *prp2, unsigned size, const StateProps *sp, const ColWorld *w) {
  memset(a, 0, sizeof *a); a->want = -1;
  if (!prp2 || size < 64 || memcmp(prp2, "PRP2", 4)) return 1;
  const unsigned char *h = (const unsigned char *)prp2;
  unsigned off = *(const unsigned *)(h + 52), sz = *(const unsigned *)(h + 56);
  if (!off || !sz) return 1;
  if ((off & 15) || (unsigned long long)off + sz > size || sz < 32) return -1;
  const unsigned char *b = h + off;
  if (memcmp(b, "ANM1", 4) || *(const unsigned *)(b + 4) != 1) return -2;
  unsigned nobj = *(const unsigned *)(b + 8), nvol = *(const unsigned *)(b + 12), oo = *(const unsigned *)(b + 16), ov = *(const unsigned *)(b + 20);
  if (nobj > AO_MAXOBJ || (unsigned long long)oo + 80ull * nobj > sz || (unsigned long long)ov + 72ull * nvol > sz) return -3;
  a->obj = (const AoObj *)(b + oo); a->vol = (const AoVol *)(b + ov); a->nobj = nobj; a->nvol = nvol;
  for (unsigned k = 0; k < nobj; k++) {   // сверка ссылок: экземпляры SPR1 и объёмы COL1 существуют
    const AoObj *o = &a->obj[k];
    if (o->nInst && (!sp || (unsigned)o->firstInst + o->nInst > sp->ninst)) return -4;
    if ((unsigned)o->firstVol + o->nVol > nvol) return -5;
    if (o->nVol && !o->nInst) return -5;
  }
  for (unsigned v = 0; v < nvol; v++) {
    const AoVol *V = &a->vol[v];
    if (V->obj >= nobj || (w && (V->kind == 0 ? V->col >= w->nbox : V->col >= w->ncyl))) return -6;
    if (sp && V->joint >= sp->type[sp->inst[a->obj[V->obj].firstInst].type].nJoint) return -6;
  }
  return 0;
}

static int insideR(const AoObj *o, const float p[3], float pad) {   // pad > 0 — шар радиусом pad вокруг точки (приближённо: запас к размерам объёма)
  if (!o->trig) return 0;
  float d[3] = {p[0] - o->c[0], p[1] - o->c[1], p[2] - o->c[2]};
  if (o->trig == 1) { float r = o->h[0] + pad; return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= r * r; }
  for (int k = 0; k < 3; k++) {
    float s = d[0] * o->ax[3 * k] + d[1] * o->ax[3 * k + 1] + d[2] * o->ax[3 * k + 2];
    if (fabsf(s) > o->h[k] + pad) return 0;
  }
  return 1;
}
static int inside(const AoObj *o, const float p[3]) { return insideR(o, p, 0.f); }
void aoSetCar(AnimObjs *a, const float c[4][3]) { memcpy(a->car, c, sizeof a->car); a->carOn = 1; }
// след за суставом (сверка g11 круг 2, P05:7): центр и оси триггера — из позы кадра 0 в текущую позу сустава
static void inv3(const float *M, float *I) {   // M, I — 3x3 по столбцам (элемент строки i, столбца j = M[3 * j + i])
  float a = M[0], b = M[3], c = M[6], d = M[1], e = M[4], f = M[7], g = M[2], h = M[5], i = M[8];
  float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g), k = fabsf(det) > 1e-12f ? 1.f / det : 0.f;
  float r[9] = {(e * i - f * h) * k, (c * h - b * i) * k, (b * f - c * e) * k,
                (f * g - d * i) * k, (a * i - c * g) * k, (c * d - a * f) * k,
                (d * h - e * g) * k, (b * g - a * h) * k, (a * e - b * d) * k};   // по строкам
  for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++) I[3 * col + row] = r[3 * row + col];
}
static void rvT(const float *R, const float *v, float *o) {   // o = R^T * v
  o[0] = R[0] * v[0] + R[1] * v[1] + R[2] * v[2]; o[1] = R[3] * v[0] + R[4] * v[1] + R[5] * v[2]; o[2] = R[6] * v[0] + R[7] * v[1] + R[8] * v[2];
}
static void rvF(const float *R, const float *v, float *o);   // ниже: o = R * v
static void followObj(AnimObjs *a, StateProps *sp, unsigned k, AoObj *oT) {
  const AoObj *o = &a->obj[k]; *oT = *o;
  if (AO_FOLLOW_SV_OLD || o->pad != 1 || !o->pad2 || !o->trig || !o->nInst || !sp || !sp->run) return;
  unsigned i = o->firstInst, j = o->pad2 - 1u; SpRun *r = &sp->run[i];
  if (j >= sp->type[sp->inst[i].type].nJoint) return;
  if (!a->j0ok[k]) { float keep = r->poseF; r->poseF = 0.f; r->poseValid = 0; spPose(sp, i); memcpy(a->j0[k], r->jm + j * 12, 48); r->poseF = keep; r->poseValid = 0; a->j0ok[k] = 1; }
  spPose(sp, i);
  const float *J = r->jm + j * 12, *J0 = a->j0[k]; float I0[9], p[3], q[3];
  inv3(J0, I0);
  float rel[3] = {o->c[0] - r->x, o->c[1] - r->y, o->c[2] - r->z};
  rvT(r->R, rel, p); p[0] -= J0[9]; p[1] -= J0[10]; p[2] -= J0[11];
  rvF(I0, p, q); rvF(J, q, p); p[0] += J[9]; p[1] += J[10]; p[2] += J[11]; rvF(r->R, p, q);
  oT->c[0] = r->x + q[0]; oT->c[1] = r->y + q[1]; oT->c[2] = r->z + q[2];
  for (int m = 0; m < 3; m++) {
    float v[3] = {o->ax[3 * m], o->ax[3 * m + 1], o->ax[3 * m + 2]};
    rvT(r->R, v, p); rvF(I0, p, q); rvF(J, q, p); rvF(r->R, p, q);
    float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (l > 1e-6f) { q[0] /= l; q[1] /= l; q[2] /= l; }
    oT->ax[3 * m] = q[0]; oT->ax[3 * m + 1] = q[1]; oT->ax[3 * m + 2] = q[2];
  }
}

// R по столбцам (как stateprops.c): o = R * v
static void rv(const float *R, const float *v, float *o) {
  o[0] = R[0] * v[0] + R[3] * v[1] + R[6] * v[2];
  o[1] = R[1] * v[0] + R[4] * v[1] + R[7] * v[2];
  o[2] = R[2] * v[0] + R[5] * v[1] + R[8] * v[2];
}
static void rvF(const float *R, const float *v, float *o) { rv(R, v, o); }
static float nrm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-12f) { v[0] /= l; v[1] /= l; v[2] /= l; } return l; }

static void placeVols(AnimObjs *a, StateProps *sp, ColWorld *w, unsigned k) {
  const AoObj *o = &a->obj[k]; unsigned i = o->firstInst; const SpRun *r = &sp->run[i];
  spPose(sp, i);
  for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol; v++) {
    const AoVol *V = &a->vol[v]; const float *J = r->jm + V->joint * 12;
    float jc[3], c[3], t[3];
    rv(J, V->c, jc); jc[0] += J[9]; jc[1] += J[10]; jc[2] += J[11];
    rv(r->R, jc, c); c[0] += r->x; c[1] += r->y; c[2] += r->z;
    if (V->kind == 0) {
      ColBox *B = (ColBox *)&w->box[V->col];   // секция COL1 лежит в ОЗУ уровня: живые ящики переставляем на месте
      float ax[3][3], s[3];
      for (int q = 0; q < 3; q++) { rv(J, V->a + 3 * q, t); rv(r->R, t, ax[q]); s[q] = nrm(ax[q]); }
      float d = ax[0][0] * ax[1][0] + ax[0][1] * ax[1][1] + ax[0][2] * ax[1][2];   // правая ортонормированная тройка
      for (int q = 0; q < 3; q++) ax[1][q] -= ax[0][q] * d;
      nrm(ax[1]);
      ax[2][0] = ax[0][1] * ax[1][2] - ax[0][2] * ax[1][1]; ax[2][1] = ax[0][2] * ax[1][0] - ax[0][0] * ax[1][2];
      ax[2][2] = ax[0][0] * ax[1][1] - ax[0][1] * ax[1][0];
      memcpy(B->c, c, sizeof c); memcpy(B->ax, ax[0], 12); memcpy(B->ay, ax[1], 12); memcpy(B->az, ax[2], 12);
      for (int q = 0; q < 3; q++) B->h[q] = V->h[q] * s[q];
    } else {
      ColCyl *C = (ColCyl *)&w->cyl[V->col];
      float ax[3]; rv(J, V->a, t); rv(r->R, t, ax); float s = nrm(ax);
      if (ax[1] < 0) { ax[0] = -ax[0]; ax[1] = -ax[1]; ax[2] = -ax[2]; }
      memcpy(C->c, c, sizeof c); memcpy(C->axis, ax, sizeof ax); C->r = V->h[0]; C->halfLen = V->h[1] * s;
    }
    a->moved++;
  }
}

static int nearObj(const StateProps *sp, const AoObj *o, float px, float pz) { const SpRun *s = &sp->run[o->firstInst]; float dx = s->x - px, dz = s->z - pz; return dx * dx + dz * dz < 40.f * 40.f; }   // проба клипа — когда игрок ближе 40 м
static float boxTop(const ColBox *b) { return b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2]; }
// Проба клипа объекта с живыми ящиками (поза в нескольких кадрах; поза и ящики потом возвращаются на текущий кадр): 1 — подъёмник (ящик поднимается больше LIFT_MIN_RISE),
// 3 — горизонтальная платформа (центр ящика уезжает на TOGGLE_MIN_MOVE и больше), 2 — ни то ни другое. Для подъёмника малого хода считает растяжение хода liftK
// (все ящики к концу клипа доходят до верха самого высокого ящика в покое: с любого два прыжка до крыши) и начальный кадр движения liftF0.
static int liftProbe(AnimObjs *a, StateProps *sp, ColWorld *w, unsigned k) {
  const AoObj *o = &a->obj[k]; AoRun *r = &a->run[k]; if (!w || !sp->run || !o->nVol) return 2;
  float last = o->nfr > 1 ? (float)(o->nfr - 1) : 0.f, top0[16], c0[16][3], rise[16] = {0}, move = 0.f, f0 = -1.f; unsigned nb = 0, nbox = 0;
  for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst; i++) a->liftK[k] = 0.f, sp->run[i].liftK = 0.f;   // проба по клипу как нарисован
  for (int s = 0; s < 5 + (int)last; s++) {   // 0..4 — пять проб по клипу, дальше — кадр за кадром до первого движения (начало подъёма)
    float fr = s < 5 ? last * s * .25f : (float)(s - 5);
    for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst; i++) { sp->run[i].poseF = fr; sp->run[i].poseValid = 0; }
    placeVols(a, sp, w, k); nb = 0;
    for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol && nb < 16; v++, nb++) {
      const AoVol *V = &a->vol[v]; if (V->kind != 0) continue;
      const ColBox *b = &w->box[V->col]; float t = boxTop(b);
      if (s == 0) { top0[nb] = t; memcpy(c0[nb], b->c, sizeof c0[nb]); nbox++; }
      else { if (t - top0[nb] > rise[nb]) rise[nb] = t - top0[nb];
             float hd = hypotf(b->c[0] - c0[nb][0], b->c[2] - c0[nb][2]); if (hd > move) move = hd;
             if (f0 < 0.f && s >= 5 && (fabsf(t - top0[nb]) > .03f || hd > .03f)) f0 = fr; } }
  }
  float maxRise = 0.f, minRise = 1e9f, minTop = 1e9f, maxTop = -1e9f; unsigned nr = 0;
  for (unsigned q = 0; q < nb; q++) if (rise[q] > LIFT_MIN_RISE * .6f) { nr++; if (rise[q] < minRise) minRise = rise[q]; if (top0[q] < minTop) minTop = top0[q]; if (top0[q] > maxTop) maxTop = top0[q]; }
  for (unsigned q = 0; q < nb; q++) if (rise[q] > maxRise) maxRise = rise[q];
  a->liftF0[k] = f0 > 6.f ? f0 - 6.f : 0.f; a->liftK[k] = 1.f;
  int cls = maxRise > LIFT_MIN_RISE ? 1 : move >= TOGGLE_MIN_MOVE ? 3 : 2;
  if (cls == 1 && !DUFF_LIFT_BOOST_OLD && maxRise < LIFT_SMALL_RISE && nr >= 2 && minRise > .3f) {
    float K = 1.f + (maxTop - minTop) / minRise; if (K > LIFT_KMAX) K = LIFT_KMAX; if (K > 1.05f) a->liftK[k] = K;
  }
  for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst; i++) { sp->run[i].liftK = a->liftK[k] > 1.f ? a->liftK[k] : 0.f; sp->run[i].poseF = r->f; sp->run[i].poseValid = 0; }
  placeVols(a, sp, w, k);
  (void)nbox; return cls;
}
// Гомер стоит ногами на верху живого ящика объекта (поверху ±0.25 м, по плоскости в пределах ящика +0.3 м)
static int standsOnLift(const AnimObjs *a, const ColWorld *w, unsigned k, const float f[3]) {
  const AoObj *o = &a->obj[k];
  for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol; v++) {
    const AoVol *V = &a->vol[v]; if (V->kind != 0) continue;
    const ColBox *b = &w->box[V->col]; float top = boxTop(b);
    if (fabsf(f[1] - top) > .25f) continue;
    float d[3] = {f[0] - b->c[0], f[1] - b->c[1], f[2] - b->c[2]}; int ok = 1;
    const float *ax[3] = {b->ax, b->ay, b->az};
    for (int q = 0; q < 3; q++) { if (fabsf(ax[q][1]) > .7f) continue; float s = d[0] * ax[q][0] + d[1] * ax[q][1] + d[2] * ax[q][2]; if (fabsf(s) > b->h[q] + .3f) ok = 0; }
    if (ok) return 1;
  }
  return 0;
}
#ifndef AO_PLAYONCE_OLD
#define AO_PLAYONCE_OLD 0
#endif
#include <strings.h>
int aoNamesAdopt(AnimObjs *a, StateProps *sp, const void *buf, unsigned size) {
  a->nm = NULL; a->evt = NULL; a->nevt = 0; a->bA = a->bB = -1; a->bFired = 0;
  if (ANIMNAMES_SV_OLD || !a->obj || !buf || size < 32 || memcmp(buf, "OBJN", 4)) return -1;
  const unsigned char *b = (const unsigned char *)buf; const unsigned *u = (const unsigned *)(b + 4);
  unsigned ver = u[0], no = u[1], nt = u[2], ny = u[3], oo = u[4], ot = u[5], oy = u[6];
  if (ver != 1 || no != a->nobj || (unsigned long long)oo + 32ull * no > size || (unsigned long long)ot + 80ull * nt > size || (unsigned long long)oy + 16ull * ny > size) return -1;
  const AoName *nm = (const AoName *)(b + oo);
  for (unsigned k = 0; k < no; k++) if (nm[k].action != a->obj[k].action || nm[k].nfr != a->obj[k].nfr) return -1;   // другой экспорт: имена не сопоставить
  a->nm = nm; a->evt = (const AoEvTrig *)(b + ot); a->nevt = nt;
  if (sp && sp->run && ny) spFollowAdopt(sp, b + oy, ny);
  for (unsigned k = 0; k < no; k++) {
    if (!strcasecmp(nm[k].name, "pidgeon_A_group")) a->bA = (int)k;
    else if (!strcasecmp(nm[k].name, "pidgeon_B_group")) a->bB = (int)k;
  }
  if (a->bA >= 0 && a->bB >= 0 && nt && sp && sp->run) {   // стая взлёта ждёт события: спрятана и стоит
    const AoObj *ob = &a->obj[a->bB]; a->bLen = (ob->fps > 0.f ? (float)ob->nfr / ob->fps : 1.f) + .2f;
    for (unsigned i = ob->firstInst; i < (unsigned)ob->firstInst + ob->nInst; i++) sp->run[i].alive = 0;
  } else a->bA = a->bB = -1;
  return 0;
}
// голуби (P11-16): игрок вошёл в объём локатора flyingpidgeon (событие 47) — сидящая стая прячется и стоит, стая взлёта сбрасывается, играется и становится видимой, звук взлёта; отошёл дальше
// BIRDS_REARM м — стая вернулась на место. BIRDS_SV_OLD=1 — стаи не трогаем
#ifndef BIRDS_SV_OLD
#define BIRDS_SV_OLD 0
#endif
#define BIRDS_REARM 150.f
static void birdsStep(AnimObjs *a, StateProps *sp, const float feet[3], int onFoot, float dt) {
  if (BIRDS_SV_OLD || a->bA < 0 || a->bB < 0 || !a->nevt) return;
  const AoEvTrig *e = &a->evt[0]; AoObj t; memset(&t, 0, sizeof t);
  t.trig = e->rect ? 2 : 1; memcpy(t.c, e->c, 12); memcpy(t.h, e->h, 12); memcpy(t.ax, e->ax, 36);
  const AoObj *oa = &a->obj[a->bA], *ob = &a->obj[a->bB];
  if (!a->bFired) {
    int in = onFoot ? inside(&t, feet) : insideR(&t, feet, 1.5f);
    for (int q = 0; q < 4 && !in && !onFoot && a->carOn; q++) in = inside(&t, a->car[q]);
    if (!in) return;
    a->bFired = 1; a->bT = 0.f;
    for (unsigned i = oa->firstInst; i < (unsigned)oa->firstInst + oa->nInst; i++) sp->run[i].alive = 0;
    for (unsigned i = ob->firstInst; i < (unsigned)ob->firstInst + ob->nInst; i++) spEnter(sp, i, 0);
    if (a->sound) a->sound(a->soundUser, "pigeon_takeoff", e->c[0], e->c[1], e->c[2]);
  } else {
    a->bT += dt;
    if (a->bT > a->bLen) for (unsigned i = ob->firstInst; i < (unsigned)ob->firstInst + ob->nInst; i++) sp->run[i].alive = 0;   // улетели
    float dx = feet[0] - e->c[0], dz = feet[2] - e->c[2];
    if (dx * dx + dz * dz > BIRDS_REARM * BIRDS_REARM) {
      a->bFired = 0;
      for (unsigned i = oa->firstInst; i < (unsigned)oa->firstInst + oa->nInst; i++) spEnter(sp, i, 0);
      for (unsigned i = ob->firstInst; i < (unsigned)ob->firstInst + ob->nInst; i++) sp->run[i].alive = 0;
    }
  }
}
void aoStep(AnimObjs *a, StateProps *sp, ColWorld *w, float px, float py, float pz, int onFoot, int action, float dt) {
  if (!a->obj || !sp || !sp->run) return;
  float feet[3] = {px, py, pz}, body[3] = {px, py + 0.9f, pz};   // в объёме триггера — ступни или середина тела игрока
  birdsStep(a, sp, feet, onFoot, dt);   // круг 5 (g11): голуби
  int press = action ? a->want : -1;
  if (press >= 0) a->presses++;
  a->want = -1;
  for (unsigned k = 0; k < a->nobj; k++) {
    const AoObj *o = &a->obj[k]; AoRun *r = &a->run[k];
    float last = o->nfr > 1 ? (float)(o->nfr - 1) : 0.f, fps = o->fps > 0.f ? o->fps : 30.f;
    int wasPlay = r->playing, wasIn = r->inTrig;
    AoObj oT; followObj(a, sp, k, &oT);   // сверка g11 круг 2 (P05:7): триггер едет за суставом
    int in;
    if (AOTRIG_SV_OLD) in = inside(o, feet) || inside(o, body);
    else if (onFoot) in = inside(&oT, feet);   // сверка g11 круг 2 (P05:9): пешком — одна точка (ступни), без запаса
    else {   // из машины — шар 1,5 м вокруг центра плюс углы коробки
      in = insideR(&oT, feet, 1.5f);
      for (int q = 0; q < 4 && !in && a->carOn; q++) in = inside(&oT, a->car[q]);
    }
    switch (o->action) {
      case AO_PLAYANIM:   // △ у выключателя: клип один раз с начала (подъёмник Duff: вверх, пауза, вниз — кадр 0 в конце)
        if ((int)k == press && !r->playing && r->delay <= 0.f) {
          if (AO_SWITCH_OLD || !ANIMPRESS_SV_OLD) { r->playing = 1; r->f = 0.f; a->plays++; } else r->delay = AO_PRESS_DELAY;   // правка 462: старт клипа с задержкой; сверка g11 P05-4: задержки у нажатия нет
        }
        if (!DUFF_LIFT_OLD && ANIMAUTO_SV_OLD && onFoot && o->nVol && w) {   // №519: встал на платформу подъёмника — едет сам (сверка g11 P05-5: ANIMAUTO_SV_OLD=0 — только по △)
          if (!r->pad[1] && nearObj(sp, o, px, pz)) r->pad[1] = (uint8_t)liftProbe(a, sp, w, k);
          if (r->pad[1] == 1 && !r->playing && standsOnLift(a, w, k, feet)) { r->playing = 1; r->f = DUFF_LIFT_SKIP_OLD ? 0.f : a->liftF0[k]; r->delay = 0.f; a->plays++; }
        }
        if (r->delay > 0.f) { r->delay -= dt; if (r->delay <= 0.f) { r->delay = 0.f; r->playing = 1; r->f = 0.f; a->plays++; } }
        if (onFoot && in && !r->playing && r->delay <= 0.f && a->want < 0) a->want = (int)k;
        if (r->playing) { r->f += dt * fps; if (r->f >= last) { r->f = ANIMHOLD_SV_OLD ? 0.f : last; r->playing = 0; } }   // сверка g11 P05-6: по окончании клип держит последний кадр
        break;
      case AO_PLAYONCE:   // круг 5 (g07b, P05-18): первое △ играет клип один раз вперёд и навсегда снимает триггер (inTrig = уже сработал). AO_PLAYONCE_OLD=1 — не обрабатывается (как до круга)
        if (AO_PLAYONCE_OLD) break;
        if ((int)k == press && !r->inTrig && !r->playing) { r->playing = 1; r->f = 0.f; r->inTrig = 1; a->plays++; }
        if (onFoot && in && !r->inTrig && !r->playing && a->want < 0) a->want = (int)k;
        if (r->playing) { r->f += dt * fps; if (r->f >= last) { r->f = last; r->playing = 0; } }
        break;
      case AO_AUTOPLAY:   // вход в объём: клип один раз (ветка качается, платформа-лифт АЭС)
        if (in && !r->inTrig && !r->playing) { r->playing = 1; r->f = 0.f; a->plays++; }
        if (r->playing) { r->f += dt * fps; if (r->f >= last) { r->f = ANIMHOLD_SV_OLD ? 0.f : last; r->playing = 0; } }   // сверка g11 P05-6: по окончании клип держит последний кадр
        break;
      case AO_TOGGLE:     // △ у выключателя: клип по кругу / пауза на текущем кадре
        if ((int)k == press) { r->playing = !r->playing; a->plays++; }
        if (!TOGGLE_AUTO_OLD && ANIMAUTO_SV_OLD && onFoot && o->nVol && w) {   // №519: платформа АЭС (сдвиг ≥ 3 м) или подъёмник Toggle едет сама, пока на ней стоят; давно пустая — в начале цикла останавливается
          if (!r->pad[1] && nearObj(sp, o, px, pz)) r->pad[1] = (uint8_t)liftProbe(a, sp, w, k);
          if (r->pad[1] == 1 || r->pad[1] == 3) {
            if (standsOnLift(a, w, k, feet)) { a->idleT[k] = 0.f; if (!r->playing) { r->playing = 1; r->f = DUFF_LIFT_SKIP_OLD ? 0.f : a->liftF0[k]; a->plays++; } }
            else if (r->playing && (a->idleT[k] += dt) > TOGGLE_IDLE_STOP && r->f < 6.f) { r->playing = 0; r->f = 0.f; a->idleT[k] = 0.f; }
          }
        }
        if (onFoot && in && a->want < 0) a->want = (int)k;
        if (r->playing && last > 0.f) r->f = fmodf(r->f + dt * fps, last);
        break;
      case AO_AUTODOOR:   // игрок в объёме — открывается до конца клипа, вне — закрывается обратно к кадру 0
        r->f += (in ? dt : -dt) * fps; if (r->f < 0.f) r->f = 0.f; if (r->f > last) r->f = last;
        break;
      default: break;     // AO_NONE / AO_DOORBELL: кадр ведёт состояние SPR1 (цикл из данных)
    }
    r->inTrig = (uint8_t)in;
    // круг 5 (g11, P11-15): звук подвижного объекта по его имени на запуске клипа; у объектов, что крутятся сами (платформы АЭС без кнопки), — раз за круг клипа, пока игрок ближе 60 м.
    // Имена и таблица звуков (sound.c sndAnimObj): нет звука у объекта — больше не спрашиваем
    if (a->nm && a->sound && !a->nosnd[k] && o->nInst) {
      int go = (!wasPlay && r->playing) || (o->action == AO_AUTODOOR && in && !wasIn);
      const SpRun *s0 = &sp->run[o->firstInst]; float dxs = s0->x - px, dzs = s0->z - pz;
      if (o->action == AO_NONE && o->nfr > 1 && s0->alive && dxs * dxs + dzs * dzs < 60.f * 60.f) { if ((a->sndT[k] -= dt) <= 0.f) { go = 1; a->sndT[k] = (float)o->nfr / (o->fps > 0.f ? o->fps : 30.f); } }
      if (go && !a->sound(a->soundUser, a->nm[k].name, s0->x, s0->y, s0->z)) a->nosnd[k] = 1;
    }
    if (o->action != AO_NONE && o->action != AO_DOORBELL)
      for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst; i++) {
        SpRun *s = &sp->run[i]; if (s->poseF != r->f) { s->poseF = r->f; s->poseValid = 0; }
      }
    if (o->nVol && w) {
      const SpRun *s = &sp->run[o->firstInst];
      float dx = s->x - px, dz = s->z - pz;
      // All parts share the animated pose, including remote clusters of long objects.
      // The first drawable origin is not a bound for the collision volumes.
      if (!ANIM_COL_POSE_OLD || dx * dx + dz * dz < AO_NEAR * AO_NEAR || !r->pad[0]) { placeVols(a, sp, w, k); r->pad[0] = 1; }
    }
  }
}

int aoSwitchRing(const AnimObjs *a, unsigned k, float c[3], float *r) {
  if (AO_SWITCH_OLD || !a->obj || k >= a->nobj) return 0;
  const AoObj *o = &a->obj[k];
  if ((o->action != AO_PLAYANIM && o->action != AO_TOGGLE && !(o->action == AO_PLAYONCE && !AO_PLAYONCE_OLD && !a->run[k].inTrig)) || !o->trig) return 0;
  c[0] = o->c[0]; c[2] = o->c[2];
  if (o->trig == 1) { c[1] = o->c[1]; *r = o->h[0] > 0.6f ? o->h[0] : 0.6f; }   // сфера: центр и радиус
  else { c[1] = o->c[1] - o->h[1]; *r = fmaxf(o->h[0], o->h[2]) > 0.6f ? fmaxf(o->h[0], o->h[2]) : 0.6f; }   // ящик: основание
  return 1;
}
int aoWantsAction(const AnimObjs *a) { return a->obj ? a->want : -1; }

void aoPlace(AnimObjs *a, StateProps *sp, ColWorld *w, unsigned k) {
  if (!a->obj || !sp || !sp->run || !w || k >= a->nobj || !a->obj[k].nVol) return;
  placeVols(a, sp, w, k); a->run[k].pad[0] = 1;
}
