// ---------- уровни 1–7: набор уровня в памяти, смена уровня (29.09, «уровни 2–7 как в оригинале») ----------
// Оригинал (данные диска, docs/РЕЦЕПТ_УРОВНЯ.md §1, §9): уровень целиком описан файлами с номером; у каждого своя газета
// загрузки loading{N} (load.shr, 7 кадров), свой радар l{N}hudmap и фон паузы ingamel{N} (ui.shr уровня), своя музыка и звуки
// (audio/ уровня). Порт: levelUnload освобождает всё, что прочитали levelLoad и levelLoadAfter; levelUiSnd переоткрывает
// интерфейс и звук уровня. Куча после круга 1 -> 2 -> 1 — та же ±64 КБ (гейт tests/levels/switch.sh, строки LEVEL)
#include "lmap.h"   // загрузка 2 п.2: сжатая карта высот и стен (lmap.shr)
static void lspReset(void);   // lightsprite.h подключается позже (main.c): сброс таблицы света при выгрузке уровня
static void coinSprPreload(void);   // coinspr.h (позже): лента монеты — с карты под газетой загрузки, а не на первой монете в кадре (проверка 17.10: заминка ~25 мс)
static int loadedLevel;                 // чей набор сейчас в памяти (0 — ничей)
static int uiLevel = 1, sndLevel = 1;   // чей ui.shr / audio/ открыт (на заставке — уровня 1, как раньше)
static unsigned char *levelData; static void *colMem; static float carLift;
static int loaderUp;                    // загрузчик клеток запущен (levelUnload ждёт его простоя)
static int lvLeak;                      // отрицательный контроль гейта («flag lvleak»): выгрузка забывает PVS и коллизию
// №757: тени деревьев уровня — какие текстуры не рисовать (main_render.h TREESH_SKIP). Отпечаток — мультимножество альфы уровня 0 (не зависит от порядка
// палитры, полубайтов и swizzle): H = Σ mix(альфа) по 64×64 текселам; эталоны посчитаны по картинкам диска (treeshadow.bmp, treeshad_overcast.bmp)
static unsigned treeShMix(unsigned a) { unsigned m = (a + 1u) * 0x9E3779B1u; m ^= m >> 15; m *= 0x85EBCA77u; m ^= m >> 13; return m; }
static void treeShScan(void) {
  free(texNoDraw); texNoDraw = calloc(ntex ? ntex : 1, 1); texNoDrawN = texNoDraw ? ntex : 0; treeShSkipped = 0;
  int found[2] = {-1, -1};
  for (unsigned i = 0; i < texNoDrawN; i++) { const Tex *x = &tex[i]; if (x->w != 64 || x->h != 64 || !x->pal || !x->lv[0]) continue;
    const unsigned *pal = (const unsigned *)x->pal; int part = 0; for (int c = 0; c < (x->t4 ? 16 : 256); c++) { unsigned a = pal[c] >> 24; part += a > 0 && a < 255; }
    if (part < 4) continue;   // непрозрачные и вырезки — мимо, без обхода текселов
    const unsigned char *b = (const unsigned char *)x->lv[0]; unsigned h = 0, nb = (unsigned)texRowBytes(x->t4, x->pre, 64) * 64u;
    for (unsigned k = 0; k < nb; k++) { if (x->t4) h += treeShMix(pal[b[k] & 15] >> 24) + treeShMix(pal[b[k] >> 4] >> 24); else h += treeShMix(pal[b[k]] >> 24); }
    int w = h == 0xCB4D84CAu ? 0 : h == 0x4A7650B1u ? 1 : -1;   // treeshadow (L1), treeshad_overcast (L2)
    if (w >= 0) { texNoDraw[i] = 1; found[w] = (int)i; } }
  if (smkOn) { char s_[120]; snprintf(s_, sizeof s_, "TREESH level=%d treeshadow=%d overcast=%d draw=%d\n", gameLevel, found[0], found[1], treeShOn); smkWrite(s_); }
}
// №904 (Роб 23.10: «ворота у дома Бернса вблизи нормальные, издалека дырявые, какая-то оптимизация прозрачности»; L1 F9-1): решётки, ворота, переплёты окон —
// вырезка с тонкими прутьями (альфа почти только 0/255, толщина прута 3–6% стороны текстуры). Мипы уровней 2 и ниже при альфа-тесте рвут прутья в крошку
// (уровень 2: прутья тоньше пикселя, при тесте > 0x40 остаются обрывки; уровень 3 и дальше — тёмная клякса с дырами); на дальности 20 м кадр — рваная решётка,
// на 30 м — полоса. Правило семейства: у таких текстур всех уровней мипов не больше LATTICE_LEVELS (0 и 1) — дальние пиксели берут уровень 1 (тонкие прутья
// остаются прутьями, при удалении мельчают, как у любой решётки). Признак — по данным текстуры при загрузке (как treeShScan): уровень 0, маска альфа > 0x40,
// толщина = 2·площадь/периметр (текселы) к большей стороне < LATTICE_THICK, доля среднего (альфа 8..247) <= 2%. LATTICE_OLD=1 / файл latticeold рядом с EBOOT /
// «flag lattice 0|1» (1 — по правилу, 0 — как было; в сценарии, затем dloff 1) — прежнее поведение, красный контроль гейта tests/render/lattice_mips.sh
#ifndef LATTICE_OLD
#define LATTICE_OLD 0
#endif
#ifndef LATTICE_LEVELS
#define LATTICE_LEVELS 2
#endif
#ifndef LATTICE_THICK
#define LATTICE_THICK 0.11f
#endif
static int latticeOn = !LATTICE_OLD;
static unsigned latticeN;
static inline unsigned latTexel(const Tex *x, int px, int py) {   // индекс палитры тексела уровня 0 (swizzle: блоки 16 Б x 8 строк, как tools/texq.py swizzle)
  int rb = texRowBytes(x->t4, x->pre, x->w), bc = x->t4 ? px >> 1 : px; const unsigned char *b = (const unsigned char *)x->lv[0];
  unsigned v = x->pre ? b[((py >> 3) * (rb >> 4) + (bc >> 4)) * 128 + (py & 7) * 16 + (bc & 15)] : b[py * rb + bc];
  return x->t4 ? ((px & 1) ? v >> 4 : v & 15) : v;
}
static void latticeApply(void) { for (unsigned i = 0; i < ntex; i++) { Tex *x = &tex[i]; if (x->lat) x->levels = latticeOn && x->levelsAll > LATTICE_LEVELS ? LATTICE_LEVELS : x->levelsAll; } }
static void latticeScan(void) {
  latticeN = 0;
  for (unsigned i = 0; i < ntex; i++) { Tex *x = &tex[i]; x->levelsAll = x->levels; x->lat = 0;
    if (x->levels <= LATTICE_LEVELS || !x->pal || !x->lv[0] || x->w < 16 || x->h < 16 || x->w > 256 || x->h > 256) continue;
    const unsigned *pal = (const unsigned *)x->pal; int lo = 0, hi = 0, mid = 0; for (int c = 0; c < (x->t4 ? 16 : 256); c++) { unsigned a = pal[c] >> 24; lo += a <= 0x40; hi += a >= 200; mid += a > 8 && a < 247; }
    if (!lo || !hi) continue;   // нет и прозрачного, и плотного — не вырезка
    unsigned area = 0, per = 0, midpx = 0;
    for (int y = 0; y < x->h; y++) for (int px = 0; px < x->w; px++) { unsigned a = pal[latTexel(x, px, y)] >> 24; midpx += a > 8 && a < 247; if (a <= 0x40) continue; area++;
      if (px == 0 || y == 0 || px == x->w - 1 || y == x->h - 1 || (pal[latTexel(x, px - 1, y)] >> 24) <= 0x40 || (pal[latTexel(x, px + 1, y)] >> 24) <= 0x40 || (pal[latTexel(x, px, y - 1)] >> 24) <= 0x40 || (pal[latTexel(x, px, y + 1)] >> 24) <= 0x40) per++; }
    if (!area || !per || midpx * 50u > (unsigned)(x->w * x->h)) continue;
    float thick = 2.f * (float)area / (float)per / (float)(x->w > x->h ? x->w : x->h);
    if (thick < LATTICE_THICK) { x->lat = 1; latticeN++; if (smkOn) { char s_[100]; snprintf(s_, sizeof s_, "LATTICE tex=%u %dx%d thick=%.3f levels=%d->%d\n", i, x->w, x->h, thick, x->levels, LATTICE_LEVELS); smkWrite(s_); } } }
  latticeApply();
  if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "LATTICES level=%d n=%u on=%d\n", gameLevel, latticeN, latticeOn); smkWrite(s_); }
}
// №907 (Роб 23.10, снимок TEST53: у синего автомата с газировкой у АЭС «нормали смотрят внутрь» — стенки не видно, видна изнанка задней и тёмный низ): у предметов диска
// (пропы PRP1, стейтпропы SPR1: автоматы, будки, ящики, колонки) материал помечен translucent (флаг 2), хотя текстура целиком непрозрачна (альфа 255) и у вершин альфа 255 —
// смешивание ничего не меняет. Но кусок с флагом 2 уходит в очередь полупрозрачного: глубину не пишет, треугольники ложатся в порядке файла, отсечения изнанки у пропов нет
// (двусторонние) — у закрытого предмета (кузов автомата, 70 треугольников) задняя и дальняя стенки ложатся поверх ближней. Правило семейства, как у машин (№413б, texOpaque/carGlass):
// кусок пропа или стейтпропа с флагом 2 (не аддитивный, не небо), у которого вся палитра занятых текселов и все вершины с альфой >= BLENDOPQ_ALPHA, рисуется непрозрачным (с записью глубины).
// Один проход по таблицам кусков при загрузке уровня (правит секцию в памяти, NEW GAME берёт её же), в кадре ничего не стоит. Прозрачное в данных (стекло, огни, сетки) остаётся.
// BLENDOPQ_OLD=1 / «flag blendopq 0|1» (в сценарии) / файл blendopqold рядом с EBOOT — прежнее, красный контроль гейта tests/inside907
#ifndef BLENDOPQ_OLD
#define BLENDOPQ_OLD 0
#endif
#ifndef BLENDOPQ_ALPHA
#define BLENDOPQ_ALPHA 250   // как у экспорта (binary_alpha: 250/255)
#endif
#define BLENDOPQ_MAX 1024
static int blendOpqOn = !BLENDOPQ_OLD;
static int *bqFl[BLENDOPQ_MAX]; static unsigned char bqOld[BLENDOPQ_MAX]; static int bqN, bqTris, bqInst;   // куски, чей флаг снят (адрес поля flags, прежний байт), треугольники, экземпляры
static int bqVertsOpaque(const void *vb, unsigned vOff, unsigned nv, int fl) {
  if (fl & F_V12) return 1;   // 12 Б: цвет 5650, альфа 255
  const unsigned char *p = (const unsigned char *)vb + vOff;
  for (unsigned i = 0; i < nv; i++) if (p[i * 16 + 7] < BLENDOPQ_ALPHA) return 0;
  return 1;
}
static void bqPiece(int *flp, int tex, const void *vb, unsigned vOff, unsigned nv, unsigned ni, int inst) {
  int fl = *flp;
  if (!(fl & 2) || (fl & (4 | F_ADDITIVE)) || bqN >= BLENDOPQ_MAX) return;
  if (!texOpaque(tex) || !bqVertsOpaque(vb, vOff, nv, fl)) return;
  bqFl[bqN] = flp; bqOld[bqN] = (unsigned char)(fl & 2); bqN++; bqTris += (int)ni / 3; bqInst += inst;
  if (blendOpqOn) *flp = fl & ~2;
}
static void blendOpqApply(void) { for (int i = 0; i < bqN; i++) { if (blendOpqOn) *bqFl[i] &= ~2; else *bqFl[i] |= bqOld[i]; } }
static void blendOpqScan(void) {
  bqN = bqTris = bqInst = 0;
  if (haveProps) {
    unsigned cnt[2048] = {0}; for (unsigned i = 0; i < props.ninst; i++) if (props.def[i].type < 2048) cnt[props.def[i].type]++;
    for (unsigned t = 0; t < props.ntype && t < 2048; t++)
      for (unsigned k = 0; k < props.types[t].nPiece; k++) { PropPiece *pp = &props.pieces[props.types[t].firstPiece + k]; bqPiece(&pp->flags, pp->tex, props.vertBase, pp->vOff, pp->nv, pp->ni, (int)cnt[t]); }
  }
  if (haveSP) {
    unsigned cnt[2048] = {0}; for (unsigned i = 0; i < sprops.ninst; i++) if (sprops.inst[i].type < 2048) cnt[sprops.inst[i].type]++;
    for (unsigned t = 0; t < sprops.ntype && t < 2048; t++)
      for (unsigned k = 0; k < sprops.type[t].nPiece; k++) { SpPiece *pp = (SpPiece *)&sprops.piece[sprops.type[t].firstPiece + k]; bqPiece((int *)&pp->flags, pp->tex, sprops.vertBase, pp->vOff, pp->nv, pp->ni, (int)cnt[t]); }
  }
  if (smkOn) { char s[120]; snprintf(s, sizeof s, "BLENDOPQ level=%d pieces=%d tris=%d inst=%d on=%d\n", gameLevel, bqN, bqTris, bqInst, blendOpqOn); smkWrite(s); }
}
// правка Роба 137: в PauseSunday.pag оригинала пункта RESET CAR нет (docs/ui_integration.md) — у игрока строка паузы скрыта;
// служебный режим (svcMode) её видит. У игрока RESET CAR остаётся на SELECT в машине (ControllerPS2.pag)
#ifndef RESET_ROW_OLD   // слово Роба 17.10: «RESET CAR не нужен нигде» — строка скрыта и в служебном режиме (тур); RESET_ROW_OLD=1 — как раньше (видна при svcMode). SELECT в машине остаётся
#define RESET_ROW_OLD 0
#endif
static void pauseRowsSvc(void) { uiMenuHide(UI_MENU_PAUSE, UI_P_RESET_CAR, RESET_ROW_OLD ? !svcMode : 1); }
static void uiMenusSetup(void) {   // пункты главного меню и паузы по подписям пакета (после uiInit; сохранения — svmInit раньше)
  mainNew = mainOpt = mainLoad = mainResume = -1;
  if (!uiReady) return;
  // 12а: в главном меню работает только NEW GAME, остальные пункты затемнены (сохранения, опции, альбом — позже)
  for (int i = 0; i < uiMenuCount(UI_MENU_MAIN); i++) { const char *l = uiMenuLabel(UI_MENU_MAIN, i);
    if (!strcmp(l, "NEW GAME")) mainNew = i; else if (!strcmp(l, "OPTIONS")) mainOpt = i;
    else if (!strcmp(l, "LOAD GAME")) mainLoad = i; else if (!strcmp(l, "RESUME GAME")) mainResume = i;
    else if (!strcmp(l, "BONUS GAME") || !strcmp(l, "MINI GAME")) mainMini = i;   // правка 420: бонус-игра (подпись пакета PAL — BONUS GAME)
    else if (!strcmp(l, "SCRAP BOOK") || !strcmp(l, "SCRAPBOOK")) mainScrap = i; }   // №862: альбом карточек
  { char fn[256]; SceIoStat st; sibling(fn, "menu.shr"); if (sceIoGetstat(fn, &st) < 0) mainOpt = -1;   // пакета нет — пункт серый
    pausePackFile(fn, "pmenu.shr"); if (sceIoGetstat(fn, &st) >= 0) { uiMenuEnable(UI_MENU_PAUSE, UI_P_OPTIONS, 1); uiMenuEnable(UI_SCREEN_PAUSEMSN, UI_PM_OPTIONS, 1); } }
  if (mainNew >= 0) for (int i = 0; i < uiMenuCount(UI_MENU_MAIN); i++) uiMenuEnable(UI_MENU_MAIN, i, i == mainNew || i == mainOpt || (svmOn() && i == mainLoad) ||
                                                                                  (i == mainMini && bonusAvailable()));
  pauseRowsSvc();
}
static void uiOpen(void) {   // ui.shr уровня gameLevel (нет своего — уровня 1): HUD в VRAM сразу за буфером глубины
  char fn[256]; levelFile(fn, "ui.shr"); uiReady = uiInit(fn) == 0;
  if (!uiReady && gameLevel > 1) { sibling(fn, "ui.shr"); uiReady = uiInit(fn) == 0; }
  uiLevel = gameLevel;
  if (uiReady) { unsigned vo = BUF_W * SCR_H * ((fb16 ? 2 : 4) * 2 + 2), ve = 0x200000 - (fb16 && !JITTER_HIST_OLD ? JIT_HIST_BYTES : 0); int uu_ = uiUseVram(vo, ve - vo); tvBase = (vo + (unsigned)uu_ + 63u) & ~63u; tvEnd = ve & ~63u; }   // №657: остаток VRAM под текстуры города (flag texvram)   // последние 272 КБ VRAM — история мира для jitter.h
}
static int sndFail1065, sndFail1065Sim;   // №1065 служебно «flag sndfail1065 N»: следующие N открытий банка звука отказывают (проверка повтора без тесной кучи)
static void levelUiSnd(void) {   // интерфейс и звук уровня gameLevel (на заставке открыты уровня 1)
  if (uiLevel != gameLevel) { bbStep = "ui-free"; if (uiReady) uiShutdown(); bbStep = "ui-open"; uiOpen(); bbStep = "ui-menus"; uiMenusSetup(); bbStep = "ui-cfg"; cfgApply(); uiSfxInit(smkWrite); }
  if (sndLevel != gameLevel) { bbStep = "snd-off"; sndShutdown(); char ad[256]; levelFile(ad, "audio/"); bbStep = "snd-init"; int sndRc_ = sndFail1065 > 0 ? (sndFail1065--, -1) : sndInit(ad);
    if (sndRc_ && memLoad1065 && (sndFail1065 >= 0 && (strstr(audioError(), "memory") || sndFail1065Sim))) {   // №1065: банк звука не выделился — место и один повтор
      unsigned f0_ = heapFreeBytes(), b0_ = memMaxBlock ? memMaxBlock() : 0; mem1060Pre(2400000u, "sfx.sab");
      int rc1_ = sndRc_; sndRc_ = sndFail1065 > 0 ? (sndFail1065--, -1) : sndInit(ad);
      if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "SNDRETRY1065 level=%d rc=%d->%d free=%.2f->%.2f blk=%.2f->%.2f t=%.1f\n", gameLevel, rc1_, sndRc_, f0_ / 1048576.f, heapFreeBytes() / 1048576.f, b0_ / 1048576.f, memMaxBlock ? memMaxBlock() / 1048576.f : 0.f, smkT); smkWrite(l_); } }
    if (sndRc_ == 0 || !memLoad1065) sndLevel = gameLevel;   /* №1065: окончательный отказ — уровень звука не выставлен */ bbStep = "snd-cfg"; cfgApply(); uiSfxInit(smkWrite);
    bbThForgetAudio(); feNewsResume(); }   // правка 410: зонд BBCPU — к новым потокам звука по имени, с новой базой; №789: стингер газеты — дальше
  bbStep = "ui+snd";
}
static int levelAvailable(int n) {   // набор уровня n рядом с EBOOT (level.shr; у 2–7 — в l<n>/)
  if (n < 1 || n > 7) return 0;
  int keep = gameLevel; gameLevel = n; char fn[256]; levelFile(fn, "level.shr"); gameLevel = keep;
  SceIoStat st; return sceIoGetstat(fn, &st) >= 0;
}
static int msLevelOpen(int n) {   // правка 348: уровень n в MISSION SELECT — свой всегда; другие — в DEV SAVE, если набор рядом с EBOOT
  // облако 05.10 (карьера): и в обычной игре — уровни, до которых игра дошла (запись прогресса всех уровней, savegame.h SV_EXT_LV),
  // как ◄ ► у оригинала (MissionSelect.scr: планка LEVEL N со стрелками). Прежнее — flag lvprogold 1
  return n == gameLevel || (msDevSession && levelAvailable(n)) ||
         (!gameLvProgOld && levelAvailable(n) && n >= 1 && n <= (svLvReached(&lvKeep) > gameLevel ? svLvReached(&lvKeep) : gameLevel));
}
static unsigned msLevelRows(int n) { unsigned r = 0; return !gameLvProgOld && svLvGet(&lvKeep, n, NULL, &r) ? r : 0u; }   // облако 05.10: строки уровня n
extern unsigned memMaxBlock(void);
static unsigned lvMx0;   // №696: крупнейший блок кучи в момент h0
static void levelLog(const char *what, unsigned t0, unsigned h0) {   // smoke.txt: строка LEVEL (гейт tests/levels/switch.sh)
  if (!smkOn) return;
  char s[440];
  snprintf(s, sizeof s, "LEVEL ev=%s n=%d loaded=%d ms=%.0f heap0=%u heap=%u mx0=%u mx=%u spawn=%.2f,%.2f,%.2f,%.3f cells=%u tex=%u col=%d props=%d sp=%d "
           "traffic=%d msn=%d clt=%d rooms=%d fx=%d ui=%d/%d snd=%d/%d t=%.1f\n", what, gameLevel, loadedLevel,
           (sceKernelGetSystemTimeLow() - t0) / 1000.f, h0, heapFreeBytes(), lvMx0, memMaxBlock(), spawnP[0], spawnP[1], spawnP[2], spawnP[3], ncell, ntex,
           haveCol, haveProps, haveSP, haveTraffic, mvPackLevel(), haveColl ? coll.level : 0, rooms.nroom, fxReady(), uiReady, uiLevel,
           sndReady(), sndLevel, smkT);
  smkWrite(s);
}
// №679 (Роб 17.10, режим «мультик»): контур окружения — готовые линии силуэта зданий и крупных предметов (tools/psp_export.py SHR_TOON=1, файл toonlines.shr
// уровня, TLN2): по клеткам TL_CELL м, вершины int16 в сантиметрах от угла клетки; линии вынесены наружу по нормали грани в экспорте. Рисуются, когда файл есть
// (привязка к настройке Cartoon — за ведущим режима): GU_LINES по клеткам ближе TL_RANGE м, цвет ~70 % чёрного, малый сдвиг глубины, толщина — второй
// проход со сдвигом экрана на 1 пиксель. -DTOONLINES_OLD=1 — не читать и не рисовать
#ifndef TOONLINES_OLD
#define TOONLINES_OLD 0
#endif
// №1062: выключатель INK1062_OLD / ink1062Old — в main_render.h (первым в порядке включения)
#define TL_CELL 64.f
#define TL_RANGE 60.f
typedef struct { int16_t cx, cz; uint32_t first, count; } TlCell;
static int levelToonWanted, levelToonLoaded;   // №679: хочет ли игрок мультик (ставит переключатель Cartoon / флаг сценария toonworld), загружен ли уровень-мультик
#ifndef TOON_STAT_LAZY_OLD
#define TOON_STAT_LAZY_OLD 0   // проверка 18.10: 1 — средний цвет текстур считать при каждой загрузке (и в обычном режиме)
#endif
#ifndef LEVEL_TOON_FILE_OLD
#define LEVEL_TOON_FILE_OLD 0   // №754: 1 — мир-мультик отдельным файлом level_toon.shr (только L1, со следующей загрузки уровня), как TEST32
#endif
#ifndef TOONLINES_ORDER_OLD
#define TOONLINES_ORDER_OLD 0   // №754: 1 — линии после стекла, дыма и частиц (видны сквозь них), как TEST32
#endif
#ifndef TOONLINE_THICK_OLD
#define TOONLINE_THICK_OLD 0   // №754 (план этап 1): 1 px во все стороны; 1 — вертикали в 2 px (вторая линия на пиксель правее)
#endif
static TlCell *tlC; static int16_t *tlV; static unsigned tlNC, tlN; static int tlOn = 1, tlDepth = 12, tlThick = TOONLINE_THICK_OLD; static unsigned tlDrawn;
// №824 (Роб 18.10: «дороги бледные, не хватает контраста — контур вдоль асфальтовой дороги, только там, где дорога для машин встречается с полом/тротуаром»):
// второй набор линий — край асфальта проезжей части (файл roadlines.shr уровня, TLN2 как toonlines.shr, tools/roadlines_export.py; есть на всех уровнях L1–L7,
// у L2–L7 toonlines.shr нет). Грузится и рисуется вместе с линиями мира: тот же режим Cartoon, тот же проход, перо, туман дали и толщина (skinToonLineW),
// дальность TL_RANGE. -DROADLINES_OLD=1 — roadlines.shr не читать и не рисовать (TOONLINES_OLD=1 гасит и их)
#ifndef ROADLINES_OLD
#define ROADLINES_OLD 0
#endif
// №906 (Роб 23.10: «контур бордюра не на верхнем уголке, а у низа — где бордюр стыкуется с асфальтом; на уголке сильно мерцает бело-чёрная линия
// вдоль дорог»): в данных линия поднята на 10 см (= высота бордюра — ложилась на его верхнюю грань); опускаем её к асфальту (1,5 см над краем) и держим
// от утопания на скользящем угле сдвигом глубины (64 вместо 12), а не высотой. Подбор по кадрам вдоль улицы L1 (x 186–219, z 164). ROADLINE906_OLD=1 — прежнее
#ifndef ROADLINE906_OLD
#define ROADLINE906_OLD 0
#endif
static int rlDepth = ROADLINE906_OLD ? 12 : 64; static float rlLift = ROADLINE906_OLD ? 0.f : -.085f;   // №824: сдвиг глубины линий дороги (как у линий мира, tlDepth) и добавочная высота над данными, м — служебные флаги roaddepth / roadlift (подбор); в данных линия уже поднята на 10 см
static TlCell *rlC; static int16_t *rlV; static unsigned rlNC, rlN, tlUsW, tlUsR, tlFr;
static int16_t *tlY, *rlY;   // №774: нижняя высота линий клетки-полосы, см (полосы по 2 м после tlBandSplit)   // tlUs*, tlFr — цена подачи линий мира и дороги на CPU за окно smoke.txt (только при smkOn)
// №719 (видео IMG_7280 3:18–3:51, Роб: «контур темнее, просил 70 — нужно 77–80, может чуть больше»): 80 % чёрного. TOONLINE_DARK_OLD=1 — прежние 70 %
#ifndef TOONLINE_DARK_OLD
#define TOONLINE_DARK_OLD 0
#endif
#define TL_RGBA_K(k) (TOONLINE_DARK_OLD ? 0xFF4D4D4Du : TOON_INK_OLD ? 0xFF333333u : skinInkAbgr(k))   // №951: перо по категории (мир / край асфальта)
#define TL_RGBA TL_RGBA_K(INK_WORLD)   // №796: перо линий (skin.h)   // №754: тушь вместо 80 % чёрного
// №978 (Роб 25.10: «контур бордюров не чёрный и не везде — тот же цвет, насыщеннее и темнее (у бежевых и серых — насыщенный приятный серый в оттенке), тонко»): линии края дороги (roadlines.shr, они
// и есть контур бордюра у низа) — пером тёмного насыщенного тона бордюра (skinInkTone от цвета бордюра №974), одним пикселем (без второго прохода толщины), только вблизи (INK978_RANGE м).
// INK978_OLD=1 / файл ink978old рядом с EBOOT / «flag ink978 0» — как было (перо стиля Ink, толщина и дальность как у остальных линий)
#ifndef INK978_OLD
#define INK978_OLD 0
#endif
#ifndef INK978_RANGE
#define INK978_RANGE 28.f
#endif
static int ink978On = !INK978_OLD;
static unsigned ink978Pen(void) { if (curb987On && curbNat[3]) return skinInkTone(curbNat[0], curbNat[1], curbNat[2]); return skinInkTone((unsigned)curbR, (unsigned)curbG, (unsigned)curbB); }   // №987: тон родного бордюра, не серо-фиолетовый
static int tlSelReset;   // №912: набор линий сменился — отбор записей не переиспользовать
static void edgeFree(void);
static void toonLinesFree(void) { edgeFree(); tlSelReset = 1; if (tlC) free(tlC); if (tlV) free(tlV); tlC = 0; tlV = 0; tlNC = tlN = 0; if (rlC) free(rlC); if (rlV) free(rlV); rlC = 0; rlV = 0; rlNC = rlN = 0; free(tlY); free(rlY); tlY = rlY = 0; }
static int tlReadSet(const char *name, TlCell **pc, int16_t **pv, unsigned *pnc, unsigned *pn) {   // №824: файл TLN2 уровня -> клетки и вершины (прежнее чтение toonlines.shr, одно на два файла)
  char fn[256]; levelFile(fn, name); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  uint32_t h[3]; if (sceIoRead(f, h, 12) != 12 || h[0] != 0x324E4C54u || h[1] > 20000u || h[2] > 400000u) { sceIoClose(f); return 0; }   // 'TLN2'
  TlCell *c = (TlCell *)malloc((size_t)h[1] * sizeof(TlCell)); int16_t *v = (int16_t *)malloc((size_t)h[2] * 12 + 16);
  int ok = c && v && sceIoRead(f, c, (int)(h[1] * sizeof(TlCell))) == (int)(h[1] * sizeof(TlCell)) && sceIoRead(f, v, (int)(h[2] * 12)) == (int)(h[2] * 12);
  sceIoClose(f); if (!ok) { if (c) free(c); if (v) free(v); return 0; }
  *pc = c; *pv = v; *pnc = h[1]; *pn = h[2]; return 1;
}
#include "tlines.h"
// №983 (Роб 25.10: «кромка тротуара у дороги серо-фиолетовая; контур тропинок насыщенным тоном того же цвета»): данные считает Мак (tools/edge983_list.py): edge983.bin — крайние треугольники тротуара у
// проезжей части (с цветом света вершин), по клеткам 64 м; движок рисует их слоем поверх тротуара заливкой бордюра №974 (один вызов на клетку, сдвиг глубины слоя); pathlines983.shr — отрезки
// линий мира на краях тротуаров и тропинок, рисуются пером тёмного насыщенного тона тротуара, 1 px, ближе INK978_RANGE м. EDGE983_OLD=1 / файл edge983old / «flag edge983 0» — кромки нет;
// PATH983_OLD=1 / файл path983old / «flag path983 0» — контур тропинок как был
#ifndef EDGE983_OLD
#define EDGE983_OLD 0
#endif
#ifndef PATH983_OLD
#define PATH983_OLD 0
#endif
#ifndef EDGE983_RANGE
#define EDGE983_RANGE 70.f
#endif
static int edgeOn = !EDGE983_OLD, pathOn = !PATH983_OLD;
static TlCell *egC; static MkVert *egV; static unsigned egNC, egN, egDrawn; static TlCell *plC; static int16_t *plV; static unsigned plNC, plN;
static void edgeFree(void) { free(egC); free(egV); egC = 0; egV = 0; egNC = egN = 0; free(plC); free(plV); plC = 0; plV = 0; plNC = plN = 0; }
static void edgeLoad(void) {
  { char fn[256]; levelFile(fn, "edge983.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { unsigned hd[4]; int ok = sceIoRead(f, hd, 16) == 16 && hd[0] == 0x33384745u && hd[1] <= 4000u && hd[2] <= 600000u;   // 'EG83'; подпись (hd[3]) проверяет tools/check_sidecars.py: при загрузке уровня таблица кусков ещё не готова
      TlCell *c = ok ? (TlCell *)malloc((size_t)hd[1] * sizeof(TlCell)) : 0; MkVert *v = ok ? (MkVert *)memalign(16, (size_t)hd[2] * sizeof(MkVert) + 16) : 0;
      if (c && v && sceIoRead(f, c, (int)(hd[1] * sizeof(TlCell))) == (int)(hd[1] * sizeof(TlCell)) && sceIoRead(f, v, (int)(hd[2] * 12)) == (int)(hd[2] * 12)) { egC = c; egV = v; egNC = hd[1]; egN = hd[2]; sceKernelDcacheWritebackRange(v, hd[2] * 12); c = 0; v = 0; }
      free(c); free(v); sceIoClose(f); } }
  tlReadSet("pathlines983.shr", &plC, &plV, &plNC, &plN);
  if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "EDGE983 level=%d cells=%u verts=%u pathcells=%u pathlines=%u\n", gameLevel, egNC, egN, plNC, plN); smkWrite(l_); }
}
static int edgeCurbTex(void) { static unsigned gen_ = ~0u; static int t_ = -1; if (gen_ != flatLevelGen) { gen_ = flatLevelGen; t_ = -1; for (unsigned i = 0; i < ntex && i < 1024; i++) if (tex[i].curb == 1) { t_ = (int)i; break; } } return t_; }
static unsigned edgeDraw(float ex, float ey, float ez) {   // слой кромки; вызывается до линий мира (линии лягут поверх)
  if (!ink1062Old) return 0;   // №1062
  (void)ey; if (!edgeOn || !egNC || !flatOn || !curbOn || curb987On) return 0;   // №987: кромка серо-фиолетовым убрана — бордюр родной
  int ct = edgeCurbTex(); if (ct < 0) return 0;
  unsigned n = 0; int bound_ = 0;
  for (unsigned c = 0; c < egNC; c++) {
    float ox = egC[c].cx * TL_CELL, oz = egC[c].cz * TL_CELL; if (ex < ox - EDGE983_RANGE || ex > ox + TL_CELL + EDGE983_RANGE || ez < oz - EDGE983_RANGE || ez > oz + TL_CELL + EDGE983_RANGE) continue;
    if (!bound_) { nzPend = 0; bound = -2; bindTexLean(ct); setFlags(F_LAYER); bound_ = 1; }
    ScePspFMatrix4 m = {{327.68f, 0, 0, 0}, {0, 327.68f, 0, 0}, {0, 0, 327.68f, 0}, {ox, 0.f, oz, 1}};
    sceGuSetMatrix(GU_MODEL, &m); sceGuDrawArray(GU_TRIANGLES, MK_VT, (int)egC[c].count, 0, egV + egC[c].first); n += egC[c].count / 3;
  }
  egDrawn = n; return n;
}
static unsigned pathPen(void) {   // тёмный насыщенный тон тротуара (цвет заливки класса «тротуар»)
  for (unsigned t = 0; t < ntex && t < 1024; t++) if (flatGd[t] == GD_CLS_WALK && flatTint[t]) { unsigned c = flatTint[t]; return skinInkTone(c & 255, (c >> 8) & 255, (c >> 16) & 255); }
  return 0;
}
// №1023 (Роб 26.10: «контур асфальта тонкий, почему у почтового ящика толстый, а у дороги нет; нужен тёмный насыщенный цвет асфальта, плотный, видный, не один пиксель, без дёрганья и биения, как мультик, а не шум»): с №978 край асфальта и бордюров —
// 1 px и только ближе 28 м (линия на границе поверхностей рвётся и мерцает). Теперь край асфальта (линии №824) — несколько проходов со сдвигом на пиксель, как у линий мира №779, но толще: у глаза 3 px (крест из пяти сдвигов), до 30 м — 2 px
// (квадрат 2×2, до 36 м), до 46 м — 1 px (у глаза крест до 22 м); перо то же (№1008: тёмный тон асфальта уровня), тот же туман и сдвиг глубины. ROADTHICK1023_OLD=1 / файл roadthick1023old / «flag roadthick1023 0» — как в TEST72 (1 px, 28 м)
#ifndef ROADTHICK1023_OLD
#define ROADTHICK1023_OLD 1   // Роб 26.10 ночь: вдоль асфальта видна вертикальная грань вместо контура; толстый край выключен по умолчанию
#endif
static int roadThick1023Old = ROADTHICK1023_OLD;
#define RT_PASSES 6
static const signed char rtDx[RT_PASSES] = {0, 1, 0, 1, -1, 0}, rtDy[RT_PASSES] = {0, 0, 1, 1, 0, -1};
static const float rtNear[RT_PASSES] = {0.f, 36.f, 36.f, 36.f, 22.f, 22.f};   // дальность прохода: 0 — основной (всё, до RT_RANGE)
#define RT_RANGE 46.f
static unsigned rtPen(void) {   // перо края дороги: тёмный насыщенный тон асфальта (№1008), ещё темнее на 40 % — чтобы линия читалась и на тёмном асфальте (фиолетовый L1)
  unsigned c = !roadInk1008Old && asphNat[3] ? skinInkTone(asphNat[0], asphNat[1], asphNat[2]) : ink978Pen();
  return 0xff000000u | ((c & 0xff) * 6 / 10) | (((c >> 8 & 0xff) * 6 / 10) << 8) | (((c >> 16 & 0xff) * 6 / 10) << 16); }
