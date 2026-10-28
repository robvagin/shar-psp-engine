// Проверка анимации знака на заставке (пункт Л, правка Роба 28.09 вечер): тот же viewer/bootanim.h + bootseq.h, что на
// PSP, кадры в память на Маке.
//   1) у каждой клетки 4×4 в первом проходе стадии строго / → | → \ → ■ → рисунок (без пропусков и возвратов)
//   2) символы тонкие: / | \ по 3 px толщиной 1 px, ■ — точка 1 px и по охвату меньше чёрточек,
//      все четыре разной формы; строка 0 и столбец 0 клетки у символов пустые (зазор сетки)
//   3) «не сливаются»: ни в одном кадре первого прохода пиксель символа не касается (8 соседей) ни одного пикселя
//      соседней клетки — ни чужого символа, ни уже собранного знака
//   4) в конце знак попиксельно равен bootlogo.h 1:1 (115×39), вне клеток знака ничего; второй проход — тот же знак
//   5) раскладка экрана: место знака на сетке колонок текста (x - BUI_X0 кратно BUI_CW), верх по строке заголовка,
//      правое поле = левому (с 30.09: в пределах клетки шрифта; зазоры между строками точек знака — фон); ход экрана bootSeqDraw: итоговый кадр (пропуск START) = последнему кадру хода (без прыжков)
//   6) правка 125: шрифт — msx PSPSDK 8×8, 1 бит: каждый пиксель текста ровно фон или цвет текста, попиксельно = биты
//      msx с шагом 8; сетка 60×34
//   7) правка 127: знаки причастных (viewer/bootmarks.h) — 6 слотов 144×40 на сетке 8 внутри полей 8, не пересекают друг
//      друга, знак GONE, проверки и подвал; разбор BMK1 (испорченный crc — отказ); итог каждого режима попиксельно = битмапу
//      1:1; развёртка/растворение/палитра/перо — горящих пикселей не убывает; режимы попарно разные на середине; бюджет —
//      знаки рисуют ≤ 2 × 144×40 put за кадр; последний знак успевает до конца хода
// Красное: -DBSQ_MARK_PLANT (слот 3 на знаке GONE) -> FAIL; -DBOOTMARK_ALWAYS_REDRAW (все знаки каждый кадр) -> FAIL;
//          -DBUI_CW=6 (прежний шаг клетки, знаки налезают, сетка не 60) -> FAIL;
//          -DBOOTANIM_GLYPHS='{0x054,0x092,0x111,0x01B}' (прежний ■ 2×2, «толстый») -> FAIL;
//          -DBOOTANIM_GOFS=0 (зазор клетки справа-снизу: символы касаются уже собранного знака) -> FAIL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bootseq.h"
#define FW 120
#define FH 44
static unsigned fb[FH][FW], end0[FH][FW];
// правки Роба 98–101: знак рисуется ×BOOTANIM_SCALE точками BOOTANIM_DOT_W×BOOTANIM_DOT_H — проверки 1–4 идут по пикселю знака:
// левый верхний пиксель каждого блока (точка), остальные пиксели блока — зазор (фон или продолжение той же точки)
static int gapBad;
static void put(void *u, int x, int y, unsigned c) {
  (void)u; int sx = x % BOOTANIM_SCALE, sy = y % BOOTANIM_SCALE; x /= BOOTANIM_SCALE; y /= BOOTANIM_SCALE;
  if (x < 0 || x >= FW || y < 0 || y >= FH) return;
  if (!sx && !sy) { fb[y][x] = c; return; }
  if (sy >= BOOTANIM_DOT_H || sx >= BOOTANIM_DOT_W) { if (c != 0xFF000000u) gapBad++; }   // зазор между строками точек — всегда фон
}
static int lit(int x, int y) { return x >= 0 && y >= 0 && x < FW && y < FH && fb[y][x] != 0xFF000000u; }
static unsigned scr[BUI_H][BUI_W];
static void sput(void *u, int x, int y, unsigned c) { (void)u; if (x >= 0 && x < BUI_W && y >= 0 && y < BUI_H) scr[y][x] = c; }
// правка 127: 6 пробных знаков (все 4 индекса, разные размеры) упаковкой BMK1 -> bootMarksParse
#define HOST_MARKS 8   // синтетика BMK1: восемь знаков-слотов (BMK_MAX движка с правки 397 больше — кадры покоя)
static unsigned char bmkFile[HOST_MARKS * (36 + BMK_SLOT_W * BMK_SLOT_H / 4) + 16]; static BootMarks bmk;
static unsigned bmkBuild(void) {
  unsigned char *b = bmkFile; int p = 12; memcpy(b, "BMK1", 4); b[4] = HOST_MARKS; b[5] = 0; b[6] = BMK1_SLOT_W; b[7] = 0; b[8] = BMK1_SLOT_H; b[9] = 0; b[10] = b[11] = 0;
  for (int k = 0; k < HOST_MARKS; k++) {
    int w = BMK1_SLOT_W - k * BMK1_SLOT_W / (2 * HOST_MARKS), h = BMK1_SLOT_H - k * BMK1_SLOT_H / (2 * HOST_MARKS);   // правка 148: слоты BMK1 64×24 (синтетика режимов)
    b[p] = (unsigned char)w; b[p + 1] = 0; b[p + 2] = (unsigned char)h; b[p + 3] = 0; b[p + 4] = 2; b[p + 5] = (unsigned char)(k % BMK_NMODE);
    memset(b + p + 6, 0, 14); b[p + 6] = (unsigned char)('a' + k);
    unsigned pal[4] = {0xFF000000u, 0xFF505050u, 0xFF9A9A9Au, 0xFFE0E0E0u};
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) b[p + 20 + 4 * i + j] = (unsigned char)(pal[i] >> (8 * j));
    p += 36; int nb = (w * h + 3) / 4; memset(b + p, 0, nb);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { int v = (x / 5 + y / 3 + k) % 5; v = v > 3 ? 0 : v; int i = y * w + x; b[p + (i >> 2)] |= (unsigned char)(v << (6 - 2 * (i & 3))); }
    p += nb;
  }
  unsigned c = bmkCrc32(b, p); for (int j = 0; j < 4; j++) b[p + j] = (unsigned char)(c >> (8 * j));
  return (unsigned)p + 4;
}
static int rectHit(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) { return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah; }
int main(void) {
  int bmkLen = (int)bmkBuild(), parsed = bootMarksParse(bmkFile, bmkLen, &bmk);
  bmkFile[20] ^= 1; int badRej = bootMarksParse(bmkFile, bmkLen, &bmk) < 0; bmkFile[20] ^= 1; bootMarksParse(bmkFile, bmkLen, &bmk);
  bootSeqMarks = &bmk;
  static signed char seq[BOOTANIM_BH][BOOTANIM_BW][8]; static int nseq[BOOTANIM_BH][BOOTANIM_BW];
  for (int i = 0; i < FH; i++) for (int j = 0; j < FW; j++) fb[i][j] = 0xFF000000u;
  int frames = 0, outside = 0, touch = 0, touchFr = -1;
  for (int pass = 0; pass < 2; pass++)
    for (float front = -BOOTANIM_WIDTH; front < bootAnimSpan() + BOOTANIM_WIDTH; front += BOOTANIM_SPEED, frames++) {
      bootAnimFrame(pass, front, 0, 0, 0xFF000000u, put, 0);
      for (int by = 0; by < BOOTANIM_BH; by++) for (int bx = 0; bx < BOOTANIM_BW; bx++) {
        if (!bootAnimRest(bx, by)) { for (int b = 0; b < 16; b++) outside += lit(4 * bx + b % 4, 4 * by + b / 4); continue; }
        if (pass) continue;
        unsigned c; int k = bootAnimCell(0, front, bx, by, &c);
        if (k >= 0 && (!nseq[by][bx] || seq[by][bx][nseq[by][bx] - 1] != k) && nseq[by][bx] < 8) seq[by][bx][nseq[by][bx]++] = (signed char)k;
        if (k < 0 || k >= BOOTANIM_NSYM) continue;
        for (int b = 0; b < 16; b++) {   // пиксель символа -> соседи вне своей клетки должны быть пустыми
          int x = 4 * bx + b % 4, y = 4 * by + b / 4; if (!lit(x, y)) continue;
          for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy; if (nx / 4 == bx && ny / 4 == by && nx >= 0 && ny >= 0) continue;
            if (lit(nx, ny)) { touch++; if (touchFr < 0) touchFr = frames; }
          }
        }
      }
      if (pass == 0 && front + BOOTANIM_SPEED >= bootAnimSpan() + BOOTANIM_WIDTH) memcpy(end0, fb, sizeof fb);
    }
  int badSeq = 0, blocks = 0, badPix = 0, px = 0;
  for (int by = 0; by < BOOTANIM_BH; by++) for (int bx = 0; bx < BOOTANIM_BW; bx++) if (bootAnimRest(bx, by)) {
    blocks++; int ok = nseq[by][bx] == BOOTANIM_NSYM + 1; for (int k = 0; ok && k <= BOOTANIM_NSYM; k++) ok = seq[by][bx][k] == k; badSeq += !ok; }
  for (int y = 0; y < FH; y++) { int n = y < BOOTLOGO_H ? (int)strlen(bootLogo[y]) : 0;
    for (int x = 0; x < FW; x++) { int want = x < n && bootLogo[y][x] == '#'; px += want; badPix += want != lit(x, y); } }
  // символы
  int thin = 1, gutter = 1, distinct = 1, smaller = 1;
  for (int k = 0; k < 3; k++) if (__builtin_popcount(bootAnimGlyph[k]) != 3) thin = 0;
  if (__builtin_popcount(bootAnimGlyph[3]) > 1) thin = 0;   // Роб 28.09 21:00: ■ — одна точка (2×2 «в строке толстые»)
  for (int k = 0; k < BOOTANIM_NSYM; k++) { unsigned m = bootAnimGlyphCell(k); if (m & 0x111Fu) gutter = 0;   // строка 0 и столбец 0
    for (int j = 0; j < k; j++) if (bootAnimGlyph[j] == bootAnimGlyph[k]) distinct = 0; }
  { unsigned g = bootAnimGlyph[3]; int x0 = 3, x1 = -1, y0 = 3, y1 = -1;
    for (int b = 0; b < 9; b++) if (g >> b & 1) { int x = b % 3, y = b / 3; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (x1 - x0 + 1 > 2 || y1 - y0 + 1 > 2) smaller = 0; }
  int same = !memcmp(end0, fb, sizeof fb);
  // раскладка и ход экрана
  int rm = BUI_W - (BUI_LOGO_X + BOOTANIM_PW * BOOTANIM_SCALE);   // правка 98: клетка шрифта 6 px — правое поле в пределах клетки от левого
  // правка 203 (Роб 03.10): знак GONE Makers ×2 справа вверху на сетке колонок, правое поле = левому (в пределах клетки), колонка
  // текста слева кончается до знака (BUI_TEXT_R); ниже по итоговому кадру — ни один пиксель текста не заходит в правую колонку
  int grid = (BUI_LOGO_X - BUI_X0) % BUI_CW == 0 && BUI_LOGO_Y == BUI_Y0 && rm >= BUI_X0 && rm < BUI_X0 + BUI_CW && !gapBad
             && BOOTANIM_SCALE == 2 && BUI_TEXT_R <= BUI_LOGO_X - 4 && BUI_LOGO_X >= BUI_W / 2;
  static unsigned last[BUI_H][BUI_W]; BootSys s = {333, 166, 24576, 0, 1000, 100, "MS0:"};
  // правка 203: строки данных — числа как в build/ (порядок величин настоящий)
  s.vramK = 2048; s.nFiles = 95; s.mediaMB = 546; s.ntex = 520; s.nchunk = 7964; s.pvsW = 48; s.pvsH = 68; s.pvsCell = 20;
  s.colTri = 78612; s.colBox = 1615; s.colCyl = 675; s.colSeg = 900; s.trfModel = 5; s.trfPed = 25; s.trfRoad = 99; s.sfxRes = 72; s.musTrack = 532;
  s.nMovie = 14; s.movieMB = 369;
  static unsigned char fb2[65536]; static BootMarks fm; int fileN = -1;
  { const char *path=getenv("BOOTMARKS_FILE");FILE *f = fopen(path?path:"build/bootmarks.bin", "rb"); if (f) { int n2 = (int)fread(fb2, 1, sizeof fb2, f); fclose(f); fileN = bootMarksParse(fb2, n2, &fm); } }
  if (fileN > 0) bootSeqMarks = &fm;   // раскладка и ход — с настоящими знаками (BMK2 справа); синтетика BMK1 — для режимов появления
  for (int y = 0; y < BUI_H; y++) for (int x = 0; x < BUI_W; x++) scr[y][x] = BUI_BG;
  int len = bootSeqLen(); for (int n = 0; n < len; n++) bootSeqDraw(n, &s, sput, 0);
  memcpy(last, scr, sizeof scr); bootSeqDraw(len, &s, sput, 0); int seqSame = !memcmp(last, scr, sizeof scr);
  for (int y = 0; y < BUI_H; y++) for (int x = 0; x < BUI_W; x++) scr[y][x] = BUI_BG;   // пропуск на первом кадре
  bootSeqDraw(0, &s, sput, 0); bootSeqDraw(len, &s, sput, 0); int skipSame = !memcmp(last, scr, sizeof scr);
  // правка 127: знаки — раскладка, итог 1:1, монотонность, разные режимы, бюджет
  int slotBad = 0, finalBad = 0, monoBad = 0, sameModes = 0, fileBad = 0;
#ifndef BOOT_MARKS
#define BOOT_MARKS 1   // = viewer/bootscreen.h (правка Роба 148, 10:2x: «вставь логотипы, дай пиксель» — мелкие пиксельные значки)
#endif
  int nm = 0; if (fileN > 0) for (int k = 0; k < fm.n; k++) nm += fm.m[k].mode < 6;   // знаки в потоке: без тега (6) и кадров покоя (7, 8, правка 397)
  if (fileN > 0) { bmkSW = fm.slotW; bmkSH = fm.slotH; bmkV = fm.v; bmkN = fm.n; }   // правка 254: раскладка рядов от числа знаков
#if !defined(BMK_257_OLD) && !defined(BMK_254_OLD)
  if (fileN > 0 && fm.v >= 2) {   // правка 257: спонсорский поток — места по ширине каждого знака (как bootSeqMarksDraw)
    int w[BMK_MAX], h[BMK_MAX]; int sc = fm.scale > 1 ? fm.scale : 1;
    for (int k = 0; k < nm; k++) { w[k] = fm.m[k].w * sc; h[k] = fm.m[k].h * sc; }
    bmkFlowLayout(w, h, nm); bmkFlow = 1;
  }
#endif
  for (int k = 0; BOOT_MARKS && k < nm; k++) {   // правка 203: слоты справа под знаком GONE, внутри полей, не в колонке текста
    int x = bmkSlotX(k), y = bmkSlotY(k), sw = bmkFlow ? bmkFW[k] : bmkSW, sh = bmkFlow ? bmkFH[k] : bmkSH;   // 257: в потоке — сам знак
    slotBad += x < BUI_LOGO_X || y < BUI_LOGO_Y + BOOTANIM_PH * BOOTANIM_SCALE || x + sw > BUI_W - BUI_X0 || y + sh > BUI_H - BUI_Y0;
    for (int j = 0; j < k; j++) slotBad += rectHit(x, y, sw, sh, bmkSlotX(j), bmkSlotY(j), bmkFlow ? bmkFW[j] : bmkSW, bmkFlow ? bmkFH[j] : bmkSH);
    slotBad += rectHit(x, y, sw, sh, BUI_LOGO_X, BUI_LOGO_Y, BOOTANIM_PW * BOOTANIM_SCALE, BOOTANIM_PH * BOOTANIM_SCALE);
    slotBad += rectHit(x, y, sw, sh, 0, 0, BUI_TEXT_R, BUI_H);                                       // колонка текста слева
  }
  static unsigned mid[BMK_NMODE][BMK_SLOT_H][BMK_SLOT_W];
  for (int md = 0; md < 6; md++)   /* правка 264: режим 6 (тег) рисует bootTagFrame, не bootMarkFrame */ {
    const BootMark *m = &bmk.m[0]; int prev = -1;
    for (int t2 = 0; t2 < BMK_LEN; t2++) {
      for (int y = 0; y < BMK_SLOT_H; y++) for (int x = 0; x < BMK_SLOT_W; x++) scr[y][x] = BUI_BG;
      bootMarkFrame(m, md, t2, 0, 0, BUI_BG, sput, 0);
      int litN = 0; for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) litN += scr[y][x] != BUI_BG;
      if ((md == 1 || md == 3 || md == 4 || md == 5) && litN < prev) monoBad++;
      prev = litN;
      if (t2 == BMK_LEN / 2) for (int y = 0; y < BMK_SLOT_H; y++) memcpy(mid[md][y], scr[y], sizeof mid[md][y]);
      if (t2 == BMK_LEN - 1) for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) finalBad += scr[y][x] != m->pal[bmkPix(m, x, y)];
    }
  }
  for (int a = 0; a < 6; a++) for (int b2 = 0; b2 < a; b2++) sameModes += !memcmp(mid[a], mid[b2], sizeof mid[a]);
  long putMax = 0; for (int y = 0; y < BUI_H; y++) for (int x = 0; x < BUI_W; x++) scr[y][x] = BUI_BG;
  for (int n = 0; n <= len; n++) { bootMarkPuts = 0; bootSeqDraw(n, &s, sput, 0); if (n < len && bootMarkPuts > putMax) putMax = bootMarkPuts; }
  int lastEnd = BSQ_MARK0 + BSQ_MARK_STEP * (nm > 0 ? nm - 1 : 0) + BMK_LEN, budget = putMax <= 2 * bmkSW * bmkSH && lastEnd <= len;   // ≤ 2 слота за кадр
  fileBad = fileN <= 0; printf("build/bootmarks.bin: %s (%d знаков, BMK%d, слот %dx%d)\n", fileBad ? "ИСПОРЧЕН или нет" : "разобран", fm.n, fm.v, fm.slotW, fm.slotH);
  printf("знаки: разобрано %d, испорченный crc отвергнут: %s; слоты вне сетки/полей или пересекаются: %d; итог режимов != битмап: %d px;\n"
         "  убывание горящих (развёртка/растворение/палитра/перо): %d; одинаковых режимов на середине: %d; put знаков за кадр макс %ld (≤ %d), последний кончается к кадру %d из %d\n",
         parsed, badRej ? "да" : "нет", slotBad, finalBad, monoBad, sameModes, putMax, 2 * bmkSW * bmkSH, lastEnd, len);
  // правки 125 и 203: шрифт msx 8×8, 1 бит; шаг знака — по чернилам + 1 px (Роб 03.10: «межбуквенный очень большой»)
  const char *probe = "GONE MAKERS 0123 gjy%"; const unsigned ink = 0xFFE0E0E0u; int fontBad = 0, fontMid = 0, x = BUI_X0;
  for (int y = 0; y < BUI_H; y++) for (int xx = 0; xx < BUI_W; xx++) scr[y][xx] = 0xFF123456u;
  int xEnd = buiTextX(sput, 0, BUI_X0, BUI_Y0, ink, probe);
  for (int i = 0; probe[i]; i++) {
    unsigned char ch = (unsigned char)probe[i]; const unsigned char *g = bootMsx + 8 * ch; unsigned mm = 0; for (int r = 0; r < 8; r++) mm |= g[r];
    int l = 0, rr = 7; while (l < 8 && !(mm >> (7 - l) & 1)) l++; while (rr >= 0 && !(mm >> (7 - rr) & 1)) rr--;
    int w = l < 8 ? rr - l + 1 : 0, adv = w ? w + 1 : 3;
    for (int r = 0; r < 8; r++) for (int c = 0; c < adv; c++) {
      unsigned v = scr[BUI_Y0 + r][x + c]; int want = w && c < w && (g[r] >> (7 - (l + c))) & 1;
      fontMid += v != ink && v != BUI_BG; fontBad += v != (want ? ink : BUI_BG); }
    x += adv;
  }
  float advAvg = (float)(xEnd - BUI_X0) / (float)strlen(probe);
  int fontGrid = BUI_CW == 8 && BUI_CH == 12 && BUI_W / BUI_CW == 60 && xEnd == x && advAvg <= 6.5f;   // правка 203: плотнее клетки 8
  printf("шрифт msx 8x8 по чернилам: промежуточных цветов %d, отличий от битов msx: %d, средний шаг %.2f px (<= 6,5), сетка %dx%d: %s\n",
         fontMid, fontBad, advAvg, BUI_W / BUI_CW, BUI_H / BUI_CH, fontGrid ? "да" : "нет");
  // правка 203: итоговый кадр — строки данных есть (числа из файлов), текст не заходит в правую колонку знаков
  for (int y = 0; y < BUI_H; y++) for (int xx = 0; xx < BUI_W; xx++) scr[y][xx] = BUI_BG;
  bootSeqMarks = 0; bootSeqDraw(0, &s, sput, 0); bootSeqDraw(len, &s, sput, 0);
  int spill = 0, dataRows = 0;
  for (int y = BUI_Y0; y < BUI_H; y++) for (int xx = BUI_TEXT_R; xx < BUI_LOGO_X; xx++) spill += scr[y][xx] != BUI_BG;
  for (int row = BSQ_ROW0 + 7; row < BSQ_ROW0 + 14; row++) { int lit = 0; for (int xx = BUI_X0; xx < BUI_TEXT_R; xx++) lit += scr[BUI_Y0 + row * BUI_CH + 3][xx] != BUI_BG; dataRows += lit > 0; }
  BootSys e = s; e.ntex = e.nchunk = e.pvsW = e.colTri = e.trfModel = e.sfxRes = e.nMovie = 0;   // нет файлов — строк данных нет
  for (int y = 0; y < BUI_H; y++) for (int xx = 0; xx < BUI_W; xx++) scr[y][xx] = BUI_BG;
  bootSeqDraw(0, &e, sput, 0); bootSeqDraw(len, &e, sput, 0);
  int emptyRows = 0, sh988_ = BOOT_WORK_OLD || BOOT_AI988_OLD ? 0 : 1; for (int row = BSQ_ROW0 + 7 + sh988_; row < BSQ_ROW0 + 14 + sh988_; row++)   /* №988: строка AI сдвигает блок данных на ряд ниже */ { int lit = 0; for (int xx = BUI_X0; xx < BUI_TEXT_R; xx++) lit += scr[BUI_Y0 + row * BUI_CH + 3][xx] != BUI_BG; emptyRows += lit == 0; }
  printf("данные: строк с числами %d из 7, без файлов пустых строк %d из 7 (одна — «Loading»), текст в колонке знаков: %d px\n", dataRows, emptyRows, spill);
  int dataBad = dataRows != 7 || emptyRows < 6 || spill;
  printf("кадров %d, клеток %d (%dx%d по %d px), пикселей знака %d (%dx%d)\n", frames, blocks, BOOTANIM_BW, BOOTANIM_BH, BOOTANIM_CELL, px, BOOTLOGO_W, BOOTLOGO_H);
  printf("стадии / | \\ ■ рисунок нарушены у %d клеток; / | \\ по 3 px и ■ = 1 px: %s; ■ меньше чёрточек (<=2x2): %s; разной формы: %s; зазор сетки у символов: %s\n",
         badSeq, thin ? "да" : "нет", smaller ? "да" : "нет", distinct ? "да" : "нет", gutter ? "да" : "нет");
  printf("касаний символа с соседней клеткой (символ или знак): %d%s\n", touch, touch ? " (первый кадр ниже)" : "");
  if (touch) printf("  первое касание в кадре %d\n", touchFr);
  printf("в конце отличий от bootlogo.h 1:1: %d px; вне знака нарисовано: %d; после второго прохода тот же: %s\n", badPix, outside, same ? "да" : "нет");
  printf("знак на сетке текста (x=%d, y=%d, правое поле %d): %s; итог хода = последний кадр: %s; пропуск START = тот же итог: %s\n",
         BUI_LOGO_X, BUI_LOGO_Y, rm, grid ? "да" : "нет", seqSame ? "да" : "нет", skipSame ? "да" : "нет");
  int fails = badSeq + !thin + !smaller + !distinct + !gutter + (touch > 0) + badPix + outside + !same + !grid + !seqSame + !skipSame + (fontMid > 0) + (fontBad > 0) + !fontGrid
              + (parsed != HOST_MARKS) + !badRej + slotBad + (finalBad > 0) + monoBad + sameModes + !budget + fileBad + dataBad;
  printf("RESULT: %s (%d failures)\n", fails ? "FAIL" : "OK", fails);
  return fails != 0;
}
