// Пиксельные знаки причастных на заставке GONE Makers (правка Роба 127, 01.10: «очень пиксельные логотипы … пусть будет
// как старая игра; красиво появлялись, подобно GONE Makers, может по-разному; очень сильно оптимизировать»). Своего экрана
// логотипов у оригинала нет — правило порта. Данные — build/bootmarks.bin (tools/bootmarks.py: плашки gamelogo с диска
// Роба + его файлы), читаются в bootBegin и освобождаются до игры; файла нет — экран без знаков, игра идёт.
// Знак — битмап 1:1, 2 бита на пиксель (индекс 0 — фон), палитра 4 серых системной гаммы. Появление обобщает знак GONE
// (viewer/bootanim.h) на любой битмап: bootMarkFrame(знак, режим, кадр) — все режимы функции (x, y, t) без таблиц:
//   0 волна / | \ ■ по клеткам 4×4, как у GONE (семья);  1 развёртка строк сверху вниз, новая строка бликом;
//   2 мозаика 8 -> 4 -> 2 -> 1 (как SNES), цвет блока — большинство;  3 растворение по Байеру 4×4 (16 шагов);
//   4 проявление палитрой в 3 шага (как NES);  5 перо слева направо колонками с бликом.
// Рисуется знак только в своём окне появления (BMK_LEN кадров) и на итоговом кадре — не больше двух знаков за кадр.
// Чистый C без PSP: общий для viewer/bootscreen.h, превью (tools/bootscreen_preview.c) и проверки (tests/bootlogo).
#ifndef BOOTMARKS_H
#define BOOTMARKS_H
#include "bootanim.h"

// Правка Роба 203 (03.10): «логотипы — настоящие; GONE Makers справа, под ним все остальные». BMK2 — цветные знаки 4 бита
// (16 цветов, индекс 0 — фон) в слотах до 112×40, сеткой 2 × 3 под знаком GONE (ширина ряда = ширине знака ×2, 232 px).
// BMK1 (2 бита, 4 серых, слоты 64×24, ряд под проверками) читается по-прежнему — старый файл не ломает экран
#define BMK_SLOT_W 112
#define BMK_SLOT_H 40
#define BMK1_SLOT_W 64        // правка 148: прежние мелкие значки 64×24
#define BMK1_SLOT_H 24
#define BMK_MAX 32            // правка 254: восемь знаков (+ El Barto); правка 397: + кадры покоя знаков (режимы 7, 8)
#define BMK_NMODE 9           // правка 264: 6 — «распыление» тега El Barto; правка 397: 7/8 — кадр покоя знака с тем же именем (смена раз в 6/24 кадров)
// Правка Роба 291 (04.10: «El Barto мельче, чтобы по пикселям чётко и читабельнее — крупный и сливается»): тег рисуется пикселем
// 1×1 (было 2×2) в большем разрешении знака (до 128×72) — на экране меньше, штрихи в один пиксель. BMK_TAG_PX 2 — прежнее
#ifndef BMK_TAG_PX
#define BMK_TAG_PX 1
#endif
#define BMK_TAG_MAXW (BMK_TAG_PX == 1 ? 128 : 96)
#define BMK_TAG_MAXH (BMK_TAG_PX == 1 ? 72 : 64)
#define BMK_LEN 32            // кадров появления одного знака (0,5 с)
#define BMK_STAGE 6.f         // волна: пикселей хода фронта на стадию символа
#define BMK_WIDTH 16.f        // волна: ширина блика за фронтом
typedef struct {
  int w, h, mode, bpp;        // bpp 2 (BMK1) или 4 (BMK2)
  unsigned pal[16];           // ABGR
  const unsigned char *bits;  // строки подряд; 2 бита: 4 пикселя в байте, 4 бита: 2 пикселя; старшие биты — левый
  char name[15];
  unsigned ink;               // №1010: краска тега (ABGR из 4 последних байт поля имени записи); 0 — цвет палитры
} BootMark;
// №1010 (Роб 26.10: El Barto на заставке — родной, красным, с мусорного бака L6 из самой игры): в файле знаков два тега режима 6 — прежний 'elbarto'
// (файл Роба 04.10) и 'elbarto6' (текстура бака l6_dump.p3d с диска, tools/bootmarks.py); рисуется один: 'elbarto6' своей краской, если он есть.
// ELBARTO1010_OLD=1 / файл elbarto1010old рядом с EBOOT (bootElbarto1010Old) — прежний тег прежним цветом. Пиксели тега — только в build/bootmarks.bin, не в коде
#ifndef ELBARTO1010_OLD
#define ELBARTO1010_OLD 1   // Роб 26.10 23:0x: «El Barto верни тот, что был до этого» — прежний тег по умолчанию
#endif
static int bootElbarto1010Old = ELBARTO1010_OLD;
#define BMK_TAG6_NAME "elbarto6"
typedef struct { int n, slotW, slotH, v, scale; BootMark m[BMK_MAX]; } BootMarks;   // v: 1 — BMK1, 2 — BMK2; scale — пиксель знака квадратом scale×scale

