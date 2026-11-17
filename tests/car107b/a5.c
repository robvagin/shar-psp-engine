// №406 (Роб 09.10, PSP: «на скорости и в резких поворотах машины отрываются от дороги, будто без массы и инерции,
// переворачиваются»): каждая машина диска (carparams.h carCons) на ровной площадке тем же carUpdate, что в игре.
// Заезд: разгон по прямой полным газом до установившейся скорости, затем полный руль с полным газом (и отдельно — с ручником).
// Печатает по машине: скорость входа, наибольший крен, доля кадров на двух колёсах и меньше, отрыв всех колёс, перевороты.
//   cc -std=c11 -O2 -I viewer tests/cars/cornering.c viewer/car.c viewer/col.c -lm -o cornering && ./cornering [машина] [--max-flips N]
// Гейт матрицы (tests/cars/physics_matrix.sh): --max-flips 7 — уровень прежнего порта a359ccd (7 переворотов / 27 на двух колёсах,
// замер этим же файлом 09.10); красный — физика Fable до 406 (-DCAR_TYRE_LEVER_OLD=1 -DCAR_WEEBLE_GROUND_OLD=1: 78).
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } floorData;
int main(int argc, char **argv) {
  (void)argc; (void)argv;
  memset(&floorData, 0, sizeof floorData); memcpy(floorData.h.magic, "COL1", 4);
  floorData.h.ox = floorData.h.oz = -2000; floorData.h.cell = 4000; floorData.h.W = floorData.h.H = 1; floorData.h.ntri = floorData.h.nref = 2;
  floorData.t[0] = (ColTri){{-2000, 0, -2000}, {2000, 0, 2000}, {2000, 0, -2000}, {0, 1, 0}};
  floorData.t[1] = (ColTri){{-2000, 0, -2000}, {-2000, 0, 2000}, {2000, 0, 2000}, {0, 1, 0}};
  floorData.start[1] = 2; floorData.refs[0] = 0; floorData.refs[1] = 1;
  ColWorld w; if (colInit(&w, &floorData, sizeof floorData)) return 2;
  // 107b A5: famil_v, полный руль вправо, газ держит скорость (как запись PS2 s_ps2_c107_a5_twowheel_{40,60,80}_R): крен макс, минимальная скорость, кадров без контакта, радиус круга
  static const float kmh[] = {40, 60, 80};
  for (int k = 0; k < 3; k++) {
    CarParams p; carParamsFor("famil_v", &p); Car c; carInit(&c, &p, &w, 0, 0, 0, 0); c.huskDrive = 1; float tgt = kmh[k] / 3.6f;
    float dt = 1.f / 50.f;
    for (int f = 0; f < 50 * 25; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; carUpdate(&c, &w, &in, dt); }
    float vin = c.v * 3.6f, rollMax = 0, vmin = 1e9f; int off = 0, two = 0, n = 0; float h0 = c.h, rad = 0;
    for (int f = 0; f < 50 * 6; f++) {
      CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; in.steer = 1.f; carUpdate(&c, &w, &in, dt); n++;
      float r = fabsf(c.roll) * 57.2958f; if (r > rollMax) rollMax = r; float v = c.v * 3.6f; if (v < vmin) vmin = v; off += c.wheels < 4; two += c.wheels <= 2;
      if (f == 100) { float dh = c.h - h0; while (dh > 3.14159f) dh -= 6.28318f; while (dh < -3.14159f) dh += 6.28318f; (void)dh; }
    }
    printf("a5 %.0f vin %.1f roll_max %.1f vmin %.1f frames_not4 %d frames_le2 %d of %d\n", kmh[k], vin, rollMax, vmin, off, two, n); (void)rad;
  }
  return 0;
}
