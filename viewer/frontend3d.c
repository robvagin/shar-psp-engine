// frontend3d.c: 3D-гостиная главного меню (fe.shr, формат FE1 — tools/frontend3d_export.py). См. frontend3d.h.
#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "frontend3d.h"
#include "skin.h"
#include "tone.h"   // №1021-герои: toneToon (Cartoon)
#include "fe_billcut.h"
#include "fe_gag_clock.h"

// ---- формат FE1 (как в шапке tools/frontend3d_export.py) ----
typedef struct {
  char magic[4]; unsigned version, size, nTex, offTex, nPiece, offPiece, nTrack, offTrack, nBill, offBill, nFrames,
      offCam, menuFrame; float fps; unsigned offHomer, sizeHomer; float place[12]; unsigned offLights, srcTris, offTexNames;
} FeHeader;
typedef struct { unsigned short w, h; unsigned char nlev, pad[3]; unsigned offClut, offLev[8]; unsigned short bufw[8]; unsigned pad2; } FeTex;
typedef struct {
  unsigned short tex, texTrack, flags, node, vis, pad; unsigned nverts, offVerts;
  float origin[3], scale, bbox[6]; unsigned srcTris;
} FePiece;
typedef struct { unsigned type, nframes; float fps; unsigned flags, off, pad[3]; } FeTrack;
typedef struct { unsigned short track, tex, flags; unsigned char mode, glow; float w, h; unsigned abgr; float m[12]; unsigned pad[3]; } FeBill;
typedef struct { float key[3]; unsigned keyCol; float fill[3]; unsigned fillCol; unsigned ambCol; float scale; } FeLights;
typedef struct { short u, v; unsigned c; short x, y, z, pad; } FeVert;
typedef struct { short u, v; unsigned c; float x, y, z; } FeBVert;               // подсветки: uv 16 бит, позиция float
// перелёт (хвост файла: последние 16 Б = 'FLT1', u32 смещение таблицы, u32 число записей, u32 0)
typedef struct { unsigned char item, flags; unsigned short pad, go0, go1, back0, back1; float deg; } FeFlight;
_Static_assert(sizeof(FeHeader) == 128 && sizeof(FeTex) == 64 && sizeof(FePiece) == 64 && sizeof(FeTrack) == 32 &&
               sizeof(FeBill) == 80 && sizeof(FeLights) == 40 && sizeof(FeVert) == 16 && sizeof(FeBVert) == 20 &&
               sizeof(FeFlight) == 16, "FE1 layout");
enum { T_MAT = 1, T_VIS, T_TEX, T_BILL };
enum { TR_LOOP = 1, TR_TIMELINE = 2 };
enum { F_ATEST = 1, F_BLEND = 2, F_ADD = 4, F_NOZW = 8, F_MENUHID = 16, F_PREHOMER = 32 };
#define VT_PIECE (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
#define VT_BILL  (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define MAXP 1024

FeStats feStats;
int feGlowTest = -1;
void (*feReadHook)(unsigned done, unsigned total);
#define FE_CHUNK (256u << 10)          // кусок чтения fe.shr (ход загрузки на экране LoadingFE / титульном)
static unsigned char *fd;            // файл целиком
static const FeHeader *H; static const FeTex *tx; static const FePiece *pc; static const FeTrack *tr; static const FeBill *bl;
static const FeLights *lt;
static SkinModel hm; static SkinPlayer *hp; static int haveHomer;
// №404 (Роб 08.10: «пасхалки меню: прохожие заглядывают в окно, Мэгги ползает»): сценки гостиной из fe.shr (таблица GAG1,
// tools/frontend3d_export.py; порядок Gag0..Gag7 страницы 3dFEGags.pag: gaghomer, gaggrand, gagmole, gagfrink, gagbarne,
// gagnick, gagsnake, maggie). Из данных диска (p3d): страница 3dFEGags.pag показывает одну сценку из восьми (у каждой свой
// слой), рисует их камерой гостиной FE_Camera и стоит в MainMenu.scr ПЕРЕД 3dFE.pag (сценка рисуется до комнаты: до всего
// смешиваемого гостиной); глаза — дорожка текстур TEX_* по времени клипа (ключи CHR1, blinkFps 0 — без случайного мигания
// skin.c); клип сценки — один раз (флаг цикла 0), перенос корня играет (Мэгги ползёт 3,5 м и обратно).
// Из исполняемого файла диска (SLES_518.97, CGuiScreenMainMenu: конструктор 0x1b8180, обновление 0x1b9088–0x1b952c):
//   * часы сценок — мс экрана главного меню; первая сценка на 15000 мс (FE_GAG_FIRST_MS);
//   * за 2000 мс до запуска выбирается номер: (rand()/10 % 97) % число слоёв (8) — повтор той же разрешён;
//     для Gag0 ещё отрезок Гомера: (rand()/10 % 97) % 5;
//   * запуск: прошлый слой гасится, новый показывается, его анимация с начала; часы в 0; следующий порог —
//     ((rand() & 15) + 15) * 1000 мс, то есть 15..30 с от НАЧАЛА прошлой (FE_GAG_NEXT_*); прошлая сценка видна (последний
//     кадр), пока её не сменит следующая;
//   * Gag0 (слой Homer, gaghomer): код ставит объекту «Homer» страницы 3dFE (Гомер меню) анимацию сценки на отрезок кадров
//     FE_HOMER_GAG_SEG (таблица 0x42bc98, 5 штук) со смешиванием 10 кадров (0x42bccc), без цикла; доиграл — Гомер меню
//     снова 0..60 своего цикла, без смешивания, с кадра 0. Порт: Гомер меню играет клип 1 своего CHR1 (движение gaghomer на
//     его скелете, экспорт); модели gaghomer в fe.shr нет (флаг 4). В записи PS2 при входе в меню виден один бодрствующий
//     Гомер — модель сценки Gag0 отдельно не рисуется;
//   * звук: за 2 с — событие 180 со своим звуком сценки (FE_Gag_Homer, FE_Gag_Moleman, FE_Gag_Barney, FE_Gag_Maggie…),
//     при запуске — 181; таблица звуков заполняется при запуске игры (в файле её нет) — в порт не перенесено (вопрос В100).
// Правило порта: во время перелёта камеры новая сценка не начинается (ждёт конца перелёта); идущая доигрывает.
// -DFE_GAGS_OLD=1 — без сценок (красный контроль tests/menu/gags.sh)
#ifndef FE_GAGS_OLD
#define FE_GAGS_OLD 0
#endif
#define FE_GAG_FIRST_MS 15000u   // SLES_518.97 0x1b8180: li a3,15000 -> порог первой сценки
#define FE_GAG_NEXT_MIN_S 15u    // 0x1b9510..0x1b9524: ((rand() & 15) + 15) * 1000
#define FE_GAG_NEXT_RND 16u
#define FE_HOMER_GAG_N 5         // 0x42bcc8
#define FE_HOMER_GAG_BLEND_F 10.f   // 0x42bccc: смешивание 10 кадров
static const float FE_HOMER_GAG_SEG[FE_HOMER_GAG_N][2] = {{0, 75}, {75, 145}, {145, 215}, {215, 355}, {355, 475}};   // 0x42bc98, кадры @ 30
#define FE_HOMER_LOOP_END 60.f   // 0x1b915c: 60.0 — цикл Гомера меню 0..60 после сценки
#define FE_MAXGAG 8
#define GAG_ONHOMER 4
typedef struct { char name[16]; unsigned offChr, sizeChr; float frames, fps, place[12]; unsigned flags, pad[3]; } FeGag;   // 96 Б
static const FeGag *gagRec; static SkinModel gm[FE_MAXGAG]; static int ngag;
static SkinPlayer *gp; static int gagCur = -1, gagShown = -1, homerGag = -1; static unsigned gagSeed = 12345u;
static FeGagClock gagClock; static int gagMenuActive = 1; static unsigned lastT;
int feGagForce = -1, feGagAuto = 1, feGagSeg = -1; float feGagAt = -1.f;   // служебное (смоук «flag fegag N», «fegagat С», «fegagauto 0», «fegagseg K»)
void (*feGagLog)(const char *line);
void (*feGagEvent)(int event, int index, int segment);
void feGagMenu(int active) {
  active = active != 0;
  if (feGagLog && active != gagMenuActive) { char ln[112]; snprintf(ln, sizeof ln, "FEMENU active=%d elapsed_ms=%u pending=%d\n", active, gagClock.elapsed, gagClock.pending); feGagLog(ln); }
  if (active != gagMenuActive && !FE_GAG_CLOCK_OLD) lastT = 0;   // exclude the interval without feDraw (e.g. title)
  gagMenuActive = active;
}
static unsigned gagRand(void) { gagSeed = gagSeed * 1103515245u + 12345u; return (gagSeed >> 1) & 0x7fffffffu; }
static int homerHasGag(void) { return haveHomer && hm.h->nClips >= 2; }
static void gagStart(int k, int forced) {
  int pendingSeg = gagClock.pending == k ? gagClock.segment : -1;
  feGagClockStarted(&gagClock, gagRand);
  gagShown = -1; gagCur = -1;
  if (gagRec[k].flags & GAG_ONHOMER) {                      // Gag0: движение играет Гомер меню (клип 1), отрезок из 5
    if (!homerHasGag()) return;
    int seg = feGagSeg >= 0 && feGagSeg < FE_HOMER_GAG_N ? feGagSeg : pendingSeg >= 0 ? pendingSeg : (int)((gagRand() / 10u % 97u) % FE_HOMER_GAG_N);
    homerGag = seg; skinPlay(hp, 1, 0, FE_HOMER_GAG_BLEND_F / 30.f); hp->t = FE_HOMER_GAG_SEG[seg][0] / 30.f;
    if (feGagAt >= 0.f) { hp->t = feGagAt; hp->prev = -1; }   // служебное: поза без смешивания
  } else {
    gagCur = gagShown = k; skinInit(gp, &gm[k]); skinPlay(gp, 0, 0, 0.f);   // отрезок Гомера (если идёт) доиграет сам
    if (feGagAt >= 0.f) gp->t = feGagAt;
  }
  feStats.gagStarts++;
  if (feGagEvent) {
    if (forced) feGagEvent(180, k, gagShown < 0 ? homerGag : -1);
    feGagEvent(181, k, gagShown < 0 ? homerGag : -1);
  }
  if (feGagLog) { char ln[144]; snprintf(ln, sizeof ln, "FEGAG start i=%d name=%s forced=%d seg=%d at=%.2f next_ms=%u\n", k, gagRec[k].name, forced,
                                         gagShown < 0 ? homerGag : -1, gagShown < 0 && homerGag >= 0 ? hp->t : gagShown >= 0 ? gp->t : 0.f, gagClock.next); feGagLog(ln); }
}
static int gagEyeTex(void) {                               // кадр дорожки TEX_* на времени клипа (-1 — нет дорожки)
  const SkinModel *m = &gm[gagShown]; int n = (int)m->h->nBlink;
  if (!n) return -1;
  float f = gp->t * gagRec[gagShown].fps; int i = 0;
  while (i + 1 < n && m->blink[i + 1].frame <= f) i++;
  return m->blink[i].tex;
}
static float clockS, fpsAcc, cpuAcc; static int fpsN;
static short order[2][MAXP]; static float dist[2][MAXP];   // смешиваемое до Гомера [0] и после [1] (страница 3dFE.pag)
static const FeFlight *flt; static int nflt;
static float tlPos, tlEnd; static int flying;             // кадр таймлайна камеры (дробный), куда летим, идёт ли перелёт
// правка 265 (frontend3d.h): мип гостиной по глубине пикселя. pDens[i] — плотность текстуры куска i в повторах на метр
// (sqrt(площадь UV / площадь в мире) по его треугольникам; 0 — без текстуры или вырожденный: как раньше, AUTO)
#define FE_MIP_LIFT 0.5f      // уровень = log2(тексели на пиксель, поверхность лицом к камере) + 0,5: у AUTO наклонные
                              // поверхности берут +0..1,7 уровня (max по осям), лицом — +0; середина (правило порта)
