// №1060 (Роб 27.10 вечер: «чтобы у нас всегда была возможность какого-то кэша для будки … если у нас нету его, сделать прелоадер и переход; нас никто не торопит; предусмотреть сценарии освобождения места
// и потом подгрузки его обратно»). Будка №1051 открывалась тремя ступенями (клетки дальше 50 м; пакет кусками по 256 КБ + дальше 20 м; все чужие клетки), и «TELEPHONE BOOTH UNAVAILABLE» оставался,
// если не помогли и они. Теперь четвёртая ступень, которая не подводит; она же — единый приём «освободить → загрузить → вернуть»:
//   1. СЦЕНА (boothScene1060): экран загрузки (газета уровня, Loading.scr — как у MISSION SELECT и переноса миссии, msnTeleport) закрывает кадр; под ним загрузчик стоит, все клетки города и пакет будки
//      про запас — в кучу, шкала растёт; газета гаснет, пакет будки читается кусками по 256 КБ в освободившееся место.
//   2. ПЕРЕСБОРКА (boothRebuild1060): не помогла и сцена (наибольший блок кучи меньше любой картинки пакета — дробление долгоживущими блоками, как у долгой игры на PSP): уровень выгружается и грузится
//      обратно под газетой тем же путём, что смена уровня (levelUnload / levelLoad / menuStartGame из сохранения в памяти), игрок и его машина встают на прежнее место — куча сложена заново.
//   3. ВОЗВРАТ (mem1060Return): после будки (или если открыть всё равно не вышло) клетки вокруг игрока читаются обратно под газетой (msnTeleport), стрим городом дальше ведёт сам.
// В обычной игре, когда места хватает (первые три ступени №1051 открыли будку), ничего этого нет: ни перехода, ни возврата, цена — ноль. Порог — по результату открытия (наибольший блок), не по сумме свободного.
// Журнал: BOOTH1060 scene / rebuild / back / fail (smoke.txt и чёрный ящик). MEM1060_OLD=1 / файл mem1060old рядом с EBOOT / flag mem1060 0 — как в №1051 (три ступени и «unavailable»);
// MEM1060_REBUILD_OLD=1 / файл mem1060rebuildold / flag mem1060rebuild 0 — без пересборки (только сцена)
#ifndef MEM1060_REBUILD_OLD
#define MEM1060_REBUILD_OLD 0
#endif
static int mem1060Rebuild = !MEM1060_REBUILD_OLD;   // mem1060 и mem1060Pre — в main_stream.h
static int mem1060Back;   // 1 — клетки города выгружены сценой: после будки вернуть под газетой
static int mem1060Scenes, mem1060Rebuilds;
// служебно (flag mem1060log N): строка MEM1060 stat — загруженных клеток, свободно, наибольший блок (гейт tests/mem1060: «всё освобождённое на месте, утечки нет»)
static void mem1060Stat(int tag) {
  int cells = 0; for (unsigned i = 0; i < ncell; i++) if (!cl[i].owner && cl[i].state == 2) cells++;
  char s_[160]; snprintf(s_, sizeof s_, "MEM1060 stat tag=%d cells=%d free=%.2f blk=%.2f scenes=%d rebuilds=%d t=%.1f\n", tag, cells, heapFreeBytes() / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, mem1060Scenes, mem1060Rebuilds, smkT);
  if (smkOn) smkWrite(s_);
}

