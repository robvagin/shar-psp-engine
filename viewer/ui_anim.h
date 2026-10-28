// Чистая математика анимаций HUD: одна и та же для рендера PSP (viewer/ui.c) и проверок на Маке (tests/ui/anim.c).
// Без GU и без часов системы: всё — функции времени t (секунды) и шага dt.
//
// ОТКУДА ЧИСЛА. В проектах Scrooby нет ни одной ключевой кривой (0x121000 — 0 во всех *.p3d, knowledge/facts/
// ps2-scrooby-animation-scope.yaml), у ActionButton одна картинка «action» без скорости и кривой пульса
// (ps2-action-fire-sprite-data.yaml). Поэтому пульс △, всплывание уведомлений и скорость кольца Hit & Run —
// ПРАВИЛА ПОРТА, сверить с эталоном PS2 (клип с △ у двери и «STAGE COMPLETE»). Цвета и кадры мигалки — из данных
// Hud.pag (ниже у каждого числа — откуда оно).
#ifndef SHAR_UI_ANIM_H
#define SHAR_UI_ANIM_H
#include <math.h>

#define UI_PI 3.14159265358979f

// ---- 1. Пульс подсказки △ (Hud.pag Foreground/ActionButton, тег ACTION): масштаб от центра элемента --------------
// правило порта, сверить с эталоном PS2:
//  появление — пружина от 0.5 к 1 (затухание 0.4, 2.5 Гц): перелёт ~13%, успокоение ~0.6 с («упругая инерция»);
//  дыхание — 1 ± 8% синусом с периодом 1.0 с: △ на PSP 28 px -> ±2.2 px, глазом видно, соседей не задевает;
//  амплитуда дыхания нарастает за первые 0.5 с, чтобы не спорить с пружиной появления.
#define UI_PULSE_FROM   0.5f
#define UI_PULSE_ZETA   0.4f
#define UI_PULSE_HZ     2.5f
#define UI_PULSE_AMP    0.08f
#define UI_PULSE_PERIOD 1.0f
#define UI_PULSE_RAMP   0.5f
static inline float uiSpring(float t, float from, float zeta, float hz) {   // отклик пружины на ступеньку from -> 1
  if (t <= 0.f) return from;
  float w = 2.f * UI_PI * hz, wd = w * sqrtf(1.f - zeta * zeta), e = expf(-zeta * w * t);
  return 1.f - (1.f - from) * e * (cosf(wd * t) + zeta / sqrtf(1.f - zeta * zeta) * sinf(wd * t));
}
// Правка К-29 (Наблюдатель, 10.10: видео PS2 yt-bziOipI4gDI 38:04–38:06, 60 к/с, площадь △ по 120 кадрам): период пульса ~0,50 с (пики на кадрах 4, 35, 64, 93),
// размер плавно ходит от ~0,6 до ~1,0 диаметра (оценка по трём кадрам на глаз; регион и темп видео не определены). ПРАВИЛО ПОРТА по этой оценке:
// 0,8 ± 0,2 с периодом 0,5 с, появление — та же пружина (от 0,5 к 1, масштаб среднего 0,8). UI_PULSE_PS2_OLD=1 — прежнее: 1 ± 0,08, период 1,0 с
#ifndef UI_PULSE_PS2_OLD
#define UI_PULSE_PS2_OLD 0
#endif
#define UI_PULSE2_MEAN   0.8f
#define UI_PULSE2_AMP    0.2f
#define UI_PULSE2_PERIOD 0.5f
// Правка Роба 512/542/568 (11–12.10): карточка миссии и руки (штанги с болтами)
// Въезд: карточка и руки приезжают слева за UI_BR_IN (0,40 с) по spline uiBriefInProg(t).
// Покой: карточка по центру, руки (штанги с болтами) по бокам видны целиком от краёв экрана до болтов (showL=1, showR=1).
// Выезд после Continue: карточка и руки уезжают вправо за UI_BR_EXIT (0,45 с) с ускорением ~ t², затухая по альфе.
// UI_BRIEF_ANIM_OLD=1 — как до анимации (всё на месте сразу).
#ifndef UI_BRIEF_ANIM_OLD
#define UI_BRIEF_ANIM_OLD 0
#endif
#ifndef BRIEF_ARMS_OLD
#define BRIEF_ARMS_OLD 0
#endif
#define UI_BR_IN 0.40f
#ifndef BRIEF_TEXT_LATE_OLD
#define BRIEF_TEXT_LATE_OLD 0   // 1 — текст карточки едет вместе с рамкой (как PS2 и до 16.10)
#endif
#ifndef BRIEF_TEXT_FADE
#define BRIEF_TEXT_FADE 0.15f   // с: проявление текста после остановки карточки
#endif
#ifndef BRIEF_PS2_TIMING_OLD
#define BRIEF_PS2_TIMING_OLD 0   // 1 — короткие вход/выезд (рука 0,40 / 0,45 с), как в TEST17
#endif
#define UI_BR_EXIT (BRIEF_PS2_TIMING_OLD ? 0.45f : 0.95f)
static inline float uiBriefInProg(float t) {   // доля пути карточки при въезде, t с
  static const float k[9] = {0.f, .115f, .237f, .415f, .532f, .677f, .824f, .948f, 1.f};
  if (t <= 0.f) return 0.f; if (t >= UI_BR_IN) return 1.f;
  float u = t / 0.05f; int i = (int)u; float f = u - (float)i; return k[i] + (k[i + 1] - k[i]) * f;
}
// pose: dx карточки, левой и правой руки (px PSP от покоя; руки видны при *showL/*showR), прозрачность карточки и затемнения.
// leaving: 0 — вход, t от появления; 1 — выход, t от Continue
// cardR/cardL — правый/левый край карточки, armLR — правый край левой руки, armRL — левый край правой руки (px PSP в покое)
static inline void uiBriefPose(int leaving, float t, float cardL, float cardR, float armLR, float armRL, float scrW,
                               float *card, float *armL, float *armR, int *showL, int *showR, float *cardA, float *backA) {
  *card = *armL = *armR = 0.f; *showL = 1; *showR = 1; *cardA = *backA = 1.f;
  if (!leaving) {
    if (t < UI_BR_IN) {
      float p = uiBriefInProg(t);
      *card = -(cardR + 16.f) * (1.f - p);
      *armL = *card;
      *armR = *card;
    }
    // №612 (запись PS2, ПС2_БРИФИНГ_РУКИ_кадры): в покое рук нет; на въезде карточку привозит рука со своей стороны (у которой болт
    // у левого края), на выезде забирает рука с правой. BRIEF_ARMS_OLD=1 — прежнее (обе видны всегда, едут вместе с карточкой)
    if (!BRIEF_ARMS_OLD) {
      *showR = 0;
      if (BRIEF_PS2_TIMING_OLD) { if (t >= UI_BR_IN) *showL = 0; }
      else if (t >= UI_BR_IN) {   // запись PS2 (4_карточка_руки, вход): рука-зажим остаётся у карточки ещё ~0,2 с и уходит влево к 0,85 с
        if (t >= 0.85f) *showL = 0;
        else if (t > 0.60f) { float u = (t - 0.60f) / 0.25f; *armL = -(armLR + 16.f) * u * u; }
      }
    }
    return;
  }
  float far = scrW - cardL + 20.f;
  if (!BRIEF_PS2_TIMING_OLD && !BRIEF_ARMS_OLD) {
    // запись PS2 (4_карточка_руки, выезд, ~0,95 с): правая рука выезжает к карточке за 0,38 с, карточка стоит до 0,56 с, затем с ускорением вправо
    // поверх мира; затемнение уходит за 0,05–0,55 с, карточка не бледнеет
    float u = t / 0.38f; if (u > 1.f) u = 1.f; u = 1.f - (1.f - u) * (1.f - u);
    float tau = (t - 0.56f) / (UI_BR_EXIT - 0.56f); if (tau < 0.f) tau = 0.f; if (tau > 1.f) tau = 1.f;
    *card = far * tau * tau; *armL = *card; *showL = 0;
    *armR = *card + (1.f - u) * (scrW - armRL + 8.f);
    *cardA = t >= UI_BR_EXIT ? 0.f : 1.f;
    if (t > 0.05f) *backA = t >= 0.55f ? 0.f : 1.f - (t - 0.05f) / 0.50f;
    return;
  }
  float tau = t / UI_BR_EXIT; if (tau > 1.f) tau = 1.f;
  *card = far * tau * tau;
  *armL = *card;
  *armR = *card;
  if (!BRIEF_ARMS_OLD) {   // правая рука сперва выезжает к карточке (первые 0,12 с, от края экрана до зажима), потом увозит её
    *showL = 0;
    float u = t / 0.12f; if (u > 1.f) u = 1.f; u = 1.f - (1.f - u) * (1.f - u);
    *armR = *card + (1.f - u) * (scrW - armRL + 8.f);
  }
  if (t > 0.25f) *cardA = t >= UI_BR_EXIT ? 0.f : 1.f - (t - 0.25f) / (UI_BR_EXIT - 0.25f);
  if (t > 0.10f) *backA = t >= UI_BR_EXIT ? 0.f : 1.f - (t - 0.10f) / (UI_BR_EXIT - 0.10f);
}
static inline float uiPulseScale(float t) {   // t — секунды с появления подсказки
  if (t < 0.f) t = 0.f;
  float ramp = t < UI_PULSE_RAMP ? t / UI_PULSE_RAMP : 1.f;
  if (!UI_PULSE_PS2_OLD)
    return UI_PULSE2_MEAN * uiSpring(t, UI_PULSE_FROM, UI_PULSE_ZETA, UI_PULSE_HZ) + UI_PULSE2_AMP * ramp * sinf(2.f * UI_PI * t / UI_PULSE2_PERIOD);
  return uiSpring(t, UI_PULSE_FROM, UI_PULSE_ZETA, UI_PULSE_HZ) + UI_PULSE_AMP * ramp * sinf(2.f * UI_PI * t / UI_PULSE_PERIOD);
}

