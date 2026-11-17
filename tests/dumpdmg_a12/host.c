// A1.2: по данным ВСЕХ этапов dump уровней 1–7 (число ADDCOLLECTIBLE и BINDCOLLECTIBLETO) решение о неуязвимости такое, как в оригинале:
// с привязками — машина игрока бьётся, цель неуязвима; без привязок — машина игрока неуязвима при любом числе предметов, цель неуязвима при нескольких.
// Красный контроль: -DGAME_A12_DUMPDMG_OLD=1 (по числу предметов). Печатает таблицу этапов.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../viewer/mission/msn_run.c"
#include "../../viewer/mission/dumpdmg.h"
int main(int ac, char **av) {
  int bad = 0, n = 0;
  for (int a = 1; a < ac; a++) {
    FILE *f = fopen(av[a], "rb"); if (!f) { printf("нет %s\n", av[a]); return 2; } fseek(f, 0, SEEK_END); size_t sz = ftell(f); rewind(f);
    void *b = aligned_alloc(16, (sz + 15) & ~15ul); if (fread(b, 1, sz, f) != sz) return 2; fclose(f);
    static MsnFile mf; char err[128]; if (msn_open(&mf, b, sz, err, sizeof err)) { printf("open %s: %s\n", av[a], err); return 2; }
    for (uint32_t si = 0; si < mf.nstages; si++) {
      const MsnStage *st = &mf.stages[si]; if (st->obj != MSN_OBJ_DUMP) continue;
      int nitems = 0, nbind = 0;
      for (unsigned i = 0; i < st->cmd_count; i++) { int op = mf.cmds[st->cmd_first + i].op; if (op == MSN_OP_ADDCOLLECTIBLE) nitems++; else if (op == MSN_OP_BINDCOLLECTIBLETO) nbind++; }
      int wantP = nbind == 0, wantT = nbind > 0 || nitems > 1;   // оригинал
      int gotP = dumpPlayerImmune(nbind, nitems), gotT = dumpTargetImmune(nbind, nitems);
      n++; int ok = gotP == wantP && gotT == wantT; if (!ok) bad++;
      printf("%s L%d этап %3u предметов %2d привязок %2d: игрок неуязвим %d (оригинал %d), цель неуязвима %d (оригинал %d)\n", ok ? "ok  " : "FAIL", mf.hdr->level, si, nitems, nbind, gotP, wantP, gotT, wantT);
    }
  }
  printf("%s: этапов dump %d, расхождений с оригиналом %d\n", GAME_A12_DUMPDMG_OLD ? "old" : "new", n, bad); return bad ? 1 : 0;
}
