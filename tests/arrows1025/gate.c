// №1025 (Роб 26.10: «зелёные стрелки на полу следуют подъёму и спаду дороги»): на настоящих данных уровня (коллизия COL1 + дороги трафика) набор стрелок пути — три квада 4,1 × 4,1 м через 4 м вдоль курса —
// кладётся на коллизию. Для каждой точки маршрута игры (trafficRouteArrow, как tests/roadarrow) считаются высоты четырёх углов каждого квада над землёй под этим углом:
//   прежнее — весь набор плоско на самую высокую точку коллизии под ним + 0,06 м (roadarrow.h);  новое — квад через точку земли под центром с нормалью (те же формулы, что msnart.c: проекция осей на плоскость, +0,05 м);
//   плоско-красное (--flat) — центр на земле, без наклона (красный контроль: на склоне углы тонут/висят).
// Углы «утонули» при зазоре < -0,03 м, «висят» при > 0,25 м. Печатает долю и наибольшие значения; RESULT PASS: новое — утонувших 0 % и висящих ≤ 2 %, и прежнее хуже по обоим.
//   gate LEVEL.shr HOMER.shr [--flat]       (--flat: красный — PASS означает «красный пойман»: плоско-красное хуже порога)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
#include "roadarrow.h"
static int gAt(float x, float z, float yHi, float yLo, float *out) { float n[3]; return colGround(bwCol(), x, z, yHi, yLo, out, n); }
typedef struct { unsigned n, sunk, hang; float lo, hi; } St;
static void acc(St *s, float gap) { s->n++; if (gap < -.03f) s->sunk++; if (gap > .25f) s->hang++; if (gap < s->lo) s->lo = gap; if (gap > s->hi) s->hi = gap; }
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage\n"); return 2; }
  int flat = argc > 3 && !strcmp(argv[3], "--flat");
  static BotWorld w; if (bwNew(&w)) return 2;
  const Traffic *t = (const Traffic *)bwTraffic(); if (!t) { printf("нет трафика\nRESULT: FAIL\n"); return 1; }
  const float tg[6][2] = {{220, 172}, {-150, -300}, {300, -100}, {-300, 200}, {0, 0}, {100, 350}};
  St so = {0, 0, 0, 1e9f, -1e9f}, sn = {0, 0, 0, 1e9f, -1e9f}; unsigned npts = 0, steep = 0;
  for (float px = -600; px <= 600; px += 20) for (float pz = -600; pz <= 600; pz += 20) for (float ph = 0; ph < 6.2f; ph += 1.57f) for (int g = 0; g < 6; g++) {
    float p[3], h; if (!trafficRouteArrow(t, px, pz, ph, tg[g][0], tg[g][1], 70.f, p, &h)) continue;
    float top = raTop(gAt, p, h); if (top < -1e8f) continue; npts++;
    float yOld = raLiftedY(gAt, p, h), sh = sinf(h), ch = cosf(h), rx = ch, rz = -sh;   // курс (sh, ch), вправо (rx, rz)
    int hill = 0;
    for (int a = -1; a <= 1; a++) {
      float cx = p[0] + sh * 4.f * a, cz = p[2] + ch * 4.f * a, gy, n3[3];
      if (!colGround(bwCol(), cx, cz, p[1] + 4.f, p[1] - 8.f, &gy, n3) || n3[1] < .5f) continue;
      float nl = sqrtf(n3[0] * n3[0] + n3[1] * n3[1] + n3[2] * n3[2]); n3[0] /= nl; n3[1] /= nl; n3[2] /= nl; if (n3[1] < .985f) hill = 1;
      float X[3] = {rx * 2.05f, 0, rz * 2.05f}, Y[3] = {sh * 2.05f, 0, ch * 2.05f};
      if (!flat) { float *AX[2] = {X, Y};
        for (int e = 0; e < 2; e++) { float *V = AX[e], l0 = sqrtf(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]), dn = V[0] * n3[0] + V[1] * n3[1] + V[2] * n3[2];
          for (int i = 0; i < 3; i++) V[i] -= dn * n3[i]; float l1 = sqrtf(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]); for (int i = 0; i < 3; i++) V[i] *= l0 / l1; } }
      float lift = gy + .05f;   // как msnart.c: поднять на недостачу самого низкого угла
      for (int c = 0; c < 4; c++) { float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f, cg, qx = cx + sx * X[0] + sy * Y[0], qz = cz + sx * X[2] + sy * Y[2];
        if (!flat && gAt(qx, qz, gy + 3.f, gy - 3.f, &cg)) { float need = cg + .03f - (gy + sx * X[1] + sy * Y[1]); if (need > lift - gy) lift = gy + need; } }
      for (int c = 0; c < 4; c++) { float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f;
        float qx = cx + sx * X[0] + sy * Y[0], qz = cz + sx * X[2] + sy * Y[2], qyN = lift + sx * X[1] + sy * Y[1], qyO = yOld, cg;
        if (!gAt(qx, qz, gy + 3.f, gy - 3.f, &cg)) continue;
        acc(&so, qyO - cg); acc(&sn, qyN - cg); } }
    steep += hill;
  }
  printf("точек маршрута %d (из них со склоном > 10 %%: %u): углов %u\n", npts, steep, sn.n);
  printf("  прежнее: утонули %.2f %%, висят %.2f %%, зазор %.2f…%.2f м\n", so.n ? 100.0 * so.sunk / so.n : 0.0, so.n ? 100.0 * so.hang / so.n : 0.0, so.lo, so.hi);
  printf("  %s: утонули %.2f %%, висят %.2f %%, зазор %.2f…%.2f м\n", flat ? "плоско-красное" : "новое", sn.n ? 100.0 * sn.sunk / sn.n : 0.0, sn.n ? 100.0 * sn.hang / sn.n : 0.0, sn.lo, sn.hi);
  double sunkN = sn.n ? 100.0 * sn.sunk / sn.n : 100, hangN = sn.n ? 100.0 * sn.hang / sn.n : 100, sunkO = so.n ? 100.0 * so.sunk / so.n : 0, hangO = so.n ? 100.0 * so.hang / so.n : 0;
  int good = npts >= 5 && sunkN == 0.0 && hangN <= 2.0 && (sunkO + hangO) > (sunkN + hangN);
  printf("RESULT: %s\n", flat ? (good ? "FAIL (красный не пойман)" : "PASS (красный пойман)") : (good ? "PASS" : "FAIL"));
  return flat ? good : !good;
}
