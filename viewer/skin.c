#include "xsub.h"
#define XSK(i, n) XS_ADD_SK(i, n)   // 41 пачек, 42 вершин контура, 43/45/48 загрузок костей в проходах контур/тело/тень, 44/46/47/49 вызовов; слова по проходам — прежний счётчик skinW (flag skinw)
// skin.c: .chr loader, animation sampling/blending, bone palette, GE hardware-skinned drawing. See skin.h.
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "skin.h"
#include "levlight.h"

#ifdef SKIN_HOST
#include <stdio.h>
#else
#include <malloc.h>
#include <pspkernel.h>
#include <pspgu.h>
#endif

_Static_assert(sizeof(SkinHeader) == 128 && sizeof(SkinJoint) == 128 && sizeof(SkinBatch) == 48 && sizeof(SkinTex) == 48 &&
               sizeof(SkinClip) == 80 && sizeof(SkinTrackRec) == 16 && sizeof(SkinBlinkKey) == 8, "CHR1 layout");

// ---------------------------------------------------------------- loading
static int inside(const SkinHeader *h, unsigned off, unsigned len) { return off <= h->size && len <= h->size - off; }

int skinBatchValid(const SkinModel *m, const SkinBatch *b) {
  const SkinHeader *h=m->h; unsigned stored=b->nverts;
  if(!b->stride || b->nb<1 || b->nb>8 || b->tex>=h->nTex || b->offVerts>h->size)return 0;
  if(b->flags&4) {
#ifdef CHAR_INDEXED_OLD
    return 0;
#else
    if(h->version!=2 || (b->vtype & (3u<<11))!=(2u<<11) || (b->pad[0]&1) || b->pad[0]>h->size || b->nverts>(h->size-b->pad[0])/2)return 0;
    stored=b->pad[1];if(!stored || stored>65535)return 0;
    const unsigned short *idx=(const unsigned short *)(m->base+b->pad[0]);
    for(unsigned i=0;i<b->nverts;i++)if(idx[i]>=stored)return 0;
#endif
  }
  if(stored>(h->size-b->offVerts)/b->stride)return 0;
  for(unsigned j=0;j<b->nb;j++)if(b->bones[j]>=h->nJoints)return 0;
  return 1;
}

// №914 (оптимизация x_trf: пешеход на L5 — 2200–2600 слов GE за вызов, из них три прохода (оболочки, тело, ступень тени) по ~800 слов — загрузка матриц костей: у пачки 5–8 костей,
// 13 слов каждая; слот костей k пачки — это k-й вес вершины, а какая кость в каком слоте, экспорт (tools/char_export.py slot) выбирал без оглядки на соседние пачки: соседние пачки
// делят до половины костей, но в разных слотах, и кэш слотов движка (slotBone) почти не срабатывал). Теперь при создании модели слоты переставляются один раз: пачки идут в порядке
// рисования, кость, уже лежащая в слоте s < nb в GE от прошлой пачки, остаётся в слоте s; остальные — в свободные слоты по порядку. Перестановка — в весах вершин (первые nb байт 8 бит) и в bones[]
// пачки: сумма Σ w_k · M(кость k) для вершины та же (порядок слагаемых другой: ошибка округления GE порядка 1e-7). Модель с уже переставленными слотами (метка в заголовке) не трогается.
// SKIN_SLOTOPT_OLD=1 / файл skinslotold рядом с EBOOT (читается до загрузки уровня; skinSetSlotOpt) — прежние слоты
#ifndef SKIN_SLOTOPT_OLD
#define SKIN_SLOTOPT_OLD 0
#endif
static int skinSlotOptOn = !SKIN_SLOTOPT_OLD; unsigned skinSlotModels, skinSlotBefore, skinSlotAfter;   // загрузок матриц (костей) по порядку пачек до и после (сумма по моделям)
void skinSetSlotOpt(int on) { skinSlotOptOn = on; }
// стоимость (загрузок костей) трёх проходов skinDrawPal при назначении слотов asg: оболочки (тело и глаза, flags&3 равен 0 или 1) с пустых слотов, тело (все пачки), ступень тени (flags&3 равен 0);
// слоты переходят из прохода в проход, как в GE
static unsigned slotCost(const SkinModel *m, unsigned n, unsigned char asg[][8], const unsigned char *nbs) {
  unsigned char st[8]; memset(st, 0, sizeof st); unsigned c = 0;
  for (int pass = 0; pass < 3; pass++)
    for (unsigned i = 0; i < n; i++) { unsigned f = m->b[i].flags & 3;
      if ((pass == 0 && f > 1) || (pass == 2 && f)) continue;
      for (unsigned k = 0; k < nbs[i]; k++) if (st[k] != asg[i][k] + 1) { st[k] = (unsigned char)(asg[i][k] + 1); c++; } }
  return c;
}
void skinSlotOpt(const SkinModel *mc) {
  if (!mc || !mc->h || mc->h->nBatches > 64) return; SkinHeader *h = (SkinHeader *)mc->h; SkinModel *m = (SkinModel *)mc;
  if (!skinSlotOptOn || h->pad[7] == 0x534C4F54u) return;
  unsigned n = h->nBatches; unsigned char asg[64][8], org[64][8], nbs[64], fix[64]; unsigned char st[8]; memset(st, 0, sizeof st);
  for (unsigned i = 0; i < n; i++) {
    const SkinBatch *b = &m->b[i]; unsigned nb = b->nb; nbs[i] = (unsigned char)(nb > 8 ? 8 : nb); memset(asg[i], 0, 8); memset(org[i], 0, 8);
    for (unsigned k = 0; k < nbs[i]; k++) { org[i][k] = asg[i][k] = b->bones[k]; }
    int dup = nb < 1 || nb > 8; for (unsigned a = 0; a < nbs[i]; a++) for (unsigned c = a + 1; c < nbs[i]; c++) if (b->bones[a] == b->bones[c]) dup = 1;
    unsigned sv = skinBatchStoredVerts(b);
    fix[i] = !(!dup && ((b->vtype >> 9) & 3) == 1 && (int)(((b->vtype >> 14) & 7) + 1) == (int)nb && b->stride >= nb && sv && (uint64_t)b->offVerts + (uint64_t)sv * b->stride <= h->size);
  }
  unsigned before = slotCost(m, n, org, nbs);
  for (unsigned i = 0; i < n; i++) {   // начальное назначение: кость, уже лежащая в слоте от прошлой пачки, остаётся в нём
    if (fix[i]) { for (unsigned k = 0; k < nbs[i]; k++) st[k] = asg[i][k] + 1; continue; }
    unsigned nb = nbs[i]; int nw[8], used[8] = {0, 0, 0, 0, 0, 0, 0, 0}; for (unsigned k = 0; k < nb; k++) nw[k] = -1;
    for (unsigned k = 0; k < nb; k++) for (unsigned sl = 0; sl < nb; sl++) if (nw[sl] < 0 && st[sl] == org[i][k] + 1) { nw[sl] = (int)k; used[k] = 1; break; }
    for (unsigned k = 0; k < nb; k++) if (!used[k]) for (unsigned sl = 0; sl < nb; sl++) if (nw[sl] < 0) { nw[sl] = (int)k; break; }
    for (unsigned sl = 0; sl < nb; sl++) asg[i][sl] = org[i][nw[sl]];
    for (unsigned k = 0; k < nb; k++) st[k] = asg[i][k] + 1;
  }
  unsigned best = slotCost(m, n, asg, nbs);
  for (int round = 0; round < 8; round++) {   // подъём по обменам слотов внутри пачки, пока стоимость трёх проходов падает
    int imp = 0;
    for (unsigned i = 0; i < n; i++) { if (fix[i]) continue;
      for (unsigned a = 0; a < nbs[i]; a++) for (unsigned c = a + 1; c < nbs[i]; c++) {
        unsigned char t = asg[i][a]; asg[i][a] = asg[i][c]; asg[i][c] = t;
        unsigned cc = slotCost(m, n, asg, nbs); if (cc < best) { best = cc; imp = 1; } else { t = asg[i][a]; asg[i][a] = asg[i][c]; asg[i][c] = t; } } }
    if (!imp) break;
  }
  for (unsigned i = 0; i < n; i++) { if (fix[i]) continue;
    SkinBatch *b = (SkinBatch *)&m->b[i]; unsigned nb = nbs[i]; int nw[8], ident = 1;   // nw[слот] = старый номер слота кости
    for (unsigned sl = 0; sl < nb; sl++) { nw[sl] = -1; for (unsigned k = 0; k < nb; k++) if (org[i][k] == asg[i][sl]) nw[sl] = (int)k; if (nw[sl] != (int)sl) ident = 0; }
    if (ident) continue;
    unsigned sv = skinBatchStoredVerts(b); unsigned char *vb = (unsigned char *)m->base + b->offVerts;
#ifndef SKIN_SLOTOPT_BREAK   // отрицательный контроль хост-гейта tests/skinslot: кости переставлены, веса нет
    for (unsigned v = 0; v < sv; v++) { unsigned char *w = vb + (size_t)v * b->stride, t[8]; memcpy(t, w, nb); for (unsigned sl = 0; sl < nb; sl++) w[sl] = t[nw[sl]]; }
#endif
    for (unsigned sl = 0; sl < nb; sl++) b->bones[sl] = asg[i][sl];
  }
  h->pad[7] = 0x534C4F54u; skinSlotModels++; skinSlotBefore += before; skinSlotAfter += best;
#ifndef SKIN_HOST
  sceKernelDcacheWritebackRange((void *)m->base, h->size);
#endif
}
static SkinModel *skinFromBuf(unsigned char *d, unsigned size);
// №775: причина последнего отказа skinLoad (игрок уровня на PSP «только машина»): 1 не открылся, 2 нет памяти, 3 прочитано меньше (skinLoadGot — сколько),
// 4 разбор (заголовок, границы, батчи). 0 — прочитан
int skinLoadErr; unsigned skinLoadGot;
SkinModel *skinLoad(const char *path) {
  unsigned char *d = 0; unsigned size = 0; skinLoadErr = 0; skinLoadGot = 0;
#ifdef SKIN_HOST
  FILE *f = fopen(path, "rb"); if (!f) { skinLoadErr = 1; return 0; }
  fseek(f, 0, SEEK_END); size = (unsigned)ftell(f); fseek(f, 0, SEEK_SET);
  d = aligned_alloc(64, (size + 63) & ~63u);
  if (!d || fread(d, 1, size, f) != size) { skinLoadErr = d ? 3 : 2; fclose(f); free(d); return 0; }
  fclose(f);
#else
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0); if (fd < 0) { skinLoadErr = 1; return 0; }
  size = (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET);
  d = memalign(64, size);
  if (!d) { skinLoadErr = 2; sceIoClose(fd); return 0; }
  int got = sceIoRead(fd, d, size);
  if (got != (int)size) { skinLoadErr = 3; skinLoadGot = (unsigned)got; sceIoClose(fd); free(d); return 0; }
  sceIoClose(fd);
#endif
  SkinModel *m = skinFromBuf(d, size); if (!m) skinLoadErr = 4;
  return m;
}
// разбор и проверка прочитанного файла .chr (d — память под модель, при отказе освобождается)
static SkinModel *skinFromBuf(unsigned char *d, unsigned size) {
  const SkinHeader *h = (const SkinHeader *)d;
  if (size < sizeof(SkinHeader) || memcmp(h->magic, "CHR1", 4) || (h->version != 1 && h->version != 2) || h->size != size || h->nJoints > SKIN_MAXJ ||
      !inside(h, h->offJoints, h->nJoints * 128) || !inside(h, h->offBatches, h->nBatches * 48) || !inside(h, h->offTex, h->nTex * 48) ||
      !inside(h, h->offClips, h->nClips * 80) || !inside(h, h->offBlink, h->nBlink * 8)) { free(d); return 0; }
#ifdef CHAR_INDEXED_OLD
  if(h->version!=1){free(d);return 0;}
#endif
  SkinModel *m = malloc(sizeof *m); if (!m) { free(d); return 0; }
  m->base = d; m->h = h;
  m->j = (const SkinJoint *)(d + h->offJoints); m->b = (const SkinBatch *)(d + h->offBatches);
  m->tex = (const SkinTex *)(d + h->offTex); m->c = (const SkinClip *)(d + h->offClips);
  m->blink = (const SkinBlinkKey *)(d + h->offBlink);
  for (unsigned i = 0; i < h->nBatches; i++) {       // everything the GE will read must be inside the file
    const SkinBatch *b = &m->b[i];
    if (!skinBatchValid(m,b)) { free(d); free(m); return 0; }
  }
  for (unsigned i = 0; i < h->nClips; i++) if (!inside(h, m->c[i].offTracks, h->nJoints * 16)) { free(d); free(m); return 0; }
  skinSlotOpt(m);   // №914
#ifndef SKIN_HOST
  sceKernelDcacheWritebackRange(d, size);            // the GE reads vertices, CLUTs and texels from RAM
#endif
  return m;
}

