// Машины миссий с ИИ: что и по каким данным — viewer/msncar.h. Чистый C99, без PSP и malloc.
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "msncar.h"
#include "homer.h"   // HOMER_STEP / HOMER_HEIGHT: опора на крыше и высота Гомера (как у трафика)

// ---- правила порта (в данных нет; сверить с эталоном PS2)
#define LOOK0 5.f          // упреждение руля, м
#define LOOKV .45f         // + с * скорость
#define A_LAT 7.f          // поперечное ускорение в повороте, м/с2 (при TireGrip 2.4 машин игрока; у ИИ .con — до 11)
#define WP_R 10.f          // точка пути пройдена ближе 10 м (финиш гонки — тоже: сфера carsphere игрока 6.5 м + кузов)
#define CLOSE_DIRECT 20.f  // цель ближе — напрямую, без полос
#define LANE_SNAP 30.f     // полоса дальше — «вне дорог»: к точке напрямую
#define STUCK_V 1.f        // газ есть, а скорость ниже — застряла
#define STUCK_T 1.5f
#define BACK_T 1.2f
#define RESET_FAR 60.f     // не приближается к точке пути 10 с и игрок дальше — на свой путь
#define NOPROG_T 10.f      // без приближения к цели на 3 м столько секунд — «не едет»
#define UTURN 60.f         // цена разворота (на перекрёстке, на старте против полосы), м: разворот в три приёма ~6 с
#ifndef MC_CATCHUP_GEO_OLD
#define MC_CATCHUP_GEO_OLD 0   // 03.10: 1 — догонялка гонки по прежней мере (по прямой до любой точки пути; красный tests/aicar/catchup.sh)
#endif
// Аудит 09.10 п. 10 (правка 347, В42/В91): множитель догонялки гонки — к скорости ИГРОКА, а не к TopSpeed .con соперника.
// Диск: комментарий к SetStageAIRaceCatchupParams в scripts/missions/level01/m7i.mfk:40-45 описывает три числа как доли скорости игрока
// (при минимуме, без догонялки и при максимуме). Запись PS2 yt-bziOipI4gDI (радар, стрелка соперника; 4 к/с, 1,65 м на
// пиксель видео — подгонка карты радара l1hudmap к кадрам финиша): в m7 Смитерс (TopSpeed 150 км/ч, у игрока 120) 81% гонки едет
// ПОЗАДИ игрока на 8–39 м (квартили), впереди не дальше 53 м, догоняет со скоростью 4–8 м/с и финиширует ~20 м позади; его путь
// 1411 м по прямой проходит за те же 126 с, что игрок свои 2118, — ~11 м/с по прямой при своих 41,7. С множителем к своей TopSpeed порт
// отпускал его вперёд (0,5 × 41,7 = 20,8 м/с больше среднего темпа игрока PS2 ~17 м/с — бот проигрывал).
// Пока игрок пешком (запись 39:53–39:55: Гомер бежит к машине, Смитерс уходит на ~15–20 м/с) — без догонялки, свой ход .con.
// Потолок — прежний (TopSpeed × множитель), пол 3 м/с — прежний: правила порта. MC_CATCHUP_SPEED_OLD=1 — прежнее
// (красный tests/aicar/racepace.sh)
#ifndef MC_CATCHUP_SPEED_OLD
#define MC_CATCHUP_SPEED_OLD 0
#endif
#ifndef MC_BREAK_SPEED
#define MC_BREAK_SPEED 0   // отрицательный контроль гейта tests/aicar: 1 — TopSpeed не из .con (60 м/с)
#endif
#ifndef MC_BREAK_ROADS
#define MC_BREAK_ROADS 0   // отрицательный контроль: 1 — дороги не используются (прямо к точке сквозь дома)
#endif

// сверка P04 A13 / B2: точка пути пройдена, когда расстояние до неё (в 3D) меньше радиуса: 10 м; в бонус-игре (свой планировщик
// mcPlanHook, «Super Sprint» оригинала) 7 м. Было: расстояние в плоскости, везде 10 м. MC_WPR_OLD = 1 — прежнее
int mcSprint;   // №1116: бонус-игра («Super Sprint») с дорогами трассы: те же правила гонки Sprint, что с mcPlanHook (радиус точки 7 м, не ждут игрока), но путь — по дорогам e->tr
static float wpRad(void) { return MC_WPR_OLD || !(mcPlanHook || mcSprint) ? WP_R : 7.f; }
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
#ifndef MC_BREAK_HEADING
#define MC_BREAK_HEADING 0   // отрицательный контроль гейта tests/aicar: 1 — машина ставится курсом персонажа (без +π)
#endif
float mcCarHeading(float h) { return MC_BREAK_HEADING ? h : wrapPi(h + 3.14159265f); }

