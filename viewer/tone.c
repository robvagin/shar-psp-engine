// Кривая тона палитр мира — viewer/tone.h. Перенесено из main.c palTone (правки 29, 99) и расширено правкой 144.
#include "tone.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PAL_GAMMA 1.10f      // правка 29: середины темнее (экран PSP светлее монитора)
#define PAL_CONTRAST 1.12f   // правка 99: контраст вокруг середины (128)

static int satPct = TONE_SAT_DEF;
static unsigned char lut[256]; static int lutReady;
static int gammaPct = 100;   // калибровка под экран PSP-2000 (проба 16.10): 100 — как есть; больше — темнее середины, меньше — светлее
static unsigned char glut[256]; static int glutPct = -1;

int toneSatIndex(int pct) {
  int i = (pct - TONE_SAT_MIN + TONE_SAT_STEP / 2) / TONE_SAT_STEP;
  return i < 0 ? 0 : i >= TONE_SAT_N ? TONE_SAT_N - 1 : i;
}
int toneSatFromIndex(int idx) {
  idx = idx < 0 ? 0 : idx >= TONE_SAT_N ? TONE_SAT_N - 1 : idx;
  return TONE_SAT_MIN + idx * TONE_SAT_STEP;
}
void toneSetGamma(int pct) { gammaPct = pct < 50 ? 50 : pct > 200 ? 200 : pct; }
int toneGammaPct(void) { return gammaPct; }
void toneSetSat(int pct) { satPct = toneSatFromIndex(toneSatIndex(pct)); }
int toneSatPct(void) { return satPct; }

static unsigned char clamp8(float v) { return (unsigned char)fminf(255.f, fmaxf(0.f, v + .5f)); }

