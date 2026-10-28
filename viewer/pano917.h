// №917: кольцо-панорама зоны 250 м — рисование и подкачка слотов (загрузка pano917io.h, чистая часть pano917core.h). Рисуется после ленты силуэта и до города: без проверки и записи глубины,
// город перекрывает его там, где он есть; лента от 150 м остаётся фоном за кольцом (там, где кромка панорамы прозрачна), небо «последним» не затирает кольцо, потому что лента писала глубину по тому же силуэту.
// Сегменты — плоские квады на радиусе R вокруг центра панорамы (мировая привязка даёт параллакс при движении внутри клетки); весь слой уменьшен вокруг глаза в дальнюю плоскость кадра (углы те же).
// Туман дальности и цвет неба запечены в картинку (снимок игры при дальности 250 м), рантайм-туман кольцо не трогает. Кольцо рисуется, только пока город до 150 м (drawDist 149,5..155): высокий взгляд (полная даль) и
// пониженная дальность его не берут.
#pragma once
#include "pano917io.h"
typedef struct { float u, v; unsigned c; float x, y, z; } PnoV;
static int pano917Any = 1;   // №917б: кольцо за краем города при любой дальности (не только 150 м); «flag pano917any 0» — прежнее правило
static int pano917Punch = 1;   // «flag pano917punch 0» — без пробивки (A/B гейта); PANO917_OLD гасит всё кольцо
static unsigned pnoUsAcc, pnoUsN, pnoUsMax, pnoLayers, pnoSegs, pnoStepT, pnoLogReady, pnoPunches;   // цена: мкс процессора на кадр, слоёв и сегментов в кадре
// подкачка: на кадр не больше одного нового запроса, пока загрузчик свободен; жертва — слот не из нужных сейчас и не читаемый; кучи нет в запасе — не просим
static void pano917Step(const int *need, int pre) {
  if (pnoReq >= 0 || !pre) return;
  unsigned now = sceKernelGetSystemTimeLow(); if (pnoFails && now - pnoStepT < 1000000u) return;
  for (int k = 0; k < pre; k++) {
    int have = 0; for (int i = 0; i < PNO_SLOTS; i++) if (pnoS[i].id == need[k]) have = 1;
    if (have) continue;
    PnoSlot *v = 0; int vi = -1;
    for (int i = 0; i < PNO_SLOTS && !v; i++) if (pnoS[i].id < 0 && pnoS[i].st == 0) { v = &pnoS[i]; vi = i; }
    for (int i = 0; i < PNO_SLOTS && !v; i++) { if (pnoS[i].st != 2) continue; int in = 0; for (int q = 0; q < pre; q++) if (need[q] == pnoS[i].id) in = 1; if (!in) { v = &pnoS[i]; vi = i; } }
    if (!v) return;
    if (!v->buf) { if (!memPrefetchRoom(pnoSz + (vi < 2 ? 1u << 20 : 3u << 20))) return; v->buf = (unsigned char *)memalign(64, pnoSz); if (!v->buf) return; }   // память на уровень: два слота (смена панорам) с запасом 1 МБ над резервом, третий (подкачка) — 3 МБ
    v->st = 1; v->id = need[k]; pnoStepT = now; __asm__ volatile("" ::: "memory"); pnoReq = vi; ldSignal();
    if (smkOn && pnoLogReady < 12) { pnoLogReady++; char s[96]; snprintf(s, sizeof s, "PANO917 req id=%d x=%.0f z=%.0f slot=%d\n", need[k], pnoE[need[k]].x, pnoE[need[k]].z, vi); smkWrite(s); }
    return;
  }
}
static const float pnoSinT[PNO_NAZ + 1] = {0.f, 0.38268343f, 0.70710678f, 0.92387953f, 1.f, 0.92387953f, 0.70710678f, 0.38268343f, 0.f, -0.38268343f, -0.70710678f, -0.92387953f, -1.f, -0.92387953f, -0.70710678f, -0.38268343f, 0.f};
static const float pnoCosT[PNO_NAZ + 1] = {1.f, 0.92387953f, 0.70710678f, 0.38268343f, 0.f, -0.38268343f, -0.70710678f, -0.92387953f, -1.f, -0.92387953f, -0.70710678f, -0.38268343f, 0.f, 0.38268343f, 0.70710678f, 0.92387953f, 1.f};
// ex, ey, ez — глаз; сегмент вне взгляда (по горизонтали шире кадра на запас) не рисуем
static void pano917Draw(float ex, float ey, float ez) {
  { static int lv_ = -99; if (lv_ != gameLevel) { lv_ = gameLevel; pano917Load(); } }   // панорамы уровня (нет файла — кольца нет)
  if (!pano917Active() || pano917On == 2 || inRoom() || (!pano917Any && (drawDist < 149.5f || drawDist > 155.f))) return;   // pano917On 2 — тест: город 150 м без кольца (красный контроль гейта: «везде 150»)
  unsigned t0 = sceKernelGetSystemTimeLow();
  int ids[PNO_SLOTS]; float f[2], a[2]; int pre = 0; int c = pno917Pick(pnoE, pnoN, ex, ez, ids, f, &pre);
  pano917Step(ids, pre);
  int dr[2], nd = 0; float fd[2];
  for (int k = 0; k < c; k++) for (int i = 0; i < PNO_SLOTS; i++) if (pnoS[i].id == ids[k] && pnoS[i].st == 2) { dr[nd] = i; fd[nd] = f[k]; nd++; break; }
  pnoLayers = (unsigned)nd;
  if (!nd) { unsigned us = sceKernelGetSystemTimeLow() - t0; pnoUsAcc += us; if (us > pnoUsMax) pnoUsMax = us; pnoUsN++; return; }
  pno917Alphas(fd, nd, a);
  const float R = pnoR, RC = R / 0.98078528f; const unsigned H = pnoH;   // 0.9808 = cos(11,25°): вершины сегмента на RC, середина стороны на R
  float fl = sqrtf(cfX * cfX + cfZ * cfZ), fx = fl > .3f ? cfX / fl : 0.f, fz = fl > .3f ? cfZ / fl : 0.f;   // крутой взгляд вниз/вверх — все сегменты
  int fogWas = sceGuGetStatus(GU_FOG), blendWas = sceGuGetStatus(GU_BLEND);
  sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_FOG);
  sceGuEnable(GU_TEXTURE_2D); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0, 0xff);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_CLAMP, GU_CLAMP); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
  sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f);
  { ScePspFMatrix4 id; memset(&id, 0, sizeof id); id.x.x = id.y.y = id.z.z = id.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &id); }
  unsigned segs = 0;
  for (int L = 0; L < nd; L++) {
    const PnoE *e = &pnoE[pnoS[dr[L]].id]; const unsigned char *b = pnoS[dr[L]].buf;
    float dx = e->x - ex, dz = e->z - ez, dist = sqrtf(dx * dx + dz * dz), far_ = sqrtf((RC + dist) * (RC + dist) + 4.f * R * R);
    float k = (drawDist + 4.f) / far_;   // всё внутри дальней плоскости; масштаб вокруг глаза углов не меняет
    float ytop = (e->ey + tanf(e->hi * 0.01745329252f) * R - ey) * k, ybot = (e->ey + tanf(e->lo * 0.01745329252f) * R - ey) * k;
    unsigned col = ((unsigned)(a[L] * 255.f + .5f) << 24) | 0xffffffu;
    if (pnoPsm == 8) { sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, b); sceGuTexMode(GU_PSM_T8, 0, 0, 1); sceGuTexImage(0, 512, (int)(4 * H), 512, b + 1024); }
    else { sceGuClutMode(GU_PSM_8888, 0, 0x0f, 0); sceGuClutLoad(2, b); sceGuTexMode(GU_PSM_T4, 0, 0, 1); sceGuTexImage(0, 512, (int)(4 * H), 512, b + 64); }
    sceGuTexFlush();
    PnoV *v = (PnoV *)sceGuGetMemory(PNO_NAZ * 6 * sizeof(PnoV)); int nv = 0;
    for (int s = 0; s < PNO_NAZ; s++) {
      if (fl > .3f) { float mx = dx + R * (pnoSinT[s] + pnoSinT[s + 1]) * .5f / .98078528f, mz = dz + R * (pnoCosT[s] + pnoCosT[s + 1]) * .5f / .98078528f, ml = sqrtf(mx * mx + mz * mz) + 1e-3f;
        if ((mx * fx + mz * fz) / ml < 0.2f) continue; }   // сегмент вне кадра: центр дальше ±78° от взгляда (кадр ±40…45° по горизонтали, сегмент 22,5°, запас на параллакс и широкий FOV)
      float x0 = (dx + RC * pnoSinT[s]) * k, z0 = (dz + RC * pnoCosT[s]) * k, x1 = (dx + RC * pnoSinT[s + 1]) * k, z1 = (dz + RC * pnoCosT[s + 1]) * k;
      float u0 = ((s & 3) * 128 + 1.5f) / 512.f, u1 = ((s & 3) * 128 + 126.5f) / 512.f, vt = ((s >> 2) * H + .5f) / (4.f * H), vb = ((s >> 2) * H + H - .5f) / (4.f * H);
      PnoV *q = v + nv; nv += 6;
      q[0] = (PnoV){u0, vt, col, x0, ytop, z0}; q[1] = (PnoV){u1, vt, col, x1, ytop, z1}; q[2] = (PnoV){u0, vb, col, x0, ybot, z0};
      q[3] = (PnoV){u1, vt, col, x1, ytop, z1}; q[4] = (PnoV){u1, vb, col, x1, ybot, z1}; q[5] = (PnoV){u0, vb, col, x0, ybot, z0};
    }
    if (nv && L == 0 && pnoFlags && pano917Punch) {   // пробивка (№917, слово дирижёра 24.10): тексели с индексом пробивки (альфа CLUT 1) пишут в глубину «пусто» без цвета — лента силуэта в этих пикселях стёрта, небо «последним» ляжет само
      sceGuPixelMask(0xffffffffu); sceGuDisable(GU_BLEND); sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_EQUAL, 1, 0xff); sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
      sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_ALWAYS); sceGuDepthMask(GU_FALSE); sceGuDepthRange(0, 0);
      sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, nv, 0, v);
      sceGuDepthRange(65535, 0); sceGuDepthFunc(GU_GEQUAL); sceGuDisable(GU_DEPTH_TEST); sceGuPixelMask(0);
      sceGuEnable(GU_BLEND); sceGuAlphaFunc(GU_GREATER, 0, 0xff); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); pnoPunches++; }
    if (nv) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, nv, 0, v); segs += (unsigned)nv / 6; }
  }
  pnoSegs = segs;
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexScale(128.f, 128.f); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); sceGuDisable(GU_ALPHA_TEST);
  if (!blendWas) sceGuDisable(GU_BLEND); sceGuEnable(GU_DEPTH_TEST); if (fogWas) sceGuEnable(GU_FOG);
  resetState();
  unsigned us = sceKernelGetSystemTimeLow() - t0; pnoUsAcc += us; if (us > pnoUsMax) pnoUsMax = us; pnoUsN++;
  if (pnoUsN >= 30) { if (smkOn) { char s[220]; snprintf(s, sizeof s, "PANO917 cost level=%d avg_us=%u max_us=%u frames=%u layers=%u segs=%u loads=%u load_us=%u load_max_us=%u fails=%u punch=%u fl=%.2f\n", gameLevel, pnoUsAcc / pnoUsN, pnoUsMax, pnoUsN, pnoLayers, pnoSegs, pnoLoads, pnoLoadUs, pnoLoadMax, pnoFails, pnoPunches, fl); smkWrite(s); } pnoUsAcc = pnoUsN = pnoUsMax = 0; pnoLoadUs = pnoLoadMax = 0; }
}
