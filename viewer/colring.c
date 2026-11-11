// Коллизия-96: COL1 кольцом, см. colring.h. Только главный поток. Чистый C99, без PSP-заголовков (ввод-вывод через ColRingIO).
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "colring.h"

#define KEEP_TICKS 300u      // дочитанное вне кольца живёт столько кадров без обращений (≈ 5 с); обращение через *Sync обновляет давность
#define CACHE_PAGES 48u      // не больше стольких страниц вне кольца (≈ 0,6 МБ): NPC и машины миссий в десятке мест города держат по 2-3 страницы каждый
#define CACHE_TILES 32u
#define BUDGET_PAGES 2u      // подгрузка за кадр
#define BUDGET_TILES 1u
#define REPLAN_M 8.f         // кольцо пересчитывается, когда фокус уехал дальше
#define LOGMAX 32u

#ifdef COLRING_TEST_HOOKS
int colRingTestNoSync;   // тест (красный контроль): синхронная страховка отключена — запрос вне кольца получает «пусто» (то, чего в игре быть не должно)
#endif
static void rlog(ColRing *r, const char *s) { if (r->io.log) r->io.log(s); }
static int rd(ColRing *r, unsigned off, void *dst, unsigned n) { return r->io.read(r->io.user, off, dst, n); }

static float tileDist2(const ColRing *r, uint32_t t, float fx, float fz);
static void *rdAlloc(ColRing *r, unsigned off, unsigned n, int *err, int code) {
  if (!n) return 0;
  void *p = malloc(n); if (!p) { *err = code; return 0; }
  if (rd(r, off, p, n)) { free(p); *err = code - 1; return 0; }
  return p;
}

ColRing *colRingOpen(const ColRingIO *io, void *poolMem, unsigned poolBytes, int *errOut) {
  int e = 0; if (!errOut) errOut = &e; *errOut = 0;
  ColRing *r = (ColRing *)calloc(1, sizeof *r); if (!r) { *errOut = -1; return 0; }
  r->io = *io;
  uint32_t h[64];
  if (rd(r, 0, h, sizeof h)) { *errOut = -2; free(r); return 0; }
  if (memcmp(h, "COLR", 4) || h[1] != 2) { *errOut = -3; free(r); return 0; }
  float f3[3]; memcpy(f3, &h[2], 12); r->ox = f3[0]; r->oz = f3[1]; r->cell = f3[2];
  r->W = h[5]; r->H = h[6]; r->ntri = h[7]; r->nbox = h[8]; r->ncyl = h[9]; r->nseg = h[10]; r->nref = h[11];
  if (h[12] != CRING_TL || h[15] != CRING_TP || !(r->cell > 0.f) || !r->W || !r->H) { *errOut = -4; free(r); return 0; }
  r->TW = h[13]; r->TH = h[14]; r->npages = h[16]; r->crc = h[17]; r->mch = h[18]; r->secSize = h[19]; r->fp = h[20]; memcpy(&r->minY, &h[21], 4); r->flags = h[22];
  uint32_t offSlot = h[23], offSmall = h[24]; r->offTri = h[25]; uint32_t offTile = h[26], offTilePg = h[27]; r->offBlob = h[28], r->lenJmp = h[30], r->lenSrf = h[32], r->lenBake = h[34];
  uint32_t offJmp = h[29], offSrf = h[31], offBake = h[33]; r->need250pg = h[35]; r->need250blob = h[36]; r->need340pg = h[38]; r->need340blob = h[39];
  if (r->TW != (r->W + 15) / 16 || r->TH != (r->H + 15) / 16 || r->npages != (r->ntri + CRING_TP - 1) / CRING_TP) { *errOut = -5; free(r); return 0; }
  r->ntile = r->TW * r->TH; int er = 0;
  r->slot = (uint32_t *)rdAlloc(r, offSlot, r->ntri * 4, &er, -10);
  r->small = (uint8_t *)rdAlloc(r, offSmall, r->nbox * 64 + r->ncyl * 48 + r->nseg * 32, &er, -12);
  r->tile = (ColRingTile *)rdAlloc(r, offTile, r->ntile * (unsigned)sizeof(ColRingTile), &er, -14);
  uint32_t npgl = 0; if (r->tile) for (uint32_t t = 0; t < r->ntile; t++) npgl += r->tile[t].npg;
  r->tilePg = (uint32_t *)rdAlloc(r, offTilePg, npgl * 4, &er, -16);
  r->jmp = (uint8_t *)rdAlloc(r, offJmp, r->lenJmp, &er, -18);
  r->srf = (uint8_t *)rdAlloc(r, offSrf, r->lenSrf, &er, -20);
  r->bake = (uint8_t *)rdAlloc(r, offBake, r->lenBake, &er, -22);
  r->pg = (const ColTri **)calloc(r->npages + 1, sizeof(ColTri *)); r->pgSrc = (uint8_t *)calloc(r->npages + 1, 1);
  r->blob = (const uint32_t **)calloc(r->ntile, sizeof(uint32_t *));
  r->pgFar = (const ColTri **)calloc(r->npages + 1, sizeof(ColTri *)); r->blobFar = (const uint32_t **)calloc(r->ntile, sizeof(uint32_t *));
  r->wantTile = (uint8_t *)calloc(r->ntile, 1); r->wantPage = (uint8_t *)calloc(r->npages + 1, 1);
  r->stampTile = (uint32_t *)calloc(r->ntile, 4); r->stampPage = (uint32_t *)calloc(r->npages + 1, 4);
  r->order = (uint32_t *)malloc(r->ntile * 4 + 4); r->pendT = (uint32_t *)malloc(r->ntile * 4 + 4); r->pendP = (uint32_t *)malloc((size_t)(npgl + r->npages + 1) * 4);
  if (er || (r->ntri && !r->slot) || (!r->small && (r->nbox + r->ncyl + r->nseg)) || !r->tile || (npgl && !r->tilePg) || !r->pg || !r->pgSrc || !r->blob || !r->pgFar || !r->blobFar || !r->wantTile || !r->wantPage ||
      !r->stampTile || !r->stampPage || !r->order || !r->pendT || !r->pendP || (r->lenJmp && !r->jmp)) { *errOut = er ? er : -30; colRingClose(r); return 0; }
  r->poolMem = (uint8_t *)poolMem; r->poolPages = poolMem ? poolBytes / CRING_PAGEB : 0;
  if (r->poolPages) { r->freeStk = (uint16_t *)malloc(r->poolPages * 2); if (!r->freeStk) { *errOut = -31; colRingClose(r); return 0; }
    for (uint32_t i = 0; i < r->poolPages; i++) r->freeStk[i] = (uint16_t)(r->poolPages - 1 - i); r->nfree = r->poolPages; }
  r->R = 250.f; r->tickNo = 1;
  return r;
}

