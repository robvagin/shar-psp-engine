// 108 1301: персонаж миссии (Мардж m0) идёт по маршруту через твёрдый предмет/стейтпроп уровня 1: не должен оказаться внутри него.
// Находим вблизи Мардж твёрдую точку P (propsSolidHomer или homerSolidSprops) со свободным подходом 3 м по одному из 8 направлений, ставим Мардж в P − 3u, путь в P + 3u, 10 с.
//   npcprop LEVEL HOMER MSN -> строки "P ..." и "RESULT x кадров внутри"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
static int solidAt(float x, float z, float y) { float px = x, pz = z, t, a, b; if (propsSolidHomer(&W.props, &px, &pz, y, GAME_NPC_RADIUS, HOMER_STEP, HOMER_HEIGHT, &t, &a, &b)) return 1; px = x; pz = z; return homerSolidSprops(&W.sp, &px, &pz, y, GAME_NPC_RADIUS, &t, &a, &b); }
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwPropsSolid = 1; bwNew(&W);
  GameNpc *mg = (GameNpc *)gameMsnNpc(&W.msn, "marge"); if (!mg) return 2;
  mg->walking = 0; mg->nwp = 0; float y0 = mg->pos.y, cx = mg->pos.x, cz = mg->pos.z; int found = 0, inside = 0, tests = 0;
  for (float r = 4.f; r < 100.f && found < 6; r += 1.f) for (int k = 0; k < 48 && found < 6; k++) {
    float a = k * 0.1309f, px = cx + sinf(a) * r, pz = cz + cosf(a) * r; if (!solidAt(px, pz, y0)) continue;
    for (int d = 0; d < 8; d++) { float u = d * 0.7854f, ux = sinf(u), uz = cosf(u); int free_ = 1;
      for (float s = 0.5f; s <= 3.f; s += .25f) if (solidAt(px - ux * s, pz - uz * s, y0)) { free_ = 0; break; }
      if (!free_ || solidAt(px + ux * 3.f, pz + uz * 3.f, y0)) continue;
      mg->pos.x = px - ux * 3.f; mg->pos.z = pz - uz * 3.f; mg->pos.y = y0; mg->h = u; mg->nwp = 1; mg->iwp = 0; mg->walking = 1; mg->wp[0].x = px + ux * 3.f; mg->wp[0].y = y0; mg->wp[0].z = pz + uz * 3.f;
      int in = 0; float dmin = 1e9f; for (int f = 0; f < 600; f++) { bwStep(&W, 0, 0, 0); if (solidAt(mg->pos.x, mg->pos.z, mg->pos.y)) in++; float dd = hypotf(mg->pos.x - px, mg->pos.z - pz); if (dd < dmin) dmin = dd; }
      printf("P (%.1f;%.1f) курс %d: кадров внутри предмета %d, ближе всего к точке %.2f м, конец (%.1f;%.1f)\n", px, pz, d, in, dmin, mg->pos.x, mg->pos.z); inside += in; tests++; found++; break; } }
  printf("RESULT испытаний %d, кадров внутри предметов %d\n", tests, inside);
  return tests >= 3 && inside == 0 ? 0 : 1;
}
