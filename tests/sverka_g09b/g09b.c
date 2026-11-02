// Сверка с исходником, окно g09b-menu, круг 6 (02.11.2026): проверки правил меню на CPU Мака (viewer/menu_rules.h — те же функции зовёт игра).
// Каждый случай — ожидание из отчётов сверки (числа и порядок оригинала своими словами), без знания нашей реализации. Строки: CHECK <имя> OK|FAIL.
// Для каждого правила его выключатель -D*_OLD=1 обязан уронить свою проверку (tests/sverka_g09b/g09b.sh). Источник чисел: проверяющий круга 3
// (g09b-menu_проверка.md, «пересчитано по исходнику»); сам оригинал в круге 6 не открыт: нет доступа.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../viewer/menu_rules.h"

static int fails, all;
static void ck(const char *n, int ok, const char *fmt, ...) {
  char d[200]; __builtin_va_list ap; __builtin_va_start(ap, fmt); vsnprintf(d, sizeof d, fmt, ap); __builtin_va_end(ap);
  all++; if (!ok) fails++; printf("CHECK %-28s %s  %s\n", n, ok ? "OK  " : "FAIL", d);
}
static int eq(float a, float b, float e) { return fabsf(a - b) <= e; }
// время, когда кривая перехода длительности d достигает доли p (поиск по секундам с шагом 1 мс)
// эталон: переход камеры оригинала, посчитанный по его же шагу (кубическое движение к цели, доля шага за кадр = (0,98·t/лимит + 0,02)·кадр/16 мс, начальная скорость 0), доля пути на момент t
static float refCam(float t, float limitMs) {
  float p = 1.f, rate = 0.f, T = 0.f, ms = 1000.f / 60.f, prev = 0.f, prevT = 0.f;
  for (int i = 0; i < 90; i++) {
    T += ms; float I = (.98f * T / limitMs + .02f) * ms / 16.f; if (I > 1.f) I = 1.f;
    float a = rate + p * 2.f, b = -p * 3.f - rate * 2.f, pn = a * I * I * I + b * I * I + rate * I + p; rate = a * I * I * 3.f + b * I * 2.f + rate; p = pn;
    if (T / 1000.f >= t) { float f = (t - prevT) / (T / 1000.f - prevT); return prev + (1.f - p - prev) * f; }
    prev = 1.f - p; prevT = T / 1000.f;
  }
  return prev;
}
static float maxDev(float d, float limitMs) { float m = 0.f; for (float t = 0.f; t <= .8f; t += .01f) m = fmaxf(m, fabsf(mrCamU(t, d) - fminf(1.f, refCam(t, limitMs)))); return m; }
static float tAt(float d, float p) { for (float t = 0.f; t < 3.f; t += .001f) if (mrCamU(t, d) >= p) return t; return 9.f; }

