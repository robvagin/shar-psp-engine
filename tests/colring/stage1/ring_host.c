// Коллизия-96, этап 1: тест тождества COL1-кольцом. Игра не меняется.
//   ring_host MODE LEVEL.shr [FILE.colring] N SEED [ringR] [flags]
//   MODE a  — эталон: прежний COL1, по каждому запросу пишет 8-байтный отпечаток в DIGEST (stdout не нужен)
//   MODE b  — «кольцо»: тот же col.c на копии COL1, где примитивы уложены по colring (номера -> места), а страницы примитивов под mprotect.
//             Вне кольца страница дочитывается обработчиком SIGSEGV (синхронная страховка); результат обязан совпасть бит в бит.
//   MODE r  — красный контроль: как b, но обработчик отдаёт мусор вместо данных (ожидаем расхождения)
// Номера примитивов в отпечатке приводятся к прежним (slot -> id). Запросы строятся из исходных массивов, поэтому в a и b одни и те же.
// Выход b: строка  RESULT mism=<n> sync_q=<n> ...  и PASS/FAIL (mism==0). DIGEST — файл, который пишет a и читает b/r.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include "col.h"

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

static const int SZ[4] = {48, 64, 48, 32};
typedef struct { char magic[4]; float ox, oz, cell; uint32_t W, H, ntri, nbox, ncyl, nseg, nref, flags; } Hdr;

// ---- исходный мир
static uint8_t *secA; static size_t secAsz; static Hdr HA;
static const uint8_t *arrA[4]; static const uint32_t *csA, *rfA;
static void parseA(const uint8_t *lvl, size_t lsz) {
  uint32_t off = *(const uint32_t *)(lvl + 92), end = *(const uint32_t *)(lvl + 96);
  if (end <= off || end > lsz) end = (uint32_t)lsz;
  secAsz = end - off; secA = aligned_alloc(16, (secAsz + 15) & ~15ull); memcpy(secA, lvl + off, secAsz);
  memcpy(&HA, secA, sizeof HA);
  const uint8_t *p = secA + 48; uint32_t n[4] = {HA.ntri, HA.nbox, HA.ncyl, HA.nseg};
  for (int t = 0; t < 4; t++) { arrA[t] = p; p += (size_t)n[t] * SZ[t]; }
  csA = (const uint32_t *)p; p += ((size_t)HA.W * HA.H + 1) * 4; rfA = (const uint32_t *)p;
}

// ---- мир кольца
typedef struct { uint32_t TL, page, TW, TH, n[4], slotOff[4], dataOff[4], tileOff, pagesOff, blobOff; } Rhdr;
static Rhdr RH; static uint8_t *ring; static size_t ringsz;
static uint32_t *slot[4], *inv[4]; static size_t primBaseOff[4];   // смещение массива типа от начала primB
static uint8_t *regionB, *secB; static size_t secBsz;
static uint8_t *primB, *shadow; static size_t primLen, osPage; static size_t npg;
static uint8_t *resident, *tileOn; static size_t nresident, peakResident;
static int tilePagesN; static int **tilePages; static int *tilePagesCnt; static size_t *tileBlobBytes;
static volatile uint64_t syncPages, curQuery, syncQuery = (uint64_t)-1, syncQueries; static int mode_red;
static double curDist; static int dbgSync, dbgN, curKind; static uint64_t syncByBucket[6];

