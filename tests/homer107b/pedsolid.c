// Гейт «невидимое пешком» (окно Гомер 107b; сверка Fable-2 П7/П8): homerSolidSprops на реальных стейтпропах уровней 1–7.
//   П7: для каждого экземпляра типа со следованием ставим Гомера в центр объёма, каким его видит МАШИНА (spVolumeCenter, поза с макс. отходом от кадра 0),
//       на высоте низа объёма. Новое: выталкивает всегда. Старое (объём в позе кадра 0): не выталкивает там, где объём ушёл от позы больше своего размера.
//   П8: у типа с состоянием 0 без событий/вызовов/автоперехода (реальных таких в L1–L7 для Гомера нет: L7 тип 76 это cls 2, Гомер их и так пропускает) стираем маску рисунка в копии
//       данных (состояние «ничего не рисуется»), ставим Гомера в центр. Новое: не выталкивает. Старое: выталкивает. Контроль: с рисунком выталкивает в обеих сборках.
//   cc ... -DSKIN_HOST [-DHOMERSP107B_OLD=1] tests/homer107b/pedsolid.c viewer/{homer,watchdog,skin,col,car,props,stateprops,coins}.c -lm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "col.h"
#include "props.h"
#include "stateprops.h"
void uiFlash(void) {}
static unsigned char *rd(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (*n + 15) & ~15); if (fread(d, 1, *n, f) != (size_t)*n) return 0; fclose(f); return d; }
static Props props; static ColWorld world; static StateProps sp;
static void mulRR(const float R[9], const float v[3], float o[3]) { o[0] = R[0] * v[0] + R[3] * v[1] + R[6] * v[2]; o[1] = R[1] * v[0] + R[4] * v[1] + R[7] * v[2]; o[2] = R[2] * v[0] + R[5] * v[1] + R[8] * v[2]; }
// верхний полуразмер объёма (как в homerSolidSprops)
static float halfH(const SpRun *R, const SpCol *C) {
  float a[3][3], ey = 0.f; for (int m = 0; m < 3; m++) mulRR(R->R, C->a + 3 * m, a[m]);
  if (C->kind == 1) return C->h[0]; for (int m = 0; m < 3; m++) ey += C->h[m] * fabsf(a[m][1]); return ey; }
