// Трафик, стоящие машины и пешеходы уровня (этап 5). Что и по каким данным — viewer/traffic.h. Чистый C99; на Маке
// собирается с -DSKIN_HOST (tests/traffic/host.sh), на PSP — в EBOOT (viewer/CMakeLists.txt).
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>          // snprintf (правка 240: имя в событии)
#include "traffic.h"
#include "carwheels.h"       // правка 354: скрытые колёса оригинала
#include "wheelcap.h"        // №867: боковины колеса (картинка диска) без вращения
#include "homer.h"          // HOMER_STEP / HOMER_HEIGHT: те же пороги, что у стейтпропов для Гомера
#ifdef SKIN_HOST
#include <stdio.h>
#else
#include <malloc.h>
#include <pspkernel.h>
#endif

// ---- правила порта (в данных нет; числа — по ощущению, сверка с эталоном PS2 позже)
#define SPAWN_LEAD 40.f      // центр кольца — игрок + 40 м вперёд (hit-and-run-web TRAFFIC_RULES.cameraLead)
#define SPAWN_R 65.f         // кольцо спавна (spawnRadius)
#define SPAWN_BAND 10.f      // ширина кольца
#define REMOVE_R 75.f        // удаление вне кадра (removeRadius)
#define REMOVE_HARD 130.f    // удаление в любом случае
#define INIT_R 100.f         // при сбросе — до 100 м (initialRadius)
// №Г10/К-57 (находки PS2-Наблюдателя 10.10, fact ps2-traffic-natural-restart): минивэн после остановки без удара трогается сам и набирает
// 4,15 м/с за 1 с, 11,70 за 3 с, 19,53 за 5 с, то есть около 3,9 м/с2 (порт 3,0: 3 / 9 / 15). Г11: у перекрёстка стоит около 2 с (кадры 438-536
// при 50 Гц), порт 1 с. Правило порта по записи одного минивэна. TRAFFIC_RESTART_OLD=1 — прежние 3,0 м/с2 и 1 с
#ifndef TRAFFIC_RESTART_OLD
#define TRAFFIC_RESTART_OLD 0
#endif
#define ACCEL (TRAFFIC_RESTART_OLD ? 3.f : 3.9f)            // разгон трафика, м/с2
#define DECEL 6.f            // торможение до препятствия, м/с2
#define STOP_GAP 3.f         // стоп за 3 м до препятствия
#define LAT_G 3.f            // в повороте не больше sqrt(3 * R); кривая перекрёстка (изгиб полосы — LAT_G_FIT, аудит п. 22)
#define STOP_WAIT (TRAFFIC_RESTART_OLD ? 1.f : 2.f)        // trafficBehaviour 1: постоять у края перекрёстка
#define JAM_WAIT 4.f         // ждёт занятый перекрёсток дольше — едет
#define LOOK 4.f             // точка преследования впереди
#define DECIDE 12.f          // выезд выбирается за 12 м до конца полосы (не позже тормозного пути, аудит 09.10 п. 22)
// Аудит 09.10 п. 22: ход трафика по трассам близнеца PS2 tw_v14_* (минивэн 0xeee320, tools/research/traffic22/minivan_pace.py,
// шаг кадра 19,9 мс по машине игрока той же трассы). Три дороги L1 со скоростью данных 50 км/ч (13,9 м/с): на ходу 16,3–18,0 м/с,
// медиана 17,3 по 14 замерам — PACE_K = 17,3 / 13,9 ≈ 1,25. На изгибах (R ≈ 20–50 м) скорость не падает при поперечном 6–14 м/с²
// — LAT_G_FIT = 14 (наибольшее записанное без сброса скорости) на изгибах полос; кривые перекрёстков в записи не видны — прежний LAT_G. Торможение к перекрёстку 15,7 -> 5,9 м/с
// за 1,4 с (7,0 м/с²), 17,1 -> 4,0 за 2,4 с (5,5) — DECEL 6 совпадает; начинается за ~22 м до остановки — выезд выбирается не позже
// тормозного пути с DECEL (+1 м, правило порта), иначе порт тормозил за 11 м с 12 м/с². Разгон с места, ожидание у перекрёстка
// (STOP_WAIT) в записи не видны — прежние. Включено после правила занятия перекрёстка (TRAFFIC_JUNC_OLD выше). TRAFFIC_PACE_OLD=1 — прежнее (красный tests/traffic/pace.sh)
// ОСТАТОК 09.10 п. 17: в занятый перекрёсток не въезжать. Прежде после JAM_WAIT (4 с) машина въезжала в занятый — две машины,
// простоявшие у одного перекрёстка, въезжали разом и вставали друг в друге (tests/traffic/host.sh с темпом PS2: 220 кадров
// наложения; журнал ENTER busy1 wait4.0). TRAFFIC_JUNC_OLD=1 — прежнее (красный tests/traffic/junction.sh)
#ifndef TRAFFIC_JUNC_OLD
#define TRAFFIC_JUNC_OLD 0
#endif
#ifndef TRAFFIC_PACE_OLD
#define TRAFFIC_PACE_OLD 0
#endif
// №624 (TEST18: «NPC-машины слишком быстро»): замер Sonnet-PS2 по 5 слотам пула L1, 135 окон 0,2 с (knowledge/facts/ps2-traffic-cruise-speed.yaml):
// ход оригинала выше 15 м/с — медиана 16,5 (р10 15,9, р90 16,9), у каждой машины 16,3–16,6; порт 17,4 был на 5 % быстрее. PACE_K = 16,5 / 13,9 ≈ 1,185.
// TRAFFIC_PACE_K_OLD=1 — прежние 1,25 (17,4 м/с)
#ifndef TRAFFIC_PACE_K_OLD
#define TRAFFIC_PACE_K_OLD 0
#endif
#define PACE_K (TRAFFIC_PACE_OLD ? 1.f : (TRAFFIC_PACE_K_OLD ? 1.25f : 1.185f))
#define LAT_G_FIT (TRAFFIC_PACE_OLD ? LAT_G : 14.f)
#define ROAD_V(t, r) (((t)->road[r].speed ? (t)->road[r].speed : 50) / 3.6f * PACE_K)
#define PED_SPAWN_MIN 15.f   // пешеходы появляются в 15..45 м (при сбросе от 8 м)
#define PED_SPAWN_MAX 45.f
#define PED_REMOVE 60.f
#define PED_HIT_V TR_PED_HIT_V   // машина быстрее — сбивает, медленнее — отталкивает (traffic.h; правка 240 — и персонажи миссий)
#define PED_G 20.f           // падение сбитого пешехода
#define PED_DOWN TR_PED_DOWN     // лежит
// Правка 240: отлёт сбитого (trafficCarHitsBody, traffic.h) — правило порта по записи PS2 (7:11,1–7:11,3: удар углом капота на
// газоне — сбитый уходит от кузова вбок ~10 м/с и вперёд примерно со скоростью машины, виден рядом с ней, под машину не уходит).
// Подбор: удар углом 45° на 15 м/с даёт вбок 10,4 м/с, вперёд 14,9 м/с; лоб в лоб — 1,5 скорости машины + 2 м/с вперёд
#ifndef TR_KNOCK_OLD
#define TR_KNOCK_OLD 0       // 1 — прежний отлёт (красный контроль): 0,8 скорости машины + 2 м/с по нормали
#endif
#define TR_KNOCK_E .5f       // по нормали: (1 + E) скорости сближения
#define TR_KNOCK_T .3f       // вдоль кузова: доля касательной скорости машины
#define TR_KNOCK_MIN 2.f     // по нормали сверх того, м/с (прежнее «+ 2 м/с по нормали»)
#define TR_BODY_MID .8f      // луч полёта к стенам — на этой высоте над ступнями, м (сбитый не пролетает сквозь дома; правило порта)
// Правка 180 (близнец Б2: оригинал читает p_nhitbyc_* — прохожий «чуть не сбит»): машина игрока быстрее PED_NEAR_V проходит
// ближе PED_NEAR_R от круга прохожего, не задев, и идёт к нему — прохожий уклоняется (клип npd_dodge рига) и кричит; не чаще раза
// в PED_NEAR_COOL. Пороги — правило порта (условий оригинала на диске нет)
#define PED_NEAR_V 8.f
#define PED_NEAR_R 2.f
#define PED_NEAR_COOL 4.f
#define PED_WALK_DEF 1.3f    // если у клипа нет корня
#define PARK_BACK 100.f      // сдвинутая стоящая машина вне кадра дальше этого — на место
#define HIT_PED TR_HIT_PED   // доли шкалы Hit & Run (gameplay.md §6 / 100)
#define HIT_CAR .05f
#define HIT_WRECK .25f
#define HIT_KICK .13f

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
// правка 252: как wrapPi в homer.c — угол-бесконечность/1e30 не крутит цикл вечно; |a| < 64 — как было
static float wrapPi(float a) {
  if (!(a > -64.f && a < 64.f)) a = (a > -1e30f && a < 1e30f) ? fmodf(a, 6.2831853f) : 0.f;
  while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a;
}
static float rnd(Traffic *t) { t->seed = t->seed * 1664525u + 1013904223u; return (float)(t->seed >> 8) * (1.f / 16777216.f); }
static int inside(const Traffic *t, uint32_t off, uint32_t len) { return off <= t->size && len <= t->size - off; }
static void pushEvW(Traffic *t, int type, int idx, float x, float y, float z, float amount, const char *who) {
  if (t->nev >= 16) return;
  TrEvent *e = &t->ev[t->nev++]; e->type = type; e->index = idx; e->x = x; e->y = y; e->z = z; e->amount = amount;
  memset(e->who, 0, sizeof e->who); snprintf(e->who, sizeof e->who, "%.15s", who ? who : "");   // правка 240: имя для голоса
                                                                                                   // (имя модели TrPedModel — до 16 байт)
}
static void pushEv(Traffic *t, int type, int idx, float x, float y, float z, float amount) { pushEvW(t, type, idx, x, y, z, amount, 0); }
void trafficPushEvent(Traffic *t, int type, int idx, float x, float y, float z, float amount, const char *who) {
  if (t && t->hd) pushEvW(t, type, idx, x, y, z, amount, who);
}

// ================================================================ загрузка
static uint32_t fnv1a(const char *s) { uint32_t h = 0x811C9DC5u; while (*s) { h ^= (unsigned char)*s++; h *= 0x01000193u; } return h; }
int trafficParkModel(const Traffic *t, const char *name) {
  int list[TR_MAXMODEL], n = 0;
  for (uint32_t i = 0; i < t->hd->nSlot; i++) if (t->slot[i].park) {
    int m = t->slot[i].model, dup = 0; for (int k = 0; k < n; k++) dup |= list[k] == m;
    if (!dup && n < TR_MAXMODEL) list[n++] = m;
  }
  return n ? list[fnv1a(name) % (uint32_t)n] : -1;
}