// №1052 (Роб 27.10: «у телефонной будки снизу голубым светится — это пятно убери»): основание-восьмиугольник будки (плоская верхняя грань на 0,19 м и кромка 0,09–0,30 м — отдельные куски; стенки основания — нижние треугольники кусков 0 и 5 того же текстурного набора)
// не рисуется: у всех будок всех уровней (признак — стейтпроп из 7 кусков, где кусок 1 — плоский восьмиугольник из 6 треугольников на высоте 0,19 м размером 2×2 м). Сама будка, трубка и векторный блик остаются.
// BOOTHBASE1052_OLD=1 / файл boothbase1052old рядом с EBOOT — как в TEST73 (на лету не переключается: данные куска правятся при загрузке уровня)
#ifndef BOOTHBASE1052_OLD
#define BOOTHBASE1052_OLD 0
#endif
static int boothBase1052Old = BOOTHBASE1052_OLD;
static unsigned boothBase1052N;   // служебно: сколько типов будок поправлено при загрузке (строка BOOTHBASE1052 в журнале)
static void boothBase1052(StateProps *s) {
  if (boothBase1052Old) return;
  for (unsigned t = 0; t < s->ntype; t++) {
    const SpType *T = &s->type[t]; if (T->nPiece != 7 || T->kind) continue;
    SpPiece *P = (SpPiece *)(uintptr_t)&s->piece[T->firstPiece];
    if (!(P[1].nv == 18 && P[1].ni == 18 && P[6].nv == 42)) continue;   // уже поправлено или другая модель
    const short *v1 = (const short *)((const char *)s->vertBase + P[1].vOff);
    int flat = 1; for (unsigned q = 0; q < 18; q++) { float y = v1[8 * q + 5] * P[1].sc / 32768.f + P[1].oy; if (y < .15f || y > .23f) flat = 0; }
    if (!flat) continue;
    P[1].nv = P[1].ni = 0; P[6].nv = P[6].ni = 0;   // верхняя грань и кромка основания
    for (int kk = 0; kk < 2; kk++) {   // стенки основания: треугольники кусков 0 и 5 целиком ниже 0,32 м
      SpPiece *p = &P[kk ? 5 : 0]; if (p->nv != p->ni || p->nv % 3) continue;
      short *v = (short *)((char *)s->vertBase + p->vOff); unsigned w = 0;
      for (unsigned q = 0; q + 2 < p->nv; q += 3) {
        float ymax = -1e9f; for (unsigned r = 0; r < 3; r++) { float y = v[8 * (q + r) + 5] * p->sc / 32768.f + p->oy; if (y > ymax) ymax = y; }
        if (ymax <= .32f) continue;
        if (w != q) memmove(v + 8 * w, v + 8 * q, 3 * 16); w += 3; }
      if (w != p->nv) { p->nv = p->ni = w; sceKernelDcacheWritebackRange(v, (unsigned)(16 * (w ? w : 1))); } }
    boothBase1052N++;
  }
  if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "BOOTHBASE1052 level=%d types=%u\n", gameLevel, boothBase1052N); smkWrite(l_); }
}
static void toonLinesLoad(void) {
  toonLinesFree(); if (!ink1062Old) { if (smkOn) smkWrite("INK1062 lines off (no read)\n"); return; }   // №1062: линии мира и край асфальта не читаются (ни ввода-вывода, ни памяти)
  if (TOONLINES_OLD || !(levelToonLoaded || (!LEVEL_TOON_FILE_OLD && skinToonEnabled()))) return;   // линии — с режимом Cartoon (№754: и без level_toon.shr, сразу при переключении)
  if (tlReadSet("toonlines.shr", &tlC, &tlV, &tlNC, &tlN) && smkOn) { char l_[96]; snprintf(l_, sizeof l_, "TOONLINES level=%d lines=%u cells=%u\n", gameLevel, tlN, tlNC); smkWrite(l_); }
  if (!ROADLINES_OLD && tlReadSet("roadlines.shr", &rlC, &rlV, &rlNC, &rlN) && smkOn) { char l_[96]; snprintf(l_, sizeof l_, "ROADLINES level=%d lines=%u cells=%u\n", gameLevel, rlN, rlNC); smkWrite(l_); }   // №824
  tlBandSplit(&tlC, tlV, &tlNC, &tlY); tlBandSplit(&rlC, rlV, &rlNC, &rlY);   // №774
  edgeLoad();   // №983
  if (tlNC || rlNC) sceKernelDcacheWritebackAll();
}
// №951б: тон линий мира на уровень — среднее по текстурам уровня (не земля, не зелень, не слишком тёмные и светлые), через skinInkTone; нет статистики — умбра №951
static void inkWorldRefresh(void) {
  if (!skinInkToneOn()) return;
  static unsigned key_; unsigned okN = 0; for (unsigned t = 0; t < ntex && t < 1024; t++) okN += tex[t].tstatOk;
  unsigned key = ntex * 2654435761u ^ okN * 40503u ^ (unsigned)(uintptr_t)tex; if (key == key_) return; key_ = key;
  unsigned long sr = 0, sg = 0, sb = 0, n = 0;
  for (unsigned t = 0; t < ntex && t < 1024; t++) { const Tex *x = &tex[t]; if (!x->tstatOk || (x->tstat[4] & 4)) continue;
    unsigned r = x->tstat[0], g = x->tstat[1], b = x->tstat[2], l = (r * 30u + g * 59u + b * 11u) / 100u; if (l < 25u || l > 230u) continue;
    if (skinInkTone(r, g, b) == INK_TREE_ABGR) continue; sr += r; sg += g; sb += b; n++; }
  skinInkWorldAbgr = n >= 8 ? skinInkTone((unsigned)(sr / n), (unsigned)(sg / n), (unsigned)(sb / n)) : 0;
  if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "INKTONE level=%d world=%08x tex=%lu\n", gameLevel, skinInkWorldAbgr, n); smkWrite(l_); }
}
static unsigned toonLinesDraw(float ex, float ey, float ez) {
  if (!ink1062Old) return 0;   // №1062
  if ((!tlNC && !rlNC) || !tlOn) return 0;
  inkWorldRefresh();
  if (!TOONLINE_W779_OLD && skinToonLineW <= 0) return 0;
  int thick_ = TOONLINE_W779_OLD ? tlThick : (tlThick || skinToonLineW >= 7);
  unsigned n = 0, nr = 0;
  sceGuDisable(GU_TEXTURE_2D); sceGuColor(TL_RGBA_K(INK_WORLD)); sceGuDepthOffset(tlDepth);
  int fog0_ = sceGuGetStatus(GU_FOG);
  if (!TOONLINE_W779_OLD) { sceGuEnable(GU_FOG); if (TOON_AIR796_OLD) sceGuFog(5.f, 5.f + 35.f / .6f, skinToonAirColor() & 0xffffffu); else if (TOON_INK821_OLD) sceGuFog(8.f, 8.f + 32.f / .45f, skinToonAirColor() & 0xffffffu); else sceGuFog(12.f, 12.f + 28.f / .25f, skinToonAirColor() & 0xffffffu); }   // №821: с 12 м, к 40 м 0,25; №779: вдали светлее, к цвету дали; №796: с 8 м, к 40 м 0,45 (как контур героев)
  for (int pass = 0; pass <= thick_; pass++) {
    if (pass) sceGuOffset(2048 - SCR_W / 2 + 1, 2048 - SCR_H / 2);   // вторая линия на пиксель правее: толщина 2 px
    unsigned t0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; unsigned w0_ = smkOn ? (unsigned)sceGuCheckList() : 0;
    unsigned c0_ = tlDrawSet(tlC, tlV, tlY, tlNC, pass, ex, ey, ez, 0.f);
    unsigned t1_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    sceGuDepthOffset(rlDepth); sceGuColor(!roadInk1008Old && asphNat[3] ? skinInkTone(asphNat[0], asphNat[1], asphNat[2]) : ink978On ? ink978Pen() : TL_RGBA_K(INK_ROAD));   // №1008: край асфальта — тёмный тон самого асфальта
    unsigned c1_ = 0; if (!(ink978On && (pass || !roadThick1023Old))) { if (ink978On) tlRangeOv = INK978_RANGE; c1_ = tlDrawSet(rlC, rlV, rlY, rlNC, pass, ex, ey, ez, rlLift); tlRangeOv = 0.f; }   // №824: край асфальта — тем же проходом, пером, туманом и толщиной
    sceGuDepthOffset(tlDepth); sceGuColor(TL_RGBA_K(INK_WORLD));
    if (smkOn) { tlUsW += t1_ - t0_; tlUsR += sceKernelGetSystemTimeLow() - t1_; tlWords += ((unsigned)sceGuCheckList() - w0_) / 4; }   // цена подачи линий на CPU (мкс), в smoke.txt раз в 1,5 с на кадр
    if (pathOn && !curb987On && plNC && !pass) { unsigned pp_ = pathPen(); if (pp_) { sceGuDepthOffset(tlDepth + 28); sceGuColor(pp_); tlRangeOv = INK978_RANGE; tlDrawSet(plC, plV, 0, plNC, 0, ex, ey, ez, 0.f); tlRangeOv = 0.f; sceGuDepthOffset(tlDepth); sceGuColor(TL_RGBA_K(INK_WORLD)); } }   // №983: линии тропинок поверх линий мира
    if (!pass) { n = c0_; nr = c1_; tlFr++; }
    if (pass) sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
  }
  if (!roadThick1023Old && ink978On && rlNC) {   // №1023: край асфальта — толстые проходы (после линий мира; туман и состояние те же)
    unsigned rt0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    sceGuDepthOffset(rlDepth); sceGuColor(rtPen());
    for (int q = 0; q < RT_PASSES; q++) {
      sceGuOffset(2048 - SCR_W / 2 + rtDx[q], 2048 - SCR_H / 2 + rtDy[q]); tlRangeOv = RT_RANGE; tlThickOv = rtNear[q];
      unsigned c_ = tlDrawSet(rlC, rlV, rlY, rlNC, q, ex, ey, ez, rlLift); if (!q) nr = c_; }
    tlRangeOv = 0.f; tlThickOv = 0.f; sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    if (smkOn) { static unsigned rtT_; unsigned now_ = sceKernelGetSystemTimeLow(); tlUsR += now_ - rt0_; if (now_ - rtT_ > 1000000u) { rtT_ = now_; char l_[120]; snprintf(l_, sizeof l_, "ROADTHICK1023 pen=%08x passes=%d us=%u lines=%u\n", rtPen(), RT_PASSES, now_ - rt0_, nr); smkWrite(l_); } }   // служебно: перо, проходы, мкс кадра на толстые проходы
  }
  sceGuDepthOffset(0); sceGuEnable(GU_TEXTURE_2D);
  if (!TOONLINE_W779_OLD) { fogBase(); if (!fog0_) sceGuDisable(GU_FOG); }   // №779: туман кадра как был
  { static unsigned tl_t; unsigned now_ = sceKernelGetSystemTimeLow(); if (smkOn && now_ - tl_t > 1500000u) { tl_t = now_; char l_[96];
      float fr_ = tlFr ? 1.f / (float)tlFr : 0.f;
      if (tlNC) { snprintf(l_, sizeof l_, "TOONLINES draw=%u of %u us=%.1f cells0=%.1f cells1=%.1f words=%.0f\n", n, tlN, tlUsW * fr_, tlDc[0] * fr_, tlDc[1] * fr_, tlWords * fr_); smkWrite(l_); }
      if (rlNC) { snprintf(l_, sizeof l_, "ROADLINES draw=%u of %u us=%.1f\n", nr, rlN, tlUsR * fr_); smkWrite(l_); }   // №824: us — мкс на кадр, подача линий на CPU
      tlUsW = tlUsR = tlFr = 0; tlDc[0] = tlDc[1] = tlWords = 0; } }
  tlDrawn = n + nr; return n + nr;
}