// ---- 2. Всплывающие уведомления (Hud.pag: MissionProgress/StageComplete, MissionComplete, ItemCollected, Wasps...) --
// правило порта, сверить с эталоном PS2: вход 0.35 с — масштаб «с перелётом» (easeOutBack, c = 1.70158: перелёт ~10%)
// от 0.4 к 1, прозрачность 0 -> 1 за первые 0.12 с; выход 0.2 с — масштаб 1 -> 0.6 (easeInCubic) и прозрачность -> 0.
// Рамка задания (Message) — мягче: от 0.8, выход — только прозрачность (она у края экрана и держится долго).
enum { UI_POP_BANNER = 0, UI_POP_BOX = 1, UI_POP_SLIDE = 2 };
// ---- 2в. Строка задания внизу (пилюля слева, тег OBJECTIVE), №516 — ЗАМЕР PS2 (Наблюдатель, docs/handoff/ПУЗЫРЬ_АНИМАЦИЯ_PS2_2026-10-11.md, запись
// bziOipI4gDI 1:43 «GET INTO YOUR VEHICLE», 60 к/с, шаги по 2 кадра = 30 в секунду): ширина полная с первого кадра, растёт только ВЫСОТА из тонкой
// полосы вокруг своего центра: 0,54 → 0,70 → 0,85 → 0,97 → 1,0 (4 шага, ≈0,12 с), текст виден сразу и сжат по высоте вместе с пилюлей, заливка не
// проявляется; уходит за ОДИН кадр, без сжатия и затухания. Раньше: масштаб 0,8 → 1 с перелётом 0,35 с + прозрачность и выход прозрачностью 0,2 с (UI_POP_BOX).
// BUBBLE_ANIM_OLD=1 — прежнее. Подсказка с портретом (Барт, из центра экрана, 0,55 → 1,0 за 5 шагов) — другой элемент, здесь не затронут.
#ifndef BUBBLE_ANIM_OLD
#define BUBBLE_ANIM_OLD 0
#endif
enum { UI_POP_PILL = 3 };
#ifndef BUBBLE_TEXT_LATE_OLD
#define BUBBLE_TEXT_LATE_OLD 0
#endif
// №638 (Роб 16.10, повтор: «рамка сначала вдвое уже текста, растёт, висит в воздухе»; решение по записи PS2 ps2-bubble-open-anim.yaml, кадры 28..36 при 50 к/с):
// рамка вместе с текстом и значком растёт ЕДИНЫМ масштабом (без растяжения по одной оси) вокруг центра своей конечной рамки (ширина и высота посчитаны до показа),
// 0,25 → 0,53 → 0,60 → 0,70 → 0,77 → 0,86 → 0,94 → 1,0 за 8 кадров (0,16 с); прозрачность 0,3 → 1 за первые 0,06 с; текст и значок с первого кадра, не отдельно.
// BUBBLE_UNIFORM_OLD=1 — прежнее (только высота 0,54 → 1, текст позже).
#ifndef BUBBLE_UNIFORM_OLD
#define BUBBLE_UNIFORM_OLD 0
#endif
// №942 (Роб 24.10 вечер: «текст сначала узкой колонкой справа в пузыре, потом разъезжается; так не надо — текст всё время во всю ширину пузыря, появляется чуть позже после рамки,
// фейдом или минимальным слайдом справа налево; анимация иногда лагает; кружок аватара переезжает по ходу появления»): рамка растёт единым масштабом, как раньше (№638, замер PS2),
// а СОДЕРЖИМОЕ пузыря (текст, аватар) не масштабируется вовсе: стоит в конечном месте и конечной раскладки, не рисуется, пока рамка не раскрылась (BUB_CONTENT_DELAY),
// затем проявляется за BUB_CONTENT_FADE; текст при этом съезжает справа налево на BUB_CONTENT_SLIDE px (аватар — на месте). BUBBLE_TEXT942_OLD=1 — прежнее (содержимое растёт вместе с рамкой).
// Лаг: возраст пузыря (bubAge в ui.c) копит шаги не больше BUB_DT_CAP — подвисший кадр (загрузка миссии) не съедает анимацию; тот же шаг у полёта значка в угол (BUBBLE_LAG942_OLD=1 — часы как раньше)
#ifndef BUBBLE_TEXT942_OLD
#define BUBBLE_TEXT942_OLD 0
#endif
#ifndef BUBBLE_LAG942_OLD
#define BUBBLE_LAG942_OLD 0
#endif
#ifndef BUBBLE_WRAP942_OLD
#define BUBBLE_WRAP942_OLD 0   // 1 — перенос строк пузыря (fitW, закон v1) считается заново при каждом вызове, 2-3 раза за кадр; 0 — один раз на текст и ширину
#endif
#define UI_BUB_CONTENT_DELAY 0.16f   // с: рамка раскрылась (8 шагов по 0,02 с)
#define UI_BUB_CONTENT_FADE  0.14f   // с: проявление текста и аватара
#define UI_BUB_CONTENT_SLIDE 6       // px: текст стоит правее на столько в начале проявления
#define UI_BUB_DT_CAP        0.04f   // с: самый большой шаг возраста за кадр (25 к/с); подвисший кадр не пропускает шаги
#define UI_BUB_STEPS 8
static const float UI_BUB_K[UI_BUB_STEPS] = {0.25f, 0.525f, 0.595f, 0.70f, 0.77f, 0.86f, 0.935f, 0.998f};
// №992 (Роб 26.10 после TEST70: «бабл появляется прикольно, но всё равно не синхронизирован с текстом. Меньше анимации в появлении у бабла и больше синхронизации с текстом»):
// появление короче и спокойнее — 3 шага по 1/50 с (0,06 с) вместо 8 (0,16 с), старт 0,88 вместо 0,25; текст и аватар едут ВМЕСТЕ с рамкой (тот же масштаб и та же прозрачность, без задержки
// BUB_CONTENT_DELAY и без сдвига справа налево): с первого кадра виден весь пузырь. Раскладка текста конечная (ширина зоны текста не сжимается), кегль и место не тронуты.
// BUBBLE992_OLD=1 / файл bubble992old рядом с EBOOT / flag bubble992 0 (uiBubble992Old) — как в TEST70 (№942: 8 шагов, содержимое после раскрытия рамки)
#ifndef BUBBLE992_OLD
#define BUBBLE992_OLD 0
#endif
extern int uiBubble992Old;   // ui.c: 1 — появление как в TEST70
// №1040 (Роб 26.10 на TEST72: «ты очень усложнил анимацию message bubble, при появлении всё лагает; сделай простое появление пузыря с простым появлением текста, как в оригинале, но под PSP»): одно простое движение —
// рамка вместе с текстом и значком растёт единым масштабом 0,88 → 0,94 → 0,98 → 1 тремя шагами по 1/50 с (замер PS2 из №638: ступени, без перелёта), БЕЗ ступенчатой прозрачности и БЕЗ отдельного фейда текста
// (№1004) и сдвига: текст и аватар едут ровно тем же преобразованием, что рамка, с первого кадра. Перенос строк и ширина считаются один раз на сообщение (кэш раскладки на несколько сообщений, не по одному), не по кадрам
// появления. BUBBLE1040_OLD=1 / файл bubble1040old / «flag bubble1040 0» — как в TEST72 (№992 + №1004)
#ifndef BUBBLE1040_OLD
#define BUBBLE1040_OLD 0
#endif
extern int uiBubble1040;   // ui.c: 1 — простое появление
#define UI_BUB992_STEPS 3
static const float UI_BUB992_K[UI_BUB992_STEPS] = {0.88f, 0.94f, 0.98f};
static inline float uiBubScale(float t) {
  int i = (int)floorf((t < 0.f ? 0.f : t) * 50.f + 1e-4f);
  if (!uiBubble992Old) return i >= UI_BUB992_STEPS ? 1.f : UI_BUB992_K[i];
  return i >= UI_BUB_STEPS ? 1.f : UI_BUB_K[i];
}
#define UI_PILL_STEPS 4
static const float UI_PILL_K[UI_PILL_STEPS] = {0.54f, 0.70f, 0.85f, 0.97f};   // высота пилюли по шагам 1/30 с, затем 1,0
static inline float uiPillScale(float t) {   // t — секунды с показа
  int i = (int)floorf((t < 0.f ? 0.f : t) * 30.f + 1e-4f);
  return i >= UI_PILL_STEPS ? 1.f : UI_PILL_K[i];
}