// Предзагрузка модели без остановки кадра (разбор ночи 11.10 / тур 12: чтение .chr ~21–28 мс на модель — кадр 40–70 мс при каждой подгрузке персонажа этапа).
// PSP: чтение уходит в sceIoReadAsync, кадры идут; skinPreReady — прочитано, skinPreFinish — разбор из памяти (≈ мс); если ещё не прочитано, Finish ждёт остаток.
// Хост (SKIN_HOST, боты): читает сразу при Start — поведение и порядок прежние. Результат тот же, что у skinLoad.
struct SkinPre { int fd; unsigned char *d; unsigned size; int done, err; long long res; };   // done/res: итог sceIoPollAsync, когда он вернул «готово» (поллинг забирает результат: sceIoWaitAsync после него даёт ошибку)
SkinPre *skinPreStart(const char *path) {
  SkinPre *p = calloc(1, sizeof *p); if (!p) return 0; p->fd = -1;
#ifdef SKIN_HOST
  FILE *f = fopen(path, "rb"); if (!f) { free(p); return 0; }
  fseek(f, 0, SEEK_END); p->size = (unsigned)ftell(f); fseek(f, 0, SEEK_SET);
  p->d = aligned_alloc(64, (p->size + 63) & ~63u);
  if (!p->d || fread(p->d, 1, p->size, f) != p->size) { fclose(f); free(p->d); free(p); return 0; }
  fclose(f);
#else
  p->fd = sceIoOpen(path, PSP_O_RDONLY, 0); if (p->fd < 0) { free(p); return 0; }
  p->size = (unsigned)sceIoLseek32(p->fd, 0, PSP_SEEK_END); sceIoLseek32(p->fd, 0, PSP_SEEK_SET);
  p->d = memalign(64, p->size);
  if (!p->d || sceIoReadAsync(p->fd, p->d, p->size) < 0) { sceIoClose(p->fd); free(p->d); free(p); return 0; }
#endif
  return p;
}
// Ревизия 11.10 (1.2): sceIoPollAsync возвращает 0 — готово, 1 — идёт, отрицательное — ОШИБКА; ошибку считали «ещё идёт» вечно: слот очереди, буфер и дескриптор не освобождались, после четырёх таких
// сбоев модели персонажей этапа не грузились. Теперь ошибка = «закончено» (skinPreFinish вернёт 0, вызывающий читает синхронно). SKINPRE_ERR_OLD=1 — прежнее
// 15.10 (TEST18, разбор «недогрузки» D_L1_city): фоновое чтение моделей персонажей не срабатывало ни разу — 94 из 94 «MSNNPC async-fail, sync read» (каждый раз синхронное чтение 28–33 мс в кадре):
// skinPreReady опрашивал sceIoPollAsync (забирает результат), а skinPreFinish затем звал sceIoWaitAsync, который после этого возвращает ошибку. Теперь итог опроса запоминается и Wait не зовётся.
// SKINPRE_POLL_RES_OLD=1 — прежнее
#ifndef SKINPRE_POLL_RES_OLD
#define SKINPRE_POLL_RES_OLD 0
#endif
int skinPreDone(int pollRet) { return SKINPRE_ERR_OLD ? pollRet == 0 : pollRet <= 0; }
int skinPreReady(SkinPre *p) {
#ifdef SKIN_HOST
  (void)p; return 1;
#else
  if (!p) return 0;
  if (!SKINPRE_POLL_RES_OLD && p->done) return 1;
  SceInt64 res = -1; int r = sceIoPollAsync(p->fd, &res), d = skinPreDone(r);
  if (!SKINPRE_POLL_RES_OLD && d) { p->done = 1; p->err = r < 0; p->res = res; }
  return d;
#endif
}
SkinModel *skinPreFinish(SkinPre *p) {
  if (!p) return 0;
#ifndef SKIN_HOST
  SceInt64 res = -1; int r = 0;
  if (!SKINPRE_POLL_RES_OLD && p->done && !p->err) res = p->res;   // опрос уже вернул «готово» и забрал результат — Wait дал бы ошибку и модель шла бы синхронным чтением (TEST18: 94 из 94 «async-fail»)
  else r = sceIoWaitAsync(p->fd, &res);
  sceIoClose(p->fd);
  if (r < 0 || res != (SceInt64)p->size) { free(p->d); free(p); return 0; }
#endif
  unsigned char *d = p->d; unsigned size = p->size; free(p);
  return skinFromBuf(d, size);
}
void skinPreCancel(SkinPre *p) {
  if (!p) return;
#ifndef SKIN_HOST
  SceInt64 res; sceIoWaitAsync(p->fd, &res); sceIoClose(p->fd);
#endif
  free(p->d); free(p);
}

static struct { const SkinModel *m; int clip; float lift; } footCache[96]; static int nFootCache;   // №510: skinFootLift
void skinFree(SkinModel *m) {
  if (m) { for (int i = 0; i < nFootCache; ) if (footCache[i].m == m) footCache[i] = footCache[--nFootCache]; else i++;   // адрес модели может достаться другой
    free((void *)m->base); free(m); } }

int skinFindClip(const SkinModel *m, const char *name) {
  size_t n = strlen(name);
  for (unsigned i = 0; i < m->h->nClips; i++) if (!strcmp(m->c[i].name, name)) return (int)i;
  for (unsigned i = 0; i < m->h->nClips; i++) {
    size_t l = strlen(m->c[i].name);
    if (l > n && m->c[i].name[l - n - 1] == '_' && !strcmp(m->c[i].name + l - n, name)) return (int)i;
  }
  return -1;
}
int skinFindJoint(const SkinModel *m, const char *name) {
  for (unsigned i = 0; i < m->h->nJoints; i++) if (!strcmp(m->j[i].name, name)) return (int)i;
  return -1;
}

// ---------------------------------------------------------------- sampling
// key pair for frame f: fr[k] <= f < fr[k+1]; caller handles f outside [fr[0], fr[n-1]]
static int findKey(const unsigned short *fr, int n, float f) {
  int lo = 0, hi = n - 1;
  while (hi - lo > 1) { int mid = (lo + hi) >> 1; if ((float)fr[mid] <= f) lo = mid; else hi = mid; }
  return lo;
}

// shortest path; nlerp while the two rotations are close (dot > 0.95, < 36 deg), slerp beyond (a few Homer tracks
// have keys up to 120 deg apart). Result normalised.
static void qmix(float *o, const float *a, const float *b, float u) {
  float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], sb = 1.f, ka, kb;
  if (d < 0) { d = -d; sb = -1.f; }
  if (d > .95f) { ka = 1.f - u; kb = u; }
  else { float th = acosf(d > 1.f ? 1.f : d), s = 1.f / sinf(th); ka = sinf((1.f - u) * th) * s; kb = sinf(u * th) * s; }
  kb *= sb;
  float x = ka * a[0] + kb * b[0], y = ka * a[1] + kb * b[1], z = ka * a[2] + kb * b[2], w = ka * a[3] + kb * b[3];
  float r = 1.f / sqrtf(x * x + y * y + z * z + w * w);
  o[0] = x * r; o[1] = y * r; o[2] = z * r; o[3] = w * r;
}

#define Q16 (1.f / 32767.f)
static void rotAt(const unsigned char *base, const SkinTrackRec *tr, float f, float *q) {
  int n = tr->nRot; const unsigned short *fr = (const unsigned short *)(base + tr->offRot);
  const short *v = (const short *)(base + tr->offRot + ((2 * n + 3) & ~3));
  int k; float u;
  if (n == 1 || f <= fr[0]) { k = 0; u = 0; }
  else if (f >= fr[n - 1]) { k = n - 1; u = 0; }
  else { k = findKey(fr, n, f); u = (f - fr[k]) / (float)(fr[k + 1] - fr[k]); }
  const short *a = v + 4 * k;
  if (u == 0.f) {
    float x = a[0] * Q16, y = a[1] * Q16, z = a[2] * Q16, w = a[3] * Q16, r = 1.f / sqrtf(x * x + y * y + z * z + w * w);
    q[0] = x * r; q[1] = y * r; q[2] = z * r; q[3] = w * r; return;
  }
  float qa[4] = {a[0] * Q16, a[1] * Q16, a[2] * Q16, a[3] * Q16}, qb[4] = {a[4] * Q16, a[5] * Q16, a[6] * Q16, a[7] * Q16};
  qmix(q, qa, qb, u);
}
static void transAt(const unsigned char *base, const SkinTrackRec *tr, float f, float *t) {
  int n = tr->nTrans; const unsigned short *fr = (const unsigned short *)(base + tr->offTrans);
  const float *v = (const float *)(base + tr->offTrans + ((2 * n + 3) & ~3));
  int k; float u;
  if (n == 1 || f <= fr[0]) { k = 0; u = 0; }
  else if (f >= fr[n - 1]) { k = n - 1; u = 0; }
  else { k = findKey(fr, n, f); u = (f - fr[k]) / (float)(fr[k + 1] - fr[k]); }
  const float *a = v + 3 * k;
  if (u == 0.f) { t[0] = a[0]; t[1] = a[1]; t[2] = a[2]; return; }
  t[0] = a[0] + (a[3] - a[0]) * u; t[1] = a[1] + (a[4] - a[1]) * u; t[2] = a[2] + (a[5] - a[2]) * u;
}

static float clipFrame(const SkinClip *c, float t) {
  float f = t * c->fps, last = c->frames - 1.f;
  return f < 0.f ? 0.f : f > last ? last : f;
}

void skinSample(const SkinModel *m, int clip, float t, SkinXf *out) {
  int nj = (int)m->h->nJoints, root = (int)m->h->rootJoint;
  if (clip < 0 || clip >= (int)m->h->nClips) {       // no clip: bind pose
    for (int j = 0; j < nj; j++) { memcpy(out[j].q, m->j[j].q, 16); memcpy(out[j].t, m->j[j].t, 12); }
    return;
  }
  const SkinClip *c = &m->c[clip]; float f = clipFrame(c, t);
  const SkinTrackRec *tr = (const SkinTrackRec *)(m->base + c->offTracks);
  for (int j = 0; j < nj; j++, tr++) {
    if (tr->nRot) rotAt(m->base, tr, f, out[j].q); else memcpy(out[j].q, m->j[j].q, 16);
    if (tr->nTrans && j != root) transAt(m->base, tr, f, out[j].t); else memcpy(out[j].t, m->j[j].t, 12);
  }
}

void skinRootMotion(const SkinModel *m, int clip, float t, float out[3]) {
  int root = (int)m->h->rootJoint;
  memcpy(out, m->j[root].t, 12);
  if (clip < 0 || clip >= (int)m->h->nClips) return;
  const SkinClip *c = &m->c[clip];
  const SkinTrackRec *tr = (const SkinTrackRec *)(m->base + c->offTracks) + root;
  if (tr->nTrans) transAt(m->base, tr, clipFrame(c, t), out);
}

void skinBlend(SkinXf *a, const SkinXf *b, float w, int n) {
  if (w <= 0.f) return;
  if (w >= 1.f) { memcpy(a, b, sizeof(SkinXf) * n); return; }
  for (int j = 0; j < n; j++) {
    qmix(a[j].q, a[j].q, b[j].q, w);
    for (int k = 0; k < 3; k++) a[j].t[k] += (b[j].t[k] - a[j].t[k]) * w;
  }
}

// ---------------------------------------------------------------- palette
// affine 3x4 matrices in ScePspFMatrix4 layout: x, y, z = basis columns, w = translation (w components unused)
static void mul34(ScePspFMatrix4 *o, const ScePspFMatrix4 *a, const float *b /* 4 columns x 3 */) {
  const float *A = (const float *)a; float *O = (float *)o;
  for (int c = 0; c < 4; c++) {
    const float *bc = b + 3 * c;
    O[4 * c + 0] = A[0] * bc[0] + A[4] * bc[1] + A[8] * bc[2];
    O[4 * c + 1] = A[1] * bc[0] + A[5] * bc[1] + A[9] * bc[2];
    O[4 * c + 2] = A[2] * bc[0] + A[6] * bc[1] + A[10] * bc[2];
    O[4 * c + 3] = 0.f;
  }
  O[12] += A[12]; O[13] += A[13]; O[14] += A[14]; O[15] = 1.f;
}

