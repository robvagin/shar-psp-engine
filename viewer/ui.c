// UI pack renderer (see ui.h for the API and the exact GU state it changes).
// Mirrors render()/layout_text() in tools/ui_export.py pixel for pixel where it matters
// (hinted text metrics, sprite-font spacing, anchoring is precomputed in the pack).
#pragma GCC optimize("O2")
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <pspkernel.h>
#include <psputils.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspge.h>
#include "ui.h"
#include "ui_radial.h"
#include "ui_anim.h"
#include "ui_layout.h"   // правка 126: пиксельный шрифт 8×8 пузыря, перенос, начало текста задания
#include "marker_lod.h"
#include "glintshape941.h"   // №1012: тот же векторный шейп блика ✦ — метка выделенного пункта меню
#include "bootmsx.h"    // шрифт msx 8×8 (pspsdk libpspdebug, tools/bootmsx.sh)

typedef struct {
  char magic[4]; unsigned version;
  unsigned ntex, offTex, nitem, offItem, nscreen, offScreen, nfont, offFont, nstr, offStr,
           nsfont, offSfont, nicon, offIcon, nslot, offSlot, npts, offPts;
  int mapTex; float mapX0, mapZ0, mapMpp;
  unsigned size, res[7];
} UiHdr;                                                    // 128 B
typedef struct { unsigned short psm, flags, w, h; unsigned offPix, offClut, nclut, bufw, rows, res; } UiTex;   // 32 B
typedef struct {
  unsigned char type, screen, tag, field, fmt, halign, valign, flags;
  short x, y, w, h;
  unsigned color, color2;
  signed char sdx, sdy; unsigned char font, menu;
  unsigned short first, count;
  float px, py, ang;
  unsigned prefix;                                          // T_NUM: slot+1 of an icon left of the number
  float clip;                                               // T_RADARMAP/ICONS: радиус круга радара (0 = ang), масштаб по ang
  unsigned res[3];
} UiItem;                                                   // 64 B
typedef struct { unsigned first, count, nmenu, tags; } UiScreen;
typedef struct { unsigned short tex, lh, base, pad; unsigned name, res; } UiFontHdr;
typedef struct { unsigned short u, v; unsigned char w; signed char left; unsigned char adv, flags; } UiGlyph;
typedef struct { UiFontHdr h; UiGlyph g[256]; } UiFont;
typedef struct { unsigned short tex, u, v; unsigned char w, h; signed char xoff; unsigned char adv; unsigned short flags; } UiSGlyph;
typedef struct { unsigned short tex, u, v; unsigned char w, h; unsigned color, pad; } UiIcon;
typedef struct { unsigned short tex, u, v, w, h; short x, y, pad; } UiSlot;
typedef struct { float x, y; unsigned color; } UiPt;

enum { T_RECT, T_POLY, T_IMG, T_TEXT, T_NUM, T_RADARMAP, T_RADARICONS, T_SLOT, T_BAR, T_SECTOR };
// F_FUSE, F_RATE — экраны загрузки (tools/load_export.py): фитиль сдвигается на PROGRESS/1000 * ang пикселей вправо
// (огонь; у полосы Wick сгоревшая левая часть не рисуется); кадр T_IMG / строка T_TEXT = часы uiScrDraw * px (к/с)
enum { F_WRAP = 1, F_GRAD = 2, F_SHADOW = 4, F_MENU = 8, F_LINEAR = 16, F_SELONLY = 32, F_FUSE = 64, F_RATE = 128 };
#define ITEM_EXT(it) ((it)->res[0] & 1u)                 // картинка извне (брифинг миссии): res[0] бит 0 (биты flags заняты)
#define ITEM_TITLE(it) ((it)->res[0] & 2u)               // заголовок паузы «PAUSE MENU» (Pause.pag): res[0] бит 1
#define ITEM_CONE(it) ((it)->res[0] & 4u)                // клин радара RadarCone0 (Hud.pag): res[0] бит 2, правка Роба 50
#define ITEM_PIX8(it) ((it)->res[0] & 8u)                // строка гранью pix8 (правка 141: будка, гардероб): res[0] бит 3
// Правка Роба 312 (04.10, твик-панель типографики build/ТИПОГРАФИКА_ПАНЕЛЬ, мост build/ui_typo.json -> tools/ui_typo.py): пакет несёт
// настройки строки, которые нельзя запечь в грань пакета. res[0] бит 4 — строка гранью пакета вместо pix8 (ITEM_NOPIX8); res[2], числа
// со знаком, px: биты 0–3 межбуквенное pix8, 4–7 межстрочное pix8 (−8…7), байт 1 — добавка к верхнему полю текста задания, байт 2 —
// к левому полю от аватара, байт 3 — к правому полю переноса в рамке; res[1] строки (у картинок там бегунок меню) — к левому полю
// задания без значка. Без ui_typo.json всё нули — рисуется как раньше
#define ITEM_NOPIX8(it) ((it)->res[0] & 16u)
#define TYPO_TRK(it) ((int)(((it)->res[2] & 0xfu) ^ 8u) - 8)
#define TYPO_LEAD(it) ((int)(((it)->res[2] >> 4 & 0xfu) ^ 8u) - 8)
#define TYPO_PADT(it) ((int)(signed char)((it)->res[2] >> 8 & 0xffu))
#define TYPO_PADL(it) ((int)(signed char)((it)->res[2] >> 16 & 0xffu))
#define TYPO_PADR(it) ((int)(signed char)((it)->res[2] >> 24 & 0xffu))
#define TYPO_PADN(it) ((it)->type == T_TEXT ? (int)(signed char)((it)->res[1] & 0xffu) : 0)   // res[1] строки: поле задания без значка
static int typoShot;   // правка 312: служебный снимок фона панели (flag uitypo N): 1 — без текста, 2 — без интерфейса
void uiTypoShot(int mode) { typoShot = mode; }
#define PREFIX_GAP 2                    // px between the coin icon and the count (tools/ui_export.py: same)
#define DIM_ABGR 0x80a0a0a0u            // disabled menu item (not implemented yet): grey, half alpha
enum { FMT_NONE, FMT_INT, FMT_TIME, FMT_FRAC, FMT_ORDINAL, FMT_SELECT, FMT_PCT };
#define SEL_ABGR 0xff14e6ffu            // selected menu item: SHAR yellow (255,230,20)

#define VT_SPR  (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define VT_CSPR (GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define VT_FTEX (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define VT_FCOL (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
typedef struct { unsigned short u, v; unsigned color; short x, y, z, pad; } VSpr;    // 16 B
typedef struct { unsigned color; short x, y, z, pad; } VCSpr;                          // 12 B
typedef struct { float u, v; unsigned color; float x, y, z; } VFTex;                  // 24 B
typedef struct { unsigned color; float x, y, z; } VFCol;                              // 16 B

struct UiSegs; static const unsigned char *pakAtIn(const struct UiSegs *sg, const unsigned char *base, unsigned off);
static unsigned char *pak; static struct UiSegs *pakSegs;   // pakSegs — куски пакета (pakReadSplit), 0 — пакет одним куском
static const UiHdr *H;
static const UiTex *tex;
static const UiItem *items;
static const UiScreen *screens;
static const UiFont *fonts;
static const unsigned *strOff;
static const UiSGlyph *sfonts;
static const UiIcon *icons;
static const UiSlot *slots;
static const UiPt *pts;
static MarkerView markerView;
static int markerDrawn;
int uiMarkerDrawn(void) { return markerDrawn; }
void uiMarkerView(const float *eye, const float *forward, const float *right, const float *up, float focal) {
  memcpy(markerView.eye, eye, 12); memcpy(markerView.forward, forward, 12);
  memcpy(markerView.right, right, 12); memcpy(markerView.up, up, 12); markerView.focal = focal;
  markerDrawn=0;
}

static int fieldv[UI_F_COUNT];
// №528 (Роб 11.10): пузырь задания в двух экземплярах — res[0] бит 9 (512): рисуется, пока у задания есть значок (аватар Talk to Marge); бит 10 (1024): пока значка нет (Vehicle destroyed).
// У каждого свои настройки (шрифт, поля, ширина рамки, tools/ui_typo.py hud_bubble_avatar / hud_bubble_plain). Нет битов — элемент всегда. UI_BUBBLEVAR_OLD=1 — оба рисуются, как до правки
#ifndef UI_BUBBLEVAR_OLD
#define UI_BUBBLEVAR_OLD 0
#endif
#ifndef BUBBLE_GROW_OLD
#define BUBBLE_GROW_OLD 0   // служебно (прогон «было»): только без роста рамки, остальное по-новому
#endif
#ifndef BUBBLE_CENTER_OLD
#define BUBBLE_CENTER_OLD 0   // №616 (Роб 15.10): 1 — текст пузыря без значка прижат к левому краю блока (прежнее); 0 — строки по центру блока, поля слева и справа равны для любой длины
#endif
#ifndef BUBBLE_RULES689_OLD
#define BUBBLE_RULES689_OLD 0   // №689 (Роб 17.10): один вид для всех баблов сообщений: текст ВСЕГДА слева, рамка по ширине текста (уже полной — сжимается, шире — растёт до радара), перенос шире раньше, чем рамка растёт вверх (выше верха радара не лезет); 1 — прежнее (№616 по центру, рамка пакета, рост вверх)
#endif
#ifndef BUBBLE_VCENTER1_OLD
#define BUBBLE_VCENTER1_OLD 0   // №671: 1 — прежнее (№640): текст в рамке на несколько строк сдвигается вниз к центру по высоте; 0 — текст стоит там, где задано панелью типографики Роба (hud_bubble_avatar/_plain: padT, dy) и у верха текстовой области, как у PS2; рамка растёт вниз/вверх под строки (№646) и поля справа/слева ровные (№640) остаются
#endif
#ifndef BUBBLE_LAW_OLD
#define BUBBLE_LAW_OLD 0   // Принцип пузыря v1 (Роб 17.10, видео 17:06 и голосом: «текст не может заходить на территорию округлого края бабла»; закон — память
                           // project_shar_bubble_law): капсула = скругление R (половина высоты рамки) + прямая часть + скругление R; буквы только на прямой части;
                           // высота рамки постоянная, растёт только ширина — по самой широкой строке (короткий текст — чуть шире); правый край не дальше BUB_LAW_RIGHT
                           // (2 отступа радара от края экрана до обода радара); строки ровные, без висячего слова. 1 — прежнее (№619/№646/№689/№708)
#endif
#define BUB_LAW_PAD 2      // px: от конца скругления до буквы и от буквы до начала скругления
#define BUB_LAW_SHORT 12   // px: однострочный текст — рамка шире текста на столько («даже немножечко шире»)
#define BUB_LAW_SLACK 3    // px: прозрачные поля рамки пакета (56 против видимых 50) и пустой низ ячейки строки — сравнение полей сверху/снизу «на глаз» (bubble_v1_check)
#define BUB_LAW_RIGHT 341  // px экрана: обод радара x 387 − 2 × 23 (отступ обода от правого края экрана; замер эмулятора 17.10)
// Роб 17.10 вечер (№747 + слово после TEST31): «один формат» — у ВСЕХ пузырей одна ширина, как родная рамка MessageBox диска (правый край 308 px)
// капельку шире; короткий текст («TALK TO MARGE») пузырь не сужает, длинный не расширяет — строки укладываются внутри (перенос ровный, без висячего
// слова, отступы панели Роба). BUB_ONE_OLD=1 — ширина по тексту (принцип v1)
#ifndef BUB_ONE_OLD
#define BUB_ONE_OLD 0
#endif
#define BUB_ONE_RIGHT 287   // Роб: ширина пузыря «TALK TO MARGE» (рамка x 21..217, 196 px) + треть = 261 -> видимый край 282 (+5: прозрачное поле рамки)
#ifndef BUBBLE_EVEN_OLD
#define BUBBLE_EVEN_OLD 0   // №640 (Роб, видео TEST23 1:48–2:17: «надпись в бабле криво»): 1 — прежнее (текст у верха рамки, правое поле меньше левого); 0 — поля сверху/снизу и слева/справа равны у любого пузыря
#endif
#ifndef BUBBLE_AVATAR_PAD_OLD
#define BUBBLE_AVATAR_PAD_OLD 0   // №708 (Роб, видео IMG_7279 3:50): 1 — прежнее: у пузыря с аватаром/значком текст кончается в 20 px от правого края рамки (заходит на скруглённый край капсулы 24 px); 0 — запас справа +10 px (текст кончается до скругления с зазором)
#endif
#ifndef PILL_NATIVE_OLD
#define PILL_NATIVE_OLD 0   // №724 (Роб 17.10, видео IMG_7280 5:05: «рамки бабла VEHICLE DESTROYED — не родной масштаб, битые края»): №710 растягивал края капсулы
                            // целиком в (h + gh) / h раза (рамка 274x56 при росте на 3 px — края x1,07, ступеньки и обод толще сверху, чем сбоку). Теперь края
                            // родного размера, рост добирает строка середины (как у всех рамок 3 x 3). 1 — прежнее масштабирование №710
#endif
#ifndef MSG_PILL_OLD
#define MSG_PILL_OLD 0   // №710 (Роб, видео IMG_7279 4:26): 1 — прежнее: пузырь без аватара стоит с поля x=49 и при росте под 3 строки края капсулы остаются радиусом 24 (прямоугольник с малым радиусом); 0 — рамка у левого поля экрана (как пузырь с аватаром), края капсулы растут с высотой (полукруг)
#endif
#ifndef BUBBLE_FIT_OLD
#define BUBBLE_FIT_OLD 0   // №548: 1 — прежний перенос текста пузыря без значка и правый край рамки (до поля экрана)
#endif
static inline int itemVarOff(const UiItem *it) {
  unsigned v = it->res[0] & 1536u; if (!v || UI_BUBBLEVAR_OLD) return 0;
  int plain = fieldv[UI_F_OBJICON] < 0; return (v & 512u) ? plain : !plain;
}
static unsigned tagMask[UI_SCREEN_MAX];                    // экраны пакета (второй пакет — до UI_SCREEN_MAX)
static float rpx, rpz, rhead, rrange = 150.f;
static struct { unsigned char type, hasH; float x, z, h; } ricon[UI_RADAR_MAX];   // h — курс (стрелка машины повёрнута, правка 122)
static int nricon;
static int radarDrawn[UI_R_COUNT];
int uiRadarDrawn(int type) { return (unsigned)type < UI_R_COUNT ? radarDrawn[type] : 0; }
static int curTex, curFilt, texOn;
static UiStats st;
static const unsigned char **texPix;                        // pixel pointer per texture (RAM, or VRAM after uiUseVram)
static unsigned menuOff[UI_SCREEN_MAX];                     // bit i = menu item i disabled (dimmed, skipped)
static unsigned menuHid[UI_SCREEN_MAX];                     // правка Роба 137: bit i = строка i скрыта (не рисуется, пропускается)
static int menuBackdrop = 1;                                 // 0: главное меню без заглушки 3D-гостиной (её рисует frontend3d)
void uiMenuBackdrop(int on) { menuBackdrop = on; }
static int mainFlight;                                       // камера меню летит: только рамка-телевизор (ui.h UI_T_MAIN_ROW)
void uiMenuFlight(int on) { mainFlight = on; }
static void uiLogf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static char actText[48];   // правка 206: подпись у значка действия (uiActionText)
static char placeText[24];  // №598: код клетки на паузе (uiPlaceCode)
#define ACT_FADE_S .2f        // правка 272: подпись у △ проявляется за столько секунд — пока пружина значка доходит до размера (правило порта)
// Правка Роба 500: подпись обнуляют в начале КАЖДОГО кадра (main_loop.h uiActionText(NULL)) и ставят заново, поэтому строка ACTTEXT писалась в журнал каждый кадр, пока
// подпись видна (тур SHARTEST6: L2 «Talk To Milhouse» — 430 строк на 16 с, запись на карту памяти в окне x_ui 3–4 мс против 0,3 мс на L1). Теперь строка — при появлении
// и смене подписи; подпись, которой не было хотя бы в одном кадре, появится строкой снова (гейт action_label.sh видит то же). UI_ACTLOG_OLD=1 — прежнее
#ifndef UI_ACTLOG_OLD
#define UI_ACTLOG_OLD 0
#endif
void uiActionText(const char *s) {
  static char logged[sizeof actText]; static int setNow;
  char was[sizeof actText]; memcpy(was, actText, sizeof was); snprintf(actText, sizeof actText, "%s", s ? s : "");
  if (UI_ACTLOG_OLD) { if (strcmp(was, actText) && actText[0]) uiLogf("ACTTEXT %s\n", actText); return; }   // служебно: гейт tests/ui/action_label.sh
  if (!actText[0]) { if (!setNow) logged[0] = 0; setNow = 0; return; }   // кадр начался: если подписи в прошлом кадре не ставили — «видна заново»
  setNow = 1;
  if (strcmp(logged, actText)) { memcpy(logged, actText, sizeof logged); uiLogf("ACTTEXT %s\n", actText); }
}
#ifndef UI_MSG_HARDBR
#define UI_MSG_HARDBR 0
#endif
static int msgHardBr = UI_MSG_HARDBR;   // правка 267: 1 — прежнее (жёсткие переносы, узкая строка без значка)
static char msgText[160];                                    // uiMessage text ("" = objective string by index)
static float subFadeA = 1.f;   // №639: прозрачность всего экрана на время кроссфейда (uiSubFade)
static float mboxK = 1.f, mboxA = 1.f, mboxCx, mboxCy;   // №639: окно сообщения всплывает как пузырь задания (единый масштаб из центра рамки)
static char mboxText[256];   // правка 122: текст окна сообщения (uiDrawMessageBox)
static char msnDoneText[64];  // правка 122: текст вспышки конца миссии (uiMissionDoneText): «MISSION COMPLETE!» / «BONUS MISSION\nCOMPLETE!»
void uiMissionDoneText(const char *s) { snprintf(msnDoneText, sizeof msnDoneText, "%s", s ? s : ""); }
static unsigned msgUntil;                                    // system time (us) the message hides at, 0 = never
#ifndef PAUSE_ANIM_OLD
#define PAUSE_ANIM_OLD 0
#endif
static unsigned pauseT0us, pauseCloseUs; static int pauseClosing; static float pauseSlow = 1.f;   // pauseSlow: служебно (flag pauseslow K) — анимация паузы медленнее в K раз, снимки середины
static float pauseClk(unsigned t0) { return (unsigned)(sceKernelGetSystemTimeLow() - t0) * 1e-6f / pauseSlow; }
void uiPauseSlow(float k) { pauseSlow = k > 0.f ? k : 1.f; }   // №631: анимация паузы
static int pauseAnimPose(float *sc, float *al); static float pauseDimA(void);
static int paused, pauseSel, pauseScr = UI_SCREEN_PAUSE;   // pauseScr — какая пауза открыта: свободной езды или миссии
static const signed char PM_MAP[] = {UI_P_CONTINUE, UI_P_RESTART_MISSION, UI_P_ABORT_MISSION, UI_P_OPTIONS, UI_P_QUIT_GAME};
static int spinPx, spinN;   // №703: площадь (px) и число вращающихся слоёв за кадр
static int scrMode; static float scrT;                       // рисуется пакет экранов загрузки (uiScrDraw) и его часы
static float spinT = -1.f, spinA = 1.f;                      // uiScrSpin: время вращения и яркость крутящихся картинок на один вызов uiScrDraw
static int raysStyle;   // №833: uiScrRays — стиль векторных лучей на один вызов uiScrDraw (0 — картинкой)
static int menuHidden;                                       // uiPauseMenuHide: пункты паузы скрыты (рамка и заголовок остаются)
void uiPauseMenuHide(int on) { menuHidden = on; }
static int scrLeakOld;                                       // правка 348: красный контроль (ui.h uiScrPauseLeakOld)
void uiScrPauseLeakOld(int on) { scrLeakOld = on; }

// ---- анимации HUD (viewer/ui_anim.h): часы интерфейса, всплывание уведомлений, пульс △, кольцо Hit & Run ------------
// Часы идут только пока рисуется HUD (пауза их останавливает), шаг не больше 0.1 с. Показ/снятие тега ловится при
// рисовании (сравнение с прошлым кадром), а не в uiShow: за кадр разные места игры включают и гасят один тег (△ у
// машины, у гэга, у собеседника), и «вкл-выкл-вкл» внутри кадра не должно перезапускать анимацию.
static float uiClk; static unsigned uiClkLast; static int uiClkInit;
static unsigned hudPrev, hudLeave, hudForce;                 // маска прошлого кадра HUD, теги в фазе выхода, служебный показ
static float tagOnT[32], tagOffT[32], forceUntil[32];
static int msgNew, cdNew;                                    // новый текст задания / новое число отсчёта — всплыть заново
static char msgLast[160];                                    // текст задания, который гаснет (msgText уже очищен)
static unsigned animOff;                                     // UI_ANIM_* — отрицательные контроли гейтов
static unsigned traceBits; static void (*uiLogFn)(const char *);
static void (*popHook)(int ev, int on);
static struct { int on; float s, a, cx, cy, sy; } xf;   // sy: масштаб по y, 0 — как s (строка задания растёт только по высоте, №516)            // масштаб от точки (cx,cy) и прозрачность текущего элемента
static float tutK = 1.f;   // №516: масштаб подсказки Барта из центра экрана (uiTutorialScale, ui_anim.h 2г)
static float lastSpr[4];                                     // прямоугольник последнего спрайта с масштабом (как ушёл в GE)
// №405 (Роб 08.10: «первый экран как в оригинале, Press Start, наверное, скейлится»): «Press START button» титульного (Splash.pag,
// тег PRESS_START, boot.shr) пульсирует масштабом от своего центра. ЗАМЕР записи PS2 (build/ps2ref/external/yt-bziOipI4gDI.mp4,
// 11,7–19,2 с, 60 к/с, 450 кадров; tests/title/ps2_pulse.py): ширина надписи — синусоида, период 0,600 с, меньшая/большая 0,825,
// центр стоит (±0,5 px), яркость постоянна (не мигает); логотип неподвижен (центр ±0,1 px). В данных диска кривых нет (код).
// PORT_RULE: наибольший размер = прежний размер порта (boulder_16 1:1, правка 152). Первый показ — середина диапазона
// с ростом: native PS2 first-frame154 растёт к peak162–164; точная phase/scale из PNG без Scrooby binding не выводится.
// SLES helper 0x1e7b58 принимает phase ($f16); binding/инициализация PressStart не установлены. Midpoint + sin — правило порта,
// не точная фаза оригинала. Period/min/max прежние; -DTITLE_START_PHASE_OLD=1 возвращает прежний старт с cospeak.
// -DTITLE_PULSE_OLD=1 — надпись стоит (красный контроль tests/title/title.sh)
#ifndef TITLE_PULSE_OLD
#define TITLE_PULSE_OLD 0
#endif
#ifndef TITLE_START_PHASE_OLD
#define TITLE_START_PHASE_OLD 0
#endif
#define PS_PULSE_PERIOD .600f    // с, запись PS2
#define PS_PULSE_MIN .825f       // меньшая/большая, запись PS2
float uiPressStartBox[5] = {-1.f};   // служебное: часы экрана и рамка надписи, как ушла в GE (строка TITLEPS смоука)
static float psPulse(float t) {
#if TITLE_START_PHASE_OLD
  return PS_PULSE_MIN + (1.f - PS_PULSE_MIN) * .5f * (1.f + cosf(2.f * 3.14159265f * t / PS_PULSE_PERIOD));
#else
  return PS_PULSE_MIN + (1.f - PS_PULSE_MIN) * .5f * (1.f + sinf(2.f * 3.14159265f * t / PS_PULSE_PERIOD));
#endif
}
// Правка Роба 412 («выбранный пункт главного меню в оригинале фирменно масштабировался»). Исполняемый файл диска SLES_518.97 (чтение,
// psp-objdump): CGuiMenu (конструктор 0x1a5c30, пятый аргумент — флаги пункта, +12) у главного меню создаётся с флагами 2
// (CGuiScreenMainMenu 0x1b8208: пункт 1, флаги 2 — только масштаб, без перелива цвета, бит 1). Обновление выбранного пункта 0x1a6eb8:
// масштаб = 0x1e7b58(t, 600, 1.05, 0.1, φ) = 1,05 + 0,1 · sin(2π t / 600 мс − φ), φ = −asin(−0,5) = π/6 (0x1a7000..0x1a701c);
// t — мс выбранного пункта по модулю 600 (0x1a74e8..0x1a7504), обнуляется при смене пункта (0x1a6780). Значит, на смене пункта
// масштаб ровно 1,0, дальше 0,95..1,15 с периодом 0,600 с (меньшая/большая 0,826 — как замер «Press START» №405: 0,601 / 0,825).
// Масштаб — от центра строки (0x1a70cc: x 0, y половина высоты). ПРАВИЛО ПОРТА (как №405 «Press START»): форма и время — диска, но
// наибольший размер — нынешняя строка порта (масштаб / 1,15: 0,826..1,0, на смене пункта 0,87): шрифт меню порта уже крупнее на 20 %
// (Роб 26.09), и RESUME GAME при 1,15 наезжал на правую стрелку на 5 px (замер смоука 09.10: строка до x 312, стрелка с x 307).
// MENU_PULSE_OLD=1 — строка стоит (красный tests/menu/item_pulse.sh)
#ifndef MENU_PULSE_OLD
#define MENU_PULSE_OLD 0
#endif
// №889 (Роб 23.10: «текст пунктов главного меню рваный по пикселям, размер как в Options, там чёткий»): пульс масштабом 0,83..1,0 пересэмплировал буквы
// каждый кадр (на смене пункта 0,87, наибольшее 1,0 — на долю секунды), чёткой строка не бывала никогда; у Options паузы пульса нет. Главное меню — строка
// стоит (как Options), в гостиной по-прежнему светится предмет пункта. MENU_MAIN_PULSE_OLD=1 — прежний пульс главного меню (гейт tests/menu/item_pulse.sh)
#ifndef MENU_MAIN_PULSE_OLD
#define MENU_MAIN_PULSE_OLD 0
#endif
// №924 (Роб 24.10: «в главном меню пропала анимация RESUME GAME, LOAD GAME и других — пункты прыгали масштабом с приятным изингом; стрелочки починил, теперь всё
// хорошо, но анимация пропала. Верни»): пульс главного меню, снятый №889, возвращается как был — по числам диска (1,05 + 0,1·sin, период 0,6 с, на смене пункта 1,0 от
// наибольшего 0,87), стрелки №888 не тронуты. MENU_PULSE924_OLD=1 — строка стоит, как после №889 (красный tests/menu/item_pulse.sh, зелёный tests/menu889)
#ifndef MENU_PULSE924_OLD
#define MENU_PULSE924_OLD 0
#endif
#define MENU_PULSE_MS 600.f      // 0x1a6f8c / 0x1a7054
#define MENU_PULSE_MID 1.05f     // 0x1a705c
#define MENU_PULSE_AMP .1f       // 0x1a7068
static float menuPulse(float tMs) { return MENU_PULSE_MID + MENU_PULSE_AMP * sinf(2.f * 3.14159265f * tMs / MENU_PULSE_MS - 3.14159265f / 6.f); }
static int menuPulseSel = -1, menuPulseKey = -1; static unsigned menuPulseLast; static float menuPulseMs;
// Правка 413/412 (хвост, 09.10): тот же пульс на остальных меню. SLES_518.97 — флаги пункта у каждого экрана (пятый аргумент
// конструктора CGuiMenu 0x1a5c30): 2 (масштаб) — MainMenu 0x1b8208, PauseSunday 0x1d79cc, PauseMission 0x1d611c, PauseOptions 0x1d65c4,
// Sound 0x1c0df8, Options 0x1be078, Controller 0x1b6c50, ViewMovies 0x1c2698, ScrapBook 0x1bf2d0, GameSlots 0x1b7344 / 0x1dd73c,
// Language 0x1b13e4, PhoneBooth 0x1d97f0, MiniPause 0x1c6bdc, Splash 0x1c173c; 0 (без эффекта) — PauseSettings 0x1d6e5c, MissionSelect
// 0x1dccb8 / 0x1dcda4, MemoryCard 0x1aae2c, MiniMenu 0x1c49ec, Preview 0x1d80f4. Бит 0 (перелив цвета 0x1e7c50) не ставит ни один экран.
// Экраны основного пакета — по номеру; второго (uiScrDrawMenu) — флагом вызывающего (uiScrMenuFlags). MENU_PULSE_OLD=1 — без пульса
#ifndef FAILYN_PULSE_OLD
#define FAILYN_PULSE_OLD 0   // №572: 1 — прежнее поведение (без пульса YES/NO на экране провала)
#endif
static int failynPulseOld = FAILYN_PULSE_OLD;
void uiFailynPulseOld(int on) { failynPulseOld = on; }
static int scrPulseFlags;
void uiScrMenuFlags(int flags) { scrPulseFlags = flags; }
static int pulseFlagsOf(int sidx) {
  if (scrMode) return scrPulseFlags;
  if (!failynPulseOld && sidx == UI_SCREEN_MISSIONOVER) return 2;
  return sidx == UI_SCREEN_MAIN || sidx == UI_SCREEN_PAUSE || sidx == UI_SCREEN_PAUSEMSN ? 2 : 0;
}
float uiMenuItemBox[6] = {-1.f};   // служебное: мс пункта, масштаб, рамка строки, как ушла в GE (строка MENUITEM смоука)
static float hnrShown, hnrLastT, hnrTarget; static int hnrPursuit, hnrScript; static float hnrScriptT0;
void uiSetLog(void (*log)(const char *)) { uiLogFn = log; }
void uiTrace(unsigned bits) { traceBits = bits; }
void uiAnimOff(unsigned bits) { animOff = bits; }
void uiPopHook(void (*fn)(int ev, int on)) { popHook = fn; }
void uiForceShow(int tag, float seconds) {
  if ((unsigned)tag >= 32) return;
  if (seconds > 0.f) { hudForce |= 1u << tag; forceUntil[tag] = uiClk + seconds; } else hudForce &= ~(1u << tag);
}
void uiHnrScript(int on) { hnrScript = on; hnrScriptT0 = -1.f; }
static void uiLogf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void uiLogf(const char *fmt, ...) {
  if (!uiLogFn) return;
  char s[160]; va_list a; va_start(a, fmt); vsnprintf(s, sizeof s, fmt, a); va_end(a); uiLogFn(s);
}
// служебно (сценарий/гейт TEST60 №942; шаг часов режется до 0,1 с, как у живых, «uiclkadv 0.3» = подвисший кадр): flag uiclk 1 — часы интерфейса стоят, flag uiclkadv Д — сдвинуть на Д секунд; кадры появления пузыря снимаются по возрасту, а не по скорости эмулятора. В игре не вызывается
static int uiClkHold; static float uiClkPend;
static float uiClkDt;       // №942: шаг часов HUD за этот кадр (с)
static float bubAge;        // №942: возраст пузыря задания с показа, шаг за кадр не больше UI_BUB_DT_CAP (подвисший кадр не съедает появление)
static float objFlyT;       // №942: время полёта значка в угол, тот же шаг
static float popAgeOf(int t, int on) { return (t == UI_T_OBJECTIVE && on && !BUBBLE_LAG942_OLD) ? bubAge : uiClk - (on ? tagOnT[t] : tagOffT[t]); }
void uiClkTest(int mode, float v) { if (mode == 0) { uiClkHold = 0; uiClkPend = 0.f; } else if (mode == 1) uiClkHold = 1; else uiClkPend += v; }
static void uiClockTick(void) {
  unsigned now = sceKernelGetSystemTimeLow();
  if (uiClkHold) { uiClkDt = uiClkInit ? (uiClkPend > .1f ? .1f : uiClkPend) : 0.f; uiClk += uiClkDt; uiClkPend = 0.f; uiClkInit = 1; uiClkLast = now; if (bubAge < 1000.f) bubAge += uiClkDt < UI_BUB_DT_CAP ? uiClkDt : UI_BUB_DT_CAP; return; }
  uiClkDt = 0.f;
  if (uiClkInit) { float d = (now - uiClkLast) * 1e-6f; uiClkDt = d > .1f ? .1f : d; uiClk += uiClkDt; }
  if (bubAge < 1000.f) bubAge += uiClkDt < UI_BUB_DT_CAP ? uiClkDt : UI_BUB_DT_CAP;
  uiClkInit = 1; uiClkLast = now;
}
static int popKind(int tag) {   // какие теги HUD всплывают (ui_anim.h, п. 2); -1 — без анимации
  switch (tag) {
    case UI_T_OBJECTIVE: return BUBBLE_ANIM_OLD ? UI_POP_BOX : UI_POP_PILL;   // №516: строка задания растёт по высоте (ui_anim.h 2в)
    case UI_T_STAGE_COMPLETE: case UI_T_MISSION_COMPLETE: case UI_T_MISSION_FAILED: case UI_T_CARD_TEXT: case UI_T_CARDS:
    case UI_T_ITEMS_COMPLETE: case UI_T_WASPS: case UI_T_HNR_TICKET: case UI_T_COUNTDOWN: case UI_T_ITEM_DROPPED:
      return UI_POP_BANNER;
    case UI_T_COINS: return UI_POP_SLIDE;   // правка Роба 51: въезд сверху / уезд вверх (ui_anim.h, п. 2б)
  }
  return -1;
}
static inline unsigned mulAlpha(unsigned col, float a) {
  if (a >= 1.f) return col;
  unsigned al = (unsigned)((col >> 24) * (a > 0.f ? a : 0.f) + .5f);
  return (col & 0x00ffffffu) | (al << 24);
}
static inline float xfX(float x) { return xf.on ? xf.cx + (x - xf.cx) * xf.s : x; }
static inline float xfSy(void) { return xf.sy > 0.f ? xf.sy : xf.s; }
static inline float xfY(float y) { return xf.on ? xf.cy + (y - xf.cy) * xfSy() : y; }

static const char *str(unsigned i) { return i < H->nstr ? (const char *)pakAtIn(pakSegs, pak, strOff[i]) : ""; }