static unsigned bmkRd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned bmkRd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned bmkCrc32(const unsigned char *p, int n) {   // тот же crc32, что zlib.crc32 в tools/bootmarks.py
  unsigned c = 0xFFFFFFFFu;
  for (int i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
  return ~c;
}
// разобрать BMK1 (формат — tools/bootmarks.py); битмапы указывают внутрь b. Вернуть число знаков, -1 — файл испорчен
static int bootMarksParse(const unsigned char *b, int len, BootMarks *out) {
  out->n = 0;
  if (len < 16 || b[0] != 'B' || b[1] != 'M' || b[2] != 'K' || (b[3] != '1' && b[3] != '2' && b[3] != '3')) return -1;
#ifdef BOOTMARKS_BMK3_OLD
  if (b[3] == '3') return -1;
#endif
#ifndef BOOTMARKS_SHORT_HEADER_OLD
#define BOOTMARKS_SHORT_HEADER_OLD 0
#endif
  if (!BOOTMARKS_SHORT_HEADER_OLD && b[3] == '3' && len < 24) return -1;
  if (bmkCrc32(b, len - 4) != bmkRd32(b + len - 4)) return -1;
  int v = b[3] - '0', bpp = v == 1 ? 2 : v == 2 ? 4 : 1, npal = 1 << bpp, rec = 20 + (v == 3 ? 0 : 4 * npal);
  int n = (int)bmkRd16(b + 4), p = v == 3 ? 20 : 12, sw = (int)bmkRd16(b + 6), sh = (int)bmkRd16(b + 8), sc = v >= 2 && bmkRd16(b + 10) == 2 ? 2 : 1;
  unsigned shared[2] = {0, 0}; if (v == 3) { shared[0] = bmkRd32(b + 12); shared[1] = bmkRd32(b + 16); }
  if (n > BMK_MAX || (v == 1 && (sw != BMK1_SLOT_W || sh != BMK1_SLOT_H)) || (v >= 2 && (sw < 8 || sh < 8 || sw > BMK_SLOT_W || sh > BMK_SLOT_H))) return -1;
  for (int k = 0; k < n; k++) {
    if (p + rec > len - 4) return -1;
    BootMark *m = &out->m[k];
    m->w = (int)bmkRd16(b + p); m->h = (int)bmkRd16(b + p + 2); m->mode = b[p + 5]; m->bpp = bpp;
    int tag = v >= 2 && m->mode == 6;   // правка 264: тег El Barto — не в слоте (до 96×64, рисуется 2×2)
    if (b[p + 4] != bpp || m->w < 1 || m->h < 1 || m->mode >= BMK_NMODE) return -1;
    if (tag ? (m->w > BMK_TAG_MAXW || m->h > BMK_TAG_MAXH) : (m->w * sc > sw || m->h * sc > sh)) return -1;
    for (int i = 0; i < 14; i++) m->name[i] = (char)b[p + 6 + i]; m->name[14] = 0;
    m->ink = m->mode == 6 ? bmkRd32(b + p + 16) : 0u;   // №1010: краска тега — байты 10..13 поля имени (у прежних файлов нули)
    for (int i = 0; i < 16; i++) m->pal[i] = i < npal ? (v == 3 ? shared[i] : bmkRd32(b + p + 20 + 4 * i)) : m->pal[0];
    p += rec; m->bits = b + p; p += bpp == 1 ? (m->w * m->h + 7) / 8 : bpp == 2 ? (m->w * m->h + 3) / 4 : (m->w * m->h + 1) / 2;
    if (p > len - 4) return -1;
  }
  if (p != len - 4) return -1;
  out->n = n; out->slotW = sw; out->slotH = sh; out->v = v; out->scale = sc; return n;
}
static int bmkPix(const BootMark *m, int x, int y) {
  int i = y * m->w + x;
  return m->bpp == 4 ? (m->bits[i >> 1] >> (i & 1 ? 0 : 4)) & 15 : m->bpp == 2 ? (m->bits[i >> 2] >> (6 - 2 * (i & 3))) & 3 : (m->bits[i >> 3] >> (7 - (i & 7))) & 1;
}
// раскладка (правка 203): BMK2 — ряды под знаком GONE справа, сколько слотов входит в его ширину (72 -> 3), знак по центру слота;
// BMK1 — прежний ряд из 6 мелких под проверками слева
#define BMK_ROW0 (BSQ_ROW0 + 9)
#define BMK_GAPX 8
#define BMK_GAPY 6
#define BMK_PER_ROW 6
static int bmkSW = BMK1_SLOT_W, bmkSH = BMK1_SLOT_H, bmkV = 1, bmkSc = 1;   // слот, версия и масштаб набора (bootSeqMarks)
static int bmkN = BMK_MAX;   // правка 254: знаков в наборе — для неполного последнего ряда и раскладки рядов по высоте
// Правка Роба 254 (03.10 18:1x): «вся правая часть занимается логотипами». Ряды знаков BMK2 разнесены по всей высоте правой
// части: от строки под знаком GONE до низа последней строки слева (BMK_BOTTOM); неполный последний ряд — по краям (2 знака:
// El Barto слева, старый PlayStation в правом углу) или посередине (1). BMK_254_OLD — прежняя плотная раскладка сверху
#ifndef BMK_BOTTOM
#define BMK_BOTTOM (BUI_Y0 + BUI_ROW_FOOT * BUI_CH + 8)
#endif
// Правка Роба 257 (03.10 20:3x): «как спонсорские, аккуратно снизу логотипа». BMK2 — поток: знаки идут рядами по ширине знака
// GONE (BOOTANIM_PW × BOOTANIM_SCALE), ряд — по центру, знак — по центру высоты ряда; блок начинается в 12 px под знаком GONE,
// между знаками BMK_FLOW_GX, между рядами BMK_FLOW_GY. bmkFlowLayout считает места по ширине каждого знака (набор bootSeqMarks);
// BMK_257_OLD — прежние слоты правки 254
#define BMK_FLOW_GX 10
#define BMK_FLOW_GY 8
static int bmkFlow, bmkFX[BMK_MAX], bmkFY[BMK_MAX], bmkFW[BMK_MAX], bmkFH[BMK_MAX], bmkFRows;
static void bmkFlowLayout(const int *w, const int *h, int n) {
  int rowW = BOOTANIM_PW * BOOTANIM_SCALE, y = BUI_LOGO_Y + BOOTANIM_PH * BOOTANIM_SCALE + 12, k = 0; bmkFRows = 0;
  while (k < n) {
    int j = k, tw = w[k], rh = h[k];
    while (j + 1 < n && tw + BMK_FLOW_GX + w[j + 1] <= rowW) { j++; tw += BMK_FLOW_GX + w[j]; if (h[j] > rh) rh = h[j]; }
    int x = BUI_LOGO_X + (rowW - tw) / 2, gaps = j - k, extra = 0;
#ifndef BMK_395_OLD
    // правка 395 (Роб 08.10: «плохо разместил, было лучше»): ряд из двух и более знаков — от края до края надписи GONE, как
    // принятый на PSP 06.10 набор (ряды 228 и 227 из 232 px); лишнее место делится между промежутками, одиночный знак — по центру
    if (gaps > 0) { x = BUI_LOGO_X; extra = rowW - tw; }
#endif
    for (int i = k; i <= j; i++) { bmkFX[i] = x; bmkFY[i] = y + (rh - h[i]) / 2; bmkFW[i] = w[i]; bmkFH[i] = h[i];
      x += w[i] + BMK_FLOW_GX + (gaps > 0 ? (extra * (i - k + 1)) / gaps - (extra * (i - k)) / gaps : 0); }
#ifndef BMK_395_OLD
    if (gaps == 2) {   // правка 396 (Роб 08.10): три знака — равные отступы: край, знак, знак, знак, край
      int free = rowW - (w[k] + w[k + 1] + w[k + 2]), xx = BUI_LOGO_X;
      for (int i = 0; i < 3; i++) { xx += (free * (i + 1)) / 4 - (free * i) / 4; bmkFX[k + i] = xx; xx += w[k + i]; }
      bmkFX[k] -= 2; bmkFX[k + 2] += 2;   // Роб 08.10: крайние на 2 px наружу
    }
#endif
    y += rh + BMK_FLOW_GY; k = j + 1; bmkFRows++;
  }
}
static int bmkPer(void) { int per = (BOOTANIM_PW * BOOTANIM_SCALE + BMK_GAPX) / (bmkSW + BMK_GAPX); return per < 1 ? 1 : per; }
static int bmkSlotX(int k) {
#ifdef BSQ_MARK_PLANT   // tests/bootlogo: отрицательный контроль — слот 1 сдвинут на знак GONE -> FAIL
  if (k == 1) return BUI_LOGO_X;
#endif
  if (bmkFlow && bmkV >= 2) return bmkFX[k];   // правка 257: левый верх самого знака
  if (bmkV >= 2) { int per = bmkPer(), col = k % per;
#ifndef BMK_254_OLD
                  int rows = (bmkN + per - 1) / per, row = k / per, inRow = row == rows - 1 ? bmkN - row * per : per;
                  if (inRow < per && per > 1) col = inRow == 1 ? per / 2 : col * (per - 1) / (inRow - 1);
#endif
                  return BUI_LOGO_X + col * (bmkSW + BMK_GAPX); }
  return BUI_X0 + (k % BMK_PER_ROW) * (bmkSW + BMK_GAPX);
}
static int bmkSlotY(int k) {
#ifdef BSQ_MARK_PLANT
  if (k == 1) return BUI_LOGO_Y;
#endif
  if (bmkFlow && bmkV >= 2) return bmkFY[k];
  if (bmkV >= 2) { int per = bmkPer(), top = BUI_LOGO_Y + BOOTANIM_PH * BOOTANIM_SCALE + 12, pitch = bmkSH + BMK_GAPY;
#ifndef BMK_254_OLD
                  int rows = (bmkN + per - 1) / per;
                  if (rows > 1 && (BMK_BOTTOM - top - bmkSH) / (rows - 1) > pitch) pitch = (BMK_BOTTOM - top - bmkSH) / (rows - 1);
#endif
                  return top + (k / per) * pitch; }
  return BUI_Y0 + BMK_ROW0 * BUI_CH + (k / BMK_PER_ROW) * (bmkSH + 16);
}
static const unsigned char bmkBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static long bootMarkPuts;   // счётчик put за кадр (tests/bootlogo: бюджет)
// Правка Роба 264: тег El Barto «распыляется», будто Барт тегнул экран: пиксель тега появляется, когда фронт баллончика (слева
// направо, по ходу письма) прошёл его колонку с разбросом брызг ±BMK_TAG_JIT; только что легший — светлее (краска ещё мокрая) на
// BMK_TAG_WET доли хода. Пиксель 2×2; фон тега не рисуется (тег только прибавляется — в обоих буферах кадра одно и то же).
#define BMK_TAG_LEN 48        // кадров распыления (0,8 с)
#define BMK_TAG_JIT .18f
#define BMK_TAG_WET .10f
static unsigned bmkHash(int x, int y) { unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u; h ^= h >> 13; h *= 0x5bd1e995u; return h ^ (h >> 15); }
static void bootTagFrame(const BootMark *m, int t, int ox, int oy, void (*put)(void *, int, int, unsigned), void *u) {
  float p = t >= BMK_TAG_LEN - 1 ? 1.3f : t <= 0 ? 0.f : (float)t / (BMK_TAG_LEN - 1) * 1.3f;   // 1,3: хвост — последние брызги легли
  for (int y = 0; y < m->h; y++)
    for (int x = 0; x < m->w; x++) {
      int i = bmkPix(m, x, y); if (!i) continue;
      float at = (float)x / (float)m->w + ((float)(bmkHash(x, y) & 1023) / 1023.f - .5f) * 2.f * BMK_TAG_JIT;
      if (at > p) continue;
      unsigned c = m->ink && !bootElbarto1010Old ? m->ink : m->pal[i];   // №1010: тег L6 — цветом краски с бака
      if (p < 1.3f && p - at < BMK_TAG_WET) c = bootAnimMix(c, 0xFFFFFFFFu, .45f * (1.f - (p - at) / BMK_TAG_WET));
      for (int sy = 0; sy < BMK_TAG_PX; sy++) for (int sx = 0; sx < BMK_TAG_PX; sx++) { put(u, ox + x * BMK_TAG_PX + sx, oy + y * BMK_TAG_PX + sy, c); bootMarkPuts++; }
    }
}
// Правка 397 (Роб 08.10: «пусть дрыгаются»): после появления знак с кадрами покоя (записи режима 7/8 с тем же именем в конце
// файла, не в потоке) сменяет их по кругу: 7 — раз в 6 кадров (цветок OpenAI крутится), 8 — раз в 24 (человечек Claude моргает,
// подпрыгивает). Смена кадра — только отличающиеся пиксели (бюджет кадра); полная отрисовка — на итоговом кадре
static int bmkIdleStep(int mode) { return mode == 7 ? 6 : mode == 8 ? 24 : 0; }
static void bmkIdleDraw(const BootMark *was, const BootMark *now, int ox, int oy, unsigned bg, void (*put)(void *, int, int, unsigned), void *u) {
  for (int y = 0; y < now->h; y++) for (int x = 0; x < now->w; x++) {
    int ci = bmkPix(now, x, y); unsigned c = ci ? now->pal[ci] : bg;
    if (was) { int pi = bmkPix(was, x, y); unsigned pc = pi ? was->pal[pi] : bg; if (pc == c) continue; }
    for (int sy = 0; sy < bmkSc; sy++) for (int sx = 0; sx < bmkSc; sx++) { put(u, ox + x * bmkSc + sx, oy + y * bmkSc + sy, c); bootMarkPuts++; }
  }
}

// кадр t (0..BMK_LEN-1; t >= BMK_LEN-1 — итоговый битмап) знака m в режиме mode, левый верх (ox, oy)
static void bootMarkFrame(const BootMark *m, int mode, int t, int ox, int oy, unsigned bg, void (*put)(void *, int, int, unsigned), void *u) {
  float p = t >= BMK_LEN - 1 ? 1.f : t <= 0 ? 0.f : (float)t / (BMK_LEN - 1);
  int w = m->w, h = m->h, blk = p < .25f ? 8 : p < .5f ? 4 : p < .75f ? 2 : 1, lvl = 1 + (int)(p * 3.f);
  float front = -BMK_WIDTH + p * (w + 2 * h + 2 * BMK_WIDTH + BMK_STAGE * BOOTANIM_NSYM);
  int scan = (int)(p * h), pen = (int)(p * w), B = p >= 1.f ? 8 : mode == 0 ? BOOTANIM_CELL : mode == 2 ? blk : 8;
  for (int y0 = 0; y0 < h; y0 += B)   // по блокам B×B: свойство блока (клетка со знаком, цвет большинства) — один раз на блок
    for (int x0 = 0; x0 < w; x0 += B) {
      int y1 = y0 + B < h ? y0 + B : h, x1 = x0 + B < w ? x0 + B : w, maj = 0, stage = -1;
      if (p < 1.f && (mode == 0 || mode == 2)) {
        int cnt[16] = {0};
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) cnt[bmkPix(m, x, y)]++;
        for (int j = 1; j < 16; j++) if (cnt[j] > cnt[maj]) maj = j;
        if (mode == 0 && cnt[0] < (x1 - x0) * (y1 - y0)) { float d = front - (float)(x0 + 2 * y0); stage = d < 0.f ? -1 : (int)(d / BMK_STAGE); }
      }
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          int i = bmkPix(m, x, y); unsigned c = bg;
          if (p >= 1.f) c = m->pal[i];
          else switch (mode) {
            case 0:   // волна как у GONE: клетка 4×4 со знаком проходит / | \ ■, потом рисунок с бликом; пустая клетка — фон
              if (stage < 0) break;
              if (stage < BOOTANIM_NSYM) { if (bootAnimGlyphCell(stage) >> ((y - y0) * BOOTANIM_CELL + (x - x0)) & 1) c = BOOTANIM_GLINT; }
              else if (i) { float dd = front - (float)(x + 2 * y), tt = dd / BMK_WIDTH; if (tt > 1.f) tt = 1.f; if (tt < 0.f) tt = 0.f;
                            c = dd < BMK_WIDTH ? bootAnimMix(BOOTANIM_GLINT, m->pal[i], tt * tt) : m->pal[i]; }
              else c = m->pal[0];
              break;
            case 1: if (y < scan) c = m->pal[i]; else if (y == scan && i) c = BOOTANIM_GLINT; break;   // развёртка строк
            case 2: c = m->pal[maj]; break;                                                          // мозаика: большинство блока
            case 3: if (bmkBayer[(y & 3) * 4 + (x & 3)] < (int)(p * 16.f)) c = m->pal[i]; break;    // растворение Байером
            case 4: if (m->bpp == 4) { if (bmkBayer[(y & 3) * 4 + (x & 3)] < (int)(p * 16.f)) c = m->pal[i]; }   // цветной: как растворение
                    else c = m->pal[i > 3 - lvl ? i - (3 - lvl) : 0]; break;                         // палитра NES: 3 шага
            default: if (x < pen) c = m->pal[i]; else if (x == pen && i) c = BOOTANIM_GLINT; break;   // перо колонками
          }
          for (int sy = 0; sy < bmkSc; sy++) for (int sx = 0; sx < bmkSc; sx++) { put(u, ox + x * bmkSc + sx, oy + y * bmkSc + sy, c); bootMarkPuts++; }
        }
    }
}
#endif