// №754 (Роб 17.10: «пресетик, как в фотошопе, только для режима Мультик: некоторые цвета ярче, некоторые приглушить, цвет доминирует, тёмный контур,
// цвета 90-х»; реф scenic_simpsons, замер 588 кадров: нет чёрного и белого, много индиго и лавандового, заливки сочные). Поверх кривой палитры, только
// при включённом Cartoon (toneSetToon): 1) оттенок цветного на 30 % к ближнему цвету палитры сериала (по оттенку, яркости и насыщенности);
// 2) средние серые (асфальт, бетон, двор) — к барвинку той же яркости; 3) плёнка: тёмное к индиго TOON_INK, светлое к кремовому; 4) насыщенность
// ×1,35 с мягким потолком. Пробы на кадрах мультика L1 — scratchpad p90/preset90.py (та же формула). TONE_TOON90_OLD=1 — без пресета
#ifndef TONE_TOON90_OLD
#define TONE_TOON90_OLD 0
#endif
static int toonOn;
// №754: мир-мультик на палитрах обычного уровня (вместо отдельного level_toon.shr — включается сразу и на всех уровнях): цвет палитры стягивается к среднему
// цвету текстуры по её точкам — ровные материалы почти в заливку, пёстрые (вывески, окна) сохраняют до 60 % рисунка. TONE_FLAT_OLD=1 — без стягивания
#ifndef TONE_FLAT_OLD
#define TONE_FLAT_OLD 0
#endif
static const unsigned char *toneSt;
void toneSetStat(const unsigned char *st) { toneSt = st; }
// №773/№777 (Роб 18.10: «разграничители парковки и пол разных оттенков, хотя в оригинале одна текстура — что-то сильнее стягиваешь, что-то нет»; «регулятор,
// насколько видны родные текстуры»): доля рисунка текстуры в мультике — ОДНА для всех (toneToonTex, 0 — ровная заливка средним цветом, 100 — родная текстура,
// по умолчанию 35), вместо доли 0,10–0,60 по разбросу яркости каждой текстуры. TONE_TEX_UNIFORM_OLD=1 — прежнее (по разбросу)
#ifndef TONE_TEX_UNIFORM_OLD
#define TONE_TEX_UNIFORM_OLD 0
#endif
static int toonTexPct = 35;
// №865 (Роб 22.10: «однотонные текстуры — шум, пара кирпичей — в мультике не так необходимы: заливками, оптимизированнее; дороги, асфальт, траву —
// оставить с текстурами»): к общей доле рисунка (toonTexPct) — два правила по самой текстуре: земля (метка ground экспорта, tstat[4] бит 4) — рисунок не
// меньше TOON_GROUND_K; почти однотонная (разброс яркости ≤ ~16 из 255: tstat[3] ≤ TOON_FLAT_ST, texToonStat) — ровная заливка средним цветом. TOON_FILL865_OLD=1 — как было
#ifndef TOON_FILL865_OLD
#define TOON_FILL865_OLD 0
#endif
#define TOON_GROUND_K .70f
#define TOON_FLAT_ST 46   // tstat[3] = (0,10 + 0,5·(sd−10)/40)·255: 46 — sd ≈ 15
// №892 (Роб 23.10: «окна зданий вдалеке слишком светлые, странное сглаживание»; замер L1, дальний дом ~100 м, эмулятор: тёмная панель окна без Cartoon — яркость 64 из 255,
// стена 188 (контраст 123, у PS2 тёмные окна 50–58 при стене 150–170); в Cartoon панель 118, контраст 73: общая доля рисунка toonTex (45 %) стягивает ВСЕ записи палитры к
// среднему цвету текстуры, и тёмное стекло на светлой стене теряет 55 % глубины (окно — светлый размытый прямоугольник). Правило: запись, темнее среднего текстуры по яркости больше
// TOON_WIN_D0 (из 255; исходные цвета, до кривой), держит рисунок сильнее — доля растёт от общей к TOON_WIN_K на отрезке TOON_WIN_RAMP; земля и листва (tstat[4] биты 4, 2) и ровные
// текстуры (k = 0) не меняются; без Cartoon шаг не работает. Палитра мультика №825 (оттенки) не тронута. TONE_WIN892_OLD=1 — как до правки
#ifndef TONE_WIN892_OLD
#define TONE_WIN892_OLD 0
#endif
#ifndef TOON_WIN_D0
#define TOON_WIN_D0 55.f
#endif
#ifndef TOON_WIN_RAMP
#define TOON_WIN_RAMP 45.f
#endif
#ifndef TOON_WIN_K
#define TOON_WIN_K .90f
#endif
// №1026 (Роб 26.10: «билборды, картинки на вывесках, не поддаются цветокору; и монеты тоже, и подобные элементы»): вывески, плакаты и картинки в списке notoon.txt шли мимо шагов мультика (пресет «Мультик 90-х») и мимо
// контраста из меню — в них действовали только насыщенность и гамма. Теперь они проходят ту же цветокоррекцию, что город: пресет мультика, контраст, насыщенность, гамма; картинка при этом НЕ стягивается к среднему
// цвету (надпись остаётся читаемой). Облака неба и купол (бит 8, main_levels.h) — как были. Спрайт монеты, спрайты ключа и телефона и текстуры машин палитры уже проходят (palTone при загрузке и при смене
// настроек, журнал COINRETONE / TONE1026); здесь добавлены только вывески. TONE1026_OLD=1 / файл tone1026old / «flag tone1026 0» — вывески как были
#ifndef TONE1026_OLD
#define TONE1026_OLD 0
#endif
static int tone1026On = !TONE1026_OLD;
void toneSet1026(int on) { tone1026On = on != 0; }
int tone1026Get(void) { return tone1026On; }
void toneToonTex(int pct) { toonTexPct = pct < 0 ? 0 : pct > 100 ? 100 : pct; }
int toneToonTexPct(void) { return toonTexPct; }
// №777 (Роб: «настроить насыщенность, контрастность»): контраст палитр вокруг середины (128) последним шагом кривой, в обоих режимах; 100 — как есть, 50..200
static int contrastPct = 100;
void toneSetContrast(int pct) { contrastPct = pct < 50 ? 50 : pct > 200 ? 200 : pct; }
int toneContrastPct(void) { return contrastPct; }
void toneSetToon(int on) { toonOn = on != 0; }

int toneToon(void) { return toonOn; }
static const unsigned char T90[22][3] = {
  {0x3f,0xa9,0xe0},{0x2d,0x7f,0xd0},{0x6f,0xc3,0xe8},{0x3a,0x30,0xb8},{0x24,0x1c,0x78},{0x5a,0x2e,0xa0},{0xaa,0xa8,0xe8},{0xb8,0xb4,0xf0},
  {0x7a,0x3f,0xc8},{0xe2,0x53,0x3a},{0xf0,0x8a,0x2e},{0xf5,0xa0,0x4a},{0xf0,0x6a,0x9a},{0xf4,0xa6,0xc8},{0x7f,0xd3,0xa8},{0x2b,0x7f,0x8a},
  {0x8c,0xc6,0x3a},{0x2f,0x6b,0x2a},{0xd9,0xa3,0x2c},{0xc9,0xb5,0x3a},{0x8e,0x1b,0x14},{0xd8,0xb8,0x8a}};
