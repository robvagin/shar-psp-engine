// 108 П1 (Fable §5.2): карта высот и карта стен уровня страницами вместо плотных массивов (на L1/L4 плотные hm+wl = 6,2 МБ при 35 % непустых ячеек слоя 0,
// 0,1 % слоя 1, слоях 2–3 пустых и 99,8 % ячеек стен «стены нет»). Значения те же бит в бит: страница — как есть, пустая страница — общая.
//   слой 0 высот: страницы 8×8 по 64 int16; слои 1–3: страницы 16×16 по 256×3 int16; стены: страницы 16×16 по 256×2 int16.
//   таблица страниц — uint16 на страницу: 0 — страница пустая (слой 0 и слои 1–3: все -32768; стены: у каждой ячейки w0 > w1), k>0 — k-я страница (с единицы).
// Хранилище лежит в lmap.shr версии 2 (сжато LZ4, tools/lmap_pack.c), в level.shr плотный массив остаётся как был (все инструменты читают его по-прежнему).
// P1PAGES_OLD=1 / файл p1pagesold рядом с EBOOT / flag p1pages 0 — плотный массив в блоке уровня, как раньше.
#ifndef LMAPPAGE_H
#define LMAPPAGE_H
#include <string.h>
#include <stdint.h>
#define LMPS_MAGIC 0x3153504cu   // 'LPS1'
typedef struct { uint32_t magic, hw, hh, n0, nu, nw, offT0, offTu, offTw, offP0, offPu, offPw, total, pad[3]; } LmpsHead;   // 64 Б
typedef struct { const unsigned short *t0, *tu, *tw; const short *p0, *pu, *pw; int hw, hh, w8, w16; } LmPg;
static const short lmpEmptyH[4] = {-32768, -32768, -32768, -32768};
static const short lmpEmptyW[2] = {32767, -32768};
// ячейка высот (4 слоя): указатель на 4 int16; tmp — буфер сборки, когда слои лежат в разных страницах
static inline const short *lmpHm(const LmPg *g, int gx, int gz, short *tmp) {
  unsigned k0 = g->t0[(gz >> 3) * g->w8 + (gx >> 3)], ku = g->tu[(gz >> 4) * g->w16 + (gx >> 4)];
  if (!(k0 | ku)) return lmpEmptyH;
  tmp[0] = k0 ? g->p0[(k0 - 1) * 64u + (((unsigned)gz & 7u) << 3 | ((unsigned)gx & 7u))] : (short)-32768;
  if (ku) { const short *u = g->pu + ((ku - 1) * 256u + (((unsigned)gz & 15u) << 4 | ((unsigned)gx & 15u))) * 3u; tmp[1] = u[0]; tmp[2] = u[1]; tmp[3] = u[2]; }
  else tmp[1] = tmp[2] = tmp[3] = -32768;
  return tmp;
}
static inline const short *lmpWl(const LmPg *g, int gx, int gz) {   // ячейка стен: 2 int16 (нижняя, верхняя); пустая страница — «стены нет» (w0 > w1)
  unsigned k = g->tw[(gz >> 4) * g->w16 + (gx >> 4)];
  return k ? g->pw + ((k - 1) * 256u + (((unsigned)gz & 15u) << 4 | ((unsigned)gx & 15u))) * 2u : lmpEmptyW;
}
// проверка хранилища и сборка читателя; 0 — годно. s — распакованные байты, n — их число, hw/hh — сетка из заголовка level.shr
static inline int lmpOpen(LmPg *g, const unsigned char *s, unsigned n, unsigned hw, unsigned hh) {
  if (n < sizeof(LmpsHead) || ((uintptr_t)s & 3)) return -1;
  const LmpsHead *h = (const LmpsHead *)s; unsigned w8 = (hw + 7) / 8, h8 = (hh + 7) / 8, w16 = (hw + 15) / 16, h16 = (hh + 15) / 16;
  if (h->magic != LMPS_MAGIC || h->hw != hw || h->hh != hh || h->total != n) return -2;
  if (h->n0 > 65535 || h->nu > 65535 || h->nw > 65535) return -3;
  if ((uint64_t)h->offT0 + (uint64_t)w8 * h8 * 2 > n || (uint64_t)h->offTu + (uint64_t)w16 * h16 * 2 > n || (uint64_t)h->offTw + (uint64_t)w16 * h16 * 2 > n) return -4;
  if ((uint64_t)h->offP0 + (uint64_t)h->n0 * 128 > n || (uint64_t)h->offPu + (uint64_t)h->nu * 1536 > n || (uint64_t)h->offPw + (uint64_t)h->nw * 1024 > n) return -5;
  if ((h->offT0 | h->offTu | h->offTw | h->offP0 | h->offPu | h->offPw) & 1) return -6;
  g->t0 = (const unsigned short *)(s + h->offT0); g->tu = (const unsigned short *)(s + h->offTu); g->tw = (const unsigned short *)(s + h->offTw);
  g->p0 = (const short *)(s + h->offP0); g->pu = (const short *)(s + h->offPu); g->pw = (const short *)(s + h->offPw);
  g->hw = (int)hw; g->hh = (int)hh; g->w8 = (int)w8; g->w16 = (int)w16;
  for (unsigned i = 0; i < w8 * h8; i++) if (g->t0[i] > h->n0) return -7;
  for (unsigned i = 0; i < w16 * h16; i++) if (g->tu[i] > h->nu || g->tw[i] > h->nw) return -8;
  return 0;
}
// размер хранилища по плотным массивам (для инструмента): хранилище строит lmpBuild
#ifdef LMP_BUILD
#include <stdlib.h>
static inline unsigned lmpAl16(unsigned v) { return (v + 15u) & ~15u; }
// hm — hw*hh*4 int16, wl — hw*hh*2 int16; out — буфер достаточного размера (плотный размер + 4 КБ хватает); возвращает размер хранилища
static unsigned lmpBuild(const short *hm, const short *wl, unsigned hw, unsigned hh, unsigned char *out) {
  unsigned w8 = (hw + 7) / 8, h8 = (hh + 7) / 8, w16 = (hw + 15) / 16, h16 = (hh + 15) / 16;
  unsigned n0 = 0, nu = 0, nw = 0;
  unsigned short *t0 = calloc(w8 * h8, 2), *tu = calloc(w16 * h16, 2), *tw = calloc(w16 * h16, 2);
  for (unsigned py = 0; py < h8; py++) for (unsigned px = 0; px < w8; px++) { int ne = 0;
    for (unsigned y = 0; y < 8 && !ne; y++) for (unsigned x = 0; x < 8; x++) { unsigned gx = px * 8 + x, gz = py * 8 + y; if (gx < hw && gz < hh && hm[((size_t)gz * hw + gx) * 4] != -32768) { ne = 1; break; } }
    if (ne) t0[py * w8 + px] = (unsigned short)++n0; }
  for (unsigned py = 0; py < h16; py++) for (unsigned px = 0; px < w16; px++) { int ne = 0, nv = 0;
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 16; x++) { unsigned gx = px * 16 + x, gz = py * 16 + y; if (gx >= hw || gz >= hh) continue;
      const short *c = hm + ((size_t)gz * hw + gx) * 4; if (c[1] != -32768 || c[2] != -32768 || c[3] != -32768) ne = 1;
      const short *w = wl + ((size_t)gz * hw + gx) * 2; if (w[0] <= w[1]) nv = 1; }
    if (ne) tu[py * w16 + px] = (unsigned short)++nu; if (nv) tw[py * w16 + px] = (unsigned short)++nw; }
  LmpsHead h; memset(&h, 0, sizeof h); h.magic = LMPS_MAGIC; h.hw = hw; h.hh = hh; h.n0 = n0; h.nu = nu; h.nw = nw;
  unsigned p = (unsigned)sizeof h;
  h.offT0 = p; p = lmpAl16(p + w8 * h8 * 2); h.offTu = p; p = lmpAl16(p + w16 * h16 * 2); h.offTw = p; p = lmpAl16(p + w16 * h16 * 2);
  h.offP0 = p; p = lmpAl16(p + n0 * 128); h.offPu = p; p = lmpAl16(p + nu * 1536); h.offPw = p; p = lmpAl16(p + nw * 1024); h.total = p;
  memset(out, 0, p); memcpy(out, &h, sizeof h); memcpy(out + h.offT0, t0, w8 * h8 * 2); memcpy(out + h.offTu, tu, w16 * h16 * 2); memcpy(out + h.offTw, tw, w16 * h16 * 2);
  short *p0 = (short *)(out + h.offP0), *pu = (short *)(out + h.offPu), *pw = (short *)(out + h.offPw);
  for (unsigned py = 0; py < h8; py++) for (unsigned px = 0; px < w8; px++) { unsigned k = t0[py * w8 + px]; if (!k) continue;
    for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++) { unsigned gx = px * 8 + x, gz = py * 8 + y; p0[(k - 1) * 64 + (y << 3 | x)] = (gx < hw && gz < hh) ? hm[((size_t)gz * hw + gx) * 4] : (short)-32768; } }
  for (unsigned py = 0; py < h16; py++) for (unsigned px = 0; px < w16; px++) { unsigned ku = tu[py * w16 + px], kw = tw[py * w16 + px];
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 16; x++) { unsigned gx = px * 16 + x, gz = py * 16 + y; int in = gx < hw && gz < hh;
      if (ku) { short *u = pu + ((ku - 1) * 256 + (y << 4 | x)) * 3; for (int l = 0; l < 3; l++) u[l] = in ? hm[((size_t)gz * hw + gx) * 4 + 1 + l] : (short)-32768; }
      if (kw) { short *w = pw + ((kw - 1) * 256 + (y << 4 | x)) * 2; w[0] = in ? wl[((size_t)gz * hw + gx) * 2] : (short)32767; w[1] = in ? wl[((size_t)gz * hw + gx) * 2 + 1] : (short)-32768; } } }
  free(t0); free(tu); free(tw); return p;
}
#endif
#endif
