// phone_view.c — телефонная будка (Scrooby PhoneBooth.scr), см. phone_view.h. Шторка и времена — как у лавки (shop_view.c);
// картинка машины — 2D с диска (cars2d, правка 104), номер строки phonecars.txt = ROW0
#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "phone_view.h"
#include "ui.h"
#include "sound.h"
#include "audio.h"
#include <pspiofilemgr.h>
#include "menu_access.h"   // пункт ДОБИВКИ 26: состав и порядок галереи, звёзды и Accept у закрытой (запись PS2 ps2_v105_booth_access)

enum { PV_T_LOCKED = UI_T_GAMBLE, PV_T_STATS = UI_T_STREETRACE, PV_T_ARROWS = UI_T_SKIP, PV_T_DAMAGE = UI_T_DAMAGE,
       PV_T_ACCEPT = UI_T_ACTION };   // ACTION — подсказка Accept (Accept.pag; tools/shop_export.py TAG_ACCEPT)
// Правка Роба 470 (10.10): у повреждённой машины — картинка повреждённой (cars2d/<имя>d.p3d, две картинки в одном месте: метки CARDS — целая,
// PROXIMITY — повреждённая; tools/shop_export.py PHONE_DAMAGED_PIC); полностью разбитую можно взять починенной за монеты: Accept списывает
// PHONE_REPAIR_COST (ПРАВИЛО ПОРТА: цены на экране PS2 нет, запись 37:54), недостаёт монет — как у закрытой, строка под картинкой красная.
// PHONE_DAMAGED_PIC_OLD=1 / PHONE_REPAIR_OLD=1 — прежнее (одна картинка; починка даром)
#ifndef PHONE_DAMAGED_PIC_OLD
#define PHONE_DAMAGED_PIC_OLD 0
#endif
#ifndef PHONE_REPAIR_OLD
#define PHONE_REPAIR_OLD 0
#endif
#ifndef PHONE_REPAIR_COST_OLD
#define PHONE_REPAIR_COST 10   // оригинал: «Cost to Fix: 10 coins» (вики игры, скриншот Роба 17.10); PHONE_REPAIR_COST_OLD — прежние 50 (правило порта)
#else
#define PHONE_REPAIR_COST 50
#endif
#define PHONE_DMG_PIC_MIN 100    // %: повреждённая картинка — только у разбитой целиком (запись PS2: при 16–18 % картинка та же, что у целой)
#define PV_GAL_CX 154            // ось галереи экрана, px (layout_phone.json: имя и «DAMAGED» центрированы на 153,5)
enum { PV_T_PIC_OK = UI_T_CARDS, PV_T_PIC_DMG = UI_T_PROXIMITY, PV_T_SPOT = UI_T_WASPS };   // PV_T_SPOT — свечение под картинкой (tools/shop_export.py TAG_SPOT): под 3D-остовом не рисуется
#ifndef PHONE_REPAIR_TEXT_OLD
#define PHONE_REPAIR_TEXT_OLD 0   // №535: 1 — «REPAIR: 50 COINS» кодом (uiText), как до правки
#endif
enum { PV_T_REPAIR_OK = UI_T_COUNTDOWN, PV_T_REPAIR_NO = UI_T_ITEMS_COMPLETE };   // tools/shop_export.py booth_layout (RepairOk / RepairNo)
enum { PV_ROW_CAR = 0, PV_ROW_DMG = 1, PV_ROW_STARS = 4 };   // ROW1 — tools/shop_export.py PHONE_ROW_DMG
enum { PV_IN = 0, PV_IDLE, PV_OUT, PV_DONE };
#define PV_DARK_HOLD .6f
#define PV_LIGHT_IN  .4f
#define PV_DARK_A    .8f
#define PV_LIGHT_OUT .3f
#define PV_DT_MAX .05f