static float t90[22][3]; static int t90ready;
static void rgbHsv(float r, float g, float b, float *h, float *s, float *v) {
  float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn; *v = mx; *s = mx > 0.f ? d / mx : 0.f;
  if (d <= 0.f) { *h = 0.f; return; }
  float hh = mx == r ? fmodf((g - b) / d, 6.f) : mx == g ? (b - r) / d + 2.f : (r - g) / d + 4.f; if (hh < 0.f) hh += 6.f; *h = hh / 6.f;
}
static void hsvRgb(float h, float s, float v, float *r, float *g, float *b) {
  float hh = h * 6.f; int i = (int)floorf(hh); float f = hh - (float)i; i = ((i % 6) + 6) % 6;
  float p = v * (1.f - s), q = v * (1.f - f * s), t = v * (1.f - (1.f - f) * s);
  switch (i) { case 0: *r = v; *g = t; *b = p; break; case 1: *r = q; *g = v; *b = p; break; case 2: *r = p; *g = v; *b = t; break;
               case 3: *r = p; *g = q; *b = v; break; case 4: *r = t; *g = p; *b = v; break; default: *r = v; *g = p; *b = q; }
}
static float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
// №780 (Роб 18.10: «присадки нравятся, но иногда пол болотный и темнеет — насыщеннее, звонче, контрастнее»; проба build/toon779/prev780.py на кадрах PSP TEST33):
// 1б) оливковые и жёлто-зелёные (55–85°, «болото» тротуаров, газона, дерева) уводятся к жёлтому (~52°) или к зелёному (~88°), светлее и сочнее; 3) тёмное к туши
// мягче (порог 0,22 вместо 0,30, доля 0,60 вместо 0,85: пол не темнеет); 3б) середины светлее (степень 0,94) и мягкая S-кривая (+10 % контраста); 4) насыщенность ×1,5.
// TONE_T90_780_OLD=1 — пресет №754 как в TEST33 (проба tests/tone/preset90.py с old=True)
#ifndef TONE_T90_780_OLD
#define TONE_T90_780_OLD 0
#endif
// №784/№801 (Роб 18.10 после TEST34: «немножечко более светлых оттенков»; «тёмные цвета увести в более красивые, траву — в интересный оттенок: изумрудный,
// рыжий, горчичный, как у Patagonia»): 1в) зелёные 70–170° (трава, кусты) — к изумрудному (оттенок к 158°, доля 0,95; насыщенность к 0,50 — после ×1,5 около 0,75; яркость к 0,48, глубже); тёмно-коричневые 12–42°
// (земля, дерево, кирпич) — к ржавому 18°; 3) тёмное не тонет в туши: цветное тёмное — глубокий тон того же оттенка (S ≥ 0,62, яркость к 0,24), серо-чёрное —
// к глубокой сливе #2E2257; 3б) середины ещё светлее (степень 0,90 вместо 0,94). Проба — tests/tone/preset90.py (new801). TONE_T90_801_OLD=1 — как TEST34 (№780)
#ifndef TONE_T90_801_OLD
#define TONE_T90_801_OLD 0
#endif
// №825 (Роб 18.10 вечер: «оттенки как креветочный, выгоревший красно-оранжевый; комплементарные — оливковый; минимально неоновые цвета — только светящееся;
// vibrant, Patagonia vibe; синий — насыщенный navy, яркие голубые, бирюзовые; трава капельку насыщеннее и темнее; небо всегда оптимистичное»; «тёмное уходит
// не в чёрное, а в более синее»). Ресёрч — docs/handoff/МУЛЬТИК_90х_АНАЛИЗ_2026-10-17.md «Ресёрч 18.10» (палитра Springfield ggsci: #F05C3B #FD7446 #46732E
// #D5E4A2 #197EC0 #71D0F5 #1A9993 #370335). 1г) тёплое светлое (352–36°) — к креветочному 13°, S к 0,55, светлее; тусклое жёлто-зелёное тёмное (48–78°) — к оливе 74°;
// синие 188–232° (к 250° — на нет: фиолетовая улица и лаванда остаются): тёмные — navy 224° (S от 0,72), светлые — голубой 200° (S к 0,50, светлее); 168–190° — бирюза 178°; трава (1в) — S к 0,56, яркость к 0,44
// (было 0,50 / 0,48); 3) тёмное цветное уходит к синему (оттенок к 235° до 0,35 пути); 5) анти-неон: после ×1,5 зелёно-голубое светлое — S не больше 0,82.
// Проба — tests/tone/preset90.py (new825). TONE_T90_825_OLD=1 — как №801
#ifndef TONE_T90_825_OLD
#define TONE_T90_825_OLD 0
#endif
static float hueTo(float h, float tgt, float w) { float d = tgt - h + .5f; d -= floorf(d); d -= .5f; h += d * w; return h - floorf(h); }   // по короткой дуге, доли 0..1
static float bandW(float hd, float a, float b, float ramp) { float x = hd; if (a > b) { if (x < b + ramp) x += 360.f; b += 360.f; } return clamp01((x - a + ramp) / ramp) * clamp01((b + ramp - x) / ramp); }   // 1 внутри [a, b], к краям — спад ramp°
static void toon90(unsigned char *c) {
  static const float INK[3] = {0x2A / 255.f, 0x24 / 255.f, 0x40 / 255.f}, CREAM[3] = {0xF2 / 255.f, 0xEE / 255.f, 0xDC / 255.f};
  if (!t90ready) { for (int j = 0; j < 22; j++) rgbHsv(T90[j][0] / 255.f, T90[j][1] / 255.f, T90[j][2] / 255.f, &t90[j][0], &t90[j][1], &t90[j][2]); t90ready = 1; }
  float r = c[0] / 255.f, g = c[1] / 255.f, b = c[2] / 255.f, h, s, v;
  rgbHsv(r, g, b, &h, &s, &v);
  float w = clamp01((s - .12f) / .1f) * .3f;   // 1) оттенок к палитре сериала (только цветное)
  if (w > 0.f) {
    int bj = 0; float bd = 1e9f;
    for (int j = 0; j < 22; j++) { float dh = fabsf(h - t90[j][0]); if (dh > .5f) dh = 1.f - dh;
      float sc = dh * 3.f + fabsf(v - t90[j][2]) * 1.2f + fabsf(s - t90[j][1]); if (sc < bd) { bd = sc; bj = j; } }
    float d = t90[bj][0] - h + .5f; d -= floorf(d); d -= .5f;
    h += d * w; h -= floorf(h); s += (fmaxf(s, t90[bj][1]) - s) * w * .5f;
    hsvRgb(h, s, v, &r, &g, &b);
  }
  if (!TONE_T90_780_OLD) {   // 1б) №780: без болота — олива к жёлтому или зелёному, светлее и сочнее
    rgbHsv(r, g, b, &h, &s, &v);
    float hd = h * 360.f, wo = clamp01(1.f - fabsf(hd - 70.f) / 25.f) * clamp01((s - .12f) / .1f);
    if (wo > 0.f) {
      h += (hd < 70.f ? -1.f : 1.f) * wo * (18.f / 360.f); h -= floorf(h);
      if (v < .80f) v += (.80f - v) * .40f * wo;
      s += (fmaxf(s, .70f) - s) * .35f * wo;
      hsvRgb(h, s, v, &r, &g, &b);
    }
  }
  if (!TONE_T90_801_OLD) {   // 1в) №801: трава и кусты — к изумрудному, тёмно-коричневое — к ржавому
    rgbHsv(r, g, b, &h, &s, &v);
    float hd = h * 360.f, sc = clamp01((s - .20f) / .15f);
    float wg = clamp01((hd - 70.f) / 10.f) * clamp01((170.f - hd) / 15.f) * sc * .95f;
    float wr = clamp01((hd - 12.f) / 8.f) * clamp01((42.f - hd) / 8.f) * sc * clamp01((.60f - v) / .15f) * .50f;
    float gS = TONE_T90_825_OLD ? .50f : .56f, gV = TONE_T90_825_OLD ? .48f : .44f;   // №825: трава капельку насыщеннее (0,50 -> 0,56) и темнее (0,48 -> 0,44)
    if (wg > 0.f) { float d = 158.f / 360.f - h; h += d * wg; s += (fminf(s, gS) - s) * wg / .95f; v += (gV - v) * .40f * wg / .95f; }   // изумрудный: оттенок к 158°, насыщенность к 0,50 (×1,5 ниже доводит до ~0,75), яркость к 0,48 (глубже, не мятный)
    else if (wr > 0.f) { float d = 18.f / 360.f - h; h += d * wr; h -= floorf(h); s += (fmaxf(s, .65f) - s) * .6f * wr / .5f; if (v < .45f) v += (.45f - v) * .35f * wr / .5f; }
    if (wg > 0.f || wr > 0.f) hsvRgb(h, s, v, &r, &g, &b);
  }
  if (!TONE_T90_825_OLD) {   // 1г) №825: креветочный, олива, navy / голубой / бирюза
    rgbHsv(r, g, b, &h, &s, &v);
    float hd = h * 360.f, sc = clamp01((s - .15f) / .15f);
    float ww = bandW(hd, 352.f, 36.f, 12.f) * sc * clamp01((v - .50f) / .15f) * .70f;   // тёплое светлое -> креветочный (не жёлтое: кожа героев 49° вне полосы)
    float wo = bandW(hd, 48.f, 78.f, 12.f) * clamp01((s - .40f) / .15f) * clamp01((.62f - v) / .12f) * .60f;   // тусклое тёмное жёлто-зелёное -> олива (тротуары светлые — вне)
    float wb = bandW(hd, 188.f, 232.f, 18.f) * sc, wt = bandW(hd, 170.f, 188.f, 10.f) * sc * .70f;   // широкие края: таблица 25³ без ступенек; до 250° — фиолетовая улица и лаванда свои
    if (ww > 0.f) { h = hueTo(h, 13.f / 360.f, ww); s += (.55f - s) * .50f * ww; v += (fmaxf(v, .90f) - v) * .40f * ww; }
    if (wo > 0.f) { h = hueTo(h, 74.f / 360.f, wo); s += (fmaxf(s, .55f) - s) * .60f * wo; v += (.50f - v) * .40f * wo; }
    if (wb > 0.f) { float wd = clamp01((.55f - v) / .25f);   // тёмные — navy, светлые — чистый голубой
      float hb = (224.f * wd + 200.f * (1.f - wd)) / 360.f; h = hueTo(h, hb, wb * .70f);
      float st = .50f + (fmaxf(s, .72f) - .50f) * wd, vt = fmaxf(v, .86f) + (.36f - fmaxf(v, .86f)) * wd;   // без ступеньки: доли по тёмности
      s += (st - s) * .50f * wb; v += (vt - v) * .35f * wb; }
    if (wt > 0.f) { h = hueTo(h, 178.f / 360.f, wt); s += (fmaxf(s, .55f) - s) * .50f * wt; v += (fmaxf(v, .62f) - v) * .30f * wt; }
    if (ww > 0.f || wo > 0.f || wb > 0.f || wt > 0.f) hsvRgb(h, s, v, &r, &g, &b);
  }
  rgbHsv(r, g, b, &h, &s, &v);   // 2) средние серые -> барвинок той же яркости
  float mid = clamp01((v - .12f) / .15f) * clamp01((.88f - v) / .15f), wg = clamp01((.15f - s) / .15f) * mid * .7f;
  if (wg > 0.f) { float lr, lg, lb; hsvRgb(250.f / 360.f, .20f, v, &lr, &lg, &lb); r += (lr - r) * wg; g += (lg - g) * wg; b += (lb - b) * wg; }
  v = fmaxf(r, fmaxf(g, b)); float s2 = v > 1e-6f ? (v - fminf(r, fminf(g, b))) / v : 0.f;   // 3) плёнка: тёмное -> индиго, светлое белёсое -> кремовый (цветные светлые стены — свои)
  float wd = TONE_T90_780_OLD ? clamp01(1.f - v / .30f) * .85f : clamp01(1.f - v / .22f) * .60f, wl = clamp01((v - .80f) / .20f) * .75f * clamp01(1.f - s2 / .4f);
  if (!TONE_T90_801_OLD) {   // 3) №801: тёмное — глубокий тон своего оттенка, серо-чёрное — глубокая слива (не мутная тушь)
    static const float DEEP[3] = {0x2E / 255.f, 0x22 / 255.f, 0x57 / 255.f};
    float w3 = clamp01(1.f - v / .30f);
    if (w3 > 0.f) {
      float hh, ss, vv; rgbHsv(r, g, b, &hh, &ss, &vv); float wc = clamp01((ss - .10f) / .10f);
      if (!TONE_T90_825_OLD) hh = hueTo(hh, 235.f / 360.f, w3 * .35f * clamp01((ss - .10f) / .15f));   // №825: тёмное уходит не в чёрное, а в синее
      if (wc > 0.f) { float s3 = ss + (fmaxf(ss, .62f) - ss) * .6f * w3, v3 = vv + (.24f - vv) * .5f * w3; float r3, g3, b3; hsvRgb(hh, s3, v3, &r3, &g3, &b3);
        r += (r3 - r) * wc; g += (g3 - g) * wc; b += (b3 - b) * wc; }
      float wn = (1.f - wc) * w3 * .7f; r += (DEEP[0] - r) * wn; g += (DEEP[1] - g) * wn; b += (DEEP[2] - b) * wn;
    }
  } else { r += (INK[0] - r) * wd; g += (INK[1] - g) * wd; b += (INK[2] - b) * wd; }
  r += (CREAM[0] - r) * wl; g += (CREAM[1] - g) * wl; b += (CREAM[2] - b) * wl;
  if (!TONE_T90_780_OLD) {   // 3б) №780: звонче — середины светлее, мягкая S-кривая контраста
    float q[3] = {r, g, b};
    for (int j = 0; j < 3; j++) { float x = powf(clamp01(q[j]), TONE_T90_801_OLD ? .94f : .90f); q[j] = x + (x - .5f) * .10f * (1.f - fabsf(2.f * x - 1.f)); }   // №784: светлее
    r = q[0]; g = q[1]; b = q[2];
  }
  float y = .299f * r + .587f * g + .114f * b, dd[3] = {r - y, g - y, b - y}, k = TONE_T90_780_OLD ? 1.35f : 1.50f;   // 4) насыщенность с мягким потолком
  for (int j = 0; j < 3; j++) { float lim = dd[j] > 1e-3f ? (1.f - y) / dd[j] : dd[j] < -1e-3f ? -y / dd[j] : 1e9f; if (lim < k) k = lim; }
  if (k < 1.f) k = 1.f;
  r = clamp01(y + dd[0] * k); g = clamp01(y + dd[1] * k); b = clamp01(y + dd[2] * k);
  if (!TONE_T90_825_OLD) {   // 5) №825: анти-неон — светлое зелёно-голубое (55–200°) не кислотное: S не больше 0,82 (светящееся — своими текстурами, notoon.txt)
    rgbHsv(r, g, b, &h, &s, &v); float wn = bandW(h * 360.f, 55.f, 200.f, 10.f) * clamp01((v - .78f) / .10f);
    if (wn > 0.f && s > .82f) { s += (.82f - s) * wn; hsvRgb(h, s, v, &r, &g, &b); }
  }
  c[0] = clamp8(255.f * r); c[1] = clamp8(255.f * g); c[2] = clamp8(255.f * b);
}

