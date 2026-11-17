// 108: уступ вне трамплина: famil_v въезжает под 0° на уступ 0,25 м на 1 м (вверх 30 км/ч) / съезжает (вниз 20 км/ч, разбег) против записей PS2 окна PS2 (s_ps2_c107_a3_curb_up_30, curb_down_20, flat.tsv).
// PS2 вверх 30: тангаж +5,4° (после — −3,3°), vy до +1,16, колёса передние в упоре 0,2662, колёса в контакте ниже нейтрали до −0,041, кадры без контакта всех колёс есть.
// Печатает: тангаж макс/мин, vy макс/мин, подъём кузова, минимальный ход колеса в контакте (xs), доля колесо-кадров в контакте с xs<0, кадров без контакта у всех колёс.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "car.h"
static struct { ColHdr h; ColTri t[6]; unsigned start[2], refs[6]; } fl;
static ColTri tri(float ax, float ay, float bx, float by, float cx, float cy, float dx, int up) { (void)up; (void)dx; (void)cy; (void)cx; (void)by; (void)bx; (void)ay; (void)ax; ColTri t; memset(&t, 0, sizeof t); return t; }
int main(int argc, char **argv) {
  (void)tri; int drop = argc > 1 && !strcmp(argv[1], "drop"); int down = drop || (argc > 1 && !strcmp(argv[1], "down")); float kmh = drop ? 40.f : down ? 20.f : 30.f, H = drop ? .8f : .25f, L = drop ? .2f : 1.f;
  memset(&fl, 0, sizeof fl); memcpy(fl.h.magic, "COL1", 4); fl.h.ox = -200; fl.h.oz = -20; fl.h.cell = 400; fl.h.W = fl.h.H = 1; fl.h.ntri = 6; fl.h.nref = 6;   // клетка 400 м на 400 м: z от -20 вне клетки не нужен
  fl.h.oz = -200;
  float X0 = down ? -H : 0.f;   // ступень: у «вверх» нижний ярус слева, у «вниз» верхний слева
  float yl = down ? H : 0.f, yr = down ? 0.f : H, zz = 200.f;
  float nx = (yl - yr) / L, nrm = sqrtf(nx * nx + 1.f); (void)X0;
  // нижний/верхний ярус слева x<0, скат 0..L, ярус справа x>L
  fl.t[0] = (ColTri){{-200, yl, -zz}, {-200, yl, zz}, {0, yl, zz}, {0, 1, 0}}; fl.t[1] = (ColTri){{-200, yl, -zz}, {0, yl, zz}, {0, yl, -zz}, {0, 1, 0}};
  fl.t[2] = (ColTri){{0, yl, -zz}, {0, yl, zz}, {L, yr, zz}, {nx / nrm, 1.f / nrm, 0}}; fl.t[3] = (ColTri){{0, yl, -zz}, {L, yr, zz}, {L, yr, -zz}, {nx / nrm, 1.f / nrm, 0}};
  fl.t[4] = (ColTri){{L, yr, -zz}, {L, yr, zz}, {200, yr, zz}, {0, 1, 0}}; fl.t[5] = (ColTri){{L, yr, -zz}, {200, yr, zz}, {200, yr, -zz}, {0, 1, 0}};
  fl.start[1] = 6; for (int i = 0; i < 6; i++) fl.refs[i] = i;
  ColWorld w; if (colInit(&w, &fl, sizeof fl)) { puts("мир"); return 2; }
  CarParams p; carParamsFor("famil_v", &p); Car c; float x0 = -40.f;
  carInit(&c, &p, &w, x0, down ? H : 0.f, 0.f, 1.5707963f); c.huskDrive = 1; float tgt = kmh / 3.6f, dt = 1.f / 60.f;
  float errMax = 0, errAir = 0; int errN = 0, nW = 0; float pmax = -99, pmin = 99, vmax = -99, vmin = 99, y0 = 0, ymax = -99, xmin = 99, nneg = 0, nall = 0, nair = 0, xmax = -99;
  for (int f = 0; f < 60 * 12; f++) {
    CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < tgt ? 1.f : 0.f; carUpdate(&c, &w, &in, dt);
    if (f == 60 * 3) y0 = c.y;
    if (c.x < -6.f || c.x > 8.f || f < 60 * 3) continue;
    float pit = c.pitch * 57.2958f; if (pit > pmax) pmax = pit; if (pit < pmin) pmin = pit; float vy = c.vel[1]; if (vy > vmax) vmax = vy; if (vy < vmin) vmin = vy; if (c.y > ymax) ymax = c.y;
    for (int i = 0; i < 4; i++) { float phys = c.xs[i] - c.eq, dr = carWheelDy(&c, i, 9.f), e = fabsf(dr - phys); if (c.xs[i] > -c.lim + 1e-4f) { nW++; if (e > errMax) errMax = e; if (e > .02f) errN++; } else if (e > errAir) errAir = e; }   // 108: рисунок колеса против фактического хода (низ колеса = точка контакта)
    int ng = 0; for (int i = 0; i < 4; i++) { if (c.xs[i] > -c.lim + 1e-3f) { ng++; nall++; if (c.xs[i] < xmin) xmin = c.xs[i]; if (c.xs[i] < 0) nneg++; if (c.xs[i] > xmax) xmax = c.xs[i]; } } if (!ng) nair++;
  }
  printf("curb %s %.0f км/ч: тангаж %.2f..%.2f vy %.2f..%.2f y-подъём %.3f xs_min_контакт %.3f xs_max %.3f колёс-кадров с xs<0 %.0f из %.0f, кадров без контакта %.0f\n", drop ? "обрыв" : down ? "вниз" : "вверх", kmh, pmin, pmax, vmin, vmax, ymax - y0, xmin, xmax, nneg, nall, nair);
  printf("draw %s: ошибка рисунка колеса против хода подвески в контакте: макс %.3f м, колёс-кадров > 2 см: %d из %d; в воздухе макс %.3f м\n", drop ? "обрыв" : down ? "вниз" : "вверх", errMax, errN, nW, errAir);
  return 0;
}
