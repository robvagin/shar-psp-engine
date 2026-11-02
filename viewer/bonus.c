// Бонус-игра: правила гонки (viewer/bonus.h — что из данных, что правило порта)
#include <string.h>
#include <math.h>
#include "bonus.h"

_Static_assert(sizeof(BnsHdr) == 128, "BnsHdr 128 B");
_Static_assert(sizeof(BnsPick) == 32, "BnsPick 32 B (tools/bonus/export.py PK_FMT)");
_Static_assert(sizeof(BnsHaz) == 128, "BnsHaz 128 B (tools/bonus/export.py HZ_FMT)");

int bnsParse(BnsTrack *t, const void *data, unsigned size) {
  memset(t, 0, sizeof *t);
  if (size < sizeof(BnsHdr)) return -2;
  const BnsHdr *h = (const BnsHdr *)data;
  if (memcmp(h->magic, "BNS1", 4) || (h->ver != 1 && h->ver != 2)) return -1;
  if (h->ncp < 2 || h->ncp > BNS_MAXCP || h->nstart != BR_MAX || h->nwpf > BNS_MAXWP || h->nwpr > BNS_MAXWP) return -3;
  if (h->npathf > BNS_MAXPATH || h->npathr > BNS_MAXPATH) return -3;
  unsigned npk = h->ver >= 2 ? h->npick : 0, nhz = h->ver >= 2 ? h->nhaz : 0;   // версия 2: подборы и зоны после пути ИИ
  if (npk > BNS_MAXPICK || nhz > BNS_MAXHAZ) return -3;
  unsigned need = sizeof(BnsHdr) + h->ncp * sizeof(BnsCp) + h->nstart * sizeof(BnsStart) + (h->nwpf + h->nwpr) * sizeof(BnsWp) +
                  (h->npathf + h->npathr) * sizeof(BnsPath) + npk * sizeof(BnsPick) + nhz * sizeof(BnsHaz);
  if (size < need) return -2;
  const char *p = (const char *)data + sizeof(BnsHdr);
  t->h = h; t->cp = (const BnsCp *)p; p += h->ncp * sizeof(BnsCp);
  t->st = (const BnsStart *)p; p += h->nstart * sizeof(BnsStart);
  t->wpf = (const BnsWp *)p; p += h->nwpf * sizeof(BnsWp); t->wpr = (const BnsWp *)p; p += h->nwpr * sizeof(BnsWp);
  t->pf = (const BnsPath *)p; p += h->npathf * sizeof(BnsPath); t->pr = (const BnsPath *)p; p += h->npathr * sizeof(BnsPath);
  t->pk = (const BnsPick *)p; t->npick = npk; p += npk * sizeof(BnsPick); t->hz = (const BnsHaz *)p; t->nhaz = nhz;
  for (unsigned i = 0; i < nhz; i++) for (int k = 0; k < 3; k++) if (!(t->hz[i].h[k] > 0.f && t->hz[i].h[k] < 500.f)) return -3;
  for (unsigned i = 0; i < h->ncp; i++) for (int k = 0; k < 3; k++) if (!(t->cp[i].h[k] > 0.f && t->cp[i].h[k] < 500.f)) return -3;
  return 0;
}

int bnsInside(const BnsCp *c, const float p[3], float grow) {
  float d[3] = {p[0] - c->c[0], p[1] - c->c[1], p[2] - c->c[2]};
  if (!c->rect) return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= (c->h[0] + grow) * (c->h[0] + grow);
  for (int k = 0; k < 3; k++) {
    float u = d[0] * c->ax[k][0] + d[1] * c->ax[k][1] + d[2] * c->ax[k][2];
    if (fabsf(u) > c->h[k] + grow) return 0;
  }
  return 1;
}

static float d2xz(const float *a, const float *b) { float x = a[0] - b[0], z = a[2] - b[2]; return x * x + z * z; }

