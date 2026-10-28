// №705 ч.2: прыжок «гроб» L1 C7-6 (jumpzone10/11, запись PS2 v22_c76: 100 км/ч на отрыве, подъём 4,69 м, в воздухе 1,92 с, дальность 40,8 м, точка отрыва PS2 (-406,24; 28,17), у порта z зеркален)
// порт: машина с газом до 27,8 м/с едет по -z с x -406,2 от z = start; печатаем точку и скорость отрыва, вершину над точкой отрыва, дальность и время полёта. Аргументы: level.shr [start_z] [lat] [old]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2;
  float z0 = argc > 2 ? atof(argv[2]) : 60.f, lat = argc > 3 ? atof(argv[3]) : 0.f; for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "old")) carJumpZoneGateOld = 1;
  float x0 = -406.2f + lat, g;
  if (!colGround(&w, x0, z0, 30.f, -10.f, &g, 0)) { puts("нет земли в старте"); return 1; }
  Car c; carInit(&c, NULL, &w, x0, g, z0, 3.14159265f); for (int i = 0; i < 20; i++) carUpdate(&c, &w, &(CarInput){0}, 1.f / 60);
  carSetSpeed(&c, 8.f); int took = 0, landed = 0; float tx = 0, tz = 0, ty = 0, tv = 0, apex = -1e9, lx = 0, lz = 0, tt = 0, lt_ = 0; unsigned boosts0 = 0;
  for (int fr = 0; fr < 900; fr++) {
    CarInput in = {0}; in.throttle = c.v < 27.8f ? 1.f : 0.f; carUpdate(&c, &w, &in, 1.f / 60);
    if (!took && !c.grounded && c.z < 40.f && c.vel[1] > 1.f) { took = 1; tx = c.x; tz = c.z; ty = c.y; tv = hypotf(c.vel[0], c.vel[2]); tt = fr / 60.f; }
    if (took && !landed) { if (c.y > apex) apex = c.y; if (c.grounded && fr / 60.f > tt + 0.3f) { landed = 1; lx = c.x; lz = c.z; lt_ = fr / 60.f; break; } }
    if (c.z < -150.f) break; (void)boosts0; }
  printf("отрыв: %s x %.1f z %.1f, v %.1f м/с (%.0f км/ч); вершина над точкой отрыва %.2f м; в воздухе %.2f с; дальность %.1f м; подшагов разгона %d  (PS2: 100 км/ч, 4,69 м, 1,92 с, 40,8 м)\n",
         took ? "да" : "НЕТ", tx, tz, tv, tv * 3.6f, took ? apex - ty : 0.f, landed ? lt_ - tt : 0.f, landed ? hypotf(lx - tx, lz - tz) : 0.f, c.boosts);
  return 0; }
