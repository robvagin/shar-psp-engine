// №1102 (g11): лифты и платформы только по △. Хост на данных уровней (ANM1/SPR1/COL1 из level.shr): для каждого объекта с действием PlayAnim / Toggle
// (подъёмник Duff, платформы АЭС): (а) Гомер стоит на платформе без △ — клип не идёт (в старом режиме часть объектов едет сама); (б) Гомер в объёме выключателя — подсказка △ есть
// (aoWantsAction == объект), △ запускает клип. Печатает итоги по уровню. Параметры: LEVEL.shr [LEVEL2.shr ...]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "animobj.h"
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
static unsigned char *rd(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (n + 15) & ~15); if (fread(d, 1, n, f) != (size_t)n) return 0; fclose(f); return d; }
int main(int argc, char **argv) {
  int nObj = 0, nHint = 0, nPress = 0, nAuto = 0, nNoTrig = 0;
  for (int a = 1; a < argc; a++) {
    unsigned char *data = rd(argv[a]); if (!data) continue;
    static Props props; static ColWorld world; static AnimObjs ao; static StateProps sp;
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88); FILE *f = fopen(argv[a], "rb"); fseek(f, 0, SEEK_END); unsigned ce = (unsigned)ftell(f); fclose(f);
    if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    memset(&props, 0, sizeof props); memset(&ao, 0, sizeof ao); memset(&sp, 0, sizeof sp);
    colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    if (aoInit(&ao, data + po, *(unsigned *)(data + po + 40), &sp, &world)) continue;
    for (unsigned k = 0; k < ao.nobj; k++) {
      const AoObj *o = &ao.obj[k]; if (o->action != AO_PLAYANIM && o->action != AO_TOGGLE) continue;
      nObj++;
      // (а) на платформе без △: на верху первого живого ящика объекта (его положение — после первого шага далеко от игрока)
      aoStep(&ao, &sp, &world, 1e4f, 0.f, 1e4f, 1, 0, 1.f / 30.f);
      float fx = 0, fy = 0, fz = 0; int haveBox = 0;
      for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol && !haveBox; v++) {
        const AoVol *V = &ao.vol[v]; if (V->kind != 0) continue; const ColBox *bx = &world.box[V->col];
        fx = bx->c[0]; fz = bx->c[2]; fy = bx->c[1] + fabsf(bx->ax[1]) * bx->h[0] + fabsf(bx->ay[1]) * bx->h[1] + fabsf(bx->az[1]) * bx->h[2]; haveBox = 1; }
      if (haveBox) for (int fr = 0; fr < 150; fr++) aoStep(&ao, &sp, &world, fx, fy, fz, 1, 0, 1.f / 30.f);
      if (ao.run[k].playing) nAuto++;
      // (б) в объёме выключателя
      memset(&ao.run[k], 0, sizeof ao.run[k]);
      aoStep(&ao, &sp, &world, o->c[0], o->c[1], o->c[2], 1, 0, 1.f / 30.f);
      int hint = aoWantsAction(&ao) == (int)k; nHint += hint; if (!hint) nNoTrig++;
      aoStep(&ao, &sp, &world, o->c[0], o->c[1], o->c[2], 1, 1, 1.f / 30.f);
      nPress += ao.run[k].playing;
    }
  }
  printf("objects %d\nauto_started %d\nhint %d\npress_started %d\nno_hint %d\n", nObj, nAuto, nHint, nPress, nNoTrig);
  return 0;
}
