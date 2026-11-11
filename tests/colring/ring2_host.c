// Коллизия-96, этап 2: тождество НАСТОЯЩЕГО кольца (viewer/colring.c + col.c при COLRING_OLD=0) с прежним COL1. Игра не меняется.
//   ring2_host MODE LEVEL.shr FILE.colring N SEED RING_R DIGEST [colbake.shr]
//   a — эталон: прежний COL1 целиком (colInitBake), по запросу пишется 8-байтный отпечаток
//   b — кольцо: colInitRing, кольцо за фокусом (colRingTick раз в 256 запросов, каждые 8192 — телепорт), недостающее дочитывается синхронно
//   r — красный контроль: синхронная страховка выключена (запрос вне кольца получает «пусто») — отпечатки обязаны разойтись
//   c — красный контроль 2: кольцо без закрытия по ящикам/заборам и без латки: отпечатки обязаны разойтись (портим байт малого блока)
// Запросы строятся из исходных массивов (одни и те же в a и b), номера примитивов не меняются.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include "col.h"
#include "colring.h"

extern int colRingTestNoSync;
static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static float rf(void) { return (rnd() & 0xffffff) / 16777216.f; }
static float rr(float a, float b) { return a + (b - a) * rf(); }
static uint8_t *file_read(const char *p, size_t *n) {
  FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
  fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *b = malloc(*n); if (fread(b, 1, *n, f) != *n) exit(2); fclose(f); return b;
}
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
unsigned mcProbeHash(const void *p, unsigned n) {   // копия msncar.c (тест сверяет mcHash в файле кольца с настоящим)
  const unsigned char *b = (const unsigned char *)p; unsigned h = 2166136261u ^ n, i = 0;
  for (; i + 4 <= n; i += 4) { unsigned w = (unsigned)b[i] | (unsigned)b[i + 1] << 8 | (unsigned)b[i + 2] << 16 | (unsigned)b[i + 3] << 24; h = (h ^ w) * 16777619u; h ^= h >> 15; }
  for (; i < n; i++) h = (h ^ b[i]) * 16777619u;
  return h;
}
typedef struct { char magic[4]; float ox, oz, cell; uint32_t W, H, ntri, nbox, ncyl, nseg, nref, flags; } Hdr;
static uint8_t *secA; static size_t secAsz; static Hdr HA;
static const uint8_t *triA;
static void parseA(const uint8_t *lvl, size_t lsz) {
  uint32_t off = *(const uint32_t *)(lvl + 92), pvs = *(const uint32_t *)(lvl + 88), prp = *(const uint32_t *)(lvl + 96), end = 0;
  if (pvs && (!end || pvs < end)) end = pvs; if (prp && (!end || prp < end)) end = prp; if (!end) end = (uint32_t)lsz;
  secAsz = end - off; secA = aligned_alloc(16, (secAsz + 15) & ~15ull); memcpy(secA, lvl + off, secAsz);
  memcpy(&HA, secA, sizeof HA); triA = secA + 48;
}
static int ioFd; static long long ioBytes; static unsigned ioCalls;
static int ioRead(void *u, unsigned off, void *dst, unsigned n) { ioCalls++; ioBytes += n; return pread(ioFd, dst, n, off) == (ssize_t)n ? 0 : -1; }
static unsigned logN; static void ioLog(const char *s) { if (getenv("RING_VERBOSE") && logN++ < 40) fprintf(stderr, "%s\n", s); }