static void pageFree(ColRing *r, uint32_t p) {   // страница из плана (pg) или из дальнего кэша (pgFar)
  const ColTri *x = r->pg[p] ? r->pg[p] : r->pgFar[p]; if (!x) return;
  if (r->pgSrc[p] == 1) r->freeStk[r->nfree++] = (uint16_t)(((const uint8_t *)x - r->poolMem) / CRING_PAGEB);
  else { free((void *)x); if (r->st.heapPages) r->st.heapPages--; }
  r->pg[p] = 0; r->pgFar[p] = 0; r->pgSrc[p] = 0; r->st.residentPages--; r->st.evictPages++;
}
static void tileFree(ColRing *r, uint32_t t) {
  const uint32_t *x = r->blob[t] ? r->blob[t] : r->blobFar[t]; if (!x) return;
  free((void *)x); r->blob[t] = 0; r->blobFar[t] = 0; r->st.blobBytes -= r->tile[t].bytes; r->st.evictTiles++;
}
void colRingClose(ColRing *r) {
  if (!r) return;
  if (r->pg) for (uint32_t p = 0; p < r->npages; p++) { if (r->pg[p] && r->pgSrc[p] == 2) free((void *)r->pg[p]); if (r->pgFar && r->pgFar[p] && r->pgSrc[p] == 2) free((void *)r->pgFar[p]); }
  if (r->blob) for (uint32_t t = 0; t < r->ntile; t++) { free((void *)r->blob[t]); if (r->blobFar) free((void *)r->blobFar[t]); }
  free(r->pgFar); free(r->blobFar);
  free(r->slot); free(r->small); free(r->tile); free(r->tilePg); free(r->jmp); free(r->srf); free(r->bake);
  free(r->pg); free(r->pgSrc); free(r->blob); free(r->wantTile); free(r->wantPage); free(r->stampTile); free(r->stampPage);
  free(r->order); free(r->pendT); free(r->pendP); free(r->freeStk); free(r);
}

float colRingRadius(const ColRing *r) { return r->R; }
void colRingSetRadius(ColRing *r, float R) { if (R > 40.f) { r->R = R; r->planValid = 0; } }

