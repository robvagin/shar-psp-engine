// Следы шин (правка Роба 249), см. skid.h. Источник правил — исполняемый файл PS2 SLES_518.97 (диск Роба
// hit-and-run-web/source/disc), разбор psp-objdump 03.10; поля машины оригинала: +0x1a4 газ, +0x1b0 тормоз/назад, +0x1c0 ручник
// (0/1), +0x118 ось «вперёд», +0x130 «вбок», +0x13c скорость, +0x14c доля TopSpeed (не больше 1), +0x17c режим (1 занос, 2 занос
// на ручнике), +0x28c пробуксовка, +0x288 уровень заноса. Адреса — функции оригинала:
//   0x282208  уровень заноса: занос — |скорость·вбок|, не меньше ручник×0,5; тормоз > 0,05 на ходу вперёд при доле TopSpeed 0,07..0,5 —
//             1,0; ручник > 0,85 при доле > 0,07 — не меньше ручник×0,5; не меньше пробуксовки; ненулевой — не меньше 0,5
//   0x280f8c  пробуксовка с места: газ > 0,99 при доле < BurnoutRange — 1 − 0,5·доля/BurnoutRange
//   0x281684  задний ход: машина катится назад медленнее 50 км/ч — пробуксовка = тормоз (тормоз у оригинала и «назад»)
//   0x288378  колёса: 4 (2 у honor_v/rocke_v, ни одного у парящих hbike_v/frink_v/ship/witchcar/mono_v); при пробуксовке — только
//             ведущие; колесо без опоры след не тянет; машина на «рельсах» трафика (+0x184 = 1) — тоже
//   0x2052f0  SkidMarkGenerator: альфа участка = уровень×0,5×255; поворот скорости (скалярное < 0,7) или смена поверхности —
//             новый след; шейдер по поверхности 0x205218: трава (1) grass_skidmark_shader, грунт (7) dirt_skidmark_shader, иначе skid_m
//   0x204cb8  полуширина следа 0,25 м на колесо
//   0x2061f0  новый участок не ближе √0,05 = 0,224 м; точка поднята на 0,1 м по нормали земли; v текстуры +2 на метр (плитка 0,5 м)
//   0x206ab8 / 0x206b20  первый участок прозрачный; при конце следа последний участок прозрачный, к нему спад по последнему метру
//   0x205e60  цвет вершины (a, a, a, a): текстура темнеет вместе с прозрачностью; глубину следы не пишут
//   0x205830  угасание за 5 с + 1 с только при флаге менеджера, который ставит одна SuperSprint (0x2a21c8); в обычной игре следы
//             живут, пока их место в пуле не понадобится (свободный и невидимый — 0x205928). Запись PS2: следы торможения на стоянке
//             Kwik-E-Mart (2:48–2:51) на месте и через 66 с (3:57)
// Правила порта (данных нет): кольцо SKID_QUADS квадов вместо пула 30 следов (затирается старейший квад); участок раз в 1 м
// (у оригинала плитка 0,5 м — текстура симметрична, повтор = зеркало); подъём 0,05 м по нормали (как тень машины carvis.c);
// поверхность всегда «дорога»: типов поверхности в COL1 нет (трава и грунт — skidg/skidd — следующим шагом)
#include "gevb.h"
#ifndef GEVB_SKID_OLD
#define GEVB_SKID_OLD 0   // -DGEVB_SKID_OLD=1 — индексы следов шин в список GE, как раньше
#endif
#include "skid.h"
#include "sverka_shared.h"
#include <math.h>
#include <string.h>
#include <strings.h>
#ifndef SKID_HOST
#include <pspgu.h>
#include <pspkernel.h>
#include "fx.h"
#endif