static void mem1060Log(const char *what, unsigned f0, unsigned b0, int n, int ok) {
  char s_[200]; snprintf(s_, sizeof s_, "BOOTH1060 %s level=%d n=%d free=%.2f->%.2f blk=%.2f->%.2f ok=%d t=%.1f\n", what, gameLevel, n,
                         f0 / 1048576.f, heapFreeBytes() / 1048576.f, b0 / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, ok, smkT);
  if (smkOn) smkWrite(s_); bbNote(s_);
}
// загрузчик стоит, все клетки города и готовый пакет будки — в кучу; возвращает число выгруженных клеток
static int mem1060Free(void) {
  lqn = 0; __asm__ volatile("" ::: "memory");
  { LdWait lw_ = {0}; while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); sceKernelDelayThread(1000); } }
  ringState = 0;
  int n = cellsDropBeyond(-1.f);
  uiScrWarmClear();   // пакет будки, лежавший про запас
  return n;
}
// экран загрузки: газета уровня влетает над последним кадром мира; 1 — открыт
static int mem1060NewsIn(void **fbp) {
  char fn[256]; sibling(fn, "load.shr");
  uiPakSegMax1051(256u << 10);   // газета тоже кусками по 256 КБ: в раздробленной куче цельного мегабайта нет
  int rc = uiScrOpen(fn); uiPakSegMax1051(0);
  if (rc != 0) return 0;
  scrTvOver = 0; ldBase = 0; loadBegin(fbp); return 1;
}
static void mem1060NewsOut(void **fbp) { loadEnd(fbp); SCR_CLOSE(); }
// ПЕРЕХОД БЕЗ КУЧИ (слово дирижёра: «прелоадер и переход обязаны появляться именно когда памяти нет»): газета (load.shr ≈ 1,1 МБ цельными кусками) в тесной куче не открывается, поэтому переход рисуется
// вершинами GE в списке команд (sceGuGetMemory — память списка, не куча): затемнение кадра мира чёрным слоем с нарастающей альфой (8 кадров), затем «фитиль» экрана загрузки — шнур тянется слева направо,
// на конце искра; пройденная часть погасла. Текста нет (шрифты лежат в пакетах), украшений нет. Цвета — кремовый шнур и жёлто-оранжевая искра Loading.scr
typedef struct { unsigned c; short x, y, z, pad; } M1060V;
static int mem1060Frames;   // кадров перехода без кучи за последнюю сцену (журнал TRANS1060, гейт)
static void mem1060Frame(void **fbp, int permille, int fadeA) {   // fadeA 0..255 — чёрный слой поверх мира (без очистки); permille < 0 — только слой
  sceGuStart(GU_DIRECT, list); resetState(); sceGuScissor(0, 0, SCR_W, SCR_H);
  if (permille >= 0) { sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT); }
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  M1060V *v = (M1060V *)sceGuGetMemory(12 * sizeof(M1060V)); int m = 0;
  if (permille < 0) { v[m].c = (unsigned)fadeA << 24; v[m].x = 0; v[m].y = 0; v[m].z = 0; m++; v[m].c = (unsigned)fadeA << 24; v[m].x = SCR_W; v[m].y = SCR_H; v[m].z = 0; m++; }
  else {
    const int x0 = 70, x1 = SCR_W - 70, y = SCR_H - 62; int xh = x0 + (x1 - x0) * permille / 1000;
    unsigned t = sceKernelGetSystemTimeLow(), fl = (t / 90000u) & 1u;   // искра мерцает двумя кадрами, как огонь экрана загрузки
    v[m].c = 0xff90b0d0; v[m].x = (short)xh; v[m].y = (short)(y - 2); v[m].z = 0; m++; v[m].c = 0xff90b0d0; v[m].x = (short)x1; v[m].y = (short)(y + 2); v[m].z = 0; m++;   // непрогоревший шнур
    unsigned c1 = fl ? 0xff30e0ff : 0xff2090ff, c2 = fl ? 0xff2090ff : 0xff30e0ff;
    v[m].c = c1; v[m].x = (short)(xh - 7); v[m].y = (short)(y - 7); v[m].z = 0; m++; v[m].c = c1; v[m].x = (short)(xh + 7); v[m].y = (short)(y + 7); v[m].z = 0; m++;   // искра
    v[m].c = c2; v[m].x = (short)(xh - 3); v[m].y = (short)(y - 3); v[m].z = 0; m++; v[m].c = c2; v[m].x = (short)(xh + 3); v[m].y = (short)(y + 3); v[m].z = 0; m++;
  }
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, m, 0, v);
  resetState(); sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf(); mem1060Frames++;
}
static void mem1060Fade(void **fbp) {   // мир гаснет за 8 кадров (слой поверх незачищенного кадра: альфа растёт монотонно в обоих буферах)
  for (int i = 1; i <= 8; i++) mem1060Frame(fbp, -1, i == 8 ? 255 : 32 * i);
}
static int mem1060Bar;   // шкала, permille
static void mem1060BarTo(void **fbp, int to, int frames) {
  int from = mem1060Bar; if (frames < 1) frames = 1;
  for (int i = 1; i <= frames; i++) { mem1060Bar = from + (to - from) * i / frames; mem1060Frame(fbp, mem1060Bar, 0); }
}
static void mem1060TransLog(const char *what, int shot) {
  char s_[96]; snprintf(s_, sizeof s_, "TRANS1060 %s frames=%d bar=%d t=%.1f\n", what, mem1060Frames, mem1060Bar, smkT);
  if (smkOn) smkWrite(s_);
  if (shot && smkOn) { sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); smokeShotNow(strstr(what, "fe") ? "trans1060_fe" : "trans1060"); }   // кадр перехода — окном (шнур — trans1060, LoadingFE — trans1060_fe) (гейт: на экране шнур, не чёрный)
}
// №1060б (слово Роба: «не шнур, а наш экран загрузки при выходе из игры в меню — LoadingFE, Itchy & Scratchy, фитиль к бомбе; на будку и на любой случай, когда нужно подгрузить память»):
// переход = LoadingFE.scr из load.shr (viewer/main_frontend.h feLoadShown): затемнение без кучи → освободить → load.shr в освобождённое (кусками по 256 КБ) → LoadingFE с ходом загрузки →
// загрузка → load.shr закрыт до возврата клеток. Шнур вершинами GE (mem1060Frame) остаётся только запасным: load.shr не поместился и после освобождения; чёрного кадра нет.
// MEM1060FE_OLD=1 / файл mem1060feold / flag mem1060fe 0 — как было (газета уровня или шнур)
#ifndef MEM1060FE_OLD
#define MEM1060FE_OLD 0
#endif
static int mem1060Fe = !MEM1060FE_OLD;
static int mem1060FeOpen;            // load.shr открыт этим переходом (1060б)
static int mem1060WarmUsed;         // пакет будки читался про запас этим переходом (uiScrWarm): после будки снять
static int mem1060FeShot;            // снимок LoadingFE уже сделан в этой сцене
static void **mem1060FeFbp;
static int mem1060FeIn(void **fbp) {   // 1 — LoadingFE показан; 0 — не поместился (шнур)
  if (!mem1060Fe) return 0;
  char fn[256]; sibling(fn, "load.shr");
  uiPakSegMax1051(256u << 10); int rc = uiScrOpen(fn); uiPakSegMax1051(0);
  if (rc != 0) return 0;
  scrTvOver = 0; scrT0 = sceKernelGetSystemTimeLow(); mem1060FeOpen = 1; mem1060FeFbp = fbp; mem1060FeShot = 0;
  scrFrame(UI_LOAD_FE, 0, 1, fbp); mem1060Frames++; return 1;
}
static void mem1060FeFrame(void **fbp, int permille, int shot) {
  scrFrame(UI_LOAD_FE, permille, 1, fbp); mem1060Frames++; mem1060Bar = permille;
  if (shot && !mem1060FeShot && smkOn) { mem1060FeShot = 1; sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); smokeShotNow("trans1060_fe"); }
}
static void mem1060FeTo(void **fbp, int to, int frames, int shot) {
  int from = mem1060Bar; if (frames < 1) frames = 1;
  for (int i = 1; i <= frames; i++) mem1060FeFrame(fbp, from + (to - from) * i / frames, shot && i == frames);
}
static void mem1060FeOut(void) { if (mem1060FeOpen) { SCR_CLOSE(); mem1060FeOpen = 0; } }
// кадр экрана перехода между кусками чтения пакета (uiPakProgress1060)
static void mem1060PakHook(unsigned done, unsigned total) {
  if (!mem1060FeOpen || !mem1060FeFbp) return;
  mem1060FeFrame(mem1060FeFbp, 300 + (int)((unsigned long long)done * 600u / (total ? total : 1)), done * 2 >= total);
}
// переход «шнур или LoadingFE» одним вызовом: прогресс до to за frames кадров
static void mem1060Prog(void **fbp, int to, int frames, int shot) { if (mem1060FeOpen) mem1060FeTo(fbp, to, frames, shot); else mem1060BarTo(fbp, to, frames); }