static void levelLoadAfter(void) {   // после newGame (порядок как раньше): комнаты, камеры старта миссий и зон уровня
  {char pd[256];levelFile(pd,"");phoneViewWarm(pd);}ldStep("phone-warm");
  roomsLoad(); ldStep("rooms");    // 11.2: комнаты (interiors-l<N>.bin + таблица interiors-l<N>-gfx.bin); нет — двери не открываются
  msncamLoad(); ldStep("msncam");   // правка 40: камеры старта миссий (msncam.shr уровня); loadPump — кадр газеты (правка 356: читается под ней)
  zonecamLoad(); ldStep("zonecam");  // правка 40: камеры зон города (zonecam-l<N>.shr)
#ifndef CAR_ZONE_CAM_OLD
  carzoneLoad(); ldStep("carzone");  // №408: камеры зон машины (carzone-l<N>.shr)
#endif
  if (roomsMsg[0]) smkWrite(roomsMsg);
  msnRoomSync();     // первая фаза уровня в комнате (L4, L7: Гомер дома)
  { char sd[256]; levelFile(sd, ""); if (pcOld && gameLevel > 1) strcat(sd, "pcold/"); wardrobeInit(sd); gameMsnSetSkinTarget(skinTargetMain); }   // правка 107: костюмы и места их выбора (skins.txt) — на каждом входе в уровень; 352: pcold — без костюмов уровня 2–7, как было
  if (skinWorn) homerWear(0);   // (новая игра, загрузка, смена уровня, служебный запуск), не только в newGame; уровень — в своём (правило порта)
  coinSprPreload();
}
static void pfClear(void);
static void levelUnload(void) {   // всё, что держит набор уровня, — обратно в кучу (ui.shr и audio/ — levelUiSnd при чтении)
  if (starve1060 >= 0.f) { heapLeft(1000.f); starve1060 = -1.f; }   // №1060 служебное: потребитель уровня умер вместе с ним
  carLtReset();   // №853: длинные треугольники машин уровня (свой участок памяти)
  skyHorizonInvalidate();
  uiScrWarmClear();
  unsigned t0 = sceKernelGetSystemTimeLow(), h0 = smkOn ? heapFreeBytes() : 0; lvMx0 = smkOn ? memMaxBlock() : 0;
  mvFree();                                                   // миссии: модели, маркеры, пакет, msnart.shr
  mcrFreeAll(); msnartRoom = 0; free(mcrDir); mcrDir = 0; mcrN = 0; mcSetGeo(0, 0); mcrTexOk = 0; gameMsnSetRoads(0);   // машины миссий
  if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  gagStreamG = gagStreamT = -1; shakeG = -1;
  if (rs.room >= 0 || rg.mem) roomLeave();
  roomsFree(&rooms); rgN = 0; memset(rgBad, 0, sizeof rgBad); roomsMsg[0] = 0;
  nzcam = 0; free(zcamBuf); zcamBuf = 0; zoneCamOn = 0;
#ifndef CAR_ZONE_CAM_OLD
  nczcam = 0; free(czcamBuf); czcamBuf = 0; czs.cur = -1;
#endif
  carZoneOn = 0;
  nmcam = 0; free(mcamBuf); mcamBuf = 0; mcPlay = 0; phoneAnim = 0; phoneAnimT = -1.f;
  if (haveColl) collectFree(&coll);
  #ifndef GAG_SKIN_OLD
  gagSkinFree(); collStreamFree();
  if(collStreamFd>=0){sceIoClose(collStreamFd);collStreamFd=-1;}
  #endif
  haveColl = 0; free(collPak); collPak = 0; free(cxTex); cxTex = 0; cxNtex = 0; G.pColl = 0;
  // клетки города: очередь загрузчика пуста, он простаивает (ждёт семафор и клеток не трогает) — потом из кучи
  lqn = 0; __asm__ volatile("" ::: "memory");
  { LdWait lw_ = {0}; while (loaderUp && (!loaderIdle || loaderBusy)) { loaderWaitStep(&lw_, "unload", 0, 0); sceKernelDelayThread(1000); } }
  ringState = 0; tailState = 0;   // загрузка 2 п.5/6: задания прежнего уровня сняты
  pfClear();
  for (unsigned i = 0; i < ncell; i++) unloadCell(&cl[i]);
  stInit = 0; loadedBytes = 0;
  if (haveTraffic) { carFillFree(); toneBlkDel(traffic.base); trafficFree(&traffic); }
  haveTraffic = 0; trTexBase = 0;
  if (haveSP) spFree(&sprops);
  haveSP = 0; haveAo = 0; aoAct = 0;
  if (haveCoins) coinsFree(&coins);
  haveCoins = 0;
  if (haveProps) propsFree(&props);
  haveProps = 0; lvaFree(propSec); propSec = 0; propSecN = 0;
  if (!lvLeak) { if (haveCol) colFree(&world); lvaFree(colMem); }
  haveCol = 0; colMem = 0; memset(&world, 0, sizeof world);  chaseView = 0;   // №639: вид по SELECT не переходит на другой уровень
  haveCarVis = 0; doorHinge = -1; lvlCarOk = -9;   // машина следующего уровня — из его CAR1
  lvlLightOk = -9; skinLightLevel(0, 0, 0, 0);       // свет следующего уровня — из его CAR1 (пока — свет уровня 1)
  if (!lvLeak) { lvaFree(pvsEnt); lvaFree(pv2Mem); }   // №962: могут лежать в запасе участка lva
  pv2Mem = 0; memset(&pv2, 0, sizeof pv2); pv2Cur = pv2CurCell = -1;
  lspReset();   // аудит Сборки: таблица света не переживает смену уровня
  dalFree(); dripFree();   // №877, №770
  pvsEnt = 0; pvsPool = 0; pvsW = pvsH = 0; pvsRange = PVS_RANGE;
  free(regMask); regMask = 0; regW = regH = 0; regEye = 0xFFFFu;   // правка 95: маска регионов уровня
  free(zg); free(zp); zg = 0; zp = 0; nzg = nzp = zgOn = zgInit = zgStart = 0; regRes = 0xFFFFu;   // правка 181
  for (unsigned i = 0; i < ntex; i++) if (!tex[i].pre && tex[i].slv[0] && tex[i].slv[0] != tex[i].lv[0]) free(tex[i].slv[0]);   // swizzleAll
  skinToonFlush();   // правка 428: оболочки персонажей прежнего уровня (модели уходят вместе с ним)
  road934Free();   // №934: дальние дороги и мосты прежнего уровня
  marksClose(); free(tex); tex = 0; ntex = 0; free(pc); pc = 0; free(cr); cr = 0; cullHierFree(); npc = 0; free(carP); carP = 0; free(skyP); skyP = 0; ncarP = nskyP = 0; sky954Free();
  for (unsigned i = 0; cdl && i < ncell; i++) dlFree(i);   // правка 275
  free(cdl); cdl = 0; free(cdlMark); cdlMark = 0;
  cgGen++; csNearPrevOk = 0; peOk = 0; nfAge = 99; phPrevOk = 0; phHold = 0;   // №654: кэши камеры не переживают уровень
  roadqFree();   // №864
  slLightResetReq = 1; toneJobCancel(); toneBlkDel(levelData); free(cl); cl = 0; free(cellBigF); cellBigF = 0; ncell = 0; lvaFree(levelData); levelData = 0; hm = wl = 0;
  lvaReset();   // №775: участок уровня пуст — следующий уровень кладёт резидентную часть, COL1 и PRP2 туда же
  fxFree();
  loadedLevel = 0; bound = -2;
#if LEAK696_TRACE
  { extern void lt696Dump(void (*)(const char *), const char *); lt696Dump(smkWrite, "unload"); { char q[80]; snprintf(q, sizeof q, "LT696 df n=%u over=%u tot=%u\n", dfN, dfOverflow, dfTotal); smkWrite(q); } }
#endif
  levelLog("free", t0, h0);
}
// служебные флаги сценария (гейты tests/levels/switch.sh, save_level.sh): lvset N — уровень NEW GAME (как L/R), lvwin — все сюжетные
// миссии пройдены (конец уровня событием рантайма), lvlog — строка LEVEL; отрицательные контроли: lvleak — выгрузка забывает PVS
// и коллизию, lvnosave — загрузка сохранения берёт уровень 1
#ifndef LOAD_ONE_OLD   // правка 356 (ниже, loadOneAt): 1 — прежние газета, ирис, газета при смене уровня
#define LOAD_ONE_OLD 0
#endif
static int loadOneOld = LOAD_ONE_OLD;
#ifndef START_NOLOAD_OLD   // №576 (Роб 12.10): старт из меню — без мелькания экрана загрузки, сразу ирис в комнату
#define START_NOLOAD_OLD 0
#endif
static int startNoLoadOld = START_NOLOAD_OLD;
// №663 (Роб, видео TEST26 1:24, 1:48, 5:36, 6:00: «почему нет заставки газеты»): №576 уводил старт из меню (RESUME / LOAD / NEW GAME при уже прочитанном уровне) в ирис без экрана
// загрузки, а работа старта (запись, миссия строки, круг клеток, прогрев) шла под ЧЁРНЫМ кадром, 5,6 с на PSP. Как у PS2, любая загрузка уровня показывает газету этого уровня:
// теперь газета и на старте из меню (ирис №576 остаётся только при START_NEWS_OLD=1, «flag startnewsold 1» или файл startnewsold рядом с EBOOT — красный контроль гейта)
#ifndef START_NEWS_OLD
#define START_NEWS_OLD 0
#endif
static int startNewsOld = START_NEWS_OLD;
// Кружок после газеты (ирис news_out) — по файлу-выключателю newsiris рядом с EBOOT или «flag newsiris 1»; по умолчанию его нет: у PS2 газета загрузки кончается жёстким срезом на уровень
// (кадры 5630 -> 5640, build/twins/o4/ps2_card_save5), кружок бывает только в переходе из меню до газеты
static int newsIris;
#ifndef RESUME_RING_OLD
#define RESUME_RING_OLD 0
#endif
static int resumeRingOld = RESUME_RING_OLD;
static int pauseCellOld;   // №598: код клетки на паузе — прежний вывод uiText (шрифт отладочный msx); флаг сценария pausecellold 1 или файл pausecellold рядом с EBOOT
static void levelService(const char *f, float a) {
  if (!strcmp(f, "lvset")) { int n = (int)a; if (levelAvailable(n)) menuLevel = n;
    char s[64]; snprintf(s, sizeof s, "MENULEVEL n=%d set=%d t=%.1f\n", menuLevel, n, smkT); smkWrite(s); }
  else if (!strcmp(f, "startnoloadold")) startNoLoadOld = (int)a;
  else if (!strcmp(f, "startnewsold")) startNewsOld = (int)a;
  else if (!strcmp(f, "newsiris")) newsIris = (int)a;
  else if (!strcmp(f, "loadwaitlose")) lwLose = (int)a;
  else if (!strcmp(f, "loadwaitms")) { lwMs = (unsigned)a; loadFuseIoLimitMs((unsigned)a); }   // №LOADWAIT: служебные флаги в меню (до игрового цикла)
  else if (!strcmp(f, "loadreadhang")) loadFuseIoHang((int)a);
  else if (!strcmp(f, "ldslow")) ldSlowUs = (unsigned)a;   // №775: медленная карта на экранах загрузки (мкс на кусок чтения)
  else if (!strcmp(f, "loadwaitold")) { loadWaitOld = (int)a; loadFuseIoWaitOld(loadWaitOld); }
  else if (!strcmp(f, "newslvred")) newsLvRed = (int)a;
  else if (!strcmp(f, "framesheet")) { fsInt = a; fsT0 = 0; fsN = 0; }
  else if (!strcmp(f, "newsshot")) { newsShotOn = (int)a; newsShotK = 0; }
  else if (!strcmp(f, "lvwin")) { mvServiceWin(); smkWrite("LVWIN\n"); }
  else if (!strcmp(f, "lvlog")) { lvMx0 = memMaxBlock(); levelLog("now", sceKernelGetSystemTimeLow(), heapFreeBytes()); }
  else if (!strcmp(f, "lvfuseslow")) fuseProbeArm(a > 0.f ? (unsigned)(a * 1000.f) : 0);
  else if (!strcmp(f, "lvshot")) { loadShot = 1; loadShotAt = a; }   // снимок «loading» со следующего экрана загрузки (газета уровня)
  else if (!strcmp(f, "lvleak")) { lvLeak = 1; smkWrite("LVLEAK on\n"); }
  else if (!strcmp(f, "lvleak696old")) { lvLeak696Old = (int)a; smkWrite("LVLEAK696OLD on\n"); }
  else if (!strcmp(f, "lvnosave")) { lvNoSave = 1; smkWrite("LVNOSAVE on\n"); }
  else if (!strcmp(f, "lvtwice")) { loadOneOld = 1; smkWrite("LVTWICE on\n"); }   // правка 356: красный — прежние газета, ирис, газета
}
// Правка Роба 352 (05.10, голосом: «переносишь на разные уровни с одним и тем же скином; на втором уровне Барт, на третьем Лиза или
// Мардж — на каждом уровне свой главный игрок»). [диск] игрок уровня — AddCharacter("имя", ...) в scripts/missions/levelNN/leveli.mfk:
// L1 и L7 Гомер, L2 и L6 Барт, L3 Лиза, L4 Мардж, L5 Апу; у каждого своя модель, скелет и клипы (art/chars/<имя>_m/_a.p3d), за рулём —
// общий набор водителя ndr (.cho). Порт: пакет игрока уровня l<N>/player.shr (tools/homer_export.py --level N; костюмы уровня
// l<N>/skin1.shr, "skin2.shr", "skin3.shr" и l<N>/skins.txt, wardrobe.shr — tools/skins_export.py --level N); нет файла — Гомер
// homer.shr. Меняется в начале чтения уровня (новая игра, LOAD / RESUME, смена уровня, служебный запуск): старый пакет сначала в кучу,
// потом новый — два персонажа кучу не делят (Барт 330 КБ, Апу 356 КБ против 325 КБ Гомера); newGame ставит игрока заново. Управление,
// числа ходьбы, прыжка и посадки — общие (viewer/homer.c; в данных диска своих чисел у персонажей нет: walker один — правило порта)
// №775 (TEST33 на PSP: L4 «Мардж только машина» — модель игрока не встала, ни своя, ни запасной Гомер): причина отказа — в ящик синхронно (op fail) и строкой
// PLAYERFAIL в журнал; своя модель читается ещё раз, потом Гомер. err: 1 не открылся, 2 нет памяти, 3 прочитано меньше, 4 разбор, 5 homerInit. PLAYER_RETRY_OLD=1 — прежнее
#ifndef PLAYER_RETRY_OLD
#define PLAYER_RETRY_OLD 0
#endif
static void playerFailNote(const char *name, int err) {
  char a_[64]; snprintf(a_, sizeof a_, "player_%s_e%d_%u", name, err, skinLoadGot); bbOp("fail", a_, 1);
  if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "PLAYERFAIL file=%s err=%d got=%u heap=%u blk=%u t=%.1f\n", name, err, skinLoadGot, heapFreeBytes(), memMaxBlock(), smkT); smkWrite(s_); }
}
static void playerLoad(void) {
  if (!homerM) return;                         // homer.shr не прочитан — пешком никого (как было)
  if (!pcPath[0]) sibling(pcPath, "homer.shr");  // при запуске main читает homer.shr
  char fn[256]; sibling(fn, "homer.shr");
  if (!pcOld && gameLevel > 1) { char lf[256]; SceIoStat st; levelFile(lf, "player.shr"); if (sceIoGetstat(lf, &st) >= 0) snprintf(fn, sizeof fn, "%s", lf); }
  unsigned h0 = smkOn ? heapFreeBytes() : 0, t0 = sceKernelGetSystemTimeLow();
  int same = !strcmp(fn, pcPath);
  char base[256]; sibling(base, ""); size_t bl = strlen(base);   // имя файла от папки EBOOT — для ящика и журнала
  if (!same) {
    bbOp("player", strncmp(fn, base, bl) ? fn : fn + bl, 1);   // самописец: чтение с карты — строка до чтения
    SkinModel *old = homerM; homerM = 0; bbStep = "player-free"; skinFree(old);
    bbStep = "player-read"; SkinModel *m = skinLoad(fn); bbStep = "player-init"; int ok = m && homerInit(&homer, m) == 0;
    if (!ok && !PLAYER_RETRY_OLD) {   // №775: отказ — причина в ящик (синхронно) и в журнал, одна повторная попытка того же файла, потом Гомер
      playerFailNote(strncmp(fn, base, bl) ? fn : fn + bl, m ? 5 : skinLoadErr);
      if (m) skinFree(m); m = skinLoad(fn); ok = m && homerInit(&homer, m) == 0; }
    if (!ok) { if (m) skinFree(m); sibling(fn, "homer.shr"); m = skinLoad(fn); ok = m && homerInit(&homer, m) == 0;   // запасной — Гомер
      if (!ok && !PLAYER_RETRY_OLD) playerFailNote("homer.shr", m ? 5 : skinLoadErr); }
    if (!ok) { if (m) skinFree(m); m = 0; haveHomer = 0; pcPath[0] = 0; }
    else { snprintf(pcPath, sizeof pcPath, "%s", fn); skinWorn = 0; homer.solid = homerSolidAll; homer.solidUser = 0; }
    homerM = m;
  }
  if (smkOn) { const char *b2 = strncmp(pcPath, base, bl) ? pcPath : pcPath + bl;
    char s[256]; snprintf(s, sizeof s, "PLAYER ev=%s level=%d file=%s pre=%s joints=%u clips=%u heap0=%u heap=%u ms=%.0f old=%d t=%.1f\n",
                          same ? "keep" : "load", gameLevel, b2, haveHomer ? homerClipPrefix() : "-", homerM ? homerM->h->nJoints : 0u,
                          homerM ? homerM->h->nClips : 0u, h0, heapFreeBytes(), (sceKernelGetSystemTimeLow() - t0) / 1000.f, pcOld, smkT); smkWrite(s); }
}
// Чтение набора уровня gameLevel (прежде — тело main после меню, перенесено без изменений порядка; уровень 1 читается
// тем же путём): резидентная часть level.shr, PVS, COL1, PRP2/SPR1, CAR1, текстуры, куски, клетки, трафик, частицы, миссии,
// карточки. Экран загрузки (ldOn) — огонь по фитилю по настоящему чтению. Интерфейс и звук уровня — levelUiSnd
#ifndef MCPROBE_BAKE_OLD
#define MCPROBE_BAKE_OLD 0   // 1 — проверка дорог считается при каждой загрузке (до загрузки 2)
#endif
#ifndef MCPROBE_VERIFY
#define MCPROBE_VERIFY 0     // 1 — служебная сборка: считать и сверить с запечённым (строка MCPVERIFY в smoke.txt)
#endif
#ifndef LOADORDER_OLD
#define LOADORDER_OLD 0   // 1 — секции level.shr читаются в прежнем порядке (PVS1, REG1, COL1, PRP2, CAR1)
#endif
#ifndef LMAP_OLD
#define LMAP_OLD 0   // 1 — карта высот и стены читаются плотным массивом из level.shr (до загрузки 2)
#endif
static unsigned phT0;
static void phMark(const char *what) {   // загрузка 2: время первых шагов levelLoad (до ldStep), строка LOADPH2 имя ms=
  bbStep = what;
  unsigned now = sceKernelGetSystemTimeLow(); if (smkOn && LOADPH) { char s[80]; snprintf(s, sizeof s, "LOADPH2 %s ms=%u\n", what, (now - phT0) / 1000u); smkWrite(s); } phT0 = now;
}
static unsigned mcColHash;   // хеш сырого COL1 текущего уровня (msncar.c mcProbeHash)
static void levelColAdopt(void *m, unsigned sz) {   // COL1 прочитан в m: хеш сырого (до colInit) и разбор
  if (!MCPROBE_BAKE_OLD) mcColHash = mcProbeHash(m, sz);
  if (colInit(&world, m, sz) == 0) { haveCol = 1; colMem = m; } else lvaFree(m);
}
static int propKickLog946(void *u, unsigned i, float dx, float dz) {   // №946 служебно (смоук): строка на каждый удар по предмету PRP1 и что игра сделала с ним
  int r = gamePropKick(u, i, dx, dz); if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "PROPKICK i=%u type=%u r=%d t=%.2f\n", i, props.def[i].type, r, smkT); smkWrite(l_); } return r; }
