// Частицы: реализация (см. fx.h, формат — tools/fx_pack.py, данные — docs/fx_spec.md).
// Правка 313: шаг жизни частиц и сборка биллбордов — viewer/fxdsp.h (одна функция для главного процессора и Media Engine);
// здесь — пакет, системы, рождение частиц и команды GE. Кадр: fxUpdate (рождение) -> fxDrawBegin (задание: на ME или отложено) ->
// [главный рисует отложенное полупрозрачное, main.c tqFlush] -> fxDrawEnd (итог: с ME или счёт на главном; команды GE)
#include "fx.h"
#include "fxdsp.h"
#include "sparkle937.h"
#include "glintshape941.h"   // №941
#include <pspgu.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>

typedef struct { char magic[4]; unsigned ver, nTex, nSys, nEmit, nKey, nBB, nAmb, nBrk,
                 offTex, offSys, offEmit, offKey, offBB, offAmb, offBrk; } FxHdr;
typedef struct { unsigned short w, h, bufw; unsigned char psm, swz; unsigned offPix, offClut; char name[16]; } FxTex;
typedef struct { char name[32]; unsigned short firstEmit, nEmit, nFrames; unsigned char oneShot, maxInst; float fps;
                 short instType, brkType; unsigned short firstBB, nBB; unsigned pad; } FxSys;
typedef struct { unsigned short sys, pad; float x, y, z; } FxAmb;
typedef struct { short type; unsigned short nSys, sys[4], firstBB, nBB; } FxBrk;
_Static_assert(sizeof(FxHdr) == 64 && sizeof(FxTex) == 32 && sizeof(FxSys) == 56 && sizeof(FxEmit) == 468 &&
               sizeof(FxKey) == 8 && sizeof(FxBB) == 32 && sizeof(FxAmb) == 16 && sizeof(FxBrk) == 16, "FXP1");

static unsigned char *blob;
static FxHdr *H; static FxTex *T; static FxSys *S; static FxEmit *E; static FxKey *K; static FxBB *BB;
static FxAmb *AM; static FxBrk *BR;
#ifndef FX_BOOM_OLD
#define FX_BOOM_OLD 0
#endif
static int explosionOld = FX_BOOM_OLD;
void fxExplosionOld(int on) { explosionOld = on; }

// частица (viewer/fxdsp.h FxPart); правка 313: частицы живут в задании FJ (FJ.ps, FJ.np) — его считает главный или ME
typedef FxPart P;
static FxJob fjA; static FxJob *fjB, *fjCur = &fjA;   // правка 313: задание кадра (fjB — запасное, берётся с ME: ME не ответил)
#define FJ (*fjCur)

// экземпляр системы
typedef struct { short sys; unsigned char used, spawning, once, pad; unsigned short gen; float t, bbT, rate, age;
                 float pos[3], m[9]; float acc[MAXE], ek[MAXE]; unsigned short live[MAXE]; int nlive; } Inst;
static Inst in_[FX_MAX_INST];
static unsigned short genCtr = 1;

typedef FxSpr Spr;
static Spr spr[MAXSPR]; static int nspr;   // спрайты кадра копируются в задание в fxDrawBegin

float fxVel = 30.f;
#ifndef FX_STAR923_OLD
#define FX_STAR923_OLD 0
#endif
static unsigned char texVec[64];   // №899: 1 — картинка блика (звезда), 2 — искра-штрих (fxVecGroup); №923: 3 — цветная звезда-царапина (scratch2_blue / _red)
static unsigned texTint[64];       // №923: цвет картинки (множитель ABGR) у texVec 3: у родной картинки он свой, в векторе был белый
static int ready, sysCar = -1, sysSpark = -1, sysPaint = -1, sysDust = -1, texScratch = -1, texGlow = -1, texDust1 = -1, texCloud = -1, texSmoke953[4] = {-1, -1, -1, -1};   // texDust1/texCloud — №847; texSmoke953 — smoke1/2/3/3alph (№953)
#ifndef FX_SOFTDUST847_OLD
#define FX_SOFTDUST847_OLD 0
#endif
static float eye[3]; static int haveEye;
static int ambH[64];

static unsigned rs = 777u;
static inline float fr(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.f / 16777216.f); }
static inline float fr2(void) { return fr() * 2.f - 1.f; }

// ---------- загрузка ----------
#include "pfetch.h"
__attribute__((weak)) void *(*pfTakeHook)(const char *path, unsigned *size);   // pfetch.h; слабый — хост-гейты линкуются без движка
int fxLoad(const char *path) {
  unsigned psz = 0; unsigned char *b = pfTakeHook ? (unsigned char *)pfTakeHook(path, &psz) : 0; int sz, rd;   // файл уже прочитан загрузчиком в фоне
  if (b) { sz = rd = (int)psz; }
  else {
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
  if (fd < 0) return 0;
  sz = sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET);
  b = memalign(16, sz);
  if (!b) { sceIoClose(fd); return 0; }
  rd = sceIoRead(fd, b, sz); sceIoClose(fd); }
  if (rd != sz || memcmp(b, "FXP1", 4)) { free(b); return 0; }
  if (blob) free(blob);
  blob = b; H = (FxHdr *)b;
  T = (FxTex *)(b + H->offTex); S = (FxSys *)(b + H->offSys); E = (FxEmit *)(b + H->offEmit);
  K = (FxKey *)(b + H->offKey); BB = (FxBB *)(b + H->offBB); AM = (FxAmb *)(b + H->offAmb); BR = (FxBrk *)(b + H->offBrk);
  for (unsigned r = 0; r < H->nBrk; r++) if (BR[r].type == 24)
    for (unsigned k = 0; k < BR[r].nBB; k++) { unsigned i = BR[r].firstBB + k; if (i < H->nBB && BB[i].mode == 0) BB[i].mode = 2; }
  sceKernelDcacheWritebackRange(b, sz);
  ready = 1;
  sysCar = fxFindInst(14); if (sysCar < 0) sysCar = fxFind("car_damage3SysShape");
  sysSpark = fxFind("code_spark"); sysPaint = fxFind("code_paint"); sysDust = fxFind("code_dust");
  texScratch = fxTexFind("scratch2.bmp"); texGlow = fxTexFind("glow2.bmp"); texDust1 = fxTexFind("dust1.bmp"); texCloud = fxTexFind("cloud.tga");   // №847
  texSmoke953[0] = fxTexFind("smoke1.bmp"); texSmoke953[1] = fxTexFind("smoke2.bmp"); texSmoke953[2] = fxTexFind("smoke3.bmp"); texSmoke953[3] = fxTexFind("smoke3alph.bmp");   // №953
  memset(texVec, 0, sizeof texVec);   // №899: картинки бликов и искр — векторно (ниже fxVecGroup)
  for (unsigned i = 0; i < H->nTex && i < 64; i++) { const char *n = T[i].name;
    if (!FX_STAR923_OLD && !strncmp(n, "scratch2_blue", 13)) { texVec[i] = 3; texTint[i] = 0xffffc740u; }   // картинка: центр белый, тело (R .25 G .78 B 1) — по весу видимости пикселей tools/fx_export.py --png
    else if (!FX_STAR923_OLD && !strncmp(n, "scratch2_red", 12)) { texVec[i] = 3; texTint[i] = 0xff4080ffu; }   // (R 1 G .5 B .25)
    else if (!strncmp(n, "scratch2", 8) || !strncmp(n, "glow", 4) || !strncmp(n, "bluestarglow", 12) || !strncmp(n, "halo", 4) || !strncmp(n, "spark4", 6)) texVec[i] = 1;
    else if (!strncmp(n, "spark3", 6)) texVec[i] = 2; }
  if (!FX_SPARKLE937_OLD) {   // №937: эмиттеры со звездой-sparkle — флаг попа (fxdsp.h fxdPop)
    for (unsigned e = 0; e < H->nEmit; e++) if (E[e].tex < 64 && texVec[E[e].tex] == 3) E[e].flags |= 0x40;
    sceKernelDcacheWritebackRange(b, sz);
  }
  return 1;
}

void fxInit(int level) {
  FJ.np = 0; nspr = 0; memset(in_, 0, sizeof in_); memset(ambH, 0, sizeof ambH);
  if (ready) return;
  // частицы уровня: у уровня 1 — fx.shr рядом с EBOOT, у 2–7 — l<N>/fx.shr (tools/fx_pack.py --level N; viewer/main.c levelFile)
  char nm[32], p[304]; if (level > 1) snprintf(nm, sizeof nm, "l%d/fx.shr", level); else strcpy(nm, "fx.shr");
  if (getcwd(p, 256)) { size_t l = strlen(p); if (l && p[l - 1] != '/') strcat(p, "/"); strcat(p, nm); if (fxLoad(p)) return; }
  if (fxLoad(nm)) return;
  snprintf(p, sizeof p, "ms0:/PSP/GAME/SHARVIEW/%s", nm); fxLoad(p);
}
int fxReady(void) { return ready; }
// смена уровня (viewer/main.c levelUnload): пакет частиц уровня — обратно в кучу; fxInit(N) прочитает l<N>/fx.shr
void fxFree(void) {
  FJ.np = 0; nspr = 0; memset(in_, 0, sizeof in_); memset(ambH, 0, sizeof ambH);
  if (blob) free(blob);
  blob = 0; H = 0; ready = 0;
  sysCar = sysSpark = sysPaint = sysDust = texScratch = texGlow = texDust1 = texCloud = -1; texSmoke953[0] = texSmoke953[1] = texSmoke953[2] = texSmoke953[3] = -1;
}

int fxFind(const char *name) {
  if (!ready) return -1;
  for (unsigned i = 0; i < H->nSys; i++) if (!strncmp(S[i].name, name, 31)) return i;
  return -1;
}
int fxFindInst(int type) {
  if (!ready) return -1;
  for (unsigned i = 0; i < H->nSys; i++) if (S[i].instType == type) return i;
  return -1;
}
int fxTexFind(const char *name) {
  if (!ready) return -1;
  for (unsigned i = 0; i < H->nTex; i++) if (!strncmp(T[i].name, name, 15)) return i;
  return -1;
}