// галерея (правка 100): build/phonecars.txt — «имя уровень_открытия 4 звезды» по строке (tools/shop_export.py --phone-list):
// машины-награды уровня и машины персонажей AddVehicleSelectInfo; номер строки = картинка (cars2d) = ROW0
// Правка 442 (Роб, 09.10: «машины, открытые на одном уровне, доступны на всех?»; ответ оригинала — окно PS2-Наблюдатель). Два варианта:
//   А (PHONE_ALL_CARS 0 / PHONE_ALL_CARS_OLD 1, как было): галерея — машины каталога ТЕКУЩЕГО уровня (BindReward уровня), чужие не видны
//   Б (по умолчанию с правки 450): все машины-награды всех уровней; открыта, если взята на своём уровне (Coins.owned[уровень открытия], бит =
//     номер BindReward в каталоге того уровня) или это defaultcar того же/более раннего уровня. Данные Б — phonecars.txt и phone.shr,
//     собранные tools/shop_export.py с SHR_PHONE_ALL=1 (строка «имя уровень 4 звезды номер квест»); без этих колонок Б работает как А.
//     Модель чужой машины берётся из msncars текущего уровня (gameMsnPhoneCar); нет модели — вызывается своя, как сейчас
// Правка Роба 450 (10.10, голосом: «все машины, которые приобрёл за игру, доступны в будке на всех уровнях — прикол игры»): вариант Б теперь
// по умолчанию; вариант А (только каталог текущего уровня) — PHONE_ALL_CARS_OLD=1 (и SHR_PHONE_ALL_OLD=1 в экспорте)
#ifndef PHONE_ALL_CARS_OLD
#define PHONE_ALL_CARS_OLD 0
#endif
#ifndef PHONE_ALL_CARS
#define PHONE_ALL_CARS (!PHONE_ALL_CARS_OLD)
#endif
#define PV_MAX 48   // вариант Б: 42 машины BindReward семи уровней + машины персонажей
static char pvName[PV_MAX][16], pvQuest[PV_MAX][16]; static int pvLvl[PV_MAX], pvStars[PV_MAX][4], pvIdx[PV_MAX];
static int ord[PV_MAX], nOrd;   // пункт ДОБИВКИ 26: строки phonecars.txt в порядке показа (cur — место в ord)
static int on, phase, cur, n, chosen = -1, sndRes, sScroll = -1, sAccept = -1, sBack = -1, sLocked = -1;
static float t;
static float raysT;   // №763: часы вращения лучей — t обнуляется на входе в выбор и на выходе
#ifndef RAYS_FADE_OLD
#define RAYS_FADE_OLD 0   // №763: 1 — лучи на часах экрана (угол прыгал при смене фазы) и без затухания, как TEST32
#endif
static char dirPath[256];
#ifndef PHONE_LOAD_WARM_OLD
#define PHONE_LOAD_WARM_OLD 0
#endif
// Правка Роба 474 (10.10): phone.shr (2 МБ при 42 машинах × целая/повреждённая) читается при открытии будки и уходит при выходе, а не живёт в куче весь
// уровень (прежнее: страница прогревалась после загрузки уровня). PHONE_RESIDENT_OLD=1 — прежнее резидентное
#ifndef PHONE_RESIDENT_OLD
#define PHONE_RESIDENT_OLD 0
#endif
static int phoneWarmOld=PHONE_RESIDENT_OLD?PHONE_LOAD_WARM_OLD:1,warmCount;
static char warmDir[256];
static Shop *pvShop;   // магазин с открытия (isNow, №557)
static char carNow[16]; static int dmgPct;   // правка 104: машина игрока сейчас («» — своя машина уровня) и её урон, %

