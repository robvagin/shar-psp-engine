// №1105: SCRAP BOOK главного меню целиком (подключается после main_save.h). Экраны оригинала из frontend.p3d (scrap.shr, tools/menu_export.py --pack scrap):
//   0 ScrapBook      — OPEN BOOK / GAME STATS (наклейка Красти при 100 %)
//   1 ScrapBookContents — планка уровня (L / R листают 7 уровней по кругу), разделы: миссии, костюмы, машины, карточки; у каждого «сделано / всего» и картинка уровня
//   2 ScrapBookStats — Game Stats: девять строк «сделано / всего» и процент игры с одной цифрой после запятой
//   3.. галереи миссий, костюмов, машин, карточек (см. ниже)
// Данные собранного — из записи (viewer/scrapdata.h): загруженная на этом экране запись, иначе игра этой сессии, иначе самая свежая запись на карте. Размер записи не менялся.
// Раскладка 16:9, якоря и шрифты — правила tools/ui_export.py (как у всех экранов меню); переходы между экранами — №1117 (ниже).
// SCRAPBOOK1105_OLD=1 — прежний альбом (только сетка карточек, scrapRun).
#ifndef MAIN_SCRAP_H
#define MAIN_SCRAP_H
#include "scrapdata.h"

enum { SB_BOOK = 0, SB_CONTENTS = 1, SB_STATS = 2 };

// Память (вопрос дирижёра 02.11): альбом и галереи открываются ТОЛЬКО из главного меню (menuRun, пункт SCRAP BOOK; из паузы и MISSION SELECT их нет). Куча меню может быть тесной
// (уровень после выхода из игры мог остаться в памяти, наибольший кусок ~0,66 МБ): пакеты читаются кусками по 256 КБ (ступень №1051, uiPakSegMax1051), границы только на началах разделов
// и картинок. Не хватило памяти — uiScrOpen отдаёт -3, всё выделенное освобождено (pakFreeAll), утечки нет; галерея тогда не открывается (звук «закрыто», альбом остаётся), а если не открылся
// сам альбом — прежняя сетка карточек (scrapRun). SCRAPMEM1105_OLD=1 — пакеты цельным куском, как у прочих меню
#ifndef SCRAPMEM1105_OLD
#define SCRAPMEM1105_OLD 0
#endif
static int scrapOpenPack(const char *fn) {
  if (!SCRAPMEM1105_OLD) uiPakSegMax1051(256u << 10);
  int r = uiScrOpen(fn);
  if (!SCRAPMEM1105_OLD) uiPakSegMax1051(0);
  if (r && smkOn) { char s[160]; snprintf(s, sizeof s, "SCRAPBOOK1105 nomem pack=%.60s rc=%d t=%.1f\n", fn, r, smkT); smkWrite(s); }
  return r;
}

