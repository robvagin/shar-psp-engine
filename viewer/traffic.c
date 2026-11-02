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
// сверка g02-traffic, СВЕРКА_МАШИНЫ 8.4 (строка 42): оригинал — желаемая скорость 60 км/ч (16,7 м/с) на всех уровнях и разгон 3,5 м/с2 (запись PS2 давала 3,9 и
// 16,5 м/с по трём дорогам). TRF_SV_SPEED_OLD=1 — прежнее: скорость дороги × PACE_K, разгон 3,9
#ifndef TRF_SV_SPEED_OLD
#define TRF_SV_SPEED_OLD 0
#endif
#define ACCEL (TRAFFIC_RESTART_OLD ? 3.f : (TRF_SV_SPEED_OLD ? 3.9f : 3.5f))            // разгон трафика, м/с2
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
#define ROAD_V(t, r) (TRF_SV_SPEED_OLD ? (((t)->road[r].speed ? (t)->road[r].speed : 50) / 3.6f * PACE_K) : (60.f / 3.6f))
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

// ======================================================================================================================================
// Правила оригинала для трафика и пешеходов (02.11.2026, окно g02-traffic, правки Роба 1077–1081). Каждое правило — за выключателем TRF_SV_*_OLD (по умолчанию 0 = как у
// оригинала; 1 = прежнее поведение порта). Номер строки: docs/handoff/СВЕРКА_ВНЕДРЕНИЕ/g02-traffic.md (P03-21 = СВЕРКА_ПОЛНАЯ/P03_мир.md, «МАШИНЫ 8.1» = СВЕРКА_МАШИНЫ.md).
// Числа — по отчётам сверки и отчёту проверки; код свой, в стиле файла.
// ======================================================================================================================================
#ifndef TRF_SV_BRAKE_OLD
#define TRF_SV_BRAKE_OLD 0     // T1/T2/8.5 (22, 23, 43): коридор ±3/±2 м, глубина max(15, 1,5 v), запас по центрам 5/3 м; трафик видит трафик только на своей полосе
#endif
#ifndef TRF_SV_JUNC_OLD
#define TRF_SV_JUNC_OLD 0      // P16-01/02, Д11, 8.4 (29, 30, 59): стоп-знак (behaviour 1) = очередь дорог, 3,5 с на дорогу; без управления — не ждёт; скорость 9/12 м/с
#endif
#ifndef TRF_SV_CHOOSE_OLD
#define TRF_SV_CHOOSE_OLD 0    // T3/P16-04/Д2/Д3 (24, 31, 61, 62): выезд по номеру полосы, полоса с местом, повтор раз в 5 с, кривая на трети
#endif
#ifndef TRF_SV_LANECHG_OLD
#define TRF_SV_LANECHG_OLD 0   // 8.2/T5 (10, 26): смена полосы
#endif
#ifndef TRF_SV_DODGE_OLD
#define TRF_SV_DODGE_OLD 0     // 8.1/T4 (9, 25): уворот от машины игрока
#endif
#ifndef TRF_SV_REJOIN_OLD
#define TRF_SV_REJOIN_OLD 0    // P03-25, 8.7 (21, 44): условия возврата на полосу
#endif
#ifndef TRF_SV_HONK_OLD
#define TRF_SV_HONK_OLD 0      // P03-15, 8.3 (16, 41): ответ на гудок игрока, сигнал помехе
#endif
#ifndef TRF_SV_REMOVE_OLD
#define TRF_SV_REMOVE_OLD 0    // P03-21 (18): удаление дальше 75 м без условия видимости, вне кадра 5 с
#endif
#ifndef TRF_SV_SPAWN_OLD
#define TRF_SV_SPAWN_OLD 0     // P03-23, P18-09, P03-24 (20, 34, 55): плотность полосы, интервалы, скорость при рождении
#endif
#ifndef TRF_SV_HUSK_OLD
#define TRF_SV_HUSK_OLD 0      // P03-05 (13): остов не у всех моделей, не больше 5
#endif
#ifndef TRF_SV_DMG2_OLD
#define TRF_SV_DMG2_OLD 0      // P01-09 (11): удар не убивает, если прочность была > 2 %
#endif
#ifndef TRF_SV_PEDSPAWN_OLD
#define TRF_SV_PEDSPAWN_OLD 0  // 7.2, P03-12/17, Д8, P03-14/18, 7.9 (4, 14, 53, 58, 51, 54, 65): рождение пешехода на границе круга 45 м
#endif
#ifndef TRF_SV_PEDSPEED_OLD
#define TRF_SV_PEDSPEED_OLD 0  // 7.3 (5): 1,0 / 1,5 м/с / бег 3,0
#endif
#ifndef TRF_SV_PANIC_OLD
#define TRF_SV_PANIC_OLD 0     // 7.4, P03-16 (6, 52): паника
#endif
#ifndef TRF_SV_PEDDODGE_OLD
#define TRF_SV_PEDDODGE_OLD 0  // 7.5, P02-40 (7, 48): уворот по прогнозу на 1 с, съёживание или прыжок вбок
#endif
#ifndef TRF_SV_PEDHIT_OLD
#define TRF_SV_PEDHIT_OLD 0    // 7.6, P02-42 (8, 50): трафик сбивает пешеходов, лёгкий толчок лежащему
#endif
#ifndef TRF_SV_PEDFACE_OLD
#define TRF_SV_PEDFACE_OLD 0   // P02-37 (12): стоп лицом к игроку в 1,4 м
#endif
#ifndef TRF_SV_PEDTALK_OLD
#define TRF_SV_PEDTALK_OLD 0   // 7.8, P02-39 (39, 47): разговоры
#endif
#ifndef TRF_SV_PEDPATH_OLD
#define TRF_SV_PEDPATH_OLD 0   // P02-38 (46): точка тропы пройдена в 0,5 м
#endif
#ifndef TRF_SV_KICK_OLD
#define TRF_SV_KICK_OLD 0      // 3.1 (2): пинок задевает одного (ближайшего)
#endif
#define SV_COR_CAR 3.f         // T2: боковое смещение центра цели от оси своей машины, м: машины
#define SV_COR_PED 2.f         // пешеходы
#define SV_STOP_CAR 5.f        // T1: остановка по расстоянию между центрами: перед машиной
#define SV_STOP_PED 3.f        // перед пешеходом
#define SV_JV_STOP 9.f         // 8.4: скорость в перекрёстке со стоп-знаком (behaviour 1), м/с
#define SV_JV_FREE 12.f        // в перекрёстке типа 0 (по второму читателю P16-01 только тип 0; остальные — желаемая скорость)
#define SV_QUEUE_T 3.5f        // P16-01: раз в 3,5 с зелёный получает первая дорога очереди
#define SV_ONE_HIT_MIN .02f    // P01-09: остаток прочности меньше 2 % — «не с одного удара»
#define SV_HUSK_MAX 5          // P03-05: остовов одновременно
#ifndef TRF_SV_KICKF_OLD
#define TRF_SV_KICKF_OLD 0     // круг 5, 3.3: 1 — прежний пинок пешехода 5 м/с вбок и 3 вверх
#endif
#define SV_PED_MASS 100.f      // масса пешехода по умолчанию, кг (в данных нет)
#define SV_STEER_SIGN 1.f      // 8.1: знак руля CarInput.steer для «налево» (калибровка tests/sverka_g02)

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
#ifndef TRF_SV_ZPARK_OLD
#define TRF_SV_ZPARK_OLD 0     // круг 5, P03-19: 1 — места выбраны один раз на уровень (FNV), зона только прячет/показывает
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
#if TRAFFIC_ZONE_PARK
  if (!TRF_SV_ZPARK_OLD) for (int i = 0; i < t->npark; i++) t->park[i].off = 0;   // круг 5 (P03-19): кандидат — любое место зоны, выбор при каждой загрузке зоны (svParkReselect)
