// №1175: настоящие props.c / stateprops.c на данных уровня, без эмулятора. Каждый тип с вызовом BREAK (барьеры donotenter, gatesmasha, переезд, туннельный забор): удар машины (событие
// VEHICLE_HIT), 2 с кадров по 1/30. Печать: уровень, тип, число родных частиц обломка (выпуск с sys >= 0: система fx.shr, привязанная по имени, как в игре), число кусков обломка, запасных щепок пресета.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld world;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || !props.spr || spInit(&sp, props.spr, props.sprSize)) { printf("TYPES 0\n"); return 0; }
  sp.nativeBreakFx = 1;   // как в игре: уровень с fx.shr
  for (unsigned k = 0; k < spFxNameCount(&sp); k++) spFxBind(&sp, k, 100 + (int)k);   // система «найдена по имени» (номер условный)
  int types = 0, withFx = 0;
  for (unsigned t = 0; t < sp.ntype; t++) {
    const SpType *T = &sp.type[t]; int brk = 0;
    for (unsigned s = 0; s < T->nState && !brk; s++) { const SpState *S = &sp.state[T->firstState + s]; for (unsigned c = 0; c < S->nCall; c++) if (sp.call[S->firstCall + c].op == SP_OP_BREAK) brk = 1; }
    if (!brk || T->kind) continue;
    unsigned inst = ~0u; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { inst = i; break; }
    if (inst == ~0u) continue;
    spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return 3; sp.nativeBreakFx = 1;
    for (unsigned k = 0; k < spFxNameCount(&sp); k++) spFxBind(&sp, k, 100 + (int)k);
    { SpFxOut d_; for (int k = 0; k < 3; k++) { spUpdate(&sp, &world, 0, 0, 1.f / 30.f); while (spFxPop(&sp, &d_)) {} } }   // выпуск при загрузке (все системы уровня) — до удара, не считается
    if (!spEvent(&sp, inst, SP_EV_VEHICLE_HIT)) { printf("BARRIER type %u: удар не принят\n", t); continue; }
    int nat = 0, preset = 0; SpFxOut o;
    for (int fr = 0; fr < 60; fr++) { spUpdate(&sp, &world, 0, 0, 1.f / 30.f); while (spFxPop(&sp, &o)) { if (getenv("DBG")) printf("  fx sys %d preset %u frame %d\n", o.sys, o.preset, fr); if (o.sys >= 0) nat++; else preset++; } }
    int pcs = 0; for (unsigned i = 0; i < sp.ninst; i++) if (sp.run[i].state != 0 && sp.type[sp.inst[i].type].kind == 1) pcs += sp.type[sp.inst[i].type].nPiece;
    types++; withFx += nat > 0;
    printf("BARRIER type %u: родных частиц %d, кусков обломка %d, запасных щепок %d\n", t, nat, pcs, preset);
  }
  printf("TYPES %d\nWITHFX %d\n", types, withFx); return 0;
}
