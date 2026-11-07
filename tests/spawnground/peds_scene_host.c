// A2 Опора-93: прохожие трафика не рождаются на неготовой сцене (канон п.11). Крючок sceneReadyHook подменён: «клетки» к востоку от x=EDGE не загружены.
// peds_scene_host LEVEL PLAYER [FRAMES] -> S<TAB>рождений всего<TAB>рождений на неготовой сцене
// Рождение = пешеход, которого не было в прошлом кадре на этом месте (режим OFF -> не OFF).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W; static float edge;
static int fakeReady(const ColWorld *w, float x, float z, int *failed) { (void)w; (void)z; if (failed) *failed = 0; return x < edge; }
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) return 2;
  int frames = argc > 3 ? atoi(argv[3]) : 3600;
  bwNew(&W);
  const Traffic *T = (const Traffic *)bwTraffic(); if (!T) { fprintf(stderr, "нет трафика\n"); return 3; }
  edge = W.car.x;   // восток от машины игрока — «холодный»
  sceneReadyHook = fakeReady;
  static int was[TR_MAXPED]; int total = 0, bad = 0;
  for (int f = 0; f < frames; f++) {
    bwStep(&W, 0, 0, 0);
    for (int i = 0; i < T->maxPeds && i < TR_MAXPED; i++) { int on = T->ped[i].mode != TRP_OFF;
      if (on && !was[i]) { total++; if (T->ped[i].x >= edge) bad++; }
      was[i] = on; }
  }
  printf("S\t%d\t%d\n", total, bad);
  return 0;
}