void skinPalette(const SkinModel *m, const SkinXf *xf, ScePspFMatrix4 *pal, ScePspFMatrix4 *world) {
  ScePspFMatrix4 wtmp[SKIN_MAXJ];
  ScePspFMatrix4 *W = world ? world : wtmp;
  int nj = (int)m->h->nJoints;
  for (int j = 0; j < nj; j++) {
    const SkinJoint *jt = &m->j[j]; const float *q = xf[j].q;
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float L[12] = {                                   // local = T * R * S as 4 columns x 3 rows
      (1 - 2 * (y * y + z * z)) * jt->s[0], (2 * (x * y + z * w)) * jt->s[0], (2 * (x * z - y * w)) * jt->s[0],
      (2 * (x * y - z * w)) * jt->s[1], (1 - 2 * (x * x + z * z)) * jt->s[1], (2 * (y * z + x * w)) * jt->s[1],
      (2 * (x * z + y * w)) * jt->s[2], (2 * (y * z - x * w)) * jt->s[2], (1 - 2 * (x * x + y * y)) * jt->s[2],
      xf[j].t[0], xf[j].t[1], xf[j].t[2]};
    if (jt->parent < 0) {
      float *o = (float *)&W[j];
      for (int c = 0; c < 4; c++) { o[4 * c] = L[3 * c]; o[4 * c + 1] = L[3 * c + 1]; o[4 * c + 2] = L[3 * c + 2]; o[4 * c + 3] = 0; }
      o[15] = 1.f;
    } else mul34(&W[j], &W[jt->parent], L);
    mul34(&pal[j], &W[j], jt->inv);
  }
}

// ---------------------------------------------------------------- player
void skinInit(SkinPlayer *p, const SkinModel *m) {
  memset(p, 0, sizeof *p);
  p->m = m; p->clip = p->prev = -1; p->speed = 1.f; p->eyeTex = -1; p->seed = 12345u;
  p->blinkWait = SKIN_BLINK_OLD ? 2.f : 0.f; p->blinkT = -1.f; p->jaw = skinFindJoint(m, "Jaw"); p->jawOpen = 0.f;
  skinSample(m, -1, 0, p->xf); skinPalette(m, p->xf, p->pal, p->world);
}

void skinPlay(SkinPlayer *p, int clip, int loop, float fade) {
  if (clip == p->clip) { p->loop = loop; return; }
  if (p->clip >= 0 && fade > 0.f) {
    // if a fade is still running the outgoing pose is approximated by the current clip only
    p->prev = p->clip; p->prevT = p->t; p->prevLoop = p->loop; p->fade = 0.f; p->fadeLen = fade;
  } else p->prev = -1;
  p->clip = clip; p->t = 0.f; p->loop = loop;
}

static float advance(const SkinModel *m, int clip, float t, int loop) {
  if (clip < 0) return 0.f;
  float len = m->c[clip].length;
  if (len <= 0.f) return 0.f;
  if (loop) { t = fmodf(t, len); if (t < 0) t += len; }
  else if (t > len) t = len;
  return t;
}

void skinTick(SkinPlayer *p, float dt) {
  const SkinModel *m = p->m; float d = dt * p->speed;
  p->t = advance(m, p->clip, p->t + d, p->loop);
  if (p->prev >= 0) {
    p->fade += dt;
    if (p->fade >= p->fadeLen) p->prev = -1;
    else p->prevT = advance(m, p->prev, p->prevT + d, p->prevLoop);
  }
  // eye blink (original texture animation, 9 frames @ 30 fps), every 2..6 s
  p->eyeTex = -1;
  if (m->h->nBlink && m->h->blinkFps > 0.f) {
    if (p->blinkT < 0.f) {
      p->blinkWait -= dt;
      if (p->blinkWait <= 0.f) p->blinkT = 0.f;
    }
    if (p->blinkT >= 0.f) {
      float f = p->blinkT * m->h->blinkFps; const SkinBlinkKey *k = m->blink; int n = (int)m->h->nBlink;
      if (f > k[n - 1].frame + 1.f) {
        p->blinkT = -1.f; p->seed = p->seed * 1103515245u + 12345u;
        p->blinkWait = SKIN_BLINK_OLD ? 2.f + (float)((p->seed >> 16) & 1023) * (4.f / 1023.f) : 2.f + (float)((p->seed >> 16) & 1023) * (2.f / 1023.f);   // сверка g04-homer P15-01: 2,0…4,0 с
      } else {
        int i = 0; while (i + 1 < n && k[i + 1].frame <= f) i++;
        p->eyeTex = k[i].tex; p->blinkT += dt;
      }
    }
  }
}

void skinPose(SkinPlayer *p) {
  const SkinModel *m = p->m; int nj = (int)m->h->nJoints;
  skinSample(m, p->clip, p->t, p->xf);
  if (p->prev >= 0 && p->fadeLen > 0.f) {
    skinSample(m, p->prev, p->prevT, p->tmp);
    float u = p->fade / p->fadeLen; u = u * u * (3.f - 2.f * u);        // smoothstep
    skinBlend(p->tmp, p->xf, u, nj);
    memcpy(p->xf, p->tmp, sizeof(SkinXf) * nj);
  }
  if (p->jawOpen != 0.f) skinJawOpen(p->xf, p->jaw, p->jawOpen);
  skinPalette(m, p->xf, p->pal, p->world);
}
void skinJawOpen(SkinXf *xf, int j, float rad) {
  if (j < 0 || j >= SKIN_MAXJ || rad == 0.f) return;
  float s = sinf(rad * .5f), c = cosf(rad * .5f), *q = xf[j].q, x = q[0], y = q[1], z = q[2], w = q[3];   // q * (0, 0, s, c)
  q[0] = x * c + y * s; q[1] = y * c - x * s; q[2] = z * c + w * s; q[3] = w * c - z * s;
}

void skinUpdate(SkinPlayer *p, float dt) { skinTick(p, dt); skinPose(p); }

// сверка g04-homer P15-02: ход рта говорящего по оригиналу. Угол идёт с постоянной скоростью до границы (нижняя −18° у всех, верхняя своя), на каждом развороте границы заново
// сдвигаются внутрь на случайное 0…shift и выбирается новая скорость в диапазоне персонажа; первый ход от нуля в минус. Размах ≈ 36°, ход вверх или вниз ≈ 0,22…0,25 с
static float mouthRnd(SkinMouth *mo) { mo->seed = mo->seed * 1664525u + 1013904223u; return (float)((mo->seed >> 8) & 0xFFFF) / 65535.f; }
float skinMouthStep(SkinMouth *mo, const char *model, float dt) {
  if (!mo->init) {
    static const struct { const char *n; float hi, vlo, vhi, sh; } T[] = {   // круг 3: таблица по проверке исходника (верхний угол °, скорость от…до, сдвиг границ °)
      {"homer", 24.1f, 2.00f, 3.44f, 7.83f}, {"bart", 29.9f, 2.50f, 4.00f, 7.2f}, {"lisa", 27.0f, 2.50f, 4.00f, 7.2f}, {"marge", 24.1f, 1.66f, 3.44f, 7.45f},
      {"apu", 28.1f, 2.20f, 3.78f, 7.45f},
      {"snake", 32.0f, 2.56f, 4.12f, 7.45f}, {"gil", 32.0f, 2.56f, 4.12f, 7.45f}, {"willie", 32.0f, 2.56f, 4.12f, 7.45f}, {"hibbert", 32.0f, 2.56f, 4.12f, 7.45f},
      {"wiggum", 32.0f, 2.56f, 4.12f, 7.45f}, {"barney", 32.0f, 2.56f, 4.12f, 7.45f}, {"captain", 32.0f, 2.56f, 4.12f, 7.45f}, {"cbg", 32.0f, 2.56f, 4.12f, 7.45f}, {"zombie", 32.0f, 2.56f, 4.12f, 7.45f},
      {"milhouse", 29.9f, 2.22f, 4.00f, 7.2f}, {"ralph", 27.0f, 2.22f, 4.00f, 7.2f}, {"moleman", 25.9f, 1.56f, 3.54f, 7.2f}, {"mole", 25.9f, 1.56f, 3.54f, 7.2f},
      {"grandpa", 24.1f, 2.20f, 3.56f, 7.83f}, {"grampa", 24.1f, 2.20f, 3.56f, 7.83f}, {"krusty", 24.1f, 2.20f, 3.56f, 7.83f}, {"patty", 24.1f, 2.20f, 3.56f, 7.83f}, {"selma", 24.1f, 2.20f, 3.56f, 7.83f},
      {"skinner", 28.1f, 2.00f, 3.78f, 7.45f}, {"burns", 28.1f, 2.00f, 3.78f, 7.45f}, {"carl", 28.1f, 2.00f, 3.78f, 7.45f}, {"lenny", 28.1f, 2.00f, 3.78f, 7.45f}, {"louie", 28.1f, 2.00f, 3.78f, 7.45f},
      {"moe", 28.1f, 2.00f, 3.78f, 7.45f}, {"otto", 28.1f, 2.00f, 3.78f, 7.45f}, {"smithers", 28.1f, 2.00f, 3.78f, 7.45f},
      {"ned", 28.1f, 1.78f, 3.78f, 7.45f}, {"cletus", 24.1f, 1.88f, 3.78f, 7.45f}, {"frink", 24.1f, 1.88f, 3.78f, 7.45f},
      {"jimbo", 27.0f, 1.88f, 3.78f, 7.45f}, {"teen", 27.0f, 1.88f, 3.78f, 7.45f}, {"nriviera", 27.0f, 1.88f, 3.78f, 7.45f}, {"kearney", 29.9f, 2.34f, 3.78f, 7.2f} };
    float hi = 25.9f, vlo = 1.66f, vhi = 3.44f, sh = 7.45f;   // по умолчанию (и для имён вне таблицы)
    char low[48]; int n = 0; for (; model && model[n] && n < 47; n++) low[n] = (char)((model[n] >= 'A' && model[n] <= 'Z') ? model[n] + 32 : model[n]); low[n] = 0;
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (strstr(low, T[i].n)) { hi = T[i].hi; vlo = T[i].vlo; vhi = T[i].vhi; sh = T[i].sh; break; }
    mo->hi = mo->hi0 = hi * 0.0174533f; mo->lo = -18.f * 0.0174533f; mo->vlo = vlo; mo->vhi = vhi; mo->shift = sh * 0.0174533f;
    mo->a = 0.f; mo->dir = -1; mo->seed = mo->seed ? mo->seed : 2463534242u; mo->v = vlo + (vhi - vlo) * mouthRnd(mo); mo->init = 1;
  }
  float tgt = mo->dir < 0 ? mo->lo : mo->hi;   // граница, к которой идёт; она сдвигается внутрь на каждом развороте (ниже)
  mo->a += mo->dir * mo->v * dt;
  if (mo->dir < 0 ? mo->a <= tgt : mo->a >= tgt) {
    mo->a = tgt; mo->dir = -mo->dir;
    mo->v = mo->vlo + (mo->vhi - mo->vlo) * mouthRnd(mo);
    // границы заново сдвигаются внутрь на случайное 0…shift (относительно исходных −18° и верхней)
    mo->lo = -18.f * 0.0174533f + mouthRnd(mo) * mo->shift;
    mo->hi = mo->hi0 - mouthRnd(mo) * mo->shift;
  }
  return mo->a;
}

float skinClipSpeed(const SkinModel *m, int clip) {
  if (clip < 0 || clip >= (int)m->h->nClips || m->c[clip].length <= 0.f) return 0.f;
  float a[3], b[3]; skinRootMotion(m, clip, 0.f, a); skinRootMotion(m, clip, m->c[clip].length, b);
  float dx = b[0] - a[0], dz = b[2] - a[2];
  return sqrtf(dx * dx + dz * dz) / m->c[clip].length;
}

