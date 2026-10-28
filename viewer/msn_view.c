// Миссии в EBOOT: интерфейс, персонажи, маркеры (см. msn_view.h). Логика миссий — game_msn.c (общая с ботами).
#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "msn_view.h"
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
#include "ui.h"
#include "msnart.h"
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
static int preFail;   // modelFor: файл не открылся / повреждён — повторять нечего
static int prePending(const char *name) { for (int i = 0; i < NPRE; i++) if (pre[i].p && msn_streq(pre[i].name, name)) return 1; return 0; }
#define NANIM 10
typedef struct { char name[24]; const SkinModel *m; SkinPlayer p; int idle, walk; int talkLine, gest; float gestT;
                 int flail, land, getup, poseTick; float lift; int liftInit; } NpcAnim;   // правка 240: сбит машиной — клипы рига flail / fall_land / get_up (-1 — нет); №510: lift — подъём стоп
static NpcAnim *anim[NANIM];
static void dropModel(int k) {   // модель и все её анимации — из кучи (слот фонового персонажа отдан персонажу миссии)
  bbOp("npc-", model[k].name, 0);   // правка 103: самописец
  for (int i = 0; i < NANIM; i++) if (anim[i] && anim[i]->m == model[k].m) { free(anim[i]); anim[i] = 0; }
  skinFree(model[k].m); model[k].m = 0; model[k].name[0] = 0; model[k].amb = 0;
}
static void dropModel(int k);
static int preStartBlocked(void);
static const SkinModel *modelFor(const char *name, int amb) {
  for (int i = 0; i < NMODEL; i++) if (model[i].m && msn_streq(model[i].name, name)) { if (!amb) model[i].amb = 0; return model[i].m; }
  int used = 0, ambUsed = 0, freeSlot = -1;
  for (int i = 0; i < NMODEL; i++) { if (model[i].m) { used++; ambUsed += model[i].amb; } else if (freeSlot < 0) freeSlot = i; }
  if (amb && (ambUsed >= NMODEL_AMB || used >= NMODEL)) return 0;          // фоновому — только свободный слот из своих двух
  if (!amb && freeSlot < 0)                                                // персонаж миссии: слот фонового освобождается
    for (int i = 0; i < NMODEL; i++) if (model[i].m && model[i].amb) { dropModel(i); break; }
  for (unsigned k = 0; k < sizeof npcFiles / sizeof *npcFiles; k++) {
    const char *b = npcFiles[k] + 6; size_t n = strlen(name);   // "chars/" + имя + ".chr"
    if (strncmp(b, name, n) || strcmp(b + n, ".chr")) continue;
    for (int i = 0; i < NMODEL; i++) if (!model[i].m) {
      char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir, npcFiles[k]);
      unsigned t0 = sceKernelGetSystemTimeLow();
      if (!STAGE_PREFETCH_OLD && !mvPump) {   // фоновое чтение: пока файл не прочитан, модели нет (вернём 0, вызывающий повторит)
        int pi = -1; for (int q = 0; q < NPRE; q++) if (pre[q].p && msn_streq(pre[q].name, name)) pi = q;
        if (pi < 0) {   // начать чтение
          if (preStartBlocked()) return 0;
          for (int q = 0; q < NPRE && pi < 0; q++) if (!pre[q].p) pi = q;
          if (pi < 0) return 0;   // очередь чтения занята — позже
          bbOp("npc+", name, BB_SYNC_ASYNC_OLD ? 1 : 0);   // фоновое чтение: запись ящика не в кадре (blackbox.h)
          pre[pi].p = skinPreStart(fn); if (!pre[pi].p) { logf_("MSNNPC load-fail %s\n", fn); bbOp("npc!", name, 0); preFail = 1; return 0; }
          strncpy(pre[pi].name, name, sizeof pre[pi].name - 1); pre[pi].name[sizeof pre[pi].name - 1] = 0; pre[pi].t0 = t0 = sceKernelGetSystemTimeLow();
          if (!skinPreReady(pre[pi].p)) { logf_("MSNNPC prefetch %s\n", name); return 0; }
        } else if (!skinPreReady(pre[pi].p)) return 0;
        model[i].m = skinPreFinish(pre[pi].p); pre[pi].p = 0; pre[pi].name[0] = 0;
        if (!model[i].m && !SKINPRE_ERR_OLD) { logf_("MSNNPC async-fail %s, sync read\n", name); model[i].m = skinLoad(fn); }   // ревизия 1.2: ошибка фонового чтения — синхронно, как до заглядывания вперёд
        if (!model[i].m) { logf_("MSNNPC load-fail %s\n", fn); bbOp("npc!", name, 0); preFail = 1; return 0; }
        strncpy(model[i].name, name, sizeof model[i].name - 1); model[i].amb = amb;
        logf_("MSNNPC load %s bytes=%u ms=%.0f async=1\n", name, model[i].m->h->size, (sceKernelGetSystemTimeLow() - t0) / 1000.f);
        return model[i].m;
      }
      if (mvPump) mvPump();   // правка 244: на экране загрузки — кадр газеты до и после чтения модели
      bbOp("npc+", name, 1);   // правка 103: самописец — ящик на карте ДО чтения модели (встанет посреди — видно, кто грузился)
      model[i].m = skinLoad(fn);
      if (mvPump) mvPump();
      if (!model[i].m) { logf_("MSNNPC load-fail %s\n", fn); bbOp("npc!", name, 0); return 0; }
      strncpy(model[i].name, name, sizeof model[i].name - 1); model[i].amb = amb;
      logf_("MSNNPC load %s bytes=%u ms=%.0f\n", name, model[i].m->h->size, (sceKernelGetSystemTimeLow() - t0) / 1000.f);
      return model[i].m;
    }
  }
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
  int briefing, brFirst, brLoaded, brQueued, brPicLost, brWager; unsigned brT0; char present[32], title[96], info[256];   // брифинг миссии (MissionLoad.scr): картинка SetPresentationBitmap
  int levelDone; float levelDoneAt;    // MSN_EV_LEVEL_COMPLETE: 1 — уровень, 2 — игра пройдена; когда отдать main.c (clock_)
  int levelEnd;                        // открыт экран конца уровня (LevelEnd.scr), ждёт X
  char fmv[32]; int fmvWait;           // цель «fmv» (SetFMVInfo): ролик ждёт main.c (mvFmvPending / mvFmvDone)
  char dynaRoom[32];                   // последний SetDynaLoadData: интерьер («…@», "SimpsonsHouse"; "" — город), как в оригинале до следующего
} H;
static int levelEndOld;   // служебное (flag uilevelendold): прежняя вспышка 3 с вместо экрана LevelEnd (отрицательный контроль)
static int iconIndex(const char *icon) {   // значок задания: из msnicons (msnart.shr, может ещё дочитываться) — иначе без значка
  return icon && *icon ? UI_OBJICON_EXTERNAL : -1;
}
// №958б (Роб: значок цели с L1 «зависает» и показывается на других уровнях; в эмуляторе не воспроизведено): состояние значка — в строку чёрного ящика
// каждую секунду (поле ico, viewer/main_frontend.h bbSample) и строкой ICON при каждой смене значка/облачка (кто, какой уровень цели); в игре и в выпуске
#ifndef BBICON958_OLD
#define BBICON958_OLD 0
#endif
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
void mvBubblePump(void) { if (holdOn && (!mvCamBusy || !mvCamBusy())) { holdOn = 0; objectiveShow(holdText, holdIcon); } }
static void uiObjectiveCb(void *u, const char *text, int msg, const char *icon) {
  (void)u; (void)msg;
  if (text && *text && mvCamBusy && mvCamBusy()) { snprintf(holdText, sizeof holdText, "%s", text); snprintf(holdIcon, sizeof holdIcon, "%s", icon ? icon : ""); holdOn = 1; return; }   // №1047: сначала наезд, потом пузырь
  holdOn = 0; objectiveShow(text, icon);
}
static void objectiveShow(const char *text, const char *icon) {
  if (text && *text) { msnartSetObjIcon(icon); uiMessage(text, iconIndex(icon), 0.f); iconLv = mvPackLevel(); iconNote("set"); }
  else { uiMessage(NULL, -1, 0.f); iconNote("off"); }
}
static void uiTimerCb(void *u, int ms, int up) { (void)u; (void)up; uiHudSet(UI_F_TIMER, ms < 0 ? 0 : ms); uiHudShow(UI_T_TIMER, ms >= 0); }
static void uiParCb(void *u, int ms) { (void)u; uiHudSet(UI_F_PARTIME, ms < 0 ? 0 : ms); uiHudShow(UI_T_PARTIME, ms >= 0); }
static void uiCounterCb(void *u, int done, int total) {   // гонка по кругам — прогресс показывают круги, не точки
  (void)u; int race = M.rt.obj == MSN_OBJ_RACE && M.rt.laps > 1;
  uiHudSet(UI_F_COLLECTED, done); uiHudSet(UI_F_COLLECT_TOTAL, total); uiHudShow(UI_T_COLLECTIBLES, total > 0 && !race);
}
static void uiLapCb(void *u, int lap, int laps) { (void)u; uiHudSet(UI_F_LAP, lap); uiHudSet(UI_F_LAPS, laps); uiHudShow(UI_T_LAP, laps > 1); }
static void uiCountdownCb(void *u, const char *text, float s) {   // "3" "2" "1" — число HUD; "GO!" и прочее — строка задания
  (void)u; (void)s;
  if (!text) { uiHudShow(UI_T_COUNTDOWN, 0); return; }   // слово остаётся для угасания; следующая цифра его сбросит (uiCountdownText(0))
  if (text[0] >= '0' && text[0] <= '9') { uiCountdownText(0); uiHudSet(UI_F_COUNTDOWN, atoi(text)); uiHudShow(UI_T_COUNTDOWN, 1); }
  else if (!text[0]) uiHudShow(UI_T_COUNTDOWN, 0);   // пустая строка — не слово: без устаревшего числа на экране
#if !defined(COUNTDOWN_LINE_OLD) || !COUNTDOWN_LINE_OLD
  else { uiCountdownText(text); uiHudShow(UI_T_COUNTDOWN, 1); }   // №630: слова отсчёта («PSYCHE», «GO!») в центре тем же зелёным шрифтом, длительность записи сценария
#else
  else { uiHudShow(UI_T_COUNTDOWN, 0); uiMessage(text, -1, s > 0.f ? s : 1.f); }
#endif
}
static void uiFlashCb(void *u, int what, const char *text) {
  (void)u;
  if (what == MSN_FLASH_STAGE_COMPLETE) H.stageFlash = clock_ + 2.f;          // MissionProgress ("этап пройден")
  else if (what == MSN_FLASH_MISSION_COMPLETE || what == MSN_FLASH_BONUS_COMPLETE) {   // правка 122: текст вспышки — из пакета миссий
    H.missionFlash = clock_ + 3.f; uiMessage(NULL, -1, 0.f);                             // (MISSION_COMPLETE / BONUS_MISSION_COMPLETE)
    uiMissionDoneText(text && *text ? text : (what == MSN_FLASH_BONUS_COMPLETE ? "BONUS MISSION\nCOMPLETE!" : "MISSION COMPLETE!")); }
  else uiMessage(text, -1, 3.f);                                              // уровень / игра пройдены
  logf_("MSN flash %d %s t=%.1f\n", what, text ? text : "", clock_);
}
static void uiFailedCb(void *u, int reason, const char *text, const char *hint) {   // экран MissionOver: причина, подсказка
  (void)u; H.failed = 1; H.failSel = 0;
  { static const char *const why[] = {"none", "timeout", "outofvehicle", "damage", "position", "race", "followdistance", "keepbarrel",
                                      "partime", "abort", "hitnrun"};   // правка 281: причина провала — в чёрный ящик (на PSP видно, отчего fail)
    bbOp("fail", reason >= 0 && reason < (int)(sizeof why / sizeof *why) ? why[reason] : "?", 1); }
  snprintf(H.failMsg, sizeof H.failMsg, "%s", text ? text : "");
  uiSetMissionOver(text, hint); uiMessage(NULL, -1, 0.f);
  logf_("MSN failed %d %s t=%.1f\n", reason, text ? text : "", clock_);
}
static int mkShadowOld;   // 04.10: 1 — маркер цели без пятна <рисунок>_shadow (как до правки; флаг сценария mkshadowold)
void mvMarkerShadowOld(int on) { mkShadowOld = on; }
static void navMarkerCb(void *u, const MsnMarker *m) { (void)u; H.markerOn = m != 0; if (m) H.marker = *m;
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
static void itemCollectedCb(void *u, int index, const MsnItem *it) { (void)u; if (it) cfxAt(it->effect, &it->at.pos, "item", index); }
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
#ifndef BRIEF_PIC_OLD
#define BRIEF_PIC_OLD 0   // №512: =1 — ждать картинку брифинга без предела, как раньше
#endif
#define BRIEF_PIC_MAX 8.0f
static int brBriefed;          // брифинг уже был на этом уровне (mvInit сбрасывает)
static int brOld;              // служебное (mvBriefOld): отрицательный контроль гейта — прежняя карточка
void mvBriefOld(int on) { brOld = on; }
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
static float brReadyT = -1.f;   // с какого времени брифинга всё прежнее готово (для предела ожидания чтения)
static int readaheadBusy(void) { for (int q = 0; q < NPRE; q++) if (pre[q].p && !skinPreReady(pre[q].p)) return 1; return 0; }
static int briefReadyBase(void);
static int briefReady(void) {
  int r = briefReadyBase();
  if (!r || MSN_READAHEAD_OLD || BRIEF_CONTINUE_READY_OLD) return r;
  float t = briefSec(); if (brReadyT < 0.f) brReadyT = t;
  return readaheadBusy() && t < brReadyT + BRIEF_RA_MAX ? 0 : 1;
}
static int briefReadyBase(void) {
  if (brOld) return 1;
  if (H.brFirst && briefSec() < BRIEF_FIRST_LOAD) return 0;
  const void *pix = 0; int tw, th, w, h;
  if (!H.present[0] || (msnartImageData(H.present, &pix, &tw, &th, &w, &h) && pix)) {
    if (!BRIEF_ART_PRELOAD_OLD && H.present[0] && !uiBriefPicSettled(3) && briefSec() < BRIEF_PIC_MAX) return 0;   // №612 (видео TEST17): картинка догружалась уже на показанной карточке — ждём 3 кадра готовой картинки под газетой
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
          else if (c->op == MSN_OP_SETRACEENTERYFEE) pay = 2 * msn_arg_i(&M.f, c, 0); } }
      logf_("MSN wager fee=%d par=%d pay=%d t=%.1f\n", fee, par, pay, clock_); }
    uiHudSet(UI_F_WAGER_FEE, fee); uiHudSet(UI_F_WAGER_PAR, par); uiHudSet(UI_F_WAGER_PAY, pay); uiSetWager(gm && fee > 0); H.brWager = gm && fee > 0; }
  H.briefing = 1; uiMessage(NULL, -1, 0.f); uiBriefAnim(1); if (!BRIEF_ANIM_AT_LOAD_OLD) uiBriefHold();   // №512: карточка въезжает слева; №994: пока не готова — только LOADING, въезд стартует при готовности
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
static void raceOff(void) { if (raceProps[0]) bbOp("race-", raceProps, 0); raceProps[0] = 0; nRaceSeg = 0; colExtraFences(NULL, 0); }
static void raceFences(const char *stem) {
  nRaceSeg = 0; colExtraFences(NULL, 0);
  const unsigned char *p = rfData; if (!p || rfSize < 8 || memcmp(p, "RFN1", 4)) return;
  unsigned n; memcpy(&n, p + 4, 4); unsigned off = 8;
  for (unsigned r = 0; r < n && off + 36 <= rfSize; r++) {
    char nm[33]; memcpy(nm, p + off, 32); nm[32] = 0; unsigned k; memcpy(&k, p + off + 32, 4); off += 36;
    if (off + k * 24 > rfSize) return;
    if (!strcasecmp(nm, stem)) {
      for (unsigned i = 0; i < k && nRaceSeg < RACE_SEG_MAX; i++) {
        float f[6]; memcpy(f, p + off + i * 24, 24); ColSeg *g = &raceSeg[nRaceSeg++];
        g->a[0] = f[0]; g->a[1] = f[1]; g->b[0] = f[2]; g->b[1] = f[3]; g->n[0] = f[4]; g->n[1] = f[5];
        float y; if (!gameMsnGroundY(&M, (f[0] + f[2]) * .5f, (f[1] + f[3]) * .5f, &y)) y = 0.f;
        g->y0 = y - 2.f; g->y1 = y + 3.f;
      }
      colExtraFences(raceSeg, nRaceSeg); return;
    }
    off += k * 24;
  }
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
  logf_("MSN race-props %s fences=%u t=%.1f\n", stem, nRaceSeg, clock_);
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
static void fadeCb(void *u, int out, float t) { (void)u; if (out) { fadeDir = 1; fadeLen = t > .05f ? t : .05f; } else { fadeDir = -1; fadeLen = t > .05f ? t : FADE_IN; } }
static void stayBlackCb(void *u) { (void)u; stayBlack = 1; fadeA = 1.f; fadeDir = 0; }
float mvFadeAlpha(void) { return active ? fadeA : 0.f; }
void mvServiceFade(int out, float t) { fadeCb(0, out, t); }   // служебное: flag fadeout / fadein (проверка затемнения)
static void fadeTick(float dt) {
  if (fadeDir) { fadeA += (float)fadeDir * dt / fadeLen; if (fadeA >= 1.f) { fadeA = 1.f; fadeDir = 0; } if (fadeA <= 0.f) { fadeA = 0.f; fadeDir = 0; } }
}
static void cleanupCb(void *u) {
  (void)u; raceOff(); fadeA = 0.f; fadeDir = 0; stayBlack = 0;   // правка 122: пропы и заборы гонки — до конца гонки
  H.failed = 0; H.markerOn = 0; H.talk[0] = 0; uiMessage(NULL, -1, 0.f); iconNote("cleanup");
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
static void voMissionEnd(int fail) { if (!MV_VO_OLD) { voPending = fail; voAt = clock_ + .6f; } }
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
        #ifndef SND_VO_EXTRA_OLD
        if (c->car.carHitV > 0.f) sndRivalSay(c->driver, SND_V_HITCAR, 0);
        else
#endif
        if (c->car.damage > voDmg[i] + .05f) sndRivalSay(c->driver, SND_V_DAMAGE, 0);
      }
      voDmg[i] = c->car.damage;
    }
  }
  if (voPending >= 0 && clock_ >= voAt) {
    int f = voPending; voPending = -1;
    if (!(voRival[0] && sndRivalSay(voRival, f ? SND_V_MFAIL : SND_V_MVIC, 1))) sndSay(f ? SND_SAY_MFAIL : SND_SAY_MVIC, 1);
  }
}
static void voRace(int pos, int prev) {   // место игрока сменилось (1 — впереди)
  if (MV_VO_OLD || pos <= 0 || prev <= 0 || pos == prev) return;
  static int alt;
  if (pos < prev) sndSay(SND_SAY_PASS, 0);
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
static void eventCb(void *u, const MsnEvent *e) {
  (void)u; const GameMsn *m = &M;
  switch (e->type) {
    case MSN_EV_MISSION_START: H.present[0] = 0; logf_("MSN mission %s \"%s\" t=%.1f\n", e->name ? e->name : "", e->text ? e->text : "", clock_); msnartMissionStart(); break;
    case MSN_EV_STAGE_START: H.stages++;
      if (!stayBlack && fadeA > 0.f && fadeDir >= 0) { fadeDir = -1; fadeLen = FADE_IN; }   // правка 122: из чёрного   // talk_target этого этапа уже пришёл (msn_game зовёт его до хвоста событий)
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
    case MSN_EV_MISSION_COMPLETE: logf_("MSN complete %s t=%.1f\n", e->name ? e->name : "", clock_); voMissionEnd(0); break;
    case MSN_EV_MISSION_FAILED: voMissionEnd(1); break;   // аудит 09.10: реплика провала (экран провала — свой обработчик)
    case MSN_EV_DROP: uiForceShow(UI_T_ITEM_DROPPED, 2.f);   // таран машины с грузом: «Item Dropped!» (Scrooby HUD), 2 с — правило порта
      logf_("MSN drop %d at %.1f %.1f t=%.1f\n", e->index, e->pos.x, e->pos.z, clock_); break;
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
static const MsnAudioBackend audioBackend = {0, abTrack, abSec, abPlay, abPlaying, abMusic, abRelease, abMode, abLog};

int mvInit(const char *ebootPath, int level) {
  ambElapsed = 0.f;
  brBriefed = 0;   // правка 52: первая карточка уровня — с загрузкой
  strncpy(dir, ebootPath, sizeof dir - 1); char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
  char art[sizeof dir + 8]; if (level > 1) snprintf(art, sizeof art, "%sl%d/", dir, level); else snprintf(art, sizeof art, "%s", dir);   // msnart.shr: 2–7 — в l<N>/
  msnartInit(art);
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
void mvFree(void) { mvShutdown(); mvFreePack(); msnartShutdown(); }   // смена уровня: всё, что держали миссии уровня
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
int mvServiceBot(void) {
  if (!active || M.rt.status != MSN_RUNNING) return 0;
  if (M.rt.obj == MSN_OBJ_TALKTO) return 1;   // собеседник: пешком △ говорит, в машине △ выходит из неё (миссии m6/m7 начинаются с игроком за рулём)
  if (!gameOnFoot(M.g)) return 0;
  switch (M.rt.obj) {
    case MSN_OBJ_GETIN: return 1;
    case MSN_OBJ_GOTO: case MSN_OBJ_RACE: case MSN_OBJ_FOLLOW: case MSN_OBJ_LOSETAIL: case MSN_OBJ_DELIVERY: case MSN_OBJ_DUMP: case MSN_OBJ_DESTROY: case MSN_OBJ_DESTROYBOSS: return 2;
    default: return 0;
  }
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
void mvServiceFail(void) {   // служебное (flag msnfail): провал миссии с экраном повтора (№572)
  H.failed = 1; H.failSel = 0;
  uiSetMissionOver("Timed out.", "Hint : Speed is essential! Drive a fast vehicle.");
  uiMessage(NULL, -1, 0.f);
}
int mvActive(void) { return active; }
void mvShutdown(void) {   // выход в главное меню: всё, что миссии заняли в куче, освобождается (куча меню — как до игры)
  if (active) { MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e); }   // прошлый круг: убрать всё
  if (active) msnAudioStopVoice(&M.audio);   // и реплику завершения, если ещё звучит
  abSavedMode = -1;
  for (int i = 0; i < NANIM; i++) if (anim[i]) { free(anim[i]); anim[i] = 0; }
  for (int k = 0; k < NMODEL; k++) if (model[k].m) { skinFree(model[k].m); model[k].m = 0; model[k].name[0] = 0; }
  for (int q = 0; q < NPRE; q++) if (pre[q].p) { skinPreCancel(pre[q].p); pre[q].p = 0; pre[q].name[0] = 0; }   // фоновые чтения моделей — отменить
  nNpcPend = 0;
  msnartReset(); uiMessage(NULL, -1, 0.f); iconNote("shutdown"); uiPauseMenuHide(0); raceOff(); cfxReset();
  H.levelDone = 0; H.fmvWait = 0; H.levelEnd = 0;
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
int mvInMission(void) { return active && M.rt.cur >= 0 && (M.rt.status == MSN_RUNNING || M.rt.status == MSN_COUNTDOWN) && M.rt.phase == 1 && !msn_free_roam(&M.rt); }
void mvPauseRestart(void) {
  if (!active || M.rt.cur < 0) return;
  H.failed = 0; uiHudShow(UI_T_MISSION_FAILED, 0);
  if (M.f.missions[M.rt.cur].main != MSN_NONE16) {   // основная часть с первого этапа: как переход вступление -> миссия
    M.rt.cp_phase = 1; M.rt.cp_stage = 0; M.rt.cp_timer = 0; M.rt.cp_remaining = 0; msn_retry(&M.rt, 0);
  } else msn_retry(&M.rt, 1);
  logf_("MSN pause-restart %s t=%.1f\n", msn_str(&M.f, M.f.missions[M.rt.cur].id), clock_);
}
void mvPauseAbort(void) {
  if (!active || M.rt.cur < 0) return;
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
void mvPolice(int on) { if (active) gameMsnHnrPolice(&M,on); }
void mvPoliceHold(void) { if (active) gameMsnHnrPoliceHold(&M); }
float mvHnrDecay(void) { return active ? M.hnr.decay : 3.f; }
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
void mvTick(int action, float dt) {
  mvPrefetchPoll();   // разбор тура 12: фоновое чтение моделей персонажей этапа
  if (!active) return;
  clock_ += dt; fadeTick(dt);
  msnartPump();   // записи msnart.shr, заказанные LoadP3DFile, дочитываются фоном
  GamePad p; memset(&p, 0, sizeof p); if (action) p.hit = GP_TRIANGLE;
  static float tickMax;   // служебное: самый долгий кадр логики миссий (с машинами миссии) за секунду — в строке MCAR
  { unsigned tq = sceKernelGetSystemTimeLow(); gameMsnTick(&M, &p, dt); float ms = (sceKernelGetSystemTimeLow() - tq) / 1000.f; if (ms > tickMax) tickMax = ms; }
  { static char fcLast[16]; const char *fc = gameMsnForcedCar(&M);   // SetForcedCar: смена машины игрока — строка журнала
    if (strcmp(fc ? fc : "", fcLast)) { snprintf(fcLast, sizeof fcLast, "%s", fc ? fc : ""); logf_("MSN forced-car %s t=%.1f\n", fc ? fc : "-", clock_); } }
  if (logFn && M.cars.n && (carLogT += dt) >= 1.f) {
    carLogT = 0.f; int racers = 0, pos = gameMsnRacePos(&M, &racers);
    char buf[1200]; int n = 0;   // все машины — одной записью в журнал (запись на карту памяти — одна за секунду, не по машине)
    for (int i = 0; i < M.cars.n && n < (int)sizeof buf - 200; i++) { const McCar *c = &M.cars.c[i]; if (!c->on) continue;
      n += snprintf(buf + n, sizeof buf - n, "MCAR t=%.1f name=%s mode=%s x=%.1f z=%.1f v=%.1f wp=%d/%d fin=%d dmg=%.2f lane=%.1f geo=%d con=%d pos=%d/%d tickms=%.1f replans=%u\n", clock_, c->name, mcModeName[c->mode],
                    c->car.x, c->car.z, c->car.v, c->wp, M.cars.nwp * M.cars.laps, c->finished, c->car.damage, c->offLane, c->geoOk, c->conOk, pos, racers, tickMax, c->replans); }
    if (n) logFn(buf);
    tickMax = 0.f;
  }
}

// ------------------------------------------------------------------ 3D
int mvDollarOld;   // правка 329: 1 — без значка доллара над продавцами (прежнее, красный контроль)
// Low city terrain (L3/L6) is still outdoors. Interior membership comes from
// SHI1 floors, with an explicit script room taking precedence for NPCs.
static int pointVisible(float x, float y, float z, const char *explicitRoom) {
  const char *room = explicitRoom && *explicitRoom ? explicitRoom : npcRoomAt ? npcRoomAt(x, y, z) : NULL;
  return room ? M.interior && msn_streq(room, M.interior) : !M.interior;
}
#ifndef NPC_FAR_POSE_OLD
#define NPC_FAR_POSE_OLD 0
#endif
static int npcVisible(const GameNpc *n, float ex, float ey, float ez, float far) {
  float dx = n->pos.x - ex, dy = n->pos.y - ey, dz = n->pos.z - ez;
  if (dx * dx + dy * dy + dz * dz > far * far) return 0;
  if (clipGuardBody(ex, ey, ez, n->pos.x, n->pos.y, n->pos.y + 1.9f, n->pos.z, .35f, CAM_HIDE_NPC)) return 0;   // №502: персонаж у самого глаза не рисуется
  return pointVisible(n->pos.x, n->pos.y, n->pos.z, n->interior);
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
static int homerLine = -1;
static int tlLine = -1, tlPc; static float tlJawH, tlJawN;   // служебное: итог реплики — рот (TALKLINE)
static void talkHomer(int line, int pcSpeaks) {   // Гомер: жест своей реплики и рот (viewer/homer.c homerTalk / homerJaw)
  Game *g = M.g; if (!g || !*g->pHaveHomer) return;
  Homer *Hm = g->pHomer;
  if (line >= 0) {
    if (line != homerLine) { homerLine = line; homerTalk(Hm, gameMsnTalkAnim(&M, 0, line)); }
    homerJaw(Hm, pcSpeaks ? talkJaw(talkClock) : 0.f);
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
  talkClock += dt; talkHomer(line, pcSpeaks);
  if (line >= 0 && logFn && (int)(clock_ * 2.f) != talkLogT) {   // где стоят, куда смотрят, кто говорит, жесты
    talkLogT = (int)(clock_ * 2.f); const GameNpc *tn = M.talk.npc[0] ? gameMsnNpc(&M, M.talk.npc) : 0; const Homer *Hm = M.g->pHomer;
    NpcAnim *ta = tn ? animFor(npcKey(tn), 0, 0) : 0;
    logf_("TALK t=%.1f npc=%s line=%d pc=%d homer=%.2f,%.2f,%.3f npcpos=%.2f,%.2f,%.3f dist=%.2f gest=%s,%s jaw=%.3f,%.3f hclip=%d\n", clock_,
          M.talk.npc[0] ? M.talk.npc : "-", line, pcSpeaks, Hm->x, Hm->z, Hm->h, tn ? tn->pos.x : 0.f, tn ? tn->pos.z : 0.f, tn ? tn->h : 0.f,
          tn ? hypotf(Hm->x - tn->pos.x, Hm->z - tn->pos.z) : -1.f, gameMsnTalkAnim(&M, 0, line) ? gameMsnTalkAnim(&M, 0, line) : "none",
          gameMsnTalkAnim(&M, 1, line) ? gameMsnTalkAnim(&M, 1, line) : "none", Hm->jawOpen, ta ? ta->p.jawOpen : 0.f, Hm->clip == Hm->cTalk && Hm->cTalk >= 0);
  }
  for (int i = 0; i < M.nnpc; i++) {
    const GameNpc *n = &M.npc[i]; NpcAnim *a = animFor(npcKey(n), 0, 0);
    if (!a) continue;
    int clip = (n->walking || n->homing == 1) && !n->blocked && a->walk >= 0 ? a->walk : a->idle, loop = 1;   // №510: путь перекрыт и обойти нельзя — клип покоя, не ходьба на месте
    if (n->knock == TRP_FLY && a->flail >= 0) clip = a->flail;   // правка 240: сбит — как прохожий трафика (traffic.c pedPlay)
    else if (n->knock == TRP_DOWN) { clip = a->land >= 0 ? a->land : a->flail >= 0 ? a->flail : a->idle; loop = 0; }
    else if (n->knock == TRP_GETUP && a->getup >= 0) { clip = a->getup; loop = 0; }
    if (line >= 0 && msn_streq(n->name, M.talk.npc)) {   // собеседник: жест своей реплики, рот, когда говорит он
      if (line != a->talkLine) { const char *g = gameMsnTalkAnim(&M, 1, line); a->talkLine = line; a->gest = g ? skinFindClip(a->m, g) : -1; a->gestT = 0.f; }
      a->gestT += dt;
      if (a->gest >= 0 && a->gestT < skinClipLength(a->m, a->gest)) { clip = a->gest; loop = 0; }
      a->p.jawOpen = pcSpeaks ? 0.f : talkJaw(talkClock + .37f);
      if (a->p.jawOpen > tlJawN) tlJawN = a->p.jawOpen;
    } else { a->talkLine = a->gest = -1; a->p.jawOpen = 0.f; }
    if (clip >= 0) skinPlay(&a->p, clip, loop, .2f);
    if (!npcVisible(n, ex, ey, ez, (!NPC_GOV_OLD && mvGov && n->knock == 0 && !(line >= 0 && msn_streq(n->name, M.talk.npc))) ? NPC_GOV_DIST : NPC_DRAW_DISTANCE)) { skinTick(&a->p, dt); continue; }
    // Правка Роба 500: персонаж вне пирамиды кадра (за спиной камеры, сбоку) — только время клипа, без позы, палитры и рисования: картинка та же, GE всё равно
    // обрезал бы. Рамка с запасом на полёт сбитого. NPC_VIS_OLD=1 — прежнее
    if (!NPC_VIS_OLD && mvVis) { float bb[6] = {n->pos.x - 1.5f, n->pos.y - .3f, n->pos.z - 1.5f, n->pos.x + 1.5f, n->pos.y + 3.f, n->pos.z + 1.5f};
      if (!mvVis(bb)) { skinTick(&a->p, dt); continue; } }
    // Правка Роба 500 (оптимизация отрисовки, персонажи): дальше 30 м поза (сэмпл, смешивание, палитра костей) — через кадр, время клипа идёт каждый кадр, как у
    // прохожих трафика (traffic.c, 25 м). Собеседник и сбитый пешеход — каждый кадр (рот, полёт). NPC_FAR_POSE_OLD=1 — прежнее
    { float fx = n->pos.x - ex, fz = n->pos.z - ez;
      if (!NPC_FAR_POSE_OLD && fx * fx + fz * fz > 30.f * 30.f && !(line >= 0 && msn_streq(n->name, M.talk.npc)) && n->knock == 0 && (a->poseTick++ & 1)) skinTick(&a->p, dt);
      else skinUpdate(&a->p, dt); }
    float lt_ = skinFootLift(a->m, a->p.clip); if (!a->liftInit) { a->lift = lt_; a->liftInit = 1; } else a->lift += (lt_ - a->lift) * fminf(1.f, dt * 10.f);   // №510: стопы на полу (сглажено при смене клипа)
    ScePspFMatrix4 w; skinPlace(&w, n->pos.x, n->pos.y + a->lift, n->pos.z, n->h);
    skinDraw(&a->p, &w);
  }
  drawDrivers(ex, ey, ez, dt);
}
// №748 (Роб 17.10, TEST31: «всем людям такую же тень… как минимум Мардж, все герои»): кто из персонажей миссий рисуется в этом кадре (дальность, комната,
// не у самого глаза — npcVisible) — точки ног и рост модели для векторной тени
int mvNpcFeet(float (*out)[4], int max, float ex, float ey, float ez, float r) {
  if (!active) return 0;
  int k = 0;
  for (int i = 0; i < M.nnpc && k < max; i++) {
    const GameNpc *n = &M.npc[i]; float dx = n->pos.x - ex, dz = n->pos.z - ez; if (dx * dx + dz * dz > r * r) continue;   // проверка 17.10: сначала дальность, потом поиск модели по имени
    NpcAnim *a = animFor(npcKey(n), 0, 0);
    if (!a || !a->m || !a->m->h || !npcVisible(n, ex, ey, ez, r)) continue;
    float ht = a->m->h->bbox[4] - a->m->h->bbox[1];
    out[k][0] = n->pos.x; out[k][1] = n->pos.y; out[k][2] = n->pos.z; out[k][3] = ht > .3f && ht < 4.f ? ht : 1.84f; k++;
  }
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
static void drawRouteArrow(float ex, float ey, float ez, float t) {
  if (forceArrowOn) { routeOn = 1; routeP[0] = forceArrowP[0]; routeP[1] = forceArrowP[1]; routeP[2] = forceArrowP[2]; routeH = forceArrowP[3]; routeT = 1e9f; }
  else
  if (!H.markerOn || M.interior || M.rt.status != MSN_RUNNING || !pointVisible(H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, NULL)) { routeOn = 0; return; }
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
  if (raceProps[0] && !M.interior) msnartDrawCull(raceProps, NULL, ex, ey, ez, t, 70.f);   // правка 122: шины и отбойники трассы гонки
  if (H.markerOn && H.marker.drawable && *H.marker.drawable) {
    int ident = M.rt.obj == MSN_OBJ_INTERIOR;   // вход в здание: звезда двери смоделирована в координатах мира (kwk_door.p3d)
    if (ident || pointVisible(H.marker.pos.x, H.marker.pos.y, H.marker.pos.z, NULL)) {
      msnartDraw(H.marker.drawable, ident ? 0 : &H.marker.pos.x, ex, ey, ez, t);
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
    if (tgt && (M.rt.obj == MSN_OBJ_DESTROY || M.rt.obj == MSN_OBJ_DUMP)) arrow = "arrow_destroy";
    else if (tgt && M.rt.obj == MSN_OBJ_FOLLOW) arrow = "arrow_chase";
    else if (tgt && M.rt.obj == MSN_OBJ_LOSETAIL) arrow = "arrow_evade";
    else if (M.rt.obj == MSN_OBJ_RACE) arrow = "arrow_race";
    if (!arrow) continue;
    float dx = c->car.x - ex, dz = c->car.z - ez; if (dx * dx + dz * dz > 150.f * 150.f) continue;
    float p[3] = {c->car.x, c->car.y + c->car.p.bodyHi + .6f, c->car.z}; msnartDraw(arrow, p, ex, ey, ez, t);
  }
  // Правка 180 (близнец О4 03.10, кадры f03600 и f04600): над персонажем бонус-миссии — её значок (AddNPCCharacterBonusMission,
  // 5-й аргумент: checkered — уличная гонка, dice — гонка на ставку, exclamation — бонус-миссия), под ним — светлое пятно
  // <значок>_shadow (missgen.p3d), пока миссия не пройдена. Дальше 100 м не рисуется (правило порта)
  unsigned k3_ = sceKernelGetSystemTimeLow(); mvMkUs[3] += k3_ - k2_;
  if (!M.interior) for (int i = 0; i < M.nnpc; i++) {
    const GameNpc *n = &M.npc[i]; int mi = n->bonus - 1;
    if (mi < 0 || (uint32_t)mi >= M.f.nmissions || mi >= 32 || ((M.rt.completed & (1u << mi)) && !M.rt.dev_bonus_replay)) continue;
    const MsnMission *ms = &M.f.missions[mi]; if (ms->bonus_npc == 0xFFFFFFFFu) continue;
    const char *icon = msn_arg_s(&M.f, &M.f.cmds[ms->bonus_npc], 4); if (!icon || !*icon) continue;
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
  if (!brOld && !H.brLoaded) { if (hit & PSP_CTRL_CROSS) H.brQueued = 1; return; }   // правка 52: ✕ во время LOADING — после загрузки
  if (hit & PSP_CTRL_CROSS) { H.briefing = 0; uiBriefAnim(0); logf_("MSN briefing-continue t=%.1f\n", clock_); if (M.mg.hud.objective && *M.mg.hud.objective) uiObjectiveCb(&M, M.mg.hud.objective, M.mg.hud.objective_msg, M.mg.hud.icon);
    // правка 345: [PS2] через 3 кадра после ✕ Continue — реплика водителя на старте миссии W_Mstart_<код>_* (3 из 3, выбор случайный)
    if (sndSay(SND_SAY_MSTART, 1)) logf_("MSN say mstart %s t=%.1f\n", sndSayLastName(), clock_); }
  else if (brOld && (hit & PSP_CTRL_TRIANGLE)) { H.briefing = 0; logf_("MSN briefing-cancel t=%.1f\n", clock_); gameMsnAbort(&M); }   // только прежняя карточка (отр. контроль): в эталоне PS2 «Cancel Mission» на карточке нет
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
  (void)dt;
  if (!active) return;
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
    uiSetBriefing(H.title, H.info, pix, tw, th, w, h);
    if (!H.brLoaded && briefReady()) { H.brLoaded = 1; logf_("MSN briefing-loaded path=%s ra_wait=%.2f wait=%.2f first=%d queued=%d pic=%s lost=%d title=\"%s\" t=%.1f\n", H.brFirst ? "newspaper" : "card-only", brReadyT < 0.f ? 0.f : briefSec() - brReadyT, briefSec(), H.brFirst, H.brQueued, H.present[0] ? H.present : "-", H.brPicLost, uiBriefTitle(), clock_);
      if (!BRIEF_ANIM_AT_LOAD_OLD) uiBriefAnim(1);   // №612: въезд карточки и руки — с момента, когда карточку видно (до этого экран газеты LOADING); на видео TEST17 въезд проходил под газетой
      if (H.brQueued) { H.brQueued = 0; mvBriefingInput(PSP_CTRL_CROSS); } }
    float bs = briefSec(); int blinkOn = (int)(bs / BRIEF_BLINK) % 2 == 0;
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_PRESS_START, H.brLoaded);                 // ✕ Continue
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_GAMBLE, H.brWager && H.brLoaded);       // строки ставки — вместе с текстом карточки
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_LOADING, !H.brLoaded && blinkOn);         // LOADING мигает
    uiShow(UI_SCREEN_MISSIONLOAD, UI_T_SKIP, brOld);                             // Cancel Mission — только прежняя карточка
  }
  MVPF(1);
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
  MVPF(6);
  const McCar *tc = M.rt.status == MSN_RUNNING ? gameMsnTargetCar(&M) : 0; int obj = M.rt.obj;
  uiSetDamage(tc && (obj == MSN_OBJ_DESTROY || obj == MSN_OBJ_DUMP) ? tc->car.damage : 0.f);
  float lim = 0.f;
  if (tc && obj == MSN_OBJ_LOSETAIL) lim = M.rt.objdist > 0.f ? M.rt.objdist : 150.f;
  if (tc && obj == MSN_OBJ_FOLLOW) for (int k = 0; k < M.rt.st->ncond; k++) {
    const MsnCond *cd = &M.f.conds[M.rt.st->cond_first + k];
    for (unsigned i = 0; cd->type == MSN_COND_FOLLOWDISTANCE && i < cd->cmd_count; i++) {
      const MsnCmd *c = &M.f.cmds[cd->cmd_first + i]; if (c->op == MSN_OP_SETFOLLOWDISTANCES) lim = msn_arg_f(&M.f, c, 1); } }
  if (lim > 0.f) { MsnWorld w; gameMsnWorld(&M, &w); float d = hypotf(w.player.x - tc->car.x, w.player.z - tc->car.z) / lim;
    uiHudSet(UI_F_PROXIMITY, (int)(fminf(1.f, d) * 1000.f)); }
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
  if (act == 0 || act == 1) {
    H.failed = 0; uiHudShow(UI_T_MISSION_FAILED, 0);
    logf_("MSN %s t=%.1f\n", act ? "abort" : "retry", clock_);
    if (active) { if (act) gameMsnAbort(&M); else msn_retry(&M.rt, 0); }
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
static void mvSwitchTo(int mi, const char *why) {   // смена миссии из меню / сценария: прежняя цель и её маркер уходят, новая стартует с начала вступления
  const char *from = M.rt.cur >= 0 && (unsigned)M.rt.cur < M.f.nmissions ? msn_str(&M.f, M.f.missions[M.rt.cur].id) : "-";
  if (!msnSwitchOld) msn_switch_clear(&M.rt);
  else { MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e); }   // прежнее: только CLEANUP
  logf_("MSNSWITCH why=%s from=%s to=%s marker=%d gmarker=%d reg=%d obj=\"%.40s\" talk=%d briefing=%d t=%.1f\n", why, from, msn_str(&M.f, M.f.missions[mi].id), H.markerOn,
        M.mg.marker_on, msn_reg_mission_count(&M.rt), M.mg.hud.objective ? M.mg.hud.objective : "", H.talk[0] != 0, H.briefing, clock_);
  msn_start_mission(&M.rt, mi, 0);
}
static int msnSelStatus(int i) { return i < 0 ? 0 : (M.rt.completed >> i & 1u) ? 2 : i == M.rt.story ? 1 : 0; }
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
  for (int k = 0; k < 7; k++) { int st = msRowStatus(k); uiHudSet(UI_F_MS0 + k, st); uiScrEnable(UI_PX_MISSIONSELECT, k, st > 0); }
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
int mvMissionSelect(int on, unsigned hit) {
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
  if (!strcmp(flag, "uitalkoff")) { M.talk.off = 1; logf_("TALKOFF (отрицательный контроль: разговор не ставится)\n"); return; }   // правки 26, 38
  if (!strcmp(flag, "uipulsetrace")) { uiTrace(UI_TRACE_PULSE); return; }   // правка 41: рамка △ покадрово, без служебного показа
  if (!strcmp(flag, "uinovoice")) { abNoVoice = 1; logf_("VOICE off (отрицательный контроль)\n"); return; }
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
void mvServiceMainAt(int code) {   // служебное (правка 302): основная часть миссии code/100 с этапа code%100 (как чекпоинт этапа)
  int ph0 = code < 0; if (ph0) code = -code;   // отрицательный код — вступительная часть (фаза 0)
  int mi = code / 100, st = code % 100;
  if (!active || mi < 0 || (unsigned)mi >= M.f.nmissions || M.f.missions[mi].main == MSN_NONE16) return;
  logf_("MSN mainat %s stage=%d t=%.1f\n", msn_str(&M.f, M.f.missions[mi].id), st, clock_);
  MsnEvent e; memset(&e, 0, sizeof e); e.type = MSN_EV_CLEANUP; M.rt.host.event(M.rt.host.user, &e);
  M.rt.cp_phase = ph0 ? 0 : 1; M.rt.cp_stage = st; msn_start_mission(&M.rt, mi, 1);
  if (ph0) M.rt.story = mi;   // прогулка — у сюжетной миссии (как в игре; правка 330: SAVE GAME проверяет story)
}
void mvServiceUfo(void) { logf_("MSNUFO on=%d pos=%.1f,%.1f,%.1f charge=%.1f sent=%d old=%d ship=%d beam=%d drawn=%u t=%.1f\n", active ? M.ufoOn : 0, M.ufoPos[0], M.ufoPos[1], M.ufoPos[2], M.ufoCharge, M.ufoSent, gameBoss967Old, msnartHas("spaceship"), msnartHas("UfoBeam"), ufoDrawn, clock_); }   // №967: состояние тарелки для гейта tests/missions/boss_l7.sh
void mvServiceHud(void) {   // №943: что миссия считает целью сейчас (stage — этап рантайма), что держит HUD (obj — текст цели) и маркер; строки сверяет tests/msnhome/check.py
  { int ic, px; const char *t, *l; uiObjDebug(&ic, &px, &t, &l); logf_("MSNICON want=\"%s\" field=%d pix=%d pixok=%d home=%d msg=\"%.30s\" last=\"%.30s\" active=%d t=%.1f\n", msnartWantIcon(), ic, px, msnartIconPixOk(), uiHomeShown(), t, l, active, clock_); }   // №958б
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
int mvTalkPrompt(void) { return !mvTalkActive() ? UI_LB_NONE : talkIsStart() ? UI_LB_START : UI_LB_SKIP; }
int mvTalkInput(unsigned hit) {
  if (!mvTalkActive()) return 0;
  int start = talkIsStart();
  if (hit & PSP_CTRL_CROSS) {   // SKIP / Start Mission: разговор кончается сразу — дальше брифинг или следующий этап
    logf_("TALKSKIP start=%d line=%d t=%.1f\n", start, TC.line, clock_);
    msnAudioStopVoice(&M.audio); msn_notify(&M.rt, MSN_IN_DIALOGUE_DONE, 0, 0);
  } else if (start && (hit & PSP_CTRL_TRIANGLE)) {   // Cancel: миссия не начинается, назад к её вступлению
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
  const char *targetRoom = npcRoomAt ? npcRoomAt(p.x, p.y, p.z) : NULL;
  if (rt->obj != MSN_OBJ_INTERIOR && targetRoom && pointVisible(p.x, p.y, p.z, NULL) && gameOnFoot(g)) {   // та же настоящая комната:
    Homer *H = g->pHomer; float dx = H->x - p.x, dz = H->z - p.z, d = sqrtf(dx * dx + dz * dz);   // по линии от Гомера к цели,
    if (d < .01f) { dx = 1.f; dz = 0.f; d = 1.f; }                                                  // в 1,8 м от неё, лицом к ней
    float k = (1.8f + far) / d; homerPlace(H, NULL, p.x + dx * k, H->y, p.z + dz * k, atan2f(-dx, -dz));
    logf_("MSNGO room obj=%d to %.1f %.1f %.1f far=%.1f\n", rt->obj, p.x, p.y, p.z, far); return;
  }
  if (rt->obj == MSN_OBJ_INTERIOR || targetRoom) { logf_("MSNGO room obj=%d (комнаты не в этой сборке)\n", rt->obj); return; }
  float off = 1.5f;   // правка 345: к собеседнику — в его радиус разговора (1,3 м по умолчанию), как подошёл бы игрок
  if (rt->obj == MSN_OBJ_TALKTO) { float r = msn_talk_reach(rt, rt->c_talk) - .4f; if (r < off) off = r; }
  if (gameOnFoot(g)) homerPlace(g->pHomer, g->pWorld, p.x + off + far, p.y + .5f, p.z, g->pHomer->h);
  else {   // машина: на far м к югу от цели, носом к ней (снимок видит маркер впереди)
    float cx = p.x, cz = p.z - far, h = far > 0.f ? atan2f(p.x - cx, p.z - cz) : g->pCar->h;
    carPlace(g->pCar, g->pWorld, cx, p.y + 1.f, cz, h);
  }
  logf_("MSNGO obj=%d to %.1f %.1f %.1f far=%.1f\n", rt->obj, p.x, p.y, p.z, far);
}
