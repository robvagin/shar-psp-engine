// Боты по всем уровням и всем известным рискам (правка Роба 415, 09.10: «гонять все уровни, проходить логично, покрыть все
// нюансы — перевороты, заборы, камеры, ИИ, летающие машины»). Игра без экрана tools/bots/world.c — тот же C-код логики, что
// EBOOT (car.c, col.c, traffic.c, game.c, миссии), плюс viewer/carzone.c (камеры зон и прыжков машины) и viewer/camsafe.c
// (камера в геометрии). Боты только МЕРЯЮТ: код игры и данные уровня не меняются, карта не правится. Процессор Мака, без AI и сети.
//
//   allrisk LEVEL PLAYER ROADS MSN --mode M [--seed S] [--secs T] [--only ID] [--cars a,b,...] [--focus X Z ...]
// Режимы (каждый случай начинается со свежего мира bwNew — повтор одного случая: тот же бинарник, те же данные, --only ID):
//   progress  логичное прохождение уровня ОДНИМ миром без сброса эпизода: спавн → точки начала миссий по порядку пакета (сюжет,
//             потом бонусные) → ближайший трамплин (зона JMP1, разгон по линии) → ближайшая срезка (дорога shortcut=1) → …;
//             водитель навигатора по графу дорог; застрял — сдать назад, каждая 4-я попытка RESET CAR (как игрок). Здесь же
//             длинный мир (возврат предметов, трафик, шкала Hit & Run живут весь прогон) и бюджет кадра (заменитель на Маке)
//   roll      все машины диска (carparams.h, 89): на прямых дорогах уровня разгон до 80 км/ч, потом полный руль с газом, ручник с
//             рулём, торможение по прямой. Перевороты, две колеса, крен наружу/внутрь, клевок носом при торможении
//   jumps     каждая зона прыжка уровня (JMP1, Event 48 диска; дом jumpzone17 на L1/L4/L7): 60/90/120 км/ч × сдвиг −1,5/0/+1,5 м.
//             Отрыв, полёт, нос в полёте, посадка на колёса, переворот, невидимая стена в воздухе, застрял после посадки,
//             камера зоны машины (carzone-l<N>.shr) включилась ли
//   hedges    изгороди (заборы-изгороди COL1 и объёмы FOLIAGE): с обеих сторон на 40 и 70 км/ч — застрял, подбросило, раскачка
//             (разгон без газа), переворот
//   traffic   тараны машин трафика: что делает сбитая машина 10 с (возврат на полосу, едет задом, едет на боку, висит в воздухе)
//   phantom   радиальные заезды у узлов дорог и у мест из замечаний Роба (--focus X Z): удар о стену, у которой на картинке
//             ничего нет (невидимая стена, «фантом»)
// Наблюдатель на каждом кадре всех режимов: переворот, езда на двух колёсах, провал под землю, удар о невидимую стену (на земле и
// в воздухе), застревание, автоматический возврат машины, удары машина-машина, трафик в воздухе/под землёй/на боку, камера за
// машиной (заменитель: идеальная точка FollowCameraData без сглаживания — глаз за стеной/под землёй/зажат), время кадра.
// Вывод (stdout, TSV): CASE семейство вердикт id уровень место x y z объект кадр метрики; STAT режим метрики. Вердикт: ok,
// risk (найден известный риск), info (наблюдение без нормы), fail (проверка не смогла пройти — нет данных).
// Сборка: tools/bots/allrisk.py собирает сам (cc … tools/bots/allrisk.c $BOTS_LIB viewer/camsafe.c viewer/carzone.c).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include "world.h"
#include "navgraph.h"
#include "traffic.h"
#include "msncar.h"
#include "camsafe.h"
#include "carzone.h"
#include "carcam.h"
#include "carparams.h"
#include "placecode.h"
#include "mission/msn.h"

#define PI 3.14159265f
static BotWorld W; static const ColWorld *CW; static int LV = 1; static const char *MODE = "?"; static unsigned SEED = 1;
static const char *ONLY; static char CASEID[96]; static int caseOn = 1;   // --only: выполнить только случай с этим id
static unsigned char *lvlFile; static long lvlSize;
static int hoverCar;   // машина без колёс по данным (сценарии диска: hover, SetWheelsHidden) — hbike_v, frink_v

// ---------------------------------------------------------------- рисуемая геометрия (как tests/airwalls/host.c, tools/cartest/walls.c)
typedef struct { float bb[6]; int32_t tex; uint32_t flags, nv, ni, owner; float ox, oy, oz, sc; uint32_t off, nf, cell, pad[2]; } Rec;
typedef struct { float bb[6]; uint32_t first, n, owner, off, size, pad; } CellRec;
static float *T; static unsigned NT; static unsigned *gS, *gL; static int GW, GH; static float GOX, GOZ; static const float GC = 2.f;
static void loadRender(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return;
  fseek(f, 0, SEEK_END); lvlSize = ftell(f); rewind(f); lvlFile = malloc((size_t)lvlSize);
  if (fread(lvlFile, 1, (size_t)lvlSize, f) != (size_t)lvlSize) { fclose(f); free(lvlFile); lvlFile = 0; return; }
  fclose(f);
  uint32_t npc, tableOff, cellOff; memcpy(&npc, lvlFile + 8, 4); memcpy(&tableOff, lvlFile + 76, 4); memcpy(&cellOff, lvlFile + 84, 4);
  const Rec *recs = (const Rec *)(lvlFile + tableOff); const CellRec *cells = (const CellRec *)(lvlFile + cellOff);
  unsigned cap = 1 << 20; T = malloc(sizeof(float) * 9 * cap);
  for (unsigned i = 0; i < npc; i++) {
    const Rec *r = &recs[i]; if (r->owner != 0 || (r->flags & 4) || !(r->flags & 16)) continue;   // город, не небо, с индексами
    int vs = (r->flags & 32) ? 12 : 16; const uint8_t *b = lvlFile + cells[r->cell].off + r->off;
    for (unsigned k = 0; k + 2 < r->ni; k += 3) {
      if (NT == cap) { cap *= 2; T = realloc(T, sizeof(float) * 9 * cap); }
      for (int j = 0; j < 3; j++) {
        const int16_t *q = (const int16_t *)(b + (size_t)(k + j) * vs + (vs == 12 ? 6 : 8));
        T[9 * NT + 3 * j] = r->ox + q[0] * (r->sc / 32768.f); T[9 * NT + 3 * j + 1] = r->oy + q[1] * (r->sc / 32768.f);
        T[9 * NT + 3 * j + 2] = r->oz + q[2] * (r->sc / 32768.f);
      }
      NT++;
    }
  }
  GOX = CW->ox; GOZ = CW->oz; GW = (int)(CW->W * CW->cell / GC) + 1; GH = (int)(CW->H * CW->cell / GC) + 1;
  gS = calloc((size_t)GW * GH + 1, 4);
  for (int pass = 0; pass < 2; pass++) {
    for (unsigned t = 0; t < NT; t++) {
      const float *a = T + 9 * t;
      int cx0 = (int)floorf((fminf(a[0], fminf(a[3], a[6])) - GOX) / GC), cx1 = (int)floorf((fmaxf(a[0], fmaxf(a[3], a[6])) - GOX) / GC);
      int cz0 = (int)floorf((fminf(a[2], fminf(a[5], a[8])) - GOZ) / GC), cz1 = (int)floorf((fmaxf(a[2], fmaxf(a[5], a[8])) - GOZ) / GC);
      if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= GW) cx1 = GW - 1; if (cz1 >= GH) cz1 = GH - 1;
      for (int z = cz0; z <= cz1; z++) for (int x = cx0; x <= cx1; x++) { size_t c = (size_t)z * GW + x; if (pass == 0) gS[c + 1]++; else gL[gS[c]++] = t; }
    }
    if (pass == 0) { for (size_t c = 0; c < (size_t)GW * GH; c++) gS[c + 1] += gS[c]; gL = malloc(sizeof(unsigned) * (gS[(size_t)GW * GH] + 1)); }
    else { for (size_t c = (size_t)GW * GH; c > 0; c--) gS[c] = gS[c - 1]; gS[0] = 0; }
  }
}
static float rayTri(const float *o, const float *d, const float *a) {
  float e1[3] = {a[3] - a[0], a[4] - a[1], a[5] - a[2]}, e2[3] = {a[6] - a[0], a[7] - a[1], a[8] - a[2]};
  float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
  float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2]; if (fabsf(det) < 1e-9f) return -1;
  float inv = 1.f / det, s[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
  float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv; if (u < 0 || u > 1) return -1;
  float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
  float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0 || u + v > 1) return -1;
  return (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
}
static float rayCast(const float *o, const float *d, float tmax) {
  if (!NT) return 0.f;   // нет рисуемой геометрии — считаем, что препятствие видно (невидимых стен не выдумывать)
  float ex = o[0] + d[0] * tmax, ez = o[2] + d[2] * tmax, best = -1;
  int cx0 = (int)floorf((fminf(o[0], ex) - GOX) / GC), cx1 = (int)floorf((fmaxf(o[0], ex) - GOX) / GC);
  int cz0 = (int)floorf((fminf(o[2], ez) - GOZ) / GC), cz1 = (int)floorf((fmaxf(o[2], ez) - GOZ) / GC);
  if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= GW) cx1 = GW - 1; if (cz1 >= GH) cz1 = GH - 1;
  for (int z = cz0; z <= cz1; z++) for (int x = cx0; x <= cx1; x++) {
    size_t c = (size_t)z * GW + x;
    for (unsigned k = gS[c]; k < gS[c + 1]; k++) { float t = rayTri(o, d, T + 9 * gL[k]); if (t >= 0 && t <= tmax && (best < 0 || t < best)) best = t; }
  }
  return best;
}
// есть ли на картинке препятствие за точкой удара в слое кузова (лучи 0.4..1.4 м над полом, три со сдвигом вбок 0.5 м)
static int visAt(const float *P, float nx, float nz) {
  static const float H[4] = {.4f, .7f, 1.f, 1.4f};
  for (int s = -1; s <= 1; s++) for (int k = 0; k < 4; k++) {
    float o[3] = {P[0] + nx * .3f + nz * .5f * s, P[1] + H[k], P[2] + nz * .3f - nx * .5f * s}, dd[3] = {-nx, 0, -nz};
    if (rayCast(o, dd, 1.3f) >= 0) return 1;
  }
  return 0;
}

