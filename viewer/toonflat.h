// №912 «Мультик без текстур» (Роб 23.10: «основу игры делаем мультик и стараемся убрать все текстуры, в том числе асфальта; генеративный шум, который весит меньше текстуры,
// но даёт рябь, похожую на асфальт; в меню — возможность вернуть»). Поверхность мира (город, земля, предметы) рисуется БЕЗ текстуры: цвет заливки — преобладающий тон
// текстуры (по чистым участкам палитры, без шумовых точек), через ту же цветокоррекцию мультика (toneColorRGB, палитра №825). Цвет вершин (запечённый свет) остаётся,
// заливка множится на него так же, как раньше множилась текстура: GE считает свет (GU_LIGHTING без источников) — окружающий цвет модели = заливка, материал ambient = цвет
// вершины (sceGuColorMaterial(GU_AMBIENT)): результат = заливка × цвет вершины, привязки текстуры нет. Земля (метка ground) — вместо картинки мелкая процедурная
// текстура шума (64×64 T8 = 4 КБ с мипами, строится кодом при запуске; UV те же, что у родной текстуры), заливка множится на неё.
// Исключения (остаются с текстурой): рисунок (второй цвет заметной доли и контраста, у земли строже — разметка), прозрачность в палитре, текстуры из списка notoon (вывески),
// куски с прозрачностью/сложением/небо/машины и всё, что не из таблицы текстур уровня (персонажи, интерфейс, трафик).
// TOON_FLAT912_OLD=1 / файл flat912old рядом с EBOOT / «flag flat912 0» — как до правки
#ifndef TOON_FLAT912_OLD
#define TOON_FLAT912_OLD 0
#endif
#ifndef TOON_FLAT_NOISE_OLD
#define TOON_FLAT_NOISE_OLD 0   // 1 — земля с родной текстурой (только заливка остальных)
#endif
#define FLAT_BETA_S0 .95f   // доля преобладающего тона, выше которой тон чистый (без примеси среднего)
#define FLAT_BETA_K 1.80f
#define FLAT_BETA_MAX .80f
#ifndef FLAT_EDGE_WALL
#define FLAT_EDGE_WALL .035f
#endif
#ifndef FLAT_EDGE_GROUND
#define FLAT_EDGE_GROUND .025f
#endif
// TOON_NOISE_WORLD=1: шум земли в мировых координатах (текстурная матрица GE из положения вершины). В безголовом PPSSPP такая привязка даёт постоянную точку текстуры (проверено шахматкой: узора нет),
// на железе не проверена, поэтому по умолчанию шум идёт по родным UV куска (растянут вдоль дороги, как родной рисунок)
#ifndef TOON_NOISE_WORLD
#define TOON_NOISE_WORLD 0
#endif
#define NOISE_TEX_ID 0x3E00   // 0x3E01.. — рисунок земли №969 по классам (газон, тротуар)
#include "ground969.h"
#define FLAT_EL_OK 1
#define FLAT_EL_BAD 2
static int flatTexUser;         // игрок вернул текстуры (Options → Textures: On)
static int marksOk;             // разметка дорог: файл marks912.bin открыт (marksLoad, main_levels.h)
static unsigned flatSig;        // подпись текстур кусков текущего уровня (как у notoon.txt)
static unsigned flatLevelGen;   /* счётчик загрузок уровня (flatListLoad): пересборка заливки один раз на уровень */
static int flatRebuildOld;      /* файл flatrebuildold: без пересборки после загрузки (А без исправления; красный контроль гейта) */
static unsigned flatLevelN;     /* число текстур самого уровня (дальше в tex[] — машины трафика и слоты миссий: заливки у них нет) */
static int flatRescanOld, flatDump;   /* flatRescanOld — прежний опрос всех текстур (файл flatrescanold); flatDump — дамп FDIAG/FLATX/FLATT (файл flatdump, «flag flatdump 1»), по умолчанию выключен */
static int flatFromFile;        // решение по текстурам прочитано из flat912.bin (tools/flat_list.py) — движок их не разбирает
static int flatOn;              // заливка включена сейчас (Cartoon и «Textures: Off»)
static int flatOld = TOON_FLAT912_OLD, flatNoiseOn = !TOON_FLAT_NOISE_OLD;
static unsigned flatTintKey = ~0u, flatTintN; static unsigned flatTint[1024], flatQ[1024], flatExact[1024];   // flatQ — грубый ключ порядка кусков (4 бита на канал), flatExact — точный ключ состояния заливки (24 бита тона)
static unsigned flatKinds[3];   // служебное: сколько текстур в каждом классе (для строки FLAT)
static Tex noiseTex; static unsigned char noiseBuf[8192] __attribute__((aligned(64))), noisePal[1024] __attribute__((aligned(64))); static int noiseReady;
static float noiseMean = .92f;  // среднее значение шума: заливку множат на 1/noiseMean, чтобы тон не темнел
// №949 (Роб, TEST59 на PSP: «шум газона и бордюров на L6 E9-5 растянули во всю поверхность — везде пятна как мокрые лужи»): шум шёл по родным UV куска, а у газона и бордюров родная
// текстура растянута на 10–90 м (мало повторов UV на метр), и шум растягивался вместе с ней. Правило: тайл шума — noisePitch метров на любой поверхности. Мак (tools/noise949_list.py →
// noise949.bin рядом с level.shr) считает по каждому куску с шумом, сколько метров мира на единицу UV по u и по v (медиана по площади); движок перед куском с шумом ставит
// sceGuTexScale(128 · метры_u / P, 128 · метры_v / P) — две команды GE на кусок, привязка текстуры та же. Вне кусков с шумом масштаб кадровый (128, 128): flatOff и привязка родной текстуры
// возвращают его. NOISE949_OLD=1 / файл noise949old рядом с EBOOT / «flag noise949 0» — прежнее (шум по родным UV)
#ifndef NOISE949_OLD
#define NOISE949_OLD 0
#endif
static int nzOn = !NOISE949_OLD;       // правило включено
static unsigned nzPend;                // код масштаба куска для ближайшей привязки (u | v << 8; 0 — нет): ставит NZP(d) перед bindTexLean
static int nzCur;                      // что стоит в GE: 0 — кадровый (128, 128); -1 — неизвестно (после листа); иначе код (u | v << 8)
static float nzLut[256];               // метров на тайл по коду (0 — нет)
static unsigned nzPieces, nzBinds, nzTabN; static int nzFileOk;   // служебное: кусков с масштабом, привязок масштаба, длина таблицы; файл прочитан
static void nzInit(void) { nzLut[0] = 0.f; for (int b = 1; b < 256; b++) nzLut[b] = powf(2.f, (float)(b - 1) / 16.f - 4.f); }
static int nzOffOn;   // в GE стоит ненулевой сдвиг текстуры (№977)
static inline void nzReset(void) { sceGuTexScale(128.f, 128.f); if (nzOffOn) { sceGuTexOffset(0.f, 0.f); nzOffOn = 0; } nzCur = 0; }
static unsigned flatPieces, flatPiecesNoise;   // счёт кусков, пошедших заливкой (диагностика)
// ---- процедурный шум асфальта: периодический (стыкуется при повторе), несколько масштабов, слабый контраст, без точек; мипы затухают к ровному
static unsigned flatHash(int x, int y, int s) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static float flatVal(int x, int y, int period, int s) { x = ((x % period) + period) % period; y = ((y % period) + period) % period; return (float)(flatHash(x, y, s) & 0xffff) / 65535.f; }
static float flatNoise(float u, float v, int cells, int seed) {   // u, v в долях тайла, cells — клеток на тайл
  float x = u * cells, y = v * cells; int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3.f - 2.f * fx); fy = fy * fy * (3.f - 2.f * fy);
  float a = flatVal(xi, yi, cells, seed), b = flatVal(xi + 1, yi, cells, seed), c = flatVal(xi, yi + 1, cells, seed), d = flatVal(xi + 1, yi + 1, cells, seed);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
