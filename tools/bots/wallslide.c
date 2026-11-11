// Коллизия-96: клин у стены. wallslide LEVEL PLAYER x z hdeg kmh steer gas frames — машина с заданного места курсом hdeg (0 = +z, 90 = +x), газ и руль держатся;
// итог: макс. кадров подряд почти стоя (< 5 км/ч после 30-го кадра). Нужен для гейта tests/cars/wallstick_gate.sh (скользящий удар не должен клинить нос в стену)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 10 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: wallslide LEVEL PLAYER x z hdeg kmh steer gas frames\n"); return 2; }
  float x = atof(argv[3]), z = atof(argv[4]), hd = atof(argv[5]) * 0.0174533f, kmh = atof(argv[6]), st = atof(argv[7]); int gas = atoi(argv[8]), N = atoi(argv[9]); float gy;
  if (!colGroundMat(bwCol(), x, z, (getenv("WS_Y") ? (float)atof(getenv("WS_Y")) : 0.f) + 40.f, (getenv("WS_Y") ? (float)atof(getenv("WS_Y")) : 0.f) - 50.f, &gy, 0, 0)) { puts("нет земли"); return 2; }
  memset(&W, 0, sizeof W); bwNew(&W); bwPlace(&W, x, gy + .3f, z, hd); for (int f = 0; f < 20; f++) bwStep(&W, 0, 0.f, 0.f);
  float v0 = kmh / 3.6f; W.car.vel[0] = sinf(hd) * v0; W.car.vel[2] = cosf(hd) * v0; W.car.v = v0;
  int low = 0, lowMax = 0; float rmax = 0; float hold = getenv("WS_HOLD") ? (float)atof(getenv("WS_HOLD")) : 0.f, h0 = hd;
  for (int f = 0; f < N; f++) { { unsigned bt = gas ? GP_R : 0; float sv = st; if (hold > 0.f) { float v_ = hypotf(W.car.vel[0], W.car.vel[2]) * 3.6f; bt = v_ < hold ? GP_R : 0; float e = h0 - W.car.h; while (e > 3.14159f) e -= 6.28318f; while (e < -3.14159f) e += 6.28318f; sv = fmaxf(-1.f, fminf(1.f, -e * 2.2f)); } bwStep(&W, bt, sv, 0.f); if (f > 30 && fabsf(W.car.roll) * 57.2958f > rmax) rmax = fabsf(W.car.roll) * 57.2958f; } float v = hypotf(W.car.vel[0], W.car.vel[2]) * 3.6f; low = (v < 5.f && f > 30) ? low + 1 : 0; if (low > lowMax) lowMax = low; }
  printf("макс. подряд почти стоя (<5 км/ч) %d кадров, макс. крен %.0f°\n", lowMax, rmax); return 0; }
