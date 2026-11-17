// 1304 (Гомер: настоящий провал по таймеру на L2 m1 «DRIVE TO THE TRY-N-SAVE», затем «yes» — повтор не стартует): хост-игра без экрана. Провал таймера, msn_retry(0) как restartRun, потом статус и этап.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
static void dump(const char *t) { const MsnRuntime *r = &W.msn.rt; printf("R1304 %-12s cur=%d phase=%d stage=%d obj=%d status=%d timer_on=%d remaining=%.1f cp=%d/%d restoring=%d\n", t, r->cur, r->phase, r->stage, r->obj, r->status, r->timer_on, r->remaining, r->cp_phase, r->cp_stage, r->restoring); }
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "retry_host: нет данных\n"); return 2; }
  bwNew(&W); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  for (int g = 0; g < 40 && !(W.msn.rt.phase == 1 && W.msn.rt.stage >= 1); g++) { msn_service_pass(&W.msn.rt); for (int f = 0; f < 6; f++) bwStep(&W, 0, 0, 0); }
  dump("до таймера");
  W.msn.rt.remaining = 0.05f; W.msn.rt.timer_on = 1;
  for (int f = 0; f < 180 && W.msn.rt.status != MSN_FAILED; f++) bwStep(&W, 0, 0, 0);
  dump("после провала");
  int failed = W.msn.rt.status == MSN_FAILED;
  msn_retry(&W.msn.rt, 0);   // restartRun(2): как «yes»
  dump("сразу retry");
  for (int f = 0; f < 120; f++) bwStep(&W, 0, 0, 0);
  dump("через 2 с");
  int ok = failed && W.msn.rt.status == MSN_RUNNING && W.msn.rt.phase == 1 && W.msn.rt.obj != 0;
  printf("RESULT %s\n", ok ? "OK" : "FAIL"); return ok ? 0 : 1;
}
