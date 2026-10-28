// №599 «свет спрайтами»: рисование таблицы постоянного света (viewer/lightsprite_core.h) и счётчики приёмки. Подключается из main.c
// после main_levels.h; вызовы — в main_loop.h вокруг прежнего цикла билбордов стейтпропов.
//   lspBegin()            кадр: таблица (при первой отрисовке уровня), какие экземпляры «свои»
//   lspOldShown(i, b)     прежний путь нарисовал этот билборд (для счётчиков)
//   lspOwnsBill(i, b)     билборд ведёт таблица — прежний цикл его пропускает
//   lspDraw(ex, ey, ez)   рисует записи таблицы одной пачкой на текстуру, ведёт счётчики появления
// «flag lspold 1» в сценарии (LIGHT_SPRITE_OLD=1 при сборке) — таблица не рисует, всё как до правки; счётчики идут и так (замер «было»).
#include "lightsprite_core.h"
static LspTab lsp; static int lspOld = LIGHT_SPRITE_OLD, lspBuilt, lspAxesFlag;
static float *lspA, *lspAp; static uint8_t *lspFr; static unsigned lspArr, lspLastFrame;
// счётчики сцены (строка LSP в smoke.txt)
static unsigned lspFramesN, lspShownSum, lspPopN, lspPopNear, lspDrawN, lspBindN, lspQuadN, lspFrameNo;
static unsigned lspAlphaJumps;
// №703 (Роб: «дохрена фонарей с конусом на асфальт, лагает»): конусы (mode 2) — один цилиндрический квад на источник; дальше LIGHTCONE_DIST_M не рисуются (плавно за LIGHTCONE_FADE_M),
// в кадре не больше LIGHTCONE_MAX ближних. LIGHTCONE_SPRITE_OLD=1 (или файл lightconeold рядом с EBOOT, flag lightconeold 1) — прежнее (100 м, без предела)
#ifndef LIGHTCONE_SPRITE_OLD
#define LIGHTCONE_SPRITE_OLD 0
#endif
#ifndef LIGHTCONE_DIST_M
#define LIGHTCONE_DIST_M 60.f
#endif
#ifndef LIGHTCONE_FADE_M
#define LIGHTCONE_FADE_M 16.f
#endif
#ifndef LIGHTCONE_MAX
#define LIGHTCONE_MAX 12
#endif
#ifndef LIGHTCONE_IN_A
#define LIGHTCONE_IN_A 0.35f   // глаз ближе этой доли полуширины конуса к оси: конус погашен
#endif
#ifndef LIGHTCONE_IN_B
#define LIGHTCONE_IN_B 1.0f    // с этой долей полуширины и дальше: полная яркость
#endif
static int lspConeOld = LIGHTCONE_SPRITE_OLD; static float lspConeDist = LIGHTCONE_DIST_M, lspConeFade = LIGHTCONE_FADE_M; static int lspConeMax = LIGHTCONE_MAX;
static double lspAreaSum;   // площадь заливки квадов света на кадр (пиксели, ограничена экраном), сумма за сцену
#ifndef LIGHTHALO_DIST_M
#define LIGHTHALO_DIST_M 60.f   // ореолы и знаки (mode 0/1) дальше не рисуются (плавно за LIGHTCONE_FADE_M)
#endif
#ifndef LIGHTHALO_MAX
#define LIGHTHALO_MAX 24        // и не больше стольких ближних на кадр
#endif
static float lspHaloDist = LIGHTHALO_DIST_M; static int lspHaloMax = LIGHTHALO_MAX;
// №715/736 (Роб 17.10: «векторный свет, как Jet Set Radio / The Simpsons Game на PSP: у фонаря просто треугольник с немножко прозрачным светом, без блюрного края»):
// конус (mode 2) — чёткая трапеция ровного цвета (узко у лампы, во всю ширину конуса у пола), без текстуры, обычная прозрачность; развёрнута к камере
// (цилиндрический билборд вокруг оси конуса), у камеры плавно гаснет (lspConeIn). Под конусом на полу — чёткий эллипс того же цвета, бледнее. Размер — конус
// оригинала (без расширения LSP_CONE_W/L). Все конусы и пятна кадра — один вызов. VEC_LIGHT_OLD=1 (файл veclightold, «flag veclight 0») — картинка с диска
#ifndef VEC_LIGHT_OLD
#define VEC_LIGHT_OLD 0
#endif
#define VL_TOPW .2f       // полуширина у лампы — доля полуширины у пола
#define VL_A 60.f         // прозрачность луча (из 255): «немножко света»
#define VL_MID .45f       // яркость на середине луча — доля яркости у лампы (к полу — 0)
#define VL_POOL_A 45.f    // пятно на полу
#define VL_POOL_N 16      // сторон эллипса пятна
// №852: углы эллипса пятна — таблица на VL_POOL_N + 1 точек (было 4 sinf/cosf на сторону каждого пятна каждый кадр); значения те же
static float vlPC_[VL_POOL_N + 1], vlPS_[VL_POOL_N + 1]; static int vlPOk_;
static void vlPoolTab(void) { if (vlPOk_) return; for (int i = 0; i <= VL_POOL_N; i++) { vlPC_[i] = cosf(6.2831853f * i / VL_POOL_N); vlPS_[i] = sinf(6.2831853f * i / VL_POOL_N); } vlPOk_ = 1; }
#define VL_PULL .15f      // пятно ближе к глазу по лучу взгляда, м (не тонет в полу)
#define VL_MAX 32
#define VL_DIST 120.f     // векторный конус дешёвый (один вызов на все, без текстуры): видно дальше прежних 60 м — далёкие фонари (Роб: «и вдалеке»)
#define VL_FADE 20.f
#define VL_WARM_K .5f     // доля тёплого (255, 236, 170) в цвете; остальное — цвет лампы из данных
#include "lamppool935.h"
static unsigned lpSkipN[2], lpDrawN;   // №935: лампы без эллипса (мост / ломаный пол) и с эллипсом — счёт при первом поиске пола
static int vecLight = !VEC_LIGHT_OLD; static float *lspPoolY; static unsigned vecLightN;   // lspPoolY: высота пола под концом конуса (лениво; 1e30 — не искали, −1e30 — нет пола)
static inline float lspHaloFadeAt(float d2) {
  if (lspConeOld) return lspFadeAt(d2);
  float end = lspHaloDist, beg = end - lspConeFade;
  if (d2 <= beg * beg) return 1.f;
  if (d2 >= end * end) return 0.f;
  return (end - sqrtf(d2)) / lspConeFade;
}
static inline float lspConeFadeAt(float d2) {
  if (lspConeOld) return lspFadeAt(d2);
  float end = lspConeDist, beg = end - lspConeFade;
  if (d2 <= beg * beg) return 1.f;
  if (d2 >= end * end) return 0.f;
  return (end - sqrtf(d2)) / lspConeFade;
}
static int lspCmpF(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static void lspReset(void) { lspFree(&lsp); free(lspA); free(lspAp); free(lspFr); free(lspPoolY); lspA = lspAp = 0; lspFr = 0; lspPoolY = 0; lspArr = 0; lspBuilt = 0; lspLastFrame = 0; }
static void lspEnsure(void) {
  if (lspBuilt && lsp.spInst == sprops.inst && lsp.spN == sprops.ninst && lsp.axesOld == lspAxesFlag) return;
  lspReset(); lspBuilt = 1; lspBuild(&lsp, &sprops, lspAxesFlag);
  lspArr = lsp.n; lspA = (float *)calloc(lspArr + 1, sizeof(float)); lspAp = (float *)calloc(lspArr + 1, sizeof(float)); lspFr = (uint8_t *)calloc(lspArr + 1, 1);
  if (!lspA || !lspAp || !lspFr) { lspReset(); lspBuilt = 1; return; }
  lspPoolY = (float *)malloc((lspArr + 1) * sizeof(float)); if (lspPoolY) for (unsigned k = 0; k <= lspArr; k++) lspPoolY[k] = 1e30f;   // пятна ищут пол один раз
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "LSPBUILD level=%d entries=%u inst=%u dist=%.0f fade=%.0f old=%d\n", gameLevel, lsp.n, sprops.ninst, (double)LSP_DIST_M, (double)LSP_FADE_M, lspOld); smkWrite(s); }
}
// №913 (оптимизация предметов: lspDraw 0,25–0,31 мс Мака из 0,52 «билбордов» на L5, 316 записей света): за кадр таблица обходилась четыре раза целиком (lspFrameLive, проход конусов,
// проход ореолов, основной), а в дальности света (VL_DIST 120 м) стоит десяток записей. Теперь обход — по списку записей в радиусе LSP_RANGE + LSP_NEAR_MARGIN от глаза на момент
// построения: пока глаз сместился не более LSP_NEAR_MARGIN (8 м), любая запись вне списка дальше дальности света от глаза (затухание 0, ничего не рисуется), поэтому проверки по записям
// списка те же, что были, и результат тот же (запись, вне списка, ничего не нарисовала бы и не поменяла бы порог «не больше N ближних»). Список — по возрастанию номера (порядок пачек
// по текстуре прежний). «Своими» (lsp.live) считаются только записи списка: экземпляры дальше 60 м билборды стейтпропов не обходят (cand2_), больше live нигде не читается.
// LSP_NEAR_OLD=1 / «flag lspnear 0|1 (2 — отрицательный контроль)» / файл lspnearold рядом с EBOOT — прежний полный обход
#ifndef LSP_NEAR_OLD
#define LSP_NEAR_OLD 0
#endif
#define LSP_NEAR_MARGIN 8.f
static int lspNearOn = !LSP_NEAR_OLD;
static uint16_t *lspNear; static unsigned lspNearN, lspNearCap, lspNearBuilds, lspNearFrames; static float lspNearE[3]; static const LspE *lspNearTab; static unsigned lspNearTabN; static int lspNearOk;
static double lspStatMs;   // служебно: время счётчиков появления в lspDraw (только smkOn), вычитается из bb_t
static float lspRangeM(void) { float r = lspConeDist > VL_DIST ? lspConeDist : VL_DIST; if (lspHaloDist > r) r = lspHaloDist; if (LSP_DIST_M > r) r = LSP_DIST_M; return r; }
static void lspNearEnsure(float ex, float ey, float ez) {
  if (!lspNearOn || !lsp.n) return;
  float dx = ex - lspNearE[0], dy = ey - lspNearE[1], dz = ez - lspNearE[2];
  if (lspNearOk && lspNearTab == lsp.e && lspNearTabN == lsp.n && dx * dx + dy * dy + dz * dz <= LSP_NEAR_MARGIN * LSP_NEAR_MARGIN) return;
  if (lspNearCap < lsp.n) { free(lspNear); lspNear = (uint16_t *)malloc(sizeof(uint16_t) * (lsp.n + 1)); lspNearCap = lspNear ? lsp.n : 0; }
  if (!lspNear) { lspNearOk = 0; return; }
  float R = lspNearOn == 2 ? 20.f : lspRangeM() + LSP_NEAR_MARGIN, R2 = R * R; lspNearN = 0;   // 2 — отрицательный контроль гейта: радиус 20 м теряет свет

  for (unsigned k = 0; k < lsp.n; k++) { const LspE *e = &lsp.e[k]; float x = e->p[0] - ex, y = e->p[1] - ey, z = e->p[2] - ez; if (x * x + y * y + z * z <= R2) lspNear[lspNearN++] = (uint16_t)k; }
  lspNearE[0] = ex; lspNearE[1] = ey; lspNearE[2] = ez; lspNearTab = lsp.e; lspNearTabN = lsp.n; lspNearOk = 1; lspNearBuilds++;
}
static inline unsigned lspNL(void) { return lspNearOn && lspNearOk ? lspNearN : lsp.n; }   // сколько записей обходить
static inline unsigned lspNK(unsigned j) { return lspNearOn && lspNearOk ? lspNear[j] : j; }   // j-я запись обхода -> номер в таблице
static void lspFrameLiveNear(void) {   // lspFrameLive (lightsprite_core.h), только по списку
  memset(lsp.live, 0, lsp.spN);
  for (unsigned j = 0; j < lspNearN; j++) { const LspE *e = &lsp.e[lspNear[j]]; const SpRun *r = &sprops.run[e->inst];
    if (r->alive && r->state == e->state0 && !memcmp(&r->x, e->snap, sizeof e->snap)) lsp.live[e->inst] = 1; }
}
static int lspDumpOnce_;
static void lspBegin(void) {
  lspFrameNo++; lspEnsure(); if (!lsp.n) return;
  if (smkOn && lspDumpOnce_ != gameLevel) { lspDumpOnce_ = gameLevel; unsigned nm_[4] = {0, 0, 0, 0}; for (unsigned k = 0; k < lsp.n; k++) nm_[lsp.e[k].mode & 3]++; char l_[120]; snprintf(l_, sizeof l_, "LSPMODES level=%d m0=%u m1=%u m2=%u m3=%u\n", gameLevel, nm_[0], nm_[1], nm_[2], nm_[3]); smkWrite(l_); }   // служебно (№913): состав таблицы по режимам (0 ореол, 1 знак, 2 конус, 3 луч)
  lspNearEnsure(eyeX, eyeY, eyeZ); lspNearFrames++;
  if (!lspOld) { if (lspNearOn && lspNearOk && lsp.live) lspFrameLiveNear(); else lspFrameLive(&lsp, &sprops); } else if (lsp.live) memset(lsp.live, 0, lsp.spN);
  memset(lspA, 0, sizeof(float) * lspArr);
}
static inline int lspOwnsBill(unsigned i, unsigned b) { return lsp.n && lspOwns(&lsp, i, b); }
static inline void lspOldShown(unsigned i, unsigned b) { int k = lsp.n ? lspEntryOf(&lsp, i, b) : -1; if (k >= 0) lspA[k] = 1.f; }
static float lspConeIn(const float v[12], float ex, float ey, float ez) {   // №703/№707: камера внутри конуса (гараж: заливка на весь экран и срез у глаза = мигание) — доля яркости по расстоянию от глаза до оси конуса
  float A_[3] = {(v[0] + v[3]) * .5f, (v[1] + v[4]) * .5f, (v[2] + v[5]) * .5f}, B_[3] = {(v[6] + v[9]) * .5f, (v[7] + v[10]) * .5f, (v[8] + v[11]) * .5f};
  float AB_[3] = {B_[0] - A_[0], B_[1] - A_[1], B_[2] - A_[2]}, AE_[3] = {ex - A_[0], ey - A_[1], ez - A_[2]}, ll_ = AB_[0] * AB_[0] + AB_[1] * AB_[1] + AB_[2] * AB_[2] + 1e-6f;
  float t_ = (AE_[0] * AB_[0] + AE_[1] * AB_[1] + AE_[2] * AB_[2]) / ll_; t_ = t_ < 0.f ? 0.f : t_ > 1.f ? 1.f : t_;
  float dx_ = ex - (A_[0] + AB_[0] * t_), dy_ = ey - (A_[1] + AB_[1] * t_), dz_ = ez - (A_[2] + AB_[2] * t_), r_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_);
  float w_ = sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2])) * .5f;
  float g_ = (r_ - LIGHTCONE_IN_A * w_) / ((LIGHTCONE_IN_B - LIGHTCONE_IN_A) * w_ + .5f); return g_ < 0.f ? 0.f : g_ > 1.f ? 1.f : g_;
}
static uint32_t vlColor(uint32_t abgr) {   // цвет света без альфы: ярче до 255 по старшему каналу, наполовину к тёплому
  float r = (float)(abgr & 0xFF), g = (float)((abgr >> 8) & 0xFF), b = (float)((abgr >> 16) & 0xFF), m = fmaxf(r, fmaxf(g, b));
  if (m < 1.f) { r = g = b = m = 255.f; }
  float s = 255.f / m; r *= s; g *= s; b *= s;
  r += (255.f - r) * VL_WARM_K; g += (236.f - g) * VL_WARM_K; b += (170.f - b) * VL_WARM_K;
  return ((unsigned)(b + .5f) << 16) | ((unsigned)(g + .5f) << 8) | (unsigned)(r + .5f);
}
static void vecLightDraw_(const uint16_t *hk, const float *hf, unsigned nh, float ex, float ey, float ez, int inR) {   // №715/736: чёткие конусы и пятна — один вызов, без текстуры
  typedef struct { unsigned c; float x, y, z; } VV;
  unsigned b0 = ggVbUsed, nv = 0; VV *q = (VV *)ggVbAlloc(nh * (12 + VL_POOL_N * 3) * sizeof(VV)); if (!q) return;
  float sw = lspConeW, sl = lspConeL; lspConeW = lspConeL = 1.f;   // размер конуса оригинала
  for (unsigned i = 0; i < nh; i++) {
    const LspE *e = &lsp.e[hk[i]]; float v[12]; lspQuad(e, ex, ey, ez, v);
    int topPlus = (v[7] + v[10]) > (v[1] + v[4]); const float *Lo = topPlus ? v + 6 : v, *Fo = topPlus ? v : v + 6;   // углы конца у лампы и у пола
    float L[3], F[3], X[3]; for (int a = 0; a < 3; a++) { L[a] = (Lo[a] + Lo[3 + a]) * .5f; F[a] = (Fo[a] + Fo[3 + a]) * .5f; X[a] = (Fo[3 + a] - Fo[a]) * .5f; }
    float g = lspConeOld ? 1.f : lspConeIn(v, ex, ey, ez), al = VL_A * hf[i] * g; uint32_t rgb = vlColor(e->abgr);
    if (al >= 1.f) {   // Роб 17.10: «к низу зафейдить» — у лампы VL_A, на середине VL_MID доли, у пола 0 (край у пола не виден и не дрожит)
      unsigned cT = ((unsigned)(al + .5f) << 24) | rgb, cM = ((unsigned)(al * VL_MID + .5f) << 24) | rgb, cB = rgb;
      float P[6][3]; for (int a = 0; a < 3; a++) { float Ma = (L[a] + F[a]) * .5f; P[0][a] = L[a] - X[a] * VL_TOPW; P[1][a] = L[a] + X[a] * VL_TOPW;
        P[2][a] = Ma - X[a] * (VL_TOPW + 1.f) * .5f; P[3][a] = Ma + X[a] * (VL_TOPW + 1.f) * .5f; P[4][a] = F[a] - X[a]; P[5][a] = F[a] + X[a]; }
      static const unsigned char tri[12] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5};
      for (int m = 0; m < 12; m++) { VV *o = &q[nv++]; int c_ = tri[m]; o->c = c_ < 2 ? cT : c_ < 4 ? cM : cB; o->x = P[c_][0]; o->y = P[c_][1]; o->z = P[c_][2]; } }
    unsigned k = hk[i]; float hw = sqrtf(X[0] * X[0] + X[2] * X[2]), pa = VL_POOL_A * hf[i];
    if (al >= 1.f) p990[2]++;   // №990: конус нарисован
    if (pool990) continue;   // №990: эллипса на полу нет ни у одной лампы, остаётся конус с затуханием вниз
    if (!haveCol || !lspPoolY || hw < .05f || pa < 1.f) continue;
    if (lspPoolY[k] > 1e29f) { float y_, n_[3]; lspPoolY[k] = colGround(&world, F[0], F[2], L[1] - .1f, F[1] - 12.f, &y_, n_) ? y_ : -1e30f;
      if (lspPoolY[k] > -1e29f) { if (lpPoolSkip(&world, F, lspPoolY[k])) { lspPoolY[k] = -1e30f; lpSkipN[1]++; } else lpDrawN++; } }   // №935: мост/эстакада — без эллипса, только конус
    float gy = lspPoolY[k]; if (gy < -1e29f) continue;
    float dxp = ex - F[0], dyp = ey - gy, dzp = ez - F[2], dd = sqrtf(dxp * dxp + dyp * dyp + dzp * dzp), kk = dd > .3f ? VL_PULL / dd : .5f; if (kk > .5f) kk = .5f;
    unsigned c = ((unsigned)(pa + .5f) << 24) | rgb; float Cc[3] = {F[0], gy + .02f, F[2]}; p990[0]++; p990Tri += VL_POOL_N;
    vlPoolTab(); for (int j = 0; j < VL_POOL_N; j++) {
      float Pp[3][3] = {{Cc[0], Cc[1], Cc[2]}, {Cc[0] + hw * vlPC_[j], Cc[1], Cc[2] + hw * vlPS_[j]}, {Cc[0] + hw * vlPC_[j + 1], Cc[1], Cc[2] + hw * vlPS_[j + 1]}};
      for (int m = 0; m < 3; m++) { VV *o = &q[nv++]; o->c = c; o->x = Pp[m][0] + (ex - Pp[m][0]) * kk; o->y = Pp[m][1] + (ey - Pp[m][1]) * kk; o->z = Pp[m][2] + (ez - Pp[m][2]) * kk; } }
  }
  lspConeW = sw; lspConeL = sl;
  if (!nv) return;
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); setCull(0); if (!inR) sceGuEnable(GU_FOG);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState(); lspDrawN++; vecLightN += nh;
}
static void vecLightDraw(const uint16_t *hk, const float *hf, unsigned nh, float ex, float ey, float ez, int inR) {   // №990: служебно — время и вызовы (Mac µs → PSP ×5,8 для оценки цены)
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; vecLightDraw_(hk, hf, nh, ex, ey, ez, inR);
  if (smkOn) { p990us[0] += sceKernelGetSystemTimeLow() - t0; p990c[0]++; p990tri[0] += p990Tri; p990Tri = 0; }
}
// №805 (Роб 18.10: «L7, последняя миссия, бейсбольное поле F13-9 — свет размытый; наш векторный конус с фейдом вниз и кружок на
// поле»): луч летающей тарелки (UfoBeam, art/missions/level07/ufobeam.p3d — билборды к камере сложением, размер и цвет по кадрам
// клипа) — не размытой картинкой, а как векторный свет (vecLightDraw): трапеция ровного цвета (верх — VL_TOPW ширины, на середине
// VL_MID яркости, книзу в ноль) и эллипс на земле под лучом. Размер, место и цвет — кадр клипа данных. VEC_BEAM_OLD=1 — картинка
#ifndef VEC_BEAM_OLD
#define VEC_BEAM_OLD 0
#endif
static unsigned vecBeamN;
// №995 (Роб после TEST70: «у тарелки НЛО на L7 стоит блинк, надо просто цилиндр света, можно чуть конусообразный, как будто НЛО светит сверху вниз»): вместо билбордов луча UfoBeam
// (квадратные размытые кадры клипа и трапеция с эллипсом, на железе луча почти не видно) — один полупрозрачный усечённый конус от днища тарелки до земли: у днища уже, книзу шире до зоны заряда луча
// (BEAM995_RBOT = радиус зоны BOSS967_R), яркость у днища BEAM995_A, на середине BEAM995_AMID доли, у земли BEAM995_ABOT доли; боковая поверхность без текстуры, один вызов, по BEAM995_SIDES граней.
// BEAM995_OLD=1 (в игре файл beam995old или «flag beam995 0») — прежнее: билборды луча с эллипсом
#ifndef BEAM995_OLD
#define BEAM995_OLD 0
#endif
#ifndef BEAM995_SIDES
#define BEAM995_SIDES 14
#endif
#ifndef BEAM995_RTOP
#define BEAM995_RTOP 7.f       // радиус у днища, м
#endif
#ifndef BEAM995_RBOT
#define BEAM995_RBOT 11.f     // радиус у земли, м: зона заряда BOSS967_R (game_msn.h)
#endif
#ifndef BEAM995_DROP
#define BEAM995_DROP 1.5f     // верх конуса ниже центра тарелки, м
#endif
#ifndef BEAM995_A
#define BEAM995_A 110.f       // альфа у днища (из 255)
#endif
#ifndef BEAM995_AMID
#define BEAM995_AMID .65f     // яркость на середине — доля от яркости у днища
#endif
#ifndef BEAM995_ABOT
#define BEAM995_ABOT .55f    // яркость у земли — доля от яркости у днища
#endif
// №1036 (Роб на PSP, TEST72: «цилиндр у тарелки белый, фосфорно-неоновый»): цвет луча — не бледно-мятный из vlColor (ярче до 255 и наполовину к тёплому белому: (234,255,200)), а родной зелёный UfoBeam (квады ufobeam.p3d:
// (124,255,134), (165,213,168), (58,191,53) — зелёный), мягче: BEAM1036_RGB и прозрачность BEAM1036_A; смешивание обычное (SRC_ALPHA / 1 − SRC_ALPHA, не сложение), затухание к земле и форма конуса те же. BEAM1036_OLD=1 / файл beam1036old / «flag beam1036 0» — как в TEST72
#ifndef BEAM1036_OLD
#define BEAM1036_OLD 0
#endif
#ifndef BEAM1036_A
#define BEAM1036_A 100.f
#endif
#define BEAM1036_RGB 0x005fc85au   // ABGR без альфы: R 90, G 200, B 95
static int beam1036 = !BEAM1036_OLD;
static int beam995 = !BEAM995_OLD; static uint32_t b995Rgb = 0x00c8ffeau; static unsigned b995Draws, b995Tri, b995Calls, b995Us, b995UsOld; static float b995Top, b995Gy;
static int beam995Draw_(const float ufo[3], float ex, float ey, float ez) {   // 1 — луч нарисован (или тарелка не над землёй), картинку луча не рисовать; 0 — рисовать как было
  (void)ex; (void)ey; (void)ez;
  if (!beam995 || !vecLight) return 0;
  float gy, top = ufo[1] - BEAM995_DROP; static float gyP[3] = {1e30f, 0.f, 0.f}, gyV, csT[BEAM995_SIDES + 1], snT[BEAM995_SIDES + 1]; static int tabOk;
  if (gyP[0] != ufo[0] || gyP[1] != ufo[1] || gyP[2] != ufo[2]) { float n_[3]; gyV = (haveCol && colGround(&world, ufo[0], ufo[2], top - .5f, top - 90.f, &gy, n_)) ? gy : top - 14.f; memcpy(gyP, ufo, sizeof gyP); }   // земля под тарелкой — раз на место (тарелка стоит)
  gy = gyV; if (!tabOk) { for (int i = 0; i <= BEAM995_SIDES; i++) { csT[i] = cosf(6.2831853f * i / BEAM995_SIDES); snT[i] = sinf(6.2831853f * i / BEAM995_SIDES); } tabOk = 1; }
  if (gy > top - 2.f) return 1;
  typedef struct { unsigned c; float x, y, z; } BV3;
  const int NS = BEAM995_SIDES; unsigned b0 = ggVbUsed, nv = 0; BV3 *q = (BV3 *)ggVbAlloc(NS * 12 * sizeof(BV3)); if (!q) return 1;
  float a0_ = beam1036 ? BEAM1036_A : BEAM995_A, yy[3] = {top, top + (gy - top) * .4f, gy}, rr[3], aa[3] = {a0_, a0_ * BEAM995_AMID, a0_ * BEAM995_ABOT};
  for (int i = 0; i < 3; i++) rr[i] = BEAM995_RTOP + (BEAM995_RBOT - BEAM995_RTOP) * (yy[i] - top) / (gy - top);
  unsigned cc[3]; for (int i = 0; i < 3; i++) cc[i] = ((unsigned)(aa[i] + .5f) << 24) | (beam1036 ? BEAM1036_RGB : b995Rgb);
  for (int s = 0; s < NS; s++) { float c0 = csT[s], s0 = snT[s], c1 = csT[s + 1], s1 = snT[s + 1];
    for (int i = 0; i < 2; i++) { BV3 *o = q + nv;
      float x0a = ufo[0] + rr[i] * c0, z0a = ufo[2] + rr[i] * s0, x1a = ufo[0] + rr[i] * c1, z1a = ufo[2] + rr[i] * s1, x0b = ufo[0] + rr[i + 1] * c0, z0b = ufo[2] + rr[i + 1] * s0, x1b = ufo[0] + rr[i + 1] * c1, z1b = ufo[2] + rr[i + 1] * s1;
      o[0] = (BV3){cc[i], x0a, yy[i], z0a}; o[1] = (BV3){cc[i], x1a, yy[i], z1a}; o[2] = (BV3){cc[i + 1], x0b, yy[i + 1], z0b};
      o[3] = (BV3){cc[i], x1a, yy[i], z1a}; o[4] = (BV3){cc[i + 1], x1b, yy[i + 1], z1b}; o[5] = (BV3){cc[i + 1], x0b, yy[i + 1], z0b}; nv += 6; } }
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); setCull(0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState(); b995Draws++; b995Tri += nv / 3; b995Top = top; b995Gy = gy;
  return 1;
}
static int beam995Draw(const float ufo[3], float ex, float ey, float ez) {   // служебно: время (Mac µs) и вызовы за кадр
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int r = beam995Draw_(ufo, ex, ey, ez);
  if (smkOn) { b995Us += sceKernelGetSystemTimeLow() - t0; b995Calls++; }
  return r;
}
static int vecBeamHook_(const float p[3], const float X[3], const float Y[3], unsigned abgr);
static int vecBeamHook(const float p[3], const float X[3], const float Y[3], unsigned abgr) {   // служебно: время старого пути (билборды луча) за кадр
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int r = vecBeamHook_(p, X, Y, abgr);
  if (smkOn && !beam995) b995UsOld += sceKernelGetSystemTimeLow() - t0;
  return r;
}
static int vecBeamHook_(const float p[3], const float X[3], const float Y[3], unsigned abgr) {
  if (VEC_BEAM_OLD || !vecLight) return 0;
  if (smkOn) { static uint32_t seen_[8]; static int ns_; int k_ = 0; for (; k_ < ns_; k_++) if (seen_[k_] == abgr) break; if (k_ == ns_ && ns_ < 8) { seen_[ns_++] = abgr; char l_[120]; snprintf(l_, sizeof l_, "UFOBEAMCOL abgr=%08x hw=%.2f hh=%.2f t=%.1f\n", abgr, sqrtf(X[0] * X[0] + X[1] * X[1] + X[2] * X[2]), sqrtf(Y[0] * Y[0] + Y[1] * Y[1] + Y[2] * Y[2]), smkT); smkWrite(l_); } }   // №1036 служебно: исходные цвета квадов луча
  if (beam995) { b995Rgb = vlColor(abgr & 0xffffffu); return 1; }   // №995: квады луча (блинк, размытые квадраты) не рисуются — свет даёт конус beam995Draw
  float ya = (abgr >> 24) / 255.f; if (ya < .01f) return 1;
  { float hwq = X[0] * X[0] + X[1] * X[1] + X[2] * X[2], hhq = Y[0] * Y[0] + Y[1] * Y[1] + Y[2] * Y[2];
    if (hhq < hwq * 1.69f) return 1; }   // квадратное размытое свечение луча (UfoBeam: 9–31 м по кадрам) — не рисуется: свет луча — конус и эллипс высокого квада
  if (smkOn && !(vecBeamN & 63)) { char s_[160]; snprintf(s_, sizeof s_, "VECBEAM n=%u p=%.1f,%.1f,%.1f hw=%.2f hh=%.2f a=%.2f abgr=%08x t=%.1f\n", vecBeamN, p[0], p[1], p[2],
    sqrtf(X[0] * X[0] + X[1] * X[1] + X[2] * X[2]), sqrtf(Y[0] * Y[0] + Y[1] * Y[1] + Y[2] * Y[2]), ya, abgr, smkT); smkWrite(s_); }   // служебно: где луч (раз в 64 вызова)
  typedef struct { unsigned c; float x, y, z; } BV2;
  unsigned b0 = ggVbUsed, nv = 0; BV2 *q = (BV2 *)ggVbAlloc((12 + VL_POOL_N * 3) * sizeof(BV2)); if (!q) return 1;
  float T[3], B[3]; for (int a = 0; a < 3; a++) { T[a] = p[a] + Y[a]; B[a] = p[a] - Y[a]; } if (T[1] < B[1]) { float t_[3]; memcpy(t_, T, sizeof t_); memcpy(T, B, sizeof T); memcpy(B, t_, sizeof B); }
  uint32_t rgb = vlColor(abgr & 0xffffffu); float al = VL_A * ya;
  unsigned cT = ((unsigned)(al + .5f) << 24) | rgb, cM = ((unsigned)(al * VL_MID + .5f) << 24) | rgb, cB = rgb;
  float P[6][3]; for (int a = 0; a < 3; a++) { float Ma = (T[a] + B[a]) * .5f; P[0][a] = T[a] - X[a] * VL_TOPW; P[1][a] = T[a] + X[a] * VL_TOPW;
    P[2][a] = Ma - X[a] * (VL_TOPW + 1.f) * .5f; P[3][a] = Ma + X[a] * (VL_TOPW + 1.f) * .5f; P[4][a] = B[a] - X[a]; P[5][a] = B[a] + X[a]; }
  static const unsigned char tri[12] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5};
  for (int m = 0; m < 12; m++) { BV2 *o = &q[nv++]; int c_ = tri[m]; o->c = c_ < 2 ? cT : c_ < 4 ? cM : cB; o->x = P[c_][0]; o->y = P[c_][1]; o->z = P[c_][2]; }
  { float gy, n_[3], hw = sqrtf(X[0] * X[0] + X[2] * X[2]);   // эллипс на земле под лучом
    if (haveCol && hw > .05f && colGround(&world, B[0], B[2], B[1] + 2.f, B[1] - 30.f, &gy, n_)) {
      float ex = eyeX, ey = eyeY, ez = eyeZ, dxp = ex - B[0], dyp = ey - gy, dzp = ez - B[2], dd = sqrtf(dxp * dxp + dyp * dyp + dzp * dzp), kk = dd > .3f ? VL_PULL / dd : .5f; if (kk > .5f) kk = .5f;
      unsigned c = ((unsigned)(VL_POOL_A * ya + .5f) << 24) | rgb; float Cc[3] = {B[0], gy + .02f, B[2]};
      vlPoolTab(); for (int j = 0; j < VL_POOL_N; j++) {
        float Pp[3][3] = {{Cc[0], Cc[1], Cc[2]}, {Cc[0] + hw * vlPC_[j], Cc[1], Cc[2] + hw * vlPS_[j]}, {Cc[0] + hw * vlPC_[j + 1], Cc[1], Cc[2] + hw * vlPS_[j + 1]}};
        for (int m = 0; m < 3; m++) { BV2 *o = &q[nv++]; o->c = c; o->x = Pp[m][0] + (ex - Pp[m][0]) * kk; o->y = Pp[m][1] + (ey - Pp[m][1]) * kk; o->z = Pp[m][2] + (ez - Pp[m][2]) * kk; } } } }
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); setCull(0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState(); vecBeamN++;
  return 1;
}
// №793 (Роб 18.10: «свечение под персонажами с миссией "!" — векторное на всех уровнях; найти другие размытые свечения»): пятно
// «<рисунок>_shadow» на земле — синий диск под персонажем бонус-миссии/гонки (exclamation/dice/checkered_shadow), голубое кольцо
// сферы цели (carsphere/triggersphere_shadow) — не размытой картинкой, а чёткими кольцами: две ступени яркости, замеренные по
// картинке данных (msnart.c msnartFlat: ≥ 50 % и ≥ 12 % пика), цвет и сложение — как у оригинала; ближе к глазу на VL_PULL
// (не тонет в полу), как пятна векторного света. VEC_GLOW_OLD=1 — прежняя картинка
#ifndef VEC_GLOW_OLD
#define VEC_GLOW_OLD 0
#endif
#define VG_N 28   // сторон круга
#ifndef VG_COST852_OLD
#define VG_COST852_OLD 0
#endif
#define VG_N852 18     // №852: сторон круга
#define VG_LOD_D 25.f  // №852: дальше — только внутренний диск
#include "glintmotion976.h"   // №976: SPHERE976_N
static unsigned vecGlowN;   // пятен за всё время (служебная строка смоука)
static unsigned vgStatN, vgStatUs, vgStatV;   // №849 служебно: пятна на кадр (строка GLINTSTAT)
static int vecGlowHook(const char *name, const float *p, float ex, float ey, float ez) {
  if (VEC_GLOW_OLD || !vecLight) return 0;
  unsigned vg0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; vgStatN++;
  MsnartFlat f; if (!msnartFlat(name, &f)) return 0;
  if (geOver(GE_SOFT)) return 1;
  // №852 (тур TEST41 на PSP, L3 в езде: значки бонусных персонажей 0,4–0,9 мс кадра — пятно на полу до 420 вершин на процессоре): пятно вне кадра
  // не считается; круг — VG_N852 сторон; дальше VG_LOD_D — только внутренний диск. VG_COST852_OLD=1 — как было
  int vgN_ = VG_COST852_OLD ? VG_N : VG_N852, far_ = 0, sph_ = !fxGlint976Old && !VG_COST852_OLD && strstr(name, "sphere");   // №976
  if (!VG_COST852_OLD) { float e3_[3] = {ex, ey, ez}; if (rfSphereOut(pl, e3_, p, f.rad * f.out1 + .5f)) return 1;
    float dx_ = p[0] - ex, dz_ = p[2] - ez; far_ = dx_ * dx_ + dz_ * dz_ > VG_LOD_D * VG_LOD_D; if (sph_ && !far_) vgN_ = SPHERE976_N; }
  if (smkOn && strstr(name, "sphere")) { static int ln_ = -1; if (ln_ != vgN_ * 2 + far_) { ln_ = vgN_ * 2 + far_; char l_[96]; snprintf(l_, sizeof l_, "SPHERE976 name=%s sides=%d far=%d old=%d\n", name, vgN_, far_, fxGlint976Old); smkWrite(l_); } }   // №976 служебно
  typedef struct { unsigned c; float x, y, z; } GV;
  unsigned b0 = ggVbUsed, nv = 0; GV *q = (GV *)ggVbAlloc(3 * vgN_ * 6 * sizeof(GV)); if (!q) return 1;
  float dxp = ex - p[0], dyp = ey - p[1], dzp = ez - p[2], dd = sqrtf(dxp * dxp + dyp * dyp + dzp * dzp), kk = dd > .3f ? VL_PULL / dd : .5f; if (kk > .5f) kk = .5f;
  const float band[3][2] = {{f.in0, f.in1}, {f.out0, f.in0}, {f.in1, f.out1}}; const unsigned col[3] = {f.colIn, f.colOut, f.colOut};
  for (int k = 0; k < 3; k++) {
    if (far_ && k) break;   // №852: вдали — только внутренний диск
    float r0 = band[k][0] * f.rad, r1 = band[k][1] * f.rad; if (r1 - r0 < .01f || !(col[k] >> 24)) continue;
    for (int j = 0; j < vgN_; j++) {
      float c0, s0, c1, s1;
      if (!VG_COST852_OLD) {   // №852: углы круга — таблица (sinf/cosf на PSP медленные: было 4 на отрезок, до 216 на пятно)
        static float vgC_[VG_N852 + 1], vgS_[VG_N852 + 1], vgC6_[SPHERE976_N + 1], vgS6_[SPHERE976_N + 1]; static int vgOk_;
        if (!vgOk_) { for (int i_ = 0; i_ <= VG_N852; i_++) { vgC_[i_] = cosf(6.2831853f * i_ / VG_N852); vgS_[i_] = sinf(6.2831853f * i_ / VG_N852); }
          for (int i_ = 0; i_ <= SPHERE976_N; i_++) { vgC6_[i_] = cosf(6.2831853f * i_ / SPHERE976_N); vgS6_[i_] = sinf(6.2831853f * i_ / SPHERE976_N); } vgOk_ = 1; }
        if (vgN_ == SPHERE976_N) { c0 = vgC6_[j]; s0 = vgS6_[j]; c1 = vgC6_[j + 1]; s1 = vgS6_[j + 1]; }
        else { c0 = vgC_[j]; s0 = vgS_[j]; c1 = vgC_[j + 1]; s1 = vgS_[j + 1]; } }
      else { float a0 = 6.2831853f * j / vgN_, a1 = 6.2831853f * (j + 1) / vgN_; c0 = cosf(a0); s0 = sinf(a0); c1 = cosf(a1); s1 = sinf(a1); }
      float P[4][3] = {{p[0] + r0 * c0, p[1], p[2] + r0 * s0}, {p[0] + r1 * c0, p[1], p[2] + r1 * s0}, {p[0] + r0 * c1, p[1], p[2] + r0 * s1}, {p[0] + r1 * c1, p[1], p[2] + r1 * s1}};
      static const unsigned char ring[6] = {0, 1, 2, 2, 1, 3}, disc[3] = {0, 1, 3};
      const unsigned char *tri = r0 < .005f ? disc : ring; int nt = r0 < .005f ? 3 : 6;
      for (int m = 0; m < nt; m++) { GV *o = &q[nv++]; const float *v = P[tri[m]]; o->c = col[k];
        o->x = v[0] + (ex - v[0]) * kk; o->y = v[1] + (ey - v[1]) * kk; o->z = v[2] + (ez - v[2]) * kk; } }
  }
  if (!nv) return 1;
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);   // сложение, как билборд оригинала (blend 2)
  sceGuDepthMask(GU_TRUE); setCull(0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuEnable(GU_TEXTURE_2D); resetState(); vecGlowN++;
  if (smkOn) { vgStatUs += sceKernelGetSystemTimeLow() - vg0_; vgStatV += nv; }
  return 1;
}
static void lspDraw(float ex, float ey, float ez, int inR) {
  if (!lsp.n) return;
  typedef struct { float u, v; unsigned c; float x, y, z; } BV;
  unsigned bb0 = ggVbUsed; int open = 0; int curTex = -2; unsigned nq = 0; BV *q = 0; int bindTex = -1;
  float e3[3] = {ex, ey, ez};
  uint16_t hk_[VL_MAX]; float hf_[VL_MAX]; unsigned nh_ = 0;   // №715/736: чёткие конусы кадра
  float cdS_ = lspConeDist, cfS_ = lspConeFade; int cmS_ = lspConeMax; if (vecLight) { lspConeDist = VL_DIST; lspConeFade = VL_FADE; lspConeMax = VL_MAX; }   // вернуть в конце
  float coneT = 1e30f, haloT = 1e30f;   // пороги d² для «не больше LIGHTCONE_MAX / LIGHTHALO_MAX ближних»
  lspNearEnsure(ex, ey, ez); unsigned nL_ = lspNL();   // №913: обход по списку записей в радиусе света (LSP_NEAR_OLD — все)
  if (smkOn) { smkLspList += nL_; smkLspAll += lsp.n; }
  if (!lspConeOld && lspConeMax > 0 && !lspOld) {
    static float cd_[1024]; unsigned nc_ = 0;
    for (unsigned j_ = 0; j_ < nL_ && nc_ < 1024; j_++) { unsigned k = lspNK(j_); const LspE *e = &lsp.e[k]; if (e->mode != 2 || !lsp.live[e->inst]) continue;
      float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz; if (lspConeFadeAt(d2) <= 0.f) continue;
      if (rfSphereOut(pl, e3, e->p, (e->hw > e->hh ? e->hw : e->hh) * 1.42f * lspConeW + .5f)) continue; cd_[nc_++] = d2; }
    if ((int)nc_ > lspConeMax) { qsort(cd_, nc_, sizeof(float), lspCmpF); coneT = cd_[lspConeMax - 1]; }
    nc_ = 0;
    for (unsigned j_ = 0; j_ < nL_ && nc_ < 1024; j_++) { unsigned k = lspNK(j_); const LspE *e = &lsp.e[k]; if (e->mode > 1 || !lsp.live[e->inst]) continue;
      float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz; if (lspHaloFadeAt(d2) <= 0.f) continue;
      if (rfSphereOut(pl, e3, e->p, (e->hw > e->hh ? e->hw : e->hh) * 1.42f + .5f)) continue; cd_[nc_++] = d2; }
    if ((int)nc_ > lspHaloMax) { qsort(cd_, nc_, sizeof(float), lspCmpF); haloT = cd_[lspHaloMax - 1]; }
  }
  for (unsigned j_ = 0; j_ < nL_; j_++) {
    unsigned k = lspNK(j_); const LspE *e = &lsp.e[k];
    if (lspOld || !lsp.live[e->inst]) continue;
    if (slLightOn && (e->mode < 2 || (e->mode == 3 && gameLevel != 7))) continue;   // №658 (PS2: у фонарей ни ореола, ни пятна): при lightspr 1 точечные ореолы и знаки (mode 0/1) не рисуются; остаются конусы (mode 2: гараж АЭС) и на L7 лучи mode 3 (столб НЛО); зелёных лучей снаружи АЭС у PS2 нет (kadры a_02/a_04/b_18)
    float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
    float f = e->mode == 2 ? lspConeFadeAt(d2) : e->mode < 2 ? lspHaloFadeAt(d2) : lspFadeAt(d2); if (f <= 0.f) continue;
    if ((e->mode == 2 && d2 > coneT) || (e->mode < 2 && d2 > haloT)) continue;
    float rad = (e->hw > e->hh ? e->hw : e->hh) * 1.42f + .5f;
    if (rfSphereOut(pl, e3, e->p, rad)) { lspA[k] = f; continue; }   // вне кадра: alpha ведём для счётчика, не рисуем
    uint32_t col = lspFadeCol(e->abgr, f); if (e->mode == 2 && lspConeA != 1.f) { unsigned a_ = (unsigned)((float)(col >> 24) * lspConeA + .5f); col = (col & 0x00FFFFFFu) | ((a_ > 255u ? 255u : a_) << 24); } if (!(col >> 24)) continue;
    if (geOver(GE_SOFT)) break;
    lspA[k] = f;
    if (dripPtN) { float dv_[12]; lspQuad(e, ex, ey, ez, dv_); if (dripSkipBill(dv_, 0)) continue; }   // №770: струя из трубы — столбом (drip770.h)
    if (vecLight && (e->mode == 2 || e->mode == 3)) { if (nh_ < VL_MAX) { hk_[nh_] = (uint16_t)k; hf_[nh_] = f; nh_++; } continue; }   // №715/736: чёткий конус (и луч L7) — ниже одним вызовом
    if (e->tex != curTex) {   // смена текстуры: сбросить пачку, привязать новую
      if (nq) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); lspDrawN++; nq = 0; }
      curTex = e->tex; q = 0;
    }
    if (!open) {
      open = 1;
      ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
      sceGuSetMatrix(GU_MODEL, &I); sceGuTexScale(1.f, 1.f);
      sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); curFlags = -1; setCull(0);
      sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      if (!glowFogOld) sceGuDisable(GU_FOG);
    }
    if (bindTex != curTex) { bindTexLean(curTex); bindTex = curTex; lspBindN++; }
    if (!q) {   // пачка: места на все записи этой текстуры подряд (не больше, чем осталось в таблице)
      unsigned run = 0; for (unsigned j = j_; j < nL_ && lsp.e[lspNK(j)].tex == curTex; j++) run++;
      q = (BV *)ggVbAlloc(run * 6 * sizeof(BV)); nq = 0;
    }
    float v[12]; lspQuad(e, ex, ey, ez, v);
    if (e->mode == 2 && !lspConeOld) {   // №703/№707: камера внутри конуса (гараж: заливка на весь экран и срез у глаза = мигание) — гасить по расстоянию от глаза до оси конуса
      float g_ = lspConeIn(v, ex, ey, ez);
      if (g_ < 1.f) { unsigned a_ = (unsigned)((float)(col >> 24) * g_ + .5f); col = (col & 0x00FFFFFFu) | (a_ << 24); if (!a_) for (int m_ = 3; m_ < 12; m_++) v[m_] = v[m_ % 3]; }
    }
    static const unsigned char ord[6] = {0, 1, 2, 2, 1, 3};
    BV *o = q + nq * 6;
    for (int m = 0; m < 6; m++) { int c = ord[m]; o[m].u = (float)(c & 1); o[m].v = spBillTexV(c, spBillUvOld); o[m].c = col; o[m].x = v[3 * c]; o[m].y = v[3 * c + 1]; o[m].z = v[3 * c + 2]; }
    nq++; lspQuadN++;
    if (smkOn) { float w_ = sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2])), h_ = sqrtf((v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2])); float fp_ = 136.f / tanf(curFovY * .5f * GU_PI / 180.f), a_ = w_ * h_ * fp_ * fp_ / (d2 + 1.f); lspAreaSum += a_ > 130560.f ? 130560.f : a_; }
    if (j_ + 1 >= nL_ || lsp.e[lspNK(j_ + 1)].tex != curTex) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); lspDrawN++; nq = 0; q = 0; }
  }
  if (nq) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nq * 6, 0, q); lspDrawN++; }
  if (open) { ggVbWriteback(bb0); sceGuTexScale(128.f, 128.f); if (!inR) sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); resetState(); }
  if (nh_) vecLightDraw(hk_, hf_, nh_, ex, ey, ez, inR);
  lspConeDist = cdS_; lspConeFade = cfS_; lspConeMax = cmS_;
  // счётчики появления: запись, которой не было в кадре (alpha 0) и вдруг есть (скачок alpha >= 0,35), а в прошлом кадре её место было в поле зрения — щелчок
  if (!smkOn) return;
  unsigned st0_ = sceKernelGetSystemTimeLow();
  if (lspNearOn && lspNearOk) {   // №913 сверка (только смоук): запись вне списка не должна иметь ненулевое затухание — иначе список потерял бы свет (lsp_viol в PHSUB; «flag lspnear 2» даёт нарушения)
    unsigned jn_ = 0, viol_ = 0;
    for (unsigned k = 0; k < lsp.n; k++) { if (jn_ < lspNearN && lspNear[jn_] == k) { jn_++; continue; }
      const LspE *e = &lsp.e[k]; float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
      float f = e->mode == 2 ? lspConeFadeAt(d2) : e->mode < 2 ? lspHaloFadeAt(d2) : lspFadeAt(d2); if (f > 0.f) viol_++; }
    smkLspViol += viol_; }
  int cont = lspLastFrame && lspFrameNo == lspLastFrame + 1; lspLastFrame = lspFrameNo; lspFramesN++;
  for (unsigned k = 0; k < lsp.n; k++) {
    const LspE *e = &lsp.e[k];
    float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz;
    int inFr = !rfSphereOut(pl, e3, e->p, (e->hw > e->hh ? e->hw : e->hh) * 1.42f + .5f) && d2 < 140.f * 140.f;
    if (lspA[k] > 0.f) lspShownSum++;
    if (cont && lspFr[k] && lspA[k] - lspAp[k] >= .35f) { lspPopN++; if (d2 < 30.f * 30.f) lspPopNear++; }
    if (cont && fabsf(lspA[k] - lspAp[k]) > .12f && lspAp[k] > 0.f && lspA[k] > 0.f) lspAlphaJumps++;
    lspAp[k] = lspA[k]; lspFr[k] = (uint8_t)inFr;
  }
  lspStatMs += (sceKernelGetSystemTimeLow() - st0_) / 1000.0;
}
// №660 (Роб: «гараж АЭС светлее»; эталон PS2 Стенда2: яркость кадра 40–58 из 255 против 32–36 у нас): внутри зала с лампами-конусами (таблица №599, mode 2) к кадру
// прибавляется ровный свет зеленоватого оттенка — один полноэкранный аддитивный прямоугольник без текстуры (около 0,5 мс заливки на PSP). Сила — flag garagelift N (доля 0–255
// белого, 0 — выкл, по умолчанию выкл), плавно по расстоянию до ближней лампы: полная в 16 м, ноль в 30 м. GARAGE_LIFT_OLD=1 / flag garagelift 0 — как было
static int garageLiftN;
static void garageLiftDraw(float ex, float ey, float ez) {
  if (!garageLiftN || !lsp.n) return;
  float dm = 1e9f;
  for (unsigned k = 0; k < lsp.n; k++) { const LspE *e = &lsp.e[k]; if (e->mode != 2) continue;
    float dx = e->p[0] - ex, dy = e->p[1] - ey, dz = e->p[2] - ez, d2 = dx * dx + dy * dy + dz * dz; if (d2 < dm) dm = d2; }
  float d = sqrtf(dm), f = (30.f - d) / 14.f;
  if (smkOn) { static unsigned gl_n; if (!(gl_n++ & 63)) { unsigned n2 = 0; for (unsigned k = 0; k < lsp.n; k++) n2 += lsp.e[k].mode == 2; char s_[120]; snprintf(s_, sizeof s_, "GARAGELIFT d=%.1f f=%.2f n=%d mode2=%u\n", d, f, garageLiftN, n2); smkWrite(s_); } }
  if (f <= 0.f) return; if (f > 1.f) f = 1.f; f = f * f * (3.f - 2.f * f);
  float a = (float)garageLiftN * f; unsigned g = (unsigned)(a + .5f), r = (unsigned)(a * .82f + .5f), b = (unsigned)(a * .78f + .5f); if (g > 255u) g = 255u; if (r > 255u) r = 255u; if (b > 255u) b = 255u;
  unsigned col = 0xff000000u | (b << 16) | (g << 8) | r;
  typedef struct { unsigned c; float x, y, z; } LV;
  LV *v = (LV *)sceGuGetMemory(4 * sizeof(LV));
  v[0].c = v[1].c = v[2].c = v[3].c = col; v[0].z = v[1].z = v[2].z = v[3].z = 0.f;
  v[0].x = 0.f; v[0].y = 0.f; v[1].x = 480.f; v[1].y = 0.f; v[2].x = 0.f; v[2].y = 272.f; v[3].x = 480.f; v[3].y = 272.f;
  int fogWas_ = sceGuGetStatus(GU_FOG);   // проверка 18.10: дымку — обратно
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE);
  sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, col, 0xffffffffu);
  sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, v);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuEnable(GU_TEXTURE_2D); sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE); if (fogWas_) sceGuEnable(GU_FOG); resetState();
}
static void lspStatLine(void) {   // из smkStat: строка на сцену
  if (!smkOn || !lspFramesN) { lspFramesN = lspShownSum = lspPopN = lspPopNear = lspDrawN = lspBindN = lspQuadN = lspAlphaJumps = 0; lspAreaSum = 0; return; }
  char s[300]; double k = 1.0 / lspFramesN;
  snprintf(s, sizeof s, "LSP scene=%s old=%d entries=%u frames=%u shown=%.1f pops=%u pops_near=%u alpha_jumps=%u quads=%.1f draws=%.1f binds=%.1f area=%.1f%%\n", smkScene, lspOld, lsp.n, lspFramesN,
           lspShownSum * k, lspPopN, lspPopNear, lspAlphaJumps, lspQuadN * k, lspDrawN * k, lspBindN * k, lspAreaSum * k / 130560.0 * 100.0);
  smkWrite(s); lspFramesN = lspShownSum = lspPopN = lspPopNear = lspDrawN = lspBindN = lspQuadN = lspAlphaJumps = 0; lspAreaSum = 0;
}