// ---------------------------------------------------------------- вывод
static unsigned long long frame;   // кадров этого случая (progress — всего прогона)
static void cs(const char *fam, const char *verdict, const char *id, float x, float y, float z, const char *fmt, ...) {
  char pc[16], obj[16] = "-", m[640]; int nl = 0, layer = -1;
  placeCode(x, z, pc, sizeof pc);
  if (CW && isfinite(x) && isfinite(z)) layer = bwWhere(x, y, z, 2.f, &nl, obj);
  va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
  printf("CASE\t%s\t%s\t%s\tL%d\t%s\t%.2f\t%.2f\t%.2f\t%d:%s\t%llu\t%s\n", fam, verdict, id, LV, pc, x, y, z, layer, obj, frame, m);
  fflush(stdout);
}

// ---------------------------------------------------------------- геометрия машины
static void qUp(const float q[4], float u[3]) { float w = q[0], x = q[1], y = q[2], z = q[3]; u[0] = 2 * (x * y - w * z); u[1] = 1 - 2 * (x * x + z * z); u[2] = 2 * (y * z + w * x); }
static void qFwd(const float q[4], float f[3]) { float w = q[0], x = q[1], y = q[2], z = q[3]; f[0] = 2 * (x * z + w * y); f[1] = 2 * (y * z - w * x); f[2] = 1 - 2 * (x * x + y * y); }
static float groundBelow(float x, float y, float z, float above, float below) { float g, n[3]; return colGround(CW, x, z, y + above, y - below, &g, n) ? g : NAN; }
static float lowestGround(float x, float z) { float top = 1e4f, g, n[3], lo = NAN; for (int k = 0; k < 16 && colGround(CW, x, z, top, -1e4f, &g, n); k++) { lo = g; top = g - 1.f; } return lo; }

// ---------------------------------------------------------------- камеры: зоны машины (carzone-l<N>.shr) и заменитель камеры за машиной
static RoomCam czcam[CARZONE_CAMS]; static int nczcam; static CarZoneState czs; static unsigned char *czBuf;
static void loadCarzone(const char *level) {
  char p[600]; snprintf(p, sizeof p, "%s", level); char *sl = strrchr(p, '/'); snprintf(sl ? sl + 1 : p, sizeof p - (size_t)(sl ? sl + 1 - p : 0), "carzone-l%d.shr", LV);
  FILE *f = fopen(p, "rb"); if (!f) return; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  czBuf = n > 128 && n < (1 << 20) ? malloc((size_t)n) : 0;
  if (czBuf && fread(czBuf, 1, (size_t)n, f) == (size_t)n && !memcmp(czBuf, "ZCM1", 4)) nczcam = roomCamsParse(czcam, CARZONE_CAMS, czBuf, (unsigned)n);
  fclose(f); if (nczcam < 0) nczcam = 0;
  memset(&czs, 0, sizeof czs); czs.cur = -1;
}
static int czStep(void) {   // как main.c (№408): кузов — центр и 8 углов; -> камера зоны или -1
  if (!nczcam || gameOnFoot(&W.G)) return -1;
  const Car *c = &W.car; CarBox cb; memset(&cb, 0, sizeof cb); carBox(c, &cb);
  float lo = c->p.bodyLo + c->modelRaise, hi = c->p.bodyHi + c->modelRaise, bd[9][3], e3[3], l3[3], fy;
  bd[0][0] = cb.x; bd[0][1] = cb.y + .5f * (lo + hi); bd[0][2] = cb.z; int nb = 1;
  for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) for (int u = 0; u < 2; u++) {
    float ox = cb.fx * cb.hl * a + cb.fz * cb.hw * b, oz = cb.fz * cb.hl * a - cb.fx * cb.hw * b;
    bd[nb][0] = cb.x + ox; bd[nb][1] = cb.y + cb.gx * ox + cb.gz * oz + (u ? hi : lo); bd[nb][2] = cb.z + oz; nb++; }
  float ref[3] = {c->x, c->y + carModelLift(c, W.lift > 0.f ? W.lift : 0.f), c->z}, sp = hypotf(c->vel[0], c->vel[2]);
  return carZoneStep(czcam, nczcam, &czs, (const float (*)[3])bd, nb, ref, sp, BW_DT, e3, l3, &fy);
}
static const CarCamData *camOf(const char *name) { for (size_t i = 0; i < sizeof carCamTab / sizeof carCamTab[0]; i++) if (!strcasecmp(carCamTab[i].name, name)) return &carCamTab[i]; return 0; }
static CsScene csS;
// -> флаги: 1 глаз за стеной от опоры, 2 глаз под землёй, 4 зажата (свободно меньше 2 м по штанге), 8 глаз в кузове трафика
static int followProbe(void) {
  const Car *c = &W.car; const CarCamData *cd = camOf(c->p.name); if (!cd) cd = camOf("famil_v"); if (!cd) return 0;
  float fx = sinf(c->h), fz = cosf(c->h), rx = cosf(c->h), rz = -sinf(c->h);
  float t[3] = {c->x + rx * cd->tx + fx * cd->tz, c->y + cd->ty, c->z + rz * cd->tx + fz * cd->tz};
  float e[3] = {t[0] - fx * cd->farMag * sinf(cd->farElev), t[1] + cd->farMag * cosf(cd->farElev), t[2] - fz * cd->farMag * sinf(cd->farElev)};
  csS.w = CW; csS.nbox = 0; csS.ncyl = 0;
  const Traffic *tr = bwTraffic();
  if (tr) for (int i = 0; i < tr->maxCars && csS.nbox < CS_MAXBOX; i++) if (tr->car[i].mode != TRM_OFF) {
    const Car *k = &tr->car[i].car; if (hypotf(k->x - e[0], k->z - e[2]) > 14.f) continue;
    CarBox b; memset(&b, 0, sizeof b); carBox(k, &b); CsBox *o = &csS.box[csS.nbox++];
    o->x = b.x; o->z = b.z; o->fx = b.fx; o->fz = b.fz; o->hw = b.hw; o->hl = b.hl; o->y = b.y; o->gx = b.gx; o->gz = b.gz; o->lo = b.lo; o->hi = b.hi; }
  float f[3] = {t[0] - e[0], t[1] - e[1], t[2] - e[2]}, l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (l < 1e-3f) return 0;
  f[0] /= l; f[1] /= l; f[2] /= l;
  float tanV = tanf(31.f * PI / 180.f), tanH = tanV * 16.f / 9.f;
  int p = csProbe(&csS, e, f, tanV, tanH, 1.5f, t), out = 0;
  if (p & CS_IN_EYE) out |= 1; if (p & CS_IN_GROUND) out |= 2; if (p & (CS_IN_CAR)) out |= 8;
  if (csRayFrac(&csS, t, e, 1) * l < 2.f) out |= 4;
  return out;
}

// ---------------------------------------------------------------- наблюдатель
typedef struct {
  int flips, twoRunMax, twoRun, airRun, airMax, invis, invisFence, invisAir, hedge, stuckN, falls, carHits, trHover, trSunk, trSide, camEye, camGround, camSqueeze, camCar, czOn, czName;
  float rollMax, x, y, z; int rights0, recov0, rev0, resets0; int upside; float slowT; int intent;
} Watch;
static Watch wa;
static char czSeen[CARZONE_CAMS];
static double stepUs, stepUsMax; static unsigned long long stepN; static unsigned stepHist[40]; static int trMax, pedMax;
static double nowUs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static float evT[32]; static float evX[32], evZ[32];   // приглушение повторов одного события: семейство -> время и место прошлого
enum { E_FLIP, E_TWO, E_INVIS, E_INVFENCE, E_AIRWALL, E_STUCK, E_FALL, E_TRHOVER, E_TRSUNK, E_TRSIDE, E_CAM, E_RECOV, E_MCAR, E_N };
static const char *EFAM[E_N] = {"rollover", "two_wheel", "invisible_wall", "invisible_fence", "air_wall", "stuck", "fall_through", "traffic_hover", "traffic_sunk",
                                "traffic_on_side", "camera_follow", "auto_return", "ai_car_state"};