// №754: пресет — таблицей 17³ с трилинейной интерполяцией (строится один раз, 14,7 КБ): toon90 напрямую в 11 раз дороже кривой (Мак 25 против 2,3 мкс
// на палитру; на PSP это +0,4–0,7 с на 533 палитры при загрузке и при переключении Cartoon). Сверка с пробой — tests/tone/toon90_check.py. TONE_T90LUT_OLD=1 — напрямую
#ifndef TONE_T90LUT_OLD
#define TONE_T90LUT_OLD 0
#endif
#define T90N 25
static unsigned char t90lut[T90N * T90N * T90N * 3]; static int t90ok;   // проверка 18.10: без кучи (.bss); №780: 25³ (45,8 КБ) вместо 17³ — рецепт с анти-болотом и S-кривой нелинейнее, 17³ не проходил гейт (95-й 15,8 против 14)
static void toon90build(void) {
  if (!t90ok) {
    for (int r = 0; r < T90N; r++) for (int g = 0; g < T90N; g++) for (int b = 0; b < T90N; b++) {
      unsigned char q[4] = {(unsigned char)(r * 255 / (T90N - 1)), (unsigned char)(g * 255 / (T90N - 1)), (unsigned char)(b * 255 / (T90N - 1)), 255}; toon90(q);
      unsigned char *o = t90lut + ((r * T90N + g) * T90N + b) * 3; o[0] = q[0]; o[1] = q[1]; o[2] = q[2];
#ifdef TONE_T90_GATE_RED
      o[0] = q[2]; o[2] = q[0];   // только красный контроль гейта tests/tone/toon90_check.py: каналы таблицы перепутаны
#endif
    }
    t90ok = 1;
  }
}
void toneToonPrep(void) { if (!TONE_T90LUT_OLD) toon90build(); }   // проверка 18.10: таблицу строить при включении Cartoon, а не в цикле загрузки
static void toon90lut(unsigned char *c) {
  toon90build();
  float fr = c[0] * ((T90N - 1) / 255.f), fg = c[1] * ((T90N - 1) / 255.f), fb = c[2] * ((T90N - 1) / 255.f);
  int r0 = (int)fr, g0 = (int)fg, b0 = (int)fb; if (r0 > T90N - 2) r0 = T90N - 2; if (g0 > T90N - 2) g0 = T90N - 2; if (b0 > T90N - 2) b0 = T90N - 2;
  float dr = fr - r0, dg = fg - g0, db = fb - b0;
  for (int k = 0; k < 3; k++) {
    #define L_(i, j, l) (float)t90lut[(((r0 + (i)) * T90N + g0 + (j)) * T90N + b0 + (l)) * 3 + k]
    float c00 = L_(0,0,0) + (L_(0,0,1) - L_(0,0,0)) * db, c01 = L_(0,1,0) + (L_(0,1,1) - L_(0,1,0)) * db;
    float c10 = L_(1,0,0) + (L_(1,0,1) - L_(1,0,0)) * db, c11 = L_(1,1,0) + (L_(1,1,1) - L_(1,1,0)) * db;
    #undef L_
    float c0 = c00 + (c01 - c00) * dg, c1 = c10 + (c11 - c10) * dg; c[k] = clamp8(c0 + (c1 - c0) * dr);
  }
}

