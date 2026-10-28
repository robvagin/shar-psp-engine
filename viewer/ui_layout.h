// Правка Роба 126 (01.10): пузырь сообщений — пиксельный шрифт 8×8 (msx из pspsdk, как pspDebugScreen/PSPLink) и текст левее.
// Чистый заголовок без PSP-зависимостей (как ui_radial.h): его же собирает хост-гейт tests/ui/pix8.c.
//  * атлас pix8: 128×64 T4 без свизла, ячейки 8×8, 1 бит (индекс 0 — пусто, 15 — перо). ASCII 32..126 — глифы msx как есть;
//    знаки-заместители кнопок PS2 из библии (tools/ui_export.py BUTTON_CHARS: £ ¿ — X, ® µ — ○, § © — □, @ — △) — свои знаки 8×8
//    по той же сетке (в msx их нет). Шаг 8 (моноширинный, как PSPLink), строка 10 (8 + тень 1 + 1), рисуется 1:1, GU_NEAREST.
//  * перенос: жадный по пробелам, как прежний wrapLines; '\n' и '\\' — жёсткий перенос (библия рвёт строки «\», как в
//    uiDrawTutorial и титрах), знак не рисуется (раньше выходил «?» или «\»).
//  * начало текста задания: ПРАВИЛО ПОРТА (данных о левом поле текста без значка на диске нет; Hud.pag ставит текст за место
//    значка). Без значка — поле пузыря (левый край пузыря + отступ текста от его верха, тот же отступ, что справа в fitW);
//    с аватаром извне — за его видимым краем, но не левее поля; значок-слот пакета — место текста из пакета.
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#define PIX8_TW 128                 // атлас: 16 ячеек в ряд
#define PIX8_TH 64                  // 7 рядов ячеек (99 глифов) -> степень двойки
#define PIX8_ADV 8                  // шаг знаков кнопок X ○ △ □ (свои 8×8)
#define PIX8_LH 10
// Уточнение Роба 01.10 (126/125): тонкий 1 px «худой, межбуквенный громадный» — в пузыре жирный компактный: вертикальные штрихи
// 2 px (строка глифа OR сдвиг на 1 вправо), ширина буквы по рисунку + 1 px, пробел 3 px. Крупный A (точка 2×2) в пузырь в два
// ряда не влезает (пятая часть заданий — 3–5 строк) — ждёт решения Роба про больший пузырь. tests/ui/pix8.sh: -DPIX8_BOLD=0 -> FAIL
#ifndef PIX8_BOLD
#define PIX8_BOLD 1
#endif
#define PIX8_NCELL 99               // 95 ASCII + X ○ △ □
#define PIX8_TEXBYTES (PIX8_TW * PIX8_TH / 2)

