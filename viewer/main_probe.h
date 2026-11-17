// ---------- тест-прогон v2 ----------
// P1: весь уровень (как v1: 169 точек x 4 направления), P2: 16 худших видов x варианты-переключатели,
// P3: 8 худших видов с настоящей вертикальной синхронизацией, считаем пропущенные кадры. Отчёт bench.txt.
#define MAXBP 1024
#define NW2 16
#define NW3 8
static const struct { const char *name; unsigned set, clr; } VAR[] = {
  {"BASE", 0, 0}, {"NOSWZ", 0, O_SWZ}, {"NOTEX", 0, O_TEX}, {"BILIN", 0, O_TRI}, {"NOPVS", 0, O_PVS},
  {"CONE", 0, O_PLANES}, {"LEGACY", 0, O_LEAN}, {"ALLNEAR", 0, O_FAR}, {"NOSKY", 0, O_SKY},
  {"NOSKYCULL", 0, O_SKYCULL}, {"QVP", O_QVP, 0}, {"D100", O_D100, 0}, {"NOHUD", 0, O_HUD},
  {"FAST", O_D100, O_TRI},
  {"NOCULL", 0, O_CULL}, {"SKYFIRST", 0, O_SKYLAST},
};
#define NVAR ((int)(sizeof VAR / sizeof VAR[0]))
static float bpx[MAXBP], bpy[MAXBP], bpz[MAXBP]; static int nbp;
static float p1w[MAXBP * 4], p1all[MAXBP * 8]; static int p1n;
static double p1cpu, p1tris, p1draws; static float p1cpuMax; static int p1trisMax, p1drawsMax, p1pvs;
static int w2[NW2], nw2, w3[NW3], nw3;
static float p2res[NW2][NVAR], p2tri[NVAR], p2drw[NVAR], bm[3]; static int bmn;
static int p3miss[NW3], p3missAll; static float p3max; static unsigned lastVc;
static int bpass, bview, bsub, bvar, bwait, bwaitIdle, bVsync, bMeasure; static unsigned bwaitT0;
static char brep[4][64];
// проверка PVS (служебный файл pvscheck): каждый вид рисуем с PVS и без и сравниваем пиксели
static unsigned *fbA, *curFb; static int pcDiff[MAXBP * 4];
static void pvscheckWrite(void) {
  char fn[256]; sibling(fn, "pvscheck.txt");
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return;
  static char b[4096]; int n = 0, nv = nbp * 4, bad = 0, over100 = 0; long tot = 0;
  for (int v = 0; v < nv; v++) { if (pcDiff[v]) bad++; if (pcDiff[v] > 100) over100++; tot += pcDiff[v]; }
  n += snprintf(b + n, sizeof b - n, "PVSCHECK %d views: with diff %d, diff>100px %d, total diff px %ld, pvs %s\nworst (x z heading px):\n",
                nv, bad, over100, tot, pvsEnt ? "yes" : "no");
  for (int k = 0; k < 25; k++) {
    int bi = -1; for (int v = 0; v < nv; v++) if (pcDiff[v] > 0 && (bi < 0 || pcDiff[v] > pcDiff[bi])) bi = v;
    if (bi < 0) break;
    n += snprintf(b + n, sizeof b - n, "   %.1f %.1f %d %d\n", bpx[bi / 4], bpz[bi / 4], bi % 4, pcDiff[bi]); pcDiff[bi] = -pcDiff[bi];
  }
  for (int v = 0; v < nv; v++) if (pcDiff[v] < 0) pcDiff[v] = -pcDiff[v];
  sceIoWrite(f, b, n); sceIoClose(f);
}