// ---------- кривые ----------
static float key(const FxEmit *e, int p, float t) {
  const FxKey *k = K + e->key[p][0]; int n = e->key[p][1];
  if (n <= 1 || t <= k[0].frame) return k[0].v;
  for (int i = 1; i < n; i++)
    if (t <= k[i].frame) { float f0 = k[i - 1].frame, f1 = k[i].frame; return k[i - 1].v + (k[i].v - k[i - 1].v) * (t - f0) / (f1 - f0); }
  return k[n - 1].v;
}
#define qrot fxdQrot   // правка 313: кривые, цвет и поворот — viewer/fxdsp.h (тот же код)
static void basis(const float dir[3], float m[9]) {   // столбцы x, y, z; y = dir
  float y[3] = {0, 1, 0};
  if (dir) { float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]); if (l > 1e-5f) { y[0] = dir[0] / l; y[1] = dir[1] / l; y[2] = dir[2] / l; } }
  float r[3] = {0, 0, 1}; if (fabsf(y[2]) > .9f) { r[0] = 1; r[2] = 0; }
  float x[3] = {y[1] * r[2] - y[2] * r[1], y[2] * r[0] - y[0] * r[2], y[0] * r[1] - y[1] * r[0]};
  float l = sqrtf(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]); x[0] /= l; x[1] /= l; x[2] /= l;
  float z[3] = {x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]};
  m[0] = x[0]; m[1] = x[1]; m[2] = x[2]; m[3] = y[0]; m[4] = y[1]; m[5] = y[2]; m[6] = z[0]; m[7] = z[1]; m[8] = z[2];
}
static inline void mv(const float m[9], const float v[3], float o[3]) {
  o[0] = m[0] * v[0] + m[3] * v[1] + m[6] * v[2]; o[1] = m[1] * v[0] + m[4] * v[1] + m[7] * v[2]; o[2] = m[2] * v[0] + m[5] * v[1] + m[8] * v[2];
}
static const float I9[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

// ---------- рождение ----------
// vel: NULL = из эмиттера; addv: добавить к скорости (машина едет); ak: множитель альфы
static P *spawn(int ei, int inst, float t, float fps, const float pos[3], const float m[9], const float *vel,
                const float *addv, float ak) {
  if (FJ.np >= FX_MAX) return NULL;
  const FxEmit *e = &E[ei];
  P *p = &FJ.ps[FJ.np++];
  float q[4] = {key(e, P_QW, t), key(e, P_QX, t), key(e, P_QY, t), key(e, P_QZ, t)};
  float ql = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]); if (ql < 1e-6f) { q[0] = 1; ql = 1; }
  q[0] /= ql; q[1] /= ql; q[2] /= ql; q[3] /= ql;
  static const float X[3] = {1, 0, 0}, Y[3] = {0, 1, 0}, Z[3] = {0, 0, 1};
  float ax[3], ay[3], az[3]; qrot(q, X, ax); qrot(q, Y, ay); qrot(q, Z, az);
  float lp[3] = {key(e, P_TX, t), key(e, P_TY, t), key(e, P_TZ, t)};
  if (e->gen == 1) {           // плоскость W×H поперёк оси вылета
    float a = fr2() * .5f * e->genW, b = fr2() * .5f * e->genH;
    for (int i = 0; i < 3; i++) lp[i] += ax[i] * a + az[i] * b;
  } else if (e->gen == 2) {    // сфера радиуса R
    float zz = fr2(), ph = fr() * 6.2831853f, rr = sqrtf(1.f - zz * zz);
    lp[0] += e->genW * rr * cosf(ph); lp[1] += e->genW * zz; lp[2] += e->genW * rr * sinf(ph);
  }
  float th = fr2() * e->spreadH, tv = fr2() * e->spreadV, ch = cosf(th), sh = sinf(th), cv = cosf(tv), sv = sinf(tv);
  float ld[3]; for (int i = 0; i < 3; i++) ld[i] = ay[i] * ch * cv + ax[i] * sh * cv + az[i] * sv;
  float wp[3], wd[3]; mv(m, lp, wp); mv(m, ld, wd);
  p->x = pos[0] + wp[0]; p->y = pos[1] + wp[1]; p->z = pos[2] + wp[2];
  if (vel) { p->vx = vel[0]; p->vy = vel[1]; p->vz = vel[2]; }
  else {
    float sp = (key(e, P_SPD, t) + key(e, P_SPDV, t) * fr2()) * fxVel;
    p->vx = wd[0] * sp; p->vy = wd[1] * sp; p->vz = wd[2] * sp;
  }
  if (addv) { p->vx += addv[0]; p->vy += addv[1]; p->vz += addv[2]; }
  p->vg = 0.f;
  float wgt = key(e, P_WGT, t) + key(e, P_WGTV, t) * fr2();
  p->ga = key(e, P_GRA, t) * wgt * fxVel * fxVel;
  float life = (key(e, P_LIF, t) + key(e, P_LIFV, t) * fr2()) / fps; if (life < .02f) life = .02f;
  p->age = 0.f; p->inv = 1.f / life;
  p->size = key(e, P_SIZ, t) + key(e, P_SIZV, t) * fr2(); if (p->size < .01f) p->size = .01f;
  p->spin = (key(e, P_SPI, t) + key(e, P_SPIV, t) * fr2()) * fps;
  p->ang = e->angleMode == 1 ? fr() * 6.2831853f : e->angle;
  float dra = key(e, P_DRA, t); p->lnd = (dra > 0.f && dra < .9999f) ? logf(dra) * 30.f : 0.f;
  float alp = key(e, P_ALP, t) + key(e, P_ALPV, t) * fr2(); if (alp < 0) alp = 0; if (alp > 255) alp = 255;
  p->ak = ak * alp * (1.f / 255.f);
  float br = 1.f + key(e, P_CLRV, t) * (1.f / 255.f) * fr2();
  int r = key(e, P_CLRR, t) * br, g = key(e, P_CLRG, t) * br, b = key(e, P_CLRB, t) * br;
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  p->rgb = (b << 16) | (g << 8) | r;
  p->em = ei; p->inst = inst < 0 ? 0xffff : inst; p->seed = (unsigned char)(fr() * 255.f);
  if (inst >= 0) { in_[inst].live[ei - S[in_[inst].sys].firstEmit]++; in_[inst].nlive++; }
  return p;
}

// ---------- системы ----------
static int handleOf(int i) { return (in_[i].gen << 8) | (i + 1); }
static Inst *fromHandle(int h) {
  int i = (h & 255) - 1; if (i < 0 || i >= FX_MAX_INST) return NULL;
  Inst *s = &in_[i]; return (s->used && s->gen == (unsigned short)(h >> 8)) ? s : NULL;
}
static int start(int id, const float pos[3], const float dir[3], int once) {
  if (!ready || id < 0 || id >= (int)H->nSys) return 0;
  int slot = -1, cnt = 0, oldest = -1; float oa = -1.f;
  for (int i = 0; i < FX_MAX_INST; i++) {
    if (!in_[i].used) { if (slot < 0) slot = i; continue; }
    if (in_[i].sys == id && in_[i].once) { cnt++; if (in_[i].age > oa) { oa = in_[i].age; oldest = i; } }
  }
  if (once && cnt >= S[id].maxInst && oldest >= 0) in_[oldest].spawning = 0;   // потолок maxInstances: гасим старейшую
  if (slot < 0) return 0;
  Inst *s = &in_[slot]; memset(s, 0, sizeof *s);
  s->sys = id; s->used = 1; s->spawning = 1; s->once = once || S[id].oneShot; s->rate = 1.f;
  s->gen = genCtr++; if (!genCtr) genCtr = 1;
  for (int k = 0; k < MAXE; k++) s->ek[k] = 1.f;
  s->pos[0] = pos[0]; s->pos[1] = pos[1]; s->pos[2] = pos[2]; basis(dir, s->m);
  return handleOf(slot);
}
int fxStartSystem(int id, const float pos[3], const float dir[3]) { return start(id, pos, dir, 0); }
int fxStart(const char *name, const float pos[3], const float dir[3]) { return start(fxFind(name), pos, dir, 0); }
int fxBurst(int id, const float pos[3], const float dir[3]) { return start(id, pos, dir, 1); }
// правка Роба 80: окно EFX предмета «кадры 80..95» — система с кадра 80 (как контроллер оригинала), а не с нуля (развал ящика
// запаздывал на ~2.7 с: эмиттеры системы ящика работают только на её кадрах 80..95)
int fxBurstAt(int id, const float pos[3], const float dir[3], float frame) {
  int h = start(id, pos, dir, 1); if (!h || frame <= 0.f) return h;
  Inst *s = &in_[(h & 0xFF) - 1]; float nf = (float)S[id].nFrames; s->t = frame < nf ? frame : 0.f;
  return h;
}
int fxBurstBreakable(int type, const float pos[3], const float dir[3]) {
  if (!ready) return 0;
  for (unsigned i = 0; i < H->nBrk; i++) if (BR[i].type == type) {
    int h = 0;
    for (int k = 0; k < BR[i].nSys && k < 4; k++) { int x = start(BR[i].sys[k], pos, dir, 1); if (!h) h = x; }
    return h;
  }
  return 0;
}
// №923 (Роб 24.10: «бью вперёд, а разбивается влево»): родные системы обломка (осколки стекла, щепа) заданы в осях предмета и летят вдоль его нормали; порт ставил их
// по осям мира (dir NULL) — у стекла витрины, повёрнутого на 90°, осколки шли вдоль стекла. Система ставится с поворотом вокруг вертикали: её локальная +Z — по
// направлению yaw (atan2(vx, vz) скорости машины). FXB_YAW_OFS — поправка знака оси разлёта эмиттеров
#ifndef FXB_YAW_OFS
#define FXB_YAW_OFS 0.f
#endif
int fxBurstBreakableYaw(int type, const float pos[3], float yaw) {
  if (!ready) return 0;
  float c = cosf(yaw + FXB_YAW_OFS), s = sinf(yaw + FXB_YAW_OFS);
  for (unsigned i = 0; i < H->nBrk; i++) if (BR[i].type == type) {
    int h = 0;
    for (int k = 0; k < BR[i].nSys && k < 4; k++) {
      int x = start(BR[i].sys[k], pos, NULL, 1);
      Inst *in = x ? fromHandle(x) : NULL;
      if (in) { in->m[0] = c; in->m[1] = 0.f; in->m[2] = -s; in->m[3] = 0.f; in->m[4] = 1.f; in->m[5] = 0.f; in->m[6] = s; in->m[7] = 0.f; in->m[8] = c; }
      if (!h) h = x;
    }
    return h;
  }
  return 0;
}
void fxMove(int h, const float pos[3], const float dir[3]) {
  Inst *s = fromHandle(h); if (!s) return;
  s->pos[0] = pos[0]; s->pos[1] = pos[1]; s->pos[2] = pos[2]; if (dir) basis(dir, s->m);
}
void fxSetRate(int h, float k) { Inst *s = fromHandle(h); if (s) s->rate = k; }
void fxSetEmitRate(int h, int e, float k) { Inst *s = fromHandle(h); if (s && e >= 0 && e < MAXE) s->ek[e] = k; }
void fxStop(int h) { Inst *s = fromHandle(h); if (s) s->spawning = 0; }
int fxAlive(int h) { return fromHandle(h) != NULL; }

