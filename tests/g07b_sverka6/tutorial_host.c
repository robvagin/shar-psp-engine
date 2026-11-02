// Окно g07b-msnview, круг 6: подсказки обучения (настоящий viewer/tutorial.c, заглушки как в tests/tutorial/exit_host.c).
// P15-11: очередь подсказок идёт в порядке прихода событий (а не по номеру подсказки). P15-10: угон машины трафика — если «в свою машину» ещё не была, показывается она, а «в чужую»
// считается показанной; если была — один раз «в чужую»; «газ» на посадку в угнанную не показывается. Красный контроль: -DTUT_QORDER_OLD=1 и -DTUT_FOREIGN_OLD=1.
#include <stdio.h>
#include <string.h>
#include <pspctrl.h>
#include "tutorial.h"
#include "ui.h"
int audioTrack(const char *n) { (void)n; return -1; }
int sndMode(int m) { (void)m; return 0; } int sndModeCur(void) { return 0; } void sndVoice(int v) { (void)v; } int sndVoicePlaying(int v) { (void)v; return 0; }
int uiDrawTutorial(int msg, int cont) { (void)msg; (void)cont; return 1; }
void uiTutorialScale(float k) { (void)k; }
void uiPopHookFire(int ev) { (void)ev; }
void uiTutLayoutOld(int on) { (void)on; }
int uiTutorialCount(void) { return 30; }
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
// крутим игру, пока подсказки показываются; закрываем каждую ✕ после реплики; -> порядок показа (номера подсказок)
static int drain(int *order, int max) {
  int n = 0; unsigned seen = tutShown();
  for (int it = 0; it < 4000 && n < max; it++) {
    tutTick(.05f); int d = 0;
    if (tutActive()) { tutInput(0, 2.1f, &d); tutInput(PSP_CTRL_CROSS, 1.f / 60, &d); }
    unsigned now = tutShown(); unsigned add = now & ~seen;
    for (int e = 0; e < TUT_COUNT; e++) if (add >> e & 1u) order[n++] = e;
    seen = now;
  }
  return n;
}
int main(void) {
  char d[240]; int ord[16];
  // ---- P15-11: три события пришли, пока шёл разговор: WRENCH, COINS, BREAK (по номерам было бы BREAK, COINS, WRENCH; «рядом с …» в очередь не встаёт)
  tutReset(1); tutSetBusy(1); tutEvent(TUT_WRENCH); tutEvent(TUT_COINS); tutEvent(TUT_BREAK); tutSetBusy(0);
  int n = drain(ord, 8);
  snprintf(d, sizeof d, "порядок показа: %d %d %d (ждём WRENCH=%d COINS=%d BREAK=%d)", n > 0 ? ord[0] : -1, n > 1 ? ord[1] : -1, n > 2 ? ord[2] : -1, TUT_WRENCH, TUT_COINS, TUT_BREAK);
  check("queue_order", n == 3 && ord[0] == TUT_WRENCH && ord[1] == TUT_COINS && ord[2] == TUT_BREAK, d);
  // ---- P15-10 (а): «в свою машину» ещё не показана, угнали чужую → показывается «в свою», «в чужая» помечена показанной
  tutReset(1); tutForeignCar();
  snprintf(d, sizeof d, "угон до «в свою»: на экране подсказка (%d), показана «в свою» (%d), «в чужую» ещё ждёт (%d); ждём 1 1 0", tutActive(), (tutShown() >> TUT_GETIN) & 1, tutPending(TUT_FOREIGN));
  check("foreign_first", tutActive() && (tutShown() >> TUT_GETIN & 1) && !tutPending(TUT_FOREIGN), d);
  n = drain(ord, 8); tutForeignCar(); n = drain(ord, 8);
  snprintf(d, sizeof d, "второй угон ничего не показывает (подсказок после него: %d, ждём 0)", n); check("foreign_twice", n == 0 && !tutActive(), d);
  // ---- P15-10 (б): «в свою» уже была → один раз «в чужую»
  tutReset(1); tutEvent(TUT_GETIN); drain(ord, 8); tutForeignCar();
  snprintf(d, sizeof d, "угон после «в свою»: показана «в чужую» (%d, ждём 1)", (tutShown() >> TUT_FOREIGN) & 1); check("foreign_second", (tutShown() >> TUT_FOREIGN & 1) && tutActive(), d);
  drain(ord, 8); tutForeignCar(); n = drain(ord, 8);
  snprintf(d, sizeof d, "и больше не повторяется (подсказок %d, ждём 0)", n); check("foreign_once", n == 0, d);
  // ---- «газ» на посадку в угнанную не показывается; на следующую (свою) — показывается
  tutReset(1); tutEvent(TUT_GETIN); drain(ord, 8); tutForeignCar(); drain(ord, 8); tutEvent(TUT_GAS);
  snprintf(d, sizeof d, "после угона «газ» не показан (%d, ждём 0)", (tutShown() >> TUT_GAS) & 1); check("gas_skipped", !(tutShown() >> TUT_GAS & 1), d);
  tutEvent(TUT_GAS);
  snprintf(d, sizeof d, "на следующую посадку «газ» показан (%d, ждём 1)", (tutShown() >> TUT_GAS) & 1); check("gas_next", (tutShown() >> TUT_GAS & 1) != 0, d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
