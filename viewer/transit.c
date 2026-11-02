// transit.c — ирис оригинала и журнал взрыва (см. transit.h). Данные — iris.shr (tools/iris_export.py, формат IRS1).
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "transit.h"
#include "ui.h"        // сверка g09a-hud круг 5 (M2:36): надпись LOADING на долгом чёрном экране
#include "blackbox.h"   // правка 103: bbBeatTick — кадр ириса показан (сторож самописца, main.c)

#define TR_W 480
#define TR_H 272
#define TR_BUFW 512
#define TR_MAXV 256

#define IRIS_SLOW 1.95f   // правка Роба 54 (эталон PS2 tools/ps2ref/GAPS_L1.md: закрытие ~0,65 с против 0,33 с по данным)
// Правка 333: ирис посадки и выхода у машин с SetIrisTransition (why car_in / car_out) — в темпе данных: запись PS2 (картинка 4:3,
// 60 к/с) у Plow King 869,25 / 1085,20 с, ракетной 1978,45 с, седана трафика 2061,23 с — закрытие 0,27–0,32 с, темно 0,17–0,18 с,
// раскрытие 0,30–0,32 с, т. е. ключи iris.p3d 0/10/15/25 при 30 к/с без замедления (ирис комнат и меню у записи ~×1,93 — там загрузка).
// «flag irisslow K» (служебное) задаёт темп и этим переходам
#define IRIS_CAR_SLOW 1.f
typedef struct { unsigned c; float x, y, z; } TrV;
static struct {
  int ok, off;
  unsigned nv, nk, abgr; float fps, nframes, fov, dist;
  float key[16][2];               // кадр, вес морфа (1 — открыто, 0 — закрыто)
  float *base, *offs;             // nv × (x, y), nv × (dx, dy)
  float k, rin1;                  // px на единицу сетки (cover 480×272); вписанный радиус отверстия при весе 1, единицы
  float fClosed, fOpen, fEnd;     // закрыто с / раскрытие с / конец (кадры данных)
  int psm; unsigned fbBytes; TransitLog log; const float *clock;
  float slow;                     // служебное: время ×1/slow
  float cur;                      // темп текущего перехода (правка 333: car_in / car_out — IRIS_CAR_SLOW, остальные — slow)
  int active; float f; const char *why;   // ирис на экране: кадр анимации (после закрытия — от fClosed до fEnd)
  int mode; float ht, hold;       // правка 349: mode 1 — чёрный экран после затемнения (прошло ht из hold с)
} T = {0};
static unsigned int __attribute__((aligned(16))) tlist[8192];

static float rdF(const unsigned char *p) { float v; memcpy(&v, p, 4); return v; }
static unsigned rdU(const unsigned char *p) { unsigned v; memcpy(&v, p, 4); return v; }