// №510 (Роб: «Милхаус ходит выше уровня»): клипы общего набора npd_loco_* сняты под один скелет, а у персонажей разные пропорции — стопы в клипе
// выше (Милхаус +0,18…0,22 м, Ральф +0,3…0,34, Нельсон +0,14…0,18) или ниже (Скиннер, Гил −0,11) своих стоп в позе привязки; клипы
// собственного набора (Барт, Лиза, Гомер) совпадают. Правило порта: самый низкий сустав стопы/носка/щиколотки клипа (min по кадрам) возвращается
// на высоту этого сустава в позе привязки — смещение по высоте при расстановке (skinPlace(..., y + lift)). Считается один раз на пару (модель,
// клип) и кешируется; меньше 3 см — 0. -DSKIN_FOOTLIFT_OLD=1 — без поправки
#ifndef SKIN_FOOTLIFT_OLD
#define SKIN_FOOTLIFT_OLD 0
#endif
static int footJoint(const char *n) {
  for (const char *p = n; *p; p++) { char c = (char)(*p | 0x20);
    if (c == 'f' && (p[1] | 0x20) == 'o' && (p[2] | 0x20) == 'o' && (p[3] | 0x20) == 't') return 1;
    if (c == 't' && (p[1] | 0x20) == 'o' && (p[2] | 0x20) == 'e') return 1;
    if (c == 'a' && (p[1] | 0x20) == 'n' && (p[2] | 0x20) == 'k' && (p[3] | 0x20) == 'l' && (p[4] | 0x20) == 'e') return 1; }
  return 0;
}
// №762 (Роб 17.10, TEST32 на PSP: «продавец ходит в воздухе, стопы на уровне колен Гомера»): у части моделей подошвы в позе привязки не на нуле — низ модели
// (bbox[1]; сустав стопы у всех на 5–10 см выше него): gil +0,30, skinner +0,36, прохожие male2/moleman/ralph/kearney/eddie +0,11…0,14, patty/selma −0,40.
// Поправка стоп считала только разницу клипа и позы привязки — персонаж висел или тонул на эту высоту. Теперь подошва позы привязки вычитается всегда
// (у Гомера и большинства 0). SKIN_SOLE_OLD=1 / «flag soleold 1» — как было
#ifndef SKIN_SOLE_OLD
#define SKIN_SOLE_OLD 0
#endif
static int skinSoleOld = SKIN_SOLE_OLD;
static float skinSole(const SkinModel *m) { if (skinSoleOld || !m || !m->h) return 0.f; float s = m->h->bbox[1]; return (fabsf(s) >= .03f && fabsf(s) < .8f) ? s : 0.f; }
void skinSetSoleOld(int on) { skinSoleOld = on; nFootCache = 0; }
float skinFootLift(const SkinModel *m, int clip) {
  if (SKIN_FOOTLIFT_OLD || !m) return 0.f;
  if (clip < 0 || clip >= (int)m->h->nClips || m->c[clip].length <= 0.f) return -skinSole(m);
  for (int i = 0; i < nFootCache; i++) if (footCache[i].m == m && footCache[i].clip == clip) return footCache[i].lift;
  float rest = 1e9f, low = 1e9f;
  for (unsigned j = 0; j < m->h->nJoints; j++) { if (!footJoint(m->j[j].name)) continue;
    const float *v = m->j[j].inv; float y = -(v[3] * v[9] + v[4] * v[10] + v[5] * v[11]);   // начало сустава в позе привязки: −Rᵀ·p, строка y (столбец 1 матрицы GE)
    if (y < rest) rest = y; }
  if (rest < 1e8f) {
    SkinXf xf[SKIN_MAXJ]; ScePspFMatrix4 pal[SKIN_MAXJ] __attribute__((aligned(16))), wd[SKIN_MAXJ] __attribute__((aligned(16)));
    for (int f = 0; f < 12; f++) { skinSample(m, clip, m->c[clip].length * (float)f / 12.f, xf); skinPalette(m, xf, pal, wd);
      for (unsigned j = 0; j < m->h->nJoints; j++) if (footJoint(m->j[j].name) && wd[j].w.y < low) low = wd[j].w.y; } }
  float lift = (rest < 1e8f && low < 1e8f) ? rest - low : 0.f;
  if (fabsf(lift) < .03f || fabsf(lift) > 1.f) lift = 0.f;
  lift -= skinSole(m);   // №762: подошва позы привязки — на землю
  if (nFootCache < 96) { footCache[nFootCache].m = m; footCache[nFootCache].clip = clip; footCache[nFootCache].lift = lift; nFootCache++; }
  return lift;
}

void skinPlace(ScePspFMatrix4 *w, float x, float y, float z, float heading) {
  float c = cosf(heading), s = sinf(heading);
  memset(w, 0, sizeof *w);
  w->x.x = c; w->x.z = -s; w->y.y = 1.f; w->z.x = s; w->z.z = c;       // +Z (model forward) -> (sin h, 0, cos h)
  w->w.x = x; w->w.y = y; w->w.z = z; w->w.w = 1.f;
}

void skinPlaceTilt(ScePspFMatrix4 *w, float x, float y, float z, float heading, float pitch, float pivotY) {
  float c = cosf(heading), s = sinf(heading), cp = cosf(pitch), sp = sinf(pitch);
  memset(w, 0, sizeof *w);                                    // Ry(heading) * [точка поворота] Rx(pitch)
  w->x.x = c;       w->x.z = -s;
  w->y.x = sp * s;  w->y.y = cp;  w->y.z = sp * c;            // +Y модели (голова) -> к курсу
  w->z.x = cp * s;  w->z.y = -sp; w->z.z = cp * c;            // +Z модели (лицо) -> вниз
  w->w.x = x - pivotY * sp * s; w->w.y = y + pivotY * (1.f - cp); w->w.z = z - pivotY * sp * c; w->w.w = 1.f;
}

int skinToon;
int skinToonEyes = !TOON_EYES_OLD;
unsigned skinToonDraws, skinToonVerts, skinToonBytesNow, skinToonHullCalls, skinToonSeamFixed;   // №901: skinToonSeamFixed — вершин строчек, слитых с цветом соседей
void skinSetToon(int on) { skinToon = SHAR_TOON_AVAILABLE ? !!on : 0; }

// ---------------------------------------------------------------- drawing
#ifndef SKIN_HOST
static ScePspFVector3 sunDir = {SKIN_SUN_DIR_X, SKIN_SUN_DIR_Y, SKIN_SUN_DIR_Z};
void skinSunDirXZ(float *x, float *z);
static unsigned sunCol = SKIN_SUN_ABGR, ambCol = SKIN_AMB_ABGR;   // свет уровня 1; уровень N — skinLight из LGT1 (main.c)

// 16.10 (№78, Роб: «Гомер сливается с дорогой»): контраст света персонажей чуть выше, форма света не меняется — солнце ярче, фон темнее (проценты; 100/100 — как было).
// Свет Гомера на улице: солнце 0x90 + фон 0x70 дают почти плоскую заливку (сумма 256). flag homerlit 1 = SKIN_LIT_SUN_PCT / SKIN_LIT_AMB_PCT; skinsun / skinamb — отдельно. Цена нулевая (те же команды GE)
int skinSunPct = 100, skinAmbPct = 100;
static unsigned skinScaleCol(unsigned c, int pct) {
  if (pct == 100) return c;
  unsigned r = (c & 255) * pct / 100, g = ((c >> 8) & 255) * pct / 100, b = ((c >> 16) & 255) * pct / 100;
  return (c & 0xff000000u) | ((b > 255 ? 255 : b) << 16) | ((g > 255 ? 255 : g) << 8) | (r > 255 ? 255 : r);
}
void skinLight(float dx, float dy, float dz, unsigned sunAbgr, unsigned ambientAbgr) {
  float l = sqrtf(dx * dx + dy * dy + dz * dz); if (l <= 0.f) l = 1.f;
  sunDir.x = dx / l; sunDir.y = dy / l; sunDir.z = dz / l; sunCol = sunAbgr; ambCol = ambientAbgr;
}
void skinSunDirXZ(float *x, float *z) { *x = sunDir.x; *z = sunDir.z; }   // живое направление НА солнце (то, что задал уровень)
void skinLightLevel(const float ambS[3], const float dirS[3], unsigned *sunAbgr, unsigned *ambAbgr) {
  sunDir.x = SKIN_SUN_DIR_X; sunDir.y = SKIN_SUN_DIR_Y; sunDir.z = SKIN_SUN_DIR_Z;   // как до 29.09 (без нормировки)
  sunCol = dirS ? levLightScale(SKIN_SUN_ABGR, dirS) : SKIN_SUN_ABGR;
  ambCol = ambS ? levLightScale(SKIN_AMB_ABGR, ambS) : SKIN_AMB_ABGR;
  if (sunAbgr) *sunAbgr = sunCol;
  if (ambAbgr) *ambAbgr = ambCol;
}
static int nLit; static ScePspFVector3 litDir[4]; static unsigned litCol[4], litAmb;   // skinLights (комната); nLit = 0 — солнце
void skinLights(int n, const float dir[][3], const unsigned *abgr, unsigned ambientAbgr) {
  nLit = n < 0 ? 0 : n > 4 ? 4 : n; litAmb = ambientAbgr;
  for (int i = 0; i < nLit; i++) {
    float l = sqrtf(dir[i][0] * dir[i][0] + dir[i][1] * dir[i][1] + dir[i][2] * dir[i][2]); if (l <= 0.f) l = 1.f;
    litDir[i].x = dir[i][0] / l; litDir[i].y = dir[i][1] / l; litDir[i].z = dir[i][2] / l; litCol[i] = abgr[i];
  }
}

unsigned gateSkinDraws, gateSkinBad;   // гейт «КАДР ИГРОКА»: пачек скелетных моделей (прохожие, NPC, Гомер) ушло в GE / из них с битой текстурой (индекс вне таблицы, нулевой размер)
static void bindTex(const SkinModel *m, int t) {
  const SkinTex *x = &m->tex[t];
  sceGuClutLoad(32, m->base + x->offClut);                         // 256 x 8888
  sceGuTexMode(GU_PSM_T8, 0, 0, x->swizzled ? 1 : 0);
  sceGuTexImage(0, x->w, x->h, x->w, m->base + x->offPix);          // also flushes the texture cache
}

