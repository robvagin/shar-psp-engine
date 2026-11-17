// Ф2-7 (109, Стена): перепись — миссии (level0N.msn), где этап с DisableHitAndRun сменяется этапом без него в той же фазе (оригинал снимает запрет на конце этапа, мы держим до конца миссии).
// Сборка: cc -std=gnu99 -O1 -w -Iviewer -Iviewer/mission tests/stena109/hnr_census.c viewer/mission/msn_file.c viewer/mission/msn_ops.c -lm -o /tmp/hnr_census && /tmp/hnr_census level01.msn ...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission/msn.h"
int main(int argc, char **argv) {
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) { fclose(f); continue; } fclose(f);
    static MsnFile F; char err[96]; if (msn_open(&F, b, n, err, sizeof err)) { printf("%s: %s\n", argv[a], err); continue; }
    int with = 0, gaps = 0;
    for (unsigned m = 0; m < F.nmissions; m++) for (int w = 0; w < 2; w++) { unsigned pi = w ? F.missions[m].main : F.missions[m].intro; if (pi == MSN_NONE16 || pi >= F.nphases) continue; const MsnPhase *ph = &F.phases[pi]; int seen = 0;
      for (unsigned q = 0; q < ph->stage_count; q++) { const MsnStage *sg = &F.stages[ph->first_stage + q]; int has = 0;
        for (unsigned c = 0; c < sg->cmd_count; c++) if (F.cmds[sg->cmd_first + c].op == MSN_OP_DISABLEHITANDRUN) has = 1;
        if (has) { with++; seen = 1; } else if (seen) { gaps++; printf("%s: миссия %s (индекс %u) фаза %d: этап %u без команды после этапа с DisableHitAndRun\n", argv[a], msn_str(&F, F.missions[m].id), m, w, q); seen = 0; } } }
    printf("%s: этапов с DisableHitAndRun %d, мест 'после него этап без команды' %d\n", argv[a], with, gaps);
  }
  return 0;
}
