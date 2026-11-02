// Окно g07b-msnview, круг 7 (P17-16): порядок прохода чекпойнтов бонус-трасс по спискам оригинала (b02 и b07 — номер 3 дважды; b05 — 7 и 3 дважды), настоящий viewer/bonus.c и настоящие .bns.
// Проверяем: (1) номера круга по порядку = список, вперёд и назад; (2) круг, пройденный по списку, засчитывается ровно один раз в конце; (3) пропуск повторного чекпойнта — круг не засчитан;
// (4) путь ИИ цел: у каждой остановки круга есть точка пути (npath > 0). Красный контроль: -DBR_LAPLIST_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bonus.h"
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static unsigned char *slurp(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f);
  unsigned char *d = malloc((size_t)s); if (!d || fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); return 0; } fclose(f); *n = (unsigned)s; return d;
}
// списки оригинала (данные), вперёд; назад — выводится
static const int FWD[8][14] = { {0}, {2,3,4,5,6,7,8,1,0}, {2,3,4,5,6,3,8,9,1,0}, {2,3,4,5,6,7,1,0}, {2,3,4,5,6,7,1,0}, {2,3,4,5,6,7,8,9,10,7,3,11,1,0}, {2,3,4,5,6,7,1,0}, {2,3,4,5,6,3,8,9,1,0} };
static const int REV[8][14] = { {0}, {8,7,6,5,4,3,2,1,0}, {9,8,3,6,5,4,3,2,1,0}, {7,6,5,4,3,2,1,0}, {7,6,5,4,3,2,1,0}, {11,3,7,10,9,8,7,6,5,4,3,2,1,0}, {7,6,5,4,3,2,1,0}, {9,8,3,6,5,4,3,2,1,0} };
static int nums(const BonusRace *b, int *out) { int n = 0; for (int k = 1; k <= b->ncp; k++) out[n++] = (int)b->t.cp[b->order[k % b->ncp]].order; return n; }
int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: lap_host DIR_BNS_MAIN DIR_BNS_NEW\n"); return 2; }
  char path[512], d[256]; static BnsTrack T[8]; static unsigned char *D[8];
  for (int n = 1; n <= 7; n++) {
    unsigned sz; snprintf(path, sizeof path, "%s/b0%d.bns", (n == 2 || n == 5 || n == 7) ? argv[2] : argv[1], n);
    D[n] = slurp(path, &sz); if (!D[n] || bnsParse(&T[n], D[n], sz)) { fprintf(stderr, "нет/плохой %s\n", path); return 2; }
  }
  for (int n = 1; n <= 7; n++) for (int rev = 0; rev < 2; rev++) {
    BonusRace b; brInit(&b, &T[n], 1, rev, 4, 0);
    int got[16], k = nums(&b, got); const int *want = rev ? REV[n] : FWD[n]; int wn = 0; while (want[wn]) wn++;
    int same = k == wn; for (int i = 0; same && i < k; i++) same = got[i] == want[i];
    snprintf(d, sizeof d, "b0%d %s: длина круга %d (ждём %d), первый %d, последний %d (ждём финиш 1)", n, rev ? "назад" : "вперёд", k, wn, got[0], got[k - 1]); check("order", same, d);
    snprintf(d, sizeof d, "b0%d %s: остановок пути %d, точек пути %d (ждём > 0: у каждой остановки круга есть точка)", n, rev ? "назад" : "вперёд", b.nwp, b.npath); check("path", b.npath > 0, d);
    // круг по списку: машина 0 ставится в центр каждого чекпойнта по порядку; засчитан ровно в конце
    float pos[4][3]; for (int i = 0; i < 4; i++) { pos[i][0] = pos[i][2] = 1e6f; pos[i][1] = 0; }
    brUpdate(&b, (const float (*)[3])pos, BR_COUNT_S + .1f);
    int lapAt = -1; for (int s = 0; s < b.ncp; s++) { memcpy(pos[0], b.t.cp[b.order[(s + 1) % b.ncp]].c, 12); unsigned m = brUpdate(&b, (const float (*)[3])pos, .02f); if ((m & 1u) && lapAt < 0) lapAt = s; }
    snprintf(d, sizeof d, "b0%d %s: круг засчитан на шаге %d из %d (ждём последний), кругов %d", n, rev ? "назад" : "вперёд", lapAt + 1, b.ncp, b.r[0].lap); check("lap_counted", lapAt == b.ncp - 1 && b.r[0].lap == 1 && b.r[0].finished, d);
    // пропуск повторного чекпойнта: проходим все, кроме второго вхождения повторяющегося номера
    int dupAt = -1; for (int s = 0; s < k && dupAt < 0; s++) for (int t = 0; t < s; t++) if (got[t] == got[s] && got[s] != 1) { dupAt = s; break; }
    if (dupAt >= 0) {
      BonusRace c; brInit(&c, &T[n], 1, rev, 4, 0); for (int i = 0; i < 4; i++) { pos[i][0] = pos[i][2] = 1e6f; }
      brUpdate(&c, (const float (*)[3])pos, BR_COUNT_S + .1f);
      for (int s = 0; s < c.ncp; s++) { if (s == dupAt) continue; memcpy(pos[0], c.t.cp[c.order[(s + 1) % c.ncp]].c, 12); brUpdate(&c, (const float (*)[3])pos, .02f); }
      snprintf(d, sizeof d, "b0%d %s: пропущен повторный №%d — круг не засчитан (кругов %d, ждём 0)", n, rev ? "назад" : "вперёд", got[dupAt], c.r[0].lap); check("skip_dup", c.r[0].lap == 0 && !c.r[0].finished, d);
    }
  }
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