static void levelPropAdopt(void *m, unsigned sz) {   // PRP1/PRP2 прочитан в m
  if (propsInit(&props, m, sz) == 0) {
    haveProps = 1; propSec = m; propSecN = sz;   // 12а: секция живёт до выхода, NEW GAME берёт состояние из неё заново
    if (props.coinType >= 0 && props.ncoin && coinsInit(&coins, props.coinPos, props.ncoin) == 0) { haveCoins = 1; coins.dropAuto = 1; }   // пока игрок всегда в машине
    props.onKick = propKickLog946; props.kickUser = &G;   // шкала, обломки SPR1 (viewer/game.c) + звук и частицы ATC (onPropKickFx)
    rfSpReset();   // render_fast О2: радиусы типов — от нового SPR1
    if (props.spr && spInit(&sprops, props.spr, props.sprSize) == 0) haveSP = 1;
    if (haveSP) boothBase1052(&sprops);   // №1052
    for (unsigned i_ = 0; i_ < props.npiece; i_++) flatMarkPiece(props.pieces[i_].tex, props.pieces[i_].flags & (0xFF | F_ADDITIVE), 0);   // №912: куски предметов — те же правила заливки
    if (haveSP) for (unsigned i_ = 0; i_ < sprops.npiece; i_++) flatMarkPiece(sprops.piece[i_].tex, sprops.piece[i_].flags & (0xFF | F_ADDITIVE), 0);
    aoBind();
  }
  else lvaFree(m);
}
static unsigned mcColHash;   // хеш сырого COL1 текущего уровня (msncar.c mcProbeHash)
// №PFETCH: очередь опережающего чтения целых файлов (слоты pf в main_stream.h). pfTake — хук для traffic.c / fx.c: ждёт, пока загрузчик дочитает (кадр газеты идёт), отдаёт буфер
static void pfClear(void) { for (int i = 0; i < PF_MAX; i++) { while (pf[i].state == 1) sceKernelDelayThread(1000); if (pf[i].state == 2) free(pf[i].buf); pf[i].buf = 0; pf[i].state = 0; } }
static void pfQueue(const char *path, const char *key) {
  if (pfOld || !loaderUp) return;
  for (int i = 0; i < PF_MAX; i++) if (pf[i].state == 0 || pf[i].state == 4) {
    snprintf(pf[i].path, sizeof pf[i].path, "%s", path); snprintf(pf[i].key, sizeof pf[i].key, "%s", key); pf[i].buf = 0; pf[i].size = 0;
    __asm__ volatile("" ::: "memory"); pf[i].state = 1; ldSignal(); return; }
}
static unsigned pfCrc(const unsigned char *d, unsigned n) {   // быстрая сумма по 32-битным словам (FNV-1a по словам, хвост по байтам): журнал не должен удлинять замеряемую загрузку
  unsigned h = 0x811C9DC5u, i = 0; for (; i + 4 <= n; i += 4) { unsigned w = d[i] | d[i + 1] << 8 | d[i + 2] << 16 | (unsigned)d[i + 3] << 24; h = (h ^ w) * 0x01000193u; }
  for (; i < n; i++) h = (h ^ d[i]) * 0x01000193u; return h; }