// стоящие машины (Sonnet-PS2, ps2-parked-cars-count-l1.yaml): на L1 у оригинала стоят ровно 2 машины из 93 мест CarStart флага 1 (16 в 150 м от дома):
// это модели пула с флагом стоянки (leveli.mfk:195-199, glastruc и schoolbu). Порт занимал ВСЕ места данных (L1 10) — в 5 раз больше. Теперь машин
// столько, сколько моделей пула с флагом стоянки (но не больше мест): места по порядку FNV-1a имени и номера, сначала по одному на модель
// (чтобы были обе), остальные места пустые (машина убрана далеко, gone=1). TRAFFIC_PARK_FEW_OLD=1 — прежнее, заняты все
#ifndef TRAFFIC_PARK_FEW_OLD
#define TRAFFIC_PARK_FEW_OLD 0
#endif
#define PARK_CHOOSE_MAX 128
static void parkChoose(Traffic *t) {
  int n = 0; for (uint32_t i = 0; i < t->hd->nSlot; i++) n += t->slot[i].park ? 1 : 0;
  int N = (TRAFFIC_PARK_FEW_OLD || n <= 0) ? t->npark : (n < t->npark ? n : t->npark);
  uint32_t key[PARK_CHOOSE_MAX]; int ord[PARK_CHOOSE_MAX], np = t->npark < PARK_CHOOSE_MAX ? t->npark : PARK_CHOOSE_MAX;
  for (int i = 0; i < np; i++) { char nm[16]; snprintf(nm, sizeof nm, "%s#%d", t->parkd[i].name, i); key[i] = fnv1a(nm); ord[i] = i; }
  for (int i = 1; i < np; i++) { int v = ord[i], j = i - 1; while (j >= 0 && key[ord[j]] > key[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
  int pick[PARK_CHOOSE_MAX] = {0}, got = 0, seen[TR_MAXMODEL] = {0};
  for (int k = 0; k < np && got < N; k++) { int i = ord[k], m = t->parkd[i].model; if (m < TR_MAXMODEL && seen[m]) continue; if (m < TR_MAXMODEL) seen[m] = 1; pick[i] = 1; got++; }
  for (int k = 0; k < np && got < N; k++) { int i = ord[k]; if (!pick[i]) { pick[i] = 1; got++; } }
  for (int i = 0; i < t->npark; i++) t->park[i].off = i < np ? !pick[i] : 0;
}
static int pedModel(Traffic *t, int i) {   // CHR1 внутри блоба пешеходов: всё, что прочтёт GE, — внутри блоба
  const TrHeader *H = t->hd; const unsigned char *d = t->base + H->pedOff;
  uint32_t off = t->pedm[i].hdrOff, sz = H->pedBytes;
  if (off > sz || sz - off < sizeof(SkinHeader)) return -1;
  const SkinHeader *h = (const SkinHeader *)(d + off);
#define IN(o, l) ((uint32_t)(o) <= sz && (uint32_t)(l) <= sz - (uint32_t)(o))
  if (memcmp(h->magic, "CHR1", 4) || h->version != 1 || h->size != sz || h->nJoints > SKIN_MAXJ || !h->nClips ||
      !IN(h->offJoints, h->nJoints * 128) || !IN(h->offBatches, h->nBatches * 48) || !IN(h->offTex, h->nTex * 48) ||
      !IN(h->offClips, h->nClips * 80) || !IN(h->offBlink, h->nBlink * 8)) return -2;
  const SkinBatch *b = (const SkinBatch *)(d + h->offBatches);
  for (uint32_t k = 0; k < h->nBatches; k++) {
    if (b[k].nb < 1 || b[k].nb > 8 || b[k].tex >= h->nTex || !IN(b[k].offVerts, b[k].nverts * b[k].stride)) return -3;
    for (int j = 0; j < b[k].nb; j++) if (b[k].bones[j] >= h->nJoints) return -3;
  }
  const SkinClip *c = (const SkinClip *)(d + h->offClips);
  for (uint32_t k = 0; k < h->nClips; k++) if (!IN(c[k].offTracks, h->nJoints * 16)) return -4;
  const SkinTex *x = (const SkinTex *)(d + h->offTex);
  for (uint32_t k = 0; k < h->nTex; k++) if (!IN(x[k].offClut, 1024) || !IN(x[k].offPix, (uint32_t)x[k].w * x[k].h)) return -5;
#undef IN
  SkinModel *m = &t->peds[i];
  m->base = d; m->h = h; m->j = (const SkinJoint *)(d + h->offJoints); m->b = b; m->tex = x; m->c = c;
  m->blink = (const SkinBlinkKey *)(d + h->offBlink);
  skinSlotOpt(m);   // №914
  static const char *cn[TRC_N] = {"npd_loco_walk", "npd_loco_run", "npd_loco_idle_rest", "npd_flail", "npd_get_up", "npd_dodge",
                                  "npd_cringe", "npd_fall_land"};
  for (int k = 0; k < TRC_N; k++) t->clip[i][k] = (signed char)skinFindClip(m, cn[k]);
  if (t->clip[i][TRC_WALK] < 0 || t->clip[i][TRC_IDLE] < 0) return -6;
  float v = skinClipSpeed(m, t->clip[i][TRC_WALK]);
  t->walkV[i] = v > .3f && v < 4.f ? v : PED_WALK_DEF;
  return 0;
}

int trafficInitMem(Traffic *t, void *data, unsigned size, int own) {
  memset(t, 0, sizeof *t);
  t->missionPedGroup = -1;
  t->base = data; t->size = size; t->own = own;
  const TrHeader *h = (const TrHeader *)data; t->hd = h;
  if (size < sizeof(TrHeader) || memcmp(h->magic, "TRF1", 4) || h->version != 1 || h->size > size) return -2;
  if (!inside(t, h->offJunc, h->nJunc * 32) || !inside(t, h->offRoad, h->nRoad * 32) || !inside(t, h->offPt, h->nPt * 16) ||
      !inside(t, h->offOut, h->nOut * 2) || !inside(t, h->offModel, h->nModel * 128) || !inside(t, h->offPiece, h->nPiece * 80) ||
      !inside(t, h->vertOff, h->vertBytes) || !inside(t, h->offTex, h->texBytes) || !inside(t, h->offSlot, h->nSlot * 4) ||
      !inside(t, h->offPath, h->nPath * 32) || !inside(t, h->offPathPt, h->nPathPt * 12) || !inside(t, h->offZone, h->nZone * 64) ||
      !inside(t, h->offGroup, h->nGroup * 16) || !inside(t, h->offPed, h->nPed * 32) || !inside(t, h->pedOff, h->pedBytes) ||
      !inside(t, h->offPark, h->nPark * 32)) return -3;
  if (!h->nModel || h->nModel > TR_MAXMODEL || h->nPed > TR_MAXPEDMODEL || h->huskModel >= h->nModel) return -4;
  const unsigned char *b = t->base;
  t->junc = (const TrJunc *)(b + h->offJunc); t->road = (const TrRoad *)(b + h->offRoad); t->pt = (const TrPt *)(b + h->offPt);
  t->out = (const uint16_t *)(b + h->offOut); t->model = (const TrModel *)(b + h->offModel); t->piece = (const TrPiece *)(b + h->offPiece);
  t->vert = b + h->vertOff; t->slot = (const TrSlot *)(b + h->offSlot); t->path = (const TrPath *)(b + h->offPath);
  t->pathPt = (const float *)(b + h->offPathPt); t->zone = (const TrZone *)(b + h->offZone); t->grp = (const TrGroup *)(b + h->offGroup);
  t->pedm = (const TrPedModel *)(b + h->offPed); t->parkd = (const TrPark *)(b + h->offPark);
  t->loader = 0; t->nLoader = 0; t->zoneMask = ~0u;   // правка 139: старый файл без секции — правило зон молчит
  if (h->nLoader && inside(t, h->offLoader, h->nLoader * 64)) { t->loader = (const TrZone *)(b + h->offLoader); t->nLoader = (int)h->nLoader; }
  const TrGroup *grp = t->grp;
  // дороги, полосы, перекрёстки
  for (uint32_t i = 0; i < h->nJunc; i++) if ((uint32_t)t->junc[i].firstOut + t->junc[i].nOut > h->nOut) return -5;
  for (uint32_t i = 0; i < h->nOut; i++) if (t->out[i] >= h->nRoad) return -5;
  for (uint32_t i = 0; i < h->nRoad; i++) {
    const TrRoad *r = &t->road[i];
    if (r->startJ >= h->nJunc || r->endJ >= h->nJunc || !r->lanes || r->nPt < 2 || r->firstPt + (uint32_t)r->lanes * r->nPt > h->nPt) return -5;
  }
  // модели машин и куски
  for (uint32_t i = 0; i < h->nPiece; i++) {
    const TrPiece *p = &t->piece[i]; int vs = (p->flags & 32) ? 12 : 16;
    if (p->tex >= (int)h->nTex || p->tex < -1 || p->ni < 0 || p->ni % 3 || !(p->flags & 16) ||
        p->off > h->vertBytes || (uint32_t)p->ni * vs > h->vertBytes - p->off || p->wheel > 4) return -6;
  }
  for (uint32_t i = 0; i < h->nModel; i++) {
    const TrModel *m = &t->model[i];
    if ((uint32_t)m->firstPiece + m->nPiece > h->nPiece || (uint32_t)m->firstLod + m->nLod > h->nPiece || !m->nPiece) return -6;
    CarParams *P = &t->params[i]; char nm[17]; int k = 0;
    for (; k < 16 && m->name[k]; k++) nm[k] = (m->name[k] >= 'A' && m->name[k] <= 'Z') ? (char)(m->name[k] + 32) : m->name[k];
    nm[k] = 0;
    carParamsFor(nm, P);   // ходовые из .con модели (minivana, glastruc, …); геометрия — из модели TRF1
    if (!TRF_SEAT_OLD && h->offSeat && inside(t, h->offSeat, h->nModel * 12)) {   // №666: сиденье водителя (сустав dl) модели — посадка угнанной машины по нему
      float dl[3]; memcpy(dl, b + h->offSeat + i * 12, 12);
      if (dl[1] > -8.f && dl[1] < 8.f && fabsf(dl[0]) < 8.f && fabsf(dl[2]) < 8.f) {
        // правило tools/psp_export.car_info (машина уровня и машины миссий): якорь посадки сдвинут от сиденья так же, как у эталона famil_v
        // (сиденье −0,4367; 0,1740, якорь −1,475; 0,57), точка у двери — не ближе к борту, чем у эталона (зазор 0,375 м от полуширины 1,10)
        P->seat[0] = dl[0]; P->seat[1] = dl[1]; P->seat[2] = dl[2]; P->seatData = 1;
        P->seatAx = dl[0] + (-1.475f + 0.436739266f); P->seatAz = dl[2] + (0.57f - 0.173990607f);
        P->doorX = fminf(P->seatAx, -(m->halfW + 0.375f)); P->doorZ = P->seatAz; } }
    P->halfW = m->halfW; P->halfL = m->halfL; P->bodyOfs = m->bodyOfs; P->wheelX = m->wheelX; P->wheelF = m->wheelF;
    P->wheelR = m->wheelR; P->wheelRad = m->wheelRad; P->bodyLo = m->bodyLo; P->bodyHi = m->bodyHi;
    P->bandLo = fminf(.35f, m->bodyLo + .1f); P->bandHi = fmaxf(P->bandLo + .5f, m->bodyHi - .4f);
  }
  for (uint32_t i = 0; i < h->nSlot; i++) if (t->slot[i].model >= h->nModel) return -6;
  // пути и группы пешеходов, модели
  for (uint32_t i = 0; i < h->nPath; i++) if (t->path[i].n < 2 || t->path[i].first + t->path[i].n > h->nPathPt) return -7;
  for (uint32_t i = 0; i < h->nGroup; i++) {
    if (grp[i].n > 4) return -7;
    for (int k = 0; k < grp[i].n; k++) if (grp[i].model[k] >= h->nPed) return -7;
  }
  for (uint32_t i = 0; i < h->nPed; i++) { int r = pedModel(t, (int)i); if (r) return -20 + r; }
  t->npeds = (int)h->nPed;
  for (uint32_t i = 0; i < h->nPark; i++) if (t->parkd[i].model >= h->nModel) return -8;
  t->npark = (int)h->nPark;
  if (t->npark && !(t->park = calloc((size_t)t->npark, sizeof(TrParked)))) return -9;
  parkChoose(t);
  t->maxCars = h->nSlot < TR_MAXCAR ? (int)h->nSlot : TR_MAXCAR;
  t->maxPeds = h->nPed && h->nPath ? TR_MAXPED : 0;
  t->drawDist = 90.f; t->pedDrawDist = 60.f; t->seed = 0x2545F491u;
#ifndef SKIN_HOST
  sceKernelDcacheWritebackRange(data, size);   // GE читает вершины, палитры и тексели из ОЗУ
#endif
  return 0;
}

#include "pfetch.h"
__attribute__((weak)) void *(*pfTakeHook)(const char *path, unsigned *size);   // pfetch.h; слабый — хост-гейты линкуются без движка
__attribute__((weak)) void *(*trAllocHook)(unsigned n); __attribute__((weak)) void (*trFreeHook)(void *p);   // №962: буфер traffic.shr (1,3 МБ) — в запас участка lva (main_levels.h); нет хука — куча, как раньше
#define TR_FREE(p) do { if (trFreeHook) trFreeHook(p); else free(p); } while (0)
// №875 (Роб 23.10: «NPC-машины встают прямо на пешеходном переходе»): трафик тормозит передним бампером у конца полосы (stopLine), а плитка с зеброй
// и надписью STOP лежит перед концом полосы (L1: tex 106, задний край в 2–2,7 м). Бот на Маке (tools/scout/zebra.py) пишет рядом с traffic.shr
// stop875.bin: 'STP1', n, {u16 дорога, u16 полоса, f32 отступ м}. Машина тормозит на линии, отодвинутой за задний край плитки (brakeLine); уже
// проехала её, а перекрёсток занят — стоп у прежней линии, как было. «В перекрёстке» (inJunction) — по прежней линии. Нет файла — как было. TRF_STOP875_OLD=1 — файл не читается
#ifndef TRF_STOP875_OLD
#define TRF_STOP875_OLD 0
#endif
static float *trStopOff; static unsigned trStopRoads, trStopN;
static void trStopLoad(const char *path, unsigned nRoad) {
  free(trStopOff); trStopOff = 0; trStopRoads = 0; trStopN = 0;
  if (TRF_STOP875_OLD || !path) return;
  char fn[256]; snprintf(fn, sizeof fn, "%s", path); char *sl = strrchr(fn, '/'); snprintf(sl ? sl + 1 : fn, sizeof fn - (size_t)(sl ? sl + 1 - fn : 0), "stop875.bin");
#ifdef SKIN_HOST
  FILE *f = fopen(fn, "rb"); if (!f) return; unsigned char h[8]; if (fread(h, 1, 8, f) != 8 || memcmp(h, "STP1", 4)) { fclose(f); return; }
#else
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return; unsigned char h[8]; if (sceIoRead(f, h, 8) != 8 || memcmp(h, "STP1", 4)) { sceIoClose(f); return; }
#endif
  unsigned n; memcpy(&n, h + 4, 4);
  trStopOff = calloc((size_t)nRoad * 4u, sizeof *trStopOff); trStopRoads = nRoad;
  for (unsigned i = 0; trStopOff && i < n; i++) { unsigned char e[8];
#ifdef SKIN_HOST
    if (fread(e, 1, 8, f) != 8) break;
#else
    if (sceIoRead(f, e, 8) != 8) break;
#endif
    uint16_t rd, ln; float off; memcpy(&rd, e, 2); memcpy(&ln, e + 2, 2); memcpy(&off, e + 4, 4);
    if (rd < nRoad && ln < 4 && off > 0.f && off <= 8.f) { trStopOff[rd * 4u + ln] = off; trStopN++; } }
#ifdef SKIN_HOST
  fclose(f);
#else
  sceIoClose(f);
#endif
}
int trafficStopCount(void) { return (int)trStopN; }
int trafficLoad(Traffic *t, const char *path) {
  unsigned char *d = 0; unsigned size = 0;
#ifdef SKIN_HOST
  FILE *f = fopen(path, "rb"); if (!f) return -1;
  fseek(f, 0, SEEK_END); size = (unsigned)ftell(f); fseek(f, 0, SEEK_SET);
  d = aligned_alloc(64, (size + 63) & ~63u);
  if (!d || fread(d, 1, size, f) != size) { fclose(f); free(d); return -1; }
  fclose(f);
#else
  d = pfTakeHook ? (unsigned char *)pfTakeHook(path, &size) : 0;   // файл уже прочитан загрузчиком в фоне
  if (!d) {
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0); if (fd < 0) return -1;
  size = (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET);
  d = trAllocHook ? (unsigned char *)trAllocHook(size) : (unsigned char *)memalign(64, size);
  if (!d || sceIoRead(fd, d, size) != (int)size) { sceIoClose(fd); TR_FREE(d); return -1; }
  sceIoClose(fd); }
#endif
  int r = trafficInitMem(t, d, size, 1);
  if (r) { TR_FREE(d); t->base = 0; t->own = 0; free(t->park); t->park = 0; }
  else trStopLoad(path, t->hd->nRoad);   // №875: стоп-линии до зебр (бот tools/scout/zebra.py)
  return r;
}
static void trFarWheelsFree(void);
void trafficFree(Traffic *t) {
  free(t->park); t->park = 0; t->npark = 0; trFarWheelsFree();   // №877: сетки дальних колёс — по моделям этого уровня
  if (t->own) TR_FREE(t->base);
  t->base = 0; t->hd = 0;
}

// ---- текстуры: записи подряд, как в level.shr (заголовок 16 Б, CLUT 1024 Б, уровни по 16 Б)
int trafficTexCount(const Traffic *t) { return t->hd ? (int)t->hd->nTex : 0; }
void trafficTexInfo(const Traffic *t, int i, int *w, int *h, int *levels, int *pre, void **pal, void *lv[8]) {
  const unsigned char *p = t->base + t->hd->offTex, *end = p + t->hd->texBytes;
  for (int k = 0; p + 16 + 1024 <= end; k++) {
    const uint16_t *hh = (const uint16_t *)p; int tw = hh[0], th = hh[1], nl = hh[2], sw = p[7] == 1;
    const unsigned char *q = p + 16 + 1024; void *l[8] = {0};
    int lw = tw, lh = th;
    for (int j = 0; j < nl && j < 8; j++) {
      l[j] = (void *)q;
      q += (size_t)(sw && lw < 16 ? 16 : lw) * (sw ? (lh + 7) & ~7 : lh); q = (const unsigned char *)(((uintptr_t)q + 15) & ~(uintptr_t)15);
      lw = lw > 1 ? lw >> 1 : 1; lh = lh > 1 ? lh >> 1 : 1;
    }
    if (k == i) { *w = tw; *h = th; *levels = nl > 8 ? 8 : nl; *pre = sw; *pal = (void *)(p + 16); memcpy(lv, l, sizeof l); return; }
    p = (const unsigned char *)(((uintptr_t)q + 15) & ~(uintptr_t)15);
  }
  *w = *h = *levels = 0; *pre = 0; *pal = 0; memset(lv, 0, 8 * sizeof(void *));
}

// ================================================================ дороги
static const TrPt *lanePts(const Traffic *t, int road, int lane) { const TrRoad *r = &t->road[road]; return t->pt + r->firstPt + (uint32_t)lane * r->nPt; }
static float laneLen(const Traffic *t, int road, int lane) { return lanePts(t, road, lane)[t->road[road].nPt - 1].s; }
static int roadOk(const Traffic *t, int r) { return !(t->road[r].flags & 3); }
static void lanePos(const Traffic *t, int road, int lane, float s, float p[3]) {
  const TrPt *P = lanePts(t, road, lane); int n = t->road[road].nPt;
  if (s <= 0.f) { p[0] = P[0].x; p[1] = P[0].y; p[2] = P[0].z; return; }
  for (int k = 0; k + 1 < n; k++) {
    if (s > P[k + 1].s && k + 2 < n) continue;
    float L = P[k + 1].s - P[k].s, u = L > 1e-4f ? (s - P[k].s) / L : 0.f;   // за концом — продолжение последнего отрезка
    p[0] = P[k].x + (P[k + 1].x - P[k].x) * u; p[1] = P[k].y + (P[k + 1].y - P[k].y) * u; p[2] = P[k].z + (P[k + 1].z - P[k].z) * u;
    return;
  }
}
static void laneDir(const Traffic *t, int road, int lane, int end, float d[3]) {   // направление первого/последнего отрезка
  const TrPt *P = lanePts(t, road, lane); int n = t->road[road].nPt, a = end ? n - 2 : 0;
  d[0] = P[a + 1].x - P[a].x; d[1] = P[a + 1].y - P[a].y; d[2] = P[a + 1].z - P[a].z;
  float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); if (l < 1e-5f) l = 1.f;
  d[0] /= l; d[1] /= l; d[2] /= l;
}
static void bzPoint(const float bz[4][3], float u, float p[3]) {
  float a = (1 - u) * (1 - u) * (1 - u), b = 3 * u * (1 - u) * (1 - u), c = 3 * u * u * (1 - u), d = u * u * u;
  for (int k = 0; k < 3; k++) p[k] = a * bz[0][k] + b * bz[1][k] + c * bz[2][k] + d * bz[3][k];
}
static void bzAt(const TrCar *c, float s, float p[3]) {
  if (s <= 0.f) { bzPoint(c->bz, 0.f, p); return; }
  int k = 0; while (k < 7 && c->bzTab[k + 1] < s) k++;
  float L = c->bzTab[k + 1] - c->bzTab[k], u = (k + (L > 1e-5f ? clampf((s - c->bzTab[k]) / L, 0.f, 1.f) : 0.f)) / 8.f;
  bzPoint(c->bz, u, p);
}
// выбрать выезд с перекрёстка в конце дороги и кривую до ближней полосы; 0 — тупик
static int chooseNext(Traffic *t, TrCar *c) {
  const TrRoad *r = &t->road[c->road]; const TrJunc *j = &t->junc[r->endJ];
  int cand[16], n = 0, rev = -1;
  for (int k = 0; k < j->nOut && n < 16; k++) {
    int o = t->out[j->firstOut + k]; if (!roadOk(t, o)) continue;
    if (o == (int)r->rev || t->road[o].endJ == r->startJ) { rev = o; continue; }   // разворот — только если больше некуда
    cand[n++] = o;
  }
  if (!n && rev >= 0) cand[n++] = rev;
  if (!n) return 0;
  int o = cand[(int)(rnd(t) * n) % n];
  float p0[3], d0[3], best = 1e30f; int bl = 0;
  lanePos(t, c->road, c->lane, laneLen(t, c->road, c->lane), p0); laneDir(t, c->road, c->lane, 1, d0);
  for (int l = 0; l < t->road[o].lanes; l++) {
    const TrPt *P = lanePts(t, o, l); float dx = P[0].x - p0[0], dz = P[0].z - p0[2], d2 = dx * dx + dz * dz;
    if (d2 < best) { best = d2; bl = l; }
  }
  const TrPt *P = lanePts(t, o, bl); float d1[3]; laneDir(t, o, bl, 0, d1);
  float p3[3] = {P[0].x, P[0].y, P[0].z}, dx = p3[0] - p0[0], dy = p3[1] - p0[1], dz = p3[2] - p0[2];
  float d = sqrtf(dx * dx + dy * dy + dz * dz), hd = d * .4f;
  for (int k = 0; k < 3; k++) { c->bz[0][k] = p0[k]; c->bz[1][k] = p0[k] + d0[k] * hd; c->bz[2][k] = p3[k] - d1[k] * hd; c->bz[3][k] = p3[k]; }
  float prev[3], cur[3]; bzPoint(c->bz, 0.f, prev); c->bzTab[0] = 0.f;
  for (int k = 1; k <= 8; k++) {
    bzPoint(c->bz, k / 8.f, cur);
    float ex = cur[0] - prev[0], ey = cur[1] - prev[1], ez = cur[2] - prev[2];
    c->bzTab[k] = c->bzTab[k - 1] + sqrtf(ex * ex + ey * ey + ez * ez); memcpy(prev, cur, sizeof prev);
  }
  c->bzLen = c->bzTab[8]; c->nroad = o; c->nlane = bl; c->next = 1; c->junc = r->endJ;
  return 1;
}
// точка маршрута машины на ds метров вперёд от s (полоса -> кривая -> следующая полоса)
static void routeAt(const Traffic *t, const TrCar *c, float ds, float p[3]) {
  float s = c->s + ds;
  if (c->elem == 0) {
    float L = laneLen(t, c->road, c->lane);
    if (s <= L || !c->next) { lanePos(t, c->road, c->lane, s, p); return; }
    s -= L;
  }
  if (s <= c->bzLen || c->bzLen <= 0.f) { bzAt(c, s, p); return; }
  lanePos(t, c->nroad, c->nlane, s - c->bzLen, p);
}

void trafficRoutePoint(const Traffic *t, const TrCar *c, float ds, float p[3]) { routeAt(t, c, ds, p); }   // служебное: tests/traffic/turns.c

// ================================================================ машины
static void yawPitchQ(float q[4], float h, float pitch) {   // тело -> мир: курс h, нос вверх pitch (как car.c quatYaw + наклон)
  float cy = cosf(h * .5f), sy = sinf(h * .5f), cp = cosf(-pitch * .5f), sp = sinf(-pitch * .5f);
  q[0] = cy * cp; q[1] = cy * sp; q[2] = sy * cp; q[3] = -sy * sp;
}
// кинематика по полосе -> состояние твёрдого тела (для carHitCar и отрисовки)
static void syncKinematic(TrCar *c, const float p[3], float h, float pitch, float v, float yawRate) {
  Car *k = &c->car;
  k->x = p[0]; k->y = p[1]; k->z = p[2]; k->h = h; k->pitch = pitch; k->roll = 0.f;
  yawPitchQ(k->q, h, pitch);
  float fx = sinf(h) * cosf(pitch), fy = sinf(pitch), fz = cosf(h) * cosf(pitch), tp = tanf(pitch);
  k->vel[0] = fx * v; k->vel[1] = fy * v; k->vel[2] = fz * v; k->w[0] = 0.f; k->w[1] = yawRate; k->w[2] = 0.f;
  k->v = v; k->vl = 0.f; k->vy = fy * v; k->gx = sinf(h) * tp; k->gz = cosf(h) * tp;
  k->ry = k->y; k->grounded = 1; k->wheels = 4;
}
static int inView(const float eye[3], const float fwd[3], float x, float y, float z, float r, float dist) {
  float dx = x - eye[0], dy = y - eye[1], dz = z - eye[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (d - r > dist) return 0;
  if (d < r + 2.f) return 1;
  return (dx * fwd[0] + dy * fwd[1] + dz * fwd[2]) / d > .35f - r / d;   // шире кадра (fov 62°, 16:9): с запасом
}
static float carR(const Car *k) { return carSphereR(k); }
// Правка 139: место стоящей машины живо, пока загружен файл его зоны (TRAFFIC_ZONE_PARK, traffic.h). Выключено — всегда живо
static int parkLive(const Traffic *t, int i) {
#if TRAFFIC_ZONE_PARK
  return !t->park[i].gone;
#else
  (void)t; (void)i; return 1;
#endif
}
#if TRAFFIC_ZONE_PARK
// Игрок в воротах уличного загрузчика -> набор загруженных файлов = его маска (уличный загрузчик называет все файлы зон,
// tools/traffic_export.py dyna_loaders). Место меняет состояние (пропасть / вернуться) только спящей машиной и только вне
// кадра — ни на месте, ни там, где машина стоит сейчас (inView с дальностью рисования): на глазах ничего не исчезает и не
// появляется. Пропадая, сдвинутая машина возвращается на своё место (как PARK_BACK)
static void parkZones(Traffic *t, const ColWorld *w, const float ppos[3], const float eye[3], const float fwd[3]) {
  for (int i = 0; i < t->nLoader; i++) {
    const TrZone *Z = &t->loader[i]; float d[3] = {ppos[0] - Z->c[0], ppos[1] - Z->c[1], ppos[2] - Z->c[2]};
    if (fabsf(d[0] * Z->ax[0] + d[1] * Z->ax[1] + d[2] * Z->ax[2]) < Z->h[0] && fabsf(d[1]) < Z->h[1] + 2.f &&
        fabsf(d[0] * Z->az[0] + d[1] * Z->az[1] + d[2] * Z->az[2]) < Z->h[2]) { t->zoneMask = Z->group; break; }
  }
  for (int i = 0; i < t->npark; i++) {
    const TrPark *k = &t->parkd[i]; TrParked *P = &t->park[i]; if (P->off) continue;
    int want = !k->zmask || (k->zmask & t->zoneMask);
    if (want == !P->gone || P->state) continue;
#ifndef TRZ_PLANT_NOVIEW   // красный контроль гейта tests/parkzone: без проверки кадра
    float R = carR(&P->car);
    if (inView(eye, fwd, P->car.x, P->car.y + 1.f, P->car.z, R, t->drawDist) || inView(eye, fwd, k->x, k->y + 1.f, k->z, R, t->drawDist)) continue;
#else
    (void)eye; (void)fwd;
#endif
    if (!want) { carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h); P->wreck = 0; P->stillT = 0.f; }
    P->gone = !want;
  }
}
#endif
static void bodyCenter(const Car *k, float *x, float *z) { *x = k->x + sinf(k->h) * k->p.bodyOfs; *z = k->z + cosf(k->h) * k->p.bodyOfs; }

// ближайшее препятствие впереди по ходу (машины, стоящие, игрок, Гомер пешком, пешеходы): зазор, м
// препятствие: круг (Гомер, пешеход) или кузов-прямоугольник (машины: торчащий в полосу зад стоящей — тоже препятствие)
typedef struct { float x, z, r, fx, fz, hw, hl; int own, box; } Obs;   // own — номер машины трафика (-1 прочие): себя не видит
static void obsCar(Obs *o, const Car *k, int own) {
  CarBox b; carBox(k, &b); o->x = b.x; o->z = b.z; o->fx = b.fx; o->fz = b.fz; o->hw = b.hw; o->hl = b.hl; o->r = 0.f; o->own = own; o->box = 1;
}
static void obsCircle(Obs *o, float x, float z, float r) { o->x = x; o->z = z; o->r = r; o->own = -1; o->box = 0; }
// зазор до препятствия впереди машины k (центр кузова bx, bz, курс fx, fz); 1e9 — не впереди и не в полосе
static float gapOne(const Car *k, float bx, float bz, float fx, float fz, const Obs *o) {
  float dx = o->x - bx, dz = o->z - bz, a = dx * fx + dz * fz, l = -dx * fz + dz * fx, lane = k->p.halfW + .4f;
  if (!o->box) {
    if (a + o->r <= 0.f || a - o->r > 40.f || fabsf(l) > lane + o->r) return 1e9f;
    return a - o->r - k->p.halfL;
  }
  float ua = o->fx * fx + o->fz * fz, ul = -o->fx * fz + o->fz * fx;          // ось препятствия в осях машины
  float ea = o->hl * fabsf(ua) + o->hw * fabsf(ul), el = o->hl * fabsf(ul) + o->hw * fabsf(ua);   // полуразмеры проекций
  if (a + ea <= 0.f || a - ea > 40.f || fabsf(l) - el > lane) return 1e9f;
  return a - ea - k->p.halfL;
}
static float gapAhead(const Traffic *t, const TrCar *c, const Obs *o, int n) {
  const Car *k = &c->car; float fx = sinf(k->h), fz = cosf(k->h), bx, bz, gap = 1e9f;
  int self = (int)(c - t->car);
  bodyCenter(k, &bx, &bz);
  for (int i = 0; i < n; i++) if (o[i].own != self) gap = fminf(gap, gapOne(k, bx, bz, fx, fz, &o[i]));
  for (int i = 0; i < t->npark; i++) {   // стоящие (в том числе сдвинутые на дорогу)
    if (!parkLive(t, i)) continue;
    const Car *p = &t->park[i].car; float dx = p->x - bx, dz = p->z - bz;
    if (dx * dx + dz * dz > 45.f * 45.f) continue;
    Obs q; obsCar(&q, p, -1); gap = fminf(gap, gapOne(k, bx, bz, fx, fz, &q));
  }
  return gap;
}
// стоп-линия: передний бампер у края перекрёстка (начало машины — между колёсами, перед торчит на halfL + bodyOfs)
static float stopLine(const Traffic *t, const TrCar *c) { return laneLen(t, c->road, c->lane) - (c->car.p.halfL + c->car.p.bodyOfs) - .3f; }
static float brakeLine(const Traffic *t, const TrCar *c) {   // №875: линия торможения — за зеброй (stop875.bin), иначе stopLine
  float sl = stopLine(t, c);
  if (trStopOff && (unsigned)c->road < trStopRoads && (unsigned)c->lane < 4u) sl -= trStopOff[(unsigned)c->road * 4u + (unsigned)c->lane];
  return sl;
}
// машина в перекрёстке: на кривой или передом за стоп-линией
static int inJunc(const Traffic *t, const TrCar *c, int j) {
  if (c->mode != TRM_DRIVE || c->junc != j) return 0;
  return c->elem == 1 || (c->next > 0 && c->s > stopLine(t, c));
}
static int juncBusy(const Traffic *t, const TrCar *self, int j) {
  for (int i = 0; i < t->maxCars; i++) { const TrCar *c = &t->car[i]; if (c != self && inJunc(t, c, j)) return 1; }
  return 0;
}
// путь впереди: круг кузова (полуширина) в точках маршрута перед бампером через 1 м до 12 м; расстояние до первой, что
// задевает препятствие (в повороте и на перекрёстке прямой взгляд вперёд промахивается)
static float obsDist(const Obs *o, float x, float z) {
  float dx = x - o->x, dz = z - o->z;
  if (!o->box) return sqrtf(dx * dx + dz * dz) - o->r;
  float a = fabsf(dx * o->fx + dz * o->fz) - o->hl, l = fabsf(-dx * o->fz + dz * o->fx) - o->hw;
  return (a > 0.f && l > 0.f) ? sqrtf(a * a + l * l) : fmaxf(a, l);
}
// №1001б (цель: минус 1 мс PSP в езде L3–L7, поведение побитно то же): на PSP/в эмуляторе pathGap — две трети цены езды машины трафика (12 вызовов routeAt и обход всех препятствий
// на каждую точку), а препятствий на пути впереди почти всегда нет. Точки пути лежат не дальше front + 12 м (дуга) от начала машины: препятствие, чей круг/габарит дальше
// front + 12 + r + габарит + запас 3 м, задеть не может ни в какой точке — такие отбрасываются заранее (без них результат тот же), нет ни одного — 1e9 без единого routeAt.
// TRFCOST1001B_OLD=1 / файл trfcost1001bold / flag trfcost1001b 0 — прежний обход
#ifndef TRFCOST1001B_OLD
#define TRFCOST1001B_OLD 0
#endif
int trfCost1001b = !TRFCOST1001B_OLD;
static float pathGapFast(const Traffic *t, const TrCar *c, const Obs *o, int n, const Obs *pk, int npk) {
  const Car *k = &c->car; int self = (int)(c - t->car); float front = k->p.halfL + k->p.bodyOfs, r = k->p.halfW + .2f;
  float p0[3]; routeAt(t, c, 0.f, p0);
#ifndef TRFCOST1001B_BREAK
#define TRFCOST1001B_BREAK 0   // отрицательный контроль гейта tests/trfcost1001b: 1 — отбор слишком тесный (поведение меняется, гейт обязан покраснеть)
#endif
  float reach = TRFCOST1001B_BREAK ? 1.f : front + 12.f + r + 3.f;
  const Obs *rel[TR_MAXPED + TR_MAXCAR + TR_MAXEXTRA + 2 + 24]; int nr = 0;
  for (int i = 0; i < n; i++) {
    if (o[i].own == self) continue;
    float hr = o[i].box ? sqrtf(o[i].hl * o[i].hl + o[i].hw * o[i].hw) : o[i].r, dx = o[i].x - p0[0], dz = o[i].z - p0[2], R = reach + hr;
    if (dx * dx + dz * dz <= R * R) rel[nr++] = &o[i];
  }
  for (int i = 0; i < npk; i++) {
    float hr = pk[i].box ? sqrtf(pk[i].hl * pk[i].hl + pk[i].hw * pk[i].hw) : pk[i].r, dx = pk[i].x - p0[0], dz = pk[i].z - p0[2], R = reach + hr;
    if (dx * dx + dz * dz <= R * R) rel[nr++] = &pk[i];
  }
  if (!nr) return 1e9f;
  for (float d = 1.f; d <= 12.f; d += 1.f) {
    float p[3]; routeAt(t, c, front + d, p);
    for (int i = 0; i < nr; i++) if (obsDist(rel[i], p[0], p[2]) < r) return d - 1.f;
  }
  return 1e9f;
}
static float pathGap(const Traffic *t, const TrCar *c, const Obs *o, int n, const Obs *pk, int npk) {
  if (trfCost1001b) return pathGapFast(t, c, o, n, pk, npk);
  const Car *k = &c->car; int self = (int)(c - t->car); float front = k->p.halfL + k->p.bodyOfs, r = k->p.halfW + .2f;
  for (float d = 1.f; d <= 12.f; d += 1.f) {
    float p[3]; routeAt(t, c, front + d, p);
    for (int i = 0; i < n; i++) if (o[i].own != self && obsDist(&o[i], p[0], p[2]) < r) return d - 1.f;
    for (int i = 0; i < npk; i++) if (obsDist(&pk[i], p[0], p[2]) < r) return d - 1.f;
  }
  return 1e9f;
}
// №1001б: разбивка trafficUpdate по частям (на PSP при включённой диагностике, строка TRFSUB): 0 — зоны и удаление, 1 — спавн машин, 2 — спавн пешеходов, 3 — препятствия,
// 4 — езда и физика машин, 5 — столкновения машин, 6 — стоящие машины, 7 — пешеходы
unsigned trfProfUs[8]; int trfProfOn; static unsigned trfProfT0;
#ifdef __PSP__
#define TP(i) do { if (trfProfOn) { unsigned t_ = sceKernelGetSystemTimeLow(); if ((i) > 0) trfProfUs[(i) - 1] += t_ - trfProfT0; trfProfT0 = t_; } } while (0)
#else
#define TP(i) do { } while (0)
#endif
static void driveStep(Traffic *t, const ColWorld *w, TrCar *c, const Obs *obs, int nobs, float dt) {
  const TrRoad *r = &t->road[c->road];
  float L = c->elem == 0 ? laneLen(t, c->road, c->lane) : c->bzLen;
  float decide = TRAFFIC_PACE_OLD || c->elem ? DECIDE : fmaxf(DECIDE, L - brakeLine(t, c) + c->v * c->v / (2.f * DECEL) + 1.f);
  if (c->elem == 0 && !c->next && L - c->s < decide && !chooseNext(t, c)) c->next = -1;   // тупик: стоп у конца, удалится вне кадра
  float vRoad = ROAD_V(t, c->road);
  float vt = vRoad;
  // поворот: изменение курса на 10 м вперёд -> радиус
  { float a[3], b[3], e[3]; routeAt(t, c, 0.f, a); routeAt(t, c, 5.f, b); routeAt(t, c, 10.f, e);
    float h1 = atan2f(b[0] - a[0], b[2] - a[2]), h2 = atan2f(e[0] - b[0], e[2] - b[2]), da = fabsf(wrapPi(h2 - h1));
    float lg = c->elem == 0 && L - c->s > 10.f ? LAT_G_FIT : LAT_G;   // изгиб полосы (записан) / кривая перекрёстка (не записана — прежнее)
    if (da > .02f) vt = fminf(vt, sqrtf(lg * 5.f / da)); }
  // перекрёсток: стоп передним бампером у края (behaviour 1), занятый — ждать
  float stopS = 1e9f;
  if (c->elem == 0 && c->next != 0) {
    float sl = brakeLine(t, c); if (c->s > sl + .05f) sl = stopLine(t, c);   // №875: проехал линию у зебры — стоп у прежней
    float toStop = sl - c->s;
    int jn = c->next > 0 ? c->junc : r->endJ, beh1 = t->junc[jn].behaviour == 1, busy = c->next > 0 && juncBusy(t, c, jn);
    int mayGo = c->next > 0 && (!busy || (TRAFFIC_JUNC_OLD && c->waitT > JAM_WAIT)) && (!beh1 || c->waitT >= STOP_WAIT);
    if (!mayGo && toStop > -.05f) {
      stopS = sl; vt = fminf(vt, sqrtf(2.f * DECEL * fmaxf(0.f, toStop - .2f)));
      if (toStop < 1.5f && c->v < .6f) c->waitT += dt;
    }
  }
  Obs pk[24]; int npk = 0;   // стоящие машины рядом — для пути впереди
  for (int i = 0; i < t->npark && npk < 24; i++) {
    if (!parkLive(t, i)) continue;
    const Car *p = &t->park[i].car; float dx = p->x - c->car.x, dz = p->z - c->car.z;
    if (dx * dx + dz * dz < 25.f * 25.f) obsCar(&pk[npk++], p, -1);
  }
  float gap = fminf(gapAhead(t, c, obs, nobs), pathGap(t, c, obs, nobs, pk, npk) + STOP_GAP - 1.f);
  if (gap < 1e8f) vt = fminf(vt, sqrtf(2.f * DECEL * fmaxf(0.f, gap - STOP_GAP)));
  c->vt = vt;
  c->car.brkLit = (vt < c->v - .3f || (c->v < .3f && vt < .1f)) ? 1.f : 0.f;   // №465: тормозит или ждёт у стоп-линии — стоп-сигналы горят
  float dv = clampf(vt - c->v, -2.f * DECEL * dt, ACCEL * dt); c->v = fmaxf(0.f, c->v + dv);
  if (c->elem == 0 && c->next <= 0 && c->s + c->v * dt > L) c->v = 0.f;   // тупик / выезд не выбран
  if (c->s < stopS && c->s + c->v * dt > stopS) { c->s = stopS; c->v = 0.f; }   // за стоп-линию без разрешения — нет
  c->s += c->v * dt;
  if (c->elem == 0 && c->next > 0 && c->s >= L) { c->s -= L; c->elem = 1; c->waitT = 0.f; }
  if (c->elem == 1 && c->s >= c->bzLen) { c->s -= c->bzLen; c->elem = 0; c->road = c->nroad; c->lane = c->nlane; c->next = 0; c->junc = -1; }
  float p[3], look[3]; routeAt(t, c, 0.f, p); routeAt(t, c, LOOK, look);
  float dx = look[0] - p[0], dz = look[2] - p[2], hl = sqrtf(dx * dx + dz * dz);
  float h = hl > .05f ? atan2f(dx, dz) : c->car.h, pitch = hl > .05f ? atanf((look[1] - p[1]) / hl) : 0.f;
#ifndef TRAFFIC_TURN_OLD
  // №400 (Роб 08.10: «поворачивают не по живому»): центр на кривой и нос на точку в 4 м заворачивали кузов раньше хода — задняя
  // ось ехала боком (снос p95 14–42° на семи уровнях, tests/traffic/turns.sh). Теперь как у колёсной машины: начало машины (между
  // осями) идёт по полосе, задняя ось тянется за ним на wheelR без бокового сноса, курс — от задней оси к началу; в повороте нос
  // выходит наружу, хвост внутрь, на прямой машина ровно в полосе. Правило порта (как ведёт трафик оригинал, в данных нет).
  // Прежнее: -DTRAFFIC_TURN_OLD
  { float wr = fmaxf(.5f, c->car.p.wheelR), qx = p[0] - c->rearX, qz = p[2] - c->rearZ;
    if (!c->rearOk || qx * qx + qz * qz > 9.f * wr * wr) {   // новая машина / вернулась в полосу — задняя ось на маршруте
      float ra[3]; routeAt(t, c, -wr, ra); c->rearX = ra[0]; c->rearZ = ra[2]; c->rearOk = 1; qx = p[0] - ra[0]; qz = p[2] - ra[2]; }
    float ql = sqrtf(qx * qx + qz * qz);
    if (ql > .05f) { qx /= ql; qz /= ql; c->rearX = p[0] - qx * wr; c->rearZ = p[2] - qz * wr; h = atan2f(qx, qz); } }
#endif
  float g; if (colGround(w, p[0], p[2], p[1] + 1.2f, p[1] - 2.5f, &g, 0)) p[1] = g;   // колёса на поверхность коллизии
  float yawRate = dt > 0.f ? wrapPi(h - c->car.h) / dt : 0.f;
  syncKinematic(c, p, h, clampf(pitch, -.35f, .35f), c->v, yawRate);
  c->wheelAng += c->v / fmaxf(.2f, c->car.p.wheelRad) * dt;
}
// ближайшая к (x, z) точка полосы с курсом ±60° от h не дальше sqrt(maxd2): дорога, полоса, путь s; 0 — нет
static int nearestLane(const Traffic *t, float x, float z, float h, float maxd2, int *road, int *lane, float *sOut) {
  float best = maxd2; int br = -1, bl = 0; float bs = 0.f;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadOk(t, (int)r)) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int i = 0; i + 1 < n; i++) {
        float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
        float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f);
        float qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
        if (d2 >= best || fabsf(wrapPi(atan2f(ax, az) - h)) > 1.05f) continue;
        best = d2; br = (int)r; bl = l; bs = P[i].s + (P[i + 1].s - P[i].s) * u;
      }
    }
  }
  if (br < 0) return 0;
  *road = br; *lane = bl; *sOut = bs; return 1;
}
// правка Роба 117 (30.09: вызванная у будки машина «вдоль проезжей части»): ближайшая к (x, z) точка любой полосы не дальше maxd,
// без условия курса; p — точка на полосе, *h — курс по ходу полосы. 1 — нашлась
int trafficLaneSpot(const Traffic *t, float x, float z, float maxd, float p[3], float *h) {
  if (!t || !t->hd) return 0;
  float best = maxd * maxd; int br = -1, bl = 0; float bs = 0.f;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadOk(t, (int)r)) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int i = 0; i + 1 < n; i++) {
        float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
        float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f);
        float qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
        if (d2 >= best) continue;
        best = d2; br = (int)r; bl = l; bs = P[i].s + (P[i + 1].s - P[i].s) * u;
      }
    }
  }
  if (br < 0) return 0;
  float a[3], b[3], L = laneLen(t, br, bl), s0 = bs > L - LOOK ? L - LOOK : bs;   // курс — по отрезку полосы впереди точки
  lanePos(t, br, bl, bs, p); lanePos(t, br, bl, s0 < 0.f ? 0.f : s0, a); lanePos(t, br, bl, (s0 < 0.f ? 0.f : s0) + LOOK, b);
  *h = atan2f(b[0] - a[0], b[2] - a[2]);
  return 1;
}
// Правка 122 (сверка 30.09, запись PS2 2:13, 2:20, 4:03): зелёные стрелки навигации на дороге. Путь по дорогам трафика от игрока
// к цели: дорога игрока — ближайшая полоса не дальше 12 м с курсом в пределах 90°; цель — ближайшая к ней дорога не дальше 60 м;
// Дейкстра по перекрёсткам (дороги односторонние, срезки flags 1 не берём). Стрелка — на выезде ближайшего перекрёстка по пути, если
// до него не больше maxd: p — точка в 6 м от начала выезда посередине полос, *h — курс выезда. 0 — стрелки нет (уже на дороге цели,
// пути нет, игрок не на дороге)
static void roadMid(const Traffic *t, int r, float s, float p[3]) {   // точка посередине полос дороги r на пути s
  float a[3], b[3]; int L = t->road[r].lanes > 0 ? t->road[r].lanes : 1;
  lanePos(t, r, 0, s, a); lanePos(t, r, L - 1, s, b);
  p[0] = (a[0] + b[0]) * .5f; p[1] = (a[1] + b[1]) * .5f; p[2] = (a[2] + b[2]) * .5f;
}
#ifndef NEAR_ROAD849_OLD
#define NEAR_ROAD849_OLD 0   // №849: 1 — синус/косинус курса на каждом отрезке и без отсева далёких отрезков, как было
#endif
static int nearRoad(const Traffic *t, float x, float z, float maxd, int useH, float h, float *sOut) {
  float best = maxd * maxd; int br = -1; float bs = 0.f;
  const float sh_ = sinf(h), ch_ = cosf(h);   // №849: курс — один раз (было: sinf/cosf на каждом отрезке каждой дороги)
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (t->road[r].flags & 1) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int i = 0; i + 1 < n; i++) {
        if (!NEAR_ROAD849_OLD) { float x0 = P[i].x - x, x1 = P[i + 1].x - x, z0 = P[i].z - z, z1 = P[i + 1].z - z;   // отрезок целиком дальше maxd по одной оси — мимо
          if ((x0 > maxd && x1 > maxd) || (x0 < -maxd && x1 < -maxd) || (z0 > maxd && z1 > maxd) || (z0 < -maxd && z1 < -maxd)) continue; }
        float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
        if (useH && (NEAR_ROAD849_OLD ? ax * sinf(h) + az * cosf(h) : ax * sh_ + az * ch_) < 0.f) continue;   // против хода дороги
        float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f);
        float qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
        if (d2 < best) { best = d2; br = (int)r; bs = P[i].s + (P[i + 1].s - P[i].s) * u; }
      }
    }
  }
  if (sOut) *sOut = bs;
  return br;
}
#ifndef ROUTE_CACHE849_OLD
#define ROUTE_CACHE849_OLD 0
#endif
int trafficRouteArrow(const Traffic *t, float px, float pz, float ph, float tx, float tz, float maxd, float p[3], float *h) {
  if (!t || !t->hd || !t->hd->nJunc) return 0;
  float s0; int r0 = nearRoad(t, px, pz, 12.f, 1, ph, &s0), rg;
  // №849 (тур TEST39: L3 в езде расчёт пути стрелки 0,7–3,3 мс эмулятора раз в 0,25 с — на PSP рывок): дорога цели и расстояния от каждого
  // перекрёстка до цели (обратная Дейкстра) — один раз на цель; дальше первая дорога пути — лучший выезд из перекрёстка машины (длина + остаток
  // до цели), тот же кратчайший путь. ROUTE_CACHE849_OLD=1 — прямая Дейкстра на каждый вызов, как было
  enum { NJ = 256 }; unsigned nj = t->hd->nJunc < NJ ? t->hd->nJunc : NJ;
  if (!ROUTE_CACHE849_OLD) {
    static const Traffic *ct_; static float ctx_ = 1e30f, ctz_ = 1e30f, cdist_[NJ]; static int crg_ = -1;
    if (ct_ != t || ctx_ != tx || ctz_ != tz) {
      ct_ = t; ctx_ = tx; ctz_ = tz; crg_ = nearRoad(t, tx, tz, 60.f, 0, 0.f, 0);
      for (unsigned j = 0; j < nj; j++) cdist_[j] = 1e30f;
      if (crg_ >= 0 && t->road[crg_].startJ < nj) {
        unsigned char dn_[NJ]; memset(dn_, 0, sizeof dn_); cdist_[t->road[crg_].startJ] = 0.f;
        for (;;) {   // обратная Дейкстра: по дорогам, ведущим В перекрёсток u
          unsigned u = nj; float bd = 1e30f;
          for (unsigned j = 0; j < nj; j++) if (!dn_[j] && cdist_[j] < bd) { bd = cdist_[j]; u = j; }
          if (u == nj) break;
          dn_[u] = 1;
          for (unsigned o = 0; o < t->hd->nRoad; o++) { const TrRoad *R = &t->road[o];
            if ((R->flags & 1) || R->endJ != u || R->startJ >= nj) continue;
            float nd = cdist_[u] + R->length; if (nd < cdist_[R->startJ]) cdist_[R->startJ] = nd; }
        }
      }
    }
    rg = crg_;
    if (r0 < 0 || rg < 0 || r0 == rg) return 0;
    unsigned j0 = t->road[r0].endJ, jg = t->road[rg].startJ; if (j0 >= nj || jg >= nj) return 0;
    int first = -1; float bestc = 1e30f;
    if (jg == j0) first = rg;
    else { const TrJunc *J = &t->junc[j0];
      for (int k = 0; k < J->nOut; k++) { int o = t->out[J->firstOut + k]; const TrRoad *R = &t->road[o];
        if ((R->flags & 1) || R->endJ >= nj) continue;
        float c = R->length + cdist_[R->endJ]; if (c < bestc) { bestc = c; first = o; } }
      if (bestc >= 1e29f) return 0; }
    if (first < 0) return 0;
    if (t->road[first].endJ == t->road[r0].startJ && t->road[first].startJ == t->road[r0].endJ) return 0;   // разворот назад: стрелки нет, путь покажет радар
    if (laneLen(t, r0, 0) - s0 > maxd) return 0;   // перекрёсток ещё далеко
    float a[3], b[3]; roadMid(t, first, 6.f, p); roadMid(t, first, 0.f, a); roadMid(t, first, 12.f, b);
    *h = atan2f(b[0] - a[0], b[2] - a[2]);
    return 1;
  }
  rg = nearRoad(t, tx, tz, 60.f, 0, 0.f, 0);
  if (r0 < 0 || rg < 0 || r0 == rg) return 0;
  float dist[NJ]; short via[NJ]; unsigned char done[NJ];
  for (unsigned j = 0; j < nj; j++) { dist[j] = 1e30f; via[j] = -1; done[j] = 0; }
  unsigned j0 = t->road[r0].endJ, jg = t->road[rg].startJ; if (j0 >= nj || jg >= nj) return 0;
  dist[j0] = 0.f;
  for (;;) {   // Дейкстра без кучи: перекрёстков десятки (уровень 1 — 44)
    unsigned u = nj; float bd = 1e30f;
    for (unsigned j = 0; j < nj; j++) if (!done[j] && dist[j] < bd) { bd = dist[j]; u = j; }
    if (u == nj || u == jg) break;
    done[u] = 1;
    const TrJunc *J = &t->junc[u];
    for (int k = 0; k < J->nOut; k++) {
      int o = t->out[J->firstOut + k]; const TrRoad *R = &t->road[o];
      if ((R->flags & 1) || R->endJ >= nj) continue;
      float nd = dist[u] + R->length;
      if (nd < dist[R->endJ]) { dist[R->endJ] = nd; via[R->endJ] = (short)o; }
    }
  }
  if (jg != j0 && via[jg] < 0) return 0;
  int first = -1;   // первая дорога пути: от jg назад до выезда из j0
  if (jg == j0) first = rg;
  else for (unsigned j = jg, guard = 0; guard < nj; guard++) { int o = via[j]; if (o < 0) break; if (t->road[o].startJ == j0) { first = o; break; } j = t->road[o].startJ; }
  if (first < 0) return 0;
  if (t->road[first].endJ == t->road[r0].startJ && t->road[first].startJ == t->road[r0].endJ) return 0;   // разворот назад: стрелки нет, путь покажет радар
  if (laneLen(t, r0, 0) - s0 > maxd) return 0;   // перекрёсток ещё далеко
  float a[3], b[3]; roadMid(t, first, 6.f, p); roadMid(t, first, 0.f, a); roadMid(t, first, 12.f, b);
  *h = atan2f(b[0] - a[0], b[2] - a[2]);
  return 1;
}
#ifndef TRF_UPRIGHT_OLD
#define TRF_UPRIGHT_OLD 0   // аудит 09.10: 1 — на полосу через 1,5 с в любом положении (и на боку)
#endif
#ifndef TRF_REJOIN_OLD
#define TRF_REJOIN_OLD 0
#endif
// Правка Роба 479 (10.10, таран машины трафика в бок на ходу): СЛАБЫЙ удар — машина останавливается, сигналит и едет дальше по полосе;
// СИЛЬНЫЙ — отлетает, стоит «оглушённая» и потом пытается вернуться на свою полосу. Порога силы в данных диска нет (HitPoints — это
// прочность, не порог): правило порта — сближение по нормали удара (car.carHitV) от TRF_RAM_STRONG_V. Прежнее поведение (машина навсегда
// остаётся телом после удара, запись PS2 В112: минивэн стоит 13,8 с) — -DTRF_RAM_OLD=1. «Честный» разбитый остов в полосу не возвращается
#ifndef TRF_RAM_OLD
#define TRF_RAM_OLD 0
#endif
#ifndef TRF_RAM_STRONG_V
#define TRF_RAM_STRONG_V 6.f    // м/с сближения (~22 км/ч): выше — сильный
#endif
#ifndef TRF_RAM_WEAK_STOP
#define TRF_RAM_WEAK_STOP 1.f   // с неподвижности до гудка
#endif
#ifndef TRF_RAM_WEAK_GO
#define TRF_RAM_WEAK_GO 1.8f    // с неподвижности до выезда на полосу
#endif
#ifndef TRF_RAM_STUN
#define TRF_RAM_STUN 13.8f      // с «оглушения» после сильного удара: запись PS2 В112 (минивэн после тарана стоит 13,76 с), потом пытается вернуться
#endif
static void ramLatch(TrCar *c) {   // удар по машине трафика: сила и сброс таймеров №479
  float v = c->car.carHitV; if (v > c->hitV) c->hitV = v;
  c->ramStrong = c->hitV >= TRF_RAM_STRONG_V; c->stunT = 0.f; c->honkDone = 0;
}
// Historical fallback, retained only for genuine negative/rear-axis controls
static int backToLane(Traffic *t, TrCar *c) {
  int r, l; float s;
  if (!nearestLane(t, c->car.x, c->car.z, c->car.h, 16.f, &r, &l, &s)) return 0;
  c->mode = TRM_DRIVE; c->elem = 0; c->road = r; c->lane = l; c->s = s; c->next = 0; c->v = 0.f; c->waitT = 0.f;
#ifndef TRAFFIC_REJOIN_REAR_OLD
  // Регресс №400 (aicar 09.10): задняя ось (rearX/Z) оставалась от езды ДО удара. Машину отбросило назад на 1–3 wheelR — прежняя
  // ось оказывалась ВПЕРЕДИ новой точки полосы, ближе 3 wheelR (сброса нет), и курс «от оси к началу» разворачивался на 180°:
  // машина ехала по полосе задом наперёд со скоростью кузова против хода (поворот π за кадр, рывок до 90 рад/с); такие удары
  // разгоняли машину миссии до 2,2 TopSpeed и копили урон (aicar: m3/m7 smith_v разбиты, вне полос 25%). На полосе — как новая
  // машина: ось ставится на маршрут (driveStep). Прежнее: -DTRAFFIC_REJOIN_REAR_OLD
  c->rearOk = 0;
#endif
  return 1;
}
// ---- угон (правка Роба 63, запись PS2 33:40: Гомер подходит к машине трафика и уезжает на ней) -----------------------------
// ближайшая целая машина трафика (едет или стоит) к точке x, z: расстояние до кузова (центр минус полуширина) <= r; -1 нет
int trfNearRadial;   // правка 285: служебное — прежняя мера близости угона
int trafficNearest(const Traffic *t, float x, float z, float r, int *park, float *dist) {
  int best = -1, bp = 0; float bd = 1e9f;
  if (!t->hd) return -1;
  for (int pass = 0; pass < 2; pass++) {
    int n = pass ? t->npark : t->maxCars;
    for (int i = 0; i < n; i++) {
      const Car *k;
      if (!pass) { const TrCar *c = &t->car[i]; if (c->mode == TRM_OFF || c->mode == TRM_WRECK) continue; k = &c->car; }
      else { if (t->park[i].wreck || !parkLive(t, i)) continue; k = &t->park[i].car; }
      if (k->destroyed) continue;
      float d;
      if (!trfNearRadial) {
        // правка Роба 285 (04.10, школьный автобус): расстояние до коробки кузова, а не до центра минус полуширина — у длинных машин
        // (автобус, грузовик) △ у носа/кормы не находил машину. «flag trfnearold 1» — прежнее (красный tests/traffic/jack_bus.py)
        float bx, bz; bodyCenter(k, &bx, &bz); float fx = sinf(k->h), fz = cosf(k->h), ex = x - bx, ez = z - bz;
        float da = fmaxf(fabsf(ex * fx + ez * fz) - k->p.halfL, 0.f), ds = fmaxf(fabsf(-ex * fz + ez * fx) - k->p.halfW, 0.f);
        d = sqrtf(da * da + ds * ds);
      } else { float dx = k->x - x, dz = k->z - z; d = sqrtf(dx * dx + dz * dz) - k->p.halfW; }
      if (d < bd) { bd = d; best = i; bp = pass; }
    }
  }
  if (best < 0 || bd > r) return -1;
  if (park) *park = bp; if (dist) *dist = bd;
  return best;
}
const Car *trafficCarAt(const Traffic *t, int i, int park) { return park ? &t->park[i].car : &t->car[i].car; }
int trafficCarModelAt(const Traffic *t, int i, int park) { return park ? (int)t->parkd[i].model : t->car[i].model; }
// машину забрал игрок: едущая — выключена (трафик пополнится сам), стоящая — убрана с места до trafficReset
void trafficTake(Traffic *t, int i, int park) {
  if (!park) { t->car[i].mode = TRM_OFF; return; }
  TrParked *p = &t->park[i]; p->state = 0; p->car.x = p->car.z = 1e7f; p->car.y = -1000.f; p->car.v = 0.f;
}
int trafficCarPlace(Traffic *t, const ColWorld *w, float x, float z, float h) {
  int r, l; float s, p[3], a[3];
  if (!t->maxCars || !nearestLane(t, x, z, h, 40.f * 40.f, &r, &l, &s)) return -1;
  TrCar *c = &t->car[0]; int model = t->slot[0].model;
  memset(c, 0, sizeof *c); c->model = model; c->road = r; c->lane = l; c->s = s; c->junc = -1;
  lanePos(t, r, l, s, p); lanePos(t, r, l, s + LOOK, a);
  carInit(&c->car, &t->params[model], w, p[0], p[1], p[2], atan2f(a[0] - p[0], a[2] - p[2]));
  c->mode = TRM_DRIVE; c->v = ROAD_V(t, r) * .6f;
  return 0;
}
static void physStep(Traffic *t, const ColWorld *w, TrCar *c, float dt) {
  CarInput in = {0.f, 0.f, 0.f, 1.f};
  carUpdate(&c->car, w, &in, dt);
  c->wheelAng += c->car.v / fmaxf(.2f, c->car.p.wheelRad) * dt;
  float wv = fabsf(c->car.w[0]) + fabsf(c->car.w[1]) + fabsf(c->car.w[2]);
  c->stillT = (fabsf(c->car.v) < .3f && fabsf(c->car.vl) < .3f && wv < .3f) ? c->stillT + dt : 0.f;
  // В112: native moving minivan stops in1.24s and stands13.76s; v14_ram3 also stands8s
  // Physical car stays physical after the impact. Rejoining a lane was a port guess
  if (TRF_REJOIN_OLD) {
    if (!TRF_UPRIGHT_OLD && c->mode == TRM_PHYS && c->car.wheels < 4 && c->stillT > 0.f) c->stillT = 0.f;
    if (c->mode == TRM_PHYS && c->stillT > 1.5f) { if (!backToLane(t, c)) c->stillT = -1e9f; }
  } else if (!TRF_RAM_OLD && c->mode == TRM_PHYS && !c->car.destroyed) {   // №479
    // №491: на боку/крыше машина лежит и сама не встаёт (автоподъёма нигде нет: carUpdate не выравнивает лежащее тело, трафик его не
    // переставляет); три колеса на земле при наклоне до 60° — не «перевёрнута» и возвращается
    { const float *q = c->car.q; if (1.f - 2.f * (q[1] * q[1] + q[3] * q[3]) < .5f && c->stillT > 0.f) c->stillT = 0.f; }
    if (!c->ramStrong) {   // слабый: стоит, гудит, едет дальше
      if (c->stillT > TRF_RAM_WEAK_STOP && !c->honkDone) { c->honkDone = 1; pushEv(t, TR_EV_CAR_HONK, (int)(c - t->car), c->car.x, c->car.y, c->car.z, 0.f); }
      if (c->stillT > TRF_RAM_WEAK_GO) { if (backToLane(t, c)) c->hitV = 0.f; else c->stillT = -1e9f; }
    } else if (c->stillT > 0.f && (c->stunT += dt) > TRF_RAM_STUN) {   // сильный: оглушён, потом возвращается на полосу
      if (backToLane(t, c)) { c->hitV = 0.f; c->ramStrong = 0; } else c->stunT = 0.f;
    }
  }
}
// места спавна: точки полос (вершины ломаных, 1339 на ур. 1) в кольце и вне кадра; до SPAWN_CAND случайных из подходящих
#define SPAWN_CAND 48
// ПРЕЖНЕЕ правило (до правки 328, «flag trfspawnold 1» — красный контроль tests/traffic/spawnview.sh): кольцо, расстояние до игрока
// и кадр проверялись по ВЕРШИНЕ ломаной полосы, а машина вставала в случайную точку отрезка до следующей вершины; при сбросе кадр
// не проверялся вовсе. Тело не менять — оно и есть контроль
static void spawnCarsOld(Traffic *t, const ColWorld *w, const Car *player, const float ppos[3], const float cen[2],
                         const float eye[3], const float fwd[3]) {
  int need = 0; for (int i = 0; i < t->maxCars; i++) need += t->car[i].mode == TRM_OFF;
  if (!need) return;
  struct { uint16_t r, k; uint8_t l; } cand[SPAWN_CAND]; int nc = 0, seen = 0;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadOk(t, (int)r)) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int k = 0; k + 1 < n; k++) {
        float dx = P[k].x - cen[0], dz = P[k].z - cen[1], dc = sqrtf(dx * dx + dz * dz);
        float qx = P[k].x - ppos[0], qz = P[k].z - ppos[2], dp = sqrtf(qx * qx + qz * qz);
        if (t->fresh ? (dp < 20.f || dp > INIT_R) : (fabsf(dc - SPAWN_R) > SPAWN_BAND || dp < 30.f)) continue;
        if (!t->fresh && inView(eye, fwd, P[k].x, P[k].y, P[k].z, 4.f, t->drawDist)) continue;
        seen++;   // выборка резервуаром: каждая подходящая точка с равной вероятностью
        int j = nc < SPAWN_CAND ? nc++ : (int)(rnd(t) * seen);
        if (j < SPAWN_CAND) { cand[j].r = (uint16_t)r; cand[j].k = (uint16_t)k; cand[j].l = (uint8_t)l; }
      }
    }
  }
  for (int i = 0, tries = 0; i < t->maxCars && nc > 0 && tries < 2 * SPAWN_CAND; i++) {
    TrCar *c = &t->car[i]; if (c->mode != TRM_OFF) continue;
    int placed = 0;
    while (!placed && tries++ < 2 * SPAWN_CAND) {
      int q = (int)(rnd(t) * nc) % nc, r = cand[q].r, l = cand[q].l;
      const TrPt *P = lanePts(t, r, l); float s = P[cand[q].k].s + rnd(t) * (P[cand[q].k + 1].s - P[cand[q].k].s), p[3];
      lanePos(t, r, l, s, p);
      int clash = 0;
      for (int k = 0; k < t->maxCars && !clash; k++) if (t->car[k].mode != TRM_OFF) {
        float ex = t->car[k].car.x - p[0], ez = t->car[k].car.z - p[2]; clash = ex * ex + ez * ez < 14.f * 14.f; }
      for (int k = 0; k < t->npark && !clash; k++) { if (!parkLive(t, k)) continue;
        float ex = t->park[k].car.x - p[0], ez = t->park[k].car.z - p[2]; clash = ex * ex + ez * ez < 5.f * 5.f; }
      if (player) { float ex = player->x - p[0], ez = player->z - p[2]; clash |= ex * ex + ez * ez < 12.f * 12.f; }
      if (clash) continue;
      int model = t->slot[i].model;
      memset(c, 0, sizeof *c);
      c->model = model; c->road = r; c->lane = l; c->s = s; c->junc = -1;
      float a[3]; lanePos(t, r, l, s + LOOK, a); float h = atan2f(a[0] - p[0], a[2] - p[2]);
      carInit(&c->car, &t->params[model], w, p[0], p[1], p[2], h);
      c->mode = TRM_DRIVE; c->v = ROAD_V(t, r) * (t->fresh ? .5f : 1.f);
      placed = 1;
    }
  }
}