#endif
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
  { float rv = t->clip[i][TRC_RUN] >= 0 ? skinClipSpeed(m, t->clip[i][TRC_RUN]) : 0.f; t->runV[i] = rv > 1.f && rv < 8.f ? rv : 3.4f; }   // 7.3: бег 3,0 м/с — клип npd_loco_run, шаг по его скорости корня
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
// ---- сверка g02-traffic: плотность полос, занятость, право/лево (P18-09, P16-04, T3)
// Плотность дороги (слово MaxCars из данных, TRF1 TrRoad.maxCars) делится на число полос, остаток — последней (P18-09)
static int laneDens(const Traffic *t, int road, int lane) {
  const TrRoad *R = &t->road[road]; int n = R->lanes > 0 ? R->lanes : 1, d = R->maxCars;
  return lane == n - 1 ? d / n + d % n : d / n;
}
// сколько машин трафика едет по полосе или уже ведёт на неё (смена полосы, кривая перекрёстка); self не считается
static int laneCars(const Traffic *t, const TrCar *self, int road, int lane) {
  int n = 0;
  for (int i = 0; i < t->maxCars; i++) {
    const TrCar *o = &t->car[i]; if (o == self || o->mode != TRM_DRIVE) continue;
    if (o->elem == 0 ? (o->road == road && (o->lane == lane || (o->chgK > 0.f && o->chgLane == lane))) : (o->nroad == road && o->nlane == lane)) n++;
  }
  return n;
}
// какая сторона «лево»: у дороги с обратной дорогой (rev) обратные полосы лежат слева от хода; знак векторного произведения (ход × смещение) — «лево».
// Считается по всем таким парам один раз на данные: +1 или −1
static int leftSign(const Traffic *t) {
  static const TrHeader *key_; static int ls_;
  if (key_ == t->hd) return ls_;
  int votes = 0;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    const TrRoad *R = &t->road[r]; if (R->rev == 0xFFFF || R->rev >= t->hd->nRoad || !roadOk(t, (int)r) || R->nPt < 2) continue;
    float a[3], b[3], o[3]; lanePos(t, (int)r, 0, R->length * .5f, a); lanePos(t, (int)r, 0, R->length * .5f + 1.f, b);
    lanePos(t, R->rev, 0, t->road[R->rev].length * .5f, o);
    float dx = b[0] - a[0], dz = b[2] - a[2], ox = o[0] - a[0], oz = o[2] - a[2], cr = dx * oz - dz * ox;
    votes += cr > 0.f ? 1 : cr < 0.f ? -1 : 0;
  }
  key_ = t->hd; ls_ = votes >= 0 ? 1 : -1; return ls_;
}
// кривая через перекрёсток: Безье от конца полосы въезда p0 (курс d0) к началу полосы выезда (road o, lane bl); внутренние точки — на hdk доле расстояния между концами
static void bezierTo(Traffic *t, TrCar *c, int o, int bl, const float p0[3], const float d0[3], float hdk) {
  const TrPt *P = lanePts(t, o, bl); float d1[3]; laneDir(t, o, bl, 0, d1);
  float p3[3] = {P[0].x, P[0].y, P[0].z}, dx = p3[0] - p0[0], dy = p3[1] - p0[1], dz = p3[2] - p0[2];
  float d = sqrtf(dx * dx + dy * dy + dz * dz), hd = d * hdk;
  for (int k = 0; k < 3; k++) { c->bz[0][k] = p0[k]; c->bz[1][k] = p0[k] + d0[k] * hd; c->bz[2][k] = p3[k] - d1[k] * hd; c->bz[3][k] = p3[k]; }
  float prev[3], cur[3]; bzPoint(c->bz, 0.f, prev); c->bzTab[0] = 0.f;
  for (int k = 1; k <= 8; k++) {
    bzPoint(c->bz, k / 8.f, cur);
    float ex = cur[0] - prev[0], ey = cur[1] - prev[1], ez = cur[2] - prev[2];
    c->bzTab[k] = c->bzTab[k - 1] + sqrtf(ex * ex + ey * ey + ez * ez); memcpy(prev, cur, sizeof prev);
  }
  c->bzLen = c->bzTab[8]; c->nroad = o; c->nlane = bl; c->next = 1; c->junc = t->road[c->road].endJ;
}
// Выбор выезда по оригиналу (T3, P16-04, Д2, Д3). 1 — выезд выбран, кривая готова; 0 — тупик (выездов нет); −1 — выезды есть, но мест нет нигде: стоять, повтор через 5 с.
// Выезды: без разворота (направление выезда не равно обратному въезду с допуском 0,05 и это не обратная дорога). Единственный выезд — без углов, но с проверкой места.
// Иначе по номеру полосы: нулевая (и единственная) — прямо или направо с равной вероятностью, последняя — прямо или налево, средние — только прямо; берётся выезд,
// чей угол ближе к нужному повороту. Полоса выезда: тот же номер, если на ней машин меньше плотности, иначе первая со свободным местом; нет места — выезд не берётся
static int chooseNextSv(Traffic *t, TrCar *c) {
  const TrRoad *r = &t->road[c->road]; const TrJunc *j = &t->junc[r->endJ];
  float p0[3], d0[3]; lanePos(t, c->road, c->lane, laneLen(t, c->road, c->lane), p0); laneDir(t, c->road, c->lane, 1, d0);
  struct { int o, bl; float ang; } cand[16]; int n = 0, rev = -1;
  float ls = (float)leftSign(t);
  for (int k = 0; k < j->nOut && n < 16; k++) {
    int o = t->out[j->firstOut + k]; if (!roadOk(t, o)) continue;
    float d1[3]; laneDir(t, o, 0, 0, d1);
    int uturn = o == (int)r->rev || t->road[o].endJ == r->startJ || hypotf(d1[0] + d0[0], d1[2] + d0[2]) < .05f;
    if (uturn) { rev = o; continue; }
    float cr = (d0[0] * d1[2] - d0[2] * d1[0]) * ls, dt_ = d0[0] * d1[0] + d0[2] * d1[2];
    cand[n].o = o; cand[n].bl = -1; cand[n].ang = atan2f(cr, dt_); n++;   // + налево, − направо
  }
  if (!n && rev >= 0) { cand[0].o = rev; cand[0].bl = -1; cand[0].ang = 3.14159265f; n = 1; }
  if (!n) return 0;
  for (int k = 0; k < n; k++) {   // полоса выезда с местом (или −1)
    int o = cand[k].o, L = t->road[o].lanes, li = c->lane < L ? c->lane : L - 1, bl = -1;
    if (laneCars(t, c, o, li) < laneDens(t, o, li)) bl = li;
    else for (int l = 0; l < L; l++) if (laneCars(t, c, o, l) < laneDens(t, o, l)) { bl = l; break; }
    cand[k].bl = bl;
  }
  int pick = -1;
  if (n == 1) pick = cand[0].bl >= 0 ? 0 : -1;
  else {
    int last = r->lanes - 1, opt[2], no = 0; opt[no++] = 0;   // 0 — прямо, 1 — направо, 2 — налево
    if (c->lane == 0) opt[no++] = 1; else if (c->lane == last) opt[no++] = 2;
    int want = opt[(int)(rnd(t) * no) % no]; float tgt = want == 0 ? 0.f : want == 1 ? -1.5707963f : 1.5707963f, best = 1e9f;
    for (int k = 0; k < n; k++) { if (cand[k].bl < 0) continue; float e = fabsf(wrapPi(cand[k].ang - tgt)); if (e < best) { best = e; pick = k; } }
  }
  if (pick < 0) return -1;
  bezierTo(t, c, cand[pick].o, cand[pick].bl, p0, d0, 1.f / 3.f);   // Д3: внутренние точки на трети расстояния
  return 1;
}
// выбрать выезд с перекрёстка в конце дороги и кривую до ближней полосы; 0 — тупик (прежнее правило порта: случайный из всех, кроме разворота)
static int chooseNextOld(Traffic *t, TrCar *c) {
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
static int chooseNext(Traffic *t, TrCar *c) { return TRF_SV_CHOOSE_OLD ? chooseNextOld(t, c) : chooseNextSv(t, c); }
// точка маршрута машины на ds метров вперёд от s (полоса -> кривая -> следующая полоса)
static void routeAt(const Traffic *t, const TrCar *c, float ds, float p[3]) {
  float s = c->s + ds;
  if (c->elem == 0) {
    float L = laneLen(t, c->road, c->lane);
    if (s <= L || !c->next) {
      lanePos(t, c->road, c->lane, s, p);
      if (!TRF_SV_LANECHG_OLD && c->chgK > 0.f) {   // 8.2/T5: смена полосы — между своей и соседней полосой по ходу манёвра (гладкая ступень)
        float q[3], L2 = laneLen(t, c->road, c->chgLane), k = c->chgK > 1.f ? 1.f : c->chgK, u = k * k * (3.f - 2.f * k);
        lanePos(t, c->road, c->chgLane, L > 1e-3f ? s * L2 / L : s, q);
        for (int i = 0; i < 3; i++) p[i] += (q[i] - p[i]) * u;
      }
      return;
    }
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
// круг 5 (P03-19): при каждой загрузке зоны улицы — до max = (типов / 3), +1 при нечётном числе типов или если типов меньше 3, машин на случайные свободные места зоны, каждая
// с шансом «случайное по числу типов ≤ 2» (при ≤ 3 типах всегда); всего в мире не больше числа типов. Выгрузка зоны убирает её машины. Типы — модели пула с флагом стоянки
static void svParkReselect(Traffic *t) {
  int list[TR_MAXMODEL], types = 0;
  for (uint32_t i = 0; i < t->hd->nSlot; i++) if (t->slot[i].park) { int m = t->slot[i].model, dup = 0; for (int k = 0; k < types; k++) dup |= list[k] == m; if (!dup && types < TR_MAXMODEL) list[types++] = m; }
  if (!types) return;
  unsigned prev = t->zonePrev == ~0u && t->zoneMask != ~0u ? 0u : t->zonePrev;   // «загружено всё» до первых ворот — условное: первые ворота грузят свои файлы заново
  unsigned nb = t->zoneMask & ~prev; int active = 0, maxN = types / 3 + ((types & 1) || types < 3 ? 1 : 0);
  for (int i = 0; i < t->npark; i++) {
    const TrPark *k = &t->parkd[i]; TrParked *P = &t->park[i];
    if (P->sel && k->zmask && !(k->zmask & t->zoneMask)) P->sel = 0;   // зона выгружена — машина с ней
    active += P->sel;
  }
  for (int pick = 0; pick < maxN && active < types; pick++) {
    int cand[PARK_CHOOSE_MAX], nc = 0;
    for (int i = 0; i < t->npark && nc < PARK_CHOOSE_MAX; i++) { const TrPark *k = &t->parkd[i]; if (!t->park[i].sel && ((k->zmask && (k->zmask & nb)) || (!k->zmask && !prev))) cand[nc++] = i; }
    if (!nc) break;
    if ((int)(rnd(t) * types) % types > 2) continue;   // шанс «случайное по числу типов ≤ 2»
    t->park[cand[(int)(rnd(t) * nc) % nc]].sel = 1; active++;
  }
}
static void parkZones(Traffic *t, const ColWorld *w, const float ppos[3], const float eye[3], const float fwd[3]) {
  for (int i = 0; i < t->nLoader; i++) {
    const TrZone *Z = &t->loader[i]; float d[3] = {ppos[0] - Z->c[0], ppos[1] - Z->c[1], ppos[2] - Z->c[2]};
    if (fabsf(d[0] * Z->ax[0] + d[1] * Z->ax[1] + d[2] * Z->ax[2]) < Z->h[0] && fabsf(d[1]) < Z->h[1] + 2.f &&
        fabsf(d[0] * Z->az[0] + d[1] * Z->az[1] + d[2] * Z->az[2]) < Z->h[2]) { t->zoneMask = Z->group; break; }
  }
  if (!TRF_SV_ZPARK_OLD && t->zoneMask != t->zonePrev) { svParkReselect(t); t->zonePrev = t->zoneMask; }
  for (int i = 0; i < t->npark; i++) {
    const TrPark *k = &t->parkd[i]; TrParked *P = &t->park[i]; if (P->off) continue;
    int want = TRF_SV_ZPARK_OLD ? (!k->zmask || (k->zmask & t->zoneMask)) : (P->sel && (!k->zmask || (k->zmask & t->zoneMask)));
    if (want == !P->gone || P->state) continue;
#ifndef TRZ_PLANT_NOVIEW   // красный контроль гейта tests/parkzone: без проверки кадра
    float R = carR(&P->car);
    if (inView(eye, fwd, P->car.x, P->car.y + 1.f, P->car.z, R, t->drawDist) || inView(eye, fwd, k->x, k->y + 1.f, k->z, R, t->drawDist)) continue;
#else
    (void)eye; (void)fwd;
#endif
    if (!want) { carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h); P->car.svKind = 4; P->wreck = 0; P->stillT = 0.f; }
    P->gone = !want;
  }
}
#endif
static void bodyCenter(const Car *k, float *x, float *z) { *x = k->x + sinf(k->h) * k->p.bodyOfs; *z = k->z + cosf(k->h) * k->p.bodyOfs; }

// ближайшее препятствие впереди по ходу (машины, стоящие, игрок, Гомер пешком, пешеходы): зазор, м
// препятствие: круг (Гомер, пешеход) или кузов-прямоугольник (машины: торчащий в полосу зад стоящей — тоже препятствие)
enum { OBK_NONE, OBK_PLAYER, OBK_MCAR, OBK_HOMER, OBK_PED, OBK_TRAF, OBK_PARK };   // сверка g02-traffic: вид препятствия (T2: свои правила коридора)
typedef struct { float x, z, r, fx, fz, hw, hl; int own, box, kind; } Obs;   // own — номер машины трафика (-1 прочие): себя не видит; kind — OBK_*
static void obsCar(Obs *o, const Car *k, int own) {
  CarBox b; carBox(k, &b); o->x = b.x; o->z = b.z; o->fx = b.fx; o->fz = b.fz; o->hw = b.hw; o->hl = b.hl; o->r = 0.f; o->own = own; o->box = 1; o->kind = OBK_NONE;
}
static void obsCircle(Obs *o, float x, float z, float r) { o->x = x; o->z = z; o->r = r; o->own = -1; o->box = 0; o->kind = OBK_NONE; }
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
// ---- сверка g02-traffic: что видит машина впереди (T1, T2, 8.5, 8.3), очередь стоп-знака (P16-01, Д11), уворот (8.1), смена полосы (8.2/T5)
static const Car *svPlayerCar;   // машина игрока этого кадра (ставит trafficUpdate): уворот смотрит её скорость
typedef struct { float rem, playerD, pedD, dist, homerD; int blocked; } SvLook;   // dist — расстояние между центрами до помехи, которая даёт rem; homerD — до Гомера пешком   // rem — расстояние до остановки по центрам (запас вычтен), м; playerD/pedD — до машины игрока / пешехода в коридоре
// та же ли полоса: свой путь (полоса, кривая) совпадает с путём другой машины трафика
static int sameLane(const TrCar *c, const TrCar *o) {
  if (o->mode != TRM_DRIVE) return 1;   // сбитая и разбитая лежит на дороге — препятствие для любой
  if (c->elem == 0 && o->elem == 0) return (c->road == o->road && (c->lane == o->lane || (o->chgK > 0.f && o->chgLane == c->lane) || (c->chgK > 0.f && c->chgLane == o->lane))) || (c->next > 0 && o->road == c->nroad && o->lane == c->nlane);
  if (c->elem == 1 && o->elem == 0) return o->road == c->nroad && o->lane == c->nlane;
  if (c->elem == 0 && o->elem == 1) return c->next > 0 && o->nroad == c->road && o->nlane == c->lane;
  return o->nroad == c->nroad && o->nlane == c->nlane;
}
// T1/T2/8.5: цели в коридоре впереди: боковое смещение центра ≤ 3 м (машина), ≤ 2 м (пешеход, Гомер); глубина max(15 м, 1,5 с × скорость) по расстоянию до центра;
// запас остановки по центрам: машина 5,0 м, пешеход 3,0 м; трафик видит трафик только на своей полосе; на кривой перекрёстка — по точкам маршрута вперёд (pathGap)
static float svLook(const Traffic *t, const TrCar *c, const Obs *obs, int nobs, const Obs *pk, int npk, SvLook *out) {
  const Car *k = &c->car; float fx = sinf(k->h), fz = cosf(k->h), bx, bz, rem = 1e9f, depth = fmaxf(15.f, 1.5f * c->v);
  int self = (int)(c - t->car); bodyCenter(k, &bx, &bz);
  out->playerD = out->pedD = out->homerD = 1e9f; out->dist = 1e9f; out->blocked = 0;
  for (int pass = 0; pass < 2; pass++) {
    int n = pass ? npk : nobs;
    for (int i = 0; i < n; i++) {
      const Obs *o = pass ? &pk[i] : &obs[i]; if (o->own == self) continue;
      if (o->kind == OBK_TRAF && o->own >= 0 && o->own < t->maxCars && !sameLane(c, &t->car[o->own])) continue;
      float dx = o->x - bx, dz = o->z - bz, a = dx * fx + dz * fz, l = -dx * fz + dz * fx, d = sqrtf(dx * dx + dz * dz);
      int ped = !o->box; float lim = ped ? SV_COR_PED : SV_COR_CAR;
      if (a <= 0.f || fabsf(l) > lim || d > depth) continue;
      float r = d - (ped ? SV_STOP_PED : SV_STOP_CAR); if (r < rem) { rem = r; out->dist = d; }
      if (o->kind == OBK_PLAYER) out->playerD = fminf(out->playerD, d);
      if (o->kind == OBK_HOMER) out->homerD = fminf(out->homerD, d);   // круг 3 (8.3): сигналит только игроку-машине и Гомеру пешком, не прохожим
      if (ped) out->pedD = fminf(out->pedD, d);
    }
  }
  if (!TRF_SV_BRAKE_OLD) {   // страховка PSP: длинная машина игрока/миссии/стоящая, чей кузов торчит в нашу полосу, хотя центр дальше 3 м от оси (поперёк дороги) — по кузову, как раньше
    for (int pass = 0; pass < 2; pass++) {
      int n = pass ? npk : nobs;
      for (int i = 0; i < n; i++) {
        const Obs *o = pass ? &pk[i] : &obs[i]; if (o->own == self || o->kind == OBK_TRAF || o->kind == OBK_NONE) continue;
        { float ex = o->x - bx, ez = o->z - bz; if (!o->box || fabsf(-ex * fz + ez * fx) <= SV_COR_CAR) continue; }   // центр в коридоре — уже учтён по правилу оригинала
        float g = gapOne(k, bx, bz, fx, fz, o); if (g > 1e8f || g > depth) continue;
        float r = g - 1.5f; if (r < rem) rem = r;
        if (o->kind == OBK_PLAYER) out->playerD = fminf(out->playerD, g + k->p.halfL);
      }
    }
  }
  if (c->elem == 1 || (c->next > 0 && laneLen(t, c->road, c->lane) - c->s < depth)) {   // кривая перекрёстка: препятствия на маршруте (круги кузова по точкам маршрута)
    Obs f2[2 + TR_MAXPED + TR_MAXCAR + TR_MAXEXTRA]; int nf = 0;
    for (int i = 0; i < nobs && nf < (int)(sizeof f2 / sizeof f2[0]); i++) {
      if (obs[i].kind == OBK_TRAF && obs[i].own >= 0 && obs[i].own < t->maxCars && !sameLane(c, &t->car[obs[i].own])) continue;
      f2[nf++] = obs[i]; }
    float pg = pathGap(t, c, f2, nf, pk, npk);
    if (pg < 1e8f) { float r = pg + k->p.halfL + k->p.bodyOfs - SV_STOP_CAR; if (r < rem) rem = r; }
  }
  out->rem = rem; return rem;
}
// P16-01 / Д11: очередь дорог на стоп-знаке перекрёстка jn (одна запись на дорогу)
static void svQueueAdd(Traffic *t, int jn, int road) {
  if ((unsigned)jn >= 256u) return;
  for (int i = 0; i < t->jq[jn].n; i++) if (t->jq[jn].q[i] == road) return;
  if (t->jq[jn].n < 8 && road < 127) t->jq[jn].q[t->jq[jn].n++] = (signed char)road;
}
// раз в кадр: очередь пуста — таймер заводится заново на 3,5 с; иначе раз в 3,5 с первая дорога очереди получает зелёный: передние машины её полос трогаются (relJ)
static void svQueueStep(Traffic *t, float dt) {
  if (TRF_SV_JUNC_OLD) return;
  for (uint32_t j = 0; j < t->hd->nJunc && j < 256u; j++) {
    if (t->junc[j].behaviour != 1) continue;
    if (!t->jq[j].n) { t->jq[j].tm = SV_QUEUE_T; continue; }
    if ((t->jq[j].tm -= dt) > 0.f) continue;
    t->jq[j].tm = SV_QUEUE_T;
    int road = t->jq[j].q[0]; memmove(t->jq[j].q, t->jq[j].q + 1, (size_t)(t->jq[j].n - 1)); t->jq[j].n--;
    for (int i = 0; i < t->maxCars; i++) {
      TrCar *c = &t->car[i];
      if (c->mode == TRM_DRIVE && c->elem == 0 && c->road == road && t->road[road].endJ == j && laneLen(t, road, c->lane) - c->s < 12.f) c->relJ = (int)j + 1;
    }
  }
}
// 8.3: сигнал помехе — машина игрока ближе 10 м (пешеход ближе 8 м) в коридоре: первый раз сразу, дальше раз в 5 с
static void svHonk(Traffic *t, TrCar *c, const SvLook *lk) {
  int kind = lk->playerD < 10.f ? 1 : lk->homerD < 8.f ? 2 : 0;   // круг 3 (8.3): сигналят машине игрока ближе 10 м и Гомеру пешком ближе 8 м, остальным пешеходам — нет
  if (kind != c->honkKind) { c->honkKind = kind; if (kind) c->honkT = 0.f; }   // новый объект — сигнал сразу; тот же — раз в 5 с
  if (kind == 1) c->honkedP = 1;
  if (!kind || c->honkT > 0.f) return;
  pushEv(t, TR_EV_CAR_HONK, (int)(c - t->car), c->car.x, c->car.y, c->car.z, 0.f); pushEv(t, TR_EV_CAR_WAY, (int)(c - t->car), c->car.x, c->car.y, c->car.z, 0.f);   // и голос «пробка» (CarWay, g08 P09:3)
  c->honkT = 5.f;
}
// 8.2/T5: можно ли сменить полосу c на lane2; Lm — длина манёвра
static int svLaneFree(const Traffic *t, const TrCar *c, int lane2, float Lm) {
  if (lane2 < 0 || lane2 >= t->road[c->road].lanes) return 0;
  if (laneCars(t, c, c->road, lane2) >= laneDens(t, c->road, lane2)) return 0;   // на соседней машин меньше плотности полосы
  float lim = 2.f * Lm; lim *= lim;
  for (int i = 0; i < t->maxCars; i++) {
    const TrCar *o = &t->car[i]; if (o == c || o->mode == TRM_OFF) continue;
    if (o->mode == TRM_DRIVE && o->elem == 0 && o->road == c->road && (o->lane == lane2 || (o->chgK > 0.f && o->chgLane == lane2))) {
      float dx = o->car.x - c->car.x, dz = o->car.z - c->car.z; if (dx * dx + dz * dz < lim) return 0; }   // ни одной не ближе 2 длин манёвра
  }
  return 1;
}
static int svStartChange(Traffic *t, TrCar *c, float Lm) {
  int n = t->road[c->road].lanes; if (n < 2 || c->elem != 0 || c->next != 0 || c->v < .5f || c->chgK > 0.f) return 0;
  float L = laneLen(t, c->road, c->lane); if (L - c->s < Lm + 30.f) return 0;   // впереди должно хватать дороги до выбора выезда
  int first = rnd(t) < .5f ? -1 : 1;
  for (int k = 0; k < 2; k++) {
    int lane2 = c->lane + (k ? -first : first);
    if (!svLaneFree(t, c, lane2, Lm)) continue;
    c->chgLane = lane2; c->chgK = 1e-4f; c->chgDur = clampf(Lm / fmaxf(c->v, 1.f), .4f, 6.f); return 1;   // время манёвра = длина манёвра / скорость (круг 3)
  }
  return 0;
}
static void svChangeStep(Traffic *t, TrCar *c, float dt) {
  if (c->chgK <= 0.f) return;
  c->chgK += dt / c->chgDur;
  if (c->chgK >= 1.f) {   // манёвр закончен: своя полоса — новая, путь пересчитан по длине полосы; следующий бросок не раньше чем через 5 с
    float L1 = laneLen(t, c->road, c->lane), L2 = laneLen(t, c->road, c->chgLane);
    c->s = L1 > 1e-3f ? c->s * L2 / L1 : c->s; c->lane = c->chgLane; c->chgK = 0.f; c->rearOk = 0;   // пауза 5 с считается от броска, не от конца манёвра (круг 3)
  }
}
// 8.1/T4: уворот от машины игрока: перед нами именно она, дальше 10 м, игрок быстрее 40 км/ч (11,1 м/с), мы быстрее 30 км/ч (8,3 м/с), линия игрока (3 м шириной,
// 1,5 с вперёд) накрывает нас. Возврат 1: поворачиваем в сторону от игрока (полный газ и руль 0,8 с, дальний свет мигает), потом ручник — стоим как физическая машина
static int svDodge(Traffic *t, TrCar *c, const SvLook *lk) {
  const Car *P = svPlayerCar; if (!P || TRF_SV_DODGE_OLD || c->v < 8.33f || lk->playerD > 1e8f || lk->playerD <= 10.f || c->honkedP) return 0;   // сигналили игроку с ближней дистанции — на него не увёртывается
  float pvx = P->vel[0], pvz = P->vel[2], pv = sqrtf(pvx * pvx + pvz * pvz); if (pv < 11.1f) return 0;
  float bx, bz; bodyCenter(&c->car, &bx, &bz);
  // круг 3 (отчёт проверки): коридор — от игрока вдоль его скорости, поперёк ±3,0 м, вперёд — скорость самой машины трафика × 1,5 с по прямой от игрока, без минимума
  float ux = pvx / pv, uz = pvz / pv, rx = bx - P->x, rz = bz - P->z, ap = rx * ux + rz * uz, lp = -rx * uz + rz * ux;
  if (ap <= 0.f || sqrtf(rx * rx + rz * rz) > c->v * 1.5f || fabsf(lp) > 3.f) return 0;
  float fx = sinf(c->car.h), fz = cosf(c->car.h), side = (P->x - bx) * -fz + (P->z - bz) * fx;   // + : игрок с той стороны, куда указывает (−fz, fx)
  c->dodgeSide = side > 0.f ? -1 : 1; c->dodgeT = .8f; c->flashT = .8f; c->dodged = 1; c->dodgeHb = 0; c->mode = TRM_PHYS; c->stillT = 0.f; c->honkDone = 1;   // дальний свет — только эти 0,8 с; запрета возврата нет
  (void)t; return 1;
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
  if (c->retryT > 0.f) c->retryT -= dt;
  if (c->honkT > 0.f) c->honkT -= dt;
  if (c->chgCool > 0.f) c->chgCool -= dt;
  if (c->flashT > 0.f) c->flashT -= dt;
  if (c->elem == 0 && !c->next && c->retryT <= 0.f && L - c->s < decide) {
    int rc = chooseNext(t, c);
    if (rc == 0) c->next = -1;                  // тупик: стоп у конца, удалится вне кадра
    else if (rc < 0) c->retryT = 5.f;           // Д2: мест на выездах нет — стоим у конца дороги, выбор повторяется раз в 5 с
  }
  float vRoad = ROAD_V(t, c->road);
  float vt = vRoad;
  // скорость в перекрёстке (8.4): стоп-знак (тип 1) 9 м/с, тип 0 — 12, остальные — желаемая; к кривой подходит с тормозным путём DECEL (строка 42)
  float vJ = vRoad;
  if (!TRF_SV_JUNC_OLD && c->junc >= 0 && c->junc < (int)t->hd->nJunc) { int bj = (int)t->junc[c->junc].behaviour; vJ = bj == 1 ? SV_JV_STOP : bj == 0 ? SV_JV_FREE : vRoad; }
  // поворот: изменение курса на 10 м вперёд -> радиус
  { float a[3], b[3], e[3]; routeAt(t, c, 0.f, a); routeAt(t, c, 5.f, b); routeAt(t, c, 10.f, e);
    float h1 = atan2f(b[0] - a[0], b[2] - a[2]), h2 = atan2f(e[0] - b[0], e[2] - b[2]), da = fabsf(wrapPi(h2 - h1));
    if (TRF_SV_JUNC_OLD) {
      float lg = c->elem == 0 && L - c->s > 10.f ? LAT_G_FIT : LAT_G;   // изгиб полосы (записан) / кривая перекрёстка (не записана — прежнее)
      if (da > .02f) vt = fminf(vt, sqrtf(lg * 5.f / da));
    } else if (da > .02f && c->elem == 0 && L - c->s > 10.f) vt = fminf(vt, sqrtf(LAT_G_FIT * 5.f / da));   // изгиб полосы; кривую перекрёстка ведёт скорость в перекрёстке vJ
  }
  if (!TRF_SV_JUNC_OLD) {
    if (c->elem == 1) vt = fminf(vt, vJ);
    else if (c->next > 0 && vJ < vRoad - .5f && c->junc >= 0 && t->junc[c->junc].behaviour == 1) vt = fminf(vt, sqrtf(vJ * vJ + 2.f * DECEL * fmaxf(0.f, L - c->s)));   // круг 3: подъезд тормозит только к стоп-знаку; к типу 0 едет на полной и сбрасывает уже в перекрёстке
  }
  // перекрёсток: стоп передним бампером у края (behaviour 1), занятый — ждать (прежнее); с g02-traffic: стоп-знак = очередь дорог, остальные без ожидания
  float stopS = 1e9f;
  int waiting = !TRF_SV_CHOOSE_OLD && c->elem == 0 && c->next == 0 && c->retryT > 0.f;   // выезда с местом нет
  if (c->elem == 0 && (c->next != 0 || waiting)) {
    float sl = brakeLine(t, c); if (c->s > sl + .05f) sl = stopLine(t, c);   // №875: проехал линию у зебры — стоп у прежней
    float toStop = sl - c->s;
    int jn = c->next > 0 ? c->junc : r->endJ, beh1 = t->junc[jn].behaviour == 1, mayGo;
    if (TRF_SV_JUNC_OLD) {
      int busy = c->next > 0 && juncBusy(t, c, jn);
      mayGo = c->next > 0 && (!busy || (TRAFFIC_JUNC_OLD && c->waitT > JAM_WAIT)) && (!beh1 || c->waitT >= STOP_WAIT);
    } else mayGo = c->next > 0 && (!beh1 || c->relJ == jn + 1);   // P16-01/02: без управления не ждёт; стоп-знак — пока дорога не получит зелёный
    if (!mayGo && toStop > -.05f) {
      stopS = sl; vt = fminf(vt, sqrtf(2.f * DECEL * fmaxf(0.f, toStop - .2f)));
      if (toStop < 1.5f && c->v < .6f) { c->waitT += dt; if (!TRF_SV_JUNC_OLD && beh1 && c->next > 0) svQueueAdd(t, jn, c->road); }
    }
  }
  float svBrakeRem = 1e9f;   // круг 3: остаток до цели торможения (центры минус запас), 1e9 — помехи нет
  Obs pk[24]; int npk = 0;   // стоящие машины рядом — для пути впереди
  for (int i = 0; i < t->npark && npk < 24; i++) {
    if (!parkLive(t, i)) continue;
    const Car *p = &t->park[i].car; float dx = p->x - c->car.x, dz = p->z - c->car.z;
    if (dx * dx + dz * dz < 25.f * 25.f) { obsCar(&pk[npk], p, -1); pk[npk++].kind = OBK_PARK; }
  }
  if (TRF_SV_BRAKE_OLD) {
    float gap = fminf(gapAhead(t, c, obs, nobs), pathGap(t, c, obs, nobs, pk, npk) + STOP_GAP - 1.f);
    if (gap < 1e8f) vt = fminf(vt, sqrtf(2.f * DECEL * fmaxf(0.f, gap - STOP_GAP)));
  } else {
    SvLook lk; float rem = svLook(t, c, obs, nobs, pk, npk, &lk);
    if (lk.playerD > 1e8f) c->honkedP = 0;
    if (!TRF_SV_HONK_OLD) svHonk(t, c, &lk);
    if (rem < 1e8f && svDodge(t, c, &lk)) return;   // 8.1: ушли в физику (газ, руль, ручник)
    // 8.2/T5 (круг 3): помеха впереди — смена полосы пробуется КАЖДЫЙ кадр без паузы, но только если расстояние между центрами минус 6 м не меньше 15 м (цель дальше 21 м);
    // длина манёвра = это расстояние. Ближе 21 м смены нет — машина тормозит. Торможение по помехе при манёвре оставлено как страховка PSP
    if (rem < 1e8f && !TRF_SV_LANECHG_OLD && c->chgK <= 0.f && c->elem == 0 && c->next == 0 && lk.dist >= 21.f) {
      if (svStartChange(t, c, lk.dist - 6.f)) rem = 1e9f;
    }
    // T1/8.5 (круг 3): торможение равномерное, замедление v² / (2 × остаток) каждый кадр; остаток ≤ 0 — нуль. Сглаживание историей позиций (0,33 с, P01-30) не делалось:
    // у цели скорость не больше остаток / 0,17 с, поэтому остановка приходит ровно в точку запаса без скачка (правило порта вместо «нуля на 0,33 × v + 1 м»)
    if (rem < 1e8f) { svBrakeRem = rem; }
  }
  // 8.2: бросок на смену полосы раз в 5 с, шанс 1/2, если помех нет (не на перекрёстке, дорога с двумя и более полосами, машина движется); пауза — только у этих бросков, таймер сбрасывает бросок
  if (!TRF_SV_LANECHG_OLD && c->chgK <= 0.f && c->elem == 0 && c->next == 0 && c->v > .5f && t->road[c->road].lanes > 1 && svBrakeRem > 1e8f) {
    if (c->chgCool <= 0.f) { c->chgCool = 5.f; if (rnd(t) < .5f) svStartChange(t, c, 15.f); }
  }
  if (!TRF_SV_LANECHG_OLD) svChangeStep(t, c, dt);
  c->vt = vt;
  c->car.brkLit = (vt < c->v - .3f || (c->v < .3f && vt < .1f)) ? 1.f : 0.f;   // №465: тормозит или ждёт у стоп-линии — стоп-сигналы горят
  float dmax = (!TRF_SV_SPEED_OLD && !TRF_SV_JUNC_OLD && c->elem == 1) ? 10.f : 2.f * DECEL;   // круг 3 (8.4): в перекрёстке скорость сбрасывается на −10 м/с²
  float dv = clampf(vt - c->v, -dmax * dt, ACCEL * dt); c->v = fmaxf(0.f, c->v + dv);
  if (!TRF_SV_BRAKE_OLD && svBrakeRem < 1e8f) {
    if (svBrakeRem <= .02f) c->v = 0.f;
    else { c->v = fmaxf(0.f, c->v - c->v * c->v / (2.f * svBrakeRem) * dt); c->v = fminf(c->v, svBrakeRem / .17f); }
    if (c->v < c->vt - .3f || svBrakeRem < 3.f) c->car.brkLit = 1.f;
  }
  if (c->elem == 0 && c->next <= 0 && c->s + c->v * dt > L) c->v = 0.f;   // тупик / выезд не выбран
  if (c->s < stopS && c->s + c->v * dt > stopS) { c->s = stopS; c->v = 0.f; }   // за стоп-линию без разрешения — нет
  c->s += c->v * dt;
  if (c->elem == 0 && c->next > 0 && c->s >= L) { c->s -= L; c->elem = 1; c->waitT = 0.f; c->chgK = 0.f; }
  if (c->elem == 1 && c->s >= c->bzLen) { c->s -= c->bzLen; c->elem = 0; c->road = c->nroad; c->lane = c->nlane; c->next = 0; c->junc = -1; c->relJ = 0; }
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
static int nearestLaneH(const Traffic *t, float x, float z, float h, float maxd2, float headTol, int *road, int *lane, float *sOut) {
  float best = maxd2; int br = -1, bl = 0; float bs = 0.f;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadOk(t, (int)r)) continue;
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int i = 0; i + 1 < n; i++) {
        float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
        float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f);
        float qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
        if (d2 >= best || fabsf(wrapPi(atan2f(ax, az) - h)) > headTol) continue;
        best = d2; br = (int)r; bl = l; bs = P[i].s + (P[i + 1].s - P[i].s) * u;
      }
    }
  }
  if (br < 0) return 0;
  *road = br; *lane = bl; *sOut = bs; return 1;
}
static int nearestLane(const Traffic *t, float x, float z, float h, float maxd2, int *road, int *lane, float *sOut) {
  return nearestLaneH(t, x, z, h, maxd2, 1.05f, road, lane, sOut);
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
// ---- сверка g02-traffic, P16-06 (строка 1): навигация целиком. Путь по дорогам считается ОДИН раз при старте цели (без множителя) и дальше не пересчитывается:
// свернувший не туда видит красные стрелки позади и зелёные впереди по-прежнему по старому пути. Зелёная стрелка — на каждом перекрёстке пути впереди игрока (смотрит на
// следующую дорогу), красная «не туда» — на пройденных; игрок ближе 10,95 м (квадрат < 120) к предыдущему перекрёстку — тот ещё впереди. Плюс на той одной дороге пути, где
// игрок стоит: целая часть от (длина / 40 − 1), от 0 до 5 стрелок через 40 м от начала дороги по её собственному направлению; красная — дальше ~11 м от игрока и позади него;
// за целью на дороге цели не ставятся. Тип цели «neither» — стрелок нет вовсе (вызывающий не зовёт Build). Рисует и зовёт msn_view.c / game_msn.c (передано);
// TRF_SV_NAV_OLD=1 — прежнее поведение живёт в trafficRouteArrow, эти две функции просто не вызывают
#ifndef TRF_SV_NAV_OLD
#define TRF_SV_NAV_OLD 0   // P16-06: 1 — Build ничего не строит (прежняя стрелка у ближайшего перекрёстка живёт в trafficRouteArrow)
#endif
static float navRoadDist(const Traffic *t, int road, float x, float z, float *sOut) {
  float best = 1e30f, bs = 0.f; const TrPt *P = lanePts(t, road, 0); int n = t->road[road].nPt;
  for (int i = 0; i + 1 < n; i++) {
    float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
    float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f), qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
    if (d2 < best) { best = d2; bs = P[i].s + (P[i + 1].s - P[i].s) * u; }
  }
  if (sOut) *sOut = bs; return sqrtf(best);
}
int trafficNavBuild(Traffic *t, float px, float pz, float ph, float tx, float tz) {
  if (TRF_SV_NAV_OLD || !t || !t->hd || !t->hd->nJunc) return 0;
  t->nav.n = 0; t->nav.goalRoad = -1; t->nav.tx = tx; t->nav.tz = tz;
  int r0 = nearRoad(t, px, pz, 12.f, 1, ph, 0), rg = nearRoad(t, tx, tz, 60.f, 0, 0.f, 0);
  if (r0 < 0 || rg < 0) return 0;
  t->nav.goalRoad = rg;
  if (r0 == rg) { t->nav.road[t->nav.n++] = (short)r0; return 1; }
  enum { NJ = 256 }; unsigned nj = t->hd->nJunc < NJ ? t->hd->nJunc : NJ;
  float dist[NJ]; short via[NJ]; unsigned char done[NJ];
  for (unsigned j = 0; j < nj; j++) { dist[j] = 1e30f; via[j] = -1; done[j] = 0; }
  unsigned j0 = t->road[r0].endJ, jg = t->road[rg].startJ; if (j0 >= nj || jg >= nj) return 0;
  dist[j0] = 0.f;
  for (;;) {
    unsigned u = nj; float bd = 1e30f;
    for (unsigned j = 0; j < nj; j++) if (!done[j] && dist[j] < bd) { bd = dist[j]; u = j; }
    if (u == nj || u == jg) break;
    done[u] = 1;
    const TrJunc *J = &t->junc[u];
    for (int k = 0; k < J->nOut; k++) { int o = t->out[J->firstOut + k]; const TrRoad *R = &t->road[o];
      if ((R->flags & 1) || R->endJ >= nj) continue;
      float nd = dist[u] + R->length; if (nd < dist[R->endJ]) { dist[R->endJ] = nd; via[R->endJ] = (short)o; } }
  }
  if (jg != j0 && via[jg] < 0) return 0;
  short rev[48]; int nr = 0;   // от цели назад до выезда из j0
  for (unsigned j = jg, guard = 0; j != j0 && guard < nj && nr < 46; guard++) { int o = via[j]; if (o < 0) break; rev[nr++] = (short)o; j = t->road[o].startJ; }
  t->nav.road[t->nav.n++] = (short)r0;
  for (int i = nr - 1; i >= 0 && t->nav.n < 46; i--) t->nav.road[t->nav.n++] = rev[i];
  if (t->nav.road[t->nav.n - 1] != rg) t->nav.road[t->nav.n++] = (short)rg;
  return 1;
}
int trafficNavArrows(const Traffic *t, float px, float pz, TrNavArrow *out, int max) {
  if (!t || !t->hd || t->nav.n <= 0 || max <= 0) return 0;
  int n = t->nav.n, idx = 0, no = 0; float bd = 1e30f, sPl = 0.f;
  for (int i = 0; i < n; i++) { float s, d = navRoadDist(t, t->nav.road[i], px, pz, &s); if (d < bd) { bd = d; idx = i; sPl = s; } }
  for (int i = 0; i + 1 < n && no < max; i++) {   // перекрёстки пути: конец дороги i
    int rn = t->nav.road[i + 1]; const TrJunc *J = &t->junc[t->road[t->nav.road[i]].endJ];
    float dx = J->x - px, dz = J->z - pz, d2 = dx * dx + dz * dz;
    int ahead = i >= idx || d2 < 120.f;
    float a[3], b[3], p[3]; roadMid(t, rn, 6.f, p); roadMid(t, rn, 0.f, a); roadMid(t, rn, 12.f, b);
    memcpy(out[no].p, p, sizeof p); out[no].h = atan2f(b[0] - a[0], b[2] - a[2]); out[no].red = !ahead; no++;
  }
  { int r = t->nav.road[idx]; float len = t->road[r].length; int cnt = (int)(len / 40.f - 1.f); cnt = cnt < 0 ? 0 : cnt > 5 ? 5 : cnt;
    float sGoal = 1e30f; if (r == t->nav.goalRoad) navRoadDist(t, r, t->nav.tx, t->nav.tz, &sGoal);
    for (int j = 1; j <= cnt && no < max; j++) {
      float s = j * 40.f; if (s > sGoal) break;   // за целью на её дороге — нет
      float a[3], b[3], p[3]; roadMid(t, r, s, p); roadMid(t, r, s + 0.f, a); roadMid(t, r, s + 8.f, b);
      float dxx = p[0] - px, dzz = p[2] - pz; int behind = s < sPl, far11 = dxx * dxx + dzz * dzz > 120.f;
      memcpy(out[no].p, p, sizeof p); out[no].h = atan2f(b[0] - a[0], b[2] - a[2]); out[no].red = behind && far11; no++;
    }
  }
  return no;
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
  if (!nearestLaneH(t, c->car.x, c->car.z, c->car.h, TRF_SV_REJOIN_OLD ? 16.f : 64.f, TRF_SV_REJOIN_OLD ? 1.05f : 3.2f, &r, &l, &s)) return 0;
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
// P03-25 / 8.7: возврат на полосу по условиям оригинала. 1 — вернуть, 0 — ждать, −1 — запретить навсегда. Машина лежит спокойно, не разбита; наклон < 10°;
// ближайшая полоса не дальше 8 м (на прямой дороге; в перекрёстке и при смене полосы оригинал берёт 4 м — перекрёсток здесь не отличается), курс в пределах 90°
// от хода полосы; на полосе есть место и впереди нет помех (цель — точка на полосе в 10 м впереди). Нарушены наклон, дальность или курс — запрет навсегда
// круг 3 (отчёт проверки): полоса — собственная полоса этой машины (на кривой перекрёстка — полоса, куда она шла); расстояние и курс считаются до неё; «нет помех» — коридор впереди
// цели (±3 м для машин, ±2 м для людей, 15 м): игрок, машины миссий, пешеходы, стоящие и трафик на этой же полосе; при неудаче по месту или помехе таймер ждёт заново 3 с
static int svLaneClear(const Traffic *t, const TrCar *c, int r, int l, float s) {
  float a[3], b[3]; lanePos(t, r, l, s, a); lanePos(t, r, l, s + 2.f, b);
  float hx = b[0] - a[0], hz = b[2] - a[2], hl = sqrtf(hx * hx + hz * hz); if (hl < 1e-4f) return 1; hx /= hl; hz /= hl;
  struct { float x, z; int ped; } o[2 + TR_MAXEXTRA + TR_MAXPED + 4 + TR_MAXCAR]; int n = 0;
  if (svPlayerCar) { o[n].x = svPlayerCar->x; o[n].z = svPlayerCar->z; o[n++].ped = 0; }
  for (int i = 0; i < t->nextra && i < TR_MAXEXTRA; i++) if (t->extra[i]) { o[n].x = t->extra[i]->x; o[n].z = t->extra[i]->z; o[n++].ped = 0; }
  for (int i = 0; i < t->maxPeds && n < (int)(sizeof o / sizeof o[0]); i++) if (t->ped[i].mode != TRP_OFF) { o[n].x = t->ped[i].x; o[n].z = t->ped[i].z; o[n++].ped = 1; }
  for (int i = 0; i < t->npark && n < (int)(sizeof o / sizeof o[0]); i++) if (parkLive(t, i)) { o[n].x = t->park[i].car.x; o[n].z = t->park[i].car.z; o[n++].ped = 0; }
  for (int i = 0; i < t->maxCars && n < (int)(sizeof o / sizeof o[0]); i++) { const TrCar *q = &t->car[i];
    if (q == c || q->mode != TRM_DRIVE || q->elem != 0 || q->road != r || q->lane != l) continue; o[n].x = q->car.x; o[n].z = q->car.z; o[n++].ped = 0; }
  for (int i = 0; i < n; i++) {
    float dx = o[i].x - a[0], dz = o[i].z - a[2], fa = dx * hx + dz * hz, fl = -dx * hz + dz * hx;
    if (fa > -2.f && fa < 15.f && fabsf(fl) < (o[i].ped ? SV_COR_PED : SV_COR_CAR)) return 0;
  }
  return 1;
}
static int svRejoinCheck(const Traffic *t, TrCar *c, int *r, int *l, float *s) {
  const float *q = c->car.q;
  if (1.f - 2.f * (q[1] * q[1] + q[3] * q[3]) < .9848f) return -1;   // наклон больше 10°
  *r = c->elem == 1 ? c->nroad : c->road; *l = c->elem == 1 ? c->nlane : c->lane;
  if (*r < 0 || *r >= (int)t->hd->nRoad) return -1;
  { const TrPt *P = lanePts(t, *r, *l); int n = t->road[*r].nPt; float best = 1e30f; *s = 0.f;   // ближайшая точка своей полосы
    for (int i = 0; i + 1 < n; i++) {
      float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
      float u = clampf(((c->car.x - P[i].x) * ax + (c->car.z - P[i].z) * az) / L2, 0.f, 1.f), qx = P[i].x + ax * u - c->car.x, qz = P[i].z + az * u - c->car.z, d2 = qx * qx + qz * qz;
      if (d2 < best) { best = d2; *s = P[i].s + (P[i + 1].s - P[i].s) * u; } }
    if (best > 64.f) return -1; }   // дальше 8 м от своей полосы
  float a[3], b[3]; lanePos(t, *r, *l, *s, a); lanePos(t, *r, *l, *s + 1.f, b);
  if (fabsf(wrapPi(atan2f(b[0] - a[0], b[2] - a[2]) - c->car.h)) > 1.5707963f) return -1;   // курс дальше 90° от хода полосы
  if (laneCars(t, c, *r, *l) >= laneDens(t, *r, *l)) return 0;   // места на полосе нет
  return svLaneClear(t, c, *r, *l, *s + 10.f) ? 1 : 0;           // цель — точка в 10 м впереди
}
// возврат на заданную полосу (из svRejoinCheck)
static int backToLaneAt(TrCar *c, int r, int l, float s) {
  c->mode = TRM_DRIVE; c->elem = 0; c->road = r; c->lane = l; c->s = s; c->next = 0; c->v = 0.f; c->waitT = 0.f; c->rearOk = 0; c->chgK = 0.f; c->junc = -1; c->relJ = 0; c->dodged = 0;
  return 1;
}
static void physStep(Traffic *t, const ColWorld *w, TrCar *c, float dt) {
  CarInput in = {0.f, 0.f, 0.f, 1.f};
  if (!TRF_SV_DODGE_OLD && c->dodged) {   // 8.1: 0,8 с полный газ и руль, затем ручник ровно на один кадр, дальше колёса свободны — катится сама
    if (c->dodgeT > 0.f) { in.throttle = 1.f; in.handbrake = 0.f; in.steer = (float)c->dodgeSide * SV_STEER_SIGN; c->car.wheelMaxOvr = 2.0944f; if ((c->dodgeT -= dt) <= 0.f) c->dodgeHb = 1; }   // сверка g01-cars круг 2 (8.1): на время уворота предел колёс 120°
    else { c->car.wheelMaxOvr = 0.f; if (c->dodgeHb) { in.handbrake = 1.f; c->dodgeHb = 0; } else in.handbrake = 0.f; }
  } else c->car.wheelMaxOvr = 0.f;
  if (c->flashT > 0.f) c->flashT -= dt;
  carUpdate(&c->car, w, &in, dt);
  { int air = !c->car.grounded && c->car.airT > .3f;   // круг 2 (g08 P09:30): приземление после полёта — подвеска (suspension_ronks), если ударило сильнее 4 м/с
    if (c->wasAir && c->car.grounded && -c->vy0 > 4.f) pushEv(t, TR_EV_CAR_LAND, (int)(c - t->car), c->car.x, c->car.y, c->car.z, -c->vy0);
    c->wasAir = air; c->vy0 = c->car.vel[1]; }
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
    if (!TRF_SV_REJOIN_OLD) {   // P03-25 / 8.7 (строки 21, 44, круг 3): слабый и сильный удар — одни условия: через 3 с после съезда, лежит спокойно, своя полоса в 8 м, место и коридор свободны; 13,8 с оглушения нет
      if (!c->ramStrong && c->stillT > TRF_RAM_WEAK_STOP && !c->honkDone) { c->honkDone = 1; pushEv(t, TR_EV_CAR_HONK, (int)(c - t->car), c->car.x, c->car.y, c->car.z, 0.f); }
      c->outLaneT += dt;
      if (!c->noRejoin && c->stillT > .3f && c->outLaneT >= 3.f) {
        int r, l; float s, rc = (float)svRejoinCheck(t, c, &r, &l, &s);
        if (rc > 0.f) { backToLaneAt(c, r, l, s); c->hitV = 0.f; c->ramStrong = 0; c->outLaneT = 0.f; }
        else if (rc < 0.f) c->noRejoin = 1;   // запрет навсегда: остаётся твёрдой до удаления
        else c->outLaneT = 0.f;               // места нет или помеха: ждёт заново 3 с
      }
    } else if (!c->ramStrong) {   // слабый: стоит, гудит, едет дальше
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
  if (dp < (TRF_SV_SPAWN_OLD ? (t->fresh ? INIT_MIN : SPAWN_MIN) : (t->fresh ? INIT_MIN : 25.f))) return 0;   // P03-23: 25 м — геометрия кольца 65 м вокруг точки на 40 м впереди (отдельного «50 м впереди» в оригинале нет)
  if (TRF_SV_SPAWN_OLD && dp < SPAWN_AHEAD_MIN) for (int k = 0; k < V->ndir; k++) if (qx * V->dir[k][0] + qz * V->dir[k][1] > 0.f) return 0;
  // кадр камеры оставлен как страховка PSP: у оригинала машина невидима, пока не въехала в 85..105 м (затухание), у порта затухания нет (P03-22 — слово Роба), без этого машина «вылезает» на глазах
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
  float bandW = TRF_SV_SPAWN_OLD ? SPAWN_BAND : 5.f;   // P03-23: кольцо 65 м (пересечения полосы с кругом), поэтому уже, чем прежняя полоса ±10 м
  float lo = t->fresh ? 0.f : SPAWN_R - bandW, hi = t->fresh ? INIT_R : SPAWN_R + bandW;
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
        if (!TRF_SV_SPAWN_OLD) { int dn = laneDens(t, (int)r, l), cap = dn < 2 ? dn : 2; if (cap < 1 || laneCars(t, NULL, (int)r, l) >= cap) continue; }   // P18-09/P03-23: на полосе меньше min(плотность полосы, 2) машин
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
        float ex = t->car[k].car.x - p[0], ez = t->car[k].car.z - p[2], need = 14.f;
        if (!TRF_SV_SPAWN_OLD) {   // P03-23: от других машин не ближе 5 м + на своей полосе скорость × 1,5 с, иначе сумма радиусов кузовов
          const TrCar *o = &t->car[k]; const CarParams *P1 = &t->params[t->slot[i].model];
          int same = o->mode == TRM_DRIVE && o->elem == 0 && o->road == r && o->lane == l;
          need = 5.f + (same ? ROAD_V(t, r) * 1.5f : P1->halfL + o->car.p.halfL); }
        clash = ex * ex + ez * ez < need * need; }
      for (int k = 0; k < t->npark && !clash; k++) { if (!parkLive(t, k)) continue;
        float ex = t->park[k].car.x - p[0], ez = t->park[k].car.z - p[2]; clash = ex * ex + ez * ez < 5.f * 5.f; }
      if (player) { float ex = player->x - p[0], ez = player->z - p[2]; clash |= ex * ex + ez * ez < 12.f * 12.f; }
      if (clash) continue;
      int model = t->slot[i].model;
      memset(c, 0, sizeof *c);
      c->model = model; c->road = r; c->lane = l; c->s = s; c->junc = -1;
      float a[3]; lanePos(t, r, l, s + LOOK, a); float h = atan2f(a[0] - p[0], a[2] - p[2]);
      carInit(&c->car, &t->params[model], w, p[0], p[1], p[2], h);
      c->mode = TRM_DRIVE; c->v = ROAD_V(t, r) * (t->fresh && TRF_SV_SPAWN_OLD ? .5f : 1.f);
      if (!TRF_SV_SPAWN_OLD && laneLen(t, r, l) - s < 10.f) c->v = 0.f;   // P03-24: скорость желаемая, но 0, если точка ближе 10 м до конца дороги
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
static int pedStands(const TrPed *e) { return e->mode == TRP_WALK || e->mode == TRP_STAND || e->mode == TRP_GETUP || e->mode == TRP_DODGE || e->mode >= TRP_PANIC; }   // TRP_PANIC.. — сверка g02-traffic: тоже на ногах
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
// ---- сверка g02-traffic: рождение пешехода по оригиналу (СВЕРКА_ГОМЕР 7.1, 7.2, 7.3, 7.9; P03-12, P03-14, P03-17, P03-18, Д8)
// Центр зоны — игрок + 30 м вперёд по взгляду камеры; рождение на границе круга 45 м (первое пересечение выбранной случайной тропы с кругом), первые 3 с после
// загрузки — круг 90 м вокруг игрока. Одна попытка (один кандидат) на каждый второй кадр, первые 3 с — на каждый. Не ближе 4, 7 или 12 м (равновероятно)
// к другому человеку и к игроку, к машине — не ближе её радиуса + 1 м, на одной тропе не больше 3, модель равновероятно среди моделей группы с остатком лимита
// (по умолчанию 5), разных одновременно не больше 4. Не в кадре ближе 60 м. Скорость при рождении: 1,0 (40 %), 1,5 (40 %) или бег 3,0 (20 %); тропа в одну сторону
#define SV_PED_R 45.f
#define SV_PED_R0 90.f
#define SV_PED_LEAD 30.f
static int svCircleCross(const float a[3], const float b[3], float cx, float cz, float R, float *u) {
  float dx = b[0] - a[0], dz = b[2] - a[2], fx = a[0] - cx, fz = a[2] - cz, A = dx * dx + dz * dz, B = 2.f * (fx * dx + fz * dz), C = fx * fx + fz * fz - R * R;
  if (A < 1e-8f) return 0;
  float D = B * B - 4.f * A * C; if (D < 0.f) return 0;
  float sq = sqrtf(D), u1 = (-B - sq) / (2.f * A), u2 = (-B + sq) / (2.f * A);
  if (u1 >= 0.f && u1 <= 1.f) { *u = u1; return 1; }
  if (u2 >= 0.f && u2 <= 1.f) { *u = u2; return 1; }
  return 0;
}
static void pedPlay(Traffic *t, TrPed *e, int clip, int loop);
static void pedSetSpeedRoll(Traffic *t, TrPed *e, int m) {
  if (TRF_SV_PEDSPEED_OLD) { e->speed = t->walkV[m]; e->run = 0; return; }
  float r = rnd(t);   // 7.3: 40 % — 1,0 м/с, 40 % — 1,5 м/с, 20 % — бег 3,0 м/с
  e->run = r >= .8f; e->speed = r < .4f ? 1.0f : r < .8f ? 1.5f : 3.0f;
  e->homeRun = e->run; e->homeSpeed = e->speed;   // круг 3: своя скорость не теряется; 1,0 — только на время возврата на тропу
}
static void spawnPedsSv(Traffic *t, const ColWorld *w, const float ppos[3], const float eye[3], const float fwd[3]) {
  if (!t->maxPeds || t->streetRace || t->group < 0 || t->group >= (int)t->hd->nGroup || !t->hd->nPath) return;
  const TrGroup *g = t->grp + t->group; if (!g->n) return;
  TrPed *e = 0; for (int i = 0; i < t->maxPeds; i++) if (t->ped[i].mode == TRP_OFF) { e = &t->ped[i]; break; }
  if (!e) return;
  int early = t->loadT < 3.f;
  float fl = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]), fx = fl > 1e-4f ? fwd[0] / fl : 0.f, fz = fl > 1e-4f ? fwd[2] / fl : 1.f;
  float cx = early ? ppos[0] : ppos[0] + fx * SV_PED_LEAD, cz = early ? ppos[2] : ppos[2] + fz * SV_PED_LEAD, R = early ? SV_PED_R0 : SV_PED_R;
  int p = (int)(rnd(t) * t->hd->nPath) % (int)t->hd->nPath; const TrPath *P = &t->path[p];
  { float dcx = P->cx - cx, dcz = P->cz - cz, dc = sqrtf(dcx * dcx + dcz * dcz); if (dc - P->rad > R || dc + P->rad < R) { t->pedTryT += 1.f; goto fallback; } }
  {
    int k = -1; float u = 0.f;
    for (uint32_t q = 0; q < P->n; q++) { if (svCircleCross(pathP(t, p, (int)q), pathP(t, p, (int)q + 1), cx, cz, R, &u)) { k = (int)q; break; } }
    if (k < 0) { t->pedTryT += 1.f; goto fallback; }
    const float *a = pathP(t, p, k), *b = pathP(t, p, k + 1);
    float x = a[0] + (b[0] - a[0]) * u, y = a[1] + (b[1] - a[1]) * u, z = a[2] + (b[2] - a[2]) * u;
    float qx = x - ppos[0], qz = z - ppos[2], dp = sqrtf(qx * qx + qz * qz);
    if (!early && dp < t->pedDrawDist && inView(eye, fwd, x, y + 1.f, z, 1.f, 1e9f)) return;   // в кадре и ближе 60 м не появляется
    static const float mind[3] = {4.f, 7.f, 12.f}; float md = mind[(int)(rnd(t) * 3.f) % 3];
    if (qx * qx + qz * qz < md * md) return;                                                   // к игроку
    int onPath = 0;
    for (int j = 0; j < t->maxPeds; j++) if (t->ped[j].mode != TRP_OFF) {
      float ex = t->ped[j].x - x, ez = t->ped[j].z - z; if (ex * ex + ez * ez < md * md) return;   // к любому другому человеку
      onPath += t->ped[j].path == p; }
    if (onPath >= 3) return;                                                                   // на одной тропе не больше 3
    for (int j = 0; j < t->maxCars; j++) if (t->car[j].mode != TRM_OFF) {
      float ex = t->car[j].car.x - x, ez = t->car[j].car.z - z, r = carR(&t->car[j].car) + 1.f; if (ex * ex + ez * ez < r * r) return; }
    for (int j = 0; j < t->npark; j++) if (parkLive(t, j)) {
      float ex = t->park[j].car.x - x, ez = t->park[j].car.z - z, r = carR(&t->park[j].car) + 1.f; if (ex * ex + ez * ez < r * r) return; }
    if (svPlayerCar) { float ex = svPlayerCar->x - x, ez = svPlayerCar->z - z, r = carR(svPlayerCar) + 1.f; if (ex * ex + ez * ez < r * r) return; }
    int ok[4], nok = 0, kinds = 0, seenM[TR_MAXPEDMODEL] = {0};   // модель: равновероятно среди моделей группы с остатком лимита, разных одновременно ≤ 4
    for (int j = 0; j < t->maxPeds; j++) if (t->ped[j].mode != TRP_OFF && !seenM[t->ped[j].model]) { seenM[t->ped[j].model] = 1; kinds++; }
    for (int j = 0; j < g->n; j++) {
      int m = g->model[j], lim = g->count[j] ? g->count[j] : 5, have = 0;
      for (int q = 0; q < t->maxPeds; q++) have += t->ped[q].mode != TRP_OFF && t->ped[q].model == m;
      if (have < lim && (seenM[m] || kinds < 4)) ok[nok++] = j;
    }
    if (!nok) return;
    int m = g->model[ok[(int)(rnd(t) * nok) % nok]];
    memset(e, 0, sizeof *e);
    e->model = m; e->path = p; e->seg = k; e->dir = 1; e->s = u; pedSetSpeedRoll(t, e, m);   // тропа проходится по кругу отрезков в одну сторону (P03-18)
    e->x = x; e->y = y; e->z = z; e->partner = -1;
    e->h = atan2f(b[0] - x, b[2] - z);
    if (!pedGround(w, e, 1.5f)) return;   // №594: рождение над пустотой — другая точка
    skinInit(&e->sp, &t->peds[m]); e->mode = TRP_WALK; pedPlay(t, e, e->run && t->clip[m][TRC_RUN] >= 0 ? TRC_RUN : TRC_WALK, 1);
    t->pedTryT = 0.f;
    return;
  }
fallback:
  if (t->pedTryT > 45.f) {   // «оставить прежний отбор как PSP-страховку, если на границе пусто» (строка 14): около 3 с без единого рождения — прежнее правило, одна попытка
    t->pedTryT = 0.f;
    spawnPeds(t, w, ppos, eye, fwd);
  }
}
// 7.4 / P03-16: паника. Пинок игрока, гудок игрока, наезд машины игрока пугают всех пешеходов в 12 м. Паникующий бежит от игрока 3 м/с, раз в 2 с меняет курс случайным
// разбросом до ±5 м (если дальше 6 м от игрока); кончается, когда упёрся в стену или забор; потом 3 с стоит и идёт по тропе. Заразна: пешеход рядом (3 м — радиус заразности
// в отчёте не назван, взят на глаз) с паникующим паникует сам
static void pedPanic(Traffic *t, TrPed *e, float px, float pz) {
  if (TRF_SV_PANIC_OLD || e->test || e->mode == TRP_OFF || e->mode == TRP_FLY || e->mode == TRP_DOWN || e->mode == TRP_GETUP || e->mode == TRP_PANIC) return;
  if (!e->homeSpeed) { e->homeSpeed = e->speed; e->homeRun = e->run; }
  e->mode = TRP_PANIC; e->stT = 0.f; e->pTurnT = 2.f; e->speed = 3.f; e->run = 1; e->retPath = 0; e->avoidSide = 0; e->avoidT = 0.f; e->partner = -1;
  { float dx = e->x - px, dz = e->z - pz, d = sqrtf(dx * dx + dz * dz); if (d > .01f) { e->pDirX = dx / d; e->pDirZ = dz / d; } else { e->pDirX = sinf(e->h); e->pDirZ = cosf(e->h); } }   // первый курс — прямо от игрока, без разброса
  pedPlay(t, e, t->clip[e->model][TRC_RUN] >= 0 ? TRC_RUN : TRC_WALK, 1);
}
void trafficScare(Traffic *t, float x, float z) {
  if (!t || !t->hd || TRF_SV_PANIC_OLD) return;
  for (int i = 0; i < t->maxPeds; i++) { TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF) continue;
    float dx = e->x - x, dz = e->z - z; if (dx * dx + dz * dz <= 144.f) pedPanic(t, e, x, z); }
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
static void pedHitBy(Traffic *t, int i, const Car *k, float dt) {
  // правка 240: лежащего машина тоже толкает и подбрасывает (иначе едущий дальше игрок проезжает по нему, и сбитый «уходит под
  // машину»); новое нарушение Hit & Run — только за удар стоящего (правило порта)
  TrPed *e = &t->ped[i]; int down = !TR_KNOCK_OLD && e->mode == TRP_DOWN, byPlayer = k == svPlayerCar;
  if (!pedStands(e) && !down) return;
  if (!TRF_SV_PEDHIT_OLD && down && !byPlayer) {   // P02-42: лежащий (физика) от машины трафика быстрее 1 м/с получает лёгкий толчок 1,4 м/с по нормали
    float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]), nx, nz, depth;
    if (sp > 1.f && fabsf(k->y - e->y) <= 1.8f && boxCircle(k, e->x, e->z, TR_PED_R, &nx, &nz, &depth)) { e->x += nx * 1.4f * dt; e->z += nz * 1.4f * dt; }
    return;
  }
  float v[3];
  if (trafficCarHitsBody(k, &e->x, e->y, &e->z, TR_PED_R, v) != 2) return;   // 1 — медленно: уже вытолкнут
  if (!TRF_SV_PEDHIT_OLD && !byPlayer) {   // 7.6: у трафика в физике скорость нулевая — отлёт придуман: вверх (6…11) × скорость / 28 м/с, вдоль курса машины и по нормали
    float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]);
    v[1] = (6.f + 5.f * rnd(t)) * sp / 28.f;
  }
  pedFly(t, i, v[0], v[2], v[1]); e->knocked = 1; if (byPlayer) e->mouthT = 1.5f;   // возглас при сбитии игроком (P15-06)
  if (!down && byPlayer) { pushEvW(t, TR_EV_PED_HIT, i, e->x, e->y, e->z, HIT_PED, t->pedm[e->model].name); trafficScare(t, k->x, k->z); }   // игрок сбил — пугает пешеходов в 12 м
  else if (!down && TRF_SV_PEDHIT_OLD) pushEvW(t, TR_EV_PED_HIT, i, e->x, e->y, e->z, HIT_PED, t->pedm[e->model].name);
}
// 7.5 / P02-40: уворот от машины по прогнозу. Каждый кадр: машина ближе 20 м и в течение 1 с столкнётся (машина шириной 1,5 м, пешеход 0,5 м) — пешеход сначала
// СТАНОВИТСЯ; если машина всё равно едет и игрок в машине — в половине случаев съёживается на месте (лицом к игроку), в половине отпрыгивает вбок 1 м/с в сторону, где
// пешеход стоит относительно курса машины, потом возврат на тропу; игрок вне машины — больше ничего. Событие «чуть не сбил» (голос) — при съёживании/прыжке
static int pedWillHit(const TrPed *e, const Car *k, int isPlayerCar) {
  float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]); if (sp < .5f) return 0;
  float bx, bz; bodyCenter(k, &bx, &bz); float dx = e->x - bx, dz = e->z - bz;
  if (dx * dx + dz * dz > 400.f || fabsf(k->y - e->y) > 2.5f) return 0;
  float ux = k->vel[0] / sp, uz = k->vel[2] / sp, a = dx * ux + dz * uz, l = -dx * uz + dz * ux;
  // круг 3 (проверка по исходнику): допуск поперёк — сумма «машина 1,5 + пешеход 0,5» = 2,0 м в каждую сторону, вперёд — путь за 1 с плюс та же сумма
  if (a > -2.f && a < sp + 2.f && fabsf(l) < 2.f) return 1;
  if (e->mode == TRP_WALK && isPlayerCar) {   // идущий пешеход и едущий игрок: грубее — пересекаются два шара движения за 1 с (радиус каждого — половина пути)
    float ps = e->speed, cx = bx + k->vel[0] * .5f, cz = bz + k->vel[2] * .5f, px = e->x + sinf(e->h) * ps * .5f, pz = e->z + cosf(e->h) * ps * .5f;
    float r = sp * .5f + ps * .5f; return (cx - px) * (cx - px) + (cz - pz) * (cz - pz) < r * r;
  }
  return 0;
}
static void pedReactCar(Traffic *t, int i, const Car *k, int driving, float dt) {
  TrPed *e = &t->ped[i];
  if ((e->mode != TRP_WALK && e->mode != TRP_STAND) || e->test) return;
  if (TRF_SV_PEDDODGE_OLD) return;
  if (!pedWillHit(e, k, k == svPlayerCar)) return;
  if (e->mode == TRP_WALK) { e->mode = TRP_STAND; e->blockT = .8f; e->stT = 1e-3f; pedPlay(t, e, TRC_IDLE, 1); return; }   // сначала становится
  if (e->stT <= 0.f) { e->stT = 1e-3f; return; }
  e->blockT = .8f; e->stT += dt;
  if (!driving || e->stT < .3f) return;   // игрок вне машины — больше ничего; после остановки наблюдает 0,3 с, потом бросок каждый кадр, пока угроза держится
  float sp = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]), ux = k->vel[0] / sp, uz = k->vel[2] / sp, bx, bz;
  bodyCenter(k, &bx, &bz);
  float l = -(e->x - bx) * uz + (e->z - bz) * ux;
  e->stT = 0.f;
  if (rnd(t) < .5f) { e->mode = TRP_CRINGE; pedPlay(t, e, TRC_CRINGE, 0); }
  else { float sg = l >= 0.f ? 1.f : -1.f; e->mode = TRP_SIDE; e->pDirX = -uz * sg; e->pDirZ = ux * sg; pedPlay(t, e, TRC_DODGE, 0); }
  if (e->nearCool <= 0.f) { e->nearCool = PED_NEAR_COOL; e->mouthT = 1.2f; pushEvW(t, TR_EV_PED_NEAR, i, e->x, e->y, e->z, 0.f, t->pedm[e->model].name); }   // голос — не чаще раза в 4 с
}
// 7.8 / P02-39: два пешехода на одной тропе, подошедшие ближе 1,5 м (быстрые) или 2 м (медленные, ≤ 1 м/с) и не дальше 35 м от игрока, останавливаются, поворачиваются друг
// к другу и говорят. Рот у говорящего двигать нечем (челюстей в риге нет — P15-06, передано), роли меняются по таймеру
static void pedTalkScan(Traffic *t, const float ppos[3]) {
  if (TRF_SV_PEDTALK_OLD) return;
  for (int i = 0; i < t->maxPeds; i++) {   // круг 3: каждый кадр, любой другой идущий пешеход в 35 м от игрока (не только с той же тропы и не только шагом), расстояние трёхмерное
    TrPed *a = &t->ped[i]; if (a->mode != TRP_WALK || a->test) continue;
    if ((a->x - ppos[0]) * (a->x - ppos[0]) + (a->z - ppos[2]) * (a->z - ppos[2]) > 35.f * 35.f) continue;
    float lim = a->speed > 1.f ? 1.5f : 2.f;   // 1,5 м, если сам идёт быстрее 1 м/с, иначе 2 м
    for (int j = 0; j < t->maxPeds; j++) {
      TrPed *b = &t->ped[j]; if (j == i || b->mode != TRP_WALK || b->test) continue;
      float dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
      if (dx * dx + dy * dy + dz * dz >= lim * lim) continue;
      a->mode = b->mode = TRP_TALK; a->partner = j; b->partner = i; a->stT = b->stT = 0.f; a->pTurnT = b->pTurnT = 1.f; a->speaker = 1; b->speaker = 0;
      pedPlay(t, a, TRC_IDLE, 1); pedPlay(t, b, TRC_IDLE, 1); break;
    }
  }
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
// круг 3 (7.3): вышел с тропы (после паники, съёживания, прыжка, уворота, вставания) — идёт назад к ней со скоростью 1,0; вернулся ближе 1 м — снова своя рождённая скорость
static void pedGoHome(TrPed *e) { if (TRF_SV_PEDSPEED_OLD) { e->speed = 1.f; e->run = 0; return; } if (!e->homeSpeed) { e->homeSpeed = e->speed; e->homeRun = e->run; } e->speed = 1.f; e->run = 0; e->retPath = 1; }
static float pedPathDist(const Traffic *t, const TrPed *e) {
  const TrPath *P = &t->path[e->path]; float best = 1e30f;
  for (uint32_t k = 0; k < P->n; k++) {
    const float *a = pathP(t, e->path, (int)k), *b = pathP(t, e->path, (int)k + 1); float ax = b[0] - a[0], az = b[2] - a[2], L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
    float u = clampf(((e->x - a[0]) * ax + (e->z - a[2]) * az) / L2, 0.f, 1.f), qx = a[0] + ax * u - e->x, qz = a[2] + az * u - e->z; if (qx * qx + qz * qz < best) best = qx * qx + qz * qz;
  }
  return sqrtf(best);
}
// круг 5 (P15-06): рот двигается у говорящего (разговор пешеходов 7.8) и пока звучит возглас (сбит, пнут, чуть не сбит): ход по SkinMouth в той же мере 0…0,22 рад, что и у героев в msn_view.c
#ifndef TRF_SV_MOUTH_OLD
#define TRF_SV_MOUTH_OLD 0
#endif
static void pedMouth(Traffic *t, TrPed *e, float dt) {
  if (TRF_SV_MOUTH_OLD) return;
  int speaks = e->mouthT > 0.f || (e->mode == TRP_TALK && e->speaker);
  if (e->mouthT > 0.f) e->mouthT -= dt;
  if (!speaks) { e->sp.jawOpen = 0.f; return; }
  float a = skinMouthStep(&e->mouth, t->pedm[e->model].name, dt), lo = -18.f * 0.0174533f, span = e->mouth.hi0 - lo;
  float u = span > .01f ? (a - lo) / span : 0.f; u = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
  e->sp.jawOpen = .22f * u;
}
static int pedMoveClip(const Traffic *t, const TrPed *e) { return (!TRF_SV_PEDSPEED_OLD && e->run && t->clip[e->model][TRC_RUN] >= 0) ? TRC_RUN : TRC_WALK; }   // 7.3: бег — клип npd_loco_run
static void pedStep(Traffic *t, const ColWorld *w, int i, const Car *player, int driving, const float ppos[3], float dt) {
  TrPed *e = &t->ped[i];
  switch (e->mode) {
    case TRP_WALK: case TRP_STAND: {
      if (e->test) { if (e->mode != TRP_STAND) { e->mode = TRP_STAND; pedPlay(t, e, TRC_IDLE, 1); } break; }
      if (!TRF_SV_PEDFACE_OLD && !e->test && (e->mode == TRP_WALK || e->mode == TRP_STAND) && fabsf(ppos[1] - e->y) < 2.f) {   // P02-37: игрок ближе 1,41 м — стоп лицом к нему
        float fx_ = ppos[0] - e->x, fz_ = ppos[2] - e->z;
        if (fx_ * fx_ + fz_ * fz_ < 2.f) { e->mode = TRP_FACE; e->stT = 0.f; pedPlay(t, e, TRC_IDLE, 1); break; } }
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
          if (e->mode != TRP_WALK) { e->mode = TRP_WALK; pedPlay(t, e, pedMoveClip(t, e), 1); }
          break; } }
      if (blocked) { e->blockT = .5f; if (e->mode != TRP_STAND) { e->mode = TRP_STAND; pedPlay(t, e, TRC_IDLE, 1); } break; }
      if (e->mode == TRP_STAND) { if ((e->blockT -= dt) > 0.f) break; e->mode = TRP_WALK; pedPlay(t, e, pedMoveClip(t, e), 1); }
      if (e->retPath && pedPathDist(t, e) < 1.f) { e->retPath = 0; e->speed = e->homeSpeed; e->run = e->homeRun; pedPlay(t, e, pedMoveClip(t, e), 1); }   // круг 3 (7.3): вернулся на тропу — своя скорость
      const float *a = pathP(t, e->path, e->seg), *b = pathP(t, e->path, e->seg + 1);
      float tx = e->dir > 0 ? b[0] : a[0], tz = e->dir > 0 ? b[2] : a[2];
      float dx = tx - e->x, dz = tz - e->z, d = sqrtf(dx * dx + dz * dz), step = e->speed * dt;
      if (d <= step || d < .05f) { e->x = tx; e->z = tz; e->seg += e->dir; e->seg = (e->seg % (int)t->path[e->path].n + (int)t->path[e->path].n) % (int)t->path[e->path].n; }
      else if (!TRF_SV_PEDPATH_OLD && d < .5f) { e->seg += e->dir; e->seg = (e->seg % (int)t->path[e->path].n + (int)t->path[e->path].n) % (int)t->path[e->path].n; }   // P02-38: точка пройдена в 0,5 м
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
    case TRP_PANIC: {   // 7.4: бежит от игрока 3 м/с, раз в 2 с курс с разбросом до ±5 м (дальше 6 м от игрока); упёрся в стену или забор — конец
      float dx = e->x - ppos[0], dz = e->z - ppos[2], d = sqrtf(dx * dx + dz * dz);
      if ((e->pTurnT -= dt) <= 0.f) {
        e->pTurnT = 2.f;
        float vx = dx, vz = dz;   // вектор «от игрока к пешеходу» в настоящую длину; дальше 6 м к каждой оси отдельно прибавляется 0…5 м со случайным знаком
        if (d > 6.f) { vx += (rnd(t) < .5f ? -1.f : 1.f) * rnd(t) * 5.f; vz += (rnd(t) < .5f ? -1.f : 1.f) * rnd(t) * 5.f; }
        float vl = sqrtf(vx * vx + vz * vz);
        if (vl > .01f) { e->pDirX = vx / vl; e->pDirZ = vz / vl; } else { e->pDirX = sinf(e->h); e->pDirZ = cosf(e->h); }
      }
      float nx = e->x + e->pDirX * 3.f * dt, nz = e->z + e->pDirZ * 3.f * dt, a3[3] = {e->x, e->y + TR_BODY_MID, e->z}, b3[3] = {nx, e->y + TR_BODY_MID, nz};
      if (colRayFrac(w, a3, b3, 0) < 1.f || colCrossFence(w, a3[0], a3[2], b3[0], b3[2], 0, 0) || colCrossFence(w, b3[0], b3[2], a3[0], a3[2], 0, 0)) {   // стена или забор: паника кончается, 3 с стоит и идёт по тропе
        e->mode = TRP_STAND; e->blockT = 3.f; pedGoHome(e); pedNearest(t, e); pedPlay(t, e, TRC_IDLE, 1); break; }
      e->x = nx; e->z = nz;
      { float th = atan2f(e->pDirX, e->pDirZ), dh = wrapPi(th - e->h), mx = 8.f * dt; e->h = wrapPi(e->h + clampf(dh, -mx, mx)); }
      if (!pedGround(w, e, .6f)) { if ((e->noGroundT += dt) > PED_NOGROUND_T) e->mode = TRP_OFF; } else e->noGroundT = 0.f;
      break; }
    case TRP_FACE: {   // P02-37: стоит лицом к игроку не меньше 3 с, потом идёт дальше, если никого рядом (иначе ждёт ещё 3 с)
      float dx = ppos[0] - e->x, dz = ppos[2] - e->z, want = atan2f(dx, dz), da = wrapPi(want - e->h);
      e->h = fabsf(da) > .785f ? want : wrapPi(e->h + clampf(da, -3.f * dt, 3.f * dt));   // больше 45° — сразу; 33–45° у оригинала клип turn60 (в риге порта клипа нет — плавный поворот)
      if ((e->stT += dt) >= 3.f) {
        if (dx * dx + dz * dz < 2.f && fabsf(ppos[1] - e->y) < 2.f) e->stT = 0.f;
        else { e->mode = TRP_WALK; pedPlay(t, e, pedMoveClip(t, e), 1); }
      }
      break; }
    case TRP_CRINGE: {   // 7.5: съёжился на месте, лицом к игроку, пока машина не проедет
      float dx = ppos[0] - e->x, dz = ppos[2] - e->z; e->h = wrapPi(e->h + clampf(wrapPi(atan2f(dx, dz) - e->h), -6.f * dt, 6.f * dt));
      if ((e->stT += dt) > 1.5f) { e->mode = TRP_WALK; pedGoHome(e); pedPlay(t, e, pedMoveClip(t, e), 1); }
      break; }
    case TRP_SIDE: {   // 7.5 / P02-40: отпрыгнул вбок 1 м/с в сторону, где стоит относительно курса машины; потом возврат на тропу
      float nx = e->x + e->pDirX * dt, nz = e->z + e->pDirZ * dt, a3[3] = {e->x, e->y + TR_BODY_MID, e->z}, b3[3] = {nx, e->y + TR_BODY_MID, nz};
      if (colRayFrac(w, a3, b3, 0) >= 1.f) { e->x = nx; e->z = nz; }
      pedGround(w, e, .6f);
      if ((e->stT += dt) > 1.f) { pedNearest(t, e); e->mode = TRP_WALK; pedGoHome(e); pedPlay(t, e, pedMoveClip(t, e), 1); }
      break; }
    case TRP_TALK: {   // 7.8 / P02-39: разговор двух на одной тропе: 3 с говорит, затем раз в секунду проверка — в 2 случаях из 3 роли меняются (разговор идёт), иначе конец
      TrPed *o = e->partner >= 0 && e->partner < t->maxPeds ? &t->ped[e->partner] : 0;
      if (!o || o->mode != TRP_TALK || o->partner != i) { e->mode = TRP_WALK; pedPlay(t, e, pedMoveClip(t, e), 1); break; }
      { float dx = o->x - e->x, dz = o->z - e->z; e->h = wrapPi(e->h + clampf(wrapPi(atan2f(dx, dz) - e->h), -6.f * dt, 6.f * dt)); }
      e->stT += dt;
      if (i < e->partner && e->stT >= 3.f && (e->pTurnT -= dt) <= 0.f) {   // решает младший номер пары: раз в секунду бросок; разговор сам не кончается — только когда кто-то сменил состояние
        e->pTurnT = 1.f;
        if (rnd(t) * 3.f < 2.f) { e->speaker = !e->speaker; o->speaker = !e->speaker; e->stT = o->stT = 0.f; }   // 2 из 3: роли меняются, новый говорящий снова говорит полные 3 с
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
      if (e->t >= len) { if (!e->test) pedGoHome(e); e->mode = e->test ? TRP_STAND : TRP_WALK; pedPlay(t, e, e->test ? TRC_IDLE : pedMoveClip(t, e), 1); }   // служебный (сценарий) — стоит; 7.3: вернувшийся на тропу идёт 1,0 м/с
      break; }
    case TRP_GETUP: {
      e->t += dt; int c = t->clip[e->model][TRC_GETUP]; float len = c >= 0 ? skinClipLength(&t->peds[e->model], c) : 1.f;
      if (e->t >= len) { if (!e->test) pedNearest(t, e); if (!e->test) pedGoHome(e); e->mode = e->test ? TRP_STAND : TRP_WALK; pedPlay(t, e, e->test ? TRC_IDLE : pedMoveClip(t, e), 1); }   // 7.3: после вставания идёт 1,0 м/с
      break; }
  }
  pedMouth(t, e, dt);
  skinTick(&e->sp, dt * (e->mode == TRP_WALK || e->mode == TRP_PANIC ? e->speed / (e->run && !TRF_SV_PEDSPEED_OLD ? t->runV[e->model] : t->walkV[e->model]) : 1.f));
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
// P01-09: смерть не с одного удара. Прочность была больше 2 % (урон < 0,98), а после удара стала бы меньше — остаётся 0,001 (урон 0,999), машина жива, и следующую 1,0 с удары
// не засчитываются совсем. Взрывной урон правилу не подчиняется. Для машины игрока и миссий правило — в car.c (передано g01-cars); здесь трафик и стоящие
static void svNoOneHit(Car *k, float *seen, float *win, float dt) {
  if (TRF_SV_DMG2_OLD) return;
  if (*win > 0.f) { *win -= dt; if (k->damage > *seen) { k->damage = *seen; k->destroyed = 0; } return; }   // окно: урон не растёт
  if (*seen < 1.f - SV_ONE_HIT_MIN && k->damage >= 1.f - SV_ONE_HIT_MIN && !carBoomed(k)) { k->damage = .999f; k->destroyed = 0; *win = 1.f; }
  *seen = k->damage;
}
// P03-05: остов (huskA) ставится не у всех моделей — у этих после взрыва машина просто пропадает
static int svHuskOk(const Traffic *t, int model) {
  static const char *no[] = {"honor_v", "schoolbu", "willi_v", "hallo", "oblit_v", "mono_v", "knigh_v", "cfire_v", "ccola", "hbike_v", "rocke_v", "atv_v", "dune_v", "coffin", "ship", "witchcar"};
  if (TRF_SV_HUSK_OLD || model < 0 || model >= (int)t->hd->nModel) return 1;
  for (unsigned i = 0; i < sizeof no / sizeof no[0]; i++) {
    const char *a = t->model[model].name, *b = no[i]; int k = 0;
    for (; k < 16 && a[k] && b[k]; k++) { char ca = a[k] >= 'A' && a[k] <= 'Z' ? (char)(a[k] + 32) : a[k]; if (ca != b[k]) break; }
    if (k < 16 && !a[k] && !b[k]) return 0;
  }
  return 1;
}
static int svHuskCount(const Traffic *t) {   // сколько остовов уже стоит (не больше SV_HUSK_MAX на трафик, стоящие, погоню, игрока и ИИ — здесь трафик и стоящие)
  int n = 0;
  for (int i = 0; i < t->maxCars; i++) n += t->car[i].mode != TRM_OFF && carBoomed(&t->car[i].car) && svHuskOk(t, t->car[i].model);
  for (int i = 0; i < t->npark; i++) n += !t->park[i].gone && carBoomed(&t->park[i].car) && svHuskOk(t, t->parkd[i].model);
  return n;
}
void trafficPlayerHorn(Traffic *t, int on, float x, float z) {
  if (!t || !t->hd) return;
  if (on) { t->hornOn = 1; t->hornX = x; t->hornZ = z; trafficScare(t, x, z); }   // гудок пугает пешеходов в 12 м (7.4) и будит ответ трафика в 25 м (P03-15)
}
void trafficStreetRace(Traffic *t, int on) {
  if (!t || !t->hd) return;
  t->streetRace = on != 0;
  if (on) for (int i = 0; i < t->maxPeds; i++) t->ped[i].mode = TRP_OFF;
}
int trfOneTapDeath;
#ifndef TRF_SV_PROPSCARS_OLD
#define TRF_SV_PROPSCARS_OLD 0   // круг 2, g11 P11-02: 1 — предметы ломает только машина игрока (звать trafficCarsNear не нужно)
#endif
int trafficCarsNear(Traffic *t, float x, float z, float r, Car **out, int max) {
  int n = 0; if (TRF_SV_PROPSCARS_OLD || !t || !t->hd) return 0;
  for (int i = 0; i < t->maxCars && n < max; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF || (c->mode == TRM_DRIVE && c->v < .3f)) continue;   // стоящий на светофоре предмет не будит: касание считает движение
    float dx = c->car.x - x, dz = c->car.z - z; if (dx * dx + dz * dz < r * r) out[n++] = &c->car;
  }
  for (int i = 0; i < t->npark && n < max; i++) {
    TrParked *P = &t->park[i]; if (P->gone || !P->state) continue;
    float dx = P->car.x - x, dz = P->car.z - z; if (dx * dx + dz * dz < r * r) out[n++] = &P->car;
  }
  return n;
}
void trafficReset(Traffic *t, const ColWorld *w) {
  if (!t->hd) return;
  for (int i = 0; i < TR_MAXCAR; i++) t->car[i].mode = TRM_OFF;
  for (int i = 0; i < TR_MAXPED; i++) t->ped[i].mode = TRP_OFF;
  for (int i = 0; i < t->npark; i++) {
    const TrPark *k = &t->parkd[i]; TrParked *P = &t->park[i];
    carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h); P->car.svKind = 4;   // сверка g01 круг 2 (7.4): вид для svBoom — стоящая
    P->state = 0; P->wreck = 0; P->stillT = 0.f; P->gone = (TRAFFIC_ZONE_PARK && !TRF_SV_ZPARK_OLD) ? 1 : P->off; P->sel = 0;   // круг 5: в зонном режиме сначала пусто, места выбирает загрузка зоны
    if (P->off) { P->car.x = P->car.z = 1e7f; P->car.y = -1000.f; P->car.v = 0.f; }   // место пустое: машины нет
  }
  t->zoneMask = ~0u;   // правка 139: до первых ворот загрузчика загружено всё (ничего не прячем)
  t->zoneGroup = 0; t->group = t->missionPedGroup >= 0 ? t->missionPedGroup : 0; t->fresh = 1; t->scanT = t->pedScanT = 0.f; t->nev = 0; t->solidHave = 0;
  t->zonePrev = 0; t->loadT = 0.f; t->pedTryT = 0.f; t->pedFrame = 0; t->nhonk = 0; t->hornOn = 0; memset(t->jq, 0, sizeof t->jq);   // сверка g02-traffic (P03-12: первые 3 с после загрузки; очереди стоп-знаков и ответ на гудок — заново)
  for (int i = 0; i < TR_MAXPED; i++) t->ped[i].partner = -1;
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
  t->time += dt; t->loadT += dt;
  svPlayerCar = driving ? player : 0;   // сверка g02-traffic: машина игрока для увора и рождения (пешком — нет)
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
    if (!TRF_SV_REMOVE_OLD) {   // P03-21: всё дальше 75 м от точки (игрок + 40 м по взгляду камеры) убирается независимо от видимости; плюс вне кадра дольше 5 с
      c->offT = seen ? 0.f : c->offT + dt;
      if (t->loadT < 3.f) continue;   // круг 3: первые 3 с после загрузки трафик не удаляется вовсе
      if (d > REMOVE_R || c->offT > 5.f || (c->next < 0 && c->v < .1f && !seen)) c->mode = TRM_OFF;
    } else
    if (d > REMOVE_HARD || (d > REMOVE_R && !seen) || (c->next < 0 && c->v < .1f && !seen)) c->mode = TRM_OFF;
  }
  // P16-01 / Д11: очереди стоп-знаков; P03-15: ответ на гудок игрока
  svQueueStep(t, dt);
  if (!TRF_SV_HONK_OLD) {
    for (int q = 0; q < t->nhonk; ) {
      if (t->car[t->honkQ[q].car].mode == TRM_OFF) { t->honkQ[q] = t->honkQ[--t->nhonk]; continue; }   // убранная с дороги выпадает из очереди
      if ((t->honkQ[q].t -= dt) <= 0.f) { int ci = t->honkQ[q].car; pushEv(t, TR_EV_CAR_HONK, ci, t->car[ci].car.x, t->car[ci].car.y, t->car[ci].car.z, 0.f); t->honkQ[q] = t->honkQ[--t->nhonk]; continue; }
      q++;
    }
    if (t->hornOn && !t->nhonk) {   // очередь пуста: до 3 машин трафика в 25 м, каждая с шансом 10 % (бросок каждый кадр, пока гудок зажат)
      for (int i = 0; i < t->maxCars && t->nhonk < 3; i++) { TrCar *c = &t->car[i]; if (c->mode != TRM_DRIVE) continue;
        float dx = c->car.x - t->hornX, dz = c->car.z - t->hornZ; if (dx * dx + dz * dz > 625.f) continue;
        if (rnd(t) < .1f) { t->honkQ[t->nhonk].car = i; t->honkQ[t->nhonk++].t = .5f + .3f * rnd(t); } }
    }
    t->hornOn = 0;   // держится, только пока вызывают trafficPlayerHorn каждый кадр
  }
  TP(1);
  if ((t->scanT -= dt) <= 0.f || t->fresh) {
    t->scanT = TRF_SV_REMOVE_OLD ? .25f : .2f;   // P03-21: проход каждые 0,2 с
    int fresh0 = t->fresh; if (!TRF_SV_REMOVE_OLD && t->loadT < 3.f) t->fresh = 1;   // круг 3: первые 3 с — кольцо 100 м вокруг игрока на каждом проходе (раз в 0,2 с), а не в один кадр
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
    t->fresh = fresh0;
  }
  TP(2);
  if (t->streetRace) { for (int i = 0; i < t->maxPeds; i++) t->ped[i].mode = TRP_OFF; }   // P03-13: уличная гонка — пешеходов нет (и на гонках 1–2 они не стоят у дороги — P02-41: сняты вовсе)
  if (!TRF_SV_PEDSPAWN_OLD) {
    // 7.2 / P03-12: центр зоны — игрок + 30 м вперёд по взгляду камеры. Дальше 50 м от центра или вне кадра дольше 2 с (не больше одного за проход) — снимается; первые 3 с после загрузки не снимается
    float fl2 = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]), gx = fl2 > 1e-4f ? fwd[0] / fl2 : 0.f, gz = fl2 > 1e-4f ? fwd[2] / fl2 : 1.f;
    float c30x = ppos[0] + gx * SV_PED_LEAD, c30z = ppos[2] + gz * SV_PED_LEAD; int dropped = 0;
    for (int i = 0; i < t->maxPeds && t->loadT >= 3.f; i++) {
      TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF || e->test) continue;
      float dx = e->x - c30x, dz = e->z - c30z;
      if (dx * dx + dz * dz > 50.f * 50.f) { e->mode = TRP_OFF; continue; }
      if (inView(eye, fwd, e->x, e->y + 1.f, e->z, 1.f, 1e9f)) e->offT = 0.f;
      else if ((e->offT += dt) > 2.f && !dropped) { e->mode = TRP_OFF; dropped = 1; }
    }
    t->pedFrame++;   // Д8: одна попытка на каждый второй кадр, первые 3 с — на каждый
    if (t->loadT < 3.f || !(t->pedFrame & 1)) spawnPedsSv(t, w, ppos, eye, fwd);
  } else {
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF || e->test) continue;
    float dx = e->x - ppos[0], dz = e->z - ppos[2], d = sqrtf(dx * dx + dz * dz);
    if (d > PED_REMOVE * 1.5f || (d > PED_REMOVE && !inView(eye, fwd, e->x, e->y + 1.f, e->z, 1.f, t->pedDrawDist))) e->mode = TRP_OFF;
  }
  if ((t->pedScanT -= dt) <= 0.f || t->fresh) { t->pedScanT = .5f; if (!t->streetRace) spawnPeds(t, w, ppos, eye, fwd); }
  }
  t->fresh = 0;
  TP(3);
  // --- препятствия для водителей (стоящие машины gapAhead смотрит сам)
  Obs obs[2 + TR_MAXPED + TR_MAXCAR + TR_MAXEXTRA]; int no = 0, carObs[TR_MAXCAR];
  if (player) { obsCar(&obs[no], player, -1); obs[no++].kind = OBK_PLAYER; }
  for (int i = 0; i < t->nextra && i < TR_MAXEXTRA; i++) if (t->extra[i]) { obsCar(&obs[no], t->extra[i], -1); obs[no++].kind = OBK_MCAR; }   // машины миссий
  if (!driving) { obsCircle(&obs[no], ppos[0], ppos[2], HOMER_RADIUS + .5f); obs[no++].kind = OBK_HOMER; }
  for (int i = 0; i < t->maxPeds; i++) if (t->ped[i].mode != TRP_OFF) { obsCircle(&obs[no], t->ped[i].x, t->ped[i].z, TR_PED_R + .4f); obs[no++].kind = OBK_PED; }
  for (int i = 0; i < t->maxCars; i++) {
    carObs[i] = -1; if (t->car[i].mode == TRM_OFF) continue;
    carObs[i] = no; obsCar(&obs[no], &t->car[i].car, i); obs[no++].kind = OBK_TRAF;
  }
  TP(4);
  // --- езда и физика
  t->activeCars = t->movingCars = 0;
  for (int i = 0; i < t->maxCars; i++) {
    TrCar *c = &t->car[i]; if (c->mode == TRM_OFF) continue;
    t->activeCars++; if (c->hitCool > 0.f) c->hitCool -= dt;
    if (c->mode == TRM_DRIVE) {
      driveStep(t, w, c, obs, no, dt);
      if (carObs[i] >= 0) { obsCar(&obs[carObs[i]], &c->car, i); obs[carObs[i]].kind = OBK_TRAF; }
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
    if (trfOneTapDeath && player && c->car.damage > dmg && c->hitCool > .9f) { c->car.damage = 1.f; c->car.destroyed = 1; }   // чит «One Tap Traffic Death» (P16-17)
    svNoOneHit(&c->car, &c->dmgSeen, &c->dmgWin, dt);   // P01-09
    if (c->car.destroyed && c->mode != TRM_WRECK) c->mode = TRM_WRECK;
    if (!TRF_SV_HUSK_OLD && c->mode == TRM_WRECK && carBoomed(&c->car) && (!svHuskOk(t, c->model) || svHuskCount(t) > SV_HUSK_MAX)) c->mode = TRM_OFF;   // P03-05: без остова — пропадает; шестой остов — тоже
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
    svNoOneHit(&P->car, &P->dmgSeen, &P->dmgWin, dt);   // P01-09
    if (!TRF_SV_HUSK_OLD && carBoomed(&P->car) && (!svHuskOk(t, t->parkd[i].model) || svHuskCount(t) > SV_HUSK_MAX)) {   // P03-05: без остова — пропадает
      P->state = 0; P->wreck = 0; P->car.x = P->car.z = 1e7f; P->car.y = -1000.f; P->car.v = 0.f; P->car.damage = 0.f; P->car.destroyed = 0; continue; }
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
      carInit(&P->car, &t->params[k->model], w, k->x, k->y + .5f, k->z, k->h); P->car.svKind = 4; P->wreck = 0; }
  }
  TP(7);
  // --- пешеходы
  t->activePeds = 0;
  if (!TRF_SV_PANIC_OLD) {   // паника заразна: пешеход рядом с паникующим паникует сам
    for (int i = 0; i < t->maxPeds; i++) if (t->ped[i].mode == TRP_PANIC)
      for (int j = 0; j < t->maxPeds; j++) { TrPed *o = &t->ped[j]; if (j == i || o->mode != TRP_WALK) continue;   // заражается идущий по тропе: не стоящий и не говорящий
        float px_ = o->x - ppos[0], pz_ = o->z - ppos[2]; if (px_ * px_ + pz_ * pz_ > 35.f * 35.f) continue;   // не дальше 35 м от игрока
        float dx = o->x - t->ped[i].x, dy = o->y - t->ped[i].y, dz = o->z - t->ped[i].z, r = o->speed > 1.f ? 1.5f : 2.f;   // 1,5 м (быстрый) или 2 м (медленный), расстояние трёхмерное
        if (dx * dx + dy * dy + dz * dz < r * r) pedPanic(t, o, ppos[0], ppos[2]); }
  }
  if (!TRF_SV_PEDTALK_OLD) pedTalkScan(t, ppos);   // круг 3: каждый кадр
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (e->mode == TRP_OFF) continue;
    t->activePeds++;
    if (player) pedHitBy(t, i, player, dt);
    if (e->nearCool > 0.f) e->nearCool -= dt;
    if (e->talkCool > 0.f) e->talkCool -= dt;
    if (!TRF_SV_PEDDODGE_OLD) {   // 7.5: прогноз на 1 с по машине игрока и по машинам трафика
      if (player && driving) pedReactCar(t, i, player, driving, dt);
      for (int j = 0; j < t->maxCars; j++) if (t->car[j].mode == TRM_DRIVE || t->car[j].mode == TRM_PHYS) pedReactCar(t, i, &t->car[j].car, driving, dt);
    } else
    if (player && driving) pedNearMiss(t, i, player);
    for (int j = 0; j < t->maxCars; j++) if (t->car[j].mode == TRM_PHYS || t->car[j].mode == TRM_WRECK || (!TRF_SV_PEDHIT_OLD && t->car[j].mode == TRM_DRIVE)) pedHitBy(t, i, &t->car[j].car, dt);   // 7.6: сбивает любая машина, и едущая по рельсам
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
  // 3.1 (строка 2): удар задевает ОДИН объект — первый на отрезке; здесь — ближайший к центру удара из задетых (форма отрезка и порядок объектов — у homer.c/game.c, передано)
  int only = -1; float bd = 1e30f;
  if (!TRF_SV_KICK_OLD) {
    for (int i = 0; i < t->maxPeds; i++) { TrPed *e = &t->ped[i]; if (!pedStands(e)) continue;
      float ex = e->x - x, ey = e->y + .9f - y, ez = e->z - z, R = r + TR_PED_R + .3f, d2 = ex * ex + ey * ey + ez * ez;
      if (d2 <= R * R && d2 < bd) { bd = d2; only = i; } }
  }
  for (int i = 0; i < t->maxPeds; i++) {
    TrPed *e = &t->ped[i]; if (!pedStands(e)) continue;
    if (!TRF_SV_KICK_OLD && i != only) continue;
    float ex = e->x - x, ey = e->y + .9f - y, ez = e->z - z, R = r + TR_PED_R + .3f;
    if (ex * ex + ey * ey + ez * ez > R * R) continue;
    { float kv = 400.f / SV_PED_MASS * .70710678f;   // круг 5 (3.3): импульс 400 под 45° вверх, скорость = импульс / масса. Массы пешехода в данных диска не нашлось — по умолчанию 100 (как у предметов без массы, g11)
      if (TRF_SV_KICKF_OLD) pedFly(t, i, dx * 5.f, dz * 5.f, 3.f); else pedFly(t, i, dx * kv, dz * kv, kv); }
    if (!TRF_SV_KICKF_OLD) e->mouthT = 1.2f;   // возглас при ударе (P15-06)
    if (TRF_SV_KICK_OLD || !e->knocked) pushEvW(t, TR_EV_PED_KICK, i, e->x, e->y, e->z, HIT_KICK, t->pedm[e->model].name);   // 3.4: повторный пинок по уже сбитому события не даёт
    e->knocked = 1; n++;
  }
  trafficScare(t, x, z);   // 7.4: пинок игрока пугает пешеходов в 12 м (сбитый уже летит — паника его не трогает)
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
      if (carShowHusk(k)) { if (!svHuskOk(t, model)) continue; model = (int)t->hd->huskModel; }   // P03-05: у части моделей остова нет — после взрыва машина пропадает
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