const char *const mcModeName[6] = {"NULL", "race", "target", "evade", "chase", "waypoint"};
static const McGeo *geoTab; static int geoN;
static int ieq(const char *a, const char *b) {
  if (!a || !b) return 0;
  for (; *a && *b; a++, b++) { int x = *a, y = *b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
  return *a == *b;
}
void mcSetGeo(const McGeo *g, int n) { geoTab = g; geoN = n; }
int mcGeoFromPack(const unsigned char *hdr, const unsigned char *dir, unsigned bytes, McGeo *out, int cap) {
  unsigned h[8]; if (!hdr || memcmp(hdr, "MCR1", 4)) return 0;
  memcpy(h, hdr, sizeof h);
  unsigned n = h[3], ver = h[1], infoAt = ver >= 2 && h[7] >= 128 && h[6] >= h[4] ? h[6] - h[4] : 0;
  int k = 0;
  for (unsigned i = 0; i < n && k < cap && (i + 1) * 176 <= bytes; i++) {
    const unsigned char *d = dir + i * 176; TrModel M; memcpy(&M, d + 16, sizeof M);
    McGeo *g = &out[k++]; memset(g, 0, sizeof *g); memcpy(g->name, d, 15);
    g->lift = M.lift; g->wheelRad = M.wheelRad; g->halfW = M.halfW; g->halfL = M.halfL; g->bodyOfs = M.bodyOfs;
    g->wheelX = M.wheelX; g->wheelF = M.wheelF; g->wheelR = M.wheelR; g->bodyLo = M.bodyLo; g->bodyHi = M.bodyHi;
    if (infoAt && infoAt + (i + 1) * h[7] <= bytes) { memcpy(g->info, dir + infoAt + i * h[7], 128); g->hasInfo = 1; }
  }
  return k;
}
const McGeo *mcGeoFind(const char *name) { for (int i = 0; i < geoN; i++) if (ieq(geoTab[i].name, name)) return &geoTab[i]; return 0; }

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float d2xz(const float *a, float x, float z) { float dx = a[0] - x, dz = a[2] - z; return dx * dx + dz * dz; }

int mcMode(const char *ai) {
  for (int i = 1; i < 6; i++) if (ieq(ai, mcModeName[i])) return i;
  return MC_PARKED;
}

// ================================================================ дороги (TRF1: полосы — ломаные с путём s от начала)
static const TrPt *lanePts(const Traffic *t, int r, int l) { const TrRoad *R = &t->road[r]; return t->pt + R->firstPt + (uint32_t)l * R->nPt; }
static void lanePos(const Traffic *t, int r, int l, float s, float p[3]) {
  const TrPt *P = lanePts(t, r, l); int n = t->road[r].nPt;
  if (s <= 0.f) { p[0] = P[0].x; p[1] = P[0].y; p[2] = P[0].z; return; }
  for (int k = 0; k + 1 < n; k++) {
    if (s > P[k + 1].s && k + 2 < n) continue;
    float L = P[k + 1].s - P[k].s, u = L > 1e-4f ? clampf((s - P[k].s) / L, 0.f, 1.f) : 0.f;
    p[0] = P[k].x + (P[k + 1].x - P[k].x) * u; p[1] = P[k].y + (P[k + 1].y - P[k].y) * u; p[2] = P[k].z + (P[k + 1].z - P[k].z) * u;
    return;
  }
}
static int roadsOk(const Traffic *t) { return t && t->hd && t->hd->nRoad && !MC_BREAK_ROADS; }
// круг 7, P04:A12 и СВЕРКА_С_ИСХОДНИКОМ:1.9 (проверено по оригиналу; данные g12-data: TrRoad.pad8 — сложность среза, L1: 15…23): путь строится по дорогам БЕЗ срезов;
// потом на каждом перекрёстке пути: среди срезов, выходящих из этого перекрёстка, берутся те, чей конец лежит на дальнейшем пути; бросок «умения» —
// случайное целое от меньшего числа SetVehicleAIParams включительно до большего исключительно (по умолчанию 15 и 25) плюс добавка догонялки (целая часть
// коэффициента догонялки × 20); срез годен, если бросок не меньше его сложности; из годных — самый дальний, он заменяет участок пути до своего конца.
// svSkOn = 0 — прежнее правило (срезы как обычные дороги графа). MC_SHORTCUT_OLD = 1 — срезы разрешены, если второе число > 0
static int svSkOn, svSkLo, svSkHi, svSkMod;
static int svStreet;   // уличная гонка 1 или 2 (McEnv.noRaceWait последнего кадра)
static unsigned svRng = 12345u;
static unsigned svRandU(void) { svRng = svRng * 1664525u + 1013904223u; return svRng >> 8; }
static int roadAllowed(const Traffic *t, int r, int shortcuts) { return !(t->road[r].flags & 1) || shortcuts; }
// ближайшая к (x, z) точка оси полосы; maxAng >= 0 — курс полосы в пределах ±maxAng от h. -> 1 и дорога, полоса, s, d2
// y < -1e8 — высота любая; иначе полоса не дальше LANE_DY по высоте (эстакада над улицей — не та дорога)
#define LANE_DY 3.5f
#define MC_MAXROAD 256
extern int mcRoadPlanOld;
static const float *rpBoxes(const Traffic *t);   // рамки дорог (rpEnsure): 4 массива по MC_MAXROAD (x0, x1, z0, z1); NULL — нет
// №1116 (сверка g03-aicars): точки WPR/WPL бонус-трасс стоят по высоте неточно (b02 WPL01/05/06 на 2,6–5,2 м выше полотна, b05 WPR04 ниже на 5, b07 WPL04 на 3,3):
// отсев по LANE_DY (3,5 м) уводил цель на полосу другого яруса (b02 против хода: цель (−12; 40) ложилась на конец дороги у центра, машина разворачивалась). Оригинал
// ищет ближайший элемент пути без отсева; у порта на время поиска цели бонус-гонки — по 3D-расстоянию. MC_SPRINT_GOALY_OLD = 1 — прежнее
#ifndef MC_SPRINT_GOALY_OLD
#define MC_SPRINT_GOALY_OLD 0
#endif
static int laneSoftY;
static int nearestLane(const Traffic *t, float x, float y, float z, float h, float maxAng, float maxd2, int shortcuts,
                       int *road, int *lane, float *sOut, float *d2Out) {
  float best = maxd2; int br = -1, bl = 0; float bs = 0.f;
  const float *bx = mcRoadPlanOld ? 0 : rpBoxes(t);   // ресёрч процессора 10.10: дорога, чья рамка дальше лучшего найденного (с запасом на округление), улучшить не может
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (!roadAllowed(t, (int)r, shortcuts)) continue;
    if (bx && r < MC_MAXROAD) {
      float x0 = bx[r], x1 = bx[MC_MAXROAD + r], z0 = bx[2 * MC_MAXROAD + r], z1 = bx[3 * MC_MAXROAD + r];
      float dx = x < x0 ? x0 - x : x > x1 ? x - x1 : 0.f, dz = z < z0 ? z0 - z : z > z1 ? z - z1 : 0.f, db = dx * dx + dz * dz;
      if (db > best * 1.0001f + 0.01f) continue; }
    for (int l = 0; l < t->road[r].lanes; l++) {
      const TrPt *P = lanePts(t, (int)r, l); int n = t->road[r].nPt;
      for (int i = 0; i + 1 < n; i++) {
        float ax = P[i + 1].x - P[i].x, az = P[i + 1].z - P[i].z, L2 = ax * ax + az * az; if (L2 < 1e-6f) continue;
        float u = clampf(((x - P[i].x) * ax + (z - P[i].z) * az) / L2, 0.f, 1.f);
        float qx = P[i].x + ax * u - x, qz = P[i].z + az * u - z, d2 = qx * qx + qz * qz;
        if (laneSoftY && y > -1e8f) { float dy = P[i].y + (P[i + 1].y - P[i].y) * u - y; d2 += dy * dy; }   // №1116: мягкая высота — 3D-расстояние без жёсткого отсева
        if (d2 >= best || (maxAng >= 0.f && fabsf(wrapPi(atan2f(ax, az) - h)) > maxAng)) continue;
        if (!laneSoftY && y > -1e8f && fabsf(P[i].y + (P[i + 1].y - P[i].y) * u - y) > LANE_DY) continue;
        best = d2; br = (int)r; bl = l; bs = P[i].s + (P[i + 1].s - P[i].s) * u;
      }
    }
  }
  if (br < 0) return 0;
  *road = br; *lane = bl; *sOut = bs; if (d2Out) *d2Out = best; return 1;
}
// RESET CAR по дорогам (день 11.10, свод ботов п. 5: возврат «в последнее безопасное место» ставил машину в карман — L7 (333,−26), K3 (219/222,−444) на L1/L4/L7):
// ближняя ось полосы по ходу движения (курс в пределах 70 градусов от направления скорости, не дальше 80 м, на той же высоте ±3,5 м; нет — любая
// высота), точка на 8 м впереди по полосе, курс вдоль неё. 0 — дорог нет или полосы рядом нет (вызывающий вернёт по-старому)
int mcResetPoint(const Traffic *t, float x, float y, float z, float h, float vx, float vz, float out[4]) {
  if (!t || !t->hd || !t->hd->nRoad) return 0;
  float dir = vx * vx + vz * vz > 4.f ? atan2f(vx, vz) : h; int r, l; float s, d2;
  // сперва полоса под машиной (до 4 м, любой курс — стоит на дороге, дальше по её направлению), потом по ходу (курс ±70°, до 40 м), потом любая до 80 м
  if (!nearestLane(t, x, y, z, dir, -1.f, 16.f, 0, &r, &l, &s, &d2) && !nearestLane(t, x, y, z, dir, 1.2217f, 40.f * 40.f, 0, &r, &l, &s, &d2) &&
      !nearestLane(t, x, y, z, dir, -1.f, 80.f * 80.f, 0, &r, &l, &s, &d2) && !nearestLane(t, x, -1e9f, z, dir, -1.f, 80.f * 80.f, 0, &r, &l, &s, &d2)) return 0;
  float len = t->road[r].length, s1 = s + 8.f; if (s1 > len - 2.f) s1 = fmaxf(0.f, len - 2.f);
  float p[3], q[3]; lanePos(t, r, l, s1, p); lanePos(t, r, l, fminf(len, s1 + 3.f), q);
  if (q[0] == p[0] && q[2] == p[2]) lanePos(t, r, l, fmaxf(0.f, s1 - 3.f), q), out[3] = atan2f(p[0] - q[0], p[2] - q[2]); else out[3] = atan2f(q[0] - p[0], q[2] - p[2]);
  out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; return 1;
}
float mcLaneDist(const Traffic *t, float x, float z) {
  int r, l; float s, d2;
  if (!t || !t->hd || !t->hd->nRoad) return 1e9f;
  return nearestLane(t, x, -1e9f, z, 0.f, -1.f, 1e12f, 1, &r, &l, &s, &d2) ? sqrtf(d2) : 1e9f;
}
// кратчайший путь по дорогам: состояние — дорога и направление проезда (0 — по полосе, от startJ к endJ; 1 — против неё).
// ПРАВИЛО ПОРТА (в данных нет, как ИИ миссий выбирает сторону дороги): машины миссий и бот-игрок едут по оси полос дороги в
// любую сторону — по встречной (обратный порядок точек полосы), если так короче; по своей полосе дешевле (WRONGWAY), так что
// на двусторонней улице (дорога и её rev) едут по своей стороне. Только по полосам трафика (дорога односторонняя) путь
// к точке за спиной — объезд квартала (у гонок уровня 1 путь по полосам в 1.6–5.7 раза длиннее прямой, гейт tests/aicar).
// Разворот на перекрёстке или на месте старта — +UTURN м. Дейкстра по состояниям; -> состояний в пути, 0 — нет пути
#define MC_MAXROAD 256
#ifndef MC_CHORD_ALWAYS
#define MC_CHORD_ALWAYS 0
#endif
#ifndef MC_CHORD_OLD
#define MC_CHORD_OLD 0   // 1 — без хорды 40 % на перекрёстках (только середина дороги)
#endif
// сверка P16-05 (+ P04 A1, документ СВЕРКА_ПОЛНАЯ): обратный ход по дороге в плане пути стоит 1,30 длины, не 1,15. MC_PATHMID_OLD = 1 — прежнее 1,15
// круг 3: у погони и в уличных гонках 1 и 2 множитель 1,0 в обе стороны (ИИ ездит по обеим сторонам); задаёт replan, после планирования возвращается
#if MC_PATHMID_OLD
#define WRONGWAY_BASE 1.15f
#else
#define WRONGWAY_BASE 1.30f
#endif
static float wrongWayK = WRONGWAY_BASE;
#define WRONGWAY (wrongWayK)
// дороги, где эта машина уже не проехала (застряла, не приближалась к цели): +AVOID м к цене — объезд (правило порта, как
// объезд места тупика у миссионера); список — у машины (McCar.avoid), планировщику — на время вызова
#define AVOID 400.f
static const short *avoidR; static int avoidN;
// дорога, чью ось перекрывает твёрдая коллизия уровня (кузов на оси полосы глубже 0.3 м, как станок tools/cartest/roadblock.c:
// на уровне 1 — 16 проб из 6795 на 5 дорогах, коробки поперёк полос — пункт 11 очереди): +BLOCKED м к цене. ПРАВИЛО ПОРТА: путь
// ИИ по такой дороге — только если другого нет. Проверка — по требованию: дорога проверяется, когда впервые попала в путь (на
// PSP сразу на экране загрузки — mcProbeRoads, ~4500 проб; иначе по требованию); кэш на уровень (сменился traffic.shr — заново)
#define BLOCKED 2000.f
static const ColWorld *probeW; static const void *probedFor;
static unsigned char roadChecked[256], roadBad[256];
static float avoidCost(int r) { float c = r < 256 && roadBad[r] ? BLOCKED : 0.f; for (int i = 0; i < avoidN; i++) if (avoidR[i] == r) return c + AVOID; return c; }
// правка 347: уступ вверх по ходу. Срезы уровня (дороги диска с флагом shortcut) спускаются с подпорных стен и бордюров дворов:
// по своему направлению машина спрыгивает, против — упирается в стену. Правило порта «по оси полос в любую сторону» (выше) вело
// машины миссий и бота-игрока ВВЕРХ по таким срезам: S-M-R-T — Скиннер в каждой гонке на 24 м/с в стену 2 м у (231, 338)
// (срез 13, уступ +1.7 м), стоит в ней 7–25 с, бот по тому же пути — тоже. [диск] уровень 1: таких мест 11 на 9 срезах, все только
// против хода дороги; L2–L7: 5–15 на уровень против хода. ПРАВИЛО ПОРТА: проезд дороги ПРОТИВ её хода (сама встречная — правило порта
// выше), где опора по оси полосы за 1 м поднимается выше LEDGE_UP, — как перекрытая дорога (+BLOCKED: только если другого пути
// нет); где именно — ledgeLo/Hi (путь s полосы), чтобы начало и конец пути на этой дороге судить по своему куску. По ходу дороги
// не судим (по ходу её задал диск; редкие «уступы по ходу» L2–L7 — 1–2 на уровень, могут быть дырой в коллизии под эстакадой).
// mcLedgeOld = 1 — прежнее (сценарий flag ledgeold 1, сборка -DMC_LEDGE_OLD=1)
#ifndef MC_LEDGE_OLD
#define MC_LEDGE_OLD 0
#endif
int mcLedgeOld = MC_LEDGE_OLD;
// правка Роба 486 (вопрос В92, замер tools/cartest/ledge.c по рисунку и коллизии, 10.10): «уступы по ходу дороги» на L1–L7 — не дыры под
// эстакадой, а настоящие нарисованные уступы: на 8 местах по ходу 6 «wall» (внизу рисунка нет, верх уступа нарисован: L1/L4 дорога 548
// 2,28 м у (−174; 233); L2/L5 дорога 80 0,99 м; L7 дорога 682 4,48 м у (333; −31)), 2 «ghost» (L3/L6: опора выше без рисунка, 2,2–2,7 м),
// «over» (эстакада над нарисованной дорогой) — 0 на всех уровнях. Поэтому уступ вверх судится и по ходу дороги (направление 0), как против.
// MC_LEDGE_FWD_OLD=1 / mcLedgeFwdOld = 1 — по ходу не судить (правило правки 347)
#ifndef MC_LEDGE_FWD_OLD
#define MC_LEDGE_FWD_OLD 0
#endif
int mcLedgeFwdOld = MC_LEDGE_FWD_OLD;
#define LEDGE_UP .8f
static unsigned char roadLedge[256];   // бит d: по направлению d (0 — по полосе, 1 — против) есть уступ вверх
static float ledgeLo[256][2], ledgeHi[256][2];
// №517 (Наблюдатель/дирижёр 11.10, железо SHARTEST12: LOADGAP ~180 мс на 25 % загрузки, огонь и газета стоят): проверка дорог гоняла кадр газеты только между дорогами, а одна длинная
// дорога считается ~180 мс. Теперь кадр зовётся и внутри дороги, каждые 16 проб (loadPump сам не рисует чаще LOAD_PUMP_MS). MCPROBE_PUMP_OLD=1 — только между дорогами
#ifndef MCPROBE_PUMP_OLD
#define MCPROBE_PUMP_OLD 0
#endif
void (*mcProbePump)(void);
static unsigned probeTick;
static void probePumpMaybe(void) { if (!MCPROBE_PUMP_OLD && mcProbePump && (++probeTick & 15u) == 0) mcProbePump(); }
static void probeLedge(const Traffic *t, int r) {
  const TrRoad *R = &t->road[r]; roadLedge[r] = 0;
  ledgeLo[r][0] = 1e30f; ledgeHi[r][0] = -1e30f;
  for (int d = mcLedgeFwdOld ? 1 : 0; d < 2; d++) { ledgeLo[r][d] = 1e30f; ledgeHi[r][d] = -1e30f;   // против хода (правка 347) и по ходу (правка 486)
    for (int l = 0; l < R->lanes; l++) { const TrPt *P = lanePts(t, r, l); int n = R->nPt, have = 0; float prev = 0.f;
      for (int kk = 0; kk + 1 < n; kk++) { int a = d ? n - 1 - kk : kk, b = d ? a - 1 : a + 1;
        float dx = P[b].x - P[a].x, dz = P[b].z - P[a].z, L = sqrtf(dx * dx + dz * dz);
        for (float u = 0.f; u < L; u += 1.f) { probePumpMaybe(); float x = P[a].x + dx * u / L, y = P[a].y + (P[b].y - P[a].y) * u / L, z = P[a].z + dz * u / L, g;
          if (!have) { if (colGround(probeW, x, z, y + 1.5f, y - 3.f, &g, 0)) { prev = g; have = 1; } continue; }
          if (colGround(probeW, x, z, prev + LEDGE_UP, prev - 6.f, &g, 0)) { prev = g; continue; }   // ровно, склон, спуск, спрыгнуть
          if (colGround(probeW, x, z, prev + 6.f, prev + LEDGE_UP, &g, 0)) {   // опора только выше — стена/уступ вверх
            float sp = P[a].s + (P[b].s - P[a].s) * u / L; roadLedge[r] |= (unsigned char)(1u << d);
            if (sp < ledgeLo[r][d]) ledgeLo[r][d] = sp; if (sp > ledgeHi[r][d]) ledgeHi[r][d] = sp; prev = g; }
        } } } }
}
// №1147 (вождение бонус-трасс): уступы вверх на оси дороги — правило порта для срезов-дворов улиц (правка 347); на бонус-трассах это подъёмы мостов и трамплины (b07 R3: уступ в оба хода),
// и дорога с «уступом» получала +2000 м к цене: машина вместо цели на дороге ехала в обход всей трассы против хода. На трассах уступы не судим. MC_SPRINT_LEDGE_OLD = 1 — прежнее
#ifndef MC_SPRINT_LEDGE_OLD
#define MC_SPRINT_LEDGE_OLD 0
#endif
static float ledgeCost(int r, int d) { return (MC_SPRINT_LEDGE_OLD || !mcSprint) && !mcLedgeOld && r < 256 && (roadLedge[r] >> d & 1) ? BLOCKED : 0.f; }
// кусок дороги r от s0 по направлению d (до конца, если s1 < 0 — иначе до s1) проходит уступ вверх?
static float ledgeCostPart(int r, int d, float s0, float s1) {
  if (mcLedgeOld || (!MC_SPRINT_LEDGE_OLD && mcSprint) || r >= 256 || !(roadLedge[r] >> d & 1)) return 0.f;
  if (d == 0) return ledgeHi[r][0] > s0 && (s1 < 0.f || ledgeLo[r][0] < s1) ? BLOCKED : 0.f;
  return ledgeLo[r][1] < s0 && (s1 < 0.f || ledgeHi[r][1] > s1) ? BLOCKED : 0.f;
}
static int probeRoad(const Traffic *t, int r) {   // -> 1: ось полосы перекрыта
  const TrRoad *R = &t->road[r];
  for (int l = 0; l < R->lanes; l++) { const TrPt *P = lanePts(t, r, l);
    for (int k = 0; k + 1 < R->nPt; k++) { float dx = P[k + 1].x - P[k].x, dz = P[k + 1].z - P[k].z, L = sqrtf(dx * dx + dz * dz);
      for (float s = 0.f; s < L; s += 3.f) {   // шаг 3 м (коробки поперёк полос — от 4 м вдоль)
        probePumpMaybe(); float x = P[k].x + dx * s / L, y = P[k].y + (P[k + 1].y - P[k].y) * s / L, z = P[k].z + dz * s / L;
        Car c; carInit(&c, NULL, probeW, x, y + .5f, z, atan2f(dx, dz)); if (fabsf(c.y - y) > 2.f) continue;   // другая опора — не судим
        CarBox b; carBox(&c, &b); colProbeCar(probeW, &b); if (b.maxDepth > .3f) return 1;
      } } }
  return 0;
}
// mcProbePump объявлен выше   // правка 244: кадр газеты между дорогами (viewer/main.c loadPump) — проверка идёт ~0,6 с
int mcProbeRoads(const Traffic *t, const ColWorld *w) {   // все дороги уровня сразу (экран загрузки): в пути потом без проверок
  if (!t || !t->hd || !w) return 0;
  const ColWorld *keep = probeW; probeW = w;
  memset(roadChecked, 0, sizeof roadChecked); memset(roadBad, 0, sizeof roadBad); memset(roadLedge, 0, sizeof roadLedge); probedFor = t->hd;
  int n = (int)t->hd->nRoad < 256 ? (int)t->hd->nRoad : 256, bad = 0;
  for (int r = 0; r < n; r++) { if (mcProbePump) mcProbePump(); roadChecked[r] = 1; roadBad[r] = (unsigned char)probeRoad(t, r); probeLedge(t, r); bad += roadBad[r]; }
  probeW = keep; return bad;
}
// Загрузка 2 (пункт 1): результат mcProbeRoads запечён офлайн (tools/mcp_bake.c) в файл mcpr.shr уровня, секция MCP1: хеши входов
// (дороги и точки traffic.shr + сырой COL1 level.shr) + roadBad/roadLedge/ledgeLo/ledgeHi. Хеш сошёлся — взять, нет — считать как раньше
// (mcProbeRoads). MCPROBE_BAKE_OLD=1 — сборка без запечённого. MC_BAKE_VER растёт при любой правке проб (probeRoad/probeLedge/константы)
#define MC_BAKE_VER 1u
unsigned mcProbeHash(const void *p, unsigned n) {   // 32 бита, по словам: 3,7 МБ COL1 — единицы мс (побайтный FNV был бы ×4 дольше)
  const unsigned char *b = (const unsigned char *)p; unsigned h = 2166136261u ^ n, i = 0;
  for (; i + 4 <= n; i += 4) { unsigned w = (unsigned)b[i] | (unsigned)b[i + 1] << 8 | (unsigned)b[i + 2] << 16 | (unsigned)b[i + 3] << 24; h = (h ^ w) * 16777619u; h ^= h >> 15; }
  for (; i < n; i++) h = (h ^ b[i]) * 16777619u;
  return h;
}
static unsigned trafficRoadHash(const Traffic *t) {
  return mcProbeHash(t->road, t->hd->nRoad * (unsigned)sizeof(TrRoad)) * 31u ^ mcProbeHash(t->pt, t->hd->nPt * (unsigned)sizeof(TrPt));
}
typedef struct { char magic[4]; uint32_t ver, hTraffic, hCol, nRoad, nBad, mcLedgeFwdOld, hBody; } McpHead;   // hBody — хеш массивов после заголовка (порча файла)   // 32 Б, дальше bad[256] ledge[256] lo[512] hi[512]
unsigned mcProbeBakeBuild(const Traffic *t, unsigned colHash, unsigned char *out) {   // после mcProbeRoads: массивы -> MCP1; -> байт
  McpHead *h = (McpHead *)out; memset(out, 0, MCP_FILE_BYTES); memcpy(h->magic, "MCP1", 4);
  int n = (int)t->hd->nRoad < 256 ? (int)t->hd->nRoad : 256, bad = 0; for (int r = 0; r < n; r++) bad += roadBad[r];
  h->ver = MC_BAKE_VER; h->hTraffic = trafficRoadHash(t); h->hCol = colHash; h->nRoad = t->hd->nRoad; h->nBad = (uint32_t)bad; h->mcLedgeFwdOld = (uint32_t)mcLedgeFwdOld;
  unsigned char *p = out + 32; memcpy(p, roadBad, 256); memcpy(p + 256, roadLedge, 256);
  float *f = (float *)(p + 512); for (int r = 0; r < 256; r++) for (int d = 0; d < 2; d++) { f[r * 2 + d] = r < n ? ledgeLo[r][d] : 1e30f; f[512 + r * 2 + d] = r < n ? ledgeHi[r][d] : -1e30f; }
  h->hBody = mcProbeHash(out + 32, MCP_FILE_BYTES - 32);
  return MCP_FILE_BYTES;
}
int mcProbeBakedLoad(const Traffic *t, unsigned colHash, const void *blob, unsigned size) {   // -> число перекрытых дорог, -1: нет/не тот (считать)
  const McpHead *h = (const McpHead *)blob;
  if (!t || !t->hd || !blob || size < MCP_FILE_BYTES || memcmp(h->magic, "MCP1", 4) || h->ver != MC_BAKE_VER || h->nRoad != t->hd->nRoad ||
      h->hTraffic != trafficRoadHash(t) || h->hCol != colHash || h->mcLedgeFwdOld != (uint32_t)mcLedgeFwdOld ||
      h->hBody != mcProbeHash((const unsigned char *)blob + 32, MCP_FILE_BYTES - 32)) return -1;
  int n = (int)t->hd->nRoad < 256 ? (int)t->hd->nRoad : 256;
  memset(roadChecked, 0, sizeof roadChecked); memset(roadBad, 0, sizeof roadBad); memset(roadLedge, 0, sizeof roadLedge); probedFor = t->hd;
  const unsigned char *p = (const unsigned char *)blob + 32; const float *f = (const float *)(p + 512); int bad = 0;
  for (int r = 0; r < n; r++) { roadChecked[r] = 1; roadBad[r] = p[r]; roadLedge[r] = p[256 + r]; bad += roadBad[r];
    for (int d = 0; d < 2; d++) { ledgeLo[r][d] = f[r * 2 + d]; ledgeHi[r][d] = f[512 + r * 2 + d]; } }
  return bad;
}
int mcProbeStateSame(const unsigned char *blob) {   // сверка: текущие (посчитанные) массивы == запечённые в blob; 1 — совпали
  const unsigned char *p = blob + 32; const float *f = (const float *)(p + 512);
  for (int r = 0; r < 256; r++) { if (!roadChecked[r]) continue; if (roadBad[r] != p[r] || roadLedge[r] != p[256 + r]) return 0;
    for (int d = 0; d < 2; d++) if (ledgeLo[r][d] != f[r * 2 + d] || ledgeHi[r][d] != f[512 + r * 2 + d]) return 0; }
  return 1;
}
// путь из planRoads: дороги, ещё не проверенные, — проверить; -> 1, если нашлась перекрытая (путь надо переложить)
static int probePath(const Traffic *t, const int *path, int np) {
  if (!probeW) return 0;
  if (probedFor != (const void *)t->hd) { memset(roadChecked, 0, sizeof roadChecked); memset(roadBad, 0, sizeof roadBad); memset(roadLedge, 0, sizeof roadLedge); probedFor = t->hd; }
  int found = 0;
  for (int i = 0; i < np; i++) { int r = path[i] >> 1; if (r >= 256 || roadChecked[r]) continue;
    roadChecked[r] = 1; if ((roadBad[r] = (unsigned char)probeRoad(t, r))) found = 1;
    probeLedge(t, r); if (!mcLedgeOld && (roadLedge[r] >> (path[i] & 1) & 1)) found = 1; }   // правка 347: путь вверх по уступу — переложить
  return found;
}
static float laneHeading(const Traffic *t, int r, int l, float s) {   // курс оси полосы в точке s (по направлению полосы)
  const TrPt *P = lanePts(t, r, l); int n = t->road[r].nPt, k = 0;
  while (k + 2 < n && P[k + 1].s < s) k++;
  return atan2f(P[k + 1].x - P[k].x, P[k + 1].z - P[k].z);
}
static int isUturn(const Traffic *t, int u, int o) {
  return o == u || (unsigned)o == t->road[u].rev || t->road[o].rev == (unsigned)u;
}
static int planRoadsOld(const Traffic *t, int r0, int l0, float s0, float h, int rg, float sg, int shortcuts, int *path, int cap) {
  int n = (int)t->hd->nRoad; if (n > MC_MAXROAD) n = MC_MAXROAD;
  if (r0 >= n || rg >= n) return 0;   // правка 347: дорога вне таблиц (больше MC_MAXROAD) — пути нет (не писать за край dist)
  static float dist[2 * MC_MAXROAD]; static short prev[2 * MC_MAXROAD]; static unsigned char done[2 * MC_MAXROAD];
  for (int i = 0; i < 2 * n; i++) { dist[i] = 1e30f; prev[i] = -1; done[i] = 0; }
  const TrRoad *R0 = &t->road[r0];
  float hl = laneHeading(t, r0, l0, s0), back = fabsf(wrapPi(hl - h)) > 1.5708f;   // машина смотрит против полосы
  dist[2 * r0] = (fmaxf(0.f, R0->length - s0)) + (back ? UTURN : 0.f) + ledgeCostPart(r0, 0, s0, -1.f);
  dist[2 * r0 + 1] = s0 * WRONGWAY + (back ? 0.f : UTURN) + ledgeCostPart(r0, 1, s0, -1.f);
  float bestGoal = 1e30f; int goalState = -1, goalPrev = -1;
  if (r0 == rg) {   // та же дорога: прямо к точке, если она в эту сторону
    float c0 = sg >= s0 ? sg - s0 + (back ? UTURN : 0.f) + ledgeCostPart(r0, 0, s0, sg) : 1e30f,
          c1 = sg <= s0 ? (s0 - sg) * WRONGWAY + (back ? 0.f : UTURN) + ledgeCostPart(r0, 1, s0, sg) : 1e30f;
    if (c0 < bestGoal) { bestGoal = c0; goalState = -2; }
    if (c1 < bestGoal) { bestGoal = c1; goalState = -3; }
  }
  for (;;) {
    int u = -1; float du = 1e30f;
    for (int i = 0; i < 2 * n; i++) if (!done[i] && dist[i] < du) { du = dist[i]; u = i; }
    if (u < 0 || du >= bestGoal) break;
    done[u] = 1;
    int ur = u >> 1, ud = u & 1; const TrRoad *U = &t->road[ur];
    unsigned ej = ud ? U->startJ : U->endJ;   // перекрёсток, где кончается проезд
    for (int o = 0; o < n; o++) {
      const TrRoad *O = &t->road[o];
      for (int d = 0; d < 2; d++) {
        if ((d ? O->endJ : O->startJ) != ej) continue;                      // въезд на дорогу o с этого перекрёстка
        if (!roadAllowed(t, o, shortcuts) && o != rg) continue;
        float turn = isUturn(t, ur, o) ? UTURN : 0.f;
        if (o == rg) {   // точка цели на этой дороге
          float c = du + turn + (d ? (O->length - sg) * WRONGWAY : sg) + ledgeCostPart(o, d, d ? O->length + 1.f : -1e-3f, sg);
          if (c < bestGoal) { bestGoal = c; goalState = 2 * o + d; goalPrev = u; }
        }
        int st = 2 * o + d; if (done[st]) continue;
        float nd = du + turn + O->length * (d ? WRONGWAY : 1.f) + avoidCost(o) + ledgeCost(o, d);
        if (nd < dist[st]) { dist[st] = nd; prev[st] = (short)u; }
      }
    }
  }
  if (goalState == -1) return 0;
  if (goalState < 0) { path[0] = 2 * r0 + (goalState == -3); return 1; }   // та же дорога
  int tmp[2 * MC_MAXROAD], k = 0; tmp[k++] = goalState;
  for (int u = goalPrev; u >= 0 && k < 2 * MC_MAXROAD; u = prev[u]) { tmp[k++] = u; if ((u >> 1) == r0 && prev[u] < 0) break; }
  if ((tmp[k - 1] >> 1) != r0) return 0;
  int m = k < cap ? k : cap;
  for (int i = 0; i < m; i++) path[i] = tmp[k - 1 - i];
  return m;
}
// Правка «быстрый расчёт» (ресёрч процессора 10.10, P10): planRoadsOld выбирал вершину перебором всех 2n состояний и для КАЖДОЙ вершины перебирал ВСЕ n дорог
// с проверкой перекрёстка (≈ 4n²): mcPlan 23–56 мкс на Маке в среднем, до 198 макс (7–20 мс на PSP в одном кадре), а replan зовёт его до 4 раз подряд и
// раз в секунду на каждого преследователя. Тот же Дейкстра: дороги, начинающиеся/кончающиеся на перекрёстке j, лежат в списке j в том же порядке
// (дорога, сторона), что давал прежний двойной цикл; очередь — двоичная куча по паре (расстояние, номер состояния) — тот же выбор, что у перебора с
// «<» (первый с наименьшим расстоянием). Расчёт расстояний тот же (те же слагаемые, тот же порядок). Результат путей бит в бит
// (tests/aicar/roadplan.c). -DROADPLAN_OLD=1 или mcRoadPlanOld = 1 — прежний перебор
#ifndef ROADPLAN_OLD
#define ROADPLAN_OLD 0
#endif
int mcRoadPlanOld = ROADPLAN_OLD;
#ifndef ROADPLAN_QUEUE_OLD
#define ROADPLAN_QUEUE_OLD 0
#endif
#ifndef MCAR_WARM_OLD
#define MCAR_WARM_OLD 0
#endif
static int replanBudget = 1000;   // плановых перепланировок, ещё доступных в этом кадре (вне mcUpdate — без ограничения)
#ifndef ROADPLAN_BREAK
#define ROADPLAN_BREAK 0   // только красный контроль гейта: ломает порядок равных расстояний
#endif
#define RP_MAXJ 1024
#define RP_HEAP 4096
static struct { const void *hd; unsigned sig; int n, nj; short start[RP_MAXJ + 1]; short ent[2 * MC_MAXROAD]; float bx0[MC_MAXROAD], bx1[MC_MAXROAD], bz0[MC_MAXROAD], bz1[MC_MAXROAD]; } rpAdj;   // ent: 2*o+d
static int rpEnsure(const Traffic *t, int n) {
  unsigned sig = 2166136261u; int maxj = -1;
  for (int o = 0; o < n; o++) { unsigned a = t->road[o].startJ, b = t->road[o].endJ;
    if (a >= RP_MAXJ || b >= RP_MAXJ) return 0;
    sig = (sig ^ a) * 16777619u; sig = (sig ^ b) * 16777619u; if ((int)a > maxj) maxj = (int)a; if ((int)b > maxj) maxj = (int)b; }
  if (rpAdj.hd == t->hd && rpAdj.sig == sig && rpAdj.n == n) return 1;
  int nj = maxj + 1; static short cnt[RP_MAXJ + 1]; memset(cnt, 0, sizeof(short) * (size_t)(nj + 1));
  for (int o = 0; o < n; o++) for (int d = 0; d < 2; d++) cnt[(d ? t->road[o].endJ : t->road[o].startJ) + 1]++;
  rpAdj.start[0] = 0; for (int j = 0; j < nj; j++) rpAdj.start[j + 1] = (short)(rpAdj.start[j] + cnt[j + 1]);
  static short fill[RP_MAXJ]; for (int j = 0; j < nj; j++) fill[j] = rpAdj.start[j];
  for (int o = 0; o < n; o++) for (int d = 0; d < 2; d++) { int j = d ? t->road[o].endJ : t->road[o].startJ; rpAdj.ent[fill[j]++] = (short)(2 * o + d); }
  for (int o = 0; o < n; o++) { float x0 = 1e30f, x1 = -1e30f, z0 = 1e30f, z1 = -1e30f; const TrRoad *R = &t->road[o];
    for (int l = 0; l < R->lanes; l++) { const TrPt *P = lanePts(t, o, l); for (int i = 0; i < R->nPt; i++) {
      if (P[i].x < x0) x0 = P[i].x; if (P[i].x > x1) x1 = P[i].x; if (P[i].z < z0) z0 = P[i].z; if (P[i].z > z1) z1 = P[i].z; } }
    rpAdj.bx0[o] = x0; rpAdj.bx1[o] = x1; rpAdj.bz0[o] = z0; rpAdj.bz1[o] = z1; }
  rpAdj.hd = t->hd; rpAdj.sig = sig; rpAdj.n = n; rpAdj.nj = nj; return 1;
}
static const float *rpBoxes(const Traffic *t) {
  int n = (int)t->hd->nRoad; if (n > MC_MAXROAD) n = MC_MAXROAD;
  if (!rpEnsure(t, n)) return 0;
  return rpAdj.bx0;   // bx0, bx1, bz0, bz1 лежат подряд в структуре, по MC_MAXROAD
}
typedef struct { float d; int i; } RpH;
static RpH rpHeap[RP_HEAP]; static int rpN;
static int rpLess(RpH a, RpH b) { return a.d < b.d || (a.d == b.d && (ROADPLAN_BREAK ? a.i > b.i : a.i < b.i)); }
static int rpPush(float d, int i) {
  if (rpN >= RP_HEAP) return 0;
  int k = rpN++; RpH e = {d, i};
  while (k > 0) { int p = (k - 1) >> 1; if (!rpLess(e, rpHeap[p])) break; rpHeap[k] = rpHeap[p]; k = p; }
  rpHeap[k] = e; return 1;
}
static RpH rpPop(void) {
  RpH top = rpHeap[0], e = rpHeap[--rpN]; int k = 0;
  for (;;) { int c = 2 * k + 1; if (c >= rpN) break; if (c + 1 < rpN && rpLess(rpHeap[c + 1], rpHeap[c])) c++; if (!rpLess(rpHeap[c], e)) break; rpHeap[k] = rpHeap[c]; k = c; }
  if (rpN > 0) rpHeap[k] = e;
  return top;
}
static int planRoads(const Traffic *t, int r0, int l0, float s0, float h, int rg, float sg, int shortcuts, int *path, int cap) {
  if (mcRoadPlanOld) return planRoadsOld(t, r0, l0, s0, h, rg, sg, shortcuts, path, cap);
  int n = (int)t->hd->nRoad; if (n > MC_MAXROAD) n = MC_MAXROAD;
  if (r0 >= n || rg >= n) return 0;
  if (!rpEnsure(t, n)) return planRoadsOld(t, r0, l0, s0, h, rg, sg, shortcuts, path, cap);
  static float dist[2 * MC_MAXROAD]; static short prev[2 * MC_MAXROAD]; static unsigned char done[2 * MC_MAXROAD];
  for (int i = 0; i < 2 * n; i++) { dist[i] = 1e30f; prev[i] = -1; done[i] = 0; }
  rpN = 0;
  const TrRoad *R0 = &t->road[r0];
  float hl = laneHeading(t, r0, l0, s0), back = fabsf(wrapPi(hl - h)) > 1.5708f;
  dist[2 * r0] = (fmaxf(0.f, R0->length - s0)) + (back ? UTURN : 0.f) + ledgeCostPart(r0, 0, s0, -1.f);
  dist[2 * r0 + 1] = s0 * WRONGWAY + (back ? 0.f : UTURN) + ledgeCostPart(r0, 1, s0, -1.f);
  float bestGoal = 1e30f; int goalState = -1, goalPrev = -1;
  if (r0 == rg) {
    float c0 = sg >= s0 ? sg - s0 + (back ? UTURN : 0.f) + ledgeCostPart(r0, 0, s0, sg) : 1e30f,
          c1 = sg <= s0 ? (s0 - sg) * WRONGWAY + (back ? 0.f : UTURN) + ledgeCostPart(r0, 1, s0, sg) : 1e30f;
    if (c0 < bestGoal) { bestGoal = c0; goalState = -2; }
    if (c1 < bestGoal) { bestGoal = c1; goalState = -3; }
  }
  if (dist[2 * r0] < 1e30f && !rpPush(dist[2 * r0], 2 * r0)) return planRoadsOld(t, r0, l0, s0, h, rg, sg, shortcuts, path, cap);
  if (dist[2 * r0 + 1] < 1e30f && !rpPush(dist[2 * r0 + 1], 2 * r0 + 1)) return planRoadsOld(t, r0, l0, s0, h, rg, sg, shortcuts, path, cap);
  for (;;) {
    int u = -1; float du = 1e30f;
    while (rpN > 0) { RpH e = rpPop(); if (done[e.i] || e.d > dist[e.i]) continue; u = e.i; du = e.d; break; }
    if (u < 0 || du >= bestGoal) break;
    done[u] = 1;
    int ur = u >> 1, ud = u & 1; const TrRoad *U = &t->road[ur];
    unsigned ej = ud ? U->startJ : U->endJ;
    for (int a = rpAdj.start[ej]; a < rpAdj.start[ej + 1]; a++) {
      int o = rpAdj.ent[a] >> 1, d = rpAdj.ent[a] & 1; const TrRoad *O = &t->road[o];
      if (!roadAllowed(t, o, shortcuts) && o != rg) continue;
      float turn = isUturn(t, ur, o) ? UTURN : 0.f;
      if (o == rg) {
        float c = du + turn + (d ? (O->length - sg) * WRONGWAY : sg) + ledgeCostPart(o, d, d ? O->length + 1.f : -1e-3f, sg);
        if (c < bestGoal) { bestGoal = c; goalState = 2 * o + d; goalPrev = u; }
      }
      int st = 2 * o + d; if (done[st]) continue;
      float nd = du + turn + O->length * (d ? WRONGWAY : 1.f) + (ROADPLAN_BREAK == 2 ? 0.f : avoidCost(o)) + ledgeCost(o, d);
      if (nd < dist[st]) { dist[st] = nd; prev[st] = (short)u; if (!rpPush(nd, st)) return planRoadsOld(t, r0, l0, s0, h, rg, sg, shortcuts, path, cap); }
    }
  }
  if (goalState == -1) return 0;
  if (goalState < 0) { path[0] = 2 * r0 + (goalState == -3); return 1; }
  int tmp[2 * MC_MAXROAD], k = 0; tmp[k++] = goalState;
  for (int u = goalPrev; u >= 0 && k < 2 * MC_MAXROAD; u = prev[u]) { tmp[k++] = u; if ((u >> 1) == r0 && prev[u] < 0) break; }
  if ((tmp[k - 1] >> 1) != r0) return 0;
  int m = k < cap ? k : cap;
  for (int i = 0; i < m; i++) path[i] = tmp[k - 1 - i];
  return m;
}
static int emit(float out[][3], int n, int cap, float x, float y, float z) {
  if (n >= cap) return n;
  if (n > 0) { float dx = out[n - 1][0] - x, dz = out[n - 1][2] - z; if (dx * dx + dz * dz < 4.f) return n; }   // гуще 2 м — не нужно
  out[n][0] = x; out[n][1] = y; out[n][2] = z; return n + 1;
}
// сверка P04 A1: середина дороги — среднее первой и последней полосы в той же точке (у однополосной — сама ось)
static void midPt(const Traffic *t, int r, int kk, float p[3]) {
  const TrRoad *R = &t->road[r]; const TrPt *a = lanePts(t, r, 0), *b = lanePts(t, r, R->lanes > 0 ? R->lanes - 1 : 0);
  float dx = a[kk].x - b[kk].x, dz = a[kk].z - b[kk].z;
  if (dx * dx + dz * dz > 9.f * 9.f) { p[0] = a[kk].x; p[1] = a[kk].y; p[2] = a[kk].z; return; }   // крайние полосы дальше 9 м (широкая дорога с разделителем: середина — клумба или барьер, цель sr3 schoolbu вставала в нём навсегда) — ось первой
  p[0] = (a[kk].x + b[kk].x) * .5f; p[1] = (a[kk].y + b[kk].y) * .5f; p[2] = (a[kk].z + b[kk].z) * .5f;
}
static void midAt(const Traffic *t, int r, float s, float p[3]) {
  const TrRoad *R = &t->road[r]; float a[3], b[3]; lanePos(t, r, 0, s, a); lanePos(t, r, R->lanes > 0 ? R->lanes - 1 : 0, s, b);
  float dx = a[0] - b[0], dz = a[2] - b[2];
  if (dx * dx + dz * dz > 9.f * 9.f) { memcpy(p, a, sizeof a); return; }
  for (int i = 0; i < 3; i++) p[i] = (a[i] + b[i]) * .5f;
}
// круг 7: срезы на перекрёстках пути (см. выше). path — состояния 2 * дорога + направление; -> новая длина
// №1147 (сверка g03-aicars, вождение бонус-трассы): срез не должен перескакивать дорогу цели (последнюю в пути): у b07 WPR04 лежит на R3, срез R1 кончается на том же перекрёстке,
// что R3, — машина ехала срезом мимо цели, точка не засчитывалась, и она кружила круг за кругом. Срез заменяет только промежуточные дороги пути. MC_SHORTCUT_GOAL_OLD = 1 — прежнее
#ifndef MC_SHORTCUT_GOAL_OLD
#define MC_SHORTCUT_GOAL_OLD 0
#endif
static int applyShortcuts(const Traffic *t, int *path, int np) {
  int nR = (int)t->hd->nRoad;
  for (int i = 0; i + 1 < np; i++) {
    int ri = path[i] >> 1, di = path[i] & 1; unsigned jct = di ? t->road[ri].startJ : t->road[ri].endJ;
    int roll = svSkLo + (svSkHi > svSkLo ? (int)(svRandU() % (unsigned)(svSkHi - svSkLo)) : 0) + svSkMod, bestJ = -1, bestR = -1;
    for (int r = 0; r < nR; r++) {
      const TrRoad *S = &t->road[r]; if (!(S->flags & 1) || S->startJ != jct) continue;
      if (S->pad8 && (int)S->pad8 > roll) continue;   // pad8 = 0 — сложности в данных нет
      for (int j = i + 1; j < np - (MC_SHORTCUT_GOAL_OLD ? 0 : 1); j++) { int rj = path[j] >> 1, dj = path[j] & 1; unsigned ej = dj ? t->road[rj].startJ : t->road[rj].endJ;
        if (ej == S->endJ) { if (j > bestJ) { bestJ = j; bestR = r; } break; } }
    }
    if (bestR >= 0) { path[i + 1] = 2 * bestR; int cut = bestJ - (i + 1); for (int k = i + 2; k + cut < np; k++) path[k] = path[k + cut]; np -= cut; }
  }
  return np;
}
int mcDbgRoad(int r, int *checked, int *bad, int *ledge) { if (r < 0 || r >= 256) return 0; *checked = roadChecked[r]; *bad = roadBad[r]; *ledge = roadLedge[r]; return 1; }   // служебное (полигон бонус-игры)
void mcPlanAvoid(const short *r, int n) { avoidR = r; avoidN = n; }   // служебное (гейт tests/aicar/roadplan.c): список объездов для mcPlan
int mcPlan(const Traffic *t, float x, float y, float z, float h, const float g[3], int shortcuts, float out[][3], int cap, int *offroad) {
  int dummy; if (!offroad) offroad = &dummy;
  *offroad = 0;
  if (cap < 2) return 0;
  if (!roadsOk(t)) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int rg, lg, r0, l0; float sg, s0, dg;
  // начало и конец — с любой дороги (машина стоит на стоянке-срезе у дома — выезжает по нему); запрет срезов — в пути
  laneSoftY = !MC_SPRINT_GOALY_OLD && mcSprint;
  int goalOn = nearestLane(t, g[0], g[1], g[2], 0.f, -1.f, LANE_SNAP * LANE_SNAP, 1, &rg, &lg, &sg, &dg);
  laneSoftY = 0;
  if (!goalOn) {
    *offroad = 1; out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  if (d2xz(g, x, z) < CLOSE_DIRECT * CLOSE_DIRECT) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  // начало — ближняя полоса обычной дороги (не среза: срезы уровня 1 идут дворами, 5 из них перекрыты коллизией), потом любая
  if (!nearestLane(t, x, y, z, h, -1.f, 12.f * 12.f, 0, &r0, &l0, &s0, 0) &&
      !nearestLane(t, x, y, z, h, -1.f, 12.f * 12.f, shortcuts, &r0, &l0, &s0, 0) &&
      !nearestLane(t, x, y, z, h, -1.f, LANE_SNAP * LANE_SNAP, 1, &r0, &l0, &s0, 0)) {
    out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int path[64], scp = svSkOn ? 0 : shortcuts, np = planRoads(t, r0, l0, s0, h, rg, sg, scp, path, 64);
  for (int it = 0; it < 3 && np && probePath(t, path, np); it++) np = planRoads(t, r0, l0, s0, h, rg, sg, scp, path, 64);   // перекрытая — в объезд
  if (svSkOn && np > 1) np = applyShortcuts(t, path, np);
  if (!np) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int n = 0, lane = l0; float last[3] = {x, y, z};
#if !MC_PATHMID_OLD
  float prevLast[3] = {x, y, z}; int havePrev = 0;
#endif
  { float p[3]; lanePos(t, r0, l0, s0, p); n = emit(out, n, cap, p[0], p[1], p[2]); memcpy(last, p, sizeof last); }   // с проекции машины на полосу
  for (int i = 0; i < np && n < cap; i++) {
    int r = path[i] >> 1, d = path[i] & 1; const TrRoad *R = &t->road[r];
#if !MC_PATHMID_OLD
    {   // сверка P04 A1: по середине дороги; на перекрёстке — прямая хорда: от конца прежнего пути на 40 % расстояния вперёд по его направлению,
        // затем к началу выезда (если до него больше 1 м; не больше 0,001 м — ни одной вставки). Было: ближняя к въезду полоса, без хорды
      if (i > 0) {
        float e0[3]; midPt(t, r, d ? R->nPt - 1 : 0, e0);
        float ex = e0[0] - last[0], ez = e0[2] - last[2], dist = sqrtf(ex * ex + ez * ez);
        float dx = last[0] - prevLast[0], dz = last[2] - prevLast[2], dl = sqrtf(dx * dx + dz * dz);
        // круг 7, сверено с оригиналом: он строит хорду на любом перекрёстке (при расстоянии до выезда больше 1 м), без проверки угла. ОТЛИЧИЕ ПОРТА (число наше, не из оригинала):
        // на повороте круче ~30° (cos < 0,85) хорда вела точку руля в угол здания, и цель sr3 schoolbu вставала у стены навсегда (стенд tests/aicar/host.sh),
        // поэтому хорда только на проездах прямо. -DMC_CHORD_ALWAYS=1 — как в оригинале
        if (!MC_CHORD_OLD && dist > 1.f && havePrev && dl > 1e-3f && (MC_CHORD_ALWAYS || (dx * ex + dz * ez) > .85f * dl * dist)) {
          float q[3] = {last[0] + dx / dl * dist * .4f, last[1] + (e0[1] - last[1]) * .4f, last[2] + dz / dl * dist * .4f};
          n = emit(out, n, cap, q[0], q[1], q[2]); memcpy(prevLast, last, sizeof last); memcpy(last, q, sizeof last);
        }
      }
      const TrPt *P = lanePts(t, r, 0);
      int last_ = i == np - 1 && r == rg;
      float from = i == 0 ? s0 : (d ? 1e30f : -1.f), to = last_ ? sg : (d ? -1.f : 1e30f);
      for (int k = 0; k < R->nPt && n < cap; k++) {
        int kk = d ? R->nPt - 1 - k : k; float s = P[kk].s, p[3];
        if (d ? (s >= from || s <= to) : (s <= from || s >= to)) continue;
        midPt(t, r, kk, p); int n0 = n; n = emit(out, n, cap, p[0], p[1], p[2]);
        if (n != n0) { memcpy(prevLast, last, sizeof last); havePrev = 1; memcpy(last, p, sizeof last); }
      }
      if (last_ && n < cap) { float p[3]; midAt(t, r, sg, p); n = emit(out, n, cap, p[0], p[1], p[2]); }
      continue;
    }
#endif
    if (i > 0) {   // полоса следующей дороги — ближняя к месту въезда
      float best = 1e30f; lane = 0;
      for (int l = 0; l < R->lanes; l++) { const TrPt *P = lanePts(t, r, l); const TrPt *e = d ? &P[R->nPt - 1] : &P[0];
        float dd = d2xz(last, e->x, e->z); if (dd < best) { best = dd; lane = l; } }
    }
    const TrPt *P = lanePts(t, r, lane);
    int last_ = i == np - 1 && r == rg;
    float from = i == 0 ? s0 : (d ? 1e30f : -1.f), to = last_ ? sg : (d ? -1.f : 1e30f);
    for (int k = 0; k < R->nPt && n < cap; k++) {
      int kk = d ? R->nPt - 1 - k : k; float s = P[kk].s;
      if (d ? (s >= from || s <= to) : (s <= from || s >= to)) continue;
      n = emit(out, n, cap, P[kk].x, P[kk].y, P[kk].z); last[0] = P[kk].x; last[1] = P[kk].y; last[2] = P[kk].z;
    }
    if (last_ && n < cap) { float p[3]; lanePos(t, r, lane, sg, p); n = emit(out, n, cap, p[0], p[1], p[2]); }
  }
  if (n < cap) n = emit(out, n, cap, g[0], g[1], g[2]);
  if (!n) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; n = 1; }
  return n;
}

// ================================================================ машины
static const McGeo *geoFor(const char *name, const Traffic *tr, McGeo *tmp) {
  for (int i = 0; i < geoN; i++) if (ieq(geoTab[i].name, name)) return &geoTab[i];
  if (tr && tr->hd) for (uint32_t i = 0; i < tr->hd->nModel; i++) {   // модель трафика того же имени (minivanA, schoolbu …)
    const TrModel *M = &tr->model[i]; char nm[17]; memcpy(nm, M->name, 16); nm[16] = 0;
    if (!ieq(nm, name)) continue;
    memset(tmp, 0, sizeof *tmp); memcpy(tmp->name, nm, 15);
    tmp->lift = M->lift; tmp->wheelRad = M->wheelRad; tmp->halfW = M->halfW; tmp->halfL = M->halfL; tmp->bodyOfs = M->bodyOfs;
    tmp->wheelX = M->wheelX; tmp->wheelF = M->wheelF; tmp->wheelR = M->wheelR; tmp->bodyLo = M->bodyLo; tmp->bodyHi = M->bodyHi;
    return tmp;
  }
  return 0;
}
void mcInit(MsnCars *m) { memset(m, 0, sizeof *m); m->laps = 1; }
McCar *mcFind(MsnCars *m, const char *name) {
  for (int i = 0; i < m->n; i++) if (m->c[i].on && ieq(m->c[i].name, name)) return &m->c[i];
  return 0;
}
static void resetPath(McCar *c) {
  // сверка g03-aicars: состояние водителя «как в оригинале» — заново (ожидание игрока waitT и счёт событий waitEvt не сбрасываются: их читает миссия)
  c->turning = 0; c->stkOn = 0; c->stkT = 0.f; c->stopHold = 0; c->direct = 0; c->ramT = 0.f; c->slipT = 0.f; c->flipHoldT = 0.f; c->offT = 0.f;
  c->chaseRoad = -2; c->chaseRoadT = 0.f; c->losT = 0.f; c->losOk = 0; c->backLen = 0.f; c->stkThr = 0.f;
#if !MC_SPEED_OLD
  c->vdes = .7f * c->baseTop; c->catchT = 2.f;   // круг 3: первые 2 с — 0,7 своей TopSpeed
#else
  c->vdes = 0.f; c->catchT = 0.f;
#endif
  c->wp = 0; c->passed = -1; c->finished = 0; c->nroute = 0; c->seg = 0; c->routeWp = -1; c->replanT = 0.f; c->stuckN = 0; c->progWp = -2; c->progT = 0.f;
  c->rs[0] = c->car.x; c->rs[1] = c->car.y; c->rs[2] = c->car.z; c->rk = 0; c->rAhead = -1; c->rDist = 0.f; }
McCar *mcSpawn(MsnCars *m, const ColWorld *w, const Traffic *tr, const char *name, const float pos[3], float h, int hasH,
               const char *ai, const char *con, const char *driver) {
  McCar *c = mcFind(m, name);
  if (!c) {
    for (int i = 0; i < m->n; i++) if (!m->c[i].on) { c = &m->c[i]; break; }
    if (!c) { if (m->n >= MC_MAX) return 0; c = &m->c[m->n++]; }
  }
  memset(c, 0, sizeof *c);
  strncpy(c->name, name ? name : "", sizeof c->name - 1); strncpy(c->con, con ? con : "", sizeof c->con - 1);
  strncpy(c->driver, driver ? driver : "", sizeof c->driver - 1);
  CarParams P; c->conOk = carParamsForAi(con, name, &P);
  McGeo tmp; const McGeo *G = geoFor(name, tr, &tmp);
  c->lift = .45f;
  if (G) {   // кузов и колёса — модель машины (как у трафика: TRF1)
    c->geoOk = 1; c->lift = G->lift;
    P.halfW = G->halfW; P.halfL = G->halfL; P.bodyOfs = G->bodyOfs; P.wheelX = G->wheelX; P.wheelF = G->wheelF; P.wheelR = G->wheelR;
    P.wheelRad = G->wheelRad; P.bodyLo = G->bodyLo; P.bodyHi = G->bodyHi;
    P.bandLo = fminf(.35f, G->bodyLo + .1f); P.bandHi = fmaxf(P.bandLo + .5f, G->bodyHi - .4f);
  }
  c->baseTop = P.topSpeed; c->baseGas = P.gasScale; c->mult = 1.f; c->baseGrip = P.tireGrip;
  carInit(&c->car, &P, w, pos[0], pos[1] + .5f, pos[2], hasH ? h : 0.f);
  // Урон машине миссии от машин — полный, как до правки 176 (её × CAR_HIT_DMG_K — про трафик: оракул В14, минивэн трафика цел после
  // 14 таранов). Ночь ботов 02.10→03.10: с × 0,45 цель тарана m3 (smith_v, M3dest.con SetHitPoints 1.5) не разбивалась вовремя;
  // сколько ударов у оригинала — близнец В26. -DMC_HIT_DMG_TRAFFIC=1 — прежнее (× 0,45), красный гейта tests/aicar/m3dmg.sh
#if !defined(MC_HIT_DMG_TRAFFIC) || !MC_HIT_DMG_TRAFFIC
  c->car.hitDmgK = 1.f;
#endif
  c->mode = mcMode(ai); c->on = 1; c->asleep = c->mode == MC_PARKED;
  resetPath(c);
  return c;
}
void mcActivate(MsnCars *m, const ColWorld *w, const char *name, const float *pos, float h, int hasH, const char *ai) {
  McCar *c = mcFind(m, name); if (!c) return;
  if (pos) { carPlace(&c->car, w, pos[0], pos[1] + .5f, pos[2], hasH ? h : c->car.h); }
  c->mode = mcMode(ai); c->asleep = c->mode == MC_PARKED; resetPath(c);
}
void mcWaypoint(MsnCars *m, int index, const float p[3], int laps) {
  if (index == 0) {   // новый путь этапа: машины с ИИ начинают его сначала
    m->nwp = 0; m->laps = laps > 0 ? laps : 1;
    for (int i = 0; i < m->n; i++) if (m->c[i].on && m->c[i].mode != MC_PARKED) resetPath(&m->c[i]);
  }
  if (m->nwp < MC_WP) { memcpy(m->wp[m->nwp], p, sizeof m->wp[0]); m->nwp++; }
}
void mcParams(MsnCars *m, const char *name, int lo, int hi) { McCar *c = mcFind(m, name); if (c) { c->skLo = lo; c->skHi = hi; c->hasSk = 1; } }
void mcCatchup(MsnCars *m, const char *name, int kind, const float *p, int n) {
  McCar *c = mcFind(m, name); if (!c) return;
  if (kind == 0 && n >= 4) { memcpy(c->rc, p, sizeof c->rc); c->hasRc = 1; }
  if (kind == 1 && n >= 2) { c->tc[0] = p[0]; c->tc[1] = p[1]; c->hasTc = 1; }
}
void mcRemove(MsnCars *m, const char *name) { McCar *c = mcFind(m, name); if (c) memset(c, 0, sizeof *c); }
void mcClear(MsnCars *m) { m->n = 0; m->nwp = 0; m->laps = 1; memset(m->c, 0, sizeof m->c); }

// ---------------------------------------------------------------- гонка: остаток пути по точкам этапа
static int total(const MsnCars *m) { return m->nwp * (m->laps > 0 ? m->laps : 1); }
static const float *wpAt(const MsnCars *m, int k) { return m->wp[k % m->nwp]; }
static float legs(const MsnCars *m, int k) {   // от точки k до последней (с кругами)
  float s = 0.f; int T = total(m);
  for (int j = k; j + 1 < T; j++) { const float *a = wpAt(m, j), *b = wpAt(m, j + 1); s += sqrtf(d2xz(a, b[0], b[2])); }
  return s;
}
float mcRemaining(const MsnCars *m, const McCar *c) {
  if (!m->nwp || c->finished) return 0.f;
  int k = c->wp < total(m) ? c->wp : total(m) - 1;
  return sqrtf(d2xz(wpAt(m, k), c->car.x, c->car.z)) + legs(m, k);
}
float mcRemainingFrom(const MsnCars *m, const float p[3]) {
  if (!m->nwp) return 0.f;
  float best = 1e30f; int T = total(m);
  for (int k = 0; k < T; k++) { float r = sqrtf(d2xz(wpAt(m, k), p[0], p[2])) + legs(m, k); if (r < best) best = r; }
  return best;
}
float mcRaceFrac(const MsnCars *m, const McCar *c) {   // пройденная доля пути: точек позади + доля текущего отрезка
  int T = total(m); if (!T) return 0.f;
  if (c->finished || c->wp >= T) return 1.f;
  const float *b = wpAt(m, c->wp); float d = sqrtf(d2xz(b, c->car.x, c->car.z)), leg = d;
  if (c->wp > 0) { const float *a = wpAt(m, c->wp - 1); leg = sqrtf(d2xz(a, b[0], b[2])); }
  return ((float)c->wp + (leg > 1.f ? 1.f - clampf(d / leg, 0.f, 1.f) : 0.f)) / (float)T;
}
// правка 300 (Роб 04.10: «гонка не считает, кто первый: обгоняю Скиннера срезками, а пишет второй»; было 178). Прежнее место —
// сравнение долей по ЧИСЛУ точек: у Скиннера в S-M-R-T три точки (m1_AI_path1 103 м, m1_AI_path1b 131 м, m1_AI_path4 381 м от
// места старта), каждая — треть доли; у игрока одна (финиш), доля — по прямой от старта (512 м). За первые 234 м своего пути
// Скиннер получал 2/3, и игрок в 196 м от финиша (доля 0.62) был «2-м» при Скиннере в 367 м от финиша (доля 0.68) — замер
// tests/aicar/racepos.sh. Теперь — остаток пути до финиша в метрах, у каждого по своим точкам: срезка и обгон на дороге видны
// сразу; на общем последнем отрезке (m1, sr2, sr3) рядом — поровну. ПРАВИЛО ПОРТА: как меряет оригинал — не измерено (В9, В42)
int mcRacePosOld;
float mcPathRemaining(const float (*pts)[3], int n, int laps, int next, const float p[3]) {
  int T = n * (laps > 0 ? laps : 1); if (n <= 0 || next >= T) return 0.f;
  if (next < 0) next = 0;
  float s = sqrtf(d2xz(pts[next % n], p[0], p[2]));
  for (int k = next; k + 1 < T; k++) { const float *a = pts[k % n], *b = pts[(k + 1) % n]; s += sqrtf(d2xz(a, b[0], b[2])); }
  return s;
}
static int racer(const McCar *c);
int mcRacePlace(const MsnCars *m, float prem, int *racers) {
  int ahead = 0, k = 0;
  for (int i = 0; i < m->n; i++) { const McCar *c = &m->c[i]; if (!racer(c) || !m->nwp) continue; k++; if (mcRemaining(m, c) < prem) ahead++; }
  if (racers) *racers = k;
  return k ? ahead + 1 : 0;
}
float mcPathFracCount(const float (*pts)[3], int n, int laps, int next, float sx, float sz, const float p[3]) {
  int T = n * (laps > 0 ? laps : 1); if (n <= 0 || next >= T) return 1.f;
  if (next < 0) next = 0;
  const float *b = pts[next % n]; float ax = sx, az = sz;
  if (next > 0) { ax = pts[(next - 1) % n][0]; az = pts[(next - 1) % n][2]; }
  float d = sqrtf(d2xz(b, p[0], p[2])), leg = sqrtf(d2xz(b, ax, az));
  return ((float)next + (leg > 1.f ? 1.f - fminf(1.f, d / leg) : 0.f)) / (float)T;
}
int mcRacePlaceOld(const MsnCars *m, float fp, int *racers) {
  int ahead = 0, k = 0;
  for (int i = 0; i < m->n; i++) { const McCar *c = &m->c[i]; if (!racer(c)) continue; k++; if (mcRaceFrac(m, c) > fp) ahead++; }
  if (racers) *racers = k;
  return k ? ahead + 1 : 0;
}
// путь соперника для догонялки (03.10): от места, где он начал путь этапа (rs), через его точки (с кругами); *tot — длина пути.
// -> пройдено им по этому пути, м
static float raceDoneLen(const MsnCars *m, const McCar *c, float *tot) {
  int T = total(m); float L0 = T ? sqrtf(d2xz(wpAt(m, 0), c->rs[0], c->rs[2])) : 0.f; *tot = L0 + (T ? legs(m, 0) : 0.f);
  if (!T || *tot < 1.f) return 0.f;
  if (c->finished || c->wp >= T) return *tot;
  const float *b = wpAt(m, c->wp); float d = sqrtf(d2xz(b, c->car.x, c->car.z));
  if (c->wp == 0) return L0 > 1.f ? L0 * (1.f - clampf(d / L0, 0.f, 1.f)) : 0.f;
  const float *a = wpAt(m, c->wp - 1); float leg = sqrtf(d2xz(a, b[0], b[2]));
  return L0 + (legs(m, 0) - legs(m, c->wp - 1)) + (leg > 1.f ? leg * (1.f - clampf(d / leg, 0.f, 1.f)) : 0.f);
}
// где игрок на том же пути (03.10): проекция на отрезки пути соперника; из нескольких близких мест (трасса с возвратом: старт и
// финиш рядом) — то, чья доля пути ближе к доле пройденной гонки игрока по ЕГО точкам (pfrac, game_msn.c gameMsnRaceFracLen).
// -> пройдено игроком по пути соперника, м
static float playerDoneLen(const MsnCars *m, const McCar *c, const float p[3], float pfrac, float tot) {
  int T = total(m); float ax = c->rs[0], az = c->rs[2], s0 = 0.f, best = 1e30f, sb = 0.f;
  for (int k = 0; k < T; k++) {
    const float *b = wpAt(m, k); float dx = b[0] - ax, dz = b[2] - az, L = sqrtf(dx * dx + dz * dz);
    float t = L > 1e-3f ? clampf(((p[0] - ax) * dx + (p[2] - az) * dz) / (L * L), 0.f, 1.f) : 0.f;
    float qx = ax + dx * t, qz = az + dz * t, sk = s0 + L * t;
    float cost = sqrtf((p[0] - qx) * (p[0] - qx) + (p[2] - qz) * (p[2] - qz)) + fabsf(sk / tot - pfrac) * tot;
    if (cost < best) { best = cost; sb = sk; }
    s0 += L; ax = b[0]; az = b[2];
  }
  return sb;
}
// правка 326: место по правилу Роба «кто ближе к точке — тот и первый» (msncar.h). Точки — гонки игрока; у соперника свой счёт
// пройденных c->rk. Прежние меры (правка 300 — остаток метров по своим точкам; 178 — доли по числу точек) — за флагами
int mcRacePos300;
int mcRaceAhead(int rk, float dr, int pk, float dp, int prev) {
  if (rk != pk) return rk > pk;
  float h = prev == 1 ? MC_RACE_HYST * .5f : prev == 0 ? -MC_RACE_HYST * .5f : 0.f;   // впереди — держится, пока не отстал на 0,5 м
  return dr < dp + h;
}
static int inWp(const float *q, const float p[3]) { return d2xz(q, p[0], p[2]) < WP_R * WP_R; }
// сверка P04 Д4 / 1.2: догонялка гонки сравнивает соперника с игроком сначала по кругам и контрольной точке (c->rk против pk — счёт с кругами),
// при равенстве — по разнице расстояний до той же точки (c->rDist против dp); данные последнего расчёта места игрока (mcRacePlaceRob)
static int svPk = -1; static float svDp;
int mcRacePlaceRob(MsnCars *m, const float (*pts)[3], int n, int laps, int pk, const float p[3], McInPt in, void *u, float *dpo,
                   int *racers) {
  int T = n * (laps > 0 ? laps : 1), ahead = 0, k = 0;
  if (dpo) *dpo = 0.f;
  if (racers) *racers = 0;
  if (n <= 0) return 0;
  if (pk < 0) pk = 0;
  float dp = pk < T ? sqrtf(d2xz(pts[pk % n], p[0], p[2])) : 0.f;
  for (int i = 0; i < m->n; i++) {
    McCar *c = &m->c[i]; if (!racer(c)) continue;
    k++; const float q[3] = {c->car.x, c->car.y, c->car.z};
    for (;;) {   // точки гонки игрока, через которые соперник проехал (как игрок — внутри точки); одну обойдённую — со следующей
      if (c->finished) { c->rk = T; break; }
      if (c->rk >= T - 1) break;   // финиш (последняя точка) — только когда доехал: кто выиграл, решает миссия (условие race)
      if (in ? in(u, c->rk, q) : inWp(pts[c->rk % n], q)) { c->rk++; continue; }
      if (c->rk + 1 < T - 1 && (in ? in(u, c->rk + 1, q) : inWp(pts[(c->rk + 1) % n], q))) { c->rk += 2; continue; }
      break;
    }
    c->rDist = c->rk < T ? sqrtf(d2xz(pts[c->rk % n], q[0], q[2])) : 0.f;
    c->rAhead = mcRaceAhead(c->rk, c->rDist, pk, dp, c->rAhead);
    ahead += c->rAhead;
  }
  if (dpo) *dpo = dp;
  svPk = pk; svDp = dp;
  if (racers) *racers = k;
  return k ? ahead + 1 : 0;
}
static int racer(const McCar *c) { return c->on && (c->mode == MC_RACE || c->mode == MC_EVADE || c->mode == MC_TARGET || c->mode == MC_WAYPOINT); }
int mcRacePosition(const MsnCars *m, const float p[3], int *racers) {
  float me = mcRemainingFrom(m, p); int ahead = 0, n = 0;
  for (int i = 0; i < m->n; i++) { const McCar *c = &m->c[i]; if (!racer(c) || !m->nwp) continue; n++; if (mcRemaining(m, c) < me) ahead++; }
  if (racers) *racers = n;
  return ahead + 1;
}

// ---------------------------------------------------------------- водитель
// точка на маршруте на L м впереди проекции машины; seg — отрезок проекции (только вперёд)
static void aimPoint(McCar *c, float L, float out[3]) {
  float x = c->car.x, z = c->car.z;
  if (c->nroute <= 1) { memcpy(out, c->route[0], 12); return; }
  int best = c->seg; float bd = 1e30f, bu = 0.f;
  for (int k = c->seg; k < c->nroute - 1 && k < c->seg + 8; k++) {
    const float *a = c->route[k], *b = c->route[k + 1]; float ax = b[0] - a[0], az = b[2] - a[2], l2 = ax * ax + az * az;
    float u = l2 > 1e-6f ? clampf(((x - a[0]) * ax + (z - a[2]) * az) / l2, 0.f, 1.f) : 0.f;
    float qx = a[0] + ax * u - x, qz = a[2] + az * u - z, d = qx * qx + qz * qz;
    if (d < bd) { bd = d; best = k; bu = u; }
  }
  c->seg = best; c->offLane = sqrtf(bd);
  // от проекции вперёд на L
  int k = best; float u = bu;
  while (k < c->nroute - 1) {
    const float *a = c->route[k], *b = c->route[k + 1]; float sl = sqrtf(d2xz(a, b[0], b[2])), rest = sl * (1.f - u);
    if (L <= rest && sl > 1e-4f) { float t = u + L / sl; for (int j = 0; j < 3; j++) out[j] = a[j] + (b[j] - a[j]) * t; return; }
    L -= rest; k++; u = 0.f;
  }
  memcpy(out, c->route[c->nroute - 1], 12);
}
// допустимая скорость по кривизне маршрута впереди (на тормозной путь с текущей скорости + 30 м) и торможению заранее
static float curveSpeed(const McCar *c, float vtop, float dec) {
  // d — путь от машины до вершины k: до первой (seg+1) — напрямую, дальше — по отрезкам маршрута
  float v = vtop, d = 0.f;
  float far = fmaxf(60.f, c->car.v * c->car.v / (2.f * dec) + 30.f);
  float alat = clampf(A_LAT * c->car.p.tireGrip / 2.4f, A_LAT, 11.f);   // шины .con: у ИИ-водителей TireGrip 30 — держат лучше
  for (int k = c->seg + 1; k < c->nroute - 1 && d < far; k++) {
    const float *a = c->route[k - 1], *b = c->route[k], *e = c->route[k + 1];
    float l1 = sqrtf(d2xz(a, b[0], b[2])), l2 = sqrtf(d2xz(b, e[0], e[2]));
    d += k == c->seg + 1 ? sqrtf(d2xz(b, c->car.x, c->car.z)) : l1;
    if (l1 < 1e-3f || l2 < 1e-3f) continue;
    float th = fabsf(wrapPi(atan2f(e[0] - b[0], e[2] - b[2]) - atan2f(b[0] - a[0], b[2] - a[2])));
    if (th < .08f) continue;
    float R = .5f * fminf(fminf(l1, l2), 14.f) / tanf(fminf(th, 3.f) * .5f), vc = sqrtf(alat * fmaxf(R, 2.f));
    v = fminf(v, sqrtf(vc * vc + 2.f * dec * fmaxf(0.f, d - 3.f)));
  }
  return v;
}
// машина o впереди в коридоре (кузов + OBS_MARGIN с боков, до 35 м): -> зазор по ходу, м (1e9 — не мешает); *l — где она поперёк
// (> 0 — справа), *need — насколько сдвинуться вбок, чтобы пройти с зазором OBS_MARGIN
#define OBS_MARGIN .5f
#define SHIFT_MAX 3.f      // объезд вбок от оси полосы до 3 м (правило порта: улица уровня 1 — две полосы по 3.5–4 м)
static float gapCar(const Car *k, const Car *o, float *l, float *need) {
  CarBox A, B; carBox(k, &A); carBox(o, &B);
  float dx = B.x - A.x, dz = B.z - A.z, a = dx * A.fx + dz * A.fz, ll = -dx * A.fz + dz * A.fx;
  if (fabsf(k->y - o->y) > 3.f) return 1e9f;
  float ua = B.fx * A.fx + B.fz * A.fz, ul = -B.fx * A.fz + B.fz * A.fx;
  float ea = B.hl * fabsf(ua) + B.hw * fabsf(ul), el = B.hl * fabsf(ul) + B.hw * fabsf(ua), nd = A.hw + OBS_MARGIN - (fabsf(ll) - el);
  if (a + ea <= 0.f || a - ea > 35.f || nd <= 0.f) return 1e9f;
  *l = ll; *need = nd;
  return fmaxf(0.f, a - ea - A.hl);
}
// машины впереди: ближняя в коридоре -> *shift — объезд вбок (м, > 0 — влево), если хватает SHIFT_MAX; *gap/*ov — зазор и скорость
// той, перед которой тормозить (трафик, стоящие), *free_ — она стоит и её можно объехать (тормозить меньше; едущую — догнать
// до её скорости, объезжая, если есть место). Игрок и машины миссии не
// тормозят (правило порта: соперники в гонке толкаются, как в оригинале) — их только объезжают
static int dbgKind;   // служебное: чья это машина впереди (1+режим трафика, 9 стоящая)
static int obsPass;   // правка 347: та, перед которой тормозить, едет (не стоит) и сбоку хватает места объехать (SHIFT_MAX)
// правка 347: бот-игрок (careful в гонке и слежке, таран с целью c->ignore) обгоняет едущую машину — соперника и трафик, — если
// сбоку есть место: к ней не медленнее её скорости + 3 м/с, объезжая вбок (руль — как объезд, ниже). Было: careful ехал за Скиннером
// в 0–3 м всю гонку S-M-R-T с его скоростью (k8), а в m6 бот-таран 10–20 с ехал за машиной трафика, пока фургон уходил на 100 м;
// на записи PS2 (S-M-R-T 5:00.8–5:02.8) игрок идёт со Скиннером борт в борт и обходит его, в m6 бьёт фургон через 6 с после GO.
// Стоящую машину трафика со свободным боком careful объезжает сразу на 5 м/с (было: 1.5 м/с, упор 1.5 с, назад 1.2 с, и только потом
// объезд на 5 м/с — S-M-R-T: 10–25 с гонки у машин, ждущих на перекрёстках). Без простора сбоку — как было. Машины миссий игры
// не затронуты. mcBotPassOld = 1 — прежнее (MISSIONER_PASS_OLD=1)
int mcBotPassOld;
// правка 347: бот-игрок в таране (MC_CHASE, c->ignore — имя цели; машины миссий игры ignore не ставят) — что в коридоре ДАЛЬШЕ
// цели, не помеха: между ними цель, её и надо ударить. Было (m6, ночь 05.10): бот в 2–6 м за фургоном cBone тормозил под машину
// трафика перед фургоном и ехал за ним 10–20 с, не ударив ни разу. mcBotRamOld = 1 — прежнее (MISSIONER_RAM_OLD=1)
int mcBotRamOld;
static float ramTargetGap(const MsnCars *m, const McCar *c) {
  if (mcBotRamOld || c->mode != MC_CHASE || !c->ignore[0]) return 1e9f;
  for (int i = 0; i < m->n; i++) if (&m->c[i] != c && m->c[i].on && ieq(m->c[i].name, c->ignore)) { float l, nd; return gapCar(&c->car, &m->c[i].car, &l, &nd); }
  return 1e9f;
}
static void obstacles(const MsnCars *m, const McCar *c, const McEnv *e, float *gap, float *ov, float *near, float *shift, int *free_) {
  dbgKind = 0; *free_ = 0; obsPass = 0;
  *gap = *near = 1e9f; *shift = 0.f; *ov = 0.f; float g, l, nd, fx = sinf(c->car.h), fz = cosf(c->car.h), tgap = ramTargetGap(m, c);
#define OBS2(o, hard, pass) do { g = gapCar(&c->car, (o), &l, &nd); if (g > tgap) g = 1e9f; if (g < *near) { *near = g; *shift = nd <= SHIFT_MAX ? (l >= 0.f ? nd : -nd) : 0.f; } \
                          if ((hard) && g < *gap) { *gap = g; *ov = (o)->vel[0] * fx + (o)->vel[2] * fz; dbgKind = (hard); *free_ = nd <= SHIFT_MAX && fabsf(*ov) < 1.f; \
                                                    obsPass = (pass) && nd <= SHIFT_MAX && *ov > 3.f; } } while (0)
#define OBS(o, hard) OBS2(o, hard, 1)
  if (e->player && c->mode != MC_CHASE) OBS(e->player, 0);
  // машину миссии бот обгоняет, только если она соперник гонки (за целью слежки — держаться следом, не обгонять)
  const int kMsn = c->careful ? 8 : 0, kPark = racer(c) && !c->careful ? 0 : 9;
  for (int i = 0; i < m->n; i++) if (&m->c[i] != c && m->c[i].on && !(c->mode == MC_CHASE && ieq(m->c[i].name, c->ignore))) OBS2(&m->c[i].car, kMsn, m->c[i].mode == MC_RACE);
  if (e->tr && e->tr->hd) {
    for (int i = 0; i < e->tr->maxCars; i++) if (e->tr->car[i].mode != TRM_OFF) {
      const Car *o = &e->tr->car[i].car; if ((o->x - c->car.x) * (o->x - c->car.x) + (o->z - c->car.z) * (o->z - c->car.z) < 40.f * 40.f) OBS(o, 1 + e->tr->car[i].mode); }
    for (int i = 0; i < e->tr->npark; i++) {
      const Car *o = &e->tr->park[i].car; if ((o->x - c->car.x) * (o->x - c->car.x) + (o->z - c->car.z) * (o->z - c->car.z) < 40.f * 40.f) OBS(o, kPark); }
      // ↑ правка 122 (гонка sr3, 30.09): соперник гонки у стоящей машины не встаёт намертво — только объезжает её (при развороте на
      // скорости honor_v стоял 40+ с за стоящей у обочины, автобус и игрок — за ним: пробка). Толкнуть стоящую может, как в оригинале
  }
#undef OBS
#undef OBS2
}
static int ciHas(const char *h, const char *n) {   // подстрока без учёта регистра (чистый C99: strcasestr нет)
  for (; *h; h++) { const char *x = h, *y = n; while (*x && *y && (*x | 32) == (*y | 32)) { x++; y++; } if (!*y) return 1; }
  return 0;
}
McPlanHook mcPlanHook; void *mcPlanUser;   // правка 420: свой планировщик маршрута (бонус-игра), NULL — как было
// Подшаги физики ИИ-машин реже вдали (ресёрч процессора 10.10, P4): шаг 120 Гц у машины, которую от игрока отделяют десятки метров и которая вне
// столкновений, не нужен — 120→60 Гц вдвое дешевле (расхождение траектории на дороге ≤ 0,1 м за 40 с), 40 Гц втрое (≤ 0,25 м). Ступени по расстоянию до
// игрока с запасом 4 м (не дрожат на границе): ближе 25 м — полный шаг; дальше — 60 Гц (40 Гц пробовали: расхождение до десятков метров). После любого контакта (стена, машина,
// кузов, удар) машина 1,5 с идёт полным шагом. Преследователь рядом всегда полный. -DAI_SUBSTEP_OLD=1 или mcAiSubstepOld = 1 — как было
#ifndef AI_SUBSTEP_OLD
#define AI_SUBSTEP_OLD 1   // ПО УМОЛЧАНИЮ ВЫКЛЮЧЕНО (10.10): безопасно (спокойная езда, 0,5 с) шаг реже даёт только 1–10 % физики преследователя, а расхождение с полным шагом до 1,7–2,6 м на L1/L4/L7 после контактов; решает Роб. Включить: -DAI_SUBSTEP_OLD=0
#endif
int mcAiSubstepOld = AI_SUBSTEP_OLD;
#ifndef AI_SUBSTEP_BREAK
#define AI_SUBSTEP_BREAK 0   // красный контроль гейта: 40 Гц дальше порога
#endif
float mcHitSpeed;
unsigned (*mcClockUs)(void); float mcPartMs[3]; int mcPartN;   // служебное: часы и подфазы mcUpdate (main_loop.h, только смоук): 0 физика carUpdate, 1 всё drive, 2 число машин
static void aiSubstep(McCar *c, const McEnv *e, float dt) {
  Car *k = &c->car;
  if (c->subBase <= 0.f) c->subBase = k->p.sub;
  if (mcAiSubstepOld) { k->p.sub = c->subBase; return; }
  if (k->hitSpeed > 0.f || k->wallV > 0.f || k->carHitV > 0.f || k->hardV > 0.f || k->bodyHit || k->contacts) c->subFullT = 1.5f;
  else if (c->subFullT > 0.f) c->subFullT -= dt;
  float px_ = e->player && e->driving ? e->player->x : e->ppos[0], pz_ = e->player && e->driving ? e->player->z : e->ppos[2];
  // спокойная езда: руль почти прямо, колёса не скользят, все на земле — только тогда (и 0,5 с подряд) шаг реже: на повороте, в заносе и у стены 60 Гц разошёлся от полного на десятки метров
  if (fabsf(c->lastIn.steer) < .25f && k->slip == 0 && k->grounded && k->wheels == 4) c->subCalmT += dt; else c->subCalmT = 0.f;
  float d = sqrtf((px_ - k->x) * (px_ - k->x) + (pz_ - k->z) * (pz_ - k->z)); int t = c->subTier; c->subD = d;
  if (t == 0 && d > 29.f) t = 1; else if (t >= 1 && d < 21.f) t = 0;
  if (t == 1 && d > 64.f) t = 2; else if (t == 2 && d < 56.f) t = 1;
  c->subTier = t;
#ifdef AI_SUB_DBG
  { static int cnt; if (cnt++ % 400 == 0) printf("aiSub: car %.1f,%.1f ppos %.1f,%.1f player %p driving %d player.x %.1f,%.1f d %.1f\n", k->x, k->z, e->ppos[0], e->ppos[2], (void *)e->player, e->driving, e->player ? e->player->x : -1.f, e->player ? e->player->z : -1.f, d); }
#endif
  float want = c->subFullT > 0.f || t == 0 || c->subCalmT < .5f ? c->subBase : (AI_SUBSTEP_BREAK ? 1.f / 40.f : 1.f / 60.f);   // ступень 2 (40 Гц) отброшена: на повороте и у стены разошлась на десятки метров (tests/aicar/substep.c)
  k->p.sub = want > c->subBase ? want : c->subBase;
}
static void replan(McCar *c, const MsnCars *m, const McEnv *e, const float g[3]) {
  if (mcPlanHook) {   // бонус-игра: маршрут по плотному пути трассы (viewer/bonus.c brPlan); 0 точек — прямо к цели, как без дорог
    int n = mcPlanHook(mcPlanUser, c, g, c->route, MC_ROUTE);
    if (n <= 0) { memcpy(c->route[0], g, 12); n = 1; }
    c->nroute = n; c->offroad = 0; c->seg = 0; c->replans++;
    float dx = c->route[n - 1][0] - g[0], dz = c->route[n - 1][2] - g[2];
    c->routeCut = dx * dx + dz * dz > 1.f; (void)m; (void)e;
    return;
  }
  // Правка Роба 457 (10.10, голосом, В89/В90/В107: «Скиннер в гонке L1 едет только по дороге, дворами не срезает»): ТОЛЬКО Скиннер L1 m1
  // (.con Missions\level01\M1race.con) — по дорожной сети без срезов-дворов (дороги диска с флагом shortcut); остальные гонщики миссий —
  // как было (срезы, если SetVehicleAIParams макс > 0). Цель-точка на самом срезе по-прежнему достижима (planRoads). MC_RIVAL_ROAD_OLD=1 — прежнее
  int sc = !c->hasSk || c->skHi > 0;
#ifndef MC_RIVAL_ROAD_OLD
#define MC_RIVAL_ROAD_OLD 0
#endif
  if (!MC_RIVAL_ROAD_OLD && c->mode == MC_RACE && ciHas(c->con, "level01") && ciHas(c->con, "m1race")) sc = 0;
  avoidR = c->avoid; avoidN = c->navoid; probeW = e->w;
#if !MC_PATHMID_OLD
  wrongWayK = (c->mode == MC_CHASE || svStreet) ? 1.f : WRONGWAY_BASE;
#endif
#if !MC_SHORTCUT_OLD
  svSkOn = sc && c->mode != MC_CHASE;   // у полиции срезов нет (правило порта, правка 197: срез дворами через твёрдую клумбу)
  if (svSkOn) {
    int lo = c->hasSk ? c->skLo : 15, hi = c->hasSk ? c->skHi : 25; if (hi < lo) { int t_ = hi; hi = lo; lo = t_; }
    svSkLo = lo; svSkHi = hi; svSkMod = (int)(c->catchF * 20.f);   // добавка догонялки: гонка −20…20, цель −20…0, уход 0…20
  }
#endif
  c->nroute = mcPlan(e->tr, c->car.x, c->car.y, c->car.z, c->car.h, g, sc, c->route, MC_ROUTE, &c->offroad);
  avoidN = 0; probeW = 0; svSkOn = 0; wrongWayK = WRONGWAY_BASE; c->seg = 0; c->replans++;
  float dx = c->route[c->nroute - 1][0] - g[0], dz = c->route[c->nroute - 1][2] - g[2];
  c->routeCut = dx * dx + dz * dz > 1.f;
  (void)m;
}
// дорога, на которую машина не может въехать (упирается второй раз): та, где следующая точка маршрута, — в объезд
static void avoidNext(McCar *c, const Traffic *t) {
  const float *q = c->route[c->seg + 1 < c->nroute ? c->seg + 1 : c->nroute - 1]; int r, l; float s_;
  if (!nearestLane(t, q[0], q[1], q[2], 0.f, -1.f, 4.f * 4.f, 1, &r, &l, &s_, 0)) return;
  for (int i = 0; i < c->navoid; i++) if (c->avoid[i] == r) return;
  if (c->navoid < MC_AVOID) c->avoid[c->navoid++] = (short)r;
  else { memmove(c->avoid, c->avoid + 1, sizeof c->avoid - sizeof c->avoid[0]); c->avoid[MC_AVOID - 1] = (short)r; }
  c->stuckN = 0;
}
// водитель кадра: руль на точку пути впереди (погоня рядом — на игрока), объезд машины впереди, скорость — TopSpeed
// (с множителем кадра), повороты и зазор; разворот в три приёма и выход из застревания. -> in (без carUpdate)
static void control(const MsnCars *m, McCar *c, const McEnv *e, const float goal[3], float dt, CarInput *in) {
  Car *k = &c->car;
  float v = k->v, aim[3], L = LOOK0 + LOOKV * fabsf(v);
  if (c->mode == MC_CHASE && d2xz(goal, k->x, k->z) < 30.f * 30.f) memcpy(aim, goal, sizeof aim);
  else aimPoint(c, L, aim);
  float gap, ov, near, shift; int free_; obstacles(m, c, e, &gap, &ov, &near, &shift, &free_); c->dbgGap = gap; c->dbgSwerve = near; c->dbgKind = dbgKind;
  {   // машина впереди: плавный объезд вбок на сколько нужно (до SHIFT_MAX), ближе 25 м; влево от курса = (cos h, -sin h)
    float want = near < 25.f && c->mode != MC_CHASE ? shift * 1.2f : 0.f;
    if (c->pushT > 0.f) { c->pushT -= dt; want = c->pushSide * SHIFT_MAX * 1.3f; }   // объезд стоящей после разворота: в сторону до упора
    c->shiftF += (want - c->shiftF) * fminf(1.f, dt * 3.f);
    aim[0] += cosf(k->h) * c->shiftF; aim[2] += -sinf(k->h) * c->shiftF;
  }
  float err = wrapPi(atan2f(aim[0] - k->x, aim[2] - k->z) - k->h);
  in->steer = clampf(-err * 2.2f, -1.f, 1.f);
  float dec = fminf(2.f * (k->p.brakeScale + 3.f), 9.f) * .7f;
  float vt = fminf(fminf(k->p.topSpeed, c->vcap > 0.f ? c->vcap : 1e9f), curveSpeed(c, k->p.topSpeed, dec));
  if (fabsf(err) > 1.2f) vt = fminf(vt, 6.f);                       // цель сбоку/сзади: медленно, разворот
  if (fabsf(err) > 2.2f && fabsf(v) < 3.f && c->backT <= 0.f && c->mode != MC_CHASE) {   // цель позади, места мало: назад
    c->backT = BACK_T; c->backSteer = err > 0.f ? 1.f : -1.f; c->turn3 = 1; }            // с обратным рулём (разворот в три приёма;
                                                                                         // путь прежний — он и задаёт разворот)
  // трафик / стоящая впереди: догнать до её скорости к зазору 3 м; стоящую — объехать (руль выше) и ползти 4 м/с, толкая,
  // а не ждать (правило порта: соперники оригинала распихивают трафик)
  // объехать можно — до неё ещё успеть сдвинуться: не тише 8 м/с и с запасом 2 м (правило порта)
  // после «упёрлась и сдала назад» (pushT) — мимо неё на 7 м/с, толкая (правило порта: иначе машина и трафик ждут друг друга)
  // бот-игрок careful (часть race 30.09): не толкать — к машине впереди не быстрее 1.5 м/с и в объезде после упора тоже (урон игроку)
  // Правка Роба 476 (10.10, голосом): машины-цели миссий, которые от игрока уезжают (фургон Bonestorm m6 и т.п., режимы target / evade), объезжают трафик —
  // не встают за ним и не толкают: при свободном боку идут мимо не тише 10 м/с и не быстрее её скорости + 3 м/с минимум (как обгон бота), без «толкания»
  // (+36 в зазоре). Без места сбоку — как было. MC_FLEE_PASS_OLD=1 — прежнее
#ifndef MC_FLEE_PASS_OLD
#define MC_FLEE_PASS_OLD 0
#endif
  const int flee = !MC_FLEE_PASS_OLD && (c->mode == MC_TARGET || c->mode == MC_EVADE);
  if (gap < 1e8f && (c->pushT <= 0.f || c->careful)) {
    int push = free_ && !c->careful && !flee;
    // careful после упора (pushT) мимо стоящей, которую можно объехать вбок (free_): 5 м/с, не 1.5 (иначе стоит у неё: gr1 у (−86, 506))
    float lo = c->careful ? (c->pushT > 0.f && free_ ? 5.f : 1.5f) : (free_ ? 8.f : 4.f);
    if ((c->careful || (c->mode == MC_CHASE && c->ignore[0])) && obsPass && !mcBotPassOld) lo = fmaxf(lo, ov + 3.f);   // правка 347: обгон
    if (c->careful && free_ && dbgKind != 8 && !mcBotPassOld) lo = fmaxf(lo, 5.f);   // правка 347: стоящую машину трафика — в объезд сразу на 5 м/с
    if (flee && free_) lo = fmaxf(lo, 10.f);                                          // правка 476: уезжающая цель объезжает на ходу
    if (flee && obsPass) lo = fmaxf(lo, ov + 3.f);
    vt = fminf(vt, fmaxf(lo, sqrtf(fmaxf(0.f, ov) * fmaxf(0.f, ov) + 2.f * dec * fmaxf(0.f, gap - (push ? 1.f : 3.f)) + (push ? 36.f : 0.f))));
  }
  c->vt = vt;
  if (c->backT > 0.f) {   // назад с обратным рулём
    c->backT -= dt; in->steer = c->backSteer; in->brake = 1.f; in->throttle = 0.f;
    if (c->careful && v < -3.f) in->brake = 0.f;   // бот-игрок: задним ходом не быстрее 3 м/с (gr1: удары задом на 5-10 м/с — урон 0.1-0.2 за раз)
  } else if (v < vt - .3f) {
    float u = vt / fmaxf(1.f, k->p.topSpeed); in->throttle = clampf(u * u + (vt - v) * .5f, .15f, 1.f);
  } else if (v > vt + 1.5f) in->brake = clampf((v - vt) * .3f, .2f, 1.f);
  else { float u = vt / fmaxf(1.f, k->p.topSpeed); in->throttle = clampf(u * u, 0.f, 1.f); }
  // застряла: газ есть, скорость нет — назад с обратным рулём, потом новый путь от этого места; стоит за машиной впереди
  // (зазор держит скорость у нуля) дольше 3 с — тоже: сдать назад и объехать (стоящая на полосе не уедет сама)
  int blocked = gap < 4.f && fabsf(v) < STUCK_V && c->backT <= 0.f;   // упёрлась в стоящую — не толкать, через 1.5 с назад и в объезд
  if (c->backT <= 0.f && (in->throttle > .3f || blocked) && fabsf(v) < STUCK_V) {
    if ((c->stuckT += dt) > STUCK_T) { c->stuckT = 0.f; c->backT = BACK_T; c->backSteer = err > 0.f ? 1.f : -1.f; c->stuckN++; c->turn3 = 0;
      if (blocked) { c->pushT = BACK_T + 3.f; c->pushSide = shift != 0.f ? (shift > 0.f ? 1.f : -1.f) : (c->stuckN & 1 ? 1.f : -1.f); }
      if (c->stuckN >= 2 && roadsOk(e->tr) && c->nroute) avoidNext(c, e->tr); }   // второй раз о то же — дорогу впереди в объезд
  } else { c->stuckT = 0.f; if (c->backT > 0.f && c->backT <= dt) { if (!c->turn3) c->nroute = 0; c->turn3 = 0; } }
}
// ================================================================ сверка g03-aicars (02.11.2026): водитель «как в оригинале»
// Всё ниже — свой код по поведению и числам из отчётов сверки (P04 и СВЕРКА_С_ИСХОДНИКОМ раздел 1), не по устройству кода оригинала.
// Старый водитель control() остаётся: боты (mcBotInput) и сборка с -DMC_DRIVE_OLD=1 едут по нему.
#define SV_KMH5 1.3889f   // «+5 км/ч» к желаемой скорости, м/с (P04 A2, 1.4)

