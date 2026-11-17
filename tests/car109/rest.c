// 109 п.3: стоянка famil_v на ровном: сжатие подвески по колёсам (c->xs), жёсткости, центр масс — против записи PS2 покоя (off2 перед 0,1211 / зад 0,1445)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } fl;
int main(int argc, char **argv) {
  memset(&fl, 0, sizeof fl); memcpy(fl.h.magic, "COL1", 4); fl.h.ox = fl.h.oz = -2000; fl.h.cell = 4000; fl.h.W = fl.h.H = 1; fl.h.ntri = fl.h.nref = 2;
  fl.t[0] = (ColTri){{-2000, 0, -2000}, {2000, 0, 2000}, {2000, 0, -2000}, {0, 1, 0}}; fl.t[1] = (ColTri){{-2000, 0, -2000}, {-2000, 0, 2000}, {2000, 0, 2000}, {0, 1, 0}};
  fl.start[1] = 2; fl.refs[0] = 0; fl.refs[1] = 1; ColWorld w; if (colInit(&w, &fl, sizeof fl)) return 2;
  CarParams p; carParamsFor(argc > 1 ? argv[1] : "famil_v", &p); Car c; carInit(&c, &p, &w, 0, 0, 0, 0);
  for (int f = 0; f < 600; f++) { CarInput in; memset(&in, 0, sizeof in); in.brake = 0; carUpdate(&c, &w, &in, 1.f / 60.f); }
  printf("rest lim %.3f eq %.3f xs %.4f %.4f %.4f %.4f kS %.0f %.0f %.0f %.0f com %.3f %.3f %.3f wheelF %.3f wheelR %.3f mass %.0f pitch %.3f\n", c.lim, c.eq, c.xs[0], c.xs[1], c.xs[2], c.xs[3], c.kS[0], c.kS[1], c.kS[2], c.kS[3], c.com[0], c.com[1], c.com[2], p.wheelF, p.wheelR, p.mass, c.pitch * 57.2958f);
  return 0;
}