// файл пакета UIP1 целиком в указатели выше (pak, H, tex ... texPix, tagMask); 0 = ок
// Правка Роба 500 (все пути оптимизации отрисовки, часть «интерфейс»): кэши раскладки живут, пока пакет интерфейса тот же. layGen растёт при каждой смене
// пакета (pakSet / pakRead / uiShutdown); -DUI_LAYOUT_CACHE_OLD=1 — прежнее (без кэшей)
#ifndef UI_LAYOUT_CACHE_OLD
#define UI_LAYOUT_CACHE_OLD 0
#endif
static unsigned layGen = 1;
#ifndef MSNART_ROOM_OLD
#define MSNART_ROOM_OLD 0
#endif
__attribute__((weak)) int (*msnartRoom)(unsigned);   /* msnart.c задаёт настоящий; слабый — чтобы хост-гейты tests/ui линковались без msnart.c */   // msnart.c, ставит main_frontend.h: освободить далёкие клетки города под крупное выделение
// №637/TEST23 (будка: «TELEPHONE BOOTH UNAVAILABLE»): пакет phone.shr (3,1 МБ) читался одним куском, а в раздробленной куче цельного куска
// такого размера нет (самый большой 2,08 МБ при 10 МБ свободно). Теперь пакет крупнее PAK_SEG_MAX читается несколькими кусками: границы
// только на началах разделов и картинок/палитр (их смещения из заголовка и таблицы текстур), поэтому раздел не рвётся; адрес по смещению
// файла даёт pakAtIn. Не влезло — просим движок освободить далёкие клетки города (msnartRoom) и повторяем. PAK_SPLIT_OLD=1 — прежнее, одним куском.
#ifndef PAK_SPLIT_OLD
#define PAK_SPLIT_OLD 0
#endif
#define PAK_SEG_MAX (1u << 20)
#define PAK_SEG_N 32
static unsigned pakSegMax1051 = PAK_SEG_MAX;   // №1051: будка в тесной куче читает пакет кусками помельче (границы по-прежнему только на началах разделов и картинок)
void uiPakSegMax1051(unsigned bytes) { pakSegMax1051 = bytes ? bytes : PAK_SEG_MAX; }
void (*uiPakProgress1060)(unsigned done, unsigned total);   // №1060б: кадр экрана загрузки между кусками чтения пакета (ставит mem1060.h на время сцены)
struct UiSegs { int n; unsigned start[PAK_SEG_N], len[PAK_SEG_N]; unsigned char *p[PAK_SEG_N]; };
static const unsigned char *pakAtIn(const struct UiSegs *sg, const unsigned char *base, unsigned off) {
  if (!sg) return base + off;
  for (int i = 0; i < sg->n; i++) if (off >= sg->start[i] && off - sg->start[i] < sg->len[i]) return sg->p[i] + (off - sg->start[i]);
  return sg->p[0];   // смещение вне пакета: пакет проверен при чтении, сюда не попадаем; безопасный адрес вместо чтения мимо
}
static void pakFreeAll(unsigned char *base, struct UiSegs *sg) {
  if (!sg) { free(base); return; }
  for (int i = 0; i < sg->n; i++) free(sg->p[i]);
  free(sg);
}
static int pakSplitCmp(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
// читает пакет кусками: 0 — ок (sgOut, заголовок в sgOut->p[0]); отрицательное — как pakRead
static int pakReadSplit(SceUID fd, unsigned size, struct UiSegs **sgOut) {
  UiHdr hb; sceIoLseek32(fd, 0, PSP_SEEK_SET);
  if (sceIoRead(fd, &hb, sizeof hb) != (int)sizeof hb || memcmp(&hb, "UIP1", 4)) return -4;
  if ((hb.size && hb.size != size) || hb.offTex > size || hb.ntex > 4096 || hb.offTex + hb.ntex * sizeof(UiTex) > size) return -5;
  UiTex *tt = (UiTex *)malloc(sizeof(UiTex) * (hb.ntex ? hb.ntex : 1)); unsigned *st = (unsigned *)malloc(sizeof(unsigned) * (3u * hb.ntex + 16u));
  if (!tt || !st) { free(tt); free(st); return -3; }
  sceIoLseek32(fd, hb.offTex, PSP_SEEK_SET);
  if (sceIoRead(fd, tt, hb.ntex * sizeof(UiTex)) != (int)(hb.ntex * sizeof(UiTex))) { free(tt); free(st); return -4; }
  unsigned n = 0, so[10] = {0, hb.offTex, hb.offItem, hb.offScreen, hb.offFont, hb.offStr, hb.offSfont, hb.offIcon, hb.offSlot, hb.offPts};
  for (int i = 0; i < 10; i++) if (so[i] < size) st[n++] = so[i];
  for (unsigned t = 0; t < hb.ntex; t++) { if (tt[t].offPix < size) st[n++] = tt[t].offPix; if (tt[t].offClut < size) st[n++] = tt[t].offClut; }
  free(tt);
  qsort(st, n, sizeof(unsigned), pakSplitCmp);
  struct UiSegs *sg = (struct UiSegs *)calloc(1, sizeof *sg); if (!sg) { free(st); return -3; }
  unsigned a = 0;   // первая граница текущего куска
  for (unsigned i = 0; i < n; ) {
    if (i && st[i] == st[i - 1]) { i++; continue; }
    unsigned b = i + 1; while (b < n && st[b] == st[i]) b++;   // дубли
    unsigned segStart = st[i], segEnd = b < n ? st[b] : size; unsigned j = b;
    while (j < n) { unsigned e = (j + 1 < n ? st[j + 1] : size); unsigned nj = j + 1; while (nj < n && st[nj] == st[j]) nj++; e = nj < n ? st[nj] : size;
      if (e - segStart > pakSegMax1051) break; segEnd = e; j = nj; }
    if (sg->n >= PAK_SEG_N) { pakFreeAll(0, sg); free(st); return -5; }
    sg->start[sg->n] = segStart; sg->len[sg->n] = segEnd - segStart; sg->n++;
    i = j; (void)a;
  }
  free(st);
  for (int i = 0; i < sg->n; i++) {
    unsigned char *m = (unsigned char *)memalign(64, sg->len[i]);
    if (!m && !MSNART_ROOM_OLD && msnartRoom && msnartRoom(sg->len[i])) m = (unsigned char *)memalign(64, sg->len[i]);
    if (!m) { pakFreeAll(0, sg); return -3; }
    sg->p[i] = m;
    sceIoLseek32(fd, sg->start[i], PSP_SEEK_SET);
    if (sceIoRead(fd, m, sg->len[i]) != (int)sg->len[i]) { sg->n = i + 1; pakFreeAll(0, sg); return -4; }
    sceKernelDcacheWritebackRange(m, sg->len[i]);
    if (uiPakProgress1060) uiPakProgress1060((unsigned)(i + 1), (unsigned)sg->n);
  }
  *sgOut = sg; return 0;
}
static int pakRead(const char *path) {
  layGen++;
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
  if (fd < 0) return -1;
  int size = (int)sceIoLseek(fd, 0, PSP_SEEK_END);
  sceIoLseek(fd, 0, PSP_SEEK_SET);
  if (size < 128) { sceIoClose(fd); return -2; }
  struct UiSegs *sg = 0; unsigned char *m;
  if (!PAK_SPLIT_OLD && (unsigned)size > pakSegMax1051) {
    int sr = pakReadSplit(fd, (unsigned)size, &sg); sceIoClose(fd); if (sr) return sr;
    m = sg->p[0];
  } else {
  m = (unsigned char *)memalign(64, (size_t)size);
  if (!m && !MSNART_ROOM_OLD && msnartRoom && msnartRoom((unsigned)size)) m = (unsigned char *)memalign(64, (size_t)size);   // №637: цельный кусок под пакет — освободить далёкие клетки
  if (!m) { sceIoClose(fd); return -3; }
  int got = sceIoRead(fd, m, (SceSize)size);
  sceIoClose(fd);
  if (got != size || memcmp(m, "UIP1", 4)) { free(m); return -4; }
  }
#define PKP(o) pakAtIn(sg, m, (o))
  {                                                          // обрезанный / испорченный пакет — отказ, а не чтение мимо файла
    const UiHdr *h = (const UiHdr *)m; unsigned sz = (unsigned)size;
    if ((h->size && h->size != sz) || h->offTex > sz || h->offItem > sz || h->offScreen > sz || h->offFont > sz || h->offStr > sz ||
        h->offSfont > sz || h->offIcon > sz || h->offSlot > sz || h->offPts > sz ||
        h->nscreen > 64 || h->offScreen + h->nscreen * 16u > sz || h->offItem + h->nitem * 64u > sz || h->offStr + h->nstr * 4u > sz) {
      pakFreeAll(m, sg); return -5; }
  }
  pak = m; pakSegs = sg; H = (const UiHdr *)m;
  tex = (const UiTex *)PKP(H->offTex);
  items = (const UiItem *)PKP(H->offItem);
  screens = (const UiScreen *)PKP(H->offScreen);
  fonts = (const UiFont *)PKP(H->offFont);
  strOff = (const unsigned *)PKP(H->offStr);
  sfonts = (const UiSGlyph *)PKP(H->offSfont);
  icons = (const UiIcon *)PKP(H->offIcon);
  slots = (const UiSlot *)PKP(H->offSlot);
  pts = (const UiPt *)PKP(H->offPts);
  if (!sg) sceKernelDcacheWritebackRange(m, (unsigned)size);         // GE reads textures/CLUTs from RAM (у кусков — при чтении)
  texPix = (const unsigned char **)malloc(sizeof(*texPix) * (H->ntex ? H->ntex : 1));
  if (!texPix) { pakFreeAll(m, sg); pak = 0; pakSegs = 0; H = 0; return -3; }
  for (unsigned t = 0; t < H->ntex; t++) texPix[t] = PKP(tex[t].offPix);
  memset(tagMask, 0, sizeof tagMask);
  for (unsigned s = 0; s < H->nscreen && s < UI_SCREEN_MAX; s++) tagMask[s] = screens[s].tags;
  return 0;
}
#undef PKP

int uiInit(const char *path) {
  int r = pakRead(path);
  if (r) return r;
  tagMask[UI_SCREEN_HUD] |= 1u << UI_T_HNR_FULL; // clipped continuously, empty at zero
  memset(fieldv, 0, sizeof fieldv);
  fieldv[UI_F_LAPS] = 3; fieldv[UI_F_CARDS_TOTAL] = 7; fieldv[UI_F_COLLECT_TOTAL] = 10; fieldv[UI_F_POSITION] = 1;
  rpx = rpz = rhead = 0; nricon = 0;
  msgText[0] = 0; msgUntil = 0; paused = 0; pauseSel = 0; pauseClosing = 0;
  memset(menuOff, 0, sizeof menuOff); memset(menuHid, 0, sizeof menuHid);
  // pause items that have no function in the port yet: dimmed and skipped (uiMenuEnable turns them on)
  menuOff[UI_SCREEN_PAUSE] = (1u << UI_P_MISSION_SELECT) | (1u << UI_P_LEVEL_PROGRESS) | (1u << UI_P_VIEW_CARDS) |
                             (1u << UI_P_OPTIONS) | (1u << UI_P_SAVE_GAME);
  menuOff[UI_SCREEN_PAUSEMSN] = 1u << UI_PM_OPTIONS;   // OPTIONS — когда есть pmenu.shr (main.c)
  pauseScr = UI_SCREEN_PAUSE;
  return 0;
}

void uiShutdown(void) { layGen++; uiScrWarmClear(); if (pak) pakFreeAll(pak, pakSegs); if (texPix) free(texPix); pak = 0; pakSegs = 0; H = 0; texPix = 0; }

// ---- второй пакет: экраны вокруг игры (boot.shr / load.shr). Указатели пакета меняются местами на время вызова,
// поля (fieldv) общие: uiScrDraw ставит свои и возвращает UI_F_LEVEL как было
typedef struct {
  unsigned char *pak; struct UiSegs *seg; const UiHdr *H; const UiTex *tex; const UiItem *items; const UiScreen *screens; const UiFont *fonts;
  const unsigned *strOff; const UiSGlyph *sfonts; const UiIcon *icons; const UiSlot *slots; const UiPt *pts;
  const unsigned char **texPix; unsigned tagMask[UI_SCREEN_MAX], menuOff[UI_SCREEN_MAX];
} UiPak;
static UiPak mainPak, scrPak;
static UiPak warmPak;
static char warmPath[320];
static void pakGet(UiPak *p) {
  p->pak = pak; p->seg = pakSegs; p->H = H; p->tex = tex; p->items = items; p->screens = screens; p->fonts = fonts; p->strOff = strOff;
  p->sfonts = sfonts; p->icons = icons; p->slots = slots; p->pts = pts; p->texPix = texPix; memcpy(p->tagMask, tagMask, sizeof tagMask);
  memcpy(p->menuOff, menuOff, sizeof menuOff);
}
static void pakSet(const UiPak *p) {
  layGen++;
  pak = p->pak; pakSegs = p->seg; H = p->H; tex = p->tex; items = p->items; screens = p->screens; fonts = p->fonts; strOff = p->strOff;
  sfonts = p->sfonts; icons = p->icons; slots = p->slots; pts = p->pts; texPix = p->texPix; memcpy(tagMask, p->tagMask, sizeof tagMask);
  memcpy(menuOff, p->menuOff, sizeof menuOff);
}
int uiScrReady(void) { return scrPak.H != 0; }
// uiScrRowText: текст строки k экрана sc второго пакета из кода (слоты сохранений, viewer/savemenu.c). Своя таблица на каждый
// экран: подпись слота 0 не должна попадать на кнопку 0 экрана вопроса/сообщения (было 28.09: CONTINUE -> «LEVEL 1 28/09»)
static const char *scrRowTxt[UI_SCREEN_MAX][12];
static unsigned scrTint[UI_SCREEN_MAX][32];
#ifdef UI_ROWTXT_GLOBAL   // отрицательный контроль гейта tests/savegame (сборка с -DUI_ROWTXT_GLOBAL): одна таблица на все экраны, как до 28.09
#define RT_SC(sc) 0
#else
#define RT_SC(sc) (sc)
#endif
static void pauseEv(int ev, int on);
void uiScrRowText(int sc, int row, const char *s) { if ((unsigned)sc < UI_SCREEN_MAX && row >= 0 && row < 12) scrRowTxt[RT_SC(sc)][row] = s; }
const char *uiScrRowShown(int sc, int row) {   // что нарисовано в строке меню row экрана sc второго пакета (гейт: кнопки, не подписи слотов)
  if (!scrPak.H || (unsigned)sc >= scrPak.H->nscreen || (unsigned)sc >= UI_SCREEN_MAX) return "";
  if (row >= 0 && row < 12 && scrRowTxt[RT_SC(sc)][row]) return scrRowTxt[RT_SC(sc)][row];
  const UiScreen *S2 = &scrPak.screens[sc];
  for (unsigned i = 0; i < S2->count; i++) { const UiItem *it = &scrPak.items[S2->first + i];
    if (it->type == T_TEXT && (it->flags & F_MENU) && it->menu == row) { UiPak keep; pakGet(&keep); pakSet(&scrPak); const char *r = str(it->first); pakSet(&keep); return r; } }
  return "";
}
void uiScrClose(void) {
  if (scrPak.pak && scrPak.pak != warmPak.pak) pakFreeAll(scrPak.pak, scrPak.seg);
  if (scrPak.texPix) free((void *)scrPak.texPix);
  memset(&scrPak, 0, sizeof scrPak); memset(scrRowTxt, 0, sizeof scrRowTxt); memset(scrTint, 0, sizeof scrTint);
}
void uiScrWarmClear(void) {
  if(warmPak.pak && scrPak.pak==warmPak.pak)uiScrClose();
  if(warmPak.pak)pakFreeAll(warmPak.pak,warmPak.seg);free((void*)warmPak.texPix);
  memset(&warmPak,0,sizeof warmPak);warmPath[0]=0;
}
int uiScrWarm(const char *path) {
  if(!path || strlen(path)>=sizeof warmPath)return -1;
  if(warmPak.H&&!strcmp(path,warmPath))return 0;
  uiScrWarmClear();UiPak keep,empty={0};pakGet(&keep);pakSet(&empty);
  int r=pakRead(path);if(!r){pakGet(&warmPak);snprintf(warmPath,sizeof warmPath,"%s",path);}
  pakSet(&keep);return r;
}
int uiScrWarmReady(const char *path){return warmPak.H&&path&&!strcmp(path,warmPath);}
int uiScrOpen(const char *path) {
  uiScrClose();
  if(warmPak.H && path && !strcmp(path,warmPath)){
    scrPak=warmPak;
    unsigned n=warmPak.H->ntex?warmPak.H->ntex:1;
    scrPak.texPix=malloc(n*sizeof *scrPak.texPix);
    if(!scrPak.texPix){memset(&scrPak,0,sizeof scrPak);return -3;}
    memcpy((void*)scrPak.texPix,warmPak.texPix,n*sizeof *scrPak.texPix);
    return 0;
  }
  pakGet(&mainPak); memset(&scrPak, 0, sizeof scrPak); pakSet(&scrPak);
  int r = pakRead(path);
  if (!r) pakGet(&scrPak);
  pakSet(&mainPak);
  return r;
}
void uiScrShow(int s, int tag, int on) {
  if (!scrPak.H || (unsigned)s >= UI_SCREEN_MAX || (unsigned)tag >= 32) return;
  if (on) scrPak.tagMask[s] |= 1u << tag; else scrPak.tagMask[s] &= ~(1u << tag);
  scrPak.tagMask[s] |= 1u;                                  // ALWAYS
}
static void drawScreen(int sidx, int selected);
void uiScrTint(int screen, int tag, unsigned abgr) {
  if ((unsigned)screen < UI_SCREEN_MAX && (unsigned)tag < 32) scrTint[screen][tag] = abgr;
}
static int hideDy(const UiScreen *sc, int sidx, const UiItem *it);   // правка 137 (ниже, у menuStep)
void uiScrDraw(int screen, int progress, float t, int level) {
  if (!scrPak.H) return;
  if (progress < 0) progress = 0; if (progress > 1000) progress = 1000;
  int lv = fieldv[UI_F_LEVEL];
  fieldv[UI_F_PROGRESS] = progress; fieldv[UI_F_LEVEL] = level; scrT = t > 0.f ? t : 0.f;
  pakGet(&mainPak); pakSet(&scrPak); scrMode = 1;
  spinPx = spinN = 0;
  drawScreen(screen, -1);
  scrMode = 0; pakSet(&mainPak); spinT = -1.f; spinA = 1.f; raysStyle = 0;   // uiScrSpin, uiScrRays — на один вызов
  fieldv[UI_F_LEVEL] = lv;
  if (uiLogFn && spinN) {   // №703: учёт заливки экрана загрузки/газеты раз в ~1 с: всё покрытие, из него вращающиеся звёзды (после обрезки), слои
    static unsigned lastScrLog;
    unsigned now = sceKernelGetSystemTimeLow();
    if (now - lastScrLog > 1000000u) { lastScrLog = now; uiLogf("SCRFILL screen=%d draws=%d px_all=%d spin_px=%d spin_layers=%d\n", screen, st.draws, st.pixels, spinPx, spinN); }
  }
}

// ---- меню оригинала во втором пакете (tools/menu_export.py: menu.shr, pmenu.shr): строки, значения, ввод
// №557: прямоугольник первой картинки (T_IMG) с меткой tag на экране второго пакета, px: куда ставить 3D-остов вместо картинки (viewer/phone_view.c)
int uiScrTagRect(int screen, int tag, int *r) {
  if (!scrPak.H || (unsigned)screen >= scrPak.H->nscreen || (unsigned)tag >= 32) return 0;
  const UiScreen *sc = &scrPak.screens[screen];
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &scrPak.items[sc->first + i];
    if (it->type != T_IMG || it->tag != tag || !it->count) continue;
    const UiSlot *sl = &scrPak.slots[it->first]; r[0] = sl->x; r[1] = sl->y; r[2] = sl->w; r[3] = sl->h; return 1;
  }
  return 0;
}
static const UiScreen *scrScreen(int screen) { return (scrPak.H && (unsigned)screen < scrPak.H->nscreen) ? &scrPak.screens[screen] : 0; }
int uiScrRows(int screen) { const UiScreen *sc = scrScreen(screen); return sc ? (int)sc->nmenu : 0; }
int uiScrRowKind(int screen, int row, int *nopt) {
  const UiScreen *sc = scrScreen(screen); int kind = UI_ROW_ITEM;
  if (nopt) *nopt = 0;
  if (!sc || row < 0 || row > 11) return -1;
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &scrPak.items[sc->first + i];
    if (it->field != UI_F_ROW0 + row) continue;
    if (it->type == T_BAR) return UI_ROW_SLIDER;
    if (it->type == T_TEXT && it->fmt == FMT_SELECT) { if (nopt) *nopt = it->count; kind = UI_ROW_OPTIONS; }
  }
  return kind;
}
const char *uiScrRowLabel(int screen, int row) {
  const UiScreen *sc = scrScreen(screen);
  if (!sc) return "";
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &scrPak.items[sc->first + i];
    if (it->type == T_TEXT && (it->flags & F_MENU) && it->menu == row)
      return it->first < scrPak.H->nstr ? (const char *)pakAtIn(scrPak.seg, scrPak.pak, scrPak.strOff[it->first]) : "";
  }
  return "";
}
void uiScrSetRow(int row, int v) { if (row >= 0 && row <= 11) fieldv[UI_F_ROW0 + row] = v; }
int  uiScrGetRow(int row) { return (row >= 0 && row <= 11) ? fieldv[UI_F_ROW0 + row] : 0; }
void uiScrEnable(int screen, int row, int on) {
  if ((unsigned)screen >= UI_SCREEN_MAX || (unsigned)row >= 32) return;
  if (on) scrPak.menuOff[screen] &= ~(1u << row); else scrPak.menuOff[screen] |= 1u << row;
}
int uiScrEnabled(int screen, int row) {
  return (unsigned)screen < UI_SCREEN_MAX && (unsigned)row < 32 && row < uiScrRows(screen) && !(scrPak.menuOff[screen] >> row & 1);
}
void uiScrDrawMenu(int screen, int sel, float t) {
  if (!scrPak.H) return;
  scrT = t > 0.f ? t : 0.f;
  pakGet(&mainPak); pakSet(&scrPak); scrMode = 1;
  drawScreen(screen, sel);
  scrMode = 0; pakSet(&mainPak); scrPulseFlags = 0; spinT = -1.f; spinA = 1.f; raysStyle = 0;   // 413: флаги пульса — на один вызов (и uiScrSpin, uiScrRays)
}
// №763 (Роб 17.10, TEST32: «появление и уход лучей фейдом, как градиент»; лучи будки №750 и гардероба №761): свои часы вращения у экрана, который
// сбрасывает время показа при смене фазы (будка и гардероб обнуляют t на входе в выбор и на выходе — лучи прыгали на другой угол), и доля яркости
void uiScrSpin(float t, float a) { spinT = t; spinA = a < 0.f ? 0.f : a > 1.f ? 1.f : a; }
int uiScrScreens(void) { return scrPak.H ? (int)scrPak.H->nscreen : 0; }
int uiScrBackInput(unsigned hit) {   // экран без строк (VIEW CREDITS): △ / ○ — назад со звуком back, как у меню
  if (!(hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE))) return 0;
  pauseEv(UI_EV_BACK, 1); return 1;
}
// звуки экранов меню (OPTIONS, Sound, Settings, слоты сохранений), как в паузе (frontend.spt): смена строки и значения —
// scroll, пункт — accept, △/○ — back. Значение строки (ползунок, вариант) — scroll: правило порта, отдельного звука в данных нет
int uiScrMenuInput(int screen, int *sel, unsigned hit) {
  int n = uiScrRows(screen);
  if (n <= 0 || !sel) return UI_SCR_NONE;
  if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { pauseEv(UI_EV_BACK, 1); return UI_SCR_BACK; }   // △ — Back (Back.pag), ○ — тоже назад
  int dir = (hit & PSP_CTRL_UP) ? -1 : (hit & PSP_CTRL_DOWN) ? 1 : 0, was = *sel;
  if (!uiScrEnabled(screen, *sel) && !dir) dir = 1;
  if (dir) for (int k = 1; k <= n; k++) { int j = *sel + dir * k, i = (j % n + n) % n; if (uiPadRepeated && (j < 0 || j >= n)) break;   // правка 202
                                         if (uiScrEnabled(screen, i)) { *sel = i; break; } }
  if (*sel != was && (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN))) pauseEv(UI_EV_SCROLL, 1);
  if (!uiScrEnabled(screen, *sel)) return UI_SCR_NONE;
  int nopt, kind = uiScrRowKind(screen, *sel, &nopt), v = fieldv[UI_F_ROW0 + *sel];
  if (kind == UI_ROW_OPTIONS && nopt > 0 && (hit & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_CROSS))) {
    v = ((hit & PSP_CTRL_LEFT) ? v + nopt - 1 : v + 1) % nopt; fieldv[UI_F_ROW0 + *sel] = v; pauseEv(UI_EV_SCROLL, 1); return UI_SCR_CHANGED;
  }
  if (kind == UI_ROW_SLIDER && (hit & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
    int v0 = v; v += (hit & PSP_CTRL_LEFT) ? -100 : 100; v = v < 0 ? 0 : v > 1000 ? 1000 : v; fieldv[UI_F_ROW0 + *sel] = v;
    if (v != v0) pauseEv(UI_EV_SCROLL, 1);
    return UI_SCR_CHANGED;
  }
  if (kind == UI_ROW_ITEM && (hit & PSP_CTRL_CROSS)) { pauseEv(UI_EV_ACCEPT, 1); return *sel; }
  return UI_SCR_NONE;
}

// ---- VRAM: copy the textures the in-game HUD samples (not pause/menu pages) into free VRAM
static unsigned texBytes(const UiTex *x) { return x->psm == 4 ? x->bufw * x->rows / 2 : x->bufw * x->rows; }
int uiUseVram(unsigned vramOffset, unsigned maxBytes) {
  if (!H) return 0;
  unsigned char need[64]; memset(need, 0, sizeof need);
  const UiScreen *sc = &screens[UI_SCREEN_HUD];
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    switch (it->type) {
      case T_IMG: case T_SLOT: case T_BAR: case T_SECTOR:
        for (unsigned k = 0; k < it->count; k++) need[slots[it->first + k].tex & 63] = 1;
        break;
      case T_TEXT: if (it->font < H->nfont) need[fonts[it->font].h.tex & 63] = 1; break;
      case T_NUM:
        if (it->font < H->nsfont) for (int c = 0; c < 256; c++) { const UiSGlyph *g = &sfonts[it->font * 256 + c]; if (g->flags && g->tex != 0xffff) need[g->tex & 63] = 1; }
        if (it->prefix) need[slots[it->prefix - 1].tex & 63] = 1;
        break;
      case T_RADARMAP: if (H->mapTex >= 0) need[H->mapTex & 63] = 1; break;
      case T_RADARICONS: for (int k = 0; k < UI_R_COUNT; k++) if (icons[k].tex != 0xffff) need[icons[k].tex & 63] = 1; break;
    }
  }
  if (H->nfont) need[fonts[0].h.tex & 63] = 1;             // uiText (debug line / placeholders)
  unsigned used = 0;
  uintptr_t base = ((uintptr_t)sceGeEdramGetAddr() + vramOffset) | 0x40000000u;   // uncached alias for the copy
  for (unsigned t = 0; t < H->ntex && t < 64; t++) {
    if (!need[t]) continue;
    unsigned n = texBytes(&tex[t]), at = (used + 15) & ~15u;
    if (at + n > maxBytes) continue;                        // does not fit: stays in RAM
    memcpy((void *)(base + at), texPix[t], n);
    texPix[t] = (const unsigned char *)(((uintptr_t)sceGeEdramGetAddr() + vramOffset + at));
    used = at + n;
  }
  return (int)used;
}

void uiHudSet(int f, int v) {
  if ((unsigned)f >= UI_F_COUNT) return;
  if (f == UI_F_COUNTDOWN && fieldv[f] != v) cdNew = 1;       // 3 -> 2 -> 1: каждое число всплывает (ui_anim.h, п. 2)
  fieldv[f] = v;
}
#ifndef COUNTDOWN_LINE_OLD
#define COUNTDOWN_LINE_OLD 0   // №630: 1 — слова отсчёта («PSYCHE», «GO!», реплика соперника) в пузыре внизу, как до правки
#endif
static char cdText[48];
void uiCountdownText(const char *text) {   // №630 (Роб 15.10, оригинал: слова отсчёта в центре экрана тем же зелёным шрифтом, что цифры); NULL — снова число
  char t[48]; int n = 0;
  if (text) for (; text[n] && n < 47; n++) t[n] = (char)toupper((unsigned char)text[n]);
  t[n] = 0;
  if (strcmp(t, cdText)) { strcpy(cdText, t); cdNew = 1; }
}
int uiHudGet(int f) { return (unsigned)f < UI_F_COUNT ? fieldv[f] : 0; }
void uiShow(int s, int tag, int on) {
  if ((unsigned)s >= UI_SCREEN_COUNT || (unsigned)tag >= 32) return;
  if (on) tagMask[s] |= 1u << tag; else tagMask[s] &= ~(1u << tag);
  if (s == UI_SCREEN_HUD && tag == UI_T_HNR_METER) {
    if (on) tagMask[s] |= 1u << UI_T_HNR_FULL; else tagMask[s] &= ~(1u << UI_T_HNR_FULL);
  }
  tagMask[s] |= 1u;                                         // ALWAYS
}
void uiRadarSetPlayer(float x, float z, float h) { rpx = x; rpz = z; rhead = h; }
void uiRadarSetRange(float m) { if (m > 1.f) rrange = m; }
void uiRadarClear(void) { nricon = 0; }
void uiRadarAdd(int type, float x, float z) {
  if (nricon >= UI_RADAR_MAX || (unsigned)type >= UI_R_COUNT) return;
  ricon[nricon].type = (unsigned char)type; ricon[nricon].x = x; ricon[nricon].z = z; ricon[nricon].hasH = 0; nricon++;
}
void uiRadarAddH(int type, float x, float z, float heading) {
  int n0 = nricon; uiRadarAdd(type, x, z); if (nricon > n0) { ricon[n0].hasH = 1; ricon[n0].h = heading; }
}
UiStats uiLastStats(void) { return st; }

// ---------------------------------------------------------------------------------------
// vertex memory: taken from the display list, written through the cached alias (like hud.c)
static void *vbeg(unsigned bytes, void **flushBase) {
  uintptr_t m = (uintptr_t)sceGuGetMemory((int)bytes + 128) & ~(uintptr_t)0x40000000;
  void *p = (void *)((m + 63) & ~(uintptr_t)63);
  *flushBase = p;
  return p;
}
static void vend(void *base, unsigned bytes) { sceKernelDcacheWritebackInvalidateRange(base, (bytes + 63) & ~63u); }

static void texEnable(int on) {
  if (on == texOn) return;
  texOn = on;
  if (on) sceGuEnable(GU_TEXTURE_2D); else sceGuDisable(GU_TEXTURE_2D);
}
static unsigned char __attribute__((aligned(16))) pix8Tex[PIX8_TEXBYTES];   // правка 126: атлас 128×64 T4 (4 КБ RAM)
static unsigned int __attribute__((aligned(64))) pix8Clut[16];
#define PIX8_TEXID 0xffffu      // номер «текстуры» грани pix8 (вне таблицы пакета)
static void bindTex(unsigned t, int linear) {
  texEnable(1);
  if (t == PIX8_TEXID) {
    if ((int)t != curTex) {
      sceGuClutMode(GU_PSM_8888, 0, 0x0f, 0); sceGuClutLoad(2, pix8Clut);
      sceGuTexMode(GU_PSM_T4, 0, 0, 0); sceGuTexImage(0, PIX8_TW, PIX8_TH, PIX8_TW, pix8Tex);
      curTex = (int)t; st.binds++;
    }
    linear = 0;                 // пиксельный шрифт — только 1:1 без фильтрации
  } else if ((int)t != curTex && t < H->ntex) {
    const UiTex *x = &tex[t];
    sceGuClutMode(GU_PSM_8888, 0, x->nclut > 16 ? 0xff : 0x0f, 0);
    sceGuClutLoad((int)(x->nclut / 8), pakAtIn(pakSegs, pak, x->offClut));
    sceGuTexMode(x->psm == 4 ? GU_PSM_T4 : GU_PSM_T8, 0, 0, (int)(x->flags & 1));
    sceGuTexImage(0, x->w, x->h, (int)x->bufw, texPix[t]);          // (sends TEX_FLUSH)
    curTex = (int)t; st.binds++;
  }
  int f = linear ? GU_LINEAR : GU_NEAREST;
  if (f != curFilt) { curFilt = f; sceGuTexFilter(f, f); }
}

static void beginUi(void) {
  curTex = -1; curFilt = -1; texOn = -1;
  sceGuDisable(GU_DEPTH_TEST);
  sceGuDepthMask(GU_TRUE);
  sceGuDisable(GU_FOG);
#if !UI_ALPHA_TEST_OLD
  // Правка Роба 287 (04.10: «вокруг карты квадрат полупрозрачный, сероватый; то же вокруг аватарки Мардж»): кадр 5650 с дизерингом
  // GE (правка 230) — железо пишет и полностью прозрачные пиксели квадрата картинки, и дизеринг рисует на них сетку 4x4 поверх 3D:
  // виден квадрат вокруг круглого радара и аватарки. Прозрачное (альфа 0) не пишется вовсе. Эмулятор дизеринг не рисует — только
  // железо. -DUI_ALPHA_TEST_OLD=1 — прежнее
  sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0, 0xff);
#else
  sceGuDisable(GU_ALPHA_TEST);
#endif
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuShadeModel(GU_SMOOTH);
}

// one textured 1:1 sprite (2 vertices) into a buffer
static inline VSpr *putSpr(VSpr *v, int u, int vv, int w, int h, int x, int y, unsigned col) {
  v[0].u = (unsigned short)u; v[0].v = (unsigned short)vv; v[0].color = col; v[0].x = (short)x; v[0].y = (short)y; v[0].z = 0; v[0].pad = 0;
  v[1].u = (unsigned short)(u + w); v[1].v = (unsigned short)(vv + h); v[1].color = col;
  v[1].x = (short)(x + w); v[1].y = (short)(y + h); v[1].z = 0; v[1].pad = 0;
  return v + 2;
}