static int quiet(int e, float x, float z) {   // 1 — тот же вид события в 8 м за последние 5 с: не печатать заново
  float t = frame / 60.f; if (t - evT[e] < 5.f && hypotf(x - evX[e], z - evZ[e]) < 8.f) return 1;
  evT[e] = t; evX[e] = x; evZ[e] = z; return 0;
}
static int watchLog;   // progress/phantom: события печатаются сразу (длинный мир); в случаях — итог строкой случая
static void watchReset(void) { memset(&wa, 0, sizeof wa); wa.rights0 = W.car.rights; wa.recov0 = W.car.recoveries; wa.rev0 = W.car.reverts; wa.resets0 = (int)W.st.resets; memset(czSeen, 0, sizeof czSeen); }
static void trafficSanity(void) {
  const Traffic *tr = bwTraffic(); if (!tr) return;
  int nc, np, nk; bwTrafficCounts(&nc, &np, &nk); if (nc > trMax) trMax = nc; if (np > pedMax) pedMax = np;
  for (int i = 0; i < tr->maxCars; i++) { const TrCar *t = &tr->car[i]; if (t->mode != TRM_DRIVE) continue;
    const Car *k = &t->car; float g = groundBelow(k->x, k->y, k->z, 1.5f, 8.f), u[3]; qUp(k->q, u);
    if (isfinite(g) && k->y - g > 1.0f) { wa.trHover++; if (watchLog && !quiet(E_TRHOVER, k->x, k->z)) cs(EFAM[E_TRHOVER], "risk", CASEID, k->x, k->y, k->z, "car=%d gap=%.2f", i, k->y - g); }
    if (!isfinite(g) && isfinite(lowestGround(k->x, k->z)) && k->y < lowestGround(k->x, k->z) - .8f) { wa.trSunk++; if (watchLog && !quiet(E_TRSUNK, k->x, k->z)) cs(EFAM[E_TRSUNK], "risk", CASEID, k->x, k->y, k->z, "car=%d", i); }
    if (u[1] < .5f) { wa.trSide++; if (watchLog && !quiet(E_TRSIDE, k->x, k->z)) cs(EFAM[E_TRSIDE], "risk", CASEID, k->x, k->y, k->z, "car=%d up=%.2f (едет по полосе, кузов на боку/крыше)", i, u[1]); }
  }
  if (W.haveMsn) for (int i = 0; i < W.msn.cars.n; i++) { const McCar *m = &W.msn.cars.c[i]; if (!m->on) continue;
    float g = groundBelow(m->car.x, m->car.y, m->car.z, 1.5f, 8.f), u[3]; qUp(m->car.q, u);
    if ((isfinite(g) && m->car.y - g > 3.f && m->car.wheels == 0 && m->car.airT > 3.f) || u[1] < 0.f)
      if (watchLog && !quiet(E_MCAR, m->car.x, m->car.z)) cs(EFAM[E_MCAR], "risk", CASEID, m->car.x, m->car.y, m->car.z, "msncar=%s air=%.1f up=%.2f", m->name, m->car.airT, u[1]); }
}
typedef struct { float x, y, z, h, v, roll, pitch; int wheels, grounded; } TwTrace;   // Коллизия-96: последние 150 кадров до события «на два колеса» (ALLRISK_TWTRACE=файл)
static TwTrace twRing[150]; static int twRingN;
static void watchFrame(int gasHeld) {
  const Car *c = &W.car; if (gameOnFoot(&W.G)) return;
  { TwTrace *t = &twRing[twRingN++ % 150]; t->x = c->x; t->y = c->y; t->z = c->z; t->h = c->h; t->v = c->v * 3.6f; t->roll = c->roll * 57.2958f; t->pitch = c->pitch * 57.2958f; t->wheels = c->wheels; t->grounded = c->grounded; }
  float u[3]; qUp(c->q, u); float roll = fabsf(c->roll) * 57.2958f; if (roll > wa.rollMax) wa.rollMax = roll;
  int up = u[1] < 0.f;
  if (up && !wa.upside) { wa.flips++; if (watchLog && !quiet(E_FLIP, c->x, c->z)) cs(EFAM[E_FLIP], "risk", CASEID, c->x, c->y, c->z, "car=%s v=%.1f", c->p.name, c->v * 3.6f); }
  wa.upside = up;
  int two = c->grounded && c->wheels > 0 && c->wheels <= 2 && roll > 12.f;
  wa.twoRun = two ? wa.twoRun + 1 : 0; if (wa.twoRun > wa.twoRunMax) wa.twoRunMax = wa.twoRun;
  if (wa.twoRun == 30 && watchLog && getenv("ALLRISK_TWTRACE")) { FILE *tf = fopen(getenv("ALLRISK_TWTRACE"), "a"); if (tf) { fprintf(tf, "# событие два колеса %s у (%.1f %.1f %.1f)\n", CASEID, c->x, c->y, c->z); for (int i = 0; i < 150 && i < twRingN; i++) { const TwTrace *t = &twRing[(twRingN + i - (twRingN < 150 ? twRingN : 150)) % 150]; fprintf(tf, "%.2f %.2f %.2f h%.0f v%.0f roll%.1f pit%.1f w%d g%d\n", t->x, t->y, t->z, t->h * 57.2958f, t->v, t->roll, t->pitch, t->wheels, t->grounded); } fclose(tf); } }
  if (wa.twoRun == 30 && watchLog && !quiet(E_TWO, c->x, c->z)) cs(EFAM[E_TWO], "risk", CASEID, c->x, c->y, c->z, "car=%s roll=%.0f v=%.0f", c->p.name, roll, c->v * 3.6f);
  wa.airRun = c->wheels == 0 && !c->grounded ? wa.airRun + 1 : 0; if (wa.airRun > wa.airMax) wa.airMax = wa.airRun;
  if (c->wallV > 4.f && !visAt(c->wallP, c->wallN[0], c->wallN[1])) {
    int air = c->wheels == 0, fence = (c->wallType & 3) == COL_SEG; if (air) wa.invisAir++; else if (fence) wa.invisFence++; else wa.invis++;
    int e = air ? E_AIRWALL : fence ? E_INVFENCE : E_INVIS;
    if (watchLog && !quiet(e, c->wallP[0], c->wallP[2]))
      cs(EFAM[e], e == E_INVFENCE ? "info" : "risk", CASEID, c->wallP[0], c->wallP[1], c->wallP[2], "v=%.1f prim=%c%u n=%.2f,%.2f car=%s", c->wallV, "tbcs"[c->wallType & 3], c->wallIdx, c->wallN[0], c->wallN[1], c->p.name);
  }
  if (c->foliV > 0.f) wa.hedge++;
  if (c->carHitV > 3.f) wa.carHits++;
  { float lo = lowestGround(c->x, c->z); if (isfinite(lo) && c->y < lo - 2.f) { wa.falls++; if (watchLog && !quiet(E_FALL, c->x, c->z)) cs(EFAM[E_FALL], "risk", CASEID, c->x, c->y, c->z, "below=%.1f", lo - c->y); } }
  if (gasHeld && fabsf(c->v) < .8f) wa.slowT += BW_DT; else wa.slowT = 0;
  if (wa.slowT > 4.f && wa.slowT < 4.f + BW_DT * 1.5f) { wa.stuckN++; if (watchLog && !quiet(E_STUCK, c->x, c->z)) cs(EFAM[E_STUCK], "info", CASEID, c->x, c->y, c->z, "газ 4 с, скорость < 0,8 м/с"); }
  if (c->recoveries > wa.recov0) { if (watchLog && !quiet(E_RECOV, c->x, c->z)) cs(EFAM[E_RECOV], "info", CASEID, c->x, c->y, c->z, "recoveries=%d reverts=%d why=%d", c->recoveries, c->reverts, c->revWhy); wa.recov0 = c->recoveries; }
  int cz = czStep(); if (cz >= 0 && !czSeen[cz]) { czSeen[cz] = 1; wa.czOn++;
    if (watchLog) cs("jump_camera", "info", CASEID, c->x, c->y, c->z, "cam=%s v=%.0f", czcam[cz].name, hypotf(c->vel[0], c->vel[2]) * 3.6f); }
  if (frame % 15 == 0) { int f = followProbe();
    if (f & 1) wa.camEye++; if (f & 2) wa.camGround++; if (f & 4) wa.camSqueeze++; if (f & 8) wa.camCar++;
    if ((f & 2) && watchLog && !quiet(E_CAM, c->x, c->z)) cs(EFAM[E_CAM], "info", CASEID, c->x, c->y, c->z, "глаз (заменитель) под землёй; flags=%d", f); }
  if (frame % 30 == 0) trafficSanity();
}
static void step(unsigned b, float ax) {
  double t0 = nowUs(); bwStep(&W, b, ax, 0.f); double d = nowUs() - t0;
  stepUs += d; stepN++; if (d > stepUsMax) stepUsMax = d; { int k = 0; double v = d; while (v >= 1.5 && k < 39) { v /= 1.5; k++; } stepHist[k]++; }
  frame++; watchFrame((b & GP_R) != 0);
}
static double pctUs(double p) { unsigned long long n = 0, want = (unsigned long long)(p * stepN); for (int k = 0; k < 40; k++) { n += stepHist[k]; if (n >= want) return pow(1.5, k + 1); } return stepUsMax; }
static void statPerf(void) {
  printf("STAT\tperf\tsteps=%llu\tus_avg=%.1f\tus_p99=%.0f\tus_max=%.0f\ttraffic_max=%d\tpeds_max=%d\tnote=Мак, не PSP\n", stepN, stepN ? stepUs / stepN : 0, pctUs(.99), stepUsMax, trMax, pedMax);
}

// отпечаток мира по ЗНАЧЕНИЯМ (без указателей): bwHash хэширует блоб трафика побайтно, а в нём указатель клипа пешехода
// (SkinPlayer) — между процессами он разный (ASLR), и отпечаток расходится при одинаковом мире. Здесь — только числа состояния
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const unsigned char *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
static uint64_t arHash(int *ntr) {
  uint64_t h = 1469598103934665603ull; const Car *c = &W.car;
  h = fnv(h, &c->x, sizeof(float) * 4); h = fnv(h, c->vel, sizeof c->vel); h = fnv(h, c->q, sizeof c->q); h = fnv(h, &c->damage, sizeof c->damage);
  h = fnv(h, &W.homer.x, sizeof(float) * 3); h = fnv(h, &W.hnrHeat, sizeof(float));
  for (unsigned i = 0; W.haveProps && i < W.props.ninst; i++) h = fnv(h, &W.props.st[i].hidden, sizeof W.props.st[i].hidden);
  const Traffic *tr = bwTraffic(); int n = 0;
  if (tr) { for (int i = 0; i < tr->maxCars; i++) { const TrCar *t = &tr->car[i]; h = fnv(h, &t->mode, sizeof t->mode); if (t->mode != TRM_OFF) { h = fnv(h, &t->car.x, sizeof(float) * 3); n++; } }
    for (int i = 0; i < tr->maxPeds; i++) { const TrPed *p = &tr->ped[i]; h = fnv(h, &p->mode, sizeof p->mode); if (p->mode != TRP_OFF) h = fnv(h, &p->x, sizeof(float) * 3); } }
  if (W.haveMsn) for (int i = 0; i < W.msn.cars.n; i++) if (W.msn.cars.c[i].on) h = fnv(h, &W.msn.cars.c[i].car.x, sizeof(float) * 3);
  if (ntr) *ntr = n; return h;
}

// ---------------------------------------------------------------- старт случая
static int selectCase(const char *fmt, ...) {   // id случая; --only — только он
  va_list ap; va_start(ap, fmt); vsnprintf(CASEID, sizeof CASEID, fmt, ap); va_end(ap);
  caseOn = !ONLY || !strcmp(ONLY, CASEID); return caseOn;
}
static void fresh(void) { bwFree(&W); bwNew(&W); frame = 0; memset(evT, 0, sizeof evT); for (int i = 0; i < 32; i++) evT[i] = -100.f; czs.cur = -1; czs.in = czs.skip = 0; }
static int useCar(const char *name, float x, float y, float z, float h) {
  CarParams p; if (name && !carParamsFor(name, &p)) return 0;
  if (name) carInit(&W.car, &p, CW, x, y, z, h);
  bwPlace(&W, x, y, z, h);
  hoverCar = !strcasecmp(W.car.p.name, "hbike_v") || !strcasecmp(W.car.p.name, "frink_v");
  watchReset(); return 1;
}
// ехать по прямой курсом h, держа скорость vt (м/с); -> 1, если за secs держал курс без удара
static void holdLine(float h, float vt, int frames, int *hitWall) {
  for (int f = 0; f < frames; f++) {
    float e = h - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
    unsigned b = W.car.v < vt ? GP_R : (W.car.v > vt + 2.f ? GP_L : 0); float ax = fmaxf(-1.f, fminf(1.f, -e * 2.2f));
    step(b, ax); if (hitWall && W.car.wallV > 3.f) (*hitWall)++;
  }
}

