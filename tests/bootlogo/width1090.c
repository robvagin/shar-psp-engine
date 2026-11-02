// №1090: строки верхнего блока экрана загрузки не заходят за правый край колонки текста (BUI_TEXT_R) — Мак
#include <stdio.h>
#include <string.h>
#include "bootseq.h"
static unsigned scr[BUI_H][BUI_W];
static void sput(void *u, int x, int y, unsigned c) { (void)u; if (x >= 0 && x < BUI_W && y >= 0 && y < BUI_H) scr[y][x] = c; }
int main(void) {
  BootSys s; memset(&s, 0, sizeof s);
  int len = bootSeqLen(); for (int n = 0; n <= len; n++) bootSeqDraw(n, &s, sput, 0);
  int bad = 0;
  for (int row = BSQ_ROW0; row < BSQ_ROW0 + 8; row++) {
    int y0 = BUI_Y0 + row * BUI_CH, mx = -1;
    for (int y = y0; y < y0 + BUI_CH && y < BUI_H; y++) for (int x = 0; x < BUI_LOGO_X; x++) if (scr[y][x] && scr[y][x] != BUI_BG) mx = x > mx ? x : mx;
    int over = 0; for (int y = y0; y < y0 + BUI_CH && y < BUI_H; y++) for (int x = BUI_TEXT_R + 1; x < BUI_LOGO_X; x++) if (scr[y][x] && scr[y][x] != BUI_BG) over = 1;
    printf("ряд %d: правый край текста x=%d (предел %d)%s\n", row, mx, BUI_TEXT_R, over ? "  ЗА КРАЕМ" : ""); bad += over;
  }
  printf("RESULT: %s\n", bad ? "FAIL" : "OK"); return bad != 0;
}