static void pageLoad(size_t pg, int garbage) {
  uint8_t *a = primB + pg * osPage;
  mprotect(a, osPage, PROT_READ | PROT_WRITE);
  if (garbage) memset(a, 0xA5, osPage); else memcpy(a, shadow + pg * osPage, osPage);
  if (!resident[pg]) { resident[pg] = 1; nresident++; if (nresident > peakResident) peakResident = nresident; }
}
static void pageEvict(size_t pg) {
  uint8_t *a = primB + pg * osPage; memset(a, 0xEE, osPage);
  mprotect(a, osPage, PROT_NONE); if (resident[pg]) { resident[pg] = 0; nresident--; }
}
static void segv(int sig, siginfo_t *si, void *u) {
  uint8_t *a = (uint8_t *)si->si_addr;
  if (a >= primB && a < primB + npg * osPage) {
    size_t pg = (size_t)(a - primB) / osPage;
    if (dbgSync && curDist < 250 && dbgN < dbgSync) { dbgN++; size_t off = (size_t)(a - primB); int t = 3; while (t > 0 && off < primBaseOff[t]) t--; fprintf(stderr, "SYNC q=%llu dist=%.0f kind=%d тип=%d id=%u\n", (unsigned long long)curQuery, curDist, curKind, t, (unsigned)inv[t][(off - primBaseOff[t]) / SZ[t]]); }
    pageLoad(pg, mode_red); syncPages++;
    if (syncQuery != curQuery) { syncQuery = curQuery; syncQueries++; int b = curDist < 150 ? 0 : curDist < 200 ? 1 : curDist < 250 ? 2 : curDist < 300 ? 3 : curDist < 500 ? 4 : 5; syncByBucket[b]++; }
    return;
  }
  signal(sig, SIG_DFL);
}

static void ringParse(void) {
  const uint8_t *g = ring; const uint32_t *u;
  const uint32_t *h = (const uint32_t *)g;
  // HFMT: 4s I 3f 2I 5I 4I I 4I 4I 4I : индексы u32: 0 magic,1 ver,2-4 ox oz cell,5 W,6 H,7-10 n,11 nref,12 TL,13 page,14 TW,15 TH,16 crc,17-20 slot,21-24 data,25 tile,26 pages,27 blob,28 total
  RH.TL = h[12]; RH.page = h[13]; RH.TW = h[14]; RH.TH = h[15];
  for (int t = 0; t < 4; t++) { RH.n[t] = h[7 + t]; RH.slotOff[t] = h[17 + t]; RH.dataOff[t] = h[21 + t]; }
  RH.tileOff = h[25]; RH.pagesOff = h[26]; RH.blobOff = h[27]; (void)u;
}