// №1117 (Роб: «в альбоме нет плавных переходов между страницами»): переходы как у оригинала, движение наше (изинг как в паузе и OPTIONS, №1087).
//   Оглавление и статистика: вход из обложки — камера гостиной по кадрам 810–830 оригинала, выход назад — 830–850 (камера почти стоит: обложка дышит на пару пикселей);
//   экран: старый гаснет 0,20 с, пусто 0,10 с, новый проявляется 0,30 с (плавное замедление в обе стороны, без перелёта). Между оглавлением и галереями камера не летит (как у оригинала): те же гаснет 0,20 / проявляется
//   0,25 с. Открытие и закрытие альбома — проявляется / гаснет. Просмотр в галерее: картинка летит из своего слота в место просмотра 250 мс (масштаб вокруг точки: центр картинки идёт по прямой, размер от слота до просмотра),
//   сетка гаснет, подпись проявляется. Ввод на время перехода глохнет. SCRAPANIM1117_OLD=1 — мгновенные переходы, как в №1105.
#ifndef SCRAPANIM1117_OLD
#define SCRAPANIM1117_OLD 0
#endif
#define SCRAP_OUT_S .20f
#define SCRAP_GAP_S .10f
#define SCRAP_IN_S .30f
#define SCRAP_FLY_S .25f
static float scrapEase(float x) { x = x < 0.f ? 0.f : x > 1.f ? 1.f : x; return x < .5f ? 4.f * x * x * x : 1.f - powf(-2.f * x + 2.f, 3.f) * .5f; }   // плавно в обе стороны (кубическая)
// один кадр альбома: мир (гостиная), экран scr с прозрачностью al (0 — только мир)
static void scrapPresent(int mainSel, int scr, int selv, float t, float al, void **fbp, const char *tag) {
  unsigned budgetT0 = sceKernelGetSystemTimeLow(), budgetIo0 = smkIoUs;
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  feDraw(mainSel); resetState();
  if (al > .003f) { uiSubFade(al); uiScrMenuFlags(2); uiScrDrawMenu(scr, selv, t); uiSubFade(1.f); }
  resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuFinish(); sceGuSync(0, 0); menuBudget(budgetT0, budgetIo0, tag);
  sceDisplayWaitVblankStart(); *fbp = swapBuf();
}
// экран гаснет перед закрытием пакета (или выходом из альбома): SCRAP_OUT_S, мир остаётся
static void scrapFadeOut(int mainSel, int scr, int selv, float t, void **fbp) {
  if (SCRAPANIM1117_OLD) return;
  unsigned last = sceKernelGetSystemTimeLow(); float te = 0.f;
  while (te < SCRAP_OUT_S) {
    unsigned n = sceKernelGetSystemTimeLow(); float dt = (n - last) * 1e-6f; last = n; if (dt > .05f) dt = .05f; te += dt;
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    scrapPresent(mainSel, scr, selv, t + te, 1.f - scrapEase(te / SCRAP_OUT_S), fbp, "scrapfade");
  }
}
// полёт картинки просмотра (пиксели экрана PSP, из tools/menu_export.py: слоты и место просмотра; костюм поднят на 40 px — №1121): kind 0 миссии, 1 костюмы, 2 машины
static void scrapFlyCall(int kind, int sel, float e) {
  float sx, sy, sw, vx, vy, vw;
  if (kind == 0) { sx = 108.f + 68.f * (sel % 4) + 30.f; sy = 73.f + 68.f * (sel / 4) + 23.5f; sw = 60.f; vx = 161.f + 74.5f; vy = 68.f + 58.5f; vw = 149.f; }
  else if (kind == 1) { static const float X[3] = {110.f, 201.f, 291.f}; sx = X[sel % 3] + 39.5f; sy = 51.f + 76.5f; sw = 79.f; vx = 200.f + 50.5f; vy = 33.f + 98.f; vw = 101.f; }
  else { static const float X[3] = {90.f, 189.f, 288.f}; sx = X[sel % 3] + 51.f; sy = 63.f + 68.f * (sel / 3) + 39.5f; sw = 102.f; vx = 200.f + 101.5f; vy = 79.f + 75.f; vw = 203.f; }
  float s0 = sw / vw, px = (sx - s0 * vx) / (1.f - s0), py = (sy - s0 * vy) / (1.f - s0);   // центр картинки при s = s0 — в центре слота
  uiScrFly(1, UI_F_ROW0 + 8, s0 + (1.f - s0) * e, px, py, 1.f - e, e);
}
static void scrapSource(SaveGame *g, const SaveGame *loaded) {
  svClear(g); SaveGame t;
  if (loaded) *g = *loaded;
  else if (bonusSession && haveColl) saveCapture(g);
  else if (svmLatest(&t)) *g = t;
  if (scrapAllTest) { for (int L = 1; L < SV_LEVELS; L++) g->cards[L] = 0x7f; }
  else if (svmIsDev(g) && !devCollOld) bonusDevCards(g->cards);
}
static void scrapUiLevel(const ScrapData *d, int L) {   // поля экрана «Оглавление» для уровня L
  int c[4][2]; scrapContents(d, L, c);
  static const int fld[4] = {UI_F_STAT0 + 14, UI_F_STAT0 + 6, UI_F_STAT0 + 4, UI_F_STAT0 + 8};   // миссии (STAT_SM), костюмы (SKINS), машины (CARS), карточки (CARDS)
  for (int k = 0; k < 4; k++) { uiHudSet(fld[k], c[k][0]); uiHudSet(fld[k] + 1, c[k][1]); }
  uiHudSet(UI_F_LEVEL, L - 1);
}
static void scrapUiStats(const ScrapData *d) {          // поля экрана Game Stats
  int st[9][2], pct; scrapStats(d, st, &pct);
  static const int fld[8] = {UI_F_STAT0 + 14, UI_F_STAT0 + 12, UI_F_STAT0 + 10, UI_F_STAT0 + 8, UI_F_STAT0 + 6, UI_F_STAT0 + 4, UI_F_STAT0 + 2, UI_F_STAT0 + 0};   // сюжет, бонус, гонки, карточки, костюмы, машины, осы, гэги
  for (int k = 0; k < 8; k++) { uiHudSet(fld[k], st[k][0]); uiHudSet(fld[k] + 1, st[k][1]); }
  uiScrSetRow(10, st[8][0]); uiScrSetRow(11, st[8][1]);   // ролики: пара ROW10 / ROW11
  uiHudSet(UI_F_STAT_PCT, pct);
}
// навигация сетки разделов (3 в верхнем ряду + карточки под средним): ◄ ► по верхнему ряду, ▼ — к карточкам, ▲ — от карточек к среднему
static int scrapGridMove(int sel, unsigned hit) {
  if (sel == 3) { if (hit & PSP_CTRL_UP) return 1; return sel; }
  if (hit & PSP_CTRL_RIGHT) return sel < 2 ? sel + 1 : sel;
  if (hit & PSP_CTRL_LEFT) return sel > 0 ? sel - 1 : sel;
  if (hit & PSP_CTRL_DOWN) return 3;
  return sel;
}

