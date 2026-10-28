// Линии мультика по клеткам: полосы по высоте (№774), отбор записей, подача в GE (№779, №824, №912). Чистая часть без PSP-заголовков: подачу в GE даёт вызывающий (sceGuSetMatrix, sceGuDrawArray),
// поэтому её же гоняет tests/toon/lines912.sh с подставными вызовами. Требует: TlCell, TL_CELL, TL_RANGE, TL_RGBA не нужен, ScePspFMatrix4, GU_LINES, GU_VERTEX_16BIT, GU_TRANSFORM_3D, GU_MODEL
#pragma once
// №774 (Роб 18.10: «L1: серая ломаная полоса в воздухе — от коридора D8-9 через фиолетовый зал, сквозь ступеньки и колонны, до капель D6-1»): линии края
// асфальта и мира рисовались по расстоянию в плане, без высоты; в подземном зале под улицей линии дороги над головой видны сквозь потолок (изнутри
// его нет). Клетки делятся на полосы по высоте (2 м, порядок линий внутри клетки меняется, сами линии — те же); полоса, вся выше глаза больше чем на
// TL_ABOVE + TL_ABOVE_K × расстояние в плане до клетки, не рисуется (над головой — не видна; дорога в гору вдали — видна). TL_BAND774_OLD=1 — как было
#ifndef TL_BAND774_OLD
#define TL_BAND774_OLD 0
#endif
#define TL_ABOVE 3.f
#define TL_ABOVE_K .25f
static int tlLineCmp(const void *a, const void *b) { const int16_t *x = (const int16_t *)a, *y = (const int16_t *)b; int ya = x[1] < x[4] ? x[1] : x[4], yb = y[1] < y[4] ? y[1] : y[4]; return (ya / 200 - (ya < 0)) - (yb / 200 - (yb < 0)); }
static void tlBandSplit(TlCell **pc, int16_t *V, unsigned *pnc, int16_t **py) {
  if (TL_BAND774_OLD || !*pc || !*pnc) return;
  unsigned nb = 0; TlCell *C = *pc;
  for (unsigned c = 0; c < *pnc; c++) {   // сортировка линий клетки по полосе высоты, счёт полос
    int16_t *L = V + (size_t)C[c].first * 6; qsort(L, C[c].count, 12, tlLineCmp);
    int last = 0x7fffffff; for (unsigned i = 0; i < C[c].count; i++) { int y = L[i * 6 + 1] < L[i * 6 + 4] ? L[i * 6 + 1] : L[i * 6 + 4], b = y / 200 - (y < 0); if (b != last) { nb++; last = b; } } }
  TlCell *N = (TlCell *)malloc((size_t)nb * sizeof(TlCell)); int16_t *Y = (int16_t *)malloc((size_t)nb * sizeof(int16_t)); if (!N || !Y) { free(N); free(Y); return; }
  unsigned k = 0;
  for (unsigned c = 0; c < *pnc; c++) { const int16_t *L = V + (size_t)C[c].first * 6; int last = 0x7fffffff;
    for (unsigned i = 0; i < C[c].count; i++) { int y = L[i * 6 + 1] < L[i * 6 + 4] ? L[i * 6 + 1] : L[i * 6 + 4], b = y / 200 - (y < 0);
      if (b != last) { N[k].cx = C[c].cx; N[k].cz = C[c].cz; N[k].first = C[c].first + i; N[k].count = 0; Y[k] = (int16_t)y; k++; last = b; }
      N[k - 1].count++; if (y < Y[k - 1]) Y[k - 1] = (int16_t)y; } }
  free(C); *pc = N; *pnc = nb; *py = Y;
}
// №779: линии мира от того же ползунка толщины, что контур героев (skinToonLineParams): 0 — без линий, 1–6 — 1 px, 7–10 — 2 px (второй проход на пиксель правее,
// только у клеток ближе TL_THICK_NEAR м: вдали линия не толще). Воздушная перспектива, как у контура героев: туман GE в цвет дали (skinToonAirColor) от 5 м,
// к 40 м — 0,6 цвета дали (полный — 63 м); после линий — прежний туман кадра (fogBase). TOONLINE_W779_OLD=1 — как раньше (tlThick, всегда, без светления)
#define TL_THICK_NEAR 20.f
#ifndef TOONLINE_W779_OLD
#define TOONLINE_W779_OLD 0
#endif
static unsigned tlDc[2], tlWords;   // №912 диагностика: подачи линий (DrawArray) и слова списка GE на них (строка TOONLINES)
// №912 (окно «Оптимизация»: мультик +0,32 мс эмулятора в x_fx; разбор — линии мира и дорог ≈ 0,28 мс). Файл линий режет клетку на полосы по высоте (№774) — записей TlCell на одну клетку несколько
// (87 подач на кадр у клеток в 60 м), а матрица клетки загружалась перед КАЖДОЙ записью (13 слов из 17 на подачу): теперь матрица — один раз на клетку, подряд идущие в файле записи одной клетки
// (первая линия = конец предыдущей) — одной подачей. Сам отбор записей (216 проходов на набор за кадр, 0,1 мс эмулятора) переиспользуется, пока глаз сдвинулся меньше TL_REUSE м и прошло
// меньше TL_REUSE_AGE кадров: границы отбора (60 м, полоса над головой, 20 м у толстой линии) на таком сдвиге не заметны. Линии те же. TLBATCH912_OLD=1 — как было
#ifndef TLBATCH912_OLD
#define TLBATCH912_OLD 0
#endif
#ifndef TL_REUSE
#define TL_REUSE .5f
#endif
#ifndef TL_REUSE_AGE
#define TL_REUSE_AGE 8
#endif
#define TL_SEL_MAX 400
typedef struct { const TlCell *C; int pass, n, age; float e[3], oy; unsigned lines; unsigned short idx[TL_SEL_MAX]; } TlSel;
static TlSel tlSel[12];   // №1023: 12 мест (линии мира 2 прохода, края дороги до 6, тропинки 1)
static float tlThickOv;   // №1023: >0 — своя дальность толстого прохода (края дороги), м
 static unsigned tlSelHit, tlSelMiss, tlSelOver;   // tlSelOver: отборов, где кусков больше TL_SEL_MAX (хвост молча не рисовался — Надзор 24.10)
