// Темп соперника гонки m7 на НАСТОЯЩИХ дорогах уровня 1 против радара записи PS2 (задача Инспектора 09.11: racepace/catchup красные).
// Стенд racepace_host.c ставит игрока на путь соперника (прямые по плоской площадке): у PS2 путь Смитерса (race1 -> nav1 -> race1b -> race9,
// 4 точки) короче пути игрока (7 точек), и кинематический игрок на 17 м/с по пути соперника ничего не доказывает. Здесь оба едут по
// дорогам: соперник — ИИ порта (msncar.c, M7race.con, догонялка m7 80/0.5/1.0/1.5), «игрок» — вторая машина ИИ на точках игрока
// (race1, 2, 3, 5, 6, 7, 9) с постоянной желаемой скоростью Vp (первые FOOT с игрок пешком у старта, соперник уже едет). Место
// игрока для догонялки считается теми же mcRacePlaceRob, что в игре (game_msn.c).
//   racereal_host LEVEL.shr TRAFFIC.shr M7RADAR.tsv [Vp] [out.tsv]
// Проверки сравнивают с радаром PS2 (rival_m7.tsv: fwd_m — вперёд/назад от игрока, 4 к/с): доля кадров, где соперник впереди; квартили;
// максимум впереди; финиш игрока ~126 с.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
#include "msncar.h"
static int fails;
static void check(const char *n, int ok, const char *d) { printf("CHECK %-8s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static unsigned char *readFile(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, ((size_t)s + 63) & ~(size_t)63);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (unsigned)s; return b;
}
static ColWorld W; static Traffic T; static MsnCars MR, MP;
static const float RW[4][3] = {{223, 8, -502}, {38, 3, 211}, {-180, -2, 261}, {-183, 4, 110}};   // Смитерс: race1, m5_smithers_nav1, race1b, race9
static const float PW[7][3] = {{223, 8, -502}, {380, -2, -343}, {342, 5, 94}, {164, 3, 269}, {124, 2, 559}, {-116, 3, 486}, {-183, 4, 110}};
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return (x > y) - (x < y); }
static float qt(float *v, int n, float q) { qsort(v, n, sizeof *v, cmpf); return v[(int)(q * (n - 1))]; }
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage\n"); return 2; }
  float Vp = argc > 4 ? (float)atof(argv[4]) : 16.f; const char *outp = argc > 5 ? argv[5] : 0;
  unsigned n; unsigned char *lvl = readFile(argv[1], &n);
  if (!lvl || n < 104 || memcmp(lvl, "SHR7", 4)) { fprintf(stderr, "нет level.shr\n"); return 2; }
  unsigned pv = *(unsigned *)(lvl + 88), co = *(unsigned *)(lvl + 92), po = *(unsigned *)(lvl + 96), ce = n;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, lvl + co, ce - co)) { fprintf(stderr, "COL1\n"); return 2; }
  if (trafficLoad(&T, argv[2])) { fprintf(stderr, "traffic\n"); return 2; }
  float rs[3] = {7.58f, 0.f, -291.85f}, ps[3] = {7.58f, 0.f, -285.f}; float gy, gn[3];
  if (colGround(&W, rs[0], rs[2], 50.f, -50.f, &gy, gn)) rs[1] = gy + .5f;
  if (colGround(&W, ps[0], ps[2], 50.f, -50.f, &gy, gn)) ps[1] = gy + .5f;
  float h0 = atan2f(RW[0][0] - rs[0], RW[0][2] - rs[2]);
  mcInit(&MR); mcInit(&MP);
  McCar *r = mcSpawn(&MR, &W, &T, "smith_v", rs, h0, 1, "race", "Missions\\level01\\M7race.con", "smithers");
  if (!r) { printf("FAIL: машина\n"); return 1; }
  for (int k = 0; k < 4; k++) mcWaypoint(&MR, k, RW[k], 1);
  const float rc[4] = {80.f, .5f, 1.f, 1.5f}; mcCatchup(&MR, "smith_v", 0, rc, 4);
  // путь игрока по дорогам: mcPlan от старта через его 7 точек (те же полосы, что ИИ), ломаная; игрок идёт по ней кинематически с Vp
  static float RT[8192][3]; int nrt = 0; { float prev[3] = {ps[0], ps[1], ps[2]}, ph = h0; static float seg[1024][3];
    for (int i = 0; i < 7; i++) { int off, k = mcPlan(&T, prev[0], prev[1], prev[2], ph, PW[i], 1, seg, 1024, &off);
      for (int j = 0; j < k && nrt < 8190; j++) { if (nrt && hypotf(seg[j][0] - RT[nrt - 1][0], seg[j][2] - RT[nrt - 1][2]) < .5f) continue; memcpy(RT[nrt++], seg[j], 12); }
      if (k >= 2) ph = atan2f(seg[k - 1][0] - seg[k - 2][0], seg[k - 1][2] - seg[k - 2][2]);
      memcpy(prev, PW[i], 12); } }
  static float RL[8192]; RL[0] = 0.f; for (int i = 1; i < nrt; i++) RL[i] = RL[i - 1] + hypotf(RT[i][0] - RT[i - 1][0], RT[i][2] - RT[i - 1][2]);
  float routeLen = RL[nrt - 1]; printf("путь игрока по дорогам %.0f м (%d точек), прямыми между его точками %.0f м\n", routeLen, nrt, 0.f);
  Car pl0; carInit(&pl0, 0, &W, ps[0], ps[1], ps[2], h0);   // «игрок пешком»: стоит у старта FOOT секунд
  pl0.p.topSpeed = 33.3f;   // машина игрока m7: TopSpeed 120 км/ч (нижняя граница догонялки считается от неё)
  float sp = 0.f; int sk = 0;
  const float dt = 1.f / 60.f, FOOT = 2.5f; int pk = 0;
  static float fwd[4000]; int nf = 0, nfar = 0, nall = 0; float maxAhead = -1e9f, leadFin = 1e9f, tFin = -1.f, tRival = -1.f;
  FILE *T2 = outp ? fopen(outp, "w") : 0;
  for (int f = 0; f < 60 * 240; f++) {
    float t = f * dt; int driving = t >= FOOT;
    if (driving) { sp = fminf(routeLen, sp + Vp * dt); while (sk + 2 < nrt && RL[sk + 1] < sp) sk++;
      float u = RL[sk + 1] > RL[sk] ? (sp - RL[sk]) / (RL[sk + 1] - RL[sk]) : 0.f;
      pl0.x = RT[sk][0] + (RT[sk + 1][0] - RT[sk][0]) * u; pl0.z = RT[sk][2] + (RT[sk + 1][2] - RT[sk][2]) * u; pl0.y = RT[sk][1] + (RT[sk + 1][1] - RT[sk][1]) * u;
      pl0.h = atan2f(RT[sk + 1][0] - RT[sk][0], RT[sk + 1][2] - RT[sk][2]); pl0.v = sp < routeLen ? Vp : 0.f;
      pl0.vel[0] = sinf(pl0.h) * pl0.v; pl0.vel[2] = cosf(pl0.h) * pl0.v; pl0.vel[1] = 0.f; }
    Car *pc = &pl0;
    McEnv e; memset(&e, 0, sizeof e); e.w = &W; e.tr = &T; e.player = pc; e.driving = driving; e.ppos[0] = pc->x; e.ppos[1] = pc->y; e.ppos[2] = pc->z;
    mcUpdate(&MR, &e, dt, 0, 0);
    while (pk < 7 && hypotf(pc->x - PW[pk][0], pc->z - PW[pk][2]) < 10.f) pk++;
    float dp; int nr; mcRacePlaceRob(&MR, PW, 7, 1, pk, e.ppos, 0, 0, &dp, &nr);
    float dx = r->car.x - pc->x, dz = r->car.z - pc->z, fw = dx * sinf(pc->h) + dz * cosf(pc->h);
    float dd = hypotf(dx, dz);
    if (driving && f % 15 == 0) { nall++; if (dd > 86.f) nfar++; else if (nf < 4000) fwd[nf++] = fw; }   // радар PS2 видит до ~86 м (край круга)
    if (driving && dd <= 86.f && fw > maxAhead) maxAhead = fw;
    if (r->finished && tRival < 0.f) tRival = t;
    if (pk >= 7 && tFin < 0.f) { tFin = t; leadFin = fw; }
    if (T2 && f % 15 == 0) fprintf(T2, "%.2f\t%.1f\t%.1f\t%.1f\t%.1f\twp%d\tpk%d\trk%d ah%d vdes%.1f f%.2f dp%.0f rd%.0f\n", t, fw, r->car.v, pc->v, hypotf(dx, dz), r->wp, pk, r->rk, r->rAhead, r->vdes, r->catchF, dp, r->rDist);
    if (tFin >= 0.f && t > tFin + 3.f) break;
  }
  if (T2) fclose(T2);
  // радар PS2
  static float ps2[2000]; int np = 0; float psMax = -1e9f; int psAhead = 0;
  { FILE *F = fopen(argv[3], "r"); char ln[200]; if (F) { fgets(ln, sizeof ln, F); while (fgets(ln, sizeof ln, F) && np < 2000) { float t, a, b, c, d, e2; if (sscanf(ln, "%f %f %f %f %f %f", &t, &a, &b, &c, &d, &e2) >= 5) { ps2[np] = d; if (d > psMax) psMax = d; psAhead += d > 0; np++; } } fclose(F); } }
  int ahead = 0; for (int i = 0; i < nf; i++) ahead += fwd[i] > 0;
  float q1 = qt(fwd, nf, .25f), q2 = qt(fwd, nf, .5f), q3 = qt(fwd, nf, .75f);
  float p1 = qt(ps2, np, .25f), p2 = qt(ps2, np, .5f), p3 = qt(ps2, np, .75f);
  char d[300];
  printf("игрок Vp %.1f м/с: финиш игрока %.1f с, соперника %.1f с; кадров %d (4 к/с)\n", Vp, tFin, tRival, nf);
  printf("PS2:  впереди %.0f%%, квартили %.1f / %.1f / %.1f, максимум %.1f\n", 100.f * psAhead / (np ? np : 1), p1, p2, p3, psMax);
  printf("порт: дальше радара (86 м) %.0f%% кадров (PS2 1%%); в круге радара: впереди %.0f%%, квартили %.1f / %.1f / %.1f, максимум %.1f, на финише игрока %.1f\n", 100.f * nfar / (nall ? nall : 1), 100.f * ahead / (nf ? nf : 1), q1, q2, q3, maxAhead, leadFin);
  snprintf(d, sizeof d, "впереди игрока не дальше 60 м (PS2: 53 м): максимум %.1f м", maxAhead); check("ahead", maxAhead <= 60.f, d);
  snprintf(d, sizeof d, "соперник впереди не больше трети времени (PS2 19%%): %.0f%%", 100.f * ahead / (nf ? nf : 1)); check("share", nf && ahead * 3 <= nf, d);
  snprintf(d, sizeof d, "медиана позади в пределах 10..40 м (PS2 %.0f): %.1f м", p2, q2); check("median", q2 <= -5.f && q2 >= -40.f, d);
  snprintf(d, sizeof d, "на финише игрока соперник не впереди (PS2: ~20 м позади): %.1f м", leadFin); check("finish", tFin > 0.f && leadFin <= 0.f, d);
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