// sprite batcher: consecutive 1:1 sprites on the same texture page go out as ONE draw call
// (draw order is kept: any other kind of draw flushes the batch first)
#define BATCH_MAX 384
static VSpr batch[BATCH_MAX * 2];
static VFTex fbatch[BATCH_MAX * 2];                          // те же спрайты с масштабом (xf): вершины float, фильтр линейный
static int bn, btex = -1, bmode;                             // bmode 1 — в пачке спрайты fbatch
static void bflush(void) {
  if (!bn) return;
  if (bmode) {
    unsigned bytes = (unsigned)bn * 2 * sizeof(VFTex);
    void *b; VFTex *v = (VFTex *)vbeg(bytes, &b);
    memcpy(v, fbatch, bytes);
    vend(b, bytes);
    bindTex((unsigned)btex, 1);
    sceGuDrawArray(GU_SPRITES, VT_FTEX, bn * 2, 0, v);
    st.draws++; st.verts += bn * 2;
    for (int i = 0; i < bn; i++) st.pixels += (int)((fbatch[2 * i + 1].x - fbatch[2 * i].x) * (fbatch[2 * i + 1].y - fbatch[2 * i].y));
    bn = 0; return;
  }
  unsigned bytes = (unsigned)bn * 32;
  void *b; VSpr *v = (VSpr *)vbeg(bytes, &b);
  memcpy(v, batch, bytes);
  vend(b, bytes);
  bindTex((unsigned)btex, 0);
  sceGuDrawArray(GU_SPRITES, VT_SPR, bn * 2, 0, v);
  st.draws++; st.verts += bn * 2;
  for (int i = 0; i < bn; i++) st.pixels += (batch[2 * i + 1].x - batch[2 * i].x) * (batch[2 * i + 1].y - batch[2 * i].y);
  bn = 0;
}
static float coinIc[4]; static int coinIcSeen, coin3d;   // правка 79: место иконки монеты у счёта (центр x, y, высота, альфа)
void uiCoin3D(int on) { coin3d = on; }
int uiCoinIcon(float *cx, float *cy, float *h, float *alpha) {   // 1 — счёт монет нарисован в этом кадре (иконка здесь)
  int seen = coinIcSeen; coinIcSeen = 0; if (!seen) return 0;
  *cx = coinIc[0]; *cy = coinIc[1]; *h = coinIc[2]; *alpha = coinIc[3]; return 1;
}
static int gDx;                                              // №710: сдвиг по x рамки и текста пузыря без аватара к левому полю
static int dbgWrapW, dbgNl, dbgWrapCalc;   // №942 (гейт): ширина строки и число строк последнего текста пузыря; сколько раз за прогон считался перенос
int uiBubble992Old = BUBBLE992_OLD;   // №992: 1 — появление пузыря как в TEST70 (flag bubble992 0 / файл bubble992old)
void uiBubble992Set(int old) { uiBubble992Old = old; }
// №1004 (Роб 26.10 после TEST71, «в который раз»: текст в пузыре сообщения появляется сначала узким по ширине зоны, потом расширяется — убрать; пусть появится чуть позже, и всё):
// с №992 текст масштабировался вместе с рамкой (0,88 → 1). Теперь текст пузыря задания и окна сообщения, окна сообщения №639 и подсказки №516 НЕ масштабируется никогда: пока рамка
// раскрывается или уходит, текста нет; когда рамка стала полной, текст сразу в конечной раскладке и ширине, проявляется за UI_TL1004_FADE. Аватар и значок — как в №992.
// TEXTLATE1004_OLD=1 / файл textlate1004old рядом с EBOOT / flag textlate1004 0 — как в TEST71
#ifndef TEXTLATE1004_OLD
#define TEXTLATE1004_OLD 0
#endif
#define UI_TL1004_DELAY 0.06f   // с: рамка раскрылась (3 шага №992 по 0,02 с)
#define UI_TL1004_FADE  0.08f   // с: проявление текста на месте
int uiTextLate1004Old = TEXTLATE1004_OLD;
int uiBubble1040 = !BUBBLE1040_OLD;   // №1040 (ui_anim.h)
int uiDrawLog1040;   // служебно: журнал цены кадра интерфейса во время появления без трассы POP (она сама тратит время)
void uiDrawLog1040Set(int on) { uiDrawLog1040 = on; }
void uiBubble1040Set(int on) { if (uiBubble1040 != (on != 0)) { uiBubble1040 = on != 0; layGen++; } }
void uiTextLate1004Set(int old) { uiTextLate1004Old = old; }
static int bubSlideNow;                                       // №942: сдвиг вправо текста пузыря в начале проявления (px)
// №942: содержимое пузыря задания (текст, аватар, значок) — всё, кроме самой капсулы (широкая картинка)
static int bubContent(const UiItem *it) { return it->type == T_TEXT || it->type == T_SLOT || (it->type == T_IMG && it->w * 2 < it->h * 3); }
static int gDy;                                              // сдвиг по y текущего элемента (стопка оверлеев миссии)
#define OBJ_SHOW_S 3.5f   // с: задание с рамкой и текстом внизу слева, потом свёрнуто (запись PS2 2:13 -> 2:16)
#define OBJ_FLY_S .35f    // с: значок уезжает вверх
#ifndef UI_OBJTOP_OLD
#define UI_OBJTOP_OLD 0   // правка 365: 1 — свёрнутый значок по верху слота (высокий значок обрезан краем)
#endif
#define OBJ_TOP_Y 1       // px: верх рамки значка в свёрнутом виде (угол вверху слева, запись PS2 2:16: было 6; правка Роба 157 — на 5 px ближе к краю)
static int objCollapse = 1;   // 0 — прежнее поведение (рамка задания всё время), отрицательный контроль
void uiObjectiveCollapse(int on) { objCollapse = on; }
static inline void badd(int t, int u, int v, int w, int h, int x, int y, unsigned col) {
  int m = xf.on; x += gDx;
  if (m && t == (int)PIX8_TEXID) {   // правка 126: пиксельный шрифт во всплывании не масштабируется — целое место, только прозрачность
    x = (int)floorf(xfX((float)x) + .5f); y = (int)floorf(xfY((float)(y + gDy)) + .5f) - gDy; col = mulAlpha(col, xf.a); m = 0;
  }
  if (t != btex || bn >= BATCH_MAX || m != bmode) { bflush(); btex = t; bmode = m; }
  if (!m) {
    putSpr(&batch[2 * bn++], u, v, w, h, x, y + gDy, col);
    if (x < lastSpr[0]) lastSpr[0] = x; if (y + gDy < lastSpr[1]) lastSpr[1] = y + gDy;
    if (x + w > lastSpr[2]) lastSpr[2] = x + w; if (y + gDy + h > lastSpr[3]) lastSpr[3] = y + gDy + h;
    return;
  }
  float x0 = xfX((float)x), y0 = xfY((float)(y + gDy)), x1 = x0 + w * xf.s, y1 = y0 + h * xfSy();   // масштаб от точки xf
  unsigned c = mulAlpha(col, xf.a);
  VFTex *q = &fbatch[2 * bn++];
  q[0] = (VFTex){(float)u, (float)v, c, x0, y0, 0.f};
  q[1] = (VFTex){(float)(u + w), (float)(v + h), c, x1, y1, 0.f};
  if (x0 < lastSpr[0]) lastSpr[0] = x0; if (y0 < lastSpr[1]) lastSpr[1] = y0;   // рамка нарисованного (гейт: замер)
  if (x1 > lastSpr[2]) lastSpr[2] = x1; if (y1 > lastSpr[3]) lastSpr[3] = y1;
}

