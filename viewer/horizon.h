// Горизонт: плоский однотонный силуэт дальнего города (слово Роба 16.10: «фасады как в вестернах, силуэт дома, маяк виден издалека»).
// Файл horizon.shr уровня (tools/horizon_export.py, HOR1): для клеток сетки заранее посчитан контур верхней кромки всего, что дальше дальности города (вектор: азимут + tan угла места).
// Рисуется лентой треугольников по цилиндру вокруг камеры (радиус HZ_R чуть дальше дальности города), без текстуры, без записи и проверки глубины, ДО подложки и города:
// ближний настоящий 3D проявляется поверх. Цвет — тон горизонта уровня (fogCol), притемнённый; вершины пересобираются только при смене клетки или fogCol.
// Цена: до ~300 вершин, один вызов GE. Выключено по умолчанию: «flag horizon 1/0» в сценарии или файл horizon рядом с EBOOT. HORIZON_OLD=1 — функция не рисует вовсе (прежнее)
#pragma once
#ifndef HORIZON_OLD
#define HORIZON_OLD 0
#endif
#include "horizon_chord.h"
#include "far909.h"
#include "hzzone913.h"
// №997 (Роб 26.10 после TEST70: «вдали тяжело грузить родные текстуры, но можно на Маке понять, какого примерно цвета фасады дальних домов, и раскрасить их, как машины до текстур. Сейчас они в тон уровня, неплохо, но иногда палится»):
// хвост HCL1 horizon.shr (tools/horizon_export.py, смещение в заголовке DST2) — цвет фасада на каждой вершине контура (средний цвет текстуры × свет вершин, палитра уровня 16 цветов, 4 бита на вершину).
// Верх мировой ленты и карточки ориентиров красятся прежним тоном силуэта (fogCol, притемнённый), умноженным на относительный цвет фасада (фасад / среднее по палитре): оттенок виден, яркость остаётся яркостью дымки (ночью тёмная); влияние 90 % на 150 м → 50 % на 900 м, вдали почти прежнее.
// Нет хвоста в файле (прежние данные) или HZCOL997_OLD=1 / файл hzcol997old рядом с EBOOT / «flag hzcol997 0» — лента прежнего тона. Цена: те же вершины и вызов, на клетку лишние 3 столбца цвета при пересборке
#ifndef HZCOL997_OLD
#define HZCOL997_OLD 0
#endif
#ifndef HZ_HC_W0
#define HZ_HC_W0 90   // %: влияние цвета фасада на ближней границе (150 м)
#endif
#ifndef HZ_HC_W1
#define HZ_HC_W1 50   // %: то же на 900 м и дальше
#endif
static unsigned hzFacadeCol(unsigned colT, unsigned f1, unsigned f2, float w2, float D);
static void hzFacadeGain(unsigned f, float D, unsigned short *g3);
static unsigned hzHcInv[3];   // №997: 256·65536 / среднее по палитре HCL1 (канал), считается один раз при загрузке: на колонку ленты — умножение, не деление
static const unsigned char *hzLmCol;   // №997: цвета карточек ориентиров (r, g, b, 0) в хвосте HCL1, по таблице ориентиров (0 — нет)
static int hzCol997Old = HZCOL997_OLD; static const unsigned char *hzHcl;   static unsigned hzTopSpread;   // №997 (гейт, только со смоуком): разброс цвета верхних вершин мировой ленты по столбцам, сумма СКО каналов ×10
   // hzHcl — хвост HCL1 (0 — нет, лента прежняя)