int transitLoad(const char *path) {
  SceUID f = sceIoOpen(path, PSP_O_RDONLY, 0); if (f < 0) return -1;
  int n = sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  unsigned char *b = n > 36 ? malloc(n) : NULL;
  if (!b || sceIoRead(f, b, n) != n) { sceIoClose(f); free(b); return -2; }
  sceIoClose(f);
  if (memcmp(b, "IRS1", 4)) { free(b); return -3; }
  T.nv = rdU(b + 4); T.fps = rdF(b + 8); T.nframes = rdF(b + 12); T.nk = rdU(b + 16);
  T.fov = rdF(b + 20); T.dist = rdF(b + 24); T.abgr = rdU(b + 28);
  unsigned need = 32 + T.nk * 8 + T.nv * 16;
  if (T.nv == 0 || T.nv > TR_MAXV || T.nv % 3 || T.nk < 2 || T.nk > 16 || (unsigned)n < need || T.fps <= 0.f) { free(b); return -4; }
  const unsigned char *p = b + 32;
  for (unsigned i = 0; i < T.nk; i++, p += 8) { T.key[i][0] = rdF(p); T.key[i][1] = rdF(p + 4); }
  T.base = malloc(T.nv * 16); if (!T.base) { free(b); return -5; }
  T.offs = T.base + T.nv * 2;
  memcpy(T.base, p, T.nv * 16);
  free(b);
  // фазы из ключей: закрыто — первый и последний ключ с наименьшим весом; конец — последний ключ
  float vmin = 1e9f; for (unsigned i = 0; i < T.nk; i++) if (T.key[i][1] < vmin) vmin = T.key[i][1];
  T.fClosed = -1.f;
  for (unsigned i = 0; i < T.nk; i++) if (T.key[i][1] <= vmin + 1e-4f) { if (T.fClosed < 0.f) T.fClosed = T.key[i][0]; T.fOpen = T.key[i][0]; }
  T.fEnd = T.key[T.nk - 1][0];
  // масштаб: камера данных (горизонтальный fov на 640 px) -> px/единицу оригинала, затем cover под 480×272
  float kSrc = 320.f / (T.dist * tanf(T.fov * .5f));
  T.k = kSrc * sqrtf(240.f * 240.f + 136.f * 136.f) / 400.f;
  // вписанный радиус отверстия при весе 1: вершины морфа (отверстие), число углов — число разных направлений
  float R = 0.f; int nang = 0; float ang[TR_MAXV];
  for (unsigned i = 0; i < T.nv; i++) {
    float dx = T.offs[2 * i], dy = T.offs[2 * i + 1]; if (dx * dx + dy * dy < 1e-6f) continue;
    float x = T.base[2 * i] + dx, y = T.base[2 * i + 1] + dy, r = sqrtf(x * x + y * y); if (r > R) R = r;
    float a = atan2f(y, x); int dup = 0; for (int k = 0; k < nang; k++) if (fabsf(ang[k] - a) < 1e-3f) dup = 1;
    if (!dup && nang < TR_MAXV) ang[nang++] = a;
  }
  T.rin1 = nang > 2 ? R * cosf(3.14159265f / nang) : R;
  T.slow = IRIS_SLOW; T.ok = 1;   // правка 54: ирис PS2 ~0,65 с (замер), ключи диска при 30 к/с дают 0,33 с
  return 0;
}
void transitSetup(int psm, unsigned fbBytes, TransitLog log, const float *clock) { T.psm = psm; T.fbBytes = fbBytes; T.log = log; T.clock = clock; }
int transitFadeReady(void) { return T.fbBytes && !T.off; }
int transitReady(void) { return T.ok && !T.off; }
void transitSlow(float k) { T.slow = k > 0.f ? k : IRIS_SLOW; }
void transitOff(int off) { T.off = off; }
int transitBusy(void) { return T.active; }
void transitDrop(void) { T.active = 0; }   // №663: ирис, закрытый над газетой, не раскрывать над роликом (NEW GAME: ролик fmv1a идёт из чёрного)
int transitFullyClosed(void);   // ниже (нужна weightAt)

static float weightAt(float f) {   // канал STE: линейно между ключами
  if (f <= T.key[0][0]) return T.key[0][1];
  for (unsigned i = 1; i < T.nk; i++) if (f <= T.key[i][0]) {
    float a = T.key[i - 1][0], b = T.key[i][0], u = b > a ? (f - a) / (b - a) : 1.f;
    return T.key[i - 1][1] + (T.key[i][1] - T.key[i - 1][1]) * u;
  }
  return T.key[T.nk - 1][1];
}
static void logFrame(const char *phase, float f, float w) {
  if (!T.log) return;
  char s[160]; snprintf(s, sizeof s, "IRIS why=%s phase=%s f=%.2f w=%.3f r=%.1f t=%.2f\n", T.why ? T.why : "-", phase, f, w,
                        w * T.rin1 * T.k, T.clock ? *T.clock : 0.f);
  T.log(s);
}
// сетка данных с весом морфа w; экранные координаты -> орто-проекция (отсечение GE, вершины за краем экрана — законно)
static void drawMesh(float w) {
  static const int st[] = {GU_DEPTH_TEST, GU_BLEND, GU_ALPHA_TEST, GU_TEXTURE_2D, GU_FOG, GU_LIGHTING, GU_CULL_FACE};
  int sv[7]; for (int i = 0; i < 7; i++) { sv[i] = sceGuGetStatus(st[i]); sceGuDisable(st[i]); }
  ScePspFMatrix4 I, P; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  P = I; P.x.x = 2.f / TR_W; P.y.y = -2.f / TR_H; P.z.z = -1.f; P.w.x = -1.f; P.w.y = 1.f;
  sceGuSetMatrix(GU_PROJECTION, &P); sceGuSetMatrix(GU_VIEW, &I); sceGuSetMatrix(GU_MODEL, &I);
  sceGuScissor(0, 0, TR_W, TR_H);
  // Кэш вершин (15.10, TEST18: «transit» 1,5–2,3 мс на кадр, пока ирис держит закрытый кадр под карточкой брифинга — вес морфа тот же, а вершины считались заново каждый кадр):
  // при том же весе, цвете, размере и масштабе вершины берутся из постоянного буфера (запись в память — один раз, DcacheWriteback). TRANSIT_CACHE_OLD=1 — прежнее
#ifndef TRANSIT_CACHE_OLD
#define TRANSIT_CACHE_OLD 0
#endif
  static TrV *tcV; static unsigned tcN, tcAbgr; static float tcW = -1e30f, tcK;
  TrV *v;
  if (!TRANSIT_CACHE_OLD && T.nv) {
    if (!tcV || tcN != T.nv || tcW != w || tcAbgr != T.abgr || tcK != T.k) {
      if (!tcV || tcN != T.nv) { free(tcV); tcV = (TrV *)memalign(64, (T.nv * sizeof(TrV) + 63) & ~63u); tcN = tcV ? T.nv : 0; }
      if (tcV) {
        for (unsigned i = 0; i < T.nv; i++) {
          float x = T.base[2 * i] + w * T.offs[2 * i], y = T.base[2 * i + 1] + w * T.offs[2 * i + 1];
          tcV[i].c = T.abgr; tcV[i].x = TR_W * .5f + x * T.k; tcV[i].y = TR_H * .5f - y * T.k; tcV[i].z = 0.f;
        }
        sceKernelDcacheWritebackRange(tcV, T.nv * sizeof(TrV)); tcW = w; tcAbgr = T.abgr; tcK = T.k;
      }
    }
    v = tcV;
  } else v = NULL;
  if (!v) {
    v = (TrV *)sceGuGetMemory(T.nv * sizeof(TrV));
    for (unsigned i = 0; i < T.nv; i++) {
      float x = T.base[2 * i] + w * T.offs[2 * i], y = T.base[2 * i + 1] + w * T.offs[2 * i + 1];
      v[i].c = T.abgr; v[i].x = TR_W * .5f + x * T.k; v[i].y = TR_H * .5f - y * T.k; v[i].z = 0.f;
    }
  }
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, T.nv, 0, v);
  for (int i = 0; i < 7; i++) sceGuSetStatus(st[i], sv[i]);
}

