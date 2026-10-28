// №658 «свет и тени спрайтами, одним модулем» (слово Роба 16.10: «как это используется на PSP: спрайты, которые имитируют свет, без настоящего свечения;
// то же с текстурами тени»). Подключается из main.c после lightsprite.h, до main_loop.h.
//   ТЕНИ (flag shadowspr 1 / -DSHADOW_SPRITE=1): под Гомером, своей машиной, чужими машинами и прохожими один квад с мягкой текстурой на земле
//   (наклон — по нормали опоры коллизии под центром, без проекции на геометрию), одной пачкой на текстуру, общий бюджет SL_SH_MAX квадов.
//   Прежние пятна (homerShadow, carVisDrawShadow) при включённом модуле НЕ вызываются (main_loop.h), а не обходятся.
//   Строка в smoke.txt SHADOWSPR: квады/кадр, пачки, мкс/кадр.
#ifndef SPRITELIGHT_H
#define SPRITELIGHT_H
#ifndef SHADOW_SPRITE
#define SHADOW_SPRITE 0       // до тура выкл; «flag shadowspr 1» включает без пересборки
#endif
#define SL_SH_MAX 56          // общий бюджет квадов теней на кадр (№748: +люди миссий, прохожие, предметы; было 28)
#define SV_A 51u              // прозрачность векторной тени машины (из 255): 20 % чёрного (Роб 17.10: «еле заметные… я б дал 20 % чёрного»; TEST31: «под машиной выглядит хорошо»)
// №748/№749 (Роб 17.10, TEST31 на PSP, видео IMG_7282 3:00–3:30): «тень под Гомером прозрачнее, может даже меньше, и всем людям такую же… чтобы у людей и у
// объектов, у всего была векторная тень»; «когда он прыгает, тень должна уменьшаться, и у всех» — тень показывает, где стоишь: пропрыгнуть, переступить, край.
// Люди (Гомер, персонажи миссий, прохожие) и предметы — эллипс тоном SV_A_PPL; в прыжке/полёте тень на земле под ним меньше (SV_JUMP_*); у края опоры тень
// Гомера обрезается по краю (svHeroRim). SHADOW_PPL_OLD=1 (файл shadowpplold, «flag shpplold 1») — как в TEST31
#ifndef SHADOW_PPL_OLD
#define SHADOW_PPL_OLD 0
#endif
#define SV_A_PPL 36u          // люди и предметы: 14 % чёрного (прозрачнее машины)
#define SV_JUMP_K .1f         // тень меньше на 10 % за метр высоты над землёй…
#define SV_JUMP_MIN .5f       // …но не меньше половины («масштаб минимальный»)
#define SV_PPL_R .32f         // радиус тени человека ростом SV_PPL_H, м (= тень Гомера SL_HS_R × SV_HERO_K)
#define SV_PPL_H 1.84f        // рост Гомера в модели (skin.h)
#define SV_NPC_R 15.f         // люди миссий дальше — без тени (как трафик SL_SH_R)
#define SV_PROP_R 12.f        // предметы ближе к глазу, м
#define SV_PROP_MAX 12        // предметов с тенью на кадр (ближние)
#define SV_PROP_HMAX 2.2f     // только невысокие предметы (урны, ящики, почтовые ящики, гидранты, скамейки): у фонаря, столба, знака коробка с кронштейном — тень-эллипс ложилась бы длинной полосой
#define SV_PROP_ASP 4.f       // и не вытянутые сильнее 4 : 1
#define SV_EDGE_DROP .3f      // край опоры: земля ниже опоры героя больше чем на 0,3 м — тень обрезается по краю
static int slPplOld = SHADOW_PPL_OLD; static unsigned slPplN, slPropN, slRimUs, slRimCut;   // служебное: в строке SHADOWSPR
#define SL_SH_R 15.f          // PS2: тень трафика на 15 м уже не различима, на 30 м нет — чужие машины и прохожие дальше 15 м без тени (Гомер и своя машина всегда)
#define SL_SH_PED_R 15.f
#ifndef HOMER_SPOT_OLD
#define HOMER_SPOT_OLD 0      // 1 — прежнее пятно Гомера (файл homerspotold рядом с EBOOT)
#endif
#ifndef HOMER_SPOT
#define HOMER_SPOT 1          // 16.10 (план финала, Камера п.3): пятно Гомера как у PS2; выкл — прежний круг 0,4 м под ногами. «flag homerspot 1» включает без пересборки (и сам модуль теней)
#endif
// PS2 (ps2_light_shadow_car_homer_l1, ps2_light_garage_truck_homer): у Гомера мягкое небольшое пятно у ног, чуть в сторону падения тени (от солнца), шире ступней, силуэт не повторяется.
// Круг 0,4 м целиком уходил под ступни (на PSP и в эмуляторе разница с выкл. 3–6 единиц): радиус 0,62 м, центр смещён от солнца на 0,14 м, альфа 150
#define SL_HS_R .62f
#define SL_HS_OFF .14f
#define SL_SH_DENS 122.f      // плотность тени (альфа середины, 0..255; ×0,835 в веере ≈ 0,40): одно число на машины и героев; по кадрам PS2 honor_v (lv2_seat) мягкое пятно, затемнение ~35 %
#define SL_HS_A SL_SH_DENS
static int slShadowOn = SHADOW_SPRITE; static int slHomerSpot = HOMER_SPOT; static int slLightInitDummy;