// Правка Роба 428 «мультяшный режим» (РАСШИРЕНИЕ ПОРТА, не оригинал; выкл. по умолчанию): контур-оболочка персонажа и ровная
// заливка без света. Оболочка = копия вершин пачки, сдвинутых по своей нормали на SKIN_TOON_W, рисуется вторым проходом той же
// пачки (те же кости) чёрным, без текстуры и света, ПЕРЕД пачкой и без записи глубины: пачка ложится поверх, остаётся ободок
// за силуэтом и по краям частей (рука на фоне тела). Отсечение граней не нужно — обход полос в данных не проверен.
// Копии — в кэше по модели (строятся при первом рисовании, предел SKIN_TOON_BYTES); skinToonFlush — при смене уровня.
#ifndef SKIN_HOST
#define SKIN_TOON_W .025f          // правило порта (вкус): 2,5 см ≈ 1 пиксель PSP на 5,7 м (272 строки, FOVY 62°)
// №754 (Роб 17.10: «и на остальных персонажах тоже контур, просто внешний, на беглую руку»): оболочка 2,5 см с камеры дальше 6 м тоньше пикселя — у прохожих
// контура не видно. Теперь три готовые толщины по расстоянию до камеры (план КОНТУР_МУЛЬТИКА_ПЛАН, этап 2, запасной путь): 2,5 / 6 / 14 см до 7 / 16 / 35 м —
// на экране ~1–2,5 px; дальше 35 м без контура; глаза обводятся только вблизи. Кэш строит толщину при первой нужде. SKIN_TOON_LEVELS_OLD=1 — одна толщина, как TEST32
#ifndef SKIN_TOON_LEVELS_OLD
#define SKIN_TOON_LEVELS_OLD 0
#endif
// №779 (Роб 18.10: «вдалеке у них самая толстая линия — неправильно, усреднить, как в сериале 90-х; толщину — ползунком в настройках»; следом: «линия по толщине
// плюс-минус единая, а вдали чтобы толщина и контрастность немножечко описывали перспективу, глубину; мелким дальним персонажам — не толще»): толщина на экране
// от ползунка (skinToonLineParams: width 0..10, 5 — по умолчанию ≈ 1,7 px), пять готовых оболочек по дальности (до 4,5 / 8 / 14 / 24 / 40 м, дальше без контура).
// Вытяжка ступени считается по её БЛИЖНЕЙ границе: на любой дали линия не толще цели, а цель с расстоянием чуть тоньше (×1 до 4 м, ×0,78 к 14 м, ×0,56 к 24 м, не меньше ×0,45) —
// у мелких дальних никогда не толще ближних. Контраст — toonAirMix: тушь с расстоянием светлеет к цвету дали (воздушная перспектива). Смена ползунка — кэш заново.
// SKIN_TOON_W779_OLD=1 — толщины №754 (2,5 / 6 / 14 см до 7 / 16 / 35 м), без ползунка и без светления
#ifndef SKIN_TOON_W779_OLD
#define SKIN_TOON_W779_OLD 0
#endif
#define SKIN_TOON_K 5
// №931 (Роб 24.10: «машины, которые уезжают вдаль, остаются с такой же толстой обводкой, и не кажется, что они уезжают; у любых NPC-машин, не у нашей, контур вдали должен становиться тоньше; и у NPC-прохожих тоже»):
// у NPC (трафик, стоящие, машины миссий, прохожие) цель на экране падает с расстоянием: 5 м как у всех (≈1,7 px), 20 м — 0,9 px, до TOON_NPC_N (30 м) — 0,7 px, дальше линии нет (раньше 1,2 px
// держались до 40 м и пропадали разом). Тушь вдали светлеет вдвое быстрее (TOON_NPC_AIR). Машина игрока и герой — как были. У прохожих свои ступени оболочки (SKIN_TOON_K..2K−1), кэш общий.
// TOON_NPC931_OLD=1 — прежнее (одинаковый контур у всех до 40 м)
#ifndef TOON_NPC931_OLD
#define TOON_NPC931_OLD 0
#endif
#ifndef TOON_NPC_N
#define TOON_NPC_N 30.f
#endif
#ifndef TOON_NPC_PX20
#define TOON_NPC_PX20 .9f
#endif
#ifndef TOON_NPC_PXN
#define TOON_NPC_PXN .7f
#endif
#ifndef TOON_NPC_AIR
#define TOON_NPC_AIR 2.f
#endif
#define SKIN_TOON_KN (SKIN_TOON_K * 2)
static int toonNpcOn;
void skinToonNpc(int on) { toonNpcOn = on; }
static const float toonWk754[3] = {SKIN_TOON_W, .06f, .14f}, toonDk754[3] = {7.f, 16.f, 35.f};
static const float toonDk779[SKIN_TOON_K] = {4.5f, 8.f, 14.f, 24.f, 40.f}, toonDlo779[SKIN_TOON_K] = {3.f, 4.5f, 8.f, 14.f, 24.f};   // ступень 0 — по 3 м (камера у героя)
int skinToonLineW = 5, skinToonEyeMode = 1;
int skinInkStyle = INK951_DEF, skinInk951bOld = INK951B_OLD; unsigned skinInkWorldAbgr;   // №951: перо контура (skin.h skinInkAbgr)
// №823 (Роб 18.10: «слишком большой диапазон толщины: у нас не сильно толстая линия, а вдали видна и не сразу уходит в тонкую»): спад цели с
// расстоянием мягче — к 74 м, не тоньше ×0,72 (было к 49 м, ×0,45). TOON_RANGE823_OLD=1 — как TEST35
static float toonPxAt(float d) { float g = 1.f - (d - 4.f) / (TOON_RANGE823_OLD ? 45.f : 70.f), gmin = TOON_RANGE823_OLD ? .45f : .72f; g = g > 1.f ? 1.f : g < gmin ? gmin : g; return (.32f * (float)skinToonLineW + .1f) * g; }   // цель, px на экране
static float toonPxNpc(float d) {   // №931: цель NPC, px на экране: 5 м — как toonPxAt, 20 м — TOON_NPC_PX20, TOON_NPC_N — TOON_NPC_PXN, линейно между
  float p5 = toonPxAt(5.f);
  if (d <= 5.f) return toonPxAt(d);
  if (d <= 20.f) return p5 + (TOON_NPC_PX20 - p5) * (d - 5.f) / 15.f;
  float p = TOON_NPC_PX20 + (TOON_NPC_PXN - TOON_NPC_PX20) * (d - 20.f) / (TOON_NPC_N - 20.f); return p < TOON_NPC_PXN ? TOON_NPC_PXN : p;
}
static float toonWk(int lv) {   // вытяжка оболочки ступени lv, м: пиксели цели на ближней границе ступени × её расстояние / 226,3 (272 строки, FOVY 62°)
  if (SKIN_TOON_W779_OLD) return toonWk754[lv < 3 ? lv : 2];
  if (lv >= SKIN_TOON_K) { int k_ = lv - SKIN_TOON_K; return toonPxNpc(toonDlo779[k_]) * toonDlo779[k_] / 226.3f; }   // №931: ступени NPC
  return toonPxAt(toonDlo779[lv]) * toonDlo779[lv] / 226.3f;
}
// №779: воздушная перспектива контура — доля цвета дали в туши: 0 до 5 м, к 40 м — 0,7. Цвет дали — skinToonAir (main_camera.h skyFogColor), пока нет — сиренево-серый
static unsigned toonAirABGR = 0xffc8b0b0u;
void skinToonAir(unsigned abgr) { toonAirABGR = 0xff000000u | (abgr & 0xffffffu); }
// №796 (Роб: «темнее и контрастнее; у машин обводка очень светлая»): светление к цвету дали с 8 м и не больше 0,45 к 40 м (было с 5 м до 0,7). TOON_AIR796_OLD=1 — как TEST34
static unsigned toonAirMix(unsigned ink, float d) {
  float d0 = TOON_AIR796_OLD ? 5.f : TOON_INK821_OLD ? 8.f : 12.f, kmax = TOON_AIR796_OLD ? .7f : TOON_INK821_OLD ? .45f : .25f;   // №821: темнее и вдали
  if (SKIN_TOON_W779_OLD || d <= d0) return ink;
  float k = (d - d0) / (40.f - d0); k = (k > 1.f ? 1.f : k) * kmax;
  unsigned o = 0xff000000u;
  for (int c = 0; c < 24; c += 8) { float a = (float)((ink >> c) & 255u), b = (float)((toonAirABGR >> c) & 255u); o |= (unsigned)(a + (b - a) * k + .5f) << c; }
  return o;
}
unsigned skinToonAirMix(unsigned ink, float d) { return toonAirMix(ink, d); }
unsigned skinToonAirColor(void) { return toonAirABGR; }   // №779: цвет дали — туман линий мира
// №779 (дирижёр 18.10, слово Роба: «машинам NPC — трафик, стоящие, машины миссий — тоже контур, как у машины игрока и героев»): вытяжка контура машины на расстоянии d, м —
// та же цель на экране, что у героев (toonPxAt), но без ступеней: машина раздувается матрицей (main_render.h carToonHull), кэша нет. 0 — без контура (ползунок 0 или дальше 40 м)
float skinToonWorldW(float d) {
  if (SKIN_TOON_W779_OLD) return .025f;
  if (skinToonLineW <= 0 || d > toonDk779[SKIN_TOON_K - 1]) return 0.f;
  float dmin_ = TOON_RANGE823_OLD ? 3.f : 2.f, dd = d < dmin_ ? dmin_ : d; return toonPxAt(dd) * dd / 226.3f;   // №823: вблизи (до 2 м) не толще цели
}   // №779: то же для линий мира (main_levels.h toonLinesDraw)
float skinToonWorldWNpc(float d) {   // №931: вытяжка контура NPC-машины на расстоянии d, м; 0 — без контура (дальше TOON_NPC_N, ползунок 0)
  if (SKIN_TOON_W779_OLD || TOON_NPC931_OLD) return skinToonWorldW(d);
  if (skinToonLineW <= 0 || d > TOON_NPC_N) return 0.f;
  float dd = d < 2.f ? 2.f : d; return toonPxNpc(dd) * dd / 226.3f;
}
unsigned skinToonAirMixNpc(unsigned ink, float d) { return (SKIN_TOON_W779_OLD || TOON_NPC931_OLD) ? toonAirMix(ink, d) : toonAirMix(ink, d * TOON_NPC_AIR); }   // №931: тушь NPC вдали светлее
void skinToonLineParams(int width, int eyes) {   // настройки (main_save.h cfgApply): толщина контура и глаза
  width = width < 0 ? 0 : width > 10 ? 10 : width; eyes = eyes < 0 ? 0 : eyes > 2 ? 2 : eyes;
  if (width != skinToonLineW && !SKIN_TOON_W779_OLD) { skinToonLineW = width; skinToonFlush(); }
  skinToonEyeMode = eyes; skinToonEyes = eyes != 0;
}
static float toonEye[3]; static int toonEyeOk;
void skinToonEye(float x, float y, float z) { toonEye[0] = x; toonEye[1] = y; toonEye[2] = z; toonEyeOk = 1; }
static int toonLevel(const ScePspFMatrix4 *w, float *dist) {   // ступень оболочки по расстоянию от глаза до модели, -1 — без контура; dist — расстояние, м (0 — неизвестно)
  *dist = 0.f;
  if (SKIN_TOON_LEVELS_OLD || !toonEyeOk || !w) return 0;
  float dx = w->w.x - toonEye[0], dy = w->w.y - toonEye[1], dz = w->w.z - toonEye[2], d2 = dx * dx + dy * dy + dz * dz;
  *dist = sqrtf(d2);
  if (toonNpcOn && !TOON_NPC931_OLD && !SKIN_TOON_W779_OLD) { static const float dkN[SKIN_TOON_K] = {4.5f, 8.f, 14.f, 24.f, TOON_NPC_N}; for (int k = 0; k < SKIN_TOON_K; k++) if (d2 < dkN[k] * dkN[k]) return SKIN_TOON_K + k; return -1; }   // №931: ступени NPC, дальше TOON_NPC_N линии нет
  int nk = SKIN_TOON_W779_OLD ? 3 : SKIN_TOON_K; const float *dk = SKIN_TOON_W779_OLD ? toonDk754 : toonDk779;
  for (int k = 0; k < nk; k++) if (d2 < dk[k] * dk[k]) return k;
  return -1;
}
#define SKIN_TOON_BYTES (SKIN_TOON_LEVELS_OLD ? 1u << 20 : SKIN_TOON_W779_OLD ? 2u << 20 : (5u << 20) / 2) // правило порта: кэш оболочек не больше 1 МБ кучи (№754: 2 МБ на три толщины; №779: 2,5 МБ на пять)
#define SKIN_TOON_N 24
#ifndef SKIN_TOON_V2_OLD
#define SKIN_TOON_V2_OLD 0   // №428: 1 — оболочка как 09.10: копия nverts вершин и рисование без индексов (на CHR1 v2 читает за буфером и рисует не те вершины)
#endif
static inline unsigned toonStored(const SkinBatch *b) { return SKIN_TOON_V2_OLD ? b->nverts : skinBatchStoredVerts(b); }
static struct { const unsigned char *base; unsigned size, verts; unsigned char *hull[SKIN_TOON_KN]; unsigned bytes[SKIN_TOON_KN]; unsigned short tried; signed char *sn; unsigned snBytes; unsigned char seams; } toonC[SKIN_TOON_N];   // №901: sn — общие нормали вытяжки (4 байта на вершину)
static int vfmtOffsets(unsigned vt, unsigned stride, int *nOff, int *pOff) {   // смещения нормали (8 бит) и позиции (float) в вершине GE
  static const int wsz[4] = {0, 1, 2, 4}, tsz[4] = {0, 1, 2, 4}, csz[8] = {0, 0, 0, 0, 2, 2, 2, 4}, nsz[4] = {0, 1, 2, 4}, psz[4] = {0, 1, 2, 4};
  int wf = (vt >> 9) & 3, nw = wf ? (int)((vt >> 14) & 7) + 1 : 0, tf = vt & 3, cf = (vt >> 2) & 7, nf = (vt >> 5) & 3, pf = (vt >> 7) & 3;
  if (nf != 1 || pf != 3 || (vt & (7u << 18))) return 0;   // нужны нормаль 8 бит и позиция float, без морфа
  int o = 0, al = 1;
  if (wf) { o += nw * wsz[wf]; if (wsz[wf] > al) al = wsz[wf]; }
  if (tf) { o = (o + tsz[tf] - 1) & ~(tsz[tf] - 1); o += 2 * tsz[tf]; if (tsz[tf] > al) al = tsz[tf]; }
  if (cf) { o = (o + csz[cf] - 1) & ~(csz[cf] - 1); o += csz[cf]; if (csz[cf] > al) al = csz[cf]; }
  *nOff = o; o += 3 * nsz[nf];
  o = (o + 3) & ~3; *pOff = o; o += 3 * psz[pf]; if (4 > al) al = 4;
  o = (o + al - 1) & ~(al - 1);
  return (unsigned)o == stride;
}
void skinToonFlush(void) { for (int i = 0; i < SKIN_TOON_N; i++) { for (int k = 0; k < SKIN_TOON_KN; k++) free(toonC[i].hull[k]); free(toonC[i].sn); memset(&toonC[i], 0, sizeof toonC[i]); } skinToonBytesNow = 0; }
// №901 (Роб 23.10: «Контур: очень важно доработать»; гомер на бегу — у поднятой ноги обводки нет, на стыках рубашка/штаны и у ботинок линия рвётся): оболочка раздувалась
// по нормали САМОЙ вершины. В модели у острых рёбер (ботинок, подошва, отворот) и на стыках пачек (рубашка/штаны, нога/ботинок) у одной точки несколько вершин с разными
// нормалями: каждая уезжала в свою сторону, и между кусками оболочки оставалась щель — на силуэте там нет линии. Теперь вершины с одной позицией (по всей модели, все пачки)
// получают общее направление — сумму нормалей; на остром ребре вытяжка растёт на 1/cos до TOON_SMOOTH_K, чтобы грани не стали тоньше. Если нормали почти противоположны
// (двойной лист: шип, веко) — своя нормаль, как раньше. Считается один раз на модель (хэш по позиции, O(n)). TOON_SMOOTH901_OLD=1 — как TEST36
#ifndef TOON_SMOOTH901_OLD
#define TOON_SMOOTH901_OLD 0
#endif
#define TOON_SMOOTH_K 1.6f
static int toonKey(float v) { return (int)floorf(v * 2000.f + .5f); }   // позиция с точностью 0,5 мм
static signed char *toonSmooth(const SkinModel *m, unsigned *outBytes) {   // 4 байта на хранимую вершину пачки (по порядку пачек): nx, ny, nz (×127), k ((k-1)/(K-1)×127); 0 — нет памяти
  unsigned total = 0; for (unsigned i = 0; i < m->h->nBatches; i++) total += toonStored(&m->b[i]);
  if (!total) return 0;
  unsigned cap = 16; while (cap < total * 2u) cap <<= 1;
  signed char *sn = calloc(total, 4u); int *tab = malloc((size_t)cap * sizeof(int)), *grp = malloc((size_t)total * sizeof(int));
  float *un = malloc((size_t)total * 3u * sizeof(float)), *ps = malloc((size_t)total * 3u * sizeof(float)), *gs = calloc((size_t)total * 5u, sizeof(float));   // un — единичные нормали, ps — позиции, gs — на группу: сумма xyz, число, наименьший косинус
  if (!sn || !tab || !grp || !un || !ps || !gs) { free(sn); free(tab); free(grp); free(un); free(ps); free(gs); return 0; }
  memset(tab, 0xff, (size_t)cap * sizeof(int));
  unsigned vi = 0, ng = 0;
  for (unsigned i = 0; i < m->h->nBatches; i++) {
    const SkinBatch *b = &m->b[i]; int no, po; unsigned sv = toonStored(b); int ok = !(b->flags & 2) && vfmtOffsets(b->vtype, b->stride, &no, &po);
    for (unsigned v = 0; v < sv; v++, vi++) {
      grp[vi] = -1;
      if (!ok) continue;
      const unsigned char *q = m->base + b->offVerts + v * b->stride; const signed char *nn = (const signed char *)(q + no);
      float nx = nn[0], ny = nn[1], nz = nn[2], l = sqrtf(nx * nx + ny * ny + nz * nz); if (l < 1e-3f) continue;   // нулевая нормаль — не раздуваем (как прежде)
      un[vi * 3] = nx / l; un[vi * 3 + 1] = ny / l; un[vi * 3 + 2] = nz / l; memcpy(ps + vi * 3, q + po, 12);
      int kx = toonKey(ps[vi * 3]), ky = toonKey(ps[vi * 3 + 1]), kz = toonKey(ps[vi * 3 + 2]);
      unsigned h = ((unsigned)kx * 73856093u) ^ ((unsigned)ky * 19349663u) ^ ((unsigned)kz * 83492791u); h ^= h >> 15; h &= cap - 1u;
      for (;; h = (h + 1u) & (cap - 1u)) {
        int r = tab[h];
        if (r < 0) { tab[h] = (int)vi; grp[vi] = (int)ng++; break; }
        if (toonKey(ps[(unsigned)r * 3]) == kx && toonKey(ps[(unsigned)r * 3 + 1]) == ky && toonKey(ps[(unsigned)r * 3 + 2]) == kz) { grp[vi] = grp[r]; break; }
      }
    }
  }
  for (unsigned v = 0; v < total; v++) if (grp[v] >= 0) { float *g = gs + (unsigned)grp[v] * 5u; g[0] += un[v * 3]; g[1] += un[v * 3 + 1]; g[2] += un[v * 3 + 2]; g[3] += 1.f; g[4] = 1.f; }
  for (unsigned g_ = 0; g_ < ng; g_++) { float *g = gs + g_ * 5u, l = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    if (g[3] > 1.5f && l > .35f * g[3]) { g[0] /= l; g[1] /= l; g[2] /= l; } else { g[3] = 0.f; } }   // g[3] = 0 — у группы нет общей стороны (двойной лист): каждая вершина по своей нормали
  for (unsigned v = 0; v < total; v++) if (grp[v] >= 0) { float *g = gs + (unsigned)grp[v] * 5u;
    if (g[3] > 0.f) { float cs = g[0] * un[v * 3] + g[1] * un[v * 3 + 1] + g[2] * un[v * 3 + 2]; if (cs < g[4]) g[4] = cs; } }
  for (unsigned v = 0; v < total; v++) {
    if (grp[v] < 0) continue;
    const float *g = gs + (unsigned)grp[v] * 5u, *d = g[3] > 0.f ? g : un + v * 3; float k = 1.f;
    if (g[3] > 0.f) { float c = g[4] < 1.f / TOON_SMOOTH_K ? 1.f / TOON_SMOOTH_K : g[4]; k = 1.f / c; }   // острое ребро: вытяжка 1/cos, чтобы грани не стали тоньше
    signed char *o = sn + (size_t)v * 4u;
    for (int c = 0; c < 3; c++) o[c] = (signed char)(int)floorf(d[c] * 127.f + (d[c] < 0.f ? -.5f : .5f));
    o[3] = (signed char)(int)((k - 1.f) / (TOON_SMOOTH_K - 1.f) * 127.f + .5f);
  }
  free(tab); free(grp); free(un); free(ps); free(gs);
  if (outBytes) *outBytes = total * 4u;
  return sn;
}
// №901 (Роб 23.10: «внутри однотонного линий нет»; Гомер: короткие тёмные штрихи на отворотах у щиколоток, тёмная черта у воротника): это не оболочка, а сама модель. Персонажи-«образцы»
// (атлас char_swatches_lit: цвет вершины = клетка 8×8 текселей) держат тонкие кольца-строчки (ширина 1–3 см) цвета сланца (66,74,82 / 49,49,58): на ногах Гомера их 20 вершин в каждой из двух пачек,
// у воротника 14; они видны и при выключенном мультике. Чёрные (глаза, рот, брови) и цветные куски не трогаем. В мультике вершина строчки берёт клетку образца ближайшей по позиции вершины
// другого цвета той же пачки — строчка сливается с отворотом/рубашкой. Один раз на модель, правка прямо в загруженном буфере модели (в мультике она остаётся до перезагрузки модели).
// TOON_SEAM901_OLD=1 — как TEST36
// №901, доводка (бледные точки у щиколотки на бегу): кольцо строчки целиком берёт одну клетку. TOON_SEAM_RING901_OLD=1 — по вершине, как TEST54 (2 вершины на кольцо оставались тёмными)
#ifndef TOON_SEAM901_OLD
#define TOON_SEAM901_OLD 0
#endif
#ifndef TOON_SEAM_RING901_OLD
#define TOON_SEAM_RING901_OLD 0
#endif
#define TOON_SEAM_THIN .035f   // строчка тоньше 3,5 см по одной из осей, длиннее 8 см по другой
static int toonSwatchRGB(const SkinModel *m, int t, unsigned u16, unsigned v16, int *rgb) {   // цвет клетки образца: u, v — сырые 16 бит вершины (u/256 — доля ширины); 0 — не образец или не T8
  if (t < 0 || t >= (int)m->h->nTex) return 0;
  const SkinTex *T = &m->tex[t]; if (!strstr(T->name, "swatch") || T->psm != 5 || !T->swizzled || T->w < 16 || T->h < 8) return 0;
  int x = (int)((unsigned)((float)u16 / 256.f * (float)T->w)), y = (int)((unsigned)((float)v16 / 256.f * (float)T->h)); if (x >= (int)T->w) x = (int)T->w - 1; if (y >= (int)T->h) y = (int)T->h - 1;
  unsigned idx = m->base[T->offPix + ((unsigned)(y >> 3) * (T->w >> 4) + (unsigned)(x >> 4)) * 128u + (unsigned)(y & 7) * 16u + (unsigned)(x & 15)];
  const unsigned char *c = m->base + T->offClut + idx * 4u; rgb[0] = c[0]; rgb[1] = c[1]; rgb[2] = c[2]; return 1;
}
static int toonSlate(const int *c) { int mx = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]), mn = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]); return mx >= 45 && mx <= 120 && mx - mn <= 36; }   // тёмный сланец: не чёрный (глаза), не цветной
static int toonSeams(SkinModel *m) {   // число исправленных вершин
  int fixed = 0; unsigned char *base = (unsigned char *)m->base;
  for (unsigned bi = 0; bi < m->h->nBatches; bi++) {
    const SkinBatch *b = &m->b[bi]; int no, po; unsigned sv = toonStored(b);
    if ((b->flags & 3) || !vfmtOffsets(b->vtype, b->stride, &no, &po) || (b->vtype & 3) != 2) continue;   // без глаз и вырезов; координаты текстуры u16
    static const int wsz[4] = {0, 1, 2, 4}; int wf = (int)((b->vtype >> 9) & 3), nw = wf ? (int)((b->vtype >> 14) & 7) + 1 : 0, to = (nw * wsz[wf] + 1) & ~1;   // смещение u, v в вершине: после весов, по 2 байта
    if (to + 4 > (int)b->stride) continue;
    unsigned char *vb = base + b->offVerts; int *key = malloc((size_t)sv * sizeof(int)), cs[3]; if (!key) continue;
    for (unsigned v = 0; v < sv; v++) { unsigned short uv_[2]; memcpy(uv_, vb + v * b->stride + to, 4); key[v] = toonSwatchRGB(m, b->tex, uv_[0], uv_[1], cs) ? (toonSlate(cs) ? 1 : (cs[0] + cs[1] + cs[2] < 90 ? 3 : 2)) : 0; }   // 1 сланец, 2 цвет, 3 чёрный (глаза, рот, брови — оставляем)
    for (unsigned v = 0; v < sv; v++) {
      if (key[v] != 1) continue;
      unsigned grp_ = 0; float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f}; unsigned short uvv[2]; memcpy(uvv, vb + v * b->stride + to, 4);
      for (unsigned w = 0; w < sv; w++) if (key[w] == 1) { unsigned short uw[2]; memcpy(uw, vb + w * b->stride + to, 4); if (uw[0] != uvv[0] || uw[1] != uvv[1]) continue;
        float p[3]; memcpy(p, vb + w * b->stride + po, 12); grp_++; for (int k = 0; k < 3; k++) { if (p[k] < lo[k]) lo[k] = p[k]; if (p[k] > hi[k]) hi[k] = p[k]; } }
      float e0 = hi[0] - lo[0], e1 = hi[1] - lo[1], e2 = hi[2] - lo[2], mn_ = fminf(e0, fminf(e1, e2)), mx_ = fmaxf(e0, fmaxf(e1, e2));
      if (grp_ > 60 || mn_ >= TOON_SEAM_THIN || mx_ < .08f) continue;   // широкий кусок цвета — это одежда/обувь, не строчка
      if (hi[1] > m->h->bbox[1] + .72f * (m->h->bbox[4] - m->h->bbox[1])) continue;   // верхняя четверть роста — лицо: рот, брови, подбородок оставляем
      if (TOON_SEAM_RING901_OLD) {   // как TEST54: клетка у каждой вершины своя — ближайшего соседа другого цвета в 5 см, чёрное рядом (в 5 см) — не трогаем; группа пересчитывается после каждой вершины
        float pv[3]; memcpy(pv, vb + v * b->stride + po, 12); float best_ = 1e9f, blk_ = 1e9f; int bw_ = -1;
        for (unsigned w = 0; w < sv; w++) if (key[w] == 3) { float pw[3]; memcpy(pw, vb + w * b->stride + po, 12); float dx = pw[0] - pv[0], dy = pw[1] - pv[1], dz = pw[2] - pv[2], d2 = dx * dx + dy * dy + dz * dz; if (d2 < blk_) blk_ = d2; }
        if (blk_ < .05f * .05f) continue;
        for (unsigned w = 0; w < sv; w++) if (key[w] == 2) { float pw[3]; memcpy(pw, vb + w * b->stride + po, 12); float dx = pw[0] - pv[0], dy = pw[1] - pv[1], dz = pw[2] - pv[2], d2 = dx * dx + dy * dy + dz * dz; if (d2 < best_) { best_ = d2; bw_ = (int)w; } }
        if (bw_ < 0 || best_ > .05f * .05f) continue;
        memcpy(vb + v * b->stride + to, vb + (unsigned)bw_ * b->stride + to, 4); fixed++; continue;
      }
      // №901, доводка: строчка берёт ОДНУ клетку на всё кольцо. Прежде кольцо правили по вершине: после первых исправленных в группе «одна клетка» оставалось меньше длины 8 см, условие строчки переставало
      // выполняться, и по две вершины на кольцо (Гомер: 2 из 12 в каждой из двух пачек) оставались тёмными; треугольник между исправленной и тёмной вершиной тянет текстурную координату через соседние клетки атласа — бледные точки (белая/жёлтая клетка) у щиколотки на бегу
      float best = 1e9f; int bw = -1, dark_ = 0;
      for (unsigned v2 = v; v2 < sv; v2++) { if (key[v2] != 1) continue; unsigned short u2[2]; memcpy(u2, vb + v2 * b->stride + to, 4); if (u2[0] != uvv[0] || u2[1] != uvv[1]) continue;
        float pv[3]; memcpy(pv, vb + v2 * b->stride + po, 12);
        for (unsigned w = 0; w < sv; w++) if (key[w] >= 2) { float pw[3]; memcpy(pw, vb + w * b->stride + po, 12); float dx = pw[0] - pv[0], dy = pw[1] - pv[1], dz = pw[2] - pv[2], d2 = dx * dx + dy * dy + dz * dz;
          if (key[w] == 3) { if (d2 < .05f * .05f) dark_ = 1; } else if (d2 < best) { best = d2; bw = (int)w; } } }
      if (dark_ || bw < 0 || best > .05f * .05f) continue;   // рядом чёрное (рот, глаз, бровь) — черта лица; соседа другого цвета нет ближе 5 см — не трогаем
      for (unsigned v2 = v; v2 < sv; v2++) { if (key[v2] != 1) continue; unsigned short u2[2]; memcpy(u2, vb + v2 * b->stride + to, 4); if (u2[0] != uvv[0] || u2[1] != uvv[1]) continue;
        memcpy(vb + v2 * b->stride + to, vb + (unsigned)bw * b->stride + to, 4); key[v2] = 4; fixed++; }
    }
    free(key);
    sceKernelDcacheWritebackRange(vb, sv * b->stride);
  }
  return fixed;
}
static unsigned char *toonHull(const SkinModel *m, int lv) {
  skinToonHullCalls++;
  int fr = -1;
  for (int i = 0; i < SKIN_TOON_N; i++) {
    if (toonC[i].base == m->base && toonC[i].size == m->h->size && toonC[i].verts == m->h->totalVerts) {
      if (toonC[i].hull[lv] || (toonC[i].tried >> lv & 1)) return toonC[i].hull[lv];
      fr = i; break; }
    if (!toonC[i].base && fr < 0) fr = i;
  }
  if (fr < 0) return 0;
  unsigned bytes = 0, bytes2 = 0; for (unsigned i = 0; i < m->h->nBatches; i++) { unsigned sz_ = (toonStored(&m->b[i]) * m->b[i].stride + 15) & ~15u; bytes += sz_;
    if (!EYE_SIL827_OLD && (m->b[i].flags & 3) == 1) bytes2 += sz_; }   // №827: копия глаз толщиной тела — для внешнего силуэта
  toonC[fr].base = m->base; toonC[fr].size = m->h->size; toonC[fr].verts = m->h->totalVerts; toonC[fr].tried |= (unsigned short)(1u << lv);   // и при отказе: не пробовать каждый кадр
  if (!bytes || skinToonBytesNow + bytes + bytes2 > SKIN_TOON_BYTES) return 0;
  unsigned char *h = memalign(16, bytes + bytes2); if (!h) return 0;
  if (!TOON_SEAM901_OLD && !toonC[fr].seams) { toonC[fr].seams = 1; skinToonSeamFixed += (unsigned)toonSeams((SkinModel *)m); }   // №901: строчки образцов — один раз на модель
  if (!TOON_SMOOTH901_OLD && !toonC[fr].sn) { unsigned sb_ = 0; toonC[fr].sn = toonSmooth(m, &sb_); toonC[fr].snBytes = toonC[fr].sn ? sb_ : 0; skinToonBytesNow += toonC[fr].snBytes; }   // №901: общие нормали вытяжки — один раз на модель
  const signed char *sn_ = TOON_SMOOTH901_OLD ? 0 : toonC[fr].sn;
  unsigned o = 0;
  for (int pass = 0; pass < (bytes2 ? 2 : 1); pass++)   // проход 1 (№827) — копии глаз толщиной тела за основными оболочками
  for (unsigned i = 0; i < m->h->nBatches; i++) {
    const SkinBatch *b = &m->b[i]; int no, po; unsigned sv = toonStored(b), n = sv * b->stride;   // CHR1 v2: в файле хранится pad[1] вершин, не nverts
    if (pass && (b->flags & 3) != 1) continue;
    unsigned sv0_ = 0; for (unsigned j = 0; j < i; j++) sv0_ += toonStored(&m->b[j]);   // №901: номер первой вершины пачки в общих нормалях
    memcpy(h + o, m->base + b->offVerts, n);
    if (vfmtOffsets(b->vtype, b->stride, &no, &po))
      for (unsigned v = 0; v < sv; v++) {
        unsigned char *q = h + o + v * b->stride; const signed char *nn = (const signed char *)(q + no); float p[3];
        float nx = nn[0] / 127.f, ny = nn[1] / 127.f, nz = nn[2] / 127.f, l = sqrtf(nx * nx + ny * ny + nz * nz), kk_ = 1.f;
        if (l < 1e-3f) continue;
        if (sn_) { const signed char *s_ = sn_ + (size_t)(sv0_ + v) * 4u; float sx = s_[0] / 127.f, sy = s_[1] / 127.f, sz = s_[2] / 127.f, sl = sqrtf(sx * sx + sy * sy + sz * sz);   // №901: общее направление группы вершин одной позиции
          if (sl > 1e-3f) { nx = sx; ny = sy; nz = sz; l = sl; kk_ = 1.f + s_[3] / 127.f * (TOON_SMOOTH_K - 1.f); } }
        memcpy(p, q + po, 12); float w_ = ((b->flags & 1) && !pass ? SKIN_TOON_EYE_W : toonWk(lv)) * kk_; p[0] += nx / l * w_; p[1] += ny / l * w_; p[2] += nz / l * w_; memcpy(q + po, p, 12);
      }
    o += (n + 15) & ~15u;
  }
  sceKernelDcacheWritebackRange(h, bytes + bytes2);
  toonC[fr].hull[lv] = h; toonC[fr].bytes[lv] = bytes + bytes2; skinToonBytesNow += bytes + bytes2;
  return h;
}
#else
void skinToonFlush(void) {}
#endif
#ifndef SKIN_HOST
// рампа ступени тени: 32 x 8 texel 8888 (строки одинаковы, V не используется); U < 0,575 (N·L < 0,15) — тень (ABGR: чуть холоднее и темнее), выше — свет (белый)
static unsigned toonRamp[32 * 8] __attribute__((aligned(16))); static int toonRampOk;
static void toonRampInit(void) {
  if (toonRampOk) return;
  for (int y = 0; y < 8; y++) for (int x = 0; x < 32; x++) toonRamp[y * 32 + x] = x < 18 ? 0xffc8b8b8u : 0xffffffffu;
  sceKernelDcacheWritebackRange(toonRamp, sizeof toonRamp); toonRampOk = 1;
}
#endif
#ifndef SKIN_HOST
// №779: диапазон глубины (near, 0) двумя командами GE (масштаб и центр Z) вместо sceGuDepthRange (4 слова: ещё мин/макс Z — их сжатый диапазон не трогает).
// Те же числа, что считает libgu sceGuDepthRange(near, 0) при нулевом sceGuDepthOffset (у персонажей он 0); контекст libgu не меняется — возврат (65535) совпадает с ним
static void skinZRange(int nearZ) { float z = (float)((unsigned)nearZ >> 1); sceGuSendCommandf(68, z - (float)nearZ); sceGuSendCommandf(71, z); }
#endif
unsigned skinWProf, skinW[5], skinNB[2];   // №914 служебно: слова GE по стадиям skinDrawPal (настройка, оболочки, тело, ступень тени, хвост) и число пачек / оболочек
#define SKWM(v_) do { if (skinWProf) v_ = (unsigned)sceGuCheckList(); } while (0)
void skinDrawPal(const SkinModel *m, const ScePspFMatrix4 *pal, const ScePspFMatrix4 *world, int eyeTex) {
  XSK(41, m->h->nBatches);
  unsigned wk0_ = 0, wk1_ = 0, wk2_ = 0, wk3_ = 0, wk4_ = 0; SKWM(wk0_);
  sceGuSetMatrix(GU_MODEL, world);
  sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDepthMask(GU_FALSE);
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f);
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuEnable(GU_LIGHTING);
  if (nLit) {   // LightGroup комнаты: источники 0..nLit-1
    for (int i = 0; i < nLit; i++) {
      sceGuEnable(GU_LIGHT0 + i); sceGuLight(i, GU_DIRECTIONAL, GU_DIFFUSE, &litDir[i]);
      sceGuLightColor(i, GU_DIFFUSE, litCol[i]); sceGuLightColor(i, GU_AMBIENT, 0);
    }
    sceGuAmbient(litAmb);
  } else {
    sceGuEnable(GU_LIGHT0);
    sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &sunDir);
    sceGuLightColor(0, GU_DIFFUSE, skinScaleCol(sunCol, skinSunPct)); sceGuLightColor(0, GU_AMBIENT, 0);
    sceGuAmbient(skinScaleCol(ambCol, skinAmbPct));
  }
  sceGuModelColor(0, 0xffffffff, 0xffffffff, 0); sceGuAmbientColor(0xffffffff);
  int bound = -1, atest = 0; int slotBone[16]; memset(slotBone, 0, sizeof slotBone);   // слот пуст: в GE чужая матрица
  SKWM(wk1_);