int transitClose(void **fbp, const char *why) {
  if (!transitReady()) return 0;
  T.why = why; T.active = 0; T.mode = 0; int shown = 0;
  T.cur = (why && !strncmp(why, "car_", 4) && T.slow == IRIS_SLOW) ? IRIS_CAR_SLOW : T.slow;   // правка 333
  unsigned vram = (unsigned)sceGeEdramGetAddr(), t0 = sceKernelGetSystemTimeLow();
  for (;;) {
    float tt = (sceKernelGetSystemTimeLow() - t0) * 1e-6f / T.cur, f = T.key[0][0] + tt * T.fps;
    if (f > T.fClosed) f = T.fClosed;
    float w = weightAt(f);
    unsigned draw = (unsigned)*fbp, front = draw ? 0u : T.fbBytes;   // показан другой буфер: застывший кадр мира
    sceGuStart(GU_DIRECT, tlist);
    sceGuCopyImage(T.psm, 0, 0, TR_W, TR_H, TR_BUFW, (void *)(vram + front), 0, 0, TR_BUFW, (void *)(vram + draw));
    sceGuTexSync();
    drawMesh(w);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = sceGuSwapBuffers(); shown++; bbBeatTick();   // правка 103: сторож
    logFrame("close", f, w);
    if (f >= T.fClosed) break;
  }
  T.active = 1; T.f = T.fClosed;   // дальше закрыто и раскрытие — над игрой (transitStep/transitDraw)
  return shown;
}
// ------------------------------------------------------------------ правка 349: затемнение (зона DEATH)
// [PS2] 05.10 (pcsx2-twins, кадры экрана через 2 vsync PAL, build: agent349/ps2/s1, s4): машина в объёме R6D01 / Z6D1 и Гомер
// пешком — яркость всего кадра (и HUD) падает линейно до нуля за 46 кадров 50 Гц (0,92 с; центр и края — в одной пропорции, не
// ирис), машина/Гомер всё это время стоят; перенос — в чёрном; чёрный ещё 12–14 кадров (0,26 с), затем картинка сразу (кадр
// до — 0, следующий — полная яркость). Время — как у ириса: «flag irisslow K» растягивает и его (снимки середины)
static float fadeSlow(void) { return (T.ok && T.slow > 0.f) ? T.slow / IRIS_SLOW : 1.f; }
static int fadeCopy;   // 1 — застывший кадр скопирован в основную память; 0 — памяти не нашлось, затемнение от показанного кадра
static void logFade(const char *phase, float a, float tt) {
  if (!T.log) return;
  char s[128]; snprintf(s, sizeof s, "FADE why=%s phase=%s a=%.3f tt=%.3f copy=%d t=%.2f\n", T.why ? T.why : "-", phase, a, tt, fadeCopy, T.clock ? *T.clock : 0.f);
  T.log(s);
}
static void drawBlack(float a) {   // чёрный прямоугольник на весь кадр с прозрачностью 1 − a (a = 1 — чёрный)
  static const int st[] = {GU_DEPTH_TEST, GU_BLEND, GU_ALPHA_TEST, GU_TEXTURE_2D, GU_FOG, GU_LIGHTING, GU_CULL_FACE};
  int sv[7]; for (int i = 0; i < 7; i++) { sv[i] = sceGuGetStatus(st[i]); sceGuDisable(st[i]); }
  if (a < 1.f) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); }
  sceGuScissor(0, 0, TR_W, TR_H);
  unsigned al = a <= 0.f ? 0u : a >= 1.f ? 255u : (unsigned)(a * 255.f + .5f);
  TrV *v = (TrV *)sceGuGetMemory(2 * sizeof(TrV));
  v[0].c = al << 24; v[0].x = 0.f; v[0].y = 0.f; v[0].z = 0.f;
  v[1].c = al << 24; v[1].x = TR_W; v[1].y = TR_H; v[1].z = 0.f;
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
  for (int i = 0; i < 7; i++) sceGuSetStatus(st[i], sv[i]);
}
int transitFade(void **fbp, const char *why, float sec) {
  if (!transitFadeReady() || sec <= 0.f) return 0;
  T.why = why; T.active = 0; T.mode = 0; int shown = 0; float sf = fadeSlow();
  unsigned vram = (unsigned)sceGeEdramGetAddr(), front0 = *fbp ? 0u : T.fbBytes;
  // застывший кадр — копией в основную память: каждый кадр затемнения рисуется от него (не от уже затемнённого: двойная буферизация)
  void *raw = malloc(T.fbBytes + 64), *buf = raw ? (void *)(((unsigned)raw + 63u) & ~63u) : NULL; fadeCopy = buf != NULL;
  if (buf) {
    sceKernelDcacheWritebackInvalidateRange(buf, T.fbBytes);
    sceGuStart(GU_DIRECT, tlist);
    sceGuCopyImage(T.psm, 0, 0, TR_W, TR_H, TR_BUFW, (void *)(vram + front0), 0, 0, TR_BUFW, buf);
    sceGuTexSync(); sceGuFinish(); sceGuSync(0, 0);
  }
  float aPrev = 0.f; unsigned t0 = sceKernelGetSystemTimeLow();
  for (;;) {
    float tt = (sceKernelGetSystemTimeLow() - t0) * 1e-6f / sf, a = tt / sec; if (a > 1.f) a = 1.f;
    unsigned draw = (unsigned)*fbp, front = draw ? 0u : T.fbBytes;
    // без копии (памяти нет): от показанного кадра, прозрачность с поправкой на уже наложенное затемнение
    float ad = buf ? a : (a >= 1.f ? 1.f : 1.f - (1.f - a) / (1.f - aPrev));
    sceGuStart(GU_DIRECT, tlist);
    sceGuCopyImage(T.psm, 0, 0, TR_W, TR_H, TR_BUFW, buf ? buf : (void *)(vram + front), 0, 0, TR_BUFW, (void *)(vram + draw));
    sceGuTexSync();
    drawBlack(ad);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = sceGuSwapBuffers(); shown++; bbBeatTick();   // правка 103: сторож
    logFade("out", a, tt); aPrev = a;
    if (a >= 1.f) break;
  }
  free(raw);
  return shown;
}
void transitHold(float sec) {
  if (!transitFadeReady()) return;
  T.mode = 1; T.ht = 0.f; T.hold = sec; T.active = sec > 0.f;
  if (!T.active) logFade("done", 0.f, 0.f);
}

