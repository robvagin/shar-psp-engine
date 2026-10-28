// №1037: перенос текстов заданий до/после (чистая функция uiWrap из viewer/ui_layout.h, ширины букв pix8 как в tests/ui/objective_text.py). host MAXW MODE < texts.txt; MODE 0 — прежнее, 1 — №1037
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "advtab.h"
#include "../../viewer/ui_layout.h"
static int lwf(const void *ctx, const char *s, int n) { (void)ctx; int w = 0; for (int i = 0; i < n; i++) { unsigned char c = (unsigned char)s[i]; w += ADV[c < 128 ? c : 63]; } return w; }
int main(int argc, char **argv) {
  int maxw = atoi(argv[1]); uiWrap1037 = atoi(argv[2]); char line[512];
  while (fgets(line, sizeof line, stdin)) {
    char *tab = strchr(line, '\t'); if (!tab) continue; *tab = 0; char *t = tab + 1; t[strcspn(t, "\n")] = 0;
    int st[16], ln[16]; int wf = 1 | UI_WRAP_NOWIDOW | UI_WRAP_BALANCE; int nl = uiWrap(t, maxw, wf, lwf, 0, st, ln, 16);
    int mw = 0; for (int i = 0; i < nl; i++) { int w = lwf(0, t + st[i], ln[i]); if (w > mw) mw = w; }   // как ui.c fitW (закон v1): ширина = самая широкая строка первого переноса, и текст раскладывается ещё раз по ней (textLayout)
    if (mw > 0 && mw < maxw) nl = uiWrap(t, mw, wf, lwf, 0, st, ln, 16);
    printf("%s\t%d\t", line, nl);
    for (int i = 0; i < nl; i++) { printf("%s%.*s", i ? " / " : "", ln[i], t + st[i]); }
    printf("\n");
  }
  return 0;
}