#ifndef SKIN_HOST
  float tdist_ = 0.f; int tlv_ = skinToonEnabled() ? toonLevel(world, &tdist_) : -1;   // №754: толщина контура по расстоянию
  unsigned char *hull = tlv_ >= 0 ? toonHull(m, tlv_) : 0; unsigned hullOff = 0;
  // №779/№781 (Роб 18.10, Апу и Лиза: «свитер из отдельных блоков и каждому контур», «шипы и лицо разлинованы — ломано»): контур только по внешнему силуэту.
  // Оболочка тела рисуется со сдвигом глубины назад на SKIN_TOON_SIL_M (диапазон глубины GE сжат: (65535 − δ, 0), δ = 65535 · сдвиг / расстояние — тот же сдвиг
  // в метрах на любой дали): где за кромкой в нескольких сантиметрах своё же тело (соседний блок свитера, шип, лицо), оболочка проигрывает тест глубины и не видна;
  // остаётся кромка на фоне и линия там, где часть выступает вперёд больше сдвига (рука перед кофтой). Кадр 5650 без трафарета — только глубина.
  // SKIN_TOON_SIL_OLD=1 — прежнее (оболочка каждой пачки без сдвига: линии между частями)
#ifndef SKIN_TOON_SIL_OLD
#define SKIN_TOON_SIL_OLD 0
#endif
#ifndef SKIN_TOON_SIL_M
#define SKIN_TOON_SIL_M .12f
#endif
  int silD_ = (!SKIN_TOON_SIL_OLD && tdist_ > .3f) ? (int)fminf(20000.f, SKIN_TOON_SIL_M * 65535.f / tdist_) : 0;
  if (skinToonEnabled()) sceGuDisable(GU_LIGHTING);   // правка 428: ровная заливка — цвет текстуры без света
  // №797: оболочки ВСЕХ пачек тела сначала — без сдвига и без записи глубины, с тестом против уже нарисованного; потом все пачки тела. Тело закрывает оболочку
  // везде внутри своего силуэта (шипы, свитер, лицо — без линий), снаружи остаётся край против всего, что уже в кадре: мир и своя машина (она рисуется раньше героя,
  // main_loop.h). Глаза — прежним мягким кольцом у своей пачки, поверх уже нарисованного лица. SKIN_TOON_PRE_OLD=1 — как TEST34 (skin.h)
  int pre_ = hull && !SKIN_TOON_PRE_OLD && (SKIN_TOON_W779_OLD || skinToonLineW > 0);
  // №822 (Роб 18.10: «контур не замыкается под ногами, на светлом выглядит незавершённым»): низ оболочки под подошвой уходит под пол и
  // проигрывал тест глубины. Оболочки предпрохода — с выигрышем SKIN_TOON_FOOT_M по глубине против мира (диапазон глубины GE растянут
  // к глазу: (65535 + δ, 0), δ = 65535 · сдвиг / расстояние — тот же сдвиг в метрах на любой дали, как SKIN_TOON_SIL_M назад): линия под
  // ступнями видна над полом; тело (рисуется следом) закрывает оболочку внутри силуэта, как прежде. SKIN_TOON_FOOT_OLD=1 — как TEST35