// ---------- старый API ----------
void fxSpawn(int kind, float x, float y, float z, float vx, float vy, float vz) {
  if (!ready) return;
  float pos[3] = {x, y, z}, add[3] = {vx, 0.f, vz}, vel[3] = {vx, vy, vz};
  switch (kind) {
    case FX_SMOKE: if (sysCar >= 0) spawn(S[sysCar].firstEmit, -1, 0, S[sysCar].fps, pos, I9, NULL, add, .6f); break;
    case FX_DARK:  if (sysCar >= 0) spawn(S[sysCar].firstEmit, -1, 0, S[sysCar].fps, pos, I9, NULL, add, 1.f); break;
    case FX_FIRE:
      if (sysCar >= 0 && S[sysCar].nEmit >= 3)
        for (int k = 0; k < 4; k++) spawn(S[sysCar].firstEmit + 1 + (k & 1), -1, 0, S[sysCar].fps, pos, I9, NULL, add, 1.f);
      break;
    case FX_SPARK: if (sysSpark >= 0) spawn(S[sysSpark].firstEmit, -1, 0, 30.f, pos, I9, vel, NULL, 1.f); break;
    case FX_PAINT: if (sysPaint >= 0) spawn(S[sysPaint].firstEmit, -1, 0, 30.f, pos, I9, vel, NULL, 1.f); break;
    case FX_DUST:  if (sysDust >= 0) spawn(S[sysDust].firstEmit, -1, 0, 30.f, pos, I9, vel, NULL, 1.f); break;
  }
}

int fxHomerDust(int kind, float x, float y, float z, float power) {
  if (!ready || sysDust < 0) return 0;
  if (kind == FXD_STEP) { fxSpawn(FX_DUST, x, y + .08f, z, (fr() - .5f) * .4f, .2f + fr() * .15f, (fr() - .5f) * .4f); return 1; }   // клуб на шаг: почти на месте
  int n = kind == FXD_BOUNCE ? 5 : (power > FXD_LAND_MIN ? (power > 14.f ? 4 : 3) : 0);
  float sp = kind == FXD_BOUNCE ? 1.2f : .5f + .04f * fminf(power, 20.f);   // разлёт вбок, м/с
  for (int k = 0; k < n; k++) {
    float a = (k + fr() * .6f) * (6.2831853f / (float)n);
    fxSpawn(FX_DUST, x + cosf(a) * .25f, y + .08f, z + sinf(a) * .25f, cosf(a) * sp, .25f + fr() * .2f, sinf(a) * sp);
  }
  return n;
}

// ---------- обновление ----------
static void ambient(void) {
  if (!haveEye || !H->nAmb) return;
  for (unsigned i = 0; i < H->nAmb && i < 64; i++) {
    float dx = AM[i].x - eye[0], dy = AM[i].y - eye[1], dz = AM[i].z - eye[2], d2 = dx * dx + dy * dy + dz * dz;
    if (!ambH[i] && d2 < 60.f * 60.f) { float p[3] = {AM[i].x, AM[i].y, AM[i].z}; ambH[i] = start(AM[i].sys, p, NULL, 0); }
    else if (ambH[i] && (d2 > 70.f * 70.f || !fxAlive(ambH[i]))) { fxStop(ambH[i]); ambH[i] = 0; }
  }
}

// правка 313: шаг жизни частиц (прежний цикл «частицы» здесь) считает задание FJ в fxDrawBegin/fxDrawEnd — на ME или на главном;
// порядок прежний: рождение по таймлайну, затем шаг всех частиц с тем же dt. Второй fxUpdate без fxDraw — шаг считается сразу
static int simPend;
static void applyDead(void) {   // умершие частицы задания -> счётчики live экземпляров (как раньше внутри цикла частиц)
  for (int k = 0; k < FJ.ndead; k++) { Inst *s = &in_[FJ.dead[k].inst]; s->live[FJ.dead[k].em - S[s->sys].firstEmit]--; s->nlive--; }
  FJ.ndead = 0;
}
// №976 (Роб 25.10: «точка, куда надо дойти: голубой кружок на полу — проверить, что векторный, ровный край; звёздочки, что танцуют спиралью как ДНК и исчезают, когда заходишь, — слишком
// насыщенные, прозрачнее и доработать движение»): кружок — vecGlowHook (lightsprite.h), уже векторный, но 18 сторон: у сферы цели — SPHERE976_N сторон, пока ближе VG_LOD_D; столб звёзд
// (collectfx.c) — прозрачность COL976_A и ход по канону кинематики. GLINT976_OLD=1 / flag glint976old 1 — как TEST66
#ifndef GLINT976_OLD
#define GLINT976_OLD 0
#endif
int fxGlint976Old = GLINT976_OLD;
int fxGlintOne970;   // №970 (fx.h)
static int coinSat970;   // №970: сколько монет кадра уже получили три звезды (сброс в fxUpdate)
void fxUpdate(float dt) {
  coinSat970 = 0;   // №970: новый кадр — счёт монет с тремя звёздами заново
  if (!ready) return;
  if (dt > .1f) dt = .1f;
  if (simPend) { FJ.E = E; FJ.stage = FXD_SIM; fxdRun(&FJ); applyDead(); simPend = 0; }
  ambient();
  // системы: рождение по таймлайну
  for (int i = 0; i < FX_MAX_INST; i++) {
    Inst *s = &in_[i]; if (!s->used) continue;
    const FxSys *y = &S[s->sys];
    float df = dt * y->fps;
    s->age += dt; s->bbT += df;
    if (s->spawning) {
      for (int k = 0; k < y->nEmit && k < MAXE; k++) {
        const FxEmit *e = &E[y->firstEmit + k];
        float rat = key(e, P_RAT, s->t) * s->rate * s->ek[k];
        if (rat <= 0.f) continue;
        s->acc[k] += rat * df;
        int n = (int)s->acc[k]; s->acc[k] -= n;
        int num = (int)key(e, P_NUM, s->t);
        while (n-- > 0 && s->live[k] < num && FJ.np < FX_MAX) spawn(y->firstEmit + k, i, s->t, y->fps, s->pos, s->m, NULL, NULL, 1.f);
      }
      s->t += df;
      if (s->t >= y->nFrames) { if (s->once) s->spawning = 0; else s->t = fmodf(s->t, (float)y->nFrames); }
    }
    int bbDone = 1;
    for (int b = 0; b < y->nBB; b++) if (s->bbT < BB[y->firstBB + b].nFrames) bbDone = 0;
    if (!s->spawning && s->nlive <= 0 && bbDone) s->used = 0;
  }
  FJ.dt = dt; simPend = 1;   // частицы — в задании кадра (fxDrawBegin)
}

int fxCount(void) { return FJ.np; }