static void buildB(void) {
  osPage = (size_t)sysconf(_SC_PAGESIZE);
  uint32_t W = HA.W, H = HA.H; size_t cells = (size_t)W * H;
  size_t prim = 0; for (int t = 0; t < 4; t++) { primBaseOff[t] = prim; prim += (size_t)RH.n[t] * SZ[t]; }
  primLen = prim; size_t tail = (cells + 1) * 4 + (size_t)HA.nref * 4;
  secBsz = 48 + prim + tail;
  size_t total = osPage * 2 + secBsz + osPage * 2;
  regionB = mmap(0, total, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  secB = regionB + osPage - 48;   // массив треугольников начинается ровно на границе страницы
  primB = secB + 48;
  Hdr hb = HA; hb.flags = 0; memcpy(secB, &hb, sizeof hb);
  for (int t = 0; t < 4; t++) {   // данные по местам: slot-порядок хранится в файле как есть
    memcpy(primB + primBaseOff[t], ring + RH.dataOff[t], (size_t)RH.n[t] * SZ[t]);
    slot[t] = (uint32_t *)(ring + RH.slotOff[t]);
    inv[t] = malloc(((size_t)RH.n[t] + 1) * 4);
    for (uint32_t i = 0; i < RH.n[t]; i++) inv[t][slot[t][i]] = i;
  }
  // cellStart и refs из блобов тайлов; номера ссылок -> slot, порядок прежний
  uint32_t *cs = (uint32_t *)(primB + prim), *rf_ = cs + cells + 1; size_t pos = 0;
  const uint32_t *tt = (const uint32_t *)(ring + RH.tileOff);
  uint32_t *cnt = calloc(cells, 4);
  uint32_t TL = RH.TL;
  // двухпроходно: сначала размеры клеток, потом заполнение в порядке клеток по строкам
  for (uint32_t ty = 0; ty < RH.TH; ty++) for (uint32_t tx = 0; tx < RH.TW; tx++) {
    const uint32_t *e = tt + 4 * (ty * RH.TW + tx); const uint32_t *loc = (const uint32_t *)(ring + RH.blobOff + e[0]);
    for (uint32_t lz = 0; lz < TL; lz++) for (uint32_t lx = 0; lx < TL; lx++) { uint32_t gx = tx * TL + lx, gz = ty * TL + lz; if (gx < W && gz < H) cnt[(size_t)gz * W + gx] = loc[lz * TL + lx + 1] - loc[lz * TL + lx]; }
  }
  cs[0] = 0; for (size_t c = 0; c < cells; c++) cs[c + 1] = cs[c] + cnt[c];
  if (cs[cells] != HA.nref) { fprintf(stderr, "refs: %u != %u\n", cs[cells], HA.nref); exit(3); }
  for (uint32_t ty = 0; ty < RH.TH; ty++) for (uint32_t tx = 0; tx < RH.TW; tx++) {
    const uint32_t *e = tt + 4 * (ty * RH.TW + tx); const uint32_t *loc = (const uint32_t *)(ring + RH.blobOff + e[0]);
    const uint32_t *r = loc + TL * TL + 1;
    for (uint32_t lz = 0; lz < TL; lz++) for (uint32_t lx = 0; lx < TL; lx++) { uint32_t gx = tx * TL + lx, gz = ty * TL + lz; if (gx >= W || gz >= H) continue;
      size_t c = (size_t)gz * W + gx; for (uint32_t k = loc[lz * TL + lx]; k < loc[lz * TL + lx + 1]; k++) { uint32_t v = r[k], t = v >> 29, id = v & 0x1fffffffu; rf_[cs[c] + (k - loc[lz * TL + lx])] = (t << 29) | slot[t][id]; } } }
  (void)pos; free(cnt);
  // защищаемые страницы: только целые страницы внутри массивов примитивов
  npg = primLen / osPage; shadow = malloc(npg * osPage);   // копия «карты памяти» снимается ПОСЛЕ colInit: colInit правит данные на месте (латка кустов BUSH_SOFT, col.c ~567)
  resident = calloc(npg, 1); tileOn = calloc(RH.TW * RH.TH, 1);
  for (size_t p = 0; p < npg; p++) { resident[p] = 1; } nresident = npg;
  // страницы, нужные каждому тайлу: по ссылкам и slot-адресам
  tilePages = calloc(RH.TW * RH.TH, sizeof(int *)); tilePagesCnt = calloc(RH.TW * RH.TH, sizeof(int)); tileBlobBytes = calloc(RH.TW * RH.TH, sizeof(size_t));
  uint8_t *mark = calloc(npg + 1, 1);
  for (uint32_t ti = 0; ti < RH.TW * RH.TH; ti++) {
    const uint32_t *e = tt + 4 * ti; const uint32_t *loc = (const uint32_t *)(ring + RH.blobOff + e[0]); const uint32_t *r = loc + TL * TL + 1;
    int *lst = malloc(sizeof(int) * (npg + 1)); int nl = 0;
    for (uint32_t k = 0; k < e[1]; k++) { uint32_t t = r[k] >> 29, id = r[k] & 0x1fffffffu; size_t a = primBaseOff[t] + (size_t)slot[t][id] * SZ[t], b = a + SZ[t] - 1;
      for (size_t pg = a / osPage; pg <= b / osPage && pg < npg; pg++) if (!mark[pg]) { mark[pg] = 1; lst[nl++] = (int)pg; } }
    for (int i = 0; i < nl; i++) mark[lst[i]] = 0;
    tilePages[ti] = lst; tilePagesCnt[ti] = nl; tileBlobBytes[ti] = (size_t)e[1] * 4 + (TL * TL + 1) * 4;
  }
  free(mark);
}
static size_t ringBlobBytes, ringPeak; static int pinSmall = 1;
static void ringMove(float fx, float fz, float R) {
  uint8_t *want = calloc(npg, 1); ringBlobBytes = 0;
  float cell = HA.cell;
  for (uint32_t ty = 0; ty < RH.TH; ty++) for (uint32_t tx = 0; tx < RH.TW; tx++) {
    float x0 = HA.ox + tx * RH.TL * cell, x1 = x0 + RH.TL * cell, z0 = HA.oz + ty * RH.TL * cell, z1 = z0 + RH.TL * cell;
    float dx = fmaxf(fmaxf(x0 - fx, 0.f), fx - x1), dz = fmaxf(fmaxf(z0 - fz, 0.f), fz - z1);
    if (dx * dx + dz * dz > R * R) continue;
    uint32_t ti = ty * RH.TW + tx; ringBlobBytes += tileBlobBytes[ti];
    for (int i = 0; i < tilePagesCnt[ti]; i++) want[tilePages[ti][i]] = 1;
  }
  if (pinSmall) for (size_t p = primBaseOff[1] / osPage; p < npg; p++) want[p] = 1;   // ящики, капсулы, заборы целиком резидентны (0,2 МБ): цепочки заборов и живые объёмы ANM1 читаются вне клеток
  for (size_t p = 0; p < npg; p++) { if (want[p] && !resident[p]) pageLoad(p, 0); else if (!want[p] && resident[p]) pageEvict(p); }
  free(want); if (nresident > ringPeak) ringPeak = nresident;
}

// ---- запросы
static float *tc;   // центры плоских треугольников: x y z (исходный мир)
static uint32_t ntc; static uint32_t *tcTile, *tileStart, *tileList; static uint32_t tilesN, TWq, THq;
static void buildCenters(void) {
  uint32_t nt = HA.ntri; tc = malloc(sizeof(float) * 3 * nt); ntc = 0;
  for (uint32_t i = 0; i < nt; i++) { const float *t = (const float *)(arrA[0] + (size_t)i * 48); if (t[10] < 0.5f) continue;
    tc[3 * ntc] = (t[0] + t[3] + t[6]) / 3; tc[3 * ntc + 1] = (t[1] + t[4] + t[7]) / 3; tc[3 * ntc + 2] = (t[2] + t[5] + t[8]) / 3; ntc++; }
  TWq = (HA.W + 15) / 16; THq = (HA.H + 15) / 16; tilesN = TWq * THq;
  uint32_t *cntq = calloc(tilesN + 1, 4); tcTile = malloc(4 * ntc);
  for (uint32_t i = 0; i < ntc; i++) { int gx = (int)((tc[3 * i] - HA.ox) / HA.cell) / 16, gz = (int)((tc[3 * i + 2] - HA.oz) / HA.cell) / 16; if (gx < 0) gx = 0; if (gz < 0) gz = 0; if (gx >= (int)TWq) gx = TWq - 1; if (gz >= (int)THq) gz = THq - 1; tcTile[i] = gz * TWq + gx; cntq[tcTile[i]]++; }
  tileStart = malloc(4 * (tilesN + 1)); tileStart[0] = 0; for (uint32_t i = 0; i < tilesN; i++) tileStart[i + 1] = tileStart[i] + cntq[i];
  tileList = malloc(4 * ntc); memset(cntq, 0, 4 * tilesN);
  for (uint32_t i = 0; i < ntc; i++) tileList[tileStart[tcTile[i]] + cntq[tcTile[i]]++] = i;
  free(cntq);
}
static int pickNear(float fx, float fz, float d, float *p) {   // треугольник в пределах d от фокуса
  for (int tries = 0; tries < 40; tries++) {
    float x = fx + rr(-d, d), z = fz + rr(-d, d); if ((x - fx) * (x - fx) + (z - fz) * (z - fz) > d * d) continue;
    int gx = (int)((x - HA.ox) / HA.cell) / 16, gz = (int)((z - HA.oz) / HA.cell) / 16; if (gx < 0 || gz < 0 || gx >= (int)TWq || gz >= (int)THq) continue;
    uint32_t ti = gz * TWq + gx, n = tileStart[ti + 1] - tileStart[ti]; if (!n) continue;
    uint32_t i = tileList[tileStart[ti] + rnd() % n]; p[0] = tc[3 * i]; p[1] = tc[3 * i + 1]; p[2] = tc[3 * i + 2]; return 1;
  }
  return 0;
}
static void pickAny(float *p) { uint32_t i = rnd() % ntc; p[0] = tc[3 * i]; p[1] = tc[3 * i + 1]; p[2] = tc[3 * i + 2]; }

static long traceQ = -1; static int ringMode;   // 0 — эталон
static uint32_t mapIdx(uint32_t type, uint32_t idx) { if (!ringMode || type > 3 || idx >= RH.n[type]) return idx; return inv[type][idx]; }
static uint32_t mapRef(uint32_t ref) { if (!ringMode) return ref; uint32_t t = ref >> 29; if (t > 3) return ref; return (t << 29) | mapIdx(t, ref & 0x1fffffffu); }
static uint64_t dig_carbox(uint64_t h, const CarBox *b) {
  h = fnv(h, &b->x, 4); h = fnv(h, &b->z, 4); h = fnv(h, &b->y, 4); h = fnv(h, &b->n, 4);
  int n = b->n > COL_MAX_CONTACTS ? COL_MAX_CONTACTS : b->n;
  for (int i = 0; i < n; i++) { h = fnv(h, &b->nx[i], 4); h = fnv(h, &b->nz[i], 4); h = fnv(h, &b->depth[i], 4); h = fnv(h, &b->type[i], 1);
    uint32_t ix = b->index[i]; if (b->type[i] <= 3 && ix != COL_XFENCE) ix = mapIdx(b->type[i], ix); h = fnv(h, &ix, 4); }
  h = fnv(h, &b->maxDepth, 4); h = fnv(h, &b->behindFence, 4); h = fnv(h, &b->nshapes, 4);
  return h;
}

int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "ring_host a|b|r LEVEL.shr FILE.colring|- N SEED [ringR] [ensure] DIGEST\n"); return 2; }
  const char mode = argv[1][0]; const char *lvlp = argv[2], *ringp = argv[3]; long N = atol(argv[4]); rs ^= (uint64_t)atoll(argv[5]) * 0x9E3779B97F4A7C15ull; if (!rs) rs = 1;
  float R = argc > 6 ? (float)atof(argv[6]) : 250.f; int ensure = argc > 7 ? atoi(argv[7]) : 0; const char *digp = argc > 8 ? argv[8] : "/tmp/ringdig.bin";
  size_t lsz; uint8_t *lvl = file_read(lvlp, &lsz); parseA(lvl, lsz); free(lvl);
  dbgSync = getenv("RING_DEBUG") ? atoi(getenv("RING_DEBUG")) : 0;
  if (getenv("RING_NOPIN")) pinSmall = 0;
  if (getenv("RING_TRACE")) traceQ = atol(getenv("RING_TRACE"));
  ringMode = mode != 'a'; mode_red = mode == 'r';
  ColWorld w; int rc;
  if (!ringMode) rc = colInit(&w, secA, secAsz);
  else {
    ring = file_read(ringp, &ringsz); ringParse(); buildB(); rc = colInit(&w, secB, secBsz); memcpy(shadow, primB, npg * osPage);
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = segv; sa.sa_flags = SA_SIGINFO; sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0);
  }
  if (rc) { fprintf(stderr, "colInit %d\n", rc); return 3; }
  buildCenters();
  FILE *df = fopen(digp, mode == 'a' ? "wb" : "rb"); if (!df) { perror(digp); return 2; }
  uint64_t *dref = 0; if (mode != 'a') { dref = malloc(8 * (size_t)N); if (fread(dref, 8, N, df) != (size_t)N) { fprintf(stderr, "мало отпечатков\n"); return 2; } }
  float fx = 0, fz = 0, fp[3]; pickAny(fp); fx = fp[0]; fz = fp[2];
  if (ringMode) { for (size_t p = 0; p < npg; p++) pageEvict(p); peakResident = 0; nresident = 0; ringMove(fx, fz, R); }
  long mism = 0, firstBad = -1, nNear = 0, nFar = 0; uint64_t kindN[5] = {0}; size_t maxRingBlob = 0;
  for (long q = 0; q < N; q++) {
    if (q % 256 == 0) {   // «кадр»: фокус едет к ближайшему из нескольких кандидатов, иногда телепорт (миссия)
      if (q % 8192 == 0) { pickAny(fp); fx = fp[0]; fz = fp[2]; if (ringMode && ensure) ringMove(fx, fz, R); }
      else { float p2[3]; if (pickNear(fx, fz, 22.f, p2)) { fx = p2[0]; fz = p2[2]; } }
      if (ringMode && !(q % 8192 == 0 && q && !ensure)) { ringMove(fx, fz, R); if (ringBlobBytes > maxRingBlob) maxRingBlob = ringBlobBytes; }
    }
    curQuery = q; float p[3]; int far = (rnd() % 100) < 12; float dist;
    if (far) { pickAny(p); nFar++; } else { if (!pickNear(fx, fz, 150.f, p)) pickAny(p); nNear++; }
    dist = sqrtf((p[0] - fx) * (p[0] - fx) + (p[2] - fz) * (p[2] - fz)); curDist = dist;
    uint64_t h = 1469598103934665603ull; int kind = rnd() % 100 < 40 ? 0 : rnd() % 100 < 55 ? 1 : rnd() % 100 < 60 ? 2 : 3; kindN[kind]++; curKind = kind;
    if (kind == 0) {
      float y, n3[3] = {0}; int mat = 0; uint32_t ref = 0; float yt = p[1] + (rnd() & 1 ? 2.f : 40.f), yb = p[1] - (rnd() & 1 ? 30.f : 200.f);
      int r = colGroundMatId(&w, p[0] + rr(-3, 3), p[2] + rr(-3, 3), yt, yb, &y, n3, &mat, &ref);
      if (q == traceQ) fprintf(stderr, "T ground r=%d y=%.5f n=%.4f,%.4f,%.4f mat=%d ref=%08x raw=%08x pad=%g\n", r, y, n3[0], n3[1], n3[2], mat, mapRef(ref), ref, (ref>>29)==1 ? w.box[ref&0x1fffffff].pad : -1.f);
      h = fnv(h, &r, 4); if (r) { h = fnv(h, &y, 4); h = fnv(h, n3, 12); h = fnv(h, &mat, 4); uint32_t mr = mapRef(ref); h = fnv(h, &mr, 4); }
    } else if (kind == 1) {
      float a[3] = {p[0], p[1] + 1.f, p[2]}, b[3]; float L = rr(2, 45), th = rr(0, 6.2831853f); b[0] = a[0] + cosf(th) * L; b[2] = a[2] + sinf(th) * L; b[1] = a[1] + rr(-4, 6);
      float f = colRayFrac(&w, a, b, rnd() & 1); if (q == traceQ) fprintf(stderr, "T ray f=%.6f\n", f); h = fnv(h, &f, 4);
    } else {
      CarBox b; memset(&b, 0, sizeof b); float th = rr(0, 6.2831853f); b.x = p[0]; b.z = p[2]; b.fx = cosf(th); b.fz = sinf(th); b.hw = 1.0f; b.hl = 2.3f; b.y = p[1]; b.lo = .35f; b.hi = 1.6f; b.fsupOn = rnd() & 1; b.fsup = p[1];
      if (kind == 2) { int r = colProbeCar(&w, &b); h = fnv(h, &r, 4); }
      else { float mx = 0, mz = 0, nx = 0, nz = 0; int r = colPushCar(&w, &b, &mx, &mz, &nx, &nz); h = fnv(h, &r, 4); h = fnv(h, &mx, 4); h = fnv(h, &mz, 4); h = fnv(h, &nx, 4); h = fnv(h, &nz, 4); }
      if (q == traceQ) { fprintf(stderr, "T car kind=%d x=%.5f z=%.5f n=%d maxD=%.5f beh=%d nsh=%d\n", kind, b.x, b.z, b.n, b.maxDepth, b.behindFence, b.nshapes); for (int i = 0; i < b.n && i < COL_MAX_CONTACTS; i++) fprintf(stderr, "   c%d type=%d idx=%u depth=%.5f n=%.4f,%.4f\n", i, b.type[i], b.type[i] <= 3 && b.index[i] != COL_XFENCE ? mapIdx(b.type[i], b.index[i]) : b.index[i], b.depth[i], b.nx[i], b.nz[i]); }
      h = dig_carbox(h, &b);
    }
    if (mode == 'a') fwrite(&h, 8, 1, df);
    else if (dref[q] != h) { mism++; if (firstBad < 0) firstBad = q; }
  }
  if (mode == 'a') { fclose(df); printf("RESULT эталон: %ld запросов, ближних %ld, дальних %ld, виды ground/ray/probe/push = %llu/%llu/%llu/%llu\n", N, nNear, nFar, (unsigned long long)kindN[0], (unsigned long long)kindN[1], (unsigned long long)kindN[2], (unsigned long long)kindN[3]); return 0; }
  double resMB = peakResident * (double)osPage / 1e6, blobMB = maxRingBlob / 1e6, csMB = ((double)HA.W * HA.H + 1) * 4 / 1e6, slotMB = ((double)RH.n[0] + RH.n[1] + RH.n[2] + RH.n[3]) * 4 / 1e6;
  printf("RESULT %s R=%.0f ensure=%d N=%ld mism=%ld first=%ld sync_q=%llu sync_pages=%llu | sync по дальности от фокуса <150/<200/<250/<300/<500/дальше: %llu/%llu/%llu/%llu/%llu/%llu\n",
         mode == 'r' ? "КРАСНЫЙ" : "кольцо", R, ensure, N, mism, firstBad, (unsigned long long)syncQueries, (unsigned long long)syncPages,
         (unsigned long long)syncByBucket[0], (unsigned long long)syncByBucket[1], (unsigned long long)syncByBucket[2], (unsigned long long)syncByBucket[3], (unsigned long long)syncByBucket[4], (unsigned long long)syncByBucket[5]);
  printf("MEM кольцо без догрузок: примитивы %.2f МБ; пик с догрузками между кадрами %.2f МБ\n", ringPeak * (double)osPage / 1e6, resMB);
  printf("MEM итого (примитивы кольца) %.2f МБ + блобы тайлов кольца %.2f + cellStart целиком %.2f + таблица slot %.2f = %.2f МБ (COL1 было %.2f)\n",
         ringPeak * (double)osPage / 1e6, blobMB, csMB, slotMB, ringPeak * (double)osPage / 1e6 + blobMB + csMB + slotMB, secAsz / 1e6);
  if (mode == 'r') { printf(mism > 0 ? "RED ok (расхождения есть)\n" : "RED FAIL (красный не покраснел)\n"); return mism > 0 ? 0 : 1; }
  printf(mism == 0 ? "PASS\n" : "FAIL\n"); return mism == 0 ? 0 : 1;
}
