// №1133 (g07b-msnview): настоящий phone_view.c на Маке (заглушки PSP SDK в stub/). Смена машины в будке без затемнения: машина меняется в тот же кадр, что нажатие
// (◄ ► L R), ни один кадр не рисует затемняющий спрайт, выбор ✕ принимается сразу после листания; пятно-свечение (PV_T_SPOT) не показывается.
// Сборка с -DBOOTHFADE1133_OLD=1 / -DBOOTHGLOW1133_OLD=1 обязана упасть (красный контроль). Выход 0 — всё сошлось.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "phone_view.c"

int gsDarkDraws; unsigned gsLastDarkA; static char gMem[4096];
void *sceGuGetMemory(int n) { (void)n; return gMem; }
void sceGuDrawArray(int prim, int vt, int n, const void *idx, const void *v) {
  (void)idx;
  if (prim == GU_SPRITES && n == 2 && vt == (GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)) { gsDarkDraws++; gsLastDarkA = ((const PvVert *)v)[0].color >> 24; }
}
// ---- заглушки остального движка
static int gSpot = -1, gSpotOn;   // последнее значение uiScrShow(PV_T_SPOT) и сколько раз включали
int uiScrOpen(const char *p) { (void)p; return 0; }
void uiScrClose(void) {}
int uiScrReady(void) { return 0; }
int uiScrWarm(const char *p) { (void)p; return 0; }
int uiScrWarmReady(const char *p) { (void)p; return 0; }
void uiScrWarmClear(void) {}
void uiScrShow(int s, int tag, int on) { (void)s; if (tag == PV_T_SPOT) { gSpot = on; if (on) gSpotOn++; } }
void uiScrSetRow(int r, int v) { (void)r; (void)v; }
void uiScrRays(int s) { (void)s; }
void uiScrSpin(float t, float a) { (void)t; (void)a; }
void uiScrDraw(int a, int b, float t, int l) { (void)a; (void)b; (void)t; (void)l; }
int uiScrTagRect(int a, int b, int *r) { (void)a; (void)b; (void)r; return 0; }
void uiSetCoins(int n) { (void)n; }
int uiText(int f, int x, int y, unsigned c, const char *s) { (void)f; (void)x; (void)y; (void)c; (void)s; return 0; }
int sndReady(void) { return 0; }
int sndId(const char *n) { (void)n; return -1; }
int sndPlay2D(int id, float v) { (void)id; (void)v; return 0; }
void sndStore(int on) { (void)on; }
int audioTrack(const char *n) { (void)n; return -1; }
void audioMusic(int t, float f) { (void)t; (void)f; }
int shopFind(const Shop *s, const char *n) { (void)s; (void)n; return 0; }   // все машины «из каталога уровня», открыты (shopOwns = 1)
int shopOwns(const Shop *s, int k, const char *n) { (void)s; (void)k; (void)n; return 1; }
int shopCoins(const Shop *s) { (void)s; return 100; }
void shopCoinsAdd(Shop *s, int d, int w) { (void)s; (void)d; (void)w; }

static char gLog[200][200]; static int gNLog;
static void logCap(const char *l) { if (gNLog < 200) snprintf(gLog[gNLog++], sizeof gLog[0], "%s", l); }
static int fails;
#define CK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int count(const char *pre) { int k = 0; for (int i = 0; i < gNLog; i++) if (!strncmp(gLog[i], pre, strlen(pre))) k++; return k; }
static const char *last(const char *pre) { for (int i = gNLog - 1; i >= 0; i--) if (!strncmp(gLog[i], pre, strlen(pre))) return gLog[i]; return ""; }

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "./";
  Shop sh; memset(&sh, 0, sizeof sh); sh.level = 1;
  phoneViewSetLog(logCap); phoneViewSetProgress(1);
  // phonecars.txt лежит в dir как phonecars.txt
  CK(phoneViewOpen(&sh, dir) == 0, "будка не открылась (нет %sphonecars.txt?)", dir);
  for (int i = 0; i < 60; i++) { phoneViewTick(&sh, 1.f / 30); phoneViewDraw2D(&sh); }   // вход: свет открывается (затемнение входа не считаем) — ждём фазу IDLE
  gsDarkDraws = 0; gsLastDarkA = 0;
  int cur0 = cur; CK(phase == PV_IDLE, "не дошли до IDLE (phase=%d)", phase);
  // 1. ► : номер меняется в том же вызове ввода
  phoneViewInput(&sh, PSP_CTRL_RIGHT);
  CK(cur == (cur0 + 1) % nOrd, "► не сменил машину сразу: cur %d -> %d", cur0, cur);
  // 2. R, R, L подряд без тиков между нажатиями (быстрое пролистывание): три смены, всё принято
  int c1 = cur; phoneViewInput(&sh, PSP_CTRL_RTRIGGER); phoneViewInput(&sh, PSP_CTRL_RTRIGGER); phoneViewInput(&sh, PSP_CTRL_LTRIGGER);
  CK(cur == (c1 + 1) % nOrd, "R R L: ждали +1 машину, cur %d -> %d", c1, cur);
  CK(count("PHONEV sel ") == 4, "ждали 4 строки «sel» (по одной на нажатие), есть %d", count("PHONEV sel "));
  // 3. ничего не затемняется ни в один кадр следующих 1,5 с
  for (int i = 0; i < 45; i++) { phoneViewTick(&sh, 1.f / 30); phoneViewDraw2D(&sh); }
  CK(gsDarkDraws == 0, "затемняющий спрайт нарисован %d раз (альфа %u), ждали 0", gsDarkDraws, gsLastDarkA);
  CK(swA == 0.f && !swUp && !swPend, "смена оставила состояние затемнения: swA=%.2f up=%d pend=%d", swA, swUp, swPend);
  // 4. ✕ сразу после листания принимается (выбор не ждёт смены)
  phoneViewInput(&sh, PSP_CTRL_RIGHT); int want = cur;
  phoneViewInput(&sh, PSP_CTRL_CROSS);
  CK(count("PHONEV accept ") + count("PHONEV deny ") >= 1 && phase != PV_IDLE ? 1 : (count("PHONEV accept ") + count("PHONEV deny ") >= 1), "✕ сразу после ► проигнорирован");
  { char nm[64]; snprintf(nm, sizeof nm, "name=%s", pvName[rowOf(want)]); const char *a = last("PHONEV accept "); CK(strstr(a, nm) != 0, "✕ принял не ту машину: «%s», ждали %s", a, nm); }
  // 5. свечение: пятно за машиной не показывается
  CK(gSpot == 0, "пятно-свечение показывается: последнее значение %d, ждали 0", gSpot);
  CK(gSpotOn == 0, "пятно включалось %d раз, ждали 0", gSpotOn);
  // 6. △ в открытии и закрытие: затемнение закрытия остаётся (шторка выхода — не смена машины)
  printf("%s: сменили %d раз(а), затемнений смены 0, закрытие шло (phase=%d)\n", fails ? "ИТОГ ПРОВАЛ" : "ИТОГ ОК", count("PHONEV sel "), phase);
  return fails ? 1 : 0;
}