// №646: спрайт с растяжением (источник sw x sh -> приёмник dw x dh); тот же батчер и тот же масштаб всплывания xf, что у badd
static void baddS(int t, int u, int v, int sw, int sh, int x, int y, int dw, int dh, unsigned col) {
  x += gDx;
  if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
  int m = xf.on;
  if (t != btex || bn >= BATCH_MAX || m != bmode) { bflush(); btex = t; bmode = m; }
  if (!m) {
    VSpr *q = &batch[2 * bn++];
    putSpr(q, u, v, sw, sh, x, y + gDy, col); q[1].x = (short)(x + dw); q[1].y = (short)(y + gDy + dh);
    if (x < lastSpr[0]) lastSpr[0] = x; if (y + gDy < lastSpr[1]) lastSpr[1] = y + gDy;
    if (x + dw > lastSpr[2]) lastSpr[2] = x + dw; if (y + gDy + dh > lastSpr[3]) lastSpr[3] = y + gDy + dh;
    return;
  }
  float x0 = xfX((float)x), y0 = xfY((float)(y + gDy)), x1 = x0 + dw * xf.s, y1 = y0 + dh * xfSy();
  unsigned c = mulAlpha(col, xf.a);
  VFTex *q = &fbatch[2 * bn++];
  q[0] = (VFTex){(float)u, (float)v, c, x0, y0, 0.f};
  q[1] = (VFTex){(float)(u + sw), (float)(v + sh), c, x1, y1, 0.f};
  if (x0 < lastSpr[0]) lastSpr[0] = x0; if (y0 < lastSpr[1]) lastSpr[1] = y0;
  if (x1 > lastSpr[2]) lastSpr[2] = x1; if (y1 > lastSpr[3]) lastSpr[3] = y1;
}
// №646: рамка пузыря задания, выросшая под текст (fitW: вширь gw и вверх gh): 3 x 3 — углы и края как в пакете, середина растянута (1 px источника)
static void drawSlotGrow(const UiSlot *s, int x, int y, int gw, int gh, unsigned col) {
  int cx = s->w / 2, cy = s->h / 2, rw = s->w - cx - 1, rh = s->h - cy - 1;
  int X[3] = {0, cx, cx + 1}, SW[3] = {cx, 1, rw}, DW[3] = {cx, 1 + gw, rw};
  if (gw < 0) {   // №689: рамка сжата под короткий текст: края капсулы (по 24 px) как есть, середина источника сжимается
    int cap = 24; if (cap * 2 + 2 > s->w) cap = s->w / 2 - 1; if (cap < 1) cap = 1;
    int mid = s->w - 2 * cap, dm = mid + gw; if (dm < 1) dm = 1;
    X[0] = 0; X[1] = cap; X[2] = s->w - cap; SW[0] = cap; SW[1] = mid; SW[2] = cap; DW[0] = cap; DW[1] = dm; DW[2] = cap;
  }
  int Y[3] = {0, cy, cy + 1}, SH[3] = {cy, 1, rh}, DH[3] = {cy, 1 + gh, rh};
  if (PILL_NATIVE_OLD && !MSG_PILL_OLD && gh > 0 && gw >= 0 && s->h >= 16) {   // №710 (выключено №724: края капсулы — родным размером, ниже): рамка выросла вверх: края капсулы растут вместе с высотой (полукруг, а не прямоугольник с малым радиусом); середина как раньше
    int cap = 24; if (cap * 2 + 2 > s->w) cap = s->w / 2 - 1; if (cap < 1) cap = 1;
    int H = s->h + gh, capD = (cap * H + s->h / 2) / s->h, W = s->w + gw; if (capD * 2 + 2 > W) capD = W / 2 - 1;
    baddS(s->tex, s->u, s->v, cap, s->h, x, y, capD, H, col);
    baddS(s->tex, s->u + s->w - cap, s->v, cap, s->h, x + W - capD, y, capD, H, col);
    int dy = 0;
    for (int j = 0; j < 3; j++) { baddS(s->tex, s->u + cap, s->v + Y[j], s->w - 2 * cap, SH[j], x + capD, y + dy, W - 2 * capD, DH[j], col); dy += DH[j]; }
    return;
  }
  int dx = 0;
  for (int i = 0; i < 3; i++) { int dy = 0;
    for (int j = 0; j < 3; j++) { baddS(s->tex, s->u + X[i], s->v + Y[j], SW[i], SH[j], x + dx, y + dy, DW[i], DH[j], col); dy += DH[j]; }
    dx += DW[i]; }
}
static void drawSlot(const UiSlot *s, int x, int y, int w, unsigned col) {
  badd(s->tex, s->u, s->v, w, s->h, x, y, col);
}
// газета в полёте (uiScrNews): четырёхугольник поворота/масштаба от центра места, float-вершины, линейный фильтр
static float newsAng, newsScale = 1.f, newsAlpha = 1.f;
void uiScrNews(float ang, float scale, float alpha) { newsAng = ang; newsScale = scale; newsAlpha = alpha; }
static void drawSlotRot(const UiSlot *s, int x, int y, unsigned col) {
  bflush();
  bindTex(s->tex, 1);
  float cx = x + s->w * .5f, cy = y + s->h * .5f, hw = s->w * .5f, hh = s->h * .5f;
  float c1 = fabsf(cosf(newsAng)), s1 = fabsf(sinf(newsAng)), ex = hw * c1 + hh * s1, ey = hw * s1 + hh * c1, k = newsScale;
  // повёрнутая газета целиком в кадре (вершины за краем экрана в 2D GE не нужны): масштаб не больше вписанного
  if (ex > 0.f) { k = fminf(k, fminf(cx, 480.f - cx) / ex); } if (ey > 0.f) { k = fminf(k, fminf(cy, 272.f - cy) / ey); }
  float c = cosf(newsAng) * k, sn = sinf(newsAng) * k; unsigned a = mulAlpha(col, newsAlpha);
  void *b; VFTex *v = (VFTex *)vbeg(4 * sizeof(VFTex), &b);
  static const signed char q[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  for (int i = 0; i < 4; i++) {
    float dx = q[i][0] * hw, dy = q[i][1] * hh;
    v[i] = (VFTex){(float)(s->u + (q[i][0] > 0 ? s->w : 0)), (float)(s->v + (q[i][1] > 0 ? s->h : 0)), a,
                   cx + dx * c - dy * sn, cy + dx * sn + dy * c, 0.f};
    if (v[i].x < lastSpr[0]) lastSpr[0] = v[i].x; if (v[i].y < lastSpr[1]) lastSpr[1] = v[i].y;
    if (v[i].x > lastSpr[2]) lastSpr[2] = v[i].x; if (v[i].y > lastSpr[3]) lastSpr[3] = v[i].y;
  }
  vend(b, 4 * sizeof(VFTex));
  sceGuDrawArray(GU_TRIANGLE_STRIP, VT_FTEX, 4, 0, v);
  st.draws++; st.verts += 4; st.pixels += (int)(s->w * s->h * k * k);
}

// ---- правка Роба 336 (05.10: «на экране загрузки лучи были анимированы, крутились по-живому в разные стороны») ------------
// Звёзды Overlay1/Overlay0 экрана Loading.scr (tools/load_export.py stars): картинка звезды (своя страница, линейный фильтр)
// растянута на квадрат места (it->x, y, w, h — 630 px вокруг центра экрана), повёрнутый на it->ang (°/с, против часовой
// стрелки на экране) x часы экрана scrT вокруг (it->px, it->py). Как у оригинала (CGuiScreenLoading, SLES_518.97): 18 °/с,
// Overlay0 по часовой, Overlay1 против. Повёрнутый квадрат обрезается краями экрана на процессоре (Сазерленд — Ходжмен,
// до 8 вершин, uv по линейке) и уходит одним веером в 2D: вершин за краем экрана нет, заливка — ровно экран, без лишнего.
// Пакет без звёзд (LOAD_STARS_OLD=1 — прежний сведённый неподвижный фон) сюда не заходит. res[0] бит 5 — tools/ui_export.py
#define ITEM_ARML(it) ((it)->res[0] & 64u)               // №512: левая рука карточки миссии (tools/ui_export.py briefing_anim_marks): res[0] бит 6
#define ITEM_ARMR(it) ((it)->res[0] & 128u)              // правая рука: бит 7
#define ITEM_HOME(it) ((it)->res[0] & 2048u)             // №623: значок дома вне миссии (tools/ui_export.py home_icon): бит 11
#define ITEM_BACK(it) ((it)->res[0] & 256u)              // фон за карточкой (полосы LetterBox.pag): бит 8, на месте, гаснет при выезде
#define ITEM_SPIN(it) ((it)->res[0] & 32u)
typedef struct { float x, y, u, v; } SpinV;
static int spinClip(const SpinV *in, int n, SpinV *out, int axis, float lim, int keepLess) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    const SpinV *a = &in[i], *b = &in[(i + 1) % n];
    float da = (axis ? a->y : a->x) - lim, db = (axis ? b->y : b->x) - lim;
    int ia = keepLess ? da <= 0.f : da >= 0.f, ib = keepLess ? db <= 0.f : db >= 0.f;
    if (ia) out[m++] = *a;
    if (ia != ib) { float t = da / (da - db);
      out[m++] = (SpinV){a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, a->u + (b->u - a->u) * t, a->v + (b->v - a->v) * t}; }
  }
  return m;
}
// №833 (Роб 18.10: «в магазине одежды и в телефонной будке лучи хочу как в прелоадере: две „звёздочки“ из лучей, крутятся с разной скоростью в разные
// стороны; вектором, чтобы красивее; и на экране загрузки — наши векторные лучи»): крутящийся слой лучей (ITEM_SPIN) — без картинки, клиньями ровного цвета;
// прозрачность вершин спадает от центра к краю, как альфа прежних картинок. Стиль — на один вызов uiScrDraw (uiScrRays):
//   UI_RAYS_LOAD — экран загрузки: каждый слой Overlay0/1 — 8 лучей, луч 0,62 периода (замер loadbgd1), цвет картинки (255,245,169), альфа по лучу
//                  130 у центра -> 66 на 0,30 стороны -> 0 на 0,48 (замер) × альфа слоя (115 / 64), скорость слоя (±18 °/с, разные стороны) — как было;
//   UI_RAYS_SHOP — будка и гардероб: из одного слоя лучей — две звезды: 12 лучей в полпериода +9 °/с (как было) и 8 узких лучей −14 °/с в другую сторону,
//                  цвет и альфа прежних клиньев (200,216,255; 16,8 у центра -> 0 на 340 пикс), центр прежний.
// UI_RAYS_VEC_OLD=1 — картинкой (и выключатели мест: LOAD_RAYS_VEC_OLD, BOOTH_RAYS_VEC_OLD, WARD_RAYS_VEC_OLD)
#ifndef UI_RAYS_VEC_OLD
#define UI_RAYS_VEC_OLD 0
#endif
void uiScrRays(int style) { raysStyle = style; }
typedef struct { int n; float duty, degs, a0, am, rm, R; unsigned rgb; } RaysL;   // лучей, доля периода под лучом, °/с (+ против часовой), альфа у центра и на rm, радиус, 0xBBGGRR
static void raysVec(float cx, float cy, const RaysL *L, float t, float fade) {
  int mid = L->rm > 0.f && L->rm < L->R; unsigned n = (unsigned)L->n * (mid ? 9u : 3u); void *b; VFCol *v = (VFCol *)vbeg(n * sizeof(VFCol), &b), *q = v;
  #define RA(a_) ((unsigned)fminf(255.f, fmaxf(0.f, (a_) * fade + .5f)) << 24 | (L->rgb & 0xFFFFFFu))
  unsigned c0 = RA(L->a0), cm = RA(L->am), ce = RA(0.f);
  #undef RA
  float th0 = L->degs * t * (3.1415927f / 180.f), per = 6.2831853f / (float)L->n, w = per * L->duty;
  for (int i = 0; i < L->n; i++) {   // клин: центр -> (середина) -> край; угол растёт — против часовой на экране (y вниз)
    float a = th0 + per * (float)i, c1 = cosf(a), s1 = -sinf(a), c2 = cosf(a + w), s2 = -sinf(a + w);
    if (!mid) { *q++ = (VFCol){c0, cx, cy, 0.f}; *q++ = (VFCol){ce, cx + c1 * L->R, cy + s1 * L->R, 0.f}; *q++ = (VFCol){ce, cx + c2 * L->R, cy + s2 * L->R, 0.f}; continue; }
    float m1x = cx + c1 * L->rm, m1y = cy + s1 * L->rm, m2x = cx + c2 * L->rm, m2y = cy + s2 * L->rm, o1x = cx + c1 * L->R, o1y = cy + s1 * L->R, o2x = cx + c2 * L->R, o2y = cy + s2 * L->R;
    *q++ = (VFCol){c0, cx, cy, 0.f}; *q++ = (VFCol){cm, m1x, m1y, 0.f}; *q++ = (VFCol){cm, m2x, m2y, 0.f};
    *q++ = (VFCol){cm, m1x, m1y, 0.f}; *q++ = (VFCol){ce, o1x, o1y, 0.f}; *q++ = (VFCol){cm, m2x, m2y, 0.f};
    *q++ = (VFCol){cm, m2x, m2y, 0.f}; *q++ = (VFCol){ce, o1x, o1y, 0.f}; *q++ = (VFCol){ce, o2x, o2y, 0.f};
  }
  vend(b, n * sizeof(VFCol));
  sceGuDrawArray(GU_TRIANGLES, VT_FCOL, (int)n, 0, v);
  st.draws++; st.verts += (int)n; spinN++;
}
static void drawSpin(const UiItem *it, const UiSlot *s) {
  if (!UI_RAYS_VEC_OLD && raysStyle) {   // №833: векторные лучи вместо картинки
    float tt = spinT >= 0.f ? spinT : scrT, ia = (float)(it->color >> 24) / 255.f;
    bflush(); texEnable(0);
    if (raysStyle == UI_RAYS_LOAD) { RaysL L = {8, .62f, it->ang, 130.f * ia, 66.f * ia, .30f * it->w, .48f * it->w, 0xA9F5FFu}; raysVec(it->px, it->py, &L, tt, spinA); }
    else { RaysL A = {12, .5f, it->ang, 16.8f, 0.f, 0.f, 340.f, 0xFFD8C8u}, B = {8, .32f, -1.55f * it->ang, 16.8f, 0.f, 0.f, 340.f, 0xFFD8C8u};
      raysVec(it->px, it->py, &A, tt, spinA); raysVec(it->px, it->py, &B, tt, spinA); }
    return;
  }
  float th = it->ang * (spinT >= 0.f ? spinT : scrT) * (3.14159265f / 180.f), c = cosf(th), sn = sinf(th), cx = it->px, cy = it->py;
  unsigned col = it->color; if (spinA < 1.f) col = (col & 0x00ffffffu) | (unsigned)((col >> 24) * spinA + .5f) << 24;   // uiScrSpin: затухание вместе с фоном
  static const signed char q[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  SpinV p[12], r[12];
  for (int i = 0; i < 4; i++) {   // против часовой на экране (ось y вниз): x' = x·cos + y·sin, y' = -x·sin + y·cos
    float dx = it->x + q[i][0] * it->w - cx, dy = it->y + q[i][1] * it->h - cy;
    p[i] = (SpinV){cx + dx * c + dy * sn, cy - dx * sn + dy * c, (float)(s->u + q[i][0] * s->w), (float)(s->v + q[i][1] * s->h)};
  }
  int n = spinClip(p, 4, r, 0, 0.f, 0); n = spinClip(r, n, p, 0, 480.f, 1);
  n = spinClip(p, n, r, 1, 0.f, 0); n = spinClip(r, n, p, 1, 272.f, 1);
  if (n < 3) return;
  bflush();
  bindTex(s->tex, 1);
  void *b; VFTex *v = (VFTex *)vbeg((unsigned)n * sizeof(VFTex), &b);
  for (int i = 0; i < n; i++) v[i] = (VFTex){p[i].u, p[i].v, col, p[i].x, p[i].y, 0.f};
  vend(b, (unsigned)n * sizeof(VFTex));
  sceGuDrawArray(GU_TRIANGLE_FAN, VT_FTEX, n, 0, v);
  st.draws++; st.verts += n; st.pixels += 480 * 272;
  { float A = 0.f; for (int i = 0; i < n; i++) { int j = (i + 1) % n; A += p[i].x * p[j].y - p[j].x * p[i].y; } spinPx += (int)fabsf(A * .5f); spinN++; }   // №703: настоящая площадь после обрезки экраном (для SCRFILL)
}
// ---- конец блока правки 336 (вызов — в drawScreen, case T_IMG) ------------------------------------------------------------

// значок задания из данных миссии (uiSetObjIconImage): в рамке первой картинки ObjectiveIcon, по центру, пропорции как есть
static const void *oiPix; static int oiTw, oiTh, oiW, oiH, oiVisR, oiVisL, oiVisT;
#include "ui_edge_padding.h"
// правка Роба 18 (28.09): текст задания начинается сразу за ВИДИМЫМ краем текущего значка (круг Мардж уже рамки 101×95), без
// значка — от левого края места значка; раньше — за самым широким значком всех миссий, у Мардж зазор 22 px
static int objPadR = -1;   // №646: отступ текста от левого края пузыря в варианте без значка (= правое поле у любого варианта)
static int bubShift;   // №710: на сколько пузырь без аватара сдвинут влево к полю экрана (0 — не сдвинут)
#define BUB_LEFT_X 19   // №710: левое поле экрана у пузыря задания (как у рамки с аватаром)
#define BUB_AV_EXTRA 10   // №708: запас справа у пузыря с аватаром/значком: текст кончается до скруглённого края капсулы (24 px) с зазором
#define BUB_AV_RIGHT_MAX 352   // №708: правее запас не растёт (радар от 385)
static int bubDrawGrow, bubDW, bubDH;   // №646: у рисуемой рамки задания — на сколько она выросла (px вширь и вверх)
static float objTextX = -1.f; static int objBubY = -1, objBubH, objBubX, objBubW;   // правка 267: пузырь текста задания (место картинки)   // экранный x начала текста задания в этом кадре (-1 — раскладка из пакета)
#define OBJ_TEXT_GAP 4.f
void uiSetObjIconImage(const void *pix, int tw, int th, int w, int h) {
  oiPix = pix; oiTw = tw; oiTh = th; oiW = w; oiH = h; oiVisR = w;
  oiVisL = oiVisT = 0;
  if (pix && w > 0 && h > 0) {   // RGBA 8888 без свизла: столбцы справа налево, пока альфа меньше 32
    const unsigned *p = (const unsigned *)pix; oiVisR = 0;
    for (int x = w - 1; x >= 0 && !oiVisR; x--) for (int y = 0; y < h; y++) if ((p[y * tw + x] >> 24) >= 32) { oiVisR = x + 1; break; }
    if (oiVisR) {
      oiVisL = w; oiVisT = h;
      for (int y = 0; y < h; y++) for (int x = 0; x < oiVisR; x++) if ((p[y * tw + x] >> 24) >= 32) {
        if (x < oiVisL) oiVisL = x;
        if (y < oiVisT) oiVisT = y;
      }
    }
  }
}
// Роб 17.10 вечер (memory feedback_shar_icon_custom_top): у каждого значка свой верх — из экспорта (голова, вывеска, предмет — целиком;
// невидимые полупрозрачные пиксели и тонкая мелочь — не верх). ICON_TOP_OLD=1 — верх по альфе 32, как раньше
#ifndef ICON_TOP_OLD
#define ICON_TOP_OLD 0
#endif
void uiSetObjIconTop(int top) { if (!ICON_TOP_OLD && oiPix && top >= 0 && top < oiH) oiVisT = top; }
// Роб 17.10 вечер: значок в углу — таймеру и месту в гонке нужно место: левый край цифр = видимый правый край ЭТОГО рисунка + HUD_HOME_GAP
// (у значка-кольца — как прежде у кольца, у широкого — вывеска, бензопила — правее). ICON_SPACE_OLD=1 — по рамке/запечённому месту
#ifndef ICON_SPACE_OLD
#define ICON_SPACE_OLD 0
#endif
// картинка извне (RGBA 8888, w x h в углу текстуры tw x th) в рамку bx,by,bw,bh: по центру, пропорции как есть
static void drawExt(const void *pix, int tw, int th, int iw, int ih, int bx, int by, int bw, int bh, unsigned col) {
  if (!pix || iw <= 0 || ih <= 0) return;
  bflush();
  float k = fminf((float)bw / iw, (float)bh / ih), w = iw * k, h = ih * k;
  float x0 = bx + (bw - w) * .5f, y0 = by + (bh - h) * .5f;
  if (xf.on) {   // всплывающая рамка задания: значок вместе с ней
    x0 = xfX(x0); y0 = xfY(y0); w *= xf.s; h *= xfSy(); col = mulAlpha(col, xf.a);
    if (x0 < lastSpr[0]) lastSpr[0] = x0; if (y0 < lastSpr[1]) lastSpr[1] = y0;
    if (x0 + w > lastSpr[2]) lastSpr[2] = x0 + w; if (y0 + h > lastSpr[3]) lastSpr[3] = y0 + h;
  }
  texEnable(1); curTex = -1;
  sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, tw, th, tw, pix);
  if (curFilt != GU_LINEAR) { curFilt = GU_LINEAR; sceGuTexFilter(GU_LINEAR, GU_LINEAR); }
  void *b; VFTex *v = (VFTex *)vbeg(2 * sizeof(VFTex), &b);
  v[0] = (VFTex){0.f, 0.f, col, x0, y0, 0.f};
  v[1] = (VFTex){(float)iw, (float)ih, col, x0 + w, y0 + h, 0.f};
  vend(b, 2 * sizeof(VFTex));
  sceGuDrawArray(GU_SPRITES, VT_FTEX, 2, 0, v);
  st.draws++; st.verts += 2; st.pixels += (int)(w * h);
}
// аватар = высота пузыря (Роб 28.09, правка 22; tools/ui_export.py avatar_fit): рамка значка извне — px, py, ang (w), clip (h)
// элемента; значок msnart_export кладёт ровно этого размера — рисуется 1:1. Старый пакет (без рамки) — в рамке первого слота
static int avatarOld;   // служебное: 1 — прежний размер (отрицательный контроль гейта tests/ui/avatar_check.py)
static int objFrame(const UiItem *it, int *bx, int *by, int *bw, int *bh) {   // 1 — рамка аватара пакета, 0 — первый слот
  const UiSlot *s = &slots[it->first]; int nw = !avatarOld && it->ang > 0.f && it->clip > 0.f;
  *bx = nw ? (int)it->px : s->x; *by = nw ? (int)it->py : s->y; *bw = nw ? (int)it->ang : s->w; *bh = nw ? (int)it->clip : s->h;
  return nw;
}
static float iconRightX = -1.f, iconKK, iconHomeTgt;   // кадр: видимый правый край значка в углу (-1 — нет), его подъём 0..1, место цифр при значке дома
static float objVisRight(const UiItem *it) {   // видимый правый край картинки значка (px экрана), -1 — нет картинки
  if (ICON_SPACE_OLD || !oiPix || oiW <= 0 || oiH <= 0 || oiVisR <= 0 || fieldv[UI_F_OBJICON] != UI_OBJICON_EXTERNAL) return -1.f;
  int bx, by, bw, bh; objFrame(it, &bx, &by, &bw, &bh); float k = fminf((float)bw / oiW, (float)bh / oiH);
  return bx + (bw - oiW * k) * .5f + oiVisR * k;
}
static void drawObjIcon(const UiItem *it, unsigned col) {
  int bx, by, bw, bh, nw = objFrame(it, &bx, &by, &bw, &bh);
  drawExt(oiPix, oiTw, oiTh, oiW, oiH, bx, by + gDy, bw, bh, col);   // gDy: свёрнутое задание — значок вверху
  if ((traceBits & UI_TRACE_AVATAR) && oiPix && oiW > 0 && oiH > 0) {   // как ушло в GE: картинка по центру рамки с пропорциями
    float k = fminf((float)bw / oiW, (float)bh / oiH), w = oiW * k, h = oiH * k, x0 = bx + (bw - w) * .5f, y0 = by + gDy + (bh - h) * .5f;
    if (xf.on) { x0 = xfX(x0); y0 = xfY(y0); w *= xf.s; h *= xfSy(); }
    uiLogf("UIAVATAR t=%.3f x=%.2f y=%.2f w=%.2f h=%.2f img=%dx%d %s\n", uiClk, x0, y0, w, h, oiW, oiH, nw ? "rule" : "old");
  }
}
void uiAvatarOld(int on) { avatarOld = on; }
// Начало текста задания в ЭТОМ кадре (правки Роба 18 и 48): до рисования текста, по значку, который будет нарисован (раньше
// считалось при рисовании значка — он идёт в пакете после текста, и текст брал начало с прошлого кадра: у «GO!» и других
// сообщений без значка — место значка левее пузыря, текст вылезал влево). Значок извне (аватар) — сразу за его видимым краем,
// но не левее поля пузыря (отступ текста от верха пузыря); без значка или пока значок не дочитан — раскладка пакета (-1:
// место текста Hud.pag, внутри пузыря)
static int objOld;   // служебное: 1 — задание без значка начинается с места значка, левее пузыря (прежний код, гейт правки 48)
void uiObjTextOld(int on) { objOld = on; }
static float objTextStart(const UiScreen *sc) {
  const UiItem *ic = 0, *tx = 0;
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    if (it->type == T_SLOT && it->field == UI_F_OBJICON && it->count) ic = it;
    if (it->type == T_TEXT && it->field == UI_F_OBJECTIVE && !itemVarOff(it)) tx = it;
  }
  if (objOld && ic && fieldv[UI_F_OBJICON] < 0) return slots[ic->first].x;   // прежний код (отрицательный контроль): с места значка
  if (!tx) return -1.f;
  int bubble = 0; float field = 0.f; bubShift = 0;
  for (unsigned i = 0; i < sc->count; i++) {   // пузырь, в котором стоит текст (как fitW): его поле — край + отступ от верха
    const UiItem *b = &items[sc->first + i];
    if (b->type != T_IMG || b->tag != tx->tag || b->w * 2 < tx->w || itemVarOff(b)) continue;
    if (tx->x < b->x || tx->y < b->y || tx->x >= b->x + b->w || tx->y >= b->y + b->h) continue;
    float lo = b->x + (tx->y - b->y); if (!bubble || lo > field) { field = lo; objBubY = b->y; objBubH = b->h; objBubX = b->x; objBubW = b->w; } bubble = 1;
  }
  int icon = fieldv[UI_F_OBJICON];
  float padL = (float)TYPO_PADL(tx), padN = (float)TYPO_PADN(tx), r;   // правка 312: добавки к левому полю из ui_typo.json (0 — как было)
  objPadR = bubble ? (int)(uiObjStart(UI_OBJ_NOICON, 0.f, 1, field) + padN - (float)objBubX + .5f) : -1;
  if (!MSG_PILL_OLD && bubble && icon < 0 && objBubX > BUB_LEFT_X && !msgHardBr) bubShift = objBubX - BUB_LEFT_X;   // №710: пузырь без аватара — у левого поля экрана
  if ((icon < 0 || !ic) && !BUBBLE_LAW_OLD && bubble) return (float)(objBubX + objBubH / 2 + BUB_LAW_PAD);   // v1: буквы с конца левого скругления
  if (icon < 0 || !ic) { r = uiObjStart(UI_OBJ_NOICON, 0.f, bubble, field); return r >= 0.f ? r + padN : r; }   // правка 126: без значка — от поля пузыря
  if (icon != UI_OBJICON_EXTERNAL) {
    if (BUBBLE_RULES689_OLD) return uiObjStart(UI_OBJ_SLOT, 0.f, bubble, field);
    const UiSlot *sl = &slots[ic->first];   // №689: малый значок (слот пакета) — текст сразу за картинкой значка с тем же зазором и полем, что у аватара, а не на месте текста пакета (≈90 px правее)
    r = uiObjStart(UI_OBJ_AVATAR, (float)(sl->x + sl->w) + OBJ_TEXT_GAP, bubble, field); return r >= 0.f ? r + padL : r;
  }
  if (!oiPix || oiW <= 0 || oiH <= 0) return -1.f;                            // аватар ещё не дочитан: место из пакета
  int bx, by, bw, bh; objFrame(ic, &bx, &by, &bw, &bh);   // рамка аватара (правка 22) или первого слота (старый пакет)
  float k = fminf((float)bw / oiW, (float)bh / oiH), x = bx + (bw - oiW * k) * .5f + oiVisR * k + OBJ_TEXT_GAP;
  r = uiObjStart(UI_OBJ_AVATAR, x, bubble, field); return r >= 0.f ? r + padL : r;
}
static int brPixSeen; static const void *brPix; static int brTw, brTh, brW, brH; static char brTitle[96], brInfo[256], ovReason[96], ovHint[192];
void uiSetMissionOver(const char *reason, const char *hint) {
  strncpy(ovReason, reason ? reason : "", sizeof ovReason - 1); strncpy(ovHint, hint ? hint : "", sizeof ovHint - 1);
}
static int brMixedCase;   // отрицательный контроль правки 52 (uiBriefMixedCase): название как в библии
void uiBriefMixedCase(int on) { brMixedCase = on; }
const char *uiBriefTitle(void) { return brTitle; }   // служебно: название так, как его рисует карточка
void uiSetBriefing(const char *title, const char *info, const void *pix, int tw, int th, int w, int h) {
  strncpy(brTitle, title ? title : "", sizeof brTitle - 1); strncpy(brInfo, info ? info : "", sizeof brInfo - 1);
  if (!brMixedCase) for (char *c = brTitle; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;   // правка 52: название ПРОПИСНЫМИ
  if (pix && pix == brPix) brPixSeen++; else brPixSeen = 0;   // №612: сколько кадров подряд картинка карточки уже на месте
  brPix = pix; brTw = tw; brTh = th; brW = w; brH = h;
}
int uiBriefPicSettled(int frames) { return brPix && brPixSeen >= frames; }
// Роб 17.10 «добивай»: карточка гонки на ставку (gr1…): поля Entry Fee / Time to Beat / Payout страницы MissionLoad.pag диска порт не выводил.
// Теперь строки справа на чёрной плашке (tools/ui_export.py briefing_wager), картинка задания — левее на WAGER_PIC_DX. WAGER_CARD_OLD=1 — прежнее
#ifndef WAGER_CARD_OLD
#define WAGER_CARD_OLD 0
#endif
#define WAGER_PIC_DX 85   // px: картинка гонки (гонщик + флаги, справа от центра) уходит в левую часть плашки
static int brWager;
void uiSetWager(int on) { brWager = on && !WAGER_CARD_OLD; }

// сверка с оригиналом 30.09 (запись PS2 23:56 против 1:40): у бонусной миссии карточка брифинга бордовая, у обычной — синяя;
// цвет рамки-фона MissionLoad.pag (прямоугольник не во всю ширину) меняется, прозрачность та же. Замер цвета по записи: (102, 20, 22)
static int briefBonus, curScreen;   // curScreen — экран drawScreen (определён ниже, здесь — предварительно)
void uiBriefingBonus(int on) { briefBonus = on; }
// №512: состояние анимации карточки (ui_anim.h uiBriefPose): время от появления / от Continue по системным часам
// №633 (видео TEST19, брифинги 1-6: в окне выезда карточки один кадр 60–280 мс — x_city 56–283, x_ui 28–141, на экране «6 fps»): первый живой кадр мира после Continue холодный, а время выезда
// шло от нажатия и карточка перескакивала. Теперь карточка стоит на месте, пока этот кадр не кончится (uiBriefExitGo в конце кадра, страховка 0,6 с), и выезд идёт ровно от его конца.
// BRIEF_EXIT_SMOOTH_OLD=1 — время выезда от нажатия, как было
#ifndef BRIEF_EXIT_SMOOTH_OLD
#define BRIEF_EXIT_SMOOTH_OLD 0
#endif
// №994 (Роб 26.10 после TEST70: «часто глюк: показывается карточка, а потом её привозят. Карточка перед миссией немножечко лагает. Сделай так, чтобы она не появлялась сразу, а её привозила рука, и потом увозила рука, потому что опять рассинхрон»):
// причина по трассе BRIEFANIM/BRIEFDRAW (tests/card994): таймер въезда запускался при открытии брифинга (msn_view.c) и ещё раз, когда карточка готова (№612), а первые кадры рисовались уже после
// окончания первого въезда — карточка стояла на месте (руку уже уносило), потом второй запуск сбрасывал её за край и привозил заново. Теперь: пока карточка не готова (ждём загрузку), рисуется только
// подпись LOADING, карточка, руки и текст не рисуются; въезд стартует со второго запуска и его часы идут с ПЕРВОГО нарисованного кадра (длинный кадр загрузки между запуском и рисованием не съедает въезд):
// первый видимый кадр карточки — в руке за краем экрана. Выезд уже от руки (№612/№633), не тронут. CARD994_OLD=1 / файл card994old рядом с EBOOT / flag card994 0 — как в TEST70
#ifndef CARD994_OLD
#define CARD994_OLD 0
#endif
int uiCard994Old = CARD994_OLD;
void uiCard994Set(int old) { uiCard994Old = old; }
static int brWaitLoad, brPend;
static int brLeaving, brAnimOn, brHold, brHoldN; static unsigned brT0us, brHoldAt;
void uiBriefAnim(int on) {
  if (UI_BRIEF_ANIM_OLD) return;
  brT0us = sceKernelGetSystemTimeLow(); brLeaving = !on; brAnimOn = 1; brWaitLoad = 0; brPend = !on || uiCard994Old ? 0 : 1;   // №994: часы въезда — с первого нарисованного кадра
  if (traceBits & UI_TRACE_POP) uiLogf("BRIEFANIM on=%d us=%u\n", on, brT0us);   // №994 (гейт): момент старта въезда/выезда
  brHold = !on && !BRIEF_EXIT_SMOOTH_OLD; brHoldAt = brT0us; brHoldN = 2;   // два кадра: первый живой кадр и следующий (камера, клетки)
}
void uiBriefHold(void) { if (!UI_BRIEF_ANIM_OLD && !uiCard994Old) brWaitLoad = 1; }   // №994: брифинг открыт, карточка ещё не готова — только LOADING, без карточки на месте
static float brAnimSec(void) {
  if (brPend) { brPend = 0; brT0us = sceKernelGetSystemTimeLow(); }
  if (brHold) { if ((unsigned)(sceKernelGetSystemTimeLow() - brHoldAt) < 600000u) return 0.f; brHold = 0; brT0us = sceKernelGetSystemTimeLow(); }
  return (unsigned)(sceKernelGetSystemTimeLow() - brT0us) * 1e-6f; }
void uiBriefExitGo(void) { if (brHold && brLeaving && --brHoldN <= 0) { brHold = 0; uiLogf("BRIEFEXIT go after=%u ms\n", (unsigned)(sceKernelGetSystemTimeLow() - brHoldAt) / 1000u); brT0us = sceKernelGetSystemTimeLow(); } }   // конец первого живого кадра после Continue: выезд карточки стартует
int uiBriefExitBusy(void) { return !UI_BRIEF_ANIM_OLD && brAnimOn && brLeaving && brAnimSec() < UI_BR_EXIT; }
static void drawRect(const UiItem *it) {
  bflush();
  texEnable(0);
  if (briefBonus && curScreen == UI_SCREEN_MISSIONLOAD && it->w < 400) {
    UiItem r = *it; r.color = (it->color & 0xff000000u) | 0x00161466u; r.flags &= ~F_GRAD; it = &r;
    void *b; VFCol *v = (VFCol *)vbeg(4 * sizeof(VFCol), &b);
    float x0 = xfX(it->x), y0 = xfY(it->y), x1 = xfX(it->x + it->w), y1 = xfY(it->y + it->h);
    unsigned c0 = mulAlpha(it->color, xf.on ? xf.a : 1.f);
    v[0] = (VFCol){c0, x0, y0, 0}; v[1] = (VFCol){c0, x1, y0, 0}; v[2] = (VFCol){c0, x0, y1, 0}; v[3] = (VFCol){c0, x1, y1, 0};
    vend(b, 4 * sizeof(VFCol));
    sceGuDrawArray(GU_TRIANGLE_STRIP, VT_FCOL, 4, 0, v);
    st.draws++; return;
  }
  if ((it->flags & F_GRAD) || xf.on) {   // градиент или всплывающий прямоугольник (масштаб, прозрачность): float-вершины
    void *b; VFCol *v = (VFCol *)vbeg(4 * sizeof(VFCol), &b);
    float x0 = xfX(it->x), y0 = xfY(it->y), x1 = xfX(it->x + it->w), y1 = xfY(it->y + it->h);
    unsigned c0 = mulAlpha(it->color, xf.on ? xf.a : 1.f), c1 = mulAlpha((it->flags & F_GRAD) ? it->color2 : it->color, xf.on ? xf.a : 1.f);
    v[0] = (VFCol){c0, x0, y0, 0}; v[1] = (VFCol){c0, x1, y0, 0};
    v[2] = (VFCol){c1, x0, y1, 0}; v[3] = (VFCol){c1, x1, y1, 0};
    vend(b, 4 * sizeof(VFCol));
    sceGuDrawArray(GU_TRIANGLE_STRIP, VT_FCOL, 4, 0, v);
    st.verts += 4;
  } else {
    void *b; VCSpr *v = (VCSpr *)vbeg(2 * sizeof(VCSpr), &b);
    v[0] = (VCSpr){it->color, it->x, it->y, 0, 0};
    v[1] = (VCSpr){it->color, (short)(it->x + it->w), (short)(it->y + it->h), 0, 0};
    vend(b, 2 * sizeof(VCSpr));
    sceGuDrawArray(GU_SPRITES, VT_CSPR, 2, 0, v);
    st.verts += 2;
  }
  st.draws++; st.pixels += it->w * it->h;
}

static int radarTarget(float *ang, float *dist);
static int coneOff;   // отрицательный контроль гейта правки 50 (uiRadarConeOff): клин как в данных — вверх и всегда
static int rdDrawn, rdRim, rdRimOther, rdCone; static float rdConeAng, rdTgtX, rdTgtZ;   // служебно: что нарисовал радар (uiRadarDebug)
void uiRadarDebug(int *drawn, int *rim, int *rimOther, int *cone, float *coneAng, float *tx, float *tz) {
  *drawn = rdDrawn; *rim = rdRim; *rimOther = rdRimOther; *cone = rdCone; *coneAng = rdConeAng; *tx = rdTgtX; *tz = rdTgtZ;
}
void uiRadarConeOff(int on) { coneOff = on; }
// №241 (запись PS2 b3_bust / b3_moving / b3_chase, кадры радара): красная стрелка полиции (группа AIHarassCars) остаётся на обводе радара и тогда, когда машина дальше его радиуса;
// «flag radarharassold 1» / RADAR_HARASS_OLD=1 — прежнее (за краем не показывается)
#ifndef RADAR_HARASS_OLD
#define RADAR_HARASS_OLD 0
#endif
static int radarHarassOld = RADAR_HARASS_OLD;
void uiRadarHarassOld(int on) { radarHarassOld = on; }
static void drawPoly(const UiItem *it) {
  // Правка Роба 50 (эталон PS2 2:10–2:30, кадры радара 4:25…28:20): клин RadarCone0 — от центра к цели миссии; цель за краем —
  // до обода; цели нет — клина нет. В данных Hud.pag клин смотрит вверх (остриё — точка 0), поворачивает его код оригинала
  float ca = 1.f, sa = 0.f, sc = 1.f, ax = 0.f, ay = 0.f; int cone = ITEM_CONE(it) && !coneOff;
  if (cone) {
    float a, d; if (!radarTarget(&a, &d)) { rdCone = 0; return; }
    const UiPt *p0 = &pts[it->first]; ax = p0->x; ay = p0->y; float L = 0.f;
    for (unsigned i = 1; i < it->count; i++) { const UiPt *p = &pts[it->first + i]; float l = sqrtf((p->x - ax) * (p->x - ax) + (p->y - ay) * (p->y - ay)); if (l > L) L = l; }
    ca = cosf(a); sa = sinf(a); sc = L > 1.f ? d / L : 1.f;
  }
  if (ITEM_CONE(it)) { rdCone = 1; rdConeAng = cone ? atan2f(sa, ca) : 0.f; }   // угол по часовой от «вверх», радианы
  bflush();
  texEnable(0);
  unsigned n = it->count;
  void *b; VFCol *v = (VFCol *)vbeg(n * sizeof(VFCol), &b);
  for (unsigned i = 0; i < n; i++) {
    const UiPt *p = &pts[it->first + i]; float x = p->x, y = p->y;
    if (cone) { float dx = (x - ax) * sc, dy = (y - ay) * sc; x = ax + dx * ca - dy * sa; y = ay + dx * sa + dy * ca; }   // поворот по часовой (y вниз)
    v[i] = (VFCol){mulAlpha(p->color, xf.on ? xf.a : 1.f), xfX(x), xfY(y), 0};
  }
  vend(b, n * sizeof(VFCol));
  sceGuDrawArray(GU_TRIANGLE_FAN, VT_FCOL, (int)n, 0, v);
  st.draws++; st.verts += (int)n; st.pixels += it->w * it->h / 2;
}

// ---------------------------------------------------------------------------------------
// text (hinted metrics: integer bearing/advance, line start snapped to a whole pixel)
// №552 (Роб 11.10: «TH сливается»): парный кернинг пиксельной грани со своим атласом — одна пара в поле res заголовка шрифта (tools/pixel_fonts.py KERN:
// бит 24 — есть, первый | второй << 8 | дельта << 16, регистр не важен); прибавляется к шагу первого знака пары внутри строки. Нет бита — как раньше
static inline int upc_(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static inline int kernAdj(const UiFont *f, const char *s, int i, int n) {
  unsigned r = f->h.res;
  if (!(r >> 24 & 1) || i + 1 >= n) return 0;
  return upc_((unsigned char)s[i]) == (int)(r & 255) && upc_((unsigned char)s[i + 1]) == (int)(r >> 8 & 255) ? (signed char)(r >> 16 & 255) : 0;
}
static int lineWidth(const UiFont *f, const char *s, int n) {
  int w = 0;
  for (int i = 0; i < n; i++) {
    const UiGlyph *g = &f->g[(unsigned char)s[i]];
    if (!g->flags) g = &f->g['?'];
    w += g->adv + kernAdj(f, s, i, n);
  }
  return w;
}
static int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

#define MAXLINES 8
// split s into lines (greedy word wrap on spaces when wrap && wider than maxw)
static int lineWidthCb(const void *f, const char *s, int n) { return lineWidth((const UiFont *)f, s, n); }
// правка 126: перенос — viewer/ui_layout.h uiWrap (прежний жадный по пробелам + жёсткий перенос '\n' и '\\')
static int wrapLines(const UiFont *f, const char *s, int maxw, int wrap, int *st0, int *len) {
  return uiWrap(s, maxw, wrap, lineWidthCb, f, st0, len, MAXLINES);
}

// Скорость (наряд 11.10, TEST13: uiDraw 2,5 мс каждый кадр): перенос по словам и ширины строк одного и того же текста считались в каждом кадре заново (у HUD десятки строк, у каждой
// uiWrap с несколькими lineWidth). Теперь результат (строки и ширины) лежит в маленьком кэше по ключу «грань, ширина, перенос, содержимое строки, поколение раскладки»; вершины строятся те же
// (кадр побитно тот же). UI_BATCH_OLD=1 — как до правки (считать каждый раз)
#ifndef UI_BATCH_OLD
#define UI_BATCH_OLD 0
#endif
static struct { const UiFont *f; int w, wrap, nl, n; unsigned hs, gen; int st0[MAXLINES], len[MAXLINES], lw[MAXLINES]; } txtCache[48]; static unsigned txtNext;
static int batchOld = UI_BATCH_OLD;   // служебное: flag uibatchold — без кэша раскладки (проверка «кадр побитно тот же»)
void uiBatchOld(int on) { batchOld = on; }
static int textLayout(const UiFont *f, const char *s, int w, int wrap, int *st0, int *len, int *lwv) {
  if (!batchOld) {
    unsigned hs = 2166136261u; int n = 0; for (const unsigned char *q = (const unsigned char *)s; *q; q++, n++) hs = (hs ^ *q) * 16777619u;
    for (unsigned i = 0; i < sizeof txtCache / sizeof *txtCache; i++) {
      const typeof(txtCache[0]) *c = &txtCache[i];
      if (c->f == f && c->w == w && c->wrap == wrap && c->n == n && c->hs == hs && c->gen == layGen && c->nl > 0) {
        memcpy(st0, c->st0, sizeof c->st0); memcpy(len, c->len, sizeof c->len); memcpy(lwv, c->lw, sizeof c->lw); return c->nl;
      }
    }
    int nl = wrapLines(f, s, w, wrap, st0, len);
    for (int l = 0; l < nl; l++) lwv[l] = lineWidth(f, s + st0[l], len[l]);
    typeof(txtCache[0]) *c = &txtCache[txtNext]; txtNext = (txtNext + 1) % (sizeof txtCache / sizeof *txtCache);
    c->f = f; c->w = w; c->wrap = wrap; c->n = n; c->hs = hs; c->gen = layGen; c->nl = nl;
    memcpy(c->st0, st0, sizeof c->st0); memcpy(c->len, len, sizeof c->len); for (int l = 0; l < MAXLINES; l++) c->lw[l] = l < nl ? lwv[l] : 0;
    return nl;
  }
  int nl = wrapLines(f, s, w, wrap, st0, len);
  for (int l = 0; l < nl; l++) lwv[l] = lineWidth(f, s + st0[l], len[l]);
  return nl;
}
static void textGlyphs2(const UiFont *f, const char *s, int x, int y, int w, int h, int ha, int va, int wrap,
                        int dx, int dy, unsigned col, int shadow, int sdx, int sdy, unsigned scol) {
  int st0[MAXLINES], len[MAXLINES], lwv[MAXLINES];
  int nl = textLayout(f, s, w, wrap, st0, len, lwv);
  int lh = f->h.lh, bh = lh * nl, gh = f->h.tex == PIX8_TEXID ? 8 : (f->h.pad ? f->h.pad : lh);   // pix8: ячейка 8 px, строка 10;
  // грань пакета с межстрочным '^N' (правка 312): pad — высота ячейки буквы, строка lh другая (без ui_typo.json pad = 0)
  int y0 = va == 0 ? y : (va == 2 ? y + h - bh : y + floordiv(h - bh, 2));
  for (int pass = shadow ? 0 : 1; pass < 2; pass++) {   // сперва тень, потом буквы (порядок вершин прежний); раскладка одна на оба прохода
    int ddx = pass ? dx : sdx, ddy = pass ? dy : sdy; unsigned cc = pass ? col : scol;
    for (int l = 0; l < nl; l++) {
      const char *p = s + st0[l];
      int lw = lwv[l];
      float x0 = ha == 0 ? (float)x : (ha == 2 ? (float)(x + w - lw) : x + (w - lw) * 0.5f);
      int pen = (int)floorf(x0 + 0.5f);
      for (int i = 0; i < len[l]; i++) {
        const UiGlyph *g = &f->g[(unsigned char)p[i]];
        if (!g->flags) g = &f->g['?'];
        if (g->flags & 1) badd(f->h.tex, g->u, g->v, g->w, gh, pen + g->left + ddx, y0 + l * lh + ddy, cc);
        pen += g->adv + kernAdj(f, p, i, len[l]);
      }
    }
  }
}
__attribute__((unused)) static void textGlyphs(const UiFont *f, const char *s, int x, int y, int w, int h, int ha, int va, int wrap,
                       int dx, int dy, unsigned col) {
  textGlyphs2(f, s, x, y, w, h, ha, va, wrap, dx, dy, col, 0, 0, 0, 0);
}

static int curScreen;                                        // экран, который рисует drawScreen (для fitW)
static int fitW(const UiItem *t, int sidx, int *maxLines);
static void objTextItem(const UiItem *it, UiItem *t2);
static int fitWBase(const UiItem *t, int sidx, int *maxLines);
static const char *fitS; static int lastGW, lastGH, lawRight = -1;   // lawRight: v1 — правый край рамки пузыря (координаты пакета), -1 — нет   // №619: текст, под который пузырь задания растёт; последний рост (px)
// Правка Роба 126: сообщения и уведомления — пиксельной гранью pix8 (msx 8×8, 1 бит, шаг 8), остальное — шрифтами пакета.
// Кому: текст задания/сообщения в пузыре HUD (OBJECTIVE), окно сообщения (MSGBOX: будка, гардероб, «unlocked»), уведомления
// HUD «Item Dropped!» и текст карточки/открытий (CARD_TEXT, с переносом). Подписи кнопок, брифинг, провал, баннеры — пакет
static UiFont pix8F; static int pix8Ready, pix8Off;
static int objWidowOld;   // служебное (flag uiobjwidowold): текст задания с висячим словом, как до правки 215
void uiObjWidowOld(int on) { objWidowOld = on; }
void uiWrap1037Set(int on) { if (uiWrap1037 != (on != 0)) { uiWrap1037 = on != 0; layGen++; } }   // layGen — ключ кэша раскладки текста   // №1037: наименьшее число строк прежде красоты переноса (ui_layout.h)
void uiPix8(int on) { pix8Off = !on; }
static const UiFont *pix8Font(void) {
  if (!pix8Ready) {
    pix8Build(bootMsx, pix8Tex);
    for (int i = 1; i < 16; i++) pix8Clut[i] = 0xffffffu | (unsigned)(i * 17) << 24;
    pix8Clut[0] = 0;
    memset(&pix8F, 0, sizeof pix8F);
    pix8F.h.tex = (unsigned short)PIX8_TEXID; pix8F.h.lh = PIX8_LH; pix8F.h.base = 8;
    for (int c = 0; c < 256; c++) {
      int k = pix8Cell((unsigned char)c); if (k < 0) continue;
      UiGlyph *g = &pix8F.g[c]; int l = 0, w = 8; if (PIX8_BOLD) pix8Ink(bootMsx, k, &l, &w);
      g->u = (unsigned short)((k & 15) * 8 + l); g->v = (unsigned short)((k >> 4) * 8); g->w = (unsigned char)w;
      g->adv = (unsigned char)pix8Adv(bootMsx, k);   // жирный компактный: ширина по букве (уточнение Роба 01.10)
      g->flags = c == ' ' ? 2 : 1;   // пробел: глиф есть, рисовать нечего
    }
    sceKernelDcacheWritebackRange(pix8Tex, sizeof pix8Tex); sceKernelDcacheWritebackRange(pix8Clut, sizeof pix8Clut);
    pix8Ready = 1;
  }
  return &pix8F;
}
static int pix8Want(const UiItem *it, int screen) {
  if (pix8Off || it->type != T_TEXT || ITEM_NOPIX8(it)) return 0;
  if (it->field == UI_F_OBJECTIVE || it->field == UI_F_MSGBOX || ITEM_PIX8(it)) return 1;
  return screen == UI_SCREEN_HUD && (it->tag == UI_T_ITEM_DROPPED || (it->tag == UI_T_CARD_TEXT && (it->flags & F_WRAP)));
}
// правка 312: межбуквенное и межстрочное pix8 строки (res[2]) — копия грани с другим шагом знака и строки; трекинг как у граней
// пакета с «+N» (tools/ui_export.py FontCache): шаг +N, отступ +N/2 (вниз), строка по центру не съезжает. Четыре варианта в кэше
static UiFont pix8V[4]; static int pix8VKey[4], pix8VNext;
static const UiFont *pix8Variant(int trk, int lead) {
  const UiFont *b = pix8Font();
  if (!trk && !lead) return b;
  int key = 1 << 16 | (trk & 0xff) | (lead & 0xff) << 8;
  for (int i = 0; i < 4; i++) if (pix8VKey[i] == key) return &pix8V[i];
  int i = pix8VNext; pix8VNext = (pix8VNext + 1) & 3;
  UiFont *v = &pix8V[i]; *v = *b; pix8VKey[i] = key;
  v->h.lh = (unsigned short)(b->h.lh + lead < 1 ? 1 : b->h.lh + lead);
  for (int c = 0; c < 256; c++) if (v->g[c].flags) {
    int a = v->g[c].adv + trk, l = v->g[c].left + (trk >= 0 ? trk / 2 : -((-trk + 1) / 2));
    v->g[c].adv = (unsigned char)(a < 1 ? 1 : a); v->g[c].left = (signed char)l;
  }
  return v;
}
static const UiFont *itemFont(const UiItem *it, int screen) {
  return pix8Want(it, screen) ? pix8Variant(TYPO_TRK(it), TYPO_LEAD(it)) : &fonts[it->font < H->nfont ? it->font : 0];
}
// №1012 (Роб 26.10: «для целостности меню внутреннее перевести в векторный формат, раз мультик по умолчанию; не на всё, а на ховер и важные объекты, чтобы выделение выделялось, аккуратненько»):
// в режиме Cartoon выделенный пункт меню (пауза, её подстраницы OPTIONS / PICTURE / SOUND / CAMERA / GAME, главное меню) получает векторную плашку-капсулу: чернильный контур (чёрный),
// жёлтая жилка, тёмно-фиолетовая заливка (светлее панели, чтобы читалась) и блик ✦ того же шейпа, что у ключей и фар (glintshape941.h) у левого края. Плашка стоит неподвижно (пульс текста сохранён), текст и раскладка не тронуты.
// MENUVEC1012_OLD=1 / файл menuvec1012old / flag menuvec1012 0 — как в TEST71 (только жёлтый текст). Остальные пункты, рамки и значки — как были
#ifndef MENUVEC1012_OLD
#define MENUVEC1012_OLD 1   // Роб 26.10 ночь: «не то меню — паузу менять не надо, откати»; плашка выключена по умолчанию, вектор — главное меню с гостиной (№1021)
#endif
int uiMenuVec1012Old = MENUVEC1012_OLD; void uiMenuVec1012Set(int old) { uiMenuVec1012Old = old; }
extern int toneToon(void);
static void mvFan(const float *px, const float *py, int n, unsigned col, float cx, float cy) {   // веер вокруг центра: n точек контура + замыкание
  void *b; VFCol *v = (VFCol *)vbeg((unsigned)(n + 2) * sizeof(VFCol), &b);
  v[0] = (VFCol){col, xfX(cx), xfY(cy), 0};
  for (int i = 0; i <= n; i++) { int k = i % n; v[i + 1] = (VFCol){col, xfX(px[k]), xfY(py[k]), 0}; }
  vend(b, (unsigned)(n + 2) * sizeof(VFCol));
  sceGuDrawArray(GU_TRIANGLE_FAN, VT_FCOL, n + 2, 0, v);
  st.draws++; st.verts += n + 2;
}
static void mvCapsule(float x0, float y0, float x1, float y1, unsigned col) {   // капсула: полукруги на концах (по 8 отрезков)
  float r = (y1 - y0) * .5f, cy = (y0 + y1) * .5f, px[18], py[18]; int n = 0;
  for (int i = 0; i <= 8; i++) { float a = -1.5707963f + 3.1415927f * i / 8.f; px[n] = x1 - r + cosf(a) * r; py[n++] = cy + sinf(a) * r; }
  for (int i = 0; i <= 8; i++) { float a = 1.5707963f + 3.1415927f * i / 8.f; px[n] = x0 + r + cosf(a) * r; py[n++] = cy + sinf(a) * r; }
  mvFan(px, py, n, col, (x0 + x1) * .5f, cy);
}
static int menuVecWanted(const UiItem *it, int sidx, int selected, const char *s) {
  if (uiMenuVec1012Old || !s || !*s || typoShot || !toneToon()) return 0;
  if (it->flags & F_MENU) return it->menu == selected && !(it->menu < 32 && (menuOff[sidx] >> it->menu & 1));
  return sidx == UI_SCREEN_MAIN && !scrMode && it->field == UI_F_MENU_ITEM;
}
static void drawTextItem(const UiItem *it, const char *s, unsigned col);
static void menuVecPlate(const UiItem *it, const char *s) {
  const UiFont *f = itemFont(it, curScreen); if (!f || it->font >= H->nfont) return;
  int tw = lineWidth(f, s, (int)strlen(s)), lh = f->h.lh > 0 ? f->h.lh : 14;
  float tx = it->halign == 1 ? it->x + floordiv(it->w - tw, 2) : it->halign == 2 ? (float)(it->x + it->w - tw) : (float)it->x;
  float ty = (it->valign == 1 ? it->y + floordiv(it->h - lh, 2) : it->valign == 2 ? it->y + it->h - lh : it->y) + (float)gDy;   // gDy — сдвиг оверлея, как у текста
  int small = lh < 14;   // пиксельный шрифт 8×8 (страница CAMERA / GAME): ячейка почти равна строке
  float padX = small ? 11.f : 15.f, x0 = tx - padX, x1 = tx + tw + padX, y0 = small ? ty - 2.f : ty + 2.f, y1 = small ? ty + lh + 2.f : ty + lh - 1.f;   // крупный шрифт: буквы стоят в верхних ~15 px ячейки строки
  if (traceBits & UI_TRACE_MENU) uiLogf("MENUVEC1012 plate=%.0f,%.0f,%.0f,%.0f text=%.0f,%.0f,%d,%d\n", x0 - 2.f, y0 - 2.f, x1 + 2.f, y1 + 2.f, tx, ty, tw, lh);   // гейт tests/menuvec1012: рамка плашки и рамка текста
  float a = xf.on ? xf.a : 1.f; bflush(); texEnable(0);
  mvCapsule(x0 - 2.f, y0 - 2.f, x1 + 2.f, y1 + 2.f, mulAlpha(0xff000000u, a));   // чернильный контур
  mvCapsule(x0 - 1.f, y0 - 1.f, x1 + 1.f, y1 + 1.f, mulAlpha(0xff14e6ffu, a));   // жёлтая жилка (цвет выделения SHAR)
  mvCapsule(x0, y0, x1, y1, mulAlpha(0xf0701c3cu, a));                          // заливка (ABGR: R 60, G 28, B 112)
  float px[GS_MAXPTS], py[GS_MAXPTS]; int n = gsOutline(&GS_GLINT_A, 5, 1.f, px, py); float cx = x0 + (small ? 4.f : 7.f), cy = (y0 + y1) * .5f, R = (y1 - y0) * (small ? .5f : .56f);
  for (int i = 0; i < n; i++) { px[i] = cx + px[i] * R; py[i] = cy + py[i] * R; }
  mvFan(px, py, n, mulAlpha(0xff000000u, a), cx, cy);                           // блик: чёрная подложка (контур) и светлая звезда поменьше
  for (int i = 0; i < n; i++) { px[i] = cx + (px[i] - cx) * .72f; py[i] = cy + (py[i] - cy) * .72f; }
  mvFan(px, py, n, mulAlpha(0xffd8f6ffu, a), cx, cy);
}
static void drawTextItem(const UiItem *it, const char *s, unsigned col) {
  if (typoShot || it->font >= H->nfont || !s || !*s) return;
  const UiFont *f = itemFont(it, curScreen);
  int ha = it->halign, va = it->valign, wrap = it->flags & F_WRAP, w = it->w, ml = MAXLINES;
  if (!BUBBLE_LAW_OLD && wrap && it->field == UI_F_OBJECTIVE) va = 0;   // v1: строки пузыря от верха текстовой области (закон: у верха, как в панели), любое число строк
  if (wrap && it->field == UI_F_OBJECTIVE && !objWidowOld) wrap |= UI_WRAP_NOWIDOW;   // правка 215: без висячего слова, как PS2
  if (wrap && it->field == UI_F_OBJECTIVE && ((it->res[0] & 1024u) || !BUBBLE_LAW_OLD) && !BUBBLE_FIT_OLD) wrap |= UI_WRAP_BALANCE;   // №548; v1 — у всех пузырей   // №548: пузырь без значка — ровные строки
  fitS = it->field == UI_F_OBJECTIVE ? s : 0;
  if (wrap) w = fitW(it, curScreen, &ml);                     // перенос по рамке, в которой стоит текст
  fitS = 0;
  if (it->field == UI_F_OBJECTIVE && (traceBits & UI_TRACE_POP)) { int st_[MAXLINES], ln_[MAXLINES], lw_[MAXLINES]; dbgWrapW = w; dbgNl = textLayout(f, s, w, wrap, st_, ln_, lw_); }   // №942: для строки UIBUB942
  int ty = it->y, th = it->h;   // №619: рамка задания выросла вверх на lastGH — текст вместе с ней (центрованный — по новой высоте)
  if (wrap && it->field == UI_F_OBJECTIVE && lastGH) { ty -= lastGH; if (it->valign != 0) th += lastGH; }
  if (!BUBBLE_EVEN_OLD && wrap && it->field == UI_F_OBJECTIVE && it->valign == 0 && ml > 1) {   // №640: текст у верха рамки, рассчитанной на несколько строк — по высоте поровну сверху и снизу
    int st0[MAXLINES], len[MAXLINES], lwv[MAXLINES], lh = f->h.lh > 0 ? f->h.lh : 1, nl = textLayout(f, s, w, wrap, st0, len, lwv);   // та же раскладка, что у textGlyphs2 (кэш txtCache): повторного переноса нет
    if (BUBBLE_VCENTER1_OLD && nl >= 1 && nl < ml) ty += ((ml - nl) * lh) / 2;   // №671: по умолчанию текст прижат к верху текстовой области, блок растёт вниз (PS2 и панель Роба)
  }
  if (!BUBBLE_LAW_OLD && wrap && it->field == UI_F_OBJECTIVE && objBubY >= 0 && objBubH > 0) {   // v1, тесно (много строк в постоянной высоте): поле снизу не меньше верхнего
    int st0[MAXLINES], len[MAXLINES], lwv[MAXLINES], lh = f->h.lh > 0 ? f->h.lh : 1, nl = textLayout(f, s, w, wrap, st0, len, lwv);
    int topP = ty - objBubY, botP = objBubY + objBubH - (ty + nl * lh);
    if (botP - BUB_LAW_SLACK < topP) ty -= (topP - botP + BUB_LAW_SLACK + 1) / 2;
  }
#if UI_LAYOUT_CACHE_OLD
  if (it->flags & F_SHADOW) textGlyphs(f, s, it->x, ty, w, th, ha, va, wrap, it->sdx, it->sdy, it->color2);
  textGlyphs(f, s, it->x, ty, w, th, ha, va, wrap, 0, 0, col);
#else
  textGlyphs2(f, s, it->x, ty, w, th, ha, va, wrap, 0, 0, col, (it->flags & F_SHADOW) != 0, it->sdx, it->sdy, it->color2);
#endif
}
// служебно (гейт): влезает ли s в рамку задания HUD — строк после переноса, сколько помещается, самая широкая строка
void uiLastBubbleGrow(int *gw, int *gh) { *gw = lastGW; *gh = lastGH; }   // №619: служебно (гейт): на сколько px рамка выросла у последнего uiTextFit
int uiTextFit(const char *s, int *lines, int *maxLines, int *widest, int *boxW) {
  return uiTextFitField(UI_SCREEN_HUD, UI_F_OBJECTIVE, s, lines, maxLines, widest, boxW);
}
int uiTextFitField(int screen, int field, const char *s, int *lines, int *maxLines, int *widest, int *boxW) {
  if (!H || !s || (unsigned)screen >= H->nscreen) return 0;
  const UiScreen *sc = &screens[screen];
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    if (it->type != T_TEXT || it->field != field || itemVarOff(it)) continue;
    UiItem t2;
    if (field == UI_F_OBJECTIVE && !BUBBLE_FIT_OLD) { objTextItem(it, &t2); it = &t2; }   // №619: меряем тот же текст, что рисуется (после сдвига за значок), а не сырой элемент пакета
    const UiFont *f = itemFont(it, screen);
    if (!(it->flags & F_WRAP)) {   // однострочный (название миссии): ширина строки в рамке
      *lines = *maxLines = 1; *widest = lineWidth(f, s, (int)strlen(s)); *boxW = it->w;
      return *widest <= it->w;
    }
    fitS = field == UI_F_OBJECTIVE ? s : 0;
    int st0[MAXLINES], len[MAXLINES], w = fitW(it, screen, maxLines),
        nl = wrapLines(f, s, w, 1 | (field == UI_F_OBJECTIVE && !objWidowOld ? UI_WRAP_NOWIDOW : 0), st0, len), mw = 0;
    for (int l = 0; l < nl; l++) { int lw = lineWidth(f, s + st0[l], len[l]); if (lw > mw) mw = lw; }
    fitS = 0; *lines = nl; *widest = mw; *boxW = w;
    return nl <= *maxLines && mw <= w;
  }
  return 0;
}

// VIEW CREDITS (ViewCredits.scr, 29.09): порядок страниц экрана — чёрный CreditsBgd (метка ALWAYS), титры ViewCredits.pag
// (SKIP: блоки CREDITS_00..31 библии, строки по «\»), рамка TVFrame и Back поверх (STREETRACE) — титры уходят под рамку.
// Место блоков в данных — у нижнего края (Scrooby y 0, 440x15, по центру): блоки идут подряд вниз от него и едут вверх на
// scroll пикселей. Шаг строки — высота строки шрифта. 1 — ещё что-то видно, 0 — всё уехало за верх
int uiScrDrawCredits(int screen, float scroll, UiCredits *info) {
  const UiScreen *sc = scrScreen(screen);
  if (!sc) return 0;
  scrT = 0.f; pakGet(&mainPak); pakSet(&scrPak); scrMode = 1;
  unsigned keep = tagMask[screen];
  tagMask[screen] = 1u << UI_T_ALWAYS; drawScreen(screen, -1);
  beginUi(); bn = 0; btex = -1; xf.on = 0;
  float y = 0.f; int lines = 0, blocks = 0, first = -1, top = 9999, bot = -9999, lx0 = 9999, lx1 = -9999; char ln[128];
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    if (it->type != T_TEXT || it->tag != UI_T_SKIP || it->font >= H->nfont) continue;
    const UiFont *f = &fonts[it->font]; int lh = f->h.lh > it->h ? f->h.lh : it->h;
    if (!blocks) y = it->y - scroll;
    const char *s = str(it->first);
    for (const char *p = s;;) {
      const char *e = p; while (*e && *e != '\\') e++;
      int n = (int)(e - p) < (int)sizeof ln - 1 ? (int)(e - p) : (int)sizeof ln - 1; memcpy(ln, p, n); ln[n] = 0;
      int yi = (int)floorf(y + .5f);
      if (yi + lh > -lh && yi < 272 + lh && n) {
        UiItem t = *it; t.y = (short)yi; t.h = (short)lh; t.flags &= ~F_WRAP; drawTextItem(&t, ln, it->color);
        if (first < 0) first = blocks;
        if (yi < top) top = yi; if (yi + lh > bot) bot = yi + lh;
        int w = lineWidth(f, ln, n), x0 = t.x + (t.halign == 0 ? 0 : t.halign == 2 ? t.w - w : (t.w - w) / 2);
        if (x0 < lx0) lx0 = x0; if (x0 + w > lx1) lx1 = x0 + w;
      }
      y += lh; lines++;
      if (!*e) break; p = e + 1;
    }
    blocks++;
  }
  bflush();
  tagMask[screen] = 1u << UI_T_STREETRACE; drawScreen(screen, -1);
  tagMask[screen] = keep; scrMode = 0; pakSet(&mainPak);
  if (info) { info->blocks = blocks; info->lines = lines; info->first = first; info->top = top; info->bottom = bot;
              info->x0 = lx0; info->x1 = lx1; info->end = y; }
  return y > 0.f;
}
void uiPlaceCode(const char *s) { snprintf(placeText, sizeof placeText, "%s", s ? s : ""); }
int uiHasPlaceCode(void) {
  if (!H) return 0;
  for (unsigned i = 0; i < H->nitem; i++) if (items[i].field == UI_F_PLACE) return 1;
  return 0;
}
int uiText(int font, int x, int y, unsigned abgr, const char *s) {
  if (!H || (unsigned)font >= H->nfont || !s) return 0;
  const UiFont *f = &fonts[font];
  int n = (int)strlen(s), w = lineWidth(f, s, n);
  beginUi();
  bn = 0; btex = -1;
  UiItem it; memset(&it, 0, sizeof it);
  it.x = (short)x; it.y = (short)y; it.w = (short)w; it.h = (short)f->h.lh; it.font = (unsigned char)font;
  it.flags = F_SHADOW; it.sdx = 1; it.sdy = 1; it.color2 = 0xc0000000u; it.color = abgr;
  drawTextItem(&it, s, abgr);
  bflush();
  return w;
}
int uiFontFind(const char *name) {   // номер шрифта пакета: грань base; есть вариант для текстов миссий (base*k) — он
  if (!H || !name) return 0;
  int best = -1; size_t n = strlen(name);
  for (unsigned i = 0; i < H->nfont; i++) {
    const char *f = str(fonts[i].h.name);
    if (!strncmp(f, name, n) && f[n] == '*') return (int)i;       // мягко увеличенная та же грань (tools/ui_export.py)
    if (!strcmp(f, name)) best = (int)i;
  }
  return best < 0 ? 0 : best;
}
const char *uiFontName(int font) { return (H && (unsigned)font < H->nfont) ? str(fonts[font].h.name) : ""; }