void brInit(BonusRace *b, const BnsTrack *t, int laps, int reverse, int n, int human) {
  memset(b, 0, sizeof *b);
  b->t = *t; b->laps = laps < BR_LAPS_MIN ? BR_LAPS_MIN : laps > BR_LAPS_MAX ? BR_LAPS_MAX : laps;
  b->reverse = reverse != 0; b->n = n < 1 ? 1 : n > BR_MAX ? BR_MAX : n; b->human = human;
  b->ncp = (int)t->h->ncp;
  // порядок хода: финиш (номер 1 в данных, cp[0]), дальше по номерам; обратный ход — от последнего к первому
  b->order[0] = 0;
  for (int k = 1; k < b->ncp; k++) b->order[k] = reverse ? b->ncp - k : k;
  // остановки ИИ на круге — центры чекпойнтов хода по порядку, финиш последним (правило порта: точки WPR/WPL данных лежат
  // внутри плотного пути между ними, tools/bonus/export.py; номера WPR у b05/b07 не идут по кругу)
  b->nwp = 0;
  for (int k = 1; k <= b->ncp && b->nwp <= BNS_MAXWP; k++) {
    const BnsCp *c = &t->cp[b->order[k % b->ncp]];
    memcpy(b->wp[b->nwp], c->c, 12); b->wpr[b->nwp] = c->h[0]; b->nwp++;
  }
  b->path = reverse ? t->pr : t->pf; b->npath = (int)(reverse ? t->h->npathr : t->h->npathf);
  for (int k = 0; k < b->nwp; k++) b->stopAt[k] = -1;
  for (int i = 0; i < b->npath; i++) { int k = (int)b->path[i].tag; if (k >= 0 && k < b->nwp && b->stopAt[k] < 0) b->stopAt[k] = i; }
  for (int k = 0; k < b->nwp; k++) if (b->stopAt[k] < 0) b->npath = 0;   // путь без всех остановок — не пользоваться
  if (b->npath) for (int k = 0; k < b->nwp; k++) memcpy(b->wp[k], b->path[b->stopAt[k]].p, 12);   // остановка — точка пути в объёме
  for (int i = 0; i < b->n; i++) {
    BrRacer *r = &b->r[i]; r->next = b->ncp > 1 ? 1 : 0; r->bestLap = 0.f; r->place = i + 1;
    float h; brStart(b, i, r->lastPos, &h);
  }
  b->state = BR_COUNT; b->count = BR_COUNT_S;
  for (int i = 0; i < BR_MAX; i++) { b->nitro[i] = BR_NITRO3_OLD ? 0 : BR_NITRO_CHARGES; b->aiHornT[i] = BR_AIHORN3_OLD ? 0.f : BR_AI_HORN_GAP_S; b->aiChkT[i] = 0.f; }   // круг 3: таймер между гудками ИИ стартует с полного значения, первый гудок не раньше 10 с гонки   // сверка g07b P17-14: 3 заряда в начале заезда
  b->rng = 0x2545F491u ^ (uint32_t)(t->h ? t->h->track : 0u) * 2654435761u; b->dnfOn = 0; b->dnfT = 0.f;
}

void brStart(const BonusRace *b, int i, float pos[3], float *h) {
  const BnsStart *s = &b->t.st[i < (int)b->t.h->nstart ? i : 0];
  memcpy(pos, s->p, 12);
  float a = s->h + (b->reverse ? 3.14159265f : 0.f);
  *h = atan2f(sinf(a), cosf(a));
}

static float progOf(const BonusRace *b, const BrRacer *r, const float p[3]) {
  int prev = (r->next + b->ncp - 1) % b->ncp;
  const float *A = b->t.cp[b->order[prev]].c, *B = b->t.cp[b->order[r->next]].c;
  float L = sqrtf(d2xz(A, B)) + 1e-3f, d = sqrtf(d2xz(p, B));
  float f = 1.f - d / L; if (f < 0.f) f = 0.f; if (f > 1.f) f = 1.f;
  int passed = r->next == 0 ? b->ncp - 1 : r->next - 1;   // пройдено в этом круге (финиш старта не считается)
  return (float)(r->lap * b->ncp + passed) + f;
}

static int brAhead(const BrRacer *a, int ia, const BrRacer *b, int ib) {   // a впереди b?
  if (a->finished != b->finished) return a->finished;
  if (a->finished) return a->finishT < b->finishT || (a->finishT == b->finishT && ia < ib);
  return a->prog > b->prog || (a->prog == b->prog && ia < ib);
}
unsigned brUpdate(BonusRace *b, const float (*pos)[3], float dt) {
  unsigned lapMask = 0;
  if (b->state == BR_COUNT) {
    b->count -= dt;
    if (b->count <= 0.f) b->state = BR_RACE;
    return 0;
  }
  if (b->state == BR_DONE) return 0;
  b->time += dt;
  for (int i = 0; i < BR_MAX; i++) if (b->nitroGap[i] > 0.f) b->nitroGap[i] -= dt;   // сверка g07b: пауза между гудками
  for (int i = 0; i < b->n; i++) {
    BrRacer *r = &b->r[i]; if (r->finished) continue;
    r->lapT += dt; r->totalT += dt;
    const BnsCp *c = &b->t.cp[b->order[r->next]];
    if (bnsInside(c, pos[i], b->grow[i])) {
      r->cps++;
      if (r->next == 0) {   // финиш после всех чекпойнтов круга — круг
        r->lap++; lapMask |= 1u << i;
        if (r->bestLap <= 0.f || r->lapT < r->bestLap) r->bestLap = r->lapT;
        r->lapT = 0.f;
        if (r->lap >= b->laps) { r->finished = 1; r->finishT = b->time; b->finishedN++; if (b->finishedN == 1) b->sinceFirst = 0.f; }
      }
      r->next = (r->next + 1) % b->ncp;
    }
    r->prog = progOf(b, r, pos[i]);
  }
  // места: доехавшие — по времени финиша, остальные — по мере пути
  for (int i = 0; i < b->n; i++) {
    int pl = 1;
    for (int j = 0; j < b->n; j++) if (j != i && brAhead(&b->r[j], j, &b->r[i], i)) pl++;
    b->r[i].place = pl;
  }
  if (b->finishedN) b->sinceFirst += dt;
#ifndef BONUS_DNF_HUMAN_OLD
#define BONUS_DNF_HUMAN_OLD 0
#endif
  int hd = b->human >= 0 && b->r[b->human].finished;
  // сверка g07b-msnview (P17-15): 10 с до DNF начинаются, когда доехал предпоследний (n−1 из n) или доехали все люди; последний финишёр обрывает отсчёт сразу (finishedN == n ниже).
  // Было: 20 с от первого финишёра (BR_DNF_S). BR_DNF10_OLD=1 — прежнее
  if (!BR_DNF10_OLD) { if (!b->dnfOn && ((b->n > 1 && b->finishedN >= b->n - 1) || hd)) { b->dnfOn = 1; b->dnfT = 0.f; } if (b->dnfOn) b->dnfT += dt; }
  int timedOut = !BR_DNF10_OLD ? (b->dnfOn && b->dnfT >= BR_DNF10_S) : BONUS_DNF_HUMAN_OLD ? (hd && b->time - b->r[b->human].finishT >= BR_DNF_S)
                                  : (b->finishedN && b->sinceFirst >= BR_DNF_S);
  if (b->finishedN == b->n || timedOut) {
    for (int i = 0; i < b->n; i++) if (!b->r[i].finished) b->r[i].dnf = 1;
    b->state = BR_DONE;
  }
  return lapMask;
}