// Правка Роба 270 (04.10: «следы — очень широкие колёса; в оригинале тоньше, контрастнее — перепроверь с PS2»): запись PS2 2:50
// (стоянка Kwik-E-Mart, два следа торможения на одной глубине): ширина следа / расстояние между следами = 30/195 px = 0,15; у порта
// с полушириной 0,25 — 20/74 = 0,27. Число 0x204cb8 (0,25 м) — полная ширина следа, не половина: полуширина 0,125 м даёт 0,14.
// «flag skidwide» — прежние 0,25 (красный контроль tests/skid/width.py)
// сверка g01-cars, P01-27: полуширина следа = радиус колеса × 0,4 (famil_v 0,15 м, у каждой машины своя), подъём над землёй 0,1 м; участок не короче 0,224 м и текстура 0,5 м на период —
// уже так. Было: полуширина 0,125 м у всех, подъём 0,05 м. -DSV_SKID_W_OLD=1
#ifndef SV_SKID_W_OLD
#define SV_SKID_W_OLD 0
#endif
static float curHalfW = .125f;
#define SKID_HALF_W (skidWide ? .25f : curHalfW)   // 0x204cb8 / 2
static int skidWide;
void skidWideOld(int on) { skidWide = on != 0; }
#define SKID_LIFT (SV_SKID_W_OLD ? .05f : .1f)        // правило порта (оригинал 0,1 от низа колеса, у порта — от земли COL1, как тень)
#define SKID_STEP 1.f         // правило порта: участок раз в метр (оригинал 0,5)
#define SKID_MIN .2236068f    // 0x206340: |d|² < 0,05
#define SKID_VPM 2.f          // 0x206414: v += 2 · путь
#define SKID_TURN .7f         // 0x205414
#define SKID_FADE 1.f         // 0x206b20: спад альфы к концу по последнему метру
#define SKID_JUMP 6.f         // правило порта: точка дальше от последнего участка — машину переставили, след заново
#define SKID_DRAW_DIST 90.f   // правило порта: дальше от глаза не рисуем (город — drawDist)
#define SKID_HIST 8
#ifndef SV_SKID_SURF_OLD
#define SV_SKID_SURF_OLD 0
#endif

typedef struct { float u, v; unsigned c; float x, y, z; } SV;   // TEXTURE_32BITF | COLOR_8888 | VERTEX_32BITF
static SV vb[SKID_QUADS * 4] __attribute__((aligned(16)));
static unsigned short qown[SKID_QUADS];   // метка следа-хозяина, 0 — пусто
static unsigned char qlive[SKID_QUADS];   // квад тянется за колесом (его не затираем)
static float qlen[SKID_QUADS];
static int head, used, dirty, made, skipped, drawnLast, curSlot, madeAi;
static float lvlLast, usLast, usCar;
// Правка Роба 500 (машины в кадре, «следы»): машины миссии с ИИ дальше SKID_DRAW_DIST + 40 м от глаза (прошлый кадр) следов не оставляют — раньше каждый кадр
// считали уровень скольжения и по лучу земли на каждое из 4 колёс (colGroundMat), хотя квадраты дальше 90 м не рисуются. След начинается заново, когда машина
// ближе; игрок (слот 0) всегда. SKID_FAR_OLD=1 — прежнее
#ifndef SKID_FAR_OLD
#define SKID_FAR_OLD 0
#endif
static float skEye[3]; static int skEyeOk;
static int levelOn = 1, svcOn = 1, skidTex = -2;   // skidTex: номер skid.bmp в fx.shr (tools/fx_pack.py SKID_TEX); -2 — не искали, -1 — нет в пакете
static int (*geRoomFn)(unsigned);

typedef struct {
  int on, q, nh, hist[SKID_HIST];
  unsigned short tag;
  float s[3], sl[3], sr[3], sv;   // последний закреплённый участок: середина, края, v текстуры
  unsigned char sa;               // его альфа
  float d[3];                     // направление скорости машины на прошлом шаге (0x205414)
  int surf;                       // сверка g01-cars 9.2: вид поверхности следа (0 дорога, 1 трава, 2 грунт); смена — новый след
} Trail;
static Trail tr[SKID_CARS][4];
static unsigned short tagCtr;