static int flatDbg;   // служебное «flag flatdbg 1»: заливка текстуры — цвет по номеру (находит, какая текстура на кусте кадра)
static int noiseNoLight;   // служебное «flag noiselite 1»: земля с шумом без света GE (проверка привязки координат)
static int noiseChk;   // служебное «flag noisechk 1»: шахматка 8x8 вместо шума (проверка привязки координат)
static float noiseConst = -1.f;   // служебное «flag noiseconst X» (0..1): шум заменяется ровным значением (проверка цены множителя текстуры)
static float noiseAmp = .22f; static float noiseW[4] = {.50f, .30f, .15f, .05f};   // размах яркости (доля), потолок 1,0: тёмные пятна до 70 % («flag noiseamp X» — подбор)
static void flatNoiseBuild(void) {
  if (noiseReady) return;
  const int W = 64, L = 7; float *f = (float *)malloc(W * W * sizeof(float)); if (!f) return;
  float lo = 1e9f, hi = -1e9f;
  for (int y = 0; y < W; y++) for (int x = 0; x < W; x++) { float u = (x + .5f) / W, v = (y + .5f) / W;
    float n = noiseW[0] * flatNoise(u, v, 4, 1) + noiseW[1] * flatNoise(u + .31f, v + .17f, 8, 2) + noiseW[2] * flatNoise(u + .63f, v + .41f, 16, 3) + noiseW[3] * flatNoise(u + .11f, v + .83f, 32, 4);
    f[y * W + x] = n; if (n < lo) lo = n; if (n > hi) hi = n; }
  // нормировка в 0..1, затем яркость 1 − A·(1 − n): светлое — почти без изменения, тёмные пятна и зерно
  float mean = 0.f; for (int i = 0; i < W * W; i++) { float n = (f[i] - lo) / (hi - lo); n = n * n * (3.f - 2.f * n) * .35f + n * .65f; { float q_ = 1.f - n; f[i] = 1.f - noiseAmp * q_ * q_ * q_; } /* тёмные пятна редки, основа светлая: среднее ≈ 0,9, заливку почти не нужно подтягивать (потолок канала 255) */ mean += f[i]; }
  if (noiseChk) { for (int y = 0; y < W; y++) for (int x = 0; x < W; x++) f[y * W + x] = (((x >> 3) + (y >> 3)) & 1) ? 1.f : .3f; mean = 0.65f * W * W; }
  if (noiseConst >= 0.f) { for (int i = 0; i < W * W; i++) f[i] = noiseConst; mean = noiseConst * W * W; }
  mean /= W * W; noiseMean = mean;
  unsigned char *p = noiseBuf; int lw = W; float *src = f; float *cur = (float *)malloc(W * W * sizeof(float)); if (!cur) { free(f); return; }
  static const float fade[8] = {1.f, 1.f, .60f, .30f, .10f, 0.f, 0.f, 0.f};   // к ровному цвету с дистанцией
  noiseTex.w = W; noiseTex.h = W; noiseTex.levels = L; noiseTex.pre = 0; noiseTex.t4 = 0; noiseTex.slope = .12f; noiseTex.ground = 1; noiseTex.levelsAll = L;
  for (int l = 0; l < L; l++) {
    int stride = lw < 16 ? 16 : lw; noiseTex.lv[l] = p;
    for (int y = 0; y < lw; y++) for (int x = 0; x < lw; x++) { float g = mean + (src[y * lw + x] - mean) * fade[l]; g = g < 0.f ? 0.f : g > 1.f ? 1.f : g; p[y * stride + x] = (unsigned char)(g * 255.f + .5f); }
    p += stride * lw; p = (unsigned char *)(((uintptr_t)p + 15) & ~(uintptr_t)15);
    int nw = lw >> 1; if (nw >= 1) { for (int y = 0; y < nw; y++) for (int x = 0; x < nw; x++) cur[y * nw + x] = .25f * (src[(2 * y) * lw + 2 * x] + src[(2 * y) * lw + 2 * x + 1] + src[(2 * y + 1) * lw + 2 * x] + src[(2 * y + 1) * lw + 2 * x + 1]);
      memcpy(f, cur, nw * nw * sizeof(float)); src = f; }   // src всегда f: исходное усреднение без затухания (затухание — при записи уровня)
    lw = nw;
  }
  free(cur); free(f);
  for (int i = 0; i < 256; i++) { noisePal[i * 4] = noisePal[i * 4 + 1] = noisePal[i * 4 + 2] = (unsigned char)i; noisePal[i * 4 + 3] = 255; }
  if (smkOn) { unsigned long long a0 = 0, a1 = 0; const unsigned char *l0 = (const unsigned char *)noiseTex.lv[0], *l1 = (const unsigned char *)noiseTex.lv[1]; for (int i = 0; i < 64 * 64; i++) a0 += l0[i]; for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) a1 += l1[y * 32 + x];
    { char l2_[240]; int n2 = snprintf(l2_, sizeof l2_, "NOISELV"); int lw2 = 64; for (int l = 0; l < 7; l++) { const unsigned char *q = (const unsigned char *)noiseTex.lv[l]; int st = lw2 < 16 ? 16 : lw2; unsigned long long a = 0; int mn = 255, mx = 0;
        for (int y = 0; y < lw2; y++) for (int x = 0; x < lw2; x++) { int v = q[y * st + x]; a += v; if (v < mn) mn = v; if (v > mx) mx = v; }
        n2 += snprintf(l2_ + n2, sizeof l2_ - n2, " L%d[%dx%d mean=%.3f %d..%d]", l, lw2, lw2, a / (double)(lw2 * lw2 * 255), mn, mx); lw2 >>= 1; } snprintf(l2_ + n2, sizeof l2_ - n2, "\n"); smkWrite(l2_); }
    char l_[160]; snprintf(l_, sizeof l_, "NOISE amp=%.2f mean_f=%.3f lvl0=%.3f lvl1=%.3f\n", noiseAmp, noiseMean, a0 / (4096.0 * 255.0), a1 / (1024.0 * 255.0)); smkWrite(l_); }
  noiseTex.pal = noisePal; sceKernelDcacheWritebackRange(noiseBuf, sizeof noiseBuf); sceKernelDcacheWritebackRange(noisePal, sizeof noisePal); noiseReady = 1;
}
// №969/№977: газон, тротуар и бордюр — свои текстуры вместо общего шума земли (№912). Варианты («flag ground969 N»): 1, 2 — «бумага» (ground969.h: мелкое зерно без низких частот, газон 8 / 12 м);
// 3, 4 — фрагмент РОДНОЙ текстуры чистого асфальта уровня (flat912.bin, класс 3: «асфальт родной» с наибольшей площадью) — те же индексы и мипы, серая палитра (светотень асфальта вокруг средней),
// заливка множится на неё; газон — 6 / 10 м. Тротуар и бордюр всегда на фрагменте асфальта (нет асфальта на уровне — «бумага»). На каждый кусок свой вариант: зеркало по u и v, масштаб 1 / 1,25 / 1,6, сдвиг —
// выбирается хэшем начала куска (детерминированно), две команды GE на кусок. Класс текстуры — по тону заливки (зелёный — газон; светлый нейтральный — тротуар); остальная земля с общим шумом.
// Пилот: только уровень 1 («flag ground969all 1» — все уровни). GROUND969_OLD=1 / файл ground969old рядом с EBOOT / «flag ground969 0» — прежний шум
static int gdStyle = GROUND969_OLD ? 0 : GROUND969_STYLE, gdAll = 1, gdBuiltStyle, gdSrc = -1; static unsigned gdBuiltGen = ~0u, gdBuiltSum;   // gdAll: на всех уровнях (flag ground969all 0 — только L1) static unsigned gdBuiltGen = ~0u, gdBuiltSum;
static Tex gdTex[GD_NCLS + 1]; static unsigned char gdBuf[GD_NCLS + 1][GD_BYTES] __attribute__((aligned(64))), gdAsPal[GD_NCLS + 1][1024] __attribute__((aligned(64)));
static int gdReady[GD_NCLS + 1], gdAsp[GD_NCLS + 1]; static float gdPitch[GD_NCLS + 1], gdMean[GD_NCLS + 1];
static unsigned char flatGd[1024];   // класс земли по номеру текстуры (0 — общий шум)
static inline int gdLevelOk(void) { return gameLevel == 1 || gdAll; }
// №1030 (Роб 26.10: «не нравится пятнистая текстура для пола, как мокрый пол, как плесень, выглядит грязно; бежево-песочный оттенок грязный; либо родной, либо нежная бумага»): земля заливкой, которая шла с общим пятнистым
// шумом №912 (остальная земля: песок, глина, дорожки; не газон, не тротуар), получает то же бумажное зерно №969 (класс GD_CLS_FLOOR: мелкое зерно без низких частот), а тёплый жёлто-оранжевый тон (R ≥ G ≥ B) идёт наполовину к холодному
// серому соседей (тротуар №985): чище, не песок. Рисунок (родная текстура) остаётся как есть. FLOOR1030_OLD=1 / файл floor1030old / «flag floor1030 0» — как в TEST71
#ifndef FLOOR1030_OLD
#define FLOOR1030_OLD 0
#endif
static int floor1030On = !FLOOR1030_OLD;
static int gdClassOf(int r, int g, int b) {
  int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b); float lum = .299f * r + .587f * g + .114f * b;
  if (g >= r && g - b >= 40) return GD_CLS_GRASS;
  if (mx - mn <= 24 && lum >= 125.f) return GD_CLS_WALK;
  return floor1030On ? GD_CLS_FLOOR : 0;   // №1030
}
// серая палитра фрагмента асфальта: яркость записи относительно медианы -> 0,94 + gain · (отношение − 1); возвращает среднее
static float gdAsphaltPal(const Tex *src, unsigned char *dst, float gain) {
  const unsigned char *sp = (const unsigned char *)src->pal; float lum[256], srt[256]; int n = 0;
  for (int i = 0; i < 256; i++) { lum[i] = .299f * sp[i * 4] + .587f * sp[i * 4 + 1] + .114f * sp[i * 4 + 2]; if (sp[i * 4 + 3] >= 16) srt[n++] = lum[i]; }
  for (int i = 1; i < n; i++) { float v = srt[i]; int j = i - 1; while (j >= 0 && srt[j] > v) { srt[j + 1] = srt[j]; j--; } srt[j + 1] = v; }
  float med = n ? srt[n / 2] : 128.f; if (med < 1.f) med = 1.f; float sum = 0.f;
  for (int i = 0; i < 256; i++) { float v = .94f + gain * (lum[i] / med - 1.f); v = v < 0.f ? 0.f : v > 1.f ? 1.f : v; unsigned char g = (unsigned char)(v * 255.f + .5f); dst[i * 4] = dst[i * 4 + 1] = dst[i * 4 + 2] = g; dst[i * 4 + 3] = 255; sum += v; }
  return sum / 256.f;
}
static void gdBuild(void) {   // после flatNoiseBuild; повтор — при смене варианта, уровня или палитры асфальта (задание тона переписывает её)
  if (!gdStyle || !noiseReady) return;
  int src = -1; for (unsigned i = 0; i < ntex && i < 1024; i++) if (tex[i].curb == 3 && tex[i].pal && tex[i].levels >= 1) { src = (int)i; break; }
  unsigned ps = 2166136261u; if (src >= 0) { const unsigned char *p = (const unsigned char *)tex[src].pal; for (int i = 0; i < 1024; i += 4) ps = (ps ^ p[i] ^ ((unsigned)p[i + 1] << 8)) * 16777619u; }
  if (gdBuiltStyle == gdStyle && gdBuiltGen == flatLevelGen && gdSrc == src && gdBuiltSum == ps) return;
  static const float gain[GD_NCLS + 1] = { 0.f, .35f, .30f, .25f, .30f }, aspPitch[GD_NCLS + 1] = { 0.f, 6.f, 3.f, 1.6f, 3.f };
  for (int c = 1; c <= GD_NCLS; c++) {
    Tex *x = &gdTex[c]; gdReady[c] = 0; gdAsp[c] = 0;
    int asp = src >= 0 && gdStyle >= 3;   // варианты 3, 4 (пробные): газон, тротуар и бордюр на фрагменте асфальта; по умолчанию (1, 2) — «бумага», бордюр ровной заливкой
    if (asp) {
      *x = tex[src]; x->pal = gdAsPal[c]; x->ground = 1; x->slope = tex[src].slope > 0.f ? tex[src].slope : .12f;
      gdMean[c] = gdAsphaltPal(&tex[src], gdAsPal[c], gain[c]); gdPitch[c] = (c == GD_CLS_GRASS && gdStyle == 4) ? 10.f : aspPitch[c]; gdAsp[c] = 1;
      sceKernelDcacheWritebackRange(gdAsPal[c], 1024); gdReady[c] = 1;
    } else if (c != GD_CLS_CURB) {
      GdPar pr = gdPar(c, gdStyle == 2 ? 2 : 1); float f[GD_W * GD_W], mean; unsigned char *lv[8];
      gdField(f, c, pr.amp, &mean); gdPack(f, mean, gdBuf[c], lv); gdMean[c] = mean; gdPitch[c] = pr.pitch;
      memset(x, 0, sizeof *x); x->w = GD_W; x->h = GD_W; x->levels = GD_L; x->levelsAll = GD_L; x->slope = .12f; x->ground = 1; x->pal = noisePal;
      for (int l = 0; l < GD_L; l++) x->lv[l] = lv[l];
      sceKernelDcacheWritebackRange(gdBuf[c], GD_BYTES); gdReady[c] = 1;
    }
  }
  gdBuiltStyle = gdStyle; gdBuiltGen = flatLevelGen; gdSrc = src; gdBuiltSum = ps; flatTintKey = ~0u;
  if (smkOn) { char l_[200]; snprintf(l_, sizeof l_, "GROUND969 style=%d src=%d asp=%d/%d/%d ready=%d/%d/%d pitch=%.1f/%.1f/%.1f mean=%.3f/%.3f/%.3f\n", gdStyle, src, gdAsp[1], gdAsp[2], gdAsp[3], gdReady[1], gdReady[2], gdReady[3], gdPitch[1], gdPitch[2], gdPitch[3], gdMean[1], gdMean[2], gdMean[3]); smkWrite(l_); }
}
// №974 (Роб 25.10: бордюры в мультике по оттенку как песок; нужен серо-фиолетовый, как в референсах Симпсонов; угловые детали с люком и стоком сильно отличаются по тону): бордюр —
// ровная заливка серо-фиолетового (без шума), угловая деталь (рисунок с люком, палитра) — в тот же тон, насыщенный фон-асфальт на ней остаётся. Класс текстуры по данным Мака:
// последний байт строки flat912.bin (tools/flat_list.py): 1 — заливка бордюра, 2 — рисунок угловой детали (поиск по содержимому текстуры на всех уровнях). CURB974_OLD=1 /
// файл curb974old рядом с EBOOT / «flag curb974 0» — как было
#ifndef CURB974_OLD
#define CURB974_OLD 0
#endif
static int curbR = 150, curbG = 140, curbB = 168;   // «flag curbrgb 0xRRGGBB (десятичное)» — подбор тона
// №985 (Роб 25.10: «весь тротуар холодный серый — оттенок брать, текстура нет»): тротуар по умолчанию холодным серым (тот же оттенок, что у варианта Б, curb974 2) с ровным бумажным зерном, без
// фрагмента асфальта; бордюр и кромка на таком тротуаре темнее и насыщеннее в том же оттенке (иначе сливаются). WALKCOLD985_OLD=1 / файл walkcold985old / «flag walkcold985 0» — тротуар тёплый, как в TEST67
#ifndef WALKCOLD985_OLD
#define WALKCOLD985_OLD 0
#endif
static int curbOn = !CURB974_OLD, curbAll = !WALKCOLD985_OLD;   // curbAll — толкование (Б): весь тротуар холодным серым («flag curb974 2»)
// №987 (Роб 26.10 после тура TEST69 на PSP: «бордюры оставим родное; на некоторых слишком желтит, а уголочки прям серые; хочу, чтобы всё выглядело единым; не перебарщивать»; выбор «Родной, уголки под него»):
// бордюр — родная заливка (серо-фиолетовая заливка №974, кромка у дороги №983 и контур тропинок №983 выключены; перо бордюра №978 берёт тон родного бордюра); холодный серый тротуар №985, газон «бумагой»
// и асфальт +8 % остаются; угловая деталь (люк, сток) — в тон своего соседа на уровне: Мак (tools/flat_list.py) кладёт номер соседней заливки в R,G строки flat912.bin (B = 0x87), движок берёт её
// итоговый цвет; тёплая жёлто-оранжевая заливка земли — чуть спокойнее по насыщенности (CURB987_YS). Цена в кадре 0 (всё при загрузке и сборке таблицы заливок). CURB987_OLD=1 / файл curb987old рядом с EBOOT /
// «flag curb987 0» — как в TEST69
#ifndef CURB987_OLD
#define CURB987_OLD 0
#endif
#ifndef CURB987_YS
#define CURB987_YS .6f       // доля насыщенности выше порога, остающаяся у тёплой жёлто-оранжевой земли
#endif
#ifndef CURB987_S0
#define CURB987_S0 .30f      // порог насыщенности (HSV)
#endif
// №1064 (Роб 28.10: «текстуру бордюра поставить родную и иметь возможность это настроить в настройках»): бордюр (текстуры curb 1) и угловая деталь (curb 2: люки, стоки) в мультике и при Textures Off рисуются
// СВОЕЙ текстурой оригинала, без заливки и без подкраски №974/№987 (остальное — газон, тротуар, асфальт, земля — не трогается). Строка Curbs на странице PICTURE: Original (по умолчанию) / Tinted
// (заливка и подкраска №987, как в TEST76; cfg.curbTint). CURB1064_OLD=1 / файл curb1064old рядом с EBOOT / «flag curb1064 0» — как в TEST76 (всегда Tinted)
#ifndef CURB1064_OLD
#define CURB1064_OLD 0
#endif
static int curbNat1064 = !CURB1064_OLD, curbWant1064 = !CURB1064_OLD, curb1064Old = CURB1064_OLD;   // curbNat1064 — бордюр родной текстурой (действует), curbWant1064 — выбор строки Curbs; переносится в curbNat1064 при загрузке уровня (flatListLoad): пересборка заливок на ходу сдвигала тротуар L1 в жёлтый, поэтому строка действует со следующей загрузки уровня
static int curb987On = !CURB987_OLD; static float curb987Ys = CURB987_YS;   // «flag curb987 0|1», «flag curb987ys X»
static unsigned char curbNat[4];                                           // цвет родного бордюра уровня после правил (для пера №978); curbNat[3] — есть ли
// №1008 (Роб 26.10 после TEST71: «контур по краям асфальта ты дал светло-серым; хочу комплементарный самому асфальту: асфальт фиолетовый — тёмно-фиолетовый, серый — тёмно-серый»):
// после №987 край асфальта (линии №824) рисовался пером родного бордюра (светлый серо-бежевый). Теперь перо края асфальта — skinInkTone от среднего тона асфальта уровня (тот же оттенок,
// темнее и насыщеннее; почти серый — тёмно-серый). ROADINK1008_OLD=1 / файл roadink1008old рядом с EBOOT / flag roadink1008 0 — как в TEST71
#ifndef ROADINK1008_OLD
#define ROADINK1008_OLD 0
#endif
// №1007 (Роб 26.10: «Гомер на парковке у Квик-и-Марта: асфальт одного оттенка, а пешеходная зона рядом синяя, хотя должна сливаться с фоном асфальта; усреднить»): после цветокоррекции мультика заливка
// соседних земель одного фона расходится по тону. Мак (tools/zones1007_list.py → zones1007.bin рядом с level.shr) находит заливку, граница которой в основном с асфальтом, а тон заметно (ΔE) другой, и кладёт
// пару {текстура, к чему (номер заливки соседа, 0xFFFF — средний тон асфальта уровня), доля смещения}; движок при сборке таблицы заливок подтягивает итоговый тон на эту долю (рисунок, полосы и шум остаются).
// Формат ZN07: 'ZN07', n, подпись (как у flat912.bin), n × 8 байт {tex u16, цель u16, доля 0..255, 0, 0, 0}. Цена в кадре 0. ZONES1007_OLD=1 / файл zones1007old рядом с EBOOT / «flag zones1007 0» — как в TEST71
#ifndef ZONES1007_OLD
#define ZONES1007_OLD 0
#endif
static int zn7On = !ZONES1007_OLD; static unsigned zn7N; static struct { unsigned short tex, tgt; unsigned char mix; } zn7E[256];
static int zn7Asph(unsigned char *c) {   // средний тон асфальта уровня: родные тона текстур-асфальта после цветокоррекции мультика (0 — на уровне такого нет)
  unsigned sr = 0, sg = 0, sb = 0, n = 0;
  for (unsigned t = 0; t < ntex && t < 1024; t++) { const Tex *x = &tex[t]; if (!x->asph) continue; unsigned char m[4] = {(unsigned char)(x->flatC & 255), (unsigned char)((x->flatC >> 8) & 255), (unsigned char)((x->flatC >> 16) & 255), 255}; toneColorRGB(m); sr += m[0]; sg += m[1]; sb += m[2]; n++; }
  if (!n) return 0; c[0] = (unsigned char)(sr / n); c[1] = (unsigned char)(sg / n); c[2] = (unsigned char)(sb / n); c[3] = 255; return 1;
}
static const typeof(zn7E[0]) *zn7Of(unsigned t) { for (unsigned e = 0; e < zn7N; e++) if (zn7E[e].tex == t) return &zn7E[e]; return 0; }
static void flatBaseC(const Tex *x, int gd, unsigned char *c);
static int flatGdOf(const Tex *x);
static int zn7Target(unsigned g, unsigned char *c) {   // итоговый тон цели: 0xFFFF — средний асфальт; заливка — её тон правил; рисунок — родной тон после цветокоррекции
  if (g == 0xffffu) return zn7Asph(c);
  if (g >= ntex || g >= 1024) return 0; const Tex *x = &tex[g];
  if (x->flatK == 1) { flatBaseC(x, flatGdOf(x), c); return 1; }
  c[0] = (unsigned char)(x->flatC & 255); c[1] = (unsigned char)((x->flatC >> 8) & 255); c[2] = (unsigned char)((x->flatC >> 16) & 255); c[3] = 255; toneColorRGB(c); return 1;
}
static void zn7Pal(const Tex *x, unsigned char *pal) {   // палитра рисунка земли у асфальта (RGBA×256): записи рядом с основным тоном сдвигаются к тону соседа, остальные (полосы, разметка) почти не меняются
  long i0 = x - tex; if (i0 < 0 || i0 >= (long)ntex || i0 >= 1024 || x->flatK != 0) return;
  const typeof(zn7E[0]) *e = zn7Of((unsigned)i0); if (!e) return;
  unsigned char tg[4], c0[4] = {(unsigned char)(x->flatC & 255), (unsigned char)((x->flatC >> 8) & 255), (unsigned char)((x->flatC >> 16) & 255), 255};
  if (!zn7Target(e->tgt, tg)) return; toneColorRGB(c0);
  float m = e->mix / 255.f;
  for (int i = 0; i < 256; i++) { unsigned char *q = pal + i * 4; if (q[3] < 16) continue;
    float dr = (float)q[0] - c0[0], dg = (float)q[1] - c0[1], db = (float)q[2] - c0[2], w = expf(-(dr * dr + dg * dg + db * db) / (2.f * 70.f * 70.f));
    for (int k = 0; k < 3; k++) { float v = q[k] + ((float)tg[k] - (float)c0[k]) * m * w; q[k] = (unsigned char)(v < 0.f ? 0.f : v > 255.f ? 255.f : v + .5f); } }
}
static unsigned char asphNat[4];   // средний тон асфальта уровня после цветокоррекции; asphNat[3] — есть ли
static int roadInk1008Old = ROADINK1008_OLD;
// №979 (Роб 25.10: «асфальт капельку светлее, без смены кривой и оттенка»): светлота (HSL) палитры родного асфальта всех уровней +ASPHLIGHT_K, оттенок и насыщенность те же. ASPHLIGHT_OLD=1 /
// файл asphlightold рядом с EBOOT / «flag asphlight 0» — как было. Какие текстуры — по данным Мака (старшие 4 бита последнего байта строки flat912.bin, tools/flat_list.py)
#ifndef ASPHLIGHT_OLD
#define ASPHLIGHT_OLD 0
#endif
#ifndef ASPHLIGHT_K
#define ASPHLIGHT_K 1.08f
#endif
static int asphOn = !ASPHLIGHT_OLD;
static void asphLight(unsigned char *pal) {
  for (int i = 0; i < 256; i++) { unsigned char *q = pal + i * 4; if (q[3] < 16) continue;
    float r = q[0] / 255.f, g = q[1] / 255.f, b = q[2] / 255.f, mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), l = (mx + mn) * .5f, d = mx - mn;
    if (d < 1e-6f) { float v = l * ASPHLIGHT_K; if (v > 1.f) v = 1.f; q[0] = q[1] = q[2] = (unsigned char)(v * 255.f + .5f); continue; }
    float s = l > .5f ? d / (2.f - mx - mn) : d / (mx + mn), h = mx == r ? (g - b) / d + (g < b ? 6.f : 0.f) : mx == g ? (b - r) / d + 2.f : (r - g) / d + 4.f;   // h в 0..6
    float l2 = l * ASPHLIGHT_K; if (l2 > .98f) l2 = .98f; float c = (1.f - (2.f * l2 - 1.f < 0.f ? 1.f - 2.f * l2 : 2.f * l2 - 1.f)) * s, x2 = c * (1.f - ((h - 2.f * (float)(int)(h * .5f)) - 1.f < 0.f ? 1.f - (h - 2.f * (float)(int)(h * .5f)) : (h - 2.f * (float)(int)(h * .5f)) - 1.f)), m = l2 - c * .5f;
    float o[3]; int hi = (int)h; switch (hi % 6) { case 0: o[0] = c; o[1] = x2; o[2] = 0; break; case 1: o[0] = x2; o[1] = c; o[2] = 0; break; case 2: o[0] = 0; o[1] = c; o[2] = x2; break; case 3: o[0] = 0; o[1] = x2; o[2] = c; break; case 4: o[0] = x2; o[1] = 0; o[2] = c; break; default: o[0] = c; o[1] = 0; o[2] = x2; }
    for (int k = 0; k < 3; k++) { float v = (o[k] + m) * 255.f; q[k] = (unsigned char)(v < 0.f ? 0.f : v > 255.f ? 255.f : v + .5f); } }
}
// №999 (Роб 26.10 на TEST71: «пешеходные переходы на брусчатке слишком кислотные, пусть будут в цвет остальной брусчатки»): у текстуры перехода (и других разметочных текстур дороги) средний цвет для стягивания палитры
// (tone.c toneSetStat) — свой, с белыми полосами, и оливковые записи брусчатки на развилке тона уходили в лаймово-зелёный, а у чистой дороги рядом — в коричнево-оранжевый. Теперь тон берёт статистику
// чистой текстуры дороги того же цвета (донор, данные Мака: R,G = номер донора, B = XWALK999_MARK в строке flat912.bin у причины 3 и 7 с битом асфальта). XWALK999_OLD=1 / файл xwalk999old рядом с EBOOT /
// «flag xwalk999 0» — как в TEST71
#ifndef XWALK999_OLD
#define XWALK999_OLD 0
#endif
#define XWALK999_MARK 0x99
static int xwalk999On = !XWALK999_OLD;
// №1005 (Роб 26.10: «на уровне Лизы красивая родная текстура пешеходной зоны, я бы её оставил»): светлая плитка тротуара L3 остаётся родной текстурой — данные Мака (flat912.bin: вид 0, причина 11, цвет заливки и доля
// сохранены). WALK1005_OLD=1 / файл walk1005old рядом с EBOOT / «flag walk1005 0» — такие строки читаются заливкой, как в TEST71
#ifndef WALK1005_OLD
#define WALK1005_OLD 0
#endif
#define WALK1005_REASON 11
static int walk1005On = !WALK1005_OLD;
static const unsigned char *toneStatOf(const Tex *x) {   // статистика тона текстуры: у перехода — донора (чистая дорога того же тона)
  if (!x || !x->tstatOk) return 0;
  if (xwalk999On && x->xdon && tex && x >= tex && x < tex + ntex && x->xdon <= ntex) { const Tex *d = &tex[x->xdon - 1]; if (d->tstatOk) { static unsigned char db_[5]; memcpy(db_, d->tstat, 5); db_[4] |= 32; return db_; } }   // №1029: бит 32 — статистика донора: в tone.c тянутся к ней только записи цвета дороги, жёлтая разметка и белые полосы не трогаются
  return x->tstat;
}
static void xwalk999Mean(const Tex *x, int *o, const int *ref) {   // служебно (журнал, приёмка): цвет текстуры по палитре после тона — без ref самая частая насыщенная запись и её окрестность 40 (средний), с ref — самая близкая к ref запись с долей не меньше 1 %; o[3] — сколько точек
  unsigned cnt[256]; memset(cnt, 0, sizeof cnt); o[0] = o[1] = o[2] = o[3] = 0;
  if (!x || !x->pal || !x->lv[0] || x->levels < 1) return;
  unsigned rb = (unsigned)texRowBytes(x->t4, x->pre, x->w), rows = x->pre ? ((unsigned)x->h + 7) & ~7u : (unsigned)x->h, n = rb * rows; const unsigned char *px = (const unsigned char *)x->lv[0];
  if (x->t4) for (unsigned i = 0; i < n; i++) { cnt[px[i] & 15]++; cnt[px[i] >> 4]++; } else for (unsigned i = 0; i < n; i++) cnt[px[i]]++;
  int top = -1; unsigned tc = 0; int bd = 1 << 30;
  for (int j = 0; j < 256; j++) { const unsigned char *c = (const unsigned char *)x->pal + j * 4; if (!cnt[j] || c[3] < 16) continue;
    if (ref) { if (cnt[j] * 100u < n * 2u / 2u) continue; int dr = c[0] - ref[0], dg = c[1] - ref[1], db = c[2] - ref[2], d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; top = j; tc = cnt[j]; } continue; }
    int mx = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]), mn = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
    if (mx < 40 || (mx - mn) * 100 < mx * 25) continue; if (cnt[j] > tc) { tc = cnt[j]; top = j; } }
  if (top < 0) return;
  const unsigned char *t0 = (const unsigned char *)x->pal + top * 4; double sr = 0, sg = 0, sb = 0, sw = 0;
  if (ref) { o[0] = t0[0]; o[1] = t0[1]; o[2] = t0[2]; o[3] = (int)tc; return; }
  for (int j = 0; j < 256; j++) { const unsigned char *c = (const unsigned char *)x->pal + j * 4; if (!cnt[j] || c[3] < 16) continue;
    int dr = c[0] - t0[0], dg = c[1] - t0[1], db = c[2] - t0[2]; if (dr * dr + dg * dg + db * db > 1600) continue;
    sr += (double)cnt[j] * c[0]; sg += (double)cnt[j] * c[1]; sb += (double)cnt[j] * c[2]; sw += cnt[j]; }
  if (sw > 0) { o[0] = (int)(sr / sw + .5); o[1] = (int)(sg / sw + .5); o[2] = (int)(sb / sw + .5); o[3] = (int)sw; }
}
static int curbTarget(const Tex *x, unsigned char *c);
static void curbPal(const Tex *x, unsigned char *pal) {   // палитра уголка (RGBA×256): светлые нейтральные и тёплые записи — в тон бордюра с сохранением светотени; фиолетовый асфальт и прозрачные не трогаем
  if (!x || !pal || !toneToon()) return;   // только в мультике: обычный вид палитр не трогаем
  if (zn7On && zn7N) zn7Pal(x, pal);   // №1007
  if (asphOn && x->asph) asphLight(pal);   // №979
  if (!curbOn || x->curb != 2 || curbNat1064) return;   // №1064: угловая деталь родной палитрой
  int tr = curbR, tg = curbG, tb = curbB; { unsigned char t_[4]; if (curb987On && curbTarget(x, t_)) { tr = t_[0]; tg = t_[1]; tb = t_[2]; } }   // №987: тон соседа на уровне (был серо-фиолетовый)
  for (int i = 0; i < 256; i++) { unsigned char *q = pal + i * 4; int r = q[0], g = q[1], b = q[2]; if (q[3] < 16) continue;
    if (b > r + 10) continue;
    float lum = .299f * r + .587f * g + .114f * b, k = lum / 170.f; if (k > 1.25f) k = 1.25f;
    int nr = (int)(tr * k + .5f), ng = (int)(tg * k + .5f), nb = (int)(tb * k + .5f); q[0] = nr > 255 ? 255 : nr; q[1] = ng > 255 ? 255 : ng; q[2] = nb > 255 ? 255 : nb; }
}
// ---- таблица заливок: цвет через цветокоррекцию мультика, 4 бита на канал (куски одного цвета потом склеиваются)
static int flatGdOf(const Tex *x) {   // класс земли текстуры (0 — общий шум); №987: родной бордюр без зерна
  if (!(gdStyle && x->curb != 3 && x->curb != 2 && gdBuiltStyle == gdStyle && (x->tstat[4] & 4) && flatNoiseOn && x->flatK == 1 && gdLevelOk())) return 0;
  if (curb987On && x->curb == 1) return 0;
  int gc_ = curbOn && x->curb == 1 ? GD_CLS_CURB : gdClassOf((int)(x->flatC & 255), (int)((x->flatC >> 8) & 255), (int)((x->flatC >> 16) & 255));
  return gc_ && gdReady[gc_] ? gc_ : 0;
}
// цвет заливки текстуры после цветокоррекции мультика, смешения со средним и правил №974 / №985 / №987 (без зерна); общий для таблицы заливок и для палитры угловой детали
static void flatBaseC(const Tex *x, int gd, unsigned char *c) {
  c[0] = (unsigned char)(x->flatC & 255); c[1] = (unsigned char)((x->flatC >> 8) & 255); c[2] = (unsigned char)((x->flatC >> 16) & 255); c[3] = 255;
  unsigned char m[4] = {x->tstat[0], x->tstat[1], x->tstat[2], 255};
  toneColorRGB(c); toneColorRGB(m);   // преобладающий тон и среднее — оба через цветокоррекцию мультика; у бимодальных и градиентных текстур тон сдвигается к среднему (вид глазом)
  { float share = x->flatS / 255.f, be = (FLAT_BETA_S0 - share) * FLAT_BETA_K; be = be < 0.f ? 0.f : be > FLAT_BETA_MAX ? FLAT_BETA_MAX : be;
    for (int j = 0; j < 3; j++) c[j] = (unsigned char)(c[j] + (m[j] - (float)c[j]) * be + .5f); }
  if (curbOn && !curb987On && x->curb == 1) { c[0] = (unsigned char)(curbR - (curbAll ? 28 : 0)); c[1] = (unsigned char)(curbG - (curbAll ? 32 : 0)); c[2] = (unsigned char)(curbB - (curbAll ? 18 : 0)); }   // №974: бордюр — серо-фиолетовая заливка
  else if ((curbOn || curb987On) && curbAll && gd == GD_CLS_WALK) { c[0] = (unsigned char)(curbR + 8); c[1] = (unsigned char)(curbG + 8); c[2] = (unsigned char)(curbB + 6); }   // №985: весь тротуар холодный серый (№987: не зависит от заливки бордюра)
  else if (floor1030On && gd == GD_CLS_FLOOR && c[0] >= c[1] && c[1] >= c[2] && c[0] > 0 && (float)(c[0] - c[2]) / (float)c[0] > .2f) {   // №1030: тёплая земля — наполовину к холодному серому соседей
    int wr = curbR + 8, wg = curbG + 8, wb = curbB + 6; c[0] = (unsigned char)((c[0] + wr + 1) / 2); c[1] = (unsigned char)((c[1] + wg + 1) / 2); c[2] = (unsigned char)((c[2] + wb + 1) / 2); }
  else if (curb987On && (x->tstat[4] & 4) && gd != GD_CLS_GRASS) {   // №987: тёплая жёлто-оранжевая земля (R ≥ G ≥ B, насыщенность выше порога) — спокойнее; газон, холодный тротуар, дорога и дома не трогаются
    int mx = c[0], mn = c[2]; if (c[0] >= c[1] && c[1] >= c[2] && mx > 0) { float sat = (float)(mx - mn) / (float)mx;
      if (sat > CURB987_S0) { float s2 = CURB987_S0 + (sat - CURB987_S0) * curb987Ys, f = s2 / sat; for (int j = 1; j < 3; j++) c[j] = (unsigned char)(mx - (mx - c[j]) * f + .5f); } } }
}
// тон угловой детали (№987): Мак кладёт в R,G её строки номер соседней заливки (B = 0x87); нет данных — родной бордюр уровня; 0 — нет ни того ни другого (останется серо-фиолетовый №974)
static int curbTarget(const Tex *x, unsigned char *c) {
  if (((x->flatC >> 16) & 255) == 0x87u) { unsigned p = x->flatC & 0xffffu; if (p < ntex && p < 1024 && tex[p].flatK == 1) { flatBaseC(&tex[p], flatGdOf(&tex[p]), c); return 1; } }
  for (unsigned t = 0; t < ntex && t < 1024; t++) if (tex[t].curb == 1 && tex[t].flatK == 1) { flatBaseC(&tex[t], flatGdOf(&tex[t]), c); return 1; }
  return 0;
}
static void flatTintBuild(void) {
  unsigned key = toneKey() ^ ((unsigned)ntex << 20) ^ (unsigned)(toneToonTexPct() << 8) ^ ((unsigned)gdStyle << 28) ^ ((unsigned)gdBuiltStyle << 26) ^ ((unsigned)gameLevel << 22) ^ (gdAll ? 0x55u : 0u) ^ (curbOn ? 0x2aa00000u : 0u) ^ (curbAll ? 0x1500000u : 0u) ^ (curbNat1064 ? 0x00640000u : 0u) ^ (curb987On ? 0x00a70000u + (unsigned)(curb987Ys * 100.f) * 977u : 0u) ^ (zn7On && zn7N ? 0x00b70000u + zn7N * 131u : 0u) ^ (floor1030On ? 0x00300000u : 0u); if (key == flatTintKey && flatTintN == ntex) return;
  flatTintKey = key; flatTintN = ntex; flatKinds[0] = flatKinds[1] = flatKinds[2] = 0; curbNat[3] = 0;
  for (unsigned t = 0; t < ntex && t < 1024; t++) { const Tex *x = &tex[t]; flatTint[t] = 0; flatQ[t] = 0; if (!x->flatK) continue;
    flatGd[t] = (unsigned char)flatGdOf(x);
    unsigned char c[4]; flatBaseC(x, flatGd[t], c);
    float k = (x->tstat[4] & 4) && flatNoiseOn ? 1.f / (flatGd[t] ? gdMean[flatGd[t]] : noiseMean) : 1.f;
    if (curb987On && x->curb == 1) { k = 1.f; if (!curbNat[3]) { curbNat[0] = c[0]; curbNat[1] = c[1]; curbNat[2] = c[2]; curbNat[3] = 1; } }   // №987: родной бордюр — ровная заливка своего тона
    else if (curbOn && x->curb == 1) k = flatGd[t] ? 1.f / gdMean[flatGd[t]] : 1.f;   // №974: бордюр — с зерном асфальта, если есть
    unsigned r = (unsigned)(c[0] * k + .5f), g = (unsigned)(c[1] * k + .5f), b = (unsigned)(c[2] * k + .5f); if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    if (flatDbg) { unsigned h_ = t * 2654435761u; r = 60 + ((h_ >> 4) & 0xbf); g = 60 + ((h_ >> 12) & 0xbf); b = 60 + ((h_ >> 20) & 0xbf); if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; }
    unsigned qr = (r * 15 + 127) / 255, qg = (g * 15 + 127) / 255, qb = (b * 15 + 127) / 255;
    flatQ[t] = 1u | (qr << 1) | (qg << 5) | (qb << 9); flatTint[t] = 0xff000000u | b << 16 | g << 8 | r; flatExact[t] = 0x40000000u | (b << 18 | g << 10 | r << 2);
    if (x->flatE == FLAT_EL_OK) flatKinds[(x->tstat[4] & 4) && flatNoiseOn ? 2 : 1]++; }
  { unsigned sr_ = 0, sg_ = 0, sb_ = 0, n_ = 0; asphNat[3] = 0;   // №1008: средний тон асфальта уровня (текстуры класса «асфальт» №979, среднее через цветокоррекцию мультика) — для пера края асфальта
    for (unsigned t = 0; t < ntex && t < 1024; t++) { const Tex *x = &tex[t]; if (!x->asph || !x->tstatOk) continue;
      unsigned char m[4] = {x->tstat[0], x->tstat[1], x->tstat[2], 255}; toneColorRGB(m); sr_ += m[0]; sg_ += m[1]; sb_ += m[2]; n_++; }
    if (n_) { asphNat[0] = (unsigned char)(sr_ / n_); asphNat[1] = (unsigned char)(sg_ / n_); asphNat[2] = (unsigned char)(sb_ / n_); asphNat[3] = 1; } }
  if (zn7On) for (unsigned e = 0; e < zn7N; e++) {   // №1007: заливка у асфальта — к тону соседа (итоговому, после шума и правил)
    unsigned t = zn7E[e].tex, g = zn7E[e].tgt; if (t >= ntex || t >= 1024 || !flatQ[t]) continue;
    unsigned tr, tg, tb;
    if (tex[t].flatK != 1) continue;   // рисунок — через палитру (zn7Pal)
    if (g == 0xffffu) { unsigned char a_[4]; if (!zn7Asph(a_)) continue; tr = a_[0]; tg = a_[1]; tb = a_[2]; }
    else { if (g >= ntex || g >= 1024 || !flatQ[g]) continue; tr = flatTint[g] & 255; tg = (flatTint[g] >> 8) & 255; tb = (flatTint[g] >> 16) & 255; }
    float m = zn7E[e].mix / 255.f; unsigned c0 = flatTint[t];
    unsigned r = (unsigned)((c0 & 255) + ((float)tr - (float)(c0 & 255)) * m + .5f), gg = (unsigned)(((c0 >> 8) & 255) + ((float)tg - (float)((c0 >> 8) & 255)) * m + .5f), b = (unsigned)(((c0 >> 16) & 255) + ((float)tb - (float)((c0 >> 16) & 255)) * m + .5f);
    if (r > 255) r = 255; if (gg > 255) gg = 255; if (b > 255) b = 255;
    unsigned qr = (r * 15 + 127) / 255, qg = (gg * 15 + 127) / 255, qb = (b * 15 + 127) / 255;
    flatQ[t] = 1u | (qr << 1) | (qg << 5) | (qb << 9); flatTint[t] = 0xff000000u | b << 16 | gg << 8 | r; flatExact[t] = 0x40000000u | (b << 18 | gg << 10 | r << 2); }
}
// шум земли в МИРОВЫХ координатах: UV строит GE из положения вершины текстурной матрицей (изотропно, не тянется вдоль дороги, как родные UV); матрицу собирает main_render.h из матрицы модели
static int flatMap = 2;                 // режим координат GE: 0 родные UV, 1 из положения (шум), 2 неизвестно
static int flatInList, flatNoSuppress;  // идёт сборка листа (текстурная матрица ставится перед вызовом листа); предметы — заливка без шума
static float flatMdl[16]; static unsigned flatMdlGen, flatTexGen, flatNoiseN, flatTexEmitN; static int flatMdlOk;
static float noiseUvG = 1.f;            // «flag noiseg X»
static float noisePitch = 5.f;          // длина тайла шума, м («flag noisepitch X»)
#define NOISE_PITCH noisePitch
static void flatTexEmit(void);
// вариант (б) для сравнения: шум в ЦВЕТЕ ВЕРШИН земли (при загрузке клетки; «flag flatnoise 2», берётся со следующей загрузки уровня). Шаг вершин земли — метры, поэтому пятна крупные
static int flatVNoise; static float flatVAmp = .45f; static unsigned flatVNoiseVerts;
static float flatVn(float x, float z) {
  float a = flatNoise(fmodf(fmodf(x / (64 * 1.5f), 1.f) + 1.f, 1.f), fmodf(fmodf(z / (64 * 1.5f), 1.f) + 1.f, 1.f), 64, 5), b = flatNoise(fmodf(fmodf(x / (64 * .7f), 1.f) + 1.f, 1.f), fmodf(fmodf(z / (64 * .7f), 1.f) + 1.f, 1.f), 64, 6);
  float n = .6f * a + .4f * b; n = (n - .5f) * 2.2f + .5f; return n < 0.f ? 0.f : n > 1.f ? 1.f : n;
}
// №1007б (Роб: «тон на кусок города»): площадка/парковка у асфальта делит текстуру с бордюрами, поэтому тон правится не по текстуре, а по куску. Мак (tools/zones1007_list.py → zones1007p.bin рядом с level.shr) кладёт для куска
// множитель цвета вершин R,G,B (не больше 1: заливка и палитра множатся на цвет вершины, свет запечён в нём же); движок умножает цвет вершин куска при чтении клетки (там же, где шум вершин) — ближних и дальних.
// Формат ZP07: 'ZP07', npc, подпись (как у flat912.bin), n, затем n × 8 байт {кусок u32, R, G, B, 0}. Цена в кадре 0. ZONES1007_OLD=1 / файл zones1007old / «flag zones1007 0» (читается со следующей загрузки клетки) — как в TEST71
static unsigned char *zpMul; static unsigned zpN, zpApplied, zpNpc;   // zpMul[k*4..k*4+3] — множитель куска k (байт 3 — есть ли)
static void zpVertex(const Piece *d, unsigned char *vtx, unsigned n, const unsigned char *m) {
  if (!vtx || !n || !m[3]) return;
  int v12 = (d->r.flags & F_V12) != 0; unsigned vs = v12 ? 12u : 16u;
  for (unsigned i = 0; i < n; i++) { unsigned char *q = vtx + (size_t)i * vs;
    if (v12) { unsigned short c = *(unsigned short *)(q + 4); unsigned r = (unsigned)((c & 31) * (m[0] / 255.f) + .5f), g = (unsigned)(((c >> 5) & 63) * (m[1] / 255.f) + .5f), b = (unsigned)((c >> 11) * (m[2] / 255.f) + .5f); *(unsigned short *)(q + 4) = (unsigned short)(r | g << 5 | b << 11); }
    else { q[4] = (unsigned char)(q[4] * (m[0] / 255.f) + .5f); q[5] = (unsigned char)(q[5] * (m[1] / 255.f) + .5f); q[6] = (unsigned char)(q[6] * (m[2] / 255.f) + .5f); } }
  zpApplied++;
}
static void flatVertexNoise(const Piece *d, unsigned char *vtx, unsigned n) {
  const Tex *x = (unsigned)d->r.tex < ntex ? &tex[d->r.tex] : 0; if (!flatVNoise || !x || !x->flatK || x->flatE != FLAT_EL_OK || !(x->tstat[4] & 4)) return;
  int v12 = (d->r.flags & F_V12) != 0; unsigned vs = v12 ? 12u : 16u, po = v12 ? 6u : 8u; float k = d->r.sc / 32768.f;
  for (unsigned i = 0; i < n; i++) { unsigned char *q = vtx + (size_t)i * vs; const short *p = (const short *)(q + po); float f = 1.f - flatVAmp * (1.f - flatVn(d->r.ox + p[0] * k, d->r.oz + p[2] * k));
    if (v12) { unsigned short c = *(unsigned short *)(q + 4); unsigned r = (unsigned)((c & 31) * f + .5f), g = (unsigned)(((c >> 5) & 63) * f + .5f), b = (unsigned)((c >> 11) * f + .5f); *(unsigned short *)(q + 4) = (unsigned short)(r | g << 5 | b << 11); }
    else { q[4] = (unsigned char)(q[4] * f + .5f); q[5] = (unsigned char)(q[5] * f + .5f); q[6] = (unsigned char)(q[6] * f + .5f); }
    flatVNoiseVerts++; }
}
// класс: 0 — с текстурой, 1 — заливка, 2 — земля (шум + заливка)
static inline int flatKind(int t) {
  if (!flatOn || (unsigned)t >= ntex || (unsigned)t >= 1024) return 0; const Tex *x = &tex[t];
  if (!x->flatK || x->flatE != FLAT_EL_OK || !flatQ[t]) return 0;
  if (curbNat1064 && x->curb == 1) return 0;                    // №1064: бордюр — своя текстура оригинала
  if (x->flatK == 3 && !marksOk) return 0;                      // асфальт с разметкой: без слоя разметки остаётся текстурой
  if ((curbOn || curb987On) && x->curb == 1 && !(flatGd[t] && gdReady[flatGd[t]])) return 1;   // №974: бордюр без шума (№987: и родной)
  if ((x->tstat[4] & 4) && flatNoiseOn && !flatNoSuppress && !flatVNoise) return flatGd[t] && gdReady[flatGd[t]] ? 3 : 2;   // 3 — рисунок №969
  return 1;
}
static inline void flatMarkPiece(int t, int flags, int owner) {   // кусок таблицы города или предмета: пригоден ли для заливки
  if ((unsigned)t >= ntex || (unsigned)t >= 1024) return; Tex *x = &tex[t];
  if (owner != 0 || (flags & (1 | 2 | 4 | (1 << 24)))) x->flatE |= FLAT_EL_BAD; else x->flatE |= FLAT_EL_OK;
}
// ---- состояние света GE: 0 выключен, 1 включён, 2 неизвестно (после листа, сброса состояний)
static int flatLit = 2, flatTouched; static unsigned flatAmb; static int flatAmbOk;
static unsigned lstBinds, lstFrames, lstSumB, lstImm, lstFlatImm;   // №912: смен состояния текстуры внутри вызванных листов за кадр
static unsigned flatSwitches, flatBinds;   // диагностика: включений света, привязок заливки
static void flatOff(void) {
  if (nzCur) nzReset();
  if (!flatTouched) { flatLit = 0; flatMap = 0; return; }
  if (flatMap != 0) { sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0); flatMap = 0; }
  if (flatLit == 0) return;
  sceGuDisable(GU_LIGHTING); sceGuColorMaterial(0); flatLit = 0; flatAmbOk = 0;
}
// ---- преобладающий тон текстуры по палитре и счётчикам точек (cnt — сколько точек на запись палитры, pal — ИСХОДНАЯ палитра до кривой тона)
// Тон — среднее записей, лежащих рядом (радиус R в RGB) с самой весомой записью: шумовые тёмные точки и редкие блики в тон не попадают. Рисунок (flatK = 0): прозрачные
// точки; второй цвет заметной доли и контраста (окна на стене, надписи); много разных цветов. У земли порог строже: разметка дороги остаётся с текстурой
#define FLAT_R2 1600     // R = 40 в RGB
#define FLAT_R2_FAR 6400 // 2R: второй цвет ищется дальше этого от первого
static inline int flatD2(const unsigned char *a, const unsigned char *b) { int r = (int)a[0] - b[0], g = (int)a[1] - b[1], bl = (int)a[2] - b[2]; return r * r + g * g + bl * bl; }
static float flatLuma(const unsigned char *c) { return .299f * c[0] + .587f * c[1] + .114f * c[2]; }
static int flatBestCluster(const unsigned char *pal, const unsigned *cnt, const unsigned char *skip, const unsigned char *skipC, unsigned *wOut, unsigned char *colOut) {   // -> номер записи или -1
  int cand[12]; unsigned cw[12]; int nc = 0;
  for (int j = 0; j < 256; j++) { if (!cnt[j] || pal[j * 4 + 3] < 16) continue; if (skip && flatD2(pal + j * 4, skipC) <= FLAT_R2_FAR) continue; (void)skip;
    int k = nc < 12 ? nc++ : -1; if (k < 0) { int m = 0; for (int q = 1; q < 12; q++) if (cw[q] < cw[m]) m = q; if (cnt[j] <= cw[m]) continue; k = m; }
    cand[k] = j; cw[k] = cnt[j]; }
  unsigned bw = 0; int bj = -1;
  for (int q = 0; q < nc; q++) { unsigned w = 0; for (int j = 0; j < 256; j++) { if (!cnt[j] || pal[j * 4 + 3] < 16) continue; if (skip && flatD2(pal + j * 4, skipC) <= FLAT_R2_FAR) continue; if (flatD2(pal + cand[q] * 4, pal + j * 4) <= FLAT_R2) w += cnt[j]; }
    if (w > bw) { bw = w; bj = cand[q]; } }
  if (bj < 0) return -1;
  float sr = 0.f, sg = 0.f, sb = 0.f, sw = 0.f;
  for (int j = 0; j < 256; j++) { if (!cnt[j] || pal[j * 4 + 3] < 16) continue; if (skip && flatD2(pal + j * 4, skipC) <= FLAT_R2_FAR) continue;
    if (flatD2(pal + bj * 4, pal + j * 4) <= FLAT_R2) { sr += cnt[j] * pal[j * 4]; sg += cnt[j] * pal[j * 4 + 1]; sb += cnt[j] * pal[j * 4 + 2]; sw += cnt[j]; } }
  colOut[0] = (unsigned char)(sr / sw + .5f); colOut[1] = (unsigned char)(sg / sw + .5f); colOut[2] = (unsigned char)(sb / sw + .5f); *wOut = bw; return bj;
}
// плотность резких границ: доля соседних точек (по x и по y), у которых яркость записей палитры различается больше FLAT_EDGE_D (надпись, окно, разметка — границы; зерно асфальта — нет)
#define FLAT_EDGE_D 35
static float flatEdge(const Tex *t, const unsigned char *pal) {
  const unsigned char *px = (const unsigned char *)t->lv[0]; if (!px || t->w < 2 || t->h < 2) return 0.f;
  unsigned char lum[256]; for (int j = 0; j < 256; j++) lum[j] = (unsigned char)(.299f * pal[j * 4] + .587f * pal[j * 4 + 1] + .114f * pal[j * 4 + 2]);
  int w = t->w, h = t->h, rb = texRowBytes(t->t4, t->pre, w), bpr = rb / 16; int st = (int)sqrtf((float)(w * h) / 1500.f); if (st < 1) st = 1;
  #define FE_AT(X, Y, OUT) do { int x_ = (X), y_ = (Y); unsigned o_; \
      if (t->pre) o_ = (unsigned)(((y_ >> 3) * bpr + (t->t4 ? x_ >> 5 : x_ >> 4)) * 128 + (y_ & 7) * 16 + (t->t4 ? ((x_ >> 1) & 15) : (x_ & 15))); else o_ = (unsigned)(y_ * rb + x_); \
      unsigned char b_ = px[o_]; (OUT) = t->t4 ? ((x_ & 1) ? b_ >> 4 : b_ & 15) : b_; } while (0)
  unsigned n = 0, e = 0;
  for (int y = 0; y + 1 < h; y += st) for (int x = 0; x + 1 < w; x += st) { int a, b, c; FE_AT(x, y, a); FE_AT(x + 1, y, b); FE_AT(x, y + 1, c);
    int d1 = (int)lum[a] - lum[b], d2 = (int)lum[a] - lum[c]; if (d1 < 0) d1 = -d1; if (d2 < 0) d2 = -d2; n += 2; if (d1 > FLAT_EDGE_D) e++; if (d2 > FLAT_EDGE_D) e++; }
  #undef FE_AT
  return n ? (float)e / (float)n : 0.f;
}
static unsigned flatScanUs, flatScanN;   // служебное: время и число разборов текстур (строка FLAT)
static void flatScan0(Tex *t, const unsigned char *pal, const unsigned *cnt);
static void flatScan(Tex *t, const unsigned char *pal, const unsigned *cnt) { if (flatFromFile) return; unsigned t0 = sceKernelGetSystemTimeLow(); flatScan0(t, pal, cnt); flatScanUs += sceKernelGetSystemTimeLow() - t0; flatScanN++; }
static void flatScan0(Tex *t, const unsigned char *pal, const unsigned *cnt) {
  t->flatK = 0; t->curb = 0; t->asph = 0; t->flatC = 0; t->flatS = 0; t->flatR = 0; t->flatEd = 0; if (flatOld) return; flatTintKey = ~0u;
  unsigned tot = 0, all = 0, semi = 0;
  for (int j = 0; j < 256; j++) { if (!cnt[j]) continue; all += cnt[j]; if (pal[j * 4 + 3] >= 16) { tot += cnt[j]; if (pal[j * 4 + 3] < 240) semi += cnt[j]; } }
  if (!all || !tot) { t->flatR = 1; return; }
  if ((all - tot) * 50u > all || semi * 50u > all) { t->flatR = 1; return; }   // больше 2 % прозрачных или полупрозрачных точек — вырезы, остаётся с текстурой
  if (t->tstat[4] & 1) { t->flatR = 2; return; }                                 // список notoon (вывески, плакаты, стекло витрин)
  unsigned w1 = 0, w2 = 0; unsigned char c1[3], c2[3];
  if (flatBestCluster(pal, cnt, 0, 0, &w1, c1) < 0) { t->flatR = 1; return; }
  int ground = (t->tstat[4] & 4) != 0;
  if (flatBestCluster(pal, cnt, c1, c1, &w2, c2) >= 0) {   // второй цвет вне 2R от первого
    float contrast = fabsf(flatLuma(c2) - flatLuma(c1)); unsigned c2d = (unsigned)sqrtf((float)flatD2(c1, c2));
    float share = (float)w2 / (float)tot, lim = ground ? .004f : .15f, clim = ground ? 40.f : 55.f;   // у земли — тонкая разметка (1–2 % точек) тоже рисунок
    if ((share >= lim && (contrast >= clim || c2d >= 150u)) || (!ground && share >= .25f && contrast >= 38.f)) { t->flatR = 3; return; }   // вторая половина заметно светлее/темнее — не однотонный материал
  }
  { float ef = flatEdge(t, pal); t->flatEd = (unsigned char)(ef > 1.f ? 255 : ef * 255.f); if (ef > (ground ? FLAT_EDGE_GROUND : FLAT_EDGE_WALL)) { t->flatR = 5; return; } }   // рисунок по границам
  if ((float)w1 / (float)tot < .40f) { t->flatR = 4; return; }               // много разных цветов: картинка, не материал
  t->flatC = (unsigned)c1[0] | (unsigned)c1[1] << 8 | (unsigned)c1[2] << 16; t->flatK = 1; t->flatS = (unsigned char)(255.f * (float)w1 / (float)tot);
}