static float tlRangeOv;   // №978: >0 — своя дальность отбора клеток (линии края дороги), м
// №1043 (Роб: «везде на фоне вдалеке появились какие-то чёрные провода»; кадры дирижёра l970_wr_*: на плоской тёмной земле ломаные чёрные линии углами и стойками без домов): линии мира и края дороги берутся из
// level_toon.shr по своей сетке 64 м и рисовались, даже если клеток города под ними ещё нет в памяти (после рывка/телепорта, пока клетки грузятся, на краю подгрузки, у срезанной дальности): остаются
// чёрные контуры домов в пустоте поверх подложки. Теперь клетка линий рисуется, только когда в памяти (state 2) есть клетка города, перекрывающая её квадрат. INKFAR1043_OLD=1 / файл inkfar1043old / «flag inkfar1043 0» — как было
#ifndef INKFAR1043_OLD
#define INKFAR1043_OLD 0
#endif
static int inkFar1043On = !INKFAR1043_OLD;
static unsigned tlGateN, tlGateCut;
static int tlCellLoaded(float ox, float oz) {
  for (unsigned i = 0; i < ncell; i++) { const Cell *c = &cl[i]; if (c->state != 2) continue;
    if (c->bb[3] >= ox && c->bb[0] <= ox + TL_CELL && c->bb[5] >= oz && c->bb[2] <= oz + TL_CELL) return 1; }
  return 0;
}
static int tlKeep(const TlCell *C, const int16_t *Y, unsigned c, int pass, float ex, float ey, float ez, float oy) {   // запись в отборе кадра: дальность, полоса над головой, толстая линия только вблизи
  float ox = C[c].cx * TL_CELL, oz = C[c].cz * TL_CELL;
  float rg_ = tlRangeOv > 0.f ? tlRangeOv : (float)TL_RANGE;   // №978: у линий края дороги дальность короче
  if (ex < ox - rg_ || ex > ox + TL_CELL + rg_ || ez < oz - rg_ || ez > oz + TL_CELL + rg_) return 0;
  if (inkFar1043On && cl && ncell) { tlGateN++; if (!tlCellLoaded(ox, oz)) { tlGateCut++; return 0; } }   // №1043
  if (Y) { float nx_ = ex < ox ? ox : ex > ox + TL_CELL ? ox + TL_CELL : ex, nz_ = ez < oz ? oz : ez > oz + TL_CELL ? oz + TL_CELL : ez;   // №774: полоса над головой
    if (Y[c] * .01f + oy - ey > TL_ABOVE + TL_ABOVE_K * sqrtf((nx_ - ex) * (nx_ - ex) + (nz_ - ez) * (nz_ - ez))) return 0; }
  if (pass && !TOONLINE_W779_OLD) { float nx_ = ex < ox ? ox : ex > ox + TL_CELL ? ox + TL_CELL : ex, nz_ = ez < oz ? oz : ez > oz + TL_CELL ? oz + TL_CELL : ez;   // №779: 2 px — только вблизи
    float tn_ = tlThickOv > 0.f ? tlThickOv : TL_THICK_NEAR; if ((nx_ - ex) * (nx_ - ex) + (nz_ - ez) * (nz_ - ez) > tn_ * tn_) return 0; }
  return 1;
}
static unsigned tlDrawSet(const TlCell *C, const int16_t *V, const int16_t *Y, unsigned NC, int pass, float ex, float ey, float ez, float oy) {   // №824: один набор линий (мир или дорога) за один проход; число линий (только pass 0)
  unsigned n = 0;
  if (TLBATCH912_OLD) {
    for (unsigned c = 0; c < NC; c++) {
      if (!tlKeep(C, Y, c, pass, ex, ey, ez, oy)) continue;
      float ox = C[c].cx * TL_CELL, oz = C[c].cz * TL_CELL;
      ScePspFMatrix4 m = {{327.68f, 0, 0, 0}, {0, 327.68f, 0, 0}, {0, 0, 327.68f, 0}, {ox, oy, oz, 1}};   // int16 в GE нормированы на 32768: сантиметры -> метры = 32768 / 100
      sceGuSetMatrix(GU_MODEL, &m);
      sceGuDrawArray(GU_LINES, GU_VERTEX_16BIT | GU_TRANSFORM_3D, C[c].count * 2, 0, V + (size_t)C[c].first * 6);
      tlDc[pass ? 1 : 0]++;
      if (!pass) n += C[c].count;
    }
    return n;
  }
  if (tlSelReset) { memset(tlSel, 0, sizeof tlSel); tlSelReset = 0; }
  TlSel *s = 0; for (int k = 0; k < 12; k++) if (tlSel[k].C == C && tlSel[k].pass == pass) { s = &tlSel[k]; break; }
  if (!s) { for (int k = 0; k < 12; k++) if (!tlSel[k].C) { s = &tlSel[k]; break; } if (!s) s = &tlSel[0]; s->C = C; s->pass = pass; s->age = 1 << 20; }
  float dx = ex - s->e[0], dy = ey - s->e[1], dz = ez - s->e[2];
  if (s->age >= TL_REUSE_AGE || s->oy != oy || dx * dx + dy * dy + dz * dz > TL_REUSE * TL_REUSE) {   // пересчёт отбора
    int k = 0; unsigned lines = 0;
    for (unsigned c = 0; c < NC && k < TL_SEL_MAX; c++) if (tlKeep(C, Y, c, pass, ex, ey, ez, oy)) { s->idx[k++] = (unsigned short)c; lines += C[c].count; }
    if (k == TL_SEL_MAX) { unsigned more = 0; for (unsigned c = 0; c < NC; c++) more += tlKeep(C, Y, c, pass, ex, ey, ez, oy); if (more > (unsigned)k) { tlSelOver++; if (smkOn && tlSelOver <= 8) { char o_[100]; snprintf(o_, sizeof o_, "TLSELOVER keep=%u max=%d pass=%d n=%u\n", more, TL_SEL_MAX, pass, tlSelOver); smkWrite(o_); } } }   // переполнение отбора — строка в журнал (первые 8), не молча
    s->n = k; s->lines = lines; s->e[0] = ex; s->e[1] = ey; s->e[2] = ez; s->oy = oy; s->age = 0; tlSelMiss++;
  } else { s->age++; tlSelHit++; }
  int have = 0, lcx = 0, lcz = 0; uint32_t rf = 0, rc = 0;   // накопленная подача: клетка, первая линия, число линий
  for (int k = 0; k < s->n; k++) {
    unsigned c = s->idx[k];
    if (have && C[c].cx == lcx && C[c].cz == lcz && C[c].first == rf + rc) { rc += C[c].count; continue; }   // продолжение той же подачи
    if (have) { sceGuDrawArray(GU_LINES, GU_VERTEX_16BIT | GU_TRANSFORM_3D, rc * 2, 0, V + (size_t)rf * 6); tlDc[pass ? 1 : 0]++; }
    if (!have || C[c].cx != lcx || C[c].cz != lcz) {
      ScePspFMatrix4 m = {{327.68f, 0, 0, 0}, {0, 327.68f, 0, 0}, {0, 0, 327.68f, 0}, {C[c].cx * TL_CELL, oy, C[c].cz * TL_CELL, 1}};   // int16 в GE нормированы на 32768: сантиметры -> метры = 32768 / 100
      sceGuSetMatrix(GU_MODEL, &m); }
    have = 1; lcx = C[c].cx; lcz = C[c].cz; rf = C[c].first; rc = C[c].count;
  }
  if (have) { sceGuDrawArray(GU_LINES, GU_VERTEX_16BIT | GU_TRANSFORM_3D, rc * 2, 0, V + (size_t)rf * 6); tlDc[pass ? 1 : 0]++; }
  return pass ? 0 : s->lines;
}
