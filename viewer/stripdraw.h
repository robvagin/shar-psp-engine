// К4: куски города неиндексными полосами (ПЛАН_ОПТИМИЗАЦИЙ 17.10 п.1; данные — tools/strips.c, SHR_STRIPS=1; формат в шапке tools/strips.c).
// Кусок с флагом F_STRIP: Rec.nv/ni/nf — длины СПИСКОВ (счётчики, проверки, проценты остаются прежними); блоб: заголовок 16 Б (nsvN, nsN, nsvF, nsF), вершины ближней версии,
// длины полос u16[nsN], вершины дальней, длины u16[nsF]. Piece.v — вершины ближней, idx — её длины, fidx — длины дальней; вершины дальней — stripFarV().
// Рисует drawGeo (одна команда GU_TRIANGLE_STRIP на полосу). Читатели на процессоре (срез у камеры ggCut, сетка камеры, лучи камеры) получают список:
//   pieceListP — постоянная развёртка куска (по требованию, живёт до выгрузки клетки; её читает GE), pieceListT — временная (слот 0..2, до следующего вызова слота).
// STRIPS_OLD=1 — полосы при загрузке клетки разворачиваются в списки (картинка та же, память прежняя; красный контроль гейта).
#ifndef STRIPS_OLD
#define STRIPS_OLD 0
#endif
#define F_STRIP (1u << 26)
static const unsigned char *pieceListP(Piece *d, int far, unsigned *n);
typedef struct { unsigned nsvN, nsN, nsvF, nsF; } StripHdr;
static inline const StripHdr *stripHdr(const Piece *d) { return (const StripHdr *)((const char *)d->v - 16); }
static inline unsigned stripVs(const Piece *d) { return (d->r.flags & F_V12) ? 12u : 16u; }
static inline const unsigned char *stripFarV(const Piece *d) { return (const unsigned char *)d->idx + ((stripHdr(d)->nsN * 2u + 3u) & ~3u); }
// полосы -> список треугольников (чётный треугольник k: v[k], v[k+1], v[k+2]; нечётный: v[k+1], v[k], v[k+2]); побайтно вырожденные выбрасываются.
// out == 0 — только счёт. Возвращает число вершин списка (кратно 3)
static unsigned stripExpand(const unsigned char *v, const unsigned short *len, unsigned ns, unsigned vs, unsigned char *out) {
  unsigned n = 0;
  for (unsigned s = 0; s < ns; s++) {
    unsigned L = len[s];
    for (unsigned k = 0; k + 2 < L; k++) {
      const unsigned char *a = v + (size_t)k * vs, *b = a + vs, *c = b + vs;
      if (k & 1) { const unsigned char *t = a; a = b; b = t; }
      if (!memcmp(a, b, vs) || !memcmp(b, c, vs) || !memcmp(a, c, vs)) continue;
      if (out) { memcpy(out + (size_t)n * vs, a, vs); memcpy(out + (size_t)(n + 1) * vs, b, vs); memcpy(out + (size_t)(n + 2) * vs, c, vs); }
      n += 3;
    }
    v += (size_t)L * vs;
  }
  return n;
}
static int stripsAsList, stripsCapture;   // служебно «flag stripslist 1»: полосы рисуются списком из развёртки (гейт: тот же кадр тем же путём), 2 — красный контроль (половина треугольников); нужен «flag dloff 1»
static void stripDraw(const Piece *d, int far, int vt) {   // одна команда PRIM на полосу
  if (stripsAsList) { unsigned n; const unsigned char *lv = pieceListP((Piece *)d, far, &n); if (lv && n >= 3) { if (stripsAsList == 2) n = (n / 6) * 3; if (n) sceGuDrawArray(GU_TRIANGLES, vt, (int)n, 0, lv); } return; }
  const StripHdr *h = stripHdr(d); const unsigned vs = stripVs(d);
  const unsigned char *v = far ? stripFarV(d) : (const unsigned char *)d->v; const unsigned short *len = far ? d->fidx : d->idx; unsigned ns = far ? h->nsF : h->nsN;
  for (unsigned k = 0; k < ns; k++) { sceGuDrawArray(GU_TRIANGLE_STRIP, vt, len[k], 0, v); v += (size_t)len[k] * vs; }
}
// оценка слов списка GE для листа клетки (по 8 слов на команду, как у списка: 72 + 8 на 65532 вершин)
static inline unsigned stripDlWords(const Piece *d, int far) { const StripHdr *h = stripHdr(d); return 16u + 8u * (far ? h->nsF : h->nsN); }
// постоянная развёртка (обе версии): блок {u32 nN, nF, offF, pad} + список ближней + список дальней; освобождает unloadCell (cpFree(unr))
static const unsigned char *pieceListP(Piece *d, int far, unsigned *n) {
  const unsigned vs = stripVs(d);
  if (!(d->r.flags & F_STRIP)) { *n = far ? d->r.nf : d->r.ni; return (const unsigned char *)d->v + (far ? (size_t)d->r.ni * vs : 0); }
  if (!d->unr) {
    const StripHdr *h = stripHdr(d); unsigned nN = stripExpand(d->v ? (const unsigned char *)d->v : 0, d->idx, h->nsN, vs, 0), nF = stripExpand(stripFarV(d), d->fidx, h->nsF, vs, 0);
    unsigned offF = (16u + nN * vs + 15u) & ~15u; unsigned char *b = (unsigned char *)cpAlloc((size_t)offF + (size_t)nF * vs + 16);
    if (!b) { *n = 0; return 0; }
    unsigned hd[4] = {nN, nF, offF, 0}; memcpy(b, hd, 16);
    stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs, b + 16); if (nF) stripExpand(stripFarV(d), d->fidx, h->nsF, vs, b + offF);
    sceKernelDcacheWritebackRange(b, offF + nF * vs); d->unr = b;
  }
  const unsigned *hd = (const unsigned *)d->unr; *n = far ? hd[1] : hd[0]; return (const unsigned char *)d->unr + (far ? hd[2] : 16);
}
// временная развёртка ближней версии для читателей на процессоре; слот — свой у каждого читателя (0 сетка камеры, 1 геометрия камеры, 2 лучи камеры), кэш по куску
static const unsigned char *pieceListT(const Piece *d, int slot, unsigned *n) {
  static struct { const void *key; unsigned n, cap; unsigned char *buf; } S[3];
  if (!(d->r.flags & F_STRIP)) { *n = d->r.ni; return (const unsigned char *)d->v; }
  if (S[slot].key == d->v) { *n = S[slot].n; return S[slot].buf; }
  const StripHdr *h = stripHdr(d); const unsigned vs = stripVs(d); unsigned cnt = stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs, 0);
  if (cnt * vs > S[slot].cap) { free(S[slot].buf); S[slot].cap = (cnt * vs + 4095u) & ~4095u; S[slot].buf = (unsigned char *)malloc(S[slot].cap); if (!S[slot].buf) { S[slot].cap = 0; S[slot].key = 0; *n = 0; return 0; } }
  stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs, S[slot].buf); S[slot].key = d->v; S[slot].n = cnt; *n = cnt; return S[slot].buf;
}
// загрузка клетки (загрузчик): указатели полосового куска и кэш срезов ggBatch по развёртке; STRIPS_OLD=1 — кусок целиком в список (память кучи, флаг снимается)
#include "reliab.h"
static volatile int stripScratchLock, stripScratchHeld; static unsigned char *stripScratch; static unsigned stripScratchCap;
static void stripScratchDelay(unsigned us) { sceKernelDelayThread(us); }
static unsigned char *stripScratchGet(unsigned bytes) {   // №1195: замок с пределом 3 с — не взят: 0 и строка LOCK в ящик (кусок остаётся без кэша срезов, как при нехватке памяти)
  stripScratchHeld = 0; if (!relSpinAcquire(&stripScratchLock, REL_LOCK_LIMIT_US, 50u, stripScratchDelay, "stripScratch")) return 0;
  stripScratchHeld = 1;
  if (bytes > stripScratchCap) { free(stripScratch); stripScratchCap = (bytes + 4095u) & ~4095u; stripScratch = (unsigned char *)malloc(stripScratchCap); if (!stripScratch) stripScratchCap = 0; }
  return stripScratch;
}
static void stripScratchPut(void) { if (stripScratchHeld) { stripScratchHeld = 0; __sync_lock_release(&stripScratchLock); } }   // №1195: отпускает только тот, кто взял