#ifndef FE_MIP_OLD            // сборка -DFE_MIP_OLD=1 — прежнее по умолчанию (как smoke flag feold265 1)
#define FE_MIP_OLD 0
#endif
int feMipOld = FE_MIP_OLD, feSweep;
static float *pDens; static float mipK;
static int floorTex = -1, floorDrawn, homeLast = -1; void (*feFrameLog)(const char *line);   // правка 293: журнал перелёта по кадрам                   // mipK: наклон на 1 текстель/м (кадр: размер пикселя на 1 м глубины)
static int sweepOn, sweepN, sweepK, sweepPass, swPrev = -1;   // служебный покадровый перелёт (feSweep); swPrev — метка прошлого кадра
static float sweepPos[64];
static char feDir[192];                                   // папка fe.shr (рядом с EBOOT): снимки покадрового перелёта — feDir/shots
// правка 293 (служебное fecap, frontend3d.h): кадры настоящего перелёта в ОЗУ — состояние (код у feCapGrab)
#define FE_CAP_MAX 48
int feCapReq, feCapFrom, feCapTag = 293;   // правка 325: fecapfrom — пропустить первые N кадров перелёта, fecaptag — fe<метка>_*.bmp
static int capFrom, capTagNow = 293, capArm, capWant, capThis, capHead, capTail, capSlots, capSeen, capFb32, capK[FE_CAP_MAX]; static float capTl[FE_CAP_MAX], capNowTl;
static unsigned char *capBuf; static unsigned capSlot;

int feReady(void) { return H != 0; }

static int inside(unsigned off, unsigned len) { return off <= H->size && len <= H->size - off; }

// CHR1 внутри fe.shr годен для GE (все смещения внутри) — Гомер меню и сценки №404
static int chrModel(SkinModel *m, unsigned off, unsigned size, unsigned fsize) {
  if (off > fsize || size > fsize - off || size < sizeof(SkinHeader)) return 0;
  const unsigned char *d = fd + off; const SkinHeader *sh = (const SkinHeader *)d;
  int ok = !memcmp(sh->magic, "CHR1", 4) && sh->size == size && sh->nJoints <= SKIN_MAXJ &&
           sh->nClips && sh->offBatches + sh->nBatches * 48 <= sh->size && sh->offTex + sh->nTex * 48 <= sh->size &&
           sh->offClips + sh->nClips * 80 <= sh->size && sh->offJoints + sh->nJoints * 128 <= sh->size &&
           sh->offBlink <= sh->size && sh->nBlink <= (sh->size - sh->offBlink) / 8;
  for (unsigned i = 0; ok && i < sh->nBatches; i++) {
    const SkinBatch *b = (const SkinBatch *)(d + sh->offBatches) + i;
    ok = b->nb >= 1 && b->nb <= 8 && b->tex < sh->nTex && b->offVerts + b->nverts * b->stride <= sh->size;
    for (int k = 0; ok && k < b->nb; k++) ok = b->bones[k] < sh->nJoints;
  }
  for (unsigned i = 0; ok && i < sh->nClips; i++) ok = ((const SkinClip *)(d + sh->offClips))[i].offTracks + sh->nJoints * 16 <= sh->size;
  if (ok) { m->base = d; m->h = sh; m->j = (const SkinJoint *)(d + sh->offJoints); m->b = (const SkinBatch *)(d + sh->offBatches);
            m->tex = (const SkinTex *)(d + sh->offTex); m->c = (const SkinClip *)(d + sh->offClips); m->blink = (const SkinBlinkKey *)(d + sh->offBlink); }
  return ok;
}