// P04 A3, Д5: объезд. Ближайшая машина в радиусе 50 м (любая: трафик, стоящие, машины миссий, машина игрока), не позади (центр не дальше
// 0,7 м позади центра этой машины, вдоль курса), точка руля сдвигается вбок от неё на 3 м × косинус угла между курсом и направлением на неё,
// каждый кадр, без сглаживания. -> сдвиг, м (> 0 — влево от курса)
static float svAvoid(const MsnCars *m, const McCar *c, const McEnv *e) {
  const Car *k = &c->car; float fx = sinf(k->h), fz = cosf(k->h), best = 50.f * 50.f, sh = 0.f;
#define SVC(o) do { const Car *q_ = (o); float dx_ = q_->x - k->x, dz_ = q_->z - k->z, dy_ = q_->y - k->y, d2_ = dx_ * dx_ + dz_ * dz_ + dy_ * dy_; \
                    if (d2_ < best && d2_ > 1e-4f) { float al_ = dx_ * fx + dz_ * fz; if (al_ >= -.7f) { \
                      best = d2_; sh = (-dx_ * fz + dz_ * fx >= 0.f ? 1.f : -1.f) * 3.f * fmaxf(0.f, al_ / sqrtf(d2_)); } } } while (0)   // расстояние — в 3D (круг 7)
  if (e->player) SVC(e->player);
  for (int i = 0; i < m->n; i++) if (&m->c[i] != c && m->c[i].on) SVC(&m->c[i].car);
  if (e->tr && e->tr->hd) {
    for (int i = 0; i < e->tr->maxCars; i++) if (e->tr->car[i].mode != TRM_OFF) SVC(&e->tr->car[i].car);
    for (int i = 0; i < e->tr->npark; i++) SVC(&e->tr->park[i].car);
  }