static void *trAllocSoft962(unsigned n) { return lvaAllocSoft(n, 64u << 10, -1); }
static void trFreeSoft962(void *p) { lvaFree(p); }
static void *pfTake(const char *path, unsigned *size) {
  size_t pl = strlen(path);
  for (int i = 0; i < PF_MAX; i++) { if (pf[i].state == 0 || pf[i].state == 4) continue;
    size_t kl = strlen(pf[i].key); if (pl < kl || strcmp(path + pl - kl, pf[i].key)) continue;
    unsigned t0 = sceKernelGetSystemTimeLow();
    { LdWait lw_ = {0}; while (pf[i].state == 1) { loaderWaitStep(&lw_, "pfetch", 0, 0); if (ldOn) loadPump(); sceKernelDelayThread(1000); } }
    unsigned ms = (sceKernelGetSystemTimeLow() - t0) / 1000u; void *b = pf[i].state == 2 ? pf[i].buf : 0; *size = b ? pf[i].size : 0;
    if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "PFETCH take key=%s size=%u wait_ms=%u hit=%d crc=%08x\n", pf[i].key, *size, ms, b != 0, b ? pfCrc((const unsigned char *)b, *size) : 0u); smkWrite(s_); }
    pf[i].buf = 0; pf[i].state = 4; return b; }
  return 0;
}
// №PREREAD: заголовок (104 Б) читаем здесь, дальше всё то же, что levelLoad сделал бы сам: размер резидентной части, начало чтения (за плотным массивом карты, если рядом lmap.shr), буфер
static void levelPreread(void) {
  if (!prereadOld) loaderEnsure();
  if (prereadOld || !loaderUp || levelData || prState || prBuf) {
    if (smkOn) { char s_[112]; snprintf(s_, sizeof s_, "PREREAD skip old=%d loader=%d data=%d st=%d buf=%d t=%.1f\n", prereadOld, loaderUp, levelData != 0, prState, prBuf != 0, smkT); smkWrite(s_); } return; }
  char fn[256], lf[256]; levelFile(fn, "level.shr"); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fd < 0) return;
  unsigned char hdr[104]; int ok = sceIoRead(fd, hdr, 104) == 104 && !memcmp(hdr, "SHR7", 4); sceIoClose(fd); if (!ok) return;
  float *hd = (float *)(hdr + 12); int hw_ = *(int *)(hd + 14), hh_ = *(int *)(hd + 15);
  unsigned ncell_ = *(unsigned *)(hdr + 80), cellOff_ = *(unsigned *)(hdr + 84), resident = cellOff_ + 48 * ncell_;
  if (!LEVEL_TAIL_OLD && *(unsigned *)(hdr + 76) > 104 && *(unsigned *)(hdr + 76) < resident) resident = *(unsigned *)(hdr + 76);   // таблицы кусков и клеток (хвост) в участок не читаются: levelLoad берёт их отдельно
  unsigned dend = lmapDenseEnd((unsigned)hw_, (unsigned)hh_); SceIoStat st; levelFile(lf, "lmap.shr");
  unsigned start = (!LMAP_OLD && dend < resident && sceIoGetstat(lf, &st) >= 0) ? dend : 0;
  lvaInit(); prBuf = lvaAlloc(resident); if (!prBuf) return;
  snprintf(prPath, sizeof prPath, "%s", fn); prOff = start; prSize = resident; __asm__ volatile("" ::: "memory"); prState = 1; ldSignal();
  if (smkOn) { char s_[112]; snprintf(s_, sizeof s_, "PREREAD start off=%u size=%u t=%.1f\n", start, resident, smkT); smkWrite(s_); }
}
static void tailReadSync(void) {   // №LOADWAIT: тот же хвост COL1/PRP2, что читает загрузчик (main_stream.h), но в главном потоке
  tailCol = tailProp = 0; tailPropSz = 0; SceUID tf = sceIoOpen(levelShr, PSP_O_RDONLY, 0); if (tf < 0) return;
  if (tailColSz) { void *m = lvaAlloc(tailColSz); sceIoLseek32(tf, tailColOff, PSP_SEEK_SET); if (m && sceIoRead(tf, m, tailColSz) == (int)tailColSz) tailCol = m; else lvaFree(m); }
  if (tailPropOff) { unsigned char ph[64]; sceIoLseek32(tf, tailPropOff, PSP_SEEK_SET); int pn = sceIoRead(tf, ph, 64); unsigned sz = levelPropSize(ph, pn);
    if (sz) { void *m = lvaAlloc(sz); sceIoLseek32(tf, tailPropOff, PSP_SEEK_SET); if (m && sceIoRead(tf, m, sz) == (int)sz) { tailProp = m; tailPropSz = sz; } else lvaFree(m); } }
  sceIoClose(tf);
}
// №912: решение «заливка или рисунок» по каждой текстуре уровня считает Мак (tools/flat_list.py → flat912.bin в папке уровня; на PSP разбор текстур стоил бы ≈ 1,8 с загрузки).
// Формат FLT1: 'FLT1', ntex, подпись (FNV-1a по номерам текстур кусков, как у notoon.txt), по текстуре 8 байт {заливка, причина, R, G, B, доля, границы, 0}. Нет файла или подпись не та —
// разбор на месте (toonflat.h flatScan, прежний путь)
static void flatListLoad(const unsigned char *table, unsigned npc_) {
  curbNat1064 = curbWant1064;   // №1064: строка Curbs действует с загрузки уровня (заливки не пересобираются на ходу)
  flatFromFile = 0; flatLevelN = ntex; flatLevelGen++; free(zpMul); zpMul = 0; zpN = 0; zpNpc = 0; zpApplied = 0; if (flatOld || !tex || !ntex) return;
  char fn[256]; levelFile(fn, "flat912.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned hd[3]; int ok = sceIoRead(f, hd, 12) == 12 && hd[0] == 0x31544c46u && hd[1] == ntex;   // 'FLT1'
  unsigned h = 2166136261u; for (unsigned i = 0; ok && i < npc_; i++) { unsigned v; memcpy(&v, table + 80u * i + 24u, 4); h = (h ^ v) * 16777619u; }
  unsigned char *b = ok && h == hd[2] ? (unsigned char *)malloc(ntex * 8u) : 0;
  if (b && sceIoRead(f, b, (int)(ntex * 8u)) == (int)(ntex * 8u)) {
    for (unsigned i = 0; i < ntex; i++) { const unsigned char *r = b + i * 8; Tex *x = &tex[i];
      x->flatK = (r[1] == WALK1005_REASON && r[0] == 0 && !walk1005On) ? 1 : r[0]; x->flatR = r[1]; x->flatC = (unsigned)r[2] | (unsigned)r[3] << 8 | (unsigned)r[4] << 16; x->flatS = r[5]; x->flatEd = r[6]; x->curb = r[7] & 15; x->asph = r[7] >> 4; x->xdon = (r[4] == XWALK999_MARK && (r[1] == 3 || r[1] == 7) && (r[7] & 0x10)) ? (unsigned short)((r[2] | r[3] << 8) + 1) : 0; if (x->xdon > ntex) x->xdon = 0; x->flatDone = 1; }
    flatFromFile = 1; flatTintKey = ~0u;
  }
  free(b); sceIoClose(f); flatSig = h;
  if (flatFromFile) { char zf[256]; levelFile(zf, "zones1007p.bin"); SceUID z = sceIoOpen(zf, PSP_O_RDONLY, 0);   // №1007б: множитель цвета вершин по куску
    if (z >= 0) { unsigned zh[4]; if (sceIoRead(z, zh, 16) == 16 && zh[0] == 0x3730505au && zh[1] == npc_ && zh[2] == h && zh[3] <= npc_) { unsigned n = zh[3]; unsigned char *zb = n ? (unsigned char *)malloc(n * 8u) : 0; unsigned char *zm = (unsigned char *)calloc(npc_ + 1u, 4);
        if (zb && zm && sceIoRead(z, zb, (int)(n * 8u)) == (int)(n * 8u)) { for (unsigned i = 0; i < n; i++) { unsigned k; memcpy(&k, zb + i * 8u, 4); if (k < npc_) { zm[k * 4] = zb[i * 8 + 4]; zm[k * 4 + 1] = zb[i * 8 + 5]; zm[k * 4 + 2] = zb[i * 8 + 6]; zm[k * 4 + 3] = 1; } } zpMul = zm; zpN = n; zpNpc = npc_; zm = 0; }
        free(zb); free(zm); }
      sceIoClose(z); }
    if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "ZONES1007P level=%d pieces=%u on=%d\n", gameLevel, zpN, zn7On); smkWrite(l_); } }
  zn7N = 0; if (flatFromFile) { char zf[256]; levelFile(zf, "zones1007.bin"); SceUID z = sceIoOpen(zf, PSP_O_RDONLY, 0);   // №1007: пары «заливка у асфальта → тон соседа»
    if (z >= 0) { unsigned zh[3]; if (sceIoRead(z, zh, 12) == 12 && zh[0] == 0x37304e5au && zh[1] <= 256 && zh[2] == h) { unsigned n = zh[1]; unsigned char zb[256 * 8];
        if (n && sceIoRead(z, zb, (int)(n * 8u)) == (int)(n * 8u)) { for (unsigned i = 0; i < n; i++) { zn7E[i].tex = (unsigned short)(zb[i * 8] | zb[i * 8 + 1] << 8); zn7E[i].tgt = (unsigned short)(zb[i * 8 + 2] | zb[i * 8 + 3] << 8); zn7E[i].mix = zb[i * 8 + 4]; } zn7N = n; } }
      sceIoClose(z); } }
  if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "FLATLIST level=%d ok=%d ntex=%u file=%u/%08x sig=%08x\n", gameLevel, flatFromFile, ntex, ok ? hd[1] : 0, ok ? hd[2] : 0, h); smkWrite(l_);
    unsigned k_ = 0, n_ = 0; for (unsigned i_ = 0; flatFromFile && i_ < ntex; i_++) if (tex[i_].flatR == WALK1005_REASON) { n_++; k_ += tex[i_].flatK == 0; }
    snprintf(l_, sizeof l_, "WALK1005 level=%d on=%d rows=%u native=%u\n", gameLevel, walk1005On, n_, k_); smkWrite(l_); }
}
// №949: масштаб шума земли по кускам (tools/noise949_list.py → noise949.bin рядом с level.shr). Формат NZ49: 'NZ49', npc, подпись (та же FNV-1a, что у flat912.bin), npc × {u, v}.
// Нет файла или подпись не та — у кусков nz = 0: шум по родным UV, как было
static void nzLoad(const unsigned char *table, unsigned npc_) {
  nzFileOk = 0; nzPieces = 0; nzCur = -1; nzPend = 0; if (!nzOn || !pc) return;
  char fn[256]; levelFile(fn, "noise949.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned hd[3]; int ok = sceIoRead(f, hd, 12) == 12 && hd[0] == 0x39345a4eu && hd[1] == npc_;   // 'NZ49'
  unsigned h = 2166136261u; for (unsigned i = 0; ok && i < npc_; i++) { unsigned v; memcpy(&v, table + 80u * i + 24u, 4); h = (h ^ v) * 16777619u; }
  unsigned char *b = ok && h == hd[2] ? (unsigned char *)malloc(2u * npc_) : 0;
  if (b && sceIoRead(f, b, (int)(2u * npc_)) == (int)(2u * npc_)) {
    nzInit(); for (unsigned i = 0; i < npc_; i++) { unsigned c = (unsigned)b[2 * i] | (unsigned)b[2 * i + 1] << 8; pc[i].nz = c; nzPieces += c != 0; }
    nzFileOk = 1; nzTabN = npc_; }
  free(b); sceIoClose(f);
  if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "NOISE949 level=%d ok=%d file=%u/%08x sig=%08x pieces=%u\n", gameLevel, nzFileOk, ok ? hd[1] : 0, ok ? hd[2] : 0, h, nzPieces); smkWrite(l_); }
}
// №912: разметка дорог слоем (toonflat.h, tools/marks_list.py): файл marks912.bin рядом с level.shr, открыт на время уровня; вершины клеток читает загрузчик клеток
// №918 (Роб 24.10: «дорогу с разметкой верни; не можешь воссоздать — ставь текстуры родные; самое дешёвое, что похоже на оригинал»): слой разметки выключен —
// асфальт с разметкой (вид 3) рисуется родной текстурой (flatKind: без marksOk вид 3 = текстура), заодно №915 зебра. MARKS918_OLD=1 / flag marks918old 1 — слой как в TEST57
#ifndef MARKS918_OLD
#define MARKS918_OLD 0
#endif
static int marksLayerOld = MARKS918_OLD;
static void marksLoad(void) {
  marksClose(); if (flatOld || !flatFromFile || !ncell || !marksLayerOld) return;
  char fn[256]; levelFile(fn, "marks912.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned hd[5]; int ok = sceIoRead(f, hd, 20) == 20 && hd[0] == 0x324b524du && hd[1] == (flatLevelN ? flatLevelN : ntex) && hd[2] == flatSig && hd[3] == ncell && hd[4] <= 16;   // 'MRK1'
  unsigned *tab = ok ? (unsigned *)malloc(ncell * 12u) : 0; unsigned char col[64];
  if (tab && sceIoRead(f, tab, (int)(ncell * 12u)) == (int)(ncell * 12u) && sceIoRead(f, col, (int)(hd[4] * 4u)) == (int)(hd[4] * 4u)) {
    marksNcls = hd[4]; for (unsigned k = 0; k < marksNcls; k++) { marksRgb[k][0] = col[k * 4]; marksRgb[k][1] = col[k * 4 + 1]; marksRgb[k][2] = col[k * 4 + 2]; }
    marksTab = tab; marksNcell = ncell; marksHdr = 20u + ncell * 12u + hd[4] * 4u; marksCell = (MkCell *)calloc(ncell, sizeof(MkCell)); marksSema = sceKernelCreateSema("marks", 0, 1, 1, 0);
    marksFd = f; marksOk = marksCell && marksSema >= 0; tab = 0; f = -1; }
  free(tab); if (f >= 0) sceIoClose(f);
  if (!marksOk) marksClose();
  marksEpoch++; marksToneBuild();
  if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "MARKSLIST level=%d ok=%d ncell=%u ncls=%u\n", gameLevel, marksOk, ncell, marksNcls); smkWrite(l_); }
}
// №754: средний цвет текстуры и разброс яркости по её точкам (верхний уровень, у крупных — каждая третья): для стягивания палитры в мультике (tone.c toneSetStat).
// Разброс 10 и меньше — почти ровная заливка (10 % рисунка), 50 и больше — 60 % рисунка; прозрачные точки (вырезы) не в счёт. Без точек — tstat[3] = 0 (не стягивать)
static void texToonStat(Tex *t, const unsigned char *pal) {   // pal — ИСХОДНАЯ палитра (до кривой тона)
  memset(t->tstat, 0, 4); t->tstatOk = 1; if (!flatFromFile) { t->flatK = 0; t->curb = 0; t->asph = 0; t->flatC = 0; t->flatDone = !flatOld; } const unsigned char *px = (const unsigned char *)t->lv[0]; if (!px || !pal || t->levels < 1) return;   // tstat[4] (№786, облака) не трогается
  unsigned rb = (unsigned)texRowBytes(t->t4, t->pre, t->w), rows = t->pre ? ((unsigned)t->h + 7) & ~7u : (unsigned)t->h, n = rb * rows, step = n > 4096 ? 3 : 1, cnt[256];
  memset(cnt, 0, sizeof cnt);
  if (t->t4) for (unsigned i = 0; i < n; i += step) { cnt[px[i] & 15]++; cnt[px[i] >> 4]++; } else for (unsigned i = 0; i < n; i += step) cnt[px[i]]++;
  float sr = 0.f, sg = 0.f, sb = 0.f, sl = 0.f, sll = 0.f, sw = 0.f;
  for (int j = 0; j < 256; j++) { if (!cnt[j]) continue; const unsigned char *c = pal + j * 4; if (c[3] < 16) continue;
    float w = (float)cnt[j], l = .299f * c[0] + .587f * c[1] + .114f * c[2]; sr += w * c[0]; sg += w * c[1]; sb += w * c[2]; sl += w * l; sll += w * l * l; sw += w; }
  if (sw <= 0.f) return;
  float m = sl / sw, var = sll / sw - m * m, sd = var > 0.f ? sqrtf(var) : 0.f, k = (sd - 10.f) / 40.f; k = k < 0.f ? 0.f : k > 1.f ? 1.f : k; k = .10f + k * .50f;
  t->tstat[0] = (unsigned char)(sr / sw + .5f); t->tstat[1] = (unsigned char)(sg / sw + .5f); t->tstat[2] = (unsigned char)(sb / sw + .5f); t->tstat[3] = (unsigned char)(k * 255.f + .5f);
  flatScan(t, pal, cnt);
}
// №775 тест («flag fragheap N», только сценарий): куча как после долгой игры на PSP — ящик TEST33/TEST27 перед загрузкой L5/L6: свободно 39 МБ, наибольший
// кусок 11,4 / 9,5 МБ. Здесь вся свободная память дробится так, что наибольший кусок ≤ N МБ: островки по 256 КБ остаются навсегда (как долгоживущие выделения)
#ifndef FE_MEM841_OLD
#define FE_MEM841_OLD 0
#endif
// №841: гостиная меню (fe.shr 5,2 МБ одним куском) при выгруженном уровне — в участок уровня lva (17,8 МБ, выделен при первой загрузке и не дробится):
// тур TEST36 на PSP — после долгой игры наибольший кусок кучи даже без уровня 4,1–6,5 МБ. Участок пуст (lvaUsed 0) только без уровня; выход из меню
// освобождает гостиную (feFree) раньше загрузки уровня, бонус-игра — раньше трассы
static void *feMemAlloc841(unsigned n) {
  if (!FE_MEM841_OLD && lvaBase && !loadedLevel && !lvaUsed) { void *p = lvaAlloc(n); if (lvaOwns(p)) return p; free(p); }
  return memalign(64, n);
}
static void feMemFree841(void *p) { lvaFree(p); }   // не из участка — free
static unsigned fragHeapMB, fragMenuMB;   // fragMenuMB — №841 тест: «flag fragmenu N» перед гостиной меню
static void heapLeft(float mb) {   // Б TEST57 тест: «flag heapleft N» — прежнее занятое отдаётся, затем куча занимается блоками 64 КБ, пока свободно больше N МБ (N больше свободного — только отдать)
  static void *volatile hog_[1024]; static unsigned nh_;
  while (nh_) free(hog_[--nh_]);
  unsigned want = (unsigned)(mb * 1048576.f);
  while (heapFreeBytes() > want && nh_ < 1024u) { void *q = malloc(64u << 10); if (!q) break; hog_[nh_++] = q; }   // указатель сохранён: пустой malloc компилятор выбрасывает
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "HEAPLEFT want=%.2f got=%.2f blocks=%u t=%.1f\n", mb, heapFreeBytes() / 1048576.f, nh_, smkT); smkWrite(s_); }
}
static void fragHeap(unsigned maxBytes) {
  const unsigned PIN = 512u << 10;   // островок; все свободные куски не меньше него сначала заняты — островок встаёт ровно за своей частью
  static void *keep[256]; int nk = 0, nb = 0, pins = 0; void *blk[96]; unsigned sz[96];
  for (; nb < 96; nb++) { unsigned m = memMaxBlock(); if (m < PIN) break; m -= 64u; blk[nb] = malloc(m); if (!blk[nb]) break; sz[nb] = m; }
  for (int i = 0; i < nb; i++) {
    if (sz[i] <= maxBytes) { if (nk < 256) keep[nk++] = blk[i]; else free(blk[i]); continue; }
    free(blk[i]); unsigned left = sz[i];
    while (left > maxBytes && nk < 256) { void *pt = malloc(maxBytes - PIN); if (!pt) break; keep[nk++] = pt; if (malloc(PIN)) pins++; left -= maxBytes; }
    { unsigned m = memMaxBlock(); if (m >= PIN && nk < 256) { void *r = malloc(m - 64u); if (r) keep[nk++] = r; } }   // остаток этого куска занят до конца: островки следующего не уйдут в него
  }
  for (int k = 0; k < nk; k++) free(keep[k]);
  if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "FRAGHEAP max=%uMB pins=%d heap=%u blk=%u\n", maxBytes >> 20, pins, heapFreeBytes(), memMaxBlock()); smkWrite(s_); }
}
// №863 (Роб 23.10: «L3 проседает — что там полупрозрачное?»; «размножить одну картинку моря вдаль»): море L3/L6 (текстура волн) помечено в данных
// полупрозрачным и рисуется ближней версией (нарезка 4,4 м) до far_plain (140 м): на набережной L3 TEST46 — 20 к/с, 50 тыс. треугольников, из них ≈15 тыс. море.
// У кусков моря есть дальняя версия (исходная сетка PS2, рёбра до 28 м): 4 575 треугольников против 44 737 — та же картинка волн крупными плитками.
// Список кусков «полупрозрачное без прозрачности» (все вершины и картинка непрозрачны) печёт Мак (tools/opq863.py, opq863.bin в папке уровня: 'OPQ1',
// подпись таблицы кусков, n, индексы). Такой кусок берёт дальнюю версию на ЛЮБОЙ дальности, если его ящик целиком в безопасной пирамиде полосы GE
// (ggBoxRiskW: GE нарисует все треугольники целиком); у воды под камерой — ближняя, как раньше. Нет файла — как было. OPQ863_OLD=1 — файл не читается
#ifndef OPQ863_OLD
#define OPQ863_OLD 0
#endif
// №907-город (дирижёр 23.10: «куски ГОРОДА с флагом 2 при непрозрачной текстуре — то же правило, что у пропов»): тот же список opq863.bin (все вершины ближней и дальней версии и вся картинка
// непрозрачны; вершины городских клеток при загрузке не в памяти — стримятся, поэтому признак печёт Мак) даёт и второе: у таких кусков флаг 2 (смешивание) снимается при загрузке уровня.
// Кусок перестаёт идти в очередь полупрозрачного (сортировка по расстоянию, без записи глубины, порядок треугольников по файлу: у закрытых и многогранных кусков видна изнанка задних граней —
// L1 частокол у озера) и рисуется непрозрачным с записью глубины, листами клеток. Исключение — «листы» моря L3/L6 (дальняя версия втрое и более дешевле ближней, у остальных не дешевле 0,43):
// их выбор дальней версии (№863) работает только на пути очереди (мировая матрица), на обычном пути он отваливается (опыт 23.10: opqfar 11194/2490 -> 237/1404, ktris L3 +30%), и листы клеток
// строили бы море ближней версией. Они остаются как были.
// Данные уровня не меняются. CITYOPQ_OLD=1 / файл cityopqold рядом с EBOOT — прежнее (флаг остаётся)
#ifndef CITYOPQ_OLD
#define CITYOPQ_OLD 0
#endif
static int cityOpqOn = !CITYOPQ_OLD; static unsigned cityOpqN, cityOpqTris;
static unsigned opqFarN;   // opqFar[k] (main_render.h) — кусок k: дальняя версия при безопасном ящике; счётчики кадров — в строке GGSTAT (opqfar)
static void opq863Apply(const unsigned char *tab) {
  free(opqFar); opqFar = 0; opqFarN = 0; cityOpqN = cityOpqTris = 0;
  if (OPQ863_OLD || !npc) return;
  char fn[256]; levelFile(fn, "opq863.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned h[3] = {0, 0, 0}, n = 0, tris = 0, trisF = 0, idx[256];
  if (sceIoRead(f, h, 12) == 12 && !memcmp(h, "OPQ1", 4) && h[1] == pvs2Sig(tab, npc) && (opqFar = (unsigned char *)calloc(npc, 1))) {
    unsigned left = h[2];
    while (left) { unsigned k = left < 256 ? left : 256; if (sceIoRead(f, idx, (int)(k * 4)) != (int)(k * 4)) break; left -= k;
      for (unsigned j = 0; j < k; j++) { unsigned i = idx[j]; n++; if (i >= npc) continue; Rec *r = &pc[i].r;
        if (r->owner != 0 || !(r->flags & 2) || (r->flags & (4 | F_ADDITIVE)) || !texOpaque(r->tex)) continue;
        int farSheet = (r->flags & F_FARP) && r->nf && (unsigned)r->nf * 10u <= (unsigned)r->ni * 3u;   // дальняя версия втрое и более дешевле: «лист» моря L3/L6 (отношение 0,05–0,3), у предметов 0,43–1,0
        if (!cityOpqOn || farSheet) { if ((r->flags & F_FARP) && r->nf) { opqFar[i] = 1; opqFarN++; tris += (unsigned)r->ni / 3; trisF += r->nf / 3; } }
        else { r->flags &= ~2; cityOpqN++; cityOpqTris += (unsigned)r->ni / 3; } } }   // №907-город: смешивание пустое — кусок непрозрачный   // №907-город: смешивание пустое — кусок непрозрачный
  }
  sceIoClose(f);
  if (smkOn) { char s_[140]; snprintf(s_, sizeof s_, "OPQ863 level=%d sig=%d list=%u pieces=%u tris_near=%u tris_far=%u cityopq=%d pieces_unblend=%u tris_unblend=%u\n", gameLevel, h[1] == pvs2Sig(tab, npc), n, opqFarN, tris, trisF, cityOpqOn, cityOpqN, cityOpqTris); smkWrite(s_); }
}
static void levelLoad(void **fbp) {
  lvaInit();   // №775: участок под три крупных куска уровня — при первой загрузке игры (куча ещё чистая), дальше тот же
  if (fragHeapMB) { fragHeap(fragHeapMB << 20); fragHeapMB = 0; }   // №775 тест: «flag fragheap N» — один раз, перед игроком уровня
  skyHorizonInvalidate();   // a new sky mesh, texture, or palette must be sampled
  BBP(BBP_LOAD);   // правка 103: сторож — долгое чтение уровня видно как фаза load, а не как зависание кадра
  phT0 = sceKernelGetSystemTimeLow();
  if (memLoad1065) { levelUiSnd(); phMark("ui+sfx"); }   // №1065: самые крупные долгоживущие блоки (ui.shr 1 МБ, банк звука до 2,15 МБ) — первыми, пока куча пуста (раньше после игрока и в раздробленной куче: TEST77 L5 без звука)
  playerLoad(); phMark("player");    // правка 352: игрок уровня — до набора уровня (пакет ложится в кучу раньше клеток города)
  unsigned lt0 = sceKernelGetSystemTimeLow(), lh0 = smkOn ? heapFreeBytes() : 0; lvMx0 = smkOn ? memMaxBlock() : 0;
  levelFile(levelShr, "level.shr");
  levelToonLoaded = 0;
  if (levelToonWanted && !TOONLINES_OLD && LEVEL_TOON_FILE_OLD) { char tf_[256]; SceIoStat st_; levelFile(tf_, "level_toon.shr"); if (sceIoGetstat(tf_, &st_) >= 0) { snprintf(levelShr, sizeof levelShr, "%s", tf_); levelToonLoaded = 1; } }   // №679: мир «мультик» — отдельный файл уровня (заливки + ступень света), выбор при загрузке уровня; коллизия та же (COL1 побайтно)
  if (!memLoad1065) { levelUiSnd(); phMark("ui+sfx"); }
  if (!FARPLAIN_OLD) {   // дальние версии кусков по уровню: файл far_plain в папке уровня (число — D, м); нет файла — как задано рядом с EBOOT (по умолчанию 0 — дальних версий нет)
    static float farPlainBoot = -1.f; if (farPlainBoot < 0.f) farPlainBoot = farPlainD;
    char fn_[256], b_[16]; levelFile(fn_, "far_plain"); SceUID o_ = sceIoOpen(fn_, PSP_O_RDONLY, 0); farPlainD = farPlainBoot;
    if (o_ >= 0) { int n_ = sceIoRead(o_, b_, 15); sceIoClose(o_); b_[n_ > 0 ? n_ : 0] = 0; farPlainD = n_ > 0 ? strtof(b_, 0) : 100.f; if (farPlainD < 60.f) farPlainD = 100.f; if (farPlainD > 240.f) farPlainD = 240.f; }
    if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "FARPLAIN level=%d d=%.0f file=%d\n", gameLevel, farPlainD, o_ >= 0); smkWrite(l_); }
  }
  toonLinesLoad();   // №679: контур мультика (файл toonlines.shr уровня, если есть)
  // резидентная часть: заголовок, карта высот, текстуры, таблицы кусков и клеток, PVS
  SceUID fd = sceIoOpen(levelShr, PSP_O_RDONLY, 0);
  if (fd < 0) { char m[200]; snprintf(m, sizeof m, "Level %d: %s not found.", gameLevel, levelShr); fail(m); }
  unsigned char hdr[104];   // заголовок SHR7 104 Б (@100 — смещение CAR1)
  if (sceIoRead(fd, hdr, 104) != 104 || memcmp(hdr, "SHR7", 4)) fail("level.shr is an old version. Run the export again.");
  ntex = *(unsigned *)(hdr + 4); npc = *(unsigned *)(hdr + 8);
  float *hd = (float *)(hdr + 12);
  float spx = hd[6], spy = hd[7], spz = hd[8], sph = hd[9]; carLift = hd[10];   // подъём кузова машины уровня (рисование)
  spawnP[0] = spx; spawnP[1] = spy; spawnP[2] = spz; spawnP[3] = sph;   // 12а: NEW GAME и загрузка города после меню
  hox = hd[11]; hoz = hd[12]; hcell = hd[13]; hw = *(int *)(hd + 14); hh = *(int *)(hd + 15);
  unsigned tableOff = *(unsigned *)(hdr + 76);
  ncell = *(unsigned *)(hdr + 80); unsigned cellOff = *(unsigned *)(hdr + 84), pvsOff = *(unsigned *)(hdr + 88);
  unsigned residentFull = cellOff + 48 * ncell, residentSize = (!LEVEL_TAIL_OLD && tableOff > 104 && tableOff < residentFull) ? tableOff : residentFull;   // residentSize — то, что остаётся в памяти (до таблицы кусков)
  unsigned char *data;
  if (prBuf && prSize == residentSize) { data = prBuf; }   // №PREREAD: буфер уже выделен и читается загрузчиком
  else { if (prBuf) { while (prState == 1) sceKernelDelayThread(1000); lvaFree(prBuf); } prBuf = 0; prState = 0; data = lvaAlloc(residentSize); }
  levelData = data;
  if (!data) {
    char m[160]; snprintf(m, sizeof m, "Not enough memory: %.1f MB needed, %.1f MB free.",
      residentSize / 1048576.f, sceKernelMaxFreeMemSize() / 1048576.f); fail(m);
  }
  unsigned off0 = 0, lmapMs = 0;   // загрузка 2 п.2: карта высот и стены из lmap.shr (сжатые), плотный массив в level.shr пропускается
  if (!LMAP_OLD) {
    unsigned dend = lmapDenseEnd((unsigned)hw, (unsigned)hh), lt = sceKernelGetSystemTimeLow();
    char fn[256]; levelFile(fn, "lmap.shr"); SceUID lf = dend < residentSize ? sceIoOpen(fn, PSP_O_RDONLY, 0) : -1;
    if (lf >= 0) {
      LmapHead lh; unsigned fsz = (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END);
      if (sceIoRead(lf, &lh, sizeof lh) == (int)sizeof lh && !memcmp(lh.magic, LMAP_MAGIC, 4) && lh.ver == 1 && lh.key == lmapKey(hdr, fsz) &&
          lh.denseBytes == dend - 104 && lh.compBytes && lh.compBytes < (16u << 20)) {
        unsigned char *cb = memalign(16, lh.compBytes);
        if (cb && sceIoRead(lf, cb, lh.compBytes) == (int)lh.compBytes && lmapUnpack(cb, lh.compBytes, data + 104, lh.denseBytes) == 0 &&
            lmapHash(data + 104, lh.denseBytes) == lh.denseHash) { memcpy(data, hdr, 104); off0 = dend; }
        free(cb);
      }
      sceIoClose(lf); lmapMs = (sceKernelGetSystemTimeLow() - lt) / 1000;
    }
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "LMAP level=%d used=%d ms=%u\n", gameLevel, off0 != 0, lmapMs); smkWrite(s); }
  }
  int preUsed = 0; unsigned preMs = 0;
  if (prBuf == data) { unsigned pt_ = sceKernelGetSystemTimeLow(); LdWait lw_ = {0}; while (prState == 1) { loaderWaitStep(&lw_, "preread", 0, 0); if (ldOn) loadPump(); sceKernelDelayThread(1000); } preMs = (sceKernelGetSystemTimeLow() - pt_) / 1000u;
    preUsed = prState == 2 && off0 == prOff; prBuf = 0; prState = 0; }   // готово и то же начало, что у главного (lmap принят) — читать не надо
  if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "PREREAD use=%d wait_ms=%u off0=%u t=%.1f\n", preUsed, preMs, off0, smkT); smkWrite(s_);
    if (preUsed && prereadSum) { char l_[96]; snprintf(l_, sizeof l_, "PREREAD sum off=%u size=%u sum=%08x\n", off0, residentSize - off0, pfCrc(data + off0, residentSize - off0)); smkWrite(l_); } }
  sceIoLseek32(fd, off0, PSP_SEEK_SET);
  if (preUsed) { if (ldOn) loadFrame(1 * LOAD_RES_SHARE / 6, fbp); }
  else
  for (unsigned off = off0; off < residentSize; ) {   // кусками по 512 КБ: между ними кадр экрана загрузки (огонь и кадры не стоят)
    unsigned n = residentSize - off; if (n > (512u << 10)) n = 512u << 10;
    if (sceIoRead(fd, data + off, n) != (int)n) fail("Read error.");
    off += n; if (ldOn) loadFrame((int)((unsigned long long)off * (LOAD_RES_SHARE / 6) / residentSize), fbp);
  }
  phMark("resident");
  const unsigned char *tab, *cellTab; unsigned char *tabBuf = 0;   // таблица кусков и таблица клеток: во временном буфере (или в участке, LEVEL_TAIL_OLD)
  if (residentSize == residentFull) { tab = data + tableOff; cellTab = data + cellOff; }
  else { unsigned tabSz = residentFull - tableOff;
    unsigned co_ = *(unsigned *)(hdr + 92), pr_ = *(unsigned *)(hdr + 96), en_ = pvsOff && (!pr_ || pvsOff < pr_) ? pvsOff : pr_, colSz_ = (co_ && en_ > co_) ? en_ - co_ : 0;
    tabBuf = (unsigned char *)(lvaRoom(tabSz + colSz_ + (2u << 20)) ? lvaAlloc(tabSz) : memalign(16, tabSz));   // L2, L3, L5–L7: в участке на 4–5 МБ больше нужного место есть — хвост в него, куча не дробится; L1/L4 — в кучу на время загрузки
    if (!tabBuf) { char m[96]; snprintf(m, sizeof m, "Not enough memory: %.1f MB for tables.", tabSz / 1048576.f); fail(m); }
    sceIoLseek32(fd, tableOff, PSP_SEEK_SET); if (sceIoRead(fd, tabBuf, tabSz) != (int)tabSz) fail("Read error.");
    tab = tabBuf; cellTab = tabBuf + (cellOff - tableOff);
    if (smkOn) { char s_[112]; snprintf(s_, sizeof s_, "LEVELTAIL tab=%u keep=%u inlva=%d t=%.1f\n", tabSz, residentSize, lvaOwns(tabBuf), smkT); smkWrite(s_); } }
  if (ldOn) loadFrame(1 * LOAD_RES_SHARE / 6, fbp);   // правка Роба 3: огонь по фитилю Loading.scr идёт по настоящему чтению уровня
  unsigned propOff = *(unsigned *)(hdr + 96);
  // загрузка 2 п.3: секции читаются в порядке файла (tools/shr_order.py: COL1, PRP2, CAR1, REG1, PVS1 подряд за резидентной частью, потом клетки);
  // LOADORDER_OLD=1 — прежний порядок чтения PVS1, REG1, COL1, PRP2, CAR1 (для файлов прежней раскладки он же и возрастающий)
  int tailBg = 0;   // загрузка 2 п.6: COL1 и PRP2 читает загрузчик (разбор — после таблиц текстур/кусков/клеток, tailFinish ниже)
  { unsigned colOff_ = *(unsigned *)(hdr + 92);
    if (!TAILBG_OLD && loaderUp && colOff_ && propOff && !tailState) {
      unsigned end = 0; if (pvsOff && (!end || pvsOff < end)) end = pvsOff; if (propOff && (!end || propOff < end)) end = propOff;
      if (end > colOff_) { tailColOff = colOff_; tailColSz = end - colOff_; tailPropOff = propOff; tailCol = tailProp = 0; tailPropSz = 0; tailColAl = tailPropAl = 0; tailBg = 1;
        __asm__ volatile("" ::: "memory"); tailState = 1; ldSignal(); } } }
  if (!pfOld && loaderUp) { char fn_[256], nm_[32], px_[304]; pfTakeHook = pfTake;   // №PFETCH: целые файлы — следом за хвостом
    levelFile(fn_, "traffic.shr"); pfQueue(fn_, "traffic.shr");
    if (gameLevel > 1) snprintf(nm_, sizeof nm_, "l%d/fx.shr", gameLevel); else strcpy(nm_, "fx.shr");
    if (getcwd(px_, 256)) { size_t l_ = strlen(px_); if (l_ && px_[l_ - 1] != '/') strcat(px_, "/"); strcat(px_, nm_); pfQueue(px_, nm_); } }
  { static const unsigned char seqNew[5] = {2, 3, 4, 1, 0}, seqOld[5] = {0, 1, 2, 3, 4};
    for (int ph = 0; ph < 5; ph++) { int step_ = LOADORDER_OLD ? seqOld[ph] : seqNew[ph]; if (tailBg && (step_ == 2 || step_ == 3)) continue; switch (step_) {
    case 0: {
    if (pvsOff) {
      unsigned char ph[32];
      sceIoLseek32(fd, pvsOff, PSP_SEEK_SET);
      if (sceIoRead(fd, ph, 32) == 32 && !memcmp(ph, "PVS1", 4)) {
        float *pf = (float *)(ph + 4); unsigned *pu = (unsigned *)(ph + 16);
        pvsOx = pf[0]; pvsOz = pf[1]; pvsCell = pf[2]; pvsW = pu[0]; pvsH = pu[1];
        if (!PVS_FAR_OLD && pu[3] >= 115 && pu[3] <= 1000) pvsRange = (float)pu[3];   // правка 497: дальность списков из заголовка (0 — 115)
        unsigned sz = pvsW * pvsH * 8 + pu[2] * 2;
        void *m = lvaAllocSoft(sz, 0, tailBg);   // №962: запас участка, если COL1/PRP2 уже или заведомо влезут
        if (m && sceIoRead(fd, m, sz) == (int)sz) { pvsEnt = m; pvsPool = (unsigned short *)((char *)m + pvsW * pvsH * 8); }
        else if (m) lvaFree(m);
        if (!PVS_SECTOR_OLD && pvsEnt) {   // PV2S (tools/pvs2.py): PVS по курсу в горячих клетках, отдельный файл pv2s.shr рядом с уровнем (как lmap.shr); нет файла / подпись другого экспорта — PVS1 как был
          char fn2_[256]; levelFile(fn2_, "pv2s.shr"); SceUID f2_ = sceIoOpen(fn2_, PSP_O_RDONLY, 0);
          if (f2_ >= 0) { unsigned h2[8] = {0};
            if (sceIoRead(f2_, h2, 32) == 32 && !memcmp(h2, "PV2S", 4) && h2[2] == 8 && h2[1] <= 65536u && h2[3] <= (1u << 24) && h2[4] == pvs2Sig(tab, npc)) {
              unsigned sz2 = 32u + 4u * h2[1] + 64u * h2[1] + 2u * h2[3]; void *m2 = lvaAllocSoft(sz2, 0, tailBg);
              sceIoLseek32(f2_, 0, PSP_SEEK_SET);
              if (m2 && sceIoRead(f2_, m2, sz2) == (int)sz2 && pvs2Init(&pv2, m2, sz2, h2[4])) pv2Mem = m2; else if (m2) lvaFree(m2); }
            if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "PV2LOAD level=%d file=1 hdr=%.4s nhot=%u nsec=%u sig=%08x want=%08x ok=%d\n", gameLevel, (const char *)h2, h2[1], h2[2], h2[4], pvs2Sig(tab, npc), pv2.ok); smkWrite(s_); }
            sceIoClose(f2_); }
        }
      }
    }
    } break;
    case 1: {
    { // правка 95: REG1 — хвост 'REGF' (смещение, размер) прямо перед PVS1, а без PVS — в конце файла (tools/regions_export.py)
      unsigned end = pvsOff ? pvsOff : (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END); unsigned char ft[16], rh[32];
      if (end >= 48 && sceIoLseek32(fd, end - 16, PSP_SEEK_SET) == (int)(end - 16) && sceIoRead(fd, ft, 16) == 16 && !memcmp(ft, "REGF", 4)) {
        unsigned at = *(unsigned *)(ft + 4);
        if (at + 32 <= end && sceIoLseek32(fd, at, PSP_SEEK_SET) == (int)at && sceIoRead(fd, rh, 32) == 32 && !memcmp(rh, "REG1", 4)) {
          unsigned *ru = (unsigned *)(rh + 4); float *rf = (float *)(rh + 20);
          unsigned nreg = ru[1], w = ru[2], h = ru[3], sz = w * h * 2, mo = at + 32 + 16 * nreg;
          if (nreg && nreg <= 16 && w && h && w < 4096 && h < 4096 && mo + sz <= end) {
            void *m = memalign(16, sz);
            if (m && sceIoLseek32(fd, mo, PSP_SEEK_SET) == (int)mo && sceIoRead(fd, m, sz) == (int)sz) {
              regMask = m; regW = (int)w; regH = (int)h; regOx = rf[0]; regOz = rf[1]; regCell = rf[2] > 0.f ? rf[2] : 20.f;
              unsigned go = (mo + sz + 15u) & ~15u; unsigned char gh[16];   // правка 181: GAT1 за маской (версия 2)
              if (ru[0] >= 2 && go + 16 <= end && sceIoLseek32(fd, go, PSP_SEEK_SET) == (int)go && sceIoRead(fd, gh, 16) == 16 && !memcmp(gh, "GAT1", 4)) {
                unsigned ng = *(unsigned *)(gh + 4), np = *(unsigned *)(gh + 8);
                if (ng && ng <= ZG_MAX && np <= ZG_PLACES && go + 16 + ng * sizeof(ZoneGate) + np * sizeof(ZonePlace) <= end) {
                  zg = malloc(ng * sizeof(ZoneGate)); zp = malloc((np ? np : 1) * sizeof(ZonePlace));
                  if (zg && zp && sceIoRead(fd, zg, ng * sizeof(ZoneGate)) == (int)(ng * sizeof(ZoneGate)) &&
                      (!np || sceIoRead(fd, zp, np * sizeof(ZonePlace)) == (int)(np * sizeof(ZonePlace)))) {
                    nzg = (int)ng; nzp = (int)np; zgOn = 1; zgInit = 0; zgStart = 1; }
                  else { free(zg); free(zp); zg = 0; zp = 0; }
                }
              }
            }
            else if (m) free(m);
          }
        }
      }
      if (smkOn) { char s[112]; snprintf(s, sizeof s, "REG1 %s %dx%d cell %.0f gates %d places %d\n", regMask ? "on" : "none", regW, regH, regCell, nzg, nzp); smkWrite(s); }
    }
    } break;
    case 2: {
    { unsigned colOff = *(unsigned *)(hdr + 92);
      if (colOff) {
        // граница COL1 — следующая секция по файлу (PVS1 и/или PRP1, какая ближе), иначе конец файла
        unsigned end = 0;
        if (pvsOff && (!end || pvsOff < end)) end = pvsOff;
        if (propOff && (!end || propOff < end)) end = propOff;
        if (!end) end = (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END);
        unsigned sz = end - colOff; void *m = lvaAlloc(sz);
        sceIoLseek32(fd, colOff, PSP_SEEK_SET);
        if (m && sceIoRead(fd, m, sz) == (int)sz) levelColAdopt(m, sz);
        else if (m) lvaFree(m);
      } }
    } break;
    case 3: {
    if (propOff) {
      unsigned char ph[64];
      sceIoLseek32(fd, propOff, PSP_SEEK_SET);
      int pn = sceIoRead(fd, ph, 64);
      if (pn >= 32 && (!memcmp(ph, "PRP1", 4) || (pn == 64 && !memcmp(ph, "PRP2", 4)))) {
        unsigned *pu = (unsigned *)(ph + 4);
        unsigned sz = 32 + pu[3] + pu[4] + pu[0] * (unsigned)sizeof(PropPiece) + pu[1] * (unsigned)sizeof(PropType) +
                      pu[2] * (unsigned)sizeof(PropDef) + pu[6] * 12u;   // 32=заголовок; далее vertBytes+idxBytes+кус.+тип.+экз.+монеты
        if (!memcmp(ph, "PRP2", 4)) sz = *(unsigned *)(ph + 40);   // PRP2: @40 — размер всей секции (PRP1 + SPR1)
        void *m = lvaAlloc(sz);
        sceIoLseek32(fd, propOff, PSP_SEEK_SET);
        if (m && sceIoRead(fd, m, sz) == (int)sz) levelPropAdopt(m, sz);
        else if (m) lvaFree(m);
      }
    }
    } break;
    case 4: {
    { unsigned carOff = *(unsigned *)(hdr + 100);   // CAR1 (машина): после PRP1, до PVS1; секция < 4 КБ
      if (carOff) {
        static unsigned char cb[4096]; unsigned sz = sizeof cb;
        if (pvsOff > carOff && pvsOff - carOff < sz) sz = pvsOff - carOff;
        sceIoLseek32(fd, carOff, PSP_SEEK_SET);
        int n = sceIoRead(fd, cb, sz);
        if (n > 0 && carVisInit(&cv, cb, (unsigned)n) == 0) haveCarVis = 1;
        if (n > 0) lvlCarOk = carParamsFromCar1(cb, (unsigned)n, &lvlCar);   // машина уровня: имя, ходовые .con, кузов, посадка, камера
        if (haveCarVis) carVisSetCar(&cv, lvlCarOk == 1 ? lvlCar.name : "famil_v");   // правка 354: скрытые колёса, запас хода
        if (n > 0) lvlLightOk = levLightParse(cb, (unsigned)n, &lvlLight);   // свет уровня (LGT1): персонажам — skinLightLevel
        levelLightApply(lvlLightOk);
        if (smkOn) { char s[224]; const CarParams *P = lvlCarOk == 1 ? &lvlCar : 0; CarParams d; if (!P) { carDefaults(&d); P = &d; }
          snprintf(s, sizeof s, "CAR level=%d car=%s src=%s mass=%.0f top=%.1f grip=%.2f body=%.3fx%.3f ofs=%.3f lo=%.2f hi=%.2f door=%.3f,%.3f seat=%.3f,%.3f cam=%.2f/%.2f\n",
                   gameLevel, P->name, carParamsError(lvlCarOk), P->mass, P->topSpeed * 3.6f, P->tireGrip, 2.f * P->halfW, 2.f * P->halfL, P->bodyOfs,
                   P->bodyLo, P->bodyHi, P->doorX, P->doorZ, P->seatAx, P->seatAz, P->camElev, P->camMag); smkWrite(s); }
      } }
    } break;
    } } }
  phMark("sections");
  if (ldOn) loadFrame(2 * LOAD_RES_SHARE / 6, fbp);
  if (haveCarVis) for (int i = 0; i < cv.nhinge; i++)            // шарнир двери водителя (DoorDRot)
    if (cv.hinge[i].joint < cv.njoint && !strncmp(cv.joint[cv.hinge[i].joint].name, "DoorDRot", 8)) doorHinge = i;
  sceIoClose(fd);
  ldStep("pre-tone");
  toneBlkAdd(data, residentSize, TF_LEVEL, 0);   // правка 144: резидентная часть = level.shr с нуля, палитры — заново из файла
  size_t p = 104;
  hm = (short *)(data + p); p += (size_t)hw * hh * 4 * 2; p = (p + 15) & ~15;
  wl = (short *)(data + p); p += (size_t)hw * hh * 2 * 2; p = (p + 15) & ~15;
  tex = calloc(ntex ? ntex : 1, sizeof(Tex)); groundTexN = 0; fdReset();
  // №786 (Роб 18.10 после TEST34: «дым — облачка, тучки — стал контрастнее и не такой красивый; вернуть прежний, по умолчанию для обоих режимов он красивый»): текстуры
  // кусков неба (облака, даль; владелец 2 в таблице кусков) — без шагов мультика (стягивание, пресет 90-х, контраст): тон как в обычном режиме. Купол неба (цвета
  // вершин) мультик красит по-прежнему (№785, main_camera.h skyDomeTone). SKY_CLOUD_TOON_OLD=1 — облака тоже через мультик (как TEST34)