static const unsigned char pix8Btn[4][8] = {   // X, ○, △, □ — бит 7 = левый пиксель (сетка msx)
  {0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00},
  {0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00},
  {0x00, 0x18, 0x18, 0x24, 0x24, 0x42, 0x7e, 0x00},
  {0x00, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x00},
};
static inline int pix8Cell(unsigned char c) {   // ячейка атласа или -1 (глифа нет -> рисуется '?')
  if (c == 0xa3 || c == 0xbf) return 95;        // X
  if (c == 0xae || c == 0xb5) return 96;        // ○
  if (c == '@') return 97;                      // △
  if (c == 0xa7 || c == 0xa9) return 98;        // □
  if (c >= 32 && c <= 126) return c - 32;
  return -1;
}
static inline const unsigned char *pix8Rows(const unsigned char *msx, int cell) {
  return cell >= 95 ? pix8Btn[cell - 95] : msx + (cell + 32) * 8;
}
// Жирный: утолщение вправо слепляет штрихи через 1 px (G становится 6, середина M и W заливается) — заглавные и цифры, у которых
// так выходит, нарисованы руками (штрих 2 px, просветы сохранены, M W & шириной 7); у прочих утолщение только там, где не слипается
static const struct { char c; unsigned char r[8]; } pix8Fix[] = {
  {'A', {0x30, 0x78, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0x00}}, {'D', {0xF8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xF8, 0x00}},
  {'G', {0x78, 0xCC, 0xC0, 0xDC, 0xCC, 0xCC, 0x78, 0x00}}, {'K', {0xCC, 0xD8, 0xF0, 0xE0, 0xF0, 0xD8, 0xCC, 0x00}},
  {'M', {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00}}, {'N', {0xCC, 0xEC, 0xFC, 0xDC, 0xCC, 0xCC, 0xCC, 0x00}},
  {'Q', {0x78, 0xCC, 0xCC, 0xCC, 0xDC, 0xD8, 0x6C, 0x00}}, {'R', {0xF8, 0xCC, 0xCC, 0xF8, 0xD8, 0xCC, 0xCC, 0x00}},
  {'V', {0xCC, 0xCC, 0xCC, 0xCC, 0x78, 0x78, 0x30, 0x00}}, {'W', {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00}},
  {'X', {0xCC, 0xCC, 0x78, 0x30, 0x78, 0xCC, 0xCC, 0x00}}, {'0', {0x70, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0x70, 0x00}},
  {'1', {0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00}}, {'4', {0x18, 0x38, 0x78, 0xD8, 0xFC, 0x18, 0x18, 0x00}},
  {'&', {0x70, 0xD8, 0xD8, 0x76, 0xDC, 0xCC, 0x76, 0x00}},
};
static inline unsigned pix8Row(const unsigned char *msx, int cell, int y) {   // строка глифа в атласе (жирный: штрих 2 px)
  unsigned r = pix8Rows(msx, cell)[y];
  if (!PIX8_BOLD || cell >= 95) return r;
  for (unsigned i = 0; i < sizeof pix8Fix / sizeof pix8Fix[0]; i++) if (pix8Fix[i].c == cell + 32) return pix8Fix[i].r[y];
  return (r | ((r >> 1) & ~(r << 1))) & 0xffu;   // утолщение вправо, кроме места, где оно закрыло бы просвет 1 px
}
static inline void pix8Ink(const unsigned char *msx, int cell, int *l, int *w) {   // столбцы рисунка: левый и ширина, px
  unsigned m = 0; for (int y = 0; y < 8; y++) m |= pix8Row(msx, cell, y);
  if (cell >= 95) { *l = 0; *w = 8; return; }                 // знаки кнопок — целой клеткой
  if (!m) { *l = 0; *w = 2; return; }                          // пробел: 2 + 1 = 3 px
  int a = 0, b = 7; while (!(m & (0x80u >> a))) a++; while (!(m & (0x80u >> b))) b--;
  *l = a; *w = b - a + 1;
}
static inline int pix8Adv(const unsigned char *msx, int cell) {   // шаг знака: ширина рисунка + 1 px (моноширинный 8 — PIX8_BOLD=0)
  if (!PIX8_BOLD) return PIX8_ADV;
  int l, w; pix8Ink(msx, cell, &l, &w); return cell >= 95 ? PIX8_ADV + 1 : w + 1;
}
// атлас T4 (линейный, младший полубайт — левый тексель)
static inline void pix8Build(const unsigned char *msx, unsigned char *tex) {
  for (int i = 0; i < PIX8_TEXBYTES; i++) tex[i] = 0;
  for (int c = 0; c < PIX8_NCELL; c++) {
    int ox = (c & 15) * 8, oy = (c >> 4) * 8;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (pix8Row(msx, c, y) & (0x80u >> x)) {
          int t = (oy + y) * PIX8_TW + ox + x;
          tex[t >> 1] |= (unsigned char)((t & 1) ? 0xf0 : 0x0f);
        }
  }
}
static inline int pix8Texel(const unsigned char *tex, int x, int y) {
  int t = y * PIX8_TW + x; return (tex[t >> 1] >> ((t & 1) * 4)) & 15;
}

