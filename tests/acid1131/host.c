// №1131 (g11): арифметика капель кислоты (viewer/acid1131.h): яркость и падающие капли. Столб 15 м (deathgoop) и труба 4 м.
#include <stdio.h>
#include "acid1131.h"
int main(void) {
  const float H[2] = {15.f, 4.f}; int active[2] = {0, 0}, samples = 0, bad = 0, nonmono = 0, drops_seen = 0; float lenMax = 0;
  for (int c = 0; c < 2; c++) {
    float T = 3.3f + H[c], B = 3.3f, prevY[ACID_DROPS]; int prevOn[ACID_DROPS] = {0};
    for (float t = 0.f; t < 10.f; t += .02f) {
      int n = 0;
      for (int j = 0; j < ACID_DROPS; j++) {
        float yh, len; int on = acidDropAt(t, j, c, T, B, &yh, &len);
        if (on) { n++; drops_seen++; if (yh > T + .01f || yh < B + .09f || len < .5f || len > 1.91f) bad++; if (len > lenMax) lenMax = len; if (prevOn[j] && yh > prevY[j] + 1e-4f) nonmono++; prevY[j] = yh; }
        prevOn[j] = on;
      }
      samples++; if (n) active[c]++;
    }
  }
  printf("top %.0f\nlow %.0f\npool %.0f\ndrops_seen %d\nactive_big_pct %d\nactive_pipe_pct %d\nbad %d\nnonmono %d\n", acidAlphaTop(ACID1131_OLD), acidAlphaLow(ACID1131_OLD), acidAlphaPool(ACID1131_OLD), drops_seen, 100 * active[0] / (samples / 2), 100 * active[1] / (samples / 2), bad, nonmono);
  return 0;
}
