#include "dcache96.h"
// B1 (план оптимизации, 10.10.2026): запекание неподвижных пропов (PRP1) и стейтпропов (SPR1) в дисплей-листы GE по клеткам 48 м. Замер 9D: цикл пропов и
// стейтпропов пишет в список ≈30–40 слов на кусок (привязка текстуры, флаги, матрица, DrawArray: sp_loop ≈17,5 мкс на видимый кусок, props 1,1–1,8 мс), а 94 % кусков
// стейтпропов L1 (73 % L2, 98 % L7) стоят в покое. Запечённый лист клетки — один CALL: команды те же, что писал бы старый путь (bindTexLean, setFlags, матрица куска, drawGeo),
// порядок внутри клетки по (флаги, текстура). Что запекается: экземпляр в покое (стейтпроп: kind 0, состояние с неподвижной позой; проп: спит, не спрятан), все видимые
// куски — непрозрачные (без бита 2 и аддитивных) и годные (pieceOk). Остальное рисует прежний путь (динамический список экземпляров, каждый кадр). Изменение экземпляра ловит
// сравнение подписи (состояние, живость, положение, поза/ориентация) со снятой при запекании — сканом всех экземпляров раз в кадр (≈0,15 мс на 700), без перехватов в
// симуляции; несовпавший экземпляр рисуется прежним путём В ТОТ ЖЕ КАДР, лист клетки пересобирается в этом же кадре (до PB_BUILD_MAX за кадр, иначе клетка в этом кадре
// рисуется прежним путём целиком), обратно в лист экземпляр попадает после PB_STILL спокойных кадров. Клетка, у глаза которой лежит малый экземпляр (правило camNearSphere,
// №502), рисуется прежним путём в этом кадре. -DPROP_BAKE_OLD=1 — как до B1
#include "bisect.h"
#include "propbake_core.h"
#ifndef PROP_BAKE_OLD
#define PROP_BAKE_OLD BIS(0)
#endif
#ifndef PROP_FAST_OLD
#define PROP_FAST_OLD 0
#endif
#ifndef PROP_WIN_OLD
#define PROP_WIN_OLD 0
#endif
// №915 (оптимизация предметов, «пропы 5–7 мс PSP»: на железе PBPROF показывает ≈0,8–1,3 мс за кадр на машинерию листов — скан экземпляров 0,26–0,53 (+0,2 стейтпропы), обход окна клеток
// 0,06–0,19, сборка динамического списка 0,06–0,15; вызовы самих листов — 15–19 за кадр, ≈40 слов, почти бесплатны): каждый кадр три обхода «всех» — экземпляров (скан подписей, из них
// проверяется 30–50), клеток окна (200+ клеток по 56 Б, почти все пустые) и снова экземпляров (динамический список). Теперь: (1) скан — по списку ближних экземпляров (в радиусе PB_NEAR + 12 м
// от глаза на момент построения, пересборка раз в 8 кадров или после сдвига глаза на 12 м) плюс срез индексов (i + кадр) & 7 == 0 для дальних — те же экземпляры проверяются в те же кадры,
// что в прежнем скане (дальние раз в 8, дальше 150 м раз в 16 кадров); (2) клетки — список активных (есть лист или грязная), пустые клетки не обходятся; видимые клетки кадра — отдельным
// списком по возрастанию номера (порядок вызовов листов прежний); (3) динамический список — базовый (не запечённые и с прозрачными кусками; пересчёт при смене запечённости) плюс члены клеток,
// рисуемых прежним путём в этом кадре, по возрастанию номера (порядок рисования прежний). Экземпляр, сдвинувшийся вдали, замечается не позже чем через 8–16 кадров, как и раньше.
// PB_FAST_OLD=1 / «flag pbfast 0|1» / файл pbfastold рядом с EBOOT — прежние обходы
#ifndef PB_FAST_OLD
#define PB_FAST_OLD 0
#endif
static int pbFastOn = !PB_FAST_OLD;
#define PB_NEAR_MARGIN 12.f
#define PB_NEAR 60.f
#define PB_FAR2 (150.f * 150.f)
#define PB_STILL 12
#define PB_BUILD_MAX 3
#define PB_CELL 48.f
#define PB_CAP (2u << 20)
typedef struct { float cx, cy, cz, rad; unsigned *buf; unsigned bytes, nmem; int tris, draws; unsigned short nb; PbMem *mem; unsigned char dirty, vis, force, hasBig; } PbCell;
typedef struct {
  int kind; unsigned n, nc, nx, nz; float x0, z0;
  PbCell *cell; unsigned short *baked; PbSig *sig; unsigned char *still, *inel, *hasTr; unsigned *dyn, ndyn; int on;
  unsigned *act; unsigned nact; unsigned char *inAct;   // №915: активные клетки (лист или грязная)
  unsigned *nearL; unsigned nearN, nearFr; unsigned char *nearMark; float nearE[3]; int nearOk;   // №915: ближние экземпляры
  unsigned *dynBase; unsigned ndynBase; int dynBaseDirty; unsigned *visL; unsigned nvis;   // №915: базовый динамический список; видимые клетки кадра
  float *px, *pz; float maxRad; unsigned wx0, wx1, wz0, wz1, fr, scanned, skipped;   // PROP_FAST: положение при последней проверке подписи; кадр набора; сколько проверено / пропущено (вдали) за кадр
} PbSet;
static void *pbGarbage[48]; static unsigned pbGarbageN;   // старые листы клеток освобождаются только в начале следующего кадра (после sceGuSync)
static unsigned pbChkFrames, pbChkDyn, pbChkVis, pbChkForce; static int pbChkOn;   // №915: сверка списков (смоук, строка PBCHK)
static PbSet pbP, pbS; static unsigned pbBigForce; static unsigned pbBytes, pbBuilt, pbCalls, pbFallback; static int pbTried;
static void pbFreeSet(PbSet *s) {
  for (unsigned c = 0; c < s->nc; c++) { if (s->cell[c].buf) { pbBytes -= s->cell[c].bytes; free(s->cell[c].buf); } free(s->cell[c].mem); }
  free(s->act); free(s->inAct); free(s->nearL); free(s->nearMark); free(s->dynBase); free(s->visL); free(s->px); free(s->pz); free(s->cell); free(s->baked); free(s->sig); free(s->still); free(s->inel); free(s->hasTr); free(s->dyn); memset(s, 0, sizeof *s);
}
static void pbGarbageFlush(void);
static void pbReset(void) { pbFreeSet(&pbP); pbFreeSet(&pbS); pbGarbageFlush(); pbBytes = 0; pbTried = 0; memset(bigTab, 0, sizeof bigTab); }   // bigTab — кэш границ кусков (№640): после смены уровня адреса вершин другие
static void pbSigOf(const PbSet *s, unsigned i, PbSig *g) { if (s->kind == 0) pbSigProp(&props.st[i], g); else pbSigSp(&sprops.run[i], g); }
static void pbPos(const PbSet *s, unsigned i, float *x, float *z) { if (s->kind == 0) { *x = props.st[i].x; *z = props.st[i].z; } else { *x = sprops.run[i].x; *z = sprops.run[i].z; } }
static unsigned pbCellOf(const PbSet *s, float x, float z) {
  int cx = (int)floorf((x - s->x0) / PB_CELL), cz = (int)floorf((z - s->z0) / PB_CELL);
  if (cx < 0) cx = 0; if (cz < 0) cz = 0; if (cx >= (int)s->nx) cx = (int)s->nx - 1; if (cz >= (int)s->nz) cz = (int)s->nz - 1;
  return (unsigned)cx + (unsigned)cz * s->nx;
}
static inline void pbActAdd(PbSet *s, unsigned c) { if (!s->inAct[c]) { s->inAct[c] = 1; s->act[s->nact++] = c; } }
static int pbCmpU(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
static int pbInit(PbSet *s, int kind) {
  unsigned n = kind == 0 ? (haveProps ? props.ninst : 0) : (haveSP ? sprops.ninst : 0);
  memset(s, 0, sizeof *s); s->kind = kind; s->n = n; if (!n) return 0;
  float x0 = 1e30f, x1 = -1e30f, z0 = 1e30f, z1 = -1e30f;
  for (unsigned i = 0; i < n; i++) { float x, z; pbPos(s, i, &x, &z); if (x < x0) x0 = x; if (x > x1) x1 = x; if (z < z0) z0 = z; if (z > z1) z1 = z; }
  float G = PB_CELL; s->x0 = x0 - 200.f; s->z0 = z0 - 200.f;   // запас: упавший за край проп не выходит из сетки (индекс прижимается к краю)
  unsigned nx = (unsigned)((x1 - x0 + 400.f) / G) + 1, nz = (unsigned)((z1 - z0 + 400.f) / G) + 1; if ((unsigned long long)nx * nz > 16384ull) return 0;
  s->nx = nx; s->nz = nz; s->nc = nx * nz;
  s->cell = (PbCell *)calloc(s->nc, sizeof(PbCell)); s->baked = (unsigned short *)calloc(n, sizeof(unsigned short)); s->sig = (PbSig *)calloc(n, sizeof(PbSig));
  s->still = (unsigned char *)calloc(n, 1); s->inel = (unsigned char *)calloc(n, 1); s->hasTr = (unsigned char *)calloc(n, 1); s->dyn = (unsigned *)malloc(n * sizeof(unsigned));
  s->px = (float *)calloc(n, sizeof(float)); s->pz = (float *)calloc(n, sizeof(float));
  s->act = (unsigned *)malloc(s->nc * sizeof(unsigned)); s->inAct = (unsigned char *)calloc(s->nc, 1); s->nearL = (unsigned *)malloc(n * sizeof(unsigned)); s->nearMark = (unsigned char *)calloc(n, 1);
  s->dynBase = (unsigned *)malloc(n * sizeof(unsigned)); s->visL = (unsigned *)malloc(s->nc * sizeof(unsigned)); s->dynBaseDirty = 1;
  if (!s->cell || !s->baked || !s->sig || !s->still || !s->inel || !s->hasTr || !s->dyn || !s->px || !s->pz || !s->act || !s->inAct || !s->nearL || !s->nearMark || !s->dynBase || !s->visL) { pbFreeSet(s); return 0; }
  for (unsigned i = 0; i < n; i++) { pbSigOf(s, i, &s->sig[i]); s->px[i] = s->sig[i].v[0]; s->pz[i] = s->sig[i].v[2]; }
  s->on = 1; return 1;
}
// №639 (аудит): запечённая клетка рисовалась листом, и clip-guard (объект ближе 0,3 м к глазу не рисуется, по взгляду ближе 1,8 м) на неё не действовал: клетка с экземпляром у глаза идёт прежним путём,
// где правило есть (clipGuardProp / clipGuardSphere). CAM_CLIP_GUARD_OLD=1 — прежнее
static inline int pbClipNear(float ex, float ey, float ez, const PbMem *m) { float dx = m->c[0] - ex, dy = m->c[1] - ey, dz = m->c[2] - ez, R = m->r + 2.4f; return dx * dx + dy * dy + dz * dz < R * R; }
static int pbOkPiece0(const PbPiece *o);
static PbMem *pbBigMem;   // экземпляр, чьи куски сейчас собирает pbCollect (pbOkPiece дописывает в него крупные)
static int pbOkPiece(const PbPiece *o) {
  if (pbBigMem && pbBigMem->nbig < 1) { float c[3], sz = bigSize(o->v, o->ni, o->flags, o->m, c); if (sz > 0.f) { float *b = pbBigMem->big[pbBigMem->nbig++]; b[0] = c[0]; b[1] = c[1]; b[2] = c[2]; b[3] = sz; bigWorldBox(o->v, o->ni, o->flags, o->m, pbBigMem->bb[pbBigMem->nbig - 1]); } }
  return pbOkPiece0(o); }
static int pbOkPiece0(const PbPiece *o) { Piece p; memset(&p, 0, sizeof p); p.r.tex = o->tex; p.r.flags = o->flags; p.r.ni = o->ni; p.v = (Vert *)o->v; p.idx = (unsigned short *)o->idx; return pieceOk(&p, 0); }
static int pbCollect(const PbSet *s, unsigned i, PbPiece *out, int max, PbMem *mm) { pbBigMem = mm; int r = s->kind == 0 ? pbCollectProp(&props, i, out, max, mm, pbOkPiece) : pbCollectSp(&sprops, i, out, max, mm, pbOkPiece); pbBigMem = 0; return r; }
static int pbCmpPiece(const void *a, const void *b) {
  const PbPiece *x = a, *y = b; if (x->flags != y->flags) return x->flags < y->flags ? -1 : 1;
  return x->tex < y->tex ? -1 : x->tex > y->tex ? 1 : 0;
}
#define PB_MAXP 1536
static PbPiece pbBuf[PB_MAXP]; static PbMem pbMemBuf[1024];
// пересборка листа клетки c: члены — экземпляры, чьё текущее положение в клетке c, годные и спокойные (или уже запечённые тут); -> 1 собрано, 0 нечего, -1 не вышло
static int pbRebuild(PbSet *s, unsigned c) {
  PbCell *C = &s->cell[c]; unsigned np = 0, nm = 0;
  unsigned short tag = (unsigned short)(c + 1); s->dynBaseDirty = 1;
  for (unsigned i = 0; i < s->n && nm < 1024; i++) {
    float x, z; pbPos(s, i, &x, &z); if (pbCellOf(s, x, z) != c) { if (s->baked[i] == tag) s->baked[i] = 0; continue; }
    PbSig g; pbSigOf(s, i, &g);
    int was = s->baked[i] == tag && !memcmp(&g, &s->sig[i], sizeof g);
    if (!was && (s->still[i] < PB_STILL || s->inel[i])) { if (s->baked[i] == tag) s->baked[i] = 0; continue; }
    PbMem mm; int k = pbCollect(s, i, pbBuf + np, PB_MAXP - (int)np, &mm);
    if (k < 0) { s->inel[i] = 1; if (s->baked[i] == tag) s->baked[i] = 0; continue; }
    np += (unsigned)k; pbMemBuf[nm++] = mm; s->baked[i] = tag; s->sig[i] = g; s->hasTr[i] = mm.hasTr;
  }
  if (C->buf) { pbBytes -= C->bytes; if (pbGarbageN < 48) pbGarbage[pbGarbageN++] = C->buf; else free(C->buf); C->buf = 0; C->bytes = 0; }
  free(C->mem); C->mem = 0; C->nmem = 0; C->tris = C->draws = 0;
  if (!nm) return 0;
  s->dynBaseDirty = 1; pbActAdd(s, c);
  C->mem = (PbMem *)malloc(nm * sizeof(PbMem)); if (!C->mem) { for (unsigned i = 0; i < s->n; i++) if (s->baked[i] == tag) s->baked[i] = 0; return -1; }
  memcpy(C->mem, pbMemBuf, nm * sizeof(PbMem)); C->nmem = nm; C->hasBig = 0; for (unsigned q = 0; q < nm; q++) if (pbMemBuf[q].nbig) C->hasBig = 1;
  float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  for (unsigned q = 0; q < nm; q++) { const PbMem *m = &pbMemBuf[q]; float rr = s->kind == 0 ? m->r + 2.f * props.types[props.def[m->id].type].halfH + 1.f : m->r + 3.f;
    for (int a = 0; a < 3; a++) { if (m->c[a] - rr < lo[a]) lo[a] = m->c[a] - rr; if (m->c[a] + rr > hi[a]) hi[a] = m->c[a] + rr; } }
  C->cx = (lo[0] + hi[0]) * .5f; C->cy = (lo[1] + hi[1]) * .5f; C->cz = (lo[2] + hi[2]) * .5f;
  { float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2]; C->rad = .5f * sqrtf(dx * dx + dy * dy + dz * dz); if (C->rad > s->maxRad) s->maxRad = C->rad; }
  if (!np) return 1;   // экземпляры без кусков (убраны): лист не нужен
  qsort(pbBuf, np, sizeof(PbPiece), pbCmpPiece);
  unsigned words = 16; for (unsigned q = 0; q < np; q++) words += 96 + 8 * (pbBuf[q].ni / 65532 + 1);   // 72 слова куска города + матрица модели (13) + запас
  unsigned bytes = words * 4;
  if (pbBytes + bytes > PB_CAP) { for (unsigned i = 0; i < s->n; i++) if (s->baked[i] == tag) s->baked[i] = 0; free(C->mem); C->mem = 0; C->nmem = 0; return -1; }
  unsigned *b = (unsigned *)memalign(16, bytes); if (!b) { for (unsigned i = 0; i < s->n; i++) if (s->baked[i] == tag) s->baked[i] = 0; free(C->mem); C->mem = 0; C->nmem = 0; return -1; }
  sceKernelDcacheWritebackInvalidateRange(b, bytes);
  dlLost(); sceGuStart(GU_CALL, b); flatOff(); unsigned nb0_ = binds + flatBinds; flatInList = 1; flatNoSuppress = 1;
  if (!dlNoDepth) sceGuDepthRange(65535, 0);
  int tris = 0, dr = 0;
  for (unsigned q = 0; q < np; q++) { PbPiece *o = &pbBuf[q]; Piece p; memset(&p, 0, sizeof p); p.r.tex = o->tex; p.r.flags = o->flags; p.r.ni = o->ni; p.v = (Vert *)o->v; p.idx = (unsigned short *)o->idx;
    bindTexLean(o->tex); setFlags(o->flags); sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)o->m); drawGeo(&p, 0, o->ni); tris += o->ni / 3; dr++; }
  if (phTfx) { sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); phTfx = 0; }
  flatOff(); flatInList = 0; flatNoSuppress = 0;
  DCALL96(); unsigned sz = (unsigned)sceGuFinish(); dlLost();
  if (sz > bytes) { smkGeBad++; free(b); for (unsigned i = 0; i < s->n; i++) if (s->baked[i] == tag) s->baked[i] = 0; free(C->mem); C->mem = 0; C->nmem = 0; return -1; }
  C->buf = b; C->bytes = bytes; C->tris = tris; C->draws = dr; C->nb = (unsigned short)(binds + flatBinds - nb0_); pbBytes += bytes; pbBuilt++;
  return 1;
}
// №637: три прохода по ВСЕМ клеткам набора (875 клеток × 56 Б, кэш PSP 16 КБ) заменены проходом по окну клеток вокруг глаза: дальность рисования + наибольший радиус клетки + запас 60 м.
// Клетка вне окна не видна (дальность), её экземпляры дальше дальности+12 м не берутся в динамические; грязная пустая клетка вне окна пересобирается, когда окно до неё дойдёт. PROP_WIN_OLD=1 — все клетки
static void pbWindow(PbSet *s, float ex, float ez, float maxD) {
  if (PROP_WIN_OLD) { s->wx0 = 0; s->wx1 = s->nx - 1; s->wz0 = 0; s->wz1 = s->nz - 1; return; }
  float W = maxD + s->maxRad + 60.f; int a = (int)floorf((ex - W - s->x0) / PB_CELL), b = (int)floorf((ex + W - s->x0) / PB_CELL), c = (int)floorf((ez - W - s->z0) / PB_CELL), d = (int)floorf((ez + W - s->z0) / PB_CELL);
  if (a < 0) a = 0; if (c < 0) c = 0; if (b >= (int)s->nx) b = (int)s->nx - 1; if (d >= (int)s->nz) d = (int)s->nz - 1;
  if (b < a) a = b = (a < (int)s->nx ? a : (int)s->nx - 1); if (d < c) c = d = (c < (int)s->nz ? c : (int)s->nz - 1);   // глаз далеко за картой: одна крайняя клетка
  s->wx0 = (unsigned)a; s->wx1 = (unsigned)b; s->wz0 = (unsigned)c; s->wz1 = (unsigned)d;
}
#define PB_WIN_FOR(s, c) for (unsigned wz_ = (s)->wz0; wz_ <= (s)->wz1; wz_++) for (unsigned wx_ = (s)->wx0, c = wx_ + wz_ * (s)->nx; wx_ <= (s)->wx1; wx_++, c++)
// кадр набора: скан подписей, пересборка видимых грязных клеток, список динамических экземпляров. cull — клетка видима (дальность + пирамида)
static void pbScanOne(PbSet *s, unsigned i) {
    s->scanned++;
    PbSig g; pbSigOf(s, i, &g); int same = !memcmp(&g, &s->sig[i], sizeof g); s->px[i] = g.v[0]; s->pz[i] = g.v[2];
    if (s->baked[i]) { if (!same) { { unsigned cb_ = s->baked[i] - 1u; s->cell[cb_].dirty = 1; pbActAdd(s, cb_); s->dynBaseDirty = 1; } s->baked[i] = 0; s->sig[i] = g; s->still[i] = 0; s->inel[i] = 0; } }
    else if (same) { if (s->still[i] < 255) s->still[i]++;
      if (s->still[i] == PB_STILL && !s->inel[i]) { float x, z; pbPos(s, i, &x, &z); { unsigned cc_ = pbCellOf(s, x, z); s->cell[cc_].dirty = 1; pbActAdd(s, cc_); } } }
    else { s->sig[i] = g; s->still[i] = 0; s->inel[i] = 0; }
}