static void toneEntry(unsigned char *c);
static int entryNoToon;   // №786: текущая палитра без шагов мультика (облака неба)
// №825 (Роб: «листва зелёная, как в их заставке: деревья сравнительно с травой насыщеннее, оптимистичнее, радостнее — со вкусом»): листва отдельно от травы —
// текстуры из раздела foliage списка уровня (notoon.txt, tools/notoon_list.py по именам: tree, leaf, bush, hedge…; tstat[4] бит 2). После мультика зелёное
// листвы (60–190°) — оттенок к 112° (сочный жёлто-зелёный вместо изумруда травы 158°), S от 0,70, светлее (к 0,64). TONE_FOLIAGE825_OLD=1 — листва как трава
#ifndef TONE_FOLIAGE825_OLD
#define TONE_FOLIAGE825_OLD 0
#endif
static int entryFoliage;
static void foliage825(unsigned char *c) {
  float r = c[0] / 255.f, g = c[1] / 255.f, b = c[2] / 255.f, h, s, v; rgbHsv(r, g, b, &h, &s, &v);
  float w = bandW(h * 360.f, 60.f, 190.f, 15.f) * clamp01((s - .12f) / .12f);
  if (w <= 0.f) return;
  h = hueTo(h, 112.f / 360.f, .80f * w); s += (fmaxf(s, .70f) - s) * .70f * w; v += (fmaxf(v, .64f) - v) * .45f * w;
  hsvRgb(h, s, v, &r, &g, &b); c[0] = clamp8(255.f * r); c[1] = clamp8(255.f * g); c[2] = clamp8(255.f * b);
}
static void toneLutInit(void) {
  if (lutReady) return;
  for (int i = 0; i < 256; i++) { float v = 255.f * powf(i / 255.f, PAL_GAMMA); v = (v - 128.f) * PAL_CONTRAST + 128.f; lut[i] = clamp8(v); }
  lutReady = 1;
}
void toneColorRGB(unsigned char *c) { if (!c) return; toneLutInit(); entryNoToon = 0; entryFoliage = 0; toneEntry(c); }
void toneColorRGBNT(unsigned char *c) { if (!c) return; toneLutInit(); entryNoToon = 1; entryFoliage = 0; toneEntry(c); entryNoToon = 0; }   // №954: как toneColorRGB, но без шагов мультика — так красится палитра текстуры неба (бит 1 tstat[4], №786)
unsigned toneKey(void) { return (unsigned)toonOn | (unsigned)satPct << 1 | (unsigned)gammaPct << 10 | (unsigned)contrastPct << 19 | (TONE_TOON90_OLD ? 1u << 28 : 0u) | (TONE_WIN892_OLD ? 1u << 29 : 0u) | (tone1026On ? 1u << 30 : 0u); }
void tonePal(unsigned char *c) {
  if (!c) return;
  toneLutInit();
  int sign1026_ = tone1026On && toneSt && (toneSt[4] & 1) && !(toneSt[4] & (8 | 4));   // №1029: земля (асфальт, разметка, всё дорожное) — не вывеска, тон как в TEST71   // №1026: вывеска (notoon, не небо) — шаги мультика и контраст как у города, но без стяжки к среднему
  entryNoToon = toneSt && (toneSt[4] & 1) && !sign1026_; entryFoliage = toneSt && (toneSt[4] & 2) && !entryNoToon;   // №786: облака неба и вывески — без шагов мультика (бит 1); №825: листва (бит 2)
  if (toonOn && !entryNoToon && !sign1026_ && !TONE_TOON90_OLD && !TONE_FLAT_OLD && toneSt && toneSt[3]) {   // №754: рисунок текстуры отступает, цвет доминирует (исходные цвета, до кривой)
    float k = TONE_TEX_UNIFORM_OLD ? toneSt[3] / 255.f : toonTexPct / 100.f; unsigned char *q = c;
    if (!TOON_FILL865_OLD && !TONE_TEX_UNIFORM_OLD) {   // №865: земля (дорога, асфальт, трава) — с рисунком; почти однотонные стены — заливкой
      if (toneSt[4] & 4) { if (k < TOON_GROUND_K) k = TOON_GROUND_K; }
      else if (toneSt[3] <= TOON_FLAT_ST) k = 0.f; }
    float lm = .299f * toneSt[0] + .587f * toneSt[1] + .114f * toneSt[2]; int keepDark = !TONE_WIN892_OLD && k > 0.f && !(toneSt[4] & (2 | 4));   // №892: не земля и не листва
    for (int i = 0; i < 256; i++, q += 4) {
      float kk = k;
      if (toneSt[4] & 32) { float ddr = q[0] - (float)toneSt[0], ddg = q[1] - (float)toneSt[1], ddb = q[2] - (float)toneSt[2]; if (ddr * ddr + ddg * ddg + ddb * ddb > 3600.f) continue; }   // №1029: статистика донора (№999) тянет только записи цвета самой дороги: жёлтая разметка и белые полосы остаются как были
      if (keepDark) { float dd = lm - (.299f * q[0] + .587f * q[1] + .114f * q[2]); if (dd > TOON_WIN_D0) kk = k + (TOON_WIN_K - k) * clamp01((dd - TOON_WIN_D0) / TOON_WIN_RAMP); }   // глубокое тёмное (стекло окон, двери) — рисунок держится сильнее общей доли
      for (int j = 0; j < 3; j++) q[j] = clamp8(toneSt[j] + (q[j] - (float)toneSt[j]) * kk);
    }
  }
  for (int i = 0; i < 256; i++, c += 4) toneEntry(c);
}
static void toneEntry(unsigned char *c) {   // одна запись палитры (и один цвет вершины, toneColorRGB): кривая, насыщенность, гамма, мультик, контраст
  float sat = satPct / 100.f;
  {
    float r = lut[c[0]], g = lut[c[1]], b = lut[c[2]], y = .299f * r + .587f * g + .114f * b, k = sat;
#if TONE_SOFT
    // мягкий потолок: наибольший множитель, при котором ни один канал не выходит за 0..255 (k = 1 — цвет как есть, всегда в пределах)
    float d[3] = {r - y, g - y, b - y};
    for (int j = 0; j < 3; j++) {
      float lim = d[j] > .5f ? (255.f - y) / d[j] : d[j] < -.5f ? -y / d[j] : 1e9f;
      if (lim < k) k = lim;
    }
    if (k < 1.f) k = 1.f;
#endif
    c[0] = clamp8(y + (r - y) * k); c[1] = clamp8(y + (g - y) * k); c[2] = clamp8(y + (b - y) * k);
    if (gammaPct != 100) {
      if (glutPct != gammaPct) { for (int j = 0; j < 256; j++) glut[j] = clamp8(255.f * powf(j / 255.f, gammaPct / 100.f)); glutPct = gammaPct; }
      c[0] = glut[c[0]]; c[1] = glut[c[1]]; c[2] = glut[c[2]];
    }
    if (toonOn && !entryNoToon && !TONE_TOON90_OLD) { if (TONE_T90LUT_OLD) toon90(c); else toon90lut(c); if (entryFoliage && !TONE_FOLIAGE825_OLD) foliage825(c); }   // №754: пресет «Мультик 90-х» (№786: не облака)
    if (contrastPct != 100 && !entryNoToon) { float kc = contrastPct / 100.f; for (int j = 0; j < 3; j++) c[j] = clamp8(128.f + ((float)c[j] - 128.f) * kc); }   // №777: контраст из настроек (№786: облака — как прежде)
  }
}