// перенос строк: s -> начала/длины строк (не больше maxLines). lw — ширина куска в пикселях
typedef int (*UiLineW)(const void *ctx, const char *s, int n);
static inline int uiIsBreak(char c) { return c == '\n' || c == '\\'; }
#define UI_WRAP_BALANCE 4   // бит wrap: ровные строки (№548, пузырь без значка): число строк как при полной ширине, ширина — наименьшая, дающая то же число строк. BUBBLE_FIT_OLD=1 — без
#define UI_WRAP_NOWIDOW 2   // бит wrap: без висячего слова в последней строке абзаца (правка 215, текст задания HUD)
#define UI_WRAP_NOGLUE 8    // бит wrap (№1037): без склейки коротких служебных слов со следующим (правило №401 отключено для этого переноса)
// №1037 (Роб 26.10 на TEST72: «Talk to the sea captain в 3 строки, а места на 2; я просил без висячих строк, а в итоге всё с переносом; золотую середину»): правила красоты (короткое слово не в конце строки №401,
// без висячего слова правка 215, ровные строки №548) раздували число строк: «TALK TO THE / SEA CAPTAIN» помещается в две, но «THE» в конце строки запрещено, и выходило «TALK / TO THE SEA / CAPTAIN». Теперь порядок такой:
// 1) наименьшее число строк, которое помещается в ширину (жадный перенос без правил); 2) только внутри этого числа строк — правила по убыванию (все; без склейки коротких слов, но без висячего слова; без висячего
// слова не требуется) — берётся первая раскладка, которая не добавляет строку. Никогда не добавлять строку ради красоты. WRAP1037_OLD=1 / файл wrap1037old / «flag wrap1037 0» — прежнее
#ifndef WRAP1037_OLD
#define WRAP1037_OLD 0
#endif
static int uiWrap1037 = !WRAP1037_OLD;
static unsigned uiWrapN1040[2];   // №1040 служебно: вызовов uiWrapPara (верхний уровень) и uiWrapPara0 (с рекурсией баланса) — сколько раз считался перенос
// №401 (Роб 08.10: «в уведомлениях висят строки: в конце or, of, but»): короткое служебное слово (артикль, союз, предлог) не
// остаётся последним в строке — уходит на следующую вместе со своим словом, если там влезает. Правило набора порта, для всех
// переносимых текстов. Прежнее: -DUI_WRAP_SHORT_OLD
static inline int uiShortWord(const char *w, int n) {
  static const char *const list[] = {"A", "AN", "THE", "OF", "OR", "BUT", "AND", "NOR", "TO", "IN", "ON", "AT", "BY", "FOR", "AS",
                                     "IF", "SO", "INTO", "ONTO", "FROM", "WITH", "THAN", "YOUR", "MY", "IS", "IT"};
  if (n < 1 || n > 4) return 0;
  for (unsigned k = 0; k < sizeof list / sizeof *list; k++) {
    const char *a = list[k]; int j = 0;
    while (j < n && a[j] && (w[j] == a[j] || w[j] == a[j] + 32)) j++;
    if (j == n && !a[j]) return 1;
  }
  return 0;
}
static inline int uiWrapPara0(const char *s, int base, int n, int maxw, int wrap, UiLineW lw, const void *ctx,
                             int *st0, int *len, int nl, int maxLines) {
  const char *p = s + base; int nl0 = nl; uiWrapN1040[1]++;
  if (nl >= maxLines) return nl;
  if (!wrap || maxw <= 0 || lw(ctx, p, n) <= maxw) { st0[nl] = base; len[nl] = n; return nl + 1; }
  int ls = 0, i = 0, cur = -1;                          // cur = конец принятой части строки
  while (i <= n && nl < maxLines) {
    int we = i; while (we < n && p[we] != ' ') we++;    // слово [i, we)
#ifndef UI_WRAP_SHORT_OLD
    for (int ws = i; we < n && !(wrap & UI_WRAP_NOGLUE) && uiShortWord(p + ws, we - ws);) {   // №401: короткое слово склеено со следующим (как неразрывный пробел)
      ws = we + 1; we = ws; while (we < n && p[we] != ' ') we++; }
#endif
    if (cur < 0 || lw(ctx, p + ls, we - ls) <= maxw) cur = we;
    else { st0[nl] = base + ls; len[nl] = cur - ls; nl++; ls = i; cur = we; }
    i = we + 1;
  }
  if (nl < maxLines) { st0[nl] = base + ls; len[nl] = (cur < 0 ? n : cur) - ls; nl++; }
  if ((wrap & UI_WRAP_BALANCE) && nl - nl0 >= 2) {   // №548: наименьшая ширина с тем же числом строк (двоичный поиск); перенос заново по ней, с тем же правилом висячего слова
    int want = nl - nl0, lo = 1, hi = maxw, bw = wrap & ~UI_WRAP_BALANCE;
    while (lo < hi) { int mid = (lo + hi) / 2; if (uiWrapPara0(s, base, n, mid, bw, lw, ctx, st0, len, nl0, maxLines) - nl0 <= want) hi = mid; else lo = mid + 1; }
    return uiWrapPara0(s, base, n, lo, bw, lw, ctx, st0, len, nl0, maxLines);
  }
  if ((wrap & UI_WRAP_NOWIDOW) && nl - nl0 >= 2) {   // правка 215: последняя строка не из одного слова — последнее слово
    int a0 = st0[nl - 2], al = len[nl - 2], b0 = st0[nl - 1], bl = len[nl - 1], k = a0 + al - 1, sp = 0;   // прошлой строки
    for (int j = b0; j < b0 + bl; j++) sp |= s[j] == ' ';                                                    // вниз, если влезает
    while (k > a0 && s[k] != ' ') k--;                                            // («COLLECT THE ICE CREAM / AND COLA», как PS2)
    if (!sp && k > a0 && lw(ctx, s + k + 1, b0 + bl - (k + 1)) <= maxw) { len[nl - 2] = k - a0; st0[nl - 1] = k + 1; len[nl - 1] = b0 + bl - (k + 1); }
  }
  return nl;
}
static inline int uiWrapPara(const char *s, int base, int n, int maxw, int wrap, UiLineW lw, const void *ctx,
                             int *st0, int *len, int nl, int maxLines) {   // №1037: наименьшее число строк — прежде всего; правила красоты только внутри него
  uiWrapN1040[0]++;
  if (!uiWrap1037 || !wrap || maxw <= 0 || nl >= maxLines || lw(ctx, s + base, n) <= maxw) return uiWrapPara0(s, base, n, maxw, wrap, lw, ctx, st0, len, nl, maxLines);
  int plain = uiWrapPara0(s, base, n, maxw, UI_WRAP_NOGLUE, lw, ctx, st0, len, nl, maxLines) - nl;   // жадный перенос без правил — наименьшее число строк
  int tries[3] = { wrap, wrap | UI_WRAP_NOGLUE, (wrap & ~UI_WRAP_NOWIDOW) | UI_WRAP_NOGLUE };
  for (int k = 0; k < 3; k++) { int r = uiWrapPara0(s, base, n, maxw, tries[k], lw, ctx, st0, len, nl, maxLines); if (r - nl <= plain) return r; }
  return uiWrapPara0(s, base, n, maxw, UI_WRAP_NOGLUE, lw, ctx, st0, len, nl, maxLines);
}
static inline int uiWrap(const char *s, int maxw, int wrap, UiLineW lw, const void *ctx, int *st0, int *len, int maxLines) {
  int n = 0; while (s[n]) n++;
#ifdef UI_WRAP_OLD   // отрицательный контроль гейта: прежний wrapLines (жёстких переносов нет)
  return uiWrapPara(s, 0, n, maxw, wrap, lw, ctx, st0, len, 0, maxLines);
#else
  int nl = 0, a = 0;
  for (int i = 0; i <= n && nl < maxLines; i++)
    if (i == n || uiIsBreak(s[i])) { nl = uiWrapPara(s, a, i - a, maxw, wrap, lw, ctx, st0, len, nl, maxLines); a = i + 1; }
  return nl;
#endif
}

// начало текста задания (x). kind: 0 — без значка, 1 — значок-слот пакета, 2 — аватар извне (edge — его видимый край + зазор).
// bubble: 1 — текст стоит в пузыре, field = левый край пузыря + отступ. -1 — место текста из пакета
enum { UI_OBJ_NOICON, UI_OBJ_SLOT, UI_OBJ_AVATAR };
static inline float uiObjStart(int kind, float edge, int bubble, float field) {
#ifdef UI_OBJ_OLD   // отрицательный контроль гейта: до правки 126 без значка текст стоял на месте из пакета
  if (kind == UI_OBJ_NOICON) return -1.f;
#endif
  if (kind == UI_OBJ_SLOT) return -1.f;
#ifndef UI_OBJ_PAD
#define UI_OBJ_PAD 6.f   // правки Роба 211 и 228 (03.10): «Get in vehicle» и пузыри без картинки слева — текст у края плашки, «сливается»; +6 px вправо
#endif
  if (kind == UI_OBJ_NOICON) return bubble ? field + UI_OBJ_PAD : -1.f;
  return bubble && edge < field ? field : edge;
}
#endif
