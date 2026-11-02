// Проверка круга 5 (g11b-collect, P11-Д5): забор в луче осы перекрывает рамку по двум концам отрезка (диагональный забор закрывает весь прямоугольник).
// Мир собран вручную: один забор по диагонали от (5,5) до (25,25), высота 0..3. Луч идёт рядом и параллельно, не пересекая сам отрезок.
// Без выключателя (colRayFenceBox = 0) луч проходит; с ним — перекрыт; луч за пределами рамки проходит всегда.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "col.h"
int main(void) {
  enum { W = 4, H = 4, CELLS = W * H };
  size_t sz = 48 + 32 + (CELLS + 1) * 4 + CELLS * 4;
  uint32_t *buf = aligned_alloc(16, (sz + 15) & ~15u); memset(buf, 0, sz);
  char *b = (char *)buf; memcpy(b, "COL1", 4);
  float *hf = (float *)(b + 4); hf[0] = 0.f; hf[1] = 0.f; hf[2] = 10.f;
  uint32_t *hu = (uint32_t *)(b + 16); hu[0] = W; hu[1] = H; hu[2] = 0; hu[3] = 0; hu[4] = 0; hu[5] = 1; hu[6] = CELLS; hu[7] = 0;
  float *sg = (float *)(b + 48);   // a[2], b[2], n[2], y0, y1
  float dx = 20.f, dz = 20.f, l = 28.2842712f; sg[0] = 5.f; sg[1] = 5.f; sg[2] = 25.f; sg[3] = 25.f; sg[4] = -dz / l; sg[5] = dx / l; sg[6] = 0.f; sg[7] = 3.f;
  uint32_t *cs = (uint32_t *)(b + 48 + 32); for (int i = 0; i <= CELLS; i++) cs[i] = (uint32_t)i;
  uint32_t *rf = cs + CELLS + 1; for (int i = 0; i < CELLS; i++) rf[i] = 3u << 29;
  ColWorld w; int rc = colInit(&w, buf, (unsigned)sz); if (rc) { printf("FAIL colInit %d\n", rc); return 2; }
  int fail = 0;
  float a[3] = {8.f, 1.f, 16.f}, c[3] = {16.f, 1.f, 24.f};         // параллельно диагонали, внутри рамки
  float a2[3] = {30.f, 1.f, 30.f}, c2[3] = {36.f, 1.f, 30.f};      // вне рамки
  colRayFenceBox = 0; float f0 = colRayFrac(&w, a, c, 0);
  colRayFenceBox = 1; float f1 = colRayFrac(&w, a, c, 0), f2 = colRayFrac(&w, a2, c2, 0);
  colRayFenceBox = 0;
  printf("%s луч рядом с диагональным забором без рамки проходит (%.2f)\n", f0 >= 1.f ? "PASS" : "FAIL", f0); fail += f0 < 1.f;
  printf("%s с рамкой по двум концам луч внутри рамки перекрыт (%.2f)\n", f1 < 1.f ? "PASS" : "FAIL", f1); fail += f1 >= 1.f;
  printf("%s луч вне рамки проходит и с рамкой (%.2f)\n", f2 >= 1.f ? "PASS" : "FAIL", f2); fail += f2 < 1.f;
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return fail ? 1 : 0;
}
