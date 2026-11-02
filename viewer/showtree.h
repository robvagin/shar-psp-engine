// №1119, код Show Tree (оригинал: отладочная подсветка красным «дерева» разбиения мира — коробки листьев — и треугольника коллизии под игроком, поверх всего, без глубины).
// У порта своё разбиение мира — клетки города (Cell.bb, main_state.h) и куски (Piece.r.bb); показываем их аналогом: красные коробки ближайших к игроку клеток и красный треугольник опоры под игроком
// (три точки земли вокруг него в 1,2 м), векторно, одним вызовом списка GE, только при включённом коде. Проекция считается на процессоре (без матриц GE): ≤ 12 коробок × 12 рёбер + 3 ребра.
// Число клеток (12) и радиус треугольника (1,2 м) — наше. SHOWTREE1119_OLD=1 — подсветки нет
#ifndef SHOWTREE1119_OLD
#define SHOWTREE1119_OLD 0
#endif
typedef struct { unsigned c; float x, y, z; } StV;
static int stProject(const float e[3], const float f[3], const float r[3], const float u[3], float tanH, float tanV, const float p[3], float o[3]) { return ccProject(e, f, r, u, tanH, tanV, (float)SCR_W, (float)SCR_H, p, o); }
static void stEdge(StV *v, int *n, int cap, const float e[3], const float f[3], const float r[3], const float u[3], float tanH, float tanV, const float a[3], const float b[3]) {
  float pa[3], pb[3]; if (*n + 2 > cap) return;
  if (!stProject(e, f, r, u, tanH, tanV, a, pa) || !stProject(e, f, r, u, tanH, tanV, b, pb)) return;   // ребро за камерой не рисуем (без среза по ближней — упрощение)
  v[*n].c = 0xff0000ffu; v[*n].x = pa[0]; v[*n].y = pa[1]; v[*n].z = 0.f; (*n)++;
  v[*n].c = 0xff0000ffu; v[*n].x = pb[0]; v[*n].y = pb[1]; v[*n].z = 0.f; (*n)++;
}
// e — глаз, l — точка взгляда, fovYdeg — вертикальный угол, p — игрок, tri — три точки земли под ним (или NULL)
static int showTreeDraw(const float e[3], const float l[3], float fovYdeg, const float p[3], const float tri[3][3]) {
  if (SHOWTREE1119_OLD) return 0;
  float f[3] = {l[0] - e[0], l[1] - e[1], l[2] - e[2]}, fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (fl < 1e-4f) return 0;
  for (int i = 0; i < 3; i++) f[i] /= fl;
  float r[3] = {f[1] * 0.f - f[2] * 1.f + 0.f, 0.f, 0.f}, hl;   // r = f × (0,1,0)
  r[0] = -f[2]; r[1] = 0.f; r[2] = f[0]; hl = sqrtf(r[0] * r[0] + r[2] * r[2]); if (hl < 1e-4f) return 0; r[0] /= hl; r[2] /= hl;
  float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};   // u = r × f
  float tanV = tanf(fovYdeg * .5f * 3.14159265f / 180.f), tanH = tanV * ((float)SCR_W / (float)SCR_H);
  enum { CAP = 12 * 24 + 8 }; StV *v = (StV *)sceGuGetMemory(CAP * sizeof(StV)); int n = 0;
  if (tri) for (int i = 0; i < 3; i++) stEdge(v, &n, CAP, e, f, r, u, tanH, tanV, tri[i], tri[(i + 1) % 3]);
  // ближайшие 12 клеток по расстоянию до коробки
  int pick[12]; float pd[12]; int np = 0;
  for (unsigned ci = 0; ci < ncell && cl; ci++) {
    if (cl[ci].state != 2) continue;
    const float *b = cl[ci].bb;
    float dx = fmaxf(fmaxf(b[0] - p[0], p[0] - b[3]), 0.f), dz = fmaxf(fmaxf(b[2] - p[2], p[2] - b[5]), 0.f), d = dx * dx + dz * dz;
    if (np < 12) { pick[np] = (int)ci; pd[np++] = d; }
    else { int w = 0; for (int k = 1; k < 12; k++) if (pd[k] > pd[w]) w = k; if (d < pd[w]) { pick[w] = (int)ci; pd[w] = d; } }
  }
  for (int k = 0; k < np; k++) {
    const float *b = cl[pick[k]].bb;
    float c[8][3]; for (int i = 0; i < 8; i++) { c[i][0] = b[(i & 1) ? 3 : 0]; c[i][1] = b[(i & 2) ? 4 : 1]; c[i][2] = b[(i & 4) ? 5 : 2]; }
    static const unsigned char ed[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (int i = 0; i < 12; i++) stEdge(v, &n, CAP, e, f, r, u, tanH, tanV, c[ed[i][0]], c[ed[i][1]]);
  }
  if (!n) return 0;
  sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDisable(GU_FOG); sceGuDisable(GU_LIGHTING); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_ALPHA_TEST);
  sceGuScissor(0, 0, SCR_W, SCR_H);
  sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, n, 0, v);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE);
  return 1;
}