// Автотур «ИГРОК» (ветка claude/autotour-player-1016, viewer/acct.h): раз в секунду строка «quads=.. us=.. shadows=..» для счёта «свет и тени»; без acct.h — ничего не считается
#ifndef SL_NO_ACCT   // acct.h на магистрали (acctSetLight не static); без него — -DSL_NO_ACCT=1
extern void acctSetLight(const char *s);
static unsigned slAcN, slAcQ, slAcSh, slAcUs, slAcT0;
static void slAcctAdd(unsigned quads, unsigned shadows, unsigned us) {
  unsigned now = sceKernelGetSystemTimeLow(); if (!slAcT0) slAcT0 = now;
  slAcQ += quads; slAcSh += shadows; slAcUs += us; slAcN++;
  if (now - slAcT0 >= 1000000u) { char b[96]; unsigned n = slAcN ? slAcN : 1; snprintf(b, sizeof b, "quads=%u us=%u shadows=%u", slAcQ / n, slAcUs / n, slAcSh / n); acctSetLight(b); slAcN = slAcQ = slAcSh = slAcUs = 0; slAcT0 = now; }
}
#define SL_AC_T0 unsigned ac0_ = sceKernelGetSystemTimeLow()
#else
#define SL_AC_T0 unsigned ac0_ = 0
#define slAcctAdd(q, sh, us) ((void)0)
#endif
static unsigned slShQuads, slShDraws, slShUs, slShFrames;
typedef struct { float u, v; unsigned c; float x, y, z; } SlV;
typedef struct { float x, y, z, n[3], fx, fz, hx, hz; unsigned a, tone; int box, rim, own; const void *key; const float *sil; } SlSh;   // sil — №811: силуэт машины сверху (CS_N точек в долях габарита) вместо восьмиугольника   // tone — доля чёрного векторной тени (из 255), rim — обрезка по краю (slRim)
static SlSh slSh[SL_SH_MAX]; static int slShN;
// Текстура тени оригинала beeshadow.bmp (l0N_fx.p3d, 64×64, белый мягкий диск с альфой; рисуется затемнением, не сложением): радиальный профиль альфы по записи PS2
// (кадры build/ps2ref/light_tex/l01_fx.p3d__beeshadow.png): плато 213 до r=12 текселей, спад к 0 на r=19 (диск ≈ 59 % половины квада). Файла в наших пакетах нет — снимается в код.
static const unsigned char slProf[33] = {213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 212, 209, 196, 163, 109, 52, 16, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define SL_DISK 0.59f         // радиус диска в долях половины квада
static unsigned slTex[64 * 64] __attribute__((aligned(16))); static int slTexReady;
static void slTexInit(void) {
  for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
    float r = sqrtf((x - 31.5f) * (x - 31.5f) + (y - 31.5f) * (y - 31.5f)); int k = (int)r; float f = r - k; if (k > 31) k = 31;
    float a = slProf[k] * (1.f - f) + slProf[k + 1] * f;
    slTex[y * 64 + x] = ((unsigned)(a + .5f)) << 24;   // ABGR: чёрный, альфа — тень (оригинал белый и красит альфой; у нас обычное затемнение SRC_ALPHA / 1−SRC_ALPHA)
  }
  sceKernelDcacheWritebackRange(slTex, sizeof slTex); slTexReady = 1;
}
static int slShOwnNext; static const void *slShKeyNext; static const ColWorld *slShW;   // №973 служебно: тень своей машины для замера (SHADOW973)
static const float *slShSilNext;   // №811: силуэт для следующей тени машины (slShCar)
static int slShAddT(float x, float y, float z, const float n[3], float fx, float fz, float hx, float hz, float alpha, int box, unsigned tone) {
  if (slShN >= SL_SH_MAX || alpha < 2.f) return 0;
  SlSh *s = &slSh[slShN++]; s->x = x; s->y = y; s->z = z; memcpy(s->n, n, 12); s->fx = fx; s->fz = fz; s->hx = hx; s->hz = hz;
  alpha *= skinToonShadowK();   // №675: в мультике тени слабее
  s->a = (unsigned)(alpha > 255.f ? 255.f : alpha); s->box = box; s->tone = tone; s->rim = 0; s->own = slShOwnNext; slShOwnNext = 0; s->key = slShKeyNext; slShKeyNext = 0; s->sil = slShSilNext; slShSilNext = 0;
  return 1;
}
static void slShAdd(float x, float y, float z, const float n[3], float fx, float fz, float hx, float hz, float alpha, int box) { slShAddT(x, y, z, n, fx, fz, hx, hz, alpha, box, SV_A); }
#ifndef SL_SH_FAN_OLD
#define SL_SH_FAN_OLD 0       // 1 — прежняя тень текстурой 64×64 (квадратные края), красный контроль
#endif
#define SL_FAN_N 16           // сегментов веера
static int slShFanOld = SL_SH_FAN_OLD;
typedef struct { unsigned c; float x, y, z; } SlFV;
// №716/722/735/737 (видео IMG_7280, слово Роба 17.10 «полутон с крупным зерном, а не полупрозрачность»): тени точками — альфа-тест по узору, без смешивания
// и сортировки; точки плотные в середине и редеют к краю (альфа вершин × альфа узора). Машина — прямоугольник по кузову, герой и прохожие — круг.
// Тень притянута к камере по лучу взгляда (SL_HT_PULL): не тонет под нарисованным асфальтом (он выше опоры коллизии на 5–10 см) и не рвётся о бордюр,
// как плоский диск с отступом 4 единицы глубины (звезда на снимке q_hit709_12). Откат: -DSL_SH_HALF_OLD=1, файл shadowhalfold, «flag shhalf 0»
#ifndef SL_SH_HALF_OLD
#define SL_SH_HALF_OLD 1   // 17.10 16:5x слово Роба «тень очень жуткая»: точки выключены, тени как в TEST30
#endif
#define SL_HT_CELL .065f      // шаг точек на земле, м (Роб 17.10: 10 см — «слишком крупная»)
#define SL_HT_REF 61          // порог альфа-теста: в середине точки занимают ~70 % площади (Роб: «70 % чёрного»), к краю мельче
#define SL_HT_PULL .15f       // притяжка к камере, м
#define SL_HT_PULLK .08f      // не больше этой доли расстояния до глаза
#define SL_HT_SOFT .28f       // ширина редеющего края у прямоугольника машины, м
#define SL_HT_RGB 0x00000000u // точки чёрные (Роб 17.10: «прям чёрную»); тон задаёт площадь точек
static int slShHalf = !SL_SH_HALF_OLD;   // узор и состояние полутона — viewer/halftone.h (общие с конусами света)
static float slCarKX, slCarKZ;   // определены ниже (тень машин №680): доли кузова у прежнего овала
static float slHtK;   // доля притяжки к камере для текущей тени (по центру: одна sqrt на тень)
static void slHtV(SlV *o, const float P[3], const float C[3], const float R[3], const float U[3], unsigned a) {
  float lx = (P[0] - C[0]) * R[0] + (P[1] - C[1]) * R[1] + (P[2] - C[2]) * R[2], lz = (P[0] - C[0]) * U[0] + (P[1] - C[1]) * U[1] + (P[2] - C[2]) * U[2];
  float ex_ = eyeX - P[0], ey_ = eyeY - P[1], ez_ = eyeZ - P[2], k = slHtK;
  htUV(lx, lz, SL_HT_CELL, &o->u, &o->v); o->c = (a << 24) | SL_HT_RGB;   // сетка под 45° — ромбиком (Роб 17.10)
  o->x = P[0] + ex_ * k; o->y = P[1] + ey_ * k; o->z = P[2] + ez_ * k;
}
static void slShFlushHalf(void) {
  unsigned bb0 = ggVbUsed, nv = 0;
  for (int i = 0; i < slShN; i++) nv += slSh[i].box ? 54u : (unsigned)SL_FAN_N * 9u;
  SlV *q = (SlV *)ggVbAlloc(nv * sizeof(SlV)); unsigned k = 0;
  for (int i = 0; i < slShN; i++) { const SlSh *s = &slSh[i];
    float dn = s->fx * s->n[0] + s->fz * s->n[2];
    float U[3] = {s->fx - s->n[0] * dn, -s->n[1] * dn, s->fz - s->n[2] * dn}, ul = sqrtf(U[0] * U[0] + U[1] * U[1] + U[2] * U[2]); if (ul < 1e-4f) continue;
    U[0] /= ul; U[1] /= ul; U[2] /= ul;
    float R[3] = {s->n[1] * U[2] - s->n[2] * U[1], s->n[2] * U[0] - s->n[0] * U[2], s->n[0] * U[1] - s->n[1] * U[0]};
    float C[3] = {s->x + s->n[0] * .04f, s->y + s->n[1] * .04f, s->z + s->n[2] * .04f};
    { float dx = eyeX - C[0], dy = eyeY - C[1], dz = eyeZ - C[2], d = sqrtf(dx * dx + dy * dy + dz * dz); slHtK = d > 1e-3f ? fminf(SL_HT_PULL / d, SL_HT_PULLK) : 0.f; }
    unsigned ac = s->a * 2u > 255u ? 255u : s->a * 2u;   // плотность середины: SL_SH_DENS ≈ 122 -> почти сплошные точки
    if (s->box) {   // машина: прямоугольник чуть шире кузова (габарит CarBox), край редеет на SL_HT_SOFT
      float hw = s->hx * SL_DISK / slCarKX * 1.10f, hl = s->hz * SL_DISK / slCarKZ * 1.05f, sw = fminf(SL_HT_SOFT, hw * .5f), sl = fminf(SL_HT_SOFT, hl * .5f);
      float xs[4] = {-hw, -hw + sw, hw - sw, hw}, zs[4] = {-hl, -hl + sl, hl - sl, hl}, G[4][4][3]; unsigned A[4][4];
      for (int a_ = 0; a_ < 4; a_++) for (int b_ = 0; b_ < 4; b_++) {
        for (int c = 0; c < 3; c++) G[a_][b_][c] = C[c] + R[c] * xs[a_] + U[c] * zs[b_];
        A[a_][b_] = (a_ == 0 || a_ == 3 || b_ == 0 || b_ == 3) ? 0u : ac; }
      static const unsigned char qa[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
      for (int a_ = 0; a_ < 3; a_++) for (int b_ = 0; b_ < 3; b_++) for (int m = 0; m < 6; m++) {
        int ia = a_ + qa[m][0], ib = b_ + qa[m][1]; slHtV(&q[k++], G[ia][ib], C, R, U, A[ia][ib]); }
    } else {        // герой, прохожий: круг, с 0,6 радиуса точки редеют к краю
      float r = s->hx * SL_DISK, rz = s->hz * SL_DISK, P[2][SL_FAN_N + 1][3]; unsigned am = ac * 9u / 10u;
      for (int j = 0; j <= SL_FAN_N; j++) { float th = (float)(j % SL_FAN_N) * (6.2831853f / SL_FAN_N), cs = cosf(th), sn = sinf(th);
        for (int rr = 0; rr < 2; rr++) { float f = rr ? 1.f : .6f; for (int c = 0; c < 3; c++) P[rr][j][c] = C[c] + (R[c] * r * cs + U[c] * rz * sn) * f; } }
      for (int j = 0; j < SL_FAN_N; j++) {
        slHtV(&q[k++], C, C, R, U, ac); slHtV(&q[k++], P[0][j], C, R, U, am); slHtV(&q[k++], P[0][j + 1], C, R, U, am);
        slHtV(&q[k++], P[0][j], C, R, U, am); slHtV(&q[k++], P[1][j], C, R, U, 0); slHtV(&q[k++], P[0][j + 1], C, R, U, am);
        slHtV(&q[k++], P[0][j + 1], C, R, U, am); slHtV(&q[k++], P[1][j], C, R, U, 0); slHtV(&q[k++], P[1][j + 1], C, R, U, 0); }
    }
    slShQuads++;
  }
  if (k) {
    htBegin(SL_HT_REF);
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)k, 0, q); slShDraws++;
    ggVbWriteback(bb0); htEnd();
  }
  slShN = 0;
}
// Роб 17.10 (после векторного света): «у машины тоже чёткая тень» — векторная тень: машина — тёмный силуэт по габариту кузова (прямоугольник со срезанными
// углами, 8 вершин), герой и прохожий — эллипс 16 сторон; ровная прозрачность без размытия, одна высота на тень (без «звезды» на бордюре), притянута к камере
// по лучу взгляда (не тонет в асфальте). Все тени кадра — один вызов без текстуры. SHADOW_VEC_OLD=1 (файл shadowvecold, «flag shvec 0») — прежние тени
#ifndef SHADOW_VEC_OLD
#define SHADOW_VEC_OLD 0
#endif
// SV_A (машина) и SV_A_PPL (люди, предметы) — в начале файла
#define SV_CUT .28f        // срез углов прямоугольника машины — доля меньшей полуоси
#define SV_N 16            // сторон эллипса героя
#define CS_N 32            // №811: точек силуэта машины сверху
#define SV_HERO_K .52f     // радиус эллипса героя — доля размера из списка теней (было 0,8 → 0,6: Гомер 0,50 → 0,37 м; №748 «может даже меньше»: 0,52 — 0,32 м)
#define SV_HERO_K_OLD .6f  // как в TEST31 (flag shpplold 1)
#define SV_CUT_ZK .35f     // срез углов по длине машины — доля среза по ширине (боковые точки ближе к торцу, скос пологий)
#define SV_CAR_W .96f      // ширина тени машины — доля габарита кузова (было 1,05)
#define SV_CAR_L .94f      // длина (было 1,02)
static int slShVec = !SHADOW_VEC_OLD;
#define SV_HERO_BK .12f   // тень героя: к камере, м (Роб 17.10: «ниже, к нам»)
#define SV_HERO_RT .05f   // и вправо по экрану, м («чуть вправо»)
static int slHeroCenterOld;   // flag shherooff 1 — тень героя со сдвигом от солнца SL_HS_OFF (как было)
static float svCs[SV_N][2]; static int svCsOn;   // косинус и синус сторон эллипса — один раз, не на каждую тень
static float slRim[SV_N];   // №749: доля радиуса по каждой вершине эллипса героя (1 — целиком, меньше — край опоры ближе)
static float svJumpK(float h) { return h <= 0.f ? 1.f : fmaxf(SV_JUMP_MIN, 1.f - SV_JUMP_K * h); }   // №749: в прыжке тень меньше
static float svHk(void) { return slPplOld ? SV_HERO_K_OLD : SV_HERO_K; }
// №853 служебно (diaglog): треугольники векторных теней — в оси камеры (как diagGeo): под риском защитной полосы, точно выброшенные GE, и «улетевшие»
// вершины (дальше 3 м от центра тени). Строка SHDIAG раз в секунду; худшая тень — с местом, нормалью и расстоянием до глаза
static unsigned svdRisk, svdDrop, svdWild, svdTri; static float svdMax, svdW[10];
static unsigned svCutTri, svCutFans, svCutUs;   // №853: срез теней у камеры — треугольников, вееров, мкс за секунду
static void svDiag853(const SlSh *s, const float C[3], float d, const unsigned *v0, unsigned n, unsigned stride) {
  float mx = 0.f;
  for (unsigned t = 0; t + 2 < n; t += 3) { GgV g[3];
    for (int m = 0; m < 3; m++) { const float *p = (const float *)((const char *)v0 + (t + m) * stride + 4); float e0 = p[0] - ggE[0], e1 = p[1] - ggE[1], e2 = p[2] - ggE[2];
      g[m].c[0] = ggR[0] * e0 + ggR[1] * e1 + ggR[2] * e2; g[m].c[1] = ggU[0] * e0 + ggU[1] * e1 + ggU[2] * e2; g[m].c[2] = ggF[0] * e0 + ggF[1] * e1 + ggF[2] * e2;
      float r = sqrtf((p[0] - C[0]) * (p[0] - C[0]) + (p[1] - C[1]) * (p[1] - C[1]) + (p[2] - C[2]) * (p[2] - C[2])); if (r > mx) mx = r; }
    svdTri++; if (ggRisk(&g[0], &g[1], &g[2], &ggK)) { svdRisk++; if (ggDropExact(&g[0], &g[1], &g[2], &ggK)) svdDrop++; } }
  if (mx > 3.f) svdWild++;
  if (mx > svdMax) { svdMax = mx; svdW[0] = C[0]; svdW[1] = C[1]; svdW[2] = C[2]; svdW[3] = s->n[0]; svdW[4] = s->n[1]; svdW[5] = s->n[2]; svdW[6] = d; svdW[7] = s->hx; svdW[8] = s->hz; svdW[9] = (float)(s->box ? (s->sil ? 2 : 1) : 0); }
}
static void svDiagSecond853(void) {
  char l_[360]; snprintf(l_, sizeof l_, "SHDIAG t=%.1f sc=%s tri=%u risk=%u drop=%u wild=%u max_m=%.2f at=%.1f,%.1f,%.1f n=%.2f,%.2f,%.2f eye_d=%.2f hx=%.2f hz=%.2f kind=%.0f zn=%.2f cut_tri=%u cut_fans=%u cut_us=%u\n",
    smkT, smkScene, svdTri, svdRisk, svdDrop, svdWild, svdMax, svdW[0], svdW[1], svdW[2], svdW[3], svdW[4], svdW[5], svdW[6], svdW[7], svdW[8], svdW[9], ggK.zn, svCutTri, svCutFans, svCutUs); smkWrite(l_);
  svdRisk = svdDrop = svdWild = svdTri = 0; svdMax = 0.f; svCutTri = svCutFans = svCutUs = 0;
}
// №853 (Роб 20.10, видео IMG_7907, L6 набережная: «под ногами пол ломается, по диагонали непонятные текстуры бьются»): Барт вышел из
// машины — камера у кузова, ближняя плоскость опускается до 0,25–0,35 м (frameNear), защитная полоса GE у near сужается до ±2 м.
// Векторная тень машины (восьмиугольник до 2,7 м от центра) уходит под камеру: её треугольники одни кадры GE рисует клином до низа
// экрана, другие выбрасывает целиком — тёмные клинья мигают по полу (эмулятор, выход из машины у будок L6: 37–50 треугольников теней
// в секунду под риском, 19–25 точно выброшены). Правило города (ge_guard.h, правка 340): треугольник тени под риском и в кадре режется
// на процессоре по пирамиде полосы, веером; дальние тени — одна проверка шара. SV_CUT853_OLD=1 — как было
#ifndef SV_CUT853_OLD
#define SV_CUT853_OLD 0
#endif
typedef struct { unsigned c; float x, y, z; } SvV;
typedef struct { const SvV *p; int n; } SvFan;
static SvFan svFan[48]; static int svFanN;
static int svNearRisk(const SvV *v, unsigned n) {   // шар вершин тени целиком в безопасной пирамиде (или целиком за near) — резать нечего
  float c[3] = {0.f, 0.f, 0.f}; for (unsigned i = 0; i < n; i++) { c[0] += v[i].x; c[1] += v[i].y; c[2] += v[i].z; } float inv = 1.f / (float)n; c[0] *= inv; c[1] *= inv; c[2] *= inv;
  float r2 = 0.f; for (unsigned i = 0; i < n; i++) { float dx = v[i].x - c[0], dy = v[i].y - c[1], dz = v[i].z - c[2], q = dx * dx + dy * dy + dz * dz; if (q > r2) r2 = q; }
  float r = sqrtf(r2), e0 = c[0] - ggE[0], e1 = c[1] - ggE[1], e2 = c[2] - ggE[2];
  float x = ggR[0] * e0 + ggR[1] * e1 + ggR[2] * e2, y = ggU[0] * e0 + ggU[1] * e1 + ggU[2] * e2, z = ggF[0] * e0 + ggF[1] * e1 + ggF[2] * e2;
  if (z + r < ggK.zn) return 0;   // весь за ближней плоскостью: GE не рисует ничего
  float zl = z - r, sx = ggK.tvx * (GG_SAFE / 240.f), sy = ggK.tvy * (GG_SAFE / 136.f), kx = sqrtf(1.f + sx * sx), ky = sqrtf(1.f + sy * sy);
  return !(zl > ggK.zn * GG_NEAR && sx * z - fabsf(x) > r * kx && sy * z - fabsf(y) > r * ky);
}
static unsigned svCutTris(SvV *v, unsigned n) {   // треугольники под риском — веерами в svFan (память кольца), остальные сдвигаются к началу; возвращает их число вершин
  unsigned w = 0;
  for (unsigned t = 0; t + 2 < n; t += 3) { GgV g[3];
    for (int m = 0; m < 3; m++) { const SvV *p = &v[t + m]; float e0 = p->x - ggE[0], e1 = p->y - ggE[1], e2 = p->z - ggE[2];
      g[m].c[0] = ggR[0] * e0 + ggR[1] * e1 + ggR[2] * e2; g[m].c[1] = ggU[0] * e0 + ggU[1] * e1 + ggU[2] * e2; g[m].c[2] = ggF[0] * e0 + ggF[1] * e1 + ggF[2] * e2;
      g[m].l[0] = p->x; g[m].l[1] = p->y; g[m].l[2] = p->z; g[m].t[0] = g[m].t[1] = 0.f; g[m].k[0] = g[m].k[1] = g[m].k[2] = g[m].k[3] = 0.f; }
    if (svFanN < (int)(sizeof svFan / sizeof svFan[0]) && ggRisk(&g[0], &g[1], &g[2], &ggK)) {
      GgV o[GG_MAXV]; int no = ggCut(&g[0], &g[1], &g[2], &ggK, o); svCutTri++;
      if (no >= 3) { SvV *f = (SvV *)ggVbAlloc((unsigned)no * sizeof(SvV)); if (f) { for (int i = 0; i < no; i++) { f[i].c = v[t].c; f[i].x = o[i].l[0]; f[i].y = o[i].l[1]; f[i].z = o[i].l[2]; }
          svFan[svFanN].p = f; svFan[svFanN].n = no; svFanN++; svCutFans++; continue; } }
      if (no < 3) continue;   // в кадре ничего
    }
    if (w != t) { v[w] = v[t]; v[w + 1] = v[t + 1]; v[w + 2] = v[t + 2]; }
    w += 3;
  }
  return w;
}
// №973 (Роб 25.10: «тень машины Мардж на L4 сильно лагает, прыгает сквозь текстуру пола»; замер tests/shadow973, езда по холмам L4: до 9 кадров из 31 за секунду вершина тени на 0,2–0,5 м ниже опоры
// коллизии, и тут же на 0,6 м выше). Причина: тень машины (до 5,3 м длиной) — одна плоскость по опоре под ЦЕНТРОМ (нормаль и высота одной точки), на изломе склона, бордюре и холме углы уходят под землю
// или висят над ней, а высота центра скачет шагом (взлёт с трамплина, стык клеток). Теперь форма та же (восьмиугольник, у необычных машин силуэт), но высота — по опоре под 17 точками (центр, кольцо на
// половине, кольцо по краю; для силуэта — по 8 направлениям с промежуточными), вершина = опора + подъём 6 см (+ по крутизне до 7 см), тень состоит из центра и двух колец (24 треугольника вместо 8) и ложится
// на рельеф; рост высоты сразу, спад за 100 мс (как №522); точка без опоры (край, обрыв) гаснет по альфе, а не проваливается. Состояние — в кэше по машине (24 записи), ушёл дальше 4 м за кадр — с нуля.
// SHADOW973_OLD=1 / файл shadow973old / «flag shadow973old 1» — прежняя плоскость по центру
#ifndef SHADOW973_OLD
#define SHADOW973_OLD 0
#endif
static int sh973 = !SHADOW973_OLD;
static int s973DiagOn;   // flag shdiag973 1: замер SHADOW973 включён (без него в smoke-сборке тень считается как в выпуске: без таймеров и лишних запросов)
#define S973_N 5            // слоты опоры: 0 — центр, 1..4 — углы (своя машина) или 1, 2 — нос и корма (чужие)
#define S973_LIFT .06f
#define S973_EDGE .9f   // опору ищем в полосе вокруг плоскости по центру (берём самую верхнюю — ту, что видна); дальше полосы опоры нет: край тени гаснет
#define S973_SNAP .15f   // спад опоры глубже этого — тень сразу на землю, без 100 мс парения
#define S973_K .95f      // углы опоры — доля габарита контура
typedef struct { const void *key; unsigned frame, us; float x, z, fx, fz, nx, nz, H[S973_N], L[S973_N]; unsigned char ok[S973_N], have, flat, wv; int wnp, wown; float whw, whl, ww[32][4], wp[32]; } S973E;   // ww, wp — веса вершин контура (кэш: контур машины не меняется)
static S973E s973E[24]; static unsigned s973Frame, s973Samples, s973Meshes, s973Us;
static S973E *s973Get(const void *key) {
  S973E *w = &s973E[0]; for (int i = 0; i < 24; i++) { if (s973E[i].key == key) return &s973E[i]; if (s973E[i].frame < w->frame) w = &s973E[i]; }
  memset(w, 0, sizeof *w); w->key = key; return w;
}
// Тень = веер из центра по контуру (8 сторон или силуэт). Высота вершины — по опоре в 4 углах (своя машина; билинейно, центр поправкой «пирамидой») или в носу и корме (чужие; по длине плюс поперечный наклон
// нормали центра); запросов к коллизии: 4 у своей, 2 у чужой, у стоящей — ни одного. Точка без опоры гасит свою долю веера по альфе.
static unsigned slShMesh973(const SlSh *s, const float *px, const float *pz, int np, unsigned col, float kk, SvV *o) {
  const ColWorld *w = slShW; unsigned t0 = s973DiagOn ? sceKernelGetSystemTimeLow() : 0;
  float fl = sqrtf(s->fx * s->fx + s->fz * s->fz); if (fl < 1e-4f || !w) return 0;
  float fx = s->fx / fl, fz = s->fz / fl, rx = fz, rz = -fx;   // R = n × U для n = (0,1,0), как у плоской тени
  float hwS = 0.f, hlS = 0.f; for (int j = 0; j < np; j++) { hwS = fmaxf(hwS, fabsf(px[j])); hlS = fmaxf(hlS, fabsf(pz[j])); }
  if (hwS < .1f || hlS < .1f) return 0;
  int own = s->own, ns = own ? 4 : 2;
  S973E *e = s973Get(s->key); unsigned now = sceKernelGetSystemTimeLow();
  float dt = e->have ? (now - e->us) * 1e-6f : 0.f; if (dt < .001f) dt = .001f; if (dt > .1f) dt = .1f;
  float a = dt / (.1f + dt);
  if (e->have && (s973Frame - e->frame > 6 || fabsf(s->x - e->x) + fabsf(s->z - e->z) > 4.f)) e->have = 0;   // отстало или телепорт — без сглаживания
  float H[S973_N], L[S973_N]; unsigned char ok[S973_N]; float n1 = fmaxf(s->n[1], .5f);
  int still = e->have && fabsf(s->x - e->x) + fabsf(s->z - e->z) < .02f && fabsf(s->y - e->H[0]) < .01f && fabsf(fx - e->fx) + fabsf(fz - e->fz) < .01f;   // стоит: опора та же, запросов нет
  // ровное место: прошлый замер показал, что опора под всеми точками лежит в плоскости по центру (до 1,5 см) — следующие кадры (у своей один, у чужой до трёх) берут плоскость без запросов; нормаль ушла — замер
  int plane = !still && e->have && e->flat > 0 && fabsf(s->n[0] - e->nx) + fabsf(s->n[2] - e->nz) < .02f;
  if (still) { memcpy(H, e->H, sizeof H); memcpy(L, e->L, sizeof L); memcpy(ok, e->ok, sizeof ok); }
  else if (plane) { e->flat--;
    for (int i = 0; i < S973_N; i++) { float lx = own ? (i == 0 ? 0.f : ((i - 1) & 1 ? 1.f : -1.f) * S973_K * hwS) : 0.f, lz = own ? (i == 0 ? 0.f : ((i - 1) & 2 ? 1.f : -1.f) * S973_K * hlS) : (i == 0 ? 0.f : i == 1 ? 1.f : -1.f) * S973_K * hlS;
      H[i] = s->y + (-s->n[0] * (rx * lx + fx * lz) - s->n[2] * (rz * lx + fz * lz)) / n1; L[i] = e->L[i]; ok[i] = e->ok[i]; } }
  else {
    H[0] = s->y; L[0] = S973_LIFT; ok[0] = 1; if (e->have && e->ok[0] && s->y < e->H[0] && e->H[0] - s->y < S973_SNAP) H[0] = e->H[0] + a * (s->y - e->H[0]);
    for (int i = 1; i < S973_N; i++) {
      if (i > ns) { H[i] = H[0]; L[i] = S973_LIFT; ok[i] = 0; continue; }
      float lx = own ? ((i - 1) & 1 ? 1.f : -1.f) * S973_K * hwS : 0.f, lz = own ? ((i - 1) & 2 ? 1.f : -1.f) * S973_K * hlS : (i == 1 ? 1.f : -1.f) * S973_K * hlS;
      float X = s->x + rx * lx + fx * lz, Z = s->z + rz * lx + fz * lz, yp = s->y + (-s->n[0] * (X - s->x) - s->n[2] * (Z - s->z)) / n1, yv, nn[3];
      if (colGround(w, X, Z, yp + S973_EDGE, yp - S973_EDGE, &yv, nn)) {   // полоса вокруг плоскости по центру: ниже — обрыв, выше — стена, там тень гаснет
        float sl = sqrtf(fmaxf(0.f, 1.f - nn[1] * nn[1])) / fmaxf(nn[1], .5f); L[i] = S973_LIFT + fminf(.07f, .25f * sl); ok[i] = 1;
        H[i] = (e->have && e->ok[i] && yv < e->H[i] && e->H[i] - yv < S973_SNAP) ? e->H[i] + a * (yv - e->H[i]) : yv;   // мелкий спад сглаживаем, уступ глубже 15 см — сразу на землю
      } else { H[i] = yp; L[i] = S973_LIFT; ok[i] = 0; } }
    if (s973DiagOn) s973Samples += (unsigned)ns;
    { float dm = 0.f; int allok = 1; for (int i = 1; i <= ns; i++) { float lx = own ? ((i - 1) & 1 ? 1.f : -1.f) * S973_K * hwS : 0.f, lz = own ? ((i - 1) & 2 ? 1.f : -1.f) * S973_K * hlS : (i == 1 ? 1.f : -1.f) * S973_K * hlS;
        float yp = s->y + (-s->n[0] * (rx * lx + fx * lz) - s->n[2] * (rz * lx + fz * lz)) / n1; dm = fmaxf(dm, fabsf(H[i] - yp)); allok &= ok[i]; }
      e->flat = (allok && dm < .015f) ? (own ? 1 : 3) : 0; e->nx = s->n[0]; e->nz = s->n[2]; }
  }
  memcpy(e->H, H, sizeof H); memcpy(e->L, L, sizeof L); memcpy(e->ok, ok, sizeof ok); e->have = 1; e->frame = s973Frame; e->us = now; e->x = s->x; e->z = s->z; e->fx = fx; e->fz = fz;
  float Lm = 0.f; for (int i = 0; i <= ns; i++) Lm += L[i]; Lm /= (float)(ns + 1);
  float cross = (-s->n[0] * rx - s->n[2] * rz) / n1;
  SvV V[1 + 32]; if (np > 32) np = 32;
  V[0].c = col; { float P[3] = {s->x, H[0] + Lm, s->z}; V[0].x = P[0] + (eyeX - P[0]) * kk; V[0].y = P[1] + (eyeY - P[1]) * kk; V[0].z = P[2] + (eyeZ - P[2]) * kk; }
  if (!e->wv || e->wnp != np || e->wown != own || fabsf(e->whw - hwS) > .002f || fabsf(e->whl - hlS) > .002f) {   // веса вершин: от контура, один раз на машину
    for (int j = 0; j < np; j++) { float lx = px[j], lz = pz[j], t = lz / (S973_K * hlS);
      e->wp[j] = fmaxf(0.f, 1.f - fabsf(lx) / hwS) * fmaxf(0.f, 1.f - fabsf(lz) / hlS);
      if (own) { float u = lx / (S973_K * hwS); for (int k = 0; k < 4; k++) e->ww[j][k] = (((k & 1) ? 1.f + u : 1.f - u) * .5f) * (((k & 2) ? 1.f + t : 1.f - t) * .5f); }
      else { e->ww[j][0] = (1.f + t) * .5f; e->ww[j][1] = (1.f - t) * .5f; e->ww[j][2] = t > 0.f ? fminf(1.f, t) : 0.f; e->ww[j][3] = t < 0.f ? fminf(1.f, -t) : 0.f; } }
    e->wv = 1; e->wnp = np; e->wown = own; e->whw = hwS; e->whl = hlS; }
  int allok = 1; for (int i = 1; i <= ns; i++) allok &= ok[i];
  float mean = own ? (H[1] + H[2] + H[3] + H[4]) * .25f : (H[1] + H[2]) * .5f;
  for (int j = 0; j < np; j++) {
    const float *ww = e->ww[j]; float lx = px[j], lz = pz[j], h, f = 1.f;
    if (own) { h = ww[0] * H[1] + ww[1] * H[2] + ww[2] * H[3] + ww[3] * H[4]; if (!allok) for (int k = 0; k < 4; k++) if (!ok[1 + k]) f -= fmaxf(0.f, ww[k]); }
    else { h = ww[0] * H[1] + ww[1] * H[2] + cross * lx; if (!allok) f -= (ok[1] ? 0.f : ww[2]) + (ok[2] ? 0.f : ww[3]); }
    h += (H[0] - mean) * e->wp[j];
    if (f < 1.f) f = fmaxf(0.f, f);
    float P[3] = {s->x + rx * lx + fx * lz, h + Lm, s->z + rz * lx + fz * lz};
    SvV *v = &V[1 + j]; v->c = f < 1.f ? (col & 0xffffffu) | ((unsigned)((col >> 24) * f) << 24) : col;
    v->x = P[0] + (eyeX - P[0]) * kk; v->y = P[1] + (eyeY - P[1]) * kk; v->z = P[2] + (eyeZ - P[2]) * kk; }
  unsigned m = 0;
  for (int j = 0; j < np; j++) { int j2 = (j + 1) % np; o[m++] = V[0]; o[m++] = V[1 + j]; o[m++] = V[1 + j2]; }
  if (s973DiagOn) { s973Meshes++; s973Us += sceKernelGetSystemTimeLow() - t0; }
  return m;
}
// служебно (smoke): тень своей машины против опоры коллизии. Раз в секунду строка SHADOW973: кадров n, кадров с видимой вершиной тени ниже опоры + 5 см (sunk5) и ниже опоры (sunk0), худший зазор
// вершин minclr и наибольший maxclr (м; опора — colGround под вершиной: до 0,6 м выше и 1,2 м ниже, считаются вершины с альфой), средняя высота вершин тени: переключений знака шага больше 3 см (flip), наибольший шаг dcmax,
// us — мкс на сетку (кадр), samples — опорных запросов на кадр
static float s973Asym;   // несимметрия контура тени своей машины: для вершины — расстояние до ближайшей зеркальной (м), худшее
static unsigned s973N, s973Sunk5, s973Sunk0, s973Flip; static float s973Min = 9.f, s973Max = -9.f, s973Dc, s973Prev, s973PrevD; static int s973Have; static float s973T0 = -1.f;
static void sh973Diag(const SvV *v, unsigned n, float cy) {
  if (!slShW) return;
  int s5 = 0, s0 = 0; float ys = 0.f; int ny = 0; (void)cy;
  for (unsigned j = 0; j < n; j++) { if (!(v[j].c >> 24)) continue; ys += v[j].y; ny++;
    float g, nn[3]; if (!colGround(slShW, v[j].x, v[j].z, v[j].y + .6f, v[j].y - 1.2f, &g, nn)) continue;
    float c = v[j].y - g; if (c < s973Min) s973Min = c; if (c > s973Max) s973Max = c; if (c < .05f) s5 = 1; if (c < 0.f) s0 = 1; }
  s973N++; s973Sunk5 += s5; s973Sunk0 += s0;
  cy = ny ? ys / ny : cy;
  if (s973Have) { float d = cy - s973Prev; if (fabsf(d) > s973Dc) s973Dc = fabsf(d); if (fabsf(d) > .03f) { if (s973PrevD * d < 0.f) s973Flip++; s973PrevD = d; } }
  s973Prev = cy; s973Have = 1;
  if (s973T0 < 0.f) s973T0 = smkT;
  if (smkT - s973T0 >= 1.f) { char l_[260]; unsigned mm = s973Meshes ? s973Meshes : 1;
    snprintf(l_, sizeof l_, "SHADOW973 t=%.1f sc=%s n=%u sunk5=%u sunk0=%u minclr=%.3f maxclr=%.3f flip=%u dcmax=%.3f meshes=%u samples=%u us=%u asym=%.3f\n", smkT, smkScene, s973N, s973Sunk5, s973Sunk0, s973Min, s973Max, s973Flip, s973Dc, s973Meshes, s973Samples, s973Us / mm, s973Asym); smkWrite(l_);
    s973N = s973Sunk5 = s973Sunk0 = s973Flip = 0; s973Min = 9.f; s973Max = -9.f; s973Dc = 0.f; s973T0 = smkT; s973Meshes = s973Samples = s973Us = 0; }
}
static void slShFlushVec(void) {
  unsigned nv_ = 0; for (int i = 0; i < slShN; i++) nv_ += slSh[i].sil ? (unsigned)CS_N * 3u : slSh[i].box ? 8u * 3u : (unsigned)SV_N * 3u;   // машина 8 сторон (силуэт №811 — CS_N), эллипс SV_N
  unsigned by_ = nv_ * (unsigned)sizeof(SvV); if (ggVbUsed + by_ > GGVB_BYTES && !geRoomHard(by_)) { slShN = 0; return; }   // проверка 17.10: кольцо полно — из списка GE только при месте
  unsigned b0 = ggVbUsed, k = 0; SvV *q = (SvV *)ggVbAlloc(by_); if (!q) { slShN = 0; return; }
  for (int i = 0; i < slShN; i++) { const SlSh *s = &slSh[i]; unsigned k0_ = k;
    float dn = s->fx * s->n[0] + s->fz * s->n[2];
    float U[3] = {s->fx - s->n[0] * dn, -s->n[1] * dn, s->fz - s->n[2] * dn}, ul = sqrtf(U[0] * U[0] + U[1] * U[1] + U[2] * U[2]); if (ul < 1e-4f) continue;
    U[0] /= ul; U[1] /= ul; U[2] /= ul;
    float R[3] = {s->n[1] * U[2] - s->n[2] * U[1], s->n[2] * U[0] - s->n[0] * U[2], s->n[0] * U[1] - s->n[1] * U[0]};
    float C[3] = {s->x + s->n[0] * .04f, s->y + s->n[1] * .04f, s->z + s->n[2] * .04f};
    float dx = eyeX - C[0], dy = eyeY - C[1], dz = eyeZ - C[2], d = sqrtf(dx * dx + dy * dy + dz * dz), kk = d > 1e-3f ? fminf(SL_HT_PULL / d, SL_HT_PULLK) : 0.f;
    unsigned a = s->a * 2u > 255u ? 255u : s->a * 2u; a = a * s->tone / 255u; unsigned c = a << 24;
    if (!TOON_INK_OLD && skinToonEnabled()) { unsigned at = s->a * 2u > 255u ? 255u : s->a * 2u; at = at * TOON_SH_A / 255u; c = at << 24 | (skinInkAbgr(INK_SHADOW) & 0xffffffu); }   // №754: в мультике тень в тон туши, одна плотность у машин и людей   // плотность из списка теней (угасание у края дальности) × SV_A — один тон у всех (Роб 17.10: «пропорционально такие же у всех»)
    float px[CS_N > SV_N ? CS_N : SV_N], pz[CS_N > SV_N ? CS_N : SV_N]; int np;
    if (s->box && s->sil) {   // №811: машина необычной формы — многоугольник по силуэту сверху (доли габарита), то же сжатие, что у восьмиугольника
      float hw = s->hx * SL_DISK / slCarKX * SV_CAR_W, hl = s->hz * SL_DISK / slCarKZ * SV_CAR_L;
      for (int j = 0; j < CS_N; j++) { px[j] = s->sil[2 * j] * hw; pz[j] = s->sil[2 * j + 1] * hl; } np = CS_N;
    } else if (s->box) {   // машина: прямоугольник по габариту, углы срезаны
      float hw = s->hx * SL_DISK / slCarKX * SV_CAR_W, hl = s->hz * SL_DISK / slCarKZ * SV_CAR_L, cx_ = SV_CUT * fminf(hw, hl), cz_ = cx_ * SV_CUT_ZK;   // Роб 17.10: «капельку меньше»; срез по длине короче — боковые точки ближе к торцу, скос пологий
      float xs[8] = {-hw + cx_, hw - cx_, hw, hw, hw - cx_, -hw + cx_, -hw, -hw}, zs[8] = {-hl, -hl, -hl + cz_, hl - cz_, hl, hl, hl - cz_, -hl + cz_};
      for (int j = 0; j < 8; j++) { px[j] = xs[j]; pz[j] = zs[j]; } np = 8;
    } else {        // герой, прохожий: эллипс
      float r = s->hx * SL_DISK * svHk(), rz = s->hz * SL_DISK * svHk();   // Роб 17.10: «тень под Гомером меньше»
      if (!svCsOn) { for (int j = 0; j < SV_N; j++) { float th = 6.2831853f * j / SV_N; svCs[j][0] = cosf(th); svCs[j][1] = sinf(th); } svCsOn = 1; }
      for (int j = 0; j < SV_N; j++) { float k_ = s->rim ? slRim[j] : 1.f; px[j] = r * svCs[j][0] * k_; pz[j] = rz * svCs[j][1] * k_; } np = SV_N;
    }
    if (sh973 && s->box && s->key) { k += slShMesh973(s, px, pz, np, c, kk, &q[k]); }   // №973: форма по опоре под 17 точками
    else
    for (int j = 0; j < np; j++) { int j2 = (j + 1) % np; float lx[3] = {0.f, px[j], px[j2]}, lz[3] = {0.f, pz[j], pz[j2]};
      for (int m = 0; m < 3; m++) { float P[3] = {C[0] + R[0] * lx[m] + U[0] * lz[m], C[1] + R[1] * lx[m] + U[1] * lz[m], C[2] + R[2] * lx[m] + U[2] * lz[m]};
        SvV *o = &q[k++]; o->c = c; o->x = P[0] + (eyeX - P[0]) * kk; o->y = P[1] + (eyeY - P[1]) * kk; o->z = P[2] + (eyeZ - P[2]) * kk; } }
    if (s973DiagOn && s->own && s->box) { sh973Diag(&q[k0_], k - k0_, s->y);
      float w_ = 0.f; for (int j = 0; j < np; j++) { float m_ = 1e9f; for (int i2 = 0; i2 < np; i2++) { float dx_ = px[j] + px[i2], dz_ = pz[j] - pz[i2], d_ = sqrtf(dx_ * dx_ + dz_ * dz_); if (d_ < m_) m_ = d_; } if (m_ > w_) w_ = m_; } s973Asym = w_; }
    if (!SV_CUT853_OLD && !bigCutBlock && ggK.tvy > 0.f && k > k0_) { unsigned u0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      if (svNearRisk(&q[k0_], k - k0_)) k = k0_ + svCutTris(&q[k0_], k - k0_);   // №853: у камеры — срез на процессоре
      if (smkOn) svCutUs += sceKernelGetSystemTimeLow() - u0_; }
    if (diagOn) svDiag853(s, C, d, &q[k0_].c, k - k0_, sizeof(SvV));   // №853 служебно: что уходит в GE после среза — под риском, выброшено, улетевшие вершины
    slShQuads++;
  }
  if (k || svFanN) {
    ggVbWriteback(b0);
    ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE);
    if (k) { sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)k, 0, q); slShDraws++; }
    for (int i = 0; i < svFanN; i++) sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, svFan[i].n, 0, svFan[i].p);   // №853: срезанные у камеры
    sceGuEnable(GU_TEXTURE_2D); sceGuDepthMask(GU_FALSE); resetState();
  }
  svFanN = 0; slShN = 0;
}
static void slShFlush(void) {
  if (!slShN) return;
  if (slShVec) { slShFlushVec(); return; }   // векторная тень (Роб 17.10)
  if (slShHalf) { slShFlushHalf(); return; }
  unsigned bb0 = ggVbUsed; int drew = 0;
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE); sceGuDepthOffset(4);
  if (!slShFanOld) {
    // №680/№698: тень без текстуры — веер: центр и кольцо 0,63 радиуса с полной альфой, внешнее кольцо альфа 0 (вершинный градиент, краёв нет); радиус = SL_DISK × полуось
    sceGuDisable(GU_TEXTURE_2D);
    unsigned nv = (unsigned)slShN * SL_FAN_N * 9;
    SlFV *q = (SlFV *)ggVbAlloc(nv * sizeof(SlFV)); unsigned k = 0;
    for (int i = 0; i < slShN; i++) { const SlSh *s = &slSh[i];
      float dn = s->fx * s->n[0] + s->fz * s->n[2];
      float ux = s->fx - s->n[0] * dn, uy = -s->n[1] * dn, uz = s->fz - s->n[2] * dn, ul = sqrtf(ux * ux + uy * uy + uz * uz); if (ul < 1e-4f) continue; ux /= ul; uy /= ul; uz /= ul;
      float rx = s->n[1] * uz - s->n[2] * uy, ry = s->n[2] * ux - s->n[0] * uz, rz = s->n[0] * uy - s->n[1] * ux;
      float cx = s->x + s->n[0] * .05f, cy = s->y + s->n[1] * .05f, cz = s->z + s->n[2] * .05f;
      float ax = s->hx * SL_DISK, az = s->hz * SL_DISK; unsigned ca = ((unsigned)(s->a * .835f)) << 24;
      float P[2][SL_FAN_N + 1][3];
      for (int j = 0; j <= SL_FAN_N; j++) { float th = (float)(j % SL_FAN_N) * (6.2831853f / SL_FAN_N), cs = cosf(th), sn = sinf(th);
        for (int r = 0; r < 2; r++) { float f = r ? 1.f : .63f;
          P[r][j][0] = cx + (rx * ax * cs + ux * az * sn) * f; P[r][j][1] = cy + (ry * ax * cs + uy * az * sn) * f; P[r][j][2] = cz + (rz * ax * cs + uz * az * sn) * f; } }
      #define FV(a_, p_) do { q[k].c = (a_); q[k].x = (p_)[0]; q[k].y = (p_)[1]; q[k].z = (p_)[2]; k++; } while (0)
      float C[3] = {cx, cy, cz};
      for (int j = 0; j < SL_FAN_N; j++) {
        FV(ca, C); FV(ca, P[0][j]); FV(ca, P[0][j + 1]);
        FV(ca, P[0][j]); FV(0, P[1][j]); FV(ca, P[0][j + 1]);
        FV(ca, P[0][j + 1]); FV(0, P[1][j]); FV(0, P[1][j + 1]); }
      #undef FV
      slShQuads++; }
    if (k) { sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)k, 0, q); slShDraws++; drew = 1; }
    if (drew) ggVbWriteback(bb0);
    sceGuDepthOffset(0); sceGuDepthMask(GU_FALSE); resetState();
    slShN = 0; return;
  }
  if (!slTexReady) slTexInit();
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexMode(GU_PSM_8888, 0, 0, 0);
  static const unsigned char ord[6] = {0, 1, 2, 2, 1, 3};
  for (int t = 0; t < 2; t++) {
    int cnt = 0; for (int i = 0; i < slShN; i++) if (t == 0) cnt++;
    if (!cnt) continue;
    SlV *q = (SlV *)ggVbAlloc((unsigned)cnt * 6 * sizeof(SlV)); int k = 0;
    for (int i = 0; i < slShN; i++) { const SlSh *s = &slSh[i]; if (t != 0) continue;
      // оси на плоскости земли: вперёд = (fx, 0, fz) без составляющей по нормали, вбок = нормаль × вперёд
      float dn = s->fx * s->n[0] + 0.f * s->n[1] + s->fz * s->n[2];
      float ux = s->fx - s->n[0] * dn, uy = -s->n[1] * dn, uz = s->fz - s->n[2] * dn, ul = sqrtf(ux * ux + uy * uy + uz * uz); if (ul < 1e-4f) continue; ux /= ul; uy /= ul; uz /= ul;
      float rx = s->n[1] * uz - s->n[2] * uy, ry = s->n[2] * ux - s->n[0] * uz, rz = s->n[0] * uy - s->n[1] * ux;
      float cx = s->x + s->n[0] * .05f, cy = s->y + s->n[1] * .05f, cz = s->z + s->n[2] * .05f;
      float P[4][3]; for (int c = 0; c < 4; c++) { float sx = (c & 1) ? 1.f : -1.f, sz = (c & 2) ? 1.f : -1.f;
        P[c][0] = cx + rx * s->hx * sx + ux * s->hz * sz; P[c][1] = cy + ry * s->hx * sx + uy * s->hz * sz; P[c][2] = cz + rz * s->hx * sx + uz * s->hz * sz; }
      SlV *o = q + k * 6;
      for (int m = 0; m < 6; m++) { int c = ord[m]; o[m].u = (float)(c & 1); o[m].v = (float)((c >> 1) & 1); o[m].c = s->a << 24; o[m].x = P[c][0]; o[m].y = P[c][1]; o[m].z = P[c][2]; }
      k++; slShQuads++; }
    if (k) { sceGuTexImage(0, 64, 64, 64, slTex); sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, k * 6, 0, q); slShDraws++; drew = 1; }
  }
  if (drew) ggVbWriteback(bb0);
  sceGuDepthOffset(0); sceGuDepthMask(GU_FALSE); sceGuDisable(GU_TEXTURE_2D); sceGuTexScale(128.f, 128.f); resetState();
  slShN = 0;
}
// кадр: набрать тени всех, кто рядом, одной пачкой; ground: опора коллизии под центром (нормаль n), без проекции на геометрию
static int slGround(const ColWorld *w, float x, float z, float y, float *gy, float n[3]) {
  n[0] = 0.f; n[1] = 1.f; n[2] = 0.f;
  return colGround(w, x, z, y + 1.f, y - 30.f, gy, n);
}
static float slCarKX = 1.45f, slCarKZ = 1.35f, slCarA = SL_SH_DENS;   // полуоси овала в долях кузова и тёмная середина (0..255); служебно: flag carshkx/carshkz/carsha
// №811 (Роб 18.10: «у Барта машина как самолёт, а векторная тень под ней прямоугольник — для машин необычной формы своя векторная тень
// по силуэту сверху»): у своей машины уровня (куски CAR1 level.shr, вариант «целая») один раз считается силуэт сверху — самый дальний
// край модели в каждом из CS_N секторов вокруг центра габарита (в осях кузова). Машина «необычной формы» — край в ≥ CS_ODD_N секторах
// дальше CS_ODD_HI или ближе CS_ODD_LO от края габарита (крылья, узкий нос): тень по силуэту; обычные машины — восьмиугольник, как было
// (тень машин Роб принял). Считается на ровной земле, целая машина, все куски на месте; смена уровня — заново. CAR_SIL_OLD=1 — как было
#ifndef CAR_SIL_OLD
#define CAR_SIL_OLD 0
#endif
#define CS_ODD_HI 1.28f
#define CS_ODD_LO .72f
#define CS_ODD_N 4
#ifndef SIL973_FLOOR
#define SIL973_FLOOR .6f   // №973: радиус силуэта не меньше этой доли радиуса габарита по тому же лучу
#endif
static float csSil[2 * CS_N], csSilRaw[2 * CS_N]; static int csilState, csLevel = -1;   // №877: было csState — то же имя у ленты монет (coinspr.h) в одном main.c = ОДНА переменная: смена уровня перечитывала монеты (160 КБ утечки за раз), тень брала силуэт прошлой машины. csilState: 0 — не считан, 1 — необычная (тень по силуэту), -1 — обычная
static void csCompute(const Car *k, const CarBox *cb) {
  if (!haveCarVis || k->destroyed || ncarP <= 0) return;
  char b_[200];
  float R[CS_N]; for (int j = 0; j < CS_N; j++) R[j] = -1.f; int npc_ = 0, nv_ = 0;
  float fx = cb->fx, fz = cb->fz, fl = sqrtf(fx * fx + fz * fz); if (fl < 1e-4f) return; fx /= fl; fz /= fl;
  for (int i = 0; i < ncarP; i++) {
    const Piece *d = &pc[carP[i]]; if (cl[d->r.cell].state != 2) return;   // кусок ещё не прочитан — позже
    int joint = d->r.pad[0], variant = d->r.pad[1]; if (variant != 0 || carVisJointHidden(&cv, joint) || !d->v) continue;   // флаг 2 (смешивание) не пропускать: у кузова он стоит при непрозрачной текстуре (carGlass)
    npc_++; nv_ += d->r.nv;
    float m[16]; carVisPieceMatrix(&cv, k, joint, 0, d->r.ox, d->r.oy, d->r.oz, d->r.sc, m);
    int v12 = (d->r.flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8;
    for (int v = 0; v < d->r.nv; v++) { const short *q = (const short *)((const char *)d->v + v * vs + xo);
      float a = q[0] * (1.f / 32768.f), b = q[1] * (1.f / 32768.f), c = q[2] * (1.f / 32768.f);
      float X = m[0] * a + m[4] * b + m[8] * c + m[12], Z = m[2] * a + m[6] * b + m[10] * c + m[14];
      float dx = X - cb->x, dz = Z - cb->z, lz = dx * fx + dz * fz, lx = dx * fz - dz * fx, r = sqrtf(lx * lx + lz * lz);
      int s_ = (int)((atan2f(lx, lz) + 3.14159265f) * (CS_N / 6.2831853f)); if (s_ < 0) s_ = 0; if (s_ >= CS_N) s_ = CS_N - 1;
      if (r > R[s_]) R[s_] = r; } }
  int have = 0; for (int j = 0; j < CS_N; j++) have += R[j] > 0.f; if (have < CS_N / 2) return;
  for (int j = 0; j < CS_N; j++) if (R[j] <= 0.f) { int a = j, b = j; while (R[a = (a + CS_N - 1) % CS_N] <= 0.f) ; while (R[b = (b + 1) % CS_N] <= 0.f) ; R[j] = (R[a] + R[b]) * .5f; }
  int odd = 0; float qmin = 9.f, qmax = 0.f;
  for (int j = 0; j < CS_N; j++) { float th = ((j + .5f) * (6.2831853f / CS_N)) - 3.14159265f, sx = sinf(th), cz = cosf(th);
    float rb = fminf(fabsf(sx) > 1e-3f ? cb->hw / fabsf(sx) : 1e9f, fabsf(cz) > 1e-3f ? cb->hl / fabsf(cz) : 1e9f), q_ = R[j] / rb;
    if (q_ > CS_ODD_HI || q_ < CS_ODD_LO) odd++; if (q_ < qmin) qmin = q_; if (q_ > qmax) qmax = q_;
    csSilRaw[2 * j] = R[j] * sx / cb->hw; csSilRaw[2 * j + 1] = R[j] * cz / cb->hl;
    // №973: симметрия по продольной оси (максимум из зеркальных секторов) и нижний порог радиуса от габарита: убирает «огрызок» (машина Апу: сектор с радиусом 0,05 габарита), контур шаттла Барта остаётся
    float m_ = R[CS_N - 1 - j], r_ = fmaxf(R[j], m_), fl_ = SIL973_FLOOR * rb; if (r_ < fl_) r_ = fl_;
    csSil[2 * j] = r_ * sx / cb->hw; csSil[2 * j + 1] = r_ * cz / cb->hl; }
  csilState = odd >= CS_ODD_N ? 1 : -1;
  if (smkOn) { snprintf(b_, sizeof b_, "CARSIL level=%d car=%s odd=%d sectors=%d q=%.2f..%.2f pieces=%d verts=%d hw=%.2f hl=%.2f t=%.1f\n", gameLevel, k->p.name, csilState > 0, odd, qmin, qmax, npc_, nv_, cb->hw, cb->hl, smkT); smkWrite(b_); }
}
static void slShCar(const ColWorld *w, const Car *k, float ex, float ez) {
  if (!carShadow1003On) return;   // №1003: тени машин выключены строкой Car Shadows (по умолчанию) — ни запросов опоры, ни рисования
  if (k != &car && hypotf(k->x - ex, k->z - ez) >= SL_SH_R) return;
  CarBox cb; memset(&cb, 0, sizeof cb); carBox(k, &cb); float n[3], gy;
  if (!slGround(w, cb.x, cb.z, cb.y, &gy, n)) return;
  float fade = 1.f - fminf(1.f, fmaxf(0.f, (cb.y + cb.lo - gy) / 6.f)), jk = slPplOld ? 1.f : svJumpK(cb.y - gy - .1f);   // №749: пол колёс выше земли — тень меньше (на земле как была)
  if (!CAR_SIL_OLD && k == &car) { if (csLevel != gameLevel) { csLevel = gameLevel; csilState = 0; }
    if (!csilState && fabsf(cb.y + cb.lo - gy) < .3f) csCompute(k, &cb);   // на земле
    if (csilState > 0) slShSilNext = sh973 ? csSil : csSilRaw; }
  slShOwnNext = k == &car; slShKeyNext = k; slShW = w;
  slShAdd(cb.x, gy, cb.z, n, cb.fx, cb.fz, cb.hw * slCarKX / SL_DISK * jk, cb.hl * slCarKZ / SL_DISK * jk, slCarA * fade, 1);
  slShSilNext = 0; slShOwnNext = 0; slShKeyNext = 0;
}
// №680: тень машин (flag carshspr 1 / -DCAR_SH_SPR=1): мягкий овал по размерам кузова (CarBox) вместо прямоугольной сетки по SetShadowAdjustments; у PS2 тень машины — мягкое тёмное пятно, чуть шире кузова (кадры lv2_seat, honor_v). Рисуется и у трафика, машин миссий, оставленных.
#ifndef CAR_SH_SPR
#define CAR_SH_SPR 1
#endif
static int slCarShOn = CAR_SH_SPR, slOnlyCars;
// №698: размер пятна героя по ширине модели (пешком играет: L1,L7 Гомер, L2,L6 Барт, L3 Лиза, L4 Мардж, L5 Апу); у Гомера 1
static float slHeroK(void) { static const float k[8] = {1.f, 1.f, .8f, .72f, .95f, .85f, .8f, 1.f}; return gameLevel >= 1 && gameLevel <= 7 ? k[gameLevel] : 1.f; }
static int slOnlyHomer;   // homerspot без shadowspr: модуль рисует только пятно Гомера (машины и прохожие остаются на прежних пятнах)
// №749: край опоры под тенью героя — 8 лучей от центра тени по осям эллипса; земля на луче ниже опоры больше SV_EDGE_DROP (или нет земли) — край, тень до него
// (3 шага деления пополам); между лучами — меньшая доля соседей. Стоит на месте — прежний край (без запросов к коллизии)
static int svSolid(const ColWorld *w, float x, float z, float gy) { float y, n[3]; return colGround(w, x, z, gy + .4f, gy - 30.f, &y, n) && y > gy - SV_EDGE_DROP; }
static void svHeroRim(const ColWorld *w, float cx, float cz, float gy, float r, float hx, float hz) {   // hx, hz — место Гомера: ключ кэша (поворот камеры сдвигает центр тени — пробы не повторять)
  static float lx, lz, lgy, lr; static int ok;
  if (ok && fabsf(hx - lx) < .03f && fabsf(hz - lz) < .03f && fabsf(gy - lgy) < .02f && fabsf(r - lr) < .01f) return;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; lx = hx; lz = hz; lgy = gy; lr = r; ok = 1;
  float t8[8];
  for (int d = 0; d < 8; d++) { float ux = r * svCs[2 * d][0], uz = r * svCs[2 * d][1], lo = 0.f, hi = 1.f;   // вершина 2d эллипса: ось x тени = мир x, ось z = мир z (тень героя без поворота)
    if (svSolid(w, cx + ux, cz + uz, gy)) { t8[d] = 1.f; continue; }
    for (int it = 0; it < 3; it++) { float m = (lo + hi) * .5f; if (svSolid(w, cx + ux * m, cz + uz * m, gy)) lo = m; else hi = m; }
    t8[d] = lo; }
  for (int j = 0; j < SV_N; j++) slRim[j] = (j & 1) ? fminf(t8[j >> 1], t8[((j >> 1) + 1) & 7]) : t8[j >> 1];
  { float m_ = 1.f; for (int d = 0; d < 8; d++) m_ = fminf(m_, t8[d]); if (m_ < .99f) slRimCut++; }   // служебное: обрезка по краю была (SHADOWSPR rim_cut)
  if (smkOn) slRimUs += sceKernelGetSystemTimeLow() - t0;
}
static void svPerson(const ColWorld *w, float x, float y, float z, float ht, int ground) {   // эллипс человека: по росту; выше земли — меньше и бледнее
  float n[3] = {0.f, 1.f, 0.f}, gy = y;
  if (ground && !slGround(w, x, z, y, &gy, n)) return;
  float h = y - gy, fade = 1.f - fminf(1.f, fmaxf(0.f, h / 8.f)), r = SV_PPL_R * fminf(1.15f, fmaxf(.55f, ht / SV_PPL_H)) * svJumpK(h) / (SL_DISK * svHk());
  slPplN += (unsigned)slShAddT(x, gy, z, n, 0.f, 1.f, r, r, SL_HS_A * fade, 0, SV_A_PPL);
}
static void svPeople(const ColWorld *w, float ex, float ez) {
  float f_[8][4]; int nf_ = mvNpcFeet(f_, 8, ex, eyeY, ez, SV_NPC_R);   // персонажи миссий (Мардж, Апу…): те, что рисуются в этом кадре
  for (int i = 0; i < nf_; i++) svPerson(w, f_[i][0], f_[i][1], f_[i][2], f_[i][3], 1);
  if (haveTraffic && !inRoom()) for (int i = 0; i < traffic.maxPeds; i++) { const TrPed *p = &traffic.ped[i];   // прохожие: идёт по земле — его высота; летит или лежит — земля под ним
    if (p->mode == TRP_OFF || hypotf(p->x - ex, p->z - ez) >= SL_SH_PED_R) continue;
    svPerson(w, p->x, p->y, p->z, traffic.pedm ? traffic.pedm[p->model].height : SV_PPL_H, p->mode == TRP_FLY || p->mode == TRP_DOWN); }
  if (!haveProps || inRoom() || !props.st) return;
  // предметы: список ближних раз в 8 кадров (или глаз сдвинулся на 3 м); земля под спящим — один раз (кэш по экземпляру)
  static const PropState *cSt; static float *gyC; static unsigned gyN, cand[32], nc, tick; static float cx_, cz_;
  if (cSt != props.st || gyN != props.ninst) { free(gyC); gyC = (float *)malloc(props.ninst * sizeof(float)); gyN = gyC ? props.ninst : 0; cSt = props.st; nc = 0; tick = 0;
    for (unsigned i = 0; i < gyN; i++) gyC[i] = 1e30f; }
  if (!gyN) return;
  if (!(tick++ & 7) || hypotf(ex - cx_, ez - cz_) > 3.f) { cx_ = ex; cz_ = ez; nc = 0; float R2 = (SV_PROP_R + 4.f) * (SV_PROP_R + 4.f);
    for (unsigned i = 0; i < props.ninst && nc < 32; i++) { const PropState *st = &props.st[i]; float dx = st->x - ex, dz = st->z - ez; if (dx * dx + dz * dz < R2) cand[nc++] = i; } }
  unsigned pick[SV_PROP_MAX]; float pd[SV_PROP_MAX]; int np = 0;
  for (unsigned c = 0; c < nc; c++) { const PropState *st = &props.st[cand[c]]; if (st->hidden || st->fadeT > 0.f) continue;
    float dx = st->x - ex, dz = st->z - ez, d2 = dx * dx + dz * dz; if (d2 > SV_PROP_R * SV_PROP_R) continue;
    int k; if (np < SV_PROP_MAX) k = np++; else { if (d2 >= pd[SV_PROP_MAX - 1]) continue; k = SV_PROP_MAX - 1; }   // ближние SV_PROP_MAX (вставкой; полный список — вытесняем дальний)
    while (k > 0 && pd[k - 1] > d2) { pd[k] = pd[k - 1]; pick[k] = pick[k - 1]; k--; } pd[k] = d2; pick[k] = cand[c]; }
  for (int c = 0; c < np; c++) { unsigned i = pick[c]; if (i >= gyN || i >= props.ninst) continue; const PropState *st = &props.st[i];   // страховка проверки 17.10
    float qw = st->q[0], qx = st->q[1], qy = st->q[2], qz = st->q[3];   // оси коробки в мире (как propPieceMatrix): столбцы поворота × полуразмеры
    float A[3][3] = {{(1.f - 2.f * (qy * qy + qz * qz)) * st->hx, 2.f * (qx * qy + qw * qz) * st->hx, 2.f * (qx * qz - qw * qy) * st->hx},
                     {2.f * (qx * qy - qw * qz) * st->hy, (1.f - 2.f * (qx * qx + qz * qz)) * st->hy, 2.f * (qy * qz + qw * qx) * st->hy},
                     {2.f * (qx * qz + qw * qy) * st->hz, 2.f * (qy * qz - qw * qx) * st->hz, (1.f - 2.f * (qx * qx + qy * qy)) * st->hz}};
    int ax = (A[2][0] * A[2][0] + A[2][2] * A[2][2]) > .09f * st->hz * st->hz ? 2 : 0;   // «вперёд» тени — ось z модели по земле (лежит на боку — ось x)
    float ux = A[ax][0], uz = A[ax][2], ul = sqrtf(ux * ux + uz * uz); if (ul < 1e-4f) continue; ux /= ul; uz /= ul;
    float eU = 0.f, eR = 0.f, eY = 0.f;   // полуразмер проекции коробки на землю вдоль и поперёк, по высоте
    for (int k = 0; k < 3; k++) { eU += fabsf(A[k][0] * ux + A[k][2] * uz); eR += fabsf(A[k][0] * uz - A[k][2] * ux); eY += fabsf(A[k][1]); }
    if (fmaxf(eU, eR) < .08f || fmaxf(eU, eR) > 3.f || 2.f * eY > SV_PROP_HMAX || fmaxf(eU, eR) > SV_PROP_ASP * fminf(eU, eR)) continue;
    float n[3] = {0.f, 1.f, 0.f}, gy, bot = st->y - eY;
    if (!st->asleep) gyC[i] = 1e30f;   // проснулся (пнули) — запомненная земля устарела: заснёт в другом месте
    if (st->asleep && gyC[i] < 1e29f) gy = gyC[i];
    else { if (!slGround(w, st->x, st->z, bot, &gy, n)) { if (st->asleep) gyC[i] = -1e30f; continue; } if (st->asleep) gyC[i] = gy; }
    if (gy < -1e29f) continue;
    float h = bot - gy, jk = svJumpK(h), fade = 1.f - fminf(1.f, fmaxf(0.f, h / 8.f)), kk = jk / (SL_DISK * svHk());
    slPropN += (unsigned)slShAddT(st->x, gy, st->z, n, ux, uz, eR * kk, eU * kk, SL_HS_A * fade, 0, SV_A_PPL); }
}
static void slShadowFrame(const ColWorld *w, float ex, float ez) {
  if ((!slShadowOn && !slOnlyHomer && !slOnlyCars) || !w) return;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; SL_AC_T0;
  slShN = 0; float n[3], gy; s973Frame++;
  if (haveHomer && onFoot() && !slOnlyCars) {   // Гомер: круг 0,6 м
    static float gys; static int gysOn;
    if (slGround(w, homer.x, homer.z, homer.y, &gy, n)) { float gyRaw_ = gy;   // проверка 17.10: край сравнивать с настоящей землёй, не со сглаженной (на спуске сглаженная выше — ложный край)
      if (!gysOn || fabsf(gy - gys) > .5f) gys = gy; else gys += (gy - gys) * .3f; gysOn = 1; gy = gys;   // как у прежнего пятна №605: высота центра сглаживается
      float fade = 1.f - fminf(1.f, fmaxf(0.f, (homer.y - gy) / 8.f)), jk_ = slPplOld ? 1.f : svJumpK(homer.y - gy);   // №749: в прыжке тень меньше
      if (slHomerSpot) {
        float sx_, sz_; skinSunDirXZ(&sx_, &sz_); float sl_ = sqrtf(sx_ * sx_ + sz_ * sz_); if (sl_ < 1e-4f) { sx_ = SKIN_SUN_DIR_X; sz_ = SKIN_SUN_DIR_Z; sl_ = sqrtf(sx_ * sx_ + sz_ * sz_); }   // тень падает от солнца ТЕКУЩЕГО уровня (skinLight), против (x, z) направления на солнце
        float ho_ = slShVec && !slHeroCenterOld ? 0.f : SL_HS_OFF, nx_ = 0.f, nz_ = 0.f;   // Роб 17.10: «тень выглядит сдвинутой влево вверх — опусти ниже к нам и чуть вправо»
        if (slShVec && !slHeroCenterOld) { float bx_ = ex - homer.x, bz_ = ez - homer.z, bl_ = sqrtf(bx_ * bx_ + bz_ * bz_);   // по экрану: к камере SV_HERO_BK м, вправо SV_HERO_RT м
          if (bl_ > 1e-3f) { bx_ /= bl_; bz_ /= bl_; nx_ = bx_ * SV_HERO_BK + bz_ * SV_HERO_RT; nz_ = bz_ * SV_HERO_BK - bx_ * SV_HERO_RT; } }
        float hcx_ = homer.x - sx_ / sl_ * ho_ + nx_, hcz_ = homer.z - sz_ / sl_ * ho_ + nz_, hr_ = SL_HS_R * slHeroK() * jk_ / SL_DISK;
        if (slShAddT(hcx_, gy, hcz_, n, 0.f, 1.f, hr_, hr_, SL_HS_A * fade, 0, slPplOld ? SV_A : SV_A_PPL) && !slPplOld && slShVec) {   // №748: прозрачнее, как у всех людей
          if (!svCsOn) { for (int j = 0; j < SV_N; j++) { float th = 6.2831853f * j / SV_N; svCs[j][0] = cosf(th); svCs[j][1] = sinf(th); } svCsOn = 1; }
          svHeroRim(w, hcx_, hcz_, gyRaw_, hr_ * SL_DISK * svHk(), homer.x, homer.z); slSh[slShN - 1].rim = 1; }
      } else
      slShAdd(homer.x, gy, homer.z, n, 0.f, 1.f, .4f * slHeroK() / SL_DISK, .4f * slHeroK() / SL_DISK, SL_SH_DENS * fade, 0); } }
  if (!slOnlyCars && !slPplOld && slShVec) svPeople(w, ex, ez);   // №748: люди миссий, прохожие, предметы
  if (!slOnlyHomer && haveCarVis && !inRoom()) slShCar(w, &car, ex, ez);
  if (!slOnlyHomer && haveTraffic && !inRoom()) {
    for (int i = 0; i < traffic.maxCars; i++) if (traffic.car[i].mode != TRM_OFF) slShCar(w, &traffic.car[i].car, ex, ez);
    for (int i = 0; i < traffic.npark; i++) if (!traffic.park[i].gone) slShCar(w, &traffic.park[i].car, ex, ez);
    for (int i = 0; !slOnlyCars && (slPplOld || !slShVec) && i < traffic.maxPeds; i++) { const TrPed *p = &traffic.ped[i];
      if ((p->mode == TRP_WALK || p->mode == TRP_STAND || p->mode == TRP_GETUP || p->mode == TRP_DODGE) && hypotf(p->x - ex, p->z - ez) < SL_SH_PED_R) {
        n[0] = 0.f; n[1] = 1.f; n[2] = 0.f; slShAdd(p->x, p->y, p->z, n, 0.f, 1.f, .28f / SL_DISK, .28f / SL_DISK, 115.f, 0); } } }
  if (!slOnlyHomer) { const MsnCars *m = mvCars(); if (m && !inRoom()) for (int i = 0; i < m->n; i++) if (m->c[i].on) slShCar(w, &m->c[i].car, ex, ez); }
  { unsigned nsh_ = (unsigned)slShN; slShFlush(); slAcctAdd(nsh_, nsh_, sceKernelGetSystemTimeLow() - ac0_); }
  if (smkOn) { slShUs += sceKernelGetSystemTimeLow() - t0; slShFrames++; }
}

// ============================ СВЕТ (flag lightspr 1 / -DLIGHT_SPRITES=1) ============================
// Авторские световые куски города (BLMD со сложением, до NEON_GLOW_MAXTRI треугольников: конусы фонарей/ламп, световые пятна, неон) прежний путь не рисует (NEON_SKIP).
// Модуль при загрузке клетки снимает каждый такой кусок в ОДИН спрайт: вертикальный (конус, неон) — цилиндрический билборд от оси Y с градиентом цвета «верх/низ» из вершин;
// плоский (пятно) — квад на земле. Положение, размер, цвет и текстурные координаты берутся из самого меша (не угадываются: ориентация конуса сверху вниз сохраняется).
// Таблица живёт до смены уровня (свет всегда на месте, не подгружается по ходу езды), рисуется одной пачкой на текстуру со сложением, плавно по дальности (LSP_DIST_M/LSP_FADE_M).
// В кадре работы нет: расчёт — раз на кусок при загрузке клетки (до 2 клеток за кадр). Прежнее — NEON_GLOW_ON=1 / «flag neonglow 1» (при lightspr 1 не используется).
#ifndef LIGHT_SPRITES
#define LIGHT_SPRITES 0       // до тура выкл; «flag lightspr 1» включает без пересборки
#endif
#ifndef NEONSIGN950_OLD
#define NEONSIGN950_OLD 0
#endif
static int neonSign950 = !NEONSIGN950_OLD;
#define SL_L_MAX 3072
#define SL_UV_MAX 4.f         // размах текстурных координат меша не больше 4 повторов
#ifndef SL_POOLS
#define SL_POOLS 0
#endif
#define SL_POOL_MAX 6.f       // плоское пятно света не больше 6 м (пятна под фонарями 2–4 м)
#define SL_L_BUDGET 160       // квадов света в кадре
typedef struct { float p[3], X[3], Y[3], uv[4][2]; uint32_t cB, cT; int tex; uint8_t kind, pool; uint16_t pi1; } SlL;   // pi1 (№933): индекс пропа + 1 у свечения пропа (0 — не от пропа): сбитый или сдвинутый проп гасит своё свечение   // pool (№935): 0 — не искали, 1 — эллипс под конусом рисовать, 2 — мост/эстакада, без эллипса   // kind 0 — вертикальный (билборд от оси Y), 1 — плоский (оси X, Y заданы)
static unsigned slDbgAdd, slDbgMaxNi;
static SlL *slL; static unsigned slLn, slLcap; static const void *slLspInst, *slLpInst; static unsigned char *slLdone; static unsigned slLdoneN;
static int slLightInit = LIGHT_SPRITES; static unsigned slLQuads, slLDraws, slLFrames, slLUs, slLBuildUs, slLCells;
static int slLCmp(const void *a, const void *b) { int x = ((const SlL *)a)->tex, y = ((const SlL *)b)->tex; return x < y ? -1 : x > y; }
static void slLightReset(void) { free(slL); free(slLdone); slL = 0; slLdone = 0; slLspInst = 0; slLpInst = 0; slLn = slLcap = slLdoneN = 0; slLightResetReq = 0; }
static uint32_t slAvg(const uint32_t *c, int n) {
  if (!n) return 0; unsigned a = 0, b = 0, g = 0, r = 0; for (int i = 0; i < n; i++) { a += c[i] >> 24; b += (c[i] >> 16) & 255; g += (c[i] >> 8) & 255; r += c[i] & 255; }
  return ((a / n) << 24) | ((b / n) << 16) | ((g / n) << 8) | (r / n);
}
enum { SL_MAXV = 3 * NEON_GLOW_MAXTRI };
static void slLightVerts(float (*P)[3], float (*U)[2], const uint32_t *C, int n, int tex) {
  const int MAXV = SL_MAXV;
  float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f}, umn = 1e30f, umx = -1e30f, vmn = 1e30f, vmx = -1e30f;
  for (int i = 0; i < n; i++) { for (int k = 0; k < 3; k++) { if (P[i][k] < mn[k]) mn[k] = P[i][k]; if (P[i][k] > mx[k]) mx[k] = P[i][k]; }
    if (U[i][0] < umn) umn = U[i][0]; if (U[i][0] > umx) umx = U[i][0]; if (U[i][1] < vmn) vmn = U[i][1]; if (U[i][1] > vmx) vmx = U[i][1]; }
  float dx = mx[0] - mn[0], dy = mx[1] - mn[1], dz = mx[2] - mn[2], ext = fmaxf(dx, dz);
  if (ext < .05f && dy < .05f) return;
  if (umx - umn > SL_UV_MAX || vmx - vmn > SL_UV_MAX) return;   // плитка по меш-текстуре в десятки повторов (мерцание воды, узор): спрайтом не передать, остаётся без свечения
  if (dy < .3f * ext && (!SL_POOLS || ext > SL_POOL_MAX)) return;   // PS2: пятен света на земле нет (гараж АЭС, фонари L7)   // большие плоские аддитивные куски (вода, мерцание) — не свет: остаются как были (прежний путь, не нарисованы)
  // №950 (Роб 25.10, TEST59: «L6 причал C8-6: очень много вывесок со светом, долго прогружаются; свет вывескам не нужен, достаточно текстуры вывески, без свечения — на PSP не потянем»):
  // вертикальный невысокий аддитивный кусок (неон, табличка: высота не больше 1,5 ширины) — плашка свечения вывески; таблица её больше не строит (рисуется только своя текстура вывески,
  // куски города со сложением и так скрыты NEON_SKIP). Остаются конусы и лучи (вытянутые). NEONSIGN950_OLD=1 / «flag neonsign950 0» — плашки как были
  if (neonSign950 && dy * .5f <= 1.5f * ext * .5f) return;
  if (slLn >= SL_L_MAX) return;
  if (slLn >= slLcap) { unsigned nc = slLcap ? slLcap * 2 : 256; if (nc > SL_L_MAX) nc = SL_L_MAX; SlL *q = (SlL *)realloc(slL, nc * sizeof(SlL)); if (!q) return; slL = q; slLcap = nc; }
  SlL *e = &slL[slLn];
  memset(e, 0, sizeof *e); e->tex = tex;
  uint32_t lo[MAXV], hi[MAXV]; int nl = 0, nh = 0; float ym = (mn[1] + mx[1]) * .5f;
  for (int i = 0; i < n; i++) { if (P[i][1] < ym) lo[nl++] = C[i]; else hi[nh++] = C[i]; }
  uint32_t all = slAvg(C, n);
  if (dy < .3f * ext) {   // плоский: пятно на земле; текстурные координаты — линейная подгонка u,v по (x,z) (нормальные уравнения 3×3)
    e->kind = 1; e->p[0] = (mn[0] + mx[0]) * .5f; e->p[1] = mn[1] + dy * .5f; e->p[2] = (mn[2] + mx[2]) * .5f; e->X[0] = dx * .5f; e->Y[2] = dz * .5f;
    e->cB = e->cT = all;
    double s1 = n, sx = 0, sz = 0, sxx = 0, sxz = 0, szz = 0, su = 0, sxu = 0, szu = 0, sv = 0, sxv = 0, szv = 0;
    for (int i = 0; i < n; i++) { double x = P[i][0], z = P[i][2], u = U[i][0], v = U[i][1]; sx += x; sz += z; sxx += x * x; sxz += x * z; szz += z * z; su += u; sxu += x * u; szu += z * u; sv += v; sxv += x * v; szv += z * v; }
    double A[3][3] = {{s1, sx, sz}, {sx, sxx, sxz}, {sz, sxz, szz}};
    double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    float cu[3] = {0, 0, 0}, cv[3] = {0, 0, 0}; int ok = fabs(det) > 1e-6;
    if (ok) {
      double rhs[2][3] = {{su, sxu, szu}, {sv, sxv, szv}};
      for (int q = 0; q < 2; q++) { double sol[3];
        for (int c = 0; c < 3; c++) { double M[3][3]; memcpy(M, A, sizeof M); for (int rr = 0; rr < 3; rr++) M[rr][c] = rhs[q][rr];
          double dd = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]); sol[c] = dd / det; }
        for (int c = 0; c < 3; c++) (q ? cv : cu)[c] = (float)sol[c]; }
    }
    for (int c = 0; c < 4; c++) { float x = e->p[0] + ((c & 1) ? e->X[0] : -e->X[0]), z = e->p[2] + ((c & 2) ? e->Y[2] : -e->Y[2]);
      if (ok) { e->uv[c][0] = cu[0] + cu[1] * x + cu[2] * z; e->uv[c][1] = cv[0] + cv[1] * x + cv[2] * z; }
      else { e->uv[c][0] = (c & 1) ? umx : umn; e->uv[c][1] = (c & 2) ? vmx : vmn; } }
  } else {   // вертикальный: конус, луч, неон — цилиндрический билборд; v по высоте (подгонка v = b0 + b1·y), u по ширине меша
    e->kind = 0; e->p[0] = (mn[0] + mx[0]) * .5f; e->p[1] = ym; e->p[2] = (mn[2] + mx[2]) * .5f; e->Y[1] = dy * .5f; e->X[0] = ext * .5f;
    e->cB = nl ? slAvg(lo, nl) : all; e->cT = nh ? slAvg(hi, nh) : all;
    double s1 = n, sy = 0, syy = 0, sv = 0, syv = 0; for (int i = 0; i < n; i++) { double y = P[i][1], v = U[i][1]; sy += y; syy += y * y; sv += v; syv += y * v; }
    double den = s1 * syy - sy * sy, b1 = fabs(den) > 1e-9 ? (s1 * syv - sy * sv) / den : 0.0, b0 = (sv - b1 * sy) / s1;
    float vB = (float)(b0 + b1 * mn[1]), vT = (float)(b0 + b1 * mx[1]);
    if (fabs(den) <= 1e-9) { vB = vmn; vT = vmx; }
    e->uv[0][0] = umn; e->uv[0][1] = vB; e->uv[1][0] = umx; e->uv[1][1] = vB; e->uv[2][0] = umn; e->uv[2][1] = vT; e->uv[3][0] = umx; e->uv[3][1] = vT;
  }
  slLn++;
}
static void slLightPiece(const Piece *d) {   // кусок города: вершины short → мир (ox + sc/32768·raw)
  const Rec *r = &d->r; int v12 = (r->flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8, noidx = (r->flags & F_NOIDX) != 0, n = r->ni - r->ni % 3;
  if (n < 3 || !d->v || (!noidx && !d->idx) || r->sc <= 0.f) return;
  if (n > SL_MAXV) n = SL_MAXV;
  float P[SL_MAXV][3], U[SL_MAXV][2]; uint32_t C[SL_MAXV];
  for (int i = 0; i < n; i++) {
    unsigned vi = noidx ? (unsigned)i : d->idx[i]; const char *b = (const char *)d->v + vi * vs;
    const short *xy = (const short *)(b + xo); const short *uv = (const short *)b; float k = r->sc * (1.f / 32768.f);
    P[i][0] = r->ox + k * xy[0]; P[i][1] = r->oy + k * xy[1]; P[i][2] = r->oz + k * xy[2];
    U[i][0] = uv[0] * (1.f / 256.f); U[i][1] = uv[1] * (1.f / 256.f); C[i] = v12 ? 0xFFFFFFFFu : *(const uint32_t *)(b + 4); }
  slLightVerts(P, U, C, n, r->tex);
}
static void slLightSprops(void) {   // стейтпропы (фонари, лампы, гараж): аддитивные куски в позе экземпляра на момент снимка, матрица piece (raw/32768 → мир)
  for (unsigned i = 0; i < sprops.ninst; i++) {
    const SpType *T = &sprops.type[sprops.inst[i].type]; int any = 0;
    for (unsigned k = 0; k < T->nPiece; k++) { const SpPiece *pp = &sprops.piece[T->firstPiece + k]; if (pp->flags & F_ADDITIVE) { slDbgAdd++; if (pp->ni > slDbgMaxNi) slDbgMaxNi = pp->ni; } if ((pp->flags & F_ADDITIVE) && pp->ni >= 3 && pp->ni <= 3 * NEON_GLOW_MAXTRI) any = 1; }
    if (!any) continue;
    spPose(&sprops, i);
    for (unsigned k = 0; k < T->nPiece; k++) {
      const SpPiece *pp = &sprops.piece[T->firstPiece + k]; if (!(pp->flags & F_ADDITIVE) || pp->ni < 3 || pp->ni > 3 * NEON_GLOW_MAXTRI) continue;
      float m[16]; spPieceMatrix(&sprops, i, k, m);
      const Vert *vv = (const Vert *)((const char *)sprops.vertBase + pp->vOff); const unsigned short *ix = (const unsigned short *)((const char *)sprops.idxBase + pp->iOff);
      int n = (int)pp->ni - (int)pp->ni % 3; if (n > SL_MAXV) n = SL_MAXV;
      float P[SL_MAXV][3], U[SL_MAXV][2]; uint32_t C[SL_MAXV];
      for (int a = 0; a < n; a++) { const Vert *v = &vv[ix[a]]; float x = v->x * (1.f / 32768.f), y = v->y * (1.f / 32768.f), z = v->z * (1.f / 32768.f);
        P[a][0] = m[0] * x + m[4] * y + m[8] * z + m[12]; P[a][1] = m[1] * x + m[5] * y + m[9] * z + m[13]; P[a][2] = m[2] * x + m[6] * y + m[10] * z + m[14];
        U[a][0] = v->u * (1.f / 256.f); U[a][1] = v->v * (1.f / 256.f); C[a] = v->c; }
      slLightVerts(P, U, C, n, pp->tex);
    }
  }
}
// №933 (Роб: «все светы — векторные»): у ПРОПОВ (PRP1: фонари L7 tex 419, 74 экземпляра; L6 tex 27, 31) малые куски со сложением NEON_SKIP не рисует, а таблица света брала только стейтпропы и город —
// свечения у них не было вовсе. Теперь те же куски (≤ NEON_GLOW_MAXTRI треугольников, F_ADDITIVE) уходят в таблицу тем же slLightVerts в позе экземпляра на момент загрузки (фонарь стоит) и
// рисуются тем же одним вызовом slLightDrawVec. SLPROPS933_OLD=1 / «flag slprops933 0» / файл slprops933old — как было (без свечения)
#ifndef SLPROPS933_OLD
#define SLPROPS933_OLD 0
#endif
// slProps933: 0 — как было (без свечения пропов), 1 — плашка как у неона города, 2 — мягкий диск (векторный, к краю в ноль): у пропов свечение — круглое пятно у лица/лампы, ровная плашка закрывает предмет прямоугольником
static int slProps933 = SLPROPS933_OLD ? 0 : 2; static unsigned slPropN;
static void slLightProps(void) {
  if (!haveProps || !slProps933) return; unsigned pc0_ = 0, pe0_ = slLn; (void)pc0_;
  for (unsigned i = 0; i < props.ninst; i++) {
    const PropState *st = &props.st[i]; const PropType *ty = &props.types[props.def[i].type]; if (st->hidden) continue;
    for (unsigned k = 0; k < ty->nPiece; k++) {
      const PropPiece *pp = &props.pieces[ty->firstPiece + k];
      if (!(pp->flags & F_ADDITIVE) || pp->ni < 3 || pp->ni > 3 * NEON_GLOW_MAXTRI) continue;
      float m[16]; propPieceMatrix(st, pp->ox, pp->oy, pp->oz, pp->sc, m);
      const char *vb = (const char *)props.vertBase + pp->vOff; const unsigned short *ix = (const unsigned short *)((const char *)props.idxBase + pp->iOff);
      int v12 = (pp->flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8, noidx = (pp->flags & F_NOIDX) != 0;   // как drawGeo: 12-байтовые вершины без цвета, без индексов — подряд
      int n = (int)pp->ni - (int)pp->ni % 3; if (n > SL_MAXV) n = SL_MAXV;
      float P[SL_MAXV][3], U[SL_MAXV][2]; uint32_t C[SL_MAXV];
      for (int a = 0; a < n; a++) { unsigned vi = noidx ? (unsigned)a : ix[a]; if (vi >= pp->nv) { n = 0; break; }
        const char *b = vb + vi * vs; const short *uv = (const short *)b, *xyz = (const short *)(b + xo); float x = xyz[0] * (1.f / 32768.f), y = xyz[1] * (1.f / 32768.f), z = xyz[2] * (1.f / 32768.f);
        P[a][0] = m[0] * x + m[4] * y + m[8] * z + m[12]; P[a][1] = m[1] * x + m[5] * y + m[9] * z + m[13]; P[a][2] = m[2] * x + m[6] * y + m[10] * z + m[14];
        U[a][0] = uv[0] * (1.f / 256.f); U[a][1] = uv[1] * (1.f / 256.f); C[a] = v12 ? 0xFFFFFFFFu : *(const uint32_t *)(b + 4); }
      if (n < 3) continue;
      unsigned before = slLn; slLightVerts(P, U, C, n, pp->tex); slPropN += slLn - before; pc0_++;
      if (slLn > before && slProps933 >= 2 && slL[before].kind == 0 && slL[before].Y[1] * 2.f <= slL[before].X[0] * 2.f * 1.5f) slL[before].kind = 2;   // невысокий вертикальный (не конус/луч) — диск
      if (slLn > before && i < 65535u) slL[before].pi1 = (uint16_t)(i + 1);
    }
  }
  if (smkOn) { char l_[140]; snprintf(l_, sizeof l_, "SLPROPS pieces=%u entries=%u ninst=%u t=%.1f\n", pc0_, slLn - pe0_, props.ninst, smkT); smkWrite(l_); }
}
static void slLightBuild(void) {
  if (!cl || !ncell || !pc) return;
  if (slLdoneN != ncell) { free(slLdone); slLdone = (unsigned char *)calloc(ncell, 1); slLdoneN = slLdone ? ncell : 0; slLn = 0; slLspInst = 0; slLpInst = 0; }
  if (sprops.ninst && slLspInst != sprops.inst) { slLspInst = sprops.inst; slLightSprops(); slLCells++; }
  if (haveProps && props.ninst && slProps933 && slLpInst != props.st) { slLpInst = props.st; slLightProps(); slLCells++; }   // №933: свет пропов
  if (!slLdone) return;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0, built = 0, before = slLn;
  for (unsigned c = 0; c < ncell && built < 2; c++) {
    if (slLdone[c] || cl[c].state != 2) continue;
    slLdone[c] = 1; built++; slLCells++;
    for (unsigned k = cl[c].first; k < cl[c].first + cl[c].n && k < npc; k++) { const Piece *d = &pc[k];
      if ((d->r.flags & F_ADDITIVE) && d->r.owner == 0) { if (d->r.ni > slDbgMaxNi) slDbgMaxNi = d->r.ni; slDbgAdd++; }
      if ((d->r.flags & F_ADDITIVE) && d->r.owner == 0 && d->r.ni <= 3 * NEON_GLOW_MAXTRI && d->r.ni >= 3) slLightPiece(d);
      else if (smkOn && (d->r.flags & F_ADDITIVE) && d->r.owner == 0) {   // №805: служебно — аддитивный кусок крупнее предела (остаётся картинкой)
        char b[200]; snprintf(b, sizeof b, "SLBIG cell=%u ni=%d tex=%d bb=%.1f,%.1f,%.1f..%.1f,%.1f,%.1f\n", c, d->r.ni, d->r.tex, d->r.bb[0], d->r.bb[1], d->r.bb[2], d->r.bb[3], d->r.bb[4], d->r.bb[5]); smkWrite(b); } }
  }
  if (smkOn && slLn != before) for (unsigned k = before; k < slLn; k++) { const SlL *e = &slL[k]; char b[260]; snprintf(b, sizeof b, "SLENTRY sc=%s k=%u kind=%d tex=%d p=%.1f,%.1f,%.1f X=%.2f,%.2f,%.2f Y=%.2f,%.2f,%.2f uv0=%.2f,%.2f uv3=%.2f,%.2f cB=%08x cT=%08x\n", smkScene, k, e->kind, e->tex, e->p[0], e->p[1], e->p[2], e->X[0], e->X[1], e->X[2], e->Y[0], e->Y[1], e->Y[2], e->uv[0][0], e->uv[0][1], e->uv[3][0], e->uv[3][1], e->cB, e->cT); smkWrite(b); }
  if (slLn != before && slLn > 1) qsort(slL, slLn, sizeof(SlL), slLCmp);
  if (smkOn) slLBuildUs += sceKernelGetSystemTimeLow() - t0;
}
// Закон света Роба 17.10 («векторный 2D-конус к нам, книзу затухает, снизу эллипс — такой свет везде»): авторские световые куски тем же приёмом, что конусы
// таблицы (lightsprite.h vecLightDraw): вертикальный вытянутый (конус, луч) — трапеция ровного цвета, узко у верха, книзу в ноль, под ним эллипс; вертикальный
// невысокий (неон, табличка) — ровная полупрозрачная плашка; плоский (пятно) — эллипс. Без текстур, один вызов. При vecLight 0 — прежние картинки со сложением
static unsigned slPoolSkipN, slPoolDrawN;
#define SL_DISC_R 1.2f   // радиус диска свечения пропа — доля большей полустороны куска
#define SL_DISC_A 1.2f   // альфа в центре — доля VL_A
static void slResetPool935(void) { for (unsigned k = 0; k < slLn; k++) slL[k].pool = 0; }   // №935: флаг сценария lamppool935 меняет правило — решения по конусам заново   // №935: конусы таблицы клеток/стейтпропов: без эллипса (мост) и с эллипсом
static void slLightDrawVec_(float ex, float ey, float ez) {
  typedef struct { unsigned c; float x, y, z; } VV;
  unsigned b0 = ggVbUsed, nv = 0, cap = slLn * (12 + VL_POOL_N * 3); if (cap > 24000u) cap = 24000u;
  VV *q = (VV *)ggVbAlloc(cap * sizeof(VV)); if (!q) return;
  float e3[3] = {ex, ey, ez}; unsigned quads = 0;
  for (unsigned k = 0; k < slLn && nv + 12 + VL_POOL_N * 3 <= cap; k++) {
    const SlL *e = &slL[k];
    if (e->pi1 && slProps933 && (!haveProps || e->pi1 > props.ninst)) continue;
    if (e->pi1 && haveProps) { const PropState *ps_ = &props.st[e->pi1 - 1]; if (ps_->hidden || fabsf(ps_->x - ps_->home[0]) + fabsf(ps_->z - ps_->home[2]) > .6f || fabsf(ps_->y - ps_->home[1]) > 1.5f) continue; }   // №933: сбитый (спрятан, играет обломок) или сдвинутый проп — свечения нет
    float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz, f = lspFadeAt(d2); if (f <= 0.f) continue;
    float rad = sqrtf(e->X[0] * e->X[0] + e->Y[1] * e->Y[1] + e->Y[2] * e->Y[2]) * 1.42f + .5f; if (rfSphereOut(pl, e3, e->p, rad)) continue;
    uint32_t rgb = vlColor(e->kind != 1 ? e->cT : e->cB);
    if (e->kind == 1) {   // пятно на полу — эллипс
      unsigned c = ((unsigned)(VL_POOL_A * 1.4f * f + .5f) << 24) | rgb; float R0 = e->X[0], R1 = e->Y[2], kk = sqrtf(d2) > .3f ? VL_PULL / sqrtf(d2) : .5f; if (kk > .5f) kk = .5f;
      vlPoolTab(); for (int j = 0; j < VL_POOL_N; j++) {
        float P[3][3] = {{e->p[0], e->p[1] + .02f, e->p[2]}, {e->p[0] + R0 * vlPC_[j], e->p[1] + .02f, e->p[2] + R1 * vlPS_[j]}, {e->p[0] + R0 * vlPC_[j + 1], e->p[1] + .02f, e->p[2] + R1 * vlPS_[j + 1]}};
        for (int m = 0; m < 3; m++) { VV *o = &q[nv++]; o->c = c; o->x = P[m][0] + (ex - P[m][0]) * kk; o->y = P[m][1] + (ey - P[m][1]) * kk; o->z = P[m][2] + (ez - P[m][2]) * kk; } }
      quads++; continue; }
    if (e->kind == 2) {   // №933: свечение пропа (фонарь-тыква, лампа) — мягкий диск лицом к камере: в центре «немножко света», к краю в ноль; восемь секторов, 24 вершины
      float xl2 = sqrtf(dx * dx + dz * dz), Rx = xl2 > 1e-3f ? dz / xl2 : 1.f, Rz = xl2 > 1e-3f ? -dx / xl2 : 0.f, R = fmaxf(e->X[0], e->Y[1]) * SL_DISC_R;
      unsigned cC = ((unsigned)(VL_A * SL_DISC_A * f + .5f) << 24) | rgb, cE = rgb;
      vlPoolTab(); for (int j = 0; j < 8; j++) { int a0 = 2 * j, a1 = 2 * j + 2;
        float P3[3][3] = {{e->p[0], e->p[1], e->p[2]}, {e->p[0] + Rx * R * vlPC_[a0], e->p[1] + R * vlPS_[a0], e->p[2] + Rz * R * vlPC_[a0]}, {e->p[0] + Rx * R * vlPC_[a1], e->p[1] + R * vlPS_[a1], e->p[2] + Rz * R * vlPC_[a1]}};
        for (int m = 0; m < 3; m++) { VV *o = &q[nv++]; o->c = m ? cE : cC; o->x = P3[m][0]; o->y = P3[m][1]; o->z = P3[m][2]; } }
      quads++; continue; }
    // вертикальный: ось «вправо» лицом к камере вокруг вертикали
    float xl = sqrtf(dx * dx + dz * dz), Xx = xl > 1e-3f ? (dz / xl) * e->X[0] : e->X[0], Xz = xl > 1e-3f ? (-dx / xl) * e->X[0] : 0.f, hh = e->Y[1];
    int cone = hh * 2.f > e->X[0] * 2.f * 1.5f;   // вытянутый по высоте — конус/луч, иначе неон/табличка
    float top = e->p[1] + hh, bot = e->p[1] - hh, tw = cone ? VL_TOPW : 1.f;
    unsigned aT = (unsigned)(VL_A * (cone ? 1.f : .8f) * f + .5f), cT = (aT << 24) | rgb, cM = cone ? (((unsigned)(aT * VL_MID + .5f)) << 24) | rgb : cT, cB = cone ? rgb : cT;
    float P[6][3] = {{e->p[0] - Xx * tw, top, e->p[2] - Xz * tw}, {e->p[0] + Xx * tw, top, e->p[2] + Xz * tw},
                     {e->p[0] - Xx * (tw + 1.f) * .5f, e->p[1], e->p[2] - Xz * (tw + 1.f) * .5f}, {e->p[0] + Xx * (tw + 1.f) * .5f, e->p[1], e->p[2] + Xz * (tw + 1.f) * .5f},
                     {e->p[0] - Xx, bot, e->p[2] - Xz}, {e->p[0] + Xx, bot, e->p[2] + Xz}};
    static const unsigned char tri[12] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5};
    for (int m = 0; m < 12; m++) { int c_ = tri[m]; VV *o = &q[nv++]; o->c = c_ < 2 ? cT : c_ < 4 ? cM : cB; o->x = P[c_][0]; o->y = P[c_][1]; o->z = P[c_][2]; }
    if (cone) p990[3]++;   // №990: конус клетки нарисован
    if (cone && pool990) { quads++; continue; }   // №990: эллипса под конусом нет, конус остаётся
    if (cone && !((SlL *)e)->pool) {   // №935: тот же признак, что у ламп таблицы света (lamppool935.h): под полотном вторая поверхность глубже 3 м — эллипс не рисуется, конус остаётся
      float F_[3] = {e->p[0], bot, e->p[2]}, gy_, n_[3]; int sk_ = 0;
      if (haveCol && colGround(&world, e->p[0], e->p[2], bot + 1.f, bot - 12.f, &gy_, n_)) sk_ = lpPoolSkip(&world, F_, gy_);
      ((SlL *)e)->pool = (uint8_t)(sk_ ? 2 : 1); if (sk_) slPoolSkipN++; else slPoolDrawN++; }
    if (cone && e->pool != 2) {   // эллипс под конусом (пятно на полу)
      p990[1]++; p990tri[1] += VL_POOL_N; unsigned c = ((unsigned)(VL_POOL_A * f + .5f) << 24) | rgb; float R0 = e->X[0], kk = sqrtf(d2) > .3f ? VL_PULL / sqrtf(d2) : .5f; if (kk > .5f) kk = .5f;
      vlPoolTab(); for (int j = 0; j < VL_POOL_N; j++) {
        float Q[3][3] = {{e->p[0], bot + .02f, e->p[2]}, {e->p[0] + R0 * vlPC_[j], bot + .02f, e->p[2] + R0 * vlPS_[j]}, {e->p[0] + R0 * vlPC_[j + 1], bot + .02f, e->p[2] + R0 * vlPS_[j + 1]}};
        for (int m = 0; m < 3; m++) { VV *o = &q[nv++]; o->c = c; o->x = Q[m][0] + (ex - Q[m][0]) * kk; o->y = Q[m][1] + (ey - Q[m][1]) * kk; o->z = Q[m][2] + (ez - Q[m][2]) * kk; } } }
    quads++;
  }
  if (!nv) return;
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); setCull(0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState(); slLQuads += quads; slLDraws++;
}
static void slLightDrawVec(float ex, float ey, float ez) {   // №990: служебно — время и вызовы
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; slLightDrawVec_(ex, ey, ez);
  if (smkOn) { p990us[1] += sceKernelGetSystemTimeLow() - t0; p990c[1]++; }
}
static void slLightDraw(float ex, float ey, float ez, int inR) {
  if (slLightResetReq) slLightReset();
  if (!slLightOn || inR) return;
  if ((slLFrames & 3) == 0 || !slLn) slLightBuild();   // раз в 4 кадра ищем новые клетки
  if (vecLight) { unsigned t0v = smkOn ? sceKernelGetSystemTimeLow() : 0; slLFrames++; if (slLn) slLightDrawVec(ex, ey, ez); if (smkOn) slLUs += sceKernelGetSystemTimeLow() - t0v; return; }   // закон света: вектор
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0;
  slLFrames++;
  if (!slLn) return;
  typedef struct { float u, v; unsigned c; float x, y, z; } LV;
  unsigned bb0 = ggVbUsed; int open = 0, curTex = -2, bindTex = -1; unsigned nq = 0, quads = 0; LV *q = 0; float e3[3] = {ex, ey, ez};
  static const unsigned char ord[6] = {0, 1, 2, 2, 1, 3};
  for (unsigned k = 0; k < slLn; k++) {
    const SlL *e = &slL[k];
    float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
    float f = lspFadeAt(d2); if (f <= 0.f) continue;
    float rad = sqrtf(e->X[0] * e->X[0] + e->Y[1] * e->Y[1] + e->Y[2] * e->Y[2]) * 1.42f + .5f;
    if (rfSphereOut(pl, e3, e->p, rad)) continue;
    uint32_t cB = lspFadeCol(e->cB, f), cT = lspFadeCol(e->cT, f); if (!((cB | cT) >> 24)) continue;
    if (geOver(GE_SOFT) || quads >= SL_L_BUDGET) break;
    if (e->tex != curTex) {
      if (nq) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); slLDraws++; nq = 0; }
      curTex = e->tex; q = 0;
    }
    if (!open) {
      open = 1; ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
      sceGuSetMatrix(GU_MODEL, &I); sceGuTexScale(1.f, 1.f);
      sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); curFlags = -1; setCull(0);
      sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      if (!glowFogOld) sceGuDisable(GU_FOG);
    }
    if (bindTex != curTex) { bindTexLean(curTex); bindTex = curTex; sceGuTexWrap(GU_REPEAT, GU_REPEAT); }
    if (!q) { unsigned run = 0; for (unsigned j = k; j < slLn && slL[j].tex == curTex; j++) run++; q = (LV *)ggVbAlloc(run * 6 * sizeof(LV)); nq = 0; }
    float X[3], Y[3];
    if (e->kind == 1) { memcpy(X, e->X, 12); memcpy(Y, e->Y, 12); }
    else {   // цилиндрический билборд вокруг оси Y: X = ось × (глаз − центр)
      Y[0] = 0.f; Y[1] = e->Y[1]; Y[2] = 0.f; float xl = sqrtf(dx * dx + dz * dz); if (xl < 1e-3f) { X[0] = e->X[0]; X[1] = 0.f; X[2] = 0.f; }
      else { X[0] = (dz / xl) * e->X[0]; X[1] = 0.f; X[2] = (-dx / xl) * e->X[0]; } }
    float w[4][3]; for (int c = 0; c < 4; c++) { float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f; for (int a = 0; a < 3; a++) w[c][a] = e->p[a] + sx * X[a] + sy * Y[a]; }
    LV *o = q + nq * 6;
    for (int m = 0; m < 6; m++) { int c = ord[m]; o[m].u = e->uv[c][0]; o[m].v = e->uv[c][1]; o[m].c = (e->kind != 1 && (c & 2)) ? cT : cB; o[m].x = w[c][0]; o[m].y = w[c][1]; o[m].z = w[c][2]; }
    nq++; quads++;
    if (k + 1 >= slLn || slL[k + 1].tex != curTex) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); slLDraws++; nq = 0; q = 0; }
  }
  if (nq) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); slLDraws++; }
  if (open) { ggVbWriteback(bb0); sceGuTexScale(128.f, 128.f); if (!inR) sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); resetState(); }
  slLQuads += quads; if (smkOn) slLUs += sceKernelGetSystemTimeLow() - t0;
}
static unsigned slNVert(void) { unsigned n = 0; for (unsigned k = 0; k < slLn; k++) n += slL[k].kind != 1; return n; }
static void slStatLine(void) {   // из smkStat: строка на сцену
  if (!smkOn || (!slShFrames && !slLFrames)) { slShQuads = slShDraws = slShUs = slShFrames = 0; return; }
  char s[200]; double k = 1.0 / (slShFrames ? slShFrames : 1);
  if (slShFrames) snprintf(s, sizeof s, "SHADOWSPR scene=%s on=%d quads=%.1f draws=%.1f us=%.0f frames=%u ppl=%.1f props=%.1f rim_us=%.0f rim_cut=%u\n", smkScene, slShadowOn, slShQuads * k, slShDraws * k, slShUs * k, slShFrames, slPplN * k, slPropN * k, slRimUs * k, slRimCut);
  if (slShFrames) smkWrite(s); slShQuads = slShDraws = slShUs = slShFrames = 0; slPplN = slPropN = slRimUs = slRimCut = 0;
  { static int dumped = -1; if (dumped != gameLevel && lsp.n && lsp.n < 400 && smkOn) { dumped = gameLevel; for (unsigned k = 0; k < lsp.n; k++) { const LspE *e = &lsp.e[k]; char b[220]; snprintf(b, sizeof b, "LSPENTRY lv=%d k=%u mode=%d tex=%d p=%.1f,%.1f,%.1f hw=%.2f hh=%.2f abgr=%08x flags=%x inst=%u bill=%u type=%u\n", gameLevel, k, e->mode, e->tex, e->p[0], e->p[1], e->p[2], e->hw, e->hh, e->abgr, e->flags, e->inst, (unsigned)e->bill, (unsigned)sprops.inst[e->inst].type); smkWrite(b); } } }
  if (slLFrames && slLn && slLn < 40) for (unsigned k = 0; k < slLn; k++) { const SlL *e = &slL[k]; char b[260]; snprintf(b, sizeof b, "SLENTRY k=%u kind=%d tex=%d p=%.1f,%.1f,%.1f hw=%.2f hh=%.2f uv0=%.2f,%.2f uv3=%.2f,%.2f cB=%08x cT=%08x\n", k, e->kind, e->tex, e->p[0], e->p[1], e->p[2], e->X[0], e->Y[1], e->uv[0][0], e->uv[0][1], e->uv[3][0], e->uv[3][1], e->cB, e->cT); smkWrite(b); }
  if (slLFrames) { double k2 = 1.0 / slLFrames; snprintf(s, sizeof s, "LIGHTSPR scene=%s on=%d table=%u vert=%u cells=%u dbgadd=%u dbgmaxni=%u quads=%.1f draws=%.1f us=%.0f build_us=%u frames=%u\n", smkScene, slLightOn, slLn, slNVert(), slLCells, slDbgAdd, slDbgMaxNi, slLQuads * k2, slLDraws * k2, slLUs * k2, slLBuildUs, slLFrames);
    smkWrite(s); }
  slLQuads = slLDraws = slLUs = slLBuildUs = slLFrames = 0;
}

