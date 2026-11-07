// uidiag.c — см. uidiag.h (№1186). Только целые числа; никаких выделений памяти.
#include <pspgu.h>
#include <string.h>
#include "uidiag.h"
#undef sceGuDrawArray
UiBig uiBigRec[UIDIAG_NBIG]; unsigned uiBigN; static unsigned bigTaken;
UiLast uiLastDraw;
unsigned uiDrawSeq, uiFrDraws, uiFrVerts, uiFrAreaPx, uiFrMaxVerts, uiFrMaxAreaPx;
unsigned uiSecDraws, uiSecVerts, uiSecMaxVerts, uiSecMaxAreaPx, uiSecAreaPx, uiSecFrames, uiSecFrAreaMax;
static int esz(int bits) { return bits == 1 ? 1 : bits == 2 ? 2 : bits == 3 ? 4 : 0; }
// положение x,y в вершине: смещение, тип (1 — s8, 2 — s16, 3 — f32), шаг; 0 — не разобрано (веса, индексы, ничего)
static int layout(int vt, int *off, int *kind, int *stride) {
  if (vt & (7 << 9)) return 0;   // веса
  int o = 0, al = 1, a;
  int t = esz(vt & 3), tn = t ? 2 * t : 0;   // текстура: 8/16/f32 × 2
  if (t) { a = t; o = (o + a - 1) & ~(a - 1); o += tn; if (a > al) al = a; }
  int c = (vt >> 2) & 7; int cs = c >= 4 && c <= 6 ? 2 : c == 7 ? 4 : 0;
  if (cs) { o = (o + cs - 1) & ~(cs - 1); o += cs; if (cs > al) al = cs; }
  int nn = (vt >> 5) & 3, ns = esz(nn);
  if (ns) { o = (o + ns - 1) & ~(ns - 1); o += 3 * ns; if (ns > al) al = ns; }
  int vk = (vt >> 7) & 3, vs = esz(vk);
  if (!vs) return 0;
  o = (o + vs - 1) & ~(vs - 1); *off = o; *kind = vk; o += 3 * vs; if (vs > al) al = vs;
  *stride = (o + al - 1) & ~(al - 1);
  return 1;
}
static void bound(const unsigned char *v, int n, int off, int kind, int stride, int *w, int *h) {
  float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f; int step = n > 64 ? n / 64 : 1;
  for (int i = 0; i < n; i += step) {
    const unsigned char *p = v + (unsigned)i * (unsigned)stride + off; float x, y;
    if (kind == 3) { x = ((const float *)p)[0]; y = ((const float *)p)[1]; }
    else if (kind == 2) { x = ((const short *)p)[0]; y = ((const short *)p)[1]; }
    else { x = ((const signed char *)p)[0]; y = ((const signed char *)p)[1]; }
    if (!(x == x) || !(y == y)) continue;
    if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
  }
  if (x1 < x0 || y1 < y0) { *w = *h = 0; return; }
  float fw = x1 - x0, fh = y1 - y0;
  *w = fw > 2e9f ? 2000000000 : (int)fw; *h = fh > 2e9f ? 2000000000 : (int)fh;
}
void uiDiagDraw(int prim, int vt, int n, const void *idx, const void *v, const char *fn, int line) {
  int w = 0, h = 0, off, kind, stride;
  if ((vt & GU_TRANSFORM_2D) && v && n > 0 && layout(vt, &off, &kind, &stride)) bound((const unsigned char *)v, n, off, kind, stride, &w, &h);
  unsigned area = (unsigned)(((long long)w * h) > 0x7fffffffLL ? 0x7fffffff : (long long)w * h);
  uiDrawSeq++; uiFrDraws++; uiFrVerts += (unsigned)n; uiFrAreaPx += area;
  if ((unsigned)n > uiFrMaxVerts) uiFrMaxVerts = (unsigned)n;
  if (area > uiFrMaxAreaPx) uiFrMaxAreaPx = area;
  uiLastDraw.fn = fn; uiLastDraw.line = line; uiLastDraw.n = n; uiLastDraw.vt = vt; uiLastDraw.w = w; uiLastDraw.h = h; uiLastDraw.seq = uiDrawSeq;
  if (n > UIDIAG_VERTS || area > 480u * 272u * UIDIAG_AREAK) {
    unsigned i = 0; for (; i < uiBigN && i < UIDIAG_NBIG; i++) if (uiBigRec[i].fn == fn && uiBigRec[i].line == line) break;
    if (i < UIDIAG_NBIG) {
      if (i == uiBigN) { uiBigRec[i].fn = fn; uiBigRec[i].line = line; uiBigRec[i].n = n; uiBigRec[i].vt = vt; uiBigRec[i].w = w; uiBigRec[i].h = h; uiBigRec[i].cnt = 0; uiBigRec[i].seq = uiDrawSeq; uiBigN++; }
      uiBigRec[i].cnt++;
      if ((unsigned)n > (unsigned)uiBigRec[i].n) uiBigRec[i].n = n;
      if (w > uiBigRec[i].w) uiBigRec[i].w = w;
      if (h > uiBigRec[i].h) uiBigRec[i].h = h;
    }
  }
  sceGuDrawArray(prim, vt, n, idx, v);
}
void uiDiagFrame(void) {
  uiSecFrames++; uiSecDraws += uiFrDraws; uiSecVerts += uiFrVerts; uiSecAreaPx += uiFrAreaPx;
  if (uiFrMaxVerts > uiSecMaxVerts) uiSecMaxVerts = uiFrMaxVerts;
  if (uiFrMaxAreaPx > uiSecMaxAreaPx) uiSecMaxAreaPx = uiFrMaxAreaPx;
  if (uiFrAreaPx > uiSecFrAreaMax) uiSecFrAreaMax = uiFrAreaPx;
  uiFrDraws = uiFrVerts = uiFrAreaPx = uiFrMaxVerts = uiFrMaxAreaPx = 0;
}
int uiDiagTakeBig(UiBig *out) {
  if (bigTaken >= uiBigN || bigTaken >= UIDIAG_NBIG) return 0;
  *out = uiBigRec[bigTaken]; bigTaken++; return 1;
}