int brPlaceOf(const BonusRace *b, int i) { return b->r[i].place; }

int brPoints(int place) {   // measured PS2 finish points 4/3/2/1; DNF handled separately
  return place >= 1 && place <= 4 ? 5 - place : 0;
}

void brTopLens(const BnsTrack *t, float *fovyDeg, float *nearZ, float *farZ) {
  if (!BONUS_CAMERA_PS2_OLD && t->h->track == 1) {
    *fovyDeg = 17.2892584f; *nearZ = 150.f; *farZ = 800.f;
  }
}

void brTopCam(const BnsTrack *t, float fovyDeg, float aspect, float eye[3], float at[3], float up[3], float *dist) {
  const BnsHdr *h = t->h;
  if (!BONUS_CAMERA_PS2_OLD && h->track == 1) {
    // Retail P3D/EE uses mirrored Z (tools/bonus/track.py _mirror).
    const float e[3] = {26.88f, 157.87f, 206.23f};
    const float u[3] = {-0.0781452f, 0.7965135f, -0.5995493f};
    for (int k = 0; k < 3; k++) { eye[k] = e[k]; at[k] = 0.f; up[k] = u[k]; }
    if (dist) *dist = 261.105881f; return;
  }
  float f[3] = {h->look[0], h->look[1], h->look[2]}, u[3] = {h->up[0], h->up[1], h->up[2]};
  float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); for (int k = 0; k < 3; k++) f[k] /= fl;
  // правая ось r = f × u, верх заново u = r × f (ортонормально)
  float r[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
  float rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]); for (int k = 0; k < 3; k++) r[k] /= rl;
  u[0] = r[1] * f[2] - r[2] * f[1]; u[1] = r[2] * f[0] - r[0] * f[2]; u[2] = r[0] * f[1] - r[1] * f[0];
  const float *bb = h->bbox;
  float c[3] = {(bb[0] + bb[3]) * .5f, (bb[1] + bb[4]) * .5f, (bb[2] + bb[5]) * .5f};
  float tv = tanf(fovyDeg * 3.14159265f / 360.f), th = tv * aspect, d = 1.f;
  for (int k = 0; k < 8; k++) {
    float p[3] = {(k & 1 ? bb[3] : bb[0]) - c[0], (k & 2 ? bb[4] : bb[1]) - c[1], (k & 4 ? bb[5] : bb[2]) - c[2]};
    float x = p[0] * r[0] + p[1] * r[1] + p[2] * r[2], y = p[0] * u[0] + p[1] * u[1] + p[2] * u[2];
    float z = p[0] * f[0] + p[1] * f[1] + p[2] * f[2];   // глубже цели — дальше от глаза
    float dx = fabsf(x) / th - z, dy = fabsf(y) / tv - z;
    if (dx > d) d = dx; if (dy > d) d = dy;
  }
  for (int k = 0; k < 3; k++) { at[k] = c[k]; eye[k] = c[k] - f[k] * d; up[k] = u[k]; }
  if (dist) *dist = d;
}

int brStuck(BonusRace *b, int i, const float pos[3], float dt) {
  BrRacer *r = &b->r[i]; (void)pos;
  if (r->finished || b->state != BR_RACE) { r->stuckT = 0.f; r->bestProg = r->prog; return 0; }
  if (r->prog > r->bestProg + BR_STUCK_PROG) { r->bestProg = r->prog; r->stuckT = 0.f; return 0; }   // продвинулась к чекпойнту
  r->stuckT += dt;
  if (r->stuckT < BR_STUCK_S) return 0;
  r->stuckT = 0.f; r->bestProg = r->prog; return 1;
}