// ============================ №690: блеск под гаечным ключом (flag glowspr 1 / GLOW_SPRITE_OLD=1 — как до правки) ============================
// Роб: «у летающего ключа было свечение: ключ оставить 3D, звёздочку-блинг под ним сделать плоским спрайтом, как в Doom». У диска (art/wrench.p3d) звёздочка — группы квадов
// bluestarglow/glowsolid; таблица №599 их не содержит, в TEST30 у ключа свечения нет. Здесь: один квад-билборд (поворот вокруг вертикали к камере) на ключ, сложением, своя текстура 64×64
// (рисуется кодом: пятно + шипастая звезда, цвет голубовато-белый как bluestarglow), всегда на месте, мягко гаснет с 30 до 40 м, пульс по времени. Цена: ≤ 32 квада, одна пачка.
#ifndef GLOW_SPRITE_OLD
#define GLOW_SPRITE_OLD 0
#endif
#ifndef WRGLOW_R
#define WRGLOW_R 1.4f          // полуразмер квада, м
#endif
static int wrGlowOn = !GLOW_SPRITE_OLD; static unsigned wrGlowTex[64 * 64] __attribute__((aligned(16))); static int wrGlowReady; static unsigned wrGlowQuads, wrGlowFrames, wrGlowUs;
static void wrGlowInit(void) {
  for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
    float dx = (x - 31.5f) / 31.5f, dy = (y - 31.5f) / 31.5f, r = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
    float spike = .5f + .5f * cosf(a * 8.f);                       // 8 лучей
    float core = 1.f - r / .32f; if (core < 0.f) core = 0.f;       // яркое ядро
    float rr = .55f + .38f * spike, ring = 1.f - fabsf(r - rr * .62f) / .14f; if (ring < 0.f) ring = 0.f;   // шипастое кольцо
    float halo = 1.f - r / .9f; if (halo < 0.f) halo = 0.f; halo = halo * halo * .7f;
    float v = core * .8f + ring * .5f + halo * 1.1f; if (v > 1.f) v = 1.f;
    unsigned A = (unsigned)(v * 255.f + .5f);
    wrGlowTex[y * 64 + x] = (A << 24) | (150u << 16) | (225u << 8) | 255u;   // ABGR: тёплый белый с золотом
  }
  sceKernelDcacheWritebackRange(wrGlowTex, sizeof wrGlowTex); wrGlowReady = 1;
}
// №803 (Роб 18.10 после TEST34: «блинк-блинк по-прежнему не тот — нужен ровно как в оригинале»): у диска (art/wrench.p3d, составной рисунок wrench) под ключом
// три билборда сложением, все на 0,9 м дальше ключа от камеры (DIST -0.9): звезда-«царапина» scratch2 цвета (133,38,255) шириной 5,22↔5,72 м, цвет мигает
// полный → чёрный → полный каждую секунду (анимация BQG_wrenchShape46: 61 кадр, 30 к/с, ключи 0/15/30/45/60); кольцо glowsolid (72,91,255) 5,24↔5,74 м;
// малая звезда scratch2 (64,70,255) 2,09↔2,29 м. Картинки — из диска (tools/wrglow_export.py -> wrglow.bin рядом с EBOOT: серые на чёрном, яркость в RGB),
// числа — здесь. Нет файла — прежняя звезда кодом. WRGLOW_ORIG_OLD=1 — прежняя звезда (№690)
#ifndef WRGLOW_ORIG_OLD
#define WRGLOW_ORIG_OLD 0
#endif
#ifndef WRGLOW_CY
#define WRGLOW_CY 1.8f   // центр блика над точкой ключа, м (центр модели ключа; кадр TEST30)
#endif
static int wrgState; static unsigned *wrgTex[2]; static int wrgW[2], wrgH[2];   // 0 — не читали, 1 — картинки есть, -1 — нет (scratch2, glowsolid)
static void wrgLoad(void) {
  wrgState = -1; char fn[256]; sibling(fn, "wrglow.bin"); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fd < 0) return;
  unsigned char h[8]; int ok = sceIoRead(fd, h, 8) == 8 && !memcmp(h, "WRG1", 4) && (h[4] | h[5] << 8) == 2;
  for (int i = 0; ok && i < 2; i++) {
    unsigned short wh[2]; ok = sceIoRead(fd, wh, 4) == 4 && wh[0] >= 8 && wh[0] <= 128 && wh[1] >= 8 && wh[1] <= 128 && !(wh[0] & (wh[0] - 1)) && !(wh[1] & (wh[1] - 1));
    if (!ok) break;
    unsigned n = (unsigned)wh[0] * wh[1] * 4u; wrgTex[i] = (unsigned *)memalign(16, n);
    ok = wrgTex[i] && sceIoRead(fd, wrgTex[i], n) == (int)n; wrgW[i] = wh[0]; wrgH[i] = wh[1];
    if (ok) sceKernelDcacheWritebackRange(wrgTex[i], n);
  }
  sceIoClose(fd);
  if (ok) wrgState = 1; else for (int i = 0; i < 2; i++) { free(wrgTex[i]); wrgTex[i] = 0; }
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "WRGLOW orig=%d tex=%dx%d,%dx%d\n", wrgState, wrgW[0], wrgH[0], wrgW[1], wrgH[1]); smkWrite(s_); }
}
static void wrgDraw(unsigned tex, const void *q, unsigned n) {   // n квадов одной картинки сложением (1·src + 1·dst)
  sceGuTexImage(0, wrgW[tex], wrgH[tex], wrgW[tex], wrgTex[tex]); sceGuTexFlush();
  sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n * 6, 0, q);
}
// Уточнение Роба 18.10 (блик для ключа, знака миссии и трубки будки — один): асимметричная звезда (scratch2 диска: лучи разной длины, искажённые хвостики),
// ДВЕ наложены одна на другую и по-разному масштабируются во времени (свой период, фаза и размах; вторая повёрнута на 38°) — блик в воздухе; обе сильно
// прозрачные (сложением с ослабленным цветом). Один рисовальщик на всех: glintBegin / glintAdd (центр, размер, два цвета, прозрачность, сдвиг фазы) / glintFlush —
// одна пачка, одна картинка. У ключа — цвета оригинала (133,38,255) и голубой. Кольцо glowsolid диска — по флагу WRGLOW_RING=1
#ifndef WRGLOW_RING
#define WRGLOW_RING 0
#endif
#define GL_MAX 96
#ifndef GLINT_A
#define GLINT_A .7f   // «обе сильно прозрачные»: доля цвета звезды при сложении (картинка scratch2 сама тёмная: яркое только ядро и лучи)
#endif
typedef struct { float u, v; unsigned c; float x, y, z; } GlV;
static GlV *glQ; static unsigned glN, glCap, glBb0, glRingN; static GlV *glRing;
// №816 (Роб 18.10 вечер: «блик не увидел вообще — нужен заметный: шестиконечная бликовая звёздочка, две одна на другой, вторая повёрнута и другого
// размера, масштабируются не в фазе, сильно прозрачные, но видимые»): блик векторный, без картинки — звезда из шести лучей-бликов (узкие клинья от центра,
// лучи разной длины, с изломом-хвостиком) и мягкого ядра; цвет луча к концу гаснет до нуля. Вторая звезда — повёрнута на 30° и меньше (0,62), свой
// период и фаза пульса. Лицом к глазу (как квады AAX msnart), сложением с альфой — свет, как свечения оригинала (BLMD 2). Цвета лучей — оригинала
// (133,38,255) и (72,91,255), ядро почти белое. GLINT_VEC_OLD=1 — две звезды картинкой scratch2 (wrglow.bin)
#ifndef GLINT_VEC_OLD
#define GLINT_VEC_OLD 0
#endif
#ifndef GLINT_VA
#define GLINT_VA .55f   // альфа лучей у основания (к концам 0): «сильно прозрачные, но видимые»
#endif
typedef struct { unsigned c; float x, y, z; } GvV;   // 16 Б: цвет + точка
// №900 (Роб 23.10: «блик под гаечным ключом — мультяшный, без контура: рисованная асимметричная звёздочка, как снежинка, лучи в разные стороны;
// у лучей округлые основания и острые кончики, к центру сходятся в звёздочку; два векторных объекта чуть-чуть двигаются относительно друг друга
// („блинк-блинк“) — мягкость, как будто размыто, но без блюра»): луч — лепесток (основание — полукруг у центра, бока сходятся в острый кончик),
// ровная заливка без затухания к кончику; ядро — ровный светлый шестиугольник. Вторая звезда меньше, светлее, повёрнута и плавает по кругу на
// несколько сантиметров относительно первой. Блик общий (ключ, трубка будки, значки заданий, искры — слово Роба 18.10 «блик один»).
// GLINT_SNOW900_OLD=1 — прежние клинья с изломом (№816)
#ifndef GLINT_SNOW900_OLD
#define GLINT_SNOW900_OLD 0
#endif
#define GV_STAR (GLINT_SNOW900_OLD ? 72u : 108u)   // вершин звезды: прежняя 6 лучей × 3 треугольника + ядро 6; №900 — 6 лепестков × 5 + ядро 6
// №941 (Роб 24.10: «блики сделали как снежинки — переделать в ромбик, как бубна на картах, с доведением округлого угла к центру; контур не нужен; четырёхконечный,
// пятиконечный, асимметричный; внутренняя дуга округлая, внешняя точка острая»): общий шейп viewer/glintshape941.h вместо лепестков №900. Звезда A — четыре луча, B — пять
// (меньше, повёрнута), оба асимметричные; ровная заливка веером от центра, светлая сердцевина — тот же шейп .36, без контура. Вершин на звезду 72 / 90 (было 108).
// GLINT_SHAPE941_OLD=1 — лепестки №900
#include "glintshape941.h"
#define GV_STAR_MAX 108u
#define GV_SEG_RAY 4      // отрезков на сторону луча (отклонение дуги от хорды ≈ 1 % радиуса)
#define GV_SEG_CORE 2
#define GV_CORE_K .36f
static const GsDef *gvDef(int s) { return s ? &GS_GLINT_B : &GS_GLINT_A; }
static unsigned gvNs(int s) {   // вершин звезды s (0 — A, 1 — B)
  if (GLINT_SHAPE941_OLD) return GLINT_SNOW900_OLD ? 72u : 108u;
  return (unsigned)gvDef(s)->n * 3u * (GV_SEG_RAY + GV_SEG_CORE);
}
static struct { GvV *v; unsigned n; } gvB[32]; static unsigned gvNB, gvBb0;
static unsigned gvCol(unsigned rgb, float a) { if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f; return (unsigned)(a * 255.f + .5f) << 24 | (rgb & 0xFFFFFFu); }   // rgb = 0xBBGGRR
// цена (замер 18.10, эмулятор, сцена S_MSN с двумя значками): звезда с 36 синусами/косинусами на каждый кадр — около 70 мкс на значок, x_fx 1,00 -> 1,42 мс.
// Форма звёзд не меняется (в кадре — только размер, оси взгляда и цвет): точки звезды радиуса 1 считаются один раз (gvTpl), в кадре — умножение на радиус
// и оси. GV_PRE_OLD=1 — форма заново в каждом кадре
#ifndef GV_PRE_OLD
#define GV_PRE_OLD 0
#endif
typedef struct { GvV *o; const float *c, *R, *U; float rad; const unsigned *col; } GvEmit;   // куда и как класть точки звезды
static void gvShapeSnow(float rot, void (*emit)(void *u, float x, float y, int k), void *u) {   // №900: лепестки-лучи снежинки, k как у gvShape
  static const float len[6] = {1.f, .7f, .92f, .6f, .97f, .74f}, jit[6] = {0.f, .09f, -.07f, .12f, -.1f, .05f}, wid[6] = {.13f, .11f, .12f, .1f, .13f, .11f};
  for (int i = 0; i < 6; i++) {   // основание — полукруг радиуса w с центром на r0 от середины звезды (обращён к центру), кончик — точка на L
    float an = rot + (float)i * 1.0471976f + jit[i], dx = cosf(an), dy = sinf(an), px = -dy, py = dx, L = len[i], w = wid[i], r0 = .1f;
    float cx = dx * r0, cy = dy * r0, P[4][2];
    for (int k = 0; k < 4; k++) { float a = 1.5707963f + 1.0471976f * (float)k;   // 90°, 150°, 210°, 270° от направления луча: полукруг к центру
      float ca = cosf(a), sa = sinf(a); P[k][0] = cx + (dx * ca + px * sa) * w; P[k][1] = cy + (dy * ca + py * sa) * w; }
    for (int k = 0; k < 3; k++) { emit(u, cx, cy, 1); emit(u, P[k][0], P[k][1], 0); emit(u, P[k + 1][0], P[k + 1][1], 0); }
    emit(u, cx, cy, 1); emit(u, P[3][0], P[3][1], 0); emit(u, dx * L, dy * L, 2);   // бока к острому кончику
    emit(u, cx, cy, 1); emit(u, dx * L, dy * L, 2); emit(u, P[0][0], P[0][1], 0);
  }
  for (int i = 0; i < 6; i++) { float a0 = rot + (float)i * 1.0471976f + .5235988f, a1 = a0 + 1.0471976f;   // ядро: ровный шестиугольник 0,16
    emit(u, 0.f, 0.f, 3); emit(u, cosf(a0) * .16f, sinf(a0) * .16f, 4); emit(u, cosf(a1) * .16f, sinf(a1) * .16f, 4); }
}
static void gvShape941(int s, void (*emit)(void *u, float x, float y, int k), void *u) {   // №941: веер от центра по контуру общего шейпа; k 0 — луч, 3 — сердцевина
  float px[GS_MAXPTS], py[GS_MAXPTS]; const GsDef *d = gvDef(s);
  for (int pass = 0; pass < 2; pass++) {
    int seg = pass ? GV_SEG_CORE : GV_SEG_RAY, m = gsOutline(d, seg, pass ? GV_CORE_K : 1.f, px, py), k = pass ? 3 : 0;
    for (int i = 0; i < m; i++) { int j = (i + 1) % m; emit(u, 0.f, 0.f, k); emit(u, px[i], py[i], k); emit(u, px[j], py[j], k); }
  }
}
static void gvShape(float rot, void (*emit)(void *u, float x, float y, int k), void *u) {   // треугольники звезды радиуса 1; k — цвет точки:
  if (!GLINT_SNOW900_OLD) { gvShapeSnow(rot, emit, u); return; }
  static const float len[6] = {1.f, .64f, .88f, .58f, .95f, .72f}, jit[6] = {0.f, .06f, -.05f, .09f, -.08f, .04f}, bend[6] = {.05f, -.07f, .04f, -.05f, .08f, -.06f};   // 0 основание луча, 1 середина, 2 кончик, 3 центр ядра, 4 край ядра
  for (int i = 0; i < 6; i++) {   // луч: основание у центра (±w), середина со сдвигом поперёк (излом), кончик — хвостик дальше в ту же сторону
    float an = rot + (float)i * 1.0471976f + jit[i], dx = cosf(an), dy = sinf(an), px = -dy, py = dx, L = len[i], w = .06f, b = bend[i] * L;
    float bx = px * w, by = py * w, mx = dx * L * .5f + px * b, my = dy * L * .5f + py * b, tx = dx * L + px * b * 2.2f, ty = dy * L + py * b * 2.2f;
    float m1x = mx + px * w * .5f, m1y = my + py * w * .5f, m2x = mx - px * w * .5f, m2y = my - py * w * .5f;
    emit(u, bx, by, 0); emit(u, -bx, -by, 0); emit(u, m1x, m1y, 1);
    emit(u, -bx, -by, 0); emit(u, m2x, m2y, 1); emit(u, m1x, m1y, 1);
    emit(u, m1x, m1y, 1); emit(u, m2x, m2y, 1); emit(u, tx, ty, 2);
  }
  for (int i = 0; i < 6; i++) { float a0 = rot + (float)i * 1.0471976f + .5235988f, a1 = a0 + 1.0471976f;   // ядро: шестиугольник радиуса 0,15, к краю гаснет
    emit(u, 0.f, 0.f, 3); emit(u, cosf(a0) * .15f, sinf(a0) * .15f, 4); emit(u, cosf(a1) * .15f, sinf(a1) * .15f, 4); }
}
static void gvEmitWorld(void *u, float x, float y, int k) {   // точка звезды -> вершина в мире
  GvEmit *e = (GvEmit *)u; GvV *o = e->o++; x *= e->rad; y *= e->rad; o->c = e->col[k];
  o->x = e->c[0] + e->R[0] * x + e->U[0] * y; o->y = e->c[1] + e->R[1] * x + e->U[1] * y; o->z = e->c[2] + e->R[2] * x + e->U[2] * y;
}
static float gvTpl[2][GV_STAR_MAX][2]; static unsigned char gvTk[2][GV_STAR_MAX]; static unsigned gvTplN; static int gvTplOk;   // шаблон двух звёзд (повороты 0 и 30°)
static void gvEmitTpl(void *u, float x, float y, int k) {
  int s = *(const int *)u; if (gvTplN >= GV_STAR_MAX) return; gvTpl[s][gvTplN][0] = x; gvTpl[s][gvTplN][1] = y; gvTk[s][gvTplN] = (unsigned char)k; gvTplN++;
}
static float gvCoreK970 = 1.f;   // №970: доля светлой сердцевины звезды (0 у блика будки: два слоя — звезда A и звезда B, без третьего светлого ромба внутри)
static GvV *gvStar(GvV *o, const float *c, const float *R, const float *U, float rad, int s, unsigned rgb, unsigned core, float a) {   // s: 0 — звезда A, 1 — B (+30°)
  unsigned col[5] = {gvCol(rgb, a), gvCol(rgb, a * .45f), gvCol(rgb, 0.f), gvCol(core, a * .75f), gvCol(core, 0.f)};   // ядро — тоже прозрачное: сквозь просвет трубки не белое пятно
  if (!GLINT_SNOW900_OLD) { col[0] = col[1] = col[2] = gvCol(rgb, a); col[3] = col[4] = gvCol(core, a * .8f * gvCoreK970); }   // №900: ровная мультяшная заливка лучей и ядра
  if (!GLINT_SHAPE941_OLD) col[0] = col[1] = col[2] = gvCol(rgb, a);   // №941: рабочие цвета те же, шейп общий
  if (GV_PRE_OLD) { GvEmit e = {o, c, R, U, rad, col}; if (!GLINT_SHAPE941_OLD) gvShape941(s, gvEmitWorld, &e); else gvShape(s ? .5235988f : 0.f, gvEmitWorld, &e); return e.o; }
  if (!gvTplOk) { for (int k = 0; k < 2; k++) { gvTplN = 0; if (!GLINT_SHAPE941_OLD) gvShape941(k, gvEmitTpl, &k); else gvShape(k ? .5235988f : 0.f, gvEmitTpl, &k); } gvTplOk = 1; }
  for (unsigned i = 0, ns_ = gvNs(s); i < ns_; i++, o++) {
    float x = gvTpl[s][i][0] * rad, y = gvTpl[s][i][1] * rad; o->c = col[gvTk[s][i]];
    o->x = c[0] + R[0] * x + U[0] * y; o->y = c[1] + R[1] * x + U[1] * y; o->z = c[2] + R[2] * x + U[2] * y;
  }
  return o;
}
#ifndef GV_COST845_OLD
#define GV_COST845_OLD 0
#endif
#define GV_LOD_D 18.f    // №845: дальше — одна звезда блика
#define GV_AO_D 30.f     // №845: блики выключателей и звонков — ближе
#define GV_Q_D 40.f      // №845: блики очереди (искры, карточки, огни машин, заграждения) — ближе
static float gvSinTab[256]; static int gvSinOk;
static float gvSinT(float turns) {   // sin(2π · turns) по таблице 256 точек
  if (!gvSinOk) { for (int i = 0; i < 256; i++) gvSinTab[i] = sinf(6.2831853f * (float)i / 256.f); gvSinOk = 1; }
  float f = turns - floorf(turns); return gvSinTab[(int)(f * 256.f) & 255];
}
// №944 (служебно): перепись путей бликов взаимодействия. g944Note — первое появление (место, что, a, b) в журнале smoke строкой GLINT944 note; g944Bill — квады-билборды, что остались
// картинкой (вычитание — тени — не пишется); счётчики за секунду — в строке GLINT944 (glintStatTick)
static unsigned g944Cnt[8];   // 0 props_bill, 1 coll_bill, 2 bonus_bill (всё, что осталось картинкой, кроме вычитания)
static void g944Note(const char *site, const char *what, const float *p, int a, int b) {
  if (!smkOn) return;
  static struct { char k[56]; } seen[96]; static int ns; char key[56]; snprintf(key, sizeof key, "%s/%s/%d/%d", site, what, a, b);
  for (int i = 0; i < ns; i++) if (!strcmp(seen[i].k, key)) return;
  if (ns >= 96) return; snprintf(seen[ns++].k, sizeof seen[0].k, "%s", key);
  char l[220]; snprintf(l, sizeof l, "GLINT944 note site=%s what=%s a=%d b=%d p=%.1f,%.1f,%.1f\n", site, what, a, b, p ? p[0] : 0.f, p ? p[1] : 0.f, p ? p[2] : 0.f); smkWrite(l);
}
static void g944Bill(const char *site, int blm, int bt, const float *v, unsigned col, unsigned inst, unsigned b) {
  if (!smkOn || blm == 3) return;
  g944Cnt[site[0] == 'p' ? 0 : site[0] == 'c' ? 1 : 2]++;
  { float c[3] = {(v[0] + v[9]) * .5f, (v[1] + v[10]) * .5f, (v[2] + v[11]) * .5f}; char w[40]; snprintf(w, sizeof w, "i%u_b%u_t%d", inst, b, bt); g944Note(site, w, c, blm, (int)(col >> 24)); }
}
static unsigned gsAdd, gsAo, gsQ, gsFl, gsF0, gsUs, gsT0, gsFlUs;   // №845 (служебно): блики на кадр — строка GLINTSTAT раз в секунду
static void glintStatTick(void) {
  if (!smkOn) return; unsigned n = sceKernelGetSystemTimeLow(); if (!gsT0) { gsT0 = n; gsF0 = smkFrameAbs; }
  if (n - gsT0 < 1000000u) return; unsigned fr = smkFrameAbs - gsF0; if (!fr) fr = 1;
  char s_[200]; snprintf(s_, sizeof s_, "GLINTSTAT t=%.1f sc=%s fr=%u add=%.1f ao=%.1f q=%.1f flush=%.1f add_us=%.0f flush_us=%.0f vg=%.1f vg_v=%.0f vg_us=%.0f (на кадр)\n", smkT, smkScene, fr, (float)gsAdd / fr, (float)gsAo / fr, (float)gsQ / fr, (float)gsFl / fr, (float)gsUs / fr, (float)gsFlUs / fr, (float)vgStatN / fr, (float)vgStatV / fr, (float)vgStatUs / fr);
  smkWrite(s_);
  if (g944Cnt[0] | g944Cnt[1] | g944Cnt[2] | g944DoorOld | g944DoorNew) { char l_[200]; snprintf(l_, sizeof l_, "GLINT944 t=%.1f sc=%s fr=%u door_old=%u door_new=%u props_img=%u coll_img=%u bonus_img=%u (за секунду)\n", smkT, smkScene, fr, g944DoorOld, g944DoorNew, g944Cnt[0], g944Cnt[1], g944Cnt[2]); smkWrite(l_);
    g944DoorOld = g944DoorNew = 0; memset(g944Cnt, 0, sizeof g944Cnt); }
  gsAdd = gsAo = gsQ = gsFl = gsUs = gsFlUs = 0; vgStatN = vgStatUs = vgStatV = 0; gsT0 = n; gsF0 = smkFrameAbs;
}
// №970 (Роб 25.10, ключ: «меньше и прозрачнее все слои; пульсировать случайнее, но ближе к центру; крайние хороши, а наложенные друг на друга — с задержкой по фазе и мягким easing к центру»):
// ход двух слоёв ключа задан СКОРОСТЬЮ и фазой, не позицией. Размеры слоёв — три несоизмеримые частоты (37, 21 и 11 циклов за 60 с: «случайнее», шов при переходе часов в 60 с пропал —
// все частоты целые за 60 с); вторая звезда отстаёт по фазе на 0,28 с; её сдвиг — угол как интеграл скорости, радиус дышит между 25 % и 100 % от GM970_RMAX (0,035 размера, было 0,06)
// по smoothstep (ease-in-out) — к центру подходит и уходит плавно. Только ключ (gvMode970 ставит wrGlowDraw); остальные блики не тронуты. GLINT970_OLD=1 / flag glint970old 1 — как было
static float gm970Sin(float turns) { return gvSinT(turns); }
#define GM970_SIN gm970Sin
#include "glintmotion970.h"
static int gvMode970, gvOne970;   // gvOne970 — одна звезда (звёзды монет)
static int gvLog1022;   // №1022 служебно (flag glint1022log 1): строка на каждый блик ближе 12 м за кадр — центр, размер, пульс, источник
static const char *gvSrc1022 = "?";
// №1022 (Роб 26.10 ночь: «векторные блики и свечение на гаечном ключе и на телефоне жёстко глючат, даже в эмуляторе; количество не откатывать, доработать отображение»):
// сама звезда ходит плавно (журнал GV1022: скачок размера ≤ 1,1 % за кадр), но она большая (полуразмер ≈ 1,8 м у ключа) и стояла за предметом с проверкой глубины мира: низ звезды
// уходил в землю, бордюр и крышу будки, они резали её в рваный клин, который менялся с каждым поворотом камеры (прогон g1022ab, кадры wo/po). Звезда не пишет глубину, поэтому
// ключ и трубка (спрайты, рисуются после неё) ложатся сверху на любом её удалении. Теперь звезда перенесена по лучу от глаза ближе — к GF1022_FRONT м перед центром предмета — и
// уменьшена в то же число раз: на экране тот же центр, размер, форма, цвет и ход №970, а в мире она меньше и выше и до земли не достаёт. Перед ней (Гомер, стены) — закрывают честно.
// GLINTFIX1022_OLD=1 / файл glintfix1022old рядом с EBOOT / flag glintfix1022 0 — как в TEST71
#ifndef GLINTFIX1022_OLD
#define GLINTFIX1022_OLD 0
#endif
#define GF1022_FRONT .9f   // м перед центром предмета: столько же, сколько звезда стояла за ним (0,9); запас низа над землёй ≈ 0,5–0,7 м при глазе 5 м
static int glintFix1022 = !GLINTFIX1022_OLD;
// №1031 (Роб 26.10: «блики под гаечным ключом глючат именно издалека, вблизи пропадает»): звезда ключа стояла с проверкой глубины мира; издалека взгляд скользит над землёй (угол 5–10°),
// низ звезды (полуразмер 1,8 м, центр на 1,8 м над точкой ключа) ложится на землю и бордюр, глубина 16 бит рвёт край в полосы, и они мерцают с каждым шагом камеры. Теперь, как у трубки будки (№1022):
// центр ключа виден лучом из глаза — звезда целиком без проверки глубины (ключ-спрайт рисуется после и ложится сверху); не виден — звезды нет.
// GLINT1031_OLD=1 / файл glint1031old рядом с EBOOT / flag glint1031 0 — как после №1022
#ifndef GLINT1031_OLD
#define GLINT1031_OLD 0
#endif
static int glint1031 = !GLINT1031_OLD;
// трубка будки: звезда в 2,5 раза крупнее трубки и уходит в крышу будки при любом переносе — рисуется без проверки глубины, если центр трубки виден лучом из глаза
// (коллизия, кэш по месту на GF1022_RAYN кадров); не виден — звезды нет. Трубка (спрайт) рисуется после и ложится сверху
#define GF1022_RAYN 4
static struct { float key[3]; unsigned frame; int vis; } gf1022V[16];
static int gf1022Visible(const float c[3], float ex, float ey, float ez) {
  int k = -1, old = 0; for (int i = 0; i < 16; i++) { if (fabsf(gf1022V[i].key[0] - c[0]) + fabsf(gf1022V[i].key[2] - c[2]) < .5f) { k = i; break; } if (gf1022V[i].frame < gf1022V[old].frame) old = i; }
  if (k >= 0 && smkFrameAbs - gf1022V[k].frame < GF1022_RAYN) return gf1022V[k].vis;
  if (k < 0) k = old;
  float a[3] = {ex, ey, ez}, b[3] = {c[0], c[1], c[2]}, d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}, l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (l > .8f) { b[0] += d[0] / l * .6f; b[1] += d[1] / l * .6f; b[2] += d[2] / l * .6f; }   // конец луча на 0,6 м к глазу: не цепляет саму будку
  int vis = l <= .8f || !haveCol || colRayFrac(&world, a, b, 0) >= .999f;
  memcpy(gf1022V[k].key, c, 12); gf1022V[k].frame = smkFrameAbs; gf1022V[k].vis = vis; return vis;
}
static float gf1022Pull(float *c, const float obj[3], float ex, float ey, float ez) {   // c — центр звезды (меняется); obj — центр предмета; вернуть множитель размера
  if (!glintFix1022) return 1.f;
  float dc[3] = {c[0] - ex, c[1] - ey, c[2] - ez}, lc = sqrtf(dc[0] * dc[0] + dc[1] * dc[1] + dc[2] * dc[2]);
  float dobj = sqrtf((obj[0] - ex) * (obj[0] - ex) + (obj[1] - ey) * (obj[1] - ey) + (obj[2] - ez) * (obj[2] - ez)) - GF1022_FRONT;
  if (lc < .5f || dobj < .3f) return 1.f;
  float k = dobj / lc; if (k > 1.f) k = 1.f; if (k < .15f) k = .15f;
  c[0] = ex + dc[0] * k; c[1] = ey + dc[1] * k; c[2] = ez + dc[2] * k; return k;
}
static void gvAdd(const float c[3], float ex, float ey, float ez, float scale, unsigned rgb1, unsigned rgb2, float alpha, float phase) {
  if (gvNB >= 32) return;
  if (smkOn) { static int n944_, lv944_ = -1; static int16_t k944_[96][3]; if (lv944_ != gameLevel) { lv944_ = gameLevel; n944_ = 0; } if (n944_ < 96) { int16_t q_[3] = {(int16_t)lroundf(c[0]), (int16_t)lroundf(c[1]), (int16_t)lroundf(c[2])}; int j_ = 0; for (; j_ < n944_; j_++) if (!memcmp(k944_[j_], q_, sizeof q_)) break;   // №957б служебно: где в мире стоят блики (первое появление на 1 м сетке)
      if (j_ == n944_) { memcpy(k944_[n944_++], q_, sizeof q_); char l_[160]; snprintf(l_, sizeof l_, "GV944 lv=%d sc=%s p=%.1f,%.1f,%.1f scale=%.2f rgb=%06x,%06x a=%.2f\n", gameLevel, smkScene, c[0], c[1], c[2], scale, rgb1 & 0xffffffu, rgb2 & 0xffffffu, alpha); smkWrite(l_); } } }
  unsigned gs0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; gsAdd++;
  float dx = c[0] - ex, dy = c[1] - ey, dz = c[2] - ez, l = sqrtf(dx * dx + dz * dz), dl = sqrtf(dx * dx + dy * dy + dz * dz); if (l < 1e-3f) return;
  // №845 (тур TEST38 на PSP: блики — 1,5–3 мс кадра L2/L3, на блик 144 вершины считает процессор): блик вне кадра не считается; дальше GV_LOD_D —
  // одна звезда (большая); пульс — по таблице синуса (sinf на PSP медленный). GV_COST845_OLD=1 — как было
  if (!GV_COST845_OLD) { float e3_[3] = {ex, ey, ez}; if (!bonusTrack && rfSphereOut(pl, e3_, c, .6f * scale + .3f)) { if (smkOn) gsUs += sceKernelGetSystemTimeLow() - gs0_; return; } }
  int two_ = (GV_COST845_OLD || dl < GV_LOD_D) && !gvOne970;
  GvV *v = (GvV *)ggVbAlloc((gvNs(0) + (two_ ? gvNs(1) : 0u)) * sizeof(GvV));
  const float R[3] = {-dz / l, 0.f, dx / l}, U[3] = {-dy / dl * dx / l, l / dl, -dy / dl * dz / l};
  float tm = (float)(sceKernelGetSystemTimeLow() % 60000000u) * 1e-6f + phase;
  float kA, kB;
  if (GV_COST845_OLD) { kA = 1.f + .14f * sinf(6.2831853f * tm / 1.3f); kB = 1.f + .3f * sinf(6.2831853f * tm / .85f + 2.1f); }   // пульс двух звёзд не в фазе
  else { kA = 1.f + .14f * gvSinT(tm / 1.3f); kB = 1.f + .3f * gvSinT(tm / .85f + .3342254f); }   // 2,1 рад = 0,334 оборота
  if (!GV_COST845_OLD && !GLINT_SNOW900_OLD) { kA = 1.f + .06f * gvSinT(tm / 1.3f); kB = 1.f + .12f * gvSinT(tm / .85f + .3342254f); }   // №900: пульс мягче — «блинк-блинк» сдвигом
  float wx_ = 0.f, wy_ = 0.f;   // №970: ключ — свой ход слоёв (glintmotion970.h)
  if (gvMode970 && !fxGlint970Old) { gm970(tm, &kA, &kB, &wx_, &wy_); wx_ *= scale; wy_ *= scale; }
  if (gvLog1022 && smkOn && dl < 12.f) { char l_[200]; snprintf(l_, sizeof l_, "GV1022 f=%u src=%s m970=%d c=%.3f,%.3f,%.3f sc=%.3f kA=%.3f kB=%.3f w=%.4f,%.4f a=%.3f ph=%.2f tm=%.3f\n", smkFrameAbs, gvSrc1022, gvMode970, c[0], c[1], c[2], scale, kA, kB, wx_, wy_, alpha, phase, tm); smkWrite(l_); }
  GvV *o = gvStar(v, c, R, U, .5f * scale * kA, 0, rgb1, 0xFFF0E0u, alpha);
  if (two_ && gvMode970 && !fxGlint970Old) {
    float cB[3] = {c[0] + R[0] * wx_ + U[0] * wy_, c[1] + R[1] * wx_ + U[1] * wy_, c[2] + R[2] * wx_ + U[2] * wy_};
    unsigned l2 = ((((rgb2 >> 16) & 255u) + 255u) / 2u) << 16 | ((((rgb2 >> 8) & 255u) + 255u) / 2u) << 8 | (((rgb2 & 255u) + 255u) / 2u);
    gvStar(o, cB, R, U, .5f * scale * .62f * kB, 1, l2, 0xFFF0E0u, alpha * .9f);
  } else
  if (two_ && !GLINT_SNOW900_OLD) {   // №900: вторая звезда плавает по кругу на 6 % размера относительно первой, светлее (цвет к белому)
    float ox = .06f * scale * gvSinT(tm / 1.1f), oy = .05f * scale * gvSinT(tm / 1.1f + .25f), cB[3] = {c[0] + R[0] * ox + U[0] * oy, c[1] + R[1] * ox + U[1] * oy, c[2] + R[2] * ox + U[2] * oy};
    unsigned l2 = ((((rgb2 >> 16) & 255u) + 255u) / 2u) << 16 | ((((rgb2 >> 8) & 255u) + 255u) / 2u) << 8 | (((rgb2 & 255u) + 255u) / 2u);
    gvStar(o, cB, R, U, .5f * scale * .62f * kB, 1, l2, 0xFFF0E0u, alpha * .9f);
  } else
  if (two_) gvStar(o, c, R, U, .5f * scale * .62f * kB, 1, rgb2, 0xFFF0E0u, alpha * .9f);
  gvB[gvNB].v = v; gvB[gvNB].n = gvNs(0) + (two_ ? gvNs(1) : 0u); gvNB++;
  if (smkOn) gsUs += sceKernelGetSystemTimeLow() - gs0_;
}
static void gvFlush(void) {
  glintStatTick();
  if (!gvNB) return;
  unsigned gf0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; gsFl++;
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
  sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_FOG);
  ggVbWriteback(gvBb0);   // вершины в память до вызовов: GE читает их из RAM
  for (unsigned i = 0; i < gvNB; ) { unsigned n = gvB[i].n, j = i + 1;   // №839: блики, лежащие в памяти подряд, — одним вызовом (искры монет, столб подбора)
    while (j < gvNB && gvB[j].v == gvB[i].v + n) { n += gvB[j].n; j++; }
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, gvB[i].v); i = j; }
  sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDepthMask(GU_FALSE); resetState();
  gvNB = 0;
  if (smkOn) gsFlUs += sceKernelGetSystemTimeLow() - gf0_;
}
// №941 (служебно, flag glintlab 1): лист форм на чёрном — A, B (путь блика spritelight), звезда стекла (kind 3) и звезда частиц (kind 1) в ряд, прямо перед камерой, один размер (радиус 48 пикс);
// гейт tests/glint941 меряет растр снимка (доля тела, острота кончиков, число лучей). При GLINT_SHAPE941_OLD — прежние формы тем же путём
static int glintLab941;
static void glintLabDraw(float ex, float ey, float ez, float fx, float fy, float fz, float fovDeg) {
  float fl = sqrtf(fx * fx + fz * fz); if (smkOn) { static int n_; if (n_++ < 3) { char l_[160]; snprintf(l_, sizeof l_, "GLINTLAB e=%.1f,%.1f,%.1f f=%.3f,%.3f,%.3f fov=%.1f\n", ex, ey, ez, fx, fy, fz, fovDeg); smkWrite(l_); } }
  if (fl < 1e-4f) return;
  float f[3] = {fx, fy, fz}, R[3] = {-fz / fl, 0.f, fx / fl}, U[3] = {-fy * fx / fl, fl, -fy * fz / fl};
  float e3_[3] = {ex, ey, ez}, D = 10.f, ppm = ((float)SCR_H * .5f) / tanf(fovDeg * .5f * 3.14159265f / 180.f) / D;   // пикселей на метр на расстоянии D
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  if (glintLab941 == 2) {   // №944: звезда входа (msnart.c) тем же путём, что у двери в игре; центр экрана на 20 пикс выше середины, полуось квада 38,4 пикс (кончик большого луча 1,25 от неё = 48)
    float c[3], rad = 38.4f / ppm, X[3], Y[3]; for (int j = 0; j < 3; j++) { c[j] = e3_[j] + f[j] * D + U[j] * (20.f / ppm); X[j] = R[j] * rad; Y[j] = U[j] * rad; }
    msnartDoorLab944(c, X, Y, 0xFFFFFFFFu, 0.f); resetState(); return;
  }
  for (int i = 0; i < 4; i++) {
    float px = (float)(i - 1) * 120.f - 60.f, c[3], rad = 48.f / ppm;   // центры: -180, -60, 60, 180 пикс от середины экрана, на 20 пикс выше середины (сверху — строки HUD, снизу — радар)
    for (int j = 0; j < 3; j++) c[j] = e3_[j] + f[j] * D + R[j] * (px / ppm) + U[j] * (20.f / ppm);
    sceGuDisable(GU_DEPTH_TEST);   // глубина после очистки обратная (GEQUAL) — лист рисуется без проверки
    if (i < 2) {
      gvBb0 = ggVbUsed; GvV *v = (GvV *)ggVbAlloc(gvNs(i) * sizeof(GvV)); if (!v) continue;
      gvStar(v, c, R, U, rad, i, 0xFFFFFFu, 0xFFFFFFu, .7f); gvB[0].v = v; gvB[0].n = gvNs(i); gvNB = 1; gvFlush();
    } else { fxLab941(c, R, U, i == 2 ? rad / .78f : rad, i == 2 ? 3 : 1); resetState(); }   // звезда стекла: радиус = .78 полуоси квада
  }
}
static void glintBegin(unsigned cap) { if (!GLINT_VEC_OLD) { gvNB = 0; gvBb0 = ggVbUsed; return; }
  glQ = 0; glN = 0; glCap = cap * 2u < GL_MAX ? cap * 2u : GL_MAX; glRing = 0; glRingN = 0; glBb0 = ggVbUsed; }   // cap — сколько бликов (по 2 звезды)
