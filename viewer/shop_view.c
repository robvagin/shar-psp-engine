// shop_view.c — экран покупки у продавца машин (Scrooby PurchaseRewards.scr), см. shop_view.h.
#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "shop_view.h"
#include "ui.h"
#include "blackbox.h"   // правка 306: открытие лавки — в чёрный ящик
#include "frontend3d.h"
#include "sound.h"

// метки и поля экрана purchase (tools/shop_export.py, шапка): те же номера, что в пакете shop.shr
enum { SV_T_LOCKED = UI_T_GAMBLE, SV_T_STATS = UI_T_STREETRACE, SV_T_ARROWS = UI_T_SKIP };
enum { SV_ROW_CAR = 0, SV_ROW_VIEW = 1, SV_ROW_STARS = 4 };
enum { SV_FLY_CAR = 0, SV_FLY_EMPTY = 1 };   // перелёты FLT1 сцены: оборот с машиной / без неё

// времена экрана — по записи PS2 (14:03.6–14:09.5, кадры 10/с): правила порта, в данных времени нет
// Сверка g09a-hud (круг 1, M2:34 / Д11): свет лавки открывается и закрывается линейно по 0,5 с, затемнение до 85 %. SHOP_LIGHT_SV_OLD=1 — прежнее (0,4 / 0,3 с, 80 %)
#ifndef SHOP_LIGHT_SV_OLD
#define SHOP_LIGHT_SV_OLD 0
#endif
// Сверка g09a-hud (круг 1, M2:Д9): счёт на экране при покупке убывает 1,0 с десятью шагами по 0,1 с (пересчёт по времени). SHOP_COUNT_SV_OLD=1 — плавно
#ifndef SHOP_COUNT_SV_OLD
#define SHOP_COUNT_SV_OLD 0
#endif
#define SV_DARK_HOLD .6f     // после закрытого ириса сцена тёмная (ирис раскрывается над ней, текст уже виден)
#define SV_LIGHT_IN  (SHOP_LIGHT_SV_OLD ? .4f : .5f)     // свет зажигается; с его началом — машина и счёт монет
#define SV_DARK_A    (SHOP_LIGHT_SV_OLD ? .8f : .85f)    // тёмная сцена видна тускло (шторы, колонны различимы): прозрачность шторки PreviewLightCover
#define SV_COUNT    1.0f     // монеты убывают до новой суммы (151 -> 1 за 1 с), машина уходит с последней монетой
#define SV_EMPTY     .3f     // пустой подиум
#define SV_LIGHT_OUT (SHOP_LIGHT_SV_OLD ? .3f : .5f)     // свет гаснет (и счёт), потом ирис
#define SV_DT_MAX .05f       // шаг часов экрана не больше (длинный кадр ириса, загрузка сцены ◄ ►)

#define SV_DENIED_BLINK 1.2f
static float deniedT;   // правка 306: мигание цены после отказа (мало монет)
static int on, failed, plantOff, phase, curItem = -1, statsOn, carVis, pending = -1, coinsFrom, coinsTo, sndRes;
static float t;
static char dirPath[256];
static int sScroll = -1, sAccept = -1, sBack = -1, sLocked = -1;
static void (*logFn)(const char *);