// ступень 3 (сцена): место под будку. Возвращает результат phoneViewOpen (0 — открыта). Переход виден всегда: LoadingFE (если load.shr поместился в освобождённое), иначе шнур без кучи
static int boothScene1060(const char *sd, void **fbp) {
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock ? memMaxBlock() : 0; mem1060Scenes++; stBoothHold1051 = 1; mem1060Frames = 0; mem1060Bar = 0; mem1060FeShot = 0;
  mem1060Fade(fbp); mem1060BarTo(fbp, 30, 2);            // мир гаснет без кучи; шнур держит кадр, пока идёт освобождение (на экране не чёрный)
  int n = mem1060Free();                                 // загрузчик стоит, клетки и пакет про запас — в кучу
  int fe = mem1060FeIn(fbp);                             // load.shr в освобождённое: LoadingFE вместо шнура
  int gz = !mem1060Fe ? mem1060NewsIn(fbp) : 0;          // MEM1060FE_OLD: как в №1060 — газета уровня, если поместилась
  mem1060Log(fe ? "scene-fe" : gz ? "scene" : "scene-cord", f0, b0, n, 1);
  if (gz) { loadFrame(300, fbp); loadFrame(600, fbp); mem1060NewsOut(fbp); }
  else mem1060Prog(fbp, 300, 8, 0);
  char dn[256]; snprintf(dn, sizeof dn, "%s", sd);
  if (fe) {   // пакет будки читается кусками про запас (uiScrWarm), пока на экране LoadingFE с ходом; потом load.shr закрыт и будка открывается готовым пакетом
    uiPakProgress1060 = mem1060PakHook; uiPakSegMax1051(256u << 10);
    phoneViewWarmOld(0); mem1060WarmUsed = 1; phoneViewWarm(dn);
    uiPakSegMax1051(0); uiPakProgress1060 = 0;
    mem1060FeTo(fbp, 950, 6, 1); mem1060TransLog("scene-fe", 0);
    mem1060FeOut();                                      // load.shr закрыт до возврата клеток
  } else if (!gz) { mem1060Prog(fbp, 640, 12, 1); mem1060TransLog("scene", 1); }
  uiPakSegMax1051(256u << 10);
  int po = phoneViewOpen(mvShop(), sd);
  uiPakSegMax1051(0);
  if (!fe && !gz) mem1060BarTo(fbp, 900, 6);
  mem1060Log("scene-open", f0, b0, n, po == 0);
  return po;
}

