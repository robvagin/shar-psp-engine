// ---- настройки игрока из меню оригинала (Options → Sound, пауза → Options → Sound / Settings; tools/menu_export.py):
// файл settings.bin рядом с EBOOT. Громкость 0..1000 (шаг ползунка 100), варианты — номер строки Scrooby (Radar: Off/Auto)
// Settings / cfg — выше, у newGame (подсказки Барта читают cfg.tutSeen)
// правка 144: насыщенность сменилась — палитры мира заново из файлов (без копии в ОЗУ) и через кривую с новым множителем.
// Читается только 1 КБ на палитру (≈ 600 палитр уровня 1); пока игрок в паузе. Строка TONE — гейт tests/tone/run.sh
typedef struct { SceUID fd; int file; } ToneRd;
static unsigned toneFdOpened, toneFdClosed;   // 16.10 (аудит железа): учёт дескрипторов смены тона; в покое равны (строка TONEFD в smoke.txt)
static void toneRdClose(ToneRd *r) { if (r->fd >= 0) { sceIoClose(r->fd); toneFdClosed++; } r->fd = -1; r->file = -1; }
static int toneRead(void *u, unsigned file, unsigned off, void *dst, unsigned n) {
  ToneRd *r = (ToneRd *)u;
  if (r->file != (int)file) {
    if (r->fd >= 0) { sceIoClose(r->fd); toneFdClosed++; }
    char fn[256]; r->file = (int)file;
    if (file == TF_LEVEL) snprintf(fn, sizeof fn, "%s", levelShr);
    else if (file == TF_TRAFFIC) levelFile(fn, "traffic.shr");
    else if (file == TF_MCR) snprintf(fn, sizeof fn, "%s", mcrPath);
    else levelFile(fn, "interiors-l%d-gfx.bin");
    r->fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (r->fd >= 0) toneFdOpened++;
  }
  if (r->fd < 0 || sceIoLseek32(r->fd, (int)off, PSP_SEEK_SET) != (int)off) return -1;
  return sceIoRead(r->fd, dst, n) == (int)n ? 0 : -1;
}
static void toneHashLog(const char *who) {   // гейт «бит в бит»: хэш всех палитр мира после смены тона (старый и новый путь обязаны совпасть)
  if (!smkOn) return;
  unsigned h = 2166136261u, n = 0, hl = 2166136261u;   /* hl — только текстуры самого уровня (без трафика и слотов миссий) */
  for (int pass = 0; pass < 2; pass++) { Tex *T = pass ? rgTex : tex; unsigned m = pass ? rgNtex : ntex;
    for (unsigned i = 0; T && i < m; i++) if (T[i].pal && toneBlkHas((const unsigned char *)T[i].pal)) { const unsigned char *c = (const unsigned char *)T[i].pal; int lv_ = !pass && i < flatLevelN; for (int k = 0; k < 1024; k++) { h = (h ^ c[k]) * 16777619u; if (lv_) hl = (hl ^ c[k]) * 16777619u; } n++; } }
  char b[112]; snprintf(b, sizeof b, "TONEHASH %s n=%u h=%08x hl=%08x gamma=%d sat=%d t=%.1f\n", who, n, h, hl, toneGammaPct(), toneSatPct(), smkT); smkWrite(b);
}
static void texToonStat(Tex *t, const unsigned char *pal);   // main_levels.h
static void coinSprRetone(void);   // coinspr.h: палитра монеты заново из исходной копии (проверка 18.10)
static void skyFogColor(void);
static void toneApplyAllSync(void) {
  if (palToneOff) return;
  skyHorizonInvalidate();   // level sky CLUTs are reread and recoloured below
  unsigned t0 = sceKernelGetSystemTimeLow(); int ok = 0, miss = 0, err = 0; ToneRd rd = {-1, -1};
  for (int pass = 0; pass < 2; pass++) {
    Tex *T = pass ? rgTex : tex; unsigned n = pass ? rgNtex : ntex;
    for (unsigned i = 0; T && i < n; i++) {
      if (!T[i].pal) continue;
      if (toneToon() && !T[i].tstatOk) { unsigned char o_[1024]; if (toneReadOrig((const unsigned char *)T[i].pal, o_, toneRead, &rd) == 0) texToonStat(&T[i], o_); }   // проверка 18.10
      toneSetStat(toneStatOf(&T[i])); int r = toneRedo((unsigned char *)T[i].pal, toneRead, &rd); toneSetStat(0);
      if (r == 0) { ok++; if (!pass) curbPal(&T[i], (unsigned char *)T[i].pal); sceKernelDcacheWritebackRange(T[i].pal, 1024); } else if (r == -1) miss++; else err++;
    }
  }
  toneRdClose(&rd);
  bound = -2;   // CLUT перезагрузится при следующей привязке
  skyFogColor(); groundCover.ready = 0; fogK = -1.f; coinSprRetone();   // проверка 18.10: как в конце задания тона
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "TONE sat=%d pal=%d miss=%d err=%d ms=%u t=%.1f\n", toneSatPct(), ok, miss, err,
                                     (sceKernelGetSystemTimeLow() - t0) / 1000u, smkT); smkWrite(s); }
  toneHashLog("old");
}
// 16.10 (план финала, Камера п.1): смена тона (gamma, насыщенность) без остановки. Раньше toneApplyAll читал 531 палитру из файлов за один кадр
// (0,85 с на PSP). Теперь: toneApplyAll только ставит задание; каждый кадр toneJobStep читает и тонирует несколько палитр в ОТДЕЛЬНЫЙ буфер
// (бюджет TONE_JOB_US мкс на кадр, не меньше одной), а когда все готовы, одним кадром копирует их на место (≈531 КБ memcpy). Живые палитры до того
// не трогаются: на экране либо старый тон, либо новый, смеси нет. Набор блоков изменился (уровень, трафик, комната) — задание начинается заново.
// Нет памяти под буфер — прежний синхронный путь. «flag tonejobold 1» / -DTONE_JOB_OLD=1 — всегда прежний путь.
#ifndef TONE_JOB_OLD
#define TONE_JOB_OLD 0
#endif
#ifndef TONE_JOB_US
#define TONE_JOB_US 3000
#endif
static int toneJobOld = TONE_JOB_OLD, toneJobBudget = TONE_JOB_US;
static struct { unsigned char **pal; Tex **tx; unsigned char *stage; unsigned n, done, gen, workUs, steps, startT; int active; ToneRd rd; } toneJob = { .rd = { -1, -1 } };   // fd = -1 с самого начала: нулевой дескриптор (stdin) закрывать нельзя (первый toneJobFree закрывал fd 0)
static void toneJobFree(void) {
  toneRdClose(&toneJob.rd);
  free(toneJob.pal); free(toneJob.tx); free(toneJob.stage); memset(&toneJob, 0, sizeof toneJob); toneJob.rd.fd = -1; toneJob.rd.file = -1;
}
static void toneJobStart(void) {
  toneJobFree();
  unsigned n = 0;
  for (int pass = 0; pass < 2; pass++) { Tex *T = pass ? rgTex : tex; unsigned m = pass ? rgNtex : ntex;
    for (unsigned i = 0; T && i < m; i++) if (T[i].pal && toneBlkHas((const unsigned char *)T[i].pal)) n++; }
  if (!n) { skyHorizonInvalidate(); bound = -2; return; }
  toneJob.pal = (unsigned char **)malloc(n * sizeof(unsigned char *)); toneJob.tx = (Tex **)malloc(n * sizeof(Tex *)); toneJob.stage = (unsigned char *)malloc((size_t)n * 1024);
  if (!toneJob.pal || !toneJob.tx || !toneJob.stage) { toneJobFree(); toneApplyAllSync(); return; }   // нет памяти под буфер — как раньше
  unsigned k = 0;
  for (int pass = 0; pass < 2; pass++) { Tex *T = pass ? rgTex : tex; unsigned m = pass ? rgNtex : ntex;
    for (unsigned i = 0; T && i < m; i++) if (T[i].pal && toneBlkHas((const unsigned char *)T[i].pal)) { toneJob.tx[k] = &T[i]; toneJob.pal[k++] = (unsigned char *)T[i].pal; } }
  toneJob.n = k; toneJob.gen = toneBlkGen(); toneJob.active = 1; toneJob.rd.fd = -1; toneJob.rd.file = -1; toneJob.startT = sceKernelGetSystemTimeLow();
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "TONEJOB start n=%u budget=%d t=%.1f\n", k, toneJobBudget, smkT); smkWrite(s); }
}
static void toneApplyAll(void) {
  if (palToneOff) return;
  if (toneJobOld) { toneApplyAllSync(); return; }
  toneJobStart();
}
static void toneJobStep(void) {
  if (!toneJob.active) return;
  if (toneJob.gen != toneBlkGen()) { toneJobStart(); return; }   // набор блоков менялся: pal[] могли освободить
  unsigned t0 = sceKernelGetSystemTimeLow(), now = t0; unsigned did = 0;
  unsigned bud = (uiReady && uiPaused()) ? 4u * (unsigned)toneJobBudget : (unsigned)toneJobBudget;   // в паузе кадр лёгкий: слайдер «Colour» применяется быстрее
  while (toneJob.done < toneJob.n && (!did || (now - t0) < bud)) {
    unsigned char *dst = toneJob.stage + (size_t)toneJob.done * 1024;
    Tex *tx_ = toneJob.tx[toneJob.done]; int rr_;
    if (toneToon() && tx_ && !tx_->tstatOk && (rr_ = toneReadOrig(toneJob.pal[toneJob.done], dst, toneRead, &toneJob.rd)) == 0) {   // проверка 18.10: средний цвет — по исходной палитре, одним чтением
      texToonStat(tx_, dst); toneSetStat(tx_->tstat); tonePal(dst); toneSetStat(0); }
    else { toneSetStat(toneStatOf(tx_)); rr_ = toneRedoTo(toneJob.pal[toneJob.done], dst, toneRead, &toneJob.rd); toneSetStat(0); }
    if (rr_ != 0) memcpy(dst, toneJob.pal[toneJob.done], 1024);   // не прочиталась: остаётся как есть
    else curbPal(tx_, dst);   // №974: угловая деталь бордюра — в тон бордюра
    toneJob.done++; did++; now = sceKernelGetSystemTimeLow();
  }
  toneJob.workUs += now - t0; toneJob.steps++;
  // дескриптор живёт между шагами (открытие файла дороже чтения 1 КБ: по шагу на палитру выходило 12 мс/кадр), но закрывается при любом выходе: commit, restart, toneJobCancel (выгрузка уровня)
  if (toneJob.done < toneJob.n) return;
  // все готовы: одним кадром на место
  unsigned c0 = sceKernelGetSystemTimeLow();
  skyHorizonInvalidate();
  for (unsigned i = 0; i < toneJob.n; i++) { memcpy(toneJob.pal[i], toneJob.stage + (size_t)i * 1024, 1024); sceKernelDcacheWritebackRange(toneJob.pal[i], 1024); }
  bound = -2;   // CLUT перезагрузится при следующей привязке
  skyFogColor(); groundCover.ready = 0; fogK = -1.f; coinSprRetone();   // проверка 18.10: цвета, посчитанные из палитр (дымка и фон кадра, заглушка клеток, подстилка земли, монета), — заново, как у палитр
  unsigned cu = sceKernelGetSystemTimeLow() - c0;
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "TONE sat=%d pal=%u miss=0 err=0 ms=%u t=%.1f\n", toneSatPct(), toneJob.n, (toneJob.workUs + cu) / 1000u, smkT); smkWrite(s);
               snprintf(s, sizeof s, "TONEJOB done n=%u steps=%u work_ms=%u commit_us=%u wall_ms=%u gamma=%d t=%.1f\n", toneJob.n, toneJob.steps, toneJob.workUs / 1000u, cu, (c0 - toneJob.startT) / 1000u, toneGammaPct(), smkT); smkWrite(s); }
  toneHashLog("job");
  toneJobFree();
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "TONEFD opened=%u closed=%u t=%.1f\n", toneFdOpened, toneFdClosed, smkT); smkWrite(s); }   // после закрытия: равны
}
static void toneJobCancel(void) {   // уровень выгружается: буфер 0,5 МБ не держим на время загрузки следующего
  int was = toneJob.active; toneJobFree();
  if (was && smkOn) { char s[96]; snprintf(s, sizeof s, "TONEJOB cancel opened=%u closed=%u t=%.1f\n", toneFdOpened, toneFdClosed, smkT); smkWrite(s); }
}
static int toonModeChanged;   // №754: Cartoon переключили — линии города загрузить/убрать сразу (после skinSetToon, ниже)
static void toonLinesLoad(void);
static int levelToonWanted;   // №679: определение в main_levels.h (предварительное: C допускает повтор static без инициализатора)
// №777: интерфейс панели изображения и мультика (реализует ветка контура и цветов: tone.c, skin.c); здесь — слабые заглушки, чтобы сборка шла и без неё
__attribute__((weak)) void toneSetContrast(int pct) { (void)pct; }                      // 50..200 %, 100 — нейтрально; оба режима (палитры)
__attribute__((weak)) void toneToonTex(int pct) { (void)pct; }                          // 0..100 % родной текстуры в мультике (палитры)
__attribute__((weak)) void skinToonLineParams(int width, int eyes) { (void)width; (void)eyes; }   // толщина контура 0..10, глаза 0 / 1 мягко / 2 полно
// №912: заливка вместо текстур — включается с Cartoon, когда текстуры мира у игрока выключены; опрос раз в кадр (после смены тона статистика текстур готова). Смена — листы клеток и запечённые предметы заново
static void flatPoll(void) {
  int want = toneToon() && !flatOld && !flatTexUser && !toneJob.active && ntex > 0;
  { static int hl_ = -1; if (smkOn && flatDump && !toneJob.active && ntex > 0 && hl_ != gameLevel) { hl_ = gameLevel; toneHashLog("lvl"); } }   /* хэш палитр после загрузки уровня: сверка с итогом прежнего задания тона (TONEHASH job) */
  if (want) { flatNoiseBuild(); gdBuild(); flatTintBuild(); }
  { static unsigned palHookGen_ = ~0u; static int palHookToon_ = -1;   // №974/№979: палитры тонируются при загрузке таблицы текстур, раньше чем читается flat912.bin (там класс угловой детали и асфальта) — один раз на уровень и режим добавляем их тон здесь; задание тона дальше ведёт их само (curbPal в шаге)
    int tn_ = toneToon(); if (flatFromFile && !toneJob.active && ntex > 0 && (palHookGen_ != flatLevelGen || palHookToon_ != tn_)) { palHookGen_ = flatLevelGen; palHookToon_ = tn_;
      unsigned nh_ = 0, nx_ = 0; ToneRd rdx_ = {-1, -1}; if (tn_) for (unsigned i_ = 0; i_ < ntex; i_++) if (tex[i_].pal && (tex[i_].asph || tex[i_].curb == 2 || (zn7On && zn7N && zn7Of((unsigned)i_)))) {
        if (xwalk999On && tex[i_].xdon && !palToneOff) { toneSetStat(toneStatOf(&tex[i_])); if (toneRedo((unsigned char *)tex[i_].pal, toneRead, &rdx_) == 0) nx_++; toneSetStat(0); }   // №999: палитра перехода заново из исходной, со статистикой донора
        curbPal(&tex[i_], (unsigned char *)tex[i_].pal); sceKernelDcacheWritebackRange(tex[i_].pal, 1024); nh_++; }
      toneRdClose(&rdx_); if (smkOn && tn_ && flatFromFile) { char l_[100]; snprintf(l_, sizeof l_, "XWALK999 level=%d on=%d retoned=%u\n", gameLevel, xwalk999On, nx_); smkWrite(l_);
        for (unsigned i_ = 0; i_ < ntex; i_++) if (tex[i_].xdon) { int a_[4], b_[4]; xwalk999Mean(&tex[tex[i_].xdon - 1], b_, 0); xwalk999Mean(&tex[i_], a_, b_); char m_[160]; snprintf(m_, sizeof m_, "XWALK999TEX level=%d tex=%u donor=%u own=%d,%d,%d(%d) don=%d,%d,%d(%d)\n", gameLevel, i_, tex[i_].xdon - 1u, a_[0], a_[1], a_[2], a_[3], b_[0], b_[1], b_[2], b_[3]); smkWrite(m_); } }
      if (nh_) { bound = -2; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "CURBPALLOAD level=%d hooked=%u t=%.1f\n", gameLevel, nh_, smkT); smkWrite(l_);
        unsigned char t_[4] = {0, 0, 0, 0}; int ok_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024 && !ok_; q_++) if (tex[q_].curb == 2) ok_ = curbTarget(&tex[q_], t_);
        snprintf(l_, sizeof l_, "CURB987 level=%d on=%d nat=%d,%d,%d(%d) corner=%d,%d,%d(%d)\n", gameLevel, curb987On, curbNat[0], curbNat[1], curbNat[2], curbNat[3], t_[0], t_[1], t_[2], ok_); smkWrite(l_); } } } }
  { static const void *rescanFor_; static unsigned rescanGen_; int base_ = toneToon() && !flatOld && ntex > 0 && !toneJob.active;   // текстуры загружены, пока заливка была выключена: статистика не считалась — задание тона пересчитает (без перезагрузки уровня)
    if (base_ && (rescanFor_ != (const void *)tex || rescanGen_ != flatLevelGen)) { int miss_ = 0; for (unsigned q_ = 0, qn_ = !flatRescanOld && flatLevelN && flatLevelN < ntex ? flatLevelN : ntex; q_ < qn_; q_++) if (!tex[q_].flatDone && tex[q_].pal) { miss_++; tex[q_].tstatOk = 0; }
      rescanFor_ = tex; rescanGen_ = flatLevelGen; if (miss_) { toneApplyAll(); return; }
      /* заливка уже включена, а уровень сменился: прежде это шло через задание тона (flatOn 0 → 1 с пересборкой); тона, цвета разметки, листы клеток и кэши — заново под текстуры нового уровня */
      if (flatOn && !flatRebuildOld) { unsigned r0_ = sceKernelGetSystemTimeLow(), r1_, r2_, r3_; flatTintKey = ~0u; flatTintBuild(); r1_ = sceKernelGetSystemTimeLow(); marksToneBuild(); marksEpoch++; r2_ = sceKernelGetSystemTimeLow(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); r3_ = sceKernelGetSystemTimeLow();
        if (smkOn) { char l_[128]; snprintf(l_, sizeof l_, "FLATREBUILD gen=%u tint_us=%u marks_us=%u lists_us=%u t=%.1f\n", flatLevelGen, r1_ - r0_, r2_ - r1_, r3_ - r2_, smkT); smkWrite(l_); } } } }
  { static unsigned lastKey_; unsigned key_ = toneKey() ^ ((unsigned)toneToonTexPct() << 8);   // настройки цвета сменились при включённой заливке: цвета в листах клеток (свет без источников, разметка) — заново
    if (want && flatOn && key_ != lastKey_) { lastKey_ = key_; flatTintKey = ~0u; flatTintBuild(); marksToneBuild(); marksEpoch++; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }
    else if (want && !flatOn) { lastKey_ = key_; marksToneBuild(); marksEpoch++; } }
  if (want == flatOn) return;
  flatOn = want; if (want) flatTouched = 1;
  for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_);
  ggcClear(); pbReset(); resetState();
  if (smkOn && flatDump && want) for (unsigned t_ = 0; t_ < ntex && t_ < 1024; t_++) { const Tex *x_ = &tex[t_]; if (x_->flatK || !fdPieces[t_]) continue; char l_[160];
      snprintf(l_, sizeof l_, "FLATX %u %dx%d reason=%d el=%d g=%d tris=%u mean=%d,%d,%d edge=%d\n", t_, x_->w, x_->h, x_->flatR ? x_->flatR : 9, x_->flatE, (x_->tstat[4] & 4) != 0, fdTris[t_], x_->tstat[0], x_->tstat[1], x_->tstat[2], x_->flatEd); smkWrite(l_); }
  if (smkOn && flatDump && want) for (unsigned t_ = 0; t_ < ntex && t_ < 1024; t_++) { const Tex *x_ = &tex[t_]; if (x_->flatK != 0 || !(x_->tstat[4] & 4)) continue;   // №1007: рисунки земли и стен — тон после цветокоррекции мультика
      unsigned char c_[4] = {(unsigned char)(x_->flatC & 255), (unsigned char)((x_->flatC >> 8) & 255), (unsigned char)((x_->flatC >> 16) & 255), 255}; toneColorRGB(c_); char l_[200];
      snprintf(l_, sizeof l_, "FLATD %u %dx%d reason=%d g=%d asph=%d curb=%d tris=%u dom=%d,%d,%d toned=%d,%d,%d\n", t_, x_->w, x_->h, x_->flatR, (x_->tstat[4] & 4) != 0, x_->asph, x_->curb, fdTris[t_], x_->flatC & 255, (x_->flatC >> 8) & 255, (x_->flatC >> 16) & 255, c_[0], c_[1], c_[2]); smkWrite(l_); }
  if (smkOn && flatDump && want) { flatTintBuild(); for (unsigned t_ = 0; t_ < ntex && t_ < 1024; t_++) { const Tex *x_ = &tex[t_]; if (!x_->flatK) continue; char l_[200];
      snprintf(l_, sizeof l_, "FLATT %u %dx%d el=%d g=%d mean=%d,%d,%d dom=%d,%d,%d share=%d tint=%d,%d,%d tris=%u edge=%d\n", t_, x_->w, x_->h, x_->flatE, (x_->tstat[4] & 4) != 0, x_->tstat[0], x_->tstat[1], x_->tstat[2],
               x_->flatC & 255, (x_->flatC >> 8) & 255, (x_->flatC >> 16) & 255, x_->flatS, flatTint[t_] & 255, (flatTint[t_] >> 8) & 255, (flatTint[t_] >> 16) & 255, fdTris[t_], x_->flatEd); smkWrite(l_); } }
  if (smkOn) { char s_[160]; flatTintBuild(); unsigned ks_ = 0, ke_ = 0, kt_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) { ks_ += tex[q_].flatK != 0; ke_ += tex[q_].flatE == FLAT_EL_OK; kt_ += tex[q_].tstatOk; }
    snprintf(s_, sizeof s_, "FLAT on=%d flat=%u ground=%u noise_mean=%.3f ntex=%u stat_ok=%u scan_k=%u elig=%u toon=%d scan_us=%u scan_n=%u marks=%d cells=%u verts=%u res=%u small=%u drops=%u t=%.1f\n", flatOn, flatKinds[1], flatKinds[2], noiseMean, ntex, kt_, ks_, ke_, toneToon(), flatScanUs, flatScanN, marksOk, marksCellsLoaded, marksVertsLoaded, marksVertsResident, marksSmallLoads, marksSmallDrops, smkT); smkWrite(s_); }
}
static void cfgApply(void) {
  if (cfg.colour != toneSatPct()) { toneSetSat(cfg.colour); cfg.colour = toneSatPct(); toneApplyAll(); }
  { static int ctSet = -1, txSet = -1, lwSet = -1, eySet = -1; int pal = 0;   // №777: только при смене (первый вызов — значения файла)
    if (cfg.contrast < 50 || cfg.contrast > 200) cfg.contrast = 100;
    if (cfg.toonTex < 0 || cfg.toonTex > 100) cfg.toonTex = 35;
    if (cfg.lineWidth < 0 || cfg.lineWidth > 10) cfg.lineWidth = 5;
    if (cfg.eyeLines < 0 || cfg.eyeLines > 2) cfg.eyeLines = 1;
    if (cfg.ink < 0 || cfg.ink > 2) cfg.ink = INK951_DEF;
    skinInkStyle = cfg.ink;   // №951: перо контура читают рисование героев, машин, линий и пропов каждый кадр
    if (cfg.contrast != ctSet) { ctSet = cfg.contrast; toneSetContrast(cfg.contrast); pal = 1; }
    if (cfg.toonTex != txSet) { txSet = cfg.toonTex; toneToonTex(cfg.toonTex); pal |= toneToon(); }
    if (cfg.lineWidth != lwSet || cfg.eyeLines != eySet) { lwSet = cfg.lineWidth; eySet = cfg.eyeLines; skinToonLineParams(cfg.lineWidth, cfg.eyeLines); }
    if (pal) toneApplyAll(); }
  { static int ddSet = -1; if (cfg.drawDist < 0 || cfg.drawDist > 2) cfg.drawDist = 1;   // №795: Draw Distance — пол регулятора дальности (200 м — как было, без подмены)
    if (cfg.drawDist != ddSet) { static const float dd_[3] = {150.f, 200.f, 250.f}; ddSet = cfg.drawDist; ddFloorOver = cfg.drawDist == 1 && DRAWDEF1042_OLD ? 0.f : dd_[cfg.drawDist]; } }   // №1042б: 200 тоже явный выбор (потолок города не ниже выбранного, FAR1042)
  if (cfg.curbTint < 0 || cfg.curbTint > 1) cfg.curbTint = 0; curbWant1064 = !curb1064Old && !cfg.curbTint;   // №1064: строка Curbs (Original — родная текстура бордюра, Tinted — как в TEST76)
  if (cfg.carShadow < 0 || cfg.carShadow > 1) cfg.carShadow = 0; carShadow1003On = carShadow1003Old || cfg.carShadow;   // №1003: тени машин — строка Car Shadows
  if (cfg.camHeight < 0 || cfg.camHeight > 4) cfg.camHeight = 2;   // №996: страница CAMERA — Height и Distance (0..4, 2 — как было)
  if (cfg.camDist < 0 || cfg.camDist > 4) cfg.camDist = 2;
  { float h_ = camCfg996On ? camCfgH996[cfg.camHeight] : 1.f, d_ = camCfg996On ? camCfgD996[cfg.camDist] : 1.f;
    if (h_ != homerCamUserH || d_ != homerCamUserD) { homerCamUserH = h_; homerCamUserD = d_; camCfgSnap996 = 1; } }
  flatTexUser = cfg.texturesOn != 0;   // №912: Textures On — родные текстуры (заливку включает flatPoll при Cartoon и Off)
  if ((cfg.toon != 0) != toneToon()) { toneSetToon(cfg.toon != 0); if (cfg.toon) toneToonPrep(); toneApplyAll(); toonModeChanged = 1; }   // №754: пресет 90-х на палитрах — вместе с Cartoon, фоновым заданием
  sndUserVolumes(cfg.music / 1000.f, cfg.effects / 1000.f, cfg.vehicle / 1000.f, cfg.voice / 1000.f);
  if (uiReady) uiHudShow(UI_T_RADAR, cfg.radar != 0);
  tutSetUser(cfg.tutorial != 0);
  { static int tsSet = -1; if (!TREESH_OLD && cfg.treeShadow != tsSet) { tsSet = cfg.treeShadow; treeShOn = cfg.treeShadow != 0; } }   // проверка 18.10: только при смене — откаты (treeshold, flag treesh) не перетираются. Роб 17.10: тени деревьев вкл/выкл в настройках (main_render.h TREESH_SKIP), сразу, без перезагрузки уровня
  { static int toonSet = -1; if (cfg.toon != toonSet) { if (toonSet != -1 || cfg.toon) levelToonWanted = cfg.toon != 0; if (!cfg.toon && toonSet > 0) skinToonFlush(); toonSet = cfg.toon; skinSetToon(cfg.toon != 0); } }   /* проверка 18.10: выключили — кэш оболочек (до 2 МБ) в кучу */   // №679: мир-мультик (level_toon.shr, линии) берётся со следующей загрузки уровня; служебный toonworld на первой установке не затирается   // №428: только при смене значения — служебный файл toon / команда smoke не затираются
  if (toonModeChanged) { toonModeChanged = 0; if (haveCol) toonLinesLoad(); }   // №754: линии города — сразу с режимом (toonLinesLoad сам убирает при выключенном)
}
// №851 (разбор туров 19.10): тур сдвигает настройки на шаг (Draw Distance, насыщенность, линия) и они сохраняются — каждый прогон шёл с другими
// (TEST34 — дальность 200 м, TEST36–39 — 250 м, повтор TEST39 — 150 м), скорость туров не сравнить. cfgDef — значения по умолчанию сборки
// (до чтения settings.bin); служебный «flag cfgdefaults 1» в сценарии возвращает их
static Settings cfgDef; static int cfgDefOk;
static void cfgLoad(void) {
  if (!cfgDefOk) { cfgDef = cfg; cfgDefOk = 1; }
  char fn[256]; sibling(fn, "settings.bin"); Settings t = cfg;   // чего нет в файле (запись ver 1 короче) — по умолчанию
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f >= 0) { int n = sceIoRead(f, &t, sizeof t), n1 = (int)offsetof(Settings, tutorial);
    if (n >= n1 && !memcmp(t.magic, "SHST", 4) && t.ver >= 1 && t.ver <= 14) {
      if (t.ver == 7 && n >= (int)sizeof t) t.oldTv = t.oldTv ? 5 : 0;   // №830: ver 7 — Off/On, теперь сила 0..10
      if (t.ver == 1 || n < (int)sizeof t) { Settings d = cfg; memcpy((char *)&d, &t, t.ver == 1 ? (size_t)n1 : (size_t)n); t = d; }
      if (!DRAWDEF1042_OLD && t.ver < 13) t.drawDist = 2;   // №1042б: миграция один раз — Draw Distance 250 (выбор игрока потом сохраняется как есть)
      if (t.ver < 14) t.curbTint = 0;   // №1064: старые сохранения получают «Original» (родной бордюр)
      t.ver = DRAWDEF1042_OLD ? 12 : 14; if (t.curbTint < 0 || t.curbTint > 1) t.curbTint = 0; if (t.carShadow < 0 || t.carShadow > 1) t.carShadow = 0; t.movies |= 1u; if (t.camHeight < 0 || t.camHeight > 4) t.camHeight = 2; if (t.camDist < 0 || t.camDist > 4) t.camDist = 2; if (t.ink < 0 || t.ink > 2) t.ink = INK951_DEF; if (t.oldTv < 0 || t.oldTv > 10) t.oldTv = 5; if (t.texturesOn < 0 || t.texturesOn > 1) t.texturesOn = 0; cfg = t; }   // ver 1 — без tutorial / tutSeen / movies; ver 2 — без colour; ver 3 — без invertCam; ver 4 — без contrast … toonTex; ver 5 — без drawDist; ver 6 — без oldTv (по умолчанию)
    sceIoClose(f); }
  cfgApply();
}
static void cfgSave(void) {
  if (saveBlocked) { static int n_; if (smkOn && n_++ < 5) smkWrite("SAVEBLOCK cfg\n"); return; }   // автотур: настройки игрока не трогаем
  char fn[256]; sibling(fn, "settings.bin");
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if (f >= 0) { sceIoWrite(f, &cfg, sizeof cfg); sceIoClose(f); }
}
// ---- сохранения (viewer/savemenu.h): текущая игра -> запись и обратно (миссии — msnStart по svPending)
static SaveGame lvKeep;   // облако 05.10 (карьера): прогресс миссий всех уровней этой игры (ext записи; новая игра — пусто)
static void saveCapture(SaveGame *s) {
  svClear(s); MsnProgress p;
  svCapture(s, mvProgress(&p) ? &p : NULL, haveColl ? &coll : NULL, haveCoins ? &coins : NULL);
#if !TICKET_CONTENT_C3_OLD
  ticketSave(&ticketView.st,s);
#endif
  s->level = (uint8_t)gameLevel;   // уровень записи — тот, что в памяти (и без пакета карточек)
  s->music = cfg.music; s->effects = cfg.effects; s->vehicle = cfg.vehicle; s->voice = cfg.voice; s->radar = cfg.radar;
  s->ext[SV_EXT_CAM] = (uint8_t)(SV_EXT_CAM_SET | (cfg.invertCam ? SV_EXT_CAM_INV : 0));   // правка 362: инверсия L/R камеры (savegame.h)
  s->skin = (uint8_t)(skinWorn > 0 ? skinWorn : 0);   // правка 107: надетый костюм
  if (!gameLvProgOld) {   // облако 05.10 (карьера): прогресс миссий всех уровней (savegame.h SV_EXT_LV) — прежние из lvKeep, текущий из миссий
    svLvKeep(s, &lvKeep);
    if (s->msnLevel == gameLevel) svLvPut(s, gameLevel, s->msnCompleted, mvMissionRows());
  }
  if (!spOnceOld) {   // правка 334: разовые предметы — все уровни игры, текущий снят с уровня в памяти (spOnceOld — прежнее: не пишутся)
    memcpy(s->once, onceKeep, sizeof s->once);
    if (haveSP && gameLevel >= 1 && gameLevel < SV_LEVELS) svOnceCapture(&s->once[gameLevel], &sprops);
  }
}
// правка 421: маска открытых трасс бонус-игры (viewer/bonus_unlock.h) — из карточек игры в памяти или из записи. Каждая смена —
// строка BONUSOPEN (гейт tests/bonus/unlock.sh) и пункт BONUS GAME главного меню (серый, пока ни одной трассы)
static void bonusOpenSet(unsigned m, const char *src) {
  int ch = m != bonusOpen; bonusOpen = m;
  if (uiReady && mainNew >= 0 && mainMini >= 0) uiMenuEnable(UI_MENU_MAIN, mainMini, bonusAvailable());
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "BONUSOPEN mask=%02x src=%s changed=%d menu=%d t=%.1f\n", bonusOpenMask(), src, ch,
                                     mainMini >= 0 && bonusAvailable(), smkT); smkWrite(s); }
}
static void bonusOpenFromColl(const char *src) {   // карточки всех уровней игры в памяти (Collect.st.cards)
  if (!haveColl) return;
  unsigned char c[COLL_LEVELS] = {0}; for (int L = 1; L < COLL_LEVELS; L++) c[L] = (unsigned char)collectCards(&coll, L);
  bonusOpenSet(bonusUnlockMask(c), src);
}
static void bonusOpenFromSave(const SaveGame *g, const char *src) {   // запись (NULL — нет): её карточки; DEV SAVE — все уровни собраны
  unsigned char c[SV_LEVELS] = {0};
  if (g) { memcpy(c, g->cards, sizeof c); if (svmIsDev(g) && !devCollOld && !BONUS_UNLOCK_OLD) bonusDevCards(c); }
  bonusOpenSet(bonusUnlockMask(c), src);
}
static void saveApply(const SaveGame *s) {   // после newGame (карточки/осы/гэги, кошелёк, монеты мира — savegame.c; настройки)
  svApply(s, haveColl ? &coll : NULL, haveCoins ? &coins : NULL);
#if !TICKET_CONTENT_C3_OLD
  ticketRestore(&ticketView.st,s); if(haveColl)coll.st.ticketFlags=ticketView.st.flags;
#endif
  // правка Роба 350 (05.10: «DEV SAVE — полностью пройденный уровень, не только миссии: карточки открыты, всё собрано — проверить,
  // что подгружается»): при загрузке DEV (после svApply — запись DEV их не несёт) на уровне собраны все карточки, осы и гэги этого уровня (их число — из данных уровня,
  // collect.h); бонусы/гонки считаются пройденными в DEV resume; transient dev_bonus_replay сохраняет доступ к повтору у NPC.
  // Прежнее (только миссии и покупки) — flag devcollold 1
  if (svmIsDev(s) && haveColl && !devCollOld && coll.level > 0) {
    svDevCollect(&coll);
#if !BONUS_UNLOCK_OLD   // правка 421 (Роб 09.10: «в этот сейв, который полностью всё прохождение, добавь — чтобы я затестил все треки»):
    { unsigned char dc[COLL_LEVELS] = {0}; bonusDevCards(dc);   // DEV — карточки всех 7 уровней собраны -> открыты все 7 трасс бонус-игры
      for (int L = 1; L < COLL_LEVELS; L++) if (L != coll.level && dc[L])
        collectSetProgress(&coll, L, collectCards(&coll, L) | dc[L], collectWasps(&coll, L), collectGags(&coll, L)); }
#endif
    int gt = 0; collectCount(&coll, coll.level, COLL_GAGS, &gt);
    char d_[128]; snprintf(d_, sizeof d_, "SVDEV collect level=%d cards=%x/%u wasps=%u gags=%u\n", coll.level, collectCards(&coll, coll.level), coll.ncard, coll.nwasp, (unsigned)gt); smkWrite(d_);
  }
  if (!svmIsDev(s))   // правка 335: DEV SAVE настройки звука и радара не трогает (в записи — по умолчанию)
    { cfg.music = s->music; cfg.effects = s->effects; cfg.vehicle = s->vehicle; cfg.voice = s->voice; cfg.radar = s->radar;
      if (s->ext[SV_EXT_CAM] & SV_EXT_CAM_SET) cfg.invertCam = (s->ext[SV_EXT_CAM] & SV_EXT_CAM_INV) != 0;   // правка 362 (запись до 05.10 — как было)
      cfgApply(); cfgSave(); }
  if (s->skin && s->skin != skinWorn && wardrobeFile(s->skin)) homerWear(s->skin);   // правка 107: костюм, в котором сохранился
  lvKeep = *s;   // облако 05.10 (карьера): прогресс всех уровней загруженной записи (до 05.10 — svLvGet выводит его из уровня записи)
  if (!spOnceOld) {   // правка 334: разбитые разовые предметы уровня — разбитыми, монеты из них не выпадают
    memcpy(onceKeep, s->once, sizeof onceKeep);
    int r = haveSP && gameLevel >= 1 && gameLevel < SV_LEVELS ? svOnceApply(&s->once[gameLevel], &sprops) : 0;
    if (smkOn) { char t[128]; unsigned n = 0; if (haveSP) svOnceSig(&sprops, &n);
      snprintf(t, sizeof t, "SVONCE op=apply level=%d saved=%d/%u restored=%d level_n=%u t=%.1f\n", gameLevel,
               gameLevel >= 1 && gameLevel < SV_LEVELS ? svOnceCount(&s->once[gameLevel]) : 0,
               gameLevel >= 1 && gameLevel < SV_LEVELS ? s->once[gameLevel].n : 0u, r, n, smkT); smkWrite(t); }
  }
}
static void saveLog(const char *src) {   // smoke.txt: прогресс игры (сверка гейтом tools/save_gate.sh)
  if (!smkOn) return;
  SaveGame g; saveCapture(&g); char p[256], s[320]; svProgressText(&g, p, sizeof p);
  snprintf(s, sizeof s, "SVPROG src=%s %s t=%.1f\n", src, p, smkT); smkWrite(s);
}
static int savePauseOpen(int askQuit) { SaveGame g; saveCapture(&g); return svmOpen(SVM_SAVE, &g, 0, askQuit); }
unsigned svmDialogFrame(float dt, int start) {   // правка 309 (savemenu.h): окно утилиты сохранения PSP на экране — сценарий смоука идёт
  // (press/wait/shot; goto и флаги в окне не действуют), нажатия сценария, начатые при окне, — в контроллер эмулятора строкой TWPAD
  // (tools/twins/ppsspp_headless_pad.patch); ✕ выбора слота, ещё не отпущенный к появлению окна, окну не передаётся
  static unsigned last, n0;
  if (start) { n0 = smkPressN; last = 0; }
  SmokeCmd sc; while (smokeNext(dt, &sc)) {}
  unsigned p = smkPressN != n0 ? smokePad() : 0;
  if (smkOn && p != last) { char s[48]; snprintf(s, sizeof s, "TWPAD %x t=%.1f\n", p, smkT); smkWrite(s); }
  last = p; return p;
}
static void saveService(const char *f) {   // служебные флаги гейта: svmark — отметить прогресс, svlog — строка, svnowrite — без записи
  if (!strcmp(f, "svonceold")) { spOnceOld = 1; if (smkOn) { char t[48]; snprintf(t, sizeof t, "SVONCEOLD on=1 t=%.1f\n", smkT); smkWrite(t); } }   // правка 334: прежнее (красный)
  else if (!strcmp(f, "svonce")) {   // правка 334: разовые предметы уровня в журнал — сколько, сколько не целы, номера не целых
    char t[200]; int k = 0, nb = 0; unsigned n = 0;
    if (haveSP) svOnceSig(&sprops, &n);
    k = snprintf(t, sizeof t, "SVONCE op=now level=%d n=%u", gameLevel, n);
    for (unsigned i = 0; haveSP && i < sprops.ninst; i++) if (spOnce(&sprops, i) && sprops.run[i].state) nb++;
    k += snprintf(t + k, sizeof t - k, " broken=%d coins=%d at=", nb, haveCoins ? coins.collected : -1);
    for (unsigned i = 0; haveSP && i < sprops.ninst && k < (int)sizeof t - 24; i++)
      if (spOnce(&sprops, i) && sprops.run[i].state) k += snprintf(t + k, sizeof t - k, "%u:%d,", i, sprops.run[i].state);
    snprintf(t + k, sizeof t - k, " t=%.1f\n", smkT); if (smkOn) smkWrite(t);
  }
  else   if (!strcmp(f, "svnowrite")) svmNoWrite(1);
  else if (!strcmp(f, "svdialog")) svmDialog(1);       // правка 309: окно утилиты сохранения PSP и в служебном запуске
  else if (!strcmp(f, "svquiet")) svmDialog(0);        // правка 309: прежний тихий путь без окна (красный контроль tests/savegame/dialog/run.py)
  else if (!strcmp(f, "svpickempty")) svmPickEmpty(1);   // тур финала L7 для железа: занятый слот (сейв Роба) не перезаписывается
  else if (!strcmp(f, "svdlgfail")) svmDialogFail(1);  // правка 309: окно отвергает все варианты — запись тихим путём
  else if (!strcmp(f, "svlog")) saveLog("now");
  else if (!strcmp(f, "svmark")) {   // m0 пройдена, карточка 1, +7 монет, монета мира 0 собрана, музыка 300
    mvServiceMark(0);
    if (haveColl) collectSetProgress(&coll, coll.level, collectCards(&coll, coll.level) | 1u, collectWasps(&coll, coll.level), collectGags(&coll, coll.level));
    if (haveCoins) { coins.collected += 7; if (coins.nworld) coins.c[0].active = 0; }
    cfg.music = 300; cfgApply(); saveLog("mark");
  }
}
static int rowOf(int scr, const char *label) {   // строка экрана по подписи из данных (без регистра, по началу)
  for (int r = 0; r < uiScrRows(scr); r++) { const char *l = uiScrRowLabel(scr, r); int k = 0;
    while (label[k] && l[k] && toupper((unsigned char)l[k]) == toupper((unsigned char)label[k])) k++;
    if (!label[k]) return r; }
  return -1;
}
static int cfgColourRow;   // строка Colour: номер значения 0..TONE_SAT_N-1 (в cfg.colour — сотые)
static int optLinkScr = -1, optLinkRow = -1;   // №783: строка-переход (PICTURE → CARTOON) открытой страницы; -1 — нет
static int cfgContrastRow, cfgTexRow;   // №777: номер варианта строк Contrast (0.50..2.00 шагом 0.10) и Texture Detail (0..100 % шагом 5)
static int *cfgSlot(int scr, int row, int sound) {   // значение настройки за строкой экрана Sound / Settings
  static const char *snd[4] = {"MUSIC", "EFFECTS", "VEHICLE", "VOICE"};
  int *v[4] = {&cfg.music, &cfg.effects, &cfg.vehicle, &cfg.voice};
  if (sound) { for (int i = 0; i < 4; i++) if (row == rowOf(scr, snd[i])) return v[i]; return 0; }
  if (row == rowOf(scr, "TUTORIAL")) return &cfg.tutorial;   // строка Tutorial (Settings паузы): подсказки Барта вкл/выкл (29.09)
  if (row == rowOf(scr, "INVERT")) return &cfg.invertCam;   // правка 362: строка Invert Camera Control (данные PauseSettings.pag): Off / On
  if (row == rowOf(scr, "COLOUR") || row == rowOf(scr, "SATURATION")) { cfgColourRow = toneSatIndex(cfg.colour); return &cfgColourRow; }   // правка 144; №753: подпись Saturation
  if (scr == optLinkScr && row == optLinkRow) return 0;   // №783: строка-переход «Cartoon» на PICTURE — не значение (✕ открывает страницу CARTOON)
  if (SHAR_TOON_AVAILABLE && row == rowOf(scr, "CARTOON")) return &cfg.toon;   // №428: строка Cartoon (место серой Jump Cameras): Off / On
  if (row == rowOf(scr, "TEXTURES")) return &cfg.texturesOn;   // №912: строка Textures на PICTURE (Off — заливка вместо текстур, On — родные); раньше «TEXTURE» (Texture Detail), совпадение по началу подписи
  if (row == rowOf(scr, "TREE")) return &cfg.treeShadow;
  if (row == rowOf(scr, "CURB")) return &cfg.curbTint;   // №1064: Curbs (Original / Tinted)
  if (row == rowOf(scr, "CAR SHADOW")) return &cfg.carShadow;   // №1003: Car Shadows (Off / On)   // Роб 17.10: строка «Tree Shadows» (Off / On) — подпись в пакете интерфейса ставит терминал игры
  if (row == rowOf(scr, "CONTRAST")) { cfgContrastRow = (cfg.contrast - 50) / 10; return &cfgContrastRow; }   // №777: подстраница PICTURE
  if (row == rowOf(scr, "LINE")) return &cfg.lineWidth;                                                   // №777: подстраница CARTOON — Line Width
  if (row == rowOf(scr, "INK")) return &cfg.ink;                                                          // №951: Ink — Indigo / Black / Tone
  if (row == rowOf(scr, "HEIGHT")) return &cfg.camHeight;                                                // №996: страница CAMERA — Height (Low … High)
  if (row == rowOf(scr, "DISTANCE")) return &cfg.camDist;                                                // №996: Distance (Near … Far)
  if (row == rowOf(scr, "EYE")) return &cfg.eyeLines;                                                     // Eye Lines: Off / Soft / Full
  if (row == rowOf(scr, "TEXTURE")) { cfgTexRow = cfg.toonTex / 5; return &cfgTexRow; }                   // Texture Detail
  if (row == rowOf(scr, "DRAW")) return &cfg.drawDist;                                                     // №795: Draw Distance 150 / 200 / 250 м
  if (row == rowOf(scr, "OLD")) return &cfg.oldTv;                                                        // №800/№830: Old TV — Off, 10 %..100 % (страница CARTOON)
  return row == rowOf(scr, "RADAR") ? &cfg.radar : 0;
}
// значение строки сменилось (ползунок / вариант): в cfg и сразу в игру. Строка Colour — номер значения насыщенности (tone.h);
// строка вариантов в ui.c идёт по кругу, а насыщенность — шкала: 1.60 → 1.00 одним нажатием не перескакивает (правило порта)
// №777: шкалы Contrast, Line Width, Texture Detail — так же, без перескока по кругу. SETTINGS_SCALE_OLD=1 — новые строки идут по кругу
#ifndef SETTINGS_SCALE_OLD
#define SETTINGS_SCALE_OLD 0
#endif
static void cfgRowChanged(int scr, int row, int sound) {
  int *v = cfgSlot(scr, row, sound); if (!v) return;
  int nv = uiScrGetRow(row);
  if (v == &cfgColourRow || (!SETTINGS_SCALE_OLD && (v == &cfgContrastRow || v == &cfgTexRow || v == &cfg.lineWidth))) { int old = *v;
    if (nv - old > 1 || old - nv > 1) { nv = old; uiScrSetRow(row, nv); } }
  if (v == &cfgColourRow) cfg.colour = toneSatFromIndex(nv);
  if (v == &cfgContrastRow) cfg.contrast = 50 + 10 * nv;
  if (v == &cfgTexRow) cfg.toonTex = 5 * nv;
  *v = nv; cfgApply();
}
// экран настроек: значения строк из cfg; выключенные — то, чего в порте ещё нет (серые, курсор пропускает)
static int optEnter(int scr, int sound) {   // -> первая включённая строка (на ней курсор)
  int first = -1;
  for (int r = 0; r < uiScrRows(scr); r++) { int *v = cfgSlot(scr, r, sound); uiScrSetRow(r, v ? *v : 0); uiScrEnable(scr, r, v != 0);
                                             if (v && first < 0) first = r; }
  return first < 0 ? 0 : first;
}
static void optLog(const char *pack, int scr, int sel, int r) {
  if (!smkOn) return;
  char s[192]; int n = snprintf(s, sizeof s, "OPT pack=%s scr=%d sel=%d r=%d rows=", pack, scr, sel, r);
  for (int k = 0; k < uiScrRows(scr) && n < 150; k++) n += snprintf(s + n, sizeof s - n, "%s%d%s", k ? "," : "", uiScrGetRow(k), uiScrEnabled(scr, k) ? "" : "x");
  snprintf(s + n, sizeof s - n, " vol=%d,%d,%d,%d radar=%d tut=%d colour=%d inv=%d toon=%d tree=%d ct=%d lw=%d eye=%d tex=%d ink=%d camh=%d camd=%d carsh=%d curb=%d t=%.1f\n", cfg.music, cfg.effects, cfg.vehicle, cfg.voice,
           cfg.radar, cfg.tutorial, cfg.colour, cfg.invertCam, cfg.toon, cfg.treeShadow, cfg.contrast, cfg.lineWidth, cfg.eyeLines, cfg.toonTex, cfg.ink, cfg.camHeight, cfg.camDist, cfg.carShadow, cfg.curbTint, smkT); smkWrite(s);
  if (smkOn) { char d_[48]; snprintf(d_, sizeof d_, "OPTDD dd=%d floor=%.0f t=%.1f\n", cfg.drawDist, ddFloorOver, smkT); smkWrite(d_); }
}
// меню паузы: 0 pauseoptions, 1 pausesound, 2 pausesettings (pmenu.shr открыт на время); -1 — закрыто
// №777 (Роб 18.10): пакет с подстраницами (7 экранов, tools/menu_export.py SUBPAGES) — 2 раздел SETTINGS (пункты CAMERA / GAME / PICTURE / CARTOON),
// 3..6 строки подстраниц; △ с подстраницы — в раздел, из раздела — в OPTIONS. Пакет из 3 экранов — прежняя одна страница SETTINGS
enum { PM_OPTIONS = 0, PM_SOUND = 1, PM_SETTINGS = 2, PM_CAMERA = 3, PM_GAME = 4, PM_PICTURE = 5, PM_CARTOON = 6, PM_N = 7 };
static int pmPages(void) { return uiScrScreens() >= PM_N; }   // раздел SETTINGS с подстраницами
// №783 (Роб 18.10 после TEST34): «в OPTIONS сразу SOUND, CAMERA, GAME, PICTURE; внутри PICTURE — CARTOON; внутри CARTOON под вкл/выкл — его настройки».
// Пакет с пунктом PICTURE на OPTIONS (tools/menu_export.py MENU_FLAT_OLD) — раздел SETTINGS не открывается; на PICTURE строка «Cartoon» без значения —
// переход на страницу CARTOON, △ оттуда — обратно на PICTURE (курсор на переходе). MENU_FLAT_OLD=1 у экспорта — прежний пакет, движок ведёт себя как TEST34
#ifndef MENU_FLAT_ENGINE_OLD
#define MENU_FLAT_ENGINE_OLD 0
#endif
#ifndef PICTURE_CARTOON_FIRST_OLD
#define PICTURE_CARTOON_FIRST_OLD 0   // №891: 1 — курсор на PICTURE входит на первую строку-настройку (пакет с Cartoon последней)
#endif
static int pmFlat(void) { return !MENU_FLAT_ENGINE_OLD && pmPages() && rowOf(PM_OPTIONS, "PICTURE") >= 0; }
static void pmEnterPage(int scr, int *sel) {   // страница значений: строки из cfg; на PICTURE — строка-переход «Cartoon» доступна
  optLinkScr = optLinkRow = -1;
  if (pmFlat() && scr == PM_PICTURE) { int lr = rowOf(PM_PICTURE, "CARTOON"); if (lr >= 0 && uiScrRowKind(PM_PICTURE, lr, NULL) == UI_ROW_ITEM) { optLinkScr = PM_PICTURE; optLinkRow = lr; } }
  *sel = optEnter(scr, scr == PM_SOUND);
  if (optLinkRow >= 0) { uiScrEnable(PM_PICTURE, optLinkRow, 1);
    if (!PICTURE_CARTOON_FIRST_OLD && optLinkRow < *sel) *sel = optLinkRow; }   // №891: Cartoon — первая строка PICTURE, курсор входит на неё
}
static int pmValues(int scr) { return scr == PM_SOUND || scr >= PM_CAMERA || (scr == PM_SETTINGS && !pmPages()); }   // экран строк значений
// LEVEL PROGRESS паузы (LevelStats.scr) и конец уровня (LevelEnd.scr), 29.09: строки миссий, машин, одежды и всего гэгов — от
// миссий (viewer/msn_view.c mvLevelStats), карточки, осы и найденные гэги — прогресс сборов уровня (viewer/collect.h collectCount)
static int lsOpen, lsEndLogged; static float lsEndT;   // пауза: открыт LEVEL PROGRESS; конец уровня: строка записана, сколько стоит
static void levelStatsFill(UiLevelStats *s) {
  mvLevelStats(s);
  if (!haveColl) return;
  int t = 0;
  s->done[UI_LS_CARDS] = collectCount(&coll, gameLevel, COLL_CARDS, &t); s->total[UI_LS_CARDS] = t;
  s->done[UI_LS_WASPS] = collectCount(&coll, gameLevel, COLL_WASPS, &t); s->total[UI_LS_WASPS] = t;
  s->done[UI_LS_GAGS] = collectCount(&coll, gameLevel, COLL_GAGS, &t); if (!s->total[UI_LS_GAGS]) s->total[UI_LS_GAGS] = t;
}
static void levelStatsLog(const char *what) {
  if (!smkOn) return;
  UiLevelStats ls; levelStatsFill(&ls); char s[256]; int n = snprintf(s, sizeof s, "LEVELSTATS %s pct=%d", what, uiLevelStatsPct(&ls));
  static const char *nm[UI_LS_COUNT] = {"gags", "wasps", "cars", "skins", "cards", "races", "bonus", "story"};
  for (int k = 0; k < UI_LS_COUNT && n < (int)sizeof s - 24; k++) n += snprintf(s + n, sizeof s - n, " %s=%d/%d", nm[k], ls.done[k], ls.total[k]);
  snprintf(s + n, sizeof s - n, " t=%.1f\n", smkT); smkWrite(s);
}
// VIEW CARDS паузы (PauseViewCards.scr, 29.09): пакет pextra.shr на время показа (tools/menu_export.py), карточки уровня — маска
// collectCards; ◄ ► ▲ ▼ — выбор (сетка 4 + 3, как места Card0..6 PauseViewCards.pag), X на собранной — крупно с названием и
// описанием (библия CARD_*), X / △ — обратно к сетке, △ / ○ на сетке — в паузу. В поставке: "pextra.shr"
static int vcOpen, vcSel, vcZoom;
// идея Роба 85 (29.09): у крупной карточки — компактный QR на страницу её эпизода в вики сообщества Симпсонов
// (tools/cardqr_export.py -> cardqr-l<N>.bin: CQR1, по карточке u8 n + модули битами). Модуль 2 px, тихая зона 2 модуля, белое
// поле; место — правый нижний угол колонки текста (описание уже на ширину кода, tools/ui_export.py VC_QR_W)
#define VC_QR_MOD 2
#define VC_QR_RX 434   // правый край колонки текста карточки (build/ui/layout_pextra.json CardDesc)
#define VC_QR_BY 222   // низ колонки
static unsigned char vcQr[2048]; static int vcQrLen;
static void vcQrLoad(void) {
  char fn[256], nm[32]; snprintf(nm, sizeof nm, "cardqr-l%d.bin", gameLevel); sibling(fn, nm); vcQrLen = 0;
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int n = sceIoRead(f, vcQr, sizeof vcQr); sceIoClose(f);
  vcQrLen = n > 5 && !memcmp(vcQr, "CQR1", 4) ? n : 0;
}
static void vcQrDraw(int card) {
  if (!vcQrLen) return;
  int p = 5, cnt = vcQr[4];
  for (int k = 0; k < cnt && p < vcQrLen; k++) {
    int n = vcQr[p++], bytes = (n * n + 7) / 8;
    if (k != card) { p += bytes; continue; }
    if (!n || p + bytes > vcQrLen) return;
    const unsigned char *b = vcQr + p; int side = (n + 4) * VC_QR_MOD, x0 = VC_QR_RX - side, y0 = VC_QR_BY - side;
    typedef struct { unsigned c; short x, y, z, pad; } QV;
    int dark = 0; for (int i = 0; i < n * n; i++) dark += (b[i >> 3] >> (7 - (i & 7))) & 1;
    QV *v = (QV *)sceGuGetMemory((size_t)(2 + 2 * dark) * sizeof(QV)); int m = 0;
    v[m].c = 0xFFFFFFFFu; v[m].x = (short)x0; v[m].y = (short)y0; v[m].z = 0; m++;
    v[m].c = 0xFFFFFFFFu; v[m].x = (short)(x0 + side); v[m].y = (short)(y0 + side); v[m].z = 0; m++;
    for (int i = 0; i < n * n; i++) if ((b[i >> 3] >> (7 - (i & 7))) & 1) {
      int mx = x0 + (i % n + 2) * VC_QR_MOD, my = y0 + (i / n + 2) * VC_QR_MOD;
      v[m].c = 0xFF000000u; v[m].x = (short)mx; v[m].y = (short)my; v[m].z = 0; m++;
      v[m].c = 0xFF000000u; v[m].x = (short)(mx + VC_QR_MOD); v[m].y = (short)(my + VC_QR_MOD); v[m].z = 0; m++;
    }
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND); sceGuDepthMask(GU_TRUE);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, m, 0, v);
    resetState(); return;
  }
}
static void vcLog(const char *ev) { if (smkOn) { char s[96]; snprintf(s, sizeof s, "VIEWCARDS %s sel=%d zoom=%d got=%u t=%.1f\n", ev, vcSel, vcZoom,
                                                          haveColl ? collectCards(&coll, gameLevel) : 0u, smkT); smkWrite(s); } }
