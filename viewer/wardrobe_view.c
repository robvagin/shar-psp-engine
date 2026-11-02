// wardrobe_view.c — гардероб Гомера, см. wardrobe_view.h. Шторка и времена — как у будки (phone_view.c)
#ifdef WARDROBE_HOST
// Mac gate uses the same catalogue, preview loader and input state machine;
// only PSP I/O and display/audio endpoints are supplied by the harness.
#include "wardrobe_host_io.h"
#else
#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspgum.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "wardrobe_view.h"
#include "ui.h"
#include "sound.h"
#include "audio.h"
#include "skin.h"
#include <math.h>

enum { WV_T_LOCKED = UI_T_GAMBLE, WV_T_ARROWS = UI_T_SKIP };
enum { WV_IN = 0, WV_IDLE, WV_OUT, WV_DONE };
// Сверка g09a-hud (круг 1, M2:34 / Д11): свет открывается и закрывается линейно по 0,5 с, затемнение до 85 %. WARD_LIGHT_SV_OLD=1 — прежнее (0,3 / 0,3 с, 80 %)
#ifndef WARD_LIGHT_SV_OLD
#define WARD_LIGHT_SV_OLD 0
#endif
// Сверка g09a-hud (круг 1, P13:13): список гардероба — стартовый костюм, если он не надет, плюс платные, кроме надетого; купленный надевается бесплатно (уже так). WARD_HIDEWORN_OLD=1 — надетый тоже в списке
#ifndef WARD_HIDEWORN_OLD
#define WARD_HIDEWORN_OLD 0
#endif
#define WV_DARK_HOLD .4f
#define WV_LIGHT_IN  (WARD_LIGHT_SV_OLD ? .3f : .5f)
#define WV_DARK_A    (WARD_LIGHT_SV_OLD ? .8f : .85f)
#define WV_LIGHT_OUT (WARD_LIGHT_SV_OLD ? .3f : .5f)
#define WV_MAX 8
#define WV_SPOTS 8

static char wName[WV_MAX][16], wFile[WV_MAX][32]; static int wPrice[WV_MAX], n;
static float spot[WV_SPOTS][4]; static int nspot;
static int on, phase, cur, chosen = -1, sndRes, sScroll = -1, sAccept = -1, sBack = -1, sLocked = -1;
static float t; static int wHide = -1;   // wHide — надетый костюм, в списке его нет
static float raysT;   // №763: часы вращения лучей №761 — t обнуляется на входе в выбор и на выходе
#ifndef RAYS_FADE_OLD
#define RAYS_FADE_OLD 0   // №763: 1 — лучи на часах экрана (угол прыгал при смене фазы) и без затухания, как TEST32
#endif
static void (*logFn)(const char *);
// Правка Роба 119 (30.09: «не плодить картинку и 3D — сразу 3D-модель Гомера в каждом наряде, вращается, как машины в лавке»):
// вместо картинки skins2d — модель выбранного костюма (homer*.shr, тот же файл, что надевается), клип стойки loco_idle_rest,
// поворот WV_SPIN. Грузится при выборе, освобождается при смене и на выходе (одна модель ~300 КБ). Место — окно PreviewImage
// страницы PhoneBooth (build/ui/layout_wardrobe.json: [138, 80, 103, 96]) в полосе MiddleBar (y 46–193); фон страницы (тег
// LOADING, tools/skins_export.py) — под Гомером, надписи, замок и стрелки — поверх
#define WV_CX 189.5f   // px центра Гомера на экране 480x272
#define WV_CY 118.f
#define WV_HPX 128.f   // px роста (1.84 м)
// правка 141: место Гомера — из раскладки (строка «view cx cy hpx» в skins.txt, tools/skins_export.py: центр галереи 240 и рост
// в доле средней полосы); старый skins.txt без строки -> запасные WV_CX/WV_CY/WV_HPX
static float vCx = WV_CX, vCy = WV_CY, vHpx = WV_HPX;
#define WV_FOVY 30.f
#define WV_SPIN .9f    // рад/с, оборот за 7 с
static SkinModel *pm; static int pmK = -1; static SkinPlayer pp; static float pmAng; static char wDir[256];

