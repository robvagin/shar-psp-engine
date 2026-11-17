// 107b 1285: машина на боку и вверх колёсами в хост-физике порта: через сколько секунд выполняются условия автосброса оригинала (подъём кузова < 0,2 и скорость < 1 м/с; либо колёс 0, подъём < 0, покой).
//   Печатает по роллу 90° (бок) и 180° (крыша): t, upy, колёс, скорость; итог: секунд до условия «на боку дольше 1,0 с».
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "car.h"
static ColWorld w;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); unsigned char hdr[104]; if (!f || fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&w, cm, end - colOff)) return 2; fclose(f);
  for (int deg = 90; deg <= 180; deg += 90) {
    Car c; carInit(&c, NULL, &w, 220.f, 3.5f, 172.f, 1.5708f); for (int i = 0; i < 90; i++) { CarInput in = {0, 0, 0, 1}; carUpdate(&c, &w, &in, 1 / 60.f); }
    float th = deg * 0.0174533f * .5f; c.q[0] = cosf(th); c.q[1] = 0.f; c.q[2] = 0.f; c.q[3] = sinf(th); c.y += 1.2f; c.vel[0] = c.vel[1] = c.vel[2] = 0.f;
    float sideT = 0, tHit = -1;
    for (int k = 0; k < 60 * 8; k++) {
      CarInput in = {0, 0, 0, 0}; carUpdate(&c, &w, &in, 1 / 60.f);
      float upy = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]), sp = sqrtf(c.vel[0] * c.vel[0] + c.vel[1] * c.vel[1] + c.vel[2] * c.vel[2]), an = sqrtf(c.w[0] * c.w[0] + c.w[1] * c.w[1] + c.w[2] * c.w[2]);
      if (sp < 1.f && upy < .2f) sideT += 1 / 60.f; else sideT = 0.f;
      int flipped = c.wheels == 0 && upy < 0.f && sp < .15f && an < .3f;
      if (tHit < 0 && (sideT > 1.f || flipped)) tHit = k / 60.f;
      if (k % 60 == 59) printf("  roll %d t=%d upy=%.2f wheels=%d sp=%.2f an=%.2f\n", deg, (k + 1) / 60, upy, c.wheels, sp, an);
    }
    printf("tipped %d trigger_s %.2f\n", deg, tHit);
  }
  return 0;
}