#undef SVC
  return sh;
}

// P04 A2 A4 A5 Д1 Д5, 1.4 1.5 1.6: руль и газ. Две точки впереди по пути: ближняя на 0,8 с (не меньше 5 м; задним ходом всегда 5 м), дальняя на 1,0 с
// (не меньше 10 м). Руль = угол до точки (рад) / предельный угол колёс .con, не больше 1; задним ходом знак обратный, ×1,25. Газ релейный: полный,
// пока скорость не выше желаемой + 5 км/ч, иначе ноль; тормоза в езде нет. Подготовка к повороту: вход, если направление от ближней точки к дальней
// отличается от курса больше 45° и скорость выше 2 м/с, выход — когда звено снова в пределах 45° или скорость не выше 1 м/с; пока она активна,
// руль по дальней точке (объезд не действует), а при угле до ближней точки больше ~32° (cos 0,85) и скорости выше 6 м/с — ручник и газ ноль
// (вход в поворот заносом); иначе выше 50 м/с тормоз, выше 40 м/с газ сброшен, ниже — газ по желаемой скорости.
// Перед трафиком, стоящими и другими машинами скорость НЕ ограничивается (прежний закон зазора — в control()).
#ifndef MC_SPRINT_JUMP_OLD
#define MC_SPRINT_JUMP_OLD 0
#endif
#ifndef MC_SPRINT_CURVE_OLD
#define MC_SPRINT_CURVE_OLD 0
#endif
static void controlOrig(const MsnCars *m, McCar *c, const McEnv *e, const float goal[3], float dt, CarInput *in) {
  Car *k = &c->car; float v = sqrtf(k->vel[0] * k->vel[0] + k->vel[1] * k->vel[1] + k->vel[2] * k->vel[2]), av = v, nearP[3], farP[3], aim[3];   // круг 7: «скорость» оригинала — модуль полной скорости, без знака
  int rev = c->backT > 0.f;
  if (c->direct && c->mode == MC_CHASE) { memcpy(nearP, goal, 12); memcpy(farP, goal, 12); }
  else { aimPoint(c, rev ? 5.f : fmaxf(5.f, .8f * av), nearP); aimPoint(c, fmaxf(10.f, 1.f * av), farP); }
  float ang = 0.f;
  { float ddx = farP[0] - nearP[0], ddz = farP[2] - nearP[2];
    if (ddx * ddx + ddz * ddz > 1.f) ang = wrapPi(atan2f(ddx, ddz) - k->h);
    // круг 3 (проверка по исходнику): под потерей управления поворот не готовится
    if (!c->turning && !rev && c->slipT <= 0.f && fabsf(ang) > .7854f && av > 2.f) c->turning = 1; }
  if (c->turning) memcpy(aim, farP, 12);
  else {
    memcpy(aim, nearP, 12);
    if (c->mode != MC_CHASE && !rev) { float sh = svAvoid(m, c, e); aim[0] += cosf(k->h) * sh; aim[2] += -sinf(k->h) * sh; c->shiftF = sh; }
  }
  float err = wrapPi(atan2f(aim[0] - k->x, aim[2] - k->z) - k->h), mw = fmaxf(.2f, k->p.maxWheelTurn), st = clampf(-err / mw, -1.f, 1.f);
  in->steer = (rev || c->stopHold) ? clampf(-st * 1.25f, -1.f, 1.f) : st;   // круг 3: знак обратный ×1,25 и в остановке после заднего хода
  if (c->slipT > 0.f) in->steer = clampf(in->steer * 1.5f, -1.f, 1.f);   // P04 A7: потеря управления — руль ×1,5
  memcpy(c->dbgNear, nearP, 12); memcpy(c->dbgFar, farP, 12);   // круг 6: точки руля этого кадра (тесты tests/aicar/sverka6)
  float vdes = c->vdes > 0.f ? c->vdes : fminf(k->p.topSpeed, c->vcap > 0.f ? c->vcap : 1e9f);
  // №1116 (сверка g03-aicars): бонус-трасса по дорогам — скорость по кривизне маршрута впереди (шикана и шпилька среза b07, зигзаги b02/b04). У оригинала желаемая
  // скорость — доля TopSpeed игрока, а поворот держит физика его машин и их ручник; у порта в зигзагах (полоса 6,5 м) водитель на 30 м/с упирался в борт. Правило
  // порта, только в бонус-игре (mcSprint): тот же расчёт, что у прежнего водителя (curveSpeed). MC_SPRINT_CURVE_OLD = 1 — без ограничения (прежнее)
  // №1147: зона прыжка данных (JMP1, трамплины b06/b03/b04) на машине или в 8 / 16 м впереди — полный газ, без ограничения по кривизне (старый водитель трассы так и делал, brDriverInit);
  // желаемая скорость соперника — доля скорости игрока, и с ней трамплин не берётся (b06: 9–15 падений за гонку). MC_SPRINT_JUMP_OLD = 1 — прежнее
  int jumpAhead = 0;
  if (!MC_SPRINT_JUMP_OLD && mcSprint && !rev && e->w && e->w->nzone) {
    float sn = sinf(k->h), cs = cosf(k->h);
    for (int q = 0; q < 3 && !jumpAhead; q++) jumpAhead = colJumpZoneAt(e->w, k->x + sn * 8.f * q, k->y + .5f, k->z + cs * 8.f * q) >= 0;
    if (jumpAhead) vdes = k->p.topSpeed;
  }
  if (!jumpAhead && !MC_SPRINT_CURVE_OLD && mcSprint && !rev) vdes = fminf(vdes, curveSpeed(c, vdes, fminf(2.f * (k->p.brakeScale + 3.f), 9.f) * .7f));
  c->vt = vdes; c->dbgGap = 1e9f; c->dbgSwerve = 1e9f; c->dbgKind = 0;
  if (rev) { in->brake = 1.f; in->throttle = 0.f; c->turning = 0; }
  else if (c->stopHold) { in->handbrake = 1.f; in->throttle = 0.f; if (av < 1.f) c->stopHold = 0; }   // после заднего хода — ручник до остановки
  else if (c->turning) {
    // круг 3 (порядок по проверке): сначала занос — нос отвернут от ближней точки больше ~32° и скорость выше 6 м/с: ручник, газ ноль, состояние остаётся;
    // потом выход (звено в пределах 45° или скорость не выше 1 м/с) с полным газом; иначе выше 50 м/с тормоз, выше 40 м/с газ сброшен, ниже — по желаемой скорости
    float en = wrapPi(atan2f(nearP[0] - k->x, nearP[2] - k->z) - k->h);
    if (cosf(en) < .85f && av > 6.f) { in->handbrake = 1.f; in->throttle = 0.f; }
    else if (fabsf(ang) <= .7854f || av <= 1.f) { c->turning = 0; in->throttle = 1.f; }
    else if (v > 50.f) in->brake = 1.f;
    else if (v > 40.f) in->throttle = 0.f;
    else in->throttle = v <= vdes + SV_KMH5 ? 1.f : 0.f;
  } else in->throttle = v <= vdes + SV_KMH5 ? 1.f : 0.f;
}

