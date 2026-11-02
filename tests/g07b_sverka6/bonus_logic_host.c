// Окно g07b-msnview, круг 6: логика бонус-гонки на настоящих трассах диска (build/bonus/b0N.bns), настоящий viewer/bonus.c.
// Строки: P17-14 / P07:17 (3 заряда, гудок +10 м/с, пауза 0,5 с), P17-18 / P07:18 / P17-18 (возврат нитро 15 с и ключа 30 с, подбор нитро = +1 заряд),
// P04 B1 (ИИ: не раньше 10 с после старта, затем не чаще раза в 10 с, путь в пределах 10° от курса, решение раз в 2 с), P17-20 (на «GO!» до ИИ−1 машин с шансом 3/7),
// P17-17 (ловушки переключает только лидер: первый датчик — вперёд, второй — назад; при обратном ходе наоборот), P17 Д3 (событие 63 не подбрасывает).
// Красные контроли: -DBR_NITRO3_OLD=1 -DBR_AIHORN3_OLD=1 -DBR_RESPAWN_KIND_OLD=1 -DBR_TRAP_LEADER_OLD=1 -DBR_LAUNCH_OLD=1 — каждый обязан уронить свои проверки.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bonus.h"

static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static unsigned char *slurp(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f);
  unsigned char *d = malloc((size_t)s); if (!d || fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); return 0; } fclose(f); *n = (unsigned)s; return d;
}
static BnsTrack T[8]; static unsigned char *D[8];
static void raceStart(BonusRace *b, const BnsTrack *t, int laps, int reverse, int n, int human) {   // инициализация и выход из отсчёта «3 2 1»
  brInit(b, t, laps, reverse, n, human);
  float pos[BR_MAX][3]; for (int i = 0; i < BR_MAX; i++) brStart(b, i < n ? i : 0, pos[i], &(float){0});
  brUpdate(b, (const float (*)[3])pos, BR_COUNT_S + .01f);
}
static void spot(const BonusRace *b, int i, Car *k) { float h; float p[3]; brStart(b, i, p, &h); memset(k, 0, sizeof *k); k->x = p[0]; k->y = p[1]; k->z = p[2]; k->h = h; }

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: bonus_logic_host build/bonus\n"); return 2; }
  char path[512], d[256];
  for (int n = 1; n <= 7; n++) {
    snprintf(path, sizeof path, "%s/b0%d.bns", argv[1], n); unsigned sz;
    D[n] = slurp(path, &sz); if (!D[n] || bnsParse(&T[n], D[n], sz)) { fprintf(stderr, "нет/плохой %s\n", path); return 2; }
  }
  // ---- 1. три заряда в начале заезда, гудок: +10 м/с вдоль курса, пауза 0,5 с, заряд тратится
  { BonusRace b; raceStart(&b, &T[1], 1, 0, 4, 0); Car k; spot(&b, 0, &k);
    int c0 = b.nitro[0]; snprintf(d, sizeof d, "зарядов на старте %d (ждём 3)", c0); check("charges3", c0 == 3 && b.nitro[3] == 3, d);
    float vx = k.vel[0], vz = k.vel[2]; int h1 = brHorn(&b, 0, &k);
    float dv = (k.vel[0] - vx) * sinf(k.h) + (k.vel[2] - vz) * cosf(k.h);
    snprintf(d, sizeof d, "гудок: сработал %d, прибавка вдоль курса %.2f м/с (ждём 10), зарядов %d", h1, dv, b.nitro[0]); check("horn_kick", h1 && fabsf(dv - 10.f) < .01f && b.nitro[0] == 2, d);
    int h2 = brHorn(&b, 0, &k); snprintf(d, sizeof d, "повтор сразу: %d (ждём 0)", h2); check("horn_gap0", !h2, d);
    float pos[BR_MAX][3]; for (int i = 0; i < BR_MAX; i++) brStart(&b, 0, pos[i], &(float){0});
    brUpdate(&b, (const float (*)[3])pos, .49f); int h3 = brHorn(&b, 0, &k);
    snprintf(d, sizeof d, "через 0,49 с: %d (ждём 0, пауза 0,5 с)", h3); check("horn_gap049", !h3, d);
    brUpdate(&b, (const float (*)[3])pos, .03f); int h4 = brHorn(&b, 0, &k);
    snprintf(d, sizeof d, "через 0,52 с: %d (ждём 1), зарядов %d", h4, b.nitro[0]); check("horn_gap052", h4 && b.nitro[0] == 1, d);
    b.nitroGap[0] = 0; brHorn(&b, 0, &k); b.nitroGap[0] = 0; int h5 = brHorn(&b, 0, &k);
    snprintf(d, sizeof d, "заряды кончились: гудок %d (ждём 0)", h5); check("horn_empty", !h5 && b.nitro[0] == 0, d);
    BonusRace c; brInit(&c, &T[1], 1, 0, 4, 0); Car k2; spot(&c, 0, &k2); int h6 = brHorn(&c, 0, &k2);
    snprintf(d, sizeof d, "в отсчёте «3 2 1» гудок %d (ждём 0)", h6); check("horn_count", !h6, d); }
  // ---- 2. подборы: нитро +1 заряд и возврат 15 с, ключ — починка и возврат 30 с
  { int tn = 0, tw = 0, qn = -1, qw = -1;
    for (int n = 1; n <= 7; n++) for (unsigned q = 0; q < T[n].npick; q++) { if (T[n].pk[q].kind == BNS_PK_NITRO && !tn) { tn = n; qn = (int)q; } if (T[n].pk[q].kind == BNS_PK_WRENCH && !tw) { tw = n; qw = (int)q; } }
    snprintf(d, sizeof d, "в данных есть нитро (b0%d) и ключ (b0%d)", tn, tw); check("pick_data", tn && tw, d);
    if (tn) { BonusRace b; raceStart(&b, &T[tn], 1, 0, 4, 0); Car k; spot(&b, 0, &k); const BnsPick *P = &T[tn].pk[qn]; k.x = P->p[0]; k.y = P->p[1]; k.z = P->p[2];
      int got = brPick(&b, 0, &k); snprintf(d, sizeof d, "взяли подбор %d (ждём %d), зарядов %d (ждём 4: 3 + 1)", got, qn, b.nitro[0]); check("nitro_plus1", got == qn && b.nitro[0] == 4, d);
      brPickTick(&b, 14.9f); int still = !brPickShown(&b, qn); brPickTick(&b, .2f); int back = brPickShown(&b, qn);
      snprintf(d, sizeof d, "нитро: на 14,9 с ещё нет (%d), на 15,1 с вернулся (%d)", still, back); check("nitro_15s", still && back, d); }
    if (tw) { BonusRace b; raceStart(&b, &T[tw], 1, 0, 4, 0); Car k; spot(&b, 0, &k); const BnsPick *P = &T[tw].pk[qw]; k.x = P->p[0]; k.y = P->p[1]; k.z = P->p[2]; k.damage = .8f;
      int got = brPick(&b, 0, &k); int fixed = k.damage == 0.f; brPickTick(&b, 29.9f); int still = !brPickShown(&b, qw); brPickTick(&b, .2f); int back = brPickShown(&b, qw);
      snprintf(d, sizeof d, "ключ: взят %d, починил %d, на 29,9 с ещё нет (%d), на 30,1 с вернулся (%d)", got == qw, fixed, still, back); check("wrench_30s", got == qw && fixed && still && back, d); } }
  // ---- 3. ИИ: гудок не раньше 10 с после старта, решение раз в 2 с, затем не чаще раза в 10 с, угол к пути ≤ 10°
  { BonusRace b; raceStart(&b, &T[1], 1, 0, 4, 0); Car k; spot(&b, 1, &k);
    float h0; { float p[3]; brStart(&b, 1, p, &h0); }
    // курс пути рядом со стартом: берём курс, при котором гудит (подбор по кругу не нужен: путь у старта идёт по курсу старта в пределах ~10°)
    float first = -1, second = -1; int horns = 0; float t = 0;
    for (int s = 0; s < 3000 && horns < 2; s++) { t += .02f; b.time = t; if (brAiHorn(&b, 1, &k, .02f)) { if (!horns) first = t; else second = t; horns++; b.nitroGap[1] = 0; } }
    snprintf(d, sizeof d, "первый гудок ИИ на %.2f с (ждём ≥ 10 и ≤ 12), второй на %.2f с (ждём ≥ первый + 10)", first, second);
    check("ai_first10", first >= 10.f - .03f && first <= 12.1f, d); check("ai_every10", second >= first + 10.f - .03f, d);
    BonusRace c; raceStart(&c, &T[1], 1, 0, 4, 0); Car k3; spot(&c, 1, &k3); float hh = k3.h; int early = 0;
    for (int s = 0; s < 450; s++) early += brAiHorn(&c, 1, &k3, .02f);   // 9 с
    snprintf(d, sizeof d, "за первые 9 с гудков %d (ждём 0)", early); check("ai_early", early == 0, d);
    (void)hh;
    BonusRace e; raceStart(&e, &T[1], 1, 0, 4, 0); Car k4; spot(&e, 1, &k4); k4.h += 30.f * 3.14159265f / 180.f; int off = 0;
    for (int s = 0; s < 1500; s++) off += brAiHorn(&e, 1, &k4, .02f);   // 30 с, курс в стороне от пути на 30°
    snprintf(d, sizeof d, "курс на 30° в стороне от пути: гудков %d (ждём 0)", off); check("ai_angle", off == 0, d);
    // решение раз в 2 с: после 10 с заряд есть, курс верный — гудок приходит не позже чем через 2 с, а не «в тот же кадр» после 10-й секунды у всех подряд
    BonusRace g; raceStart(&g, &T[1], 1, 0, 4, 0); Car k5; spot(&g, 1, &k5); int cnt = 0, lastAt = -1;
    for (int s = 0; s < 800; s++) { int hh2 = brAiHorn(&g, 1, &k5, .02f); if (hh2) { cnt++; lastAt = s; g.nitroGap[1] = 0; } }
    snprintf(d, sizeof d, "за 16 с гудков %d (ждём 1: на ~10–12 с; второй не раньше 20 с), последний в кадре %d", cnt, lastAt); check("ai_once16", cnt == 1, d); }
  // ---- 4. «GO!»: до ИИ−1 машин тратят заряд, каждая с шансом 3/7; никто не взял — одна случайная (ИИ больше одного)
  { int totalUsed = 0, never = 0, maxUsed = 0, trials = 200;
    for (int s = 0; s < trials; s++) { BonusRace b; raceStart(&b, &T[1], 1, 0, 4, 0); b.rng ^= (uint32_t)s * 2246822519u; Car ks[4]; Car *kp[4]; for (int i = 0; i < 4; i++) { spot(&b, i, &ks[i]); kp[i] = &ks[i]; }
      int u = brGoNitro(&b, kp); totalUsed += u; if (u > maxUsed) maxUsed = u; if (!u) never++; }
    snprintf(d, sizeof d, "200 стартов: ни разу без гудка %d (ждём 0), максимум %d (≤ 2 = ИИ−1), среднее %.2f (3 ИИ с шансом 3/7 ≈ 1,0–1,3)", never, maxUsed, totalUsed / (float)trials);
    check("go_nitro", never == 0 && maxUsed <= 2 && totalUsed / (float)trials > .8f && totalUsed / (float)trials < 1.6f, d); }
  { int used1 = 0; for (int s = 0; s < 200; s++) { BonusRace b; raceStart(&b, &T[1], 1, 0, 2, 0); b.rng ^= (uint32_t)s * 2246822519u; Car ks[2]; Car *kp[2]; for (int i = 0; i < 2; i++) { spot(&b, i, &ks[i]); kp[i] = &ks[i]; } used1 += brGoNitro(&b, kp); }
    snprintf(d, sizeof d, "человек + один ИИ, 200 стартов: гудков на «GO!» %d (ждём 0: до ИИ−1 = 0 машин)", used1); check("go_one_ai", used1 == 0, d); }
  // ---- 5. ловушки: только лидер; первый датчик — вперёд, второй — назад; обратный ход меняет датчики
  for (int rev = 0; rev < 2; rev++) { int n = 4; if (T[n].nhaz < 2) continue;
    BonusRace b; raceStart(&b, &T[n], 1, rev, 4, 0); b.ntrap = 1; b.trap[0].state = BR_TRAP_IDLE; b.trap[0].fps = 30.f; b.trap[0].last = 30.f;
    int sens[BNS_MAXHAZ], ns = 0; for (unsigned q = 0; q < T[n].nhaz; q++) if (T[n].hz[q].kind == BNS_HZ_TRAP) sens[ns++] = (int)q;
    if (ns < 2) { check("trap_data", 0, "нет двух датчиков ловушки на b04"); continue; }
    float pos[BR_MAX][3]; for (int i = 0; i < BR_MAX; i++) { pos[i][0] = pos[i][1] = pos[i][2] = 1e6f; }
    for (int i = 0; i < 4; i++) b.r[i].place = i + 1;   // лидер — машина 0
    int first = rev ? 1 : 0, second = rev ? 0 : 1;
    // не лидер (машина 1) входит в «первый по ходу» датчик — ничего
    memcpy(pos[1], T[n].hz[sens[first]].c, 12); brTrapStep(&b, (const float (*)[3])pos, 4, .02f);
    snprintf(d, sizeof d, "ход %s: не лидер в первом датчике — ловушка %d (ждём 0 = стоит)", rev ? "обратный" : "прямой", b.trap[0].state); check("trap_nonleader", b.trap[0].state == BR_TRAP_IDLE, d);
    pos[1][0] = pos[1][1] = pos[1][2] = 1e6f;
    memcpy(pos[0], T[n].hz[sens[first]].c, 12); brTrapStep(&b, (const float (*)[3])pos, 4, .02f);
    snprintf(d, sizeof d, "лидер в первом датчике — ловушка пошла вперёд (%d, ждём %d)", b.trap[0].state, BR_TRAP_RISE); check("trap_lead_in", b.trap[0].state == BR_TRAP_RISE, d);
    for (int s = 0; s < 200; s++) brTrapStep(&b, (const float (*)[3])pos, 4, .02f);   // уходит наверх и держится без выдержки
    snprintf(d, sizeof d, "лидер остался в первом: наверху и держится (%d, ждём %d)", b.trap[0].state, BR_TRAP_HOLD); check("trap_hold", b.trap[0].state == BR_TRAP_HOLD, d);
    memcpy(pos[0], T[n].hz[sens[second]].c, 12); brTrapStep(&b, (const float (*)[3])pos, 4, .02f);
    snprintf(d, sizeof d, "лидер во втором датчике — назад (%d, ждём %d)", b.trap[0].state, BR_TRAP_LOWER); check("trap_lead_out", b.trap[0].state == BR_TRAP_LOWER, d); }
  // ---- 6. событие 63 (B04G): поршни никого не подбрасывают
  { BonusRace b; raceStart(&b, &T[4], 1, 0, 4, 0); b.ntrap = 1; b.trap[0].state = BR_TRAP_RISE; Car k; spot(&b, 0, &k); int fired = 0;
    for (unsigned q = 0; q < T[4].nhaz; q++) if (T[4].hz[q].kind == BNS_HZ_LAUNCH) { k.x = T[4].hz[q].c[0]; k.y = T[4].hz[q].c[1]; k.z = T[4].hz[q].c[2]; k.vel[1] = 0; fired += brLaunch(&b, 0, &k); }
    snprintf(d, sizeof d, "в объёмах подброса поршней сработало %d (ждём 0), вертикальная скорость %.1f", fired, k.vel[1]); check("no_launch", fired == 0 && k.vel[1] == 0.f, d); }
  // ---- 7. ИИ перевёрнут/разбит: на колёса через 1 с, порог 75° от вертикали (cos75° = 0,2588), скорость не учитывается
  { float q[4]; int ang = 0; double worst = 0;
    for (int a = 0; a <= 180; a += 5) { double th = a * 3.14159265358979 / 180.0; q[0] = (float)cos(th / 2); q[1] = (float)sin(th / 2); q[2] = 0; q[3] = 0; double e = fabs(brUpY(q) - cos(th)); if (e > worst) worst = e; ang++; }
    snprintf(d, sizeof d, "вертикаль оси «вверх» по кватерниону на %d углах: наибольшая ошибка %.6f (ждём < 1e-5)", ang, worst); check("upy_formula", worst < 1e-5, d);
    // оси x и z для крена и тангажа дают то же
    { float qz[4] = { (float)cos(.5), 0, 0, (float)sin(.5) }; snprintf(d, sizeof d, "поворот вокруг z на 57,3°: %.5f (ждём %.5f)", brUpY(qz), cos(1.0)); check("upy_z", fabs(brUpY(qz) - cos(1.0)) < 1e-5, d); } }
  { float t; int fired; float up74 = (float)cos(74 * 3.14159265358979 / 180), up76 = (float)cos(76 * 3.14159265358979 / 180);
    t = 0; fired = 0; for (int s = 0; s < 150; s++) fired += brAiFlipStep(up74, 0, &t, .02f);
    snprintf(d, sizeof d, "наклон 74° (мягче порога) 3 с: сбросов %d (ждём 0)", fired); check("flip_74", fired == 0, d);
    t = 0; fired = 0; int at = -1; for (int s = 0; s < 150; s++) if (brAiFlipStep(up76, 0, &t, .02f)) { fired++; if (at < 0) at = s; }
    snprintf(d, sizeof d, "наклон 76°: первый сброс в кадре %d (%.2f с, ждём 1,00 ± 0,02), за 3 с сбросов %d (ждём 2: на 1-й и 2-й секунде, третья не успела)", at, (at + 1) * .02f, fired); check("flip_76", at >= 48 && at <= 50 && fired == 2, d);
    t = 0; fired = 0; for (int s = 0; s < 49; s++) fired += brAiFlipStep(up76, 0, &t, .02f); fired += brAiFlipStep(1.f, 0, &t, .02f); for (int s = 0; s < 49; s++) fired += brAiFlipStep(up76, 0, &t, .02f);
    snprintf(d, sizeof d, "0,98 с наклона, вернулась на колёса, ещё 0,98 с: сбросов %d (ждём 0: счёт обнулился)", fired); check("flip_recover", fired == 0, d);
    t = 0; fired = 0; at = -1; for (int s = 0; s < 100; s++) if (brAiFlipStep(1.f, 1, &t, .02f)) { fired++; if (at < 0) at = s; }
    snprintf(d, sizeof d, "разбита (на колёсах): сброс в кадре %d (ждём 48..50), не отменяется", at); check("flip_destroyed", at >= 48 && at <= 50, d); }
  // ---- 8. очки за место 4, 3, 2, 1 (P17-23)
  snprintf(d, sizeof d, "очки за места 1..4: %d %d %d %d (ждём 4 3 2 1)", brPoints(1), brPoints(2), brPoints(3), brPoints(4)); check("points", brPoints(1) == 4 && brPoints(2) == 3 && brPoints(3) == 2 && brPoints(4) == 1, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
