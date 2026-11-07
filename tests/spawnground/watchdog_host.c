// Приёмка сторожа (канон п.12): каждый вид искусственно ломается (wdBreakMask), сторож обязан починить и написать строку WATCHDOG <вид>.
//   watchdog_host LEVEL PLAYER BITS FRAMES   -> S<TAB>строк_сторожа_нужного_вида<TAB>починок_ok<TAB>мин_y_героя_минус_дно<TAB>герой_на_опоре_в_конце
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "watchdog.h"
static BotWorld W; static int nline, nok;
static const char *kind;
static void logLine(const char *l) { if (!strstr(l, kind)) return; nline++; if (strstr(l, "ok=1")) nok++; if (getenv("WD_VERBOSE")) printf("L\t%s\n", l); }
int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "watchdog_host LEVEL PLAYER KIND BITS FRAMES\n"); return 2; }
  if (bwData(argv[1], argv[2], 0)) return 2;
  kind = argv[3]; wdBreakMask = atoi(argv[4]); int frames = atoi(argv[5]);
  const ColWorld *cw = bwCol(); float floor_ = 1e9f;
  for (uint32_t i = 0; i < cw->ntri; i++) { const ColTri *T = &cw->tri[i]; floor_ = fminf(floor_, fminf(T->a[1], fminf(T->b[1], T->c[1]))); }
  bwNew(&W); wdSetLog(logLine);
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float sx = W.car.x, sz = W.car.z;
  homerPlace(&W.homer, cw, sx + 3.f, W.car.y, sz, 0.f); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  float miny = 1e9f; float g;
  for (int f = 0; f < frames; f++) { bwStep(&W, 0, 0, 0); float y = W.homer.y - floor_; if (wdBreakMask & 7) { if (y < miny) miny = y; } else if (W.car.y - floor_ < miny) miny = W.car.y - floor_; }
  int on = (wdBreakMask & 7) ? colGround(cw, W.homer.x, W.homer.z, W.homer.y + 1.f, W.homer.y - 3.f, &g, 0) : colGround(cw, W.car.x, W.car.z, W.car.y + 1.f, W.car.y - 3.f, &g, 0);
  printf("S\t%d\t%d\t%.1f\t%d\n", nline, nok, miny, on);
  return 0;
}
