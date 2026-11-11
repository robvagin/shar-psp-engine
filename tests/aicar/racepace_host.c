// Аудит 09.10 п. 10 (правка 347, В42/В91): темп соперника гонки против записи PS2.
// m7 The Fat and Furious, запись yt-bziOipI4gDI 39:53–42:02 (радар: стрелка Смитерса относительно игрока, 4 к/с, 1,65 м на пиксель
// видео; tools/research/race10/rival_radar.py): Смитерс впереди игрока не дальше 53 м (старт, игрок ещё садится), с 18-й секунды
// позади: квартили отставания 8–39 м, дальше 86 м — 1% кадров, на финише ~20 м позади. Средний темп игрока PS2 не меньше 16,8 м/с
// (126 с на 2118 м по прямой между его контрольными точками m7).
// Стенд: ровная площадка, путь Смитерса m7 по его точкам с диска (m7_smithers_carstart -> race1 -> m5_smithers_nav1 -> race1b ->
// race9), машина M7race.con, догонялка m7 80 / 0.5 / 1.0 / 1.5. Игрок: первые 2,5 с пешком у старта (как Гомер на записи), потом
// машина игрока идёт по тому же пути ровно 17 м/с (нижняя граница темпа PS2; кинематически — проверяется соперник, не игрок).
// 09.11 (Кольцо-96): ГЕЙТ КРАСНЫЙ И НА ЗЕЛЁНОМ КОДЕ, и это не «соперник быстрее оригинала». Пока игрок едет 17 м/с, желаемая скорость соперника по
// оригиналу (догонялка соперника в оригинале, сверка g03 02.11) не ниже 0,5 × TopSpeed МАШИНЫ ИГРОКА 16,7 + 1,39 («+5 км/ч» до отсечки газа) = 18,1 м/с > 17:
// соперник впереди расходится на 1,1 м/с при любой правильной реализации. Игрок 17 м/с — нижняя граница темпа PS2 по прямой, а дорогами тот же заезд
// ~3200 м (tests/aicar/racereal_host.c: Смитерс + игрок по настоящим дорогам L1, радар PS2 из rival_radar.py). Здесь игрок идёт по ЛОМАНОЙ СОПЕРНИКА
// на плоскости. Не ставить пороги по этому стенду; см. racereal_host.c.
// Проверки (числа записи PS2): впереди не дальше 60 м; после первого обгона дальше 90 м позади — не больше 5% времени; на финише
// игрока соперник не впереди. Красный (tests/aicar/racepace.sh): -DMC_CATCHUP_SPEED_OLD=1 (множитель к своей TopSpeed) обязан упасть.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "car.h"
#include "msncar.h"
static int fails;
static void check(const char *n, int ok, const char *d) { printf("CHECK %-8s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } fx;
static ColWorld W;
static MsnCars M;
#define NP 5
static const float P[NP][3] = {{7.58f, 0.f, -291.85f}, {222.51f, 0.f, -501.68f}, {38.39f, 0.f, 210.67f}, {-179.87f, 0.f, 261.43f},
                               {-183.42f, 0.f, 109.84f}};   // диск: art/missions/level01/m7.p3d (y — ровная площадка)
static float L[NP];   // путь от старта до точки
static void at(float s, float *x, float *z, float *h) {   // точка пути на расстоянии s
  int k = 1; while (k < NP - 1 && L[k] < s) k++;
  float a = L[k] - L[k - 1], u = a > 0 ? fminf(1.f, fmaxf(0.f, (s - L[k - 1]) / a)) : 0.f;
  *x = P[k - 1][0] + (P[k][0] - P[k - 1][0]) * u; *z = P[k - 1][2] + (P[k][2] - P[k - 1][2]) * u;
  *h = atan2f(P[k][0] - P[k - 1][0], P[k][2] - P[k - 1][2]);
}
static float proj(float x, float z, int seg0) {   // пройдено по пути до проекции точки (на отрезках от seg0)
  float best = 1e30f, s = 0.f;
  for (int k = seg0 > 1 ? seg0 : 1; k < NP; k++) {
    float ax = P[k][0] - P[k - 1][0], az = P[k][2] - P[k - 1][2], l2 = ax * ax + az * az;
    float u = fminf(1.f, fmaxf(0.f, ((x - P[k - 1][0]) * ax + (z - P[k - 1][2]) * az) / l2));
    float qx = P[k - 1][0] + ax * u - x, qz = P[k - 1][2] + az * u - z, d = qx * qx + qz * qz;
    if (d < best) { best = d; s = L[k - 1] + u * sqrtf(l2); }
  }
  return s;
}
int main(int argc, char **argv) {
  const char *tsv = argc > 1 ? argv[1] : 0; FILE *T = tsv ? fopen(tsv, "w") : 0;
  float V = argc > 2 ? (float)atof(argv[2]) : 17.f;
  memcpy(fx.h.magic, "COL1", 4); fx.h.ox = fx.h.oz = -800; fx.h.cell = 1600; fx.h.W = fx.h.H = 1; fx.h.ntri = 2; fx.h.nref = 2;
  fx.t[0] = (ColTri){{-800, 0, -800}, {800, 0, 800}, {800, 0, -800}, {0, 1, 0}};
  fx.t[1] = (ColTri){{-800, 0, -800}, {-800, 0, 800}, {800, 0, 800}, {0, 1, 0}};
  fx.start[1] = 2; fx.refs[0] = 0; fx.refs[1] = 1;
  if (colInit(&W, &fx, sizeof fx)) { printf("FAIL: мир\n"); return 1; }
  L[0] = 0.f; for (int k = 1; k < NP; k++) L[k] = L[k - 1] + hypotf(P[k][0] - P[k - 1][0], P[k][2] - P[k - 1][2]);
  mcInit(&M);
  McCar *c = mcSpawn(&M, &W, 0, "smith_v", P[0], atan2f(P[1][0] - P[0][0], P[1][2] - P[0][2]), 1, "race", "Missions\\level01\\M7race.con", "smithers");
  if (!c) { printf("FAIL: машина\n"); return 1; }
  for (int k = 1; k < NP; k++) mcWaypoint(&M, k - 1, P[k], 1);
  const float rc[4] = {80.f, .5f, 1.f, 1.5f}; mcCatchup(&M, "smith_v", 0, rc, 4);
  float px, pz, ph; at(0.f, &px, &pz, &ph);
  Car pc; carInit(&pc, 0, &W, px, 0.f, pz, ph);
  const float dt = 1.f / 60.f, FOOT = 2.5f, FIN = 10.f;   // FIN: финиш засчитан ближе 10 м — как у соперника (msncar.c WP_R)
  float tFin = -1.f, s = 0.f, maxAhead = -1e9f, behindLong = 0.f, after = 0.f, leadEnd = 0.f, tEnd = -1.f; int passed = 0;
  for (int f = 0; f < 60 * 400; f++) {
    float t = f * dt; int driving = t >= FOOT;
    if (driving) s = fminf(L[NP - 1], s + V * dt);
    at(s, &px, &pz, &ph); pc.x = px; pc.z = pz; pc.h = ph; pc.v = driving && s < L[NP - 1] ? V : 0.f;
    McEnv e; memset(&e, 0, sizeof e); e.w = &W; e.player = &pc; e.driving = driving; e.ppos[0] = px; e.ppos[2] = pz;
    e.haveFrac = 1; e.pfrac = s / L[NP - 1];
    mcUpdate(&M, &e, dt, 0, 0);
    if (c->finished && tFin < 0.f) tFin = t;
    // > 0 — соперник впереди; финишировал раньше игрока — впереди на путь игрока с тех пор
    float sc = c->finished ? L[NP - 1] - FIN + V * (t - tFin) : proj(c->car.x, c->car.z, c->wp), lead = sc - s;
    if (lead > maxAhead) maxAhead = lead;
    if (driving && lead < 0.f) passed = 1;
    if (passed) { after += dt; if (lead < -90.f) behindLong += dt; }
    if (T && f % 15 == 0) fprintf(T, "%.2f\t%.1f\t%.1f\t%.1f\t%.2f\t%.1f\n", t, s, sc, lead, c->mult, c->car.v);
    if (s >= L[NP - 1] - FIN && tEnd < 0.f) { tEnd = t; leadEnd = lead; }
    if (tEnd >= 0.f) break;
  }
  if (T) fclose(T);
  char d[240];
  printf("путь %.0f м, игрок %.1f м/с, финиш игрока %.1f с\n", L[NP - 1], V, tEnd);
  snprintf(d, sizeof d, "соперник впереди игрока не дальше 60 м (PS2: 53 м): максимум %.1f м", maxAhead);
  check("ahead", maxAhead <= 60.f, d);
  snprintf(d, sizeof d, "после обгона позади дальше 90 м не больше 5%% времени (PS2: 1%%): %.0f%% из %.1f с", after > 0 ? 100.f * behindLong / after : 100.f, after);
  check("tail", passed && behindLong <= .05f * after, d);
  snprintf(d, sizeof d, "на финише игрока соперник не впереди (PS2: ~20 м позади): %.1f м", leadEnd);
  check("finish", tEnd > 0.f && leadEnd <= 0.f, d);
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