// P04 A6, 1.10 (круг 3, по проверке): застревание. Счёт идёт от первого кадра со скоростью не выше 1 м/с и не сбрасывается, когда машина поехала; он
// чистится только после заднего хода и при сбросе пути. Проверка идёт в кадрах со скоростью не выше 1 м/с: прошло больше порога — застряла (на втором и
// следующих замедлениях сразу). Порог 0,7 с; после заднего хода 1,5 с, пока скорость не поднимется выше 1 м/с, дальше снова 0,7 с. Задний ход: старт 0,3 с,
// каждое застревание +0,2 до 1,5 с, у погони 0,5 с; при скорости вперёд выше 14 м/с счётчик попыток и время заднего хода возвращаются. Седьмое срабатывание
// ставит машину на дорогу на месте — кроме погони и цели уничтожения этапа (McCar.noPlace, ставит game_msn.c). В ожидании, после тарана и под потерей
// управления счёт не идёт. После заднего хода — ручник до остановки, потом вперёд. Прежнее — в control() и drive() под MC_STUCK_OLD (вместе с MC_DRIVE_OLD)
static void svStuck(McCar *c, const McEnv *e, float dt) {
  Car *k = &c->car; float av = sqrtf(k->vel[0] * k->vel[0] + k->vel[1] * k->vel[1] + k->vel[2] * k->vel[2]);   // модуль полной скорости
  if (c->stkOn && c->backT <= 0.f) c->stkT += dt;   // время идёт всегда (метка времени в оригинале), в любом состоянии
  if (c->slipT > 0.f) { c->backT = 0.f; c->stopHold = 0; c->stkOn = 0; return; }   // потеря управления прерывает задний ход и остановку
  if (c->backT > 0.f) {
    c->backT -= dt; c->stkOn = 0; c->stkT = 0.f; c->stkThr = 1.5f;
    if (c->backT <= 0.f) { c->backT = 0.f; c->stopHold = 1; c->nroute = 0; }   // после заднего хода путь заново от этого места
    return;
  }
  if (c->waiting || c->ramT > 0.f || c->stopHold || c->turning) return;   // застревание проверяется только в обычной езде
  if (av > 14.f) { c->stuckN = 0; if (c->backLen > .3f) c->backLen = .3f; }
  if (av > 1.f) c->stkThr = .7f;
  if (c->stkThr <= 0.f) c->stkThr = .7f;
  if (av > 1.f) return;
  if (!c->stkOn) { c->stkOn = 1; c->stkT = 0.f; }
  if (c->stkT > c->stkThr) {
    // круг 7, сверено с оригиналом: время заднего хода: старт 0,3 с; у погони каждый раз 0,5 с; у остальных +0,2 с за срабатывание; если вышло больше 1,6 с — заднего хода
    // нет: счётчик возвращается к 0,3 с, порог 1,5 с, и машина ставится на дорогу на месте (кроме погони, цели уничтожения этапа и бамперных машин)
    c->stkOn = 0; c->stkT = 0.f; c->stuckN++;
    float len = c->mode == MC_CHASE ? .5f : (c->backLen > 0.f ? c->backLen : .3f) + .2f;
    if (len > 1.6f) {
      c->backLen = .3f; c->stkThr = 1.5f; c->stuckN = 0;
      if (!c->noPlace) {
        float p[4];
        if (mcResetPoint(e->tr, k->x, k->y, k->z, k->h, k->vel[0], k->vel[2], p)) carPlace(k, e->w, p[0], p[1] + .5f, p[2], p[3]);
        else carPlace(k, e->w, k->x, k->y + .5f, k->z, k->h);
        c->resets++; c->nroute = 0;
      }
    } else { c->backLen = len; c->backT = len; }
  }
}