static float *tc; static uint32_t ntc, *tileStart, *tileList, TWq, THq;
static void buildCenters(void) {
  uint32_t nt = HA.ntri; tc = malloc(sizeof(float) * 3 * nt); ntc = 0;
  for (uint32_t i = 0; i < nt; i++) { const float *t = (const float *)(triA + (size_t)i * 48); if (t[10] < 0.5f) continue;
    tc[3 * ntc] = (t[0] + t[3] + t[6]) / 3; tc[3 * ntc + 1] = (t[1] + t[4] + t[7]) / 3; tc[3 * ntc + 2] = (t[2] + t[5] + t[8]) / 3; ntc++; }
  TWq = (HA.W + 15) / 16; THq = (HA.H + 15) / 16; uint32_t tilesN = TWq * THq;
  uint32_t *cntq = calloc(tilesN + 1, 4), *tt = malloc(4 * ntc);
  for (uint32_t i = 0; i < ntc; i++) { int gx = (int)((tc[3 * i] - HA.ox) / HA.cell) / 16, gz = (int)((tc[3 * i + 2] - HA.oz) / HA.cell) / 16; if (gx < 0) gx = 0; if (gz < 0) gz = 0; if (gx >= (int)TWq) gx = TWq - 1; if (gz >= (int)THq) gz = THq - 1; tt[i] = gz * TWq + gx; cntq[tt[i]]++; }
  tileStart = malloc(4 * (tilesN + 1)); tileStart[0] = 0; for (uint32_t i = 0; i < tilesN; i++) tileStart[i + 1] = tileStart[i] + cntq[i];
  tileList = malloc(4 * ntc); memset(cntq, 0, 4 * tilesN);
  for (uint32_t i = 0; i < ntc; i++) tileList[tileStart[tt[i]] + cntq[tt[i]]++] = i;
  free(cntq); free(tt);
}
static int pickNear(float fx, float fz, float d, float *p) {
  for (int tries = 0; tries < 40; tries++) {
    float x = fx + rr(-d, d), z = fz + rr(-d, d); if ((x - fx) * (x - fx) + (z - fz) * (z - fz) > d * d) continue;
    int gx = (int)((x - HA.ox) / HA.cell) / 16, gz = (int)((z - HA.oz) / HA.cell) / 16; if (gx < 0 || gz < 0 || gx >= (int)TWq || gz >= (int)THq) continue;
    uint32_t ti = gz * TWq + gx, n = tileStart[ti + 1] - tileStart[ti]; if (!n) continue;
    uint32_t i = tileList[tileStart[ti] + rnd() % n]; p[0] = tc[3 * i]; p[1] = tc[3 * i + 1]; p[2] = tc[3 * i + 2]; return 1;
  }
  return 0;
}
static void pickAny(float *p) { uint32_t i = rnd() % ntc; p[0] = tc[3 * i]; p[1] = tc[3 * i + 1]; p[2] = tc[3 * i + 2]; }
static uint64_t dig_carbox(uint64_t h, const CarBox *b) {
  h = fnv(h, &b->x, 4); h = fnv(h, &b->z, 4); h = fnv(h, &b->y, 4); h = fnv(h, &b->n, 4);
  int n = b->n > COL_MAX_CONTACTS ? COL_MAX_CONTACTS : b->n;
  for (int i = 0; i < n; i++) { h = fnv(h, &b->nx[i], 4); h = fnv(h, &b->nz[i], 4); h = fnv(h, &b->depth[i], 4); h = fnv(h, &b->type[i], 1); h = fnv(h, &b->index[i], 4); }
  h = fnv(h, &b->maxDepth, 4); h = fnv(h, &b->behindFence, 4); h = fnv(h, &b->nshapes, 4);
  return h;
}

