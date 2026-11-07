// №1219 (Роб 06.11: «в бонус-игре в гонках в машинах никого нету водителей… просто чтобы внутри кто-то сидел, кто не сильно будет грузить»; 15:0x «да» —
// нейтральная фигура): в каждой машине бонус-гонки на месте водителя сидит неподвижная фигура-манекен своей геометрии — торс, шея, голова, руки к рулю,
// без текстур, цвета вершин с готовой светотенью (верх светлее, бока и низ темнее), ≈80 треугольников, один вызов на машину. Персонажей сериала не рисуем.
// Место — сустав dl модели (seat x y z блока машины msncars, psp_export CAR_INFO_FMT: info + 16 Б, поля 12..14), как у водителей машин миссий (msn_view.c
// drawDrivers). Нет блока машины — без фигуры; разбитая — без фигуры; дальше BDRV_FAR м — без фигуры. BNSDRIVER1219_OLD=1 — как было (машины пустые)
#ifndef BNSDRIVER1219_OLD
#define BNSDRIVER1219_OLD 0
#endif
#define BDRV_SC 2.f      // вершины — short / 32767 × BDRV_SC м
#define BDRV_FAR 60.f
#define BDRV_MAXV 288
#define BDRV_K 1.25f     // рост фигуры: 1 — человек; 1,25 — плечи над спинкой сиденья, как у водителей оригинала (эмулятор 06.11: при 1 из-за спинки видна одна голова)
static unsigned char *bdrvV; static int bdrvN; static float bdrvLo[3], bdrvHi[3];
static void bdrvPut(const float p[3], uint32_t c) {
  if (bdrvN >= BDRV_MAXV) return;
  unsigned char *q = bdrvV + (size_t)bdrvN * 16; memset(q, 0, 16); memcpy(q + 4, &c, 4);
  for (int e = 0; e < 3; e++) { float x = p[e] * BDRV_K / BDRV_SC * 32767.f; ((short *)(q + 8))[e] = (short)(x < -32767.f ? -32767 : x > 32767.f ? 32767 : x);
    if (p[e] * BDRV_K < bdrvLo[e]) bdrvLo[e] = p[e] * BDRV_K; if (p[e] * BDRV_K > bdrvHi[e]) bdrvHi[e] = p[e] * BDRV_K; }
  bdrvN++;
}
static uint32_t bdrvShade(uint32_t rgb, float k) {   // rgb 0xRRGGBB × k -> ABGR вершины
  int r = (int)(((rgb >> 16) & 255) * k), g = (int)(((rgb >> 8) & 255) * k), b = (int)((rgb & 255) * k);
  r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
  return 0xFF000000u | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
}
static void bdrvQuad(const float a[3], const float b[3], const float c[3], const float d[3], uint32_t col) { bdrvPut(a, col); bdrvPut(b, col); bdrvPut(c, col); bdrvPut(a, col); bdrvPut(c, col); bdrvPut(d, col); }
// брусок между точками p0 (низ) и p1 (верх) с полушириной hx и полуглубиной hz в плоскости, поперечной оси (ось почти вертикальна или наклонена вперёд)
static void bdrvBar(const float p0[3], const float p1[3], float hx, float hz, uint32_t rgb) {
  float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2], l = sqrtf(ax * ax + ay * ay + az * az); if (l < 1e-4f) return; ax /= l; ay /= l; az /= l;
  float ux = 1.f, uy = 0.f, uz = 0.f;                                         // поперечная ось X модели (вправо)
  float wx = ay * uz - az * uy, wy = az * ux - ax * uz, wz = ax * uy - ay * ux, wl = sqrtf(wx * wx + wy * wy + wz * wz);
  if (wl < 1e-3f) { ux = 0.f; uz = 1.f; wx = ay * uz - az * uy; wy = az * ux - ax * uz; wz = ax * uy - ay * ux; wl = sqrtf(wx * wx + wy * wy + wz * wz); }
  wx /= wl; wy /= wl; wz /= wl; ux = wy * az - wz * ay; uy = wz * ax - wx * az; uz = wx * ay - wy * ax;   // u — вправо, w — вперёд/назад
  float v[8][3];
  for (int i = 0; i < 8; i++) { const float *o = i < 4 ? p0 : p1; float sx = (i & 1) ? hx : -hx, sz = (i & 2) ? hz : -hz;
    v[i][0] = o[0] + ux * sx + wx * sz; v[i][1] = o[1] + uy * sx + wy * sz; v[i][2] = o[2] + uz * sx + wz * sz; }
  bdrvQuad(v[4], v[5], v[7], v[6], bdrvShade(rgb, 1.00f));   // торец верхний
  bdrvQuad(v[0], v[2], v[3], v[1], bdrvShade(rgb, .55f));    // торец нижний
  bdrvQuad(v[0], v[1], v[5], v[4], bdrvShade(rgb, .80f));    // грани
  bdrvQuad(v[2], v[6], v[7], v[3], bdrvShade(rgb, .70f));
  bdrvQuad(v[0], v[4], v[6], v[2], bdrvShade(rgb, .75f));
  bdrvQuad(v[1], v[3], v[7], v[5], bdrvShade(rgb, .75f));
}
// голова: восьмигранник в три пояса (низ — полюс, два кольца, верх — полюс), центр c, радиус r
static void bdrvHead(const float c[3], float r, uint32_t rgb) {
  enum { N = 8 }; float ring[2][N][3], top[3] = {c[0], c[1] + r * 1.05f, c[2]}, bot[3] = {c[0], c[1] - r * 1.1f, c[2]};
  const float ry[2] = {-.45f, .45f};
  for (int k = 0; k < 2; k++) for (int j = 0; j < N; j++) { float a = 6.2831853f * (float)j / N, rr = r * .95f;
    ring[k][j][0] = c[0] + rr * cosf(a); ring[k][j][1] = c[1] + r * ry[k]; ring[k][j][2] = c[2] + rr * .92f * sinf(a); }
  for (int j = 0; j < N; j++) { int j2 = (j + 1) % N;
    bdrvPut(top, bdrvShade(rgb, 1.f)); bdrvPut(ring[1][j2], bdrvShade(rgb, 1.f)); bdrvPut(ring[1][j], bdrvShade(rgb, 1.f));
    bdrvQuad(ring[0][j], ring[1][j], ring[1][j2], ring[0][j2], bdrvShade(rgb, .82f));
    bdrvPut(bot, bdrvShade(rgb, .6f)); bdrvPut(ring[0][j], bdrvShade(rgb, .6f)); bdrvPut(ring[0][j2], bdrvShade(rgb, .6f)); }
}
// сетка строится раз: начало координат — сустав dl (таз на сиденье), +Y вверх, −Z вперёд (модель машины смотрит в −Z), руль впереди
static int bdrvBuild(void) {
  if (bdrvV) return 1;
  bdrvV = (unsigned char *)memalign(16, BDRV_MAXV * 16); if (!bdrvV) return 0;
  bdrvN = 0; for (int e = 0; e < 3; e++) { bdrvLo[e] = 1e9f; bdrvHi[e] = -1e9f; }
  const uint32_t body = 0x4E545C, skin = 0xB4B0A8;   // торс — тёмный серо-синий, голова и кисти — светлый серый манекена
  float t0[3] = {0.f, .02f, .03f}, t1[3] = {0.f, .52f, .06f}, n0[3] = {0.f, .52f, .06f}, n1[3] = {0.f, .60f, .05f}, hc[3] = {0.f, .72f, .04f};
  bdrvBar(t0, t1, .19f, .11f, body);                                                       // торс (чуть откинут назад)
  bdrvBar(n0, n1, .05f, .05f, skin);                                                       // шея
  bdrvHead(hc, .12f, skin);                                                                // голова
  for (int s = -1; s <= 1; s += 2) {                                                       // плечо -> локоть -> кисть на руле
    float sh[3] = {s * .2f, .46f, .05f}, el[3] = {s * .22f, .30f, -.18f}, hd[3] = {s * .16f, .40f, -.40f};
    bdrvBar(sh, el, .045f, .045f, body); bdrvBar(el, hd, .04f, .04f, skin);
  }
  sceKernelDcacheWritebackRange(bdrvV, (unsigned)(bdrvN * 16));   // GE читает вершины из памяти (урок №863)
  return 1;
}
static unsigned bdrvDrawn;   // служебное: фигур за кадр
static float bdrvEye[3];      // глаз камеры кадра (ставит кадр бонус-гонки до машин)
static void bnsDriverDraw(const Car *k, const char *name, const McrModel *Mm) {
  if (BNSDRIVER1219_OLD || k->destroyed || !Mm) return;
  const McGeo *G = mcGeoFind(name); float f[27]; memset(f, 0, sizeof f); if (G && G->hasInfo) memcpy(f, G->info + 16, sizeof f);   // seat x y z = f[12..14]
  if (smkOn) { static char seen[4][16]; int i_ = 0; while (i_ < 4 && seen[i_][0] && strcmp(seen[i_], name)) i_++;
    if (i_ < 4 && !seen[i_][0]) { snprintf(seen[i_], 16, "%s", name); char s_[128]; snprintf(s_, sizeof s_, "BONUS ev=driver car=%s geo=%d info=%d seat=%.2f,%.2f,%.2f t=%.1f\n", name, G != 0, G ? G->hasInfo : 0, f[12], f[13], f[14], smkT); bnsLog(s_); } }
  if (!G || !G->hasInfo) return;
  if (!(fabsf(f[12]) < 2.f && f[13] > -1.f && f[13] < 2.5f && fabsf(f[14]) < 3.f) || (f[12] == 0.f && f[13] == 0.f && f[14] == 0.f)) return;
  float dx = k->x - bdrvEye[0], dz = k->z - bdrvEye[2]; if (dx * dx + dz * dz > BDRV_FAR * BDRV_FAR) return;
  if (!bdrvBuild()) return;
  float m[16]; carPieceMatrix(k, carModelLift(k, Mm->M->lift), f[12], f[13], f[14], BDRV_SC, m);
  Piece d; memset(&d, 0, sizeof d);
  for (int e = 0; e < 3; e++) { d.r.bb[e] = bdrvLo[e]; d.r.bb[3 + e] = bdrvHi[e]; }
  d.r.tex = -1; d.r.flags = 16; d.r.nv = bdrvN; d.r.ni = bdrvN; d.r.owner = 1; d.r.sc = BDRV_SC; d.v = (Vert *)bdrvV;   // 16 — без индексов
  drawPieceM(&d, (const ScePspFMatrix4 *)m); bdrvDrawn++;
}
