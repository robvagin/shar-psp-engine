// №1099 (Роб 02.11.2026, окно g12-data): светящиеся окна вечернего и ночного города (L4, L5, L7, мелкие на L1) издалека «бьются, ломаются и сливаются в одну большую
// оранжевую лужу». Причина по замеру tools/winspr_export.py и журнала: окно — наклейка 32x32 с тёмной рамкой 2–3 текселя; дальше ~50 м рамка меньше пикселя, мип
// смешивает окна и рамки в пятно, а мелкая рамка мигает при движении (на L4 окно в среднем в 5–60 см перед стеной, 16-бит глубина дальше ~75–240 м не различает).
// Закон света (psp-render §5): вдали — ровный цвет без текстуры, один вызов, глубину не писать. Здесь: дальше WINSPR_DIST метров от глаза каждое окно рисуется ровным
// прямоугольником цвета своей яркой середины (треугольники из файла winspr-l<N>.shr: готовые середины окон, tools/winspr_export.py), со своим сдвигом глубины
// WINSPR_BIAS к камере; ближе — как было (рамка и переплёты текстурой). Клетки дальше farPlainD (дальний проход города, своя проекция, глубина потом очищается) рисуются в
// ДАЛЬНЕМ проходе (zfPassEnd), остальные — в основном (после света), по одному вызову sceGuDrawArray на проход; у куска окна дальней клетки текстурный рисунок не рисуется вовсе
// (WINSPR_SKIP). Файла нет или он не от этого level.shr — ничего не меняется. WINSPR1099_OLD=1 — как было (файл не читается, куски рисуются, спрайтов нет).
#ifndef WINSPR_H
#define WINSPR_H
// №1155 (Роб: «никаких отдельных светящихся спрайтов; окно — часть текстуры дома, не мигает вдали») заменяет №1099 и №1125: спрайты окон по умолчанию ВЫКЛЮЧЕНЫ (файл winspr-lN.shr не читается, куски окон
// рисуются текстурой, как до №1099). WINFLAT1155_OLD=1 — спрайты окон включены (как TEST84).
#ifndef WINFLAT1155_OLD
#define WINFLAT1155_OLD 0
#endif
#ifndef WINSPR1099_OLD
#define WINSPR1099_OLD (!WINFLAT1155_OLD)
#endif
#ifndef WINSPR_DIST
#define WINSPR_DIST 50.f       // м от глаза: дальше — ровный прямоугольник (рамка окна в 2 текселя из 32 при окне ≈ 3,5 м становится меньше пикселя на ≈ 50 м при угле кадра порта)
#endif
#ifndef WINSPR_BIAS
#define WINSPR_BIAS 3          // единиц глубины к камере (≈ 11 см на 60 м основного прохода; в дальнем проходе — миллиметры)
#endif
#ifndef WINSPR_MAXTRI
#define WINSPR_MAXTRI 1000     // треугольников за вызов (3000 вершин по 16 Б = 48 КБ в кольце кадра)
#endif
// №1125 (Роб на PSP, TEST84, L7: жёлтые окна мигают и видны сквозь фасады). Причина по коду: дальний проход (zfPassEnd) идёт при виде БЕЗ переноса (город в осях камеры, как у кусков), а окна рисовались
// в абсолютных координатах — вдали окно уезжало на вектор глаза и ездило вместе с камерой; в основном проходе абсолютные координаты в сотнях метров дают у GE (float24) ошибку вершины до 2 см (шаг
// глубины 16 бит у дальних клеток — миллиметры) — окно дралось с фасадом. Теперь: вершины всегда в осях камеры (v − глаз, вид без переноса — как у города), окно ВЫНЕСЕНО к камере по нормали на 
// WINZ_K шагов глубины 16 бит на этом расстоянии плюс WINZ_C (запас на ошибки float24), обращённые от камеры окна не рисуются (иначе окна дальних стен видны сквозь дом при смещении глубины), глубина
// проверяется (GEQUAL, смещение WINSPR_BIAS) и не пишется. WINZ1125_OLD=1 — как в TEST84 (абсолютные координаты, без выноса и отсечения тыльных).
#ifndef WINZ1125_OLD
#define WINZ1125_OLD 0
#endif
#ifndef WINZ_K
#define WINZ_K 4.f      // шагов глубины 16 бит выноса к камере: шаг = (far − near)·d² / (far·near·65535) (psp-render §4)
#endif
#ifndef WINZ_C
#define WINZ_C 0.02f    // м: запас на ошибку float24 у GE (≈ 2 см в осях камеры на 500–900 м от начала координат; в осях камеры, как у города, она на порядок меньше)
#endif
static int winCityRel = 1;   // main_loop.h: 1 — город рисуется при виде без переноса (rel); дальний проход идёт именно так
typedef struct { unsigned c; float x, y, z; } WspV;
typedef struct { unsigned cell, first, n; } WspCell;
typedef struct { float v[9]; unsigned c; } WspTri;
static unsigned char *winBuf; static const WspCell *winCell; static unsigned winNCell; static const WspTri *winTri;
static WspV winScr[WINSPR_MAXTRI * 3];
static unsigned winDrawN[2], winDrawTri[2], winDrawUs[2], winFrames;   // служебно (смоук): вызовов, треугольников, мкс (основной / дальний)
static void winsprFree(void) { free(winBuf); winBuf = 0; winCell = 0; winNCell = 0; winTri = 0; free(winTex); winTex = 0; winTexN = 0; }
// file — весь файл (n байт), lvl — номер уровня; ntex_/npc_/ncell_ — из заголовка загруженного level.shr (отпечаток). 1 — принят
static int winsprParse(const unsigned char *f, unsigned n, int lvl, unsigned ntex_, unsigned npc_, unsigned ncell_) {
  winsprFree();
  if (WINSPR1099_OLD || n < 48 || memcmp(f, "WSP1", 4) != 0) return 0;
  const unsigned *h = (const unsigned *)f;
  if (h[1] != 1 || (int)h[2] != lvl || h[3] != ntex_ || h[4] != npc_ || h[5] != ncell_) return 0;
  unsigned nc = h[6], oc = h[7], nt = h[8], ot = h[9], nx = h[10], ox = h[11];
  if (nc > 4096 || (unsigned long long)oc + 12ull * nc > n || (unsigned long long)ot + 40ull * nt > n || (unsigned long long)ox + 4ull * nx > n || nx > 64) return 0;
  winBuf = (unsigned char *)malloc(n); if (!winBuf) return 0; memcpy(winBuf, f, n);
  winCell = (const WspCell *)(winBuf + oc); winNCell = nc; winTri = (const WspTri *)(winBuf + ot);
  for (unsigned i = 0; i < nc; i++) if (winCell[i].cell >= ncell_ || (unsigned long long)winCell[i].first + winCell[i].n > nt) { winsprFree(); return 0; }
  winTex = (unsigned char *)calloc(ntex_ ? ntex_ : 1, 1); if (!winTex) { winsprFree(); return 0; }
  const int *tx = (const int *)(winBuf + ox); for (unsigned i = 0; i < nx; i++) if (tx[i] >= 0 && (unsigned)tx[i] < ntex_) winTex[tx[i]] = 1;
  winTexN = ntex_; return 1;
}
// проход: 0 — основной (после света), 1 — дальний (до очистки глубины, дальняя проекция)
static void winsprFlush(int farPass, int inR) {
  if (!winNCell || inR || WINSPR1099_OLD) return;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0, nv = 0; int two = zfBegin() == 2; const float n2 = WINSPR_DIST * WINSPR_DIST, f2 = farPlainD * farPlainD;
  for (unsigned k = 0; k < winNCell && nv + 3 <= WINSPR_MAXTRI * 3; k++) {
    unsigned ci = winCell[k].cell; if (ci >= ncell || cl[ci].state != 2) continue;
    if ((two && boxD2(cl[ci].bb) > f2) != (farPass != 0)) continue;   // дальняя клетка — только в дальнем проходе
    float bi[6]; memcpy(bi, cl[ci].bb, sizeof bi);
    if (!chClassify(bi, pl, eyeX, eyeY, eyeZ, drawDist)) continue;
    const WspTri *t = winTri + winCell[k].first;
    for (unsigned j = 0; j < winCell[k].n && nv + 3 <= WINSPR_MAXTRI * 3; j++, t++) {
      float dx = t->v[0] - eyeX, dy = t->v[1] - eyeY, dz = t->v[2] - eyeZ;
      if (dx * dx + dy * dy + dz * dz < n2) continue;
      if (WINZ1125_OLD) { for (int m = 0; m < 3; m++) { WspV *o = &winScr[nv++]; o->c = t->c; o->x = t->v[3 * m]; o->y = t->v[3 * m + 1]; o->z = t->v[3 * m + 2]; } continue; }
      float e1[3] = {t->v[3] - t->v[0], t->v[4] - t->v[1], t->v[5] - t->v[2]}, e2[3] = {t->v[6] - t->v[0], t->v[7] - t->v[1], t->v[8] - t->v[2]};
      float nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0], nl = sqrtf(nx * nx + ny * ny + nz * nz);
      if (nl < 1e-9f) continue; nx /= nl; ny /= nl; nz /= nl;
      float cx = (t->v[0] + t->v[3] + t->v[6]) * (1.f / 3.f) - eyeX, cy = (t->v[1] + t->v[4] + t->v[7]) * (1.f / 3.f) - eyeY, cz = (t->v[2] + t->v[5] + t->v[8]) * (1.f / 3.f) - eyeZ;
      float fc = -(nx * cx + ny * cy + nz * cz); if (fc <= 0.f) continue;   // тыльная сторона (окно дальней стены дома) — не рисуем
      float dd = sqrtf(cx * cx + cy * cy + cz * cz), zn = farPass ? farPlainD * ZF932_NEAR_K : zfNear, zf = drawDist + 10.f;
      float eps = WINZ_K * (zf - zn) * dd * dd / (zf * zn * 65535.f) + WINZ_C;   // вынос к камере по нормали
      for (int m = 0; m < 3; m++) { WspV *o = &winScr[nv++]; o->c = t->c; o->x = t->v[3 * m] - eyeX + nx * eps; o->y = t->v[3 * m + 1] - eyeY + ny * eps; o->z = t->v[3 * m + 2] - eyeZ + nz * eps; }
    }
  }
  if (!nv) return;
  unsigned b0 = ggVbUsed; WspV *q = (WspV *)ggVbAlloc(nv * sizeof(WspV)); if (!q) return;
  memcpy(q, winScr, nv * sizeof(WspV)); ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  ScePspFMatrix4 vA, vR; if (!WINZ1125_OLD) { sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vA); sceGumMatrixMode(GU_MODEL); vR = vA; vR.w.x = vR.w.y = vR.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vR); }   // №1125: вершины в осях камеры
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_BLEND); sceGuDepthMask(GU_TRUE); setCull(0); sceGuEnable(GU_FOG);
  sceGuDepthOffset(WINZ1125_OLD ? WINSPR_BIAS : 0);   // №1125: запас даёт вынос по нормали (WINZ_K шагов + WINZ_C), смещение глубины не нужно (оно же и тянуло окно сквозь ближние дома)
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nv, 0, q);
  sceGuDepthOffset(curBias); if (!WINZ1125_OLD) sceGuSetMatrix(GU_VIEW, (farPass && winCityRel) ? &vR : &vA);   // дальний проход продолжается видом без переноса, основной — с переносом
  sceGuEnable(GU_TEXTURE_2D); resetState();
  winDrawN[farPass]++; winDrawTri[farPass] += nv / 3; if (smkOn) winDrawUs[farPass] += sceKernelGetSystemTimeLow() - t0;
}
#endif