static void pbCellStep(PbSet *s, unsigned c, float ex, float ey, float ez, float maxD, const float e3[3], int *builds) {
    PbCell *C = &s->cell[c]; if (!C->nmem && !C->dirty) return;
    if (C->nmem) { float dx = C->cx - ex, dz = C->cz - ez, R = maxD + C->rad; if (dx * dx + dz * dz > R * R) return;
      float cc[3] = {C->cx, C->cy, C->cz}; if (rfSphereOut((const float (*)[3])pl, e3, cc, C->rad)) return; }
    else if (!C->dirty) return;
    C->vis = 1;
    if (C->dirty) {
      int stale = C->nmem != 0;
      if (stale ? builds[0] < PB_BUILD_MAX : builds[1] < 1) { builds[stale ? 0 : 1]++; unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int rc = pbRebuild(s, c); C->dirty = 0;
        if (smkOn) { char l[120]; snprintf(l, sizeof l, "PBAKE k=%d cell=%u rc=%d mem=%u bytes=%u ms=%.2f t=%.1f\n", s->kind, c, rc, C->nmem, C->bytes, (sceKernelGetSystemTimeLow() - t0) / 1000.0, smkT); smkWrite(l); } }
      else { if (stale) { C->force = 1; pbFallback++; } return; }
    }
    // №640: крупный кусок (10–150 м) у глаза режется ggCut только в динамическом пути (drawPieceM, BIG_CUT_OLD=1 — без среза): пока глаз ближе размера куска к его центру, клетка рисуется прежним путём
    if (C->nmem && C->hasBig && !bigCutOld) for (unsigned q = 0; q < C->nmem && !C->force; q++) { const PbMem *m = &C->mem[q];
      for (unsigned b = 0; b < m->nbig; b++) { float dx = m->big[b][0] - ex, dy = m->big[b][1] - ey, dz = m->big[b][2] - ez; if (dx * dx + dy * dy + dz * dz < m->big[b][3] * m->big[b][3] && ggBoxRiskW(m->bb[b])) { C->force = 1; pbBigForce++; break; } } }
    if (C->nmem) { float dx = C->cx - ex, dy = C->cy - ey, dz = C->cz - ez, R = C->rad + 3.4f;
      if (dx * dx + dy * dy + dz * dz < R * R) for (unsigned q = 0; q < C->nmem; q++) { const PbMem *m = &C->mem[q]; if ((m->hide && camNearSphere(ex, ey, ez, m->c[0], m->c[1], m->c[2], m->r)) || (!CAM_CLIP_GUARD_OLD && (s->kind == 0 || m->r <= 2.5f) && pbClipNear(ex, ey, ez, m))) { C->force = 1; break; } } }
}

