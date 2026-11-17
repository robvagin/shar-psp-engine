// Правка 1265 (Стена 107): машина игрока проезжает путь Смитерса через клетку M7-1 (L1 m3 финал): где встаёт и о что (журнал стен/контактов).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
static const float P[][2] = {{410,52},{412,20},{415,-20},{416,-46},{437,-75},{432,-95},{424,-107},{392,-110},{359,-110}};
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], argc > 3 && argv[3][0] != '-' ? argv[3] : 0)) return 2;
  int sgn = getenv("SGN") ? atoi(getenv("SGN")) : 1; float v0 = getenv("V") ? (float)atof(getenv("V")) : 10.f;
  bwNew(&W); float y; colGround(bwCol(), P[0][0], P[0][1], 50.f, -50.f, &y, 0);
  bwPlace(&W, P[0][0], y + .5f, P[0][1], atan2f(P[1][0] - P[0][0], P[1][1] - P[0][1]));
  int k = 1, n = (int)(sizeof P / sizeof P[0]);
  for (int f = 0; f < 60 * 80 && k < n; f++) {
    float dx = P[k][0] - W.car.x, dz = P[k][1] - W.car.z, d = hypotf(dx, dz); if (d < 6.f) { k++; continue; }
    float err = atan2f(dx, dz) - W.car.h; while (err > 3.14159f) err -= 6.28318f; while (err < -3.14159f) err += 6.28318f;
    float v = hypotf(W.car.vel[0], W.car.vel[2]);
    unsigned b = 0; float st = fmaxf(-1.f, fminf(1.f, sgn * err * 1.5f));
    CarInput in = {v < v0 ? 1.f : 0.f, 0.f, st, 0}; (void)b;
    bwStep(&W, in.throttle > .5f ? GP_CROSS : 0, st, 0); (void)in;
    if (f % 60 == 0) printf("t=%d wp=%d pos %.1f %.1f %.1f h=%.2f v=%.1f contacts=%d hard=%.1f\n", f / 60, k, W.car.x, W.car.y, W.car.z, W.car.h, v, W.car.contacts, W.car.hardV);
  }
  printf("END wp=%d/%d pos %.1f %.1f\n", k, n, W.car.x, W.car.z); return 0; }
