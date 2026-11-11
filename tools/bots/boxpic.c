// №1223 (Кольцо-96): неподвижные ящики списка «коллизия без картинки» (Опора, tests/airwalls/fam.c не видит рисунок SPR1) против рисунка стейтпропов/анимированных объектов:
//   boxpic LEVEL HOMER ID...  -> строка на ящик (ближайшая вершина рисунка SPR1 в позе кадра 0), RESULT OK, если у каждого есть вершина ближе полудиагонали + 1,5 м
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
typedef struct { short u, v; unsigned int c; short x, y, z, pad; } V16;
static BotWorld W;
#define SLACK 1.5f
int main(int argc, char **argv) {
  int bad = 0;
  if (bwData(argv[1], argv[2], 0)) return 2; memset(&W,0,sizeof W); if (bwNew(&W)) return 2; const ColWorld *cw = bwCol();
  for (int a = 3; a < argc; a++) { unsigned b = atoi(argv[a]); const ColBox *B = &cw->box[b]; printf("box %u c(%.1f %.1f %.1f) h(%.2f %.2f %.2f)\n", b, B->c[0], B->c[1], B->c[2], B->h[0], B->h[1], B->h[2]);
    float best[3] = {1e9,1e9,1e9}; int bi[3] = {-1,-1,-1};
    for (unsigned i = 0; i < W.sp.ninst; i++) { const SpType *T = &W.sp.type[W.sp.inst[i].type]; if (!T->nPiece) continue; if (hypotf(W.sp.run[i].x - B->c[0], W.sp.run[i].z - B->c[2]) > 40.f) continue; spPose(&W.sp, i);
      float dm = 1e9f; for (unsigned p = 0; p < T->nPiece; p++) { const SpPiece *pc = &W.sp.piece[T->firstPiece + p]; float m[16]; spPieceMatrix(&W.sp, i, p, m); const V16 *v = (const V16 *)((const char *)W.sp.vertBase + pc->vOff);
        for (unsigned q = 0; q < pc->nv; q++) { float x = v[q].x/32768.f, y = v[q].y/32768.f, z = v[q].z/32768.f; float wx = m[0]*x+m[4]*y+m[8]*z+m[12], wy = m[1]*x+m[5]*y+m[9]*z+m[13], wz = m[2]*x+m[6]*y+m[10]*z+m[14]; float d = sqrtf((wx-B->c[0])*(wx-B->c[0])+(wy-B->c[1])*(wy-B->c[1])+(wz-B->c[2])*(wz-B->c[2])); if (d < dm) dm = d; } }
      for (int k = 0; k < 3; k++) if (dm < best[k]) { for (int j = 2; j > k; j--) { best[j] = best[j-1]; bi[j] = bi[j-1]; } best[k] = dm; bi[k] = (int)i; break; } }
    { float hd = sqrtf(B->h[0]*B->h[0] + B->h[1]*B->h[1] + B->h[2]*B->h[2]); int ok = bi[0] >= 0 && best[0] <= hd + SLACK; bad += !ok; printf("  -> %s (ближайшая вершина %.1f м, полудиагональ %.1f м)\n", ok ? "ok" : "БЕЗ РИСУНКА", best[0], hd); }
    for (int k = 0; k < 3; k++) if (bi[k] >= 0) printf("  ближайший рисунок SPR1: inst %d type %u (%s) nPiece %u dist до вершины %.1f м; центр inst (%.1f %.1f %.1f)\n", bi[k], W.sp.inst[bi[k]].type, "", W.sp.type[W.sp.inst[bi[k]].type].nPiece, best[k], W.sp.run[bi[k]].x, W.sp.run[bi[k]].y, W.sp.run[bi[k]].z); }
  printf("RESULT %s\n", bad ? "FAIL" : "OK"); return bad != 0; }