// Правка Роба 328 (04.10: «машины NPC иногда появляются внезапно прямо передо мной на дороге — убрать; пусть тупят, но не
// настолько, чтобы мешать»). Замер tests/traffic/spawnview.sh (7 уровней x 18 мин езды): прежнее правило рождало машину в кадре
// ближе дальности рисования 37–113 раз за 18 мин (ур. 1 — 50, из них 3 ближе 30 м; ур. 2 и 5 — 113 и 101, ближе 60 м 32 и 29:
// отрезки полос там до 180 м), ближайшая — в 21 м от камеры (12 м от игрока) прямо по оси кадра: кольцо и кадр проверялись по
// вершине полосы, а машина вставала в случайную точку отрезка; при сбросе (NEW GAME, goto) кадр не проверялся — 13–27% рождений
// в кадре, ближайшее в 4 м от камеры; на кадре переноса миссией камера ещё старая (далеко) — «пустой» кадр. Правило (все уровни;
// правило порта — у оригинала, по разбору exe hit-and-run-web docs/PURSUIT_PARITY.md, кольцо 65 м вокруг точки 40 м впереди
// камеры: в кадре это не ближе ~90 м):
//  1) кандидаты — точки полос через SPAWN_STEP м по длине (не вершины), все проверки — в самой точке рождения;
//  2) не в кадре (сфера кузова + SPAWN_PAD, ближе дальности рисования машин): ни камеры (eye/fwd прошлого кадра), ни камеры «за
//     игроком» — SPAWN_CAM_BACK м позади по ходу машины (пешком — по взгляду камеры): туда встанет камера после разговора, «наезда»
//     миссии, переноса; дальше дальности рисования — можно (машина въедет в дальность, как всё на краю);
//  3) впереди (по взгляду камеры или по ходу машины игрока) не ближе SPAWN_AHEAD_MIN м, сзади и сбоку — не ближе SPAWN_MIN
//     (при сбросе — INIT_MIN; за спиной и за поворотом можно);
//  4) при сбросе — те же правила (раньше кадр не проверялся);
//  5) как у оригинала (тот же разбор exe): через 1,5 с машина ещё не уйдёт из круга удаления — меньше пустых рождений (было
//     74 в минуту на 5 машин, каждая жила ~4 с): плотность вокруг игрока не падает, хотя часть мест теперь закрыта кадром.
// Застрявшая машина уходит только вне кадра (как было) и рождается заново по этому же правилу.
int trfSpawnOld;
#define SPAWN_STEP 8.f        // шаг точек-кандидатов по полосе, м
#define SPAWN_PAD 2.f         // запас к радиусу кузова в проверке кадра, м
#define SPAWN_CAM_BACK 9.f    // камера «за игроком»: столько позади, на 3 м выше
#define SPAWN_AHEAD_MIN 50.f  // впереди не ближе, м
#define SPAWN_MIN 30.f        // сзади/сбоку не ближе (прежнее), м
#define INIT_MIN 20.f         // при сбросе сзади/сбоку не ближе (прежнее), м
#define SPAWN_AHEAD_T 1.5f    // прогноз (оригинал по разбору exe: «1.5-second prediction of the lane and player positions»), с
typedef struct { float eye[3], fwd[3], eye2[3], fwd2[3], dir[2][2], pv[2]; int ndir; } SpawnView;
static int spawnSpotOk(const Traffic *t, const SpawnView *V, const float ppos[3], const float p[3], float R) {
  float qx = p[0] - ppos[0], qz = p[2] - ppos[2], dp = sqrtf(qx * qx + qz * qz);
  if (dp < (t->fresh ? INIT_MIN : SPAWN_MIN)) return 0;
  if (dp < SPAWN_AHEAD_MIN) for (int k = 0; k < V->ndir; k++) if (qx * V->dir[k][0] + qz * V->dir[k][1] > 0.f) return 0;
  float r = R + SPAWN_PAD;
  if (inView(V->eye, V->fwd, p[0], p[1] + 1.f, p[2], r, t->drawDist)) return 0;
  if (inView(V->eye2, V->fwd2, p[0], p[1] + 1.f, p[2], r, t->drawDist)) return 0;
  return 1;
}
static void spawnCars(Traffic *t, const ColWorld *w, const Car *player, const float ppos[3], const float cen[2],
                      const SpawnView *V) {
  int need = 0; for (int i = 0; i < t->maxCars; i++) need += t->car[i].mode == TRM_OFF;
  if (!need) return;
  float Rmax = 0.f;   // радиус кузова самой длинной модели пула: кандидат годится любой машине
  for (int i = 0; i < t->maxCars; i++) { const CarParams *P = &t->params[t->slot[i].model]; float R = P->halfL + fabsf(P->bodyOfs) + .5f; if (R > Rmax) Rmax = R; }
  struct { uint16_t r; uint8_t l, pad; float s, ds; } cand[SPAWN_CAND]; int nc = 0, seen = 0;
  float lo = t->fresh ? 0.f : SPAWN_R - SPAWN_BAND, hi = t->fresh ? INIT_R : SPAWN_R + SPAWN_BAND;
  float cx = t->fresh ? ppos[0] : cen[0], cz = t->fresh ? ppos[2] : cen[1];   // при сбросе — вокруг игрока до INIT_R
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadOk(t, (int)r)) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int k = 0; k + 1 < n; k++) {
        float ax = P[k].x - cx, az = P[k].z - cz, bx = P[k + 1].x - cx, bz = P[k + 1].z - cz;
        float ex = bx - ax, ez = bz - az, L2 = ex * ex + ez * ez, u = L2 > 1e-6f ? clampf(-(ax * ex + az * ez) / L2, 0.f, 1.f) : 0.f;
        float mx = ax + ex * u, mz = az + ez * u;   // отрезок целиком вне кольца — мимо (быстро)
        if (mx * mx + mz * mz > hi * hi || fmaxf(ax * ax + az * az, bx * bx + bz * bz) < lo * lo) continue;
        float L = P[k + 1].s - P[k].s; int m = (int)ceilf(L / SPAWN_STEP); if (m < 1) m = 1;
        for (int j = 0; j < m; j++) {
          float u = (j + .5f) / m, s = P[k].s + L * u, p[3] = {P[k].x + (P[k + 1].x - P[k].x) * u, P[k].y + (P[k + 1].y - P[k].y) * u, P[k].z + (P[k + 1].z - P[k].z) * u};
          float dx = p[0] - cx, dz = p[2] - cz, dc = sqrtf(dx * dx + dz * dz);
          if (dc < lo || dc > hi) continue;
          if (!spawnSpotOk(t, V, ppos, p, Rmax)) continue;
          if (!t->fresh) {   // 5) через SPAWN_AHEAD_T машина (по полосе со скоростью дороги) ещё в REMOVE_R от центра (сдвинутого ходом игрока)
            float f[3], v = ROAD_V(t, r); lanePos(t, (int)r, l, s + v * SPAWN_AHEAD_T, f);
            float qx = f[0] - (cen[0] + V->pv[0] * SPAWN_AHEAD_T), qz = f[2] - (cen[1] + V->pv[1] * SPAWN_AHEAD_T);
            if (qx * qx + qz * qz > REMOVE_R * REMOVE_R) continue;
          }
          seen++;   // выборка резервуаром: каждая подходящая точка с равной вероятностью
          int q = nc < SPAWN_CAND ? nc++ : (int)(rnd(t) * seen);
          if (q < SPAWN_CAND) { cand[q].r = (uint16_t)r; cand[q].l = (uint8_t)l; cand[q].s = s; cand[q].ds = L / m; }
        }
      }
    }
  }
  for (int i = 0, tries = 0; i < t->maxCars && nc > 0 && tries < 2 * SPAWN_CAND; i++) {
    TrCar *c = &t->car[i]; if (c->mode != TRM_OFF) continue;
    int placed = 0;
    while (!placed && tries++ < 2 * SPAWN_CAND) {
      int q = (int)(rnd(t) * nc) % nc, r = cand[q].r, l = cand[q].l;
      float s = cand[q].s + (rnd(t) - .5f) * cand[q].ds, p[3];
      lanePos(t, r, l, s, p);
      if (!spawnSpotOk(t, V, ppos, p, Rmax)) { s = cand[q].s; lanePos(t, r, l, s, p); }   // сдвиг внутри шага вывел из правила — точка кандидата
      int clash = 0;
      for (int k = 0; k < t->maxCars && !clash; k++) if (t->car[k].mode != TRM_OFF) {
        float ex = t->car[k].car.x - p[0], ez = t->car[k].car.z - p[2]; clash = ex * ex + ez * ez < 14.f * 14.f; }
      for (int k = 0; k < t->npark && !clash; k++) { if (!parkLive(t, k)) continue;
        float ex = t->park[k].car.x - p[0], ez = t->park[k].car.z - p[2]; clash = ex * ex + ez * ez < 5.f * 5.f; }
      if (player) { float ex = player->x - p[0], ez = player->z - p[2]; clash |= ex * ex + ez * ez < 12.f * 12.f; }
      if (clash) continue;
      int model = t->slot[i].model;
      memset(c, 0, sizeof *c);
      c->model = model; c->road = r; c->lane = l; c->s = s; c->junc = -1;
      float a[3]; lanePos(t, r, l, s + LOOK, a); float h = atan2f(a[0] - p[0], a[2] - p[2]);
      carInit(&c->car, &t->params[model], w, p[0], p[1], p[2], h);
      c->mode = TRM_DRIVE; c->v = ROAD_V(t, r) * (t->fresh ? .5f : 1.f);
      placed = 1;
    }
  }
}