static inline float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
// Правка Роба 424 (09.10, голосом: «следы шин на повороте жёлтые/коричневые — не оставлять контрастные полосы»): след — ровный
// нейтральный цвет текстуры диска skid.bmp (art/l01_fx.p3d: серый 27–46, среднее 32,32,32; альфа текстуры 207–227, среднее 221),
// без текстуры: квады рисуются цветом вершин, альфа × 221/255 — как текстура × вершина у оригинала (0x205e60), а путь через
// текстуру и её палитру, где мог появиться оттенок, не участвует. Травяной и грунтовый skidg/skidd (зелёно-жёлтый, коричневый) у порта
// не используются (типов поверхности в COL1 нет; слово Роба 424 — без контрастных). SKID_TINT_OLD=1 — прежнее (текстура × (a,a,a,a))
#ifndef SKID_TINT_OLD
#define SKID_TINT_OLD 0
#endif
#define SKID_RGB 0x202020u     // skid.bmp: среднее 32, 32, 32
#define SKID_TEXA 221u         // skid.bmp: средняя альфа
static inline unsigned col(unsigned char a) {   // 0x205e60: (a, a, a, a) × текстура
  return SKID_TINT_OLD ? 0x01010101u * a : ((unsigned)a * SKID_TEXA / 255u) << 24 | SKID_RGB; }
static inline unsigned char alphaRaw(unsigned c) { unsigned a = c >> 24; return (unsigned char)(SKID_TINT_OLD ? a : (a * 255u + SKID_TEXA - 1u) / SKID_TEXA > 255u ? 255u : (a * 255u + SKID_TEXA - 1u) / SKID_TEXA); }

void skidSetGeRoom(int (*room)(unsigned)) { geRoomFn = room; }
void skidEnable(int on) { svcOn = on; }

void skidClear(int on) {
  memset(vb, 0, sizeof vb); memset(qown, 0, sizeof qown); memset(qlive, 0, sizeof qlive); memset(qlen, 0, sizeof qlen);
  memset(tr, 0, sizeof tr);
  for (int s = 0; s < SKID_CARS; s++) for (int i = 0; i < 4; i++) tr[s][i].q = -1;
  head = used = made = skipped = drawnLast = madeAi = 0; dirty = 1; levelOn = on; lvlLast = 0.f; skidTex = -2;
}

#ifndef SKID_MARKS_ALL_OLD
#define SKID_MARKS_ALL_OLD 0
#endif
void skidBeginLevel(int level) {
  skidClear(!SKID_MARKS_ALL_OLD || level == 1);
}

// оси тела машины в мире (как quatM в car.c): L — влево, U — вверх, F — вперёд
static void axes(const float *q, float L[3], float U[3], float F[3]) {
  float w = q[0], x = q[1], y = q[2], z = q[3];
  L[0] = 1 - 2 * (y * y + z * z); L[1] = 2 * (x * y + w * z); L[2] = 2 * (x * z - w * y);
  U[0] = 2 * (x * y - w * z); U[1] = 1 - 2 * (x * x + z * z); U[2] = 2 * (y * z + w * x);
  F[0] = 2 * (x * z + w * y); F[1] = 2 * (y * z - w * x); F[2] = 1 - 2 * (x * x + y * y);
}

// Правка Роба 497 (10.10: следы шин у snake_v и вообще у всех машин — на повороте с заносом, при торможении, на ручнике): правило оригинала привязано к ДОЛЕ TopSpeed
// (тормоз 0,07–0,5, ручник > 0,07) и к режиму заноса c->slip, который у цепких машин (TireGrip 2,9 у snake_v, 3,05 у apu_v) почти не включается. Параметры
// snake_v по диску: TopSpeed 160 км/ч, TireGrip 2,9, EBrakeEffect 0,4, SuspensionLimit 0,6 — не выходят за ряд соседей, особого условия не нашёл. ПРАВИЛО ПОРТА
// (независимо от TopSpeed, по реальной скорости): тормоз > 0,05 на ходу вперёд при 3 м/с ≤ скорость ≤ max(0,5·TopSpeed, 22 м/с) — след; ручник > 0,85 при скорости
// ≥ 1,5 м/с — не меньше 0,5; боковая скорость |v·вбок| ≥ 2,5 м/с при скорости ≥ 6 м/с — след по её доле (занос без включённого режима). SKID_ALL_CARS_OLD=1 — как было
#ifndef SKID_ALL_CARS_OLD
#define SKID_ALL_CARS_OLD 0
#endif

#ifndef SKID_OLD
#define SKID_OLD 0
#endif
static int skidOldOn = SKID_OLD;
void skidOld(int on) { skidOldOn = on != 0; }

