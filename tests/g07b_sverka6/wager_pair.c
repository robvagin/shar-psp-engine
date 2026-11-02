// Окно g07b-msnview, круг 6, пара «карточка ставки ↔ выплата рантайма» (P06-3, P07-3, P01-20; дефект №9 из навыка сверки).
// Что доказывает: (1) формула карточки (msn_wager.h, её зовёт mvWagerPay в msn_view.c) = вход + целая часть (коэффициент × вход);
// (2) рантайм ставочной гонки на настоящих данных диска (build/missions/levelNN.msn), доехав до финиша быстрее пара, платит ТО ЖЕ число, которое показала бы карточка,
// через msnWagerPayFn; (3) у уровней 1–5 это 3 входа, у 6–7 — 2 входа (коэффициент по умолчанию). Красный контроль: -DSV_WAGER_ODDS_OLD=1 и/или -DMSN_BONUSREPEAT_OLD=1 обязаны упасть.
//   wager_pair build/missions/level01.msn ... level07.msn
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msn.h"
#include "msn_wager.h"
#include "carparams.h"   // таблица машин с диска: коэффициент SetGamblingOdds каждой

#ifndef SV_WAGER_ODDS_OLD
#define SV_WAGER_ODDS_OLD 0
#endif
static int fails, nchecks;
static float oddsOf(const char *n) { for (unsigned i = 0; i < sizeof carCons / sizeof *carCons; i++) if (!strcmp(carCons[i].name, n)) return carCons[i].gamblingOdds; return -9.f; }
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-10s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }

// Карточка: то же, что mvWagerPay в msn_view.c (там перед формулой стоит выключатель прежнего; повторяем его, чтобы красный контроль видел именно его)
static float cardOdds = -1.f; static int cardLevel = 1;
static int cardPay(int fee) {
  if (SV_WAGER_ODDS_OLD) return 2 * fee;
  float o = cardOdds; if (o < 0.f) o = mvWagerOddsDefault(cardLevel);
  return mvWagerPayOf(fee, o);
}

static MsnFile F; static MsnRuntime R; static MsnWorld W;
static float coinsGot; static int doneEv;
static void onEvent(void *u, const MsnEvent *e) { (void)u; if (e->type == MSN_EV_MISSION_COMPLETE) { coinsGot = e->value; doneEv++; } }
static void onCommand(void *u, const MsnCall *c) { (void)u; (void)c; }
static int onEntity(void *u, const char *n, MsnEntity *o) { (void)u; (void)n; (void)o; return 0; }
static int onOwns(void *u, int k, const char *n) { (void)u; (void)k; (void)n; return 1; }

static int pGamble(const MsnStage *S) { return (S->flags & MSN_SF_ELAPSED) && msn_streq(msn_str(&F, S->obj_arg), "gamble"); }
static int feeOf(const MsnMission *M) {
  const MsnPhase *ph = &F.phases[M->main]; int fee = -1;
  for (unsigned s = 0; s < ph->stage_count; s++) { const MsnStage *st = &F.stages[ph->first_stage + s];
    for (unsigned i = 0; i < st->cmd_count; i++) if (F.cmds[st->cmd_first + i].op == MSN_OP_SETRACEENTERYFEE) fee = msn_arg_i(&F, &F.cmds[st->cmd_first + i], 0); }
  return fee;
}

