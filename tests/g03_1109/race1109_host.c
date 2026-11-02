// Правка Роба 1109, окно g03-aicars: правила бонус-гонки по оригиналу на настоящих трассах b01..b07 (BNS1 из build/bonus), без физики машин.
// Проверки: (1) список чекпойнтов круга с повторами (b02/b07: 3 дважды, b05: 7 и 3 по второму разу), прямой и обратный ход; круг не засчитывается без повторов;
// (2) «читер»: чекпойнт не по порядку — возврат на предыдущий шаг, финиш такого круга не даёт; повторное касание предыдущего чекпойнта не наказывается;
// (3) место: круги, шаг круга, потом ПУТЬ до следующего чекпойнта (а не доля прямой): пара точек, где они расходятся; (4) гонка 4 машин по пути с разной скоростью: места
// в каждый момент по скорости, финиши по порядку, три круга на каждой трассе. Красный: bonus.c с -DBR_SEQ_OLD=1 -DBR_CHEAT_OLD=1 -DBR_POSDIST_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bonus.h"
static int fails, nchk;
static void check(const char *n, int ok, const char *d) { nchk++; printf("CHECK %-9s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static const int SEQ[8][2][16] = {{{0}}, {{2, 3, 4, 5, 6, 7, 8, 1, 0}, {8, 7, 6, 5, 4, 3, 2, 1, 0}}, {{2, 3, 4, 5, 6, 3, 8, 9, 1, 0}, {9, 8, 3, 6, 5, 4, 3, 2, 1, 0}},
  {{2, 3, 4, 5, 6, 7, 1, 0}, {7, 6, 5, 4, 3, 2, 1, 0}}, {{2, 3, 4, 5, 6, 7, 1, 0}, {7, 6, 5, 4, 3, 2, 1, 0}},
  {{2, 3, 4, 5, 6, 7, 8, 9, 10, 7, 3, 11, 1, 0}, {11, 3, 7, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}}, {{2, 3, 4, 5, 6, 7, 1, 0}, {7, 6, 5, 4, 3, 2, 1, 0}},
  {{2, 3, 4, 5, 6, 3, 8, 9, 1, 0}, {9, 8, 3, 6, 5, 4, 3, 2, 1, 0}}};   // списки шагов круга оригинала по трассам (0 — конец)
static BnsTrack T[8]; static void *D[8];
static const float FAR[3] = {1e5f, 0.f, 1e5f};
static void step(BonusRace *b, float (*pos)[3]) { brUpdate(b, (const float (*)[3])pos, .02f); }
static int cpOf(const BonusRace *b, int num) { for (unsigned c = 0; c < b->t.h->ncp; c++) if ((int)b->t.cp[c].order == num) return (int)c; return -1; }
// машина who входит в чекпойнт num и выходит
static void visit(BonusRace *b, float (*pos)[3], int who, int num) {
  int c = cpOf(b, num); if (c < 0) return;
  memcpy(pos[who], b->t.cp[c].c, 12); step(b, pos);
  memcpy(pos[who], FAR, 12); step(b, pos);
}
static int gHuman = 0;
static BonusRace *fresh(int tr, int rev, int laps, float (*pos)[3]) {
  static BonusRace B; brInit(&B, &T[tr], laps, rev, 4, gHuman);
  for (int i = 0; i < 4; i++) memcpy(pos[i], FAR, 12);
  brUpdate(&B, (const float (*)[3])pos, BR_COUNT_S + .1f); for (int i = 0; i < 4; i++) B.grow[i] = 1.5f; step(&B, pos); return &B;   // запас на ширину кузова, как в игре; кадр гонки с машинами вдали: маски объёмов от старта сброшены
}
int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "build/bonus"; char fn[256], d[300];
  for (int t = 1; t <= 7; t++) {
    snprintf(fn, sizeof fn, "%s/b%02d.bns", dir, t); FILE *f = fopen(fn, "rb"); if (!f) { printf("нет %s\n", fn); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); D[t] = malloc((size_t)n); if (fread(D[t], 1, (size_t)n, f) != (size_t)n) return 2; fclose(f);
    if (bnsParse(&T[t], D[t], (unsigned)n)) { printf("битый %s\n", fn); return 2; }
  }
  float pos[4][3];
  // ---- (1) круг по списку оригинала, оба хода, все семь трасс
  { int okAll = 1; char bad[120] = "";
    for (int t = 1; t <= 7; t++) for (int r = 0; r < 2; r++) {
      BonusRace *b = fresh(t, r, 1, pos); int k = 0;
      for (; SEQ[t][r][k]; k++) { visit(b, pos, 0, SEQ[t][r][k]); if (SEQ[t][r][k + 1] && b->r[0].lap) { okAll = 0; snprintf(bad, sizeof bad, "b0%d ход %d: круг раньше времени на шаге %d", t, r, k); } }
      if (b->ncp != k) { okAll = 0; snprintf(bad, sizeof bad, "b0%d ход %d: шагов круга %d, в списке оригинала %d", t, r, b->ncp, k); }
      if (b->r[0].lap != 1 || !b->r[0].finished) { okAll = 0; snprintf(bad, sizeof bad, "b0%d ход %d: список из %d шагов круг не дал", t, r, k); } }
    snprintf(d, sizeof d, "все 7 трасс в оба хода: круг даёт именно список оригинала %s", bad); check("seq", okAll, d); }
  { // повторы обязательны: b02 и b05 с одним касанием «лишних» чекпойнтов круг не дают
    int ok = 1;
    { BonusRace *b = fresh(2, 0, 1, pos); int s[] = {2, 3, 4, 5, 6, 8, 9, 1, 0}; for (int k = 0; s[k]; k++) visit(b, pos, 0, s[k]); if (b->r[0].lap) ok = 0; }
    { BonusRace *b = fresh(5, 0, 1, pos); int s[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 0}; for (int k = 0; s[k]; k++) visit(b, pos, 0, s[k]); if (b->r[0].lap) ok = 0; }
    { BonusRace *b = fresh(7, 1, 1, pos); int s[] = {9, 8, 6, 5, 4, 3, 2, 1, 0}; for (int k = 0; s[k]; k++) visit(b, pos, 0, s[k]); if (b->r[0].lap) ok = 0; }
    snprintf(d, sizeof d, "без повторных касаний (b02: 3 раз, b05: 7 и 3, b07 обратный: 3 раза) круг не засчитан"); check("repeat", ok, d); }
  // ---- (2) «читер»
  { BonusRace *b = fresh(1, 0, 1, pos);
    visit(b, pos, 0, 5);                 // ждали 2 — взяли 5: назад на финиш, пометка
    int cheat1 = b->r[0].next == 0 && b->r[0].cheated;
    visit(b, pos, 0, 1);                 // нужный (финиш, он же «предыдущий» для шага 0): без круга
    int afterFin = b->r[0].lap == 0 && b->r[0].next == 1 && !b->r[0].cheated;
    int s[] = {2, 3, 4, 5, 6, 7, 8, 1, 0}; for (int k = 0; s[k]; k++) visit(b, pos, 0, s[k]);
    int lap = b->r[0].lap == 1;
    BonusRace *c = fresh(1, 0, 1, pos); visit(c, pos, 0, 2); visit(c, pos, 0, 2); int noPunish = !c->r[0].cheated && c->r[0].next == 2;
    snprintf(d, sizeof d, "чужой чекпойнт: возврат и пометка (%d); финиш помеченного круга не даёт (%d); потом круг считается (%d); повтор предыдущего не наказан (%d)", cheat1, afterFin, lap, noPunish);
    check("cheat", cheat1 && afterFin && lap && noPunish, d); }
  int pathOk = 0, lapOk = 0; char pathWhat[300] = "", lapWhat[300] = "";
  // ---- (3) место по пути, а не по прямой: найти на трассе отрезок, где точка раньше по пути стоит ближе к чекпойнту по прямой
  { int found = 0, ok = 0; char what[160] = "";
    for (int t = 1; t <= 7 && !found; t++) for (int r = 0; r < 2 && !found; r++) {
      BonusRace *b = fresh(t, r, 3, pos); if (b->npath < 5) continue;
      for (int g = 0; g < b->nwp && !found; g++) {
        int pr = (g + b->nwp - 1) % b->nwp, N = b->npath, i0 = b->stopAt[pr], i1 = b->stopAt[g], len = (i1 - i0 + N) % N; if (len < 6) continue;
        const float *B = b->wp[g];
        for (int s1 = 1; s1 < len - 2 && !found; s1++) for (int s2 = s1 + 2; s2 < len && !found; s2++) {
          const float *p1 = b->path[(i0 + s1) % N].p, *p2 = b->path[(i0 + s2) % N].p;
          float d1 = hypotf(p1[0] - B[0], p1[2] - B[2]), d2 = hypotf(p2[0] - B[0], p2[2] - B[2]);
          if (d1 + 8.f < d2) {   // p1 раньше по пути, но по прямой ближе к цели на 8 м: по пути впереди тот, кто в p2
            memcpy(pos[0], p1, 12); memcpy(pos[1], p2, 12); memcpy(pos[2], FAR, 12); memcpy(pos[3], FAR, 12);
            // обе машины на шаге g: выставить next перед обновлением
            b->r[0].next = b->r[1].next = (g + 1) % b->ncp; b->r[0].inMask = b->r[1].inMask = 0;
            step(b, pos);
            ok = b->r[1].place < b->r[0].place; found = 1; snprintf(what, sizeof what, "b0%d ход %d шаг %d: точки пути %d и %d, прямая %.0f против %.0f м; места %d и %d", t, r, g, s1, s2, d1, d2, b->r[0].place, b->r[1].place);
          } } } }
    snprintf(d, sizeof d, "на одном шаге впереди тот, кому ближе по ПУТИ: %s", found ? what : "подходящей пары не нашлось"); pathOk = found && ok; strncpy(pathWhat, d, sizeof pathWhat - 1); }
  { // круг и шаг важнее расстояния
    BonusRace *b = fresh(1, 0, 3, pos); b->r[0].lap = 1; b->r[0].next = 1; b->r[1].lap = 0; b->r[1].next = 3; b->r[2].lap = 0; b->r[2].next = 3;
    for (int i = 0; i < 4; i++) memcpy(pos[i], b->t.cp[b->order[b->r[i].next]].c, 12); memcpy(pos[3], FAR, 12); for (int i = 0; i < 3; i++) for (int q = 0; q < 3; q++) pos[i][q] += i == 0 ? 80.f : 0.f;
    step(b, pos);
    snprintf(d, sizeof d, "круг 1 на шаге 1 впереди круга 0 на шаге 3 (места %d и %d); на круге 0 шаг 3 впереди шага 1", b->r[0].place, b->r[1].place);
    lapOk = b->r[0].place == 1 && b->r[1].place < b->r[3].place; strncpy(lapWhat, d, sizeof lapWhat - 1); }
  // ---- (4) гонка четырёх по пути: три круга, места по скорости (все машины — ИИ: правило «читера» к ним не применяется)
  gHuman = -1;
  { int allOk = 1; char bad[200] = "";
    for (int t = 1; t <= 7; t++) for (int r = 0; r < 2; r++) {
      BonusRace *b = fresh(t, r, 3, pos); if (b->npath < 5) { allOk = 0; snprintf(bad, sizeof bad, "b0%d ход %d: нет пути ИИ (старый .bns?)", t, r); continue; }
      float sp[4] = {1.00f, .93f, .86f, .79f}, at[4] = {0, 0, 0, 0}; int N = b->npath, start = b->stopAt[b->nwp - 1], mono = 1, fin = 0; double tFin[4] = {0, 0, 0, 0};
      for (int f = 0; f < 60 * 400 && b->state != BR_DONE && fin < 4; f++) {
        for (int i = 0; i < 4; i++) { if (b->r[i].finished) continue; at[i] += sp[i] * .35f; int ix = (start + (int)at[i]) % N; memcpy(pos[i], b->path[ix].p, 12);
          for (int k = 0; k < b->nwp; k++) if (b->stopAt[k] == ix) memcpy(pos[i], b->t.cp[b->order[(k + 1) % b->ncp]].c, 12); }   // на остановке — центр её чекпойнта (точка пути лежит рядом с тонкими воротами)
        step(b, pos);
        for (int i = 0; i < 4; i++) if (b->r[i].finished && tFin[i] == 0) { tFin[i] = f; fin++; }
        if (f % 20 == 0 && f > 200) for (int i = 0; i < 3; i++) if (!b->r[i].finished && !b->r[i + 1].finished && at[i] - at[i + 1] > 40.f && b->r[i].place > b->r[i + 1].place) { mono = 0; }
      }
      if (!(fin == 4 && tFin[0] < tFin[1] && tFin[1] < tFin[2] && tFin[2] < tFin[3]) || !mono || b->r[0].place != 1 || b->r[3].place != 4) { allOk = 0; if (getenv("V")) fprintf(stderr, "FAIL b0%d ход %d: финишировали %d порядок %.0f %.0f %.0f %.0f mono %d places %d %d\n", t, r, fin, tFin[0], tFin[1], tFin[2], tFin[3], mono, b->r[0].place, b->r[3].place);
      snprintf(bad, sizeof bad, "b0%d ход %d: финишировали %d, порядок %.0f %.0f %.0f %.0f, места по ходу монотонны %d", t, r, fin, tFin[0], tFin[1], tFin[2], tFin[3], mono); }
    }
    snprintf(d, sizeof d, "4 машины по пути с разной скоростью, 3 круга, 7 трасс × 2 хода: финиш по скорости, места по ходу %s", bad); check("places", pathOk && lapOk && allOk, d); printf("      %s\n      %s\n", pathWhat, lapWhat); }
  printf("RESULT %s провалов %d из %d\n", fails ? "FAIL" : "PASS", fails, nchk);
  return fails ? 1 : 0;
}