int main(void) {
  // M2 Д2 / P13 №1 / P14 Д1: у запросов да/нет курсор на «No» (индекс 1)
  ck("курсор запроса", mrYesNoCursor() == 1, "индекс %d, ждём 1 (No)", mrYesNoCursor());
  // P13 Д5: START на записи и вопросах паузы снимает паузу целиком (1); на конце уровня (только бит «сохранить?», без «выйти?») не действует; на LOAD GAME не действует
  ck("START вопрос выхода", mrStartRes(1, 2) == 1 && mrStartRes(1, 3) == 1, "askQuit 2 -> %d, 3 -> %d, ждём 1", mrStartRes(1, 2), mrStartRes(1, 3));
  ck("START запись из паузы", mrStartRes(1, 0) == 1, "%d, ждём 1", mrStartRes(1, 0));
  ck("START конец уровня", mrStartRes(1, 1) == -1, "%d, ждём -1", mrStartRes(1, 1));
  ck("START список загрузки", mrStartRes(0, 0) == -1, "%d, ждём -1", mrStartRes(0, 0));
  // M2 Д2: выход из паузы идёт через вопросы; вне миссии сначала «сохранить?» (бит 0), потом «выйти?» (бит 1)
  ck("вопросы выхода", mrQuitAsks() == 1 && mrQuitAsk(0) == 2 && mrQuitAsk(1) == 3, "asks %d, миссия %d, свободная езда %d, ждём 1,2,3", mrQuitAsks(), mrQuitAsk(0), mrQuitAsk(1));
  // P14 №6, P16-24: подпись слота; PAL — день/месяц/год(2 цифры), время с секундами; обучение первого уровня — одно слово «(TUT)»
  char b[64];
  mrSlotLabel(b, sizeof b, 3, 4, 5, 11, 2026, 14, 3, 9); ck("подпись обычная", !strcmp(b, "(L3 M4) 05/11/26 14:03:09"), "«%s»", b);
  mrSlotLabel(b, sizeof b, 1, 0, 31, 12, 2005, 0, 59, 59); ck("подпись обучения", !strcmp(b, "(TUT) 31/12/05 00:59:59"), "«%s»", b);
  mrSlotLabel(b, sizeof b, 2, 0, 1, 2, 2020, 1, 2, 3); ck("подпись L2 M0", !strcmp(b, "(L2 M0) 01/02/20 01:02:03"), "«%s»", b);
  // M1 №7, M3 №15: «- DEMO -» первые 0,25 с скрыта, потом 0,25 с видна, период 0,5 с
  ck("мигание DEMO", !mrDemoShown(.10f) && mrDemoShown(.30f) && !mrDemoShown(.60f) && mrDemoShown(.80f), "0,10:%d 0,30:%d 0,60:%d 0,80:%d, ждём 0,1,0,1",
     mrDemoShown(.10f), mrDemoShown(.30f), mrDemoShown(.60f), mrDemoShown(.80f));
  // M3 №10, P17-2: простой титульного 30 с, кнопки счёт не сбрасывают
  ck("простой 30 с", !mrDemoIdleDue(29.9f, MR_DEMO_IDLE_S) && mrDemoIdleDue(30.f, MR_DEMO_IDLE_S), "предел %.1f, 29,9:%d 30:%d", MR_DEMO_IDLE_S, mrDemoIdleDue(29.9f, MR_DEMO_IDLE_S), mrDemoIdleDue(30.f, MR_DEMO_IDLE_S));
  ck("кнопка не сбрасывает простой", !mrIdleReset(1), "зажатая кнопка -> сброс %d, ждём 0", mrIdleReset(1));
  // СВЕРКА_КАМЕРА 3.1/3.2: S-кривая, 50 % за 0,26 с, 90 % за 0,40 с, 95 % за 0,45 с, 99 % за 0,48 с (лимит 7 с); допуск 0,02 с
  float t50 = tAt(CAM_OUT_S, .5f), t90 = tAt(CAM_OUT_S, .9f), t95 = tAt(CAM_OUT_S, .95f), t99 = tAt(CAM_OUT_S, .99f);
  ck("посадка 50/90/95/99", eq(t50, .26f, .02f) && eq(t90, .40f, .02f) && eq(t95, .45f, .02f) && eq(t99, .48f, .02f), "%.3f %.3f %.3f %.3f с, ждём .26 .40 .45 .48", t50, t90, t95, t99);
  ck("посадка = выход", eq(CAM_IN_TAU, CAM_OUT_S, 1e-4f), "вход %.3f выход %.3f", CAM_IN_TAU, CAM_OUT_S);
  ck("старт с нулевой скорости", mrCamU(.02f, CAM_OUT_S) < .01f, "u(0,02 с) = %.4f, ждём < .01 (у экспоненты ~.12)", mrCamU(.02f, CAM_OUT_S));
  // круг 7: форма — по самому переходу оригинала: отклонение от пересчёта ≤ 0,04 (лимит 7 с) и ≤ 0,055 (лимит 3 с)
  { float d7 = maxDev(CAM_OUT_S, 7000.f), d3 = maxDev(CAMIN1041_BLEND, 3000.f); ck("форма по пересчёту", d7 <= .04f && d3 <= .055f, "лимит 7 с: %.3f (≤ .04), лимит 3 с: %.3f (≤ .055)", d7, d3); }
  // 3.4: конец наезда меню — быстрый переход, 95 % за 0,32 с (лимит 3 с)
  float tm = tAt(CAMIN1041_BLEND, .95f); ck("конец наезда 95 %", eq(tm, .32f, .015f), "%.3f с, ждём .32", tm);
  // M2 №22: полосы разговора 0,5 с
  ck("полосы разговора", eq(LB_SLIDE, .5f, 1e-4f), "%.3f с, ждём .5", LB_SLIDE);
  // M3 №1: перелёты OPTIONS: SOUND 660-690/690-720, титры 600-630/630-660, ролики 914-944/944-974; между подэкранами напрямую перелёта нет
  { int a, bb, ok = 1; static const int T[6][4] = {{0,1,660,690},{1,0,690,720},{0,2,600,630},{2,0,630,660},{0,3,914,944},{3,0,944,974}};
    for (int i = 0; i < 6; i++) { a = bb = -1; if (!mrOptFly(T[i][0], T[i][1], &a, &bb) || a != T[i][2] || bb != T[i][3]) ok = 0; }
    ck("перелёты OPTIONS", ok && !mrOptFly(1, 2, &a, &bb), "6 диапазонов; 1->2 без перелёта: %d", !mrOptFly(1, 2, &a, &bb)); }
  // P03-06: угон — дверь открыта, пока машина едет не быстрее 4,5 м/с
  ck("угон по скорости", !mrJackBlocked(4.4f) && mrJackBlocked(4.6f) && mrJackBlocked(-5.f) && !mrJackBlocked(0.f), "4,4:%d 4,6:%d -5:%d 0:%d, ждём 0,1,1,0", mrJackBlocked(4.4f), mrJackBlocked(4.6f), mrJackBlocked(-5.f), mrJackBlocked(0.f));
  // P05 №9 / P15-18 (подсказки обучения): выдавальщик в 5 м
  ck("подсказка от выдавальщика", eq(TUT_GIVER_M, 5.f, 1e-4f), "%.1f м, ждём 5", TUT_GIVER_M);
  // P03 Д7: старт миссии и победа сбрасывают шкалу; провал нет; отмена из паузы не победа; повтор после провала сбрасывает
  ck("миссия: старт", mrHnrMsnEvent(0, 1, 0, 0) == 1, "%d, ждём 1", mrHnrMsnEvent(0, 1, 0, 0));
  ck("миссия: победа", mrHnrMsnEvent(1, 0, 0, 0) == 2, "%d, ждём 2", mrHnrMsnEvent(1, 0, 0, 0));
  ck("миссия: провал не победа", mrHnrMsnEvent(1, 0, 1, 0) == 0, "%d, ждём 0", mrHnrMsnEvent(1, 0, 1, 0));
  ck("миссия: отмена не победа", mrHnrMsnEvent(1, 0, 0, 1) == 0, "%d, ждём 0", mrHnrMsnEvent(1, 0, 0, 1));
  ck("миссия: нет замера", mrHnrMsnEvent(-1, 1, 0, 0) == 0 && mrHnrMsnEvent(1, 1, 0, 0) == 0, "без смены/без замера ничего");
  ck("миссия: повтор сбрасывает", mrHnrRetry(1, 0, 1) && !mrHnrRetry(0, 0, 1) && !mrHnrRetry(1, 0, 0) && mrHnrRetryReset(), "повтор %d, шкала в 0: %d", mrHnrRetry(1, 0, 1), mrHnrRetryReset());
  // P03-08: погоня по одной — проход постановки раз в 0,7 с, снятие дальних раз в 0,5 с
  ck("проходы погони", eq(HNR_PASS_S, .7f, 1e-4f) && eq(HNR_FAR_S, .5f, 1e-4f) && !HNRONE_OLD, "%.2f и %.2f с, ждём .7 и .5", HNR_PASS_S, HNR_FAR_S);
  // P03-10: после ареста шкала 0, индикатор держит 5 % 1,3 с и спадает 0,03/с (до нуля за 1,67 с)
  { float sh = HNR_SHOW_LEVEL, hd = HNR_SHOW_HOLD_S; int n = 0;
    for (; n < 600 && (hd > 0.f || sh > 0.f); n++) { mrHnrShowStep(&sh, &hd, 1.f / 60.f); if (n == 71) break; }   // 1,2 с
    int holdOk = eq(sh, .05f, 1e-4f);
    for (; n < 100; n++) mrHnrShowStep(&sh, &hd, 1.f / 60.f);   // 100 кадров = 1,67 с: держим 1,3 с, спад .37 с
    int midOk = sh < .05f && sh > .035f;
    for (n = 0; n < 600; n++) mrHnrShowStep(&sh, &hd, 1.f / 60.f);
    ck("хвост показа после ареста", holdOk && midOk && sh == 0.f && eq(mrHnrRing(0.f, .05f), .05f, 1e-5f) && mrHnrRing(.5f, .05f) == .5f, "держит %d, спад %d, итог %.3f, кольцо при шкале 0: %.3f", holdOk, midOk, sh, mrHnrRing(0.f, .05f)); }
  // круг 7. P05-9: шар по трём осям, строго внутри (радиус 3: по земле 2,5 м, но на 2 м выше — вне)
  ck("будка: шар", mrSphereIn(2.5f, 0.f, 0.f, 3.f) && !mrSphereIn(2.5f, 2.f, 0.f, 3.f) && !mrSphereIn(3.f, 0.f, 0.f, 3.f), "по земле %d, выше %d, на радиусе %d, ждём 1,0,0", mrSphereIn(2.5f, 0.f, 0.f, 3.f), mrSphereIn(2.5f, 2.f, 0.f, 3.f), mrSphereIn(3.f, 0.f, 0.f, 3.f));
  // P02-43: тень Гомера: масштаб 1 − 0,5·h/1,9: на земле 1, на вершине прыжка 1,9 м — 0,5, на 3,8 м — 0
  ck("тень от высоты", eq(mrShadowScale(0.f), 1.f, 1e-4f) && eq(mrShadowScale(1.9f), .5f, 1e-4f) && eq(mrShadowScale(3.8f), 0.f, 1e-4f) && eq(mrShadowScale(.95f), .75f, 1e-4f), "0:%.3f 1,9:%.3f 3,8:%.3f 0,95:%.3f, ждём 1 .5 0 .75", mrShadowScale(0.f), mrShadowScale(1.9f), mrShadowScale(3.8f), mrShadowScale(.95f));
  // P06-18: на старте миссии чинится только разбитая машина трафика; помятая и своя остаются
  ck("ремонт на старте", mrRepairOnStart(1, 1.f, 1) && !mrRepairOnStart(0, .3f, 1) && !mrRepairOnStart(1, 1.f, 0), "разбитая чужая %d, помятая чужая %d, разбитая своя %d, ждём 1,0,0", mrRepairOnStart(1, 1.f, 1), mrRepairOnStart(0, .3f, 1), mrRepairOnStart(1, 1.f, 0));
  // M3 №14: глушить всё, кроме музыки, при любом старте игры из меню
  ck("глушение при старте", mrDuckOnStart(0) && mrDuckOnStart(1), "NEW GAME %d, RESUME %d, ждём 1,1", mrDuckOnStart(0), mrDuckOnStart(1));
  // P03 Д2: помеченная машина погони — снимается, когда дольше 5 с подряд вне кадра (в кадре — счёт с нуля)
  { float t = 0.f; int r1 = 0; for (int i = 0; i < 299; i++) r1 |= mrHnrOutStep(&t, 0, 1.f / 60.f);   // 4,98 с: ещё нет
    int r2 = mrHnrOutStep(&t, 0, 1.f / 60.f); r2 |= mrHnrOutStep(&t, 0, 1.f / 60.f); /* 5,02 с: пора */ mrHnrOutStep(&t, 1, 1.f / 60.f); int r3 = t == 0.f;
    ck("5 с вне кадра", !r1 && r2 && r3, "на 4,98 с %d, на 5,02 с %d, в кадре сброс %d, ждём 0,1,1", r1, r2, r3); }
  // P03-07: на время разговора машины погони в запас с позой, не убирать совсем
  ck("погоня в запас", mrHnrSuspKeep(), "%d, ждём 1", mrHnrSuspKeep());
  // P14 Д2: NEW GAME «Yes» — радар On, подсказки и ролики заново, Jump Cameras On; прочее (громкости) функция не получает вовсе
  { int rd = 0, jc = 0; unsigned ts = 5, mv = 7; mrNewGameDefaults(&rd, &ts, &mv, &jc); ck("NEW GAME: умолчания", rd == 1 && ts == 0 && mv == 0 && jc == 1, "радар %d, подсказки %u, ролики %u, камеры %d, ждём 1,0,0,1", rd, ts, mv, jc); }
  // №1111: язык игры. Язык системы PSP -> 0 English 1 French 2 German 3 Spanish; итальянский (оригинал его выключил) и португальский (на диске нет) — вопрос «Select Language»
  ck("язык системы", mrLangFromPsp(1) == 0 && mrLangFromPsp(2) == 1 && mrLangFromPsp(4) == 2 && mrLangFromPsp(3) == 3 && mrLangFromPsp(5) == -1 && mrLangFromPsp(7) == -1 && mrLangFromPsp(0) == -1,
     "EN %d FR %d DE %d ES %d IT %d PT %d JA %d, ждём 0 1 2 3 -1 -1 -1", mrLangFromPsp(1), mrLangFromPsp(2), mrLangFromPsp(4), mrLangFromPsp(3), mrLangFromPsp(5), mrLangFromPsp(7), mrLangFromPsp(0));
  { char f[128]; mrLangFile(f, sizeof f, "ms0:/G/", 2, "l3/ui.shr");
    ck("файл на языке", mrLangDepends(1, "ui.shr") && mrLangDepends(3, "l2/level02.msn") && mrLangDepends(2, "cardqr-l1.bin") && !mrLangDepends(1, "level.shr") && !mrLangDepends(0, "ui.shr") && !strcmp(f, "ms0:/G/lang/G/l3/ui.shr"),
       "ui F %d, msn S %d, карточки G %d, level.shr %d, английский %d, путь «%s»", mrLangDepends(1, "ui.shr"), mrLangDepends(3, "l2/level02.msn"), mrLangDepends(2, "cardqr-l1.bin"), mrLangDepends(1, "level.shr"), mrLangDepends(0, "ui.shr"), f); }
  ck("выбор языка стрелками", mrLangMove(0, 1) == 1 && mrLangMove(3, 1) == 0 && mrLangMove(0, -1) == 3, "0+1=%d 3+1=%d 0-1=%d, ждём 1 0 3", mrLangMove(0, 1), mrLangMove(3, 1), mrLangMove(0, -1));
  // №1143: при MINI GAME наезд камеры на трек один (в таблице данных есть пункт MINI, плюс диапазон оригинала 880–913): табличный пропускается
  ck("MINI GAME: один наезд", mrMiniFlyTable(1) + mrMiniFlyRange() == 1 && mrMiniFlyTable(0) + mrMiniFlyRange() == 1, "таблица есть: %d, нет: %d, ждём 1 и 1", mrMiniFlyTable(1) + mrMiniFlyRange(), mrMiniFlyTable(0) + mrMiniFlyRange());
  // №1151: раздел CAMERA паузы — ступени (середина 2 = как было), любой неверный шаг — середина; у старого settings.bin пешком берёт прежние Height / Distance
  ck("камера: ступени", mrCamH(0) == .5f && mrCamH(4) == 1.5f && mrCamD(0) == .7f && mrCamD(4) == 1.3f && mrCamTilt(0) == -10.f && mrCamTilt(4) == 10.f && mrCamFov(0) == .85f && mrCamFov(4) == 1.15f
     && mrCamH(2) == 1.f && mrCamD(2) == 1.f && mrCamTilt(2) == 0.f && mrCamFov(2) == 1.f && mrCamH(9) == 1.f, "H %.2f..%.2f D %.2f..%.2f tilt %.0f..%.0f fov %.2f..%.2f", mrCamH(0), mrCamH(4), mrCamD(0), mrCamD(4), mrCamTilt(0), mrCamTilt(4), mrCamFov(0), mrCamFov(4));
  ck("камера: старый файл", mrCamFootMigrate(100, 200, 2, 4) == 4 && mrCamFootMigrate(300, 200, 1, 4) == 1, "короткий файл %d (ждём 4), полный %d (ждём 1)", mrCamFootMigrate(100, 200, 2, 4), mrCamFootMigrate(300, 200, 1, 4));
  printf("RESULT %d/%d\n", all - fails, all);
  return fails ? 1 : 0;
}