// ---------- спрайты ----------
// №731 (видео IMG_7280 9:02–9:29, Роб: «что полупрозрачное, как вода из гидранта, — спрайт как в Doom, вырезанный по краю, анимированный, всегда ко мне
// лицом»): частицы со смешиванием (blend 0: дым, вода, пыль) рисуются альфа-тестом без смешивания — край вырезан по альфе текстуры × альфе вершины, при
// угасании частица уменьшается, а не тает; сортировка и чтение кадра не нужны. Сложение (огонь, искры) и вычитание — как были. FX_CUTOUT_OLD=1 / «flag fxcut 0» — прежнее
#ifndef FX_CUTOUT_OLD
#define FX_CUTOUT_OLD 0
#endif
#ifndef FX_CUT_REF
#define FX_CUT_REF 0x58
#endif
int fxCutout = !FX_CUTOUT_OLD;
// №953 (Роб 25.10, TEST59: «дым, машина слегка разбита: в уголках чёрные заливки; в мультике на дым контур не применять, взять дым как в оригинале PS2»): дым машины и
// трубы (smoke1/smoke2/smoke3/smoke3alph) резался альфа-тестом №731 — на краях клубов темнели RGB-пиксели с альфой чуть выше порога: чёрная кайма и трещины между клубами.
// Дым — мягким смешиванием, как в оригинале и как пыль №847. SMOKE953_OLD=1 / «flag smoke953 0» — вырезка
#ifndef SMOKE953_OLD
#define SMOKE953_OLD 0
#endif
int fxSoftSmoke953 = !SMOKE953_OLD;
void (*fxGlowGlint)(float, float, float, float, unsigned, float, float);   // №839 (fx.h)
static inline int isSmoke953(int tex) { return tex >= 0 && (tex == texSmoke953[0] || tex == texSmoke953[1] || tex == texSmoke953[2] || tex == texSmoke953[3]); }
void fxSprite(int tex, float x, float y, float z, float w, float h, float rot, unsigned abgr, int blend, int flat) {
  if (!ready || tex < 0 || tex >= (int)H->nTex || nspr >= MAXSPR) return;
  Spr *s = &spr[nspr++]; s->x = x; s->y = y; s->z = z; s->w = w; s->h = h; s->rot = rot; s->c = abgr;
  s->tex = tex; s->blend = blend; s->flat = flat;
}
void fxSparkleCA(float x, float y, float z, float size, float phase, unsigned abgr, float alpha) {   // №910: alpha — доля прозрачности блика (монеты)
  if (fxGlowGlint && !GLOW_GLINT_OLD) { fxGlowGlint(x, y, z, size * 1.3f, abgr, alpha, phase); return; }   // №839: искра — векторным бликом (пульс свой)
  float pu = .75f + .25f * sinf(phase * 6.2831853f);
  unsigned c = abgr, o = 0;
  for (int k = 0; k < 24; k += 8) o |= ((unsigned)(((c >> k) & 255) * pu) & 255) << k;
  o |= 0xff000000u;
  if (texGlow >= 0) fxSprite(texGlow, x, y, z, size * .6f, size * .6f, 0.f, (o >> 2) & 0xff3f3f3f, 1, 0);
  if (texScratch >= 0) fxSprite(texScratch, x, y, z, size * 1.3f * pu, size * 1.3f * pu, phase * 1.5707963f, o, 1, 0);
}
void fxSparkleC(float x, float y, float z, float size, float phase, unsigned abgr) { fxSparkleCA(x, y, z, size, phase, abgr, 1.f); }
void fxSparkle(float x, float y, float z, float size, float phase) { fxSparkleC(x, y, z, size, phase, 0xffc8f0ff); }
// №910 (Роб 23.10: «блики у монеток жёсткие, перегнули — в несколько раз прозрачнее»): блёстки монет — векторный блик с долей COIN_GLINT_A. COIN_GLINT910_OLD=1 — как было (1)
#ifndef COIN_GLINT910_OLD
#define COIN_GLINT910_OLD 0
#endif
#define COIN_GLINT_A (COIN_GLINT910_OLD ? 1.f : .3f)
// №965 (Роб 25.10, TEST62: «блик у монеток слишком большой, размером с саму монетку; вдвое меньше и строго в центре монетки»): 1) размер блика монет — COIN_GLINT_S от прежнего
// (радиус главной звезды 0,72 м при монете в 0,69 м шириной -> 0,36 м); 2) блик стоял в плоскости, перпендикулярной взгляду, а монета-спрайт — в вертикальной: они пересекались
// по глубине, верхняя половина блика пряталась за монетой, нижняя светила поверх («блик внизу, не в центре», полоса поперёк монеты). Теперь блик сдвинут к глазу на COIN_GLINT_LIFT м
// ВДОЛЬ луча (на экране центр тот же, размер выравнивается по расстоянию) — целиком перед монетой. COIN_GLINT965_OLD=1 — как было
#ifndef COIN_GLINT965_OLD
#define COIN_GLINT965_OLD 0
#endif
#ifndef COIN_GLINT_S
#define COIN_GLINT_S (COIN_GLINT965_OLD ? 1.f : .5f)
#endif
#ifndef COIN_GLINT_LIFT
#define COIN_GLINT_LIFT .3f
#endif
// №970 (Роб 25.10: «на монетке блик другой: меньше, но несколько маленьких вокруг монетки»): вместо одной звезды в центре — COIN_SAT_N маленьких на кольце вокруг монеты в плоскости,
// перпендикулярной взгляду (центр кольца — центр монеты, сдвиг к глазу вдоль луча тот же, что в №965: все звёзды целиком перед монетой). Кольцо поворачивается со вращением монеты,
// размеры и радиусы у звёзд разные (не «часы»). Дальше COIN_SAT_D м — одна маленькая в центре (цена: каждая звезда — своя пачка вершин). GLINT970_OLD=1 / flag glint970old 1 — как №965
#ifndef GLINT970_OLD
#define GLINT970_OLD 0
#endif
int fxGlint970Old = GLINT970_OLD;
int fxBling989 = !BLING989_OLD;   // №989 (fx.h)
// №1020 (Роб 26.10: «на монетах много мелких бликов; хочу возле монетки асимметрично один-два блика, чтобы рандомно появлялись; у монет хотя бы минимальное рандомное поведение, бликов меньше, но более рандомные»):
// у каждой монеты (зерно — её номер, детерминированно) два «слота» блика, второй есть не у каждой; у слота свой период (1,5–3 с) и сдвиг, блик виден 36 % периода (плавное нарастание и спад sin², рывков нет),
// место при каждом появлении новое: случайный угол вокруг монеты на радиусе у края монеты (асимметрия), размер чуть разный. Форма блика, цвет, сдвиг к глазу (№965) те же. Дальше COIN_SAT_NEAR м и сверх
// COIN_SAT_MAXN монет за кадр — один слот. COIN1020_OLD=1 / файл coin1020old / «flag coin1020 0» — прежние три звезды кольцом. Кадр оборота спрайта сдвигается по монете — coinspr.h
#ifndef COIN1020_OLD
#define COIN1020_OLD 0
#endif
int fxCoin1020 = !COIN1020_OLD;
#ifndef COIN1020_DUTY
#define COIN1020_DUTY .36f
#endif
unsigned fxCoinStat[4];
static unsigned hash1020(unsigned s) { s ^= s >> 16; s *= 0x7feb352du; s ^= s >> 15; s *= 0x846ca68bu; s ^= s >> 16; return s; }
static float rnd1020(unsigned seed, unsigned k) { return (float)(hash1020(seed * 0x9e3779b1u + k * 0x85ebca6bu + 0x1234567u) & 0xffffff) * (1.f / 16777216.f); }
#ifndef COIN_SAT_N
#define COIN_SAT_N 3
#endif
// цена (просьба 25.10: три звезды у каждой монеты +0,5 мс PSP — дорого): звёзды маленькие, поэтому ОДНА звезда на блик (вторая не нужна), три звезды — только у монет ближе COIN_SAT_NEAR
// м и не больше COIN_SAT_MAXN монет за кадр; до COIN_SAT_FAR м — одна маленькая звезда в центре; дальше блика нет. Вершины всех звёзд кадра — общим списком в одном вызове (gvFlush)
#ifndef COIN_SAT_NEAR
#define COIN_SAT_NEAR 11.f
#endif
#ifndef COIN_SAT_FAR
#define COIN_SAT_FAR 22.f
#endif
#ifndef COIN_SAT_MAXN
#define COIN_SAT_MAXN 6
#endif
#ifndef COIN_SAT_A
#define COIN_SAT_A .4f
#endif   // доля прозрачности маленькой звезды: меньше размер — можно чуть плотнее, суммарный свет не растёт
void fxSparkleCoin(float x, float y, float z, float size, float phase, float ex, float ey, float ez) {
  float k = 1.f, dx = ex - x, dy = ey - y, dz = ez - z, d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (!COIN_GLINT965_OLD) { float o = COIN_GLINT_LIFT;
    if (d > o + .3f) { k = (d - o) / d; x += dx * (o / d); y += dy * (o / d); z += dz * (o / d); } }   // ближе — монета у самых глаз, сдвиг не нужен
  if (fxGlint970Old || COIN_GLINT965_OLD || d < .3f) { fxSparkleCA(x, y, z, size * COIN_GLINT_S * k, phase, 0xffc8f0ff, COIN_GLINT_A); return; }
  if (d > COIN_SAT_FAR) return;   // далеко: блика нет
  fxCoinStat[0]++; { unsigned g_ = (d > COIN_SAT_NEAR || coinSat970 >= COIN_SAT_MAXN) ? 1u : (unsigned)COIN_SAT_N; fxCoinStat[1] += g_; if (g_ > fxCoinStat[2]) fxCoinStat[2] = g_; }   // №1020 служебно
  fxGlintOne970 = 1;
  if (d > COIN_SAT_NEAR || coinSat970 >= COIN_SAT_MAXN) { fxSparkleCA(x, y, z, size * .3f * k, phase, 0xffc8f0ff, COIN_SAT_A); fxGlintOne970 = 0; return; }
  coinSat970++;
  static const float sz[COIN_SAT_N] = {.2f, .15f, .18f}, rho[COIN_SAT_N] = {.36f, .42f, .33f}, ang0[COIN_SAT_N] = {.4f, 2.6f, 4.5f};   // размер звезды и радиус кольца — доли size
  float vx = dx / d, vy = dy / d, vz = dz / d, rl = sqrtf(vx * vx + vz * vz); if (rl < 1e-3f) { fxSparkleCA(x, y, z, size * .3f * k, phase, 0xffc8f0ff, COIN_SAT_A); fxGlintOne970 = 0; return; }
  float Rx = -vz / rl, Rz = vx / rl, Ux = -vy * vx / rl, Uy = rl, Uz = -vy * vz / rl;   // оси экрана: вправо (горизонталь), вверх
  for (int j = 0; j < COIN_SAT_N; j++) {
    float a = ang0[j] + phase * 6.2831853f * .7f, ca = cosf(a), sa = sinf(a), r = rho[j] * size * k;
    fxSparkleCA(x + (Rx * ca + Ux * sa) * r, y + Uy * sa * r, z + (Rz * ca + Uz * sa) * r, size * sz[j] * k, phase + (float)j * .31f, 0xffc8f0ff, COIN_SAT_A);
  }
  fxGlintOne970 = 0;
}

