// №1195 (g01-cars): проверка таблиц level.shr (viewer/lvcheck.h): настоящие файлы всех уровней — цело; подменённые таблицы — каждая порча поймана. Сборка и прогон: tests/g01cars/lvcheck.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvcheck.h"
static int fails, skipped;
#define CHECK(name, cond) do { int ok_ = !!(cond); printf("%s  %s\n", ok_ ? "OK  " : "FAIL", name); if (!ok_) fails++; } while (0)
static void put(unsigned char *b, unsigned off, unsigned v) { memcpy(b + off, &v, 4); }
int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "";
  static const char *names[7] = {"level.shr", "l2/level.shr", "l3/level.shr", "l4/level.shr", "l5/level.shr", "l6/level.shr", "l7/level.shr"};
  unsigned char *sh = 0, *st = 0, *sc = 0; unsigned sfs = 0;   // L1: образец для подмен
  for (int L = 0; L < 7; L++) {
    char fn[512]; snprintf(fn, sizeof fn, "%s/%s", dir, names[L]); FILE *f = fopen(fn, "rb"); if (!f) { printf("ПРОПУСК  нет файла %s\n", fn); skipped++; continue; }
    unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) { fclose(f); continue; }
    fseek(f, 0, SEEK_END); unsigned fs = (unsigned)ftell(f);
    unsigned npc = lvcU(hdr + 8), ncell = lvcU(hdr + 80), tOff = lvcU(hdr + 76), cOff = lvcU(hdr + 84);
    unsigned char *tab = malloc(80u * npc), *ct = malloc(48u * ncell); fseek(f, tOff, SEEK_SET); fread(tab, 80, npc, f); fseek(f, cOff, SEEK_SET); fread(ct, 48, ncell, f); fclose(f);
    const char *why; int r = lvCheckTables(hdr, tab, ct, fs, &why); char nm[100]; snprintf(nm, sizeof nm, "настоящий %s: таблицы целы (кусков %u, клеток %u)", names[L], npc, ncell);
    CHECK(nm, r == 0); if (r) printf("   причина: %s\n", why);
    if (L == 0) { sh = malloc(104); memcpy(sh, hdr, 104); st = tab; sc = ct; sfs = fs; } else { free(tab); free(ct); }
  }
  if (!sh) { printf("ПРОПУСК  подмены: нет level.shr L1 для образца\n"); return skipped ? 77 : 1; }
  unsigned npc = lvcU(sh + 8), ncell = lvcU(sh + 80), ntex = lvcU(sh + 4); const char *why;
  // подмены (каждая — копия L1 с одной порчей)
  #define MUT(label, code) do { unsigned char *h = malloc(104), *t = malloc(80u * npc), *c = malloc(48u * ncell); memcpy(h, sh, 104); memcpy(t, st, 80u * npc); memcpy(c, sc, 48u * ncell); unsigned fs = sfs; code; \
      int r = lvCheckTables(h, t, c, fs, &why); char nm_[120]; snprintf(nm_, sizeof nm_, "порча поймана: %s (%s)", label, r ? why : "НЕ поймана"); CHECK(nm_, r != 0); free(h); free(t); free(c); } while (0)
  MUT("текстура куска за таблицей", put(t, 80u * 100 + 24, ntex + 7));
  MUT("текстура куска −5", put(t, 80u * 100 + 24, (unsigned)-5));
  MUT("клетка куска за таблицей", put(t, 80u * 200 + 68, ncell + 3));
  MUT("диапазон кусков клетки за таблицей", put(c, 48u * 10 + 28, npc));
  MUT("блоб клетки за концом файла", put(c, 48u * 10 + 36, fs));
  MUT("смещение куска за блобом клетки", put(t, 80u * (lvcU(c + 24) ) + 60, 0x7fffffffu));
  MUT("число вершин куска отрицательное", put(t, 80u * 50 + 32, (unsigned)-1));
  MUT("таблицы кусков налезают на клетки", put(h, 8, npc + 100000));
  MUT("клеток больше, чем влезет в файл", put(h, 80, 60000));
  MUT("текстур слишком много", put(h, 4, 100000));
  MUT("файл обрезан (клетки за концом)", fs = lvcU(h + 84) + 100);
  // msnart.shr: настоящие файлы целы, порча заголовка поймана
  { static const char *mn[7] = {"msnart.shr", "l2/msnart.shr", "l3/msnart.shr", "l4/msnart.shr", "l5/msnart.shr", "l6/msnart.shr", "l7/msnart.shr"}; unsigned rowSize = 96; int good = 0, seen = 0;
    for (int L = 0; L < 7; L++) { char fn[512]; snprintf(fn, sizeof fn, "%s/%s", dir, mn[L]); FILE *f = fopen(fn, "rb"); if (!f) continue; unsigned h[4]; if (fread(h, 4, 4, f) != 4) { fclose(f); continue; } fseek(f, 0, SEEK_END); long fs = ftell(f); fclose(f); seen++; good += lvMsaHdrOk(h, fs, rowSize); }
    char nm[100]; snprintf(nm, sizeof nm, "настоящие msnart.shr: %d из %d целы", good, seen); CHECK(nm, seen > 0 && good == seen);
    unsigned hb[4] = {0x3241534Du, 0xffffffffu, 16u, 0}; CHECK("порча msnart.shr: число строк огромное", !lvMsaHdrOk(hb, 100000, rowSize));
    unsigned hc[4] = {0x3241534Du, 100u, 99950u, 0}; CHECK("порча msnart.shr: таблица за концом файла", !lvMsaHdrOk(hc, 100000, rowSize)); }
  // красный контроль: -DLVCHECK1195_OLD=1 — проверка выключена, все порчи пройдут, этот тест падает
  printf(fails ? "ПРОВАЛ: %d\n" : "RESULT ok\n", fails); return fails != 0;
}