// ---- галереи (этап 2): пакет уровня lN/scrapgal.shr (tools/menu_export.py --pack scrapgal; экраны 0 миссии, 1 костюмы, 2 машины), на время галереи пакет альбома закрыт ----
static SaveGame scrapStartSave; static int scrapStartRow = -1;   // запрос «начать эту миссию» из галереи: запись-перенос и строка MISSION SELECT (0..6); читает main_levels.h
static int scrapAskStart(int mainSel, void **fbp) {   // «Would you like to load this mission?» Yes / No (курсор на No): 1 — Yes. Пакет галереи на время вопроса закрыт (вопрос берёт fsave.shr)
  if (!svmOn() || svmAskOpen(SVM_ASK_STARTMSN)) return 1;
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last; int r;
  do {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    r = svmInput(hit);
    sceGuStart(GU_DIRECT, list); resetState();
    sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    feDraw(mainSel); resetState(); svmDraw((t - t0) * 1e-6f); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
  } while (r == SVM_R_NONE);
  if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "SCRAPBOOK1105 startask answer=%s t=%.1f\n", r == SVM_R_YES ? "yes" : "no", smkT); smkWrite(s_); }
  return r == SVM_R_YES;
}
static int scrapGalleryOpen(int kind, int L, const ScrapData *d, int open[8]) {   // сколько слотов в галерее; open[i] — слот открыт
  int n = kind == 0 ? 8 : kind == 1 ? 3 : 6, ix[8];
  if (kind == 0) for (int i = 0; i < 8; i++) open[i] = scrapMissionSlot(d, L, i);
  else { scrapRewSlots(L, kind == 2, ix); for (int i = 0; i < n; i++) open[i] = scrapRewOwnedAt(d, L, ix[i]); }
  return n;
}
static int scrapGalMove(int sel, unsigned hit, int n, int cols, const int open[8]) {   // сетка слотов; курсор встаёт только на открытые
  int dr = (hit & PSP_CTRL_DOWN) ? 1 : (hit & PSP_CTRL_UP) ? -1 : 0, dc = (hit & PSP_CTRL_RIGHT) ? 1 : (hit & PSP_CTRL_LEFT) ? -1 : 0;
  if (!dr && !dc) return sel;
  int r = sel / cols, c = sel % cols;
  for (int k = 1; k < 8; k++) { int rr = r + dr * k, cc = c + dc * k; if (rr < 0 || cc < 0 || cc >= cols || rr * cols + cc >= n) break; if (open[rr * cols + cc]) return rr * cols + cc; }
  return sel;
}
// -> 0 назад в оглавление; 1 запрошен запуск миссии (scrapStartSave / scrapStartRow); -1 пакета галереи нет
static int scrapGallery(int kind, int L, const ScrapData *d, const SaveGame *g, int mainSel, void **fbp) {
  char fn[256]; SceIoStat st; int keep = gameLevel; gameLevel = L; levelFile(fn, "scrapgal.shr"); gameLevel = keep;
  if (sceIoGetstat(fn, &st) < 0 || scrapOpenPack(fn)) return -1;
  if (uiScrScreens() < 3) { SCR_CLOSE(); return -1; }
  int open[8] = {0}, n = scrapGalleryOpen(kind, L, d, open), cols = kind == 0 ? 4 : 3, sel = 0, view = 0, any = 0;
  for (int i = 0; i < n; i++) { any |= open[i]; uiScrSetRow(i, open[i]); }
  for (int i = 0; i < n; i++) if (open[i]) { sel = i; break; }
  uiHudSet(UI_F_LEVEL, L - 1); uiHudSet(UI_F_MS_LARROW, 0); uiHudSet(UI_F_MS_RARROW, 0);
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "SCRAPBOOK1105 gallery kind=%d level=%d open=%d%d%d%d%d%d%d%d t=%.1f\n", kind, L, open[0], open[1], open[2], open[3], open[4], open[5], open[6], open[7], smkT); smkWrite(s); }
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last; int result = 0;
  float vt = 0.f, fin = SCRAPANIM1117_OLD ? 9.f : 0.f;   // №1117: ход просмотра 0..1 и часы проявления галереи
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    if (uiScrBackInput(hit)) { if (view) view = 0; else break; }   // △ / ○: из просмотра в сетку, из сетки в оглавление
    else if (!view) {
      int ns = scrapGalMove(sel, hit, n, cols, open); if (ns != sel) { sel = ns; uiPopHookFire(UI_EV_SCROLL); }
      if (hit & PSP_CTRL_CROSS) { if (any && open[sel]) { view = 1; uiPopHookFire(UI_EV_ACCEPT); if (smkOn) { char s[96]; snprintf(s, sizeof s, "SCRAPBOOK1105 view kind=%d slot=%d t=%.1f\n", kind, sel, smkT); smkWrite(s); } } else uiPopHookFire(UI_EV_LOCKED); }
    } else if ((hit & PSP_CTRL_CROSS) && kind == 0 && sel < 7) {   // запуск пройденной миссии (бонусная — только просмотр): вопрос Yes / No
      scrapFadeOut(mainSel, kind, sel, (t - t0) * 1e-6f, fbp);
      SCR_CLOSE();
      int yes = scrapAskStart(mainSel, fbp);
      if (yes) {
        scrapStartSave = *g; scrapStartSave.level = scrapStartSave.msnLevel = (uint8_t)L; scrapStartSave.msnStory = 0xff; scrapStartSave.msnPhase = scrapStartSave.msnStage = 0;
        scrapStartSave.msnCompleted = d->known[L] == 2 ? d->mask[L] : 0; scrapStartSave.nworld = 0xffff; memset(scrapStartSave.worldCoins, 0, sizeof scrapStartSave.worldCoins);
        scrapStartRow = sel; if (smkOn) { char s[96]; snprintf(s, sizeof s, "SCRAPBOOK1105 start level=%d row=%d t=%.1f\n", L, sel, smkT); smkWrite(s); }
        return 1;
      }
      gameLevel = L; levelFile(fn, "scrapgal.shr"); gameLevel = keep;
      if (scrapOpenPack(fn)) return -1;
      for (int i = 0; i < n; i++) uiScrSetRow(i, open[i]);
      uiHudSet(UI_F_LEVEL, L - 1); fin = SCRAPANIM1117_OLD ? 9.f : 0.f; vt = 0.f;
      sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = sceKernelGetSystemTimeLow(); continue;
    }
    uiScrSetRow(8, sel); uiScrSetRow(9, sel);
    if (SCRAPANIM1117_OLD) vt = (float)view; else { vt += (view ? dt : -dt) / SCRAP_FLY_S; vt = vt < 0.f ? 0.f : vt > 1.f ? 1.f : vt; }   // №1117: полёт картинки 250 мс
    fin += dt; float al = SCRAPANIM1117_OLD ? 1.f : scrapEase(fin / SCRAP_IN_S);
    uiScrShow(kind, UI_T_SKIP, vt < 1.f); uiScrShow(kind, UI_T_STREETRACE, vt > 0.f);
    uiScrShow(kind, UI_T_PRESS_START, view ? (kind == 0 && sel < 7) : any);   // Accept: в сетке — если есть открытое; в просмотре миссии — запуск (кроме бонусной)
    if (vt > 0.f && vt < 1.f) scrapFlyCall(kind, sel, scrapEase(vt)); else uiScrFly(0, 0, 1.f, 0.f, 0.f, 1.f, 1.f);
    scrapPresent(mainSel, kind, sel, (t - t0) * 1e-6f, al, fbp, "scrapgallery");
    uiScrFly(0, 0, 1.f, 0.f, 0.f, 1.f, 1.f);
  }
  vt = 0.f; uiScrShow(kind, UI_T_SKIP, 1); uiScrShow(kind, UI_T_STREETRACE, 0); uiScrSetRow(8, sel); uiScrSetRow(9, sel);
  scrapFadeOut(mainSel, kind, sel, (sceKernelGetSystemTimeLow() - t0) * 1e-6f, fbp);
  uiPopHookFire(UI_EV_BACK); SCR_CLOSE();
  return result;
}
static int scrapBookRun(int mainSel, void **fbp, const SaveGame *loaded) {
  char fn[256]; SceIoStat st; sibling(fn, "scrap.shr");
  if (sceIoGetstat(fn, &st) < 0 || scrapOpenPack(fn)) return 0;
  if (uiScrScreens() < 3) { SCR_CLOSE(); return 0; }
  SaveGame g; scrapSource(&g, loaded);
  ScrapData d; scrapFill(&d, &g, cfg.movies);
  int scr = SB_BOOK, sel[3] = {0, 0, 0}, L = 1;
  if (scrapGen != lvGen) scrapLast = 0;
  if (!SCRAP_LAST_OLD && scrapLast >= 1 && scrapLast <= 7) L = scrapLast;   // помнит уровень, пока меню живо (P14 Д5)
  uiScrShow(SB_BOOK, UI_T_SKIP, scrapKrusty(&d));
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "SCRAPBOOK1105 open pct=%d krusty=%d cards=%d stats=%d/%d t=%.1f\n", d.pctGame, scrapKrusty(&d), d.sum[3], d.sum[0], 49, smkT); smkWrite(s); }
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last;
  int trOn = !SCRAPANIM1117_OLD, trTo = SB_BOOK; float trT = SCRAP_OUT_S + SCRAP_GAP_S;   // №1117: переход между экранами; на входе — проявляется