void fxSparkleCoin1020(float x, float y, float z, float size, float age, unsigned seed, float ex, float ey, float ez) {
  float k = 1.f, dx = ex - x, dy = ey - y, dz = ez - z, d = sqrtf(dx * dx + dy * dy + dz * dz);
  { float o = COIN_GLINT_LIFT; if (d > o + .3f) { k = (d - o) / d; x += dx * (o / d); y += dy * (o / d); z += dz * (o / d); } }   // как №965: целиком перед монетой
  if (d < .3f) { fxSparkleCA(x, y, z, size * COIN_GLINT_S * k, age, 0xffc8f0ff, COIN_GLINT_A); return; }
  if (d > COIN_SAT_FAR) return;
  int slots = (d <= COIN_SAT_NEAR && coinSat970 < COIN_SAT_MAXN && rnd1020(seed, 30) < .45f) ? 2 : 1;
  if (d <= COIN_SAT_NEAR && coinSat970 < COIN_SAT_MAXN) coinSat970++;
  float vx = dx / d, vy = dy / d, vz = dz / d, rl = sqrtf(vx * vx + vz * vz); if (rl < 1e-3f) return;
  float Rx = -vz / rl, Rz = vx / rl, Ux = -vy * vx / rl, Uy = rl, Uz = -vy * vz / rl;   // оси экрана: вправо (горизонталь), вверх
  fxCoinStat[0]++; unsigned gl_ = 0;
  for (int j = 0; j < slots; j++) {
    float P = 1.5f + 1.5f * rnd1020(seed, 10 + (unsigned)j), t = age + rnd1020(seed, 20 + (unsigned)j) * P + (j ? 0.f : 0.f);
    float n = floorf(t / P), u = (t - n * P) / P; if (u >= COIN1020_DUTY) continue;
    float e = sinf(3.14159265f * u / COIN1020_DUTY); e *= e;   // плавно 0 -> 1 -> 0
    unsigned cyc = (unsigned)(int)n * 7u + (unsigned)j;
    float a = rnd1020(seed, 100 + cyc) * 6.2831853f, ca = cosf(a), sa = sinf(a), rho = (.36f + .16f * rnd1020(seed, 200 + cyc)) * size * k, sz = (.15f + .07f * rnd1020(seed, 300 + cyc)) * size * k;
    fxGlintOne970 = 1;
    fxSparkleCA(x + (Rx * ca + Ux * sa) * rho, y + Uy * sa * rho, z + (Rz * ca + Uz * sa) * rho, sz, rnd1020(seed, 400 + cyc) + u, 0xffc8f0ff, COIN_SAT_A * 1.25f * e);
    fxGlintOne970 = 0; gl_++;
  }
  fxCoinStat[1] += gl_; if (gl_ > fxCoinStat[2]) fxCoinStat[2] = gl_;
}

// ---------- рисование ----------
// правка 313: сборка биллбордов — viewer/fxdsp.h fxdBuild (на главном или на ME); здесь индексы, буфер вершин и команды GE
static unsigned short qidx[FX_NEL * 6] __attribute__((aligned(16)));
static int qidxN;
// Вершины частиц кадра: свой буфер (до правки 313 — sceGuGetMemory в списке GE). Главный пишет его мимо кэша (адрес | 0x40000000,
// как прежде список GE), ME — своим кэшем со сбросом строк в конце задания; GE читает его в этом же кадре (sceGuSync в конце кадра)
static FxV vbuf[FX_NEL * 4] __attribute__((aligned(64)));
#ifdef __psp__
#define VOUT ((FxV *)(0x40000000u | (unsigned)vbuf))
#else
#define VOUT vbuf
#endif

static void bindTex(int t) {
  const FxTex *x = &T[t];
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuClutLoad(x->psm == 4 ? 2 : 32, blob + x->offClut);
  sceGuTexMode(x->psm == 4 ? GU_PSM_T4 : GU_PSM_T8, 0, 0, x->swz);
  sceGuTexImage(0, x->w, x->h, x->bufw, blob + x->offPix);
  sceGuTexFlush();
}
int fxTexBind(int t) { if (!ready || t < 0 || t >= (int)H->nTex) return 0; bindTex(t); return 1; }

float fxFill; int fxSkipped;
static int (*geRoomFn)(unsigned);
void fxSetGeRoom(int (*room)(unsigned bytes)) { geRoomFn = room; }

// ---------- Media Engine (правка 313, флаг-файл mefx рядом с EBOOT; служба — viewer/me/meprobe.c, модуль viewer/me/sharme.c) ----------
// Задание кадра FJ уходит на ME в fxDrawBegin, пока главный рисует отложенное полупрозрачное (main.c tqFlush), и забирается в
// fxDrawEnd. ME занят (звук) или выключен — то же задание считает главный (та же функция). ME не ответил за FX_ME_TIMEOUT_US —
// ME частицам больше не отдаём, частицы начинаются заново в запасном задании (в старое ME мог бы ещё писать)
static int fjOn;                 // кадр начат (fxDrawBegin), итог ещё не забран
static unsigned fxJobUs, fxJobN;  // правка 313 (замер, main.c «flag meprof»): счёт задания частиц на главном, мкс, за кадры
void fxProfJob(unsigned *us, unsigned *n) { *us = fxJobUs; *n = fxJobN; fxJobUs = fxJobN = 0; }
#ifdef __psp__
#include <pspkernel.h>
#include "me/mebox.h"
#include "mebudget.h"
void fxSetMeMax(int n) { meFxSetMax(n); }   // ME-доля, вариант B: предел элементов задания частиц на ME (флаг-файл mefxmax, main_loop.h)
#include "fpuguard.h"
int meSvcScheme(void);
int meSvcSubmit(int client, volatile MeBox *ubox, unsigned kind, void *job, unsigned words);
int meSvcPoll(int client, volatile MeBox *ubox, unsigned seq, unsigned *sum);
int meSvcHeld(void); int meSvcSlept(void); void meSvcForget(int c);
#define FX_ME_TIMEOUT_US 20000u
static MeBox fxBox __attribute__((aligned(64)));
int fxMeActive(void);   // ящик частиц в памяти игры (схема ME_MEM_USER; в схемах 1–3 ящик — в ядре)
static volatile int fxMeState;   // 0 — нет, 1 — частицы на ME, 2 — выключено до конца запуска
static unsigned fxMeSeq, fxMeJobs, fxMeMain, fxMeWaitMax; static char fxMeWhy[32] = "-";
// правка 410 (09.10, docs/handoff/ME_ПЛАН_2026-10-09.md, К2 п.1): кадр без частиц на ME не отдаём — в нём только биллборды систем и
// спрайты, их сборка на главном дешевле отдачи: T2 (частицы на главном) в секундах fx=0/0 upd_us 55, T3 (на ME) — 389 (~334 мкс на кадр
// за отдачу). «Пустое» по одним элементам не годится: эмулятор 09.10 (пустышка ME, сцена после взрыва, fx=0/0) — 309 отдач без частиц
// и ни одной совсем пустой (биллборды и спрайты есть почти всегда). FX_ME_EMPTY_OLD=1 — как раньше (отдавать и такие кадры).
// MEPROF mefx=…/e<число> — кадров без частиц, не отданных ME; …/z<число> — отданных без частиц (новый код: 0)
#ifndef FX_ME_EMPTY_OLD
#define FX_ME_EMPTY_OLD 0
#endif
static unsigned fxMeEmpty, fxMeNoPart;
static int fjMe;                 // задание кадра ушло на ME
int fxMeAttach(void) {           // viewer/me/meprobe.c meFxBoot: служба ME поставлена
  if (!fjB && !(fjB = memalign(64, sizeof *fjB))) return -1;
  memset(&fxBox, 0, sizeof fxBox); sceKernelDcacheWritebackInvalidateRange(&fxBox, sizeof fxBox);
  sceKernelDcacheWritebackInvalidateRange(vbuf, sizeof vbuf);   // в кэше главного ни строки буфера вершин (его пишет ME)
  fxMeState = 1; return 0;
}
int fxMeActive(void) { return fxMeState == 1; }   // №653: частицы на ME — клиент 1 занят, город на ME не отдаём
int fxMeStateStr(char *out, int n) {   // «on123/4/0.6» — заданий ME за всё время / посчитано на главном (ME занят) / худшее ожидание, мс
  if (fxMeState == 1) return snprintf(out, n, "on%u/%u/%.1f/e%u/z%u", fxMeJobs, fxMeMain, fxMeWaitMax / 1000.f, fxMeEmpty, fxMeNoPart);
  return snprintf(out, n, "%s", fxMeState == 2 ? fxMeWhy : "-");
}
static void fjMainPtrs(void) { FJ.E = E; FJ.BB = BB; FJ.blob = blob; FJ.vout = VOUT; }
extern int mePeakGate, mePeak;
static int fxMeKick(void) {
  if (mePeakGate && !mePeak) return 0;   // вне пика частицы считает главный процессор (ME-доля, флаг-файл mepeak, audio.c mePeakStep)
  if (fxMeState == 1 && meSvcSlept()) { fxMeState = 2; meSvcForget(1); snprintf(fxMeWhy, sizeof fxMeWhy, "off:sleep"); }   // после сна ME не трогаем
  if (fxMeState != 1 || meSvcHeld()) return 0;
  if (!FX_ME_EMPTY_OLD && FJ.np == 0) { fxMeEmpty++; return 0; }   // правка 410: без частиц — на главном, без отдачи
  if (!meFxWithinBudget(FJ.np,FJ.nb,FJ.nspr)) { fxMeMain++; return 0; }
  int s = meSvcScheme(); if (s < 0) return 0;
  if ((s & ME_MEM_LOW) && (meIsHigh((unsigned)&FJ) || meIsHigh((unsigned)vbuf) || meIsHigh((unsigned)blob))) {
    fxMeState = 2; meSvcForget(1); snprintf(fxMeWhy, sizeof fxMeWhy, "off:high"); return 0; }
  FJ.fcsr = fpuGet();
  FJ.E = (const FxEmit *)meUserAddr(s, (unsigned)E); FJ.BB = (const FxBB *)meUserAddr(s, (unsigned)BB);
  FJ.blob = (const unsigned char *)meUserAddr(s, (unsigned)blob); FJ.vout = (FxV *)meUserAddr(s, (unsigned)vbuf);
  // участки пакета, которые читает счёт: эмиттеры, биллборды и их таблицы кадров (LUT лежат за таблицей BB до конца пакета)
  memset(FJ.inv, 0, sizeof FJ.inv);
  FJ.inv[0][0] = (unsigned)FJ.E; FJ.inv[0][1] = H->nEmit * sizeof(FxEmit);
  if (H->nBB) { unsigned lo = H->offBB, hi = H->offBB + H->nBB * sizeof(FxBB);
    for (unsigned b = 0; b < H->nBB; b++) { unsigned e = BB[b].offLut + BB[b].nFrames * 12u; if (BB[b].offLut < lo) lo = BB[b].offLut; if (e > hi) hi = e; }
    FJ.inv[1][0] = meUserAddr(s, (unsigned)blob + lo); FJ.inv[1][1] = hi - lo; }
  sceKernelDcacheWritebackInvalidateRange(&FJ, sizeof FJ);   // задание — в память, в кэше главного его строк нет
  int np0 = FJ.np;                                          // правка 410: до отдачи (после неё FJ — у ME)
  int q = meSvcSubmit(1, &fxBox, ME_KIND_FX, &FJ, sizeof FJ);
  if (q < 0) { fjMainPtrs(); fxMeMain++; return 0; }        // ME занят звуком — кадр считает главный
  fxMeSeq = (unsigned)q; if (!np0) fxMeNoPart++; return 1;
}
static int fxMeCollect(void) {   // 1 — итог ME в FJ; 0 — считать на главном
  unsigned t0 = sceKernelGetSystemTimeLow(), w = 0, sum = 0;
  while (!meSvcPoll(1, &fxBox, fxMeSeq, &sum)) {
    w = sceKernelGetSystemTimeLow() - t0;
    if (w >= FX_ME_TIMEOUT_US) {   // ME молчит: частицы ему больше не отдаём; старое задание не трогаем (ME мог бы ещё писать в него)
      fxMeState = 2; meSvcForget(1); snprintf(fxMeWhy, sizeof fxMeWhy, "off:timeout@%u", fxMeSeq);
      FxJob *o = fjCur; fjCur = fjB;
      memset(fjCur, 0, sizeof *fjCur); fjCur->fillK = o->fillK > 0.f ? o->fillK : 1.f;
      for (int i = 0; i < FX_MAX_INST; i++) { memset(in_[i].live, 0, sizeof in_[i].live); in_[i].nlive = 0; }
      return -1;
    }
  }
  sceKernelDcacheInvalidateRange(&FJ, sizeof FJ);           // чужие чтения задания за время счёта — из кэша вон
  fjMainPtrs();
  if (w > fxMeWaitMax) fxMeWaitMax = w;
  if (sum == ME_MIX_BADSIZE) { fxMeState = 2; meSvcForget(1); snprintf(fxMeWhy, sizeof fxMeWhy, "off:layout"); return 0; }   // модуль другой сборки
  fxMeJobs++; return 1;
}
#else
int fxMeActive(void) { return 0; }
int fxMeStateStr(char *out, int n) { return snprintf(out, n, "-"); }
#endif

