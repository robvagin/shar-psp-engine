// 108 1297: карточка из машины (collect.c cardsStep, carCardCorner108Old). Правило оригинала (Fable): точка машины — низ колёс + 0,63 м; углы кузова — родной радиус карточки.
// Часть A (синтетика, ровная земля y=0, настоящая карточка пакета перенесена, машина famil_v курсом 0): карточка на 3,2 м — под ней и в 1,5 м сбоку не берётся; на 1 м и в 2,4 м берётся.
// Часть B (настоящие L1–L7: пакет карточек и земля уровня): сколько карточек берётся машиной, стоящей под ними на земле, — старое (carCardCorner108Old=1) и новое.
//   card1297 A | card1297 B PACK LEVEL.SHR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "collect.h"
#include "car.h"
static void *readall(const char *path, size_t *size) { FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); *size = ftell(f); rewind(f); void *p = aligned_alloc(16, (*size + 15) & ~(size_t)15); if (!p || fread(p, 1, *size, f) != *size) return NULL; fclose(f); return p; }
unsigned homerSolidFrame;   // заглушка: props.c/stateprops.c ссылаются, в тесте без Гомера
static Collect c; static Coins loot; static Car car; static ColWorld flatW; static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } fl;
static int take(float cx, float cy, float cz, int i, float carh) {   // машина с низом колёс в (cx,cy,cz), берётся ли карточка i
  collectNewGame(&c); coinsFree(&loot); coinsInit(&loot, NULL, 0); car.h = carh; car.x = cx; car.y = cy; car.z = cz;
  CollPlayer p = {cx, cy, cz, 0, -1, 0}; collectStep(&c, &p, NULL, &car, NULL, &loot, 1.f / 60);
  CollEvent e; int n = 0; while (collectPop(&c, &e)) n += e.type == CE_CARD && e.idx == i; return n;
}
int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "A";
  memset(&fl, 0, sizeof fl); memcpy(fl.h.magic, "COL1", 4); fl.h.ox = fl.h.oz = -2000; fl.h.cell = 4000; fl.h.W = fl.h.H = 1; fl.h.ntri = fl.h.nref = 2;
  fl.t[0] = (ColTri){{-2000, 0, -2000}, {2000, 0, 2000}, {2000, 0, -2000}, {0, 1, 0}}; fl.t[1] = (ColTri){{-2000, 0, -2000}, {-2000, 0, 2000}, {2000, 0, 2000}, {0, 1, 0}}; fl.start[1] = 2; fl.refs[1] = 1;
  if (colInit(&flatW, &fl, sizeof fl)) return 2;
  CarParams cp; carParamsFor("famil_v", &cp); carInit(&car, &cp, &flatW, 0, 0, 0, 0);
  if (mode[0] == 'A') {
    size_t n; unsigned char *pak = readall(argv[2], &n); if (!pak || collectInit(&c, pak, n)) return 2;
    CollCard *k = (CollCard *)&c.card[0]; printf("карточка r=%.2f, машина halfL %.2f halfW %.2f\n", k->r, car.p.halfL, car.p.halfW);
    // карточка r=0,75: центр машины (низ + 0,63 новое / 0,9 старое) берёт на 2,25 м; углы кузова — 0,75 (новое) / 2,25 (старое)
    struct { const char *nm; float h, side, fwd, want; } T[] = {{"3,0 м, под ней", 3.0f, 0.f, 0.f, 0}, {"3,0 м, в 1,5 м сбоку", 3.0f, 1.5f, 0.f, 0}, {"1 м, под ней", 1.f, 0.f, 0.f, 1}, {"1 м, в 1,2 м сбоку", 1.f, 1.2f, 0.f, 1}, {"1,5 м, 3,6 м перед носом (угол 0,93)", 1.5f, 0.f, 3.6f, 0}};
    int bad = 0;
    for (int t = 0; t < 5; t++) { k->x = 50 + T[t].fwd * 0.f; k->y = T[t].h; k->z = 50 + T[t].fwd; int g = take(50 + T[t].side, 0, 50, 0, 0.f); printf("A %-38s берёт=%d (ждём %d)%s\n", T[t].nm, g, (int)T[t].want, g == (int)T[t].want ? "" : " <-- НЕ ТАК"); bad += g != (int)T[t].want; }
    printf("RESULT %s\n", bad ? "FAIL" : "PASS"); return bad ? 1 : 0;
  }
  size_t n; unsigned char *pak = readall(argv[2], &n); if (!pak || collectInit(&c, pak, n)) return 2;
  size_t ln; unsigned char *lv = readall(argv[3], &ln); static ColWorld W;
  unsigned co = *(unsigned *)(lv + 92), po = *(unsigned *)(lv + 96), pv = *(unsigned *)(lv + 88), ce = (unsigned)ln; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, lv + co, ce - co)) return 2;
  int ground = 0, tot = c.ncard;
  for (unsigned i = 0; i < c.ncard; i++) {
    const CollCard *k = &c.card[i]; float gy, nn[3]; int mat;
    if (!colGroundMat(&W, k->x, k->z, k->y + 50.f, k->y - 100.f, &gy, nn, &mat)) continue;
    float cy = gy; int got = 0; for (int hh = 0; hh < 4 && !got; hh++) got = take(k->x, cy, k->z, (int)i, hh * 1.5708f);
    ground += got;
  }
  printf("B level=%d карточек %d, берётся машиной с земли под ней: %d (carCardCorner108Old=%d)\n", c.level, tot, ground, carCardCorner108Old);
  return 0;
}