// ---------------------------------------------------------------- дороги: прямые отрезки
typedef struct { float x, y, z, h, len; } Straight;
static Straight str[256]; static int nstr;
static void findStraights(void) {
  for (int i = 0; i < nroad && nstr < 256; i++) {
    Road *r = &roads[i]; float dx = r->bx - r->ax, dz = r->bz - r->az, L = hypotf(dx, dz); if (L < 6.f) continue;
    float h = atan2f(dx, dz), ux = dx / L, uz = dz / L, len = 0, py = r->ay; int ok = 1;
    for (float s = 2.f; s <= 90.f; s += 2.f) {   // вперёд по курсу: ровная опора без ступеней и заборов
      float x = r->ax + ux * s, z = r->az + uz * s, g; float n3[3];
      if (!colGround(CW, x, z, py + .8f, py - 1.2f, &g, n3) || fabsf(g - py) > .5f || n3[1] < .97f) { ok = s > 70.f; break; }
      if (colCrossFence(CW, x - ux * 2, z - uz * 2, x, z, 0, 0)) { ok = s > 70.f; break; }
      py = g; len = s;
    }
    if (!ok || len < 70.f) continue;
    if (colDeathAt(CW, r->ax, r->ay + 1.f, r->az, 1.f) >= 0 || colDeathAt(CW, r->ax + ux * 60.f, r->ay + 1.f, r->az + uz * 60.f, 1.f) >= 0) continue;   // в зоне DEATH игра вернёт игрока на точку
    int dup = 0; for (int k = 0; k < nstr; k++) if (hypotf(str[k].x - r->ax, str[k].z - r->az) < 120.f) dup = 1;
    if (dup) continue;
    str[nstr++] = (Straight){r->ax, r->ay, r->az, h, len};
  }
}

// тангаж кузова относительно опоры под ним, рад (нос вверх +): тело минус уклон дороги по курсу
// Тангаж кузова относительно дороги. 09.10 (агент физики): уклон дороги — по плоскости колёс (земля под передней и задней осью),
// а не по нормали земли под началом машины: на горбах и в ямах (L1/L4 F11-5: уклон 0 → 9,7° → 0 за 24 м; L3/L6 H8-4) нормаль под
// точкой расходится с плоскостью четырёх колёс, и торможение там выглядело как «нос вверх» (81 случай), хотя на ровной площадке все
// эти машины клюют вниз (attitude.c C). Прежнее: -DRELPITCH_POINT_OLD=1
#ifndef RELPITCH_POINT_OLD
#define RELPITCH_POINT_OLD 0
#endif
static float relSlope;   // уклон дороги последнего relPitch, рад
static float relPitch(void) {
  float fw[3], gn[3] = {0, 1, 0}, gy; qFwd(W.car.q, fw);
  float fx = sinf(W.car.h), fz = cosf(W.car.h), slope;
  float lf = W.car.p.wheelF, lr = W.car.p.wheelR, gf, gr;
  if (!RELPITCH_POINT_OLD && lf + lr > .5f &&
      colGround(CW, W.car.x + fx * lf, W.car.z + fz * lf, W.car.y + 2.f, W.car.y - 3.f, &gf, 0) &&
      colGround(CW, W.car.x - fx * lr, W.car.z - fz * lr, W.car.y + 2.f, W.car.y - 3.f, &gr, 0))
    slope = atanf((gf - gr) / (lf + lr));
  else {
    if (!colGround(CW, W.car.x, W.car.z, W.car.y + 1.f, W.car.y - 2.f, &gy, gn)) { gn[0] = 0; gn[1] = 1; gn[2] = 0; }
    slope = asinf(fmaxf(-1, fminf(1, -(gn[0] * fx + gn[2] * fz))));
  }
  relSlope = slope;
  return asinf(fmaxf(-1, fminf(1, fw[1]))) - slope;
}
// ---------------------------------------------------------------- режим roll: все машины на прямых дорогах уровня
static void modeRoll(const char *only, int nplaces) {
  findStraights();
  if (!nstr) { cs("rollover", "fail", "roll:no-straights", 0, 0, 0, "нет прямых дорог 70 м"); return; }
  int places = nstr < nplaces ? nstr : nplaces;   // три прямые в разных частях уровня (сортировки нет: порядок отрезков в данных)
  static const char *MAN[3] = {"gas_turn", "ebrake_turn", "brake"};
  int nflip = 0, ntwo = 0, ninward = 0, nnoseup = 0, ncase = 0;
  for (int m_ = 0; m_ < CAR_CONS; m_++) {
    if (only && !strstr(only, carCons[m_].name)) continue;
    for (int pl = 0; pl < places; pl++) for (int man = 0; man < 3; man++) {
      if (!selectCase("roll:%s:s%d:%s", carCons[m_].name, pl, MAN[man])) continue;
      const Straight *s = &str[(pl * nstr / places + SEED - 1) % nstr];   // другой seed — другие прямые
      fresh(); useCar(carCons[m_].name, s->x, s->y + .3f, s->z, s->h);
      if (hypotf(W.car.x - s->x, W.car.z - s->z) > 3.f) { cs("rollover", "fail", CASEID, s->x, s->y, s->z, "машина не встала на прямую (игра перенесла её)"); continue; }
      int wall = 0;
      // разгон до 80 км/ч по прямой (не дольше 45 м пути и 8 с)
      float x0 = W.car.x, z0 = W.car.z;
      for (int f = 0; f < 8 * 60 && W.car.v < 80.f / 3.6f && hypotf(W.car.x - x0, W.car.z - z0) < 45.f; f++) holdLine(s->h, 99.f, 1, &wall);
      float vin = W.car.v, pitch0 = relPitch(), slope0 = relSlope;
      watchReset();
      float leanSum = 0, aSum = 0, pitchSum = 0; int nlean = 0, npitch = 0; float pvx = W.car.vel[0], pvz = W.car.vel[2];
      for (int f = 0; f < 4 * 60; f++) {
        unsigned b = man == 0 ? GP_R : man == 1 ? (GP_R | GP_SQUARE) : GP_L; float ax = man == 2 ? 0.f : 1.f;
        if (man == 2 && W.car.v < 1.f) b = 0;
        step(b, ax);
        float u[3], fw[3]; qUp(W.car.q, u); qFwd(W.car.q, fw);
        float rx = cosf(W.car.h), rz = -sinf(W.car.h), alat = ((W.car.vel[0] - pvx) * rx + (W.car.vel[2] - pvz) * rz) * 60.f;
        pvx = W.car.vel[0]; pvz = W.car.vel[2];
        float gn[3] = {0, 1, 0}, gy; if (!colGround(CW, W.car.x, W.car.z, W.car.y + 1.f, W.car.y - 2.f, &gy, gn)) { gn[0] = 0; gn[1] = 1; gn[2] = 0; }
        // крен и тангаж — относительно опоры под машиной (поперечный уклон и горка не выдаются за крен/клевок)
        if (man < 2 && f >= 20 && f < 100 && fabsf(alat) > 3.f && W.car.wheels >= 3) { leanSum += ((u[0] - gn[0]) * rx + (u[2] - gn[2]) * rz) * (alat > 0 ? -1.f : 1.f); aSum += fabsf(alat); nlean++; }
        // только на ровном уклоне (как в начале торможения ±1°): на изломе дороги кузов отстаёт от плоскости колёс (09.10, агент физики)
        if (man == 2 && f >= 5 && f < 50 && W.car.v > 2.f && W.car.wheels >= 3) { float rp = relPitch() - pitch0;
          if (RELPITCH_POINT_OLD || fabsf(relSlope - slope0) < .0175f) { pitchSum += rp; npitch++; } }
        (void)fw;
      }
      // lean > 0 — кузов клонится наружу поворота (как у настоящей машины и у старого порта), < 0 — внутрь (аудит 09.10 п. 1)
      float lean = nlean ? asinf(fmaxf(-1, fminf(1, leanSum / nlean))) * 57.2958f : NAN, dive = npitch ? pitchSum / npitch * 57.2958f : NAN;
      int flip = wa.flips > 0 || W.car.rights > wa.rights0, two = wa.twoRunMax > 30, inward = nlean > 10 && lean < -.5f, noseUp = npitch > 10 && dive > .5f;   // порог 0,5° — шум опоры
      nflip += flip; ntwo += two; ninward += inward; nnoseup += noseUp; ncase++;
      char m[400]; snprintf(m, sizeof m, "car=%s man=%s v_in=%.0f roll_max=%.1f two_wheel_s=%.2f flip=%d lean_out_deg=%.2f alat=%.1f dive_deg=%.2f air_s=%.2f wall_in_runup=%d",
         carCons[m_].name, MAN[man], vin * 3.6f, wa.rollMax, wa.twoRunMax / 60.f, flip, lean, nlean ? aSum / nlean : 0.f, dive, wa.airMax / 60.f, wall);
      // по строке на каждый замеренный признак: знаменатель покрытия — сколько раз признак меряли
      cs("rollover", flip ? "risk" : "ok", CASEID, W.car.x, W.car.y, W.car.z, "%s", m);
      if (man < 2) cs("two_wheel", two ? "risk" : "ok", CASEID, W.car.x, W.car.y, W.car.z, "%s", m);
      if (nlean > 10) cs("roll_direction", inward ? "risk" : "ok", CASEID, W.car.x, W.car.y, W.car.z, "%s (lean_out_deg > 0 — наружу поворота, как у настоящей машины)", m);
      if (npitch > 10) cs("pitch_direction", noseUp ? "risk" : "ok", CASEID, W.car.x, W.car.y, W.car.z, "%s (dive_deg < 0 — нос клюёт при торможении)", m);
      if (hoverCar) cs("hover_car", flip || two || wa.airMax > 60 ? "risk" : "ok", CASEID, W.car.x, W.car.y, W.car.z, "%s (машина без колёс по данным)", m);
    }
  }
  printf("STAT\troll\tcases=%d\tflips=%d\ttwo_wheel=%d\tlean_inward=%d\tnose_up_braking=%d\tstraights=%d\n", ncase, nflip, ntwo, ninward, nnoseup, nstr);
}