float skidLevel(const Car *c, const CarInput *in, float *burn) {
  const CarParams *P = &c->p;
  float sp = sqrtf(c->vel[0] * c->vel[0] + c->vel[1] * c->vel[1] + c->vel[2] * c->vel[2]);
  float ratio = P->topSpeed > 0.f ? fminf(1.f, sp / P->topSpeed) : 0.f;   // +0x14c: скорость / TopSpeed, не больше 1 (0x2806a8)
  float thr = 0.f, brk = 0.f, eb = 0.f;
  if (in) { thr = fminf(1.f, fmaxf(0.f, in->throttle)); brk = fminf(1.f, fmaxf(0.f, in->brake)); eb = fminf(1.f, fmaxf(0.f, in->handbrake)); }
  float L[3], U[3], F[3]; axes(c->q, L, U, F);
  float fwd = c->vel[0] * F[0] + c->vel[1] * F[1] + c->vel[2] * F[2];
  float b = (ratio < P->burnoutRange && thr > .99f && P->burnoutRange > 0.f) ? 1.f - .5f * ratio / P->burnoutRange : 0.f;   // 0x280f8c
  if (fwd < 0.f && sp * 3.6f < 50.f) b = brk;   // 0x281684: катится назад медленнее 50 км/ч — пробуксовка = тормоз (и 0 без него)
#if !SKID_OLD
  if (!skidOldOn) {
    if (eb > 0.1f) b = 0.f;   // №562: ручник держит колёса — пробуксовки нет
    if (sp < 0.3f && brk > 0.f) b = 0.f;   // №562: стоя на месте на тормозе — не задний ход, пробуксовки нет
  }
#endif
  if (burn) *burn = b;
  float lvl = 0.f;
  if (sp > 1e-3f) {
    float vd[3] = {c->vel[0] / sp, c->vel[1] / sp, c->vel[2] / sp};
    if (c->slip == 1 || c->slip == 2) { lvl = fabsf(dot3(vd, L)); if (lvl < eb * .5f) lvl = eb * .5f; }   // 0x2822c0
    if (brk > .05f && ratio > .07f && fwd > 0.f && ratio < .5f) lvl = 1.f;                                 // 0x2822f4: тормоз на ходу
    if (eb > .85f && ratio > .07f && lvl < eb * .5f) lvl = eb * .5f;                                       // 0x282388: ручник
    if (!SKID_ALL_CARS_OLD) {   // правка 497: по реальной скорости, а не по доле TopSpeed
      if (brk > .05f && fwd > 0.f && sp >= 3.f && sp <= fmaxf(.5f * P->topSpeed, 22.f)) lvl = 1.f;
      if (eb > .85f && sp >= 1.5f && lvl < eb * .5f) lvl = eb * .5f;
      float lat = fabsf(dot3(vd, L)) * sp;
      if (lat >= 2.5f && sp >= 6.f && c->grounded && lvl < fabsf(dot3(vd, L))) lvl = fabsf(dot3(vd, L));
    }
  }
  if (lvl < b) lvl = b;                                                                                     // 0x2823e0
  if (lvl > 0.f && lvl < .5f) lvl = .5f;                                                                    // 0x28240c
  return lvl;
}

