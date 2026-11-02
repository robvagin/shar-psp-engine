// Круг 7 (g11b-collect, P04 W5): снаряд осы и мир через крюк svBoltWorld. Мир собран вручную (стена x = 20); крюк ставит тест: шар радиуса 0,6 в (14; 2; 10) изображает предмет мира.
// Ждём: снаряд взрывается у шара (x между 13,0 и 14,3, не долетает до стены), удар применён один раз; без крюка (NULL) или с WASPBOLTWORLD_OLD=1 — долетает до стены 20,75.
// Оса стоит в (10, 2, 10), игрок в 3 м прыгает (pJump 1 м — снаряд его пропускает) и снаряд летит в стену. Красный контроль: -DWASP_BOLT_BEHIND75_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "collect.h"
#include "col.h"
#include "sverka_shared.h"
static Collect c; static unsigned char *blob;
static int applied, queried; static float hitDir[3];
static float fakeWorld(const float a[3], const float b[3], const float dir[3], int apply) {
  const float cx = 14.f, cy = 2.f, cz = 10.f, R = .6f; queried++;
  float sx = b[0] - a[0], sy = b[1] - a[1], sz = b[2] - a[2], sl2 = sx * sx + sy * sy + sz * sz;
  float u = sl2 > 1e-9f ? ((cx - a[0]) * sx + (cy - a[1]) * sy + (cz - a[2]) * sz) / sl2 : 0.f; u = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
  float dx = a[0] + sx * u - cx, dy = a[1] + sy * u - cy, dz = a[2] + sz * u - cz;
  if (dx * dx + dy * dy + dz * dz > R * R) return 2.f;
  if (apply) { applied++; memcpy(hitDir, dir, sizeof hitDir); }
  return u;
}
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f);
  blob = aligned_alloc(16, (n + 15u) & ~15u); if (fread(blob, 1, n, f) != n) return 2; fclose(f);
  if (collectInit(&c, blob, n)) return 2;
  enum { W = 4, H = 4, CELLS = W * H };
  size_t sz = 48 + 32 + (CELLS + 1) * 4 + CELLS * 4;
  uint32_t *buf = aligned_alloc(16, (sz + 15) & ~15u); memset(buf, 0, sz);
  char *b = (char *)buf; memcpy(b, "COL1", 4);
  float *hf = (float *)(b + 4); hf[0] = 0.f; hf[1] = 0.f; hf[2] = 10.f;
  uint32_t *hu = (uint32_t *)(b + 16); hu[0] = W; hu[1] = H; hu[2] = 0; hu[3] = 0; hu[4] = 0; hu[5] = 1; hu[6] = CELLS; hu[7] = 0;
  float *sg = (float *)(b + 48); sg[0] = 20.f; sg[1] = 0.f; sg[2] = 20.f; sg[3] = 20.f; sg[4] = -1.f; sg[5] = 0.f; sg[6] = 0.f; sg[7] = 3.f;
  uint32_t *cs = (uint32_t *)(b + 48 + 32); for (int i = 0; i < CELLS + 1; i++) cs[i] = (uint32_t)i;
  uint32_t *rf = cs + CELLS + 1; for (int i = 0; i < CELLS; i++) rf[i] = 3u << 29;
  ColWorld w; if (colInit(&w, buf, (unsigned)sz)) { printf("FAIL colInit\n"); return 2; }
  int bi = -1; for (unsigned i = 0; i < c.nwasp; i++) if (!strcmp(c.wasp[i].name, "w_bonuscar")) bi = (int)i;
  CollWasp *wp = (CollWasp *)&c.wasp[bi]; wp->x = 10.f; wp->y = 2.f; wp->z = 10.f; wp->flags = 1; wp->rot = 0.f;
  static Homer hm; memset(&hm, 0, sizeof hm); hm.grounded = 0; hm.groundY = 0.8f;
  CollPlayer p = {13.f, 1.8f, 10.f, 1, -1, 0};
  svBoltWorld = (argc > 2 && !strcmp(argv[2], "null")) ? 0 : fakeWorld;
  float endx = -1.f; int fired = 0;
  for (int i = 0; i < 60 * 40 && endx < 0.f; i++) {
    collectStep(&c, &p, &hm, 0, &w, 0, 1.f / 60.f);
    if (c.st.boltFired && !fired) fired = 1;
    if (fired && c.st.boltOn[0] == 2) endx = c.sp.run[c.bolt0].x;
  }
  int hook = svBoltWorld != 0;
  int ok = hook ? (fired && endx > 13.0f && endx < 14.3f && applied == 1 && hitDir[0] > .9f) : (fired && fabsf(endx - 20.75f) < .3f && !queried);
  printf("%s круг 7 снаряд и мир: крюк %s, остановился на x=%.2f, ударов применено %d, спрошено %d, направление x %.2f (ждём: %s)\n", ok ? "PASS" : "FAIL", hook ? "есть" : "нет", endx, applied, queried, hitDir[0],
         hook ? "x у предмета 13,0…14,3, удар 1 раз" : "стена 20,75, мир не спрошен");
  printf("RESULT %s\n", ok ? "PASS" : "FAIL"); return ok ? 0 : 1;
}