static void pbFrameSet(PbSet *s, float ex, float ey, float ez, float maxD, int *builds) {   // builds[0] — пересборки устаревших клеток (до PB_BUILD_MAX), builds[1] — первые запекания (до 1 за кадр: без рывка на въезде)
  if (!s->on) return;
  unsigned pt0_ = smkOn ? sceKernelGetSystemTimeLow() : 0, pt1_ = 0, pt2_ = 0;
  const float e3[3] = {ex, ey, ez};
  pbWindow(s, ex, ez, maxD);
  if (!pbFastOn) { PB_WIN_FOR(s, c) { s->cell[c].vis = 0; s->cell[c].force = 0; } } else s->nvis = 0;
  // 1. скан: изменившиеся запечённые -> динамические, клетка грязная; спокойные динамические -> заявка на запекание в клетку по положению
  // PROP_FAST (аудит FPS п.1, пропы 5,7–7 мс на PSP): подпись каждого экземпляра проверялась КАЖДЫЙ кадр (PropState ~100+ Б по всей карте, кэш PSP 16 КБ), на Маке L1 0,42 мс из 1,5 мс
  // x_props. Теперь экземпляр дальше PB_NEAR (60 м) от глаза по положению прошлой проверки проверяется раз в 8 кадров; ближние (машина игрока, трафик, сбитые) — каждый кадр. Сбитый вдали замечается
  // не позже чем через 8 кадров (0,27 с, вне боевой зоны камеры). PROP_FAST_OLD=1 — каждый кадр, как было
  s->fr++; s->scanned = s->skipped = 0;
  if (!pbFastOn) {
    for (unsigned i = 0; i < s->n; i++) {
      if (!PROP_FAST_OLD) { float dx = s->px[i] - ex, dz = s->pz[i] - ez; float d2 = dx * dx + dz * dz; if (d2 > PB_NEAR * PB_NEAR && ((i + s->fr) & (d2 > PB_FAR2 ? 15u : 7u))) { s->skipped++; continue; } }
      pbScanOne(s, i);
    }
  } else {   // №915: ближние по списку + срез индексов (i + кадр) & 7 == 0; условия проверки те же, что в цикле выше
    float mx_ = ex - s->nearE[0], my_ = ey - s->nearE[1], mz_ = ez - s->nearE[2];
    if (!s->nearOk || s->fr - s->nearFr >= 8u || mx_ * mx_ + my_ * my_ + mz_ * mz_ > PB_NEAR_MARGIN * PB_NEAR_MARGIN) {
      float R2 = (PB_NEAR + PB_NEAR_MARGIN) * (PB_NEAR + PB_NEAR_MARGIN); s->nearN = 0; memset(s->nearMark, 0, s->n);
      for (unsigned i = 0; i < s->n; i++) { float dx = s->px[i] - ex, dz = s->pz[i] - ez; if (dx * dx + dz * dz <= R2) { s->nearL[s->nearN++] = i; s->nearMark[i] = 1; } }
      s->nearE[0] = ex; s->nearE[1] = ey; s->nearE[2] = ez; s->nearFr = s->fr; s->nearOk = 1;
    }
    const float N2 = PB_NEAR * PB_NEAR;
    for (unsigned u = 0; u < s->nearN; u++) { unsigned i = s->nearL[u]; float dx = s->px[i] - ex, dz = s->pz[i] - ez, d2 = dx * dx + dz * dz;
      if (d2 > N2 && ((i + s->fr) & (d2 > PB_FAR2 ? 15u : 7u))) { s->skipped++; continue; } pbScanOne(s, i); }
    for (unsigned i = (8u - (s->fr & 7u)) & 7u; i < s->n; i += 8) { if (s->nearMark[i]) continue; float dx = s->px[i] - ex, dz = s->pz[i] - ez, d2 = dx * dx + dz * dz;
      if (d2 > N2 && ((i + s->fr) & (d2 > PB_FAR2 ? 15u : 7u))) { s->skipped++; continue; } pbScanOne(s, i); }
  }
  if (smkOn) pt1_ = sceKernelGetSystemTimeLow();
  // 2. видимые клетки: пересборка грязных, у глаза малый экземпляр — прежним путём
  if (!pbFastOn) { PB_WIN_FOR(s, c) { pbCellStep(s, c, ex, ey, ez, maxD, e3, builds); } }
  else {   // №915: только активные клетки (лист или грязная); пустые не обходятся
    for (unsigned a = 0; a < s->nact; ) { unsigned c = s->act[a]; PbCell *C = &s->cell[c];
      if (!C->nmem && !C->dirty) { C->vis = 0; C->force = 0; s->inAct[c] = 0; s->act[a] = s->act[--s->nact]; continue; }
      a++; unsigned cx = c % s->nx, cz = c / s->nx; if (cx < s->wx0 || cx > s->wx1 || cz < s->wz0 || cz > s->wz1) continue;
      C->vis = 0; C->force = 0; pbCellStep(s, c, ex, ey, ez, maxD, e3, builds); if (C->vis) s->visL[s->nvis++] = c; }
    if (s->nvis > 1) qsort(s->visL, s->nvis, sizeof(unsigned), pbCmpU);   // вызовы листов — по возрастанию номера клетки, как обход окна
  }
  if (smkOn) pt2_ = sceKernelGetSystemTimeLow();
  // 3. динамические: не запечённые + члены клеток, рисуемых прежним путём в этом кадре
  s->ndyn = 0;
  // PROP_FAST (шаг 2, стейтпропы 201 кандидат / 25 видимых): в список динамических не берём экземпляр, который по положению последней проверки дальше дальности рисования + запас 12 м
  // (за 8 кадров вдали проп уходит не дальше ~8 м): циклы пропов и стейтпропов не тратят на него ни spDrawable, ни сферу кадра. Ближние и сбитые — как прежде. PROP_FAST_OLD=1 — все
  float far2 = (maxD + 12.f) * (maxD + 12.f);
  if (!pbFastOn) {
    for (unsigned i = 0; i < s->n; i++) { unsigned b = s->baked[i];
      if (!b || s->hasTr[i] || s->cell[b - 1].force) {
        if (!PROP_FAST_OLD) { float dx = s->px[i] - ex, dz = s->pz[i] - ez; if (dx * dx + dz * dz > far2) continue; }
        s->dyn[s->ndyn++] = i; } }   // hasTr: прозрачные куски запечённого экземпляра рисует прежний путь
  } else {   // №915: базовый список (не запечённые и с прозрачными кусками; пересчёт при смене запечённости) + члены клеток, рисуемых прежним путём
    if (s->dynBaseDirty) { s->ndynBase = 0; for (unsigned i = 0; i < s->n; i++) if (!s->baked[i] || s->hasTr[i]) s->dynBase[s->ndynBase++] = i; s->dynBaseDirty = 0; }
    for (unsigned u = 0; u < s->ndynBase; u++) { unsigned i = s->dynBase[u]; if (pbFastOn == 2 && (u & 1)) continue; float dx = s->px[i] - ex, dz = s->pz[i] - ez; if (dx * dx + dz * dz > far2) continue; s->dyn[s->ndyn++] = i; }
    unsigned nbase_ = s->ndyn;
    // pbFastOn 2 — отрицательный контроль гейта (база без каждого второго)
    for (unsigned u = 0; u < s->nvis; u++) { unsigned c = s->visL[u]; const PbCell *C = &s->cell[c]; if (!C->force) continue;
      for (unsigned q = 0; q < C->nmem; q++) { unsigned i = C->mem[q].id; if (s->baked[i] != c + 1 || s->hasTr[i]) continue; float dx = s->px[i] - ex, dz = s->pz[i] - ez; if (dx * dx + dz * dz > far2) continue; s->dyn[s->ndyn++] = i; } }
    if (s->ndyn > nbase_) qsort(s->dyn, s->ndyn, sizeof(unsigned), pbCmpU);   // порядок рисования — по возрастанию номера экземпляра, как прежде
  }
  if (smkOn && pbFastOn && pbChkOn) {   // №915 сверка (только смоук, flag pbchk 1): динамический список и видимые клетки равны тем, что дали бы полные обходы
    unsigned bad = 0, ref = 0;
    for (unsigned i = 0; i < s->n; i++) { unsigned b = s->baked[i]; if (!b || s->hasTr[i] || s->cell[b - 1].force) { float dx = s->px[i] - ex, dz = s->pz[i] - ez; if (dx * dx + dz * dz > far2) continue; if (ref >= s->ndyn || s->dyn[ref] != i) bad++; ref++; } }
    if (ref != s->ndyn) bad++;
    unsigned vb = 0, k = 0; PB_WIN_FOR(s, c) { if (s->cell[c].vis) { if (k >= s->nvis || s->visL[k] != c) vb++; k++; } } if (k != s->nvis) vb++;
    for (unsigned u = 0; u < s->nvis; u++) if (s->cell[s->visL[u]].force) pbChkForce++;
    pbChkFrames++; pbChkDyn += bad; pbChkVis += vb; }
  if (smkOn) { static float acc_[2][4]; static unsigned n_[2]; static float tt_; unsigned t3_ = sceKernelGetSystemTimeLow(); int k_ = s->kind & 1; acc_[k_][0] += (pt1_ - pt0_) / 1000.f; acc_[k_][1] += (pt2_ - pt1_) / 1000.f; acc_[k_][2] += (t3_ - pt2_) / 1000.f; acc_[k_][3] += s->ndyn; n_[k_]++;
    if (k_ == 1 && n_[1] >= 30) { char l_[240]; snprintf(l_, sizeof l_, "PBPROF props: n=%u cells=%u scan=%.3f cellsloop=%.3f dynbuild=%.3f ndyn=%.0f checked=%u/%u | sprops: n=%u scan=%.3f cellsloop=%.3f dynbuild=%.3f ndyn=%.0f checked=%u/%u ms/frame\n", pbP.n, pbP.nc, acc_[0][0] / n_[0], acc_[0][1] / n_[0], acc_[0][2] / n_[0], acc_[0][3] / n_[0], pbP.scanned, pbP.n, pbS.n, acc_[1][0] / n_[1], acc_[1][1] / n_[1], acc_[1][2] / n_[1], acc_[1][3] / n_[1], pbS.scanned, pbS.n); smkWrite(l_); memset(acc_, 0, sizeof acc_); n_[0] = n_[1] = 0; } }
}
// вызвать листы видимых клеток (после pbFrameSet); состояние GE после вызова недействительно
static void pbCallSet(PbSet *s) {
  if (!s->on) return;
  if (pbFastOn) { for (unsigned u = 0; u < s->nvis; u++) { PbCell *C = &s->cell[s->visL[u]]; if (!C->vis || C->force || !C->buf || geOver(GE_HARD)) continue;
    sceGuCallList(C->buf); dlLost(); drawnTris += C->tris; draws += C->draws; pbCalls++; } return; }
  PB_WIN_FOR(s, c) { PbCell *C = &s->cell[c]; if (!C->vis || C->force || !C->buf || geOver(GE_HARD)) continue;
    sceGuCallList(C->buf); dlLost(); drawnTris += C->tris; draws += C->draws; pbCalls++; }
}
// список динамических экземпляров кадра: NULL — запекание выключено или не готово (обходить всех, как прежде); n — число
static unsigned *pbDynList(PbSet *s, unsigned *n) { if (PROP_BAKE_OLD || !s->on) { *n = 0; return 0; } *n = s->ndyn; return s->dyn; }
static void pbGarbageFlush(void) { for (unsigned i = 0; i < pbGarbageN; i++) free(pbGarbage[i]); pbGarbageN = 0; }
static void pbEnsure(void) { pbGarbageFlush(); if (pbTried || PROP_BAKE_OLD) return; pbTried = 1; pbInit(&pbP, 0); pbInit(&pbS, 1); }
// непрозрачные куски экземпляра i сейчас лежат в листе (клетка рисуется листом): прежний путь их пропускает
static inline int pbPartBaked(const PbSet *s, unsigned i) { return !PROP_BAKE_OLD && s->on && s->baked[i] && !s->cell[s->baked[i] - 1].force; }
