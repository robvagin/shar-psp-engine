// ps2-twin P05:25 (g07b-msnview): «занято» у будки. Чистая логика viewer/phonebusy.h + данные миссий диска (build/missions/levelNN.msn).
// Красный контроль: -DPHONEBUSY_OLD=1 (будки всегда включены, гудка нет) обязан упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msn.h"
#include "phonebusy.h"
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-10s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static int runningOf(int st) { return st == MSN_RUNNING || st == MSN_COUNTDOWN || st == MSN_BETWEEN; }   // то же множество, что в game_msn.c gameMsnBoothsOff
int main(int argc, char **argv) {
  char d[260];
  // 1. логика входа в объём
  PbArm a = {-1, 0};
  pbStep(&a, 3, 1);
  check("on", pbPress(&a, 3, 1) == PB_OPEN && pbHint(&a, 3, 1), "вошёл при включённых: △ открывает, подсказка есть");
  check("busy", pbPress(&a, 3, 0) == PB_BUSY && !pbHint(&a, 3, 0), "будки выключились, пока стоишь внутри: △ — гудок, не открывает, подсказки нет");
  check("busy2", pbPress(&a, 3, 0) == PB_BUSY, "гудок на каждое нажатие (один звук за △), состояние не съедается");
  pbStep(&a, -1, 0); check("out", pbPress(&a, -1, 0) == PB_NONE && pbPress(&a, -1, 1) == PB_NONE, "вне будки △ молчит и при выключенных, и при включённых");
  pbStep(&a, 3, 0);
  check("silent", pbPress(&a, 3, 0) == PB_NONE && !pbHint(&a, 3, 0), "вошёл при выключенных: молча, подсказки нет");
  pbStep(&a, 3, 1);
  check("reenable", pbPress(&a, 3, 1) == PB_NONE && !pbHint(&a, 3, 1), "включились, пока стоишь внутри: всё равно молча (нужно выйти и войти)");
  pbStep(&a, -1, 1); pbStep(&a, 3, 1);
  check("reenter", pbPress(&a, 3, 1) == PB_OPEN && pbHint(&a, 3, 1), "вышел и вошёл при включённых: открывает");
  pbStep(&a, 5, 0);
  check("other", a.k == 5 && !a.armed && pbPress(&a, 5, 0) == PB_NONE, "шагнул в объём другой будки при выключенных: молча");
  // 2. когда будки выключены: миссия с принудительной машиной в ходе
  int forced = 0, plain = 0, bad = 0;
  for (int i = 1; i < argc; i++) {
    FILE *fp = fopen(argv[i], "rb"); if (!fp) return 2; fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    void *buf = malloc((size_t)n + 16); if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2; fclose(fp);
    MsnFile F; char err[200]; if (msn_open(&F, buf, (size_t)n, err, sizeof err)) return 2;
    for (uint32_t m = 0; m < F.nmissions; m++) {
      int ff = (F.missions[m].flags & MSN_MF_FORCEDCAR) != 0; if (ff) forced++; else plain++;
      for (int st = MSN_IDLE; st <= MSN_LEVEL_DONE; st++) {
        int want = ff && runningOf(st), got = pbMissionOff((int)m, ff, runningOf(st));
        if (want != got) { bad++; if (bad < 4) printf("  %s st=%d: ждали off=%d, есть %d\n", msn_str(&F, F.missions[m].id), st, want, got); }
      }
    }
  }
  check("missions", bad == 0, "выключены ровно миссии с флагом FORCEDCAR в ходе (идёт / отсчёт / между этапами); пройдена, провалена, пусто — включены");
  snprintf(d, sizeof d, "миссий с принудительной машиной на диске: %d (ждём ≥ 5), без неё: %d", forced, plain); check("coverage", forced >= 5 && plain >= 5, d);
  check("nomission", !pbMissionOff(-1, 1, 1), "нет текущей миссии — будки включены");
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