#define HZ_MAXV 1600   // №911: было 1400; вершин ленты на клетку до 4 × (контур + промежуточные по дуге)
#define HZ_FADE 0.14f   // градиент: высота перехода в дымку под верхним краем контура, доля радиуса
#define HZ_MAXLM 16
#define HZ_SLICE 24   // столбцов контура за кадр при пересборке (≈ 4–8 кадров на клетку)
#define HZ_LM_HIDE 10.f   // карточка ориентира гаснет, когда до него ближе drawDist−10 м (настоящая башня к этому моменту уже в дымке дальнего края); перекрытие 10 м — без пустого кадра
#define HZ_LMV 64
#define HZ_INK 0.45f   // доля тёмного тона в цвете силуэта (0 — ровно fogCol, 1 — чёрный)
typedef struct { float cx, cz, ey; unsigned nv, off, p0, p1, p2; } HzCell;
typedef struct { unsigned c; float x, y, z; } HzVtx;
typedef struct { float cx, cz, y0, h; unsigned np, off, p0, p1; } HzLm;
#ifndef HZ_LM928_OLD
#define HZ_LM928_OLD 0
#endif
static int hzLmOn = 1;   // №928: ориентиры дали (до 16 на уровень, автоотбор tools/landmark_pick.sh) одним вызовом и только впереди; «flag hzlm 0» — без ориентиров (A/B гейта); HZ_LM928_OLD=1 — прежний путь (по вызову на ориентир)
// №1042 (Роб 26.10, TEST72 на PSP: «вдалеке белая стена», «цветовые силуэты домов не получились»; «за краем дальности только небо»): лента силуэтов горизонта (№887, цвета фасадов №997, карточки ориентиров) не рисуется и не загружается ни на одном уровне.
// Даль даёт стрим настоящих домов (Оптимизация) и дальность по выбору игрока (Небо). HZOFF1042_OLD=1 / файл hzoff1042old рядом с EBOOT / «flag hzoff1042 0» — лента как в TEST72
#ifndef HZOFF1042_OLD
#define HZOFF1042_OLD 0
#endif
static int hzOff1042Old = HZOFF1042_OLD;
static int horizonOn; static int hzTrace; static float hzCullD;   // №909 диагностика: «flag hztrace 1» — строка HZT на кадр (глаз, взгляд, радиус города, рисуемая клетка ленты)
#ifndef FAR150_OLD
#define FAR150_OLD 0
#endif
#define FAR150_D 150.f
#define FAR150_UP 8.f   // м над глазом клетки силуэта (ey расчёта)
#ifndef HZ130_ON
#define HZ130_ON 0
#endif
static int hz130On = HZ130_ON;   // №913: 130 м вместо 150 в зоне «130» (файл far130 рядом с EBOOT или «flag far130 1»)
static Hz913 hz913St; static HzZone hzZ; static unsigned char *hzZData; static int hzSetCur = -1, hzSetWant;   // №913: радиус города по зонам, таблица зон, рисуемый контур (0 — от 130, 1 — от 150)
static Far909 far909St;   // №909: состояние «высокого взгляда» (far909.h)
static int far150On = 1;   // №887: город до 150 м, дальше силуэты (main_loop.h)
static unsigned char *hzData; static unsigned hzN, hzSize; static int hzLevel = -99; static float hzR = 260.f; static const HzCell *hzC; static const HzLm *hzL; static unsigned hzNL;
static unsigned hzUsAcc, hzUsN, hzUsMax;   // цена рисования (процессор), мкс: сумма, число кадров, максимум; строка HORIZON cost в журнале смоука раз в 30 кадров
static int hzCur = -1; static unsigned hzCol, hzFogB; static unsigned hzVerts, hzCalls, hzNv;   // последний кадр: вершин, вызовов
#ifndef HZ_VB934_OLD
#define HZ_VB934_OLD 0   // 1 — два буфера прежней ленты статически в образе (51 КБ BSS); 0 — по требованию из кучи: мировая лента (№934) их не использует
#endif
#if HZ_VB934_OLD
static HzVtx hzVbS[2][HZ_MAXV] __attribute__((aligned(64))); static HzVtx *hzVbBuf = &hzVbS[0][0];
static int hzVbEnsure(void) { return 1; }
#else
static HzVtx *hzVbBuf;
static int hzVbEnsure(void) { if (!hzVbBuf) { hzVbBuf = (HzVtx *)malloc(2 * HZ_MAXV * sizeof(HzVtx)); if (hzVbBuf) memset(hzVbBuf, 0, 2 * HZ_MAXV * sizeof(HzVtx)); } return hzVbBuf != 0; }
#endif
#define hzVb(i) (hzVbBuf + (size_t)(i) * HZ_MAXV)   // два буфера ленты: рисуется активный, в другой порциями собирается новая (смена клетки без скачка кадра)
static unsigned short hzXaz[HZ_MAXV / 4]; static short hzXt[HZ_MAXV / 4]; static unsigned hzXn;   // №911: контур собираемой клетки с промежуточными вершинами (horizon_chord.h)
// №934 (HOR2, слово дирижёра 24.10: «дома прыгают»; разбор Контура: скачки ленты 25–30° при смене клетки на склоне). Вершина контура несёт расстояние D до своей точки (хвост DST2 horizon.shr, 4 бита, 130 м × 1,19^k). Лента строится в МИРОВЫХ
// координатах и с НАСТОЯЩЕГО глаза: контуры двух ближайших клеток сводятся на общую сетку азимутов (2°), точки (cx + D sin a, ey + tan·D, cz − D cos a) смешиваются по положению глаза (вторая клетка 0…0,5 в полосе HZ_WBAND у границы клеток,
// у границы обе по 0,5 — непрерывно), проекция каждой точки на цилиндр дальности по направлению с глаза. Раньше: контур одной клетки, высота поправлялась на (ey клетки − ey глаза)/154 м одинаково для всех, а дом стоит на D ≥ 300 м,
// и смена клетки на склоне прыгала на 25–30°. Нет хвоста DST2 в файле или HZ_WORLD934_OLD=1 / «flag hzworld 0» — прежняя лента в осях клетки. Цена: пересчёт при сдвиге глаза больше HZ_WSTEP м (стоя — ни разу), на кадр копия вершин
#ifndef HZ_WORLD934_OLD
#define HZ_WORLD934_OLD 0
#endif
#ifndef HZ_WBAND
#define HZ_WBAND 40.f   // м: ширина полосы подмеса соседних клеток (аналитика tools/hz934_jump.py: при 40 м p90 скачка за шаг 3 м ≤ 3,4° на любом перепаде)
#endif
#ifndef HZ_WSTEP
#define HZ_WSTEP 1.5f   // м: сдвиг глаза, при котором мировая лента пересчитывается (на дальности 150+ м < 0,6° ≈ 3 px)
#endif
#define HZ_RN 180   // столбцов общей сетки азимутов (2°); рисуются не все (HZ_WCOLS934_OLD=0): столбцы у вершин контура и редкая сетка для хорды, вершин ленты 4 × столбцов (было 4 × 181 = 724, цена копии и GE ×5 против прежних 148)
#ifndef HZ_WCOLS934_OLD
#define HZ_WCOLS934_OLD 0   // 1 — все 181 столбец сетки
#endif
#ifndef HZ_WFILL
#define HZ_WFILL 5   // каждый 5-й столбец (10°) входит всегда: хорда между вершинами не провисает (при 10° стрелка 0,4 % радиуса)
#endif
static int hzWorldOn = !HZ_WORLD934_OLD; static const unsigned char *hzDst;   // hzDst — хвост DST2 (0 — нет, лента прежняя)
static const float hzDq[16] = { 130.f, 154.7f, 184.1f, 219.1f, 260.7f, 310.2f, 369.2f, 439.3f, 522.8f, 622.1f, 740.3f, 880.9f, 1048.3f, 1247.5f, 1484.5f, 1766.5f };   // шкала расстояний DST2: 130 · 1,19^k
typedef struct { int cell, set; unsigned stamp; float cx, cz, ey, D[HZ_RN + 1], t[HZ_RN + 1]; unsigned char km[HZ_RN + 1]; unsigned hc[HZ_RN + 1]; unsigned short gn[HZ_RN + 1][3]; } HzRs;   // gn — множители цвета фасада ×256 по каналам (№997), 0 — нет цвета   // контур клетки на общей сетке азимутов: расстояние и tan по столбцам (8 Б на столбец) и отметка «столбец рисуется»
#define HZ_RSN 3   // клеток в кэше: у границы нужны две, третья — запас от перебора на углу; кэш 3 × 1,6 КБ (прежний 4 × 3,1 КБ с готовыми x, y, z стоил кучи уровня)
static HzRs hzRs[HZ_RSN]; static unsigned hzRsTick; static float hzSinT[HZ_RN + 1], hzCosT[HZ_RN + 1]; static int hzSinInit;
static HzVtx hzWV[4 * (HZ_RN + 1)] __attribute__((aligned(64))); static float hzWex, hzWey, hzWez, hzWR, hzWw2; static unsigned hzWnc = HZ_RN + 1; static unsigned hzWsig, hzWUsAcc, hzWUsN, hzWBuilds;
static int hzAct; static int hzBldCell = -1, hzBldSet; static unsigned hzBldK, hzBldCol, hzBldFog;
__attribute__((noinline)) static float hz913Cap(float ex, float ez, float dt) {   // №913: потолок радиуса города по зоне глаза (hzzone913.h); base — радиус ленты минус 4 м (130 у новых лент, 150 у прежних)
  float base = hz130On ? hzR - 4.f : 150.f; return hz913Step(&hz913St, hzZoneMode(&hzZ, ex, ez, base < 140.f ? 1 : 0), base, dt);   // 130 м по умолчанию — только с флагом far130 (слово дирижёра 24.10: Роб решает по новой цифре выигрыша); без него зона «130» держит 150, зоны 250 работают
}
// №1042 (Роб на PSP, TEST72: «поставил в настройках 250 м — ничего не видно; вдалеке большая белая стена»): строка PICTURE «Draw Distance» поднимала только ПОЛ регулятора (ddFloorOver), а потолок города вне зон 250 м
// (№887/№913) оставался 150 м: при выбранных 200/250 город всё равно обрывался на 150, дальше шла лента на 154 м в дымке. Теперь потолок не ниже выбранного игроком (FAR1042_OLD=1 / файл far1042old / flag far1042 0 — прежний потолок);
// лента силуэтов сама растягивается до drawDist + 4 (равномерный масштаб, строка ~377), туман города идёт от drawDist
#ifndef FAR1042_OLD
#define FAR1042_OLD 0
#endif
static int far1042On = !FAR1042_OLD;
__attribute__((noinline)) static float hz913Apply(float dd, float ex, float ez, float dt, int ok, int hi) {   // потолок города: возвращает новую drawDist
  if (FAR150_OLD || !far150On || !hzN || hzR >= FAR150_D + 30.f) return dd;
  float cap = HZ913_OLD ? FAR150_D : hz913Cap(ex, ez, dt);
  if (far1042On && ddFloorOver > cap) cap = ddFloorOver;   // №1042: Draw Distance 200/250 в настройках — город до выбранного, не до 150
  return ok && dd > cap && !hi ? cap : dd;
}
static void horizonFree(void) { if (!HZ_VB934_OLD) { free(hzVbBuf); hzVbBuf = 0; } hzDst = 0; hzHcl = 0; hzLmCol = 0; for (int q = 0; q < HZ_RSN; q++) hzRs[q].cell = -1; hzWsig = 0; hzWex = hzWey = hzWez = 1e9f; hzWR = 0.f; free(hzZData); hzZData = 0; memset(&hzZ, 0, sizeof hzZ); memset(&hz913St, 0, sizeof hz913St); hzSetCur = -1; free(hzData); hzData = 0; hzN = 0; hzC = 0; hzL = 0; hzNL = 0; hzCur = -1; hzBldCell = -1; hzLevel = -99; }
static void horizonLoad(int level) {
  horizonFree(); hzLevel = level; // одна попытка на уровень: нет файла — молча без горизонта; №917: панорамы зон 250 м этого уровня (нет файла — как раньше)
  char fn[256]; levelFile(fn, "horizon.shr");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int sz = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  if (sz < 32 || sz > (1 << 20)) { sceIoClose(f); return; }
  unsigned char *m = (unsigned char *)memalign(64, (size_t)sz);
  if (!m || sceIoRead(f, m, (SceSize)sz) != sz || memcmp(m, "HOR1", 4)) { sceIoClose(f); free(m); return; }
  sceIoClose(f);
  unsigned n = *(unsigned *)(m + 4); float r = *(float *)(m + 8);
  if (32 + 32ull * n > (unsigned)sz) { free(m); return; }
  for (unsigned i = 0; i < n; i++) { const HzCell *c = (const HzCell *)(m + 32) + i; if (c->nv < 2 || c->nv > HZ_CHORD_NV || c->off + 4ull * c->nv > (unsigned)sz) { free(m); return; } }
  for (unsigned i = 0; i < n; i++) { HzCell *c = (HzCell *)(m + 32) + i; if (c->p2 && (c->p2 < 2 || c->p2 > HZ_CHORD_NV || c->p1 + 4ull * c->p2 > (unsigned)sz)) { c->p1 = 0; c->p2 = 0; } }   // №913: второй контур (от 150 м) — p1 смещение, p2 число вершин; нет или битый — зона 150 рисуется контуром от 130
  unsigned nl = *(unsigned *)(m + 12), lo = *(unsigned *)(m + 16);
  if (nl > HZ_MAXLM || (nl && lo + 32ull * nl > (unsigned)sz)) nl = 0;
  for (unsigned i = 0; i < nl; i++) { const HzLm *l = (const HzLm *)(m + lo) + i; if (l->np < 2 || l->np > HZ_LMV / 2 || l->off + 4ull * l->np > (unsigned)sz) { nl = 0; break; } }
  hzDst = 0;   // №934: хвост DST2 (расстояния вершин, 4 бита) — подвал 'DSTF'+смещение; размер и хвост кратны 4 (u32 читаются без выравнивания только так); битый — как нет
  if (sz >= 64 && !(sz & 3) && !memcmp(m + sz - 16, "DSTF", 4)) { unsigned t0 = *(unsigned *)(m + sz - 12);
    if (!(t0 & 3) && t0 + 16ull + 4ull * n <= (unsigned)sz - 16 && !memcmp(m + t0, "DST2", 4) && *(unsigned *)(m + t0 + 4) == n) {
      int ok = 1; for (unsigned i = 0; i < n && ok; i++) { const HzCell *c = (const HzCell *)(m + 32) + i; unsigned o0 = *(unsigned *)(m + t0 + 16 + 4 * i);
        if (c->nv < 2 || t0 + (unsigned long long)o0 + (c->nv + 1) / 2 + (c->p2 + 1) / 2 > (unsigned)sz - 16) ok = 0; }
      if (ok) hzDst = m + t0;
      if (ok) { unsigned ho = *(unsigned *)(m + t0 + 8);   // №997: хвост HCL1 перед DST2 (смещение в заголовке DST2; 0 — нет)
        if (ho && !(ho & 3) && ho + 80ull + 4ull * n <= t0 && !memcmp(m + ho, "HCL1", 4) && *(unsigned *)(m + ho + 4) == n) {
          int ok2 = 1; for (unsigned i = 0; i < n && ok2; i++) { const HzCell *c = (const HzCell *)(m + 32) + i; unsigned o0 = *(unsigned *)(m + ho + 80 + 4 * i);
            if (ho + (unsigned long long)o0 + (c->nv + 1) / 2 + (c->p2 + 1) / 2 > t0) ok2 = 0; }
          if (ok2) { hzHcl = m + ho; for (int c = 0; c < 3; c++) { unsigned sm_ = 0; for (int k = 0; k < 16; k++) sm_ += m[ho + 16 + 4 * k + c]; hzHcInv[c] = 16u * 256u * 4096u / (sm_ + 16u); } unsigned lmo = *(unsigned *)(m + ho + 8); if (nl && lmo && !(lmo & 3) && lmo + 4ull * nl <= t0) hzLmCol = m + lmo; } } } } }
  hzData = m; hzSize = (unsigned)sz; hzN = n; hzC = (const HzCell *)(m + 32); hzNL = nl; hzL = nl ? (const HzLm *)(m + lo) : 0; if (r > 50.f && r < 1000.f) hzR = r;
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "HZLOAD level=%d bytes=%u dst=%d free=%u\n", level, (unsigned)sz, hzDst != 0, heapFreeBytes()); smkWrite(s_); }   // №934: куча сразу после загрузки ленты (точные байты; гейт кучи сверяет с базой)
  { char zn[256]; levelFile(zn, "hzzone.bin"); SceUID zf = sceIoOpen(zn, PSP_O_RDONLY, 0);   // №913: таблица зон дальности ('HZN1', w, h u32, x0, z0, шаг f32, w·h байт)
    if (zf >= 0) { unsigned char zb[24];
      if (sceIoRead(zf, zb, 24) == 24 && !memcmp(zb, "HZN1", 4)) { unsigned zw, zhh; float zg[3]; memcpy(&zw, zb + 4, 4); memcpy(&zhh, zb + 8, 4); memcpy(zg, zb + 12, 12);
        if (zw && zhh && zw * zhh <= 65536u && zg[2] > 1.f && (hzZData = (unsigned char *)malloc(zw * zhh)) && sceIoRead(zf, hzZData, (int)(zw * zhh)) == (int)(zw * zhh)) { hzZ.x0 = zg[0]; hzZ.z0 = zg[1]; hzZ.step = zg[2]; hzZ.w = zw; hzZ.h = zhh; hzZ.m = hzZData; }
        else { free(hzZData); hzZData = 0; } }
      sceIoClose(zf); } }
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "HORIZON loaded level=%d cells=%u lm=%u bytes=%u\n", level, n, hzNL, (unsigned)sz); smkWrite(s); }
}
// лента по контуру клетки i: вершины в осях камеры (x, z на радиусе hzR; y — угол места × радиус), высота привязана к миру матрицей
// цвет дымки, в которую уходит дальний город: ровно тот, что идёт в sceGuFog города (cityFogColor, main_stream.h; при fogsky = fogCol, иначе groundCover.rgb)
static unsigned hzFogCol(void) { return cityFogColor(); }
static int hzMode(void) { return horizonOn == 6 ? 1 : horizonOn == 7 ? 4 : horizonOn == 8 ? 5 : horizonOn; }   // базовый цвет без учёта градиента
static int hzGrad(void) { return horizonOn >= 6 && horizonOn <= 8; }
#ifdef HORIZON_CONTRAST_OLD
#define HORIZON_CONTRAST_OLD_V 1
#else
#define HORIZON_CONTRAST_OLD_V 0
#endif
#ifndef HORIZON_GROUND_OLD
#define HORIZON_GROUND_OLD 0
#endif
#define HZ_GROUND_K 42u     // % яркости средней земли (текстуры в игре под тоном уровня темнее сырых палитр)
#define HZ_GROUND_FOG 12u   // % дымки в цвете земли ленты: даль чуть светлее ближней земли
// цвет низа ленты (дальняя земля): средняя земля 120-450 м из горизонта клетки (p0 = 0x00BBGGRR), смешанная с дымкой; нет данных — цвет дымки, как раньше
static unsigned hzGroundCol(const HzCell *c, unsigned fog) {
  if (HORIZON_GROUND_OLD || !c->p0) return 0xff000000u | (fog & 0xffffffu);
  unsigned g = c->p0, o = 0xff000000u;
  for (int sh = 0; sh <= 16; sh += 8) { unsigned gv = ((g >> sh) & 255u) * HZ_GROUND_K / 100u, fv = (fog >> sh) & 255u; o |= (((gv * (100u - HZ_GROUND_FOG) + fv * HZ_GROUND_FOG) / 100u) & 255u) << sh; }
  return o;
}
static unsigned horizonColor(void) {
  int m = hzMode();
  if (m != 1 && m != 4 && m != 5) return 0xffff00ffu;
  unsigned fc = hzFogCol(), fr = fc & 255u, fg = (fc >> 8) & 255u, fb = (fc >> 16) & 255u, r, g, b;
  if (m == 5) { unsigned sc = skyHorizonColor(), sr = sc & 255u, sg = (sc >> 8) & 255u, sb = (sc >> 16) & 255u; r = (fr * 6u + sr * 4u) / 10u; g = (fg * 6u + sg * 4u) / 10u; b = (fb * 6u + sb * 4u) / 10u;
#ifndef HORIZON_CONTRAST_OLD
    r = r * 38u / 100u + 8u; g = g * 40u / 100u + 12u; b = b * 45u / 100u + 40u;   // темнее и холоднее неба: на закате силуэт не тонет в небе (Роб, причал L3)
#else
    r = r * 9u / 10u; g = g * 9u / 10u; b = b * 9u / 10u;
#endif
  }
  else if (m == 4) { r = fr * 85u / 100u; g = fg * 85u / 100u; b = fb * 85u / 100u; }
  else { unsigned ink = 255u - (unsigned)(HZ_INK * 255.f); r = fr * ink / 255u; g = fg * ink / 255u; b = fb * ink / 255u; }
  return 0xff000000u | (b << 16) | (g << 8) | r;
}
// лента: две полосы по три ряда вершин (верх контура / переход / низ под землёй). Без градиента все ряды одного цвета. Полоса A: хранится в hzV[0..hzNv), полоса B — следом
// пересборка ленты клетки i в неактивный буфер порциями по HZ_SLICE столбцов (k0..k1); true — готово
static int horizonBuildSlice(int i, unsigned col, unsigned k0, unsigned k1) {
  const HzCell *c = &hzC[i]; unsigned nv = hzXn; HzVtx *V = hzVb(hzAct ^ 1);
  unsigned colB = hzGrad() ? hzGroundCol(c, hzFogCol()) : col;
  if (k1 > nv) k1 = nv;
  for (unsigned k = k0; k < k1; k++) {
    unsigned short az = hzXaz[k]; short t = hzXt[k];
    float a = (float)az * (6.2831853f / 65536.f), x = hzR * sinf(a), z = -hzR * cosf(a);
    float yt = horizonOn == 3 ? hzR * 0.2f : hzR * (float)t * (1.f / 16384.f), ym = yt - hzR * HZ_FADE, yb = -hzR * 0.5f;
    if (ym < yb || !hzGrad()) ym = yb;   // без градиента полоса A тянется от верха до низа
    unsigned colT = col; if (hzGrad() && !HORIZON_CONTRAST_OLD_V) { float tt = (float)t * (1.f / 16384.f), f = tt * (1.f / 0.05f); f = f < 0.f ? 0.f : f > 1.f ? 1.f : f; unsigned c0 = colB & 255u, c1 = (colB >> 8) & 255u, c2 = (colB >> 16) & 255u, d0 = col & 255u, d1 = (col >> 8) & 255u, d2 = (col >> 16) & 255u; unsigned q = (unsigned)(f * 256.f); colT = 0xff000000u | (((c2 * (256u - q) + d2 * q) >> 8) << 16) | (((c1 * (256u - q) + d1 * q) >> 8) << 8) | ((c0 * (256u - q) + d0 * q) >> 8); }   // выше линии горизонта глаза (дома, холмы) — тёмный силуэт, ниже (дальняя земля) — цвет дымки, без шва с землёй
    V[2 * k] = (HzVtx){colT, x, yt, z};       V[2 * k + 1] = (HzVtx){colB, x, ym, z};   // полоса A
    V[2 * nv + 2 * k] = (HzVtx){colB, x, ym, z}; V[2 * nv + 2 * k + 1] = (HzVtx){colB, x, yb, z};   // полоса B
  }
  return k1 >= nv;
}
// один шаг: если цель (клетка, цвет) не совпадает с рисуемой — начать/продолжить пересборку; первая лента уровня собирается за раз. Когда готова — смена буферов за кадр
static void horizonUpdate(int best, unsigned col) {
  if (!hzVbEnsure()) return;   // нет места под буферы прежней ленты — ленты нет (hzCur < 0: horizonDraw выходит)
  unsigned fog = hzFogCol();
  int set_ = hzSetWant && hzC[best].p2;   // №913: контур от 150 м, если он есть в клетке и город сейчас до 150+ м
  if (hzCur == best && hzCol == col && hzFogB == fog && hzSetCur == set_) { hzBldCell = -1; return; }
  if (hzBldCell != best || hzBldCol != col || hzBldFog != fog || hzBldSet != set_) { hzBldCell = best; hzBldCol = col; hzBldFog = fog; hzBldSet = set_; hzBldK = 0; { const HzCell *bc_ = &hzC[best]; unsigned so_ = set_ ? bc_->p1 : bc_->off, sn_ = set_ ? bc_->p2 : bc_->nv; hzXn = hzChordExpand(hzData + so_, sn_, hzXaz, hzXt); } }   // один вызов разбора вместо двух подставленных копий
  unsigned nv = hzXn, step = hzCur < 0 ? nv : HZ_SLICE;
  if (horizonBuildSlice(best, col, hzBldK, hzBldK + step)) {
    hzAct ^= 1; hzNv = nv; hzCur = best; hzCol = col; hzFogB = fog; hzSetCur = set_; hzBldCell = -1;
    if (smkOn) { char s[128]; snprintf(s, sizeof s, "HORIZON cell=%d at=%.0f,%.0f ey=%.1f nv=%u col=%08x grad=%d\n", best, hzC[best].cx, hzC[best].cz, hzC[best].ey, nv, col, hzGrad()); smkWrite(s); }
  } else hzBldK += step;
}
// ориентиры: профиль вращения (радиус, высота) -> лента L,R вдоль высоты, лицом к камере на расстоянии не дальше drawDist+4 (дальше дальней плоскости вида не нарисовать), с тем же угловым размером (равномерный масштаб от глаза).
// Ближе drawDist−HZ_LM_HIDE не рисуем: там настоящая геометрия (карточка симметрична по оси, у настоящей башни пристройка с одной стороны: поверх неё карточка торчала бы до ~7 м)
// прежний путь (до №928): по вызову на ориентир, без отбора по взгляду — HZ_LM928_OLD=1
static void horizonLmDrawOld(float ex, float ey, float ez, unsigned col) {
  for (unsigned q = 0; q < hzNL; q++) {
    const HzLm *l = &hzL[q]; float dx = l->cx - ex, dz = l->cz - ez, D = sqrtf(dx * dx + dz * dz);
    if (D < drawDist - HZ_LM_HIDE || D > 1500.f || D < 1.f) continue;
    float Dp = D < drawDist + 4.f ? D : drawDist + 4.f, s = Dp / D, ux = dx / D, uz = dz / D, rx = -uz, rz = ux;
    float bx = ux * Dp, bz = uz * Dp, by = (l->y0 - ey) * s;
    const unsigned char *pp = hzData + l->off; unsigned n = l->np;
    HzVtx *v = (HzVtx *)sceGuGetMemory(2 * n * sizeof(HzVtx));
    for (unsigned k = 0; k < n; k++) {
      float r = (float)*(const unsigned short *)(pp + 4 * k) * 0.01f * s, h = (float)*(const unsigned short *)(pp + 4 * k + 2) * 0.01f * s;
      v[2 * k] = (HzVtx){col, bx - rx * r, by + h, bz - rz * r}; v[2 * k + 1] = (HzVtx){col, bx + rx * r, by + h, bz + rz * r};
    }
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)(2 * n), 0, v); hzVerts += 2 * n; hzCalls++;
  }
}
static void horizonLmDraw(float ex, float ey, float ez, unsigned col) {
  // №928: все видимые ориентиры одним вызовом (полосы склеены вырожденными треугольниками) и только впереди (за спиной и далеко сбоку не строим): до 16 ориентиров на уровень, цена ≈ 1 вызов вместо 16
  float fl = sqrtf(cfX * cfX + cfZ * cfZ), fx = fl > .3f ? cfX / fl : 0.f, fz = fl > .3f ? cfZ / fl : 0.f; unsigned tot = 0; unsigned char vis[HZ_MAXLM];
  for (unsigned q = 0; q < hzNL && q < HZ_MAXLM; q++) {
    const HzLm *l = &hzL[q]; float dx = l->cx - ex, dz = l->cz - ez, D = sqrtf(dx * dx + dz * dz); vis[q] = 0;
    if (D < drawDist - HZ_LM_HIDE || D > 1500.f || D < 1.f) continue;
    if (fl > .3f && (dx * fx + dz * fz) / D < 0.45f) continue;   // вне ±63° от взгляда не видно (кадр ±40…45° + полуширина карточки до 18°)
    vis[q] = 1; tot += 2 * l->np + (tot ? 2u : 0u);
  }
  if (!tot) return;
  HzVtx *v = (HzVtx *)sceGuGetMemory(tot * sizeof(HzVtx)); unsigned k = 0;
  for (unsigned q = 0; q < hzNL && q < HZ_MAXLM; q++) {
    if (!vis[q]) continue;
    const HzLm *l = &hzL[q]; float dx = l->cx - ex, dz = l->cz - ez, D = sqrtf(dx * dx + dz * dz);
    float Dp = D < drawDist + 4.f ? D : drawDist + 4.f, s = Dp / D, ux = dx / D, uz = dz / D, rx = -uz, rz = ux;
    float bx = ux * Dp, bz = uz * Dp, by = (l->y0 - ey) * s;
    const unsigned char *pp = hzData + l->off; unsigned n = l->np;
    unsigned lc = col; if (hzLmCol && !hzCol997Old && q < hzNL) { unsigned c4 = *(const unsigned *)(hzLmCol + 4 * q) & 0xffffffu; if (c4) lc = hzFacadeCol(col, c4, 0u, 0.f, D); }   // №997: карточка ориентира — цвет его фасада, смешанный с прежним тоном по расстоянию
    for (unsigned m = 0; m < n; m++) {
      float r = (float)*(const unsigned short *)(pp + 4 * m) * 0.01f * s, h = (float)*(const unsigned short *)(pp + 4 * m + 2) * 0.01f * s;
      HzVtx a = {lc, bx - rx * r, by + h, bz - rz * r}, b = {lc, bx + rx * r, by + h, bz + rz * r};
      if (!m && k) { v[k] = v[k - 1]; v[k + 1] = a; k += 2; }   // стык полос: вырожденные треугольники (последняя вершина предыдущей и первая этой повторены)
      v[k++] = a; v[k++] = b;
    }
  }
  sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)k, 0, v); hzVerts += k; hzCalls++;
}
// №934: цвет верха ленты по углу места (та же формула, что в horizonBuildSlice): выше линии горизонта глаза (дома, холмы) — тёмный силуэт, ниже (дальняя земля) — цвет дымки
static unsigned hzColTop(unsigned col, unsigned colB, float tt) {
  if (!hzGrad() || HORIZON_CONTRAST_OLD_V) return col;
  float f = tt * (1.f / 0.05f); f = f < 0.f ? 0.f : f > 1.f ? 1.f : f; unsigned c0 = colB & 255u, c1 = (colB >> 8) & 255u, c2 = (colB >> 16) & 255u, d0 = col & 255u, d1 = (col >> 8) & 255u, d2 = (col >> 16) & 255u, q = (unsigned)(f * 256.f);
  return 0xff000000u | (((c2 * (256u - q) + d2 * q) >> 8) << 16) | (((c1 * (256u - q) + d1 * q) >> 8) << 8) | ((c0 * (256u - q) + d0 * q) >> 8);
}
// контур клетки (вершины az/tan/D) на общей сетке азимутов 2°, считается на лету при сборке ленты: без кэша клеток (кэш на 4 клетки стоил 12 КБ образа и 100 мкс на смену клетки; куча уровня не хуже TEST59).
// HzCv — просмотр контура одной клетки: k растёт вместе с j (столбцы идут по возрастанию азимута), значения D и tan — линейно между вершинами
typedef struct { const HzCell *c; const unsigned char *p, *dd, *hq; unsigned nv, k; } HzCv;   // hq — тетрады цвета фасада вершин (№997), 0 — нет
static void hzCvInit(HzCv *v, int ci, int set) {
  v->c = &hzC[ci]; v->nv = set ? v->c->p2 : v->c->nv; v->p = hzData + (set ? v->c->p1 : v->c->off); v->k = 0;
  v->dd = hzDst + *(const unsigned *)(hzDst + 16 + 4 * ci) + (set ? (v->c->nv + 1) / 2 : 0);   // тетрады: младшая — чётная вершина
  v->hq = hzHcl ? hzHcl + *(const unsigned *)(hzHcl + 80 + 4 * ci) + (set ? (v->c->nv + 1) / 2 : 0) : 0;
}
static void hzCvAt(HzCv *v, int j, float *Dp, float *tp, unsigned *cp) {
  const unsigned char *p = v->p, *dd = v->dd; unsigned nv = v->nv, k = v->k; float aj = (float)j * (65536.f / HZ_RN);
  while (k < nv && (float)*(const unsigned short *)(p + 4 * k) <= aj) k++;   // k — число вершин с азимутом ≤ aj
  v->k = k; unsigned k0 = k ? k - 1 : nv - 1, k1 = k < nv ? k : 0; float a0 = (float)*(const unsigned short *)(p + 4 * k0), a1 = (float)*(const unsigned short *)(p + 4 * k1);
  if (!k) a0 -= 65536.f; if (k >= nv) a1 += 65536.f;   // до первой вершины и после последней — через ноль круга
  float f = a1 > a0 ? (aj - a0) / (a1 - a0) : 0.f, t0 = (float)*(const short *)(p + 4 * k0 + 2), t1 = (float)*(const short *)(p + 4 * k1 + 2);
  float D0 = hzDq[(dd[k0 >> 1] >> ((k0 & 1) << 2)) & 15], D1 = hzDq[(dd[k1 >> 1] >> ((k1 & 1) << 2)) & 15];
  *tp = (t0 + (t1 - t0) * f) * (1.f / 16384.f); *Dp = D0 + (D1 - D0) * f;
  if (cp && v->hq) {   // №997: цвет фасада — палитра уровня, линейно между вершинами; 0x00BBGGRR
    const unsigned char *q0 = hzHcl + 16 + 4 * ((v->hq[k0 >> 1] >> ((k0 & 1) << 2)) & 15), *q1 = hzHcl + 16 + 4 * ((v->hq[k1 >> 1] >> ((k1 & 1) << 2)) & 15); unsigned fq = (unsigned)(f * 256.f);
    *cp = (((q0[2] * (256u - fq) + q1[2] * fq) >> 8) << 16) | (((q0[1] * (256u - fq) + q1[1] * fq) >> 8) << 8) | ((q0[0] * (256u - fq) + q1[0] * fq) >> 8); }
}
static void hzCvMark(const HzCv *v, unsigned char *km) { for (unsigned q = 0; q < v->nv; q++) { int j = (int)((float)*(const unsigned short *)(v->p + 4 * q) * (HZ_RN / 65536.f) + 0.5f); km[j > HZ_RN ? HZ_RN : j] = 1; } }   // столбец у каждой вершины контура
// контур клетки на общей сетке (кэш на HZ_RSN клеток, заполнение один раз на клетку и набор контура ≈ 100 мкс)
static HzRs *hzResample(int ci, int set) {
  hzRsTick++; HzRs *r = &hzRs[0];
  for (int q = 0; q < HZ_RSN; q++) if (hzRs[q].cell == ci && hzRs[q].set == set) { hzRs[q].stamp = hzRsTick; return &hzRs[q]; }
  for (int q = 1; q < HZ_RSN; q++) if (hzRs[q].cell < 0 || (r->cell >= 0 && hzRs[q].stamp < r->stamp)) r = &hzRs[q];
  HzCv v; hzCvInit(&v, ci, set); r->cell = ci; r->set = set; r->stamp = hzRsTick; r->cx = v.c->cx; r->cz = v.c->cz; r->ey = v.c->ey;
  for (int j = 0; j <= HZ_RN; j++) { r->hc[j] = 0; hzCvAt(&v, j, &r->D[j], &r->t[j], &r->hc[j]); if (r->hc[j]) hzFacadeGain(r->hc[j], r->D[j], r->gn[j]); else r->gn[j][0] = r->gn[j][1] = r->gn[j][2] = 0; r->km[j] = HZ_WCOLS934_OLD || j == 0 || j == HZ_RN || j % HZ_WFILL == 0; }
  if (!HZ_WCOLS934_OLD) hzCvMark(&v, r->km);
  return r;
}
// №997: цвет верха ленты и карточек ориентиров: прежний тон силуэта colT (fogCol, притемнённый — он уже несёт свет уровня, ночью тёмный), умноженный на ОТНОСИТЕЛЬНЫЙ цвет фасада (цвет фасада / среднее по палитре уровня,
// 0,5…1,8 по каналам): оттенок дома виден, яркость остаётся яркостью дымки (сырые цвета фасада светлее ночной дымки и «выпрыгивали» бы). Доля влияния фасада 90 % на 150 м → 50 % на 900 м и дальше. 0x00BBGGRR → 0xffBBGGRR
static void hzFacadeGain(unsigned f, float D, unsigned short *g3) {   // множители ×256 по каналам для цвета фасада f на расстоянии D (один раз на клетку и столбец при пересчёте контура, не на кадр ленты)
  float u = (D - 150.f) * (1.f / 750.f); u = u < 0.f ? 0.f : u > 1.f ? 1.f : u; int w = (int)(HZ_HC_W0 + (float)((int)HZ_HC_W1 - (int)HZ_HC_W0) * u + .5f);   // влияние фасада, %
  for (int sh = 0, c = 0; sh <= 16; sh += 8, c++) { int rel = (int)((((f >> sh) & 255u) * hzHcInv[c]) >> 12); rel = rel < 128 ? 128 : rel > 460 ? 460 : rel; g3[c] = (unsigned short)(256 + (rel - 256) * w / 100); }
}
static unsigned hzMulGain(unsigned colT, const unsigned short *g3) {
  unsigned o = 0xff000000u; for (int sh = 0, c = 0; sh <= 16; sh += 8, c++) { unsigned v = (((colT >> sh) & 255u) * g3[c]) >> 8; o |= (v > 255u ? 255u : v) << sh; } return o;
}
static unsigned hzFacadeCol(unsigned colT, unsigned f1, unsigned f2, float w2, float D) {   // одиночный цвет (карточки ориентиров, до 16 на кадр)
  unsigned short g[3], h[3]; hzFacadeGain(f1, D, g); if (f2) { hzFacadeGain(f2, D, h); unsigned q2 = (unsigned)(w2 * 256.f); for (int c = 0; c < 3; c++) g[c] = (unsigned short)((g[c] * (256u - q2) + h[c] * q2) >> 8); }
  return hzMulGain(colT, g);
}
// мировая лента: две ближайшие клетки (i1 — ближняя, i2 — вторая, или −1), вес второй w2 (0…0,5), глаз (ex, ey, ez); до 4 × (HZ_RN + 1) вершин в hzWV, полосы A и B как у прежней ленты
static void hzWorldBuild(int i1, int i2, float w2, float ex, float ey, float ez, unsigned col, float R) {
  if (!hzSinInit) { for (int j = 0; j <= HZ_RN; j++) { float a = (float)j * (6.2831853f / HZ_RN); hzSinT[j] = sinf(a); hzCosT[j] = cosf(a); } hzSinInit = 1; }
  int set1 = hzSetWant && hzC[i1].p2, set2 = i2 >= 0 && hzSetWant && hzC[i2].p2; HzRs *r1 = hzResample(i1, set1), *r2 = i2 >= 0 && w2 > 0.f ? hzResample(i2, set2) : 0;
  if (r2 == r1) r2 = 0; if (r1->cell != i1) r1 = hzResample(i1, set1);   // вытеснение при переборе кэша
  unsigned NC = 0; for (int j = 0; j <= HZ_RN; j++) NC += r1->km[j] | (r2 ? r2->km[j] : 0);   // столбцов в ленте: объединение отмеченных у двух клеток
  unsigned colB = hzGrad() ? hzGroundCol(&hzC[i1], hzFogCol()) : col; float fade = R * HZ_FADE, bot = -R * 0.5f; int gr = hzGrad(); hzWnc = NC; unsigned o = 0;
  for (int j = 0; j <= HZ_RN; j++) {
    if (!(r1->km[j] | (r2 ? r2->km[j] : 0))) continue;
    float D = r1->D[j], tt = r1->t[j], px = r1->cx + D * hzSinT[j], py = r1->ey + tt * D, pz = r1->cz - D * hzCosT[j];
    if (r2) { float D2 = r2->D[j], t2 = r2->t[j]; px += (r2->cx + D2 * hzSinT[j] - px) * w2; py += (r2->ey + t2 * D2 - py) * w2; pz += (r2->cz - D2 * hzCosT[j] - pz) * w2; tt += (t2 - tt) * w2; }
    float dx = px - ex, dz = pz - ez, dh2 = dx * dx + dz * dz, s = R / sqrtf(dh2 > 1.f ? dh2 : 1.f), xr = dx * s, zr = dz * s, yt = (py - ey) * s, ym = yt - fade;
    if (ym < bot || !gr) ym = bot;   // без градиента полоса A тянется от верха до низа
    unsigned colF = col;   // №997: тон силуэта над горизонтом — цвет фасада дальнего дома, смешанный с прежним тоном; ниже горизонта (дальняя земля) остаётся прежний цвет земли (hzColTop смешивает по углу места)
    if (hzHcl && !hzCol997Old && r1->gn[j][0]) { unsigned short g[3] = { r1->gn[j][0], r1->gn[j][1], r1->gn[j][2] };
      if (r2 && r2->gn[j][0]) { unsigned q2 = (unsigned)(w2 * 256.f); for (int c = 0; c < 3; c++) g[c] = (unsigned short)((g[c] * (256u - q2) + r2->gn[j][c] * q2) >> 8); }
      colF = hzMulGain(col, g); }
    unsigned colT = hzColTop(colF, colB, tt);
    hzWV[2 * o] = (HzVtx){colT, xr, yt, zr}; hzWV[2 * o + 1] = (HzVtx){colB, xr, ym, zr};
    hzWV[2 * NC + 2 * o] = (HzVtx){colB, xr, ym, zr}; hzWV[2 * NC + 2 * o + 1] = (HzVtx){colB, xr, bot, zr}; o++;
  }
  if (smkOn && o) { float sm[3] = {0, 0, 0}, sq[3] = {0, 0, 0}; for (unsigned q = 0; q < o; q++) for (int c = 0; c < 3; c++) { float v = (float)((hzWV[2 * q].c >> (8 * c)) & 255u); sm[c] += v; sq[c] += v * v; }
    float tot = 0.f; for (int c = 0; c < 3; c++) { float m = sm[c] / (float)o, var = sq[c] / (float)o - m * m; tot += sqrtf(var > 0.f ? var : 0.f); } hzTopSpread = (unsigned)(tot * 10.f); }
}
// №934 диагностика (smoke, «flag hztrace 2»): кромка нарисованной ленты — угол места (градусы ×100, целое) на 72 азимутах через 5° от глаза; по строкам HZE гейт tests/horizon/hz934.sh считает скачок между соседними шагами глаза. k, dy — масштаб и сдвиг матрицы прежней ленты (мировая: 1, 0)
static void hzTraceEdge(const HzVtx *V, unsigned nc, int wr, float k, float dy, float ex, float ey, float ez) {
  if (nc < 2 || nc > 4 * (HZ_RN + 1)) return;
  char s[760]; int n = snprintf(s, sizeof s, "HZE %.2f %.2f %.2f %d %d", ex, ey, ez, (int)gameLevel, wr);
  for (int q = 0; q < 72 && n < (int)sizeof s - 12; q++) {
    float ta = (float)q * (6.2831853f / 72.f), best = -1e9f, bestH = 1e9f; int lo = -1, hi = -1;
    for (unsigned j = 0; j < nc; j++) { float x = V[2 * j].x * k, z = V[2 * j].z * k, a = atan2f(x, -z); if (a < 0.f) a += 6.2831853f;
      float d = ta - a; if (d < -3.14159f) d += 6.2831853f; if (d > 3.14159f) d -= 6.2831853f;
      if (d >= 0.f && d < bestH) { bestH = d; lo = (int)j; } if (d < 0.f && d > best) { best = d; hi = (int)j; } }
    if (lo < 0) lo = hi; if (hi < 0) hi = lo;
    float e[2]; int id[2] = { lo, hi };
    for (int w = 0; w < 2; w++) { const HzVtx *p = &V[2 * id[w]]; float x = p->x * k, z = p->z * k, y = p->y * k + dy; e[w] = atan2f(y, sqrtf(x * x + z * z)) * 57.29578f; }
    float f = (bestH < 1e8f && best > -1e8f && bestH - best > 1e-6f) ? bestH / (bestH - best) : 0.f;
    n += snprintf(s + n, sizeof s - n, " %d", (int)((e[0] + (e[1] - e[0]) * f) * 100.f));
  }
  s[n < (int)sizeof s - 2 ? n : (int)sizeof s - 2] = 0; strcat(s, "\n"); smkWrite(s);
}
static void horizonDraw(float ex, float ey, float ez) {
  unsigned t0_ = sceKernelGetSystemTimeLow();
  hzVerts = hzCalls = 0;   // счётчики кадра; hzNv — число вершин собранной ленты, живёт между кадрами
  if (HORIZON_OLD || !horizonOn || inRoom() || clearChk || !hzOff1042Old) return;   // №1042: ленты силуэтов горизонта (№887, цвета фасадов №997) нет ни на одном уровне; данные ленты не читаются (куча)
  if (hzLevel != gameLevel) horizonLoad(gameLevel);
  if (!hzN || geOver(GE_SOFT)) return;
  int best = 0, second = -1; float bd = 1e30f, bd2 = 1e30f;   // две ближайшие клетки: вторая нужна мировой ленте (подмес у границы)
  for (unsigned i = 0; i < hzN; i++) { float dx = hzC[i].cx - ex, dz = hzC[i].cz - ez, d = dx * dx + dz * dz; if (d < bd) { bd2 = bd; second = best; bd = d; best = (int)i; } else if (d < bd2) { bd2 = d; second = (int)i; } }
  if (bd == 1e30f) second = -1; if (bd2 == 1e30f) second = -1;
  unsigned col = horizonColor();
  if (!HZ913_OLD) { if (drawDist >= 149.5f) hzSetWant = 1; else if (drawDist < 146.f) hzSetWant = 0; }   // №913: контур от 150 м, пока город до 150+ м (гистерезис: без дрожи на пороге)
  int world = hzWorldOn && hzDst;   // №934: мировая лента (есть хвост DST2 и не выключена); прежняя собирается и рисуется только без неё
  if (!world) { horizonUpdate(best, col); if (hzCur < 0 || !hzVbBuf) return; }   // прежняя лента: буферы по требованию (HZ_VB934_OLD=0); нет буфера или первой сборки — не рисуем
  if (hzTrace && smkOn) { char s[220]; float gy_ = 0.f, clr_ = ground(ex, ez, ey + 1.f, 0.f, &gy_) ? ey - gy_ : -999.f; snprintf(s, sizeof s, "HZT %u %.2f %.2f %.2f %.3f %.3f %d %.0f %.0f %u %d %.2f %.2f %.0f %d\n", smkFrameAbs, ex, ey, ez, cfX, cfZ, hzCur, drawDist, hzCullD, hzXn, far909St.hi, clr_, hzC[best].ey, hz913St.cap, hzSetCur); smkWrite(s); }
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_FOG);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE);   // глубину пишем: небо рисуется последним (O_SKYLAST) только там, где глубины нет; лента на дальнем крае, город перекрывает её
  unsigned nvc, nv4; HzVtx *v;
  if (world) {   // вершины пересчитываются, когда глаз сдвинулся больше HZ_WSTEP, сменились клетки/вес/цвет/дальность; иначе копия прежних
    float R = drawDist + 4.f, w2 = 0.f; if (second >= 0) { float da = sqrtf(bd), db = sqrtf(bd2), t = (da + HZ_WBAND - db) * (1.f / HZ_WBAND); w2 = 0.5f * (t >= 1.f ? 1.f : t > 0.f ? t : 0.f); }
    unsigned fog = hzFogCol(), sig = (unsigned)best * 2654435761u ^ (unsigned)(second + 1) * 40503u ^ col ^ (fog * 31u) ^ ((unsigned)hzSetWant << 20) ^ (unsigned)hzGrad() * 77u ^ (hzHcl && !hzCol997Old ? 0x9b3u : 0u);
    float mx_ = ex - hzWex, my_ = ey - hzWey, mz_ = ez - hzWez;
    if (sig != hzWsig || R != hzWR || mx_ * mx_ + my_ * my_ + mz_ * mz_ > HZ_WSTEP * HZ_WSTEP) {
      unsigned tw0 = sceKernelGetSystemTimeLow(); hzWorldBuild(best, second, w2, ex, ey, ez, col, R); hzWex = ex; hzWey = ey; hzWez = ez; hzWR = R; hzWw2 = w2; hzWsig = sig;
      hzWUsAcc += sceKernelGetSystemTimeLow() - tw0; hzWUsN++; hzWBuilds++; }
    ScePspFMatrix4 id; memset(&id, 0, sizeof id); id.x.x = id.y.y = id.z.z = id.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &id);
    nvc = hzWnc; nv4 = 4 * nvc; v = (HzVtx *)sceGuGetMemory(nv4 * sizeof(HzVtx)); memcpy(v, hzWV, nv4 * sizeof(HzVtx));
  } else {
    ScePspFMatrix4 m; memset(&m, 0, sizeof m); float k = (drawDist + 4.f) / hzR;   // дальняя плоскость вида = drawDist + 10: лента внутри неё, равномерный масштаб сохраняет углы контура
    m.x.x = m.z.z = m.y.y = k; m.w.w = 1.f; m.w.y = hzC[hzCur].ey - ey;   // мировая высота клетки относительно глаза (рисуется ещё прежняя клетка, пока новая собирается)
    sceGuSetMatrix(GU_MODEL, &m); nvc = hzNv; nv4 = 4 * hzNv; v = (HzVtx *)sceGuGetMemory(nv4 * sizeof(HzVtx)); memcpy(v, hzVb(hzAct), nv4 * sizeof(HzVtx));   // вершины в списке команд, как у подложки клеток (cellUnderlayDraw)
  }
  if (hzTrace == 2 && smkOn) { if (world) hzTraceEdge(hzWV, nvc, 1, 1.f, 0.f, ex, ey, ez); else hzTraceEdge(hzVb(hzAct), hzNv, 0, (drawDist + 4.f) / hzR, hzC[hzCur].ey - ey, ex, ey, ez); }
  sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)(2 * nvc), 0, v);
  if (hzGrad()) sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)(2 * nvc), 0, v + 2 * nvc);   // без градиента вторая полоса целиком под землёй, не рисуем
  hzVerts = hzGrad() ? nv4 : 2 * nvc; hzCalls = hzGrad() ? 2 : 1;
  if (hzNL && hzLmOn) { ScePspFMatrix4 id; memset(&id, 0, sizeof id); id.x.x = id.y.y = id.z.z = id.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &id); if (HZ_LM928_OLD) horizonLmDrawOld(ex, ey, ez, col); else horizonLmDraw(ex, ey, ez, col); }
  resetState();
  unsigned us_ = sceKernelGetSystemTimeLow() - t0_; hzUsAcc += us_; if (us_ > hzUsMax) hzUsMax = us_;
  if (++hzUsN >= 30) { if (smkOn) { char s[224]; snprintf(s, sizeof s, "HORIZON cost level=%d avg_us=%u max_us=%u frames=%u verts=%u calls=%u drawDist=%.0f world=%d wus=%u builds=%u hcl=%d hcold=%d spread=%u\n", gameLevel, hzUsAcc / hzUsN, hzUsMax, hzUsN, hzVerts, hzCalls, drawDist, hzWorldOn && hzDst != 0, hzWUsN ? hzWUsAcc / hzWUsN : 0u, hzWBuilds, hzHcl != 0, hzCol997Old, hzTopSpread); hzWUsAcc = hzWUsN = 0; hzWBuilds = 0; smkWrite(s); } hzUsAcc = hzUsN = hzUsMax = 0; }
}