#ifndef SKIN_TOON_FOOT_OLD
#define SKIN_TOON_FOOT_OLD 0
#endif
#ifndef SKIN_TOON_FOOT_M
#define SKIN_TOON_FOOT_M .08f
#endif
  int footD_ = (pre_ && !SKIN_TOON_FOOT_OLD && tdist_ > .3f) ? (int)fminf(8000.f, SKIN_TOON_FOOT_M * 65535.f / tdist_) : 0;
  if (pre_) {
    if (footD_) skinZRange(65535 + footD_);
    unsigned hc_ = TOON_HULL_BLEND_OLD || SKIN_TOON_V675_OLD ? 0xff000000u : toonAirMix(TOON_HULL_ABGR, tdist_ * (toonNpcOn && !TOON_NPC931_OLD ? TOON_NPC_AIR : 1.f));
    sceGuDisable(GU_TEXTURE_2D); sceGuAmbientColor(hc_); sceGuModelColor(0, hc_, hc_, 0); sceGuDepthMask(GU_TRUE);
    if (TOON_HULL_BLEND_OLD && !SKIN_TOON_V675_OLD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0, TOON_HULL_DST); }
    unsigned ho_ = 0, he_ = 0; if (!EYE_SIL827_OLD) for (unsigned i = 0; i < m->h->nBatches; i++) he_ += (toonStored(&m->b[i]) * m->b[i].stride + 15) & ~15u;   // №827: начало копий глаз
    for (unsigned i = 0; i < m->h->nBatches; i++) {
      const SkinBatch *b = &m->b[i];
      int eyeSil_ = !EYE_SIL827_OLD && (b->flags & 3) == 1 && (EYE_SIL838_OLD || (tlv_ >= SKIN_TOON_K ? tlv_ - SKIN_TOON_K : tlv_) <= 1);   // №827: глаз — во внешний силуэт (копия толщиной тела); вырезы по альфе — нет; №838: только ближе 8 м
      if (!(b->flags & 3) || eyeSil_) {   // тело: не глаза (1) и не вырезы по альфе (2); №827 — и глаза
        for (int k = 0; k < b->nb; k++) {
#ifndef SKIN_BONECACHE_OLD
          if (k < 16 && slotBone[k] == b->bones[k] + 1) continue;
          if (k < 16) slotBone[k] = b->bones[k] + 1;
#endif
          sceGuBoneMatrix(k, &pal[b->bones[k]]); XSK(43, 1); }
        const void *hi_ = 0;
#ifndef CHAR_INDEXED_OLD
        if (!SKIN_TOON_V2_OLD && (b->flags & 4)) hi_ = m->base + b->pad[0];   // CHR1 v2: оболочка теми же индексами, что и пачка
#endif
        sceGuDrawArray(b->prim, b->vtype, b->nverts, hi_, hull + (eyeSil_ ? he_ : ho_)); XSK(44, 1); XSK(42, b->nverts); skinToonDraws++; skinToonVerts += b->nverts;
        if (eyeSil_) he_ += (toonStored(b) * b->stride + 15) & ~15u;
      }
      ho_ += (toonStored(b) * b->stride + 15) & ~15u;
    }
    if (TOON_HULL_BLEND_OLD && !SKIN_TOON_V675_OLD) sceGuDisable(GU_BLEND);
    if (footD_) skinZRange(65535);   // №822: диапазон глубины обратно
    sceGuDepthMask(GU_FALSE); sceGuEnable(GU_TEXTURE_2D); sceGuAmbientColor(0xffffffff); sceGuModelColor(0, 0xffffffff, 0xffffffff, 0);
  }
