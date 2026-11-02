// Окно g07b-msnview, круг 6: правила экранов миссии (viewer/msn_rules.h — те же функции, что зовёт msn_view.c).
// Строки: P16-07 / P16 Д4 (дорожные стрелки: режимы и типы целей), P09 Д8 (реплика старта миссии), P13-3 (Skip после 7 запусков, L7 с 5-й), P13-6 (значки), M2:16 (мигание кругов), M2:6 (Item Dropped), M2:27 (LOADING).
// Красный контроль: -DSV_NAVWHITE_OLD=1 -DSV_NAVPICK_OLD=1 -DSV_NAVARROW_OLD=1 -DSV_RACE_MSTART3_OLD=1 -DSV_SKIP7_OLD=1 -DSV_LOADBLINK_OLD=1 — каждый уронит свои проверки.
#include <stdio.h>
#include <string.h>
#include "msn_rules.h"
#ifndef SV_NAVWHITE_OLD
#define SV_NAVWHITE_OLD 0
#endif
#ifndef SV_NAVPICK_OLD
#define SV_NAVPICK_OLD 0
#endif
#ifndef SV_NAVARROW_OLD
#define SV_NAVARROW_OLD 0
#endif
#ifndef SV_RACE_MSTART3_OLD
#define SV_RACE_MSTART3_OLD 0
#endif
#ifndef SV_SKIP7_OLD
#define SV_SKIP7_OLD 0
#endif
#ifndef SV_LOADBLINK_OLD
#define SV_LOADBLINK_OLD 0
#endif
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static int route(int obj, int nav, int on) { return mvRouteAllowed(obj, nav, on, SV_NAVARROW_OLD, SV_NAVWHITE_OLD, SV_NAVPICK_OLD); }
int main(void) {
  char d[200];
  // ---- дорожные стрелки по типам целей: каждый тип из перечня, считаем с обеих сторон (21 значение enum)
  static const struct { int obj; const char *n; int want; } T[] = {
    { MSN_OBJ_NONE, "none", 0 }, { MSN_OBJ_GOTO, "goto", 1 }, { MSN_OBJ_GETIN, "getin", 0 }, { MSN_OBJ_TALKTO, "talkto", 1 }, { MSN_OBJ_DIALOGUE, "dialogue", 0 },
    { MSN_OBJ_INTERIOR, "interior", 1 }, { MSN_OBJ_GOOUTSIDE, "gooutside", 1 }, { MSN_OBJ_RACE, "race", 1 }, { MSN_OBJ_FOLLOW, "follow", 0 }, { MSN_OBJ_LOSETAIL, "losetail", 0 },
    { MSN_OBJ_DELIVERY, "delivery", 1 }, { MSN_OBJ_DUMP, "dump", 1 }, { MSN_OBJ_DESTROY, "destroy", 0 }, { MSN_OBJ_PICKUPITEM, "pickupitem", 0 }, { MSN_OBJ_DESTROYBOSS, "destroyboss", 0 },
    { MSN_OBJ_TIMER, "timer", 0 }, { MSN_OBJ_FMV, "fmv", 0 }, { MSN_OBJ_BUYCAR, "buycar", 0 }, { MSN_OBJ_BUYSKIN, "buyskin", 0 }, { MSN_OBJ_COINS, "coins", 0 }, { MSN_OBJ_DUMMY, "dummy", 0 } };
  int ntypes = (int)(sizeof T / sizeof *T), bad = 0;
  snprintf(d, sizeof d, "в таблице %d типов, в перечислении %d (MSN_OBJ_COUNT)", ntypes, MSN_OBJ_COUNT); check("obj_count", ntypes == MSN_OBJ_COUNT, d);
  for (int i = 0; i < ntypes; i++) { int got = route(T[i].obj, MSN_NAV_DEFAULT, 1); if (got != T[i].want) { bad++; printf("   тип %s: стрелки %d, ждали %d\n", T[i].n, got, T[i].want); } }
  snprintf(d, sizeof d, "несовпадений по типам целей: %d (ждём 0; стрелки только у 7 типов)", bad); check("obj_types", !bad, d);
  snprintf(d, sizeof d, "goto + neither: %d, goto + nearest road: %d (ждём 0 и 0)", route(MSN_OBJ_GOTO, MSN_NAV_NEITHER, 1), route(MSN_OBJ_GOTO, MSN_NAV_NEAREST_ROAD, 1));
  check("nav_modes0", !route(MSN_OBJ_GOTO, MSN_NAV_NEITHER, 1) && !route(MSN_OBJ_GOTO, MSN_NAV_NEAREST_ROAD, 1), d);
  snprintf(d, sizeof d, "goto + both/intersection/по умолчанию: %d %d %d (ждём 1 1 1)", route(MSN_OBJ_GOTO, MSN_NAV_BOTH, 1), route(MSN_OBJ_GOTO, MSN_NAV_INTERSECTION, 1), route(MSN_OBJ_GOTO, MSN_NAV_DEFAULT, 1));
  check("nav_modes1", route(MSN_OBJ_GOTO, MSN_NAV_BOTH, 1) && route(MSN_OBJ_GOTO, MSN_NAV_INTERSECTION, 1) && route(MSN_OBJ_GOTO, MSN_NAV_DEFAULT, 1), d);
  snprintf(d, sizeof d, "переключатель Intersect Nav System выключен: %d (ждём 0)", route(MSN_OBJ_GOTO, MSN_NAV_DEFAULT, 0)); check("nav_switch", !route(MSN_OBJ_GOTO, MSN_NAV_DEFAULT, 0), d);
  // ---- реплика старта миссии
  snprintf(d, sizeof d, "уличная гонка молчит (%d), ставочная звучит (%d), обычная звучит (%d)", mvMstartMuted(MSN_MF_STREETRACE, SV_RACE_MSTART3_OLD), mvMstartMuted(MSN_MF_GAMBLE, SV_RACE_MSTART3_OLD), mvMstartMuted(0, SV_RACE_MSTART3_OLD));
  check("mstart", mvMstartMuted(MSN_MF_STREETRACE, SV_RACE_MSTART3_OLD) && !mvMstartMuted(MSN_MF_GAMBLE, SV_RACE_MSTART3_OLD) && !mvMstartMuted(0, SV_RACE_MSTART3_OLD), d);
  // ---- Skip: порог 7 на границе (6 нет, 7 есть), L7 с 5-й сюжетной нет, прочие уровни есть
  snprintf(d, sizeof d, "6 запусков: %d (ждём 0)", mvSkipAllowed(6, 3, 2, SV_SKIP7_OLD)); check("skip_6", !mvSkipAllowed(6, 3, 2, SV_SKIP7_OLD), d);
  snprintf(d, sizeof d, "7 запусков: %d (ждём 1)", mvSkipAllowed(7, 3, 2, SV_SKIP7_OLD)); check("skip_7", mvSkipAllowed(7, 3, 2, SV_SKIP7_OLD), d);
  snprintf(d, sizeof d, "7 запусков на L7 в 4-й миссии: %d (ждём 1), в 5-й: %d, в 7-й: %d (ждём 0 и 0)", mvSkipAllowed(7, 7, 4, SV_SKIP7_OLD), mvSkipAllowed(7, 7, 5, SV_SKIP7_OLD), mvSkipAllowed(20, 7, 7, SV_SKIP7_OLD));
  check("skip_l7", mvSkipAllowed(7, 7, 4, SV_SKIP7_OLD) && !mvSkipAllowed(7, 7, 5, SV_SKIP7_OLD) && !mvSkipAllowed(20, 7, 7, SV_SKIP7_OLD), d);
  snprintf(d, sizeof d, "5-я миссия на L6 (не последний уровень): %d (ждём 1)", mvSkipAllowed(7, 6, 5, SV_SKIP7_OLD)); check("skip_l6", mvSkipAllowed(7, 6, 5, SV_SKIP7_OLD), d);
  // ---- значки строки
  snprintf(d, sizeof d, "пройдена %d, попытки без пропуска %d, нет попыток %d, пропущена %d (ждём 2 1 0 0)", mvRowIcon(1, 3, 0), mvRowIcon(0, 3, 0), mvRowIcon(0, 0, 0), mvRowIcon(0, 3, 1));
  check("row_icons", mvRowIcon(1, 3, 0) == 2 && mvRowIcon(0, 3, 0) == 1 && mvRowIcon(0, 0, 0) == 0 && mvRowIcon(0, 3, 1) == 0 && mvRowIcon(1, 0, 1) == 2, d);
  // ---- мигания
  int lapOk = 1, firstHidden = -1; for (int k = 0; k < 8; k++) { int n = mvLapStep(k * .25f + .01f); if (n != k) lapOk = 0; }
  if (mvLapStep(8 * .25f + .01f) != -1 || mvLapStep(.0f) != 0) lapOk = 0; (void)firstHidden;
  snprintf(d, sizeof d, "круги: 8 шагов по 0,25 с, затем конец (%d)", lapOk); check("lap_blink", lapOk, d);
  snprintf(d, sizeof d, "Item Dropped: 0,1 с вкл %d, 0,3 с выкл %d, 0,6 с вкл %d (ждём 1 0 1)", mvPulseOn(.1f, .25f), mvPulseOn(.3f, .25f), mvPulseOn(.6f, .25f));
  check("drop_blink", mvPulseOn(.1f, .25f) && !mvPulseOn(.3f, .25f) && mvPulseOn(.6f, .25f), d);
  int load0 = SV_LOADBLINK_OLD ? mvPulseOn(.1f, .25f) : !mvPulseOn(.1f, .25f), load1 = SV_LOADBLINK_OLD ? mvPulseOn(.3f, .25f) : !mvPulseOn(.3f, .25f);
  snprintf(d, sizeof d, "LOADING: в первые 0,25 с видна %d (ждём 0), потом %d (ждём 1)", load0, load1); check("load_blink", !load0 && load1, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