// Гонка до финиша: каждый тик ставим игрока в точку цели (так же, как это сделал бы проехавший трассу) — быстрее пара, значит выплата полагается
static int playRace(int mi, int ph, int si) {
  memset(&R, 0, sizeof R);
  MsnHost h; memset(&h, 0, sizeof h); h.event = onEvent; h.command = onCommand; h.entity = onEntity; h.owns = onOwns;
  msn_init(&R, &F, &h); coinsGot = -1; doneEv = 0;
  msn_start_mission(&R, mi, 0); R.cp_phase = ph; R.cp_stage = si; R.cp_timer = 0; R.cp_remaining = 0; msn_retry(&R, 0);
  memset(&W, 0, sizeof W); W.on_foot = 0; W.vehicle = "famil_v"; W.vehicle_health = 1.f; W.coins = 1000; W.player.x = W.player.z = -5000.f;
  for (int i = 0; i < 20000 && R.status != MSN_MISSION_DONE; i++) {
    MsnVec3 p;
    if (R.status == MSN_RUNNING && msn_nav_target(&R, &W, &p)) {
      const MsnLoc *l = msn_nav_locator(&R, &W);          // закрывает цель объём триггера локатора, а не метка: встаём в центр первого объёма
      if (l && l->ntrig) { const MsnTrig *t = &F.trigs[l->first_trig]; p.x = t->center[0]; p.y = t->center[1]; p.z = t->center[2]; }
      W.player = p;
    }
    msn_tick(&R, 0.05f, &W);
    if (R.status == MSN_FAILED) { printf("  (провал: fail=%d stage=%d obj=%d пар=%.0f время=%.1f круг=%d/%d точка=%d/%d)\n", R.fail, R.stage, R.obj, R.par, R.remaining, R.lap, R.laps, R.next_item, R.nitems); return 0; }
  }
  if (R.status != MSN_MISSION_DONE) printf("  (не дошла: status=%d stage=%d obj=%d next=%d/%d)\n", R.status, R.stage, R.obj, R.next_item, R.nitems);
  return R.status == MSN_MISSION_DONE;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: wager_pair levelNN.msn ...\n"); return 2; }
  char d[200];
  // ---- 1. формула карточки на значениях, где «соседние ложные» дают другой ответ
  cardOdds = 2.f; snprintf(d, sizeof d, "вход 50, коэфф 2,0: %d (ждём 150, прежнее 100)", cardPay(50)); check("card_x2", cardPay(50) == 150, d);
  cardOdds = 1.f; snprintf(d, sizeof d, "вход 50, коэфф 1,0: %d (ждём 100)", cardPay(50)); check("card_x1", cardPay(50) == 100, d);
  cardOdds = 3.f; snprintf(d, sizeof d, "вход 50, коэфф 3,0: %d (ждём 200)", cardPay(50)); check("card_x3", cardPay(50) == 200, d);
  cardOdds = 1.5f; snprintf(d, sizeof d, "вход 7, коэфф 1,5: %d (ждём 7+10=17, округление дало бы 18)", cardPay(7)); check("card_trunc", cardPay(7) == 17, d);
  cardOdds = -1.f; cardLevel = 3; snprintf(d, sizeof d, "L3 без данных машины: %d (ждём 3 входа = 150)", cardPay(50)); check("dflt_L3", cardPay(50) == 150, d);
  cardLevel = 7; snprintf(d, sizeof d, "L7 без данных машины: %d (ждём 2 входа = 100)", cardPay(50)); check("dflt_L7", cardPay(50) == 100, d);
  // ---- 1б. данные диска: стартовые машины уровней — коэффициент, который берёт карточка по умолчанию
  { static const char *const st[7] = { "famil_v", "honor_v", "lisa_v", "marge_v", "apu_v", "bart_v", "homer_v" };
    for (int l = 1; l <= 7; l++) { float o = oddsOf(st[l - 1]); snprintf(d, sizeof d, "L%d стартовая %s: коэфф %.1f, по умолчанию уровня %.1f", l, st[l - 1], o, mvWagerOddsDefault(l));
      check("disc_odds", o == mvWagerOddsDefault(l), d); } }
  // ---- 2. рантайм платит то же, что карточка (по всем ставочным гонкам диска)
  int races = 0;
  for (int a = 1; a < argc; a++) {
    FILE *fp = fopen(argv[a], "rb"); if (!fp) { perror(argv[a]); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    void *buf = malloc((size_t)n + 16); if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2; fclose(fp);
    char err[200]; if (msn_open(&F, buf, (size_t)n, err, sizeof err)) { fprintf(stderr, "msn_open: %s\n", err); return 2; }
    int lvl = (F.hdr && F.hdr->level) ? (int)F.hdr->level : a;
    for (uint32_t m = 0; m < F.nmissions; m++) {
      const MsnMission *M = &F.missions[m]; if (!(M->flags & MSN_MF_GAMBLE) || M->main == MSN_NONE16) continue;
      int fee = feeOf(M); if (fee <= 0) continue;
      int found = 0, ph = 1, si = 0;
      { const MsnPhase *P = &F.phases[M->main]; for (unsigned s = 0; s < P->stage_count; s++) if (pGamble(&F.stages[P->first_stage + s])) { si = (int)s; found = 1; break; } }
      if (!found) continue;
      for (int k = 0; k < 3; k++) {
        float odds = k == 0 ? 2.f : k == 1 ? 1.f : 3.f;    // коэффициент текущей машины игрока
        cardOdds = odds; cardLevel = lvl;
        msnWagerPayFn = cardPay;                           // так игра подключает рантайм (game_msn.c: msnWagerPayFn = mvWagerPay)
        int ok = playRace((int)m, ph, si);
        int want = cardPay(fee);
        snprintf(d, sizeof d, "L%d %s вход %d коэфф %.1f: рантайм %.0f, карточка %d, финиш %d", lvl, msn_str(&F, M->id), fee, odds, coinsGot, want, ok);
        check("pair", ok && (int)coinsGot == want && want == mvWagerPayOf(fee, odds), d);
      }
      // без коэффициента машины: по умолчанию уровня; ставка = 3 входа (L1–L5) / 2 входа (L6–L7)
      cardOdds = -1.f; cardLevel = lvl; msnWagerPayFn = cardPay;
      { int ok = playRace((int)m, ph, si); int want = (lvl >= 6 ? 2 : 3) * fee;
        snprintf(d, sizeof d, "L%d %s без данных машины: рантайм %.0f, ждём %d", lvl, msn_str(&F, M->id), coinsGot, want); check("pair_dflt", ok && (int)coinsGot == want, d); }
      races++;
    }
  }
  snprintf(d, sizeof d, "ставочных гонок диска пройдено: %d", races); check("coverage", races >= 7, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