// sprite-font strings (big cartoon digits/letters)
#ifndef ORDINAL_OLD
#define ORDINAL_OLD 0   // №545: 1 — прежнее положение окончания места «1ST» в гонке
#endif
#ifndef ORDINAL_DX
#define ORDINAL_DX 3    // px влево
#endif
#ifndef TIMER_ICON_OLD
#define TIMER_ICON_OLD 0   // №540: 1 — прежнее положение таймера (налезает на круглый значок цели в углу)
#endif
#ifndef TIMER_ICON_GAP
#define TIMER_ICON_GAP 3
#endif
#ifndef HUD_SHIFT_COND_OLD
#define HUD_SHIFT_COND_OLD 0   // №623: 1 — прежнее положение блока времени и места (правее кружка миссии всегда, №614)
#endif
#ifndef HUD_HOME_ICON_OLD
#define HUD_HOME_ICON_OLD 0   // №623: 1 — без значка дома вне миссии (блок времени и места тогда по условию кружка цели, см. HUD_SHIFT_COND_OLD)
#endif
#ifndef HUD_HOME_GAP
#define HUD_HOME_GAP 11   // px: от видимого правого края значка до цифр места и круга (у кружка задания по кольцу так же: кольцо 69 -> цифры 80)
#endif
#ifndef HUD_CORNER_X
#define HUD_CORNER_X 20   // px: левый край блока времени/места без кружка = правое поле радара по ободу (ободок радара до края экрана 20 px)
#endif
#ifndef HUD_CORNER_DY
#define HUD_CORNER_DY 7   // px вверх: верх блока без кружка = нижнее поле радара (16 px по ободу), по диагонали симметрично
#endif
// №993 (Роб: «почему по умолчанию всегда в левом верхнем углу дом Симпсонов, как будто туда ехать; на разных уровнях всплывает»). Причина: синтетический значок
// дома №623 (HomeIcon из ui.shr, придуман 15.10 по внешнему видео L1 и слову Роба) рисуется всякий раз, когда в углу нет кружка задания: при старте каждой
// миссии, пока значок задания ещё в пузыре внизу, между миссиями, на всех уровнях. На диске дом стоит в углу только там, где этап зовёт SetHUDIcon("simpsons")
// (L1 m1sdi m2sdi m3sdi m5sdi m6i, L4 m6sdi m7i, L7 m1i) — и он рисуется обычным значком задания, а не этим элементом. Теперь значка дома вне задания нет:
// нет кружка — блок времени и места в углу (как просил Роб в первой версии №623). ICON993_OLD=1 (flag icon993 0, файл icon993old) — как было
#ifndef ICON993_OLD
#define ICON993_OLD 0
#endif
int icon993Old = ICON993_OLD;
static int hudHomeOn; static float hudHomeA = 1.f, hudHomeExtra;
int uiHomeShown(void) { return hudHomeOn && hudHomeA > .5f; }   // №993: диагностика (MSNICON home=)   // hudHomeExtra: на сколько место и круг правее запечённого положения, пока виден значок дома
static float hudBlkV, hudBlkClk = -1.f;   // №623: 0 — блок в углу (кружка нет), 1 — правее кружка; плавно, вместе с подъёмом значка (OBJ_FLY_S)
static int hudBlockX = HUD_CORNER_X;      // левый край цифр места/круга в пакете (с запечённым сдвигом №614 или без)
static int hudBlkInit;
#ifndef HUD_BLK_RESET_OLD
#define HUD_BLK_RESET_OLD 0   // 1 — прежнее: после паузы в вызовах (смена уровня, меню) блок доезжает из старого положения ~0,2 с
#endif
static float hudBlkUpdate(int present, float k, int home) {
  float dt = uiClk - hudBlkClk; hudBlkClk = uiClk;
  if (dt < 0.f || dt > .25f) { dt = 0.f; if (!HUD_BLK_RESET_OLD) hudBlkInit = 0; }   // HUD не рисовался (смена уровня, меню, пауза): первый кадр сразу в нужном месте
  if (!hudBlkInit) { hudBlkInit = 1; hudBlkV = present ? k : 0.f; return hudBlkV; }   // первый кадр — сразу в нужном месте
  if (present && !home) hudBlkV = k;                         // значок задания едет вверх — блок едет вправо той же кривой
  else if (present) { hudBlkV += dt * (1.f / .35f); if (hudBlkV > 1.f) hudBlkV = 1.f; }   // значок дома вернулся — блок плавно правее него
  else { hudBlkV -= dt * (1.f / .35f); if (hudBlkV < 0.f) hudBlkV = 0.f; }               // кружка нет совсем — блок плавно в угол
  return hudBlkV;
}
#ifndef COIN_RIGHT_OLD
#define COIN_RIGHT_OLD 0   // №672 (Роб: «всё задом наперёд»): у PS2 значок монеты СПРАВА от цифр счёта («26 🪙» — запись PS2 5:00 HUD, 14:06 лавка),
                           // у порта был слева. Теперь справа: цифры отодвигаются на ширину значка, правый край счёта на месте. 1 — прежнее (слева)
#endif
static void drawNum(const UiItem *it, const char *s) {
  if (typoShot || it->font >= H->nsfont || !s) return;
  const UiSGlyph *G = &sfonts[it->font * 256];
  int n = (int)strlen(s), tw = 0;
  for (int i = 0; i < n; i++) {
    unsigned c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'z') c -= 32;
    if (G[c].flags) tw += G[c].adv;
  }
  int x0 = it->halign == 0 ? it->x : (it->halign == 2 ? it->x + it->w - tw : it->x + floordiv(it->w - tw, 2));
  if (it->prefix && it->prefix <= H->nslot) {               // coin icon of the count (moves with the digits)
    const UiSlot *p = &slots[it->prefix - 1]; int px_ = x0 - p->w - PREFIX_GAP, py_ = it->y + floordiv(it->h - p->h, 2);
    if (!COIN_RIGHT_OLD) { int cw = p->w + PREFIX_GAP; x0 -= it->halign == 2 ? cw : it->halign == 0 ? 0 : cw / 2; px_ = x0 + tw + PREFIX_GAP; }   // №672: монета справа от цифр
    // правка Роба 79: у оригинала здесь крутится 3D-монета (код, не Scrooby) — место иконки отдаём main.c (uiCoinIcon),
    // плоскую не рисуем, когда он рисует 3D
    coinIc[0] = (float)px_ + p->w * .5f; coinIc[1] = (float)(py_ + gDy) + p->h * .5f; coinIc[2] = (float)p->h; coinIc[3] = xf.on ? xf.a : 1.f;
    coinIcSeen = 1;
    if (!coin3d) badd(p->tex, p->u, p->v, p->w, p->h, px_, py_, 0xffffffffu);
  }
  for (int i = 0; i < n; i++) {
    unsigned c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'z') c -= 32;
    const UiSGlyph *g = &G[c];
    if (!g->flags) continue;
    if (g->tex != 0xffff) {
      int gy = it->valign == 1 ? it->y + floordiv(it->h - g->h, 2) : (it->valign == 0 ? it->y : it->y + it->h - g->h);
      badd(g->tex, g->u, g->v, g->w, g->h, x0 + g->xoff, gy, it->color);
    }
    x0 += g->adv;
  }
}

static void fmtField(const UiItem *it, char *buf, int n) {
  int v = fieldv[it->field];
  switch (it->fmt) {
    case FMT_TIME: { int s = v / 1000; if (s < 0) s = 0; snprintf(buf, (size_t)n, "%d:%02d", s / 60, s % 60); break; }
    case FMT_FRAC: snprintf(buf, (size_t)n, "%d/%d", v, it->field + 1 < UI_F_COUNT ? fieldv[it->field + 1] : 0); break;
    case FMT_INT: if (it->field == UI_F_POSITION) { uiPosText(buf, n, v, fieldv[UI_F_RACERS]); break; }   // аудит 09.10: «1/2»
      snprintf(buf, (size_t)n, "%d", v); break;
    default: snprintf(buf, (size_t)n, "%d", v); break;
  }
}
#ifndef TITLE_COINS_FIT_OLD
#define TITLE_COINS_FIT_OLD 0   // лавка (Роб: «заголовок налезает на счёт»): название машины по центру, а при 4 цифрах монет длинные названия
                                // (Surveillance Van L1, Car Built For Homer L5, Globex Super Villain Car L6) заходили под счёт. Текст по центру
                                // в одной строке со счётом монет отъезжает влево ровно настолько, чтобы до счёта осталось TITLE_COINS_GAP;
                                // левее поля TITLE_COINS_LEFT не уходит. 1 — прежнее (всегда по центру)
#endif
#define TITLE_COINS_GAP 8
#define TITLE_COINS_LEFT 8
// левый край счёта монет экрана в строке [y0, y1) (цифры и значок монеты), INT_MAX — счёта в этой строке нет
static int coinsLeftInRow(const UiScreen *sc, int y0, int y1) {
  int left = 0x7fffffff;
  for (unsigned j = 0; j < sc->count; j++) {
    const UiItem *c = &items[sc->first + j];
    if (c->type != T_NUM || c->field != UI_F_COINS || c->y >= y1 || c->y + c->h <= y0 || c->font >= H->nsfont) continue;
    char b[24]; fmtField(c, b, sizeof b);
    const UiSGlyph *G = &sfonts[c->font * 256]; int tw = 0;
    for (const char *q = b; *q; q++) { unsigned ch = (unsigned char)*q; if (G[ch].flags) tw += G[ch].adv; }
    int x0 = c->halign == 0 ? c->x : (c->halign == 2 ? c->x + c->w - tw : c->x + floordiv(c->w - tw, 2));
    if (c->prefix && c->prefix <= H->nslot) { int cw = slots[c->prefix - 1].w + PREFIX_GAP; x0 -= COIN_RIGHT_OLD ? cw : c->halign == 2 ? cw : c->halign == 0 ? 0 : cw / 2; }   // как drawNum
    if (x0 < left) left = x0;
  }
  return left;
}

// Sample the original white ring through a clockwise fan; the original blue ring stays below.
static void drawHnrFill(const UiItem *it) {
  UiRadialPoint p[UI_RADIAL_MAX]; int n = uiRadialFan(fieldv[UI_F_HNR], p);
  if (!n || !it->count) return;
  bflush(); const UiSlot *s = &slots[it->first];
  void *b; VFTex *v = (VFTex *)vbeg(n * sizeof(VFTex), &b);
  for (int i = 0; i < n; i++)
    v[i] = (VFTex){s->u + p[i].u * s->w, s->v + p[i].v * s->h, it->color,
                    s->x + p[i].u * s->w, s->y + p[i].v * s->h, 0};
  { // цвет заливки (ui_anim.h, п. 3, правка 241): жёлтая; выше 78 — красная/чёрная; полиция — красная/синяя в фазе с мигалкой
    unsigned col = uiHnrColor(hnrPursuit, hnrTarget, hnrLastT);
    if (animOff & UI_ANIM_OLDHNR)   // прежний код (отрицательный контроль): синий, с середины красный по часам системы
      col = fieldv[UI_F_HNR] > 500 ? (((sceKernelGetSystemTimeLow() >> 18) & 1) ? 0xFF2A2AE0u : 0xFF151570u) : 0xFFE08A2Au;
    for (int i = 0; i < n; i++) v[i].color = col; }
  vend(b, n * sizeof(VFTex)); bindTex(s->tex, 1);   // (цвет — до записи кэша: вершины уходят в GE из памяти)
  sceGuDrawArray(GU_TRIANGLE_FAN, VT_FTEX, n, 0, v);
  st.draws++; st.verts += n; st.pixels += s->w * s->h;
  if (traceBits & UI_TRACE_HNR)   // гейт кольца: доля круга и цвет — как ушли в GE (последняя вершина веера)
    uiLogf("UIHNR t=%.3f te=%.3f target=%d shown=%d n=%d end=%.4f,%.4f col=%08x light=%d pursuit=%d\n", uiClk, hnrLastT, (int)(hnrTarget * 1000.f + .5f), fieldv[UI_F_HNR], n,
           (v[n - 1].u - s->u) / s->w, (v[n - 1].v - s->v) / s->h, v[0].color, fieldv[UI_F_HNR_LIGHT], hnrPursuit);
}

// radar: world (dx,dz) -> screen offset; heading-up, right = (-cos h, sin h)
#define RADAR_SEG 32
#ifndef UI_RADAR_TABLE_OLD
#define UI_RADAR_TABLE_OLD 0
#endif
// круг радара: карта видна только внутри кольца шкалы (clip из ui.shr), масштаб по радиусу Map0 (ang); старый файл: clip 0
static inline float radarClip(const UiItem *it) { return it->clip > 0.f && it->clip < it->ang ? it->clip : it->ang; }

static void drawRadarMap(const UiItem *it) {
  if (H->mapTex < 0) return;
  bflush();
  float cx = it->px, cy = it->py, r = it->ang, k = r / rrange, rc = radarClip(it);
  float c = cosf(rhead), s = sinf(rhead), inv = 1.f / H->mapMpp;
  // Правка 500: 34 пары sinf/cosf на круг каждый кадр — таблица углов (те же выражения, вершины побитно те же); UI_RADAR_TABLE_OLD=1 — прежнее
  static float rdCos[RADAR_SEG + 1], rdSin[RADAR_SEG + 1]; static int rdTab;
  if (!rdTab) { for (int i = 0; i <= RADAR_SEG; i++) { float a = i * (2.f * GU_PI / RADAR_SEG); rdCos[i] = cosf(a); rdSin[i] = sinf(a); } rdTab = 1; }
  void *b; VFTex *v = (VFTex *)vbeg((RADAR_SEG + 2) * sizeof(VFTex), &b);
  for (int i = 0; i <= RADAR_SEG + 1; i++) {
    float ox = 0, oy = 0;
    if (i) {
#if UI_RADAR_TABLE_OLD
      float a = (i - 1) * (2.f * GU_PI / RADAR_SEG); ox = rc * cosf(a); oy = rc * sinf(a);
#else
      ox = rc * rdCos[i - 1]; oy = rc * rdSin[i - 1];
#endif
    }
    float rr = ox / k, ff = -oy / k;                       // right, forward (metres)
    float dx = -c * rr + s * ff, dz = s * rr + c * ff;
    v[i] = (VFTex){(rpx + dx - H->mapX0) * inv, (rpz + dz - H->mapZ0) * inv, 0xffffffffu, cx + ox, cy + oy, 0};
  }
  vend(b, (RADAR_SEG + 2) * sizeof(VFTex));
  bindTex((unsigned)H->mapTex, 1);
  sceGuDrawArray(GU_TRIANGLE_FAN, VT_FTEX, RADAR_SEG + 2, 0, v);
  st.draws++; st.verts += RADAR_SEG + 2; st.pixels += (int)(3.1416f * rc * rc);
}