// ---------------------------------------------------------------- зоны прыжка: линия разгона из данных
typedef struct { float sx, sy, sz, h, lx, ly, lz, runup; int ok; } JLine;
static JLine jumpLine(const ColZone *z) {
  JLine best = {0}; float bestScore = -1e9f;
  const float *axes[3] = {z->ax, z->ay, z->az};
  for (int k = 0; k < 3; k++) {
    if (fabsf(axes[k][1]) > .5f) continue;
    float dx = axes[k][0], dz = axes[k][2], l = hypotf(dx, dz); if (l < .1f) continue; dx /= l; dz /= l;
    for (int sg = -1; sg <= 1; sg += 2) {
      float ddx = dx * sg, ddz = dz * sg;
      float g0 = groundBelow(z->c[0], z->c[1], z->c[2], 4.f, 10.f); if (!isfinite(g0)) g0 = z->c[1] - z->h[1];
      // кромка: ход вперёд по опоре от центра зоны (ступени до 1 м), последняя точка перед обрывом > 1 м
      float py = g0, lipS = 0; int drop = 0;
      for (float s = .5f; s <= 25.f; s += .5f) { float g = groundBelow(z->c[0] + ddx * s, py, z->c[2] + ddz * s, 1.f, 1.f); if (!isfinite(g)) { drop = 1; break; } py = g; lipS = s; }
      float lx = z->c[0] + ddx * lipS, lz = z->c[2] + ddz * lipS, ly = py;
      float after = groundBelow(lx + ddx * 3, ly, lz + ddz * 3, .5f, 60.f), dropH = isfinite(after) ? ly - after : 20.f;
      // разгон: назад от кромки по опоре без ступеней вверх > 0.6 м и без заборов, до 50 м
      float run = 0, ry = ly;
      for (float s = 1.f; s <= 50.f; s += 1.f) { float x = lx - ddx * s, zz = lz - ddz * s, g = groundBelow(x, ry, zz, .6f, 3.f);
        if (!isfinite(g) || colCrossFence(CW, x + ddx, zz + ddz, x, zz, 0, 0)) break; ry = g; run = s; }
      float rise = ly - groundBelow(lx - ddx * 8, ly, lz - ddz * 8, 1.f, 6.f); if (!isfinite(rise)) rise = 0;
      float score = (drop ? 5.f : 0.f) + fminf(dropH, 8.f) + fminf(rise, 3.f) * 2.f + (run >= 25.f ? 6.f : run / 5.f);
      if (score > bestScore) { bestScore = score; best = (JLine){lx - ddx * run, ry, lz - ddz * run, atan2f(ddx, ddz), lx, ly, lz, run, run >= 15.f}; }
    }
  }
  return best;
}
// заезд на трамплин: разгон к кромке и 8 с после отрыва. -> строка случая
static void jumpRun(const ColZone *z, const JLine *J, float kmh, float lat) {
  float rx = cosf(J->h), rz = -sinf(J->h);
  fresh(); useCar(0, J->sx + rx * lat, J->sy + .3f, J->sz + rz * lat, J->h);
  float vt = kmh / 3.6f; int took = 0, landed = 0, landFrame = 0, onWheels = 0, wallAir = 0; float maxY = -1e9f, ty = 0, pitchMax = -90, pitchMin = 90, pitchLand = NAN, dist = 0, tx = 0, tz = 0;
  int reachZone = 0, boosts0 = W.car.boosts, airF = -1;
  for (int f = 0; f < 20 * 60; f++) {
    float e = J->h - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
    unsigned b = took && !landed ? 0 : (W.car.v < vt ? GP_R : 0); float ax = took ? 0.f : fmaxf(-1.f, fminf(1.f, -e * 2.2f));
    step(b, ax);
    if (colZoneContains(z, W.car.x, W.car.y + .5f, W.car.z, 1.f)) reachZone = 1;
    float fw[3]; qFwd(W.car.q, fw); float pitch = asinf(fmaxf(-1, fminf(1, fw[1]))) * 57.2958f;
    // отрыв: колёса в воздухе у кромки после зоны; засчитан, если полёт дольше 0,15 с (короткий отскок на кочке — не прыжок)
    if (!took && W.car.wheels == 0 && reachZone && hypotf(W.car.x - J->lx, W.car.z - J->lz) < 12.f) { if (airF < 0) { airF = f; ty = W.car.y; tx = W.car.x; tz = W.car.z; maxY = -1e9f; pitchMax = -90; pitchMin = 90; wallAir = 0; } }
    if (airF >= 0 && !landed) { if (W.car.y > maxY) maxY = W.car.y; if (pitch > pitchMax) pitchMax = pitch; if (pitch < pitchMin) pitchMin = pitch;
      if (W.car.wallV > 3.f && W.car.wheels == 0) wallAir++;
      if (W.car.wheels > 0) { if (f - airF >= 9) { took = airF ? airF : 1; landed = 1; landFrame = f; pitchLand = pitch; dist = hypotf(W.car.x - tx, W.car.z - tz); } airF = -1; } }
    if (landed && f - landFrame >= 30 && f - landFrame <= 150 && !onWheels) { float u[3]; qUp(W.car.q, u); onWheels = u[1] > .7f && W.car.wheels >= 3; }
    if (landed && f - landFrame > 8 * 60) break;
    if (!took && airF < 0 && f > 12 * 60) break;
  }
  int noLand = airF >= 0 && !landed;   // в воздухе до конца заезда (20 с): висит, застрял на ребре или улетел
  if (noLand) took = airF ? airF : 1;
  float movedAfter = landed ? hypotf(W.car.x - tx, W.car.z - tz) - dist : 0; int escaped = -1;
  if (landed && movedAfter < 10.f) {   // встал после посадки: как игрок — 2 с назад с рулём, потом газ с рулём 4 с; выехал ли на 5 м
    float x0 = W.car.x, z0 = W.car.z;
    for (int f = 0; f < 120; f++) step(GP_L, 1.f);
    for (int f = 0; f < 240; f++) step(GP_R, -.6f);
    escaped = hypotf(W.car.x - x0, W.car.z - z0) > 5.f;
  }
  int flip = wa.flips > 0 || W.car.rights > wa.rights0, stuck = escaped == 0;
  const char *fam = !took ? "jump_approach" : flip || noLand ? "jump_landing" : wa.invisAir ? "air_wall" : !onWheels && landed ? "jump_landing" : stuck ? "jump_stuck" : "jumps";
  const char *verdict = !took ? "info" : (flip || noLand || wa.invisAir || (landed && !onWheels) || stuck) ? "risk" : "ok";
  cs(fam, verdict, CASEID, W.car.x, W.car.y, W.car.z, "zone=%s kmh=%.0f lat=%.1f no_landing=%d took=%d air_s=%.2f height=%.1f dist=%.1f pitch_air=%.1f..%.1f pitch_land=%.1f on_wheels=%d flip=%d wall_air=%d invis_air=%d moved_after=%.1f escaped=%d boosts=%d jumpcams=%d runup=%.0f",
     z->name, kmh, lat, noLand, took > 0, took && landed ? (landFrame - took) / 60.f : 0.f, took ? maxY - ty : 0.f, dist, pitchMin, pitchMax, pitchLand, onWheels, flip, wallAir, wa.invisAir, movedAfter, escaped,
     W.car.boosts - boosts0, wa.czOn, J->runup);
  if (took && nczcam) cs("jump_camera", "info", CASEID, J->lx, J->ly, J->lz, "zone=%s kmh=%.0f камер зоны машины включилось %d (из %d в пакете уровня)", z->name, kmh, wa.czOn, nczcam);
}
static void modeJumps(void) {
  int n = 0;
  for (unsigned i = 0; i < CW->nzone; i++) {
    const ColZone *z = &CW->zone[i]; JLine J = jumpLine(z);
    if (!J.ok) { if (selectCase("jump:%s:line", z->name)) cs("jump_approach", "info", CASEID, z->c[0], z->c[1], z->c[2], "zone=%s разгон по данным короче 15 м (%.0f м)", z->name, J.runup); continue; }
    static const float K[3] = {60, 90, 120}, LAT[3] = {-1.5f, 0, 1.5f};
    for (int k = 0; k < 3; k++) for (int l = 0; l < 3; l++) { if (!selectCase("jump:%s:%d:%+.1f", z->name, (int)K[k], LAT[l])) continue; jumpRun(z, &J, K[k], LAT[l]); n++; }
  }
  printf("STAT\tjumps\tzones=%u\tcases=%d\tcarzone_cams=%d\n", CW->nzone, n, nczcam);
}

