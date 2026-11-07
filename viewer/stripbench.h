// Тур на железе: список треугольников против неиндексных полос (К4, ОПТИМИЗАТОРЫ_РЕСЁРЧ 17.10, docs/ТУР_ПОЛОСЫ_GE_ТЗ_2026-10-17.md).
// Включается ТОЛЬКО служебным файлом stripbench рядом с EBOOT (и данными stripbench.bin рядом, tools/strip_probe --emit); без файла код не исполняется.
// После levelLoad: сцена из кусков файла (одна плотная клетка L1), свои проекция и вид; режимы кадра:
//   A список (GU_TRIANGLES) · A2 то же из готового листа · B отдельные полосы (по команде на полосу) · B2 то же из готового листа · C одна полоса с вырожденными.
// Каждый режим в двух вариантах: V0 обычный вьюпорт, V1 scissor 1×1 (заливка снята, остаются вершины и команды). 20 кадров прогрева + 120 замера, без vsync.
// Время: cpu — от начала сборки списка до sceGuFinish; tot — до конца sceGuSync (GE дорабатывает); медиана и худшее, мкс. Отчёт stripbench.txt.
// Проверка «режимы рисуют одинаково»: после замера один кадр режима читается из буфера (5650) и сравнивается с режимом A (diff px).
typedef struct { unsigned piece, flags; int tex; unsigned vs; float ox, oy, oz, sc; unsigned nList, nStripV, nStrips, nSew; float bb[6];
                 const unsigned char *list, *strip, *sew; const unsigned short *len; unsigned *dlA, *dlB; } SbRec;