// ================================================================ пешеходы
static const float *pathP(const Traffic *t, int p, int k) { const TrPath *P = &t->path[p]; return t->pathPt + 3 * (P->first + (uint32_t)((k % (int)P->n + (int)P->n) % (int)P->n)); }
static void pedPlay(Traffic *t, TrPed *e, int clip, int loop) {
  int c = t->clip[e->model][clip]; if (c < 0) c = t->clip[e->model][TRC_IDLE];
  skinPlay(&e->sp, c, loop, .2f);
}
static int pedStands(const TrPed *e) { return e->mode == TRP_WALK || e->mode == TRP_STAND || e->mode == TRP_GETUP || e->mode == TRP_DODGE; }
// №594 (L3 D5-9, пешеходы «на пару уровней выше земли»): путь пешехода может идти над местом без коллизии земли (замер tests/traffic/ped_ground.c: L3 путь 30
// у x −347 z −214 — под ногами пусто) — пешеход держал высоту пути и висел в воздухе. Теперь шаг без земли в окне [y−3, y+up] считается; дольше
// PED_NOGROUND_T пешеход убирается (появится на другом пути). -DPED_GROUND_OLD=1 — прежнее. Возврат 1 — земля найдена
#ifndef PED_GROUND_OLD
#define PED_GROUND_OLD 0
#endif
#ifndef PED_NOGROUND_T
#define PED_NOGROUND_T 0.6f
#endif
static int pedGround(const ColWorld *w, TrPed *e, float up) {
  float g; if (colGround(w, e->x, e->z, e->y + up, e->y - 3.f, &g, 0)) { e->y = g; return 1; }
  return 0;
}
static void spawnPeds(Traffic *t, const ColWorld *w, const float ppos[3], const float eye[3], const float fwd[3]) {
  if (!t->maxPeds || t->group < 0 || t->group >= (int)t->hd->nGroup) return;
  const TrGroup *g = t->grp + t->group; if (!g->n) return;
  int tries = t->fresh ? 300 : 6;
  for (int i = 0; i < t->maxPeds && tries > 0; i++) {
    TrPed *e = &t->ped[i]; if (e->mode != TRP_OFF) continue;
    while (tries-- > 0) {
      int p = (int)(rnd(t) * t->hd->nPath) % (int)t->hd->nPath; const TrPath *P = &t->path[p];
      float dx = P->cx - ppos[0], dz = P->cz - ppos[2];
      if (sqrtf(dx * dx + dz * dz) > P->rad + PED_SPAWN_MAX) continue;
      int k = (int)(rnd(t) * P->n) % (int)P->n; const float *a = pathP(t, p, k), *b = pathP(t, p, k + 1);
      float u = rnd(t), x = a[0] + (b[0] - a[0]) * u, y = a[1] + (b[1] - a[1]) * u, z = a[2] + (b[2] - a[2]) * u;
      float qx = x - ppos[0], qz = z - ppos[2], dp = sqrtf(qx * qx + qz * qz);
      if (dp > PED_SPAWN_MAX || dp < (t->fresh ? 8.f : PED_SPAWN_MIN)) continue;
      if (!t->fresh && dp < 35.f && inView(eye, fwd, x, y + 1.f, z, 1.f, t->pedDrawDist)) continue;
      int clash = 0;
      for (int j = 0; j < t->maxPeds && !clash; j++) if (t->ped[j].mode != TRP_OFF) {
        float ex = t->ped[j].x - x, ez = t->ped[j].z - z; clash = ex * ex + ez * ez < 3.f * 3.f; }
      if (clash) continue;
      int tot = 0; for (int j = 0; j < g->n; j++) tot += g->count[j];
      int pick = (int)(rnd(t) * (tot ? tot : 1)), m = g->model[0];
      for (int j = 0; j < g->n; j++) { if (pick < g->count[j]) { m = g->model[j]; break; } pick -= g->count[j]; }
      memset(e, 0, sizeof *e);
      e->model = m; e->path = p; e->seg = k; e->dir = rnd(t) < .5f ? 1 : -1; e->speed = t->walkV[m];
      e->x = x; e->y = y; e->z = z; e->s = u;
      const float *nx = e->dir > 0 ? b : a; e->h = atan2f(nx[0] - x, nx[2] - z);
      if (!PED_GROUND_OLD && !pedGround(w, e, 1.5f)) continue;   // №594: рождение над пустотой — другая точка
      else if (PED_GROUND_OLD) pedGround(w, e, 1.5f);
      skinInit(&e->sp, &t->peds[m]); e->mode = TRP_WALK; pedPlay(t, e, TRC_WALK, 1);
      break;
    }
  }
}
static void pedFly(Traffic *t, int i, float vx, float vz, float vy) {
  TrPed *e = &t->ped[i];
  e->mode = TRP_FLY; e->vx = vx; e->vz = vz; e->vy = vy; e->t = 0.f; pedPlay(t, e, TRC_FLAIL, 1);
}
// точка пути, ближайшая к пешеходу: отрезок и доля
static void pedNearest(const Traffic *t, TrPed *e) {
  const TrPath *P = &t->path[e->path]; float best = 1e30f;
  for (uint32_t k = 0; k < P->n; k++) {
    const float *a = pathP(t, e->path, (int)k), *b = pathP(t, e->path, (int)k + 1);
    float ax = b[0] - a[0], az = b[2] - a[2], L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
    float u = clampf(((e->x - a[0]) * ax + (e->z - a[2]) * az) / L2, 0.f, 1.f), qx = a[0] + ax * u - e->x, qz = a[2] + az * u - e->z;
    if (qx * qx + qz * qz < best) { best = qx * qx + qz * qz; e->seg = (int)k; e->s = u; }
  }
}
// Гомер/другой пешеход/машина игрока прямо впереди ближе 1.2 м — стоять
static int pedBlocked(const Traffic *t, const TrPed *e, const Car *player, int driving, const float ppos[3]) {
  float fx = sinf(e->h), fz = cosf(e->h);
  if (!driving && !t->noSolid) {
    float dx = ppos[0] - e->x, dz = ppos[2] - e->z, a = dx * fx + dz * fz, l = -dx * fz + dz * fx;
    if (fabsf(ppos[1] - e->y) < 1.6f && a > 0.f && a < TR_PED_R + HOMER_RADIUS + .5f && fabsf(l) < TR_PED_R + HOMER_RADIUS) return 1;
  }
  for (int j = 0; j < t->maxPeds; j++) {
    const TrPed *o = &t->ped[j]; if (o == e || !pedStands(o)) continue;
    float dx = o->x - e->x, dz = o->z - e->z, a = dx * fx + dz * fz, l = -dx * fz + dz * fx;
    if (a > 0.f && a < 2.f * TR_PED_R + .4f && fabsf(l) < 2.f * TR_PED_R) return (e - t->ped) > j;   // уступает младший номер
  }
  if (player) {
    CarBox b; carBox(player, &b); float px = e->x + fx * .8f, pz = e->z + fz * .8f, dx = px - b.x, dz = pz - b.z;
    if (fabsf(dx * b.fx + dz * b.fz) < b.hl + TR_PED_R && fabsf(-dx * b.fz + dz * b.fx) < b.hw + TR_PED_R) return 1;
  }
  return 0;
}
// кузов машины против круга пешехода: 1 = касание (нормаль от машины к пешеходу, глубина)
static int boxCircle(const Car *k, float x, float z, float r, float *nx, float *nz, float *depth) {
  CarBox b; carBox(k, &b);
  float dx = x - b.x, dz = z - b.z, rx = -b.fz, rz = b.fx, a = dx * b.fx + dz * b.fz, s = dx * rx + dz * rz;
  float ca = clampf(a, -b.hl, b.hl), cs = clampf(s, -b.hw, b.hw), ea = a - ca, es = s - cs, d2 = ea * ea + es * es;
  if (d2 >= r * r) return 0;
  float na, ns;
  if (d2 > 1e-8f) { float d = sqrtf(d2); na = ea / d; ns = es / d; *depth = r - d; }
  else { float pa = b.hl - fabsf(a), ps = b.hw - fabsf(s);
    if (pa < ps) { na = a < 0 ? -1.f : 1.f; ns = 0.f; *depth = pa + r; } else { na = 0.f; ns = s < 0 ? -1.f : 1.f; *depth = ps + r; } }
  *nx = na * b.fx + ns * rx; *nz = na * b.fz + ns * rz;
  return 1;
}
// правка 240: удар машины по человеку — общий для прохожих (pedHitBy) и персонажей миссий (game_msn.c), см. traffic.h
int trafficCarHitsBody(const Car *k, float *x, float y, float *z, float r, float v[3]) {
  if (fabsf(k->y - y) > 1.8f) return 0;
  float nx, nz, depth;
  if (!boxCircle(k, *x, *z, r, &nx, &nz, &depth)) return 0;
  float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]);
  if (sp <= PED_HIT_V) { *x += nx * depth; *z += nz * depth; return 1; }   // медленно — отталкивает
