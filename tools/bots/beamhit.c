// Стенд №1223 (Build 94, Данные-93): машина в полёте пролетает сквозь невидимый висячий ящик коллизии (балки над ямой, L1/L4/L7 F4-7).
//   beamhit LEVEL PLAYER [--boxes i,j,...] [--v V] [--find x y z] [--track i] [-v]
// Для каждого ящика: машина стартует в воздухе в 10 м от центра, перпендикулярно его длинной оси, на высоте центра ящика, скорость V (по умолчанию 20 м/с),
// летит 3 с. Строка: B <индекс> <ПРОШЛА|БЛОК> c(x y z) размер конец_у_ящика v_конец. Для каждого ящика печатает, живой ли он (объект ANM1) и что за объект (строки O); --track i — центр живого ящика по времени.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: beamhit LEVEL PLAYER [--air FILE] [--boxes i,j] [--v V] [-v]\n"); return 2; }
  float V = 20.f; int ids[256], nid = 0, verbose = 0; 
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--v") && i + 1 < argc) V = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--boxes") && i + 1 < argc) { char *s = strtok(argv[++i], ","); while (s && nid < 256) { ids[nid++] = atoi(s); s = strtok(NULL, ","); } }
    else if (!strcmp(argv[i], "-v")) verbose = 1;
  }
  static BotWorld W; memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
  ColWorld *cw = (ColWorld *)bwCol();
  for (int i = 3; i + 3 < argc; i++) if (!strcmp(argv[i], "--find")) {   // служебно: индексы ящиков памяти с центром рядом с (x y z)
    float fx = (float)atof(argv[i + 1]), fy = (float)atof(argv[i + 2]), fz = (float)atof(argv[i + 3]);
    for (uint32_t b = 0; b < cw->nbox; b++) if (fabsf(cw->box[b].c[0] - fx) < .6f && fabsf(cw->box[b].c[1] - fy) < .6f && fabsf(cw->box[b].c[2] - fz) < .6f) printf("F\t%u c(%.2f %.2f %.2f)\n", b, cw->box[b].c[0], cw->box[b].c[1], cw->box[b].c[2]);
    printf("nbox %u\n", cw->nbox); return 0; }
  for (int i = 3; i + 1 < argc; i++) if (!strcmp(argv[i], "--track")) {   // служебно: центр живого ящика по времени, машина стоит рядом
    int b = atoi(argv[i + 1]); const ColBox *B = &cw->box[b]; carPlace(&W.car, cw, B->c[0] + 20.f, B->c[1], B->c[2], 0.f);
    for (int f = 0; f < 420; f++) { bwStep(&W, 0, 0, 0); if (f % 15 == 0) printf("T\t%.1f\t(%.2f %.2f %.2f)\n", f / 30.f, B->c[0], B->c[1], B->c[2]); }
    return 0; }
  int blocked = 0, passed = 0;
  for (int k = 0; k < nid; k++) {
    int i = ids[k]; if (i < 0 || (uint32_t)i >= cw->nbox) continue;
    const ColBox *B = &cw->box[i]; int la = 0; for (int a = 1; a < 3; a++) if (B->h[a] > B->h[la]) la = a;
    const float *ax = la == 0 ? B->ax : la == 1 ? B->ay : B->az;
    float px = -ax[2], pz = ax[0], pl = hypotf(px, pz); if (pl < 1e-3f) { px = 1; pz = 0; pl = 1; } px /= pl; pz /= pl;
    B = &cw->box[i];
    float sx = B->c[0] - px * 10.f, sz = B->c[2] - pz * 10.f, hd = atan2f(px, pz);
    carPlace(&W.car, cw, sx, B->c[1], sz, hd);
    W.car.y = B->c[1]; CarBox cb; carBox(&W.car, &cb); W.car.vel[0] = cb.fx * V; W.car.vel[2] = cb.fz * V; W.car.vel[1] = 0.f; W.car.v = V;
    { int live = -1; if (W.haveAo) for (unsigned q = 0; q < W.ao.nvol; q++) if (W.ao.vol[q].col == (uint32_t)i) { live = W.ao.vol[q].obj; break; }
      if (live >= 0) printf("O\t%d\tживой объём объекта %d имя %s действие %d кадров %d кол-во_экз %d\n", i, live, W.ao.nm ? W.ao.nm[live].name : "?", W.ao.obj[live].action, W.ao.obj[live].nfr, W.ao.obj[live].nInst);
      else printf("O\t%d\tне живой (статика)\n", i);
      if (live >= 0) { const AoObj *o = &W.ao.obj[live]; const SpInst *in = &W.sp.inst[o->firstInst]; const SpType *ty = &W.sp.type[in->type];
        { const SpState *st = &W.sp.state[ty->firstState]; const SpPiece *pc = &W.sp.piece[ty->firstPiece];
          printf("O\t%d\t  состояние0: vis=0x%x autoTo %u poseMode %d poseMin %u poseMax %u speed %.2f; кусок0: ox %.1f oy %.1f oz %.1f sc %.1f nv %u ni %u tex %d flags 0x%x; run %s Tc(%.1f %.1f %.1f) Th(%.1f %.1f %.1f)\n", i, st->vis, st->autoTo, st->poseMode, st->poseMin, st->poseMax, st->poseSpeed, pc->ox, pc->oy, pc->oz, pc->sc, pc->nv, pc->ni, pc->tex, pc->flags, W.sp.run ? "есть" : "нет", ty->c[0], ty->c[1], ty->c[2], ty->h[0], ty->h[1], ty->h[2]); }
        printf("O\t%d\t  объект: c(%.1f %.1f %.1f) h(%.1f %.1f %.1f) fps %.0f; экземпляр (%.1f %.1f %.1f) тип %u: кусков %u суставов %u кадров %u состояний %u\n", i, o->c[0], o->c[1], o->c[2], o->h[0], o->h[1], o->h[2], o->fps, in->x, in->y, in->z, in->type, ty->nPiece, ty->nJoint, ty->frames & 0xffff, ty->nState); } }
    float c0[3] = {B->c[0], B->c[1], B->c[2]}; float s0 = (W.car.x - B->c[0]) * px + (W.car.z - B->c[2]) * pz; int crossed = 0; float vmin = 1e9f;
    for (int f = 0; f < 180; f++) {
      bwStep(&W, 0, 0, 0);
      float s = (W.car.x - B->c[0]) * px + (W.car.z - B->c[2]) * pz, vh = hypotf(W.car.vel[0], W.car.vel[2]);
      if (vh < vmin) vmin = vh;
      if (s * s0 < 0) { crossed = 1; break; }
      if (vh < 1.f && f > 10) break;
    }
    float s = (W.car.x - B->c[0]) * px + (W.car.z - B->c[2]) * pz;
    int ok = crossed && vmin > 0.4f * V; if (ok) passed++; else blocked++;
    printf("B\t%d\t%s\tc(%.1f %.1f %.1f) h(%.2f %.2f %.2f)\tу_ящика %.1f v_мин %.1f\n", i, ok ? "ПРОШЛА" : "БЛОК", c0[0], c0[1], c0[2], B->h[0], B->h[1], B->h[2], s, vmin);
  }
  printf("ИТОГО ящиков %d · БЛОК %d · ПРОШЛА %d\n", nid, blocked, passed);
  return 0;
}