static SbRec *sbR; static int sbN; static void *sbMem; static float sbC[3];
static int sbWanted(void) { char fn[256]; sibling(fn, "stripbench"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0; sceIoClose(f); return 1; }
static int sbLoad(void) {
  if (sbMem) return 0;   // повторный запуск сценарием — данные уже в памяти
  char fn[256]; sibling(fn, "stripbench.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return -1;
  int sz = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET); sbMem = memalign(64, (size_t)sz + 64); if (!sbMem) { sceIoClose(f); return -2; }
  int got = sceIoRead(f, sbMem, sz); sceIoClose(f); if (got != sz || memcmp(sbMem, "SBN1", 4)) return -3;
  const unsigned char *p = (const unsigned char *)sbMem; unsigned n; memcpy(&n, p + 4, 4); p += 8;
  sbR = (SbRec *)calloc(n ? n : 1, sizeof(SbRec)); if (!sbR) return -4;
  for (unsigned i = 0; i < n; i++) {
    SbRec *r = &sbR[i]; unsigned h[18]; memcpy(h, p, 72); p += 72;
    r->piece = h[0]; r->flags = h[1]; r->tex = (int)h[2]; r->vs = h[3]; memcpy(&r->ox, h + 4, 16); r->nList = h[8]; r->nStripV = h[9]; r->nStrips = h[10]; r->nSew = h[11]; memcpy(r->bb, h + 12, 24);
    r->list = p; p += (size_t)r->nList * r->vs; r->strip = p; p += (size_t)r->nStripV * r->vs; r->len = (const unsigned short *)p; p += ((size_t)r->nStrips * 2 + 3) / 4 * 4;
    r->sew = p; p += (size_t)r->nSew * r->vs; p = (const unsigned char *)(((uintptr_t)p + 15) & ~(uintptr_t)15);
    if (p - (const unsigned char *)sbMem > sz) return -5;
  }
  sbN = (int)n; sceKernelDcacheWritebackInvalidateRange(sbMem, (unsigned)sz + 64);
  float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  for (int i = 0; i < sbN; i++) for (int a = 0; a < 3; a++) { if (sbR[i].bb[a] < lo[a]) lo[a] = sbR[i].bb[a]; if (sbR[i].bb[a + 3] > hi[a]) hi[a] = sbR[i].bb[a + 3]; }
  for (int a = 0; a < 3; a++) sbC[a] = (lo[a] + hi[a]) * .5f;
  return 0;
}
enum { SB_A, SB_A2, SB_B, SB_B2, SB_C, SB_NMODE };
static const char *const sbName[SB_NMODE] = {"A_list", "A2_list_dl", "B_strips", "B2_strips_dl", "C_one_strip"};
static void sbDrawRec(const SbRec *r, int mode) {
  const int vt = (r->flags & F_V12) ? VT12 : VT16, vs = (int)r->vs;
  if (mode == SB_A) sceGuDrawArray(GU_TRIANGLES, vt, (int)r->nList, 0, r->list);
  else if (mode == SB_B) { const unsigned char *v = r->strip; for (unsigned k = 0; k < r->nStrips; k++) { sceGuDrawArray(GU_TRIANGLE_STRIP, vt, r->len[k], 0, v); v += (size_t)r->len[k] * vs; } }
  else if (mode == SB_C) sceGuDrawArray(GU_TRIANGLE_STRIP, vt, (int)r->nSew, 0, r->sew);
}
static unsigned *sbBuildDl(const SbRec *r, int mode) {   // лист-вызов: только команды рисования (текстура, флаги, матрица ставятся кадром)
  unsigned words = 64 + (mode == SB_B ? r->nStrips * 8 : 16), bytes = words * 4; unsigned *b = (unsigned *)memalign(16, bytes); if (!b) return 0;
  sceKernelDcacheWritebackInvalidateRange(b, bytes); dlLost(); sceGuStart(GU_CALL, b); sbDrawRec(r, mode == SB_B ? SB_B : SB_A); unsigned sz = (unsigned)sceGuFinish(); dlLost();
  if (sz > bytes) { free(b); return 0; }
  return b;
}
#define SBOUT(f, str, len) do { if ((f) >= 0) sceIoWrite((f), (str), (len)); smkWrite(str); } while (0)
static int sbCmp(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
static unsigned short *sbShot;
static void sbFrame(int mode, int var, int reps, unsigned *tCpu, unsigned *tTot, void **fbp, int grab) {
  GE_NEST_GUARD("sbFrame");   // №1186/№1188
  unsigned t0 = sceKernelGetSystemTimeLow();
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuViewport(2048, 2048, SCR_W, SCR_H); sceGuScissor(0, 0, var ? 1 : SCR_W, var ? 1 : SCR_H);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuShadeModel(GU_SMOOTH); sceGuDisable(GU_FOG);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvlMode = 0;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuClearColor(0xff604020); sceGuClearDepth(0); if (!var) sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  sceGuDisable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); curFlags = -1;
  sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(60.f, ASPECT, 1.f, 1000.f);
  sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
  { ScePspFVector3 E = {0.f, 38.f, 78.f}, A = {0.f, 6.f, 0.f}, U = {0.f, 1.f, 0.f}; sceGumLookAt(&E, &A, &U); }
  sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); sceGumUpdateMatrix();
  for (int rp = 0; rp < reps; rp++)
    for (int i = 0; i < sbN; i++) {
      const SbRec *r = &sbR[i]; const int dl = (mode == SB_A2 || mode == SB_B2);
      bindTexLean(r->tex); setFlags((int)r->flags);
      ScePspFMatrix4 m = {{r->sc, 0, 0, 0}, {0, r->sc, 0, 0}, {0, 0, r->sc, 0}, {r->ox - sbC[0], r->oy - sbC[1], r->oz - sbC[2] - 46.f * rp, 1}};
      sceGuSetMatrix(GU_MODEL, &m);
      if (dl) { unsigned *b = mode == SB_A2 ? r->dlA : r->dlB; if (b) sceGuCallList(b); } else sbDrawRec(r, mode);
    }
  sceGuScissor(0, 0, SCR_W, SCR_H);
  unsigned t1 = sceKernelGetSystemTimeLow(); sceGuFinish(); sceGuSync(0, 0); unsigned t2 = sceKernelGetSystemTimeLow();
  *tCpu = t1 - t0; *tTot = t2 - t0;
  if (grab && fb16 && sbShot) { const unsigned short *s = (const unsigned short *)((unsigned)sceGeEdramGetAddr() + (unsigned)*fbp); for (int y = 0; y < SCR_H; y++) memcpy(sbShot + y * 512, s + y * 512, 480 * 2); }
}
static void sbRun(void **fbp) {
  char out[256]; sibling(out, "stripbench.txt"); SceUID of = sceIoOpen(out, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); char s[320]; int n;
  int e = sbLoad();
  if (e) { n = snprintf(s, sizeof s, "STRIPBENCH load error %d (нужен stripbench.bin рядом с EBOOT)\n", e); SBOUT(of, s, n); if (of >= 0) sceIoClose(of); return; }
  unsigned tv = 0, ts = 0, sv = 0, sw = 0; for (int i = 0; i < sbN; i++) { tv += sbR[i].nList; ts += sbR[i].nStrips; sv += sbR[i].nStripV; sw += sbR[i].nSew; }
  int reps = 8; { int per = (int)(ts * 8 + sbN * 12); if (per < 1) per = 1; int lim = 40000 / per; if (lim < 1) lim = 1; if (lim < reps) reps = lim; }
  n = snprintf(s, sizeof s, "STRIPBENCH pieces=%d reps=%d vertsList=%u vertsStrips=%u strips=%u vertsSewn=%u tris=%u\n", sbN, reps, tv, sv, ts, sw, tv / 3);
  SBOUT(of, s, n);
  for (int i = 0; i < sbN; i++) { sbR[i].dlA = sbBuildDl(&sbR[i], SB_A); sbR[i].dlB = sbBuildDl(&sbR[i], SB_B); }
  sbShot = (unsigned short *)malloc(512 * 272 * 2); unsigned short *ref = (unsigned short *)malloc(512 * 272 * 2); int haveRef = 0;
  static unsigned cpu[128], tot[128];
  for (int var = 0; var < 2; var++) for (int mode = 0; mode < SB_NMODE; mode++) {
    for (int k = 0; k < 20; k++) { unsigned a, b; sbFrame(mode, var, reps, &a, &b, fbp, 0); }
    for (int k = 0; k < 120; k++) sbFrame(mode, var, reps, &cpu[k], &tot[k], fbp, 0);
    unsigned cmax = 0, tmax = 0; for (int k = 0; k < 120; k++) { if (cpu[k] > cmax) cmax = cpu[k]; if (tot[k] > tmax) tmax = tot[k]; }
    qsort(cpu, 120, 4, sbCmp); qsort(tot, 120, 4, sbCmp);
    int diff = -1; unsigned a, b;
    if (!var) { sbFrame(mode, 0, reps, &a, &b, fbp, 1);
      if (mode == SB_A) { memcpy(ref, sbShot, 512 * 272 * 2); haveRef = 1; diff = 0; }
      else if (haveRef) { diff = 0; for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W; x++) if (ref[y * 512 + x] != sbShot[y * 512 + x]) diff++; }
      sceDisplayWaitVblankStart(); *fbp = swapBuf(); }
    unsigned dv = mode == SB_A || mode == SB_A2 ? tv * reps : mode == SB_C ? sw * reps : sv * reps;
    n = snprintf(s, sizeof s, "%s V%d verts=%u cpu_med=%u cpu_max=%u tot_med=%u tot_max=%u diffpx=%d\n", sbName[mode], var, dv, cpu[60], cmax, tot[60], tmax, diff);
    SBOUT(of, s, n);
  }
  if (of >= 0) sceIoClose(of);
  resetState(); dlLost();   // кадр движка заново ставит своё состояние
}