// ---- блоки-источники палитр (перечитывание при смене настройки)
#define TONE_BLK_MAX 16
typedef struct { const unsigned char *base; unsigned size, file, off; } ToneBlk;
static ToneBlk blk[TONE_BLK_MAX];

static unsigned blkGen;   // счётчик изменений набора блоков: фоновая смена тона (main_save.h toneJob) начинает заново, если набор менялся
unsigned toneBlkGen(void) { return blkGen; }
int toneBlkHas(const unsigned char *pal) {
  for (int i = 0; i < TONE_BLK_MAX; i++) if (blk[i].base && pal >= blk[i].base && pal + 1024 <= blk[i].base + blk[i].size) return 1;
  return 0;
}
void toneBlkDel(const void *base) {
  blkGen++;
  for (int i = 0; i < TONE_BLK_MAX; i++) if (blk[i].base == base) memset(&blk[i], 0, sizeof blk[i]);
}
void toneBlkClear(void) { blkGen++; memset(blk, 0, sizeof blk); }
void toneBlkAdd(const void *base, unsigned size, unsigned file, unsigned off) {
  if (!base || !size) return;
  toneBlkDel(base); blkGen++;
  for (int i = 0; i < TONE_BLK_MAX; i++)
    if (!blk[i].base) { blk[i].base = base; blk[i].size = size; blk[i].file = file; blk[i].off = off; return; }
}
int toneRedo(unsigned char *pal, ToneRead rd, void *user) {
  for (int i = 0; i < TONE_BLK_MAX; i++) {
    const ToneBlk *b = &blk[i];
    if (!b->base || pal < b->base || pal + 1024 > b->base + b->size) continue;
#ifdef TONE_INPLACE
    (void)rd; (void)user;   // отрицательный контроль гейта tests/tone: кривая поверх уже тонированной палитры (как palTone до 01.10)
#else
    if (rd(user, b->file, b->off + (unsigned)(pal - b->base), pal, 1024)) return -2;
#endif
    tonePal(pal);
    return 0;
  }
  return -1;
}

