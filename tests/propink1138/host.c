// №1138 (g11): у какого числа стейтпропов уровня есть обводка в мультике — только будки. Данные L1–L7: для каждого типа SPR1 решение propInkWanted.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "props.h"
#include "stateprops.h"
#include "propink1138.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps sp;
int main(int argc, char **argv) {
  int types = 0, booth = 0, wanted = 0, inst = 0, instWanted = 0;
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) continue; fclose(f);
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    memset(&props, 0, sizeof props); memset(&sp, 0, sizeof sp); colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); if (spInit(&sp, props.spr, props.sprSize)) continue;
    for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; types++;
      int isB = T->nPiece == 7 && propInkIsBooth(T->nPiece, T->kind, sp.piece[T->firstPiece + 1].nv, sp.piece[T->firstPiece + 6].nv); booth += isB; wanted += propInkWanted(PROPINK1138_OLD, isB); }
    for (unsigned i = 0; i < sp.ninst; i++) { const SpType *T = &sp.type[sp.inst[i].type]; int isB = T->nPiece == 7 && propInkIsBooth(T->nPiece, T->kind, sp.piece[T->firstPiece + 1].nv, sp.piece[T->firstPiece + 6].nv); inst++; instWanted += propInkWanted(PROPINK1138_OLD, isB); }
  }
  printf("types %d\nbooth_types %d\nwanted_types %d\ninstances %d\nwanted_instances %d\n", types, booth, wanted, inst, instWanted);
  return 0;
}