static void logf_(const char *fmt, ...) {   // строка SHOPV ... t=.. в smoke.txt
  if (!logFn) return;
  char line[200], body[160]; va_list ap; va_start(ap, fmt); vsnprintf(body, sizeof body, fmt, ap); va_end(ap);
  snprintf(line, sizeof line, "SHOPV %s t=%.2f\n", body, t); logFn(line);
}
static void snd(int id) { if (id >= 0) sndPlay2D(id, 1.f); }
static void sounds(void) {
  if (sndRes || !sndReady()) return;
  sScroll = sndId("scroll"); sAccept = sndId("accept"); sBack = sndId("back"); sLocked = sndId("locked_out");   // frontend.spt
  sndRes = 1;
}
static void fly(int vis) { carVis = vis; feFly(vis ? SV_FLY_CAR : SV_FLY_EMPTY, FE_GO); }
static int loadCar(Shop *s, int item) {   // сцена подиума этой машины (reward-<номер среди машин на продажу>.shr); -1 — нет файла
  int k = shopForsaleCar(s, item); if (k < 0) return -1;
  char nm[32], fn[320]; snprintf(nm, sizeof nm, "reward-%d.shr", k); snprintf(fn, sizeof fn, "%s%s", dirPath, nm);
  if (feLoad(fn) != 0) return -1;
  curItem = item; fly(phase == SHOPV_IDLE);   // до света — без машины
  return 0;
}
static void release(Shop *s) {   // экран снят: покупка (если была) засчитана, пакеты освобождены
  if (s && pending >= 0) { int r = shopBuy(s, pending); logf_("commit item=%d r=%d coins=%d", pending, r, shopCoins(s)); }
  pending = -1;
  feFree(); uiScrClose();
  on = 0; curItem = -1; phase = SHOPV_DONE;
}

int shopViewOn(void) { return on; }
int shopViewPhase(void) { return phase; }
void shopViewSetLog(void (*log)(const char *)) { logFn = log; }
void shopViewPlant(int off) { plantOff = off; }

// Будка-93 (правка Роба 1233, T08): лавка при нехватке памяти молча не открывалась (failed=1, ни ступеней, ни слова игроку). Теперь: одна попытка, ступени лестницы (shopViewHeal, ставит main: mem1060Pre),
// вторая попытка, и если снова нет, игроку сообщение (shopViewFailNote: «STORE UNAVAILABLE»), а не тишина. shopViewFailTest N — служебное (flag shopfail N): следующие N открытий пакета отказывают.
// SHOPHEAL1233_OLD=1 в main (shopViewHeal = NULL, shopViewFailNote = NULL) — как было
int (*shopViewHeal)(unsigned need);
void (*shopViewFailNote)(const char *what);
int shopViewFailTest;
int (*shopViewOpenFn)(const char *fn);   // Build 95 П2: открытие пакета лавки ведёт машина переходов (ldrRun, main_collect.h): 0 — открыт; NULL — прежний путь (ступень shopViewHeal)
static int packOpen(const char *fn) {
  if (shopViewOpenFn) return shopViewOpenFn(fn);
  int rc = shopViewFailTest > 0 ? (shopViewFailTest--, uiScrClose(), -3) : uiScrOpen(fn);
  if (rc != 0 && shopViewHeal) { logf_("heal %s", fn); shopViewHeal(1u << 20); rc = shopViewFailTest > 0 ? (shopViewFailTest--, uiScrClose(), -3) : uiScrOpen(fn); }
  return rc;
}
int shopViewTick(Shop *s, const char *dir, float dt) {
  if (deniedT > 0.f) deniedT -= dt;   // правка 306
  if (!s || !s->open) {                 // лавку закрыли не мы (миссия, выход в меню): экран снимается сразу
    if (on) release(s);
    failed = 0; return SHOPV_EV_NONE;
  }
  if (!on) {
    if (failed || plantOff) return SHOPV_EV_NONE;
    if (uiScrReady()) { failed = 1; logf_("busy"); return SHOPV_EV_NONE; }   // второй пакет занят
    snprintf(dirPath, sizeof dirPath, "%s", dir ? dir : "");
    char fn[320]; snprintf(fn, sizeof fn, "%sshop.shr", dirPath);
    phase = SHOPV_IN; t = 0.f; statsOn = 0; pending = -1;
    if (packOpen(fn) != 0) { failed = 1; logf_("nopack %s", fn); if (shopViewFailNote) shopViewFailNote("STORE UNAVAILABLE"); return SHOPV_EV_NONE; }
    { int cnt_ = shopSellerCount(s, s->at); if (cnt_ > 0 && s->sel >= cnt_) s->sel = 0; }   // сверка g09a-hud: список продавца стал короче (купленное ушло)
    if (loadCar(s, shopSellerItem(s, s->at, s->sel)) != 0) { uiScrClose(); failed = 1; logf_("noscene sel=%d", s->sel); return SHOPV_EV_NONE; }
    on = 1; sounds();
    logf_("open seller=%d item=%d coins=%d name=%s", s->at, curItem, shopCoins(s), s->item[curItem].name);
    bbOp("shop", s->seller[s->at].npc, 0);   // правка 306: на PSP видно, когда △ открыл лавку (а не разговор миссии)
    return SHOPV_EV_OPEN;
  }
  t += dt < SV_DT_MAX ? dt : SV_DT_MAX;   // кадр после ириса несёт всё время его закрытия (transitClose держит кадр) — не в счёт темноты
  int it = shopSellerItem(s, s->at, s->sel);
  if (it != curItem && it >= 0 && phase == SHOPV_IDLE) {   // ◄ ►: сцена другой машины
    if (loadCar(s, it) != 0) { logf_("noscene item=%d", it); }
    logf_("sel item=%d name=%s", it, s->item[it].name);
  }
  switch (phase) {
    case SHOPV_IN:
      if (!carVis && t >= SV_DARK_HOLD) fly(1);
      if (t >= SV_DARK_HOLD + SV_LIGHT_IN) { phase = SHOPV_IDLE; t = 0.f; logf_("idle"); }
      break;
    case SHOPV_BOUGHT:
      if (carVis && t >= SV_COUNT) fly(0);                  // машина уходит с последней монетой
      if (t >= SV_COUNT + SV_EMPTY) { phase = SHOPV_OUT; t = 0.f; }
      break;
    case SHOPV_OUT:
      if (t >= SV_LIGHT_OUT) { phase = SHOPV_DONE; logf_("done bought=%d coins=%d", pending >= 0, pending >= 0 ? coinsTo : shopCoins(s)); return SHOPV_EV_CLOSE; }
      break;
  }
  return SHOPV_EV_NONE;
}