// ступень 4 (пересборка кучи): уровень выгружается и грузится обратно, игрок и машина — на прежнем месте. Возвращает результат phoneViewOpen.
// Миссия: сохранение держит уровень, этап и пройденные миссии; внутриэтапное (таймеры, объекты, водители) не сериализуется — этап начинается заново, как после RESUME. Строка BOOTH1060 msn — до и после
static int boothRebuild1060(const char *sd, void **fbp) {
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock ? memMaxBlock() : 0, t0 = sceKernelGetSystemTimeLow(); mem1060Rebuilds++;
  float hx = homer.x, hy = homer.y, hz = homer.z, hh = homer.h, cx = car.x, cy = car.y, cz = car.z, ch = car.h;
  SaveGame carry; saveCapture(&carry);   // прогресс, монеты, карточки, разовые предметы — всё, что пишет сохранение
  char pb[160]; svProgressText(&carry, pb, sizeof pb);
  mem1060BarTo(fbp, 100, 6);
  levelResetUi980(); uiPause(0); cellsDropAll(); mvShutdown(); mcrFreeAll(); msnartRoom = 0; uiScrWarmClear();
  sndEngine(0.f, 0.f, 0); sndSkid(0.f, 0); sndCarAir(0); sndMusic(SND_MUS_OFF); sndMaster(0.f);
  int fe = mem1060FeIn(fbp), gz = 0;     // перед выгрузкой уровня load.shr обычно не помещается — тогда шнур, а LoadingFE откроется сразу после выгрузки
  if (!mem1060Fe) { gz = mem1060NewsIn(fbp); if (!gz) mem1060BarTo(fbp, 300, 8); }   // MEM1060FE_OLD: газета уровня, как в №1060
  else if (!fe) mem1060BarTo(fbp, 300, 8);
  levelUnload();
  if (!mem1060Fe) { if (!gz) { mem1060BarTo(fbp, 400, 4); gz = mem1060NewsIn(fbp); }
    if (gz) { ldBase = LOAD_RES_SHARE; sndMaster(1.f); feStinger("newspaper_spin"); } else { sndMaster(1.f); mem1060TransLog("rebuild-nonews", 1); } }
  else {
    if (!fe) { mem1060BarTo(fbp, 400, 4); fe = mem1060FeIn(fbp); }
    if (fe) { ldScr1060 = UI_LOAD_FE; ldBase = LOAD_RES_SHARE; loadBegin(fbp); sndMaster(1.f); mem1060TransLog("rebuild-fe", 1); }   // экран загрузки уровня на время пересборки — LoadingFE, ход чтения — loadPump
    else { sndMaster(1.f); mem1060TransLog("rebuild-cord", 1); } }
  levelLoad(fbp);
  svPending = carry; svResumeOn = 1;
  menuStartGame(fbp, t0, 1);                   // комнаты и камеры уровня, круг клеток вокруг старта, экран загрузки закрыт внутри
  ldScr1060 = UI_LOAD_LEVEL; mem1060FeOpen = 0;
  homerPlace(&homer, &world, hx, hy, hz, hh); carPlace(&car, &world, cx, cy, cz, ch); chase.init = 0;
  { SaveGame now; saveCapture(&now); char pa[160]; svProgressText(&now, pa, sizeof pa);
    char s_[400]; snprintf(s_, sizeof s_, "BOOTH1060 msn same=%d before=[%s] after=[%s] t=%.1f\n", !strcmp(pb, pa), pb, pa, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
  stBoothHold1051 = 1;
  phoneViewSetProgress(svLvReached(&lvKeep)); phoneViewSetCar(mvForcedCar(), car.destroyed ? 1.f : car.damage);   // как перед открытием в main_loop.h
  int po = phoneViewOpen(mvShop(), sd);
  mem1060Log("rebuild", f0, b0, (int)((sceKernelGetSystemTimeLow() - t0) / 1000u), po == 0);
  return po;
}

// после будки (или если открыть не вышло): освобождённое вернуть — клетки вокруг игрока читаются под газетой, дальше стрим
static void mem1060Return(void **fbp) {
  if (!mem1060Back) return;
  mem1060Back = 0; stBoothHold1051 = 0;
  if (mem1060WarmUsed) { phoneViewWarmOld(1); mem1060WarmUsed = 0; }   // пакет про запас, взятый переходом, — обратно в кучу до чтения клеток
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock ? memMaxBlock() : 0; int fe = 0;
  if (mem1060Fe && mem1060FeIn(fbp)) { mem1060FeOut(); fe = 1; }   // LoadingFE поместится — клетки читаются под ним (msnTeleport открывает load.shr сам)
  if (fe) { ldScr1060 = UI_LOAD_FE; msnTeleport(homer.x, homer.y, homer.z, fbp); ldScr1060 = UI_LOAD_LEVEL; }
  else if (!mem1060Fe) msnTeleport(homer.x, homer.y, homer.z, fbp);   // MEM1060FE_OLD: газета уровня, как в №1060
  else { mem1060BarTo(fbp, 1000, 14); }   // load.shr не открылся: шнур, клетки догрузит стрим
  mem1060Log(fe ? "back-fe" : "back", f0, b0, 0, 1);
}

// служебно (flag mem1060fedemo N): LoadingFE на ходе N из 1000 тем же путём, что в переходе, и снимок ref1060 — эталон для гейта (сверка с кадром перехода)
static void mem1060FeDemo(void **fbp, int pm) {
  if (!mem1060FeIn(fbp)) { if (smkOn) smkWrite("MEM1060 fedemo open-fail\n"); return; }
  mem1060Bar = 0; mem1060FeTo(fbp, pm, 4, 0);
  if (smkOn) { sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); smokeShotNow("ref1060"); }
  mem1060FeOut();
}