static unsigned farPm = 20;   // дальних запросов (расстановка миссий, далёкий трафик) на тысячу; RING_FAR=пермилле (стресс 120)
int main(int argc, char **argv) {
  if (argc < 8) { fprintf(stderr, "ring2_host a|b|r|c LEVEL.shr FILE.colring N SEED RING_R DIGEST [colbake.shr]\n"); return 2; }
  const char mode = argv[1][0]; long N = atol(argv[4]); rs ^= (uint64_t)atoll(argv[5]) * 0x9E3779B97F4A7C15ull; if (!rs) rs = 1;
  if (getenv("RING_FAR")) farPm = (unsigned)atoi(getenv("RING_FAR"));
  float R = (float)atof(argv[6]); const char *digp = argv[7];
  size_t lsz; uint8_t *lvl = file_read(argv[2], &lsz); parseA(lvl, lsz); free(lvl);
  unsigned char *bake = 0; size_t bakeN = 0; ColRing *ring = 0; ColWorld w; int rc;
  if (mode == 'a') {
    if (argc > 8) bake = file_read(argv[8], &bakeN);
    rc = colInitBake(&w, secA, secAsz, bake, (unsigned)bakeN, mcProbeHash(secA, (unsigned)secAsz));
    if (bake && !colBakeLastUsed()) { fprintf(stderr, "эталон: colbake не принят\n"); return 3; }
  } else {
    ioFd = open(argv[3], O_RDONLY); if (ioFd < 0) { perror(argv[3]); return 2; }
    ColRingIO io = { ioRead, 0, ioLog }; int err = 0;
    unsigned poolPages; { uint32_t h[64]; if (pread(ioFd, h, sizeof h, 0) != sizeof h) return 2; poolPages = (R > 260.f && h[38] ? h[38] : h[35]) + 48; /* пик кольца 250 м (запас 10 % в файле) + кэш вне кольца */ }
    void *pool = malloc((size_t)poolPages * CRING_PAGEB);
    ring = colRingOpen(&io, pool, poolPages * CRING_PAGEB, &err); if (!ring) { fprintf(stderr, "colRingOpen %d\n", err); return 3; }
    if (ring->mch != mcProbeHash(secA, (unsigned)secAsz)) { fprintf(stderr, "ПРОВАЛ: mcHash в файле кольца %08x != настоящий %08x\n", ring->mch, mcProbeHash(secA, (unsigned)secAsz)); return 3; }
    if (mode == 'c') for (uint32_t i = 0; i < ring->nbox; i += 4) ring->small[(size_t)i * 64 + 3] ^= 0x40;   // испорчен резидентный блок ящиков (порядок центров ×2^64)
    colRingSetRadius(ring, R);
    rc = colInitRing(&w, ring);
    if (rc == 0 && !colBakeLastUsed()) { fprintf(stderr, "кольцо: colbake не принят\n"); return 3; }
    if (mode == 'r') colRingTestNoSync = 1;
  }
  if (rc) { fprintf(stderr, "init %d\n", rc); return 3; }
  buildCenters();
  FILE *df = fopen(digp, mode == 'a' ? "wb" : "rb"); if (!df) { perror(digp); return 2; }
  uint64_t *dref = 0; if (mode != 'a') { dref = malloc(8 * (size_t)N); if (fread(dref, 8, N, df) != (size_t)N) { fprintf(stderr, "мало отпечатков\n"); return 2; } }
  float fx, fz, fp[3], hd = 0.f; pickAny(fp); fx = fp[0]; fz = fp[2];
  if (ring) colRingEnsure(ring, fx, fz);
  long mism = 0, firstBad = -1, nNear = 0, nFar = 0; unsigned syncQ = 0; unsigned sb[6] = {0}; unsigned peakQ = 0, peakQat = 0, peakKind = 0, peakFar = 0; float peakDist = 0; unsigned peakRes = 0, maxWR = 0, maxUR = 0, maxTot = 0;
  for (long q = 0; q < N; q++) {
    if (q % 256 == 0) {   // «кадр»: фокус идёт по курсу 0,7 м за кадр (≈ 42 м/с при 60 FPS), курс иногда меняется; раз в 600 кадров — телепорт (миссия, дверь)
      int tele = q % (256 * 600) == 0 && q;
      if (tele) { pickAny(fp); fx = fp[0]; fz = fp[2]; if (ring) colRingEnsure(ring, fx, fz); }   // как переход «Переходов» п.14: кольцо готово до постановки
      else { if (rnd() % 300 == 0) hd = rr(0, 6.2831853f); fx += cosf(hd) * .7f; fz += sinf(hd) * .7f;
        { float pp[3]; if (!pickNear(fx, fz, 60.f, pp)) { pickAny(fp); fx = fp[0]; fz = fp[2]; if (ring) colRingEnsure(ring, fx, fz); } } }   // ушёл с карты: возрождение на дороге, как сторож игры
      if (ring) { colRingTick(ring, fx, fz); { unsigned wr = 0, ur = 0; for (uint32_t pp = 0; pp < ring->npages; pp++) if (ring->pg[pp]) { if (ring->wantPage[pp]) wr++; else ur++; } if (wr > maxWR) maxWR = wr; if (ur > maxUR) maxUR = ur; if (wr + ur > maxTot) maxTot = wr + ur; } unsigned rb = colRingResidentBytes(ring); if (rb > peakRes) peakRes = rb; }
    }
    float p[3]; int far = (rnd() % 1000) < farPm;
    if (far) { pickAny(p); nFar++; } else { if (!pickNear(fx, fz, 150.f, p)) pickAny(p); nNear++; }
    float dist = sqrtf((p[0] - fx) * (p[0] - fx) + (p[2] - fz) * (p[2] - fz));
    unsigned s0 = ring ? ring->st.syncPages + ring->st.syncTiles : 0;
    uint64_t h = 1469598103934665603ull; int kind = rnd() % 100 < 40 ? 0 : rnd() % 100 < 55 ? 1 : rnd() % 100 < 60 ? 2 : 3;
    if (kind == 0) {
      float y, n3[3] = {0}; int mat = 0; uint32_t ref = 0; float yt = p[1] + (rnd() & 1 ? 2.f : 40.f), yb = p[1] - (rnd() & 1 ? 30.f : 200.f);
      int r = colGroundMatId(&w, p[0] + rr(-3, 3), p[2] + rr(-3, 3), yt, yb, &y, n3, &mat, &ref);
      h = fnv(h, &r, 4); if (r) { h = fnv(h, &y, 4); h = fnv(h, n3, 12); h = fnv(h, &mat, 4); h = fnv(h, &ref, 4); }
    } else if (kind == 1) {
      float a[3] = {p[0], p[1] + 1.f, p[2]}, b[3]; float L = rr(2, 45), th = rr(0, 6.2831853f); b[0] = a[0] + cosf(th) * L; b[2] = a[2] + sinf(th) * L; b[1] = a[1] + rr(-4, 6);
      float f = colRayFrac(&w, a, b, rnd() & 1); h = fnv(h, &f, 4);
    } else {
      CarBox b; memset(&b, 0, sizeof b); float th = rr(0, 6.2831853f); b.x = p[0]; b.z = p[2]; b.fx = cosf(th); b.fz = sinf(th); b.hw = 1.0f; b.hl = 2.3f; b.y = p[1]; b.lo = .35f; b.hi = 1.6f; b.fsupOn = rnd() & 1; b.fsup = p[1];
      if (kind == 2) { int r = colProbeCar(&w, &b); h = fnv(h, &r, 4); }
      else { float mx = 0, mz = 0, nx = 0, nz = 0; int r = colPushCar(&w, &b, &mx, &mz, &nx, &nz); h = fnv(h, &r, 4); h = fnv(h, &mx, 4); h = fnv(h, &mz, 4); h = fnv(h, &nx, 4); h = fnv(h, &nz, 4); }
      h = dig_carbox(h, &b);
    }
    if (ring && ring->st.residentPages > peakQ) { peakQ = ring->st.residentPages; peakQat = q; peakKind = kind; peakFar = far; peakDist = dist; }
    if (ring && ring->st.syncPages + ring->st.syncTiles != s0) { syncQ++; int bk = dist < 150 ? 0 : dist < 200 ? 1 : dist < 250 ? 2 : dist < 300 ? 3 : dist < 500 ? 4 : 5; sb[bk]++; }
    if (mode == 'a') fwrite(&h, 8, 1, df);
    else if (dref[q] != h) { mism++; if (firstBad < 0) firstBad = q; }
  }
  if (mode == 'a') { fclose(df); printf("RESULT эталон: %ld запросов, ближних %ld, дальних %ld\n", N, nNear, nFar); return 0; }
  ColRingStats *S = &ring->st;
  printf("RESULT %s R=%.0f N=%ld mism=%ld first=%ld sync_q=%u sync(page/tile)=%u/%u prefetch(page/tile)=%u/%u evict=%u/%u ioErr=%u poolShort=%u heapPeak=%u wantPeak=%u | sync_q по дальности <150/<200/<250/<300/<500/далее: %u/%u/%u/%u/%u/%u\n",
         mode == 'r' ? "КРАСНЫЙ(без синхр.)" : mode == 'c' ? "КРАСНЫЙ(порча байта)" : "кольцо", R, N, mism, firstBad, syncQ, S->syncPages, S->syncTiles, S->loadPages, S->loadTiles, S->evictPages, S->evictTiles, S->ioErr, S->poolShort, S->heapPeak, S->wantPeak,
         sb[0], sb[1], sb[2], sb[3], sb[4], sb[5]);
  printf("TICK после кадра: ждали-резидент пик %u, вне кольца пик %u, всего пик %u (пул %u)\n", maxWR, maxUR, maxTot, ring->poolPages);
  printf("PEAKQ страниц %u на запросе %u kind=%u far=%u dist=%.0f\n", peakQ, peakQat, peakKind, peakFar, peakDist);
  printf("MEM резидент кольца пик %.2f МБ (страницы пик %u = %.2f МБ, блобы пик %.2f МБ, таблицы %.2f МБ), COL1 было %.2f МБ; чтений %u, %.1f МБ\n",
         peakRes / 1e6, S->residentPeak, S->residentPeak * CRING_PAGEB / 1e6, S->blobPeak / 1e6, (peakRes - 0.0) / 1e6 - S->residentPeak * CRING_PAGEB / 1e6 - S->blobPeak / 1e6, secAsz / 1e6, ioCalls, ioBytes / 1e6);
  if (mode != 'b') { printf(mism > 0 ? "RED ok (расхождения есть)\n" : "RED FAIL (красный не покраснел)\n"); return mism > 0 ? 0 : 1; }
  printf(mism == 0 ? "PASS\n" : "FAIL\n"); return mism == 0 ? 0 : 1;
}