void shopViewEnd(Shop *s) {
  release(s);
  if (s) s->open = 0;
  logf_("end coins=%d open=%d", s ? shopCoins(s) : -1, s ? s->open : -1);
}

static float coverAlpha(void);
void shopViewInput(Shop *s, unsigned hit) {
  if (!SHOP_LIGHT_SV_OLD && on && s && phase == SHOPV_IN && (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE))) {   // Д11: закрытие в разгар открытия идёт с текущей точки света
    float a_ = coverAlpha(); snd(sBack); phase = SHOPV_OUT; t = SV_LIGHT_OUT * (a_ > SV_DARK_A ? 1.f : a_ / SV_DARK_A); logf_("cancel-mid"); return;
  }
  if (!on || !s || phase != SHOPV_IDLE) return;
  int it = shopSellerItem(s, s->at, s->sel);
  if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { snd(sBack); phase = SHOPV_OUT; t = 0.f; logf_("cancel"); return; }
  if (hit & PSP_CTRL_LEFT) { shopStep(s, -1); snd(sScroll); logf_("step -1 sel=%d", s->sel); }   // №706: шаг в журнал (гейт повтора)
  if (hit & PSP_CTRL_RIGHT) { shopStep(s, 1); snd(sScroll); logf_("step 1 sel=%d", s->sel); }
  if (hit & PSP_CTRL_SQUARE) { statsOn ^= 1; snd(sScroll); logf_("stats=%d", statsOn); }   // □ Show Stats / Hide Stats
  if ((hit & PSP_CTRL_CROSS) && it >= 0) {
    const ShopItem *m = &s->item[it];
    if (shopOwns(s, m->kind, m->name) || shopCoins(s) < m->price) {   // куплено или мало монет: покупки нет
      snd(sLocked >= 0 ? sLocked : sBack); deniedT = SV_DENIED_BLINK;   // правка 306: цена мигает — видно, что не хватает монет
      logf_("denied item=%d price=%d coins=%d owned=%d", it, m->price, shopCoins(s), shopOwns(s, m->kind, m->name)); return;
    }
    // покупка: списание и «куплено» — после ириса (shopViewEnd), когда игра снова идёт; здесь — отсчёт монет на экране
    pending = it; coinsFrom = shopCoins(s); coinsTo = coinsFrom - m->price;
    phase = SHOPV_BOUGHT; t = 0.f; snd(sAccept);
    logf_("buy item=%d price=%d coins=%d name=%s", it, m->price, coinsTo, m->name);
  }
}

