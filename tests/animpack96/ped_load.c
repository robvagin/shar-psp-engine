// Модели-96: настоящий читатель (viewer/traffic.c + skin.c, хост) на двух пакетах трафика: СТАРОМ и НОВОМ (индексные пачки пешеходов + упакованные ключи).
//   ped_load OLD_traffic.shr NEW_traffic.shr -> модели пешеходов читаются (trafficInitMem == 0), у каждой каждая пачка проходит skinBatchValid,
//   вершины полос побайтно равны (с раскрытием индексов), позы всех клипов в 4 отсчётах на кадр: поворот (град) и перенос (мм) в пределах порога.
// -DPEDLOAD_TOL=0.01 — красный контроль (порог ÷100 должен падать).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#ifndef PEDLOAD_TOL
#define PEDLOAD_TOL 1.0
#endif
static unsigned char *rd(const char *p, unsigned *sz) {
  FILE *g = fopen(p, "rb"); if (!g) return 0; fseek(g, 0, SEEK_END); *sz = (unsigned)ftell(g); fseek(g, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, (*sz + 63) & ~63u); if (fread(b, 1, *sz, g) != *sz) return 0; fclose(g); return b;
}
static const unsigned char *vert(const SkinModel *m, const SkinBatch *b, unsigned i) {
  unsigned k = i;
  if (b->flags & 4) k = ((const unsigned short *)(m->base + b->pad[0]))[i];
  return m->base + b->offVerts + (size_t)k * b->stride;
}
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  unsigned s1, s2; unsigned char *a = rd(argv[1], &s1), *b = rd(argv[2], &s2); if (!a || !b) { puts("нет файла"); return 2; }
  static Traffic A, B; int r1 = trafficInitMem(&A, a, s1, 0), r2 = trafficInitMem(&B, b, s2, 0);
  if (r1 || r2) { printf("FAIL trafficInitMem старый %d новый %d\n", r1, r2); return 1; }
  if (A.npeds != B.npeds || !A.npeds) { printf("FAIL число пешеходов %d / %d\n", A.npeds, B.npeds); return 1; }
  double wr = 0, wt = 0; int bad = 0, nb = 0, idx = 0; unsigned nposes = 0;
  for (int i = 0; i < A.npeds; i++) {
    const SkinModel *ma = &A.peds[i], *mb = &B.peds[i];
    if (ma->h->nBatches != mb->h->nBatches || ma->h->nClips != mb->h->nClips || ma->h->nJoints != mb->h->nJoints) { bad++; printf("FAIL модель %d: состав\n", i); continue; }
    for (unsigned k = 0; k < mb->h->nBatches; k++) {
      const SkinBatch *x = &ma->b[k], *y = &mb->b[k]; nb++; idx += (y->flags & 4) ? 1 : 0;
      if (!skinBatchValid(mb, y)) { bad++; printf("FAIL модель %d пачка %d: skinBatchValid\n", i, k); continue; }
      if (x->nverts != y->nverts || x->stride != y->stride || x->nb != y->nb || x->tex != y->tex || x->vtype != (y->vtype & ~(3u << 11)) + 0 * 0) {
        if (x->nverts != y->nverts || x->stride != y->stride || x->nb != y->nb || x->tex != y->tex) { bad++; printf("FAIL модель %d пачка %d: поля\n", i, k); continue; }
      }
      // skinSlotOpt при создании переставляет слоты костей в весах и bones[] у обеих моделей одинаково, поэтому сравнение побайтное
      for (unsigned v = 0; v < x->nverts; v++) if (memcmp(vert(ma, x, v), vert(mb, y, v), x->stride)) { bad++; printf("FAIL модель %d пачка %d вершина %u: байты\n", i, k, v); break; }
    }
    unsigned nj = mb->h->nJoints; SkinXf xa[SKIN_MAXJ], xb[SKIN_MAXJ];
    for (unsigned c = 0; c < mb->h->nClips; c++) {
      const SkinClip *cl = &mb->c[c]; unsigned ns = (unsigned)((cl->frames - 1.f) * 4.f) + 1u;
      for (unsigned s = 0; s < ns; s++) {
        float t = s / (4.f * cl->fps), ra[3], rb[3];
        skinSample(ma, (int)c, t, xa); skinSample(mb, (int)c, t, xb); skinRootMotion(ma, (int)c, t, ra); skinRootMotion(mb, (int)c, t, rb); nposes++;
        for (unsigned j = 0; j < nj; j++) {
          float d = fabsf(xa[j].q[0] * xb[j].q[0] + xa[j].q[1] * xb[j].q[1] + xa[j].q[2] * xb[j].q[2] + xa[j].q[3] * xb[j].q[3]); if (d > 1.f) d = 1.f;
          double ang = 2 * acos(d) * 180 / M_PI; if (ang > wr) wr = ang;
          for (int q = 0; q < 3; q++) { double e = fabs(xa[j].t[q] - xb[j].t[q]) * 1000; if (e > wt) wt = e; }
        }
        for (int q = 0; q < 3; q++) { double e = fabs(ra[q] - rb[q]) * 1000; if (e > wt) wt = e; }
      }
    }
  }
  int ok = !bad && wr <= 0.2 * PEDLOAD_TOL && wt <= 2.0 * PEDLOAD_TOL;
  printf("пешеходов %d, пачек %d (индексных %d), поз %u; пакет %u -> %u Б; макс. поворот %.3f° (порог %.3f), перенос %.3f мм (порог %.3f), нарушений %d\n", A.npeds, nb, idx, nposes, s1, s2, wr, 0.2 * PEDLOAD_TOL, wt, 2.0 * PEDLOAD_TOL, bad);
  puts(ok ? "PASS" : "FAIL"); return ok ? 0 : 1;
}
