#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspiofilemgr.h>
#include "msnres.h"
// формат MRS1: заголовок 32 Б, миссии по 40 Б, записи по 96 Б (tools/missions/msnres.py)
typedef struct { char id[8]; unsigned first, n, art, car, chr, peak, pad[2]; } Rec;
static unsigned char *blob; static unsigned nm, ne; static const Rec *recs; static const MsnResEnt *ents;
void msnResClose(void) { free(blob); blob = 0; nm = ne = 0; recs = 0; ents = 0; }
int msnResOpen(const char *dir, int level) {
  msnResClose();
  char fn[300]; if (level > 1) snprintf(fn, sizeof fn, "%sl%d/msnres-l%d.shr", dir, level, level); else snprintf(fn, sizeof fn, "%smsnres-l%d.shr", dir, level);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return -1;
  int sz = sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  if (sz < 32 || sz > (1 << 20)) { sceIoClose(f); return -1; }
  blob = (unsigned char *)malloc((size_t)sz); if (!blob) { sceIoClose(f); return -1; }
  int rd = sceIoRead(f, blob, sz); sceIoClose(f);
  unsigned h[8]; if (rd != sz) goto bad; memcpy(h, blob, 32);
  if (memcmp(blob, "MRS1", 4) || h[1] != 1 || (int)h[2] != level) goto bad;
  nm = h[3]; ne = h[4]; unsigned off = h[5];
  if (nm > 64 || 32u + nm * sizeof(Rec) > (unsigned)sz || off < 32u + nm * sizeof(Rec) || off + (size_t)ne * sizeof(MsnResEnt) > (size_t)sz) goto bad;
  recs = (const Rec *)(blob + 32); ents = (const MsnResEnt *)(blob + off);
  for (unsigned i = 0; i < nm; i++) if (recs[i].first + recs[i].n > ne) goto bad;
  return 0;
bad: msnResClose(); return -1;
}
int msnResMission(const char *id) { if (!id) return -1; for (unsigned i = 0; i < nm; i++) if (!strncmp(recs[i].id, id, 8)) return (int)i; return -1; }
unsigned msnResCount(int mi) { return mi >= 0 && (unsigned)mi < nm ? recs[mi].n : 0; }
const MsnResEnt *msnResAt(int mi, unsigned i) { return mi >= 0 && (unsigned)mi < nm && i < recs[mi].n ? &ents[recs[mi].first + i] : 0; }
unsigned msnResPeak(int mi) { return mi >= 0 && (unsigned)mi < nm ? recs[mi].peak : 0; }
unsigned msnResBytes(int mi, int kind) { return mi < 0 || (unsigned)mi >= nm ? 0 : kind == MSNRES_CAR ? recs[mi].car : kind == MSNRES_CHR ? recs[mi].chr : recs[mi].art; }
int msnResShared(const char *key) {   // 1363: общие ресурсы миссий уровня (cards, wrench, missgen, двери, значки, которые просят несколько миссий)
  if (!ents || !key || !*key) return 0;
  unsigned cnt = 0;
  for (unsigned m = 0; m < nm; m++)
    for (unsigned i = 0; i < recs[m].n; i++) { const MsnResEnt *e = &ents[recs[m].first + i];
      if (e->kind != MSNRES_ART || strcmp(e->key, key)) continue;
      if (recs[m].id[0] == 'L' && !recs[m].id[1]) return 1;
      cnt++; break; }
  return cnt >= 2;
}