// Правка Роба 50 (эталон PS2): у обода «прилипает» только цель — точка миссии, контрольная точка / финиш гонки, машина-цель;
// телефон, магазин, раздатчики гонок и бонусов, предметы видны только в пределах радара (у оригинала их за краем нет)
int uiMarkerSmall(int type, const float *center, const float *half) {
  float depth, px=markerPixels(&markerView,center,half,&depth);
  if (!H || (unsigned)type>=UI_R_COUNT || !markerIsSmall(px)) return 0;
  const UiIcon *ic=&icons[type];
  if (ic->tex==0xffff || !ic->w || !ic->h) return 0;
  float s=MARKER_ICON_PX*depth/markerView.focal, longest=fmaxf(ic->w,ic->h);
  float hw=s*ic->w/longest*.5f, hh=s*ic->h/longest*.5f;
  bflush(); curTex=curFilt=texOn=-1; bindTex(ic->tex,0); // мир изменяет GE вне кэша UI
  sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA); sceGuTexWrap(GU_CLAMP,GU_CLAMP);
  sceGuTexScale(1.f,1.f); sceGuTexOffset(0.f,0.f);
  sceGuDisable(GU_FOG); sceGuDisable(GU_LIGHTING); sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_ALPHA_TEST);
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
  ScePspFMatrix4 I; memset(&I,0,sizeof I); I.x.x=I.y.y=I.z.z=I.w.w=1.f; sceGuSetMatrix(GU_MODEL,&I);
  VFTex *v=sceGuGetMemory(4*sizeof(VFTex));
  for(int i=0;i<4;i++) {
    float sx=(i&1)?1.f:-1.f, sy=(i&2)?-1.f:1.f;
    v[i].u=ic->u+((i&1)?ic->w:0); v[i].v=ic->v+((i&2)?ic->h:0); v[i].color=ic->color;
    v[i].x=center[0]+sx*hw*markerView.right[0]+sy*hh*markerView.up[0];
    v[i].y=center[1]+sx*hw*markerView.right[1]+sy*hh*markerView.up[1];
    v[i].z=center[2]+sx*hw*markerView.right[2]+sy*hh*markerView.up[2];
  }
  sceGuDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,4,0,v);
  sceGuDepthMask(GU_FALSE); curTex=-1;
  markerDrawn++;
  return 1;
}
static int radarSticks(int type) { return (type == UI_R_HARASS && !radarHarassOld) || type == UI_R_MISSION || type == UI_R_WAYPOINT || type == UI_R_CHECKER || type == UI_R_AICAR; }
static const UiItem *radarIconsItem(void) {
  const UiScreen *sc = &screens[UI_SCREEN_HUD];
  for (unsigned i = 0; i < sc->count; i++) if (items[sc->first + i].type == T_RADARICONS) return &items[sc->first + i];
  return 0;
}
// цель клина: первая «прилипающая» иконка по порядку важности; ang — по часовой от «вверх», dist — пиксели до неё (не дальше обода)
static int radarTarget(float *ang, float *dist) {
  static const int order[] = {UI_R_MISSION, UI_R_AICAR, UI_R_WAYPOINT, UI_R_CHECKER};
  const UiItem *it = radarIconsItem(); if (!it) return 0;
  float k = it->ang / rrange, c = cosf(rhead), s = sinf(rhead), lim = radarClip(it);
  for (unsigned o = 0; o < sizeof order / sizeof *order; o++)
    for (int i = 0; i < nricon; i++) {
      if (ricon[i].type != order[o]) continue;
      float dx = ricon[i].x - rpx, dz = ricon[i].z - rpz, sx = (-c * dx + s * dz) * k, sy = -(s * dx + c * dz) * k;
      float d = sqrtf(sx * sx + sy * sy); if (d < 2.f) return 0;   // цель под игроком — клина нет
      *ang = atan2f(sx, -sy); *dist = d < lim ? d : lim; rdTgtX = ricon[i].x; rdTgtZ = ricon[i].z; return 1;
    }
  return 0;
}
static void drawRadarIcons(const UiItem *it) {
  rdDrawn = rdRim = rdRimOther = 0;
  memset(radarDrawn, 0, sizeof radarDrawn);
  float cx = it->px, cy = it->py, r = it->ang, k = r / rrange;
  float c = cosf(rhead), s = sinf(rhead);
  for (int i = 0; i < nricon; i++) {
    const UiIcon *ic = &icons[ricon[i].type];
    if (ic->tex == 0xffff) continue;
    float dx = ricon[i].x - rpx, dz = ricon[i].z - rpz;
    float sx = (-c * dx + s * dz) * k, sy = -(s * dx + c * dz) * k;
    float d = sqrtf(sx * sx + sy * sy), lim = radarClip(it) - (ic->w > ic->h ? ic->w : ic->h) * 0.35f;
    if (d > lim && !coneOff && !radarSticks(ricon[i].type)) continue;   // не цель — за краем не показываем
    if (d > lim && d > 0) { sx *= lim / d; sy *= lim / d; rdRim++; if (!radarSticks(ricon[i].type)) rdRimOther++; }
    rdDrawn++;
    radarDrawn[ricon[i].type]++;
    unsigned icol0 = ricon[i].type == UI_R_HARASS && !radarHarassOld ? 0xff0000ffu : ic->color;
    if (ricon[i].hasH) {   // правка 122 (запись PS2 5:05, 15:30): стрелка машины миссии — по её курсу относительно радара
      bflush(); bindTex(ic->tex, 0);
      float ang = rhead - ricon[i].h, co = cosf(ang), sn = sinf(ang), hw = ic->w * .5f, hh = ic->h * .5f, ox = cx + sx, oy = cy + sy;
      void *vb; VFTex *v = (VFTex *)vbeg(4 * sizeof(VFTex), &vb);
      static const signed char q[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
      for (int k2 = 0; k2 < 4; k2++) {
        float dx2 = q[k2][0] * hw, dy2 = q[k2][1] * hh;
        v[k2] = (VFTex){(float)(ic->u + (q[k2][0] > 0 ? ic->w : 0)), (float)(ic->v + (q[k2][1] > 0 ? ic->h : 0)), icol0,
                        ox + dx2 * co - dy2 * sn, oy + dx2 * sn + dy2 * co, 0.f};
      }
      vend(vb, 4 * sizeof(VFTex)); sceGuDrawArray(GU_TRIANGLE_STRIP, VT_FTEX, 4, 0, v); st.draws++;
      continue;
    }
    unsigned icol = ricon[i].type == UI_R_HARASS && !radarHarassOld ? 0xff0000ffu : ic->color;   // №241: в данных стрелка белая, красной её делает игра (запись PS2: ≈(200,0,0))
    int x = (int)floorf(cx + sx - ic->w * 0.5f + 0.5f), y = (int)floorf(cy + sy - ic->h * 0.5f + 0.5f);
    badd(ic->tex, ic->u, ic->v, ic->w, ic->h, x, y, icol);
  }
}

// ---- Оверлеи миссии HUD (Hud.pag MissionOverlays): таймер, пар-время, круги, место, счётчик предметов в данных Scrooby
// стоят в ОДНОЙ точке (135,385 45x50) — оригинал показывает их не поверх друг друга. Правило: видимые оверлеи идут
// стопкой вниз от этой точки в порядке OV_ORDER, шаг — высота строки числа (T_NUM) оверлея. uiOverlayStack(0) выключает
// правило (отрицательный контроль гейта tests/missions/emu_hud.sh). uiOverlayBoxes — итоговые рамки (служебно).
static const unsigned char OV_ORDER[] = {UI_T_TIMER, UI_T_PARTIME, UI_T_LAP, UI_T_POSITION, UI_T_COLLECTIBLES};
#define NOV ((int)sizeof OV_ORDER)
static int ovStack = 1, ovBox[NOV][4], ovOn[NOV];
void uiOverlayStack(int on) { ovStack = on; }
static int ovIndex(int tag) { for (int k = 0; k < NOV; k++) if (OV_ORDER[k] == tag) return k; return -1; }
int uiOverlayBoxes(int *tags, int (*rect)[4], int max) {
  int n = 0;
  for (int k = 0; k < NOV && n < max; k++) if (ovOn[k]) { tags[n] = OV_ORDER[k]; memcpy(rect[n], ovBox[k], sizeof ovBox[k]); n++; }
  return n;
}
static void ovBoxAdd(int k, int x, int y, int w, int h) {   // рамка оверлея = объединение его элементов
  int *b = ovBox[k];
  if (!ovOn[k]) { b[0] = x; b[1] = y; b[2] = x + w; b[3] = y + h; ovOn[k] = 1; return; }
  if (x < b[0]) b[0] = x; if (y < b[1]) b[1] = y; if (x + w > b[2]) b[2] = x + w; if (y + h > b[3]) b[3] = y + h;
}
// ---- Текст в рамке: перенос по ширине рамки-картинки того же тега, в которой он стоит (Hud.pag: Message / MissionObjective
// в рамке задания), отступ справа и снизу = отступу текста от верха рамки (толщина рамки одна со всех сторон); строк —
// сколько помещается по высоте. Не влезает — меньший шрифт пакета той же рамки; и так не влезает — uiTextFit сообщает.
static int fitWScan(const UiItem *t, int sidx, int *maxLines);
// Кэш fitW (Правка 500): ответ зависит только от экрана, тега, места и ширины текста, отступов и грани, а ищет рамку линейным обходом всех элементов экрана —
// на каждый перенесённый текст каждого кадра. Ключ по СОДЕРЖИМОМУ (в drawTextItem передают копии элемента на стеке, адрес не годится), сброс по layGen
static struct { unsigned gen; int sidx, tag, x, y, w, padt, padr; const UiFont *f; int outW, outMl; } fitCache[24]; static unsigned fitNext;
static int fitWBase(const UiItem *t, int sidx, int *maxLines) {
#if UI_LAYOUT_CACHE_OLD
  return fitWScan(t, sidx, maxLines);
#else
  const UiFont *f = itemFont(t, sidx); int padt = TYPO_PADT(t), padr = TYPO_PADR(t);
  for (unsigned i = 0; i < sizeof fitCache / sizeof *fitCache; i++) {
    const typeof(fitCache[0]) *c = &fitCache[i];
    if (c->gen == layGen && c->sidx == sidx && c->tag == t->tag && c->x == t->x && c->y == t->y && c->w == t->w && c->padt == padt && c->padr == padr && c->f == f) {
      *maxLines = c->outMl; return c->outW;
    }
  }
  int ml = 0, w = fitWScan(t, sidx, &ml);
  fitCache[fitNext] = (typeof(fitCache[0])){layGen, sidx, t->tag, t->x, t->y, t->w, padt, padr, f, w, ml}; fitNext = (fitNext + 1) % (sizeof fitCache / sizeof *fitCache);
  *maxLines = ml; return w;
#endif
}
// №619 (Роб 15.10: «текст никогда не сваливается ни вбок, ни вниз с плашки»): пузырь задания растёт под текст. Перенос по ширине без висячего слова (как было);
// если слово шире рамки — рамка шире ровно на нужное; строк больше, чем помещается — рамка выше (вверх от низа, не больше BUB_MAXEXTRA строк), для крайности рамка шире.
// Кегль и шрифт Роба не трогаем. BUBBLE_FIT_OLD=1 — прежнее (рамка фиксирована)
#define BUB_MAXEXTRA 3
#define BUB_MAXEXTRA2 5   // №646: рост рамки вверх до 5 строк сверх комфортных 2 (6-7 строк длинной подсказки в рамке с аватаром)
#define BUB_COMFORT 2   // №646: строк, при которых поля рамки задания ровные без роста
#define BUB_RIGHT_LIMIT 344
#define BUB_MINTEXT 48   // №689: самая узкая ширина текста в рамке (px), короче — рамка не сжимается дальше
// №708: у пузыря с аватаром/значком рамка шире на запас справа, чтобы текст кончался до скругления капсулы (кеш fitW этого не знает: считаем после него)
static void avPad(void) {
  if (BUBBLE_AVATAR_PAD_OLD || BUBBLE_RULES689_OLD || fieldv[UI_F_OBJICON] < 0 || objBubW <= 0) return;
  int room = BUB_AV_RIGHT_MAX - (objBubX + objBubW + lastGW), ex = room < 0 ? 0 : room > BUB_AV_EXTRA ? BUB_AV_EXTRA : room;
  lastGW += ex;
}
static int fitW(const UiItem *t, int sidx, int *maxLines) {
  int w0 = fitWBase(t, sidx, maxLines); lastGW = lastGH = 0;
  if (BUBBLE_FIT_OLD || BUBBLE_GROW_OLD || t->field != UI_F_OBJECTIVE || !fitS || !*fitS || !(t->flags & F_WRAP)) return w0;
  if (!BUBBLE_LAW_OLD && objBubW > 0 && objBubH > 0) {   // Принцип пузыря v1: высота постоянная, ширина по самой широкой строке, буквы только на прямой части
    const UiFont *f = itemFont(t, sidx); int st0[MAXLINES], len[MAXLINES], R = objBubH / 2, ml = *maxLines < 1 ? 1 : *maxLines;
    int wf = 1 | (objWidowOld ? 0 : UI_WRAP_NOWIDOW) | UI_WRAP_BALANCE;
    int maxTW = BUB_LAW_RIGHT + bubShift - R - BUB_LAW_PAD - t->x; if (maxTW < BUB_MINTEXT) maxTW = BUB_MINTEXT;   // bubShift: рамка рисуется левее пакета на него
    if (!BUB_ONE_OLD) { maxTW = BUB_ONE_RIGHT + bubShift - R - BUB_LAW_PAD - t->x; if (maxTW < BUB_MINTEXT) maxTW = BUB_MINTEXT; }   // один формат
    static struct { const UiFont *f; int maxTW, wf, nl, mw, ok, w37; char s[160]; } wcs[4]; static int wcN;   // №942: перенос считается один раз на текст и ширину; №1040: четыре записи (сообщение, задание и подсказка не вытесняют друг друга и не пересчитываются по кадрам появления)
    int nl = 0, mw = 0, hit_ = 0;
    if (!BUBBLE_WRAP942_OLD && strlen(fitS) < sizeof wcs[0].s) for (int k = 0; k < 4; k++) { if (wcs[k].ok && wcs[k].f == f && wcs[k].maxTW == maxTW && wcs[k].wf == wf && wcs[k].w37 == uiWrap1037 && !strcmp(wcs[k].s, fitS)) { nl = wcs[k].nl; mw = wcs[k].mw; hit_ = 1; break; } }
    if (!hit_) {
      nl = wrapLines(f, fitS, maxTW, wf, st0, len);
      for (int l = 0; l < nl; l++) { int lw = lineWidth(f, fitS + st0[l], len[l]); if (lw > mw) mw = lw; }
      dbgWrapCalc++; if (traceBits & UI_TRACE_POP) uiLogf("UIWRAP942 calc=%d w=%d nl=%d\n", dbgWrapCalc, maxTW, nl);
      static int adv1037_; if ((traceBits & UI_TRACE_POP) && !adv1037_++) { char b_[300]; int o_ = 0; for (int c_ = 32; c_ < 127 && o_ < 280; c_++) o_ += snprintf(b_ + o_, sizeof b_ - o_, "%d,", f->g[c_].flags ? f->g[c_].adv : -1); uiLogf("UIADV1037 %s\n", b_); }   // №1037 служебно: шаги знаков шрифта пузыря (код 32..126)
      if (strlen(fitS) < sizeof wcs[0].s) { int k = wcN; wcN = (wcN + 1) & 3; wcs[k].f = f; wcs[k].maxTW = maxTW; wcs[k].wf = wf; wcs[k].w37 = uiWrap1037; wcs[k].nl = nl; wcs[k].mw = mw; snprintf(wcs[k].s, sizeof wcs[k].s, "%s", fitS); wcs[k].ok = 1; }
    }
    int tw = mw + (nl == 1 ? BUB_LAW_SHORT : 0); if (tw > maxTW) tw = maxTW; if (tw < BUB_MINTEXT) tw = BUB_MINTEXT;
    if (!BUB_ONE_OLD) tw = maxTW;   // рамка всегда одной ширины
    lawRight = t->x + tw + BUB_LAW_PAD + R; lastGW = lawRight - (objBubX + objBubW); lastGH = 0;   // правый край рамки: после текста — поле и правое скругление
    *maxLines = nl > ml ? nl : ml;
    return mw > 0 ? mw : maxTW;
  }
  const UiFont *f = itemFont(t, sidx); int lh = f->h.lh > 0 ? f->h.lh : 1, ml0 = *maxLines, st0[MAXLINES], len[MAXLINES];
  int wf = 1 | (objWidowOld ? 0 : UI_WRAP_NOWIDOW) | ((t->res[0] & 1024u) ? UI_WRAP_BALANCE : 0);
  int cap = BUB_RIGHT_LIMIT - (t->x + w0) + bubShift; if (cap < 0) cap = 0;   // №710: сдвинутая влево рамка может расти вправо на сдвиг
  // Кэш (15.10, x_ui 3,5–4,7 мс в погонях): подбор роста рамки — до cap/2 переносов строки на вызов, а вызывается 2–3 раза за кадр, пока виден текст задания, и текст
  // тот же. Ключ — рамка, экран, ширина, строки, флаги и сам текст; ответ (рост, высота, строки) тот же побитно. UI_FITW_CACHE_OLD=1 — без кэша
#ifndef UI_FITW_CACHE_OLD
#define UI_FITW_CACHE_OLD 0
#endif
  static struct { int x, y, tw, th, font; unsigned res0; int sidx, w0, ml0, wf, cap, gw, gh, ml, w; char s[160]; int ok; } fc;   // ключ по полям, не по адресу: вызывающий передаёт копию на стеке
  if (!UI_FITW_CACHE_OLD && fc.ok && fc.x == t->x && fc.y == t->y && fc.tw == t->w && fc.th == t->h && fc.font == (int)t->font && fc.res0 == (unsigned)t->res[0] && fc.sidx == sidx && fc.w0 == w0 && fc.ml0 == ml0 && fc.wf == wf && fc.cap == cap && !strncmp(fc.s, fitS, sizeof fc.s - 1) && strlen(fitS) < sizeof fc.s) {
    lastGW = fc.gw; lastGH = fc.gh; *maxLines = fc.ml; avPad(); return fc.w;
  }
  int g = 0, nl = 0, mw = 0;
  for (; g <= cap; g += 2) {   // наименьший рост ширины, при котором самое широкое слово помещается и строк не больше ml0 + BUB_MAXEXTRA
    nl = wrapLines(f, fitS, w0 + g, wf, st0, len); mw = 0;
    for (int l = 0; l < nl; l++) { int lw = lineWidth(f, fitS + st0[l], len[l]); if (lw > mw) mw = lw; }
    if (mw <= w0 + g && nl <= ml0 + (BUBBLE_EVEN_OLD ? BUB_MAXEXTRA : BUBBLE_RULES689_OLD ? 1 : 0)) break;   // №689: сначала вширь до помещения в строки рамки, вверх — только если шире некуда   // №646: сначала вширь, пока строк больше, чем помещается + 1
  }
  if (g > cap) g = cap;
  nl = wrapLines(f, fitS, w0 + g, wf, st0, len);
  int mlG = (!BUBBLE_EVEN_OLD && ml0 > BUB_COMFORT) ? BUB_COMFORT : ml0;   // №646: поля сверху и снизу у 3–4 строк не меньше, чем у 1–2 (рамка рассчитана на 2 строки с запасом, дальше растёт на строку за строку)
  int extra = nl - mlG; if (extra < 0) extra = 0; { int mx = BUBBLE_EVEN_OLD ? BUB_MAXEXTRA : BUB_MAXEXTRA2; if (extra > mx) extra = mx; }
  lastGW = g; lastGH = extra * lh; *maxLines = ml0 + extra;
  if (!BUBBLE_RULES689_OLD && !BUBBLE_EVEN_OLD) {   // №689: рамка по ширине текста: самая широкая строка (не уже BUB_MINTEXT) = ширина текста; короткий текст — рамка сжимается, длинный — растёт (до BUB_RIGHT_LIMIT)
    int mw2 = 0; for (int l = 0; l < nl; l++) { int lw = lineWidth(f, fitS + st0[l], len[l]); if (lw > mw2) mw2 = lw; }
    if (mw2 < BUB_MINTEXT) mw2 = BUB_MINTEXT;
    if (mw2 > w0 + g) mw2 = w0 + g;
    lastGW = mw2 - w0;
  }
  if (!UI_FITW_CACHE_OLD && strlen(fitS) < sizeof fc.s) { fc.x = t->x; fc.y = t->y; fc.tw = t->w; fc.th = t->h; fc.font = (int)t->font; fc.res0 = (unsigned)t->res[0]; fc.sidx = sidx; fc.w0 = w0; fc.ml0 = ml0; fc.wf = wf; fc.cap = cap; fc.gw = lastGW; fc.gh = lastGH; fc.ml = *maxLines; fc.w = w0 + g; snprintf(fc.s, sizeof fc.s, "%s", fitS); fc.ok = 1; }
  avPad();
  return w0 + g;
}
static int fitWScan(const UiItem *t, int sidx, int *maxLines) {
  const UiScreen *sc = &screens[sidx]; int best = -1, bw = 0, bh = 0;
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *b = &items[sc->first + i];
    if (b->type != T_IMG || b->tag != t->tag || b == t || itemVarOff(b)) continue;
    if (b->w * 2 < t->w) continue;   // правка 122: значок на углу рамки (окно сообщения: ErrorIcon над началом текста) — не рамка текста
    if (t->x < b->x || t->y < b->y || t->x >= b->x + b->w || t->y >= b->y + b->h) continue;   // текст начинается в рамке
    if (best < 0 || b->w * b->h < bw * bh) { best = (int)i; bw = b->w; bh = b->h; }
  }
  if (best < 0) {   // без рамки: строк — до ближайшего элемента ниже, перекрывающего по x (или до низа экрана)
    int bottom = 272;
    for (unsigned i = 0; i < sc->count; i++) {
      const UiItem *b = &items[sc->first + i];
      if (b == t || b->y <= t->y || b->type == T_RECT || b->x >= t->x + t->w || b->x + b->w <= t->x) continue;
      if (b->y < bottom) bottom = b->y;
    }
    const UiFont *f = itemFont(t, sidx);
    *maxLines = (bottom - t->y) / (f->h.lh > 0 ? f->h.lh : 1); if (*maxLines < 1) *maxLines = 1; if (*maxLines > MAXLINES) *maxLines = MAXLINES;
    return t->w;
  }
  const UiItem *b = &items[sc->first + best]; int inset = t->y - b->y - TYPO_PADT(t);   // TYPO_PADT/PADR — правка 312 (0 — как было)
  int right = b->x + b->w - inset - TYPO_PADR(t), bottom = b->y + b->h - inset, w = right - t->x;
  const UiFont *f = itemFont(t, sidx);
  *maxLines = (bottom - t->y) / (f->h.lh > 0 ? f->h.lh : 1); if (*maxLines < 1) *maxLines = 1;
  return w < t->w ? w : t->w;
}
// №381/382: a long original hint can occupy three lines at readable PSP size.
// Move the retry question and its answers together below the actual hint block.
static int failureMenuDy(const UiScreen *sc, int *menuY, int *left, int *right) {
  const UiItem *hint = NULL;
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *t = &items[sc->first + i];
    if (t->type == T_TEXT && t->field == UI_F_MSN_HINT) { hint = t; break; }
  }
  if (!hint || !ovHint[0]) return 0;
  *left = hint->x; *right = hint->x + hint->w; *menuY = 272;
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *t = &items[sc->first + i];
    if (t->type == T_TEXT && t->field == UI_F_NONE && t->y > hint->y && t->x < *right && t->x + t->w > *left && t->y < *menuY)
      *menuY = t->y;
  }
  const UiFont *f = itemFont(hint, UI_SCREEN_MISSIONOVER);
  int st0[MAXLINES], len[MAXLINES], maxLines, w = hint->w;
  if (hint->flags & F_WRAP) w = fitW(hint, UI_SCREEN_MISSIONOVER, &maxLines);
  int nl = wrapLines(f, ovHint, w, hint->flags & F_WRAP, st0, len);
  int height = nl * f->h.lh;
  int top = hint->valign == 0 ? hint->y : hint->valign == 2 ? hint->y + hint->h - height : hint->y + floordiv(hint->h - height, 2);
  int dy = top + height + f->h.lh / 2 - *menuY;
  return dy > 0 ? dy : 0;
}
// №619: положение текста задания (значок слева, пузырь без значка) — одно место для рисования и для роста рамки
static void objTextItem(const UiItem *it, UiItem *t2) {
  *t2 = *it;
  if (objTextX >= 0.f && objTextX < it->x) {   // правка Роба 18: левый край текста — за значком (objTextStart)
    t2->x = (short)((int)objTextX - 1);
    if (it->valign == 0) { t2->w = (short)(t2->w + (int)(it->x - objTextX) + 1); t2->y = (short)(t2->y + 3 + TYPO_PADT(it)); }   // TYPO_PADT — правка 312
    else if (fieldv[UI_F_OBJICON] < 0 && !msgHardBr) { t2->w = (short)(t2->w + (int)(it->x - objTextX) + 1);   // правка 267: без значка — строка на всю ширину
      if (objBubY >= 0 && objBubH > 0) { t2->y = (short)objBubY; t2->h = (short)objBubH; } }   // и блок по центру самого пузыря
    if (!BUBBLE_EVEN_OLD && fieldv[UI_F_OBJICON] < 0 && !msgHardBr && objBubW > 0) {   // №640: правый край блока — на том же поле от края рамки, что и левый (раньше правое поле было вдвое меньше левого)
      int padL = t2->x - objBubX, r = objBubX + objBubW - padL; if (padL >= 0 && r > t2->x + 20 && r < t2->x + t2->w) t2->w = (short)(r - t2->x);
    }
    if (!BUBBLE_CENTER_OLD && BUBBLE_RULES689_OLD && fieldv[UI_F_OBJICON] < 0 && !msgHardBr) t2->halign = 1;   // №689: по умолчанию текст слева (BUBBLE_RULES689_OLD=1 — прежнее №616, по центру блока)   // №616: пузырь без значка — строки по центру блока (откат BUBBLE_CENTER_OLD=1)
  } else { t2->x = (short)(t2->x - 1); if (it->valign == 0) t2->y = (short)(t2->y + 3 + TYPO_PADT(it)); }   // с аватаром (Talk to Marge) — тот же сдвиг 156
  if (!BUBBLE_RULES689_OLD) t2->halign = 0;   // №689: текст сообщения и задания ВСЕГДА слева (с аватаром, со значком и без), не по центру
  if (!BUBBLE_LAW_OLD) { t2->y = (short)(it->y + 3 + TYPO_PADT(it)); t2->h = it->h; t2->valign = 0; }   // v1: верх строк у всех пузырей — как у TALK TO MARGE (галочка Роба 17.10): место пакета + 3 + padT панели
  if (!BUBBLE_EVEN_OLD && fieldv[UI_F_OBJICON] >= 0 && objBubW > 0 && objPadR > 0) {   // №646: с значком или аватаром — правый край блока на том же поле от края рамки, что без значка (текст не касается рамки)
    int r = objBubX + objBubW - objPadR; if (r > t2->x + 20 && r < t2->x + t2->w) t2->w = (short)(r - t2->x);
  }
}
// Разбивка кадра HUD (видео TEST17: x_ui 3,5–4,7 мс в погонях, UIPROF пуст): время по типам элементов и по тегам внутри drawScreen(HUD), мкс на кадр.
// Работает только при uiProfOn (ставит main в тест-апке: smkOn); строка UIT раз в секунду — uiProfTake. UI_PROF2_OLD=1 — без счёта.
#ifndef UI_PROF2_OLD
#define UI_PROF2_OLD 0
#endif
int uiProfOn;
static unsigned pfTyUs[10], pfTyN[10], pfTagUs[32], pfSetupUs, pfItemsUs, pfFlushUs, pfFrames, pfItemsN;
int uiProfTake(char *out, int n) {   // строка UIT (среднее мс на кадр HUD) и обнуление; 0 — кадров не было
  if (!pfFrames) return 0;
  static const char *TY[10] = {"rect", "poly", "img", "text", "num", "radarmap", "radaricons", "slot", "bar", "sector"};
  double k = 1.0 / pfFrames / 1000.0; int l = snprintf(out, (size_t)n, "UIT n=%u items=%.1f setup=%.2f loop=%.2f flush=%.2f ty:", pfFrames, (double)pfItemsN / pfFrames, pfSetupUs * k, pfItemsUs * k, pfFlushUs * k);
  for (int i = 0; i < 10 && l < n - 40; i++) if (pfTyUs[i]) l += snprintf(out + l, (size_t)(n - l), " %s=%.2f/%.1f", TY[i], pfTyUs[i] * k, (double)pfTyN[i] / pfFrames);
  l += snprintf(out + l, (size_t)(n - l), " tags:");
  for (int r = 0; r < 8; r++) { int b = -1; for (int t = 0; t < 32; t++) if (pfTagUs[t] && (b < 0 || pfTagUs[t] > pfTagUs[b])) b = t;
    if (b < 0 || l >= n - 24) break; l += snprintf(out + l, (size_t)(n - l), " %d=%.2f", b, pfTagUs[b] * k); pfTagUs[b] = 0; }
  memset(pfTyUs, 0, sizeof pfTyUs); memset(pfTyN, 0, sizeof pfTyN); memset(pfTagUs, 0, sizeof pfTagUs); pfSetupUs = pfItemsUs = pfFlushUs = pfFrames = pfItemsN = 0;
  if (l < n - 1) { out[l++] = '\n'; out[l] = 0; }
  return l;
}
// №1035 (Роб, сверка с PS2: дом Симпсонов и Kwik-E-Mart, кадры build/twins/o4/v17_house2 и v11_kwik_fwd): в помещениях радар как в оригинале — рамка-кольцо (и шапка счётчика погони, она отдельным тегом) остаётся,
// карта, значки, клин цели, стрелка героя и фон круга исчезают, круг прозрачный, сквозь него видна сцена. Признак «в помещении» — тот же inRoom(), что переключает комнатную камеру; выход на улицу возвращает карту в тот же кадр.
// RADAR1035_OLD=1 / файл radar1035old / «flag radar1035 0» — как в TEST72 (в комнате карта у двери в городе)
#ifndef RADAR1035_OLD
#define RADAR1035_OLD 0
#endif
static int radarIndoor1035, radar1035Old = RADAR1035_OLD;
void uiRadarIndoor(int on) { radarIndoor1035 = on; }
void uiRadar1035Set(int old) { radar1035Old = old; }
static int radarRimIdx(const UiScreen *sc) {   // рамка-кольцо — последний элемент радара экрана (Hud.pag: карта, фон, клин, стрелка, значки, затем рамка)
  static const UiScreen *c; static int r = -1; if (c == sc) return r;
  c = sc; r = -1; for (unsigned i = 0; i < sc->count; i++) if (items[sc->first + i].tag == UI_T_RADAR) r = (int)i; return r;
}
static void drawScreen(int sidx, int selected) {
  if (!H || (unsigned)sidx >= H->nscreen || typoShot >= 2) return;
  const int pfOn = !UI_PROF2_OLD && uiProfOn && sidx == UI_SCREEN_HUD && !scrMode; unsigned pf0 = pfOn ? sceKernelGetSystemTimeLow() : 0, pfLoop0 = 0;
  const UiScreen *sc = &screens[sidx];
  unsigned mask = tagMask[sidx];
  unsigned hid = !scrMode && (unsigned)sidx < UI_SCREEN_MAX ? menuHid[sidx] : 0;   // правка 137 (не пакет экранов загрузки)
  if (sidx == UI_SCREEN_MAIN && mainFlight && !scrMode) mask &= ~(1u << UI_T_MAIN_ROW | 1u << UI_T_LEVEL_NAME);
  curScreen = sidx;
  memset(&st, 0, sizeof st);
  beginUi();
  bn = 0; btex = -1;
  char buf[32];
  xf.on = 0;
  // ---- анимации HUD: показ/снятие тегов с прошлого кадра, гаснущие теги, центры всплывающих групп (ui_anim.h)
  int hud = sidx == UI_SCREEN_HUD && !scrMode;
  int failureY = 272, failureLeft = 0, failureRight = 0;
  int failureDy = !scrMode && sidx == UI_SCREEN_MISSIONOVER ? failureMenuDy(sc, &failureY, &failureLeft, &failureRight) : 0;
  int actionDy = 0;
  if (hud && !UI_OBJTOP_OLD) {
    float padding = 2.f; int actionTop = 10000;
    for (unsigned j = 0; j < sc->count; j++) {
      const UiItem *a = &items[sc->first + j];
      if (a->type == T_SLOT && a->field == UI_F_OBJICON && a->count) {
        int bx_, by_, bw_, bh_; objFrame(a, &bx_, &by_, &bw_, &bh_);
        float s_ = oiW > 0 && oiH > 0 ? fminf((float)bw_ / oiW, (float)bh_ / oiH) : 0.f;
        padding = uiEdgeMargin(s_ > 0.f ? bx_ + (bw_ - oiW * s_) * .5f + oiVisL * s_ : bx_);
      }
      if (a->tag == UI_T_ACTION && a->type == T_IMG) {
        int top_ = a->y + (a->res[2] ? (int)a->res[2] - 1 : 0);
        if (top_ < actionTop) actionTop = top_;
      }
    }
#ifndef UI_ACTION_TOP_EXTRA   // №403 (Роб 08.10, PSP: «треугольник поднял слишком близко к верху»): блок △ на 4 px ниже поля №387
#define UI_ACTION_TOP_EXTRA 4   // (было ~3–4 px от края экрана, стало ~8); прежнее: -DUI_ACTION_TOP_EXTRA=0
#endif
    if (actionTop < 10000) actionDy = (int)floorf(padding + UI_ACTION_TOP_EXTRA - actionTop + .5f);
  }
  unsigned leaveDraw = 0;
  static float pillCy, pv[32][4], tbox[32][4];                       // рамка группы тега (центр всплывания) / нарисованная рамка
  if (hud) {
    uiClockTick();
    for (int t = 0; t < 32; t++) if ((hudForce >> t & 1) && uiClk >= forceUntil[t]) hudForce &= ~(1u << t);
    mask |= hudForce;
    unsigned rise = mask & ~hudPrev, fall = hudPrev & ~mask;
    if (msgNew && (mask >> UI_T_OBJECTIVE & 1)) rise |= 1u << UI_T_OBJECTIVE;   // новый текст задания в той же рамке
    if (cdNew && (mask >> UI_T_COUNTDOWN & 1)) rise |= 1u << UI_T_COUNTDOWN;    // 3 -> 2 -> 1
    msgNew = cdNew = 0;
    for (int t = 0; t < 32; t++) {
      unsigned b = 1u << t;
      if (rise & b) { tagOnT[t] = uiClk; if (t == UI_T_OBJECTIVE) { bubAge = 0.f; objFlyT = 0.f; } hudLeave &= ~b; if (popHook) popHook(t, 1); }
      if (fall & b) { tagOffT[t] = uiClk; if (popKind(t) >= 0 && !(animOff & UI_ANIM_NOPOP)) hudLeave |= b; if (popHook) popHook(t, 0); }
    }
    hudPrev = mask;
    for (int t = 0; t < 32; t++) if (hudLeave >> t & 1) {
      float s_, a_;
      if ((mask >> t & 1) || !uiPopState(popKind(t), 0, uiClk - tagOffT[t], &s_, &a_)) hudLeave &= ~(1u << t);
    }
    leaveDraw = hudLeave;
    for (int t = 0; t < 32; t++) { pv[t][0] = pv[t][1] = tbox[t][0] = tbox[t][1] = 1e9f; pv[t][2] = pv[t][3] = tbox[t][2] = tbox[t][3] = -1e9f; }
    pillCy = -1e9f;
    for (unsigned i = 0; i < sc->count; i++) {
      const UiItem *it = &items[sc->first + i]; float *p = pv[it->tag & 31];
      if (it->tag == UI_T_OBJECTIVE && it->type == T_IMG && it->h > 0) pillCy = it->y + it->h * .5f;
      if (popKind(it->tag) < 0 || it->w <= 0 || it->h <= 0) continue;
      if (!BUBBLE_UNIFORM_OLD && it->tag == UI_T_OBJECTIVE && (itemVarOff(it) || (it->type == T_SLOT && it->field == UI_F_OBJICON && fieldv[UI_F_OBJICON] < 0))) continue;   // №638: центр всплывания — только видимая рамка (не другой вариант пузыря и не слот значка, которого нет)
      if (it->x < p[0]) p[0] = it->x; if (it->y < p[1]) p[1] = it->y;
      if (it->x + it->w > p[2]) p[2] = it->x + it->w; if (it->y + it->h > p[3]) p[3] = it->y + it->h;
    }
  }
  float objOc = uiClk - tagOnT[UI_T_OBJECTIVE] - OBJ_SHOW_S;   // №942: сколько секунд значок цели уже летит в угол (<= 0 — ещё нет)
  if (hud && !BUBBLE_LAG942_OLD) { if (objOc <= 0.f) objFlyT = 0.f; else { objFlyT += uiClkDt < UI_BUB_DT_CAP ? uiClkDt : UI_BUB_DT_CAP; if (objFlyT <= 0.f) objFlyT = 1e-4f; if (objFlyT > objOc) objFlyT = objOc; } objOc = objOc > 0.f ? objFlyT : objOc; }
  objTextX = objTextStart(sc);
  int bubGW = 0, bubGH = 0; lawRight = -1;   // №619: рамка задания растёт под текст (fitW): вширь и вверх от низа
  if (hud && !BUBBLE_FIT_OLD && !BUBBLE_GROW_OLD && (((mask | leaveDraw) >> UI_T_OBJECTIVE) & 1u)) {
    const char *bs = msgText[0] ? msgText : msgLast;
    if (*bs) for (unsigned j = 0; j < sc->count; j++) {
      const UiItem *a = &items[sc->first + j];
      if (a->type != T_TEXT || a->field != UI_F_OBJECTIVE || itemVarOff(a) || !(a->flags & F_WRAP)) continue;
      UiItem t2; objTextItem(a, &t2); int ml_ = 0; fitS = bs; fitW(&t2, sidx, &ml_); fitS = 0; bubGW = lastGW; bubGH = lastGH; break;
    }
  }
  if (!BUBBLE_UNIFORM_OLD && (bubGW || bubGH) && pv[UI_T_OBJECTIVE][2] >= pv[UI_T_OBJECTIVE][0]) { pv[UI_T_OBJECTIVE][1] -= bubGH; pv[UI_T_OBJECTIVE][2] += bubGW; }
  if (hud && bubShift && pv[UI_T_OBJECTIVE][2] >= pv[UI_T_OBJECTIVE][0]) { pv[UI_T_OBJECTIVE][0] -= bubShift; pv[UI_T_OBJECTIVE][2] -= bubShift; }   // №710: центр всплывания — сдвинутая рамка   // №638: центр всплывания — конечная рамка (с ростом под текст), а не рамка пакета
  int ovDy[NOV]; memset(ovDy, 0, sizeof ovDy);
  if (sidx == UI_SCREEN_HUD) {   // стопка видимых оверлеев: шаг — высота строки числа предыдущего
    int y = 0; memset(ovOn, 0, sizeof ovOn);
    for (int k = 0; k < NOV; k++) {
      if (!(mask & (1u << OV_ORDER[k]))) continue;
      int rowh = 0;
      for (unsigned i = 0; i < sc->count; i++) { const UiItem *it = &items[sc->first + i]; if (it->tag == OV_ORDER[k] && it->type == T_NUM && it->h > rowh) rowh = it->h; }
      ovDy[k] = ovStack ? y : 0; y += rowh;
    }
  }
  float brDx = 0.f, brArmL = 0.f, brArmR = 0.f, brCardA = 1.f, brBackA = 1.f; int brShowL = 1, brShowR = 1, brAnim = 0;
  if (sidx == UI_SCREEN_MISSIONLOAD && !scrMode && brAnimOn && !UI_BRIEF_ANIM_OLD) {   // №512: пути карточки и рук — по краям элементов пакета
    float cL = 1e9f, cR = -1e9f, aLR = -1e9f, aRL = 1e9f;
    for (unsigned j = 0; j < sc->count; j++) {
      const UiItem *a = &items[sc->first + j];
      if (ITEM_BACK(a)) continue;
      if (ITEM_ARML(a)) { if (a->x + a->w > aLR) aLR = a->x + a->w; }
      else if (ITEM_ARMR(a)) { if (a->x < aRL) aRL = a->x; }
      else { if (a->x < cL) cL = a->x; if (a->x + a->w > cR) cR = a->x + a->w; }
    }
    if (cR > cL) {
      float t = brWaitLoad && !brLeaving ? 0.f : brAnimSec();
      if (brLeaving && t >= UI_BR_EXIT) brAnimOn = 0;
      else { uiBriefPose(brLeaving, t, cL, cR, aLR > -1e8f ? aLR : cL, aRL < 1e8f ? aRL : cR, 480.f, &brDx, &brArmL, &brArmR, &brShowL, &brShowR, &brCardA, &brBackA); brAnim = 1; }
    }
  }
  if (sidx == UI_SCREEN_MISSIONLOAD && !scrMode && (traceBits & UI_TRACE_POP)) uiLogf("BRIEFDRAW us=%u wait=%d anim=%d on=%d leaving=%d dx=%.1f armL=%.1f armR=%.1f showL=%d showR=%d cardA=%.2f backA=%.2f\n", sceKernelGetSystemTimeLow(), brWaitLoad, brAnim, brAnimOn, brLeaving, brDx, brArmL, brArmR, brShowL, brShowR, brCardA, brBackA);   // №994 (гейт): что нарисовано в каждом кадре карточки
  float hudBlk = 1.f;
  if (hud && !HUD_SHIFT_COND_OLD) {   // №623 (Роб 15.10): блок времени и места правее кружка только когда кружок цели виден в углу вверху слева; нет кружка — в самом углу
    int present = 0; float kk = 0.f;
    if (objCollapse && (mask >> UI_T_OBJECTIVE & 1u) && fieldv[UI_F_OBJICON] >= 0) {
      float oc = objOc;
      if (oc > 0.f) { present = 1; kk = oc / OBJ_FLY_S; if (kk > 1.f) kk = 1.f; kk = kk * kk * (3.f - 2.f * kk); }
    }
    int bx = 9999, hasHome = 0; float homeR = 0.f;
    for (unsigned j = 0; j < sc->count; j++) { const UiItem *c = &items[sc->first + j]; if (c->type == T_NUM && (c->tag == UI_T_LAP || c->tag == UI_T_POSITION) && c->x < bx) bx = c->x; if (c->type == T_IMG && ITEM_HOME(c)) { hasHome = 1; homeR = c->px; } }
    // №623: значок дома — пока HUD виден (радар) и кружка задания в углу нет; кружок цели поднимается — значок дома гаснет той же кривой
    hudHomeOn = icon993Old && !HUD_HOME_ICON_OLD && hasHome && (mask >> UI_T_RADAR & 1u);   // №993: вне задания дома нет
    hudHomeA = present ? 1.f - kk : 1.f;
    hudBlk = hudBlkUpdate(present || hudHomeOn, hudHomeOn ? 1.f : kk, hudHomeOn);
    hudBlockX = bx < 9999 ? bx : HUD_CORNER_X;
    iconRightX = -1.f; iconKK = kk; iconHomeTgt = hudBlockX + fmaxf(0.f, homeR + (float)HUD_HOME_GAP - (float)hudBlockX);
    if (present) for (unsigned j = 0; j < sc->count; j++) { const UiItem *c = &items[sc->first + j]; if (c->type == T_SLOT && c->field == UI_F_OBJICON) { iconRightX = objVisRight(c); break; } }
    hudHomeExtra = hudHomeOn ? fmaxf(0.f, homeR + (float)HUD_HOME_GAP - (float)hudBlockX) * hudHomeA : 0.f;   // дом выступает правее кружков задания: цифры не налезают на него, при подъёме кружка задания сдвиг сходит на нет
  }
  if (pfOn) { pfLoop0 = sceKernelGetSystemTimeLow(); pfSetupUs += pfLoop0 - pf0; pfFrames++; }
  { float dm_ = (sidx == UI_SCREEN_PAUSE || sidx == UI_SCREEN_PAUSEMSN) ? pauseDimA() : 0.f;   // №631: затемнение мира
    if (dm_ > 0.f) { UiItem r; memset(&r, 0, sizeof r); r.type = T_RECT; r.x = 0; r.y = 0; r.w = 480; r.h = 272; r.color = (unsigned)(dm_ * 255.f + .5f) << 24; xf.on = 0; drawRect(&r); } }
  float pS = 1.f, pA = 1.f; int pAnim = !scrMode && (sidx == UI_SCREEN_PAUSE || sidx == UI_SCREEN_PAUSEMSN) && pauseAnimPose(&pS, &pA);   // №631
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    if (itemVarOff(it)) continue;   // №528: другой вариант пузыря задания
    unsigned tb = 1u << (it->tag & 31);
    if (!(mask & tb) && !(leaveDraw & tb)) continue;
    xf.on = 0; xf.sy = 0.f; bubSlideNow = 0;
    if (pAnim) { xf.on = 1; xf.s = pS; xf.a = pA; xf.cx = 240.f; xf.cy = 120.f; }   // №631 (центр: по кадрам PS2 плита растёт из середины по x и около 0,44 высоты по y): рамка и пункты паузы растут из центра
    // №631: у PS2 отдельной плашки на весь экран нет, мир темнеет на 7 % (pauseDimA выше); плашка пака (75 % чёрного на весь экран) при включённой анимации не рисуется
    if (!PAUSE_ANIM_OLD && !scrMode && (sidx == UI_SCREEN_PAUSE || sidx == UI_SCREEN_PAUSEMSN) && it->type == T_RECT && it->x <= 0 && it->y <= 0 && it->w >= 480 && it->h >= 272) continue;
    UiItem brIt;
    if (brAnim) {   // №512: вход слева / выход вправо; руки — только на время переезда; полосы фона — на месте, гаснут при выезде
      float dx = (brWaitLoad && !brLeaving && it->tag == UI_T_LOADING) ? 0.f : ITEM_ARML(it) ? brArmL : ITEM_ARMR(it) ? brArmR : ITEM_BACK(it) ? 0.f : brDx, al = ITEM_BACK(it) ? brBackA : (ITEM_ARML(it) || ITEM_ARMR(it)) ? brCardA : brCardA;
      if ((ITEM_ARML(it) && !brShowL) || (ITEM_ARMR(it) && !brShowR) || al <= 0.f) continue;
      if (brWaitLoad && !brLeaving && !ITEM_BACK(it) && it->tag != UI_T_LOADING) continue;   // №994: ждём загрузку — карточки, рук и текста нет
      brIt = *it; brIt.x = (short)floorf((float)it->x + dx + .5f); it = &brIt;
      if (!BRIEF_TEXT_LATE_OLD && !brLeaving && !brWaitLoad && it->type == T_TEXT && !ITEM_BACK(it)) {   // №217 голосом Роба (16.10): текст карточки после сборки рамки: пока карточка едет — нет, затем проявляется за BRIEF_TEXT_FADE
        float tt_ = brAnimSec() - UI_BR_IN; if (tt_ < 0.f) continue; if (tt_ < BRIEF_TEXT_FADE) al *= tt_ / BRIEF_TEXT_FADE; }
      if (al < 1.f) { xf.on = 1; xf.s = 1.f; xf.a = al; xf.cx = xf.cy = 0.f; }
    }
    UiItem bubIt; bubDrawGrow = 0;
    if ((bubGW || bubGH) && it->tag == UI_T_OBJECTIVE && it->type == T_IMG) {   // №619: рамка пузыря задания по росту текста
      bubDrawGrow = !BUBBLE_EVEN_OLD; bubDW = (!BUBBLE_LAW_OLD && lawRight > 0) ? lawRight - (it->x + it->w) : bubGW; bubDH = bubGH; bubIt = *it; bubIt.y = (short)(bubIt.y - bubGH); bubIt.h = (short)(bubIt.h + bubGH); bubIt.w = (short)(bubIt.w + bubDW); it = &bubIt;
    }
    UiItem hbIt;
    int hudBlkMove = hud && !HUD_SHIFT_COND_OLD && hudBlk < 1.f;
    int hudGrp = it->tag == UI_T_LAP || it->tag == UI_T_POSITION, hudTm = it->tag == UI_T_TIMER || it->tag == UI_T_PARTIME;
    int hudMove = hudBlkMove && (hudGrp || hudTm);
    if (hud && !HUD_SHIFT_COND_OLD && hudGrp && iconRightX >= 0.f) {   // Роб 17.10: место и круг — правее видимого рисунка значка (+ HUD_HOME_GAP), в угол без значка
      float xi = fmaxf((float)HUD_CORNER_X, iconRightX + (float)HUD_HOME_GAP), tgt = hudHomeOn ? iconHomeTgt + (xi - iconHomeTgt) * iconKK : HUD_CORNER_X + hudBlk * (xi - HUD_CORNER_X);
      hbIt = *it; hbIt.x = (short)floorf((float)it->x - (float)hudBlockX + tgt + .5f); it = &hbIt;
    } else
    if (hud && !HUD_SHIFT_COND_OLD && hudGrp && (hudBlkMove || hudHomeExtra > 0.f)) {   // №623: место и круг — от запечённого в пакете положения (+58, №614): в угол (кружка нет) или правее значка дома
      hbIt = *it; hbIt.x = (short)floorf((float)it->x - (1.f - hudBlk) * (float)(hudBlockX - HUD_CORNER_X) + hudHomeExtra + .5f); it = &hbIt;
    }
    if (hud && it->type == T_IMG && ITEM_HOME(it)) {   // №623: значок дома — полупрозрачность по подъёму кружка задания
      if (!hudHomeOn || hudHomeA <= 0.f) continue;
      if (hudHomeA < 1.f) { xf.on = 1; xf.s = 1.f; xf.a = hudHomeA; xf.cx = xf.cy = 0.f; }
    }
    if (!uiTextLate1004Old && !uiBubble1040 && it->type == T_TEXT && ((sidx == UI_SCREEN_MESSAGE && mboxK < .999f) || (sidx == UI_SCREEN_TUTORIAL && tutK < .999f))) continue;   // №1004: текст окна сообщения и подсказки не растёт с рамкой — появляется, когда рамка раскрылась
    if (sidx == UI_SCREEN_MESSAGE && (mboxK < .999f || mboxA < 1.f)) { xf.on = 1; xf.s = mboxK; xf.a = mboxA; xf.cx = mboxCx; xf.cy = mboxCy; }   // №639
    if (sidx == UI_SCREEN_TUTORIAL && tutK < .999f) { xf.on = 1; xf.s = tutK; xf.a = 1.f; xf.cx = 240.f; xf.cy = 136.f; }   // №516: подсказка Барта растёт из центра экрана
    if (hud && popKind(it->tag) >= 0 && !(animOff & UI_ANIM_NOPOP)) {   // всплывание / угасание группы от её центра
      int on = (mask & tb) != 0; float s_, a_;
      uiPopState(popKind(it->tag), on, popAgeOf(it->tag, on), &s_, &a_);
      // №610: текст пузыря (строка задания, окно сообщения) не рисуем, пока рамка растёт: он раскладывался узко и перестраивался. BUBBLE_TEXT_LATE_OLD=1 — прежнее
      if (BUBBLE_UNIFORM_OLD && !BUBBLE_TEXT_LATE_OLD && on && it->type == T_TEXT && (popKind(it->tag) == UI_POP_PILL || popKind(it->tag) == UI_POP_BOX) && (s_ != 1.f || a_ < 1.f)) continue;
      if (uiBubble992Old && !BUBBLE_TEXT942_OLD && !BUBBLE_UNIFORM_OLD && popKind(it->tag) == UI_POP_PILL && on && bubContent(it)) {   // №942 (и №992: только в режиме TEST70, новое — содержимое едет с рамкой по ветке ниже): текст и аватар не растут с рамкой: после раскрытия рамки проявляются на месте, конечной раскладкой
        float c = (popAgeOf(it->tag, 1) - UI_BUB_CONTENT_DELAY) / UI_BUB_CONTENT_FADE;
        if (c <= 0.f) continue;                                    // рамка → потом содержимое
        if (c > 1.f) c = 1.f;
        float e = 1.f - (1.f - c) * (1.f - c);                     // ease-out
        if (c < 1.f) { xf.on = 1; xf.s = 1.f; xf.a = e; xf.cx = xf.cy = 0.f; if (it->type == T_TEXT) bubSlideNow = (int)((float)UI_BUB_CONTENT_SLIDE * (1.f - e) + .5f); }
      } else
      if (!uiTextLate1004Old && !uiBubble1040 && it->type == T_TEXT && (popKind(it->tag) == UI_POP_PILL || popKind(it->tag) == UI_POP_BOX) && (s_ != 1.f || a_ < 1.f)) {   // №1004: текст пузыря не растёт и не сжимается с рамкой
        if (!on || s_ != 1.f) continue;                            // рамка раскрывается или уходит — текста нет
        float c = (popAgeOf(it->tag, 1) - UI_TL1004_DELAY) / UI_TL1004_FADE; if (c <= 0.f) continue; if (c > 1.f) c = 1.f;
        xf.on = 1; xf.s = 1.f; xf.a = a_ * c; xf.cx = xf.cy = 0.f;   // затем проявляется на месте, конечной раскладкой, без сдвига
      } else
      if (s_ != 1.f || a_ < 1.f) {
        const float *p = pv[it->tag & 31];
        xf.on = 1; xf.s = s_; xf.a = a_; xf.cx = (p[0] + p[2]) * .5f; xf.cy = (p[1] + p[3]) * .5f;
        if (popKind(it->tag) == UI_POP_PILL && BUBBLE_UNIFORM_OLD) { xf.s = 1.f; xf.sy = s_; if (pillCy > -1e8f) xf.cy = pillCy; }   // №516: только по высоте, вокруг центра самой пилюли
      }
    }
    if (subFadeA < 1.f) { if (!xf.on) { xf.on = 1; xf.s = 1.f; xf.a = 1.f; xf.cx = xf.cy = 0.f; } xf.a *= subFadeA; }   // №639: кроссфейд подэкранов паузы
    lastSpr[0] = lastSpr[1] = 1e9f; lastSpr[2] = lastSpr[3] = -1e9f;
    int ok_ = sidx == UI_SCREEN_HUD ? ovIndex(it->tag) : -1;
    gDy = ok_ >= 0 ? ovDy[ok_] : 0;
    gDx = (hud && bubShift && it->tag == UI_T_OBJECTIVE && !itemVarOff(it)) ? -bubShift : 0;   // №710
    gDx += bubSlideNow;   // №942
    if (hud && bubGH && !BUBBLE_EVEN_OLD && it->tag == UI_T_OBJECTIVE && (it->type == T_SLOT || (it->type == T_IMG && it->w * 2 < it->h * 3))) gDy -= bubGH / 2;   // №646: значок/аватар остаётся по центру выросшей рамки (рамка растёт вверх от низа)
    if (failureDy && it->type == T_TEXT && it->field == UI_F_NONE && it->y >= failureY && it->x < failureRight && it->x + it->w > failureLeft)
      gDy += failureDy;
    if (hud && it->tag == UI_T_ACTION) gDy += actionDy;   // №387: △ и любая подпись едут вместе
    if (xf.on && popKind(it->tag) == UI_POP_SLIDE) {   // счёт монет: группа сдвинута вверх за край на (1 - выезд) своей нижней кромки
      gDy -= (int)((1.f - xf.a) * (pv[it->tag & 31][3] + 2.f) + .5f); xf.a = xf.a < .5f ? xf.a * 2.f : 1.f;
    }
    if (hud && it->tag == UI_T_OBJECTIVE && objCollapse && (mask & tb)) {
      // сверка с оригиналом 30.09 (запись PS2 2:10–2:16, 25:10): задание с текстом видно OBJ_SHOW_S, потом рамка и текст уходят,
      // а значок цели уезжает в угол вверху слева и остаётся там до следующего задания (новый текст — снова внизу с рамкой)
      float oc = objOc;
      if (oc > 0.f) {
        if (it->type != T_SLOT) continue;
        float k = oc / OBJ_FLY_S; if (k > 1.f) k = 1.f; k = k * k * (3.f - 2.f * k);
        // правка Роба 365 (05.10: «высокие значки — крастибургер — не обрезать, когда уходят в верхний левый угол»): значок задания
        // извне рисуется рамкой аватара (objFrame: px, py — выше слота, у мест и предметов рисунок выше круга), а сдвиг считался от
        // верха слота — верх картинки уходил за край экрана. Теперь вверх к OBJ_TOP_Y едет верх рамки, которую рисуем. UI_OBJTOP_OLD=1 — прежнее
        int top = it->y;
#if !UI_OBJTOP_OLD
        if (it->field == UI_F_OBJICON && fieldv[UI_F_OBJICON] == UI_OBJICON_EXTERNAL) { int fx_, fy_, fw_, fh_; objFrame(it, &fx_, &fy_, &fw_, &fh_); top = fy_; }
#endif
        float shift = OBJ_TOP_Y - top;
        if (!UI_OBJTOP_OLD && it->field == UI_F_OBJICON && fieldv[UI_F_OBJICON] == UI_OBJICON_EXTERNAL && oiPix) {
          int bx_, by_, bw_, bh_; objFrame(it, &bx_, &by_, &bw_, &bh_);
          shift = uiIconTopShift(bx_, by_, bw_, bh_, oiW, oiH, oiVisL, oiVisT);
        }
        gDy = (int)floorf(k * shift + .5f);
      }
    }
    if (ok_ >= 0 && (it->type == T_NUM || it->type == T_TEXT)) ovBoxAdd(ok_, it->x, it->y + gDy, it->w, it->h);
    if (hudMove) gDy -= (int)floorf((1.f - hudBlk) * (float)HUD_CORNER_DY + .5f);   // №623: в углу блок выше на HUD_CORNER_DY (поле от верха = поле радара от низа)
    if ((it->flags & F_SELONLY) && it->menu != selected) continue;
    if (hid && (it->flags & (F_MENU | F_SELONLY)) && it->menu < 32 && (hid >> it->menu & 1)) continue;   // правка 137: скрытая строка
    if (hid) gDy += hideDy(sc, sidx, it);                                                               // строки ниже — вверх
    // правка 348: только экран паузы основного пакета — у второго пакета (scrMode) свой номер экрана: MissionSelect в pextra.shr = 1 =
    // UI_SCREEN_PAUSE, и названия миссий (F_MENU) и заголовок MISSION SELECT пропадали (Роб: «только 1 2 3 4, галочки и прямоугольник»)
    if (menuHidden && sidx == pauseScr && (!scrMode || scrLeakOld) && (it->flags & F_MENU)) continue;   // подменю (MISSION SELECT) рисует свой список
    // в подменю заголовка PAUSE MENU нет: MissionSelect.scr собран без Pause.pag (ingame.p3d), только PauseSunday/PauseMission
    if (menuHidden && sidx == pauseScr && (!scrMode || scrLeakOld) && ITEM_TITLE(it)) continue;
    // заглушка CamAndSet (3dFE.pag, tools/ui_export.py): прямоугольник во весь экран под рамкой ТВ
    if (!scrMode && !menuBackdrop && sidx == UI_SCREEN_MAIN && it->type == T_RECT && it->x <= 0 && it->y <= 0 && it->w >= 480 && it->h >= 272) continue;
    if (it->field == UI_F_SPEED || it->field == UI_F_SPEED_UNIT) continue;   // Роб 27.09: в оригинале спидометра нет
    if (radarIndoor1035 && !radar1035Old && hud && it->tag == UI_T_RADAR && (int)(it - &items[sc->first]) != radarRimIdx(sc)) continue;   // №1035: в помещении карта, значки, клин, стрелка и фон круга не рисуются — остаётся рамка-кольцо
    st.items++;
    // фитиль экрана загрузки: сдвиг вправо на долю загрузки (tools/load_export.py fuse_shift — то же округление)
    int fuse = (it->flags & F_FUSE) ? (int)floorf(fieldv[UI_F_PROGRESS] * (1.f / 1000.f) * it->ang + .5f) : 0;
    unsigned pfI0 = pfOn ? sceKernelGetSystemTimeLow() : 0;
    switch (it->type) {
      case T_RECT:
        if (fuse) { UiItem r = *it; r.x = (short)(r.x + fuse); r.w = (short)(r.w - fuse); if (r.w > 0) drawRect(&r); }
        else drawRect(it);
        break;
      case T_POLY: drawPoly(it); break;
      case T_IMG: {
        if (ITEM_EXT(it)) { drawExt(brPix, brTw, brTh, brW, brH, it->x - (brWager ? WAGER_PIC_DX : 0), it->y, it->w, it->h, it->color); break; }   // картинка задания (ставка — левее)
        if (it->tag == UI_T_HNR_FULL) { drawHnrFill(it); break; }
        // Роб 27.09: неподвижное синее кольцо Hit & Run не рисуем (оно и есть заполнение — рисуется drawHnrFill по
        // шкале). Мигалка над радаром видна всегда (Роб 27.09 вечер: «мигалка была, пропала»), кадр — uiSetHitAndRun.
        // Роб 28.09 (правка 36): дорожка под заливкой — полужёлтая (ui_anim.h UI_HNR_TRACK), не пустая
        if (it->tag == UI_T_HNR_METER && it->field != UI_F_HNR_LIGHT) {
          if (animOff & UI_ANIM_OLDHNR) break;   // прежний код (отрицательный контроль гейта): дорожки нет
          if (it->count) { const UiSlot *s = &slots[it->first]; drawSlot(s, s->x, s->y, s->w, UI_HNR_TRACK);
            if (traceBits & UI_TRACE_HNR) uiLogf("UIHNRBG t=%.3f col=%08x x=%d y=%d w=%d h=%d\n", uiClk, UI_HNR_TRACK, s->x, s->y, s->w, s->h); }
          break;
        }
        int fr = it->field ? fieldv[it->field] : 0;
        float flS = 1.f, flA = 1.f; int flick = 0;
        if ((it->flags & F_RATE) && it->count) fr = (int)(scrT * it->px) % it->count;   // кадр по часам экрана
#if !FUSE_FLICKER_OLD
        if (scrMode && (it->flags & F_RATE) && it->field == UI_F_FIRE && it->count >= 2) { fr = fireFlicker(scrT, it->count, &flS, &flA); flick = 1; }
#endif
        if (fr < 0) fr = 0;
        if (fr >= it->count) fr = it->count - 1;
        const UiSlot *s = &slots[it->first + fr];
        if (flick) { xf.on = 1; xf.s = flS; xf.a = flA; xf.cx = s->x + fuse + s->w * .5f; xf.cy = s->y + gDy + s->h * .5f; }
        if (it->res[1] && it->res[1] <= 12) {                    // бегунок ползунка меню: по дорожке ang (px) по значению строки
          int v = fieldv[UI_F_ROW0 + it->res[1] - 1]; if (v < 0) v = 0; if (v > 1000) v = 1000;
          fuse += (int)floorf((v - 500) * (1.f / 1000.f) * it->ang + .5f);
        }
        if (hud && it->tag == UI_T_ACTION && !(animOff & UI_ANIM_NOPULSE)) {   // подсказка △ дышит от своего центра (ui_anim.h, п. 1)
          xf.on = 1; xf.a = 1.f; xf.s = uiPulseScale(uiClk - tagOnT[UI_T_ACTION]);
          xf.cx = s->x + s->w * .5f; xf.cy = s->y + gDy + s->h * .5f;
        }
        // №482 (Роб: на первом экране логотип стоит, мигает только Press Start): вращение 336 — для звёзд экрана загрузки; на титульном
        // (UI_BOOT_SPLASH) вращающиеся элементы страницы рисуются в нулевом угле. TITLE_LOGO_STILL_OLD=1 — прежнее
        if (scrMode && ITEM_SPIN(it)) {
#ifndef TITLE_LOGO_STILL_OLD
#define TITLE_LOGO_STILL_OLD 0
#endif
          float t0_ = scrT; if (!TITLE_LOGO_STILL_OLD && sidx == UI_BOOT_SPLASH) scrT = 0.f;
          drawSpin(it, s); scrT = t0_; break; }   // правка 336: крутящиеся звёзды экрана загрузки
        if (scrMode && it->field == UI_F_LEVEL && (newsAng != 0.f || newsScale != 1.f || newsAlpha < 1.f)) {   // газета в полёте
          if (newsScale > 0.f && newsAlpha > 0.f) drawSlotRot(s, s->x, s->y, it->color);
          break;
        }
        unsigned color = it->color;
        if (scrMode && (unsigned)sidx < UI_SCREEN_MAX) {
          unsigned tint = scrTint[sidx][it->tag & 31];
          if (tint) { color = 0; for (int k = 0; k < 4; k++) color |= (((it->color >> (8*k) & 255) * (tint >> (8*k) & 255) + 127) / 255) << (8*k); }
        }
        if (bubDrawGrow && s->w > 3 * s->h / 2 && s->w >= 8 && s->h >= 8) drawSlotGrow(s, it->x + fuse, it->y, bubDW, bubDH, color);   // №646: рамка задания выросла
        else drawSlot(s, it->x + fuse, it->y, s->w, color);
        if (hud && it->tag == UI_T_ACTION && (traceBits & UI_TRACE_PULSE)) {   // гейт пульса: рамка △, как ушла в GE
          float r[4] = {s->x, s->y, s->x + s->w, s->y + s->h};
          if (xf.on) memcpy(r, lastSpr, sizeof r);
          uiLogf("UIPULSE t=%.3f age=%.3f x0=%.2f y0=%.2f x1=%.2f y1=%.2f w0=%d\n", uiClk, uiClk - tagOnT[UI_T_ACTION], r[0], r[1], r[2], r[3], s->w);
        }
        break;
      }
      case T_SLOT: {
        if (!it->count) break;
        if (it->field == UI_F_OBJICON && fieldv[UI_F_OBJICON] < 0) break;   // задание без значка: только текст (начало — objTextStart)
        if (it->field == UI_F_OBJICON && fieldv[UI_F_OBJICON] == UI_OBJICON_EXTERNAL) { drawObjIcon(it, it->color); break; }
        int k = fieldv[it->field] % it->count; if (k < 0) k += it->count;
        const UiSlot *s = &slots[it->first + k];
        drawSlot(s, s->x, s->y, s->w, it->color);
        break;
      }
      case T_BAR: {
        const UiSlot *s = &slots[it->first];
        int v = fieldv[it->field]; if (v < 0) v = 0; if (v > 1000) v = 1000;
        int w = (s->w * v + 500) / 1000;
        if (w > 0) drawSlot(s, s->x, s->y, w, it->color);
        break;
      }
      case T_SECTOR: break; // legacy eight shutters replaced by continuous radial clipping
      case T_TEXT: {
        const char *s;
        if (!it->count) break;
        if (it->field == UI_F_MSN_TITLE) s = brTitle;                 // брифинг: название и описание миссии (библия)
        else if (it->field == UI_F_MSN_INFO) s = brInfo;
        else if (it->field == UI_F_MSN_FAIL) s = ovReason;            // провал: причина и подсказка (библия)
        else if (it->field == UI_F_MSN_HINT) s = ovHint;
        else if (it->field == UI_F_MENU_ITEM) s = str(it->first + (unsigned)((selected < 0 ? 0 : selected) % it->count));
        else if (it->fmt == FMT_INT) { snprintf(buf, sizeof buf, "%d", fieldv[it->field]); s = buf; }
        else if (it->fmt == FMT_FRAC) { fmtField(it, buf, sizeof buf); s = buf; }                     // LevelStats: «3/7»
        else if (it->fmt == FMT_TIME && it->field >= UI_F_WAGER_FEE) { fmtField(it, buf, sizeof buf); s = buf; }   // карточка ставки: «3:15»
        else if (it->fmt == FMT_PCT) { snprintf(buf, sizeof buf, "%d%%", fieldv[it->field]); s = buf; }   // LevelStats: «42%»
        else if (it->fmt == FMT_ORDINAL) { int p = fieldv[it->field]; s = p < 0 ? "" : str(it->first + (unsigned)(p >= 1 && p <= 3 ? p - 1 : 3)); }
        else if (it->field == UI_F_MSGBOX) s = mboxText;                 // правка 122: окно сообщения
        else if (it->field == UI_F_ACTION_TEXT) s = actText;             // правка 206: подпись у △
        else if (it->field == UI_F_PLACE) s = placeText;                 // №598: код клетки на паузе
        else if (it->field == UI_F_OBJECTIVE && msgText[0]) s = msgText;
        else if (it->field == UI_F_OBJECTIVE && (leaveDraw & tb) && msgLast[0]) s = msgLast;   // гаснущий текст задания
        else if (it->fmt == FMT_SELECT) { int k = ((it->flags & F_RATE) ? (int)(scrT * it->px) : fieldv[it->field]) % it->count;
                                          if (k < 0) k += it->count; s = str(it->first + (unsigned)k); }
        else s = str(it->first);
        if (scrMode) {   // uiScrRowText: подпись строки (F_MENU) или текст с полем ROW<k> — из кода
          int r = (it->flags & F_MENU) ? it->menu : (it->field >= UI_F_ROW0 && it->field <= UI_F_ROW11) ? it->field - UI_F_ROW0 : -1;
          if (r >= 0 && r < 12 && (unsigned)sidx < UI_SCREEN_MAX && scrRowTxt[RT_SC(sidx)][r]) s = scrRowTxt[RT_SC(sidx)][r];
        }
        unsigned col = it->color;
        if (it->flags & F_MENU) {
          if (it->menu < 32 && (menuOff[sidx] >> it->menu & 1)) col = DIM_ABGR;
          else if (it->menu == selected) col = SEL_ABGR;
        }
        if (hud && it->field == UI_F_ACTION_TEXT && !(animOff & UI_ANIM_NOACTFADE)) {   // правка 272: подпись у △ проявляется вместе
          float a = (uiClk - tagOnT[UI_T_ACTION]) / ACT_FADE_S;                          // с пружиной значка (ui_anim.h п. 1), а не
          if (a < 1.f) { xf.on = 1; xf.s = 1.f; xf.a = a < 0.f ? 0.f : a; xf.cx = xf.cy = 0.f; }   // выскакивает целиком
        }
        if (it->field == UI_F_OBJECTIVE) { UiItem t2; objTextItem(it, &t2); drawTextItem(&t2, s, col);
        } else if (scrMode && it->tag == UI_T_PRESS_START) {   // №405: пульс масштабом от центра надписи (запись PS2)
          if (!TITLE_PULSE_OLD) { xf.on = 1; xf.a = subFadeA; xf.s = psPulse(scrT); xf.cx = it->x + it->w * .5f; xf.cy = it->y + it->h * .5f; }
          drawTextItem(it, s, col);
          uiPressStartBox[0] = scrT; memcpy(uiPressStartBox + 1, lastSpr, sizeof lastSpr);
        } else if (!pAnim && (pulseFlagsOf(sidx) & 2) &&
                   ((sidx == UI_SCREEN_MAIN && !scrMode && it->field == UI_F_MENU_ITEM) ||
                   ((it->flags & F_MENU) && it->menu == selected))) {   // правка 412: пульс выбранного пункта
          unsigned now = sceKernelGetSystemTimeLow(); float d = (now - menuPulseLast) * 1e-3f; menuPulseLast = now;
          int key = scrMode * 256 + sidx;
          if (selected != menuPulseSel || key != menuPulseKey || d > 100.f || d < 0.f) { menuPulseSel = selected; menuPulseKey = key; menuPulseMs = 0.f; }   // смена пункта / экрана / пауза
          else menuPulseMs = fmodf(menuPulseMs + d, MENU_PULSE_MS);
          // №924: до правки (MENU_PULSE924_OLD=1) главное меню стояло. Ветка та же, но масштаб 1 и без преобразования: строка рисуется как раньше, а рамка всё равно попадает в журнал MENUITEM —
          // красный гейт tests/menu/item_pulse.py падает по ширине (не пульсирует), а не по отсутствию строк
          int still_ = MENU_PULSE_OLD || (!MENU_MAIN_PULSE_OLD && MENU_PULSE924_OLD && sidx == UI_SCREEN_MAIN && !scrMode);
          float sc_ = still_ ? 1.f : menuPulse(menuPulseMs) / (MENU_PULSE_MID + MENU_PULSE_AMP);   // правило порта: наибольший — строка порта
          if (menuVecWanted(it, sidx, selected, s)) menuVecPlate(it, s);   // №1012: плашка стоит, пульсирует только текст
          if (!still_) { xf.on = 1; xf.a = subFadeA; xf.s = sc_; xf.cx = it->x + it->w * .5f; xf.cy = (float)(it->y + gDy) + it->h * .5f; }
          lastSpr[0] = lastSpr[1] = 1e9f; lastSpr[2] = lastSpr[3] = -1e9f;
          drawTextItem(it, s, col); xf.on = 0;
          uiMenuItemBox[0] = menuPulseMs; uiMenuItemBox[1] = sc_; memcpy(uiMenuItemBox + 2, lastSpr, sizeof lastSpr);
          if (traceBits & UI_TRACE_MENU) uiLogf("MENUITEM us=%u scr=%d mode=%d sel=%d ms=%.1f s=%.4f x0=%.2f y0=%.2f x1=%.2f y1=%.2f\n", now, sidx, scrMode,
                                                selected, menuPulseMs, sc_, lastSpr[0], lastSpr[1], lastSpr[2], lastSpr[3]);
        } else if (it->fmt == FMT_ORDINAL && hud && !ORDINAL_OLD) {   // №545 (Роб 11.10): окончание «ST/ND/RD» места в гонке ближе к цифре
          UiItem t2 = *it; t2.x = (short)(t2.x - ORDINAL_DX); drawTextItem(&t2, s, col);
        } else if (scrMode && !TITLE_COINS_FIT_OLD && it->halign == 1 && !(it->flags & F_WRAP)) {   // лавка: название не заходит под счёт монет
          int cl = coinsLeftInRow(sc, it->y, it->y + it->h);
          if (cl != 0x7fffffff) {
            const UiFont *f_ = itemFont(it, curScreen); int tw = lineWidth(f_, s, (int)strlen(s)), right = it->x + floordiv(it->w - tw, 2) + tw;
            int sh = right - (cl - TITLE_COINS_GAP), room = it->x + floordiv(it->w - tw, 2) - TITLE_COINS_LEFT;
            if (sh > room) sh = room;
            if (sh > 0) { UiItem t2 = *it; t2.x = (short)(t2.x - sh); if (menuVecWanted(it, sidx, selected, s)) menuVecPlate(&t2, s); drawTextItem(&t2, s, col); break; }
          }
          if (menuVecWanted(it, sidx, selected, s)) menuVecPlate(it, s);   // №1012: страницы OPTIONS (PICTURE, SOUND, CAMERA, GAME) идут этой веткой
          drawTextItem(it, s, col);
        } else { if (menuVecWanted(it, sidx, selected, s)) menuVecPlate(it, s); drawTextItem(it, s, col); }   // №1012
        break;
      }
      case T_NUM:
        if (it->field == UI_F_MSN_DONE) {   // правка 122: «MISSION COMPLETE!» / «BONUS MISSION» + «COMPLETE!» (две строки, как в записи PS2 26:50)
          const char *tx = msnDoneText[0] ? msnDoneText : str(it->first); int nl = 1;
          for (const char *q = tx; *q; q++) if (*q == '\n') nl++;
          int lh = it->h > 0 ? it->h : 16, k = 0; char ln_[64];
          for (const char *q = tx; ; k++) {
            const char *e = strchr(q, '\n'); int n = e ? (int)(e - q) : (int)strlen(q); if (n > 63) n = 63;
            memcpy(ln_, q, (size_t)n); ln_[n] = 0;
            UiItem t2 = *it; t2.y = (short)(it->y + (2 * k - (nl - 1)) * lh / 2); drawNum(&t2, ln_);
            if (!e) break; q = e + 1;
          }
        }
        else if ((it->field == UI_F_TIMER || it->field == UI_F_PARTIME) && hud && !HUD_SHIFT_COND_OLD) {
          // №623: левый край времени между углом (кружка нет) и правым краем круга + TIMER_ICON_GAP (кружок наверху); переход той же кривой, что подъём значка
          int right = -9999;
          for (unsigned j = 0; j < sc->count; j++) { const UiItem *c = &items[sc->first + j];
            if (c->type == T_SLOT && c->field == UI_F_OBJICON) { int bx_, by_, bw_, bh_; objFrame(c, &bx_, &by_, &bw_, &bh_); if (bx_ + bw_ > right) right = bx_ + bw_; } }
          float xt = (float)HUD_CORNER_X; if (right > -9999) xt += hudBlk * (float)(right + TIMER_ICON_GAP - HUD_CORNER_X);
          if (iconRightX >= 0.f) { float xi = fmaxf((float)HUD_CORNER_X, iconRightX + (float)HUD_HOME_GAP); xt = hudHomeOn ? xt + (xi - xt) * iconKK : HUD_CORNER_X + hudBlk * (xi - HUD_CORNER_X); }   // Роб 17.10: от рисунка
          fmtField(it, buf, sizeof buf); UiItem t2 = *it; t2.halign = 0; t2.x = (short)floorf(xt + .5f); drawNum(&t2, buf);
        }
        else if (it->field == UI_F_TIMER && !TIMER_ICON_OLD && hud && objCollapse && (mask >> UI_T_OBJECTIVE & 1u) && objOc > 0.f) {
          // №540 (Роб 11.10): таймер налезал на круглый значок цели в углу; со значком левый край времени = правый край круга + TIMER_ICON_GAP
          int right = -9999;
          for (unsigned j = 0; j < sc->count; j++) { const UiItem *c = &items[sc->first + j];
            if (c->type == T_SLOT && c->field == UI_F_OBJICON) { int bx_, by_, bw_, bh_; objFrame(c, &bx_, &by_, &bw_, &bh_); if (bx_ + bw_ > right) right = bx_ + bw_; } }
          fmtField(it, buf, sizeof buf);
          if (right > -9999) { UiItem t2 = *it; t2.halign = 0; t2.x = (short)(right + TIMER_ICON_GAP); drawNum(&t2, buf); }
          else drawNum(it, buf);
        }
        else if (it->field == UI_F_COUNTDOWN && cdText[0] && !COUNTDOWN_LINE_OLD) drawNum(it, cdText);   // №630: слово отсчёта тем же шрифтом
        else if (it->field) { fmtField(it, buf, sizeof buf); drawNum(it, buf); }
        else drawNum(it, str(it->first));
        break;
      case T_RADARMAP: drawRadarMap(it); break;
      case T_RADARICONS: drawRadarIcons(it); break;
    }
    if (pfOn) { unsigned d_ = sceKernelGetSystemTimeLow() - pfI0; if ((unsigned)it->type < 10) { pfTyUs[it->type] += d_; pfTyN[it->type]++; } pfTagUs[it->tag & 31] += d_; pfItemsN++; }
    if (hud && (traceBits & UI_TRACE_POP) && it->tag == UI_T_OBJECTIVE && lastSpr[2] > lastSpr[0] && (bubContent(it) || it->type == T_IMG))   // №942 (гейт): что и где нарисовано по кадрам появления; f — капсула, t — текст, a — аватар/значок
      uiLogf("UIBUB942 age=%.3f part=%c x0=%.1f y0=%.1f x1=%.1f y1=%.1f a=%.2f wrapw=%d nl=%d\n", popAgeOf(UI_T_OBJECTIVE, 1), it->type == T_TEXT ? 't' : bubContent(it) ? 'a' : 'f',
             lastSpr[0], lastSpr[1], lastSpr[2], lastSpr[3], xf.on ? xf.a : 1.f, dbgWrapW, dbgNl);
    if (hud && (traceBits & UI_TRACE_AVATAR) && it->tag == UI_T_OBJECTIVE && it->type == T_IMG && lastSpr[2] > lastSpr[0])
      uiLogf("UIBUBBLE t=%.3f x=%.2f y=%.2f w=%.2f h=%.2f\n", uiClk, lastSpr[0], lastSpr[1], lastSpr[2] - lastSpr[0], lastSpr[3] - lastSpr[1]);   // пузырь задания, как ушёл в GE
    if (hud && (traceBits & UI_TRACE_AVATAR) && it->type == T_TEXT && it->field == UI_F_OBJECTIVE && lastSpr[2] > lastSpr[0])   // правка 48: текст задания, как ушёл в GE
      uiLogf("UIOBJTEXT t=%.3f x=%.2f y=%.2f w=%.2f h=%.2f icon=%d\n", uiClk, lastSpr[0], lastSpr[1], lastSpr[2] - lastSpr[0], lastSpr[3] - lastSpr[1], fieldv[UI_F_OBJICON]);
    if (hud && popKind(it->tag) >= 0 && lastSpr[2] > lastSpr[0]) {   // нарисованная рамка группы — для гейта всплывания
      float *b = tbox[it->tag & 31];
      if (lastSpr[0] < b[0]) b[0] = lastSpr[0]; if (lastSpr[1] < b[1]) b[1] = lastSpr[1];
      if (lastSpr[2] > b[2]) b[2] = lastSpr[2]; if (lastSpr[3] > b[3]) b[3] = lastSpr[3];
    }
  }
  unsigned pfF0 = pfOn ? sceKernelGetSystemTimeLow() : 0; if (pfOn) pfItemsUs += pfF0 - pfLoop0;
  bflush(); gDy = 0; gDx = 0; xf.on = 0;
  if (pfOn) pfFlushUs += sceKernelGetSystemTimeLow() - pfF0;
  if (hud && (traceBits & UI_TRACE_POP))   // гейт всплывания: рамка группы как нарисована (без масштаба — рамка данных) и альфа
    for (int t = 0; t < 32; t++) {
      unsigned b = 1u << t; float s_ = 1.f, a_ = 1.f;
      if (popKind(t) < 0 || !((mask | leaveDraw) & b) || pv[t][2] < pv[t][0]) continue;
      int on = (mask & b) != 0;
      if (!(animOff & UI_ANIM_NOPOP)) uiPopState(popKind(t), on, popAgeOf(t, on), &s_, &a_);
      const float *r = tbox[t][2] > tbox[t][0] ? tbox[t] : pv[t];
      uiLogf("UIPOP t=%.3f tag=%d on=%d age=%.3f w=%.2f h=%.2f w0=%.0f h0=%.0f a=%.3f\n", uiClk, t, on,
             popAgeOf(t, on), r[2] - r[0], r[3] - r[1], pv[t][2] - pv[t][0], pv[t][3] - pv[t][1], a_);
    }
}