// проверка 18.10: исходная палитра из файла в dst без кривой (для подсчёта среднего цвета текстуры, когда Cartoon включили посреди уровня); -1 — блока нет, -2 — чтение не удалось
int toneReadOrig(const unsigned char *pal, unsigned char *dst, ToneRead rd, void *user) {
  for (int i = 0; i < TONE_BLK_MAX; i++) {
    const ToneBlk *b = &blk[i];
    if (!b->base || pal < b->base || pal + 1024 > b->base + b->size) continue;
    return rd(user, b->file, b->off + (unsigned)(pal - b->base), dst, 1024) ? -2 : 0;
  }
  return -1;
}

// то же, но результат в dst (подготовка в фоне, без записи в живую палитру): исходник из файла, затем кривая; -1 — блока нет, -2 — чтение не удалось
int toneRedoTo(const unsigned char *pal, unsigned char *dst, ToneRead rd, void *user) {
  for (int i = 0; i < TONE_BLK_MAX; i++) {
    const ToneBlk *b = &blk[i];
    if (!b->base || pal < b->base || pal + 1024 > b->base + b->size) continue;
    if (rd(user, b->file, b->off + (unsigned)(pal - b->base), dst, 1024)) return -2;
    tonePal(dst);
    return 0;
  }
  return -1;
}
