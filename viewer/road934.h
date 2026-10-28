// №934 (слово Роба 24.10 16:02: «за 200–250 м пропадают дороги и мосты»; кольцо-панорама везде отклонено — параллакс 20–50 px, HLOD-коробки не брать никогда).
// Дальние версии кусков дорог и мостов до RD_FAR (520 м): файл road934.shr уровня (tools/road934_export.py, RD35) — только полотно и силуэт пролётов, упрощено с границей ошибки 0,5 м (tools/road934_simplify.py: вершины только существующие, край держит ограничитель; 23 тыс. → 3–4 тыс. треугольников на уровень), заливка цветом средней текстуры × свет, без текстур. Читается один раз на уровень (≤ 64 КБ, живёт в куче до смены уровня); вершины общие у всех блоков, s16 от центра рамки уровня (одна матрица модели на кадр); вершина 8 Б (цвет 5650 + s16 x3).
// Рисуется до города (после ленты горизонта, до силуэтов и подложки): настоящий город перекрывает слой по порядку рисования, поэтому слой начинается на RD_HIDE м ближе дальности города
// (drawDist − RD_HIDE) и «щелчка» на границе нет; дальше — настоящая геометрия, а не картинка (нет параллакса). Тот же приём, что у hlod.h: мир от глаза масштабируется на k = (drawDist + 4) / RD_FAR
// (углы те же, дальняя плоскость кадра drawDist + 10 не режет), глубина не пишется и не проверяется; блоки от дальнего к ближнему, внутри блока по возрастанию высоты (мост после полотна под ним).
// Дымка: с RD_FOG0 доли пути от дальности города до RD_FAR линейно к цвету горизонта (на RD_FAR — 100 %), дальний край растворяется, а не обрывается.
// Цена: один набор матриц на кадр и до RD_MAXB вызовов GE (по одному на блок 160 м, тот же массив вершин); в кадре ≈ 1–2 тыс. треугольников (замер в журнале: строка ROAD934 раз в 60 кадров).
// FARROAD934_OLD=1 — функция не рисует вовсе (прежнее). Выключатели: файл farroad934old рядом с EBOOT, «flag farroad934 0/1» в сценарии; «flag farroad934dbg 1» — сплошной пурпур без дымки (гейт tests/road934).
#pragma once
#ifndef FARROAD934_OLD
#define FARROAD934_OLD 0
#endif
#ifndef RD_NEARCLIP_OLD
#define RD_NEARCLIP_OLD 0   // 1 — без среза по ближней плоскости слоя (блоки целиком, как в первой версии)
#endif
#define RD_FAR 520.f      // дальше слой не рисуется (на ней дымка 100 %)
#define RD_HIDE 30.f      // слой начинается на столько ближе дальности города: перекрытие под настоящим городом
#define RD_FOG0 .55f      // доля пути от дальности города до RD_FAR, с которой слой уходит в дымку
#define RD_MAXB 56        // блоков за кадр
#define RD_CMG 5.f        // запас ящиков при отборе, м: слой ≥ 100 м от глаза, 5 м — меньше 3° у края кадра
#define RD_REUSE 8        // не дольше стольки кадров подряд отбор берётся прежний
#define RD_REUSE_D2 4.f   // ... пока глаз сдвинулся не больше чем на 2 м (квадрат)
#define RD_REUSE_COS .9995f   // ... и взгляд повернулся не больше чем на 1,8°
#ifndef RD_CACHE_OLD
#define RD_CACHE_OLD 0    // 1 — отбор блоков каждый кадр заново (прежнее)
#endif
typedef struct { float bb[6]; unsigned ioff, ni; } RdBlk;   // рамка блока (мир), смещение индексов от начала файла, число индексов
static int road934On = 1, road934Dbg, road934Reuse = 1;
static struct { float e[3], f[3], fov, dd; int lvl, ok; unsigned ns, age, sel[RD_MAXB]; } rdK;   // последний отбор блоков
static unsigned char *rdData; static unsigned rdN, rdSize, rdVOff; static int rdLevel = -99; static const RdBlk *rdB; static float rdO[3], rdE; static float *rdC;   // rdC: на блок cx, cz, rad (полудиагональ рамки в плане), (RD_FAR + rad)² — быстрые отсевы без рамки
static unsigned rdBlocks, rdTris, rdCalls, rdUsAcc, rdUsMax, rdFr, rdPoseN, rdHit;   // последний кадр: блоков, треугольников, вызовов; цена по 60 кадров
static void road934Free(void) { free(rdData); free(rdC); rdK.ok = 0; rdData = 0; rdC = 0; rdN = 0; rdB = 0; rdLevel = -99; }
static void road934Load(int level) {
  road934Free(); rdLevel = level;   // одна попытка на уровень: нет файла — молча без дальних дорог
  char fn[256]; levelFile(fn, "road934.shr");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int sz = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  if (sz < 64 || sz > (1 << 20)) { sceIoClose(f); return; }
  if (heapFreeBytes() < MEM_RESERVE + (unsigned)sz + (1u << 20)) { sceIoClose(f); if (smkOn) { char s[96]; snprintf(s, sizeof s, "ROAD934 skipped level=%d heap=%u need=%u\n", level, heapFreeBytes(), MEM_RESERVE + (unsigned)sz + (1u << 20)); smkWrite(s); } return; }   // без запаса кучи не берём: слой — украшение, клетки важнее
  unsigned char *m = (unsigned char *)memalign(64, (size_t)sz);
  if (!m || sceIoRead(f, m, (SceSize)sz) != sz || memcmp(m, "RD35", 4)) { sceIoClose(f); free(m); return; }
  sceIoClose(f);
  unsigned n = *(unsigned *)(m + 4), nv = *(unsigned *)(m + 12); const float *hd = (const float *)(m + 16);
  unsigned voff = (32u + 32u * n + 15u) & ~15u;
  if (n == 0 || n > 1024 || nv == 0 || nv > 65535 || voff + 8ull * nv > (unsigned)sz || !(hd[3] >= 1.f && hd[3] <= 8192.f)) { free(m); return; }   // таблица и вершины целиком внутри файла: иначе видеочип прочитал бы чужую память
  const RdBlk *B = (const RdBlk *)(m + 32);
  for (unsigned i = 0; i < n; i++) { const RdBlk *b = &B[i];
    if (!b->ni || b->ni % 3 || (b->ioff & 1) || b->ioff < voff + 8u * nv || (unsigned long long)b->ioff + 2ull * b->ni > (unsigned)sz) { free(m); return; }
    const unsigned short *ix = (const unsigned short *)(m + b->ioff);   // индекс за пределами вершин видеочип прочитал бы мимо массива (эмулятор это прощает, PSP нет): проверка один раз на уровень
    for (unsigned q = 0; q < b->ni; q++) if (ix[q] >= nv) { free(m); return; } }
  sceKernelDcacheWritebackRange(m, (unsigned)sz);   // видеочип читает вершины прямо из памяти
  rdC = (float *)malloc(16u * n); if (!rdC) { free(m); return; }
  for (unsigned i = 0; i < n; i++) { const float *bb = B[i].bb; float hx = (bb[3] - bb[0]) * .5f, hz = (bb[5] - bb[2]) * .5f, r = sqrtf(hx * hx + hz * hz), R = RD_FAR + RD_CMG + r;
    rdC[4 * i] = (bb[0] + bb[3]) * .5f; rdC[4 * i + 1] = (bb[2] + bb[5]) * .5f; rdC[4 * i + 2] = r; rdC[4 * i + 3] = R * R; }
  rdData = m; rdSize = (unsigned)sz; rdN = n; rdB = B; rdVOff = voff; rdO[0] = hd[0]; rdO[1] = hd[1]; rdO[2] = hd[2]; rdE = hd[3];
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "ROAD934 loaded level=%d blocks=%u bytes=%u\n", level, n, (unsigned)sz); smkWrite(s); }
}
static void road934Draw(float ex, float ey, float ez) {
  rdBlocks = rdTris = rdCalls = 0;
  if (road934Dbg && smkOn && !(++rdPoseN & 7)) { char s[200]; snprintf(s, sizeof s, "ROAD934 pose t=%.2f eye=%.2f,%.2f,%.2f fwd=%.4f,%.4f,%.4f fovy=%.2f dd=%.1f\n", smkT, ex, ey, ez, cfX, cfY, cfZ, curFovY, drawDist); smkWrite(s); }   // служебное: поза кадра для гейта (tests/road934)
  if (FARROAD934_OLD || !road934On || inRoom() || clearChk) return;
  unsigned t0 = sceKernelGetSystemTimeLow();
  if (rdLevel != gameLevel) road934Load(gameLevel);
  if (!rdN || geOver(GE_SOFT)) return;
  float near_ = drawDist - RD_HIDE; if (near_ < 60.f) near_ = 60.f;
  float k = (drawDist + 4.f) / RD_FAR;
  unsigned ns;
  { float ddx = ex - rdK.e[0], ddy = ey - rdK.e[1], ddz = ez - rdK.e[2];   // отбор блоков переиспользуется, пока глаз почти на месте и взгляд почти тот же (ящики взяты с запасом RD_CMG); глаз, поле зрения и дальность — те же
    int same = !RD_CACHE_OLD && road934Reuse && rdK.ok && rdK.lvl == gameLevel && rdK.fov == curFovY && rdK.dd == drawDist && rdK.age < RD_REUSE
      && ddx * ddx + ddy * ddy + ddz * ddz <= RD_REUSE_D2 && cfX * rdK.f[0] + cfY * rdK.f[1] + cfZ * rdK.f[2] >= RD_REUSE_COS;
    if (same) { rdK.age++; ns = rdK.ns; rdHit++; }
    else {
      float sd[RD_MAXB]; ns = 0; unsigned *sel = rdK.sel; const float mg = cullMg + RD_CMG;   // запас ящиков при отборе
      for (unsigned i = 0; i < rdN; i++) {
        const float *c = rdC + 4 * i; float dx = c[0] - ex, dz = c[1] - ez, d = dx * dx + dz * dz;   // d — квадрат расстояния до центра рамки: ключ порядка
        if (d > c[3]) continue;                   // рамка целиком дальше RD_FAR (круг радиуса rad вокруг центра)
        float t = near_ - RD_CMG - c[2]; if (t > 0.f && d < t * t) continue;   // рамка целиком ближе начала слоя: весь блок под настоящим городом
        const float *bb = rdB[i].bb; int hid = 0;   // боковые плоскости кадра (как visBox, i = 0, 1: лево/право; верх/низ слой с дальнего расстояния почти не режут — лишнее видеочип срежет сам)
        for (int pi = 0; pi < 2 && !hid; pi++) {
          float px = (pl[pi][0] > 0 ? bb[3] + mg : bb[0] - mg) - ex, py = (pl[pi][1] > 0 ? bb[4] + mg : bb[1] - mg) - ey, pz = (pl[pi][2] > 0 ? bb[5] + mg : bb[2] - mg) - ez;
          hid = px * pl[pi][0] + py * pl[pi][1] + pz * pl[pi][2] < 0; }
        if (hid) continue;
        if (ns == RD_MAXB) { if (d >= sd[0]) continue; memmove(sd, sd + 1, (ns - 1) * sizeof sd[0]); memmove(sel, sel + 1, (ns - 1) * sizeof sel[0]); ns--; }   // переполнение: самые дальние уступают ближним
        unsigned j = ns++; while (j > 0 && sd[j - 1] < d) { sd[j] = sd[j - 1]; sel[j] = sel[j - 1]; j--; }
        sd[j] = d; sel[j] = i;   // дальние первыми
      }
      rdK.ok = 1; rdK.lvl = gameLevel; rdK.fov = curFovY; rdK.dd = drawDist; rdK.age = 0; rdK.ns = ns; rdK.e[0] = ex; rdK.e[1] = ey; rdK.e[2] = ez; rdK.f[0] = cfX; rdK.f[1] = cfY; rdK.f[2] = cfZ;
    }
  }
  const unsigned *sel = rdK.sel;
  if (!ns) return;
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_ALWAYS); sceGuDepthMask(GU_TRUE);   // глубина не пишется и не проверяется: порядок блоков, город после слоя
  if (road934Dbg) { sceGuEnable(GU_FOG); sceGuFog(0.f, 1e-3f, 0xff00ffu); }   // служебное: сплошной пурпур (как hlod 4), гейт tests/road934
  else if (fogCity1042On) { sceGuDisable(GU_FOG); }   // №1042: слой дальних дорог без дымки (до RD_FAR полным цветом)
  else { float f0 = drawDist + (RD_FAR - drawDist) * RD_FOG0; sceGuEnable(GU_FOG); sceGuFog(f0 * k, (RD_FAR + 2.f) * k, cityFogColor() & 0xffffffu); }
  // ближняя плоскость слоя = near_ (в масштабе k): видеочип сам срезает дороги ближе дальности города, блоки (160 м) заходят под город на десятки метров; матрица считается при смене поля зрения/дальности, не каждый кадр
  static ScePspFMatrix4 pm1; static float pmFov = -1.f, pmNear = -1.f, pmDd = -1.f; ScePspFMatrix4 pm0;
  if (!RD_NEARCLIP_OLD) {
    sceGumMatrixMode(GU_PROJECTION); sceGumStoreMatrix(&pm0);
    if (pmFov != curFovY || pmNear != near_ || pmDd != drawDist) { sceGumPushMatrix(); sceGumLoadIdentity(); sceGumPerspective(curFovY, ASPECT, near_ * k, drawDist + 10.f); sceGumStoreMatrix(&pm1); sceGumPopMatrix(); pmFov = curFovY; pmNear = near_; pmDd = drawDist; }
    sceGumMatrixMode(GU_MODEL); sceGuSetMatrix(GU_PROJECTION, &pm1);
  }
  ScePspFMatrix4 m; memset(&m, 0, sizeof m); m.x.x = m.y.y = m.z.z = rdE * k; m.w.w = 1.f;   // одна матрица на все блоки: вершины общие, s16 от центра рамки уровня
  m.w.x = (rdO[0] - ex) * k; m.w.y = (rdO[1] - ey) * k; m.w.z = (rdO[2] - ez) * k;
  sceGuSetMatrix(GU_MODEL, &m);
  for (unsigned q = 0; q < ns; q++) {
    const RdBlk *b = &rdB[sel[q]];
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_5650 | GU_VERTEX_16BIT | GU_INDEX_16BIT | GU_TRANSFORM_3D, (int)b->ni, rdData + b->ioff, rdData + rdVOff);
    rdTris += b->ni / 3; rdCalls++;
  }
  rdBlocks = ns;
  if (!RD_NEARCLIP_OLD) sceGuSetMatrix(GU_PROJECTION, &pm0);
  sceGuDepthMask(GU_FALSE); sceGuDepthFunc(GU_GEQUAL);
  if (!farProbe && !groundCoverFog(&groundCover) && !fogCityOld) { sceGuDisable(GU_FOG); fogK = -1.f; }   // как перед городом в main_loop.h
  else { sceGuEnable(GU_FOG); fogBase(); }   // дымка города включена: параметры слоя не должны остаться ни в видеочипе, ни в кэше fogK — после слоя рисуется подложка клеток (cellUnderlayDraw), она не выставляет дымку сама и без этого брала бы туман слоя (при flag farroad934dbg 1 — сплошной пурпур)
  resetState();
  unsigned us = sceKernelGetSystemTimeLow() - t0; rdUsAcc += us; if (us > rdUsMax) rdUsMax = us;
  if (++rdFr >= 60) { if (smkOn) { char s[160]; snprintf(s, sizeof s, "ROAD934 cost level=%d avg_us=%u max_us=%u blocks=%u tris=%u calls=%u drawDist=%.0f hit=%u\n", gameLevel, rdUsAcc / rdFr, rdUsMax, rdBlocks, rdTris, rdCalls, drawDist, rdHit); smkWrite(s); }
    rdUsAcc = rdFr = rdUsMax = rdHit = 0; }
}