// ---------- кольцо ----------
#ifndef PLAYER_SKID_SOUND_OLD
#define PLAYER_SKID_SOUND_OLD 0
#endif
static int skidHoverOldOn; int skidHover(const char *name);
#ifndef SV_TIRESND_OLD
#define SV_TIRESND_OLD 0
#endif
float skidSoundLevel(const Car *c, const CarInput *in) {
  if (!c || !c->grounded) return 0.f;
  // №413 (Роб: летающие машины должны быть без звука шин): у парящих машин (hbike_v, frink_v, ship, witchcar, mono_v) нет колёс, как нет и следов
  // (skidHover); звук пробуксовки им тоже не нужен. SKID_HOVER_SOUND_OLD=1 — прежнее (шины шумят)
#ifndef SKID_HOVER_SOUND_OLD
#define SKID_HOVER_SOUND_OLD 0
#endif
  if (!SKID_HOVER_SOUND_OLD && !skidHoverOldOn && skidHover(c->p.name)) return 0.f;
  if (PLAYER_SKID_SOUND_OLD) return c->slip ? fminf(1.f, fabsf(c->vl) / 6.f + .3f) : 0.f;
#if !SKID_OLD
  if (!skidOldOn) {
    float sp = sqrtf(c->vel[0] * c->vel[0] + c->vel[1] * c->vel[1] + c->vel[2] * c->vel[2]);
    float eb = in ? in->handbrake : 0.f;
    float brk = in ? in->brake : 0.f;
    // №562: стоя на месте (sp < 0.5 м/с), на ручнике или на тормозе — скрежета шин нет
    if (sp < 0.5f && (eb > 0.1f || brk > 0.1f)) return 0.f;
    // Стоя в упоре в стену без движения — скрежета нет
    if (sp < 0.2f) return 0.f;
    float burnL = 0.f, lvl = skidLevel(c, in, &burnL);
    if (sp < 0.5f && (eb > 0.1f || brk > 0.1f)) return 0.f;
    // сверка g01-cars, P10-12: звук шин включается при уровне выше PeeloutMin 0,1 (global.spt), ниже гаснет; громкость: пробуксовка — 0,8·(уровень − 0,1)/0,9; занос на ходу — 0,6 на старте,
    // растёт 0,3 в секунду до 0,9. Высота клипа в оригинале не меняется (в audio.c у нас 0,9 + 0,2·уровень — передано g08-sound). Было: порог 0,02, громкость уровень × 0,65. -DSV_TIRESND_OLD=1
    if (!SV_TIRESND_OLD) {
      static float slideT0 = -1.f;
      if (lvl <= .1f) { slideT0 = -1.f; return 0.f; }
      if (burnL >= lvl - 1e-4f && burnL > 0.f) { slideT0 = -1.f; return fminf(1.f, .8f * (lvl - .1f) / .9f); }
      if (slideT0 < 0.f || c->t < slideT0) slideT0 = c->t;
      return fminf(.9f, .6f + .3f * (c->t - slideT0));
    }
    // Громкость скрежета резины: умеренная, не глушит мотор (как в оригинале PS2)
    return fminf(1.f, fmaxf(0.f, lvl * 0.65f));
  }
#endif
  return skidLevel(c, in, NULL); // PS2 Vehicle skid rule shared with marks; audio chooses the authored clip
}

static int ringAlloc(unsigned short tag) {
  for (int k = 0; k < SKID_QUADS; k++) {
    int q = head; head = (head + 1) % SKID_QUADS;
    if (qlive[q]) continue;                       // чужой тянущийся квад не трогаем
    if (used < SKID_QUADS) used++;
    qown[q] = tag; qlive[q] = 1; qlen[q] = 0.f; made++; if (curSlot > 0) madeAi++;
    return q;
  }
  return -1;
}
static void quadWrite(int q, const float *al, const float *ar, float va, unsigned char aa,
                      const float *bl, const float *br, float vbv, unsigned char ab) {
  SV *v = &vb[q * 4];
  v[0] = (SV){0.f, va, col(aa), al[0], al[1], al[2]};
  v[1] = (SV){1.f, va, col(aa), ar[0], ar[1], ar[2]};
  v[2] = (SV){1.f, vbv, col(ab), br[0], br[1], br[2]};
  v[3] = (SV){0.f, vbv, col(ab), bl[0], bl[1], bl[2]};
  dirty = 1;
}
static inline unsigned char alphaOf(int q, int end) { return alphaRaw(vb[q * 4 + (end ? 2 : 0)].c); }
static void alphaSet(int q, int end, unsigned char a) {
  SV *v = &vb[q * 4 + (end ? 2 : 0)];
  v[0].c = col(a); v[1].c = col(a); dirty = 1;
}

// 0x206b20: последний участок прозрачный, к нему спад по последнему метру (альфа не больше 255 × путь до конца)
static void trailEnd(Trail *t) {
  if (!t->on) return;
  if (t->q >= 0) {
    qlive[t->q] = 0;
    if (qlen[t->q] >= SKID_MIN && t->nh < SKID_HIST) t->hist[t->nh++] = t->q;
    else if (qlen[t->q] >= SKID_MIN) { memmove(t->hist, t->hist + 1, sizeof(int) * (SKID_HIST - 1)); t->hist[SKID_HIST - 1] = t->q; }
    else { qown[t->q] = 0; dirty = 1; }   // короче шага — не след
  }
  float dEnd = 0.f;
  for (int k = t->nh - 1; k >= 0 && dEnd < SKID_FADE; k--) {
    int q = t->hist[k];
    if (qown[q] != t->tag) break;          // кольцо уже затёрло
    float a1 = 255.f * dEnd; if (a1 < alphaOf(q, 1)) alphaSet(q, 1, (unsigned char)a1);
    dEnd += qlen[q];
    float a0 = 255.f * dEnd; if (a0 < alphaOf(q, 0)) { alphaSet(q, 0, (unsigned char)a0);
      if (k > 0 && qown[t->hist[k - 1]] == t->tag) alphaSet(t->hist[k - 1], 1, (unsigned char)a0); }   // тот же участок у соседа
  }
  t->on = 0; t->q = -1; t->nh = 0;
}