int feFailTest;   // №841 тест (frontend3d.h)
void *(*feAllocHook)(unsigned size); void (*feFreeHook)(void *p);   // №841 (frontend3d.h): где держать fe.shr; 0 — куча
static void fdFree(void *p) { if (feFreeHook) feFreeHook(p); else free(p); }
int feLoad(const char *path) {
  feFree(); memset(&feStats, 0, sizeof feStats);
  { const char *sl = strrchr(path, '/'); size_t n = sl ? (size_t)(sl - path) : 0; if (n >= sizeof feDir) n = 0;
    memcpy(feDir, path, n); feDir[n] = 0; }
  unsigned t0 = sceKernelGetSystemTimeLow();
  SceUID f = sceIoOpen(path, PSP_O_RDONLY, 0); if (f < 0) return -1;
  unsigned size = (unsigned)sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  fd = feAllocHook ? (unsigned char *)feAllocHook(size) : memalign(64, size);
  if (fd && feFailTest > 0) { feFailTest--; fdFree(fd); fd = 0; }   // №841 тест: «flag fefail N» — следующие N чтений гостиной без памяти, как на PSP с раздробленной кучей
  if (!fd) { sceIoClose(f); return -2; }
  for (unsigned got = 0; got < size;) {              // кусками: между ними экран загрузки рисует ход (feReadHook)
    unsigned n = size - got < FE_CHUNK ? size - got : FE_CHUNK;
    if (sceIoRead(f, fd + got, n) != (int)n) { sceIoClose(f); fdFree(fd); fd = 0; return -2; }
    got += n; if (feReadHook) feReadHook(got, size);
  }
  sceIoClose(f);
  H = (const FeHeader *)fd;
  if (size < 128 || memcmp(H->magic, "FE1", 4) || H->version != 1 || H->size != size || H->nPiece > MAXP ||
      !inside(H->offTex, H->nTex * 64) || !inside(H->offPiece, H->nPiece * 64) || !inside(H->offTrack, H->nTrack * 32) ||
      !inside(H->offBill, H->nBill * 80) || !inside(H->offCam, H->nFrames * 48) || !inside(H->offLights, 40) ||
      !inside(H->offHomer, H->sizeHomer) || H->menuFrame >= H->nFrames) { feFree(); return -3; }
  tx = (const FeTex *)(fd + H->offTex); pc = (const FePiece *)(fd + H->offPiece); tr = (const FeTrack *)(fd + H->offTrack);
  bl = (const FeBill *)(fd + H->offBill); lt = (const FeLights *)(fd + H->offLights);
  for (unsigned i = 0; i < H->nPiece; i++)          // всё, что прочтёт GE, — внутри файла
    if (!inside(pc[i].offVerts, pc[i].nverts * 16) || (pc[i].tex != 0xffff && pc[i].tex >= H->nTex) ||
        (pc[i].node != 0xffff && pc[i].node >= H->nTrack) || (pc[i].texTrack != 0xffff && pc[i].texTrack >= H->nTrack)) { feFree(); return -4; }
  static const unsigned esz[5] = {0, 48, 1, 2, 24};
  for (unsigned i = 0; i < H->nTrack; i++) {
    if (!tr[i].nframes || tr[i].type < 1 || tr[i].type > 4 || !inside(tr[i].off, tr[i].nframes * esz[tr[i].type])) { feFree(); return -4; }
    if (tr[i].type == T_TEX)
      for (unsigned k = 0; k < tr[i].nframes; k++) if (((const unsigned short *)(fd + tr[i].off))[k] >= H->nTex) { feFree(); return -4; }
  }
  for (unsigned i = 0; i < H->nBill; i++)
    if ((bl[i].tex != 0xffff && bl[i].tex >= H->nTex) || (bl[i].track != 0xffff && bl[i].track >= H->nTrack)) { feFree(); return -4; }
  for (unsigned i = 0; i < H->nTex; i++) {
    const FeTex *x = &tx[i]; int lw = x->w, lh = x->h;
    if (!x->nlev || x->nlev > 8 || !inside(x->offClut, 1024)) { feFree(); return -4; }
    for (int l = 0; l < x->nlev; l++) {
      if (x->bufw[l] < lw || !inside(x->offLev[l], (unsigned)x->bufw[l] * ((lh + 7) & ~7))) { feFree(); return -4; }
      lw = lw > 1 ? lw >> 1 : 1; lh = lh > 1 ? lh >> 1 : 1;
    }
  }
  // Гомер меню: CHR1 внутри файла (viewer/skin.h), свой плеер; всё, что прочтёт GE, — внутри CHR1
  const SkinHeader *sh = (const SkinHeader *)(fd + H->offHomer);
  int homerOk = H->sizeHomer >= sizeof(SkinHeader) && !memcmp(sh->magic, "CHR1", 4) && sh->size == H->sizeHomer && sh->nJoints <= SKIN_MAXJ &&
                sh->nClips && sh->offBatches + sh->nBatches * 48 <= sh->size && sh->offTex + sh->nTex * 48 <= sh->size &&
                sh->offClips + sh->nClips * 80 <= sh->size && sh->offJoints + sh->nJoints * 128 <= sh->size &&
           sh->offBlink <= sh->size && sh->nBlink <= (sh->size - sh->offBlink) / 8;
  for (unsigned i = 0; homerOk && i < sh->nBatches; i++) {
    const SkinBatch *b = (const SkinBatch *)(fd + H->offHomer + sh->offBatches) + i;
    homerOk = b->nb >= 1 && b->nb <= 8 && b->tex < sh->nTex && b->offVerts + b->nverts * b->stride <= sh->size;
    for (int k = 0; homerOk && k < b->nb; k++) homerOk = b->bones[k] < sh->nJoints;
  }
  for (unsigned i = 0; homerOk && i < sh->nClips; i++) homerOk = ((const SkinClip *)(fd + H->offHomer + sh->offClips))[i].offTracks + sh->nJoints * 16 <= sh->size;
  if (homerOk) {
    const unsigned char *d = fd + H->offHomer;
    hm.base = d; hm.h = sh; hm.j = (const SkinJoint *)(d + sh->offJoints); hm.b = (const SkinBatch *)(d + sh->offBatches);
    hm.tex = (const SkinTex *)(d + sh->offTex); hm.c = (const SkinClip *)(d + sh->offClips); hm.blink = (const SkinBlinkKey *)(d + sh->offBlink);
    hp = memalign(16, sizeof(SkinPlayer));
    if (hp) { skinInit(hp, &hm); skinPlay(hp, 0, 1, 0.f); haveHomer = 1; }
  }
  flt = 0; nflt = 0;                                // перелёты: хвост FLT1 (нет — старый файл, камера стоит)
  if (size >= 144 && !memcmp(fd + size - 16, "FLT1", 4)) {
    const unsigned *t = (const unsigned *)(fd + size - 16);
    if (t[2] <= 16 && inside(t[1], t[2] * 16)) {
      flt = (const FeFlight *)(fd + t[1]); nflt = (int)t[2];
      for (int i = 0; i < nflt; i++)
        if (flt[i].go0 >= H->nFrames || flt[i].go1 >= H->nFrames || flt[i].back0 >= H->nFrames || flt[i].back1 >= H->nFrames) { flt = 0; nflt = 0; break; }
    }
  }
  ngag = 0; gagRec = 0; gagCur = gagShown = homerGag = -1; feGagClockInit(&gagClock); gagMenuActive = 1;
  gagSeed ^= sceKernelGetSystemTimeLow();
  if (!FE_GAGS_OLD && size >= 144 && !memcmp(fd + size - 16, "FLT1", 4)) {
    unsigned og = ((const unsigned *)(fd + size - 16))[3];
    if (og && inside(og, 16) && !memcmp(fd + og, "GAG1", 4)) {
      unsigned n = ((const unsigned *)(fd + og))[1];
      if (n <= FE_MAXGAG && inside(og + 16, n * sizeof(FeGag))) {
        gagRec = (const FeGag *)(fd + og + 16);
        for (unsigned i = 0; i < n; i++) {
          if (!(gagRec[i].fps > 0.f)) break;
          if (gagRec[i].flags & GAG_ONHOMER) { memset(&gm[i], 0, sizeof gm[i]); ngag++; continue; }   // модели нет: играет Гомер меню
          if (!chrModel(&gm[i], gagRec[i].offChr, gagRec[i].sizeChr, size)) break;
          const SkinModel *m = &gm[i]; int ok = 1;
          for (unsigned k = 0; k < m->h->nBlink; k++) ok &= m->blink[k].tex < m->h->nTex;   // ключи глаз — свои текстуры
          if (!ok) break;
          ngag++;
        }
        if (ngag != (int)n) ngag = 0;
        if (ngag && !(gp = memalign(16, sizeof(SkinPlayer)))) ngag = 0;
      }
    }
  }
  feStats.gags = ngag;
  sceKernelDcacheWritebackRange(fd, size);          // GE читает вершины, палитры и тексели из ОЗУ
  pDens = malloc(H->nPiece * sizeof(float));        // правка 265: плотность текстуры куска (повторы на метр) — для наклона мипа
  if (pDens) for (unsigned i = 0; i < H->nPiece; i++) {
    const FePiece *p = &pc[i]; const FeVert *v = (const FeVert *)(fd + p->offVerts);
    float wa = 0.f, ua = 0.f; pDens[i] = 0.f;
    if (p->tex == 0xffff && p->texTrack == 0xffff) continue;
    for (unsigned k = 0; k + 2 < p->nverts; k += 3) {   // в осях куска (s16) — масштаб ниже; узлы дорожек без масштаба
      float ax = v[k + 1].x - v[k].x, ay = v[k + 1].y - v[k].y, az = v[k + 1].z - v[k].z;
      float bx = v[k + 2].x - v[k].x, by = v[k + 2].y - v[k].y, bz = v[k + 2].z - v[k].z;
      float cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
      wa += sqrtf(cx * cx + cy * cy + cz * cz);
      float u0 = (unsigned short)v[k].u, v0 = (unsigned short)v[k].v;   // GE читает u16 (tools/uvq.py, правка 43)
      ua += fabsf(((unsigned short)v[k + 1].u - u0) * ((unsigned short)v[k + 2].v - v0) - ((unsigned short)v[k + 2].u - u0) * ((unsigned short)v[k + 1].v - v0));
    }
    float s = p->scale / 32767.f;                    // s16 -> метры; uv: число / 256 = повторы (TexScale 128)
    wa *= s * s; ua *= 1.f / 65536.f;
    if (wa > 1e-8f && ua > 1e-10f) pDens[i] = sqrtf(ua / wa);
  }
  floorTex = -1;   // правка 293 (служебное): текстура пола гостиной — для покадрового журнала перелёта на железе
  if (H->offTexNames) for (unsigned i = 0; i < H->nTex; i++) if (!strncmp((const char *)(fd + H->offTexNames + 32 * i), "floor3", 6)) floorTex = (int)i;
  feStats.bytes = size + (hp ? sizeof(SkinPlayer) : 0) + (gp ? sizeof(SkinPlayer) : 0) + (pDens ? H->nPiece * sizeof(float) : 0);
  feStats.loadMs = (sceKernelGetSystemTimeLow() - t0) / 1000.f;
  feStats.flights = nflt;
  lastT = 0; clockS = 0.f; fpsAcc = cpuAcc = 0.f; fpsN = 0; feGlowTest = -1;
  tlPos = tlEnd = (float)H->menuFrame; flying = 0;
  return 0;
}