int brPlan(const BonusRace *b, int goal, const float pos[3], float (*out)[3], int cap) {
  if (b->npath < 2 || b->nwp < 1 || cap < 2) return 0;
  int g = goal % b->nwp, pr = (g + b->nwp - 1) % b->nwp, N = b->npath;
  int i0 = b->stopAt[pr], i1 = b->stopAt[g];
  int len = (i1 - i0 + N) % N; if (len == 0) len = N;
  // ближняя к машине точка отрезка пути pr -> g (по xz; ярусы моста различает высота)
  int best = i0; float bd = 1e30f;
  for (int s = 0; s <= len; s++) {
    const float *q = b->path[(i0 + s) % N].p; float dy = q[1] - pos[1];
    float d = d2xz(q, pos) + 4.f * dy * dy;
    if (d < bd) { bd = d; best = s; }
  }
  if (best < len) best++;   // следующая точка — впереди машины
  int n = 0;
  for (int s = best; s <= len && n < cap - 1; s++) memcpy(out[n++], b->path[(i0 + s) % N].p, 12);
  if (n == 0 || (n < cap && best + n - 1 >= len)) {   // дошли до остановки — последняя точка — сама остановка данных
    if (n == 0) n = 1; memcpy(out[n - 1], b->wp[g], 12);
  }
  return n;
}

void brKeepAlive(Car *k) {
  if (k->damage > BR_DMG_MAX) k->damage = BR_DMG_MAX;
  k->destroyed = 0; k->deadT = 0.f;
}

int brRespawn(BonusRace *b, int i, const float pos[3], float out[3], float *h) {
  BrRacer *rr = &b->r[i]; int goal = brGoal(b, i);
  if (rr->respawnCp != rr->cps) { rr->respawnCp = rr->cps; rr->respawnN = 0; }
  rr->respawnN++;
  float r[BNS_MAXPATH][3]; int n = brPlan(b, goal, pos, r, BNS_MAXPATH);
  if (n < 2) return 0;
  int k = BR_RESPAWN_AHEAD < n - 1 ? BR_RESPAWN_AHEAD : n - 2;
  if (rr->respawnN >= 3) k = n - 2;   // третий подряд — к самой остановке (последняя точка маршрута — она)
  memcpy(out, r[k], 12);
  *h = atan2f(r[k + 1][0] - r[k][0], r[k + 1][2] - r[k][2]);
  if (rr->respawnN >= 3) { memcpy(out, r[n - 1], 12); rr->respawnN = 0; }
  return 1;
}

int brGoal(const BonusRace *b, int i) {
  const BrRacer *r = &b->r[i];
  int k = r->next == 0 ? b->nwp - 1 : r->next - 1;   // остановка k — чекпойнт order[k + 1], последняя — финиш
  return r->lap * b->nwp + k;
}

