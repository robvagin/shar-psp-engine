// №1105: таблица структуры уровней для SCRAP BOOK (viewer/scrapdata_tab.h): какие миссии уровня сюжетные / бонусная / гонки (номер бита в маске пройденных = индекс в файле
// levelNN.msn), награды уровня (BindReward по порядку = номер бита в Coins.owned), сколько гэгов (SetTotalGags). Только числа и имена; читает собранные build/levels/lN/levelNN.msn.
//   cc -std=c11 -I viewer -I viewer/mission tools/scrap_tab.c viewer/mission/msn_file.c viewer/mission/msn_ops.c -o build/scrap_tab && build/scrap_tab <папка build/levels> > viewer/scrapdata_tab.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mission/msn.h"
#include "mission/msn_format.h"
#include "mission/msn_ops.h"
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  printf("// СГЕНЕРИРОВАНО tools/scrap_tab.c (№1105): структура уровней для альбома. Не править руками.\n");
  printf("#ifndef SCRAPDATA_TAB_H\n#define SCRAPDATA_TAB_H\n");
  printf("typedef struct { signed char story[7]; signed char bonus; unsigned nrew, totalGags; unsigned races;\n  struct { unsigned char car, quest; char name[12]; } rew[12]; } ScrapLvTab;\n");
  printf("// quest: 0 defaultcar/defaultskin, 1 bonusmission, 2 streetrace, 3 forsale\nstatic const ScrapLvTab scrapTab[8] = {\n  {{-1,-1,-1,-1,-1,-1,-1}, -1, 0, 0, 0, {{0, 0, \"\"}}},\n");
  for (int L = 1; L <= 7; L++) {
    char p[512]; snprintf(p, sizeof p, "%s/l%d/level%02d.msn", argv[1], L, L);
    FILE *fp = fopen(p, "rb"); if (!fp) { fprintf(stderr, "нет %s\n", p); return 1; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); rewind(fp);
    unsigned char *b = aligned_alloc(16, ((size_t)n + 15) & ~15u); if (fread(b, 1, (size_t)n, fp) != (size_t)n) return 1; fclose(fp);
    MsnFile f; char err[160]; if (msn_open(&f, b, (size_t)n, err, sizeof err)) { fprintf(stderr, "%s: %s\n", p, err); return 1; }
    int story[32], ns = 0, bonus = -1; unsigned races = 0;
    for (unsigned i = 0; i < f.nmissions; i++) {
      unsigned fl = f.missions[i].flags;
      if (fl & MSN_MF_STREETRACE) races |= 1u << i;
      else if (fl & MSN_MF_BONUSMISSION) bonus = (int)i;
      else if (!(fl & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) && ns < 32) story[ns++] = (int)i;
    }
    int tg = 0; const MsnLevelInfo *lv = f.level;
    for (uint32_t i = 0; lv && i < lv->rew_count; i++) { const MsnCmd *c = &f.cmds[lv->rew_first + i]; if (c->op == MSN_OP_SETTOTALGAGS && msn_argc(c) > 1) tg = msn_arg_i(&f, c, 1); }
    printf("  {{");
    for (int k = 0; k < 7; k++) { int i = ns - 7 + k; printf("%d%s", i >= 0 ? story[i] : -1, k < 6 ? "," : ""); }
    printf("}, %d, %d, %d, 0x%x, {", bonus, msn_reward_count(&f), tg, races);
    for (int k = 0, nr = msn_reward_count(&f); k < nr && k < 12; k++) {
      MsnReward r; if (!msn_reward_get(&f, k, &r)) continue;
      int q = !strcmp(r.quest, "bonusmission") ? 1 : !strcmp(r.quest, "streetrace") ? 2 : !strcmp(r.quest, "forsale") ? 3 : 0;
      printf("{%d, %d, \"%.11s\"}%s", r.kind == MSN_OWN_CAR ? 1 : 0, q, r.name, k + 1 < nr ? ", " : "");
    }
    printf("}},   // L%d: миссий в файле %u\n", L, f.nmissions);
    free(b);
  }
  printf("};\n#endif\n");
  return 0;
}