void feFree(void) {
  if (hp) free(hp);
  if (gp) free(gp);
  if (pDens) free(pDens);
  if (fd) fdFree(fd);
  hp = 0; gp = 0; ngag = 0; gagRec = 0; gagCur = gagShown = homerGag = -1; pDens = 0; fd = 0; H = 0; haveHomer = 0; flt = 0; nflt = 0; flying = 0; sweepOn = 0;
  if (capBuf) { free(capBuf); capBuf = 0; } capArm = capThis = capSeen = 0;   // правка 293: кадры fecap без feCapFlush — не терять память
}

// ---- перелёты камеры ----
static const FeFlight *flightOf(int item) {
  for (int i = 0; i < nflt; i++) if (flt[i].item == item) return &flt[i];
  return 0;
}
int feFlight(int item, int *hasBack) {
  const FeFlight *f = H ? flightOf(item) : 0;
  if (hasBack) *hasBack = f && (f->flags & 1);
  return f != 0;
}
int feFly(int item, int mode) {
  const FeFlight *f = H ? flightOf(item) : 0;
  if (!f || (mode != FE_GO && !(f->flags & 1))) return 0;   // обратного перелёта в данных нет — камера стоит
  if (mode == FE_BACK) { tlPos = f->back0; tlEnd = f->back1; }
  else { tlPos = f->go0; tlEnd = mode == FE_ROUND ? f->back1 : f->go1; }   // у петли go1 == back0: кадры подряд
  flying = 1; lastT = 0;                                    // время перелёта — с первого кадра после запуска
  sweepOn = 0;
  if (feSweep && mode == FE_GO && f->go1 > f->go0) {        // служебное 265: покадровый перелёт, два прохода (frontend3d.h)
    int n = 0; float a = f->go0, b = f->go1, split = b - 10.f > a ? b - 10.f : a;
    for (float x = a; x <= split + 1e-3f && n < 64; x += 1.f) sweepPos[n++] = x;
    for (float x = split + .5f; x <= b + 1e-3f && n < 64; x += .5f) sweepPos[n++] = x;
    sweepN = n; sweepK = 0; sweepPass = 0; sweepOn = 1; swPrev = -1; feSweep = 0;
  }
  capArm = feCapReq > 0 && mode == FE_GO && !sweepOn; capSeen = 0;   // правка 293 (fecap): этот перелёт — по кадрам в ОЗУ, один раз
  if (capArm) { capWant = feCapReq; feCapReq = 0; capFrom = feCapFrom; capTagNow = feCapTag; }
  return 1;
}
int feFlying(void) { return flying; }
void feHome(void) { if (H) { tlPos = tlEnd = (float)H->menuFrame; flying = 0; } }
float feFrame(void) { return tlPos; }

// ---- дорожки ----
static int trFrame(const FeTrack *t, float timeline) {
  if (t->flags & TR_TIMELINE) { int f = (int)timeline; return f < (int)t->nframes ? f : (int)t->nframes - 1; }
  int f = (int)(clockS * t->fps); return f % (int)t->nframes;
}
static const float *trMat(int i, float tl) { const FeTrack *t = &tr[i]; return (const float *)(fd + t->off) + 12 * trFrame(t, tl); }
static void rowsToMat(ScePspFMatrix4 *m, const float *r) {   // 3 строки (R|t), столбцовая запись GE
  m->x.x = r[0]; m->x.y = r[4]; m->x.z = r[8];  m->x.w = 0.f;
  m->y.x = r[1]; m->y.y = r[5]; m->y.z = r[9];  m->y.w = 0.f;
  m->z.x = r[2]; m->z.y = r[6]; m->z.z = r[10]; m->z.w = 0.f;
  m->w.x = r[3]; m->w.y = r[7]; m->w.z = r[11]; m->w.w = 1.f;
}

// ---- текстуры ----
static int bound;
static void bindTex(int t) {
  if (t == bound) return; bound = t;
  if (t < 0) { sceGuDisable(GU_TEXTURE_2D); return; }
  const FeTex *x = &tx[t];
  sceGuEnable(GU_TEXTURE_2D);
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, fd + x->offClut);
  sceGuTexMode(GU_PSM_T8, x->nlev - 1, 0, 1);
  int lw = x->w, lh = x->h;
  for (int l = 0; l < x->nlev; l++) { sceGuTexImage(l, lw, lh, x->bufw[l], fd + x->offLev[l]); lw = lw > 1 ? lw >> 1 : 1; lh = lh > 1 ? lh >> 1 : 1; }
  sceGuTexFilter(x->nlev > 1 ? GU_LINEAR_MIPMAP_LINEAR : GU_LINEAR, GU_LINEAR);
}
// правка 265: выбор мипа куска. mode 0 — правило (наклон по глубине пикселя), 1 — прежнее AUTO, 2 — самый мелкий уровень
// (имитация «зелёного полигона», красный контроль гейта картинки). dens — повторы на метр (pDens), 0 — AUTO
static int curLvl; static float curSl;
static void lvlAuto(void) { if (curLvl != 1) { sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvl = 1; } }
static void mipPick(int t, float dens, int mode) {
  if (t < 0) return;
  const FeTex *x = &tx[t];
  if (x->nlev < 2 || mode == 1 || dens <= 0.f) { lvlAuto(); return; }
  if (mode == 2) { sceGuTexLevelMode(GU_TEXTURE_CONST, (float)(x->nlev - 1)); curLvl = -1; return; }   // уровень у каждой текстуры свой
  float sl = dens * sqrtf((float)x->w * (float)x->h) * mipK;   // тексели на метр × размер пикселя на 1 м глубины / 2 × 2^LIFT
  if (curLvl != 0) { sceGuTexLevelMode(GU_TEXTURE_SLOPE, 0.f); curLvl = 0; }
  if (sl != curSl) { sceGuTexSlope(sl); curSl = sl; }
}
static int curFl;
static void setFlags(int fl) {
  if (fl == curFl) return; curFl = fl;
  if (fl & F_ATEST) sceGuEnable(GU_ALPHA_TEST); else sceGuDisable(GU_ALPHA_TEST);
  if (fl & F_ADD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff); }        // donut: ONE, ONE
  else if (fl & F_BLEND) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); }
  else sceGuDisable(GU_BLEND);
  sceGuDepthMask((fl & F_NOZW) ? GU_TRUE : GU_FALSE);
}

// ---- камера и отсечение ----
static float cp[3], cf[3], cr[3], cu[3], tvx, tvy, cnear;
static void norm3(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0.f) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void cross3(float *o, const float *a, const float *b) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
// коробка (в осях узла r или мира) видна? 8 углов против ближней и 4 боковых граней пирамиды (запас 5%)
static int boxVisible(const float *bb, const float *r, float *dOut) {
  int out[5] = {0, 0, 0, 0, 0}; float zs = 0.f;
  for (int k = 0; k < 8; k++) {
    float p[3] = {bb[(k & 1) ? 3 : 0], bb[(k & 2) ? 4 : 1], bb[(k & 4) ? 5 : 2]}, w[3];
    if (r) for (int i = 0; i < 3; i++) w[i] = r[4 * i] * p[0] + r[4 * i + 1] * p[1] + r[4 * i + 2] * p[2] + r[4 * i + 3];
    else { w[0] = p[0]; w[1] = p[1]; w[2] = p[2]; }
    float d[3] = {w[0] - cp[0], w[1] - cp[1], w[2] - cp[2]};
    float z = d[0] * cf[0] + d[1] * cf[1] + d[2] * cf[2], x = d[0] * cr[0] + d[1] * cr[1] + d[2] * cr[2], y = d[0] * cu[0] + d[1] * cu[1] + d[2] * cu[2];
    out[0] += z < cnear; out[1] += x > z * tvx * 1.05f; out[2] += -x > z * tvx * 1.05f; out[3] += y > z * tvy * 1.05f; out[4] += -y > z * tvy * 1.05f;
    zs += z;
  }
  for (int i = 0; i < 5; i++) if (out[i] == 8) return 0;
  if (dOut) *dOut = zs * .125f;
  return 1;
}