// Сверка g09a-hud круг 5 (M2:36): если экран чёрный (ирис закрыт или чёрное после затемнения) дольше 3 с, надпись LOADING мигает с периодом 0,25 с. Время считается по часам
// игры (dt шагов ириса, с учётом замедления не масштабируется). IRIS_LOADING_OLD=1 — надписи нет
#ifndef IRIS_LOADING_OLD
#define IRIS_LOADING_OLD 0
#endif
static float blackT;
void transitStep(float dt) {
  if (!IRIS_LOADING_OLD) { if (T.active && transitFullyClosed()) blackT += dt; else blackT = 0.f; }
  if (!T.active) return;
  if (T.mode == 1) {   // правка 349: чёрный после затемнения, затем картинка сразу
    T.ht += dt / fadeSlow();
    if (T.ht >= T.hold) { T.active = 0; T.mode = 0; logFade("done", 0.f, T.ht); return; }
    logFade("hold", 1.f, T.ht); return;
  }
  T.f += dt * T.fps / T.cur;
  if (T.f >= T.fEnd) { T.active = 0; logFrame("done", T.fEnd, 1.f); return; }
  float w = weightAt(T.f);
  logFrame(T.f < T.fOpen ? "hold" : "open", T.f, w);
}
// №625 (TEST18, Роб): ирис старта миссии закрывается при ОТКРЫТИИ брифинга и стоит закрытым под карточкой; после Continue он продолжал идти
// (пауза, раскрытие), карточка уезжала по чёрному/под раскрывающимся кругом. PS2: карточка уезжает поверх мира, ириса нет — поэтому по Continue ирис
// миссии («msn», ещё не раскрытый) снимается сразу, мир открыт, карточка с рукой уезжает над ним. 1 — снят
int transitSkipMsn(void) {
  if (!T.active || T.mode != 0 || !T.why || strcmp(T.why, "msn")) return 0;
  T.active = 0; logFrame("skip", T.f, 1.f); return 1;
}
int transitFullyClosed(void) { return T.active && (T.mode == 1 || weightAt(T.f) <= .001f); }   // ирис закрыт целиком (или чёрный после затемнения): мир под ним не виден
static void loadingBlink(void) {
  if (IRIS_LOADING_OLD || blackT <= 3.f || ((int)((blackT - 3.f) / .25f) & 1)) return;
  int font = uiFontFind("boulder_16"); if (font < 0) font = 0;
  int w = uiText(font, 0, -100, 0x00000000u, "LOADING");   // замер ширины за экраном
  uiText(font, (TR_W - w) / 2, 128, 0xffffffffu, "LOADING");
}
void transitDraw(void) {
  if (!T.active) return;
  if (T.mode == 1) { drawBlack(1.f); loadingBlink(); return; }   // правка 349
  drawMesh(weightAt(T.f)); loadingBlink();
}

