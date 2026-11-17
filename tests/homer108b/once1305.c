// 108 1305: разовый предмет (ящик с монетами, правка 334), разбитый чужой машиной, не пишется в сохранение и возвращается по правилу района; разбитый игроком — пишется и не возвращается.
// L2 (настоящий пакет): берём разовый стейтпроп, машина без игрока (spHitOther) / с игроком (spUpdate-путь spHitCar) едет через него.
//   once1305 LEVEL.SHR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
#include "props.h"
#include "stateprops.h"
#include "savegame.h"
unsigned homerSolidFrame;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
static Props props; static StateProps sp; static Coins loot;
static void *readall(const char *path, size_t *size) { FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); *size = ftell(f); rewind(f); void *p = aligned_alloc(16, (*size + 15) & ~(size_t)15); if (!p || fread(p, 1, *size, f) != *size) return NULL; fclose(f); return p; }
static int never(void *u, float x, float y, float z, float r) { (void)u; (void)x; (void)y; (void)z; (void)r; return 0; }
static int capturedState(unsigned inst) { SvOnce o; svOnceCapture(&o, &sp); unsigned k = 0; for (unsigned i = 0; i < sp.ninst && k < o.n; i++) { if (!spOnce(&sp, i)) continue; if (i == inst) return (o.st[k >> 1] >> ((k & 1) * 4)) & 15u; k++; } return -1; }
int main(int argc, char **argv) {
  size_t ln; unsigned char *lv = readall(argv[1], &ln); static ColWorld W;
  unsigned co = *(unsigned *)(lv + 92), po = *(unsigned *)(lv + 96), pv = *(unsigned *)(lv + 88), ce = (unsigned)ln; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, lv + co, ce - co) || propsInit(&props, lv + po, *(unsigned *)(lv + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 2;
  coinsInit(&loot, NULL, 0); CarParams p; carParamsFor("famil_v", &p); int total = 0, okOther = 0, okPlayer = 0, tried = 0;
  for (unsigned i = 0; i < sp.ninst && tried < 6; i++) {
    if (!spOnce(&sp, i)) continue; const SpInst *I = &sp.inst[i]; float gy, n[3]; int mat; if (!colGroundMat(&W, I->x, I->z, I->y + 3.f, I->y - 6.f, &gy, n, &mat)) continue;
    for (int who = 0; who < 2 && tried < 6; who++) {   // 0 — чужая машина, 1 — игрок
      spZoneReload(&sp); for (unsigned j = 0; j < sp.ninst; j++) sp.run[j].byOther = 0;
      float ang = 0.f, sx = I->x - sinf(ang) * 9.f, sz = I->z - cosf(ang) * 9.f; Car c; carInit(&c, &p, &W, sx, gy, sz, ang); c.huskDrive = who; c.svPlayer = who; carSetSpeed(&c, 14.f);
      for (int f = 0; f < 120; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = 1.f; carUpdate(&c, &W, &in, 1.f / 60); if (who == 0) { spHitOtherWho = "traffic"; spHitOther(&sp, &W, &c); } else spHitCar(&sp, &W, &c, &loot); spUpdate(&sp, &W, &c, who ? &loot : NULL, 1.f / 60); }
      if (sp.run[i].state == 0) continue;   // не разбился на этом проезде — другой экземпляр
      tried++; total++;
      int saved = capturedState(i); for (int f = 0; f < 60 * 8; f++) spRespawnStep(&sp, I->x + 600.f, I->z, 1.f / 60, never, NULL);
      int back = sp.run[i].state == 0;
      printf("X экземпляр %u type=%u %s: в сохранении %d, вернулся %d, byOther %d\n", i, (unsigned)I->type, who ? "игрок" : "чужая", saved, back, sp.run[i].byOther);
      if (who == 0) okOther += (saved == 0 && back); else okPlayer += (saved != 0 && !back);
    } }
  printf("RESULT чужая %d, игрок %d из %d\n", okOther, okPlayer, total);
  return total >= 2 && okOther >= 1 && okPlayer >= 1 && okOther + okPlayer == total ? 0 : 1; }