static void drawPiece(const FePiece *p, float tl) {
  ScePspFMatrix4 m; const float *r = p->node != 0xffff ? trMat(p->node, tl) : 0;
  float s = p->scale * (32768.f / 32767.f);
  static const float I[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  if (!r) r = I;
  // M = узел * T(origin) * S(s)
  m.x.x = r[0] * s; m.x.y = r[4] * s; m.x.z = r[8] * s;  m.x.w = 0.f;
  m.y.x = r[1] * s; m.y.y = r[5] * s; m.y.z = r[9] * s;  m.y.w = 0.f;
  m.z.x = r[2] * s; m.z.y = r[6] * s; m.z.z = r[10] * s; m.z.w = 0.f;
  m.w.x = r[0] * p->origin[0] + r[1] * p->origin[1] + r[2] * p->origin[2] + r[3];
  m.w.y = r[4] * p->origin[0] + r[5] * p->origin[1] + r[6] * p->origin[2] + r[7];
  m.w.z = r[8] * p->origin[0] + r[9] * p->origin[1] + r[10] * p->origin[2] + r[11]; m.w.w = 1.f;
  int t = p->tex;
  if (p->texTrack != 0xffff) { const FeTrack *k = &tr[p->texTrack]; t = ((const unsigned short *)(fd + k->off))[trFrame(k, tl)]; }
  bindTex(t == 0xffff ? -1 : t); setFlags(p->flags);
  mipPick(t == 0xffff ? -1 : t, pDens ? pDens[p - pc] : 0.f, sweepOn ? (sweepPass ? feMipOld : 1) : feMipOld);
  sceGuSetMatrix(GU_MODEL, &m);
  sceGuDrawArray(GU_TRIANGLES, VT_PIECE, (int)p->nverts, 0, fd + p->offVerts);
  feStats.pieces++; feStats.tris += (int)p->nverts / 3; if (t == floorTex) floorDrawn++;
}

#define FE_GLOW_LIFT .05f
// правка 293: подсветка пункта при старте перелёта гаснет, а не пропадает за кадр. У оригинала (запись близнеца PCSX2,
// build/twins/o4/v15_newgame/png, счётчик кадров 50/с): × на NEW GAME — кадр 2650, камера трогается 2656, жёлтая подсветка газеты
// на ковре гаснет 2656 -> 2664 (0,16 с; надпись NEW GAME и Accept — 2658 -> ~2670). У порта подсветка исчезала в первом же кадре
// движения (FEFR тура TEST 9: bills 9 -> 7) — вспышка на полу у газеты в самом начале наезда. Прежнее: «flag feglowcut 1»
#define FE_GLOW_FADE_S .16f
#ifndef FE_GLOW_CUT_OLD
#define FE_GLOW_CUT_OLD 0
#endif
int feGlowCut = FE_GLOW_CUT_OLD;
// правка 325 (viewer/fe_billcut.h): подсветка, которую GE нарисовал бы не целиком, режется на процессоре по пирамиде защитной
// полосы. Прежнее (квад как есть — у PSP свет из окна на полу пропадает при наезде к газете): «flag febillold 1» или сборка
// -DFE_BILL_CUT_OLD=1. billNeed — подсветок кадра не целиком в пирамиде (риск), billCut — из них нарезано (журнал FEFR bn= bc=)
#ifndef FE_BILL_CUT_OLD
#define FE_BILL_CUT_OLD 0
#endif
int feBillOld = FE_BILL_CUT_OLD;
static int billNeed, billCut;
static float glowFade = 1.f, glowOffT = -1.f;            // множитель подсветки пункта (1 — полная) и часы гостиной, когда ушли из главного вида
static int feHeroOn(void);
static void drawBill(const FeBill *b, int sel) {
  float w = b->w, h = b->h; unsigned c = b->abgr;
  if (b->track != 0xffff) {
    const FeTrack *k = &tr[b->track];
    int fr = (feGlowTest >= 0 && b->glow != 0xff) ? 0 : trFrame(k, 0.f);   // гейт: подсветка в полную силу (кадр 0 цикла)
    const float *v = (const float *)(fd + k->off) + 6 * fr;
    w = v[0]; h = v[1];
    int ch[4]; for (int i = 0; i < 4; i++) { float x = v[2 + i] * 255.f + .5f; ch[i] = x < 0.f ? 0 : x > 255.f ? 255 : (int)x; }
    c = (unsigned)ch[0] | (unsigned)ch[1] << 8 | (unsigned)ch[2] << 16 | (unsigned)ch[3] << 24;
  }
  if (b->glow != 0xff && glowFade < 1.f) {                  // правка 293: гаснущая подсветка — сложение: яркость, смешивание: альфа
    unsigned k = (unsigned)(glowFade * 256.f), r = c & 255, g = (c >> 8) & 255, bb = (c >> 16) & 255, a = c >> 24;
    if (b->flags & F_ADD) { r = r * k >> 8; g = g * k >> 8; bb = bb * k >> 8; } else a = a * k >> 8;
    c = r | g << 8 | bb << 16 | a << 24;
  }
  const float *m = b->m; float o[3] = {m[3], m[7], m[11]}, ax[3], ay[3];
  if (b->glow != 0xff) {   // правило порта: плоская подсветка (видик, журнал) лежит на поверхности предмета — 16-битная глубина PSP
    float t[3] = {cp[0] - o[0], cp[1] - o[1], cp[2] - o[2]}; norm3(t);   // её прячет; на 5 см к камере (глазом не видно)
    for (int k = 0; k < 3; k++) o[k] += t[k] * FE_GLOW_LIFT;
  }
  if (b->mode == 0) { ax[0] = m[0]; ax[1] = m[4]; ax[2] = m[8]; ay[0] = m[1]; ay[1] = m[5]; ay[2] = m[9]; }
  else if (b->mode == 1) {
    ay[0] = m[1]; ay[1] = m[5]; ay[2] = m[9]; norm3(ay);
    float tc[3] = {cp[0] - o[0], cp[1] - o[1], cp[2] - o[2]}; cross3(ax, ay, tc); norm3(ax);
  } else { memcpy(ax, cr, 12); memcpy(ay, cu, 12); }
  static const float sx[4] = {-.5f, .5f, -.5f, .5f}, sy[4] = {.5f, .5f, -.5f, -.5f};
  static const short su[4] = {0, 256, 0, 256}, sv[4] = {0, 0, 256, 256};
  float P[4][3], T[4][2];
  for (int i = 0; i < 4; i++) {
    for (int k = 0; k < 3; k++) P[i][k] = o[k] + ax[k] * sx[i] * w + ay[k] * sy[i] * h;
    T[i][0] = su[i] / 256.f; T[i][1] = sv[i] / 256.f;
  }
  FeCutCam kc = {{cp[0], cp[1], cp[2]}, {cr[0], cr[1], cr[2]}, {cu[0], cu[1], cu[2]}, {cf[0], cf[1], cf[2]}, tvx, tvy, cnear};
  FeCutVert cut[FE_CUT_MAXV]; int nc = feBillCut(P, T, &kc, cut);   // правка 325: -1 — квад целиком в пирамиде полосы
  bindTex(b->tex == 0xffff ? -1 : b->tex); setFlags(b->flags); lvlAuto();   // подсветки — как раньше (AUTO)
  ScePspFMatrix4 id; gumLoadIdentity(&id); sceGuSetMatrix(GU_MODEL, &id);
  if (nc >= 0) billNeed++;
  if (nc >= 0 && !feBillOld) {                              // правка 325: срез по пирамиде защитной полосы — веер, GE не режет
    billCut++; feStats.bills++; (void)sel;
    if (nc < 3) return;                                     // вся подсветка за near или вне пирамиды — не видна
    FeBVert *v = (FeBVert *)sceGuGetMemory((unsigned)nc * sizeof(FeBVert));
    for (int i = 0; i < nc; i++) {
      float s = cut[i].s * 256.f + .5f, t2 = cut[i].t * 256.f + .5f;
      v[i].u = (short)(s < 0.f ? 0 : s > 256.f ? 256 : (int)s); v[i].v = (short)(t2 < 0.f ? 0 : t2 > 256.f ? 256 : (int)t2); v[i].c = c;
      v[i].x = cut[i].w[0]; v[i].y = cut[i].w[1]; v[i].z = cut[i].w[2];
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, VT_BILL, nc, 0, v);
    feStats.tris += nc - 2;
    return;
  }
  FeBVert *v = (FeBVert *)sceGuGetMemory(4 * sizeof(FeBVert));
  for (int i = 0; i < 4; i++) {
    v[i].u = su[i]; v[i].v = sv[i]; v[i].c = c;
    v[i].x = P[i][0]; v[i].y = P[i][1]; v[i].z = P[i][2];
  }
  sceGuDrawArray(GU_TRIANGLE_STRIP, VT_BILL, 4, 0, v);
  feStats.bills++; feStats.tris += 2; (void)sel;
}

// №1021-герои (Роб 26.10: «главное меню — гостиная внутри дома в игре уже мультиком, сделай по такому же подобию, только Гомеру сильную обводку»): в Cartoon Гомер и герои сценок меню рисуются тем же путём, что в игре
// (skinDrawPal в мультяшном режиме: ровный цвет, контур-оболочка по расстоянию), контур — видимой толщины под камеру меню. FEHERO1021_OLD=1 / файл fehero1021old / «flag fehero1021 0» — как в TEST71 (свет и текстуры)
#ifndef FEHERO1021_OLD
#define FEHERO1021_OLD 1   // Роб 26.10 23:4x: «меню в гостиной мультиком выбивается, диван просвечивает сквозь Гомера — откати, меню родное» — выключено по умолчанию
#endif
int feHero1021 = !FEHERO1021_OLD;
static int feHeroOn(void) { return feHero1021 && skinToonEnabled() && toneToon(); }
static void feHeroRestore(void) {   // состояние GE и кэши feDraw после skinDrawPal (оно оставляет своё)
  sceGuDisable(GU_LIGHT1); sceGuDisable(GU_LIGHT0); sceGuDisable(GU_LIGHTING); sceGuColorMaterial(0); sceGuEnable(GU_TEXTURE_2D);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuDepthMask(GU_FALSE); sceGuDisable(GU_FOG); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuShadeModel(GU_SMOOTH);
  sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f); sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); sceGuAmbientColor(0xffffffff); sceGuModelColor(0, 0xffffffff, 0xffffffff, 0); curLvl = -1; curSl = -1.f; bound = -2; curFl = -1;
}
static void drawChr(const SkinModel *m, const SkinPlayer *pl, const float *place, int eyeTex);
static void drawHomer(void) { drawChr(&hm, hp, H->place, -1); }
// eyeTex >= 0 — текстура батчей глаз (флаг 1): кадр дорожки TEX_* сценки №404
static void drawChr(const SkinModel *m, const SkinPlayer *pl, const float *place, int eyeTex) {
  ScePspFMatrix4 w; rowsToMat(&w, place);
  if (feHeroOn()) {   // №1021-герои: путь игры (skinDrawPal в мультяшном режиме) и возврат состояния кадра гостиной
    ScePspFVector3 kd = {lt->key[0], lt->key[1], lt->key[2]}; unsigned kc = lt->keyCol; float kdir[1][3] = {{kd.x, kd.y, kd.z}};
    skinToonEye(cp[0], cp[1], cp[2]); skinLights(1, kdir, &kc, lt->ambCol);
    setFlags(0); skinDrawPal(m, pl->pal, &w, eyeTex);
    skinLights(0, 0, 0, 0);
    for (unsigned i = 0; i < m->h->nBatches; i++) feStats.tris += (int)m->b[i].nverts / 3;
    feHeroRestore();
    return; }
  ScePspFVector3 k = {lt->key[0], lt->key[1], lt->key[2]}, f = {lt->fill[0], lt->fill[1], lt->fill[2]};
  sceGuSetMatrix(GU_MODEL, &w);
  setFlags(0); bound = -2;
  sceGuEnable(GU_LIGHTING); sceGuEnable(GU_LIGHT0); sceGuEnable(GU_LIGHT1);
  sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &k); sceGuLightColor(0, GU_DIFFUSE, lt->keyCol); sceGuLightColor(0, GU_AMBIENT, 0);
  sceGuLight(1, GU_DIRECTIONAL, GU_DIFFUSE, &f); sceGuLightColor(1, GU_DIFFUSE, lt->fillCol); sceGuLightColor(1, GU_AMBIENT, 0);
  sceGuAmbient(lt->ambCol);
  sceGuModelColor(0, 0xffffffff, 0xffffffff, 0); sceGuAmbientColor(0xffffffff);
  sceGuEnable(GU_TEXTURE_2D); sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvl = 1;
  int cur = -1;
  for (unsigned i = 0; i < m->h->nBatches; i++) {
    const SkinBatch *b = &m->b[i];
    int ti = (eyeTex >= 0 && (b->flags & 1)) ? eyeTex : (int)b->tex;
    if (ti != cur) {
      const SkinTex *x = &m->tex[ti]; cur = ti;
      sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, m->base + x->offClut);
      sceGuTexMode(GU_PSM_T8, 0, 0, x->swizzled ? 1 : 0); sceGuTexImage(0, x->w, x->h, x->w, m->base + x->offPix);
    }
    for (int j = 0; j < b->nb; j++) sceGuBoneMatrix(j, &pl->pal[b->bones[j]]);
    sceGuDrawArray(b->prim, b->vtype, b->nverts, 0, m->base + b->offVerts);
    feStats.tris += (int)b->nverts / 3;
  }
  sceGuDisable(GU_LIGHT1); sceGuDisable(GU_LIGHT0); sceGuDisable(GU_LIGHTING);
}

