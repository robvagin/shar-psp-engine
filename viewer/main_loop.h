#include "watchdog.h"
// сторож (watchdog.h): приёмник журнала и часы — как у остальных служебных строк смоука
static void wdSmkLine(const char *l) { if (!smkOn) return; char b[340]; snprintf(b, sizeof b, "%s\n", l); smkWrite(b); }
static float wdSmkClock(void) { return smkT; }
static int irisDt0Test1258;   // служебное flag irisdt0 1 (Будка-93, 1258)
static int irisStuckExempt1258(void);
// канон п.11 (вторая половина) и п.13: сцена города под точкой готова — клетки, покрывающие точку, в памяти (state 2). Нет покрытия (вода, пустота) — готова. Комната: её пол — это и есть сцена (резидентна)
static float smkSceneHoldEnd;   // служебное «flag scenehold S» (A1 Опора-93): сцена города «не готова» до smkT + S — холодная клетка под героем без зависимости от скорости чтения
static int sceneReadyMain(const ColWorld *w, float x, float z, int *failed) {
  if (failed) *failed = 0;
  if (smkOn && w == &world && smkT < smkSceneHoldEnd) return 0;
  if (w != &world || !ncell || !cl) return 1;
  int ready = 1;
  for (unsigned i = 0; i < ncell; i++) { const Cell *c = &cl[i]; if (c->owner) continue;
    if (x < c->bb[0] || x > c->bb[3] || z < c->bb[2] || z > c->bb[5]) continue;
    if (c->state == 3) { if (failed) *failed = 1; ready = 0; } else if (c->state != 2) ready = 0; }
  return ready;
}
static int mvSceneMain(float x, float z) { return sceneReadyMain(&world, x, z, 0); }   // персонажи миссии (msn_view.c npcVisible, NPCSCENE431_OLD)
// «клетку под точкой с высшим приоритетом»: неудавшиеся (state 3) снова в очередь загрузчика; память под неё освобождает лестница окна «Меню» (memLadderCell, необязательная — слабая ссылка)
extern int memLadderCell(int cell) __attribute__((weak));
#ifndef CELLREGFIX_OLD
#define CELLREGFIX_OLD 0
#endif
static int streamEyeOv;   // Лестница-96: flag streameye 1 (служебно, сцены с eyeOv: trfgo, phonego, bonusgo)
static int footFsm;   // Лестница-96 (№4б2): foot/goto сценария через transitClose + msnTeleport (flag footfsm 1)
static int cellRegFix = !CELLREGFIX_OLD; static unsigned cellRegFixN;   // CELLREGFIX_OLD=1 / «flag cellregfix 0» - сторож cellfail только просит повтор
static void sceneWantMain(const ColWorld *w, float x, float z) {
  if (w != &world || !ncell || !cl) return;
  for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || x < c->bb[0] || x > c->bb[3] || z < c->bb[2] || z > c->bb[5]) continue;
    if (c->state == 3) { if (memLadderCell) memLadderCell((int)i); c->state = 0; cellFail |= 2; cellFailT = 0; }   // повтор в ближайшем кадре
    if (cellRegFix && zgOn && c->state == 0 && CELL_REG(c) && CELL_REG(c) <= 16 && !(((regRes | regPre) >> (CELL_REG(c) - 1)) & 1u)) {   // Опора-93, этап 2 (L1: cellfail failed=0 каждые 4 с 175 с): под героем клетка региона, которого нет в наборе ворот (перенос без ворот) - стрим её не берёт, повтор не помогает
      regRes |= 1u << (CELL_REG(c) - 1); cellRegFixN++;
      if (smkOn) { char s[110]; snprintf(s, sizeof s, "CELLREGFIX cell=%u reg=%u res=%04x t=%.2f\n", i, CELL_REG(c), regRes, smkT); smkWrite(s); } }
    if (c->state == 0 && sceneBoost1257) { if (smkOn && sceneBoostCell != (int)i) { char s_[100]; snprintf(s_, sizeof s_, "SCENEBOOST cell=%u size=%u t=%.2f\n", i, c->size, smkT); smkWrite(s_); } sceneBoostCell = (int)i; sceneBoostUs = sceKernelGetSystemTimeLow(); }   // Будка-93 (1257): клетка под ждущим героем - в голову очереди загрузчика
    if (smkOn && c->state != 2) { char s[100]; snprintf(s, sizeof s, "SCENEWANT cell=%u state=%d at=%.1f,%.1f t=%.2f\n", i, c->state, x, z, smkT); smkWrite(s); } }
}
// сторож, вид cellfail: клетка города под героем не в памяти дольше WDG_CELL_T — просьба загрузить с приоритетом; строка WATCHDOG
static int wdCellCheck(void *u, float dt, char *info, int cap) {
  static float t; (void)u;
  if (!haveHomer || !haveCol || G.pHomerWorld || homerSpawnPending(&homer) || !onFoot()) { t = 0.f; return 0; }
  int failed = 0;
  if (sceneReadyMain(&world, homer.x, homer.z, &failed)) { t = 0.f; return 0; }
  t += dt; if (t < 4.f) return 0;
  t = 0.f; sceneWantMain(&world, homer.x, homer.z);
  snprintf(info, cap, "pos=%.2f,%.2f,%.2f failed=%d fix=requeue regfix=%u heap=%.2f", homer.x, homer.y, homer.z, failed, cellRegFixN, heapFreeBytes() / 1048576.f);
  return 1;
}
// №808 (Роб 18.10: «когда включается мультик — минимальный знак, что думает, чтобы понимать, что не зависло»): пока палитры пересчитываются фоновым
// заданием toneJob (включение мультика, насыщенность, контраст, доля текстуры; на PSP ~4 с) — внизу слева кольцо из 8 точек с бегущей яркой точкой;
// появляется через 0,15 с, плавно гаснет вместе с заданием. Без текстур, одним вызовом. BUSY808_OLD=1 — без знака
#ifndef BUSY808_OLD
#define BUSY808_OLD 0
#endif
// Правка Роба 432 служебно (flag heapfrag КБ): блоки по 32 КБ занимают всю свободную кучу, затем из них остаются каждый (КБ/32+1)-й — свободное лежит дырами по КБ; журнал HEAPFRAG
static void *fragBlk[3000]; static int fragN;
static void heapFragSet(int kb) {
  while (fragN) free(fragBlk[--fragN]);
  if (kb <= 0) { if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "HEAPFRAG off free=%.2f blk=%.2f\n", heapFreeBytes() / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f); smkWrite(l_); } return; }
  void *all[3000]; int n = 0; while (n < 3000 && (all[n] = malloc(32u << 10))) { memset(all[n], 1, 32u << 10); n++; }
  int step = kb / 32 + 1; for (int i = 0; i < n; i++) { if (i % step == step - 1) fragBlk[fragN++] = all[i]; else free(all[i]); }
  if (smkOn) { char l_[128]; snprintf(l_, sizeof l_, "HEAPFRAG hole=%dK held=%d free=%.2f blk=%.2f\n", kb, fragN, heapFreeBytes() / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f); smkWrite(l_); }
}
// Правка Роба 432 (06.11): бот меню MISSION SELECT (flag msnselfuzz СЕМЯ ЧИСЛО_ПОСЛЕДОВАТЕЛЬНОСТЕЙ). Каждая последовательность: случайное состояние прохождения уровня и отказов пакета,
// пауза, ↓ до пункта MISSION SELECT, ✕, 3..12 случайных кнопок меню (↑ ↓ ◄ ► L R, круг/треугольник/START), выход. Свидетель mvMsnSelOracle читает состояние независимо от самопроверки
// и после КАЖДОГО кадра открытого меню должен молчать (иначе строка MSNSELBAD). ✕ на строке не нажимается (старт миссии — отдельная проверка). Итог MSNSELFUZZ
#ifndef MSNFUZZWAIT_OLD
#define MSNFUZZWAIT_OLD 0   // 1 - сценарий ждёт фиксированное время, а не конец бота (на медленной машине итога MSNSELFUZZ нет)
#endif
static int msFuzzOn, msFuzzSeq, msFuzzMax, msFuzzSteps, msFuzzBad, msFuzzWait, msFuzzLeft = -1, msFuzzOpened; static unsigned msFuzzS;
static unsigned msFuzzRnd(void) { msFuzzS = msFuzzS * 1664525u + 1013904223u; return msFuzzS >> 8; }
static unsigned msFuzzHit(void) {
  if (msFuzzWait > 0) { msFuzzWait--; return 0; }
  if (msFuzzSeq >= msFuzzMax) { if (msFuzzOn == 1) { msFuzzOn = 2; if (smkOn && !MSNFUZZWAIT_OLD) smkWait = 0.f; if (smkOn) {   // Лестница-96: бот кончил - ожидание сценария снимается (прогон не зависит от скорости эмулятора)
     char l_[160]; snprintf(l_, sizeof l_, "MSNSELFUZZ seq=%d steps=%d opened=%d bad=%d fixes=%d fallback=%d lad=%u/%u/%u/%u fail=%u t=%.1f\n", msFuzzSeq, msFuzzSteps, msFuzzOpened, msFuzzBad, mvMsnSelCount(), mvMsnSelFallbacks(), memLadStep[1], memLadStep[2], memLadStep[3], memLadStep[4], memLadFail, smkT); smkWrite(l_); } } return 0; }
  if (mvMissionSelectOn()) {
    msFuzzSteps++; if (mvMsnSelOracle()) msFuzzBad++;
    if (msFuzzLeft < 0) msFuzzLeft = 3 + (int)(msFuzzRnd() % 10u);
    if (msFuzzLeft-- == 0) { msFuzzLeft = -1; msFuzzWait = 2; static const unsigned out_[3] = { PSP_CTRL_CIRCLE, PSP_CTRL_TRIANGLE, PSP_CTRL_START }; return out_[msFuzzRnd() % 3u]; }
    static const unsigned mv_[8] = { PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT, PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_DOWN, PSP_CTRL_UP };
    msFuzzWait = 1; return mv_[msFuzzRnd() % 8u];
  }
  if (!uiPaused()) {   // начало последовательности: новое состояние, пауза
    if (msFuzzLeft >= 0) msFuzzLeft = -1;
    msFuzzSeq++; unsigned r = msFuzzRnd();
    mvMsFuzzState(msFuzzRnd());
    { int kb_ = (r % 5u == 0u) ? 100 + (int)((r >> 8) % 300u) : 0; if (kb_ || fragN) heapFragSet(kb_); }   // иногда куча раздроблена дырами 100–400 КБ
    msOpenFail = (r % 6u == 0u) ? (int)(1 + (r >> 4) % 4u) : 0;   // иногда пакет не открывается 1–4 раза подряд (ступени и повтор должны справиться)
    msFuzzWait = 3; return PSP_CTRL_START;
  }
  if (msFuzzLeft == -2) { msFuzzLeft = -1; }
  if (uiPauseSelected() == UI_P_MISSION_SELECT) { msFuzzOpened++; msFuzzWait = 4; return PSP_CTRL_CROSS; }   // меню могло не открыться (серый пункт / нечего выбрать): следующий круг снова пауза
  msFuzzWait = 1;
  { unsigned r = msFuzzRnd() % 12u; return r < 8u ? PSP_CTRL_DOWN : r < 10u ? PSP_CTRL_UP : PSP_CTRL_START; }
}
static int carLights970;   // №970 служебно (flag carlights970 N): 1 — стоп-сигналы горят, 2 — задний ход, 3 — оба (для кадров блика огней без езды)
// №912 (оптимизация, x_props: «билборд = своё состояние смешивания + привязка + отдельный DrawArray на 4 вершины»): квады стейтпропов кадра (тень ящика, свет автомата, дым будки, дуги щитка)
// идут по одному в порядке кандидатов; подряд идущие квады с одной текстурой и одним режимом смешивания (BLMD) собираются в один буфер и рисуются одним вызовом (треугольники, 6 вершин на квад),
// состояние смешивания/тумана и привязка текстуры — один раз на группу. Порядок рисования не меняется (склеиваются только соседи), картинка та же. BB_BATCH_OLD=1 / «flag bbbatch 0|1» /
// файл bbbatchold рядом с EBOOT — прежнее (по вызову на квад).
#ifndef BB_BATCH_OLD
#define BB_BATCH_OLD 0
#endif
#ifndef BB_BATCH_MAX
#define BB_BATCH_MAX 8
#endif
static unsigned long long p7S[6];   // №919: PH7SPLIT — части ph7 (дальность, отсечение, streamMiss, cullCityEnd, остаток между ними)
static float cmV, cmYaw, cmA, cmP[2];   // служебное движение камеры (flag cammove / camyaw)
static int bbBatchOn = !BB_BATCH_OLD;
static void busyDraw(int on) {
  static unsigned t0; static float a;
  unsigned now = sceKernelGetSystemTimeLow();
  if (on) { if (!t0) t0 = now | 1u; } else t0 = 0;
  float want = (on && (unsigned)(now - t0) > 150000u) ? 1.f : 0.f; a += (want - a) * .25f;
  if (a < .02f) return;
  typedef struct { unsigned c; short x, y, z, pad; } BusyV;
  BusyV *v = (BusyV *)sceGuGetMemory(16 * sizeof(BusyV)); int m = 0;
  const float cx = 26.f, cy = SCR_H - 44.f, rr = 7.f, ph = (float)(now % 1000000u) * 8e-6f;   // 8 шагов в секунду
  for (int k = 0; k < 8; k++) {
    float an = k * .7853982f, px = cx + rr * sinf(an), py = cy - rr * cosf(an), d = fmodf(ph - (float)k + 16.f, 8.f);
    float lum = .3f + .7f * fmaxf(0.f, 1.f - d / 4.f); unsigned al = (unsigned)(235.f * a * lum);
    unsigned c = al << 24 | 0x00ffffffu;
    v[m].c = c; v[m].x = (short)(px - 1.5f); v[m].y = (short)(py - 1.5f); v[m].z = 0; m++;
    v[m].c = c; v[m].x = (short)(px + 1.5f); v[m].y = (short)(py + 1.5f); v[m].z = 0; m++;
  }
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, m, 0, v);
  resetState();
}
#ifndef BOOTH_CAM_SKIP_OLD
#define BOOTH_CAM_SKIP_OLD 0   // №792 (Роб 18.10): перебивка камеры будки (облёт и взгляд на новую машину) пропускается ✕ и START, как в оригинале (1 — прежнее)
#endif
#ifndef LB_START_SKIP_OLD
#define LB_START_SKIP_OLD 0   // Роб 17.10: START при чёрных полосах — пропуск, а не пауза (1 — прежнее)
#endif
#include "xsub.h"
#include "gebench1.h"
#include "acct.h"
extern unsigned gateSkinDraws, gateSkinBad;
#include "bisect.h"
// №639: выход из подэкрана паузы по △/○ — сначала подэкран гаснет 0,20 с (кнопка откладывается), потом закрывается и проявляется меню (PAUSE_SUB_FADE_OLD=1 — сразу)
#ifndef PAUSE_SUB_FADE_OLD
#define PAUSE_SUB_FADE_OLD 0
#endif
static int pxCl; static unsigned pxClT0, pxClHit;
// №739: выход из игры только из главного потока — звук меню и игры остановлен (смеситель и чтение музыки дождались конца), загрузчик клеток
// дочитал (не дольше 2 с), потом система. Зовут: кадр по exitReq (HOME, конец тура) и выход из меню; блэкбокс пишет вызывающий
static void exitSafeNow(void) {
  if (EXIT739_OLD) sceKernelExitGame();
  unsigned t0 = sceKernelGetSystemTimeLow(), w = 0;
  feAudioStop(); sndShutdown();
  for (; loaderUp && (!loaderIdle || loaderBusy) && w < 2000000u; w += 5000) sceKernelDelayThread(5000);
  if (smkFd >= 0) { char s_[96]; snprintf(s_, sizeof s_, "EXIT739 safe sound=off loader_wait_ms=%u total_ms=%u\n", w / 1000, (sceKernelGetSystemTimeLow() - t0) / 1000);
    smkWrite(s_); smkFlush(); sceIoClose(smkFd); smkFd = -1; }
  sceKernelExitGame();
}

// №557: остов в будке при свете «витрины»: у остова нет нормалей, поэтому нормаль грани считается по трём вершинам (наружу от центра куска), свет — направленный сверху-слева от камеры
// BOOTH_HUSK_AMB + BOOTH_HUSK_DIF * max(0, n·L), цвет вершины = запечённый цвет * свет, удвоение цвета текстуры (как у мира). Рисует кусок вместо drawPiece; 0 — формат не тот, рисовать обычным путём
#ifndef BOOTH_HUSK_AMB
#define BOOTH_HUSK_AMB .6f
#endif
#ifndef BOOTH_HUSK_DIF
#define BOOTH_HUSK_DIF 1.6f
#endif
static int boothHuskLit(const Piece *t, float c, float s, float ty) {
  int cnt = t->r.ni; if (cnt < 3 || !(t->r.flags & F_NOIDX) || !(t->r.flags & F_V12) || cnt % 3 || geOver(GE_HARD) || !pieceOk(t, 0)) return 0;
  const unsigned char *vb = (const unsigned char *)t->v;
  typedef struct { unsigned short u, v; unsigned col; short x, y, z, pad; } HV;
  HV *o = (HV *)sceGuGetMemory((unsigned)cnt * sizeof(HV)); if (!o) return 0;
  float cx = 0, cy = 0, cz = 0;
  for (int q = 0; q < cnt; q++) { const short *p = (const short *)(vb + q * 12 + 6); cx += p[0]; cy += p[1]; cz += p[2]; }
  cx /= cnt; cy /= cnt; cz /= cnt;
  // направление на свет в мировых осях: камера (-5.6, 2.5, -7.4) -> (0, .62, 0); экранное «вправо» r = d × up
  float d[3] = {5.6f, 2.5f - .62f > 0 ? -1.88f : 0.f, 7.4f}, dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); for (int i = 0; i < 3; i++) d[i] /= dl;
  float r[3] = {-d[2], 0.f, d[0]}, rl = sqrtf(r[0] * r[0] + r[2] * r[2]); r[0] /= rl; r[2] /= rl;
  float L[3] = {-.55f * r[0] - .35f * d[0], .75f - .35f * d[1], -.55f * r[2] - .35f * d[2]}, ll = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]); for (int i = 0; i < 3; i++) L[i] /= ll;
  for (int q = 0; q < cnt; q += 3) {
    float P[3][3];
    for (int k = 0; k < 3; k++) { const short *p = (const short *)(vb + (q + k) * 12 + 6); P[k][0] = p[0]; P[k][1] = p[1]; P[k][2] = p[2]; }
    float a[3] = {P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]}, b[3] = {P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]};
    float n[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    float m0[3] = {(P[0][0] + P[1][0] + P[2][0]) / 3 - cx, (P[0][1] + P[1][1] + P[2][1]) / 3 - cy, (P[0][2] + P[1][2] + P[2][2]) / 3 - cz};
    if (n[0] * m0[0] + n[1] * m0[1] + n[2] * m0[2] < 0.f) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; }
    float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]); if (nl < 1e-6f) nl = 1.f; for (int i = 0; i < 3; i++) n[i] /= nl;
    float w[3] = {c * n[0] + s * n[2], n[1], -s * n[0] + c * n[2]};   // модель -> мир (курс), как в drawPiece
    float nd = w[0] * L[0] + w[1] * L[1] + w[2] * L[2]; if (nd < 0.f) nd = 0.f;
    float f = BOOTH_HUSK_AMB + BOOTH_HUSK_DIF * nd;
    for (int k = 0; k < 3; k++) {
      const unsigned char *vv = vb + (q + k) * 12; unsigned c5 = *(const unsigned short *)(vv + 4);
      float rr = ((c5 & 31) * 255 / 31) * f, gg = (((c5 >> 5) & 63) * 255 / 63) * f, bl = (((c5 >> 11) & 31) * 255 / 31) * f;
      rr = rr > 255.f ? 255.f : rr; gg = gg > 255.f ? 255.f : gg; bl = bl > 255.f ? 255.f : bl;
      HV *h = &o[q + k]; h->u = *(const unsigned short *)vv; h->v = *(const unsigned short *)(vv + 2); h->col = 0xff000000u | (unsigned)bl << 16 | (unsigned)gg << 8 | (unsigned)rr;
      const short *p = (const short *)(vv + 6); h->x = p[0]; h->y = p[1]; h->z = p[2]; h->pad = 0;
    }
  }
  float k = t->r.sc, ox = t->r.ox, oy = t->r.oy, oz = t->r.oz;
  ScePspFMatrix4 m = {{c * k, 0, -s * k, 0}, {0, k, 0, 0}, {s * k, 0, c * k, 0}, {c * ox + s * oz, ty + oy, -s * ox + c * oz, 1}};
  bindTexLean(t->r.tex); setFlags(t->r.flags);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB | 0x100);   // удвоение цвета: свет до 2×
  sceGuSetMatrix(GU_MODEL, &m);
  sceGuDrawArray(GU_TRIANGLES, VT16, cnt, 0, o);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  drawnTris += cnt / 3; draws++;
  return 1;
}
static void g944ListOut(const char *n, int nb, int nm) { char l[120]; snprintf(l, sizeof l, "DRAW944 name=%s nbill=%d nmesh=%d\n", n, nb, nm); smkWrite(l); }
static int g944DrawIdx = -1; static const char *const g944DrawNames[] = {"sim_door", "kwk_door", "sch_door", "l1_doors", "l1_doorstars", "simp_mission_doorstar", "kwik_mission_doorstar", "school_mission_doorstar", "l1m7door", "moe_door", "l2_doors", "l2_doorstars", "moes_mission_doorstar", "ad_door", "obs_door", "l3_doors", "l3_doorstars", "ad_mission_doorstar", "obsv_mission_doorstar", "dmv_door", "dmv_mission_doorstar", "l4_doors", "l4_doorstars"};   // №944 служебно
static void movieNote1206(const char *l) { bbNote(l); if (smkOn) smkWrite(l); }   // №1206: повтор открытия ролика (movie.c movieOpen) — в ящик и журнал
// ag/loadorder: наблюдатель падения (только журнал, smkOn). Герой или машина без земли под собой больше LOFALL_T с — строка FALL1209 один раз за эпизод:
// где, в каком мире (город / комната), какой переход ждёт (msNewsPend, выезд карточки, ирис). Отличает «земли нет в этом мире» (комната и город перепутаны) от обычного падения с обрыва
#define LOFALL_T 0.35f
#define LOVOID_D 70.f    // видимая незагруженная клетка ближе (м) — «дыра рядом» (VOID1209)
// №1200 (Роб 05.11: «сбоку на дом на скорости упал и в текстуре застрял… предусмотреть, чтобы в таких случаях был ресет машины на дорогу»):
// машина игрока с газом или тормозом CARSTUCK1200_S секунд не сдвинулась дальше 1,5 м и не стоит на колёсах (меньше 3 колёс на земле: вклинилась в дом,
// висит на крыше, лежит на боку) — тот же сброс на ближайшую дорогу, что RESET CAR. CARSTUCK1200_OLD=1 / flag carstuck1200old 1 — как было
#ifndef CARSTUCK1200_OLD
#define CARSTUCK1200_OLD 0
#endif
#ifndef CARSTUCK1200_S
#define CARSTUCK1200_S 4.f
#endif
static int stuckReq1200, carStuck1200Old;
static void loFallWatch(float dt) {
  static float tNo[2]; static int rep[2];
  { static int vN, vRep; static float vMin = 1e9f, vT0;   // VOID1209: дыра рядом (видимая незагруженная клетка ближе LOVOID_D) дольше 3 кадров подряд — одна строка за эпизод: когда началась, как долго, насколько близко
    float mnd_ = stMissNearD; stMissNearD = 1e9f;   // значение действует один кадр: кадр без streamMiss (комната, пауза) эпизод завершает
    if (!inRoom() && mnd_ < LOVOID_D) { if (!vN) vT0 = smkT; vN++; if (mnd_ < vMin) vMin = mnd_; }
    else { if (vRep) { char s[160]; snprintf(s, sizeof s, "VOID1209 end frames=%d dur=%.2f t=%.2f\n", vN, smkT - vT0, smkT); smkWrite(s); } vN = 0; vRep = 0; vMin = 1e9f; }
    if (vN >= 3 && !vRep) { vRep = 1; char s[200]; snprintf(s, sizeof s, "VOID1209 start near=%.0f room=%d iris=%d ldon=%d t0=%.2f t=%.2f\n", vMin, rs.room, transitBusy(), ldOn, vT0, smkT); smkWrite(s); } }
  for (int who = 0; who < 2; who++) {   // 0 герой пешком, 1 машина
    int on = who == 0 ? (haveHomer && onFoot()) : (haveCol && !onFoot());
    if (!on || !haveCol) { tNo[who] = 0.f; rep[who] = 0; continue; }
    float x = who ? car.x : homer.x, y = who ? car.y : homer.y, z = who ? car.z : homer.z, g, n[3];
    const ColWorld *cw = (who == 0 && G.pHomerWorld) ? G.pHomerWorld : &world;
    int has = colGround(cw, x, z, y + 2.f, y - 100.f, &g, n), air = who ? !car.grounded : !homer.grounded;
    if (!has && air) tNo[who] += dt; else if (has) tNo[who] = 0.f;
    if (tNo[who] > LOFALL_T && !rep[who]) { rep[who] = 1;
      char s[220]; snprintf(s, sizeof s, "FALL1209 who=%s pos=%.2f,%.2f,%.2f noground_s=%.2f world=%s room=%d mem=%d pend=%d brief_exit=%d iris=%d t=%.2f\n", who ? "car" : "hero", x, y, z, tNo[who],
        who == 0 && G.pHomerWorld ? "room" : "city", rs.room, rg.mem != 0, msNewsPend, uiBriefExitBusy(), transitBusy(), smkT); smkWrite(s); }
  }
}

#ifndef Z0INIT_OLD
#define Z0INIT_OLD 0   // Build 95: 1 - постоянные блоки (кольца звука, модель игрока, спрайты, отложенный блок) лениво, как было
#endif
extern void audioPrealloc(void); extern void skinKeepPrealloc(void); extern void uiZonePrealloc(void);
static void z0Init(void) {   // Z0-init: всё постоянное выделяется ОДИН раз при старте, пока куча чиста, - внизу, до участка уровня и до того, как середина кучи занята
  if (Z0INIT_OLD) return;
  uiZonePrealloc(); audioPrealloc(); skinKeepPrealloc(); wsPreload(); coinSprPreload(); memLadResPut();
}
extern void smallPoolInit(void);   // memwatch.c: пул мелких блоков (Build 95 шаг 1) - первым вызовом, самым низом кучи
int main(int argc, char **argv) {
  smokeBallastEarly(argc > 0 ? argv[0] : 0);   // служебно (только если рядом лежит файл ballast): балласт у края кучи, до пула и Z0
  smallPoolInit();
  z0Init();
  bmBegin(argc > 0 ? argv[0] : 0, SHR_KEEP_SMOKE || !SHR_REL);   // №1192: шаги старта на карту (bootmark.txt), самый первый вызов
  feAllocHook = feMemAlloc841; feFreeHook = feMemFree841;   // №841: гостиная меню без уровня — в участок уровня
  fxGlowGlint = glowGlintQ_;   // №839: искры, подбор, огни машин (fx.c, collectfx.c, carvis.c) — в очередь бликов кадра
  msnartNote944 = g944Note; msnartGlowGlint = glowGlintHook;   // №839: размытые свечения значков — векторным бликом (spritelight.h)
  msnartVecBeam = vecBeamHook;   // №805: луч тарелки L7 — векторно (lightsprite.h)
  msnartBeam995 = beam995Draw;   // №995: свет под тарелкой — конус от днища до земли (lightsprite.h)
  mvFlatGlow = vecGlowHook;   // №793: пятна <рисунок>_shadow под значками миссий — векторно (lightsprite.h)
  movieNote = movieNote1206;
  bmMark("msioInit>"); msioInit(); { char fn_[256]; SceIoStat st_; sibling(fn_, "farminold"); if (sceIoGetstat(fn_, &st_) >= 0) farMinOn = 0; sibling(fn_, "farheadold"); if (sceIoGetstat(fn_, &st_) >= 0) farHeadOn = 0; sibling(fn_, "farcellold"); if (sceIoGetstat(fn_, &st_) >= 0) farOn = 0; sibling(fn_, "farfake"); if (sceIoGetstat(fn_, &st_) >= 0) farFake = 1; sibling(fn_, "skyclear1249old"); if (sceIoGetstat(fn_, &st_) >= 0) skyClear1249Off = 1; sibling(fn_, "citycap100old"); if (sceIoGetstat(fn_, &st_) >= 0) cityCapOff = 1; sibling(fn_, "ddreg1244old"); if (sceIoGetstat(fn_, &st_) >= 0) ddReg1244Off = 1; sibling(fn_, "cellthrashold"); if (sceIoGetstat(fn_, &st_) >= 0) cellThrashOff = 1; sibling(fn_, "cellthrashrestold"); if (sceIoGetstat(fn_, &st_) >= 0) cellThrashRestOff = 1; sibling(fn_, "streamtight99l4"); if (sceIoGetstat(fn_, &st_) >= 0) streamTight99L4 = 1; sibling(fn_, "streamtight99old"); if (sceIoGetstat(fn_, &st_) >= 0) streamTight99Off = 1; sibling(fn_, "cellthrashceilold"); if (sceIoGetstat(fn_, &st_) >= 0) cellThrashCeilOff = 1; } bmMark("msioInit<");   // правки 252/259: обращения к карте по одному, сброс кэша ARK-4 (viewer/msio.h) — до первого чтения
  bmMark("bootFirst>"); bootFirst(argv[0]); bmMark("bootFirst<");   // правка Роба 28: первый кадр нашей заставки — самым первым делом (viewer/bootscreen.h)
// Правка Роба 263 (04.10: «зачем-то убрал звук запуска, будто кулер начинает крутиться; поставил музыку игры — не надо вообще,
// музыка игры будет в игре, слишком рано и жёстко»): под заставкой GONE Makers только звук включения (кнопка, кулер, бип —
// bootbeep.h), темы уровня нет; звук уровня поднимается после заставки, как до правки 205. BOOT_MUSIC_205=1 — тема под заставкой
#ifndef BOOT_MUSIC_205
#define BOOT_MUSIC_205 0
#endif
#if BOOT_MUSIC_205
  // правка Роба 205 (03.10: «загрузочный экран в конце очень затухает; поставить основную тему первого уровня»): звук уровня 1
  // поднимается до заставки GONE Makers, и под ней играет тема езды уровня 1 (sunday_drive_intro -> sunday_drive, как свободная
  // езда) — через лицензию до роликов интро (introRun глушит музыку). Писк включения (bootbeep.h) остаётся. Заменена правкой 263
  { char ad[256]; snprintf(ad, sizeof ad, "%s", argv[0]); char *sl = strrchr(ad, '/'); if (sl) strcpy(sl + 1, "audio/"); else strcpy(ad, "audio/");
    if (!smkOn) sndSeed(sceKernelGetSystemTimeLow());   // №685: в игре семя случайного звука от времени старта
    if (sndInit(ad) == 0) { sndLevel = 1; sndMode(SND_MODE_NORMAL); sndMaster(1.f); sndMusic(SND_MUS_FREEROAM); } }
#endif
  G.hk.coinLose = coinLoseSnd;   // сверка g10-flow круг 2: звук потери монет
  svBoltWorld = waspBoltWorld;   // сверка g11b круг 7 (P04 W5): снаряд осы бьёт подвижные предметы, стейтпропы, пешеходов, трафик (WASPBOLTWORLD_OLD=1 в collect.c — нет)
  G.hk.wrench = onWrench;   // пункт 4: гаечные ключи (крючок после u — позиционная инициализация G выше не сдвигается)
  int th = sceKernelCreateThread("cb", cbThread, 0x11, 0x4000, 0, 0);   // правка 103: выход по HOME пишет чёрный ящик (snprintf, запись) — 4000 Б было впритык
  bmMark("cb-thread>"); if (th >= 0) sceKernelStartThread(th, 0, 0); bmMark("cb-thread<");
  scePowerSetClockFrequency(333, 333, 166); bmMark("clock333");
  gameMsnRecordHook = svRecordHook1114;   // №1114: лучшие времена и попытки миссий пишутся в таблицу записи (savegame.c)
  bmMark("bootBegin>"); bootBegin(argv[0], "bootmarks.bin"); bmMark("bootBegin<");   // загрузочный экран GONE Makers, звук включения и знаки причастных (viewer/bootscreen.h, правки 125–128)
  skinSetToon(0);
  bmMark("blackBoxBoot>"); blackBoxBoot(); bmMark("blackBoxBoot<");       // правка 103: прошлый чёрный ящик без EXIT -> blackbox_prev.txt
  bmMark("kexcBoot>"); kexcBoot(); bmMark("kexcBoot<");           // аудит 01.10 22:35: ловушка вылета (sharexc.prx) — до первой записи ящика, kexc в шапке

  strncpy(path, argv[0], sizeof(path) - 16); path[sizeof(path) - 16] = 0;
  bmMark("bbWriterStart>"); bbWriterStart(); bmMark("bbWriterStart<");      // правка 103 (01.10): поток записи самописца — ящик и журнал звуков на карту фоном
  msnartSprite = iconSprHook;   // добавка Роба 18.10: «!» и трубка над будкой — спрайтом (spritelight.h)
  skinGeRoom = geRoomHard; msnartGeRoom = geRoomHard;   // правка 103 (01.10): персонажи и рисунки миссий — в бюджете списка GE
  char *slash = strrchr(path, '/'); if (slash) slash[1] = 0; else path[0] = 0;
  saveBlocked = smokeWantsFlag(path, "nosave");   // «flag nosave» среди первых flag сценария: тур ничего не пишет в сохранения и настройки
  playerFirst = smokeWantsFlag(path, "player");   // «flag player» среди первых flag сценария: режим игрока с первого кадра (автотур ИГРОК)
  strcat(path, "level.shr");
  { int lv = smokeWantsLevel(path); if (lv) gameLevel = lv; }   // служебное: «flag level N» сценария — уровень N из папки l<N>/
  levelFile(levelShr, "level.shr");   // уровень 1 — тот же путь
  // Роб 28.09 «как в игре»: после заставки GONE Makers — порядок bootup.p3d оригинала (ниже, после включения GE)
  bootShots = smokeEarly(path);   // служебное: «flag bootshots» — снимки этих экранов (tools/screens_gate.sh)
  { char fn[256]; sibling(fn, "boot.shr"); scrTvOver = !uiScrOpen(fn); }
  // шаг 2 аудита: кадр 5650 + дизеринг (вдвое меньше записи/очистки, +0.55 МБ VRAM свободно после буферов);
  // служебный файл fb32 рядом с EBOOT -> 8888, как раньше (сравнение картинок). PPSSPP дизеринг не рисует.
  { char fn[256]; SceIoStat st_; sibling(fn, "fogskyold"); if (sceIoGetstat(fn, &st_) >= 0) fogSkyOn = 0; }   // ДАЛЬ: файл-флаг рядом с EBOOT — туман и подстилка как в магистрали до fogsky
  { char fn[256]; SceIoStat st_; sibling(fn, "bubble992old"); if (sceIoGetstat(fn, &st_) >= 0) uiBubble992Set(1); }   // №992: файл-флаг рядом с EBOOT — появление пузыря как в TEST70 (8 шагов, текст после рамки)
  { char fn[256]; SceIoStat st_; sibling(fn, "boothbase1052old"); if (sceIoGetstat(fn, &st_) >= 0) boothBase1052Old = 1; }   // №1052: файл-флаг рядом с EBOOT — основание будки как в TEST73
  { char fn[256]; SceIoStat st_; sibling(fn, "carmtxold"); if (sceIoGetstat(fn, &st_) >= 0) carMtxOld = 1; }
  { char fn[256]; SceIoStat st_; sibling(fn, "caroutlinea"); if (sceIoGetstat(fn, &st_) >= 0) carOutline = 0; sibling(fn, "caroutlineb"); if (sceIoGetstat(fn, &st_) >= 0) carOutline = 1; sibling(fn, "caroutlinec"); if (sceIoGetstat(fn, &st_) >= 0) carOutline = 2; }   // варианты контура машины: файл рядом с EBOOT (b — реже оболочки у мелких кусков, c — кэш толщины на 4 кадра)
  { char fn[256]; SceIoStat st_; sibling(fn, "lvcheckold"); if (sceIoGetstat(fn, &st_) >= 0) lvCheckOld = 1; }   // №1195: файл-флаг рядом с EBOOT — таблицы level.shr без проверки, как в TEST89
  { char fn[256]; SceIoStat st_; sibling(fn, "locklimold"); if (sceIoGetstat(fn, &st_) >= 0) relLockOld = 1; }   // №1195: файл-флаг рядом с EBOOT — замки stripScratch и marks без предела, как в TEST89
  { char fn[256]; SceIoStat st_; sibling(fn, "relstackon"); if (sceIoGetstat(fn, &st_) >= 0) relStackOn = 1; }   // закраска и зонд стека потоков — только по файлу (по умолчанию выключено до проверки на железе; потоки, начатые раньше чтения файла, не закрашены)
  { char fn[256]; SceIoStat st_; sibling(fn, "relstackold"); if (sceIoGetstat(fn, &st_) >= 0) relStackOld = 1; }   // №1195: файл-флаг — зонд стека выключен (потоки, начатые до чтения файла, уже закрашены)   // ТЕСТ90: файл-флаг рядом с EBOOT — матрица каждого куска машины считается заново, как в TEST89
  { char fn[256]; SceIoStat st_; sibling(fn, "glowoff1052old"); if (sceIoGetstat(fn, &st_) >= 0) glowOff1052Old = 1; }   // №1052: файл-флаг рядом с EBOOT — крупные аддитивные куски с текстурой как в TEST73
  { char fn[256]; SceIoStat st_; sibling(fn, "hzoff1042old"); if (sceIoGetstat(fn, &st_) >= 0) hzOff1042Old = 1; }   // №1042: файл-флаг рядом с EBOOT — лента горизонта как в TEST72
  { char fn[256]; SceIoStat st_; sibling(fn, "pole1046old"); if (sceIoGetstat(fn, &st_) >= 0) pole1046Old = 1; }   // №1046: файл-флаг рядом с EBOOT — столб валится вяло, как в TEST73
  { char fn[256]; SceIoStat st_; sibling(fn, "boothlr1057old"); if (sceIoGetstat(fn, &st_) >= 0) boothLR1057Old = 1; }   // №1057: файл-флаг рядом с EBOOT — в будке L R не повторяются (как в TEST73)
  { char fn[256]; SceIoStat st_; sibling(fn, "mselr1058old"); if (sceIoGetstat(fn, &st_) >= 0) mvMsLR1058Set(1); }   // №1058: файл-флаг рядом с EBOOT — в Mission Select уровень только ◄ ►
  { char fn[256]; SceIoStat st_; sibling(fn, "jzgate705old"); if (sceIoGetstat(fn, &st_) >= 0) carJumpZoneGateOld = 1; }   // №705 ч.2: файл-флаг рядом с EBOOT — разгон тонких зон прыжка как в TEST73
  { char fn[256]; SceIoStat st_; sibling(fn, "police723old"); if (sceIoGetstat(fn, &st_) >= 0) mcPolice723Old = 1; }   // №723: файл-флаг рядом с EBOOT — полиция подъезжает к стоящему игроку вплотную, как до правки
  { char fn[256]; SceIoStat st_; sibling(fn, "hnrpause723old"); if (sceIoGetstat(fn, &st_) >= 0) gameHnrChasePauseOld = 1; }   // №723: файл-флаг рядом с EBOOT — в погоне пауза спада шкалы 3 с, как до правки
  { char fn[256]; SceIoStat st_; sibling(fn, "boothlr1038old"); if (sceIoGetstat(fn, &st_) >= 0) boothLR1038Old = 1; }   // №1038: файл-флаг рядом с EBOOT — в будке только ◄ ►
  { char fn[256]; SceIoStat st_; sibling(fn, "radar1035old"); if (sceIoGetstat(fn, &st_) >= 0) uiRadar1035Set(1); }   // №1035: файл-флаг рядом с EBOOT — радар в комнате как в TEST72
  { char fn[256]; SceIoStat st_; sibling(fn, "menuvec1012old"); if (sceIoGetstat(fn, &st_) >= 0) uiMenuVec1012Set(1); }   // №1012: файл-флаг рядом с EBOOT — выделение пункта как в TEST71
  { char fn[256]; SceIoStat st_; sibling(fn, "textlate1004old"); if (sceIoGetstat(fn, &st_) >= 0) uiTextLate1004Set(1); }   // №1004: файл-флаг — текст пузыря как в TEST71
  { char fn[256]; SceIoStat st_; sibling(fn, "roadink1008old"); if (sceIoGetstat(fn, &st_) >= 0) roadInk1008Old = 1; }   // №1008: файл-флаг — край асфальта пером бордюра, как в TEST71
  { char fn[256]; SceIoStat st_; sibling(fn, "roadthick1023old"); if (sceIoGetstat(fn, &st_) >= 0) roadThick1023Old = 1; }   // №1023: файл-флаг рядом с EBOOT — край асфальта 1 px и 28 м, как в TEST72
  { char fn[256]; SceIoStat st_; sibling(fn, "roadink1008old"); if (sceIoGetstat(fn, &st_) >= 0) roadInk1008Old = 1; }
  { char fn[256]; SceIoStat st_; sibling(fn, "glint1031old"); if (sceIoGetstat(fn, &st_) >= 0) glint1031 = 0; }   // №1031: файл-флаг — звезда ключа с проверкой глубины, как после №1022
  { char fn[256]; SceIoStat st_; sibling(fn, "streamahead1042old"); if (sceIoGetstat(fn, &st_) >= 0) streamAhead1042 = 0; }   // №1042: файл-флаг — очередь загрузки как было
  { char fn[256]; SceIoStat st_; sibling(fn, "glintfix1022old"); if (sceIoGetstat(fn, &st_) >= 0) glintFix1022 = 0; }   // №1022: файл-флаг — свечения как в TEST71   // №1008: файл-флаг — край асфальта пером бордюра, как в TEST71
  { char fn[256]; SceIoStat st_; sibling(fn, "carshadow1003old"); if (sceIoGetstat(fn, &st_) >= 0) { carShadow1003Old = 1; carShadow1003On = 1; } }   // №1003: файл-флаг — тени машин всегда, как в TEST71
  { char fn[256]; SceIoStat st_; sibling(fn, "pole998old"); if (sceIoGetstat(fn, &st_) >= 0) spPole998Old = 1; }   // №998: файл-флаг рядом с EBOOT — столбы cls 10 при касании вскользь не падают (как в TEST71)
  { char fn[256]; SceIoStat st_; sibling(fn, "card994old"); if (sceIoGetstat(fn, &st_) >= 0) uiCard994Set(1); }   // №994: файл-флаг рядом с EBOOT — карточка миссии как в TEST70 (стоит на месте до второго запуска въезда)
  { char fn[256]; SceIoStat st_; sibling(fn, "hzcol997old"); if (sceIoGetstat(fn, &st_) >= 0) hzCol997Old = 1; }   // №997: файл-флаг рядом с EBOOT — лента горизонта прежнего тона (без цвета фасадов)
  { char fn[256]; SceIoStat st_; sibling(fn, "repeat706old"); if (sceIoGetstat(fn, &st_) >= 0) uiPadRepeatOld(1); }   // №706: файл-флаг рядом с EBOOT — автоповтор стрелок как до №706 (только ↑/↓)
  { char fn[256]; SceIoStat st_; sibling(fn, "carfill930old"); if (sceIoGetstat(fn, &st_) >= 0) carFillOn = 0; }
  { char fn[256]; SceIoStat st_; sibling(fn, "flat912old"); if (sceIoGetstat(fn, &st_) >= 0) flatOld = 1;
    sibling(fn, "flatrescanold"); if (sceIoGetstat(fn, &st_) >= 0) flatRescanOld = 1;   /* №912 п.А: прежний опрос всех текстур tex[] (трафик → фоновое задание тона на каждом уровне) */
    sibling(fn, "flatdump"); if (sceIoGetstat(fn, &st_) >= 0) flatDump = 1;
    sibling(fn, "flatrebuildold"); if (sceIoGetstat(fn, &st_) >= 0) flatRebuildOld = 1; }   /* А без исправления: пересборки заливки после смены уровня нет */   /* дамп FDIAG/FLATX/FLATT — только по файлу или «flag flatdump 1» */   // №912: файл-флаг рядом с EBOOT — мир с текстурами, как до заливки
  { char fn[256]; SceIoStat st_; sibling(fn, "neonarch957old"); if (sceIoGetstat(fn, &st_) >= 0) neonArch957 = 0; }   // №957б
  { char fn[256]; SceIoStat st_; sibling(fn, "pool990old"); if (sceIoGetstat(fn, &st_) >= 0) pool990 = 0; }   // №990: эллипсы под фонарями как в TEST70
  { char fn[256]; SceIoStat st_; sibling(fn, "lvamem962old"); if (sceIoGetstat(fn, &st_) >= 0) lvaMem962Old = 1; }   // №962: куски уровня в запас участка lva (main_stream.h)
  { char fn[256]; SceIoStat st_; sibling(fn, "pvschunkold"); if (sceIoGetstat(fn, &st_) >= 0) pvsChunkOld = 1; }   // №1068: PVS1 кусками (main_levels.h pvsLoadChunks)
  { char fn[256]; SceIoStat st_; sibling(fn, "sndslotold"); if (sceIoGetstat(fn, &st_) >= 0) audioSlotOld(1); }   // №1069: банк звука каждый раз из кучи, как в TEST79 (audio.c)
  { char fn[256]; SceIoStat st_; sibling(fn, "beam995old"); if (sceIoGetstat(fn, &st_) >= 0) beam995 = 0; }   // №995: билборды луча тарелки как в TEST70
  { char fn[256]; SceIoStat st_; sibling(fn, "boss967old"); if (sceIoGetstat(fn, &st_) >= 0) gameBoss967Old = 1; }   // №967: тарелка L7 и луч, цель destroyboss (game_msn.c)
  { char fn[256]; SceIoStat st_; sibling(fn, "lvatail962bold"); if (sceIoGetstat(fn, &st_) >= 0) lvaTail962bOld = 1; }   // №962б: резерв под COL1/PRP2 по готовности чтения, как было
  { char fn[256]; SceIoStat st_; sibling(fn, "sky954old"); if (sceIoGetstat(fn, &st_) >= 0) sky954Old = 1; sibling(fn, "sky954starsold"); if (sceIoGetstat(fn, &st_) >= 0) sky954StarsOld = 1; }   // №954: файл-флаг рядом с EBOOT — небо как в TEST59 / без звёзд-точек
  { char fn[256]; SceIoStat st_; sibling(fn, "mecityoff"); if (sceIoGetstat(fn, &st_) >= 0) ccFileOff = 1; }   // №653: файл-флаг рядом с EBOOT — отсечение города целиком на главном
  { char fn[256], b[16]; sibling(fn, "jitter"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);   // эксперимент (viewer/jitter.h): временное сглаживание полпикселя
    if (f >= 0) { int n = sceIoRead(f, b, 15); sceIoClose(f); jitterOn = 1; if (n > 0) { b[n] = 0; char *e_; int v = (int)strtol(b, &e_, 10); if (v >= 5 && v <= 90) jitterPct = v; int c_ = (int)strtol(e_, 0, 10); if (c_ >= 40 && c_ <= 100) jitterCover = c_; } }   // файл: «доля прошлого кадра % [охват высоты %]», например «40 80»
    sibling(fn, "nojitter"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); jitterOn = 0; } }
  { char fn[256]; sibling(fn, "toon"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); toonDevFile = 1; skinSetToon(1); } }   // правка 428: мультяшный режим файлом toon рядом с EBOOT (служебно; поверх настройки игрока, в выпуск не кладётся — release.h)
  { char fn[256]; sibling(fn, "fb32"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); fb16 = 0; }
    sibling(fn, "clearchk"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); clearChk = 1; }
    sibling(fn, "seamchk"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); seamChk = clearChk; }
    sibling(fn, "seamsold"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); seamsOld = 1; }
    sibling(fn, "near07"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); nearZ = .7f; }         // Ф2: сравнение
    { sibling(fn, "bisect"); int fb_ = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fb_ >= 0) { char bb_[16] = {0}; sceIoRead(fb_, bb_, 15); sceIoClose(fb_); bisOld = (unsigned)strtoul(bb_, 0, 16); } }   // bisect.h: маска прежних путей без пересборки
    { sibling(fn, "groundbias"); int fg_ = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fg_ >= 0) { char gb_[16] = {0}; sceIoRead(fg_, gb_, 15); sceIoClose(fg_); groundMul = groundBiasParse(gb_); } }   // множитель наклона мипа земли (main_render.h texSlope); нет файла: 1
    { sibling(fn, "rollps2"); int fr_ = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fr_ >= 0) { char rb_[16] = {0}; sceIoRead(fr_, rb_, 15); sceIoClose(fr_); carRollGain = carRollGainParse(rb_); } }   // №388: крен ближе к PS2 (car.c, добавка к плечу бокового крена, м); пусто — 0,22; нет файла — как было
    sibling(fn, "nolayer"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); layerBias = 0; }
    sibling(fn, "toonworld"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); levelToonWanted = 1; } }   // №679: служебный файл — уровень-мультик с первой загрузки (смоук)
  int fbBpp = fb16 ? 2 : 4;
  void *fbp0 = (void *)0, *fbp1 = (void *)(BUF_W * SCR_H * fbBpp), *zbp = (void *)(BUF_W * SCR_H * fbBpp * 2);  fbpG1226 = &fbp0;   // Будка-93 (1226): roomOpen закрывает ирис сам
#ifndef BOOT_CLEAR_OLD
#define BOOT_CLEAR_OLD 0
#endif
  // №740 (Роб 17.10: «между дисклеймером и экраном-телевизором на мгновение глюк»): заставка и NOTICE рисуются отладочным экраном (32 бита, VRAM с нуля),
  // а sceGuDispBuffer сразу показывает буфер fbp1 в 16 битах на той же памяти — кадр мусора из картинки NOTICE в чужом формате, пока не придёт первый кадр
  // лицензии. Оба буфера кадра заливаются чёрным до переключения: между NOTICE и телевизором — чёрный кадр. BOOT_CLEAR_OLD=1 — как было
  if (!BOOT_CLEAR_OLD) memset((void *)(0x44000000u + (unsigned)fbp0), 0, (size_t)BUF_W * SCR_H * fbBpp * 2);
  sceGuInit(); sceGuStart(GU_DIRECT, list);
  sceGuDrawBuffer(fb16 ? GU_PSM_5650 : GU_PSM_8888, fbp0, BUF_W); sceGuDispBuffer(SCR_W, SCR_H, fbp1, BUF_W);
  if (fb16) { static const ScePspIMatrix4 dith = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
              sceGuSetDither(&dith); sceGuEnable(GU_DITHER); }
  sceGuFrontFace(GU_CCW);   // лицевой обход CCW (gebench, tools/wind.c); отсечение включает setFlags по флагам куска
  sceGuDepthBuffer(zbp, BUF_W); guZbpG = zbp; { char fn_[256]; SceIoStat st_; sibling(fn_, "gurestateold"); if (sceIoGetstat(fn_, &st_) >= 0) guRestateOff = 1; else guPwrInit(); }
  sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2); sceGuViewport(2048, 2048, SCR_W, SCR_H);
  sceGuDepthRange(65535, 0); sceGuScissor(0, 0, SCR_W, SCR_H); sceGuEnable(GU_SCISSOR_TEST);
  sceGuDepthFunc(GU_GEQUAL); sceGuEnable(GU_DEPTH_TEST); sceGuDisable(GU_CULL_FACE);
  sceGuShadeModel(GU_SMOOTH); sceGuEnable(GU_CLIP_PLANES);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuAlphaFunc(GU_GREATER, 0x40, 0xff);
  sceGuFinish(); sceGuSync(0, 0);
  sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
  // Роб 28.09 «как в игре»: после заставки GONE Makers — порядок bootup.p3d оригинала: лицензия (License.scr), пока
  // читается уровень, потом BootupLoad.scr (текст лицензии закрыт, LOADING), пока грузится остальное; дальше ролики,
  // титульный экран и меню. Самодельные строки загрузки и шкала заставки не показываются (bootscreen.h)
  scrT0 = sceKernelGetSystemTimeLow(); scrFrame(UI_BOOT_LICENSE, 0, 1, &fbp0); scrFrame(UI_BOOT_LICENSE, 0, 1, &fbp0);
  unsigned licT0 = scrT0;
  if (bootShots) scrShot("license", "boot", UI_BOOT_LICENSE, "-");

  // правка Роба 3 (28.09): как в оригинале — сначала только интерфейс и звук (лицензия, BootupLoad), ролики, титульный
  // экран и главное меню; уровень читается после NEW GAME под экраном загрузки Loading.scr. Музыка уровня — только в игре
  { char zf_[256]; SceIoStat zs_; int zu_ = -1, zn_ = -1; sibling(zf_, "zoneuiold"); if (sceIoGetstat(zf_, &zs_) >= 0) zu_ = 0; sibling(zf_, "zonenewsold"); if (sceIoGetstat(zf_, &zs_) >= 0) zn_ = 0; sibling(zf_, "zonenewson"); if (zn_ < 0 && sceIoGetstat(zf_, &zs_) >= 0) zn_ = 1; if (zu_ >= 0 || zn_ >= 0) uiZoneSet(zu_, zn_); }   // TEST91 1А шаг 1: файлы zoneuiold (пакет ui.shr из кучи), zonenewson (слот газеты включить; по умолчанию выключен), zonenewsold — до первого пакета
  { bmMark("uiOpen>"); uiOpen(); bmMark("uiOpen<");   // этап 7: ui.shr уровня 1 (радар, фон паузы, меню); текстуры HUD — в VRAM сразу за буфером глубины
    bmMark("svmInit>"); svmInit(path, uiReady && (!svcStartP() || smokeWantsFlag(path, "saves")));   // служебные запуски — только с «flag saves»
    if (smokeWantsFlag(path, "devcollold")) devCollOld = 1;   // правка 350: красный контроль ставится до загрузки DEV (меню)
    if (smokeWantsFlag(path, "bonusopen")) bonusSvcAll = 1;   // правка 421: служебно — все трассы бонус-игры открыты (гейты гонки tests/bonus/emu.sh)
    bmMark("svmInit<"); uiMenusSetup(); bmMark("uiMenusSetup<");   // 12а: пункты главного меню и паузы
    { char fn_[256]; SceIoStat st_; sibling(fn_, "defaults1066old"); if (sceIoGetstat(fn_, &st_) >= 0) { cfg.colour = DEFAULTS794B_OLD ? TONE_SAT_DEF : 156; cfg.contrast = DEFAULTS794B_OLD ? 100 : 150; cfg.eyeLines = 1; cfg.toonTex = DEFAULTS794_OLD ? 35 : 45; cfg.oldTv = 5; cfg.texturesOn = 0; } }   // №1066: файл-флаг — прежние значения по умолчанию (до cfgLoad: cfgDef и чистый файл)
    bmMark("cfgLoad>"); cfgLoad(); bmMark("cfgLoad<"); if (toonDevFile) { cfg.toon = 1; cfgApply(); } }
  bmMark("sndInit>");
  if (!sndReady() || sndLevel != gameLevel || sndLangPending()) { char ad[256]; if (sndReady() && sndLangPending()) sndShutdown(); levelFile(ad, "audio/"); sndInit(ad); sndLevel = gameLevel; }   // №1111 (g08): сменился язык озвучки — пакет читается заново   // нет пакета — тишина, игра идёт; правка 205: уровень 1 уже поднят под заставкой
  bmMark("sndInit<");
  bmMark("transitLoad>"); { char fn[256]; sibling(fn, "iris.shr"); transitLoad(fn);   // нет пакета — переходы мгновенные
    transitSetup(fb16 ? GU_PSM_5650 : GU_PSM_8888, BUF_W * SCR_H * fbBpp, smkWrite, &smkT); transitHoldHook = spawnHoldHook; transitHoldNote = bbNote; transitStuckExempt = irisStuckExempt1258; transitStuckInfo = irisStuckInfo1258; }
  bmMark("transitLoad<"); bmMark("bootLicense>"); bootLicense(licT0, &fbp0); bmMark("bootLicense<");   // лицензия не короче LICENSE_MS, дальше BootupLoad
  bmMark("smokeInit>"); smokeInit(path); bmMark("smokeInit<");   // служебное: smokelog / scenario.txt (viewer/smoke.h)
  acctInit();        // учёт шести счетов (acct.h): сценарий или файл acct
  if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PLAYERFIRST %d svc_at_start=%d\n", playerFirst, svcStart()); smkWrite(s_); }
  if (kexcTimedOut) bbNote("ME skip: kexc timeout (модульный замок мог остаться занятым)\n");
  bmMark("meAudioBoot>"); if (!kexcTimedOut) { int meAudioBoot(const char *, int, char *, int); char md[256], mr[160]; sibling(md, ""); meAudioBoot(md, smokeIsEmu(), mr, sizeof mr);   // правка 276:
    if (mr[0]) svLog(mr); }   // флаг-файл meaudio — смешивание звука на Media Engine (пилот, по умолчанию выкл; viewer/me/meprobe.c, viewer/audio.c)
  bmMark("meAudioBoot<"); bmMark("meFxBoot>"); if (!kexcTimedOut) { int meFxBoot(const char *, int, char *, int); char md[256], mr[200]; sibling(md, ""); meFxBoot(md, smokeIsEmu(), mr, sizeof mr);   // правка 313:
    if (mr[0]) svLog(mr); }   // флаг-файл mefx — частицы и свечение на Media Engine (по умолчанию выкл; viewer/fxdsp.h, viewer/fx.c)
  bmMark("meFxBoot<"); { char fn[256]; SceIoStat st; sibling(fn, "mehandold"); if (sceIoGetstat(fn, &st) >= 0) audioMeHand(0); }
  if (CRASHNOW_OK) { char fn[256]; SceIoStat st; sibling(fn, "crashnow"); if (sceIoGetstat(fn, &st) >= 0) crashNowAt = sceKernelGetSystemTimeLow() + 12000000u + 1u; }   // №1208в: нарочный вылет через 12 с игры
  { char fn[256]; SceIoStat st; sibling(fn, "lvlogoson"); if (sceIoGetstat(fn, &st) >= 0) lvLogosOn = 1; }   // №1199: файл-выключатель «логотипы как у игрока TEST88»   // №1100: файл-выключатель (окно потоков 2048 Б, как было)
  { char fn[256]; SceIoStat st; sibling(fn, "mepeak"); if (sceIoGetstat(fn, &st) >= 0) { extern int mePeakGate; mePeakGate = 1; } }   // ME-доля, адаптивный помощник (audio.c)
  { char fn[256], nb[16] = {0}; sibling(fn, "mefxmax"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);   // ME-доля, вариант B: предел элементов задания частиц на ME
    if (f >= 0) { sceIoRead(f, nb, sizeof nb - 1); sceIoClose(f); fxSetMeMax(atoi(nb)); } }
  {char fn[256];SceIoStat st;sibling(fn,"gagskinstreamold");if(sceIoGetstat(fn,&st)>=0)gagSkinStreamOld=1;}
  {char fn[256];SceIoStat st;sibling(fn,"gagplan99old");if(sceIoGetstat(fn,&st)>=0)gagPlan99Off=1;}
  {char fn[256];SceIoStat st;sibling(fn,"collsplitold");if(sceIoGetstat(fn,&st)>=0)collSplitOld=1;}   // Пропечка-96: файл-выключатель COLLSPLIT (вершины гэгов в блоке пакета, как в Build 95)
  pcOld = smokeWantsFlag(path, "pcold") || PC_OLD_DEF;   // правка 352: красный контроль — на всех уровнях Гомер (до меню: LOAD ставит уровень)
  { char fn[256]; SceIoStat st; sibling(fn, "pcold"); if (sceIoGetstat(fn, &st) >= 0) pcOld = 1; }
  // TEST27 (план финала, Камера): сверка с оригиналом по умолчанию ВКЛ; откат — -D*_OLD=1 при сборке или пустой файл рядом с EBOOT (coneold, homerspotold, homerlitold)
  { char fn[256]; SceIoStat st; int old_;
    // №658/№599 (Камера, перепроверка 17.10): конусы гаража АЭС и лучи НЛО L7 по умолчанию рисует таблица №599 (то, что делал «flag lightspr 1»): прежний путь билбордов пропускает ВСЕ аддитивные
    // билборды при neonGlowOn=0, так что в TEST27/28 конусов гаража не было вовсе. Ореолы, знаки и неон остаются скрытыми (slLightOn), цена в гараже 6 квадов / 1 вызов. Откат: файл lightsprold рядом с EBOOT или -DLIGHTSPR_DEFAULT_OLD=1
#ifndef LIGHTSPR_DEFAULT_OLD
#define LIGHTSPR_DEFAULT_OLD 0
#endif
    old_ = LIGHTSPR_DEFAULT_OLD; sibling(fn, "lightsprold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;
    if (!old_) { slLightOn = 1; lspOld = 0; }
    sibling(fn, "shadowvecold"); if (sceIoGetstat(fn, &st) >= 0) slShVec = 0;   // файл-выключатель векторной тени
    sibling(fn, "shadow973old"); if (sceIoGetstat(fn, &st) >= 0) sh973 = 0;   // №973: тень машины плоскостью по центру, как было
    sibling(fn, "icon993old"); if (sceIoGetstat(fn, &st) >= 0) icon993Old = 1;   // №993: значок дома №623 вне задания, как было
    sibling(fn, "boothmem1051old"); if (sceIoGetstat(fn, &st) >= 0) boothMem1051 = 0;   // №1051: файл-флаг — будка как в TEST72
    sibling(fn, "deathring1239old"); if (sceIoGetstat(fn, &st) >= 0) deathRing1239Old = 1;   // Build 95 П2: файл-выключатель
    sibling(fn, "carhard96old"); if (sceIoGetstat(fn, &st) >= 0) carHard96On = 0;   // Build 96 (1250): файл-выключатель
    sibling(fn, "carvis96old"); if (sceIoGetstat(fn, &st) >= 0) carVis96 = 0;   // Build 96 (1250): файл-выключатель
    sibling(fn, "cleanreload96old"); if (sceIoGetstat(fn, &st) >= 0) cleanReload96 = 0;
    sibling(fn, "irismodal1259old"); if (sceIoGetstat(fn, &st) >= 0) irisModal1259Old = 1; sibling(fn, "irisstuck1258old"); if (sceIoGetstat(fn, &st) >= 0) irisStuck1258Old = 1; sibling(fn, "sceneboost1257old"); if (sceIoGetstat(fn, &st) >= 0) sceneBoost1257 = 0; sibling(fn, "gagholdcap1257old"); if (sceIoGetstat(fn, &st) >= 0) gagHoldCap1257Old = 1; sibling(fn, "gagpend1257old"); if (sceIoGetstat(fn, &st) >= 0) collGagPend1257Old = 1;   // Будка-93 (1257): файлы-выключатели   // Build 96: файл-выключатель чистой перезагрузки
    sibling(fn, "lvldr96old"); if (sceIoGetstat(fn, &st) >= 0) lvLdr96 = 0;   // Build 96: файл-выключатель LEVEL_NEXT/LEVEL_START на ldrRun
    sibling(fn, "tplanplace96old"); if (sceIoGetstat(fn, &st) >= 0) tplanPlace96 = 0;   // Build 96: файл-выключатель
    sibling(fn, "tfsm95old"); if (sceIoGetstat(fn, &st) >= 0) tfsm95Old = 1;   // Build 95 П2: файл-выключатель
    sibling(fn, "movie1239old"); if (sceIoGetstat(fn, &st) >= 0) movie1239Old = 1;   // Будка-93 (1239): файл-выключатель
    sibling(fn, "lvsavenext1234old"); if (sceIoGetstat(fn, &st) >= 0) lvSaveNext1234Old = 1;   // Будка-93 (1234): файл-выключатель
    sibling(fn, "shopheal1233old"); if (sceIoGetstat(fn, &st) >= 0) shopHeal1233Old = 1;   // Будка-93 (1233 T08/T09): файл-выключатель
    shopHeal1233Wire();
    sibling(fn, "loadcord1233old"); if (sceIoGetstat(fn, &st) >= 0) loadCord1233Old = 1;   // Будка-93 (1233 T01/T02): файл-выключатель
    sibling(fn, "t03old"); if (sceIoGetstat(fn, &st) >= 0) t03Old = 1;   // Будка-93 (1233 T03): файл-выключатель
    sibling(fn, "transitplanold"); if (sceIoGetstat(fn, &st) >= 0) transitPlanOld = 1;   // Будка-93 (1233): файл-выключатель
    sibling(fn, "booth1225old"); if (sceIoGetstat(fn, &st) >= 0) booth1225Old = 1;   // Будка-93 (1225): файл-выключатель
    sibling(fn, "dooriris1226old"); if (sceIoGetstat(fn, &st) >= 0) doorIris1226Old = 1;   // Будка-93 (1226): файл-выключатель
    sibling(fn, "gaghold1226old"); if (sceIoGetstat(fn, &st) >= 0) gagHold1226Old = 1;
    sibling(fn, "pvb1old"); if (sceIoGetstat(fn, &st) >= 0) pvb1Old = 1;   // П3: файл-выключатель — PVS1 списками, как до PVB1
    sibling(fn, "mem1060old"); if (sceIoGetstat(fn, &st) >= 0) mem1060 = 0;   // №1060: файл-флаг — будка как в №1051 (три ступени)
    sibling(fn, "memmapold"); if (sceIoGetstat(fn, &st) >= 0) { memmapOff = 1; memmapSetOn(0); }   // TEST90 MEMMAP: без учёта живых выделений и строк
    sibling(fn, "talkroomfloorold"); if (sceIoGetstat(fn, &st) >= 0) msnTalkRoomOld = 1;   // радиус разговора в комнате как в TEST89
    sibling(fn, "lvapvson"); if (sceIoGetstat(fn, &st) >= 0) lvaBytes = LVA_BYTES_BASE + (LVA_PVS_KB << 10);   // TEST90 1В: участок +LVA_PVS_KB (по умолчанию выключено, см. main_stream.h)
    sibling(fn, "lvapvsold"); if (sceIoGetstat(fn, &st) >= 0) lvaBytes = LVA_BYTES_BASE;   // TEST90 1В: участок прежнего размера, PVS кусками в куче (до первого lvaInit)
    sibling(fn, "collretryold"); if (sceIoGetstat(fn, &st) >= 0) collRetry1 = 0;   // ТЕСТ90 память: пакет гэгов без повтора, как в TEST89
    sibling(fn, "pinfreeold"); if (sceIoGetstat(fn, &st) >= 0) pinFree1 = 0;   // ТЕСТ90 память: мелкие кэши прежнего уровня освобождаются в конце загрузки, как в TEST89
    sibling(fn, "unloadfirstold"); if (sceIoGetstat(fn, &st) >= 0) unloadFirst1b = 0;   // TEST90 1В: газета и музыка загрузки до выгрузки уровня, как в TEST89
    sibling(fn, "feunloadfirstold"); if (sceIoGetstat(fn, &st) >= 0) feUnloadFirst1b = 0;   // TEST90 1В: гостиная меню при загруженном уровне, как в TEST89
    sibling(fn, "memload1065old"); if (sceIoGetstat(fn, &st) >= 0) memLoad1065 = 0;   // №1065: файл-флаг — порядок загрузки и открытие газеты как в TEST77
    sibling(fn, "mem1060rebuildold"); if (sceIoGetstat(fn, &st) >= 0) mem1060Rebuild = 0;   // №1060: без пересборки уровня
    sibling(fn, "memtrim1060old"); if (sceIoGetstat(fn, &st) >= 0) memTrim1060 = 0;   // №1060: выгрузка клеток при тесной куче разом, как было
    sibling(fn, "dcache96old"); if (sceIoGetstat(fn, &st) >= 0) dcache96 = 0;   // Build 96: без записи кэша на месте и очистки перед чтением с карты, как было
    sibling(fn, "dcacheall96"); if (sceIoGetstat(fn, &st) >= 0) dcAll96On = 1;   // Build 96 диагностика (только тур-сборка): sceKernelDcacheWritebackAll в начале кадра и перед закрытием списков
    sibling(fn, "memtrim2old"); if (sceIoGetstat(fn, &st) >= 0) memTrim2 = 0;   // №1060в: эпизод выгрузки по перемеру кучи каждый кадр, как MEMTRIM1060
    sibling(fn, "camclipfast1060old"); if (sceIoGetstat(fn, &st) >= 0) camClipFast1060 = 0;   // №1060: счётчик CAMCLIPSTAT полным обходом, как было
    sibling(fn, "mem1060feold"); if (sceIoGetstat(fn, &st) >= 0) mem1060Fe = 0;   // №1060б: переход газетой уровня или шнуром, как в №1060
    sibling(fn, "spawnholdold"); if (sceIoGetstat(fn, &st) >= 0) homerSceneGate431 = homerSpawnWait431 = 0;   // правка 431: постановка без ожидания опоры и сцены, как было
    sibling(fn, "mcrlad1238old"); if (sceIoGetstat(fn, &st) >= 0) mcrLad1238 = 0;   // №1238: машина миссии мимо лестницы, как было
    sibling(fn, "mcrwait94old"); if (sceIoGetstat(fn, &st) >= 0) mcrWait94On = 0;   // Build 94: машина миссии без метки «ждёт память», повтор каждый кадр, как было
    msnartWatch = msnartWatchLog94;   // Build 94: строки WATCHDOG от повтора чтения записей миссии (и при служебном старте flag msn)
    sibling(fn, "ph6split95old"); if (sceIoGetstat(fn, &st) >= 0) ph6Split95 = 0;   // Build 95: без строки PH6MEM
    sibling(fn, "dlretry95old"); if (sceIoGetstat(fn, &st) >= 0) dlRetry95 = 0;   // Build 95: лист клетки после отказа памяти пробуется каждый кадр и молча, как было
    sibling(fn, "roomanimgt95old"); if (sceIoGetstat(fn, &st) >= 0) gtOn95 = 0;   // Build 95: анимации текстур и мерцания на настенном времени, как было
    sibling(fn, "gagreset96old"); if (sceIoGetstat(fn, &st) >= 0) collectGagReset96 = 0;   // Build 96: гэги комнаты на входе не сбрасываются, как было
    sibling(fn, "roomplace96old"); if (sceIoGetstat(fn, &st) >= 0) homerRoomPlace96 = 0;   // Build 96: постановка на пол комнаты снова «hold» сторожа, как было
    sibling(fn, "msnartretry94old"); if (sceIoGetstat(fn, &st) >= 0) msnartRetry94 = 0;   // Build 94: запись предмета миссии без повтора, как было
    sibling(fn, "runslide1230old"); if (sceIoGetstat(fn, &st) >= 0) homerRunSlide1230 = 0;   // №1230: бег по косой вдоль стены гасится в ноль, как было
    sibling(fn, "memchk1237old"); if (sceIoGetstat(fn, &st) >= 0) memChk1237 = 0;   // №1237: без экрана про Use Extra Memory
    sibling(fn, "npcretry96old"); if (sceIoGetstat(fn, &st) >= 0) npcRetry96 = 0;   // Build 96: повтор модели персонажа как было (каждый опрос, строка на каждый)
    sibling(fn, "npclad1235old"); if (sceIoGetstat(fn, &st) >= 0) npcLad1235 = 0;   // №1235: модель персонажа миссии мимо лестницы, молча пусто при нехватке памяти, как было
    sibling(fn, "camlr1231old"); if (sceIoGetstat(fn, &st) >= 0) camLR1231 = 0;   // №1231: L+R пешком — как было (ничего не делает)
    sibling(fn, "drop1229old"); if (sceIoGetstat(fn, &st) >= 0) uiDrop1229 = 0;   // №1229: «Item Dropped!» как было (висит до конца миссии, крупнее и ниже)
    sibling(fn, "glass1224old"); if (sceIoGetstat(fn, &st) >= 0) glassSolid1224 = 0;   // №1224: целое стекло пустое для Гомера пешком, как было
    sibling(fn, "bench1034old"); if (sceIoGetstat(fn, &st) >= 0) bench1034 = 0;   // №1034: скамейки и большие низкие пропы пустые для Гомера пешком, как было
    sibling(fn, "ramp1050old"); if (sceIoGetstat(fn, &st) >= 0) colRamp1050 = 0;   // №1050: торец тонкой стенки гасит машину, как было
    sibling(fn, "ramprail1050old"); if (sceIoGetstat(fn, &st) >= 0) colRampRail1050 = 0;   // №1050 (Б): торцы бортов трамплина L5 держат, как было
    sibling(fn, "dmgmercy96old"); if (sceIoGetstat(fn, &st) >= 0) carDmgMercy96 = 0;   // Опора-93: пощада на последнем ударе как была (машина от ударов не взрывалась)
    sibling(fn, "seller96old"); if (sceIoGetstat(fn, &st) >= 0) gameSeller96 = 0;   // L7 m2: продавец и бонус-персонаж, как было
    sibling(fn, "cardinfo96old"); if (sceIoGetstat(fn, &st) >= 0) uiCardInfo96Set(0);   // описание карточки: две строки, как было
    sibling(fn, "camin1041old"); if (sceIoGetstat(fn, &st) >= 0) camIn1041 = 0;
    sibling(fn, "msncamwallold"); if (sceIoGetstat(fn, &st) >= 0) mcWall103 = 0;   // Р3: наезд с диска без сдвига от стены, как было
    sibling(fn, "msncamfar"); { SceUID f_ = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f_ >= 0) { char b_[16] = {0}; sceIoRead(f_, b_, 15); sceIoClose(f_); float v_ = (float)atof(b_); if (v_ >= 10.f && v_ <= 300.f) camFar1041 = v_; } }   // Р3: длина наезда числом (метры от игрока)
    sibling(fn, "msncamshort96"); if (sceIoGetstat(fn, &st) >= 0) mcShort96 = 1;   // Р3: синтетический наезд 1246 вместо родных (файл)
    sibling(fn, "msncamshort96old"); if (sceIoGetstat(fn, &st) >= 0) mcShort96 = 0;   // 1246: наезды с диска, как было   // №1041: наезд с первого кадра и склейка на выходе, как было
    sibling(fn, "trfcost1001bold"); if (sceIoGetstat(fn, &st) >= 0) trfCost1001b = 0;   // №1001б: pathGap прежним полным обходом
    sibling(fn, "shadowpplold"); if (sceIoGetstat(fn, &st) >= 0) slPplOld = 1;
    sibling(fn, "cityopqold"); if (sceIoGetstat(fn, &st) >= 0) cityOpqOn = 0;   // №907-город: куски города с пустым смешиванием остаются полупрозрачными, как было
    sibling(fn, "bbbatchold"); if (sceIoGetstat(fn, &st) >= 0) bbBatchOn = 0;   // №912: квады билбордов по вызову на квад, как было
    sibling(fn, "lspnearold"); if (sceIoGetstat(fn, &st) >= 0) lspNearOn = 0;   // №913: свет обходит все записи, как было
    sibling(fn, "ggpropbatchold"); if (sceIoGetstat(fn, &st) >= 0) ggPropBatchOn = 0;   // №913: срез кусков пропов полным обходом, как было
    sibling(fn, "skinslotold"); if (sceIoGetstat(fn, &st) >= 0) skinSetSlotOpt(0);   // №914: слоты костей как в экспорте
    sibling(fn, "slprops933old"); if (sceIoGetstat(fn, &st) >= 0) slProps933 = 0;   // №933
    sibling(fn, "piececull923old"); if (sceIoGetstat(fn, &st) >= 0) pieceCull923 = 0;   // №923
    sibling(fn, "carink1028old"); if (sceIoGetstat(fn, &st) >= 0) carInk1028 = 0;   // №1028: сдвиг оболочки своей машины и машин миссий прежний (30 см)
    sibling(fn, "roomdlold"); if (sceIoGetstat(fn, &st) >= 0) roomDlOn = 0;   // листы комнат (РОВНЫЕ_30 шаг 2): комнаты без готовых листов GE (прямой путь, как до правки)
    sibling(fn, "carink991old"); if (sceIoGetstat(fn, &st) >= 0) carInk991 = 0;   // №991: сдвиг оболочки NPC-машин прежний (30 см)
    sibling(fn, "spnear1000old"); if (sceIoGetstat(fn, &st) >= 0) spNear1000 = 0;   // №1000: далёкие деревья шагают каждый кадр, как было
    sibling(fn, "boothdecal1052old"); if (sceIoGetstat(fn, &st) >= 0) msnartDecal1052 = 0;   // №1052: пятно-сложение под значком телефона как было
    sibling(fn, "arrows1025old"); if (sceIoGetstat(fn, &st) >= 0) msnartArrow1025 = 0;   // №1025: стрелки пути как были (с бликами, набором на самую высокую точку)
    sibling(fn, "dropink922old"); if (sceIoGetstat(fn, &st) >= 0) dropInk922 = 0;   // №922: заплатка обводки машин отключена (прежнее)
    sibling(fn, "cltfix940old"); if (sceIoGetstat(fn, &st) >= 0) cltFix940 = 0;   // №940: память длинных треугольников как было
    sibling(fn, "dropfix921old"); if (sceIoGetstat(fn, &st) >= 0) dropFix921 = 0;   // №921: заплатка выброса GE у пропов и света отключена (прежнее)
    sibling(fn, "crmoveold"); if (sceIoGetstat(fn, &st) >= 0) crMoveOn = 0;   // №919: допуски повтора отсечения как у B8
    sibling(fn, "pbfastold"); if (sceIoGetstat(fn, &st) >= 0) pbFastOn = 0;   // №915: запечённые листы — полные обходы, как было
    sibling(fn, "blendopqold"); if (sceIoGetstat(fn, &st) >= 0) blendOpqOn = 0;   // №907: пропы с пустым смешиванием в очереди полупрозрачного, как было
    sibling(fn, "latticeold"); if (sceIoGetstat(fn, &st) >= 0) latticeOn = 0;   // №904: решётки с полным набором мипов, как было
    sibling(fn, "treeshold"); if (sceIoGetstat(fn, &st) >= 0) treeShOn = 1;   // №753: тени деревьев рисовать как было   // №748/749: тени людей и предметов, прыжок, край — как в TEST31
    sibling(fn, "veclightold"); if (sceIoGetstat(fn, &st) >= 0) vecLight = 0;   // №715/736: файл-выключатель чёткого света
    old_ = SL_SH_HALF_OLD; sibling(fn, "shadowhalfold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;   // №716/722: тени полутоном; откат файлом shadowhalfold или -DSL_SH_HALF_OLD=1
    slShHalf = !old_;
    old_ = CONE_WIDE_OLD; sibling(fn, "coneold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;
    if (!old_) { lspConeW = LSP_CONE_W; lspConeL = LSP_CONE_L; lspConeA = LSP_CONE_A; }
    old_ = GLOW_SPRITE_OLD; sibling(fn, "glowsprold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;   // №690: откат файлом glowsprold или -DGLOW_SPRITE_OLD=1
    wrGlowOn = !old_;
    old_ = LIGHTCONE_SPRITE_OLD; sibling(fn, "lightconeold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;   // №703: конусы до 60 м, не больше 12, гасятся у глаза; откат — файл lightconeold или -DLIGHTCONE_SPRITE_OLD=1
    lspConeOld = old_;
    old_ = HOMER_SPOT_OLD; sibling(fn, "homerspotold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;
    slHomerSpot = !old_;
    old_ = !CAR_SH_SPR; sibling(fn, "carshold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;   // №680: тень машин веером; откат файлом carshold или -DCAR_SH_SPR=0
    slCarShOn = !old_;
    old_ = SL_SH_FAN_OLD; sibling(fn, "shfanold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;   // №698: прежняя тень текстурой
    slShFanOld = old_;
    old_ = HOMER_LIT_OLD; sibling(fn, "homerlitold"); if (sceIoGetstat(fn, &st) >= 0) old_ = 1;
    if (!old_) { skinSunPct = SKIN_LIT_SUN_PCT; skinAmbPct = SKIN_LIT_AMB_PCT; } }
  // №660: гамма палитр числом из файла «gamma» рядом с EBOOT (85..130, пробелы и перевод строки вокруг числа допустимы), читается при старте, без пересборки: Роб кладёт файл и смотрит на экране PSP.
  // Нет файла или число вне 85..130 — как раньше (100). -DGAMMA_FILE_OLD=1 — файл не читается. Палитры мира тонируются при загрузке уровня уже с этим числом (toneSetGamma до первого уровня)
#ifndef GAMMA_FILE_OLD
#define GAMMA_FILE_OLD 0
#endif
  if (!GAMMA_FILE_OLD) { char fn[256]; sibling(fn, "gamma"); SceUID gf = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (gf >= 0) { char gb[16] = {0}; int gn = sceIoRead(gf, gb, sizeof gb - 1); sceIoClose(gf);
      if (gn > 0) { char *ge = 0; long gv = strtol(gb, &ge, 10); if (ge != gb && gv >= 85 && gv <= 130) toneSetGamma((int)gv); } } }   // и файлом pcold рядом с EBOOT (PSP)
  if (smokeWantsFlag(path, "pausecellold")) pauseCellOld = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "pausecellold"); if (sceIoGetstat(fn, &st) >= 0) pauseCellOld = 1; }   // №598: откат шрифта кода клетки
  if (smokeWantsFlag(path, "startnoloadold")) startNoLoadOld = 1;
  if (smokeWantsFlag(path, "startnewsold")) startNewsOld = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "startnewsold"); if (sceIoGetstat(fn, &st) >= 0) startNewsOld = 1; }
  if (smokeWantsFlag(path, "newsiris")) newsIris = 1;
  if (smokeWantsFlag(path, "pfetchold")) pfOld = 1;
  if (smokeWantsFlag(path, "prereadold")) prereadOld = 1;
  ioWaitNote = ioWaitNoteMain; ioWaitLimitOld = waitLimitOld; msioLockTimeout = msioLockTimeoutMain;   // ТЕСТ90
  if (smokeWantsFlag(path, "waitlimitold")) { waitLimitOld = 1; ioWaitLimitOld = 1; }   // ТЕСТ90: ожидания без предела, как в TEST89
  if (smokeWantsFlag(path, "roomiop1on") && !ROOMIO_OLD) roomPfOn = 1;   // ТЕСТ90 П1: предчтение комнат включить (если по умолчанию выключено)
  if (smokeWantsFlag(path, "syncreadp1old")) syncReadP1Old = 1;   // ТЕСТ90 П1: гэги синхронно в кадре, как в TEST89
  if (smokeWantsFlag(path, "loadwaitold")) loadWaitOld = 1;
  { char fn[256]; SceIoStat st;   // файлы-выключатели пути загрузки «как в TEST26» (набор tools/test26load_set.sh): ящик синхронный, REGPRE без гистерезиса, стингер газеты как раньше
    sibling(fn, "bbsyncold"); if (sceIoGetstat(fn, &st) >= 0) bbSyncOld = 1;
    sibling(fn, "noise949old"); if (sceIoGetstat(fn, &st) >= 0) nzOn = 0;   // №949: шум по родным UV, как было
    sibling(fn, "ground969old"); if (sceIoGetstat(fn, &st) >= 0) gdStyle = 0;
    sibling(fn, "curb974old"); if (sceIoGetstat(fn, &st) >= 0) curbOn = 0;
    sibling(fn, "bling989old"); if (sceIoGetstat(fn, &st) >= 0) fxBling989 = 0;   // №989: блики на объектах как в TEST70
    sibling(fn, "camcfg996old"); if (sceIoGetstat(fn, &st) >= 0) camCfg996On = 0;   // №996: камера без множителей страницы CAMERA
    sibling(fn, "ink978old"); if (sceIoGetstat(fn, &st) >= 0) ink978On = 0;
    sibling(fn, "trfgeo984old"); if (sceIoGetstat(fn, &st) >= 0) trfGeoOld = 1;   // №984: прежний расчёт проверок риска GE
    sibling(fn, "tone1026old"); if (sceIoGetstat(fn, &st) >= 0) toneSet1026(0);   // №1026: вывески мимо шагов мультика и контраста, как в TEST71
    sibling(fn, "coin1020old"); if (sceIoGetstat(fn, &st) >= 0) fxCoin1020 = 0;   // №1020: блики монет кольцом из трёх, кадр оборота общий, как в TEST71
    sibling(fn, "glow1261old"); if (sceIoGetstat(fn, &st) >= 0) glow1261On = 0;   // №1261: без свечения
    sibling(fn, "bubble1262old"); if (sceIoGetstat(fn, &st) >= 0) uiBubble1262Set(0);   // №1262: без выезда
    sibling(fn, "bubble1251old"); if (sceIoGetstat(fn, &st) >= 0) uiBubble1251Set(0);   // №1251: кружок и рамка вместе
    sibling(fn, "bubble1247old"); if (sceIoGetstat(fn, &st) >= 0) { uiBubble1247Set(0); mvBubble1247Old = 1; }   // №1247: пузырь как в TEST96
    sibling(fn, "bubble1040old"); if (sceIoGetstat(fn, &st) >= 0) uiBubble1040Set(0);   // №1040: появление пузыря как в TEST72
    sibling(fn, "wrap1037old"); if (sceIoGetstat(fn, &st) >= 0) uiWrap1037Set(0);   // №1037: перенос текстов как в TEST72
    sibling(fn, "inksep1009old"); if (sceIoGetstat(fn, &st) >= 0) inkSep1009 = 0;   // №1009: порядок рисования машин трафика как в TEST71
    sibling(fn, "walk1005old"); if (sceIoGetstat(fn, &st) >= 0) walk1005On = 0;   // №1005: плитка тротуара L3 заливкой, как в TEST71
    sibling(fn, "xwalk999old"); if (sceIoGetstat(fn, &st) >= 0) xwalk999On = 0;   // №999: переход на брусчатке как в TEST71
    sibling(fn, "curb987old"); if (sceIoGetstat(fn, &st) >= 0) curb987On = 0;   // №987: бордюр, кромка, контур тропинок и уголки как в TEST69
    sibling(fn, "curb1064old"); if (sceIoGetstat(fn, &st) >= 0) { curb1064Old = 1; curbNat1064 = curbWant1064 = 0; }   // №1064: файл-флаг — бордюр как в TEST76 (заливка и подкраска №987), строка Curbs не действует
    sibling(fn, "zones1007old"); if (sceIoGetstat(fn, &st) >= 0) zn7On = 0;   // №1007: стыки зон как в TEST71
    sibling(fn, "floor1030old"); if (sceIoGetstat(fn, &st) >= 0) floor1030On = 0;   // №1030: земля с общим шумом и тёплым песком, как в TEST71
    sibling(fn, "fehero1021old"); if (sceIoGetstat(fn, &st) >= 0) feHero1021 = 0;   // №1021-герои: Гомер в меню как в TEST71
    sibling(fn, "beam1036old"); if (sceIoGetstat(fn, &st) >= 0) beam1036 = 0;   // №1036: луч тарелки бледно-мятный, как в TEST72
    sibling(fn, "vote1048old"); if (sceIoGetstat(fn, &st) >= 0) vote1048On = 0;   // №1048: без голоса фургона Квимби
    sibling(fn, "walkcold985old"); if (sceIoGetstat(fn, &st) >= 0) curbAll = 0;   // №985: тротуар тёплый, как в TEST67
    sibling(fn, "edge983old"); if (sceIoGetstat(fn, &st) >= 0) edgeOn = 0;   // №983: без кромки
    sibling(fn, "path983old"); if (sceIoGetstat(fn, &st) >= 0) pathOn = 0;   // №983: контур тропинок как был
    sibling(fn, "asphlightold"); if (sceIoGetstat(fn, &st) >= 0) asphOn = 0;   // №979: асфальт как был   // №978: контур бордюра как был   // №974: бордюр как был   // №969: прежний шум земли
    sibling(fn, "loadwaitold"); if (sceIoGetstat(fn, &st) >= 0) loadFuseIoWaitOld(1);
    sibling(fn, "regprehystold"); if (sceIoGetstat(fn, &st) >= 0) regPreHyst = 0;
    sibling(fn, "regprememold"); if (sceIoGetstat(fn, &st) >= 0) regPreMemOld = 1;
    sibling(fn, "loadnewsmusicold"); if (sceIoGetstat(fn, &st) >= 0) loadNewsMusicOld = 1; }
  { char fn[256]; SceIoStat st; sibling(fn, "loadwaitold"); if (sceIoGetstat(fn, &st) >= 0) loadWaitOld = 1; }   // №LOADWAIT: откат
  if (smokeWantsFlag(path, "tutresumeold")) tutResumeOld = 1;
  if (smokeWantsFlag(path, "card1063old")) card1063Old = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "card1063old"); if (sceIoGetstat(fn, &st) >= 0) card1063Old = 1; }   // №1063: откат — свечение карточки блик-звездой №839
  if (smokeWantsFlag(path, "walkcam1059old")) homerWalkCam1059Old = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "walkcam1059old"); if (sceIoGetstat(fn, &st) >= 0) homerWalkCam1059Old = 1; }   // №1059: откат — кадр управления держится до отпускания стика
  if (smokeWantsFlag(path, "ink1062old")) ink1062Old = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "ink1062old"); if (sceIoGetstat(fn, &st) >= 0) ink1062Old = 1; }   // №1062: откат — контур окружения как в TEST74
  { char fn[256]; SceIoStat st; sibling(fn, "tutresumeold"); if (sceIoGetstat(fn, &st) >= 0) tutResumeOld = 1; }   // №670: откат
  { char fn[256]; SceIoStat st; sibling(fn, "prereadold"); if (sceIoGetstat(fn, &st) >= 0) prereadOld = 1; sibling(fn, "prereadsum"); if (sceIoGetstat(fn, &st) >= 0) prereadSum = 1; }   // №PREREAD: откат; prereadsum — служебная сумма буфера для гейта
  { char fn[256]; SceIoStat st; sibling(fn, "waitlimitold"); if (sceIoGetstat(fn, &st) >= 0) { waitLimitOld = 1; ioWaitLimitOld = 1; } }   // ТЕСТ90: файл-выключатель
  { char fn[256]; SceIoStat st; sibling(fn, "roomiop1on"); if (sceIoGetstat(fn, &st) >= 0 && !ROOMIO_OLD) roomPfOn = 1; }   // ТЕСТ90 П1: файл-включатель предчтения комнат
  { char fn[256]; SceIoStat st; sibling(fn, "syncreadp1old"); if (sceIoGetstat(fn, &st) >= 0) syncReadP1Old = 1; }   // ТЕСТ90 П1: файл-выключатель
  { char fn[256]; SceIoStat st; sibling(fn, "pfetchold"); if (sceIoGetstat(fn, &st) >= 0) pfOld = 1; }   // №PFETCH: откат
  if (smokeWantsFlag(path, "roomcamswitchold")) roomCamSwitchOld = 1;
  if (smokeWantsFlag(path, "footroom1209old")) footRoom1209Old = 1;
  if (smokeWantsFlag(path, "msntelefree1213old")) msnTeleFree1213Old = 1;   // №1213: перенос миссии без выгрузки далёких клеток
  if (smokeWantsFlag(path, "carstuck1200old")) carStuck1200Old = 1;   // №1200: откат автосброса застрявшей машины   // №1209: откат пропуска foot в комнату вне комнаты
  if (smokeWantsFlag(path, "roomexitring1209old")) roomExitRing1209Old = 1;   // ag/loadorder: выход из комнаты без кольца клеток у двери (стрим, как было)
  if (smokeWantsFlag(path, "footlowland1209old")) footLowland1209Old = 1;   // ag/loadorder: foot ниже −10 м вне комнаты тоже пропускается, как в №1209
  if (smokeWantsFlag(path, "teledoor1209old")) teleDoor1209Old = 1;   // ag/loadorder: перенос в комнату читает кольцо вокруг точки в кармане комнат, как было
  { char fn[256]; SceIoStat st; sibling(fn, "roomexitring1209old"); if (sceIoGetstat(fn, &st) >= 0) roomExitRing1209Old = 1; sibling(fn, "teledoor1209old"); if (sceIoGetstat(fn, &st) >= 0) teleDoor1209Old = 1; }
  { char fn[256]; SceIoStat st; sibling(fn, "roomcamswitchold"); if (sceIoGetstat(fn, &st) >= 0) roomCamSwitchOld = 1; }   // №229 школа: откат правила смены камеры комнаты
  if (smokeWantsFlag(path, "resumeringold")) resumeRingOld = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "colbakeold"); if (sceIoGetstat(fn, &st) >= 0) colBakeOld = 1; sibling(fn, "colbakecheck"); if (sceIoGetstat(fn, &st) >= 0) colBakeCheck = 1; sibling(fn, "colbakedump"); if (sceIoGetstat(fn, &st) >= 0) colBakeDumpF = 1; }   // №bake-3Б
  { char fn[256]; SceIoStat st; sibling(fn, "texbakeold"); if (sceIoGetstat(fn, &st) >= 0) texBakeOld = 1; sibling(fn, "texbakedump"); if (sceIoGetstat(fn, &st) >= 0) texBakeDump = 1; sibling(fn, "texbakecheck"); if (sceIoGetstat(fn, &st) >= 0) texBakeCheck = 1; sibling(fn, "texbakediag"); if (sceIoGetstat(fn, &st) >= 0) texBakeDiag = 1;   // №bake-3А
    sibling(fn, "texbakecorrupt"); SceUID cf_ = sceIoOpen(fn, PSP_O_RDONLY, 0); if (cf_ >= 0) { char c_ = '1'; if (sceIoRead(cf_, &c_, 1) < 1) c_ = '1'; texBakeCorrupt = c_ == '2' ? 2 : 1; sceIoClose(cf_); } }
  { char fn[256]; SceIoStat st; sibling(fn, "lmaphashold"); if (sceIoGetstat(fn, &st) >= 0) lmapHashOld = 1; }   // №bake-3Д: откат
  { char fn[256]; SceIoStat st; sibling(fn, "resumeringold"); if (sceIoGetstat(fn, &st) >= 0) resumeRingOld = 1; }   // №RESUME-круг: откат
  { char fn[256]; SceIoStat st; sibling(fn, "newsiris"); if (sceIoGetstat(fn, &st) >= 0) newsIris = 1; }   // №663: кружок после газеты — по файлу (по умолчанию жёсткий срез, как PS2)   // №663: откат на ирис без газеты
  if (smokeWantsFlag(path, "loadsilent")) loadSilent = 1;
  { char fn[256]; SceIoStat st; sibling(fn, "loadsilent"); if (sceIoGetstat(fn, &st) >= 0) loadSilent = 1; }   // №665: тишина на газете, как у PS2   // №663: откат на ирис без газеты
  { char fn[256]; SceIoStat st; sibling(fn, "startnoloadold"); if (sceIoGetstat(fn, &st) >= 0) startNoLoadOld = 1; }   // №576: откат на экран загрузки
  padResyncOld = smokeWantsFlag(path, "padresyncold");   // правка 279: красный контроль — флаг сценария читается до меню (ролик раньше цикла игры)
  audioStreamNote = streamNote;   // правка 155: журнал потоков музыки и атмосферы (пишет только в смоуке)
  { char s[256]; bootReport(s, sizeof s); smkWrite(s); }   // писк заставки сыгран? (tools/boot_gate.sh)
  { SndStats ss; sndStats(&ss); char s[96]; snprintf(s, sizeof s, "BOOTMUSIC music=%s state=%d\n", ss.music, ss.musicState); smkWrite(s); }   // правка 205: тема уровня 1 под заставкой
  svmBlock(saveBlocked); svmSetLog(svLog); svmScan(); svmDevBoot(levelAvailable); svmAllBoot();   // правка 335: флаг-файл devsave -> DEV SAVE в нижнем слоте   // правка 238: и в чёрный ящик (на PSP без smoke строки SV* терялись)   // сохранения: слоты читаются на экране BootupLoad, самое свежее — для RESUME GAME
  sfxLogStart();   // правка 135.7: флаг-файл sfxlog — журнал звуков эффектов
  uiSfxInit(smkWrite);   // звуки интерфейса уже в главном меню и OPTIONS (frontend.spt), не только в игре
  svcLabel = svcFile(); svcMode = svcStartP() || svcLabel;   // правка 44 (и выбор уровня в меню): служебный режим известен до меню
  feColdRequested = smokeWantsFlag(path, "fecold");
  pauseRowsSvc();
  setBootCheck();   // TEST93 (Данные-93, C1+C2): штамп набора set.txt и полнота файлов уровней; служебный режим — только журнал SETREV/SETCHECK
  menuOn = uiReady && mainNew >= 0 && (smokeWantsMenu(path) || !svcStartP());
  if (menuOn && (!svcStartP() || smokeWantsIntro(path) || feColdRequested))
    introRun(&fbp0, 0, !FE_START_INTRO_OLD || FE_COLD_FLOW_OLD || (svcMode && !feColdRequested) ? 5 : 4);
  if (menuOn) { SCR_CLOSE(); menuRun(&fbp0); }   // boot.shr закрыт до меню (куча меню без пакетов); NEW GAME оставляет открытым Loading.scr — под ним читается уровень
  { char fn[256]; sibling(fn, "homer.shr");
    homerM = skinLoadH(fn);   // этап 4: Гомер (tools/homer_export.py)
    if (homerM && homerInit(&homer, homerM) == 0) haveHomer = 1;
    homerDoorCan = homerDoorCanFn;   // №667
    if (haveHomer) { homer.solid = homerSolidAll; homer.solidUser = 0; homerCamRayExtra = camCarsFrac; } }   // 284: камера пешком обходит машины   // ящики/автоматы, трафик и пешеходы — твёрдые
  { char fn[256]; sibling(fn, "view60"); SceUID v6 = sceIoOpen(fn, PSP_O_RDONLY, 0); if (v6 >= 0) { sceIoClose(v6); longView = 0; } }   // служебное: старт в режиме 60 FPS / 115 м
  if (!FARPLAIN_OLD) { char fn[256], b[16]; sibling(fn, "far_plain"); SceUID o = sceIoOpen(fn, PSP_O_RDONLY, 0);   // правка 508: дальняя версия города без нарезки; число в файле — D, м
    if (o >= 0) { int n = sceIoRead(o, b, 15); sceIoClose(o); b[n > 0 ? n : 0] = 0; farPlainD = n > 0 ? strtof(b, 0) : 100.f; if (farPlainD < 60.f) farPlainD = 100.f; if (farPlainD > 240.f) farPlainD = 240.f; } }
  { char fn[256], b[16]; sibling(fn, "oldstream"); SceUID o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); oldStream = 1; }
    sibling(fn, "oldfog"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); fogCol = SKY & 0xffffff; oldFog = 1; }
    sibling(fn, "far1042old"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); far1042On = 0; }   // №1042: потолок города 150 м при Draw Distance 250, как в TEST72
    sibling(fn, "fogoff1042old"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); fogOff1042On = 0; }   // №1042: дымка края дальности как в TEST72
    sibling(fn, "fogcity1042old"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); fogCity1042On = 0; }   // №1042: туман города как в TEST72/73
    sibling(fn, "inkfar1043old"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); inkFar1043On = 0; }   // №1043: линии мира и в пустоте без клеток, как было
    sibling(fn, "far130"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); hz130On = 1; }   // №913: файл far130 рядом с EBOOT — 130 м
    sibling(fn, "flypath"); o = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (o >= 0) { int n = sceIoRead(o, b, 15); sceIoClose(o); b[n > 0 ? n : 0] = 0; flyPathV = n > 0 ? strtof(b, 0) : 30.f; if (flyPathV <= 0.f) flyPathV = 30.f; }
    sibling(fn, "loadacc"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); loadAccOn = 1; }
    sibling(fn, "farroad934old"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); road934On = 0; }   // №934: файл farroad934old рядом с EBOOT — без дальних дорог и мостов (прежнее)
    sibling(fn, "pano917old"); { SceIoStat st_; if (sceIoGetstat(fn, &st_) >= 0) pano917On = 0; }   // №917: файл-флаг — без кольца-панорамы дали
    sibling(fn, "silh"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); silhOn = 1; }   // Загрузка 3: файл silh рядом с EBOOT = силуэты включены   // Загрузка 3: учёт подгрузки раз в секунду (main_stream.h)
    sibling(fn, "horizon"); o = sceIoOpen(fn, PSP_O_RDONLY, 0); if (o >= 0) { char hb_[4] = {0}; int hn_ = sceIoRead(o, hb_, 1); sceIoClose(o); horizonOn = (hn_ == 1 && hb_[0] >= '1' && hb_[0] <= '8') ? hb_[0] - '0' : 8; }   // Горизонт: файл horizon рядом с EBOOT = силуэты включены; первый байт-цифра 1..8 — вариант цвета (flag horizon N), пустой файл = 8 (смесь дымки и неба, градиент)
    sibling(fn, "slowio"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);   // служебное: КБ/с медленной карты
    sibling(fn, "streamlog"); SceUID g = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (g >= 0) { int n = sceIoRead(g, b, 15); sceIoClose(g); b[n > 0 ? n : 0] = 0; stLogAt = n > 0 ? strtof(b, 0) : 30.f; if (stLogAt <= 0.f) stLogAt = 30.f; }
    if (f >= 0) { int n = sceIoRead(f, b, 15); sceIoClose(f); b[n > 0 ? n : 0] = 0; slowIoKBs = n > 0 ? strtoul(b, 0, 10) : 1500; if (!slowIoKBs) slowIoKBs = 1500; } }
  loaderEnsure();   // загрузчик (viewer/main_stream.h): мог быть поднят раньше предчтением уровня (№PREREAD)
  hudInit(); uiSfxInit(smkWrite); tutSetLog(smkWrite);   // звуки интерфейса в игре и гейт анимаций HUD (viewer/ui_sfx.c); подсказки
  fxSetGeRoom(geRoom);   // правка 103: частицы — только если есть место в списке GE
  skidSetGeRoom(geRoom);   // и следы шин (правка 249)
  mvSetLog(smkWrite); shopViewSetLog(smkWrite); wardrobeSetLog(smkWrite); phoneViewSetLog(smkWrite);
  levelLoad(&fbp0);   // набор уровня gameLevel (выбран в меню; служебный запуск — уровень 1)
  if (sbWanted()) { sbRun(&fbp0); blackBox(1); sceKernelExitGame(); }   // тур полос (stripbench.h): только с файлом stripbench


  float x = spawnP[0], y = spawnP[1], z = spawnP[2], h = spawnP[3], v = 0, vy = 0, steer = 0;
  if (haveCol) { x = car.x; y = car.y; z = car.z; }   // машина уже на старте (levelLoad)
  { float g; if (ground(x, z, y + 2.f, 0.f, &g)) y = g; }
  static float weR_ = 2.4f; static float eyeOv[6]; static int eyeOvOn; static float trfgoH = 2.2f;   // служебно (smoke, №634): flag eyex/eyey/eyez/eyedx/eyedy/eyedz/eyeon — глаз и взгляд кадра задаются сценарием (замер «камера у стены»)
  int hudOn0 = 0;   // релиз с KEEP_HUD: счётчик скрыт, пока рядом с EBOOT нет файла hudon (есть — виден с первого кадра); R+SELECT переключает. Файл игроку не заперт (release.h)
  if (SHR_REL && SHR_KEEP_HUD) { char fn_[256]; SceIoStat st_; sibling(fn_, "hudon"); hudOn0 = sceIoGetstat(fn_, &st_) >= 0; }
  int fly = 0, hud = hudOn0, pending = 0, autob = 0, frames = 0, hudTick = 0; unsigned prev = 0;
  PadEdges padE = {0, 0, 0}; unsigned briefSince = 0; int briefShown = 0;   // правка 25: пульт по отсчётам; брифинг — нажатия после его показа
  svcMode = svcStartP() || svcFile();   // правка 44: служебные кнопки и строки — только здесь
  pauseRowsSvc();
  float fx = x, fy = y + 3, fz = z, fyaw = 0, fpitch = -.2f, fps = 0, workMs = 0, workMax = 0, cpuMs = 0, shownMax = 0;
  float camx = x - sinf(h) * 7, camy = y + 3, camz = z - cosf(h) * 7;
  sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  unsigned last = sceKernelGetSystemTimeLow(), acc = last, optUser;
  SceCtrlData pad;
  // служебное: файл autobench рядом с EBOOT -> тест-прогон сразу и выход (для проверки в эмуляторе)
  { char fn[256]; sibling(fn, "autobench"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); autob = 1; }
    sibling(fn, "pvscheck"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); autob = 3; }
    sibling(fn, "autodrive"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); autodrive = 1; }
    sibling(fn, "autostraight"); f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { char b[16]; int n = sceIoRead(f, b, 15); sceIoClose(f); autodrive = 2; if (n > 0) { b[n] = 0; adStop = strtof(b, 0); } }   // число в файле — через сколько секунд тормозить (проверка пропов)
    sibling(fn, "testdmg"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); car.damage = prevDmg = .9f; }   // служебное: сильный дым, машина ещё цела
    sibling(fn, "testboom"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); car.damage = prevDmg = 1.f; car.destroyed = 1; }   // служебное: остов сразу
    sibling(fn, "slowmo"); f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); slowmo = 1; } }
  // служебное: opts.txt (маска переключателей, hex) и goto.txt (x z курс) — для сравнения картинок
  { char fn[256], b[64]; sibling(fn, "opts.txt"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { int n = sceIoRead(f, b, 63); sceIoClose(f); if (n > 0) { b[n] = 0; opt = strtoul(b, 0, 16); } }
    sibling(fn, "goto.txt"); f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { int n = sceIoRead(f, b, 63); sceIoClose(f);
      if (n > 0) { b[n] = 0; char *e; x = strtof(b, &e); z = strtof(e, &e); h = strtof(e, 0); y = 200; float g; if (ground(x, z, y, 0.f, &g)) y = g;
        if (haveCol) { carPlace(&car, &world, x, y, z, h); x = car.x; y = car.y; z = car.z; h = car.h; }   // этап 3б: goto.txt ставит и машину, не только камеру
        camx = x - sinf(h) * 7.5f; camy = y + 3.2f; camz = z - cosf(h) * 7.5f; } } }

  // служебное (проверка машины снимками): camang.txt — угол камеры вокруг машины, рад; carseq.txt — steer/brake/rev/drive
  { char fn[256], b[32]; sibling(fn, "camang.txt"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { int n = sceIoRead(f, b, 31); sceIoClose(f); if (n > 0) { b[n] = 0; camAng = strtof(b, 0); } }
    sibling(fn, "carseq.txt"); f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { int n = sceIoRead(f, b, 31); sceIoClose(f); if (n > 0) { b[n] = 0;
      carSeq = b[0] == 's' ? 1 : b[0] == 'b' ? 2 : b[0] == 'r' ? 3 : b[0] == 'd' ? 4 : 0; } }
    if (camAng != 0.f) { camx = x - sinf(h + camAng) * 7.5f; camz = z - cosf(h + camAng) * 7.5f; } }

  if (haveHomer && haveCol) { HomerCar hc; homerCarFrom(&hc, &car, cv.lift); homerSetInCar(&homer, &hc); }   // этап 4: старт за рулём
  // 12а: главное меню, кроме служебных запусков (они, как раньше, сразу в игре; сценарий с «flag menu» — через меню)
  levelReady = 1;
  if (menuOn) menuStartGame(&fbp0, menuT0, 1);   // правка Роба 3: NEW GAME первого меню — новая игра и город под тем же Loading.scr; комнаты и камеры — там же (356)
  else { cellsRing(0, &fbp0); sndMusic(SND_MUS_FREEROAM); levelLoadAfter(); }   // служебный запуск сразу в игру: музыка свободной езды здесь, не на заставке
  // levelLoadAfter: 11.2 комнаты, правка 40 камеры старта миссий и зон — уровня gameLevel
  optUser = opt; toMenu = 0;   // первое меню уже было до чтения уровня
  if (!menuOn) SCR_CLOSE();   // boot.shr больше не нужен (в пути меню его закрыл титульный экран)
  for (;;) {
    if (exitReq) exitSafeNow();   // №739
    unsigned t0 = sceKernelGetSystemTimeLow(); smkMarkT0 = t0; memset(smkMarkT, 0, sizeof smkMarkT); BBP(BBP_INPUT); relFrame(svLog, t0); carOutlineFrame++;   // правка 103: фаза для сторожа; №1195: строки потоков (замки, стек) — в ящик и smoke.txt, зонд стека раз в 5 с
    unsigned hwHit, smHit;   // правка Роба 25: нажатия по всем отсчётам пульта с прошлого кадра (короткое между кадрами не теряется)
    { SceCtrlData pb[PADBUF_N]; int pn = sceCtrlPeekBufferPositive(pb, PADBUF_N); if (pn < 1) { pn = 1; memset(&pb[0], 0, sizeof(SceCtrlData)); }
      unsigned pts[PADBUF_N], pbt[PADBUF_N]; for (int i = 0; i < pn; i++) { pts[i] = pb[i].TimeStamp; pbt[i] = pb[i].Buttons; }
      if (padResync) { padResync = 0; padE.init = 0; prev = smokePad(); }   // правка 279: после ролика — пульт с нынешнего состояния (и нажатия сценария)
      pad = pb[pn - 1]; hwHit = padEdges(&padE, pts, pbt, pn, briefSince); }
    { unsigned sm = smokePad(); smHit = sm & ~prev; prev = sm; pad.Buttons |= sm; smokeStick(&pad.Lx, &pad.Ly); }   // служебное: press из scenario.txt; №947 stick
    // Правка Роба 159 (01.10 ~14:20): «управляю только левым аналогом — не считается активностью, PSP уходит в сон». Прошивка
    // сбрасывает таймер автосна только кнопками: отклонённый стик (за мёртвой зоной) — сами сообщаем об активности
    if (pad.Lx < 128 - STICK_ACTIVE || pad.Lx > 128 + STICK_ACTIVE || pad.Ly < 128 - STICK_ACTIVE || pad.Ly > 128 + STICK_ACTIVE) scePowerTick(PSP_POWER_TICK_ALL);
    float dt = (t0 - last) / 1000000.f; last = t0; if (dt > .05f) dt = .05f; if (slowmo) dt *= .25f;   // служебное: замедление для снимков
    { float n = floorf(dt * 60.f + .5f); if (n >= 1.f && fabsf(dt - n * (1.f / 60.f)) < .002f) dt = n * (1.f / 60.f); }   // шаг кадра = ровно N кадров развёртки: иначе дрожь таймера даёт 1 или 3 подшага физики вместо 2 — машина дёргается
    if (camDet && smkOn) dt = 1.f / 30.f;   // служебное (flag camdet): прогоны старого и нового кода идут кадр в кадр
    unsigned hit = hwHit | smHit;
    if (msFuzzOn && smkOn) hit |= msFuzzHit();   // правка 432: бот меню (flag msnselfuzz)
    ownerWatch(dt);   // Лестница-96: клетки неба и машины игрока не готовы - перечитать (WATCHDOG owner)
    memLadTick(dt);   // правка 435: лестница памяти — раз в секунду наибольший кусок, очередь «вернуть»
    if (msHeld432 && !mvMissionSelectOn()) msRelease432();   // правка 432: меню закрыто — радиус подгрузки обычный
#ifndef MSNBOT_OLD
#define MSNBOT_OLD 0   // 1 — flag msnbot не действует (бот миссий ночного цикла идёт без помощника: △ в машине выходит из неё и гонка падает)
#endif
    if (!MSNBOT_OLD && msnBot && smkOn && msnBotPass > 0.f) {   // 06.11 (прогон «начало-конец миссий»): этап стоит дольше msnBotPass с — помощник засчитывает его, как цель (MSN pass … why=stuck)
      static int key_ = -2, go_, passed_; static float since_; int k_ = mvStageKey();
      if (k_ != key_) { key_ = k_; since_ = smkT; go_ = 0; if (passed_ && mvServicePlace(1)) chase.init = 0; passed_ = 0; }   // после зачёта: слежка — к машине-цели (иначе «Failed to follow vehicle» сразу)
      else if (k_ >= 0 && !mvTalkActive() && !mvBriefing() && smkT - since_ > msnBotPass) {
        if (!go_ && mvServicePlace(0)) { go_ = 1; since_ = smkT - msnBotPass + 3.f; chase.init = 0; }   // сначала к цели этапа, как доехал бы игрок; 3 с — игре засчитать самой
        else { passed_ = mvServicePass("stuck"); since_ = smkT; } }
    }
    if (!MSNBOT_OLD && msnBot && smkOn && autodrive == 0) {   // №бот миссий: что делает живой игрок между msngo — пропускает реплики и брифинг ✕, △ у собеседника и двери, садится за руль на машинных этапах
      if (mvTalkActive()) { if (smkT >= msnBotT) { mvTalkInput(PSP_CTRL_CROSS); msnBotT = smkT + .6f; } }
      else if (mvBriefing()) hit |= PSP_CTRL_CROSS;
      else if (smkT >= msnBotT) { int need = mvServiceBot();
        if (!need && onFoot() && roomsDoorNear(&rooms, &rs, &G) && mvSvcDoorWant()) need = 1;   // 06.11: поставлен к двери / выходу — △, как игрок
        if (need == 1) { hit |= PSP_CTRL_TRIANGLE; msnBotT = smkT + 1.5f; if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "MSNBOT tri t=%.1f\n", smkT); smkWrite(s_); } }
        else if (need == 2 && haveHomer && haveCol && onFoot()) { HomerCar hc; homerCarFrom(&hc, &car, cv.lift); homerSetInCar(&homer, &hc); homerDoor(0, 0.f); chase.init = 0; msnBotT = smkT + 1.f;
          if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "MSNBOT seat t=%.1f\n", smkT); smkWrite(s_); } } } }
    if (autodrive == 4 && !adSeatOld) { if (mvTalkActive()) mvTalkInput(PSP_CTRL_CROSS); else if (mvBriefing()) hit |= PSP_CTRL_CROSS; }   // №914: вступление миссии (разговор, брифинг) автопилот проходит ✕, как игрок, — иначе мир стоит и езды нет
    if (uiReady && !uiPaused() && (wardrobeOn() || phoneViewOn() || shopViewOn())) { uiPadRepeatLR(phoneViewOn() && !boothLR1057Old); hit = uiPadRepeat(pad.Buttons, hit); uiPadRepeatLR(0); }   // №1057: в будке повторяются и L R   // №706: зажатая ◄ ► листает гардероб, будку, лавку (пауза делает это ниже)
    toneJobStep();   // 16.10: смена тона без остановки — несколько палитр за кадр (main_save.h)
    flatPoll();   // №912
    float wallDt = dt; dtBr1258 = 0;   // служебное: сценарий идёт и в паузе (там dt = 0)
    int resetCar = 0;
    if (uiReady && uiPaused()) {                   // этап 7: пауза — мир стоит, кнопки идут в меню (X выбрать, START/O — назад)
      hit = uiPadRepeat(pad.Buttons, hit);         // правка 202: зажатая ↑/↓ — автоповтор по спискам паузы
      if (!PAUSE_SUB_FADE_OLD && !svmActive()) {   // №639: отложенный выход из подэкрана
        unsigned pn_ = sceKernelGetSystemTimeLow(); unsigned bk_ = PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE;
        if (pxCl) { if (!(pauseSub == PM_OPTIONS || lsOpen || vcOpen)) pxCl = 0; else if ((unsigned)(pn_ - pxClT0) >= 200000u) { pxCl = 0; hit = pxClHit; } else hit = 0; }
        else if ((hit & bk_) && (pauseSub == PM_OPTIONS || lsOpen || (vcOpen && !vcZoom))) { pxCl = 1; pxClT0 = pn_; pxClHit = hit & bk_; hit = 0; }
      } else pxCl = 0;
      int act = UI_P_NONE, ms = pauseSub >= 0 || lsOpen || vcOpen || svmActive() ? 0 : mvMissionSelect(0, hit);   // пункт 8: список MISSION SELECT забирает кнопки, пока открыт
      if (ms == 1 && !mvMissionSelectOn() && !vcOpen && pauseSub < 0) SCR_CLOSE();   // список закрыт — пакет pextra.shr из кучи
      if (!MSHEAL_OLD && mvMissionSelectNeedPack()) {   // правка 432: меню в запасном виде — пакет открываем снова (раз в 0,7 с, до 8 раз), меню пересобирается из текущего состояния
        static unsigned healT; static int healN; unsigned nw_ = sceKernelGetSystemTimeLow();
        if (healN < 8 && (unsigned)(nw_ - healT) > 700000u) { healT = nw_; healN++; char fn_[256]; pausePackFile(fn_, "pextra.shr");
          if (!msPackOpen(fn_)) { mvMissionSelectPackReady(); healN = 0; } else if (smkOn) { char l_[80]; snprintf(l_, sizeof l_, "MSNSEL healfail n=%d t=%.1f\n", healN, smkT); smkWrite(l_); } } }
      if (svmOn()) { int mvCanSave(void); uiMenuEnable(UI_MENU_PAUSE, UI_P_SAVE_GAME, mvCanSave()); }   // SAVE GAME — свободная езда и вся «прогулка» миссии (правка 330)
      pauseMsAccess();                                 // пункт ДОБИВКИ 26: MISSION SELECT серый, когда выбрать нечего
      if (svmActive()) { int sr_ = svmInput(hit);
                         if (pauseAskAct && (sr_ == SVM_R_YES || sr_ == SVM_R_NO || sr_ == SVM_R_BACK)) {   // сверка g09a-hud круг 5 (M2:Д6): ответ на вопрос Restart / Abort Mission
                           int a_ = pauseAskAct; pauseAskAct = 0;
                           if (sr_ == SVM_R_YES) { uiPauseCloseNow(); if (a_ == UI_P_RESTART_MISSION) { mvPauseRestart(); chase.init = 0; } else { mvPauseAbort(); chase.init = 0; hnrAbortMsn = 1; } } } if (sr_ == SVM_R_QUIT && !lvSaveAsk) { if (mainNew >= 0) toMenu = 1; else { blackBox(1); exitSafeNow(); } }   // SaveGame.scr   // сверка g10b-loop (P13:4): вопрос «сохранить?» перед следующим уровнем выход в меню не даёт
                         else if (sr_ == SVM_R_UNPAUSE) uiPause(0); }   // сверка g09b-menu (P13-Д5): START на экране записи закрывает паузу целиком
      else if (pauseSub >= 0) { cheatMenuFrame(pad.Buttons, hit, &hit, 1, pauseSub == PM_OPTIONS); pauseSubInput(hit); }       // OPTIONS паузы (pmenu.shr): Sound / Settings; №1104: коды оригинала на верхнем экране
#ifndef SUBSTART_OLD
#define SUBSTART_OLD 0   // сверка g10b-loop круг 2 (g07b P13:7): 1 — START на Level Progress и View Cards возвращает в меню паузы (прежнее)
#endif
      // круг 2 (g07b P13:7): START на подэкранах паузы закрывает паузу целиком (○ и △ — назад в меню паузы)
      else if (lsOpen) { if (!SUBSTART_OLD && (hit & PSP_CTRL_START) && !(hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE))) { lsOpen = 0; uiPopHookFire(UI_EV_BACK); uiPause(0); }
                         else if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | PSP_CTRL_START)) { lsOpen = 0; uiPopHookFire(UI_EV_BACK); } }   // LEVEL PROGRESS: Back
      else if (vcOpen) { if (!SUBSTART_OLD && (hit & PSP_CTRL_START) && !(hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) && !vcZoom) { uiPopHookFire(UI_EV_BACK); uiPause(0); } else vcInput(hit); }   // VIEW CARDS (pextra.shr)
      else if (ms == 2) { zoneReloadWorld("msel"); SCR_CLOSE(); uiPause(0);   // сверка g09b-menu круг 5 (P12-03): выбор миссии — зоны заново
         if (!MS_NEWS_OLD) msNewsPend = MS_NEWS_WAIT + 1;   // миссия выбрана: в игру (№893: газета всегда)
        if (!msFlowOld) { msSetupWait = 0; msLateWin = 0; msMusPend = 1; sndMusicRelease(); sndMusic(SND_MUS_OFF);   // №1070: музыка прежней миссии и места — сразу
          if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "MSFLOW1070 select t=%.1f\n", smkT); smkWrite(s_); } } }
      else if (ms == 4) { SCR_CLOSE(); uiPause(0); }   // сверка g07b-msnview P13-7: START на MISSION SELECT — выход из паузы в игру целиком (мир без выбора миссии)
      else if (ms == 3) { int lv, row; SCR_CLOSE(); uiPause(0);   // правка 348: миссия другого уровня — тот уровень (ниже, levelAdvance)
                          if (mvMissionSelectWant(&lv, &row) && msLevelOpen(lv) && !(!SETCHECK_OLD && !levelAvailable(lv) && (setLevelMissingScreen(lv), 1))) { toLevel = lv; msPendRow = row; msPendDev = msDevSession; } }
      else if (!ms) { act = uiPauseInput(hit); if (act == UI_P_MISSION_SELECT) { char fn[256]; pausePackFile(fn, "pextra.shr");
                        // правка 348: экран MissionSelect.scr на всех уровнях (строки всех 7 уровней в пакете); прежнее — только уровень 1
                        if (gameLevel == 1 || !mvMissionSelectOld()) msPackOpen(fn);
                        mvMissionSelectSetup(msLevelOpen, msDevSession); mvMissionSelectRows(msLevelRows);
                        if (!mvMissionSelect(1, 0)) SCR_CLOSE(); }   // MissionSelect.scr (pextra.shr, экран 1)
                      if (act == UI_P_OPTIONS) pauseSubOpen();
                      if (act == UI_P_LEVEL_PROGRESS) { lsOpen = 1; levelStatsLog("pause"); }
                      if (act == UI_P_VIEW_CARDS) vcStart();
                      if (act == UI_P_SAVE_GAME) savePauseOpen(0); }
      if (!uiPaused()) { lsOpen = 0; if (vcOpen) { SCR_CLOSE(); vcOpen = 0; } }
      if (smkOn && act != UI_P_NONE) { char s[48]; snprintf(s, sizeof s, "PAUSEACT act=%d t=%.1f\n", act, smkT); smkWrite(s); }   // правка 137
      if (act == UI_P_RESET_CAR) resetCar = 1;
      else if ((act == UI_P_RESTART_MISSION || act == UI_P_ABORT_MISSION) && !PAUSE_CONFIRM_OLD && svmOn() && savePauseOpen(act == UI_P_RESTART_MISSION ? 4 : 8) == 0) pauseAskAct = act;   // сверка g09a-hud круг 5 (M2:Д6): сначала «Yes / No», пауза остаётся
      else if (act == UI_P_RESTART_MISSION) { if (!PAUSE_CONFIRM_OLD) uiPauseCloseNow(); mvPauseRestart(); chase.init = 0; }   // PauseMission.scr (29.09)
      else if (act == UI_P_ABORT_MISSION) { if (!PAUSE_CONFIRM_OLD) uiPauseCloseNow(); mvPauseAbort(); chase.init = 0; hnrAbortMsn = 1; }   // сверка g09b-menu круг 3: отмена миссии — не победа (шкала Hit & Run)
      else if (act == UI_P_QUIT_GAME && !(svmOn() && (mvFreeRoam() || svmQuitAsks()) && savePauseOpen(svmQuitAsk(mvFreeRoam())) == 0)) {   // «Do you want to save your game?»; сверка g09b-menu (М2 Д2, P13-1): сначала «Are you sure you want to quit?» в обеих паузах
        if (mainNew >= 0) toMenu = 1; else { blackBox(1); exitSafeNow(); } }   // 12а: в главное меню
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (mvLevelEnd()) {                     // конец уровня (LevelEnd.scr): мир стоит, X — дальше (следующий уровень)
      if (!lsEndLogged) { levelStatsLog("end"); lsEndLogged = 1; lsEndT = 0.f; }
      lsEndT += smkShot[0] ? 0.f : wallDt;   // служебный запуск: экран стоит 3 с (как прежняя вспышка) и уходит сам — гейты уровней идут без X;
                                             // 30.09: пока сценарий ждёт снимок окна (под нагрузкой секунды), 3 с не идут — иначе X сценария опаздывал
      if (svcMode && lsEndT >= 3.f) hit |= PSP_CTRL_CROSS;
      mvLevelEndInput(hit); if (!mvLevelEnd()) lsEndLogged = 0; if (hit & PSP_CTRL_CROSS) uiPopHookFire(UI_EV_ACCEPT);
#ifndef LVEND_LOGOS_OLD
#define LVEND_LOGOS_OLD 0   // №1199 (слово Роба): 1 — логотипы 20th Century Fox и Gracie Films после конца уровня 1, как до правки (правка 54); 0 — не играть
#endif
      // №1199: между уровнями логотипы не играют. Межуровневые ролики данных (SetFMVInfo последней миссии уровня: fmv на L2, L5, L6, L7 и др.) остаются — их ведёт миссия, не этот код.
      // Для проверки на железе: файл lvlogoson рядом с EBOOT или «flag lvlogoson 1» — логотипы как у игрока TEST88 (№1192, зависание после роликов)
      if (!mvLevelEnd() && gameLevel == 1 && (LVEND_LOGOS_OLD ? (!svcMode || realLevelEnd >= 1) : lvLogosOn)) {   // №1192: «flag realend 1» — служебный запуск проходит конец уровня как игрок (ролики Fox и Gracie); правка 54: финал уровня 1 (запись PS2 42:45–43:10) — логотипы Fox и Gracie Films
        msnMovie("foxlogo.rmv", &fbp0); msnMovie("gracie.rmv", &fbp0); chase.init = 0; padE.init = 0; prev = smokePad(); }
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (wardrobeOn()) {                     // правка 107: гардероб — мир стоит
      wardrobeInput(mvShop(), hit); hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (phoneViewOn()) {                    // правка 94: экран телефонной будки — мир стоит, ◄ ► ✕ △ ○
      phoneViewInput(mvShop(), hit); hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (mvMsgBox(0)) {                      // правка 122: окно сообщения (Message.scr) — мир стоит, ✕ / ○ закрыть
      mvMsgBoxInput(hit); if (hit & PSP_CTRL_CROSS) uiPopHookFire(UI_EV_ACCEPT);
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
#ifndef PAUSE_BAN_OLD
#define PAUSE_BAN_OLD 0   // сверка g10b-loop: 1 — пауза открывается и на смене этапа, при ожидающей смене миссии и в затемнении HUD (прежнее)
#endif
#ifndef TUT_PAUSE_OLD
#define TUT_PAUSE_OLD 0   // сверка g10b-loop, P13:17: 1 — START в подсказке Барта проглатывается (прежнее)
#endif
    } else if (g10bTutPauseOpens(tutActive(), (hit & PSP_CTRL_START) != 0, uiReady, bpass, tutVoiceBusy())) {
      // сверка g10b-loop, P13:17: пока в подсказке Барта звучит реплика, START открывает паузу поверх подсказки; после паузы подсказка на месте.
      // Реплика кончилась: START ничего не делает (ветка ниже). TUT_PAUSE_OLD=1 — прежнее
      if (uiPauseMissionGrey) uiPauseMissionGrey(1);   // сверка g09a-hud круг 3 (P13:Д3): пауза поверх подсказки обучения — Restart/Abort серые
      uiPauseMission(!pauseOld && mvInMission()); pauseMsAccess(); uiPause(1); hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (tutActive()) {                      // подсказка Барта (Tutorial.scr): мир стоит, X — Continue, □ — Disable Tutorials
      int dis = 0; tutInput(hit, wallDt, &dis);
      if (dis) cfg.tutorial = 0;
      if (dis || cfg.tutSeen != tutShown()) { cfg.tutSeen = tutShown(); cfgSave(); }   // показанные — в settings.bin (RESUME не повторит)
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (mvBriefing()) {                     // пункт 8: брифинг миссии (MissionLoad) — мир стоит, X / △
      mvBriefingInput(hit); hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__;
    } else if (mvInput(&hit)) {                    // пункт 8: провал миссии — MissionOver (YES/NO, X), мир стоит
      pad.Buttons = 0; pad.Lx = pad.Ly = 128; dt = 0.f; dtBr1258 = __LINE__; transitDropModal1259();   // Будка-93 (1259): экран провала не под закрытым ирисом
    } else if (!LB_START_SKIP_OLD && (hit & PSP_CTRL_START) && (mcPlay || mvTalkActive() || (!BOOTH_CAM_SKIP_OLD && (phoneAnimT >= 0.f || phoneCamT >= 0.f))
#if !TICKET_CONTENT_C3_OLD
               || ticketViewBusy(&ticketView)
#endif
               )) {
      // Роб 17.10: пока чёрные полосы (облёт / камера старта миссии, разговор, квитанция) — START сначала ПРОПУСКАЕТ этот момент, как у роликов
      // и заставок; меню паузы — только когда полос уже нет (было: START сразу открывал паузу поверх облёта). LB_START_SKIP_OLD=1 — прежнее
      if (mcPlay) { if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "MSNCAM skip start t=%.1f\n", smkT); smkWrite(s_); if (mcPlay == &mcSyn96) { snprintf(s_, sizeof s_, "MSNCAM done t=%.2f skip=1\n", smkT); smkWrite(s_); } } mcPlay = 0; chase.init = 0; }
      else if (!BOOTH_CAM_SKIP_OLD && (phoneAnimT >= 0.f || phoneCamT >= 0.f)) {   // №792: перебивка будки — START пропускает (камера сразу за игроком)
        if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PHONECAM skip start t=%.1f\n", smkT); smkWrite(s_); } phoneAnimT = -1.f; phoneCamT = -1.f; chase.init = 0; }
      else if (mvTalkActive()) { mvTalkInput(PSP_CTRL_CROSS); if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "TALK skip start t=%.1f\n", smkT); smkWrite(s_); } }
#if !TICKET_CONTENT_C3_OLD
      else ticketViewSkip(&ticketView);
#endif
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128;
    } else if (!(hit & PSP_CTRL_START) && mvTalkInput(hit)) {   // правка 40: разговор — X пропустить / начать миссию, △ отменить (LetterBox.scr)
      hit = 0; pad.Buttons &= PSP_CTRL_START; pad.Lx = pad.Ly = 128;
    } else if ((hit & PSP_CTRL_START) && g10bPauseBanned(mvFadeAlpha(), toLevel, toMenu, msNewsPend, msPendRow)) {
      // сверка g10b-loop, M2:Д5, P13:12, P17-3: пауза запрещена, пока идёт смена этапа миссии (затемнение, ирис, чёрный экран), ждёт смена миссии
      // вперёд или назад (выбор MISSION SELECT, новый уровень) и пока HUD в затемнении или проявлении; START в эти кадры теряется. Было: пауза
      // открывалась в любой из них. PAUSE_BAN_OLD=1 — прежнее
      hit &= ~PSP_CTRL_START; pad.Buttons &= ~PSP_CTRL_START;
    } else if (hit & PSP_CTRL_START) {
      if (uiPauseMissionGrey) uiPauseMissionGrey(mvLevelEnd() || G.hnrBusted > 0.f || bustFadeT >= 0.f || (haveHomer && (homer.state == HS_GET_IN || homer.state == HS_GET_OUT || homer.state == HS_TO_DOOR)) || tutActive());   // круг 2 (g09a P13:Д3): Restart/Abort серые — миссия завершена, арест, посадка/высадка, пауза поверх обучения
      if (uiReady && !bpass) { uiPauseMission(!pauseOld && mvInMission()); pauseMsAccess(); uiPause(1); svmPauseEnter(); } else resetCar = 1;   // svmPauseEnter — сверка g09b-menu (P13-Д4)   // без ui.shr START возвращает машину на колёса
      if (smkOn && uiReady && uiPaused()) { char s[64]; snprintf(s, sizeof s, "PAUSE on mission=%d t=%.1f\n", uiPauseIsMission(), smkT); smkWrite(s);
        int pm = uiPauseIsMission() ? UI_SCREEN_PAUSEMSN : UI_MENU_PAUSE; char r[256]; int n = snprintf(r, sizeof r, "PAUSEROWS");   // правка 137: видимые строки
        for (int i = 0; i < uiMenuCount(pm) && n < (int)sizeof r - 40; i++) if (!uiMenuHidden(pm, i)) n += snprintf(r + n, sizeof r - n, " %d:%s |", i, uiMenuLabel(pm, i));
        snprintf(r + n, sizeof r - n, "\n"); smkWrite(r);
        { void mvSaveWhy(char *, int); int mvCanSave(void); char w[128]; mvSaveWhy(w, sizeof w); char q[176]; snprintf(q, sizeof q, "PAUSESAVE can=%d %s\n", mvCanSave(), w); smkWrite(q); } }   // правка 330: почему SAVE GAME серый
      hit &= ~PSP_CTRL_START;
    }
    // Правка Роба 60: подсказка не стопорит облёт к дому — на старте уровня облёт запрашивается через кадр-другой (m0sd, 0.1 с), а
    // «ходьба» срабатывала раньше и замораживала его. Правило порта (запись PS2: облёт 0:58–1:03, подсказка 1:05): подсказки ждут
    // TUT_CALM секунд свободной игры подряд (без облёта, разговора, брифинга, паузы)
    // сверка g10b-loop, P10-23: реплика «устал» (Turbo) — через 5 с от начала турбо (○ зажат, пешком, не в прыжке, не в помещении), если Гомер всё ещё в турбо; вышел раньше — таймер сброшен.
    // TURBO_TIRED_OLD=1 — прежнее (реплика на первом шаге бега, main_state.h)
    { static float turboT; static int turboSaid;
      if (g10bTurboStep(&turboT, &turboSaid, haveHomer && onFoot() && !fly && (pad.Buttons & PSP_CTRL_CIRCLE) && homer.speed > HOMER_WALK + .5f && homer.grounded && !inRoom(), dt)) sndSay(SND_SAY_TURBO, 0); }
    if (svdataLightCount() && rs.room < 0) {   // сверка g12-data круг 6 (P12-08): объёмы «смена света» — множитель солнца для персонажей на улице (viewer/svdata.c); SVLIGHT_OLD=1 — объёмы не читаются
        float lp_[3] = {onFoot() ? homer.x : car.x, onFoot() ? homer.y : car.y, onFoot() ? homer.z : car.z}, lm_[3];
        if (svLightStep(lp_, dt, lm_)) { float a_[3], d_[3]; for (int k_ = 0; k_ < 3; k_++) { a_[k_] = (lvlLightOk == 1 ? lvlLight.ambS[k_] : 1.f) * lm_[k_]; d_[k_] = (lvlLightOk == 1 ? lvlLight.dirS[k_] : 1.f) * lm_[k_]; } skinLightLevel(a_, d_, 0, 0); }
      }
    int tutFree;
    { const Shop *sh = mvShop();   // сверка 30.09 (запись PS2 14:17): игроку досталась машина (куплена, награда) — подсказка «UNLOCKED CAR»
      if (!sh) ownInit = 0;
      else { uint32_t m = shopOwnedMask(sh), nw = ownInit ? m & ~ownSeen : 0; ownSeen = m; ownInit = 1;
        for (int i = 0; nw && i < sh->nitem && i < 32; i++) if ((nw >> i & 1u) && sh->item[i].kind == SHOP_CAR) { tutEvent(TUT_UNLOCKED); break; } } }
    { int free = uiReady && !uiPaused() && !mcPlay && !mvTalkActive() && !mvBriefing() && !mvLevelEnd() && !mvFrozen();
      tutCalm = free && !tutCalmOff ? tutCalm + dt : (free ? TUT_CALM : 0.f); tutFree = free && tutCalm >= TUT_CALM;
      tutSetBusy(!tutFree); }   // разговор, брифинг, облёт, пауза: подсказка события ждёт в очереди
    if (tutFree && rs.room < 0) {
#ifndef TUTWALK_FLY_OLD
#define TUTWALK_FLY_OLD 0   // сверка g10b-loop круг 2 (g07b P15:Д4): 1 — подсказка про ходьбу только когда Гомер пешком (прежнее)
#endif
      if (onFoot() || !TUTWALK_FLY_OLD) tutEvent(TUT_WALK);   // круг 2: стартовая подсказка про ходьбу ставится в очередь по концу первого облёта (tutFree ждёт конца облёта), без условия «пешком»   // подсказка «ходьба» — как только Гомер свободен пешком (PS2 1:05, после облёта улицы)
      { float px = onFoot() ? homer.x : car.x, pz = onFoot() ? homer.z : car.z; int g = mvGiverNear(px, pz, TUT_GIVER_M);
        if (g == UI_R_STREETRACE) tutEvent(TUT_RACE); else if (g == UI_R_BONUS) tutEvent(TUT_BONUS);   // PS2 16:55, 22:55
        else if (g == UI_R_WAGER) tutEvent(TUT_TIMETRIAL); }
      tutTick(dt);   // отложенные и очередные подсказки (игровое время: пауза и подсказка его не двигают)
    }
#ifndef ENDING_SAVE860_OLD
#define ENDING_SAVE860_OLD 0   // 23.10 ВКЛЮЧЕНО: финал L7 в эмуляторе идёт до конца (титры, меню), зависание 22.10 ушло с правками кадра; 1 — как было (после SetGameOver эмулятор вставал и в диалоге записи s862g, и в титрах
                               // ending.shr (et_*, s862c). 0 — запись после финала + титры путём VIEW CREDITS в menuRun (creditsRun). См. ПЕРЕДАЧА 22.10
#endif
    { int ld = mvLevelDone();   // уровень пройден: следующий уровень; SetGameOver — финальные титры, затем главное меню
      static int endSaveAsk;   // №860: после титров открыт диалог записи («Do you want to save your game?»); закрылся — в главное меню
      if (endSaveAsk && !svmActive()) { endSaveAsk = 0; uiPauseMenuHide(0); toMenu = 1; if (smkOn) smkWrite("ENDSAVE closed\n"); }
#ifndef LV_SAVEASK_OLD
#define LV_SAVEASK_OLD 0   // сверка g10b-loop, P13:4: 1 — следующий уровень сразу по пройденному, без вопроса «сохранить?» (прежнее)
#endif
      // сверка g10b-loop, P13:4: ✕ на экране конца уровня ведёт на следующий уровень, но сперва игра спрашивает «Do you want to save your game?» (да — запись,
      // нет — дальше). Закрылось любым ответом — следующий уровень (в меню этот вопрос не выходит). Служебные запуски и туры (smkOn, svcMode) не спрашивают
      if (lvSaveAsk && !svmActive()) { toLevel = lvSaveAsk; lvSaveAsk = 0; uiPauseMenuHide(0); uiPause(0); if (smkOn) smkWrite("LVSAVEASK closed\n"); }
      if (ld && smkOn) { char s[80]; snprintf(s, sizeof s, "LEVELDONE n=%d code=%d next=%d t=%.1f\n", gameLevel, ld, levelAvailable(gameLevel + 1), smkT); smkWrite(s); }
      if (ld == 1 && !SETCHECK_OLD && gameLevel < 7 && !levelAvailable(gameLevel + 1)) setLevelMissingScreen(gameLevel + 1);   // TEST93 C3: следующего уровня нет — честный экран, потом, как прежде, меню
      if (ld == 1 && levelAvailable(gameLevel + 1)) {
        if (g10bLevelAsk(mainNew, smkOn && realLevelEnd < 2, svcMode && realLevelEnd < 2, svmOn(), uiReady, bpass)) {   // №1192: «flag realend 2» — и вопрос «сохранить?» как у игрока
          uiPause(1); uiPauseMenuHide(1);
          lvSaveNext1234 = gameLevel + 1; int sp_ = savePauseOpen(1); lvSaveNext1234 = 0;   // Будка-93 (1234): запись с вопроса хранит уровень N+1
          if (sp_ == 0) { lvSaveAsk = gameLevel + 1; if (smkOn) smkWrite("LVSAVEASK open\n"); } else { uiPauseMenuHide(0); uiPause(0); toLevel = gameLevel + 1; }
        } else toLevel = gameLevel + 1;
      }
#ifndef C3_ENDING_OLD
      else if (ld == 2) {
        char ef[256]; sibling(ef, "ending.shr");
        // №860 (Роб 20.10 и 22.10: «в сейве пропала доступность пройденного 7 уровня»): после финальной миссии титры шли прямо из игры и
        // сразу меню — записи не было, пройденный 7-й уровень не попадал в сейв; в эмуляторе мини-цикл титров поверх уровня ещё и вставал
        // (главный поток не получал процессор с 8-го кадра). Теперь: сначала диалог записи с пройденным уровнем (как QUIT GAME), потом
        // главное меню, и титры — в menuRun, когда уровня в памяти уже нет. ENDING_SAVE860_OLD=1 — как было
        if (!ENDING_SAVE860_OLD && mainNew >= 0) {
          endingPending = 1; mvMarkStoryDone();   // №860: в записи L7 пройден целиком
          if (svmOn() && uiReady) { uiPause(1); svmPauseEnter(); uiPauseMenuHide(1);   // svmPauseEnter — сверка g09b-menu
            if (savePauseOpen(1) == 0) endSaveAsk = 1; else { uiPauseMenuHide(0); uiPause(0); toMenu = 1; } }
          else toMenu = 1;
          if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "ENDSAVE open=%d t=%.1f\n", endSaveAsk, smkT); smkWrite(s_); }
        } else {
          endingRun(ef, list, resetState, swapBuf, svmDialogFrame, smkWrite, &fbp0);
          toMenu = 1;
        }
      }
#endif
      else if (ld && mainNew >= 0) toMenu = 1; }   // OLD/нет следующего уровня: прежний возврат в главное меню
    if (toLevel && !toMenu) {   // следующий уровень: газета, выгрузка прошлого, набор нового, старт его первой миссии
      int n = toLevel; toLevel = 0; levelAdvance(n, &fbp0);
      if (msPendRow >= 0) { mvMissionSelectStartRow(msPendRow); msPendRow = -1; chase.init = 0; }   // правка 348: миссия строки MISSION SELECT
      hit = 0; pad.Buttons = 0; padE.init = 0; pad.Lx = pad.Ly = 128; dt = wallDt = 0.f;
      last = t0 = sceKernelGetSystemTimeLow(); smkSettle(); e1002N = 14;
      if (haveCol) { x = car.x; y = car.y; z = car.z; h = car.h; } else { x = spawnP[0]; y = spawnP[1]; z = spawnP[2]; h = spawnP[3]; }
      v = vy = 0; fly = 0; camx = x - sinf(h) * 7.5f; camy = y + 3.2f; camz = z - cosf(h) * 7.5f;
    }
    toLevel = 0;
    if (toMenu) {   // 12а: главное меню -> NEW GAME -> загрузка со шкалой; игра заново со спавна
      if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }   // В5: жужжание осы не звучит в меню
      gameCheatsReset();   // №1104: выход в главное меню из игры — включённые коды сбрасываются (как в оригинале)
      toMenu = 0; msPendRow = -1; msPendDev = 0; msNewsPend = 0;   // правка 348: выбор MISSION SELECT не переживает выход в меню
      if (!MENU_SNAP_OLD) { if (endingPending) menuSnapOn = 0; else if (haveCol) { saveCapture(&menuSnap); menuSnapOn = 1; } }   // игра пройдена (M2:Д8 — сброс на первую миссию первого уровня не сделан): снимок не держим, RESUME — по записи   // сверка g10b-loop, P14:1: игра идёт — снимок для RESUME GAME после выхода в меню
      menuNoGame = 1; menuRun(&fbp0); menuNoGame = 0; hit = 0; pad.Buttons = 0; padE.init = 0; pad.Lx = pad.Ly = 128; dt = wallDt = 0.f;
      last = t0 = sceKernelGetSystemTimeLow(); smkSettle();
      if (haveCol) { x = car.x; y = car.y; z = car.z; h = car.h; } else { x = spawnP[0]; y = spawnP[1]; z = spawnP[2]; h = spawnP[3]; }
      v = vy = 0; fly = 0; camx = x - sinf(h) * 7.5f; camy = y + 3.2f; camz = z - cosf(h) * 7.5f;
    }
#if !TICKET_CONTENT_C3_OLD
    if(ticketViewBusy(&ticketView) && (hit&PSP_CTRL_START)){ticketViewSkip(&ticketView);hit&=~PSP_CTRL_START;pad.Buttons&=~PSP_CTRL_START;}
#endif
    if (mvFrozen() || G.hnrBusted > 0.f || bustFadeT >= 0.f || (!TICKET_CONTENT_C3_OLD&&TICKET_BUSY())) { hit &= PSP_CTRL_START; pad.Buttons &= PSP_CTRL_START; pad.Lx = pad.Ly = 128; }   // пункт 8: отсчёт 3-2-1 — игрок стоит; правка 112: арест — заморозка на таймер ареста (1,48 с, №430)
    if (mcPlay) {   // правка 40: камера старта миссии — игрок стоит; X / START — пропустить; №792: и SELECT
      if (hit & (PSP_CTRL_CROSS | PSP_CTRL_START | (BOOTH_CAM_SKIP_OLD ? 0 : PSP_CTRL_SELECT))) { if (smkOn) { char s[64]; snprintf(s, sizeof s, "MSNCAM skip t=%.1f\n", smkT); smkWrite(s); if (mcPlay == &mcSyn96) { snprintf(s, sizeof s, "MSNCAM done t=%.2f skip=1\n", smkT); smkWrite(s); } } mcPlay = 0; chase.init = 0; }
      hit = 0; pad.Buttons = 0; pad.Lx = pad.Ly = 128; }
    if (!BOOTH_CAM_SKIP_OLD && (phoneAnimT >= 0.f || phoneCamT >= 0.f) && (hit & (PSP_CTRL_CROSS | PSP_CTRL_SELECT))) {   // №792: перебивка будки — ✕ / SELECT пропускают (не прыжок и не смена вида)
      if (smkOn) { char s[64]; snprintf(s, sizeof s, "PHONECAM skip t=%.1f\n", smkT); smkWrite(s); }
      phoneAnimT = -1.f; phoneCamT = -1.f; chase.init = 0; hit &= ~(PSP_CTRL_CROSS | PSP_CTRL_SELECT); pad.Buttons &= ~(PSP_CTRL_CROSS | PSP_CTRL_SELECT); }
    // №792 (Роб 18.10: «так же, когда выхожу из машины»): переход камеры после выхода из машины (и прочие переходы camBT пешком) — ✕ / SELECT сразу ставят камеру
    // за игроком (нажатие не идёт в прыжок и смену вида)
    if (!BOOTH_CAM_SKIP_OLD && camBT >= 0.f && onFoot() && (hit & (PSP_CTRL_CROSS | PSP_CTRL_SELECT))) {
      if (smkOn) { char s[64]; snprintf(s, sizeof s, "CAMBLEND skip t=%.1f\n", smkT); smkWrite(s); }
      camBT = -1.f; hit &= ~(PSP_CTRL_CROSS | PSP_CTRL_SELECT); pad.Buttons &= ~(PSP_CTRL_CROSS | PSP_CTRL_SELECT); }
    // №704 (Роб 17.10: «когда камера крутится — выход из машины, из будки, наезды — управление неизменным, как от исходного вида; начинаешь идти, камера дёргается, и направление
    // ходьбы меняется вместе с ней»): пока камера занята (переход, облёт будки, наезд миссии, разговор) или поворачивается у стоящего Гомера (> 6°/с), «вперёд» стика
    // берётся от вида ДО перехода (замороженный курс homerCtrlYaw); новый вид — когда камера встала (стик в центре или удержан: у удержанного курс уже взят). Предел 2,5 с.
    // Идущему Гомеру поворот камеры за курсом ходьбы не мешает (скорость > 0,5). HOMER_CTRL_FREEZE_OLD=1 / flag ctrlfreezeold 1 — как было
    { static float ctrlHeld, ctrlPrev, ctrlBusyT; static int ctrlInit;
      float vx_ = (onFoot() ? homer.x : car.x) - camx, vz_ = (onFoot() ? homer.z : car.z) - camz;   // вид на экране: от глаза к герою (за рулём — вид камеры машины)
      float cy_ = vx_ * vx_ + vz_ * vz_ > 1e-4f ? atan2f(vx_, vz_) : homer.camYaw, dty = dt > 1e-4f ? fabsf(wrapPi(cy_ - ctrlPrev)) / dt : 0.f; ctrlPrev = cy_;
      // №947б (Роб 25.10: «курс держим только когда ракурс меняет сама игра»): перескок камеры у стены — решение игры; 1,5 с после него кадр управления остаётся прежним (homerFlipHold): стик, тронутый заново после короткого нуля, и смена угла вид не берут.
      // Повороты камеры самим игроком (L/R) и ход камеры за курсом Гомера курс не держат. HOMER_FLIPBUSY_OLD=1 / flag ctrl947old 1 — без заморозки
      static int flipNPrev; static float flipBusyT; if (homerCamFlipN != flipNPrev) { flipNPrev = homerCamFlipN; flipBusyT = homerFlipBusyOld ? 0.f : 1.5f;
        if (smkOn) { char l_[80]; snprintf(l_, sizeof l_, "CAMFLIP t=%.2f n=%d\n", smkT, homerCamFlipN); smkWrite(l_); } } else if (flipBusyT > 0.f) flipBusyT -= dt;
      homerFlipHold = flipBusyT > 0.f;   // steer: стик, тронутый заново в эти 1,5 с, вид не берёт — кадр остаётся прежним
      int busy_ = flipBusyT > 0.f || camBT >= 0.f || mcPlay || phoneAnimT >= 0.f || phoneCamOn || talkCamOn || (dty > .105f && homer.speed < .5f);
      homerViewOk = onFoot();   // №1059: homerViewYaw ставит конец кадра (передний вектор камеры)
      if (!ctrlInit || !onFoot() || homerCtrlFreezeOld) { ctrlHeld = cy_; ctrlInit = 1; ctrlBusyT = 0.f; }
      else if (busy_ && ctrlBusyT < 2.5f) ctrlBusyT += dt;
      else { ctrlHeld = cy_; if (!busy_) ctrlBusyT = 0.f; }
      // №704 (дирижёр: «камера дёргается при выходе, и направление меняется вместе с ней; хочу как от нашего исходного вида»): с момента выхода из машины и пока Гомер не пошёл
      // «вперёд» стика — от вида камеры за машиной до выхода (курс chase.yaw), какие бы повороты камеры пешком ни были (обход кузова, подъезд, разворот у стены). PS2: после
      // выхода камера держит курс машины (<= 15° за 1,6 с, Стенд2). HOMER_EXITYAW_OLD=1 / flag exityawold 1 — как было
      { static int exPrevFoot = -1, exOn; static float exYaw, exT, chLast; int foot_ = onFoot();
        if (!foot_) chLast = chase.yaw;
        if (exPrevFoot == 0 && foot_ && !homerExitYawOld && (EXITYAW_ANY_OLD || homer.state == HS_GET_OUT)) { exOn = 1; exYaw = chLast; exT = 0.f; }   // проверка 18.10: только настоящий выход (из машины в HS_GET_OUT); перенос Гомера пешком (homerPlace) ставит HS_FOOT прямо из машины — там курс от нового вида
        exPrevFoot = foot_;
        if (exOn) { exT += dt; if (!foot_ || exT > 4.f || homer.speed > .5f) exOn = 0; }
        if (exOn) { homerCtrlYaw = exYaw; homerCtrlOn = 1; if (smkOn && camLogOn) { char s_[100]; snprintf(s_, sizeof s_, "EXITYAW t=%.2f yaw=%.0f camYaw=%.0f\n", smkT, exYaw * 57.2958f, homer.camYaw * 57.2958f); smkWrite(s_); } }
        else { homerCtrlYaw = ctrlHeld; homerCtrlOn = !homerCtrlFreezeOld && ctrlBusyT > 0.f; }
        homerCamBusy = busy_ || exOn; }   // №836: камера в переходе — курс управления не перехватывается
      if (smkOn && benchLog1034 && onFoot()) { char s_[100]; snprintf(s_, sizeof s_, "BENCH1034 t=%.2f x=%.2f y=%.2f z=%.2f g=%d\n", smkT, homer.x, homer.y, homer.z, homer.grounded); smkWrite(s_); }   // №1034: диагностика (flag benchlog 1): где Гомер каждый кадр
      if (smkOn && camLogOn && onFoot()) { char s_[260]; snprintf(s_, sizeof s_, "CTRLYAW t=%.2f held=%.0f view=%.0f camYaw=%.0f busy=%d bt=%.2f spd=%.1f on=%d frame=%.0f walk=%.0f pos=%.2f,%.2f\n", smkT, ctrlHeld * 57.2958f, cy_ * 57.2958f, homer.camYaw * 57.2958f, busy_, ctrlBusyT, homer.speed, homerCtrlOn, homer.frame * 57.2958f, atan2f(homer.vx, homer.vz) * 57.2958f, homer.x, homer.z); smkWrite(s_); } }
    float ax = (pad.Lx - 128) / 128.f, ay = (pad.Ly - 128) / 128.f;
    if (GAME_STICK_OLD) { if (fabsf(ax) < .2f) ax = 0; if (fabsf(ay) < .2f) ay = 0; }
    else gameStick(ax, ay, &ax, &ay);   // сверка g10-flow P17-9: зона 0,25 по оси с пересчётом в 0..1 и ступень 0,05 (viewer/game.c)
    ccStX = ccStY = 0.f; ccStDown = (pad.Buttons & PSP_CTRL_DOWN) != 0 ? 1.f : 0.f;
    if (!CHEATS1104_OLD && ccStickView(chaseView) && gameCheatOn(CH_CAMERAS) && !onFoot()) { ccStX = ax; ccStY = -ay; ax = 0.f; ay = 0.f; }   // №1119: в видах comedy, kull, tracker, debug аналог ведёт камеру, руль — крестовина
    homerCamLR = 0; homerCamLRx = homerCamLRy = 0.f;   // №1231: пешком L+R вместе — стик вращает камеру вокруг Гомера, бег выключен; одиночные L и R — как раньше (правка 75, ниже)
    if (camLR1231 && haveHomer && onFoot() && !fly && !inRoom() && !mvTalkActive() && (pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) == (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
      homerCamLR = 1; homerCamLRx = ax; homerCamLRy = ay; ax = 0.f; ay = 0.f; }
    if (mvTalkActive()) gameHnrFreeze(&G, .1f);   // сверка g10-flow P03-07: в разговоре нарушения Hit & Run не считаются (спад стоит — отдельно, HNR_LEVEL_DECAY_OLD g10b-loop)
    if (autodrive == 5 && haveHomer && onFoot() && haveTraffic) {   // TEST14 §5: Гомер по путям пешеходов (viewer/smkpilot.h), бегом; стик — от камеры, как у бота
      float tx_, tz_; adT += dt;
      if (smkWalkStep(&smkWalk, &traffic, homer.x, homer.z, dt, &tx_, &tz_)) { float fr_ = !homerCtrl916Old && (homer.heldX != 0.f || homer.heldY != 0.f) ? homer.frame : homer.camYaw;   // №916: стик зажат — кадр управления held, а не нынешний вид
        float r_ = atan2f(tx_ - homer.x, tz_ - homer.z) - fr_; ax = -sinf(r_); ay = -cosf(r_); pad.Buttons |= PSP_CTRL_CIRCLE; }
      if ((smkPilotLogT += dt) >= 1.f) { smkPilotLogT = 0.f; char s_[128]; snprintf(s_, sizeof s_, "PILOT walk t=%.1f dist=%.0f turn=%d v=%.1f\n", smkT, smkWalk.dist, smkWalk.stuckN, homer.speed); smkWrite(s_); }
      if (adT > adStop) { char s_[128]; snprintf(s_, sizeof s_, "PILOT walk end t=%.1f dist=%.0f turn=%d\n", smkT, smkWalk.dist, smkWalk.stuckN); smkWrite(s_); autodrive = 0; }
    }
    int benchGo = 0; if (autob == 1) { benchGo = 1; autob = 2; }   // служебный замер — только из autobench, не с кнопки
    if (autob == 3) { benchStart(); bpass = 5; fbA = memalign(16, BUF_W * SCR_H * 4); memset(pcDiff, 0, sizeof pcDiff); autob = 2; }
    if (autob == 2 && bpass == 0 && brep[0][0]) sceKernelExitGame();
    int svc0 = fly * 2 + hud;
    if (hit & PSP_CTRL_SELECT) {
      if (svcMode) { if (pad.Buttons & PSP_CTRL_RTRIGGER) hud = !hud; else if (pad.Buttons & PSP_CTRL_LTRIGGER) { jitterOn = !jitterOn; jitHave = 0; } else longView = !longView; }   // L+SELECT: сглаживание полпикселя (jitter.h) вкл/выкл   // служебное: SELECT 30 FPS/250 м <-> 60 FPS/115 м; R+SELECT счётчик
      else if (SHR_REL && SHR_KEEP_HUD && (pad.Buttons & PSP_CTRL_RTRIGGER)) hud = !hud;   // релиз с оставленным счётчиком (release.h): R+SELECT игроку
#ifndef SELECT_VIEW_OLD
#define SELECT_VIEW_OLD 0   // сверка g10b-loop, СВЕРКА_КАМЕРА 1.2 / P08:15: 1 — SELECT меняет вид и под камерой зоны, и у багги бампер остаётся (прежнее)
#endif
      // сверка g10b-loop: 1.2 — у багги (dune_v) бампера нет, вместо него ближний вид; P08:15 — пока активна камера зоны, комнаты или вступления (статичная, рельсовая, анимационная),
      // кнопка выбора вида игнорируется. SELECT_VIEW_OLD=1 — прежнее
      else if (!selectCamOld && !SELECT_VIEW_OLD && (zoneCamOn || carZoneOn || roomCamOn || mcPlay) && haveCol && !onFoot() && !fly) { }
      else if (!selectCamOld && haveCol && !onFoot() && !fly) { chaseView = selectCycleOld ? !chaseView : ccViewNext(chaseView, !strcmp(car.p.name, "dune_v"), !CHEATS1104_OLD && gameCheatOn(CH_CAMERAS), g10bViewNext); /* №1104: с кодом Unlock All Cameras в цикле ещё два вида */ if (smkOn) { char s[48]; snprintf(s, sizeof s, "SVC camview %d t=%.1f\n", chaseView, smkT); smkWrite(s); } }   // №617: SELECT в машине — вид камеры
      else if (selectCamOld && (!SHR_REL || SHR_KEEP_RESET) && haveCol && !onFoot() && !fly) { resetCar = 1; if (smkOn) { char s[48]; snprintf(s, sizeof s, "SVC reset select t=%.1f\n", smkT); smkWrite(s); } }   // ControllerPS2.pag: SELECT в машине — RESET CAR
    }
    if (!svcMode && !(SHR_REL && SHR_KEEP_HUD)) hud = 0;   // правка 44: счётчик и служебные строки — только в служебном режиме (релиз с KEEP_HUD: и у игрока)
    if (benchGo && !onFoot()) {   // служебный тест-прогон (autobench); ○ в машине игроку не отдаём на служебное (Роб 28.09: «ломается»)
      if (bpass || brep[0][0]) { bpass = 0; brep[0][0] = 0; opt = optUser; }
      else { benchStart(); fly = 0; }
    }
    int flyChord = svcMode && (pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) == (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER);   // правка 44: у игрока L+R+△ — обычный △
    if ((hit & PSP_CTRL_TRIANGLE) && smkOn && inRoom() && wardrobeNear(homer.x, homer.y, homer.z)) {   // правка 107: кто забирает △ у места костюмов
      char s[128]; snprintf(s, sizeof s, "WARDV near msn=%d coll=%d ao=%d t=%.1f\n", mvWantsAction(), haveColl ? collectWantsAction(&coll) : -1, haveAo ? aoWantsAction(&ao) : -1, smkT); smkWrite(s); }
#ifndef ACTION_RANK_OLD
#define ACTION_RANK_OLD 0   // сверка g10b-loop, P05:2 / P05:19: 1 — цепочка △ как была (своя машина последней, без паузы между срабатываниями)
#endif
    // сверка g10b-loop, P05:19: между двумя срабатываниями кнопки действия проходит не меньше 4 кадров (счёт от последнего сработавшего); нажатие внутри паузы пропадает
    static float triCool; unsigned tri0_ = hit & PSP_CTRL_TRIANGLE;
    if (!ACTION_RANK_OLD) { if (!g10bTriStep(&triCool, 1, dt)) { hit &= ~PSP_CTRL_TRIANGLE; tri0_ = 0; } }
    int msnTri = (hit & PSP_CTRL_TRIANGLE) && !fly && !flyChord && mvWantsAction();   // △ у собеседника/точки миссии — миссии, не в машину
    if (msnTri) hit &= ~PSP_CTRL_TRIANGLE;
    // сверка g10b-loop, P05:2: ранг кнопки действия (меньше = важнее): разговор/разрушаемое миссии 0, своя (последняя) машина 1, будка 2, дверь интерьера 3, выключатель/гэг 4,
    // покупки 5–6, чужая машина 7. Было: своя машина проигрывала выключателю, гардеробу, двери комнаты и будке. Теперь, если рядом своя машина и в неё можно сесть,
    // △ идёт ей до гэг-объектов, звонка, выключателя, гардероба, двери и будки. ACTION_RANK_OLD=1 — прежняя цепочка
    if (!ACTION_RANK_OLD && (hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord && onFoot() && !inRoom()) {
      HomerCar hc_; memset(&hc_, 0, sizeof hc_); homerCarFrom(&hc_, &car, cv.lift);
      if (homerCanEnter(&homer, &hc_)) { gameTriangle(&G); hit &= ~PSP_CTRL_TRIANGLE; } }
    // круг 3 (проверка g10b-loop, P05:2): дальше по рангу — телефонная будка (2), дверь интерьера (3), потом гэг-объект и выключатель (4), покупки (5–6), чужая машина (7).
    // Было: будка и дверь шли после гэга, выключателя и гардероба, дверь раньше будки. ACTION_RANK_OLD=1 — прежний порядок цепочки
    if (!ACTION_RANK_OLD) {
#include "main_actbooth.h"
      if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && !fly && !flyChord && onFoot() && roomsDoorNear(&rooms, &rs, &G)) { rs.action = 1; hit &= ~PSP_CTRL_TRIANGLE; }
    }
    if ((hit & PSP_CTRL_TRIANGLE) && haveColl && !fly && !flyChord && onFoot() && collectWantsAction(&coll) >= 0) {   // В5: △ у action-гэга — гэгу
      G.collAction = 1; hit &= ~PSP_CTRL_TRIANGLE; }
    G.collRoom = haveColl ? collectRoomId(roomName()) + 1 : 0;   // В5: гэги комнат — только в своей комнате
#ifndef SND_VO_EXTRA_OLD
    if ((hit & PSP_CTRL_TRIANGLE) && onFoot() && !fly && !flyChord) {
      int bell = sndDoorbellNear(homer.x, homer.y + .9f, homer.z);
      if (bell >= 0) { sndDoorbellSay(bell); hit &= ~PSP_CTRL_TRIANGLE; }
    }
#endif
#ifndef SVTELEPORT_OLD
#define SVTELEPORT_OLD 0   // сверка g12-data круг 7 (P18-12): 1 — площадок переброса Teleport (L3, L6) нет
#endif
    if ((hit & PSP_CTRL_TRIANGLE) && !SVTELEPORT_OLD && svTeleportCount() && haveHomer && haveCol && !fly && !flyChord && onFoot() && !inRoom() && uiReady && !uiPaused()) {   // сверка g12-data круг 7 (P18-12): △ на площадке переброса
      float tp_[3] = {homer.x, homer.y + 1.f, homer.z}; int k_ = svTeleportAt(tp_); if (k_ >= 0) { svTpGo = k_; hit &= ~PSP_CTRL_TRIANGLE; } }
    if ((hit & PSP_CTRL_TRIANGLE) && haveAo && !fly && !flyChord && onFoot() && aoWantsAction(&ao) >= 0) {   // △ у выключателя объекта (Duff, АЭС)
      aoAct = 1; sndSwitchPress(); homerPressButton(&homer, NAN); hit &= ~PSP_CTRL_TRIANGLE; }   // сверка g04-homer круг 2 (P05:3): клип нажатия   // сверка g08-sound (P05:3): щелчок switch на 0,3 с после нажатия
#ifndef SHOP_PHASE_OLD
#define SHOP_PHASE_OLD 0   // сверка g10b-loop круг 2 (g07 P05:11): 1 — гардероб доступен всегда (прежнее)
#endif
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && !fly && !flyChord && onFoot() && inRoom() && (SHOP_PHASE_OLD || mvShopAllowed()) && wardrobeNear(homer.x, homer.y, homer.z)) {   // правка 107: △ у места костюмов; круг 2: только вступление миссии и вне миссий
      char sd[256]; levelFile(sd, ""); if (wardrobeOpen1233(sd, skinWorn) == 0) { hit &= ~PSP_CTRL_TRIANGLE; sndMusic(SND_MUS_STORE); } }   // правка 235: гардероб — Store
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && !fly && !flyChord && onFoot() && roomsDoorNear(&rooms, &rs, &G)) {   // правка 41: △ у двери / выхода комнаты — войти / выйти
      rs.action = 1; hit &= ~PSP_CTRL_TRIANGLE; }
#include "main_actbooth.h"   // сверка g10b-loop круг 3: блок будки вынесен в файл (текст тот же), ранг см. выше
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord && onFoot()) {   // правка 53: △ у машины мира (ракетная) — пересесть
      float wc[4]; int wi = mvWorldCarNear(homer.x, homer.z, JACK_R, wc);
      if (wi >= 0) { Car keep = car; CarVis keepVis = cv; int had = oldCarOn;
        if (mvTakeWorldCar(wi)) { if (!had) parkOwnCar(&keep, &keepVis);
                                  carPlace(&car, &world, wc[0], wc[1], wc[2], wc[3]); } } }
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord && onFoot()) {   // правка 63: △ у чужой машины — угон
      int jp, ji = jackNear(&jp);
      if (smkOn && haveTraffic) { int pk = 0; float d = -1.f; int ni = trafficNearest(&traffic, homer.x, homer.z, 30.f, &pk, &d);   // правка 285: служебное
        char s[160]; snprintf(s, sizeof s, "TRIJACK ji=%d near=%d park=%d d=%.2f model=%s homer=%.1f,%.1f t=%.1f\n", ji, ni, pk, d,
                              ni >= 0 ? traffic.model[trafficCarModelAt(&traffic, ni, pk)].name : "-", homer.x, homer.z, smkT); smkWrite(s); }
      if (ji >= 0 || ji == -2) { if (ji >= 0) tutForeignCar(); carjack(ji, jp); } }   // сверка g07b круг 5 (P15-10): подсказка «в чужую машину»   // машина игрока встала на её место — ниже Гомер садится в неё
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord) {   // △ — выйти / сесть, в том числе с зажатым тормозом
      gameTriangle(&G);   // выходит на малой скорости / садится (viewer/game.c)
      hit &= ~PSP_CTRL_TRIANGLE;
    }
    if (tri0_ && !(hit & PSP_CTRL_TRIANGLE)) g10bTriFired(&triCool);   // P05:19: кнопка сработала — следующие 3 кадра △ не принимается (минимум 4 кадра между срабатываниями)
    if (svcMode && (hit & PSP_CTRL_TRIANGLE) && (fly || flyChord || !haveHomer || !haveCol)) { fly = !fly; fx = camx; fy = camy; fz = camz; fyaw = atan2f(x - camx, -(z - camz)); fpitch = -.2f; }   // L+R+△ — служебный полёт, △ возвращает (только служебный режим)
    if (smkOn && fly * 2 + hud != svc0) { char s[64]; snprintf(s, sizeof s, "SVC state t=%.1f svc=%d fly=%d hud=%d\n", smkT, svcMode, fly, hud); smkWrite(s); }
    if (stuckReq1200) { stuckReq1200 = 0; resetCar = 1; }   // №1200: застряла в геометрии — на дорогу
    if (resetCar) {                                 // RESET CAR (пауза) / START без ui.shr
      if (haveCol) {
        float rx0 = car.x, rz0 = car.z;
        gameReset(&G); x = car.x; y = car.y; z = car.z; h = car.h;   // машина рядом на колёса, остов чинится, Гомер за рулём
        // Правка Роба 484 (10.10, «Reset Car — на ближайшую дорогу рядом, по направлению полосы, не на старт»): точка дорожной сети трафика
        // (viewer/traffic.c trafficLaneSpot) не дальше RESET_ROAD_R от места, где стояла машина, курс по ходу полосы; нет сети или дороги
        // рядом — прежнее (надёжное место / старт). Служебный инструмент, помечен «убрать к релизу». -DRESET_ROAD_OLD=1 — прежнее
#ifndef RESET_ROAD_OLD
#define RESET_ROAD_OLD 0
#endif
#ifndef RESET_ROAD_R
#define RESET_ROAD_R 80.f
#endif
        if (!RESET_ROAD_OLD && haveTraffic) {
          float rp[3], rh;
          if (trafficLaneSpot(&traffic, rx0, rz0, RESET_ROAD_R, rp, &rh)) { carPlace(&car, &world, rp[0], rp[1] + .5f, rp[2], rh); x = car.x; y = car.y; z = car.z; h = car.h; }
        }
        prevDmg = car.damage; sndCarDamage(car.damage);
      }
      v = vy = 0; chase.init = 0; fly = 0; props.havePreviousCar = 0;
    }
    if (haveCol) smokePose(car.x, car.y, car.z, car.h);   // служебное: позиция машины в строку SHOT (сверка гейтами)
    gtClock95 += dt;   // Build 95: игровое время для анимаций текстур и мерцаний (стоит там, где dt = 0)
    { SmokeCmd sc; for (float sdt = wallDt; smokeNext(sdt, &sc); sdt = 0.f) {   // служебное: команды scenario.txt (viewer/smoke.h)
        if (sc.op == 1) { int fsmg_ = 0; if (footFsm && haveCol && rs.room < 0 && !rg.mem && hypotf(sc.a - x, sc.b - z) > 80.f) { frames += transitClose(&fbp0, "teleport") / (longView ? 2 : 1); msnTeleNews1239 = 0; msnTeleport(sc.a, y, sc.b, &fbp0); msnTeleNews1239 = 1; fsmg_ = 1; }   // Лестница-96 (№4б2)
          x = sc.a; z = sc.b; h = sc.c; y = 200; v = vy = 0; float g; if (ground(x, z, y, 0.f, &g)) y = g;
          if (haveCol) { carPlace(&car, &world, x, y, z, h); x = car.x; y = car.y; z = car.z; h = car.h; }
          camx = x - sinf(h) * 7.5f; camy = y + 3.2f; camz = z - cosf(h) * 7.5f; chase.init = 0; fly = 0; props.havePreviousCar = 0;
          if (haveTraffic) trafficReset(&traffic, &world); if (fsmg_) { chase.init = 0; smkSettle(); } }   // телепорт: трафик и пешеходы заново вокруг
        else if (sc.op == 3) {   // foot X Z H [Y]: Гомер пешком (машина стоит где стоит); из комнаты — сначала в город
          int fsm_ = 0; if (footFsm && haveHomer && haveCol && rs.room < 0 && !rg.mem && (isnan(sc.d) || sc.d >= -10.f) && hypotf(sc.a - homer.x, sc.b - homer.z) > 80.f) { float g_ = isnan(sc.d) ? 0.f : sc.d; frames += transitClose(&fbp0, "teleport") / (longView ? 2 : 1); msnTeleNews1239 = 0; msnTeleport(sc.a, g_, sc.b, &fbp0); msnTeleNews1239 = 1; fsm_ = 1; }   // Лестница-96 (№4б2); Y < -10 - точка в комнате, туда не переносим; после постановки - smkSettle (как у остальных переходов: ирис раскрывается)
          if (haveHomer && haveCol) { float g = sc.d; if (isnan(g)) { g = 200.f; if (!ground(sc.a, sc.b, g, 0.f, &g)) g = car.y; }
            if (inRoom() && !isnan(sc.d) && sc.d < -10.f) { homerPlace(&homer, G.pHomerWorld, sc.a, g, sc.b, sc.c); }   // Y под городом — точка в этой же комнате (в триггере выхода — выход, если взведён)
            else if (!FOOTROOM1209_OLD && !footRoom1209Old && !isnan(sc.d) && sc.d < -10.f && (footLowland1209Old || npcRoomAt(sc.a, sc.d, sc.b))) { char s9_[128]; snprintf(s9_, sizeof s9_, "FOOTSKIP1209 room=%d mem=%d t=%.1f\n", rs.room, rg.mem != 0, smkT); smkWrite(s9_); }   // №1209: точка в комнате, а Гомер ещё не в ней (дверь на PSP открывается дольше) — не ставить в город под землю
            else { if (rs.room >= 0 || rg.mem) roomLeave();
              homerPlace(&homer, &world, sc.a, g, sc.b, sc.c); }
            homerDoor(0, 0.f); chase.init = 0; fly = 0; autodrive = 0; if (fsm_) smkSettle(); }
          char s[128]; snprintf(s, sizeof s, "FOOT %.2f %.2f %.2f %.3f t=%.1f\n", homer.x, homer.y, homer.z, homer.h, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "sellergo") && haveHomer && haveCol && mvShop() && (int)sc.a >= 0 && (int)sc.a < mvShop()->nseller) {   // служебное (№314 п.3, tests/shop/shops_levels): Гомер рядом с продавцом N там, где он сейчас стоит (лавка идёт за самим продавцом, правка 288)
          const ShopSeller *v_ = &mvShop()->seller[(int)sc.a]; float g = v_->y + 2.f; if (!ground(v_->x + .9f, v_->z, g, 0.f, &g)) g = v_->y;
          if (rs.room >= 0 || rg.mem) roomLeave();
          homerPlace(&homer, &world, v_->x + .9f, g, v_->z, -GU_PI / 2); homerDoor(0, 0.f); chase.init = 0; fly = 0; autodrive = 0;
          char s[160]; snprintf(s, sizeof s, "SELLERGO %d npc=%s at=%.1f,%.1f,%.1f t=%.1f\n", (int)sc.a, v_->npc, v_->x, v_->y, v_->z, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "phonein")) {   // служебное (Переход/Р2 гейт будок): Гомер в центре триггера будки k пешком (ground), строка PHONEIN k ok=; только сценарий
          int k_ = (int)sc.a, ok_ = haveHomer && haveCol && k_ >= 0 && (unsigned)k_ < world.nphone;
          if (ok_) { const float *P_ = world.phone[k_].p; float g_ = P_[1] + 3.f; if (!ground(P_[0], P_[2], g_, 0.f, &g_)) g_ = P_[1];
            if (rs.room >= 0 || rg.mem) roomLeave();
            homerPlace(&homer, &world, P_[0], g_, P_[2], 0.f); homerDoor(0, 0.f); chase.init = 0; fly = 0; autodrive = 0; smkSettle(); }
          if (smkOn) { char l_[80]; snprintf(l_, sizeof l_, "PHONEIN k=%d ok=%d n=%d t=%.1f\n", k_, ok_, haveCol ? (int)world.nphone : -1, smkT); smkWrite(l_); } }
        else if ((!strcmp(sc.name, "bonusgo") || !strcmp(sc.name, "phonego")) && haveHomer && haveCol) {   // служебное (спрайты значков 18.10): Гомер в 6 м
          // от k-го персонажа бонус-миссии со значком «!» (bonusgo) или от будки k (phonego), глаз сбоку в 7 м на высоте значка, взгляд на значок; flag eyeon 0 снимает
          // k = -1 у bonusgo — собеседник текущей цели (значок «!» над ним, msn_view.c)
          int ph_ = sc.name[0] == 'p', k_ = (int)sc.a; float xz_[2], hy_ = ph_ ? 1.1f : 3.f, g = 0.f, gy_, tk_[4];
          int ok_ = ph_ ? (k_ >= 0 && (unsigned)k_ < world.nphonex) : k_ < 0 ? mvTalkNpcAt(tk_) : mvGiverAt(UI_R_BONUS, k_, xz_);
          if (ok_ && ph_) { xz_[0] = world.phonex[k_].loc[0]; xz_[1] = world.phonex[k_].loc[2]; g = world.phonex[k_].loc[1];
            if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "PHONEX k=%d n=%d loc=%.2f,%.2f,%.2f t=%.1f\n", k_, (int)world.nphonex, xz_[0], g, xz_[1], smkT); smkWrite(l_);
              if ((unsigned)k_ < world.nphone) { snprintf(l_, sizeof l_, "PHONEW k=%d p=%.2f,%.2f,%.2f r=%.2f\n", k_, world.phone[k_].p[0], world.phone[k_].p[1], world.phone[k_].p[2], world.phone[k_].r); smkWrite(l_); } } }   // Build 96: координаты будок для сценариев (1250)
          else if (ok_ && k_ < 0) { xz_[0] = tk_[0]; xz_[1] = tk_[2]; g = tk_[1]; hy_ = tk_[3] + 3.f; }
          else if (ok_) { g = 200.f; if (!ground(xz_[0], xz_[1], g, 0.f, &g)) g = 0.f; }
          if (ok_) {
            if (rs.room >= 0 || rg.mem) roomLeave();
            gy_ = g + 2.f; if (!ground(xz_[0] + 6.f, xz_[1], gy_, 0.f, &gy_)) gy_ = g;
            homerPlace(&homer, &world, xz_[0] + 6.f, gy_, xz_[1], -GU_PI / 2); homerDoor(0, 0.f); chase.init = 0; fly = 0; autodrive = 0;
            eyeOv[0] = xz_[0] + 5.f; eyeOv[1] = g + hy_ + 1.2f; eyeOv[2] = xz_[1] + 5.f; { float dx_ = -5.f, dy_ = -1.2f, dz_ = -5.f, l_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_);
              eyeOv[3] = dx_ / l_; eyeOv[4] = dy_ / l_; eyeOv[5] = dz_ / l_; } eyeOvOn = 1; }
          char s[128]; snprintf(s, sizeof s, "%s %d ok=%d at=%.1f,%.1f,%.1f t=%.1f\n", ph_ ? "PHONEGO" : "BONUSGO", k_, ok_, ok_ ? xz_[0] : 0.f, g, ok_ ? xz_[1] : 0.f, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "aogo") && haveAo) {   // служебное (№834): глаз в 6 м от кольца выключателя k объекта с действием, взгляд на него
          int k_ = (int)sc.a, n_ = 0; float c_[3], r_ = 0.f; int ok_ = 0;
          for (unsigned j = 0; j < ao.nobj && !ok_; j++) if (aoSwitchRing(&ao, j, c_, &r_) && n_++ == k_) ok_ = 1;
          if (ok_) { eyeOv[0] = c_[0] + 4.2f; eyeOv[1] = c_[1] + 2.6f; eyeOv[2] = c_[2] + 4.2f; { float dx_ = -4.2f, dy_ = -1.6f, dz_ = -4.2f, l_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_);
              eyeOv[3] = dx_ / l_; eyeOv[4] = dy_ / l_; eyeOv[5] = dz_ / l_; } eyeOvOn = 1; }
          char s[128]; snprintf(s, sizeof s, "AOGO %d ok=%d at=%.1f,%.1f,%.1f r=%.2f t=%.1f\n", k_, ok_, ok_ ? c_[0] : 0.f, ok_ ? c_[1] : 0.f, ok_ ? c_[2] : 0.f, r_, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "gaglist") && haveColl) { for (unsigned g_ = 0; g_ < coll.ngag; g_++) { char l_[96]; snprintf(l_, sizeof l_, "GAG idx=%u name=%s room=%d inst=%d nskin=%u len=%.2f\n", g_, coll.gag[g_].name, coll.gag[g_].room, coll.gag[g_].inst, coll.gag[g_].nskin, coll.gag[g_].len); smkWrite(l_); } }   // №869 служебно
        else if (!strcmp(sc.name, "gagplay") && haveColl && haveHomer && (unsigned)(int)sc.a < coll.ngag) {   // №869 служебно: в комнату гэга и запустить его (играет по своим часам)
          int gp_ = (int)sc.a; const CollGag *g = coll.gag + gp_;
          if (rs.room >= 0 || rg.mem) roomLeave();
          int ri = -1; for (int i = 0; i < rooms.nroom; i++) if (collectRoomId(rooms.room[i].name) == g->room) ri = i;
          const float *p = ri >= 0 ? rooms.room[ri].start : &g->tx;
          homerPlace(&homer, &world, p[0], (ri >= 0 ? p[1] : g->y) + 1.f, p[2], 0.f); msnRoomSync();
          coll.st.lastRoom = collectRoomId(roomName()); for (unsigned k_ = 0; k_ < coll.ngag; k_++) coll.st.gagPick[k_] = k_ == (unsigned)gp_;
          if (sc.a == floorf(sc.a)) collectGagPlay(&coll, (unsigned)gp_); smkSettle();   // gagplay N.5 — только войти и выбрать (без запуска: проверка триггера)
          char log[96]; snprintf(log, sizeof log, "GAGPLAY idx=%d name=%s room=%d pos=%.2f,%.2f,%.2f\n", gp_, g->name, coll.st.lastRoom, g->x, g->y, g->z); smkWrite(log); }
        else if (!strcmp(sc.name, "gagsnap") && haveColl && haveHomer && (unsigned)(int)sc.a < coll.ngag) {
          gagSnap = (int)sc.a; const CollGag *g = coll.gag + gagSnap;
          if (rs.room >= 0 || rg.mem) roomLeave();
          int ri = -1; for (int i = 0; i < rooms.nroom; i++) if (collectRoomId(rooms.room[i].name) == g->room) ri = i;
          const float *p = ri >= 0 ? rooms.room[ri].start : &g->tx;
          homerPlace(&homer, &world, p[0], (ri >= 0 ? p[1] : g->y) + 1.f, p[2], 0.f); msnRoomSync();
          coll.st.lastRoom = collectRoomId(roomName()); smkSettle();
          char log[96]; snprintf(log, sizeof log, "GAGSNAP idx=%d name=%s room=%d\n", gagSnap, g->name, coll.st.lastRoom); smkWrite(log);
        }
        else if (!strcmp(sc.name, "pedtest") && haveTraffic && haveHomer) {   // служебный пешеход в 3 м перед Гомером по камере и 0.45 м правее (виден рядом с Гомером; гейт «Гомер о пешехода»)
          float a = homer.camYaw; trafficPedPlace(&traffic, &world, (int)sc.a, homer.x + sinf(a) * 3.f - cosf(a) * .45f, homer.z + cosf(a) * 3.f + sinf(a) * .45f, a + GU_PI);
          trPedMin = 1e9f; char s[160]; snprintf(s, sizeof s, "PEDTEST t=%.1f homer=%.2f,%.2f ped=%.2f,%.2f yaw=%.4f\n", smkT, homer.x, homer.z, traffic.ped[0].x, traffic.ped[0].z, a); smkWrite(s); }
        else if (!strcmp(sc.name, "pedbehind") && haveTraffic && haveHomer) {   // служебное (гейт tests/eyejump, перелёт глаза): стоящий прохожий в d м за Гомером по курсу камеры, повёрнутый на b° вправо (flag pedbehind d b)
          float a = homer.camYaw + (isnan(sc.b) ? 0.f : sc.b * (GU_PI / 180.f)), d = sc.a > 0.f ? sc.a : 1.3f; int ok = trafficPedPlace(&traffic, &world, 0, homer.x - sinf(a) * d, homer.z - cosf(a) * d, a) == 0;
          char s[128]; snprintf(s, sizeof s, "PEDBEHIND t=%.1f ok=%d ped=%.2f,%.2f\n", smkT, ok, traffic.ped[0].x, traffic.ped[0].z); smkWrite(s); }
        else if (!strcmp(sc.name, "camkick")) { camKickD = sc.d; camKickY = isnan(sc.b) ? 0.f : sc.b; }   // служебное: прошлый глаз камеры сдвинут вбок на D м (начало перелёта)
        else if ((!strcmp(sc.name, "pednear") || !strcmp(sc.name, "pedahead")) && haveTraffic) {   // служебное (гейт правки 180): прохожий в N м впереди машины
          float d = sc.a > 0.f ? sc.a : 20.f, side = sc.name[3] == 'n' ? 1.6f : 0.f, sx = cosf(car.h) * side, sz = -sinf(car.h) * side;   // игрока: сбоку 1,6 м / на пути
          int ok = trafficPedPlace(&traffic, &world, 0, car.x + sinf(car.h) * d + sx, car.z + cosf(car.h) * d + sz, car.h + GU_PI) == 0;
          char s[128]; snprintf(s, sizeof s, "PEDNEAR t=%.1f ok=%d ped=%.2f,%.2f\n", smkT, ok, traffic.ped[0].x, traffic.ped[0].z); smkWrite(s); }
        else if (!strcmp(sc.name, "trfahead") && haveTraffic) {   // служебное: машина трафика на полосе в N м впереди машины игрока (снимок движения)
          float d = sc.a > 0.f ? sc.a : 20.f; int ok = trafficCarPlace(&traffic, &world, car.x + sinf(car.h) * d, car.z + cosf(car.h) * d, car.h) == 0;
          char s[128]; snprintf(s, sizeof s, "TRFAHEAD t=%.1f ok=%d car0=%.2f,%.2f\n", smkT, ok, traffic.car[0].car.x, traffic.car[0].car.z); smkWrite(s); }
        else if (!strcmp(sc.name, "trfmodel") && haveTraffic && traffic.car[0].mode != TRM_OFF) {   // служебное (гейт правки 413): машина трафика 0 — модель N пакета
          int m = (int)sc.a; TrCar *c = &traffic.car[0];
          if (m >= 0 && m < (int)traffic.hd->nModel) { Car k = c->car; c->model = m; carInit(&c->car, &traffic.params[m], &world, k.x, k.y, k.z, k.h); }
          char s[128]; snprintf(s, sizeof s, "TRFMODEL t=%.1f model=%d name=%.16s at=%.2f,%.2f\n", smkT, c->model, traffic.model[c->model].name, c->car.x, c->car.z); smkWrite(s); }
        else if (!strcmp(sc.name, "carsndold")) sndCarSndOld(sc.a != 0.f);
        else if (!strcmp(sc.name, "hornold")) hornOld = sc.a != 0.f;   // правка 413: без гудка (красный контроль)   // правка 413а: звук всегда машиной уровня (красный контроль)
        else if (!strcmp(sc.name, "hornstopold")) hornStopOld = sc.a != 0.f;   // №561: вечный гудок NPC (красный контроль)
        else if (!strcmp(sc.name, "skidold")) skidOld(sc.a != 0.f);   // №562: скрежет резины (красный контроль)
        else if (!strcmp(sc.name, "resumeaudioold")) audioResumeOld(sc.a != 0.f);   // №560: сон/пробуждение музыки (красный контроль)
        else if (!strcmp(sc.name, "carglassold")) carGlassOld = sc.a != 0.f;   // правка 413б: прежние «прозрачные» машины (красный контроль)
        else if (!strcmp(sc.name, "seatlog")) { float sp[9] = {0}; int ok = haveHomer && homerSeatProbe(&homer, sp);   // №666: посадка героя в машине (tests/seat)
          char s[320]; snprintf(s, sizeof s, "SEAT t=%.1f level=%d hero=%s car=%s ok=%d head=%.3f roof=%.3f hip=%.3f height=%.3f under=%.3f seat=%.3f,%.3f,%.3f ax=%.3f az=%.3f lift=%.3f door=%d irisIn=%d headw=%.2f,%.2f,%.2f\n", smkT, gameLevel, homerClipPrefix(), car.p.name, ok,
                                ok ? sp[0] : 0.f, ok ? sp[1] : 0.f, ok ? sp[2] : 0.f, ok ? sp[3] : 0.f, ok ? sp[4] : 0.f, car.p.seat[0], car.p.seat[1], car.p.seat[2], car.p.seatAx, car.p.seatAz, sp[5], homerDoorCan ? homerDoorCan() : -1, homer.irisCarIn, sp[6], sp[7], sp[8]); smkWrite(s); }
        else if (!strcmp(sc.name, "sndseed")) sndSeed((unsigned)sc.a);   // №685: гейт — семя случайного звука как в игре (смоук его не зовёт)
        else if (!strcmp(sc.name, "loadwaitlose")) { lwLose = (int)sc.a; }
        else if (!strcmp(sc.name, "loadwaitms")) { lwMs = (unsigned)sc.a; loadFuseIoLimitMs((unsigned)sc.a); }   // №LOADWAIT: тест — первые N сигналов загрузчику теряются
        else if (!strcmp(sc.name, "loadwaitold")) { loadWaitOld = sc.a != 0.f; loadFuseIoWaitOld(loadWaitOld); }
        else if (!strcmp(sc.name, "loaderdeaf")) loaderDeafUntil = (sceKernelGetSystemTimeLow() + (unsigned)sc.a * 1000u) | 1u;   // ТЕСТ90 2Б: загрузчик не берёт задания
        else if (!strcmp(sc.name, "waitlimitms")) { wlMs = (unsigned)sc.a; ioWaitLimitMs = (unsigned)sc.a; msioLockLimitMs = (unsigned)sc.a; }   // ТЕСТ90: предел ожиданий N мс вместо штатного (0 — штатный)
        else if (!strcmp(sc.name, "waitlimitold")) { waitLimitOld = sc.a != 0.f; ioWaitLimitOld = waitLimitOld; }
        else if (!strcmp(sc.name, "readhangsync")) msioHangTest = (int)sc.a;
        else if (!strcmp(sc.name, "readhangloader")) msioHangLoader = (int)sc.a;   // ТЕСТ90: следующие N чтений загрузчика не возвращаются   // ТЕСТ90: следующие N чтений карты любым потоком не возвращаются
        else if (!strcmp(sc.name, "loadreadhang")) loadFuseIoHang((int)sc.a);   // №LOADWAIT: тест — первые N чтений главного при экране загрузки «не завершаются»
        else if (!strcmp(sc.name, "ldslow")) ldSlowUs = (unsigned)sc.a;   // №775: медленная карта на экранах загрузки (мкс на кусок чтения)
        else if (!strcmp(sc.name, "fragheap")) fragHeapMB = (unsigned)sc.a;
        else if (!strcmp(sc.name, "pvschunkold")) pvsChunkOld = sc.a != 0.f;   // №1068: красный контроль — PVS1 одним куском из кучи
        else if (!strcmp(sc.name, "pvscapsim")) lvaSoftCapSim = (unsigned)(sc.a * 1048576.f);   // №1068 тест: куча отказывает мягким кускам больше N МБ
        else if (!strcmp(sc.name, "heapleft")) heapLeft(sc.a);   // Б TEST57 тест: куча занята, пока свободно не станет N МБ (на железе L5 после №912: 6,6–7,9 МБ — у границы 7 МБ ворот REGPRE)
#if LEAK696_TRACE
        else if (!strcmp(sc.name, "hmap")) { extern void lt696Map(void (*)(const char *), const char *, unsigned); char tg_[24]; snprintf(tg_, sizeof tg_, "now%d", (int)sc.a); lt696Map(smkWrite, tg_, 64); { extern void lt696Dump(void (*)(const char *), const char *); lt696Dump(smkWrite, tg_); } }   // ТЕСТ90 память (диагностика): «flag hmap N» — карта кучи и прирост по вызывающим прямо сейчас
#endif
        else if (!strcmp(sc.name, "collfail")) collFailTest = (int)sc.a;   // ТЕСТ90 память: следующие N выделений пакета гэгов без памяти (проверка повтора после mem1060Pre)
        else if (!strcmp(sc.name, "fragmenu")) fragMenuMB = (unsigned)sc.a;   // №841 тест: куча раздроблена до N МБ перед гостиной меню
        else if (!strcmp(sc.name, "fefail")) feFailTest = (int)sc.a;
        else if (!strcmp(sc.name, "floornearold")) floorNearOld = sc.a != 0.f;   // №861 служебно: 1 — ближняя плоскость без правила пола
        else if (!strcmp(sc.name, "cfgdefaults") && cfgDefOk) { cfg = cfgDef; cfgApply(); char l_[160]; snprintf(l_, sizeof l_, "CFGDEF dd=%d colour=%d ct=%d lw=%d oldtv=%d t=%.1f\n", cfg.drawDist, cfg.colour, cfg.contrast, cfg.lineWidth, cfg.oldTv, smkT); smkWrite(l_); }   // №851: настройки сборки по умолчанию
        else if (!strcmp(sc.name, "memtrim1060")) memTrim1060 = sc.a != 0.f;
        else if (!strcmp(sc.name, "memtrim2")) memTrim2 = sc.a != 0.f;   // №1060в: 0 — эпизод по перемеру кучи каждый кадр (churn), как MEMTRIM1060
        else if (!strcmp(sc.name, "camclipfast1060")) camClipFast1060 = sc.a != 0.f;   // №1060: 0 — полный обход, как было
        else if (!strcmp(sc.name, "camclipchk")) camClipChk1060 = sc.a != 0.f;   // №1060 служебно: считать обоими способами, расхождения — строка CAMCLIPFAST mismatch   // №1060: 0 — выгрузка клеток при тесной куче разом, как было
        else if (!strcmp(sc.name, "boothmem1051")) boothMem1051 = sc.a != 0.f;   // №1051: 0 — будка как в TEST72 (одна попытка после неудачи)
        else if (!strcmp(sc.name, "memload1065")) memLoad1065 = sc.a != 0.f;   // №1065: 0 — прежнее (газета без проверки, ui и звук после игрока)
        else if (!strcmp(sc.name, "sndfail1065")) { sndFail1065 = (int)sc.a; sndFail1065Sim = sndFail1065 > 0; }   // №1065 служебно: следующие N открытий банка звука отказывают
        else if (!strcmp(sc.name, "sndslotold")) audioSlotOld(sc.a != 0.f);   // №1069: красный контроль — банк звука из кучи на каждом уровне, как было
        else if (!strcmp(sc.name, "sndcapsim")) audioBankCapSim((unsigned)(sc.a * 1048576.f));   // №1069 тест: банк звука из кучи больше N МБ не выделяется
        else if (!strcmp(sc.name, "scrfail1065")) scrFail1065 = (int)sc.a;   // №1065 служебно: следующие N открытий load.shr отказывают
        else if (!strcmp(sc.name, "memladder")) { memLadOn = sc.a != 0.f; if (!memLadOn) { memLadResTake(); memLadDdMul = 1.f; } }   // правка 435: 0 — прежние частные обходы
        else if (!strcmp(sc.name, "memladres")) { memLadResOn = sc.a != 0.f; if (!memLadResOn) memLadResTake(); }
        else if (!strcmp(sc.name, "mem1060")) mem1060 = sc.a != 0.f;   // №1060: 0 — будка как в №1051
        else if (!strcmp(sc.name, "mem1060rebuild")) mem1060Rebuild = sc.a != 0.f;
        else if (!strcmp(sc.name, "mem1060fe")) mem1060Fe = sc.a != 0.f;   // №1060б: 0 — газета уровня или шнур, как в №1060
        else if (!strcmp(sc.name, "mem1060fedemo")) mem1060FeDemo(&fbp0, sc.a > 0.f ? (int)sc.a : 500);   // №1060б служебно: кадр LoadingFE на заданном ходе (эталон для гейта)
        else if (!strcmp(sc.name, "boothstarve")) { if (sc.a >= 999.f) { heapLeft(1000.f); starve1060 = -1.f; } else { starve1060 = sc.a; heapLeft(sc.a); } }   // №1060 служебно: оставлять кучу не больше N МБ свободной после каждой выгрузки клеток (1000 — отдать)
        else if (!strcmp(sc.name, "mem1060log")) mem1060Stat((int)sc.a);   // №1060 служебно: состояние кучи и клеток (гейт)
        else if (!strcmp(sc.name, "heapfrag")) heapFragSet((int)sc.a);   // правка 432 служебно: раздробить кучу — свободное остаётся дырами по N КБ (0 — вернуть); модель кучи PSP Роба после погони (свободно 4 МБ, блок 0,38 МБ)
        else if (!strcmp(sc.name, "cityfill")) { static void *cf_[256]; static int cfn_; while (cfn_) free(cf_[--cfn_]); if (sc.a != 0.f && zone3CityOnX()) { for (int k_ = 0; k_ < 256; k_++) { void *m_ = zone3TryX(Z3A_CITY, "fill", 32u << 10, 16); if (!m_) break; cf_[cfn_++] = m_; } }   // служебно: область «город» забита до отказа (как кольцо старта к загрузке владельцев) - гейт s13
          char l_[100]; snprintf(l_, sizeof l_, "CITYFILL on=%d blocks=%d cityon=%d free_kb=%u maxfree_kb=%u t=%.1f\n", sc.a != 0.f, cfn_, zone3CityOnX(), zone3CityOnX() ? zone3FreeTotX(Z3A_CITY) >> 10 : 0u, zone3CityOnX() ? zone3MaxFreeX(Z3A_CITY) >> 10 : 0u, smkT); if (smkOn) smkWrite(l_); }
        else if (!strcmp(sc.name, "ringhero")) ringHeroOn = sc.a != 0.f;   // Лестница-96: 0 - круг старта у машины в levelLoad, как до правки (красный контроль)
        else if (!strcmp(sc.name, "cityfit")) cityFitOn = sc.a != 0.f;   // Лестница-96: 0 - просьбы клеток без проверки места области города, как до правки (красный контроль)
        else if (!strcmp(sc.name, "cellregfix")) cellRegFix = sc.a != 0.f;   // Опора-93: 0 - сторож cellfail не добавляет регион клетки под героем
        else if (!strcmp(sc.name, "ringcap")) ringCapOn = sc.a != 0.f;   // Лестница-96: 0 - дальность после урезанного круга не держится (красный контроль s19)
        else if (!strcmp(sc.name, "footfsm")) footFsm = sc.a != 0.f;   // Лестница-96 (№4б2): 1 - foot/goto сценария идут настоящим путём переноса (экран перехода, круг клеток под ним), а не голой телепортацией
        else if (!strcmp(sc.name, "farmin")) farMinOn = sc.a != 0.f;   // Лестница-96: 0 - клетка может менять версию без интервала 3 с
        else if (!strcmp(sc.name, "farhead")) farHeadOn = sc.a != 0.f;   // Лестница-96: 0 - обмен версий клетки без ожидания запаса кадра
        else if (!strcmp(sc.name, "farcell")) farOn = sc.a != 0.f;   // Лестница-96: 0 - только полные клетки
        else if (!strcmp(sc.name, "farfake")) farFake = sc.a != 0.f;   // Лестница-96, служебно: проба очереди версий - «дальняя» версия та же полная (ставить ДО загрузки уровня)
        else if (!strcmp(sc.name, "streameye")) streamEyeOv = sc.a != 0.f;   // Лестница-96: 1 - стрим грузит вокруг глаза сценария (eyeOv), а не камеры игры
        else if (!strcmp(sc.name, "dblprov")) cellDblProv = sc.a != 0.f;   // Лестница-96, служебно: круг клеток грузит каждую клетку дважды подряд (честное красное для dblguard 0)
        else if (!strcmp(sc.name, "dblguard")) cellDblGuard = sc.a != 0.f;   // Лестница-96 (утечка 102): 0 - клетку, уже загруженную очередью, круг/очередь грузят второй раз (красный контроль)
        else if (!strcmp(sc.name, "bonusshrink")) bonusShrink107 = sc.a != 0.f;   // Лестница-96: 0 - без отцепки областей перед бонус-трассой
        else if (!strcmp(sc.name, "visstuck")) visStuckOn = sc.a != 0.f;   // Лестница-96: 0 - сторож видимых недогруженных клеток выключен (красный контроль s18)
        else if (!strcmp(sc.name, "cellstuck")) cellStuckInj = sc.a != 0.f;   // Лестница-96, служебно: stream() не берёт каждую третью клетку (имитация отсева тура 100)
        else if (!strcmp(sc.name, "ringfit")) ringFitOn = sc.a != 0.f;   // Лестница-96: 0 - круг старта берёт все клетки радиуса, как до правки (красный контроль)
        else if (!strcmp(sc.name, "toondrop")) { extern int skinToonDropOn; skinToonDropOn = sc.a != 0.f; }   // Лестница-96: 0 - оболочки брошенных моделей остаются в кэше до смены уровня (красный контроль s15)
        else if (!strcmp(sc.name, "z3dump")) { char d_[300]; zone3DumpX(Z3A_MISSION, d_, sizeof d_); char l_[360]; snprintf(l_, sizeof l_, "Z3LIVE t=%.1f %s\n", smkT, d_); smkWrite(l_); }   // Лестница-96 диагностика: живые блоки области «миссия» по владельцам
        else if (!strcmp(sc.name, "ownerheap")) memLadOwnHeap = sc.a != 0.f;   // Лестница-96: 0 - клетки неба и машины игрока из области «город» (как было), красный контроль s13
        else if (!strcmp(sc.name, "ownerold")) memLadOwnOld = sc.a != 0.f;   // Лестница-96: 1 - клетки неба и машины игрока как до правки (красный контроль s11)
        else if (!strcmp(sc.name, "ownerdrop")) { int n_ = 0; for (unsigned i = 0; i < ncell; i++) if (cl[i].owner == (unsigned)sc.a) { unloadCell(&cl[i]); cl[i].state = 3; n_++; } char l_[96]; unsigned mn_ = ~0u; for (unsigned i = 0; i < ncell; i++) if (cl[i].owner == (unsigned)sc.a && cl[i].size < mn_) mn_ = cl[i].size; snprintf(l_, sizeof l_, "OWNERDROP owner=%d cells=%d minsz_kb=%u t=%.1f\n", (int)sc.a, n_, mn_ == ~0u ? 0u : mn_ >> 10, smkT); if (smkOn) smkWrite(l_); }   // служебно: выгрузить клетки неба (2) или машины игрока (1), как после отказа загрузки
        else if (!strcmp(sc.name, "ladnpcold")) memLadNpcOld = sc.a != 0.f;   // Лестница-96: 1 - без правки для npc/car миссии (красный контроль s9 на том же EBOOT)
        else if (!strcmp(sc.name, "ladnpc")) { unsigned b0_ = memMaxBlock ? memMaxBlock() : 0, need_ = sc.a > 0.f ? (unsigned)sc.a << 10 : b0_ + (48u << 10);   /* 0 - на 48 КБ больше сегодняшнего куска (гейт не зависит от раздробления кучи) */ int ok_ = memLadNpc1235("npc", need_, 1); char l_[140]; snprintf(l_, sizeof l_, "LADNPC need=%u ok=%d blk=%.2f->%.2f dd=%.0f mul=%.2f t=%.1f\n", need_ >> 10, ok_, b0_ / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, drawDist, memLadDdMul, smkT); if (smkOn) smkWrite(l_); }   // Лестница-96 служебно: просьба «npc» миссии на N КБ цельным куском, как у загрузчика модели (гейт s9)
        else if (!strcmp(sc.name, "memfailfake")) { extern void memFailFake(unsigned n); memFailFake((unsigned)sc.a); }   // Лестница-96 служебно: выдуманные отказы для проверки печати серий
        else if (!strcmp(sc.name, "fragblk")) fragBlkSet(sc.a);   // П4: раздробить кучу до наибольшего куска F МБ (0 - снять)
        else if (!strcmp(sc.name, "boothhog")) boothHog1051Set((int)sc.a);   // №1051 служебно: занять N МБ кучи (имитация нехватки памяти), 0 — вернуть
        else if (!strcmp(sc.name, "phonefail")) phoneFailTest = (int)sc.a;   // №842 тест: следующие N открытий будки — как без памяти   // №841 тест: следующие N чтений гостиной — без памяти   // №775 тест: куча раздроблена до N МБ перед следующей загрузкой уровня
        else if (!strcmp(sc.name, "loadwaitstat")) { char s[96]; snprintf(s, sizeof s, "LOADWAITSTAT retries=%d giveups=%d t=%.1f\n", lwRetries, lwGiveups, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "saystat")) { char s[1400]; sndSayStats(s, sizeof s); char s2[1500]; snprintf(s2, sizeof s2, "t=%.1f %s", smkT, s); smkWrite(s2); }   // №683
        else if (!strcmp(sc.name, "carglass")) { char s[128]; snprintf(s, sizeof s, "CARGLASS t=%.1f old=%d frames=%u blend=%u opaque=%u tqown=%u flips=%u\n", smkT, carGlassOld, cgFrames, cgBlendTris, cgOpaqTris, tqOwnItems, tqOwnFlips); smkWrite(s); cgBlendTris = cgOpaqTris = cgFrames = tqOwnItems = tqOwnFlips = 0; }
        else if (!strcmp(sc.name, "cardhud") && haveColl && uiReady) { cardHudT = COLL_HUD_CARD_S; uiHudSet(UI_F_CARD_TITLE, (int)sc.a); smkWrite("CARDHUD shown\n"); }   // служебно (№1261б): баннер «COLLECTOR CARD!» с карточкой N
        else if (!strcmp(sc.name, "glowphase")) { glow1261Phase = sc.a; }   // служебно (№1261): фаза пульса свечения, рад (отрицательная — по времени)
        else if (!strcmp(sc.name, "glow1261old")) { glow1261On = sc.a == 0.f; }   // №1261: 1 — без свечения (красный контроль)
        else if (!strcmp(sc.name, "trfoff") && haveTraffic) { traffic.car[0].mode = TRM_OFF; smkWrite("TRFOFF\n"); }   // служебное (гейт 413): тот же кадр без машины 0
        else if (!strcmp(sc.name, "trfstill") && haveTraffic && traffic.car[0].mode != TRM_OFF) {   // служебное (гейт правки 184): машина трафика 0 стоит (тело без водителя-ИИ)
          traffic.car[0].mode = TRM_PHYS; traffic.car[0].stillT = 0.f; memset(traffic.car[0].car.vel, 0, sizeof traffic.car[0].car.vel); }
        else if (!strcmp(sc.name, "carink1028")) carInk1028 = (int)sc.a;   // №1028
        else if (!strcmp(sc.name, "roomdlrange")) { rdlLo = (int)sc.a / 1000; rdlHi = (int)sc.a % 1000; }   // листы комнат, служебно: ло*1000+хи
        else if (!strcmp(sc.name, "roomdlmask")) { rdlMask = (unsigned)sc.a; rdlFree(); }   // листы комнат, служебно
        else if (!strcmp(sc.name, "roomdlchk")) rdlChk = sc.a != 0.f;   // листы комнат, служебно
        else if (!strcmp(sc.name, "roomdlonly")) { int v_ = (int)sc.a; rdlOnly = v_ >= 10000 ? v_ / 10000 : v_; rdlOnlyHi = v_ >= 10000 ? v_ % 10000 : 0; rdlFree(); }   // листы комнат, служебно
        else if (!strcmp(sc.name, "roomdldump")) rdlDump = sc.a != 0.f;   // листы комнат, служебно
        else if (!strcmp(sc.name, "roomdlbug")) rdlBug = sc.a != 0.f;   // листы комнат, служебно: красный контроль
        else if (!strcmp(sc.name, "roomdl")) { roomDlOn = (int)sc.a; if (!roomDlOn) rdlFree(); }   // листы комнат (РОВНЫЕ_30 шаг 2): 1 — листы комнат, 0 — прямой путь (красный контроль гейта)
        else if (!strcmp(sc.name, "carink991")) carInk991 = (int)sc.a;   // №991: 1 — сдвиг оболочки NPC-машин 80 см, 0 — прежнее (красный контроль гейта)
        else if (!strcmp(sc.name, "spnear1000")) spNear1000 = sc.a != 0.f;   // №1000: 1 — далёкие деревья спят, 0 — шагают каждый кадр (красный контроль гейта)
        else if (!strcmp(sc.name, "boothdecal1052")) msnartDecal1052 = sc.a != 0.f;   // №1052: 1 — пятно не рисуется, 0 — как было
        else if (!strcmp(sc.name, "arrows1025")) msnartArrow1025 = sc.a != 0.f;   // №1025: 1 — стрелки по дороге без бликов, 0 — как было
        else if (!strcmp(sc.name, "arrowxz")) { static float ar_[4]; extern float gArrowForce[4]; gArrowForce[0] = sc.d; gArrowForce[2] = sc.b; mvForceArrow(1, gArrowForce[0], gArrowForce[1], gArrowForce[2], gArrowForce[3]); (void)ar_; }   // №1025 служебно: flag arrowxz X Z, flag arrowyh Y ГРАД_КУРС (рад), flag arrowoff
        else if (!strcmp(sc.name, "arrowyh")) { extern float gArrowForce[4]; gArrowForce[1] = sc.d; gArrowForce[3] = sc.b; mvForceArrow(1, gArrowForce[0], gArrowForce[1], gArrowForce[2], gArrowForce[3]); }
        else if (!strcmp(sc.name, "arrowoff")) mvForceArrow(0, 0, 0, 0, 0);
        else if (!strcmp(sc.name, "votego") && haveTraffic) {   // №1048 служебно: глаз в sc.a м (по умолчанию 14) от фургона Квимби (машина едет дальше), отходит со стоянием: flag eyeon 0 снимает
          for (int i_ = 0; i_ < traffic.maxCars; i_++) { TrCar *c_ = &traffic.car[i_]; if (c_->mode == TRM_OFF || c_->model < 0 || strncmp(traffic.model[c_->model].name, "votetruc", 8)) continue;
            float gd_ = sc.a > 1.f ? sc.a : 14.f; eyeOv[0] = c_->car.x + gd_; eyeOv[1] = c_->car.y + 3.f; eyeOv[2] = c_->car.z; eyeOv[3] = -1.f; eyeOv[4] = -.2f; eyeOv[5] = 0.f; eyeOvOn = 1;
            char s_[128]; snprintf(s_, sizeof s_, "VOTEGO car=%d at=%.1f,%.1f,%.1f eye=%.1f t=%.1f\n", i_, c_->car.x, c_->car.y, c_->car.z, gd_, smkT); smkWrite(s_); break; } }
        else if (!strcmp(sc.name, "vote1048")) { vote1048On = sc.a != 0.f; }   // №1048: голос фургона Квимби
        else if (!strcmp(sc.name, "trfmodels") && haveTraffic) {   // №1048 служебно: модели трафика уровня и сколько машин какой модели сейчас ездит
          char l_[400]; int o_ = snprintf(l_, sizeof l_, "TRFMODELS level=%d n=%u:", gameLevel, traffic.hd ? (unsigned)traffic.hd->nModel : 0u);
          for (unsigned m_ = 0; traffic.hd && m_ < traffic.hd->nModel && o_ < 340; m_++) { int c_ = 0; for (int i_ = 0; i_ < traffic.maxCars; i_++) if (traffic.car[i_].mode != TRM_OFF && traffic.car[i_].model == (int)m_) c_++;
            o_ += snprintf(l_ + o_, sizeof l_ - o_, " %.10s=%d", traffic.model[m_].name, c_); }
          snprintf(l_ + o_, sizeof l_ - o_, "\n"); smkWrite(l_); }
        else if (!strcmp(sc.name, "trfgoh")) trfgoH = sc.d;   // №991 служебное: высота глаза над машиной для trfgo
        else if (!strcmp(sc.name, "trfgo") && haveTraffic && haveCol) {   // служебное (№930, снимки машин трафика): первая машина трафика встаёт, глаз в 7 м от неё под углом sc.a (рад) на 2 м выше, взгляд на неё; flag eyeon 0 снимает
          int i_ = 0; while (i_ < traffic.maxCars && traffic.car[i_].mode == TRM_OFF) i_++;
          if (i_ < traffic.maxCars) { TrCar *c_ = &traffic.car[i_]; c_->mode = TRM_PHYS; c_->stillT = 0.f; memset(c_->car.vel, 0, sizeof c_->car.vel);
            float gd_ = isnan(sc.b) ? 7.f : sc.b;   // №991 служебное: flag trfgo УГОЛ [РАССТОЯНИЕ] и flag trfgoh ВЫСОТА (по умолчанию 7 м и 2,2 м)
            eyeOv[0] = c_->car.x + gd_ * cosf(sc.a); eyeOv[1] = c_->car.y + trfgoH; eyeOv[2] = c_->car.z + gd_ * sinf(sc.a);
            float dx_ = c_->car.x - eyeOv[0], dy_ = c_->car.y + .6f - eyeOv[1], dz_ = c_->car.z - eyeOv[2], l_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_); eyeOv[3] = dx_ / l_; eyeOv[4] = dy_ / l_; eyeOv[5] = dz_ / l_; eyeOvOn = 1;
            char s_[128]; snprintf(s_, sizeof s_, "TRFGO car=%d model=%.12s at=%.1f,%.1f,%.1f t=%.1f\n", i_, traffic.model[c_->model].name, c_->car.x, c_->car.y, c_->car.z, smkT); smkWrite(s_); } }
        else if (!strcmp(sc.name, "trfhome") && haveTraffic && haveCol && haveHomer) {   // служебное (№1009, tests/inksep1009): первая машина трафика встаёт, Гомер пешком в sc.a м перед её передом (+2,6 м до бампера), глаз в 14 м перед Гомером, взгляд на машину — сцена не зависит от того, где машина оказалась
          int i_ = 0; while (i_ < traffic.maxCars && traffic.car[i_].mode == TRM_OFF) i_++;
          if (i_ < traffic.maxCars) { TrCar *c_ = &traffic.car[i_]; c_->mode = TRM_PHYS; c_->stillT = 0.f; memset(c_->car.vel, 0, sizeof c_->car.vel);
            float fx_ = sinf(c_->car.h), fz_ = cosf(c_->car.h), gap_ = 2.6f + sc.a, hx_ = c_->car.x + fx_ * gap_, hz_ = c_->car.z + fz_ * gap_, g_ = 200.f; if (!ground(hx_, hz_, g_, 0.f, &g_)) g_ = c_->car.y;
            if (rs.room >= 0 || rg.mem) roomLeave();
            homerPlace(&homer, &world, hx_, g_, hz_, c_->car.h); homerDoor(0, 0.f); chase.init = 0; fly = 0; autodrive = 0;
            eyeOv[0] = c_->car.x + fx_ * 14.f; eyeOv[1] = c_->car.y + trfgoH; eyeOv[2] = c_->car.z + fz_ * 14.f;
            float dx_ = c_->car.x - eyeOv[0], dy_ = c_->car.y + .6f - eyeOv[1], dz_ = c_->car.z - eyeOv[2], l_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_); eyeOv[3] = dx_ / l_; eyeOv[4] = dy_ / l_; eyeOv[5] = dz_ / l_; eyeOvOn = 1;
            char s_[160]; snprintf(s_, sizeof s_, "TRFHOME car=%d model=%.12s at=%.1f,%.1f,%.1f h=%.2f homer=%.1f,%.1f,%.1f t=%.1f\n", i_, traffic.model[c_->model].name, c_->car.x, c_->car.y, c_->car.z, c_->car.h, hx_, g_, hz_, smkT); smkWrite(s_); } }
        else if (!strcmp(sc.name, "msncarnear") && haveCol) {   // служебное (№314 п.1, tests/seat): машина игрока в 6 м сбоку от первой машины миссии с водителем (sc.a — по счёту), смотрит на неё
          const MsnCars *m_ = mvCars(); int want = (int)sc.a, got = 0;
          if (m_) for (int i = 0; i < m_->n && !got; i++) { const McCar *c = &m_->c[i];
            if (!c->on || !c->driver[0] || !strcmp(c->driver, "none") || c->car.destroyed) continue; if (want-- > 0) continue;
            float a = c->car.h, px = c->car.x + cosf(a) * 6.f, pz = c->car.z - sinf(a) * 6.f;
            carPlace(&car, &world, px, c->car.y + .5f, pz, atan2f(c->car.x - px, c->car.z - pz)); chase.init = 0; got = 1;
            { float fx = sinf(a), fz = cosf(a), rx = cosf(a), rz = -sinf(a), ex = c->car.x + fx * 5.5f - rx * 1.6f, ez = c->car.z + fz * 5.5f - rz * 1.6f, ey = c->car.y + 2.2f;   // камера спереди-слева, смотрит на водителя сквозь лобовое
              smkCam[0] = ex; smkCam[1] = ey; smkCam[2] = ez; smkCam[3] = c->car.x - rx * .5f - ex; smkCam[4] = c->car.y + 1.3f - ey; smkCam[5] = c->car.z - rz * .5f - ez; smkCam[6] = 45.f; smkCamOn = 1; }
            char sb[160]; snprintf(sb, sizeof sb, "MSNCARNEAR car=%s driver=%s at=%.1f,%.1f t=%.1f\n", c->name, c->driver, c->car.x, c->car.z, smkT); smkWrite(sb); }
          if (!got) smkWrite("MSNCARNEAR none\n"); }
        else if (!strcmp(sc.name, "trfspawn") && haveTraffic && haveHomer && onFoot()) {   // служебное (№666/№667, tests/seat): машина трафика модели №a (порядок пакета: minivanA, glastruc, schoolbu, ...) стоит справа от Гомера, водительской стороной к нему; затем △ — угон
          int mi = (int)sc.a; if (mi < 0 || mi >= (int)traffic.hd->nModel) mi = 0;
          const TrModel *M = &traffic.model[mi]; float a = homer.camYaw, d = M->halfW + .9f, x = homer.x - cosf(a) * d, z = homer.z + sinf(a) * d;
          TrCar *c = &traffic.car[0]; memset(c, 0, sizeof *c); c->model = mi; c->junc = -1;
          carInit(&c->car, &traffic.params[mi], &world, x, homer.y + .5f, z, a); c->mode = TRM_PHYS; c->stillT = 0.f;
          char sb[128]; snprintf(sb, sizeof sb, "TRFSPAWN model=%s at=%.1f,%.1f t=%.1f\n", M->name, x, z, smkT); smkWrite(sb); }
        else if (!strcmp(sc.name, "carroll1200") && haveCol) {   // служебное (№1200, гейт автосброса): машина игрока на бок — поворот кузова на sc.a градусов вокруг продольной оси, чуть выше земли
          float a_ = sc.a * 3.14159265f / 360.f, c_ = cosf(a_), s_ = sinf(a_); float h_ = car.h, ch_ = cosf(h_ * .5f), shh_ = sinf(h_ * .5f);   // q = q_yaw(h) * q_roll(вокруг оси z тела)
          car.q[0] = ch_ * c_; car.q[1] = -shh_ * s_; car.q[2] = shh_ * c_; car.q[3] = ch_ * s_; car.y += 1.f; car.vel[0] = car.vel[1] = car.vel[2] = 0.f;
          char sb[96]; snprintf(sb, sizeof sb, "CARROLL1200 deg=%.0f at=%.1f,%.1f,%.1f t=%.1f\n", sc.a, car.x, car.y, car.z, smkT); smkWrite(sb); }
        else if (!strcmp(sc.name, "carembed1200") && haveCol) {   // служебное (№1200, гейт автосброса): сдвинуть машину игрока на (dx, dz) без проверки стен — «влезла в текстуру»
          car.x += sc.d; car.z += isnan(sc.b) ? 0.f : sc.b; car.vel[0] = car.vel[1] = car.vel[2] = 0.f;   // flag carembed1200 DX DZ
          char sb[96]; snprintf(sb, sizeof sb, "CAREMBED1200 at=%.1f,%.1f,%.1f t=%.1f\n", car.x, car.y, car.z, smkT); smkWrite(sb); }
        else if (!strcmp(sc.name, "carang")) { char sb[160]; const char *fc_ = mvForcedCar(); snprintf(sb, sizeof sb, "OWNANG t=%.1f onfoot=%d v=%.2f spin2=%.2f forced=%s\n", smkT, onFoot(), car.v, cv.spin[2], fc_ ? fc_ : "-"); smkWrite(sb); }   // №955б (служебное): угол качения колеса своей машины
        else if (!strcmp(sc.name, "trfang") && haveTraffic) {   // №955б (служебное): угол качения колёс и скорость живых машин трафика, расстояние до игрока
          for (int i = 0; i < traffic.maxCars; i++) { const TrCar *c = &traffic.car[i]; if (c->mode == TRM_OFF) continue;
            char sb[160]; snprintf(sb, sizeof sb, "TRFANG t=%.1f i=%d model=%.8s mode=%d v=%.2f ang=%.2f d=%.0f\n", smkT, i, traffic.model[c->model].name, (int)c->mode, c->car.v, c->wheelAng, hypotf(c->car.x - car.x, c->car.z - car.z)); smkWrite(sb); } }
        else if (!strcmp(sc.name, "trfwheel") && haveTraffic && haveHomer && onFoot()) {   // №955б (служебное, tests/wheel955): машина трафика модели №a стоит в 4 м к -Z от Гомера носом к +X, угол качения колёс b рад (боком к камере camrel 0 1.1 1 0 -0.12 -1); тот же кусок кадра с углом b и b+1 — разница = вращение колеса
          int mi = (int)sc.a; if (mi < 0 || mi >= (int)traffic.hd->nModel) mi = 0;
          TrCar *c = &traffic.car[0];
          if (c->model != mi || c->mode == TRM_OFF || fabsf(c->car.x - homer.x) > .5f || fabsf(c->car.z - (homer.z - 4.f)) > .5f) { memset(c, 0, sizeof *c); c->model = mi; c->junc = -1; carInit(&c->car, &traffic.params[mi], &world, homer.x, homer.y + .5f, homer.z - 4.f, 1.5708f); c->mode = TRM_PHYS; c->stillT = 0.f; }
          c->wheelAng = isnan(sc.b) ? 0.f : sc.b;
          char sb[128]; snprintf(sb, sizeof sb, "TRFWHEEL model=%.16s idx=%d ang=%.2f t=%.1f\n", traffic.model[mi].name, mi, c->wheelAng, smkT); smkWrite(sb); }
        else if (!strcmp(sc.name, "trfboom") && haveTraffic && traffic.car[0].mode != TRM_OFF) {   // служебное (№804): машина трафика 0 — сразу после взрыва (остов)
          Car *k_ = &traffic.car[0].car; k_->damage = 1.f; k_->destroyed = 1; k_->deadT = CAR_BOOM_DELAY + sc.a;
          char sb[96]; snprintf(sb, sizeof sb, "TRFBOOM model=%d husk=%d t=%.1f\n", traffic.car[0].model, carShowHusk(k_), smkT); smkWrite(sb); }
        else if (!strcmp(sc.name, "oldtv")) otForce = sc.a < 0.f ? -1 : (int)sc.a;   // №800/№830: служебно — «старый телевизор»: 0 нет, 1 — 100 %, 2..10 — сила 20..100 %, −1 — по настройке
        else if (!strcmp(sc.name, "streetvoff")) sndStreetOff = 1;   // красный контроль правок 180/184
        else if (!strcmp(sc.name, "trfnosolid") && haveTraffic) traffic.noSolid = 1;   // отрицательный контроль: трафик не твёрдый
        else if (!strcmp(sc.name, "trflod") && haveTraffic) traffic.forceLod = 1;   // служебное: дальняя версия машин вблизи (правка Роба 56)
        else if (!strcmp(sc.name, "trfdraw") && sc.a >= 50.f && sc.a <= 200.f) traffic.carDrawDist = sc.a;   // №1202: дальность рисования машин трафика, м (90 — как было)
        else if (!strcmp(sc.name, "trffadetest")) trfFadeTest = sc.a;   // №1202 служебное: постоянная прозрачность всех машин трафика (0 — выкл)
        else if (!strcmp(sc.name, "trffade")) trfFadeOn = sc.a != 0.f;   // №1202: растворение появляющейся машины, 0 — без него
        else if (!strcmp(sc.name, "trflog")) trLog = 1;
        else if (!strcmp(sc.name, "doorwalk")) rs.needAction = 0;   // отрицательный контроль правки 41: дверь по триггеру без △ (как раньше)
        else if (!strcmp(sc.name, "artgal")) {   // служебная витрина предметов миссий: грузит их записи msnart.shr (как LoadP3DFile)
          static const char *const f[] = {"art/missions/level01/ibucket.p3d", "art/missions/level01/scien.p3d", "art/missions/level01/flanpic.p3d",
            "art/missions/level01/tomat.p3d", "art/missions/level01/poster_t.p3d", "art/missions/level01/coolr.p3d", "art/missions/level01/tux.p3d",
            "art/missions/level01/bonebox.p3d", "art/missions/generic/missgen.p3d", "art/l1_terra.p3d"};   // l1_terra — стрелки на дороге (правка 122)
          for (unsigned i = 0; i < sizeof f / sizeof *f; i++) msnartLoad(f[i], 1);
          artGal = 1;
          if (sc.a == 7.f) { msnartLoad("art/missions/level07/ufobeam.p3d", 7); msnartLoad("art/missions/level07/ufo.p3d", 7); artGal = 7; } }   // №805: «flag artgal 7» — тарелка и её луч перед машиной
        else if (!strcmp(sc.name, "aoact")) aoAct = 1;   // служебное: △ объекту уровня (Гомер в объёме выключателя: foot + wait)
        else if (!strcmp(sc.name, "aolog") && haveAo) {   // объекты с действием и живым объёмом: кадр клипа, центр последнего объёма (одна запись)
          char s[1024]; int n = snprintf(s, sizeof s, "AOLOG want=%d t=%.1f", aoWantsAction(&ao), smkT);
          for (unsigned k = 0; k < ao.nobj && n < (int)sizeof s - 64; k++) {
            const AoObj *o = &ao.obj[k]; if (!o->action || !o->nVol) continue;
            const AoVol *V = &ao.vol[o->firstVol + o->nVol - 1]; const float *c = V->kind == 0 ? world.box[V->col].c : world.cyl[V->col].c;
            n += snprintf(s + n, sizeof s - n, " | %u a%d f=%.0f p=%d y=%.2f", k, o->action, ao.run[k].f, ao.run[k].playing, c[1]); }
          snprintf(s + n, sizeof s - n, "\n"); smkWrite(s); }
        else if (!strcmp(sc.name, "autostraight")) { autodrive = 2; adT = 0.f; adStop = sc.a; }
        else if (!strcmp(sc.name, "lvreset980old")) lvReset980 = sc.a == 0.f;   // №980: 1 — смена уровня не закрывает будку, лавку, гардероб (красный контроль)
        else if (!strcmp(sc.name, "msnnpc984old")) gameMsnNpc984Old = sc.a != 0.f;   // №984: 1 — поиск персонажа по имени перебором каждый раз (как было)
        else if (!strcmp(sc.name, "msnnpctime")) gameMsnNpcTime = sc.a != 0.f;   // №984 служебно: мкс на поиск персонажа в строке MSNNPC984
        else if (!strcmp(sc.name, "msnnpcchk")) gameMsnNpcChk = sc.a != 0.f;   // №984 служебно: сверка кэша с перебором
        else if (!strcmp(sc.name, "msnnpcstat")) { char l_[160]; snprintf(l_, sizeof l_, "MSNNPC984 old=%d calls=%u miss=%u bad=%u lookup_us=%u t=%.1f\n", gameMsnNpc984Old, gameMsnNpcCalls, gameMsnNpcMiss, gameMsnNpcBad, gameMsnNpcUs, smkT); smkWrite(l_); gameMsnNpcCalls = gameMsnNpcMiss = gameMsnNpcBad = gameMsnNpcUs = 0; }
        else if (!strcmp(sc.name, "golevel") && sc.a >= 1.f && sc.a <= 7.f && levelAvailable((int)sc.a)) { autodrive = 0; toLevel = (int)sc.a; }   // тур всех уровней (08.10): тот же путь, что «уровень пройден» (levelAdvance)
        else if (!strcmp(sc.name, "cellpoolold")) cpOld = sc.a != 0.f;   // 1Б: «flag cellpoolold 1» — клетки прямо в кучу (сравнение в одном прогоне)
        else if (!strcmp(sc.name, "cellpooltails")) { cpOld = 0; cpCellsOff = sc.a != 0.f; }   // 1Б замер: пул только для хвостов клеток
        else if (!strcmp(sc.name, "cellpoolcls")) cpSetTable(sc.a >= 1.5f ? 'b' : 'a');   // 1Б: 1 — классы 16…192 КБ, 2 — мельче (при живых плитах не меняется)
        else if (!strcmp(sc.name, "autodrive")) { autodrive = 1; adT = 0.f; adStop = 0.f; }
        else if (!strcmp(sc.name, "wp")) { if (wpN < 32) { wpX[wpN] = sc.d; wpZ[wpN] = sc.b; wpN++; } }   // путевая точка (аудит 01.10)
        else if (!strcmp(sc.name, "wpv")) wpV = sc.a;
        else if (!strcmp(sc.name, "wpclear")) wpN = wpI = 0;
        else if (!strcmp(sc.name, "radartgt")) { rtOn1091 = sc.d > -90000.f; rtX1091 = sc.d; rtZ1091 = sc.b; }   // №1091 служебно: «flag radartgt X Z» — значок цели миссии на радаре в точке (X<-90000 — убрать)
        else if (!strcmp(sc.name, "wpgo")) { autodrive = 3; adT = 0.f; adStop = 0.f; wpI = 0; }
        else if (!strcmp(sc.name, "adclockold")) adClockOld = sc.a != 0.f;   // №914: отрицательный контроль — часы автопилота дважды за кадр (прежний код)
        else if (!strcmp(sc.name, "adseatold")) adSeatOld = sc.a != 0.f;   // №914: отрицательный контроль — flag drive пешком не садит Гомера
        else if (!strcmp(sc.name, "drive") && haveCol && haveTraffic) {
          if (!adSeatOld && mvTalkActive()) { mvTalkInput(PSP_CTRL_CROSS); if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PILOT talkskip t=%.1f\n", smkT); smkWrite(s_); } }   // №914: разговор вступления (L2: Френк) держит мир — сперва ✕ «пропустить», как у игрока
          if (!adSeatOld && haveHomer && onFoot()) {   // №914: Гомер пешком (машина уровня стоит вдали, в L2/L4/L6/L7 — в координатах старта L1): машина на ближнюю полосу к нему и Гомер за руль, как RESET CAR
            float fx_ = car.x, fz_ = car.z; if (rs.room >= 0 || rg.mem) roomLeave();
            carPlace(&car, &world, homer.x, homer.y + .5f, homer.z, homer.h); gameReset(&G);
            x = car.x; y = car.y; z = car.z; h = car.h; v = vy = 0; chase.init = 0; fly = 0; props.havePreviousCar = 0; prevDmg = car.damage;
            { char s_[160]; snprintf(s_, sizeof s_, "PILOT seat from=%.1f,%.1f to=%.1f,%.1f t=%.1f\n", fx_, fz_, car.x, car.z, smkT); smkWrite(s_); } }
          autodrive = 4; adT = 0.f; adStop = sc.a > 0.f ? sc.a : 60.f; smkPilotInit(&smkPilot, 1u + (unsigned)(smkT * 10.f), 16.f); smkPilot.lx = car.x; smkPilot.lz = car.z; smkPilot.props = haveProps ? &props : NULL; smkPilot.sp = haveSP ? &sprops : NULL; smkPilotLogT = 0.f;   // TEST14 §5: автопилот по дорогам трафика на SECS с
          char s_[96]; snprintf(s_, sizeof s_, "PILOT start t=%.1f secs=%.0f\n", smkT, adStop); smkWrite(s_); }
        else if (!strcmp(sc.name, "walk") && haveHomer && haveTraffic) { autodrive = 5; adT = 0.f; adStop = sc.a > 0.f ? sc.a : 25.f; smkWalkInit(&smkWalk); smkWalk.props = haveProps ? &props : NULL; smkWalk.sp = haveSP ? &sprops : NULL; smkPilotLogT = 0.f;   // TEST14 §5: Гомер по тротуару на SECS с
          char s_[96]; snprintf(s_, sizeof s_, "PILOT walk start t=%.1f secs=%.0f\n", smkT, adStop); smkWrite(s_); }
        else if (!strcmp(sc.name, "stop")) autodrive = 0;
        else if (!strcmp(sc.name, "fadeout")) mvServiceFade(1, sc.a > 0.f ? sc.a : 1.f);   // служебное: затемнение миссии (правка 122)
        else if (!strcmp(sc.name, "fadein")) mvServiceFade(0, sc.a > 0.f ? sc.a : .5f);
        else if (!strcmp(sc.name, "msnfail")) mvServiceFail();   // №572: экран провала миссии
        else if (!strcmp(sc.name, "failynpulseold")) uiFailynPulseOld(sc.a != 0.f);   // №572: контроль отката FAILYN_PULSE_OLD
        else if (!strcmp(sc.name, "celebrate")) { int ok_ = haveHomer ? homerCelebrate(&homer, sc.a != 0.f) : -1; if (smkOn) { char s_[200]; snprintf(s_, sizeof s_, "CELEBRATE big=%d ok=%d state=%d haveHomer=%d clip=%s t=%.1f\n", sc.a != 0.f, ok_, haveHomer ? (int)homer.state : -1, haveHomer, haveHomer && homer.clip >= 0 ? homer.m->c[homer.clip].name : "-", smkT); smkWrite(s_); } }   // служебное (смоук): радость героя без миссии, 0 — малая, 1 — большая (№1158 за рулём)
        else if (!strcmp(sc.name, "msnflash")) mvServiceFlash((int)sc.a);   // служебное: вспышка конца миссии (1 — бонусной), правка 122
        else if (!strcmp(sc.name, "unlockmsg")) mvServiceUnlock("cletu_v");   // служебное: окно «Congratulations! You've unlocked …» (правка 122)
        else if (!strcmp(sc.name, "hnrmusold")) hnrMusOld = sc.a != 0.f;   // правка 266: прежний конец погони (музыка не возвращалась)
        else if (!strcmp(sc.name, "policefxold")) policeFxOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "policeflash")) policeFxOld = sc.a == 0.f;   // 15.10: 1 — вспышки мигалки включены (по умолчанию выключены)
        else if (!strcmp(sc.name, "roadarrowold")) roadArrowOld = sc.a != 0.f;   // №606: 1 — стрелка на дороге как раньше
        else if (!strcmp(sc.name, "deferfreeold")) dfOld = sc.a != 0.f;       // 15.10: 1 — освобождать сразу, как раньше
        else if (!strcmp(sc.name, "overlay")) testOverlayOn = sc.a != 0.f;     // TEST16E: оверлей для видео с телефона
        else if (!strcmp(sc.name, "ddsteadyold")) ddSteadyOld = sc.a != 0.f;   // дальность «ровно» (по умолчанию) или как раньше
        else if (!strcmp(sc.name, "neonglow")) neonGlowOn = sc.a != 0.f;
        else if (!strcmp(sc.name, "neonarch957")) neonArch957 = sc.a != 0.f;   // №957б: 1 — крупное свечение предмета (арка, вывеска) без блика, 0 — как было (красный контроль)
        else if (!strcmp(sc.name, "neonsign950")) { neonSign950 = sc.a != 0.f; slLightResetReq = 1; }   // №950: 1 — без плашек свечения вывесок, 0 — как были (красный контроль; таблица строится заново)
        else if (!strcmp(sc.name, "ddfloor") && sc.a >= 100.f && sc.a <= 250.f) ddFloorOver = sc.a;   // TEST20: пол регулятора дальности (м), 0 — по умолчанию
        else if (!strcmp(sc.name, "tqdump")) tqDumpTex = (int)sc.a;
        else if (!strcmp(sc.name, "tqskip")) tqSkipTex = (int)sc.a;   // TEST20: скрыть полупрозрачное текстуры N (-1 — снять)
        else if (!strcmp(sc.name, "tqfast")) tqFastRt = sc.a != 0.f;            // TEST19: A/B перегруппировки полупрозрачного
        else if (!strcmp(sc.name, "glowbias")) { glowBias = (int)sc.a; msnartGlowBias = glowBias; curFlags = -1; }   // TEST19: сдвиг глубины света       // 15.10: 1 — неон и свечения фонарей как раньше
        else if (!strcmp(sc.name, "drawdist") && sc.a >= 50.f && sc.a <= 200.f) drawDistMul = sc.a * .01f;   // 15.10: дальность города в процентах от базовой (115/250 м)
        else if (!strcmp(sc.name, "nobust")) { hnrNoBust = sc.a > 0.f; }   // правка 433: тест — арест выключен (ветка limbo не должна упираться в BUSTED автопилота)
        else if (!strcmp(sc.name, "hnr")) { hnrHeat = fminf(1.f, fmaxf(0.f, sc.a / 100.f)); hnrCalm = 0.f; }   // служебное: шкала Hit & Run (как SetHitAndRunMeter)
        else if (!strcmp(sc.name, "hnrbust")) hnrForceBust = 1;   // служебное: арест в этом кадре (правка 227)
        else if (!strcmp(sc.name, "coins") && haveCoins) coins.collected = (int)fmaxf(0.f, sc.a);              // служебное: монеты (проверка штрафа)
        else if (!strcmp(sc.name, "fpeprobe")) fpeProbe();   // правка 252: 0/0 в главном потоке (гейт tests/crash_f10/hw.sh)
        else if (!strcmp(sc.name, "testdmg")) { car.damage = prevDmg = sc.d < 0.f ? .9f : fminf(1.f, fmaxf(0.f, sc.d)); car.destroyed = (car.damage >= 1.f); }   // «flag testdmg 0.50» — урон 0..1; без аргумента — 0.9
        else if (!strcmp(sc.name, "testboom")) { car.damage = prevDmg = 1.f; car.destroyed = 1; }
        else if (!strcmp(sc.name, "barrier1175") && haveSP) {   // №1175 служебно: «flag barrier1175 [N]» — удар машиной (событие VEHICLE_HIT) по N ближайшим барьерам с вызовом BREAK (donotenter, gatesmasha, переезд, туннельный забор); строка BARRIER1175 в smoke.txt
          int want_ = sc.a > 0.f ? (int)sc.a : 1, done_ = 0; unsigned used_[8] = {0};
          for (int k_ = 0; k_ < want_ && k_ < 8; k_++) {
            int bi_ = -1; float bd_ = 1e30f;
            for (unsigned i_ = 0; i_ < sprops.ninst; i_++) {
              const SpType *T_ = &sprops.type[sprops.inst[i_].type]; int brk_ = 0;
              for (unsigned s_ = 0; s_ < T_->nState && !brk_; s_++) { const SpState *S_ = &sprops.state[T_->firstState + s_]; for (unsigned c_ = 0; c_ < S_->nCall; c_++) if (sprops.call[S_->firstCall + c_].op == SP_OP_BREAK) brk_ = 1; }
              int dup_ = 0; for (int u_ = 0; u_ < k_; u_++) dup_ |= used_[u_] == i_ + 1;
              if (!brk_ || dup_ || !sprops.run[i_].alive || sprops.run[i_].state != 0) continue;
              float dx_ = sprops.run[i_].x - car.x, dz_ = sprops.run[i_].z - car.z, d_ = dx_ * dx_ + dz_ * dz_;
              if (d_ < bd_) { bd_ = d_; bi_ = (int)i_; }
            }
            if (bi_ < 0) break;
            used_[k_] = (unsigned)bi_ + 1; done_ += spEvent(&sprops, (unsigned)bi_, SP_EV_VEHICLE_HIT) ? 1 : 0;
            if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "BARRIER1175 inst=%d type=%u dist=%.1f old=%d live=%d t=%.1f\n", bi_, (unsigned)sprops.inst[bi_].type, sqrtf(bd_), BARRIERFX1175_OLD, fxCount(), smkT); smkWrite(s_); }
          } (void)done_; }
        else if (!strcmp(sc.name, "bitfx1161") && haveSP) {   // №1161 служебно: «flag bitfx1161 N» — N = SPFX_* (1 пыль, 2 щепа, 4 листья, 9 сено, 10 томаты, 11 обломки): 8 вызовов пресета в 5 м перед машиной, как окно EFX без родной системы; строка BITFX1161 в smoke.txt
          int ps_ = (int)sc.a, c0_ = fxCount(); float bx_ = car.x + sinf(car.h) * 5.f, bz_ = car.z + cosf(car.h) * 5.f;
          for (int k_ = 0; k_ < 8; k_++) { SpFxOut o_ = {(uint8_t)ps_, (uint8_t)(k_ == 0), -1, bx_, car.ry + .6f, bz_, 0.f, 1.f, 0.f}; spFxOut(NULL, &o_); }
          if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "BITFX1161 preset=%d calls=8 particles=%d old=%d t=%.1f\n", ps_, fxCount() - c0_, BITFX1161_OLD, smkT); smkWrite(s_); } }
        else if (!strcmp(sc.name, "phonehusk3d")) phoneViewHusk3DForce(sc.a > 0.f);   // №569: показ 3D-остова в будке (для тестов / контроля)
        else if (!strcmp(sc.name, "lvlogoson")) { lvLogosOn = sc.a != 0.f; char l_[64]; snprintf(l_, sizeof l_, "LVLOGOS %d t=%.1f\n", lvLogosOn, smkT); smkWrite(l_); }   // №1199
        else if (!strcmp(sc.name, "crashnow")) { if (CRASHNOW_OK && sc.a != 0.f) { crashNowAt = sceKernelGetSystemTimeLow() + 1500000u + 1u; smkWrite("CRASHNOW armed\n"); } }   // №1208в: «flag crashnow 1» — вылет через 1,5 с (экран вылета на железе)
        else if (!strcmp(sc.name, "realend")) { realLevelEnd = (int)sc.a; char l_[64]; snprintf(l_, sizeof l_, "REALEND %d t=%.1f\n", realLevelEnd, smkT); smkWrite(l_); }   // №1192
        else if (!strcmp(sc.name, "mefx")) { char fm_[48]; fxMeStateStr(fm_, sizeof fm_); fxMeEnable(sc.a != 0.f); char l_[96]; snprintf(l_, sizeof l_, "MEFX flag on=%d state_before=%s t=%.1f\n", sc.a != 0.f, fm_, smkT); smkWrite(l_); }   // №1186+: «flag mefx 0/1» — частицы на ME / на главном (служба — файл mefx или mefxpair; без службы ничего не меняется)
        else if (!strcmp(sc.name, "mehand")) { audioMeHand(sc.a != 0.f); }   // №1100: «flag mehand 0/1» — окно потоков в задании 1024 Б / 2048 Б (A/B в туре)
        else if (!strcmp(sc.name, "meprof")) { meProf = sc.a > 0.f || sc.a == 0.f; audioProfKern(meProf); }   // правка 313: «flag meprof» — замер для ME, «-1» — выкл
        else if (!strcmp(sc.name, "repair")) { car.damage = 0.f; car.destroyed = 0; prevDmg = 0.f; wasDestroyed = 0;
          if (haveHomer && haveCol && onFoot()) { HomerCar hc; homerCarFrom(&hc, &car, cv.lift); homerSetInCar(&homer, &hc); homerDoor(0, 0.f); chase.init = 0; } }   // выброшенный взрывом — снова за руль
        else if (!strcmp(sc.name, "hlodcull")) hlodCull = sc.a != 0.f;
        else if (!strcmp(sc.name, "fogsoft")) fogSoft = sc.a >= 1.f && sc.a <= 6.f ? sc.a : 1.f;   // туман города мягче (конец на drawDist × N), см. hlod.h
        else if (!strcmp(sc.name, "hlodafter")) { hlodAfter = sc.a != 0.f; if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "HLOD after %d\n", hlodAfter); smkWrite(s_); } }   // №704: слой после города (дешевле по заливке)
        else if (!strcmp(sc.name, "hlod")) { hlodOn = (int)sc.a; if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "HLOD flag %d\n", hlodOn); smkWrite(s_); } }   // HLOD: 0 выкл, 1 без тумана, 2 туман города, 3 мягкий (hlod.h)
        else if (!strcmp(sc.name, "hztrace")) hzTrace = (int)sc.a;
        else if (!strcmp(sc.name, "hulldiag")) thDiag = (int)sc.a;
        else if (!strcmp(sc.name, "collverify")) collVerify = sc.a != 0.f;   // разбор L3/L5: сверка списка близких экземпляров коллекции с полным перебором (COLLVER)
        else if (!strcmp(sc.name, "cammove")) { cmV = sc.a; cmP[0] = cmP[1] = 0.f; }
        else if (!strcmp(sc.name, "camyaw")) { cmYaw = sc.a; cmA = 0.f; }
#if !SHR_REL_SMK
        else if (!strcmp(sc.name, "flick")) { if (flickOn && sc.a == 0.f) flickDump(smkScene); flickOn = (int)sc.a; flickHave = 0; flickN = 0; }   // №932: датчик мерцания (smoke.h flickStep), строки FLICK/FLICKB
#endif
        else if (!strcmp(sc.name, "fogoff1042")) { fogOff1042On = sc.a != 0.f; fogK = -1.f; }   // №1042
        else if (!strcmp(sc.name, "far1042")) far1042On = sc.a != 0.f;   // №1042
        else if (!strcmp(sc.name, "fogcity1042")) { fogCity1042On = sc.a != 0.f; fogK = -1.f; }   // №1042
        else if (!strcmp(sc.name, "inkfar1043")) { inkFar1043On = sc.a != 0.f; tlSelReset = 1; }   // №1043
        else if (!strcmp(sc.name, "far130")) hz130On = sc.a != 0.f;   // №913: полная прорисовка 130 м вместо 150 в зоне «130»
        else if (!strcmp(sc.name, "horizon")) horizonOn = (int)sc.a;   // Горизонт: плоские силуэты вкл/выкл (A/B в туре); данные — файл "horizon.shr" уровня (horizon.h)
        else if (!strcmp(sc.name, "farroad934")) road934On = sc.a != 0.f;   // №934: дальние дороги и мосты вкл/выкл (A/B в туре); данные — файл "road934.shr" уровня (road934.h), ship_files находит имя по этой строке
        else if (!strcmp(sc.name, "farroad934cache")) road934Reuse = sc.a != 0.f;   // №934: 1 — отбор блоков переиспользуется, пока глаз почти на месте (по умолчанию), 0 — каждый кадр заново (замер худшего кадра)
        else if (!strcmp(sc.name, "farroad934dbg")) road934Dbg = (int)sc.a;   // №934, служебное: слой сплошным пурпуром (гейт tests/road934)
        else if (!strcmp(sc.name, "pano917punch")) pano917Punch = sc.a != 0.f;   // №917: пробивка глубины ленты по индексу панорамы вкл/выкл (A/B)
        else if (!strcmp(sc.name, "pano917mix")) pnoMixW = sc.a >= 1.f ? sc.a : PNO_MIXW;   // №917в: окно смешивания двух панорам, м
        else if (!strcmp(sc.name, "pano917any")) pano917Any = sc.a != 0.f;   // №917б: 0 — кольцо только при дальности 150 м (как в d447b1d9)
        else if (!strcmp(sc.name, "pano917")) pano917On = (int)sc.a;   // №917: 0 — без кольца, 1 — кольцо, 2 — служебное: город 150 м без кольца (красный контроль)
        else if (!strcmp(sc.name, "silh")) silhOn = sc.a != 0.f;   // Загрузка 3: силуэты за дальностью вкл/выкл (A/B в туре); данные — файл "silh.shr" уровня (silh.h), ship_files находит имя по этой строке
        else if (!strcmp(sc.name, "hzcol997")) hzCol997Old = sc.a == 0.f;   // №997: 0 — лента прежнего тона, 1 — цвет фасадов дальних домов (хвост HCL1 horizon.shr)
        else if (!strcmp(sc.name, "hzworld")) hzWorldOn = sc.a != 0.f;   // №934: мировая лента вкл/выкл на лету (A/B гейта)
        else if (!strcmp(sc.name, "hzlm")) hzLmOn = sc.a != 0.f;   // №928: ориентиры дали вкл/выкл на лету (A/B гейта)
        else if (!strcmp(sc.name, "regprememold")) regPreMemOld = sc.a != 0.f;   // Б TEST57: зазор по памяти для регионов впрок вкл/выкл на лету (A/B)
        else if (!strcmp(sc.name, "regprehyst")) regPreHyst = sc.a != 0.f;   // гистерезис REGPRE вкл/выкл на лету (A/B)
        else if (!strcmp(sc.name, "roadqold")) { roadqOld = sc.a != 0.f; if (roadqOld) rqCur = -1; }   // №864: A/B в одном туре на одном маршруте (1 — клетки по дороге выключены)
        else if (!strcmp(sc.name, "gatepre")) { gatePreOn = sc.a != 0.f; if (!gatePreOn) regPre = 0; }   // Загрузка 3: регионы ворот впрок вкл/выкл на лету (A/B в туре)
        else if (!strcmp(sc.name, "loadacc")) loadAccOn = sc.a != 0.f;   // Загрузка 3: учёт подгрузки раз в секунду
        else if (!strcmp(sc.name, "warmhere")) { float c_[3] = {car.x, car.y, car.z}; lqn = 0; __asm__ volatile("" ::: "memory"); while (loaderBusy) sceKernelDelayThread(1000);   // загрузка 2: служебное, как перенос миссии: круг клеток вокруг машины сразу (после repair/telепорта сценария)
          cellsRingAt(c_, 0, &fbp0); stInit = 0; smkSettle(); }
        else if (!strcmp(sc.name, "noeject")) G.noEject = 1;              // отрицательный контроль: взрыв не выбрасывает
        else if (!strcmp(sc.name, "noiris")) transitOff(1);               // отрицательный контроль: без ириса
        else if (!strcmp(sc.name, "deathold")) gameDeathOld(1);           // правка 349, отрицательный контроль: зоны DEATH как до неё
        else if (!strcmp(sc.name, "deathfootold")) gameDeathFootOld(1);   // ДОБИВКА 18 (В86), красный: пешком курс прежний, камера за спиной
        else if (!strcmp(sc.name, "pauseslow")) uiPauseSlow(sc.a);        // служебное: анимация паузы медленнее в N раз (снимки середины)
        else if (!strcmp(sc.name, "irisslow")) transitSlow(sc.a);         // служебное: ирис медленнее в N раз (снимки середины)
        else if (!strcmp(sc.name, "caririsold")) { homerIrisCarOld = sc.a != 0.f; smkWrite(homerIrisCarOld ? "IRISCAR old\n" : "IRISCAR new\n"); }   // правка 333: красный контроль — клипы двери и у машин с SetIrisTransition
        else if (!strcmp(sc.name, "iristest")) { frames += transitClose(&fbp0, "test") / (longView ? 2 : 1); smkSettle(); }   // служебное: ирис на месте
        else if (!strcmp(sc.name, "feshot")) { feShot = 1; feShotAt = sc.a; }   // гейт экранов: снимок LoadingFE при следующем выходе в меню
        else if (!strcmp(sc.name, "long")) longView = 1;
        else if (!strcmp(sc.name, "noreg")) { regOff = 1; smkWrite("REG off\n"); }   // отрицательный контроль правки 95: все регионы, без PVS
        else if (!strcmp(sc.name, "regon")) { regOff = 0; smkWrite("REG on\n"); }
        else if (!strcmp(sc.name, "reglog")) { char s[96]; snprintf(s, sizeof s, "REG eye %.1f %.1f mask %04x cut %d drawn %d\n", eyeX, eyeZ, regEye, regCut, nd); smkWrite(s); }
        else if (!strcmp(sc.name, "flyin")) msncamStart((int)sc.a);   // гейт tests/camera/msncam: камера старта миссии N
        else if (!strcmp(sc.name, "tfsm95old")) { tfsm95Old = sc.a != 0.f; shopHeal1233Wire(); }   // Build 95 П2: 1 — переходы прежним путём
        else if (!strcmp(sc.name, "deathring1239old")) deathRing1239Old = sc.a != 0.f;   // Build 95 П2: 1 — клетки у точки возврата стримом, как было
        else if (!strcmp(sc.name, "boothfsm95old")) boothFsm95Old = sc.a != 0.f;   // Build 95 П2: 1 — тело будки прежнее (main_actbooth.h)
        else if (!strcmp(sc.name, "tfnofit")) transitNoFit = sc.a != 0.f;   // служебно: transitPlan всегда «не хватает» (путь прелоадера при !fits)
        else if (!strcmp(sc.name, "carhard96")) carHard96On = sc.a != 0.f;   // Build 96 (1250, решение Роба): flag carhard96 0 — прежние надпись и подмена машины (красный контроль)
        else if (!strcmp(sc.name, "carvis96")) carVis96 = sc.a != 0.f;   // Build 96 (1250): flag carvis96 0 — как было (красный контроль)
        else if (!strcmp(sc.name, "carhide96")) carHide96 = (int)sc.a;   // Build 96 служебно (1250): 1 — модель машины игрока в кадре «не пришла» (сторож откатит на машину уровня)
        else if (!strcmp(sc.name, "carfail96")) carFailSim96 = (int)sc.a;   // Build 96 служебно (1250): N отказов подряд при вызове машины из будки
        else if (!strcmp(sc.name, "irismodal1259")) irisModal1259Old = sc.a == 0.f;   // Будка-93 (1259): flag irismodal1259 0 - экран провала под ирисом, как было (красный контроль)
        else if (!strcmp(sc.name, "irisstuck1258")) irisStuck1258Old = sc.a == 0.f;   // Будка-93 (1258): flag irisstuck1258 0 - сторожа вечного ириса нет (красный контроль)
        else if (!strcmp(sc.name, "irisdt0")) irisDt0Test1258 = sc.a != 0.f;   // Будка-93 служебно (1258): ирису отдаётся dt = 0 (как в модальном экране) - проверка сторожа
        else if (!strcmp(sc.name, "sceneboost1257")) sceneBoost1257 = sc.a != 0.f;   // Будка-93 (1257): flag sceneboost1257 0 - клетка под ждущим героем без головы очереди (красный контроль)
        else if (!strcmp(sc.name, "gagholdcap1257")) gagHoldCap1257Old = sc.a == 0.f;   // Будка-93 (1257): flag gagholdcap1257 0 - гэги держат ирис до предела (красный контроль)
        else if (!strcmp(sc.name, "gagpend1257")) collGagPend1257Old = sc.a == 0.f;   // Будка-93 (1257): flag gagpend1257 0 - счёт ожидания гэгов как был (красный контроль)
        else if (!strcmp(sc.name, "cleanreload96")) cleanReload96 = sc.a != 0.f;   // Build 96: flag cleanreload96 0 — после лестницы сразу SIMPLIFY (красный контроль)
        else if (!strcmp(sc.name, "lvldr96")) lvLdr96 = sc.a != 0.f;   // Build 96: flag lvldr96 0 — LEVEL_NEXT/LEVEL_START прежним телом (красный контроль)
        else if (!strcmp(sc.name, "tplanplace96")) tplanPlace96 = sc.a != 0.f;   // Build 96: flag tplanplace96 0 — план комнаты мерит только кучу (красный контроль)
        else if (!strcmp(sc.name, "tfbonusfail")) tfFailBonus = (int)sc.a;   // Build 96 служебно: отказ памяти на BONUS
        else if (!strcmp(sc.name, "tfmsnfail")) tfFailMsn = (int)sc.a;   // Build 96 служебно: отказ памяти на MISSION_SELECT (пакет меню)
        else if (!strcmp(sc.name, "tfmenufail")) tfFailMenu = (int)sc.a;   // Build 96 служебно: отказ памяти на MENU_FROM_GAME (гостиная)
        else if (!strcmp(sc.name, "tffail")) tfFailTest = (int)sc.a;   // служебно: следующие N попыток LOAD машины переходов отказывают
        else if (!strcmp(sc.name, "moviefail")) movieFailTest1239 = (int)sc.a;   // Будка-93 (1239): следующие N открытий ролика отказывают
        else if (!strcmp(sc.name, "movie1239old")) movie1239Old = sc.a != 0.f;   // 1 — как было (молчаливый пропуск)
        else if (!strcmp(sc.name, "shopfail")) { shopFailTest1233 = shopViewFailTest = (int)sc.a; }   // Будка-93 (1233 T08/T09): следующие N открытий пакета лавки и гардероба отказывают
        else if (!strcmp(sc.name, "shopheal1233old")) { shopHeal1233Old = sc.a != 0.f; shopHeal1233Wire(); }   // 1 — тихая отмена, как было
        else if (!strcmp(sc.name, "loadcord1233old")) loadCord1233Old = sc.a != 0.f;   // Будка-93 (1233 T01/T02): 1 — чёрный экран загрузки при отказе газеты, как было
        else if (!strcmp(sc.name, "t03old")) t03Old = sc.a != 0.f;   // Будка-93 (1233 T03): 1 — как было (газета без шнура, второй ирис)
        else if (!strcmp(sc.name, "t03double")) t03DoubleArm = sc.a != 0.f;   // служебно: как только ирис «msn» на экране — Continue снимает его и тут же второй перенос (1228)
        else if (!strcmp(sc.name, "transitplanold")) transitPlanOld = sc.a != 0.f;   // Будка-93 (1233): 1 — без строк TPLAN, будка прежним выражением
        else if (!strcmp(sc.name, "booth1225old")) booth1225Old = sc.a != 0.f;   // Будка-93 (1225): 1 — будка как была (сцена, пересборка, возврат, наезд)
        else if (!strcmp(sc.name, "camtest1225")) msncamStartPhase("m3sd", "mission3camShape");   // служебно: заявка камеры старта миссии 3 (проверка окна без наезда)
        else if (!strcmp(sc.name, "gaghold1226old")) gagHold1226Old = sc.a != 0.f;   // Будка-93 (1226): 1 — ирис раскрывается, не ожидая гэгов комнаты
        else if (!strcmp(sc.name, "dooriris1226old")) doorIris1226Old = sc.a != 0.f;   // Будка-93 (1226): 1 — комната читается до ириса, как было
        else if (!strcmp(sc.name, "startcam")) startCamSvc = sc.a > 0.f;   // правка 40: «flag startcam 1» — камеры старта фаз и в служебном запуске
        else if (!strcmp(sc.name, "rollps2")) { carRollGain = sc.a <= 0.f ? 0.f : sc.a == 1.f ? CAR_ROLLPS2_GAIN : fminf(sc.a, 1.5f); char s[64]; snprintf(s, sizeof s, "ROLLPS2 %.2f\n", carRollGain); smkWrite(s); }   // №388: «flag rollps2 N» для A/B в туре (0 выкл, 1 = 0,22, иначе число, м); только сценарий, без флага — как файл rollps2 или прежнее
        else if (!strcmp(sc.name, "zoneoff")) { zoneOff = 1; smkWrite("ZONECAM off\n"); }
#ifndef CAR_ZONE_CAM_OLD
        else if (!strcmp(sc.name, "carzoneoff")) { carZoneOff = sc.d != 0.f; smkWrite("CARZONE off\n"); }   // №408: красный контроль
#endif   // отрицательный контроль: без камер зон (как до правки)
        else if (!strcmp(sc.name, "phoneplant")) phonePlant = 1;
        else if (!strcmp(sc.name, "ggnear")) ggNearOn = sc.a != 0.f;
        else if (!strcmp(sc.name, "nosave")) { saveBlocked = sc.d != 0.f; svmBlock(saveBlocked); }   // d — сырое число: «flag nosave» без числа = включить   // автотур: записи сохранений и настроек выкл.
        else if (!strcmp(sc.name, "bbtest")) bbOp("player", "BBTEST_LAST_LINE", 1);   // гейт tools/bb_sync_gate.sh: синхронный по просьбе вызов, bbOp снижает его до фоновой записи; затем «flag freeze 7» — ящик обязан хранить эту строку
        else if (!strcmp(sc.name, "bbsyncold")) bbSyncOld = sc.a != 0.f;   // аудит 16.10: «flag bbsyncold 1» — синхронные записи ящика как раньше (красный контроль/A-B), 0 — по-новому
        else if (!strcmp(sc.name, "mecity")) { ccFlagOff = sc.a == 0.f; if (sc.a != 0.f) ccOn = 1; }   // №653: «flag mecity 0» — город на главном (A/B), «flag mecity 1» — на ME
        else if (!strcmp(sc.name, "mecitycmp")) ccCmp = sc.a != 0.f;     // №653: сверка результата ME с главным (строка MECITYCMP)   // №634 служебно
        else if (!strcmp(sc.name, "camclipdump")) camClipLog = 1;   // №634 служебно
        else if (!strncmp(sc.name, "eye", 3) && sc.name[3]) { const char *k_ = sc.name + 3; float v_ = sc.a != 0.f || isnan(sc.d) ? sc.a : sc.d;
          if (!strcmp(k_, "x")) { eyeOv[0] = v_; if (smkOn) { char l_[64]; snprintf(l_, sizeof l_, "EYEX t=%.2f x=%.1f\n", smkT, v_); smkWrite(l_); } } else if (!strcmp(k_, "y")) eyeOv[1] = v_; else if (!strcmp(k_, "z")) eyeOv[2] = v_; else if (!strcmp(k_, "dx")) eyeOv[3] = v_;
          else if (!strcmp(k_, "dy")) eyeOv[4] = v_; else if (!strcmp(k_, "dz")) eyeOv[5] = v_; else if (!strcmp(k_, "on")) eyeOvOn = (int)v_; }
        else if (!strcmp(sc.name, "camcrossold")) camCrossOld = sc.a != 0.f;   // CAMCROSS: 1 — глаз не проходит сквозь поверхности, пройденные опорой (прежний код: рывок глаза у двора Симпсонов)
        else if (!strcmp(sc.name, "camnfreuseold")) camNfReuseOld = sc.a != 0.f;   // №924: ближняя плоскость перелёта считается заново, как было (красный)
        else if (!strcmp(sc.name, "cgpendold")) cgPendOld = sc.a != 0.f;   // №924: полный перебор клеток для сетки камеры каждый вызов, как было (красный)
        else if (!strcmp(sc.name, "cgdirtyold")) cgDirtyOld = sc.a != 0.f;   // №924: перебор клеток для сетки камеры раз в 8 кадров, как было (красный)
        else if (!strcmp(sc.name, "camwallupold")) camWallUpOld = sc.a != 0.f;   // №922: высокие стены без продолжения вверх для луча камеры, как было (красный)
        else if (!strcmp(sc.name, "camwinsplitold")) camWinSplitOld = sc.a != 0.f;   // аудит 04.11: flag camwinsplitold 1 — окно старта достраивает до шести клеток за вызов (как в TEST88)
        else if (!strcmp(sc.name, "camxferold")) camXferOld = sc.a != 0.f;   // №921: перенос на 100 м и дальше — прошлый глаз не забывался, перелёт мерил километровые лучи (красный)
        else if (!strcmp(sc.name, "camsoftpplold")) camSoftPplOld = sc.a != 0.f;   // №916в: прежний перелёт (человек на луче срывает его рывком)
        else if (!strcmp(sc.name, "camlog")) camLogOn = sc.d != 0.f;   // правка 340: «flag camlog 0» выключает (без числа — включает, как раньше)
        else if (!strcmp(sc.name, "carcamlog")) carCamLog = 1;
        else if (!strcmp(sc.name, "nearlog")) nearLog = sc.a != 0.f;
        else if (!strcmp(sc.name, "camnearspold")) camNearSpOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camneardenseold")) camNearDenseOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "nearpassclrold")) nearPassClrOld = sc.a != 0.f;   // 1 — второй проход своей машины чистит глубину в 0 (как было: салон сквозь кузов)   // №727: 1 — 3 × 3 луча, как было   // №727: 1 — стейтпропы не в ближней плоскости (как было)
        else if (!strcmp(sc.name, "camneardataold")) camNearDataOld = sc.a != 0.f;   // №721: 1 — у камер из данных ближняя 1,5 м всегда (как было)   // №727: строка CAMNEAR (ближняя плоскость кадра и её причина) каждые 0,1 с   // гейт правки 198+: строка CARCAM (кадр и кузов) раз в секунду
        else if (!strcmp(sc.name, "camnearold")) { chaseOld = 1; chase.init = 0; }
        else if (!strcmp(sc.name, "farplain")) { farPlainD = sc.a <= 0.f ? 0.f : sc.a < 40.f ? 40.f : sc.a > 240.f ? 240.f : sc.a; char l_[64]; snprintf(l_, sizeof l_, "FARPLAIN set d=%.0f t=%.1f\n", farPlainD, smkT); smkWrite(l_); }   // №863 служебно: порог дальней версии кусков (A/B в туре)
        else if (!strcmp(sc.name, "nearforce")) nearForceV = sc.a;                  // №863 служебно: ближняя плоскость числом (0 — выкл)
        else if (!strcmp(sc.name, "clearchk")) clearChk = sc.a != 0.f;              // замер-тур: пурпурный фон — непокрытые пиксели (щели) в SHOTFILE
        else if (!strcmp(sc.name, "seamchk")) seamChk = clearChk && sc.a != 0.f;   // с clearchk: город без альфа-теста и полупрозрачного
        else if (!strcmp(sc.name, "pano917cap")) { pano917Cap = sc.a > 0.f; pano917Near = sc.a; pano917Far = isnan(sc.b) ? 250.f : sc.b; clearChk = noSkyChk = pano917Cap; }   // №917б: кадр бота дальнего кольца
        else if (!strcmp(sc.name, "seamsold")) seamsOld = sc.a != 0.f;            // замер-тур: швы как до правок 224/237/251 (сравнение на железе)
        else if (!strcmp(sc.name, "shotdump")) smkShotDumpForce = sc.a != 0.f;
        else if (!strcmp(sc.name, "nosky")) noSkyChk = sc.a != 0.f;              // замер-тур: без неба (щели видны пурпуром при clearchk)    // снимок пишет игра и в эмуляторе (проверка тура)   // красный контроль правки 198+: прежняя камера за машиной (ближний вариант)
        else if (!strcmp(sc.name, "roomcamswitchold")) roomCamSwitchOld = 1;   // №229: откат правила смены камеры комнаты (задержка 1 с и при входе в триггер, пока Гомер в текущей)
        else if (!strcmp(sc.name, "roomcamold")) roomCamStickyOld = 1;   // красный контроль правки 186: камера комнаты держится, пока Гомер в её триггере
        else if (!strcmp(sc.name, "footcamold")) { homerCamDist = 4.2f; homerCamEyeH = 1.9f; homerCamLookH = 1.2f; footFovOld = 1; }   // красный контроль правки 167: прежняя камера пешком (4,2 м, 62°)   // гейт правки 167: камера каждый кадр (CAMF) и переходы (CAMBLEND) в smoke.txt   // отрицательный контроль правки 166: облёт будки в осях её матрицы, без поворота на 90°
        else if (!strcmp(sc.name, "msncamplant")) mcPlant = (int)sc.a;    // отрицательный контроль гейта камер старта: 1 задом наперёд, 2 сдвиг
        else if (!strcmp(sc.name, "coinadd") && haveCoins) { coins.collected += sc.a > 0.f ? (int)sc.a : 1;   // служебное: +N монет (гейт правки 51)
          char s[64]; snprintf(s, sizeof s, "COINADD n=%d t=%.1f\n", coins.collected, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "coinstay")) coinNoHide = 1;
        else if (!strcmp(sc.name, "coindrop") && haveCoins) {   // служебное (гейт счётчика монет): N монет на землю в 20 м впереди (отрицательное N: позади) — coin_drawn обязан различать
          float dir_ = sc.d < 0.f ? -1.f : 1.f, fx_ = car.x + sinf(car.h) * 20.f * dir_, fz_ = car.z + cosf(car.h) * 20.f * dir_;
          coinsDrop(&coins, fabsf(sc.d) >= 1.f ? (int)fabsf(sc.d) : 5, fx_, car.y + .6f, fz_, car.y);
          char s[96]; snprintf(s, sizeof s, "COINDROP n=%d dir=%d t=%.1f\n", fabsf(sc.d) >= 1.f ? (int)fabsf(sc.d) : 5, (int)dir_, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "coincam") && haveCoins) {   // №965 служебное: камера в D м от ближайшей монеты мира (со стороны героя), чуть выше, смотрит в её центр («flag coincam D»; D по умолчанию 2,5)
          float hx_ = onFoot() ? homer.x : car.x, hz_ = onFoot() ? homer.z : car.z, bd_ = 1e9f; int bi_ = -1;
          for (unsigned i = 0; i < coins.nworld; i++) { Coin *c = &coins.c[i]; if (!c->active) continue; float d_ = (c->x - hx_) * (c->x - hx_) + (c->z - hz_) * (c->z - hz_); if (d_ < bd_) { bd_ = d_; bi_ = (int)i; } }
          if (bi_ >= 0) { Coin *c = &coins.c[bi_]; float dd_ = sqrtf(bd_) + 1e-3f, ux_ = (hx_ - c->x) / dd_, uz_ = (hz_ - c->z) / dd_, D_ = sc.d > .1f ? sc.d : 2.5f;
            smkCamRel = 0; smkCam[0] = c->x + ux_ * D_; smkCam[1] = c->y + .62f; smkCam[2] = c->z + uz_ * D_; smkCam[3] = -ux_ * D_; smkCam[4] = -.3f; smkCam[5] = -uz_ * D_; smkCam[6] = 0.f; smkCamOn = 1;
            char s_[128]; snprintf(s_, sizeof s_, "COINCAM i=%d at=%.2f,%.2f,%.2f dist=%.1f t=%.1f\n", bi_, c->x, c->y, c->z, sqrtf(bd_), smkT); smkWrite(s_); } }
        else if (!strcmp(sc.name, "oldcarram") && oldCarOn) { oldCar.x = car.x + sinf(car.h) * 6.f; oldCar.z = car.z + cosf(car.h) * 6.f; oldCar.y = car.y; oldCarX0 = oldCar.x; oldCarZ0 = oldCar.z; car.vel[0] = sinf(car.h) * 12.f; car.vel[2] = cosf(car.h) * 12.f; char s_[96]; snprintf(s_, sizeof s_, "OLDCARRAM at=%.1f,%.1f t=%.1f\n", oldCar.x, oldCar.z, smkT); smkWrite(s_); }   // служебное (гейт 1241): оставленная своя встала в 6 м перед машиной игрока, машине игрока дана скорость 12 м/с вперёд
        else if (!strcmp(sc.name, "oldcarhitold")) oldCarHit1241Old = 1;   // отрицательный контроль правки 1241
        else if (!strcmp(sc.name, "jackoff")) jackOff = 1;   // отрицательный контроль правки 63
        else if (!strncmp(sc.name, "phonecar_", 9)) { int ok = mvPhoneCar(sc.name + 9); chase.init = 0;   // служебное (гейт правки 333): машина игрока — эта награда уровня на том же месте, как из будки
          char s[96]; snprintf(s, sizeof s, "PHONECAR %s ok=%d iris=%d t=%.1f\n", sc.name + 9, ok, carIrisFor(car.p.name), smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "jackplace") && haveTraffic && haveHomer) {   // служебное (гейт правки 63): машина трафика в 2.6 м справа от Гомера
          float a = homer.camYaw, x = homer.x - cosf(a) * 2.6f, z = homer.z + sinf(a) * 2.6f;
          int ok = trafficCarPlace(&traffic, &world, x, z, a) == 0; traffic.car[0].car.v = 0.f;
          char s[128]; snprintf(s, sizeof s, "JACKPLACE ok=%d model=%s at=%.1f,%.1f t=%.1f\n", ok, traffic.model[traffic.car[0].model].name, x, z, smkT); smkWrite(s); }
        else if (!strcmp(sc.name, "glowblendold")) { glowBlendOld = sc.a != 0.f; curFlags = -1; }
        else if (!strcmp(sc.name, "glowfog")) glowFogOld = 1;
        else if (!strcmp(sc.name, "spuvold")) spBillUvOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "grandpaold")) grandpaGlassesOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "fogground")) fogGroundOn = sc.a != 0.f && !FOG_GROUND_OLD;   // ДАЛЬ: подстилка в тумане по расстоянию (main_stream.h), 0 — прежний градиент у горизонта
        else if (!strcmp(sc.name, "fogsky")) { fogSkyOn = sc.a != 0.f && !FOG_SKY_OLD; if (sc.a > 1.f) fogSkyG = sc.a; else fogSkyG = 40.f; fogK = -1.f; }   // ДАЛЬ: туман города цвета горизонта неба, подстилка с градиентом (main_stream.h)
        else if (!strcmp(sc.name, "spaxesold")) { spBillAxesOld(sc.a != 0.f); lspAxesFlag = sc.a != 0.f; }
        else if (!strcmp(sc.name, "lspold")) lspOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "cellhyb")) cellHybOn = sc.a != 0.f;   // 17.10: гибрид «лист или куски» вкл/выкл на ходу (замер на PSP)
        else if (!strcmp(sc.name, "fxcut")) fxCutout = sc.a != 0.f;
        else if (!strcmp(sc.name, "smoke953")) fxSoftSmoke953 = sc.a != 0.f;   // №953: 1 — дым мягким смешиванием, 0 — вырезка (красный контроль)
        else if (!strcmp(sc.name, "swayfar")) spSwayFar = sc.a != 0.f;   // деревья и кусты дальше 25 м без качания (0 — как было)
        else if (!strcmp(sc.name, "shdiag973")) s973DiagOn = sc.a != 0.f;   // №973 служебно: замер SHADOW973
        else if (!strcmp(sc.name, "shadow973old")) sh973 = sc.a == 0.f;   // №973: 1 — тень машины плоскостью по центру (красный контроль)
        else if (!strcmp(sc.name, "icon993")) icon993Old = sc.a == 0.f;   // №993: flag icon993 0 — значок дома №623 вне задания, как было (красный контроль)
        else if (!strcmp(sc.name, "benchlog")) benchLog1034 = sc.a != 0.f;   // №1034: служебное — строки BENCH1034 (положение Гомера по кадрам)
        else if (!strcmp(sc.name, "spawnhold")) homerSceneGate431 = homerSpawnWait431 = sc.a != 0.f;   // правка 431: flag spawnhold 0 — герой не ждёт сцену (красный контроль гейтов, которые 431 маскирует: tests/loadorder r10)
        else if (!strcmp(sc.name, "runslide1230")) homerRunSlide1230 = sc.a != 0.f;   // №1230: flag runslide1230 0 — бег по косой вдоль стены гасится в ноль (красный контроль)
        else if (!strcmp(sc.name, "npcretry96")) npcRetry96 = sc.a != 0.f;   // Build 96: flag npcretry96 0 - красный контроль
        else if (!strcmp(sc.name, "npclad1235")) npcLad1235 = sc.a != 0.f;   // №1235: flag npclad1235 0 — модель персонажа как было (красный контроль)
        else if (!strcmp(sc.name, "mcrlad1238")) mcrLad1238 = sc.a != 0.f;   // №1238: flag mcrlad1238 0 — машина миссии мимо лестницы (красный контроль)
        else if (!strcmp(sc.name, "mcrfail")) mcrFailSim1238 = (int)sc.a;   // №1238 служебное: N отказов чтения блока машины миссии подряд
        else if (!strcmp(sc.name, "msnartfailmissgen")) msnartFailMissgen = (int)sc.a;   // Кольцо-96 служебно: N отказов памяти только у missgen.p3d
        else if (!strcmp(sc.name, "msnartpicwaitold")) msnartPicWaitOld = (int)sc.a;   // Кольцо-96: 1 — прежние мгновенный повтор и 8 с чёрного для картинки брифинга (красный контроль)
        else if (!strcmp(sc.name, "zone3danglefree")) zone3DangleFreeX();   // шаг 7 Fable, красный контроль: владелец освобождает блок канарейки
        else if (!strcmp(sc.name, "zone3dangle")) zone3DangleX((int)sc.a, "canary");   // шаг 7 Fable, красный контроль: канарейка в области N (0 миссия, 1 место) и оставленный указатель; после сброса/освобождения (при файле zone3scribble) WATCHDOG zone3 dangling
        else if (!strcmp(sc.name, "colwire")) colWire1223 = (int)sc.a;   // №1223 служебно: flag colwire N — зелёный каркас ящиков коллизии мира в радиусе N м (0 — выкл.)
        else if (!strcmp(sc.name, "ph6split95")) ph6Split95 = sc.a != 0.f;   // Build 95: flag ph6split95 0 — без строки PH6MEM
        else if (!strcmp(sc.name, "dlretry95")) dlRetry95 = sc.a != 0.f;   // Build 95: flag dlretry95 0 — без паузы повтора и строки (красный контроль)
        else if (!strcmp(sc.name, "dlfail")) dlFailSim95 = (int)sc.a;   // Build 95 служебно: N первых выделений листа клетки «нет памяти»
        else if (!strcmp(sc.name, "roomanimlog")) roomAnLog95 = sc.a != 0.f;   // служебно: ROOMANIM — смены ключа анимации текстур комнаты (экран автомата Kwik)
        else if (!strcmp(sc.name, "roomanimgt95")) gtOn95 = sc.a != 0.f;   // Build 95: flag roomanimgt95 0 — настенное время (красный контроль)
        else if (!strcmp(sc.name, "gagreset96")) collectGagReset96 = sc.a != 0.f;   // Build 96: flag gagreset96 0 — гэг остаётся разбитым между входами (красный контроль)
        else if (!strcmp(sc.name, "roomplace96")) homerRoomPlace96 = sc.a != 0.f;   // Build 96: flag roomplace96 0 — hold вместо roomplace (красный контроль)
        else if (!strcmp(sc.name, "dcache96")) dcache96 = sc.a != 0.f;   // Build 96: flag dcache96 0 — как было
        else if (!strcmp(sc.name, "dcacheall96")) dcAll96On = sc.a != 0.f;   // Build 96 диагностика
        else if (!strcmp(sc.name, "mcrwait94")) mcrWait94On = sc.a != 0.f;   // Build 94: flag mcrwait94 0 — повтор каждый кадр (красный контроль)
        else if (!strcmp(sc.name, "msnartretry94")) msnartRetry94 = sc.a != 0.f;   // Build 94: flag msnartretry94 0 — без повтора (красный контроль)
        else if (!strcmp(sc.name, "msnartfail")) msnartFailSim94 = (int)sc.a;   // Build 94 служебное: N первых чтений записи предмета «нет памяти»
        else if (!strcmp(sc.name, "npcfail")) npcFailSim1235 = (int)sc.a;   // №1235 служебное: N следующих чтений модели персонажа миссии «не вышли по памяти»
        else if (!strcmp(sc.name, "memeat")) memEat1235((int)sc.a);   // №1235 служебное: занять кучу так, чтобы наибольший цельный кусок стал меньше N КБ (0 — отдать)
        else if (!strcmp(sc.name, "camlr1231")) camLR1231 = sc.a != 0.f;   // №1231: flag camlr1231 0 — L+R пешком как было (красный контроль)
        else if (!strcmp(sc.name, "dropsim")) mvDropSim(0, (int)sc.a);   // №1229 служебное: предмет N миссии «выпал»
        else if (!strcmp(sc.name, "dropget")) mvDropSim(1, (int)sc.a);   // №1229 служебное: предмет N «подобран»
        else if (!strcmp(sc.name, "drop1229")) uiDrop1229 = sc.a != 0.f;   // №1229: flag drop1229 0 — «Item Dropped!» как было (красный контроль)
        else if (!strcmp(sc.name, "glass1224")) glassSolid1224 = sc.a != 0.f;   // №1224: flag glass1224 0 — целое стекло пустое для Гомера пешком (красный контроль)
        else if (!strcmp(sc.name, "bench1034")) bench1034 = sc.a != 0.f;   // №1034: flag bench1034 0 — скамейки пустые для Гомера пешком (красный контроль)
        else if (!strcmp(sc.name, "carcon1050")) carDbgOn1050 = sc.a != 0.f;   // №1050: строки CARCON — что держит машину (контакты кузова, колёс, откат подшага)
        else if (!strcmp(sc.name, "ramp1050")) colRamp1050 = sc.a != 0.f;   // №1050: flag ramp1050 0 — торец тонкой стенки гасит машину (красный контроль)
        else if (!strcmp(sc.name, "ramprail1050")) colRampRail1050 = sc.a != 0.f;   // №1050 (Б): flag ramprail1050 0 — торцы бортов держат (красный контроль)
        else if (!strcmp(sc.name, "camexit1170old")) camExit1170 = sc.a == 0.f;   // №1170: flag camexit1170old 1 — выход из наезда по прямой (красный контроль)
        else if (!strcmp(sc.name, "msncamfar")) camFar1041 = sc.a;   // Р3: flag msncamfar N — длина наезда, метры
        else if (!strcmp(sc.name, "msncamwall")) mcWall103 = sc.a != 0.f;   // Р3: flag msncamwall 0 — без сдвига от стены (красный контроль)
        else if (!strcmp(sc.name, "msncamshort96")) mcShort96 = sc.a != 0.f;   // 1246: flag msncamshort96 0 — наезды с диска (красный контроль)
        else if (!strcmp(sc.name, "bubskip")) bubShotSkip = (int)sc.a;
        else if (!strcmp(sc.name, "bubshots")) { bubShotN = (int)sc.a; uiBubRiseFr = 0; }   // №1247: снимок каждого кадра после подъёма пузыря цели (a — сколько, b — сколько кадров пропустить)
        else if (!strcmp(sc.name, "camin1041")) camIn1041 = sc.a != 0.f;   // №1041: flag camin1041 0 — наезд с первого кадра и склейка на выходе (красный контроль)
        else if (!strcmp(sc.name, "icon993old")) icon993Old = sc.a != 0.f;
        else if (!strcmp(sc.name, "trfcost1001b")) trfCost1001b = sc.a != 0.f;   // №1001б: flag trfcost1001b 0 — прежний обход препятствий (красный контроль)
        else if (!strcmp(sc.name, "shvec")) slShVec = sc.a != 0.f;   // векторная тень (0 — прежняя)
        else if (!strcmp(sc.name, "shherooff")) slHeroCenterOld = sc.a != 0.f;   // 1 — тень героя сдвинута от солнца (как было)
        else if (!strcmp(sc.name, "shpplold")) slPplOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "bbbatch")) bbBatchOn = sc.a != 0.f;   // №912: 1 — квады билбордов группами, 0 — по вызову на квад
        else if (!strcmp(sc.name, "lamppool935")) { lampPool935 = sc.a != 0.f; if (lspPoolY) for (unsigned q_ = 0; q_ <= lspArr; q_++) lspPoolY[q_] = 1e30f; lpSkipN[0] = lpSkipN[1] = lpDrawN = 0; slPoolSkipN = slPoolDrawN = 0; slResetPool935();   // №935: 1 — мост/эстакада и ломаный пол без эллипса света, 0 — прежнее
          if (smkOn) { char l_[96]; snprintf(l_, sizeof l_, "LAMPPOOL935 on=%d t=%.1f\n", lampPool935, smkT); smkWrite(l_); } }
        else if (!strcmp(sc.name, "pool990")) { pool990 = sc.a != 0.f; memset(p990, 0, sizeof p990); memset(p990us, 0, sizeof p990us); memset(p990c, 0, sizeof p990c); memset(p990tri, 0, sizeof p990tri); }   // №990: 1 — эллипсов под фонарями нет, 0 — как в TEST70
        else if (!strcmp(sc.name, "pool990stat")) { if (smkOn) { char l_[360]; snprintf(l_, sizeof l_, "POOL990 level=%d on=%d ell_tab=%u ell_cell=%u cone_tab=%u cone_cell=%u calls_tab=%u calls_cell=%u us_tab=%.1f us_cell=%.1f tri_ell_tab=%.1f tri_ell_cell=%.1f t=%.1f\n", gameLevel, pool990, p990[0], p990[1], p990[2], p990[3], p990c[0], p990c[1],
              p990c[0] ? (double)p990us[0] / p990c[0] : 0., p990c[1] ? (double)p990us[1] / p990c[1] : 0., p990c[0] ? (double)p990tri[0] / p990c[0] : 0., p990c[1] ? (double)p990tri[1] / p990c[1] : 0., smkT); smkWrite(l_); }
          memset(p990, 0, sizeof p990); memset(p990us, 0, sizeof p990us); memset(p990c, 0, sizeof p990c); memset(p990tri, 0, sizeof p990tri); }   // №990: счёт за время с прошлой строки (µs Mac на вызов, вызовы, эллипсов и конусов за всё окно)
        else if (!strcmp(sc.name, "beam995")) { beam995 = sc.a != 0.f; b995Draws = b995Tri = b995Calls = b995Us = b995UsOld = 0; mvBeamUs = mvBeamN = 0; }   // №995: 1 — конус света от днища тарелки, 0 — билборды луча как в TEST70
        else if (!strcmp(sc.name, "beam995stat")) { if (smkOn) { char l_[200]; snprintf(l_, sizeof l_, "BEAM995 on=%d draws=%u tri=%u top=%.1f gy=%.1f calls=%u us_new=%.1f us_old=%.1f us_all=%.1f frames=%u t=%.1f\n", beam995, b995Draws, b995Tri, b995Top, b995Gy, b995Calls, b995Calls ? (double)b995Us / b995Calls : 0., b995Calls ? (double)b995UsOld / b995Calls : 0., mvBeamN ? (double)mvBeamUs / mvBeamN : 0., mvBeamN, smkT); smkWrite(l_); } b995Draws = b995Tri = b995Calls = b995Us = b995UsOld = 0; }   // №995: служебно
        else if (!strcmp(sc.name, "lamppoolstat")) { if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "LAMPPOOL level=%d on=%d drawn=%u skip_bridge=%u skip_rough=%u sl_draw=%u sl_skip=%u t=%.1f\n", gameLevel, lampPool935, lpDrawN, lpSkipN[1], lpSkipN[0], slPoolDrawN, slPoolSkipN, smkT); smkWrite(l_); } }
        else if (!strcmp(sc.name, "lspnear")) { lspNearOn = (int)sc.a; lspNearOk = 0; }   // №913: 1 — свет обходит записи в радиусе, 0 — все, как было
        else if (!strcmp(sc.name, "ggpropbatch")) ggPropBatchOn = sc.a != 0.f;   // №913: 1 — срез кусков пропов по группам, 0 — полный обход
        else if (!strcmp(sc.name, "pbchk")) pbChkOn = sc.a != 0.f;   // №915: сверка списков пропов с полными обходами (смоук)
        else if (!strcmp(sc.name, "smksettle936")) smkSettle936 = (int)sc.a;   // №936 п.2: 1 — кадры устаивания считаются в PHSEC (верный делитель), 0 — прежнее (красный контроль гейта)
        else if (!strcmp(sc.name, "slprops933")) { slProps933 = (int)sc.a; slLightResetReq = 1; }   // №933: 1 — свет пропов в таблице, 0 — как было (красный контроль гейта)
        else if (!strcmp(sc.name, "piececull923")) pieceCull923 = (int)sc.a;   // №923: 1 — отсечение кусков крупных пропов по кадру, 0 — как было, 3 — сверка по вершинам, 4 — красный контроль (радиус ×0,3 со сверкой)
        else if (!strcmp(sc.name, "dropink922")) dropInk922 = (int)sc.a;   // №922: 1 — заплатка обводки машин, 0 — прежнее (красный контроль гейта)
        else if (!strcmp(sc.name, "cltfix940")) cltFix940 = (int)sc.a;   // №940: 1 — память длинных треугольников запоминает отказ и уплотняется, 0 — прежнее (красный контроль гейта)
        else if (!strcmp(sc.name, "cltpool940")) cltLim940 = sc.a > 0.f && sc.a < (float)CLT_POOL ? (unsigned)sc.a : (unsigned)CLT_POOL;   // №940 служебно: предел участка в байтах (имитация нехватки), 0 — полный
        else if (!strcmp(sc.name, "cltstress940")) cltStress940 = (int)sc.a;   // №940 служебно: ключ кэша меняется каждый кадр
        else if (!strcmp(sc.name, "dropfix921")) dropFix921 = (int)sc.a;   // №921: 1 — заплатка выброса GE у пропов/света, 0 — прежнее (красный контроль гейта)
        else if (!strcmp(sc.name, "crmove")) { crMoveOn = (int)sc.a; crv.valid = 0; }   // №919: 1 — широкие допуски повтора отсечения, 0 — B8, 2 — широкие без запаса (отрицательный контроль)
        else if (!strcmp(sc.name, "crchk")) crChkOn = (int)sc.a;   // №919: сверка повторов со свежим отсечением (строка CRCHK)
        else if (!strcmp(sc.name, "pbfast")) { pbFastOn = (int)sc.a; pbP.nearOk = pbS.nearOk = 0; pbP.dynBaseDirty = pbS.dynBaseDirty = 1; }   // №915: 1 — скан/клетки/динамические по спискам, 0 — полные обходы
        else if (!strcmp(sc.name, "blendopq")) { blendOpqOn = sc.a != 0.f; blendOpqApply(); pbReset(); }   // №907: 1 — пропы с пустым смешиванием непрозрачные, 0 — как было
        else if (!strcmp(sc.name, "lattice")) { latticeOn = sc.a != 0.f; latticeApply(); bound = -2; curLvlMode = -1; }   // №904: 1 — решётки до LATTICE_LEVELS мипов, 0 — как было (после — dloff 1)
        else if (!strcmp(sc.name, "treesh")) treeShOn = sc.a != 0.f;   // №753: 1 — тени деревьев рисовать (как было), 0 — нет
        else if (!strcmp(sc.name, "doorout")) homerSetDoorOut(sc.a);
        else if (!strcmp(sc.name, "g944doors") && smkOn) { for (int k_ = 0; k_ < rooms.ndoor; k_++) { const RoomDoor *d_ = &rooms.door[k_]; char l_[200]; snprintf(l_, sizeof l_, "DOOR944 idx=%d name=%s p=%.2f,%.2f,%.2f h=%.2f room=%d\n", k_, d_->name, d_->p[0], d_->p[1], d_->p[2], d_->h, d_->room); smkWrite(l_); } }   // №944 служебно: двери комнат
        else if (!strcmp(sc.name, "g944door") && haveHomer && (int)sc.a >= 0 && (int)sc.a < rooms.ndoor) {   // №944 служебно: Гомер в 5 м от двери N, лицом к ней (sc.b — расстояние)
          const RoomDoor *d_ = &rooms.door[(int)sc.a]; float h_ = isnan(d_->h) ? 0.f : d_->h, dist_ = isnan(sc.b) || sc.b <= 0.f ? 5.f : sc.b;
          if (rs.room >= 0 || rg.mem) roomLeave();
          homerPlace(&homer, &world, d_->p[0] + sinf(h_) * dist_, d_->p[1] + 1.f, d_->p[2] + cosf(h_) * dist_, h_ + 3.14159265f); smkSettle(); }
        else if (!strcmp(sc.name, "g944art")) { static const char *const f1[] = {"art/missions/level01/l1_doors.p3d", "art/missions/generic/sim_door.p3d", "art/missions/generic/kwk_door.p3d", "art/missions/generic/sch_door.p3d", "art/l1m7door.p3d", "art/missions/generic/missgen.p3d", "art/cards.p3d"},
            *const f2[] = {"art/missions/level02/l2_doors.p3d", "art/missions/generic/moe_door.p3d", "art/missions/generic/missgen.p3d", "art/cards.p3d"},
            *const f3[] = {"art/missions/level03/l3_doors.p3d", "art/missions/generic/ad_door.p3d", "art/missions/generic/obs_door.p3d", "art/missions/generic/missgen.p3d", "art/cards.p3d"};
          const char *const *f = sc.a == 2.f ? f2 : sc.a == 3.f ? f3 : f1; unsigned nf = sc.a == 2.f ? sizeof f2 / sizeof *f2 : sc.a == 3.f ? sizeof f3 / sizeof *f3 : sizeof f1 / sizeof *f1;
          for (unsigned i = 0; i < nf; i++) msnartLoad(f[i], 1); }   // №944 служебно: пакеты входов L1 (как LoadP3DFile миссий)
        else if (!strcmp(sc.name, "g944list") && smkOn) msnartList944(g944ListOut);
        else if (!strcmp(sc.name, "g944draw")) { g944DrawIdx = (int)sc.a; if (smkOn && g944DrawIdx >= 0 && g944DrawIdx < (int)(sizeof g944DrawNames / sizeof g944DrawNames[0])) { char l_[96]; snprintf(l_, sizeof l_, "G944DRAW idx=%d name=%s has=%d\n", g944DrawIdx, g944DrawNames[g944DrawIdx], msnartHas(g944DrawNames[g944DrawIdx])); smkWrite(l_); } }   // №944 служебно: рисовать рисунок входа из таблицы (msnart.shr: *_door, *_doors, *_doorstar) без миссии; -1 — нет
        else if (!strcmp(sc.name, "footbakeold")) skinSetFootBakeOld(sc.a != 0.f);   // Модели-96: 1 — поправку стоп не читать из данных, считать на месте (как в 100)
        else if (!strcmp(sc.name, "footvtxold")) skinSetFootVtxOld(sc.a != 0.f);   // Модели-96: 1 — стопы по суставам, как было
        else if (!strcmp(sc.name, "soleold")) skinSetSoleOld(sc.a != 0.f);   // №762: 1 — персонажи с подошвой не на нуле висят/тонут, как было   // Роб 17.10: Гомер у двери дальше от борта на N м (0 — как было), рука не уходит в дверь   // №748/749: 1 — тени как в TEST31 (без людей миссий и предметов, Гомер 20 %, без прыжка и края)
        else if (!strcmp(sc.name, "coinspr")) coinSprite = sc.a != 0.f;
        else if (!strcmp(sc.name, "coinmirror")) csRSign = sc.a != 0.f ? -1.f : 1.f;   // служебное: кадр монеты зеркально (сверка знака)   // 17.10: монеты спрайтами одной пачкой (0 — 3D-модель каждой)
        else if (!strcmp(sc.name, "spminpx")) spMinPx = sc.a;   // 17.10: стейтпропы меньше N пикселей на экране не рисуются (0 — как было)
        else if (!strcmp(sc.name, "veclight")) vecLight = sc.a != 0.f;   // №715/736: чёткий конус ровного цвета (0 — картинка с диска)   // №731: полупрозрачные частицы вырезкой (0 — смешиванием)
        else if (!strcmp(sc.name, "lspnew")) lspOld = sc.a == 0.f;   // магистраль 16.10: свет таблицей спрайтов по умолчанию выкл (LIGHT_SPRITE_OLD=1), «flag lspnew 1» включает в туре   // №599: 1 — постоянный свет прежним путём (сравнение «было/стало» одним EBOOT)
        else if (!strcmp(sc.name, "lightspr")) { slLightOn = sc.a != 0.f; lspOld = slLightOn ? 0 : LIGHT_SPRITE_OLD; }   // самостоятельный: сам включает таблицу №599 и оставляет в ней только конусы; 0 — как по умолчанию сборки   // №658: свет спрайтами из таблицы (viewer/spritelight.h)
        else if (!strcmp(sc.name, "homerspot")) { slHomerSpot = sc.a != 0.f; }   // 16.10: пятно Гомера как у PS2 (viewer/spritelight.h); машины и прохожие остаются на прежних пятнах, если shadowspr выкл
        else if (!strcmp(sc.name, "conewide")) { if (sc.a != 0.f) { lspConeW = LSP_CONE_W; lspConeL = LSP_CONE_L; lspConeA = LSP_CONE_A; } else { lspConeW = lspConeL = lspConeA = 1.f; } }   // 16.10: конусы гаража как у PS2 (lightsprite_core.h)
        else if (!strcmp(sc.name, "conew")) lspConeW = sc.a;
        else if (!strcmp(sc.name, "conel")) lspConeL = sc.a;
        else if (!strcmp(sc.name, "conea")) lspConeA = sc.a;
        else if (!strcmp(sc.name, "homerlit")) { skinSunPct = sc.a != 0.f ? SKIN_LIT_SUN_PCT : 100; skinAmbPct = sc.a != 0.f ? SKIN_LIT_AMB_PCT : 100; }   // №78: контраст света персонажей (skin.c)
        else if (!strcmp(sc.name, "skinsun")) skinSunPct = (int)sc.a;
        else if (!strcmp(sc.name, "skinamb")) skinAmbPct = (int)sc.a;
        else if (!strcmp(sc.name, "skindata")) { skinDataLit = sc.a != 0.f; if (!skinDataLit) skinLights(0, 0, 0, 0); }   // 16.10: свет персонажей из данных уровня (LGT1 целиком)
        else if (!strcmp(sc.name, "glint1022log")) gvLog1022 = sc.a != 0.f;
        else if (!strcmp(sc.name, "glint1031")) glint1031 = sc.a != 0.f;   // №1031: 0 — звезда ключа с проверкой глубины мира
        else if (!strcmp(sc.name, "streamahead1042")) streamAhead1042 = sc.a != 0.f;   // №1042: 0 — очередь загрузки как было
        else if (!strcmp(sc.name, "glintfix1022")) glintFix1022 = sc.a != 0.f;   // №1022: 0 — звёзды-свечения ключа и значков как в TEST71   // №1022 служебно: журнал каждого блика у глаза
        else if (!strcmp(sc.name, "glintlab")) glintLab941 = (int)sc.a;   // №941 служебно: лист форм блика на чёрном (tests/glint941)
        else if (!strcmp(sc.name, "glowspr")) wrGlowOn = sc.a != 0.f;   // №690: блеск под ключом спрайтом (flag glowspr 0 — как в TEST30, без блеска)
        else if (!strcmp(sc.name, "lightconeold")) lspConeOld = sc.a != 0.f;   // №703: красный контроль — конусы как до правки (100 м, без предела)
        else if (!strcmp(sc.name, "conedist")) lspConeDist = sc.a; else if (!strcmp(sc.name, "conefade")) lspConeFade = sc.a; else if (!strcmp(sc.name, "conemax")) lspConeMax = (int)sc.a;
        else if (!strcmp(sc.name, "addstat")) { char l_[128]; snprintf(l_, sizeof l_, "ADDSTAT t=%.1f pieces=%u tris=%u big=%u\n", smkT, dbgAddN, dbgAddTri, dbgAddBig); smkWrite(l_); dbgAddN = dbgAddTri = dbgAddBig = 0; }
        else if (!strcmp(sc.name, "fxtex")) { if (sc.a == 2.f) { static char b_[900]; fxTexStat(b_, sizeof b_); char l_[960]; snprintf(l_, sizeof l_, "FXTEX t=%.1f sc=%s%s\n", smkT, smkScene, b_); smkWrite(l_); } else fxAddStatOn = sc.a != 0.f; }   // №899: 1 вкл, 2 — строка и сброс
        else if (!strcmp(sc.name, "wrencheye")) weR_ = sc.a;
        else if (!strcmp(sc.name, "wrenchgo") && haveCol && world.nwrench) {   // служебно (№599/№690, ключ): машина в 18 м от ключа, глаз рядом, взгляд на ключ; a = номер + 100 × сторона (0..3); flag eyeon 0 снимает
          unsigned wi_ = (unsigned)sc.a % 100u, wd_ = (unsigned)sc.a / 100u; float sx_ = (wd_ & 1) ? -1.f : 1.f, sz_ = (wd_ & 2) ? -1.f : 1.f; const ColWrench *W_ = &world.wrench[wi_ % world.nwrench]; char l_[96]; snprintf(l_, sizeof l_, "WRENCHGO n=%u of %u at=%.1f,%.1f,%.1f taken=%u inst=%d mapped=%d\n", wi_ % world.nwrench, world.nwrench, W_->p[0], W_->p[1], W_->p[2], (G.wrenchTaken >> (wi_ % world.nwrench)) & 1u, (int)G.wrenchInst[wi_ % world.nwrench], G.wrenchMapped); smkWrite(l_);
          x = W_->p[0] + 18.f; z = W_->p[2]; h = 0.f; y = 200; v = vy = 0; float g_; if (ground(x, z, y, 0.f, &g_)) y = g_;
          eyeOv[0] = W_->p[0] + weR_ * sx_; eyeOv[1] = W_->p[1] + weR_ * .7f; eyeOv[2] = W_->p[2] + weR_ * sz_;
          float dx_ = -weR_ * sx_, dy_ = -weR_ * .7f + .5f, dz_ = -weR_ * sz_, l2_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_); eyeOv[3] = dx_ / l2_; eyeOv[4] = dy_ / l2_; eyeOv[5] = dz_ / l2_; eyeOvOn = 1; }
        else if (!SHR_REL_SMK && !strcmp(sc.name, "xsub")) XS_FLAG(sc.a);   // служебно (xsub.h): строка XSUB — мкс, вызовы, слова GE по участкам x_car, x_homer, x_car3d
        else if (!strcmp(sc.name, "carshspr")) slCarShOn = sc.a != 0.f;   // №680: тень машин мягким овалом (spritelight.h)
        else if (!strcmp(sc.name, "carshkx")) slCarKX = sc.a; else if (!strcmp(sc.name, "carshkz")) slCarKZ = sc.a; else if (!strcmp(sc.name, "carsha")) slCarA = sc.a;
        else if (!strcmp(sc.name, "shfanold")) slShFanOld = sc.a != 0.f;   // №698: красный контроль — тень текстурой 64×64 с квадратными краями
        else if (!strcmp(sc.name, "shadowspr")) slShadowOn = sc.a != 0.f;   // №658: тени спрайтами (модуль viewer/spritelight.h)
        else if (!strcmp(sc.name, "fxboomold")) { carExplosionOld = sc.a != 0.f; fxExplosionOld(carExplosionOld); }
        else if (!strcmp(sc.name, "radarphonesold")) radarPhonesOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "markerlodold")) msnartMarkerLodOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "shadowold")) groundShadeOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "chaselevelold")) chaseLevelOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "farprobe")) { if (farProbe == 4 && (int)sc.a != 4) opt |= O_SKYCULL; farProbe = (int)sc.a; if (farProbe == 4) opt &= ~O_SKYCULL; if (sc.b == sc.b && sc.b > 0.f) farProbeD = sc.b; fogK = -1.f;
          char s[96]; snprintf(s, sizeof s, "FARPROBE mode=%d d=%.1f draw=%.1f fog=%06x\n", farProbe, farProbeD, drawDist, fogCol); smkWrite(s); }
        else if (!strcmp(sc.name, "shhalf")) slShHalf = sc.a != 0.f;   // №716/722: тени полутоном (1) или прежним веером (0) — A/B в одном туре
        else if (!strcmp(sc.name, "hzcacheold")) { skyHorizonCacheOld=sc.a!=0.f; }
        else if (!strcmp(sc.name, "hzcacherf")) skyHorizonRfPair=sc.a!=0.f;
        else if (!strcmp(sc.name, "hzcachetone")) { toneSetSat((int)sc.a); toneApplyAll(); }
        else if (!strcmp(sc.name, "tone1026")) { toneSet1026((int)sc.a); toneApplyAll(); }   // №1026: 1 — вывески как город, 0 — как в TEST71; палитры заново
        else if (!strcmp(sc.name, "tone1026log") && smkOn) { unsigned ns_ = 0, ss_[3] = {0, 0, 0}, cs_ = 0, ws_ = 0;   // служебно: сумма палитр вывесок (notoon, не небо), монеты и ключа после тона
          for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) if ((tex[q_].tstat[4] & 9) == 1 && tex[q_].pal) { ns_++; for (int j_ = 0; j_ < 1024; j_++) if ((j_ & 3) != 3) ss_[j_ & 3] += ((const unsigned char *)tex[q_].pal)[j_]; }
          if (csState == 1 && csPal) for (int j_ = 0; j_ < 1024; j_++) if ((j_ & 3) != 3) cs_ += csPal[j_];
          for (int a_ = 0; a_ < WS_N; a_++) if (wsA[a_].state == 1 && wsA[a_].pal) for (int j_ = 0; j_ < 1024; j_++) if ((j_ & 3) != 3) ws_ += ((const unsigned char *)wsA[a_].pal)[j_];
          char l_[200]; snprintf(l_, sizeof l_, "TONE1026 t=%.1f on=%d level=%d signs=%u signsum=%u,%u,%u coinsum=%u wssum=%u sat=%d ct=%d\n", smkT, tone1026Get(), gameLevel, ns_, ss_[0], ss_[1], ss_[2], cs_, ws_, toneSatPct(), toneContrastPct()); smkWrite(l_); }
        else if (!strcmp(sc.name, "tonecontrast")) { toneSetContrast((int)sc.a); toneApplyAll(); }   // служебно (№1026): контраст как в меню (50..200), палитры заново
        else if (!strcmp(sc.name, "tonejobold")) { toneJobOld = sc.a != 0.f; }   // 16.10: 1 — смена тона прежним синхронным путём
        else if (!strcmp(sc.name, "tonebud")) { toneJobBudget = (int)sc.a; }   // бюджет фоновой смены тона, мкс на кадр
        else if (!strcmp(sc.name, "gamma")) { toneSetGamma((int)(sc.a + .5f)); toneApplyAll(); char g_[48]; snprintf(g_, sizeof g_, "GAMMA pct=%d t=%.1f\n", toneGammaPct(), smkT); smkWrite(g_); }   // проба 16.10: гамма палитр под экран PSP-2000
        else if (!strcmp(sc.name, "hzcache")) { char s[128]; snprintf(s,sizeof s,"HZCACHE level=%d old=%d valid=%d calc=%u hit=%u us=%u\n",gameLevel,skyHorizonCacheOld,skyHorizonCacheValid,skyHorizonCalculations,skyHorizonCacheHits,skyHorizonUs);smkWrite(s); }
        else if (!strcmp(sc.name, "fogtoneold")) { fogToneOld=sc.a!=0.f; skyFogColor();
          char s[128];snprintf(s,sizeof s,"FOGTONE level%d old%d hzold%d rgb=%06x sky=%06x hz_ms=%.1f\n",gameLevel,fogToneOld,fogHorizonOld,fogCol,fogSkyCol,skyHorizonUs/1000.f);smkWrite(s); }
        else if (!strcmp(sc.name, "tooneyes")) { skinToonEyes = sc.a != 0.f; skinToonFlush(); }   // №679: контур глаз для показа (1 тонкий, 0 нет); кэш оболочек пересобирается
        else if (!strcmp(sc.name, "roadpts") && haveTraffic) {   // №912 служебное: середины длинных полос уровня (x z курс длина ширина) — точки для снимков улицы
          const Traffic *T_ = &traffic; unsigned nr_ = T_->hd->nRoad, shown_ = 0;
          for (unsigned r_ = 0; r_ < nr_ && shown_ < 60; r_++) { const TrRoad *rd_ = &T_->road[r_]; if (rd_->length < 70.f || rd_->nPt < 3) continue;
            const TrPt *a_ = &T_->pt[rd_->firstPt + rd_->nPt / 2 - 1], *b_ = &T_->pt[rd_->firstPt + rd_->nPt / 2 + 1]; float h_ = atan2f(b_->x - a_->x, b_->z - a_->z); char l_[160];
            snprintf(l_, sizeof l_, "ROADPT %u x=%.1f z=%.1f h=%.3f len=%.0f w=%.1f\n", r_, a_->x, a_->z, h_, rd_->length, rd_->width); smkWrite(l_); shown_++; } }
        else if (!strcmp(sc.name, "carfillplayer")) carFillPlayer = sc.a != 0.f;   // №930 служебно (гейт): как будто рисуется машина игрока — заливки быть не должно
        else if (!strcmp(sc.name, "carfill930")) carFillOn = sc.a != 0.f;   // №930: 0 — куски машин трафика с текстурой (красный контроль гейта)
        else if (!strcmp(sc.name, "flat912")) { cfg.texturesOn = sc.a == 0.f; cfgApply(); }   // №912: 0 — текстуры (как «Textures: On»), 1 — заливка
        else if (!strcmp(sc.name, "noisemix")) { static const float pr_[4][4] = {{.42f, .28f, .18f, .12f}, {.30f, .28f, .22f, .20f}, {.25f, .25f, .25f, .25f}, {.50f, .30f, .15f, .05f}}; int k_ = (int)sc.a & 3; for (int q_ = 0; q_ < 4; q_++) noiseW[q_] = pr_[k_][q_];
          noiseReady = 0; flatNoiseBuild(); flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }   // №912: набор весов октав шума
        else if (!strcmp(sc.name, "markrange")) { markRangeBig = sc.d > 0.f ? sc.d : markRangeBig; if (!isnan(sc.b)) markRangeSmall = sc.b; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); }   // №912: дальность разметки, м: крупная, мелочь
        else if (!strcmp(sc.name, "flatdump")) { flatDump = sc.a != 0.f; }
        else if (!strcmp(sc.name, "flatrebuildold")) { flatRebuildOld = sc.a != 0.f; }
        else if (!strcmp(sc.name, "flatrescanold")) { flatRescanOld = sc.a != 0.f; }
        else if (!strcmp(sc.name, "marks918old")) { marksLayerOld = sc.a != 0.f; marksLoad(); flatTintKey = ~0u; }   // №918: 1 — слой разметки как в TEST57 (обработчик пропал в 0f63456a №916 при слиянии, возвращён)
        else if (!strcmp(sc.name, "flatdbg")) { flatDbg = sc.a != 0.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState();
          if (flatDbg && smkOn) for (unsigned t_ = 0; t_ < ntex && t_ < 1024; t_++) if (flatQ[t_]) { char l_[100]; snprintf(l_, sizeof l_, "FLATID %u %u,%u,%u\n", t_, flatTint[t_] & 255, (flatTint[t_] >> 8) & 255, (flatTint[t_] >> 16) & 255); smkWrite(l_); } }
        else if (!strcmp(sc.name, "noiseconst")) { noiseConst = sc.a; if (sc.a >= 1.f) noiseConst = -1.f; noiseReady = 0; flatNoiseBuild(); flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }
        else if (!strcmp(sc.name, "noise949")) { nzOn = sc.a != 0.f; if (nzCur) nzReset(); nzCur = -1; flatTexGen = flatMdlGen - 1; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); resetState(); }   // №949: 0 — шум по родным UV (красный контроль), 1 — по метрам (масштабы из noise949.bin, прочитанного при загрузке уровня)
        else if (!strcmp(sc.name, "noisepitch")) { if (sc.a > .05f) { noisePitch = sc.a; flatTexGen = flatMdlGen - 1; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); resetState(); } }
        else if (!strcmp(sc.name, "noiseg")) { if (sc.a > 0.f) { noiseUvG = sc.a; flatTexGen = flatMdlGen - 1; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); resetState(); } }
        else if (!strcmp(sc.name, "noisechk")) { noiseChk = sc.a != 0.f; noiseReady = 0; flatNoiseBuild(); flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); resetState(); }
        else if (!strcmp(sc.name, "noiselite")) { noiseNoLight = sc.a != 0.f; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); resetState(); }
        else if (!strcmp(sc.name, "ground969") || !strcmp(sc.name, "ground969all")) {   // №969: рисунок земли «как фон сериала»: 0 — прежний шум (красный контроль), 1/2/3 — вариант рисунка; ground969all 1 — на всех уровнях (по умолчанию только L1)
          if (!strcmp(sc.name, "ground969")) { int n_ = (int)sc.a; gdStyle = n_ < 0 || n_ > 4 ? 0 : n_; } else gdAll = sc.a != 0.f;
          if (gdStyle) gdBuild(); flatTintKey = ~0u; flatTintBuild(); if (nzCur) nzReset(); nzCur = -1; for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState();
          if (smkOn) { unsigned n3_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) n3_ += flatGd[q_] != 0; char l_[160]; snprintf(l_, sizeof l_, "GROUND969SET t=%.1f style=%d all=%d level=%d tex_drawn=%u built=%d\n", smkT, gdStyle, gdAll, gameLevel, n3_, gdBuiltStyle); smkWrite(l_); } }
        else if (!strcmp(sc.name, "asphlight")) { asphOn = sc.a != 0.f; toneApplyAll(); if (smkOn) { unsigned n_ = 0; for (unsigned q_ = 0; q_ < ntex; q_++) n_ += tex[q_].asph != 0; char l_[100]; snprintf(l_, sizeof l_, "ASPHLIGHT t=%.1f on=%d level=%d tex=%u\n", smkT, asphOn, gameLevel, n_); smkWrite(l_); } }   // №979: асфальт светлее на 8 % (палитра), задание тона пересчитает
        else if (!strcmp(sc.name, "inksep1009")) { inkSep1009 = sc.a != 0.f; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "INKSEP1009SET t=%.1f on=%d\n", smkT, inkSep1009); smkWrite(l_); } }   // №1009: машины трафика до своей машины и героя
        else if (!strcmp(sc.name, "walk1005")) { walk1005On = sc.a != 0.f; unsigned n_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) if (tex[q_].flatR == WALK1005_REASON && flatFromFile) { tex[q_].flatK = walk1005On ? 0 : 1; n_++; }
          flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState();   // №1005: плитка тротуара родной текстурой или заливкой
          if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "WALK1005SET t=%.1f on=%d level=%d tex=%u\n", smkT, walk1005On, gameLevel, n_); smkWrite(l_); } }
        else if (!strcmp(sc.name, "xwalk999")) { xwalk999On = sc.a != 0.f; toneApplyAll(); if (smkOn) { unsigned n_ = 0; for (unsigned q_ = 0; q_ < ntex; q_++) n_ += tex[q_].xdon != 0; char l_[100]; snprintf(l_, sizeof l_, "XWALK999SET t=%.1f on=%d level=%d tex=%u\n", smkT, xwalk999On, gameLevel, n_); smkWrite(l_); } }   // №999: переход на брусчатке в тон дороги (статистика донора), задание тона пересчитает
        else if (!strcmp(sc.name, "trfgeo984")) { trfGeoOld = sc.a == 0.f; }   // №984: 0 — прежний расчёт проверок риска выброса GE
        else if (!strcmp(sc.name, "trfgeocheck")) { trgCheck = sc.a != 0.f; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "TRFGEOCHECK t=%.1f on=%d checks=%u mismatches=%u\n", smkT, trgCheck, trgChk, trgMis); smkWrite(l_); } }   // №984 служебно (гейт)
        else if (!strcmp(sc.name, "trgdump")) { char l_[200]; snprintf(l_, sizeof l_, "TRFGEO984 t=%.1f pieces=%u draw_us=%u bignear_calls=%u fast_calls=%u check_us=%u carnear_us=%u old=%d\n", smkT, trgPieces, trgDrawUs, trgN, trgFastN, trgBnUs, trgCnUs, trfGeoOld); smkWrite(l_); trgPieces = trgDrawUs = trgN = trgFastN = trgBnUs = trgCnUs = 0; }   // №984 служебно
        else if (!strcmp(sc.name, "curb1064")) { curb1064Old = sc.a == 0.f; cfgApply(); }   // №1064: служебно — 0 бордюр как в TEST76 (заливка №987), 1 — по строке Curbs
        else if (!strcmp(sc.name, "curbtint")) { cfg.curbTint = sc.a != 0.f; cfgApply(); }   // №1064: служебно — строка Curbs (0 Original, 1 Tinted)
        else if (!strcmp(sc.name, "curb987") || !strcmp(sc.name, "curb987ys")) { if (sc.name[7] == 'y') curb987Ys = sc.a; else curb987On = sc.a != 0.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); toneApplyAll(); tlSelReset = 1;   // №987: родной бордюр, уголки под него, спокойная земля
          if (smkOn) { unsigned n_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) n_ += tex[q_].curb == 2; unsigned char t_[4] = {0, 0, 0, 0}; int ok_ = 0; for (unsigned q_ = 0; q_ < ntex && q_ < 1024 && !ok_; q_++) if (tex[q_].curb == 2) ok_ = curbTarget(&tex[q_], t_); char l_[200]; snprintf(l_, sizeof l_, "CURB987SET t=%.1f on=%d ys=%.2f level=%d corners=%u target=%d,%d,%d(%d) nat=%d,%d,%d(%d) pen=%08x\n", smkT, curb987On, curb987Ys, gameLevel, n_, t_[0], t_[1], t_[2], ok_, curbNat[0], curbNat[1], curbNat[2], curbNat[3], ink978Pen()); smkWrite(l_); } }
        else if (!strcmp(sc.name, "fehero1021")) { feHero1021 = sc.a != 0.f; }   // №1021-герои
        else if (!strcmp(sc.name, "zones1007")) { zn7On = sc.a != 0.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); toneApplyAll(); tlSelReset = 1;   // №1007
          if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "ZONES1007SET t=%.1f on=%d n=%u level=%d asph=%d,%d,%d(%d)\n", smkT, zn7On, zn7N, gameLevel, asphNat[0], asphNat[1], asphNat[2], asphNat[3]); smkWrite(l_); } }
        else if (!strcmp(sc.name, "floor1030")) { floor1030On = sc.a != 0.f; gdBuiltStyle = -1; gdBuild(); flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }   // №1030
        else if (!strcmp(sc.name, "beam1036")) beam1036 = sc.a != 0.f;   // №1036
        else if (!strcmp(sc.name, "walkcold985")) { curbAll = sc.a != 0.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "WALKCOLD985 t=%.1f on=%d\n", smkT, curbAll); smkWrite(l_); } }   // №985: тротуар холодным серым, бордюр темнее
        else if (!strcmp(sc.name, "edge983")) { edgeOn = sc.a != 0.f; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "EDGE983SET t=%.1f on=%d cells=%u verts=%u drawn=%u\n", smkT, edgeOn, egNC, egN, egDrawn); smkWrite(l_); } }   // №983: кромка тротуара слоем
        else if (!strcmp(sc.name, "path983")) { pathOn = sc.a != 0.f; tlSelReset = 1; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "PATH983SET t=%.1f on=%d lines=%u\n", smkT, pathOn, plN); smkWrite(l_); } }   // №983: контур тропинок
        else if (!strcmp(sc.name, "ink978")) { ink978On = sc.a != 0.f; tlSelReset = 1; if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "INK978SET t=%.1f on=%d pen=%08x\n", smkT, ink978On, ink978Pen()); smkWrite(l_); } }   // №978: контур бордюра — тёмный насыщенный тон бордюра, 1 px, ближе 28 м
        else if (!strcmp(sc.name, "curbrgb")) { unsigned v_ = (unsigned)sc.a; curbR = (int)((v_ >> 16) & 255); curbG = (int)((v_ >> 8) & 255); curbB = (int)(v_ & 255); flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); toneApplyAll(); }   // №974: подбор тона бордюра
        else if (!strcmp(sc.name, "curb974")) {   // №974: 1 — бордюр серо-фиолетовый, угловая деталь в тон; 0 — как было (красный контроль). Палитры угловой детали пересчитывает задание тона
          curbOn = sc.a != 0.f; curbAll = sc.a >= 2.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); toneApplyAll();
          if (smkOn) { unsigned n1_ = 0, n2_ = 0; for (unsigned q_ = 0; q_ < ntex; q_++) { n1_ += tex[q_].curb == 1; n2_ += tex[q_].curb == 2; } unsigned g_[4] = {0, 0, 0, 0}; for (unsigned q_ = 0; q_ < ntex && q_ < 1024; q_++) g_[flatGd[q_] & 3]++; char l_[160]; snprintf(l_, sizeof l_, "CURB974SET t=%.1f on=%d all=%d level=%d fill=%u corner=%u cls=%u/%u/%u\n", smkT, curbOn, curbAll, gameLevel, n1_, n2_, g_[1], g_[2], g_[3]); smkWrite(l_); { char l2_[200]; snprintf(l2_, sizeof l2_, "CURB974TEX t104 gd=%d kind=%d tint=%08x | t110 gd=%d kind=%d tint=%08x | t116 gd=%d kind=%d tint=%08x\n", ntex > 116 ? flatGd[104] : -1, ntex > 116 ? flatKind(104) : -1, ntex > 116 ? flatTint[104] : 0, ntex > 116 ? flatGd[110] : -1, ntex > 116 ? flatKind(110) : -1, ntex > 116 ? flatTint[110] : 0, ntex > 116 ? flatGd[116] : -1, ntex > 116 ? flatKind(116) : -1, ntex > 116 ? flatTint[116] : 0); smkWrite(l2_); } } }
        else if (!strcmp(sc.name, "noiseamp")) { if (sc.a >= 0.f && sc.a <= .9f) { noiseAmp = sc.a; noiseReady = 0; flatNoiseBuild(); flatTintKey = ~0u; flatTintBuild(); if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "NOISEAMP amp=%.2f mean=%.3f tint107=%08x kind=%d key=%08x\n", noiseAmp, noiseMean, ntex > 107 ? flatTint[107] : 0, ntex > 107 ? flatKind(107) : -1, ntex > 107 ? flatExact[107] : 0); smkWrite(l_); } for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); } }   // №912: размах шума земли
        else if (!strcmp(sc.name, "flatnoise")) { flatVNoise = sc.a == 2.f; flatNoiseOn = sc.a != 0.f; flatTintKey = ~0u; flatTintBuild(); for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }   // №912: 0 — земля заливкой без шума
        else if (!strcmp(sc.name, "toon")) { cfg.toon = sc.a != 0.f; cfgApply(); }   // правка 428: мультяшный режим (расширение порта): 1 вкл, 0 выкл
        else if (!strcmp(sc.name, "ink951bold")) skinInk951bOld = sc.a != 0.f;   // №951б: 1 — Tone как №951 (умбра, деревья без контура)
        else if (!strcmp(sc.name, "ink")) { cfg.ink = (int)sc.a; cfgApply(); if (smkOn) { char l_[200]; snprintf(l_, sizeof l_, "INK951 style=%d char=%08x world=%08x road=%08x prop=%08x shadow=%08x t=%.1f\n", skinInkStyle, skinInkAbgr(INK_CHAR), skinInkAbgr(INK_WORLD), skinInkAbgr(INK_ROAD), skinInkAbgr(INK_PROP), skinInkAbgr(INK_SHADOW), smkT); smkWrite(l_); } }   // №951: перо контура из сценария + строка с цветами по категориям
        else if (!strcmp(sc.name, "linew")) { cfg.lineWidth = (int)sc.a; cfgApply(); }   // №901: толщина контура героев из сценария (0 — тело без контура: сверка «линия от оболочки или от текстуры»)
        else if (!strcmp(sc.name, "skydebug")) skyDebug_ = sc.a != 0.f;   // №701 диагностика пустоты глубины после неба
        else if (!strcmp(sc.name, "toonworld")) levelToonWanted = sc.a != 0.f;   // №679: уровень-мультик при следующей загрузке (до flag level)
        else if (!strcmp(sc.name, "toonlinethick")) tlThick = sc.a > 0.f;   // №679: вторая линия (толщина 2 px) вкл/выкл
        else if (!strcmp(sc.name, "toonlinedepth")) tlDepth = (int)sc.a;   // №679: сдвиг глубины линий контура (служебно)
        else if (!strcmp(sc.name, "roaddepth")) rlDepth = (int)sc.a;   // №824: сдвиг глубины линий дороги (служебно, подбор)
        else if (!strcmp(sc.name, "roadlift")) rlLift = sc.a * .01f;   // №824: добавочная высота линий дороги над данными, см (служебно, подбор)
        else if (!strcmp(sc.name, "card1063old")) card1063Old = sc.a != 0.f;   // №1063: 1 — свечение карточки блик-звездой №839, как в TEST74
        else if (!strcmp(sc.name, "ink1062old")) ink1062Old = sc.a != 0.f;
        else if (!strcmp(sc.name, "acid1131old")) acid1131Old = sc.a != 0.f;
        else if (!strcmp(sc.name, "propink1138old")) propInk1138Old = sc.a != 0.f;   // №1138: 1 — контур предметов как в TEST84   // №1131: 1 — капли кислоты как до правки   // №1062: 1 — контур окружения мультика как в TEST74 (ставить в начале сценария, до загрузки уровня)
        else if (!strcmp(sc.name, "toonlines")) tlOn = sc.a != 0.f;   // №679: контур окружения мультика вкл/выкл (служебно)
        else if (!strcmp(sc.name, "groundbias")) { if (sc.a >= .25f && sc.a <= 4.f) groundMul = sc.a;   // множитель наклона мипа земли без пересборки (листы клеток и запечённые куски строим заново)
          for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_);
          ggcClear(); pbReset(); resetState();
          char s_[96]; snprintf(s_, sizeof s_, "GROUNDBIAS set mul=%.2f ground=%u t=%.1f\n", groundMul, groundTexN, smkT); smkWrite(s_); }
        else if (!strcmp(sc.name, "bisect")) bisectSet((unsigned)sc.a);   // маска возвратов оптимизаций на ходу (bisect.h)
        else if (!strcmp(sc.name, "texreg")) { texRegOld = sc.a == 0.f; bound = -2; rbId = -9; curLvlMode = -1; }   // №657: 1 — пропуск неизменившихся регистров текстуры, 0 — полная запись
        else if (!strcmp(sc.name, "texvram")) { int k_ = (int)sc.a; if (k_ <= 0) { tvKB = 0; tvRestore(); } else { tvKB = k_; tvDone = 0; tvFrames = 0; } }   // №657: частые текстуры города в VRAM, N КБ (0 — выкл.)
        else if (!strcmp(sc.name, "zf932")) { zfUser = sc.a != 0.f; }   // №932б: 0 — один проход (как до правки), 1 — дальние клетки отдельным проходом
        else if (!strcmp(sc.name, "mipbias")) { mipBias = sc.d; curLvlMode = -1; }   // правка 416, служебное (листы клеток — с dloff 1)
        else if (!strcmp(sc.name, "fogcityold")) { fogCityOld = sc.a != 0.f; fogK = -1.f; }   // правка 411: прежняя дымка (красный контроль)
        else if (!strcmp(sc.name, "phold")) { phOld = sc.a != 0.f; bound = -2; }   // правка 411: заглушка по-старому
        else if (!strcmp(sc.name, "phplant")) { phPlant = sc.a != 0.f; bound = -2; dlOff = phPlant; }   // правка 411: служебно — у города «нет текстуры» (листы клеток собраны с текстурами — мимо них)
        else if (!strcmp(sc.name, "fogzold")) { fogHorizonOld = sc.a != 0.f; skyFogColor(); fogK = -1.f;   // правка 411: красный контроль — туман по-старому
          char s[128];snprintf(s,sizeof s,"FOGZ level%d old%d rgb=%06x hz_ms=%.1f\n",gameLevel,fogHorizonOld,fogCol,skyHorizonUs/1000.f);smkWrite(s); }
        else if (!strcmp(sc.name, "gebad") && nd > 0) {   // правка 103, отрицательный контроль: у текстуры куска на экране пустая палитра
          Tex *x = texAt(pc[sp[0]].r.tex); if (x) x->pal = 0; bound = -2;   // (как у выгруженной модели) — кусок обязан не рисоваться, smkGeBad растёт
          char s_[64]; snprintf(s_, sizeof s_, "GEBADPLANT tex=%d t=%.1f\n", pc[sp[0]].r.tex, smkT); smkWrite(s_); }
        else if (!strcmp(sc.name, "gelist")) {   // правка 103, гейт защиты: список GE как будто N КБ (массив прежний — памяти ничего не грозит)
          unsigned k = (unsigned)sc.a * 1024u; geCap = k >= 8192u && k < GE_BUDGET ? k : GE_BUDGET;
          char s_[64]; snprintf(s_, sizeof s_, "GELIST cap=%u t=%.1f\n", geCap, smkT); smkWrite(s_); }
        else if (!strcmp(sc.name, "freeze")) {   // правка 103, гейт сторожа: главный поток встаёт на N с там, где ждёт GE (как зависший список)
          smkFreezeMs = sc.a > 0 && sc.a < 60 ? (unsigned)(sc.a * 1000.f + .5f) : 7000u;   // доли секунды — тест повтора кнопки после долгого кадра
          char s_[64]; snprintf(s_, sizeof s_, "FREEZE ms=%u t=%.1f\n", smkFreezeMs, smkT); smkWrite(s_); }
        else if (!strcmp(sc.name, "wallfrontr")) wallFrontR = sc.a;   // №634 служебно
        else if (!strcmp(sc.name, "mipconst")) { mipConst = (int)sc.a; bound = -2; curLvlMode = -1; }   // служебно: уровень мипа N (-1 выкл.)
        else if (!strcmp(sc.name, "mipauto")) mipAuto = sc.a != 2.f;   // отрицательный контроль правки 77/88; «mipauto 2» — выкл.   // отрицательный контроль правки 76
        else if (!strcmp(sc.name, "tutnow")) tutCalmOff = 1;
        else if (!strcmp(sc.name, "npcwaitold")) npcWaitOld = sc.a != 0.f;   // №1076: красный контроль (экран загрузки не ждёт персонажей)
        else if (!strcmp(sc.name, "msflowold")) msFlowOld = sc.a != 0.f;   // №1070–1072: красный контроль (смена миссии как до правки)
        else if (!strcmp(sc.name, "carnearold")) gameMsnCarNearOld = sc.a != 0.f;   // №1073: красный контроль (машина миссии к герою с любого расстояния)
        else if (!strcmp(sc.name, "racejamold")) mcRaceJamOld = sc.a != 0.f;   // правка 1205: красный контроль — заклинивший соперник гонки не возвращается на путь
        else if (!strcmp(sc.name, "raceresetold")) gameMsnRaceResetOld = sc.a != 0.f;   // правка 1205: красный контроль — основная фаза уличной гонки без посадки игрока (L5 sr1–sr3)
        else if (!strcmp(sc.name, "msnresetold")) gameMsnResetAlways = 1;   // отрицательный контроль правки 61   // отрицательный контроль правки 60
        else if (!strcmp(sc.name, "shopvoff")) { shopViewPlant(1); smkWrite("SHOPV plant off\n"); }   // отрицательный контроль пункта 6: без экрана покупки
        else if (!strcmp(sc.name, "cardinfo96")) uiCardInfo96Set(sc.a != 0.f);   // flag cardinfo96 0 — две строки описания карточки, как было
        else if (!strcmp(sc.name, "briefold")) { mvBriefOld(1); uiBriefMixedCase(1); }   // отрицательный контроль правки 52: прежняя карточка
        else if (!strcmp(sc.name, "radarharassold")) uiRadarHarassOld(sc.a != 0.f);   // №241
        else if (!strcmp(sc.name, "burnrepold")) sndBurnRepeatOld(sc.a != 0.f);   // №180/№184
        else if (!strcmp(sc.name, "coneoff")) uiRadarConeOff(1);   // отрицательный контроль правки 50: клин вверх всегда, все значки у обода
        else if (!strcmp(sc.name, "radarlog")) radarLog = 1;        // строка RADAR раз в 0.5 с (гейт правки 50)   // отрицательный контроль правки 51: счёт висит всегда
        else if (!strcmp(sc.name, "lbnoduck")) lbNoDuck = 1;   // отрицательный контроль правки 55: разговор без приглушения
        else if (!strcmp(sc.name, "convplant")) mvTalkCamPlant((int)sc.a);  // отрицательный контроль камер разговора: 1 план на слушающем, 2 глаз на оси, 3 ближняя плоскость города
        else if (!strcmp(sc.name, "camstat")) { char s[96]; snprintf(s, sizeof s, "CAMW t=%.1f frames=%d blocked=%d onfoot=%d room=%d\n", smkT, camFrames, camBlocked, onFoot(), inRoom()); smkWrite(s);
          camFrames = camBlocked = 0; camStatOn = 1; camStatT0 = smkT + (sc.a > 0.f ? sc.a : 0.f); }   // гейт tests/camera: кадры с глазом за препятствием; N — пропустить N с (перелёт после переноса)
        else if (!strcmp(sc.name, "convvfov43")) convVFov43 = sc.a != 0.f;   // аудит 04.11: flag convvfov43 1 — вертикальный угол кадров разговора как у оригинала на 4:3
        else if (!strcmp(sc.name, "homercarpinold")) homerCarPinOld = sc.a != 0.f;   // аудит 04.11: flag homercarpinold 1 — упор луча камеры пешком в кузов подтягивает глаз к герою (как в TEST89)
        else if (!strcmp(sc.name, "nocamwall")) { homerCamWallOff = chaseWallOff = 1; smkWrite("CAMW nocamwall\n"); }   // красный контроль: без обхода стен
        else if (!strcmp(sc.name, "lumakey")) lumaKey = sc.a > 0.f;   // гейт tests/light: машина и Гомер сплошным ключом (маска)
        else if (!strcmp(sc.name, "lightplant")) {   // отрицательный контроль tests/light: свет уровня 1 (день) вместо своего
          lvlLightOk = 0; levelLightApply(0); }
        else if (!strcmp(sc.name, "carv")) { char s[64]; snprintf(s, sizeof s, "CARV t=%.1f v=%.2f onfoot=%d fly=%d\n", smkT, car.v, onFoot(), fly); smkWrite(s); }   // гейт tests/svc: скорость машины
        else if (!strcmp(sc.name, "skidhoverold")) skidHoverOld(sc.a != 0.f);   // правка 413: следы под парящими (красный контроль)
        else if (!strcmp(sc.name, "skidoff")) skidEnable(0);   // правка 249: красный контроль tests/skid (прежнее — следов нет)
        else if (!strcmp(sc.name, "skidwide")) skidWideOld(sc.a != 0.f);
        else if (!strcmp(sc.name, "msghardbr")) uiMsgHardBreaks(sc.a != 0.f);   // правка 267: прежние жёсткие переносы в нижнем пузыре
        else if (!strcmp(sc.name, "uitypo")) uiTypoShot((int)sc.a);   // правка 312: снимок фона твик-панели типографики (1 без текста, 2 без интерфейса)
        else if (!strcmp(sc.name, "padresyncold")) padResyncOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "trfnearold")) trfNearRadial = sc.a != 0.f;
        else if (!strcmp(sc.name, "cfcost")) cfCostFlag = sc.a != 0.f;   // №930в: замеры CFCOST (цена разбора куска машины NPC) — только по запросу
        else if (!strcmp(sc.name, "trfspawnold")) trfSpawnOld = sc.a != 0.f;   // правка 328: прежнее рождение трафика (красный контроль)
        else if (!strcmp(sc.name, "propsoftold")) propsSoftHomer = sc.a != 0.f;
        else if (!strcmp(sc.name, "huskold")) huskOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "burnold")) huskBurnOld = sc.a != 0.f;   // №697: остов горит постоянно (красный)
        else if (!strcmp(sc.name, "sellerlog")) mvServiceSellers();   // правка 288: продавцы
        else if (!strcmp(sc.name, "selleratloc")) gameSellerAtLoc = sc.a != 0.f;
        else if (!strcmp(sc.name, "boss967old")) gameBoss967Old = sc.a != 0.f;   // №967: 1 — тарелки и луча нет, событие не шлётся (как было)
        else if (!strcmp(sc.name, "dmgmercy96")) carDmgMercy96 = sc.a != 0.f;   // flag dmgmercy96 0 - как было (красный контроль)
        else if (!strcmp(sc.name, "seller96")) gameSeller96 = sc.a != 0.f;   // flag seller96 0 — как было (красный контроль)
        else if (!strcmp(sc.name, "shopfirstold")) gameShopFirstOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "talkcarold")) gameTalkCarOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "ldfinpri")) { ldFinPriOn = sc.a != 0.f; if (smkOn) { char l_[80]; snprintf(l_, sizeof l_, "LDFINPRI on=%d old=%d t=%.1f\n", ldFinPriOn, LDFINISHPRI1173_OLD, smkT); smkWrite(l_); } }   // №1173: переключатель разбора клеток ниже главного
        else if (!strcmp(sc.name, "susloopold")) sndSusLoopOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "wageracceptold")) { extern int svWagerAcceptOld; svWagerAcceptOld = sc.a != 0.f; }   // Данные-93: 1 — ✕ на ставочной карточке не платит взнос (прежнее)
        else if (!strcmp(sc.name, "talkroomfloorold")) msnTalkRoomOld = sc.a != 0.f;   // 1 — разговор в комнате без нижней границы радиуса 2,6 м
        else if (!strcmp(sc.name, "talkold")) msnTalkOld = sc.a != 0.f;   // правка 345: прежний радиус разговора 3 м + 1 м (красный)
        else if (!strcmp(sc.name, "pcsayold")) sndPcSayOld = sc.a != 0.f;   // правка 345: без реплик старта миссии и выхода из машины (красный)
        else if (!strcmp(sc.name, "roomambold")) sndRoomAmbOld = sc.a != 0.f;   // правка 345: в комнате атмосфера улицы (красный)
        else if (!strcmp(sc.name, "dollarold")) { extern int mvDollarOld; mvDollarOld = sc.a != 0.f; }
        else if (!strcmp(sc.name, "saveroamold")) { extern int mvSaveRoamOld; mvSaveRoamOld = sc.a != 0.f; }   // правка 330: сохранение только на этапе 0 прогулки (красный)   // правка 329: без значка доллара над продавцами (красный)   // правка 303: концовка выхода из машины петлёй (красный)   // правка 305: машина на место разговора (красный)   // правка 306: лавка перехватывает △ у собеседника этапа (красный)
        else if (!strcmp(sc.name, "policedmgold")) gamePoliceDmgOld = sc.a != 0.f;   // правка 280: полный урон полицейской (красный)   // правка 288: прежнее место разговора (красный)
        else if (!strcmp(sc.name, "camhighold")) camHighOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "cam554old")) cam554Old = sc.a != 0.f;
        else if (!strcmp(sc.name, "camcarsold")) camCarsOld = sc.a != 0.f;   // правка 284: камера сквозь машины (красный)
        else if (!strcmp(sc.name, "camgagold")) camGagOld = sc.a != 0.f;   // правка 338: сетки гэгов — не тела камеры (красный)
        else if (!strcmp(sc.name, "zonecutold")) zoneCutOld = sc.a != 0.f;   // правка 338: камеры зон срезом, как было (красный)
        else if (!strcmp(sc.name, "nearcullold")) camNearOld = sc.a != 0.f;   // №602: прежние пороги (красный контроль)
        else if (!strcmp(sc.name, "selectcamold")) selectCamOld = sc.a != 0.f;   // №617: SELECT = сброс машины, как было (красный)
        else if (!strcmp(sc.name, "camwallorgold")) camWallOrgOld = sc.a != 0.f;   // TEST19 J9-5: луч обхода стен от точки взгляда, как было (красный)
        else if (!strcmp(sc.name, "camvelpadold")) camVelPadOld = sc.a != 0.f;   // TEST19 L3: ящик чужого тела без запаса по скорости (красный)
        else if (!strcmp(sc.name, "camphoneguardold")) camPhoneGuardOld = sc.a != 0.f;   // TEST19 будка: облёт без правила камеры (красный)
        else if (!strcmp(sc.name, "phoneanim")) {   // служебное (TEST19 будка): запуск облёта ближней будки без меню
          int bk = -1; float bd = 1e30f; for (unsigned k = 0; k < world.nphonex; k++) { const float *L = world.phonex[k].loc; float d = (L[0] - homer.x) * (L[0] - homer.x) + (L[2] - homer.z) * (L[2] - homer.z); if (d < bd) { bd = d; bk = (int)k; } }
          phoneCamStart(bk, car.x, car.y, car.z); char s[64]; snprintf(s, sizeof s, "PHONEANIM k=%d t=%.1f loc=%.1f,%.1f,%.1f n=%u\n", bk, smkT, bk >= 0 ? world.phonex[bk].loc[0] : 0.f, bk >= 0 ? world.phonex[bk].loc[1] : 0.f, bk >= 0 ? world.phonex[bk].loc[2] : 0.f, world.nphonex); smkWrite(s); }
        else if (!strcmp(sc.name, "campropold")) camPropOld = sc.a != 0.f;   // TEST19 скамья L2: пропы не тела камеры (красный)
        else if (!strcmp(sc.name, "camneargeomold")) camNearGeomOld = sc.a != 0.f;   // ближняя плоскость без геометрии города, как было (красный)
        else if (!strcmp(sc.name, "camraiseold")) camRaiseOld = sc.a != 0.f;   // TEST19 скамья L2: глаз не поднимается над препятствием, только подтягивается (красный)
        else if (!strcmp(sc.name, "homerflipstandold")) homerFlipStandOld = sc.a != 0.f;   // №678: 1 — перескок камеры у стены и у стоящего Гомера (как было)
        else if (!strcmp(sc.name, "selectcycleold")) selectCycleOld = sc.a != 0.f;   // №617: 1 — SELECT только дальний/ближний
        else if (!strcmp(sc.name, "flipbusyold")) homerFlipBusyOld = sc.a != 0.f;   // №947б: 1 — перескок камеры у стены курс не замораживает (красный контроль ctrl920)
        else if (!strcmp(sc.name, "ctrl947old")) { homerCtrl916Old = sc.a == 0.f; homerFlipBusyOld = sc.a != 0.f; }   // №947б: 1 — прежнее №916/№920 (кадр держится, пока стик не отпущен); 0 — как сейчас (стик от текущего вида)
        else if (!strcmp(sc.name, "ctrl916old")) homerCtrl916Old = sc.a != 0.f;   // №916: 1 — кадр управления берётся заново при смене угла стика > 35° (прежнее правило №836)
        else if (!strcmp(sc.name, "walkcam1059old")) homerWalkCam1059Old = sc.a != 0.f;   // №1059: 1 — кадр управления держится до отпускания стика (как было)
        else if (!strcmp(sc.name, "walkcam1059tau")) homerWalkCamTau = sc.a;   // №1059: запаздывание кадра за видом, с
        else if (!strcmp(sc.name, "walkcam1059rate")) homerWalkCamRate = sc.a * (GU_PI / 180.f);   // №1059: скорость кадра за видом, °/с
        else if (!strcmp(sc.name, "ctrlfreezeold")) homerCtrlFreezeOld = sc.a != 0.f;   // №704: 1 — «вперёд» стика от текущего вида камеры (как было)
        else if (!strcmp(sc.name, "camuniformold")) { camUniformOld = sc.a != 0.f; chase.init = 0; }   // №554: 1 — камера за машиной по данным каждой машины (A/B «как PS2»), 0 — выравнено до famil_v (№494)
        else if (!strcmp(sc.name, "exityawold")) homerExitYawOld = sc.a != 0.f;   // №704: 1 — после выхода из машины «вперёд» от вида камеры пешком (как было)
        else if (!strcmp(sc.name, "garagelift")) garageLiftN = (int)sc.a;   // №660: прибавка света в зале с лампами, 0–255 белого (0 выкл)
        else if (!strcmp(sc.name, "camps2dist")) { camPs2Dist = sc.a != 0.f; chase.init = 0; }   // №554 вариант C: дальность/высота за машиной по числам PS2 Стенда3
        else if (!strcmp(sc.name, "camflip1old")) homerCamFlipOld = sc.a != 0.f;   // №916в: перескок у стены в два кадра, как было (красный)
        else if (!strcmp(sc.name, "camwallps2old")) homerCamPs2Old = sc.a != 0.f;   // стена за камерой пешком: подъезд и подъём, как было (красный)
        else if (!strcmp(sc.name, "camgeomold")) camGeomOld = sc.a != 0.f;   // №602: прежнее (красный контроль)
        else if (!strcmp(sc.name, "camgeomcacheold")) cgCacheOld = sc.a != 0.f;   // №620: проверка луча каждый кадр (красный/замер)
        else if (!strcmp(sc.name, "camavoidold")) camAvoidOld = sc.a != 0.f;   // №602: прежняя камера (красный контроль)
        else if (!strcmp(sc.name, "carbehind")) {   // служебное (№602): оставленная машина (твёрдое тело) в N м позади машины игрока, носом туда же
          float d = sc.a > 0.f ? sc.a : 4.f; Car k = car; k.x = car.x - sinf(car.h) * d; k.z = car.z - cosf(car.h) * d; carSetSpeed(&k, 0.f); k.destroyed = 0;
          parkOwnCar(&k, &cv); char s[128]; snprintf(s, sizeof s, "CARBEHIND t=%.1f at=%.2f,%.2f own=%.2f,%.2f\n", smkT, k.x, k.z, car.x, car.z); smkWrite(s); }
        else if (!strcmp(sc.name, "trfbehind") && haveTraffic) {   // служебное (№602): машина трафика на полосе в N м позади машины игрока, едет следом
          float d = sc.a > 0.f ? sc.a : 6.f; int ok = trafficCarPlace(&traffic, &world, car.x - sinf(car.h) * d, car.z - cosf(car.h) * d, car.h) == 0;
          char s[128]; snprintf(s, sizeof s, "TRFBEHIND t=%.1f ok=%d car0=%.2f,%.2f\n", smkT, ok, traffic.car[0].car.x, traffic.car[0].car.z); smkWrite(s); }
        else if (!strcmp(sc.name, "camsafeold")) camSafeOld = sc.a != 0.f;   // правка 331: прежняя камера (красный контроль)
        else if (!strcmp(sc.name, "caminlog")) camInLog = 1;   // правка 331: строка CAMINF на каждый кадр «камера внутри»
        else if (!strcmp(sc.name, "camnearsmoothold")) camNearSmoothOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camflooroverold")) camFloorOverOld = sc.a != 0.f;   // №898: прежнее (красный контроль)
        else if (!strcmp(sc.name, "camnfstillold")) camNfStillOld = sc.a != 0.f;   // №984б: 1 — лучи ближней плоскости каждый кадр, даже когда камера стоит
        else if (!strcmp(sc.name, "camrayboxold")) camRayBoxOld = sc.a != 0.f;   // №984: 1 — все тела на каждый луч ближней плоскости, как было
        else if (!strcmp(sc.name, "camnearcacheold")) camNearCacheOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camraysold")) camRaysOld = sc.a != 0.f;   // CAMPERF: 1 — лучи ближней плоскости по одному (как было)
        else if (!strcmp(sc.name, "camraysverify")) camRaysVerify = sc.a != 0.f;   // CAMPERF: считать лучи и пакетом, и по одному, расхождения — в CAMSUB mm=
        else if (!strcmp(sc.name, "cgcoarseold")) cgCoarseOld = sc.a != 0.f;   // CAMPERF: 1 — без пропуска пустых блоков сетки
        else if (!strcmp(sc.name, "cgtriold")) cgTriOld = sc.a != 0.f;   // CAMPERF: 1 — разметка треугольника по листьям (4 метки на лист)
        else if (!strcmp(sc.name, "camstatalwaysold")) camStatAlwaysOld = sc.a != 0.f;   // CAMPERF: 1 — проверка глаза за стеной каждый кадр, как было
        else if (!strcmp(sc.name, "camdet")) camDet = sc.a != 0.f;   // служебное: шаг кадра 1/30 с и бюджет стройки сетки в треугольниках (прогоны кадр в кадр)
        else if (!strcmp(sc.name, "camdatagroundold")) camDataGroundOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camsafeskipold")) camSafeSkipOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camingeom")) camInGeom = sc.a != 0.f;
        else if (!strcmp(sc.name, "gglow")) ggLowH = sc.a;   // №674: flag gglow 2.5 — срез у камеры при глазе ниже N м над землёй (0 — выкл)
        else if (!strcmp(sc.name, "diaglog")) { diagOn = smkOn && sc.a != 0.f; diagMok = 0; }   // №674: журнал треугольников без среза под риском защитной полосы GE (DIAGF/DIAGS)
        else if (!strcmp(sc.name, "cgbudgetold")) cgBudgetOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "roomfovold")) roomFovOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "camlensold")) camLensOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "cgaheadold")) cgAheadOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "carzonegroundold")) carZoneGroundOld = sc.a != 0.f;   // №694: 1 — камера зоны машины и на земле (как было)
        else if (!strcmp(sc.name, "camglidecapold")) camGlideCapOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "bumpgroundold")) bumpGroundOld = sc.a != 0.f;   // №791: 1 — глаз бампера не поднимается над асфальтом (как было)
        else if (!strcmp(sc.name, "camphonestepold")) camPhoneStepOld = sc.a > 0.f;
        else if (!strcmp(sc.name, "camin")) { { char v_[200]; snprintf(v_, sizeof v_, "CAMV t=%.1f frames=%d vfloor=%d vwall=%d near=%d ground=%d eye=%d gband=%d nearmin=%.2f onfoot=%d blift=%d bliftmax=%.2f bump_us=%u\n", smkT, camInFr, camInFl[7], camInFl[8], camInFl[1], camInFl[2], camInFl[0], camInGband, camInNearMin, onFoot(), camInBump, camInBumpMax, camBumpN ? camBumpUs / camBumpN : 0u); smkWrite(v_); camInGband = camInBump = 0; camInBumpMax = 0.f; camBumpUs = camBumpN = 0; }   // №791: ближняя плоскость против видимого пола и стен
          char s[900]; snprintf(s, sizeof s, "CAMIN t=%.1f frames=%d any=%d eye=%d near=%d ground=%d car=%d person=%d occ=%d data=%d nearmin=%.2f onfoot=%d us=%u usmax=%u jump=%d jmax=%.2f databad=%d geom=%d gtri=%d gover=%d gcalls=%u gusray=%u gusstep=%u gstepn=%u greuse=%u ghits=%u gdone=%u gvox=%u tri=%u skipny=%u s=%.1f,%.1f,%.1f bb=%.1f,%.1f,%.1f,%.1f,%.1f,%.1f ox=%.1f sc=%.5f n=%.0f pjump=%d pjmax=%.2f pdmax=%.2f pclamp=%d jsec=%s dgfix=%d cskip=%d cparts=%u/%u/%u/%u/%u/%u/%u/%u gbuild=%u/%u gidle=%u/%u pf=%u/%u pend=%u/%u/%u/%u/%u/%u\n",
            smkT, camInFr, camInAny, camInFl[0], camInFl[1], camInFl[2], camInFl[3], camInFl[4], camInFl[5], camInData, camInNearMin, onFoot(),
            camSafeUsN ? camSafeUs / camSafeUsN : 0u, camSafeUsMax, camInJump, camInJmax, camInDataBad, camInFl[6], camGeomTris, camGeomOver, cgCalls, cgUsRay, cgUsStep, cgStepN, cgReuse, cgHits, cgDone, cgVox, cgTotTri, cgSkipNy, cgS[0], cgS[1], cgS[2], cgS[3], cgS[4], cgS[5], cgS[6], cgS[7], cgS[8], cgS[9], cgS[10], cgS[11], phJump, phJmax, phDmax, phClamp, jsBuf[0] ? jsBuf : "-", dgFix, camSkipN, cpT[0], cpT[1], cpT[2], cpT[3], cpT[4], cpT[5], cpT[6], cpT[7], cgBuildN, cgBuildUs, cgScanN, cgScanUs, cgPf[0], cgPf[1], cgPendHit, cgPendFull, cgPendOver, cgPendMax, cgIdleSkip, cgPendBad); smkWrite(s); cgPendHit = cgPendFull = cgPendOver = cgPendMax = cgIdleSkip = cgPendBad = 0; cgPf[0] = cgPf[1] = 0; cgBuildN = cgBuildUs = cgScanN = cgScanUs = 0; dgFix = 0; camSkipN = 0; memset(cpT, 0, sizeof cpT); jsBuf[0] = 0; jsN = 0; jsSec = -1; jsMax = 0.f; phJump = phClamp = 0; phJmax = phDmax = 0.f; cgCalls = cgHits = cgReuse = cgUsRay = cgUsStep = cgStepN = 0; camInDataBad = 0; camSafeUs = camSafeUsMax = camSafeUsN = 0; camInJump = 0; camInJmax = 0.f; camInRelOk = 0;
          camInOn = 1; cgProf = 1; camInFr = camInAny = camInData = 0; memset(camInFl, 0, sizeof camInFl); camInNearMin = 9.f; camInT0 = smkT + sc.a; }   // правка 331: замер (гейт tests/camera/camin.py); N — пропустить N с
        else if (!strcmp(sc.name, "camturnold")) camTurnOld = sc.a != 0.f;   // правка 362: L/R камеры без инверсии, как было (красный)
        else if (!strcmp(sc.name, "boomcamold")) { boomCamOld = sc.a != 0.f; homerCamHeroOld = boomCamOld; }   // правка 361: камера держит горящую машину, после выброса — по курсу машины (красный)
        else if (!strcmp(sc.name, "invcam")) { cfg.invertCam = sc.a != 0.f; char s_[48]; snprintf(s_, sizeof s_, "INVCAM %d\n", cfg.invertCam); smkWrite(s_); }   // служебное: значение настройки 362
        else if (!strcmp(sc.name, "camgroundold")) camGroundOld = sc.a != 0.f;   // правка 284: без жёсткого пола камеры (красный)   // правка 286: угнанная машина после взрыва целая (красный)   // правка 290: пропы насквозь для Гомера (красный)
        else if (!strcmp(sc.name, "cfxold")) cfxOld = sc.a != 0.f;   // 04.10: красный контроль — без эффекта цели SetCollectibleEffect (viewer/collectfx.c)
        else if (!strcmp(sc.name, "mkshadowold")) mvMarkerShadowOld(sc.a != 0.f);   // 04.10: красный контроль — маркер цели без пятна на земле
        else if (!strcmp(sc.name, "spbillnear") && haveSP && haveHomer) {   // служебное: билборды стейтпропов ближе a м к Гомеру
          float R = sc.a > 0.f ? sc.a : 6.f;
          for (unsigned i = 0; i < sprops.ninst; i++) { const SpRun *r = &sprops.run[i]; const SpType *T = &sprops.type[sprops.inst[i].type];
            const SpInst *I_ = &sprops.inst[i]; if (!T->nBill || hypotf(I_->x - homer.x, I_->z - homer.z) > R) continue;
            for (unsigned b = 0; b < T->nBill; b++) { float v[12]; uint32_t col = 0, bfl = 0; int bt = -1;
              int ok = spBillQuad(&sprops, i, b, homer.x, homer.y + 2.f, homer.z - 5.f, v, &col, &bt, &bfl); const SpBill *B = &sprops.bill[T->firstBill + b];
              char t[200]; snprintf(t, sizeof t, "SPBILL i=%u type=%u b=%u ok=%d alive=%d mode=%d tex=%d col=%08x fl=%08x w=%.2f pos=%.2f,%.2f,%.2f v0=%.2f,%.2f,%.2f d=%.1f\n",
                i, sprops.inst[i].type, b, ok, r->alive, B->mode, bt, (unsigned)col, (unsigned)bfl, B->w, I_->x, I_->y, I_->z, v[0], v[1], v[2], hypotf(r->x - homer.x, r->z - homer.z)); smkWrite(t); } } }
        else if (!strcmp(sc.name, "propnear") && haveProps && haveHomer) {   // служебное (правка 290): ближайшие пропы к Гомеру
          for (int pass = 0; pass < 4; pass++) { float bd = 1e9f; int bi = -1; static int used[4];
            for (unsigned i = 0; i < props.ninst; i++) { int u = 0; for (int q = 0; q < pass; q++) u |= used[q] == (int)i; if (u || props.st[i].hidden) continue;
              float d = hypotf(props.st[i].x - homer.x, props.st[i].z - homer.z); if (d < bd) { bd = d; bi = (int)i; } }
            if (bi < 0) break; used[pass] = bi; const PropState *q = &props.st[bi];
            char t[160]; snprintf(t, sizeof t, "PROPNEAR i=%d type=%u pos=%.2f,%.2f,%.2f half=%.2f,%.2f,%.2f d=%.1f\n", bi, props.def[bi].type, q->x, q->y, q->z, q->hx, q->hy, q->hz, bd); smkWrite(t); } }   // правка 285: прежняя мера близости угона (красный)   // правка 279: прежний пульт после ролика (красный контроль)
        else if (!strcmp(sc.name, "dloff")) dlOff = sc.a != 0.f;
        else if (!strcmp(sc.name, "stripscompare")) {stripsCapture=sc.a!=0.f?(int)sc.a:0;rfCapture=stripsCapture!=0;}   // К4 гейт: кадр замораживается, пять проходов: чётные полосами, нечётные списком из развёртки (2 — красный контроль, проход 3 половиной треугольников); нужен «flag dloff 1»
        else if (!strcmp(sc.name, "stripslist")) stripsAsList = (int)sc.a;   // К4 гейт: полосы списком из развёртки (stripdraw.h)
        else if (!strcmp(sc.name, "jitter")) { jitterOn = sc.a != 0.f; if (sc.a >= 5.f && sc.a <= 90.f) jitterPct = (int)sc.a; jitHave = 0; }   // эксперимент jitter.h: flag jitter [доля прошлого кадра, %]; 0 — выкл
        else if (!strcmp(sc.name, "rfcompare")) rfCapture = 1;
        else if (!strcmp(sc.name, "rfbreak")) rfBreak = sc.a != 0.f;
        else if (!strcmp(sc.name, "rfold")) rfOld = (int)sc.a & 7;   // 05.10 ускорение отрисовки (render_fast.h): биты прежнего 1 О11, 2 О4, 4 О2; 7 — всё прежнее
        else if (!strcmp(sc.name, "savefail")) svmFailFirst((int)sc.a);   // правка 274: железо отвергает прежний набор (savefail 388 = 0x80110388), удачный забыт
        else if (!strcmp(sc.name, "saveold")) svmOldPath(sc.a != 0.f);   // правка 274: красный контроль — только прежний путь, без запасных
        else if (!strcmp(sc.name, "savefileoff")) svmFileOff(sc.a != 0.f);   // правка 274: без страховки файлом (красный контроль)
        else if (!strcmp(sc.name, "svrescan")) { svFileCut(0); svmScan(); }   // №1170: «перезагрузка» после обрыва: слоты читаются заново (SVSCAN), как при запуске игры (гейт tests/savegame/emu.sh)
        else if (!strcmp(sc.name, "svcut")) svFileCut(sc.a != 0.f);   // №1170: «обрыв питания» посреди записи слота файлом (гейт tests/savegame/emu.sh, прогон 5)
        else if (!strcmp(sc.name, "saveutiloff")) svmUtilOff(sc.a != 0.f);   // правка 274: утилита отвергает всё — живёт ли сохранение файлом
        else if (!strcmp(sc.name, "savematrix")) svmMatrix();   // правка 274: замер всех вариантов на тестовой записи T8 (на PSP — флаг-файл savematrix)
        else if (!strcmp(sc.name, "dlnolist")) dlNoList = sc.a != 0.f;   // правка 275: служебно — клетки целиком без листов
        else if (!strcmp(sc.name, "phonewarmold")) phoneViewWarmOld(sc.a!=0.f);
        else if (!strcmp(sc.name, "gagskinstreamold")) gagSkinStreamOld=sc.a!=0.f;
        else if (!strcmp(sc.name, "collsplitold")) collSplitOld=sc.a!=0.f;
        else if (!strcmp(sc.name, "collhold")) collHoldMs=(unsigned)sc.a;   // Пропечка-96: до загрузки уровня
        else if (!strcmp(sc.name, "policewarmold")) policeModelWarmOld=sc.a!=0.f;
        else if (!strcmp(sc.name, "stripbench")) { if (sc.a != 0.f) sbRun(&fbp0); }   // тур полос (stripbench.h): «flag stripbench 1» в сценарии тура; отчёт stripbench.txt и строки STRIPBENCH в журнал смоука
        else if (!strcmp(sc.name, "ggbatchold")) ggBatchOld=sc.a!=0.f;
        else if (!strcmp(sc.name, "ggbatchcompare")) {ggBatchCapture=1;rfCapture=1;}
        else if (!strcmp(sc.name, "clipsmall")) clipSmallOn = sc.a != 0.f;   // слово Роба: малые пропы и прохожие не прячутся clip guard (как PS2)
        else if (!strcmp(sc.name, "pv2off")) { pv2Off = sc.a != 0.f; pv2Cur = pv2CurCell = -1; }
        else if (!strcmp(sc.name, "ggold")) ggMode = (int)sc.a;   // правка 340: 1 — прежнее (треугольники у края кадра как есть, GE выбрасывает), 2 — срез в любом кадре
        else if (!strcmp(sc.name, "gglog")) { ggLog = sc.a != 0.f; char s_[220]; snprintf(s_, sizeof s_, "GGSTAT t=%.1f mode=%d frames_on=%u tris=%u pieces=%u cells=%u exam=%u us=%u us_max=%u near_min=%.2f\n",
                                                smkT, ggMode, ggFramesOn, ggTrisAll, ggPiecesAll, ggCellsAll, ggExamAll, ggUsAll, ggUsMax, ggNearMin < 1e8f ? ggNearMin : -1.f); smkWrite(s_); }   // правка 340: счётчики среза
        else if (!strcmp(sc.name, "feold265")) feMipOld = (int)sc.a;   // правка 265: красный контроль — 1 прежний мип гостиной (AUTO), 2 самый мелкий мип (имитация)
        else if (!strcmp(sc.name, "feframelog")) feFrameLog = sc.a != 0.f ? feFrameLine : 0;   // правка 293: перелёт меню — строка FEFR на кадр (тур TEST 9)
        else if (swapFlag(&sc)) {}   // правка 293: смена буфера (swapold, swaplate) и кадры перелёта меню в ОЗУ (fecap) — тур TEST 10
        else if (!strcmp(sc.name, "fe265sweep")) feSweep = sc.a != 0.f;   // правка 265: служебно — следующий перелёт меню покадрово, прежний мип и новый (frontend3d.h)
        else if (!strcmp(sc.name, "dlnodepth")) dlNoDepth = sc.a != 0.f;   // правка 275: красный контроль (лист без границ глубины)   // правка 275: прежний путь по кускам (красный контроль, сравнение)   // правка 270: прежняя ширина следа (красный контроль)
        else if (!strcmp(sc.name, "meprobe")) {   // правки 269/273/276, ступень 0: проба Media Engine — только тур на PSP (viewer/me/meprobe.c,
          int meProbeRun(const char *, const char *, int, char *, int);   // модуль ядра sharme.prx); в эмуляторе «ME skip emu», файл nome — «ME skip nome»;
          char md[256], mr[320]; sibling(md, ""); meProbeRun(md, "sharme.prx", sc.a > 1.5f ? 0 : smokeIsEmu(), mr, sizeof mr); svLog(mr); }   // «flag meprobe 2» — красный контроль (защита эмулятора снята)
        else if (!strcmp(sc.name, "movie")) {   // №TEST69: ролик по номеру подряд (служебно, тур): 0..15 — foxlogo vuglogo radlogo intro fmv1a fmv2..fmv8 gracie loot tele demo; строка MSNFMV name= frames= shown= dropped=
          static const char *const MV_[16] = {"foxlogo", "vuglogo", "radlogo", "intro", "fmv1a", "fmv2", "fmv3", "fmv4", "fmv5", "fmv6", "fmv7", "fmv8", "gracie", "loot", "tele", "demo"};
          int k_ = (int)(sc.a + .5f); if (k_ >= 0 && k_ < 16) { unsigned t0_ = sceKernelGetSystemTimeLow(); char nm_[32]; snprintf(nm_, sizeof nm_, "%s.rmv", MV_[k_]); msnMovie(nm_, &fbp0);
            char l_[96]; snprintf(l_, sizeof l_, "MOVIEWALL k=%d name=%s wall=%.2f t=%.1f\n", k_, MV_[k_], (sceKernelGetSystemTimeLow() - t0_) * 1e-6f, smkT); smkWrite(l_); } }
        else if (!strcmp(sc.name, "mefmv")) {   // TEST 10: после ступени, на которой ME замолчал, ролик не играть (задание прошивки повесило бы PSP)
          int meLadderStuck(void);
          if (meLadderStuck()) svLog("MSNFMV skip: ME stuck\n"); else msnMovie("gracie.rmv", &fbp0); }
        else if (!strcmp(sc.name, "melad")) {   // TEST 10: лесенка Media Engine, ступень N (viewer/me/meprobe.c meLadderRun, sharme.c meLadderStep)
          int meLadderRun(const char *, int, int, char *, int);
          char md[256], mr[320], st[8]; sibling(md, ""); snprintf(st, sizeof st, "%d", (int)(sc.a + .5f)); bbOp("melad", st, 1);   // ящик: ступень ДО вызова
          meLadderRun(md, smokeIsEmu(), (int)(sc.a + .5f), mr, sizeof mr); svLog(mr); }   // правки 269/276: короткий ролик после пробы — живы ли задания ME прошивки (кадры JPEG, строка MSNFMV)
        else if (!strcmp(sc.name, "skidlog")) { SkidStat k; skidGetStat(&k);   // гейт tests/skid: следы шин сейчас
          char s[176]; snprintf(s, sizeof s, "SKID t=%.1f quads=%d drawn=%d live=%d made=%d lvl=%.2f v=%.2f slip=%d us=%.0f skip=%d ai=%d\n",
                                smkT, k.quads, k.drawn, k.live, k.made, k.lvl, car.v, car.slip, k.us, k.skipped, k.madeAi); smkWrite(s); }
        else if (!strcmp(sc.name, "homerv")) {   // гейт tests/controls: скорость Гомера сейчас и наибольшая на земле с прошлой строки, удары
          char s[160]; snprintf(s, sizeof s, "HOMERV t=%.1f v=%.2f vmax=%.2f state=%d grounded=%d jumps=%u hits=%u stomps=%u onfoot=%d oldpad=%d\n",
                                smkT, haveHomer ? homer.speed : 0.f, hmVmax, haveHomer ? homer.state : -1, haveHomer ? homer.grounded : 0,
                                hmJumps, hmHits, hmStomps, onFoot(), G.footPadOld); smkWrite(s); hmVmax = 0.f; }
        else if (!strcmp(sc.name, "oldfootpad")) G.footPadOld = 1;
        else if (!strcmp(sc.name, "sfxlog")) { sfxLogOn = 1; sndLogHook(sfxLine); }   // правка 135.7: журнал эффектов в smoke.txt   // отрицательный контроль tests/controls: прежняя раскладка пешком
        else if (!strcmp(sc.name, "player") || !strcmp(sc.name, "service")) {   // правка 44: режим игрока / служебный (гейт tests/svc)
          svcMode = sc.name[0] == 's'; pauseRowsSvc(); char s[48]; snprintf(s, sizeof s, "SVC mode t=%.1f svc=%d\n", smkT, svcMode); smkWrite(s); }
        else if (!strcmp(sc.name, "msn")) { msnStart(); worldCarsPlace(); if (!msnOldRoom) msnRoomSync(); chase.init = 0; smkSettle(); }   // как NEW GAME: и машины мира (правка 268)
        else if (!strcmp(sc.name, "msnoldroom")) msnOldRoom = sc.a > 0.f;   // правка 259: красный контроль
        else if (!strcmp(sc.name, "raceposold")) mcRacePosOld = sc.a > 0.f;   // правка 300: красный контроль — место в гонке по прежним долям
        else if (!strcmp(sc.name, "setsave")) { int g_ = setSaveGuard((int)sc.a); char l_[64]; snprintf(l_, sizeof l_, "SETCHECK save level=%d ok=%d\n", (int)sc.a, g_); smkWrite(l_); }   // TEST93: гейт C3 — запись на уровне без набора (тот же setSaveGuard, что в меню LOAD/RESUME)
        else if (!strcmp(sc.name, "lvreach")) { int n_ = (int)sc.a; if (n_ >= 1 && n_ <= 7) svLvPut(&lvKeep, n_, 0u, 0u); }   // №682: сохранение «дошла игра до уровня N» (гейт будки)
        else if (!strcmp(sc.name, "lvprogold")) gameLvProgOld = sc.a > 0.f;   // облако 05.10 (карьера): красный — прогресс миссий только текущего уровня
        else if (!strcmp(sc.name, "ledgeold")) mcLedgeOld = sc.a > 0.f;   // правка 347: красный контроль — путь машин миссий вверх по уступам срезов
        else if (!strcmp(sc.name, "racepos300")) mcRacePos300 = sc.a > 0.f;
        else if (!strcmp(sc.name, "devcollold")) devCollOld = sc.a > 0.f;   // правка 350: красный контроль
        else if (!strcmp(sc.name, "fdfixold")) loaderFixOld = sc.a > 0.f;   // ловушка 05.10: красный — загрузчик не переоткрывает файл уровня
        else if (!strcmp(sc.name, "firstarm")) firstArmNow();   // observe actual submitted first frame; fdsteal is the existing real fault
        else if (!strcmp(sc.name, "fdsteal") && loaderFd >= 0) {   // ловушка 05.10: служебно отнять файл у загрузчика, как чужое закрытие на PSP
          static SceUID hold_ = -1; char fn_[256]; sibling(fn_, "music.sap"); unsigned u_ = 0; for (int k = 0; k < (int)ncell; k++) if (cl[k].state == 2) unloadCell(&cl[k]);
          sceIoClose(loaderFd); hold_ = sceIoOpen(fn_, PSP_O_RDONLY, 0); (void)u_;
          char s_[96]; snprintf(s_, sizeof s_, "FDSTEAL fd=%d took=%d t=%.1f\n", (int)loaderFd, (int)hold_, smkT); smkWrite(s_); }   // правка 326: красный контроль — место по остатку метров (правка 300)
        else if (!strcmp(sc.name, "camguardoff")) camGuardOff = sc.a > 0.f;
        else if (!strcmp(sc.name, "trdetail")) trDetailOn = sc.a > 0.f;
        else if (!strcmp(sc.name, "trace")) trOn = sc.a > 0.f;   // след 259/252: 3 кадра по фазам в trace.txt после камеры миссии и разбития машины   // след 259: куски города и комнаты по строке в первом кадре камеры миссии   // правки 259/252: красный контроль камеры кадра   // пункт 8: служебный запуск миссий (как NEW GAME; загрузка — не кадр игры)
        else if (!strcmp(sc.name, "loadblinkoff")) loadBlinkOff = 1;
        else if (!strcmp(sc.name, "loadpumpoff")) loadPumpOff = 1;   // правка 244: красный контроль — кадры газеты только между чтениями   // правка 244: красный контроль — LOADING горит всё время
        else if (!strcmp(sc.name, "msnpass")) mvServicePass("flag");   // 06.11: засчитать текущий этап, как выполненная цель
        else if (!strcmp(sc.name, "msnbotpass")) msnBotPass = sc.a;   // 06.11: помощник засчитывает этап, стоящий дольше N с (0 — нет)
        else if (!strcmp(sc.name, "msnbot")) { msnBot = sc.a != 0.f; msnBotT = 0.f; if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "MSNBOT on=%d\n", msnBot); smkWrite(s_); } }   // №бот миссий (MSNBOT_OLD)
        else if (!strcmp(sc.name, "roomsync")) { msnRoomSync(); chase.init = 0; smkSettle(); }   // ag/loadorder служебное: вход в комнату миссии по положению Гомера (проверка msnRoomSync после «падения» в пустоте)
        else if (!strcmp(sc.name, "msnroomdeep1209old")) msnRoomDeep1209Old = (int)sc.a;   // ag/loadorder: 1 — msnRoomSync ищет комнату только в окне 3 м, как было
        else if (!strcmp(sc.name, "msngo")) { mvServiceGo(0.f); chase.init = 0; smkSettle(); }   // к текущей цели (как goto; посадка — к двери)
#ifndef C3_WARDROBE_TEST_OLD
        // Codex-3 wardrobe gate: open the real screen without staging a room trip.
        // Purchases, previews and homerWear still use the normal frame/input path.
        else if (!strcmp(sc.name, "wardtestopen")) { char sd[256]; levelFile(sd, ""); wardrobeOpen1233(sd, skinWorn); smkSettle(); }
#endif
        else if (!strcmp(sc.name, "uifitfile")) {   // №619 (служебно): тексты из uifit.txt рядом с EBOOT (по строке) через настоящую раскладку пузыря задания; a=1 — пузырь без значка
          char fn[256]; sibling(fn, "uifit.txt"); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0);
          if (fd >= 0) { static char buf[65536]; int n = sceIoRead(fd, buf, sizeof buf - 1); sceIoClose(fd); if (n < 0) n = 0; buf[n] = 0;
            uiMessage("GO", sc.a > .5f ? -1 : UI_OBJICON_EXTERNAL, 1000.f); int all = 0, bad = 0;
            for (char *ln = strtok(buf, "\n"); ln; ln = strtok(NULL, "\n")) { int lines = 0, ml = 0, w = 0, bw = 0, gw = 0, gh = 0;
              int ok = uiTextFit(ln, &lines, &ml, &w, &bw); uiLastBubbleGrow(&gw, &gh); all++; bad += !ok; char o[320];
              snprintf(o, sizeof o, "UIFITF %s lines=%d/%d w=%d/%d grow=+%dw+%dh \"%.80s\"\n", ok ? "ok" : "over", lines, ml, w, bw, gw, gh, ln); smkWrite(o); }
            char o[64]; snprintf(o, sizeof o, "UIFITF total=%d over=%d\n", all, bad); smkWrite(o); }
          smkSettle(); }
        else if (uiSfxService(sc.name, sc.a)) smkSettle();   // гейт анимаций HUD: пульс △, кольцо H&R, всплывание, звуки
        else if (!strncmp(sc.name, "ui", 2)) { mvServiceUi(sc.name, sc.a); smkSettle(); }   // пункт 8: проверки HUD (служебные строки — не кадр игры)
        else if (!strcmp(sc.name, "msnselchkoff")) mvMsnSelChkOff((int)sc.a);   // правка 432: самопроверка MISSION SELECT молчит (красный контроль)
        else if (!strcmp(sc.name, "msnselbreak")) mvMsnSelBreak((int)sc.a);       // правка 432: испортить состояние открытого меню (отрицательный контроль)
        else if (!strcmp(sc.name, "msnselfuzz")) { msFuzzOn = 1; msFuzzS = (unsigned)sc.a * 2654435761u + 12345u; msFuzzMax = sc.b > 0.f ? (int)sc.b : 200; msFuzzSeq = msFuzzBad = msFuzzSteps = 0; }   // правка 432: бот меню: случайные последовательности кнопок
        else if (!strcmp(sc.name, "msopenfail")) msOpenFail = (int)sc.a;   // №894: отказ открытия пакета MISSION SELECT N раз (гейт tests/menu896)
        else if (!strcmp(sc.name, "wdbreak")) { wdBreakMask = (int)sc.a; }   // приёмка сторожа (CLAUDE.md п.12): биты WDB_* (game.h) — искусственно сломать вид; 0 — снять
        else if (!strcmp(sc.name, "msnsel")) { mvServiceSelect((int)sc.a); chase.init = 0; smkSettle(); }   // миссия N (MISSION SELECT)
        else if (!strcmp(sc.name, "msnswitchold") || !strcmp(sc.name, "mscursorold")) mvServiceSwitchOld(sc.name, (int)sc.a);   // №943: красный контроль гейта tests/msnhome (прежняя смена миссии / курсор)
        else if (!strcmp(sc.name, "lvdeferold")) lvDeferOld = (int)sc.a;   // №943: прежнее — стартовая цель уровня ставится перед выбранной строкой
        else if (!strcmp(sc.name, "sphits") && haveSP) { int nf_ = 0, nl_ = 0; float thm_ = 0.f; for (unsigned i_ = 0; i_ < sprops.ninst; i_++) { if (sprops.run[i_].fall == 1) nf_++; if (sprops.run[i_].fall == 2) nl_++; if (sprops.run[i_].fall && sprops.run[i_].fallTh > thm_) thm_ = sprops.run[i_].fallTh; } char l_[320]; int o_ = snprintf(l_, sizeof l_, "SPHITS hits=%d last=%.1f hardV=%.1f v=%.1f car=%.1f,%.1f falling=%d lying=%d maxdeg=%.0f t=%.2f ids=", sprops.hits, sprops.lastImpact, car.hardV, car.v, car.x, car.z, nf_, nl_, thm_ * 57.2958f, smkT); for (unsigned i_ = 0, k_ = 0; i_ < sprops.ninst && k_ < 12 && o_ < 280; i_++) if (sprops.run[i_].fall) { o_ += snprintf(l_ + o_, sizeof l_ - o_, "%u,", i_); k_++; } snprintf(l_ + o_, sizeof l_ - o_, "\n"); smkWrite(l_); }   // №998: ids — какие стейтпропы падают (номера экземпляров)
        else if (!strcmp(sc.name, "polefall975old")) spPoleFallOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "bubble992")) uiBubble992Set(sc.a == 0.f);   // №992: 0 — появление пузыря как в TEST70, 1 — короче, текст вместе с рамкой
        else if (!strcmp(sc.name, "carmtxold")) carMtxOld = sc.a != 0.f;
        else if (!strcmp(sc.name, "lvcheckold")) lvCheckOld = sc.a != 0.f;   // №1195: 1 — таблицы level.shr не проверять (красный контроль; действует на следующую загрузку уровня)
        else if (!strcmp(sc.name, "gebench") && sc.a != 0.f) { geBenchRun(smokeIsEmu()); resetState(); sceGuStart(GU_DIRECT, list); sceGuFinish(); sceGuSync(0, 0); }   // Н8–10/РОВНЫЕ_30 шаг 0: стенд цен вызова ядра, матрицы, привязки, состояний (gebench1.h); строки GEBENCH
        else if (!strcmp(sc.name, "caroutline")) carOutline = sc.a < 0.f ? 0 : (int)sc.a > 2 ? 0 : (int)sc.a;   // 0 — A (как сейчас), 1 — B, 2 — C
        else if (!strcmp(sc.name, "lockhold")) {   // Н8–10 служебно: «flag lockhold N» — держать замки, которые никто не отпустит (бит 1 — stripScratch, бит 2 — marksSema); 0 — отпустить. Проверка предела: строки LOCK в smoke.txt
          int m_ = (int)sc.a; static int held_;
          if ((m_ & 1) && !(held_ & 1)) { stripScratchLock = 1; held_ |= 1; } if (!(m_ & 1) && (held_ & 1)) { stripScratchLock = 0; held_ &= ~1; }
          if ((m_ & 2) && !(held_ & 2) && marksSema >= 0) { sceKernelWaitSema(marksSema, 1, 0); held_ |= 2; } if (!(m_ & 2) && (held_ & 2)) { sceKernelSignalSema(marksSema, 1); held_ &= ~2; } }
        else if (!strcmp(sc.name, "locklimold")) relLockOld = sc.a != 0.f;   // №1195: 1 — замки без предела (красный контроль)
        else if (!strcmp(sc.name, "relstackon")) relStackOn = sc.a != 0.f;   // включить закраску стека (действует на потоки, начатые после)
        else if (!strcmp(sc.name, "relstackold")) relStackOld = sc.a != 0.f;   // №1195: 1 — зонд стека выключен
        else if (!strcmp(sc.name, "locktest")) { relNote("LOCKTEST служебная строка, проверка пути потока в ящик"); }   // №1195 служебно: «flag locktest» — строка через relNote в ящик и smoke.txt   // ТЕСТ90: 1 — матрица каждого куска машины заново (красный контроль)
        else if (!strcmp(sc.name, "glowoff1052")) glowOff1052Old = sc.a == 0.f;   // №1052: 0 — как в TEST73, 1 — крупные аддитивные куски с текстурой не рисуются
        else if (!strcmp(sc.name, "hzoff1042")) hzOff1042Old = sc.a == 0.f;   // №1042: 0 — лента горизонта как в TEST72, 1 — без ленты
        else if (!strcmp(sc.name, "pole1046")) pole1046Old = sc.a == 0.f;   // №1046: 0 — столб валится вяло, как в TEST73; 1 — от толчка бампера
        else if (!strcmp(sc.name, "boothlr1057")) boothLR1057Old = sc.a == 0.f;   // №1057: 0 — в будке L R не повторяются, 1 — повторяются
        else if (!strcmp(sc.name, "mselr1058")) mvMsLR1058Set(sc.a == 0.f);   // №1058: 0 — в Mission Select уровень только ◄ ►, 1 — ещё L R
        else if (!strcmp(sc.name, "jzgate705")) carJumpZoneGateOld = sc.a == 0.f;   // №705 ч.2: 0 — разгон тонких зон прыжка как в TEST73, 1 — окно не короче 2*(6 м + полудлина машины)
        else if (!strcmp(sc.name, "police723")) mcPolice723Old = sc.a == 0.f;   // №723: 0 — полиция вплотную к стоящему (как до правки), 1 — не ближе 27 м, как PS2
        else if (!strcmp(sc.name, "hnrpause723")) gameHnrChasePauseOld = sc.a == 0.f;   // №723: 0 — пауза спада 3 с в погоне (как до правки), 1 — спад сразу, как PS2
        else if (!strcmp(sc.name, "boothlr1038")) boothLR1038Old = sc.a == 0.f;   // №1038: 0 — только ◄ ►, 1 — ещё L и R
        else if (!strcmp(sc.name, "radar1035")) uiRadar1035Set(sc.a == 0.f);   // №1035: 0 — как в TEST72, 1 — в помещении радар без карты
        else if (!strcmp(sc.name, "menuvec1012")) uiMenuVec1012Set(sc.a == 0.f);   // №1012: 0 — как в TEST71, 1 — плашка выделения в Cartoon
        else if (!strcmp(sc.name, "textlate1004")) uiTextLate1004Set(sc.a == 0.f);   // №1004: 0 — текст пузыря растёт с рамкой (TEST71)
        else if (!strcmp(sc.name, "carshadow")) { cfg.carShadow = sc.a != 0.f; cfgApply(); }   // №1003: служебно — строка Car Shadows (0 Off, 1 On)
        else if (!strcmp(sc.name, "roadink1008")) { roadInk1008Old = sc.a == 0.f; if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "ROADINK1008 old=%d asph=%d,%d,%d(%d) pen=%08x\n", roadInk1008Old, asphNat[0], asphNat[1], asphNat[2], asphNat[3], asphNat[3] ? skinInkTone(asphNat[0], asphNat[1], asphNat[2]) : 0u); smkWrite(l_); } }   // №1008: 0 — край асфальта как в TEST71
        else if (!strcmp(sc.name, "roadthick1023")) { roadThick1023Old = sc.a == 0.f; tlSelReset = 1; }   // №1023: 0 — как в TEST72, 1 — толстый край асфальта
        else if (!strcmp(sc.name, "pole998")) spPole998Old = sc.a == 0.f;   // №998: 0 — как в TEST71 (касание вскользь столб не сбивает), 1 — касание на ходу сбивает
        else if (!strcmp(sc.name, "card994")) uiCard994Set(sc.a == 0.f);   // №994: 0 — карточка миссии как в TEST70, 1 — её привозит рука, до этого только LOADING
        else if (!strcmp(sc.name, "colcache984old")) colGatherCacheOld = sc.a != 0.f;   // №984: 1 — широкая фаза столкновения кузова без кэша «пусто» (как было)
        else if (!strcmp(sc.name, "camcar982old")) homerCamCarOld = sc.a != 0.f;   // №982: 1 — машина у спины Гомера как стена (перескок через героя, глаз в кузове)
        else if (!strcmp(sc.name, "polewall975old")) spPoleWallOld = sc.a != 0.f;   // №975: диагностика удара о стейтпроп (флаг сценария)
        else if (!strcmp(sc.name, "glint976old")) fxGlint976Old = sc.a != 0.f;   // №976: 1 — кольцо цели и звёздочки столба как в TEST66
        else if (!strcmp(sc.name, "carlights970")) carLights970 = (int)sc.a;
        else if (!strcmp(sc.name, "bling989")) { fxBling989 = sc.a != 0.f; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "BLING989SET t=%.1f on=%d k=%.2f\n", smkT, fxBling989, BLING989_A); smkWrite(l_); } }   // №989: блёстки гэгов, свечения предметов и выключателей прозрачнее
        else if (!strcmp(sc.name, "camcfg996")) { camCfg996On = sc.a != 0.f; cfgApply(); }   // №996: 0 — множители камеры всегда 1 (как TEST70)
        else if (!strncmp(sc.name, "camuser", 7)) {   // №1151 (смоук): camusercarh / camusercard / camusertilt / camuserfov / camuserfooth / camuserfootd X — значение как из меню CAMERA, камера сразу на новом месте
          const char *n_ = sc.name + 7; float *v_ = !strcmp(n_, "carh") ? &camUserCarH : !strcmp(n_, "card") ? &camUserCarD : !strcmp(n_, "tilt") ? &camUserCarTilt : !strcmp(n_, "fov") ? &camUserCarFov : !strcmp(n_, "footh") ? &camUserFootH : !strcmp(n_, "footd") ? &camUserFootD : 0;
          if (!strcmp(n_, "aimlog")) { if (smkOn) { char l_[140]; snprintf(l_, sizeof l_, "CAMAIM1160 t=%.1f step=%d want=%.3f now=%.3f px=%.1f carH=%d carTilt=%d (camuseraimlog)\n", smkT, cfg.carAim, camUserCarAim, camAimC, camAimPx(camAimC), cfg.camHeight, cfg.carTilt); smkWrite(l_); } }   // №1160: только строка в журнал (проверка загрузки settings.bin)
          else if (!CARAIM1160_OLD && !strcmp(n_, "aim")) { cfg.carAim = (int)sc.a; cfgApply(); if (smkOn) { char l_[140]; snprintf(l_, sizeof l_, "CAMAIM1160 t=%.1f step=%d want=%.3f now=%.3f px=%.1f\n", smkT, cfg.carAim, camUserCarAim, camAimC, camAimPx(camAimC)); smkWrite(l_); } }   // №1160: camuseraim N — ступень 0..4 (как строка Car Aim)
          else if (v_) { *v_ = sc.a; camCfgSnap996 = 1; if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "CAMUSER1151 t=%.1f %s=%.3f carh=%.2f card=%.2f tilt=%.1f fov=%.2f footh=%.2f footd=%.2f\n", smkT, n_, sc.a, camUserCarH, camUserCarD, camUserCarTilt, camUserCarFov, camUserFootH, camUserFootD); smkWrite(l_); } } }
        else if (!strcmp(sc.name, "camheight") || !strcmp(sc.name, "camdist")) { if (sc.name[3] == 'h') cfg.camHeight = (int)sc.a; else cfg.camDist = (int)sc.a; cfgApply(); if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "CAMCFG996 t=%.1f on=%d height=%d dist=%d userH=%.2f userD=%.2f\n", smkT, camCfg996On, cfg.camHeight, cfg.camDist, homerCamUserH, homerCamUserD); smkWrite(l_); } }   // №996 служебно: шаг Height / Distance
        else if (!strcmp(sc.name, "coin1020")) { fxCoin1020 = sc.a != 0.f; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "COIN1020SET t=%.1f on=%d\n", smkT, fxCoin1020); smkWrite(l_); } }   // №1020: блики монет случайно, не больше двух; кадр оборота со сдвигом
        else if (!strcmp(sc.name, "glint970old")) fxGlint970Old = sc.a != 0.f;   // №970: 1 — блики ключа, будки, монет и огней машин как в TEST66
        else if (!strcmp(sc.name, "polebase975old")) spPoleBaseOld = sc.a != 0.f;   // №975: диагностика удара о стейтпроп (флаг сценария)
        else if (!strcmp(sc.name, "pole968old")) propsPole968Old = sc.a != 0.f;   // №968: 1 — столб падает физикой коробки, как до правки (PS2: шарнир у основания)
        else if (!strcmp(sc.name, "tallfall946old")) propsTallFallOld = sc.a != 0.f;   // №946: 1 — высокий предмет падает медленно, как было
        else if (!strcmp(sc.name, "propstat") && haveProps) { unsigned i_ = (unsigned)sc.a; if (i_ < props.ninst) { const PropState *p_ = &props.st[i_]; char l_[200]; snprintf(l_, sizeof l_, "PROPSTAT i=%u type=%u asleep=%d hidden=%d moved=%.2f up=%.2f prop=%.1f,%.1f car=%.1f,%.1f v=%.1f foot=%d t=%.2f\n", i_, props.def[i_].type, p_->asleep, p_->hidden, fabsf(p_->x - p_->home[0]) + fabsf(p_->z - p_->home[2]), 1.f - 2.f * (p_->q[1] * p_->q[1] + p_->q[3] * p_->q[3]), p_->home[0], p_->home[2], car.x, car.z, car.v, onFoot(), smkT); smkWrite(l_); } }   // №946: состояние предмета PRP1 № (гейт tests/poles946)
        else if (!strcmp(sc.name, "msnufo")) mvServiceUfo();   // №967
        else if (!strcmp(sc.name, "msnhud")) mvServiceHud();   // №943: строка MSNHUD — что сейчас считает целью миссия, HUD и маркер (гейт tests/msnhome)
        else if (!strcmp(sc.name, "resetrowold")) uiMenuHide(UI_MENU_PAUSE, UI_P_RESET_CAR, 0);   // отрицательный контроль правки 137: строка RESET CAR у игрока (прежний код)
        else if (!strcmp(sc.name, "pauseold")) pauseOld = 1;   // отрицательный контроль: в миссии пауза свободной езды (прежний код)
        else if (!strcmp(sc.name, "tutorial")) { tutSvc = 1; tutCalm = 0.f, tutReset(mvTutorialMode());   // служебно: подсказки Барта
          char s[96]; snprintf(s, sizeof s, "TUTORIAL mode level=%d user=%d texts=%d t=%.1f\n", mvTutorialMode(), tutUser(), uiTutorialCount(), smkT); smkWrite(s); }
        else if (!strncmp(sc.name, "tut", 3)) tutService(sc.name, sc.a);   // tutnovoice / tutvoice / tutagain N (viewer/tutorial.h)
        else if (!strcmp(sc.name, "cardsset") && haveColl)   // служебно (гейт VIEW CARDS): карточки уровня по маске a собраны
          { collectSetProgress(&coll, gameLevel, (unsigned)sc.a, collectWasps(&coll, gameLevel), collectGags(&coll, gameLevel)); bonusOpenFromColl("cardsset"); }
        else if (!strcmp(sc.name, "msnmain")) { mvServiceMain((int)sc.a); chase.init = 0; smkSettle(); }   // основная часть миссии N (гейт tests/aicar/emu.sh)
        else if (!strcmp(sc.name, "msnfinal")) { mvServiceFinal(); chase.init = 0; smkSettle(); }   // Будка-93 (1234): последняя сюжетная миссия уровня
        else if (!strcmp(sc.name, "lvsavenext1234old")) lvSaveNext1234Old = sc.a != 0.f;   // Будка-93 (1234): 1 — запись конца уровня хранит пройденный уровень, как было
        else if (!strcmp(sc.name, "msnprior")) mvServicePrior((int)sc.a);   // Будка-93 (1234): все сюжетные миссии кроме N пройдены
        else if (!strcmp(sc.name, "msnmainat")) { float v_ = sc.a != 0.f || isnan(sc.d) ? sc.a : sc.d; mvServiceMainAt((int)v_); chase.init = 0; smkSettle(); }   // отрицательный код (фаза 0, прогулка) — сырым sc.d: smoke.h режет a до 0 (помощник 302)   // правка 302: миссия с этапа (M*100+S)
        else if (!strcmp(sc.name, "msnfar")) { mvServiceGo(sc.a > 0.f ? sc.a : 30.f); chase.init = 0; smkSettle(); }   // мимо цели (отрицательный контроль)
        else if (!strcmp(sc.name, "view60")) longView = 0;
        else if (!strcmp(sc.name, "gureset")) guSimClobber();   // правка 1243: имитация сброса состояния GE после сна
        else if (!strcmp(sc.name, "workmix")) ddWorkMix = sc.a >= 0.f ? sc.a : -1.f;   // №1244: работа кадра регулятора дальности = N мс (отрицательное - снять)
        else if (!strcmp(sc.name, "ddsetting") && sc.a >= 0.f && sc.a <= 2.f) { cfg.drawDist = (int)sc.a; cfgApply(); }   // №1244: строка Draw Distance (0=150, 1=200, 2=250)
        else if (!strcmp(sc.name, "far150")) far150On = sc.a != 0.f;   // №887: даль 150 м + силуэты вкл/выкл
        else if (!strcmp(sc.name, "dalramp")) dalRampOn = sc.a != 0.f;   // рампа границы дали (dal877.h): «flag dalramp 0» — прежняя ступенька
        else if (!strcmp(sc.name, "dalimp")) dimOn = sc.a != 0.f;   // ориентиры дали (dalimp.h): «flag dalimp 0» — как до правки
        else if (!strcmp(sc.name, "dal")) { dalOn = sc.a != 0.f; dalCur = 1e9f; }   // №877: таблица дали вкл/выкл (замер A/B; до 23.10 15:20 флаг не доходил — стоял в levelService «lv*»)
        else if (!strcmp(sc.name, "drip")) dripOn = sc.a != 0.f;   // №770: столбы света вместо капель вкл/выкл
        else if (!strcmp(sc.name, "endingtest")) {
#ifndef C3_ENDING_OLD
          char ef[256]; sibling(ef, "ending.shr");
          endingRun(ef, list, resetState, swapBuf, svmDialogFrame, smkWrite, &fbp0);
#endif
        }
        else if (!strncmp(sc.name, "sv", 2)) saveService(sc.name);   // сохранения: svmark / svlog / svnowrite (tools/save_gate.sh)
        else if (!strncmp(sc.name, "lv", 2)) { levelService(sc.name, sc.a); smkSettle(); }   // уровни: lvwin / lvlog (tests/levels)
        else if (!strcmp(sc.name, "dropnpc")) {   // положительный контроль правки 431: клетки под городскими персонажами миссии выгружаются, персонажи обязаны скрыться (строки NPCSCENE431 hide), потом вернуться
          int dn = 0; for (int k = 0; k < 24; k++) { float nx, nz; if (!mvNpcCityPos(k, &nx, &nz)) continue;
            for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2 || nx < c->bb[0] || nx > c->bb[3] || nz < c->bb[2] || nz > c->bb[5]) continue; unloadCell(c); dn++; } }
          char s_[80]; snprintf(s_, sizeof s_, "FAULT dropnpc cells=%d %.1f\n", dn, smkT); smkWrite(s_); }
        else if (!strcmp(sc.name, "scenehold")) { smkSceneHoldEnd = smkT + (sc.a > 0.f ? sc.a : 3.f); char s_[80]; snprintf(s_, sizeof s_, "FAULT scenehold until %.1f\n", smkSceneHoldEnd); smkWrite(s_); }
        else if (!strcmp(sc.name, "dropdoor")) {   // P0.2/A1 Опора-93: клетки города у двери, из которой Гомер выйдет из комнаты, выгружаются (холодная сцена под точкой выхода)
          int dn = 0; if (rs.room >= 0 && rs.lastDoor >= 0 && rs.lastDoor < rooms.ndoor) { const float *dp = rooms.door[rs.lastDoor].p;
            for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2 || dp[0] < c->bb[0] || dp[0] > c->bb[3] || dp[2] < c->bb[2] || dp[2] > c->bb[5]) continue; unloadCell(c); dn++; } }
          char s_[80]; snprintf(s_, sizeof s_, "FAULT dropdoor cells=%d %.1f\n", dn, smkT); smkWrite(s_); }
        else if (smokeDropCell(sc.name)) for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i];   // отрицательный контроль: видимая клетка пропадает
          if (!c->owner && c->state == 2 && boxD2(c->bb) < drawDist * drawDist && visBox(c->bb, 0, 0, 0)) { unloadCell(c); break; } }
    } }

    if ((rs.room >= 0 || rg.mem) && !onFoot()) roomLeave();   // 11.2: Гомер за рулём (RESET CAR) — он в городе, комната закрыта
    if (uiReady) { uiHudShow(UI_T_ACTION, 0); uiActionText(NULL); }   // правка 206: подпись у △ — заново каждый кадр
    if (fly || bpass) props.havePreviousCar = 0;
    float cfx, cfy, cfz, ex, ey, ez;
    if (bpass) {
      benchBefore(pending, spawnP[3], &x, &y, &z, &h); v = vy = 0;
      camx = x - sinf(h) * 7.5f; camy = y + 3.2f; camz = z - cosf(h) * 7.5f; chase.init = 0;
      ex = camx; ey = camy; ez = camz;
      cfx = x - camx; cfy = (y + 1.4f) - camy; cfz = z - camz;
      float l = sqrtf(cfx * cfx + cfy * cfy + cfz * cfz); cfx /= l; cfy /= l; cfz /= l;
    } else if (!fly && haveCol) {                  // этап 2: машина через col.c/car.c
      GamePad gp = {pad.Buttons, hit, ax, ay}; CarInput in;
      if (roomHoldActive(dt)) { gp.buttons = 0; gp.hit = 0; gp.ax = 0; gp.ay = 0; }   /* сверка g11b-collect P05 №16/Д6: 0,5 с после входа, 1,0 с после выхода из комнаты без управления */
      gameCarInput(&G, &gp, &in);   // R/X газ, L/○ тормоз, □ ручник, стик/← → руль (viewer/game.c)
      if (autodrive) { if (adClockOld || autodrive < 4) adT += dt; in.throttle = 1; in.brake = 0; in.steer = autodrive == 2 ? 0.f : sinf(adT * .7f) * .5f;   // служебный автопилот
        if (adStop > 0.f && adT > adStop) { in.throttle = 0; in.brake = car.v > .5f ? 1.f : 0.f; in.handbrake = 1.f; } }
      if (autodrive == 4) {   // автопилот теста (viewer/smkpilot.h, хост-проверка tools/bots/pilot.c): дороги трафика, объезд машин, застрял > 3 с — назад и новый путь
        adT += adClockOld ? dt : wallDt; smkPilotStep(&smkPilot, &traffic, &car, dt, &in);   // №914: срок — по часам сценария (как wait), а не по dt: у модальных экранов dt = 0
        if ((smkPilotLogT += dt) >= 1.f) { smkPilotLogT = 0.f; char s_[400]; float nd_ = 1e9f, na_ = 0.f, nv_ = 0.f; int nm_ = -1;   // ближайшая машина трафика: расстояние, «вперёд» по курсу, скорость, режим
          for (int j_ = 0; j_ < traffic.maxCars; j_++) { const TrCar *o_ = &traffic.car[j_]; if (o_->mode == TRM_OFF) continue; float dx_ = o_->car.x - car.x, dz_ = o_->car.z - car.z, d_ = hypotf(dx_, dz_); if (d_ < nd_) { nd_ = d_; na_ = dx_ * sinf(car.h) + dz_ * cosf(car.h); nv_ = o_->car.v; nm_ = (int)o_->mode; } }
          snprintf(s_, sizeof s_, "PILOT t=%.1f dist=%.0f stuck=%d goals=%d v=%.1f pos=%.1f,%.1f,%.1f h=%.2f why=%d in=%.1f/%.1f/%.2f near=%.1f ahead=%.1f nv=%.1f mode=%d tilt=%.0f roll=%.0f wh=%d car=%s\n", smkT, smkPilot.dist, smkPilot.stuckN, smkPilot.goals, car.v, car.x, car.y, car.z, car.h, smkPilot.why, in.throttle, in.brake, in.steer, nd_, na_, nv_, nm_, acosf(fmaxf(-1.f, fminf(1.f, 1.f - 2.f * (car.q[1] * car.q[1] + car.q[3] * car.q[3])))) * 57.29578f, car.roll * 57.29578f, car.wheels, car.p.name); smkWrite(s_); }
        if (adT > adStop) { char s_[128]; snprintf(s_, sizeof s_, "PILOT end t=%.1f dist=%.0f stuck=%d\n", smkT, smkPilot.dist, smkPilot.stuckN); smkWrite(s_); autodrive = 0; } }
      if (autodrive == 3) {   // путевые точки (аудит 01.10): руль на точку, скорость wpV
        while (wpI < wpN) { float dx = wpX[wpI] - car.x, dz = wpZ[wpI] - car.z; if (dx * dx + dz * dz > 64.f) break;
          char s_[96]; snprintf(s_, sizeof s_, "WP %d t=%.1f v=%.2f at %.1f %.1f %.1f\n", wpI, smkT, car.v, car.x, car.y, car.z); smkWrite(s_); wpI++; }
        if (wpI < wpN) { float e = atan2f(wpX[wpI] - car.x, wpZ[wpI] - car.z) - car.h; while (e > GU_PI) e -= 2.f * GU_PI; while (e < -GU_PI) e += 2.f * GU_PI;
          in.steer = fmaxf(-1.f, fminf(1.f, WP_STEER_SIGN * e * 3.f)); in.throttle = car.v < wpV ? 1.f : 0.f; in.brake = car.v > wpV + 3.f ? .5f : 0.f; }
        else { in.steer = 0.f; in.throttle = 0.f; in.brake = car.v > .5f ? 1.f : 0.f; } }
      if (carSeq) {   // служебное: руль вправо стоя / тормоз на 5 м/с / назад / круг с рулём вправо
        in.steer = carSeq == 1 || carSeq == 4 ? 1.f : 0.f; in.throttle = carSeq == 4 ? .6f : 0.f; in.brake = carSeq == 2 || carSeq == 3 ? 1.f : 0.f;
        if (carSeq == 2) carSetSpeed(&car, 5.f); if (carSeq == 3) carSetSpeed(&car, -1.f);
      }
      gameCarLimits(&G, &in);      // остов стоит, пешком водителя нет
      gameExitAssist(&G, &in);     // правка 168: △ на ходу — машина тормозит и Гомер выходит
      mvFreezeCar(&in);            // пункт 8: отсчёт 3-2-1 — машина на ручнике, газ и руль не проходят (и служебный автопилот)
#if !COLRING_OLD
      if (colRingG && haveCol && !inRoom()) colRingFrame(haveHomer && onFoot() ? homer.x : car.x, haveHomer && onFoot() ? homer.z : car.z);   // Коллизия-96: кольцо за игроком (до шага машины)
#endif
      XS_FRAME(smkOn); smkMark(0); BBP(BBP_CAR);
      { static float vPrev1050; float v0_ = hypotf(car.vel[0], car.vel[2]);
      camSvThr = in.throttle; camSvBrk = in.brake;   // сверка g06: газ и тормоз игрока для камеры заднего хода (main_camera.h)
      mvCarOdds = svOddsHook;   // сверка g01-cars круг 2 (P01-20): шансы машины игрока для ставки
      gameCarStep(&G, &in, dt); smkMarkX(0);
      if (smkOn && carDbgOn1050) { float v1_ = hypotf(car.vel[0], car.vel[2]);
        if ((carDbg1050.bn || carDbg1050.wn || carDbg1050.revert) || vPrev1050 - v1_ > 2.f) {
          char l_[400]; int o_ = snprintf(l_, sizeof l_, "CARCON t=%.2f x=%.2f y=%.2f z=%.2f v=%.1f->%.1f rev=%d tries=%d body=%d(%.2f)", smkT, car.x, car.y, car.z, v0_, v1_, carDbg1050.revert, carDbg1050.tries, carDbg1050.bn, carDbg1050.bdepth);
          for (int k_ = 0; k_ < carDbg1050.bn && o_ < 330; k_++) o_ += snprintf(l_ + o_, sizeof l_ - o_, " [t%d#%u n%.2f,%.2f d%.2f]", carDbg1050.btype[k_], carDbg1050.bidx[k_], carDbg1050.bnx[k_], carDbg1050.bnz[k_], carDbg1050.bd[k_]);
          o_ += snprintf(l_ + o_, sizeof l_ - o_, " wheels=%d", carDbg1050.wn);
          for (int k_ = 0; k_ < carDbg1050.wn && o_ < 380; k_++) o_ += snprintf(l_ + o_, sizeof l_ - o_, " {%u#%u n%.2f,%.2f,%.2f d%.2f}", carDbg1050.wref[k_] >> 29, carDbg1050.wref[k_] & 0x1fffffffu, carDbg1050.wn3[k_][0], carDbg1050.wn3[k_][1], carDbg1050.wn3[k_][2], carDbg1050.wdepth[k_]);
          snprintf(l_ + o_, sizeof l_ - o_, " crashes=%d reverts=%d\n", car.crashes, car.reverts); smkWrite(l_); }
        vPrev1050 = v1_; }
      carDbg1050.bn = carDbg1050.wn = carDbg1050.revert = 0; }
      { static float st_, sx_, sz_, fw_, bk_;   // №1200: застряла — см. CARSTUCK1200_OLD выше
        int on_ = !CARSTUCK1200_OLD && !carStuck1200Old && haveCol && !fly && !autodrive && !onFoot();
        if (!on_) st_ = fw_ = bk_ = 0.f;
        else { if (st_ == 0.f) { sx_ = car.x; sz_ = car.z; fw_ = bk_ = 0.f; } st_ += dt;
          if (in.throttle > .3f) fw_ += dt; if (in.brake > .3f) bk_ += dt;
          float dx_ = car.x - sx_, dz_ = car.z - sz_;
          if (dx_ * dx_ + dz_ * dz_ > 2.25f || (in.throttle <= .3f && in.brake <= .3f && st_ > 8.f)) st_ = fw_ = bk_ = 0.f;   // сдвинулась или игрок стоит без газа — заново
          // не стоит на колёсах и газует/сдаёт 4 с — вклинилась или висит; на колёсах — только если и вперёд, и назад по 1 с за 5 с не вышло (упор в стену с выездом задом не трогаем)
          else if ((car.wheels < 3 && fw_ + bk_ > CARSTUCK1200_S) || (st_ > 5.f && fw_ > 1.f && bk_ > 1.f)) { stuckReq1200 = 1;
            if (smkOn) { char s_[140]; snprintf(s_, sizeof s_, "CARSTUCK1200 at=%.1f,%.1f,%.1f wheels=%d fw=%.1f bk=%.1f t=%.1f\n", car.x, car.y, car.z, car.wheels, fw_, bk_, smkT); smkWrite(s_); }
            st_ = fw_ = bk_ = 0.f; } } }
      if (haveCarVis) {
        if (carRevert96) { carRevert96 = 0; if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "WATCHDOG car revert t=%.1f\n", smkT); smkWrite(s_); } phoneSummon(phoneK >= 0 ? phoneK : 0); }   // Build 96 (1250): модель машины игрока так и не пришла — своя машина уровня
        if (!mvForcedCar()) carVisUpdate(&cv, &car, &in, dt); else carVisSpin(&cv, &car, dt);   // №955б: колёса машины миссии/угнанной крутятся
        if (smkOn && autodrive) { static int lf_, lon_, lno_, lrv_; lf_++; if (cv.brake) { lon_++; if (in.brake <= 0.f) lno_++; } if (cv.reverse) lrv_++;   // правка 435: факт про стоп-сигналы — кадров, фонарь стоп горит, из них без тормоза во вводе, задний ход
          if ((lf_ & 255) == 0) { char l_[120]; snprintf(l_, sizeof l_, "LAMP436 t=%.1f frames=%d brake_on=%d brake_on_no_input=%d reverse_on=%d\n", smkT, lf_, lon_, lno_, lrv_); smkWrite(l_); } }
        if (oldCarOn) carVisUpdate(&oldCv, &oldCar, NULL, dt);
      }   // CAR1: each car retains its own visual state
      skidCar(0, &car, &in, &world); smkMarkX(1);   // правка 249: следы шин по правилу оригинала (viewer/skid.c)
      if (haveHomer) homer.irisOn = transitReady();   // посадка при занятой двери — под ирисом (viewer/transit.c)
      // круг 3 (g11, проверка P11-02): предметы улицы ломает любая машина, не только игрока — трафик и ИИ миссий (шкала и монеты за их удары игроку не идут). PROPOTHER_SV_OLD=1 — только машина игрока
#ifndef PROPOTHER_SV_OLD
#define PROPOTHER_SV_OLD 0
#endif
#ifndef PROPCOST1086_OLD
#define PROPCOST1086_OLD 0   // №1086: машины трафика и ИИ задевают предметы по вызовам окон g02 (trafficCarsNear, выше) и g03 (McEnv.carTouch); этот полный обход нужен только при 1 (как в TEST82)
#endif
      if (!PROPOTHER_SV_OLD && PROPCOST1086_OLD && haveProps && haveCol) {
        propsKickOther = 1;
        if (haveTraffic) for (int k_ = 0; k_ < traffic.maxCars && k_ < TR_MAXCAR; k_++) if (traffic.car[k_].mode != TRM_OFF) propsHitCar(&props, &world, &traffic.car[k_].car, NULL);
        { const MsnCars *mc_ = mvCars(); if (mc_) for (int k_ = 0; k_ < mc_->n && k_ < MC_MAX; k_++) if (mc_->c[k_].on) propsHitCar(&props, &world, (Car *)&mc_->c[k_].car, NULL); }
        propsKickOther = 0;
      }
      { static int wdInit_; if (!wdInit_) { wdInit_ = 1; homerRoomPointHook = roomPoint96; wdSetLog(wdSmkLine); wdSetClock(wdSmkClock); sceneReadyHook = sceneReadyMain; sceneWantHook = sceneWantMain; if (smkOn) trafficLogLine = smkWrite; wdRegister("cellfail", "stream", wdCellCheck, 0); } }   // сторож (watchdog.h): строки WATCHDOG в журнал смоука, время игры
      gameAfterCar(&G, &gp, dt); smkMarkX(2);   // Гомер и его удары, пропы, стейтпропы, монеты (viewer/game.c; звук и частицы — хуки выше)
      if (haveHomer && onFoot() && !inRoom()) {   // сверка g04-homer круг 3 (ГОМЕР 5.7): пешего сбивают только машины ИИ миссий (погоня, цели, полиция) быстрее 1 м/с
        const MsnCars *mk_ = mvCars();
        if (mk_) for (int i_ = 0; i_ < mk_->n; i_++) if (mk_->c[i_].on && !mk_->c[i_].car.destroyed && homerCarWorth(&homer, &mk_->c[i_].car)) {   // сверка g04-homer круг 6: отсев по расстоянию и скорости до homerCarFrom (цена PSP)
          HomerCar hk_; homerCarFrom(&hk_, &mk_->c[i_].car, mk_->c[i_].lift); hk_.hostile = 1;
          if (homerKnockedByCar(&homer, &hk_)) break;
        }
      }
      if (haveHomer && onFoot() && homer.grounded && homer.speed > hmVmax) hmVmax = homer.speed;   // гейт tests/controls: бег по ○
      if (haveAo) {   // объекты уровня: действия, кадр клипа, живые объёмы COL1 (после spUpdate — поза этого кадра)
        smkMark(1); BBP(BBP_WORLD);
        int ft = onFoot();
        if (!ft) { CarBox cbA_; carBox(&car, &cbA_); float rxA_ = -cbA_.fz, rzA_ = cbA_.fx, cA_[4][3];   // сверка g11 круг 2 (P05:9 от g09b-menu): из машины триггер — шар 1,5 м вокруг центра плюс углы коробки
          for (int q_ = 0; q_ < 4; q_++) { float sl_ = q_ & 1 ? 1.f : -1.f, sw_ = q_ & 2 ? 1.f : -1.f; cA_[q_][0] = cbA_.x + cbA_.fx * cbA_.hl * sl_ + rxA_ * cbA_.hw * sw_; cA_[q_][1] = cbA_.y; cA_[q_][2] = cbA_.z + cbA_.fz * cbA_.hl * sl_ + rzA_ * cbA_.hw * sw_; }
          aoSetCar(&ao, cA_); }
        aoStep(&ao, &sprops, &world, ft ? homer.x : car.x, ft ? homer.y : car.y, ft ? homer.z : car.z, ft, aoAct, dt); aoAct = 0;
        if (smkOn && colWire1223 > 0 && haveAo && ((int)(smkT * 1.f) != (int)((smkT - dt) * 1.f))) {   // №1223 служебно: живые объекты ANM1 рядом — поза рисунка (poseF) и центр первого ящика, раз в секунду
          float px_ = ft ? homer.x : car.x, pz_ = ft ? homer.z : car.z;
          for (unsigned o_ = 0; o_ < ao.nobj; o_++) { const AoObj *O_ = &ao.obj[o_]; if (!O_->nVol || !O_->nInst) continue; const SpInst *in_ = &sprops.inst[O_->firstInst];
            if (hypotf(in_->x - px_, in_->z - pz_) > 95.f) continue;
            const SpRun *rn_ = &sprops.run[O_->firstInst]; const ColBox *b_ = 0; for (unsigned q_ = O_->firstVol; q_ < O_->firstVol + O_->nVol && q_ < ao.nvol; q_++) if (ao.vol[q_].kind == 0 && ao.vol[q_].col < world.nbox) { b_ = &world.box[ao.vol[q_].col]; break; }
            char l_[240]; if (O_->nVol >= 10) smkGateInst = O_->firstInst; snprintf(l_, sizeof l_, "AOLOG t=%.1f obj=%u inst=(%.1f %.1f %.1f) type=%u poseF=%.1f alive=%d state=%d drawable=%d nfr=%u box0=(%.2f %.2f %.2f) gate=%u eye=(%.1f %.1f %.1f) pl=(%.1f %.1f %.1f) pb=%d baked=%u still=%d inel=%d ndyn=%u inR=%d dd=%.0f\n", smkT, o_, in_->x, in_->y, in_->z, in_->type, rn_->poseF, rn_->alive, (int)rn_->state, spDrawable(&sprops, O_->firstInst, px_, pz_, 400.f), (unsigned)O_->nfr, b_ ? b_->c[0] : 0.f, b_ ? b_->c[1] : 0.f, b_ ? b_->c[2] : 0.f, O_->firstInst == smkGateInst ? smkGateMask : 0u, eyeX, eyeY, eyeZ, px_, ft ? homer.y : car.y, pz_, pbS.on, pbS.on ? (unsigned)pbS.baked[O_->firstInst] : 0u, pbS.on ? (int)pbS.still[O_->firstInst] : -1, pbS.on ? (int)pbS.inel[O_->firstInst] : -1, pbS.ndyn, -1, drawDist); smkWrite(l_); } }
        if (uiReady && ft && !uiPaused() && aoWantsAction(&ao) >= 0) { uiHudShow(UI_T_ACTION, 1); uiActionText("Use"); } }
      if (!SVTELEPORT_OLD && svTeleportCount() && haveHomer && haveCol) {   // сверка g12-data круг 7 (P18-12): площадки Teleport (L3 R5T01, lightport, Z5T1; те же на L6)
        float pp_[3] = {homer.x, homer.y + 1.f, homer.z}; int k_ = onFoot() && !inRoom() ? svTeleportAt(pp_) : -1;
        if (svTpGo >= 0) {   // оригинал: ирис закрывается, игрок в точку локатора курсом по оси X его матрицы, камера резко, ирис открывается
          int g_ = svTpGo; svTpGo = -1; float to_[3], th_;
          if (svTeleportTarget(g_, to_, &th_)) {
            frames += transitClose(&fbp0, "teleport") / (longView ? 2 : 1);
            float dx_ = to_[0] - homer.x, dz_ = to_[2] - homer.z; if (dx_ * dx_ + dz_ * dz_ > 80.f * 80.f) { msnTeleNews1239 = 0; msnTeleport(to_[0], to_[1], to_[2], &fbp0); msnTeleNews1239 = 1; }   // далеко — город с экрана загрузки, как у переноса миссии
            homerPlace(&homer, &world, to_[0], to_[1], to_[2], th_); chase.init = 0; smkSettle(); k_ = -1;
            if (smkOn) { char s_[120]; snprintf(s_, sizeof s_, "TELEPORT pad=%d to=%.1f,%.1f,%.1f h=%.2f t=%.1f\n", g_, to_[0], to_[1], to_[2], th_, smkT); smkWrite(s_); } } }
        if (k_ >= 0 && uiReady && !uiPaused()) { uiHudShow(UI_T_ACTION, 1); uiActionText("Use"); }
      }
      { const char *tw = haveHomer ? transitHomerWhy(&homer) : 0;   // выход через верх / посадка при занятой двери: ирис
        if (tw) { frames += transitClose(&fbp0, tw) / (longView ? 2 : 1); chase.init = 0; smkSettle(); camIrisCut = !strcmp(tw, "car_in"); } }
      smkMark(2); BBP(BBP_ROOMS);
      roomPrefetchTick();   // ТЕСТ90 П1: комната ближайшей двери читается впрок
      { door1226Gate = 1; int ev = roomsStep(&rooms, &rs, &G, roomOpen, 0); door1226Gate = 0;   // 11.2: пешком у двери — в комнату, у выхода — к двери (viewer/rooms.c)
        rs.action = 0;                                      // правка 41: △ этого кадра двери отдан (или нет)
        { int jp; if (!ev && uiReady && !uiPaused() && !mvWantsAction() && jackNear(&jp) >= 0) { uiHudShow(UI_T_ACTION, 1); uiActionText("Get Into Car"); } }   // 306: подпись миссии у собеседника не перебивается   // правка 63: △ у чужой машины
        { int pk_ = phoneNear(); pbStep(&pbArm, pk_, !mvBoothsOff());   // ps2-twin P05:25: каждый кадр — вошёл ли в будку при включённых (viewer/phonebusy.h)
          if (!ev && uiReady && !uiPaused() && pbHint(&pbArm, pk_, !mvBoothsOff())) { uiHudShow(UI_T_ACTION, 1); tutEvent(TUT_PHONE); uiActionText("Use Telephone Booth"); } }   // TUTORIAL_009   // правка 62: △ у будки; подсказка TUTORIAL_009
        if (!ev && uiReady && onFoot() && !uiPaused() && inRoom() && (SHOP_PHASE_OLD || mvShopAllowed()) && wardrobeNear(homer.x, homer.y, homer.z)) { uiHudShow(UI_T_ACTION, 1); uiActionText("Change Clothes"); }   // правка 118: △ у места костюмов (PurchaseSkin)
        if (!ev && uiReady && onFoot() && !uiPaused() && roomsDoorNear(&rooms, &rs, &G)) { uiHudShow(UI_T_ACTION, 1); uiActionText(inRoom() ? "Go Outside" : "Go Inside");   // TUTORIAL_013 «PRESS @ TO GO INSIDE»   // △ у двери / выхода
          if (rs.room < 0) tutEvent(TUT_INSIDE); }   // подсказка «△ — войти» (PS2 2:55, у двери Kwik-E-Mart)
        if (ev == ROOM_EV_EXIT) { roomExitAtGate(); roomClose(); e1002N = 14; zoneReloadWorld("room-exit"); }   // сверка g09b-menu круг 5 (P12-03): выход из помещения — окружающие зоны заново
#ifndef JM_ROOM_OLD
#define JM_ROOM_OLD 0   // сверка g10b-loop круг 2 (g08 P09:15): 1 — режим «только музыка» на переходе в помещение не включается (прежнее)
#endif
#ifndef MSN_STATE_OLD
#define MSN_STATE_OLD 0   // сверка g10b-loop круг 2 (g07 P06-11, P07:32): 1 — миссии не сообщают переход в помещение, арест и имя интерьера (прежнее)
#endif
        if (ev && !JM_ROOM_OLD) { sndJustMusic(1); jmRoomOn = 1; }   // круг 2 (g08 P09:15): на время перехода в помещение и из него — только музыка
        if (ev && !MSN_STATE_OLD) mvSetState(1, 0, ev > 0 ? roomName() : "");   // круг 2: на время ириса входа/выхода таймер и провалы миссии стоят, имя интерьера — миссии
        if (ev) { if (ev > 0 && irisPre1226) { frames += irisPre1226Frames / (longView ? 2 : 1); irisPre1226 = 0; } else frames += transitClose(&fbp0, ev > 0 ? "room_in" : "room_out") / (longView ? 2 : 1); chase.init = 0; roomLog(ev); smkSettle(); roomHoldStart(ev);
          if (ev < 0 && !ROOMEXITRING1209_OLD && !roomExitRing1209Old && haveHomer) { float rc_[3] = {homer.x, homer.y, homer.z}; roomRing1209(rc_, "exit"); } } }   // ag/loadorder: город у двери готов, пока ирис закрыт   // ирис: правило порта по IrisWipe.scr   // переход — как goto: кадр чтения комнаты счётчики сцены не берут
      mvSetInterior(roomName());   // пункт 8 + 11.2: миссия видит комнату, где Гомер (Апу, ведёрко; NULL — город)
      if (jmRoomOn && !JM_ROOM_OLD) { sndJustMusic(0); jmRoomOn = 0; }   // круг 2: переход кончился (ирис блокирующий, к этому кадру он уже прошёл)
      mvWorldCarsStep(onFoot() ? homer.x : car.x, onFoot() ? homer.z : car.z);   // круг 5 (P03-20): свободная машина уходит и возвращается вместе с зоной
      if (!MSN_STATE_OLD) mvSetState(0, G.hnrBusted > 0.f || bustFadeT >= 0.f, "");   // круг 2: переход кончился; арест — по состоянию полиции
      { const char *g_ = sndGetOut(haveHomer && homer.state == HS_GET_OUT); if (g_ && smkOn) { char s_[80]; snprintf(s_, sizeof s_, "SAY goc %s t=%.1f\n", *g_ ? g_ : "-", smkT); smkWrite(s_); } }   // правка 345: выход из машины — реплика водителя W_GOC_ (sound.c)
      { static int hs_ = -1; int st_ = haveHomer ? (int)homer.state : -1;
        const char *g_ = sndGetIn((!SV_GIC_OLD && st_ == HS_TO_DOOR) || st_ == HS_GET_IN || (st_ == HS_IN_CAR && hs_ == HS_TO_DOOR)); hs_ = st_;   // сверка g08-sound (P05 Д5): GIC — в начале посадки, как только △ принят (до подхода к двери)
        if (g_ && smkOn) { char s_[80]; snprintf(s_, sizeof s_, "SAY gic %s t=%.1f\n", *g_ ? g_ : "-", smkT); smkWrite(s_); } }
      { // правка Роба 235 (03.10: «в магазине у Апу — музыка магазина»): l1_music.rms OF_KiwkEMart -> KwikEmart_region (Kwik_E_Mart_Muzak,
        // петля); вышел из комнаты — музыка миссии снова (её событие), иначе тишина до следующего события (сел в машину — свободная езда)
        static const char *prevRoom; const char *rn = roomName();
        if (rn != prevRoom) {
          { const char *a_ = sndRoom(rn); if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "ROOMAMB room=%s amb=%s t=%.1f\n", rn ? rn : "-", a_, smkT); smkWrite(s_); } }   // правка 345: атмосфера комнаты (sound.c sndRoom)
          int kwik = rn && !strncasecmp(rn, "Kwik", 4), wasKwik = prevRoom && !strncasecmp(prevRoom, "Kwik", 4);
#ifndef ROOM_TUBA_OLD
#define ROOM_TUBA_OLD 0   // сверка g10b-loop круг 5 (4.4, P09:17): 1 — в комнатах без своей музыки тишина, на выходе туба по таймеру (прежнее)
#endif
          if (!ROOM_TUBA_OLD && !kwik && (rn || prevRoom)) sndTubaNow();   // круг 5: свою музыку имеет только Kwik-E-Mart; остальные комнаты играют тубу прогулки (Tuba_005/012), на выходе на улицу — сразу тубу
          if (kwik) sndMusic(SND_MUS_KWIKEMART);
          else if (wasKwik) { if (sndMusicOwned()) mvMusicResume(); else sndMusic(SND_MUS_OFF); }
          prevRoom = rn;
        } }
      smkMark(3); BBP(BBP_MSN);
      if (smkOn && !gameMsnClockUs) { gameMsnClockUs = sceKernelGetSystemTimeLow; mcClockUs = sceKernelGetSystemTimeLow; }
      unsigned p4a_ = sceKernelGetSystemTimeLow();
      mvTick(msnTri, mcPlay ? 0.f : dt);
      unsigned p4b_ = sceKernelGetSystemTimeLow();   // пункт 8: миссия по настоящему положению (как tools/bots/world.c); правка 40: под камерой старта часы миссии стоят
      // Правка Роба 249 (ОСТАТОК 09.10 п. 18): машины миссии с ИИ (соперники, цели, полиция) — тем же правилом следов, что игрок, по своему
      // вводу кадра (McCar.lastIn); слоты skid.c 1..SKID_CARS-1 по порядку машин. SKID_AI_OLD=1 — прежнее (следы только у игрока)
#ifndef SKID_AI_OLD
      if (haveCol) { const MsnCars *mc = mvCars(); int sl = 1;
        if (mc) for (int i = 0; i < mc->n && sl < SKID_CARS; i++) if (mc->c[i].on && mc->c[i].mode != MC_PARKED) skidCar(sl++, &mc->c[i].car, &mc->c[i].lastIn, &world);
        for (; sl < SKID_CARS; sl++) skidCar(sl, 0, 0, 0); }
#endif
      unsigned p4c_ = sceKernelGetSystemTimeLow();
      { char sd[256]; levelFile(sd, "");   // пункт 6: экран покупки у продавца — ирис на входе (над городом) и выходе (над погасшей сценой)
        int sev = shopViewTick(mvShop(), sd, wallDt);
        if (sev) { frames += transitClose(&fbp0, "shop") / (longView ? 2 : 1); chase.init = 0; smkSettle();
                   if (sev == SHOPV_EV_OPEN) { sndMusic(SND_MUS_STORE); sndStore(1); }   /* сверка g08 круг 3: лавка машин — «магазин» */   // правка 235: покупка у продавца (Барни, Plow King) — событие Store, Muzak
                   if (sev == SHOPV_EV_CLOSE) { sndStore(0); shopViewEnd(mvShop()); if (sndMusicOwned()) mvMusicResume(); else sndMusic(SND_MUS_OFF); } } }
#ifndef STORE_MUS_RESUME_OLD
#define STORE_MUS_RESUME_OLD 0   // правило №235 для всех экранов Store (Роб 17.10: «в магазинах музыка как в будке»): вышел из будки/гардероба — музыка миссии снова (если
                                 // её ведёт миссия), иначе тишина до следующего события, как после лавки; было — всегда тишина. 1 — прежнее
#endif
#define STORE_MUS_LEAVE() do { if (!STORE_MUS_RESUME_OLD && sndMusicOwned()) mvMusicResume(); else sndMusic(SND_MUS_OFF); } while (0)
      if (wardrobeTick(wallDt) == WARDV_EV_CLOSE) { int k = wardrobeEnd(); STORE_MUS_LEAVE(); if (k >= 0 && k != skinWorn) homerWear(k); smkSettle(); }   // правка 107
      if (phoneViewTick(mvShop(), wallDt) == PHONEV_EV_CLOSE) { boothRelease1051();   // №1051: будка закрыта — стрим снова грузит город
        // правка 94: экран будки погас — выбранная машина к будке
        const char *pc = phoneViewEnd(mvShop()); STORE_MUS_LEAVE(); if (pc) phoneSummonCar(phoneK, pc); chase.init = 0; mem1060Return(&fbp0); smkSettle(); }   // №1060: освобождённое — обратно под газетой
      unsigned p4d_ = sceKernelGetSystemTimeLow();
      collFrame(dt, &fbp0);        // В5: события карточек, ос и гэгов -> звук, частицы, HUD, ролик гэга
      unsigned p4e_ = sceKernelGetSystemTimeLow();
      { const char *fm = mvFmvPending(); if (fm) { msnMovieWorld = 1; msnMovie(fm, &fbp0); msnMovieWorld = 0; mvFmvDone();   /* сверка g11b-collect: ролик цели в игре — затемнение мира до и после (P15-04) */ chase.init = 0; smkSettle();   // цель «fmv» (SetFMVInfo)
          padE.init = 0; prev = smokePad(); } }   // START пропуска ролика ещё зажат — не пауза: пульт с чистого листа
#ifndef BRIEF_IRIS_SKIP_OLD
#define BRIEF_IRIS_SKIP_OLD 0   // 1 — после Continue ирис старта миссии доигрывает паузу и раскрытие под уезжающей карточкой (TEST18)
#endif
      { static int exWas; int exNow = uiBriefExitBusy();   // №625: начало выезда карточки (Continue) — ирис, закрытый под брифингом, снимается
        if (exNow && !exWas && !BRIEF_IRIS_SKIP_OLD && transitSkipMsn() && !t03Old) msIrisSkipWin = T03_IRISWIN_FRAMES;   // Будка-93 (1228): ирис снят, второго не будет
        exWas = exNow; }
#ifndef BRIEF_EXIT_HOLD_OLD
#define BRIEF_EXIT_HOLD_OLD 0   // 1 — ирис переноса старта закрывается сразу после Continue, карточка не успевает уехать (TEST17)
#endif
      // №612 (видео TEST17): карточка миссии уезжает с рукой ДО ириса старта — телепорт ждёт конца выезда (≤ 0,45 с), событие остаётся в очереди
      { float tp[3]; int tele = (BRIEF_EXIT_HOLD_OLD || !uiBriefExitBusy()) && mvTeleported(tp);
        if (msLateWin > 0) msLateWin--;
        if (msIrisSkipWin > 0) msIrisSkipWin--;
        if (t03DoubleArm && transitBusy()) { t03DoubleArm = 0; if (transitSkipMsn() && !t03Old) msIrisSkipWin = T03_IRISWIN_FRAMES; msNewsPend = MS_NEWS_WAIT + 1; }
        if (!tele && msNewsPend && !uiBriefExitBusy() && (msFlowOld || !mvMsnSetup() || ++msSetupWait > MS_SETUP_CAP) && --msNewsPend <= 1) {   // №893: миссия MISSION SELECT перенос не дала — газета на месте игрока (№1072: не раньше конца подготовки фазы)
          if (!msFlowOld) msLateWin = MS_LATE_WIN;
          int drv_ = !haveHomer || homerDriving(&homer);
          tp[0] = drv_ ? car.x : homer.x; tp[1] = drv_ ? car.y : homer.y; tp[2] = drv_ ? car.z : homer.z; tele = 1; msNewsPend = 1;
          if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "MSNEWS fallback at %.1f %.1f t=%.1f\n", tp[0], tp[2], smkT); smkWrite(s_); } }
        if (tele) {   // сценарий переставил игрока: камера заново; далеко (или выбор MISSION SELECT, №893) — город с экрана загрузки
          int always = msNewsPend > 0 || (!msFlowOld && msLateWin > 0); msNewsPend = 0; msLateWin = 0;   // №1072: поздний перенос того же выбора — тоже с загрузкой
          if (!t03Old && msIrisSkipWin > 0) { msIrisSkipWin = 0; always = 0; chase.init = 0; smkSettle(); if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "MSNEWS2 1228 second tele without iris t=%.1f\n", smkT); smkWrite(s_); } }   // Будка-93 (1228): ирис уже снят по Continue, второй раз не закрываем
          else { frames += transitClose(&fbp0, "msn") / (longView ? 2 : 1); chase.init = 0; smkSettle(); }   // ирис закрывается, город читается, ирис раскрывается
          float dx = tp[0] - eyeX, dz = tp[2] - eyeZ;
          float rc_[3] = {tp[0], tp[1], tp[2]}; int rk_ = -1, toRoom_ = !TELEDOOR1209_OLD && !teleDoor1209Old && roomDoorOf(tp, rc_, &rk_);   // ag/loadorder: точка переноса в комнате — город читается у её двери, а не у кармана комнат
          if (always || dx * dx + dz * dz > 80.f * 80.f) { msnTeleNews1239 = always; msnTeleport(rc_[0], rc_[1], rc_[2], &fbp0); msnTeleNews1239 = 1; }
          else if (toRoom_ && !(rs.room == rk_ && rg.mem)) roomRing1209(rc_, "tele-door");   // из комнаты в другую комнату рядом: экрана нет, кольцо у двери под закрытым ирисом
          if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "MSNEWS always=%d far=%d t=%.1f\n", always, dx * dx + dz * dz > 80.f * 80.f, smkT); smkWrite(s_); }
          if (msMusPend) { msMusPend = 0;   // №1070: после экрана загрузки — звук старта (пешком) или езда; миссия с музыкой — её начало заново
            int foot_ = haveHomer && onFoot();
            if (sndMusicOwned()) mvMusicResume(); else if (foot_) { sndMusic(SND_MUS_OFF); feStinger("level_intro"); } else sndMusic(SND_MUS_FREEROAM);
            if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "MSFLOW1070 music owned=%d foot=%d t=%.1f\n", sndMusicOwned(), foot_, smkT); smkWrite(s_); } }
          msnRoomSync(); } }   // фаза в комнате (SetDynaLoadData «…@») — вход в неё; в городе — выход
      if (camHold1225 > 0 && !phoneViewOn()) camHold1225--;   // Будка-93 (1225): отсчёт окна без наезда — только вне будки
      { const char *ph, *cn; if (mvStartCam(&ph, &cn)) msncamStartPhase(ph, cn); }   // правка 40: фаза с камерой старта, игрок поставлен — «наезд»
      if (haveTraffic && !inRoom()) {   // этап 5: после машины игрока и Гомера (в комнате город стоит); камера прошлого кадра — спавн и удаление вне кадра
        int drv = !haveHomer || homerDriving(&homer);
        float pp[3] = {drv ? car.x : homer.x, drv ? car.y : homer.y, drv ? car.z : homer.z}, e3[3] = {eyeX, eyeY, eyeZ}, f3[3] = {cfX, cfY, cfZ};
        smkMark(4); BBP(BBP_TRAFFIC); SMK5_START();
        if (smkOn) { static float acc_[6]; static unsigned n_; static float t_; unsigned now_ = sceKernelGetSystemTimeLow();   // №P4: разбивка ph4 (логика миссии) по подфазам, мс на кадр за секунду
          acc_[0] += (p4b_ - p4a_) / 1000.f; acc_[1] += (p4c_ - p4b_) / 1000.f; acc_[2] += (p4d_ - p4c_) / 1000.f; acc_[3] += (p4e_ - p4d_) / 1000.f; acc_[4] += (now_ - p4e_) / 1000.f; n_++;
          if ((t_ += dt) >= 1.f && n_) { char s_[200]; snprintf(s_, sizeof s_, "PH4SUB t=%.1f n=%u mvtick=%.2f skid=%.2f shop=%.2f coll=%.2f rest=%.2f | tick: npc=%.2f frac=%.2f cars=%.2f stage=%.2f\n", smkT, n_, acc_[0] / n_, acc_[1] / n_, acc_[2] / n_, acc_[3] / n_, acc_[4] / n_, gameMsnPartMs[0] / n_, gameMsnPartMs[1] / n_, gameMsnPartMs[2] / n_, gameMsnPartMs[3] / n_); { char s2_[120]; snprintf(s2_, sizeof s2_, "PH4CARS t=%.1f physics=%.2f drive=%.2f cars_per_frame=%.1f\n", smkT, mcPartMs[0] / n_, mcPartMs[1] / n_, mcPartMs[2] / n_); smkWrite(s2_); memset(mcPartMs, 0, sizeof mcPartMs); } memset(gameMsnPartMs, 0, sizeof gameMsnPartMs); smkWrite(s_); memset(acc_, 0, sizeof acc_); n_ = 0; t_ = 0.f; } }
        int trWas[TR_MAXCAR]; for (int i = 0; i < TR_MAXCAR; i++) trWas[i] = traffic.car[i].mode;
        // правка 355: оставленная своя машина (oldCar) — не «призрак»: водители трафика тормозят перед ней и упираются в неё
        // (она тяжёлая и стоит: сдвигается только машина трафика). CAR_HUSK_OLD=1 — прежнее
        if (!CAR_HUSK_OLD && oldCarOn && traffic.nextra < TR_MAXEXTRA) traffic.extra[traffic.nextra++] = &oldCar;
        trfProfOn = smkOn; TRFSUB_T(0); trafficUpdate(&traffic, &world, &car, drv, pp, e3, f3, dt);
        if (!CAR_HUSK_OLD && oldCarOn) trafficHitStill(&traffic, &world, &oldCar);
        if (!CAR_HUSK_OLD && !OLDCARHIT1241_OLD && !oldCarHit1241Old && oldCarOn && haveCol) trafficHitPlayerStill(&world, drv ? &car : NULL, &oldCar, &oldCarAwake, &oldCarStill, dt);   // правка 1241: игрок в чужой машине таранит оставленную свою, она сдвигается и получает урон
        if (smkOn && oldCarOn && drv) { float d_ = hypotf(car.x - oldCar.x, car.z - oldCar.z); static unsigned n_; if (d_ < 10.f && !(n_++ % 20)) { char s_[96]; snprintf(s_, sizeof s_, "OLDCARDIST d=%.2f v=%.1f old=%d moved=%.2f dmg=%.2f t=%.1f\n", d_, car.v, oldCarHit1241Old, hypotf(oldCar.x - oldCarX0, oldCar.z - oldCarZ0), oldCar.damage, smkT); smkWrite(s_); } }   // для гейта 1241
        if (!CAR_HUSK_OLD && oldCarOn && traffic.nextra > 0 && traffic.extra[traffic.nextra - 1] == &oldCar) traffic.nextra--;
        if (trLog && smkOn) for (int i = 0; i < TR_MAXCAR; i++) if (trWas[i] == TRM_OFF && traffic.car[i].mode != TRM_OFF) trSpawnPend |= 1u << i;   // правка 328
        TRFSUB_T(1);
        if (haveProps) { Car *pc_[12]; int np_ = trafficCarsNear(&traffic, pp[0], pp[2], 45.f, pc_, 12);   // сверка g02-traffic круг 2 (g11 P11-02): предметы ломает и двигает любая машина, не только игрока
          for (int i_ = 0; i_ < np_; i_++) propsHitCar(&props, &world, pc_[i_], NULL); }
        TrEvent te; while (trafficPopEvent(&traffic, &te)) {   // Hit & Run: нарушения трафика (game.c gameHnrOffense)
          // правки 180/184 (близнец Б2): голоса улицы — сбитый / чуть не сбитый / ударенный прохожий (голос по модели) и водитель
          // трафика после удара; реплика прохожего раньше реплики Гомера (поток реплик один — Гомер не говорит поверх)
          if (te.type == TR_EV_CAR_WAY) {   // сверка g02-traffic круг 2 (g08 P09:3): «пробка» — игрок загородил машину трафика: говорит герой (если за рулём) или водитель трафика, пополам
            if (!onFoot() && (te.index & 1)) sndSay(SND_SAY_CARWAY, 0); else sndDriverSayAt(SND_DRV_WAY, te.index, te.x, te.y, te.z);
            continue; }
          if (te.type == TR_EV_CAR_LAND) { sndPlay("suspension_ronks", te.x, te.y, te.z, 1.f); continue; }   // сверка g02-traffic круг 2 (g08 P09:30): подвеска машины трафика после полёта
          if (te.type == TR_EV_CAR_HONK) {   // №479: гудок машины трафика: клип её модели («<модель>_horn» в пакете звуков: minivanA_horn, schoolbus_horn…), иначе общий «horn»
            if (!HORN_OLD && !hornOld) { char hn[40] = "horn"; int mi = te.index >= 0 && te.index < traffic.maxCars ? traffic.car[te.index].model : -1;
#if !HORN_STOP_OLD
              if (!hornStopOld && te.index >= 0 && te.index < TR_MAXCAR) {
                trafficHornStart(te.index, hn, te.x, te.y, te.z);   // сверка g11b-collect P10-4: шанс 0,5, серия 1–3 гудков, дальность 30–60 м (HORN_SERIES_OLD=1 — один гудок 1,4 с, №561)
              } else
#endif
              sndPlay(hn, te.x, te.y, te.z, 1.f); }
            continue; }   // №479: гудок машины трафика после слабого тарана, не нарушение
          int pv = sndPedVoice(te.who);   // правка 240: имя в событии — прохожий трафика или персонаж миссии (game_msn.c npcKnock)
          int said = -1;
          if (te.type == TR_EV_PED_NEAR) {   // не нарушение: шкалу Hit & Run не двигает
            said = sndPedNamedSayAt(SND_PED_NEAR,te.who,te.x,te.y,te.z); if (!said) sndSay(SND_SAY_MISSP, 0);
            streetSourceBind(&te,said);
            if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "STREETV ev=near ped=%d voice=%d said=%d t=%.1f\n", te.index, pv, said, smkT); smkWrite(s_); }
            continue; }
          if (te.type == TR_EV_PED_HIT) { said = sndPedNamedSayAt(SND_PED_HIT,te.who,te.x,te.y,te.z); sndPedHit(te.x, te.y, te.z); }   // правка 135.6: удар по телу + реплика HitP
          if (te.type == TR_EV_PED_KICK) said = sndPedNamedSayAt(SND_PED_KICK,te.who,te.x,te.y,te.z);
          if (te.type == TR_EV_CAR_HIT) said = sndDriverSayAt(SND_DRV_WAY,te.index,te.x,te.y,te.z);   // машина трафика (у стоящих водителя нет: TR_EV_PARK_HIT); голос — по номеру машины, правило порта
          streetSourceBind(&te,said);
          if (smkOn && said >= 0) { char s_[96]; snprintf(s_, sizeof s_, "STREETV ev=%s idx=%d voice=%d said=%d t=%.1f\n", te.type == TR_EV_PED_HIT ? "hit" : te.type == TR_EV_PED_KICK ? "kick" : "car",
                                                     te.index, te.type == TR_EV_CAR_HIT ? te.index & 3 : pv, said, smkT); smkWrite(s_); }
          else if (te.type == TR_EV_CAR_WRECK) {   // правка 135.6: взрыв разбитой машины трафика; правка 176: и видимый — как у машины игрока
            carExplosionAt(te.x, te.y, te.z); sndTrafficWreck(te.x, te.y, te.z);
#ifndef DCAR_FOOT_OLD
#define DCAR_FOOT_OLD 0   // сверка g10b-loop, P09:Д11: 1 — реплика Dcar только на взрыв своей машины (прежнее)
#endif
            if (!DCAR_FOOT_OLD && onFoot()) sndSay(SND_SAY_DCAR, 0);   // P09:Д11: пешком Dcar звучит на взрыв любой машины (за рулём — только своей)
#ifndef SV_BOOM_TRAFFIC_OLD
#define SV_BOOM_TRAFFIC_OLD 0   // сверка g10b-loop, P03-01: 1 — взрыв машины трафика не пишется в общее событие svBoom (монеты, толчок предметам и соседям не получат сигнал)
#endif
            // сверка g10b-loop, P03-01 / P01-14: взрыв разбитой машины трафика — общее событие (viewer/sverka_shared.h) для монет (g11b: 10 монет из точки
            // машины), толчка предметам и соседним машинам (радиус 20 м, сила 20000) и звука. Читатель реагирует, когда seq вырос. Шкала +25 здесь
            // уже начисляется (gameHnrOffense ниже): читателю шкалы kind 1 не считать второй раз. SV_BOOM_TRAFFIC_OLD=1 — прежнее
            if (!SV_BOOM_TRAFFIC_OLD) { svBoom.x = te.x; svBoom.y = te.y; svBoom.z = te.z; svBoom.radius = 20.f; svBoom.force = 20000.f; svBoom.byPlayer = 1; svBoom.kind = 1; svBoom.seq++; } }
          gameHnrOffense(&G, te.type == TR_EV_CAR_WRECK ? HNR_CAR_WRECK : te.type == TR_EV_PED_HIT ? HNR_PED_HIT : te.type == TR_EV_PED_KICK ? HNR_PED_KICK : HNR_CAR_HIT); }
        TRFSUB_T(2);
        if (!onFoot()) gameHnrVehicle(&G, mvForcedCar());   // правка 121: сел в другую машину (будка, угон, машина миссии) — шкала в ноль
        TRFSUB_T(3); hnrPursuit(dt);   // правка 64/72: полная шкала — погоня полиции
        TRFSUB_T(4);
        { int dk_ = deathCheck(&fbp0); if (dk_) { frames += dk_ / (longView ? 2 : 1); smkSettle(); } }
        TRFSUB_T(5);
        { int bk_ = bustedCheck(&fbp0, dt); if (bk_) { frames += bk_ / (longView ? 2 : 1); smkSettle(); } }   // №627: затемнение после ареста   // правка 86/349: зоны DEATH — затемнение и возврат в точку оригинала
        if (trPedMin >= 0.f && traffic.ped[0].test) { float d = hypotf(homer.x - traffic.ped[0].x, homer.z - traffic.ped[0].z); if (d < trPedMin) trPedMin = d; }
      }
      TRFSUB_T(6); TRFSUB_REPORT(dt);
      SMK5(0);   // ph5: трафик и события
#ifndef COIN_FLASH_DRAW_OLD
#define COIN_FLASH_DRAW_OLD 0   // сверка g10b-loop круг 2 (g11b P03-03): 1 — вспышки исчезновения монет не рисуются (прежнее)
#endif
      // круг 2 (g11b P03-03): вспышка в точке исчезнувшей монеты (Coins.flash, живёт 0,5 с): пока собственного спрайта вспышки нет, на каждую новую вспышку — короткий всплеск искр
      // (размер частиц эффекта искр; 0,25 м оригинала отдельным спрайтом не рисуется). COIN_FLASH_DRAW_OLD=1 — без вспышек
      { static unsigned nfPrev_; if (!COIN_FLASH_DRAW_OLD && haveCoins) { for (unsigned k_ = nfPrev_; k_ < coins.nflash; k_++) for (int j_ = 0; j_ < 4; j_++)
            fxSpawn(FX_SPARK, coins.flash[k_].x, coins.flash[k_].y, coins.flash[k_].z, (rand() * (1.f / RAND_MAX) - .5f) * 2.f, rand() * (2.f / RAND_MAX) + .5f, (rand() * (1.f / RAND_MAX) - .5f) * 2.f);
          nfPrev_ = coins.nflash; } }
      { // эффекты урона: искры при ударе, дым по степени урона, огонь, взрыв
        float sh = sinf(car.h), chh = cosf(car.h), hx = car.x + sh * 1.7f, hz = car.z + chh * 1.7f, hy = car.ry + 1.0f;
        { float sl[3]; if (haveCarVis && carVisJointWorld(&cv, &car, cv.slJ, sl)) { hx = sl[0]; hy = sl[1]; hz = sl[2]; } }   // CAR1: дым из локатора sl с наклоном кузова
        #define FR() (rand() * (1.f / RAND_MAX))
        // сверка g11 P01-12 (viewer/fx.c fxCrashStars и др.): частицы удара от импульса и вида контакта (звёзды об забор, искры и крошка при ударе машины о машину), об неподвижный мир
        // (здания, стены) частиц нет; не чаще раза в 0,15 с на вид удара. CRASHFX_SV_OLD=1 — прежние искры от прироста урона (ниже)
        if (fxCrashFxSv) {
          static float crT_[2]; crT_[0] -= dt; crT_[1] -= dt;
          float mass_ = car.p.mass;
          if (car.wallV > 0.f && car.wallType == COL_SEG && crT_[0] <= 0.f && fxCrashStars(car.wallP[0], car.wallP[1] + .5f, car.wallP[2], mass_ * car.wallV)) crT_[0] = .15f;
          if (car.carHitV > 0.f && crT_[1] <= 0.f) {
            float vl_ = hypotf(car.velPre[0], car.velPre[2]), dx_ = vl_ > .1f ? car.velPre[0] / vl_ : 0.f, dz_ = vl_ > .1f ? car.velPre[2] / vl_ : 0.f, imp_ = mass_ * car.carHitV;
            if (fxCrashSparks(car.carHitP[0], car.carHitP[1], car.carHitP[2], dx_, dz_, imp_, 0)) { fxCrashPaint(car.carHitP[0], car.carHitP[1], car.carHitP[2], imp_); crT_[1] = .15f; }
          }
          // сверка g11 круг 2 (P11-14 от g01-cars): удар о листву и статику — частица ATC в точке касания, каждый удар (не чаще раза в 0,3 с)
          static float folT_; folT_ -= dt;
          if (car.foliV > 0.f && folT_ <= 0.f && foliageFxOk(car.p.mass, car.foliV)) { float fp_[3] = {hx, car.ry + .8f, hz}; if (fxFoliageHit(fp_, car.foliV)) folT_ = .3f; }   // 0,3 с между вспышками листьев — число наше, не из оригинала
        } else
        if (car.damage > prevDmg + 1e-4f) {
          int n = 6 + (int)((car.damage - prevDmg) * 300.f); if (n > 30) n = 30;
          float sx = hx, sz = hz;
          if (car.v < 0) { sx = car.x - sh * 1.9f; sz = car.z - chh * 1.9f; }
          for (int k = 0; k < n; k++)
            fxSpawn(FX_SPARK, sx, car.ry + 0.6f, sz, (FR() - .5f) * 8.f + car.vel[0] * .3f, FR() * 5.f + 1.f, (FR() - .5f) * 8.f + car.vel[2] * .3f);
        }
        prevDmg = car.damage;
        // сверка g11 круг 2 (P01-23 от g01-cars, P18-04 от g12-data): пробуксовка (Car.spinDrive) — от задних колёс дым по поверхности под колесом (Car.wheelSurf); ручник дыма не даёт. 20 выпусков в секунду на колесо
        // №1082 (Роб на PSP: слишком много дыма «горящей машины» при резком газе): две постоянные системы у задних колёс по поверхности под колесом (fxFindInst: дорога, дерево, металл 9 — лёгкая пыль dashShape;
        // трава 11; грунт, песок, гравий 10; вода 12, в данных нет — ничего); рывок со старта 2,5 с и больше (Car.burnBig) — обе на 19 (FireSpray). Сила выпуска — Car.burnLevel, 0 — клубы доживают сами.
        // WHEELSMOKE1082_OLD=1 — прежние выпуски fxWheelSpin 20 в секунду
#ifndef WHEELSMOKE1082_OLD
#define WHEELSMOKE1082_OLD 0
#endif
        if (!WHEELSMOKE1082_OLD) {
          static int hWs_[2], tWs_[2];
          int drv_ = !onFoot() && car.wheels >= 2 && !car.lastHb && car.burnLevel > 0.f;
          for (int sd_ = 0; sd_ < 2; sd_++) {
            float sg_ = sd_ ? 1.f : -1.f, wx_ = car.x - sh * fabsf(car.p.wheelR) + chh * fabsf(car.p.wheelX) * sg_, wz_ = car.z - chh * fabsf(car.p.wheelR) - sh * fabsf(car.p.wheelX) * sg_;
            int sf_ = car.wheelSurf[sd_ ? 3 : 2], ty_ = car.burnBig ? 19 : sf_ == SURF_GRASS ? 11 : (sf_ == SURF_DIRT || sf_ == SURF_SAND || sf_ == SURF_GRAVEL) ? 10 : sf_ == SURF_WATER ? 12 : 9;
            float p_[3] = {wx_, car.ry + .05f, wz_}, d_[3] = {sh, 0.f, chh};
            if (hWs_[sd_] && (!fxAlive(hWs_[sd_]) || tWs_[sd_] != ty_)) { if (fxAlive(hWs_[sd_])) fxStop(hWs_[sd_]); hWs_[sd_] = 0; }
            if (!hWs_[sd_] && drv_) { int id_ = fxFindInst(ty_); if (id_ >= 0) { hWs_[sd_] = fxStartSystem(id_, p_, d_); tWs_[sd_] = ty_; } }
            if (hWs_[sd_]) { fxMove(hWs_[sd_], p_, d_); fxSetRate(hWs_[sd_], drv_ ? fminf(1.f, car.burnLevel) : 0.f); }
          }
        } else
        if (!onFoot() && car.spinDrive && !car.lastHb && car.wheels >= 2) {
          static float wsT_; wsT_ -= dt;
          if (wsT_ <= 0.f) {
            wsT_ = .05f;
            for (int sd_ = -1; sd_ <= 1; sd_ += 2) {
              float wx_ = car.x - sh * fabsf(car.p.wheelR) + chh * fabsf(car.p.wheelX) * sd_, wz_ = car.z - chh * fabsf(car.p.wheelR) - sh * fabsf(car.p.wheelX) * sd_;
              fxWheelSpin(wx_, car.ry, wz_, car.wheelSurf[sd_ < 0 ? 2 : 3], car.vel[0], car.vel[2], 1.f);
            }
          }
        }
        { // дым/огонь: родная система car_damage3 (inst 14), одна на машину; стадии — множители эмиттеров
          static int hDmg, hHusk; static float burnT;
          float sp[3] = {hx, hy, hz};
          // №697 (Роб 17.10): после взрыва остаётся чёрный сгоревший остов, дальше не горит; огонь и дым гаснут через HUSK_BURN_SEC после взрыва.
          // HUSK_BURN_OLD=1 — прежнее (горит постоянно, красный контроль)
          burnT = carBoomed(&car) ? burnT + dt : 0.f;
          int burnOut = !huskBurnOld && burnT > HUSK_BURN_SEC;
#ifndef SMOKE_D_OLD
#define SMOKE_D_OLD 0   // сверка g10b-loop, P01-11: 1 — дым с 33 % урона (0,4), полный с 60 % (прежнее)
#endif
          // сверка g10b-loop, P01-11: дым двигателя с 60 % урона, интенсивность растёт плавно (доля прочности: (0,4 − прочность) × 2,5, то есть 0 при 60 %
          // урона и 1,0 при полном), с 98 % — полный средний дым; тяжёлый и огонь (разбита) — как было, по записи PS2 a355_boom (№383). SMOKE_D_OLD=1 — прежнее
          int stage = g10bSmokeStage(car.damage, burnOut, carBoomed(&car), car.destroyed);
          float smkRate_ = g10bSmokeRate(car.damage, stage);   // №383: до смертельного удара дым, затем огонь до взрыва (PS2 a355_boom)
          if (stage) {
            if (!fxAlive(hDmg)) hDmg = fxStartSystem(fxFindInst(14), sp, NULL);
            fxMove(hDmg, sp, NULL);
            fxSetEmitRate(hDmg, 0, smkRate_);   // darksmoke
            fxSetEmitRate(hDmg, 1, stage >= 3 ? 1.f : 0.f);   // fireSeqEm1
            fxSetEmitRate(hDmg, 2, stage >= 3 ? 1.f : 0.f);   // fireSeqEm2
          } else if (hDmg) { fxStop(hDmg); hDmg = 0; }
          if (carBoomed(&car) && !burnOut) {   // горящий остов: FireSpray (inst 19) — со взрыва (правка 199)
            float gp[3] = {car.x, car.ry + .3f, car.z};
            if (!fxAlive(hHusk)) hHusk = fxStartSystem(fxFindInst(19), gp, NULL);
            fxMove(hHusk, gp, NULL);
          } else if (hHusk) { fxStop(hHusk); hHusk = 0; }
        }
        static int wasBoomed; int boomNow = carBoomed(&car) && !wasBoomed; wasBoomed = carBoomed(&car);
        if (boomNow) {   // правка 199: взрыв через CAR_BOOM_DELAY после разбития (оригинал: горит, Гомер вылетел, потом взрыв)
          carExplosionAt(car.x, car.ry, car.z);   // №391: BurstShape, кольцо и пятно привязаны к поверхности
          sndCarExplode(car.x, car.ry, car.z);
          if (smkOn) { char s[64]; snprintf(s, sizeof s, "CARBOOM t=%.2f dead=%.2f\n", smkT, car.deadT); smkWrite(s); }
          // подсказка уже была: строка — со взрывом (PS2: ~2,6 с после удара)
          if (!tutPending(TUT_DESTROYED) && uiReady && world.nwrench) uiMessage("VEHICLE DESTROYED!\nFIND A WRENCH,\nOR GO TO A PHONEBOOTH,\nTO REPAIR YOUR VEHICLE.", -1, 6.f);   // библия, хеш 2198872 (пункт 4); подсказка уже была или выключена
        }
        if (smkOn && trOn && (boomNow || (car.destroyed && !wasDestroyed))) { trLeft = TR_FRAMES; trDetail = 0; }   // правка 252: разбитие и взрыв — 3 кадра по фазам в trace.txt
        if (car.destroyed && !wasDestroyed && tutPending(TUT_DESTROYED)) tutEventIn(TUT_DESTROYED, TUT_BOOM_S);   // разбита: подсказка Барта (PS2 33:36)
        transitEjectWatch(&homer, &car, haveHomer, car.destroyed && !wasDestroyed, dt);   // BOOM / EJECT в smoke.txt
        wasDestroyed = car.destroyed;
      }
      SMK5(1);   // ph5: эффекты урона
      x = car.x; y = car.ry; z = car.z; h = car.h; v = car.v;
      { float cp[3], lk[3], ccp[3], clk[3];
        // правка 167 (близнец В23, ps2-car-exit-camera.yaml): выход — камера машины держится, пока Гомер не вышел (HS_GET_OUT),
        // потом плавный облёт к Гомеру ~0,55 с; посадка — когда сел, плавно за машину ~1 с. Было: камера пешком с начала выхода
        // со старым курсом и сброс в конце, при посадке — склейка в кадр
        if (!CAMSET1151_OLD) {   // №1151: настройки камеры игрока — плавно (под паузой, dt = 0, и при смене в меню — сразу); пешком читает высоту и дальность через homerCamUserH/D
          int snap_ = camCfgSnap996 || dt <= 0.f;
          camUserTiltNow = camUserEase(camUserTiltNow, camUserTiltWant(camUserCarTilt), CAMU_TILT_RATE, dt, snap_); camUserFovNow = camUserEase(camUserFovNow, camUserFovWant(camUserCarFov), CAMU_FOV_RATE, dt, snap_);
          homerCamUserH = camUserH(camUserFootH); homerCamUserD = camUserD(camUserFootD); }
        if (camCfgSnap996) { camCfgSnap996 = 0; homer.camPullD = 0.f; homer.camRise = 0.f; chase.wallD = 0.f; chase.rise = 0.f; csOfsOn = csPrevOn = 0; }   // №996: смена Height / Distance — камера сразу на новом месте (в паузе dt = 0, плавное отпускание стоит)
        int carCam = !haveHomer || homer.state == HS_IN_CAR || homer.state == HS_GET_OUT;
        if (carCam) { CSUBT0; chaseCamUpdate(&car, camAng, dt, ccp, clk); CSUB(0); memcpy(cp, ccp, 12); memcpy(lk, clk, 12);   // камера как в оригинале (см. chaseCamUpdate)
          static float ccT; if (smkOn && carCamLog && (ccT += dt) >= 1.f) { ccT = 0.f; char s_[400];   // гейт tests/camera/carcam_ps2.py (правка 198+): кадр и кузов раз в секунду
            snprintf(s_, sizeof s_, "CARCAM t=%.1f car=%s ps2=%d eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f fovy=%.2f aspect=%.4f pos=%.3f,%.3f,%.3f h=%.4f lift=%.3f "
                     "box=%.3f,%.3f,%.3f,%.3f,%.3f v=%.2f back=%.3f eyeh=%.3f need=%.3f wall=%.3f\n", smkT, chaseInfo.name ? chaseInfo.name : "-", chaseInfo.ps2,
                     ccp[0], ccp[1], ccp[2], clk[0], clk[1], clk[2], curFovY, ASPECT, car.x, car.y, car.z, car.h, cv.lift, car.p.halfW, car.p.halfL, car.p.bodyOfs,
                     car.p.bodyLo, car.p.bodyHi, car.v, chaseInfo.back, chaseInfo.eye, chaseInfo.need, chaseInfo.wall); smkWrite(s_); } }
#ifndef REARVIEW_OLD
#define REARVIEW_OLD 0   // сверка g10b-loop, СВЕРКА_КАМЕРА 1.12: 1 — вида назад по кнопке нет (прежнее)
#endif
        // сверка g10b-loop, СВЕРКА_КАМЕРА 1.12 (кнопка по слову Роба №1078: стрелка вниз, L и R остаются газом и тормозом): пока она зажата, камера
        // машины срезом на заднем виде: штанга (от машины до глаза) отражается, взгляд на машину; у бампера глаз у заднего края кузова и смотрит назад.
        // Отпустил — срез обратно (камера за машиной всё это время считалась как раньше). REARVIEW_OLD=1 — прежнее
        rearViewOn = 0;
        { static int rlSt_; static float rlDly_;   // круг 7 (сверено с оригиналом): вид назад переключается только пока машина устойчива (не в воздухе, не набок, 0,2 с после); у бампера — по кнопке сразу
          if (carCam && !fly && !mcPlay && !onFoot() && (!haveHomer || homer.state == HS_IN_CAR)) {
            int un_ = g10bCamUnstable(car.comp, car.q, &rlDly_, dt), held_ = g10bRearLook(&rlSt_, (pad.Buttons & PSP_CTRL_DOWN) != 0, chaseView == 2, un_);
            rearViewOn = g10bRearCam(held_, chaseView == 2, car.x, car.z, car.h, car.p.wheelR, ccp, clk, cp, lk);
          } else { rlSt_ = 0; rlDly_ = 0.f; } }
        roomCamOn = 0; zoneCamOn = 0; carZoneOn = 0; walkFovK = 1.f;
#ifndef CAR_ZONE_CAM_OLD
        // №408: машина (Гомер за рулём) в объёме камеры зоны машины — снимает она (срезом), камера за машиной считает дальше, чтобы
        // на выходе вернуться срезом в своё место (запись PS2). Кузов — коробка carBox: центр и 8 углов (днище/крыша bodyLo/bodyHi)
        if (carCam && !onFoot() && (!haveHomer || homer.state == HS_IN_CAR) && nczcam && !carZoneOff && (CAMNAV_OPT_OLD || cfg.jumpCam) /* сверка g10-flow круг 5, P08:22: Jump Cameras Off — камеры зон у машины не работают */ && !inRoom() && !mcPlay && !fly) {
          CarBox cb; memset(&cb, 0, sizeof cb); carBox(&car, &cb);
          float lo = car.p.bodyLo + car.modelRaise, hi = car.p.bodyHi + car.modelRaise, bd[9][3], e3[3], l3[3], fy;
          bd[0][0] = cb.x; bd[0][1] = cb.y + .5f * (lo + hi); bd[0][2] = cb.z; int nb = 1;
#ifndef CARZONE_PTS_OLD
#define CARZONE_PTS_OLD 0   // сверка g10b-loop, P18-08: 1 — центр и все 8 углов кузова (прежнее)
#endif
          // сверка g10b-loop, P18-08: локатор для игрока в машине срабатывает, если внутри хоть одна из пяти точек: центр и четыре угла рамки — низ и верх по двум диагоналям
          // (нижние задние углы слева и справа, верхние передние слева и справа). Было: центр и все 8 углов (лишние: верх сзади, низ спереди). CARZONE_PTS_OLD=1 — прежнее
          for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) for (int u = 0; u < 2; u++) {
            if (!g10bCarZoneCorner(a, u)) continue;
            float ox = cb.fx * cb.hl * a + cb.fz * cb.hw * b, oz = cb.fz * cb.hl * a - cb.fx * cb.hw * b;
            bd[nb][0] = cb.x + ox; bd[nb][1] = cb.y + cb.gx * ox + cb.gz * oz + (u ? hi : lo); bd[nb][2] = cb.z + oz; nb++; }
          float ref[3] = {car.x, car.y + carModelLift(&car, cv.lift > 0.f ? cv.lift : 0.f), car.z}, sp = carZoneSpeed(car.vel);
          int prevC = czs.cur;
          // №456 (Роб: боковой ракурс прыжка держится дольше нужного, хочется раньше видеть, куда летишь): в воздухе после верхней точки
          // камера зоны уступает камере за машиной (примерно на секунду раньше приземления); возврат к зоне — после выхода из объёма.
          // CAR_ZONE_EARLY_OLD=1 — прежнее поведение
          static int czEarly;
          int zc = carZoneStep(czcam, nczcam, &czs, (const float (*)[3])bd, nb, ref, sp, dt, e3, l3, &fy);
#ifndef CAR_ZONE_EARLY_OLD
#define CAR_ZONE_EARLY_OLD 0
#endif
          if (zc < 0) czEarly = 0;
          else if (!CAR_ZONE_EARLY_OLD && !car.grounded && car.vel[1] < 0.f) czEarly = 1;
          // №694 (Роб, TEST27: «проезжал мимо магазина комиксов в машине, и камера начала крутиться»): камера andcam L3 (объём у магазина, касается зоны прыжка) включалась
          // на земле и мигала 2–4 раза за проезд (журнал CARZONEON, tests/smoke_scenes/zone_andcam_l3.txt). Камера зоны машины снимает, только когда машина в воздухе
          // не меньше 0,35 с внутри объёма (прыжок); на земле — камера за машиной. «jump» в имени (z1_housejump: удар в стену дома, запись PS2) — как было.
          // CAR_ZONE_GROUND_OLD=1 / flag carzonegroundold 1 — как было
          static float czAir;
          if (zc < 0) czAir = 0.f; else if (!car.grounded) czAir += dt;
          if (!carZoneGroundOld && zc >= 0 && czAir < .35f && !strstr(czcam[zc].name, "jump")) zc = -1;
          if (zc >= 0 && !czEarly) {
            memcpy(cp, e3, 12); memcpy(lk, l3, 12); carZoneFovY = lensData(fy); carZoneOn = 1; }
          { static int czPrevOn; if (smkOn && carZoneOn != czPrevOn) { char s_[120]; snprintf(s_, sizeof s_, "CARZONEON t=%.2f on=%d grounded=%d\n", smkT, carZoneOn, car.grounded); smkWrite(s_); } czPrevOn = carZoneOn; }
          if (smkOn && czs.cur != prevC) { char s_[200]; snprintf(s_, sizeof s_, "CARZONE t=%.2f cam=%s eye=%.2f,%.2f,%.2f car=%.2f,%.2f,%.2f v=%.1f\n", smkT,
                                             czs.cur >= 0 ? czcam[czs.cur].name : "-", cp[0], cp[1], cp[2], car.x, car.y, car.z, sp); smkWrite(s_); }
        } else czs.cur = -1;
#endif
        homer.camTurn = onFoot() && !fly ? ((pad.Buttons & PSP_CTRL_RTRIGGER) ? 1.f : 0.f) - ((pad.Buttons & PSP_CTRL_LTRIGGER) ? 1.f : 0.f) : 0.f;   // правка 75
        if (homerCamLR && camLR1231) homer.camTurn = homerCamLRx * fabsf(homerCamLRx) * 1.5f;   // №1231: L+R — аналоговый облёт (до ~2,4 рад/с при полном отклонении; одиночные L/R — 1,6)
        if (cfg.invertCam && !camTurnOld) homer.camTurn = -homer.camTurn;   // правка 362: Settings паузы → Invert Camera Control (по умолчанию On)
        // Правка Роба 361 (05.10: «вышли из машины, вылетели, машина взорвалась — камера преследует точку машины; надо показывать по
        // направлению движения героя»): правило 199 ниже выключено, камера пешком идёт за курсом Гомера (homer.c: после выброса — курс
        // полёта). «flag boomcamold 1» — прежнее (камера держит горящую машину в кадре)
#if !defined(BOOM_CAM_FREE) || !BOOM_CAM_FREE
        // правка 199 (правило порта): Гомера выбросило из горящей машины — камера пешком держит машину в кадре, пока та горит и
        // взрывается (до 2 с после взрыва), а не поворачивает за спину Гомера, улетевшего от машины: иначе взрыв за кадром.
        // У оригинала в записи (близнец boom.py) машина в кадре до взрыва и во время него
        if (boomCamOld && onFoot() && !inRoom() && car.destroyed && car.deadT < CAR_BOOM_DELAY + 2.f && hypotf(car.x - homer.x, car.z - homer.z) < 25.f) {
          float d = wrapPi(atan2f(car.x - homer.x, car.z - homer.z) - homer.camYaw);
          homer.camYaw = wrapPi(homer.camYaw + d * fminf(1.f, dt * 3.f));
        }
#endif
        if (onFoot()) { homerCamera(&homer, G.pHomerWorld ? G.pHomerWorld : &world, dt, cp, lk); chase.init = 0;
          if (smkOn && camLogOn) { char s_[200]; snprintf(s_, sizeof s_, "CAMLR1231 t=%.2f on=%d yaw=%.1f pitch=%.2f turn=%.2f sx=%.2f hx=%.2f hz=%.2f eye=%.2f,%.2f,%.2f\n", smkT, homerCamLR, homer.camYaw * 57.2958f, homerCamLRpitch, homer.camTurn, homerCamLRx, homer.x, homer.z, cp[0], cp[1], cp[2]); smkWrite(s_); }   // №1231: служебное
          { float hp_[3] = {homer.x, homer.y, homer.z}; walkFovK = walkCamSv(hp_, cp, lk, hypotf(homer.speed, homer.vy), dt); }   // сверка g06 (g06-camera:8,17,33,34): склон, автозум, крен уровня 7 (main_camera.h)   // этап 4: камера пешком (в комнате — её стены)
          if (inRoom() && nrcam) {   // 11.2: камера комнаты из данных — та, в чьём триггере Гомер (нет такой — камера пешком)
            float hp[3] = {homer.x, homer.y, homer.z}, e3[3], l3[3], fy;
            int rcr_ = roomCamStepFrom(rcam, nrcam, &rcs, hp, dt, e3, l3, &fy, cp, lk);   // сверка g06 (g06-camera:16,19,25,36): cp/lk — поза камеры ходьбы для перехода; -2 — плавный возврат к ней
            if (rcr_ == -2) { memcpy(cp, e3, 12); memcpy(lk, l3, 12); }
            if (rcr_ >= 0) {
              // №647 (Роб: «слишком видно потолок»): широкий кадр PS2 держит ГОРИЗОНТАЛЬНЫЙ угол данных камеры (запись ps2-camera-fov), вертикаль = 2atan(tan(h/2)·9/16), а не 4:3
              // (гостиная: 40° вместо 51°, по картине над диваном 4-20 % кадра у PS2 и 17-27 % у порта). CAM_ROOM_FOV_OLD=1 / flag roomfovold 1 — вертикаль 4:3 как было
              if (!roomFovOld) fy = lensData(fy);
              memcpy(cp, e3, 12); memcpy(lk, l3, 12); roomFovY = fy; roomCamOn = 1;
              homer.camYaw = atan2f(l3[0] - e3[0], l3[2] - e3[2]); } }   // управление — от вида этой камеры
          zoneCamOn = 0;
          if (!inRoom() && nzcam && !zoneOff) {   // правка 40: камера зоны города — та, в чьём триггере ноги Гомера
            float hp[3] = {homer.x, homer.y, homer.z}, e3[3], l3[3], fy; int prevZ = zcs.cur;
            int zcr_ = roomCamStepFrom(zcam, nzcam, &zcs, hp, dt, e3, l3, &fy, cp, lk);   // сверка g06 (g06-camera:16,19,25,36): переходы и выход как у оригинала
            if (zcr_ == -2) { memcpy(cp, e3, 12); memcpy(lk, l3, 12); }
            if (zcr_ >= 0) {
              memcpy(cp, e3, 12); memcpy(lk, l3, 12); zoneFovY = lensData(fy); zoneCamOn = 1;
              zoneNear = fmaxf(.3f, fminf(NEAR_Z, hypotf(e3[0] - homer.x, e3[2] - homer.z) - .5f));   // рельс бывает в 0.5 м от Гомера
              homer.camYaw = atan2f(l3[0] - e3[0], l3[2] - e3[2]); }   // управление — от вида этой камеры, как в комнатах
            if (smkOn && zcs.cur != prevZ) { char s[160]; snprintf(s, sizeof s, "ZONECAM t=%.2f cam=%s eye=%.2f,%.2f,%.2f homer=%.2f,%.2f,%.2f\n", smkT,
                                              zcs.cur >= 0 ? zcam[zcs.cur].name : "-", cp[0], cp[1], cp[2], homer.x, homer.y, homer.z); smkWrite(s); } }
          x = homer.x; y = homer.y; z = homer.z; h = homer.h; v = homer.speed; }
        { // Правка Роба 338 (05.10: «у гэгов кривые ракурсы»; у газа АЭС L1 H4-7 — камера зоны r6_cam с диска): у StaticCam в данных
          // есть CutInOut и TransitionTargetRate. Камеры зон L1, кроме r6_cam1 / homer_office / homer_outside (CutInOut 1, скорость 1), —
          // CutInOut 0, скорость 0,04–0,065 за кадр 1/60 с (у статичной камеры — в posLag записи). Толкование порта (кода оригинала нет, вопрос близнецу): 0 — вход и выход
          // плавным переходом с постоянной −1/(60·ln(1 − скорость)) (0,04 → 0,41 с), переход — за 3 постоянные (0,3–1,5 с); 1 — срезом,
          // как было у всех. «flag zonecutold 1» — прежнее (все срезом)
          static int zPrev = -1; int zNow = zoneCamOn ? zcs.cur : -1;
          if (!zoneCutOld && zNow != zPrev && csLastOk && !inRoom() && !mcPlay) {
            int via = zNow >= 0 ? zNow : zPrev;
            float dx = csLastCp[0] - cp[0], dz = csLastCp[2] - cp[2];
            if (via >= 0 && via < nzcam && zcam[via].type == 0 && !zcam[via].cut && dx * dx + dz * dz < 30.f * 30.f) {   // у рельсов поля нет — как было
              float tr = zcam[via].posLag, tau = tr > 0.f && tr < 1.f ? -1.f / (60.f * logf(1.f - tr)) : .1f;
              camBlendFrom(csLastCp, csLastLk, fminf(1.5f, fmaxf(.3f, 3.f * tau)));
              if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "ZONEBLEND t=%.2f cam=%s %s dur=%.2f\n", smkT, zcam[via].name, zNow >= 0 ? "in" : "out", camBDur); smkWrite(s_); }
            }
          }
          zPrev = zNow; }
        talkCamOn = 0;
        if (onFoot() && !inRoom() && !mcPlay && !zoneCamOn) {   // правка 40: разговор — план реплики из данных этапа (под камерой зоны — она)
          float e3[3], l3[3], fy;
          if (mvTalkCam(cp, talkRay, e3, l3, &fy, &talkNear)) { memcpy(cp, e3, 12); memcpy(lk, l3, 12); talkFovY = fy; talkCamOn = 1; } }
#ifndef TALKARC_OLD
#define TALKARC_OLD 0   // сверка g10b-loop круг 3 (P08:11): 1 — переход камеры разговора по прямой между позами, как в круге 1
#endif
#ifndef TALKCAM_BLEND_OLD
#define TALKCAM_BLEND_OLD 0   // сверка g10b-loop, P08:11: 1 — камера разговора включается и выключается срезом (прежнее)
#endif
        // сверка g10b-loop, P08:11: вход в камеру разговора и выход из неё идут переходом ~0,45 с (до 95 %), не срезом; между репликами смена плана — срез. Дуга вокруг цели
        // заменена плавной линией между позами (общий переход камеры, как у зон); срез — если прошлая поза дальше 30 м (собеседников унесло). TALKCAM_BLEND_OLD=1 — прежнее
        { static int talkPrevOn_;
          if (!TALKCAM_BLEND_OLD && talkCamOn != talkPrevOn_ && csLastOk && !mcPlay && !inRoom() && camBT < 0.f) {
            float dx_ = csLastCp[0] - cp[0], dz_ = csLastCp[2] - cp[2];
            if (dx_ * dx_ + dz_ * dz_ < 30.f * 30.f) { camBlendFrom(csLastCp, csLastLk, g10bTalkBlendSecs()); camBArc = !TALKARC_OLD; } }
          talkPrevOn_ = talkCamOn; }
        if (mcPlay && !inRoom()) {   // правка 40: «наезд» — поза камеры из анимации диска
          float up3[3], dur = (mcPlay->nfr - 1) / mcPlay->fps;
          int going, syn_ = mcPlay == &mcSyn96;
          if (syn_) {   // 1246: короткий наезд — от точки позади и выше к обычной игровой позе (cp/lk этого кадра — игровая камера, игрок стоит)
            float u = fminf(1.f, mcT / dur), k = 1.f - u * u * (3.f - 2.f * u);
            float bx = cp[0] - lk[0], bz = cp[2] - lk[2], bl = hypotf(bx, bz);
            if (bl < .01f) { float hh_ = onFoot() ? homer.h : car.h; bx = -sinf(hh_); bz = -cosf(hh_); bl = 1.f; }
            if (mcF96 < 0.f) {   // отход не дальше свободного места: луч от игровой позы к точке отхода (стена здания не закрывает кадр)
              float a_[3] = {cp[0], cp[1], cp[2]}, b_[3] = {cp[0] + bx / bl * MCSHORT96_BACK, cp[1] + MCSHORT96_UP, cp[2] + bz / bl * MCSHORT96_BACK};
              float fr_ = haveCol ? colRayFrac(onFoot() && G.pHomerWorld ? G.pHomerWorld : &world, a_, b_, 0) : 1.f;
              mcF96 = fr_ >= .999f ? 1.f : fmaxf(0.f, fr_ - .6f / hypotf(MCSHORT96_BACK, MCSHORT96_UP));   // 0,6 м до стены
              if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "MSNCAM short free=%.2f t=%.2f\n", mcF96, smkT); smkWrite(s_); } }
            cp[0] += bx / bl * MCSHORT96_BACK * k * mcF96; cp[2] += bz / bl * MCSHORT96_BACK * k * mcF96; cp[1] += MCSHORT96_UP * k * mcF96;
            mcFovY = onFoot() ? lensFoot() : lensCar(); going = mcT < dur; }
          else {
          going = msncamPose(mcPlay, mcPlant == 1 ? dur - mcT : mcT, cp, lk, up3, &mcFovY);
          mcFovY = lensData(mcFovY);   // mcPlant — отрицательный контроль гейта
          if (mcWall103 && haveCol) { float hd_[3] = {onFoot() ? homer.x : car.x, (onFoot() ? homer.y : car.y) + 1.6f, onFoot() ? homer.z : car.z};   // Р3: глаз не за стеной
            if (mcWallPull(onFoot() && G.pHomerWorld ? G.pHomerWorld : &world, hd_, cp, &mcWallSt, dt) && smkOn) { static int wn_; if (!(wn_++ % 30)) { char s_[96]; snprintf(s_, sizeof s_, "MSNCAM wall st=%.2f t=%.2f\n", mcWallSt, smkT); smkWrite(s_); } } }
          }
          if (mcPlant == 1) going = mcT < dur;
          if (mcPlant == 2) { cp[0] += 20.f; lk[0] += 20.f; }
          if (smkOn) { char s[200]; snprintf(s, sizeof s, "MSNCAMP t=%.3f ct=%.4f phase=%s eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f fovy=%.2f homer=%.2f,%.2f,%.2f car=%.2f,%.2f\n",
                                             smkT, mcT, mcPlay->phase, cp[0], cp[1], cp[2], lk[0], lk[1], lk[2], mcFovY, homer.x, homer.y, homer.z, car.x, car.z); smkWrite(s); }
          if (!going) {
            if (smkOn) { char s[64]; snprintf(s, sizeof s, "MSNCAM end t=%.1f fr=%u\n", smkT, smkFrameAbs); smkWrite(s); if (syn_) { snprintf(s, sizeof s, "MSNCAM done t=%.2f\n", smkT); smkWrite(s); } }
            mcPlay = 0; chase.init = 0; homer.camInit = 0;
            if (camIn1041 && !syn_) { camBlendFrom(cp, lk, CAMIN1041_BLEND); camBArc = camBExit = camExit1170; } }   // №1041: на выходе плавный переход от позы наезда, не склейка; №1170: по дуге вокруг точки взгляда
          if (syn_ && transitBusy() && mcHold96 < 4.f) mcHold96 += dt; else mcT += dt; }   // 1246: короткий наезд начинается, когда ирис раскрыт (под ирисом его не видно); не дольше 4 с
        if (carCam && onFoot()) { memcpy(cp, ccp, 12); memcpy(lk, clk, 12); }   // правка 167: выходит — ещё камера машины
        { // правка 167: смена камеры машина <-> пешком — плавно от последней позы (выход 0,55 с, посадка 1 с), не склейкой
          static int lastCarCam = -1; static float lastE[3], lastL[3];
          if (lastCarCam >= 0 && carCam != lastCarCam && !inRoom() && !mcPlay && !camIrisCut) {   // 333: под ирисом — склейка
            float dx = lastE[0] - cp[0], dz = lastE[2] - cp[2];
            if (dx * dx + dz * dz < 30.f * 30.f) {
              camBlendFrom(lastE, lastL, carCam ? CAM_IN_TAU : CAM_OUT_S);
              if (carCam) { camCarAxes(lastE, camBE, 1); camCarAxes(lastL, camBL, 1); camBRel = 1; }   // посадка: начало едет с машиной
            }
          }
          camBFovOn = 0;
          if (camBT >= 0.f) {
            float u, fe[3], fl[3];
            if (camBRel) { u = mrCamU(camBT, camBDur); camCarAxes(camBE, fe, 0);   // сверка g09b-menu круг 3 (СВЕРКА_КАМЕРА 3.1): посадка — S-кривая от нулевой скорости (95 % за 0,45 с), не экспонента
 camCarAxes(camBL, fl, 0); }
            else { u = fminf(1.f, camBT / camBDur); u = u * u * (3.f - 2.f * u); memcpy(fe, camBE, 12); memcpy(fl, camBL, 12); }
            if (camBArc && !camBRel) {   // круг 3 (проверка g10b-loop, P08:11): вход в камеру разговора и выход идут по дуге вокруг точки взгляда — угол вокруг неё и расстояние от неё меняются отдельно
              float d0x = fe[0] - fl[0], d0z = fe[2] - fl[2], d1x = cp[0] - lk[0], d1z = cp[2] - lk[2];
              float y0 = atan2f(d0x, d0z), y1 = atan2f(d1x, d1z), dy = y1 - y0;
              while (dy > GU_PI) dy -= 2.f * GU_PI; while (dy < -GU_PI) dy += 2.f * GU_PI;
              float r0 = hypotf(d0x, d0z), r1 = hypotf(d1x, d1z), yw = y0 + dy * u, rr = r0 + (r1 - r0) * u;
              float hh = (fe[1] - fl[1]) + ((cp[1] - lk[1]) - (fe[1] - fl[1])) * u;
              for (int k = 0; k < 3; k++) lk[k] = fl[k] + (lk[k] - fl[k]) * u;
              cp[0] = lk[0] + rr * sinf(yw); cp[1] = lk[1] + hh; cp[2] = lk[2] + rr * cosf(yw);
            } else
            for (int k = 0; k < 3; k++) { cp[k] = fe[k] + (cp[k] - fe[k]) * u; lk[k] = fl[k] + (lk[k] - fl[k]) * u; }
            camBU = u; camBFovOn = camBFov > 0.f;
            if (smkOn && camLogOn) { char s_[160]; snprintf(s_, sizeof s_, "CAMBLEND t=%.3f u=%.3f rel=%d eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f\n", smkT, u, camBRel,
                                                          cp[0], cp[1], cp[2], lk[0], lk[1], lk[2]); smkWrite(s_); }
            camBT += dt; if (((camBRel && CAMBLEND_OLD) ? u > .98f : camBT >= camBDur) || inRoom() || mcPlay) camBT = -1.f;
          }
          lastCarCam = carCam; memcpy(lastE, cp, 12); memcpy(lastL, lk, 12); camIrisCut = 0;
        }
        phoneCamOn = 0;
        if (phoneAnimT >= 0.f) {   // правка 166: облёт будки (phonecamera.p3d в осях будки), потом камера пешком с той же стороны
          if (onFoot() && !inRoom() && !mcPlay && !talkCamOn && phoneAnim && phoneAnimK >= 0 && phoneAnimK < (int)world.nphonex) {
            float e3[3], l3[3], fy, ta = fmaxf(0.f, phoneAnimT - PHONE_ANIM_WAIT); int going = phoneAnimPose(ta, e3, l3, &fy);
            memcpy(cp, e3, 12); memcpy(lk, l3, 12); phoneFovY = lensData(fy); phoneCamOn = 1;
            if (smkOn) { char s[200]; snprintf(s, sizeof s, "PHONECAM t=%.3f pt=%.3f at=%.3f booth=%s eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f fovy=%.1f\n", smkT, phoneAnimT, ta,
                                               world.phone[phoneAnimK].name, e3[0], e3[1], e3[2], l3[0], l3[1], l3[2], fy); smkWrite(s); }
            phoneAnimT += dt;
            if (!going) {
              phoneAnimT = -1.f; homer.camYaw = atan2f(homer.x - e3[0], homer.z - e3[2]); camBlendFrom(e3, l3, PHONE_ANIM_OUT);
              if (smkOn) { char s[64]; snprintf(s, sizeof s, "PHONECAM end t=%.2f\n", smkT); smkWrite(s); }
            }
          } else phoneAnimT = -1.f;
        }
        if (phoneCamT >= 0.f) {   // правка 166: камера будки — взгляд на новую машину и обратно к Гомеру
          if (onFoot() && !inRoom() && !mcPlay && !talkCamOn) {
            float wv = phoneCamT < PHONE_CAM_HOLD ? fminf(1.f, phoneCamT / .25f) : 1.f - fminf(1.f, (phoneCamT - PHONE_CAM_HOLD) / PHONE_CAM_BACK);
            wv = wv * wv * (3.f - 2.f * wv);
            for (int k = 0; k < 3; k++) lk[k] += (phoneCamCar[k] - lk[k]) * wv;
          }
          phoneCamT += dt; if (phoneCamT > PHONE_CAM_HOLD + PHONE_CAM_BACK) phoneCamT = -1.f;
        }
        if (smkCamOn) { memcpy(cp, smkCam, 12); if (smkCamRel) { cp[0] += onFoot() ? homer.x : car.x; cp[1] += onFoot() ? homer.y : car.y; cp[2] += onFoot() ? homer.z : car.z; } for (int k = 0; k < 3; k++) lk[k] = cp[k] + smkCam[3 + k]; }   // близнецы Б3: поза оригинала; №863 camrel — от героя
        // правка 331: камера игры (за машиной, пешком, переходы) — её правило и замер; камеры из данных (комнаты, зоны, наезды, разговор,
        // будка) — как есть
        camFollow = !roomCamOn && !zoneCamOn && !carZoneOn && !mcPlay && !talkCamOn && !phoneCamOn && !smkCamOn && !inRoom();
        // опора — над крышей своей машины / у головы Гомера: из середины кузова на краю крыши или на наклоне луч сразу упирался в мир
        camPivotCar = carCam;
        if (carCam) { CarBox cb_; memset(&cb_, 0, sizeof cb_); carBox(&car, &cb_); camPivot[0] = cb_.x; camPivot[1] = cb_.y + cb_.hi + .3f; camPivot[2] = cb_.z; }
        else { camPivot[0] = homer.x; camPivot[1] = homer.y + 1.5f; camPivot[2] = homer.z; }
        // TEST19 (видео 2:07): облёт будки шёл сквозь живую изгородь (кадр в листве). Облёту — то же правило «глаз не за геометрией и не внутри», опора — Гомер.
        // CAM_PHONE_GUARD_OLD=1 / flag camphoneguardold 1 — облёт как есть
        int camGuard = camFollow || (!camPhoneGuardOld && phoneCamOn && !inRoom() && !smkCamOn);
        if (camGuard && carCam && chaseView == 2 && !selectCycleOld) camGuard = 0;   // №617: вид от первого лица — глаз у водителя внутри своей машины, правило «глаз не в кузове» его не выбрасывает
        if (camGuard && !camSafeOld && haveCol && !fly) { float fv_ = onFoot() && !footFovOld ? lensFoot() : lensCar(); if (phoneCamOn && !camPhoneGuardOld) fv_ = phoneFovY; if (camBFovOn) fv_ = camBFov + (fv_ - camBFov) * camBU;
          if (!CARAIM1160_OLD && !onFoot() && chaseView != 2 && !(phoneCamOn && !camPhoneGuardOld)) fv_ = camAimWideFov(fv_, fmaxf(fabsf(camAimC), fabsf(camAimEff(camUserCarAim))));   // №1160: глаз проверяется по расширенному кадру Car Aim (закон объектива: сверху у сдвинутого кадра больше места)
          if (camBExit && camBT >= 0.f) csOfsOn = csPrevOn = 0;   // №1170: пока идёт дуга после наезда, предел скорости перелёта (25 м/с) не работает: он отставал от дуги и потом срезал её по хорде через голову героя; безопасность глаза проверяется каждый кадр как обычно
          unsigned t0_ = sceKernelGetSystemTimeLow(); csNearMinCur = (phoneCamOn && !camPhoneGuardOld) ? CAM_PHONE_CLEAR : CS_NEAR_MIN; float ed_[3] = {cp[0], cp[1], cp[2]};
          // TEST24 (цена, FPS приоритет 1): прошлый кадр — желаемый глаз годен без поправки, впереди свободно (NEAR_Z), глаз и опора сдвинулись мало, сцена проверки свежая —
          // следующий кадр правило глаза пропускается (раз в 2 кадра). Негодный глаз тогда исправится кадром позже. CAM_SAFE_SKIP_OLD=1 / flag camsafeskipold 1 — каждый кадр
          int skip_ = !camSafeSkipOld && !camSkipPrev && !phoneCamOn && csPrevOn && !csOfsOn && csWhyPick == -2 && csNfV >= NEAR_Z - 1e-3f &&
                      fabsf(ed_[0] - csLastEd[0]) + fabsf(ed_[1] - csLastEd[1]) + fabsf(ed_[2] - csLastEd[2]) < 1.5f &&
                      fabsf(camPivot[0] - csPrevP[0]) + fabsf(camPivot[1] - csPrevP[1]) + fabsf(camPivot[2] - csPrevP[2]) < 1.5f &&
                      fabsf(csSceneE[0] - ed_[0]) + fabsf(csSceneE[1] - ed_[1]) + fabsf(csSceneE[2] - ed_[2]) < 2.f;
          camSkipPrev = skip_;
          if (skip_) { csSfOn = 0; camSkipN++; memcpy(csLastEd, ed_, 12); memcpy(csPrev, ed_, 12); memcpy(csPrevP, camPivot, 12); for (int i = 0; i < 3; i++) csPrevRel[i] = ed_[i] - camPivot[i]; csPrevCar = camPivotCar; csOfsOn = 0; csSceneOk = 1; }
          else camSafeEye(cp, lk, fv_, dt); csNearMinCur = CS_NEAR_MIN; unsigned us_ = sceKernelGetSystemTimeLow() - t0_;
          if (phoneCamOn && !camPhoneGuardOld) {   // шаг глаза облёта за кадр: замер и (если годна точка) предел CAM_PHONE_STEP
            if (phPrevOk) {
              float d_[3] = {cp[0] - phPrevE[0], cp[1] - phPrevE[1], cp[2] - phPrevE[2]}, dl_ = sqrtf(d_[0] * d_[0] + d_[1] * d_[1] + d_[2] * d_[2]);
              float de_[3] = {ed_[0] - phPrevE[0], ed_[1] - phPrevE[1], ed_[2] - phPrevE[2]}, dd_ = sqrtf(de_[0] * de_[0] + de_[1] * de_[1] + de_[2] * de_[2]);
              if (dd_ > phDmax) phDmax = dd_;
              if (!camPhoneStepOld && dl_ > CAM_PHONE_STEP) {
                float q_[3] = {phPrevE[0] + d_[0] * (CAM_PHONE_STEP / dl_), phPrevE[1] + d_[1] * (CAM_PHONE_STEP / dl_), phPrevE[2] + d_[2] * (CAM_PHONE_STEP / dl_)};
                float tv_ = tanf(fv_ * (GU_PI / 360.f));
                csNearMinCur = CAM_PHONE_CLEAR; int ok_ = csSoftOk(q_, camPivot, lk, tv_, tv_ * ASPECT); csNearMinCur = CS_NEAR_MIN;
                if (!ok_) { csNearMinCur = CAM_PHONE_CLEAR; ok_ = csGlideAround(phPrevE, cp, camPivot, lk, tv_, tv_ * ASPECT, CAM_PHONE_STEP, q_); csNearMinCur = CS_NEAR_MIN; }   // прямо нельзя (листва) — в обход
                if (!ok_ && phHold < 20) { csNearMinCur = CAM_PHONE_CLEAR; ok_ = csSoftOk(phPrevE, camPivot, lk, tv_, tv_ * ASPECT); csNearMinCur = CS_NEAR_MIN; if (ok_) { memcpy(q_, phPrevE, 12); phHold++; } }   // стоим, пока облёт не освободит путь (не дольше 20 кадров)
                if (ok_) { memcpy(cp, q_, 12); float a_ = cp[0] - phPrevE[0], b_ = cp[1] - phPrevE[1], c_ = cp[2] - phPrevE[2]; dl_ = sqrtf(a_ * a_ + b_ * b_ + c_ * c_); phClamp++; if (dl_ > 1e-3f) phHold = 0; }
              }
              if (camInLog && (dl_ > .3f || dd_ > .3f)) { char s_[200]; snprintf(s_, sizeof s_, "PHSTEP t=%.2f dt=%.3f step=%.2f want=%.2f clamp=%d\n", smkT, dt, dl_, dd_, dl_ == CAM_PHONE_STEP); smkWrite(s_); }
              if (dl_ > .5f + 1e-3f) phJump++; if (dl_ > phJmax) phJmax = dl_;
            }
            memcpy(phPrevE, cp, 12); phPrevOk = 1;
          } else { phPrevOk = 0; phHold = 0; }
          csubT[1] += smkOn ? us_ : 0; camSafeUs += us_; camSafeUsN++; if (us_ > camSafeUsMax) camSafeUsMax = us_; } else csOfsOn = csPrevOn = 0;   // правка 331: глаз никогда не внутри (наружу сразу, назад плавно)
        // №639: камеры из данных (зоны, зоны машины, миссии, разговор, будка без правила) заданы сценой оригинала и стену мы не двигаем; лишь глаз под землёй (опора коллизии выше глаза) — на 0,3 м над ней.
        // Комнатные камеры roomCamOn не трогаем (коллизии у внешних стен комнаты нет). CAM_DATA_GROUND_OLD=1 / flag camdatagroundold 1 — как было
        if (!camDataGroundOld && haveCol && !fly && !camFollow && !roomCamOn && !smkCamOn && (zoneCamOn || carZoneOn || mcPlay || talkCamOn || phoneCamOn)) {
          const ColWorld *cw_ = onFoot() && G.pHomerWorld ? G.pHomerWorld : &world; float gy_;
          if (colGround(cw_, cp[0], cp[2], cp[1] + .3f, cp[1] - 8.f, &gy_, 0) && cp[1] < gy_ + .3f) { cp[1] = gy_ + .3f; dgFix++; } }
        if (onFoot() && (lk[0] - cp[0]) * (lk[0] - cp[0]) + (lk[2] - cp[2]) * (lk[2] - cp[2]) > 1e-4f) homerViewYaw = atan2f(lk[0] - cp[0], lk[2] - cp[2]);   // №1059: вид на экране = курс переднего вектора камеры этого кадра (как cam fwd PS2), в следующем кадре идёт в steer
        memcpy(csLastCp, cp, 12); memcpy(csLastLk, lk, 12); csLastOk = 1;   // правка 338: поза кадра — начало перехода камер зон
        floorNearV = 1e9f;   // №861: защита №837 считает по обычной ближней плоскости (правило пола прошлого кадра её не ослабляет); правило пола — ниже, после тряски
        if (!CAM_FLOOR837_OLD && haveCol && !inRoom() && !fly && !bpass) { CSUBT0; camFloorGuard(onFoot() && G.pHomerWorld ? G.pHomerWorld : &world, cp, lk, frameNear(0, 0), curFovY, dt); CSUB(2); }   // №837
        if (smkOn && !fly && !bpass) { static float cu_; if ((cu_ += wallDt) >= .5f) { cu_ = 0.f; char l_[200]; int ft_ = onFoot();   // №996 служебно: камера — расстояние и высота глаза от опоры (Гомер пешком / машина), стена: упёрлась ли
      snprintf(l_, sizeof l_, "CAMU996 t=%.1f foot=%d d=%.3f h=%.3f hits=%d wall=%.3f room=%d\n", smkT, ft_, hypotf(cp[0] - (ft_ ? homer.x : car.x), cp[2] - (ft_ ? homer.z : car.z)), cp[1] - (ft_ ? homer.y : car.y), ft_ ? homer.camHits : 0, ft_ ? 1.f : chaseInfo.wall, inRoom()); smkWrite(l_); } }
        camx = cp[0]; camy = cp[1]; camz = cp[2];
        if (smkOn && camLogOn) { CSUBT0; char s_[220]; snprintf(s_, sizeof s_, "CAMF t=%.3f st=%d fov=%.1f eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f homer=%.3f,%.3f,%.3f car=%.3f,%.3f,%.3f,%.3f near=%.3f rc=%d\n", smkT,
                                 haveHomer ? (int)homer.state : -1, curFovY, cp[0], cp[1], cp[2], lk[0], lk[1], lk[2], homer.x, homer.y, homer.z, car.x, car.y, car.z, car.h,
                                 frameNear(inRoom(), 0), roomCamOn); smkWrite(s_);
          if (onFoot()) { char f_[160]; snprintf(f_, sizeof f_, "FOOTCAM t=%.3f off=%.2f dist=%.2f ey=%.2f hits=%.0f pull=%.2f rise=%.2f carback=%.0f carpin=%.0f\n", smkT, homerCamDbg[0], homerCamDbg[1], homerCamDbg[2], homerCamDbg[3], homerCamDbg[4], homerCamDbg[5], homerCamDbg[6], homerCamDbg[7]); smkWrite(f_); } CSUB(4); }   // 16.10: rc = камера комнаты авторская (1) или ходячая (0), как в ветке Стен   // правка 340: ближняя плоскость кадра (модель GE tests/guard340/ge340.py)
        if (smkOn && talkCamOn) {   // гейт tests/camera/convcam_emu: кадр разговора — камера, как нарисована, и где стоят оба
          MvTalkCamInfo ti; if (mvTalkCamInfo(&ti) && (ti.line != talkLogLine || smkT - talkLogT > .25f)) {
            talkLogLine = ti.line; talkLogT = smkT; char s[400];
            snprintf(s, sizeof s, "TALKCAM t=%.2f line=%d pc=%d shot=%s side=%d data=%d best=%d eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f fovy=%.1f near=%.2f "
                     "pcpos=%.3f,%.3f,%.3f npcpos=%.3f,%.3f,%.3f pchead=%.2f npchead=%.2f pch=%.2f npch=%.2f npc=%s conv=%s\n", smkT, ti.line, ti.pcSpeaks,
                     convShotName(ti.shot), ti.side, ti.fromData, ti.hasBest, cp[0], cp[1], cp[2], lk[0], lk[1], lk[2], talkFovY, frameNear(inRoom(), 0),
                     ti.pc[0], ti.pc[1], ti.pc[2], ti.npc[0], ti.npc[1], ti.npc[2], ti.pcHead, ti.npcHead, ti.pcH, ti.npcH, ti.npc_name, ti.conv);
            smkWrite(s); } }
        if (smkOn && (camStatOn || camStatAlwaysOld) && haveCol && !roomCamOn && !mcPlay && !talkCamOn && !zoneCamOn && smkT > camStatT0) {   // гейт tests/camera (правка 40): глаз за препятствием от точки взгляда?
          const ColWorld *cw = onFoot() && G.pHomerWorld ? G.pHomerWorld : &world;
          CSUBT0; camFrames++; if (colRayFrac(cw, lk, cp, 0) < .999f || !camOverGround(cw, cp[0], cp[2], cp[1])) camBlocked++; CSUB(3); }   // за стеной или над пустотой
        ex = camx; ey = camy; ez = camz;
        cfx = lk[0] - camx; cfy = lk[1] - camy; cfz = lk[2] - camz; }
      float l = sqrtf(cfx * cfx + cfy * cfy + cfz * cfz); cfx /= l; cfy /= l; cfz /= l;
      SMK5(2); camSubReport();   // ph5: камера
      { static float vy0; static int air0;   // этап 6: мотор, удары, приземления, слушатель
        int drv = !haveHomer || (HOMER_CABENG_OLD ? homerDriving(&homer) : homerCabEngine(&homer));   // сверка g10-flow круг 2 (g04 P10 Д7): мотор кабины работает до конца анимации выхода
        // правка 175 (близнец В13, knowledge/facts/ps2-wrecked-car-sound.yaml): у оригинала мотор машины, из которой вышли (и
        // разбитой тоже), глохнет за ~3 с — гул −34 → −44 dB, дальше только фон; у порта мотор и «тарахтение» разбитой играли везде,
        // даже в доме. Пешком мотор работает ENG_OFF_S с выхода, потом выключен; сел обратно — снова
        static float engOffT; engOffT = drv ? 0.f : engOffT + dt; int dead_ = car.destroyed && !carHuskDrives(&car);   // правка 355: остов игрока едет — с мотором
        int engOn = !dead_ && (!SV_PARK_OLD || engOffT < ENG_OFF_S);   // сверка g08-sound (P10-16): оставленная машина держит холостой клип (15/50 м), не глохнет через 3 с
        streetSourceFrame(); trafficMotorFrame(dt,camx,camy,camz); trafficHornFrame(dt,camx,camy,camz); trafficVoteFrame(dt,camx,camy,camz);
        sndPoliceCars(inRoom() ? NULL : mvCars());
        sndCarPosition(car.x,car.ry,car.z,!drv); // №390: outside, source stays at the parked car; inside preserves native mix
        sndPlayerCarState(car.p.name, car.destroyed); // authored live model or the drivable huskA wreck
        sndCarMass(car.p.mass);   // сверка g08 круг 6 (P09:25, P01-13): размер удара считается по массе своей машины (раньше везде 1500 кг); SV_MASS_OLD=1 — как раньше
        sndBurnLevel(in.throttle > .99f && car.p.burnoutRange > 0.f && fabsf(car.v) < car.p.burnoutRange * car.p.topSpeed ? fmaxf(0.f, 1.f - .5f * (fabsf(car.v) / car.p.topSpeed) / car.p.burnoutRange) : 0.f);   // круг 7 (g08, P10-11): уровень бернаута как у оригинала; ручник-скольжение (уровень 1 по тормозу) не передан
        sndCarState(car.p.topSpeed, skidSoundLevel(&car, &in), !car.grounded && car.airT > .3f, haveHomer && !SV_CAB_OLD ? !homerCabEngine(&homer) : !drv, 1.f - car.damage,
                    in.throttle > .99f && car.p.burnoutRange > 0.f && fabsf(car.v) < car.p.burnoutRange * car.p.topSpeed, in.brake > .05f || car.v < -.5f);   // круг 3: бернаут и подавление Burn   // круг 2 (g04): мотор кабины работает до конца анимации выхода   // сверка g08-sound (P10-5,8,10,11,15,16): шкала передач = TopSpeed ведомой машины, занос, воздух, остаток жизни
        sndEngine(car.v, dead_ || !drv ? 0.f : in.throttle,
                  (engOn ? SND_ENG_ON : 0) | (car.v < -0.5f ? SND_ENG_REVERSE : 0) | (engOn && car.damage >= .6f ? SND_ENG_DAMAGED : 0));
        sndSkid(skidSoundLevel(&car, &in), 0); // №446: same authored skid/brake rule as tyre marks
        // Правка 413 (гудок). SLES_518.97 VehicleSoundPlayer (0x231838…0x231888): ввод гудка > 0,05 — клип HornClipName машины (поле +88
        // параметров; пусто — «horn»), петлёй ресурса, пока держат. ПРАВИЛО ПОРТА — кнопка: ↑ крестовины (в машине свободна: ← → руль,
        // X/R газ, ○/L тормоз, □ ручник, △ выход, SELECT сброс; у PSP нет L3 оригинала). HORN_OLD=1 / «flag hornold 1» — гудка нет (прежнее)
        sndHorn(!HORN_OLD && !hornOld && drv && !dead_ && engOn && (pad.Buttons & PSP_CTRL_UP) ? 1 : 0);
        if (!CHEATS1104_OLD && drv && !dead_ && engOn && (pad.Buttons & PSP_CTRL_UP)) carHornLeap(&car, 1.f);   // №1104: код Car Jump on Horn — прыжок по гудку (↑)
        if (haveTraffic) trafficPlayerHorn(&traffic, !HORN_OLD && !hornOld && drv && !dead_ && engOn && (pad.Buttons & PSP_CTRL_UP) ? 1 : 0, x, z);   // сверка g02-traffic (P03-15, 7.4): гудок игрока — трафик в 25 м отвечает, пешеходы в 12 м пугаются
        carSndFrame(&car, &world);   // правка 135: твёрдое — crash, листва — шорох, объект — только его примитивом, машина — sndCarHitCar
        sndCarDamage(car.damage);
        int air = !car.grounded && car.airT > .3f; sndCarAir(air);
#ifndef LAND_SPARK_OLD
#define LAND_SPARK_OLD 0   // сверка g10b-loop, P01-29: 1 — искр от днища при посадке после полёта нет (прежнее)
#endif
        if (air0 && car.grounded) { sndCarLand(-vy0);
          // сверка g10b-loop, P01-29: сильная посадка после полёта (подвеска в упоре) на асфальте, металле, гравии — искры от днища по тому же событию, что звук посадки;
          // поверхность — svSurfaceAt (sverka_shared.h; пока данных нет — дорога). Порог -vy0 > 4 м/с — правило порта (подвеска в упоре в данных машины не читается)
          if (LAND_SPARK_OLD && -vy0 > 4.f) { int sf_ = svSurfaceAt(car.x, car.ry, car.z, NULL);
            if (g10bLandSpark(vy0, sf_ == SURF_ROAD || sf_ == SURF_METAL || sf_ == SURF_GRAVEL))
              for (int k_ = 0; k_ < 10; k_++) fxSpawn(FX_SPARK, car.x, car.ry + .15f, car.z, (FR() - .5f) * 6.f + car.vel[0] * .2f, FR() * 3.f + .5f, (FR() - .5f) * 6.f + car.vel[2] * .2f); } }
        // круг 7 (P01-29, сверено с оригиналом): искры по упору подвески (любое колесо сжато до предела) в течение секунды после полёта на всех четырёх колёсах, один раз за полёт: 30 искр радиально
        // по земле, 8,3 м/с (2,5 м за 0,3 с), чуть вверх; поверхность асфальт, металл, гравий (svSurfaceAt). LAND_SPARK_OLD=1 — прежнее (10 искр при посадке быстрее 4 м/с)
        { static float airSince_ = 9.f; int allAir_ = car.comp[0] <= 0.f && car.comp[1] <= 0.f && car.comp[2] <= 0.f && car.comp[3] <= 0.f, bo_ = g10bBottomOut(car.comp, car.lim), ok_ = 0;
          if (bo_ && airSince_ + dt <= 1.f) { int sf_ = svSurfaceAt(car.x, car.ry, car.z, NULL); ok_ = sf_ == SURF_ROAD || sf_ == SURF_METAL || sf_ == SURF_GRAVEL; }
          if (g10bBottomSparkStep(&airSince_, dt, allAir_, bo_, ok_))
            for (int k_ = 0; k_ < G10B_SPARK_N; k_++) { float an_ = FR() * 6.2831853f; fxSpawn(FX_SPARK, car.x, car.ry + .15f, car.z, cosf(an_) * 8.3f, FR() * .8f, sinf(an_) * 8.3f); } }
        air0 = air; vy0 = car.vel[1];
        sndFrame(dt, camx, camy, camz, -cfz, cfx, x, y, z); SMK5(3); }   // зоны звука — по активному игроку (Гомер пешком или машина)
    } else if (!fly) {
      float st = ax;
      if (pad.Buttons & PSP_CTRL_LEFT) st = -1; if (pad.Buttons & PSP_CTRL_RIGHT) st = 1;
      steer += (st - steer) * fminf(1.f, dt * 8.f);
      int gas = pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_RTRIGGER);
      int brake = pad.Buttons & (PSP_CTRL_SQUARE | PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE);   // ○ — тормоз, как в оригинале (правка 42)
      if (gas) v += (v < 0 ? 30.f : 14.f) * dt;
      else if (brake) v -= (v > 0 ? 30.f : 8.f) * dt;
      else v -= v * 1.2f * dt;
      if (v > 30.f) v = 30.f; if (v < -9.f) v = -9.f;
      float turn = steer * 2.1f * fminf(1.f, fabsf(v) / 6.f) / (1.f + fabsf(v) / 25.f);
      h -= turn * dt * (v >= 0 ? 1.f : -1.f);
      float nx = x + sinf(h) * v * dt, nz = z + cosf(h) * v * dt, g;
      float bump = v >= 0 ? 2.2f : -2.2f;   // нос/багажник машины
      float px = nx + sinf(h) * bump, pz = nz + cosf(h) * bump;
      int stuck = wallAt(x, z, y);          // если застряли в стене — выпускаем
      if (ground(nx, nz, y, 1.0f, &g) && g > y - 25.f && (stuck || (!wallAt(px, pz, y) && !wallAt(nx, nz, y)))) { x = nx; z = nz; }
      else v = -v * .2f;
      if (ground(x, z, y, 1.0f, &g)) {
        if (g >= y - .35f && vy <= 0) { y = g; vy = 0; }
        else { vy -= 22.f * dt; y += vy * dt; if (y < g) { y = g; vy = 0; } }
      } else { vy -= 22.f * dt; y += vy * dt; }
      if (y < -200) { x = spawnP[0]; y = spawnP[1]; z = spawnP[2]; v = vy = 0; }
      float tx = x - sinf(h) * 7.5f, tz = z - cosf(h) * 7.5f, ty = y + 3.2f, k = fminf(1.f, dt * 5.f);
      camx += (tx - camx) * k; camy += (ty - camy) * k; camz += (tz - camz) * k;
      ex = camx; ey = camy; ez = camz;
      cfx = x - camx; cfy = (y + 1.4f) - camy; cfz = z - camz;
      float l = sqrtf(cfx * cfx + cfy * cfy + cfz * cfz); cfx /= l; cfy /= l; cfz /= l;
    } else {
      fyaw += ax * 2.f * dt; fpitch -= ay * 1.5f * dt;
      if (fpitch > 1.5f) fpitch = 1.5f; if (fpitch < -1.5f) fpitch = -1.5f;
      float sp_ = (pad.Buttons & PSP_CTRL_CROSS) ? 60.f : 15.f;
      cfx = sinf(fyaw) * cosf(fpitch); cfy = sinf(fpitch); cfz = -cosf(fyaw) * cosf(fpitch);
      float rx = cosf(fyaw), rz = sinf(fyaw);
      if (pad.Buttons & PSP_CTRL_UP) { fx += cfx * sp_ * dt; fy += cfy * sp_ * dt; fz += cfz * sp_ * dt; }
      if (pad.Buttons & PSP_CTRL_DOWN) { fx -= cfx * sp_ * dt; fy -= cfy * sp_ * dt; fz -= cfz * sp_ * dt; }
      if (pad.Buttons & PSP_CTRL_RIGHT) { fx += rx * sp_ * dt; fz += rz * sp_ * dt; }
      if (pad.Buttons & PSP_CTRL_LEFT) { fx -= rx * sp_ * dt; fz -= rz * sp_ * dt; }
      if (pad.Buttons & PSP_CTRL_RTRIGGER) fy += sp_ * dt;
      if (pad.Buttons & PSP_CTRL_LTRIGGER) fy -= sp_ * dt;
      ex = fx; ey = fy; ez = fz;
    }

    if (flyPathV > 0.f) flyPathStep(dt, &ex, &ey, &ez, &cfx, &cfy, &cfz);   // служебное: замер стриминга
    if (cmV != 0.f || cmYaw != 0.f) { cmA += cmYaw * dt; float c_ = cosf(cmA), s_ = sinf(cmA), fx_ = cfx * c_ - cfz * s_, fz_ = cfx * s_ + cfz * c_; cfx = fx_; cfz = fz_; cmP[0] += cfx * cmV * dt; cmP[1] += cfz * cmV * dt; ex += cmP[0]; ez += cmP[1]; }   // служебное «flag cammove V» (м/с вперёд по взгляду) и «flag camyaw рад/с»: камера идёт и поворачивается поверх сценария — замер отсечения и списков на скорости (в эмуляторе машина едет медленнее PSP)
    if (haveColl && !fly && !bpass) { float sy_ = ey; collShake(&ex, &ey, &ez); if (!floorNearOld && ey < sy_ && haveCol && !inRoom()) floorEyeClamp(&ey, ex, ez, sy_); }   // В5: тряска камеры гэга (GagSetCameraShake); №861: после неё глаз над полом
    drawDist = (opt & O_D100) ? 100.f : (longView ? DIST_LONG : DIST_SHORT) * drawDistMul * (DD_ADAPT_OLD ? 1.f : (ddReg1244Off || memLadDdMul >= 1.f ? ddScale * memLadDdMul : fminf(ddScale, memLadDdMul)));   // №1244: два регулятора (по времени кадра и по памяти) - минимум, а не произведение (0,6 х 0,6 давали 90 м при полу 150)   // до stream(): радиусы загрузки идут от дальности; ddScale — бюджет кадра (DD_ADAPT_OLD)
    // №887 (Роб 23.10: «крупная оптимизация — панорама дали»): бот-репортёр — даль 250 м нужна в 15–38 % мест, город до 150 м вместо 250 — −27…33 %
    // треугольников. Когда у уровня есть силуэты дали от 150 м (horizon.shr, tools/horizon_export.py --near 150 --cell 60), город, туман и подгрузка клеток —
    // до FAR150_D (только с файлом нового расчёта: радиус ленты < 180 м); дальше — плоский силуэт (стиль, принятый Робом 16.10: «фасады как в вестернах»). FAR150_OLD=1 / «flag far150 0» — прежняя даль
    static CcState ccS; static float ccFovH; static int ccCutPrev = -1; int ccOn_ = 0;   // №1104/№1119: код Unlock All Cameras — виды comedy, wreckless, chase, kull, tracker, debug (cheatcam.h); выбираются по SELECT, только с кодом
    if (!CHEATS1104_OLD && !gameCheatOn(CH_CAMERAS) && chaseView > 2) chaseView = 0;   // коды сброшены (выход в меню) — вид из списка кода уходит
    if (!CHEATS1104_OLD && !CHEATCAM_OLD && chaseView >= CC_VIEW_COMEDY && gameCheatOn(CH_CAMERAS) && haveCol && !onFoot() && !fly && !inRoom() && !mcPlay && !zoneCamOn && !carZoneOn && !talkCamOn && !phoneCamOn && !bpass) {
      CcIn in_; memset(&in_, 0, sizeof in_); in_.t[0] = car.x; in_.t[1] = car.y; in_.t[2] = car.z; in_.hx = sinf(car.h); in_.hz = cosf(car.h); in_.dt = dt;
      in_.sx = ccStX; in_.sy = ccStY; in_.toggle = ccStDown; in_.prevEye[0] = eyeX; in_.prevEye[1] = eyeY; in_.prevEye[2] = eyeZ; in_.prevLook[0] = cfX; in_.prevLook[1] = cfY; in_.prevLook[2] = cfZ;
      if (chaseView == CC_VIEW_WRECK && ccNCuts > 0) { int ch_ = ccCutHit(ccCuts, ccNCuts, in_.t); if (ch_ >= 0 && ch_ != ccCutPrev) { in_.cutEnter = 1; memcpy(in_.cutPos, ccCuts[ch_].pos, 12); } ccCutPrev = ch_; }
      float eye_[3], lk_[3]; ccOn_ = 1; ccView(&ccS, chaseView, &in_, eye_, lk_, &ccFovH);
      ex = eye_[0]; ey = eye_[1]; ez = eye_[2]; cfx = lk_[0]; cfy = lk_[1]; cfz = lk_[2];
    } else ccS.init = 0;
    int far909Hi = 0;   // №909: «высокий взгляд» — глаз высоко над землёй под ним (крыша, прыжок, мост), а не над центром ячейки ленты на склоне (far909.h)
    if (!FAR150_OLD && far150On && hzN) { float gy_ = 0.f, clr_ = ground(ex, ez, ey + 1.f, 0.f, &gy_) ? ey - gy_ : -1e9f; far909Hi = far909Step(&far909St, clr_, !(hzCur < 0 || ey < hzC[hzCur].ey + FAR150_UP), dt); }
    drawDist = hz913Apply(drawDist, ex, ez, dt, longView && !fly && !(opt & O_D100), far909Hi);   // №913: потолок города по зонам (hzzone913.h), см. horizon.h; прежнее правило №887 — 150 м вне зон, кроме FAR150_OLD / far150 0 / высокого взгляда №909
    cellThrashPx = ex; cellThrashPz = ez;
    if (!cellThrashCeilOff && cellThrashCeil > 0.f) {   // потолок «места» после повторных кругов клеток; ушёл от места на CELLTHRASHCEIL_R - снят
      float dx_ = ex - cellThrashAx, dz_ = ez - cellThrashAz;
      if (dx_ * dx_ + dz_ * dz_ > CELLTHRASHCEIL_R * CELLTHRASHCEIL_R) { cellThrashCeil += 4.f * dt; if (cellThrashCeil >= 400.f) cellThrashCeilReset(); }   // ушёл из области: потолок плавно поднимается (4 м/с), без скачка дальности
      if (cellThrashCeil > 0.f && drawDist > cellThrashCeil) drawDist = cellThrashCeil; }
    if (!cityCapOff && cityCapTgt > 0.f && zone3CityOnX()) {   // 100: потолок по ёмкости области города (main_stream.h CITYCAP100_OLD)
      if (!cityCapInit) { cityCapDd = cityCapTgt; cityCapInit = 1; }
      else if (cityCapDd > cityCapTgt) { cityCapDd -= (cityCapDd - cityCapTgt <= 25.f ? 2.5f : 15.f) * dt;   // срез до 25 м - ползком (ниже порога качелей ddreg 3 м/с), глубже - быстро: памяти ждать нельзя
       if (cityCapDd < cityCapTgt) cityCapDd = cityCapTgt; }
      else if (cityCapDd < cityCapTgt) { cityCapDd += 2.f * dt; if (cityCapDd > cityCapTgt) cityCapDd = cityCapTgt; }
      if (drawDist > cityCapDd) drawDist = cityCapDd; }
    if (ringCapDd > 0.f) {   // Лестница-96 (№4б1): круг после переноса урезан по области - не рисуем дальше загруженного (main_stream.h RINGCAP_OLD)
      if (!ringCapOn || ringCapDd >= drawDist) ringCapDd = 0.f; else { if (cityNoFitD - 2.f < ringCapDd && cityNoFitD > RINGCAP_FLOOR + 2.f) ringCapDd = cityNoFitD - 2.f;   /* клетка внутри дальности не влезла (область города раздроблена/полна): дальность под её край, дыры не будет */
        if (stMissNow <= 0) { float up_ = ringCapDd + RINGCAP_RISE * dt; if (up_ > cityNoFitD - 2.f) up_ = cityNoFitD - 2.f; if (up_ > ringCapDd) ringCapDd = up_; } drawDist = ringCapDd < drawDist ? ringCapDd : drawDist; } }
    if (pano917Cap) drawDist = pano917Far * (memLadNpcOld ? 1.f : memLadDdMul);   // №917б: служебный кадр бота — дальность ровно заданная, регуляторы не трогают; Лестница-96: кроме ступени 3 лестницы (memLadDdMul < 1 только пока памяти не хватает)
    int inR = inRoom();   // 11.2: в комнате город не подгружается и не рисуется (клетки у двери остаются в памяти)
    SMK5(4);   // ph5: остаток (ходьба без машины, служебное, ворота)
    smkMark(5);   // аудит FPS (№637): ph5 = камера и прочее до ворот, ph6 = ворота + stream + плоскости
    unsigned p6a_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    zoneGatesFrame();     // правка 181: ворота подгрузки оригинала — набор регионов в памяти
    unsigned p6b_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    stFx = cfx; stFz = cfz;   // взгляд этого кадра — для порядка клеток после большого прыжка (main_stream.h jumpRush)
    unsigned scT0 = sceKernelGetSystemTimeLow();
    pending = inR ? 0 : (streamEyeOv && eyeOvOn && eyeOvOn != 3 ? stream(eyeOv[0], eyeOv[1], eyeOv[2]) : stream(ex, ey, ez));   // Лестница-96: flag streameye 1 - при глазе сценария (eyeOv) стрим грузит вокруг него, как рисуется (тур не показывает фальшивых дыр)
    if (smkOn) { static unsigned p6t_, p6n_, p6g_, p6s_, p6m_; unsigned p6c_ = sceKernelGetSystemTimeLow(); p6n_++; p6g_ += p6b_ - p6a_; p6s_ += p6c_ - p6b_; if (p6c_ - p6b_ > p6m_) p6m_ = p6c_ - p6b_;   // аудит FPS: ворота и stream() в мкс за секунду
      if (!p6t_) p6t_ = p6c_;
      if (p6c_ - p6t_ >= 1000000u) { char l_[120]; snprintf(l_, sizeof l_, "PH6SPLIT t=%.1f frames=%u gates_us=%u stream_us=%u stream_max_us=%u\n", smkT, p6n_, p6g_ / (p6n_ ? p6n_ : 1), p6s_ / (p6n_ ? p6n_ : 1), p6m_); smkWrite(l_);
        { static unsigned lc_, lb_, lpc_, lpb_; char d_[160]; snprintf(d_, sizeof d_, "DCACHE96 t=%.1f pre_calls=%u pre_kb=%u post_calls=%u post_kb=%u on=%d all=%d\n", smkT, dc96Calls - lc_, (dc96Bytes - lb_) >> 10, dc96PostCalls - lpc_, (dc96PostBytes - lpb_) >> 10, dcache96, dcAll96On); smkWrite(d_); lc_ = dc96Calls; lb_ = dc96Bytes; lpc_ = dc96PostCalls; lpb_ = dc96PostBytes; }
        if (ph6Split95) { char m_[140]; snprintf(m_, sizeof m_, "PH6MEM t=%.1f blk=%.2f free=%.2f room=%d inR=%d pend=%d drawDist=%.0f\n", smkT, (memMaxBlock ? memMaxBlock() : 0u) / 1048576.f, heapFreeBytes() / 1048576.f, (int)inRoom(), inR, pending, drawDist); smkWrite(m_); } p6t_ = p6c_; p6n_ = p6g_ = p6s_ = p6m_ = 0; } }
    if (eyeOvOn == 3 && haveCol) { int ft_ = onFoot() && haveHomer; float px_ = ft_ ? homer.x : car.x, py_ = ft_ ? homer.y : car.y, pz_ = ft_ ? homer.z : car.z, hh_ = ft_ ? 0.f : car.h; if (ft_) { float ddx_ = sinf(0.f); (void)ddx_; hh_ = 1.5708f; } float fx_ = sinf(hh_), fz_ = cosf(hh_); ex = px_ - fx_ * eyeOv[0] + fz_ * eyeOv[2]; ey = py_ + eyeOv[1]; ez = pz_ - fz_ * eyeOv[0] - fx_ * eyeOv[2]; float dx_ = px_ - ex, dy_ = py_ + .5f - ey, dz_ = pz_ - ez, l_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_) + 1e-6f; cfx = dx_ / l_; cfy = dy_ / l_; cfz = dz_ / l_; }   // служебно (№680): «flag eyeon 3» — глаз сзади (eyex м), сбоку (eyez м), над машиной или героем (eyey м), взгляд на кузов
    else if (eyeOvOn) { ex = eyeOv[0]; ey = eyeOv[1]; ez = eyeOv[2]; cfx = eyeOv[3]; cfy = eyeOv[4]; cfz = eyeOv[5]; }   // №634 служебно
    if (smkOn) {   // №637/регресс ph6: цена stream() в главном потоке, строка STREAMCOST раз в ~2 с (среднее и максимум, мкс; клеток выгружено/запрошено за период)
      static unsigned scSum, scMax, scN, scT, scU0; unsigned dtc = sceKernelGetSystemTimeLow() - scT0; scSum += dtc; if (dtc > scMax) scMax = dtc; scN++;
      if (!scT) scT = scT0;
      if (scT0 - scT > 2000000u && scN) { char s_[160]; snprintf(s_, sizeof s_, "STREAMCOST avg_us=%u max_us=%u n=%u unl=%u loads=%u tight=%d rq=%u/%u/%u t=%.1f\n", scSum / scN, scMax, scN, zgUnloads - scU0, stLoads, memTight, rqFrames, rqMarked, rqQueued, smkT);
        smkWrite(s_); { char c_[120]; unsigned n_ = ldLatN, sm_ = ldLatSum, mx_ = ldLatMax; ldLatN = ldLatSum = ldLatMax = 0; snprintf(c_, sizeof c_, "CELLLAT n=%u avg_ms=%u max_ms=%u pri=%d t=%.1f\n", n_, n_ ? sm_ / n_ : 0u, mx_, ldFinPriOn && !LDFINISHPRI1173_OLD, smkT); smkWrite(c_); }   // №1173
        scSum = scMax = scN = 0; scT = scT0; scU0 = zgUnloads; }
    }
    eyeX = ex; eyeY = ey; eyeZ = ez; cfX = cfx; cfY = cfy; cfZ = cfz;
    if (haveSP) { sprops.nearP[0][0] = ex; sprops.nearP[0][1] = ez; sprops.nearOn = 1; }   // №1000: глаз — центр зоны, где стейтпропы-деревья не спят (stateprops.c spUpdate)
    curFovY = inR && roomCamOn && !fly && !bpass ? roomFovY : (mcPlay && !fly && !bpass ? mcFovY : (phoneCamOn && !fly && !bpass ? phoneFovY : (talkCamOn && !fly && !bpass ? talkFovY : (zoneCamOn && onFoot() && !fly && !bpass ? zoneFovY : (carZoneOn && !fly && !bpass ? carZoneFovY : (onFoot() && !fly && !bpass && !footFovOld ? lensFoot() : lensCar()))))));   // 11.2: FOV камеры комнаты; правка 40: FOV «наезда» и разговора
#ifndef BUMPER_FOV_OLD
#define BUMPER_FOV_OLD 0   // сверка g10b-loop, СВЕРКА_КАМЕРА 1.19: 1 — у бампера угол кадра как за машиной (78° по горизонтали)
#endif
    // сверка g10b-loop, СВЕРКА_КАМЕРА 1.19: бамперная камера оригинала 90° по горизонтали (за машиной 78°); на нашем 16:9 это 58,7° по вертикали
    // (tan(45°) / (16/9)). Угол кадра за машиной и пешком (правка 650) не трогаем. BUMPER_FOV_OLD=1 — прежнее
    if (chaseView == 2 && !selectCycleOld && !onFoot() && !fly && !bpass && !inR && !mcPlay && !talkCamOn && !phoneCamOn && !zoneCamOn && !carZoneOn) curFovY = g10bBumperFovY(curFovY, ASPECT);
    if (!CHEATS1104_OLD && !CHEATCAM_OLD && gameCheatOn(CH_CAMERAS) && !onFoot() && !fly && !bpass && !inR && !mcPlay && !talkCamOn && !phoneCamOn && !zoneCamOn && !carZoneOn) {   // №1104: код Unlock All Cameras — следящая и бамперная камеры 92° по горизонтали; chase — свой угол по скорости камеры
      if (ccOn_) curFovY = ccFovYFromH(ccFovH, ASPECT);   // угол вида из списка кода (у chase и wreckless — меняется сам)
      else if (chaseView <= 2) curFovY = ccFovYDeg(ASPECT);
    }
    if (walkFovK != 1.f && onFoot() && !roomCamOn && !zoneCamOn && !talkCamOn && !mcPlay && !phoneCamOn && !fly && !bpass) curFovY = 2.f * atanf(tanf(curFovY * (GU_PI / 360.f)) * walkFovK) * (180.f / GU_PI);   // сверка g06 (g06-camera:34): автозум на персонажа миссии
    if (camBurstK > 1.001f && !onFoot() && chaseView != 2 && !carZoneOn && !fly && !bpass && !inR && !mcPlay && !talkCamOn && !phoneCamOn) curFovY = 2.f * atanf(tanf(curFovY * (GU_PI / 360.f)) * camBurstK) * (180.f / GU_PI);   // сверка g06 круг 2 (g01-cars 2.2): рывок со старта слегка расширяет кадр
    if (camBFovOn && !fly && !bpass) curFovY = camBFov + (curFovY - camBFov) * camBU;   // правки 166/167: угол кадра меняется вместе с переходом
    if (smkCamOn && smkCam[6] > 0.f) curFovY = smkCam[6];   // близнецы Б3: FOV из сценария
    { int aimCar_ = !CARAIM1160_OLD && !onFoot() && chaseView != 2 && !fly && !bpass && !inR && !mcPlay && !talkCamOn && !phoneCamOn && !zoneCamOn && !carZoneOn && !roomCamOn;   // №1160: сдвиг кадра Car Aim — только вид из-за машины (бампер, пешком, комнаты, особые камеры — по центру)
      camAimC = CARAIM1160_OLD ? 0.f : camUserEase(camAimC, aimCar_ ? camAimEff(camUserCarAim) : 0.f, CAMU_AIM_RATE, dt, dt <= 0.f); }   // плавно; под паузой (dt = 0) сразу
    if (trSpawnPend && smkOn) trSpawnLog();   // правка 328: рождённые в этот кадр — по камере этого кадра
    setupPlanes(cfx, cfy, cfz);
    smkMark(6); BBP(BBP_STREAM);   // отметка SLOWFRAME: до отсечения города
#ifndef PAUSE_DD_CAP_OLD
#define PAUSE_DD_CAP_OLD 0
#endif
#ifndef PAUSE_DD
#define PAUSE_DD 120.f   // TEST22: под меню паузы мир почти не виден (затемнён) — рисуем только 120 м; загрузка клеток (stream() выше) идёт от обычной дальности, регулятор на паузе не работает; PAUSE_DD_CAP_OLD=1 — прежнее
#endif
    if (!PAUSE_DD_CAP_OLD && uiReady && uiPaused() && drawDist > PAUSE_DD) drawDist = PAUSE_DD;
    unsigned p7m_ = 0;
    if (gagSnap >= 0 || inR || shopViewOn() || phoneViewOn() || wardrobeOn()) { nd = 0; pvsUsed = 0; stMissNow = 0; }   // пункт 6: экран покупки закрывает город целиком
    else if (pano917Cap) { hzCullD = drawDist; cullCity(fly); streamMiss(); }   // №917б: кадр бота — без регулятора дали
    else { unsigned p7a_ = smkOn ? sceKernelGetSystemTimeLow() : 0; float ddK_ = dalApply(dt, fly || bpass || !longView || (opt & O_D100)); hzCullD = drawDist; unsigned p7b_ = smkOn ? sceKernelGetSystemTimeLow() : 0; cullCity(fly); unsigned p7c_ = smkOn ? sceKernelGetSystemTimeLow() : 0; drawDist = ddK_; streamMiss();
           if (smkOn) { unsigned p7d_ = sceKernelGetSystemTimeLow(); p7S[0] += p7b_ - p7a_; p7S[1] += p7c_ - p7b_; p7S[2] += p7d_ - p7c_; p7m_ = p7d_; } }   // №877: отсечение города до нужной дали (таблица бота), туман и прочее — от прежней
    if (!fly && !bpass && !(uiReady && uiPaused())) {
      gameRespawn(&G, ex, ez, dt, respawnSeen, 0);
    }

    if(haveColl && !bpass){
      if(gagSnap>=0)for(unsigned g=0;g<coll.ngag;g++)coll.st.gagPick[g]=g==(unsigned)gagSnap;
      unsigned t=sceKernelGetSystemTimeLow(),before=collStreamTrackBytes+gagSkinResidentBytes;
      if(collStreamPrepare(&coll,inR?collectRoomId(roomName()):-1,ex,ez,inR?80.f:drawDist*.92f,gagSnap))smkWrite("COLL stream error=prepare\n");
      unsigned now=collStreamTrackBytes+gagSkinResidentBytes;
      if(smkOn && before!=now){char b[240];snprintf(b,sizeof b,"GAGSTREAM resident=%u tracks=%u skins=%u ms=%.2f t=%.1f async=%u/%u defer=%u\n",collStreamResidentBytes,collStreamTrackBytes,gagSkinResidentBytes,(sceKernelGetSystemTimeLow()-t)/1000.f,smkT,collStreamAsyncDone,collStreamAsyncStarts,collStreamAsyncDefer);smkWrite(b);}
    }
    { unsigned p7e_ = smkOn ? sceKernelGetSystemTimeLow() : 0; cullCityEnd();   // №653: ответ ME по отсечению города (или запасной путь на главном)
      if (smkOn) { unsigned p7f_ = sceKernelGetSystemTimeLow(); p7S[3] += p7f_ - p7e_; if (p7m_) p7S[4] += p7e_ - p7m_; p7S[5]++;
        if (p7S[5] >= 30) { char l_[160]; snprintf(l_, sizeof l_, "PH7SPLIT t=%.1f frames=%u dal_us=%u cull_us=%u miss_us=%u end_us=%u coll_us=%u (мкс на кадр)\n", smkT, (unsigned)p7S[5], (unsigned)(p7S[0] / p7S[5]), (unsigned)(p7S[1] / p7S[5]), (unsigned)(p7S[2] / p7S[5]), (unsigned)(p7S[3] / p7S[5]), (unsigned)(p7S[4] / p7S[5])); smkWrite(l_); memset(p7S, 0, sizeof p7S); } } }
    smkMark(7); smkLogicNow = (sceKernelGetSystemTimeLow() - t0) / 1000.f;   // логика кадра до сборки списка GE (строка SLOWFRAME)
    int rfPass = 0, rfSavedBatchOld=ggBatchOld, rfSavedOld = rfOld, rfSavedHud = hud; float rfSavedDt = dt;
    float rfEye[6] = {ex, ey, ez, cfx, cfy, cfz}; unsigned rfHash = 0, rfRenderStart = 0;
    if (rfCapture && !rfFreezeTake()) rfCapture = 0;
    if (rfCapture) { dt = 0; hud = 0; rfFreezeDump(rfEye, frames); }
rf_repeat_draw:
    if (rfCapture) {
      static const int masks[5] = {7, 6, 5, 3, 7};
      rfFreezeRestore(); ex = rfEye[0]; ey = rfEye[1]; ez = rfEye[2]; cfx = rfEye[3]; cfy = rfEye[4]; cfz = rfEye[5];
      if (rfBreak && rfPass == 2) ex += .5f; // real camera fault: harness must reject this input
      float current[6] = {ex,ey,ez,cfx,cfy,cfz}; rfHash = rfFreezeHash(current);
      ggBatchOld=ggBatchCapture?(rfPass%2==0):rfSavedBatchOld;
      if (stripsCapture) stripsAsList = (rfPass % 2) ? ((stripsCapture == 2 && rfPass == 3) ? 2 : 1) : 0;
      rfOld = (ggBatchCapture || stripsCapture) ? 7 : skyHorizonRfPair ? rfSavedOld : masks[rfPass]; rfSpPose = rfSpCut = rfDlSkip = rfGgList = rfGgListF = rfSpUs = 0; spSmallCut = 0;
      rfRenderStart = sceKernelGetSystemTimeLow();
      if (skyHorizonRfPair) { int old_ = skyHorizonCacheOld; skyHorizonCacheOld = rfPass == 4; skyFogColor(); skyHorizonCacheOld = old_;
        char s_[128]; snprintf(s_,sizeof s_,"HZRF level=%d pass=%d color=%06x us=%u calc=%u hit=%u\n",gameLevel,rfPass,fogCol,skyHorizonUs,skyHorizonCalculations,skyHorizonCacheHits);smkWrite(s_); }
    }
    uiFrDraws = uiFrVerts = uiFrAreaPx = uiFrMaxVerts = uiFrMaxAreaPx = 0;   // №1186: кадр проверки вызовов интерфейса
    if (pendTalkOut) { pendTalkOut = 0;   // №1186: ирис конца разговора — на границе кадра, список кадра ещё не открыт
      if (transitReady()) { frames += transitClose(&fbp0, "talk_out") / (longView ? 2 : 1); chase.init = 0; smkSettle(); } }
    DCALL96();   // dcacheall96: диагностика кэша — всё, что процессор записал за кадр, в ОЗУ до списка кадра
    sceGuStart(GU_DIRECT, list); geListOpen = 1; guRestate(); BBP(BBP_CITY); CSM0();
    lstSumB += lstBinds; lstFrames++; lstBinds = 0; lstImm += (unsigned)binds; lstFlatImm += flatBinds; flatBinds = 0;   // №912: смены текстуры/заливки за прошлый кадр (листы и привязки на месте)
    resetState(); drawnTris = 0; draws = 0; binds = 0; ggVbUsed = 0; carFanN = 0; cltFrame++; cltBuiltF = 0; tsFrame(); geVbBegin();
    if (opt & O_QVP) { sceGuViewport(2048, 2048, SCR_W / 2, SCR_H / 2); sceGuScissor(SCR_W / 4, SCR_H / 4, SCR_W * 3 / 4, SCR_H * 3 / 4); }
    else { sceGuViewport(2048, 2048, SCR_W, SCR_H); sceGuScissor(0, 0, SCR_W, SCR_H); }
    sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuShadeModel(GU_SMOOTH);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvlMode = 0;
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    fogBase();   // цвет горизонта неба (skyFogColor), не яркий голубой
    sceGuClearColor(inR ? rg.clear : clearChk ? 0xffff00ff : 0xff000000 | fogCol); sceGuClearDepth(0);   // комната: фон из её пакета
    // шаг 1 аудита: небо рисуется после непрозрачного и закрывает всё, что осталось пустым, — цвет не чистим
    // (кроме полёта: снизу/с края уровня неба может не быть). clearchk — чистим пурпурным и считаем дыры (bench)
    int skyLast = (opt & O_SKYLAST) && (opt & O_SKY);
    if (trLeft == TR_FRAMES) { char t_[200]; int l_ = snprintf(t_, sizeof t_, "TRC frame inR=%d room=%d rmem=%p npc=%u fov=%.2f near=%.3f eye=%.1f,%.1f,%.1f dir=%.2f,%.2f,%.2f list=%d\n",
                                 inR, rs.room, (void *)rg.mem, rg.npc, curFovY, frameNear(inR, fly || bpass), ex, ey, ez, cfx, cfy, cfz, (int)sceGuCheckList()); trLine(t_, l_); }
    // Правка Роба 1249 (08.11, тур 97 на PSP: вдалеке шлейф/фейд, «дальние пиксели тянутся, как незатёртый прошлый кадр»): цвет кадра не чистился (небо рисуется после города и «закрывает всё пустое»), но небо покрывает не всё:
    // куски неба вне клеток/срезаны (SKYDRAW notres/culled), полоса между краем города (до drawDist, теперь 250 м) и небом/силуэтами, дыры в дальних листах - там оставался прошлый кадр. Теперь цвет чистится всегда цветом горизонта
    // (fogCol уже выставлен выше), небо и город ложатся поверх: непокрытое = цвет дали, а не хвост. Цена - заливка кадра 480 x 272 (≈ 0,1-0,2 мс GE). SKYCLEAR1249_OLD=1 / файл skyclear1249old - как было
    sceGuClear((skyLast && !fly && !clearChk && !inR && skyClear1249Off ? 0 : GU_COLOR_BUFFER_BIT) | GU_DEPTH_BUFFER_BIT);
    // Брифинг миссии (разбор тура 12): мир стоит (dt = 0), карточка с руками №512 поверх; прежде мир собирался и рисовался заново каждый кадр (кадр 36–51 мс, 15–19 к/с, руки рывками).
    // Теперь на третьем кадре брифинга, когда подгрузка кончилась и ирис не закрывает мир, кадр мира (до интерфейса и ириса) копируется в снимок в VRAM (область истории джиттера,
    // 272 КБ: в брифинге джиттер не работает), следующие кадры показывают снимок и рисуют только карточку (goto bf_world_done). Камера сдвинулась, закрылся экран брифинга,
    // открылся магазин/будка/комната — снимок сбрасывается и мир рисуется как прежде. Только кадр 5650. BRIEF_FREEZE_OLD=1 — как до правки
    int bfBase_ = !BRIEF_FREEZE_OLD && !bpass && !inR && uiReady && mvBriefing() && !uiPaused() && !shopViewOn() && !phoneViewOn() && !wardrobeOn() && gagSnap < 0 && !mcPlay && !rfCapture;
    int bfSnapOk_ = bfBase_ && fb16 && !JITTER_HIST_OLD && !transitBusy();
    if (bfSnapOk_) bfFrames++; else { bfFrames = 0; if (bfSnap) { bfSnap = 0; jitHave = 0; } }
    float bfDp_ = bfSnap ? fabsf(ex - bfPose[0]) + fabsf(ey - bfPose[1]) + fabsf(ez - bfPose[2]) + fabsf(cfx - bfPose[3]) + fabsf(cfy - bfPose[4]) + fabsf(cfz - bfPose[5]) : 0.f;
    int bfMode_ = bfDecide(bfBase_, bfBase_ && transitFullyClosed(), bfSnapOk_, bfSnap, bfDp_, bfFrames, pending);
    if (bfSnap && bfMode_ != BF_BLIT && bfDp_ > .01f) { bfSnap = 0; bfFrames = 1; }   // камера сдвинулась: снимок устарел
    if (bfMode_ == BF_HIDE || bfMode_ == BF_BLIT) briefWarm(ex, ey, ez, inR);   // мир стоит и не рисуется: готовим листы клеток и запекание к снятию ириса
    // №626 (TEST18, старт гонки Лизы: p95 44 мс, хвост 4 с после ириса — холодные кэши x_city 62, x_npc 24, x_car3d 14 мс и первые списки GE): пока брифинг под газетой LOADING и ирис закрыт,
    // каждый PREWARM_EVERY-й кадр мир рисуется по-настоящему (его закрывает чёрный ирис, карточки ещё нет), остальные кадры — листы клеток briefWarm. Тяжёлый первый кадр
    // (листы клеток вокруг точки старта, первая отрисовка моделей NPC и машин гонки) падает под газету, а не в первую секунду после Continue. START_PREWARM_OLD=1 — как до правки
#ifndef START_PREWARM_OLD
#define START_PREWARM_OLD 0
#endif
#define PREWARM_EVERY 4
    static unsigned pwTick; int pwDraw_ = 0;
    if (bfMode_ == BF_HIDE && !START_PREWARM_OLD && mvBriefLoading() && (pwTick++ % PREWARM_EVERY) == 0) { pwDraw_ = 1; pwFrames++; if (smkOn) { char l_[64]; snprintf(l_, sizeof l_, "PREWARM world n=%u t=%.2f\n", pwFrames, smkT); smkWrite(l_); } }
    if (bfMode_ == BF_HIDE && !pwDraw_) { bfHides++; resetState(); goto bf_world_done; }   // ирис закрыт целиком (так идёт брифинг, замер тура 12 «phase=hold w=0»): мир под ним не виден — не рисуем вовсе
    if (bfMode_ == BF_BLIT) { bfCopy(JIT_HIST_OFF, (unsigned)fbp0); bfBlits++; resetState(); goto bf_world_done; }
    sceGuDisable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); curFlags = -1;   // правка 287: интерфейс прошлого кадра ставил порог 0 — 3D с прежним порогом 0x40
    if (trLeft == TR_FRAMES) { char t_[48]; int l_ = snprintf(t_, sizeof t_, "TRC clear list=%d\n", (int)sceGuCheckList()); trLine(t_, l_); }
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    float gFov = curFovY; ScePspFVector3 up = {0, 1, 0};
    // №1084 (Роб, TEST82: «внутри комнат начался крен камеры»): вертикаль кадра, выставленная камерой пешком (крен уровня 7), не накладывается на камеры, которые ведут кадр сами
    // (камера комнаты и зоны, разговор, ролик миссии, телефон, полёт, проход). Вертикаль бампера (в машине) как была. ROLL1084_OLD=1 — как в TEST82
    if (camUpOn && (ROLL1084_OLD || !onFoot() || !(roomCamOn || zoneCamOn || talkCamOn || mcPlay || phoneCamOn || fly || bpass))) { up.x = camUpV[0]; up.y = camUpV[1]; up.z = camUpV[2]; }
    camUpOn = 0;   // сверка g06 (g06-camera:18): вид от бампера — вертикаль кадра = вертикаль кузова (main_camera.h)
    if (!camGuardOff) camGuard(&gFov, &ex, &ey, &ez, &cfx, &cfy, &cfz, &up);   // правки 259/252: в GE — только конечные матрицы
#ifndef CAM_YAWSLACK872_OLD
#define CAM_YAWSLACK872_OLD 0
#endif
#define CAM_YAW_HD 1.5f     // м: видимый глаз ближе по горизонтали (поднят над головой) — курс не ограничиваем, крутится свободно, как раньше
#define CAM_YAW_SLACK .35f   // рад (~20°): насколько желаемый курс камеры может уйти дальше видимого, пока камера упирается
    // №872 (Роб 23.10, TEST47: «камера упирается — хорошо, но иногда пытается пройти и зависает, L/R не работает; упёрлась — пусть стоит, но обратно крутить»):
    // L/R крутит желаемый курс (homer.camYaw) без предела, а защита глаза (camsafe) держит видимую камеру у препятствия — курс убегал, и обратный
    // поворот сначала «отматывал» набежавшее. Пока L/R зажаты и желаемый глаз занят (csDesiredOk, camsafe), курс не дальше видимого (от глаза к герою) больше чем на CAM_YAW_SLACK. CAM_YAWSLACK872_OLD=1 — как было
    if (!CAM_YAWSLACK872_OLD && haveHomer && onFoot() && camFollow && !inR && !fly && !bpass && !smkCamOn && homer.camTurn != 0.f && !csDesiredOk) {
      float hx_ = homer.x - ex, hz_ = homer.z - ez;   // упёрлась (желаемый глаз занят) и стоит нормально — сзади-сбоку, не над головой
      if (hx_ * hx_ + hz_ * hz_ > CAM_YAW_HD * CAM_YAW_HD) { float yv_ = atan2f(hx_, hz_), d_ = homer.camYaw - yv_;
        while (d_ > GU_PI) d_ -= 2.f * GU_PI; while (d_ < -GU_PI) d_ += 2.f * GU_PI;
        if (d_ > CAM_YAW_SLACK) homer.camYaw = yv_ + CAM_YAW_SLACK; else if (d_ < -CAM_YAW_SLACK) homer.camYaw = yv_ - CAM_YAW_SLACK;
        while (homer.camYaw > GU_PI) homer.camYaw -= 2.f * GU_PI; while (homer.camYaw < -GU_PI) homer.camYaw += 2.f * GU_PI; } }
    CSM(0); { CSUBT0; floorNearFrame(inR, fly || bpass, ex, ey, ez, cfx, cfy, cfz, gFov); CSUB(5); }   // №861: ближняя плоскость по полу у низа кадра
    { CSUBT0; camSafeFrame(inR, fly || bpass, camAimWideFov(gFov, camAimC), ex, ey, ez, cfx, cfy, cfz, dt); CSUB(6); }   // №1160: по расширенному кадру Car Aim (при c = 0 тот же угол)
    CSM(1);   // правка 331: ближняя плоскость по свободному месту; замер
    if (smkOn && camLogOn && haveTraffic) {   // №1232: служебное — глаз камеры и ближайшая машина трафика (для замера дрожи)
            float dm_ = 1e9f, vm_ = 0.f; int mv_ = 0, ck_ = -1;
            for (int i_ = 0; i_ < traffic.maxCars; i_++) if (traffic.car[i_].mode != TRM_OFF) { const Car *k_ = &traffic.car[i_].car; float d_ = hypotf(k_->x - ex, k_->z - ez); if (d_ < dm_) { dm_ = d_; float v_[3]; carAnchorVelocity(k_, v_); vm_ = hypotf(v_[0], v_[2]); mv_ = 1; ck_ = i_; } }
            if (dm_ < 14.f) { char s_[220]; snprintf(s_, sizeof s_, "CAMTRF t=%.2f eye=%.3f,%.3f,%.3f look=%.3f,%.3f,%.3f home=%.2f,%.2f dmin=%.2f v=%.1f car=%d cp=%.2f,%.2f\n", smkT, ex, ey, ez, ex + cfx, ey + cfy, ez + cfz, homer.x, homer.z, dm_, vm_, ck_, ck_ >= 0 ? traffic.car[ck_].car.x : 0.f, ck_ >= 0 ? traffic.car[ck_].car.z : 0.f); smkWrite(s_); } (void)mv_; }
    { float l_ = sqrtf(cfx * cfx + cfy * cfy + cfz * cfz); if (l_ > 1e-6f) { camFwdN[0] = cfx / l_; camFwdN[1] = cfy / l_; camFwdN[2] = cfz / l_; } }   // clipguard.h: взгляд кадра для правила «по взгляду ближе 1,8 м»
    { float zn_ = frameNear(inR, fly || bpass); camNearNow = (camFollow && !inR && !fly && !bpass && !talkCamOn && haveCol) ? zn_ : 0.f; }   // №502: порог скрытия привязан к ближней плоскости только у следящей камеры (кинокамеры разговора/зон/комнат подобраны данными)
    // B2 (план оптимизации, 10.10): пешком у своей машины/остова ближняя плоскость падает до 0,35 только из-за машины (frameNear, правка 113), и город режется на процессоре (ggOn:
    // L7 после взрыва ct_risky 3,7 + gg 3,1 мс). Если мир ближней плоскости не требует (frameNearEx без машины >= NEAR_Z) и причина только своя/оставленная машина, город и всё
    // остальное идёт с NEAR_Z, а куски своих машин ближе 1,5 м дорисовываются вторым проходом в конце 3D (глубина очищена, ближняя 0,35, дальняя 1,55: всё в этом слое ближе
    // любого пикселя первого прохода по построению). Трафик, машины миссий и SetForcedCar у глаза — прежний путь. -DNEARPASS_OLD=1 — как до B2
#ifndef NEARPASS_OLD
#define NEARPASS_OLD BIS(5)
#endif
// Роб 17.10 (снимок: «вижу внутренности своей машины», пешком мимо своей машины): во втором проходе глубина чистилась в «дальше всего» (0),
// а части машины дальше 1,5 м GE не режет, а прижимает к дальней плоскости — все они получали глубину 0 и проходили GEQUAL, кто нарисован позже,
// тот и сверху: салон вылезал сквозь кузов. Теперь глубина чистится в NEARPASS_CLR: прижатые части (0) не проходят, во втором проходе — только то,
// что ближе 1,5 м, а дальнее остаётся из первого прохода. flag nearpassclrold 1 — как было
#define NEARPASS_CLR 16
    int nearPass = 0; float znFull_ = frameNear(inR, fly || bpass);
    if (!NEARPASS_OLD && !inR && !fly && !bpass && onFoot() && haveCarVis && haveCol && !lumaKey && !mvForcedCar() && !uiPaused() && znFull_ < NEAR_Z * .999f) {
      float zw_ = frameNearEx(inR, fly || bpass, 0); frameNear(inR, fly || bpass);   // второй вызов возвращает причину (frameNearWhy) полного
      float own_, oth_; carNearEyeSplit(&own_, &oth_);
      if (zw_ >= NEAR_Z * .999f && oth_ * .8f >= NEAR_Z * .999f && own_ * .8f < NEAR_Z * .999f) { nearPass = 1; smkNearPass++; }
    }
    float znMain_ = nearPass ? NEAR_Z : znFull_;
    zfFov = (!inR && !bpass) ? gFov : 0.f; zfNear = znMain_;   // №932б: дальний проход города только в городе
    if (pano917Cap) znMain_ = pano917Near;   // №917б: ближняя плоскость = граница панорамы
    sceGumPerspective(gFov, ASPECT, znMain_, inR ? ROOM_FAR : drawDist + 10.f); camAimGum();   // №1160: сдвиг кадра (Car Aim) — до jitterProj, он берёт матрицу из gum   // Ф2: ближняя плоскость NEAR_Z (была 0.7); в комнате ROOM_NEAR; пешком у кузова/стены — ближе (homerCamera)
    if (!bpass && !inR && !uiPaused() && !pano917Cap) jitterProj((float[3]){ex, ey, ez}, (float[3]){cfx, cfy, cfz}, gFov, znMain_, drawDist + 10.f); else { jitHave = 0; jitActive = 0; }   // эксперимент jitter.h: ±¼ пикселя, только в городе
    { float zn = znMain_;   // правка 340: кадр, где GE выбросил бы треугольник у края (низкая ближняя плоскость); B2: при втором проходе у города NEAR_Z
      ggOn = !inR && (ggMode == 2 || (ggMode == 0 && zn < NEAR_Z * .999f));
      { unsigned rw0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      camZn = znMain_; camWall05 = !CAM_CLIP_CITY_OLD && !inR && camRealWall(CAM_CLIP_R);
      if (smkOn) { static unsigned rwUs_, rwN_, rwT0_; rwUs_ += sceKernelGetSystemTimeLow() - rw0_; csubT[7] += sceKernelGetSystemTimeLow() - rw0_; rwN_++; unsigned rn_ = sceKernelGetSystemTimeLow(); if (!rwT0_) rwT0_ = rn_;
        if (rn_ - rwT0_ >= 1000000u) { char l_[96]; snprintf(l_, sizeof l_, "CAMRW t=%.1f n=%u us_total=%u cache=%d\n", smkT, rwN_, rwUs_, !CAM_WALL_CACHE_OLD); smkWrite(l_); rwUs_ = rwN_ = 0; rwT0_ = rn_; } } }
      if (!ggOn && !inR && ggMode == 0 && !CAM_GG_NEAR_OLD && ggNearOn && ggNearAny()) { ggOn = 1; ggNearFrames++; }
      if (smkOn && nearLog) { static float nlT_; if (smkT - nlT_ >= .1f || smkT < nlT_) { nlT_ = smkT; char l_[360]; int nsp_ = 0; float spd_ = 99.f, spc_[3] = {0, 0, 0}, sph_[3] = {0, 0, 0}; if (haveProps) for (unsigned i_ = 0; i_ < props.ninst; i_++) { const PropState *st_ = &props.st[i_]; float d_ = hypotf(st_->x - ex, st_->z - ez); if (d_ < spd_) { spd_ = d_; spc_[0] = st_->x; spc_[1] = st_->y; spc_[2] = st_->z; sph_[0] = st_->hx; sph_[1] = st_->hy + 10.f * st_->hidden; sph_[2] = st_->hz; } } for (int i_ = 0; i_ < csScene.nbox; i_++) if (csScene.box[i_].own == 2) { const CsBox *B_ = &csScene.box[i_]; nsp_++; float d_ = hypotf(B_->x - ex, B_->z - ez); if (d_ < spd_) { spd_ = d_; spc_[0] = B_->x; spc_[1] = B_->y; spc_[2] = B_->z; sph_[0] = B_->hw; sph_[1] = B_->hl; sph_[2] = B_->hi; } } snprintf(l_, sizeof l_, "CAMNEAR nbox=%d nsp=%d spd=%.2f spc=%.1f,%.1f,%.1f sph=%.2f,%.2f,%.2f t=%.2f sc=%s fol=%d nv=%.2f safe=%.2f hit=%d same=%d zn=%.2f why=%d gg=%d eye=%.2f,%.2f,%.2f f=%.2f,%.2f,%.2f\n", csScene.nbox, nsp_, spd_, spc_[0], spc_[1], spc_[2], sph_[0], sph_[1], sph_[2], smkT, smkScene, camFollow, csDbgNv, camSafeNearV, csDbgHit, csDbgSame, zn, frameNearWhy, ggOn, ex, ey, ez, camFwdN[0], camFwdN[1], camFwdN[2]); smkWrite(l_); } }
      if (!ggOn && !inR && ggMode == 0 && ggLowH > 0.f && haveCol && !bpass) { float gy_, n_[3];   // №674: глаз низко над землёй — ближняя плоскость пересекает длинные полосы дороги (ребро > 13 м), вершина за ±2048 px: GE выбрасывает треугольник / рисует клин
        if (colGround(&world, ex, ez, ey + .5f, ey - 30.f, &gy_, n_) && ey - gy_ < ggLowH) { ggOn = 1; ggLowFrames++; } }
#ifndef GG_HILL848_OLD
#define GG_HILL848_OLD 0
#endif
      ggBigOnly = 0;   // №840: крупные треугольники у камеры — режутся, когда глаз ниже GG_BIG_H над землёй (выход из машины 1,7–2,8 м: 35 треугольников газона под выбросом; пешком 3,8 м — 0)
      if (!ggOn && !inR && ggMode == 0 && !GG_BIG840_OLD && !bpass && cellBigF && haveCol) { float gy_, n_[3];
        // №848 (Роб 19.10, TEST39: «L3, на холме бегу — пол опять проламывается»): на склоне земля под глазом ниже, чем впереди, — просвет меряется
        // и в 3 м и 6 м по взгляду (наибольшая земля из трёх точек). GG_HILL848_OLD=1 — только под глазом, как №840
        int low_ = !colGround(&world, ex, ez, ey + .5f, ey - 30.f, &gy_, n_) || ey - gy_ < GG_BIG_H;
        if (!low_ && !GG_HILL848_OLD) { float fl_ = sqrtf(cfx * cfx + cfz * cfz); if (fl_ > 1e-3f) { float fx_ = cfx / fl_, fz_ = cfz / fl_;
            for (int k_ = 1; k_ <= 2 && !low_; k_++) { float px_ = ex + fx_ * 3.f * (float)k_, pz_ = ez + fz_ * 3.f * (float)k_;
              if (colGround(&world, px_, pz_, ey + 3.f, ey - 30.f, &gy_, n_) && ey - gy_ < GG_BIG_H) low_ = 1; } } }
        if (low_) { ggOn = 1; ggBigOnly = 1; ggBigFrames++; } }
      if (smkOn) { static unsigned t0g_, nf_, non_; static float zmin_ = 1e9f; nf_++; non_ += ggOn; if (zn < zmin_) zmin_ = zn; unsigned n_ = sceKernelGetSystemTimeLow(); if (!t0g_) t0g_ = n_;
        if (n_ - t0g_ >= 1000000u) { char l_[140]; snprintf(l_, sizeof l_, "GGSTAT t=%.1f frames=%u ggOn=%u znmin=%.2f NEAR_Z=%.2f mode=%d vbsync=%u opqfar=%u/%u\n", smkT, nf_, non_, zmin_, NEAR_Z, ggMode, ggVbSyncN, opqFarUse, opqFarNear); smkWrite(l_); ggVbSyncN = 0; opqFarUse = opqFarNear = 0; t0g_ = n_; nf_ = non_ = 0; zmin_ = 1e9f; } }   // №634: срез у камеры и при обычной ближней плоскости, если кусок города у самого глаза
      if (smkOn && !inR) { smkNearFrames++; if (zn < NEAR_Z * .999f) { smkNearLow++; int why_ = frameNearWhy; float zw_ = frameNearEx(inR, fly || bpass, 0); if (zw_ >= NEAR_Z * .999f) smkNearCarOnly++;
          frameNearWhy = why_; smkNearWhy[why_ < 7 ? why_ : 0]++; } }   // B2: из-за чего ближняя плоскость ниже NEAR_Z (PHSUB near_*)
      float fl_ = sqrtf(cfx * cfx + cfy * cfy + cfz * cfz); ggF[0] = cfx / fl_; ggF[1] = cfy / fl_; ggF[2] = cfz / fl_;
      ggR[0] = ggF[1] * up.z - ggF[2] * up.y; ggR[1] = ggF[2] * up.x - ggF[0] * up.z; ggR[2] = ggF[0] * up.y - ggF[1] * up.x;   // f × up, как sceGumLookAt
      float rl_ = sqrtf(ggR[0] * ggR[0] + ggR[1] * ggR[1] + ggR[2] * ggR[2]); if (rl_ < 1e-6f) { ggR[0] = 1.f; ggR[1] = ggR[2] = 0.f; rl_ = 1.f; }
      ggR[0] /= rl_; ggR[1] /= rl_; ggR[2] /= rl_;
      ggU[0] = ggR[1] * ggF[2] - ggR[2] * ggF[1]; ggU[1] = ggR[2] * ggF[0] - ggR[0] * ggF[2]; ggU[2] = ggR[0] * ggF[1] - ggR[1] * ggF[0];
      ggE[0] = ex; ggE[1] = ey; ggE[2] = ez;   bigCutBlock = 0;
      ggK.tvy = tanf(gFov * (GU_PI / 360.f)); ggK.tvx = ggK.tvy * ASPECT; ggK.zn = zn;
      ggCamUpdate();   // №500: стоящая камера — срез города из прошлого кадра (main_render.h)
      if (ggOn) { ggFramesOn++; if (zn < ggNearMin) ggNearMin = zn; }
      ggTrisAll += (unsigned)ggTrisF; ggPiecesAll += (unsigned)ggPiecesF; ggCellsAll += (unsigned)ggCellsF; ggExamAll += (unsigned)ggExamF;
      if (smkOn) { smkGgMs += ggUsF / 1000.0; smkGgPieces += ggPiecesF; smkGgTris += ggExamF; } ggUsAll += ggUsF; if (ggUsF > ggUsMax) ggUsMax = ggUsF; ggTrisF = ggPiecesF = ggCellsF = ggExamF = 0; ggUsF = 0;
      rfGgList += rfGgListF; rfGgListF = 0; rfFrames++; }   // render_fast О11: клеток у камеры, ушедших листом (RFAST gg_list)
    CSM(2);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    if (farProbe == 4 && !inR) { float l_ = sqrtf(cfx * cfx + cfz * cfz); if (l_ > 1e-4f) { cfx /= l_; cfz /= l_; cfy = 0.f; } }   // правка 411, служебное: взгляд по горизонту (строка 136 = горизонт)
    ScePspFVector3 eye = {ex, ey, ez}, at = {ex + cfx, ey + cfy, ez + cfz};
    sceGumLookAt(&eye, &at, &up);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
    sceGumUpdateMatrix();
    if (trLeft == TR_FRAMES) {   // след 259: матрицы кадра, ушедшие в GE, — конечные ли (гейт tests/rooms/msn_from_room.sh)
      ScePspFMatrix4 pm_, vm_; sceGumMatrixMode(GU_PROJECTION); sceGumStoreMatrix(&pm_); sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vm_);
      sceGumMatrixMode(GU_MODEL); char t_[128];
      int l_ = snprintf(t_, sizeof t_, "TRC cam fov=%.2f up=%.0f,%.0f,%.0f proj=%d view=%d gg=%u\n", gFov, up.x, up.y, up.z, geMatOk(&pm_.x.x, 16), geMatOk(&vm_.x.x, 16), geGuardHits);
      trLine(t_, l_); }

    // небо: вокруг камеры, без тумана и без записи глубины (слои в порядке файла); сектора за кадром не рисуем
    CSM(3);
    if ((opt & O_SKY) && !skyLast && !inR && !noSkyChk) drawSky(ex, ey, ez, 0);
    CSM(4);
    if(!inR && !clearChk){groundCoverDraw(gFov,ey,onFoot()&&haveHomer?homer.y:car.y);resetState();fogBase();}   // замер щелей (clearchk/seamchk): земля №437 не красит щели под горизонтом (только инструмент, игру не меняет)
    CSM(5);
    if (inR || (!groundCoverFog(&groundCover) && !fogCityOld && farProbe != 1)) sceGuDisable(GU_FOG); else sceGuEnable(GU_FOG);   // №437: far fog meets the ground underlay
    // полупрозрачное города, машины, пропов и монет — в очередь, рисуется после них (tqFlush ниже)
    ntq = 0; tqOn = (opt & O_LEAN) != 0; tqEx = ex; tqEy = ey; tqEz = ez;
    skinToonEye(ex, ey, ez);   // №754: контур персонажей — толщина по расстоянию до глаза
    // город: отсортированный список
    if (opt & O_LEAN) {
      // правки Роба 224/237/251 (швы мигают): город — в осях камеры. GE считает вершины во float24 (мантисса 15 бит), город в
      // 500–900 м от начала: матрица куска с переносом ~700 м и вид с переносом ~-700 м давали ошибку до ~2 см на каждую вершину,
      // у каждого куска свою — вдоль стыков мигали точки фона (модель GE tools/seams_ge.py). Вид без переноса (глаз в начале),
      // перенос куска = центр − глаз (float32 процессора): ошибка — доля расстояния до камеры. Полупрозрачное — в очередь
      // (tqFlush рисует его позже с обычным видом) — с прежней матрицей. Служебный файл seamsold — как до правки (красный контроль)
      ScePspFMatrix4 vAbs; int rel = !seamsOld;
      winCityRel = rel;   // №1125: дальний проход окон (viewer/winspr.h) знает, каким видом рисует город
      if (rel) { sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vAbs); sceGumMatrixMode(GU_MODEL);
        ScePspFMatrix4 vRel = vAbs; vRel.w.x = vRel.w.y = vRel.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vRel); }
      if (trLeft == TR_FRAMES) { char t_[128]; int l_ = snprintf(t_, sizeof t_, "TRC city nd=%d eye=%.1f,%.1f,%.1f list=%d rel=%d\n", nd, ex, ey, ez, (int)sceGuCheckList(), rel); trLine(t_, l_); }
      if (rel && farProbe < 3 && !pano917Cap) { horizonDraw(ex, ey, ez); hlodDraw(ex, ey, ez, 0); pano917Draw(ex, ey, ez); }   // №917: кольцо-панорама поверх ленты, под городом   // Горизонт: плоский контур дальнего города, самым первым (horizon.h), выключено по умолчанию
      if (rel && farProbe < 3) road934Draw(ex, ey, ez);   // №934: дальние дороги и мосты до 520 м (road934.h), после ленты горизонта и HLOD, до силуэтов и города
      if (rel && farProbe < 3) silhDraw(ex, ey, ez);   // Загрузка 3: силуэты за дальностью (silh.h), до подложки и города, выключено по умолчанию
      if (rel && farProbe < 3) cellUnderlayDraw(ex, ey, ez);   // правка 509: подложка по клеткам до города (камерные оси; seamsold — без неё)
      CSM(6);
      unsigned dlT0 = sceKernelGetSystemTimeLow();
      if (farProbe >= 3) ;   // правка 411, служебное: без города — что за ним (небо); 4 — и взгляд по горизонту
      else if (rel && !dlOff && !seamChk && !(trLeft == TR_FRAMES && trDetail) && cdl) dlCity(ex, ey, ez);   // правка 275: листы клеток
      else for (int i = 0; i < nd; i++) { if (sf[i] && geOver(GE_SOFT)) continue;   // правка 103: дальние куски — первыми из кадра
        Piece *d = &pc[sp[i]];
        if (trLeft == TR_FRAMES && trDetail) {   // след 259: первый кадр камеры миссии — каждый кусок до отправки на GE
          const Tex *x_ = texAt(d->r.tex); char t_[200];
          int l_ = snprintf(t_, sizeof t_, "TRP i=%d p=%d cell=%u st=%d far=%d fl=%x nv=%d ni=%d v=%p idx=%p fidx=%p tex=%d pal=%p lv0=%p slv0=%p list=%d\n",
                            i, sp[i], d->r.cell, cl[d->r.cell].state, sf[i], d->r.flags, d->r.nv, d->r.ni, (void *)d->v, (void *)d->idx, (void *)d->fidx,
                            d->r.tex, x_ ? (void *)x_->pal : 0, x_ ? (void *)x_->lv[0] : 0, x_ ? (void *)x_->slv[0] : 0, (int)sceGuCheckList());
          trLine(t_, l_); }
        if (seamChk && (d->r.flags & 3)) continue;   // служебное seamchk: щели меряются по непрозрачному городу
        fogRadial(d, sf[i]);
        if (rel && !(tqOn && (d->r.flags & 2) && !(d->r.flags & 4))) drawPiece(d, sf[i], -ex, -ey, -ez, 1, 0);
        else drawPiece(d, sf[i], 0, 0, 0, 1, 0); }
      if (rel && farProbe < 3) hlodDraw(ex, ey, ez, 1);   // №704: вариант «после города» (flag hlodafter 1)
      if (rel) sceGuSetMatrix(GU_VIEW, &vAbs);
      { static unsigned acc, nfr; acc += sceKernelGetSystemTimeLow() - dlT0; if (++nfr >= 60) {   // правка 275: подача города, мс процессора
          if (smkOn) { char t_[160]; snprintf(t_, sizeof t_, "CITYDL ms=%.3f dl=%d nd=%d calls=%u fall=%u built=%u kb=%u ph=%u fogold=%d t=%.1f\n", acc / 1000.f / nfr,
                                               !dlOff, nd, dlCallsFrame, dlFallFrame, dlBuilt, dlBytesNow / 1024u, phDraws, fogCityOld, smkT); smkWrite(t_); phDraws = 0;
            if (skinToonEnabled()) { snprintf(t_, sizeof t_, "TOON mode=%d skin=%u car=%u verts=%u kb=%u seams=%u per60 t=%.1f\n", skinToon, skinToonDraws, carToonDraws, skinToonVerts, skinToonBytesNow / 1024u, skinToonSeamFixed, smkT); smkWrite(t_); }
            snprintf(t_, sizeof t_, "INK1062 world=%u hull=%u\n", worldToonDraws, hullToonCalls); smkWrite(t_); worldToonDraws = hullToonCalls = 0;   // №1062: оболочки мелких кусков города за 60 кадров (в новом 0)
            snprintf(t_, sizeof t_, "TOONLOCK available=%d mode=%d hull=%u car=%u t=%.1f\n", SHAR_TOON_AVAILABLE, skinToon, skinToonHullCalls, carToonDraws, smkT); smkWrite(t_);
            { Shop *sh_ = mvShop(); if (sh_ && haveHomer) for (int i_ = 0; i_ < sh_->nseller; i_++) { const ShopSeller *v_ = &sh_->seller[i_];   // диагностика лавок (№314 п.3): где продавец, где игрок, кто ближе
                float dx_ = homer.x - v_->x, dz_ = homer.z - v_->z; snprintf(t_, sizeof t_, "SELLER %d shop=%s npc=%s at=%.1f,%.1f,%.1f r=%.2f homer=%.1f,%.1f,%.1f d=%.1f dy=%.1f near=%d t=%.1f\n", i_, v_->shop, v_->npc, v_->x, v_->y, v_->z, v_->radius,
                  homer.x, homer.y, homer.z, sqrtf(dx_ * dx_ + dz_ * dz_), homer.y - v_->y, shopSellerNear(sh_, homer.x, homer.y, homer.z), smkT); smkWrite(t_); } }
            skinToonDraws = carToonDraws = skinToonVerts = 0; }
          acc = nfr = 0; } }
      CSM(7);
      fogBase(); }
    else {
      sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); resetState();
      for (int i = 0; i < nd; i++) drawPieceLegacy(&pc[sp[i]], sf[i]);
      resetState();
    }
    if (inR && trLeft == TR_FRAMES) { char t_[64]; int l_ = snprintf(t_, sizeof t_, "TRC room n=%u list=%d\n", rg.npc, (int)sceGuCheckList()); trLine(t_, l_); }
    CSMX(8);   // post до комнаты
    if (inR && gagSnap < 0) roomDrawMirrored();   // сверка g11b-collect круг 5 (P16-10): Moe's и BartRoom — отражённый проход под обычным; 11.2: комната вместо города (машина, пропы, монеты остались на улице — не рисуются)
    // небо последним из фона (шаг 1): после непрозрачного города, с тестом глубины — заливает только пустое;
    // машина/пропы рисуются поверх, полупрозрачное (очередь tq), частицы и HUD — после неба, как раньше
    CSMX(9);   // комната
    if (trLeft == TR_FRAMES) { char t_[64]; int l_ = snprintf(t_, sizeof t_, "TRC sky list=%d\n", (int)sceGuCheckList()); trLine(t_, l_); }
    if (skyLast && gagSnap < 0 && !inR && !noSkyChk) { drawSky(ex, ey, ez, 1); sceGuEnable(GU_FOG); }
    CSMX(10);   // небо
    if (jitHist && !bpass && !inR && !uiPaused() && !pano917Cap && jitterWorld((unsigned)fbp0)) resetState();
    CSMX(11);   // подмес мира   // jitter.h: подмес мира до динамики (шлейфы), история мира в VRAM
    if (trLeft == TR_FRAMES) { char t_[64]; int l_ = snprintf(t_, sizeof t_, "TRC sky-done list=%d\n", (int)sceGuCheckList()); trLine(t_, l_); }
    // машина
    CSM(8); CSFLUSH();
    BBP(BBP_CAR3D); jitterUse(0);   // машина игрока, Гомер, люди, трафик, машины миссий — без сдвига проекции (jitter.h, №505)
    unsigned xt_ = XS_T(), xw_ = XS_W();
    cgOwn = 1; cgFrames++; int forcedDrawn = haveCol && !bpass && !inR && mcrDrawPlayer(&car, cv.spin[2]); cgOwn = 0;   // SetForcedCar: машина миссии у игрока
    XS_LAP(28, xt_, xw_);
    static unsigned c3U[4], c3F; unsigned c3t_ = smkOn ? sceKernelGetSystemTimeLow() : 0;   // №870 (служебно): машина / тени / Гомер внутри x_car3d — строка CAR3DSUB
    if (forcedDrawn) resetState();
    if (haveCol && !bpass && !inR) { resetState(); skidDraw(ex, ey, ez); resetState(); }   // правка 249: следы шин (один вызов GE)
    XS_LAP(30, xt_, xw_);
    groundShadeView(ggE,ggF,ggR,ggU,SCR_H/(2.f*tanf(gFov*(GU_PI/360.f)))); XS_LAP(31, xt_, xw_);
    // №658: flag shadowspr 1 — тени спрайтами одним модулем (viewer/spritelight.h); прежние пятна Гомера и машины тогда не вызываются
    if (slShadowOn && haveCol && !bpass && !forcedDrawn) { resetState(); slShadowFrame(inR && G.pHomerWorld ? G.pHomerWorld : (onFoot() && G.pHomerWorld ? G.pHomerWorld : &world), ex, ez); XS_LAP(38, xt_, xw_); }
    else {
    if (haveCol && !bpass) { resetState();
      if (slHomerSpot) { slOnlyHomer = 1; slShadowFrame(G.pHomerWorld ? G.pHomerWorld : &world, ex, ez); slOnlyHomer = 0; }   // №78/тень: пятно Гомера как у PS2 (spritelight.h)
      else homerShadow(G.pHomerWorld ? G.pHomerWorld : &world); }   // №394: поверхность и экранный полутон
    XS_LAP(39, xt_, xw_);
    if (slCarShOn && haveCol && !bpass && !forcedDrawn) { resetState(); slOnlyCars = 1; slShadowFrame(&world, ex, ez); slOnlyCars = 0; resetState(); }   // №680
    else if (haveCarVis && haveCol && !bpass && !inR && !forcedDrawn) { resetState(); carVisDrawShadow(&cv, &car, &world); resetState(); }   // CAR1: тень-пятно
    XS_LAP(40, xt_, xw_);
    }
    XS_LAP(32, xt_, xw_); XS_CAR(1);
    if (smkOn) { unsigned n_ = sceKernelGetSystemTimeLow(); c3U[0] += n_ - c3t_; c3t_ = n_; }   // тени и следы
    int trfCarsEarly1009 = 0;   // №1009: машины трафика уже нарисованы (до своей машины и героя)
    if (inkSep1009 && haveTraffic && haveCol && !bpass && !inR && !forcedDrawn) {   // №1009: см. main_render.h inkSep1009
      float e3[3] = {ex, ey, ez}; unsigned tc0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      int bp1009_ = bbPhase; bbPhase = BBP_TRF3D;   // счётчики гейта (gateCount) и ящика — куски трафика в своей фазе, хотя рисуются раньше своей машины
      resetState(); trafficDrawCars(&traffic, e3, trVis, trDrawPiece, 0); trfCarsEarly1009 = 1; bbPhase = bp1009_;   // очередь контура и стекла НЕ выпускается здесь: уйдёт после тела своей машины (первый resetState дальше), контур ближнего ляжет поверх любого тела
      if (smkOn) { smkTrfCarMs += (sceKernelGetSystemTimeLow() - tc0_) / 1000.0; smkTrfN[0] += traffic.drawnCars; smkTrfN[1] += traffic.drawnPark; }
    }
    if (!inR && !forcedDrawn && !gameMsnCarHide) { float ca = cosf(h + GU_PI), sa = sinf(h + GU_PI);   // сверка g07 круг 5 (P06-15): на диалоге бонус-миссии машина рядом с героями спрятана
      if (lumaKey) sceGuFog(0.f, 1e-3f, LUMA_KEY_CAR & 0xffffff);   // гейт tests/light: машина сплошным ключом (вся в «тумане» ключа)
            if (haveCarVis && haveCol && !bpass) carVisMtxScope(&cv, &car, 1);   carToonPlayerNow = 1;   // ТЕСТ90: общая часть матриц кусков — раз на сустав
      for (int i = 0; i < ncarP; i++) {
        Piece *d = &pc[carP[i]]; if (cl[d->r.cell].state != 2) continue;
        if (lumaKey && (d->r.flags & 2)) continue;     // стёкла в маску не берём
        int joint = d->r.pad[0], variant = d->r.pad[1];   // CAR1: сустав и вариант (0 машина, 1 остов)
        if (haveCarVis && carVisJointHidden(&cv, joint)) continue;   // правка 354: колёса-заглушки оригинал не рисует
        if (haveCarVis && haveCol && !bpass) {         // с наклоном по дороге + анимация суставов, урон, остов
          if (!carVisVariantOn(&car, variant)) continue;
          XS_LAP(20, xt_, xw_); float m[16]; carVisPieceMatrix(&cv, &car, joint, variant, d->r.ox, d->r.oy, d->r.oz, d->r.sc, m); XS_LAP(21, xt_, xw_);
          int t0 = d->r.tex, f0 = d->r.flags; d->r.tex = carVisTex(&cv, t0);
          int toon_ = skinToonEnabled() && !(d->r.flags & 2) && !lumaKey; // 428
          cgOwn = 1; carGlass(d); cgOwn = 0; XS_LAP(22, xt_, xw_);
          if (!carCapSplitDraw(&cv, &car, d, joint, variant, m, toon_)) { XS_LAP(23, xt_, xw_); if (toon_) carToonHull(d, m); XS_LAP(24, xt_, xw_); drawPieceM(d, (const ScePspFMatrix4 *)m); XS_LAP(25, xt_, xw_); } else XS_LAP(26, xt_, xw_);   // №867
          d->r.tex = t0; d->r.flags = f0;
        } else if (haveCol && !bpass && !haveCarVis) {   // старый level.shr без CAR1
          float m[16]; carPieceMatrix(&car, carLift, d->r.ox, d->r.oy, d->r.oz, d->r.sc, m);
          drawPieceM(d, (const ScePspFMatrix4 *)m);
        } else {                                       // тест-прогон: машина в позе покоя
          if (variant) continue;
          Piece t = *d;
          if (haveCarVis && joint < cv.njoint) { t.r.ox += cv.joint[joint].x; t.r.oy += cv.joint[joint].y; t.r.oz += cv.joint[joint].z; }
          drawPiece(&t, 0, x, y + carLift, z, ca, sa);
        }
      }
      carVisMtxScope(0, 0, 0); carToonPlayerNow = 0;
      if (lumaKey) fogBase(); }
    XS_CAR(0); XS_LAP(20, xt_, xw_);
    if (carLights970) { cv.brake = carLights970 & 1; cv.reverse = (carLights970 >> 1) & 1; }
    if (haveCarVis && haveCol && !bpass && !inR && !forcedDrawn && !lumaKey) { resetState(); carVisDrawFlares(&cv, &car, ex, ey, ez, bindTexLean); resetState(); }   // CAR1: стоп-сигналы / задний ход
    XS_LAP(27, xt_, xw_);
    if (oldCarOn && haveCarVis && haveCol && !bpass && !inR) {   // правка 63: своя машина стоит, где её оставили
      float dx = oldCar.x - ex, dz = oldCar.z - ez;
      if (dx * dx + dz * dz < 110.f * 110.f)
        for (int i = 0; i < ncarP; i++) {
          Piece *d = &pc[carP[i]]; if (cl[d->r.cell].state != 2) continue;
          int joint = d->r.pad[0], variant = d->r.pad[1]; if (!carVisVariantOn(&oldCar, variant)) continue;
          if (carVisJointHidden(&oldCv, joint)) continue;   // правка 354
          float m[16]; carVisPieceMatrix(&oldCv, &oldCar, joint, variant, d->r.ox, d->r.oy, d->r.oz, d->r.sc, m);
          int t0 = d->r.tex; d->r.tex = carVisTex(&oldCv, t0); if (!carCapSplitDraw(&oldCv, &oldCar, d, joint, variant, m, 0)) drawPieceM(d, (const ScePspFMatrix4 *)m); d->r.tex = t0;   // №867
        }
      resetState();
    }
    XS_LAP(29, xt_, xw_);   // 27 — огни и пачка контура, 29 — оставленная машина
    if (smkOn) { unsigned n_ = sceKernelGetSystemTimeLow(); c3U[1] += n_ - c3t_; c3t_ = n_; }   // своя машина (куски, контур, огни, оставленная)
    // персонажи: на улице — свет уровня (skinLightLevel, LGT1), в комнате — LightGroup комнаты, как в оригинале: и Гомеру, и
    // героям миссий в ней (29.09, рецепт §12; до того герои миссий в комнате светились солнцем улицы)
    if (inR) skinLights(rg.nlit, (const float (*)[3])rg.lit, rg.litc, rg.amb);
    else if (skinDataLit && lvlLightOk == 1 && lvlLight.ndir) skinLights((int)lvlLight.ndir, (const float (*)[3])lvlLight.dir, lvlLight.col, lvlLight.amb);   // 16.10: свет персонажей из LightGroup "sun" уровня целиком (3 направленных + фон), не форма порта «солнце + фон × множитель»
    XS_LAP(33, xt_, xw_);
    if (haveHomer && haveCol && !bpass && gagSnap < 0 && (!homerSpawnWait431 || !homerSpawnPending(&homer))) {   // правка 431: Гомер без опоры под точкой постановки не рисуется; этап 4: Гомер (и за рулём — виден в окна); GE-скиннинг, свой свет (в комнате — LightGroup комнаты)
      float dx = homer.x - ex, dz = homer.z - ez;
      if (dx * dx + dz * dz < 80.f * 80.f) {
        if (lumaKey) sceGuFog(0.f, 1e-3f, LUMA_KEY_HOMER & 0xffffff);   // гейт tests/light: Гомер сплошным ключом
        int hv = 1;   // B5: водитель вне пирамиды кадра — поза и рисование не нужны (рамка 3,2 м вокруг места Гомера в машине); потом первый кадр — полная поза
        float bb[6];   // №520: рамка от текущей матрицы машины (не от place, который вне кадра не обновляется)
        if (!DRIVER_POSE_OLD && homerDriverBox(&homer, bb)) { hv = visBox(bb, 0, 0, 0); if (!hv) homer.drvPoseValid = 0; }
        XS_LAP(34, xt_, xw_);
        if (hv) { homerPose(&homer); XS_LAP(35, xt_, xw_); resetState(); XS_SKIN(1); homerDraw(&homer); XS_SKIN(0); resetState(); XS_LAP(36, xt_, xw_); }
        if (lumaKey) fogBase();
      }
    }
    if (smkOn) { unsigned n_ = sceKernelGetSystemTimeLow(); c3U[2] += n_ - c3t_; c3F++;   // Гомер
#if !SHR_REL_SMK
      if (c3F >= 30 && xsubOn) { char x_[1600]; int xl_ = snprintf(x_, sizeof x_, "XSUB t=%.1f sc=%s fr=%u (мкс/кадр:вызовов/кадр:слов/кадр)", smkT, smkScene, c3F);   // номера участков — xsub.h и ОТЧЁТ_x_car3d
        for (int i_ = 0; i_ < XS_N && xl_ < (int)sizeof x_ - 40; i_++) if (xsubC[i_] || xsubU[i_]) xl_ += snprintf(x_ + xl_, sizeof x_ - xl_, " [%d]%.0f:%.2f:%.0f", i_, (double)xsubU[i_] / c3F, (double)xsubC[i_] / c3F, (double)xsubW[i_] / 4.0 / c3F);
        x_[xl_++] = 10; x_[xl_] = 0; smkWrite(x_); memset(xsubU, 0, sizeof xsubU); memset(xsubC, 0, sizeof xsubC); memset(xsubW, 0, sizeof xsubW); }
#endif
      if (c3F >= 30) { char l_[120]; snprintf(l_, sizeof l_, "CAR3DSUB t=%.1f sc=%s shadow=%.3f car=%.3f homer=%.3f ms\n", smkT, smkScene, c3U[0] / 1000.0 / c3F, c3U[1] / 1000.0 / c3F, c3U[2] / 1000.0 / c3F); smkWrite(l_);
        { static unsigned fb0_, fc0_; if (skinFootBakedHits != fb0_ || skinFootComputed != fc0_) { char f_[100]; snprintf(f_, sizeof f_, "FOOTBAKE t=%.1f baked=%u computed=%u\n", smkT, skinFootBakedHits, skinFootComputed); smkWrite(f_); fb0_ = skinFootBakedHits; fc0_ = skinFootComputed; } }   /* Модели-96: сколько поправок стоп взято из данных и сколько посчитано на месте (новый клип) */ c3U[0] = c3U[1] = c3U[2] = 0; c3F = 0; } }
    BBP(BBP_NPC3D);
    if (haveCol && !bpass) { mvSetVis(trVis); mvSetSceneOk(mvSceneMain); mvSetGround(mvGroundHook); msnartGroundN = mvGroundNHook; mvSetGov(frameGov); mvDraw3D(ex, ey, ez, dt); resetState(); }   // пункт 8: персонажи миссий (Мардж, Апу)
    if (inR) skinLights(0, 0, 0, 0);
    BBP(BBP_TRF3D);
    if (haveTraffic && haveCol && !bpass && !inR) {   // этап 5: машины трафика и стоящие (куски — drawPieceM, стекло в очередь), пешеходы
      float e3[3] = {ex, ey, ez};
      unsigned tc0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      if (!trfCarsEarly1009) trafficDrawCars(&traffic, e3, trVis, trDrawPiece, 0);
      if (smkOn && !trfCarsEarly1009) { smkTrfCarMs += (sceKernelGetSystemTimeLow() - tc0_) / 1000.0; smkTrfN[0] += traffic.drawnCars; smkTrfN[1] += traffic.drawnPark; }   // №914: цена машин трафика и число
      if (smkOn && trfFadeN) { char s_[80]; snprintf(s_, sizeof s_, "TRFFADE t=%.2f n=%u amin=%.2f\n", smkT, trfFadeN, trfFadeMin); smkWrite(s_); } trfFadeN = 0; trfFadeMin = 1.f;   // №1202: кадр с растворяющейся машиной
      if (smkOn && trfPopN) { for (unsigned q_ = 0; q_ < trfPopN; q_++) { char s_[120]; snprintf(s_, sizeof s_, "TRFPOP t=%.1f %s=%u d=%.1f R=%.1f born=%d dd=%.0f\n", smkT, trfPop[q_].pass ? "park" : "car", (unsigned)trfPop[q_].i, trfPop[q_].d, trfPop[q_].R, trfPop[q_].born, traffic.drawDist); smkWrite(s_); } trfPopN = 0; }   // №1202: появление машин в кадре (расстояние, родилась / пересекла дальность)
      if (smkOn) { static float cgT_; cgT_ += dt; if (cgT_ >= 1.f) { char s_[100]; snprintf(s_, sizeof s_, "CLIPG t=%.1f seen_close=%u drawn_close=%u hidden=%u\n", smkT, clipSeen, clipDrawnClose, clipHid); smkWrite(s_); { char cf_[260]; int mis_ = 0; for (int q_ = 1; q_ < CF_SETS; q_++) mis_ += cfSet[q_].nbits; snprintf(cf_, sizeof cf_, "CARFILL t=%.1f pieces=%d mis=%d on=%d player=%d draws=%u bakes=%u verts=%u changed=%u restores=%u orig_b=%u vh=%08x flat_tri=%u tex_tri=%u thq_peak=%d\n", smkT, cfSet[0].nbits, mis_, carFillOn, carFillPlayer, cfDraws, cfBakes, cfVerts, cfChanged, cfRestores, cfOrigBytes, cfHash(), cfTris, cfTexTris, thQPeak); smkWrite(cf_); snprintf(cf_, sizeof cf_, "CFCOST t=%.1f whole_n=%u whole_us=%u fill_n=%u fill_us=%u pick_us=%u hull_us=%u\n", smkT, cfND[0], cfUsD[0], cfND[1], cfUsD[1], cfUsP, cfUsH); smkWrite(cf_); cfND[0] = cfND[1] = cfUsD[0] = cfUsD[1] = cfUsP = cfUsH = 0; } clipSeen = clipDrawnClose = clipHid = 0; cgT_ = 0.f; char bc_[220]; snprintf(bc_, sizeof bc_, "BIGCUT t=%.1f pieces=%u forced_cells=%u gg_pieces=%u gg_tris=%u gg_exam=%u gg_ms=%.2f pm=%u pmgeo=%u\n", smkT, bigPiecesF, pbBigForce, ggPiecesAll, ggTrisAll, ggExamAll, ggUsAll / 1000.0, dbgPM, dbgPMgeo); smkWrite(bc_); bigPiecesF = 0; pbBigForce = 0; dbgPM = dbgPMgeo = 0; } }   // clipguard.h: объекты ближе 0,3 м к глазу
      unsigned tp0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      trfProf = smkOn; skinWProf = smkOn;
      resetState(); trafficDrawPeds(&traffic, e3, trVis); resetState();
      if (smkOn) { smkTrfPedMs += (sceKernelGetSystemTimeLow() - tp0_) / 1000.0; smkTrfN[2] += traffic.drawnPeds; }
      if (smkOn) { for (int q_ = 0; q_ < 5; q_++) smkSkinW[q_] += skinW[q_]; smkSkinW[5] += skinNB[0]; smkSkinW[6] += skinNB[1]; } memset(skinW, 0, sizeof skinW); skinNB[0] = skinNB[1] = 0; skinWProf = 0;
      if (smkOn) { smkTrfPose += trfPoseUs / 1000.0; smkTrfDraw += trfDrawUs / 1000.0; smkTrfPoseN += trfPoseN; smkTrfWords += trfWords; } trfPoseUs = trfDrawUs = trfPoseN = trfWords = 0;
    }
    BBP(BBP_MCR3D);
    if (haveCol && !bpass && !inR) { float e3[3] = {ex, ey, ez}; mcrSync(); mcrDraw(e3); mcrStaggerOn = 0; resetState(); }   // машины миссий с ИИ
    BBP(BBP_PROPS3D); smkSub(0); jitterUse(jitHist ? 0 : 1);   // пропы, стейтпропы, билборды, частицы: со сдвигом, как город, пока подмес в конце кадра; после подмеса мира (jitHist) без сдвига (Трассы); smkSub — PHSUB (Физика)
    unsigned pq0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    if (!inR) propToonPass(eyeX, eyeY, eyeZ);   // №817: оболочки пропов и стейтпропов — до их тел (листов клеток и прежнего пути)
    if (smkOn) smkPbT[0] += (sceKernelGetSystemTimeLow() - pq0_) / 1000.0;   // №915: цена обводки пропов
    if (!inR && !PROP_BAKE_OLD && (haveProps || haveSP)) {   // B1: запечённые листы клеток пропов и стейтпропов (propbake.h); динамические — ниже прежним путём
      unsigned pb0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; pbEnsure(); int bl_[2] = {0, 0}; float md_ = drawDist * .92f + 4.f;
      pbFrameSet(&pbP, eyeX, eyeY, eyeZ, md_, bl_); pbFrameSet(&pbS, eyeX, eyeY, eyeZ, md_, bl_);
      unsigned pbc0_ = smkOn ? sceKernelGetSystemTimeLow() : 0, pbw0_ = smkOn ? (unsigned)sceGuCheckList() : 0, pbk0_ = pbCalls; if (smkOn) smkPbT[1] += (pbc0_ - pb0_) / 1000.0;
      pbCallSet(&pbP); pbCallSet(&pbS); resetState();
      if (smkOn) { smkPbT[2] += (sceKernelGetSystemTimeLow() - pbc0_) / 1000.0; smkPbT[3] += ((unsigned)sceGuCheckList() - pbw0_) / 4u; smkPbT[4] += pbCalls - pbk0_; rfPbUs += sceKernelGetSystemTimeLow() - pb0_; }
    }
    // пропы (этап 3б): грубая отсечка по расстоянию от камеры, пробуждение — в props.c
    if (haveProps && !inR) {
      gloSite = 1; unsigned npd_ = 0; unsigned *pd_ = inR ? 0 : pbDynList(&pbP, &npd_); if (!pd_) npd_ = props.ninst;
      for (unsigned ii_ = 0; ii_ < npd_; ii_++) { unsigned i = pd_ ? pd_[ii_] : ii_;
        PropDef *pd = &props.def[i]; PropState *st = &props.st[i]; PropType *ty = &props.types[pd->type];
        if (st->hidden) continue;   // вместо него играет обломок (SPR1)
        if (geOver(GE_SOFT)) break;   // правка 103: список GE почти полон
        float ddx = st->x - ex, ddz = st->z - ez;
        { float pr = drawDist * .92f; if (ddx * ddx + ddz * ddz > pr * pr) continue; }   // до почти полной дымки, без «выскакивания»
        {
#if CULL_SPHERE_OLD
          float r = ty->radius + 2.f * ty->halfH + 1.f;
#else
          float r = ty->radius + 2.f * ty->halfH + 2.0f;   // №546: запас 1–2 м по краям
#endif
          float bb[6] = {st->x - r, st->y - r, st->z - r, st->x + r, st->y + r, st->z + r};
          if (!visBox(bb, 0, 0, 0)) continue; }
        if (clipGuardProp(eyeX, eyeY, eyeZ, st->x, st->y, st->z, ty->radius, ty->halfH, ty->radius <= 1.5f && camNearSphere(eyeX, eyeY, eyeZ, st->x, st->y, st->z, ty->radius))) continue;   // + CLIP_GUARD для всех пропов   // №502: небольшой проп у самого глаза не рисуется (деревья и столбы крупнее — камера их и так обходит по коллизии)   // за кадром не рисуем (при 250 м пропов в круге сотни; раньше GE резал их сам)
        for (unsigned k = 0; k < ty->nPiece; k++) {
          PropPiece *pp = &props.pieces[ty->firstPiece + k];
          if (pp->ni <= 0) continue;
          if (pbPartBaked(&pbP, i) && !(pp->flags & (2 | F_ADDITIVE))) continue;   // B1: непрозрачный кусок уже в листе клетки
          float m[16]; propPieceMatrix(st, pp->ox, pp->oy, pp->oz, pp->sc, m);
          if (pieceCull923 && ty->nPiece >= PCULL_MIN) { float e3p_[3] = {eyeX, eyeY, eyeZ}; if (pcOut923((const char *)props.vertBase + pp->vOff, pp->nv, pp->flags, m, e3p_)) continue; }   // №923: кусок вне кадра
          Piece tmp; memset(&tmp, 0, sizeof tmp);
          tmp.r.tex = pp->tex; tmp.r.flags = pp->flags; tmp.r.ni = pp->ni;
          tmp.v = (Vert *)((char *)props.vertBase + pp->vOff);
          tmp.idx = (unsigned short *)((char *)props.idxBase + pp->iOff);
          propPiece(&tmp, m);
        }
      }
#ifndef PROP_MERGE_FLUSH_OLD
#define PROP_MERGE_FLUSH_OLD 0
#endif
      if (PROP_MERGE_FLUSH_OLD || !haveSP) propFlush();   // стейтпропы идут в ту же пачку (привязок текстур меньше: слово команды ≈ 0,5 мкс); -DPROP_MERGE_FLUSH_OLD=1 — две пачки
    }
    // стейтпропы и обломки: куски на суставах (жёсткие), видимость по состоянию и кадру; потом билборды
    smkSub(1);
    if (haveSP && !inR) {
      gloSite = 2; unsigned spUs0_ = sceKernelGetSystemTimeLow(); float e3_[3] = {eyeX, eyeY, eyeZ};   // render_fast: время стейтпропов (RFAST sp_us); глаз отсечения — как visBox
      unsigned spC0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      unsigned *cand_ = 0; unsigned nc_ = 0; { unsigned *dl_ = pbDynList(&pbS, &nc_); if (dl_) cand_ = dl_; else nc_ = spGridCollect(drawDist * .92f, e3_, (rfOld & RF_SP) ? 0 : (const float (*)[3])pl, &cand_); }
      if (smkOn) smkSpT[0] += (sceKernelGetSystemTimeLow() - spC0_) / 1000.0;   // №912: сбор кандидатов   // B1: динамические; иначе сетка: только клетки в дальности и кадре (SPROP_GRID_OLD)
      if (smkOn) smkSpCand += nc_;   // №912: обойдённые кандидаты стейтпропов (sp_inst — все экземпляры уровня)
      if (smkOn) smkGateMask = 0;
      for (unsigned ci_ = 0; ci_ < nc_; ci_++) { unsigned i = cand_ ? cand_[ci_] : ci_;
#define GB1223(b) do { if (smkOn && i == smkGateInst) smkGateMask |= (b); } while (0)
        GB1223(1);
        if (!spDrawable(&sprops, i, ex, ez, drawDist * .92f)) continue;
        GB1223(2);
        if (smkOn) smkSpDrawn++;   // №912: прошли spDrawable (sp_cand — кандидатов в цикле)
        if (geOver(GE_SOFT)) break;   // правка 103
        const SpType *T = &sprops.type[sprops.inst[i].type];
        if (!(rfOld & RF_SP)) { float r_ = rfSpRadius(sprops.inst[i].type);   // О2: вне кадра — без позы и матриц (сфера типа при любой позе)
          const SpRun *ru_ = &sprops.run[i]; float c_[3] = {ru_->x, ru_->y, ru_->z};
          if (r_ >= 0.f && rfSphereOut((const float (*)[3])pl, e3_, c_, r_)) { rfSpCut++; continue; }
          if (spMinPx > 0.f && r_ >= 0.f) { float dx_ = c_[0] - e3_[0], dy_ = c_[1] - e3_[1], dz_ = c_[2] - e3_[2], d2_ = dx_ * dx_ + dy_ * dy_ + dz_ * dz_, k_ = r_ * SP_FOCAL_PX;   // 17.10: меньше spMinPx пикселей на экране — не рисуем
            if (k_ * k_ < spMinPx * spMinPx * d2_) { spSmallCut++; continue; } } }
        GB1223(4);
        { const SpRun *rn_ = &sprops.run[i]; float c_[3], r_; spBounds(&sprops, i, c_, &r_);   // №502: небольшой стейтпроп (ящик, автомат, будка) у самого глаза не рисуется
          if (r_ <= 2.5f && clipGuardSphere(e3_[0], e3_[1], e3_[2], c_[0], c_[1], c_[2], r_, camNearSphere(e3_[0], e3_[1], e3_[2], c_[0], c_[1], c_[2], r_))) { (void)rn_; continue; } }
        GB1223(8);
        // Правка «быстрый расчёт» (x_props ТОП-1 тура SHARTEST6: поза ~32 экземпляров на кадр, у кустов и деревьев 7–21 сустава на каждой):
        // качание далёких (> 30 м — через кадр, > 60 м — раз в 4 кадра) держит прошлую позу суставов; поза считается в свой кадр
        // (индекс + кадр), экземпляр, у которого позы ещё не было, и ближние — как раньше. -DSP_POSE_LOD_OLD=1 — прежнее
#ifndef SP_POSE_LOD_OLD
#define SP_POSE_LOD_OLD BIS(4)
#endif
        // №521 (Роб, железо 0516: на АЭС мигает свет, столбы лучей лагают): лучи и свет — стейтпропы с билбордами или маской видимости; отложенная поза
        // (> 30 м через кадр, > 60 м раз в 4, при перегрузке B7 — от 15 м) обновляла их раз в 2–4 кадра, а переключение потолка кадра (0,5 с) меняло
        // темп на лету — лучи шли ступеньками и мигали. Такие экземпляры (билборды / маска видимости типа) позу не откладывают. -DSP_POSE_BEAM_OLD=1 — прежнее
#ifndef SP_POSE_BEAM_OLD
#define SP_POSE_BEAM_OLD 0
#endif
        { SpRun *rp_ = &sprops.run[i]; int skipPose_ = 0; int beam_ = !SP_POSE_BEAM_OLD && (T->nBill || T->pvisMask);
          if (!SP_POSE_LOD_OLD && !beam_ && rp_->posed && !rp_->poseValid) { float dx_ = rp_->x - ex, dz_ = rp_->z - ez, d2_ = dx_ * dx_ + dz_ * dz_;
            unsigned ev_ = frameGov ? (d2_ > 30.f * 30.f ? 4u : d2_ > 15.f * 15.f ? 2u : 1u) : (d2_ > 60.f * 60.f ? 4u : d2_ > 30.f * 30.f ? 2u : 1u); skipPose_ = ev_ > 1 && ((frames + i) % ev_) != 0;
            if (spSwayFar && T->kind == 0 && T->nState <= 1 && d2_ > SP_SWAY_FAR * SP_SWAY_FAR) skipPose_ = 1; }   // Роб 17.10: деревья и кусты вдали без качания — поза замирает (одно состояние, зацикленная анимация)
          if (!skipPose_) { unsigned pt_ = smkOn ? sceKernelGetSystemTimeLow() : 0; rfSpPose++; spPose(&sprops, i); if (smkOn) { double dt_ = (sceKernelGetSystemTimeLow() - pt_) / 1000.0; smkSpPoseMs += dt_; smkSpT[1] += dt_; } } }
        if (wrSprOn() && wrSprInst(i)) { wrSprAdd(i); continue; }   // №803: ключ — спрайтом после позы (wrSprFlush ниже), 3D-куски не рисуются
        if (smkOn) { smkSpVis++; smkSpPcs += T->nPiece; }
        unsigned spP0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; unsigned spDn_ = 0;   // №923 служебно: SPTOP — какие типы стейтпропов дают куски в кадре
        for (unsigned k = 0; k < T->nPiece; k++) {
          if (!spPieceOn(&sprops, i, k)) continue;
          const SpPiece *pp = &sprops.piece[T->firstPiece + k];
          if (!pp->ni) continue;
          if (pbPartBaked(&pbS, i) && !(pp->flags & (2 | F_ADDITIVE))) continue;   // B1: непрозрачный кусок уже в листе клетки
          unsigned pm0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
          if (smkOn && (pp->flags & 2)) smkSpT2[2] += 1.0;
          float m[16]; spPieceMatrix(&sprops, i, k, m);
          unsigned pm1_ = smkOn ? sceKernelGetSystemTimeLow() : 0; if (smkOn) smkSpT2[0] += (pm1_ - pm0_) / 1000.0;
          if (pieceCull923 && T->nPiece >= PCULL_MIN && pcOut923((const char *)sprops.vertBase + pp->vOff, pp->nv, (int)(pp->flags & (0xFF | F_ADDITIVE)), m, e3_)) continue;   // №923: кусок вне кадра
          Piece tmp; memset(&tmp, 0, sizeof tmp);
          tmp.r.tex = pp->tex; tmp.r.flags = pp->flags & (0xFF | F_ADDITIVE); tmp.r.ni = pp->ni;
          tmp.v = (Vert *)((char *)sprops.vertBase + pp->vOff);
          tmp.idx = (unsigned short *)((char *)sprops.idxBase + pp->iOff);
          propPiece(&tmp, m); spDn_++;
          if (smkOn) smkSpT2[1] += (sceKernelGetSystemTimeLow() - pm1_) / 1000.0;
        }
        if (spDn_) GB1223(16);
        if (smkOn) spTopAdd(sprops.inst[i].type, spDn_, T->nPiece, T->nJoint, T->nState, T->kind, T->nBill, T->pvisMask != 0);
        if (smkOn) smkSpT[2] += (sceKernelGetSystemTimeLow() - spP0_) / 1000.0;   // №912: куски (матрицы + propPiece)
      }
      { unsigned fl_ = smkOn ? sceKernelGetSystemTimeLow() : 0; propFlush(); if (smkOn) smkSpFlushMs += (sceKernelGetSystemTimeLow() - fl_) / 1000.0; }
      gloSite = 0; if (smkOn) spTopTick();
      rfSpUs += sceKernelGetSystemTimeLow() - spUs0_; smkSpLoopMs += (sceKernelGetSystemTimeLow() - spUs0_) / 1000.0; smkSpInst += sprops.ninst;
      smkSub(2);
      // билборды (тень ящика, свет автомата, дым будки, дуги щитка): квадраты без записи глубины, после непрозрачного
      lspBegin();   // №599: таблица постоянного света; «свои» билборды цикл ниже пропускает
      int nb = 0, aoGl_ = 0; unsigned bb0_ = ggVbUsed;   // aoGl_ — №834: блики выключателей этого кадра
      typedef struct { float u, v; unsigned c; float x, y, z; } BV2;
      BV2 *bq_ = 0; unsigned bqn_ = 0; int bqTex_ = -9, bqBlm_ = -9;   // №912: открытая группа квадов (BB_BATCH_OLD)
      unsigned *cand2_ = 0; unsigned nc2_ = spGridCollect(60.f, e3_, 0, &cand2_);
      if (smkOn) smkBbCand += nc2_;   // №912: кандидаты билбордов
      unsigned bbT0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      for (unsigned ci_ = 0; ci_ < nc2_; ci_++) { unsigned i = cand2_ ? cand2_[ci_] : ci_;
        if (!spDrawable(&sprops, i, ex, ez, 60.f)) continue;
        if (geOver(GE_SOFT)) break;   // правка 103
        const SpType *T = &sprops.type[sprops.inst[i].type];
        if (T->nBill) spPose(&sprops, i);   // О2: экземпляр вне кадра позу выше не считал (ленивая: посчитанная — бесплатно)
        for (unsigned b = 0; b < T->nBill; b++) {
          float v[12]; uint32_t col, bfl; int bt;
          if (lspOwnsBill(i, b)) continue;   // №599: этот свет рисует таблица (lspDraw ниже)
          if (!spBillQuad(&sprops, i, b, ex, ey, ez, v, &col, &bt, &bfl) || !(col >> 24)) continue;
          if (dripSkipBill(v, bfl)) continue;   // №770: капля из трубы — векторным столбом (dripDraw)
          if (aoBillInst(i)) { if (aoBillNeon(i, v)) continue; if (!aoGl_++) glintBegin(16); aoBillGlint(i, v, col, ex, ey, ez); continue; }   // №834: свечение выключателя — векторным бликом; №885: вывески без действия — без свечения
          if (!neonGlowOn && ((bfl >> 8) & 0xFF) == 2 && !(bfl & SP_BILL_PVIS)) continue;   // 15.10: ореолы фонарей, неон, лучи со сложением не рисуем (NEON_GLOW_ON=1 — как раньше); огни барьеров (PVIS) остаются
          lspOldShown(i, b);
          if (!AO_NEON885_OLD && ((bfl >> 8) & 0xFF) == 2 && !(bfl & SP_BILL_PVIS)) {   // №885: крупное неоновое свечение пропа (вывеска на арке въезда АЭС 6,3 м) — без свечения
            float w_ = (v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2]), h_ = (v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2]);
            if ((w_ > h_ ? w_ : h_) > NEON885_MAX * NEON885_MAX) continue; }
          if (!GLOW_GLINT_OLD && ((bfl >> 8) & 0xFF) == 2) { gq944Site = "props"; glowQuadQ(v, col, (float)i * .41f + (float)b); continue; }   // №839: огни заграждений и свечения пропов — бликом
          gloAdd('B', bt, (bfl >> 8) & 0xFF, (bfl & SP_BILL_PVIS) != 0, 2);   // №933: квад билборда реально рисуется текстурой
          if (!nb++) {
            ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
            sceGuSetMatrix(GU_MODEL, &I); sceGuTexScale(1.f, 1.f);
            sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); curFlags = -1; setCull(0); if (glowBias) setBias(glowBias);
          }
          unsigned bbD0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
          int blm = (bfl >> 8) & 0xFF; g944Bill("props_bill", blm, bt, v, col, i, b);   // №944; BLMD шейдера: 2 — сложение, 3 — вычитание (тень), иначе обычная прозрачность
          typedef struct { float u, v; unsigned c; float x, y, z; } BV;
          if (bbBatchOn && !BB_BATCH_OLD) {   // №912: соседние квады с той же текстурой и режимом — одной группой
            if (bqn_ && (bt != bqTex_ || blm != bqBlm_)) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)bqn_ * 6, 0, bq_); if (smkOn) smkBbCalls++; bqn_ = 0; bq_ = 0; }
            if (!bq_) {
              if (blm == 2) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
              else if (blm == 3) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
              else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
              if (blm == 2 && !glowFogOld) sceGuDisable(GU_FOG); else if (!inR) sceGuEnable(GU_FOG);
              bindTexLean(bt); bq_ = (BV2 *)ggVbAlloc(BB_BATCH_MAX * 6 * sizeof(BV2)); bqn_ = 0; bqTex_ = bt; bqBlm_ = blm;
            }
            static const unsigned char ord_[6] = {0, 1, 2, 2, 1, 3};   // полоса 0 1 2 3 -> треугольники (0 1 2) (2 1 3)
            BV2 *o_ = bq_ + bqn_ * 6;
            for (int m_ = 0; m_ < 6; m_++) { int c_ = ord_[m_]; o_[m_].u = (float)(c_ & 1); o_[m_].v = spBillTexV(c_, spBillUvOld); o_[m_].c = col; o_[m_].x = v[3 * c_]; o_[m_].y = v[3 * c_ + 1]; o_[m_].z = v[3 * c_ + 2]; }
            bqn_++; if (smkOn) smkBbQuads++;
            if (bqn_ == BB_BATCH_MAX) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)bqn_ * 6, 0, bq_); if (smkOn) smkBbCalls++; bqn_ = 0; bq_ = 0; }
            continue;
          }
          if (blm == 2) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
          else if (blm == 3) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
          else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
          // правка Роба 76: у групп квадов свечения в данных fog 0 — туман к ним не применяется. Туман GE красит чёрные края
          // свечения сложением в цвет тумана, и вдали квад светится ровным светлым квадратом «в небе» (карточка, ключ)
          if (blm == 2 && !glowFogOld) sceGuDisable(GU_FOG); else if (!inR) sceGuEnable(GU_FOG);
          BV *q = (BV *)ggVbAlloc(4 * sizeof(BV));
          for (int k = 0; k < 4; k++) { q[k].u = (float)(k & 1); q[k].v = spBillTexV(k, spBillUvOld); q[k].c = col; q[k].x = v[3 * k]; q[k].y = v[3 * k + 1]; q[k].z = v[3 * k + 2]; }
          bindTexLean(bt);
          sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, q);
          if (smkOn) { smkBbQuads++; smkBbCalls++; smkBbDrawMs += (sceKernelGetSystemTimeLow() - bbD0_) / 1000.0; }
        }
      }
      if (bqn_) { sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)bqn_ * 6, 0, bq_); if (smkOn) smkBbCalls++; bqn_ = 0; bq_ = 0; }   // №912: последняя группа квадов
      unsigned bbT1_ = smkOn ? sceKernelGetSystemTimeLow() : 0; if (smkOn) smkBbLoopMs += (bbT1_ - bbT0_) / 1000.0;
#define BBT(n_, call_) do { unsigned t_ = smkOn ? sceKernelGetSystemTimeLow() : 0; call_; if (smkOn) smkBbTm[n_] += (sceKernelGetSystemTimeLow() - t_) / 1000.0; } while (0)   // №912: время вызовов хвоста билбордов (PHSUB bb_t)
      if (aoGl_) BBT(0, glintFlush());   // №834: блики выключателей — одной пачкой
      BBT(1, glowGlintFlush(ex, ey, ez));   // №839
      BBT(2, ggVbWriteback(bb0_));   // вершины билбордов кадра — в кольце кэшируемой памяти, один сброс (GG_VB_OLD=1 — через список GE)
      if (nb) { sceGuTexScale(128.f, 128.f); if (!inR) sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); resetState(); }
      BBT(3, wrGlowDraw(ex, ey, ez, inR));   // №690: блеск под ключом (одна пачка); №829: до ключа — мягкий край спрайта смешивается с бликом за ним
      BBT(3, wrSprFlush(ex, ey, ez));   // №803: ключи спрайтом (кадры 3D-модели)
      BBT(3, garageLiftDraw(ex, ey, ez));   // №660: свет зала с лампами (flag garagelift N)
      BBT(3, dripDraw(ex, ey, ez, inR));   // №770: столбы света на месте капель
      BBT(4, lspDraw(ex, ey, ez, inR));   // №599: постоянный свет — одной пачкой на текстуру, дальность со сглаживанием
      if (smkOn) { smkBbTm[4] -= lspStatMs; lspStatMs = 0; }   // №913: счётчики появления света — только в смоуке, в цену не входят
      BBT(5, slLightDraw(ex, ey, ez, inR));   // №658: авторские световые куски (конусы, пятна, неон) спрайтами из таблицы клеток (flag lightspr 1)
      BBT(5, glow1261Draw(ex, ey, ez, inR));   // №1261: свечение дверей-входов и будок (glow1261.h)
      BBT(5, winsprFlush(0, inR));   // №1099: окна ближних клеток дальше WINSPR_DIST — ровными прямоугольниками одним вызовом (viewer/winspr.h)
      if (smkOn) smkBbTailMs += (sceKernelGetSystemTimeLow() - bbT1_) / 1000.0;
    }
    smkSub(3);
    if (smkOn) { static float ggfT_; if (smkT - ggfT_ >= 2.f || smkT < ggfT_) { ggfT_ = smkT;   // №913: кто режется у глаза (кадры за 2 с)
        char l_[200]; snprintf(l_, sizeof l_, "GGFORCE t=%.1f scene=%s bignear_us=%u/%u cut_us=%u/%u\n", smkT, smkScene, smkBigNearUs, smkBigNearN, smkBigCutUs, smkBigCutN); smkWrite(l_);
        if (crChkOn) { snprintf(l_, sizeof l_, "CRCHK mode=%d frames=%u viol=%u cand=%u extra=%u reuse=%u/%u t=%.1f\n", crMoveOn, crChkFrames, crChkViol, crChkCand, crChkExtra, rfCullReuse, rfCullAll, smkT); smkWrite(l_); }
        snprintf(l_, sizeof l_, "PBCHK frames=%u dyn_bad=%u vis_bad=%u force_cells=%u\n", pbChkFrames, pbChkDyn, pbChkVis, pbChkForce); smkWrite(l_);   // №915: сверка списков пропов с полными обходами
        snprintf(l_, sizeof l_, "SKINSLOT models=%u loads_before=%u loads_after=%u\n", skinSlotModels, skinSlotBefore, skinSlotAfter); smkWrite(l_);   // №914: слоты костей: моделей, загрузок матриц за три прохода до / после
        for (int q_ = 0; q_ < 12; q_++) if (ggfTab[q_].n) { snprintf(l_, sizeof l_, "GGFORCE_P tex=%d ni=%d phase=%d fl=%x n=%u cuts=%u us=%.0f pos=%.1f,%.1f,%.1f dist=%.1f\n", ggfTab[q_].tex, ggfTab[q_].ni, ggfTab[q_].ph, ggfTab[q_].fl, ggfTab[q_].n, ggfTab[q_].cuts, ggfTab[q_].us, ggfTab[q_].p[0], ggfTab[q_].p[1], ggfTab[q_].p[2], ggfTab[q_].dist); smkWrite(l_); }
        memset(ggfTab, 0, sizeof ggfTab); smkBigNearUs = smkBigNearN = smkBigCutUs = smkBigCutN = 0; } }
    if (haveColl && !bpass) { static float ct; ct += dt; collDraw(ex, ey, ez, inR, ct); }   // В5: карточки, осы, сцены гэгов (в комнате — её гэги)
    // монеты: один меш, крутятся вокруг Y; выпавшие гаснут (масштаб), дальность 60 м как в оригинале
    smkSub(4);
    if (haveCoins && !inR) {
      PropType *cty = &props.types[props.coinType];
      // без мипмапов: на 5 px мелкий уровень coinlogo даёт тёмное пятно (docs/pickups_spec.md)
      int ctex = props.pieces[cty->firstPiece].tex, clev = (ctex >= 0 && ctex < (int)ntex) ? tex[ctex].levels : 0;
      if (clev) { tex[ctex].levels = 1; bound = -2; }
      unsigned cidx_[512]; unsigned nci_ = coinsNear(&coins, ex, ez, 61.f, cidx_, 512);   // разбор L3/L5: монеты мира по сетке (coins.c, COINGRID_OLD), 0xFFFFFFFF — сетки нет, прежний полный обход
      unsigned ncl_ = nci_ == 0xFFFFFFFFu ? coins.n : nci_;
      for (unsigned ii_ = 0; ii_ < ncl_; ii_++) { unsigned i = nci_ == 0xFFFFFFFFu ? ii_ : cidx_[ii_];
        Coin *c = &coins.c[i];
        if (!c->active) continue;
        if (smkOn) smkCoinAct++;
        if (geOver(GE_SOFT)) break;   // правка 103
        float ddx = c->x - ex, ddz = c->z - ez;
        if (ddx * ddx + ddz * ddz > 60.f * 60.f) continue;
        // Правка «быстрый расчёт» (x_props ТОП-1 тура SHARTEST6): монеты за кадром (позади камеры, вбок) тоже считали синус/косинус, матрицу,
        // рисование куска и блёстки (две спрайта в частицах). Сфера 1,6 м (монета + блёстка до ~0,7 м) вне боковых плоскостей кадра — пропуск.
        // -DCOIN_CULL_OLD=1 — прежнее
#ifndef COIN_CULL_OLD
#define COIN_CULL_OLD 0
#endif
        if (!COIN_CULL_OLD) { float e3c_[3] = {eyeX, eyeY, eyeZ}, cc_[3] = {c->x, c->y + .3f, c->z}; if (rfSphereOut((const float (*)[3])pl, e3c_, cc_, 1.6f)) continue; }
        float a = coinAlpha(c);
        if (smkOn) smkCoinDrawn++;
        if (coinSprite && coinSprAdd(c, a, ex, ez, i)) goto coin_sparkle;   // спрайт в пачку (рисуется после цикла одним вызовом)
        { float cs_ = cosf(c->spin) * a, sn = sinf(c->spin) * a;   // проверка 17.10: синус и косинус — только 3D-модели, не спрайту
        for (unsigned k = 0; k < cty->nPiece; k++) {
          PropPiece *pp = &props.pieces[cty->firstPiece + k];
          if (pp->ni <= 0) continue;
          if (smkOn) smkCoinPcs++;
          float s_ = pp->sc, m[16] = {cs_ * s_, 0, -sn * s_, 0,  0, a * s_, 0, 0,  sn * s_, 0, cs_ * s_, 0,
                                      c->x + cs_ * pp->ox + sn * pp->oz, c->y + a * pp->oy, c->z - sn * pp->ox + cs_ * pp->oz, 1};
          Piece tmp; memset(&tmp, 0, sizeof tmp);
          tmp.r.tex = pp->tex; tmp.r.flags = pp->flags; tmp.r.ni = pp->ni;
          tmp.v = (Vert *)((char *)props.vertBase + pp->vOff);
          tmp.idx = (unsigned short *)((char *)props.idxBase + pp->iOff);
          drawPieceM(&tmp, (const ScePspFMatrix4 *)m);
        } }
        coin_sparkle:
        if (ddx * ddx + ddz * ddz < (frameGov ? 20.f * 20.f : 45.f * 45.f))   // блёстки монеты (B7: при перегрузке — только ближе 20 м) (слот частиц 0 оригинала: scratch2, аддитивно)
          { if (fxCoin1020 && !fxGlint970Old) fxSparkleCoin1020(c->x, c->y + .32f * a, c->z, 1.1f * a, c->age, i, ex, ey, ez);   // №1020: один-два случайных блика на монету
            else fxSparkleCoin(c->x, c->y + .32f * a, c->z, 1.1f * a, c->spin * .159f + i * .37f, ex, ey, ez); }   // №910: прозрачнее (COIN_GLINT910_OLD)
      }
      coinSprFlush(ctex);   // монеты-спрайты кадра — один вызов
      if (smkOn) { static float c20_; static unsigned c20n_[4]; static unsigned c20f_; c20_ += dt; c20f_++; c20n_[0] += fxCoinStat[0]; c20n_[1] += fxCoinStat[1]; if (fxCoinStat[2] > c20n_[2]) c20n_[2] = fxCoinStat[2]; c20n_[3] += fxCoinStat[1] ? 1u : 0u; memset(fxCoinStat, 0, sizeof fxCoinStat);
        if (c20_ >= .5f) { char l_[160]; snprintf(l_, sizeof l_, "COIN1020 t=%.1f on=%d frames=%u coins=%u glints=%u maxper=%u framesgl=%u phmask=%x\n", smkT, fxCoin1020, c20f_, c20n_[0], c20n_[1], c20n_[2], c20n_[3], coinPhMask1020); smkWrite(l_); c20_ = 0.f; c20f_ = 0; memset(c20n_, 0, sizeof c20n_); coinPhMask1020 = 0; } }   // №1020 служебно: монеты и блёстки за 0,5 с
      if (clev) { tex[ctex].levels = clev; bound = -2; }
    }
    smkSub(-1); BBP(BBP_FX3D);
    // Правка 313: частицы кадра — рождение (fxUpdate) и задание (шаг жизни + биллборды, viewer/fxdsp.h) уходят на Media Engine до
    // отложенного полупрозрачного; пока главный рисует стекло и листву (tqFlush), ME собирает дым, огонь и свечение; fxDrawEnd
    // забирает итог (без ME — считает сам). Порядок команд GE прежний: полупрозрачное, затем частицы
    { unsigned p0_ = meProf ? sceKernelGetSystemTimeLow() : 0;
    dashDust847();   // №847: пыль бега за ногами (dashShape)
    
#ifndef PAUSE_FX_OLD
#define PAUSE_FX_OLD 0   // сверка g10b-loop круг 7 (P17-7): 1 — частицы в паузе анимируются на реальном времени (круг 5, ошибочно); 0 — стоят с миром, как в оригинале
#endif
    // круг 7 (P17-7, сверено с оригиналом): в паузе частицы мира (искры, дым) СТОЯТ вместе с миром — продолжают только частицы HUD (летящие монеты); круг 5 ошибся в обратную сторону.
    // PAUSE_FX_OLD=1 — как в круге 5 (в паузе частицы анимируются на реальном времени). Круг 5 вдобавок съел вызовы fxDrawBegin / SMKF(0) комментарием в той же строке — восстановлено
    SMKF_START(); fxUpdate(uiPaused() && PAUSE_FX_OLD ? wallDt : dt); fxDrawBegin(ex, ey, ez, cfx, cfy, cfz); SMKF(0);
    unsigned p1_ = meProf ? sceKernelGetSystemTimeLow() : 0;
    if (!TOONLINES_ORDER_OLD && haveCol && !bpass) { unsigned eg_ = edgeDraw(ex, ey, ez); if (eg_) resetState(); unsigned tln_ = toonLinesDraw(ex, ey, ez); if (tln_) resetState(); }   // №983: слой кромки тротуара до линий   // №754 (план этап 1): линии мультика после непрозрачного, ДО стекла, листвы, дыма и частиц — сквозь них не видны
    tqFlush(); resetState(); SMKF(1);   // отложенное полупрозрачное: от дальнего к ближнему, глубину не пишет
    unsigned p2_ = meProf ? sceKernelGetSystemTimeLow() : 0;
    fxDrawEnd(); resetState(); SMKF(2);
    glowGlintFlush(ex, ey, ez);   // №839: искры, столб подбора, огни машин — бликами после частиц
    if (glintLab941) { glintLabDraw(ex, ey, ez, cfx, cfy, cfz, gFov); resetState(); }   // №941 служебно

    if (!CHEATS1104_OLD && !SHOWTREE1119_OLD && gameCheatOn(CH_TREE) && !bpass && !inR && !uiPaused() && !mvBriefing() && haveCol) {   // №1119: код Show Tree — красные коробки клеток и треугольник опоры под игроком
      float p_[3] = {onFoot() ? homer.x : car.x, onFoot() ? homer.y : car.y, onFoot() ? homer.z : car.z}, tri_[3][3], gy_; int ok_ = 1;
      for (int i = 0; i < 3; i++) { float a_ = i * 2.0943951f; tri_[i][0] = p_[0] + 1.2f * cosf(a_); tri_[i][2] = p_[2] + 1.2f * sinf(a_); if (!ground(tri_[i][0], tri_[i][2], p_[1] + 1.f, 0.f, &gy_)) { ok_ = 0; break; } tri_[i][1] = gy_ + .05f; }
      float e_[3] = {ex, ey, ez}, l_[3] = {cfx, cfy, cfz};
      if (showTreeDraw(e_, l_, gFov, p_, ok_ ? (const float (*)[3])tri_ : NULL)) resetState();
    }
    if (smkOn && colWire1223 > 0 && !bpass && !inR && haveCol) { float e_[3] = {ex, ey, ez}, l_[3] = {cfx, cfy, cfz}; if (boxWireDraw(&world, e_, l_, gFov, (float)colWire1223)) resetState(); }   // №1223 служебно: каркас ящиков коллизии (flag colwire N — радиус, м)
    if (!CHEATS1104_OLD && gameCheatOn(CH_TRIPPY) && !bpass && !inR && !uiPaused() && !mvBriefing() && trippyBlend((unsigned)(fbp0 == (void *)0 ? BUF_W * SCR_H * fbBpp : 0), fb16)) resetState();   // №1104: код Trippy — размытие кадра
    if (!bpass && !inR && !uiPaused() && jitterBlend((unsigned)(fbp0 == (void *)0 ? BUF_W * SCR_H * fbBpp : 0), fb16, (float[3]){ex, ey, ez}, gFov)) resetState();   // эксперимент jitter.h: подмес прошлого кадра SMKF(3);
    if (meProf) { unsigned p3_ = sceKernelGetSystemTimeLow(), u_ = p1_ - p0_, d_ = p3_ - p2_, n_ = (unsigned)fxCount();   // правка 313: замер
      mpFrames++; mpTqd += p2_ - p1_; mpUpd += u_; if (u_ > mpUpdMax) mpUpdMax = u_; mpDraw += d_; if (d_ > mpDrawMax) mpDrawMax = d_;
      mpFx += n_; if (n_ > mpFxMax) mpFxMax = n_; } }
    if (haveCol && !bpass) { static float mT; mT += dt;
      uiMarkerView(ggE,ggF,ggR,ggU,SCR_H/(2.f*tanf(gFov*(GU_PI/360.f))));
      static unsigned mkUs_[4], mkF_, mkT0_; unsigned m0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;   // №849 служебно: части «маркеров» (строка MARKSUB)
      tsSite = 2; aoDrawRings(ex, ey, ez, mT); resetState();   // правка 462: красное кольцо выключателей (Duff, АЭС) — видно издалека, всегда пульсирует
      unsigned m1_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      if (haveSP) mvStatePropSync(&sprops); unsigned m2_ = smkOn ? sceKernelGetSystemTimeLow() : 0; if (TOONLINES_ORDER_OLD) { unsigned tln_ = toonLinesDraw(ex, ey, ez); if (tln_) resetState(); } tsSite = 1; mvViewDir[0] = cfx; mvViewDir[1] = cfy; mvViewDir[2] = cfz; mvDrawMarkers(ex, ey, ez, mT); tsSite = 4; resetState();
      unsigned m3_ = smkOn ? sceKernelGetSystemTimeLow() : 0; if (smkOn) { mkUs_[0] += m1_ - m0_; mkUs_[1] += m2_ - m1_; mkUs_[2] += m3_ - m2_; }   // пункт 8: маркер цели (msnart.shr)
      // правка Роба 118 («не видно вешалку с чем-то»): над местом выбора костюма (ActionEvent PurchaseSkin) — значок оригинала
      // «рубашка и доллар» (составной объект shirtdollar из art/missions/generic/missgen.p3d, рядом с «!» и «$» заданий)
      float ws[4];
      for (int i = 0; wardrobeSpot(i, ws); i++) {
        float dx = ws[0] - ex, dy = ws[1] - ey, dz = ws[2] - ez;
        if (dx * dx + dy * dy + dz * dz < 40.f * 40.f) { float p[3] = {ws[0], ws[1] + WARD_ICON_H, ws[2]}; msnartDraw("shirtdollar", p, ex, ey, ez, mT); }
      }
      // правка 166 («над будками крутятся синие трубки — нет нигде»): phone_icon и голубой круг phone_icon_shadow (art/cards.p3d —
      // LoadP3DFile уровня) над ActionEvent-локатором каждой будки, ближе 40 м (близнец: трубка над будкой, пока Гомер рядом)
      if (g944DrawIdx >= 0 && g944DrawIdx < (int)(sizeof g944DrawNames / sizeof g944DrawNames[0])) msnartDraw(g944DrawNames[g944DrawIdx], 0, ex, ey, ez, mT);   // №944 служебно
      for (unsigned k = 0; k < world.nphonex && !inRoom(); k++) {
        const float *L = world.phonex[k].loc; float dx = L[0] - ex, dy = L[1] - ey, dz = L[2] - ez;
        if (dx * dx + dy * dy + dz * dz >= 40.f * 40.f) continue;
        static unsigned phI_, phS_, phG_; unsigned p0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
        float p[3] = {L[0], L[1], L[2]}; msnartDraw("phone_icon", p, ex, ey, ez, mT);
        unsigned p1_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
        float g; int gok_ = ground(L[0], L[2], L[1] + .5f, 0.f, &g); unsigned p2_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
        if (gok_) { float q[3] = {L[0], g + .03f, L[2]}; msnartDraw("phone_icon_shadow", q, ex, ey, ez, mT); }
        if (smkOn) { unsigned p3_ = sceKernelGetSystemTimeLow(); phI_ += p1_ - p0_; phG_ += p2_ - p1_; phS_ += p3_ - p2_; static unsigned pn_; if (++pn_ >= 30) { char l_[96]; snprintf(l_, sizeof l_, "PHONESUB icon=%u ground=%u shadow=%u (мкс на кадр)\n", phI_ / pn_, phG_ / pn_, phS_ / pn_); smkWrite(l_); pn_ = phI_ = phS_ = phG_ = 0; } }
      }
      resetState();
      if (smkOn) { unsigned m4_ = sceKernelGetSystemTimeLow(); mkUs_[3] += m4_ - m3_; mkF_++; if (!mkT0_) mkT0_ = m4_;
        if (m4_ - mkT0_ >= 1000000u && mkF_) { char l_[320]; snprintf(l_, sizeof l_, "MARKSUB t=%.1f sc=%s ao=%.0f sync=%.0f markers=%.0f phone=%.0f msa_calls=%.1f msa_find=%.0f route=%.0f items=%.0f npcicons=%.0f other=%.0f route_calc=%.0f route_draw=%.0f talk_sellers=%.0f (мкс на кадр)\n", smkT, smkScene, (float)mkUs_[0] / mkF_, (float)mkUs_[1] / mkF_, (float)mkUs_[2] / mkF_, (float)mkUs_[3] / mkF_, (float)msaCallN / mkF_, (float)msaFindUs / mkF_, (float)mvMkUs[0] / mkF_, (float)mvMkUs[1] / mkF_, (float)mvMkUs[2] / mkF_, (float)mvMkUs[3] / mkF_, (float)mvRouteCalcUs / mkF_, (float)mvRouteDrawUs / mkF_, (float)mvMkUs[4] / mkF_);
          smkWrite(l_); mkUs_[0] = mkUs_[1] = mkUs_[2] = mkUs_[3] = 0; mkF_ = 0; mkT0_ = m4_; msaCallN = msaFindUs = 0; memset(mvMkUs, 0, sizeof mvMkUs); mvRouteCalcUs = mvRouteDrawUs = 0; } } }
    SMKF(4);   // x_fx: маркеры, кольца, значки
    if (nearPass && !uiPaused()) {   // B2: второй проход ближних кусков своих машин
      int any_ = 0; float own2_ = 1e9f;
      { float d_ = hypotf(car.x - eyeX, car.z - eyeZ) - car.p.halfL; if (d_ < own2_) own2_ = d_; if (oldCarOn) { d_ = hypotf(oldCar.x - eyeX, oldCar.z - eyeZ) - oldCar.p.halfL; if (d_ < own2_) own2_ = d_; } }
      any_ = own2_ < NEAR_Z * 1.3f + 1.f;
      if (any_) {
        ScePspFMatrix4 pm2; sceGumMatrixMode(GU_PROJECTION); sceGumPushMatrix(); sceGumLoadIdentity(); sceGumPerspective(gFov, ASPECT, znFull_, NEAR_Z * 1.004f); sceGumStoreMatrix(&pm2); sceGumPopMatrix(); camAimPm(&pm2);
        sceGuSetMatrix(GU_PROJECTION, &pm2);
        float ggZnSave_ = ggK.zn; if (!GG_CAR853_OLD) ggK.zn = znFull_;   // №853: срез кузова у камеры — по ближней плоскости этого прохода
        sceGuDepthMask(GU_FALSE); sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuClearDepth(NEARPASS_OLD || nearPassClrOld ? 0 : NEARPASS_CLR); sceGuClear(GU_DEPTH_BUFFER_BIT); sceGuClearDepth(0); sceGuDepthRange(65535, 0);
        int tqSave_ = tqOn; tqOn = 0; resetState();
        for (int pass_ = 0; pass_ < 2; pass_++) {
          Car *ck_ = pass_ ? &oldCar : &car; CarVis *cvk_ = pass_ ? &oldCv : &cv; if (pass_ && !oldCarOn) break;
          if (hypotf(ck_->x - eyeX, ck_->z - eyeZ) - ck_->p.halfL > NEAR_Z * 1.3f + 1.f) continue;
          for (int i = 0; i < ncarP; i++) {
            Piece *d = &pc[carP[i]]; if (cl[d->r.cell].state != 2) continue;
            int joint = d->r.pad[0], variant = d->r.pad[1];
            if (carVisJointHidden(cvk_, joint)) continue;
            if (!carVisVariantOn(ck_, variant)) continue;
            float mm_[16]; carVisPieceMatrix(cvk_, ck_, joint, variant, d->r.ox, d->r.oy, d->r.oz, d->r.sc, mm_);
            int t0 = d->r.tex, f0 = d->r.flags; d->r.tex = carVisTex(cvk_, t0);
            int toon_ = !pass_ && skinToonEnabled() && !(d->r.flags & 2);
            if (!pass_) { cgOwn = 1; carGlass(d); cgOwn = 0; }
            if (!carCapSplitDraw(cvk_, ck_, d, joint, variant, mm_, toon_)) { if (toon_) carToonHull(d, mm_); drawPieceM(d, (const ScePspFMatrix4 *)mm_); }   // №867
            d->r.tex = t0; d->r.flags = f0;
          }
          resetState();
        }
        tqOn = tqSave_; ggK.zn = ggZnSave_;
        if (jitActive) jitterUse(jitHist ? 0 : 1); else { sceGumMatrixMode(GU_PROJECTION); sceGumUpdateMatrix(); }
        resetState();
      }
    }
    SMKF(5);   // x_fx: ближний проход машин
    if (artGal && haveCol) {   // служебное (flag artgal): предметы и маркеры миссий уровня 1 в ряд перед машиной, как их рисует msnart.shr
      static const char *const gal[] = {"darrow", "warrow", "icebuck", "scien", "flanpic", "tomat", "poster_t", "coolr", "tux", "bonestorm", "exclamation", "dollar",
                                        "carsphere", "triggersphere", "dice", "checkered"};
      static float gT; gT += dt; msnartPump();
      float fx = sinf(car.h), fz = cosf(car.h);
      if (artGal == 7) { float hx7 = onFoot() ? homer.x : car.x, hz7 = onFoot() ? homer.z : car.z, hh7 = onFoot() ? homer.h : car.h;   // №805: тарелка в 40 м перед героем (открытая улица)
        float p7[3] = {hx7 + sinf(hh7) * 40.f, 0.f, hz7 + cosf(hh7) * 40.f}, gy7, n7[3]; p7[1] = colGround(&world, p7[0], p7[2], 60.f, -40.f, &gy7, n7) ? gy7 : car.y;
        msnartDraw("spaceship", p7, ex, ey, ez, gT); if (!(msnartBeam995 && msnartBeam995(p7, ex, ey, ez))) msnartDraw("UfoBeam", p7, ex, ey, ez, gT); }
      else for (unsigned i = 0; i < sizeof gal / sizeof *gal; i++) {
        float row = (float)(i / 7), k = (float)(i % 7) - 3.f;
        float p[3] = {car.x + fx * (7.f + row * 5.f) + fz * k * 1.9f, car.y + .4f + row * .6f, car.z + fz * (7.f + row * 5.f) - fx * k * 1.9f};
        msnartDraw(gal[i], p, ex, ey, ez, gT);
      }
      resetState(); }
    if (shopViewOn()) { shopViewDraw3D(); resetState(); }
    if (phoneViewOn()) { phoneViewDraw3D(); resetState(); }
    { int hb[4];   // №557 (Роб 11.10): будка при 100 % урона — остов huska из мира, полный размер на месте картинки машины (окно просмотра = прямоугольник картинки). PHONE_HUSK_OLD=1 — картинка
      if (phoneViewOn() && haveCarVis && haveCol && ncarP && phoneViewHusk(hb)) {
        const float hh = -.45f * GU_PI;   // курс: капот вправо-вперёд (3/4 спереди-справа), как у картинок машин
        sceGuScissor(hb[0], hb[1], hb[0] + hb[2], hb[1] + hb[3]); sceGuViewport(2048 + (hb[0] + hb[2] / 2 - SCR_W / 2), 2048 + (hb[1] + hb[3] / 2 - SCR_H / 2), hb[2], hb[3]);
        sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(19.f, (float)hb[2] / hb[3], 1.f, 80.f);
        sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
        { ScePspFVector3 e_ = {-5.6f, 2.5f, -7.4f}, a_ = {0.f, .62f, 0.f}, u_ = {0.f, 1.f, 0.f}; sceGumLookAt(&e_, &a_, &u_); }
        sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); sceGumUpdateMatrix();
        resetState(); sceGuDisable(GU_FOG); sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND);
        float ca_ = cosf(hh), sa_ = sinf(hh);
        for (int i = 0; i < ncarP; i++) {
          Piece *d = &pc[carP[i]]; if (cl[d->r.cell].state != 2 || d->r.pad[1] != 1) continue;
          int joint = d->r.pad[0]; if (carVisJointHidden(&cv, joint)) continue;
          Piece t = *d; if (joint < cv.njoint) { t.r.ox += cv.joint[joint].x; t.r.oy += cv.joint[joint].y; t.r.oz += cv.joint[joint].z; }
          if (boothHuskLit(&t, ca_, sa_, cv.huskLift)) continue;   // №557: витринный свет (направленный сверху-слева + заполняющий) по граням остова
          drawPiece(&t, 0, 0.f, cv.huskLift, 0.f, ca_, sa_);
        }
        tqFlush();   // полупрозрачное остова (копоть, стёкла) — той же камерой, до возврата окна
        resetState();
        sceGuScissor(0, 0, SCR_W, SCR_H); sceGuViewport(2048, 2048, SCR_W, SCR_H); } }
    if (wardrobeOn()) { wardrobeDraw3D(); resetState(); }   // правка 107   // правка 94: подиум будки   // пункт 6: сцена подиума (reward-<k>.shr) — последним в 3D, свой фон
    if (bfMode_ == BF_CAPTURE) { bfCopy((unsigned)fbp0, JIT_HIST_OFF); bfSnap = 1; bfPose[0] = ex; bfPose[1] = ey; bfPose[2] = ez; bfPose[3] = cfx; bfPose[4] = cfy; bfPose[5] = cfz; bfCaptures++; }   // брифинг: снимок мира
bf_world_done:
    SMKF(6); smkSubReport(dt);   // x_fx: магазин, будка, остальное; строки PH5SUB/FXSUB
    sceGuDepthMask(GU_FALSE); BBP(BBP_UI); UIPH_START(); uiProfOn = mvProfOn = smkOn && !UIPH_OLD;
    int trUnder = !bpass && transitBusy() && uiReady && (uiPaused() || (mvBriefing() && !shopViewOn()));   // брифинг/пауза поверх закрытого ириса; экран покупки — под ирисом, как в записи PS2
    if (trUnder) { transitDraw(); resetState(); }
    UIPH(UIPH_PRE);
    if (uiReady && !bpass) {                        // этап 7: HUD (или экран паузы) после 3D и частиц, под служебным счётчиком
      if (haveCoins) {   // правка Роба 51: счёт монет — только при изменении, держится UI_COIN_HOLD (замер эталона PS2, ui_anim.h 2б)
        static int shownCoins = -1; static float coinHudT;
        if (coins.collected != shownCoins) { if (shownCoins >= 0 && !coinNoHide) coinHudT = UI_COIN_HOLD; shownCoins = coins.collected; }
        uiSetCoins(coins.collected);
        coinHudT = coinHudT > dt ? coinHudT - dt : 0.f;
        uiHudShow(UI_T_COINS, coinNoHide || coinHudT > 0.f);
      }
      uiSetSpeed(fabsf(v) * 3.6f);
      uiRadarIndoor(inR);   // №1035
      if (inR) { const RoomDoor *d = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0]; uiSetRadar(d->p[0], d->p[2], h); }   // 11.2: в комнате радар — у её двери в городе
      else uiSetRadar(x, z, h);
      // круг 2 (g09a-hud M1:Д2, Д9): карта идёт за взглядом камеры, в машине значки будок и своей машины скрыты. Функции приходят со слиянием g09a-hud; пока их нет, вызовов нет (weak)
      if (uiRadarSetCamera) uiRadarSetCamera(atan2f(cfx, cfz));
      if (uiRadarInCar) uiRadarInCar(!onFoot());
#ifndef HNR_LEVEL_DECAY_OLD
#define HNR_LEVEL_DECAY_OLD 0   // сверка g10b-loop + дирижёр, СВЕРКА_С_ИСХОДНИКОМ 2.1 / 2.5: 1 — спад шкалы 3/с на всех уровнях (прежнее)
#endif
      // 2.1: обычный спад шкалы Hit & Run задаёт скрипт уровня (SetHitAndRunDecay: L1 3,0, L2 1,5, L3–L7 1,0 ед./с; было 3,0 везде).
      // 2.5: в погоне 6 ед./с без паузы и в помещении 10 ед./с — постоянные, от уровня не зависят (дирижёр при сведении: в ветке g10b они
      // умножались на уровень, в исходнике так нет). P03-07 / 2.8: в разговоре спад не идёт (HNRTALK_OLD, сверка g09b-menu)
      if (!G.hnrOff && (HNRTALK_OLD || !mvTalkActive())) gameHnrRate(&G, dt, g10bHnrRate(gameLevel, inR, G.hnrLatched));   // спад после паузы (g10b_calc.h; сведение круга 6: g09b и g10b вынесли одно и то же в свои заголовки)
      { static float hnrT; hnrT += dt; uiSetHitAndRun(mrHnrRing(hnrHeat, hnrShow), hnrActive || (HNR_HUD_ALARM_LATCH && G.hnrLatched), hnrT); }   // правка 241: красно-синее кольцо и мигалка — когда полиция в игре (близнец tw_hnr_text: до её появления 0,74 с кольцо красно-чёрное)
      UIPH(UIPH_HUDSET);
      uiRadarClear();   // правка 236: значки радара — заново каждый кадр и без миссий (mvHud без них не чистит, машина копилась)
      mvHud(dt); UIPH(UIPH_MVHUD);   // пункт 8: задание, таймер, «этап пройден», MISSION COMPLETE, провал, значки радара
      // №393: родные ActionEvent PhoneBooth, включая свободную езду; цели миссии уже внесены первыми
      if (rtOn1091) uiRadarAdd(UI_R_MISSION, rtX1091, rtZ1091);   // №1091 служебно (flag radartgt)
      if (haveCol && !inR && !radarPhonesOld)
        for (unsigned k = 0; k < world.nphone; k++) uiRadarAdd(UI_R_PHONE, world.phone[k].p[0], world.phone[k].p[2]);
#if !RADAR_NO_CAR   // красный контроль гейта tests/ui/radar_car.sh
      // правка Роба 236 (03.10: «карта не запоминает, где была машина»): своя машина на радаре, пока Гомер пешком — значок группы
      // PlayerCars радара оригинала (HudMap.scr IconPlayerCar; был в пакете, но не выводился). В машине значок не нужен — игрок в центре
      if (onFoot() && !inR && haveCol) { uiRadarAddH(UI_R_PLAYERCAR, car.x, car.z, car.h);
        if (smkOn) { static float lx = 1e9f, lz; if (hypotf(car.x - lx, car.z - lz) > .5f) { lx = car.x; lz = car.z; char s[96]; snprintf(s, sizeof s, "RADARCAR at=%.1f,%.1f t=%.1f\n", car.x, car.z, smkT); smkWrite(s); } } }   // №236: строка при первом появлении и при каждом сдвиге машины больше чем на 0,5 м
#endif
      UIPH(UIPH_RADARADD);
      { int want = (mcPlay && !inR) || mvTalkActive() || (!TICKET_CONTENT_C3_OLD&&TICKET_BUSY());   // правка 40: полосы экрана разговора / облёта
#ifndef LB_EXIT_OLD
#define LB_EXIT_OLD 0   // сверка g10b-loop, M2:23: 1 — полосы разговора уезжают за 0,25 с, как выезжают (прежнее)
#endif
        // сверка g10b-loop, M2:23: полосы разговора и облёта уезжают 0,5 с (линейно), выезжают быстро (правило порта по записи PS2); сходящиеся к центру
        // полосы с паузой и ирисом на последнем этапе свободной езды — не сделаны (см. журнал). LB_EXIT_OLD=1 — выход тоже 0,25 с
        // круг 3 (проверка g10b-loop, M2:23): выход полос 0,5 с — как въезд (LB_SLIDE, g09b), отдельная скорость выхода убрана; LB_EXIT_OLD=1 — выход 0,25 с, как было до сверки.
        // Сходящиеся полосы: разговор последнего этапа вступления и пара Пэтти и Сельма (mvTalkConverge) — на выходе полосы сходятся к центру 0,5 с линейно (экран закрыт),
        // пауза 0,5 с, ирис раскрывается, потом полосы убраны (LB_CONV_OLD=1 — полосы просто уезжают)
#ifndef LB_CONV_OLD
#define LB_CONV_OLD 0
#endif
        { static int convLatch_, wantPrev_; if (want && mvTalkActive()) convLatch_ = mvTalkConverge();
          if (g10bLbConvStart(wantPrev_, want, convLatch_, lbConvT, mcPlay != 0)) lbConvT = 0.f;
          if (!want && !mvTalkActive() && lbConvT < 0.f) convLatch_ = 0;
          wantPrev_ = want; }
        if (lbConvT >= 0.f) {
          lbOpen = 1.f; lbConvT += dt;
          if (g10bLbConvDone(lbConvT)) {
            // №1186 (GE_ЗАТЫК_UI): ирис со своим sceGuStart+sceGuSync нельзя внутри открытого списка кадра — очередь видеочипа ждала бы конец списка кадра, а список кадра ждал бы ирис.
            // Теперь ирис закрывается на границе кадра, до sceGuStart (ниже, рядом с остальными ирисами). LB_CONV_IRIS_DEFER_OLD=1 — прежнее (внутри кадра)
#ifndef LB_CONV_IRIS_DEFER_OLD
#define LB_CONV_IRIS_DEFER_OLD 0
#endif
            if (transitReady()) {
              if (LB_CONV_IRIS_DEFER_OLD) { frames += transitClose(&fbp0, "talk_out") / (longView ? 2 : 1); chase.init = 0; smkSettle(); }
              else pendTalkOut = 1; }
            lbConvT = -1.f; lbOpen = 0.f;
          }
        } else
        lbOpen = fminf(1.f, fmaxf(0.f, lbOpen + (want ? dt / LB_SLIDE : -dt / g10bLbExitSecs(LB_SLIDE))));
        mvBubblePump();   // №1047: цель этапа, отложенная до конца наезда и ухода полос
        sndPause(uiPaused() || svmActive());   // правка 87: в паузе тишина (набор «Pause» тюнера диска)
        { static float jx, jz; static int jInit;   // правка 236: машину игрока перенесли, пока он не за рулём, — строка в журнал и чёрный ящик
          if (haveCol) { float dj = jInit ? hypotf(car.x - jx, car.z - jz) : 0.f;
            if (dj > 2.f && onFoot()) { char s[160]; snprintf(s, sizeof s, "CARJUMP from=%.1f,%.1f to=%.1f,%.1f d=%.0f op=%s room=%d msn=%s t=%.1f\n",
                                                             jx, jz, car.x, car.z, dj, bbOpS, inRoom(), bbMsn, smkT); if (smkOn) smkWrite(s); bbNote(s); }
            jx = car.x; jz = car.z; jInit = 1; } }
        sndLetterbox(want && !lbNoDuck);   // правка 55: на разговор / облёт — громкости шин «Letterbox» тюнера диска
        lbPrompt = mcPlay ? UI_LB_NONE : mvTalkPrompt();
        if (smkOn && (want ? lbPrompt : -1) != lbLogPrompt) { SndStats ss; sndStats(&ss); char s[160];
          snprintf(s, sizeof s, "LETTERBOX t=%.2f on=%d prompt=%d snd=%d music=%s\n", smkT, want, lbPrompt, sndModeApplied(), ss.music);   // правка 55: режим шин и музыка
          smkWrite(s); lbLogPrompt = want ? lbPrompt : -1; } }
      UIPH(UIPH_MISC);
      if (!pano917Cap) oldTvDraw();   // №800: «старый телевизор» — поверх мира, под интерфейсом (viewer/oldtv.h)
      collHud();   // В5: «COLLECTOR CARD!» + название + 3/7, «LEVEL CARDS COLLECTED!», «LEVEL WASPS COMPLETE!», △ у гэга
      UIPH(UIPH_COLL);
      sceGuScissor(0, 0, SCR_W, SCR_H);
      { float fa = mvFadeAlpha();   // правка 122: затемнение миссии (SetFadeOut / StayInBlack) — поверх мира, под интерфейсом
        if (fa > .003f) {
          typedef struct { unsigned color; short x, y, z, pad; } FdV;
          FdV *v = (FdV *)sceGuGetMemory(2 * sizeof(FdV)); unsigned c = (unsigned)(fminf(fa, 1.f) * 255.f + .5f) << 24;
          v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = SCR_W; v[1].y = SCR_H; v[0].z = v[1].z = 0;
          sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
          sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
          resetState(); } }
      UIPH(UIPH_FADE);
      int fsel; const char *mbt = 0;
      // №639 (Sonnet-PS2, knowledge/facts/ps2-pause-submenu-anim.yaml: Level Progress / View Cards / Options открываются КРОССФЕЙДОМ, без масштаба и сдвига; рамка на месте, меняется прозрачность):
      // вход: меню гаснет 0,24 с, пусто 0,14 с, подэкран проявляется 0,20 с; выход (△): пусто 0,08 с, меню проявляется 0,20 с (угасание подэкрана при выходе не сделано: состояние уже закрыто). PAUSE_SUB_FADE_OLD=1 — скачком
#ifndef PAUSE_SUB_FADE_OLD
#define PAUSE_SUB_FADE_OLD 0
#endif
      int pxHide = 0, pxBlank = 0; float pxA = 1.f;
      { static int pxPrev; static unsigned pxT0;
        if (!PAUSE_SUB_FADE_OLD && uiPaused() && !svmActive()) {
          int pk = pauseSub >= 0 || lsOpen || vcOpen; unsigned pn = sceKernelGetSystemTimeLow();
          if (pk != pxPrev) { pxPrev = pk; pxT0 = pn; }
          float pt = (unsigned)(pn - pxT0) * 1e-6f;
          if (pk && pxCl) pxA = 1.f - (float)(unsigned)(pn - pxClT0) * 1e-6f / .2f;
          else if (pk) { if (pt < .24f) { pxHide = 1; pxA = 1.f - pt / .24f; } else if (pt < .38f) { pxHide = 1; pxBlank = 1; } else if (pt < .58f) pxA = (pt - .38f) / .2f; }
          else if (pt < .28f) { if (pt < .08f) pxBlank = 1; else pxA = (pt - .08f) / .2f; }
        } else pxPrev = 0;
        if (pxA < .04f) { pxHide = 1; pxBlank = 1; } }   // почти нулевая прозрачность: подпись выбранного пункта рисуется мимо неё, поэтому кадр пустой
      uiSubFade(pxA);
      { // №1122: затемнение фона паузы одним слоем на весь переход меню ↔ подэкран (в пустых кадрах перехода не мигает светлым), PAUSEDIM1122_OLD=1 — как в TEST84
        float dl_ = uiPauseDimStep(uiPaused() && !svmActive() && (pauseSub >= 0 || lsOpen || vcOpen || pxHide || pxA < 1.f || pxBlank));
        if (dl_ > .003f) {
          typedef struct { unsigned color; short x, y, z, pad; } DmV;
          DmV *v = (DmV *)sceGuGetMemory(2 * sizeof(DmV)); unsigned c = (unsigned)(fminf(dl_, 1.f) * 255.f + .5f) << 24;
          v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = SCR_W; v[1].y = SCR_H; v[0].z = v[1].z = 0;
          sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
          sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
          resetState(); } }
      if (svmActive() && uiPaused()) svmDraw(0.f);   // SaveGame.scr (psave.shr)
      else if (pauseSub >= 0 && uiPaused() && !pxHide) uiScrDrawMenu(pauseSub, pauseSubSel[pauseSub], 0.f);   // OPTIONS паузы (pmenu.shr)
      else if (lsOpen && uiPaused() && !pxHide) { UiLevelStats ls; levelStatsFill(&ls); uiDrawLevelStats(&ls, 0); }   // LEVEL PROGRESS (LevelStats.scr)
      else if (vcOpen && uiPaused() && !pxHide) { uiScrDrawMenu(UI_PX_VIEWCARDS, vcSel, 0.f); if (vcZoom) vcQrDraw(vcSel); }   // VIEW CARDS (pextra.shr) + QR эпизода (идея 85)
      // правка Роба 230 (03.10: «в будке на картинках машин снизу пиксельные артефакты, шум»): кадр 5650 с дизерингом GE (шаг 2
      // аудита) — на мягкой тени картинок машин (2D, CLUT8) железо рисует сетку 4x4; эмулятор дизеринг не рисует. Страницы будки,
      // лавки и гардероба — без дизеринга (DITHER_UI_KEEP=1 — прежнее)
#if !DITHER_UI_KEEP
      else if ((wardrobeOn() || phoneViewOn() || shopViewOn()) && !uiPaused()) {
        if (fb16) sceGuDisable(GU_DITHER);
        // №532 (Роб 11.10): монета у счёта в будке вращается, как в паузе (3D-монета hudCoin3D на месте значка, плоская не рисуется). BOOTH_COIN3D_OLD=1 — плоская
#ifndef BOOTH_COIN3D_OLD
#define BOOTH_COIN3D_OLD 0
#endif
#ifndef SHOP_COIN3D_OLD
#define SHOP_COIN3D_OLD 0   // лавка и гардероб (запись PS2 14:06: у счёта монет значок монеты): плоский значок скрыт, пока HUD рисует 3D-монету, а 3D-монету звали только
                            // в будке — в лавке у счёта пусто. Теперь и в лавке 3D-монета, как в будке и паузе. 1 — прежнее
#endif
        int boothCoin = !BOOTH_COIN3D_OLD && (phoneViewOn() || (!SHOP_COIN3D_OLD && (shopViewOn() || wardrobeOn())));   // лавка и гардероб (TEST31: у счёта гардероба тоже было пусто) if (boothCoin) uiCoin3D(haveCoins && props.coinType >= 0);
        if (wardrobeOn()) wardrobeDraw2D(mvShop()); else if (phoneViewOn()) phoneViewDraw2D(mvShop()); else shopViewDraw2D(mvShop());
        if (boothCoin) hudCoin3D();
        if (fb16) sceGuEnable(GU_DITHER); }
#else
      else if (wardrobeOn() && !uiPaused()) wardrobeDraw2D(mvShop());   // правка 107: страница гардероба
      else if (phoneViewOn() && !uiPaused()) phoneViewDraw2D(mvShop());   // правка 94: страница PhoneBooth
      else if (shopViewOn() && !uiPaused()) shopViewDraw2D(mvShop());   // пункт 6: страница PurchaseRewards (shop.shr)
#endif
      else if (mvLevelEnd() && !uiPaused()) { UiLevelStats ls; levelStatsFill(&ls); uiDrawLevelStats(&ls, 1); }   // конец уровня (LevelEnd.scr)
      else if (!uiPaused() && mvMsgBox(&mbt) && uiDrawMessageBox(mbt)) {}   // правка 122: окно «Congratulations! You've unlocked …»
      else if (tutActive() && !uiPaused() && tutDraw()) {}   // подсказка Барта вместо HUD (радар скрыт, как в записи PS2)
      else if (mvBriefing() && !uiPaused()) uiDrawBriefing();
      else if (!uiPaused() && mvFailedScreen(&fsel)) uiDrawMissionOver(fsel);
      else if (lbConvT >= 0.f && !uiPaused()) uiDrawLetterboxCover(g10bLbCover(lbConvT));   // круг 3: полосы сходятся к центру 0,5 с, дальше чёрный кадр (пауза 0,5 с) и ирис
      else if (lbOpen > 0.f && !uiPaused()) uiDrawLetterbox(lbOpen, lbPrompt);   // правка 40: разговор / облёт — полосы и подписи вместо HUD
      else if (uiPaused() && pxBlank) {}   // №639: пустой кадр между меню и подэкраном
      else if (uiPaused() && mvMissionSelectOn() && uiScrReady()) mvDrawMissionSelect();   // MissionSelect.scr (pextra.shr) вместо паузы
#ifndef MSN_HUDSHOW_OLD
#define MSN_HUDSHOW_OLD 0   // сверка g07 круг 6 (P06-21, СВЕРКА_ВНЕДРЕНИЕ/g07-missions.md строка 50): было — ShowHUD("false") ставил флаг, который никто не читал, и HUD рисовался всегда; стало — пока флаг стоит, HUD не рисуется (L7 m8). 1 — как было
#endif
      else if (!MSN_HUDSHOW_OLD && !uiPaused() && mvHudHidden()) {}
      else if (!pano917Cap) { unsigned uu0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; uiCoin3D(haveCoins && props.coinType >= 0); uiFrameNo = smkFrameAbs; uiDraw(); if (uiPaused()) mvDrawMissionSelect(); hudCoin3D(); if (smkOn) rfUiUs += sceKernelGetSystemTimeLow() - uu0_; }
      uiSubFade(1.f);
      UIPH(UIPH_SCREEN);
      if (!mvBriefing() && !uiPaused() && uiBriefExitBusy()) uiDrawBriefing();   // №512: карточка уезжает вправо поверх мира после Continue
      if (!tutActive() && !uiPaused()) tutDrawExit(wallDt);   // №516: подсказка Барта уходит обратным масштабом поверх кадра игры
      if (uiPaused() && !svmActive()) {   // правка 58: код места на паузе (игроку, без служебного режима): «L1 K10-7», tools/place.py
        char pc[12], t[24]; static char pcLog[24];
        if (inR) { const RoomDoor *d = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0]; placeCode(d->p[0], d->p[2], pc, sizeof pc); }
        else placeCode(x, z, pc, sizeof pc);
        snprintf(t, sizeof t, "L%d %s", gameLevel, pc);
        if (!pauseCellOld && uiHasPlaceCode()) uiPlaceCode(t);   // №598: код клетки рисует элемент PlaceCode пакета (шрифт Bart, место и размер — из панели типографики)
        else uiText(0, 14, SCR_H - 18, 0xb0ffffff, t);
        if (smkOn && strcmp(t, pcLog)) { char s[80]; snprintf(s, sizeof s, "PLACE %s pos=%.1f,%.1f t=%.1f\n", t, x, z, smkT); smkWrite(s); snprintf(pcLog, sizeof pcLog, "%s", t); }
      }
      if (!CHEATS1104_OLD && gameCheatOn(CH_SPEEDO) && !uiPaused() && !mvBriefing() && !inR) {   // №1104: код Show Speedometer — скорость числом (в машине км/ч, пешком м/с, как в оригинале), правый нижний угол
        char t[16]; snprintf(t, sizeof t, "%d", onFoot() ? (int)homer.speed : (int)(fabsf(car.v) * 3.6f)); uiText(0, SCR_W - 14 - 14 * (int)strlen(t), SCR_H - 18, 0xe0ffffff, t); }
      if (!BUSY808_OLD) busyDraw(toneJob.active);   // №808: палитры пересчитываются — знак «думаю»
      UIPH(UIPH_EXTRA);
      resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
      if (radarLog && smkOn && (radarLogT += wallDt) >= .5f) {   // правка 50: что нарисовал радар и куда смотрит клин
        int dn, rim, rimo, cone; float ca, tx, tz; uiRadarDebug(&dn, &rim, &rimo, &cone, &ca, &tx, &tz); radarLogT = 0.f;
        char s[240]; snprintf(s, sizeof s, "RADAR t=%.1f drawn=%d rim=%d rimother=%d cone=%d ang=%.3f tgt=%.1f,%.1f pos=%.2f,%.2f h=%.4f paused=%d phones=%d markers=%d\n",
                              smkT, dn, rim, rimo, cone, ca, tx, tz, x, z, h, uiPaused(), uiRadarDrawn(UI_R_PHONE), uiMarkerDrawn()); smkWrite(s);  { char s2[96]; snprintf(s2, sizeof s2, "RADARI t=%.1f harass=%d aicar=%d mission=%d\n", smkT, uiRadarDrawn(UI_R_HARASS), uiRadarDrawn(UI_R_AICAR), uiRadarDrawn(UI_R_MISSION)); smkWrite(s2); } }
    }
    UIPH(UIPH_TAIL);
    if (!bpass) { transitHoldGate = 1; transitStep(irisDt0Test1258 ? 0.f : shopViewOn() ? wallDt : dt); transitHoldGate = 0; if (!trUnder) transitDraw(); resetState(); }   // ирис над игрой и HUD, под служебным счётчиком; мир стоит (dt 0) — ирис ждёт
    UIPH(UIPH_TRANSIT);
    if (hud && (opt & O_HUD) && !pano917Cap) { sceGuScissor(0, 0, SCR_W, SCR_H); hudDraw(); resetState(); }
    UIPH(UIPH_HUDDBG);   // служебный счётчик (R+SELECT)
    // TEST16E (слово Роба 15.10, видео с телефона 60 к/с): имя сцены, время теста, номер кадра, сектор, FPS крупно + белая метка в углу в начале каждой сцены. Только тест-апка
    // (есть сценарий, smkOn); SHARVIEW без сценария не показывает. «flag overlay 0» — убрать; TEST_OVERLAY_OFF=1 при сборке — не включать
#ifdef SHAR_RELEASE
#define TEST_OVL_ON 0   // релиз: метка сцены и строка оверлея не рисуются, даже если рядом оставлен сценарий (самоаудит Карты 16.10)
#else
#define TEST_OVL_ON 1
#endif
    { static unsigned ovFrame; ovFrame++;
      if (TEST_OVL_ON && smkOn && testOverlayOn) { char a_[80], pc_[12];
        if (inR) { const RoomDoor *d_ = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0]; placeCode(d_->p[0], d_->p[2], pc_, sizeof pc_); } else placeCode(x, z, pc_, sizeof pc_);
        snprintf(a_, sizeof a_, "t%.1f f%u %.0ffps %.21s L%d %s", smkT, ovFrame, fps, smkScene, gameLevel, pc_);   // TEST18: одна строка (время, кадр, FPS, сцена, сектор)
        hud = 0;   // старый счётчик отладки в тест-апке не рисуем: он закрывает HUD (слово Роба по видео TEST17)
        sceGuScissor(0, 0, SCR_W, SCR_H); hudOverlayLine(a_, 190, 400, smkSceneFlash > 0);
        if (smkSceneFlash > 0) { char l_[96]; snprintf(l_, sizeof l_, "OVFLASH scene=%s f=%u t=%.2f left=%d\n", smkScene, ovFrame, smkT, smkSceneFlash); smkWrite(l_); smkSceneFlash--; } resetState(); } }
    UIPH(UIPH_OVERLAY);
    uiDiagFrame();
    geNow = (unsigned)sceGuCheckList(); if (geNow > gePeakSec) gePeakSec = geNow; if (geNow > smkGePeak) smkGePeak = geNow;   // правка 103
    if (geNow > geCap) {   // правка 103 (01.10): конец кадра за бюджетом — интерфейс съел запас (массив вдвое больше — память цела); за концом массива — порча
      geOverBudget++;
      if (smkOn && (geNow > sizeof list || (geOverBudget & 63) == 1)) { char s_[96]; snprintf(s_, sizeof s_, "GELIST %s used=%u cap=%u t=%.1f\n", geNow > sizeof list ? "overflow" : "overbudget", geNow, geCap, smkT); smkWrite(s_); }
    }
    UIPH(UIPH_GECHECK);
    BBP(BBP_GESYNC);
    carToonHullFlush();   // Надзор 23.10: страховка — очередь контура машин не уходит в следующий кадр, если после неё не было resetState (будка, гараж)
    geVbEnd();   // вершины кадра из кольца — в память до запуска списка
    DCALL96();   // dcacheall96: перед закрытием списка кадра
    sceGuFinish();
    geListOpen = 0;   // №1186: список кадра закончен — вложенные списки (ирисы) допустимы
    unsigned t3 = sceKernelGetSystemTimeLow();
    if (smkFreezeMs) { unsigned ms = smkFreezeMs; smkFreezeMs = 0; sceKernelDelayThread(ms * 1000u); }   // служебное «flag freeze N» (гейт сторожа)
    sceGuSync(0, 0);
    geDeferFlush();   // отложенное освобождение: список кадра отработан
    unsigned t4 = sceKernelGetSystemTimeLow();
    { float sy_ = (t4 - t3) / 1000.f; if (sy_ > geSyncPk) geSyncPk = sy_;   // №1186: sceGuSync конца кадра, максимум за секунду; долгий (>100 мс) — строка в кольцо ящика (не чаще раза в 2 с)
      if (sy_ > 100.f && t4 - geSyncSlowT > 2000000u) { geSyncSlowT = t4; geSyncSlowN++;
        char l_[220]; snprintf(l_, sizeof l_, "GESYNCSLOW ms=%.0f ph=ui fill=%uK last=%s:%d n=%d w=%d h=%d vt=0x%x frame_area_px=%u t=%.1f\n", sy_, (unsigned)geNow >> 10, uiLastDraw.fn ? uiLastDraw.fn : "-", uiLastDraw.line, uiLastDraw.n, uiLastDraw.w, uiLastDraw.h, (unsigned)uiLastDraw.vt, uiFrAreaPx, smkT);
        bbNote(l_); if (smkOn) smkWrite(l_); } }
    smkGeWaitMs = (t4 - t3) / 1000.f;   // ME-доля: сколько кадр ждёт видеочип после конца нашего списка (sceGuSync)
    workMs = (t4 - t0) / 1000.f; farLastWorkMs = workMs; cpuMs = (t3 - t0) / 1000.f; if (ddWorkMix >= 0.f) workMs = ddWorkMix; if (workMs > workMax) workMax = workMs;   // №1244: служебная подмена работы кадра
    acctFrame(workMs, cpuMs, smkGeWaitMs, drawnTris, draws, binds, dlCallsFrame);   // учёт: 7 сложений за кадр
    frameGovStep(cpuMs, wallDt > 0.f ? wallDt : 1.f / 30.f);   // B7
    // TEST25 (слово дирижёра/Роба: статистика в игре без сценария): раз в 10 с строка STAT10 в чёрный ящик — уровень, клетка глаза (20 м), этап миссии, кадры, среднее и худшее время работы,
    // кадры дольше 33,4 мс и дольше 50 мс, дальность, свободная куча. Цена: три сложения за кадр и один snprintf в 10 с (кольцо ящика, на карту уходит вместе с ним). STAT10_OLD=1 — без строки
#ifndef STAT10_OLD
#define STAT10_OLD 0
#endif
#ifndef MEMMAP_OLD
#define MEMMAP_OLD 0
#endif
#define MEMMAP_OLD_L (MEMMAP_OLD || memmapOff)
    if (!STAT10_OLD) {
      static unsigned s10N, s10Slow, s10VSlow, s10T0; static float s10Sum, s10Max;
      s10N++; s10Sum += workMs; if (workMs > s10Max) s10Max = workMs; if (workMs > 33.4f) s10Slow++; if (workMs > 50.f) s10VSlow++;
      if (!s10T0) s10T0 = t4;
      if (t4 - s10T0 >= 10000000u && s10N) {
        char l_[200]; snprintf(l_, sizeof l_, "STAT10 lv=%d cell=%d,%d msn=%s n=%u avg=%.1f max=%.0f slow=%u vslow=%u dd=%.0f heapK=%u\n", gameLevel, (int)floorf(eyeX / 20.f), (int)floorf(eyeZ / 20.f), bbMsn, s10N, s10Sum / s10N, s10Max, s10Slow, s10VSlow, drawDist, memFreeNow == ~0u ? 0u : memFreeNow >> 10);
        bbNote(l_); if (smkOn) smkWrite(l_);
        s10N = s10Slow = s10VSlow = 0; s10Sum = s10Max = 0.f; s10T0 = t4;
      }
    }
    // TEST90 MEMMAP: раз в 30 с — живые выделения по классам размера и тяжёлые группы по вызывающему (viewer/memmap.h, расшифровка владельцев: tools/memmap_report.py по ELF). Таблица статическая,
    // замер читает её без выделений (наибольший кусок — прежняя проба memMaxBlock); цена прошлого замера печатается в строке (cost=…us). MEMMAP_OLD=1 / файл memmapold — без строки
    if (!MEMMAP_OLD_L) {   // замер по кадрам: шаг 1 — обход таблицы, шаги 2–5 — проба наибольшего куска по 3 деления, шаг 6 — строки; каждый шаг десятки–сотни микросекунд, строка — когда всё готово
      static unsigned mmT0, mmPh, mmLo, mmHi, mmFree, mmStepMax, mmSum, mmCostPrev, mmStepPrev; static float mmTs;
      if (!mmT0) mmT0 = t4;
      if (!mmPh && t4 - mmT0 >= 30000000u) { mmT0 = t4; mmPh = 1; mmStepMax = mmSum = 0; mmTs = smkOn ? smkT : bbClock; }
      if (mmPh) { unsigned u0_ = sceKernelGetSystemTimeLow();
        if (mmPh == 1) { memmapScan(); mmFree = heapFreeBytes() >> 10; mmLo = 0; mmHi = 64u << 20; mmPh = 2; }
        else if (mmPh < 6) { if (memMaxBlockStep(&mmLo, &mmHi, 3) || mmPh == 5) { if (mmHi - mmLo > (16u << 10) && mmPh == 5) memMaxBlockStep(&mmLo, &mmHi, 8); mmPh = 6; } else mmPh++; }
        else { static char mb_[1100]; unsigned n_ = memmapReport(mb_, sizeof mb_, mmTs, mmFree, mmLo >> 10, mmCostPrev, mmStepPrev);
          for (unsigned i_ = 0, st_ = 0; i_ < n_; i_++) if (mb_[i_] == '\n') { char sv_ = mb_[i_ + 1]; mb_[i_ + 1] = 0; bbNote(mb_ + st_); if (smkOn) smkWrite(mb_ + st_); mb_[i_ + 1] = sv_; st_ = i_ + 1; }
          mmCostPrev = mmSum; mmStepPrev = mmStepMax; mmPh = 0; }
        unsigned d_ = sceKernelGetSystemTimeLow() - u0_; mmSum += d_; if (d_ > mmStepMax) mmStepMax = d_; }
    }
    streamStat(workMs); loadAccStep(t4);   // Загрузка 3: строка учёта подгрузки раз в секунду (loadacc), выключена по умолчанию
    if (!DD_ADAPT_OLD) {   // бюджет кадра по дальности (main_state.h DD_*)
      static int ddLong = -1; if (ddLong != longView) { ddLong = longView; ddScale = 1.f; ddAvg = workMs; }
      float base_ = longView ? DIST_LONG : DIST_SHORT, floor_ = longView ? ddFloorHard() : DD_FLOOR_SHORT, tgt_ = longView ? DD_BUDGET_LONG : DD_BUDGET_SHORT;
      if (!bpass && !inR && haveCol && !uiPaused() && gagSnap < 0 && !(opt & O_D100) && workMs < 90.f) {
        int ddHold_ = !BRIEF_FREEZE_OLD && (bfMode_ == BF_BLIT || bfMode_ == BF_HIDE);   // брифинг показывает снимок или ничего: кадр дёшев не из-за дальности, среднюю не трогаем (иначе дальность вернётся к 250 м, и после ✕ снова 4 с рывков)
        static float ddDev = 0.f;   // №538 (тур 13, L5 m6: среднее 32 мс при цели 31,2, p95 35, 50 пропусков): регулятор смотрит не на среднее, а на среднее + K x среднее отклонение (оценка p90–95)
        if (!ddHold_) { ddDev += (fabsf(workMs - ddAvg) - ddDev) * .08f; ddAvg += (workMs - ddAvg) * .08f; }
        float ddEff_ = DD_SIGMA_OLD ? ddAvg : ddAvg + DD_SIGMA_K * ddDev;
        // разбор тура 12 (L3 sr3, L5 m6 э2/э4): после ✕ работа 45–112 мс при 250 м, спуск 20 м/с занимал 4 с. Скорость спуска растёт с превышением бюджета: до DD_DOWN_X раз (подъём прежний, 6 м/с)
        float ddRate_ = DD_FAST_OLD ? DD_DOWN : DD_DOWN * fminf(ddSteadyOld ? DD_DOWN_X : DD_STEADY_X, 1.f + fmaxf(0.f, ddEff_ - tgt_) / DD_DOWN_K);
        if (ddHold_) { }
        else if (ddEff_ > tgt_ + (ddSteadyOld ? .3f : DD_STEADY_DOWN)) ddScale -= ddRate_ * dt / base_;
        else if (ddEff_ < tgt_ - (ddSteadyOld ? DD_HYST : DD_STEADY_UP)) { if (ddReg1244Off || stMissNow <= 0) ddScale += (ddSteadyOld ? DD_UP : DD_STEADY_RISE) * dt / base_; }   // №1244: рост дальности ждёт, пока видимые клетки догружены
        // мягкий пол (TEST22, слово Роба «и даль, и плавность»): регулятор держится не ниже ddFloorLong() (200 м у уровня с дальними версиями кусков), но если кадр сверх бюджета больше чем на DD_SOFT_OVER мс — может опуститься до прежних 150 м и вернуться к мягкому полу только когда дальность снова ≥ его (подъём — прежним правилом с запасом DD_STEADY_UP). DD_SOFT_FLOOR_OLD=1 — жёсткий пол 200 (TEST21)
        float soft_ = (longView && !DD_SOFT_FLOOR_OLD) ? ddFloorLong() : floor_;
        static int ddSoftLow;
        if (!DD_SOFT_FLOOR_OLD && longView) { if (ddEff_ > tgt_ + DD_SOFT_OVER) ddSoftLow = 1; else if (ddScale >= soft_ / base_) ddSoftLow = 0; }
        else ddSoftLow = 0;
        float lo_ = ddSoftLow ? floor_ : (soft_ > floor_ ? soft_ : floor_);
        if (ddScale < lo_ / base_) ddScale = lo_ / base_; if (ddScale > 1.f) ddScale = 1.f;
        if (smkOn) { static float ddLogT_; ddLogT_ += dt; if (ddLogT_ >= 1.f) { ddLogT_ = 0.f; char l_[150]; snprintf(l_, sizeof l_, "DDREG t=%.1f dd=%.0f scale=%.3f work=%.1f eff=%.1f tgt=%.1f floor=%.0f miss=%d set=%.0f mem=%.2f thr=%.0f ceil=%.0f ad=%.0f\n", smkT, drawDist, ddScale, workMs, ddEff_, tgt_, lo_, (int)stMissNow, ddCeilSet, memLadDdMul, cellThrashEvT ? (sceKernelGetSystemTimeLow() - cellThrashEvT) / 1e6f : 999.f, cellThrashCeil, sqrtf((cellThrashPx - cellThrashAx) * (cellThrashPx - cellThrashAx) + (cellThrashPz - cellThrashAz) * (cellThrashPz - cellThrashAz))); smkWrite(l_); } } }
    }

    curFb = (unsigned *)((unsigned)sceGeEdramGetAddr() + (unsigned)fbp0); BBP(BBP_VBLANK); smkFrameAbs++; if (diagOn) { diagFrameEnd();
      if (!inR && haveCol && ggK.tvy > 0.f) { float gy_, n_[3]; if (colGround(&world, ggE[0], ggE[2], ggE[1] + .5f, ggE[1] - 30.f, &gy_, n_)) { float h_ = ggE[1] - gy_; diagGndN++; if (h_ < 2.5f) { diagLowN++; if (!ggOn) diagLowOff++; } if (h_ < diagGndMin) diagGndMin = h_; } } }   // №674: высота глаза над землёй; ближняя плоскость NEAR_Z пересекает землю ниже ~2,2 м
#if !SHR_REL_SMK
    if (smkOn) flickStep(curFb, fb16, BUF_W, (const void *)((unsigned)sceGeEdramGetAddr() + (unsigned)zbp), camZn, drawDist + 10.f);
#endif
    if (smkOn) smokeShotDump(curFb, fb16, BUF_W, clearChk);   // замер-тур (03.10): снимок «shot» на карту, на железе — сама игра
    int missed = 0, per = longView ? 2 : 1;             // целевой период: дальний режим — 2 обратных хода (ровные 30 FPS), и в P3 тоже (аудит §10.5)
    smkVblMs = 0.f;
    if (!bpass || bVsync) {
      unsigned tv0 = sceKernelGetSystemTimeLow();
      do sceDisplayWaitVblankStart(); while ((int)(sceDisplayGetVcount() - lastVc) < per);
      smkVblMs = (sceKernelGetSystemTimeLow() - tv0) / 1000.f;   // ME-доля: ожидание обратного хода
      unsigned vc = sceDisplayGetVcount(); missed = (int)(vc - lastVc) > per; lastVc = vc;
    }
    { extern void mePeakStep(float, float, int, int, char *, int); char ml[160]; mePeakStep(dt, workMs, fxCount(), hnrActive, ml, sizeof ml);
      if (ml[0] && smkOn) smkWrite(ml); }   // ME-доля: триггер включения ME в пике
    if (smkOn) loFallWatch(dt);   // ag/loadorder
    smkCpuNow = cpuMs; smkSetState(mvBriefing() ? 2 : transitBusy() ? 3 : uiPaused() ? 5 : 0); if (bpass) benchAfter(workMs, cpuMs, missed); else smokeFrame(dt, workMs, missed, per, stMissNow);   // счётчики приёмки (viewer/smoke.h)
    if (smkOn && bubShotN > 0 && uiBubRiseFr && smkFrameAbs >= uiBubRiseFr + (unsigned)bubShotSkip) { char n_[24]; snprintf(n_, sizeof n_, "bs_%05u", smkFrameAbs - 1); smokeShotNow(n_); bubShotN--; }   // №1247 (лист Роба): flag bubshots N [пропуск кадров после подъёма]
    fbp0 = swapBufG(-1); firstCount();   // правка 293: в городе смена только считается (swapGameLateN), без ожидания
    if (rfCapture) {
      unsigned renderUs = t4 - rfRenderStart; char log[360], name[24];
      snprintf(name, sizeof name, "rf_%s_%d", smkScene, rfPass);
      snprintf(log, sizeof log, "RF_FRAME sc=%s pass=%d mask=%d batch=%d frame=%ld t=%.6f hash=%08x eye=%.6f,%.6f,%.6f dir=%.6f,%.6f,%.6f fovy=%.6f near=%.6f "
        "tris=%d draws=%d binds=%d calls=%u dlskip=%u sppose=%u spcut=%u gglist=%u render_us=%u cars=%d park=%d peds=%d\n",
        smkScene, rfPass, rfOld, ggBatchOld, frames, smkT, rfHash, ex,ey,ez,cfx,cfy,cfz,gFov,frameNear(inR,fly||bpass),drawnTris,draws,binds,dlCallsFrame,rfDlSkip,rfSpPose,rfSpCut,rfGgListF,renderUs,traffic.drawnCars,traffic.drawnPark,traffic.drawnPeds);
      smkWrite(log);
      if (smokeIsEmu()) smokeShotNow(name);
      else { strcpy(smkDumpName, name); smokeShotDump(curFb, fb16, BUF_W, clearChk); }
      if (++rfPass < 5) goto rf_repeat_draw;
      rfFreezeRestore(); rfFreezeFree(); rfCapture = 0; ggBatchOld=rfSavedBatchOld;ggBatchCapture=0; stripsCapture = 0; stripsAsList = 0;rfOld = rfSavedOld; hud = rfSavedHud; dt = rfSavedDt;
    }
    if (levelHold && !ldOn) {   // правка 155: первый кадр игры после газеты и ролика — удержанное выбросить (звук уровня ставит levelAudioStart)
      levelHold = 0; int n = audioHoldStreams(levelAudioPending ? -1 : 0);
      if (smkOn) { char s[80]; snprintf(s, sizeof s, "SNDHOLD off held=%d drop=%d t=%.1f\n", n, levelAudioPending, smkT); smkWrite(s); } }
    if (levelAudioPending && !ldOn) { levelAudioPending = 0; levelAudioStart(); }
    { static unsigned audT; unsigned now_ = sceKernelGetSystemTimeLow(); if (smkOn && now_ - audT >= 1000000u) { audT = now_;   // правка 279: путь музыки
        char a_[260]; int l_ = snprintf(a_, sizeof a_, "AUDV t=%.1f mode=%d ", smkT, sndModeCur()); l_ += audioMusicState(a_ + l_, sizeof a_ - l_);
        l_ += snprintf(a_ + l_, sizeof a_ - l_, " uipause=%d svm=%d ", uiReady ? uiPaused() : -1, svmActive()); sndModeDebug(a_ + l_, sizeof a_ - l_);
        strncat(a_, "\n", sizeof a_ - strlen(a_) - 1); smkWrite(a_);
        l_ = snprintf(a_, sizeof a_, "AUDQ t=%.1f sc=%s ", smkT, smkScene); audioQualityLine(a_ + l_, sizeof a_ - l_);   // правка 410: треск и цена ME
        strncat(a_, "\n", sizeof a_ - strlen(a_) - 1); smkWrite(a_);
        l_ = snprintf(a_, sizeof a_, "AUDH t=%.1f sc=%s ", smkT, smkScene); audioHandLine(a_ + l_, sizeof a_ - l_);   // №1100: рукопожатие с ME
        strncat(a_, "\n", sizeof a_ - strlen(a_) - 1); smkWrite(a_); } }
    if (mvBriefing() && !uiPaused()) {   // правка 25: брифинг на экране с этого кадра — нажатия считаются с этой метки пульта
      if (!briefShown) { SceCtrlData q; sceCtrlPeekBufferPositive(&q, 1); briefSince = q.TimeStamp; briefShown = 1; }
    } else { briefShown = 0; briefSince = 0; }

    if (uiReady) uiBriefExitGo();   // №633
    frames++;
    static int secPh;   // №913: следующий кусок журнала секунды (0 — нет; 1–5)
    if (t4 - acc >= 1000000) { float sec_ = (t4 - acc) * 1e-6f; fps = frames * 1000000.f / (t4 - acc); frames = 0; acc = t4; shownMax = workMax; workMax = 0;
      BBP(BBP_SECOND); if (CRASHNOW_OK && crashNowAt && (int)(sceKernelGetSystemTimeLow() - crashNowAt) >= 0) { crashNowAt = 0; bmMark("crashnow"); bbOp("crashnow", "null-store", 1); if (smkOn) smkWrite("CRASHNOW fire\n"); *(volatile int *)0 = 1; }   // №1208в: запись по нулевому адресу
      geSecReport(); gePeakShown = gePeakSec; gePeakSec = 0; blackBoxSecond(sec_);   // правка 103: пик списка GE за секунду; чёрный ящик — кольцо в памяти
      { char pca_[12]; if (inR) { const RoomDoor *d_ = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0]; placeCode(d_->p[0], d_->p[2], pca_, sizeof pca_); } else placeCode(x, z, pca_, sizeof pca_); acctSecond(pca_); }   // учёт ACCT раз в секунду
      secPh = 1;   // №913: диагностика секунды кусками по кадрам (smoke.h SMK_SPREAD_OLD=1 — одним кадром, как было)
    }
    if (secPh) { static unsigned secAcc_, secMax_; int secDone_ = 0; unsigned sb0_ = sceKernelGetSystemTimeLow(); int secAll_ = SMK_SPREAD_OLD || !smkOn; ;   // одна пятая журнала секунды на кадр (первая в кадре границы секунды): цена всего блока ≈ 19 мс PSP не помещалась в запас vbl 6–7 мс и давала пропуск раз в секунду
      if (!secDone_ && (secAll_ || secPh == 1)) { secPh = 2; secDone_ = !secAll_;
      if (smkOn && thDiag && thCalls) { char h_[160]; snprintf(h_, sizeof h_, "TOONHULL t=%.1f scene=%s per_s calls=%u items=%u flushes=%u words=%u us=%u push_us=%u flush_us=%u fps=%.1f\n", smkT, smkScene, thCalls, thQItems, thQFlush, thWords, thUs, thUsPush, thUsFlush, fps); smkWrite(h_); { char o_[100]; snprintf(o_, sizeof o_, "OUTLINE t=%.1f var=%d skipB=%u hit=%u miss=%u\n", smkT, carOutline, carOutlineSkipB, carOutlineHit, carOutlineMiss); smkWrite(o_); carOutlineSkipB = carOutlineHit = carOutlineMiss = 0; } } thCalls = thWords = thUs = thQItems = thQFlush = thUsPush = thUsFlush = 0;   // №912 диагностика
      smokeSecond(fps, workMs, shownMax, cpuMs, drawnTris, draws, loadedBytes, longView); if (smkOn && ggLowH > 0.f) { char g3_[120]; snprintf(g3_, sizeof g3_, "GGLOW t=%.1f sc=%s h=%.1f frames_on=%u\n", smkT, smkScene, ggLowH, ggLowFrames); smkWrite(g3_); ggLowFrames = 0; } if (diagOn) { char g2_[200]; snprintf(g2_, sizeof g2_, "DIAGG t=%.1f sc=%s fr=%u frames=%u eyeH_lt2.5=%u of_which_ggOff=%u eyeH_min=%.2f\n", smkT, smkScene, smkFrameAbs, diagGndN, diagLowN, diagLowOff, diagGndMin); smkWrite(g2_); diagGndN = diagLowN = diagLowOff = 0; diagGndMin = 99.f; diagSecond(); }
      }
      if (!secDone_ && (secAll_ || secPh == 2)) { secPh = 3; secDone_ = !secAll_; smokeSecondB(); }   // PHDRAW
      if (!secDone_ && (secAll_ || secPh == 3)) { secPh = 4; secDone_ = !secAll_; smokeSecondC(); }   // PHSUB и сброс сумм секунды
      if (!secDone_ && (secAll_ || secPh == 4)) { secPh = 5; secDone_ = !secAll_;
      if (smkOn) { char g_[300]; snprintf(g_, sizeof g_, "GATECAT t=%.1f sc=%s fr=%d geo city=%u car=%u npc=%u trf=%u mcr=%u props=%u fx=%u kt car=%u npc=%u trf=%u props=%u untex car=%u npc=%u trf=%u props=%u cars=%d park=%d peds=%d skin=%u skinbad=%u\n", smkT, smkScene, (int)(fps + .5f),
          gateGeo[BBP_CITY], gateGeo[BBP_CAR3D], gateGeo[BBP_NPC3D], gateGeo[BBP_TRF3D], gateGeo[BBP_MCR3D], gateGeo[BBP_PROPS3D], gateGeo[BBP_FX3D],
          gateTri[BBP_CAR3D] / 1000, gateTri[BBP_NPC3D] / 1000, gateTri[BBP_TRF3D] / 1000, gateTri[BBP_PROPS3D] / 1000,
          gateUntex[BBP_CAR3D], gateUntex[BBP_NPC3D], gateUntex[BBP_TRF3D], gateUntex[BBP_PROPS3D], traffic.drawnCars, traffic.drawnPark, traffic.drawnPeds, gateSkinDraws, gateSkinBad);
        gateSkinDraws = gateSkinBad = 0; smkWrite(g_); memset(gateGeo, 0, sizeof gateGeo); memset(gateTri, 0, sizeof gateTri); memset(gateUntex, 0, sizeof gateUntex); }
      }
      if (!secDone_ && (secAll_ || secPh == 5)) { secPh = 6; secDone_ = !secAll_;
      if (smkOn && !RBLD_OLD && rbldN) {   // правка 500, шаг А1: из чего состоит сборка списка GE (мс на кадр, подфазы по отметкам BBP)
        double k_ = 1.0 / rbldN; char s_[260];
        snprintf(s_, sizeof s_, "RBLD t=%.1f sc=%s n=%u city=%.2f car=%.2f npc=%.2f trf=%.2f mcr=%.2f props=%.2f fx=%.2f ui=%.2f cull_calls=%u vis_calls=%u drawpiece=%u drawpieceM=%u\n", smkT, smkScene, rbldN,
                 rbldSum[BBP_CITY] * k_, rbldSum[BBP_CAR3D] * k_, rbldSum[BBP_NPC3D] * k_, rbldSum[BBP_TRF3D] * k_, rbldSum[BBP_MCR3D] * k_,
                 rbldSum[BBP_PROPS3D] * k_, rbldSum[BBP_FX3D] * k_, rbldSum[BBP_UI] * k_, rbcCull / rbldN, rbcVis / rbldN, rbcDraw / rbldN, rbcDrawM / rbldN);
        smkWrite(s_); }
      if (smkOn && !UIPH_OLD && uiPhN) {   // подфазы BBP_UI (мс на кадр) и разбивка кадра HUD по типам и тегам (ui.c uiProfTake)
        char s_[420]; double k_ = 1.0 / uiPhN / 1000.0; int l_ = snprintf(s_, sizeof s_, "UIPH t=%.1f sc=%s n=%u", smkT, smkScene, uiPhN);
        for (int i = 0; i < UIPH_N; i++) l_ += snprintf(s_ + l_, sizeof s_ - (size_t)l_, " %s=%.2f", UIPH_NAMES[i], uiPhUs[i] * k_);
        snprintf(s_ + l_, sizeof s_ - (size_t)l_, "\n"); smkWrite(s_);
        char u_[700]; if (uiProfTake(u_, sizeof u_)) smkWrite(u_);
        char m_[320]; if (mvProfTake(m_, sizeof m_)) smkWrite(m_);
        memset(uiPhUs, 0, sizeof uiPhUs); uiPhN = 0; }
      memset(rbldSum, 0, sizeof rbldSum); rbldN = 0; rbcCull = rbcVis = rbcDraw = rbcDrawM = 0;
      }
      if (!secDone_ && (secAll_ || secPh == 6)) { secPh = 7; secDone_ = !secAll_;
      if (smkOn && smkT > 12.f) fdDump();
      if (smkOn && rfFrames) { char s[768]; unsigned f_ = rfFrames;   // render_fast: замер раз в секунду (среднее на кадр; tests/render/rfast.py)
        snprintf(s, sizeof s, "RFAST t=%.1f sc=%s old=%d fps=%.1f work_ms=%.2f cpu_ms=%.2f ktris=%.1f draws=%d binds=%d ge=%u calls=%u fall=%u cull_us=%u sp_us=%u sp_pose=%u sp_cut=%u sp_small=%u dl_skip=%u gg_list=%u gg_cells_all=%u cull_reuse=%u/%u pv2=%u dd=%.0f hyb_cells=%u hyb_saved_kt=%.1f pb_calls=%u pb_built=%u pb_fall=%u pb_kb=%u b1_us=%u ui_us=%u warm=%u/%u lwarm=%u/%u\n",
                 smkT, smkScene, rfOld, fps, workMs, cpuMs, drawnTris / 1000.f, draws, binds, gePeakShown * 100u / GE_BUDGET, dlCallsFrame, dlFallFrame,
                 rfCullUs / f_, rfSpUs / f_, rfSpPose / f_, rfSpCut / f_, spSmallCut / f_, rfDlSkip / f_, rfGgList / f_, ggCellsAll, rfCullReuse, rfCullAll, rfPv2Frames, drawDist, rfHybCells / f_, rfHybSaved / f_ / 1000.f, pbCalls, pbBuilt, pbFallback, pbBytes >> 10, rfPbUs / f_, rfUiUs / f_, rfWarmBuilt, rfWarmBaked, rfLoadWarmBuilt, rfLoadWarmBaked);
        smkWrite(s);
        if (lstFrames) { char l_[200]; snprintf(l_, sizeof l_, "LBIND t=%.1f sc=%s flat=%d list_binds=%.1f imm_binds=%.1f flat_binds_imm=%.1f frames=%u noise_binds_total=%u texmat_emits=%u marks_calls=%u marks_tris=%u\n", smkT, smkScene, flatOn, lstSumB / (float)lstFrames, lstImm / (float)lstFrames, lstFlatImm / (float)lstFrames, lstFrames, flatNoiseN, flatTexEmitN, marksCalls, marksTrisDrawn); smkWrite(l_); lstSumB = lstImm = lstFlatImm = 0; lstFrames = 0; } }
      if (smkOn && rfFrames) { char s[300]; unsigned f_ = rfFrames;   // TEST24 (№637): куски города мимо листа по причинам, на кадр: штук/мкс (tq=полупрозрачное, far=дальние без листа, hyb=гибрид, fall=лист не собран, risky=срез у камеры, gg=клетка целиком)
        snprintf(s, sizeof s, "DCITY t=%.1f sc=%s tq=%u/%u far=%u/%u hyb=%u/%u fall=%u/%u risky=%u/%u gg=%u/%u farlist=%u farbuild=%u/%u dlcity=%u stat=%u\n", smkT, smkScene,
                 dcN[0] / f_, dcUs[0] / f_, dcN[1] / f_, dcUs[1] / f_, dcN[2] / f_, dcUs[2] / f_, dcN[3] / f_, dcUs[3] / f_, dcN[4] / f_, dcUs[4] / f_, dcN[5] / f_, dcUs[5] / f_, farListCalls / f_, dcN[6] / f_, dcUs[6] / f_, dcUs[7] / f_, dcUs[8] / f_);
        smkWrite(s); }
      if (smkOn && collVerFrames) { char s[160]; snprintf(s, sizeof s, "COLLVER t=%.1f sc=%s frames=%u miss=%u builds=%u hits=%u\n", smkT, smkScene, collVerFrames, collVerMiss, collNearBuilds, collNearHits); smkWrite(s); collVerFrames = collVerMiss = collNearBuilds = collNearHits = 0; }
      memset(dcN, 0, sizeof dcN); memset(dcUs, 0, sizeof dcUs); farListCalls = 0;
      rfFrames = rfCullUs = rfSpUs = rfSpPose = rfSpCut = rfDlSkip = rfGgList = 0; spSmallCut = 0; rfPbUs = rfUiUs = 0; rfPv2Frames = 0; rfCullReuse = rfCullAll = pbCalls = rfHybCells = rfHybSaved = 0;
      }
      if (!secDone_ && (secAll_ || secPh == 7)) { secPh = 8; secDone_ = !secAll_;
      if (smkOn && meProf && mpFrames) { char s[256]; unsigned f_ = mpFrames; float dsp_ = 0, mix_ = 0; audioDspUs(&dsp_, &mix_);   // правка 313: замер
        char fm_[40]; fxMeStateStr(fm_, sizeof fm_); unsigned jus_, jn_; fxProfJob(&jus_, &jn_); float kc_ = audioKernCopyUs();
        snprintf(s, sizeof s, "MEPROF t=%.1f frames=%u fx=%u/%u upd_us=%u/%u draw_us=%u/%u fxjob_us=%u tq=%u sort_us=%u tqall_us=%u mixbuf_us=%.0f dsp_us=%.0f kcopy_us=%.0f cpu_ms=%.1f mefx=%s\n",
                 smkT, f_, mpFx / f_, mpFxMax, mpUpd / f_, mpUpdMax, mpDraw / f_, mpDrawMax, jn_ ? jus_ / jn_ : 0, mpTqn / f_, mpSort / f_, mpTqd / f_, mix_, dsp_, kc_, cpuMs, fm_);
        smkWrite(s); mpFrames = mpSort = mpTqn = mpTqd = mpUpd = mpUpdMax = mpDraw = mpDrawMax = mpFx = mpFxMax = 0; }
      { static int huskLat; if (car.destroyed) huskLat = 1; if (smkOn && huskLat) { char s[128]; snprintf(s, sizeof s, "CARHUSK t=%.1f pos=%.2f,%.2f,%.2f v=%.2f driving=%d boomed=%d huskDrive=%d destroyed=%d dmg=%.2f\n", smkT, car.x, car.y, car.z, car.v, !onFoot(), carBoomed(&car), car.huskDrive, car.destroyed, car.damage); smkWrite(s); } }   // №355: проверка «в остов сели и он едет»
      if (smkOn && onFoot()) { char s[128]; snprintf(s, sizeof s, "HOMER t=%.1f pos=%.2f,%.2f,%.2f h=%.3f room=%s cam=%s\n", smkT, homer.x, homer.y, homer.z, homer.h, roomName() ? roomName() : "-", inRoom() && roomCamOn && rcs.cur >= 0 ? rcam[rcs.cur].name : "-"); smkWrite(s); }     // 11.2: где Гомер пешком
      }
      if (secPh > 7) secPh = 0;
      if (smkOn) { unsigned sl_ = sceKernelGetSystemTimeLow() - sb0_; secAcc_ += sl_; if (sl_ > secMax_) secMax_ = sl_; if (!secPh) { char sb_[100]; snprintf(sb_, sizeof sb_, "SECBLK t=%.1f us=%u max=%u\n", smkT, secAcc_, secMax_); smkWrite(sb_); secAcc_ = secMax_ = 0; } } }   // us — всё за секунду, max — самый тяжёлый кадр (гейт tests/smk913)
    if (trLog && haveTraffic && smkOn && (trLogT += wallDt) >= 1.f) { char s[256];   // этап 5: строка трафика раз в секунду (flag trflog, tests/traffic/emu.sh);
      trLogT = 0.f;                                 // ключ=значение — smoke_report её пропускает; не в кадре строки smokeSecond (запись файла — время кадра)
      snprintf(s, sizeof s, "TRF t=%.1f cars=%d moving=%d drawn=%d lod=%d park=%d parkdrawn=%d awake=%d peds=%d pedsdrawn=%d tris=%d group=%d pedmin=%.2f onfoot=%d homer=%.2f,%.2f car=%.2f,%.2f\n",
               smkT, traffic.activeCars, traffic.movingCars, traffic.drawnCars, traffic.lodCars, traffic.npark, traffic.drawnPark, traffic.awakePark,
               traffic.activePeds, traffic.drawnPeds, traffic.drawnTris, traffic.group, trPedMin, onFoot(), homer.x, homer.z, car.x, car.z);
      smkWrite(s); }
    if (++hudTick >= 15) {                           // текст счётчика обновляем 4 раза в секунду
      hudTick = 0; char s[128], pc[12];
      hudClear();
      if (inR) { const RoomDoor *d = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0]; placeCode(d->p[0], d->p[2], pc, sizeof pc); }   // в комнате — у её двери (как радар)
      else placeCode(x, z, pc, sizeof pc);
      snprintf(s, sizeof s, "L%d %s  FPS %4.1f  tris %4.1fk  draws %d  RAM %.1fMB  %s %3.0f km/h", gameLevel, pc, fps, drawnTris / 1000.f, draws,
               loadedBytes / 1048576.f, inR ? "ROOM" : fly ? "FLY" : haveCol ? "CAR+COL" : "CAR", fabsf(v) * 3.6f);
      hudLine(0, 0xffffffff, s);
      snprintf(s, sizeof s, "work %4.1f (max %4.1f) cpu %4.1f %s %s %3.0fm free%.1fM SEL", workMs, shownMax, cpuMs, pvsUsed ? "PVS" : "",
               longView ? "30FPS" : "60FPS", drawDist, smokeHeapFree());   // честный запас кучи (12б)
      hudLine(1, 0xffffffff, s);
      if (haveCoins && !uiReady) {   // временный счётчик — только если нет ui.shr (родной HUD рисует монеты и урон)
        char t[64]; snprintf(t, sizeof t, "COINS %d / %d   DMG %3.0f%%", coins.collected, coins.total, car.damage * 100.f);
        snprintf(s, sizeof s, "%*s", 60, t); hudLine(33, 0xff40d0ff, s);
      }
      if (onFoot()) {   // этап 4: подсказка пешком
        static const char *hs[] = {"FOOT", "KICK", "STOMP", "HURT", "TO DOOR", "GET IN", "CAR", "GET OUT"};
        snprintf(s, sizeof s, "HOMER %s %4.1f m/s  X:jump []:kick O:run X-X-[]:stomp /\\:action", hs[homer.state], homer.speed);
        hudLine(3, 0xff80ff80, s);
      }
      hudLine(2, 0xffc0c0c0, fly ? "stick:look dpad:move L/R:down/up X:fast  /\\:car START:reset"
                                 : uiReady ? "R/X:gas L/O:brake/back []:handbrake /\\:out SELECT:reset START:pause"
                                           : "R/X:gas L/O:brake/back []:handbrake /\\:out START:reset");
      if (bpass) {
        static const char *pn[] = {"", "P1 whole level", "P2 variants", "P3 vsync"};
        int tot = bpass == 1 ? nbp * 4 : bpass == 2 ? nw2 : nw3;
        snprintf(s, sizeof s, "BENCH %s  %d / %d %s", pn[bpass], bview, tot, bpass == 2 ? VAR[bvar].name : "");
        hudLine(4, 0xff80ffff, s);
      } else if (brep[0][0]) { hudLine(4, 0xff80ffff, brep[0]); hudLine(5, 0xff80ffff, brep[1]); }
      if (hud) {   // правка 103: список GE (кадр и пик секунды, % от 256 КБ), пропуски, наибольший свободный кусок кучи, отказы памяти
        snprintf(s, sizeof s, "GE %2u%% pk %2u%% skip %u bad %u  blk %.1fM  fail %u/%u", geNow * 100u / GE_BUDGET,
                 gePeakShown * 100u / GE_BUDGET, smkGeSkip, smkGeBad, memMaxBlockMB(), memFailsNow(), smkAllocFail);
        hudLine(7, 0xffffff80, s);
      }
      if (haveTraffic) {   // этап 5: трафик (служебный счётчик)
        snprintf(s, sizeof s, "TRF car %d mv %d dr %d  park dr %d  ped %d/%d  %.1fk",   // <= 60 колонок
                 traffic.activeCars, traffic.movingCars, traffic.drawnCars, traffic.drawnPark, traffic.drawnPeds, traffic.activePeds, traffic.drawnTris / 1000.f);
        hudLine(6, 0xffffc080, s);
      }
      if (!hud) hudClear();                                  // счётчик скрыт (R+SELECT): остаётся только строка трамплина
    }
  }
  return 0;
}
