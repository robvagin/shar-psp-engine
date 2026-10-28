// Машины миссий с ИИ: что и по каким данным — viewer/msncar.h. Чистый C99, без PSP и malloc.
#include <string.h>
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
// Диск: комментарий к SetStageAIRaceCatchupParams в scripts/missions/level01/m7i.mfk:40-45 называет поля
// FRACTIONPLAYERSPEED_MINCATCHUP / _NOCATCHUP / _MAXCATCHUP. Запись PS2 yt-bziOipI4gDI (радар, стрелка соперника; 4 к/с, 1,65 м на
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
static int roadAllowed(const Traffic *t, int r, int shortcuts) { return !(t->road[r].flags & 1) || shortcuts; }
// ближайшая к (x, z) точка оси полосы; maxAng >= 0 — курс полосы в пределах ±maxAng от h. -> 1 и дорога, полоса, s, d2
// y < -1e8 — высота любая; иначе полоса не дальше LANE_DY по высоте (эстакада над улицей — не та дорога)
#define LANE_DY 3.5f
#define MC_MAXROAD 256
extern int mcRoadPlanOld;
static const float *rpBoxes(const Traffic *t);   // рамки дорог (rpEnsure): 4 массива по MC_MAXROAD (x0, x1, z0, z1); NULL — нет
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
        if (d2 >= best || (maxAng >= 0.f && fabsf(wrapPi(atan2f(ax, az) - h)) > maxAng)) continue;
        if (y > -1e8f && fabsf(P[i].y + (P[i + 1].y - P[i].y) * u - y) > LANE_DY) continue;
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
#define WRONGWAY 1.15f
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
static float ledgeCost(int r, int d) { return !mcLedgeOld && r < 256 && (roadLedge[r] >> d & 1) ? BLOCKED : 0.f; }
// кусок дороги r от s0 по направлению d (до конца, если s1 < 0 — иначе до s1) проходит уступ вверх?
static float ledgeCostPart(int r, int d, float s0, float s1) {
  if (mcLedgeOld || r >= 256 || !(roadLedge[r] >> d & 1)) return 0.f;
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
void mcPlanAvoid(const short *r, int n) { avoidR = r; avoidN = n; }   // служебное (гейт tests/aicar/roadplan.c): список объездов для mcPlan
int mcPlan(const Traffic *t, float x, float y, float z, float h, const float g[3], int shortcuts, float out[][3], int cap, int *offroad) {
  int dummy; if (!offroad) offroad = &dummy;
  *offroad = 0;
  if (cap < 2) return 0;
  if (!roadsOk(t)) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int rg, lg, r0, l0; float sg, s0, dg;
  // начало и конец — с любой дороги (машина стоит на стоянке-срезе у дома — выезжает по нему); запрет срезов — в пути
  if (!nearestLane(t, g[0], g[1], g[2], 0.f, -1.f, LANE_SNAP * LANE_SNAP, 1, &rg, &lg, &sg, &dg)) {
    *offroad = 1; out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  if (d2xz(g, x, z) < CLOSE_DIRECT * CLOSE_DIRECT) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  // начало — ближняя полоса обычной дороги (не среза: срезы уровня 1 идут дворами, 5 из них перекрыты коллизией), потом любая
  if (!nearestLane(t, x, y, z, h, -1.f, 12.f * 12.f, 0, &r0, &l0, &s0, 0) &&
      !nearestLane(t, x, y, z, h, -1.f, 12.f * 12.f, shortcuts, &r0, &l0, &s0, 0) &&
      !nearestLane(t, x, y, z, h, -1.f, LANE_SNAP * LANE_SNAP, 1, &r0, &l0, &s0, 0)) {
    out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int path[64], np = planRoads(t, r0, l0, s0, h, rg, sg, shortcuts, path, 64);
  for (int it = 0; it < 3 && np && probePath(t, path, np); it++) np = planRoads(t, r0, l0, s0, h, rg, sg, shortcuts, path, 64);   // перекрытая — в объезд
  if (!np) { out[0][0] = g[0]; out[0][1] = g[1]; out[0][2] = g[2]; return 1; }
  int n = 0, lane = l0; float last[3] = {x, y, z};
  { float p[3]; lanePos(t, r0, l0, s0, p); n = emit(out, n, cap, p[0], p[1], p[2]); memcpy(last, p, sizeof last); }   // с проекции машины на полосу
  for (int i = 0; i < np && n < cap; i++) {
    int r = path[i] >> 1, d = path[i] & 1; const TrRoad *R = &t->road[r];
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
static void resetPath(McCar *c) { c->wp = 0; c->passed = -1; c->finished = 0; c->nroute = 0; c->seg = 0; c->routeWp = -1; c->replanT = 0.f; c->stuckN = 0; c->progWp = -2; c->progT = 0.f;
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
  c->baseTop = P.topSpeed; c->baseGas = P.gasScale; c->mult = 1.f;
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
  c->nroute = mcPlan(e->tr, c->car.x, c->car.y, c->car.z, c->car.h, g, sc, c->route, MC_ROUTE, &c->offroad);
  avoidN = 0; probeW = 0; c->seg = 0; c->replans++;
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
  // цель этого кадра: точка пути или игрок
  float goal[3]; int have = 0;
  if (c->mode == MC_CHASE) {
    goal[0] = e->ppos[0]; goal[1] = e->ppos[1]; goal[2] = e->ppos[2]; have = 1;
    if (e->player && e->driving) { goal[0] += e->player->vel[0] * .5f; goal[2] += e->player->vel[2] * .5f; }
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
    if (c->police && !POLICE_CHASE_OLD && !mcPolice723Old && have && e->player && e->driving && hypotf(e->player->vel[0], e->player->vel[2]) < POLICE_STAND_V) {   // №723: к стоящему не ближе 27 м
      float dx = k->x - e->ppos[0], dz = k->z - e->ppos[2], l = sqrtf(dx * dx + dz * dz) + 1e-3f, close = -(k->vel[0] * dx + k->vel[2] * dz) / l;
      if (l - (close > 0.f ? close * close / 16.f + close * .25f : 0.f) < POLICE_PS2_STAND) have = 0;
    }
  } else if (m->nwp && !c->finished) {
    int T = total(m);
    while (c->wp < T && d2xz(wpAt(m, c->wp), k->x, k->z) < WP_R * WP_R) { c->passed = c->wp; c->wp++; c->nroute = 0; }
    if (c->wp >= T) c->finished = 1;
    else { memcpy(goal, wpAt(m, c->wp), sizeof goal); have = 1; }
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
  // маршрут: новая цель, конец обрезанного пути, погоня — раз в секунду
  c->replanT -= dt;
  int urgent = c->nroute == 0 || c->routeWp != c->wp;   // нет пути / новая цель — сразу
  int need = urgent || (c->routeCut && c->seg >= c->nroute - 3) || (c->mode == MC_CHASE && c->replanT <= 0.f);
  // Очередь перепланировок (ресёрч процессора 10.10): у погонь 3–4 ИИ-машин перепланировка «раз в секунду» совпадает во времени у всех,
  // и один кадр получает 3–4 поиска подряд. Плановая (не срочная) — не больше одной за кадр на все машины; остальные — в следующих
  // кадрах (replanT остаётся ≤ 0, машина едет по прежнему пути). -DROADPLAN_QUEUE_OLD=1 или -DROADPLAN_OLD=1 — как было
  if (need && !urgent && !ROADPLAN_QUEUE_OLD && !mcRoadPlanOld && replanBudget <= 0) need = 0;
  // №537 (11.10): первый маршрут всех машин гонки/погони строился в один кадр (отмашка «GO»): 3–4 поиска по дорогам подряд, кадр логики 18 мс против 6 (тур 12, P3 L3 sr3).
  // Первый маршрут машины после resetPath (routeWp -1) — не больше одного поиска за кадр на все машины: остальные стоят на ручнике ещё кадр-два (≤ 50 мс), маршрут тот же,
  // строится на следующем кадре с того же места. Срочные перепланировки в пути (новая цель, обрезанный путь) — сразу, как было. -DMCAR_WARM_OLD=1 — как было
  if (!MCAR_WARM_OLD && urgent && c->routeWp == -1 && c->nroute == 0 && !mcRoadPlanOld && replanBudget <= 0) { in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f; carUpdate(k, e->w, &in, dt); c->lastIn = in; return; }
  if (need) { if (!urgent || (!MCAR_WARM_OLD && c->routeWp == -1)) replanBudget--; replan(c, m, e, goal); c->routeWp = c->wp; c->replanT = 1.f; }
  // догонялка
  float mult = 1.f;
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
  control(m, c, e, goal, dt, &in);
  // ПРАВИЛО ПОРТА: урон машине миссии — только от машин (carHitCar в mcUpdate); удар о стену и землю не бьёт — иначе коробки
  // коллизии поперёк полос (пункт 11 очереди: 7 мест) разбивают соперника гонки и цель погони раньше игрока
  float dmg0 = k->damage, dead0 = k->deadT; int des0 = k->destroyed;
  c->still = 0.f;   // едет: счёт покоя на отсчёте (RACE_START_LIGHT_OLD) заново
  { unsigned pu_ = mcClockUs ? mcClockUs() : 0u; carUpdate(k, e->w, &in, dt); if (mcClockUs) mcPartMs[0] += (mcClockUs() - pu_) / 1000.f; } c->lastIn = in;
  k->damage = dmg0; k->destroyed = des0; if (!des0) k->deadT = dead0;   // правка 199: часы взрыва идут только у разбитой
  // стоит на месте 10 с (сместилась меньше 8 м от точки отсчёта: упёрлась, качается у стены) / перевёрнута, а игрок
  // далеко — на свой путь впереди (правило порта, вне вида игрока). Перепланировка маршрута ход не обнуляет
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
  k->p.topSpeed = top; k->p.gasScale = c->baseGas;   // параметры .con — как в данных (множитель — только на кадр)
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
  replanBudget = 1;   // плановых перепланировок на кадр (см. очередь в drive)
  for (int i = 0; i < m->n; i++) {
    McCar *c = &m->c[i]; if (!c->on) continue;
    Car *k = &c->car;
    int moving = c->mode != MC_PARKED && !(c->finished && fabsf(k->v) < .3f) && !k->destroyed;
    if (moving) { c->asleep = 0; unsigned pd_ = mcClockUs ? mcClockUs() : 0u; drive(m, c, e, dt); if (mcClockUs) { mcPartMs[1] += (mcClockUs() - pd_) / 1000.f; mcPartMs[2] += 1.f; } }
    else if (!c->asleep) {   // стоит / доехала / разбита: физика до покоя, потом спит (как стоящие машины трафика)
      CarInput in = {0.f, fabsf(k->v) > .5f ? 1.f : 0.f, 0.f, 1.f}; carUpdate(k, e->w, &in, dt);
      float wv = fabsf(k->w[0]) + fabsf(k->w[1]) + fabsf(k->w[2]);
      c->still = (fabsf(k->v) < .2f && fabsf(k->vl) < .2f && wv < .2f) ? c->still + dt : 0.f;
      if (c->still > 1.f) c->asleep = 1;
    }
    c->wheelAng += k->v / fmaxf(.2f, k->p.wheelRad) * dt;
  }
  // удары: игрок, машины миссии между собой, трафик и стоящие
  for (int i = 0; i < m->n; i++) {
    McCar *c = &m->c[i]; if (!c->on) continue;
    if (e->player && e->driving && near2(e->player, &c->car, .5f)) {
      float before = e->player->hitSpeed;
      if (carHitCar(e->player, &c->car, e->w)) {
        c->asleep = 0; c->still = 0.f;
        if (e->player->hitSpeed > 1.f && e->player->hitSpeed > before - 1e-4f) { m->hits++; if (hit) hit(u, c->name); }
      }
    }
    for (int j = i + 1; j < m->n; j++) {
      if (e->noNpcCollide) continue;
      McCar *o = &m->c[j]; if (!o->on || !near2(&c->car, &o->car, .3f)) continue;
      if (carHitCar(&c->car, &o->car, e->w)) { c->asleep = o->asleep = 0; c->still = o->still = 0.f; }
    }
    if (e->tr && e->tr->hd) {
      Traffic *t = e->tr;
      for (int j = 0; j < t->maxCars; j++) {
        TrCar *tc = &t->car[j]; if (tc->mode == TRM_OFF || !near2(&c->car, &tc->car, .3f)) continue;
        if (carHitCar(&c->car, &tc->car, e->w)) { if (tc->mode == TRM_DRIVE) { tc->mode = TRM_PHYS; tc->stillT = 0.f; } c->asleep = 0; }
      }
      for (int j = 0; j < t->npark; j++) {
        TrParked *P = &t->park[j]; if (!near2(&c->car, &P->car, .3f)) continue;
        if (carHitCar(&c->car, &P->car, e->w)) { P->state = 1; P->stillT = 0.f; c->asleep = 0; }
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