// ---- загрузка одной страницы или тайла
static ColTri *pageAlloc(ColRing *r, int *src) {
  if (r->nfree) { *src = 1; return (ColTri *)(r->poolMem + (size_t)r->freeStk[--r->nfree] * CRING_PAGEB); }
  void *p = malloc(CRING_PAGEB); *src = 2;
  if (p) { r->st.poolShort++; if (++r->st.heapPages > r->st.heapPeak) r->st.heapPeak = r->st.heapPages; }
  return (ColTri *)p;
}
// Вне кольца дочитанное по запросу держим не больше CACHE_*: сверх — выгружается самое старое из ПРОШЛЫХ кадров (запрос не живёт дольше кадра, его указатели целы)
static void trimPages(ColRing *r) {
  for (;;) {
    unsigned n = 0, best = ~0u, bs = ~0u;
    for (uint32_t q = 0; q < r->npages; q++) if (r->pgFar[q]) { n++; if (r->stampPage[q] < r->tickNo && r->stampPage[q] < bs) { bs = r->stampPage[q]; best = q; } }
    if (n < CACHE_PAGES || best == ~0u) return;
    pageFree(r, best);
  }
}
static void trimTiles(ColRing *r) {
  for (;;) {
    unsigned n = 0, best = ~0u, bs = ~0u;
    for (uint32_t q = 0; q < r->ntile; q++) if (r->blobFar[q]) { n++; if (r->stampTile[q] < r->tickNo && r->stampTile[q] < bs) { bs = r->stampTile[q]; best = q; } }
    if (n < CACHE_TILES || best == ~0u) return;
    tileFree(r, best);
  }
}
static int pageLoad(ColRing *r, uint32_t p, int sync) {
  if (r->pg[p] || r->pgFar[p]) return 0;
  if (sync && !r->wantPage[p]) trimPages(r);
  int src = 0; ColTri *m = pageAlloc(r, &src);
  if (!m && r->inTick) {   // пул исчерпан и куча тоже: вне запроса можно вытеснить лишнюю страницу вне кольца
    for (uint32_t q = 0; q < r->npages && !m; q++) if (r->pgFar[q]) { pageFree(r, q); m = pageAlloc(r, &src); }
  }
  if (!m) { r->st.ioErr++; rlog(r, "WATCHDOG colring nomem page"); return -1; }
  if (rd(r, r->offTri + p * CRING_PAGEB, m, CRING_PAGEB)) {
    r->st.ioErr++; rlog(r, "WATCHDOG colring ioerr page");
    if (src == 1) r->freeStk[r->nfree++] = (uint16_t)(((uint8_t *)m - r->poolMem) / CRING_PAGEB); else { free(m); if (r->st.heapPages) r->st.heapPages--; }
    return -1; }
  if (sync && !r->wantPage[p]) r->pgFar[p] = m; else r->pg[p] = m;
  r->pgSrc[p] = (uint8_t)src; r->stampPage[p] = r->tickNo;
  if (++r->st.residentPages > r->st.residentPeak) r->st.residentPeak = r->st.residentPages;
  if (sync) r->st.syncPages++; else r->st.loadPages++;
  return 0;
}
static int tileLoad(ColRing *r, uint32_t t, int sync) {
  if (r->blob[t] || r->blobFar[t]) return 0;
  if (sync && !r->wantTile[t]) trimTiles(r);
  uint32_t *b = (uint32_t *)malloc(r->tile[t].bytes);
  if (!b || rd(r, r->offBlob + r->tile[t].off, b, r->tile[t].bytes)) { free(b); r->st.ioErr++; rlog(r, "WATCHDOG colring ioerr tile"); return -1; }
  if (sync && !r->wantTile[t]) r->blobFar[t] = b; else r->blob[t] = b;
  r->stampTile[t] = r->tickNo; r->st.blobBytes += r->tile[t].bytes; if (r->st.blobBytes > r->st.blobPeak) r->st.blobPeak = r->st.blobBytes;
  if (sync) r->st.syncTiles++; else r->st.loadTiles++;
  return 0;
}
static void syncLog(ColRing *r, const char *kind, uint32_t id) {
  r->nsync++; { int k = colQueryKind; if (k < 0 || k > 8) k = 0; r->st.syncByKind[k]++;
    if (k == 1) { int e = -1; for (int q = 0; q < 12; q++) { if (r->whoN[q] && r->whoRel[q] == colGroundRel) { e = q; break; } if (!r->whoN[q] && e < 0) e = q; }
      if (e >= 0) { r->whoRel[e] = colGroundRel; r->whoN[e]++; } } }
  if (r->nsync <= LOGMAX || (r->nsync & 255u) == 0) {
    char s[200]; float dx = 0, dz = 0;
    if (r->planValid) { dx = r->planX; dz = r->planZ; }
    snprintf(s, sizeof s, "WATCHDOG colring sync kind=%s id=%u n=%u focus=%.0f,%.0f q=%d rel=%ld", kind, id, r->nsync, dx, dz, colQueryKind, colGroundRel); rlog(r, s);
  }
}
static const ColTri *zeroPage(void) { static ColTri z[CRING_TP]; return z; }
static const uint32_t *emptyBlob(void) { static uint32_t z[260]; return z; }
const ColTri *colRingPageSync(ColRing *r, uint32_t p) {
  if (r->pgFar[p]) { r->stampPage[p] = r->tickNo; r->st.farHitPage++; return r->pgFar[p]; }   // дальний кэш: на карту не идём, давность обновлена
  if (r->wantPage[p]) r->st.syncWantPage++;
  syncLog(r, "page", p);
#ifdef COLRING_TEST_HOOKS
  if (colRingTestNoSync) return zeroPage();
#endif
  if (pageLoad(r, p, 1) || !(r->pg[p] || r->pgFar[p])) return zeroPage();   // карта не отдала страницу: пустой лист (громко в журнале), не падение
  return r->pg[p] ? r->pg[p] : r->pgFar[p];
}
const uint32_t *colRingTileSync(ColRing *r, uint32_t t) {
  if (r->blobFar[t]) { r->stampTile[t] = r->tickNo; r->st.farHitTile++; return r->blobFar[t]; }
  if (r->wantTile[t]) r->st.syncWantTile++;
  else if (r->planValid) { float d = sqrtf(tileDist2(r, t, r->planX, r->planZ)); int b = d < 250.f ? 0 : d < 300.f ? 1 : d < 350.f ? 2 : d < 400.f ? 3 : d < 500.f ? 4 : 5; r->st.syncTileDist[b]++; }
  syncLog(r, "tile", t);
#ifdef COLRING_TEST_HOOKS
  if (colRingTestNoSync) return emptyBlob();
#endif
  if (tileLoad(r, t, 1) || !(r->blob[t] || r->blobFar[t])) return emptyBlob();
  return r->blob[t] ? r->blob[t] : r->blobFar[t];
}