// Кадр частиц, часть 1: список элементов (биллборды систем, спрайты), камера; задание уходит на ME (если включено)
// №899 (Роб 23.10: «пройтись по всем источникам света, бликов и искр — где ещё старые растровые эффекты (подбор монеты, починка машины и т.д.) —
// всё в чистые векторные эффекты»): частицы родных систем сложением с картинкой блика (scratch2*, glow*, bluestarglow, halo, spark4 — появление и
// подбор монет и ключа, вспышки Burst, звёзды значков и карточек) — векторной четырёхлучевой звездой (два ромба крестом: длинные лучи по осям квада,
// перехват 0,16), искры-штрихи spark3 (щитки, взрывы) — векторным ромбом по длинной оси; цвет, размер, поворот и жизнь — те же (из вершин квада),
// без текстуры, сложением. Огонь, дым, обломки, дуги ос — прежние. FX_VEC899_OLD=1 — картинками
#ifndef FX_VEC899_OLD
#define FX_VEC899_OLD 0
#endif
#ifndef FX_GLINT908_OLD
#define FX_GLINT908_OLD 0
#endif
#define FX_VEC_K (FX_GLINT908_OLD ? .8f : .5f)   // доля цвета частицы без картинки (картинка блика темнее к краю); №908 (Роб 23.10: блики у «!» и у цели слишком яркие — прозрачнее) .8 -> .5
#ifndef FX_VEC_ROOM_OLD
#define FX_VEC_ROOM_OLD 0
#endif
#define FX_VEC_WAIST .16f // перехват луча звезды (доля полуоси)
// №923 (Роб 24.10: «разбивание стекла выглядит совсем по-другому; сделай как было, но в векторе»): звезда-царапина осколков стекла (scratch2_blue, у «ключа»
// scratch2_red) после №899 стала белой четырёхлучевой, а картинка — синяя (центр белый, тело голубое) с восемью неровными лучами. Теперь — вектором: цвет картинки
// (texTint), восемь лучей (четыре длинных по осям квада и четыре короче по диагоналям), ровный цвет, один вызов; доля цвета частицы FX_STAR_K (у картинки без затухания
// к краю). FX_STAR923_OLD=1 — как после №899
// №937 (Роб 24.10, слово 17:35: звезда №923 с восемью лучами — «снежинка»; нужен «блинг-блинг», классический sparkle ✨): четыре луча (вверх, вниз, влево, вправо),
// бока между лучами — вогнутые плавные дуги (суперэллипс |x|^p + |y|^p = 1, p .55), пухлое тело и острые тонкие кончики; вертикальный луч 1,0, горизонтальный .85;
// главная звезда + две малых той же формы по диагонали (слева сверху и справа снизу, .35 главной); цвет картинки (texTint), светлая сердцевина, ровная заливка,
// один вызов (треугольники от центра); без вращения частицы — всегда стоит прямо. FX_SPARKLE937_OLD=1 — восемь лучей №923
#ifndef FX_SPARKLE937_OLD
#define FX_SPARKLE937_OLD 0
#endif
#define FX_STAR_K .7f
#define FX_STAR_LONG .72f   // длина осевых лучей (доля полуоси квада): у картинки звезда не доходит до края квада
#define FX_STAR_DIAG .354f  // диагональные лучи: .5 полуоси / sqrt(2) от суммы осей
#define FX_STAR_WAIST .26f
#define FX_STAR_CORE .28f   // сердцевина — белая у картинки
typedef struct { unsigned c; float x, y, z; } FxVV;
static unsigned fxVecCol(unsigned c) { unsigned o = c & 0xff000000u; for (int k = 0; k < 24; k += 8) o |= (unsigned)(((c >> k) & 255u) * FX_VEC_K) << k; return o; }
// №937 v3: звёзды-sparkle одним вызовом с индексами. Версия 2 писала по 240 вершин на звезду (пик списка GE в сцене стекла 123 -> 207 КБ, предел частиц 205 КБ); теперь вершины
// контура общие у соседних треугольников (центр + 4n точек на звезду) и индексы 16 бит: главная (n = FX_SPK_N), сердцевина и две малые (n = FX_SPK_N2) — 60 вершин и 168 индексов
// на звезду (1,3 КБ вместо 3,8). Сверх FX_SPK_MAXQ звёзд на группу кадра остальные — только главная с n = FX_SPK_N2 (без сердцевины и спутников), картинкой не заменяются
// №941: контур — общий шейп viewer/glintshape941.h (GS_SPARK4: вогнутые гладкие бока, острые кончики, пухлое тело); вершин столько же (4n + 1). GLINT_SHAPE941_OLD=1 — суперэллипс №937
static int fxSpkRing(FxVV *o, unsigned short *ix, int base, int *nix, const float c[3], const float R[3], const float U[3], float sx, float sy, int n, unsigned col) {   // -> число вершин
  static float cpx[2][4 * SPK_MAXN + 1], cpy[2][4 * SPK_MAXN + 1]; static int cm[2], cn[2];   // контур считается один раз на n (тригонометрия на каждый квад стоила бы x_fx ×2,5)
  int ci = n == FX_SPK_N ? 0 : 1; if (cn[ci] != n) { cm[ci] = GLINT_SHAPE941_OLD ? spkOutline(cpx[ci], cpy[ci], n) : gsOutline(&GS_SPARK4, n, 1.f, cpx[ci], cpy[ci]) + 1; cn[ci] = n; }
  if (!GLINT_SHAPE941_OLD) sx /= FX_SPK_HX;   // горизонтальный луч .85 уже в шейпе GS_SPARK4; вызывающий передаёт rs * FX_SPK_HX
  const float *px = cpx[ci], *py = cpy[ci]; int m = cm[ci] - 1;
  o[0].c = col; o[0].x = c[0]; o[0].y = c[1]; o[0].z = c[2];
  for (int i = 0; i < m; i++) {
    float a = px[i] * sx, b = py[i] * sy;
    o[1 + i].c = col; o[1 + i].x = c[0] + R[0] * a + U[0] * b; o[1 + i].y = c[1] + R[1] * a + U[1] * b; o[1 + i].z = c[2] + R[2] * a + U[2] * b;
    ix[*nix] = (unsigned short)base; ix[*nix + 1] = (unsigned short)(base + 1 + i); ix[*nix + 2] = (unsigned short)(base + 1 + (i + 1) % m); *nix += 3;
  }
  return m + 1;
}
static int fxVecSparkles(const FxV *q, int nq, unsigned tint) {
  int nfull = nq < FX_SPK_MAXQ ? nq : FX_SPK_MAXQ, nlite = nq - nfull;
  unsigned vmax = (unsigned)(nfull * (1 + 4 * FX_SPK_N + 3 * (1 + 4 * FX_SPK_N2)) + nlite * (1 + 4 * FX_SPK_N2)), imax = (unsigned)(nfull * (12 * FX_SPK_N + 36 * FX_SPK_N2) + nlite * 12 * FX_SPK_N2);
  unsigned bytes = vmax * (unsigned)sizeof(FxVV) + ((imax * 2 + 3) & ~3u);
  if (!FX_VEC_ROOM_OLD && geRoomFn && !geRoomFn(bytes + 512)) return 0;   // места нет — картинкой (как у прочих звёзд)
  FxVV *o = (FxVV *)sceGuGetMemory((int)(vmax * sizeof(FxVV))); unsigned short *ix = (unsigned short *)sceGuGetMemory((int)((imax * 2 + 3) & ~3u)); if (!o || !ix) return 0;
  int nv = 0, ni = 0;
  for (int i = 0; i < nq; i++, q += 4) {
    float cx = (q[0].x + q[2].x) * .5f, cy = (q[0].y + q[2].y) * .5f, cz = (q[0].z + q[2].z) * .5f;
    float A[3] = {(q[1].x - q[0].x) * .5f, (q[1].y - q[0].y) * .5f, (q[1].z - q[0].z) * .5f}, B[3] = {(q[3].x - q[0].x) * .5f, (q[3].y - q[0].y) * .5f, (q[3].z - q[0].z) * .5f};
    unsigned c = q[0].c & 0xff000000u, cc = c;   // цвет тела: цвет частицы × FX_STAR_K × цвет картинки; сердцевина светлее
    for (int k = 0; k < 24; k += 8) { float cp = (float)((q[0].c >> k) & 255u), tn = (tint >> k & 255u) * (1.f / 255.f);
      c |= (unsigned)(cp * FX_STAR_K * tn) << k; cc |= (unsigned)(cp * FX_STAR_K * (.55f + .45f * tn)) << k; }
    // sparkle ✨ стоит прямо, без вращения частицы; оси экрана — из плоскости квада
    float nx = A[1] * B[2] - A[2] * B[1], ny = A[2] * B[0] - A[0] * B[2], nz = A[0] * B[1] - A[1] * B[0], nl = sqrtf(nx * nx + ny * ny + nz * nz);
    float s = sqrtf(A[0] * A[0] + A[1] * A[1] + A[2] * A[2]), R[3], U[3], ctr[3] = {cx, cy, cz}; int ok = 0;
    if (nl > 1e-6f) {
      nx /= nl; ny /= nl; nz /= nl;
      U[0] = -nx * ny; U[1] = 1.f - ny * ny; U[2] = -nz * ny;                      // вверх мира без составляющей по нормали квада
      float ul = sqrtf(U[0] * U[0] + U[1] * U[1] + U[2] * U[2]);
      if (ul > .2f) { U[0] /= ul; U[1] /= ul; U[2] /= ul; R[0] = U[1] * nz - U[2] * ny; R[1] = U[2] * nx - U[0] * nz; R[2] = U[0] * ny - U[1] * nx;
                      float rl = sqrtf(R[0] * R[0] + R[1] * R[1] + R[2] * R[2]); R[0] /= rl; R[1] /= rl; R[2] /= rl; ok = 1; }
    }
    if (!ok) { float is = s > 1e-6f ? 1.f / s : 1.f; for (int j = 0; j < 3; j++) { R[j] = A[j] * is; U[j] = B[j] * is; } }   // квад смотрит вдоль вертикали: его же оси
    float rs = s * FX_SPK_R;
    if (i < nfull) {
      nv += fxSpkRing(o + nv, ix, nv, &ni, ctr, R, U, rs * FX_SPK_HX, rs, FX_SPK_N, c);
      nv += fxSpkRing(o + nv, ix, nv, &ni, ctr, R, U, rs * FX_SPK_HX * FX_SPK_CORE, rs * FX_SPK_CORE, FX_SPK_N2, cc);
      float sat = rs * FX_SPK_SAT, off[3];
      for (int k = 0; k < 2; k++) {
        for (int j = 0; j < 3; j++) off[j] = ctr[j] + (k ? (.62f * R[j] - .62f * U[j]) : (-.62f * R[j] + .66f * U[j])) * s;
        nv += fxSpkRing(o + nv, ix, nv, &ni, off, R, U, sat * FX_SPK_HX, sat, FX_SPK_N2, c);
      }
    } else nv += fxSpkRing(o + nv, ix, nv, &ni, ctr, R, U, rs * FX_SPK_HX, rs, FX_SPK_N2, c);
  }
  sceKernelDcacheWritebackRange(ix, (unsigned)ni * 2u);
  sceGuDisable(GU_TEXTURE_2D);
  sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, ni, ix, o);
  sceGuEnable(GU_TEXTURE_2D);
  return 1;
}
// №941: звёзды частиц (kind 1: scratch2*, glow*, bluestarglow, halo, spark4 — подбор, вспышки, значки) — тот же шейп, симметричный крест GS_CROSS4 по осям квада (вращается вместе
// с частицей, кончики на осях A и B, как были у ромбов-креста №899), сторона — FX_CROSS_SEG отрезков, веер от центра с индексами 16 бит: 1 + 4 * 3 = 13 вершин и 36 индексов на звезду
#define FX_CROSS_SEG 3
static int fxVecCross(const FxV *q, int nq) {
  static float cpx[GS_MAXPTS], cpy[GS_MAXPTS]; static int cm; if (!cm) cm = gsOutline(&GS_CROSS4, FX_CROSS_SEG, 1.f, cpx, cpy);
  unsigned vmax = (unsigned)nq * (unsigned)(1 + cm), imax = (unsigned)nq * (unsigned)(3 * cm), bytes = vmax * (unsigned)sizeof(FxVV) + ((imax * 2 + 3) & ~3u);
  if (!FX_VEC_ROOM_OLD && geRoomFn && !geRoomFn(bytes + 256)) return 0;
  FxVV *o = (FxVV *)sceGuGetMemory((int)(vmax * sizeof(FxVV))); unsigned short *ix = (unsigned short *)sceGuGetMemory((int)((imax * 2 + 3) & ~3u)); if (!o || !ix) return 0;
  int nv = 0, ni = 0;
  for (int i = 0; i < nq; i++, q += 4) {
    float cx = (q[0].x + q[2].x) * .5f, cy = (q[0].y + q[2].y) * .5f, cz = (q[0].z + q[2].z) * .5f;
    float A[3] = {(q[1].x - q[0].x) * .5f, (q[1].y - q[0].y) * .5f, (q[1].z - q[0].z) * .5f}, B[3] = {(q[3].x - q[0].x) * .5f, (q[3].y - q[0].y) * .5f, (q[3].z - q[0].z) * .5f};
    unsigned c = fxVecCol(q[0].c); int base = nv;
    o[nv].c = c; o[nv].x = cx; o[nv].y = cy; o[nv].z = cz; nv++;
    for (int k = 0; k < cm; k++, nv++) { o[nv].c = c; o[nv].x = cx + A[0] * cpx[k] + B[0] * cpy[k]; o[nv].y = cy + A[1] * cpx[k] + B[1] * cpy[k]; o[nv].z = cz + A[2] * cpx[k] + B[2] * cpy[k]; }
    for (int k = 0; k < cm; k++) { ix[ni] = (unsigned short)base; ix[ni + 1] = (unsigned short)(base + 1 + k); ix[ni + 2] = (unsigned short)(base + 1 + (k + 1) % cm); ni += 3; }
  }
  sceKernelDcacheWritebackRange(ix, (unsigned)ni * 2u);
  sceGuDisable(GU_TEXTURE_2D);
  sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, ni, ix, o);
  sceGuEnable(GU_TEXTURE_2D);
  return 1;
}
static int fxVecGroup(const FxV *q, int nq, int kind, unsigned tint) {   // q — вершины группы (по 4 на квад); 0 — нечего рисовать
  if (kind == 3 && !FX_SPARKLE937_OLD) return fxVecSparkles(q, nq, tint);   // №937
  if (kind == 1 && !GLINT_SHAPE941_OLD) return fxVecCross(q, nq);   // №941
  int per = kind == 3 ? 36 : kind == 1 ? 12 : 6; unsigned bytes = (unsigned)(nq * per) * sizeof(FxVV);
  if (!FX_VEC_ROOM_OLD && geRoomFn && !geRoomFn(bytes + 256)) return 0;   // надзор 23.10: вершины звёзд — в списке GE (до ~114 КБ/кадр); места нет — картинкой (своему пути хватает 256 Б)
  FxVV *o = (FxVV *)sceGuGetMemory((int)bytes); if (!o) return 0;
  FxVV *w = o;
  for (int i = 0; i < nq; i++, q += 4) {
    float cx = (q[0].x + q[2].x) * .5f, cy = (q[0].y + q[2].y) * .5f, cz = (q[0].z + q[2].z) * .5f;
    float A[3] = {(q[1].x - q[0].x) * .5f, (q[1].y - q[0].y) * .5f, (q[1].z - q[0].z) * .5f}, B[3] = {(q[3].x - q[0].x) * .5f, (q[3].y - q[0].y) * .5f, (q[3].z - q[0].z) * .5f};
    unsigned c = fxVecCol(q[0].c);
    unsigned cc = c;   // цвет сердцевины (kind 3): светлее тела, почти белый с оттенком картинки
    if (kind == 3) {
      unsigned o = q[0].c & 0xff000000u, oc = o;
      for (int k = 0; k < 24; k += 8) { float cp = (float)((q[0].c >> k) & 255u), tn = (tint >> k & 255u) * (1.f / 255.f);
        o |= (unsigned)(cp * FX_STAR_K * tn) << k; oc |= (unsigned)(cp * FX_STAR_K * (.55f + .45f * tn)) << k; }
      c = o; cc = oc;
    }
    float D1[3], D2[3], AL[3], BL[3], AC[3], BC[3];
    if (kind == 3) for (int j = 0; j < 3; j++) { AL[j] = A[j] * FX_STAR_LONG; BL[j] = B[j] * FX_STAR_LONG; D1[j] = (A[j] + B[j]) * FX_STAR_DIAG; D2[j] = (A[j] - B[j]) * FX_STAR_DIAG; AC[j] = A[j] * FX_STAR_CORE; BC[j] = B[j] * FX_STAR_CORE; }
    for (int d = 0; d < (kind == 3 ? 6 : kind == 1 ? 2 : 1); d++) {   // ромб: кончики по L, перехват по S
      const float *L, *S;
      float wk = kind == 1 ? FX_VEC_WAIST : .3f;
      if (kind == 3) {
        switch (d) { case 0: L = AL; S = BL; break; case 1: L = BL; S = AL; break; case 2: L = D1; S = D2; break; case 3: L = D2; S = D1; break; case 4: L = AC; S = BC; break; default: L = BC; S = AC; break; }
        wk = d >= 4 ? .45f : FX_STAR_WAIST; c = d >= 4 ? cc : c;
      } else { L = d ? B : A; S = d ? A : B; }
      if (kind == 2 && A[0] * A[0] + A[1] * A[1] + A[2] * A[2] < B[0] * B[0] + B[1] * B[1] + B[2] * B[2]) { L = B; S = A; }   // штрих — по длинной оси
      float P[4][3]; for (int j = 0; j < 3; j++) { P[0][j] = L[j]; P[1][j] = S[j] * wk; P[2][j] = -L[j]; P[3][j] = -S[j] * wk; }
      static const unsigned char tri[6] = {0, 1, 2, 0, 2, 3};   // ромб — два треугольника по длинной диагонали
      for (int t = 0; t < 6; t++) { const float *a = P[tri[t]]; w->c = c; w->x = cx + a[0]; w->y = cy + a[1]; w->z = cz + a[2]; w++; }
    }
  }
  sceGuDisable(GU_TEXTURE_2D);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)(w - o), 0, o);
  sceGuEnable(GU_TEXTURE_2D);
  return 1;
}
// №941 (служебно, flag glintlab 1): лист форм — звезда частиц (kind 1) или стекла (kind 3) через настоящий путь рисования (fxVecCross / fxVecSparkles) на квадах c ± R·s ± U·s
void fxLab941(const float c[3], const float R[3], const float U[3], float s, int kind) {
  FxV q[4]; float A[3], B[3]; for (int j = 0; j < 3; j++) { A[j] = R[j] * s; B[j] = U[j] * s; }
  for (int k = 0; k < 4; k++) { float sa = (k == 1 || k == 2) ? 1.f : -1.f, sb = (k >= 2) ? 1.f : -1.f; q[k].u = q[k].v = 0.f; q[k].c = 0xFFFFFFFFu;
    q[k].x = c[0] + A[0] * sa + B[0] * sb; q[k].y = c[1] + A[1] * sa + B[1] * sb; q[k].z = c[2] + A[2] * sa + B[2] * sb; }
  ScePspFMatrix4 Id; memset(&Id, 0, sizeof Id); Id.x.x = Id.y.y = Id.z.z = Id.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &Id);
  sceGuDisable(GU_FOG); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_DEPTH_TEST);
  fxVecGroup(q, 1, kind == 3 ? 3 : 1, 0xFFFFFFFFu);   // тот же выбор формы, что у частиц (GLINT_SHAPE941_OLD — прежние)
}
// №899 (служебно, flag fxtex 1): какие картинки частиц сколько квадов рисуют, по смешиванию (0 обычное/вырезка, 1 сложение, 2 вычитание) — fxTexStat
int fxAddStatOn; static unsigned fxAddStat[3][64];
void fxTexStat(char *out, int n) {
  int l = 0; out[0] = 0;
  for (int b = 0; b < 3; b++) for (int t = 0; t < 64 && ready && t < (int)H->nTex; t++) if (fxAddStat[b][t] && l < n - 40) l += snprintf(out + l, n - l, " %d:%.15s=%u", b, T[t].name, fxAddStat[b][t]);
  memset(fxAddStat, 0, sizeof fxAddStat);
}
void fxDrawBegin(float ex, float ey, float ez, float fx, float fy, float fz) {
  eye[0] = ex; eye[1] = ey; eye[2] = ez; haveEye = 1;
  fxFill = 0.f; fxSkipped = 0; fjOn = 0;
  if (!ready) { nspr = 0; return; }
  int nb = 0;
  for (int i = 0; i < FX_MAX_INST; i++) {
    Inst *s = &in_[i]; if (!s->used) continue;
    const FxSys *y = &S[s->sys];
    for (int b = 0; b < y->nBB && nb < 3 * FX_MAX_INST; b++) if (s->bbT < BB[y->firstBB + b].nFrames) {
      FxJobBB *q = &FJ.bb[nb++]; q->bb = (unsigned short)(y->firstBB + b); q->x = s->pos[0]; q->y = s->pos[1]; q->z = s->pos[2]; q->t = s->bbT;
      for (int k=0;k<3;k++) q->up[k]=s->m[3+k]; }
  }
  for (int i = 0; i < nspr; i++) FJ.spr[i] = spr[i];
  FJ.nb = nb; FJ.nspr = nspr; nspr = 0;
  FJ.nt = (int)H->nTex; FJ.E = E; FJ.BB = BB; FJ.blob = blob; FJ.vout = VOUT;
  FJ.ex = ex; FJ.ey = ey; FJ.ez = ez; FJ.fx = fx; FJ.fy = fy; FJ.fz = fz;
  if (!(FJ.fillK > 0.f)) FJ.fillK = 1.f;   // прежнее static fillK = 1
  FJ.stage = (simPend ? FXD_SIM : 0u) | FXD_BUILD | (explosionOld ? FXD_GROUND_OLD : 0u); simPend = 0;
  fjOn = 1;
#ifdef __psp__
  fjMe = fxMeKick();
#endif
}