// ---------------------------------------------------------------- изгороди
static void hedgeRun(float px, float pz, float nx, float nz, float kmh, const char *what) {
  float g = lowestGround(px + nx * 14, pz + nz * 14), gy = groundBelow(px + nx * 14, isfinite(g) ? g + 3 : 0, pz + nz * 14, 6.f, 6.f);
  if (!isfinite(gy)) { cs("hedges", "info", CASEID, px, 0, pz, "%s: нет опоры для разгона", what); return; }
  float h = atan2f(-nx, -nz); fresh(); useCar(0, px + nx * 14, gy + .3f, pz + nz * 14, h);
  float vt = kmh / 3.6f, vContact = -1, vMaxAfter = 0, vyMax = 0; int contact = -1, air = 0, trapped = 0;
  for (int f = 0; f < 6 * 60; f++) {
    float e = h - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
    unsigned b = contact >= 0 ? 0 : (W.car.v < vt ? GP_R : 0);   // после касания газ отпущен: разгон дальше — раскачка листвой
    step(b, fmaxf(-1.f, fminf(1.f, -e * 2.2f)));
    if (contact < 0 && (W.car.foliV > 0.f || W.car.foliCover > .05f)) { contact = f; vContact = fabsf(W.car.v); }
    if (contact >= 0) { if (fabsf(W.car.v) > vMaxAfter) vMaxAfter = fabsf(W.car.v); if (W.car.vel[1] > vyMax) vyMax = W.car.vel[1]; if (W.car.wheels == 0) air++; }
  }
  if (contact >= 0 && W.car.foliCover > .05f) {   // застрял в листве: 3 с назад, выехал ли
    float x0 = W.car.x, z0 = W.car.z; for (int f = 0; f < 3 * 60; f++) step(GP_L, 0); trapped = hypotf(W.car.x - x0, W.car.z - z0) < 1.f;
  }
  int flip = wa.flips > 0 || W.car.rights > wa.rights0, pump = contact >= 0 && vContact > 5.f && vMaxAfter > vContact + 1.5f, launch = vyMax > 3.f || air > 30;
  const char *fam = flip ? "rollover" : "hedges";
  cs(fam, flip ? "risk" : contact < 0 ? "info" : (pump || launch || trapped) ? "risk" : "ok", CASEID, px, gy, pz, "%s kmh=%.0f contact=%d v_contact=%.1f v_max_after=%.1f vy_max=%.1f air_s=%.2f trapped=%d flip=%d cover_end=%.2f",
     what, kmh, contact >= 0, vContact * 3.6f, vMaxAfter * 3.6f, vyMax, air / 60.f, trapped, flip, W.car.foliCover);
}
static int lowFoliage(const ColBox *b) {   // объём FOLIAGE у земли (куст, изгородь), не крона дерева над дорогой
  if (b->pad != 1.f) return 0;
  float ey = fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2], g = groundBelow(b->c[0], b->c[1], b->c[2], 2.f, 12.f);
  return isfinite(g) && b->c[1] - ey < g + 1.2f && b->c[1] + ey > g + .3f;
}
static void modeHedges(int maxN) {
  int nseg = 0, nb = 0; for (unsigned i = 0; i < CW->nseg; i++) nseg += CW->seg[i].y0 < COL_SEG_FOL_Y0;
  for (unsigned i = 0; i < CW->nbox; i++) nb += lowFoliage(&CW->box[i]);
  int stepS = nseg > maxN ? nseg / maxN : 1, stepB = nb > maxN ? nb / maxN : 1, k = 0, n = 0;
  for (unsigned i = 0; i < CW->nseg; i++) { const ColSeg *s = &CW->seg[i]; if (!(s->y0 < COL_SEG_FOL_Y0)) continue; if (k++ % stepS) continue;
    float mx = (s->a[0] + s->b[0]) / 2, mz = (s->a[1] + s->b[1]) / 2;
    for (int sd = -1; sd <= 1; sd += 2) for (int v = 40; v <= 70; v += 30) { if (!selectCase("hedge:s%u:%+d:%d", i, sd, v)) continue; char w[48]; snprintf(w, sizeof w, "изгородь-забор s%u", i); hedgeRun(mx, mz, s->n[0] * sd, s->n[1] * sd, (float)v, w); n++; } }
  k = 0;
  for (unsigned i = 0; i < CW->nbox; i++) { const ColBox *b = &CW->box[i]; if (!lowFoliage(b)) continue; if (k++ % stepB) continue;
    float ax = b->ax[0], az = b->ax[2], l = hypotf(ax, az); if (l < .1f) { ax = b->az[0]; az = b->az[2]; l = hypotf(ax, az); } if (l < .1f) continue; ax /= l; az /= l;
    for (int sd = -1; sd <= 1; sd += 2) { if (!selectCase("hedge:b%u:%+d:55", i, sd)) continue; char w[48]; snprintf(w, sizeof w, "куст-объём b%u", i); hedgeRun(b->c[0] + ax * sd * 1.5f, b->c[2] + az * sd * 1.5f, ax * sd, az * sd, 55.f, w); n++; } }
  if (!n && selectCase("hedge:none")) cs("hedges", "info", CASEID, 0, 0, 0, "в данных уровня нет изгородей у земли (заборов-изгородей %d, объёмов FOLIAGE у земли %d)", nseg, nb);
  printf("STAT\thedges\tfence_hedges=%d\tfoliage_boxes=%d\tcases=%d\n", nseg, nb, n);
}

// ---------------------------------------------------------------- тараны трафика
static void modeTraffic(int places) {
  int n = 0, cand = 0;
  for (int p = 0; p < places; p++) {
    int ni = (int)((unsigned)(p * 7919u + SEED * 104729u) % (unsigned)nnode); P3 nd = node[ni];
    if (!selectCase("traffic:n%d", ni)) continue;
    fresh(); useCar(0, nd.x, nd.y + .3f, nd.z, 0.f);
    for (int f = 0; f < 4 * 60; f++) step(0, 0);   // город просыпается вокруг игрока
    Traffic *tr = (Traffic *)bwTraffic(); if (!tr) { cs("traffic_hit", "fail", CASEID, nd.x, nd.y, nd.z, "трафика нет"); return; }
    int tgt = -1; float bd = 90.f;
    for (int i = 0; i < tr->maxCars; i++) if (tr->car[i].mode == TRM_DRIVE) { float d = hypotf(tr->car[i].car.x - W.car.x, tr->car[i].car.z - W.car.z); if (d < bd) { bd = d; tgt = i; } }
    if (tgt < 0) { cs("traffic_hit", "info", CASEID, nd.x, nd.y, nd.z, "рядом нет едущих машин"); continue; }
    cand++;
    int hit = -1; float vHit = 0;
    for (int f = 0; f < 25 * 60 && hit < 0; f++) {   // на перехват: прицел — точка машины трафика через 0.5 с
      const Car *k = &tr->car[tgt].car; float tx = k->x + k->vel[0] * .5f, tz = k->z + k->vel[2] * .5f;
      float e = atan2f(tx - W.car.x, tz - W.car.z) - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
      step(GP_R, fmaxf(-1.f, fminf(1.f, -e * 2.5f)));
      if (W.car.carHitV > 2.f) { for (int i = 0; i < tr->maxCars; i++) if (tr->car[i].mode != TRM_OFF && hypotf(tr->car[i].car.x - W.car.x, tr->car[i].car.z - W.car.z) < 7.f) { hit = i; break; } vHit = W.car.carHitV; }
      if (tr->car[tgt].mode == TRM_OFF) break;
    }
    if (hit < 0) { cs("traffic_hit", "info", CASEID, W.car.x, W.car.y, W.car.z, "не догнал машину трафика %d за 25 с", tgt); continue; }
    TrCar *t = &tr->car[hit]; float x0 = t->car.x, z0 = t->car.z, maxD = 0; int physF = 0, back = -1, reverse = 0, side = 0, hover = 0, wreck = 0;
    float px = t->car.x, pz = t->car.z;
    for (int f = 0; f < 10 * 60; f++) {
      step(W.car.v > .5f ? GP_L : 0, 0);
      if (t->mode == TRM_OFF) break;
      float d = hypotf(t->car.x - x0, t->car.z - z0); if (d > maxD) maxD = d;
      if (t->mode == TRM_PHYS) physF++; if (t->mode == TRM_WRECK) wreck = 1;
      if (t->mode == TRM_DRIVE && back < 0 && physF) back = f;
      if (t->mode == TRM_DRIVE) {   // едет по полосе: курс кузова против хода — задом наперёд (регресс №400); на боку — аудит п. 15
        float vx = (t->car.x - px) * 60.f, vz = (t->car.z - pz) * 60.f, sp = hypotf(vx, vz), u[3]; qUp(t->car.q, u);
        if (sp > 2.f && (vx * sinf(t->car.h) + vz * cosf(t->car.h)) / sp < 0.f) reverse++;
        if (u[1] < .5f) side++;
        float g = groundBelow(t->car.x, t->car.y, t->car.z, 1.5f, 8.f); if (isfinite(g) && t->car.y - g > 1.f) hover++;
      }
      px = t->car.x; pz = t->car.z;
    }
    char m[400]; snprintf(m, sizeof m, "victim=%d v_hit=%.0f knock_m=%.1f phys_s=%.1f back_to_lane_s=%.1f reverse_frames=%d side_frames=%d hover_frames=%d wreck=%d hnr=%.2f (PS2: 72 км/ч -> 8,8 м)",
       hit, vHit * 3.6f, maxD, physF / 60.f, back >= 0 ? back / 60.f : -1.f, reverse, side, hover, wreck, W.hnrHeat);
    cs("traffic_hit", "ok", CASEID, x0, t->car.y, z0, "%s", m);
    cs("traffic_rejoin", back < 0 ? "info" : reverse > 30 ? "risk" : "ok", CASEID, x0, t->car.y, z0, "%s%s", m, back < 0 ? " — за 10 с на полосу не вернулась" : "");
    cs("traffic_on_side", side ? "risk" : "ok", CASEID, x0, t->car.y, z0, "%s", m);
    cs("traffic_hover", hover > 30 ? "risk" : "ok", CASEID, x0, t->car.y, z0, "%s", m);
    n++;
  }
  printf("STAT\ttraffic\tplaces=%d\twith_target=%d\thits=%d\n", places, cand, n);
}

// ---------------------------------------------------------------- невидимые стены: радиальные заезды
static void radial(float cx, float cz, float r0, const char *tag, int dirs) {
  for (int d = 0; d < dirs; d++) for (int v = 40; v <= 70; v += 30) {
    if (!selectCase("phantom:%s:%d:%d", tag, d, v)) continue;
    float a = d * 2 * PI / dirs, sx = cx + sinf(a) * r0, sz = cz + cosf(a) * r0, g = lowestGround(sx, sz);
    float top = -1e9f; { float t = 1e4f, y, n3[3]; for (int k = 0; k < 16 && colGround(CW, sx, sz, t, -1e4f, &y, n3); k++) { if (n3[1] > .8f) { top = y; } t = y - 1.f; } }   // самая нижняя пологая опора
    if (!isfinite(g)) continue; (void)top;
    float gy = g, h = atan2f(cx - sx, cz - sz);
    fresh(); useCar(0, sx, gy + .3f, sz, h);
    int hits = 0; float worst = 0; float wp[3] = {0}; int invis = 0, invFence = 0, wt = 0; unsigned wi = 0; int air = 0;
    for (int f = 0; f < 5 * 60; f++) {
      float e = h - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
      step(W.car.v < v / 3.6f ? GP_R : 0, fmaxf(-1.f, fminf(1.f, -e * 2.2f)));
      if (W.car.wallV > 3.f) { hits++; if (!visAt(W.car.wallP, W.car.wallN[0], W.car.wallN[1])) {
          if ((W.car.wallType & 3) == COL_SEG && W.car.wheels > 0) { invFence++; continue; }
          invis++; if (W.car.wallV > worst) { worst = W.car.wallV; memcpy(wp, W.car.wallP, sizeof wp); wt = W.car.wallType; wi = W.car.wallIdx; air = W.car.wheels == 0; } } }
    }
    if (invis) cs(air ? "air_wall" : "invisible_wall", "risk", CASEID, wp[0], wp[1], wp[2], "focus=%s dir=%d kmh=%d hits=%d invisible=%d prim=%c%u worst_v=%.1f invisible_fence=%d", tag, d, v, hits, invis, "tbcs"[wt & 3], wi, worst, invFence);
    else if (invFence) cs("invisible_fence", "info", CASEID, sx, gy, sz, "focus=%s dir=%d kmh=%d hits=%d invisible_fence=%d (забор данных без рисунка: у оригинала тоже есть)", tag, d, v, hits, invFence);
    else cs("invisible_wall", "ok", CASEID, sx, gy, sz, "focus=%s dir=%d kmh=%d hits=%d", tag, d, v, hits);
  }
}
static float focus[16][2]; static char focusTag[16][16]; static int nfocus;
static void modePhantom(int nodes) {
  for (int i = 0; i < nfocus; i++) radial(focus[i][0], focus[i][1], 22.f, focusTag[i], 16);
  for (int p = 0; p < nodes; p++) { int ni = (int)((unsigned)(p * 2654435761u + SEED) % (unsigned)nnode); char t[24]; snprintf(t, sizeof t, "n%d", ni); radial(node[ni].x, node[ni].z, 18.f, t, 8); }
  printf("STAT\tphantom\tfocus=%d\tnodes=%d\n", nfocus, nodes);
}

