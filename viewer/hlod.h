// HLOD: склейка дальних кварталов (п.3 №702, пилот). Файл hlod.shr уровня (tools/hlod_export.py, HLD1): блоки 160 м, ~100-150 треугольников на блок, цвет в вершинах.
// Рисуется в слое между городом (до drawDist) и лентой горизонта: тот же приём, что у ленты, — равномерный масштаб от глаза k=(drawDist+4)/HLOD_R (углы те же, порядок
// глубины внутри слоя сохранён), без текстуры. Блоки рисуются от дальних к ближним без проверки глубины, а в буфер глубины пишется узкий слой (sceGuDepthRange(8, 6)):
// он ближе неба (небо: диапазон 2..0, проходит тест только там, где кадр пуст) и дальше любого куска города, поэтому настоящий город перекрывает HLOD, а небо его не стирает.
// Выключено по умолчанию: «flag hlod N» в сценарии или файл hlod рядом с EBOOT. N: 1 — без тумана (диагностика), 2 — туман как у города (на drawDist всё в цвете дымки),
// 3 — туман как у города с учётом fogsoft (flag fogsoft N: конец тумана города на drawDist × N, то же у HLOD — непрерывный переход). HLOD_OLD=1 — функция не рисует вовсе (прежнее)
#pragma once
#ifndef HLOD_OLD
#define HLOD_OLD 0
#endif
#define HLOD_R 650.f      // дальний край слоя, м
#define HLOD_MAXB 128
#define HLOD_HIDE 10.f    // блок рисуется от drawDist − 10 м: перекрытие с городом без пустого кадра
typedef struct { float cx, cz, y0; unsigned nv, ni, voff, ioff; float rad; unsigned p0, p1, p2, p3; } HlBlock;   // 48 Б (как в файле)
static int hlodOn, hlodCull = 1, hlodAfter;   // hlodAfter (flag hlodafter 1, №704): слой рисуется ПОСЛЕ города с постоянной глубиной 7 и тестом GEQUAL — под городом пиксели отбрасываются (заливка только по пустому)   // hlodCull=0 — без отсечения по направлению (служебно: сцены с камерой-заменой eyeOv, направление игрока не совпадает с видом)
static unsigned char *hlData; static unsigned hlN, hlSize; static int hlLevel = -99; static const HlBlock *hlB;
static unsigned hlVerts, hlCalls, hlUsAcc, hlUsN, hlUsMax;
static void hlodFree(void) { free(hlData); hlData = 0; hlN = 0; hlB = 0; hlLevel = -99; }
static void hlodFail(const char *why, int a) { if (smkOn) { char s[96]; snprintf(s, sizeof s, "HLOD load fail: %s %d\n", why, a); smkWrite(s); } }
static void hlodLoad(int level) {
  hlodFree(); hlLevel = level;   // одна попытка на уровень: нет файла — молча без HLOD
  char fn[256]; levelFile(fn, "hlod.shr");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) { hlodFail("open", f); return; }
  int sz = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  if (sz < 32 || sz > (1 << 20)) { sceIoClose(f); hlodFail("size", sz); return; }
  unsigned char *m = (unsigned char *)(LVA_WIDE_OLD ? memalign(64, (size_t)sz) : lvaAllocWide((unsigned)sz));   // Build 96 шаг 6: в хвост блока уровня
  if (!m || sceIoRead(f, m, (SceSize)sz) != sz || memcmp(m, "HLD1", 4)) { sceIoClose(f); free(m); hlodFail("read/magic", sz); return; }
  sceIoClose(f);
  unsigned n = *(unsigned *)(m + 4);
  if (n == 0 || n > HLOD_MAXB || 32ull + 48ull * n > (unsigned)sz) { free(m); hlodFail("count", (int)n); return; }
  const HlBlock *b = (const HlBlock *)(m + 32);
  for (unsigned i = 0; i < n; i++)
    if (!b[i].nv || b[i].nv > 65535 || b[i].ni < 3 || b[i].ni % 3 || b[i].voff + 12ull * b[i].nv > (unsigned)sz || b[i].ioff + 2ull * b[i].ni > (unsigned)sz || (b[i].voff & 3) || (b[i].ioff & 1)) { free(m); return; }
  for (unsigned i = 0; i < n; i++) {   // индексы должны указывать внутрь блока (битый файл не должен давать чтение за пределами)
    const unsigned short *ix = (const unsigned short *)(m + b[i].ioff);
    for (unsigned k = 0; k < b[i].ni; k++) if (ix[k] >= b[i].nv) { free(m); return; }
  }
  sceKernelDcacheWritebackRange(m, (unsigned)sz);   // видеочип читает вершины прямо из памяти
  hlData = m; hlSize = (unsigned)sz; hlN = n; hlB = b;
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "HLOD loaded level=%d blocks=%u bytes=%u\n", level, n, (unsigned)sz); smkWrite(s); }
}
static void hlodDraw(float ex, float ey, float ez, int after) {
  if (after != hlodAfter) return;   // один вызов до города (прежнее), другой после (hlodAfter)
  unsigned t0_ = sceKernelGetSystemTimeLow();
  hlVerts = hlCalls = 0;
  { static int once_; if (hlodOn && !once_ && smkOn) { once_ = 1; char s_[96]; snprintf(s_, sizeof s_, "HLOD draw entered on=%d room=%d clr=%d lvl=%d/%d\n", hlodOn, inRoom(), clearChk, hlLevel, gameLevel); smkWrite(s_); } }
  _Static_assert(sizeof(HlBlock) == 48, "HlBlock 48");
  if (HLOD_OLD || !hlodOn || inRoom() || clearChk) return;
  if (hlLevel != gameLevel) hlodLoad(gameLevel);
  if (!hlN || geOver(GE_SOFT)) return;
  float k = (drawDist + 4.f) / HLOD_R, fx = cfX, fz = cfZ, fl = sqrtf(fx * fx + fz * fz); if (fl > 1e-4f) { fx /= fl; fz /= fl; }
  unsigned sel[HLOD_MAXB]; float sd[HLOD_MAXB]; unsigned ns = 0;
  for (unsigned i = 0; i < hlN; i++) {
    const HlBlock *b = &hlB[i]; float dx = b->cx - ex, dz = b->cz - ez, D = sqrtf(dx * dx + dz * dz);
    if (D - b->rad > HLOD_R || D + b->rad < drawDist - HLOD_HIDE) continue;
    if (hlodCull && D > b->rad) {   // вне обзора: угол до центра больше полуобзора + угловой радиус блока
      float c = (dx * fx + dz * fz) / D, s = b->rad / D, lim = 0.5f - 0.8f * s;   // cos(~60°) − запас на радиус блока; грубо, без плоскостей
      if (c < lim) continue;
    }
    unsigned j = ns++; while (j > 0 && sd[j - 1] < D) { sd[j] = sd[j - 1]; sel[j] = sel[j - 1]; j--; } sd[j] = D; sel[j] = i;   // дальние первыми
  }
  if (!ns) return;
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_CULL_FACE);
  if (hlodOn == 1) sceGuDisable(GU_FOG);
  else if (hlodOn == 4) { sceGuEnable(GU_FOG); sceGuFog(0.f, 1e-3f, 0xff00ffu); }   // диагностика: сплошной пурпур
  else { sceGuEnable(GU_FOG); unsigned fc = cityFogColor() & 0xffffffu; if (hlodOn == 2) sceGuFog(drawDist * FOG_NEAR * k, drawDist * k, fc); else sceGuFog(drawDist * FOG_NEAR * k, drawDist * fogSoft * k, fc); }
  if (after) { sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuDepthMask(GU_FALSE); sceGuDepthRange(7, 7); }   // №704: после города; глубина слоя постоянна (7): город (глубина больше) перекрывает, дальние блоки перекрываются ближними по порядку рисования
  else { sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_ALWAYS); sceGuDepthMask(GU_FALSE); sceGuDepthRange(8, 6); }
  for (unsigned q = 0; q < ns; q++) {
    const HlBlock *b = &hlB[sel[q]];
    ScePspFMatrix4 m; memset(&m, 0, sizeof m); m.x.x = m.y.y = m.z.z = 4096.f * k; m.w.w = 1.f;
    m.w.x = (b->cx - ex) * k; m.w.y = (b->y0 - ey) * k; m.w.z = (b->cz - ez) * k;
    sceGuSetMatrix(GU_MODEL, &m);
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_INDEX_16BIT | GU_TRANSFORM_3D, (int)b->ni, hlData + b->ioff, hlData + b->voff);
    hlVerts += b->ni; hlCalls++;
  }
  sceGuDepthRange(65535, 0); sceGuDepthFunc(GU_GEQUAL);
  sceGuEnable(GU_FOG); fogK = -1.f;   // туман города хранится в кэше fogK: свои параметры слоя HLOD не должны остаться (иначе город дальше рисуется в чужом тумане)
  resetState();
  unsigned us_ = sceKernelGetSystemTimeLow() - t0_; hlUsAcc += us_; if (us_ > hlUsMax) hlUsMax = us_;
  if (++hlUsN >= 30) { if (smkOn) { char s[128]; snprintf(s, sizeof s, "HLOD cost level=%d avg_us=%u max_us=%u frames=%u idx=%u calls=%u drawDist=%.0f\n", gameLevel, hlUsAcc / hlUsN, hlUsMax, hlUsN, hlVerts, hlCalls, drawDist); smkWrite(s); } hlUsAcc = hlUsN = hlUsMax = 0; }
}
