// 1323 п.4: крен высоких машин в установившемся круге (полный руль с газом на 60 и 80 км/ч, ровная плита, 50 Гц, как запись PS2 s_ps2_c107_a5v_marge_*).
// PS2 (marge_v): крен 8,3° / 10,3° при боковом 24 / 28 м/с² (отношение 0,346 / 0,368 °/(м/с²)), четыре колеса с контактом всю дугу.
//   cc ... tests/cog1323/m.c viewer/car.c viewer/col.c; ./m машина [вход_км/ч ...]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } fd;
int main(int argc, char **argv) {
  memset(&fd, 0, sizeof fd); memcpy(fd.h.magic, "COL1", 4); fd.h.ox = fd.h.oz = -2000; fd.h.cell = 4000; fd.h.W = fd.h.H = 1; fd.h.ntri = fd.h.nref = 2;
  fd.t[0] = (ColTri){{-2000, 0, -2000}, {2000, 0, 2000}, {2000, 0, -2000}, {0, 1, 0}}; fd.t[1] = (ColTri){{-2000, 0, -2000}, {-2000, 0, 2000}, {2000, 0, 2000}, {0, 1, 0}};
  fd.start[1] = 2; fd.refs[0] = 0; fd.refs[1] = 1; ColWorld w; if (colInit(&w, &fd, sizeof fd)) return 2;
  const char *nm = argc > 1 ? argv[1] : "marge_v"; float dt = 1.f / 50.f;
  for (int a = 2; a < argc; a++) { float kmh = atof(argv[a]), tgt = kmh / 3.6f;
    CarParams p; carParamsFor(nm, &p); Car c; carInit(&c, &p, &w, 0, 0, 0, 0); c.huskDrive = 1;
    for (int f = 0; f < 50 * 25; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; carUpdate(&c, &w, &in, dt); }
    float R[400], V[400], L[400]; int n = 0, off = 0, flip = 0; float h0 = c.h, mx = 0;
    for (int f = 0; f < 50 * 6 && n < 400; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; in.steer = 1.f; float hb = c.h; carUpdate(&c, &w, &in, dt);
      float dh = c.h - hb; while (dh > 3.14159f) dh -= 6.28318f; while (dh < -3.14159f) dh += 6.28318f;
      R[n] = fabsf(c.roll) * 57.2958f; V[n] = c.v; L[n] = fabsf(c.v * dh / dt); if (R[n] > 80.f) flip = 1; if (f > 20 && R[n] > mx) mx = R[n]; if (f > 20 && c.wcon != 15) off++; n++; }
    #define MED(A) ({ float t_[400]; int m_ = n - 20; memcpy(t_, A + 20, m_ * sizeof(float)); for (int i = 0; i < m_; i++) for (int j = i + 1; j < m_; j++) if (t_[j] < t_[i]) { float x = t_[i]; t_[i] = t_[j]; t_[j] = x; } t_[m_ / 2]; })
    float r = MED(R), l = MED(L), v = MED(V);
    printf("cog1323 %s вход %.0f: в круге v %.1f км/ч, боковое %.1f м/с², крен мед %.2f° макс %.2f°, крен/боковое %.3f, кадров с оторванным колесом %d%%, переворот %s\n", nm, kmh, v * 3.6f, l, r, mx, r / (l > .1f ? l : .1f), off * 100 / (n - 20), flip ? "ДА" : "нет");
    (void)h0; }
  return 0; }