#ifndef TR_KNOCK_UP_OLD
#define TR_KNOCK_UP_OLD 0   // 1 — прежний подброс 3 + 0.15 v (≤ 8): ~0.55 м при 11 м/с
#endif
  // Видео PS2 L1 (MattyKunn, 8:52.4): сбитая женщина вылетает над капотом ~1,5 м (на глаз по кадрам 0,1 с); при PED_G 20 нужен подъём ~1,2 м
  // у типичного удара 10–12 м/с: 3.5 + 0.3 v (≤ 8.5) → 6.8 м/с при 11 м/с. Прежнее: -DTR_KNOCK_UP_OLD=1
  v[1] = TR_KNOCK_UP_OLD ? fminf(3.f + sp * .15f, 8.f) : fminf(3.5f + sp * .3f, 8.5f);
#if TR_KNOCK_OLD
  v[0] = k->vel[0] * .8f + nx * 2.f; v[2] = k->vel[2] * .8f + nz * 2.f;
#else
  float vn = fmaxf(k->vel[0] * nx + k->vel[2] * nz, 0.f), tx = k->vel[0] - nx * vn, tz = k->vel[2] - nz * vn;   // сближение / вдоль
  float out = vn * (1.f + TR_KNOCK_E) + TR_KNOCK_MIN;
  v[0] = tx * TR_KNOCK_T + nx * out; v[2] = tz * TR_KNOCK_T + nz * out;
  *x += nx * depth; *z += nz * depth;   // полёт начинается снаружи кузова