// ---------------------------------------------------------------- водитель трассы
#define BRD_ALAT 7.f       // как A_LAT viewer/msncar.c (правило порта)
#define BRD_G 15.f         // ускорение свободного падения car.c (CarParams.gravity по умолчанию; knowledge/facts/ps2-car-gravity)
static float wrapA(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float clampF(float x, float a, float c) { return x < a ? a : x > c ? c : x; }
static const float *pathP(const BonusRace *b, int i) { int n = b->npath; return b->path[((i % n) + n) % n].p; }

void brDriverInit(BonusRace *b, int i, const CarParams *p, const ColWorld *w) {
  int n = b->npath; float *v = b->vprof[i];
  memset(&b->drv[i], 0, sizeof b->drv[i]); b->drv[i].idx = -1;
  if (n < 5) return;
  float alat = clampF(BRD_ALAT * p->tireGrip / 2.4f, BRD_ALAT, 11.f), top = p->topSpeed;
  float dec = fminf(2.f * (p->brakeScale + 3.f), 9.f) * .7f, g = p->gravity > 0.f ? p->gravity : BRD_G;
  for (int k = 0; k < n; k++) {
    const float *a = pathP(b, k - 2), *c = pathP(b, k), *e = pathP(b, k + 2);
    // радиус окружности через три точки (xz)
    float ax = c[0] - a[0], az = c[2] - a[2], bx = e[0] - c[0], bz = e[2] - c[2], cx = e[0] - a[0], cz = e[2] - a[2];
    float la = sqrtf(ax * ax + az * az), lb = sqrtf(bx * bx + bz * bz), lc = sqrtf(cx * cx + cz * cz), cr = fabsf(ax * bz - az * bx);
    float R = cr > 1e-4f ? la * lb * lc / (2.f * cr) : 1e4f, vk = sqrtf(alat * fmaxf(R, 2.f));
    // гребень: кривизна профиля высоты по пути (выпуклость вверх) — не быстрее sqrt(g / κ)
    const float *pa = pathP(b, k - 1), *pe = pathP(b, k + 1);
    float s1 = sqrtf((c[0] - pa[0]) * (c[0] - pa[0]) + (c[2] - pa[2]) * (c[2] - pa[2])) + 1e-3f;
    float s2 = sqrtf((pe[0] - c[0]) * (pe[0] - c[0]) + (pe[2] - c[2]) * (pe[2] - c[2])) + 1e-3f;
    float kap = ((c[1] - pa[1]) / s1 - (pe[1] - c[1]) / s2) / (.5f * (s1 + s2));
    int jz = 0;
    if (w) for (uint32_t z = 0; z < w->nzone; z++) if (colZoneContains(&w->zone[z], c[0], c[1] + .5f, c[2], 3.f)) jz = 1;
    b->jump[k] = (unsigned char)jz;
    if (!jz && kap > .01f) vk = fminf(vk, sqrtf(g / kap));
    v[k] = jz ? top : fminf(top, vk);
  }
  for (int pass = 0; pass < 2; pass++)   // торможение заранее (по кругу, два прохода)
    for (int k = n - 1; k >= 0; k--) {
      const float *c = pathP(b, k), *d = pathP(b, k + 1);
      float ds = sqrtf((d[0] - c[0]) * (d[0] - c[0]) + (d[2] - c[2]) * (d[2] - c[2]));
      float lim = sqrtf(v[(k + 1) % n] * v[(k + 1) % n] + 2.f * dec * ds); if (v[k] > lim) v[k] = lim;
    }
}

static int nearIdx(const BonusRace *b, int from, const float p[3], int win) {
  int n = b->npath, best = -1; float bd = 1e30f;
  int a = from < 0 ? 0 : from - 3, z = from < 0 ? n : from + win;
  for (int k = a; k < z; k++) {
    const float *q = pathP(b, k); float dy = q[1] - p[1];
    float d = (q[0] - p[0]) * (q[0] - p[0]) + (q[2] - p[2]) * (q[2] - p[2]) + 4.f * dy * dy;
    if (d < bd) { bd = d; best = ((k % n) + n) % n; }
  }
  if (from >= 0 && bd > 15.f * 15.f) return nearIdx(b, -1, p, 0);   // сбилась (возврат на путь, прыжок) — по всему кругу
  return best;
}

void brDrive(BonusRace *b, int i, const Car *k, float dt, CarInput *in) {
  memset(in, 0, sizeof *in);
  if (b->npath < 5) return;
  float pos[3] = {k->x, k->y, k->z};
  int idx = b->drv[i].idx = nearIdx(b, b->drv[i].idx, pos, 12);
  // точка взгляда: от ближней точки вперёд по пути на L
  float L = 4.f + .35f * fabsf(k->v), aim[3]; int j = idx;
  // догон: первая точка пути от ближней вперёд, которая дальше L от машины (точка рядом с машиной или сбоку — не цель)
  for (j = idx + 1; j < idx + b->npath / 2; j++) { const float *c = pathP(b, j);
    if ((c[0] - k->x) * (c[0] - k->x) + (c[2] - k->z) * (c[2] - k->z) >= L * L) break; }
  memcpy(aim, pathP(b, j), 12);
  float err = wrapA(atan2f(aim[0] - k->x, aim[2] - k->z) - k->h), v = k->v;
  float vt = b->vprof[i][(idx + 1) % b->npath];
  if (fabsf(err) > 1.2f) vt = fminf(vt, 6.f);
  b->drv[i].vt = vt;
  in->steer = k->wheels > 0 ? clampF(-err * 2.2f, -1.f, 1.f) : 0.f;   // в воздухе руль прямо
  if (b->drv[i].backT > 0.f) {
    b->drv[i].backT -= dt; in->steer = b->drv[i].backSteer; in->brake = 1.f; return;
  }
  float top = fmaxf(1.f, k->p.topSpeed), u = vt / top;
  if (b->jump[(idx + 1) % b->npath]) { in->throttle = 1.f; }   // трамплин: полный газ (зона прыжка данных)
  else if (v < vt - .3f) in->throttle = clampF(u * u + (vt - v) * .5f, .15f, 1.f);
  else if (v > vt + 1.5f) in->brake = clampF((v - vt) * .3f, .2f, 1.f);
  else in->throttle = clampF(u * u, 0.f, 1.f);
  if (fabsf(err) > 2.2f && fabsf(v) < 3.f) { b->drv[i].backT = 1.2f; b->drv[i].backSteer = err > 0.f ? 1.f : -1.f; return; }
  if (in->throttle > .3f && fabsf(v) < 1.f && k->wheels > 0) {
    if ((b->drv[i].stuckT += dt) > 1.5f) { b->drv[i].stuckT = 0.f; b->drv[i].backT = 1.2f; b->drv[i].backSteer = err > 0.f ? 1.f : -1.f; }
  } else b->drv[i].stuckT = 0.f;
}

unsigned brCollide(Car *const *cars, int n, const ColWorld *w) {
  unsigned hit = 0;
  for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
    Car *a = cars[i], *c = cars[j];
    float ra = a->p.halfL + fabsf(a->p.bodyOfs) + .5f, rc = c->p.halfL + fabsf(c->p.bodyOfs) + .5f, r = ra + rc + .3f;
    float dx = a->x - c->x, dz = a->z - c->z;
    if (dx * dx + dz * dz >= r * r || fabsf(a->y - c->y) >= 3.f) continue;
    if (carHitCar(a, c, w)) hit |= (1u << i) | (1u << j);
  }
  return hit;
}