void shopViewDraw3D(void) {
  if (!on) return;
  sceGuClearColor(0xff000000); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  if (!feFlying()) feFly(carVis ? SV_FLY_CAR : SV_FLY_EMPTY, FE_GO);   // оборот по кругу
  feDraw(-1);
}

static float coverAlpha(void) {   // шторка PreviewLightCover: свет выключен / зажигается / гаснет
  switch (phase) {
    case SHOPV_IN: return t < SV_DARK_HOLD ? SV_DARK_A : SV_DARK_A * (1.f - (t - SV_DARK_HOLD) / SV_LIGHT_IN);
    case SHOPV_OUT: return SV_DARK_A * (t / SV_LIGHT_OUT > 1.f ? 1.f : t / SV_LIGHT_OUT);
    case SHOPV_DONE: return SV_DARK_A;
    default: return 0.f;
  }
}

typedef struct { unsigned color; short x, y, z, pad; } SvVert;
void shopViewDraw2D(Shop *s) {
  if (!on || !s) return;
  float a = coverAlpha(); if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
  if (a > 0.f) {   // чёрная шторка во весь экран, как PreviewLightCover (880x530 от -120: шире кадра 4:3)
    SvVert *v = (SvVert *)sceGuGetMemory(2 * sizeof(SvVert)); unsigned c = (unsigned)(a * 255.f + .5f) << 24;
    v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = 480; v[1].y = 272; v[0].z = v[1].z = 0;
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  }
  int it = curItem >= 0 ? curItem : shopSellerItem(s, s->at, s->sel);
  const ShopItem *m = it >= 0 ? &s->item[it] : 0;
  uiScrSetRow(SV_ROW_CAR, it >= 0 ? shopForsaleCar(s, it) : 0);
  uiScrSetRow(SV_ROW_VIEW, statsOn);
  for (int k = 0; k < 4; k++) { int v = it >= 0 ? shopStars(s, it, k) : 0; uiScrSetRow(SV_ROW_STARS + k, v < 0 ? 0 : v); }
  int lit = (phase == SHOPV_IN && t >= SV_DARK_HOLD) || phase == SHOPV_IDLE || phase == SHOPV_BOUGHT;
  float cf = t < SV_COUNT ? t / SV_COUNT : 1.f; if (!SHOP_COUNT_SV_OLD) cf = floorf(cf * 10.f + 1e-4f) / 10.f;   // шаги по 0,1 с
  int coins = phase == SHOPV_BOUGHT ? coinsFrom - (int)((coinsFrom - coinsTo) * cf + .5f)
            : phase >= SHOPV_OUT && pending >= 0 ? coinsTo : shopCoins(s);
  uiSetCoins(coins);
  uiScrShow(0, UI_T_COINS, lit);
  // правка Роба 306 (04.10, m3 «купи Plow King у Барни» — 150 монет по rewards.mfk): нажал ✕, монет мало — звук locked_out и цена
  // «N coins to purchase» мигает SV_DENIED_BLINK с (правило порта; иначе покупка молча не проходила — «он только лавка»)
  int blinkOff = deniedT > 0.f && fmodf(deniedT, .3f) < .15f;
  uiScrShow(0, SV_T_LOCKED, m && phase != SHOPV_BOUGHT && !(phase >= SHOPV_OUT && pending >= 0) && !shopOwns(s, m->kind, m->name) && !blinkOff);
  uiScrShow(0, SV_T_STATS, statsOn);
  uiScrShow(0, SV_T_ARROWS, shopSellerCount(s, s->at) > 1);
  uiScrDraw(0, 0, t, 0);
}
