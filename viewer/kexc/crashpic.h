// crashpic.h — рисунок экрана вылета (№1208): картинка SHXC на весь экран 480×272 (ближайший сосед) прямо в кадровый буфер процессором, без системных вызовов и без libc: годится для
// обработчика исключения и для хост-теста tests/g10b/crashpic_host.c. fmt — формат дисплея PSP: 0 565, 1 5551, 2 4444, 3 8888. Возврат 1 — нарисовано, 0 — картинка негодна.
// Формат: 'SHXC', u16 w, u16 h, u16 ncolors (<=256), u16 packed (0 — индексы как есть; 1 — сжатые), палитра ncolors*4 (R,G,B,A);
//   packed=0: w*h индексов построчно; packed=1: u32 clen и clen байт потока (байт c: c<128 — следующие c+1 байт как есть; c>=128 — следующий байт повторить c-126 раз, 2..129), в сумме w*h индексов.
#ifndef CRASHPIC_H
#define CRASHPIC_H
// длина всей картинки в байтах по заголовку; 0 — заголовок негоден
static unsigned crashPicSize(const unsigned char *p, unsigned n) {
  if (!p || n < 16 || p[0] != 'S' || p[1] != 'H' || p[2] != 'X' || p[3] != 'C') return 0;
  unsigned w = p[4] | p[5] << 8, h = p[6] | p[7] << 8, nc = p[8] | p[9] << 8, pk = p[10] | p[11] << 8;
  if (!w || !h || w > 480 || h > 272 || !nc || nc > 256 || pk > 1) return 0;
  unsigned o = 12 + nc * 4;
  if (!pk) return n >= o + w * h ? o + w * h : 0;
  if (n < o + 4) return 0;
  unsigned cl = p[o] | p[o + 1] << 8 | p[o + 2] << 16 | (unsigned)p[o + 3] << 24;
  return cl && n >= o + 4 + cl ? o + 4 + cl : 0;
}
static int crashPicDraw(const unsigned char *p, unsigned n, void *vram, int bw, int fmt) {
  if (!crashPicSize(p, n)) return 0;
  unsigned w = p[4] | p[5] << 8, h = p[6] | p[7] << 8, nc = p[8] | p[9] << 8, pk = p[10] | p[11] << 8;
  const unsigned char *pal = p + 12, *px = pal + nc * 4;
  unsigned col[256];
  for (unsigned i = 0; i < nc; i++) { unsigned r = pal[4 * i], g = pal[4 * i + 1], b = pal[4 * i + 2];
    col[i] = fmt == 3 ? (0xff000000u | b << 16 | g << 8 | r) : fmt == 0 ? ((b >> 3) << 10 | (g >> 2) << 5 | (r >> 3)) : fmt == 1 ? (0x8000u | (b >> 3) << 10 | (g >> 3) << 5 | (r >> 3)) : (0xf000u | (b >> 4) << 8 | (g >> 4) << 4 | (r >> 4)); }
  unsigned char row[480]; const unsigned char *rs = px + 4, *re = rs + (px[0] | px[1] << 8 | px[2] << 16 | (unsigned)px[3] << 24);   // сжатый поток (при packed=1)
  unsigned rep = 0, lit = 0; unsigned char val = 0; int have = -1;   // состояние распаковщика; have — какая строка источника сейчас в row
  for (unsigned y = 0; y < 272; y++) {
    int sy = (int)(y * h / 272);
    if (!pk) { const unsigned char *r = px + (unsigned)sy * w; for (unsigned x = 0; x < w; x++) row[x] = r[x]; have = sy; }
    else while (have < sy) {   // распаковать следующую строку источника
      for (unsigned x = 0; x < w; x++) {
        if (!rep && !lit) { if (rs >= re) { row[x] = 0; continue; } unsigned c = *rs++; if (c < 128) lit = c + 1; else { rep = c - 126; val = rs < re ? *rs++ : 0; } }
        if (rep) { row[x] = val; rep--; } else { row[x] = rs < re ? *rs++ : 0; lit--; }
      }
      have++;
    }
    if (fmt == 3) { unsigned *d = (unsigned *)vram + y * (unsigned)bw; for (unsigned x = 0; x < 480; x++) d[x] = col[row[x * w / 480] % nc]; }
    else { unsigned short *d = (unsigned short *)vram + y * (unsigned)bw; for (unsigned x = 0; x < 480; x++) d[x] = (unsigned short)col[row[x * w / 480] % nc]; }
  }
  return 1;
}
#endif