// служебное 265: метка позы покадрового перелёта — 12 клеток 6×4 px с x = 60, y = 40 (внутри окна рамки-телевизора меню, она
// рисуется поверх углов и во время перелёта): белая, чёрная, биты 0..6 номера позы, бит прохода, чётность битов 0..7, белая
// (белая = 1). Читает tests/fe_guard/sweep_read.py — сверка, что записан кадр именно этой позы
static void sweepMark(int code) {
  typedef struct { unsigned c; short x, y, z, pad; } MV;
  MV *v = (MV *)sceGuGetMemory(24 * sizeof(MV));
  int par = 0; for (int b = 0; b < 8; b++) par ^= (code >> b) & 1;
  for (int i = 0; i < 12; i++) {
    int on = i == 0 || i == 11 ? 1 : i == 1 ? 0 : i == 10 ? par : (code >> (i - 2)) & 1;
    unsigned c = on ? 0xffffffffu : 0xff000000u;
    v[2 * i].c = v[2 * i + 1].c = c; v[2 * i].x = (short)(60 + i * 6); v[2 * i].y = 40; v[2 * i + 1].x = (short)(66 + i * 6); v[2 * i + 1].y = 44;
    v[2 * i].z = v[2 * i + 1].z = 0; v[2 * i].pad = v[2 * i + 1].pad = 0;
  }
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST);
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 24, 0, v);
  sceGuEnable(GU_DEPTH_TEST); bound = -2;
}
// служебное 265: кадр на экране (поза code) -> feDir/shots/fe265_<a|b>_<кадр×100>.bmp (24 бита, 480×272, как smoke.h) и строка в
// feDir/shots/fe265_sweep.txt. a — проход 0 (прежний мип), b — проход 1 (feMipOld: 0 правило, 1 прежнее, 2 самый мелкий мип).
// Запись идёт через msio (viewer/msio.h, -include): на железе — по очереди с остальной картой
static void sweepDump(int code) {
  void *top = 0; int bw = 0, pf = 0;
  if (sceDisplayGetFrameBuf(&top, &bw, &pf, PSP_DISPLAY_SETBUF_NEXTFRAME) < 0 || !top || bw < 480) return;
  unsigned t0 = sceKernelGetSystemTimeLow();
  char dir[224], fn[256]; snprintf(dir, sizeof dir, "%s/shots", feDir); sceIoMkdir(dir, 0777);
  int pass = code >> 7, idx = code & 127, tl100 = (int)(sweepPos[idx] * 100.f + .5f), mode = pass ? feMipOld : 1;
  snprintf(fn, sizeof fn, "%s/fe265_%c_%04d.bmp", dir, pass ? 'b' : 'a', tl100);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  int W = 480, Hh = 272; unsigned rowB = W * 3, size = 54 + rowB * Hh;
  unsigned char *row = f >= 0 ? malloc(rowB * 16) : 0;   // 23 КБ на время записи (карта PSP: меньше записей через msio)
  if (row) {
    unsigned char hd[54] = {'B', 'M'}; memcpy(hd + 2, &size, 4); unsigned v = 54; memcpy(hd + 10, &v, 4); v = 40; memcpy(hd + 14, &v, 4);
    memcpy(hd + 18, &W, 4); memcpy(hd + 22, &Hh, 4); hd[26] = 1; hd[28] = 24; v = rowB * Hh; memcpy(hd + 34, &v, 4);
    sceIoWrite(f, hd, 54);
    const unsigned char *base = (const unsigned char *)((unsigned)top | 0x40000000u);   // некэшируемый адрес VRAM
    for (int y = Hh - 1; y >= 0;) {                          // BMP — снизу вверх, по 16 строк за запись
      int n = 0;
      for (int k = 0; k < 16 && y >= 0; k++, y--, n++)
        for (int x = 0; x < W; x++) {
          unsigned char *o = row + n * rowB + x * 3; unsigned r, g, b;
          if (pf == PSP_DISPLAY_PIXEL_FORMAT_8888) { unsigned p = ((const unsigned *)base)[y * bw + x]; r = p & 255; g = (p >> 8) & 255; b = (p >> 16) & 255; }
          else {
            unsigned p = ((const unsigned short *)base)[y * bw + x];
            if (pf == PSP_DISPLAY_PIXEL_FORMAT_565) { r = (p & 31) << 3; g = ((p >> 5) & 63) << 2; b = (p >> 11) << 3; }
            else if (pf == PSP_DISPLAY_PIXEL_FORMAT_5551) { r = (p & 31) << 3; g = ((p >> 5) & 31) << 3; b = ((p >> 10) & 31) << 3; }
            else { r = (p & 15) << 4; g = ((p >> 4) & 15) << 4; b = ((p >> 8) & 15) << 4; }
          }
          o[0] = (unsigned char)b; o[1] = (unsigned char)g; o[2] = (unsigned char)r;
        }
      sceIoWrite(f, row, n * rowB);
    }
    free(row);
  }
  if (f >= 0) sceIoClose(f);
  snprintf(fn, sizeof fn, "%s/fe265_sweep.txt", dir);
  SceUID g = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
  if (g >= 0) {
    char s[128]; int l = snprintf(s, sizeof s, "SWEEP fe265_%c_%04d pass=%d idx=%d tl=%.2f mode=%d ok=%d fmt=%d ms=%.0f\n", pass ? 'b' : 'a', tl100,
                                  pass, idx, sweepPos[idx], mode, row != 0, pf, (sceKernelGetSystemTimeLow() - t0) / 1000.f);
    sceIoWrite(g, s, (unsigned)l); sceIoClose(g);
  }
}