// ---- 2б. Счёт монет (Coins.pag, тег COINS) — ЗАМЕР эталона PS2 (правка Роба 51; YouTube bziOipI4gDI, кадры 4 в секунду:
// 3:46–3:55, 4:53–5:05): счёт не висит всегда — при сборе въезжает сверху с проявлением (≈0.25 с), держится 2 с после
// последней монеты и уезжает вверх (≈0.25 с). Порядок чисел: 4:53.75 «21» въехал, 4:55.75 ещё виден, 4:56.0 уехал;
// 4:58.75 «26» последняя, 5:00.75 уезжает; 5:03.25 «27», 5:05.25 уехал.
#define UI_COIN_IN   0.25f
#define UI_COIN_OUT  0.25f
#define UI_COIN_HOLD 2.0f
#define UI_POP_IN   0.35f
#define UI_POP_FADE 0.12f
#define UI_POP_OUT  0.20f
static inline float uiEaseOutBack(float x) { const float c1 = 1.70158f, c3 = c1 + 1.f; x -= 1.f; return 1.f + c3 * x * x * x + c1 * x * x; }
// on: 1 — показан с момента t (секунды с показа), 0 — снят, t — секунды со снятия. Выход: масштаб и альфа 0..1.
// Возвращает 0, когда рисовать уже нечего (выход закончен).
static inline int uiPopState(int kind, int on, float t, float *scale, float *alpha) {
  if (kind == UI_POP_PILL) {   // строка задания: масштаб — по высоте (ui.c: xf.sy), альфа 1; снята — сразу нет (один кадр)
    if (!BUBBLE_UNIFORM_OLD) { *scale = on ? uiBubScale(t) : 1.f; *alpha = (on && t < 0.06f && !uiBubble1040) ? 0.3f + 0.7f * (t < 0.f ? 0.f : t) / 0.06f : 1.f; return on ? 1 : 0; }
    *alpha = 1.f; *scale = on ? uiPillScale(t) : 1.f; return on ? 1 : 0;
  }
  float from = kind == UI_POP_BOX ? 0.8f : 0.4f;
  if (t < 0.f) t = 0.f;
  if (kind == UI_POP_SLIDE) {   // счёт монет: масштаб 1, «альфа» = доля выезда (ui.c сдвигает группу вверх на 1 - alpha)
    *scale = 1.f;
    if (on) { float x = t < UI_COIN_IN ? t / UI_COIN_IN : 1.f; *alpha = 1.f - (1.f - x) * (1.f - x) * (1.f - x); return 1; }
    if (t >= UI_COIN_OUT) { *alpha = 0.f; return 0; }
    { float x = t / UI_COIN_OUT; *alpha = 1.f - x * x * x; }
    return 1;
  }
  if (on) {
    float x = t < UI_POP_IN ? t / UI_POP_IN : 1.f;
    *scale = from + (1.f - from) * uiEaseOutBack(x);
    *alpha = t < UI_POP_FADE ? t / UI_POP_FADE : 1.f;
    return 1;
  }
  if (t >= UI_POP_OUT) { *scale = 1.f; *alpha = 0.f; return 0; }
  float x = t / UI_POP_OUT;
  *scale = kind == UI_POP_BOX ? 1.f : 1.f - 0.4f * x * x * x;
  *alpha = 1.f - x;
  return 1;
}