// ---------------------------------------------------------------- логичное прохождение одним миром
typedef struct { float x, y, z; int kind; char name[40]; int zone; } Tgt;   // kind: 0 миссия, 1 трамплин, 2 срезка
static Tgt tg[512]; static int ntg;
static void missionTargets(void) {
  if (!W.haveMsn) return; const MsnFile *f = &W.msn.f;
  for (int pass = 0; pass < 2; pass++) for (uint32_t m = 0; m < f->nmissions; m++) {
    const MsnMission *M = &f->missions[m]; int bonus = (M->flags & (MSN_MF_BONUS | MSN_MF_STREETRACE | MSN_MF_GAMBLE | MSN_MF_BONUSMISSION)) != 0;
    if (bonus != pass) continue;
    uint16_t ph = M->intro != MSN_NONE16 ? M->intro : M->main; if (ph == MSN_NONE16 || ph >= f->nphases) continue;
    const MsnPhase *P = &f->phases[ph]; const MsnLoc *best = 0;
    for (uint32_t s = P->first_stage; s < (uint32_t)P->first_stage + P->stage_count && s < f->nstages && !best; s++) {
      const MsnStage *S = &f->stages[s];
      for (uint32_t c = S->cmd_first; c < S->cmd_first + S->cmd_count && c < f->ncmds && !best; c++)
        for (int a = 0; a < f->cmds[c].argc && !best; a++) { const MsnLoc *L = msn_arg_loc(f, &f->cmds[c], a);
          if (L && L->type != MSN_LT_ZONE && L->type != MSN_LT_OCCLUSION && L->type != MSN_LT_INTERIOR && isfinite(L->pos[0])) best = L; }
    }
    if (!best || ntg >= 512) continue;
    { float g = groundBelow(best->pos[0], best->pos[1], best->pos[2], 3.f, 3.f);   // точка внутри комнаты (под городом) — до неё не доехать
      if (!isfinite(g)) { if (selectCase("progress:seed%u", SEED)) cs("progress_leg", "info", CASEID, best->pos[0], best->pos[1], best->pos[2], "target=%s:%s — точка не на опоре города (интерьер), пропуск", msn_str(f, M->id), msn_str(f, best->name)); continue; } }
    Tgt *t = &tg[ntg++]; t->x = best->pos[0]; t->y = best->pos[1]; t->z = best->pos[2]; t->kind = 0; t->zone = -1;
    snprintf(t->name, sizeof t->name, "%s:%s", msn_str(f, M->id), msn_str(f, best->name));
  }
}
static int extraTargets(const char *roadsPath, Tgt *ex, int cap) {
  int n = 0;
  for (unsigned i = 0; i < CW->nzone && n < cap; i++) { ex[n] = (Tgt){CW->zone[i].c[0], CW->zone[i].c[1], CW->zone[i].c[2], 1, "", (int)i}; snprintf(ex[n].name, sizeof ex[n].name, "%s", CW->zone[i].name); n++; }
  char mp[600]; snprintf(mp, sizeof mp, "%s", roadsPath); char *dot = strrchr(mp, '.'); if (dot) snprintf(dot, sizeof mp - (size_t)(dot - mp), ".meta.tsv");
  FILE *f = fopen(mp, "r"); if (!f) return n;
  char ln[512], last[64] = ""; int seg = 0;
  while (fgets(ln, sizeof ln, f) && n < cap) {
    if (ln[0] == '#') continue; char sn[96], rn[96]; int mc, sp, it, sc;
    if (sscanf(ln, "%95s %95s %d %d %d %d", sn, rn, &mc, &sp, &it, &sc) == 6 && sc == 1 && seg < nroad && strcmp(rn, last)) {
      Road *r = &roads[seg]; ex[n] = (Tgt){(r->ax + r->bx) / 2, (r->ay + r->by) / 2, (r->az + r->bz) / 2, 2, "", -1}; snprintf(ex[n].name, sizeof ex[n].name, "%s", rn); n++;
      snprintf(last, sizeof last, "%s", rn); }
    seg++;
  }
  fclose(f); return n;
}
// к точке (gx, gy, gz) по графу дорог; застрял (медленнее 1 м/с 1,5 с) — 1,2 с назад с обратным рулём и новый путь в объезд места
// (navAvoid), каждая 4-я попытка — START (RESET CAR), как игрок. -> 1 доехал ближе R
#ifndef ARRESET_OLD
#define ARRESET_OLD 0   // 1 = прежний счёт «RESET вернул в то же место» (две посадки подряд рядом, без проверки сдвига)
#endif
static int rescuesN; static int *rescues = &rescuesN;
static int drive(float gx, float gy, float gz, float R, float secs, int *resets) {
  P3 q[MAXWP]; int n = navPlan(W.car.x, W.car.y, W.car.z, gx, gy, gz, q, MAXWP);
  int seg = 0, tries = 0, same = 0; float footT = 0, slow = 0, backT = 0, backAx = 0, rx = 1e9f, rz = 1e9f; float par[NPAR]; memcpy(par, PAR0, sizeof par);
  if (SEED > 1) { uint32_t r = SEED * 2654435761u; for (int k = 0; k < NPAR; k++) { r ^= r << 13; r ^= r >> 17; r ^= r << 5;   // другой водитель на каждый seed: числа ±15 %
      par[k] = fminf(PARHI[k], fmaxf(PARLO[k], PAR0[k] * (1.f + .3f * ((r % 1000) / 1000.f - .5f)))); } }
  for (int f = 0; f < (int)(secs * 60); f++) {
    unsigned b; float ax; navSteer(q, n, &seg, &W.car, par, &b, &ax);
    if (same >= 2) {   // RESET CAR дважды вернул машину в то же место: игрок здесь заперт — случай, и бот переносится на дорогу (вмешательство теста)
      int k = nearestNode(W.car.x + 30.f * sinf(W.car.h), W.car.y, W.car.z + 30.f * cosf(W.car.h)); P3 a = node[k]; int k2 = -1; float hh = 0;
      for (int j = adjStart[k]; j < adjStart[k + 1]; j++) { k2 = adj[j]; break; }
      if (k2 >= 0) hh = atan2f(node[k2].x - a.x, node[k2].z - a.z);
      cs("pocket_trap", "risk", CASEID, W.car.x, W.car.y, W.car.z, "RESET CAR возвращает в то же место (%d раза); бот перенесён на узел дороги %d (вмешательство теста)", same, k);
      bwPlace(&W, a.x, a.y + .3f, a.z, hh); same = 0; rx = rz = 1e9f; (*rescues)++;
      n = navPlan(W.car.x, W.car.y, W.car.z, gx, gy, gz, q, MAXWP); seg = 0; continue;
    }
    if (W.car.destroyed && !gameOnFoot(&W.G)) {   // машина разбита (остов едет 16 км/ч): игрок вызвал бы новую в будке — тест ставит новую той же модели здесь
      cs("car_destroyed", "info", CASEID, W.car.x, W.car.y, W.car.z, "урон 1.0 за %.0f с прогона; новая машина на месте (вмешательство теста)", frame / 60.f);
      CarParams cp = W.car.p; float x = W.car.x, y = W.car.y, z = W.car.z, h = W.car.h; carInit(&W.car, &cp, CW, x, y + .3f, z, h); bwPlace(&W, x, y + .3f, z, h); (*rescues)++;
    }
    if (backT > 0) { b = GP_L; ax = backAx; backT -= BW_DT; if (backT <= 0) { n = navPlan(W.car.x, W.car.y, W.car.z, gx, gy, gz, q, MAXWP); seg = 0; } }
    else if (slow > 1.5f) { navAvoid(W.car.x, W.car.z);
      if (++tries % 4 == 0) { b = GP_START; (*resets)++; } else { backT = 1.2f; backAx = -ax; } slow = 0; }
    float pbx = W.car.x, pbz = W.car.z; step(b, ax);
    if (b & GP_START) { n = navPlan(W.car.x, W.car.y, W.car.z, gx, gy, gz, q, MAXWP); seg = 0;
      // №1248в: «RESET вернул в то же место» = машину не вынесло (сдвиг от места, где застряла, < 8 м). Прежде считали только близость двух посадок подряд: когда RESET честно выносит из тупика на 28 м, а бот снова едет в тот же тупик за недостижимой целью, ложно писало «ловушку» (L7 G5-3)
      int stayed = ARRESET_OLD || hypotf(W.car.x - pbx, W.car.z - pbz) < 8.f;
      if (!stayed) { same = 0; rx = W.car.x; rz = W.car.z; } else
      if (hypotf(W.car.x - rx, W.car.z - rz) < 8.f) same++; else same = 0; rx = W.car.x; rz = W.car.z; }
    if (getenv("AR_TRACE") && f % 300 == 0) fprintf(stderr, "  %.0f с: машина %.1f %.1f %.1f v %.1f пешком %d seg %d/%d цель %.0f %.0f урон %.2f разбита %d husk %d msn places %u tp %u\n", frame / 60.f, W.car.x, W.car.y, W.car.z, W.car.v, gameOnFoot(&W.G), seg, n, gx, gz, W.car.damage, W.car.destroyed, W.car.huskDrive, W.msn.places, W.msn.teleports);
    if (gameOnFoot(&W.G)) {   // пешком (старт уровня, сценка, машина разбита и игрок вышел) — к машине и за руль
      footT += BW_DT; step(footT < 3.f && !W.car.destroyed ? GP_TRIANGLE : 0, 0);
      if (footT > 3.f) {   // за 3 с не сел (машина разбита или далеко): игрок вызвал бы новую в будке — тест ставит её у игрока
        cs("car_destroyed", "info", CASEID, W.homer.x, W.homer.y, W.homer.z, "пешком 3 с, машина %s (урон %.2f); новая машина у игрока (вмешательство теста)", W.car.destroyed ? "разбита" : "далеко", W.car.damage);
        CarParams cp = W.car.p; float x = W.homer.x, y = W.homer.y, z = W.homer.z, h = W.car.h; carInit(&W.car, &cp, CW, x, y + .5f, z, h); bwPlace(&W, x, y + .5f, z, h); (*rescues)++; footT = 0;
        n = navPlan(W.car.x, W.car.y, W.car.z, gx, gy, gz, q, MAXWP); seg = 0; }
      continue; }
    footT = 0;
    if (fabsf(W.car.v) < 1.f) slow += BW_DT; else slow = 0;
    if (hypotf(W.car.x - gx, W.car.z - gz) < R && fabsf(W.car.y - gy) < 4.f) return 1;
  }
  return 0;
}
static void modeProgress(const char *roadsPath, float secsMax) {
  selectCase("progress:seed%u", SEED); watchLog = 1;
  missionTargets(); int nm = ntg;
  static Tgt ex[256]; int nex = extraTargets(roadsPath, ex, 256); char used[256] = {0};
  // порядок: миссия → ближайший трамплин (до 400 м) → ближайшая срезка (до 300 м) → следующая миссия; после — остальное ближайшим
  static Tgt order[800]; int no = 0; float cx = W.car.x, cz = W.car.z;
  for (int i = 0; i <= nm; i++) {
    if (i < nm) { order[no++] = tg[i]; cx = tg[i].x; cz = tg[i].z; }
    for (int kind = 1; kind <= 2; kind++) {
      int b = -1; float bd = kind == 1 ? 400.f : 300.f;
      for (int k = 0; k < nex; k++) if (!used[k] && ex[k].kind == kind) { float d = hypotf(ex[k].x - cx, ex[k].z - cz); if (d < bd) { bd = d; b = k; } }
      if (b >= 0) { used[b] = 1; order[no++] = ex[b]; cx = ex[b].x; cz = ex[b].z; }
    }
  }
  for (;;) { int b = -1; float bd = 1e9f; for (int k = 0; k < nex; k++) if (!used[k]) { float d = hypotf(ex[k].x - cx, ex[k].z - cz); if (d < bd) { bd = d; b = k; } }
    if (b < 0 || no >= 800) break; used[b] = 1; order[no++] = ex[b]; cx = ex[b].x; cz = ex[b].z; }
  for (int f = 0; f < 4 * 60 && gameOnFoot(&W.G); f++) step(f & 1 ? 0 : GP_TRIANGLE, 0);   // старт пешком: △ у машины
  if (gameOnFoot(&W.G)) { cs("progress_leg", "info", CASEID, W.car.x, W.car.y, W.car.z, "старт пешком, машина не рядом: игрок посажен в машину уровня (вмешательство теста)"); bwPlace(&W, W.car.x, W.car.y + .3f, W.car.z, W.car.h); }
  printf("STAT\tprogress_plan\tmissions=%d\tjumps=%u\textra=%d\tlegs=%d\n", nm, CW->nzone, nex, no);
  int reached = 0, resets = 0, legs = 0; float km = 0, lx = W.car.x, lz = W.car.z;
  for (int i = 0; i < no && frame < (unsigned long long)(secsMax * 60); i++) {
    Tgt *t = &order[i]; int ok;
    float gx = t->x, gy = t->y, gz = t->z; JLine J;
    if (t->kind == 1) { J = jumpLine(&CW->zone[t->zone]); if (J.ok) { gx = J.sx; gy = J.sy; gz = J.sz; } }
    float d0 = hypotf(gx - W.car.x, gz - W.car.z); int r0 = resets; unsigned long long f0 = frame;
    // точка вне дорог (дверь, двор): как игрок — по дорогам до ближнего к ней узла, дальше напрямую
    int nn = nearestNode(gx, gy, gz); float off = nn >= 0 ? hypotf(node[nn].x - gx, node[nn].z - gz) : 0.f;
    ok = drive(gx, gy, gz, t->kind == 0 ? 15.f : 10.f, 40.f + d0 / 5.f, &resets);
    int viaRoad = 0; if (!ok && off > 15.f && nn >= 0) { viaRoad = hypotf(W.car.x - node[nn].x, W.car.z - node[nn].z) < 12.f; }
    if (t->kind == 1 && J.ok && (ok || hypotf(W.car.x - J.lx, W.car.z - J.lz) < 70.f)) {   // трамплин: к кромке и газ до упора, 6 с после отрыва
      int took = 0; float ty = 0, maxY = -1e9f; unsigned long long tf = 0; int pass = 0;
      for (int f = 0; f < 20 * 60; f++) {
        float aim = pass ? J.h : atan2f(J.lx - W.car.x, J.lz - W.car.z), e = aim - W.car.h; while (e > PI) e -= 2 * PI; while (e < -PI) e += 2 * PI;
        if (!pass && hypotf(W.car.x - J.lx, W.car.z - J.lz) < 4.f) pass = 1;
        step(took ? 0 : GP_R, took ? 0.f : fmaxf(-1.f, fminf(1.f, -e * 2.2f)));
        if (!took && W.car.wheels == 0 && hypotf(W.car.x - J.lx, W.car.z - J.lz) < 12.f) { took = 1; ty = W.car.y; tf = frame; }
        if (took && W.car.y > maxY) maxY = W.car.y;
        if (took && frame - tf > 6 * 60) break; }
      ok = ok || took;
      cs("jumps", took ? "ok" : "info", CASEID, W.car.x, W.car.y, W.car.z, "leg=%d zone=%s took=%d height=%.1f v=%.0f", i, t->name, took, took ? maxY - ty : 0.f, fabsf(W.car.v) * 3.6f);
    }
    legs++; reached += ok || viaRoad; km += hypotf(W.car.x - lx, W.car.z - lz) / 1000.f; lx = W.car.x; lz = W.car.z;
    cs("progress_leg", ok || viaRoad ? "ok" : "info", CASEID, t->x, t->y, t->z, "leg=%d kind=%s target=%s reached=%d via_road=%d game_s=%.0f resets=%d dist0=%.0f off_road=%.0f",
       i, t->kind == 0 ? "mission" : t->kind == 1 ? "jump" : "shortcut", t->name, ok, viaRoad, (frame - f0) / 60.f, resets - r0, d0, off);
  }
  printf("STAT\tprogress\tlegs=%d\treached=%d\tresets=%d\trescues=%d\tgame_s=%.0f\tkm=%.2f\thash=%016llx\tbw_hash=%016llx\tflips=%d\tinvisible=%d\tair_walls=%d\tfalls=%d\tstuck=%d\ttraffic_hover=%d\ttraffic_side=%d\tcam_eye_wall=%d\tcam_ground=%d\tcam_squeezed=%d\tjump_cams=%d\tprops_kicked=%u\thnr=%.2f\n",
         legs, reached, resets, rescuesN, frame / 60.f, km, (unsigned long long)arHash(0), (unsigned long long)bwHash(&W), wa.flips, wa.invis, wa.invisAir, wa.falls, wa.stuckN, wa.trHover, wa.trSide, wa.camEye, wa.camGround, wa.camSqueeze, wa.czOn, W.st.propKicks, W.hnrHeat);
}