// правка 293 (служебное fecap, frontend3d.h): настоящий перелёт по кадрам — GE копирует готовый кадр в ОЗУ, после перелёта на карту
void feCapGrab(void *fbAbs, int fb32) {
  if (!capArm || !capThis) return;
  capThis = 0;
  if (!capBuf) {                                            // первый кадр перелёта: память под кадры (нет — меньше кадров)
    capFb32 = fb32; capSlot = 480u * 272u * (fb32 ? 4u : 2u);
    for (int n = capWant < FE_CAP_MAX ? capWant : FE_CAP_MAX; n >= 2 && !capBuf; n /= 2) {
      capBuf = memalign(64, capSlot * (unsigned)n);
      if (capBuf) { capSlots = n; capHead = n * 2 / 3; if (capHead < 1) capHead = 1; capTail = n - capHead; }
    }
    if (!capBuf) { capArm = 0; if (feFrameLog) feFrameLog("FECAP nomem\n"); return; }
    sceKernelDcacheWritebackInvalidateRange(capBuf, capSlot * (unsigned)capSlots);   // строк кэша в нём нет: GE пишет мимо
  }
  int k = capSeen++, j = k - capFrom;                       // k — номер кадра перелёта; правка 325: первые fecapfrom не берём
  if (j < 0) return;
  int slot = j < capHead ? j : capTail ? capHead + (j - capHead) % capTail : -1;
  if (slot < 0) return;
  capK[slot] = k; capTl[slot] = capNowTl;
  sceGuCopyImage(capFb32 ? GU_PSM_8888 : GU_PSM_5650, 0, 0, 480, 272, 512, fbAbs, 0, 0, 480, capBuf + capSlot * (unsigned)slot);
}
void feCapFlush(void) {
  capArm = 0; capThis = 0;
  if (!capBuf) return;
  unsigned t0 = sceKernelGetSystemTimeLow(); int took = capSeen - capFrom > 0 ? capSeen - capFrom : 0, filled = took < capSlots ? took : capSlots, wrote = 0;
  sceKernelDcacheInvalidateRange(capBuf, capSlot * (unsigned)capSlots);
  char dir[224], fn[256]; snprintf(dir, sizeof dir, "%s/shots", feDir); sceIoMkdir(dir, 0777);
  int ord[FE_CAP_MAX]; for (int i = 0; i < filled; i++) ord[i] = i;
  for (int i = 1; i < filled; i++) { int x = ord[i], j = i - 1; while (j >= 0 && capK[ord[j]] > capK[x]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = x; }
  unsigned char *row = malloc(480 * 3 * 16);
  for (int q = 0; row && q < filled; q++) {
    int s = ord[q]; const unsigned char *src = capBuf + capSlot * (unsigned)s;
    snprintf(fn, sizeof fn, "%s/fe%d_%02d_%04d.bmp", dir, capTagNow, capK[s], (int)(capTl[s] * 100.f + .5f));
    SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) continue;
    int W = 480, Hh = 272; unsigned rowB = W * 3, size = 54 + rowB * Hh, v;
    unsigned char hd[54] = {'B', 'M'}; memcpy(hd + 2, &size, 4); v = 54; memcpy(hd + 10, &v, 4); v = 40; memcpy(hd + 14, &v, 4);
    memcpy(hd + 18, &W, 4); memcpy(hd + 22, &Hh, 4); hd[26] = 1; hd[28] = 24; v = rowB * Hh; memcpy(hd + 34, &v, 4);
    sceIoWrite(f, hd, 54);
    for (int y = Hh - 1; y >= 0;) {                         // BMP — снизу вверх, по 16 строк за запись (как sweepDump)
      int n = 0;
      for (int kk = 0; kk < 16 && y >= 0; kk++, y--, n++)
        for (int x = 0; x < W; x++) {
          unsigned char *o = row + n * rowB + x * 3; unsigned r, g, b;
          if (capFb32) { unsigned p = ((const unsigned *)src)[y * W + x]; r = p & 255; g = (p >> 8) & 255; b = (p >> 16) & 255; }
          else { unsigned p = ((const unsigned short *)src)[y * W + x]; r = (p & 31) << 3; g = ((p >> 5) & 63) << 2; b = (p >> 11) << 3; }
          o[0] = (unsigned char)b; o[1] = (unsigned char)g; o[2] = (unsigned char)r;
        }
      sceIoWrite(f, row, n * rowB);
    }
    sceIoClose(f); wrote++;
    if (feFrameLog) { char ln[160]; snprintf(ln, sizeof ln, "FECAP k=%d tl=%.3f file=%s\n", capK[s], capTl[s], strrchr(fn, '/') + 1); feFrameLog(ln); }
  }
  if (row) free(row);
  if (feFrameLog) { char ln[128]; snprintf(ln, sizeof ln, "FECAP done seen=%d slots=%d head=%d wrote=%d fb32=%d ms=%.0f\n", capSeen, capSlots, capHead,
                                        wrote, capFb32, (sceKernelGetSystemTimeLow() - t0) / 1000.f); feFrameLog(ln); }
  free(capBuf); capBuf = 0; capSeen = 0; capSlots = 0;
}