// ---- 2г. Подсказка обучения с портретом Барта (Tutorial.scr, UI_SCREEN_TUTORIAL), №516 — ЗАМЕР PS2 (Наблюдатель, ПУЗЫРЬ_АНИМАЦИЯ_PS2_2026-10-11.md §2, записи
// 4:17 и 33:36, 60 к/с, шаги по 2 кадра = 30 в секунду): весь пузырь (рамка, портрет, текст, подписи) растёт ЕДИНЫМ масштабом из центра экрана
// ≈0,56 → 1,0 за 4–5 шагов (0,13–0,17 с), почти линейно; текст и портрет с первого кадра, без прозрачности; выход после «Continue» — обратным масштабом
// 1,0 → 0,91 → 0,83 → 0,74 → 0,56 за 4 шага и пропадание за один кадр (до нуля не сжимается), кадр игры за пузырём открыт сразу.
// TUT_ANIM_OLD=1 — прежнее: появляется и пропадает сразу целиком.
#ifndef TUT_ANIM_OLD
#define TUT_ANIM_OLD 0
#endif
static const float UI_TUT_IN_K[4]  = {0.56f, 0.70f, 0.82f, 0.92f};   // масштаб по шагам 1/30 с, затем 1,0
static const float UI_TUT_OUT_K[4] = {0.91f, 0.83f, 0.74f, 0.58f};   // после закрытия, по шагам 1/30 с; дальше пузыря нет
static inline float uiTutEnterScale(float t) {   // t — секунды с показа
  int i = (int)floorf((t < 0.f ? 0.f : t) * 30.f + 1e-4f);
  return i >= 4 ? 1.f : UI_TUT_IN_K[i];
}
static inline int uiTutExitScale(float t, float *k) {   // t — секунды с закрытия; 0 — рисовать уже нечего
  int i = (int)floorf((t < 0.f ? 0.f : t) * 30.f + 1e-4f);
  if (i >= 4) { *k = 1.f; return 0; }
  *k = UI_TUT_OUT_K[i]; return 1;
}