// Гомер в точке (x, z) на высоте низа объёма: выталкивает ли какой-либо стейтпроп. Возврат 1 — вытолкнул
static int probe(float x, float z, float y) { float px = x, pz = z, top = -1e9f, nx = 0, nz = 0; return homerSolidSprops(&sp, &px, &pz, y, .35f, &top, &nx, &nz); }
int main(int argc, char **argv) {
  int tot7 = 0, push7 = 0, tot8 = 0, push8 = 0, ctl8 = 0, tot9 = 0, ctl9 = 0, push9 = 0;
  for (int a = 1; a + 1 < argc; a += 2) {
    long size, nn; unsigned char *data = rd(argv[a], &size), *names = rd(argv[a + 1], &nn); if (!data || !names) { printf("нет %s\n", argv[a]); return 2; }
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    memset(&props, 0, sizeof props); memset(&sp, 0, sizeof sp);
    colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    unsigned ny = *(unsigned *)(names + 16), oy = *(unsigned *)(names + 28); spFollowAdopt(&sp, names + oy, ny);
    // --- П7
    for (unsigned i = 0; i < sp.ninst; i++) {
      unsigned t = sp.inst[i].type; if (!sp.follow || t >= sp.nfollow || !sp.follow[t]) continue;
      const SpType *T = &sp.type[t]; if (!T->nCol || T->kind || T->cls == 2) continue;
      SpRun *r = &sp.run[i]; r->alive = r->colOn = r->col2On = 1;
      const SpCol *C = &sp.col[T->firstCol]; if (C->second) continue;
      unsigned nfr = T->frames & 0xFFFF; float bestD = 0, bc[3] = {0, 0, 0}; float base[3];
      float cc[3]; mulRR(r->R, C->c, cc); base[0] = cc[0] + r->x; base[1] = cc[1] + r->y; base[2] = cc[2] + r->z;
      for (unsigned f = 0; f <= nfr; f += (nfr > 40 ? nfr / 40 : 1)) {
        r->poseF = (float)f; r->poseValid = 0; float c[3]; if (!spVolumeCenter(&sp, i, 0, c)) break;
        float d = sqrtf((c[0] - base[0]) * (c[0] - base[0]) + (c[2] - base[2]) * (c[2] - base[2]));
        if (d > bestD) { bestD = d; memcpy(bc, c, sizeof bc); }
      }
      float ey = halfH(r, C); if (bestD < .25f || 2.f * ey < HOMER_STEP + .2f) continue;   // объём ушёл заметно и достаточно высокий, чтобы не «встать сверху»
      // поза с макс. отходом: вернуть
      float best = -1; unsigned bf = 0; for (unsigned f = 0; f <= nfr; f += (nfr > 40 ? nfr / 40 : 1)) { r->poseF = (float)f; r->poseValid = 0; float c[3]; spVolumeCenter(&sp, i, 0, c);
        float d = sqrtf((c[0] - base[0]) * (c[0] - base[0]) + (c[2] - base[2]) * (c[2] - base[2])); if (d > best) { best = d; bf = f; } }
      r->poseF = (float)bf; r->poseValid = 0; float c[3]; spVolumeCenter(&sp, i, 0, c);
      // изолировать экземпляр: остальные выключаем на время пробы
      static unsigned char keep[8192]; if (sp.ninst > sizeof keep) return 3;
      for (unsigned j = 0; j < sp.ninst; j++) { keep[j] = sp.run[j].colOn; if (j != i) sp.run[j].colOn = 0; }
      int p = probe(c[0], c[2], c[1] - ey); tot7++; push7 += p;
      for (unsigned j = 0; j < sp.ninst; j++) sp.run[j].colOn = keep[j];
      r->poseF = 0.f; r->poseValid = 0;
    }
    // --- П8: стираем рисунок у каждого подходящего типа (по одному экземпляру), которому это можно сделать (visWords 1)
    for (unsigned t = 0; t < sp.ntype; t++) {
      const SpType *T = &sp.type[t]; if (T->kind || !T->nCol || T->cls == 2 || T->visWords > 1) continue;
      SpState *S = (SpState *)&sp.state[T->firstState]; if (S->nFx || S->nEvent || S->nCall || S->autoTo != 0xFF || !S->vis) continue;
      unsigned inst = 0xFFFFFFFFu; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { inst = i; break; }
      if (inst == 0xFFFFFFFFu) continue;
      SpRun *r = &sp.run[inst]; r->alive = r->colOn = r->col2On = 1; r->state = 0; r->poseF = 0.f; r->poseValid = 0;
      const SpCol *C = &sp.col[T->firstCol]; if (C->second) continue; float ey = halfH(r, C); if (2.f * ey < HOMER_STEP + .2f) continue;
      float c[3]; spVolumeCenter(&sp, inst, 0, c);
      static unsigned char keep[8192]; for (unsigned j = 0; j < sp.ninst; j++) { keep[j] = sp.run[j].colOn; if ((unsigned)j != inst) sp.run[j].colOn = 0; }
      int withArt = probe(c[0], c[2], c[1] - ey);                    // контроль: с рисунком твёрдо в обеих сборках
      uint32_t v0 = S->vis; S->vis = 0;
      int noArt = probe(c[0], c[2], c[1] - ey);                       // состояние «ничего не рисуется»
      S->vis = v0; for (unsigned j = 0; j < sp.ninst; j++) sp.run[j].colOn = keep[j];
      tot8++; ctl8 += withArt; push8 += noArt;
    }
    // --- П9: у типа с несколькими состояниями и твёрдым объёмом в состоянии 0 (cls не куст) ставим состояние 1: «отреагировал»
    for (unsigned t = 0; t < sp.ntype; t++) {
      const SpType *T = &sp.type[t]; if (T->kind || !T->nCol || T->cls == 2 || T->nState < 2) continue;
      unsigned inst = 0xFFFFFFFFu; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { inst = i; break; }
      if (inst == 0xFFFFFFFFu) continue;
      SpRun *r = &sp.run[inst]; r->alive = r->colOn = r->col2On = 1; r->state = 0; r->poseF = 0.f; r->poseValid = 0;
      const SpState *S0 = &sp.state[T->firstState]; if (spNoSolid(&sp, inst)) continue;
      const SpCol *C = &sp.col[T->firstCol]; if (C->second) continue; float ey = halfH(r, C); if (2.f * ey < HOMER_STEP + .2f) continue;
      float c[3]; spVolumeCenter(&sp, inst, 0, c);
      static unsigned char keep[8192]; for (unsigned j = 0; j < sp.ninst; j++) { keep[j] = sp.run[j].colOn; if ((unsigned)j != inst) sp.run[j].colOn = 0; }
      int whole = probe(c[0], c[2], c[1] - ey);          // целый: твёрдый в обеих сборках
      r->state = 1; r->poseValid = 0; int hasArt = !spNoSolid(&sp, inst); int reacted = probe(c[0], c[2], c[1] - ey); r->state = 0; r->poseValid = 0;
      for (unsigned j = 0; j < sp.ninst; j++) sp.run[j].colOn = keep[j]; (void)S0;
      if (!hasArt) continue;   // состояние 1 без рисунка уже снимает П8, не П9: не свидетель
      tot9++; ctl9 += whole; push9 += reacted;
    }
  }
  printf("p7_total %d\np7_pushed %d\np8_total %d\np8_control_pushed %d\np8_noart_pushed %d\np9_total %d\np9_whole_pushed %d\np9_reacted_pushed %d\n", tot7, push7, tot8, ctl8, push8, tot9, ctl9, push9);
  return 0;
}
