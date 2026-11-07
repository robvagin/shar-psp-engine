// A3 Опора-93: сторож, вид nofloor, машина. L3, полоса шириной ~1 м из крутых треугольников (n.y 0,49 < COL_WALK_NY) между двумя уровнями дороги
// (x -240…-239, z -267…-257, y -50,6 / -52,6): центр кузова над полосой — проходимой опоры под ним нет, но колёса стоят на дороге по обе стороны.
// carwheels_host LEVEL PLAYER -> S<TAB>постановок<TAB>строк WATCHDOG nofloor actor=car<TAB>из них с колёсами на опоре в момент срабатывания
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "watchdog.h"
static BotWorld W; static int nnf, nwheel;
static void logLine(const char *l) { if (strstr(l, "nofloor") && strstr(l, "actor=car")) { nnf++; if (W.car.wheels > 0) nwheel++; } }
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) return 2;
  bwNew(&W); wdSetLog(logLine);
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  int n = 0;
  for (float x = -240.4f; x < -238.9f; x += .2f) for (float z = -266.f; z < -257.f; z += 4.f) for (int k = 0; k < 4; k++) {
    bwPlace(&W, x, -50.8f, z, k * 1.5708f); n++;
    for (int f = 0; f < 300; f++) bwStep(&W, 0, 0, 0);
  }
  printf("S\t%d\t%d\t%d\n", n, nnf, nwheel);
  return 0;
}
