// A1.4 (сверка Fable 13.11): машина игрока с бочкой взорвалась (передний фронт «разрушена» при rt->cargo) → «бочка разбита» → keepbarrel откатывает миссию на N этапов назад.
// По данным level07.msn (m3, m5, m6, m7). Без бочки на борту взрыв машины откат не даёт (контроль). Красный: -DMSN_BARRELLOST_OLD=1 (сигнал не шлётся) — откат не происходит.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msn.h"
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-10s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static MsnFile F; static MsnRuntime R; static MsnWorld W;
static void onEvent(void *u, const MsnEvent *e) { (void)u; (void)e; }
static void onCommand(void *u, const MsnCall *c) { (void)u; (void)c; }
static int onEntity(void *u, const char *n, MsnEntity *o) { (void)u; (void)n; (void)o; return 0; }
static int onOwns(void *u, int k, const char *n) { (void)u; (void)k; (void)n; return 1; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *fp = fopen(argv[1], "rb"); if (!fp) return 2; fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
  void *buf = malloc((size_t)n + 16); if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2; fclose(fp);
  char err[200]; if (msn_open(&F, buf, (size_t)n, err, sizeof err)) return 2;
  int tested = 0; char d[240];
  for (uint32_t m = 0; m < F.nmissions; m++) {
    const MsnMission *M = &F.missions[m]; if (M->main == MSN_NONE16) continue; const MsnPhase *P = &F.phases[M->main];
    for (unsigned s = 0; s < P->stage_count; s++) {
      const MsnStage *S = &F.stages[P->first_stage + s]; int arg = 0;
      for (int k = 0; k < S->ncond; k++) if (F.conds[S->cond_first + k].type == MSN_COND_KEEPBARREL) arg = F.conds[S->cond_first + k].arg;
      if (arg <= 0 || (int)s < arg) continue;
      for (int carry = 1; carry >= 0; carry--) {
        MsnHost h; memset(&h, 0, sizeof h); h.event = onEvent; h.command = onCommand; h.entity = onEntity; h.owns = onOwns;
        memset(&R, 0, sizeof R); msn_init(&R, &F, &h);
        msn_start_mission(&R, (int)m, 0); R.cp_phase = 1; R.cp_stage = (int)s; R.cp_timer = 0; R.cp_remaining = 0; msn_retry(&R, 0);
        memset(&W, 0, sizeof W); W.vehicle = "x_v"; W.vehicle_health = 1.f; W.coins = 1000; W.player.x = W.player.z = -5000.f;
        for (int i = 0; i < 100 && R.status == MSN_COUNTDOWN; i++) msn_tick(&R, .05f, &W);
        R.cargo = carry; int prev = 0, before = R.stage;
        msn_cargo_watch(&R, 0, &prev); msn_tick(&R, .05f, &W); int calm = R.stage == before;
        msn_cargo_watch(&R, 1, &prev); msn_tick(&R, .05f, &W); msn_tick(&R, .05f, &W);   // машина взорвалась
        int want = carry ? before - arg : before;
        snprintf(d, sizeof d, "%s этап %d, бочка на борту %d: после взрыва машины status=%d fail=%d этап %d (ждём этап %d, не провал)", msn_str(&F, M->id), before, carry, R.status, R.fail, R.stage, want);
        check(carry ? "rollback" : "nocargo", calm && R.status != MSN_FAILED && R.stage == want, d); tested++;
      }
    }
  }
  snprintf(d, sizeof d, "проверено случаев: %d (ждём ≥ 8)", tested); check("coverage", tested >= 8, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