static void glintQuad(GlV *o, const float c[3], float Rx, float Rz, float hs, float rot, unsigned col) {
  float cr = cosf(rot), sr = sinf(rot); static const float X[4] = {-1.f, 1.f, -1.f, 1.f}, Y[4] = {-1.f, -1.f, 1.f, 1.f}; static const unsigned char ord[6] = {0, 1, 2, 2, 1, 3};
  for (int m = 0; m < 6; m++) { int k = ord[m]; float xr = X[k] * cr - Y[k] * sr, yr = X[k] * sr + Y[k] * cr;
    o[m].u = (float)(k & 1); o[m].v = (float)(1 - (k >> 1)); o[m].c = col; o[m].x = c[0] + Rx * xr * hs; o[m].y = c[1] + yr * hs; o[m].z = c[2] + Rz * xr * hs; }
}
static unsigned glintCol(unsigned rgb, float k) { if (k < 0.f) k = 0.f; if (k > 1.f) k = 1.f;
  return 0xFF000000u | ((unsigned)(((rgb >> 16) & 255u) * k) << 16) | ((unsigned)(((rgb >> 8) & 255u) * k) << 8) | (unsigned)((rgb & 255u) * k); }   // rgb = 0xBBGGRR
static void glintAdd(const float c[3], float ex, float ey, float ez, float scale, unsigned rgb1, unsigned rgb2, float alpha, float phase) {
  if (!GLINT_VEC_OLD) { gvAdd(c, ex, ey, ez, scale, rgb1, rgb2, alpha, phase); return; }
  if (wrgState <= 0 || glN + 2 > glCap) return;
  if (!glQ) glQ = (GlV *)ggVbAlloc(glCap * 6 * sizeof(GlV));   // вершины — одним куском на пачку: столько, сколько заказано
  float dx = c[0] - ex, dz = c[2] - ez, l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) l = 1e-3f;
  float Rx = dz / l, Rz = -dx / l, tm = (float)(sceKernelGetSystemTimeLow() % 60000000u) * 1e-6f + phase;
  for (int s = 0; s < 2; s++) {
    float per = s ? .83f : 1.17f, ph = s ? 1.9f : 0.f, amp = s ? .5f : .2f, base = s ? .4f : 1.f, rot = s ? .66f : 0.f;   // малая — 2,09/5,22 большой, как у диска; повёрнута на 38°
    float sz = scale * base * (1.f - .5f * amp + amp * (.5f + .5f * sinf(6.2831853f * tm / per + ph)));
    glintQuad(glQ + glN * 6, c, Rx, Rz, .5f * sz, rot, glintCol(s ? rgb2 : rgb1, alpha)); glN++;
  }
  if (WRGLOW_RING && glRingN < 32) { if (!glRing) glRing = (GlV *)ggVbAlloc(32 * 6 * sizeof(GlV)); glintQuad(glRing + glRingN * 6, c, Rx, Rz, .5f * scale, 0.f, glintCol(0xFF5B48u, alpha * .6f)); glRingN++; }
}
static void glintFlush(void) {
  if (!GLINT_VEC_OLD) { gvFlush(); return; }
  if (!glN && !glRingN) return;
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_FOG);
  sceGuEnable(GU_TEXTURE_2D); sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB); sceGuTexMode(GU_PSM_8888, 0, 0, 0);
  ggVbWriteback(glBb0);   // вершины в память до вызовов: GE читает их из RAM
  if (glRingN) wrgDraw(1, glRing, glRingN);
  if (glN) wrgDraw(0, glQ, glN);
  sceGuDisable(GU_TEXTURE_2D); sceGuTexScale(128.f, 128.f); sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDepthMask(GU_FALSE); resetState();
  glN = glRingN = 0; glQ = glRing = 0;
}
// №839 (Роб 19.10: «мигает не асфальт, а размытое свечение у восклицательного знака — все блюрные свечения замени нашим бликом»): свечение
// рисунка msnart (квады сложением у «!», $, кубиков, флажков, стрелок, предметов) — наш векторный блик (две шестиконечные звезды не в фазе, как у ключа
// и трубки): центр — самого крупного квада кадра, размер — его ширина × GG_K (не больше GG_MAX м), цвета — двух крупных квадов, альфа — кадра
#define GG_K .7f
// №908 (Роб 23.10: «блики возле «!» и возле места, которое нужно сделать, слишком яркие — прозрачнее»): блики значков (свечения msnart: «!», цель,
// предметы; блик под «!» и трубкой) — доля MARK_GLINT_K. GLINT_MARK908_OLD=1 — как было (1)
#ifndef GLINT_MARK908_OLD
#define GLINT_MARK908_OLD 0
#endif
#define MARK_GLINT_K (GLINT_MARK908_OLD ? 1.f : .5f)
#define GG_MAX 3.f
static int glowGlintHook(const char *name, const float p[3], float w, unsigned c1, unsigned c2, float a, float ex, float ey, float ez) {
  if (beam995 && !strcmp(name, "spaceship")) return 1;   // №995: блинк под тарелкой не рисуется — свет даёт конус (lightsprite.h beam995Draw)
  float sc = w * GG_K; if (sc > GG_MAX) sc = GG_MAX;
  if (smkOn) { static char seen_[16][24]; static int nseen_; int k_ = 0; for (; k_ < nseen_; k_++) if (!strcmp(seen_[k_], name)) break;
    if (k_ == nseen_ && nseen_ < 16) { snprintf(seen_[nseen_++], 24, "%s", name); char l_[160]; snprintf(l_, sizeof l_, "GLOWGLINT %s p=%.1f,%.1f,%.1f w=%.2f scale=%.2f c1=%06x c2=%06x a=%.2f\n", name, p[0], p[1], p[2], w, sc, c1 & 0xffffffu, c2 & 0xffffffu, a); smkWrite(l_); } }
  glintBegin(1); glintAdd(p, ex, ey, ez, sc, c1 & 0xffffffu, c2 & 0xffffffu, (GLINT_VEC_OLD ? GLINT_A : GLINT_VA) * MARK_GLINT_K * (a > 1.f ? 1.f : a), (p[0] + p[2]) * .37f); glintFlush(); sceGuDisable(GU_FOG);   // №908: MARK_GLINT_K
  return 1;
}
// №839: очередь бликов кадра — свечения вне прохода значков (искры fx.c, столб и звёзды подбора collectfx.c, огни машин carvis.c, квады сложением
// карточек/ос/приколов main_collect.h, бонус-игры bonus_play.h, огни заграждений у пропов main_loop.h); сброс — после своего цикла и после частиц кадра
#define GGQ_MAX 96
typedef struct { float p[3], sc, a, ph; unsigned c; int one; } GgQ;   // one — №970: одна звезда (маленькие звёзды монет)
static GgQ ggQ[GGQ_MAX]; static unsigned ggQn, ggQdrop;
static void glowGlintQ_(float x, float y, float z, float sc, unsigned abgr, float a, float ph) {
  if (sc < .02f || a < .004f) return; if (ggQn >= GGQ_MAX) { ggQdrop++; return; }
  gsQ++;
  if (smkOn) { static int n_, lv_ = -1; static int16_t k_[200][3]; if (lv_ != gameLevel) { lv_ = gameLevel; n_ = 0; } if (n_ < 200) { int16_t g_[3] = {(int16_t)lroundf(x), (int16_t)lroundf(y), (int16_t)lroundf(z)}; int j_ = 0; for (; j_ < n_; j_++) if (!memcmp(k_[j_], g_, sizeof g_)) break;
      if (j_ == n_) { memcpy(k_[n_++], g_, sizeof g_); char l_[160]; snprintf(l_, sizeof l_, "GQ944 lv=%d site=%s p=%.1f,%.1f,%.1f sc=%.2f abgr=%06x a=%.2f\n", gameLevel, gq944Site ? gq944Site : "?", x, y, z, sc, abgr & 0xffffffu, a); smkWrite(l_); } } }   // №957б служебно: квады свечения пропов, ставшие бликом
  GgQ *q = &ggQ[ggQn++]; q->p[0] = x; q->p[1] = y; q->p[2] = z; q->sc = sc > GG_MAX ? GG_MAX : sc; q->a = a > 1.f ? 1.f : a; q->ph = ph; q->c = abgr & 0xFFFFFFu; q->one = fxGlintOne970;
}
static const char *gq944Site;   // служебно: кто поставил квад в очередь бликов
static void glowQuadQ(const float *v, unsigned abgr, float ph) {   // квад (4 угла v[12]) -> блик в его центре, размер — бо́льшая сторона × GG_K
  float w = sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2]));
  float h = sqrtf((v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2]));
  float k989 = gq944Site && (!strcmp(gq944Site, "coll") || !strcmp(gq944Site, "bonus")) ? BLING989_A : 1.f;   // №989: свечения предметов и бонус-объектов прозрачнее; огни и свечения пропов — как были
  glowGlintQ_((v[0] + v[3] + v[6] + v[9]) * .25f, (v[1] + v[4] + v[7] + v[10]) * .25f, (v[2] + v[5] + v[8] + v[11]) * .25f, (w > h ? w : h) * GG_K, abgr, (float)(abgr >> 24) / 255.f * k989, ph);
}
static void glowGlintFlush(float ex, float ey, float ez) {
  for (unsigned s = 0; s < ggQn; s += 32) { unsigned n = ggQn - s > 32 ? 32 : ggQn - s; glintBegin(n);
    for (unsigned k = s; k < s + n; k++) { if (!GV_COST845_OLD) { float dx_ = ggQ[k].p[0] - ex, dy_ = ggQ[k].p[1] - ey, dz_ = ggQ[k].p[2] - ez; if (dx_ * dx_ + dy_ * dy_ + dz_ * dz_ > GV_Q_D * GV_Q_D) continue; }   // №845
      gvOne970 = ggQ[k].one; glintAdd(ggQ[k].p, ex, ey, ez, ggQ[k].sc, ggQ[k].c, ggQ[k].c, (GLINT_VEC_OLD ? GLINT_A : GLINT_VA) * ggQ[k].a, ggQ[k].ph); gvOne970 = 0; }
    glintFlush(); }
  ggQn = 0;
}
// №834 (Роб 18.10: «элементы, которые включаешь — подъёмник и так далее — там кнопка подсвечивается; этот блик тоже нужен векторный»): свечение
// выключателей объектов с действием — типы только из квадов (кольцо выключателя грузовика Duff z2switchbbqgrp, кольца звонков switchN_bb, огни пульта
// АЭС z6_switchlights; tools/animobj_export.py) — не картинкой (сложением оно было спрятано за neonGlowOn), а нашим векторным бликом (glintAdd: две
// шестиконечные звезды не в фазе): центр и размер — квада, цвет и пульс (ширина, альфа) — дорожки BQG данных. AO_GLINT_OLD=1 — картинкой, как раньше
#ifndef AO_GLINT_OLD
#define AO_GLINT_OLD 0
#endif
#define AO_GLINT_K 1.3f   // альфа блика выключателя — доля альфы дорожки (у Duff 0,13↔0,47 -> 0,17↔0,61)
// №770/№885 (Роб 23.10: «звёздочка на причале над аркой с надписью, неоновые вывески на въезде в АЭС и на арке у парковки — убрать, пусть будет арка без
// неонового свечения; сильно ломает FPS; подобное — аккуратно на вектор»): блик №834 задумывался для выключателей (кнопки, звонки, подъёмник Duff), а под
// «тип только из квадов» попали и вывески без действия (АЭС: 4 мигалки 1,1 м и вывеска 2,9 м; L3: вывеска 4,1 м) и струи капель (10,5 × 17 м). Теперь блик —
// у объекта с действием или у мелкого огонька (≤ AO_GLINT_SMALL м, лампы пульта АЭС); остальные квады-свечения объектов без действия не рисуются вовсе
// (сама вывеска — куски города). AO_NEON885_OLD=1 — как было (блик у всех)
#ifndef AO_NEON885_OLD
#define AO_NEON885_OLD 0
#endif
#define AO_GLINT_SMALL .8f
#define NEON885_MAX 2.f   // м: свечение пропа со сложением крупнее — вывеска, не рисуется (огни заграждений PVIS и мелкие огни — бликом №839)
static unsigned char *aoActM;   // бит экземпляра — объект с действием (ANM1 action > 0)
static unsigned char *aoBillM; static unsigned aoBillN; static const void *aoBillKey, *aoBillKey2;
static int aoBillInst(unsigned i) {   // экземпляр i — квады выключателя объекта с действием (тип без кусков)
  if (AO_GLINT_OLD || !haveAo || !haveSP) return 0;
  if (aoBillKey != (const void *)ao.obj || aoBillKey2 != (const void *)sprops.inst || aoBillN != sprops.ninst) {   // новый уровень — маска заново
    free(aoBillM); free(aoActM); aoBillN = sprops.ninst; aoBillKey = ao.obj; aoBillKey2 = sprops.inst; aoBillM = (unsigned char *)calloc((aoBillN + 7) / 8 + 1, 1); aoActM = (unsigned char *)calloc((aoBillN + 7) / 8 + 1, 1);
    if (aoBillM && aoActM) for (unsigned k = 0; k < ao.nobj; k++) for (unsigned j = 0; j < ao.obj[k].nInst; j++) {
      unsigned x = (unsigned)ao.obj[k].firstInst + j; if (x < aoBillN) { aoBillM[x >> 3] |= (unsigned char)(1u << (x & 7)); if (ao.obj[k].action) aoActM[x >> 3] |= (unsigned char)(1u << (x & 7)); } }
  }
  if (!aoBillM || i >= aoBillN || !(aoBillM[i >> 3] & (1u << (i & 7)))) return 0;
  const SpType *T = &sprops.type[sprops.inst[i].type]; return T->nBill && !T->nPiece;
}
static int aoBillNeon(unsigned i, const float *v) {   // №885: квад-свечение объекта без действия крупнее огонька — не рисуется
  if (AO_NEON885_OLD || !aoActM || i >= aoBillN || (aoActM[i >> 3] & (1u << (i & 7)))) return 0;
  float w = sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2])), h = sqrtf((v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2]));
  return (w > h ? w : h) > AO_GLINT_SMALL;
}
static void aoBillGlint(unsigned i, const float *v, uint32_t col, float ex, float ey, float ez) {   // квад выключателя (4 угла v) -> блик в его центре
  gsAo++;
  if (smkOn) { static int n_, lv_ = -1; static int16_t k_[64][3]; if (lv_ != gameLevel) { lv_ = gameLevel; n_ = 0; } if (n_ < 64) { int16_t g_[3] = {(int16_t)lroundf((v[0] + v[9]) * .5f), (int16_t)lroundf((v[1] + v[10]) * .5f), (int16_t)lroundf((v[2] + v[11]) * .5f)}; int j_ = 0; for (; j_ < n_; j_++) if (!memcmp(k_[j_], g_, sizeof g_)) break;
      if (j_ == n_) { memcpy(k_[n_++], g_, sizeof g_); char l_[160]; snprintf(l_, sizeof l_, "AOQ944 lv=%d inst=%u p=%.1f,%.1f,%.1f act=%d\n", gameLevel, i, (v[0] + v[9]) * .5f, (v[1] + v[10]) * .5f, (v[2] + v[11]) * .5f, aoActM && i < aoBillN ? !!(aoActM[i >> 3] & (1u << (i & 7))) : -1); smkWrite(l_); } } }   // №957б служебно: квады объектов, ставшие бликом
  if (!GV_COST845_OLD) { float cx_ = (v[0] + v[9]) * .5f - ex, cy_ = (v[1] + v[10]) * .5f - ey, cz_ = (v[2] + v[11]) * .5f - ez; if (cx_ * cx_ + cy_ * cy_ + cz_ * cz_ > GV_AO_D * GV_AO_D) return; }   // №845
  float c[3] = {(v[0] + v[3] + v[6] + v[9]) * .25f, (v[1] + v[4] + v[7] + v[10]) * .25f, (v[2] + v[5] + v[8] + v[11]) * .25f};
  float w = sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2]));
  float h = sqrtf((v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2]));
  glintAdd(c, ex, ey, ez, w > h ? w : h, col & 0xFFFFFFu, col & 0xFFFFFFu, (float)(col >> 24) / 255.f * AO_GLINT_K * BLING989_A, (float)i * .41f);   // №989: блик выключателя прозрачнее
}
#ifndef WRENCH970_S
#define WRENCH970_S .7f   // №970: размер блика ключа от TEST66
#endif
#ifndef WRENCH970_A
#define WRENCH970_A .6f   // №970: прозрачность блика ключа (доля от TEST66)
#endif
#ifndef PHONE970_CORE
#define PHONE970_CORE 0.f // №970: сердцевина блика будки (0 — нет: два слоя вместо трёх)
#endif
#ifndef PHONE970_A
#define PHONE970_A .5f    // №970: блик будки «слишком яркий» — вдвое прозрачнее
#endif
static void wrGlowDraw(float ex, float ey, float ez, int inR) {
  if (!wrGlowOn || inR || !haveCol || !world.nwrench || !haveSP) return;
  if (smkOn) { static const void *wl_; if (wl_ != (const void *)world.wrench) { wl_ = world.wrench; char l_[200]; int o_ = snprintf(l_, sizeof l_, "WRENCHAT n=%u", world.nwrench);
      for (unsigned k = 0; k < world.nwrench && k < 4 && o_ < 170; k++) o_ += snprintf(l_ + o_, sizeof l_ - (size_t)o_, " %.1f,%.1f,%.1f", world.wrench[k].p[0], world.wrench[k].p[1], world.wrench[k].p[2]);
      snprintf(l_ + o_, sizeof l_ - (size_t)o_, "\n"); smkWrite(l_); } }   // №844 служебно: где ключи уровня
  if (!WRGLOW_ORIG_OLD && GLINT_VEC_OLD && !wrgState) wrgLoad();   // картинки (wrglow.bin) — только блику картинкой
  if (!WRGLOW_ORIG_OLD && (!GLINT_VEC_OLD || wrgState > 0)) {   // №803/№816: блик — две звезды (векторные, GLINT_VEC_OLD=1 — картинкой)
    unsigned t1 = smkOn ? sceKernelGetSystemTimeLow() : 0; float e3[3] = {ex, ey, ez}; unsigned n0 = 0; int nd1031_ = 0;
    glintBegin(world.nwrench < 32 ? world.nwrench : 32);
    for (unsigned k = 0; k < world.nwrench && k < 32; k++) {
      int i = G.wrenchInst[k]; if (i < 0 || !sprops.run[i].alive || ((G.wrenchTaken >> k) & 1u)) continue;
      const ColWrench *W = &world.wrench[k]; float dx = W->p[0] - ex, dy = W->p[1] - ey, dz = W->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > 40.f * 40.f) continue;
      float f = d2 < 30.f * 30.f ? 1.f : (40.f - sqrtf(d2)) / 10.f;
      float c3[3] = {W->p[0], W->p[1] + WRGLOW_CY, W->p[2]};
      if (rfSphereOut(pl, e3, c3, 3.f)) continue;
      float l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) l = 1e-3f; c3[0] += dx / l * .9f; c3[2] += dz / l * .9f;   // DIST -0.9 диска: за ключом от камеры
      float obj_[3] = {W->p[0], W->p[1] + WRGLOW_CY, W->p[2]};
      if (glint1031 && glintFix1022) { if (!gf1022Visible(obj_, ex, ey, ez)) continue; nd1031_ = 1; }   // №1031: виден — без проверки глубины, нет — звезды нет
      float kp_ = gf1022Pull(c3, obj_, ex, ey, ez);   // №1022: ближе к глазу и меньше — на экране то же, землю не задевает
      gvMode970 = 1;   // №970: меньше (WRENCH970_S) и прозрачнее (WRENCH970_A) во всех слоях, свой ход слоёв
      gvSrc1022 = "wrench";
      glintAdd(c3, ex, ey, ez, 5.22f * (fxGlint970Old ? 1.f : WRENCH970_S) * kp_, 0xFF2685u, GLINT_VEC_OLD ? 0xFF4640u : 0xFF5B48u, (GLINT_VEC_OLD ? GLINT_A : GLINT_VA) * f * (fxGlint970Old ? 1.f : WRENCH970_A), (float)k * .37f); n0++;
      gvMode970 = 0;   // цвета оригинала: (133,38,255) и (64,70,255) / (72,91,255)
    }
    if (smkOn) { wrGlowQuads += n0 * 2; wrGlowFrames++; }
    if (nd1031_) sceGuDisable(GU_DEPTH_TEST);
    glintFlush();
    if (nd1031_) sceGuEnable(GU_DEPTH_TEST);
    if (smkOn) wrGlowUs += sceKernelGetSystemTimeLow() - t1;
    return;
  }
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0;
  typedef struct { float u, v; unsigned c; float x, y, z; } WV;
  float e3[3] = {ex, ey, ez}; unsigned n = 0; WV *q = 0; unsigned bb0 = ggVbUsed;
  float tm = (float)(sceKernelGetSystemTimeLow() & 0xFFFFFFu) * 1e-6f;
  for (unsigned k = 0; k < world.nwrench && k < 32; k++) {
    int i = G.wrenchInst[k]; if (i < 0 || !sprops.run[i].alive || ((G.wrenchTaken >> k) & 1u)) continue;
    const ColWrench *W = &world.wrench[k]; float dx = W->p[0] - ex, dy = W->p[1] - ey, dz = W->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > 40.f * 40.f) continue;
    float f = d2 < 30.f * 30.f ? 1.f : (40.f - sqrtf(d2)) / 10.f;
    float c3[3] = {W->p[0], W->p[1] + 1.8f, W->p[2]};   // центр модели ключа около p+1,8 (кадр TEST30, 107 пикс/м); блеск за ним по глубине (сдвиг от камеры на 0,35 м, ниже)
    if (rfSphereOut(pl, e3, c3, WRGLOW_R * 1.5f)) continue;
    if (!wrGlowReady) wrGlowInit();
    if (!q) q = (WV *)ggVbAlloc(32 * 6 * sizeof(WV));
    float l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) l = 1e-3f; c3[0] += dx / l * .35f; c3[2] += dz / l * .35f; float rx = dz / l * WRGLOW_R, rz = -dx / l * WRGLOW_R;
    float pulse = .78f + .22f * sinf(tm * 3.1f + (float)k), R = WRGLOW_R * (.94f + .06f * sinf(tm * 3.1f + (float)k));
    unsigned a = (unsigned)(200.f * f * pulse); unsigned col = (a << 24) | 0x00FFFFFFu; (void)R;
    float P[4][3] = {{c3[0] - rx, c3[1] - WRGLOW_R, c3[2] - rz}, {c3[0] + rx, c3[1] - WRGLOW_R, c3[2] + rz}, {c3[0] - rx, c3[1] + WRGLOW_R, c3[2] - rz}, {c3[0] + rx, c3[1] + WRGLOW_R, c3[2] + rz}};
    static const unsigned char ord[6] = {0, 1, 2, 2, 1, 3}; WV *o = q + n * 6;
    for (int m = 0; m < 6; m++) { int c = ord[m]; o[m].u = (float)(c & 1); o[m].v = (float)(1 - ((c >> 1) & 1)); o[m].c = col; o[m].x = P[c][0]; o[m].y = P[c][1]; o[m].z = P[c][2]; }
    n++;
  }
  if (smkOn) { wrGlowQuads += n; wrGlowFrames++; }
  if (!n) { if (smkOn) wrGlowUs += sceKernelGetSystemTimeLow() - t0; return; }
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_FOG);
  sceGuEnable(GU_TEXTURE_2D); sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 64, 64, 64, wrGlowTex);
  sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n * 6, 0, q);
  ggVbWriteback(bb0);
  sceGuDisable(GU_TEXTURE_2D); sceGuTexScale(128.f, 128.f); sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDepthMask(GU_FALSE); resetState();
  if (smkOn) wrGlowUs += sceKernelGetSystemTimeLow() - t0;
}
// №803 (Роб 18.10: «гаечный ключ можно перевести в спрайт, как монету: кадры 3D-модели, запечённые офлайн, поверх полупрозрачного блика»): ключ мира —
// квадрат лицом к камере (вокруг вертикали) с кадром ленты wrenchspr.bin рядом с EBOOT (формат coinspr.bin: tools/wrench/wrench_dump.py -> wrench_raster.py
// -> tools/coin/coin_pack.py; 32 кадра оборота настоящей модели wrench7Shape с диска, 64/32 пикс). Центр квадрата — центр кадра ленты (cy заголовка над началом
// куска по его оси) после позы стейтпропа (подскок и подъём ключа — те же, что у 3D), кадр — по часам. 3D-куски ключа не рисуются (main_loop.h, цикл
// стейтпропов). Нет файла — 3D, как было. WRENCH_SPRITE_OLD=1 — 3D-ключ
// Добавка Роба 18.10 (тем же способом): «!» над персонажами миссий — exclspr.bin (медленный оборот вокруг своей оси; подскок и покачивание — поза
// оригинала: центр меша по кадру клипа, наклон — поворотом квадрата) и трубка над будкой — phonespr.bin (один оборот клипа оригинала, лицом к нам).
// Ленты — tools/wrench/msnart_sprite_dump.py -> icon_bake.py -> tools/coin/coin_pack.py; рисует msnart.c через msnartSprite (ниже iconSprHook),
// под значком — общий блик (glintAdd). Нет файла — 3D, как было. ICON_SPRITE_OLD=1 — 3D-значки
#ifndef WRENCH_SPRITE_OLD
#define WRENCH_SPRITE_OLD 0
#endif
#ifndef ICON_SPRITE_OLD
#define ICON_SPRITE_OLD 0
#endif
typedef struct { char magic[4]; unsigned short w, h, levels, frames, cols, rows, px, pad; float half, cy, fps; } WsHdr;   // 32 Б, как coinspr.bin
enum { WS_WRENCH, WS_EXCL, WS_PHONE, WS_N };
static const char *const wsFile[WS_N] = {"wrenchspr.bin", "exclspr.bin", "phonespr.bin"};
// №829 (Роб 18.10: «гаечный ключ очень пиксельный, слишком битый — хочется его в большем размере»): лента может идти страницами 512 × 512 (pad заголовка —
// число страниц; GE не берёт картинку больше 512): ключ — кадр 128 пикс (вблизи ключ ~140 пикс экрана — почти 1 : 1; было 64 — растяжка в 2,2 раза),
// 32 кадра = 2 страницы, файл 641 КБ (был 161 КБ) — живёт в памяти целиком, при свободных ~14 МБ кучи. Палитра с полупрозрачным краем (запекание
// 4 × 4 отсчёта на пиксель) — край рисуется смешиванием, без «лесенки» альфа-теста. CLUT8 оставлен: 8888 той же ленты — 2,5 МБ, 4444 — 1,3 МБ, а «битость»
// давали размер кадра и жёсткий край, не палитра. WS_SOFT_OLD=1 — край альфа-тестом, как раньше
#ifndef WS_SOFT_OLD
#define WS_SOFT_OLD 0
#endif
#define WS_PAGES 4
static struct { WsHdr h; unsigned char *mem; const unsigned char *pal, *lv[WS_PAGES][2]; int state, pages, soft; unsigned char palRaw[1024]; } wsA[WS_N];
#define WS_MAXQ 40
static struct { int a; float c[3], rot, px, py, ox, oy; } wsQ[WS_MAXQ]; static unsigned wsN, wsDrawn, wsFrames, wsIcons, wsIconUs;   // c — точка опоры, (px, py) — её место от центра квадрата, (ox, oy) — сдвиг по экрану, м
static void wsLoad(int a) {
  wsA[a].state = -1; char fn[256]; sibling(fn, wsFile[a]); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fd < 0) return;
  int n = (int)sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET); WsHdr *H = &wsA[a].h;
  if (n > 32 + 1024 && n < 1 << 20 && (wsA[a].mem = (unsigned char *)memalign(16, (size_t)n)) && sceIoRead(fd, wsA[a].mem, (SceSize)n) == n) {
    memcpy(H, wsA[a].mem, sizeof *H); unsigned l0 = (unsigned)H->w * H->h, l1 = l0 / 4, pg = H->pad ? H->pad : 1u;
    if (!memcmp(H->magic, "CSP1", 4) && H->w == 512 && (H->h == 256 || H->h == 512) && H->levels == 2 && pg <= WS_PAGES && H->frames >= 8 && H->frames <= H->cols * H->rows * pg &&
        H->cols * H->px == H->w && H->rows * H->px == H->h && 32u + 1024u + pg * (l0 + l1) <= (unsigned)n && H->half > .2f && H->half < 5.f && H->cy > -5.f && H->cy < 5.f &&
        H->fps > 1.f && H->fps < 120.f) {
      wsA[a].pal = wsA[a].mem + 32; wsA[a].pages = (int)pg; wsA[a].soft = 0;
      for (unsigned p = 0; p < pg; p++) { wsA[a].lv[p][0] = wsA[a].pal + 1024 + p * (l0 + l1); wsA[a].lv[p][1] = wsA[a].lv[p][0] + l0; }
      for (int i = 1; i < 256; i++) { unsigned char al_ = wsA[a].pal[i * 4 + 3]; if (al_ && al_ < 255) wsA[a].soft = 1; }   // есть полупрозрачные цвета — мягкий край
      memcpy(wsA[a].palRaw, wsA[a].pal, 1024); palTone((void *)wsA[a].pal);
      sceKernelDcacheWritebackRange(wsA[a].mem, (unsigned)n); wsA[a].state = 1; }
  }
  sceIoClose(fd);
  if (wsA[a].state < 0 && wsA[a].mem) { free(wsA[a].mem); wsA[a].mem = 0; }
  if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "WSPRITE %s atlas=%d frames=%u px=%u pages=%d soft=%d bytes=%d half=%.3f cy=%.3f fps=%.1f\n", wsFile[a], wsA[a].state, H->frames, H->px,
    wsA[a].pages, wsA[a].soft, n, H->half, H->cy, H->fps); smkWrite(l_); }
}
static void wsRetone(void) {
  for (int a = 0; a < WS_N; a++) { if (wsA[a].state != 1 || !wsA[a].pal) continue;
    memcpy((void *)wsA[a].pal, wsA[a].palRaw, 1024); palTone((void *)wsA[a].pal); sceKernelDcacheWritebackRange((void *)wsA[a].pal, 1024); }
}
static int wsOn(int a) { if (!wsA[a].state) wsLoad(a); return wsA[a].state > 0; }
static int wrSprOn(void) { if (WRENCH_SPRITE_OLD || !haveCol || !world.nwrench) return 0; return wsOn(WS_WRENCH); }
static int wrSprInst(unsigned i) { for (unsigned k = 0; k < world.nwrench && k < 32; k++) if (G.wrenchInst[k] == (int)i) return 1; return 0; }
static void wsAddPivOff(int a, const float c[3], float rot, float px, float py, float ox, float oy) {   // квадрат поворачивается на rot вокруг своей точки (px, py)
  if (wsN >= WS_MAXQ) return; wsQ[wsN].a = a; memcpy(wsQ[wsN].c, c, sizeof wsQ[wsN].c); wsQ[wsN].rot = rot; wsQ[wsN].px = px; wsQ[wsN].py = py;   // от центра, она — в c,
  wsQ[wsN].ox = ox; wsQ[wsN].oy = oy; wsN++; }   // и весь сдвинут на (ox, oy) вправо/вверх по экрану
