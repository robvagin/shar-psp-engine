// Читатель svdata-l<N>.shr (SVD1): объёмы «смена света». Подробности и формат — viewer/svdata.h и tools/svdata_export.py.
// Сверка g12-data, круг 6, P12-08: игрок входит в объём — цвет солнца умножается на цвет данных, плавно за 1,0 с от текущего множителя;
// выходит — считается число объёмов (вошёл в два, вышел из одного: свет остаётся); вышел из последнего — плавно назад к 1 за 1,0 с.
// Множитель канала = байт / 256 (а не / 255): при белом (255) множитель ровно 1, при любом другом чуть меньше доли байта, как у оригинала.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "svdata.h"

#ifndef SVLIGHT_OLD
#define SVLIGHT_OLD 0
#endif
#define SV_VOLREC 120
#define SV_LGT_MAX 64

typedef struct { float mul[3]; uint32_t kind; float c[3], h[3], ax[9]; } LgtVol;
static LgtVol *sLgt; static unsigned sNLgt;
static unsigned char sIn[SV_LGT_MAX];       // игрок в объёме i на прошлом шаге
static int sCount;                           // сколько объёмов «смена света» игрок сейчас занимает (по счёту входов и выходов)
static float sSrc[3] = {1, 1, 1}, sDst[3] = {1, 1, 1}, sCur[3] = {1, 1, 1}, sT = -1.f, sLast[3] = {1, 1, 1};

