// Миссии в EBOOT: интерфейс, персонажи, маркеры (см. msn_view.h). Логика миссий — game_msn.c (общая с ботами).
#include <pspkernel.h>
#include "power.h"   // 107б P0: эпоха питания
#include <pspgu.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "msn_view.h"
#include "msn_wager.h"
#include "msn_rules.h"
#include "mission/msn_cosm1264.h"
#ifndef BRIEF_ART_PRELOAD_OLD
#define BRIEF_ART_PRELOAD_OLD 0   // 1 — карточка по готовности данных картинки, как в TEST17
#endif
#ifndef BRIEF_ANIM_AT_LOAD_OLD
#define BRIEF_ANIM_AT_LOAD_OLD 0   // 1 — таймер въезда карточки идёт с открытия брифинга (как в TEST17)
#endif
#include "clipguard.h"
float camNearNow;   // №502 (camnear.h)
unsigned clipSeen, clipDrawnClose, clipHid; int clipSmallOn;   // clipguard.h: счётчики за секунду
float camFwdN[3] = {0, 0, 1};   // clipguard.h: нормированный взгляд кадра
#ifndef NEAR_CULL_OLD
#define NEAR_CULL_OLD 0
#endif
int camNearOld = NEAR_CULL_OLD;     // №602: прежние пороги исчезновения у глаза (NEAR_CULL_OLD=1)
#include "game_msn.h"
#include "skin.h"
#include "xsub.h"   // служебно (Модели-96, замер x_npc): участки mvDraw3D 50–57 в строке XSUB
#include "ui.h"
#include "watchdog.h"
#include "msnart.h"
#include "msnres.h"
#include "sound.h"
#include "audio.h"
#include "convcam.h"
#include "shop_view.h"   // пункт 6: экран покупки у продавца (PurchaseRewards.scr)
#include "blackbox.h"   // правка 103: bbOp — самописец чёрного ящика (main.c)
#include "stateprops.h"   // №314: бочки L7 (AddCollectibleStateProp) — экземпляры SPR1
#include "collectfx.h"   // 04.10: эффект цели SetCollectibleEffect (искры wrench_collect и др.)
#include "menu_access.h"   // пункт ДОБИВКИ 26: MISSION SELECT паузы серый, когда выбрать нечего (запись PS2 ps2_access_pause)

static char dir[256];                     // каталог EBOOT со слешем
// «LEVEL COMPLETE» / «GAME COMPLETE» (текст библии, вспышка uiFlashCb 3 с), потом main.c грузит следующий уровень (газета
// loading{N+1}) или, после SetGameOver, главное меню. Сколько держать — в данных нет: правило порта = длительность вспышки
#define MV_LEVEL_DONE_S 3.f
static void *pack; static unsigned packSize;
static GameMsn M; static int active; static unsigned seenTeleports;   // переносы игрока сценарием, уже отданные main.c
static void (*logFn)(const char *);
static float clock_;                       // секунды игры (вспышки HUD)
static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
int rb_MVGETINARROW_OLD __attribute__((weak));   /* 107б rollback.txt (viewer/rollback.h): прежний путь без пересборки */
static void logf_(const char *fmt, ...) {
  if (!logFn) return;
  char s[200]; va_list a; va_start(a, fmt); vsnprintf(s, sizeof s, fmt, a); va_end(a); logFn(s);
}
static int readAll(const char *name, void **out, unsigned *size) {   // файл рядом с EBOOT целиком, выравнивание 64
  char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir, name);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return -1;
  int n = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  void *b = n > 0 ? memalign(64, (size_t)n) : 0;
  if (!b || sceIoRead(f, b, (SceSize)n) != n) { sceIoClose(f); free(b); return -2; }
  sceIoClose(f); *out = b; *size = (unsigned)n; return 0;
}

// ------------------------------------------------------------------ персонажи: модели из данных диска (tools/char_export.py)
// Только уже экспортированные (tools/ship_rules.txt): остальные персонажи уровня есть в логике (разговор работает), но не
// рисуются — в отчёте. Модель грузится, когда сценарий ставит персонажа (AddNPC / AddAmbientCharacter), как в оригинале.
// персонажи скриптов уровней 1–7 (tools/npc_list.py --level N --c; правила сборки — tools/ship_rules.txt); npd — кожи нет; louie — обычные
// списки Pure3D (правка 180, char_export.generic_strip)
static const char *const npcFiles[] = {"chars/apu.chr", "chars/barney.chr", "chars/burns.chr", "chars/carl.chr", "chars/cletus.chr", "chars/eddie.chr", "chars/gil.chr", "chars/grandpa.chr", "chars/krusty.chr", "chars/lenny.chr", "chars/lisa.chr", "chars/louie.chr", "chars/marge.chr", "chars/milhouse.chr", "chars/moe.chr", "chars/moleman.chr", "chars/ned.chr", "chars/nelson.chr", "chars/patty.chr", "chars/ralph.chr", "chars/selma.chr", "chars/willie.chr",
  // уровни 2–7 (tools/npc_list.py --level N); Dolph includes native triangle-list hair
  "chars/bart.chr", "chars/dolph.chr", "chars/beeman.chr", "chars/brn_unf.chr", "chars/captain.chr", "chars/cbg.chr", "chars/frink.chr", "chars/hibbert.chr", "chars/jasper.chr", "chars/jimbo.chr", "chars/lou.chr", "chars/nriviera.chr", "chars/otto.chr", "chars/skinner.chr", "chars/smithers.chr", "chars/snake.chr", "chars/teen.chr", "chars/wiggum.chr", "chars/zfem1.chr", "chars/zmale1.chr", "chars/zmale3.chr",
  "chars/kearney.chr", "chars/homer.chr", "chars/male2.chr"};   // №314 п.1: male2 — водитель cNerd (L2, L3) и cCola (L4, L5); модель диска art/chars/male2_m.p3d   // перепись миссий 04.10: Кирни (L2, L4, L6) и Гомер-персонаж (L2/L5 продавец, L6 m7, водитель sr2/sr3)
#define NMODEL 8   // правка 171 (01.10): модели персонажей ~160 КБ после 143(а) — 8 слотов ≈ 1.3 МБ (было 4)
#define NMODEL_AMB 5   // фоновым персонажам (AddAmbientCharacter) — не больше 5 моделей (было 2, правка 171): остальные — персонажам миссий
                       // (правило порта, память PSP: модель 0.2–0.4 МБ, фоновых на уровне 1 — 11; на PS2 грузятся все)
static struct { char name[24]; SkinModel *m; int amb; } model[NMODEL];
// Разбор тура 12 (11.10): чтение модели персонажа с карты памяти ~21–28 мс прямо в кадре (1–3 подгрузки на этап, каждая — кадр 40–70 мс). Теперь чтение
// уходит в фон (skinPreStart, sceIoReadAsync), модель встаёт, когда файл прочитан (разбор — доли мс); персонаж миссии без модели ждёт в очереди (mvPrefetchPoll
// каждый кадр). На экране загрузки (mvPump) читаем сразу, как раньше. STAGE_PREFETCH_OLD=1 — прежнее (чтение в кадре).
#ifndef STAGE_PREFETCH_OLD
#define STAGE_PREFETCH_OLD 0
#endif
#define NPRE 4
static struct { char name[24]; SkinPre *p; unsigned t0; } pre[NPRE];
static char npcPend[8][24]; static int nNpcPend;   // персонажи миссии, чья модель ещё читается: animFor(create) повторяется каждый кадр
#ifndef NPD1361_OLD
#define NPD1361_OLD 0
#endif
int mvNpd1361 = !NPD1361_OLD;
static int preFail;   // modelFor: файл не открылся / повреждён — повторять нечего
// 1344: служебная задержка чтения модели персонажа (flag npcdelay МС): модель считается прочитанной не раньше, чем через МС после начала фонового чтения (медленная карта: красный контроль ожидания персонажей комнаты)
unsigned mvNpcDelayUs;
#define NPCDELAY1344(pi) (mvNpcDelayUs && (unsigned)(sceKernelGetSystemTimeLow() - pre[pi].t0) < mvNpcDelayUs)
static int prePending(const char *name) { for (int i = 0; i < NPRE; i++) if (pre[i].p && msn_streq(pre[i].name, name)) return 1; return 0; }
#define NANIM 10
typedef struct { char name[24]; const SkinModel *m; SkinPlayer p; int idle, walk; int talkLine, gest; float gestT;
                 int flail, land, getup, poseTick; float lift; int liftInit; SkinMouth mouth; } NpcAnim;   // правка 240: сбит машиной — клипы рига flail / fall_land / get_up (-1 — нет); №510: lift — подъём стоп
static NpcAnim *anim[NANIM];
static void dropModel(int k) {   // модель и все её анимации — из кучи (слот фонового персонажа отдан персонажу миссии)
  bbOp("npc-", model[k].name, 0);   // правка 103: самописец
  for (int i = 0; i < NANIM; i++) if (anim[i] && anim[i]->m == model[k].m) { free(anim[i]); anim[i] = 0; }
  skinFree(model[k].m); model[k].m = 0; model[k].name[0] = 0; model[k].amb = 0;
}
static void dropModel(int k);
static int preStartBlocked(void);
// №1235 (Роб, видео тестера IMG_7226, последняя миссия L1: «Бернса нет, ни модели, ни тени, он невидимый»): модель персонажа миссии читалась мимо лестницы памяти (канон п.13) и, не найдя
// цельного куска (куча разбита 40 минутами игры), молча оставалась пустым местом: preFail = 1, опрос сдавался навсегда. Теперь (1) перед чтением — memLadder (просьба нужна по размеру файла;
// персонажу миссии высший приоритет, фоновому — только ступени впрок), (2) отказ из-за памяти (файл есть) не окончателен: повтор по опросу (раз в 0,12 с, до 60 с), (3) после NPC_SUBST_N неудач
// персонажу миссии отдаётся уже загруженная модель другого персонажа (разговор и тень на месте, вид не пустой); как только слот заменённой модели выгружен, запись сброшена и читаем заново.
// NPCLAD1235_OLD=1 / flag npclad1235 0 / файл npclad1235old — как было (молча пустое место). Строки журнала: MSNNPC mem-fail / subst / ladder
#ifndef NPCLAD1235_OLD
#define NPCLAD1235_OLD 0
#endif
#define NPC_SUBST_N 8
#define NPC_SUBST_N96 3   // NPCRETRY96: отказы идут раз в секунду, замена не позже 3 с
// Build 96 (срочно, Kearney на L4 невидим 36 с при куче как у PSP и 250 м): фоновый опрос звал модель каждые 0,12-0,25 с, лестница и строка журнала шли на каждый опрос. Теперь после отказа
// (чтение не вышло или лестница не нашла места) модель того же имени не пробуется раньше чем через 1 с (npcRetryAt), замена загруженным персонажем - после трёх отказов (было восемь по 0,12 с).
// NPCRETRY96_OLD=1 / flag npcretry96 0 / файл npcretry96old - как было.
#ifndef NPCRETRY96_OLD
#define NPCRETRY96_OLD 0
#endif
int npcRetry96 = !NPCRETRY96_OLD;
#define NPC_RETRY_US 1000000u
int (*mvMemLadder)(const char *who, unsigned need, int top); int npcLad1235 = !NPCLAD1235_OLD;
// 107б P0 (сон и пробуждение, Роб 1273; генплан Fable §P0.3): файл модели персонажа «не найден» (после сна карта ещё не ответила) делал preFail = 1 — опрос сдавался сразу, персонаж не появлялся. Теперь «нет файла» повторяется раз в секунду
// до NPC_MISS_N подряд в одной эпохе питания (замена чужой моделью не включается: файл вернётся), потом как раньше. P0NPC_OLD=1 / файл p0npcold — как было
#ifndef P0NPC_OLD
#define P0NPC_OLD 0
#endif
#define NPC_MISS_N 8
int npcP0 = !P0NPC_OLD;
static struct { char name[24]; unsigned n; int subst; unsigned retryAt, dropN; unsigned miss, missEp; } npcFail[8];   // по имени: сколько отказов по памяти, слот модели-замены (-1 нет)
static int npcFailSlot(const char *name) {
  int fr = -1; for (int i = 0; i < 8; i++) { if (npcFail[i].name[0] && msn_streq(npcFail[i].name, name)) return i; if (!npcFail[i].name[0] && fr < 0) fr = i; }
  if (fr < 0) fr = 0;
  memset(&npcFail[fr], 0, sizeof npcFail[fr]); strncpy(npcFail[fr].name, name, 23); npcFail[fr].subst = -1; return fr;
}
int npcFailSim1235;
static void dropModel(int k);
static int npcFailN(const char *name, unsigned *dropN) {   // сколько отказов у имени (0 - не было); dropN - на каком отказе уже выгружали чужую модель
  for (int i = 0; i < 8; i++) if (npcFail[i].name[0] && msn_streq(npcFail[i].name, name)) { if (dropN) *dropN = npcFail[i].dropN; return (int)npcFail[i].n; }
  return 0;
}
static int npcFailMarkDrop(const char *name) { for (int i = 0; i < 8; i++) if (npcFail[i].name[0] && msn_streq(npcFail[i].name, name)) { npcFail[i].dropN = npcFail[i].n; return 1; } return 0; }
static int npcRoom(const char *fn, int amb) {   // 1 — цельный кусок под файл модели есть (после лестницы)
  if (!npcLad1235 || !mvMemLadder) return 1;
  SceIoStat st; unsigned need = 512u << 10; if (sceIoGetstat(fn, &st) >= 0 && st.st_size > 0) need = (unsigned)st.st_size + (64u << 10);
  int ok = mvMemLadder("npc", need, amb ? 0 : 1); if (!ok) logf_("MSNNPC ladder-none %s need=%u\n", fn, need); return ok;
}
static const SkinModel *npcLoadFail(const char *name, const char *fn, int amb) {   // отказ чтения/разбора: файл есть — память, не окончательно; замена загруженным персонажем
  logf_("MSNNPC load-fail %s\n", fn); bbOp("npc!", name, 0);
  SceIoStat st; int gs_ = npcLad1235 ? (pwrSimFault("npc") ? -1 : sceIoGetstat(fn, &st)) : -1;
  if (npcP0 && npcLad1235 && gs_ < 0) {   // 107б P0: файла нет сейчас — повтор раз в секунду, сдаваться не сразу
    int k_ = npcFailSlot(name); if (npcFail[k_].missEp != pwrEpoch()) { npcFail[k_].miss = 0; npcFail[k_].missEp = pwrEpoch(); }
    if (++npcFail[k_].miss <= NPC_MISS_N) { unsigned t_ = sceKernelGetSystemTimeLow() + NPC_RETRY_US; npcFail[k_].retryAt = t_ ? t_ : 1u; logf_("MSNNPC file-miss %s n=%u (повтор)\n", name, npcFail[k_].miss); return 0; } }
  if (gs_ < 0) { preFail = 1; return 0; }
  int k = npcFailSlot(name); unsigned n = ++npcFail[k].n;
  if (npcRetry96) { unsigned t_ = sceKernelGetSystemTimeLow() + NPC_RETRY_US; npcFail[k].retryAt = t_ ? t_ : 1u; }
  if (n == 1) logf_("MSNNPC mem-fail %s (повтор по опросу)\n", name);
  if (npcRetry96 && !amb && npcFail[k].dropN < n) {   // персонажу миссии нужно место: один фоновый персонаж уходит (его кусок в области такого же размера), вернётся по опросу
    for (int i = 0; i < NMODEL; i++) if (model[i].m && model[i].amb) { logf_("MSNNPC drop-for %s <- %s\n", name, model[i].name); dropModel(i); break; }
    npcFail[k].dropN = n; }
  if (n >= (npcRetry96 ? NPC_SUBST_N96 : NPC_SUBST_N) && !amb && npcFail[k].subst < 0) {
    int best = -1; for (int i = 0; i < NMODEL; i++) if (model[i].m && !msn_streq(model[i].name, name) && (best < 0 || (!model[i].amb && model[best].amb))) best = i;
    if (best >= 0) { npcFail[k].subst = best; logf_("MSNNPC subst %s -> %s\n", name, model[best].name); bbOp("npcS", name, 0); return model[best].m; }
  }
  return 0;
}
static const SkinModel *modelFor(const char *name, int amb) {
  for (int i = 0; i < NMODEL; i++) if (model[i].m && msn_streq(model[i].name, name)) { if (!amb) model[i].amb = 0; return model[i].m; }
  if (npcLad1235) for (int i = 0; i < 8; i++) if (npcFail[i].name[0] && msn_streq(npcFail[i].name, name) && npcFail[i].subst >= 0) {
    if (model[npcFail[i].subst].m) return model[npcFail[i].subst].m;   // замена жива
    npcFail[i].subst = -1; npcFail[i].n = 0; }                          // её выгрузили — читаем свою заново
  int used = 0, ambUsed = 0, freeSlot = -1;
  for (int i = 0; i < NMODEL; i++) { if (model[i].m) { used++; ambUsed += model[i].amb; } else if (freeSlot < 0) freeSlot = i; }
  if (amb && (ambUsed >= NMODEL_AMB || used >= NMODEL)) return 0;          // фоновому — только свободный слот из своих двух
  if (!amb && freeSlot < 0)                                                // персонаж миссии: слот фонового освобождается
    for (int i = 0; i < NMODEL; i++) if (model[i].m && model[i].amb) { dropModel(i); break; }
  if (npcRetry96 && !prePending(name)) {   // после отказа - не раньше чем через секунду (чтение в фоне опрашивается как раньше)
    for (int i = 0; i < 8; i++) if (npcFail[i].name[0] && msn_streq(npcFail[i].name, name)) {
      if (npcFail[i].retryAt && (int)(sceKernelGetSystemTimeLow() - npcFail[i].retryAt) < 0) return 0;
      break; } }
  int inPack1361 = 0;
  for (unsigned k = 0; k < sizeof npcFiles / sizeof *npcFiles; k++) {
    const char *b = npcFiles[k] + 6; size_t n = strlen(name);   // "chars/" + имя + ".chr"
    if (strncmp(b, name, n) || strcmp(b + n, ".chr")) continue;
    inPack1361 = 1;
    for (int i = 0; i < NMODEL; i++) if (!model[i].m) {
      char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir, npcFiles[k]);
      unsigned t0 = sceKernelGetSystemTimeLow();
      if (npcFailSim1235 > 0 && !amb && !mvPump) { npcFailSim1235--; logf_("MSNNPC sim-fail %s left=%d\n", name, npcFailSim1235); return npcLoadFail(name, fn, amb); }   // №1235 служебное (flag npcfail N): N отказов чтения подряд
      if (!STAGE_PREFETCH_OLD && !mvPump) {   // фоновое чтение: пока файл не прочитан, модели нет (вернём 0, вызывающий повторит)
        int pi = -1; for (int q = 0; q < NPRE; q++) if (pre[q].p && msn_streq(pre[q].name, name)) pi = q;
        if (pi < 0) {   // начать чтение
          if (preStartBlocked()) return 0;
          for (int q = 0; q < NPRE && pi < 0; q++) if (!pre[q].p) pi = q;
          if (pi < 0) return 0;   // очередь чтения занята — позже
          npcRoom(fn, amb);   // №1235: лестница памяти до чтения (не вышло — всё равно пробуем: отказ ловит npcLoadFail)
          bbOp("npc+", name, BB_SYNC_ASYNC_OLD ? 1 : 0);   // фоновое чтение: запись ящика не в кадре (blackbox.h)
          pre[pi].p = skinPreStart(fn); if (!pre[pi].p) return npcLoadFail(name, fn, amb);
          strncpy(pre[pi].name, name, sizeof pre[pi].name - 1); pre[pi].name[sizeof pre[pi].name - 1] = 0; pre[pi].t0 = t0 = sceKernelGetSystemTimeLow();
          if (!skinPreReady(pre[pi].p) || NPCDELAY1344(pi)) { logf_("MSNNPC prefetch %s\n", name); return 0; }
        } else if (!skinPreReady(pre[pi].p) || NPCDELAY1344(pi)) return 0;
        model[i].m = skinPreFinish(pre[pi].p); pre[pi].p = 0; pre[pi].name[0] = 0;
        if (!model[i].m && !SKINPRE_ERR_OLD) { logf_("MSNNPC async-fail %s, sync read\n", name); model[i].m = skinLoad(fn); }   // ревизия 1.2: ошибка фонового чтения — синхронно, как до заглядывания вперёд
        if (!model[i].m) return npcLoadFail(name, fn, amb);
        strncpy(model[i].name, name, sizeof model[i].name - 1); model[i].amb = amb;
        logf_("MSNNPC load %s bytes=%u ms=%.0f async=1\n", name, model[i].m->h->size, (sceKernelGetSystemTimeLow() - t0) / 1000.f);
        return model[i].m;
      }
      if (mvPump) mvPump();   // правка 244: на экране загрузки — кадр газеты до и после чтения модели
      npcRoom(fn, amb);   // №1235: лестница памяти до чтения
      bbOp("npc+", name, 1);   // правка 103: самописец — ящик на карте ДО чтения модели (встанет посреди — видно, кто грузился)
      model[i].m = skinLoad(fn);
      if (mvPump) mvPump();
      if (!model[i].m) return npcLoadFail(name, fn, amb);
      strncpy(model[i].name, name, sizeof model[i].name - 1); model[i].amb = amb;
      logf_("MSNNPC load %s bytes=%u ms=%.0f\n", name, model[i].m->h->size, (sceKernelGetSystemTimeLow() - t0) / 1000.f);
      return model[i].m;
    }
  }
  // №1361 (Роб: загрузка L5 ждала 3 с «кого-то»): AddNPC("npd", …) в L1 m5 и L5 m1 — риг без кожи (art/chars/npd_m.p3d: скелет, мешей нет; npd_a.p3d: клипы), персонаж-пустышка, модели у него
  // нет и быть не может, в пакете персонажей (npcFiles) его нет. Раньше такое имя оставалось «читается» (animFor повторялся каждый кадр до 1800 попыток), экран загрузки ждал потолок NPC_WAIT_MAX_S
  // (3,0 с на каждой загрузке L5, журнал NPCWAIT1076 left=1/.). Теперь имя вне пакета — отказ сразу (preFail), ожидание не начинается. NPD1361_OLD=1 / «flag npd1361old» / файл npd1361old — как было
  if (mvNpd1361 && !inPack1361) { preFail = 1; static char seen1361[6][24]; int k1 = 0; while (k1 < 6 && seen1361[k1][0] && strcmp(seen1361[k1], name)) k1++; if (k1 < 6 && !seen1361[k1][0]) { snprintf(seen1361[k1], 24, "%s", name); logf_("MSNNPC no-model %s (нет в пакете персонажей)\n", name); } }
  return 0;
}
// Заглядывание вперёд (разбор тура 12): модели персонажей СЛЕДУЮЩЕГО этапа читаются в фон, пока идёт текущий (на первом этапе — пока висит карточка
// брифинга, 6–8 с мир стоит); к смене этапа файл уже в памяти, разбор — доли мс. Слот модели не занимается, чужие модели не выбрасываются; читаем
// не больше двух файлов за раз (остальным — свободное место очереди).
#ifndef MSN_PREFETCH_HEAP_OLD
#define MSN_PREFETCH_HEAP_OLD 0   // 1 — читать впрок без проверки запаса кучи (прежнее)
#endif
#define MSN_PREFETCH_BYTES (512u << 10)   // файл модели 0,2-0,4 МБ с запасом
int (*mvHeapRoom)(unsigned);
// №632 (TEST19, выход из камеры будки/лавки: 12 кадров по 55–79 мс, io 23–30 мс — открытие файлов NPC в кадре камеры): пока идёт камера будки/экран лавки, чтение моделей NPC не начинаем
// (персонаж ждёт в очереди, animFor повторяется каждый кадр), а после — не чаще одного старта за 120 мс. PHONE_PREWARM_OLD=1 — прежнее
#ifndef PHONE_PREWARM_OLD
#define PHONE_PREWARM_OLD 0
#endif
int (*mvViewBusy)(void);   // 1 — камера будки / экран лавки / гардероб (ставит main)
static int preStartBlocked(void) {
  if (PHONE_PREWARM_OLD) return 0;
  static unsigned last;
  if (mvViewBusy && mvViewBusy()) return 1;
  unsigned now = sceKernelGetSystemTimeLow(); if (now - last < 120000u) return 1;
  last = now; return 0;
}
int (*mvCarReady)(const char *);   // 109: 1 — модель машины миссии в памяти (или её уже не будет: нет в пакете, битая); 0 — ещё нет, хост пробует читать (ставит main)
void (*mvCarPrefetch)(const char *);   // №629: фоновое чтение блока модели машины миссии (mcrPreStart, main_render.h)
static void modelPrefetch(const char *name) {
  if (STAGE_PREFETCH_OLD || mvPump || !name || !*name) return;
  int freeQ = 0; for (int q = 0; q < NPRE; q++) if (!pre[q].p) freeQ++;
  if (freeQ < 2) return;
  if (!MSN_PREFETCH_HEAP_OLD && mvHeapRoom && !mvHeapRoom(MSN_PREFETCH_BYTES)) return;   // ревизия 1.1: впрок читаем, только если после файла остаётся резерв кучи стриминга
  for (int i = 0; i < NMODEL; i++) if (model[i].m && msn_streq(model[i].name, name)) return;
  if (prePending(name)) return;
  int usedSlots = 0; for (int i = 0; i < NMODEL; i++) if (model[i].m) usedSlots++;
  if (usedSlots >= NMODEL) return;   // нет места под модель — читать впрок незачем
  for (unsigned k = 0; k < sizeof npcFiles / sizeof *npcFiles; k++) {
    const char *b = npcFiles[k] + 6; size_t n = strlen(name);
    if (strncmp(b, name, n) || strcmp(b + n, ".chr")) continue;
    char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir, npcFiles[k]);
    for (int q = 0; q < NPRE; q++) if (!pre[q].p) {
      pre[q].p = skinPreStart(fn); if (!pre[q].p) return;
      strncpy(pre[q].name, name, sizeof pre[q].name - 1); pre[q].name[sizeof pre[q].name - 1] = 0; pre[q].t0 = sceKernelGetSystemTimeLow();
      logf_("MSNNPC lookahead %s\n", name); return;
    }
  }
}
// Правка 180: у персонажа бонус-миссии своя анимация — ключ «имя#b» (Ральф уровня 1: фоновый и гонщик sr3 — двое, модель одна)
static const char *npcKey(const GameNpc *n) {
  static char k[4][28]; static int r;
  if (!n->bonus) return n->name;
  r = (r + 1) & 3; snprintf(k[r], sizeof k[r], "%s#b", n->name); return k[r];
}
static const GameNpc *npcByKey(const char *key) {   // персонаж по ключу анимации
  const char *h = strchr(key, '#'); size_t n = h ? (size_t)(h - key) : strlen(key);
  for (int i = 0; i < M.nnpc; i++) {
    const GameNpc *c = &M.npc[i];
    if ((c->bonus != 0) == (h != 0) && strlen(c->name) == n && !strncmp(c->name, key, n)) return c;
  }
  return 0;
}
#ifndef NPC_ANIMFOR_OLD
#define NPC_ANIMFOR_OLD 0
#endif
#ifndef NPC_VIS_OLD
#define NPC_VIS_OLD 0
#endif
static int (*mvVis)(const float bb[6]);   // отсечение по пирамиде кадра (main.c trVis); NULL — боты и хосты, без отсечения
void mvSetVis(int (*f)(const float bb[6])) { mvVis = f; }
// Канон п.11 (правка 431): персонаж миссии не рисуется, пока клетка города под ним не в памяти (state 2). Комнаты и точки ниже города (y < −10) не проверяются:
// их пол резидентен. NULL (боты, хосты) — без проверки. NPCSCENE431_OLD=1 — прежнее (рисуется всегда)
#ifndef NPCSCENE431_OLD
#define NPCSCENE431_OLD 0
#endif
static int (*mvSceneOk)(float x, float z); static int (*mvSceneOkY)(float x, float y, float z);   // 108 1293: вариант с высотой ног персонажа (NULL — прежний)
void mvSetSceneOk(int (*f)(float x, float z)) { mvSceneOk = f; }
void mvSetSceneOkY(int (*f)(float x, float y, float z)) { mvSceneOkY = f; }
int mvNpcSceneHeld;   // сколько персонажей прямо сейчас скрыто за сцену (смоук читает по строкам NPCSCENE431)
int mvNpcCityPos(int i, float *x, float *z) {   // служебно (смоук dropnpc): городской персонаж миссии под номером i (не комната) — его место; 0 — нет такого
  if (i < 0 || i >= M.nnpc) return 0;
  const GameNpc *n = &M.npc[i]; if (n->pos.y <= -10.f || n->interior[0]) return 0;
  *x = n->pos.x; *z = n->pos.z; return 1;
}
static int (*mvGround)(float x, float z, float yHi, float yLo, float *out);
void mvSetGround(int (*f)(float x, float z, float yHi, float yLo, float *out)) { mvGround = f; }
static NpcAnim *animFor(const char *name, int create, int amb) {   // name — ключ (npcKey); модель — по имени без «#b»
  // Правка Роба 500 (персонажи): имя без «#b» (snprintf + strchr) нужно только при создании / подгрузке модели, а animFor зовут на каждого персонажа каждый кадр
  // (mvDraw3D) — раньше строка копировалась и для тех, кого просто ищут. NPC_ANIMFOR_OLD=1 — прежний порядок
  char base[24];
  if (NPC_ANIMFOR_OLD) { snprintf(base, sizeof base, "%s", name); { char *h = strchr(base, '#'); if (h) *h = 0; } }
  for (int i = 0; i < NANIM; i++) if (anim[i] && msn_streq(anim[i]->name, name)) {
    if (create && !amb) { if (!NPC_ANIMFOR_OLD) { snprintf(base, sizeof base, "%s", name); { char *h = strchr(base, '#'); if (h) *h = 0; } } modelFor(base, 0); }
    return anim[i]; }
  if (!create) return 0;
  if (!NPC_ANIMFOR_OLD) { snprintf(base, sizeof base, "%s", name); { char *h = strchr(base, '#'); if (h) *h = 0; } }
  const SkinModel *m = modelFor(base, amb); if (!m) return 0;
  for (int i = 0; i < NANIM; i++) if (!anim[i]) {
    NpcAnim *a = memalign(16, sizeof *a); if (!a) return 0;
    memset(a, 0, sizeof *a); strncpy(a->name, name, sizeof a->name - 1); a->m = m;
    skinInit(&a->p, m); a->idle = skinFindClip(m, "loco_idle_rest"); a->walk = skinFindClip(m, "loco_walk"); a->talkLine = a->gest = -1;
    // правка 240: клипы сбитого (npd.cho: npd_flail / npd_fall_land / npd_get_up; marge/lisa/apu/bart.cho — свои flail и get_up);
    // имена здесь же вынимает tools/char_export.py --npc (npc_code_clips) — пакеты персонажей несут эти клипы
    a->flail = skinFindClip(m, "flail"); a->land = skinFindClip(m, "fall_land"); a->getup = skinFindClip(m, "get_up");
    if (a->idle >= 0) skinPlay(&a->p, a->idle, 1, 0.f);
    anim[i] = a; return a;
  }
  return 0;
}
static void animPrune(void) {   // персонаж ушёл из миссии — его анимация и (если больше не нужна) модель освобождаются
  for (int i = 0; i < NANIM; i++) if (anim[i] && !npcByKey(anim[i]->name)) { free(anim[i]); anim[i] = 0; }
  for (int k = 0; k < NMODEL; k++) {
    if (!model[k].m) continue;
    int used = 0; for (int i = 0; i < NANIM; i++) if (anim[i] && anim[i]->m == model[k].m) used = 1;
    if (!used) { bbOp("npc-", model[k].name, 0); skinFree(model[k].m); model[k].m = 0; model[k].name[0] = 0; }
  }
}
int mvModels(void) { int n = 0; for (int k = 0; k < NMODEL; k++) n += model[k].m != 0; return n; }   // правка 103: строка ящика

// ------------------------------------------------------------------ HUD миссии: родные элементы Scrooby (ui.shr)
static struct {
  int failed, failSel; char failMsg[200];
  float stageFlash, missionFlash;      // до какого времени (clock_) видна вспышка
  int markerOn; MsnMarker marker;
  char talk[24]; float talkH;          // собеседник цели talkto и высота значка (SetTalkToTarget, 3-й аргумент)
  struct { int type; float x, z; char name[24]; } giver[16]; int ngiver;
  int stages;                          // сколько этапов начато (служебно)
  int briefing, brFirst, brLoaded, brQueued, brPicLost, brWager, brFee; unsigned brT0; char present[32], title[96], info[256];   // брифинг миссии (MissionLoad.scr): картинка SetPresentationBitmap
  int levelDone; float levelDoneAt;    // MSN_EV_LEVEL_COMPLETE: 1 — уровень, 2 — игра пройдена; когда отдать main.c (clock_)
  int levelEnd;                        // открыт экран конца уровня (LevelEnd.scr), ждёт X
  char fmv[32]; int fmvWait;           // цель «fmv» (SetFMVInfo): ролик ждёт main.c (mvFmvPending / mvFmvDone)
  char dynaFiles[160]; unsigned dynaSeq;   // сверка g11b-collect круг 5 (P12-10): последняя строка файлов SetDynaLoadData и сколько раз команда выполнялась (старт, рестарт, этап)
  char dynaRoom[32];                   // последний SetDynaLoadData: интерьер («…@», "SimpsonsHouse"; "" — город), как в оригинале до следующего
} H;
// сверка g07b-msnview (P13-3, P13-6, P13-2, P06-6): попытки и пропуск миссий. Число учтённых «сбросов» миссии растёт при старте основной части и при каждом перезапуске
// (Restart паузы, Yes на экране провала); Skip на экране провала — после 7 учтённых (сбросы / 2), на L7 никогда для 5-й, 6-й и 7-й сюжетной миссии. Пропущенная миссия не
// пройдена, но значок в MISSION SELECT у неё жёлтая черта; крест — только при попытках и без пропуска. Данные живут сессию (запись прогресса — передано main_save.h / savegame.h).
// Было: счётчика и пропуска нет, крестом помечалась текущая сюжетная миссия всегда. SV_SKIP_OLD=1 — прежнее
#ifndef SV_SKIP7_OLD
#define SV_SKIP7_OLD 0   // круг 6 (P13-3): 1 — прежнее: учтённых попыток делилось на 2 ещё раз (Skip только после 14 запусков вместо 7)
#endif
#ifndef SV_SKIP_OLD
#define SV_SKIP_OLD 0
#endif
// 1331 (Роб: «на третьей-четвёртой попытке жму Yes, карточка, потом снова failed, теперь с пунктом Skip»): Skip появлялся на 4-м провале вместо 7-го. Попытки считались дважды: Yes на экране провала и Restart паузы бумили счётчик сами,
// и ещё раз титул карточки (titleCb), которая выходит на каждом повторе, — по два на повтор (1, 2, 4, 6, 7 на 4-м экране провала). У оригинала сброс миссии учитывается один раз за попытку (счёт сбросов / 2, Skip при 7 попытках).
// Теперь Yes и Restart сами не бумят, считает только карточка. SKIPCOUNT1331_OLD=1 / flag skipcount1331old 1 — прежнее
#ifndef SKIPCOUNT1331_OLD
#define SKIPCOUNT1331_OLD 0
#endif
int mvSkipCount1331Old = SKIPCOUNT1331_OLD;
static unsigned char msTries[32]; static unsigned msSkipped;
static void msTryBump(void) { if (!SV_SKIP_OLD && M.rt.cur >= 0 && M.rt.cur < 32 && msTries[M.rt.cur] < 255) msTries[M.rt.cur]++; }
// сверка g07b-msnview (M2:6): «Item Dropped!» мигает вкл/выкл каждые 0,25 с и висит, пока активных предметов больше нуля; снимается на нуле. Было: фиксированные 2 с без мигания.
// Въезд на 21 % высоты за 0,2 с — в ui.c (передано). SV_DROP_OLD=1 — прежнее
#ifndef SV_DROP_OLD
#define SV_DROP_OLD 0
#endif
static int dropOn; static float dropT0;
#ifndef SV_BONUS_REPEAT_OLD
#define SV_BONUS_REPEAT_OLD 0
#endif
static void dropTick(void) {
  if (!dropOn) return;
  // №1229 (Роб, TEST93 L5, Снейк: «ITEM DROPPED остаётся и мигает после подбора»): в оригинале игра шлёт HUD число АКТИВНЫХ предметов (выпавших и ещё не подобранных, не пропавших по сроку) и
  // надпись держится, пока оно больше нуля, на нуле гаснет, новое выпадение включает снова. Порт считал все ещё не собранные предметы миссии, в том числе не выпавшие — надпись висела до конца.
  // Теперь только released && !done (после срока жизни released снова 0). DROP1229_OLD=1 / flag drop1229 0 — как было
  int left = 0; for (int i = 0; i < M.rt.nitems && i < MSN_MAX_ITEMS; i++) if (!M.rt.done[i] && (!uiDrop1229 || M.rt.released[i])) left++;
  if (!left || M.rt.status != MSN_RUNNING) { dropOn = 0; uiForceShow(UI_T_ITEM_DROPPED, 0.f); { int nr_ = 0, nd_ = 0; for (int k_ = 0; k_ < M.rt.nitems && k_ < MSN_MAX_ITEMS; k_++) { nr_ += M.rt.released[k_] != 0; nd_ += M.rt.done[k_] != 0; } logf_("MSN dropmsg off t=%.1f items=%d released=%d done=%d status=%d\n", clock_, M.rt.nitems, nr_, nd_, (int)M.rt.status); } return; }
  uiForceShow(UI_T_ITEM_DROPPED, mvPulseOn(clock_ - dropT0, .25f) ? .2f : 0.f);
}
static int levelEndOld;   // служебное (flag uilevelendold): прежняя вспышка 3 с вместо экрана LevelEnd (отрицательный контроль)
static int iconIndex(const char *icon) {   // значок задания: из msnicons (msnart.shr, может ещё дочитываться) — иначе без значка
  return icon && *icon ? UI_OBJICON_EXTERNAL : -1;
}
// №958б (Роб: значок цели с L1 «зависает» и показывается на других уровнях; в эмуляторе не воспроизведено): состояние значка — в строку чёрного ящика
// каждую секунду (поле ico, viewer/main_frontend.h bbSample) и строкой ICON при каждой смене значка/облачка (кто, какой уровень цели); в игре и в выпуске
#ifndef BBICON958_OLD
#define BBICON958_OLD 0
#endif
// 1324: готов ли мир для старта отсчёта и таймера: клетки города под игроком, под машиной и в 25 м вокруг (sceneReadyHook, как у героя и раскрытия перехода). Клетка, которая не прочиталась (failed), не ждётся
static int sceneBusy1324(void) {
  if (!sceneReadyHook || !M.g || !M.g->pWorld) return 0;
  int foot = gameOnFoot(M.g); float px = foot ? M.g->pHomer->x : M.g->pCar->x, pz = foot ? M.g->pHomer->z : M.g->pCar->z;
  float pt[6][2] = {{px, pz}, {M.g->pCar->x, M.g->pCar->z}, {px + 25.f, pz}, {px - 25.f, pz}, {px, pz + 25.f}, {px, pz - 25.f}};
  for (int i = 0; i < 6; i++) { int failed = 0; if (!sceneReadyHook(M.g->pWorld, pt[i][0], pt[i][1], &failed) && !failed) return 1; }
  return 0;
}
static int iconLv;   // уровень пакета миссии, в котором значок/текст цели поставлены последний раз
static char iconKey[48];
void mvIconState(char *o, unsigned n) {
  int ic, px; const char *t, *l; uiObjDebug(&ic, &px, &t, &l);
  const char *w = msnartWantIcon(); char tt[14]; unsigned k = 0;
  for (const char *a = t && *t ? t : "-"; *a && k < sizeof tt - 1; a++) tt[k++] = *a == ' ' ? '_' : *a;
  tt[k] = 0;
  snprintf(o, n, "%s/%d/%d/L%d/%s", w && *w ? w : "-", ic, px, iconLv, tt);
}
static void iconNote(const char *what) {   // строка ICON в кольцо ящика только при смене (имя значка, текст, уровень)
  if (BBICON958_OLD) return;
  char st[56]; mvIconState(st, sizeof st);
  if (!strcmp(st, iconKey)) return;
  snprintf(iconKey, sizeof iconKey, "%s", st);
  char l[112]; snprintf(l, sizeof l, "ICON %s lvpack %d cur %s", what, mvPackLevel(), st); bbNoteIcon(l);
}
int (*mvCamBusy)(void);
static int holdOn; static char holdText[160], holdIcon[40];   // №1047: цель, ждущая конца наезда и ухода полос
static void objectiveShow(const char *text, const char *icon);
// №1247: пузырь цели ждёт ещё и аватар: пока картинка не дочитана, текст стартует с места пакета и потом прыгает (ui.c objTextStart). Чтение записи идёт с момента задания (msnartSetObjIcon ставит ожидаемый значок),
// пузырь выходит, когда картинка есть, чтение кончилось без неё или прошло BUB1247_AVATAR_CAP_FR (тогда WATCHDOG bubble). BUBBLE1247_OLD=1 — без ожидания аватара
#ifndef BUBBLE1247_OLD
#define BUBBLE1247_OLD 0
#endif
// №1264/1267/1268 (Роб 13.11, видео IMG_8263, Build 106; сверка Fable с оригиналом: значок всплывает 0,5 с, потом текст, новый текст получает полный срок, этап без текста бабл не трогает, ожидания картинки нет):
// три пути потери сообщения этапа. бит 1 (BUBBLE1264A_OLD) — этап без текста и этап-разговор (msn_game.c даёт "") гасили показанный и ждущий бабл; теперь бабл они не трогают;
// бит 2 (BUBBLE1264B_OLD) — ожидание значка было 90 кадров (≈3 с), у оригинала 0,5 с (15 кадров); бит 4 (BUBBLE1264C_OLD) — cleanup не снимал ждущий текст (в новый этап уезжал текст прежней миссии).
// «flag bubble1264old N» (маска 1|2|4) — прежнее поведение выбранных путей; 7 — всё как в Build 106. Журнал: BUBBLE1264 keep / drop
#ifndef BUBBLE1264A_OLD
#define BUBBLE1264A_OLD 0
#endif
#ifndef BUBBLE1264B_OLD
#define BUBBLE1264B_OLD 0
#endif
#ifndef BUBBLE1264C_OLD
#define BUBBLE1264C_OLD 0
#endif
int mvBubble1264Old = (BUBBLE1264A_OLD ? 1 : 0) | (BUBBLE1264B_OLD ? 2 : 0) | (BUBBLE1264C_OLD ? 4 : 0);
#define BUB1247_AVATAR_CAP_FR 90   // кадров игры (≈3 с при 30 к/с): срок идёт только пока рисуются кадры мира; на экране загрузки и под ней не тикает (журнал L5 Будки: срок истёк в загрузке, иконка пришла через 0,3 с после первого кадра)
int mvBubble1247Old = BUBBLE1247_OLD; static int holdFr;
static int avatarWait1247(const char *icon) {
  if (mvBubble1247Old || !icon || !*icon || msnartImage(icon) || !msnartReading()) return 0;
  int cap_ = mvBubble1264Old & 2 ? BUB1247_AVATAR_CAP_FR : 15;   // №1264б: 0,5 с оригинала (15 кадров при 30 к/с)
  if (holdFr >= cap_) { static int lg_ = -1; if (holdFr != lg_) { lg_ = holdFr; logf_("WATCHDOG bubble avatar-late icon=%s cap=%d кадров t=%.1f\n", icon, cap_, clock_); } return 0; }
  return 1;
}
// 1310/1311 (Роб: «непонятно, что делать»; Fable по записи оригинала: значок 0,5 с, затем текст 3,0 с, срок считает экран игры): у нас текст этапа 0 показывался под карточкой, уезжающей до 1,6 с, и под закрытым ирисом
// (экран игры тогда рисуется и срок 3,5 с идёт), игрок видел его укороченным или совсем не видел. Теперь пузырь ждёт ещё и «мир виден»: карточка не стоит и не уезжает, ирис открыт, перехода нет, нет паузы
// (крючок main — mvWorldHidden1310); срок начинается с первого кадра, где игрок видит мир. BUBVIS1310_OLD=1 / flag bubvis1310old 1 — прежнее. Журнал: BUBBLE shown=<с видно> why=expire|replaced|cleanup, сторож WATCHDOG bubble short (видно < 3,0 с)
// 1315 К5 (Роб: «сядь в машину» появилось, когда он уже в машине): «камера занята» держала пузырь БЕЗ предела. Теперь предел 2 с для полос разговора и дуги камеры, 8 с для самого наезда, потом показ и WATCHDOG bubble cam-late why=<…>. BUBCAM1315_OLD=1 / flag bubcam1315old 1 — прежнее
#ifndef BUBVIS1310_OLD
#define BUBVIS1310_OLD 0
#endif
#ifndef BUBCAM1315_OLD
#define BUBCAM1315_OLD 0
#endif
int mvCamFree1310, mvBubVis1310Old = BUBVIS1310_OLD, mvBubCam1315Old = BUBCAM1315_OLD; int (*mvWorldHidden1310)(void); const char *mvCamWhy1315;
static unsigned camSince1315, camLog1315;
static int camStuck1315(void) { mvCamWhy1315 = "duga"; return 1; }
void mvCamStuck1315(int on) { mvCamBusy = on ? camStuck1315 : 0; }   // служебно (flag camstuck1315 1): «камера занята» застряла навсегда (стенд К5)
static int worldHidden1310(void) { return H.briefing || (mvWorldHidden1310 && mvWorldHidden1310()); }
static int bubbleBusy1247(const char *icon) {
  int cam_ = !mvCamFree1310 && mvCamBusy && mvCamBusy();   // mvCamFree1310 — служебно (flag camfree1310 1): миссия без камеры старта, как у части миссий на железе (стенд К1)
  if (!mvBubCam1315Old) {
    if (cam_) { unsigned now_ = sceKernelGetSystemTimeLow(); if (!camSince1315) { camSince1315 = now_ ? now_ : 1; }
      unsigned lim_ = mvCamWhy1315 && mvCamWhy1315[0] == 'n' ? 8000000u : 2000000u;   // n(аезд) — 8 с, полосы и дуга — 2 с
      if ((unsigned)(now_ - camSince1315) >= lim_) { if (camLog1315 != camSince1315) { camLog1315 = camSince1315; logf_("WATCHDOG bubble cam-late why=%s cap=%u мс t=%.1f\n", mvCamWhy1315 ? mvCamWhy1315 : "?", lim_ / 1000u, clock_); } cam_ = 0; } }
    else camSince1315 = 0;
  }
  int hid_ = !mvBubVis1310Old && worldHidden1310();
  if ((cam_ || hid_) && !mvBubble1247Old) holdFr = 0;
  return cam_ || hid_ || avatarWait1247(icon); }   // срок ожидания аватара идёт с конца наезда, а не с прихода текста (наезд длиннее срока)
// 1310 К2: сколько секунд текст этапа был виден (мир виден: ирис открыт, карточки нет, не пауза) за 3,5 с срока экрана игры. Срок экрана идёт, пока нет карточки и паузы
static struct { int on; float el, shown; char tx[28]; } bubT;
static void bubEnd1310(const char *why) {
  if (!bubT.on) return; bubT.on = 0;
  logf_("BUBBLE shown=%.1f why=%s text=\"%s\" t=%.1f\n", bubT.shown, why, bubT.tx, clock_);
  if (!strcmp(why, "expire") && bubT.shown < 3.0f) { char i_[96]; snprintf(i_, sizeof i_, "shown=%.1f text=%s", bubT.shown, bubT.tx); wdFire("bubble", "short", i_); }
}
static void bubTick1310(float dt) {
  if (!bubT.on || uiPaused() || H.briefing) return;
  bubT.el += dt; if (!(mvWorldHidden1310 && mvWorldHidden1310())) bubT.shown += dt;
  if (bubT.el >= 3.5f) bubEnd1310("expire");
}
void mvBubblePump(void) {
  if (!holdOn) return;
  holdFr++;
  if (!mvBubble1247Old) msnartSetObjIcon(holdIcon[0] ? holdIcon : 0);   // чтение значка идёт, пока ждём; готов — встаёт в угол и в рамку, не в момент показа
  if (!bubbleBusy1247(holdIcon)) { holdOn = 0; objectiveShow(holdText, holdIcon); } }
static void uiObjectiveCb(void *u, const char *text, int msg, const char *icon) {
  (void)u; (void)msg;
  if (!(text && *text) && icon && *icon && !msnLockIcon1321Old) {   // 1321: этап без текста, но со значком (после запертого этапа): только значок в углу, бабл не трогаем (у оригинала «показать цель −1» = обновить значок)
    holdOn = 0; msnartSetObjIcon(icon); uiHudSet(UI_F_OBJICON, iconIndex(icon)); iconLv = mvPackLevel(); iconNote("set-icon-only"); logf_("LOCKICON1321 icon=%s t=%.1f\n", icon, clock_); return; }
  if (!(text && *text) && !(mvBubble1264Old & 1)) { logf_("BUBBLE1264 keep (этап без текста бабл не трогает) hold=%d t=%.1f\n", holdOn, clock_); return; }   // №1264а
  if (text && *text && !holdOn) holdFr = 0;
  if (text && *text && bubbleBusy1247(icon)) { snprintf(holdText, sizeof holdText, "%s", text); snprintf(holdIcon, sizeof holdIcon, "%s", icon ? icon : ""); holdOn = 1; return; }   // №1047: сначала наезд, потом пузырь; №1247: и аватар
  holdOn = 0; objectiveShow(text, icon);
}
static void objectiveShow(const char *text, const char *icon) {
  if (text && *text) { bubEnd1310("replaced"); bubT.on = 1; bubT.el = bubT.shown = 0.f; logf_("BUBBLE start briefing=%d hidden=%d t=%.1f\n", H.briefing, mvWorldHidden1310 ? mvWorldHidden1310() : -1, clock_); snprintf(bubT.tx, sizeof bubT.tx, "%.27s", text); msnartSetObjIcon(icon); uiMessageStage(text, iconIndex(icon), 0.f); iconLv = mvPackLevel(); iconNote("set"); }
  else { uiMessage(NULL, -1, 0.f); iconNote("off"); }
}
// сверка g09a-hud круг 3 (С16, M2:15): мигание таймера (белый–красный, период 0,5 с, ×0,67 после половины окна, ×0,5 после 0,8). Границы окна мигания в скриптах миссий не найдены
// (tools/missions не несёт такой команды), поэтому окно — правило порта: последние SV_TIMERBLINK_S секунд обратного отсчёта; таймер «вверх» не мигает и не прячется.
// ВОПРОС Робу/g07: если окно задаётся данными, подставить их. SV_TIMERBLINK_OLD=1 — без мигания
#ifndef SV_TIMERBLINK_OLD
#define SV_TIMERBLINK_OLD 0
#endif
#define SV_TIMERBLINK_S 10
extern void uiTimerBlink(int on, float frac) __attribute__((weak));
extern void uiTimerUp(int up) __attribute__((weak));
static int tmrLastSec1324 = -1;   // 1324: последнее число таймера обратного отсчёта, которое игрок видел (с), для строки провала
static void uiTimerCb(void *u, int ms, int up) {
  (void)u; if (!up && ms > (msnTimerFloor1324Old ? 1000 : 0)) tmrLastSec1324 = ms / 1000;   // как в ui.c: при остатке ≤ порога оверлей прячется — такое число игрок не видит
  uiHudSet(UI_F_TIMER, ms < 0 ? 0 : ms); uiHudShow(UI_T_TIMER, ms >= 0);
  if (uiTimerUp) uiTimerUp(ms >= 0 && up);
  if (uiTimerBlink) { int in = !SV_TIMERBLINK_OLD && ms > 1000 && !up && ms <= SV_TIMERBLINK_S * 1000; uiTimerBlink(in, in ? 1.f - (float)ms / (SV_TIMERBLINK_S * 1000.f) : 0.f); }
}
static void uiParCb(void *u, int ms) { (void)u; uiHudSet(UI_F_PARTIME, ms < 0 ? 0 : ms); uiHudShow(UI_T_PARTIME, ms >= 0); }
static void uiCounterCb(void *u, int done, int total) {   // гонка по кругам — прогресс показывают круги, не точки
  (void)u; int race = M.rt.obj == MSN_OBJ_RACE && M.rt.laps > 1;
  uiHudSet(UI_F_COLLECTED, done); uiHudSet(UI_F_COLLECT_TOTAL, total); uiHudShow(UI_T_COLLECTIBLES, total > 0 && !race);
}
// сверка g07b-msnview (M2:16): при круге больше первого группа кругов переключает видимость каждые 0,25 с, 8 переключений (4 мигания) и играет событие обновления круга.
// Звук обновления круга — передано (имя события в таблицах звука нужно владельцу ui_sfx.c). Было: просто показ. SV_LAPBLINK_OLD=1 — прежнее
#ifndef SV_LAPBLINK_OLD
#define SV_LAPBLINK_OLD 0
#endif
static float lapT0 = -1.f; static int lapShow, lapLast;
static void uiLapCb(void *u, int lap, int laps) {
  (void)u; uiHudSet(UI_F_LAP, lap); uiHudSet(UI_F_LAPS, laps); lapShow = laps > 1; uiHudShow(UI_T_LAP, laps > 1);
  if (!SV_LAPBLINK_OLD && lap > 1 && lap != lapLast && laps > 1) lapT0 = clock_;
  lapLast = lap;
}
static void lapTick(void) {
  if (lapT0 < 0.f) return;
  int n = mvLapStep(clock_ - lapT0);
  if (n < 0) { lapT0 = -1.f; uiHudShow(UI_T_LAP, lapShow); return; }
  uiHudShow(UI_T_LAP, lapShow && n % 2 == 0);
}
// сверка g07b-msnview круг 2 (g09a-hud, M2:3): длительность позиции отсчёта из сценария передаётся в ui.c через uiCountdownDur (слабая ссылка: переменная появится вместе с CDOWN_POP_OLD g09a;
// пока её нет — ничего не делается). Пустой кадр между позициями — ветка !text ниже. SV_CDOWN_DUR_OLD=1 — не передавать
#ifndef SV_CDOWN_DUR_OLD
#define SV_CDOWN_DUR_OLD 0
#endif
extern float uiCountdownDur __attribute__((weak));
static void uiCountdownCb(void *u, const char *text, float s) {   // "3" "2" "1" — число HUD; "GO!" и прочее — строка задания
  (void)u; if (!SV_CDOWN_DUR_OLD && &uiCountdownDur && text && s > 0.f) uiCountdownDur = s;
  if (!text) { uiHudShow(UI_T_COUNTDOWN, 0); return; }   // слово остаётся для угасания; следующая цифра его сбросит (uiCountdownText(0))
  if (text[0] >= '0' && text[0] <= '9') { uiCountdownText(0); uiHudSet(UI_F_COUNTDOWN, atoi(text)); uiHudShow(UI_T_COUNTDOWN, 1); }
  else if (!text[0]) uiHudShow(UI_T_COUNTDOWN, 0);   // пустая строка — не слово: без устаревшего числа на экране
#if !defined(COUNTDOWN_LINE_OLD) || !COUNTDOWN_LINE_OLD
  else { uiCountdownText(text); uiHudShow(UI_T_COUNTDOWN, 1); }   // №630: слова отсчёта («PSYCHE», «GO!») в центре тем же зелёным шрифтом, длительность записи сценария
#else
  else { uiHudShow(UI_T_COUNTDOWN, 0); uiMessage(text, -1, s > 0.f ? s : 1.f); }
#endif
}
#ifndef SV_MCOMPLETE_OLD
#define SV_MCOMPLETE_OLD 0
#endif
#ifndef SV_STAGEFLASH_OLD
#define SV_STAGEFLASH_OLD 0
#endif
static void uiFlashCb(void *u, int what, const char *text) {
  (void)u;
  if (what == MSN_FLASH_STAGE_COMPLETE) H.stageFlash = clock_ + (SV_STAGEFLASH_OLD ? 2.f : 1.f);   // сверка g07b СВЕРКА_С_ИСХОДНИКОМ 7.1: «этап пройден» 1 с (было 2 с; SV_STAGEFLASH_OLD=1)          // MissionProgress ("этап пройден")
  else if (what == MSN_FLASH_MISSION_COMPLETE || what == MSN_FLASH_BONUS_COMPLETE) {   // правка 122: текст вспышки — из пакета миссий
    H.missionFlash = clock_ + (SV_MCOMPLETE_OLD ? 3.f : 2.5f); uiMessage(NULL, -1, 0.f);   /* сверка g07b круг 2 (С_ИСХ 7.2): MISSION COMPLETE 2,5 с (было 3 с; SV_MCOMPLETE_OLD=1) */                             // (MISSION_COMPLETE / BONUS_MISSION_COMPLETE)
    uiMissionDoneText(text && *text ? text : (what == MSN_FLASH_BONUS_COMPLETE ? "BONUS MISSION\nCOMPLETE!" : "MISSION COMPLETE!")); }
  else uiMessage(text, -1, 3.f);                                              // уровень / игра пройдены
  logf_("MSN flash %d %s t=%.1f\n", what, text ? text : "", clock_);
}
// сверка g07b-msnview круг 2 (g08-sound, M2:Д1): экран провала миссии — режим звука «Store» (sndStore(2)), на выходе из экрана снимается. SV_STORE_OLD=1 — прежнее
#ifndef SV_STORE_OLD
#define SV_STORE_OLD 0
#endif
static int failStore;
// сверка g07b-msnview круг 2 (g07-missions, P06 Д3 / P07-12 «не выходи из машины»; P06-4 / P07-4 пар-время ставки; P06-10 SetIrisWipe): показ событий рантайма.
// OUTOFVEHICLE: игрок вышел — таймер окна «сядь в машину» (HUD-таймер со своими секундами, value = окно) и строка; вернулся/сброс — таймер прячется.
// PAR_LATE: ставка дольше пар-времени — раз в секунду текст «ставка проигрывается» из банка текстов (ключ ищется по хешу 82030faf, без имени в коде) и секунды до провала. IRIS: закрыть на конце этапа (1,0 с) и открыть на старте следующего — слоем затемнения msn_view.
// Было: событий никто не показывал. SV_EVENTS2_OLD=1 — прежнее
#ifndef SV_EVENTS2_OLD
#define SV_EVENTS2_OLD 0
#endif
static int oovOn; static float oovWin, oovT0;
static void failStoreOff(void) { if (failStore) { failStore = 0; sndStore(0); } }
static void uiFailedCb(void *u, int reason, const char *text, const char *hint) {   // экран MissionOver: причина, подсказка
  (void)u; H.failed = 1; H.failSel = 0;
  if (!SV_STORE_OLD && !failStore) { failStore = 1; sndStore(2); }
  { static const char *const why[] = {"none", "timeout", "outofvehicle", "damage", "position", "race", "followdistance", "keepbarrel",
                                      "partime", "abort", "hitnrun"};   // правка 281: причина провала — в чёрный ящик (на PSP видно, отчего fail)
    bbOp("fail", reason >= 0 && reason < (int)(sizeof why / sizeof *why) ? why[reason] : "?", 1); }
  snprintf(H.failMsg, sizeof H.failMsg, "%s", text ? text : "");
  uiSetMissionOver(text, hint); uiMessage(NULL, -1, 0.f);
  logf_("MSN failed %d %s t=%.1f lastshown=%d tries=%d skip=%d\n", reason, text ? text : "", clock_, tmrLastSec1324, M.rt.cur >= 0 && M.rt.cur < 32 ? msTries[M.rt.cur] : -1, mvFailSkipOk());
  { char b_[120]; snprintf(b_, sizeof b_, "reason=%d stage=%d.%d tries=%d skip=%d rem=%.1f", reason, M.rt.phase, M.rt.stage, M.rt.cur >= 0 && M.rt.cur < 32 ? msTries[M.rt.cur] : -1, mvFailSkipOk(), M.rt.remaining); bbNoteDiag("FAILINFO", b_); }   // 1331: в ящик выпуска (смоука у Роба нет)
}
static int mkShadowOld;   // 04.10: 1 — маркер цели без пятна <рисунок>_shadow (как до правки; флаг сценария mkshadowold)
void mvMarkerShadowOld(int on) { mkShadowOld = on; }
// сверка g07b-msnview круг 2 (g02-traffic, P16-06): путь навигации целиком строится ОДИН раз при старте цели (gameMsnNavBuild: дороги трафика от машины игрока до цели), потом каждый кадр
// рисуются зелёные стрелки на перекрёстках впереди, красные на пройденных (набор darrow / warrow на каждую), на длинных дорогах по формуле g02. Пешком путь не строится — повтор раз в секунду.
// Нет пути — прежняя одна стрелка у ближайшего перекрёстка (trafficRouteArrow). Высоты стрелок — пробы земли раз в 60 кадров. SV_NAVPATH_OLD=1 — только прежняя стрелка
#ifndef SV_NAVPATH_OLD
#define SV_NAVPATH_OLD 0
#endif
static int navBuilt, navTry; static float navRetry;
static void navMarkerCb(void *u, const MsnMarker *m) { (void)u; H.markerOn = m != 0; if (m) H.marker = *m;
  navBuilt = 0; navTry = m != 0; navRetry = 0.f;
  if (m && m->drawable && *m->drawable) {   // 04.10: служебная строка — маркер цели и его пятно на земле (<рисунок>_shadow)
    char sh[48]; snprintf(sh, sizeof sh, "%s_shadow", m->drawable);
    logf_("MSN marker %s shadow=%s at=%.1f,%.1f,%.1f t=%.1f\n", m->drawable, !mkShadowOld && msnartHas(sh) ? sh : "-", m->pos.x, m->pos.y, m->pos.z, clock_); }
}
// 04.10 (SetCollectibleEffect, запись PS2 10:34.7): эффект этапа — в точке собранного предмета (точки гонок, ящики доставки и
// тарана) и в точке цели goto (carsphere / triggersphere у дома Фландерса m2 и ещё 112 целей всех уровней). Правило — по данным
// этапа (msn_game.c stage_effect: первая команда SetCollectibleEffect этапа); "none" — ничего
static const char *stageEffect(void) {
  const MsnStage *st = M.rt.st; if (!st) return 0;
  for (uint32_t i = 0; i < st->cmd_count; i++)
    if (M.f.cmds[st->cmd_first + i].op == MSN_OP_SETCOLLECTIBLEEFFECT) return msn_arg_s(&M.f, &M.f.cmds[st->cmd_first + i], 0);
  return 0;
}
static int cfxWas;
static void cfxAt(const char *eff, const MsnVec3 *p, const char *why, int idx) {
  if (!eff || !*eff) return;
  float q[3] = {p->x, p->y, p->z}; int n = cfxPlay(eff, q);
  // №463 (Роб: въезд в точку миссии / чекпойнт гонки — столб света и тот же «чпонг», что при подборе предметов): звук подбора на каждый
  // эффект цели и собранной точки. MSN_GOTO_PING_OLD=1 — без звука, как было
#ifndef MSN_GOTO_PING_OLD
#define MSN_GOTO_PING_OLD 0
#endif
  if (!MSN_GOTO_PING_OLD && strcmp(eff, "none")) sndCoin();
  logf_("MSN cfx %s %s=%d at=%.1f,%.1f,%.1f parts=%d t=%.1f\n", eff, why, idx, q[0], q[1], q[2], n, clock_);
}
// №314 (перепись 16.10): AddCollectibleStateProp(имя, локатор, n) — бочка bombbarrel L7 (m3/m5/m6/m7). Раньше props.state_prop не был подключён и предмет не рисовался
// (цель «подобрать» шла по локатору невидимо). Теперь модель — экземпляр SPR1 (tools/psp_export.py mission_stateprops, тип «подбор» cls 2 со скрытым
// состоянием 0) на локаторе миссии; пока цель миссии — «подобрать предмет» (SetPickupTarget) с этим локатором, он включён (состояние 1), иначе скрыт
// (подобран, этап прошёл). -DMV_STATEPROP_OLD=1 — как было (невидимо)
#ifndef MV_STATEPROP_OLD
#define MV_STATEPROP_OLD 0
#endif
static void statePropCb(void *u, const char *name, const MsnPlace *at) {
  (void)u; if (!name || !at) return;
  logf_("MSN stateprop %s at=%.1f,%.1f,%.1f t=%.1f\n", name, at->pos.x, at->pos.y, at->pos.z, clock_);
}
// 1 — на точке p сейчас цель миссии «подобрать предмет» (её локатор совпал с точкой экземпляра)
int mvStatePropWanted(const float p[3]) {
  if (MV_STATEPROP_OLD || !active || M.rt.status != MSN_RUNNING || M.rt.obj != MSN_OBJ_PICKUPITEM || !M.rt.c_pickup_prop) return 0;
  const MsnLoc *l = msn_arg_loc(&M.f, M.rt.c_pickup_prop, 1); if (!l) return 0;
  float dx = l->pos[0] - p[0], dy = l->pos[1] - p[1], dz = l->pos[2] - p[2];
  return dx * dx + dy * dy + dz * dz < .0225f;
}
// экземпляры SPR1 «скрытых подборов» (два состояния, нулевое пустое) включаются/гасятся по цели миссии; spEnter только при смене
// №852 (тур TEST41 на PSP, L3: sync 0,1 мс кадра): подходят единицы экземпляров (скрытый подбор: класс 2, два состояния, состояние 0 без рисунка),
// а обходились все стейтпропы уровня каждый кадр — список подходящих один раз на набор (смена адреса/числа экземпляров). MV_SYNC852_OLD=1 — обход всех
#ifndef MV_SYNC852_OLD
#define MV_SYNC852_OLD 0
#endif
void mvStatePropSync(StateProps *sp) {
  if (!sp || MV_STATEPROP_OLD) return;
  static const void *key_; static unsigned keyN_, *cand_, ncand_, capCand_;
  if (!MV_SYNC852_OLD && (key_ != (const void *)sp->inst || keyN_ != sp->ninst)) {
    key_ = sp->inst; keyN_ = sp->ninst; ncand_ = 0;
    if (capCand_ < sp->ninst) { free(cand_); cand_ = (unsigned *)malloc((sp->ninst ? sp->ninst : 1) * sizeof(unsigned)); capCand_ = cand_ ? sp->ninst : 0; }
    for (unsigned i = 0; cand_ && i < sp->ninst; i++) { const SpType *T = &sp->type[sp->inst[i].type];
      if (T->cls == 2 && T->nState == 2 && T->kind == 0 && !sp->state[T->firstState].vis) cand_[ncand_++] = i; } }
  unsigned nIt_ = (!MV_SYNC852_OLD && cand_) ? ncand_ : sp->ninst;
  for (unsigned j_ = 0; j_ < nIt_; j_++) { unsigned i = (!MV_SYNC852_OLD && cand_) ? cand_[j_] : j_; if (i >= sp->ninst) continue;
    const SpInst *I = &sp->inst[i]; const SpType *T = &sp->type[I->type];
    if (T->cls != 2 || T->nState != 2 || T->kind != 0 || sp->state[T->firstState].vis) continue;   // скрытый подбор: состояние 0 без рисунка
    float p[3] = {I->x, I->y, I->z}; int want = mvStatePropWanted(p);
    static int seen_[16]; if (i < 16 && !seen_[i]) { seen_[i] = 1; logf_("MSN stateprop-inst %u type=%u at=%.1f,%.1f,%.1f t=%.1f\n", i, I->type, p[0], p[1], p[2], clock_); }
    if ((sp->run[i].state != 0) != want) { spEnter(sp, i, want ? 1 : 0); logf_("MSN stateprop-sync inst=%u show=%d t=%.1f\n", i, want, clock_); }
  }
}
extern int cfxDefault1340Old;
static void itemCollectedCb(void *u, int index, const MsnItem *it) { (void)u;
  if (!it) return;
  const char *eff = it->effect;
  if (!cfxDefault1340Old && (!eff || !*eff) && it->drawable && *it->drawable) eff = "mission_col";   // 1340: без SetCollectibleEffect и с рисунком — эффект по умолчанию, как у оригинала
  cfxAt(eff, &it->at.pos, "item", index); }
static void giverCb(void *u, const MsnGiver *g, int on) {   // значки бонус-миссий и продавцов на радаре (Scrooby HudMap)
  (void)u; int i;
  for (i = 0; i < H.ngiver; i++) if (msn_streq(H.giver[i].name, g->name)) break;
  if (!on) { if (i < H.ngiver) H.giver[i] = H.giver[--H.ngiver]; return; }
  if (i == H.ngiver) { if (H.ngiver >= 16) return; H.ngiver++; }
  H.giver[i].type = g->kind == MSN_GIVER_STREETRACE ? UI_R_STREETRACE : g->kind == MSN_GIVER_WAGER ? UI_R_WAGER :
                    g->kind == MSN_GIVER_SELLER ? UI_R_PURCHASE : UI_R_BONUS;
  H.giver[i].x = g->at.pos.x; H.giver[i].z = g->at.pos.z; strncpy(H.giver[i].name, g->name, sizeof H.giver[i].name - 1);
}
static void npcSpawnCb(void *u, const char *name, const MsnPlace *at, int kind, float r, const char *interior) {
  (void)u; (void)at; (void)r; (void)interior;
  NpcAnim *a = kind == MSN_NPC_MISSION ? animFor(name, 1, 0) : 0;   // фоновые и продавцы — моделью по близости (ambSlots)
  if (!a && kind == MSN_NPC_MISSION && !STAGE_PREFETCH_OLD && nNpcPend < 8) { snprintf(npcPend[nNpcPend++], 24, "%s", name); }   // модель читается в фоне — повторим в mvPrefetchPoll
  logf_("MSN npc %s kind=%d model=%s\n", name, kind, a ? "yes" : kind == MSN_NPC_MISSION ? (nNpcPend ? "pending" : "no") : "near");
}
// №444: draw distance is shared by models and markers. Keep the same five ambient
// model slots and one load per 0.25 game seconds; lead covers the entire queue
// plus 100 ms card-I/O reserve. Camera-to-player distance covers the chase eye.
#define NPC_DRAW_DISTANCE 80.f
// L3 причал (железо TEST17: FPS 18–21, x_npc 3,6 мс при толпе, кадр 42 мс процессора, дальность города уже на полу 150 м): в перегрузке (B7 frameGov, CPU > 30 мс два кадра подряд)
// персонажи миссии дальше NPC_GOV_DIST (50 м; на экране до 8 px высотой) только считают время клипа — без позы, палитры и рисования; собеседник и сбитые рисуются как обычно.
// NPC_GOV_OLD=1 — прежнее (80 м всегда)
#ifndef NPC_GOV_OLD
#define NPC_GOV_OLD 0
#endif
#ifndef NPC_GOV_DIST
#define NPC_GOV_DIST 50.f
#endif
static int mvGov;
void mvSetGov(int on) { mvGov = on; }
#define NPC_PREFETCH_PERIOD .25f
#define NPC_PREFETCH_IO_RESERVE .10f
#ifndef NPC_PREFETCH_OLD
#define NPC_PREFETCH_OLD 0
#endif
static float ambElapsed;
static void ambSlots(float x, float y, float z, float cameraOffset, float speed, float dt) {
  float range = GAME_AMB_NEAR, hold = GAME_AMB_FAR;
  if (NPC_PREFETCH_OLD) {
    static int tick; if (++tick < 15) return; tick = 0;
  } else {
    ambElapsed += dt; if (ambElapsed < NPC_PREFETCH_PERIOD) return;
    ambElapsed = fmodf(ambElapsed, NPC_PREFETCH_PERIOD);
    range = NPC_DRAW_DISTANCE + cameraOffset + fabsf(speed) * (NMODEL_AMB * NPC_PREFETCH_PERIOD + NPC_PREFETCH_IO_RESERVE);
    hold = range + (GAME_AMB_FAR - GAME_AMB_NEAR);
  }
  int want[NMODEL_AMB], nw = gameMsnAmbWantRange(&M, x, y, z, range, NMODEL_AMB, want), miss = -1;
  for (int j = 0; j < nw && miss < 0; j++) if (!animFor(npcKey(&M.npc[want[j]]), 0, 0)) miss = want[j];
  int used = 0, ambUsed = 0; for (int k = 0; k < NMODEL; k++) if (model[k].m) { used++; ambUsed += model[k].amb; }
  int full = miss >= 0 && (ambUsed >= NMODEL_AMB || used >= NMODEL);   // ближнему нужен слот, свободного нет
  if (npcRetry96 && miss >= 0 && !full && !prePending(M.npc[miss].name)) {   // отказ по памяти уже был: освободить кусок, выгрузив лишнего фонового (раз на отказ)
    unsigned dn = 0; int nf = npcFailN(M.npc[miss].name, &dn);
    if (nf > 0 && dn < (unsigned)nf) { full = 1; npcFailMarkDrop(M.npc[miss].name); } }
  if (!STAGE_PREFETCH_OLD && full && miss >= 0 && prePending(M.npc[miss].name)) full = 0;   // уже читается в фоне — слот не освобождаем второй раз
  for (int k = 0; k < NMODEL; k++) {
    if (!model[k].m || !model[k].amb) continue;
    int keep = 0, nearAny = 0;   // правка 180: модель общая у всех персонажей с этим именем (фоновый и бонусный Ральф)
    for (int i = 0; i < M.nnpc; i++) {
      const GameNpc *n = &M.npc[i]; if (!msn_streq(n->name, model[k].name)) continue;
      for (int j = 0; j < nw; j++) if (want[j] == i) keep = 1;
      if (hypotf(n->pos.x - x, n->pos.z - z) <= hold) nearAny = 1;
    }
    if (keep || (nearAny && !full)) continue;
    logf_("MSNNPC drop %s\n", model[k].name); dropModel(k);
    if (full) break;   // одна замена за раз
  }
  if (miss >= 0) { NpcAnim *a = animFor(npcKey(&M.npc[miss]), 1, 1); logf_("MSN npc %s kind=%d model=%s\n", M.npc[miss].name, M.npc[miss].kind, a ? "yes" : "no"); }
}
char mvAmbMissName1361[24];   // №1361 служебно: имя последнего фонового персонажа без модели (строка NPCWAIT1076)
int mvAmbMissingAt(float x, float y, float z) {   // №1076: бонусные и фоновые персонажи в пределах видимости точки без модели (Дедушка в доме L1); есть — двигает их чтение
  if (!active) return 0;
  int want[NMODEL_AMB], nw = gameMsnAmbWantRange(&M, x, y, z, NPC_DRAW_DISTANCE, NMODEL_AMB, want), miss = 0;
  for (int j = 0; j < nw; j++) if (!animFor(npcKey(&M.npc[want[j]]), 0, 0)) { miss++; snprintf(mvAmbMissName1361, sizeof mvAmbMissName1361, "%s", M.npc[want[j]].name); }
  if (miss) { ambElapsed = NPC_PREFETCH_PERIOD; ambSlots(x, y, z, 0.f, 0.f, 0.f); }
  return miss;
}
int mvAmbMissing(void) { if (!active) return 0; MsnWorld w; gameMsnWorld(&M, &w); return mvAmbMissingAt(w.player.x, w.player.y, w.player.z); }
static void presentationCb(void *u, const char *bitmap) {   // SetPresentationBitmap: картинка брифинга, грузится сейчас (фоном)
  (void)u; const char *b = strrchr(bitmap, '/'), *b2 = strrchr(bitmap, '\\'); if (b2 > b) b = b2; b = b ? b + 1 : bitmap;
  int n = 0; while (b[n] && b[n] != '.' && n < (int)sizeof H.present - 1) { H.present[n] = b[n]; n++; } H.present[n] = 0;
  msnartLoad(bitmap, 0);
}
// Правка Роба 52 (эталон PS2 1:39.75–1:41.0, кадры 4/с): карточка миссии сначала показывает мигающее «LOADING» (0.25 с горит,
// 0.25 с нет), пока миссия грузится, затем «✕ Continue» (группа LoadCompleted); «Cancel Mission» на карточке нет. Загрузка в
// порте — картинка задания (msnart читает её асинхронно); у первой миссии уровня оригинал грузит ~1.25 с — правило порта: не
// короче BRIEF_FIRST_LOAD (там же дочитывается всё, что миссии нужно), у следующих — сколько грузится картинка
#define BRIEF_FIRST_LOAD 1.25f
#define BRIEF_BLINK 0.25f
#ifndef SV_LOADBLINK_OLD
#define SV_LOADBLINK_OLD 0
#endif
#ifndef BRIEF_PIC_OLD
#define BRIEF_PIC_OLD 0   // №512: =1 — ждать картинку брифинга без предела, как раньше
#endif
#define BRIEF_PIC_MAX (BRIEFPIC1269_OLD ? 8.0f : 70.f)   // №1269: предел ждущей картинки длиннее всех повторов msnart (60 с); окончательный отказ чтения (picGaveUp) выводит карточку сразу
static int brBriefed;          // брифинг уже был на этом уровне (mvInit сбрасывает)
static int brOld;              // служебное (mvBriefOld): отрицательный контроль гейта — прежняя карточка
void mvBriefOld(int on) { brOld = on; }
// сверка g07b-msnview (P06-3, P07-3, P01-20): выплата ставки = вход + целая часть (SetGamblingOdds машины игрока × вход). Коэффициент даёт main/car через mvCarOdds;
// пока машинных данных нет (CAR1 их не несёт, передано g01/g12) — стартовая машина уровня по диску: L1–L5 2,0 (famil_v honor_v lisa_v marge_v apu_v), L6–L7 1,0 (bart_v homer_v).
// Было: всегда 2 × вход. SV_WAGER_ODDS_OLD=1 — прежнее. Платит рантайм (mission/msn_run.c), он обязан звать эту же функцию, иначе карточка и выплата разойдутся
#ifndef SV_WAGER_ODDS_OLD
#define SV_WAGER_ODDS_OLD 0
#endif
float (*mvCarOdds)(void);
int mvWagerPay(int fee) {
  if (SV_WAGER_ODDS_OLD) return 2 * fee;
  float o = mvCarOdds ? mvCarOdds() : -1.f;
  if (o < 0.f) o = mvWagerOddsDefault(mvPackLevel());
  return mvWagerPayOf(fee, o);   // формула в msn_wager.h (круг 6: её же берёт хост-тест пары карточка ↔ выплата)
}
// сверка g07b-msnview (P13-10, Д1/P13-10 второго читателя): ставка больше банка — Accept скрыт, вместо описания «нужно N монет»; ✕ не работает, ○ остаётся выходом.
// Было: Accept всегда виден. SV_WAGER_COINS_OLD=1 — прежнее. Строки библии «нужно N монет» в пакете нет (передано: tools/missions/compile.py UI_TEXT_KEYS) — пока свой английский текст
#ifndef SV_WAGER_COINS_OLD
#define SV_WAGER_COINS_OLD 0
#endif
static int wagerShort(void) { return !SV_WAGER_COINS_OLD && H.brWager && H.brFee > shopCoins(&M.shop); }
// Данные-93 (Опора, ставки gr1): этап «взнос» (coins) ждал △, а главный цикл отдаёт △ миссии только у собеседника/точки (mvWantsAction) — взнос не списывался и этап не шёл.
// Оригинал: взнос списывается сам, когда на карточке брифинга нажат ✕ (принять), и этап сразу закрыт; нехватка монет — отказ и отмена миссии. Теперь ✕ на ставочной карточке
// даёт рантайму одно «действие» на следующем кадре (оно и платит). SV_WAGERACCEPT_OLD=1 / flag wageracceptold 1 — прежнее (△ не доходит)
#ifndef SV_WAGERACCEPT_OLD
#define SV_WAGERACCEPT_OLD 0
#endif
int svWagerAcceptOld = SV_WAGERACCEPT_OLD;
static int wagerPayPend;
// сверка g07b-msnview (M2:25): на карточке миссии приглушаются все звуки кроме голоса (режим брифинг-экрана MISSIONSCREEN, как у подсказок обучения), на выходе возвращаются.
// Звук ✕ Continue (FE_CONTINUE) играет ui.c через pop-хук (передано). Было: звук шёл как в игре. SV_BRIEFMODE_OLD=1 — прежнее
#ifndef SV_BRIEFMODE_OLD
#define SV_BRIEFMODE_OLD 0
#endif
static int brSavedMode = -1;
static void brModeOn(void) { if (SV_BRIEFMODE_OLD || brSavedMode >= 0) return; brSavedMode = sndModeCur(); sndMode(SND_MODE_MISSIONSCREEN); }
static void brModeOff(void) { if (brSavedMode < 0) return; if (sndModeCur() == SND_MODE_MISSIONSCREEN) sndMode(brSavedMode); brSavedMode = -1; }
static float briefSec(void) { return (unsigned)(sceKernelGetSystemTimeLow() - H.brT0) * 1e-6f; }
// №629 (Роб: между миссиями газеты нет, есть только карточка): Continue не срабатывает, пока фоновое чтение персонажей этапа не кончилось, но не дольше BRIEF_RA_MAX сверх
// прежней готовности (картинка). Машины этапа читаются синхронно в mvPump-кадрах газеты/в кадрах карточки сразу при открытии. MSN_READAHEAD_OLD=1 — без ожидания
#ifndef MSN_READAHEAD_OLD
#define MSN_READAHEAD_OLD 0
#endif
#ifndef BRIEF_CONTINUE_READY_OLD
#define BRIEF_CONTINUE_READY_OLD 0   // 1 — Continue появляется по готовности картинки, не дожидаясь чтения впрок (прежнее)
#endif
#define BRIEF_RA_MAX 1.5f
#ifndef SV_RACE_MSTART3_OLD
#define SV_RACE_MSTART3_OLD 0   // круг 3 (проверка g07b): 1 — реплика старта молчит и у ставочных гонок (прежнее); в оригинале только у уличных
#endif
#ifndef SV_RACE_MSTART_OLD
#define SV_RACE_MSTART_OLD 0
#endif
#ifndef SV_ABORTOK_OLD
#define SV_ABORTOK_OLD 0   // сверка g07b круг 2 (g07 30, P07:23): 1 — Cancel вступления и ставки не учитывают AllowMissionAbort("false") (прежнее)
#endif
#ifndef SV_WAGER_CANCEL_OLD
#define SV_WAGER_CANCEL_OLD 0   // сверка g07b (M2 Д3, P13 Д1): 1 — на ставочной карточке отказаться нельзя (прежнее)
#endif
static float brReadyT = -1.f;   // с какого времени брифинга всё прежнее готово (для предела ожидания чтения)
static int readaheadBusy(void) { for (int q = 0; q < NPRE; q++) if (pre[q].p && !skinPreReady(pre[q].p)) return 1; return 0; }
static int briefReadyBase(void);
// 109 (а) (Fable ПОРЯДОК_ЗАГРУЗКИ §4 п.1, оригинал: карточка ждёт, пока вся миссия в памяти): к прежним условиям готовности добавлено «очередь msnart пуста» и «модели машин фазы в памяти» (mvPhaseCarsReady — крючок Стены,
// NULL — условия нет). Предел BRIEF_QUEUE_MAX (25 с, надпись LOADING остаётся): дольше — карточка выходит с записью в журнал (лестницу памяти под карточкой держат msnart и модели сами). LOADREADY109_OLD=1 / «flag loadready109old 1» / rollback.txt — как было
#ifndef LOADREADY109_OLD
#define LOADREADY109_OLD 0
#endif
#define BRIEF_QUEUE_MAX 25.f
// 109 (Запекание З9): список ресурсов миссии (msnres-l<N>.shr, tools/missions/msnres.py) — карточка заказывает у msnart ВСЕ записи миссии в порядке списка (вступление, основная часть, значки всех этапов, картинка карточки)
// сразу на старте миссии и считает себя готовой, когда каждая из них прочитана или отказана. Раньше записи основной части и значки поздних этапов просились только когда до них доходила миссия, то есть читались уже ПОД ИГРОЙ.
// Нет файла списка (старый набор) — msnResOpen = -1, всё как было. LOADLIST109_OLD=1 / flag loadlist109old 1 — прежнее
#ifndef LOADLIST109_OLD
#define LOADLIST109_OLD 0
#endif
// 109/правка Роба 1293 (последняя миссия L1: Бёрнс невидим, пока с ним не заговоришь): модели персонажей миссии (kind CHR списка) раньше читались по ходу этапов (заглядывание вперёд видело
// только следующий этап), и к появлению персонажа модели ещё не было. Теперь карточка заказывает модели ВСЕХ персонажей миссии (обоих фаз) на старте и открывается, когда они в памяти;
// после старта этапа чтений персонажей миссии нет. Слоты те же (модели миссии, не фоновые), память — msnResBytes(CHR) списка. LOADCHR109_OLD=1 / flag loadchr109old 1 — прежнее
#ifndef LOADCHR109_OLD
#define LOADCHR109_OLD 0
#endif
#ifndef FINLINE1317_OLD
#define FINLINE1317_OLD 0
#endif
int mvFinLine1317Old = FINLINE1317_OLD;
#ifndef PROXBAR1318_OLD
#define PROXBAR1318_OLD 0
#endif
int mvProxBar1318Old = PROXBAR1318_OLD;
int mvChr109Old = LOADCHR109_OLD;
// 1303 (Роб: «карточка уезжает, потом ирис, потом опять уезжает»; Fable по записи оригинала): ✕, нажатый, пока карточка ещё на LOADING, запоминался и сам закрывал карточку в миг её готовности — игрок
// не успевал её увидеть, а потом шли переходы миссии. У оригинала нажатия до появления кнопки не считаются. LOADQ1303_OLD=1 / flag loadq1303old 1 — прежнее (помнить ✕ с LOADING)
#ifndef LOADQ1303_OLD
#define LOADQ1303_OLD 0
#endif
int mvLoadQ1303Old = LOADQ1303_OLD;
int mvList109Old = LOADLIST109_OLD; static int resOk109, resMi109 = -1;
void mvResFree109(void) { msnResClose(); resOk109 = 0; resMi109 = -1; }
static int resKeep109(const MsnResEnt *r) { return (r->kind == MSNRES_ART || r->kind == MSNRES_PIC) && !(r->flags & (MSNRES_F_MISSING | MSNRES_F_ENGINE)); }
static int resChr109(const MsnResEnt *r) { return r->kind == MSNRES_CHR && !(r->flags & (MSNRES_F_MISSING | MSNRES_F_ENGINE)) && !strchr(r->key, '#'); }
static void resOrder109(const char *id) {   // MISSION_START: заказ по списку миссии id
  resMi109 = -1; if (mvList109Old || !resOk109 || !id) return;
  int mi = msnResMission(id); if (mi < 0) { logf_("MSN reslist %s none t=%.1f\n", id, clock_); return; }
  resMi109 = mi; unsigned n = msnResCount(mi), ord = 0, gone = 0, chr = 0;
  for (unsigned i = 0; i < n; i++) { const MsnResEnt *r = msnResAt(mi, i); if (r && !mvChr109Old && resChr109(r)) chr++; }
  for (unsigned i = 0; i < n; i++) { const MsnResEnt *r = msnResAt(mi, i); if (!r || !resKeep109(r)) continue; msnartLoad(r->key, 0); ord++; if (msnartPathState109(r->key) < 0) gone++; }
  logf_("MSN reslist %s entries=%u ordered=%u notintable=%u chars=%u peak=%u t=%.1f\n", id, n, ord, gone, chr, msnResPeak(mi), clock_);
}
static int resReady109(void) {   // 1 — всё заказанное прочитано или отказано (в очереди и в чтении нет)
  if (resMi109 < 0) return 1;
  unsigned n = msnResCount(resMi109);
  for (unsigned i = 0; i < n; i++) { const MsnResEnt *r = msnResAt(resMi109, i); if (r && resKeep109(r) && msnartPathState109(r->key) == 0) return 0; }
  if (!mvChr109Old) for (unsigned i = 0; i < n; i++) { const MsnResEnt *r = msnResAt(resMi109, i); if (r && resChr109(r) && !modelFor(r->key, 0)) return 0; }   // модель персонажа в памяти (чтение в фоне, повтор каждый кадр карточки)
  return 1;
}
int mvReady109Old = LOADREADY109_OLD; int (*mvPhaseCarsReady)(void);
static float carsDelay109; static int carsDelayTest109(void) { return briefSec() >= carsDelay109; }
void mvCarsDelay109(float s) { carsDelay109 = s; mvPhaseCarsReady = s > 0.f ? carsDelayTest109 : 0; }   // служебно (flag mvcarsdelay109 N): «модели машин фазы» готовы через N с после открытия карточки — стенд условия готовности
// 1337 (ящик Роба с L4: на карточке «LOADING» ждала ~20 с, а в ящике на это время ни одной записи): на чём карточка ждёт — q (очередь картинок msnart), c (модели машин фазы), l (список ресурсов: записи и персонажи) —
// пишется в ящик при смене причины («brief-wait q1 c0 l1 0.4с») и при готовности («brief-ready … ждала X с»). Поведения не меняет. MSNBRIEFBB1337_OLD=1 / mvBriefBb1337Old (файл briefbb1337old, flag briefbb1337old 1) — не писать
#ifndef MSNBRIEFBB1337_OLD
#define MSNBRIEFBB1337_OLD 0
#endif
int mvBriefBb1337Old = MSNBRIEFBB1337_OLD;
static void briefBb1337(int q, int c, int l, int base) {
  static int last = -1; static float prevSec = 1e9f, firstWait = 0.f; float s = briefSec();
  if (s < prevSec) { last = -1; firstWait = 0.f; }   // новая карточка
  prevSec = s;
  if (mvBriefBb1337Old) return;
  int mask = (!q ? 1 : 0) | (!c ? 2 : 0) | (!l ? 4 : 0) | (!base ? 8 : 0);
  if (mask == last || (last < 0 && !mask)) return;
  char t[64];
  if (mask) { if (last <= 0) firstWait = s; snprintf(t, sizeof t, "q%d c%d l%d base%d %.1fs", q, c, l, base, s); bbOp("brief-wait", t, 0); }
  else { snprintf(t, sizeof t, "ждала %.1f с (с %.1f по %.1f)", s - firstWait, firstWait, s); bbOp("brief-ready", t, 0); }
  last = mask;
}
static int briefReady(void) {
  int r = briefReadyBase();
  if (!r) briefBb1337(1, 1, 1, 0);   // 1337: карточка ждёт картинку/первую загрузку
  if (r && !mvReady109Old && !brOld) {
    int q = msnartIdle(), c = mvPhaseCarsReady ? mvPhaseCarsReady() : 1, l = resReady109();
    briefBb1337(q, c, l, 1);
    if (!q || !c || !l) { if (briefSec() < BRIEF_QUEUE_MAX) return 0; static float lg_ = -1.f; if (lg_ != clock_) { lg_ = clock_; logf_("MSN briefing-ready TIMEOUT queue_idle=%d cars_ready=%d list_ready=%d after %.1f s t=%.1f\n", q, c, l, briefSec(), clock_); } }
  }
  if (!r || MSN_READAHEAD_OLD || BRIEF_CONTINUE_READY_OLD) return r;
  float t = briefSec(); if (brReadyT < 0.f) brReadyT = t;
  return readaheadBusy() && t < brReadyT + BRIEF_RA_MAX ? 0 : 1;
}
static int briefReadyBase(void) {
  if (brOld) return 1;
  if (H.brFirst && briefSec() < BRIEF_FIRST_LOAD) return 0;
  const void *pix = 0; int tw, th, w, h;
  if (!H.present[0] || (msnartImageData(H.present, &pix, &tw, &th, &w, &h) && pix)) {
    if (!BRIEF_ART_PRELOAD_OLD && H.present[0] && !uiBriefPicSettled(3) && briefSec() < 8.0f) return 0;   // №612 (видео TEST17): картинка догружалась уже на показанной карточке — ждём 3 кадра готовой картинки под газетой
    return 1;
  }
  if (!BRIEF_PIC_OLD && !msnartPicWaitOld && msnartPicGaveUp()) {   // Кольцо-96: картинка отказана по памяти после всех повторов — карточка без неё сразу, не 8 с чёрного
    if (!H.brPicLost) { H.brPicLost = 1; logf_("MSN briefing-pic LOST %s after %.1f s lost=%u gaveup=1 t=%.1f\n", H.present, briefSec(), msnartLostPics, clock_); }
    return 1;
  }
  if (!BRIEF_PIC_OLD && briefSec() > BRIEF_PIC_MAX) {   // №512: картинка так и не доехала (чтение не удалось) — не держим игрока на LOADING вечно
    if (!H.brPicLost) { H.brPicLost = 1; logf_("MSN briefing-pic LOST %s after %.1f s lost=%u t=%.1f\n", H.present, briefSec(), msnartLostPics, clock_); }
    return 1;
  }
  return 0;
}
static void titleCb(void *u, const char *id, const char *title, const char *info, int bonus) {   // основная часть миссии: брифинг
  (void)u; uiBriefingBonus(bonus);   // сверка 30.09: бонусная — бордовая карточка
  strncpy(H.title, title ? title : "", sizeof H.title - 1); strncpy(H.info, info ? info : "", sizeof H.info - 1);
  msTryBump();   // сверка g07b P13-3: старт основной части = один учтённый сброс
  // «переходная» миссия конца уровня (L2/L5/L6 m8: ролик SetFMVInfo + таймер SetLevelOver; у L7 m8 — SetGameOver): названия в
  // библии нет (tools/missions/compile.py MF_TRANSITION), брифинга нет — игра не ждёт ✕ перед концом уровня
  if (M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions && (M.f.missions[M.rt.cur].flags & MSN_MF_TRANSITION)) {
    logf_("MSN briefing-skip %s transition t=%.1f\n", id ? id : "", clock_); return; }
  { int fee = 0, par = 0, pay = 0, gm = M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions && (M.f.missions[M.rt.cur].flags & MSN_MF_GAMBLE) && M.f.missions[M.rt.cur].main != MSN_NONE16;
    if (gm) {   // Роб 17.10: карточка гонки на ставку — числа из скрипта миссии (выплата — как платит рантайм msn_run.c mission_done: 2 × SetRaceEnteryFee)
      const MsnPhase *ph = &M.f.phases[M.f.missions[M.rt.cur].main];
      for (uint32_t s_ = 0; s_ < ph->stage_count; s_++) { const MsnStage *st = &M.f.stages[ph->first_stage + s_];
        for (uint32_t i_ = 0; i_ < st->cmd_count; i_++) { const MsnCmd *c = &M.f.cmds[st->cmd_first + i_];
          if (c->op == MSN_OP_SETCOINFEE) fee = msn_arg_i(&M.f, c, 0); else if (c->op == MSN_OP_SETPARTIME) par = msn_arg_i(&M.f, c, 0) * 1000;
          else if (c->op == MSN_OP_SETRACEENTERYFEE) pay = mvWagerPay(msn_arg_i(&M.f, c, 0)); } }
      logf_("MSN wager fee=%d par=%d pay=%d t=%.1f\n", fee, par, pay, clock_); }
    uiHudSet(UI_F_WAGER_FEE, fee); uiHudSet(UI_F_WAGER_PAR, par); uiHudSet(UI_F_WAGER_PAY, pay); uiSetWager(gm && fee > 0); H.brWager = gm && fee > 0; H.brFee = fee; }
  H.briefing = 1; brModeOn(); uiMessage(NULL, -1, 0.f); uiBriefAnim(1); if (!BRIEF_ANIM_AT_LOAD_OLD) uiBriefHold();   // №512: карточка въезжает слева; №994: пока не готова — только LOADING, въезд стартует при готовности
  brReadyT = -1.f; H.brT0 = sceKernelGetSystemTimeLow(); H.brFirst = !brBriefed; H.brLoaded = H.brQueued = H.brPicLost = 0; brBriefed = 1;   // правка 52: LOADING до загрузки
  logf_("MSN briefing %s \"%s\" bitmap=%s first=%d path=%s t=%.1f\n", id ? id : "", H.title, H.present[0] ? H.present : "-", H.brFirst, H.brFirst ? "newspaper" : "card-only", clock_);
}
// цель «fmv» (SetFMVInfo("fmv3.rmv")): ролик играет main.c (viewer/movie.c, movies/<имя>.smv), потом mvFmvDone — цель засчитана
static void fmvCb(void *u, const char *file) {
  (void)u; snprintf(H.fmv, sizeof H.fmv, "%s", file ? file : ""); H.fmvWait = 1;
  logf_("MSN fmv %s t=%.1f\n", H.fmv, clock_);
}
static void talkCb(void *u, const char *name, int icon, float h, float r) { (void)u; (void)icon; (void)r; strncpy(H.talk, name, sizeof H.talk - 1); H.talkH = h; }
static void featureCb(void *u, int feature, const MsnCall *c) {   // LoadP3DFile уровня и миссии: подгрузить то же из msnart.shr
  (void)u; (void)feature;
  if (c->cmd->op == MSN_OP_SETDYNALOADDATA) {   // что держать в памяти на фазе: файлы "…;l4i02.p3d@" и интерьер — старт в комнате
    const char *fl = msn_arg_s(c->f, c->cmd, 0); size_t n = fl ? strlen(fl) : 0;
    const char *in = n && fl[n - 1] == '@' && msn_argc(c->cmd) > 1 ? msn_arg_s(c->f, c->cmd, 1) : "";
    snprintf(H.dynaRoom, sizeof H.dynaRoom, "%s", in ? in : "");
    snprintf(H.dynaFiles, sizeof H.dynaFiles, "%s", fl ? fl : ""); H.dynaSeq++;   // круг 5 (P12-10)
    logf_("MSN dyna room=%s t=%.1f\n", H.dynaRoom[0] ? H.dynaRoom : "-", clock_);
    return;
  }
  if (c->cmd->op != MSN_OP_LOADP3DFILE) return;
  if (c->when != MSN_AT_LEVEL && c->when != MSN_AT_LOAD) return;
  msnartLoad(msn_arg_s(c->f, c->cmd, 0), c->when == MSN_AT_LEVEL);
}
// Правка 122 (сверка 30.09): пропы уличной гонки — StreetRacePropsLoad("l1_sr1p.p3d;") в инициализации гонки. Шины, отбойники,
// стартовая линия — запись msnart «art/l1_sr1p.p3d» (рисунок l1_sr1p, экземпляры — суставы, дальше 70 м не рисуются), заборы
// трассы — racefence.bin (RFN1, tools/msnart_export.py --fences) поверх коллизии мира (col.c colExtraFences), высота — от земли
// −2…+3 м. StreetRacePropsUnload стоит в той же инициализации: в оригинале — «убрать по концу гонки», здесь — cleanup миссии
#define RACE_SEG_MAX 256   // перепись миссий 04.10: заборов на трассе до 222 (L7 sr2; L5 sr2 173, L4 sr1 172) — было 192, хвост терялся
static void *rfData; static unsigned rfSize;
static char raceProps[32]; static ColSeg raceSeg[RACE_SEG_MAX]; static unsigned nRaceSeg;
// №1264 (Бабл 107, сверка Fable A2.10/Б.7; оригинал: пропы гонки и их коллизия — один файл зоны, появляются вместе): заборы трассы включаются, когда рисунок пропов уже в памяти, и каждый — когда под ним
// есть земля (высота считалась один раз, нет земли — y = 0 и забор «не на той высоте»). Нет рисунка и чтение кончилось (отказ по памяти) — заборов нет (WATCHDOG race-fences), невидимых ограждений не бывает.
// RACEFENCE1264_OLD=1 / flag racefence1264old 1 — прежнее (сразу и одним куском)
#ifndef RACEFENCE1264_OLD
#define RACEFENCE1264_OLD 0
#endif
int mvRaceFence1264Old = RACEFENCE1264_OLD;
static ColSeg raceAll[RACE_SEG_MAX]; static unsigned nRaceAll; static unsigned char raceYok[RACE_SEG_MAX]; static int raceFencePend, raceFencePub; static float raceFenceT0, raceFenceNext;
extern int mvRaceExitPlant;   // (определение в game_msn.c, int107) служебное (flag racexitplant 1): красный контроль гейта выходов — уборка гонку не снимает
static void raceOff(void) { if (mvRaceExitPlant) return; if (raceProps[0]) bbOp("race-", raceProps, 0); raceProps[0] = 0; nRaceSeg = 0; nRaceAll = 0; raceFencePend = 0; raceFencePub = 0; colExtraFences(NULL, 0); }
static void raceFences(const char *stem) {
  nRaceSeg = 0; nRaceAll = 0; raceFencePend = 0; raceFencePub = 0; colExtraFences(NULL, 0);
  const unsigned char *p = rfData; if (!p || rfSize < 8 || memcmp(p, "RFN1", 4)) return;
  unsigned n; memcpy(&n, p + 4, 4); unsigned off = 8;
  for (unsigned r = 0; r < n && off + 36 <= rfSize; r++) {
    char nm[33]; memcpy(nm, p + off, 32); nm[32] = 0; unsigned k; memcpy(&k, p + off + 32, 4); off += 36;
    if (off + k * 24 > rfSize) return;
    if (!strcasecmp(nm, stem)) {
      for (unsigned i = 0; i < k && nRaceSeg < RACE_SEG_MAX; i++) {
        float f[6]; memcpy(f, p + off + i * 24, 24); ColSeg *g = &raceSeg[nRaceSeg++];
        g->a[0] = f[0]; g->a[1] = f[1]; g->b[0] = f[2]; g->b[1] = f[3]; g->n[0] = f[4]; g->n[1] = f[5];
        float y; int ok = gameMsnGroundY(&M, (f[0] + f[2]) * .5f, (f[1] + f[3]) * .5f, &y); if (!ok) y = 0.f;
        g->y0 = y - 2.f; g->y1 = y + 3.f;
        if (!mvRaceFence1264Old) { raceAll[i] = *g; raceYok[i] = (unsigned char)ok; }
      }
      if (!mvRaceFence1264Old) { nRaceAll = nRaceSeg; nRaceSeg = 0; raceFencePend = 1; raceFenceT0 = clock_; raceFenceNext = 0.f; colExtraFences(NULL, 0); return; }   // №1264: включает raceFenceTick
      colExtraFences(raceSeg, nRaceSeg); return;
    }
    off += k * 24;
  }
}
#define RACEFENCE_WAIT_S 4.f   // чтение записи пропов кончилось, а рисунка нет: ждём столько и отказываемся от заборов
static void raceFenceTick(void) {
  if (!raceFencePend || clock_ < raceFenceNext) return;
  raceFenceNext = clock_ + .25f;
  int art = msnartHas(raceProps);
  if (!art) {
    if (!msnartReading() && clock_ - raceFenceT0 > RACEFENCE_WAIT_S) { raceFencePend = 0; logf_("WATCHDOG race-fences nopics %s fences=0/%u t=%.1f\n", raceProps, nRaceAll, clock_); }
    return;
  }
  unsigned have = 0, done = 0, add = 0;
  for (unsigned i = 0; i < nRaceAll; i++) {
    if (!raceYok[i]) { float y; if (gameMsnGroundY(&M, (raceAll[i].a[0] + raceAll[i].b[0]) * .5f, (raceAll[i].a[1] + raceAll[i].b[1]) * .5f, &y)) { raceAll[i].y0 = y - 2.f; raceAll[i].y1 = y + 3.f; raceYok[i] = 1; add++; } }
    done += raceYok[i];
  }
  have = done;
  if (add || !raceFencePub) {   raceFencePub = 1;   // набор растёт: пересобрать подряд лежащие участки с землёй и отдать коллизии заново
    nRaceSeg = 0; for (unsigned i = 0; i < nRaceAll; i++) if (raceYok[i]) raceSeg[nRaceSeg++] = raceAll[i];
    colExtraFences(raceSeg, nRaceSeg);
    logf_("MSN race-fences %s on=%u/%u art=1 t=%.1f\n", raceProps, have, nRaceAll, clock_);
  }
  if (done >= nRaceAll || clock_ - raceFenceT0 > 30.f) raceFencePend = 0;   // не пришла земля за 30 с — остальные участки далеко и не нужны
}
static void racePropsCb(void *u, const char *name, int load) {
  (void)u; if (!load || !name) return;   // Unload — по концу гонки (cleanupCb)
  char stem[32]; int n = 0;
  for (; name[n] && name[n] != '.' && name[n] != ';' && name[n] != ':' && n < 31; n++) stem[n] = (char)(name[n] >= 'A' && name[n] <= 'Z' ? name[n] + 32 : name[n]);
  stem[n] = 0; if (!n) return;
  // Правка Роба (В64, 10.10): запись PS2 (ps2-obs-video-explosion-booth-items: m7, старт 39:53) — трасса выходит через главный въезд АЭС, проезд открыт, ворот и
  // шлагбаума нет; ворота l1m7door (стена x −13,2 × z −301…−293 высотой 10,8 м у PP_entrance_in) перекрывали этот въезд. Не ставятся (меш и заборы).
  // M7_DOOR_OLD=1 — как было (ворота по m7i.mfk)
#ifndef M7_DOOR_OLD
#define M7_DOOR_OLD 0
#endif
  if (!M7_DOOR_OLD && !strcmp(stem, "l1m7door")) { logf_("MSN race-props %s skipped (V64) t=%.1f\n", stem, clock_); return; }
  char path[48]; snprintf(path, sizeof path, "art/%s.p3d", stem); msnartLoad(path, 0);
  bbOp("race+", stem, 0);   // правка 103: самописец — пропы и заборы гонки
  snprintf(raceProps, sizeof raceProps, "%s", stem); raceFences(stem);
  logf_("MSN race-props %s fences=%u t=%.1f\n", stem, mvRaceFence1264Old ? nRaceSeg : 0u, clock_);   /* №1264: заборы включит raceFenceTick (строки MSN race-fences) */
}
// правка 122 (сверка 30.09, запись PS2 26:50): машина-награда (бонусная миссия, все уличные гонки) — окно сообщения Message.scr
// «Congratulations! You've unlocked Cletus' Pickup Truck. Go to a phone booth to use it.» (текст — UNLOCK_<модель> пакета миссий,
// tools/missions/compile.py), через MBOX_DELAY после события (сначала вспышка «BONUS MISSION COMPLETE!»); ✕ / ○ — закрыть, мир стоит
#define MBOX_DELAY 2.5f
static char mboxText[200]; static float mboxAt = -1.f;
static void unlockCb(void *u, const MsnReward *r) {
  (void)u; if (!r || !r->name) return;
  char key[48]; snprintf(key, sizeof key, "UNLOCK_%s", r->name);
  const char *t = msn_text(&M.f, key); if (!t || !*t) return;
  snprintf(mboxText, sizeof mboxText, "%s", t); mboxAt = clock_ + MBOX_DELAY;
  logf_("MSN unlock-message %s t=%.1f\n", r->name, clock_);
}
void mvServiceFlash(int bonus) { uiFlashCb(0, bonus ? MSN_FLASH_BONUS_COMPLETE : MSN_FLASH_MISSION_COMPLETE, msn_text(&M.f, bonus ? "BONUS_MISSION_COMPLETE" : "MISSION_COMPLETE")); }   // служебное (flag msnflash)
void mvServiceUnlock(const char *name) { MsnReward r; memset(&r, 0, sizeof r); r.name = name; unlockCb(0, &r); }   // служебное (flag unlockmsg)
int mvMsgBox(const char **text) {
  if (!active || mboxAt < 0.f || clock_ < mboxAt || H.briefing || H.failed) return 0;
  if (text) *text = mboxText; return 1;
}
void mvMsgBoxInput(unsigned hit) {
  if (!(hit & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE))) return;
  mboxAt = -1.f; int a = sndId("accept"); if (a >= 0) sndPlay2D(a, 1.f);
  logf_("MSN unlock-message close t=%.1f\n", clock_);
}
// правка 122 (сверка 30.09, запись PS2 31:34, 37:29, 28:31): затемнения миссий — SetFadeOut(t) в конце этапа: экран за t уходит в
// чёрное (рантайм ждёт t до следующего этапа), StayInBlack — этап идёт в чёрном (Гомера переставляют), следующий этап без него —
// из чёрного за FADE_IN. Рисует main.c (mvFadeAlpha) поверх мира, под интерфейсом
#define FADE_IN .5f
static float fadeA, fadeLen = 1.f; static int fadeDir, stayBlack;
// сверка g07b-msnview (P06-9): SetFadeOut число не читает — уход в чёрное 0,25 с, чёрный держится не меньше 2,0 с (отсчёт от полного ухода), выход из чёрного 0,25 с.
// Было: длина = число скрипта (0,1; 1,0; 2), выход 0,5 с сразу. Ждёт рантайм: mission/msn_run.c:426 (пауза этапа = число; нужно 0,25 + 2,0) — передано g07-missions.
// SV_FADE_OLD=1 — прежнее
#ifndef SV_FADE_OLD
#define SV_FADE_OLD 0
#endif
#define SV_FADE_OUT .25f
#define SV_FADE_HOLD 2.0f
static int rsPend;   // перезапуск ждёт чёрного экрана: 1 — Restart паузы, 2 — Retry (restartIris)
// 1304 (Роб: «yes» на экране провала — повтора нет, как будто no; и на L2 m2): пока перезапуск ждёт чёрного (rsPend), старт этапа (MSN_EV_STAGE_START) и «ирис открыть» (MSN_EV_IRIS) разворачивали затемнение обратно, и отметка терялась до следующего чёрного.
// Теперь пока отметка взведена, затемнение не разворачивается, а если за RSPEND_FORCE_S чёрного так и нет — перезапуск идёт принудительно. RSPEND1304_OLD=1 / flag rspend1304old 1 — как было
#ifndef RSPEND1304_OLD
#define RSPEND1304_OLD 0
#endif
int mvRsPend1304Old = RSPEND1304_OLD;
#undef RSPEND1304_OLD
#define RSPEND1304_OLD mvRsPend1304Old
#define RSPEND_FORCE_S 1.5f
static float rsPendT;
// 1316 (Роб, L2 m3: победа в гонке → ирис → снова карточка и гонка → чёрный экран с HUD, выйти нельзя). Причины: (1) отметка перезапуска rsPend от несработавшего «yes» жила, пока не придёт следующий чёрный экран, — им стал финиш m3 (SetFadeOut) и миссия перезапустилась;
// (2) затемнение шло игровым dt, а под камерой миссии он 0 — чёрный замерзал, пауза при затемнении запрещена. Теперь: затемнение, срок принудительного перезапуска и выдержка считаются настенным временем; отметка привязана к миссии и снимается при её смене, конце и закрытии;
// сторож (канон 12): полностью чёрный экран > 5 с без причины (этап StayInBlack, выдержка, загрузка) открывается строкой WATCHDOG fade-stuck; пауза не запрещается затемнением дольше 3 с. FADEGUARD1316_OLD=1 / flag fadeguard1316old 1 — как было
#ifndef FADEGUARD1316_OLD
#define FADEGUARD1316_OLD 0
#endif
int mvFadeGuard1316Old = FADEGUARD1316_OLD;
// 1326 (Роб, 108 на PSP: «в начале каждой миссии 2–3 с чёрный экран: счётчик идёт, видны только элементы интерфейса, карты не видно, а я могу газовать»): после ✕ карточки экран закрывает слой затемнения или мир не нарисован,
// а игра и интерфейс идут. Канон 14: экран раскрывается один раз, когда мир нарисован; до этого интерфейс не показан, управление, отсчёт и таймер стоят. main_loop.h следит за 10 с после ✕: кадров с миром почти без
// треугольников подряд или чёрного слоя затемнения без причины (не этап StayInBlack и не перезапуск) — интерфейс прячется, dt = 0, не дольше 5 с (потом идёт как раньше, строка в журнал и ящик). WORLDHOLD1326_OLD=1 / flag worldhold1326old 1 — прежнее
#ifndef WORLDHOLD1326_OLD
#define WORLDHOLD1326_OLD 0
#endif
int mvWorldHold1326Old = WORLDHOLD1326_OLD; unsigned mvContUs1326; static float wbForceEnd = -1.f;
#undef FADEGUARD1316_OLD
#define FADEGUARD1316_OLD mvFadeGuard1316Old
#ifndef FADECLK1362B_OLD
#define FADECLK1362B_OLD 0
#endif
int mvFadeClk1362bOld = FADECLK1362B_OLD;   // 1362Б (Fable, корень класса 1304/1316/1326/1349): часы затемнения под карточкой шли по игровому dt = 0. flag fadeclk1362bold 1 / rollback.txt — как было
float mvWallDt1316;   // настоящее время кадра (main_loop.h ставит перед mvTick: под камерой миссии игровой dt = 0)
static float fclk_, fadeStuckT, fadeBlackT;   // часы затемнения (настенные); сколько подряд чёрный без причины; сколько подряд чёрный вообще
static int rsPendMsn = -1;   // миссия, которой принадлежит отметка перезапуска
#define FCLK (FADEGUARD1316_OLD ? clock_ : fclk_)
static void restartRun(int kind);
static float blackAt = -1.f; static int fadeInPend, fadeHoldNeed;   // blackAt — когда экран стал полностью чёрным; fadeInPend — выход из чёрного ждёт конца выдержки
static void fadeInStart(void) {   // выход из чёрного (конец этапа StayInBlack / начало следующего этапа)
  if (SV_FADE_OLD) { fadeDir = -1; fadeLen = FADE_IN; return; }
  if (fadeHoldNeed && (fadeDir > 0 || blackAt < 0.f || FCLK < blackAt + SV_FADE_HOLD)) { fadeInPend = 1; return; }   // чёрный ещё не выдержан — выход отложен до fadeTick
  fadeDir = -1; fadeLen = SV_FADE_OUT; fadeInPend = 0; fadeHoldNeed = 0; blackAt = -1.f;
}
static void fadeCb(void *u, int out, float t) {
  (void)u;
  if (out) { fadeDir = 1; fadeLen = SV_FADE_OLD ? (t > .05f ? t : .05f) : SV_FADE_OUT; fadeHoldNeed = !SV_FADE_OLD; fadeInPend = 0; blackAt = -1.f; }
  else if (SV_FADE_OLD) { fadeDir = -1; fadeLen = t > .05f ? t : FADE_IN; }
  else fadeInStart();
}
static void stayBlackCb(void *u) { (void)u; stayBlack = 1; fadeA = 1.f; fadeDir = 0; }
int mvFadeCover1326(void) { return active && fadeA >= .9f && !stayBlack && !rsPend && !H.failed && !H.levelDone && !H.levelEnd && !H.fmvWait && M.rt.status == MSN_RUNNING; }
void mvServiceWorldBlack(float s) { wbForceEnd = fclk_ + s; fadeA = 1.f; fadeDir = 0; logf_("WORLDBLACK1326 forced %.1f s t=%.1f\n", s, clock_); }   // служебное (flag worldblack S): чёрный слой затемнения на S с, как у PSP (гейт tests/missions/worldhold1326.sh)
float mvFadeAlpha(void) { return active ? fadeA : 0.f; }
float mvFadePauseAlpha(void) { return !FADEGUARD1316_OLD && fadeBlackT > 3.f ? 0.f : mvFadeAlpha(); }   // 1316: запрет паузы по затемнению — не дольше 3 с чёрного
void mvServiceFade(int out, float t) { fadeCb(0, out, t); }   // служебное: flag fadeout / fadein (проверка затемнения)
// 1349 (Баблу: Restart из паузы на данных 108 — ~5 с чёрного и WATCHDOG fade-stuck; те же 5 с в цикле Yes → карточка → ✕ у Роба, 1331/1326): перезапуск (rspend-fire) ставит выход из чёрного (fadeA = 1, fadeDir = −1) в кадре с dt = 0
// (под карточкой мир стоит), а fadeTick считал «дошло до чёрного» по одному fadeA >= 1 без учёта направления: выход из чёрного сам отменялся (fadeDir = 0), экран оставался чёрным, пока сторож 1316 через 5 с не снимал его.
// Теперь «дошло до чёрного» только при уходе в чёрное (fadeDir > 0). FADEDIR1349_OLD=1 / flag fadedir1349old 1 — как было
#ifndef FADEDIR1349_OLD
#define FADEDIR1349_OLD 0
#endif
int mvFadeDir1349Old = FADEDIR1349_OLD;
static void fadeTick(float dt) {
  if (fadeDir) { fadeA += (float)fadeDir * dt / fadeLen; if (fadeA >= 1.f && (fadeDir > 0 || mvFadeDir1349Old)) { fadeA = 1.f; fadeDir = 0; if (blackAt < 0.f) blackAt = FCLK; } if (fadeA > 1.f) fadeA = 1.f; if (fadeA <= 0.f && (RSPEND1304_OLD || fadeDir < 0)) { fadeA = 0.f; fadeDir = 0; } }   // 1304 (корень): «Yes»/Restart зовут restartIris в кадре с dt = 0 (экран провала и пауза — мир стоит); затемнение «из прозрачного в чёрное» (dir > 0, fadeA = 0) конец «прозрачный» обрывал в том же кадре, и перезапуск ждал чёрного вечно. Теперь конец «прозрачный» — только при движении к прозрачному
  if (rsPend && !FADEGUARD1316_OLD && (!active || M.rt.cur != rsPendMsn || M.rt.status == MSN_MISSION_DONE || M.rt.status == MSN_LEVEL_DONE)) {   // 1316: отметка чужая (миссия сменилась, пройдена или закрыта) — снять и не перезапускать
    logf_("MSN rspend-drop kind=%d msn=%d cur=%d status=%d t=%.1f\n", rsPend, rsPendMsn, M.rt.cur, M.rt.status, clock_); rsPend = 0; if (fadeA >= 1.f && fadeDir == 0 && !stayBlack) { fadeDir = -1; fadeLen = SV_FADE_OUT; fadeInPend = 0; fadeHoldNeed = 0; blackAt = -1.f; } }
  if (rsPend) rsPendT += dt; else rsPendT = 0.f;
  if (rsPend && !RSPEND1304_OLD && rsPendT >= RSPEND_FORCE_S && !(fadeA >= 1.f && fadeDir == 0)) { logf_("MSN restart-force kind=%d fade=%.2f dir=%d t=%.1f\n", rsPend, fadeA, fadeDir, clock_); fadeA = 1.f; fadeDir = 0; }   // 1304: чёрного нет 1,5 с — перезапуск принудительно
  if (rsPend && fadeA >= 1.f && fadeDir == 0) { int k = rsPend; rsPend = 0; if (!FADEGUARD1316_OLD) logf_("MSN rspend-fire kind=%d wait=%.1f t=%.1f\n", k, rsPendT, clock_); restartRun(k); { char b_[100]; snprintf(b_, sizeof b_, "kind=%d stage=%d.%d status=%d rem=%.1f wait=%.1f", k, M.rt.phase, M.rt.stage, M.rt.status, M.rt.remaining, rsPendT); bbNoteDiag("RESTART", b_); } /* 1331: старт повтора — в ящик */ fadeA = 1.f; fadeDir = -1; fadeLen = SV_FADE_OUT; fadeHoldNeed = 0; fadeInPend = 0; blackAt = -1.f; }   // сверка g07b M2 Д4: ирис закрыт — перезапуск и открытие
  if (!SV_FADE_OLD && fadeInPend && fadeDir == 0 && fadeA < 1.f) { fadeInPend = 0; fadeHoldNeed = 0; }
  if (!FADEGUARD1316_OLD) {   // 1316: сторож затемнения
    static int wasBlack; int blk = fadeA >= .99f && fadeDir == 0;
    if (blk != wasBlack) { wasBlack = blk; logf_("FADE1316 %s wall=%.1f t=%.1f stay=%d rs=%d pend=%d\n", blk ? "black" : "clear", fclk_, clock_, stayBlack, rsPend, fadeInPend); }
    fadeBlackT = blk ? fadeBlackT + dt : 0.f;
    int why = stayBlack || rsPend || fadeInPend || M.rt.load_busy || M.rt.status == MSN_LEVEL_DONE || H.levelDone || H.levelEnd || H.fmvWait;
    fadeStuckT = blk && !why ? fadeStuckT + dt : 0.f;
    if (fadeStuckT > 5.f) { logf_("WATCHDOG fade-stuck a=%.2f black=%.1f st=%d.%d status=%d t=%.1f\n", fadeA, fadeStuckT, M.rt.phase, M.rt.stage, M.rt.status, clock_); fadeDir = -1; fadeLen = SV_FADE_OUT; fadeHoldNeed = 0; blackAt = -1.f; fadeStuckT = 0.f; }
  }
  if (!SV_FADE_OLD && fadeInPend && fadeA >= 1.f && fadeDir == 0 && blackAt >= 0.f && FCLK >= blackAt + SV_FADE_HOLD) fadeInStart();   // сверка g07b P06-9: выдержка кончилась — выходим из чёрного
}
static void cleanupCb(void *u) {
  (void)u; brModeOff(); failStoreOff(); oovOn = 0; dropOn = 0; lapT0 = -1.f; lapLast = 0; raceOff(); if (!rsPend) { fadeA = 0.f; fadeDir = 0; } stayBlack = 0; fadeInPend = 0; fadeHoldNeed = 0; blackAt = -1.f;   // правка 122: пропы и заборы гонки — до конца гонки
  H.failed = 0; H.markerOn = 0; H.talk[0] = 0; if (!(mvBubble1264Old & 4)) holdOn = 0; uiMessage(NULL, -1, 0.f); bbNoteMsnEnd("cleanup"); iconNote("cleanup");
  if (!rsPend && !msnRetrying109) msnartMissionEnd109();   // 109 (е): конец миссии без повтора — её записи msnart снимаются сейчас, не на старте следующей   // №1264в: ждущий текст снимается вместе с показанным
  uiHudShow(UI_T_MISSION_FAILED, 0); uiHudShow(UI_T_TIMER, 0); uiHudShow(UI_T_COLLECTIBLES, 0); uiHudShow(UI_T_COUNTDOWN, 0);
  animPrune();
}
static char talkConv[24];   // правка 40: разговор, который идёт (имя из SetDialogueInfo) — строка TALKCAM
// Аудит 09.10 п. 12/58: реплики героя и соперника по событиям миссии (смысл семейств — по записям диска, см. tools/sound_export.py).
// ПРАВИЛО ПОРТА: итог миссии — через 0,6 с (свой разговор миссии _win/_lose, если есть, начнётся раньше и перебьёт), при сопернике
// с репликами — его V_Mvic/V_Mfail, иначе героя W_Mvic/W_Mfail; обгон: игрок вперёд — W_Pass, соперник вперёд — через раз V_Tail
// соперника или W_Passed; удар по машине соперника (урон вырос) — V_Damage. MV_VO_OLD=1 — как до аудита (реплик нет)
#ifndef MV_VO_OLD
#define MV_VO_OLD 0
#endif
static char voRival[16];            // водитель-соперник текущей миссии с репликами в пакете (AddStageVehicle, 5-й аргумент)
static int voPending = -1; static float voAt;   // 0 — победа, 1 — провал; когда сказать (clock_)
static float voDmg[MC_MAX];
static unsigned char voDestroyed[MC_MAX];
// сверка g07b-msnview круг 2 (g08-sound, P09:11): реплики соперника V_* по условиям оригинала. V_Damage / V_HitCar — машина игрока ударила машину соперника этапа ближе 8,7 м
// (квадрат 75), соперник не стоит; V_Damage если цель «уничтожить», иначе V_HitCar. V_Mvic — только цель «уничтожить». V_Mfail — только в погоне (цель losetail: за игроком гонятся).
// V_Tail — когда игрок потерял слежку (провал followdistance); из гонок (смена места) V_Tail убран. Было: V_Damage при росте урона соперника на 5 %, V_HitCar при ударе соперника,
// V_Tail при смене позиции в гонке, V_Mvic/V_Mfail всегда. SV_VOICE_COND_OLD=1 — прежнее
#ifndef SV_VOICE_COND_OLD
#define SV_VOICE_COND_OLD 0
#endif
static int voReason, voObj;
static void voMissionEnd(int fail) { if (!MV_VO_OLD) { voPending = fail; voAt = clock_ + .6f; voObj = M.rt.obj; } }
static void voTick(void) {
  if (MV_VO_OLD) return;
  if (M.rt.status == MSN_RUNNING) {   // соперник этой миссии и удары по нему
    voRival[0] = 0;
    for (int i = 0; i < M.cars.n; i++) {
      const McCar *c = &M.cars.c[i]; if (!c->on) { voDmg[i] = 0.f; voDestroyed[i] = 0; continue; }
#ifndef SND_VO_EXTRA_OLD
      if (c->car.destroyed && !voDestroyed[i]) sndSay(SND_SAY_AIDESTROY, 0);
#endif
      voDestroyed[i] = c->car.destroyed;
      if (c->driver[0] && strcasecmp(c->driver, "none") && sndRivalHas(c->driver)) {
        snprintf(voRival, sizeof voRival, "%s", c->driver);
                if (!SV_VOICE_COND_OLD) {
          const Car *pc = M.g ? M.g->pCar : 0;
          if (pc && pc->carHitV > 0.f && c->mode != MC_PARKED) { float dx = pc->x - c->car.x, dz = pc->z - c->car.z;
            if (dx * dx + dz * dz < 75.f) sndRivalSay(c->driver, M.rt.obj == MSN_OBJ_DESTROY ? SND_V_DAMAGE : SND_V_HITCAR, 0); }
        } else {
#ifndef SND_VO_EXTRA_OLD
        if (c->car.carHitV > 0.f) sndRivalSay(c->driver, SND_V_HITCAR, 0);
        else
#endif
        if (c->car.damage > voDmg[i] + .05f) sndRivalSay(c->driver, SND_V_DAMAGE, 0);
        }
      }
      voDmg[i] = c->car.damage;
    }
  }
  if (voPending >= 0 && clock_ >= voAt) {
    int f = voPending; voPending = -1;
    int said = 0;
    if (voRival[0]) {
      if (SV_VOICE_COND_OLD) said = sndRivalSay(voRival, f ? SND_V_MFAIL : SND_V_MVIC, 1);
      else if (f) said = voReason == MSN_FAIL_FOLLOWDISTANCE ? sndRivalSay(voRival, SND_V_TAIL, 1) : voObj == MSN_OBJ_LOSETAIL ? sndRivalSay(voRival, SND_V_MFAIL, 1) : 0;
      else said = voObj == MSN_OBJ_DESTROY ? sndRivalSay(voRival, SND_V_MVIC, 1) : 0;
    }
    if (!said) sndSay(f ? SND_SAY_MFAIL : SND_SAY_MVIC, 1);
  }
}
static void voRace(int pos, int prev) {   // место игрока сменилось (1 — впереди)
  if (MV_VO_OLD || pos <= 0 || prev <= 0 || pos == prev) return;
  static int alt;
  if (pos < prev) sndSay(SND_SAY_PASS, 0);
  else if (!SV_VOICE_COND_OLD) sndSay(SND_SAY_PASSED, 0);   // сверка g07b круг 2 (P09:11): V_Tail из гонок убран
  else if (!(voRival[0] && (alt ^= 1) && sndRivalSay(voRival, SND_V_TAIL, 0))) sndSay(SND_SAY_PASSED, 0);
}
// №629 (Роб: загрузка может стать дольше, лишь бы потом без перегруза): TEST18 — машина гонки (MCR load 40 мс в кадре) и NPC следующей фазы читались ПОСЛЕ Continue, потому что
// заглядывание вперёд видело только следующий этап той же фазы, а у брифинга фаза intro кончается Continue и начинается main. Теперь на последнем этапе фазы intro читаем впрок
// первый этап фазы main (персонажи — фоновое чтение, машины — mcrPreStart): чтение идёт, пока игрок читает газету/карточку. MSN_READAHEAD_OLD=1 — прежнее
#ifndef MSN_READAHEAD_OLD
#define MSN_READAHEAD_OLD 0
#endif
static void lookAheadStage(const MsnStage *st) {
  for (unsigned c = 0; c < st->cmd_count; c++) {
    const MsnCmd *k = &M.f.cmds[st->cmd_first + c];
    if (k->op == MSN_OP_ADDNPC) modelPrefetch(msn_arg_s(&M.f, k, 0));
    else if (!MSN_READAHEAD_OLD && k->op == MSN_OP_ADDSTAGEVEHICLE) { const char *cn = msn_arg_s(&M.f, k, 0);
      logf_("MSN readahead veh arg=%s hook=%d\n", cn ? cn : "-", mvCarPrefetch != 0);
      if (cn && *cn && mvCarPrefetch) { mvCarPrefetch(cn); logf_("MSN readahead car %s t=%.1f\n", cn, clock_); } }
  }
}
// 109 (в, ПОРЯДОК_ЗАГРУЗКИ §4 п.3): чтение вперёд — на ВСЕ машины фазы, а не только следующего этапа: в начале фазы (под карточкой/газетой) читаем модели всех ADDSTAGEVEHICLE её этапов (до MSN_PHASECARS_MAX имён);
// повтор раз в секунду, пока не все в памяти (чтение синхронное, один отказ не ломает: лестница и паузы в main_render.h). MSN_PHASECARS109_OLD=1 — как было (следующий этап)
#ifndef MSN_LOADWAIT_VIEW_OLD
#define MSN_LOADWAIT_VIEW_OLD 0   // 109 (г): 1 — хост не просит ждать очередь
#endif
int mvLoadWait109Old = MSN_LOADWAIT_VIEW_OLD;   // flag msnloadwait109old
#undef MSN_LOADWAIT_VIEW_OLD
#define MSN_LOADWAIT_VIEW_OLD mvLoadWait109Old
#ifndef MSN_PHASECARS109_OLD
#define MSN_PHASECARS109_OLD 0
#endif
int mvPhaseCars109Old = MSN_PHASECARS109_OLD;   // flag msnphasecars109old
#undef MSN_PHASECARS109_OLD
#define MSN_PHASECARS109_OLD mvPhaseCars109Old
#define MSN_PHASECARS_MAX 6
static char phaseCars[MSN_PHASECARS_MAX][24]; static int nPhaseCars, phaseKey = -1; static float phaseCarsT;
static void phaseCarsCollect(void) {
  int key = M.rt.cur; if (key == phaseKey) return; phaseKey = key; nPhaseCars = 0; phaseCarsT = 0.f;   // ключ — миссия: у оригинала в памяти вся миссия (вступление и основная часть), не только текущая фаза
  if (MSN_PHASECARS109_OLD || M.rt.cur < 0 || (unsigned)M.rt.cur >= M.f.nmissions) return;
  const MsnMission *ms = &M.f.missions[M.rt.cur];
  for (int w = 0; w < 2; w++) { unsigned pi = w ? ms->main : ms->intro; if (pi == MSN_NONE16 || pi >= M.f.nphases) continue; const MsnPhase *ph = &M.f.phases[pi];
    for (unsigned q = 0; q < ph->stage_count && nPhaseCars < MSN_PHASECARS_MAX; q++) { const MsnStage *sg = &M.f.stages[ph->first_stage + q];
      for (unsigned c = 0; c < sg->cmd_count && nPhaseCars < MSN_PHASECARS_MAX; c++) { const MsnCmd *k = &M.f.cmds[sg->cmd_first + c]; if (k->op != MSN_OP_ADDSTAGEVEHICLE) continue;
        const char *cn = msn_arg_s(&M.f, k, 0); if (!cn || !*cn) continue; int dup = 0; for (int i = 0; i < nPhaseCars; i++) if (!strcasecmp(phaseCars[i], cn)) dup = 1;
        if (!dup) { strncpy(phaseCars[nPhaseCars], cn, 23); phaseCars[nPhaseCars][23] = 0; nPhaseCars++; } } } }
  logf_("MSN phasecars n=%d (вся миссия) t=%.1f\n", nPhaseCars, clock_);
}
// 109: крючок Бабла mvPhaseCarsReady (msn_view.h, готовность карточки): все машины фазы в памяти (или их уже не будет — mvCarReady). 1 — готово; пока 0, карточка миссии ждёт (предел 25 с у Бабла)
// 1362 (слово Роба: «всё нужно загрузить во время загрузки с газетой»): под газетой дочитываются машины ВСЕЙ миссии (вступление и основная часть: phaseCars), а не только текущего этапа; раньше они читались
// синхронно уже в первом кадре мира (BBOP car+ cDonut sync=1, 50 мс) и под карточкой. Возвращает, сколько машин фазы ещё не в памяти (и просит их у чтения). 0 — готово или условия нет.
int mvLoadPrime1362(void) {
  if (!active || MSN_PHASECARS109_OLD || !mvCarReady || !mvCarPrefetch || M.rt.cur < 0) return 0;
  phaseCarsCollect(); int n = 0;
  for (int i = 0; i < nPhaseCars; i++) if (!mvCarReady(phaseCars[i])) { n++; mvCarPrefetch(phaseCars[i]); }
  return n;
}
static int phaseCarsAllReady(void) {
  phaseCarsCollect(); if (MSN_PHASECARS109_OLD || !mvCarReady) return 1;
  int ok = 1; for (int i = 0; i < nPhaseCars; i++) if (!mvCarReady(phaseCars[i])) { ok = 0; if (mvCarPrefetch) mvCarPrefetch(phaseCars[i]); }
  return ok;
}
int mvPhaseCarHeld(const char *name) {
  if (MSN_PHASECARS109_OLD || !name || M.rt.cur < 0 || phaseKey != M.rt.cur) return 0;
  for (int i = 0; i < nPhaseCars; i++) if (!strcasecmp(phaseCars[i], name)) return 1;
  return 0;
}
static void phaseCarsStep(float dt) {   // раз в секунду: кого из машин фазы ещё нет в памяти — читать
  if (MSN_PHASECARS109_OLD || !mvCarPrefetch || !mvCarReady) return;
  if (!mvPhaseCarsReady) mvPhaseCarsReady = phaseCarsAllReady;   // карточка Бабла ждёт машин фазы (109); служебный крючок flag mvcarsdelay109 перекрывает
  phaseCarsCollect(); if (!nPhaseCars) return;
  if ((phaseCarsT -= dt) > 0.f) return; phaseCarsT = 1.f;
  for (int i = 0; i < nPhaseCars; i++) if (!mvCarReady(phaseCars[i])) { mvCarPrefetch(phaseCars[i]); logf_("MSN phasecars read %s t=%.1f\n", phaseCars[i], clock_); }
}
// 109 (г): очередь загрузки следующего этапа пуста? Машины следующего этапа в памяти, фоновые чтения персонажей закончены. 1 — ждать (msn_run.c MSN_BETWEEN)
static int mvLoadBusy(void) {
  if (MSN_LOADWAIT_VIEW_OLD || M.rt.cur < 0 || (unsigned)M.rt.cur >= M.f.nmissions || !mvCarReady) return 0;
  const MsnMission *ms = &M.f.missions[M.rt.cur]; unsigned pi = M.rt.phase == 0 ? ms->intro : ms->main; if (pi == MSN_NONE16 || pi >= M.f.nphases) return 0;
  const MsnPhase *ph = &M.f.phases[pi]; int nx = M.rt.stage + 1; if (nx < 0 || (unsigned)nx >= ph->stage_count) return 0;
  const MsnStage *sg = &M.f.stages[ph->first_stage + nx]; int busy = 0;
  for (unsigned c = 0; c < sg->cmd_count; c++) { const MsnCmd *k = &M.f.cmds[sg->cmd_first + c];
    if (k->op == MSN_OP_ADDSTAGEVEHICLE) { const char *cn = msn_arg_s(&M.f, k, 0); if (cn && *cn && !mvCarReady(cn)) { if (mvCarPrefetch) mvCarPrefetch(cn); busy = 1; } }
    else if (k->op == MSN_OP_ADDNPC) { const char *nn = msn_arg_s(&M.f, k, 0); if (nn && *nn && prePending(nn)) busy = 1; } }
  return busy;
}
static void stageLookAhead(void) {   // персонажи (AddNPC) и машины следующего этапа той же фазы; на конце intro — первый этап main
  if (STAGE_PREFETCH_OLD || M.rt.cur < 0 || (unsigned)M.rt.cur >= M.f.nmissions) return;
  const MsnMission *ms = &M.f.missions[M.rt.cur]; unsigned pi = M.rt.phase == 0 ? ms->intro : ms->main;
  if (pi == MSN_NONE16 || pi >= M.f.nphases) return;
  const MsnPhase *ph = &M.f.phases[pi]; int nx = M.rt.stage + 1;
  logf_("MSN lookahead phase=%d stage=%d count=%u t=%.1f\n", M.rt.phase, M.rt.stage, ph->stage_count, clock_);
  for (int w = 0; w < 2 && logFn; w++) { unsigned pj = w ? ms->main : ms->intro; if (pj == MSN_NONE16 || pj >= M.f.nphases) continue;   // самоаудит 16.10: цикл «MSN plan» только для журнала (у игрока журнала нет)
    for (unsigned q = 0; q < M.f.phases[pj].stage_count; q++) { const MsnStage *sg = &M.f.stages[M.f.phases[pj].first_stage + q]; int nn = 0, nv = 0;
      for (unsigned c = 0; c < sg->cmd_count; c++) { int o = M.f.cmds[sg->cmd_first + c].op; nn += o == MSN_OP_ADDNPC; nv += o == MSN_OP_ADDSTAGEVEHICLE; }
      logf_("MSN plan %s phase=%d stage=%u obj=%d cmds=%u npc=%d veh=%d\n", msn_str(&M.f, ms->id), w, q, (int)sg->obj, sg->cmd_count, nn, nv); } }
  if (!MSN_READAHEAD_OLD && M.rt.stage >= 0 && M.rt.stage < (int)ph->stage_count) lookAheadStage(&M.f.stages[ph->first_stage + M.rt.stage]);   // №629: свой этап — его AddNPC/машины создаются после Continue брифинга, читаем под газетой
  if (nx < (int)ph->stage_count) { lookAheadStage(&M.f.stages[ph->first_stage + nx]); return; }
  if (!MSN_READAHEAD_OLD && M.rt.phase == 0 && ms->main != MSN_NONE16 && ms->main < M.f.nphases) {   // следующая фаза: первый её этап
    const MsnPhase *pm = &M.f.phases[ms->main]; if (pm->stage_count > 0) { logf_("MSN readahead main stage0 cmds=%u t=%.1f\n", M.f.stages[pm->first_stage].cmd_count, clock_); lookAheadStage(&M.f.stages[pm->first_stage]); } }
}
// 1338: строка TARGET в ящик выпуска и смоук: на старте этапа talkto/follow/goto — цель, её позиция (метка), есть ли машина цели (модель в памяти) и есть ли персонаж. Раз на этап, как только метка встала (до 3 с)
// 1337/1338 (Роб: L4 m1, этап «FOLLOW THE POLICE», на метке пусто; машина этапа не прочиталась, карточка вышла по сроку 25 с, этап пошёл без неё; каноны 16 и 17): этап с целью-машиной, чьей модели ещё нет в памяти, не идёт: логика миссии стоит
// (отсчёт, условия, ИИ машины), метка на пустом месте не показывается, пока чтение машины повторяется с паузами (лестница mcrEnsure и phaseCarsStep, файл открывается заново каждой попыткой), не дольше MCRWAIT1337_S (30 с), потом этап идёт как раньше.
// Строка TARGETWAIT1337 waited=Nms (смоук и ящик). MCRWAIT1337_OLD=1 / flag mcrwait1337old 1 — как было
#ifndef MCRWAIT1337_OLD
#define MCRWAIT1337_OLD 0
#endif
#define MCRWAIT1337_S 30.f
int mvMcrWait1337Old = MCRWAIT1337_OLD; static int tgtWait1337, tgtMark1337; static float tgtWaited1337;
static char tgtName1338[24]; static int tgtObj1338; static float tgtT1338;
// 1: логика миссии этого кадра стоит (цель-машина без модели). Зовётся раз в кадр до gameMsnTick
static int tgtCarWait1337(float dt) {
  if (!tgtWait1337) return 0;
  if (!active || M.rt.status != MSN_RUNNING || H.briefing || !tgtName1338[0]) { tgtWait1337 = 0; return 0; }
  int ready = mvCarReady ? mvCarReady(tgtName1338) : 1;
  if (!ready && tgtWaited1337 < MCRWAIT1337_S) { tgtWaited1337 += dt; if (H.markerOn) { tgtMark1337 = 1; H.markerOn = 0; } if (mvCarPrefetch) { static float pf_; if ((pf_ -= dt) <= 0.f) { pf_ = 1.f; mvCarPrefetch(tgtName1338); } } return 1; }
  tgtWait1337 = 0; if (tgtMark1337) { tgtMark1337 = 0; H.markerOn = 1; }
  { char l_[120]; snprintf(l_, sizeof l_, "stage=%d.%d name=%s waited=%dms car=%d%s", M.rt.phase, M.rt.stage, tgtName1338, (int)(tgtWaited1337 * 1000.f), ready, ready ? "" : " cap"); char s_[160]; snprintf(s_, sizeof s_, "TARGETWAIT1337 %s t=%.1f\n", l_, clock_); logf_("%s", s_); bbNoteDiag("TARGETWAIT", l_); }
  return 0;
}
static void tgtNote1338(float dt) {
  if (tgtT1338 <= 0.f || H.briefing) return;
  tgtT1338 -= dt;
  if (!H.markerOn && tgtT1338 > 0.f) return;
  tgtT1338 = 0.f;
  int car = tgtName1338[0] && mvCarReady ? (mvCarReady(tgtName1338) ? 1 : 0) : -1, npc = tgtName1338[0] ? (gameMsnNpc(&M, tgtName1338) != 0) : -1;
  char l_[160]; snprintf(l_, sizeof l_, "stage=%d.%d obj=%s name=%s pos=%.0f,%.0f,%.0f marker=%d car=%d npc=%d", M.rt.phase, M.rt.stage, msn_objective_name[tgtObj1338], tgtName1338[0] ? tgtName1338 : "-", H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, H.markerOn, car, npc);
  char s_[200]; snprintf(s_, sizeof s_, "TARGET %s t=%.1f\n", l_, clock_); logf_("%s", s_); bbNoteDiag("TARGET", l_);
}
static void eventCb(void *u, const MsnEvent *e) {
  (void)u; const GameMsn *m = &M;
  switch (e->type) {
    case MSN_EV_MISSION_START: H.present[0] = 0; logf_("MSN mission %s \"%s\" t=%.1f\n", e->name ? e->name : "", e->text ? e->text : "", clock_); msnartMissionStart(); resOrder109(e->name); break;
    case MSN_EV_STAGE_START: H.stages++;
      if (e->index == MSN_OBJ_TALKTO || e->index == MSN_OBJ_FOLLOW || e->index == MSN_OBJ_GOTO) { tgtObj1338 = e->index; snprintf(tgtName1338, sizeof tgtName1338, "%s", e->name2 ? e->name2 : ""); tgtT1338 = 3.f; } else tgtT1338 = 0.f;   // 1338: строка TARGET
      tgtWait1337 = !mvMcrWait1337Old && e->name2 && *e->name2 && mvCarReady && !mvCarReady(e->name2) && e->index != MSN_OBJ_TALKTO; tgtWaited1337 = 0.f; tgtMark1337 = 0; if (tgtWait1337) snprintf(tgtName1338, sizeof tgtName1338, "%s", e->name2);   // 1337
      if (!stayBlack && fadeA > 0.f && fadeDir >= 0 && !(rsPend && !RSPEND1304_OLD)) { fadeInStart(); }   // правка 122: из чёрного   // 1304: перезапуск ждёт чёрного — не разворачивать   // talk_target этого этапа уже пришёл (msn_game зовёт его до хвоста событий)
      logf_("MSN stage %s.%d.%d obj=%s text=\"%s\" icon=%s t=%.1f\n", m->rt.cur >= 0 ? msn_str(&m->f, m->f.missions[m->rt.cur].id) : "-",
            m->rt.phase, m->rt.stage, e->index >= 0 && e->index < MSN_OBJ_COUNT ? msn_objective_name[e->index] : "?", e->text ? e->text : "",
            e->name ? e->name : "", clock_);
      { char st[32]; snprintf(st, sizeof st, "%s.%d.%d", m->rt.cur >= 0 ? msn_str(&m->f, m->f.missions[m->rt.cur].id) : "-", m->rt.phase, m->rt.stage);
        bbOp("msn", st, 0); }   // правка 103: самописец — этап миссии
      stageLookAhead();   // разбор тура 12: модели персонажей следующего этапа — в фоновое чтение
      break;
    case MSN_EV_STAGE_COMPLETE: stayBlack = 0; H.talk[0] = 0; logf_("MSN done %d.%d t=%.1f\n", m->rt.phase, m->rt.stage, clock_);
      if (m->rt.st && m->rt.st->obj == MSN_OBJ_GOTO) {   // цель goto: эффект там, где игрок въехал в объём цели (запись PS2 10:34.7 — столб
        MsnWorld w; gameMsnWorld(m, &w); cfxAt(stageEffect(), &w.player, "goto", m->rt.stage); }   // света у машины, а не в центре локатора: объём у дома Фландерса 44 x 72 м)
      break;   // правка 122: stayBlack — StayInBlack только на свой этап
    case MSN_EV_MISSION_COMPLETE: bbNoteMsnEnd("success"); logf_("MSN complete %s t=%.1f\n", e->name ? e->name : "", clock_); voMissionEnd(0); break;
    case MSN_EV_MISSION_FAILED: bbNoteMsnEnd("fail"); voReason = e->index; voMissionEnd(1); break;   // аудит 09.10: реплика провала (экран провала — свой обработчик)
    case MSN_EV_DROP: if (SV_DROP_OLD) uiForceShow(UI_T_ITEM_DROPPED, 2.f); else { if (!dropOn) logf_("MSN dropmsg on t=%.1f\n", clock_); dropOn = 1; dropT0 = clock_; }   // таран машины с грузом: «Item Dropped!» (Scrooby HUD), 2 с — правило порта
      logf_("MSN drop %d at %.1f %.1f t=%.1f\n", e->index, e->pos.x, e->pos.z, clock_); break;
    case MSN_EV_IRIS: if (SV_EVENTS2_OLD) break;
      if (e->index) { fadeDir = 1; fadeLen = 1.f; fadeHoldNeed = 0; fadeInPend = 0; blackAt = -1.f; }
      else if (fadeA > 0.f && !(rsPend && !RSPEND1304_OLD)) { fadeDir = -1; fadeLen = 1.f; fadeInPend = 0; fadeHoldNeed = 0; blackAt = -1.f; }   // 1304: то же
      logf_("MSN iris %s t=%.1f\n", e->index ? "close" : "open", clock_); break;
    case MSN_EV_PAR_LATE: if (SV_EVENTS2_OLD) break;
      { const char *t = msn_text_h(&M.f, 0x82030fafu);   /* ключа в пакете нет — запасной текст "TOO SLOW" (раньше msn_text отдавал сам ключ) */ char b[160]; snprintf(b, sizeof b, "%s %d", t && *t ? t : "TOO SLOW", (int)e->value); uiMessage(b, -1, 1.1f); }
      logf_("MSN par-late over=%d left=%.0f t=%.1f\n", e->index, e->value, clock_); break;
    case MSN_EV_OUTOFVEHICLE: if (SV_EVENTS2_OLD) break;
      oovOn = e->index != 0; oovWin = e->value; oovT0 = clock_;
      if (!oovOn) uiHudShow(UI_T_TIMER, 0);
      logf_("MSN out-of-vehicle %d win=%.1f t=%.1f\n", oovOn, e->value, clock_); break;
    case MSN_EV_LEVEL_COMPLETE:   // последняя сюжетная миссия (AddMission) или SetLevelOver / SetGameOver — вспышка 3 с, потом main.c
      H.levelDone = e->index ? 2 : 1; H.levelDoneAt = clock_ + MV_LEVEL_DONE_S;
      if (!e->index && !levelEndOld) { H.levelEnd = 1; H.levelDoneAt = 1e30f; uiMessage(NULL, -1, 0.f); }   // уровень: экран LevelEnd.scr до X
      logf_("MSN level-complete level=%d gameover=%d t=%.1f\n", m->f.hdr ? (int)m->f.hdr->level : 0, e->index, clock_); break;
    case MSN_EV_DIALOGUE: snprintf(talkConv, sizeof talkConv, "%s", e->text ? e->text : "");   // правка 40: имя разговора (гейт камер)
      logf_("MSN dialogue %s lines=%d audio=%s t=%.1f\n", e->text ? e->text : "", e->conv ? (int)e->conv->count : 0,
                                M.audio.b ? "yes" : "no", clock_); break;
    default: break;
  }
}

// ------------------------------------------------------------------ звук миссий на PSP (msn_audio.c): голос — поток диалогов
// audio.c (дорожки music.sap), музыка — события композиции (sound.c sndMusicEvent), режим звука «Dialogue» на разговор
static int abNoVoice;   // служебный флаг смоука uinovoice: голоса «нет» (отрицательный контроль tests/missions/emu_m0.sh)
static int abTrack(void *u, const char *n) { (void)u; return sndReady() && !abNoVoice ? audioTrack(n) : -1; }
static float abSec(void *u, int t) { (void)u; return audioTrackSeconds(t); }
static void abPlay(void *u, int t) { (void)u; sndVoice(t); }
static int abPlaying(void *u, int t) { (void)u; return sndVoicePlaying(t); }
static int abMusic(void *u, const char *ev, const char **nm) { (void)u; return sndMusicEvent(ev, nm); }
static void abRelease(void *u) { (void)u; sndMusicMission(0); }
static int abSavedMode = -1;
static void abMode(void *u, int on) {
  (void)u;
  if (on) { if (abSavedMode < 0) abSavedMode = sndModeCur(); sndMode(SND_MODE_DIALOGUE); }
  else if (abSavedMode >= 0) { sndMode(abSavedMode); abSavedMode = -1; }
}
static void abLog(void *u, const char *s) { (void)u; if (logFn) logFn(s); }
static void abAlways(void *u, int on) { (void)u; sndMusicAlways(on); }   // круг 3 (g08 P09:1): этап «музыка всегда»
static void abTimeWarn(void *u) { (void)u; sndSay(SND_SAY_TIME, 0); }   // сверка g08-sound (P09:20, P06 Д4): таймер пересёк 10 с — реплика Time вместо музыки 10sec
static void abWarm(void *u) { (void)u; audioWarmMission(); }   // Звук-96
static const MsnAudioBackend audioBackend = {0, abTrack, abSec, abPlay, abPlaying, abMusic, abRelease, abMode, abLog, 0, abAlways, abTimeWarn, abWarm};

static void mvGameNote(const char *line) { logf_("%s\n", line); }   // №1073: строки game_msn.c (CARNEAR1073) в журнал
int mvInit(const char *ebootPath, int level) {
  ambElapsed = 0.f; gameMsnNote = mvGameNote;
  brBriefed = 0; memset(msTries, 0, sizeof msTries); msSkipped = 0;   // правка 52: первая карточка уровня — с загрузкой; сверка g07b: попытки и пропуски — на уровень
  strncpy(dir, ebootPath, sizeof dir - 1); char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
  char art[sizeof dir + 8]; if (level > 1) snprintf(art, sizeof art, "%sl%d/", dir, level); else snprintf(art, sizeof art, "%s", dir);   // msnart.shr: 2–7 — в l<N>/
  msnartInit(art);
  { int lv_ = level >= 1 && level <= 7 ? level : 1; msnResClose(); resMi109 = -1; resOk109 = !mvList109Old && msnResOpen(dir, lv_) == 0; logf_("MSN reslist level=%d open=%d\n", lv_, resOk109); }
  mvFreePack();   // смена уровня: пакет прошлого уровня — в кучу
  if (level < 1 || level > 7) level = 1;
  char nm[32]; if (level > 1) snprintf(nm, sizeof nm, "l%d/level%02d.msn", level, level); else snprintf(nm, sizeof nm, "level%02d.msn", level);
  if (rfData) { free(rfData); rfData = 0; rfSize = 0; }
  { char rf[32]; if (level > 1) snprintf(rf, sizeof rf, "l%d/racefence-l%d.bin", level, level); else snprintf(rf, sizeof rf, "racefence.bin");
    if (readAll(rf, &rfData, &rfSize) != 0) { rfData = 0; rfSize = 0; } }   // правка 122: заборы трасс уличных гонок (нет — гонки без заборов)
  return readAll(nm, &pack, &packSize);   // пакет уровня (tools/missions/compile.py --level N), живёт до смены уровня
}
void mvSetLog(void (*log)(const char *)) { logFn = log; }
void mvFreePack(void) { if (pack) free(pack); pack = 0; packSize = 0; }
void mvFree(void) { mvShutdown(); mvFreePack(); msnartShutdown(); mvResFree109(); }   // смена уровня: всё, что держали миссии уровня
int mvPackLevel(void) { unsigned short l; if (!pack || packSize < 8) return 0; memcpy(&l, (const char *)pack + 6, 2); return l; }   // MSN1 @6
int mvLevelEnd(void) { return active && H.levelEnd; }
int mvGiverNear(float x, float z, float r) {   // ближайший значок бонус-миссии / гонки / продавца в радиусе r (UI_R_*), -1 — нет
  int best = -1; float bd = r * r;
  for (int i = 0; active && i < H.ngiver; i++) {
    float dx = H.giver[i].x - x, dz = H.giver[i].z - z, d = dx * dx + dz * dz;
    if (d <= bd) { bd = d; best = H.giver[i].type; }
  }
  return best;
}
int mvGiverAt(int type, int k, float *xz) {   // служебное (flag bonusgo, спрайт «!»): k-й значок типа type (UI_R_*) — его x, z; 0 — нет
  for (int i = 0, n = 0; active && i < H.ngiver; i++) if (H.giver[i].type == type && n++ == k) { xz[0] = H.giver[i].x; xz[1] = H.giver[i].z; return 1; }
  return 0;
}
int mvTalkNpcAt(float *p) {   // служебное (flag bonusgo -1, спрайт «!»): собеседник текущей цели и высота его значка (talkH); 0 — нет
  if (!active || !H.talk[0] || M.rt.status != MSN_RUNNING || M.rt.obj != MSN_OBJ_TALKTO) return 0;
  const GameNpc *n = gameMsnNpc(&M, H.talk); if (!n) return 0;
  p[0] = n->pos.x; p[1] = n->pos.y; p[2] = n->pos.z; p[3] = H.talkH; return 1;
}
int mvTutorialMode(void) {
  const MsnLevelInfo *lv = active ? M.f.level : 0;
  for (uint32_t i = 0; lv && i < lv->init_count; i++) {
    const MsnCmd *c = &M.f.cmds[lv->init_first + i];
    if (c->op == MSN_OP_ENABLETUTORIALMODE && msn_argc(c) > 0) { const char *a = msn_arg_s(&M.f, c, 0);
      return (a && (!strcmp(a, "1") || !strcmp(a, "true"))) || msn_arg_i(&M.f, c, 0) == 1; }   // число 1 или строка 1 / true
  }
  return 0;
}
void mvLevelEndInput(unsigned hit) {
  if (!active || !H.levelEnd || !(hit & PSP_CTRL_CROSS)) return;
  H.levelEnd = 0; H.levelDoneAt = clock_; logf_("MSN level-end-accept t=%.1f\n", clock_);
}
void mvLevelStats(UiLevelStats *s) {
  memset(s, 0, sizeof *s);
  if (!active) return;
  for (unsigned i = 0; i < M.f.nmissions; i++) {
    unsigned fl = M.f.missions[i].flags; int k = -1, done = (M.rt.completed >> i & 1u) != 0;
    if (fl & MSN_MF_STREETRACE) k = UI_LS_RACES;
    else if (fl & MSN_MF_BONUSMISSION) k = UI_LS_BONUS;
    else if (!(fl & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION))) k = UI_LS_STORY;
    if (k >= 0) { s->total[k]++; s->done[k] += done; }
  }
  int allBonus = s->done[UI_LS_BONUS] >= s->total[UI_LS_BONUS] && s->total[UI_LS_BONUS] > 0;
  int allRaces = s->done[UI_LS_RACES] >= s->total[UI_LS_RACES] && s->total[UI_LS_RACES] > 0;
  for (int k = 0, n = msn_reward_count(&M.f); k < n; k++) {
    MsnReward r; if (!msn_reward_get(&M.f, k, &r)) continue;
    int row = r.kind == MSN_OWN_SKIN ? UI_LS_SKINS : UI_LS_CARS, own = 0;
    if (msn_streq(r.quest, "defaultcar") || msn_streq(r.quest, "defaultskin")) own = 1;
    else if (msn_streq(r.quest, "bonusmission")) own = allBonus;
    else if (msn_streq(r.quest, "streetrace")) own = allRaces;
    if (!own && M.rt.host.owns) own = M.rt.host.owns(M.rt.host.user, r.kind, r.name);
    s->total[row]++; s->done[row] += own != 0;
  }
  const MsnLevelInfo *lv = M.f.level;   // SetTotalGags (rewards.mfk уровня): всего гэгов для строки Gags
  for (uint32_t i = 0; lv && i < lv->rew_count; i++) {
    const MsnCmd *c = &M.f.cmds[lv->rew_first + i];
    if (c->op == MSN_OP_SETTOTALGAGS && msn_argc(c) > 1) s->total[UI_LS_GAGS] = msn_arg_i(&M.f, c, 1);
  }
}
int mvLevelDone(void) {   // 1 — уровень пройден, 2 — игра пройдена (после вспышки LEVEL / GAME COMPLETE), один раз
  if (!active || !H.levelDone || clock_ < H.levelDoneAt) return 0;
  int r = H.levelDone; H.levelDone = 0; return r;
}
const char *mvDynaFiles(unsigned *seq) { if (seq) *seq = active ? H.dynaSeq : 0; return active ? H.dynaFiles : ""; }   // круг 5 (P12-10): строка файлов последнего SetDynaLoadData и счётчик выполнений
const char *mvDynaRoom(void) { return active ? H.dynaRoom : ""; }   // интерьер последнего SetDynaLoadData ("" — город)
const char *mvFmvPending(void) { return active && H.fmvWait ? H.fmv : 0; }
void mvFmvDone(void) { if (!active || !H.fmvWait) return; H.fmvWait = 0; msn_notify(&M.rt, MSN_IN_FMV_DONE, NULL, 0); }
// Правка Роба 227 (03.10: «после поимки отвезли на первую миссию заново — у оригинала просто штраф»): арест в свободной езде
// (рантайм ждёт начала следующей сюжетной миссии: её вступление, этап 0) провалом не считается — только штраф (main.c).
// Провал «Hit & Run: You got busted!» (MISSION_FAILED_HIT_N_RUN) — только когда идёт сама миссия. BUSTED_FREEROAM_FAILS=1 — прежнее
#ifndef BUSTED_FREEROAM_FAILS
#define BUSTED_FREEROAM_FAILS 0
#endif
// Правка Роба 455 (10.10, голосом: «полиция во время миссии на миссию не влияет: догнала — штраф, едешь дальше; миссия не проваливается»,
// закон, В48): арест и в миссии даёт только штраф и заморозку (main_frontend.h), провала MISSION_FAILED_HIT_N_RUN нет.
// BUST_FAILS_MISSION_OLD=1 — прежнее: арест в идущей миссии проваливает её
#ifndef BUST_FAILS_MISSION_OLD
#define BUST_FAILS_MISSION_OLD 0
#endif
void mvBusted(void) {
  if (!active) return;
  if (!BUST_FAILS_MISSION_OLD) { logf_("MSN busted fine-only (mission=%d) t=%.1f\n", mvInMission(), clock_); return; }
  if (!BUSTED_FREEROAM_FAILS && !mvInMission()) { logf_("MSN busted free-roam fine-only t=%.1f\n", clock_); return; }
  msn_notify(&M.rt, MSN_IN_BUSTED, NULL, 0);
}
void mvMusicResume(void) { if (active) msnAudioSuffix(&M.audio, "_start"); }   // правка 235
// №бот миссий (окно «Меню», 24.10; flag msnbot 1, дорожка laneD ночного цикла tools/night): чего ждёт текущий этап от игрока — 1: △ (пешком: разговор / посадка),
// 2: сесть в машину (машинный этап, а Гомер пешком: после △ «в машине» или выхода). Только чтение состояния рантайма, ничего не двигает
static int svcOutCar;   // 06.11: цель в комнате, а игрок в машине (mvServiceGo) — помощник выходит из машины (△)
#ifndef MSNBOTROOM_OLD2
#define MSNBOTROOM_OLD2 0
#endif
int mvServiceBot(void) {
  if (!active || M.rt.status != MSN_RUNNING) return 0;
  if (M.rt.obj == MSN_OBJ_TALKTO) return 1;   // собеседник: пешком △ говорит, в машине △ выходит из неё (миссии m6/m7 начинаются с игроком за рулём)
  if (!MSNBOTROOM_OLD2 && (M.rt.obj == MSN_OBJ_INTERIOR || svcOutCar) && !gameOnFoot(M.g)) { svcOutCar = 0; return 1; }   // цель в комнате — из машины (△), дальше к двери
  svcOutCar = 0;
  if (!gameOnFoot(M.g)) return 0;
  if (!MSNBOTROOM_OLD2 && M.rt.obj == MSN_OBJ_GOTO && M.rt.st && (M.rt.st->flags & MSN_SF_MUSTACTION)) return 1;   // 06.11: цель «с действием» (сесть на диван и т. п.) — △ на месте
  switch (M.rt.obj) {
    case MSN_OBJ_GETIN: return 1;
    case MSN_OBJ_GOTO: case MSN_OBJ_RACE: case MSN_OBJ_FOLLOW: case MSN_OBJ_LOSETAIL: case MSN_OBJ_DELIVERY: case MSN_OBJ_DUMP: case MSN_OBJ_DESTROY: case MSN_OBJ_DESTROYBOSS: return 2;
    default: return 0;
  }
}
void mvServicePrior(int keep) {   // Будка-93 (1234, служебное flag msnprior N): все сюжетные миссии уровня, кроме N, уже пройдены, как у игрока перед последней
  if (!active) return;
  for (unsigned i = 0; i < M.f.nmissions; i++) { const MsnMission *ms = &M.f.missions[i];
    if ((ms->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT)) || (int)i == keep) continue; M.rt.completed |= 1u << i; }
  logf_("MSN prior keep=%d completed=%08x t=%.1f\n", keep, (unsigned)M.rt.completed, clock_);
}
void mvServiceFinal(void) {   // Будка-93 (1234, служебное flag msnfinal): последняя сюжетная миссия уровня с её первого этапа, остальные уже пройдены
  if (!active) return;
  int last = -1; for (unsigned i = 0; i < M.f.nmissions; i++) { const MsnMission *ms = &M.f.missions[i]; if (ms->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT)) continue; last = (int)i; }
  if (last < 0) return;
  mvServicePrior(last); mvServiceMainAt(last * 100);
}
void mvServiceWin(void) {   // служебное (flag lvwin): все сюжетные миссии уровня пройдены — конец уровня тем же событием рантайма
  if (!active) return;
  const MsnMission *last = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) { const MsnMission *ms = &M.f.missions[i];
    if (ms->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT)) continue; M.rt.completed |= 1u << i; last = ms; }
  MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e);   // текущая миссия уходит
  M.rt.status = MSN_LEVEL_DONE; M.rt.cur = -1;
  memset(&e, 0, sizeof e); e.type = MSN_EV_LEVEL_COMPLETE; e.index = last && (last->flags & MSN_MF_GAMEOVER) ? 1 : 0;
  M.rt.host.event(M.rt.host.user, &e);
}
// №860: игра пройдена (SetGameOver последней миссии L7) — все сюжетные миссии уровня пройдены, без событий (запись после финала, при любом пути к финалу)
void mvMarkStoryDone(void) {
  if (!active) return;
  for (unsigned i = 0; i < M.f.nmissions; i++) { const MsnMission *ms = &M.f.missions[i]; if (!(ms->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT))) M.rt.completed |= 1u << i; }
}
int mvGotoDiag(char *out, int n) {   // 1355 служебно (flag gotodiag): где игрок против цели goto — расстояния, внутри ли триггера, пешком ли, в комнате ли
  if (!active) return 0; MsnWorld w; gameMsnWorld(&M, &w);
  const MsnLoc *l = msn_nav_locator(&M.rt, &w); if (!l) { snprintf(out, n, "GOTODIAG noloc player=%.1f,%.1f,%.1f foot=%d room=%d", w.player.x, w.player.y, w.player.z, w.on_foot, w.interior != NULL); return 1; }
  float dx = w.player.x - l->pos[0], dy = w.player.y - l->pos[1], dz = w.player.z - l->pos[2];
  snprintf(out, n, "GOTODIAG loc=%s pos=%.1f,%.1f,%.1f ntrig=%u player=%.1f,%.1f,%.1f foot=%d room=%d d2=%.1f dy=%.1f in=%d incar=%d", msn_str(&M.f, l->name), l->pos[0], l->pos[1], l->pos[2], (unsigned)l->ntrig,
           w.player.x, w.player.y, w.player.z, w.on_foot, w.interior != NULL, hypotf(dx, dz), dy, msn_loc_contains(&M.f, l, w.player, 2.6f), msn_loc_contains_car(&M.f, l, &w, 6.5f));
  return 1; }
int mvMarkerAt(float o[3]) { if (!active || !H.markerOn) return 0; o[0] = H.marker.pos.x; o[1] = H.marker.pos.y; o[2] = H.marker.pos.z; return 1; }   // 1355 служебно (flag tgtgo): где сейчас метка цели
void mvServiceFail(void) {   // служебное (flag msnfail): провал миссии с экраном повтора (№572)
  H.failed = 1; H.failSel = 0;
  uiSetMissionOver("Timed out.", "Hint : Speed is essential! Drive a fast vehicle.");
  uiMessage(NULL, -1, 0.f);
  logf_("MSN failed 1 Timed out. t=%.1f lastshown=%d tries=%d skip=%d service=1\n", clock_, tmrLastSec1324, M.rt.cur >= 0 && M.rt.cur < 32 ? msTries[M.rt.cur] : -1, mvFailSkipOk());   // 1331: тот же формат, что у настоящего провала
}
static void restartNow(void);
void mvServiceRetry(void) { restartNow(); }
static void restartIris(int kind);
void mvServiceRetryIris(void) { restartIris(2); }   // служебное (flag msnretryiris, 1304): тот же путь, что Yes на экране провала   // служебное (flag msnretry, №1264д): тот же путь, что Restart Mission паузы / Yes на экране провала
int mvActive(void) { return active; }
// 1309 служебное (flag trflog): раз в секунду строка MCLOG по каждой машине миссии — где стоит, скорость, режим ИИ, счётчики застреваний; нет машин — пустая строка
int mvMcLogFast;
int mvMcLog(char *out, int n) {
  int k = 0; out[0] = 0; if (!active) return 0;
  MsnWorld pw_; gameMsnWorld(&M, &pw_);
  for (int i = 0; i < M.cars.n && k < n - 320; i++) { const McCar *c = &M.cars.c[i]; if (!c->on) continue;
    k += snprintf(out + k, (size_t)(n - k), "MCLOG %s pos=%.1f,%.1f v=%.1f h=%.2f thr=%.2f brk=%.2f str=%.2f hb=%.0f gap=%.1f mode=%d wp=%d stuckN=%d resets=%u replans=%u offroad=%d direct=%d nroute=%d still=%.1f top=%.1f base=%.1f vdes=%.1f turning=%d slip=%.1f dp=%.1f\n", c->name, c->car.x, c->car.z, hypotf(c->car.vel[0], c->car.vel[2]), c->car.h, c->lastIn.throttle, c->lastIn.brake, c->lastIn.steer, c->lastIn.handbrake, c->dbgGap, c->mode, c->wp, c->stuckN, c->resets, c->replans, c->offroad, c->direct, c->nroute, c->still, c->car.p.topSpeed, c->baseTop, c->vdes, c->turning, c->slipT, hypotf(pw_.player.x - c->car.x, pw_.player.z - c->car.z)); }
  return k;
}
void mvShutdown(void) {   // выход в главное меню: всё, что миссии заняли в куче, освобождается (куча меню — как до игры)
  if (active) bbNoteMsnEnd("shutdown");
  if (active) { MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e); }   // прошлый круг: убрать всё
  if (active) msnAudioStopVoice(&M.audio);   // и реплику завершения, если ещё звучит
  abSavedMode = -1;
  for (int i = 0; i < NANIM; i++) if (anim[i]) { free(anim[i]); anim[i] = 0; }
  for (int k = 0; k < NMODEL; k++) if (model[k].m) { skinFree(model[k].m); model[k].m = 0; model[k].name[0] = 0; }
  for (int q = 0; q < NPRE; q++) if (pre[q].p) { skinPreCancel(pre[q].p); pre[q].p = 0; pre[q].name[0] = 0; }   // фоновые чтения моделей — отменить
  nNpcPend = 0;
  msnartReset(); uiMessage(NULL, -1, 0.f); iconNote("shutdown"); uiPauseMenuHide(0); raceOff(); cfxReset();
  H.levelDone = 0; H.fmvWait = 0; H.levelEnd = 0;
  if (rsPend) logf_("MSN rspend-drop kind=%d why=close t=%.1f\n", rsPend, clock_);
  rsPend = 0; rsPendT = 0.f; fadeStuckT = fadeBlackT = 0.f;   // 1316
  active = 0;
}
int mvNewGame(Game *g) { return mvNewGameAt(g, NULL); }
// сохранения (viewer/savemenu.c): прогресс для записи, «свободная езда» (SAVE GAME есть только в PauseSunday), служебная отметка
int mvProgress(MsnProgress *p) { if (!active) return 0; msn_get_progress(&M.rt, p); return 1; }
int mvFreeRoam(void) { return !active || msn_free_roam(&M.rt); }
// Правка Роба 330 (04.10: «в нескольких случаях не могу сохранить — раздел меню серый»): у оригинала пауза свободной езды
// (PauseSunday, с SAVE GAME) — на всей «воскресной прогулке» сюжетной миссии (фаза 0: «доедь до дома», «купи Plow King»…), а
// пауза миссии без сохранения (PauseMission) — только в самой миссии (фаза 1). У порта сохранение было только на этапе 0 прогулки.
// Запись хранит номер сюжетной миссии и прогресс — после загрузки прогулка начинается сначала (правило порта). flag saveroamold 1 — прежнее
int mvSaveRoamOld;
void mvSaveWhy(char *o, int n) { snprintf(o, n, "cur=%d story=%d phase=%d status=%d br=%d talk=%d free=%d", M.rt.cur, M.rt.story, M.rt.phase, M.rt.status, H.briefing, M.talk.on, mvFreeRoam()); }
int mvCanSave(void) {
  if (mvFreeRoam()) return 1;
  if (mvSaveRoamOld) return 0;
  return M.rt.cur >= 0 && M.rt.cur == M.rt.story && M.rt.phase == 0 && M.rt.status == MSN_RUNNING && !H.briefing && !M.talk.on;
}
int mvResetBlocked(void) { return active && gameMsnResetBlocked(&M); }
// 1348 (Роб/дирижёр: пауза во время миссии одна, по оригиналу): в 6-секундном окне между этапами (MSN_BETWEEN: этап закончен, следующий ещё грузится) пауза открывалась свободной
// (MISSION SELECT, LEVEL PROGRESS), потому что mvInMission() в этом окне ложно. Пауза миссии — RESTART / CANCEL MISSION и окно задания (1347). Сам mvInMission не менялся: от него зависят штраф ареста, точка возврата, гэги.
// PAUSEBETWEEN1348_OLD=1 / «flag pausebetween1348old 1» — прежнее
#ifndef PAUSEBETWEEN1348_OLD
#define PAUSEBETWEEN1348_OLD 0
#endif
int mvPauseBetween1348Old = PAUSEBETWEEN1348_OLD;
#ifndef INPUTHINTS1357_OLD
#define INPUTHINTS1357_OLD 0
#endif
int inputHints1357Old = INPUTHINTS1357_OLD; static unsigned ldUs_;   // 1357 (см. main_loop.h, mvBriefingInput)
int mvInMission(void) { return active && M.rt.cur >= 0 && (M.rt.status == MSN_RUNNING || M.rt.status == MSN_COUNTDOWN) && M.rt.phase == 1 && !msn_free_roam(&M.rt); }
int mvInMissionPause(void) { return mvInMission() || (!mvPauseBetween1348Old && active && M.rt.cur >= 0 && M.rt.status == MSN_BETWEEN && M.rt.phase == 1 && !msn_free_roam(&M.rt)); }
#ifndef SV_RESTART_OLD
#define SV_RESTART_OLD 0   // сверка g07b (P13-2, P06-6): 1 — Restart паузы с первого этапа основной части и стирает контрольную точку (прежнее)
#endif
// сверка g07b-msnview (M2 Д4, P13-2): перезапуск из паузы и Yes на экране провала идут цепочкой — реплика обрывается, ирис закрывается (0,25 с), миссия перезапускается, ирис открывается (0,25 с);
// отмена миссии обрывает реплику и сразу, без ириса. Было: мгновенно, реплика договаривала. SV_RESTART_IRIS_OLD=1 — прежнее
#ifndef SV_RESTART_IRIS_OLD
#define SV_RESTART_IRIS_OLD 0
#endif
static void restartNow(void) {
  if (!active || M.rt.cur < 0) return;
  if (!SV_RESTART_OLD && M.f.missions[M.rt.cur].main != MSN_NONE16) {   // сверка g07b P13-2/P06-6: Restart идёт тем же путём, что Retry — на этап ResetToThisStage (контрольную точку), точка не стирается
    msn_retry(&M.rt, 0);
  } else if (M.f.missions[M.rt.cur].main != MSN_NONE16) {   // прежнее (SV_RESTART_OLD=1): основная часть с первого этапа, точка перезаписывается на этап 0
    M.rt.cp_phase = 1; M.rt.cp_stage = 0; M.rt.cp_timer = 0; M.rt.cp_remaining = 0; msn_retry(&M.rt, 0);
  } else msn_retry(&M.rt, 1);
  bbNoteMsnEnd("pause-restart");
  logf_("MSN pause-restart %s t=%.1f\n", msn_str(&M.f, M.f.missions[M.rt.cur].id), clock_);
}
static void restartRun(int kind) { if (kind == 1) restartNow(); else if (active && M.rt.cur >= 0) msn_retry(&M.rt, 0); }
static void restartIris(int kind) {   // kind 1 — Restart паузы, 2 — Yes на экране провала
  if (SV_RESTART_IRIS_OLD) { restartRun(kind); return; }
  msnAudioStopVoice(&M.audio);
  fadeDir = 1; fadeLen = SV_FADE_OUT; fadeHoldNeed = 0; fadeInPend = 0; blackAt = -1.f; rsPend = kind; rsPendMsn = M.rt.cur; rsPendT = 0.f;
}
void mvPauseRestart(void) {
  if (!active || M.rt.cur < 0) return;
  H.failed = 0; uiHudShow(UI_T_MISSION_FAILED, 0);
  if (mvSkipCount1331Old) msTryBump();   // сверка g07b P13-3: перезапуск = один учтённый сброс (1331: теперь считает карточка)
  restartIris(1);
}
void mvPauseAbort(void) {
  if (!active || M.rt.cur < 0) return;
  if (!SV_RESTART_IRIS_OLD) msnAudioStopVoice(&M.audio);   // сверка g07b M2 Д4: отмена обрывает реплику
  bbNoteMsnEnd("pause-abort");
  logf_("MSN pause-abort %s t=%.1f\n", msn_str(&M.f, M.f.missions[M.rt.cur].id), clock_);
  gameMsnAbort(&M);
}
void mvServiceMark(int mission) { if (active && mission >= 0 && mission < 32) M.rt.completed |= 1u << mission; }
int (*mvFlatGlow)(const char *, const float *, float, float, float);   // №793 (lightsprite.h vecGlowHook)
void (*mvPump)(void);   // правка 244: кадр экрана загрузки из долгих шагов (viewer/main.c loadPump; вне загрузки ничего не делает)
int mvNewGameAt(Game *g, const MsnProgress *resume) {
  if (!pack) return -1;
  mvShutdown();
  memset(&H, 0, sizeof H); clock_ = 0.f;   // и «уровень пройден», и ролик цели fmv
  MsnGameApi v; memset(&v, 0, sizeof v);
  v.ui.objective = uiObjectiveCb; v.ui.timer = uiTimerCb; v.ui.par_time = uiParCb; v.ui.counter = uiCounterCb; v.ui.lap = uiLapCb;
  v.ui.countdown = uiCountdownCb; v.ui.flash = uiFlashCb; v.ui.failed = uiFailedCb;
  v.ui.presentation = presentationCb; v.ui.mission_title = titleCb;
  v.nav.marker = navMarkerCb; v.nav.giver_icon = giverCb;
  v.npc.spawn = npcSpawnCb; v.npc.talk_target = talkCb;
  v.feature = featureCb; v.cleanup = cleanupCb; v.on_event = eventCb; v.fx.fmv = fmvCb;
  v.props.race_props = racePropsCb;   // правка 122: пропы и заборы уличной гонки
  v.props.state_prop = statePropCb;   // №314: AddCollectibleStateProp (бочки L7)
  v.props.item_collected = itemCollectedCb;   // 04.10: эффект SetCollectibleEffect собранного предмета
  v.shop.unlock = unlockCb;           // правка 122: окно «Congratulations! You've unlocked …»
  v.fx.fade = fadeCb; v.fx.stay_black = stayBlackCb; fadeA = 0.f; fadeDir = 0; stayBlack = 0;   // правка 122: затемнения
  mboxAt = -1.f;
  char err[160];
  unsigned t0 = sceKernelGetSystemTimeLow();
  gameMsnSetAudio(&audioBackend);   // голос разговоров и музыка миссий (нет audio/ — «нет голоса», разговор не зависает)
  active = gameMsnOpenAt(&M, g, pack, packSize, &v, resume, err, sizeof err) == 0; seenTeleports = M.teleports;
  { char ta[32]; int lv = mvPackLevel(); snprintf(ta, sizeof ta, "art/l%d_terra.p3d", lv >= 1 && lv <= 7 ? lv : 1); msnartLoad(ta, 1); }   // правка 122: стрелки на дороге
  logf_("MSNSTART ok=%d err=%s ms=%.0f npc=%d\n", active, active ? "" : err, (sceKernelGetSystemTimeLow() - t0) / 1000.f, M.nnpc);
  return active ? 0 : -1;
}
int mvFrozen(void) { return active && gameMsnFrozen(&M); }
int mvKick(float x, float y, float z, float r, float dx, float dz) { return active ? gameMsnKick(&M, x, y, z, r, dx, dz) : 0; }   // №843
void mvFreezeCar(CarInput *in) { if (active) gameMsnFreezeCar(&M, in); }
int mvWantsAction(void) { return active && !H.failed && gameMsnWantsAction(&M); }
void mvSetInterior(const char *name) { M.interior = name; }
static const char *(*npcRoomAt)(float x, float y, float z);
void mvSetRoomAt(const char *(*roomAt)(float x, float y, float z)) { npcRoomAt = roomAt; }
static int (*npcRoomGo)(const float p[3], float out[3], int interior); static int svcDoor;
void mvSetRoomGo(int (*roomGo)(const float p[3], float out[3], int interior)) { npcRoomGo = roomGo; }
int mvSvcDoorWant(void) { int r = svcDoor; svcDoor = 0; return r; }
int mvMsnSetup(void) { return active && M.inSetup; }   // №1072: окно подготовки фазы открыто — перенос игрока ещё может прийти
int mvTeleported(float out[3]) {   // сценарий переставил игрока с прошлого вызова: где он теперь
  if (!active || M.teleports == seenTeleports) return 0;
  seenTeleports = M.teleports;
  MsnWorld w; gameMsnWorld(&M, &w); out[0] = w.player.x; out[1] = w.player.y; out[2] = w.player.z;
  return 1;
}
int mvNpcPosition(const char *name,float point[3]) {
  if(!active||!name)return 0;
  const GameNpc *n=gameMsnNpc(&M,name);if(!n)return 0;
  point[0]=n->pos.x;point[1]=n->pos.y;point[2]=n->pos.z;return 1;
}
const MsnCars *mvCars(void) { return active ? &M.cars : 0; }
const char *mvForcedCar(void) { return active ? gameMsnForcedCar(&M) : 0; }
void mvWorldCar(const char *car, const float p[3], float h) { if (active) gameMsnWorldCar(&M, car, p, h); }   // правка 53
int mvWorldCarNear(float x, float z, float r, float out[4]) { return active ? gameMsnWorldCarNear(&M, x, z, r, out) : -1; }
int mvTakeWorldCar(int i) { return active ? gameMsnTakeWorld(&M, i) : 0; }
int mvHnrOff(void) { return active && gameMsnHnrOff(); }   // перепроверка 96
int mvPhoneCar(const char *car) { return active ? gameMsnPhoneCar(&M, car) : 0; }   // правка 94
int mvSummon(void) { return active ? gameMsnSummon(&M) : 1; }
int mvHudHidden(void) { return active && gameMsnHudHidden(&M); }   // сверка g07 круг 6 (P06-21)
int mvBoothBlocked(void) { return active && gameMsnBoothBlocked(&M); }   // сверка g07 круг 5 (P05-25)
int mvBoothsOff(void) { return active && gameMsnBoothsOff(&M); }   // ps2-twin P05:25
void mvSetState(int transition, int arrested, const char *entering) { if (active) gameMsnSetState(&M, transition, arrested, entering); }   // сверка g10b-loop круг 2
void mvWorldCarsStep(float px, float pz) { if (active) gameMsnWorldCarsStep(&M, px, pz); }   // сверка g10b-loop круг 5
int mvShopAllowed(void) { return active ? gameMsnShopAllowed(&M) : 1; }   // сверка g10b-loop круг 2
void mvPolice(int on) { if (active) gameMsnHnrPolice(&M,on); }
void mvPoliceAdd(void) { if (active) gameMsnHnrPoliceAdd(&M); }
int mvPoliceLimit(void) { return active ? gameMsnHnrPoliceLimit(&M) : 1; }
void mvPoliceFar(float px, float pz, float R) { if (active) gameMsnHnrPoliceFar(&M, px, pz, R); }
int mvPoliceSuspend(void) { return active ? gameMsnHnrPoliceSuspend(&M) : 0; }   // круг 7 (P03-07)
void mvPoliceResume(void) { if (active) gameMsnHnrPoliceResume(&M); }
void mvPoliceLimbo(int on) { if (active) gameMsnHnrPoliceLimbo(&M, on); }   // правка 433
void mvPoliceDrop(int i) { if (active) gameMsnHnrPoliceDrop(&M, i); }   // круг 7 (P03 Д2)
void mvPoliceHold(void) { if (active) gameMsnHnrPoliceHold(&M); }
float mvHnrDecay(void) { return active ? M.hnr.decay : 3.f; }
// 1356 (Роб: после follow у Lard Lad «штраф, штраф, штраф, не могу выйти»): погоня Hit & Run считалась по имени модели — стоящая рядом машина миссии cPolice (цель этапа 1.1.2 L4 m1) шла как полицейская в расчёте ареста.
// Теперь полиция H&R — по флагу police (mvIsHnrCar); hnrPolName1350Old = 1 (HNRPOLNAME1350_OLD / файл hnrpolname1350old) — по имени, как было
#ifndef HNRBUSTNAME1356_OLD
#define HNRBUSTNAME1356_OLD 0
#endif
int hnrBustName1356Old = HNRBUSTNAME1356_OLD;   // 1356: 1 — арест и погоня считают полицией любую машину с именем модели cPolice, и машину миссии тоже (как было); файл hnrbustname1356old, flag hnrbustname1356old 1
int mvIsHnrCar(const McCar *c) { return hnrPolName1350Old || hnrBustName1356Old ? mvIsPolice(c->name) : c->police != 0; }
void mvPoliceAt1359(float d) { if (active) gameMsnPoliceAt1359(&M, d); }
void mvNpcList1360(void) { if (active) gameMsnNpcList1360(&M, clock_); }
int mvIsPolice(const char *name) { return !strcasecmp(name,active ? M.hnr.car : "cPolice"); }
void mvCarjack(const char *model, const CarParams *P, float lift, float x, float y, float z, float h) { if (active) gameMsnCarjack(&M, model, P, lift, x, y, z, h); }
static float carLogT;   // служебное: строка MCAR раз в секунду, пока у миссии есть машины (гейт tests/aicar/emu.sh)
// каждый кадр: завершить фоновое чтение моделей персонажей миссии, ждавших очереди (STAGE_PREFETCH_OLD=1 — пусто)
static unsigned short npcTry[8];
static void mvPrefetchPoll(void) {
  if (STAGE_PREFETCH_OLD) return;
  for (int q = 0; q < NPRE; q++) if (pre[q].p && sceKernelGetSystemTimeLow() - pre[q].t0 > 60000000u && skinPreReady(pre[q].p)) {   // прочитано впрок и 60 с не понадобилось — отдать память
    logf_("MSNNPC lookahead-expire %s\n", pre[q].name); skinPreCancel(pre[q].p); pre[q].p = 0; pre[q].name[0] = 0; }
  for (int i = 0; i < nNpcPend; ) {
    preFail = 0; NpcAnim *a = animFor(npcPend[i], 1, 0);
    if (a || preFail || ++npcTry[i] > 1800) {
      logf_("MSN npc %s %s\n", npcPend[i], a ? "model=yes pending-done" : "model=no pending-give-up");
      memmove(npcPend[i], npcPend[nNpcPend - 1], 24); npcTry[i] = npcTry[nNpcPend - 1]; nNpcPend--; npcTry[nNpcPend] = 0;
    } else i++;
  }
}
const char *mvNpcPendName1361(void) { return nNpcPend ? npcPend[0] : "-"; }   // №1361 служебно: кто ещё читается (строка NPCWAIT1076)
int mvNpcPending(void) { mvPrefetchPoll(); return nNpcPend; }   // №1076: персонажей миссии, чья модель ещё читается (опрос заодно завершает готовые)
void mvTick(int action, float dt) {
  mvPrefetchPoll();   // разбор тура 12: фоновое чтение моделей персонажей этапа
  if (!active) return;
  { float wdt = FADEGUARD1316_OLD || mvWallDt1316 <= 0.f ? dt : mvWallDt1316; fclk_ += wdt; clock_ += dt; if (wbForceEnd >= 0.f) { if (fclk_ < wbForceEnd) { fadeA = 1.f; fadeDir = 0; } else { wbForceEnd = -1.f; fadeInStart(); } } fadeTick(wdt); }   // 1316: затемнение — настенными секундами
  msnartPwrPoll107(); msnartChainUs1359 = worldHidden1310() ? 24000u : 8000u; msnartPump();   // записи msnart.shr, заказанные LoadP3DFile, дочитываются фоном
  { unsigned n_, p_, us_; static int lg1359; if (!lg1359 && msnartChainResult1359(&n_, &p_, &us_)) { lg1359 = 1; logf_("MSNARTCHAIN1359 n=%u pumps=%u ms=%.1f\n", n_, p_, us_ / 1000.f); } }   // 1359: гейт
  int wagerLog = 0;
  if (wagerPayPend) {
    if (M.rt.cur != wagerPayPend - 1 || M.rt.status == MSN_IDLE || M.rt.status == MSN_LEVEL_DONE) wagerPayPend = 0;   // миссию отменили или сменили — платёж не нужен
    else if (M.rt.status == MSN_RUNNING && M.rt.obj == MSN_OBJ_COINS) { wagerPayPend = 0; action = 1; wagerLog = 1; }
  }   // Данные-93: ✕ на ставочной карточке = платёж взноса (см. wagerPayPend)
  GamePad p; memset(&p, 0, sizeof p); if (action) p.hit = GP_TRIANGLE;
  static float tickMax;   // служебное: самый долгий кадр логики миссий (с машинами миссии) за секунду — в строке MCAR
  if (MSN_LOADWAIT_VIEW_OLD) M.rt.load_busy = 0; else { M.rt.load_busy = M.rt.status == MSN_BETWEEN && mvLoadBusy(); if (M.rt.load_busy && M.rt.load_wait <= 0.f) logf_("MSN loadwait stage=%d.%d t=%.1f\n", M.rt.phase, M.rt.stage + 1, clock_); }   // 109 (г)
  M.rt.scene_busy = !msnTimerHold1324Old && !M.rt.hold_done && (M.rt.status == MSN_COUNTDOWN || (M.rt.status == MSN_RUNNING && M.rt.timer_on)) ? sceneBusy1324() : 0;   // 1324: мир под игроком не готов — отсчёт и таймер этапа ждут
  phaseCarsStep(dt);   // 109 (в)
  { unsigned tq = sceKernelGetSystemTimeLow(); gameMsnTick(&M, &p, tgtCarWait1337(dt) ? 0.f : dt); tgtNote1338(dt); if (M.rt.hold_log >= 0) { char l_[96]; snprintf(l_, sizeof l_, "TIMERHOLD1324 waited=%dms%s t=%.1f\n", M.rt.hold_log % 1000000, M.rt.hold_log >= 1000000 ? " cap" : "", clock_); logf_("%s", l_); bbOp("timerhold", l_, 0); M.rt.hold_log = -1; } float ms = (sceKernelGetSystemTimeLow() - tq) / 1000.f; if (ms > tickMax) tickMax = ms; }
  if (wagerLog) logf_("MSN wager-fee coins=%d stage=%d.%d t=%.1f\n", shopCoins(&M.shop), M.rt.phase, M.rt.stage, clock_);   // кошелёк и этап сразу после платежа (гейт tests/data93/wager_check.py)
  { static char fcLast[16]; const char *fc = gameMsnForcedCar(&M);   // SetForcedCar: смена машины игрока — строка журнала
    if (strcmp(fc ? fc : "", fcLast)) { snprintf(fcLast, sizeof fcLast, "%s", fc ? fc : ""); logf_("MSN forced-car %s t=%.1f\n", fc ? fc : "-", clock_); } }
  if (logFn && M.cars.n && (carLogT += dt) >= 1.f) {
    carLogT = 0.f; int racers = 0, pos = gameMsnRacePos(&M, &racers);
    char buf[1200]; int n = 0;   // все машины — одной записью в журнал (запись на карту памяти — одна за секунду, не по машине)
    for (int i = 0; i < M.cars.n && n < (int)sizeof buf - 200; i++) { const McCar *c = &M.cars.c[i]; if (!c->on && !c->name[0]) continue;
      n += snprintf(buf + n, sizeof buf - n, "MCAR t=%.1f name=%s on=%d leaveQ=%d mode=%s x=%.1f z=%.1f v=%.1f wp=%d/%d fin=%d dmg=%.2f lane=%.1f geo=%d con=%d pos=%d/%d tickms=%.1f replans=%u\n", clock_, c->name, c->on, c->leaveQ, mcModeName[c->mode],
                    c->car.x, c->car.z, c->car.v, c->wp, M.cars.nwp * M.cars.laps, c->finished, c->car.damage, c->offLane, c->geoOk, c->conOk, pos, racers, tickMax, c->replans); }
    if (n) logFn(buf);
    tickMax = 0.f;
  }
}

// ------------------------------------------------------------------ 3D
int mvDollarOld;   // правка 329: 1 — без значка доллара над продавцами (прежнее, красный контроль)
// Low city terrain (L3/L6) is still outdoors. Interior membership comes from
// SHI1 floors, with an explicit script room taking precedence for NPCs.
// 108 правка Роба 1314 (L2 m2, этап «TALK TO MOE»: Мо в таверне нет, посетители сидят): AddNPC "moe" ставит персонажа с тегом интерьера из скрипта ("moe1"), а комната таверны и интерьер миссии
// называются «Moes» — явное имя не совпадало, персонаж скрыт весь этап (NPCVIS hide moe interior="moe1" mission_interior="Moes" room_at="Moes"). Теперь, если тег не совпал, персонаж виден, когда комната,
// в которой он физически стоит (npcRoomAt), — интерьер миссии. NPCROOM1314_OLD=1 / файл npcroom1314old / flag npcroom1314 0 — прежнее
#ifndef NPCROOM1314_OLD
#define NPCROOM1314_OLD 0
#endif
int mvNpcRoom1314 = !NPCROOM1314_OLD;
static int pointVisible(float x, float y, float z, const char *explicitRoom) {
  const char *room = explicitRoom && *explicitRoom ? explicitRoom : npcRoomAt ? npcRoomAt(x, y, z) : NULL;
  int v = room ? M.interior && msn_streq(room, M.interior) : !M.interior;
  if (!v && mvNpcRoom1314 && explicitRoom && *explicitRoom && npcRoomAt && M.interior) { const char *r2 = npcRoomAt(x, y, z); if (r2 && msn_streq(r2, M.interior)) v = 1; }
  return v;
}
#ifndef NPC_FAR_POSE_OLD
#define NPC_FAR_POSE_OLD 0
#endif
static int npcVisible(const GameNpc *n, float ex, float ey, float ez, float far) {
  float dx = n->pos.x - ex, dy = n->pos.y - ey, dz = n->pos.z - ez;
  if (dx * dx + dy * dy + dz * dz > far * far) return 0;
  if (!NPCSCENE431_OLD && mvSceneOk && n->pos.y > -10.f && !n->interior[0]) {
    int ok = mvSceneOkY ? mvSceneOkY(n->pos.x, n->pos.y, n->pos.z) : mvSceneOk(n->pos.x, n->pos.z);
    long ix = n - M.npc; static unsigned char held[GAME_NPC_MAX];
    if (ix >= 0 && ix < GAME_NPC_MAX && held[ix] == ok) { held[ix] = !ok; mvNpcSceneHeld += ok ? -1 : 1; logf_("NPCSCENE431 %s %s at=%.1f,%.1f held=%d\n", ok ? "show" : "hide", n->name, n->pos.x, n->pos.z, mvNpcSceneHeld); }
    if (!ok) return 0;
  }
  if (clipGuardBody(ex, ey, ez, n->pos.x, n->pos.y, n->pos.y + 1.9f, n->pos.z, .35f, CAM_HIDE_NPC)) return 0;   // №502: персонаж у самого глаза не рисуется
  { int pv_ = pointVisible(n->pos.x, n->pos.y, n->pos.z, n->interior);
    static float t_; if (!pv_ && clock_ - t_ > 2.f) { t_ = clock_; logf_("NPCVIS hide %s at=%.1f,%.1f,%.1f interior=\"%s\" mission_interior=\"%s\" room_at=\"%s\" t=%.1f\n", n->name, n->pos.x, n->pos.y, n->pos.z, n->interior, M.interior ? M.interior : "-", npcRoomAt ? (npcRoomAt(n->pos.x, n->pos.y, n->pos.z) ? npcRoomAt(n->pos.x, n->pos.y, n->pos.z) : "-") : "?", clock_); }   // 108 1314: диагностика, почему персонаж не рисуется
    return pv_; }
}
// Разговор (правки Роба 26, 38): жест реплики из данных этапа (AddAmbientNpcAnimation / AddAmbientPcAnimation — клип
// «<код>_dialogue_...» персонажа, один раз с начала реплики, потом стоит), рот — у того, кто говорит (реплика по файлу
// голоса: c_*_<код>_l1.rsd). В клипах dialogue_* челюсть не движется (замер: 0° у всех), своего хода рта в данных нет —
// ПРАВИЛО ПОРТА: челюсть открывается на TALK_JAW_MAX с частотой слогов TALK_JAW_HZ (две частоты — не метроном), сверить с PS2
#define TALK_JAW_MAX 0.22f   // рад (~12.6°): рот открыт, зубы/нёбо видны на крупном плане, не «зевок»
#define TALK_JAW_HZ  3.7f
static float talkClock;
static float talkJaw(float t) {
  float a = .5f - .5f * cosf(2.f * 3.14159265f * TALK_JAW_HZ * t), b = .5f + .5f * sinf(2.f * 3.14159265f * 1.3f * t);
  return TALK_JAW_MAX * a * (.55f + .45f * b);
}
// сверка g07b-msnview круг 2 (g04-homer, P15-02): рот говорящего — skinMouthStep (угол с постоянной скоростью между −18° и верхней границей персонажа, разворот со случайным сдвигом границ)
// вместо синуса. (сверка g04-homer круг 6: у игрока имя героя уровня — homerCharName, не всегда "homer") Знак «открыто» по оригиналу не подтверждён записью PS2 (вопрос 1 отчёта P15) — угол нормируется в 0…TALK_JAW_MAX (вся полоса хода, темп и случайность — из числа оригинала). SKIN_MOUTH_OLD=1 — синус
static SkinMouth homerMouth; static const char *homerMouthFor; static float talkDt = 1.f / 60.f;   // сверка g04-homer круг 6: homerMouthFor — для какого героя рот уже заведён (герой сменился на другом уровне — строка таблицы берётся заново)
static float mouthJaw(SkinMouth *mo, const char *model, float dt) {
  float a = skinMouthStep(mo, model, dt), lo = -18.f * 0.0174533f, span = mo->hi0 - lo; if (span < .01f) return 0.f;
  float u = (a - lo) / span; if (u < 0.f) u = 0.f; if (u > 1.f) u = 1.f;
  return TALK_JAW_MAX * u;
}
static int homerLine = -1;
static int tlLine = -1, tlPc; static float tlJawH, tlJawN;   // служебное: итог реплики — рот (TALKLINE)
static void talkHomer(int line, int pcSpeaks) {   // Гомер: жест своей реплики и рот (viewer/homer.c homerTalk / homerJaw)
  Game *g = M.g; if (!g || !*g->pHaveHomer) return;
  Homer *Hm = g->pHomer;
  if (line >= 0) {
    if (line != homerLine) { homerLine = line; homerTalk(Hm, gameMsnTalkAnim(&M, 0, line)); }
    if (homerMouthFor != homerCharName()) { homerMouthFor = homerCharName(); memset(&homerMouth, 0, sizeof homerMouth); }   // сверка g04-homer круг 6
    homerJaw(Hm, pcSpeaks ? (SKIN_MOUTH_OLD ? talkJaw(talkClock) : mouthJaw(&homerMouth, homerCharName(), talkDt)) : 0.f);   // сверка g07b круг 2 (P15-02)
    if (Hm->jawOpen > tlJawH) tlJawH = Hm->jawOpen;
  } else if (homerLine >= 0) { homerLine = -1; homerTalk(Hm, NULL); homerJaw(Hm, 0.f); }
}
// Аудит 09.10 п. 20 (В61): водитель машины миссии. Диск: 5-й аргумент AddStageVehicle (скрипт миссии; c->driver) — персонаж за
// рулём, у каждого рига поза водителя in_car_idle_driver = ndr_in_car_idle (art/chars/*.cho «borrowed from ndr»; клип кладёт в пакет
// персонажа tools/char_export.py npc_script_drivers). Место — как у Гомера за рулём (homer.c anchorFrame): якорь посадки seatAx/seatAz
// из блока машины пакета msncars (psp_export.car_info), корень позы в кадре якоря — (−x, y, z) корня in_car_idle, как seatRoot
// Гомера (клип посадки −0,995 при +0,995 у in_car_idle). ПРАВИЛА ПОРТА: машина без ИИ (NULL), пока персонаж того же имени стоит
// в миссии пешком (m1 Скиннер до ActivateVehicle), — без водителя; остов (разбита) — без водителя; дальше 80 м — как персонажи миссий.
// MV_DRIVERS_OLD=1 — прежнее (водителей нет; красный tests/missions/drivers.sh)
#ifndef MV_DRIVERS_OLD
#define MV_DRIVERS_OLD 0
#endif
// №666: высота посадки водителя машины миссии — как у героя (homer.c seatLift): сиденье машины (seat[1] блока машины + lift) минус сиденье famil_v (0,499),
// иначе в высокой машине (грузовик, лимузин) водитель сидит на 0,3–0,6 м ниже. MV_DRIVER_SEAT_OLD=1 — прежнее (на земле + корень клипа)
#ifndef MV_DRIVER_SEAT_OLD
#define MV_DRIVER_SEAT_OLD 0
#endif
static unsigned char drvShown[64];   // №666: у машины миссии i водителя персонажа рисовали (запечённого в кузов водителя модели тогда не рисуем — иначе двойник)
int mvDriverShown(int i) { return i >= 0 && i < 64 && drvShown[i]; }
static int driversDrawn, driversDrawnLast = -1;
int mvDriversDrawn(void) { return driversDrawn; }
static void drawDrivers(float ex, float ey, float ez, float dt) {
  driversDrawn = 0;
  for (int i = 0; i < 64; i++) if (i >= M.cars.n || !M.cars.c[i].on) drvShown[i] = 0;   // №666: машины нет — признак сброшен
  if (MV_DRIVERS_OLD || M.interior) return;
  for (int i = 0; i < M.cars.n; i++) {
    const McCar *c = &M.cars.c[i]; const Car *k = &c->car;
    if (!c->on || !c->driver[0] || msn_streq(c->driver, "none") || k->destroyed) continue;
    if (c->mode == MC_PARKED && gameMsnNpc(&M, c->driver)) continue;   // машина ещё стоит (NULL), а персонаж пешком в миссии
    const McGeo *G = mcGeoFind(c->name);
    if (!G || !G->hasInfo) { static char lastG[16]; if (strcmp(lastG, c->name)) { logf_("MSN driver-skip %s nogeo%d t=%.1f\n", c->name, G != 0, clock_); snprintf(lastG, sizeof lastG, "%s", c->name); } continue; }   // модель трафика без блока машины — сиденья нет
    float dx = k->x - ex, dy = k->y - ey, dz = k->z - ez; if (dx * dx + dy * dy + dz * dz > 80.f * 80.f) continue;
    char key[24]; snprintf(key, sizeof key, "%s#d", c->driver);
    NpcAnim *a = animFor(key, 1, 0);
    int clip = a ? skinFindClip(a->m, "ndr_in_car_idle") : -1;   // -1: нет слота/модели или старый пакет без позы водителя
    if (clip < 0) { static char last[24]; if (strcmp(last, key)) { logf_("MSN driver-skip %s %s t=%.1f\n", key, a ? "noclip" : "noanim", clock_); snprintf(last, sizeof last, "%s", key); } continue; }
    if (!NPC_VIS_OLD && mvVis) { float bb[6] = {k->x - 3.f, k->y - 1.f, k->z - 3.f, k->x + 3.f, k->y + 3.f, k->z + 3.f}; if (!mvVis(bb)) continue; }   // правка 500: водитель вне кадра
    skinPlay(&a->p, clip, 1, 0.f);
    float f[27]; memcpy(f, G->info + 16, sizeof f);                   // psp_export.CAR_INFO_FMT: seatAx = f[15], seatAz = f[16]
    float lift = carBoomed(k) ? c->lift : carModelLift(k, c->lift), Mx[16], o[3], r[3];
    float dyS = MV_DRIVER_SEAT_OLD ? 0.f : f[13] + lift - 0.499f; dyS = dyS < -.3f ? -.3f : dyS > .8f ? .8f : dyS;   // №666: f[13] — seat[1]
    carPieceMatrix(k, lift, 0, 0, 0, 1, Mx);
    for (int j = 0; j < 3; j++) o[j] = Mx[j] * f[15] + Mx[4 + j] * (-lift + dyS) + Mx[8 + j] * f[16] + Mx[12 + j];
    float r0[3]; skinRootMotion(a->m, clip, 0.f, r); skinRootMotion(a->m, -1, 0.f, r0);   // корень клипа и покоя (skinSample корень не двигает)
    float q[3] = {-r[0] - r0[0], r[1] - r0[1], r[2] - r0[2]};   // корень в кадре якоря: (−x, y, z) клипа, как seatRoot Гомера
    ScePspFMatrix4 w;   // кадр якоря (homer.c anchorFrame): оси x и z машины наоборот
    w.x.x = -Mx[0]; w.x.y = -Mx[1]; w.x.z = -Mx[2]; w.x.w = 0.f;
    w.y.x = Mx[4];  w.y.y = Mx[5];  w.y.z = Mx[6];  w.y.w = 0.f;
    w.z.x = -Mx[8]; w.z.y = -Mx[9]; w.z.z = -Mx[10]; w.z.w = 0.f;
    w.w.x = o[0] + w.x.x * q[0] + w.y.x * q[1] + w.z.x * q[2]; w.w.y = o[1] + w.x.y * q[0] + w.y.y * q[1] + w.z.y * q[2];
    w.w.z = o[2] + w.x.z * q[0] + w.y.z * q[1] + w.z.z * q[2]; w.w.w = 1.f;
    skinUpdate(&a->p, dt);
    skinDraw(&a->p, &w);
    driversDrawn++; if (i < 64) drvShown[i] = 1;
  }
  static float logT;   // служебное (гейт tests/missions/drivers.sh): при смене числа и раз в 2 с, пока водители есть
  logT += dt;
  if (driversDrawn != driversDrawnLast || (driversDrawn && logT >= 2.f)) {
    logf_("MSN drivers-drawn %d t=%.1f\n", driversDrawn, clock_); driversDrawnLast = driversDrawn; logT = 0.f; }
}
static int talkLogT;   // служебное: строка TALK раз в ~0.5 с разговора, TALKLINE — итог реплики (гейт tests/ui/rob_rows_check.py)
void mvDraw3D(float ex, float ey, float ez, float dt) {
  unsigned xt_ = XS_T(), xw_ = XS_W();
  cfxFrame(dt);   // 04.10: квады эффекта цели — до fxDraw этого кадра
  { int a = cfxActive(); if (cfxWas && !a) logf_("MSN cfx-sprites %u t=%.1f\n", cfxSprites, clock_); cfxWas = a; }
  if (!active) return;
  { const Homer *h = M.g->pHomer;
    float dx = ex - h->x, dy = ey - h->y, dz = ez - h->z;
    float speed = !gameOnFoot(M.g) && M.g->pCar ? M.g->pCar->v : 0.f;
    ambSlots(h->x, h->y, h->z, sqrtf(dx * dx + dy * dy + dz * dz), speed, dt);
  }
  int pcSpeaks = 0, line = gameMsnTalkLine(&M, &pcSpeaks);
  if (logFn && line != tlLine) {   // реплика кончилась: сколько открывался рот у каждого за всю реплику
    if (tlLine >= 0) logf_("TALKLINE line=%d pc=%d jawmax=%.3f,%.3f t=%.1f\n", tlLine, tlPc, tlJawH, tlJawN, clock_);
    tlLine = line; tlPc = pcSpeaks; tlJawH = tlJawN = 0.f;
  }
  XS_LAP(50, xt_, xw_);
  talkClock += dt; talkDt = dt; talkHomer(line, pcSpeaks);
  if (line >= 0 && logFn && (int)(clock_ * 2.f) != talkLogT) {   // где стоят, куда смотрят, кто говорит, жесты
    talkLogT = (int)(clock_ * 2.f); const GameNpc *tn = M.talk.npc[0] ? gameMsnNpc(&M, M.talk.npc) : 0; const Homer *Hm = M.g->pHomer;
    NpcAnim *ta = tn ? animFor(npcKey(tn), 0, 0) : 0;
    logf_("TALK t=%.1f npc=%s line=%d pc=%d homer=%.2f,%.2f,%.3f npcpos=%.2f,%.2f,%.3f dist=%.2f gest=%s,%s jaw=%.3f,%.3f hclip=%d\n", clock_,
          M.talk.npc[0] ? M.talk.npc : "-", line, pcSpeaks, Hm->x, Hm->z, Hm->h, tn ? tn->pos.x : 0.f, tn ? tn->pos.z : 0.f, tn ? tn->h : 0.f,
          tn ? hypotf(Hm->x - tn->pos.x, Hm->z - tn->pos.z) : -1.f, gameMsnTalkAnim(&M, 0, line) ? gameMsnTalkAnim(&M, 0, line) : "none",
          gameMsnTalkAnim(&M, 1, line) ? gameMsnTalkAnim(&M, 1, line) : "none", Hm->jawOpen, ta ? ta->p.jawOpen : 0.f, Hm->clip == Hm->cTalk && Hm->cTalk >= 0);
  }
  for (int i = 0; i < M.nnpc; i++) {
    const GameNpc *n = &M.npc[i]; if (n->twinHide) continue;   // 1296: скрытый бонусный двойник
    NpcAnim *a = animFor(npcKey(n), 0, 0);
    if (!a) continue;
    int clip = (n->walking || n->homing == 1) && !n->blocked && a->walk >= 0 ? a->walk : a->idle, loop = 1;   // №510: путь перекрыт и обойти нельзя — клип покоя, не ходьба на месте
    if (n->knock == TRP_FLY && a->flail >= 0) clip = a->flail;   // правка 240: сбит — как прохожий трафика (traffic.c pedPlay)
    else if (n->knock == TRP_DOWN) { clip = a->land >= 0 ? a->land : a->flail >= 0 ? a->flail : a->idle; loop = 0; }
    else if (n->knock == TRP_GETUP && a->getup >= 0) { clip = a->getup; loop = 0; }
    if (line >= 0 && msn_streq(n->name, M.talk.npc)) {   // собеседник: жест своей реплики, рот, когда говорит он
      // сверка g07b-msnview круг 2 (g04-homer, P15-15): играющий жест не прерывается новой репликой, если новая жеста не задаёт. HOMER_TALK_OLD=1 — прежнее
      if (line != a->talkLine) { const char *g = gameMsnTalkAnim(&M, 1, line); a->talkLine = line;
        if (HOMER_TALK_OLD || g) { a->gest = g ? skinFindClip(a->m, g) : -1; a->gestT = 0.f; } }
      a->gestT += dt;
      if (a->gest >= 0 && a->gestT < skinClipLength(a->m, a->gest)) { clip = a->gest; loop = 0; }
      a->p.jawOpen = pcSpeaks ? 0.f : (SKIN_MOUTH_OLD ? talkJaw(talkClock + .37f) : mouthJaw(&a->mouth, a->name, dt));   // сверка g07b круг 2 (P15-02)
      if (a->p.jawOpen > tlJawN) tlJawN = a->p.jawOpen;
    } else { a->talkLine = a->gest = -1; a->p.jawOpen = 0.f; }
    XS_LAP(51, xt_, xw_);
    if (clip >= 0) skinPlay(&a->p, clip, loop, .2f);
    XS_LAP(52, xt_, xw_);
    if (!npcVisible(n, ex, ey, ez, (!NPC_GOV_OLD && mvGov && n->knock == 0 && !(line >= 0 && msn_streq(n->name, M.talk.npc))) ? NPC_GOV_DIST : NPC_DRAW_DISTANCE)) { skinTick(&a->p, dt); continue; }
    // Правка Роба 500: персонаж вне пирамиды кадра (за спиной камеры, сбоку) — только время клипа, без позы, палитры и рисования: картинка та же, GE всё равно
    // обрезал бы. Рамка с запасом на полёт сбитого. NPC_VIS_OLD=1 — прежнее
    if (!NPC_VIS_OLD && mvVis) { float bb[6] = {n->pos.x - 1.5f, n->pos.y - .3f, n->pos.z - 1.5f, n->pos.x + 1.5f, n->pos.y + 3.f, n->pos.z + 1.5f};
      if (!mvVis(bb)) { skinTick(&a->p, dt); continue; } }
    // Правка Роба 500 (оптимизация отрисовки, персонажи): дальше 30 м поза (сэмпл, смешивание, палитра костей) — через кадр, время клипа идёт каждый кадр, как у
    // прохожих трафика (traffic.c, 25 м). Собеседник и сбитый пешеход — каждый кадр (рот, полёт). NPC_FAR_POSE_OLD=1 — прежнее
    XS_LAP(53, xt_, xw_);
    { float fx = n->pos.x - ex, fz = n->pos.z - ez;
      if (!NPC_FAR_POSE_OLD && fx * fx + fz * fz > 30.f * 30.f && !(line >= 0 && msn_streq(n->name, M.talk.npc)) && n->knock == 0 && (a->poseTick++ & 1)) skinTick(&a->p, dt);
      else skinUpdate(&a->p, dt); }
    XS_LAP(54, xt_, xw_); XS_ADD(57, 1);
    float lt_ = skinFootLift(a->m, a->p.clip); if (!a->liftInit) { a->lift = lt_; a->liftInit = 1; } else a->lift += (lt_ - a->lift) * fminf(1.f, dt * 10.f);   // №510: стопы на полу (сглажено при смене клипа)
    XS_LAP(55, xt_, xw_);
    ScePspFMatrix4 w; skinPlace(&w, n->pos.x, n->pos.y + a->lift, n->pos.z, n->h);
    skinDraw(&a->p, &w);
    XS_LAP(56, xt_, xw_);
  }
  drawDrivers(ex, ey, ez, dt);
}
// №748 (Роб 17.10, TEST31: «всем людям такую же тень… как минимум Мардж, все герои»): кто из персонажей миссий рисуется в этом кадре (дальность, комната,
// не у самого глаза — npcVisible) — точки ног и рост модели для векторной тени
int mvNpcFeet(float (*out)[4], int max, float ex, float ey, float ez, float r) {
  if (!active) return 0;
  int k = 0;
  for (int i = 0; i < M.nnpc && k < max; i++) {
    const GameNpc *n = &M.npc[i]; if (n->twinHide) continue; float dx = n->pos.x - ex, dz = n->pos.z - ez; if (dx * dx + dz * dz > r * r) continue;   // проверка 17.10: сначала дальность, потом поиск модели по имени
    NpcAnim *a = animFor(npcKey(n), 0, 0);
    if (!a || !a->m || !a->m->h || !npcVisible(n, ex, ey, ez, r)) continue;
    float ht = a->m->h->bbox[4] - a->m->h->bbox[1];
    out[k][0] = n->pos.x; out[k][1] = n->pos.y; out[k][2] = n->pos.z; out[k][3] = ht > .3f && ht < 4.f ? ht : 1.84f; k++;
  }
  return k;
}
// сверка g07b-msnview (запрос g06-camera, СВЕРКА_КАМЕРА 2.9 — автозум камеры пешком к ближнему персонажу): позиции персонажей миссии (AddNPC, бонус-персонажи, продавцы)
// в радиусе r от точки (x, z); out[k] = {x, y, z, радиус тела}; без проверки видимости и модели (камере нужна позиция). Сортировка по возрастанию расстояния. -> сколько записано
int mvNpcNearList(float (*out)[4], int max, float x, float z, float r) {
  if (!active || !out || max <= 0) return 0;
  float dd[32], q[32][4]; int k = 0;
  for (int i = 0; i < M.nnpc && k < 32; i++) {
    const GameNpc *n = &M.npc[i]; if (n->twinHide) continue; float dx = n->pos.x - x, dz = n->pos.z - z, d2 = dx * dx + dz * dz; if (d2 > r * r) continue;
    int j = k++; while (j > 0 && dd[j - 1] > d2) { dd[j] = dd[j - 1]; memcpy(q[j], q[j - 1], sizeof q[0]); j--; }
    dd[j] = d2; q[j][0] = n->pos.x; q[j][1] = n->pos.y; q[j][2] = n->pos.z; q[j][3] = .35f;
  }
  if (k > max) k = max;
  memcpy(out, q, (size_t)k * sizeof q[0]);
  return k;
}
// Правка 122 (сверка 30.09, запись PS2 2:13, 2:20, 4:03): зелёные стрелки на дороге к цели задания — набор darrow местности
// (art/l1_terra.p3d: три стрелки через 4 м и свечения, лежат на асфальте) на выезде ближайшего перекрёстка по пути к метке цели
// (game_msn.c gameMsnRouteArrow: дороги трафика). Только за рулём и в городе; путь пересчитывается 4 раза в секунду
#define ROUTE_DT .25f
#ifndef ROAD_ARROW_OLD
#define ROAD_ARROW_OLD 0
#endif
#include "roadarrow.h"   // RA_LIFT .06 м над самой высокой точкой коллизии под набором
#ifndef ROAD_ARROW_BIAS
#define ROAD_ARROW_BIAS 12     // единиц сдвига глубины к камере (слои города — 6)
#endif
int roadArrowOld = ROAD_ARROW_OLD;
static float routeT, routeP[3], routeH; static int routeOn;
#ifndef ROUTE_Y849_OLD
#define ROUTE_Y849_OLD 0
#endif
float gArrowForce[4]; static int forceArrowOn; static float forceArrowP[4];   // №1025 служебно: «flag arrowat X Y Z ГРАД» — набор стрелок в точке без миссии (замер на склоне, лист)
void mvForceArrow(int on, float x, float y, float z, float h) { forceArrowOn = on; forceArrowP[0] = x; forceArrowP[1] = y; forceArrowP[2] = z; forceArrowP[3] = h; }
// сверка g07b-msnview (P16-07, P16-Д4, P13-15): дорожные стрелки к цели — только у целей goto, gooutside, talkto, interior и предметных (race, delivery, dump, pickupitem); у getin, follow, destroy,
// destroyboss, losetail, timer, dialogue, fmv, coins, buycar, buyskin их нет даже в режиме по умолчанию. Режим стрелки этапа (AddObjective, последний аргумент): neither — стрелок нет;
// nearest road — только плавающая стрелка у ближней дороги (её у нас нет — дорожного набора не рисуем); intersection/both/по умолчанию — как было. Переключатель «Intersect Nav System» в Settings
// паузы гасит стрелки: хук mvIntersectNavOn (NULL — включено; ставит main_save.h, передано). Было: стрелка для любой метки цели. SV_NAVARROW_OLD=1 — прежнее
#ifndef SV_NAVPICK_OLD
#define SV_NAVPICK_OLD 0
#endif
#ifndef SV_NAVWHITE_OLD
#define SV_NAVWHITE_OLD 0   // круг 6: 1 — прежний чёрный список (стрелки по умолчанию)
#endif
#ifndef SV_NAVARROW_OLD
#define SV_NAVARROW_OLD 0
#endif
int (*mvIntersectNavOn)(void);
static int routeAllowed(void) {   // решение целиком в msn_rules.h (mvRouteAllowed): та же функция идёт в хост-тест круга 6
  return mvRouteAllowed(M.rt.obj, H.marker.nav, !(mvIntersectNavOn && !mvIntersectNavOn()), SV_NAVARROW_OLD, SV_NAVWHITE_OLD, SV_NAVPICK_OLD);
}
static void drawNavPath(float ex, float ey, float ez, float t) {
  static TrNavArrow arr[24]; static struct { float x, z, y; } yc[24]; static int nyc; static unsigned fr;
  int n = gameMsnNavArrows(&M, arr, 24); routeOn = n > 0;
  if ((fr++ % 60u) == 0u) nyc = 0;   // высоты — раз в 60 кадров
  if (!roadArrowOld) sceGuDepthOffset(ROAD_ARROW_BIAS);
  for (int k = 0; k < n; k++) {
    float P[3] = {arr[k].p[0], arr[k].p[1], arr[k].p[2]};
    if (mvGround) { int f = -1; for (int j = 0; j < nyc; j++) if (fabsf(yc[j].x - P[0]) < .5f && fabsf(yc[j].z - P[2]) < .5f) { f = j; break; }
      if (f < 0 && nyc < 24) { yc[nyc].x = P[0]; yc[nyc].z = P[2]; yc[nyc].y = raLiftedY(mvGround, P, arr[k].h); f = nyc++; }
      if (f >= 0) P[1] = yc[f].y; }
    if (arr[k].hide) continue;   // 108 1312: проехана на 11–20 м — не рисовать
    const char *nm = arr[k].red ? "warrow" : "darrow"; if (arr[k].red && !msnartHas("warrow")) continue;
    msnartDrawRot(nm, P, arr[k].h + 3.14159265f, ex, ey, ez, t);
  }
  if (!roadArrowOld) sceGuDepthOffset(0);
}
static void drawRouteArrow(float ex, float ey, float ez, float t) {
  if (forceArrowOn) { routeOn = 1; routeP[0] = forceArrowP[0]; routeP[1] = forceArrowP[1]; routeP[2] = forceArrowP[2]; routeH = forceArrowP[3]; routeT = 1e9f; }
  else
  if (!H.markerOn || M.interior || M.rt.status != MSN_RUNNING || !routeAllowed() || !pointVisible(H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, NULL)) { routeOn = 0; return; }
  if (!SV_NAVPATH_OLD && !forceArrowOn) {   // сверка g07b круг 2 (P16-06)
    if (navTry && (navRetry -= 1.f / 60.f) <= 0.f) { navRetry = 1.f; navBuilt = gameMsnNavBuild(&M, H.marker.pos.x, H.marker.pos.z) > 0; if (navBuilt) navTry = 0; }
    if (navBuilt) { drawNavPath(ex, ey, ez, t); return; }
  }
  if ((routeT -= 1.f / 60.f) <= 0.f) {
    routeT = ROUTE_DT; int was = routeOn; float wp[3] = {routeP[0], routeP[1], routeP[2]};
    { unsigned r0_ = sceKernelGetSystemTimeLow(); routeOn = gameMsnRouteArrow(&M, &H.marker.pos.x, routeP, &routeH); mvMkUs[3] += 0 * r0_; mvRouteCalcUs += sceKernelGetSystemTimeLow() - r0_; }
    if (routeOn != was || (routeOn && fabsf(wp[0] - routeP[0]) + fabsf(wp[2] - routeP[2]) > 1.f))   // служебное: стрелка появилась/переехала
      logf_("ROUTE on=%d at=%.1f,%.1f,%.1f h=%.2f t=%.1f\n", routeOn, routeP[0], routeP[1], routeP[2], routeH, clock_); }
  if (!routeOn) return;
  // №606 (слово Роба 15.10: «зелёные стрелки на дороге часто проваливаются под дорогу — их не видно»): точка пути — узел дороги трафика, высота у него своя,
  // а асфальт под стрелками (склон, бугор, мост) бывает выше. Набор (три стрелки через 4 м вдоль курса) поднимается на самую высокую точку коллизии под ним + ROAD_ARROW_LIFT,
  // и рисуется со сдвигом глубины к камере (мелкая разница высот в 16-бит глубине не прячет стрелку). ROAD_ARROW_OLD=1 / «flag roadarrowold 1» — как раньше
  float P[3] = {routeP[0], routeP[1], routeP[2]};
  if (!roadArrowOld && mvGround && !(msnartArrow1025 && msnartGroundN)) {   // №1025: каждая стрелка берёт высоту и нормаль своей точки (msnart.c), набор не поднимается целиком
    // №849 (тур TEST39: L3 в езде «стрелка пути» до 0,24 мс эмулятора на кадр): высота набора стрелок — пробы земли — пересчитывается, только
    // когда точка или курс стрелки сменились (gameMsnRouteArrow раз в ROUTE_DT), а не каждый кадр. ROUTE_Y849_OLD=1 — каждый кадр
    static float lastP_[3] = {1e30f, 0.f, 0.f}, lastH_, lastY_;
    if (ROUTE_Y849_OLD || lastP_[0] != routeP[0] || lastP_[1] != routeP[1] || lastP_[2] != routeP[2] || lastH_ != routeH) {
      lastY_ = raLiftedY(mvGround, routeP, routeH); lastP_[0] = routeP[0]; lastP_[1] = routeP[1]; lastP_[2] = routeP[2]; lastH_ = routeH; }
    P[1] = lastY_;
  }
  if (!roadArrowOld) sceGuDepthOffset(ROAD_ARROW_BIAS);
  { unsigned r1_ = sceKernelGetSystemTimeLow(); msnartDrawRot("darrow", P, routeH + 3.14159265f, ex, ey, ez, t); mvRouteDrawUs += sceKernelGetSystemTimeLow() - r1_; }   // стрелки набора смотрят в −Z своих осей
  if (!roadArrowOld) sceGuDepthOffset(0);
}
// Перепись миссий 04.10 (docs/МИССИИ_ПОЛНОТА_2026-10-04.md): предметы целей delivery / dump — модель AddCollectible (2-й аргумент:
// poster_t, tomat, bonestorm, fish …, запись msnart файла из загрузки миссии) на своей точке, пока не подобран; dump — с места, где
// выпал из машины-цели. Раньше — только ромб на радаре (props.item_show не подключён): L1 bm1 тубы и помидоры, m6 игры Bonestorm,
// всего 315 предметов на уровнях 1–6 были невидимы. Оригинал (запись PS2 25:45–25:49, bm1): все помидоры поля видны сразу, каждый
// в жёлтой звезде. Без модели (m4 щитки PP_powerbox) — объект мира, его рисует уровень. Дальше MV_ITEM_FAR м по земле не рисуется —
// правило порта (бюджет GE). -DMV_ITEMS_OLD=1 — прежнее (красный контроль tests/missions/census.sh)
#ifndef MV_ITEMS_OLD
#define MV_ITEMS_OLD 0
#endif
#define MV_ITEM_FAR 120.f
static int itemsDrawnLast = -1;
static unsigned ufoDrawn;   // №967 служебно: сколько раз нарисована тарелка
float mvViewDir[3];   // №967: взгляд камеры этого кадра (ставит main перед mvDrawMarkers): тарелку рисуем, только если она в стороне взгляда
static void drawItems(float ex, float ey, float ez, float t) {
  int n = 0;
  if (!MV_ITEMS_OLD && M.rt.status == MSN_RUNNING && (M.rt.obj == MSN_OBJ_DELIVERY || M.rt.obj == MSN_OBJ_DUMP))
    for (int i = 0; i < M.rt.nitems && i < MSN_MAX_ITEMS; i++) {
      if (M.rt.done[i]) continue;
      if (M.rt.released[i] && M.rt.drop_blink[i] && ((int)(M.rt.drop_life[i] / .256f) & 1)) continue;   // сверка g07 круг 3: последние 5 с жизни выпавшего груза мигает (период 256 мс)
      const char *dr = msn_arg_s(&M.f, M.rt.items[i], 1); if (!dr || !*dr) continue;
      float p[3];
      if (M.rt.released[i]) { p[0] = M.rt.relpos[i].x; p[1] = M.rt.relpos[i].y; p[2] = M.rt.relpos[i].z; }
      else if (M.rt.obj == MSN_OBJ_DUMP) continue;   // таран: предмет ещё в машине-цели
      else { const MsnLoc *l = msn_arg_loc(&M.f, M.rt.items[i], 0); if (!l) continue; p[0] = l->pos[0]; p[1] = l->pos[1]; p[2] = l->pos[2]; }
      if (!pointVisible(p[0], p[1], p[2], NULL)) continue;
      float dx = p[0] - ex, dz = p[2] - ez; if (dx * dx + dz * dz > MV_ITEM_FAR * MV_ITEM_FAR) continue;
      msnartDraw(dr, p, ex, ey, ez, t); n++;
    }
  if (n != itemsDrawnLast) { if (n || itemsDrawnLast > 0) logf_("MSN items-drawn %d t=%.1f\n", n, clock_); itemsDrawnLast = n; }
}
unsigned mvBeamUs, mvBeamN;   // №995 служебно: время света луча тарелки (µs Mac) и кадры
unsigned mvRouteCalcUs, mvRouteDrawUs;   // №849 служебно: расчёт пути стрелки и рисунок стрелок (мкс)
unsigned mvMkUs[5];   // №849/№852 служебно: стрелка пути, предметы, значки персонажей, прочее (мкс)
void mvDrawMarkers(float ex, float ey, float ez, float t) {
  if (!active) return;
  unsigned k0_ = sceKernelGetSystemTimeLow();
  drawRouteArrow(ex, ey, ez, t);
  unsigned k1_ = sceKernelGetSystemTimeLow(); mvMkUs[0] += k1_ - k0_;
  drawItems(ex, ey, ez, t);   // перепись 04.10: предметы целей delivery / dump
  if (!gameBoss967Old && M.ufoOn && !M.interior) {   // №967: тарелка и луч L7 в точке локатора (рисование как в галерее artgal 7, main_loop.h)
    float dx = M.ufoPos[0] - ex, dz = M.ufoPos[2] - ez;
    float dy = M.ufoPos[1] - ey, d2 = dx * dx + dy * dy + dz * dz, dot = dx * mvViewDir[0] + dy * mvViewDir[1] + dz * mvViewDir[2];
    if (d2 < 300.f * 300.f && (d2 < 50.f * 50.f || dot > 0.35f * sqrtf(d2))) { ufoDrawn++; msnartDraw("spaceship", M.ufoPos, ex, ey, ez, t); unsigned tb_ = sceKernelGetSystemTimeLow(); if (!(msnartBeam995 && msnartBeam995(M.ufoPos, ex, ey, ez))) msnartDraw("UfoBeam", M.ufoPos, ex, ey, ez, t); mvBeamUs += sceKernelGetSystemTimeLow() - tb_; mvBeamN++; } }   // №995: mvBeamUs — весь свет луча за кадр (конус или билборды), служебно
  unsigned k2_ = sceKernelGetSystemTimeLow(); mvMkUs[1] += k2_ - k1_;
  if (raceProps[0] && !M.interior) msnartDrawCull(raceProps, NULL, ex, ey, ez, t, 70.f);
  { static unsigned rf_; if (raceProps[0] && !M.interior && ++rf_ >= 60u) { logf_("RACEPROP calls=%u us=%u t_grid=%u t_loop=%u t_all=%u mesh_us=%u meshes=%u binds=%u dist_culled=%u bnd=%u frustum_culled=%u viol=%u off=%d cull=%d cullmem=%u t=%.1f\n", msnartRaceCalls, msnartRaceUs, msnartRaceT[0], msnartRaceT[1], msnartRaceT[2], msnartRaceMeshUs, msnartRaceMesh, msnartRaceBind, msnartRaceCullN, msnartRaceBnd, msnartRaceFrustum, msnartRaceViol, msnartRaceOff, msnartCull1298, msnartCullMemBytes, clock_); msnartRaceT[0] = msnartRaceT[1] = msnartRaceT[2] = msnartRaceMeshUs = msnartRaceBnd = msnartRaceFrustum = msnartRaceUs = msnartRaceMesh = msnartRaceBind = msnartRaceCullN = msnartRaceCalls = 0; rf_ = 0; } }   // 1298 замер раз в 60 кадров   // правка 122: шины и отбойники трассы гонки
  if (H.markerOn && H.marker.drawable && *H.marker.drawable) {
    int ident = M.rt.obj == MSN_OBJ_INTERIOR;   // вход в здание: звезда двери смоделирована в координатах мира (kwk_door.p3d)
    if (ident || pointVisible(H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, NULL)) {
      // 1317 (Роб, L2 m4: «финишная лента не перпендикулярна дороге, а вдоль полосы»): лента finish_line (fline.p3d, AddCollectible гонки) стоит на локаторе с его поворотом (матрица локатора в данных диска, у CARSTART — курс),
      // а мы рисовали без поворота — лента лежала вдоль осей модели, не поперёк дороге. Теперь с поворотом локатора (MsnLoc.flags бит0 курс CARSTART / бит1 матрица, данные миссий пересобраны). FINLINE1317_OLD=1 / flag finline1317old 1 — прежнее
      if (!mvFinLine1317Old && !ident && !strcmp(H.marker.drawable, "finish_line") && H.marker.loc && (H.marker.loc->flags & 3) && H.marker.loc->heading == H.marker.loc->heading)
        msnartDrawRot(H.marker.drawable, &H.marker.pos.x, H.marker.loc->heading, ex, ey, ez, t);
      else msnartDraw(H.marker.drawable, ident ? 0 : &H.marker.pos.x, ex, ey, ez, t);
      // 04.10 (запись PS2 10:34.1–10:34.3: у сферы цели carsphere — светлое кольцо на земле): пятно цели «<рисунок>_shadow» того
      // же файла (missgen.p3d: carsphere_shadow — кольцо ring.bmp 10 м, triggersphere_shadow; у предметов миссий — tux_shadow,
      // flanpic_shadow…), как у значков бонус-миссий (правка 180). mkShadowOld = 1 — прежнее (без пятна, флаг mkshadowold)
      // Правка Роба (В60, 10.10): запись PS2 m2 (8:44–8:46): под предметом (смокинг) пятна-тени на земле НЕТ, иконка в жёлтой звезде парит на 1–1,5 м; кольцо
      // на земле — только у сфер цели (carsphere / triggersphere). MK_ITEM_SHADOW_OLD=1 — пятно и у предметов (tux_shadow, flanpic_shadow…)
#ifndef MK_ITEM_SHADOW_OLD
#define MK_ITEM_SHADOW_OLD 0
#endif
      if (!ident && !mkShadowOld && (MK_ITEM_SHADOW_OLD || strstr(H.marker.drawable, "sphere"))) { char sh[48]; snprintf(sh, sizeof sh, "%s_shadow", H.marker.drawable); if (!mvFlatGlow || !mvFlatGlow(sh, &H.marker.pos.x, ex, ey, ez)) msnartDraw(sh, &H.marker.pos.x, ex, ey, ez, t); }   // №793: кольцо сферы — векторно
    }
  }
  // стрелка над машиной миссии (missgen.p3d, LoadP3DFile уровня: arrow_race / arrow_destroy / arrow_chase / arrow_evade). ПРАВИЛО
  // ПОРТА (какая стрелка когда — по именам данных): цель тарана и груза (destroy, dump) — arrow_destroy, цель слежки (follow) —
  // arrow_chase, погоня за игроком (losetail) — arrow_evade, соперник гонки — arrow_race; высота — крыша + 0.6 м
  if (M.rt.status == MSN_RUNNING || M.rt.status == MSN_COUNTDOWN) for (int i = 0; i < M.cars.n; i++) {
    const McCar *c = &M.cars.c[i]; if (!c->on || c->mode == MC_PARKED || c->finished || c->car.destroyed) continue;
    const char *arrow = 0; int tgt = M.rt.target && msn_streq(M.rt.target, c->name);
    if (tgt && M.rt.obj == MSN_OBJ_DUMP) arrow = mvDumpArrow1264(M.rt.nbind, msnCosm1264Old & 4);   // №1264 A1.8: с привязками к точкам пути — «следуй»
    else if (tgt && M.rt.obj == MSN_OBJ_DESTROY) arrow = "arrow_destroy";
    else if (tgt && M.rt.obj == MSN_OBJ_FOLLOW) arrow = "arrow_chase";
    else if (tgt && M.rt.obj == MSN_OBJ_LOSETAIL) arrow = "arrow_evade";
    else if (M.rt.obj == MSN_OBJ_RACE) arrow = "arrow_race";
    if (!arrow) continue;
    float dx = c->car.x - ex, dz = c->car.z - ez; if (dx * dx + dz * dz > 150.f * 150.f) continue;
    float p[3] = {c->car.x, c->car.y + c->car.p.bodyHi + .6f, c->car.z}; msnartDraw(arrow, p, ex, ey, ez, t);
  }
  // A1.3 (сверка Fable 13.11, §4 с поведением оригинала): на этапе «сядь в машину» над машиной, в которую надо сесть (текущая машина игрока, у строгого getin — названная),
  // на высоте крыши прыгает стрелка «arrow» (animatedicon.h: размер 1,8 на 10 м и ближе, 5,0 на 80 м и дальше — масштаб по расстоянию в msnartDraw). Раньше стрелка была у destroy/dump/follow/losetail/race, у getin — нет.
  // MVGETINARROW_OLD=1 — как было (без стрелки)
  {
#ifndef MVGETINARROW_OLD
#define MVGETINARROW_OLD rb_MVGETINARROW_OLD
#endif
    static int gaOn;
    int want = !MVGETINARROW_OLD && (M.rt.status == MSN_RUNNING || M.rt.status == MSN_COUNTDOWN) && M.rt.obj == MSN_OBJ_GETIN && !M.interior && M.g && M.g->pCar && gameOnFoot(M.g);
    if (want) {
      const Car *gc = &M.g->pCar[0]; const McCar *named = (M.rt.target && *M.rt.target && !msn_streq(M.rt.target, "current") && !msn_streq(M.rt.target, "default")) ? (const McCar *)mcFind(&M.cars, M.rt.target) : 0;
      if (named && named->on) gc = &named->car;
      float dx = gc->x - ex, dz = gc->z - ez;
      if (dx * dx + dz * dz < 150.f * 150.f) {
        float p[3] = {gc->x, gc->y + gc->p.bodyHi + .6f, gc->z}; msnartDraw("arrow", p, ex, ey, ez, t);
        if (!gaOn) { gaOn = 1; logf_("MSN getin-arrow drawn car=%s dist=%.1f t=%.1f\n", named ? named->name : "current", sqrtf(dx * dx + dz * dz), clock_); }
      }
    } else gaOn = 0;
  }
  // Правка 180 (близнец О4 03.10, кадры f03600 и f04600): над персонажем бонус-миссии — её значок (AddNPCCharacterBonusMission,
  // 5-й аргумент: checkered — уличная гонка, dice — гонка на ставку, exclamation — бонус-миссия), под ним — светлое пятно
  // <значок>_shadow (missgen.p3d), пока миссия не пройдена. Дальше 100 м не рисуется (правило порта)
  unsigned k3_ = sceKernelGetSystemTimeLow(); mvMkUs[3] += k3_ - k2_;
  if (!M.interior) for (int i = 0; i < M.nnpc; i++) {
    const GameNpc *n = &M.npc[i]; int mi = n->bonus - 1; if (n->twinHide) continue;
    if (mi < 0 || (uint32_t)mi >= M.f.nmissions || mi >= 32) continue;
    const MsnMission *ms = &M.f.missions[mi]; if (ms->bonus_npc == 0xFFFFFFFFu) continue;
    int doneB = (M.rt.completed & (1u << mi)) && !M.rt.dev_bonus_replay;
    // сверка g07b-msnview (P06-2): одноразовым персонаж становится, только если 7-й аргумент AddNPCCharacterBonusMission равен 1 (bm1 Скиннера, Клетуса, Дедушки…). Уличные гонки и bm1 со
    // Смитерсом остаются: после победы второй значок (checkeredfinish у гонок, exclamation_shadow у bm1) и миссию можно запускать снова. Запуск и удаление персонажа — mission/msn_run.c (передано).
    // Было: любая завершённая бонус-миссия пропадала. SV_BONUS_REPEAT_OLD=1 — прежнее
    int once = msn_arg_i(&M.f, &M.f.cmds[ms->bonus_npc], 6) == 1;
    if (doneB && (SV_BONUS_REPEAT_OLD || once)) continue;
    const char *icon = msn_arg_s(&M.f, &M.f.cmds[ms->bonus_npc], 4); if (!icon || !*icon) continue;
    if (doneB && !strcmp(icon, "checkered")) icon = "checkeredfinish";
    else if (doneB && !strcmp(icon, "exclamation")) icon = "exclamation_shadow";
    float dx = n->pos.x - ex, dz = n->pos.z - ez; if (dx * dx + dz * dz > 100.f * 100.f) continue;
    int rt=!strcmp(icon,"checkered")?UI_R_STREETRACE:!strcmp(icon,"dice")?UI_R_WAGER:!strcmp(icon,"exclamation")?UI_R_BONUS:-1;
    float p[3] = {n->pos.x, n->pos.y, n->pos.z}; msnartDrawNpcMarker(icon, rt, p, ex, ey, ez, t);
    char sh[40]; snprintf(sh, sizeof sh, "%s_shadow", icon); p[1] += .03f; if (!mvFlatGlow || !mvFlatGlow(sh, p, ex, ey, ez)) msnartDraw(sh, p, ex, ey, ez, t);   // №793: векторно
  }
  mvMkUs[2] += sceKernelGetSystemTimeLow() - k3_;
  unsigned k4_ = sceKernelGetSystemTimeLow();
  if (H.talk[0] && M.rt.status == MSN_RUNNING && M.rt.obj == MSN_OBJ_TALKTO) {   // значок над собеседником (missgen: exclamation)
    const GameNpc *n = gameMsnNpc(&M, H.talk);
    if (n && npcVisible(n, ex, ey, ez, NPC_DRAW_DISTANCE)) { float p[3] = {n->pos.x, n->pos.y + H.talkH, n->pos.z}; msnartDrawNpcMarker("exclamation", UI_R_MISSION, p, ex, ey, ez, t); }
  }
  // Правка Роба 329 (04.10: «над Барни нет никакого обозначения, что он магазин — должен быть 3D-символ доллара»): над продавцом
  // машин и костюмов — значок dollar из missgen.p3d (в пакете msnart рядом с exclamation; как значок над собеседником), если он
  // сейчас не собеседник задания (тогда над ним exclamation); правило порта по данным диска. Прежнее — flag dollarold 1
  if (!mvDollarOld) for (int i = 0; i < M.shop.nseller; i++) {
    const GameNpc *n = gameMsnNpc(&M, M.shop.seller[i].npc);
    if (!n || !npcVisible(n, ex, ey, ez, NPC_DRAW_DISTANCE)) continue;
    if (H.talk[0] && M.rt.status == MSN_RUNNING && M.rt.obj == MSN_OBJ_TALKTO && !strcmp(H.talk, M.shop.seller[i].npc)) continue;
    float p[3] = {n->pos.x, n->pos.y, n->pos.z}; msnartDrawNpcMarker("dollar", UI_R_PURCHASE, p, ex, ey, ez, t);
  }
  mvMkUs[4] += sceKernelGetSystemTimeLow() - k4_;   // №852 служебно: значки разговора и продавцов
}

// ------------------------------------------------------------------ HUD
// Лавка продавца (пункт 6 ведущего 29.09; логика — viewer/game_msn.c, viewer/shop.h). Экран — PurchaseRewards.scr оригинала
// (viewer/shop_view.c: shop.shr + reward-<k>.shr рядом с EBOOT; сцена с машиной на подиуме, как в записи PS2 14:05);
// пока он на экране, кнопки его. Нет пакетов — запасная карточка брифинга (MissionLoad): название, цена, кошелёк;
// X — купить, △/○ — отказаться, ◄ ► — выбор. Мир стоит, как у брифинга (main.c ветка mvBriefing)
static int shopOn(void) { return active && gameShopOpen(&M) >= 0; }
Shop *mvShop(void) { return active ? &M.shop : 0; }
int mvBriefing(void) { return active && (H.briefing || shopOn()); }
int mvBriefLoading(void) { return active && H.briefing && !H.brLoaded; }   // №626: брифинг ещё под газетой LOADING
void mvBriefingInput(unsigned hit) {   // X — Continue, △ — Cancel Mission (кнопки страницы MissionLoad.pag)
  if (!mvBriefing()) return;
  if (shopViewOn()) { shopViewInput(&M.shop, hit); return; }
  if (!H.briefing && shopOn()) {
    GamePad p; memset(&p, 0, sizeof p);
    if (hit & PSP_CTRL_CROSS) p.hit |= GP_CROSS; if (hit & PSP_CTRL_TRIANGLE) p.hit |= GP_TRIANGLE; if (hit & PSP_CTRL_CIRCLE) p.hit |= GP_CIRCLE;
    if (hit & PSP_CTRL_LEFT) p.hit |= GP_LEFT; if (hit & PSP_CTRL_RIGHT) p.hit |= GP_RIGHT;
    unsigned b0 = M.shop.buys; gameShopInput(&M, &p);
    if (M.shop.buys != b0) logf_("MSN shop-buy item=%d coins=%d t=%.1f\n", shopSellerItem(&M.shop, M.shop.at, M.shop.sel), shopCoins(&M.shop), clock_);
    return;
  }
  { static int ldSeen_;   // 1357: карточка въезжает UI_BR_IN (0,40 с) после загрузки; в оригинале ввод только в состоянии RUNNING, когда кнопки видны (и загрузка кончена). До этого ✕/△/○ не считаются
    if (!H.brLoaded) ldSeen_ = 0; else if (!ldSeen_) { ldSeen_ = 1; ldUs_ = sceKernelGetSystemTimeLow(); }
    if (!inputHints1357Old && !brOld && H.brLoaded && (unsigned)(sceKernelGetSystemTimeLow() - ldUs_) < 400000u) hit &= ~(PSP_CTRL_CROSS | PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE); }
  if (!brOld && !H.brLoaded) { if ((hit & PSP_CTRL_CROSS) && mvLoadQ1303Old) H.brQueued = 1; else if (hit & PSP_CTRL_CROSS) logf_("MSN briefing-x-ignored (LOADING) t=%.1f\n", clock_); return; }   // правка 52: ✕ во время LOADING — после загрузки (1303: теперь не считается)
  if (wagerShort() && (hit & PSP_CTRL_CROSS)) hit &= ~PSP_CTRL_CROSS;   // сверка g07b: монет не хватает — ✕ ничего не делает (P13-10)
  if (hit & PSP_CTRL_CROSS) { H.briefing = 0; brModeOff(); uiBriefAnim(0); logf_("BRIEFIN1357 continue after_loaded_ms=%u\n", (unsigned)(sceKernelGetSystemTimeLow() - ldUs_) / 1000u); logf_("MSN briefing-continue t=%.1f\n", clock_); mvContUs1326 = sceKernelGetSystemTimeLow() | 1u; if (!msnTimerHold1324Old) { M.rt.hold_done = 0; M.rt.hold_t = 0.f; M.rt.hold_log = -1; }   /* 1324: карточка ушла — ожидание готового мира считается отсюда, не со старта этапа под карточкой */ if (!svWagerAcceptOld && H.brWager && M.rt.status == MSN_RUNNING && M.rt.cur >= 0) { wagerPayPend = M.rt.cur + 1; logf_("MSN wager-accept fee=%d t=%.1f\n", H.brFee, clock_); } if (M.mg.hud.objective && *M.mg.hud.objective) uiObjectiveCb(&M, M.mg.hud.objective, M.mg.hud.objective_msg, M.mg.hud.icon);   // платёж ждёт этап coins (на L2 перед ним идёт вступительный диалог)
    // правка 345: [PS2] через 3 кадра после ✕ Continue — реплика водителя на старте миссии W_Mstart_<код>_* (3 из 3, выбор случайный)
    // сверка g07b-msnview (P09 Д8): принятие брифинга в гонке (уличная, на ставку) реплику Mstart не даёт. Было: в любой миссии. SV_RACE_MSTART_OLD=1 — прежнее
    if ((SV_RACE_MSTART_OLD || !(M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions && mvMstartMuted(M.f.missions[M.rt.cur].flags, SV_RACE_MSTART3_OLD))) && sndSay(SND_SAY_MSTART, 1)) logf_("MSN say mstart %s t=%.1f\n", sndSayLastName(), clock_); }
  else if (brOld && (hit & PSP_CTRL_TRIANGLE)) { H.briefing = 0; brModeOff(); logf_("MSN briefing-cancel t=%.1f\n", clock_); gameMsnAbort(&M); }   // только прежняя карточка (отр. контроль): в эталоне PS2 «Cancel Mission» на карточке нет
  // сверка g07b-msnview: M2 Д3 / P13 Д1 — на карточке гонки на ставку после загрузки две кнопки: ✕ принять и ○ отказаться (△ как у подэкранов); отказ = Abort миссии (ирис, свободная езда, игрок у локатора).
  // Было: ○ и △ на новой карточке ничего не делали, ставку нельзя было отклонить. SV_WAGER_CANCEL_OLD=1 — прежнее
  else if (!SV_WAGER_CANCEL_OLD && H.brWager && (hit & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) && (SV_ABORTOK_OLD || gameMsnAbortAllowed(&M))) { H.briefing = 0; brModeOff(); uiBriefAnim(0); logf_("MSN briefing-cancel wager t=%.1f\n", clock_); uiPopHookFire(UI_EV_BACK); gameMsnAbort(&M); }   /* круг 5 (M2 Д3): звук отмены (FE_CANCEL = «назад» меню) */
}
// Разбивка mvHud (15.10, TEST18: «mvhud» 2,0–2,9 мс на кадр в погонях, 0,03 вне миссий): метки MVPF(k), мкс по кускам, строка MVHUD раз в секунду (mvProfTake). Только тест-апка (mvProfOn)
int mvProfOn; static unsigned mvPf[9], mvPfN, mvPfT;
#define MVPF(k) do { if (mvProfOn) { unsigned n_ = sceKernelGetSystemTimeLow(); mvPf[k] += n_ - mvPfT; mvPfT = n_; } } while (0)
int mvProfTake(char *out, int n) {
  if (!mvPfN) return 0;
  static const char *N[9] = {"pre", "brief", "radar", "cars", "items", "action_racepos", "vo", "damage_prox", "tail"};
  double k = 1.0 / mvPfN / 1000.0; int l = snprintf(out, (size_t)n, "MVHUD n=%u", mvPfN);
  for (int i = 0; i < 9; i++) l += snprintf(out + l, (size_t)(n - l), " %s=%.2f", N[i], mvPf[i] * k);
  l += snprintf(out + l, (size_t)(n - l), "\n"); memset(mvPf, 0, sizeof mvPf); mvPfN = 0; return l;
}
void mvHud(float dt) {
  if (!active) return;
  bubTick1310(dt);
  if (mvProfOn) { mvPfT = sceKernelGetSystemTimeLow(); mvPfN++; }
  if (!H.briefing && shopOn() && !shopViewOn()) {   // лавка продавца карточкой брифинга, если экрана покупки нет (см. mvBriefing)
    const Shop *S = &M.shop; int it = shopSellerItem(S, S->at, S->sel), n = shopSellerCount(S, S->at); static char title[48], info[160];
    if (it >= 0) {
      const ShopItem *t = &S->item[it];
      const char *nm = !strcmp(t->name, "plowk_v") ? "PLOW KING" : !strcmp(t->name, "cDuff") ? "DUFF TRUCK" : t->name;   // строки библии (нет в пакете миссий)
      snprintf(title, sizeof title, "%s", nm);
      snprintf(info, sizeof info, "%d coins%s\nYOU HAVE %d coins\nX PURCHASE   O CANCEL%s", t->price,
               shopOwns(S, t->kind, t->name) ? "  (OWNED)" : "", shopCoins(S), n > 1 ? "   < >" : "");
    } else { snprintf(title, sizeof title, "%s", S->seller[S->at].npc); snprintf(info, sizeof info, "-"); }
    uiSetBriefing(title, info, 0, 0, 0, 0, 0);
  }
  if (H.briefing) {   // картинка может ещё дочитываться (msnart асинхронно) — берём каждый кадр
    const void *pix = 0; int tw = 0, th = 0, w = 0, h = 0;
    if (H.present[0]) msnartImageData(H.present, &pix, &tw, &th, &w, &h);
    { static char need[64]; if (wagerShort()) snprintf(need, sizeof need, "YOU NEED %d COINS", H.brFee);   /* сверка g07b P13-10: вместо описания */
    uiSetBriefing(H.title, wagerShort() ? need : H.info, pix, tw, th, w, h); }
    if (!H.brLoaded && briefReady()) { H.brLoaded = 1; logf_("MSN briefing-loaded path=%s ra_wait=%.2f wait=%.2f first=%d queued=%d pic=%s lost=%d title=\"%s\" t=%.1f\n", H.brFirst ? "newspaper" : "card-only", brReadyT < 0.f ? 0.f : briefSec() - brReadyT, briefSec(), H.brFirst, H.brQueued, H.present[0] ? H.present : "-", H.brPicLost, uiBriefTitle(), clock_);
      if (!BRIEF_ANIM_AT_LOAD_OLD) uiBriefAnim(1);   // №612: въезд карточки и руки — с момента, когда карточку видно (до этого экран газеты LOADING); на видео TEST17 въезд проходил под газетой
      if (H.brQueued) { H.brQueued = 0; mvBriefingInput(PSP_CTRL_CROSS); } }
    float bs = briefSec(); int blinkOn = SV_LOADBLINK_OLD ? mvPulseOn(bs, BRIEF_BLINK) : !mvPulseOn(bs, BRIEF_BLINK);   // сверка g07b M2:27 (Н): LOADING скрыта при входе, первый переключатель через 0,25 с (SV_LOADBLINK_OLD=1 — горит с первого кадра)
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_PRESS_START, H.brLoaded && !wagerShort());                // ✕ Continue
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_GAMBLE, H.brWager && H.brLoaded);       // строки ставки — вместе с текстом карточки
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_LOADING, !H.brLoaded && blinkOn);         // LOADING мигает
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_SKIP, brOld || (!SV_WAGER_CANCEL_OLD && H.brWager && H.brLoaded && (SV_ABORTOK_OLD || gameMsnAbortAllowed(&M))));                             // Cancel Mission — только прежняя карточка
  }
  MVPF(1);
  dropTick(); lapTick(); raceFenceTick();
  if (oovOn && !SV_EVENTS2_OLD) { float left = oovWin - (clock_ - oovT0); if (left < 0.f) left = 0.f; uiHudSet(UI_F_TIMER, (int)(left * 1000.f)); uiHudShow(UI_T_TIMER, 1); }   // окно «сядь в машину»
  uiHudShow(UI_T_STAGE_COMPLETE, clock_ < H.stageFlash);
  uiHudShow(UI_T_MISSION_COMPLETE, clock_ < H.missionFlash);
  uiHudShow(UI_T_MISSION_FAILED, 0);   // FAILED рисует экран MissionOver
  uiRadarClear();
  for (int i = 0; i < H.ngiver; i++) uiRadarAdd(H.giver[i].type, H.giver[i].x, H.giver[i].z);
  if (H.markerOn && pointVisible(H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, NULL)) uiRadarAdd(UI_R_MISSION, H.marker.pos.x, H.marker.pos.z);
  MVPF(2);
  // сверка с оригиналом 30.09 (запись PS2 5:05, 15:30, 19:30, 30:30): на радаре — машины миссии: соперники гонки, цель тарана,
  // слежки и погони (группа AICars), кто гонится за игроком — погоня Hit & Run, «уйди от хвоста» (AIHarassCars); предметы сбора —
  // жёлтыми ромбами (Collectibles). Стоящие машины миссии не показываются
  if (!M.interior) for (int i = 0; i < M.cars.n; i++) {
    const McCar *c = &M.cars.c[i]; if (!c->on || c->mode == MC_PARKED) continue;
    uiRadarAddH(c->mode == MC_CHASE ? UI_R_HARASS : UI_R_AICAR, c->car.x, c->car.z, c->car.h);   // стрелка по курсу машины
  }
  MVPF(3);
  if (M.rt.status == MSN_RUNNING && (M.rt.obj == MSN_OBJ_DELIVERY || M.rt.obj == MSN_OBJ_DUMP))
    for (int i = 0; i < M.rt.nitems && i < MSN_MAX_ITEMS; i++) {
      if (M.rt.done[i]) continue;
      if (M.rt.obj == MSN_OBJ_DUMP) {   // таран: предмет виден, когда выпал из машины (место выпадения)
        if (M.rt.released[i]) uiRadarAdd(UI_R_COLLECTIBLE, M.rt.relpos[i].x, M.rt.relpos[i].z);
        continue; }
      const MsnLoc *l = msn_arg_loc(&M.f, M.rt.items[i], 0);
      if (l && pointVisible(l->pos[0], l->pos[1], l->pos[2], NULL)) uiRadarAdd(UI_R_COLLECTIBLE, l->pos[0], l->pos[2]);
    }
  MVPF(4);
  if (mvWantsAction()) { char lb[48]; uiHudShow(UI_T_ACTION, 1); uiActionText(gameMsnActionLabel(&M, lb, sizeof lb)); }   // подсказка △ у собеседника; правка 206: что делает
  // машины миссии (родные элементы Scrooby HUD): место в гонке с соперниками; урон машины-цели (destroy / dump — показывается
  // с первого удара); близость цели слежки / погони: доля предела (follow — SetFollowDistances, losetail — SetObjDistance)
  int racers = 0, pos = gameMsnRacePos(&M, &racers);
  uiHudShow(UI_T_POSITION, pos > 0); if (pos > 0) { uiHudSet(UI_F_POSITION, M.raceOffRoad ? -1 : pos); uiHudSet(UI_F_RACERS, racers + 1); }   // место этого кадра — на экране в этом же кадре; «/2» (аудит 09.10): соперники + игрок
  MVPF(5);
  { static int voPos; voRace(pos, voPos); voPos = pos; voTick(); }   // аудит 09.10: обгон, удары по сопернику, итог миссии
  { static int lastPos = -1;   // служебное (гейт tests/aicar/racepos.sh, правка 326): строка в журнал сценария при каждой смене места
    if (logFn && pos != lastPos) { lastPos = pos; MsnWorld w; gameMsnWorld(&M, &w); char b[200]; int n = 0;
      for (int i = 0; i < M.cars.n && n < 120; i++) { const McCar *c = &M.cars.c[i]; if (!c->on || c->rAhead < 0) continue;
        n += snprintf(b + n, sizeof b - (size_t)n, " %s x=%.1f z=%.1f rk=%d d=%.1f a=%d", c->name, c->car.x, c->car.z, c->rk, c->rDist, c->rAhead); }
      b[n] = 0;
      logf_("RACEPOS t=%.2f pos=%d/%d px=%.1f pz=%.1f pk=%d dp=%.1f%s\n", clock_, pos, racers, w.player.x, w.player.z,
            M.rt.lap * M.rt.nitems + M.rt.next_item, M.raceDp, b); } }
  { static float progT = -1e9f;   // служебное (правка 1205): ход гонки раз в секунду — у каждого соперника пройдено своих точек (wp) и точек гонки игрока (rk), скорость, доехал ли
    if (logFn && pos > 0 && clock_ - progT >= 1.f) { progT = clock_; char b[560]; MsnWorld w; gameMsnWorld(&M, &w);
      int n = snprintf(b, sizeof b, "RACEPROG t=%.1f pos=%d/%d px=%.0f pz=%.0f pk=%d dp=%.0f on=%d", clock_, pos, racers, w.player.x, w.player.z,
                       M.rt.lap * M.rt.nitems + M.rt.next_item, M.raceDp, !w.on_foot);
      for (int i = 0; i < M.cars.n && n < 440; i++) { const McCar *c = &M.cars.c[i]; if (!c->on || c->rAhead < 0) continue;
        n += snprintf(b + n, sizeof b - (size_t)n, " %s x=%.0f z=%.0f v=%.0f wp=%d rk=%d d=%.0f a=%d f=%d%s", c->name, c->car.x, c->car.z, c->car.v, c->wp, c->rk, c->rDist, c->rAhead, c->finished, c->waiting ? " W" : ""); }
      snprintf(b + n, sizeof b - (size_t)n, "\n"); logFn(b); } }
  MVPF(6);
  const McCar *tc = M.rt.status == MSN_RUNNING ? gameMsnTargetCar(&M) : 0; int obj = M.rt.obj;
  uiSetDamage(tc && (obj == MSN_OBJ_DESTROY || obj == MSN_OBJ_DUMP) ? tc->car.damage : 0.f);
  float lim = 0.f;
  if (tc && obj == MSN_OBJ_LOSETAIL) lim = M.rt.objdist > 0.f ? M.rt.objdist : 150.f;
  if (tc && obj == MSN_OBJ_FOLLOW) for (int k = 0; k < M.rt.st->ncond; k++) {
    const MsnCond *cd = &M.f.conds[M.rt.st->cond_first + k];
    for (unsigned i = 0; cd->type == MSN_COND_FOLLOWDISTANCE && i < cd->cmd_count; i++) {
      const MsnCmd *c = &M.f.cmds[cd->cmd_first + i]; if (c->op == MSN_OP_SETFOLLOWDISTANCES) lim = msn_arg_f(&M.f, c, 1); } }
  // 1318 (Роб: шкалы «красная — разбить, жёлтая — угнать как можно дальше, зелёная — прогресс»: что-то не так; сверка с оригиналом и записью PS2 L1 m3/m5): шкала близости в оригинале стоит полной, пока игрок рядом с целью,
  // и пустеет по мере удаления: значение = 1 − расстояние/предел (у нас было расстояние/предел, то есть наоборот); цвет полосы красный у «оторваться» (losetail), зелёный у «не отставай» (follow), у нас серый.
  // PROXBAR1318_OLD=1 / flag proxbar1318old 1 — прежнее
  if (lim > 0.f) { MsnWorld w; gameMsnWorld(&M, &w); float d = hypotf(w.player.x - tc->car.x, w.player.z - tc->car.z) / lim;
    float fill_ = mvProxBar1318Old ? d : 1.f - d; if (fill_ < 0.f) fill_ = 0.f; if (fill_ > 1.f) fill_ = 1.f;
    uiHudSet(UI_F_PROXIMITY, (int)(fill_ * 1000.f)); uiProxTint(mvProxBar1318Old ? 0u : obj == MSN_OBJ_LOSETAIL ? 0xff0000ffu : 0xff00ff00u);
    static float pl_; if (clock_ - pl_ >= 1.f) { pl_ = clock_; logf_("PROXBAR1318 obj=%s dist=%.1f lim=%.1f fill=%d t=%.1f\n", obj == MSN_OBJ_LOSETAIL ? "losetail" : "follow", d * lim, lim, (int)(fill_ * 1000.f), clock_); } }
  uiHudShow(UI_T_PROXIMITY, lim > 0.f);
  MVPF(7);
}
static void centered(int font, int y, unsigned col, const char *s) {
  int w = uiText(font, 0, -100, 0x00000000u, s);   // замер ширины (невидимо, за экраном)
  uiText(font, (480 - w) / 2, y, col, s);
}
void mvOverlay(void) { }   // провал рисует экран MissionOver целиком (uiDrawMissionOver)
int mvFailedScreen(int *sel) { if (!H.failed) return 0; *sel = H.failSel; return 1; }
int mvInput(unsigned *hit) {   // MissionOver: «Retry Mission?» YES — повтор с чекпоинта, NO — выход из миссии
  if (!H.failed) return 0;
  int act = uiMenuInput(UI_MENU_MISSIONOVER, &H.failSel, *hit);
  if (act == 2 && mvFailSkipOk()) { mvFailSkip(); *hit = 0; return 1; }   // сверка g07b P13-3: пункт SKIP (включает ui.c через mvFailSkipOk; без пункта — не достижим)
  if (act == 0 || act == 1) {
    H.failed = 0; failStoreOff(); uiHudShow(UI_T_MISSION_FAILED, 0);
    bbNoteMsnEnd(act ? "abort" : "retry");
    if (!act && mvSkipCount1331Old) msTryBump(); logf_("MSN %s tries=%d t=%.1f\n", act ? "abort" : "retry", M.rt.cur >= 0 && M.rt.cur < 32 ? msTries[M.rt.cur] : -1, clock_);
    { char b_[100]; snprintf(b_, sizeof b_, "%s stage=%d.%d tries=%d rem=%.1f", act ? "abort" : "yes", M.rt.phase, M.rt.stage, M.rt.cur >= 0 && M.rt.cur < 32 ? msTries[M.rt.cur] : -1, M.rt.remaining); bbNoteDiag("RETRY", b_); }   // 1331: в ящик выпуска   /* сверка g07b P13-3: Retry = учтённый сброс */
    if (active) { if (act) { if (!SV_RESTART_IRIS_OLD) msnAudioStopVoice(&M.audio); gameMsnAbort(&M); } else restartIris(2); }
  }
  *hit = 0; return 1;
}

// ------------------------------------------------------------------ пауза: MISSION SELECT (сюжетные миссии уровня)
// Экран оригинала MissionSelect.scr (pextra.shr открыт вызывающим, 29.09): 7 строк MissionSelect.pag — 7 последних сюжетных
// миссий уровня (на всех 7 уровнях строки страницы = они; у уровня 1 первая, The Cola Caper, в список не входит), номер, название,
// значок: check — пройдена, cross — текущая, dash — закрыта (правило порта: смысл значков в данных не подписан); выбрать можно
// пройденные и текущую. Без пакета — прежний список (пройденные + текущая) текстом в рамке паузы
// Правка Роба 348 (05.10): уровень на экране листается ◄ ► ([диск] планка LEVEL N со стрелками и шариками LArrowBgd/RArrowBgd):
// только открытые уровни (mvMissionSelectSetup: набор рядом с EBOOT и уровень открыт — в DEV SAVE все, иначе — уровень игры; правило
// порта: в записи хранится прогресс одного уровня, уход назад в обычной игре потерял бы его), закрытые не листаются; шарик стрелки
// красный MS_ARROW_FR кадров после нажатия (правило порта: в записи PS2 экрана нет). Строки чужого уровня — как у DEV (сюжетные, кроме
// последней, пройдены — game_msn.c gameMsnDevMask); X на них — тот уровень с этой миссией (mvMissionSelectWant -> main.c levelAdvance)
static int selOn, selIdx, selList[16], selN, selRows;   // selRows: 1 — строки экрана оригинала (selList[k] = миссия строки k или -1)
static int msLv, msArrL, msArrR, msOld, msDev, msWantLv, msWantRow = -1;   // правка 348: уровень на экране, шарики стрелок (кадры)
static int (*msOpen)(int);
static unsigned (*msRows)(int);   // облако 05.10 (карьера): строки другого уровня по записи прогресса
#define MS_ARROW_FR 12   // ~0,2 с при 60 к/с
// №943 (Роб 24.10 вечер: «в зависимости от того, как перемещаемся по уровням, остаётся миссия "приехать домой" — первая миссия Гомера, где показывают дом Симпсонов; её не
// переставить, даже если поменять миссию»). Правило: выбранная/активная миссия главнее стартовой цели. Три места, где это ломалось:
//  1) смена миссии выбором (MISSION SELECT, служебное msnsel) снимала цель только событием CLEANUP — маркер цели и реестр прежней миссии жили до следующего этапа
//     (msn_abort чистил, выбор — нет); теперь одна дорога mvSwitchTo: цель HUD, маркер, реестр — снять, потом старт;
//  2) ◄ ► на экране MISSION SELECT оставляли курсор на той же строке, а не на текущей миссии выбранного уровня: ✕ сразу после перелистывания стартовал строку 0 —
//     у L1 это m1 «S-M-R-T» с облётом дома Симпсонов, а текущей была m7 (курсор теперь на текущую строку уровня);
//  3) смена уровня выбором строки сперва ставила стартовую цель уровня (msn_level_start: первая непройденная сюжетная — у L1 «домой»), и только потом выбранную:
//     двойной старт (спавн NPC, чтение моделей, перенос игрока) под газетой; теперь стартовая цель при выбранной строке не ставится (rt->defer_first).
// Прежнее — MSN_SWITCH_OLD / MS_CURSOR_OLD / LVDEFER_OLD (=1) или служебно flag msnswitchold / mscursorold / lvdeferold 1 (красный контроль tests/msnhome/accept60.sh)
#ifndef MS_CURSOR_OLD
#define MS_CURSOR_OLD 0
#endif
static int msCursorOld = MS_CURSOR_OLD;
void mvServiceSwitchOld(const char *what, int on) {
  if (!strcmp(what, "msnswitchold")) msnSwitchOld = on; else if (!strcmp(what, "mscursorold")) msCursorOld = on;
}
#ifndef MSHEAL_OLD
#define MSHEAL_OLD 0
#endif
static int msChkOff = MSHEAL_OLD; static void msWdFire(const char *info);
static void mvSwitchTo(int mi, const char *why) {   // смена миссии из меню / сценария: прежняя цель и её маркер уходят, новая стартует с начала вступления
  const char *from = M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions ? msn_str(&M.f, M.f.missions[M.rt.cur].id) : "-";
  if (!msnSwitchOld) msn_switch_clear(&M.rt);
  else { MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e); }   // прежнее: только CLEANUP
  bbNoteMsnEnd(why);
  logf_("MSNSWITCH why=%s from=%s to=%s marker=%d gmarker=%d reg=%d obj=\"%.40s\" talk=%d briefing=%d t=%.1f\n", why, from, msn_str(&M.f, M.f.missions[mi].id), H.markerOn,
        M.mg.marker_on, msn_reg_mission_count(&M.rt), M.mg.hud.objective ? M.mg.hud.objective : "", H.talk[0] != 0, H.briefing, clock_);
  if (!strcmp(why, "row") || !strcmp(why, "pause")) msn_enter_mission(&M.rt, mi);   // круг 7 (P06-5): выбор в MISSION SELECT — как вход после загрузки, вступление с этапа RESET_TO_HERE; Skip и служебные — с нулевого
  else msn_start_mission(&M.rt, mi, 0);
  if (!msChkOff && (!strcmp(why, "row") || !strcmp(why, "pause")) && M.rt.cur != mi) {   // правка 432: выбор стартует именно выбранную миссию; не она — старт заново с нулевого этапа
    char s_[96]; snprintf(s_, sizeof s_, "when=start want=%d got=%d fix=restart", mi, M.rt.cur); msWdFire(s_);
    msn_start_mission(&M.rt, mi, 0);
  }
}
static int msnSelStatus(int i) {   // 0 закрыта, 1 текущая, 2 пройдена или пропущена (выбрать можно)
  if (i < 0) return 0;
  if (M.rt.completed >> i & 1u) return 2;
  if (!SV_SKIP_OLD && i < 32 && (msSkipped >> i & 1u)) return 2;   // сверка g07b P13-6: пропущенная открыта для выбора
  return i == M.rt.story ? 1 : 0;
}
static int msCur(void);
// сверка g07b P13-6: значок строки (поле UI_F_MS0+k): 2 галка — пройдена; 1 крест — были попытки и не пропущена; 0 черта — попыток нет или пропущена. Строки другого уровня — как были
static int msRowIcon(int k, int st) {
  if (SV_SKIP_OLD || st <= 0) return st;
  if (!msOld && msLv != msCur()) return st;
  int i = selList[k]; if (i < 0 || i >= 32) return st;
  if (M.rt.completed >> i & 1u) return 2;
  return mvRowIcon(0, msTries[i], (msSkipped >> i & 1u) != 0);
}
// сверка g07b P13-3: Skip на экране провала. Номер сюжетной миссии (с 1) на уровне; L7 5-я, 6-я, 7-я — без пропуска; порог 7 учтённых (сбросы / 2)
static int msStoryOrd(int mi) { int o = 0; for (int i = 0; i <= mi && i < (int)M.f.nmissions; i++) { if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue; o++; } return o; }
int mvFailSkipOk(void) {
  if (SV_SKIP_OLD || !active || !H.failed || M.rt.cur < 0 || M.rt.cur >= 32) return 0;
  if (M.f.missions[M.rt.cur].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) return 0;
  return mvSkipAllowed(msTries[M.rt.cur], M.f.hdr ? (int)M.f.hdr->level : 1, msStoryOrd(M.rt.cur), SV_SKIP7_OLD);   // круг 6 (P13-3): счёт у нас в запусках, делить на 2 не нужно
}
void mvFailSkip(void) {   // миссия прерывается, включается следующая сюжетная, прежняя помечена «пропущена»
  if (!active || M.rt.cur < 0) return;
  int cur = M.rt.cur, nx = -1;
  if (cur < 32) msSkipped |= 1u << cur;
  for (int i = cur + 1; i < (int)M.f.nmissions; i++) if (!(M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION))) { nx = i; break; }
  H.failed = 0; failStoreOff(); uiHudShow(UI_T_MISSION_FAILED, 0);
  logf_("MSN skip %s -> %d tries=%d t=%.1f\n", msn_str(&M.f, M.f.missions[cur].id), nx, cur < 32 ? msTries[cur] : 0, clock_);
  if (nx >= 0) mvSwitchTo(nx, "skip"); else gameMsnAbort(&M);
}
static int msCur(void) { return M.f.hdr ? (int)M.f.hdr->level : 1; }
static int msRowStatus(int k) {   // строка k экрана: 0 закрыта, 1 текущая, 2 пройдена
  if (k < 0 || k >= 7) return 0;
  if (msOld || msLv == msCur()) return msnSelStatus(selList[k]);
  // облако 05.10 (карьера): пройденный уровень обычной игры — строки по записи прогресса уровня (пройдена / первая непройденная —
  // текущая / дальше закрыта); всё пройдено — все строки «пройдена» (выбор — повтор миссии)
  unsigned r = msRows && !msDev ? msRows(msLv) : 0;
  if (r & 0x80u) { if (r >> k & 1u) return 2; for (int j = 0; j < k; j++) if (!(r >> j & 1u)) return 0; return 1; }
  return k < 6 ? 2 : 1;   // чужой уровень в DEV (или строки неизвестны): сюжетные, кроме последней, пройдены
}
void mvMissionSelectRows(unsigned (*levelRows)(int)) { msRows = levelRows; }
unsigned mvMissionRows(void) {
  if (!active) return 0;
  int story[32], ns = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) {
    if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
    if (ns < 32) story[ns++] = (int)i;
  }
  if (!ns) return 0;
  unsigned r = 0x80u;
  for (int k = 0; k < 7; k++) { int i = ns - 7 + k; if (i >= 0 && (M.rt.completed >> story[i] & 1u)) r |= 1u << k; }
  return r;
}
static void msRefresh(void) {
  for (int k = 0; k < 7; k++) { int st = msRowStatus(k); uiHudSet(UI_F_MS0 + k, msRowIcon(k, st)); uiScrEnable(UI_PX_MISSIONSELECT, k, st > 0); }
  uiHudSet(UI_F_MS_LEVEL, (msOld ? msCur() : msLv) - 1);
}
static int msLevelOk(int n) { return n == msCur() || (!msOld && msOpen && n >= 1 && n <= 7 && msOpen(n)); }
void mvMissionSelectSetup(int (*levelOpen)(int), int dev) { msOpen = levelOpen; msDev = dev; }
int mvMissionSelectWant(int *level, int *row) {
  if (msWantRow < 0) return 0;
  *level = msWantLv; *row = msWantRow; msWantRow = -1; return 1;
}
int mvMissionSelectOld(void) { return msOld; }
int mvMissionSelectStartRow(int row) {   // правка 348: после смены уровня — миссия строки row экрана (7 последних сюжетных уровня)
  if (!active || row < 0 || row >= 7) return 0;
  int story[32], ns = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) {
    if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
    if (ns < 32) story[ns++] = (int)i;
  }
  int mi = ns - 7 + row >= 0 ? story[ns - 7 + row] : -1;
  if (mi < 0) return 0;
  logf_("MSN select %s level=%d row=%d t=%.1f\n", msn_str(&M.f, M.f.missions[mi].id), msCur(), row, clock_);
  mvSwitchTo(mi, "row");   // старт уровня уходит (№943: цель, маркер и реестр — тоже)
  return 1;
}
int mvMissionSelectOn(void) { return selOn; }
int mvMissionSelectAvail(int rows) {   // пункт ДОБИВКИ 26: есть ли что выбрать в MISSION SELECT (иначе пункт паузы серый, menu_access.h);
                                      // rows — список откроется строками MissionSelect.scr (как selRows ниже), 0 — запасной список
  if (!active) return 1;
  int story[32], ns = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) {
    if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
    if (ns < 32) story[ns++] = (int)i;
  }
  return menuAccessMissionSelect(story, ns, M.rt.completed, M.rt.story, rows);
}
// №1058 (Роб 27.10: «в Mission Select, влево-вправо по уровням, какой город — присвой управление L R»): L — предыдущий открытый уровень, R — следующий, как ◄ и ►: тот же порядок, пропуск закрытых, звук.
// ↑ ↓ по миссиям — как было. MSLR1058_OLD=1 / файл mselr1058old / flag mselr1058 0 — только ◄ ►
#ifndef MSLR1058_OLD
#define MSLR1058_OLD 0
#endif
static int msLR1058Old = MSLR1058_OLD;
void mvMsLR1058Set(int old) { msLR1058Old = old; }
#ifndef SV_MS_START_OLD
#define SV_MS_START_OLD 0
#endif
// ------------------------------------------------------------------ правка Роба 432 (06.11, L4 Мардж): MISSION SELECT сам себя проверяет и чинит
// Фото IMG_8201: после долгой погони меню поверх игры в запасном виде (текстовый список, 6 строк из 7, без значков, мир залит синим) — пакет pextra.shr не открылся (куча
// занята погоней, три повтора msPackOpen с очисткой пакета будки не хватало), а запасной список рисовался как будто так и надо. Теперь: (1) msPackOpen ступенями отдаёт
// место (main_frontend.h), (2) пока список запасной, главный цикл повторяет открытие пакета и пересобирает меню (mvMissionSelectNeedPack / mvMissionSelectPackReady),
// (3) каждый кадр открытого меню инварианты проверяются здесь и нарушенное чинится: пакет и режим строк согласованы, список строк = последние 7 сюжетных миссий уровня по
// текущему состоянию игры, включённость строк = статус (пройдена / текущая), курсор на выбираемой строке, при открытии — на текущей миссии, есть что выбрать.
// Каждое срабатывание: строка MSNSELCHK и «WATCHDOG msnsel» (общий сторож, CLAUDE.md п.12; сторож страхует игрока, корень чинится отдельно).
// Выключатели: MSHEAL_OLD=1 / служебно flag msnselchkoff 1 (проверка молчит — красный контроль); flag msnselbreak N портит состояние меню (отрицательный контроль)
#ifndef MSHEAL_OLD
#define MSHEAL_OLD 0
#endif
#if defined(__has_include)
#if __has_include("watchdog.h")
#include "watchdog.h"   // общий сторож (окно «Опора», ветка spawn-ground): wdFire(вид, актёр, строка)
#define MS_HAVE_WD 1
#endif
#endif
static int msBreak, msNeedPack, msChkN;
static void msWdFire(const char *info) {
  msChkN++;
#ifdef MS_HAVE_WD
  wdFire("msnsel", "menu", info);
#else
  logf_("WATCHDOG msnsel actor=menu t=%.1f %s\n", clock_, info);
#endif
  logf_("MSNSELCHK %s t=%.1f\n", info, clock_);
}
int mvMsnSelCount(void) { return msChkN; }
// Строка для гейта (правка 435, п.3 наряда 07.11): что показано в 7 строках (disp: 1 включена, 0 серая, - пусто) и что должно быть по маскам пройденных и текущей, посчитанным заново (exp)
static int msStoryList(int *story);
static void msRowsLine(void) {
  char d[8] = "-------", e[8] = "-------"; int story[32], ns = msStoryList(story);
  if (selRows) for (int k = 0; k < 7; k++) {
    int m = ns - 7 + k >= 0 ? story[ns - 7 + k] : -1;
    d[k] = m < 0 ? '-' : uiScrEnabled(UI_PX_MISSIONSELECT, k) ? '1' : '0';
    e[k] = m < 0 ? '-' : ((M.rt.completed >> m & 1u) || m == M.rt.story) ? '1' : '0'; }
  logf_("MSNSELROWS lv=%d rows=%d disp=%s exp=%s n=%d idx=%d t=%.1f\n", (int)msCur(), selRows, d, e, selN, selIdx, clock_);
}
static int msFbN;
int mvMsnSelFallbacks(void) { return msFbN; }   // открытий меню в запасном виде (бот меню, правка 435: сравнение с лестницей и без неё)
void mvMsnSelChkOff(int off) { msChkOff = off; }
void mvMsnSelBreak(int n) { msBreak = n; }
static int msStoryList(int *story) {   // сюжетные миссии уровня по порядку (не бонус, не без сценария, не переход)
  int ns = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) { if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue; if (ns < 32) story[ns++] = (int)i; }
  return ns;
}
static int msPackOk(void) { return uiScrReady() && uiScrRows(UI_PX_MISSIONSELECT) >= 7; }
static void msBuild(void);
static int mvMissionSelect2(int on, unsigned hit);
static void msBreakApply(void) {   // отрицательный контроль: что-то портит состояние открытого меню, один раз за открытие
  if (!msBreak || !selOn) return;
  int b = msBreak; msBreak = 0;
  if (b == 1 && selN > 2) selList[2] = (selList[2] + 1) % (int)M.f.nmissions;      // чужая миссия в строке
  else if (b == 2) selIdx = 99;                                                    // курсор вне списка
  else if (b == 3) { uiScrClose(); }                                               // пакет пропал под открытым меню
  else if (b == 4 && selRows) { for (int k = 0; k < 7; k++) uiScrEnable(UI_PX_MISSIONSELECT, k, 0); }   // все строки серые
  else if (b == 5 && selRows) { selN = 3; }                                        // список обрезан (как «6 строк из 7» на фото)
  logf_("MSNSELBREAK kind=%d t=%.1f\n", b, clock_);
}
// Проверка с починкой. when: "open" / "frame" / "accept". Возвращает число нарушений (0 — всё согласовано)
static int msnSelCheck(const char *when) {
  if (msChkOff || !active || !selOn) return 0;
  int bad = 0, story[32], ns = msStoryList(story);
  char info[176]; int n = snprintf(info, sizeof info, "when=%s lv=%d", when, (int)msCur());
#define MSNOTE(...) do { bad++; if (n < (int)sizeof info - 40) n += snprintf(info + n, sizeof info - n, __VA_ARGS__); } while (0)
  int packOk = msPackOk();
  if (selRows && !packOk) { MSNOTE(" pack=lost fix=fallback+reopen"); selRows = 0; msNeedPack = 1; msBuild(); }       // пакет ушёл из-под меню: запасной вид и повтор открытия
  else if (!selRows && packOk) { MSNOTE(" rows=0 pack=ready fix=rebuild"); msBuild(); }
  if (selOn && selRows) {
    int dif = 0, ex[7];
    for (int k = 0; k < 7; k++) { ex[k] = ns - 7 + k >= 0 ? story[ns - 7 + k] : -1; dif += selList[k] != ex[k]; }
    if (dif || selN != 7) { MSNOTE(" list=%d/7 n=%d fix=resync", dif, selN); for (int k = 0; k < 7; k++) selList[k] = ex[k]; selN = 7; }
    int de = 0; for (int k = 0; k < 7; k++) de += (uiScrEnabled(UI_PX_MISSIONSELECT, k) != 0) != (msRowStatus(k) > 0);
    if (de) { MSNOTE(" enable=%d/7 fix=refresh", de); msRefresh(); }
  } else if (selOn && !selRows) {   // запасной список: пройденные и текущая, по порядку
    int ex[16], en = 0; for (int i = 0; i < (int)M.f.nmissions && en < 16; i++) {
      if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
      if ((M.rt.completed >> i & 1u) || i == M.rt.story) ex[en++] = i; }
    int dif = en != selN; for (int k = 0; !dif && k < en; k++) dif = selList[k] != ex[k];
    if (dif) { MSNOTE(" fblist=%d/%d fix=resync", selN, en); for (int k = 0; k < en; k++) selList[k] = ex[k]; selN = en; if (selIdx >= selN) selIdx = 0; }
  }
  if (selOn) {   // курсор: в списке и на выбираемой строке
    int okc = selIdx >= 0 && selIdx < selN && (!selRows || msRowStatus(selIdx) > 0);
    if (!okc) {
      int to = -1; for (int k = 0; k < selN; k++) if (selList[k] >= 0 && selList[k] == M.rt.story && (!selRows || msRowStatus(k) > 0)) to = k;
      for (int k = 0; to < 0 && k < selN; k++) if (!selRows || msRowStatus(k) > 0) to = k;
      MSNOTE(" cursor=%d fix=%d", selIdx, to);
      if (to >= 0) selIdx = to; else { selOn = 0; uiPauseMenuHide(0); MSNOTE(" empty fix=close"); }
    } else if (!strcmp(when, "open") && selRows && msLv == msCur()) {   // при открытии курсор — на текущей миссии
      int cur = -1; for (int k = 0; k < 7; k++) if (selList[k] >= 0 && selList[k] == M.rt.story) cur = k;
      if (cur >= 0 && msRowStatus(cur) > 0 && selIdx != cur) { MSNOTE(" cursor=%d!=cur fix=%d", selIdx, cur); selIdx = cur; }
    }
  }
#undef MSNOTE
  if (bad) msWdFire(info);
  return bad;
}
// Независимый свидетель (не чинит, читает состояние заново; для ботов и гейтов): 0 — меню согласовано, иначе код нарушения и строка MSNSELBAD
int mvMsnSelOracle(void) {
  if (!active || !selOn) return 0;
  int code = 0, story[32], ns = 0;
  for (unsigned i = 0; i < M.f.nmissions; i++) { unsigned f = M.f.missions[i].flags; if (!(f & MSN_MF_BONUS) && !(f & MSN_MF_NOSCRIPT) && !(f & MSN_MF_TRANSITION) && ns < 32) story[ns++] = (int)i; }
  if (!selRows) {   // запасной вид (пакет не открылся; лестница памяти дошла до ступени 5): допустим, пока список верный и курсор на строке; повтор открытия ведёт главный цикл
    int ex[16], en = 0; for (int i = 0; i < (int)M.f.nmissions && en < 16; i++) {
      if (M.f.missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
      if ((M.rt.completed >> i & 1u) || i == M.rt.story) ex[en++] = i; }
    if (en != selN) code = 1; else { for (int k = 0; !code && k < en; k++) if (selList[k] != ex[k]) code = 1; }
    if (!code && (selIdx < 0 || selIdx >= selN)) code = 6;
  }
  else if (!(uiScrReady() && uiScrRows(UI_PX_MISSIONSELECT) >= 7)) code = 2;   // пакет не тот или закрыт
  else if (selN != 7) code = 3;
  else { for (int k = 0; k < 7 && !code; k++) { int e = ns - 7 + k >= 0 ? story[ns - 7 + k] : -1; if (selList[k] != e) code = 4;
           else if ((uiScrEnabled(UI_PX_MISSIONSELECT, k) != 0) != (msRowStatus(k) > 0)) code = 5; }
         if (!code && (selIdx < 0 || selIdx >= 7 || msRowStatus(selIdx) <= 0)) code = 6; }
  if (code) logf_("MSNSELBAD code=%d rows=%d n=%d idx=%d t=%.1f\n", code, selRows, selN, selIdx, clock_);
  return code;
}
void mvMsFuzzState(unsigned r) {   // служебное (бот меню, flag msnselfuzz): случайное состояние прохождения уровня — для проверки меню; игру дальше не ведёт
  if (!active) return;
  int story[32], ns = msStoryList(story); if (!ns) return;
  unsigned all = 0; for (int i = 0; i < ns; i++) all |= 1u << story[i];
  int kind = (int)(r % 8); r /= 8;
  if (kind == 0) { M.rt.completed = all; M.rt.story = story[ns - 1]; }                 // все пройдены, текущая — последняя
  else if (kind == 1) { M.rt.completed = all & ~(1u << story[ns - 1]); M.rt.story = story[ns - 1]; }   // все кроме последней
  else if (kind == 2) { M.rt.completed = all; M.rt.story = -1; }                      // все пройдены, текущей нет
  else if (kind == 3) { M.rt.completed = 0; M.rt.story = story[0]; }                 // самое начало
  else { M.rt.completed = (r >> 3) & all; M.rt.story = story[(r >> 12) % (unsigned)ns]; }   // случайный набор
}
int mvMissionSelectNeedPack(void) { return active && selOn && !selRows; }
void mvMissionSelectPackReady(void) { if (active && selOn && !selRows && msPackOk()) { msBuild(); logf_("MSNSELHEAL rebuilt rows=%d t=%.1f\n", selRows, clock_); } }
int mvMissionSelect(int on, unsigned hit) {
  if (!active) return 0;
  if (selOn) { msBreakApply(); msnSelCheck("frame"); }
  if (on && !selOn) { msBuild(); if (selOn && !selRows) msFbN++; if (selOn) { msnSelCheck("open"); msRowsLine(); } return selOn; }
  return mvMissionSelect2(on, hit);
}
static void msBuild(void) {   // собрать список и вид меню из ТЕКУЩЕГО состояния игры (при каждом открытии и при починке)
  selOn = 0;
  mvMissionSelect2(1, 0);
}
static int mvMissionSelect2(int on, unsigned hit) {
  if (!active) return 0;
  if (on && !selOn) {   // открыть: пройденные и текущая сюжетная, в порядке расписания (как в оригинале)
    selN = 0; selRows = uiScrReady() && uiScrRows(UI_PX_MISSIONSELECT) >= 7; msLv = msCur(); msArrL = msArrR = 0; msWantRow = -1;
    int story[32], ns = 0;
    for (unsigned i = 0; i < M.f.nmissions; i++) {
      const MsnMission *ms = &M.f.missions[i];
      if (ms->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;   // конец уровня (ролик + таймер) — не миссия списка
      if (ns < 32) story[ns++] = (int)i;
      if (!selRows && selN < 16 && ((M.rt.completed >> i & 1u) || (int)i == M.rt.story)) selList[selN++] = (int)i;
    }
    if (selRows) {
      int any = 0; selN = 7;
      for (int k = 0; k < 7; k++) { selList[k] = ns - 7 + k >= 0 ? story[ns - 7 + k] : -1; any |= msnSelStatus(selList[k]) > 0; }
      msRefresh();
      selOn = any; selIdx = 0;
      for (int k = 0; k < 7; k++) if (selList[k] >= 0 && selList[k] == M.rt.story) selIdx = k;
      if (!msnSelStatus(selList[selIdx])) for (int k = 0; k < 7; k++) if (msnSelStatus(selList[k])) { selIdx = k; break; }
    } else {
      selOn = selN > 0; selIdx = 0;
      for (int k = 0; k < selN; k++) if (selList[k] == M.rt.story) selIdx = k;
    }
    uiPauseMenuHide(selOn);
    { int nl = 0; for (int n = 1; n <= 7; n++) nl += selRows && msLevelOk(n);
      logf_("MSNSEL open rows=%d n=%d sel=%d level=%d levels=%d dev=%d old=%d t=%.1f\n", selRows, selN, selIdx, msLv, nl, msDev, msOld, clock_); }
    return selOn;
  }
  if (!selOn) return 0;
  int was = selIdx;
  unsigned lvL = PSP_CTRL_LEFT | (msLR1058Old ? 0u : PSP_CTRL_LTRIGGER), lvR = PSP_CTRL_RIGHT | (msLR1058Old ? 0u : PSP_CTRL_RTRIGGER);
  if (selRows && !msOld && (hit & (lvL | lvR))) {   // правка 348: ◄ ► (№1058: и L R) — открытый уровень (по кругу 1..7)
    int d = (hit & lvL) ? -1 : 1, to = msLv;
    for (int k = 1; k < 7; k++) { int n = (msLv - 1 + d * k + 70) % 7 + 1; if (msLevelOk(n)) { to = n; break; } }
    int moved = to != msLv;
    if (moved) {
      msLv = to; msRefresh(); if (d < 0) msArrL = MS_ARROW_FR; else msArrR = MS_ARROW_FR;
      if (!msCursorOld) {   // №943: курсор — на текущую миссию выбранного уровня (иначе последнюю пройденную), не на строку 0 по инерции
        int cur = -1, last = -1; for (int k = 0; k < 7; k++) { int st = msRowStatus(k); if (st == 1) cur = k; if (st > 0) last = k; }
        selIdx = cur >= 0 ? cur : last >= 0 ? last : selIdx;
      } else
      if (msRowStatus(selIdx) <= 0) for (int k = 0; k < 7; k++) if (msRowStatus(k) > 0) { selIdx = k; break; }
      uiPopHookFire(UI_EV_SCROLL);
    } else uiPopHookFire(UI_EV_LOCKED);   // других открытых уровней нет
    { int top = -1; for (int k = 0; k < 7; k++) if (msRowStatus(k) > 0) top = k;   // №943: верхняя открытая строка уровня = его текущая миссия (дальше — закрытые)
      logf_("MSNSEL level=%d dir=%d moved=%d sel=%d st=%d top=%d t=%.1f\n", msLv, d, moved, selIdx, msRowStatus(selIdx), top, clock_); }
  }
  for (int d = (hit & PSP_CTRL_UP) ? -1 : (hit & PSP_CTRL_DOWN) ? 1 : 0, k = 0; d && k < selN; k++) {   // закрытые строки пропускаются
    if (uiPadRepeated && (selIdx + d < 0 || selIdx + d >= selN)) { selIdx = was; break; }   // правка 202: повтор у края встаёт
    selIdx = (selIdx + d + selN) % selN;
    if (!selRows || msRowStatus(selIdx)) break;
  }
  if (selIdx != was && (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN))) uiPopHookFire(UI_EV_SCROLL);
  // сверка g07b-msnview (P13-7): START на подэкранах паузы выходит из паузы в игру целиком (возврат 4 — main_loop.h закрывает пакет и паузу); назад в меню паузы — ○ и △. SV_MS_START_OLD=1 — START как ○ (прежнее)
  if (!SV_MS_START_OLD && (hit & PSP_CTRL_START) && !(hit & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE))) { selOn = 0; uiPauseMenuHide(0); uiPopHookFire(UI_EV_BACK); return 4; }
  if (hit & (PSP_CTRL_CIRCLE | PSP_CTRL_START | PSP_CTRL_TRIANGLE)) { selOn = 0; uiPauseMenuHide(0); uiPopHookFire(UI_EV_BACK); return 1; }   // назад в паузу
  if ((hit & PSP_CTRL_CROSS) && selRows && !msOld && msLv != msCur() && msRowStatus(selIdx) > 0) {   // правка 348: миссия другого уровня
    uiPopHookFire(UI_EV_ACCEPT);
    selOn = 0; uiPauseMenuHide(0); msWantLv = msLv; msWantRow = selIdx;
    logf_("MSNSEL want level=%d row=%d from=%d t=%.1f\n", msLv, selIdx, msCur(), clock_);
    return 3;
  }
  if ((hit & PSP_CTRL_CROSS) && selList[selIdx] >= 0 && (!selRows || msnSelStatus(selList[selIdx]))) {
    uiPopHookFire(UI_EV_ACCEPT);
    int mi = selList[selIdx]; selOn = 0; uiPauseMenuHide(0);
    logf_("MSN select %s t=%.1f\n", msn_str(&M.f, M.f.missions[mi].id), clock_);
    mvSwitchTo(mi, "pause");   // прошлая миссия уходит (№943: цель, маркер и реестр — тоже)
    return 2;
  }
  return 1;
}
void mvDrawMissionSelect(void) {   // в рамке паузы Scrooby вместо её пунктов (uiPauseMenuHide), шрифт пакета
  uiPauseMenuHide(selOn);
  if (!selOn) return;
  if (selRows && uiScrReady()) {   // экран MissionSelect.scr (pextra.shr)
    uiHudSet(UI_F_MS_LARROW, msArrL > 0); uiHudSet(UI_F_MS_RARROW, msArrR > 0);   // правка 348: шарик нажатой стрелки — красный
    if (msArrL > 0) msArrL--; if (msArrR > 0) msArrR--;
    uiScrDrawMenu(UI_PX_MISSIONSELECT, selIdx, 0.f); return; }
  // грани как на странице оригинала MissionSelect.pag: заголовок boulder_16, миссии Tt2001m__14 (с мягким шагом для PSP)
  int head = uiFontFind("boulder_16"), item = uiFontFind("Tt2001m__14");
  centered(head, 72, 0xffffffffu, uiMenuLabel(UI_MENU_PAUSE, UI_P_MISSION_SELECT));   // строка пункта паузы Scrooby
  for (int k = 0; k < selN; k++) {
    const MsnMission *ms = &M.f.missions[selList[k]];
    char s[96]; snprintf(s, sizeof s, "%d. %s", selList[k] + 1, msn_str(&M.f, ms->title));
    centered(item, 96 + k * 14, k == selIdx ? 0xff14e6ffu : 0xffffffffu, s);
  }
}

// ------------------------------------------------------------------ служебное: проверки HUD (гейт tests/missions/emu_hud.sh)
static int fitLogField(const char *what, int field, const char *t) {
  int lines = 0, ml = 0, w = 0, bw = 0, ok = uiTextFitField(UI_SCREEN_MISSIONOVER, field, t, &lines, &ml, &w, &bw);
  if (!ok) logf_("UIFIT over %s lines=%d/%d w=%d/%d \"%.60s\"\n", what, lines, ml, w, bw, t);
  return ok;
}
static int fitLog(const char *what, const char *t) {
  int lines = 0, ml = 0, w = 0, bw = 0, ok = uiTextFit(t, &lines, &ml, &w, &bw);
  int gw = 0, gh = 0; uiLastBubbleGrow(&gw, &gh);   // №619: рост рамки под текст
  if (!ok || gw || gh || !strcmp(what, "plant")) logf_("UIFIT %s %s lines=%d/%d w=%d/%d grow=+%dw+%dh \"%.60s\"\n", ok ? "ok" : "over", what, lines, ml, w, bw, gw, gh, t);
  return ok;
}
static int mvTalkPromptForce1322;   // служебно (flag uitalkprompt1322 N): подпись разговора принудительно (1 SKIP, 2 Start/Cancel), стенд
void mvServiceUi(const char *flag, float a) {
  if (!strcmp(flag, "uistack")) { uiOverlayStack(a != 0.f); logf_("UISTACK %d\n", a != 0.f); return; }
  // правка 22: рамки аватара и пузыря задания, как ушли в GE (tests/ui/avatar_check.py); uiavatarold — прежний размер (красный)
  if (!strcmp(flag, "uilevelendold")) { levelEndOld = 1; return; }
  if (!strcmp(flag, "uimsselold")) { msOld = 1; uiScrPauseLeakOld(1); logf_("MSNSEL old (красный контроль 348)\n"); return; }   // правка 348
  if (!strcmp(flag, "uimsndone")) { if (active) M.rt.completed |= (uint32_t)a; logf_("MSN completed-set 0x%x\n", (unsigned)M.rt.completed); return; }   // гейт MISSION SELECT   // отрицательный контроль: конец уровня — прежняя вспышка 3 с
  if (!strcmp(flag, "uiavatar") || !strcmp(flag, "uiavatarold")) { uiAvatarOld(flag[8] == 'o'); uiTrace(UI_TRACE_AVATAR); return; }
  // правка 48: сообщение без значка (как «GO!» отсчёта) — текст внутри пузыря (UIOBJTEXT); uimsgnoiconold — прежний код (красный)
  if (!strcmp(flag, "uibatchold")) { uiBatchOld(1); logf_("UIBATCH old\n"); return; }   // наряд 11.10: красный контроль «кадр побитно тот же»
  if (!strcmp(flag, "wrap1037")) { uiWrap1037Set((int)a); logf_("WRAP1037 on=%d\n", (int)a); return; }   // №1037
  if (!strcmp(flag, "uiobjwidowold")) { uiObjWidowOld(1); logf_("UIOBJWIDOW old\n"); return; }   // правка 215: красный контроль
  if (!strcmp(flag, "uimsgnoicon") || !strcmp(flag, "uimsgnoiconold")) { uiObjTextOld(flag[11] == 'o'); uiTrace(UI_TRACE_AVATAR);
    uiMessage("GO TO THE KWIK-E-MART", -1, 2.f); return; }
  if (!strcmp(flag, "uitalkprompt1322")) { mvTalkPromptForce1322 = (int)a; logf_("TALKPROMPT1322 force=%d\n", mvTalkPromptForce1322); return; }
  if (!strcmp(flag, "uitalkoff")) { M.talk.off = 1; logf_("TALKOFF (отрицательный контроль: разговор не ставится)\n"); return; }   // правки 26, 38
  if (!strcmp(flag, "uipulsetrace")) { uiTrace(UI_TRACE_PULSE); return; }   // правка 41: рамка △ покадрово, без служебного показа
  if (!strcmp(flag, "uinovoice")) { abNoVoice = 1; logf_("VOICE off (отрицательный контроль)\n"); return; }
  if (!strcmp(flag, "uifinmark1317")) { for (uint32_t i = 0; i < M.f.nlocs; i++) if (!strcmp(msn_str(&M.f, M.f.locs[i].name), "m4_finish")) {   // 1317 служебно: поставить метку finish_line на локатор m4_finish (стенд снимка до/после)
      memset(&H.marker, 0, sizeof H.marker); H.marker.drawable = "finish_line"; H.marker.loc = &M.f.locs[i]; H.marker.pos.x = M.f.locs[i].pos[0]; H.marker.pos.y = M.f.locs[i].pos[1]; H.marker.pos.z = M.f.locs[i].pos[2]; H.markerOn = 1; logf_("FINMARK1317 set heading=%.3f flags=%u\n", M.f.locs[i].heading, M.f.locs[i].flags); return; } logf_("FINMARK1317 not found nlocs=%u active=%d\n", (unsigned)M.f.nlocs, active); return; }
  if (!strcmp(flag, "uiproxdemo1318")) { int lose_ = a >= 100.f; float d_ = (lose_ ? a - 100.f : a) / 100.f;   /* a: 0..99 — «не отставай» (зелёная), 100..199 — «оторвись» (красная), расстояние = (a mod 100)% предела (отрицательное smoke.h режет до нуля) */   // 1318 служебно: шкала близости как у «не отставай» (a<100) / «оторвись» (a>=100) при расстоянии (a mod 100)% от предела: заполнение и цвет, как в игре
    float fill_ = mvProxBar1318Old ? d_ : 1.f - d_; if (fill_ > 1.f) fill_ = 1.f; if (fill_ < 0.f) fill_ = 0.f;
    uiHudSet(UI_F_PROXIMITY, (int)(fill_ * 1000.f)); uiProxTint(mvProxBar1318Old ? 0u : lose_ ? 0xff0000ffu : 0xff00ff00u); uiOvForce1308 = 2048u; logf_("PROXDEMO1318 d=%.2f fill=%d lose=%d old=%d\n", d_, (int)(fill_ * 1000.f), lose_, mvProxBar1318Old); return; }
  if (!strcmp(flag, "uiovforce1308")) { uiOvForce1308 = (unsigned)a; uiHudSet(UI_F_TIMER, 90000); uiHudSet(UI_F_PROXIMITY, 400); uiHudSet(UI_F_DAMAGE, 300); logf_("OVFORCE1308 mask=%u\n", uiOvForce1308); return; }   // 1308: оверлеи по маске (таймер + шкала близости/урона) для гейта
  if (!strcmp(flag, "uiov")) {   // рамки видимых оверлеев последнего кадра HUD
    int tags[8], r[8][4], n = uiOverlayBoxes(tags, r, 8);
    for (int i = 0; i < n; i++) logf_("UIOV tag=%d %d %d %d %d\n", tags[i], r[i][0], r[i][1], r[i][2], r[i][3]);
    logf_("UIOV n=%d\n", n); return;
  }
  if (!strcmp(flag, "uimsglong")) {   // служебное (правка 282): N-й по длине текст задания уровня в нижнем пузыре (со значком «simpsons»)
    int want = (int)a, used[64] = {0}; const char *pick = 0;
    for (int k = 0; k <= want && k < 64; k++) { int bi = -1; size_t bl = 0;
      for (unsigned i = 0; i < M.f.nstages && i < 64; i++) { const char *x = msn_str(&M.f, M.f.stages[i].text); size_t l = strlen(x); int dup = 0;
        for (unsigned j = 0; j < i && j < 64; j++) if (used[j] && !strcmp(msn_str(&M.f, M.f.stages[j].text), x)) dup = 1;
        if (!used[i] && !dup && l > bl) { bl = l; bi = (int)i; } }
      if (bi < 0) break; used[bi] = 1; pick = msn_str(&M.f, M.f.stages[bi].text); }
    if (pick) { uiMessage(pick, UI_OBJICON_EXTERNAL, 0.f);   // значок — нынешний значок задания
                logf_("UIMSGLONG %d \"%s\"\n", want, pick); }
    return; }
  if (!strcmp(flag, "uifit") || !strcmp(flag, "uifitplant")) {   // все тексты заданий уровня и провалы (причина + подсказка)
    int all = 0, bad = 0; char t[400];
    for (unsigned i = 0; i < M.f.nstages; i++) { const char *x = msn_str(&M.f, M.f.stages[i].text); if (*x) { all++; bad += !fitLog("stage", x); } }
    for (int r = 1; r < 9; r++) {   // провал: причина и подсказки — в свои строки экрана MissionOver
      all++; bad += !fitLogField("fail", UI_F_MSN_FAIL, msn_fail_text(&M.f, r));
      for (int h = 0; h < 8; h++) { const char *hint = msn_fail_hint(&M.f, r, h); if (!*hint) break; all++; bad += !fitLogField("hint", UI_F_MSN_HINT, hint); }
    }
    for (unsigned i = 0; i < M.f.nmissions; i++) {   // брифинг: название и описание каждой миссии уровня
      int lines = 0, ml = 0, w = 0, bw = 0; const MsnMission *ms = &M.f.missions[i];
      const char *ti = msn_str(&M.f, ms->title), *in = msn_str(&M.f, ms->info);
      if (*ti) { all++; if (!uiTextFitField(UI_SCREEN_MISSIONLOAD, UI_F_MSN_TITLE, ti, &lines, &ml, &w, &bw)) { bad++; logf_("UIFIT over title lines=%d/%d w=%d/%d \"%.60s\"\n", lines, ml, w, bw, ti); } }
      if (*in) { all++; if (!uiTextFitField(UI_SCREEN_MISSIONLOAD, UI_F_MSN_INFO, in, &lines, &ml, &w, &bw)) { bad++; logf_("UIFIT over info lines=%d/%d w=%d/%d \"%.60s\"\n", lines, ml, w, bw, in); } }
    }
    if (!strcmp(flag, "uifitplant")) {   // отрицательный контроль: самый длинный текст уровня трижды обязан не влезть
      const char *lx = ""; for (unsigned i = 0; i < M.f.nstages; i++) { const char *x = msn_str(&M.f, M.f.stages[i].text); if (strlen(x) > strlen(lx)) lx = x; }
      snprintf(t, sizeof t, "%s %s %s", lx, lx, lx); all++; bad += !fitLog("plant", t);
    }
    logf_("UIFIT total=%d over=%d\n", all, bad); return;
  }
}

// ------------------------------------------------------------------ служебное: миссия по номеру (как MISSION SELECT / разговор с бонус-персонажем)
void mvServiceSellers(void) {   // служебное (правка 288, flag sellerlog): продавцы — где место разговора и где сам персонаж
  if (!active) return;
  for (int i = 0; i < M.shop.nseller; i++) { const GameNpc *n = gameMsnNpc(&M, M.shop.seller[i].npc);
    logf_("SELLER %s at=%.2f,%.2f,%.2f npc=%s%s t=%.1f\n", M.shop.seller[i].npc, M.shop.seller[i].x, M.shop.seller[i].y, M.shop.seller[i].z,
          n ? "" : "-", n ? "" : "", clock_);
    if (n) logf_("SELLERNPC %s pos=%.2f,%.2f,%.2f walking=%d wp=%d/%d\n", n->name, n->pos.x, n->pos.y, n->pos.z, n->walking, n->iwp, n->nwp); }
  for (int i = 0; i < M.nnpc; i++) if (M.npc[i].bonus > 0)   // правка 289: бонусные персонажи (значок над головой)
    logf_("BONUSNPC %s mission=%d pos=%.2f,%.2f,%.2f walking=%d free=%d\n", M.npc[i].name, M.npc[i].bonus - 1, M.npc[i].pos.x, M.npc[i].pos.y, M.npc[i].pos.z,
          M.npc[i].walking, msn_free_roam(&M.rt));
}
void mvServiceMain(int mi) {   // служебное (гейт tests/aicar/emu.sh): основная часть миссии N с её чекпоинта, как повтор после провала
  if (!active || mi < 0 || (unsigned)mi >= M.f.nmissions || M.f.missions[mi].main == MSN_NONE16) return;
  logf_("MSN main %s t=%.1f\n", msn_str(&M.f, M.f.missions[mi].id), clock_);
  MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e);
  M.rt.cp_phase = 1; M.rt.cp_stage = 0; msn_start_mission(&M.rt, mi, 1);
}
int mvStageKey(void) { return active && M.rt.status == MSN_RUNNING ? (M.rt.cur + 1) * 10000 + M.rt.phase * 100 + M.rt.stage : -1; }   // служебное: этап сменился?
int mvServicePass(const char *why) {   // служебное (06.11): засчитать текущий этап, как выполненная цель — проверка переходов, реплик, наград и конца миссии
  if (!active || M.rt.status != MSN_RUNNING) return 0;
  logf_("MSN pass %s %d.%d obj=%s why=%s t=%.1f\n", msn_str(&M.f, M.f.missions[M.rt.cur].id), M.rt.phase, M.rt.stage, msn_objective_name[M.rt.obj], why ? why : "-", clock_);
  return msn_service_pass(&M.rt);
}
// 1348 (служебное, гейт): засчитать этап и удержать окно между этапами BETWEEN sec секунд мира, независимо от того, где игрок и как быстро грузится следующий этап (flag msnpasshold SEC)
int mvServicePassHold(float sec) { int r = mvServicePass("hold"); if (active && M.rt.status == MSN_BETWEEN && M.rt.delay < sec) M.rt.delay = sec; return r; }
void mvServiceMainAt(int code) {   // служебное (правка 302): основная часть миссии code/100 с этапа code%100 (как чекпоинт этапа)
  int ph0 = code < 0; if (ph0) code = -code;   // отрицательный код — вступительная часть (фаза 0)
  int mi = code / 100, st = code % 100;
  if (!active || mi < 0 || (unsigned)mi >= M.f.nmissions || M.f.missions[mi].main == MSN_NONE16) return;
  logf_("MSN mainat %s stage=%d t=%.1f\n", msn_str(&M.f, M.f.missions[mi].id), st, clock_);
  MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e);
  M.rt.cp_phase = ph0 ? 0 : 1; M.rt.cp_stage = st; msn_start_mission(&M.rt, mi, 1);
  if (ph0) M.rt.story = mi;   // прогулка — у сюжетной миссии (как в игре; правка 330: SAVE GAME проверяет story)
}
// 1320: чистая перезагрузка уровня (будка, дверь) посреди основной части миссии возвращала миссию в начало (BOOTH1060 msn same=0: было phase=1 stage=1, стало phase=0 stage=0). Теперь перед выгрузкой
// запоминается точка (миссия, фаза, этап), после загрузки миссия стартует с этого этапа, как чекпоинт после провала. ROOMRELOAD1320_OLD=1 — как было
int mvResumePoint1320(int *cur, int *ph, int *st) { if (!mvInMission()) return 0; *cur = M.rt.cur; *ph = M.rt.phase; *st = M.rt.stage; return 1; }
void mvResumeAt1320(int cur, int ph, int st) {
  if (!active || cur < 0 || (unsigned)cur >= M.f.nmissions || M.f.missions[cur].main == MSN_NONE16) return;
  logf_("MSN resume1320 %s phase=%d stage=%d t=%.1f\n", msn_str(&M.f, M.f.missions[cur].id), ph, st, clock_);
  MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e);
  M.rt.cp_phase = ph; M.rt.cp_stage = st; msn_start_mission(&M.rt, cur, 1);
}
void mvServiceUfo(void) { logf_("MSNUFO on=%d pos=%.1f,%.1f,%.1f charge=%.1f sent=%d old=%d ship=%d beam=%d drawn=%u t=%.1f\n", active ? M.ufoOn : 0, M.ufoPos[0], M.ufoPos[1], M.ufoPos[2], M.ufoCharge, M.ufoSent, gameBoss967Old, msnartHas("spaceship"), msnartHas("UfoBeam"), ufoDrawn, clock_); }   // №967: состояние тарелки для гейта tests/missions/boss_l7.sh
void mvServiceHud(void) {   // №943: что миссия считает целью сейчас (stage — этап рантайма), что держит HUD (obj — текст цели) и маркер; строки сверяет tests/msnhome/check.py
  { int ic, px; const char *t, *l; uiObjDebug(&ic, &px, &t, &l); logf_("MSNICON want=\"%s\" field=%d pix=%d pixok=%d home=%d msg=\"%.30s\" last=\"%.30s\" active=%d t=%.1f\n", msnartWantIcon(), ic, px, msnartIconPixOk(), uiHomeShown(), t, l, active, clock_); }   // №958б
  if (active) logf_("MSNTIMER on=%d up=%d rem=%.2f status=%d hold=%d done=%d coins=%d t=%.1f\n", M.rt.timer_on, M.rt.timer_up, M.rt.remaining, M.rt.status, M.rt.scene_busy, M.rt.hold_done, shopCoins(&M.shop), clock_);   // 1324: остаток таймера (красный контроль ожидания мира)
  if (!active) { logf_("MSNHUD active=0\n"); return; }
  int cur = M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions;
  logf_("MSNHUD cur=%s story=%s phase=%d stage=%d status=%d stagetext=\"%.40s\" obj=\"%.40s\" marker=%d gmarker=%d mpos=%.1f,%.1f briefing=%d t=%.1f\n",
        cur ? msn_str(&M.f, M.f.missions[M.rt.cur].id) : "-",
        M.rt.story >= 0 && (unsigned)M.rt.story < M.f.nmissions ? msn_str(&M.f, M.f.missions[M.rt.story].id) : "-", M.rt.phase, M.rt.stage, M.rt.status,
        cur && M.rt.st ? msn_str(&M.f, M.rt.st->text) : "", M.mg.hud.objective ? M.mg.hud.objective : "", H.markerOn, M.mg.marker_on,
        H.markerOn ? H.marker.pos.x : 0.f, H.markerOn ? H.marker.pos.z : 0.f, H.briefing, clock_);
}
void mvStartFirstIfIdle(void) {   // №943: стартовая цель уровня была отложена под выбранную строку, а строка не стартовала — миссия уровня всё же нужна
  if (!active || M.rt.cur >= 0 || M.rt.status == MSN_LEVEL_DONE) return;
  logf_("MSN start-first (строка не стартовала: стартует отложенная первая непройденная) t=%.1f\n", clock_);
  msn_start_first(&M.rt);
}
void mvServiceSelect(int mi) {
  if (!active || mi < 0 || (unsigned)mi >= M.f.nmissions) return;
  const MsnMission *ms = &M.f.missions[mi];
  logf_("MSN select %s t=%.1f\n", msn_str(&M.f, ms->id), clock_);
  if (ms->flags & MSN_MF_BONUS) { if (!msn_start_bonus(&M.rt, mi)) logf_("MSN select-refused %s\n", msn_str(&M.f, ms->id)); return; }
  mvSwitchTo(mi, "service");
}

// ------------------------------------------------------------------ правка Роба 40: камеры из данных миссий
int mvStartCam(const char **phase, const char **cam) { return active && gameMsnStartCam(&M, phase, cam); }

static float modelHead(const SkinModel *m) {   // лицо над ногами: сустав Head в позе привязки + 0.08 м (голова 0.16 м)
  int j = m ? skinFindJoint(m, "Head") : -1;
  if (j < 0) return m ? .72f * m->h->bbox[4] : 0.f;
  const float *v = m->j[j].inv;   // модель -> сустав: 3 столбца поворота и перенос; начало сустава в модели = -R^T t
  return -(v[3] * v[9] + v[4] * v[10] + v[5] * v[11]) + .08f;
}
static struct { int on, line, pc, shot, side, fromData, plant; float prev[3]; ConvScene s; } TC;
void mvTalkCamPlant(int how) { TC.plant = how; }
static int talkScene(ConvScene *s, float *pcH, float *npcH) {
  if (!active || !M.talk.on || !M.talk.npc[0] || !M.g || !*M.g->pHaveHomer) return 0;
  const GameNpc *n = gameMsnNpc(&M, M.talk.npc); if (!n) return 0;
  const Homer *H = M.g->pHomer; NpcAnim *a = animFor(npcKey(n), 0, 0);
  memset(s, 0, sizeof *s);
  s->pc[0] = H->x; s->pc[1] = H->y; s->pc[2] = H->z; s->npc[0] = n->pos.x; s->npc[1] = n->pos.y; s->npc[2] = n->pos.z;
  { const float *b_ = gameMsnTalkNpcBase(&M); if (b_) { s->npc[0] = b_[0]; s->npc[2] = b_[2]; } }   // сверка g07 круг 7 (КАМЕРА 4.3): камера считает по месту без сдвига
  s->pcHead = 1.5f; s->npcHead = a ? modelHead(a->m) : 0.f;
  if (pcH) *pcH = HOMER_HEIGHT; if (npcH) *npcH = a ? a->m->h->bbox[4] : 1.8f;
  if (M.talk.hasBest) { s->hasBest = 1; s->best[0] = M.talk.best.x; s->best[1] = M.talk.best.y; s->best[2] = M.talk.best.z; }
  return 1;
}
static int shotClear(const ConvScene *s, int shot, int side, MvRayFn blocked) {
  if (!blocked) return 1;
  float e[3], l[3], fy, f[3]; convPose(s, shot, side, e, l, &fy); convFace(s, convShotOnPc(shot) ? 0 : 1, f);
  return !blocked(f, e);
}
int mvTalkCam(const float prevEye[3], MvRayFn blocked, float eye[3], float look[3], float *fovY, float *nearZ) {
  ConvScene s;
  if (!talkScene(&s, 0, 0)) { TC.on = 0; return 0; }
  int pcSpeaks = 0, line = gameMsnTalkLine(&M, &pcSpeaks);
  if (!TC.on) { TC.on = 1; TC.line = -2; TC.shot = 0; TC.side = 0; memcpy(TC.prev, prevEye, sizeof TC.prev); }   // разговор начался
  if (line < 0) { if (TC.line < 0) { line = 0; pcSpeaks = 0; } else { line = TC.line; pcSpeaks = TC.pc; } }   // между репликами — прежний план
  if (line != TC.line) {
    int fromData = 0, shot = convShotFor((const char *const *)M.talk.cam, GAME_TALK_CAMS, line, pcSpeaks, &fromData);
    if (!TC.side) TC.side = convSide(&s, TC.prev);   // вся беседа — по одну сторону оси (правило 180°, как у записи PS2)
    if (!shotClear(&s, shot, TC.side, blocked)) {    // за стеной: тот же говорящий другим планом, иначе прежний план
      int alt = shot == CONV_NPC_FAR ? CONV_NPC_NEAR : shot == CONV_NPC_NEAR ? CONV_NPC_FAR : shot == CONV_PC_FAR ? CONV_PC_NEAR : CONV_PC_FAR;
      shot = shotClear(&s, alt, TC.side, blocked) ? alt : TC.shot;
    }
    TC.line = line; TC.pc = pcSpeaks; TC.shot = shot; TC.fromData = fromData;
    logf_("TALKSHOT t=%.1f line=%d pc=%d shot=%s data=%d side=%d best=%d\n", clock_, line, pcSpeaks, convShotName(shot), fromData, TC.side, s.hasBest);
  }
  TC.s = s;
  if (!TC.shot) return 0;
  gameMsnTalkNpcBack(&M, TC.shot == CONV_PC_NEAR);   // сверка g07 круг 7 (КАМЕРА 4.3): на крупном плане игрока собеседник на 1,8 м дальше
  int shot = TC.shot;
  if (TC.plant == 1) shot = convShotOnPc(shot) ? (shot == CONV_PC_FAR ? CONV_NPC_FAR : CONV_NPC_NEAR) : (shot == CONV_NPC_FAR ? CONV_PC_FAR : CONV_PC_NEAR);
  convPose(&s, shot, TC.side, eye, look, fovY);
  if (TC.plant == 2) {   // отрицательный контроль: глаз на оси за слушающим (как камера пешком за Гомером — правка 40)
    const float *F = convShotOnPc(shot) ? s.npc : s.pc, *S = convShotOnPc(shot) ? s.pc : s.npc;
    float ax = S[0] - F[0], az = S[2] - F[2], d = sqrtf(ax * ax + az * az) + 1e-6f; ax /= d; az /= d;
    eye[0] = F[0] - ax * 2.5f; eye[2] = F[2] - az * 2.5f;
  }
  if (nearZ) *nearZ = TC.plant == 3 ? 1e9f : convNear(&s, eye);   // plant 3 — отрицательный контроль: ближняя плоскость города
  return 1;
}
int mvTalkActive(void) { return active && M.talk.on; }
static int talkIsStart(void) {   // разговор — последний этап вступления: после него брифинг миссии (запись PS2: «✕ Start Mission / △ Cancel»)
  const MsnRuntime *rt = &M.rt;
  if (rt->cur < 0 || rt->phase != 0 || rt->obj != MSN_OBJ_DIALOGUE) return 0;
  unsigned ph = M.f.missions[rt->cur].intro;
  return ph != MSN_NONE16 && ph < M.f.nphases && rt->stage == (int)M.f.phases[ph].stage_count - 1;
}
// сверка g07b-msnview круг 2 (g07-missions 30, P07:23): AllowMissionAbort("false") — Accept/Cancel вступления не показываются и △ не отменяет (подпись «Start Mission / Cancel» заменена на ✕ SKIP). SV_ABORTOK_OLD=1 — прежнее
int mvTalkPrompt(void) { return !mvTalkActive() ? UI_LB_NONE : mvTalkPromptForce1322 ? mvTalkPromptForce1322 : (talkIsStart() && (SV_ABORTOK_OLD || gameMsnAbortAllowed(&M))) ? UI_LB_START : UI_LB_SKIP; }
// круг 3 (проверка g10b-loop, M2:23): на последнем этапе свободной езды (разговор, после которого брифинг миссии) и в разговоре Пэтти с Сельмой полосы на выходе сходятся к
// центру, а не уезжают. Диалог гонки в данных нашего порта отдельным признаком не отличается — не включён (см. журнал)
int mvTalkConverge(void) {
  if (!active || !M.talk.on) return 0;
  if (talkIsStart()) return 1;
  const char *a = M.talk.npc, *b = M.talk.npc2;
  return (!strcasecmp(a, "patty") && !strcasecmp(b, "selma")) || (!strcasecmp(a, "selma") && !strcasecmp(b, "patty")) || (!strcasecmp(a, "patty") && b[0]) || (!strcasecmp(a, "selma") && b[0]);
}
int mvTalkInput(unsigned hit) {
  if (!mvTalkActive()) return 0;
  int start = talkIsStart();
  { static int lg_; if (hit && lg_++ < 40) logf_("TALKIN1357 hit=0x%x start=%d abortok=%d t=%.2f\n", hit, start, (SV_ABORTOK_OLD || gameMsnAbortAllowed(&M)), clock_); }   // 1357: что дошло до разговора (гейт)
  if (hit & PSP_CTRL_CROSS) {   // SKIP / Start Mission: разговор кончается сразу — дальше брифинг или следующий этап
    logf_("TALKSKIP start=%d line=%d t=%.1f\n", start, TC.line, clock_);
    msnAudioStopVoice(&M.audio); msn_notify(&M.rt, MSN_IN_DIALOGUE_DONE, 0, 0);
  } else if (start && (hit & PSP_CTRL_TRIANGLE) && (SV_ABORTOK_OLD || gameMsnAbortAllowed(&M))) {   // Cancel: миссия не начинается, назад к её вступлению
    logf_("TALKCANCEL line=%d t=%.1f\n", TC.line, clock_);
    msnAudioStopVoice(&M.audio); gameMsnTalkCancel(&M);   // В85: на места до разговора, окно повторного △ (game_msn.h)
  }
  return 1;
}
int mvTalkCamInfo(MvTalkCamInfo *o) {
  if (!TC.on || !TC.shot) return 0;
  memset(o, 0, sizeof *o); float pcH = 0.f, npcH = 0.f; ConvScene s;
  if (!talkScene(&s, &pcH, &npcH)) return 0;
  o->line = TC.line; o->pcSpeaks = TC.pc; o->shot = TC.shot; o->side = TC.side; o->fromData = TC.fromData; o->hasBest = s.hasBest;
  memcpy(o->pc, s.pc, 12); memcpy(o->npc, s.npc, 12); memcpy(o->best, s.best, 12);
  o->pcHead = s.pcHead; o->npcHead = s.npcHead > .05f ? s.npcHead : 1.5f; o->pcH = pcH; o->npcH = npcH;
  snprintf(o->npc_name, sizeof o->npc_name, "%s", M.talk.npc); snprintf(o->conv, sizeof o->conv, "%s", talkConv);
  return 1;
}

// ------------------------------------------------------------------ служебное: к цели (сценарий смоука)
void mvDropSim(int op, int idx) {   // служебное (№1229): без езды за мусоровозом проверить показ и гашение «Item Dropped!» по тем же полям, что у настоящего выпадения/подбора
  if (idx < 0 || idx >= M.rt.nitems || idx >= MSN_MAX_ITEMS) return;
  if (op == 0) { { MsnWorld w_; gameMsnWorld(&M, &w_); M.rt.relpos[idx] = w_.player; M.rt.relpos[idx].x += 30.f;   /* в 30 м: не подбирается сама */ M.rt.drop_life[idx] = 0; M.rt.drop_blink[idx] = 0; } M.rt.released[idx] = 1; if (!dropOn) logf_("MSN dropmsg on t=%.1f\n", clock_); dropOn = 1; dropT0 = clock_; logf_("MSN drop %d (sim) t=%.1f\n", idx, clock_); }
  else { M.rt.done[idx] = 1; logf_("MSN got %d (sim) t=%.1f\n", idx, clock_); }
}
// 1355 (бот-проходчик по миссиям, Роб: «радар ведёт не туда, у пончиковой нет Виггама»): строка AUDIT1355 по текущему этапу — что игра считает целью (msn_nav_target), где метка HUD и радара (H.marker, радар рисует её же),
// насколько метка далека от цели, расстояние до игрока, есть ли модель машины и персонаж цели, видна ли точка цели (клетка под ней загружена). Только чтение; flag msnaudit
void mvServiceAudit(void) {
  if (!active || M.rt.cur < 0) { logf_("AUDIT1355 none t=%.1f\n", clock_); return; }
  MsnRuntime *rt = &M.rt; MsnVec3 p; memset(&p, 0, sizeof p); MsnWorld w; gameMsnWorld(&M, &w); int has = msn_nav_target(rt, &w, &p);
  const char *nm = rt->target && *rt->target ? rt->target : tgtName1338[0] ? tgtName1338 : "";
  float me = H.markerOn && has ? hypotf(H.marker.pos.x - p.x, H.marker.pos.z - p.z) : -1.f, dp = has ? hypotf(w.player.x - p.x, w.player.z - p.z) : -1.f;
  int car = *nm && mvCarReady ? (mvCarReady(nm) ? 1 : 0) : -1, npc = *nm ? (gameMsnNpc(&M, nm) != 0) : -1, vis = has ? pointVisible(p.x, p.y, p.z, NULL) : -1;
  logf_("AUDIT1355 mission=%s stage=%d.%d obj=%s name=%s target=%d(%.0f,%.0f) marker=%d(%.0f,%.0f) markerErr=%.1f playerDist=%.1f car=%d npc=%d visible=%d status=%d t=%.1f\n", msn_str(&M.f, M.f.missions[rt->cur].id), rt->phase, rt->stage,
        rt->obj >= 0 && rt->obj < MSN_OBJ_COUNT ? msn_objective_name[rt->obj] : "?", *nm ? nm : "-", has, p.x, p.z, H.markerOn, H.markerOn ? H.marker.pos.x : 0.f, H.markerOn ? H.marker.pos.z : 0.f, me, dp, car, npc, vis, (int)rt->status, clock_);
}
void mvServiceGo(float far) {
  if (!active || M.rt.status != MSN_RUNNING) return;
  Game *g = M.g; MsnRuntime *rt = &M.rt;
  if (rt->obj == MSN_OBJ_GETIN && gameOnFoot(g)) {   // к двери водителя: слева от машины (модель смотрит в -Z, руль слева)
    float h = g->pCar->h, rx = cosf(h), rz = -sinf(h), d = 1.6f + far;
    homerPlace(g->pHomer, g->pWorld, g->pCar->x - rx * d, g->pCar->y + .5f, g->pCar->z - rz * d, h);
    logf_("MSNGO getin far=%.1f\n", far); return;
  }
  MsnVec3 p; MsnWorld w; gameMsnWorld(&M, &w);
  if (!msn_nav_target(rt, &w, &p)) { logf_("MSNGO none obj=%d\n", rt->obj); return; }
#ifndef MSNBOTTRIG_OLD
#define MSNBOTTRIG_OLD 0   // 1 — как до 06.11: бот к точке локатора; зона срабатывания (объём) у части целей смещена от неё — этап не засчитывался (msnflow4: «DRIVE TO THE SCHOOL» и др.)
#endif
  if (!MSNBOTTRIG_OLD && rt->st && rt->obj != MSN_OBJ_FOLLOW && far == 0.f) {   // этап с условием «держись рядом» (followdistance: L5 m1 «DRIVE TO THE COLA TRUCK…») — перенос оторвал бы от цели и дал провал: не переносить
    for (unsigned i_ = 0; i_ < rt->st->ncond; i_++) if (M.f.conds[rt->st->cond_first + i_].type == MSN_COND_FOLLOWDISTANCE) { logf_("MSNGO hold obj=%d followdistance\n", rt->obj); return; }
  }
  if (!MSNBOTTRIG_OLD && (rt->obj == MSN_OBJ_GOTO || rt->obj == MSN_OBJ_GOOUTSIDE) && rt->c_dest) {   // к центру первой зоны срабатывания локатора
    const MsnLoc *l_ = msn_arg_loc(&M.f, rt->c_dest, 0);
    if (l_ && l_->ntrig) { const MsnTrig *t_ = &M.f.trigs[l_->first_trig]; p.x = t_->center[0]; p.y = t_->center[1]; p.z = t_->center[2]; }
  }
  const char *targetRoom = npcRoomAt ? npcRoomAt(p.x, p.y, p.z) : NULL;
#ifndef MSNBOTROOM_OLD
#define MSNBOTROOM_OLD 0   // 1 — как до 06.11: цель в комнате / этап «войти» — «комнаты не в этой сборке», этап засчитывал только помощник (142 из 520 зачётов msnflow3)
#endif
  if (!MSNBOTROOM_OLD && npcRoomGo && gameOnFoot(g)) {   // цель в другой комнате или в городе, а игрок в комнате: к двери / к выходу, △ жмёт помощник (mvSvcDoorWant)
    float q_[3] = {p.x, p.y, p.z}, o_[3]; int r_ = npcRoomGo(q_, o_, rt->obj == MSN_OBJ_INTERIOR);
    if (r_) { homerPlace(g->pHomer, r_ == 1 ? g->pWorld : NULL, o_[0], o_[1] + .5f, o_[2], g->pHomer->h); svcDoor = 1;
      logf_("MSNGO %s obj=%d to %.1f %.1f %.1f\n", r_ == 1 ? "door" : "exit", rt->obj, o_[0], o_[1], o_[2]); return; }
  }
  if (rt->obj != MSN_OBJ_INTERIOR && targetRoom && pointVisible(p.x, p.y, p.z, NULL) && gameOnFoot(g)) {   // та же настоящая комната:
    Homer *H = g->pHomer; float dx = H->x - p.x, dz = H->z - p.z, d = sqrtf(dx * dx + dz * dz);   // по линии от Гомера к цели,
    if (d < .01f) { dx = 1.f; dz = 0.f; d = 1.f; }                                                  // в 1,8 м от неё, лицом к ней
    float k = (1.8f + far) / d; homerPlace(H, NULL, p.x + dx * k, H->y, p.z + dz * k, atan2f(-dx, -dz));
    logf_("MSNGO room obj=%d to %.1f %.1f %.1f far=%.1f\n", rt->obj, p.x, p.y, p.z, far); return;
  }
  if (rt->obj == MSN_OBJ_INTERIOR || targetRoom) { if (!MSNBOTROOM_OLD && !gameOnFoot(g)) { svcOutCar = 1; logf_("MSNGO room obj=%d из машины\n", rt->obj); return; }
    logf_("MSNGO room obj=%d (комнаты не в этой сборке)\n", rt->obj); return; }
  float off = 1.5f;   // правка 345: к собеседнику — в его радиус разговора (1,3 м по умолчанию), как подошёл бы игрок
  if (rt->obj == MSN_OBJ_TALKTO) { float r = msn_talk_reach(rt, rt->c_talk) - .4f; if (r < off) off = r; }
  if (gameOnFoot(g)) homerPlace(g->pHomer, g->pWorld, p.x + off + far, p.y + .5f, p.z, g->pHomer->h);
  else {   // машина: на far м к югу от цели, носом к ней (снимок видит маркер впереди)
    float cx = p.x, cz = p.z - far, h = far > 0.f ? atan2f(p.x - cx, p.z - cz) : g->pCar->h;
    carPlace(g->pCar, g->pWorld, cx, p.y + 1.f, cz, h);
  }
  logf_("MSNGO obj=%d to %.1f %.1f %.1f far=%.1f\n", rt->obj, p.x, p.y, p.z, far);
}
int mvServicePlace(int fresh) {   // служебное (06.11, помощник msnbotpass): поставить игрока туда, где его оставил бы пройденный этап; 1 — поставил
  if (!active || M.rt.status != MSN_RUNNING) return 0;
  MsnRuntime *rt = &M.rt; Game *g = M.g;
  if (fresh) {   // зачтённый этап не довёз игрока: слежка проваливается на дальности в первые кадры — к машине-цели на 15 м
    MsnVec3 p; MsnWorld w; gameMsnWorld(&M, &w);
    int keep_ = rt->obj == MSN_OBJ_FOLLOW;   // слежка — и любой этап с условием «держись рядом» (L3 m5 dump «FOLLOW SNAKE AND COLLECT EVIDENCE»)
    for (unsigned i_ = 0; !keep_ && rt->st && i_ < rt->st->ncond; i_++) keep_ = M.f.conds[rt->st->cond_first + i_].type == MSN_COND_FOLLOWDISTANCE;
    if (!keep_ || gameOnFoot(g) || !msn_nav_target(rt, &w, &p)) return 0;
    float dx = g->pCar->x - p.x, dz = g->pCar->z - p.z; if (dx * dx + dz * dz < 30.f * 30.f) return 0;
    mvServiceGo(15.f); return 1;
  }
  if (rt->obj != MSN_OBJ_GOTO && rt->obj != MSN_OBJ_TALKTO && rt->obj != MSN_OBJ_DELIVERY && rt->obj != MSN_OBJ_PICKUPITEM && rt->obj != MSN_OBJ_GETIN && rt->obj != MSN_OBJ_INTERIOR) return 0;
  mvServiceGo(0.f); return 1;   // цели-места: к ним; машины-цели (гонка, погоня, уничтожить) не трогаем — машина встала бы в чужую
}
