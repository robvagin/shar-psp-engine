// Гейт №1223 (Кольцо-96, список Опоры): рисунок и коллизия живых объектов ANM1 идут одной позой.
//   aodraw LEVEL HOMER [--samples N] [--slack M] [-v]   RESULT: OK / FAIL; красный контроль: AODRAW_FREEZE=1 (рисунок стоит в позе кадра 0, а ящики идут по клипу)
// Для каждого объекта с живыми ящиками (объёмы на суставе): машина рядом (объекты шагают, пока игрок ближе 90 м), N снимков по всему клипу; в каждом для каждого
// живого ящика (центр) — расстояние до ближайшей вершины ПОЗЫ РИСУНКА (все включённые куски всех экземпляров объекта, матрицей spPieceMatrix, как рисует main_loop.h).
// Ящик «призрак», если вершины дальше полудиагонали ящика + slack (по умолчанию 1,5 м). Объект без рисуемых кусков с живыми ящиками — тоже призрак.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
typedef struct { short u, v; unsigned int c; short x, y, z, pad; } V16;
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], NULL)) { fprintf(stderr, "usage: aodraw LEVEL HOMER [--samples N] [--slack M] [-v]\n"); return 2; }
  int ns = 8, verbose = 0; float slack = 1.5f; const char *fz = getenv("AODRAW_FREEZE");
  for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--samples") && i + 1 < argc) ns = atoi(argv[++i]); else if (!strcmp(argv[i], "--slack") && i + 1 < argc) slack = (float)atof(argv[++i]); else if (!strcmp(argv[i], "-v")) verbose = 1; }
  memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
#ifndef BW_HAVE_AO
  puts("нет animobj"); return 2;
#else
  const ColWorld *cw = bwCol(); int nobj = 0, nvolT = 0, ghost = 0, nodraw = 0, plates = 0;
  for (unsigned k = 0; k < W.ao.nobj; k++) {
    const AoObj *o = &W.ao.obj[k]; if (!o->nInst || !o->nVol) continue;
    unsigned nlive = 0; for (unsigned q = 0; q < W.ao.nvol; q++) if (W.ao.vol[q].obj == k && W.ao.vol[q].joint) nlive++;   // «живые»: объём едет за суставом
    if (!nlive) continue;
    nobj++; const SpInst *in0 = &W.sp.inst[o->firstInst];
    float worst = 0.f; unsigned wq = 0; int wph = 0, objGhost = 0, drawn = 0;
    carPlace(&W.car, cw, in0->x + 25.f, in0->y, in0->z, 0.f);
    float T = o->fps > 0.f && o->nfr > 1 ? (float)o->nfr / o->fps : 1.f;
    for (int s = 0; s < ns; s++) {
      for (int f = 0; f < (int)(T * 60.f / ns); f++) bwStep(&W, 0, 0, 0);
      for (unsigned q = 0; q < W.ao.nvol; q++) {
        const AoVol *V = &W.ao.vol[q]; if (V->obj != k || !V->joint) continue;
        float cc[3], hd;   // центр и полудиагональ ящика / охватывающая сфера цилиндра
        if (V->kind == 0) { const ColBox *b = &cw->box[V->col]; memcpy(cc, b->c, sizeof cc); hd = sqrtf(b->h[0] * b->h[0] + b->h[1] * b->h[1] + b->h[2] * b->h[2]); }
        else { const ColCyl *y = &cw->cyl[V->col]; memcpy(cc, y->c, sizeof cc); hd = sqrtf(y->r * y->r + y->halfLen * y->halfLen); }
        float dmin = 1e9f;
        for (unsigned ii = 0; ii < o->nInst; ii++) {
          unsigned i = o->firstInst + ii; SpRun *r = &W.sp.run[i]; float keep = r->poseF;
          if (fz) { r->poseF = 0.f; r->poseValid = 0; }
          spPose(&W.sp, i);
          const SpType *Ty = &W.sp.type[W.sp.inst[i].type];
          for (unsigned p = 0; p < Ty->nPiece; p++) {
            if (!spPieceOn(&W.sp, i, p)) continue; const SpPiece *pc = &W.sp.piece[Ty->firstPiece + p]; if (!pc->ni) continue;
            float m[16]; spPieceMatrix(&W.sp, i, p, m); const V16 *v = (const V16 *)((const char *)W.sp.vertBase + pc->vOff); drawn = 1;
            for (unsigned a = 0; a < pc->nv; a += 1) {
              float x = v[a].x / 32768.f, y = v[a].y / 32768.f, z = v[a].z / 32768.f;
              float wx = m[0] * x + m[4] * y + m[8] * z + m[12], wy = m[1] * x + m[5] * y + m[9] * z + m[13], wz = m[2] * x + m[6] * y + m[10] * z + m[14];
              float d = sqrtf((wx - cc[0]) * (wx - cc[0]) + (wy - cc[1]) * (wy - cc[1]) + (wz - cc[2]) * (wz - cc[2])); if (d < dmin) dmin = d; } }
          if (fz) { r->poseF = keep; r->poseValid = 0; }
        }
        float excess = dmin - hd; if (excess > worst) { worst = excess; wq = V->col; wph = s; }
        if (verbose) printf("v\tобъект %u %s %u ф%d dmin %.2f полудиагональ %.2f\n", k, V->kind ? "цилиндр" : "ящик", V->col, s, dmin, hd);
      }
    }
    // объект без рисунка SPR1, чьи объёмы все плоские (толщина до 0,3 м: плита подкидывающей точки под пружиной, рисунок в статике города), стеной не бывает: отдельная графа
    int flat = 0; if (!drawn) { flat = 1; for (unsigned q = 0; q < W.ao.nvol; q++) { const AoVol *V = &W.ao.vol[q]; if (V->obj != k || !V->joint) continue;
        if (V->kind != 0) { flat = 0; break; } const ColBox *b = &cw->box[V->col]; float up = fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2]; if (up > .15f) { flat = 0; break; } } }
    int gh = (!drawn && !flat) || (drawn && worst > slack); plates += flat; objGhost += gh; ghost += gh; nodraw += !drawn && !flat; nvolT += (int)nlive;
    printf("O\t%2u %-22s живых ящиков %2u, рисуется %s, худший запас %.2f м (объём %u, фаза %d) -> %s\n", k, W.ao.nm ? W.ao.nm[k].name : "?", nlive, drawn ? "да" : flat ? "нет (плоская плита)" : "НЕТ", worst, wq, wph, gh ? "ПРИЗРАК" : flat ? "плита" : "ok");
  }
  printf("объектов с живыми ящиками %d, ящиков %d, призраков (объектов) %d (без рисунка %d, плоских плит %d)\nRESULT %s\n", nobj, nvolT, ghost, nodraw, plates, ghost ? "FAIL" : "OK");
  return ghost != 0;
#endif
}