static void vcShow(void) { uiScrShow(UI_PX_VIEWCARDS, UI_T_VC_GRID, !vcZoom); uiScrShow(UI_PX_VIEWCARDS, UI_T_VC_ZOOM, vcZoom); }
static int vcStart(void) {
  char fn[256]; pausePackFile(fn, "pextra.shr");
  if (uiScrOpen(fn)) return 0;
  unsigned got = haveColl ? collectCards(&coll, gameLevel) : 0u;
  for (int k = 0; k < 7; k++) uiHudSet(UI_F_VC0 + k, (int)(got >> k & 1u));
  vcOpen = 1; vcSel = 0; vcZoom = 0; vcShow(); vcQrLoad(); vcLog("open");
  return 1;
}
static void vcInput(unsigned hit) {
  int was = vcSel;
  if (vcZoom) {
    if (hit & (PSP_CTRL_CROSS | PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { vcZoom = 0; vcShow(); uiPopHookFire(UI_EV_BACK); vcLog("grid"); }
    return;
  }
  if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | PSP_CTRL_START)) { SCR_CLOSE(); vcOpen = 0; uiPopHookFire(UI_EV_BACK); vcLog("close"); return; }
  if (hit & PSP_CTRL_LEFT) vcSel = (vcSel + 6) % 7;
  if (hit & PSP_CTRL_RIGHT) vcSel = (vcSel + 1) % 7;
  if (hit & PSP_CTRL_DOWN) vcSel = vcSel < 4 ? 4 + (vcSel < 2 ? vcSel : vcSel - 1) : vcSel;   // верхний ряд 0..3, нижний 4..6 (сдвинут на полместа)
  if (hit & PSP_CTRL_UP) vcSel = vcSel >= 4 ? vcSel - 3 : vcSel;
  if (vcSel != was) { uiPopHookFire(UI_EV_SCROLL); vcLog("sel"); }
  if (hit & PSP_CTRL_CROSS) {
    if (uiHudGet(UI_F_VC0 + vcSel)) { vcZoom = 1; vcShow(); uiPopHookFire(UI_EV_ACCEPT); vcLog("zoom"); }
    else uiPopHookFire(UI_EV_LOCKED);   // несобранная — как затемнённый пункт
  }
}
// пункт ДОБИВКИ 26 (запись PS2 ps2_access_pause, L1 m0sd.0 «TALK TO MARGE»): MISSION SELECT паузы серый и курсор его обходит, когда на
// MissionSelect.scr нечего выбрать (menu_access.h menuAccessMissionSelect). Прежнее: -DPAUSE_MS_DIM_OLD=1 — пункт открыт всегда
// Режим списка — как при открытии (ниже, act == UI_P_MISSION_SELECT): экран MissionSelect.scr грузится из pextra.shr, когда уровень 1 или
// не служебное uimsselold, и пакет есть; иначе msn_view.c mvMissionSelect открывает запасной список (все пройденные и текущая)
static void pauseMsAccess(void) {
#if !PAUSE_MS_DIM_OLD
  if (!uiReady || !mvActive()) return;
  static int rowsLv = -1, rowsOld = -1, rows = 0;
  if (rowsLv != gameLevel || rowsOld != mvMissionSelectOld()) {   // пакет на уровень не меняется — stat один раз на уровень
    char fn[256]; SceIoStat st; pausePackFile(fn, "pextra.shr");
    rowsLv = gameLevel; rowsOld = mvMissionSelectOld(); rows = (gameLevel == 1 || !rowsOld) && sceIoGetstat(fn, &st) >= 0;
  }
  int on = mvMissionSelectAvail(rows);
  uiMenuEnable(UI_MENU_PAUSE, UI_P_MISSION_SELECT, on);
  static int logged = -1;
  if (smkOn && on != logged) { char s_[64]; snprintf(s_, sizeof s_, "PAUSEMS avail=%d rows=%d t=%.1f\n", on, rows, smkT); smkWrite(s_); }
  logged = on;
#endif
}
static int pauseSub = -1, pauseSubSel[PM_N];
static void pauseSubOpen(void) {
  char fn[256]; pausePackFile(fn, "pmenu.shr");
  if (uiScrOpen(fn)) return;
  pauseSub = PM_OPTIONS; memset(pauseSubSel, 0, sizeof pauseSubSel);
  for (int r = 0; r < uiScrRows(PM_OPTIONS); r++) uiScrEnable(PM_OPTIONS, r, 1);
  optLog("pmenu", pauseSub, 0, -1);
}
static void pauseSubInput(unsigned hit) {   // пауза стоит; △/○ — на экран выше, из опций — обратно в паузу
  // пункт ДОБИВКИ 26 (запись PS2 ps2_access_settings_unlock: START из SETTINGS, кадр 340 -> игра, кадр 400): START на OPTIONS /
  // SOUND / SETTINGS закрывает паузу целиком (подсказка страниц «Press START button to Continue»). Прежнее: -DPAUSE_SUB_START_OLD=1
  if (menuAccessSubStart(hit, PSP_CTRL_START)) {
    if (pauseSub != PM_OPTIONS) cfgSave();
    if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PAUSESUB start scr=%d t=%.1f\n", pauseSub, smkT); smkWrite(s_); }
    SCR_CLOSE(); pauseSub = -1; uiPause(0); return;
  }
  int *sel = &pauseSubSel[pauseSub], r = uiScrMenuInput(pauseSub, sel, hit);
  if (r == UI_SCR_CHANGED && pmValues(pauseSub)) cfgRowChanged(pauseSub, *sel, pauseSub == PM_SOUND);
  if (r == UI_SCR_BACK) {
    if (pauseSub == PM_OPTIONS) { optLog("pmenu", -1, 0, r); SCR_CLOSE(); pauseSub = -1; return; }
    if (pmValues(pauseSub)) cfgSave();
    if (pmFlat()) { int was_ = pauseSub; pauseSub = was_ == PM_CARTOON ? PM_PICTURE : PM_OPTIONS;   // №783: CARTOON → PICTURE, прочие → OPTIONS
      if (pauseSub == PM_PICTURE) { pmEnterPage(PM_PICTURE, &pauseSubSel[PM_PICTURE]); if (optLinkRow >= 0) pauseSubSel[PM_PICTURE] = optLinkRow; } else optLinkScr = optLinkRow = -1; }
    else pauseSub = pmPages() && pauseSub >= PM_CAMERA ? PM_SETTINGS : PM_OPTIONS;   // №777: с подстраницы — в раздел SETTINGS (курсор на её пункте)
  } else if (pmFlat() && pauseSub == PM_OPTIONS && r >= 0) {   // №783: пункт OPTIONS -> его страница
    static const char *op[4] = {"SOUND", "CAMERA", "GAME", "PICTURE"}; static const int ps[4] = {PM_SOUND, PM_CAMERA, PM_GAME, PM_PICTURE};
    for (int k = 0; k < 4; k++) if (r == rowOf(PM_OPTIONS, op[k])) { pauseSub = ps[k]; pmEnterPage(pauseSub, &pauseSubSel[pauseSub]); break; }
  } else if (pmFlat() && pauseSub == PM_PICTURE && r >= 0 && r == optLinkRow) {   // №783: «Cartoon» на PICTURE -> страница CARTOON
    pauseSub = PM_CARTOON; pmEnterPage(PM_CARTOON, &pauseSubSel[PM_CARTOON]);
  } else if (pauseSub == PM_OPTIONS && r >= 0) {
    if (r == rowOf(PM_OPTIONS, "SOUND")) { pauseSub = PM_SOUND; pauseSubSel[pauseSub] = optEnter(PM_SOUND, 1); }
    else if (r == rowOf(PM_OPTIONS, "SETTINGS")) {
      pauseSub = PM_SETTINGS;
      if (pmPages()) for (int k = 0; k < uiScrRows(PM_SETTINGS); k++) uiScrEnable(PM_SETTINGS, k, 1);   // №777: пункты раздела — все доступны
      else pauseSubSel[pauseSub] = optEnter(PM_SETTINGS, 0);
    }
  } else if (pmPages() && pauseSub == PM_SETTINGS && r >= 0) {   // №777: пункт раздела -> его подстраница (по подписи пункта)
    static const char *pg[4] = {"CAMERA", "GAME", "PICTURE", "CARTOON"};
    for (int k = 0; k < 4; k++) if (r == rowOf(PM_SETTINGS, pg[k])) { pauseSub = PM_CAMERA + k; pauseSubSel[pauseSub] = optEnter(pauseSub, 0); break; }
  }
  if (hit) optLog("pmenu", pauseSub, pauseSubSel[pauseSub], r);
}
// главное меню → OPTIONS (menu.shr: 0 options, 1 sound, 2 viewcredits, 3 viewmovies) над 3D-гостиной; △ — назад в главное меню.
// VIEW CREDITS (ViewCredits.scr, 29.09): титры CREDITS_00..31 библии едут вверх из-под нижнего края, CREDITS_SPEED — правило
// порта (в данных и записи PS2 скорости нет); музыка — событие композиции «Credits» (l1_music.rms: Land_of_Choc_Main), тюнер
// «Credits»; кончились или △ — назад в OPTIONS. VIEW MOVIES (ViewMovies.scr): 9 строк — ролики MOVIES[]; открыт — показан в игре
// (cfg.movies) и лежит рядом с EBOOT, остальные серые; X — ролик во весь экран (как ролики целей), START — пропустить
#define CREDITS_SPEED (35.f * 272.f / 480.f)   // px PSP в секунду: 35 px Scrooby/с (≈ 3,4 с на строку-блок в 15 px)
enum { OPT_MAIN = 0, OPT_SOUND = 1, OPT_CREDITS = 2, OPT_MOVIES = 3 };
static unsigned moviesAvail(void) {   // бит k — строку MOVIES[k] можно выбрать
  unsigned m = 0; char fn[256], rel[48]; SceIoStat st;
  for (int k = 0; k < 9; k++) { snprintf(rel, sizeof rel, "movies/%s.smv", MOVIES[k]); sibling(fn, rel);
    if (((cfg.movies | 1u | ((!TICKET_CONTENT_C3_OLD&&TICKET_MOVIE_AVAILABLE)?1u<<8:0)) >> k & 1u) && sceIoGetstat(fn, &st) >= 0) m |= 1u << k; }
  return m;
}
static void creditsLog(const char *what, float scroll, const UiCredits *ci) {
  if (!smkOn) return;
  char s[200]; snprintf(s, sizeof s, "CREDITS %s scroll=%.1f blocks=%d lines=%d first=%d top=%d bottom=%d x=%d,%d end=%.0f t=%.1f\n", what, scroll,
                        ci ? ci->blocks : 0, ci ? ci->lines : 0, ci ? ci->first : -1, ci ? ci->top : 0, ci ? ci->bottom : 0,
                        ci ? ci->x0 : 0, ci ? ci->x1 : 0, ci ? ci->end : 0.f, smkT); smkWrite(s);
}
// №862 (Роб 22.10: «в главном меню, гостиной Симпсонов, при пройденной игре должен быть доступен скрапбук, в котором карточки»):
// SCRAP BOOK — экран VIEW CARDS паузы (PauseViewCards.scr из pextra.shr уровня) над гостиной; L / R — уровень 1..7, ◄ ► ▲ ▼ X — как в
// паузе (vcInput), △ / ○ на сетке — назад в меню. Карточки: загруженная на этом экране запись, иначе игра этой сессии, иначе самая
// свежая запись (как трассы бонус-игры, правка 421); DEV SAVE — все. На время альбома gameLevel = уровень страницы (levelFile, cardqr)
#ifndef SCRAP862_OLD
#define SCRAP862_OLD 0
#endif
#define SCRAP_LB_X 24    // подпись уровня альбома: левый край и верх строки (строка подсказок ACCEPT / BACK — низ экрана)
#define SCRAP_LB_Y 248
static int scrapAllTest;   // служебно «flag scrapon 1» (гейт альбома без записи): все карточки собраны
static void scrapCards(unsigned char *c, const SaveGame *loaded) {
  memset(c, 0, SV_LEVELS); SaveGame g;
  if (scrapAllTest) { memset(c, 0x7f, SV_LEVELS); c[0] = 0; return; }
  const SaveGame *src = loaded ? loaded : (bonusSession && haveColl) ? NULL : (svmLatest(&g) ? &g : NULL);
  if (!loaded && bonusSession && haveColl) { for (int L = 1; L < SV_LEVELS && L < COLL_LEVELS; L++) c[L] = (unsigned char)collectCards(&coll, L); return; }
  if (src) { memcpy(c, src->cards, SV_LEVELS); if (svmIsDev(src) && !devCollOld) bonusDevCards(c); }
}
static int scrapPage(int L, const unsigned char *c) {
  char fn[256]; SceIoStat st; gameLevel = L; levelFile(fn, "pextra.shr");
  if (sceIoGetstat(fn, &st) < 0 || uiScrOpen(fn)) return 0;
  unsigned got = c[L]; for (int k = 0; k < 7; k++) uiHudSet(UI_F_VC0 + k, (int)(got >> k & 1u));
  vcOpen = 1; vcSel = 0; vcZoom = 0; vcShow(); vcQrLoad();
  if (smkOn) { char s[80]; snprintf(s, sizeof s, "SCRAPBOOK page=%d got=%02x t=%.1f\n", L, got, smkT); smkWrite(s); }
  return 1;
}
static void scrapRun(int mainSel, void **fbp, const unsigned char *c) {
  int keep = gameLevel, L = 0;
  for (int n = 1; n <= 7 && !L; n++) if (scrapPage(n, c)) L = n;
  if (!L) { gameLevel = keep; return; }
  int font = uiFontFind("boulder_16");
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow();
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    if ((hit & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) && !vcZoom) {   // другой уровень: пакет его карточек
      int d = (hit & PSP_CTRL_RTRIGGER) ? 1 : -1, n = L; SCR_CLOSE(); vcOpen = 0;
      for (int k = 0; k < 7; k++) { n = (n + d + 6) % 7 + 1; if (scrapPage(n, c)) { L = n; break; } }
      if (!vcOpen) break;
      uiPopHookFire(UI_EV_SCROLL); hit &= ~(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER);
    }
    if (hit) vcInput(hit);
    if (!vcOpen) break;   // △ / ○ на сетке: vcInput закрыл пакет
    unsigned budgetT0 = sceKernelGetSystemTimeLow(), budgetIo0 = smkIoUs;
    sceGuStart(GU_DIRECT, list); resetState();
    sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    feDraw(mainSel); resetState();
    uiScrDrawMenu(UI_PX_VIEWCARDS, vcSel, 0.f); if (vcZoom) vcQrDraw(vcSel);
    if (!vcZoom) {   // уровень страницы — слева внизу, в строке подсказок ACCEPT / BACK (верх занят заголовком VIEW CARDS)
      char lb[16]; snprintf(lb, sizeof lb, "LEVEL %d", L);
      int x = SCRAP_LB_X + uiText(font, SCRAP_LB_X, SCRAP_LB_Y, 0xffffffffu, lb); uiText(font, x + 12, SCRAP_LB_Y, 0xffa8a8a8u, "L / R"); }
    resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); menuBudget(budgetT0, budgetIo0, "scrapbook");
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  if (vcOpen) { SCR_CLOSE(); vcOpen = 0; }
  gameLevel = keep;
}
static void optionsRun(int mainSel, void **fbp) {
  char fn[256]; sibling(fn, "menu.shr");
  if (uiScrOpen(fn)) return;
  int scr = OPT_MAIN, rSound = rowOf(0, "SOUND"), sel[4] = {rSound < 0 ? 0 : rSound, 0, 0, 0};
  int extra = uiScrScreens() > OPT_MOVIES && uiScrRows(OPT_MOVIES) == 9;   // пакет 29.09: экраны титров и роликов
  int rCredits = extra ? rowOf(0, "VIEW CREDITS") : -1, rMovies = extra ? rowOf(0, "VIEW MOVIES") : -1;
  for (int r = 0; r < uiScrRows(0); r++) uiScrEnable(0, r, r == rSound || r == rCredits || r == rMovies);
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "OPTROWS sound=%d credits=%d movies=%d seen=0x%x t=%.1f\n", rSound, rCredits, rMovies, cfg.movies, smkT); smkWrite(s); }
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last; char logged[24] = "";
  float credT = 0.f; int credShot = 0, prevMode = SND_MODE_NORMAL; UiCredits ci; memset(&ci, 0, sizeof ci);
  optLog("menu", scr, 0, -1);
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);   // правка 202: автоповтор ↑/↓ в OPTIONS
    SmokeCmd sc; while (smokeNext(dt, &sc)) if (sc.op == 2 && !strcmp(sc.name, "moviesseen")) cfg.movies = (unsigned)sc.a | 1u;   // служебно (гейт)
    if (scr == OPT_CREDITS) {
      int done = !credShot && ci.blocks && ci.end <= 0.f;   // низ последней строки ушёл за верх экрана
      if (!smkShot[0]) credT += dt;   // снимок окна: титры стоят, пока ppsspp_run.sh не снимет
      if (uiScrBackInput(hit) || done) {
        creditsLog(done ? "end" : "back", credT * CREDITS_SPEED, &ci);
        sndMode(prevMode); sndMusic(SND_MUS_FRONTEND); scr = OPT_MAIN; hit = 0;
      }
    } else {
      int r = uiScrMenuInput(scr, &sel[scr], hit);
      if (r == UI_SCR_CHANGED && scr == OPT_SOUND) { int *v = cfgSlot(1, sel[1], 1); if (v) { *v = uiScrGetRow(sel[1]); cfgApply(); } }
      if (r == UI_SCR_BACK) { if (scr == OPT_MAIN) { optLog("menu", -1, 0, r); break; } if (scr == OPT_SOUND) cfgSave(); scr = OPT_MAIN; }
      else if (scr == OPT_MAIN && r >= 0 && r == rSound) { scr = OPT_SOUND; sel[1] = optEnter(1, 1); }
      else if (scr == OPT_MAIN && r >= 0 && r == rCredits) {   // титры: музыка «Credits», тюнер Credits (global.spt: только музыка и голос)
        scr = OPT_CREDITS; credT = 0.f; memset(&ci, 0, sizeof ci); prevMode = sndModeCur(); sndMode(SND_MODE_CREDITS);
        int id = audioTrack("credits"); if (id >= 0) audioMusic(id, .5f);
        if (smkOn) { char s[96]; snprintf(s, sizeof s, "CREDITS on music=%s t=%.1f\n", id >= 0 ? "credits" : "-", smkT); smkWrite(s); }
      }
      else if (scr == OPT_MAIN && r >= 0 && r == rMovies) {
        scr = OPT_MOVIES; unsigned av = moviesAvail(); sel[3] = 0;
        for (int k = 0; k < 9; k++) uiScrEnable(OPT_MOVIES, k, av >> k & 1u);
        if (smkOn) { char s[80]; snprintf(s, sizeof s, "MOVIES open avail=0x%x t=%.1f\n", av, smkT); smkWrite(s); }
      }
      else if (scr == OPT_MOVIES && r >= 0 && r < 9) {   // ролик во весь экран; потом снова список
        if (smkOn) { char s[80]; snprintf(s, sizeof s, "MOVIES play row=%d name=%s t=%.1f\n", r, MOVIES[r], smkT); smkWrite(s); }
        char rmv[24]; snprintf(rmv, sizeof rmv, "%s.rmv", MOVIES[r]); msnMovie(rmv, fbp);
        sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = sceKernelGetSystemTimeLow();
      }
      if (hit) optLog("menu", scr, sel[scr], r);
    }
    if (smkShot[0] && strcmp(logged, smkShot)) { optLog("menu", scr, sel[scr], -1); if (scr == OPT_CREDITS) creditsLog(smkShot, credT * CREDITS_SPEED, &ci);
                                                 strcpy(logged, smkShot); }
    credShot = smkShot[0] != 0;
    unsigned budgetT0=sceKernelGetSystemTimeLow(),budgetIo0=smkIoUs;
    sceGuStart(GU_DIRECT, list); resetState();
    sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    feDraw(mainSel); resetState();
    if (scr == OPT_CREDITS) { int was = ci.blocks; uiScrDrawCredits(OPT_CREDITS, credT * CREDITS_SPEED, &ci); if (!was) creditsLog("start", 0.f, &ci); }
    else { uiScrMenuFlags(2); uiScrDrawMenu(scr, sel[scr], (t - t0) * 1e-6f); }   // 413: Options/Sound/Controller/ViewMovies — CGuiMenu флаги 2 (SLES)
    resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); menuBudget(budgetT0,budgetIo0,"options");
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  SCR_CLOSE();
}
// №860: титры конца игры — тем же путём, что VIEW CREDITS в OPTIONS (menu.shr, экран OPT_CREDITS, музыка «Credits»), в menuRun до музыки
// меню. Экран ending.shr (CreditsPostFMV, Канг и Кодос) в эмуляторе вставал на 8–10 кадре: главный поток больше не получал процессор
// (и поверх уровня, и в меню); VIEW CREDITS тем же эмулятором идёт до конца. ENDING_SAVE860_OLD=1 — прежнее: ending.shr прямо из игры
static void creditsRun(void **fbp) {
  char fn[256]; sibling(fn, "menu.shr");
  if (uiScrOpen(fn)) return;
  if (uiScrScreens() <= OPT_MOVIES) { SCR_CLOSE(); return; }   // пакет без экрана титров (до 29.09)
  int prevMode = sndModeCur(); sndMode(SND_MODE_CREDITS);
  int id = audioTrack("credits"); if (id >= 0) audioMusic(id, .5f);
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "CREDITS on music=%s src=gameend t=%.1f\n", id >= 0 ? "credits" : "-", smkT); smkWrite(s); }
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow();
  float credT = 0.f; UiCredits ci; memset(&ci, 0, sizeof ci);
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    int done = ci.blocks && ci.end <= 0.f;   // низ последней строки ушёл за верх экрана
    if (!smkShot[0]) credT += dt;
    if (uiScrBackInput(hit) || done) { creditsLog(done ? "end" : "back", credT * CREDITS_SPEED, &ci); break; }
    sceGuStart(GU_DIRECT, list); resetState();
    sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    { int was = ci.blocks; uiScrDrawCredits(OPT_CREDITS, credT * CREDITS_SPEED, &ci); if (!was) creditsLog("start", 0.f, &ci); }
    resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0);
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  sndMode(prevMode); SCR_CLOSE();
}
