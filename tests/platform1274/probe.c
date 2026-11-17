// 1274: разведка данных — стейтпропы около платформ АЭС (L1/L4): типы, суставы, следование объёма, поза по времени. Параметры: LEVEL.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps S;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&S, props.spr, props.sprSize);
  for (unsigned i = 0; i < S.ninst; i++) { const SpType *T = &S.type[S.inst[i].type]; const SpRun *r = &S.run[i];
    if (T->kind != 0) continue;
    float dx = r->x + 110.f, dz = r->z + 300.f; if (dx * dx + dz * dz > 40.f * 40.f) continue;
    const SpState *St = &S.state[T->firstState];
    printf("i=%u type=%u pos=%.1f,%.1f,%.1f cls=%d nState=%d nJoint=%d nCol=%d poseMode=%d poseMax=%.0f fps=%d follow=%d tag=%d\n", i, S.inst[i].type, r->x, r->y, r->z, T->cls, T->nState, T->nJoint, T->nCol, St->poseMode, St->poseMax, T->fps, S.follow && S.inst[i].type < S.nfollow ? S.follow[S.inst[i].type] : -1, S.inst[i].tag);
  }
  return 0;
}