// P04 A11 Д3 Д4, 1.1 1.2 1.3 1.7 1.8: желаемая скорость по типу этапа (пересчёт раз в 2 с — вызывающий). Догонялки всегда включены, по умолчанию
// гонка D = 80 м, 0,5 / 1,1 / 1,7; цель 20 и 70 м; уход 20 и 70 м. Игрок не за рулём — 0,7 TopSpeed своей машины.
//   гонка: ближе 25 м к игроку по прямой — «норма» (1,1 × скорость игрока); дальше — другой круг или другая контрольная точка дают крайние
//     значения (соперник впереди — «медленно», позади — «быстро»), на одной точке плавно по разнице расстояний до неё / D; результат
//     множитель × скорость игрока, зажатый между «медленно» и «быстро» × TopSpeed МАШИНЫ ИГРОКА
//   цель: от 1,0 TopSpeed машины игрока (игрок ближе «мин») до 0,2 (дальше «макс»), между линейно
//   уход: от своей TopSpeed (игрок ближе 20 м) до 0,7 её (дальше 70 м)
//   без догонялки (waypoint), погоня и полиция Hit & Run: 0,7 своей TopSpeed (круг 3, по проверке: пересчёт по типу этапа у погони не выполняется,
//     остаётся стартовое значение); первые 2 с после создания, смены этапа и сброса пути — тоже 0,7 своей (resetPath)
static float svPP[3], svPV[3]; static int svPPok;   // положение и скорость игрока по кадрам (пешком — скорость персонажа; считает mcUpdate)
static float svMult = 1.f, svF;   // множитель гонки и коэффициент догонялки последнего расчёта (svF: для добавки к броску умения срезать)
// круг 7, сверено с оригиналом: коэффициент догонялки f из −1…1 даёт скорость: между «минимумом» (f = −1), «серединой» (0) и «максимумом» (1);
//   гонка: минимум, середина и максимум — скорость игрока × «медленно», «норма», «быстро», КАЖДАЯ отдельно зажата между «медленно» и «быстро» × TopSpeed
//     машины игрока; f: ближе 25 м (включительно) к игроку — 0; иначе другой круг или другая точка — ∓1 (соперник впереди −1), одна точка — разница расстояний
//     до неё / D (80 м), не больше 1, со знаком «минус», если соперник ближе к точке
//   цель: минимум 0,2 × TopSpeed игрока, середина и максимум — 1,0 ×; f = −clamp((расстояние − «мин») / («макс» − «мин»), 0, 1) (20 и 70 м)
//   уход: минимум и середина — 0,7 своей TopSpeed, максимум — своя TopSpeed; f = clamp((«макс» − расстояние) / («макс» − «мин»), 0, 1) (20 и 70 м)
//   добавка к броску умения срезать — целая часть f × 20
// Игрок не в машине — 0,7 своей TopSpeed и добавка 0. Погоня и полиция Hit & Run расчёта вообще не делают: остаётся стартовое 0,7 своей TopSpeed.
static float svDesired(const MsnCars *m, const McCar *c, const McEnv *e) {
  const Car *k = &c->car; float top = c->baseTop; svMult = 1.f; svF = 0.f;
  if (c->mode == MC_CHASE || c->mode == MC_WAYPOINT || !e->player || !e->driving) return .7f * top;
  const Car *pl = e->player; float pv = fabsf(pl->v), ptop = pl->p.topSpeed, dx = pl->x - k->x, dy = pl->y - k->y, dz = pl->z - k->z, d = sqrtf(dx * dx + dy * dy + dz * dz);
  float mn, md, mx;
  if (c->mode == MC_EVADE) { float lo = 20.f, hi = 70.f; svF = clampf((hi - d) / (hi - lo), 0.f, 1.f); mn = md = .7f * top; mx = top; }
  else if (c->mode == MC_TARGET) {
    float lo = c->hasTc ? c->tc[0] : 20.f, hi = c->hasTc ? fmaxf(lo + 1.f, c->tc[1]) : 70.f;
    svF = clampf((lo - d) / (hi - lo), -1.f, 0.f); mn = .2f * ptop; md = mx = ptop;
  } else {
    static const float rcDef[4] = {80.f, .5f, 1.1f, 1.7f}; const float *rc = c->hasRc ? c->rc : rcDef;
    float D = fmaxf(1.f, rc[0]), amin = ptop * rc[1], amax = ptop * rc[3];
    mn = clampf(pv * rc[1], amin, amax); md = clampf(pv * rc[2], amin, amax); mx = clampf(pv * rc[3], amin, amax);
    if (d > 25.f) {
      float lead = 0.f; int kind = 0;   // kind: 1 соперник впереди, −1 игрок впереди, 0 — по расстоянию
      if (c->rAhead >= 0 && svPk >= 0) {   // место игрока в гонке посчитано (game_msn.c): круги и точка, потом разница расстояний
        if (c->rk > svPk) kind = 1; else if (c->rk < svPk) kind = -1; else lead = (svDp - c->rDist) / D;
      } else {   // гонка без этих данных (боты, бонус-игра): разница пройденного по пути соперника, как в прежней догонялке
        float tot, sc = raceDoneLen(m, c, &tot);
        lead = (e->haveFrac && !MC_CATCHUP_GEO_OLD && tot >= 1.f ? sc - playerDoneLen(m, c, e->ppos, e->pfrac, tot) : mcRemainingFrom(m, e->ppos) - mcRemaining(m, c)) / D;
      }
      svF = kind ? (float)(-kind) : -clampf(lead, -1.f, 1.f);   // соперник впереди (kind 1, lead > 0) — f < 0
    }
    svMult = md;
  }
  return md + svF * (svF > 0.f ? mx - md : md - mn);
}