// ---- план кольца
static float tileDist2(const ColRing *r, uint32_t t, float fx, float fz) {
  uint32_t tx = t % r->TW, tz = t / r->TW; float s = CRING_TL * r->cell;
  float x0 = r->ox + tx * s, x1 = x0 + s, z0 = r->oz + tz * s, z1 = z0 + s;
  float dx = fmaxf(fmaxf(x0 - fx, 0.f), fx - x1), dz = fmaxf(fmaxf(z0 - fz, 0.f), fz - z1);
  return dx * dx + dz * dz;
}
static void replan(ColRing *r, float fx, float fz) {
  r->st.replans++; r->planX = fx; r->planZ = fz; r->planValid = 1;
  float R2 = r->R * r->R; r->norder = 0;
  float *key = (float *)malloc(r->ntile * sizeof(float)); if (!key) return;
  memset(r->wantTile, 0, r->ntile); memset(r->wantPage, 0, r->npages + 1);
  for (uint32_t t = 0; t < r->ntile; t++) { float d2 = tileDist2(r, t, fx, fz); key[t] = d2; if (d2 <= R2) { r->wantTile[t] = 1; r->order[r->norder++] = t; } }
  for (uint32_t i = 1; i < r->norder; i++) { uint32_t v = r->order[i]; int j = (int)i - 1; while (j >= 0 && key[r->order[j]] > key[v]) { r->order[j + 1] = r->order[j]; j--; } r->order[j + 1] = v; }
  free(key);
  r->npendT = r->npendP = 0;
  for (uint32_t i = 0; i < r->norder; i++) {   // сначала только отметки плана
    const ColRingTile *T = &r->tile[r->order[i]];
    for (uint32_t k = 0; k < T->npg; k++) r->wantPage[r->tilePg[T->pgOff + k]] |= 2;
  }
  { unsigned nw = 0; for (uint32_t p = 0; p < r->npages; p++) { r->wantPage[p] = (r->wantPage[p] & 2) ? 1 : 0; nw += r->wantPage[p]; } if (nw > r->st.wantPeak) r->st.wantPeak = nw; }
  // переезд между планом (pg, blob — быстрый путь) и дальним кэшем (pgFar, blobFar — через *Sync): без чтения карты
  unsigned cp = 0, ct = 0;
  for (uint32_t p = 0; p < r->npages; p++) {
    if (r->pgFar[p] && r->wantPage[p]) { r->pg[p] = r->pgFar[p]; r->pgFar[p] = 0; }
    else if (r->pg[p] && !r->wantPage[p]) {
      if (r->tickNo - r->stampPage[p] <= KEEP_TICKS && cp < CACHE_PAGES) { r->pgFar[p] = r->pg[p]; r->pg[p] = 0; cp++; } else pageFree(r, p); }
    else if (r->pgFar[p]) { if (r->tickNo - r->stampPage[p] <= KEEP_TICKS && cp < CACHE_PAGES) cp++; else pageFree(r, p); }
  }
  for (uint32_t t = 0; t < r->ntile; t++) {
    if (r->blobFar[t] && r->wantTile[t]) { r->blob[t] = r->blobFar[t]; r->blobFar[t] = 0; }
    else if (r->blob[t] && !r->wantTile[t]) {
      if (r->tickNo - r->stampTile[t] <= KEEP_TICKS && ct < CACHE_TILES) { r->blobFar[t] = r->blob[t]; r->blob[t] = 0; ct++; } else tileFree(r, t); }
    else if (r->blobFar[t]) { if (r->tickNo - r->stampTile[t] <= KEEP_TICKS && ct < CACHE_TILES) ct++; else tileFree(r, t); }
  }
  for (uint32_t i = 0; i < r->norder; i++) {   // списки подгрузки: ближние первыми
    uint32_t t = r->order[i]; const ColRingTile *T = &r->tile[t];
    if (!r->blob[t]) r->pendT[r->npendT++] = t;
    for (uint32_t k = 0; k < T->npg; k++) { uint32_t p = r->tilePg[T->pgOff + k]; if (r->wantPage[p] != 3) { r->wantPage[p] = 3; if (!r->pg[p]) r->pendP[r->npendP++] = p; } }
  }
  for (uint32_t p = 0; p < r->npages; p++) if (r->wantPage[p] == 3) r->wantPage[p] = 1;
}
static void serve(ColRing *r, unsigned bt, unsigned bp) {
  r->inTick = 1;
  // списки идут от ближнего к дальнему: берём с начала
  unsigned i = 0;
  while (i < r->npendT && bt) { uint32_t t = r->pendT[i++]; if (!r->blob[t]) { tileLoad(r, t, 0); bt--; } }
  if (i) { memmove(r->pendT, r->pendT + i, (r->npendT - i) * 4); r->npendT -= i; }
  i = 0;
  while (i < r->npendP && bp) { uint32_t p = r->pendP[i++]; if (!r->pg[p]) { pageLoad(r, p, 0); bp--; } }
  if (i) { memmove(r->pendP, r->pendP + i, (r->npendP - i) * 4); r->npendP -= i; }
  r->inTick = 0;
}
void colRingTick(ColRing *r, float fx, float fz) {
  r->tickNo++;
  float dx = fx - r->planX, dz = fz - r->planZ;
  if (!r->planValid || dx * dx + dz * dz > REPLAN_M * REPLAN_M || (r->tickNo & 63u) == 0) replan(r, fx, fz);
  if ((r->tickNo & 31u) == 0) {   // дальний кэш: не использовавшееся дольше KEEP_TICKS уходит
    for (uint32_t p = 0; p < r->npages; p++) if (r->pgFar[p] && r->tickNo - r->stampPage[p] > KEEP_TICKS) pageFree(r, p);
    for (uint32_t t = 0; t < r->ntile; t++) if (r->blobFar[t] && r->tickNo - r->stampTile[t] > KEEP_TICKS) tileFree(r, t); }
  trimPages(r); trimTiles(r);   // вне кольца не больше CACHE_*: накопленное за кадр (запросы вдаль) сбрасывается в начале следующего
  serve(r, BUDGET_TILES, BUDGET_PAGES);
}
void colRingEnsure(ColRing *r, float fx, float fz) {
  r->tickNo++; replan(r, fx, fz);
  while (r->npendT || r->npendP) serve(r, 64, 64);
}
unsigned colRingResidentBytes(const ColRing *r) {
  return r->st.residentPages * CRING_PAGEB + r->st.blobBytes + r->ntri * 4 + r->nbox * 64 + r->ncyl * 48 + r->nseg * 32 + r->ntile * (unsigned)sizeof(ColRingTile) + r->lenJmp + r->lenSrf + r->lenBake;
}