static void wsAddPiv(int a, const float c[3], float rot, float px, float py) { wsAddPivOff(a, c, rot, px, py, 0.f, 0.f); }
static void wsAdd(int a, const float c[3], float rot) { wsAddPiv(a, c, rot, 0.f, 0.f); }
static void wrSprAdd(unsigned i) {   // экземпляр ключа после позы: центр кадра ленты — на cy по оси куска от его начала
  float m[16]; spPieceMatrix(&sprops, i, 0, m); float cy = wsA[WS_WRENCH].h.cy, c[3] = {m[12] + m[4] * cy, m[13] + m[5] * cy, m[14] + m[6] * cy}; wsAdd(WS_WRENCH, c, 0.f);
}
// №815 (Роб 18.10: «гаечный ключ выглядит сжатым, сплющенным»): кадры ленты квадратные (запекание: один ортомасштаб по обеим осям, рисунок — квадрат
// half × half, клетка ленты 64 × 64 пикс в атласе 512 × 256), сплющивал поворот только вокруг вертикали: глаз выше ключа видел вертикальный квадрат
// наискось. Квадрат теперь лицом к глазу и по наклону взгляда (как квады AAX msnart). WS_CYL_OLD=1 — поворот только вокруг вертикали
#ifndef WS_CYL_OLD
#define WS_CYL_OLD 0
#endif
static void wsFlush(float ex, float ey, float ez) {   // все квадраты очереди: лентами по очереди (палитра и картинка ленты — один раз на ленту)
  if (!wsN) return;
  typedef struct { float u, v; unsigned c; float x, y, z; } QV;
  QV *q = (QV *)sceGuGetMemory((int)(wsN * 6 * sizeof(QV))); unsigned n = 0, t = sceKernelGetSystemTimeLow(); int set = 0;
  static const unsigned char ord[6] = {0, 2, 1, 1, 2, 3};
  for (int a = 0; a < WS_N; a++) {
    const WsHdr *H = &wsA[a].h; unsigned n0 = n;
    if (wsA[a].state <= 0) continue;
    unsigned fr = a == WS_EXCL ? 0u : (t / (unsigned)(1e6f / H->fps)) % H->frames;   // №814: «!» — кадр 0, без оборота
    unsigned per = (unsigned)H->cols * H->rows, pgi = fr / per, ft = fr % per, fc = ft % H->cols, frr = ft / H->cols;   // №829: страница ленты и место кадра на ней
    float u0 = (fc * H->px + .5f) / H->w, u1 = ((fc + 1) * H->px - .5f) / H->w, v0 = (frr * H->px + .5f) / H->h, v1 = ((frr + 1) * H->px - .5f) / H->h, hs = H->half;
    for (unsigned k = 0; k < wsN; k++) {
      if (wsQ[k].a != a) continue;
      const float *C = wsQ[k].c; float dx = C[0] - ex, dy = C[1] - ey, dz = C[2] - ez, l = sqrtf(dx * dx + dz * dz); if (l < .05f) continue;
      float Rx = -dz / l, Rz = dx / l, cr = cosf(wsQ[k].rot), sr = sinf(wsQ[k].rot), Ux = 0.f, Uy = 1.f, Uz = 0.f;
      if (!WS_CYL_OLD) { float dl = sqrtf(dx * dx + dy * dy + dz * dz); Ux = -dy / dl * dx / l; Uy = l / dl; Uz = -dy / dl * dz / l; }   // верх квадрата — поперёк взгляда
      const float X[4] = {-1.f, 1.f, -1.f, 1.f}, Y[4] = {1.f, 1.f, -1.f, -1.f}, U[4] = {u0, u1, u0, u1}, V[4] = {v0, v0, v1, v1};
      const float px = wsQ[k].px, py = wsQ[k].py, ox = wsQ[k].ox, oy = wsQ[k].oy;
      for (int m = 0; m < 6; m++) { int c = ord[m]; QV *o = q + n * 6 + m; float kx = X[c] * hs - px, ky = Y[c] * hs - py, xr = kx * cr - ky * sr + ox, yr = kx * sr + ky * cr + oy;   // поворот вокруг точки опоры
        o->u = U[c]; o->v = V[c]; o->c = 0xFFFFFFFFu; o->x = C[0] + Rx * xr + Ux * yr; o->y = C[1] + Uy * yr; o->z = C[2] + Rz * xr + Uz * yr; }
      n++;
    }
    if (n == n0) continue;
    if (!set) {
      ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
      sceGuEnable(GU_TEXTURE_2D); texOn = -1; sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
      sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_LINEAR); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
      sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
      sceGuEnable(GU_ALPHA_TEST); sceGuDepthMask(GU_FALSE); setCull(0); curFlags = -1; set = 1; }
    if (wsA[a].soft && !WS_SOFT_OLD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuAlphaFunc(GU_GREATER, 0x08, 0xff); }   // №829: мягкий край
    else { sceGuDisable(GU_BLEND); sceGuAlphaFunc(GU_GREATER, 0x60, 0xff); }
    sceGuClutLoad(32, wsA[a].pal);
    sceGuTexMode(GU_PSM_T8, 1, 0, 1); sceGuTexImage(0, H->w, H->h, H->w, wsA[a].lv[pgi][0]); sceGuTexImage(1, H->w / 2, H->h / 2, H->w / 2, wsA[a].lv[pgi][1]); sceGuTexFlush();
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)((n - n0) * 6), 0, q + n0 * 6);
  }
  wsDrawn += n; wsFrames++; wsN = 0;
  if (set) { sceGuDisable(GU_BLEND); sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexScale(128.f, 128.f); rbId = -9; rbPal = 0; resetState(); }
}
static void wrSprFlush(float ex, float ey, float ez) { wsFlush(ex, ey, ez); }
// значок миссии (msnart.c, msnartSprite): c — центр меша после позы кадра, tilt — его наклон в плоскости XY, g/gs/gd — центр, размер и сдвиг от камеры
// первого квада-свечения оригинала (у ключа, «!» и трубки — одна и та же звезда, цвета (133,38,255) и (64,70,255)); 1 — нарисовано спрайтом
// №814 (Роб 18.10: «„!“ слишком сильно трясётся из стороны в сторону; вокруг своей оси пусть не крутится, только покачивается, сначала смотрит на нас,
// как в оригинале»): «!» — всегда кадр 0 ленты (лицом к камере), без оборота; вместо качания клипа (0..33° за 1 с, с боковым ходом центра) — мягкое
// покачивание ±EXCL_SWAY_DEG за EXCL_SWAY_T с; центр по горизонтали — поза покоя (кадр 0 клипа), по высоте — текущий кадр (подскок как был).
// EXCL_SPRITE_OLD=1 — «!» 3D оригинала
#ifndef EXCL_SPRITE_OLD
#define EXCL_SPRITE_OLD 0
#endif
#ifndef EXCL_SWAY_DEG
#define EXCL_SWAY_DEG 9.f
#endif
#ifndef EXCL_SWAY_T
#define EXCL_SWAY_T 2.6f
#endif
// №818 (Роб 18.10: «угол вращения у „!“ идёт не от вертикального центра, а от его нижней точки; в оригинале он как бы прыгал на нижней точке
// и немножечко влево-вправо минимально покачивался»): числа из клипа exclamation (missgen.p3d, сустав меша, 30 к/с, круг 30 кадров): наклон +1,0° (кадр 0)
// -> -32,9° (кадр 15) -> +1,0° (кадр 30), опора — низ точки «!»: он поднимается на 0,14 м (кадры 8 и 22) и «приземляется» на крайних наклонах (0, 15, 30).
// Спрайт: квадрат поворачивается вокруг нижней точки (её место в кадре 0 ленты — pv), наклон клипа от середины (-15,95°) × EXCL_TILT_K (0,35: ±5,9° —
// «минимально»: лицом к камере качание клипа 34° читалось тряской), опора по высоте — из клипа (подскок), по горизонтали — поза покоя.
// EXCL_PIVOT_OLD=1 — покачивание вокруг центра (№814)
#ifndef EXCL_PIVOT_OLD
#define EXCL_PIVOT_OLD 0
#endif
#ifndef EXCL_TILT_K
#define EXCL_TILT_K .35f
#endif
#define EXCL_TILT_MID (-15.95f * 3.1415927f / 180.f)
// №819 (Роб 18.10: «„!“ почти как надо; капельку выше и правее относительно блика и лучше easing тряски — когда он сам наклоняется»): наклон — косинус по
// месту в клипе (круг 30 кадров = 1 с; крайние ±16,95° × EXCL_TILT_K = ±5,9° на кадрах 0, 15, 30, 45, 60 — там же «приземления» подскока), без ступенек
// по кадрам и ломаной; спрайт сдвинут по экрану на EXCL_NUDGE_X вправо и EXCL_NUDGE_Y вверх (0,06 м ≈ 3 пикс экрана 480 × 272 при 8–10 м), блик на месте;
// ось — низ точки (№818). EXCL_EASE_OLD=1 — наклон по кадрам клипа, без сдвига (№818)
#ifndef EXCL_EASE_OLD
#define EXCL_EASE_OLD 0
#endif
#ifndef EXCL_NUDGE_X
#define EXCL_NUDGE_X .06f
#endif
#ifndef EXCL_NUDGE_Y
#define EXCL_NUDGE_Y .06f
#endif
static int iconSprHook(const char *name, const float *c, const float *cr, const float *bt, const float *br, const float *pv, float tilt, float cf, const float *g, float gs, float gd, float ex, float ey, float ez) {
  int a = !strcasecmp(name, "phone_icon") ? WS_PHONE : !EXCL_SPRITE_OLD && !strcasecmp(name, "exclamation") ? WS_EXCL : -1;
  if (ICON_SPRITE_OLD || a < 0 || !wsOn(a)) return 0;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; sceGuDisable(GU_FOG);   // значки — без дымки, как 3D в msnart.c
  if (!WRGLOW_ORIG_OLD) { if (GLINT_VEC_OLD && !wrgState) wrgLoad();
    if (!GLINT_VEC_OLD || wrgState > 0) { float p[3] = {g[0], g[1], g[2]}, dx, dz, l;
      if (a == WS_EXCL && !GLINT_VEC_OLD) { p[0] = cr[0]; p[2] = cr[2]; }   // «!» стоит в позе покоя — блик под ним, не у свечения клипа
      dx = p[0] - ex; dz = p[2] - ez; l = sqrtf(dx * dx + dz * dz);
      if (l > 1e-3f) { p[0] += dx / l * gd; p[2] += dz / l * gd; }
      int nd1022_ = 0;   // №1022: «!» — звезда ближе к глазу и меньше (голова её не режет); трубка — целиком без проверки глубины, если видна
      if (a == WS_PHONE && glintFix1022) { nd1022_ = gf1022Visible(c, ex, ey, ez); if (!nd1022_) gs = 0.f; } else gs *= gf1022Pull(p, c, ex, ey, ez);
      if (gvLog1022 && smkOn) { char l_[160]; snprintf(l_, sizeof l_, "ICON1022 f=%u %s g=%.3f,%.3f,%.3f p=%.3f,%.3f,%.3f gs=%.3f gd=%.3f\n", smkFrameAbs, name, g[0], g[1], g[2], p[0], p[1], p[2], gs, gd); smkWrite(l_); }
      gvSrc1022 = name;
      if (nd1022_) sceGuDisable(GU_DEPTH_TEST);
      glintBegin(1); gvCoreK970 = (a == WS_PHONE && !fxGlint970Old) ? PHONE970_CORE : 1.f; glintAdd(p, ex, ey, ez, gs, 0xFF2685u, GLINT_VEC_OLD ? 0xFF4640u : 0xFF5B48u, (GLINT_VEC_OLD ? GLINT_A : GLINT_VA) * MARK_GLINT_K * (a == WS_PHONE && !fxGlint970Old ? PHONE970_A : 1.f), (float)a * .53f);   /* №908, №970: будка */ gvCoreK970 = 1.f; glintFlush(); if (nd1022_) sceGuEnable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); } }
  static unsigned logged_; if (smkOn && !(logged_ & (1u << a))) { logged_ |= 1u << a; char l_[160]; snprintf(l_, sizeof l_, "ICONSPR %s c=%.2f,%.2f,%.2f tilt=%.2f g=%.2f,%.2f,%.2f gw=%.2f gd=%.2f\n", name, c[0], c[1], c[2], tilt, g[0], g[1], g[2], gs, gd); smkWrite(l_); }
  if (a == WS_EXCL && !EXCL_PIVOT_OLD) { float p3[3] = {br[0], bt[1], br[2]};
    if (!EXCL_EASE_OLD) wsAddPivOff(a, p3, EXCL_TILT_K * (16.95f * 3.1415927f / 180.f) * cosf(6.2831853f * cf / 30.f), pv[0], pv[1], EXCL_NUDGE_X, EXCL_NUDGE_Y);
    else wsAddPiv(a, p3, EXCL_TILT_K * (tilt - EXCL_TILT_MID), pv[0], pv[1]); }
  else if (a == WS_EXCL) { float tm = (float)(sceKernelGetSystemTimeLow() % 60000000u) * 1e-6f, p3[3] = {cr[0], c[1], cr[2]};
    wsAdd(a, p3, EXCL_SWAY_DEG * (3.1415927f / 180.f) * sinf(6.2831853f * tm / EXCL_SWAY_T)); }
  else wsAdd(a, c, 0.f);
  wsFlush(ex, ey, ez); wsIcons++; if (smkOn) wsIconUs += sceKernelGetSystemTimeLow() - t0;   // дымку не возвращаем: как msnartDraw (вызывающий сам ставит состояние)
  return 1;
}
static void wrGlowStat(void) {
  if (!smkOn || !wrGlowFrames) { wrGlowQuads = wrGlowFrames = wrGlowUs = 0; return; }
  char s[200]; double k = 1.0 / wrGlowFrames; snprintf(s, sizeof s, "WRGLOW scene=%s on=%d quads=%.2f us=%.0f frames=%u spr=%.2f icons=%.2f icon_us=%.0f\n", smkScene, wrGlowOn, wrGlowQuads * k, wrGlowUs * k, wrGlowFrames,
    wsDrawn * k, wsIcons * k, wsIconUs * k); smkWrite(s);
  wrGlowQuads = wrGlowFrames = wrGlowUs = 0; wsDrawn = wsIcons = wsIconUs = 0;
}
#endif
