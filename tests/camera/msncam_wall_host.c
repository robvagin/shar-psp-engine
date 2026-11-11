// Тур Роба на 102, Р3: наезды старта миссий с диска (msncam.shr) против коллизии уровня, по кадрам. Движковый код: viewer/msncam.c (поза), viewer/col.c (луч), viewer/msncamwall.h (сдвиг от стены).
// Игрок = точка взгляда последнего кадра (камера кончается, глядя на него). Для каждой камеры: с какого момента играть при длине FAR м (0 = с первого кадра, как у оригинала),
// наибольшее расстояние глаза до игрока по горизонтали, кадры с глазом за стеной (луч голова->глаз перекрыт) ДО сдвига и ПОСЛЕ (MODE 1 — с msncamwall.h). Выход 1, если после сдвига остался кадр за стеной.
//   cc -std=c11 -O1 -I viewer tests/camera/msncam_wall_host.c viewer/msncam.c viewer/col.c -lm -o /tmp/mcw && /tmp/mcw level.shr msncam.shr FAR MODE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msncam.h"
#include "msncamwall.h"
static unsigned char *rd(const char *p, unsigned off, long *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long sz = ftell(f); if (!off) { *n = sz; } fseek(f, off, SEEK_SET); unsigned char *m = aligned_alloc(64, (sz + 63 + 64) & ~63L); long r = fread(m, 1, sz - off, f); (void)r; fclose(f); return m; }
static ColWorld W;
int main(int argc, char **argv) {
  if (argc < 5) { puts("usage"); return 2; }
  float FAR = (float)atof(argv[3]); int mode = atoi(argv[4]);
  FILE *f = fopen(argv[1], "rb"); unsigned char h[104]; if (!f || fread(h, 1, 104, f) != 104 || memcmp(h, "SHR7", 4)) { puts("нет уровня"); return 2; }
  unsigned pv = *(unsigned *)(h + 88), co = *(unsigned *)(h + 92), po = *(unsigned *)(h + 96); fseek(f, 0, SEEK_END); unsigned fs = (unsigned)ftell(f), end = fs;
  if (pv > co && pv < end) end = pv; if (po > co && po < end) end = po;
  unsigned char *cm = aligned_alloc(64, (end - co + 127) & ~63u); fseek(f, co, SEEK_SET); if (fread(cm, 1, end - co, f) != end - co) return 2; fclose(f);
  if (colInit(&W, cm, end - co)) { puts("COL1 не читается"); return 2; }
  long n = 0; unsigned char *pk = rd(argv[2], 0, &n); if (!pk) { puts("нет msncam.shr"); return 2; }
  static MsnCam cams[24]; int k = msncamParse(cams, 24, pk, (unsigned)n); if (k <= 0) { puts("BAD разбор"); return 2; }
  int totBefore = 0, totAfter = 0, bad = 0;
  for (int i = 0; i < k; i++) {
    const MsnCam *c = &cams[i]; if (c->nfr < 2 || c->fps < 1.f) continue;
    float dur = (c->nfr - 1) / c->fps, e[3], l[3], u[3], fy, P[3];
    msncamPose(c, dur, e, P, u, &fy);   // игрок = взгляд последнего кадра
    float t0 = 0.f;
    if (FAR > 0.f) for (float t = 0.f; t < dur; t += 1.f / c->fps) { msncamPose(c, t, e, l, u, &fy); float dx = e[0] - P[0], dz = e[2] - P[2]; if (dx * dx + dz * dz <= FAR * FAR) { t0 = t; break; } }
    int fr = 0, before = 0, after = 0; float mx = 0.f, st = -1.f, mx3 = 0.f;
    for (float t = t0; t <= dur + 1e-4f; t += 1.f / c->fps) {
      msncamPose(c, t, e, l, u, &fy); fr++;
      float hd[3] = {P[0], P[1], P[2]};
      if (colRayFrac(&W, hd, e, 0) < .999f) before++;
      if (mode) mcWallPull(&W, hd, e, &st, 1.f / c->fps);
      if (colRayFrac(&W, hd, e, 0) < .999f) after++;
      { float dx = e[0] - P[0], dz = e[2] - P[2], dy = e[1] - P[1]; float d = sqrtf(dx * dx + dz * dz); if (d > mx) mx = d; d = sqrtf(dx * dx + dz * dz + dy * dy); if (d > mx3) mx3 = d; }
    }
    printf("CAM %-12s %-8s dur %5.2f с, играть с %5.2f с, кадров %3d, глаз от игрока до %5.1f м (гориз), %5.1f м (объём); кадров за стеной: до сдвига %3d, после %3d\n", c->name, c->phase, dur, t0, fr, mx, mx3, before, after);
    totBefore += before; totAfter += after; if (after) bad++;
  }
  printf("ИТОГ камер %d, кадров за стеной до сдвига %d, после %d, камер с остатком %d\n", k, totBefore, totAfter, bad);
  return bad ? 1 : 0;
}