// Исторические пороги a292ed4 оставлены только для красного POLICE_CHASE_OLD
// 430: полиция преследует и стоящего игрока; PS2 natural road stand 94,432 -> 54,999 м
// Ни 93,7, ни 55 м не универсальный радиус остановки: в записи дорогу заняла машина
#ifndef POLICE_CHASE_OLD
#define POLICE_CHASE_OLD 0
#endif
#define POLICE_STANDOFF 30.f
#ifndef POLICE_STAND_OLD
#define POLICE_STAND_OLD 0
#endif
#ifndef POLICE_KEEPOFF_OLD
#define POLICE_KEEPOFF_OLD 0
#endif
#define POLICE_STAND_MIN 93.7f
#define POLICE_STAND_V 1.f
// №723 (Роб 17.10, видео IMG_7280 4:53: «полицейская машина въезжает на газон»). Записи PS2 (build/ps2ref/answers: b3_moving, b3_fine80, tw_v8_stand; расчёт tests/police723): к СТОЯЩЕМУ игроку у PS2 полиция
// выезжает с дороги в 45 м и подъезжает не ближе 27 м (b3_moving: 27 м на кадре 225, дальше держится 39–46 м, раз в ~11 с возвращается к месту появления; b3_fine80 на возвышении — 28,5 м; у дороги — 55 м, №430).
// После №430 порт подъезжал к стоящему игроку вплоть до 13 м и въезжал в двор на 31 м от оси дороги (PS2 — не дальше 28 м). Правило: игрок за рулём стоит (< POLICE_STAND_V) — полиция останавливается так, чтобы
// остановка (тормозной путь 8 м/с², как выше) не оказалась ближе POLICE_PS2_STAND = 27 м; стоящего дальше — по-прежнему догоняет (№430). Едущего игрока и пешего Гомера не касается.
// POLICE_PS2STAND_OLD=1 / flag police723 0 — как было (красный tests/police723)
#ifndef POLICE_PS2STAND_OLD
#define POLICE_PS2STAND_OLD 0
#endif
#define POLICE_PS2_STAND 27.f
int mcPolice723Old = POLICE_PS2STAND_OLD;
static void drive(MsnCars *m, McCar *c, const McEnv *e, float dt) {
  Car *k = &c->car; CarInput in = {0.f, 0.f, 0.f, 0.f};
#if !MC_SVKIND_OLD
  k->svKind = c->police ? 3 : 2; k->svRace = c->mode == MC_RACE || mcPlanHook || mcSprint ? 1 : 0;   // круг 2, g01 7.4: вид для svBoom и взрыв гоночной машины через 3 с
#endif
  // цель этого кадра: точка пути или игрок
  float goal[3]; int have = 0;
  if (c->mode == MC_CHASE) {
    goal[0] = e->ppos[0]; goal[1] = e->ppos[1]; goal[2] = e->ppos[2]; have = 1;
#if MC_CHASE_OLD
    if (e->player && e->driving) { goal[0] += e->player->vel[0] * .5f; goal[2] += e->player->vel[2] * .5f; }
#else
    {   // сверка P04 A9 Д6 и 2.10 (круг 3, по проверке): дальше порога — по дорогам к дороге игрока (путь до точки в 10 м от него); ближе (20 м; полиция
        // Hit & Run 60 м) — напрямую на точку упреждения: игрок + вектор его скорости (машины или, пешком, Гомера), приведённый к длине «расстояние / своя
        // скорость» метров (своя скорость ноль или назад — 1000 м); цель стоит — смещения нет. Проверка препятствий на прямой не делается. Было: ближе 30 м,
        // игрок + скорость × 0,5 с, только за рулём, луч до здания у полиции (убран)
      float ddx = e->ppos[0] - k->x, ddz = e->ppos[2] - k->z, dist = sqrtf(ddx * ddx + ddz * ddz), thr = c->police ? 60.f : 20.f;
      c->direct = dist < thr;
      if (c->direct) {
        float vx = e->player && e->driving ? e->player->vel[0] : svPV[0], vz = e->player && e->driving ? e->player->vel[2] : svPV[2], sp = sqrtf(vx * vx + vz * vz);
        if (sp > 1e-3f) { float len = k->v > 1e-3f ? fminf(1000.f, dist / k->v) : 1000.f; goal[0] += vx / sp * len; goal[2] += vz / sp * len; }
      }
    }
#endif
    // Прежние отход и остановка, только отрицательный контроль №430
    if (c->police && POLICE_CHASE_OLD) {
      float pv = e->player && e->driving ? sqrtf(e->player->vel[0] * e->player->vel[0] + e->player->vel[2] * e->player->vel[2]) : 0.f;
      float dd2 = d2xz(e->ppos, k->x, k->z);
      if (pv < 30.f / 3.6f && dd2 < POLICE_STANDOFF * POLICE_STANDOFF) have = 0;
      else if (!POLICE_STAND_OLD && e->player && e->driving && pv < POLICE_STAND_V) {   // аудит 09.10: к стоящей издалека не едет
        float dx = k->x - e->ppos[0], dz = k->z - e->ppos[2], l = sqrtf(dd2) + 1e-3f;
        goal[0] = k->x + dx / l * 200.f; goal[1] = k->y; goal[2] = k->z + dz / l * 200.f;   // прочь от игрока (правило порта)
        // ОСТАТОК 09.10 п. 13: «прочь» по дорогам на L2/L3/L5 вело мимо игрока — полиция подходила к стоящему на 78–87 м. У PS2
        // ближе POLICE_STAND_MIN не бывает: идёт к игроку (скорость к нему > 0) и с тормозным путём (8 м/с², правило порта) окажется
        // ближе — стоит (дальше её переставляет правило 10 с / 110 м, как в main.c hnrPursuit). POLICE_KEEPOFF_OLD=1 — прежнее
        float close = -(k->vel[0] * dx + k->vel[2] * dz) / l;
        if (!POLICE_KEEPOFF_OLD && close > 0.f && l - close * close / 16.f - close * .25f < POLICE_STAND_MIN) have = 0;
      }
    }
    // сверка СВЕРКА_С_ИСХОДНИКОМ 2.10 (Роб 01.11: «делать всё как в оригинале»): полиция всегда едет, стоп в 27 м от стоящего игрока убран под POLICE_STAND27_OLD = 1
    if (POLICE_STAND27_OLD && c->police && !POLICE_CHASE_OLD && !mcPolice723Old && have && e->player && e->driving && hypotf(e->player->vel[0], e->player->vel[2]) < POLICE_STAND_V) {   // №723: к стоящему не ближе 27 м
      float dx = k->x - e->ppos[0], dz = k->z - e->ppos[2], l = sqrtf(dx * dx + dz * dz) + 1e-3f, close = -(k->vel[0] * dx + k->vel[2] * dz) / l;
      if (l - (close > 0.f ? close * close / 16.f + close * .25f : 0.f) < POLICE_PS2_STAND) have = 0;
    }
  } else if (m->nwp && (!c->finished || (!MC_LOOP_OLD && c->mode != MC_RACE))) {
    int T = total(m);
#if MC_WPR_OLD
    while (c->wp < T && d2xz(wpAt(m, c->wp), k->x, k->z) < WP_R * WP_R) { c->passed = c->wp; c->wp++; c->nroute = 0; }
#else
    // сверка P04 A13 B2: расстояние до точки в 3D, радиус 10 м (бонус-игра 7 м). Было: в плоскости
    if (c->wp < T) { const float *q_ = wpAt(m, c->wp); float dy_ = q_[1] - k->y, r_ = wpRad();   // круг 7: за кадр проходится одна точка, следующая проверяется в следующем кадре
      if (d2xz(q_, k->x, k->z) + dy_ * dy_ < r_ * r_) { c->passed = c->wp; c->wp++; c->nroute = 0; } }
#endif
    if (c->wp >= T) { c->finished = 1; if (!MC_LOOP_OLD && c->mode != MC_RACE) { c->wp = 0; c->nroute = 0; } }   // круг 7: последняя точка пройдена — снова с первой
    if (c->wp < T) { memcpy(goal, wpAt(m, c->wp), sizeof goal); have = 1; }
  }
  float top = MC_BREAK_SPEED ? 60.f : c->baseTop;   // отрицательный контроль: скорость не из .con
  aiSubstep(c, e, dt);
  if (!have || e->hold) {
    in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f;
    // №старт-гонки (разбор железа TEST19 п. 2, Лиза: ph4 6,7 мс, 2,6 с подряд кадры 35–41 мс): за 3 с отсчёта стоящие соперники на ручнике считали полную физику
    // (подшаги по расстоянию до игрока) каждый кадр — на Маке 1,1 мс из 1,2 мс логики миссии. Машина, простоявшая на месте > 0,4 с (скорости и вращение ~0), считается раз в
    // 4 кадра (RACE_START_LIGHT_OLD=1 — каждый кадр, как было); удар по ней (asleep = 0, still = 0 в mcUpdate) будит её сразу
#ifndef RACE_START_LIGHT_OLD
#define RACE_START_LIGHT_OLD 0
#endif
    if (!RACE_START_LIGHT_OLD && c->still > .4f && (c->holdSkip++ & 3)) { c->lastIn = in; return; }
    { unsigned pu_ = mcClockUs ? mcClockUs() : 0u; carUpdate(k, e->w, &in, dt); if (mcClockUs) mcPartMs[0] += (mcClockUs() - pu_) / 1000.f; } c->lastIn = in;
    if (!RACE_START_LIGHT_OLD) {
      float wv = fabsf(k->w[0]) + fabsf(k->w[1]) + fabsf(k->w[2]);
      c->still = (fabsf(k->v) < .1f && fabsf(k->vl) < .1f && wv < .1f && fabsf(k->vel[1]) < .1f) ? c->still + dt * (c->still > .4f ? 4.f : 1.f) : 0.f;   // аудит 16.10: до порога 0,4 с время идёт обычное (было ×4: порог за 0,1 с), после — каждый 4-й кадр шагом ×4; падающая машина (vy) не стоит
    }
    return;
  }
#if !MC_FREEZE_OLD
  if (e->freeze && c->police) {   // круг 2, g10 2.8: в разговоре полиция стоит на месте
    in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f; carUpdate(k, e->w, &in, dt); c->lastIn = in; return;
  }
#endif
#if !MC_WAIT_OLD
  {   // сверка P04 A10: ИИ дальше 300 м от игрока по прямой встаёт на ручник (гонка — только если он впереди игрока по кругам, контрольной точке или
      // расстоянию, кроме уличных гонок 1 и 2: McEnv.noRaceWait); как игрок подошёл ближе 300 м — едет дальше. Ждёт больше 20 с — событие миссии
      // «ИИ добрался до цели» (McCar.waitEvt растёт; читает game_msn.c), дальше каждые 20 с; счёт не обнуляется при выходе из ожидания, только после события
    int waitable = (c->mode == MC_RACE || c->mode == MC_EVADE || c->mode == MC_TARGET || c->mode == MC_WAYPOINT) && e->player && !mcPlanHook && !mcSprint && !e->noRaceWait;   // круг 3: уличные гонки 1 и 2 не ждут при любом типе цели
    if (waitable && c->mode == MC_RACE) {   // гонка: ждёт, только если впереди игрока; места игрока нет (боты) — по остатку пути
      int ahead = c->rAhead >= 0 ? c->rAhead : (m->nwp && mcRemaining(m, c) < mcRemainingFrom(m, e->ppos));
      waitable = ahead != 0;
    }
    { float dy_ = e->ppos[1] - k->y; c->waiting = waitable && d2xz(e->ppos, k->x, k->z) + dy_ * dy_ > 300.f * 300.f; }   // круг 3: расстояние в 3D
    if (c->waiting) {
      in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f; c->still = 0.f;
      carUpdate(k, e->w, &in, dt); c->lastIn = in;
      if ((c->waitT += dt) >= 20.f) { c->waitT = 0.f; c->waitEvt++; }   // круг 6: счётчик (game_msn.c сравнивает с уже учтённым: импульс терял каждое второе событие)
      return;
    }
  }
#endif
#if !MC_HIT_OLD
  if (c->mode == MC_CHASE && c->ramT > 0.f) {   // сверка P04 A8: преследователь сам ударил машину — 0,5 с на ручнике (пока контакт не прерван — заново), потом назад 0,5 с
    c->ramT -= dt; in.handbrake = 1.f; in.brake = 0.f;
    carUpdate(k, e->w, &in, dt); c->lastIn = in;
    if (c->ramT <= 0.f) { c->ramT = 0.f; c->backT = c->backLen > 0.f ? c->backLen : .3f; c->stkOn = 0; c->stopHold = 0; }   // круг 3: задний ход 0,3 с (после застревания погони 0,5 с, пока не разогналась выше 14 м/с)
    return;
  }
  if (c->baseGrip <= 0.f) c->baseGrip = k->p.tireGrip;
  if (c->slipT > 0.f) c->slipT -= dt;   // сверка P04 A7: 1,5 с после удара — руль ×1,5 (в controlOrig); круг 3: придуманное ×0,6 сцепления убрано (режим скольжения физики не прочитан)
#endif
  // маршрут: новая цель, конец обрезанного пути, погоня — раз в секунду
  c->replanT -= dt;
  int urgent = c->nroute == 0 || c->routeWp != c->wp;   // нет пути / новая цель — сразу
  int chaseNeed = c->mode == MC_CHASE && c->replanT <= 0.f;
#if !MC_CHASE_OLD
  if (c->mode == MC_CHASE) {   // сверка P04 Д13: погоня перестраивает путь, когда цель перешла на другую дорогу (а не по таймеру: он — страховка 5 с); едет напрямую — путь не нужен
    chaseNeed = 0;
    if (!c->direct && roadsOk(e->tr)) {   // круг 3: проверка смены дороги под целью раз в 0,25 с (осознанно, ради кадра PSP); страховочного таймера нет
      if ((c->chaseRoadT -= dt) <= 0.f) {
        int r_, l_; float s_, d_; c->chaseRoadT = .25f;
        int rr = nearestLane(e->tr, e->ppos[0], e->ppos[1], e->ppos[2], 0.f, -1.f, LANE_SNAP * LANE_SNAP, 1, &r_, &l_, &s_, &d_) ? r_ : -1;
        if (rr != c->chaseRoad) { c->chaseRoad = rr; chaseNeed = 1; }
      }
    }
  }
#endif
  int need = urgent || (c->routeCut && c->seg >= c->nroute - 3) || chaseNeed;
#if !MC_STUCK_OLD
  {   // сверка P04 Д13 (круг 3: порог 10 м, и у погони): дальше 10 м от всех звеньев пути дольше 3 с — путь заново
    if (c->offLane > 10.f && c->nroute > 1 && !c->direct) { if ((c->offT += dt) > 3.f) { need = 1; c->offT = 0.f; } } else c->offT = 0.f;
  }
#endif
  // Очередь перепланировок (ресёрч процессора 10.10): у погонь 3–4 ИИ-машин перепланировка «раз в секунду» совпадает во времени у всех,
  // и один кадр получает 3–4 поиска подряд. Плановая (не срочная) — не больше одной за кадр на все машины; остальные — в следующих
  // кадрах (replanT остаётся ≤ 0, машина едет по прежнему пути). -DROADPLAN_QUEUE_OLD=1 или -DROADPLAN_OLD=1 — как было
  if (need && !urgent && !ROADPLAN_QUEUE_OLD && !mcRoadPlanOld && replanBudget <= 0) need = 0;
  // №537 (11.10): первый маршрут всех машин гонки/погони строился в один кадр (отмашка «GO»): 3–4 поиска по дорогам подряд, кадр логики 18 мс против 6 (тур 12, P3 L3 sr3).
  // Первый маршрут машины после resetPath (routeWp -1) — не больше одного поиска за кадр на все машины: остальные стоят на ручнике ещё кадр-два (≤ 50 мс), маршрут тот же,
  // строится на следующем кадре с того же места. Срочные перепланировки в пути (новая цель, обрезанный путь) — сразу, как было. -DMCAR_WARM_OLD=1 — как было
  if (!MCAR_WARM_OLD && urgent && c->routeWp == -1 && c->nroute == 0 && !mcRoadPlanOld && replanBudget <= 0) { in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f; carUpdate(k, e->w, &in, dt); c->lastIn = in; return; }
  if (need) { if (!urgent || (!MCAR_WARM_OLD && c->routeWp == -1)) replanBudget--; replan(c, m, e, goal); c->routeWp = c->wp; c->replanT = 1.f;
#if !MC_CHASE_OLD
    // круг 3, P04 A9: путь погони по дорогам строится до точки в 10 м от игрока
    if (c->mode == MC_CHASE) while (c->nroute > 2 && d2xz(c->route[c->nroute - 1], e->ppos[0], e->ppos[2]) < 100.f) c->nroute--;
#endif
  }
  // догонялка
  float mult = 1.f; (void)mult;
#if !MC_SPEED_OLD
  // сверка P04 A11 Д3 Д4, 1.1–1.3 1.7 1.8: желаемая скорость по типу этапа (svDesired), пересчёт раз в 2 с. TopSpeed физики машины не меняется
  // (раньше множитель догонялки менял сам предел скорости и газ). Было: множитель к TopSpeed своей машины, потолок множитель × скорость игрока
  if ((c->catchT -= dt) <= 0.f || c->vdes <= 0.f) { c->catchT = 2.f; c->vdes = svDesired(m, c, e); c->mult = svMult; c->catchF = svF; } c->vcap = MC_DRIVE_OLD ? c->vdes : 1e9f;
  k->p.topSpeed = fmaxf(3.f, top); k->p.gasScale = c->baseGas;
#else
  if (c->hasRc && e->player) {
    // 03.10 (разбор ночи 02.10, m7 The Fat and Furious): «впереди ли соперник» мерилось по прямой до ЛЮБОЙ точки его пути
    // (mcRemainingFrom — минимум по всем точкам): на трассе с возвратом (m7: юг -> восток -> север -> финиш у старта) игрок у старта
    // считался почти финишировавшим, и Смитерс весь заезд шёл с догонялкой ×1.5 (максимум), даже далеко впереди. Теперь — разница
    // пройденного по пути соперника (raceDoneLen): игрок — проекцией на этот путь, а из мест с возвратом — ближайшее к доле его гонки
    // по его точкам (pfrac, как место в гонке: правка 178, близнец В9). Включает game_msn.c (уровень 1); -DMC_CATCHUP_GEO_OLD=1 —
    // прежняя мера (красный tests/aicar/catchup.sh)
    float tot, sc = raceDoneLen(m, c, &tot);
    float lead = e->haveFrac && !MC_CATCHUP_GEO_OLD && tot >= 1.f ? sc - playerDoneLen(m, c, e->ppos, e->pfrac, tot)
                                                                  : mcRemainingFrom(m, e->ppos) - mcRemaining(m, c), D = fmaxf(1.f, c->rc[0]);
    mult = lead >= 0.f ? c->rc[2] + (c->rc[1] - c->rc[2]) * clampf(lead / D, 0.f, 1.f)
                       : c->rc[2] + (c->rc[3] - c->rc[2]) * clampf(-lead / D, 0.f, 1.f);
  }
  if (c->hasTc && e->player) {
    float d = sqrtf(d2xz(e->ppos, k->x, k->z)), lo = c->tc[0], hi = fmaxf(lo + 1.f, c->tc[1]);
    mult *= 1.f - .8f * clampf((d - lo) / (hi - lo), 0.f, 1.f);
  }
  if (c->mode == MC_EVADE && d2xz(e->ppos, k->x, k->z) < 15.f * 15.f) mult *= 1.15f;
  c->mult = mult;
  // аудит 09.10 п. 10: цель водителя — не быстрее множитель × скорость игрока (тормозит водитель, как перед поворотом); TopSpeed
  // физики кадра — прежний (top × множитель): сопротивление car.c считается от него, иначе низкий потолок гасил бы ход за кадр
  c->vcap = !MC_CATCHUP_SPEED_OLD && c->hasRc && e->player && e->driving ? fmaxf(3.f, mult * fabsf(e->player->v)) : 1e9f;
  k->p.topSpeed = fmaxf(3.f, top * mult); k->p.gasScale = c->baseGas * fmaxf(1.f, mult);
#endif
#if !MC_DRIVE_OLD
  svStuck(c, e, dt);   // (прежнее застревание — только вместе с MC_DRIVE_OLD = 1: оно внутри control())
  controlOrig(m, c, e, goal, dt, &in);
#else
  control(m, c, e, goal, dt, &in);
#endif
  // ПРАВИЛО ПОРТА: урон машине миссии — только от машин (carHitCar в mcUpdate); удар о стену и землю не бьёт — иначе коробки
  // коллизии поперёк полос (пункт 11 очереди: 7 мест) разбивают соперника гонки и цель погони раньше игрока
  float dmg0 = k->damage, dead0 = k->deadT; int des0 = k->destroyed;
  c->still = 0.f;   // едет: счёт покоя на отсчёте (RACE_START_LIGHT_OLD) заново
  { unsigned pu_ = mcClockUs ? mcClockUs() : 0u; carUpdate(k, e->w, &in, dt); if (mcClockUs) mcPartMs[0] += (mcClockUs() - pu_) / 1000.f; } c->lastIn = in;
  k->damage = dmg0; k->destroyed = des0; if (!des0) k->deadT = dead0;   // правка 199: часы взрыва идут только у разбитой
  // стоит на месте 10 с (сместилась меньше 8 м от точки отсчёта: упёрлась, качается у стены) / перевёрнута, а игрок
  // далеко — на свой путь впереди (правило порта, вне вида игрока). Перепланировка маршрута ход не обнуляет
#if MC_STUCK_OLD
  float far2 = d2xz(e->ppos, k->x, k->z);
  if (c->progWp != c->wp || d2xz(c->progRef, k->x, k->z) > 64.f || c->mode == MC_CHASE) {
    c->progWp = c->wp; c->progRef[0] = k->x; c->progRef[1] = k->y; c->progRef[2] = k->z; c->progT = 0.f;
  } else c->progT += dt;
  if ((c->progT > NOPROG_T || k->flipT > 3.f) && far2 > RESET_FAR * RESET_FAR && c->nroute > 1) {
    int j = c->seg + 2 < c->nroute ? c->seg + 2 : c->nroute - 1; const float *a = c->route[j];
    const float *b = c->route[j + 1 < c->nroute ? j + 1 : j];
    float h = (j + 1 < c->nroute) ? atan2f(b[0] - a[0], b[2] - a[2]) : atan2f(goal[0] - a[0], goal[2] - a[2]);
    carPlace(k, e->w, a[0], a[1] + .5f, a[2], h); c->stuckN = 0; c->resets++; c->nroute = 0; c->progT = 0.f;
  }
#else
  // сверка P04 Д2, 1.11: машина ИИ с путевыми точками (гонка, уход, цель), кузов наклонён от вертикали на 75° и больше (cos ≤ 0,2588) — через 1 с
  // ставится на месте на колёса; встала сама за эту секунду — отсчёт отменяется. От расстояния до игрока не зависит (было: вверх колёсами > 3 с и игрок
  // дальше 60 м — перенос на точку пути; правило «10 с без приближения на 8 м» и перенос после 5 застреваний убраны: см. svStuck)
  if (c->mode != MC_CHASE) {
    if (1.f - 2.f * (k->q[1] * k->q[1] + k->q[3] * k->q[3]) <= .2588f) {   // вертикаль кузова из кватерниона (pitch и roll вверх колёсами нули)
      if ((c->flipHoldT += dt) >= 1.f) { carPlace(k, e->w, k->x, k->y + .5f, k->z, k->h); c->flipHoldT = 0.f; c->resets++; c->nroute = 0; }
    } else c->flipHoldT = 0.f;
  }
#endif
  k->p.topSpeed = top; k->p.gasScale = c->baseGas;   // параметры .con — как в данных (множитель — только на кадр)
  if (c->baseGrip > 0.f) k->p.tireGrip = c->baseGrip;
}

