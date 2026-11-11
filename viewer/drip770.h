// №770 (Роб 18.10 и 23.10: «там, где сверху из труб капает кислота — сделать просто векторный свет: цилиндр, как будто сверху вниз светит, сквозь
// него проезжаем, как конус; лагающие полупрозрачные капли убрать — они сильно бьют текстуры»). Капли — квады-билборды объектов Anim в зонах уровня
// (L1: три струи «deathgoop» по 15 м у проезда D5-8, две трубы «goop» АЭС со всплеском, струйка внутри АЭС). Места — из диска (tools/drip770.py ->
// drip770.bin в папке уровня). Квады капель (и свечения сложением: с №839 — мигающая звезда-блик 15 м) в DRIP_R м от этих мест не рисуются; на месте каждой струи — столб света ровного цвета
// (прямоугольник лицом к камере вокруг вертикали, сверху ярче, к полу слабее) и эллипс на полу — один вызов без текстуры, как векторный конус фонаря.
// Без коллизии (проезд сквозь). DRIP770_OLD=1 / «flag drip 0» — как было
#ifndef DRIP770_OLD
#define DRIP770_OLD 0
#endif
#define DRIP_R 1.5f        // м: квады капель вокруг места струи (струя и всплеск у её подножия)
#define DRIP_MAX 32
#ifndef DRIP_LAZYGY_OLD
#define DRIP_LAZYGY_OLD 0   // 1 — земля под всеми струями на первом кадре (до Коллизии-96)
#endif
#define DRIP_A 70.f        // альфа столба у трубы (из 255)
#define DRIP_A_LOW 22.f    // у пола
#define DRIP_POOL_A 50.f
#define DRIP_W_K .28f      // радиус столба — доля ширины квада данных (струя 7,9 м -> 1,1 м)
#define DRIP_D 90.f        // дальность
// №1131 (Роб 02.11: «капли кислоты сверху на АЭС — полупрозрачные цилиндры, надо сделать более заметными, более яркими»): по данным оригинала капля — квад-билборд с шейдерами goop (полоса-кромка внизу текстуры
// 32×32, растянута на 7,9 × 15,2 м у «deathgoop» и 3 × 10 м у труб), goo (всплеск 64×64) и свечение glow2 (круглое пятно), цвет данных чисто зелёный, цикл клипа капли 2,5 с (75 кадров по 30). Текстур у нас в
// этом месте нет (№770 заменил квады векторным столбом ради кадра), поэтому яркость делаем тем же приёмом, без текстур, одним вызовом: столб плотнее и ярче (альфа сверху 150 вместо 70, снизу 70 вместо 22),
// светлая сердцевина, три падающие капли-штриха на столб по циклу 2,5 с (падение с ускорением 9,8 м/с², хвост тянется со скоростью), пятно на полу плотнее и мягкое свечение вокруг него (центр непрозрачнее,
// к краю 0). Числа яркости, сердцевины и размера капель наши, не из оригинала. ACID1131_OLD=1 / «flag acid1131old 1» — как было (столб 70/22, пятно 50, без капель)
#include "acid1131.h"
static int acid1131Old = ACID1131_OLD;
static int dripOn = !DRIP770_OLD;
static struct { float x, z, top, r, gy; uint32_t rgb; } dripC[DRIP_MAX]; static unsigned dripN; static int dripGround;
static float dripPt[DRIP_MAX * 4][2]; static unsigned dripPtN;
static void dripFree(void) { dripN = dripPtN = 0; dripGround = 0; }
static void dripLoad(void) {
  dripFree(); if (DRIP770_OLD) return;
  char fn[256]; levelFile(fn, "drip770.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  uint32_t h[2]; struct { float x, y, z; uint32_t col; float w, h; } r;
  if (sceIoRead(f, h, 8) == 8 && h[0] == 0x31505244u && h[1] < 256u)   // 'DRP1'
    for (unsigned i = 0; i < h[1] && sceIoRead(f, &r, sizeof r) == (int)sizeof r; i++) {
      if (dripPtN < DRIP_MAX * 4) { dripPt[dripPtN][0] = r.x; dripPt[dripPtN][1] = r.z; dripPtN++; }
      unsigned k = 0; while (k < dripN && (dripC[k].x - r.x) * (dripC[k].x - r.x) + (dripC[k].z - r.z) * (dripC[k].z - r.z) > DRIP_R * DRIP_R) k++;
      float top = r.y + r.h * .5f, rad = r.w * DRIP_W_K; if (rad > 2.f) rad = 2.f; if (rad < .5f) rad = .5f;
      if (k == dripN) { if (dripN >= DRIP_MAX) continue; dripN++; dripC[k].x = r.x; dripC[k].z = r.z; dripC[k].top = top; dripC[k].r = rad; dripC[k].rgb = 0x50FF6Eu; dripC[k].gy = -1e30f; }   // ABGR: кислотно-зелёный (110, 255, 80)
      else { if (top > dripC[k].top) dripC[k].top = top; if (rad > dripC[k].r) dripC[k].r = rad; }
    }
  sceIoClose(f);
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "DRIP770 level=%d quads=%u columns=%u\n", gameLevel, dripPtN, dripN); smkWrite(s_); }
}
static int dripSkipBill(const float *v, uint32_t bfl) {   // квад капли рядом с местом струи — и картинка, и свечение сложением (с №839 оно рисовалось звездой-бликом 15 м)
  (void)bfl; if (!dripOn || !dripPtN) return 0;
  float cx = (v[0] + v[3] + v[6] + v[9]) * .25f, cz = (v[2] + v[5] + v[8] + v[11]) * .25f;
  for (unsigned i = 0; i < dripPtN; i++) { float dx = cx - dripPt[i][0], dz = cz - dripPt[i][1]; if (dx * dx + dz * dz < DRIP_R * DRIP_R) return 1; }
  return 0;
}
#ifndef DRIPTRIG1159_OLD
#define DRIPTRIG1159_OLD 0   // 1 — cosf/sinf на лету, как в TEST85
#endif
static float dripCs[17][2]; static int dripCsOk;
#define DRIP_COS(j) (DRIPTRIG1159_OLD ? cosf(6.2831853f * (j) / 16.f) : dripCs[j][0])
#define DRIP_SIN(j) (DRIPTRIG1159_OLD ? sinf(6.2831853f * (j) / 16.f) : dripCs[j][1])   // №1159: cos и sin углов 2π·j/16, j = 0..16 (те же числа, что считались на лету)
static void dripDraw(float ex, float ey, float ez, int inR) {
  if (!dripOn || !dripN || inR) return;
  if (!dripCsOk) { for (int j = 0; j <= 16; j++) { float a_ = 6.2831853f * j / 16.f; dripCs[j][0] = cosf(a_); dripCs[j][1] = sinf(a_); } dripCsOk = 1; }
  if (DRIP_LAZYGY_OLD && !dripGround && haveCol) { dripGround = 1; for (unsigned k = 0; k < dripN; k++) { float y_, n_[3]; dripC[k].gy = colGround(&world, dripC[k].x, dripC[k].z, dripC[k].top - .2f, dripC[k].top - 40.f, &y_, n_) ? y_ : dripC[k].top - 15.f; } }
  typedef struct { unsigned c; float x, y, z; } DV;
  const int ac = !acid1131Old;
  const float tAcid = (float)(gtUs95() & 0xFFFFFFu) * 1e-6f;   // с (оборот 16,7 с — по циклу 2,5 с не виден)
  unsigned b0 = ggVbUsed, nv = 0; DV *q = (DV *)ggVbAlloc(dripN * (6 + 6 + 6 * ACID_DROPS + 2 * 16 * 3) * sizeof(DV)); if (!q) return;
  for (unsigned k = 0; k < dripN; k++) {
    float dx = dripC[k].x - ex, dz = dripC[k].z - ez, d = sqrtf(dx * dx + dz * dz); if (d > DRIP_D) continue;
    if (!DRIP_LAZYGY_OLD && dripC[k].gy < -1e29f) { if (!haveCol) continue; float y_, n_[3]; dripC[k].gy = colGround(&world, dripC[k].x, dripC[k].z, dripC[k].top - .2f, dripC[k].top - 40.f, &y_, n_) ? y_ : dripC[k].top - 15.f; }   // Коллизия-96: землю под струёй — когда она в дальности (было: все струи уровня на первом кадре, за 600 м от старта — дочитывание кольца вдали)
    float fade = d > DRIP_D - 15.f ? (DRIP_D - d) / 15.f : 1.f, r = dripC[k].r, rx = d > .01f ? -dz / d * r : r, rz = d > .01f ? dx / d * r : 0.f;   // поперёк взгляда
    if (d < r + .5f) fade *= (d - r * .5f) / (r * .5f + .5f) > 0.f ? (d - r * .5f) / (r * .5f + .5f) : 0.f;   // камера внутри столба — гаснет (как конус у глаза)
    if (fade <= 0.f) continue;
    const unsigned rgbC = ac ? 0x3CFF5Au : dripC[k].rgb;   // ABGR: (90, 255, 60) ярче и зеленее прежнего (110, 255, 80)
    unsigned cT = ((unsigned)(acidAlphaTop(!ac) * fade + .5f) << 24) | rgbC, cB = ((unsigned)(acidAlphaLow(!ac) * fade + .5f) << 24) | rgbC;
    float T = dripC[k].top, B = dripC[k].gy + .02f, X = dripC[k].x, Z = dripC[k].z;
    float P[4][3] = {{X - rx, T, Z - rz}, {X + rx, T, Z + rz}, {X - rx, B, Z - rz}, {X + rx, B, Z + rz}};
    static const unsigned char tri[6] = {0, 1, 2, 2, 1, 3};
    for (int m = 0; m < 6; m++) { DV *o = &q[nv++]; int c_ = tri[m]; o->c = c_ < 2 ? cT : cB; o->x = P[c_][0]; o->y = P[c_][1]; o->z = P[c_][2]; }
    if (ac) {   // сердцевина: узкий светлый столб (0,3 радиуса) и капли-штрихи
      float cx = rx * .3f, cz = rz * .3f; unsigned kT = ((unsigned)(190.f * fade + .5f) << 24) | 0x96FFBEu, kB = ((unsigned)(95.f * fade + .5f) << 24) | 0x96FFBEu;
      float Q[4][3] = {{X - cx, T, Z - cz}, {X + cx, T, Z + cz}, {X - cx, B, Z - cz}, {X + cx, B, Z + cz}};
      for (int m = 0; m < 6; m++) { DV *o = &q[nv++]; int c_ = tri[m]; o->c = c_ < 2 ? kT : kB; o->x = Q[c_][0]; o->y = Q[c_][1]; o->z = Q[c_][2]; }
      for (int j = 0; j < ACID_DROPS; j++) {
        float yh, len; if (!acidDropAt(tAcid, j, (int)k, T, B, &yh, &len)) continue;
        float wd = .1f + .1f * r, ddx = d > .01f ? -dz / d * wd : wd, ddz = d > .01f ? dx / d * wd : 0.f, yt = yh + len; if (yt > T) yt = T;
        unsigned dH = ((unsigned)(235.f * fade + .5f) << 24) | 0xAAFFD2u, dT = (0u << 24) | 0xAAFFD2u;   // голова яркая, хвост прозрачный
        float D[4][3] = {{X - ddx, yt, Z - ddz}, {X + ddx, yt, Z + ddz}, {X - ddx, yh, Z - ddz}, {X + ddx, yh, Z + ddz}};
        for (int m = 0; m < 6; m++) { DV *o = &q[nv++]; int c_ = tri[m]; o->c = c_ < 2 ? dT : dH; o->x = D[c_][0]; o->y = D[c_][1]; o->z = D[c_][2]; }
      }
    }
    unsigned cp = ((unsigned)(acidAlphaPool(!ac) * fade + .5f) << 24) | rgbC; float pr = r * (ac ? 1.6f : 1.4f);
    float dxp = ex - X, dyp = ey - B, dzp = ez - Z, dd = sqrtf(dxp * dxp + dyp * dyp + dzp * dzp), kk = dd > .3f ? .15f / dd : .5f; if (kk > .5f) kk = .5f;   // пятно ближе к глазу (не тонет в полу)
    for (int j = 0; j < 16; j++) {   // №1159: косинусы и синусы 17 углов — из таблицы (раньше 64 вызова cosf/sinf на столб на кадр, с №1131 128)
      float Pp[3][3] = {{X, B, Z}, {X + pr * DRIP_COS(j), B, Z + pr * DRIP_SIN(j)}, {X + pr * DRIP_COS(j + 1), B, Z + pr * DRIP_SIN(j + 1)}};
      for (int m = 0; m < 3; m++) { DV *o = &q[nv++]; o->c = cp; o->x = Pp[m][0] + (ex - Pp[m][0]) * kk; o->y = Pp[m][1] + (ey - Pp[m][1]) * kk; o->z = Pp[m][2] + (ez - Pp[m][2]) * kk; } }
    if (ac) {   // мягкое свечение вокруг пятна: центр плотнее, к краю прозрачное (вершинный градиент, тот же вызов)
      float hr = r * 3.f; unsigned hc = ((unsigned)(90.f * fade + .5f) << 24) | 0x3CFF3Cu, he = 0x3CFF3Cu;
      for (int j = 0; j < 16; j++) {
        float Pp[3][3] = {{X, B, Z}, {X + hr * DRIP_COS(j), B, Z + hr * DRIP_SIN(j)}, {X + hr * DRIP_COS(j + 1), B, Z + hr * DRIP_SIN(j + 1)}};
        for (int m = 0; m < 3; m++) { DV *o = &q[nv++]; o->c = m ? he : hc; o->x = Pp[m][0] + (ex - Pp[m][0]) * kk; o->y = Pp[m][1] + (ey - Pp[m][1]) * kk; o->z = Pp[m][2] + (ez - Pp[m][2]) * kk; } }
    }
  }
  if (!nv) return;
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); setCull(0); sceGuEnable(GU_FOG);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState();
}