static void (*logFn)(const char *);
static void logf_(const char *fmt, ...) {   // строка PHONEV ... t=.. в smoke.txt (гейт tests/shop/booth_emu.sh)
  if (!logFn) return;
  char line[200], body[160]; va_list ap; va_start(ap, fmt); vsnprintf(body, sizeof body, fmt, ap); va_end(ap);
  snprintf(line, sizeof line, "PHONEV %s t=%.2f\n", body, t); logFn(line);
}
void phoneViewSetLog(void (*log)(const char *)) { logFn = log; }
static void snd(int id) { if (id >= 0) sndPlay2D(id, 1.f); }
// №682: игра пройдена — машины пройденных уровней открыты. Прогресс сохранения (до какого уровня дошла игра) главный цикл ставит перед открытием
// (phoneViewSetProgress): своя машина уровня открыта, если уровень открыт в сохранении (не только текущий); награда — если куплена/взята (Coins.owned)
// или её уровень пройден целиком (ниже достигнутого). PHONE_UNLOCK_OLD=1 — прежнее (по текущему уровню и Coins.owned)
#ifndef PHONE_UNLOCK_OLD
#define PHONE_UNLOCK_OLD 0
#endif
static int pvReached, pvTop;
void phoneViewSetProgress(int reached) { pvReached = reached; }
static int ownedOther(const Shop *s, int k) {   // вариант Б: машина каталога другого уровня (в каталоге текущего её нет)
  if (!PHONE_ALL_CARS || pvIdx[k] < 0 || pvLvl[k] < 1 || pvLvl[k] > 7) return 0;
  int top = !PHONE_UNLOCK_OLD ? pvTop : s->level;
  if (!strcmp(pvQuest[k], "defaultcar")) return pvLvl[k] <= top;                 // своя с начала: уровни, открытые в сохранении
  if (!PHONE_UNLOCK_OLD && pvLvl[k] < pvTop) return 1;                       // уровень пройден: награды взяты
  return s->coins && pvIdx[k] < 32 && ((s->coins->owned[pvLvl[k]] >> pvIdx[k]) & 1u);   // взята/куплена на своём уровне (savegame v2)
}
static void topOf(const Shop *s) {   // №682: старший открытый уровень = max(записанный, текущий, старший, где в сохранении взята хоть одна машина-награда)
  pvTop = pvReached > s->level ? pvReached : s->level;
  if (s->coins) for (int L = 1; L <= 7; L++) if (s->coins->owned[L] && L > pvTop) pvTop = L;
}
static int owned(Shop *s, int k) {   // машина уровня — как у лавки (своя, куплена, выиграна); с другого уровня — закрыта (А) / по Coins.owned (Б)
  if (k < 0 || k >= n) return 0;
  if (shopFind(s, pvName[k]) < 0) return ownedOther(s, k);
  topOf(s);
  if (!PHONE_UNLOCK_OLD && pvLvl[k] >= 1 && pvLvl[k] < pvTop) return 1;   // №682: машина пройденного уровня (награда этого же уровня тоже)
  return shopOwns(s, SHOP_CAR, pvName[k]);
}
static int readList(const char *dir) {
  char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir, "phonecars.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  char buf[4096]; int len = sceIoRead(f, buf, sizeof buf - 1); sceIoClose(f); if (len <= 0) return 0; buf[len] = 0;
  int k = 0; char *line = buf;
  while (line && *line && k < PV_MAX) {
    char *nl = strchr(line, '\n'); if (nl) *nl = 0;
    int l, a, b, c, d, ix = -1; char nm[32], q[16] = "";
    int got = sscanf(line, "%31s %d %d %d %d %d %d %15s", nm, &l, &a, &b, &c, &d, &ix, q);
    if (got == 6 || got == 8) {   // 8 колонок — данные варианта Б (номер в каталоге уровня открытия, квест)
      snprintf(pvName[k], sizeof pvName[k], "%s", nm); pvLvl[k] = l; pvStars[k][0] = a; pvStars[k][1] = b; pvStars[k][2] = c; pvStars[k][3] = d;
      pvIdx[k] = got == 8 ? ix : -1; snprintf(pvQuest[k], sizeof pvQuest[k], "%s", got == 8 ? q : ""); k++; }
    line = nl ? nl + 1 : 0;
  }
  return k;
}
void phoneViewWarmOld(int old){phoneWarmOld=old!=0;if(old){warmCount=0;warmDir[0]=0;uiScrWarmClear();}}
void phoneViewWarm(const char *dir){
  warmCount=0;warmDir[0]=0;if(phoneWarmOld||!dir||strlen(dir)>=sizeof warmDir)return;
  int count=readList(dir);char fn[320];snprintf(fn,sizeof fn,"%sphone.shr",dir);
  if(count&&uiScrWarm(fn)==0){warmCount=count;snprintf(warmDir,sizeof warmDir,"%s",dir);}
}
void phoneViewSetCar(const char *name, float damage) {
  snprintf(carNow, sizeof carNow, "%s", name ? name : "");
  dmgPct = (int)(damage * 100.f + .5f); if (dmgPct < 0) dmgPct = 0; if (dmgPct > 100) dmgPct = 100;
}
static int isNow(Shop *s, int k) {   // k — машина, на которой игрок сейчас: своя вызванная / угнанная (имя) или машина уровня (defaultcar)
  if (k < 0 || k >= n) return 0;
  if (carNow[0]) return !strcmp(pvName[k], carNow);
  int it = shopFind(s, pvName[k]); return it >= 0 && !strcmp(s->item[it].quest, "defaultcar");
}
static int needsRepair(Shop *s, int k) { return !PHONE_REPAIR_OLD && isNow(s, k) && dmgPct >= 100; }   // машина игрока разбита целиком
static int rowOf(int pos) { int r = pos >= 0 && pos < nOrd ? ord[pos] : 0; return r >= 0 && r < n && r < PV_MAX ? r : 0; }   // место в галерее -> строка phonecars.txt (картинка ROW0)
static void buildOrder(Shop *s) {   // menu_access.h boothOrder: каталог уровня по типу награды; машин не из каталога уровня — нет
  int rank[PV_MAX];
  for (int k = 0; k < n; k++) {
    int it = shopFind(s, pvName[k]);
    rank[k] = boothQuestRank(it >= 0 ? s->item[it].quest : PHONE_ALL_CARS && pvIdx[k] >= 0 ? pvQuest[k] : 0);   // Б: квест из данных (машина другого уровня)
  }
  nOrd = boothOrder(n, rank, ord);
}
int phoneViewOn(void) { return on; }
int phoneViewOpen(Shop *s, const char *dir) {
  if (on || !s) return -1;
  pvShop = s;
  snprintf(dirPath, sizeof dirPath, "%s", dir ? dir : "");
  char fn[320]; snprintf(fn, sizeof fn, "%sphone.shr", dirPath);
  int warm=!phoneWarmOld && warmCount && !strcmp(dirPath,warmDir) && uiScrWarmReady(fn);
  n = warm?warmCount:readList(dirPath);
  if (!n || uiScrReady()) return -1;
  if (uiScrOpen(fn) != 0) return -1;
  logf_("load warm=%d",warm);
  buildOrder(s); if (!nOrd) { uiScrClose(); return -1; }
  cur = 0; for (int p = 0; p < nOrd; p++) if (owned(s, ord[p])) { cur = p; break; }
  // №534 (Роб 11.10): будка открывается на машине героя ЭТОГО уровня (L3 — Malibu Stacy Лизы), на которой игрок сейчас, а не на первой своей по порядку галереи (Family Sedan первого уровня). PHONE_START_NOW_OLD=1 — прежнее
#ifndef PHONE_START_NOW_OLD
#define PHONE_START_NOW_OLD 0
#endif
  if (!PHONE_START_NOW_OLD) for (int p = 0; p < nOrd; p++) if (owned(s, ord[p]) && isNow(s, ord[p])) { cur = p; break; }
  if (!sndRes && sndReady()) { sScroll = sndId("scroll"); sAccept = sndId("accept"); sBack = sndId("back"); sLocked = sndId("locked_out"); sndRes = 1; }
  { int m = audioTrack("muzak"); if (m >= 0) audioMusic(m, .3f); }   // событие Store: Store_region -> Muzak
  on = 1; phase = PV_IN; t = 0.f; raysT = 0.f; chosen = -1;
  { char ls[160]; int k = 0; ls[0] = 0; for (int p = 0; p < nOrd && k < (int)sizeof ls - 20; p++) k += snprintf(ls + k, sizeof ls - k, "%s%s", p ? "," : "", pvName[ord[p]]);
    logf_("open n=%d cur=%d name=%s owned=%d now=%d dmg=%d list=%s", nOrd, cur, pvName[rowOf(cur)], owned(s, rowOf(cur)), isNow(s, rowOf(cur)), dmgPct, ls); }
  return 0;
}
int phoneViewTick(Shop *s, float dt) {
  (void)s; if (!on) return PHONEV_EV_NONE;
  t += dt < PV_DT_MAX ? dt : PV_DT_MAX; raysT += dt < PV_DT_MAX ? dt : PV_DT_MAX;
  if (phase == PV_IN && t >= PV_DARK_HOLD + PV_LIGHT_IN) { phase = PV_IDLE; t = 0.f; }
  if (phase == PV_OUT && t >= PV_LIGHT_OUT) { phase = PV_DONE; return PHONEV_EV_CLOSE; }
  return PHONEV_EV_NONE;
}
#ifndef BOOTHLR1038_OLD
#define BOOTHLR1038_OLD 0
#endif
int boothLR1038Old = BOOTHLR1038_OLD;
#ifndef BOOTHLR1057_OLD
#define BOOTHLR1057_OLD 0
#endif
int boothLR1057Old = BOOTHLR1057_OLD;
void phoneViewInput(Shop *s, unsigned hit) {
  if (!on || phase != PV_IDLE) return;
  if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { snd(sBack); phase = PV_OUT; t = 0.f; return; }
  // №1038 (Роб 26.10: «в меню паузы Mission Select уровни переключаются L/R, то же в выборе машины в будке, дополнительно к прежнему»): L — предыдущая машина, R — следующая, как ◄ и ►: тот же звук, смена и порядок. BOOTHLR1038_OLD=1 / файл boothlr1038old / «flag boothlr1038 0» — только ◄ ►
  int d = (hit & (PSP_CTRL_RIGHT | (boothLR1038Old ? 0u : PSP_CTRL_RTRIGGER))) ? 1 : (hit & (PSP_CTRL_LEFT | (boothLR1038Old ? 0u : PSP_CTRL_LTRIGGER))) ? -1 : 0;
  if (d && nOrd > 1) { cur = (cur + d + nOrd) % nOrd; int r = rowOf(cur); snd(sScroll);
                       logf_("sel %d name=%s owned=%d lvl=%d now=%d stats=%d accept=%d", cur, pvName[r], owned(s, r), pvLvl[r], isNow(s, r),
                             boothShowStats(owned(s, r)), boothShowAccept(owned(s, r))); }
  if (hit & PSP_CTRL_CROSS) {
    int r = rowOf(cur);
    if (!owned(s, r)) { snd(sLocked >= 0 ? sLocked : sBack); logf_("deny %d name=%s", cur, pvName[r]); return; }
    if (needsRepair(s, r)) {   // правка 470: починка разбитой — за монеты
      if (shopCoins(s) < PHONE_REPAIR_COST) { snd(sLocked >= 0 ? sLocked : sBack); logf_("deny-repair %d name=%s coins=%d", cur, pvName[r], shopCoins(s)); return; }
      shopCoinsAdd(s, -PHONE_REPAIR_COST, 0); logf_("repair %d name=%s cost=%d", cur, pvName[r], PHONE_REPAIR_COST);
    }
    chosen = r; snd(sAccept); phase = PV_OUT; t = 0.f; logf_("accept %d name=%s", cur, pvName[r]);
  }
}
static void phoneGrad(void);
void phoneViewDraw3D(void) {   // правка 104: в оригинале у будки 2D-картинка машины на странице (PreviewImage) — 3D нет, фон тёмный
  if (!on) return;
  sceGuClearColor(0xff000000); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  phoneGrad();   // №557: плавный фон в 3D-проходе (до остова и до интерфейса)
}
static float coverAlpha(void) {
  switch (phase) {
    case PV_IN: return t < PV_DARK_HOLD ? PV_DARK_A : PV_DARK_A * (1.f - (t - PV_DARK_HOLD) / PV_LIGHT_IN);
    case PV_OUT: return PV_DARK_A * (t / PV_LIGHT_OUT > 1.f ? 1.f : t / PV_LIGHT_OUT);
    case PV_DONE: return PV_DARK_A;
    default: return 0.f;
  }
}
// №569 (Роб 12.10): в будке при 100 % урона — родная 2D-картинка разбитой машины с диска (pcar2d_*d.png, PV_T_PIC_DMG)
// в полном размере колонки (как целые машины после №557). 3D-остов из мира — только флагом PHONE_HUSK3D=1 (или flag phonehusk3d в сценарии).
#ifndef PHONE_HUSK3D
#define PHONE_HUSK3D 0
#endif
static int husk3dForce;
void phoneViewHusk3DForce(int on) { husk3dForce = on; }
int phoneViewHusk(int *r) {   // прямоугольник картинки (px), если в будке сейчас показывается 3D-остов; 0 — нет
  if (!on || (!PHONE_HUSK3D && !husk3dForce) || phase == PV_DONE) return 0;
  int row = rowOf(cur);
  return pvShop && needsRepair(pvShop, row) ? uiScrTagRect(0, PV_T_PIC_DMG, r) : 0;
}
typedef struct { unsigned color; short x, y, z, pad; } PvVert;
// №557 (Роб 11.10: «фон будки — ступенчатый битый градиент»): градиент пакета (BGColour: (90,99,180) вверху -> (37,43,67) внизу, цвета диска) рисовался вершинами GE в кадр 5650 без дизеринга (в будке он выключен —
// на картинках машин давал сетку) и ложился ступенями. Теперь движок сам: текстура 16x512 с упорядоченным дизерингом 4x4, посчитанная ДО 565 (значения 5/6/5 бит, расширенные, GE их не меняет),
// один спрайт на весь экран; эмулятор и железо рисуют одно и то же. Экспорт пакета без BGColour (tools/shop_export.py BOOTH_GRAD). PHONE_GRAD_OLD=1 — как было (пара с PHONE_GRAD_OLD=1 экспорта)
#ifndef PHONE_GRAD_OLD
#define PHONE_GRAD_OLD 0
#endif
typedef struct { float u, v; unsigned color; float x, y, z; } PvTex;
static unsigned pvGrad[16 * 512] __attribute__((aligned(16))); static int pvGradReady;
static void phoneGrad(void) {
  if (PHONE_GRAD_OLD) return;
  if (!pvGradReady) {
    static const unsigned char bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const float c0[3] = {90.f, 99.f, 180.f}, c1[3] = {37.f, 43.f, 67.f};
    for (int y = 0; y < 512; y++) {
      float k = y >= 271 ? 1.f : y / 271.f;
      for (int x = 0; x < 16; x++) {
        float th = (bayer[y & 3][x & 3] + .5f) / 16.f; int q[3];
        for (int c = 0; c < 3; c++) {
          float v = c0[c] + (c1[c] - c0[c]) * k, st = c == 1 ? 4.f : 8.f; int mx = c == 1 ? 63 : 31;
          q[c] = (int)(v / st + th); if (q[c] > mx) q[c] = mx;
        }
        unsigned r = (unsigned)(q[0] << 3 | q[0] >> 2), g = (unsigned)(q[1] << 2 | q[1] >> 4), b = (unsigned)(q[2] << 3 | q[2] >> 2);
        pvGrad[y * 16 + x] = 0xff000000u | b << 16 | g << 8 | r;
      }
    }
    sceKernelDcacheWritebackRange(pvGrad, sizeof pvGrad); pvGradReady = 1;
  }
  PvTex *v = (PvTex *)sceGuGetMemory(2 * sizeof(PvTex));
  v[0] = (PvTex){0.f, 0.f, 0xffffffffu, 0.f, 0.f, 0.f}; v[1] = (PvTex){480.f, 272.f, 0xffffffffu, 480.f, 272.f, 0.f};
  sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST);
  sceGuEnable(GU_TEXTURE_2D); sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 16, 512, 16, pvGrad);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB); sceGuTexFilter(GU_NEAREST, GU_NEAREST); sceGuTexWrap(GU_REPEAT, GU_CLAMP);
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
}
void phoneViewDraw2D(Shop *s) {
  if (!on || !s) return;
  float a = coverAlpha(); if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
  if (a > 0.f) {
    PvVert *v = (PvVert *)sceGuGetMemory(2 * sizeof(PvVert)); unsigned c = (unsigned)(a * 255.f + .5f) << 24;
    v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = 480; v[1].y = 272; v[0].z = v[1].z = 0;
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  }
  int r = rowOf(cur), own = owned(s, r);
  uiScrSetRow(PV_ROW_CAR, r);
  for (int k = 0; k < 4; k++) uiScrSetRow(PV_ROW_STARS + k, pvStars[r][k]);
  uiSetCoins(shopCoins(s));
  uiScrShow(0, UI_T_COINS, phase != PV_DONE);
  uiScrShow(0, PV_T_LOCKED, !own);
  uiScrShow(0, PV_T_STATS, boothShowStats(own));     // PS2: звёзды и у закрытой
  uiScrShow(0, PV_T_ACCEPT, boothShowAccept(own));   // PS2: у закрытой только Back
  uiScrShow(0, PV_T_ARROWS, nOrd > 1);
  uiScrSetRow(PV_ROW_DMG, dmgPct); uiScrShow(0, PV_T_DAMAGE, isNow(s, r));   // правка 104: «Damaged: N %» — запись PS2 37:54
  { int dmgPic = !PHONE_DAMAGED_PIC_OLD && isNow(s, r) && dmgPct >= PHONE_DMG_PIC_MIN;   // правка 470: картинка повреждённой / целой
    int huskR[4], husk3 = dmgPic && phoneViewHusk(huskR);   // №557: остов рисует 3D-проход, плоскую картинку прячем
    uiScrShow(0, PV_T_PIC_OK, !dmgPic); uiScrShow(0, PV_T_PIC_DMG, dmgPic && !husk3); uiScrShow(0, PV_T_SPOT, !husk3); }
  { int rep = !PHONE_REPAIR_TEXT_OLD && needsRepair(s, r) && phase == PV_IDLE, enough = shopCoins(s) >= PHONE_REPAIR_COST;   // №535: цена починки — строка пакета (группа phone_repair)
    uiScrShow(0, PV_T_REPAIR_OK, rep && enough); uiScrShow(0, PV_T_REPAIR_NO, rep && !enough); }
#ifndef BOOTH_RAYS_VEC_OLD
#define BOOTH_RAYS_VEC_OLD 0   // №833: 1 — лучи будки картинкой
#endif
  if (!BOOTH_RAYS_VEC_OLD) uiScrRays(UI_RAYS_SHOP);   // №833: две векторные звезды лучей, в разные стороны
  if (!RAYS_FADE_OLD) uiScrSpin(raysT, 1.f - a);   // №763: лучи (интерфейс, поверх затемнения) гаснут и проявляются вместе с градиентом под затемнением
  uiScrDraw(0, 0, t, 0);
  if (needsRepair(s, r) && phase == PV_IDLE && PHONE_REPAIR_TEXT_OLD) {   // правка 470: цена починки — под картинкой, по оси галереи (кодом, прежний путь)
    char rp[40]; snprintf(rp, sizeof rp, "REPAIR: %d COINS", PHONE_REPAIR_COST);
    int w = uiText(0, 0, -100, 0x00000000u, rp);
    uiText(0, PV_GAL_CX - w / 2, 222, shopCoins(s) >= PHONE_REPAIR_COST ? 0xff40ffffu : 0xff4040ffu, rp);
  }
}
const char *phoneViewEnd(Shop *s) {
  if (!on) return 0;
  uiScrClose(); on = 0; phase = PV_DONE;
  return s && chosen >= 0 && chosen < n ? pvName[chosen] : 0;
}