static void logf_(const char *fmt, ...) {   // строка WARDV ... t=.. в smoke.txt (как SHOPV лавки)
  if (!logFn) return;
  char line[200], body[160]; va_list ap; va_start(ap, fmt); vsnprintf(body, sizeof body, fmt, ap); va_end(ap);
  snprintf(line, sizeof line, "WARDV %s t=%.2f\n", body, t); logFn(line);
}
void wardrobeSetLog(void (*log)(const char *)) { logFn = log; }
static void snd(int id) { if (id >= 0) sndPlay2D(id, 1.f); }
static int owned(Shop *s, int k) { return k >= 0 && k < n && (wPrice[k] == 0 || shopOwns(s, SHOP_SKIN, wName[k])); }
int wardrobeInit(const char *dir) {
  n = nspot = 0; vCx = WV_CX; vCy = WV_CY; vHpx = WV_HPX;
  char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir ? dir : "", "skins.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  char buf[1024]; int len = sceIoRead(f, buf, sizeof buf - 1); sceIoClose(f); if (len <= 0) return 0; buf[len] = 0;
  for (char *line = buf; line && *line; ) {
    char *nl = strchr(line, '\n'); if (nl) *nl = 0;
    char a[32], b[32]; int p; float x, y, z, r;
    if (n < WV_MAX && sscanf(line, "skin %15s %31s %d", a, b, &p) == 3) { snprintf(wName[n], 16, "%.15s", a); snprintf(wFile[n], 32, "%.31s", b); wPrice[n] = p; n++; }
    else if (nspot < WV_SPOTS && sscanf(line, "spot %f %f %f %f", &x, &y, &z, &r) == 4) { spot[nspot][0] = x; spot[nspot][1] = y; spot[nspot][2] = z; spot[nspot][3] = r; nspot++; }
    else if (sscanf(line, "view %f %f %f", &x, &y, &r) == 3 && r > 8.f && r < 272.f) { vCx = x; vCy = y; vHpx = r; }
    line = nl ? nl + 1 : 0;
  }
  return n;
}
int wardrobeNear(float x, float y, float z) {
  for (int i = 0; i < nspot; i++) {
    float dx = x - spot[i][0], dz = z - spot[i][2];
    if (dx * dx + dz * dz < spot[i][3] * spot[i][3] && y > spot[i][1] - 2.f && y < spot[i][1] + 3.f) return 1;
  }
  return 0;
}
const char *wardrobeFile(int k) { return k >= 0 && k < n ? wFile[k] : 0; }
const char *wardrobeName(int k) { return k >= 0 && k < n ? wName[k] : 0; }   // сверка g07 круг 3: имя костюма k как в замке этапа locked skin
int wardrobeSpot(int i, float out[4]) { if (i < 0 || i >= nspot) return 0; for (int k = 0; k < 4; k++) out[k] = spot[i][k]; return 1; }
static void previewLoad(int k) {   // правка 119: модель костюма k для вращения на странице
  if (pm && k == pmK) return;
  if (pm) { skinFree(pm); pm = 0; } pmK = -1;
  if (k < 0 || k >= n) return;
  char fn[320]; snprintf(fn, sizeof fn, "%s%s", wDir, wFile[k]);
  pm = skinLoad(fn); if (!pm) { logf_("nomodel %s", fn); return; }
  pmK = k; skinInit(&pp, pm);
  int c = skinFindClip(pm, "loco_idle_rest"); if (c >= 0) skinPlay(&pp, c, 1, 0.f);
  skinUpdate(&pp, 0.f); logf_("model %d %s", k, wFile[k]);
}
int wardrobeOn(void) { return on; }
#ifndef WARDROBE_HOST
static void raysDetect(const char *dir);
#endif
int wardrobeOpen(Shop *s, const char *dir, int worn) {
  if (on || !s || !n || uiScrReady()) { logf_("refuse on=%d shop=%d n=%d busy=%d", on, s != 0, n, uiScrReady()); return -1; }
  char fn[320]; snprintf(fn, sizeof fn, "%s%s", dir ? dir : "", "wardrobe.shr");
  if (uiScrOpen(fn) != 0) { logf_("nopack %s", fn); return -1; }
  if (!sndRes && sndReady()) { sScroll = sndId("scroll"); sAccept = sndId("accept"); sBack = sndId("back"); sLocked = sndId("locked_out"); sndRes = 1; }
  { int m = audioTrack("muzak"); if (m >= 0) audioMusic(m, .3f); }   // событие Store композиции — как будка и лавка
  wHide = !WARD_HIDEWORN_OLD && n > 1 && worn >= 0 && worn < n ? worn : -1;
  cur = worn >= 0 && worn < n ? worn : 0; if (cur == wHide) cur = (cur + 1) % n; on = 1; phase = WV_IN; t = 0.f; raysT = 0.f; chosen = -1;
  snprintf(wDir, sizeof wDir, "%s", dir ? dir : ""); pmAng = 0.f; previewLoad(cur);
#ifndef WARDROBE_HOST
  raysDetect(dir);
#endif
  logf_("open n=%d cur=%d coins=%d name=%s", n, cur, shopCoins(s), wName[cur]);
  return 0;
}
int wardrobeTick(float dt) {
  if (!on) return WARDV_EV_NONE;
  t += dt < .05f ? dt : .05f; raysT += dt < .05f ? dt : .05f;
  if (pm) { skinUpdate(&pp, dt < .05f ? dt : .05f); pmAng += WV_SPIN * (dt < .05f ? dt : .05f); if (pmAng > 6.2831853f) pmAng -= 6.2831853f; }
  if (phase == WV_IN && t >= WV_DARK_HOLD + WV_LIGHT_IN) { phase = WV_IDLE; t = 0.f; }
  if (phase == WV_OUT && t >= WV_LIGHT_OUT) { phase = WV_DONE; return WARDV_EV_CLOSE; }
  return WARDV_EV_NONE;
}
void wardrobeInput(Shop *s, unsigned hit) {
  if (!on || phase != WV_IDLE) return;
  if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { snd(sBack); phase = WV_OUT; t = 0.f; return; }
  int d = (hit & PSP_CTRL_RIGHT) ? 1 : (hit & PSP_CTRL_LEFT) ? -1 : 0;
  if (d && n > (wHide >= 0 ? 2 : 1)) { cur = (cur + d + n) % n; if (cur == wHide) cur = (cur + d + n) % n; snd(sScroll); logf_("sel %d name=%s price=%d owned=%d", cur, wName[cur], wPrice[cur], owned(s, cur)); previewLoad(cur); }
  if (hit & PSP_CTRL_CROSS) {
    if (!owned(s, cur)) {   // покупка: хватает монет — списать и получить (shopBuy: принадлежность сохраняется в Coins.owned)
      int it = shopFind(s, wName[cur]);
      if (it < 0 || shopCoins(s) < wPrice[cur] || shopBuy(s, it) != SHOP_OK) { snd(sLocked >= 0 ? sLocked : sBack); logf_("deny %d name=%s price=%d coins=%d", cur, wName[cur], wPrice[cur], shopCoins(s)); return; }
      logf_("buy %d name=%s price=%d coins=%d", cur, wName[cur], wPrice[cur], shopCoins(s));
    }
    chosen = cur; snd(sAccept); phase = WV_OUT; t = 0.f;
  }
}
#ifndef WARDROBE_HOST
// №703 (Роб: «лучи очень лагают, найти обход»): фон страницы с лучами — одна непрозрачная картинка (слой RaysBG, тег WASPS, tools/wardrobe_rays.py) вместо
// градиента (LOADING) + 16 клиньев с альфой (drawRays). WARDROBE_RAYBAKE_OLD=1 при сборке или файл raysold рядом с EBOOT — прежний вариант;
// пакет без слоя (старый wardrobe.shr) — тоже прежний. Свой выбор на каждое открытие гардероба
#ifndef WARDROBE_RAYBAKE_OLD
#define WARDROBE_RAYBAKE_OLD 0
#endif
static int raysBaked;
static void raysDetect(const char *dir) {
  raysBaked = 0;
#if !WARDROBE_RAYBAKE_OLD && !defined(WARDROBE_RAYS_OLD)
  char fo[320]; SceIoStat st_; int r_[4]; snprintf(fo, sizeof fo, "%s%s", dir ? dir : "", "raysold");
  if (sceIoGetstat(fo, &st_) < 0 && uiScrTagRect(0, UI_T_WASPS, r_)) raysBaked = 1;
#endif
  logf_("rays baked=%d", raysBaked);
}
static void showTags(Shop *s, int bg) {   // фон страницы (LOADING; при запечённых лучах — WASPS) или всё остальное (ALWAYS, монеты, замок, стрелки)
  uiScrShow(0, UI_T_LOADING, bg && !raysBaked); uiScrShow(0, UI_T_WASPS, bg && raysBaked); uiScrShow(0, UI_T_ALWAYS, !bg);
  uiScrShow(0, UI_T_COINS, !bg && phase != WV_DONE);
  uiScrShow(0, WV_T_LOCKED, !bg && s && !owned(s, cur));
  uiScrShow(0, WV_T_ARROWS, !bg && n > 1);
}
static Shop *drawShop;
// №703: учёт заливки кадра гардероба (px × слоёв) — строка WARDFILL раз в ~1 с в журнал (только при заданном logFn)
static int fillBg, fillBgD, fillRays, fillFilm, fillFg, fillFgD, fillN;
static void fillLog(void) {
  if (!logFn || ++fillN < 30) return;
  fillN = 0;
  logf_("fill bg=%d/%dd rays=%d film=%d fg=%d/%dd total=%d", fillBg, fillBgD, fillRays, fillFilm, fillFg, fillFgD, fillBg + fillRays + fillFilm + fillFg);
}
// №695 (Роб, TEST27: «вместо голубого градиента по центру — еле заметные лучи, как под газетой на экране загрузки, лучи выходят из-под 3D-модели»):
// 16 клиньев из точки у ног Гомера, прозрачность от WV_RAY_A у центра до 0 на радиусе WV_RAY_R; рисуются поверх фона страницы и под моделью.
// WARDROBE_RAYS_OLD=1 — без лучей (и данные с пятном Spotlight: WARDROBE_BG_OLD=1 в tools/skins_export.py)
#ifndef WARDROBE_RAYS_OLD
#define WV_RAYS 16
#define WV_RAY_A 24       // 0..255: у центра (≈9 %)
#define WV_RAY_R 340.f
typedef struct { unsigned color; short x, y, z, pad; } WvRayV;
static void drawRays(void) {
  WvRayV *v = (WvRayV *)sceGuGetMemory(WV_RAYS * 3 * sizeof(WvRayV));
  float cx = vCx, cy = vCy + vHpx * .45f;
  for (int i = 0; i < WV_RAYS; i++) {
    float a0 = i * 6.2831853f / WV_RAYS, a1 = a0 + 3.1415927f / WV_RAYS;
    WvRayV *q = v + i * 3;
    q[0].color = ((unsigned)WV_RAY_A << 24) | 0x00ffd8c8; q[0].x = (short)cx; q[0].y = (short)cy;
    q[1].color = q[2].color = 0x00ffd8c8;
    q[1].x = (short)(cx + cosf(a0) * WV_RAY_R); q[1].y = (short)(cy + sinf(a0) * WV_RAY_R);
    q[2].x = (short)(cx + cosf(a1) * WV_RAY_R); q[2].y = (short)(cy + sinf(a1) * WV_RAY_R);
    q[0].z = q[1].z = q[2].z = 0;
  }
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST);   // без теста глубины запись глубины не идёт — маску не трогаем
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, WV_RAYS * 3, 0, v);
  sceGuEnable(GU_TEXTURE_2D);
}
#endif
void wardrobeDraw3D(void) {
  if (!on) return;
  sceGuClearColor(0xff000000); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  uiScrSetRow(0, cur); showTags(drawShop, 1);
#ifndef WARD_RAYS_VEC_OLD
#define WARD_RAYS_VEC_OLD 0   // №833: 1 — лучи гардероба картинкой
#endif
  if (!WARD_RAYS_VEC_OLD) uiScrRays(UI_RAYS_SHOP);   // №833: две векторные звезды лучей, в разные стороны
  if (!RAYS_FADE_OLD) uiScrSpin(raysT, 1.f);   // №763: лучи в проходе фона — под затемнением, гаснут вместе с градиентом
  uiScrDraw(0, 0, t, 0);   // фон страницы
  { UiStats u = uiLastStats(); fillBg = u.pixels; fillBgD = u.draws; fillRays = 0; }
#ifndef WARDROBE_RAYS_OLD
  if (!raysBaked) {
    drawRays();
    fillRays = WV_RAYS * (int)(.5f * WV_RAY_R * WV_RAY_R * (3.1415927f / WV_RAYS));   // номинал без обрезки экраном
  }
#endif
  if (!pm) return;
  // Гомер: камера на оси Z смотрит в начало; расстояние — чтобы рост 1.84 м занял WV_HPX пикселей, сдвиг — в окно PreviewImage
  float th = tanf(WV_FOVY * .5f * GU_PI / 180.f), asp = 480.f / 272.f, D = 1.84f * 272.f / (2.f * th * vHpx);
  float x = (vCx - 240.f) / 240.f * D * th * asp, y = (136.f - vCy) / 136.f * D * th;
  sceGuClearDepth(0); sceGuClear(GU_DEPTH_BUFFER_BIT);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuDisable(GU_FOG); sceGuDisable(GU_CULL_FACE); sceGuShadeModel(GU_SMOOTH);
  sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(WV_FOVY, asp, .5f, 30.f);
  sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
  ScePspFVector3 eye = {0.f, 0.f, D}, at = {0.f, 0.f, 0.f}, up = {0.f, 1.f, 0.f}; sceGumLookAt(&eye, &at, &up);
  sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); sceGumUpdateMatrix();
  ScePspFMatrix4 w; skinPlace(&w, x, y - .92f, 0.f, pmAng);
  skinToonEye(0.f, 0.f, D);   // проверка 18.10: контур мультика — по расстоянию от камеры гардероба, не от глаза мира (мир ставит свой на следующем кадре)
  skinDraw(&pp, &w);
  sceGuDisable(GU_DEPTH_TEST);
}
typedef struct { unsigned color; short x, y, z, pad; } WvVert;
void wardrobeDraw2D(Shop *s) {
  if (!on || !s) return;
  float a = phase == WV_IN ? (t < WV_DARK_HOLD ? WV_DARK_A : WV_DARK_A * (1.f - (t - WV_DARK_HOLD) / WV_LIGHT_IN))
          : phase == WV_OUT ? WV_DARK_A * (t / WV_LIGHT_OUT > 1.f ? 1.f : t / WV_LIGHT_OUT) : phase == WV_DONE ? WV_DARK_A : 0.f;
  if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
  fillFilm = a > 0.f ? 480 * 272 : 0;
  if (a > 0.f) {
    WvVert *v = (WvVert *)sceGuGetMemory(2 * sizeof(WvVert)); unsigned c = (unsigned)(a * 255.f + .5f) << 24;
    v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = 480; v[1].y = 272; v[0].z = v[1].z = 0;
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  }
  uiScrSetRow(0, cur);
  uiSetCoins(shopCoins(s));
  drawShop = s; showTags(s, 0);
  uiScrDraw(0, 0, t, 0);   // надписи, замок с ценой, стрелки, монеты — поверх Гомера
  { UiStats u = uiLastStats(); fillFg = u.pixels; fillFgD = u.draws; }
  fillLog();
}
#endif
int wardrobeEnd(void) {
  if (!on) return -1;
  uiScrClose(); on = 0; phase = WV_DONE;
  if (pm) { skinFree(pm); pm = 0; } pmK = -1;   // правка 119: модель витрины
  logf_("end chosen=%d", chosen);
  return chosen;
}
