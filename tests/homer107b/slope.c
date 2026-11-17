// Гейт В1 «склон 30° на рельефе» (окно Гомер 107b; сверка Fable-2): на реальных уровнях ищем участки рельефа (COL_TRI) 33–50° длиной подъёма ≥ 7 м и пускаем Гомера снизу вверх (стик вперёд 2,5 с).
//   Новое: заходит и поднимается (набор высоты ≥ 3 м). Старое (HOMERSLOPE107B_OLD=1): предел 30° на всём — стоп через 0,5 м, набор < 1 м.
//   cc ... -DSKIN_HOST [-DHOMERSLOPE107B_OLD=1] tests/homer107b/slope.c viewer/{homer,skin,col,car,props,stateprops,coins,watchdog}.c -lm  <homer.shr> <level.shr>...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static ColWorld world;
static int g(float x, float z, float *y, float n[3], int *type) { int mat; uint32_t ref; if (!colGroundMatId(&world, x, z, 400.f, -400.f, y, n, &mat, &ref)) return 0; *type = (int)(ref >> 29); return 1; }
static const float DT = 1.f / 60.f;
static HomerInput in0(float camYaw) { HomerInput i; memset(&i, 0, sizeof i); i.camYaw = camYaw; return i; }
typedef struct { float bx, bz, by, ux, uz, rise; } Cand;
static int scan(Cand *out, int max) {
  int found = 0; float cx = 0, cz = 0, R = 450;
  for (float x = cx - R; x < cx + R && found < max; x += 2.f) for (float z = cz - R; z < cz + R && found < max; z += 2.f) {
    float y, n[3]; int t; if (!g(x, z, &y, n, &t) || t != 0 || n[1] < .64f || n[1] > .85f) continue;
    float hl = hypotf(n[0], n[2]); if (hl < 1e-3f) continue; float ux = -n[0] / hl, uz = -n[2] / hl;
    float bx = x, bz = z, by = y; for (int s = 0; s < 40; s++) { float y2, n2[3]; int t2; float px = bx - ux * 1.f, pz = bz - uz * 1.f; if (!g(px, pz, &y2, n2, &t2) || t2 != 0 || n2[1] < .64f || n2[1] > .85f || y2 > by) break; bx = px; bz = pz; by = y2; }
    int ok = 1; float top = by, lastY = by; int len = 0;
    for (int s = 1; s <= 12; s++) { float y2, n2[3]; int t2; float px = bx + ux * s, pz = bz + uz * s; if (!g(px, pz, &y2, n2, &t2) || t2 != 0 || n2[1] < .64f || n2[1] > .85f || y2 < lastY + .2f) { ok = s >= 7; break; } lastY = y2; len = s; top = y2; }
    if (!ok || len < 7 || top - by < 4.f) continue;
    // перед склоном должна быть опора (ровное место 2 м ниже по склону не обязательно, но земля есть)
    float y0, n0[3]; int t0; if (!g(bx - ux * 2.f, bz - uz * 2.f, &y0, n0, &t0)) continue;
    out[found].bx = bx; out[found].bz = bz; out[found].by = by; out[found].ux = ux; out[found].uz = uz; out[found].rise = top - by; found++; x += 20.f;
  }
  return found;
}
int main(int argc, char **argv) {
  SkinModel *m = skinLoad(argv[1]); if (!m) return 2;
  static Homer H; if (homerInit(&H, m)) return 2;
  int tested = 0, climbed = 0, skipped = 0; float sumGain = 0;
  for (int a = 2; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) return 2;
    unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
    unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
    fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
    memset(&world, 0, sizeof world); unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) return 2; fclose(f);
    Cand cs[6]; int n = scan(cs, 6); H.solid = 0; H.solidUser = 0;
    for (int k = 0; k < n; k++) {
      float yaw = atan2f(cs[k].ux, cs[k].uz);
      float sx0 = cs[k].bx + cs[k].ux * 1.f, sz0 = cs[k].bz + cs[k].uz * 1.f, sy0, sn0[3]; int st0; if (!g(sx0, sz0, &sy0, sn0, &st0)) { skipped++; continue; }
      homerPlace(&H, &world, sx0, sy0, sz0, yaw); H.supY = H.y; H.groundY = H.y;   // уже на склоне, 1 м от низа
      if (H.spawnWait || fabsf(H.y - sy0) > .3f) { if (getenv("SLOPE_V")) printf("   [skip] spawnWait %d y %.2f want %.2f\n", H.spawnWait, H.y, sy0); skipped++; continue; }   // кандидат у края карты / ждёт мир: не свидетель
      HomerInput in = in0(yaw); in.my = 1.f; float y0 = H.y;
      for (int i = 0; i < 150; i++) { homerUpdate(&H, &world, NULL, &in, DT); homerPose(&H); }
      float gain = H.y - y0; if (getenv("SLOPE_V") && gain < 1.f) printf("   [dbg] pos %.2f %.2f %.2f start %.2f %.2f grounded %d wallHit %d voidStops %d spawnWait %d speed %.2f state %d\n", H.x, H.y, H.z, cs[k].bx, cs[k].bz, H.grounded, H.wallHit, H.voidStops, H.spawnWait, H.speed, H.state); tested++; sumGain += gain; if (gain >= 3.f) climbed++;
      printf("  %s: склон от (%.0f, %.0f) подъём %.1f м → набор %.2f м\n", argv[a] + strlen(argv[a]) - 12, cs[k].bx, cs[k].bz, cs[k].rise, gain);
    }
  }
  printf("slope_tested %d\nslope_skipped %d\nslope_climbed %d\nslope_mean_gain %.3f\n", tested, skipped, climbed, tested ? sumGain / tested : 0.f);
  return 0;
}
