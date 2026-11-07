// №1170: сохранения не пустеют при обрыве питания. Для каждого шага записи (0..N) питание «выключается» на этом шаге, затем перезагрузка и чтение слота и настроек:
// слот обязан быть прежним целым или новым целым, остатков .tmp/.bak после чтения не остаётся; первый раз (прежнего нет) — слота нет или новый целый.
// Красный контроль: -DSAVEAT_OLD=1 (прежний порядок «удалить старое, переименовать новое») обязан показать пустой слот или остаток.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "savegame.h"
#include <pspiofilemgr.h>
#include "savefile.h"
int psp_budget = -1, psp_off = 0, psp_steps = 0;
static int fails, shown;
static char dir[64];
static void mk(SaveGame *g, unsigned seq, uint8_t out[SV_BYTES]) { svClear(g); g->seq = seq; g->coins = (int)(seq * 100); svWrite(g, out); }
static int readSeq(int *seq) {   // 0 — слота нет, 1 — годный, -1 — негодный
  static uint8_t b[SV_BYTES]; int n = svFileRead(dir, 0, b, sizeof b); if (n <= 0) return 0;
  SaveGame g; if (n != SV_BYTES || svRead(b, (unsigned)n, &g) != SV_OK) return -1; *seq = (int)g.seq; return 1;
}
static int leftovers(void) { char p[96]; int k = 0; snprintf(p, sizeof p, "%ssave00.tmp", dir); k += access(p, F_OK) == 0; snprintf(p, sizeof p, "%ssave00.bak", dir); k += access(p, F_OK) == 0; return k; }
static void clean(void) { char c[160]; snprintf(c, sizeof c, "rm -f %ssave00.* %ssettings.*", dir, dir); (void)!system(c); }
static void note(const char *what, int k, int ok) { if (!ok) { fails++; if (shown++ < 6) printf("FAIL %s: обрыв на шаге %d\n", what, k); } }
// настройки: структура-макет 40 Б + CRC-хвост; годность как cfgFileValid
static int cfgValid(const void *b, int n) { if (n != 44 || memcmp(b, "SHST", 4)) return 0; uint32_t c; memcpy(&c, (const char *)b + 40, 4); return c == svCrc32((const uint8_t *)b, 40); }
static void cfgMake(uint8_t out[44], int v) { memset(out, 0, 44); memcpy(out, "SHST", 4); out[4] = (uint8_t)v; uint32_t c = svCrc32(out, 40); memcpy(out + 40, &c, 4); }
int main(void) {
  snprintf(dir, sizeof dir, "/tmp/sv1170_%d/", (int)getpid()); { char c[96]; snprintf(c, sizeof c, "mkdir -p %s", dir); (void)!system(c); }
  SaveGame g; uint8_t a[SV_BYTES], b[SV_BYTES]; mk(&g, 1, a); mk(&g, 2, b);
  // 1) слот: прежняя запись есть, пишется новая; обрыв на каждом шаге
  int nsteps = 0;
  for (int k = 0; k < 40; k++) {
    clean(); psp_budget = -1; psp_off = 0; if (svFileWrite(dir, 0, a, SV_BYTES) != 0) { printf("FAIL: исходная запись не легла\n"); return 2; }
    psp_steps = 0; psp_budget = k; int e = svFileWrite(dir, 0, b, SV_BYTES); int done = psp_off == 0 && e == 0; psp_reboot();
    int seq = 0, st = readSeq(&seq);
    note("слот пуст или испорчен", k, st == 1 && (seq == 1 || seq == 2));
    if (done) { note("запись дошла до конца, а в слоте не новое", k, seq == 2); nsteps = k; break; }
    note("остатки .tmp/.bak после чтения", k, leftovers() == 0);
  }
  // 2) первая запись (прежнего слота нет): после обрыва слота нет или он новый целый
  for (int k = 0; k < 40; k++) {
    clean(); psp_steps = 0; psp_budget = k; int e = svFileWrite(dir, 0, b, SV_BYTES); int done = psp_off == 0 && e == 0; psp_reboot();
    int seq = 0, st = readSeq(&seq); note("первая запись: испорченный слот", k, st == 0 || (st == 1 && seq == 2)); if (done) break;
  }
  // 3) настройки (тот же svAtomicWrite/Recover, свой путь и годность)
  uint8_t c1[44], c2[44]; cfgMake(c1, 1); cfgMake(c2, 2); char sp[96]; snprintf(sp, sizeof sp, "%ssettings.bin", dir);
  for (int k = 0; k < 40; k++) {
    clean(); psp_budget = -1; if (svAtomicWrite(sp, c1, 44) != 0) return 2;
    psp_budget = k; int e = svAtomicWrite(sp, c2, 44); int done = psp_off == 0 && e == 0; psp_reboot();
    svAtomicRecover(sp, cfgValid); uint8_t r[64]; FILE *f = fopen(sp, "rb"); int n = f ? (int)fread(r, 1, sizeof r, f) : 0; if (f) fclose(f);
    note("настройки: файла нет или он испорчен", k, n == 44 && cfgValid(r, n) && (r[4] == 1 || r[4] == 2));
    if (done) break;
  }
  printf("RESULT: %s — шагов записи слота %d, провалов %d\n", fails ? "FAIL" : "OK", nsteps, fails);
  { char c[96]; snprintf(c, sizeof c, "rm -rf %s", dir); (void)!system(c); }
  return fails ? 1 : 0;
}