static void side(const float *dir, const float *n, float out[3]) {   // поперёк пути по земле: dir × n, единичный
  out[0] = dir[1] * n[2] - dir[2] * n[1]; out[1] = dir[2] * n[0] - dir[0] * n[2]; out[2] = dir[0] * n[1] - dir[1] * n[0];
  float l = sqrtf(dot3(out, out));
  if (l < 1e-4f) { out[0] = 1.f; out[1] = out[2] = 0.f; return; }
  out[0] /= l; out[1] /= l; out[2] /= l;
}

static void trailPoint(Trail *t, const float *p, const float *n, const float *vd, unsigned char a, int surf) {
  if (t->on && dot3(vd, t->d) < SKID_TURN) trailEnd(t);
  if (t->on && !SV_SKID_SURF_OLD && t->surf != surf) trailEnd(t);                 // сверка g01-cars 9.2: смена поверхности — новый след. -DSV_SKID_SURF_OLD=1                       // 0x205414: резкий поворот скорости — новый след
  if (t->on) {
    float dx = p[0] - t->s[0], dy = p[1] - t->s[1], dz = p[2] - t->s[2];
    if (dx * dx + dy * dy + dz * dz > SKID_JUMP * SKID_JUMP) trailEnd(t);     // машину переставили
  }
  memcpy(t->d, vd, sizeof t->d);
  if (!t->on) {                                                                 // 0x2065ac: первый участок — поперёк скорости
    float s[3]; side(vd, n, s);
    t->on = 1; t->q = -1; t->nh = 0; t->surf = surf; t->tag = ++tagCtr ? tagCtr : ++tagCtr;
    memcpy(t->s, p, sizeof t->s);
    for (int k = 0; k < 3; k++) { t->sl[k] = p[k] - s[k] * SKID_HALF_W; t->sr[k] = p[k] + s[k] * SKID_HALF_W; }
    t->sv = 0.f; t->sa = 0;                                                     // 0x206ab8: начало прозрачное
    return;
  }
  float d[3] = {p[0] - t->s[0], p[1] - t->s[1], p[2] - t->s[2]}, L = sqrtf(dot3(d, d));
  if (L < SKID_MIN) return;                                                     // 0x206340: ближе 0,224 м — не участок
  d[0] /= L; d[1] /= L; d[2] /= L;
  float s[3], el[3], er[3]; side(d, n, s);
  for (int k = 0; k < 3; k++) { el[k] = p[k] - s[k] * SKID_HALF_W; er[k] = p[k] + s[k] * SKID_HALF_W; }
  if (t->q < 0 && (t->q = ringAlloc(t->tag)) < 0) return;
  float v1 = t->sv + L * SKID_VPM;
  quadWrite(t->q, t->sl, t->sr, t->sv, t->sa, el, er, v1, a);
  qlen[t->q] = L;
  if (L >= SKID_STEP) {                                                         // участок закреплён, дальше — новый квад
    qlive[t->q] = 0;
    if (t->nh == SKID_HIST) memmove(t->hist, t->hist + 1, sizeof(int) * (SKID_HIST - 1)), t->nh--;
    t->hist[t->nh++] = t->q; t->q = -1;
    memcpy(t->s, p, sizeof t->s); memcpy(t->sl, el, sizeof el); memcpy(t->sr, er, sizeof er);
    t->sv = v1 - floorf(v1); t->sa = a;                                         // текстура повторяется: целую часть v отбросить
  }
}

