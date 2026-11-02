#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bonus.h"

/* сверка g07b-msnview (P17-15): 10 с до DNF начинаются, когда доехал предпоследний (n-1 из n) или все люди; последний финишёр обрывает отсчёт сразу.
   BR_DNF10_OLD=1 в этой сборке — проверка прежнего правила (20 с от первого финишёра): красный контроль. */
static void lapAll(BonusRace *b, float (*pos)[3], int who) {   /* машина who проходит чекпойнты хода по порядку (1 круг) */
  for (int k = 1; k <= b->ncp; k++) { memcpy(pos[who], b->t.cp[b->order[k % b->ncp]].c, 3 * sizeof(float)); brUpdate(b, (const float (*)[3])pos, .02f); }
}
static int check(const BnsTrack *t, int human, int n3, int allFin) {
  BonusRace b; brInit(&b, t, 1, 0, 4, human);
  float pos[4][3]; for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) pos[i][j] = 100000.f;
  brUpdate(&b, (const float (*)[3])pos, BR_COUNT_S + .1f);
  int order[4] = {1, 2, 3, 0}; if (human < 0) { order[0] = 0; order[1] = 1; order[2] = 2; order[3] = 3; }
  if (BR_DNF10_OLD) {   /* прежнее: 20 с от первого финишёра, даже если человек не доехал */
    lapAll(&b, pos, order[0]);
    brUpdate(&b, (const float (*)[3])pos, 19.9f - b.sinceFirst); if (b.state == BR_DONE) return 1;
    brUpdate(&b, (const float (*)[3])pos, .2f); return b.state != BR_DONE;
  }
  if (allFin) { for (int k = 0; k < 4; k++) lapAll(&b, pos, order[k]); return b.state != BR_DONE; }   /* последний обрывает сразу */
  for (int k = 0; k < n3; k++) {   /* доехали n3 машин */
    lapAll(&b, pos, order[k]);
    if (k < 2 && human != order[k] && b.dnfOn && !(human >= 0 && b.r[human].finished)) { fprintf(stderr, "countdown too early k=%d\n", k); return 1; }
  }
  int expectOn = n3 >= 3 || (human >= 0 && b.r[human].finished);
  if (b.dnfOn != expectOn) { fprintf(stderr, "dnfOn=%d expect %d (n3=%d human=%d)\n", b.dnfOn, expectOn, n3, human); return 1; }
  if (!expectOn) { brUpdate(&b, (const float (*)[3])pos, 60.f); return b.state == BR_DONE; }   /* два из четырёх и человек не доехал — гонка ждёт */
  brUpdate(&b, (const float (*)[3])pos, 9.8f - b.dnfT); if (b.state == BR_DONE) { fprintf(stderr, "closed before 10s\n"); return 1; }
  brUpdate(&b, (const float (*)[3])pos, .4f);
  int fail = b.state != BR_DONE; for (int i = 0; i < 4; i++) if (!b.r[i].finished && !b.r[i].dnf) fail = 1;
  if (fail) fprintf(stderr, "DNF failed human=%d n3=%d\n", human, n3);
  return fail;
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  void *data = malloc((size_t)n); if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) return 2; fclose(f);
  BnsTrack t; if (bnsParse(&t, data, (unsigned)n)) return 2;
  int bad = 0;
  bad += check(&t, 0, 3, 0);   /* человек доехал вторым, третий — отсчёт 10 с */
  bad += check(&t, 0, 1, 0);   /* человек доехал первым — отсчёт 10 с */
  bad += check(&t, 1, 2, 0);   /* доехали 2 соперника, человек нет — гонка ждёт (отсчёта нет) */
  bad += check(&t, -1, 3, 0);  /* только боты: предпоследний (3 из 4) запускает отсчёт */
  bad += check(&t, 0, 4, 1);   /* все доехали — закрывается сразу */
  free(data); printf("BONUS DNF/result: %s five cases (rule %s)\n", bad ? "FAIL" : "PASS", BR_DNF10_OLD ? "20s-first-winner OLD" : "10s");
  return !!bad;
}