#ifndef SVTELEPORT_OLD
#define SVTELEPORT_OLD 0
#endif
#define SV_ACT_TELEPORT 7
#define SV_TP_MAX 16
typedef struct { float to[3], head; uint32_t kind; float c[3], h[3], ax[9]; } TpPad;
static TpPad sTp[SV_TP_MAX]; static unsigned sNTp;
static uint32_t rd32(const unsigned char *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float rdf(const unsigned char *p) { float v; memcpy(&v, p, 4); return v; }

void svdataFree(void) { free(sLgt); sLgt = 0; sNLgt = 0; sNTp = 0; svLightReset(); }

int svdataParse(const void *file, unsigned n, int level) {
  svdataFree();
  const unsigned char *b = (const unsigned char *)file;
  if (!b || n < 64 || memcmp(b, "SVD1", 4) != 0 || rd32(b + 4) != 1 || (int)rd32(b + 8) != level) return 0;
  uint32_t cnt = SVLIGHT_OLD ? 0 : rd32(b + 12), off = rd32(b + 16);   // SVLIGHT_OLD=1 — объёмы света не читаются (площадки переброса — свой выключатель)
  if (cnt > SV_LGT_MAX || off > n || (uint64_t)off + (uint64_t)cnt * SV_VOLREC > n) return 0;
  if (!SVTELEPORT_OLD && n >= 64 + 0) {   // таблица ACT (@36 число, @40 смещение): записи VolRec, у Teleport код действия 7 в слоте role, курс — f32 в слоте loc_i
    uint32_t na = rd32(b + 36), oa = rd32(b + 40);
    if (na <= 64 && oa <= n && (uint64_t)oa + (uint64_t)na * SV_VOLREC <= n)
      for (uint32_t i = 0; i < na && sNTp < SV_TP_MAX; i++) {
        const unsigned char *r = b + oa + (size_t)i * SV_VOLREC; if (rd32(r + 116) != SV_ACT_TELEPORT) continue;
        TpPad *t = &sTp[sNTp++]; for (int k = 0; k < 3; k++) { t->to[k] = rdf(r + 32 + 4 * k); t->c[k] = rdf(r + 52 + 4 * k); t->h[k] = rdf(r + 64 + 4 * k); }
        t->head = rdf(r + 112); t->kind = rd32(r + 48); for (int k = 0; k < 9; k++) t->ax[k] = rdf(r + 76 + 4 * k);
      }
  }
  if (!cnt) return 1;
  sLgt = (LgtVol *)calloc(cnt, sizeof(LgtVol)); if (!sLgt) return 0;
  for (uint32_t i = 0; i < cnt; i++) {
    const unsigned char *r = b + off + (size_t)i * SV_VOLREC; LgtVol *v = &sLgt[i];
    for (int k = 0; k < 3; k++) v->mul[k] = rdf(r + 32 + 4 * k) * (255.f / 256.f);   // aux[0..2] = байт / 255 -> байт / 256
    v->kind = rd32(r + 48);
    for (int k = 0; k < 3; k++) { v->c[k] = rdf(r + 52 + 4 * k); v->h[k] = rdf(r + 64 + 4 * k); }
    for (int k = 0; k < 9; k++) v->ax[k] = rdf(r + 76 + 4 * k);
  }
  sNLgt = cnt; return 1;
}

int svdataLightCount(void) { return (int)sNLgt; }

void svLightReset(void) {
  memset(sIn, 0, sizeof sIn); sCount = 0; sT = -1.f;
  for (int k = 0; k < 3; k++) sSrc[k] = sDst[k] = sCur[k] = sLast[k] = 1.f;
}

static int inVol(const LgtVol *v, const float p[3]) {
  float d[3] = {p[0] - v->c[0], p[1] - v->c[1], p[2] - v->c[2]};
  if (v->kind == 0) return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= v->h[0] * v->h[0];
  for (int r = 0; r < 3; r++) { float l = d[0] * v->ax[3 * r] + d[1] * v->ax[3 * r + 1] + d[2] * v->ax[3 * r + 2]; if (fabsf(l) > v->h[r]) return 0; }
  return 1;
}

int svLightStep(const float p[3], float dt, float out[3]) {
  for (unsigned i = 0; i < sNLgt; i++) {
    int now = inVol(&sLgt[i], p);
    if (now == sIn[i]) continue;
    sIn[i] = (unsigned char)now;
    if (now) {   // вход: назначение — множитель объёма; начало — то, что сейчас на экране
      sCount++;
      if (memcmp(sDst, sLgt[i].mul, sizeof sDst) != 0) { memcpy(sSrc, sCur, sizeof sSrc); memcpy(sDst, sLgt[i].mul, sizeof sDst); sT = 0.f; }
    } else if (--sCount <= 0) {   // вышел из последнего: назад к единице
      sCount = 0; memcpy(sSrc, sCur, sizeof sSrc); sDst[0] = sDst[1] = sDst[2] = 1.f; sT = 0.f;
    }
  }
  if (sT >= 0.f) {
    sT += dt; if (sT > 1.f) sT = 1.f;
    for (int k = 0; k < 3; k++) sCur[k] = sSrc[k] + (sDst[k] - sSrc[k]) * sT;
    if (sT >= 1.f) sT = -1.f;   // доехали
  }
  int ch = memcmp(sCur, sLast, sizeof sCur) != 0;
  memcpy(sLast, sCur, sizeof sLast); memcpy(out, sCur, sizeof sCur);
  return ch;
}

// Телепорт-площадки. Тот же ящик/шар, что у объёмов света (kind 0 шар радиуса h[0], 1 ящик по осям).
static int inPad(const TpPad *v, const float p[3]) {
  float d[3] = {p[0] - v->c[0], p[1] - v->c[1], p[2] - v->c[2]};
  if (v->kind == 0) return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= v->h[0] * v->h[0];
  for (int r = 0; r < 3; r++) { float l = d[0] * v->ax[3 * r] + d[1] * v->ax[3 * r + 1] + d[2] * v->ax[3 * r + 2]; if (fabsf(l) > v->h[r]) return 0; }
  return 1;
}
int svTeleportCount(void) { return (int)sNTp; }
int svTeleportAt(const float p[3]) { for (unsigned i = 0; i < sNTp; i++) if (inPad(&sTp[i], p)) return (int)i; return -1; }
int svTeleportTarget(int k, float pos[3], float *heading) {
  if (k < 0 || (unsigned)k >= sNTp) return 0;
  pos[0] = sTp[k].to[0]; pos[1] = sTp[k].to[1]; pos[2] = sTp[k].to[2]; *heading = sTp[k].head; return 1;
}