// ---------------------------------------------------------------- подборы, ловушки, свет, DEATH (bonus.h, у BnsPick)
static int volIn(uint32_t rect, const float c[3], const float ax[3][3], const float h[3], const float p[3], float grow) {
  float d[3] = {p[0] - c[0], p[1] - c[1], p[2] - c[2]};
  if (!rect) return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= (h[0] + grow) * (h[0] + grow);
  for (int k = 0; k < 3; k++) if (fabsf(d[0] * ax[k][0] + d[1] * ax[k][1] + d[2] * ax[k][2]) > h[k] + grow) return 0;
  return 1;
}

int brPick(BonusRace *b, int i, Car *k) {
  if (BONUS_PICKUPS_OLD || i < 0 || i >= BR_MAX) return -1;
  float p[3] = {k->x, k->y + .5f, k->z};
  for (unsigned q = 0; q < b->t.npick && q < BNS_MAXPICK; q++) {
    const BnsPick *P = &b->t.pk[q];
    if (b->pickTaken >> q & 1u) continue;
    float dx = p[0] - P->p[0], dy = p[1] - P->p[1], dz = p[2] - P->p[2], r = P->r + b->grow[i];
    if (dx * dx + dy * dy + dz * dz > r * r) continue;
    b->pickTaken |= 1u << q; b->pickAway[q] = BR_RESPAWN_KIND_OLD ? BR_PICK_RESPAWN_S : (P->kind == BNS_PK_NITRO ? BR_RESPAWN_NITRO_S : BR_RESPAWN_WRENCH_S);   // сверка g07b P17-18: нитро 15 с, ключ 30 с
    if (P->kind == BNS_PK_WRENCH) { k->damage = 0.f; k->destroyed = 0; k->deadT = 0.f; b->nWrench++; }   // вся машина, как ключ уровня
    else if (BR_NITRO3_OLD) { b->boostT[i] = BR_NITRO_S; b->nNitro++; }
    else { b->nitro[i]++; b->nNitro++; }   // сверка g07b P17-14: подбор «нитро» — один заряд
    return (int)q;
  }
  return -1;
}

void brPickTick(BonusRace *b, float dt) {
  for (unsigned q = 0; q < b->t.npick && q < BNS_MAXPICK; q++)
    if ((b->pickTaken >> q & 1u) && (b->pickAway[q] -= dt) <= 0.f) { b->pickTaken &= ~(1u << q); b->pickAway[q] = 0.f; }
}

int brPickShown(const BonusRace *b, int q) { return q >= 0 && q < BNS_MAXPICK && !(b->pickTaken >> q & 1u); }

int brBoost(BonusRace *b, int i, Car *k, float thr, float dt) {
  if (BONUS_PICKUPS_OLD || i < 0 || i >= BR_MAX || b->boostT[i] <= 0.f) return 0;
  b->boostT[i] -= dt;
  if (k->wheels > 0 && thr > 0.f) {   // как разгон зоны прыжка car.c jumpZoneDrive: вдоль курса, колёса на земле, × газ
    float a = BR_NITRO_ACC * (thr > 1.f ? 1.f : thr) * dt;
    k->vel[0] += sinf(k->h) * a; k->vel[2] += cosf(k->h) * a;
  }
  return 1;
}

// сверка g07b-msnview (P17-14, P04 B1, P17-20): гудок = нитро. Человек жмёт ↑ крестовины (bonus_play.h), ИИ — сам
int brHorn(BonusRace *b, int i, Car *k) {
  if (BR_NITRO3_OLD || i < 0 || i >= BR_MAX || b->state != BR_RACE || b->r[i].finished || b->nitro[i] <= 0 || b->nitroGap[i] > 0.f) return 0;
  b->nitro[i]--; b->nitroGap[i] = BR_HORN_GAP_S; b->nHorn++;
  k->vel[0] += sinf(k->h) * BR_HORN_KICK; k->vel[2] += cosf(k->h) * BR_HORN_KICK;   // +10 м/с вдоль курса
  return 1;
}
static float brRnd(BonusRace *b) { b->rng = b->rng * 1664525u + 1013904223u; return (float)((b->rng >> 8) & 0xffff) / 65536.f; }
static int pathDirAt(const BonusRace *b, const float p[3], float *h);
int brAiHorn(BonusRace *b, int i, Car *k, float dt) {
  if (BR_NITRO3_OLD || i < 0 || i >= BR_MAX || i == b->human) return 0;
  if (b->aiHornT[i] > 0.f) { b->aiHornT[i] -= dt; return 0; }
  if (!BR_AIHORN3_OLD) { b->aiChkT[i] += dt; if (b->aiChkT[i] < BR_AI_HORN_CHECK_S) return 0; b->aiChkT[i] = 0.f; }   // круг 3: решение раз в 2 с
  float h; if (b->state != BR_RACE || b->nitro[i] <= 0 || !pathDirAt(b, (float[3]){k->x, k->y, k->z}, &h)) return 0;
  float d = atan2f(sinf(h - k->h), cosf(h - k->h));   // путь впереди в пределах 10° от курса
  if (fabsf(d) > 10.f * 3.14159265f / 180.f) return 0;
  if (!brHorn(b, i, k)) return 0;
  b->aiHornT[i] = BR_AI_HORN_GAP_S; return 1;
}
int brGoNitro(BonusRace *b, Car *const *cars) {   // «GO!»: до (ИИ − 1) машин, каждая с шансом 3/7; никто не взял и ИИ больше одного — одна случайная
  if (BR_NITRO3_OLD) return 0;
  int ai[BR_MAX], na = 0; for (int i = 0; i < b->n; i++) if (i != b->human) ai[na++] = i;
  if (na == 0) return 0;
  int used = 0, cap = na > 1 ? na - 1 : 1;
  for (int a = 0; a < na && used < cap; a++) if (brRnd(b) < 3.f / 7.f && brHorn(b, ai[a], cars[ai[a]])) { used++; b->aiHornT[ai[a]] = BR_AI_HORN_GAP_S; }
  if (!used && na > 1) { int a = (int)(brRnd(b) * na); if (a >= na) a = na - 1; if (brHorn(b, ai[a], cars[ai[a]])) { used++; b->aiHornT[ai[a]] = BR_AI_HORN_GAP_S; } }
  return used;
}