#endif
  return 2;
}
int trafficBodyFly(const ColWorld *w, float p[3], float v[3], float *t, float dt) {
  v[1] -= PED_G * dt;
#if !TR_KNOCK_OLD
  if (v[0] != 0.f || v[2] != 0.f) {   // стена дома, ящик, столб на пути (луч на высоте пояса): дальше вбок не летит — падает у стены
    float a[3] = {p[0], p[1] + TR_BODY_MID, p[2]}, b[3] = {a[0] + v[0] * dt, a[1], a[2] + v[2] * dt};
    // заборы (отрезки COL1: невидимые границы, изгороди — машина их проезжает с изнанки и сквозь кусты) сбитого не держат, как и
    // до правки: иначе он падает у невидимого забора, а машина проезжает забор и по нему (замер 03.10: Пэтти у забора 1149, (193, 523))
    if (colRayFrac(w, a, b, 0) < 1.f && !colCrossFence(w, a[0], a[2], b[0], b[2], 0, 0) && !colCrossFence(w, b[0], b[2], a[0], a[2], 0, 0))
      v[0] = v[2] = 0.f;
  }
#endif
  p[0] += v[0] * dt; p[1] += v[1] * dt; p[2] += v[2] * dt; *t += dt;
  float g;
  if (v[1] < 0.f && colGround(w, p[0], p[2], p[1] + 1.f, p[1] - 4.f, &g, 0) && p[1] <= g) { p[1] = g; v[0] = v[1] = v[2] = 0.f; return 1; }
  return *t > 4.f ? -1 : 0;
}
float trafficGetUpLen(const Traffic *t) {
  for (int m = 0; t && m < t->npeds; m++) { int c = t->clip[m][TRC_GETUP]; if (c >= 0) return skinClipLength(&t->peds[m], c); }
  return 1.f;
}
static void pedHitBy(Traffic *t, int i, const Car *k) {
  // правка 240: лежащего машина тоже толкает и подбрасывает (иначе едущий дальше игрок проезжает по нему, и сбитый «уходит под
  // машину»); новое нарушение Hit & Run — только за удар стоящего (правило порта)
  TrPed *e = &t->ped[i]; int down = !TR_KNOCK_OLD && e->mode == TRP_DOWN;
  if (!pedStands(e) && !down) return;
  float v[3];
  if (trafficCarHitsBody(k, &e->x, e->y, &e->z, TR_PED_R, v) != 2) return;   // 1 — медленно: уже вытолкнут
  pedFly(t, i, v[0], v[2], v[1]);
  if (!down) pushEvW(t, TR_EV_PED_HIT, i, e->x, e->y, e->z, HIT_PED, t->pedm[e->model].name);
}
static void pedNearMiss(Traffic *t, int i, const Car *k) {   // правка 180: «чуть не сбил» — уклонение и событие для голоса
  TrPed *e = &t->ped[i];
  if ((e->mode != TRP_WALK && e->mode != TRP_STAND) || e->nearCool > 0.f || fabsf(k->y - e->y) > 1.8f) return;
  float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]); if (sp < PED_NEAR_V) return;
  float nx, nz, depth;
  if (boxCircle(k, e->x, e->z, TR_PED_R, &nx, &nz, &depth)) return;               // задел — это удар (pedHitBy)
  { CarBox b; carBox(k, &b); float vx = k->vel[0] / sp, vz = k->vel[2] / sp, dx = e->x - b.x, dz = e->z - b.z;   // идёт прямо на него —
    if (fabsf(dx * vz - dz * vx) < b.hw + TR_PED_R + .1f && dx * vx + dz * vz > 0.f) return; }                // будет удар, не «чуть не»
  if (!boxCircle(k, e->x, e->z, TR_PED_R + PED_NEAR_R, &nx, &nz, &depth)) return; // далеко
  if (k->vel[0] * nx + k->vel[2] * nz < 0.f) return;                               // уезжает от прохожего (нормаль — от машины к нему)
  e->mode = TRP_DODGE; e->t = 0.f; e->nearCool = PED_NEAR_COOL; pedPlay(t, e, TRC_DODGE, 0);
  pushEvW(t, TR_EV_PED_NEAR, i, e->x, e->y, e->z, 0.f, t->pedm[e->model].name);
}
// №510 (Роб: NPC «стоят и дёргаются», «нет логики обходить людей»): пешеход, которому преградили путь игрок пешком или другой пешеход, не
// встаёт в упор, а обходит сбоку — шаг по направлению, повёрнутому на 50° и 90° в сторону, противоположную преграде; сторона держится
// PED_AVOID_HOLD с (не прыгает кадр от кадра); без обрыва под ногами, не в машину игрока; нельзя — стоит, как было. TRF_PED_AVOID_OLD=1 — прежнее
#ifndef TRF_PED_AVOID_OLD
#define TRF_PED_AVOID_OLD 0
#endif
#define PED_AVOID_HOLD 1.5f
// №692 (Роб, причал L3: «5–7 стоят вокруг машины и смотрят»): обход невозможен (край настила, обрыв, стена) — пешеход вставал и ждал, пока
// машина игрока не уедет; у оригинала прохожие проходят мимо и не копятся. Путь перекрыт дольше PED_WAIT_T с подряд — разворот по своему
// пути назад (сторона обхода сбрасывается), потом снова вперёд; ждёт не дольше PED_WAIT_T. PED_WAIT_OLD=1 — прежнее (ждёт бесконечно)
#ifndef PED_WAIT_OLD
#define PED_WAIT_OLD 0
#endif
#define PED_WAIT_T 1.5f
static int pedSpotFree(const Traffic *t, const TrPed *e, const Car *player, int driving, const float ppos[3], float x, float z) {
  if (!driving && !t->noSolid) { float dx = ppos[0] - x, dz = ppos[2] - z, r = TR_PED_R + HOMER_RADIUS;
    if (fabsf(ppos[1] - e->y) < 1.6f && dx * dx + dz * dz < r * r && hypotf(ppos[0] - e->x, ppos[2] - e->z) >= hypotf(dx, dz)) return 0; }   // вплотную и отходит — можно
  for (int j = 0; j < t->maxPeds; j++) { const TrPed *o = &t->ped[j]; if (o == e || !pedStands(o)) continue;
    float dx = o->x - x, dz = o->z - z, r = 2.f * TR_PED_R;
    if (dx * dx + dz * dz < r * r && hypotf(o->x - e->x, o->z - e->z) >= hypotf(dx, dz)) return 0; }
  if (player) { float nx, nz, dp; if (boxCircle(player, x, z, TR_PED_R + .2f, &nx, &nz, &dp)) return 0; }
  return 1;
}
static int pedSidestep(Traffic *t, const ColWorld *w, TrPed *e, const Car *player, int driving, const float ppos[3], float dt, float ux, float uz) {
  float bx = 0.f, bz = 0.f; int have = 0;
  if (!driving && !t->noSolid && fabsf(ppos[1] - e->y) < 1.6f && hypotf(ppos[0] - e->x, ppos[2] - e->z) < 2.f) { bx = ppos[0] - e->x; bz = ppos[2] - e->z; have = 1; }
  if (!have) for (int j = 0; j < t->maxPeds && !have; j++) { const TrPed *o = &t->ped[j]; if (o == e || !pedStands(o)) continue;
    if (hypotf(o->x - e->x, o->z - e->z) < 2.f * TR_PED_R + .5f) { bx = o->x - e->x; bz = o->z - e->z; have = 1; } }
  if (!have) return 0;   // преграда — машина игрока: стоит, как было
  if (!e->avoidSide) e->avoidSide = (ux * bz - uz * bx) > 0.f ? -1 : 1;
  e->avoidT = PED_AVOID_HOLD;
  static const float ang[2] = {0.87f, 1.57f};
  float step = e->speed * dt;
  for (int k = 0; k < 4; k++) {
    float a = ang[k & 1] * (float)e->avoidSide * (k < 2 ? 1.f : -1.f);
    float c = cosf(a), sn = sinf(a), vx = ux * c - uz * sn, vz = ux * sn + uz * c, px = e->x + vx * step, pz = e->z + vz * step, g;
    if (!pedSpotFree(t, e, player, driving, ppos, px, pz)) continue;
    if (w && !colGround(w, px, pz, e->y + .6f, e->y - .6f, &g, 0)) continue;   // обрыв/пустота под ногами
    float dh = wrapPi(atan2f(vx, vz) - e->h), mx = 5.f * dt; e->h = wrapPi(e->h + clampf(dh, -mx, mx));
    e->x = px; e->z = pz; if (k >= 2) e->avoidSide = -e->avoidSide; return 1; }
  return 0;
}
// №813 (Роб 18.10: «Милхаус обходит Барта и бесконечно ходит по кругу» — у прохожих тот же обход №510): точка пути под игроком или под
// стоящим пешеходом недостижима — обход водил по кругу без конца (удачный шаг обхода не считался ожиданием №692). Теперь: точка занята
// и до неё ближе PED_ORBIT_REACH — пройдена, дальше по своему пути (1); обход без продвижения к точке PED_ORBIT_T с — рядом с точкой
// пройдена (1), далеко — стоит (2), и через PED_WAIT_T №692 развернёт его назад по пути. PED_ORBIT_OLD=1 — прежнее
#ifndef PED_ORBIT_OLD
#define PED_ORBIT_OLD 0
#endif
#define PED_ORBIT_REACH 2.f
#define PED_ORBIT_PAD .25f
#define PED_ORBIT_T 2.5f
#define PED_ORBIT_GAIN .2f
static int pedOrbit(Traffic *t, TrPed *e, int driving, const float ppos[3], float tx, float tz, float d, float dt) {
  if (PED_ORBIT_OLD) return 0;
  if (e->avoidT <= 0.f || tx != e->orbTx || tz != e->orbTz) { e->orbTx = tx; e->orbTz = tz; e->orbBest = d; e->orbStall = 0.f; }
  else if (d < e->orbBest - PED_ORBIT_GAIN) { e->orbBest = d; e->orbStall = 0.f; }
  else e->orbStall += dt;
  int taken = 0, stalled = e->orbStall > PED_ORBIT_T;
  if (!driving && !t->noSolid && fabsf(ppos[1] - e->y) < 1.6f) { float r = TR_PED_R + HOMER_RADIUS + PED_ORBIT_PAD; taken = (tx - ppos[0]) * (tx - ppos[0]) + (tz - ppos[2]) * (tz - ppos[2]) < r * r; }
  for (int j = 0; j < t->maxPeds && !taken; j++) { const TrPed *o = &t->ped[j]; if (o == e || !pedStands(o)) continue;
    float r = 2.f * TR_PED_R + PED_ORBIT_PAD; taken = (tx - o->x) * (tx - o->x) + (tz - o->z) * (tz - o->z) < r * r; }
  if (d < PED_ORBIT_REACH && (taken || stalled)) {   // пройдена: следующая точка пути (как при приходе, без шага в занявшего)
    int n = (int)t->path[e->path].n; e->seg = ((e->seg + e->dir) % n + n) % n; e->orbStall = 0.f; return 1; }
  return stalled ? 2 : 0;
}
static void pedStep(Traffic *t, const ColWorld *w, int i, const Car *player, int driving, const float ppos[3], float dt) {
  TrPed *e = &t->ped[i];
  switch (e->mode) {
    case TRP_WALK: case TRP_STAND: {
      if (e->test) { if (e->mode != TRP_STAND) { e->mode = TRP_STAND; pedPlay(t, e, TRC_IDLE, 1); } break; }
      int blocked = pedBlocked(t, e, player, driving, ppos);
      if (!TRF_PED_AVOID_OLD) {   // №510
        if (!blocked && (e->avoidT -= dt) < 0.f) { e->avoidT = 0.f; e->avoidSide = 0; }
        if (blocked && e->mode == TRP_WALK) {
          const float *a_ = pathP(t, e->path, e->seg), *b_ = pathP(t, e->path, e->seg + 1);
          float tx_ = e->dir > 0 ? b_[0] : a_[0], tz_ = e->dir > 0 ? b_[2] : a_[2], dx_ = tx_ - e->x, dz_ = tz_ - e->z, d_ = sqrtf(dx_ * dx_ + dz_ * dz_);
          int ob_ = pedOrbit(t, e, driving, ppos, tx_, tz_, d_, dt); if (ob_ == 1) break;   // №813
          if (!ob_ && pedSidestep(t, w, e, player, driving, ppos, dt, d_ > .05f ? dx_ / d_ : sinf(e->h), d_ > .05f ? dz_ / d_ : cosf(e->h))) { pedGround(w, e, .6f); break; } }
        else if (!blocked && e->mode == TRP_WALK && !e->test) {   // шаг к точке пути не должен заходить в круг игрока/пешехода (обход мог вывести сбоку)
          const float *a_ = pathP(t, e->path, e->seg), *b_ = pathP(t, e->path, e->seg + 1);
          float tx_ = e->dir > 0 ? b_[0] : a_[0], tz_ = e->dir > 0 ? b_[2] : a_[2], dx_ = tx_ - e->x, dz_ = tz_ - e->z, d_ = sqrtf(dx_ * dx_ + dz_ * dz_);
          if (d_ > .05f && !pedSpotFree(t, e, player, driving, ppos, e->x + dx_ / d_ * e->speed * dt, e->z + dz_ / d_ * e->speed * dt)) {
            int ob_ = pedOrbit(t, e, driving, ppos, tx_, tz_, d_, dt); if (ob_ == 1) break;   // №813
            if (!ob_ && pedSidestep(t, w, e, player, driving, ppos, dt, dx_ / d_, dz_ / d_)) { pedGround(w, e, .6f); break; }
            blocked = 1; } } }
      if (!PED_WAIT_OLD) {
        if (!blocked) e->waitT = 0.f;
        else if ((e->waitT += dt) > PED_WAIT_T) {   // №692: дольше PED_WAIT_T не стоять — назад по своему пути
          e->waitT = 0.f; e->dir = -e->dir; e->avoidSide = 0; e->avoidT = 0.f; e->blockT = 0.f;
          { const float *a2 = pathP(t, e->path, e->seg), *b2 = pathP(t, e->path, e->seg + 1); float ex2 = (e->dir > 0 ? b2[0] : a2[0]) - e->x, ez2 = (e->dir > 0 ? b2[2] : a2[2]) - e->z;
            if (ex2 * ex2 + ez2 * ez2 > .0025f) e->h = atan2f(ex2, ez2); }   // лицом к новой цели сразу: иначе «впереди машина» держит на месте
          if (e->mode != TRP_WALK) { e->mode = TRP_WALK; pedPlay(t, e, TRC_WALK, 1); }
          break; } }
      if (blocked) { e->blockT = .5f; if (e->mode != TRP_STAND) { e->mode = TRP_STAND; pedPlay(t, e, TRC_IDLE, 1); } break; }
      if (e->mode == TRP_STAND) { if ((e->blockT -= dt) > 0.f) break; e->mode = TRP_WALK; pedPlay(t, e, TRC_WALK, 1); }
      const float *a = pathP(t, e->path, e->seg), *b = pathP(t, e->path, e->seg + 1);
      float tx = e->dir > 0 ? b[0] : a[0], tz = e->dir > 0 ? b[2] : a[2];
      float dx = tx - e->x, dz = tz - e->z, d = sqrtf(dx * dx + dz * dz), step = e->speed * dt;
      if (d <= step || d < .05f) { e->x = tx; e->z = tz; e->seg += e->dir; e->seg = (e->seg % (int)t->path[e->path].n + (int)t->path[e->path].n) % (int)t->path[e->path].n; }
      else { e->x += dx / d * step; e->z += dz / d * step; }
      float th = d > .05f ? atan2f(dx, dz) : e->h, dh = wrapPi(th - e->h), mx = 5.f * dt;   // поворот до 5 рад/с
      e->h = wrapPi(e->h + clampf(dh, -mx, mx));
      // PROP_FAST (аудит FPS п.9): colGround на каждого пешехода каждый кадр (≈0,2–0,4 мс PSP при 20); идущий — шаг 0,05 м/кадр, склон меняет высоту на ~0,005 м/кадр: земля раз в 4 кадра
      // (таймер «без земли» идёт по 4 dt за проверку). PED_GROUND_FAST_OLD=1 — каждый кадр
#ifndef PED_GROUND_FAST_OLD
#define PED_GROUND_FAST_OLD 0
#endif
      if (PED_GROUND_FAST_OLD || !(e->gTick++ & 3)) {
        if (!pedGround(w, e, .6f) && !PED_GROUND_OLD) { if ((e->noGroundT += dt * (PED_GROUND_FAST_OLD ? 1.f : 4.f)) > PED_NOGROUND_T) { e->mode = TRP_OFF; break; } } else e->noGroundT = 0.f;
      }
      break; }
    case TRP_FLY: {   // правка 240: шаг полёта — trafficBodyFly (тот же у персонажей миссий)
      float p[3] = {e->x, e->y, e->z}, v[3] = {e->vx, e->vy, e->vz};
      int r = trafficBodyFly(w, p, v, &e->t, dt);
      e->x = p[0]; e->y = p[1]; e->z = p[2]; e->vx = v[0]; e->vy = v[1]; e->vz = v[2];
      if (r > 0) { e->mode = TRP_DOWN; e->t = 0.f; pedPlay(t, e, TRC_LAND, 0); }
      else if (r < 0) { e->mode = TRP_OFF; }   // улетел за край мира
      break; }
    case TRP_DOWN:
      if ((e->t += dt) > PED_DOWN) { e->mode = TRP_GETUP; e->t = 0.f; pedPlay(t, e, TRC_GETUP, 0); }
      break;
    case TRP_DODGE: {   // правка 180: клип уклонения один раз, потом дальше по пути
      e->t += dt; int c = t->clip[e->model][TRC_DODGE]; float len = c >= 0 ? skinClipLength(&t->peds[e->model], c) : .8f;
      if (e->t >= len) { e->mode = e->test ? TRP_STAND : TRP_WALK; pedPlay(t, e, e->test ? TRC_IDLE : TRC_WALK, 1); }   // служебный (сценарий) — стоит
      break; }
    case TRP_GETUP: {
      e->t += dt; int c = t->clip[e->model][TRC_GETUP]; float len = c >= 0 ? skinClipLength(&t->peds[e->model], c) : 1.f;
      if (e->t >= len) { if (!e->test) pedNearest(t, e); e->mode = e->test ? TRP_STAND : TRP_WALK; pedPlay(t, e, e->test ? TRC_IDLE : TRC_WALK, 1); }
      break; }
  }
  skinTick(&e->sp, dt * (e->mode == TRP_WALK ? e->speed / t->walkV[e->model] : 1.f));
}