// ---- 3. Кольцо Hit & Run вокруг радара (Hud.pag HitNRunMeter: HnRMeter — белое кольцо hrmetter, красится вершинами) --
// Показ догоняет шкалу с постоянной скоростью — последовательный прогресс-бар: сбитый предмет (+7.5%, viewer/game.c)
// дорисовывается за 0.125 с, а не скачком; остывание (-3%/с) медленнее скорости и идёт ровно за шкалой.
// правило порта, сверить с эталоном PS2: 0.6 круга в секунду.
#define UI_HNR_RATE 0.6f
static inline float uiMeterFollow(float shown, float target, float dt) {
  if (!(dt > 0.f)) return shown;
  float d = target - shown, m = UI_HNR_RATE * dt;
  return fabsf(d) <= m ? target : shown + (d > 0.f ? m : -m);
}
// Мигалка над радаром (Hud.pag HitNRun: hitnrun0 — горит красная половина, hitnrun1 — синяя, hitnrun2 — обе погашены) и цвет
// заливки кольца (белое hrmetter.png 255,255,255 красится вершинами — цвет на экране = оттенок). Правка Роба 241 (03.10):
// «до конца по логике оригинала — на ¾ бар начинает мигать, чтобы было видно периферийным зрением». Замер оригинала:
//   запись PS2 build/ps2ref/external/yt-bziOipI4gDI.mp4 (60 к/с) — 8:58.75–9:07.75 и 25:54.0–26:30.7;
//   близнец tw_hnr_text (PCSX2, build/twins/o4/tw_hnr_text/png, PAL 50 к/с, шкала 100 записью на кадре 150).
// Три режима:
//   ПОКОЙ — заливка жёлтая, мигалка не горит (кадр 2).
//   ПРЕДУПРЕЖДЕНИЕ — шкала выше 78 и полиции нет: заливка на своём уровне по очереди красная и чёрная, мигалка не горит
//     (запись 8:58.75: удар 74 -> 82 и мигание с того же кадра; 25:54.0: 73 -> 78; близнец: кадры 151–184 при шкале 100,
//     пока полиция не появилась). Порог 78 — оракул b2_warn (флаг 0x612208 срабатывает при 79, не при 77). В записи после
//     разговора с Барни при 74 мигания нет — значит мигает по уровню «выше 78», а не по защёлке 78/60 (вопрос близнецу).
//   ПОГОНЯ (полиция в игре, alarm) — заливка на своём уровне (спадает 6/с) по очереди красная и синяя, мигалка в фазе:
//     красное кольцо — горит красная половина (близнец кадры 187–268: синий с кадра 187 — полиция появилась через 0,74 с;
//     b3_chase 1000/1200/1400 — кольцо красное при 67/43/19, не только полное).
// Период 0,5 с, половина на половину: запись — 18 периодов за 9,0 с (предупреждение) и 34 за 17,1 с (погоня), фазы по
// 14–16 кадров записи; близнец — по 12–13 кадров PAL. Часы мигания — elapsed (общие для кольца и мигалки).
#define UI_HNR_PERIOD 0.5f
#define UI_HNR_WARN   .78f
static inline int uiHnrLit(float t) { return fmodf(t > 0.f ? t : 0.f, UI_HNR_PERIOD) < UI_HNR_PERIOD * .5f; }
static inline int uiSirenFrame(int alarm, float t) { return alarm ? (uiHnrLit(t) ? 0 : 1) : 2; }
// Цвета (ABGR). Жёлтый — Роб 28.09 (жёлтый HUD из данных — ItemsCount Hud.pag 254,230,20; запись PS2: 222,209,41 после сжатия).
// Красный, синий, чёрный — заливка кольца у близнеца tw_hnr_text (кадр без сжатия): 197,16,10 (кадры 151–154, 172–178, 196–205),
// 16,28,147 (190–193, 208–214), 0,0,0 (157–166, 184); запись PS2 после сжатия: 181,33,43 / 48,47,146 / 38,32,33.
#define UI_HNR_YELLOW 0xff14e6feu
// Дорожка кольца (Hud.pag HnRMeterBgd — та же картинка hrmetter под заливкой; в данных она тёмно-синяя 17,31,161, Роб 27.09 —
// синюю не рисовать): Роб 28.09 по видео с PSP (2:48, правка 36) — «заливка не такая прозрачная, полужёлтая»: пустая часть
// кольца — тот же жёлтый HUD наполовину прозрачный (альфа 0x80), заполненная — жёлтый целиком поверх. ПРАВИЛО ПОРТА (слово Роба)
#define UI_HNR_TRACK  0x2814e6feu   // правка Роба 65 (29.09): «кольцо жёлтое — бледнее или как в оригинале» (запись PS2: пустое кольцо серое) — альфа 0x80 -> 0x28
#define UI_HNR_RED    0xff0a10c5u   // 197,16,10
#define UI_HNR_BLUE   0xff931c10u   // 16,28,147
#define UI_HNR_DARK   0xff000000u   // 0,0,0
static inline unsigned uiHnrColor(int alarm, float heat01, float t) {
  if (alarm) return uiHnrLit(t) ? UI_HNR_RED : UI_HNR_BLUE;
  if (heat01 > UI_HNR_WARN) return uiHnrLit(t) ? UI_HNR_RED : UI_HNR_DARK;
  return UI_HNR_YELLOW;
}
// служебно (гейт кольца, ui.c uiHnrScript и tests/ui/anim.c): шкала по сценарию — пусто 0.5 с, удары +7.5% (как сбитый
// предмет, viewer/game.c) раз в 0.25 с до полного (с 11-го удара — 82.5%, выше 78: предупреждение), 2 с погони (полиция:
// alarm) со спадом погони 6%/с, сброс до 50% без полиции, остывание -3%/с (viewer/game.c)
static inline float uiHnrScriptHeat(float tau, int *pursuit) {
  float h = 0.f, tFull = .5f + 13 * .25f;                   // 14-й удар — полная шкала
  if (tau >= .5f) { int k = (int)((tau - .5f) / .25f) + 1; h = k * .075f; if (h > 1.f) h = 1.f; }
  *pursuit = tau >= tFull && tau < tFull + 2.f;
  if (*pursuit) h = 1.f - .06f * (tau - tFull);
  if (tau >= tFull + 2.f) { h = .5f - .03f * (tau - tFull - 2.f); if (h < 0.f) h = 0.f; }
  return h;
}
// ---- Место в гонке «1/2» (аудит 09.10 п. 25, knowledge/facts/ps2-race-position.yaml): запись PS2 yt-bziOipI4gDI.mp4 4:58 —
// «1st/2»: спрайтовое поле Hud.pag MissionOverlays/Position (тот же шрифт и прямоугольник, что у Lap «1/3», в шрифте есть «/»)
// пишет «место/участников», надстрочное «st/nd» — отдельное поле PositionOrdinal. UI_POS_RACERS_OLD=1 — прежнее: только место.
// Пробел перед «/»: у PS2 (кадр 4:58, заливка по столбцам, px PSP от левого края «1») косая черта +34…+46, «2» +51…+68, «st»
// между «1» и чертой; у порта шаг букв по чернилам (ui_export num_glyphs) — без пробела черта +16…+26 и закрывает «st», с пробелом
// (шаг 0,6 клетки) +25…+35, «2» +47…+63 — ближе к PS2 и «st» видно (подгонка по кадру PS2, гейт tests/ui/racepos_n.sh)
#ifndef UI_POS_RACERS_OLD
#define UI_POS_RACERS_OLD 0
#endif
#include <stdio.h>
static inline int uiPosText(char *b, int n, int pos, int racers) {
#ifndef C3_RACE_OFFROAD_OLD
  if (pos < 0) return racers > 0 ? snprintf(b, (size_t)n, "? /%d", racers) : snprintf(b, (size_t)n, "?");
#endif
  return racers > 0 && !UI_POS_RACERS_OLD ? snprintf(b, (size_t)n, "%d /%d", pos, racers) : snprintf(b, (size_t)n, "%d", pos);
}

