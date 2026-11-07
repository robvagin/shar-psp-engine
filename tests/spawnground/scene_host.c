// Гейт правки 431 (вторая половина): сцена города под точкой постановки не загрузилась. Крючок готовности подменён: «ещё грузится» (1,5 с) и «не загрузилась вообще» (state 3, никогда).
//   scene_host LEVEL PLAYER   -> S<TAB>держал_при_грузится_с<TAB>просьб_в_лестницу<TAB>держал_при_отказе_с<TAB>строк_cellfail<TAB>упал_ниже_опоры
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "watchdog.h"
static BotWorld W; static float simT; static float readyAt; static int failedMode, wants, cellfail;
static int ready(const ColWorld *w, float x, float z, int *failed) { (void)w; (void)x; (void)z; if (failed) *failed = failedMode; return !failedMode && simT >= readyAt; }
static void want(const ColWorld *w, float x, float z) { (void)w; (void)x; (void)z; wants++; }
static void logLine(const char *l) { if (strstr(l, "cellfail")) cellfail++; if (getenv("SCENE_VERBOSE")) printf("L\t%s\n", l); }
static float run(float seconds, int *fell) {   // сколько секунд герой ждал (pending), и не падал ли
  float held = 0.f; const ColWorld *cw = bwCol();
  for (int f = 0; f < (int)(seconds * 60.f); f++) {
    bwStep(&W, 0, 0, 0); simT += 1.f / 60.f;
    if (homerSpawnPending(&W.homer)) held += 1.f / 60.f;
    float g; if (!homerSpawnPending(&W.homer) && !W.homer.grounded && !colGround(cw, W.homer.x, W.homer.z, W.homer.y + 2.f, W.homer.y - 2.f, &g, 0)) *fell = 1;
  }
  return held;
}
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) return 2;
  const ColWorld *cw = bwCol(); bwNew(&W); wdSetLog(logLine); sceneReadyHook = ready; sceneWantHook = want;
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float px = W.car.x + 3.f, pz = W.car.z, py = W.car.y, g; int fell = 0;
  if (!colGround(cw, px, pz, py + 2.f, py - 5.f, &g, 0)) { px = W.car.x; pz = W.car.z; }
  simT = 0.f; readyAt = 1.5f; failedMode = 0; wants = 0;
  homerPlace(&W.homer, cw, px, py, pz, 0.f);
  float h1 = run(4.f, &fell);
  failedMode = 1; cellfail = 0; int wants1 = wants; wants = 0;
  homerPlace(&W.homer, cw, px + 1.f, py, pz, 0.f);
  float h2 = run(9.f, &fell);
  printf("S\t%.2f\t%d\t%.2f\t%d\t%d\n", h1, wants1, h2, cellfail, fell);
  return 0;
}