// ключ порядка кусков в листе клетки: непрозрачное/прозрачное, затем текстура; заливки — по цвету (куски одного цвета рядом, привязка одна)
#define FLAT_SORTKEY(p) ((((p)->r.flags & 3) << 14) | (flatKind((p)->r.tex) ? (0x2000 | ((flatQ[(p)->r.tex] >> 1) & 0xfff)) : ((p)->r.tex & 0xfff)))

// ---- №912 разметка дорог слоем: tools/marks_list.py → marks912.bin рядом с level.shr. Дорожные текстуры вида 3 (асфальт с разметкой) рисуются заливкой с шумом, а разметка — треугольниками
// без текстуры (цвет вершин = цвет разметки после кривой тона × запечённый свет), в тот же лист клетки, тем же матрицей клетки, со сдвигом глубины слоя (F_LAYER).
// Формат MRK2: 'MRK2', ntex, подпись, ncell, ncls u32; ncell × {first, nbig, nsmall} (вершины по 3 на треугольник: сначала крупная разметка, затем мелочь — штрихи мельче 1 м²); ncls × RGB (исходные цвета разметки, 3 байта, до кривой тона, добиты до 4);
// вершины по 12 байт: x, y, z int16 (в осях кусков клетки: sc/32768 · число + (ox, oy, oz)), свет R, G, B, класс цвета, 0, 0. В памяти поле cls: биты 0–3 класс цвета, 4–15 свет по 4 бита (RGB), отдельного массива света нет (12 байт на вершину).
typedef struct { unsigned c; short x, y, z; unsigned short cls; } MkVert;   // как читает GE: цвет 8888 + положение 16 бит (12 байт)
// крупная разметка клетки (v, nbig вершин) читается вместе с клеткой; мелочь (sv, ns вершин) — только когда клетка ближе дальности мелочи: запрос главного потока, читает загрузчик; далеко — выгружается
typedef struct { MkVert *v, *sv; unsigned nbig, ns, epoch, sepoch; volatile int ready, sready, sreq; } MkCell;
static SceUID marksFd = -1, marksSema = -1; static unsigned *marksTab, marksNcell, marksHdr, marksNcls, marksEpoch = 1; static MkCell *marksCell; static int marksOk;
static unsigned char marksRgb[16][3]; static unsigned marksTone[16];   // исходные цвета классов и цвета после кривой тона (ABGR)
static unsigned marksVertsLoaded, marksCellsLoaded, marksSmallLoads, marksSmallDrops, marksVertsResident;
static void *marksGarb[96]; static unsigned marksGarbN;   // память, на которую мог смотреть GE прошлого кадра: освобождается в начале следующего (marksGarbageFlush)
static void marksGarbageFlush(void) { for (unsigned i = 0; i < marksGarbN; i++) free(marksGarb[i]); marksGarbN = 0; }
static void marksFreeLater(void *p) { if (!p) return; if (marksGarbN < 96) marksGarb[marksGarbN++] = p; else free(p); }
#define MK_VT (GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
static volatile unsigned short marksReq[32]; static volatile unsigned marksReqHead, marksReqTail;   // очередь запросов мелочи: пишет главный поток (Head), читает загрузчик (Tail)
static void marksClose(void) {
  if (marksFd >= 0) sceIoClose(marksFd); marksFd = -1; if (marksSema >= 0) sceKernelDeleteSema(marksSema); marksSema = -1;
  if (marksCell) { for (unsigned i = 0; i < marksNcell; i++) { free(marksCell[i].v); free(marksCell[i].sv); } free(marksCell); }
  marksGarbageFlush(); marksReqHead = marksReqTail = 0; marksCell = 0; free(marksTab); marksTab = 0; marksOk = 0; marksNcell = 0; marksVertsResident = 0;
}
// группа вершин клетки из файла: from — номер первой вершины, cnt — число; -> массив MkVert (цвет ставит marksColour) или 0
static MkVert *marksRead(unsigned from, unsigned cnt) {
  if (!cnt) return 0;
  unsigned char *raw = (unsigned char *)malloc((size_t)cnt * 12u); MkVert *v = (MkVert *)memalign(16, (size_t)cnt * sizeof(MkVert)); int ok = raw && v;
  if (ok) { sceKernelWaitSema(marksSema, 1, 0); ok = sceIoLseek32(marksFd, (int)(marksHdr + from * 12u), PSP_SEEK_SET) == (int)(marksHdr + from * 12u) && sceIoRead(marksFd, raw, (int)(cnt * 12u)) == (int)(cnt * 12u); sceKernelSignalSema(marksSema, 1); }
  if (!ok) { free(raw); free(v); return 0; }
  for (unsigned i = 0; i < cnt; i++) { const unsigned char *r = raw + i * 12; const short *p = (const short *)r; v[i].x = p[0]; v[i].y = p[1]; v[i].z = p[2];
    v[i].cls = (unsigned short)((r[9] < 16 ? r[9] : 0) | (r[6] >> 4) << 4 | (r[7] >> 4) << 8 | (r[8] >> 4) << 12); v[i].c = 0xff000000u; }
  free(raw); return v;
}
// крупная разметка клетки: читает загрузчик клеток (cellFinish) или главный поток по требованию (force)
static void marksCellLoad2(unsigned ci, int force) {
  // игрок оставил родные текстуры — разметка не нужна (память и чтение; при смене подгрузится по требованию)
  if (!marksOk || ci >= marksNcell || (flatTexUser && !force)) return; MkCell *m = &marksCell[ci]; if (m->ready) return;
  unsigned first = marksTab[ci * 3], nbig = marksTab[ci * 3 + 1], ns = marksTab[ci * 3 + 2];
  MkVert *v = marksRead(first, nbig); if (nbig && !v) return;
  m->v = v; m->nbig = nbig; m->ns = ns; m->epoch = 0; __asm__ volatile("" ::: "memory"); m->ready = 1; marksCellsLoaded++; marksVertsLoaded += nbig; marksVertsResident += nbig;
}
static void marksCellLoad(unsigned ci) { marksCellLoad2(ci, 0); }
// мелочь клетки: читает загрузчик по запросу главного потока (marksServe) — или сразу, если загрузчика нет
static void marksSmallLoad(unsigned ci) {
  if (!marksOk || ci >= marksNcell) return; MkCell *m = &marksCell[ci]; if (!m->ready || m->sready) { m->sreq = 0; return; }
  MkVert *sv = marksRead(marksTab[ci * 3] + marksTab[ci * 3 + 1], marksTab[ci * 3 + 2]);
  if (sv) { m->sv = sv; m->sepoch = 0; marksSmallLoads++; marksVertsResident += m->ns; __asm__ volatile("" ::: "memory"); m->sready = 1; }
  m->sreq = 0;
}
static void marksServe(void) { while (marksReqTail != marksReqHead) { unsigned ci = marksReq[marksReqTail & 31]; marksReqTail++; marksSmallLoad(ci); } }
static int marksLoaderOn;   // загрузчик клеток запущен (loaderUp): запросы можно отдавать ему
static void marksReqSmall(unsigned ci, void (*wake)(void)) {
  MkCell *m = &marksCell[ci]; if (m->sreq || m->sready) return;
  if (!marksLoaderOn || marksReqHead - marksReqTail >= 31) { marksSmallLoad(ci); return; }
  m->sreq = 1; marksReq[marksReqHead & 31] = (unsigned short)ci; __asm__ volatile("" ::: "memory"); marksReqHead++; wake();
}
static void marksCellFree(unsigned ci) {
  if (!marksCell || ci >= marksNcell) return; MkCell *m = &marksCell[ci]; m->ready = 0; m->sready = 0;
  if (m->v) marksVertsResident -= m->nbig; if (m->sv) marksVertsResident -= m->ns;
  marksFreeLater(m->v); marksFreeLater(m->sv); m->v = m->sv = 0; m->nbig = m->ns = 0; m->epoch = m->sepoch = 0;
}
static void marksSmallDrop(unsigned ci) {   // клетка далеко: мелочь не держим (список клетки с мелочью выбросит вызывающий)
  MkCell *m = &marksCell[ci]; if (!m->sready) return; m->sready = 0; marksVertsResident -= m->ns; marksFreeLater(m->sv); m->sv = 0; m->sepoch = 0; marksSmallDrops++;
}
// цвета классов после кривой тона (мультик), по цвету вершины: тон разметки × запечённый свет; вызывает главный поток при сборке листа
static void marksToneBuild(void) {
  for (unsigned k = 0; k < marksNcls && k < 16; k++) { unsigned char c[4] = {marksRgb[k][0], marksRgb[k][1], marksRgb[k][2], 255}; toneColorRGB(c); marksTone[k] = (unsigned)c[0] | (unsigned)c[1] << 8 | (unsigned)c[2] << 16; }
}
static void marksColour(MkVert *v, unsigned n) {
  for (unsigned i = 0; i < n; i++) { unsigned c = v[i].cls, t = marksTone[c & 15], lr = ((c >> 4) & 15) * 17u, lg = ((c >> 8) & 15) * 17u, lb = ((c >> 12) & 15) * 17u;
    unsigned r = (t & 255) * lr / 255, g = ((t >> 8) & 255) * lg / 255, b = ((t >> 16) & 255) * lb / 255; v[i].c = 0xff000000u | b << 16 | g << 8 | r; }
  sceKernelDcacheWritebackRange(v, n * sizeof(MkVert));
}
