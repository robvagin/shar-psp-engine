// 109: какие значки и когда просит миссия. Для каждой фазы (карточка m*): значки из LoadP3DFile load-скрипта (msnicons) и значки этапов (SetHUDIcon). Параметр: levelNN.msn
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission/msn.h"
#include "mission/msn_ops.h"
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); void *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) return 2;
  MsnFile mf; char err[128]; if (msn_open(&mf, d, n, err, sizeof err)) { printf("open: %s\n", err); return 3; }
  int loadIcons = 0, stageIcons = 0, stageIconsNotInLoad = 0;
  for (unsigned p = 0; p < mf.nphases; p++) { const MsnPhase *P = &mf.phases[p]; char loaded[16][64]; int nl = 0;
    for (unsigned k = 0; k < P->load_count; k++) { const MsnCmd *c = &mf.cmds[P->load_first + k]; if (c->op != MSN_OP_LOADP3DFILE) continue; const char *s = msn_arg_s(&mf, c, 0); if (s && strstr(s, "msnicons") && nl < 16) { snprintf(loaded[nl++], 64, "%s", s); loadIcons++; } }
    char shown[512] = ""; int ns = 0;
    for (unsigned st = P->first_stage; st < (unsigned)P->first_stage + P->stage_count; st++) { const char *ic = msn_str(&mf, mf.stages[st].icon); if (ic && *ic) { stageIcons++; int in = 0; for (int q = 0; q < nl; q++) if (strcasestr(loaded[q], ic)) in = 1; if (!in) stageIconsNotInLoad++; ns++; strncat(shown, ic, 20); strcat(shown, in ? "+ " : "! "); } }
    printf("фаза %s: значков в load-скрипте %d, у этапов %d: %s\n", msn_str(&mf, P->name), nl, ns, shown);
  }
  printf("ИТОГ значков в load-скриптах %d, значков этапов %d, этапных значков без загрузки в load-скрипте %d\n", loadIcons, stageIcons, stageIconsNotInLoad);
  return 0;
}