// №517 (Роб 11.10: огонёк фитиля «клацает, как поворотник»): огонь мигает как живой — смена малой/большой картинки раз в 50–150 мс (случайно, 6–10 раз в секунду),
// каждый раз ещё размер и яркость ±10 %. Всё от часов экрана t (не от числа кадров загрузки); состояние сбрасывается, когда часы идут назад (новый экран загрузки).
// FUSE_FLICKER_OLD=1 — прежнее: 5 слотов по 12,5/с строго по кругу
#ifndef FUSE_FLICKER_OLD
#define FUSE_FLICKER_OLD 0
#endif
static inline int fireFlicker(float t, int count, float *scale, float *alpha) {
  static float nextT = -1.f, lastT; static int big; static unsigned rng = 0x9e3779b9u; static float sc = 1.f, al = 1.f;
  if (t < lastT - .5f || nextT < 0.f) { nextT = t; big = 0; sc = al = 1.f; }
  lastT = t;
  if (t >= nextT) {   // новая вспышка: картинка, размер, яркость, длительность 50–150 мс
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; float r0 = (rng & 0xffffu) * (1.f / 65536.f);
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; float r1 = (rng & 0xffffu) * (1.f / 65536.f);
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; float r2 = (rng & 0xffffu) * (1.f / 65536.f);
    big = !big; sc = .9f + .2f * r0; al = .9f + .1f * r1;
    nextT += .05f + .1f * r2; if (nextT <= t) nextT = t + .05f;
  }
  *scale = sc; *alpha = al;
  return big ? (count > 3 ? 3 : count - 1) : 0;   // слоты 0–2 малая картинка, 3–4 большая
}