void feDraw(int sel) {
  if (!H) return;
  int gagActive = gagMenuActive && !flying;   // freeze also on the final incoming flight frame
  if (feGlowTest >= 0) sel = feGlowTest;
  capThis = capArm && flying;                               // правка 293 (fecap): этот кадр — кадр перелёта (последний тоже)
  unsigned t = sceKernelGetSystemTimeLow();
  float dt = lastT ? (t - lastT) * 1e-6f : 0.f; lastT = t;
  if (dt > .1f) dt = .1f;
  if (dt > 0.f) { fpsAcc += dt; fpsN++; }
  if (sweepOn) dt = 0.f;                                    // служебный покадровый перелёт (265): часы гостиной и Гомер стоят
  clockS += dt;
  feStats.pieces = feStats.tris = feStats.bills = 0; feStats.frames++; floorDrawn = 0; billNeed = billCut = 0;
  int swCode = -1;
  if (sweepOn) {                                            // одна поза на кадр рисования; проход 0 — прежний мип, 1 — текущий
    if (swPrev >= 0) sweepDump(swPrev);                     // кадр прошлой позы уже на экране (main: sceGuSync + смена буфера)
    int j = sweepK++, k = j < 2 * sweepN ? j : 2 * sweepN - 1;
    sweepPass = k / sweepN; tlPos = sweepPos[k % sweepN]; swCode = (k % sweepN) | sweepPass << 7;
    swPrev = j < 2 * sweepN ? swCode : -1;
    if (j >= 2 * sweepN) { sweepOn = 0; flying = 0; tlPos = tlEnd; }   // лишний кадр — только чтобы записать последний
  } else if (flying) { tlPos += dt * H->fps; if (tlPos >= tlEnd) { tlPos = tlEnd; flying = 0; } }   // перелёт: кадры данных @ 30
  float tl = tlPos; capNowTl = tl;
  int f0 = (int)tl, f1 = f0 + 1 < (int)H->nFrames ? f0 + 1 : f0; float u = tl - (float)f0;
  const float *ca = (const float *)(fd + H->offCam) + 12 * f0, *cb = (const float *)(fd + H->offCam) + 12 * f1,
              *cm = (const float *)(fd + H->offCam) + 12 * H->menuFrame;
  float cam[12]; for (int i = 0; i < 12; i++) cam[i] = ca[i] + (cb[i] - ca[i]) * u;   // между кадрами данных — линейно
  int home = 1;                                             // камера в главном виде (поза кадра меню)
  for (int i = 0; i < 9; i++) if (fabsf(cam[i] - cm[i]) > 2e-3f) home = 0;
  glowFade = 1.f;                                           // подсветки совпадают с предметами только из главного вида; правка 293:
  if (home) glowOffT = -1.f;                                // ушли из него — подсветка пункта гаснет FE_GLOW_FADE_S (камера за это
  else if (feGlowCut || sweepOn) sel = -1;                  // время сдвигается на 2-3 см, как у оригинала), потом не рисуется
  else {
    if (glowOffT < 0.f) glowOffT = clockS;
    glowFade = 1.f - (clockS - glowOffT) / FE_GLOW_FADE_S;
    if (glowFade <= 0.f) { glowFade = 1.f; sel = -1; }
  }
  memcpy(cp, cam, 12); memcpy(cf, cam + 3, 12); norm3(cf);
  float up[3] = {cam[6], cam[7], cam[8]}; cross3(cr, cf, up); norm3(cr); cross3(cu, cr, cf);
  tvy = tanf(cam[9] * .5f) * .75f; tvx = tvy * (16.f / 9.f);   // fov оригинала — по горизонтали при 4:3; Hor+
  cnear = cam[10];
  mipK = tvy / 272.f * powf(2.f, FE_MIP_LIFT);              // правка 265: GE SLOPE — уровень = log2(2·w·наклон) (PPSSPP по замерам железа)
  if (haveHomer) {
    int hold = homerGag >= 0 && feGagAt >= 0.f && gagShown < 0;   // служебное: поза сценки Гомера на feGagAt
    if (hold) { hp->t = feGagAt; skinPose(hp); } else skinUpdate(hp, dt);
    if (homerGag >= 0 && !hold && hp->t * 30.f >= FE_HOMER_GAG_SEG[homerGag][1]) {   // отрезок доигран: свой цикл с кадра 0
      if (feGagLog) { char ln[64]; snprintf(ln, sizeof ln, "FEGAG end i=0 seg=%d\n", homerGag); feGagLog(ln); }
      homerGag = -1; skinPlay(hp, 0, 1, 0.f);
    }
  }
  if (ngag) {                                               // №404: расписание сценок (SLES_518.97, см. FE_GAG_FIRST_MS)
    if (feGagForce >= 0 && feGagForce < ngag) { int k = feGagForce; feGagForce = -1; gagStart(k, 1); }
    else if (feGagAuto && !sweepOn) {
      unsigned ev = feGagClockStep(&gagClock, (unsigned)(dt * 1000.f + .5f), gagActive, flying, ngag, gagRand);
      if (ev & FE_GAG_PREPARE) {
        if (feGagEvent) feGagEvent(180, gagClock.pending, gagClock.segment);
        if (feGagLog) { char ln[128]; snprintf(ln, sizeof ln, "FEGAG prepare i=%d seg=%d elapsed_ms=%u next_ms=%u\n", gagClock.pending, gagClock.segment, gagClock.elapsed, gagClock.next); feGagLog(ln); }
      }
      if (ev & FE_GAG_PLAY) {
        int k = gagClock.pending >= 0 ? gagClock.pending : (int)((gagRand() / 10u % 97u) % (unsigned)ngag);
        gagStart(k, 0);
      }
    }
    if (gagShown >= 0) {
      if (feGagAt >= 0.f) { gp->t = feGagAt; skinPose(gp); }   // служебное: поза на feGagAt
      else if (gagCur >= 0) {
        skinUpdate(gp, dt);
        if (gp->t >= skinClipLength(&gm[gagCur], 0)) {          // доиграла: стоит на последнем кадре до следующей
          if (feGagLog) { char ln[64]; snprintf(ln, sizeof ln, "FEGAG end i=%d\n", gagCur); feGagLog(ln); }
          gagCur = -1;
        }
      }
    }
  }
  feStats.gag = gagShown >= 0 ? gagShown : homerGag >= 0 ? 0 : -1; feStats.gagT = gagShown >= 0 ? gp->t : homerGag >= 0 ? hp->t : 0.f;
  feStats.gagElapsedMs = gagClock.elapsed; feStats.gagNextMs = gagClock.next;
  feStats.gagPending = gagClock.pending; feStats.gagMenuActive = gagActive;
  if (feGagLog) { char ln[160]; snprintf(ln, sizeof ln, "FEGCLOCK ms=%u next=%u pending=%d active=%d flying=%d\n", gagClock.elapsed, gagClock.next, gagClock.pending, gagActive, flying); feGagLog(ln); }

  sceGuClearDepth(0); sceGuClear(GU_DEPTH_BUFFER_BIT);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuDisable(GU_FOG); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_LIGHTING);
  sceGuShadeModel(GU_SMOOTH);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f);
  sceGuAlphaFunc(GU_GREATER, 0x40, 0xff);
  sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
  sceGumPerspective(2.f * atanf(tvy) * 180.f / GU_PI, 16.f / 9.f, cam[10], cam[11]);
  sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
  ScePspFVector3 eye = {cp[0], cp[1], cp[2]}, at = {cp[0] + cf[0], cp[1] + cf[1], cp[2] + cf[2]}, u3 = {up[0], up[1], up[2]};
  sceGumLookAt(&eye, &at, &u3);
  sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); sceGumUpdateMatrix();
  bound = -2; curFl = -1; curLvl = -1; curSl = -1.f;

  // порядок страницы 3dFE.pag (Scrooby рисует объекты сверху вниз): комната (непрозрачное, потом смешиваемое) и
  // подсветки ТВ/дивана — F_PREHOMER, потом Гомер (пишет глубину), потом остальные подсветки; смешиваемое внутри
  // каждой группы — от дальнего к ближнему. Старый файл без F_PREHOMER: всё смешиваемое после Гомера, как раньше
  int nb[2] = {0, 0};
  for (unsigned i = 0; i < H->nPiece; i++) {
    const FePiece *p = &pc[i];
    if (p->vis != 0xffff) continue;                          // видимость по таймлайну: в кадре меню все такие скрыты (экспорт)
    if (home && (p->flags & F_MENUHID)) continue;            // не виден из камеры меню (экспорт: буфер глубины из этой точки)
    const float *r = p->node != 0xffff ? trMat(p->node, tl) : 0;
    float d;
    if (!boxVisible(p->bbox, r, &d)) continue;
    if (p->flags & (F_BLEND | F_ADD)) { int g = (p->flags & F_PREHOMER) ? 0 : 1; if (nb[g] < MAXP) { order[g][nb[g]] = (short)i; dist[g][nb[g]++] = d; } continue; }
    drawPiece(p, tl);
  }
  for (unsigned i = 0; i < H->nBill; i++) {
    const FeBill *b = &bl[i];
    if (b->glow != 0xff && b->glow != sel) continue;
    int g = (b->flags & F_PREHOMER) ? 0 : 1; if (b->glow != 0xff && feHeroOn()) g = 1;   // №1021-герои: подсветка выбранного пункта — после Гомера и его контура, поверх
    if (nb[g] >= MAXP) continue;
    const float *m = b->m; float d[3] = {m[3] - cp[0], m[7] - cp[1], m[11] - cp[2]};
    order[g][nb[g]] = (short)(-1 - (int)i); dist[g][nb[g]++] = d[0] * cf[0] + d[1] * cf[1] + d[2] * cf[2];
  }
  if (gagShown >= 0) drawChr(&gm[gagShown], gp, gagRec[gagShown].place, gagEyeTex());   // №404: страница 3dFEGags.pag — до смешиваемого комнаты
  for (int g = 0; g < 2; g++) {
    short *o = order[g]; float *ds = dist[g]; int n = nb[g];
    for (int i = 1; i < n; i++) {                          // вставками: дальние первыми
      short oi = o[i]; float x = ds[i]; int j = i - 1;
      while (j >= 0 && ds[j] < x) { o[j + 1] = o[j]; ds[j + 1] = ds[j]; j--; }
      o[j + 1] = oi; ds[j + 1] = x;
    }
    if (g == 1 && haveHomer) drawHomer();
    for (int i = 0; i < n; i++) { if (o[i] >= 0) drawPiece(&pc[o[i]], tl); else drawBill(&bl[-1 - o[i]], sel); }
  }
  sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_BLEND); sceGuDepthMask(GU_FALSE);
  sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f);
  if (swCode >= 0) sweepMark(swCode);
  float cpu = (sceKernelGetSystemTimeLow() - t) / 1000.f; cpuAcc += cpu;
  if (feFrameLog && (flying || homeLast != home)) {   // правка 293: что нарисовано в каждом кадре перелёта (на PSP — в smoke.txt тура)
    char ln[176]; snprintf(ln, sizeof ln, "FEFR tl=%.3f dt=%.1f cpu=%.1f pc=%d tr=%d floor=%d home=%d bills=%d bn=%d bc=%d\n", tlPos, dt * 1000.f, cpu,
                           feStats.pieces, feStats.tris, floorDrawn, home, feStats.bills, billNeed, billCut);   // правка 325: bn bc
    feFrameLog(ln); }
  homeLast = home;
  if (fpsN) feStats.fps = fpsN / fpsAcc;
  feStats.cpuMs = cpuAcc / feStats.frames;
}
