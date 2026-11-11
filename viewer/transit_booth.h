// Build 95 П2 (спец Б.3): класс BOOTH через ldrRun. Ступени тела будки (№1051, №1060, №1225) становятся ступенями FREE_MORE машины переходов:
//   FREE (PLAN): клетки дальше 50 м (boothRoom1051(0), «pre»);  load: phoneViewOpen;
//   1: клетки дальше 20 м (r20); 2: все чужие клетки (all); 3: generic «все клетки в кучу» (сцена №1060: прелоадер уже на экране, мир не виден);
//   4: пересборка уровня boothRebuild1060 (последний рубеж; сама держит экран, ldrRun перед ней закрывает свой прелоадер, rebuildStep=4); после удачной пересборки возвращать нечего (mem1060Back=0).
// Экран: нет, если блок под пакет есть; иначе прелоадер Itchy & Scratchy с самого начала (раньше первые ступени шли на глазах без экрана). SIMPLIFY: надпись «TELEPHONE BOOTH UNAVAILABLE» ставит вызывающий.
// TFSM95_OLD=1 / BOOTH_FSM95_OLD=1 / flag boothfsm95old 1 — прежнее тело в main_actbooth.h
#ifndef BOOTH_FSM95_OLD
#define BOOTH_FSM95_OLD 0
#endif
static int boothFsm95Old = BOOTH_FSM95_OLD;
typedef struct { const char *sd; void **fbp; int po; int steps; } BoothCtx;
static int boothLoadCb(void *u) {
  BoothCtx *x = (BoothCtx *)u;
  if (x->po == 0) return 1;   // пересборка уже открыла будку
  if (x->steps) uiPakSegMax1051(256u << 10);   // тесная куча: пакет кусками по 256 КБ (ступени №1051)
  x->po = phoneFailTest > 0 ? (phoneFailTest--, -1) : phoneViewOpen(mvShop(), x->sd);
  if (x->steps) uiPakSegMax1051(0);
  return x->po == 0;
}
static int boothFreeCb(void *u) { (void)u; if (!boothMem1051 || PHONE_ROOM842_OLD) return 0; unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes(); int n = boothRoom1051(0); if (n) memLadNote("booth", 2, b0, f0, "pre"); return n; }
static int boothStepCb(void *u, int step) {
  BoothCtx *x = (BoothCtx *)u; x->steps = step; unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes();
  if (step == 1 || step == 2) { boothRoom1051(step); memLadNote("booth", step == 1 ? 3 : 4, b0, f0, step == 1 ? "r20" : "all"); return 1; }
  if (step == 4 && mem1060 && mem1060Rebuild) {
    x->po = boothRebuild1060(x->sd, x->fbp); memLadNote("booth", 4, b0, f0, x->po == 0 ? "rebuild-ok" : "rebuild-fail");
    if (x->po == 0 && !booth1225Old) mem1060Back = 0; else mem1060Back = 1;
    return 1;
  }
  return 0;   // ступень 3 и запасные: generic
}
// 0 — будка открыта (как phoneViewOpen); не 0 — не открыта (вызывающий пишет надпись)
static int boothOpenFsm(const char *sd, void **fbp, const float at[3]) {
  BoothCtx cx = {sd, fbp, -1, 0}; Transit t; memset(&t, 0, sizeof t);
  t.name = "booth"; t.need = 320u << 10; t.at[0] = at[0]; t.at[1] = at[1]; t.at[2] = at[2];
  t.screen = (memMaxBlock && memMaxBlock() >= t.need && !transitNoFit) ? TFS_NONE : TFS_PRELOADER;
  t.freeFn = boothFreeCb; t.load = boothLoadCb; t.stepFn = boothStepCb; t.rebuildStep = 4; t.u = &cx; t.noReturn = 1;
  int ok = ldrRun(&t, fbp);
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "BOOTH1051 result ok=%d t=%.1f\n", ok, smkT); smkWrite(s_); }
  if (!ok) { memLadNote("booth", 5, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), "simplify"); boothRelease1051(); mem1060Return(fbp); }
  return ok ? 0 : -1;
}