// Правка 413 (хвост, 09.10): у парящих машин следов нет. SLES_518.97: при сборке машины (0x28d800…) по имени ставится байт +657 —
// hbike_v (0x28d8ec), frink_v (0x28d9f8), ship (0x28dcbc), witchcar (0x28dce4), mono_v (0x28dd94); у honor_v и rocke_v — другой
// байт +658 (колёса в кузове, следы есть). Функция колёс следа 0x288378 при байте +657 сразу выходит (0x2884d8: bnez -> 0x288618).
// SKID_HOVER_OLD=1 / skidHoverOld(1) — прежнее, следы и под ними (красный контроль tests/hover/skid_hover.sh)
#ifndef SKID_HOVER_OLD
#define SKID_HOVER_OLD 0
#endif
static int skidHoverOldOn = SKID_HOVER_OLD;
void skidHoverOld(int on) { skidHoverOldOn = on != 0; }
int skidHover(const char *name) {
  static const char *const h[] = {"hbike_v", "frink_v", "ship", "witchcar", "mono_v"};
  if (!name) return 0;
  for (unsigned i = 0; i < sizeof h / sizeof *h; i++) if (!strncasecmp(name, h[i], 12)) return 1;
  return 0;
}
void skidCar(int slot, const Car *c, const CarInput *in, const ColWorld *w) {
#ifndef SKID_HOST
  unsigned t0 = sceKernelGetSystemTimeLow();
#endif
  if (slot < 0 || slot >= SKID_CARS) return;
  curSlot = slot;
  Trail *T = tr[slot];
  int farCar = 0;
  if (!SKID_FAR_OLD && slot > 0 && c && skEyeOk) { float fx = c->x - skEye[0], fz = c->z - skEye[2];
    farCar = fx * fx + fz * fz > (SKID_DRAW_DIST + 40.f) * (SKID_DRAW_DIST + 40.f); }
  float burn = 0.f, lvl = (SKID_MARKS && levelOn && svcOn && w && c && !farCar) ? skidLevel(c, in, &burn) : 0.f;
  if (slot == 0) lvlLast = lvl;
  float sp = c ? sqrtf(c->vel[0] * c->vel[0] + c->vel[1] * c->vel[1] + c->vel[2] * c->vel[2]) : 0.f;
  if (lvl < .01f || sp < 1e-3f || c->destroyed || c->supOn || (!skidHoverOldOn && skidHover(c->p.name))) {   // 0x2884d8: парящая                  // 0x205368: уровень < 0,01 — колёса след заканчивают
    for (int i = 0; i < 4; i++) trailEnd(&T[i]);
  } else {
    const CarParams *P = &c->p;
    float L[3], U[3], F[3], vd[3] = {c->vel[0] / sp, c->vel[1] / sp, c->vel[2] / sp};
    axes(c->q, L, U, F);
    unsigned char a = (unsigned char)(lvl * .5f * 255.f);                     // 0x205368 × 0,5; 0x2062e8 × 255
    for (int i = 0; i < 4; i++) {
      int rear = i >= 2;                                                        // ведущие у порта — задние (car.c)
      if ((burn > 0.f && !rear) || c->comp[i] <= 0.f) { trailEnd(&T[i]); continue; }   // 0x2884f8: пробуксовка — ведущие; 0x288518: колесо на земле
      float bx = (i & 1) ? -P->wheelX : P->wheelX, bz = i < 2 ? P->wheelF : -P->wheelR, my = c->mountY;
      float wx = c->x + L[0] * bx + U[0] * my + F[0] * bz, wy = c->y + L[1] * bx + U[1] * my + F[1] * bz,
            wz = c->z + L[2] * bx + U[2] * my + F[2] * bz;
      float gy, n[3]; int mat = COL_MAT_HARD;
      if (!colGroundMat(w, wx, wz, wy + .3f, wy - my - 1.5f, &gy, n, &mat) || mat == COL_MAT_FOLIAGE) { trailEnd(&T[i]); continue; }
      float p[3] = {wx + n[0] * SKID_LIFT, gy + n[1] * SKID_LIFT, wz + n[2] * SKID_LIFT};
      curHalfW = SV_SKID_W_OLD ? .125f : P->wheelRad * .4f;
      int sf = c->wheelSurf[i], surfKind = sf == SURF_GRASS ? 1 : sf == SURF_DIRT ? 2 : 0;   // шейдер следа: трава, грунт, иначе асфальт (0x205218)
      trailPoint(&T[i], p, n, vd, a, surfKind);
    }
  }
#ifndef SKID_HOST
  if (slot == 0) usCar = 0; usCar += (sceKernelGetSystemTimeLow() - t0);
#endif
}