const char *transitHomerWhy(const Homer *H) {
  static int init, pOver, pEj, pSeat, pCarIn, pCarOut; const char *w = NULL;
  if (init) {
    if (H->irisCarIn > pCarIn) w = "car_in";            // правка 333: машина с SetIrisTransition — посадка
    else if (H->irisCarOut > pCarOut) w = "car_out";    // ... и выход
    else if (H->irisSeat > pSeat) w = "seat";
    else if (H->overTop && !pOver && H->ejected == pEj) w = "overtop";
  }
  init = 1; pOver = H->overTop; pEj = H->ejected; pSeat = H->irisSeat; pCarIn = H->irisCarIn; pCarOut = H->irisCarOut;
  return w;
}

void transitEjectWatch(const Homer *H, const Car *c, int haveHomer, int boomNow, float dt) {
  // выброс случается в gameAfterCar раньше этого вызова в том же кадре: «был за рулём» — по прошлому кадру
  static float t = -1.f, y0, maxY; static int ej0, prevDrv, prevEj;
  if (!haveHomer) return;
  int drvBefore = prevDrv, ejBefore = prevEj; prevDrv = homerDriving(H); prevEj = H->ejected;
  if (!T.log) return;
  char s[220];
  if (boomNow) {
    t = 0.f; y0 = H->y; maxY = H->y; ej0 = ejBefore;
    snprintf(s, sizeof s, "BOOM t=%.2f car=%.2f,%.2f,%.2f driving=%d state=%d ejected_now=%d\n", T.clock ? *T.clock : 0.f, c->x, c->y, c->z,
             drvBefore, H->state, H->ejected - ejBefore);
    T.log(s); return;
  }
  if (t < 0.f) return;
  t += dt; if (H->y > maxY) maxY = H->y;
  if (t < 2.5f) return;
  CarBox b; carBox(c, &b);
  float ex = H->x - b.x, ez = H->z - b.z;
  float ua = fmaxf(fabsf(ex * b.fx + ez * b.fz) - b.hl, 0.f), us = fmaxf(fabsf(-ex * b.fz + ez * b.fx) - b.hw, 0.f);
  snprintf(s, sizeof s, "EJECT t=%.2f onfoot=%d state=%d ejects=%d clear=%.2f dist=%.2f apex=%.2f homer=%.2f,%.2f,%.2f\n",
           T.clock ? *T.clock : 0.f, !homerDriving(H) && homerOnFoot(H), H->state, H->ejected - ej0, sqrtf(ua * ua + us * us),
           hypotf(H->x - c->x, H->z - c->z), maxY - y0, H->x, H->y, H->z);
  T.log(s); t = -1.f;
}