void uiDrawHud(void) { drawScreen(UI_SCREEN_HUD, -1); }
void uiDrawBriefing(void) { drawScreen(UI_SCREEN_MISSIONLOAD, -1); }
void uiDrawLetterbox(float open, int prompt) {   // правка 40: см. ui.h
  if (!H || typoShot >= 2) return;
  if (!(open > 0.f)) return;
  if (open > 1.f) open = 1.f;
  int bh = (int)(UI_LB_BAR * open + .5f);
  if (bh > 0) {
    beginUi(); bn = 0; btex = -1; xf.on = 0;
    UiItem r; memset(&r, 0, sizeof r); r.type = T_RECT; r.x = 0; r.w = 480; r.h = (short)bh; r.color = 0xff000000u;
    r.y = 0; drawRect(&r); r.y = (short)(272 - bh); drawRect(&r);
    bflush();
  }
  if (prompt && open >= 1.f && (unsigned)UI_SCREEN_LETTERBOX < H->nscreen) {   // старый ui.shr без экрана — только полосы
    unsigned keep = tagMask[UI_SCREEN_LETTERBOX];
    tagMask[UI_SCREEN_LETTERBOX] = 1u << UI_T_ALWAYS | 1u << (prompt == UI_LB_START ? UI_T_LB_START : UI_T_LB_SKIP);
    drawScreen(UI_SCREEN_LETTERBOX, -1);
    tagMask[UI_SCREEN_LETTERBOX] = keep;
  }
}
void uiDrawMissionOver(int sel) { drawScreen(UI_SCREEN_MISSIONOVER, sel); }
// подсказка обучения: экран Tutorial.scr + строки текста спрайтовым шрифтом TutorialMessage. Строка k — по центру места
// TutorialMessage по x; первая строка — на его центре, шаг — половина высоты места (не меньше самого высокого знака + 1): 4 строки
// ложатся как в записи PS2 (1:05, строки 207..307 из 480), короче — тем же центром блока
static const UiItem *tutItem(void) {
  if (!H || (unsigned)UI_SCREEN_TUTORIAL >= H->nscreen) return 0;
  const UiScreen *sc = &screens[UI_SCREEN_TUTORIAL];
  for (unsigned i = 0; i < sc->count; i++) { const UiItem *it = &items[sc->first + i]; if (it->type == T_NUM && it->count > 1) return it; }
  return 0;
}
int uiTutorialCount(void) { const UiItem *it = tutItem(); return it ? it->count : 0; }
const char *uiTutorialText(int msg) { const UiItem *it = tutItem(); return it && msg >= 0 && msg < it->count ? str(it->first + (unsigned)msg) : ""; }
static int numWidth(const UiItem *it, const char *s) {   // ширина строки спрайтовым шрифтом (как считает drawNum)
  if (it->font >= H->nsfont || !s) return 0;
  const UiSGlyph *G = &sfonts[it->font * 256]; int tw = 0;
  for (; *s; s++) { unsigned c = (unsigned char)*s; if (c >= 'a' && c <= 'z') c -= 32; if (G[c].flags) tw += G[c].adv; }
  return tw;
}
// правка 122 (сверка 30.09, запись PS2 26:50): окно сообщения Message.scr — рамка со значком, текст с переносом по рамке,
// Continue и ✕ Accept; рисуется вместо HUD, мир стоит (main.c)
int uiDrawMessageBox(const char *text) {
  if (!H || (unsigned)UI_SCREEN_MESSAGE >= H->nscreen) return 0;
  static unsigned mboxLast, mboxT0; static char mboxPrev[256];
  unsigned now = sceKernelGetSystemTimeLow();
  if (!text) text = "";
  if (now - mboxLast > 200000u || strcmp(mboxPrev, text)) mboxT0 = now;   // новое окно: пауза в рисовании или другой текст
  mboxLast = now; snprintf(mboxPrev, sizeof mboxPrev, "%s", text);
  snprintf(mboxText, sizeof mboxText, "%s", text);
  mboxK = mboxA = 1.f;
  if (!BUBBLE_UNIFORM_OLD) {   // №639: как пузырь задания (№638): 0,25 → 1 за 8 шагов по 1/50 с, прозрачность 0,3 → 1 за 0,06 с; центр — центр самой большой плашки экрана
    float t = (now - mboxT0) * 1e-6f; mboxK = uiBubScale(t); mboxA = (t < 0.06f && !uiBubble1040) ? 0.3f + 0.7f * t / 0.06f : 1.f;
    const UiScreen *sc = &screens[UI_SCREEN_MESSAGE]; int ba = 0; mboxCx = 240.f; mboxCy = 136.f;
    for (unsigned i = 0; i < sc->count; i++) { const UiItem *b = &items[sc->first + i];
      if ((b->type == T_RECT || b->type == T_IMG) && b->w * b->h > ba) { ba = b->w * b->h; mboxCx = b->x + b->w * .5f; mboxCy = b->y + b->h * .5f; } }
  }
  unsigned keep = tagMask[UI_SCREEN_MESSAGE]; tagMask[UI_SCREEN_MESSAGE] = 1u << UI_T_ALWAYS;
  drawScreen(UI_SCREEN_MESSAGE, -1);
  tagMask[UI_SCREEN_MESSAGE] = keep; bflush(); mboxK = mboxA = 1.f;
  return 1;
}
// Правка Роба 267 (04.10: «подсказки Барта выходят за пределы рамки; текст громадный, еле вмещается — доработать типографику по
// размерам, подаче, ритму»; ничего не убирать): буквы — ровно вчетверо мельче картинок (tools/ui_export.py TUT_GLYPH_K), строка
// библии шире рамки без полей TUT_PAD_X переносится по словам; шаг строк — высота буквы + TUT_LEAD, один ритм для всех подсказок;
// блок — по центру свободного места рамки: от низа значка Барта до низа рамки (поля сверху и снизу равны), строки — по центру
// рамки. Правило порта: у PS2 первая строка на трети рамки при любом числе строк (одна строка прилипала к верху, четыре — к низу).
// «flag tutold» (uiTutLayoutOld) — прежняя раскладка: красный контроль гейта tests/ui/tutorial_fit.py
#define TUT_PAD_X 16   // px PSP от внутреннего края рамки до самой широкой строки
#define TUT_LEAD 1     // px между ячейками букв (у ячейки свой запас под обводку)
#define TUT_MAXL 8
static int tutOld = 1;   // 04.10 07:2x Роб: подсказки Барта — как было (правка 267 была не про них); новая раскладка — «flag tutold 0»
void uiTutLayoutOld(int on) { tutOld = on != 0; }
static void tutBox(int *bx, int *by, int *bw, int *bh, int *avBot) {   // рамка (самая большая плашка экрана) и низ значка Барта
  const UiScreen *sc = &screens[UI_SCREEN_TUTORIAL]; *bx = *by = *bw = *bh = 0; *avBot = 0;
  for (unsigned i = 0; i < sc->count; i++) { const UiItem *b = &items[sc->first + i];
    if (b->type == T_RECT && b->w * b->h > *bw * *bh) { *bx = b->x; *by = b->y; *bw = b->w; *bh = b->h; } }
  *avBot = *by;
  for (unsigned i = 0; i < sc->count; i++) { const UiItem *b = &items[sc->first + i];   // значок: картинка уже половины рамки,
    if ((b->type == T_IMG || b->type == T_SLOT) && b->w < *bw / 2 && b->y < *by && b->y + b->h > *by &&   // заходит на её верх
        b->x < *bx + *bw / 2 && b->x + b->w > *bx + *bw / 2 && b->y + b->h > *avBot) *avBot = b->y + b->h; }
}
void uiSubFade(float a) { subFadeA = a < 0.f ? 0.f : a > 1.f ? 1.f : a; }
void uiTutorialScale(float k) { tutK = k > 0.f ? k : 1.f; }
int uiDrawTutorial(int msg, int cont) {
  const UiItem *it = tutItem();
  if (!it) return 0;
  unsigned keep = tagMask[UI_SCREEN_TUTORIAL];   // TutorialMessage в пакете с меткой SKIP: его строки рисуются ниже
  tagMask[UI_SCREEN_TUTORIAL] = 1u << UI_T_ALWAYS | (cont ? 1u << UI_T_TUT_CONTINUE : 0u);
  drawScreen(UI_SCREEN_TUTORIAL, -1);
  tagMask[UI_SCREEN_TUTORIAL] = keep;
  int bx, by, bw, bh, avBot; tutBox(&bx, &by, &bw, &bh, &avBot);
  const char *s = uiTutorialText(msg); char line[TUT_MAXL][96]; int n = 0, k = 0;
  if (tutOld) {
    for (const char *p = s; n < 4; p++) {   // «\» и '\n' — перенос строки библии
      if (*p == '\\' || *p == '\n' || !*p) { line[n][k] = 0; if (k || *p) n++; k = 0; if (!*p) break; continue; }
      if (k < 95) line[n][k++] = *p;
    }
  } else {   // строки библии; слово, что не влезает в рамку без полей, — на следующую строку
    UiItem m = *it; int maxW = bw - 2 * TUT_PAD_X; char cur[96]; int cl = 0; cur[0] = 0;
    for (const char *p = s; n < TUT_MAXL; ) {
      char w[96]; int wl = 0;
      while (*p == ' ') p++;
      while (*p && *p != ' ' && *p != '\\' && *p != '\n' && wl < 95) w[wl++] = *p++;
      w[wl] = 0;
      if (wl) {
        char t2[192]; snprintf(t2, sizeof t2, "%s%s%s", cur, cl ? " " : "", w);
        if (cl && numWidth(&m, t2) > maxW) { snprintf(line[n++], 96, "%s", cur); snprintf(cur, sizeof cur, "%s", w); cl = wl; if (n >= TUT_MAXL) break; }
        else { snprintf(cur, sizeof cur, "%s", t2); cl = (int)strlen(cur); }
      }
      if (*p == '\\' || *p == '\n' || !*p) { if (cl) { snprintf(line[n++], 96, "%s", cur); cl = 0; cur[0] = 0; } if (!*p) break; p++; }
    }
  }
  const UiSGlyph *G = it->font < H->nsfont ? &sfonts[it->font * 256] : 0; int gh = 0;
  for (int c = 0; G && c < 256; c++) if (G[c].flags && G[c].tex != 0xffff && G[c].h > gh) gh = G[c].h;
  // прежнее: строки сверху вниз, первая — на центре места TutorialMessage, шаг — половина его высоты (запись PS2: 1 строка «PRESS △
  // TO GET INTO CAR» 1:45, 2 строки монет 2:30 и 4 строки ходьбы 1:05 начинаются на одной высоте, трети рамки; шаг 0.18 рамки)
  float pitchF = it->h * .5f; int pitch = (int)(pitchF + .5f);
  float cy0 = it->y + it->h * .5f;
  int top = 0;
  if (!tutOld) { pitch = gh + TUT_LEAD; pitchF = (float)pitch;
    int blk = (n - 1) * pitch + gh; top = avBot + floordiv(by + bh - avBot - blk, 2); }
  beginUi(); bn = 0; btex = -1; xf.on = 0; xf.sy = 0.f;
  if (tutK < .999f) { xf.on = 1; xf.s = tutK; xf.a = 1.f; xf.cx = 240.f; xf.cy = 136.f; }   // №516: текст растёт вместе с рамкой
  int tx0 = 9999, tx1 = -9999, ty0 = 9999, ty1 = -9999;
  for (int l = 0; l < n; l++) {
    UiItem t = *it; t.halign = 1; t.valign = 1;
    if (tutOld) { t.h = (short)pitch; t.y = (short)(cy0 + l * pitchF - pitch * .5f + .5f); }
    else { t.x = (short)bx; t.w = (short)bw; t.h = (short)gh; t.y = (short)(top + l * pitch); }
    drawNum(&t, line[l]);
    int w = numWidth(&t, line[l]), x0 = t.x + floordiv(t.w - w, 2);
    if (x0 < tx0) tx0 = x0; if (x0 + w > tx1) tx1 = x0 + w;
    if (t.y + floordiv(t.h - gh, 2) < ty0) ty0 = t.y + floordiv(t.h - gh, 2); if (t.y + floordiv(t.h - gh, 2) + gh > ty1) ty1 = t.y + floordiv(t.h - gh, 2) + gh;
  }
  bflush(); xf.on = 0;
  static int logMsg = -1, logCont = -1;   // гейт tests/ui/tutorial.sh: раскладка подсказки, как ушла в GE (раз на подсказку / Continue)
  if (uiLogFn && (msg != logMsg || cont != logCont)) {
    logMsg = msg; logCont = cont;
    uiLogf("UITUT msg=%d lines=%d cont=%d box=%d,%d,%d,%d text=%d,%d,%d,%d glyph=%d pitch=%d avbot=%d old=%d t=%.2f\n", msg, n, cont,
           bx, by, bw, bh, tx0, ty0, tx1 - tx0, ty1 - ty0, gh, pitch, avBot, tutOld, uiClk);
  }
  return n;
}
int uiLevelStatsPct(const UiLevelStats *s) {
  int d = 0, t = 0;
  for (int k = 0; k < UI_LS_COUNT; k++) { if (s->total[k] <= 0) continue; t += s->total[k]; d += s->done[k] < s->total[k] ? s->done[k] : s->total[k]; }
  return t ? (d * 100 + t / 2) / t : 0;
}
void uiDrawLevelStats(const UiLevelStats *s, int levelEnd) {
  if (!H || (unsigned)UI_SCREEN_LEVELSTATS >= H->nscreen) return;
  fieldv[UI_F_STAT_MODE] = levelEnd ? 1 : 0; fieldv[UI_F_STAT_PCT] = uiLevelStatsPct(s);
  for (int k = 0; k < UI_LS_COUNT; k++) { fieldv[UI_F_STAT0 + 2 * k] = s->done[k]; fieldv[UI_F_STAT0 + 2 * k + 1] = s->total[k]; }
  unsigned keep = tagMask[UI_SCREEN_LEVELSTATS];
  tagMask[UI_SCREEN_LEVELSTATS] = 1u << UI_T_ALWAYS | 1u << (levelEnd ? UI_T_LS_END : UI_T_LS_PAUSE);
  drawScreen(UI_SCREEN_LEVELSTATS, -1);
  tagMask[UI_SCREEN_LEVELSTATS] = keep;
}
void uiDrawMenu(int menu, int selected) {
  if (menu == UI_MENU_MAIN) fieldv[UI_F_MENU_ITEM] = selected;
  drawScreen(menu, selected);
}
int uiMenuCount(int menu) { return (H && (unsigned)menu < H->nscreen) ? (int)screens[menu].nmenu : 0; }
const char *uiMenuLabel(int menu, int item) {
  if (!H || (unsigned)menu >= H->nscreen) return "";
  const UiScreen *sc = &screens[menu];
  for (unsigned i = 0; i < sc->count; i++) {
    const UiItem *it = &items[sc->first + i];
    if (it->type != T_TEXT) continue;
    if ((it->flags & F_MENU) && it->menu == item) return str(it->first);
    if (it->field == UI_F_MENU_ITEM && item >= 0 && item < it->count) return str(it->first + (unsigned)item);
  }
  return "";
}

