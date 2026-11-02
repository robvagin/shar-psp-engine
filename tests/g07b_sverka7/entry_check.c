// Окно g07b-msnview, круг 7 (P06-5): вход в миссию «как после загрузки» (загрузка уровня, сохранение, MISSION SELECT) начинает вступление с этапа RESET_TO_HERE; следующая миссия и отмена — с нулевого.
// Данные: build/missions/levelNN.msn. Красный контроль: -DMSN_ENTRYRESET_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msn.h"
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static MsnFile F; static MsnRuntime R; static MsnWorld W;
static void onEvent(void *u, const MsnEvent *e) { (void)u; (void)e; }
static void onCommand(void *u, const MsnCall *c) { (void)u; (void)c; }
static int onEntity(void *u, const char *n, MsnEntity *o) { (void)u; (void)n; (void)o; return 0; }
static int onOwns(void *u, int k, const char *n) { (void)u; (void)k; (void)n; return 1; }
static void fresh(void) { MsnHost h; memset(&h, 0, sizeof h); h.event = onEvent; h.command = onCommand; h.entity = onEntity; h.owns = onOwns; memset(&R, 0, sizeof R); msn_init(&R, &F, &h);
  memset(&W, 0, sizeof W); W.on_foot = 1; W.vehicle_health = 1.f; W.coins = 1000; W.player.x = W.player.z = -5000.f; }
int main(int argc, char **argv) {
  char d[240]; int tested = 0, skipped0 = 0;
  for (int a = 1; a < argc; a++) {
    FILE *fp = fopen(argv[a], "rb"); if (!fp) return 2; fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    void *buf = malloc((size_t)n + 16); if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2; fclose(fp);
    char err[200]; if (msn_open(&F, buf, (size_t)n, err, sizeof err)) return 2;
    for (uint32_t m = 0; m < F.nmissions; m++) {
      const MsnMission *M = &F.missions[m]; if (M->intro == MSN_NONE16 || (M->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT))) continue;
      const MsnPhase *P = &F.phases[M->intro]; int cp = -1;
      for (unsigned s = 0; s < P->stage_count; s++) if (F.stages[P->first_stage + s].flags & MSN_SF_CHECKPOINT) { cp = (int)s; break; }
      if (cp <= 0) { skipped0++; continue; }   // RESET_TO_HERE на нулевом этапе (или нет) — разницы нет
      fresh(); msn_enter_mission(&R, (int)m); msn_tick(&R, .05f, &W);
      int enter = R.stage;
      fresh(); msn_start_mission(&R, (int)m, 0); msn_tick(&R, .05f, &W);
      int plain = R.stage;
      snprintf(d, sizeof d, "%s: RESET_TO_HERE на этапе %d; вход как после загрузки -> этап %d (ждём %d), обычный старт (следующая миссия, отмена) -> этап %d (ждём 0)", msn_str(&F, M->id), cp, enter, cp, plain);
      check("entry", enter == cp && plain == 0 && R.status != MSN_FAILED, d); tested++;
    }
  }
  snprintf(d, sizeof d, "миссий с RESET_TO_HERE не на нулевом этапе: %d (ждём ≥ 5), с нулевым: %d", tested, skipped0); check("coverage", tested >= 5, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