void skidGetStat(SkidStat *s) {
  int q = 0, live = 0;
  for (int i = 0; i < used; i++) if (qown[i] && qlen[i] >= SKID_MIN) q++;
  for (int k = 0; k < SKID_CARS; k++) for (int i = 0; i < 4; i++) live += tr[k][i].on;
  s->quads = q; s->drawn = drawnLast; s->live = live; s->made = made; s->skipped = skipped; s->lvl = lvlLast; s->us = usLast; s->madeAi = madeAi;
}

unsigned skidQuadRgb(int i) { return i >= 0 && i < SKID_QUADS && qown[i] ? vb[i * 4].c & 0xffffffu : 0u; }
int skidQuadAt(int i, float p[4][3], unsigned char a[4]) {
  if (i < 0 || i >= SKID_QUADS || !qown[i] || qlen[i] < SKID_MIN) return 0;
  for (int k = 0; k < 4; k++) { const SV *v = &vb[i * 4 + k]; p[k][0] = v->x; p[k][1] = v->y; p[k][2] = v->z; a[k] = alphaRaw(v->c); }
  return 1;
}

#ifndef SKID_HOST
void skidDraw(float ex, float ey, float ez) {
  unsigned t0 = sceKernelGetSystemTimeLow();
  skEye[0] = ex; skEye[1] = ey; skEye[2] = ez; skEyeOk = 1;
  drawnLast = 0;
  if (!SKID_MARKS || !levelOn || !svcOn || !used) { usLast = usCar; return; }
  if (skidTex == -2 && fxReady()) skidTex = fxTexFind("skid.bmp");   // после загрузки пакета уровня (skidClear сбрасывает)
  if (skidTex < 0 && SKID_TINT_OLD) { usLast = usCar; return; }
  int n = 0; float R2 = SKID_DRAW_DIST * SKID_DRAW_DIST;
  for (int q = 0; q < used; q++) {
    if (!qown[q] || qlen[q] < SKID_MIN) continue;
    const SV *v = &vb[q * 4]; float dx = v->x - ex, dy = v->y - ey, dz = v->z - ez;
    if (dx * dx + dy * dy + dz * dz < R2) n++;
  }
  if (!n) { usLast = usCar + (sceKernelGetSystemTimeLow() - t0); return; }
  unsigned bytes = (unsigned)n * 6u * sizeof(unsigned short);
  if (geRoomFn && !geRoomFn(bytes + 256u)) { skipped++; usLast = usCar; return; }   // правка 103: список GE полон
  unsigned short *ix = (unsigned short *)geVbAlloc((bytes + 3u) & ~3u, GEVB_SKID_OLD), *o = ix;
  for (int q = 0; q < used; q++) {
    if (!qown[q] || qlen[q] < SKID_MIN) continue;
    const SV *v = &vb[q * 4]; float dx = v->x - ex, dy = v->y - ey, dz = v->z - ez;
    if (dx * dx + dy * dy + dz * dz >= R2) continue;
    unsigned short b = (unsigned short)(q * 4);
    o[0] = b; o[1] = b + 1; o[2] = b + 2; o[3] = b; o[4] = b + 2; o[5] = b + 3; o += 6;
  }
  if (dirty) { sceKernelDcacheWritebackRange(vb, sizeof(SV) * 4u * (unsigned)used); dirty = 0; }
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_ALPHA_TEST); if (SKID_TINT_OLD) sceGuEnable(GU_TEXTURE_2D); else sceGuDisable(GU_TEXTURE_2D); sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_CLAMP, GU_REPEAT);   // поперёк — край, вдоль — повтор плитки
  sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
  sceGuDepthMask(GU_TRUE);                                                       // 0x205e60: глубину не пишут
  if (SKID_TINT_OLD) fxTexBind(skidTex);
  sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n * 6, ix, vb);
  sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND); sceGuEnable(GU_TEXTURE_2D);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexScale(128.f, 128.f);
  drawnLast = n;
  usLast = usCar + (sceKernelGetSystemTimeLow() - t0);
}
#else
void skidDraw(float ex, float ey, float ez) { (void)ex; (void)ey; (void)ez; }
#endif