// №631: рамка меню паузы (запись PS2, 50 кадр/с): открытие 0,16 с — масштаб из центра 0,40…1 (ширина 217, 261, 286, 312, 339, 363, 416 px из 416 на кадрах 30–36), закрытие 0,06 с: 1, 0,80, 0,74.
// closing 0/1, t — секунды от открытия/закрытия; возврат 1 — анимация идёт (scale, alpha заполнены), 0 — кончилась
#define UI_PAUSE_OPEN_S .16f
#define UI_PAUSE_CLOSE_S .06f
static inline int uiPausePose(int closing, float t, float *scale, float *alpha) {
  static const float so[9] = {.40f, .46f, .52f, .63f, .69f, .75f, .81f, .87f, 1.f}, sc[3] = {1.f, .80f, .74f};
  if (t < 0.f) t = 0.f;
  if (!closing) {
    if (t >= UI_PAUSE_OPEN_S) return 0;
    float f = t / .02f; int i = (int)f; if (i > 7) i = 7; float u = f - (float)i; if (u > 1.f) u = 1.f;
    *scale = so[i] + (so[i + 1] - so[i]) * u; *alpha = .2f + .8f * (t / UI_PAUSE_OPEN_S); return 1;
  }
  if (t >= UI_PAUSE_CLOSE_S) return 0;
  int i = (int)(t / .02f); if (i > 2) i = 2; *scale = sc[i]; *alpha = 1.f - .7f * (t / UI_PAUSE_CLOSE_S); return 1;
}

#endif