#ifndef SKY_CLOUD_TOON_OLD
#define SKY_CLOUD_TOON_OLD 0
#endif
  if (!SKY_CLOUD_TOON_OLD && tex) for (unsigned i = 0; i < npc; i++) { Piece q_; memcpy(&q_.r, tab + 80 * i, 80);
    if (q_.r.owner == 2 && q_.r.tex >= 0 && (unsigned)q_.r.tex < ntex) tex[q_.r.tex].tstat[4] = 9; }   // №1026: бит 8 — небо и купол (без шагов мультика и после правки), бит 1 без 8 — вывеска
  // №788 (Роб 18.10 после TEST34: «в мультике не накладывать цветокоррекцию на билборды (Красти), разбиваемые стёкла, стоп-знаки — пусть остаются светлыми, звонкими»):
  // список текстур уровня без шагов мультика — notoon.txt (номера записей level.shr через пробел; tools/notoon_list.py по именам текстур диска: вывески, щиты, знаки,
  // плакаты, логотипы, дирижабль Duff, витринное стекло). Первая строка «ntex N sig X»: число текстур и подпись (FNV-1a по номерам текстур всех кусков, Rec.tex @24) —
  // список от другого экспорта level.shr не применяется (номера записей сдвинулись бы). Нет файла или не та подпись — как было. NOTOON_OLD=1 — не читать
#ifndef NOTOON_OLD
#define NOTOON_OLD 0
#endif
  if (!NOTOON_OLD && tex) { char fn_[256]; levelFile(fn_, "notoon.txt"); SceUID nf_ = sceIoOpen(fn_, PSP_O_RDONLY, 0);
    if (nf_ >= 0) { static char nb_[4096]; int n_ = sceIoRead(nf_, nb_, sizeof nb_ - 1); sceIoClose(nf_); int marked_ = -1; unsigned tn_ = 0, sg_ = 0, h_ = 2166136261u;
      for (unsigned i = 0; i < npc; i++) { unsigned v_; memcpy(&v_, tab + 80u * i + 24u, 4); h_ = (h_ ^ v_) * 16777619u; }
      if (n_ > 0) { nb_[n_] = 0; char *q_ = strchr(nb_, '\n');
        if (q_ && sscanf(nb_, "ntex %u sig %x", &tn_, &sg_) == 2 && tn_ == ntex && sg_ == h_) { marked_ = 0;
          unsigned char bit_ = 1; int fol_ = 0;   // №825: после слова «foliage» — листва (бит 2), до него — без мультика (бит 1)
          while (*q_) { while (*q_ && (*q_ < '0' || *q_ > '9')) { if (*q_ == 'f') bit_ = 2; q_++; } if (!*q_) break; unsigned long v_ = strtoul(q_, &q_, 10); if (v_ < ntex) { tex[v_].tstat[4] |= bit_; if (bit_ == 1) marked_++; else fol_++; } }
          if (smkOn) { char f_[64]; snprintf(f_, sizeof f_, "FOLIAGE level=%d marked=%d\n", gameLevel, fol_); smkWrite(f_); } } }
      if (smkOn) { char s_[120]; snprintf(s_, sizeof s_, "NOTOON level=%d marked=%d of %u sig=%08x file=%u/%08x\n", gameLevel, marked_, ntex, h_, tn_, sg_); smkWrite(s_); } } }
  flatFromFile = 0; flatListLoad(tab, npc);
  sky954Free(); unsigned char *sky954Need = 0;   // №954: текстуры кусков неба (владелец 2) — строки градиента снимаются здесь, пока палитра исходная
  if (!sky954Old && tex && ntex && (sky954G = calloc(ntex, sizeof *sky954G))) { sky954N = ntex; sky954Need = calloc(ntex, 1);
    for (unsigned i = 0; sky954Need && i < npc; i++) { Piece q_; memcpy(&q_.r, tab + 80 * i, 80); if (q_.r.owner == 2 && q_.r.tex >= 0 && (unsigned)q_.r.tex < ntex) sky954Need[q_.r.tex] = 1; } }
  for (unsigned i = 0; i < ntex; i++) {
    unsigned short *t2 = (unsigned short *)(data + p);
    tex[i].w = t2[0]; tex[i].h = t2[1]; tex[i].levels = t2[2];
    tex[i].pre = ((unsigned char *)t2)[7] == 1;   // байт 7 = 1: уровни уже в swizzle, высота добита до кратной 8
    tex[i].t4 = ((unsigned char *)t2)[6] == 4;    // байт 6 = 4: 4 бита на тексел (правка 129, tools/texq.py CLUT4; всегда в swizzle)
    if (tex[i].t4 && !tex[i].pre) fail("level.shr: 4-bit texture without swizzle. Run the export again.");
    memcpy(&tex[i].slope, data + p + 12, 4); if (!(tex[i].slope > 0.f && tex[i].slope < 10.f)) tex[i].slope = 0.f;   // правка 77/88
    tex[i].ground = ((unsigned char *)t2)[9] == 1; if (tex[i].ground) tex[i].tstat[4] |= 4;   // №865: земля — в мультике с рисунком (tone.c)
    groundTexN += tex[i].ground && tex[i].slope > 0.f;   // метка земли (tools/psp_export.py, байт 9 заголовка): множитель groundbias действует только на неё
    tex[i].pal = data + p + 16; p += 16 + 1024; if (!(i & 15)) loadPump();
    int lw = tex[i].w, lh = tex[i].h;
    for (int l = 0; l < tex[i].levels; l++) {
      tex[i].lv[l] = data + p; if (tex[i].pre) tex[i].slv[l] = data + p;
      p += texRowBytes(tex[i].t4, tex[i].pre, lw) * (tex[i].pre ? (lh + 7) & ~7 : lh); p = (p + 15) & ~15; lw >>= 1; lh = lh > 1 ? lh >> 1 : 1;   // Ф2: мип < 16 px — строка 16 Б (байт 8 = 1)
    }
    if (toneToon() || TOON_STAT_LAZY_OLD) texToonStat(&tex[i], (const unsigned char *)tex[i].pal);   // №754: средний цвет — по исходной палитре, до кривой; в обычном режиме — лениво (задание тона при включении Cartoon)
    if (sky954Need && sky954Need[i]) sky954Build(i, &tex[i]);
    toneSetStat(tex[i].tstatOk ? tex[i].tstat : 0); palTone(tex[i].pal); toneSetStat(0);
  }
  free(sky954Need);
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "GROUNDBIAS level=%d tex=%u ground=%u mul=%.2f old=%d\n", gameLevel, ntex, groundTexN, groundMul, GROUNDBIAS_RT_OLD); smkWrite(s_); }   // ground=0 при данных без метки земли: множитель не действует
  treeShScan(); latticeScan();   ldStep("tex-table"); swizzleAll(); ldStep("swizzle");
  if (ldOn) loadFrame(3 * LOAD_RES_SHARE / 6, fbp);
  pc = calloc(npc ? npc : 1, sizeof(Piece));
  carP = malloc(2 * (npc ? npc : 1)); skyP = malloc(2 * (npc ? npc : 1));
  for (unsigned i = 0; i < npc; i++) {
    memcpy(&pc[i].r, tab + 80 * i, 80); flatMarkPiece(pc[i].r.tex, pc[i].r.flags, pc[i].r.owner);
    float *b = pc[i].r.bb;
    pc[i].cx = (b[0] + b[3]) * .5f; pc[i].cy = (b[1] + b[4]) * .5f; pc[i].cz = (b[2] + b[5]) * .5f;
    float dx = b[3] - b[0], dy = b[4] - b[1], dz = b[5] - b[2]; pc[i].rad = .5f * sqrtf(dx * dx + dy * dy + dz * dz);
    if (pc[i].r.owner == 1) carP[ncarP++] = i;
    if (pc[i].r.owner == 2) skyP[nskyP++] = i;
  }
  nzLoad(tab, npc); opq863Apply(tab); dalLoad(tab, npc); dripLoad();   // №877, №770: таблица дали бота-репортёра (viewer/dal877.h); №863: море L3/L6 и прочее «полупрозрачное без прозрачности» — непрозрачным (список с Мака, tools/opq863.py)
  if (!CULL_HOT_OLD && npc) { cr = (CullRec *)memalign(32, sizeof(CullRec) * npc);   // отсечение: горячие записи (main_state.h CullRec)
    for (unsigned i = 0; cr && i < npc; i++) { const Piece *d = &pc[i]; CullRec *h = &cr[i]; memcpy(h->bb, d->r.bb, sizeof h->bb);
      int t = d->r.tex < 0 || d->r.tex > 510 ? 511 : d->r.tex;
      h->tk = (unsigned short)(((d->r.flags & 3) << 9) | t);
      h->tcl = ((d->r.flags & 2) && !(d->r.flags & 4)) || d->r.cell >= 0xFFFFu ? 0xFFFFu : (unsigned short)d->r.cell;
      h->cell = d->r.cell >= 0xFFFFu ? 0xFFFFu : (unsigned short)d->r.cell; h->rg = (unsigned char)(d->r.pad[0] <= 255 ? d->r.pad[0] : 0); h->pad = d->r.pad[0] > 255; } }   // pad=1: регион не влез — прежний путь
  ldStep("pieces");
  cl = calloc(ncell ? ncell : 1, sizeof(Cell)); free(cellBigF); cellBigF = calloc(ncell ? ncell : 1, 1);   // №840
  cdl = calloc(ncell ? ncell : 1, sizeof(CellDL)); cdlMark = calloc(ncell ? ncell : 1, sizeof(unsigned)); dlBytesNow = 0;   // правка 275
  for (unsigned i = 0; i < ncell; i++) {
    memcpy(&cl[i], cellTab + 48 * i, 48);
    float *b = cl[i].bb;
    cl[i].cx = (b[0] + b[3]) * .5f; cl[i].cy = (b[1] + b[4]) * .5f; cl[i].cz = (b[2] + b[5]) * .5f;
    float dx = b[3] - b[0], dy = b[4] - b[1], dz = b[5] - b[2]; cl[i].rad = .5f * sqrtf(dx * dx + dy * dy + dz * dz);
    cl[i].state = 0; cl[i].mem = 0;
  }
  lvaFree(tabBuf); tab = cellTab = 0;   // таблицы скопированы в pc[] и cl[]
  ldStep("cells"); marksLoad();   // №912
  if (!FARROAD934_OLD && road934On) road934Load(gameLevel);   // №934: дальние дороги и мосты (road934.shr, ~0,2–0,4 МБ) читаются здесь, под экраном загрузки, а не рывком в кадре
  { char rq_[256]; levelFile(rq_, "roadq864.bin"); roadqLoad(rq_, ncell); }   // №864 пропечка 2: клетки впереди по дороге
  if (tailBg) {   // п.6: дождаться COL1 и PRP2 (загрузчик читал их, пока считались текстуры), разобрать в прежнем порядке: COL1, потом PRP2
    unsigned tw = sceKernelGetSystemTimeLow();
    { LdWait lw_ = {0}; while (tailState == 1) { if (loaderWaitStep(&lw_, "tail", 0, 1)) { tailReadSync(); break; } loadPump(); sceKernelDelayThread(1000); } }   // №LOADWAIT: загрузчик молчит — читаем хвост сами
    tailState = 0;
    if (tailCol) levelColAdopt(tailCol, tailColSz);
    if (tailProp) levelPropAdopt(tailProp, tailPropSz);
    tailCol = tailProp = 0;
    if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "TAILBG wait_ms=%u col=%d props=%d\n", (sceKernelGetSystemTimeLow() - tw) / 1000u, haveCol, haveProps); smkWrite(s_); }
    ldStep("tail");
  }
  // №RESUME-круг (аудит времени загрузки 16.10): при RESUME / LOAD игрок стартует у своей миссии (L1: в 562 м от spawnP), а фоновый круг вокруг spawnP — 240 клеток, 7 МБ — читался зря
  // (238 клеток потом выбрасывались, 171 дочитывались с ожиданием 606 мс, 7 МБ мешали чтению level.shr). На RESUME круг теперь читает cellsRingAt вокруг настоящей точки старта (loadOneAt).
  // RESUME_RING_OLD=1 или файл resumeringold рядом с EBOOT — прежнее. NEW GAME как раньше
  if (!svResumeOn || resumeRingOld) cellRingStart(spawnP);   // загрузка 2 п.5: круг клеток у старта читает загрузчик, пока главный грузит остальное
  // этап 5: трафик, стоящие машины, пешеходы (1.35 МБ, резидентно). Текстуры машин TRF1 — в конец tex[] (trTexBase)
  if (haveCol) { char fn[256]; levelFile(fn, "traffic.shr");
    trAllocHook = trAllocSoft962; trFreeHook = trFreeSoft962;   // №962: traffic.shr (1,3 МБ) — в запас участка lva, если он есть
    if (trafficLoad(&traffic, fn) == 0) {
      if (smkOn) { char z_[64]; snprintf(z_, sizeof z_, "STOP875 level=%d lanes=%d\n", gameLevel, trafficStopCount()); smkWrite(z_); }   // №875
      toneBlkAdd(traffic.base, traffic.size, TF_TRAFFIC, 0);   // правка 144: traffic.shr читается целиком с начала
      int n = trafficTexCount(&traffic); Tex *nt = realloc(tex, (ntex + n + 1) * sizeof(Tex));
      if (nt) { tex = nt; trTexBase = ntex; haveTraffic = 1;
        for (int i = 0; i < n; i++) { Tex *x = &tex[ntex + i]; memset(x, 0, sizeof *x);
          trafficTexInfo(&traffic, i, &x->w, &x->h, &x->levels, &x->pre, &x->pal, x->lv); palTone(x->pal); if (x->pre) memcpy(x->slv, x->lv, sizeof x->lv); }
        ntex += n; { unsigned cf0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; carFillLoad(&traffic); if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "CARFILLT level=%d fills=%d ms=%.2f t=%.1f\n", gameLevel, cfSet[0].nbits, (sceKernelGetSystemTimeLow() - cf0_) / 1000.f, smkT); smkWrite(l_); } } }
      else { toneBlkDel(traffic.base); trafficFree(&traffic); }
    } }
  ldStep("traffic");
  gameMsnSetRoads(haveTraffic ? &traffic : 0);   // машины миссий: путь по полосам трафика, удары о трафик
  if (haveTraffic && haveCol) { unsigned tp = sceKernelGetSystemTimeLow(); int nb = -1, baked = 0; unsigned char *mcb = 0;
    if (!MCPROBE_BAKE_OLD) {   // загрузка 2 п.1: запечённая проверка дорог (mcpr.shr, tools/mcp_bake.c); хеш не сошёлся / файла нет — считаем, как раньше
      char fn[256]; levelFile(fn, "mcpr.shr"); SceUID mf = sceIoOpen(fn, PSP_O_RDONLY, 0);
      if (mf >= 0) { mcb = memalign(16, MCP_FILE_BYTES); if (mcb && sceIoRead(mf, mcb, MCP_FILE_BYTES) == MCP_FILE_BYTES) { nb = mcProbeBakedLoad(&traffic, mcColHash, mcb, MCP_FILE_BYTES); baked = nb >= 0; } sceIoClose(mf); }
    }
    if (nb < 0 || MCPROBE_VERIFY) { int nb2; mcProbePump = loadPump; nb2 = mcProbeRoads(&traffic, &world); mcProbePump = 0;   // перекрытые дороги — на экране загрузки
      if (smkOn && baked && MCPROBE_VERIFY) { char s[96]; snprintf(s, sizeof s, "MCPVERIFY level=%d same=%d blocked_baked=%d blocked_calc=%d\n", gameLevel, mcProbeStateSame(mcb) && nb == nb2, nb, nb2); smkWrite(s); }
      nb = nb2; baked = 0; }
    free(mcb);
    if (smkOn) { char s[112]; snprintf(s, sizeof s, "MCPROBE roads=%u blocked=%d ms=%.0f baked=%d\n", traffic.hd->nRoad, nb, (sceKernelGetSystemTimeLow() - tp) / 1000.f, baked); smkWrite(s); } }
  { Tex *nt = realloc(tex, (ntex + MCR_MAX * MCR_TEXPER + 1) * sizeof(Tex));   // машины миссий: слоты текстур моделей по требованию
    mcrTexOk = 0; if (nt) { tex = nt; mcrTexBase = ntex; memset(&tex[ntex], 0, MCR_MAX * MCR_TEXPER * sizeof(Tex)); ntex += MCR_MAX * MCR_TEXPER; mcrTexOk = 1; } }
  ldStep("probe"); mcrDirLoad(); ldStep("mcrdir");   // №930: куски машин миссий заливкой — боковой файл пакета
  sceKernelDcacheWritebackAll(); ldStep("dcache");
  if (ldOn) loadFrame(4 * LOAD_RES_SHARE / 6, fbp);
  // машина и небо — сразу; город вокруг старта — cellsRing перед игрой (12а: после главного меню), остальное фоном
  SceUID lfd = sceIoOpen(levelShr, PSP_O_RDONLY, 0);
  for (unsigned i = 0; i < ncell; i++) if (cl[i].owner) { loadCell(lfd, i); ldStep("owner-cell"); }
  sceIoClose(lfd); ldStep("cells-done");
  skyPump = LOAD_SMOOTH_OLD ? NULL : loadPump; skyFogColor(); skyPump = NULL; ldStep("sky");
  groundCoverPump = LOAD_SMOOTH_OLD ? NULL : loadPump; groundCoverReset(&groundCover,haveCol?&world:NULL); groundCoverPump = NULL; ldStep("cover");
  fxFree(); fxInit(gameLevel); ldStep("fx");
  skidBeginLevel(gameLevel);   // №443: следы колёсного флота на всех уровнях, прежнее ограничение — SKID_MARKS_ALL_OLD
  mvSetRoomAt(npcRoomAt);
  ldStep("skid");
  if (!bonusTrack) mvInit(path, gameLevel);   // пункт 8: пакет миссий level0<N>.msn рядом с EBOOT (нет — игра без миссий); у трассы мини-игры миссий нет
  if (haveSP) {   // после fxInit: окна EFX стейтпропов -> родные системы fx.shr по имени; частицы обломков играет onPropKickFx
    for (unsigned k = 0; k < spFxNameCount(&sprops); k++) spFxBind(&sprops, k, fxFind(spFxName(&sprops, k)));
    sprops.nativeBreakFx = props.typeFx && fxReady();
  }
  ldStep("mvinit");
  sndCarSetup(lvlCarOk == 1 ? lvlCar.topSpeed : 0.f);   // шкала передач мотора — TopSpeed машины уровня (нет CAR1 v2 — 120 км/ч, как было)
  ldStep("snd-car");   // №517: шаги разнесены (sndCarSetup / привязка звуков к предметам)
  sndBindPump = ldOn ? loadPump : 0;   // кадр газеты изнутри привязки (sound.c sndBindObjects)
  if (haveProps) sndBindObjects(SND_SET_PROPS, props.ninst, props.def, sizeof(PropDef));
  ldStep("snd-bind-props");
  if (haveSP) sndBindObjects(SND_SET_SPROPS, sprops.ninst, sprops.inst, sizeof(SpInst));
  sndBindPump = 0; ldStep("snd-bind"); sndLevelStart(spx, spy, spz); ldStep("snd-start");
  propBatchReset();   // №913: группы среза кусков пропов привязаны к адресам вершин уровня
  blendOpqScan(); ldStep("blend-opq");   // №907: куски пропов и стейтпропов с пустым смешиванием — непрозрачными (BLENDOPQ_OLD)
  collLoad(); ldStep("coll");
  pfClear();   // №PFETCH: невостребованное (нет COL1 / fx.shr) — в кучу, в игре буферов не держим
