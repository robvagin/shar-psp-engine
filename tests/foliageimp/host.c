// Круг 7 (g11, P11-14): частица листвы по импульсу (50 Н·с = 10 × масса строки ATC листвы 5 кг), не по скорости 3 м/с. Хост: порог и езда в изгородь на малой скорости.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "car.h"
void uiFlash(void) {}
unsigned homerSolidFrame = 0;
static ColWorld world; static Car c;
int main(int argc, char **argv) {
  printf("fx_1500_0p5 %d\nfx_1500_0p02 %d\nfx_1500_5 %d\n", foliageFxOk(1500.f, 0.5f), foliageFxOk(1500.f, 0.02f), foliageFxOk(1500.f, 5.f));
  if (argc < 2) return 0;
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co);
  // езда в листву на 1,5 м/с: по всем сегментам листвы мира, ближайший к спавну; считаем заезды, где foliV > 0
  int n = 0, got = 0;
  for (int k = 0; k < 40; k++) {
    float ang = k * 0.157f, x = 220.f + cosf(ang) * (10.f + k), z = 172.f + sinf(ang) * (10.f + k), g = 0;
    if (!colGround(&world, x, z, 40, -50, &g, 0)) continue;
    carInit(&c, 0, &world, x, g + .6f, z, ang + 1.57f); c.svPlayer = 1; carSetSpeed(&c, 1.5f);
    int fol = 0; for (int fr = 0; fr < 90; fr++) { CarInput in = {0.f, 0, 0, 0}; carUpdate(&c, &world, &in, 1.f / 30.f); if (c.foliV > 0.f) fol = 1; }
    n++; got += fol;
  }
  printf("slow_runs %d\nslow_foliage %d\n", n, got);
  return 0;
}
