// Гейт A1.8 / A1.11 / A1.12 (Бабл 107): чистые функции viewer/mission/msn_cosm1264.h, те же, что зовут msn_run.c и msn_view.c. old=0 — новое, old=1 — прежнее (красный контроль должен давать другой ответ).
#include <stdio.h>
#include <string.h>
#include "mission/msn_cosm1264.h"
int msnCosm1264Old;
static int fails;
#define CK(c, w) do { int ok_ = (c); printf("  %-4s %s\n", ok_ ? "ok" : "FAIL", w); fails += !ok_; } while (0)
int main(void) {
  CK(mvTimerDur1264(0.5f, 0) == 0.f, "A1.11 новое: SetDurationTime(0.5) читается как 0 с");
  CK(mvTimerDur1264(0.5f, 1) == 0.5f, "A1.11 красный (old): 0,5 с");
  CK(mvTimerDur1264(7.f, 0) == 7.f && mvTimerDur1264(2.9f, 0) == 2.f, "A1.11 целые секунды не трогает, дробь вниз");
  CK(mvBeyond1264(150.5f, 150.f, 0) == 1, "A1.12 новое: 150,5 м при пределе 150 — за пределом");
  CK(mvBeyond1264(150.5f, 150.f, 1) == 0, "A1.12 красный (old): сравнение целых частей — ещё нет");
  CK(mvBeyond1264(150.f, 150.f, 0) == 0 && mvBeyond1264(149.9f, 150.f, 0) == 0 && mvBeyond1264(151.f, 150.f, 1) == 1, "A1.12 границы: ровно 150 и меньше — нет; 151 старое — да");
  CK(!strcmp(mvDumpArrow1264(5, 0), "arrow_chase"), "A1.8 новое: dump с привязками — arrow_chase (как у follow)");
  CK(!strcmp(mvDumpArrow1264(5, 1), "arrow_destroy"), "A1.8 красный (old): arrow_destroy");
  CK(!strcmp(mvDumpArrow1264(0, 0), "arrow_destroy"), "A1.8 без привязок — arrow_destroy");
  printf("RESULT %s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