#if !TICKET_CONTENT_C3_OLD
  { char tf[256]; levelFile(tf,"ticket.shr"); ticketViewLoad(&ticketView,tf,smkWrite); }
#endif
  if (ldOn) loadFrame(5 * LOAD_RES_SHARE / 6, fbp);   // В5: карточки, осы, гэги (collect-l1.shr рядом с EBOOT; нет — уровень без них)
  ldStep("ticket");
  if (haveCol) carInit(&car, lvlCarOk == 1 ? &lvlCar : NULL, &world, spawnP[0], spawnP[1], spawnP[2], spawnP[3]);   // машина уровня (CAR1 v2)
  ldStep("carinit"); if (haveTraffic) trafficReset(&traffic, &world); ldStep("trafficreset");   // этап 5: стоящие машины на места
  bound = -2; loadedLevel = gameLevel; lvGen++; __asm__ volatile("" ::: "memory");
  levelLog("load", lt0, lh0);
  if (smkOn) { char s_[300]; snprintf(s_, sizeof s_, "LVA level=%d on=%d used=%u size=%u miss=%u col=%d props=%d soft=%u softmiss=%u old=%d tail962b=%d\n", gameLevel, lvaBase != 0, lvaUsed, LVA_BYTES, lvaMiss, haveCol, haveProps, lvaSoftBytes, lvaSoftMiss, lvaMem962Old, !lvaTail962bOld); smkWrite(s_);
    for (int i_ = 0; i_ < lvaSoftRecN; i_++) { char r_[160]; snprintf(r_, sizeof r_, "LVASOFT level=%d n=%u reserve=%u used=%u tail=%d alloc=%d hit=%d\n", gameLevel, lvaSoftRec[i_].n, lvaSoftRec[i_].res, lvaSoftRec[i_].used, lvaSoftRec[i_].ts, lvaSoftRec[i_].al, lvaSoftRec[i_].hit); smkWrite(r_); } lvaSoftRecN = 0; }   // №775
}
// Главное меню до выбора NEW GAME (остальные пункты затемнены и пропускаются курсором), потом загрузка и новая игра.
// Сценарий смоука идёт и здесь (press/wait/shot/end); fbp — буфер рисования main (sceGuSwapBuffers)
static unsigned menuT0;  // время нажатия NEW GAME (строка GAME смоука)
#ifndef QUIT_NOGAME_OLD
#define QUIT_NOGAME_OLD 0   // 1 — прежнее: после QUIT в главное меню LOAD GAME предупреждает «progress to be lost» (флаг levelReady не сбрасывался)
#endif
static int menuNoGame;   // 1 — игра закрыта (QUIT/конец игры), главное меню без текущей игры: предупреждения о потере прогресса нет, пока не начата новая или загружена
static int levelReady;   // правка Роба 3 (28.09): уровень читается после NEW GAME первого меню, как в оригинале
// Ролик новой игры fmv1a (29.09): в записи PS2 (tools/ps2ref/CATALOG_L1.md, 0:20–0:55) после старта игры идёт ролик — эмблема,
// Красти с Buzz Cola по ТВ, Гомер на диване (кадры и длина 38,6 с совпадают с movies\fmv1a.rmv, таблица роликов SLES_518.97:
// demo, radlogo, foxlogo, gracie, vuglogo, intro, fmv1a..fmv8), сразу за ним — облёт улицы (0:58); газеты загрузки между ними
// нет — порядок порта: NEW GAME -> газета (уровень читается) -> ролик -> облёт. Только новая игра уровня 1 (не LOAD/RESUME и не
// переход между уровнями); START — пропустить. Служебный запуск — только с «flag newgamefmv». В поставке: "movies/fmv1a.smv"
static int newGameFmv, pauseOld;
// правка Роба 91 (30.09): «музыка уровня начинает играть раньше, чем видео» — NEW GAME: звук уровня не стартует внутри newGame(),
// только после газеты и ролика fmv1a (audioDefer держит его до конца menuStartGame)
// Правка Роба 356 (05.10, голосом): «загрузка между уровнями идёт дважды: газета, потом ирис, потом опять газета — нужна одна длинная
// загрузка с газетой на всё». Причина (журнал эмулятора, DEV SAVE -> пауза -> MISSION SELECT -> ► уровень 2 -> ✕): газета уровня
// (levelAdvance) закрывалась, и только ПОСЛЕ неё стартовала миссия выбранной строки (mvMissionSelectStartRow в главном цикле);
// сценарий переставлял игрока за 270 м от спавна -> ирис «msn» -> msnTeleport -> вторая газета (LOADSPIN/LOADSHOW дважды, MSNLOAD).
// Вторая беда того же рода: круг клеток под газетой читался вокруг спавна уровня (cellsRing), хотя после записи (LOAD GAME, DEV
// SAVE) игрок стоит у своей миссии (DEV L1: машина 47,-281 при спавне 220,172) — город вокруг игрока догружался уже в игре.
// Правило порта (у оригинала смена уровня — один экран загрузки, игра начинается готовой; запись PS2 перехода L1 -> L2 не
// содержит — она кончается логотипами Fox/Gracie): всё, что ставит игрока, — под той же газетой, до круга клеток: старт миссии
// строки MISSION SELECT, перенос сценария поглощается (ириса и второй газеты нет), круг клеток — вокруг того места, где игрок
// начнёт, комнаты и камеры уровня (levelLoadAfter) — тоже под газетой. Прежнее — «flag lvtwice» / -DLOAD_ONE_OLD=1 (красный гейта)
static int loadOneRow, loadOneTele;
// №943 (Роб 24.10: «остаётся миссия "приехать домой"»): при выбранной строке стартовая цель уровня (первая непройденная сюжетная — у L1 «домой» к дому Симпсонов) не ставится:
// msn_level_start откладывает её (rt->defer_first), строка стартует сама; не вышло — стартует отложенная (mvStartFirstIfIdle). LVDEFER_OLD=1 / «flag lvdeferold 1» — прежнее: два старта подряд
#ifndef LVDEFER_OLD
#define LVDEFER_OLD 0
#endif
static int lvDeferOld = LVDEFER_OLD;
static void loadOneStart(void) {   // под газетой: миссия выбранной строки MISSION SELECT и перенос сценария — сразу
  loadOneRow = msPendRow;
  if (msPendRow >= 0) { int ok_ = mvMissionSelectStartRow(msPendRow); if (!ok_) mvStartFirstIfIdle(); msPendRow = -1; chase.init = 0; loadPump(); }   // правка 348: миссия строки — под газетой
  float tp[3]; loadOneTele = mvTeleported(tp);   // перенос поглощён: главный цикл не закроет ирис и не откроет вторую газету
}
static void loadOneAt(float c[3]) {   // где игрок начнёт: в комнате (L4, L7 — Гомер дома) — у её двери в городе, иначе — где стоит
  int ft = onFoot(), room = inRoom() && rs.lastDoor >= 0 && rs.lastDoor < rooms.ndoor;
  if (room) { const float *d = rooms.door[rs.lastDoor].p; c[0] = d[0]; c[1] = d[1]; c[2] = d[2]; }
  else if (ft) { c[0] = homer.x; c[1] = homer.y; c[2] = homer.z; }
  else if (haveCol) { c[0] = car.x; c[1] = car.y; c[2] = car.z; }
  else { c[0] = spawnP[0]; c[1] = spawnP[1]; c[2] = spawnP[2]; }
  if (smkOn) { char s[176]; float dx = c[0] - spawnP[0], dz = c[2] - spawnP[2];
    snprintf(s, sizeof s, "LOADONE level=%d row=%d tele=%d foot=%d room=%s at=%.1f,%.1f spawn_m=%.0f t=%.1f\n", gameLevel, loadOneRow, loadOneTele, ft,
             room ? roomName() : "-", c[0], c[2], sqrtf(dx * dx + dz * dz), smkT); smkWrite(s); }
}
static void menuStartGame(void **fbp, unsigned t0, int after) {   // NEW GAME: экран загрузки Loading.scr уже открыт; новая игра и город вокруг игрока
  char s[128]; int fresh = !svResumeOn; gameMsnResumeStart(svResumeOn);   // after: набор уровня только что прочитан — комнаты и камеры уровня (levelLoadAfter) читаются здесь
  gameMsnDeferStart = !lvDeferOld && !loadOneOld && svResumeOn && msPendRow >= 0;   // №943: выбранная строка стартует сама, стартовая цель уровня не ставится
  audioDefer = 1; newGame(); audioDefer = 0; gameMsnDeferStart = 0; if (svResumeOn) { saveApply(&svPending); svResumeOn = 0; saveLog("load"); } else { memset(&lvKeep, 0, sizeof lvKeep); if (svmOn()) saveLog("new"); }   // облако 05.10: новая игра — прогресс уровней пуст   // LOAD / RESUME GAME
  bonusSession = 1; if (haveColl) bonusOpenFromColl(fresh ? "new" : "load"); else bonusOpenFromSave(fresh ? NULL : &svPending, "load");   // правка 421
  float c[3]; int one = !loadOneOld;
  if (one) { loadOneStart();                                      // правка 356: всё, что ставит игрока, — под этой газетой
    if (after) { levelLoadAfter(); after = 0; } else msnRoomSync();   // комнаты, камеры старта миссий и зон уровня — тоже
    loadOneAt(c); }
  ldWarmC[0] = one ? c[0] : spawnP[0]; ldWarmC[1] = one ? c[1] : spawnP[1]; ldWarmC[2] = one ? c[2] : spawnP[2]; ldWarmOn = 1;   // №538: точка старта для прогрева на хвосте экрана загрузки
  int n = one ? cellsRingAt(c, ldOn ? 1 : 0, fbp) : cellsRing(ldOn ? 1 : 0, fbp), audio = ldAudio, fmv = fresh && gameLevel == 1 && (!svcMode || newGameFmv);
  if (fmv) ldAudio = 0;   // звук уровня — после ролика
  // №663 (порядок PS2, ps2-newgame-newspaper.yaml): газета крутится -> кружок сжимается -> короткий чёрный -> игра (NEW GAME: ролик). Ирис №576 теперь ПОСЛЕ газеты;
  // над роликом его не раскрываем. START_NEWS_OLD=1 — без ириса после газеты, как в cce625d4 было без газеты
  if (ldOn) { loadEnd(fbp); if (newsIris && transitReady()) { transitClose(fbp, "news_out"); if (fmv) transitDrop(); } SCR_CLOSE(); }
#ifndef LEVEL_MUSIC_NOLOAD_OLD   // №664: старт без экрана загрузки (№576, RESUME / LOAD / New Game не L1) — loadEnd не вызван, звук уровня оставался в ldAudio и тема меню играла весь тур; LEVEL_MUSIC_NOLOAD_OLD=1 прежнее
  else if (ldAudio) { ldAudio = 0; levelAudioPending = 1; if (smkOn) { snprintf(s, sizeof s, "LEVELAUDIO pend noload t=%.1f\n", smkT); smkWrite(s); } }
#endif
  sndMaster(1.f);
  if (fmv) { msnMovie("fmv1a.rmv", fbp); if (audio) levelAudioPending = 1; }   // правка 105: звук — с первым кадром облёта
  if (smkOn) { snprintf(s, sizeof s, "GAME n=%d t=%.1f cells=%d ms=%.0f car=%.2f,%.2f\n", nMenu, smkT, n,
                        (sceKernelGetSystemTimeLow() - t0) / 1000.f, car.x, car.z); smkWrite(s); }
  msnRoomSync();   // старт уровня / загрузка в комнате (комнаты этого уровня уже в памяти; иначе — levelLoadAfter)
  if (after) levelLoadAfter();   // прежнее (flag lvtwice): комнаты и камеры уровня — уже после газеты
}
// Уровень пройден (MSN_EV_LEVEL_COMPLETE после вспышки LEVEL COMPLETE): следующий уровень с его газетой loading{N+1}, с начала
// первой сюжетной миссии (level.mfk AddMission — msn_level_start); с собой — прогресс всех уровней (карточки, осы, гэги) и кошелёк
// монет тем же путём, что запись и загрузка сохранения (savegame.c), монеты мира — нового уровня. В оригинале так же: уровень N+1
// открывается прохождением уровня N, сохранение пишет «LEVEL N+1». Длительность вспышки — правило порта (msn_view.c MV_LEVEL_DONE_S)
// №980 (Роб 25.10, тур TEST62 r2: будка, открытая на L7, осталась фоном на L2 после golevel — экран будки, лавки и гардероба levelAdvance не закрывал, их кадры и фон жили дальше): общий сброс экранов при
// смене уровня, любой путь (MISSION SELECT, уровень пройден, golevel; из меню — menuRun): закрыть будку, лавку, гардероб (пакеты освобождены, SCR снят), сообщение и значок цели прошлого уровня,
// меню паузы. Брифинг, разговор и маркер цели снимает mvShutdown (вызывается ниже). LVRESET980_OLD=1 / flag lvreset980old 1 — как было
#ifndef LVRESET980_OLD
#define LVRESET980_OLD 0
#endif
static int lvReset980 = !LVRESET980_OLD;
static void levelResetUi980(void) {
  if (!lvReset980) return;
  if (phoneViewOn()) phoneViewEnd(mvShop());
  if (shopViewOn()) shopViewEnd(mvShop());
  if (wardrobeOn()) wardrobeEnd();
  SCR_CLOSE(); uiMessage(NULL, -1, 0.f); uiPauseMenuHide(0);
}
// №980 служебно (smoke): что держит экран и цель — строка LVSTATE до и после смены уровня: будка, лавка, гардероб, пауза, брифинг, разговор, значок цели (mvIconState)
static void lvStateLine(const char *when) {
  if (!smkOn) return;
  char ic[160]; mvIconState(ic, sizeof ic); char s[320];
  snprintf(s, sizeof s, "LVSTATE when=%s level=%d phone=%d shop=%d ward=%d pause=%d brief=%d talk=%d icon=%s t=%.1f\n", when, gameLevel, phoneViewOn(), shopViewOn(), wardrobeOn(), uiPaused(), mvBriefing(), mvTalkActive(), ic, smkT); smkWrite(s);
}
static void levelAdvance(int n, void **fbp) {
  unsigned t0 = sceKernelGetSystemTimeLow(); lvStateLine("before");
  SaveGame carry; saveCapture(&carry);
  uint32_t lvM_ = 0; int lvG_ = !gameLvProgOld && n != gameLevel ? svLvGet(&carry, n, &lvM_, NULL) : 0;   // облако 05.10: пройденное уровня n
  carry.level = carry.msnLevel = (uint8_t)n; carry.msnStory = 0xff; carry.msnPhase = carry.msnStage = 0; carry.msnCompleted = 0;
  carry.nworld = 0xffff; memset(carry.worldCoins, 0, sizeof carry.worldCoins);   // монеты мира прошлого уровня не переносятся
  if (lvG_) carry.msnCompleted = lvM_;   // облако 05.10 (карьера): возврат на пройденный уровень — его миссии из записи (game_msn.c resumeAt)
  if (msPendDev) {   // правка 348: MISSION SELECT в DEV SAVE — перенос с меткой DEV (savemenu.h svmIsDev: seq 0 при дате), миссии — msnStart
    carry.seq = 0; carry.year = svPending.year ? svPending.year : 2026; carry.month = svPending.month ? svPending.month : 1;
    carry.day = svPending.day ? svPending.day : 1; msPendDev = 0; }
  if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  levelResetUi980(); uiPause(0); cellsDropAll(); mvShutdown(); mcrFreeAll(); msnartRoom = 0;
  sndEngine(0.f, 0.f, 0); sndSkid(0.f, 0); sndCarAir(0); sndMusic(SND_MUS_OFF); sndMaster(0.f);
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "LEVELUP from=%d to=%d t=%.1f\n", gameLevel, n, smkT); smkWrite(s); }
  gameLevel = n;
  int unl_ = 0;   // №1065: газета не открылась — предыдущий уровень выгружается ДО неё (куча пуста), открытие повторяется
  { int rc_ = scrOpen1065("levelup"); if (memLoad1065 && rc_ == -3) { levelUnload(); unl_ = 1; scrOpen1065("levelup-unloaded"); } scrTvOver = 0; }   // экран загрузки: газета уровня n
  sndMaster(1.f); feStinger("newspaper_spin");   // правка 20: газета следующего уровня влетает — стингер Newspaper_Spin
  ldBase = LOAD_RES_SHARE; loadBegin(fbp);
  if (!unl_) levelUnload();
  levelLoad(fbp);
  svPending = carry; svResumeOn = 1;
  menuStartGame(fbp, t0, 1);   // + комнаты и камеры уровня (levelLoadAfter): правка 356 — под газетой
  lvStateLine("after");
}
// правка Роба 28.09: × по пункту — перелёт камеры оригинала к его предмету (viewer/frontend3d.h feFly; кадры таймлайна
// FE_CameraController, пункт -> отрезок из данных). Пока камера летит, ввод не принимается, сценарий смоука идёт
static void menuFly(int item, int mode, int sel, void **fbp) {
  if (!feFly(item, mode)) return;
  char s[160]; unsigned last = sceKernelGetSystemTimeLow(), t0 = last, sw0 = swapN, late0 = swapLateN, wait0 = swapWaitN; float f0 = feFrame();
  while (feFlying()) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    SmokeCmd sc; while (smokeNext(dt, &sc)) if (sc.op == 2 && !strcmp(sc.name, "feglow")) feGlowTest = (int)sc.a;
                                             else if (sc.op == 2 && !strcmp(sc.name, "flynoframe")) flyNoFrame = 1;
                                             else swapFlag(&sc);
    menuFrame(sel, fbp);
  }
  if (smkOn) { snprintf(s, sizeof s, "FEFLY item=%d mode=%d from=%.0f to=%.0f s=%.2f t=%.1f swaps=%u late=%u waited=%u old=%d\n", item, mode, f0,
                        feFrame(), (sceKernelGetSystemTimeLow() - t0) * 1e-6f, smkT, swapN - sw0, swapLateN - late0, swapWaitN - wait0, swapOld);
               smkWrite(s); }
  feCapFlush();   // правка 293 (служебное fecap): кадры перелёта из ОЗУ — на карту (после перелёта, кадры не тормозит)
}
// главное меню → LOAD GAME: слоты оригинала (fsave.shr, viewer/savemenu.c) над гостиной; 1 — выбрана запись (svmLoaded)
static int saveMenuRun(int mainSel, void **fbp) {
  if (svmOpen(SVM_LOAD, NULL, QUIT_NOGAME_OLD ? levelReady : (levelReady && !menuNoGame), 0)) return 0;   // игра уже шла — «Loading this data will cause ... progress to be lost»
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last; int r;
  do {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);   // №706: зажатая стрелка листает список (0,4 с, затем 0,12 с)
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    r = svmInput(hit);
    sceGuStart(GU_DIRECT, list); resetState();
    sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    feDraw(mainSel); resetState(); svmDraw((t - t0) * 1e-6f); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
  } while (r == SVM_R_NONE);
  return r == SVM_R_LOADED;
}
#include "bonus_play.h"   // правка 420: бонус-игра (MINI GAME) — выбор, гонка, итоги
static void introRun(void **fbp, unsigned first, unsigned end);
// правка 448 (Роб, 10.10): LOAD GAME после загрузки слота возвращает в главное меню (RESUME GAME — с этим слотом), а не стартует игру.
// LOAD_TO_MENU_OLD=1 — как было: игра стартует сразу. Сверки с PS2 нет — решение Роба (удобство: сразу в бонус-гонки)
#ifndef LOAD_TO_MENU_OLD
#define LOAD_TO_MENU_OLD 0
#endif
static void menuRun(void **fbp) {
  // правка Роба 239 (03.10: «вышел из игры в главное меню — музыки нет»): QUIT GAME из паузы оставлял набор шин «Pause» (тишина:
  // музыка, эффекты = 0) — главный цикл, который его снимает, в меню не идёт; и миссия могла держать музыку (тема меню не включалась)
#if !FE_PAUSE_OLD   // красный контроль гейта tests/fe_music/quit.sh: -DFE_PAUSE_OLD=1 — прежнее
  sndPause(0); sndLetterbox(0); sndMusicRelease(); hnrMusic = 0;
#endif
  if (levelReady) levelResetUi980(); uiPause(0); if (levelReady) { cellsDropAll(); mvShutdown(); mcrFreeAll(); msnartRoom = 0; }   // пункт 8: модели и маркеры миссий — из кучи, NEW GAME загрузит заново
  sndEngine(0.f, 0.f, 0); sndSkid(0.f, 0); sndCarAir(0);
#ifndef C3_ENDING_OLD
  if (endingPending) { endingPending = 0; creditsRun(fbp); }   // №860: титры конца игры — после записи, уровень уже выгружен; потом меню
#endif
  // музыка меню оригинала: FE_region музыкальной композиции (l?_music.rms) — Simpsons_Theme
  // 29.09: режим тюнера меню — обычный (значения SetSfxVolume / SetMusicVolume без префикса), не JustMusic: в JustMusic шина
  // эффектов 0, а звуки меню оригинала (frontend.spt: scroll, accept, back, locked_out) в меню слышны. Правило порта
  sndMode(SND_MODE_NORMAL); sndMusic(SND_MUS_FRONTEND); sndMaster(1.f);
  if (smkOn) { SndStats ss; sndStats(&ss); char s[96]; snprintf(s, sizeof s, "FEBACK mode=%d state=%d t=%.1f\n", ss.modeApplied, ss.musicState, smkT); smkWrite(s); }
  feGagMenu(0);
  // правка Роба 109 (30.09): после включения — титульный оригинала (логотип и «Press START»), как Splash перед меню; без нажатий
  // DEMO_IDLE_S — ролик demo по кругу (titleIdle); любая кнопка — меню. Только первый вход после запуска (из игры — сразу меню);
  // служебный режим (смоук, гейты меню) — как раньше, сразу меню
  { static int bootTitleDone; if (!bootTitleDone) {
      bootTitleDone = 1;
      if (!svcMode || feColdRequested) {
        coldTitleWait = 1; titleIdle(0.f, fbp); coldTitleWait = 0;
        // №431: the intro belongs before Press Start, as in the previous PSP build.
        if (FE_START_INTRO_OLD && !FE_COLD_FLOW_OLD) {
          introRun(fbp, 4, 5);
          sndMode(SND_MODE_NORMAL); sndMaster(1.f); sndMusic(SND_MUS_FRONTEND);
        }
      }
    } }
  // правка 421: до первой игры трассы бонус-игры — по самому свежему сохранению (автозагрузка оригинала: RESUME GAME), после —
  // игры в памяти; до строки MENU (её on= — пункт BONUS GAME уже в нужном состоянии)
  SaveGame lg_; int hl_ = svmLatest(&lg_);
  if (!bonusSession) bonusOpenFromSave(hl_ ? &lg_ : NULL, hl_ ? "latest" : "none"); else bonusOpenSet(bonusOpen, "game");
  if (!feReady()) {
    if (fragMenuMB) { fragHeap(fragMenuMB << 20); fragMenuMB = 0; }   // №841 тест: куча как после долгой игры на PSP (эмулятор всегда чистый)
    feLoadShown(UI_LOAD_FE, "load", fbp);
    // №841 (Роб 19.10, TEST38: «после QUIT GAME главное меню без 3D гостиной: текст и рамка есть, комнаты нет»): после выхода из игры уровень
    // остаётся в куче (~34 МБ, чтобы продолжить без загрузки), а гостиной (fe.shr 5,2 МБ) нужен один сплошной кусок — на PSP после долгой игры
    // куча раздроблена, кусок не находится, feLoad без памяти, меню без комнаты. Гостиная не прочлась — уровень в кучу (levelUnload, loadedLevel 0:
    // старт из меню прочтёт его заново, как после бонус-игры) и гостиная ещё раз. FE_MEM841_OLD=1 — как было
    if (!FE_MEM841_OLD && !feReady() && loadedLevel) {
      unsigned mb0_ = smkOn ? memMaxBlock() : 0; levelUnload(); unsigned mb1_ = smkOn ? memMaxBlock() : 0;
      feLoadShown(UI_LOAD_FE, "load", fbp);
      if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "FERETRY841 unload maxblk=%u->%u ok=%d lva=%u t=%.1f\n", mb0_, mb1_, feReady(), lvaUsed, smkT); smkWrite(s_); }
    }
  }
  feGagMenu(1);
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), idleT0 = last;   // зажатое с паузы — не нажатие
#ifndef MENU_RESUME_DEFAULT_OLD
#define MENU_RESUME_DEFAULT_OLD 0   // 1 — прежнее: главное меню всегда открывается на NEW GAME
#endif
  static int menuFirstOpen = 1;   // PS2 (knowledge/facts/ps2-resume-save-flow.yaml): при старте с сохранением меню открывается на RESUME GAME; возврат в меню после QUIT картой не описан — как раньше (NEW GAME)
  int sel = (!MENU_RESUME_DEFAULT_OLD && menuFirstOpen && hl_ && mainResume >= 0) ? mainResume : mainNew, act = -1; char s[128];
  menuFirstOpen = 0;
  SaveGame menuLoaded; int menuLoadOn = 0;   // правка 448: слот, загруженный на этом экране меню (LOAD GAME возвращает в меню, RESUME GAME берёт его)
  if (mainResume >= 0) uiMenuEnable(UI_MENU_MAIN, mainResume, hl_);   // RESUME GAME — есть целое сохранение
  if (mainScrap >= 0) uiMenuEnable(UI_MENU_MAIN, mainScrap, !SCRAP862_OLD && (hl_ || (bonusSession && haveColl)));   // №862: SCRAP BOOK — есть запись или игра этой сессии
  if (mainResume >= 0 && !MENU_RESUME_DEFAULT_OLD) uiMenuHide(UI_MENU_MAIN, mainResume, !hl_);   // PS2: на чистой карте пункта RESUME GAME в меню нет (NEW GAME, LOAD GAME…); стрелки обходят
  while (act != mainNew) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);   // №706: зажатая стрелка листает список (0,4 с, затем 0,12 с)
    SmokeCmd sc; while (smokeNext(dt, &sc)) if (sc.op == 2 && !strcmp(sc.name, "loadshot")) { loadShot = 1; loadShotAt = sc.a; } else if (sc.op == 2 && !strcmp(sc.name, "newslvred")) newsLvRed = (int)sc.a; else if (sc.op == 2 && !strcmp(sc.name, "framesheet")) { fsInt = sc.a; fsT0 = 0; fsN = 0; } else if (sc.op == 2 && !strcmp(sc.name, "newsshot")) { newsShotOn = (int)sc.a; newsShotK = 0; if (smkOn) smkWrite("NEWSSHOT on\n"); }
                                            else if (sc.op == 2 && !strcmp(sc.name, "feshot")) { feShot = 1; feShotAt = sc.a; }   // гейт: LoadingFE со следующего выхода в меню
                                            else if (sc.op == 2 && !strcmp(sc.name, "feglow")) feGlowTest = (int)sc.a;   // гейт: подсветка пункта N
                                            else if (sc.op == 2 && !strcmp(sc.name, "scrapon")) { scrapAllTest = sc.a != 0.f; if (mainScrap >= 0) uiMenuEnable(UI_MENU_MAIN, mainScrap, 1); }   // №862 служебно: SCRAP BOOK открыт, все карточки собраны
                                            else if (sc.op == 2 && !strcmp(sc.name, "fegag")) feGagForce = (int)sc.a;    // №404: сценка N сразу
                                            else if (sc.op == 2 && !strcmp(sc.name, "fegagat")) feGagAt = sc.a;          // №404: поза сценки на С (-1 играет)
                                            else if (sc.op == 2 && !strcmp(sc.name, "fegagauto")) feGagAuto = sc.a != 0.f;   // №404: 0 — без расписания
                                            else if (sc.op == 2 && !strcmp(sc.name, "fegagseg")) feGagSeg = (int)sc.a;   // №404: отрезок сценки Гомера
                                            else if (sc.op == 2 && !strcmp(sc.name, "flynoframe")) flyNoFrame = 1;   // отрицательный контроль правки 49
                                            else if (sc.op == 2 && !strcmp(sc.name, "loadfast")) ldFast = 1;   // отрицательный контроль: без минимального показа
                                            else if (sc.op == 2 && !strcmp(sc.name, "spinlag")) ldLag = 1;     // отрицательный контроль: рывки разворота
                                            else if (sc.op == 2 && !strcmp(sc.name, "loadpumpoff")) loadPumpOff = 1;   // правка 244: красный — без подкачки кадров
                                            else if (sc.op == 2 && !strcmp(sc.name, "loadblinkoff")) loadBlinkOff = 1;   // правка 244: красный — LOADING не мигает
                                            else if (sc.op == 2 && !strcmp(sc.name, "startnoloadold")) startNoLoadOld = (int)sc.a;   else if (sc.op == 2 && !strcmp(sc.name, "loadwaitlose")) lwLose = (int)sc.a;   else if (sc.op == 2 && !strcmp(sc.name, "loadwaitms")) { lwMs = (unsigned)sc.a; loadFuseIoLimitMs((unsigned)sc.a); }   else if (sc.op == 2 && !strcmp(sc.name, "loadreadhang")) loadFuseIoHang((int)sc.a);   else if (sc.op == 2 && !strcmp(sc.name, "loadwaitold")) { loadWaitOld = (int)sc.a; loadFuseIoWaitOld(loadWaitOld); }   else if (sc.op == 2 && !strcmp(sc.name, "startnewsold")) startNewsOld = (int)sc.a; else if (sc.op == 2 && !strcmp(sc.name, "newsiris")) newsIris = (int)sc.a;   // №576: откат на экран загрузки
                                            else if (sc.op == 2 && !strncmp(sc.name, "lv", 2)) levelService(sc.name, sc.a);   // уровни: lvset N, lvnosave, lvleak
                                            else if (sc.op == 2 && !strcmp(sc.name, "uisfx0")) uiSfxService(sc.name, sc.a);   // отрицательный контроль: звуки меню молчат
                                            else if (sc.op == 2 && !strcmp(sc.name, "femusold")) feMusicMode = 2;   // отрицательный контроль правки 134
                                            else if (sc.op == 2 && !strcmp(sc.name, "newgamefmv")) newGameFmv = 1;   // служебно: ролик новой игры fmv1a
                                            else if (sc.op == 2 && !strncmp(sc.name, "bonus", 5)) bnsFlag(&sc);   // правка 420: служебные флаги бонус-игры
                                            else idleFlag(&sc);   // простой меню / демо и их отрицательные контроли
    if (pad.Buttons) idleT0 = t;   // любая кнопка (и зажатая) — не простой
    if ((t - idleT0) * 1e-6f >= menuIdleS) {   // правка Роба 33: долго никто не играет — титульный оригинала (потом ролик простоя), любая кнопка — назад
      feGagMenu(0);
      titleIdle((t - idleT0) * 1e-6f, fbp);
      feGagMenu(1);
      menuFrame(sel, fbp);                     // первый кадр меню сразу (отклик «кнопка -> меню» — до этого кадра)
      if (smkOn && titleExitAt) { snprintf(s, sizeof s, "MENUBACK lat_ms=%.0f iris_ms=%u t=%.1f\n", (sceKernelGetSystemTimeLow() - titleExitAt) / 1000.f - titleIrisMs,
                                           titleIrisMs, smkT); smkWrite(s); }   // правка 154: отклик — до начала ириса (ирис — сам ответ на кнопку)
      sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); continue; }
    if (svcMode && (hit & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))) {   // служебный выбор уровня: L — предыдущий, R — следующий набор
      int d = (hit & PSP_CTRL_RTRIGGER) ? 1 : -1, n = menuLevel;
      for (int k = 0; k < 7; k++) { n = (n + d + 6) % 7 + 1; if (levelAvailable(n)) break; }
      menuLevel = n; if (smkOn) { snprintf(s, sizeof s, "MENULEVEL n=%d t=%.1f\n", menuLevel, smkT); smkWrite(s); }
      hit &= ~(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER); }
    act = uiMenuInput(UI_MENU_MAIN, &sel, hit);
    if (act >= 0 && act == mainMini) {   // правка 420: MINI GAME — бонус-игра (перелёта к треку в данных нет: камера стоит)
      feGagMenu(0); menuFly(act, FE_GO, sel, fbp); bonusRun(sel, fbp); feHome(); feGagMenu(1); hit = 0;
      act = -1; sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); }
    if (act >= 0 && act == mainScrap) {   // №862: SCRAP BOOK — перелёт к альбому, карточки всех уровней (main_save.h scrapRun), перелёт назад
      unsigned char c_[SV_LEVELS]; scrapCards(c_, menuLoadOn ? &menuLoaded : NULL);
      feGagMenu(0); menuFly(act, FE_GO, sel, fbp); scrapRun(sel, fbp, c_); menuFly(act, FE_BACK, sel, fbp); feHome(); feGagMenu(1);
      act = -1; sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); }
    if (act >= 0 && act == mainOpt) {   // OPTIONS: перелёт к дивану, экраны оригинала Options / Sound (menu.shr), перелёт назад
      feGagMenu(0);
      menuFly(act, FE_GO, sel, fbp); optionsRun(sel, fbp); menuFly(act, FE_BACK, sel, fbp); feHome();
      feGagMenu(1);
      act = -1; sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); }
    if (act >= 0 && svmOn() && (act == mainLoad || act == mainResume)) {   // LOAD GAME: перелёт к видику, слоты; RESUME GAME: к ТВ, самое свежее
      feGagMenu(0);
      int it = act, loadedBack = 0; menuFly(it, FE_GO, sel, fbp);
      int ok = (act == mainResume && menuLoadOn && !LOAD_TO_MENU_OLD) ? (svPending = menuLoaded, 1) : act == mainResume ? svmLatest(&svPending) : saveMenuRun(sel, fbp);
      if (ok && act == mainLoad) svPending = *svmLoaded();
#if !LOAD_TO_MENU_OLD
      if (ok && act == mainLoad) {   // правка 448: слот загружен — назад в главное меню (появляется RESUME GAME), игра не стартует; бонус-гонки открыты по его карточкам
        menuLoaded = svPending; menuLoadOn = 1; bonusOpenFromSave(&menuLoaded, "loaded"); if (mainScrap >= 0 && !SCRAP862_OLD) uiMenuEnable(UI_MENU_MAIN, mainScrap, 1);
        if (mainResume >= 0) uiMenuEnable(UI_MENU_MAIN, mainResume, 1);
        ok = 0; loadedBack = 1;
        if (smkOn) { snprintf(s, sizeof s, "LOADBACK level=%d resume=%d t=%.1f\n", svPending.level, mainResume, smkT); smkWrite(s); }
      }
#endif
      if (!ok) { menuFly(it, FE_BACK, sel, fbp); feHome(); }
      if (loadedBack && mainResume >= 0) sel = mainResume;   // курсор на RESUME GAME
      svResumeOn = ok; act = ok ? mainNew : -1;   // дальше — как NEW GAME: загрузка уровня, потом запись (перелёт к газете не нужен — камера у предмета)
      if (!ok) feGagMenu(1);
      sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); }