// ---------------------------------------------------------------------------------------
// game-facing API (stage 7): thin layer over the fields/tags above
void uiSetCoins(int n) { fieldv[UI_F_COINS] = n < 0 ? 0 : n; }
void uiSetHitAndRun(float heat, int pursuit, float elapsed) {
  if (!isfinite(heat) || heat < 0.f) heat = 0.f; if (heat > 1.f) heat = 1.f;
  if (!isfinite(elapsed) || elapsed < 0.f) elapsed = 0.f;
  float dt = elapsed - hnrLastT; hnrLastT = elapsed;
  if (!(dt > 0.f)) dt = 0.f; if (dt > .1f) dt = .1f;
  if (hnrScript) { if (hnrScriptT0 < 0.f) hnrScriptT0 = elapsed; heat = uiHnrScriptHeat(elapsed - hnrScriptT0, &pursuit); }
  hnrPursuit = pursuit != 0; hnrTarget = heat;
  if (animOff & UI_ANIM_OLDHNR) {   // прежний код (отрицательный контроль гейта): шкала скачком, мигалка 2/1
    hnrShown = heat;
    fieldv[UI_F_HNR] = (int)(heat * 1000.f + .5f);
    fieldv[UI_F_HNR_LIGHT] = pursuit ? (sinf(elapsed * 10.f) > 0.f ? 2 : 1) : (heat > .78f ? 1 : 2);
    return;
  }
  // кольцо догоняет шкалу ровно, как прогресс-бар (ui_anim.h uiMeterFollow); мигалка и цвет кольца — от одних часов
  // elapsed (uiSirenFrame): кадры по данным hitnrun0 красный горит, 1 синий, 2 обе погашены; покой 2 сверен с PCSX2 27.09
  hnrShown = uiMeterFollow(hnrShown, heat, dt);
  fieldv[UI_F_HNR] = (int)(hnrShown * 1000.f + .5f);
  fieldv[UI_F_HNR_LIGHT] = uiSirenFrame(pursuit, elapsed);   // правка 241: мигалка горит только с полицией (pursuit = полиция в игре)
}
void uiSetDamage(float d) {
  if (d < 0.f) d = 0.f; if (d > 1.f) d = 1.f;
  fieldv[UI_F_DAMAGE] = (int)(d * 1000.f + .5f);
  uiShow(UI_SCREEN_HUD, UI_T_DAMAGE, fieldv[UI_F_DAMAGE] > 0);          // mission objective progress only
}
void uiSetSpeed(float kmh) { fieldv[UI_F_SPEED] = (int)(fabsf(kmh) + .5f); fieldv[UI_F_SPEED_UNIT] = 0; }
void uiSetRadar(float x, float z, float heading) { uiRadarSetPlayer(x, z, heading); }

static void msgTags(int on) { uiShow(UI_SCREEN_HUD, UI_T_OBJECTIVE, on); uiShow(UI_SCREEN_PAUSE, UI_T_OBJECTIVE, on); }
void uiMsgHardBreaks(int on) { msgHardBr = on != 0; }   // правка 267: служебно «flag msghardbr 1» — прежние жёсткие переносы
void uiMessage(const char *text, int icon, float seconds) {
  if (!text || !*text) { if (msgText[0]) memcpy(msgLast, msgText, sizeof msgLast); msgText[0] = 0; msgUntil = 0; msgTags(0); return; }
  // Правка Роба 267 (04.10: «в пузырях снизу текст обрублен; когда взрывается машина, текст громадный, еле вмещается — распределять
  // красиво, как TALK TO MARGE»): тексты библии несут жёсткие переносы под рамку подсказки Барта («VEHICLE DESTROYED!\nFIND A
  // WRENCH,\n…» — 4 строки, упор в край). В нижнем пузыре текст переносится по ширине самого пузыря (uiWrapPara, без висячего
  // слова) и стоит по центру — жёсткие переносы и «\\» становятся пробелами. UI_MSG_HARDBR=1 — прежнее (красный контроль)
  char flat[sizeof msgText]; int fl_ = 0;
  for (const char *p = text; *p && fl_ < (int)sizeof flat - 1; p++) {
    char c = (*p == '\n' || *p == '\\') && !msgHardBr ? ' ' : *p;
    if (c == ' ' && (fl_ == 0 || flat[fl_ - 1] == ' ')) continue;   // без двойных пробелов и пробела в начале
    flat[fl_++] = c;
  }
  while (fl_ > 0 && flat[fl_ - 1] == ' ') fl_--;
  flat[fl_] = 0; text = flat;
  if (strncmp(msgText, text, sizeof msgText - 1)) msgNew = 1;   // новый текст — рамка всплывает заново (и «чпок», ui_sfx.c)
  strncpy(msgText, text, sizeof msgText - 1); msgText[sizeof msgText - 1] = 0;
  memcpy(msgLast, msgText, sizeof msgLast);
  fieldv[UI_F_OBJICON] = icon;
  msgUntil = seconds > 0.f ? sceKernelGetSystemTimeLow() + (unsigned)(seconds * 1e6f) : 0;
  if (seconds > 0.f && !msgUntil) msgUntil = 1;
  msgTags(1);
}
// №958б: диагностика (flag msnhud): что держит угол — поле значка, есть ли картинка извне, текст сообщения и последний погасший
const void *uiObjPix(void) { return oiPix; }   // №993: какие пиксели сейчас в значке угла (сверка с пакетом, msnartIconPixOk)
void uiObjDebug(int *icon, int *hasPix, const char **txt, const char **last) { *icon = fieldv[UI_F_OBJICON]; *hasPix = oiPix != 0; *txt = msgText; *last = msgLast; }
void uiObjective(int index, int icon) {
  msgText[0] = 0; msgUntil = 0;
  fieldv[UI_F_OBJECTIVE] = index; fieldv[UI_F_OBJICON] = icon;
  msgTags(index >= 0);
}
static void msgExpire(void) {
  if (msgUntil && (int)(sceKernelGetSystemTimeLow() - msgUntil) >= 0) { memcpy(msgLast, msgText, sizeof msgLast); msgText[0] = 0; msgUntil = 0; msgTags(0); }
}

void uiMenuEnable(int menu, int item, int on) {
  if ((unsigned)menu >= UI_SCREEN_COUNT || (unsigned)item >= 32) return;
  if (on) menuOff[menu] &= ~(1u << item); else menuOff[menu] |= 1u << item;
}
// правка Роба 137: строка паузы скрыта целиком (RESET CAR — в PauseSunday.pag оригинала его нет; служебный режим оставляет).
// Коды UI_P_* (= номер строки пакета) не меняются; строки ниже скрытой поднимаются на её шаг, блок опускается на полшага —
// центр списка в рамке тот же
void uiMenuHide(int menu, int item, int on) {
  if ((unsigned)menu >= UI_SCREEN_MAX || (unsigned)item >= 32) return;
  if (on) menuHid[menu] |= 1u << item; else menuHid[menu] &= ~(1u << item);
}
int uiMenuHidden(int menu, int item) { return (unsigned)menu < UI_SCREEN_MAX && (unsigned)item < 32 && (menuHid[menu] >> item & 1); }
static int menuRowY(const UiScreen *sc, int row) {   // y строки меню row (текст F_MENU), -1 — нет
  for (unsigned i = 0; i < sc->count; i++) { const UiItem *it = &items[sc->first + i];
    if (it->type == T_TEXT && (it->flags & F_MENU) && it->menu == row) return it->y; }
  return -1;
}
static int hideDy(const UiScreen *sc, int sidx, const UiItem *it) {   // сдвиг по y элемента строки меню из-за скрытых строк
  if ((unsigned)sidx >= UI_SCREEN_MAX || !menuHid[sidx] || !(it->flags & (F_MENU | F_SELONLY)) || it->menu >= 32) return 0;
  int dy = 0;
  for (int h = 0; h < 32; h++) {
    if (!(menuHid[sidx] >> h & 1)) continue;
    int yh = menuRowY(sc, h), yn = 0x7fff, yp = -0x7fff; if (yh < 0) continue;
    for (unsigned i = 0; i < sc->count; i++) { const UiItem *q = &items[sc->first + i];
      if (q->type != T_TEXT || !(q->flags & F_MENU) || q->menu == h) continue;
      if (q->y > yh && q->y < yn) yn = q->y; if (q->y < yh && q->y > yp) yp = q->y; }
    int step = yn != 0x7fff ? yn - yh : yp != -0x7fff ? yh - yp : 0;
    if (it->y > yh) dy -= step;
    dy += step / 2;
  }
  return dy;
}
// Правка Роба 202 (03.10): «чтобы можно было клацать вверх-низ, зажимая стрелки — не на одну, а до предела, до верха, до низа».
// №706 (17.10): то же для ◄ ► — в будке, гардеробе, лавке, пунктах-вариантах, ползунках, выборе уровня, бонус-меню листалась одна машина.
// Удержание стрелки — автоповтор: пауза UI_REP_DELAY (0,4 с), дальше шаг UI_REP_EVERY (0,12 с); повторяются ТОЛЬКО четыре стрелки
// (✕ и прочие — никогда: покупка и выбор не повторяются). Шаг повтора у края списка ↑/↓ встаёт (uiPadRepeated), новое нажатие у края —
// по кругу, как было; ◄ ► на замкнутом списке крутят по кругу. Время — часы системы, не кадры (ровно на 30 и 60 к/с).
// Если функцию не звали больше UI_REP_GAP (1 с) — экран сменился надолго, зажатая с прошлого экрана стрелка не повторяется (тормоз кадра короче секунды повтор не рвёт).
// REPEAT706_OLD=1 / файл repeat706old рядом с EBOOT (uiPadRepeatOld) — прежнее: только ↑/↓, пауза 0,35 с, шаг 0,09 с
#ifndef REPEAT706_OLD
#define REPEAT706_OLD 0
#endif
#ifndef UI_REP_DELAY
#define UI_REP_DELAY 400000u   // мкс
#endif
#ifndef UI_REP_EVERY
#define UI_REP_EVERY 120000u
#endif
#define UI_REP_GAP 1000000u
// TEST34 на PSP: строка Cartoon «не переключалась» — включение мультика держит кадр ~0,5 с (палитры, таблица 90-х), зажатая ещё ► на следующем
// кадре уже старше паузы повтора и срабатывает второй раз: Off → On → Off. Кадр дольше UI_REP_HITCH заново заводит паузу повтора (тормоз не считается
// удержанием). REP_HITCH_OLD=1 — прежнее
#ifndef REP_HITCH_OLD
#define REP_HITCH_OLD 0
#endif
#define UI_REP_HITCH 150000u
int uiPadRepeated;
static int padRepOld = REPEAT706_OLD;
void uiPadRepeatOld(int on) { padRepOld = on; }
// №1057 (Роб 27.10: «зажать L и R, и оно будет постепенно кликать вправо-влево, докликать до своей тачки»): в будке (42 машины) повторяются и L R наравне с ◄ ►,
// пауза короче (0,30 с) и шаг 0,11 с — до своей машины доходит за пару секунд, звук листания ещё не трещит. Включает вызывающий на кадр (uiPadRepeatLR(1) перед uiPadRepeat);
// BOOTHLR1057_OLD=1 / файл boothlr1057old / flag boothlr1057 0 — прежнее (только ◄ ► с общими паузой 0,4 и шагом 0,12)
#ifndef UI_REP_DELAY_LR
#define UI_REP_DELAY_LR 300000u
#endif
#ifndef UI_REP_EVERY_LR
#define UI_REP_EVERY_LR 110000u
#endif
static int padLR;
void uiPadRepeatLR(int on) { padLR = on; }
unsigned uiPadRepeat(unsigned buttons, unsigned hit) {
  static unsigned key, tNext, tLast;
  int lr = padLR && !padRepOld;
  unsigned dirs = padRepOld ? (PSP_CTRL_UP | PSP_CTRL_DOWN) : (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT | (lr ? PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER : 0u)),
           delay = padRepOld ? 350000u : lr ? UI_REP_DELAY_LR : UI_REP_DELAY, every = padRepOld ? 90000u : lr ? UI_REP_EVERY_LR : UI_REP_EVERY, now = sceKernelGetSystemTimeLow();
  uiPadRepeated = 0;
  if (!padRepOld && (unsigned)(now - tLast) > UI_REP_GAP) key = 0;
  if (!padRepOld && !REP_HITCH_OLD && key && (unsigned)(now - tLast) > UI_REP_HITCH) tNext = now + delay;   // долгий кадр — пауза повтора заново
  tLast = now;
  if (hit & dirs) { key = hit & dirs; if (!padRepOld) key &= -key; tNext = now + delay; return hit; }   // одна стрелка: младший бит
  if (!key || !(buttons & key)) { key = 0; return hit; }
  if ((int)(now - tNext) >= 0) { tNext = now + every; uiPadRepeated = 1; return hit | key; }
  return hit;
}
static int menuStep(int menu, int from, int dir) {
  int n = uiMenuCount(menu);
  if (n <= 0) return 0;
  unsigned skip = menuOff[menu] | ((unsigned)menu < UI_SCREEN_MAX ? menuHid[menu] : 0);
  for (int k = 1; k <= n; k++) {
    int j = from + dir * k, i = (j % n + n) % n;
    if (uiPadRepeated && (j < 0 || j >= n)) return from;   // правка 202: повтор у края встаёт
    if (!(skip >> i & 1)) return i;
  }
  return from;
}
static int menuStepHid(int menu, int from, int dir) {   // карусель главного меню: шаг по кругу, перепрыгивая только скрытые пункты (серые остаются выбираемыми — × на них даёт locked_out)
  int n = uiMenuCount(menu); unsigned hid = (unsigned)menu < UI_SCREEN_MAX ? menuHid[menu] : 0;
  for (int k = 1; k <= n; k++) { int i = ((from + dir * k) % n + n) % n; if (!(hid >> i & 1)) return i; }
  return from;
}
int uiMenuEnabled(int menu, int item) { return (unsigned)menu < UI_SCREEN_COUNT && (unsigned)item < 32 && !(menuOff[menu] >> item & 1); }
int uiMenuInput(int menu, int *sel, unsigned hit) {   // 12а: главное меню — карусель оригинала (MainMenu.pag): ◄ ► (и ↑ ↓) по кругу
  if ((unsigned)menu >= UI_SCREEN_COUNT || !sel) return -1;   // по всем пунктам: в гостиной светится предмет каждого (Роб 28.09);
  int n = uiMenuCount(menu), was = *sel;                      // неготовые пункты приглушены, × на них не срабатывает
#ifndef MENU_RESUME_DEFAULT_OLD
#define MENU_RESUME_DEFAULT_OLD 0   // 1 — прежнее: главное меню на NEW GAME, RESUME GAME на чистой карте серый и не пропускается
#endif
  if (n > 0 && (hit & (PSP_CTRL_LEFT | PSP_CTRL_UP))) *sel = MENU_RESUME_DEFAULT_OLD ? (*sel + n - 1) % n : menuStepHid(menu, *sel, -1);   // скрытые пункты (RESUME GAME без сохранения) курсор обходит
  if (n > 0 && (hit & (PSP_CTRL_RIGHT | PSP_CTRL_DOWN))) *sel = MENU_RESUME_DEFAULT_OLD ? (*sel + 1) % n : menuStepHid(menu, *sel, 1);
  if (*sel != was) pauseEv(UI_EV_SCROLL, 1);                  // звуки меню, как в паузе (frontend.spt: scroll / accept / locked_out)
  if (!(hit & PSP_CTRL_CROSS) || n <= 0) return -1;
  if (!uiMenuEnabled(menu, *sel)) { pauseEv(UI_EV_LOCKED, 1); return -1; }
  pauseEv(UI_EV_ACCEPT, 1); return *sel;
}
int uiMenuOrder(int menu, int *out, int max) {   // 12а: сверка порядка с данными оригинала (tools/menu_check.py)
  if (!H || (unsigned)menu >= H->nscreen || !out) return 0;
  const UiScreen *sc = &screens[menu]; int n = 0, ys[32];
  for (unsigned i = 0; i < sc->count && n < max && n < 32; i++) {
    const UiItem *it = &items[sc->first + i];
    if (it->type != T_TEXT || !(it->flags & F_MENU)) continue;
    int j = n++; while (j > 0 && ys[j - 1] > it->y) { ys[j] = ys[j - 1]; out[j] = out[j - 1]; j--; }
    ys[j] = it->y; out[j] = it->menu;
  }
  return n;
}
// звуки паузы (frontend.spt: pause_on / pause_off / scroll / accept / locked_out) — через uiPopHook, событие UI_EV_*
static void pauseEv(int ev, int on) { if (popHook) popHook(ev, on); }
void uiPopHookFire(int ev) { pauseEv(ev, 1); }
void uiPauseMission(int on) {   // пауза миссии (PauseMission.scr) — если экран есть в пакете
  if (paused) return;
  pauseScr = on && H && H->nscreen > UI_SCREEN_PAUSEMSN && screens[UI_SCREEN_PAUSEMSN].nmenu ? UI_SCREEN_PAUSEMSN : UI_SCREEN_PAUSE;
}
int  uiPauseIsMission(void) { return pauseScr == UI_SCREEN_PAUSEMSN; }
static int pauseCode(int sel) {   // номер строки открытой паузы -> UI_P_*
  if (pauseScr != UI_SCREEN_PAUSEMSN) return sel;
  return sel >= 0 && sel < (int)sizeof PM_MAP ? PM_MAP[sel] : UI_P_NONE;
}
// №631 (Роб; запись PS2 L1, 50 кадр/с, Sonnet-PS2 knowledge/facts/ps2-pause-menu-anim.yaml): основа меню паузы со всеми пунктами растёт из центра масштабом с набором
// непрозрачности — ширина рамки 217, 261, 286, 312, 339, 363, 416 px (кадры 30–36 из 416 = 0,52…1) за ~0,16 с (кадры 28–36); закрытие вдвое быстрее: 416, 334, 309, пропала —
// 3 кадра (0,06 с). Не повторено: гашение HUD за 3 кадра и пустые кадры до появления рамки (у нас HUD в паузе не рисуется), затемнение мира на 7 %, возврат радара через 0,4 с.
// PAUSE_ANIM_OLD=1 — пауза появляется и исчезает скачком, как раньше
static void pauseCloseStart(void) { if (!PAUSE_ANIM_OLD) { pauseClosing = 1; pauseCloseUs = sceKernelGetSystemTimeLow(); } }
static int pauseAnimPose(float *sc, float *al) {
  if (PAUSE_ANIM_OLD) return 0;
  if (paused) return uiPausePose(0, pauseClk(pauseT0us), sc, al);
  if (pauseClosing) return uiPausePose(1, pauseClk(pauseCloseUs), sc, al);
  return 0;
}
// затемнение мира на 7 % (запись PS2 кадры 28–34, без отдельной плашки на весь экран): нарастает за 0,12 с, держится, при закрытии уходит за 0,06 с
static float pauseDimA(void) {
  if (PAUSE_ANIM_OLD || scrMode) return 0.f;
  if (paused) { float t = pauseClk(pauseT0us); return .07f * (t >= .12f ? 1.f : t / .12f); }
  if (pauseClosing) { float t = pauseClk(pauseCloseUs); return t >= UI_PAUSE_CLOSE_S ? 0.f : .07f * (1.f - t / UI_PAUSE_CLOSE_S); }
  return 0.f;
}
// радар и значок цели возвращаются резко через 0,4 с после нажатия (запись PS2 кадр 180): до этого HUD не рисуется. 1 — ещё прячем
static int pauseHudHidden(void) {
  if (PAUSE_ANIM_OLD || !pauseClosing) return 0;
  if (pauseClk(pauseCloseUs) < .4f) return 1;
  pauseClosing = 0; return 0;
}
void uiPause(int on) {
  int was = paused; if (on && !paused) { pauseT0us = sceKernelGetSystemTimeLow(); pauseClosing = 0; }
  paused = on ? 1 : 0; if (paused) pauseSel = UI_P_CONTINUE;   // CONTINUE — первая строка у обеих пауз
  if (paused && uiMenuHidden(pauseScr, pauseSel)) pauseSel = menuStep(pauseScr, pauseSel, 1);   // правка 137: первая видимая
  if (paused != was) pauseEv(UI_EV_PAUSE, paused);
}
int  uiPaused(void) { return paused; }
int  uiPauseSelected(void) { return pauseCode(pauseSel); }
int  uiPauseInput(unsigned hit) {
  if (!paused) return UI_P_NONE;
  // №806 (Роб 18.10: «выход из меню и ○, и △ — стрелка назад это треугольник»): △ тоже закрывает паузу. PAUSE_TRI_BACK_OLD=1 — только START / ○
#ifndef PAUSE_TRI_BACK_OLD
#define PAUSE_TRI_BACK_OLD 0
#endif
  if (hit & (PSP_CTRL_START | PSP_CTRL_CIRCLE | (PAUSE_TRI_BACK_OLD ? 0u : PSP_CTRL_TRIANGLE))) { paused = 0; pauseCloseStart(); pauseEv(UI_EV_PAUSE, 0); return UI_P_CONTINUE; }   // START / O / △ = back to the game
  int was = pauseSel;
  if (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN)) uiLogf("PAUSEKEY hit=%x sel=%d n=%d off=%x hid=%x rep=%d\n", hit, pauseSel, uiMenuCount(pauseScr), menuOff[pauseScr],
                                                   (unsigned)pauseScr < UI_SCREEN_MAX ? menuHid[pauseScr] : 0, uiPadRepeated);   // служебно: разбор 238
  if (hit & PSP_CTRL_UP) pauseSel = menuStep(pauseScr, pauseSel, -1);
  if (hit & PSP_CTRL_DOWN) pauseSel = menuStep(pauseScr, pauseSel, 1);
  if (pauseSel != was) pauseEv(UI_EV_SCROLL, 1);
  if (hit & PSP_CTRL_CROSS) {
    if (menuOff[pauseScr] >> pauseSel & 1) { pauseEv(UI_EV_LOCKED, 1); return UI_P_NONE; }
    int c = pauseCode(pauseSel);
    if (c == UI_P_CONTINUE || c == UI_P_RESET_CAR || c == UI_P_RESTART_MISSION || c == UI_P_ABORT_MISSION) { paused = 0; pauseCloseStart(); pauseEv(UI_EV_PAUSE, 0); }
    else pauseEv(UI_EV_ACCEPT, 1);
    return c;
  }
  return UI_P_NONE;
}

// Наряд 11.10 (TEST13: x_ui 70–390 мс отдельными секундами на открытии ириса, kw ui до 174 тыс. слов): кадр uiDraw дороже UI_PROF_US — одна строка UIPROF в журнал (не чаще раза в
// секунду): время, счётчики вызовов/привязок/вершин/элементов, состояние задания и экрана; по ней следующий тур увидит, чем именно занят интерфейс. UI_PROF_OLD=1 — без замера
#ifndef UI_PROF_OLD
#define UI_PROF_OLD 0
#endif
#ifndef UI_PROF_US
#define UI_PROF_US 12000u
#endif
static void uiDrawInner(void) {
  msgExpire();
  if (paused) { drawScreen(pauseScr, pauseSel); return; }   // the pause screen has its own coin count
  if (pauseHudHidden()) { float ps_, pa_; if (pauseAnimPose(&ps_, &pa_)) drawScreen(pauseScr, pauseSel); return; }   // №631: закрытие — 3 кадра рамка сжимается, HUD вернётся через 0,4 с
  drawScreen(UI_SCREEN_HUD, -1);
}
void uiDraw(void) {
  if (traceBits & UI_TRACE_POP) { static unsigned p_[2]; unsigned d0 = uiWrapN1040[0] - p_[0], d1 = uiWrapN1040[1] - p_[1]; p_[0] = uiWrapN1040[0]; p_[1] = uiWrapN1040[1]; if (d0 || d1 || bubAge < 0.6f) uiLogf("WRAP1040 age=%.3f calls=%u calls0=%u\n", bubAge, d0, d1); }   // №1040 служебно: сколько раз за кадр считался перенос
  if (UI_PROF_OLD) { uiDrawInner(); return; }
  unsigned t0 = sceKernelGetSystemTimeLow(); uiDrawInner(); unsigned us = sceKernelGetSystemTimeLow() - t0;
  if ((uiDrawLog1040 || (traceBits & UI_TRACE_POP)) && bubAge < 0.6f) uiLogf("UIDRAW1040 age=%.3f us=%u draws=%d verts=%d\n", bubAge, us, st.draws, st.verts);   // №1040 служебно: цена кадра интерфейса во время появления
  static unsigned lastLog;
  if (us > UI_PROF_US && t0 - lastLog > 1000000u) {
    lastLog = t0;
    uiLogf("UIPROF us=%u paused=%d draws=%d binds=%d verts=%d items=%d pix=%d msg=%d icon=%d objcollapse=%d anim=0x%x\n", us, paused, st.draws, st.binds, st.verts, st.items, st.pixels,
           (int)strlen(msgText), fieldv[UI_F_OBJICON], objCollapse, (unsigned)animOff);
  }
}
