// Перепись «невидимое твёрдое для пешего» (сверка Fable-2 П7/П8, окно Гомер 107b): по каждому состоянию каждого типа стейтпропа смотрим, остаётся ли
// объём (colOn по умолчанию 1) при состоянии без рисунка. Печатает по уровню: типов, экземпляров, состояний без рисунка, из них с событием/вызовом/эффектом/автопереходом,
// экземпляров, у которых НАЧАЛЬНОЕ состояние (0) без рисунка. Плюс размах сдвига объёма за суставом (типы со следованием) по кадрам клипа.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {} void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
static unsigned char *rd(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (*n + 15) & ~15); if (fread(d, 1, *n, f) != (size_t)*n) return 0; fclose(f); return d; }
static Props props; static ColWorld world; static StateProps sp;
static int anyArt(unsigned i) { unsigned n = spSlotCount(&sp, sp.inst[i].type); for (unsigned s = 0; s < n; s++) if (spSlotOn(&sp, i, s)) return 1; return 0; }
int main(int argc, char **argv) {
  for (int a = 1; a + 1 < argc; a += 2) {
    long size, nn; unsigned char *data = rd(argv[a], &size), *names = rd(argv[a + 1], &nn); if (!data || !names) { printf("нет %s\n", argv[a]); continue; }
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    memset(&props, 0, sizeof props); memset(&sp, 0, sizeof sp);
    colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    unsigned ny = *(unsigned *)(names + 16), oy = *(unsigned *)(names + 28); spFollowAdopt(&sp, names + oy, ny);
    unsigned noArtSt = 0, noArtBare = 0, inst0 = 0, inst0bare = 0, followInst = 0; float maxOff = 0.f;
    for (unsigned t = 0; t < sp.ntype; t++) {
      const SpType *T = &sp.type[t]; if (T->kind || !T->nCol) continue;
      for (unsigned s = 0; s < T->nState; s++) {
        const SpState *S = &sp.state[T->firstState + s]; unsigned any = 0;
        // найти экземпляр этого типа, чтобы спросить spSlotOn при нужном состоянии
        for (unsigned i = 0; i < sp.ninst && !any; i++) if (sp.inst[i].type == t) { unsigned keep = sp.run[i].state; sp.run[i].state = (uint8_t)s; any = anyArt(i) ? 1 : 2; sp.run[i].state = (uint8_t)keep; }
        if (any == 2) { noArtSt++; if (!S->nFx && !S->nEvent && !S->nCall && S->autoTo == 0xFF) noArtBare++; }
      }
    }
    for (unsigned i = 0; i < sp.ninst; i++) {
      const SpType *T = &sp.type[sp.inst[i].type]; if (T->kind || !T->nCol) continue;
      if (sp.run[i].state == 0 && sp.run[i].colOn && !anyArt(i)) { inst0++; if (getenv("CENSUS_V")) { const SpState *Sx = &sp.state[T->firstState]; printf("   inst %u type %u cls %u nCol %u pos %.1f %.1f %.1f fx %u ev %u call %u auto %u op0 %d t0 %.2f\n", i, sp.inst[i].type, T->cls, T->nCol, sp.run[i].x, sp.run[i].y, sp.run[i].z, Sx->nFx, Sx->nEvent, Sx->nCall, Sx->autoTo, Sx->nCall ? (int)sp.call[Sx->firstCall].op : -1, Sx->nCall ? sp.call[Sx->firstCall].time : -1.f); } const SpState *S = &sp.state[T->firstState]; if (!S->nFx && !S->nEvent && !S->nCall && S->autoTo == 0xFF) inst0bare++; }
      unsigned t = sp.inst[i].type;
      if (sp.follow && t < sp.nfollow && sp.follow[t]) {
        followInst++; unsigned nfr = T->frames & 0xFFFF; float base[3] = {0, 0, 0}; int have = 0;
        for (unsigned f = 0; f <= nfr; f += (nfr > 40 ? nfr / 40 : 1)) {
          sp.run[i].poseF = (float)f; sp.run[i].poseValid = 0; float c[3];
          if (!spVolumeCenter(&sp, i, 0, c)) break;
          const SpCol *C = &sp.col[T->firstCol]; const SpRun *r = &sp.run[i]; float b[3];
          for (int k = 0; k < 3; k++) b[k] = r->R[k] * C->c[0] + r->R[3 + k] * C->c[1] + r->R[6 + k] * C->c[2] + (k == 0 ? r->x : k == 1 ? r->y : r->z);
          float d = sqrtf((c[0] - b[0]) * (c[0] - b[0]) + (c[1] - b[1]) * (c[1] - b[1]) + (c[2] - b[2]) * (c[2] - b[2])); if (d > maxOff) maxOff = d; (void)base; (void)have; }
      }
    }
    printf("%s: типов %u экземпляров %u | состояний без рисунка %u (из них совсем пустых %u) | экземпляров с начальным состоянием без рисунка %u (пустых %u) | со следованием %u, макс. отход объёма от позы кадра 0: %.2f м\n",
           argv[a], sp.ntype, sp.ninst, noArtSt, noArtBare, inst0, inst0bare, followInst, maxOff);
  }
  return 0;
}
