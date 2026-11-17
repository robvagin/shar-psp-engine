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
  // 109 А5б: famil_v, полный руль вправо с ПОЛНЫМ газом (как PS2: газ держится), ровная плита; окна по 8 кадров (50 Гц) после 0,4 с поворота — те же средние, что у записи PS2:
  // скорость, угловая скорость, боковое ускорение v*w, крен; отношение крен/боковое ускорение — главное число (PS2 по медианам: 40: v47 yaw107 lat24,4 roll3,37 = 0,138; 60: v70 yaw88 lat30,0 roll4,63 = 0,154; 80: v62 yaw74 lat22,1 roll3,2 = 0,145).
  static const float kmh[] = {47, 70, 62};   // скорости в круге у PS2 (медианы записей a5 40/60/80), газ держит их
  for (int k = 0; k < 3; k++) {
    CarParams p; carParamsFor("famil_v", &p); Car c; carInit(&c, &p, &w, 0, 0, 0, 0); c.huskDrive = 1; float tgt = kmh[k] / 3.6f;
    float dt = 1.f / 50.f;
    for (int f = 0; f < 50 * 25; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; carUpdate(&c, &w, &in, dt); }
    if (getenv("ONSET")) { Car d = c; float h0 = d.h; printf("onset %.0f:", kmh[k]); for (int f = 0; f < 60; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = 1.f; in.steer = 1.f; carUpdate(&d, &w, &in, dt); if (f % 6 == 5) { float dh = d.h - h0; while (dh > 3.14159f) dh -= 6.28318f; while (dh < -3.14159f) dh += 6.28318f; printf(" [%d v%.1f h%.0f r%.1f]", f + 1, d.v, dh * 57.2958f, d.roll * 57.2958f); } } printf("\n"); }
    float xs_[400], zs_[400], hs_[400], rs_[400]; int n = 0;
    for (int f = 0; f < 50 * 6 && n < 400; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; in.steer = 1.f; carUpdate(&c, &w, &in, dt); xs_[n] = c.x; zs_[n] = c.z; hs_[n] = c.h; rs_[n] = c.roll * 57.2958f; n++; }
    float vv[40], ww[40], rr[40], aa[40]; int m = 0;
    for (int i = 20; i + 8 < n && m < 40; i += 8) {
      float dh = hs_[i + 8] - hs_[i]; while (dh > 3.14159f) dh -= 6.28318f; while (dh < -3.14159f) dh += 6.28318f;
      float v = hypotf(xs_[i + 8] - xs_[i], zs_[i + 8] - zs_[i]) / (8 * dt), om = fabsf(dh) / (8 * dt);
      vv[m] = v; ww[m] = om; aa[m] = v * om; rr[m] = fabsf(rs_[i]); m++;
    }
    #define MED(a) ({ float t_[40]; memcpy(t_, a, m * sizeof(float)); for (int i_ = 0; i_ < m; i_++) for (int j_ = i_ + 1; j_ < m; j_++) if (t_[j_] < t_[i_]) { float x_ = t_[i_]; t_[i_] = t_[j_]; t_[j_] = x_; } t_[m / 2]; })
    float mv = MED(vv), mw = MED(ww), ma = MED(aa), mr = MED(rr);
    printf("a5b %.0f v %.1f yaw %.1f latacc %.2f roll %.2f ratio %.3f\n", kmh[k], mv * 3.6f, mw * 57.2958f, ma, mr, mr / ma);
  }
  return 0;
}