// ================================================================ кадр
static void parkWake(Traffic *t, TrParked *P) {
  if (P->state) return;
  int n = 0; for (int i = 0; i < t->npark; i++) n += t->park[i].state;
  if (n < TR_MAXAWAKE) { P->state = 1; P->stillT = 0.f; }
}
static int near2(const Car *a, const Car *b, float extra) {
  float dx = a->x - b->x, dz = a->z - b->z, r = carR(a) + carR(b) + extra; return dx * dx + dz * dz < r * r && fabsf(a->y - b->y) < 3.f;
}
static void hitEvents(Traffic *t, int type, int idx, Car *k, float before, float cool) {
  if (k->hitSpeed > 1.f && cool <= 0.f) pushEv(t, type, idx, k->x, k->y, k->z, HIT_CAR);
  if (k->destroyed && before < 1.f) pushEv(t, TR_EV_CAR_WRECK, idx, k->x, k->y, k->z, HIT_WRECK);
}
void trafficReset(Traffic *t, const ColWorld *w) {
  if (!t->hd) return;
  for (int i = 0; i < TR_MAXCAR; i++) t->car[i].mode = TRM_OFF;
  for (int i = 0; i < TR_MAXPED; i++) t->ped[i].mode = TRP_OFF;
  for (int i = 0; i < t->npark; i++) {
    const TrPark *k = &t->parkd[i]; TrParked *P = &t->park[i];
    carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h);
    P->state = 0; P->wreck = 0; P->stillT = 0.f; P->gone = P->off;
    if (P->off) { P->car.x = P->car.z = 1e7f; P->car.y = -1000.f; P->car.v = 0.f; }   // место пустое: машины нет
  }
  t->zoneMask = ~0u;   // правка 139: до первых ворот загрузчика загружено всё (ничего не прячем)
  t->zoneGroup = 0; t->group = t->missionPedGroup >= 0 ? t->missionPedGroup : 0; t->fresh = 1; t->scanT = t->pedScanT = 0.f; t->nev = 0; t->solidHave = 0;
}
int trafficHitStill(Traffic *t, const ColWorld *w, const Car *k) {
  if (!t->hd || !k || t->noSolid) return 0;
  int n = 0;
  for (int i = 0; i < t->maxCars; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue;
    Car s = *k;   // копия: неподвижная машина не сдвигается, толчок достаётся машине трафика
    if (near2(&c->car, &s, .3f) && carHitCar(&c->car, &s, w)) { if (c->mode == TRM_DRIVE) { c->mode = TRM_PHYS; c->stillT = 0.f; } n++; }
  }
  for (int i = 0; i < t->npark; i++) {
    TrParked *P = &t->park[i]; if (P->gone || !P->state) continue;
    Car s = *k; if (near2(&P->car, &s, .3f) && carHitCar(&P->car, &s, w)) n++;
  }
  return n;
}
#ifndef USE_PED_GROUP_OLD
#define USE_PED_GROUP_OLD 0
#endif
int trafficPedGroup(Traffic *t, int group) {
  if (!t || !t->hd || group < -1 || group >= (int)t->hd->nGroup) return 0;
  if (!USE_PED_GROUP_OLD) {
    t->missionPedGroup = group;
    t->group = group >= 0 ? group : t->zoneGroup;
  }
  return 1;
}
void trafficUpdate(Traffic *t, const ColWorld *w, Car *player, int driving, const float ppos[3],
                   const float eye[3], const float fwd[3], float dt) {
  if (!t->hd || dt <= 0.f) return;
  TP(0);
  if (dt > .1f) dt = .1f;
  t->time += dt;
  float fl = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]), fx = fl > 1e-4f ? fwd[0] / fl : 0.f, fz = fl > 1e-4f ? fwd[2] / fl : 1.f;
  float cen[2] = {ppos[0] + fx * SPAWN_LEAD, ppos[2] + fz * SPAWN_LEAD};
  // группа пешеходов: игрок в «воротах» зоны
  for (uint32_t i = 0; i < t->hd->nZone; i++) {
    const TrZone *Z = &t->zone[i]; float d[3] = {ppos[0] - Z->c[0], ppos[1] - Z->c[1], ppos[2] - Z->c[2]};
    if (fabsf(d[0] * Z->ax[0] + d[1] * Z->ax[1] + d[2] * Z->ax[2]) < Z->h[0] && fabsf(d[1]) < Z->h[1] + 2.f &&
        fabsf(d[0] * Z->az[0] + d[1] * Z->az[1] + d[2] * Z->az[2]) < Z->h[2] && (int)Z->group < (int)t->hd->nGroup) t->zoneGroup = (int)Z->group;
  }
  t->group = t->missionPedGroup >= 0 ? t->missionPedGroup : t->zoneGroup;
#if TRAFFIC_ZONE_PARK
  parkZones(t, w, ppos, eye, fwd);
#endif
  // --- удаление и спавн
  for (int i = 0; i < t->maxCars; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue;
    float dx = c->car.x - cen[0], dz = c->car.z - cen[1], d = sqrtf(dx * dx + dz * dz);
    int seen = inView(eye, fwd, c->car.x, c->car.y + 1.f, c->car.z, carR(&c->car), t->drawDist);
    if (d > REMOVE_HARD || (d > REMOVE_R && !seen) || (c->next < 0 && c->v < .1f && !seen)) c->mode = TRM_OFF;
  }
  TP(1);
  if ((t->scanT -= dt) <= 0.f || t->fresh) {
    t->scanT = .25f;
    if (trfSpawnOld) spawnCarsOld(t, w, player, ppos, cen, eye, fwd);
    else {   // правка 328: кадр камеры и кадр камеры «за игроком», направления «впереди» (взгляд камеры, ход машины)
      SpawnView V; memcpy(V.eye, eye, sizeof V.eye); memcpy(V.fwd, fwd, sizeof V.fwd);
      float dx = fx, dz = fz;
      if (driving && player) {
        float vx = player->vel[0], vz = player->vel[2], vl = sqrtf(vx * vx + vz * vz);
        if (vl > 3.f) { dx = vx / vl; dz = vz / vl; } else { dx = sinf(player->h); dz = cosf(player->h); }
      }
      V.pv[0] = driving && player ? player->vel[0] : 0.f; V.pv[1] = driving && player ? player->vel[2] : 0.f;
      V.dir[0][0] = fx; V.dir[0][1] = fz; V.dir[1][0] = dx; V.dir[1][1] = dz; V.ndir = 2;
      V.eye2[0] = ppos[0] - dx * SPAWN_CAM_BACK; V.eye2[1] = ppos[1] + 3.f; V.eye2[2] = ppos[2] - dz * SPAWN_CAM_BACK;
      V.fwd2[0] = dx; V.fwd2[1] = 0.f; V.fwd2[2] = dz;
      spawnCars(t, w, player, ppos, cen, &V);
    }
  }
  TP(2);
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF || e->test) continue;
    float dx = e->x - ppos[0], dz = e->z - ppos[2], d = sqrtf(dx * dx + dz * dz);
    if (d > PED_REMOVE * 1.5f || (d > PED_REMOVE && !inView(eye, fwd, e->x, e->y + 1.f, e->z, 1.f, t->pedDrawDist))) e->mode = TRP_OFF;
  }
  if ((t->pedScanT -= dt) <= 0.f || t->fresh) { t->pedScanT = .5f; spawnPeds(t, w, ppos, eye, fwd); }
  t->fresh = 0;
  TP(3);
  // --- препятствия для водителей (стоящие машины gapAhead смотрит сам)
  Obs obs[2 + TR_MAXPED + TR_MAXCAR + TR_MAXEXTRA]; int no = 0, carObs[TR_MAXCAR];
  if (player) obsCar(&obs[no++], player, -1);
  for (int i = 0; i < t->nextra && i < TR_MAXEXTRA; i++) if (t->extra[i]) obsCar(&obs[no++], t->extra[i], -1);   // машины миссий
  if (!driving) obsCircle(&obs[no++], ppos[0], ppos[2], HOMER_RADIUS + .5f);
  for (int i = 0; i < t->maxPeds; i++) if (t->ped[i].mode != TRP_OFF) obsCircle(&obs[no++], t->ped[i].x, t->ped[i].z, TR_PED_R + .4f);
  for (int i = 0; i < t->maxCars; i++) {
    carObs[i] = -1; if (t->car[i].mode == TRM_OFF) continue;
    carObs[i] = no; obsCar(&obs[no++], &t->car[i].car, i);
  }
  TP(4);
  // --- езда и физика
  t->activeCars = t->movingCars = 0;
  for (int i = 0; i < t->maxCars; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue;
    t->activeCars++; if (c->hitCool > 0.f) c->hitCool -= dt;
    if (c->mode == TRM_DRIVE) {
      driveStep(t, w, c, obs, no, dt);
      if (carObs[i] >= 0) obsCar(&obs[carObs[i]], &c->car, i);
      if (c->v > .5f) t->movingCars++;
    } else physStep(t, w, c, dt);
  }
  TP(5);
  // --- столкновения: игрок и трафик, трафик между собой, стоящие
  for (int i = 0; i < t->maxCars; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue;
    float dmg = c->car.damage;
    if (player && near2(player, &c->car, .5f) && carHitCar(player, &c->car, w)) {
      carRamDamage(&c->car, dmg);   // правка 185
      ramLatch(c);
      if (c->mode == TRM_DRIVE) { c->mode = TRM_PHYS; c->stillT = 0.f; }
      hitEvents(t, TR_EV_CAR_HIT, i, &c->car, dmg, c->hitCool); c->hitCool = 1.f;
    }
    for (int j = i + 1; j < t->maxCars; j++) {
      TrCar *o = &t->car[j]; if (o->mode == TRM_OFF || (c->mode == TRM_DRIVE && o->mode == TRM_DRIVE)) continue;
      if (near2(&c->car, &o->car, .3f) && carHitCar(&c->car, &o->car, w)) {
        ramLatch(c); ramLatch(o);
        if (c->mode == TRM_DRIVE) c->mode = TRM_PHYS; if (o->mode == TRM_DRIVE) o->mode = TRM_PHYS; c->stillT = o->stillT = 0.f; }
    }
    for (int j = 0; j < t->npark; j++) {
      TrParked *P = &t->park[j]; if (P->gone) continue;
      if (near2(&c->car, &P->car, .3f) && carHitCar(&c->car, &P->car, w)) { if (c->mode == TRM_DRIVE) { c->mode = TRM_PHYS; c->stillT = 0.f; } parkWake(t, P); }
    }
    carRamCapFrame(&c->car, dmg);   // №176 повтор: один удар не разбивает трафик
    if (c->car.destroyed && c->mode != TRM_WRECK) c->mode = TRM_WRECK;
  }
  TP(6);
  t->awakePark = 0;
  for (int i = 0; i < t->npark; i++) {
    TrParked *P = &t->park[i]; if (P->gone) continue;
    float dmg = P->car.damage;
    if (player && near2(player, &P->car, .5f) && carHitCar(player, &P->car, w)) {
      carRamDamage(&P->car, dmg);   // правка 185
      parkWake(t, P); if (player->hitSpeed > 1.f) pushEv(t, TR_EV_PARK_HIT, i, P->car.x, P->car.y, P->car.z, HIT_CAR);
      if (P->car.destroyed && dmg < 1.f) pushEv(t, TR_EV_CAR_WRECK, i, P->car.x, P->car.y, P->car.z, HIT_WRECK);
    }
    carRamCapFrame(&P->car, dmg);
    if (!P->state) continue;
    for (int j = 0; j < t->npark; j++)   // проснувшаяся толкает соседние
      if (j != i && !t->park[j].gone && near2(&P->car, &t->park[j].car, .3f) && carHitCar(&P->car, &t->park[j].car, w)) parkWake(t, &t->park[j]);
    CarInput in = {0.f, 0.f, 0.f, 1.f};
    carUpdate(&P->car, w, &in, dt);
    float wv = fabsf(P->car.w[0]) + fabsf(P->car.w[1]) + fabsf(P->car.w[2]);
    P->stillT = (fabsf(P->car.v) < .2f && fabsf(P->car.vl) < .2f && wv < .2f) ? P->stillT + dt : 0.f;
    if (P->stillT > 1.f) P->state = 0;
    P->wreck = P->car.destroyed; t->awakePark += P->state;
  }
  // сдвинутая стоящая машина вне кадра далеко от игрока — на место (одна за кадр)
  if (t->npark) {
    int i = t->parkRR++ % t->npark; TrParked *P = &t->park[i]; const TrPark *k = &t->parkd[i];
    float dx = P->car.x - k->x, dz = P->car.z - k->z, px = k->x - ppos[0], pz = k->z - ppos[2];
    int moved = dx * dx + dz * dz > 1.f || fabsf(wrapPi(P->car.h - k->h)) > .2f || P->car.damage > 0.f;
    if (moved && !P->off && !P->state && px * px + pz * pz > PARK_BACK * PARK_BACK && !inView(eye, fwd, P->car.x, P->car.y + 1.f, P->car.z, carR(&P->car), t->drawDist)) {
      carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h); P->wreck = 0; }
  }
  TP(7);
  // --- пешеходы
  t->activePeds = 0;
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF) continue;
    t->activePeds++;
    if (player) pedHitBy(t, i, player);
    if (e->nearCool > 0.f) e->nearCool -= dt;
    if (player && driving) pedNearMiss(t, i, player);
    for (int j = 0; j < t->maxCars; j++) if (t->car[j].mode == TRM_PHYS || t->car[j].mode == TRM_WRECK) pedHitBy(t, i, &t->car[j].car);
    pedStep(t, w, i, player, driving, ppos, dt);
  }
  TP(8);
}

// ================================================================ Гомер
static int solidCar(const Car *k, float *x, float *z, float y, float r, float *top, float *onx, float *onz) {
  float dx0 = *x - k->x, dz0 = *z - k->z, br = carR(k) + r;
  if (dx0 * dx0 + dz0 * dz0 > br * br) return 0;
  float roof = k->y + k->p.bodyHi * cosf(k->pitch) * cosf(k->roll);
  if (roof < y - .3f || k->y + k->p.bodyLo > y + HOMER_HEIGHT) return 0;          // ниже ступней / выше головы
  float nx, nz, depth;
  if (!boxCircle(k, *x, *z, r, &nx, &nz, &depth)) return 0;
  if (roof <= y + HOMER_STEP) { if (roof > *top) *top = roof; return 0; }           // крыша вровень со ступнями — опора
  *x += nx * depth; *z += nz * depth; *onx = nx; *onz = nz;
  return 1;
}
// Пешеход для Гомера — не круглый столб, вокруг которого соскальзывают (правило порта, Роб 28.09 «пешеходы проходят сквозь
// Гомера»; требование: вбежал в пешехода — остановился): вытолкнуть назад вдоль собственного шага Гомера (от прошлого
// вызова) до касания кругов; нормаль — против шага, homer.c гасит скорость вдоль него. Обойти можно, повернув стик.
// Шаг неизвестен (первый вызов, телепорт, стоит на месте) — обычное выталкивание по радиусу.
int trafficSolid(void *u, float *x, float *z, float y, float r, float *top, float *onx, float *onz) {
  Traffic *t = (Traffic *)u; int pushed = 0;
  if (!t || !t->hd || t->noSolid) return 0;
  float mx = *x - t->solidPX, mz = *z - t->solidPZ, ml = sqrtf(mx * mx + mz * mz);
  int step = t->solidHave && ml > 1e-4f && ml < .5f;
  for (int i = 0; i < t->maxCars; i++) if (t->car[i].mode != TRM_OFF) pushed |= solidCar(&t->car[i].car, x, z, y, r, top, onx, onz);
  for (int i = 0; i < t->npark; i++) if (parkLive(t, i)) pushed |= solidCar(&t->park[i].car, x, z, y, r, top, onx, onz);
  for (int i = 0; i < t->maxPeds; i++) {
    const TrPed *e = &t->ped[i]; if (!pedStands(e)) continue;
    float ht = t->pedm[e->model].height; if (ht <= 0.f) continue;
    if (e->y + ht < y + HOMER_STEP || e->y > y + HOMER_HEIGHT) continue;
    float dx = *x - e->x, dz = *z - e->z, R = r + TR_PED_R, d2 = dx * dx + dz * dz;
    if (d2 >= R * R) continue;
    float px = *x - mx, pz = *z - mz, p2 = (px - e->x) * (px - e->x) + (pz - e->z) * (pz - e->z);
    if (step && p2 >= R * R * .98f) {   // шагнул внутрь круга: назад вдоль шага до касания
      float ux = mx / ml, uz = mz / ml, b = dx * ux + dz * uz, s = b + sqrtf(fmaxf(0.f, b * b - d2 + R * R));
      *x -= ux * s; *z -= uz * s; *onx = -ux; *onz = -uz;
    } else {
      float d = sqrtf(d2), nx = d > 1e-4f ? dx / d : sinf(e->h + 3.14159265f), nz = d > 1e-4f ? dz / d : cosf(e->h + 3.14159265f);
      *x = e->x + nx * R; *z = e->z + nz * R; *onx = nx; *onz = nz;
    }
    pushed = 1;
  }
  t->solidPX = *x; t->solidPZ = *z; t->solidHave = 1;
  return pushed;
}
int trafficKick(Traffic *t, float x, float y, float z, float r, float dx, float dz) {
  int n = 0; if (!t->hd) return 0;
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (!pedStands(e)) continue;
    float ex = e->x - x, ey = e->y + .9f - y, ez = e->z - z, R = r + TR_PED_R + .3f;
    if (ex * ex + ey * ey + ez * ez > R * R) continue;
    pedFly(t, i, dx * 5.f, dz * 5.f, 3.f); pushEvW(t, TR_EV_PED_KICK, i, e->x, e->y, e->z, HIT_KICK, t->pedm[e->model].name); n++;
  }
  return n;
}
int trafficPedPlace(Traffic *t, const ColWorld *w, int m, float x, float z, float h) {
  if (!t->maxPeds || m < 0 || m >= t->npeds) return -1;
  TrPed *e = &t->ped[0]; memset(e, 0, sizeof *e);
  e->model = m; e->x = x; e->z = z; e->y = 200.f; e->h = h; e->test = 1; e->speed = t->walkV[m];
  float g; if (colGround(w, x, z, 300.f, -300.f, &g, 0)) e->y = g; else e->y = 0.f;
  skinInit(&e->sp, &t->peds[m]); e->mode = TRP_STAND; pedPlay(t, e, TRC_IDLE, 1);
  return 0;
}
int trafficPopEvent(Traffic *t, TrEvent *e) {
  if (!t->nev) return 0;
  *e = t->ev[0]; memmove(t->ev, t->ev + 1, (size_t)(t->nev - 1) * sizeof *e); t->nev--;
  return 1;
}