// бот-игрок (tools/bots/missioner): тот же водитель ведёт машину игрока к точке goal по полосам. c — тень водителя у бота:
// c->car — копия машины игрока этого кадра (бот копирует перед вызовом), остальное — состояние водителя между кадрами;
// c->mode = MC_CHASE — таран (рядом прямо на цель, не тормозит перед машиной c->ignore)
void mcBotInput(const MsnCars *m, McCar *c, const McEnv *e, const float goal[3], float dt, CarInput *in) {
  memset(in, 0, sizeof *in);
  c->baseTop = c->car.p.topSpeed; c->baseGas = c->car.p.gasScale;
  c->replanT -= dt;
  int need = c->nroute == 0 || d2xz(c->goalAt, goal[0], goal[2]) > 25.f || (c->routeCut && c->seg >= c->nroute - 3) ||
             (c->mode == MC_CHASE && c->replanT <= 0.f);
  if (need) { replan(c, m, e, goal); memcpy(c->goalAt, goal, sizeof c->goalAt); c->replanT = 1.f; }
  control(m, c, e, goal, dt, in);
}
void mcBotInputPath(const MsnCars *m, McCar *c, const McEnv *e, const float (*pts)[3], int n, float dt, CarInput *in) {
  memset(in, 0, sizeof *in);
  c->baseTop = c->car.p.topSpeed; c->baseGas = c->car.p.gasScale;
  if (n > MC_ROUTE - 1) n = MC_ROUTE - 1;
  if (n < 1) return;
  c->route[0][0] = c->car.x; c->route[0][1] = c->car.y; c->route[0][2] = c->car.z;   // от машины — по точкам
  memcpy(c->route + 1, pts, sizeof(float) * 3 * (size_t)n); c->nroute = n + 1; c->seg = 0; c->routeCut = 0;
  control(m, c, e, pts[n - 1], dt, in);
}

// ---------------------------------------------------------------- кадр
// сверка P04 A7 A8: кто кого ударил — по скорости сближения вдоль линии центров до удара (ov — скорость той, что на (ox, oz)); у кого больше, тот ударил.
// Ударил сам преследователь (его не ударили): пауза на ручнике 0,5 с (перезапускается, пока контакт не прерван), потом назад 0,5 с (drive).
// Ударила другая машина с заметной скоростью сближения (> 1 м/с, ≈ нормированный импульс 0,02 оригинала): 1,5 с руль ×1,5 и пониженное сцепление
static void svHitNote(McCar *c, const float cv[3], const Car *o, const float ov[3]) {
#if !MC_HIT_OLD
  // круг 7, сверено с оригиналом: кто кого ударил. Стоит (скорость < 0,005 м/с) тот, кого ударили; если стоит другая — ударил я. Иначе я ударил, если моя скорость направлена
  // на другую в пределах 30° (cos 0,866); но при встречном ударе (другая тоже направлена на меня в пределах 30°) «ударены» обе. Сила — нормированный импульс: импульс / 100000;
  // порог потери управления 0,02. Импульс: приведённая масса × 1,5 × скорость сближения (число 1,5 наше: упругость удара в оригинале берёт физика)
  float nx = o->x - c->car.x, ny = o->y - c->car.y, nz = o->z - c->car.z, l = sqrtf(nx * nx + ny * ny + nz * nz);
  if (l < 1e-3f) return;
  nx /= l; ny /= l; nz /= l;
  float ms = sqrtf(cv[0] * cv[0] + cv[1] * cv[1] + cv[2] * cv[2]), hs = sqrtf(ov[0] * ov[0] + ov[1] * ov[1] + ov[2] * ov[2]); int hit;
  if (ms < .005f) hit = 1;
  else if (hs < .005f) hit = 0;
  else if ((cv[0] * nx + cv[1] * ny + cv[2] * nz) / ms < .8660254f) hit = 1;
  else hit = -(ov[0] * nx + ov[1] * ny + ov[2] * nz) / hs >= .8660254f;
  if (hit) {
    float m1 = c->car.p.mass, m2 = o->p.mass, jn = (m1 + m2 > 1.f ? m1 * m2 / (m1 + m2) : 0.f) * 1.5f * c->car.carHitV / 100000.f;
    if (jn > .02f && c->mode != MC_PARKED && !c->waiting) c->slipT = 1.5f;
  } else if (c->mode == MC_CHASE) c->ramT = .5f;
#else
  (void)c; (void)cv; (void)o; (void)ov;
#endif
}
static int near2(const Car *a, const Car *b, float extra) {
  float ra = a->p.halfL + fabsf(a->p.bodyOfs) + .5f, rb = b->p.halfL + fabsf(b->p.bodyOfs) + .5f, r = ra + rb + extra;
  float dx = a->x - b->x, dz = a->z - b->z; return dx * dx + dz * dz < r * r && fabsf(a->y - b->y) < 3.f;
}
void mcTrafficSees(const MsnCars *m, Traffic *t) {
  if (!t) return;
  t->nextra = 0;
  for (int i = 0; m && i < m->n && t->nextra < TR_MAXEXTRA; i++) if (m->c[i].on) t->extra[t->nextra++] = &m->c[i].car;
}
void mcUpdate(MsnCars *m, const McEnv *e, float dt, void (*hit)(void *u, const char *name), void *u) {
  if (dt <= 0.f || !e->w) return;
  if (dt > .1f) dt = .1f;
  { float pp[3] = {e->ppos[0], e->ppos[1], e->ppos[2]};   // скорость игрока по кадрам (нужна погоне, когда игрок пешком)
    if (svPPok && dt > 1e-4f) { svPV[0] = (pp[0] - svPP[0]) / dt; svPV[1] = (pp[1] - svPP[1]) / dt; svPV[2] = (pp[2] - svPP[2]) / dt;
      if (svPV[0] * svPV[0] + svPV[2] * svPV[2] > 3600.f) svPV[0] = svPV[1] = svPV[2] = 0.f; }   // скачок (телепорт): не скорость
    memcpy(svPP, pp, sizeof svPP); svPPok = 1; }
  svStreet = e->noRaceWait;
  replanBudget = 1;   // плановых перепланировок на кадр (см. очередь в drive)
  for (int i = 0; i < m->n; i++) {
    McCar *c = &m->c[i]; if (!c->on) continue;
    Car *k = &c->car;
    int moving = c->mode != MC_PARKED && !(c->finished && fabsf(k->v) < .3f && (MC_LOOP_OLD || c->mode == MC_RACE)) && !k->destroyed;
    if (moving) { c->asleep = 0; unsigned pd_ = mcClockUs ? mcClockUs() : 0u; drive(m, c, e, dt); if (mcClockUs) { mcPartMs[1] += (mcClockUs() - pd_) / 1000.f; mcPartMs[2] += 1.f; } }
    else if (!c->asleep) {   // стоит / доехала / разбита: физика до покоя, потом спит (как стоящие машины трафика)
      CarInput in = {0.f, fabsf(k->v) > .5f ? 1.f : 0.f, 0.f, 1.f}; carUpdate(k, e->w, &in, dt);
      float wv = fabsf(k->w[0]) + fabsf(k->w[1]) + fabsf(k->w[2]);
      c->still = (fabsf(k->v) < .2f && fabsf(k->vl) < .2f && wv < .2f) ? c->still + dt : 0.f;
      if (c->still > 1.f) c->asleep = 1;
    }
    c->wheelAng += k->v / fmaxf(.2f, k->p.wheelRad) * dt;
  }
#if !MC_PROPTOUCH_OLD
  if (e->carTouch) for (int i = 0; i < m->n; i++) {   // круг 2, g11 P11-02: машины ИИ рядом с игроком (80 м) задевают предметы, как машина игрока
    McCar *c = &m->c[i]; if (!c->on || c->car.destroyed || c->asleep || fabsf(c->car.v) < .3f || d2xz(e->ppos, c->car.x, c->car.z) > 80.f * 80.f) continue;   // круг 6: только движущиеся и рядом
    e->carTouch(e->touchU, &c->car);
  }
#endif
  // удары: игрок, машины миссии между собой, трафик и стоящие
  for (int i = 0; i < m->n; i++) {
    McCar *c = &m->c[i]; if (!c->on) continue;
    if (e->player && e->driving && near2(e->player, &c->car, .5f)) {
      float before = e->player->hitSpeed, pv0[3], cv0[3]; memcpy(pv0, e->player->vel, sizeof pv0); memcpy(cv0, c->car.vel, sizeof cv0);
      if (carHitCar(e->player, &c->car, e->w)) {
        svHitNote(c, cv0, e->player, pv0);
        c->asleep = 0; c->still = 0.f;
        if (e->player->hitSpeed > 1.f && e->player->hitSpeed > before - 1e-4f) { m->hits++; mcHitSpeed = e->player->hitSpeed; if (hit) hit(u, c->name); }
      }
    }
    for (int j = i + 1; j < m->n; j++) {
      if (e->noNpcCollide) continue;
      McCar *o = &m->c[j]; if (!o->on || !near2(&c->car, &o->car, .3f)) continue;
      float cv0[3], ov0[3]; memcpy(cv0, c->car.vel, sizeof cv0); memcpy(ov0, o->car.vel, sizeof ov0);
      if (carHitCar(&c->car, &o->car, e->w)) { c->asleep = o->asleep = 0; c->still = o->still = 0.f;
        svHitNote(c, cv0, &o->car, ov0); svHitNote(o, ov0, &c->car, cv0); }
    }
    if (e->tr && e->tr->hd) {
      Traffic *t = e->tr;
      for (int j = 0; j < t->maxCars; j++) {
        TrCar *tc = &t->car[j]; if (tc->mode == TRM_OFF || !near2(&c->car, &tc->car, .3f)) continue;
        float cv0[3], tv0[3]; memcpy(cv0, c->car.vel, sizeof cv0); memcpy(tv0, tc->car.vel, sizeof tv0);
        if (carHitCar(&c->car, &tc->car, e->w)) { if (tc->mode == TRM_DRIVE) { tc->mode = TRM_PHYS; tc->stillT = 0.f; } c->asleep = 0;
          svHitNote(c, cv0, &tc->car, tv0); }
      }
      for (int j = 0; j < t->npark; j++) {
        TrParked *P = &t->park[j]; if (!near2(&c->car, &P->car, .3f)) continue;
        float cv0[3], pv0[3]; memcpy(cv0, c->car.vel, sizeof cv0); memcpy(pv0, P->car.vel, sizeof pv0);
        if (carHitCar(&c->car, &P->car, e->w)) { P->state = 1; P->stillT = 0.f; c->asleep = 0; svHitNote(c, cv0, &P->car, pv0); }
      }
    }
  }
}

// ---------------------------------------------------------------- Гомер: кузова твёрдые, крыша — опора (как у трафика)
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
int mcSolid(const MsnCars *m, float *x, float *z, float y, float r, float *top, float *onx, float *onz) {
  int pushed = 0;
  for (int i = 0; i < m->n; i++) {
    const Car *k = &m->c[i].car; if (!m->c[i].on) continue;
    float dx0 = *x - k->x, dz0 = *z - k->z, br = k->p.halfL + fabsf(k->p.bodyOfs) + .5f + r;
    if (dx0 * dx0 + dz0 * dz0 > br * br) continue;
    float roof = k->y + k->p.bodyHi * cosf(k->pitch) * cosf(k->roll);
    if (roof < y - .3f || k->y + k->p.bodyLo > y + HOMER_HEIGHT) continue;
    float nx, nz, depth;
    if (!boxCircle(k, *x, *z, r, &nx, &nz, &depth)) continue;
    if (roof <= y + HOMER_STEP) { if (roof > *top) *top = roof; continue; }
    *x += nx * depth; *z += nz * depth; *onx = nx; *onz = nz; pushed = 1;
  }
  return pushed;
}