#endif
  SKWM(wk2_);
  for (unsigned i = 0; i < m->h->nBatches; i++) {
    const SkinBatch *b = &m->b[i];
    int t = (eyeTex >= 0 && (b->flags & 1)) ? eyeTex : b->tex;
    if (t != bound) { bindTex(m, t); bound = t; }
    if ((b->flags & 2) != atest) {                 // cut-out textures (necklaces etc.; none on Homer)
      atest = b->flags & 2;
      if (atest) { sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); sceGuEnable(GU_ALPHA_TEST); } else sceGuDisable(GU_ALPHA_TEST);
    }
    // «быстрый расчёт» (npc L2 2,2 тыс. слов на ~6 персонажей; слово списка ≈ 1 мкс): матрица кости — 13 слов; пачки одного персонажа часто делят
    // кости, и GE хранит слот до перезаписи — не пишем ту же матрицу в тот же слот повторно в пределах вызова. -DSKIN_BONECACHE_OLD=1 — прежнее
    for (int k = 0; k < b->nb; k++) {
#ifndef SKIN_BONECACHE_OLD
      if (k < 16 && slotBone[k] == b->bones[k] + 1) continue;
      if (k < 16) slotBone[k] = b->bones[k] + 1;
#endif
      sceGuBoneMatrix(k, &pal[b->bones[k]]); XSK(45, 1); }
#ifndef SKIN_HOST
    const void *hullIdx = 0;
#ifndef CHAR_INDEXED_OLD
    if (!SKIN_TOON_V2_OLD && (b->flags & 4)) hullIdx = m->base + b->pad[0];   // CHR1 v2: оболочка рисуется теми же индексами, что и пачка
#endif
    int eye_ = (b->flags & 1) != 0, bodyLine_ = SKIN_TOON_W779_OLD || skinToonLineW > 0;   // №779: ползунок 0 — тело без контура (глаза — своей строкой)
    if (hull && !(b->flags & 2) && (eye_ || bodyLine_) && (SKIN_TOON_V675_OLD || skinToonEyes || !eye_) && !((tlv_ >= SKIN_TOON_K ? tlv_ - SKIN_TOON_K : tlv_) > 0 && eye_) && !(pre_ && !eye_)) {   // №797: тело — уже в первом проходе   // №754: глаза — только вблизи   // №679: глаза (flags 1) — тонкий контур (skinToonEyes), без него — как №675. правка 428: оболочка-контур ДО пачки, без записи глубины: пачка ляжет поверх, снаружи силуэта
      // №675: контур ровным цветом TOON_HULL_ABGR (0x4d4d4d = 30 % яркости, «70 % чёрного»), без смешивания: прежнее (фон × 30 %) на траве и небе читалось чистым чёрным. TOON_HULL_BLEND_OLD=1 — как c535cd20
      unsigned hc_ = TOON_HULL_BLEND_OLD || SKIN_TOON_V675_OLD ? 0xff000000u : toonAirMix(TOON_HULL_ABGR, tdist_ * (toonNpcOn && !TOON_NPC931_OLD ? TOON_NPC_AIR : 1.f));   // №779: вдали тушь светлеет к цвету дали
      // №781 (Роб: «обводку глазам жёсткую не надо — мягкий общий контур, не такой контрастный»): глаза мягким кольцом — тушь на TOON_EYE_SOFT_PCT % поверх лица
      // (eyes 1, по умолчанию), полной тушью — eyes 2, без кольца — eyes 0. TOON_EYE_SOFT_OLD=1 — глаза как тело (полная тушь)
      int soft_ = eye_ && !TOON_EYE_SOFT_OLD && skinToonEyeMode == 1, sil_ = !eye_ && silD_ > 0;
      sceGuDisable(GU_TEXTURE_2D); sceGuAmbientColor(hc_); sceGuModelColor(0, hc_, hc_, 0); sceGuDepthMask(GU_TRUE);
      if (TOON_HULL_BLEND_OLD && !SKIN_TOON_V675_OLD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0, TOON_HULL_DST); }
      else if (soft_) { unsigned a_ = TOON_EYE_SOFT_PCT * 255u / 100u; sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, a_ * 0x010101u, (255u - a_) * 0x010101u); }
      if (sil_) skinZRange(65535 - silD_);   // №779: оболочка на SKIN_TOON_SIL_M дальше — видна только кромка силуэта
      sceGuDrawArray(b->prim, b->vtype, b->nverts, hullIdx, hull + hullOff); XSK(46, 1);   // останется ободок (вырезы по альфе — без контура)
      if (sil_) skinZRange(65535);
      if ((TOON_HULL_BLEND_OLD && !SKIN_TOON_V675_OLD) || soft_) sceGuDisable(GU_BLEND);
      sceGuDepthMask(GU_FALSE); sceGuEnable(GU_TEXTURE_2D); sceGuAmbientColor(0xffffffff); sceGuModelColor(0, 0xffffffff, 0xffffffff, 0);
      bound = -1; skinToonDraws++; skinToonVerts += b->nverts;
    }
    if (hull) hullOff += (toonStored(b) * b->stride + 15) & ~15u;
    if (t != bound) { bindTex(m, t); bound = t; }
#endif
    const void *indices=0;
#ifndef CHAR_INDEXED_OLD
    if(b->flags&4) indices=m->base+b->pad[0];
#endif
    gateSkinDraws++; if (t < 0 || t >= (int)m->h->nTex || m->tex[t].w <= 0 || m->tex[t].h <= 0) gateSkinBad++;
    sceGuDrawArray(b->prim, b->vtype, b->nverts, indices, m->base + b->offVerts); XSK(47, 1);
  }
  SKWM(wk3_);
#ifndef SKIN_HOST
  if (skinToonShadeOn()) {   // №675: ступень тени вторым проходом (shade mapping), глаза и вырезы по альфе — без неё
    if (atest) { sceGuDisable(GU_ALPHA_TEST); atest = 0; }
    toonRampInit();
    const ScePspFVector3 *ld = nLit ? &litDir[0] : &sunDir;
    sceGuLight(2, GU_DIRECTIONAL, GU_DIFFUSE, ld); sceGuLight(3, GU_DIRECTIONAL, GU_DIFFUSE, ld);   // слоты 2, 3 не включены: только вектор для shade mapping
    sceGuTexMapMode(GU_ENVIRONMENT_MAP, 2, 3);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 32, 8, 32, toonRamp);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST); sceGuTexWrap(GU_CLAMP, GU_CLAMP); sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuDepthFunc(GU_EQUAL); sceGuDepthMask(GU_TRUE); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_FIX, 0, 0);
    for (unsigned i = 0; i < m->h->nBatches; i++) {
      const SkinBatch *b = &m->b[i]; if (b->flags & 3) continue;
      for (int k = 0; k < b->nb; k++) {
        if (k < 16 && slotBone[k] == b->bones[k] + 1) continue;
        if (k < 16) slotBone[k] = b->bones[k] + 1;
        sceGuBoneMatrix(k, &pal[b->bones[k]]); XSK(48, 1); }
      const void *ix = 0;
#ifndef CHAR_INDEXED_OLD
      if (b->flags & 4) ix = m->base + b->pad[0];
#endif
      sceGuDrawArray(b->prim, b->vtype, b->nverts, ix, m->base + b->offVerts); XSK(49, 1);
    }
    sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuDepthFunc(GU_GEQUAL); sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND);
  }
#endif
  SKWM(wk4_);
  if (atest) sceGuDisable(GU_ALPHA_TEST);
  for (int i = 0; i < (nLit ? nLit : 1); i++) sceGuDisable(GU_LIGHT0 + i);
  sceGuDisable(GU_LIGHTING);
  if (skinWProf) { unsigned we_ = (unsigned)sceGuCheckList(); skinW[0] += (wk1_ - wk0_) / 4u; skinW[1] += (wk2_ - wk1_) / 4u; skinW[2] += (wk3_ - wk2_) / 4u; skinW[3] += (wk4_ - wk3_) / 4u; skinW[4] += (we_ - wk4_) / 4u; skinNB[0] += m->h->nBatches; skinNB[1]++; }
}

// Правка Роба 103 (повтор 01.10): персонажи через skinDraw (фоновые и миссий — msn_view.c, пешеходы — traffic.c, гардероб) —
// только пока в списке GE есть место под всю модель (skinGeRoom ставит main.c: тот же предел GE_HARD бюджета, что у кусков города).
// Раньше их не проверял никто: город останавливался на 90 %, а персонажи, пропы гонок и значки миссий писали дальше — за конец
// массива list, и GE настоящей PSP исполнял мусор (встаёт вся приставка; эмулятор молчит). Гомер (homerDraw -> skinDrawPal) — всегда
int (*skinGeRoom)(unsigned bytes);
unsigned skinGeSkips;
// правка 428 (аудит железа TEST27 п.1): проход контура на пачку шлёт НЕ 16 слов, а 24: отключить текстуру 1, AmbientColor 2 (два слова: цвет и альфа),
// ModelColor 4, DepthMask 1, рисование оболочки до 6 (VTYPE, BASE+IADDR, BASE+VADDR, PRIM), DepthMask 1, включить текстуру 1, AmbientColor 2, ModelColor 4 = 22,
// +2: после контура основное рисование пачки с индексами тоже 6 слов, не 4 (bound = -1: привязка текстуры 9 слов уже в 16). Хост-счётчик tests/toon/ge_words.py.
// -DSKIN_GE_TOON_OLD=1 — прежняя оценка (+16): красный контроль, на плотной сцене занижает
#ifndef SKIN_GE_TOON_OLD
#define SKIN_GE_TOON_OLD 0
#endif
#define SKIN_GE_TOON_WORDS ((SKIN_GE_TOON_OLD ? 16u : 24u) + (SKIN_TOON_V675_OLD ? 0u : 3u) + (SKIN_TOON_SIL_OLD ? 0u : 4u))   // №675: +3 слова на включение/функцию смешения/выключение вокруг прохода контура; №779: +4 — масштаб и центр Z оболочки и возврат (по 2 слова)
unsigned skinGeBytes(const SkinModel *m) {   // оценка сверху, слов: состояние и 4 источника света комнаты <= 96, на пачку — CLUT и
  unsigned w = 96;                           // текстура 9, альфа-тест 3, кости 13 x nb (BOFS + 12), рисование 4
  for (unsigned i = 0; i < m->h->nBatches; i++) w += 16u + 13u * m->b[i].nb + (skinToonEnabled() ? SKIN_GE_TOON_WORDS + (SKIN_TOON_PRE_OLD ? 0u : 13u * m->b[i].nb) : 0u);   // №797: кости второй раз в первом проходе   // правка 428: проход контура
  if (skinToonShadeOn()) { w += SKIN_TOON_SHADE_BASE; for (unsigned i = 0; i < m->h->nBatches; i++) w += SKIN_TOON_SHADE_WORDS; }   // №675: проход ступени тени
  return w * 4u;
}
void skinDraw(const SkinPlayer *p, const ScePspFMatrix4 *world) {
  if (skinGeRoom && !skinGeRoom(skinGeBytes(p->m))) { skinGeSkips++; return; }
  skinDrawPal(p->m, p->pal, world, p->eyeTex);
}
#endif
