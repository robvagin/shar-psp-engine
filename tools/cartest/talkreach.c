// «Как близко Гомер может подойти к персонажу в комнате» (правка 1191б, L2 m2 talkto Moe): комнаты interiors-lN.bin тем же кодом, что EBOOT (rooms.c, col.c, homer.c); Гомер из кольца точек вокруг цели идёт к ней
// на полном стике ходьбой, считается наименьшее трёхмерное расстояние (как msn_run.c dist3(игрок, сущность) < радиус). Печать: комната, цель, радиус кольца, лучшее расстояние и откуда.
//   talkreach interiors.bin homer.shr X Y Z [ROOMNAME]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "skin.h"
#include "rooms.h"
unsigned homerSolidFrame = 0;
static unsigned char *readAll(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f); unsigned char *b = aligned_alloc(16, ((size_t)s + 15) & ~15u); if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); return 0; } fclose(f); *n = (size_t)s; return b; }
int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "talkreach interiors.bin homer.shr X Y Z [ROOMNAME]\n"); return 2; }
  size_t n; unsigned char *b = readAll(argv[1], &n); if (!b) return 2; static Rooms R; int e = roomsParse(&R, b, n); if (e) { fprintf(stderr, "rooms: %s\n", roomsError(e)); return 2; }
  SkinModel *m = skinLoad(argv[2]); if (!m) { fprintf(stderr, "homer.shr не читается\n"); return 2; }
  float tx = atof(argv[3]), ty = atof(argv[4]), tz = atof(argv[5]); const char *want = argc > 6 ? argv[6] : 0;
  const float SP = 0.5f; const int NG = 56;   // сетка 0,5 м вокруг цели ±14 м; соседние клетки проходимы, если Гомер за 0,5 с ходьбой доходит из одной в другую
  for (int k = 0; k < R.nroom; k++) { Room *rm = &R.room[k]; if (want && !strstr(rm->name, want)) continue; float g, nn[3];
    if (!colGround(&rm->col, tx, tz, ty + 3.f, ty - 4.f, &g, nn)) { if (want) printf("комната %s: под целью (%.1f %.1f %.1f) нет пола\n", rm->name, tx, ty, tz); continue; }
    static signed char vis[56][56]; static float gyc[56][56]; memset(vis, 0, sizeof vis);
    float ox = tx - NG * SP / 2, oz = tz - NG * SP / 2; int sx0 = (int)floorf((rm->start[0] - ox) / SP), sz0 = (int)floorf((rm->start[2] - oz) / SP);
    if (sx0 < 0 || sz0 < 0 || sx0 >= NG || sz0 >= NG) { printf("комната %s: старт %.1f %.1f вне окна вокруг цели\n", rm->name, rm->start[0], rm->start[2]); continue; }
    static int qx[56 * 56], qz[56 * 56]; int qh = 0, qt = 0; qx[qt] = sx0; qz[qt++] = sz0; vis[sz0][sx0] = 1; gyc[sz0][sx0] = rm->start[1];
    float best = 1e9f, bestH = 1e9f, bx = 0, bz = 0; int nreach = 0;
    while (qh < qt) { int cx = qx[qh], cz = qz[qh++]; float px = ox + (cx + .5f) * SP, pz = oz + (cz + .5f) * SP, py = gyc[cz][cx]; nreach++;
      float d3 = sqrtf((px - tx) * (px - tx) + (py - ty) * (py - ty) + (pz - tz) * (pz - tz)); if (d3 < best) { best = d3; bestH = hypotf(px - tx, pz - tz); bx = px; bz = pz; }
      static const int DX[4] = {1, -1, 0, 0}, DZ[4] = {0, 0, 1, -1};
      for (int d = 0; d < 4; d++) { int nx = cx + DX[d], nz = cz + DZ[d]; if (nx < 0 || nz < 0 || nx >= NG || nz >= NG || vis[nz][nx]) continue;
        float qx_ = ox + (nx + .5f) * SP, qz_ = oz + (nz + .5f) * SP, gy2; if (!colGround(&rm->col, qx_, qz_, py + 1.2f, py - 1.2f, &gy2, nn)) continue;
        Homer H; memset(&H, 0, sizeof H); if (homerInit(&H, m)) continue; float hd = atan2f(qx_ - px, qz_ - pz); homerPlace(&H, &rm->col, px, py, pz, hd);
        HomerInput in; memset(&in, 0, sizeof in); in.my = 1.f; in.camYaw = hd; int ok = 0;
        for (int f = 0; f < 45; f++) { homerUpdate(&H, &rm->col, NULL, &in, 1.f / 60.f); if (hypotf(H.x - qx_, H.z - qz_) < 0.12f && fabsf(H.y - gy2) < 0.5f) { ok = 1; break; } }
        if (ok) { vis[nz][nx] = 1; gyc[nz][nx] = gy2; qx[qt] = nx; qz[qt++] = nz; } } }
    printf("комната %s (%d): цель %.1f %.1f %.1f; достижимо клеток по 0,5 м: %d; ближе всего 3D %.2f м (по горизонтали %.2f), в точке %.2f %.2f\n", rm->name, k, tx, ty, tz, nreach, best, bestH, bx, bz); }
  return 0;
}