static int pathDirAt(const BonusRace *b, const float p[3], float *h) {   // курс плотного пути хода у ближней к p точки
  if (b->npath < 2) return 0;
  int best = 0; float bd = 1e30f;
  for (int i = 0; i < b->npath; i++) {
    const float *q = b->path[i].p; float dy = q[1] - p[1], d = d2xz(q, p) + 4.f * dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  const float *a = b->path[best].p, *c = b->path[(best + 1) % b->npath].p;
  *h = atan2f(c[0] - a[0], c[2] - a[2]);
  return 1;
}

int brDeath(BonusRace *b, int i, Car *k, const ColWorld *w) {
  if (BONUS_PICKUPS_OLD || !w || !w->ndeath) return 0;
  int z = colDeathAtEx(w, k->x, k->y + .5f, k->z, .5f, 0);   // как gameDeathAt уровней (правка 349: все объёмы локатора)
  if (z < 0) return 0;
  const float *A = w->death[z].apex; float h = k->h;
  if (colDeathHeading(w, (uint32_t)z, &h)) {
    // курс локатора; смотрит против хода гонки у точки возврата (b06 death/death1 по ходу, все — в обратный ход) — развернуть на π
    float t;
    if (pathDirAt(b, A, &t)) { if (cosf(h - t) < 0.f) h += 3.14159265f; }
    else if (b->reverse) h += 3.14159265f;
    h = atan2f(sinf(h), cosf(h));
  }
  BrRacer *r = i >= 0 && i < b->n ? &b->r[i] : 0;
  if (r && i != b->human) {
    if (r->deathCp != r->cps) { r->deathCp = r->cps; r->deathN = 0; }
    float q[3], qh, p[3] = {k->x, k->y, k->z};
    if (++r->deathN >= BR_DEATH_AI_N && brRespawn(b, i, p, q, &qh)) { carPlace(k, w, q[0], q[1] + .5f, q[2], qh); r->deathN = 0; }
    else carPlace(k, w, A[0], A[1], A[2], h);
    r->stuckT = 0.f; r->bestProg = -1e9f;   // счёт «застрял» — заново от точки возврата
  } else carPlace(k, w, A[0], A[1], A[2], h);
  if (i >= 0 && i < BR_MAX) b->boostT[i] = 0.f;
  b->nDeath++;
  return 1;
}

unsigned brTrapStep(BonusRace *b, const float (*pos)[3], int n, float dt) {
  if (BONUS_PICKUPS_OLD || !b->ntrap) return 0;
  int any = 0;
  if (!BR_TRAP_LEADER_OLD) {   // сверка g07b-msnview (P17-17): ловушку переключает только лидер — вход в первый датчик по ходу включает клип вперёд, вход во второй пускает назад; без выдержки; при обратном ходе датчики меняются местами
    int sens[BNS_MAXHAZ], ns = 0; for (unsigned q = 0; q < b->t.nhaz && ns < BNS_MAXHAZ; q++) if (b->t.hz[q].kind == BNS_HZ_TRAP) sens[ns++] = (int)q;
    if (ns >= 2) {
      int lead = -1; for (int j = 0; j < n && j < BR_MAX; j++) if (b->r[j].place == 1) { lead = j; break; }
      unsigned fired2 = 0;
      for (int s2 = 0; s2 < 2; s2++) {
        const BnsHaz *H = &b->t.hz[sens[s2]];
        int in = lead >= 0 && volIn(H->rect, H->c, H->ax, H->h, pos[lead], b->grow[lead]);
        int role = b->reverse ? 1 - s2 : s2;   // 0 — вперёд (выход), 1 — назад
        if (in && !b->trapIn[s2]) for (int t = 0; t < b->ntrap; t++) {
          BrTrap *T = &b->trap[t];
          if (role == 0 && (T->state == BR_TRAP_IDLE || T->state == BR_TRAP_LOWER)) { T->state = BR_TRAP_RISE; b->launched = 0; b->nTrap++; fired2 |= 1u << t; }
          else if (role == 1 && (T->state == BR_TRAP_RISE || T->state == BR_TRAP_HOLD)) T->state = BR_TRAP_LOWER;
        }
        b->trapIn[s2] = in;
      }
      for (int t = 0; t < b->ntrap; t++) {
        BrTrap *T = &b->trap[t];
        switch (T->state) {
          case BR_TRAP_RISE: T->f += dt * T->fps; if (T->f >= T->last) { T->f = T->last; T->state = BR_TRAP_HOLD; T->hold = 1e9f; } break;
          case BR_TRAP_LOWER: T->f -= dt * T->fps; if (T->f <= 0.f) { T->f = 0.f; T->state = BR_TRAP_IDLE; } break;
          default: break;
        }
      }
      return fired2;
    }
  }
  for (int j = 0; j < n && j < BR_MAX && !any; j++)
    for (unsigned q = 0; q < b->t.nhaz; q++) {
      const BnsHaz *H = &b->t.hz[q];
      if (H->kind == BNS_HZ_TRAP && volIn(H->rect, H->c, H->ax, H->h, pos[j], b->grow[j])) { any = 1; break; }
    }
  unsigned fired = 0;
  for (int t = 0; t < b->ntrap; t++) {
    BrTrap *T = &b->trap[t];
    if (any && T->state == BR_TRAP_IDLE) { T->state = BR_TRAP_RISE; T->f = 0.f; b->launched = 0; b->nTrap++; fired |= 1u << t; }
    switch (T->state) {
      case BR_TRAP_RISE: T->f += dt * T->fps; if (T->f >= T->last) { T->f = T->last; T->state = BR_TRAP_HOLD; T->hold = BR_TRAP_HOLD_S; } break;
      case BR_TRAP_HOLD: if ((T->hold -= dt) <= 0.f) T->state = BR_TRAP_LOWER; break;
      case BR_TRAP_LOWER: T->f -= dt * T->fps; if (T->f <= 0.f) { T->f = 0.f; T->state = BR_TRAP_IDLE; } break;
      default: break;
    }
  }
  return fired;
}

int brLaunch(BonusRace *b, int i, Car *k) {
  if (BONUS_PICKUPS_OLD || !BR_LAUNCH_OLD || i < 0 || i >= BR_MAX || (b->launched >> i & 1u)) return 0;   // сверка g07b P17 Д3: событие 63 в оригинале без обработчика — поршни не подбрасывают (BR_LAUNCH_OLD=1 — прежнее)
  int rising = 0; for (int t = 0; t < b->ntrap; t++) rising |= b->trap[t].state == BR_TRAP_RISE;
  if (!rising) return 0;
  float p[3] = {k->x, k->y + .5f, k->z};
  for (unsigned q = 0; q < b->t.nhaz; q++) {
    const BnsHaz *H = &b->t.hz[q];
    if (H->kind != BNS_HZ_LAUNCH || !volIn(H->rect, H->c, H->ax, H->h, p, b->grow[i])) continue;
    if (k->vel[1] < (float)H->arg) k->vel[1] = (float)H->arg;
    b->launched |= 1u << i; b->nLaunch++;
    return 1;
  }
  return 0;
}

uint32_t brLight(const BonusRace *b, const float p[3]) {
  if (BONUS_PICKUPS_OLD) return 0;
  for (unsigned q = 0; q < b->t.nhaz; q++) {
    const BnsHaz *H = &b->t.hz[q];
    if (H->kind == BNS_HZ_LIGHT && volIn(H->rect, H->c, H->ax, H->h, p, 0.f)) return H->arg;
  }
  return 0;
}

uint32_t brLightMul(uint32_t argb) {
  float a = (float)(argb >> 24) / 255.f; uint32_t o = 0xff000000u;
  for (int k = 0; k < 3; k++) {   // ARGB данных: R — бит 16, G — 8, B — 0; на выходе ABGR (GE): R — бит 0
    float c = (float)((argb >> (16 - 8 * k)) & 0xff) / 255.f, m = 1.f - a * (1.f - c);
    o |= (uint32_t)(m * 255.f + .5f) << (8 * k);
  }
  return o;
}

int brTrapBind(BonusRace *b, const AnimObjs *a) {
  b->ntrap = 0;
  if (BONUS_PICKUPS_OLD || !a || !a->obj || !b->t.h || b->t.h->ver < 2 || !b->t.h->ntrapobj) return 0;
  for (unsigned k = 0; k < a->nobj && b->ntrap < BR_MAXTRAP; k++) {
    const AoObj *o = &a->obj[k];
    if (!o->nVol || o->action != AO_NONE || o->nfr < 2) continue;
    BrTrap *T = &b->trap[b->ntrap++];
    memset(T, 0, sizeof *T); T->obj = (int)k; T->last = (float)(o->nfr - 1); T->fps = o->fps > 0.f ? o->fps : 30.f;
  }
  return b->ntrap;
}

void brTrapApply(const BonusRace *b, AnimObjs *a, StateProps *sp, ColWorld *w) {
  for (int t = 0; t < b->ntrap; t++) {
    const AoObj *o = &a->obj[b->trap[t].obj]; float f = b->trap[t].f;
    for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst && i < sp->ninst; i++) {
      SpRun *s = &sp->run[i]; if (s->poseF != f) { s->poseF = f; s->poseValid = 0; }
    }
    aoPlace(a, sp, w, (unsigned)b->trap[t].obj);
  }
}