static void benchPoints(void) {
  int st = (int)(50.f / hcell); nbp = 0;
  for (int gz = st / 2, row = 0; gz < hh; gz += st, row++)
    for (int j = 0; j < hw / st + 1; j++) {
      int gx = st / 2 + (row & 1 ? (hw / st - j) : j) * st;
      if (gx < 0 || gx >= hw || nbp >= MAXBP) continue;
      short tmp_[4]; const short *c = hmAt(gx, gz, tmp_); int lo = 32767;
      for (int i = 0; i < 4; i++) if (c[i] != -32768 && c[i] < lo) lo = c[i];
      if (lo == 32767) continue;
      bpx[nbp] = hox + (gx + .5f) * hcell; bpz[nbp] = hoz + (gz + .5f) * hcell; bpy[nbp] = lo * .01f; nbp++;
    }
}
static int curView(void) { return bpass == 1 || bpass == 5 ? bview : bpass == 2 ? w2[bview] : w3[bview]; }
static void teleport(void) { bwait = 1; bwaitIdle = 0; bwaitT0 = sceKernelGetSystemTimeLow(); bsub = 0; }
static int cmpF(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static void pickWorst(int *out, int n, int *nout) {    // n худших видов P1, по порядку обхода
  *nout = 0;
  for (int k = 0; k < n; k++) {
    int bi = -1;
    for (int v = 0; v < nbp * 4; v++) {
      int used = 0; for (int j = 0; j < *nout; j++) if (out[j] == v) used = 1;
      if (!used && (bi < 0 || p1w[v] > p1w[bi])) bi = v;
    }
    if (bi < 0) break; out[(*nout)++] = bi;
  }
  for (int i = 1; i < *nout; i++) for (int j = i; j > 0 && out[j] < out[j - 1]; j--) { int t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
}
static void benchWrite(void) {
  char fn[256]; sibling(fn, "bench.txt");
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return;
  static char b[8192]; int n = 0;
  n += snprintf(b + n, sizeof b - n, "BENCH2 v8 (%s %s)  opt %03x  pvs %s  view %.0f m\n", __DATE__, __TIME__, O_DEFAULT, pvsEnt ? "yes" : "no", longView ? DIST_LONG : DIST_SHORT);
  if (p1n) {
    static float s[MAXBP * 8]; memcpy(s, p1all, p1n * sizeof(float)); qsort(s, p1n, sizeof(float), cmpF);
    double sum = 0; int over = 0, overT = 0; float tgt = longView ? 33.3f : 16.7f;
    for (int i = 0; i < p1n; i++) { sum += s[i]; if (s[i] > 16.7f) over++; if (s[i] > tgt) overT++; }
    n += snprintf(b + n, sizeof b - n, "ACCEPT P1 work p95 %.2f p99 %.2f max %.2f ms; over target %.1f ms (%s) %d frames\n",   // аудит §10.8
                  s[p1n * 95 / 100], s[p1n * 99 / 100], s[p1n - 1], tgt, longView ? "30 FPS" : "60 FPS", overT);
    n += snprintf(b + n, sizeof b - n,
      "P1 %d views, %d frames: work avg %.2f p50 %.2f p90 %.2f p99 %.2f max %.2f ms; over 16.7ms %.1f%%\n"
      "   cpu avg %.2f max %.2f ms; tris avg %.1fk max %.1fk; draws avg %.0f max %d; pvs used %d%%\n"
      "   (v7: avg 16.8, worst 25.0, over 55.2%%)\n",
      nbp * 4, p1n, sum / p1n, s[p1n / 2], s[p1n * 9 / 10], s[p1n * 99 / 100], s[p1n - 1], 100.f * over / p1n,
      p1cpu / p1n, p1cpuMax, p1tris / p1n / 1000., p1trisMax / 1000.f, p1draws / p1n, p1drawsMax, 100 * p1pvs / p1n);
  }
  if (clearChk) n += snprintf(b + n, sizeof b - n, "CLEARCHK: views with uncovered px %d, max %d px, total %ld px\n", ccViews, ccMax, ccTot);
  n += snprintf(b + n, sizeof b - n, "frame %s, cull %s, sky %s\n", fb16 ? "5650+dither" : "8888", (O_DEFAULT & O_CULL) ? "on" : "off", (O_DEFAULT & O_SKYLAST) ? "last" : "first");
  if (bpass >= 3 || (bpass == 0 && nw2)) {
    n += snprintf(b + n, sizeof b - n, "P2 %d worst views, median of 3 frames, no vsync:\n   variant     ms   delta   tris  draws\n", nw2);
    float base = 0; for (int v = 0; v < nw2; v++) base += p2res[v][0]; base /= nw2 ? nw2 : 1;
    unsigned npre = 0; for (unsigned t = 0; t < ntex; t++) npre += tex[t].pre != 0;
    for (int k = 0; k < NVAR; k++) {
      if (VAR[k].clr == O_SWZ && npre) {   // аудит §10.5: swz = pre || ... — на заранее swizzled данных вариант ничего не выключает
        n += snprintf(b + n, sizeof b - n, "   %-10s   n/a: %u of %u textures pre-swizzled in data, variant does not apply\n", VAR[k].name, npre, ntex); continue; }
      float m = 0; for (int v = 0; v < nw2; v++) m += p2res[v][k]; m /= nw2 ? nw2 : 1;
      n += snprintf(b + n, sizeof b - n, "   %-10s %5.2f %+6.2f %5.1fk %5.0f\n", VAR[k].name, m, m - base,
                    p2tri[k] / (nw2 * 3) / 1000.f, p2drw[k] / (nw2 * 3));
    }
  }
  if (bpass == 0 && nw3) {
    n += snprintf(b + n, sizeof b - n, "P3 vsync, %d worst views x 60 frames: missed vblanks total %d (", nw3, p3missAll);
    for (int i = 0; i < nw3; i++) n += snprintf(b + n, sizeof b - n, "%d%s", p3miss[i], i + 1 < nw3 ? " " : "");
    n += snprintf(b + n, sizeof b - n, "), worst work %.2f ms; target period %d vblank (%.1f ms, %s)\n", p3max,
                  longView ? 2 : 1, longView ? 33.3f : 16.7f, longView ? "30 FPS / 250 m" : "60 FPS / 115 m");
  }
  n += snprintf(b + n, sizeof b - n, "ACCEPT frames with visible non-resident cells %d (max %d); read errors %u; alloc failures %u; min free heap %.2f MB (start %.2f, ballast %u MB)\n",
                stMissFrames, stMissMax, smkReadErr, smkAllocFail, smkHeapMin, smkHeapFree0 / 1048576.f, smkBallastMB);
  if (p1n) {
    n += snprintf(b + n, sizeof b - n, "worst P1 views (x z heading work):\n");
    for (int i = 0; i < nw2 && n < (int)sizeof b - 80; i++) {
      int v = w2[i]; n += snprintf(b + n, sizeof b - n, "   %.0f %.0f %d %.2f\n", bpx[v / 4], bpz[v / 4], v % 4, p1w[v]);
    }
  }
  sceIoWrite(f, b, n); sceIoClose(f);
}
static void benchStart(void) {
  benchPoints(); bpass = 1; bview = 0; p1n = 0; p1cpu = p1tris = p1draws = 0; p1cpuMax = 0; p1trisMax = p1drawsMax = p1pvs = 0;
  memset(p1w, 0, sizeof p1w); memset(p2res, 0, sizeof p2res); memset(p2tri, 0, sizeof p2tri); memset(p2drw, 0, sizeof p2drw);
  nw2 = nw3 = 0; p3missAll = 0; p3max = 0; memset(p3miss, 0, sizeof p3miss); bvar = 0; bmn = 0;
  teleport();
}
// перед кадром: куда смотрим, какие переключатели, меряем ли этот кадр
static void benchBefore(int pending, float sph, float *x, float *y, float *z, float *h) {
  int v = curView();
  *x = bpx[v / 4]; *y = bpy[v / 4]; *z = bpz[v / 4]; *h = sph + (v % 4) * GU_PI * .5f;
  opt = O_DEFAULT; bMeasure = 0; bVsync = bpass == 3;
  if (bwait) {
    if (!pending) bwaitIdle++; else bwaitIdle = 0;
    if (bwaitIdle >= 2 || sceKernelGetSystemTimeLow() - bwaitT0 > 5000000) bwait = 0;
    else { sceKernelDelayThread(2000); return; }        // ждём загрузку клеток, отдаём время загрузчику
  }
  if (bpass == 1) bMeasure = bsub >= 2;
  else if (bpass == 2) { opt = (O_DEFAULT | VAR[bvar].set) & ~VAR[bvar].clr; bMeasure = bsub >= 2; }
  else if (bpass == 3) bMeasure = bsub >= 2;
  else if (bpass == 5) { opt = O_DEFAULT & ~O_HUD & ~(bsub == 1 ? O_PVS : 0); bMeasure = 1; }
}
// после кадра: записать замер, сдвинуть состояние
static void benchAfter(float work, float cpu, int missed) {
  if (bwait) return;
  int v = curView();
  if (bpass == 5) {
    unsigned *fb = (unsigned *)((unsigned)curFb | 0x40000000);
    if (bsub == 0) memcpy(fbA, fb, BUF_W * SCR_H * (fb16 ? 2 : 4));
    else {
      int d = 0;
      for (int yy = 0; yy < SCR_H; yy++) for (int xx = 0; xx < SCR_W; xx++) {
        unsigned a, c;
        if (fb16) {   // 5650 -> 888
          unsigned short pa = ((unsigned short *)fbA)[yy * BUF_W + xx], pb = ((unsigned short *)fb)[yy * BUF_W + xx];
          a = ((pa & 31) << 3) | ((pa >> 5 & 63) << 10) | ((pa >> 11) << 19); c = ((pb & 31) << 3) | ((pb >> 5 & 63) << 10) | ((pb >> 11) << 19);
        } else { a = fbA[yy * BUF_W + xx]; c = fb[yy * BUF_W + xx]; }
        int e = abs((int)(a & 255) - (int)(c & 255)) + abs((int)((a >> 8) & 255) - (int)((c >> 8) & 255)) + abs((int)((a >> 16) & 255) - (int)((c >> 16) & 255));
        if (e > 48) d++;
      }
      pcDiff[v] = d;
    }
    if (++bsub >= 2) {
      bsub = 0;
      if (++bview >= nbp * 4) { bpass = 0; pvscheckWrite(); snprintf(brep[0], 64, "PVSCHECK done"); snprintf(brep[1], 64, "pvscheck.txt"); }
      else if (bview % 4 == 0) teleport();
    }
    return;
  }
  if (bpass == 1) {
    if (bMeasure && clearChk && bsub == 3) {   // clearchk: пурпурные (непокрытые) пиксели последнего кадра вида
      const void *fb = (const void *)((unsigned)curFb | 0x40000000); int m = 0;
      for (int yy = 0; yy < SCR_H; yy++) for (int xx = 0; xx < SCR_W; xx++)
        m += fb16 ? ((const unsigned short *)fb)[yy * BUF_W + xx] == 0xF81F : (((const unsigned *)fb)[yy * BUF_W + xx] & 0xffffff) == 0xff00ff;
      if (m) ccViews++; if (m > ccMax) ccMax = m; ccTot += m;
    }
    if (bMeasure) {
      if (p1n < MAXBP * 8) p1all[p1n++] = work;
      if (work > p1w[v]) p1w[v] = work;
      p1cpu += cpu; if (cpu > p1cpuMax) p1cpuMax = cpu;
      p1tris += drawnTris; if (drawnTris > p1trisMax) p1trisMax = drawnTris;
      p1draws += draws; if (draws > p1drawsMax) p1drawsMax = draws; p1pvs += pvsUsed;
    }
    if (++bsub >= 4) {
      bsub = 0;
      if (++bview >= nbp * 4) {
        pickWorst(w2, NW2, &nw2); benchWrite();
        bpass = 2; bview = 0; bvar = 0; bmn = 0; teleport();
      } else if (bview % 4 == 0) teleport();
    }
  } else if (bpass == 2) {
    if (bMeasure) { bm[bmn++] = work; p2tri[bvar] += drawnTris; p2drw[bvar] += draws; }
    if (++bsub >= 5) {
      qsort(bm, bmn, sizeof(float), cmpF); p2res[bview][bvar] = bm[bmn / 2]; bmn = 0; bsub = 0;
      if (++bvar >= NVAR) {
        bvar = 0;
        if (++bview >= nw2) {
          pickWorst(w3, NW3, &nw3); benchWrite();
          bpass = 3; bview = 0; teleport();
        } else teleport();
      }
    }
  } else if (bpass == 3) {
    if (bMeasure) { if (missed) { p3miss[bview]++; p3missAll++; } if (work > p3max) p3max = work; }
    if (++bsub >= 62) {
      if (++bview >= nw3) {
        bpass = 0; benchWrite();
        snprintf(brep[0], 64, "BENCH v2 done. vsync misses: %d", p3missAll);
        snprintf(brep[1], 64, "report: bench.txt   O: close");
        brep[2][0] = 0;
      } else teleport();
    }
  }
}