// Кадр частиц, часть 2: итог (ME или счёт здесь же), счётчики live, команды GE по группам (смешивание, текстура)
void fxDrawEnd(void) {
  if (!fjOn) return;
  fjOn = 0;
  int got = 0;
#ifdef __psp__
  if (fjMe) { fjMe = 0; got = fxMeCollect(); if (got < 0) return; }   // ME замолчал: кадр без частиц, дальше — главный
#endif
#if !defined(__psp__) && defined(FX_JOB_COPY)
  // правка 313, проверка «путь задания» на Маке (tests/me/fx_test.py): задание уезжает в другую память (как к ME), старое место
  // затирается, счёт идёт там, итог и вершины возвращаются. FX_JOB_RED=1 — красный контроль: копия не обновляется
  if (!got) {
    static FxJob *K; static FxV *KV; static int primed;
    if (!K) { K = memalign(64, sizeof *K); KV = memalign(64, sizeof vbuf); }
    if (!FX_JOB_RED || !primed) { memcpy(K, &FJ, sizeof *K); primed = 1; }
    memset(&FJ, 0xA5, sizeof FJ); K->vout = KV;
    fxdRun(K);
    memcpy(&FJ, K, sizeof FJ); memcpy(vbuf, KV, (unsigned)K->nq * 4u * sizeof(FxV)); FJ.vout = VOUT; got = 1;
  }
#endif
#ifdef __psp__
  // «Быстрый расчёт» (x_fx взрывы 3,5–5,6 мс, задание частиц на главном 0,8–1,5 мс; запись в память мимо кэша — слово ≈ 0,2–1 мкс): задание
  // пишет вершины (24 слова на четвёрку) в кэшируемый буфер, один сброс кэша строк и вон из кэша (ME потом пишет свой кадр поверх; строк главного
  // в буфере не остаётся). -DFX_FAST_OLD=1 — запись мимо кэша, как раньше
  if (!got) { unsigned t0 = sceKernelGetSystemTimeLow();
#ifndef FX_FAST_OLD
    FJ.vout = vbuf; fxdRun(&FJ);
    if (FJ.nq > 0) { unsigned wb = ((unsigned)FJ.nq * 4u * sizeof(FxV) + 63u) & ~63u; if (wb > sizeof vbuf) wb = sizeof vbuf; sceKernelDcacheWritebackInvalidateRange(vbuf, wb); }
    FJ.vout = VOUT;
#else
    fxdRun(&FJ);
#endif
    fxJobUs += sceKernelGetSystemTimeLow() - t0; fxJobN++; }
#else
  if (!got) fxdRun(&FJ);
#endif
  if (FJ.stage & FXD_SIM) applyDead();
  fxFill = FJ.fxFill; fxSkipped = FJ.fxSkipped;
  if (FJ.empty) return;
  if (qidxN < FX_NEL) {
    int m = FX_NEL;
    for (int q = 0; q < m; q++) { unsigned short *d = qidx + q * 6, b = q * 4; d[0] = b; d[1] = b + 1; d[2] = b + 2; d[3] = b; d[4] = b + 2; d[5] = b + 3; }
    qidxN = m; sceKernelDcacheWritebackRange(qidx, sizeof qidx);
  }
  sceGuDisable(GU_FOG); sceGuDisable(GU_ALPHA_TEST);
  sceGuEnable(GU_TEXTURE_2D); sceGuEnable(GU_BLEND);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
  sceGuDepthMask(GU_TRUE);   // глубину не пишем, но проверяем
  ScePspFMatrix4 Id; memset(&Id, 0, sizeof Id); Id.x.x = Id.y.y = Id.z.z = Id.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &Id);
  int nt = FJ.nt;
  for (int k = 0; k < FJ.nk; k++) {
    int nq = FJ.qn[k]; if (!nq) continue;
    int blend = k / nt, tex = k % nt;
    // №847 (Роб 19.10: «из-под ног при беге летит мультяшная пыль»): у пыли (dust1.bmp: бег dashShape, обломки dustShape…) альфа по жизни не выше
    // 55 из 255 — порог вырезки №731 (0x58) резал её целиком, пыли не было видно; пыль — мягким смешиванием, как в оригинале. FX_SOFTDUST847_OLD=1 — вырезка
    if (fxCutout && blend == 0 && (FX_SOFTDUST847_OLD || (tex != texDust1 && tex != texCloud)) && !(fxSoftSmoke953 && isSmoke953(tex))) { sceGuDisable(GU_BLEND); sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, FX_CUT_REF, 0xff); }   // №731: вырезка
    else {
      if (fxCutout) { sceGuEnable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); }
      if (blend == 1) sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
      else if (blend == 2) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
      else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    }
    if (geRoomFn && !geRoomFn(256)) { fxSkipped += nq; continue; }   // правка 103: список GE полон (вершины — в своём буфере)
    if (fxAddStatOn && tex < 64) fxAddStat[blend > 2 ? 2 : blend][tex] += (unsigned)nq;   // №899 (служебно): квадов по картинке и смешиванию — строка FXTEX
    if (!FX_VEC899_OLD && blend == 1 && tex < 64 && texVec[tex]) {   // №899: блик / искра — вектором (вершины квадов уже посчитаны этим кадром)
#ifdef __psp__
      { unsigned a0 = (unsigned)(vbuf + 4 * FJ.qs[k]) & ~63u, a1 = ((unsigned)(vbuf + 4 * FJ.qs[k] + 4 * nq) + 63u) & ~63u;   // строки целиком: грязных строк в vbuf нет (главный сбросил, ME пишет мимо)
        sceKernelDcacheInvalidateRange((void *)a0, a1 - a0); }   // читаем то, что записали в память задание или ME
#endif
      if (fxVecGroup(vbuf + 4 * FJ.qs[k], nq, texVec[tex], texTint[tex])) continue;
    }
    bindTex(tex);
    sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                   nq * 6, qidx, vbuf + 4 * FJ.qs[k]);
  }
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST);
  sceGuDepthMask(GU_FALSE);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT);
  sceGuTexScale(128.f, 128.f);
  sceGuEnable(GU_FOG);
}
void fxDraw(float ex, float ey, float ez, float fx, float fy, float fz) { fxDrawBegin(ex, ey, ez, fx, fy, fz); fxDrawEnd(); }