// ================================================================ отрисовка
static void mul4(const float a[16], const float b[16], float o[16]) {   // столбцы: o = a * b
  for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) {
    float s = 0.f; for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k]; o[c * 4 + r] = s;
  }
}
#ifndef TRF_DRIVER_OLD
#define TRF_DRIVER_OLD 0   // №666: 1 — запечённый водитель рисуется и у машины игрока (двойник с героем)
#endif
#ifndef TRF_HUSK804_OLD
#define TRF_HUSK804_OLD 0
#endif
int trHideDriver;
// №866 (Роб 23.10, не первый раз: «издалека колёса должны быть чёрные — у жёлтых такси и школьных автобусов колёса цвета кузова»): дальняя версия
// трафика и машин миссий (> 30 м, tools/traffic_export.py LOD <= 400 треуг.) — суп в позе покоя, упрощение съедает маленькие колёса, в арках цвет
// кузова. Поверх дальней версии — четыре тёмных колеса модели (восьмигранник, 28 треуг. на колесо) одной сеткой и одним вызовом; центры и радиус —
// из TrModel (wheel[4], wheelRad), скрытые колёса оригинала (carwheels.h) не рисуются. Сетка строится раз на модель. TRF_FARWHEEL866_OLD=1 — как было
#ifndef TRF_FARWHEEL866_OLD
#define TRF_FARWHEEL866_OLD 0
#endif
#define FW_SC 4.f
#define FW_N 8
typedef struct { const TrModel *M; unsigned hide; char name[16]; float key[13]; TrPiece p; unsigned char *v; } FarWheels;   // name/key — модель миссии могла лечь по тому же адресу
static FarWheels fwTab[96]; static int fwN;
// №877 (ночная долгая игра TEST49: куча после каждой смены уровня −16…21 КБ, LT696 ra trFarWheels): модели следующего уровня ложатся по новым
// адресам, записи копились до 96 (≈0,5 МБ). Смена уровня (trafficFree) отпускает все; TRF_FWFREE877_OLD=1 — как было
#ifndef TRF_FWFREE877_OLD
#define TRF_FWFREE877_OLD 0
#endif
static void trFarWheelsFree(void) { if (TRF_FWFREE877_OLD) return; for (int i = 0; i < fwN; i++) free(fwTab[i].v); memset(fwTab, 0, sizeof fwTab); fwN = 0; }
static const FarWheels *trFarWheels(const TrModel *M, unsigned hide) {
  float key[13]; key[0] = M->wheelRad; memcpy(key + 1, M->wheel, sizeof M->wheel);
  for (int i = 0; i < fwN; i++) if (fwTab[i].M == M && fwTab[i].hide == hide && !memcmp(fwTab[i].name, M->name, 16) && !memcmp(fwTab[i].key, key, sizeof key)) return fwTab[i].v ? &fwTab[i] : 0;
  for (int i = 0; i < fwN; i++) if (fwTab[i].M == M && fwTab[i].hide == hide) { FarWheels *f = &fwTab[i]; free(f->v); f->v = 0; f->M = 0; }   // устаревшая запись адреса
  FarWheels *f = fwN < 96 ? &fwTab[fwN++] : &fwTab[(unsigned)((uintptr_t)M >> 4) % 96u];
  if (f->v) free(f->v);
  memset(f, 0, sizeof *f); f->M = M; f->hide = hide; memcpy(f->name, M->name, 16); memcpy(f->key, key, sizeof key);
  float R = M->wheelRad; if (!(R > .1f && R < 1.5f)) return 0;
  float W = R * .6f; int nw = 0; for (int w = 0; w < 4; w++) if (!(hide >> w & 1) && (M->wheel[w][0] != 0.f || M->wheel[w][2] != 0.f)) nw++;
  if (!nw) return 0;
  int per = (2 * (FW_N - 2) + 2 * FW_N) * 3, nv = nw * per;
  unsigned char *v = (unsigned char *)memalign(16, (size_t)nv * 16); if (!v) return 0;
  int o = 0; float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
  for (int w = 0; w < 4; w++) { if ((hide >> w & 1) || (M->wheel[w][0] == 0.f && M->wheel[w][2] == 0.f)) continue;
    const float *c = M->wheel[w]; float ring[2][FW_N][3];
    for (int s_ = 0; s_ < 2; s_++) for (int j = 0; j < FW_N; j++) { float a = 6.2831853f * (float)j / FW_N;
      ring[s_][j][0] = c[0] + (s_ ? W : -W) * .5f; ring[s_][j][1] = c[1] + R * cosf(a); ring[s_][j][2] = c[2] + R * sinf(a); }
    #define FW_PUT(P) do { unsigned char *q_ = v + (size_t)o * 16; memset(q_, 0, 16); *(unsigned *)(q_ + 4) = 0xFF1C1C1Cu; \
        for (int e_ = 0; e_ < 3; e_++) { float x_ = (P)[e_] / FW_SC * 32767.f; ((short *)(q_ + 8))[e_] = (short)(x_ < -32767.f ? -32767 : x_ > 32767.f ? 32767 : x_); \
          if ((P)[e_] < lo[e_]) lo[e_] = (P)[e_]; if ((P)[e_] > hi[e_]) hi[e_] = (P)[e_]; } o++; } while (0)
    for (int s_ = 0; s_ < 2; s_++) for (int j = 1; j + 1 < FW_N; j++) { FW_PUT(ring[s_][0]); FW_PUT(ring[s_][s_ ? j + 1 : j]); FW_PUT(ring[s_][s_ ? j : j + 1]); }   // боковины веером
    for (int j = 0; j < FW_N; j++) { int j2 = (j + 1) % FW_N;   // протектор
      FW_PUT(ring[0][j]); FW_PUT(ring[1][j]); FW_PUT(ring[1][j2]); FW_PUT(ring[0][j]); FW_PUT(ring[1][j2]); FW_PUT(ring[0][j2]); }
    #undef FW_PUT
  }
  sceKernelDcacheWritebackRange(v, (unsigned)(nv * 16));   // GE читает вершины из памяти (урок №863)
  for (int e = 0; e < 3; e++) { f->p.bb[e] = lo[e]; f->p.bb[3 + e] = hi[e]; }
  f->p.tex = -1; f->p.flags = 16 | 8; f->p.nv = (int)nv; f->p.ni = (int)nv; f->p.owner = 1; f->p.sc = FW_SC; f->v = v;   // 16 без индексов, 8 — двусторонний
  return f;
}
void trafficDrawModel(const TrModel *M, const TrPiece *piece, const unsigned char *vert, const Car *k, float wheelAng, int lod,
                      TrDrawFn fn, void *user, int *tris) {
  float B[16]; carPieceMatrix(k, carShowHusk(k)?M->lift:carModelLift(k,M->lift), 0.f, 0.f, 0.f, 1.f, B);
  int first = lod ? M->firstLod : M->firstPiece, n = lod ? M->nLod : M->nPiece;
  // правка 354 [PS2 код SLES 0x27a140]: у frink_v, honor_v, … колёса вылеплены в кузове, куски на суставах w0..w3 — заглушки:
  // не рисуются (rocke_v — только w2, w3). Пакеты после правки их не содержат (tools/traffic_export.py), старые — пропуск здесь
  char nm[17]; memcpy(nm, M->name, 16); nm[16] = 0; unsigned hide = carWheelHide(nm);
  // Правка 500: cosf/sinf качения одни на машину (а не на каждый кусок-колесо); TRF_WHEELTRIG_OLD=1 — прежнее
#ifndef TRF_WHEELTRIG_OLD
#define TRF_WHEELTRIG_OLD 0
#endif
  float wcos = TRF_WHEELTRIG_OLD ? 0.f : cosf(-wheelAng), wsin = TRF_WHEELTRIG_OLD ? 0.f : sinf(-wheelAng);
  for (int i = first; i < first + n; i++) {
    const TrPiece *p = &piece[i]; float m[16];
    if (p->wheel && (hide >> (p->wheel - 1) & 1)) continue;
    if (!p->wheel) { float L[16] = {p->sc, 0, 0, 0, 0, p->sc, 0, 0, 0, 0, p->sc, 0, p->ox, p->oy, p->oz, 1}; mul4(B, L, m); }
    else {   // колесо: центр колеса + поворот вокруг оси X модели (качение)
      const float *wc = M->wheel[p->wheel - 1]; float c = TRF_WHEELTRIG_OLD ? cosf(-wheelAng) : wcos, s = TRF_WHEELTRIG_OLD ? sinf(-wheelAng) : wsin, sc = p->sc;   // вперёд = -Z модели
      int slot=(wc[2]<0.f?0:2)+(wc[0]>0.f?1:0);
      float dy=clampf(k->comp[slot]-k->eq,-k->eq,k->lim-k->eq)-k->modelRaise;
      float L[16] = {sc, 0, 0, 0,  0, c * sc, s * sc, 0,  0, -s * sc, c * sc, 0,
                     wc[0] + p->ox, wc[1] + dy + c * p->oy - s * p->oz, wc[2] + s * p->oy + c * p->oz, 1};
      mul4(B, L, m);
      // №867: боковины (картинка диска с нарисованным светом) — без качения, протектор — с качением (wheelcap.h)
      int vs_ = (p->flags & 32) ? 12 : 16, cap_ = (p->flags & 16) && carWheelCapStill(nm) ? wheelCapN((void *)(vert + p->off), (int)p->ni, vs_) : 0;   // №878: только диски с переливом
      if (cap_ > 0 && p->owner != 3 && !(p->owner == 4 && trHideDriver)) {
        float L0[16] = {sc, 0, 0, 0,  0, sc, 0, 0,  0, 0, sc, 0,  wc[0] + p->ox, wc[1] + dy + p->oy, wc[2] + p->oz, 1}, m0[16]; mul4(B, L0, m0);
        TrPiece q = *p; q.ni = (uint32_t)cap_; fn(user, &q, vert + p->off, m0);
        q.ni = p->ni - (uint32_t)cap_; fn(user, &q, vert + p->off + (size_t)cap_ * vs_, m); if (tris) *tris += p->ni / 3;
        continue;
      }
    }
    // №804 (Роб 19.10, TEST38: «NPC-машины после взрыва без каркаса — одни колёса»): у остова huskA кузов — один кусок с текстурой водителей
    // (char_swatches, tools/traffic_export.py метит его owner 4 — «запечённый водитель»); у машин миссий и у машины игрока водитель рисуется
    // отдельно (trHideDriver = 1) — и кузов остова пропадал вместе с «водителем». У остова водителя нет: его куски рисуются всегда. TRF_HUSK804_OLD=1 — как было
    if (p->owner == 4 && trHideDriver && !TRF_DRIVER_OLD && (TRF_HUSK804_OLD || !carShowHusk(k))) continue;   // №666: водитель запечён в кузов, у машины игрока его нет
    if (p->owner == 3) {   // №465: блик стоп-сигнала / заднего хода (tools/traffic_export.py car_flares): рисует fn, когда горит; куски без вершин старым движком не рисуются
#ifndef TRF_BRAKELIGHT_OLD
#define TRF_BRAKELIGHT_OLD 0
#endif
      int kind = (p->flags >> 8) & 3;
      if (!TRF_BRAKELIGHT_OLD && !(carShowHusk(k) || k->destroyed) && ((kind == 0 && k->brkLit > .5f) || (kind == 1 && k->v < -.3f))) fn(user, p, vert, m);
      continue;
    }
    fn(user, p, vert + p->off, m); if (tris) *tris += p->ni / 3;
  }
  if (lod && !TRF_FARWHEEL866_OLD && !carShowHusk(k) && !k->destroyed) {   // №866: тёмные колёса поверх дальней версии
    const FarWheels *fw = trFarWheels(M, hide);
    if (fw) { float m[16]; float L[16] = {FW_SC, 0, 0, 0, 0, FW_SC, 0, 0, 0, 0, FW_SC, 0, 0, 0, 0, 1}; mul4(B, L, m); fn(user, &fw->p, fw->v, m); if (tris) *tris += fw->p.ni / 3; }
  }
}
static void drawCar(Traffic *t, const Car *k, int model, float wheelAng, float d, TrDrawFn fn, void *user) {
  float ldist = (model >= 0 && model < (int)t->hd->nModel) ? trLodDist(&t->model[model]) : TR_LOD_DIST;
  int lod = t->forceLod || d > ldist, tris = 0;   // forceLod — служебный flag trflod (правка 56)
  if (lod) t->lodCars++;
  trafficDrawModel(&t->model[model], t->piece, t->vert, k, wheelAng, lod, fn, user, &tris); t->drawnTris += tris;
}
const Car *trVisCar;   // clipguard.h: машина, чей ящик проверяет vis (NULL — нет)
void trafficDrawCars(Traffic *t, const float eye[3], int (*vis)(const float bb[6]), TrDrawFn fn, void *user) {
  t->drawnCars = t->lodCars = t->drawnPark = t->drawnTris = 0;
  if (!t->hd) return;
  for (int pass = 0; pass < 2; pass++) {
    int n = pass ? t->npark : t->maxCars;
    for (int i = 0; i < n; i++) {
      const Car *k; int model; float ang = 0.f;
      if (!pass) { const TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue; k = &c->car; model = c->model; ang = c->wheelAng; }
      else { if (!parkLive(t, i)) continue; k = &t->park[i].car; model = t->parkd[i].model; }
      if (carShowHusk(k)) model = (int)t->hd->huskModel;
      float dx = k->x - eye[0], dy = k->y - eye[1], dz = k->z - eye[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
#if CULL_SPHERE_OLD
      float R = carR(k);
      if (d - R > t->drawDist) continue;
      float bb[6] = {k->x - R, k->y - 1.f, k->z - R, k->x + R, k->y + 4.f, k->z + R};
      if (vis) { trVisCar = k; int vv_ = vis(bb); trVisCar = 0; if (!vv_) continue; }
#else
      float R = carSphereR(k);
      if (d > 30.f && d - R > t->drawDist) continue;   // №546: ближе 30 м машины не снимать дальностью
      float bb[6] = {k->x - R, k->y - R, k->z - R, k->x + R, k->y + R, k->z + R};
      if (vis) { trVisCar = k; int vv_ = vis(bb); trVisCar = 0; if (!vv_) continue; }
#endif
      drawCar(t, k, model, ang, d, fn, user);
      if (pass) t->drawnPark++; else t->drawnCars++;
    }
  }
}
#ifndef PED_POSE_FAR_OLD
#define PED_POSE_FAR_OLD 0
#endif
#ifndef SKIN_HOST
extern int trafficFrameGov;   // main_state.h frameGovStep: 1 — перегрузка кадра, дальние позы реже
extern int sceGuCheckList(void);
unsigned trfProf, trfPoseUs, trfDrawUs, trfPoseN, trfWords;   // №914 служебно: разложение цены пешеходов (смоук: main_loop.h ставит trfProf = smkOn)
void trafficDrawPeds(Traffic *t, const float eye[3], int (*vis)(const float bb[6])) {
  t->drawnPeds = 0;
  if (!t->hd) return;
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF) continue;
    float dx = e->x - eye[0], dz = e->z - eye[2], d2 = dx * dx + dz * dz;
    if (d2 > t->pedDrawDist * t->pedDrawDist) continue;
    float bb[6] = {e->x - 1.f, e->y, e->z - 1.f, e->x + 1.f, e->y + 2.2f, e->z + 1.f};
    if (vis && !vis(bb)) continue;
#if PED_POSE_FAR_OLD
    if (d2 < 25.f * 25.f || !(e->poseTick++ & (trafficFrameGov ? 3 : 1))) skinPose(&e->sp);   // дальше 25 м — поза через кадр (B7: при потолке кадра каждый 4-й)
#else
    // Правка Роба 500: три яруса — до 25 м каждый кадр, 25..45 м через кадр, дальше 45 м (на экране ~10 px высотой) каждый 4-й;
    // B7 потолок кадра: при trafficFrameGov средний ярус тоже каждый 4-й
    unsigned pu0_ = trfProf ? sceKernelGetSystemTimeLow() : 0;
    if (d2 < 25.f * 25.f || (d2 < 45.f * 45.f ? !(e->poseTick++ & (trafficFrameGov ? 3 : 1)) : !(e->poseTick++ & 3))) { skinPose(&e->sp); trfPoseN++; }
#endif
    unsigned pu1_ = trfProf ? sceKernelGetSystemTimeLow() : 0; trfPoseUs += pu1_ - pu0_;
    ScePspFMatrix4 m; skinPlace(&m, e->x, e->y + skinFootLift(e->sp.m, e->sp.clip), e->z, e->h);   // №510: стопы на полу
    unsigned pw0_ = trfProf ? (unsigned)sceGuCheckList() : 0;
    skinToonNpc(1); skinDraw(&e->sp, &m); skinToonNpc(0); t->drawnPeds++;   // №931: контур прохожего тоньше с расстоянием (skin.c)
    if (trfProf) { trfDrawUs += sceKernelGetSystemTimeLow() - pu1_; trfWords += ((unsigned)sceGuCheckList() - pw0_) / 4u; }
  }
}
#endif