#define SCRAP_TRANS(to_, a_, b_) do { if (SCRAPANIM1117_OLD) scr = (to_); else { trOn = 1; trT = 0.f; trTo = (to_); if ((a_) >= 0) feFlyRange((a_), (b_)); } } while (0)
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    if (trOn) hit = 0;   // №1117: на время перехода ввод глохнет
    int back = 0;
    if (scr == SB_BOOK) {
      int r = uiScrMenuInput(SB_BOOK, &sel[0], hit);
      if (r == UI_SCR_BACK) back = 1;
      else if (r == 0) { scrapUiLevel(&d, L); SCRAP_TRANS(SB_CONTENTS, 810, 830); uiPopHookFire(UI_EV_ACCEPT); }   // вход в оглавление: кадры камеры 810–830
      else if (r == 1) { scrapUiStats(&d); SCRAP_TRANS(SB_STATS, 810, 830); uiPopHookFire(UI_EV_ACCEPT); }
    } else if (scr == SB_CONTENTS) {
      if (hit & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) { L = (L - 1 + ((hit & PSP_CTRL_RTRIGGER) ? 1 : -1) + 7) % 7 + 1; scrapUiLevel(&d, L); uiPopHookFire(UI_EV_SCROLL); }   // L1 / R1: уровни по кругу
      int ns = scrapGridMove(sel[1], hit); if (ns != sel[1]) { sel[1] = ns; uiPopHookFire(UI_EV_SCROLL); }
      uiHudSet(UI_F_MS_LARROW, (pad.Buttons & PSP_CTRL_LTRIGGER) != 0); uiHudSet(UI_F_MS_RARROW, (pad.Buttons & PSP_CTRL_RTRIGGER) != 0);
      if (uiScrBackInput(hit)) { SCRAP_TRANS(SB_BOOK, 830, 850); uiHudSet(UI_F_MS_LARROW, 0); uiHudSet(UI_F_MS_RARROW, 0); }   // выход в обложку: 830–850
      else if (hit & PSP_CTRL_CROSS) {
        if (sel[1] == 3) {   // карточки: тот же экран просмотра, что VIEW CARDS паузы (в оригинале это один и тот же экран); пакет альбома на время закрывается
          scrapLast = L; scrapGen = lvGen; scrapFadeOut(mainSel, scr, sel[1], (t - t0) * 1e-6f, fbp); SCR_CLOSE();
          unsigned char c_[SV_LEVELS]; memcpy(c_, g.cards, SV_LEVELS);
          scrapRun(mainSel, fbp, c_);
          if (scrapOpenPack(fn)) return 1;
          uiScrShow(SB_BOOK, UI_T_SKIP, scrapKrusty(&d)); scrapUiLevel(&d, L);
          if (!SCRAPANIM1117_OLD) { trOn = 1; trT = SCRAP_OUT_S + SCRAP_GAP_S; trTo = SB_CONTENTS; }
          sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = sceKernelGetSystemTimeLow(); continue;
        }
        else {   // миссии, костюмы, машины: галерея уровня (пакет уровня; альбом на время закрыт)
          scrapFadeOut(mainSel, scr, sel[1], (t - t0) * 1e-6f, fbp); SCR_CLOSE();
          int rg = scrapGallery(sel[1], L, &d, &g, mainSel, fbp);
          if (rg == 1) { scrapLast = L; scrapGen = lvGen; return 2; }   // запуск миссии: ни пакета, ни полёта камеры назад
          if (scrapOpenPack(fn)) return 1;
          uiScrShow(SB_BOOK, UI_T_SKIP, scrapKrusty(&d)); scrapUiLevel(&d, L);
          if (rg < 0) uiPopHookFire(UI_EV_LOCKED);
          if (!SCRAPANIM1117_OLD) { trOn = 1; trT = SCRAP_OUT_S + SCRAP_GAP_S; trTo = SB_CONTENTS; }
          sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = sceKernelGetSystemTimeLow(); continue;
        }
      }
    } else {
      if (uiScrBackInput(hit)) SCRAP_TRANS(SB_BOOK, 830, 850);   // из статистики назад: 830–850
    }
    if (back) { scrapFadeOut(mainSel, scr, scr == SB_BOOK ? sel[0] : 0, (t - t0) * 1e-6f, fbp); break; }
    int drawScr = scr; float al = 1.f;   // №1117: переход: старый экран гаснет, пусто, новый проявляется; камера идёт по кадрам параллельно
    if (trOn) {
      trT += dt;
      if (trT < SCRAP_OUT_S) al = 1.f - scrapEase(trT / SCRAP_OUT_S);
      else { if (scr != trTo) scr = trTo; drawScr = scr; al = trT < SCRAP_OUT_S + SCRAP_GAP_S ? 0.f : scrapEase((trT - SCRAP_OUT_S - SCRAP_GAP_S) / SCRAP_IN_S);
             if (trT >= SCRAP_OUT_S + SCRAP_GAP_S + SCRAP_IN_S && !feFlying()) { trOn = 0; al = 1.f; } }
    }
    scrapPresent(mainSel, drawScr, drawScr == SB_BOOK ? sel[0] : drawScr == SB_CONTENTS ? sel[1] : 0, (t - t0) * 1e-6f, al, fbp, "scrapbook1105");
  }
  scrapLast = L; scrapGen = lvGen; SCR_CLOSE();
  return 1;
}
#endif