#ifndef RESUME_LOCK_OLD
#define RESUME_LOCK_OLD 0   // 1 — прежнее: × на сером пункте (RESUME GAME, SCRAP BOOK, BONUS GAME) ещё и облетает камерой к предмету туда-обратно; 0 — только звук locked_out (данные Scrooby: у серого пункта лишь locked_out; недоступный пункт не ведёт себя как выбранный)
#endif
    else if (RESUME_LOCK_OLD && act < 0 && (hit & PSP_CTRL_CROSS) && !uiMenuEnabled(UI_MENU_MAIN, sel)) {   // неготовый пункт: туда и обратно (только прежнее поведение)
      feGagMenu(0);
      menuFly(sel, FE_ROUND, sel, fbp); feHome();                                         // (нет обратного в данных — стоим)
      feGagMenu(1);
      sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = idleT0 = sceKernelGetSystemTimeLow(); }
    if (smkOn && (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_CROSS))) {
      snprintf(s, sizeof s, "MENUSEL key=%s sel=%d act=%d t=%.1f\n", (hit & PSP_CTRL_CROSS) ? "cross" : (hit & PSP_CTRL_UP) ? "up" :
               (hit & PSP_CTRL_DOWN) ? "down" : (hit & PSP_CTRL_LEFT) ? "left" : "right", sel, act, smkT); smkWrite(s); }
    menuFrame(sel, fbp);
  }
  if (smkOn) { SndStats ss; sndStats(&ss); snprintf(s, sizeof s, "FEMUS n=%d music=%s state=%d flights=%d t=%.1f\n", nMenu,   // музыка меню —
                                                    ss.music, ss.musicState, feStats.flights, smkT); smkWrite(s); }   // до стингера NEW GAME
  feGagMenu(0);
  if (!svResumeOn) { if (feMusicMode == 2) feStinger("loading_screen"); menuFly(act, FE_GO, sel, fbp); }
  if (transitBusy()) transitStep(100.f);   // правка 154: ирис титульного не доигран — в игру он не переходит
  // загрузка уровня: музыка меню и прочее не звучат, звучит только стингер газеты (правка 20; дальше тишина до игры)
  unsigned t0 = sceKernelGetSystemTimeLow();
  if (smkOn) { snprintf(s, sizeof s, "MENUGO item=%d t=%.1f\n", act, smkT); smkWrite(s); }
  if (smkOn) { snprintf(s, sizeof s, "FE n=%d ok=%d bytes=%u load=%.0f frames=%d fps=%.1f cpu=%.2f tris=%d pieces=%d bills=%d gags=%d gagstarts=%d\n", nMenu, feReady(),
                        feStats.bytes, feStats.loadMs, feStats.frames, feStats.fps, feStats.cpuMs, feStats.tris, feStats.pieces, feStats.bills,
                        feStats.gags, feStats.gagStarts); smkWrite(s); }
  feAudioFree(); feGagEvent = 0; feFree();
  if (smkOn) { snprintf(s, sizeof s, "FEMEM n=%d before=%u after=%u\n", nMenu, feU0, (unsigned)mallinfo().uordblks); smkWrite(s); }
  // какой уровень: NEW GAME — 1 (как в оригинале; служебный режим — выбранный L/R), LOAD / RESUME — уровень записи
  gameLevel = svResumeOn ? (lvNoSave ? 1 : svPending.level) : (svcMode ? menuLevel : 1);
  if (!levelAvailable(gameLevel)) gameLevel = 1;   // набора уровня нет рядом с EBOOT — уровень 1 (строка LEVEL покажет)
  int sw = levelReady && loadedLevel != gameLevel;   // в памяти набор другого уровня — сменить под экраном загрузки
  int news_ = !(!startNoLoadOld && startNewsOld && !sw && !loadShot);
  if (smkOn) { snprintf(s, sizeof s, "STARTNEWS path=%s level=%d resume=%d switch=%d t=%.1f\n", news_ ? "news" : "iris", gameLevel, svResumeOn, sw, smkT); smkWrite(s); }   // №663: газета на старте из меню (гейт tests/levels/start_news.sh)
  if (!startNoLoadOld && startNewsOld && !sw && !loadShot) {
    // №576 (Роб 12.10): старт игры из меню (New Game / Continue) — убрать мелькающий экран загрузки,
    // сразу плавный ирис в комнату (дом Симпсонов). START_NOLOAD_OLD=1 — прежнее поведение с газетой
    if (transitReady()) transitClose(fbp, "menu_room");
    newsShotIris();   // №663: красный контроль гейта: те же служебные снимки на чёрном (ирис без газеты)
    if (!svResumeOn && !musOld) { audioHoldStreams(1); levelHold = 1;
      if (smkOn) { snprintf(s, sizeof s, "SNDHOLD on t=%.1f\n", smkT); smkWrite(s); } }
    menuT0 = t0;
    if (!levelReady) return;
    menuStartGame(fbp, t0, 0);
    return;
  }
  int unl_ = 0;   // №1065: см. levelAdvance
  { int rc_ = scrOpen1065("menustart"); if (memLoad1065 && rc_ == -3 && sw) { levelUnload(); unl_ = 1; scrOpen1065("menustart-unloaded"); } scrTvOver = 0; }   // загрузка уровня: Loading.scr (газета уровня gameLevel) — на время загрузки
  feNewsStinger("newspaper_spin");   // правки 20, 134: газета влетает — один Newspaper_Spin с доли темы меню
  ldBase = levelReady && !sw ? 0 : LOAD_RES_SHARE; loadBegin(fbp);   // правка Роба 35: газета влетает до чтения, огонь без возврата
  if (!svResumeOn && !musOld) { audioHoldStreams(1); levelHold = 1;   // правка 155: NEW GAME — звук уровня после газеты и ролика
    if (smkOn) { snprintf(s, sizeof s, "SNDHOLD on t=%.1f\n", smkT); smkWrite(s); } }
  menuT0 = t0;
  if (!levelReady) return;   // первое меню: уровень ещё не читался — main читает его под этим экраном, потом menuStartGame
  if (sw) { if (!unl_) levelUnload(); levelLoad(fbp); }
  menuStartGame(fbp, t0, sw);   // сменили уровень — и его комнаты и камеры (levelLoadAfter)
}

// Интро оригинала (Роб 28.09): после заставки GONE Makers — родные логотипы создателей и ролик, потом главное меню.
// Порядок — SLES_518.97 (viewer/movie.h), START пропускает ролик. Служебные запуски — только с «flag intro» в сценарии:
// там сценарий идёт и здесь (press/wait/shot; на снимке ролик стоит, строка INTROSHOT). Итог — intro.txt рядом с EBOOT
static void introMem(const char *when) {   // smoke.txt: куча до и после интро (гейт: одна ±64 КБ)
  if (!smkOn) return;
  unsigned lo = 0, hi = 64u << 20;
  while (hi - lo > (16u << 10)) { unsigned m = (lo + hi) / 2; void *p = malloc(m); if (p) { free(p); lo = m; } else hi = m; }
  char s[96]; snprintf(s, sizeof s, "INTROMEM %s used=%u maxblk=%u t=%.1f\n", when, (unsigned)mallinfo().uordblks, lo, smkT); smkWrite(s);
}
static void introRun(void **fbp, unsigned first, unsigned end) {
  static const char *clip[] = {"movies/vuglogo.smv", "movies/foxlogo.smv", "movies/gracie.smv", "movies/radlogo.smv", "movies/intro.smv"};
  char fn[256], s[256], log[2048]; int nl, r; MovieStats st;
  introMem("before");
  r = movieSystem(1); nl = snprintf(log, sizeof log, "INTROSYS jpeg=%d\n", r);
  if (r >= 0) {
    sndMusic(SND_MUS_OFF); sndMaster(0.f);   // музыка свободной езды не звучит поверх роликов
    SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
    unsigned prev = pad.Buttons | smokePad();   // зажатое с заставки — не нажатие
    for (unsigned i = first; i < end && i < sizeof clip / sizeof *clip; i++) {
      int err, skip = 0, shot = 0; sibling(fn, clip[i]); Movie *m = movieOpen(fn, &err);
      unsigned t0 = sceKernelGetSystemTimeLow(), last = t0;
      while (m && movieUpdate(m)) {
        unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
        sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
        unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
        if (hit & PSP_CTRL_START) { skip = 1; break; }
        SmokeCmd sc; while (smokeNext(dt, &sc)) if (sc.op == 2 && !strcmp(sc.name, "tvframe")) tvFrameOn = 1;   // остальные флаги здесь не действуют
        if (!!smkShot[0] != shot) {                  // снимок окна: ролик стоит, пока ppsspp_run.sh не снимет
          shot = !!smkShot[0]; moviePause(m, shot);
          if (shot) { snprintf(s, sizeof s, "INTROSHOT name=%s clip=%u frame=%d t=%.1f\n", smkShot, i, movieFrame(m), smkT); smkWrite(s); }
        }
        sceGuStart(GU_DIRECT, list); movieDrawTv(m); sceGuFinish(); sceGuSync(0, 0);   // кадр с фоном по бокам (правка 49)
        sceDisplayWaitVblankStart(); *fbp = swapBuf();
      }
      memset(&st, 0, sizeof st); movieClose(m, &st); if (!m) st.err = err;
      snprintf(s, sizeof s, "INTRO clip=%u frames=%d shown=%d dropped=%d late=%d under=%d audio=%u dec=%.1f read=%.1f kbs=%.0f "
               "stride=%d err=%d skip=%d ms=%u t=%.1f\n", i, st.frames, st.shown, st.dropped, st.late, st.underruns, st.audio,
               st.decMsMax, st.readMsMax, st.readKBs, st.stride, st.err, skip, (sceKernelGetSystemTimeLow() - t0) / 1000, smkT);
      smkWrite(s); if (nl < (int)sizeof log) nl += snprintf(log + nl, sizeof log - nl, "%s", s);
    }
    movieSystem(0);
  }
  if (nl > (int)sizeof log - 1) nl = sizeof log - 1;
  sibling(fn, "intro.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | (first ? PSP_O_APPEND : PSP_O_TRUNC), 0777);
  if (f >= 0) { sceIoWrite(f, log, nl); sceIoClose(f); }
  introMem("after");
}

// лицензия держится не меньше LICENSE_MS (число порта: в данных длительности нет, на эталоне PS2 build/ps2ref/boot_15s — один
// кадр), служебные запуски без интро не ждут; потом BootupLoad — до конца загрузки игры
#define LICENSE_MS 3000
static void bootLicense(unsigned t0, void **fbp) {
  if (uiScrReady() && (!svcStartP() || smokeWantsIntro(path)))
    while (sceKernelGetSystemTimeLow() - t0 < LICENSE_MS * 1000u) scrFrame(UI_BOOT_LICENSE, 0, 1, fbp);
  scrFrame(UI_BOOT_LOAD, 0, 1, fbp); scrFrame(UI_BOOT_LOAD, 0, 1, fbp);
  if (bootShots) scrShot("bootload", "boot", UI_BOOT_LOAD, "-");
}