#ifdef ALLRISK_MISSION_LIBRARY
int missionerRun(int argc, char **argv);
#endif
int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "usage: allrisk LEVEL PLAYER ROADS MSN --mode progress|roll|jumps|hedges|traffic|phantom [--seed S] [--secs T] [--only ID] [--cars a,b] [--focus TAG X Z] [--n N]\n"); return 2; }
  const char *level = argv[1], *player = argv[2], *roadsP = argv[3], *msn = strcmp(argv[4], "-") ? argv[4] : 0;
  float secs = 3600; const char *cars = 0; int nn = 0;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "--mode") && i + 1 < argc) MODE = argv[++i];
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc) SEED = (unsigned)strtoul(argv[++i], 0, 10);
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) ONLY = argv[++i];
    else if (!strcmp(argv[i], "--cars") && i + 1 < argc) cars = argv[++i];
    else if (!strcmp(argv[i], "--n") && i + 1 < argc) nn = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--focus") && i + 3 < argc && nfocus < 16) { snprintf(focusTag[nfocus], 16, "%s", argv[i + 1]); focus[nfocus][0] = (float)atof(argv[i + 2]); focus[nfocus][1] = (float)atof(argv[i + 3]); nfocus++; i += 3; }
  }
#if defined(ALLRISK_MISSION_LIBRARY) && !defined(ALLRISK_MISSION_OLD)
  // Actual input-driven missioner shares world.c and mission runtime, with per-stage telemetry.
  if (!strcmp(MODE, "mission")) return missionerRun(argc, argv);
#endif
  if (bwData(level, player, msn)) { printf("CASE\tdata\tfail\tload\tL?\t--\t0\t0\t0\t-\t0\tнет данных уровня/игрока/миссий\n"); return 2; }
  LV = bwLevel(); CW = bwCol(); navLoad(roadsP); loadRender(level); loadCarzone(level);
  bwNew(&W); srand(SEED);
  printf("STAT\tdata\tlevel=%d\tcar=%s\tcar_source=%d\trender_tris=%u\troads=%d\tnodes=%d\tzones=%u\tcarzone_cams=%d\tseed=%u\n", LV, bwCarName(), bwCarSource(), NT, nroad, nnode, CW->nzone, nczcam, SEED);
  if (!strcmp(MODE, "progress")) modeProgress(roadsP, secs);
  else if (!strcmp(MODE, "roll")) modeRoll(cars, nn ? nn : 3);
  else if (!strcmp(MODE, "jumps")) modeJumps();
  else if (!strcmp(MODE, "hedges")) modeHedges(nn ? nn : 16);
  else if (!strcmp(MODE, "traffic")) modeTraffic(nn ? nn : 12);
  else if (!strcmp(MODE, "phantom")) modePhantom(nn ? nn : 10);
  else { fprintf(stderr, "allrisk: неизвестный режим %s\n", MODE); return 2; }
  statPerf();
  return 0;
}
