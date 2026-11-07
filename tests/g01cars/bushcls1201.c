// №1201б: подбираемые предметы (ключи, карточки: класс 2, масса 0, ATC 0) не получают тело куста (№1137); кусты (ATC 10) получают. tests/g01cars/bushcls1201.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld W;
int main(int argc, char **argv) {
  int pickBody = 0, shrubBody = 0, shrubTypes = 0, pickTypes = 0;
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
    unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 99; fclose(f);
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
    if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 98;
    for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; unsigned n = 0; for (unsigned i = 0; i < sp.ninst; i++) n += sp.inst[i].type == t; if (!n) continue;
      int cand = T->cls == 2 && !T->kind && T->nPiece && 2.f * T->h[1] >= .4f && 2.f * T->h[1] <= 3.f && fmaxf(T->h[0], T->h[2]) <= 2.5f;
      if (cand && T->atc == 0 && T->mass == 0.f) { pickTypes++; pickBody += T->nCol > 0; }
      if (cand && T->atc == 10 && T->mass > 0.f) { shrubTypes++; shrubBody += T->nCol > 0; } }
    free(data); spFree(&sp); }
  printf("подборы (ATC 0, масса 0): типов %d, с телом %d; кусты (ATC 10): типов %d, с телом %d\n", pickTypes, pickBody, shrubTypes, shrubBody);
  return 0;   // решение — в bushcls1201.sh (сравнение с базой без тел)
}
