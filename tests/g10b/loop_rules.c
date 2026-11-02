// Круг 6 g10b-loop: правила главного цикла (viewer/g10b_calc.h) числами. Каждая проверка помечена своим выключателем:
// с -D<ИМЯ>_OLD=1 должны упасть проверки этого имени и только они (loop_rules.sh).
#include <stdio.h>
#include <math.h>
#include "g10b_calc.h"
static int bad;
#define CK(tag, name, cond) do { int ok_ = (cond); printf("CHECK %s %s %s\n", tag, name, ok_ ? "PASS" : "FAIL"); if (!ok_) bad = 1; } while (0)
static int near_(float a, float b, float e) { return fabsf(a - b) <= e; }
int main(void) {
  // шкала: за 1 секунду на 60 кадрах теряет столько единиц, сколько спад в секунду (а не за кадр)
  { float lv[8]; for (int L = 1; L <= 7; L++) { float g = 100.f; for (int f = 0; f < 60; f++) g -= g10bHnrRate(L, 0, 0) / 60.f; lv[L] = 100.f - g; }
    CK("HNR_LEVEL_DECAY_OLD", "L1 3.0/с", near_(lv[1], 3.f, .01f));
    CK("HNR_LEVEL_DECAY_OLD", "L2 1.5/с", near_(lv[2], 1.5f, .01f));
    for (int L = 3; L <= 7; L++) CK("HNR_LEVEL_DECAY_OLD", "L3..L7 1.0/с", near_(lv[L], 1.f, .01f));
    for (int L = 1; L <= 7; L++) { CK("HNR_LEVEL_DECAY_OLD", "погоня 6/с на всех уровнях", g10bHnrRate(L, 0, 1) == 6.f);
                                    CK("HNR_LEVEL_DECAY_OLD", "помещение 10/с на всех уровнях", g10bHnrRate(L, 1, 1) == 10.f && g10bHnrRate(L, 1, 0) == 10.f); } }
  // пауза: запрещена на любом из пяти признаков, разрешена без них (граница затемнения: ровно 0 — можно)
  CK("PAUSE_BAN_OLD", "затемнение HUD > 0", g10bPauseBanned(.01f, 0, 0, 0, -1));
  CK("PAUSE_BAN_OLD", "ждёт уровень", g10bPauseBanned(0.f, 1, 0, 0, -1));
  CK("PAUSE_BAN_OLD", "ждёт меню", g10bPauseBanned(0.f, 0, 1, 0, -1));
  CK("PAUSE_BAN_OLD", "ждёт новость", g10bPauseBanned(0.f, 0, 0, 1, -1));
  CK("PAUSE_BAN_OLD", "ждёт выбор миссии (строка 0)", g10bPauseBanned(0.f, 0, 0, 0, 0));
  CK("PAUSE_BAN_OLD", "ничего не ждёт — пауза можно", !g10bPauseBanned(0.f, 0, 0, 0, -1));
#if LOOKBACK1130_OLD
  // вид назад: штанга отражается через машину (высота та же), взгляд остаётся; не зажато — ничего. Бампер: внутри кузова на (задняя ось − 1 м), высота переднего глаза − 0,3 м, взгляд на 2 м назад
  { float ccp[3] = {10.f, 3.f, 20.f}, clk[3] = {1.f, 2.f, 3.f}, cp[3] = {0, 0, 0}, lk[3] = {0, 0, 0};
    int r = g10bRearCam(1, 0, 4.f, 8.f, 0.f, 1.5f, ccp, clk, cp, lk);
    CK("REARVIEW_OLD", "зажато: срез включён", r == 1);
    CK("REARVIEW_OLD", "отражение по горизонтали", near_(cp[0], -2.f, 1e-4f) && near_(cp[2], -4.f, 1e-4f));
    CK("REARVIEW_OLD", "взгляд остаётся", lk[0] == 1.f && lk[1] == 2.f && lk[2] == 3.f);
    r = g10bRearCam(0, 0, 4.f, 8.f, 0.f, 1.5f, ccp, clk, cp, lk); CK("REARVIEW_OLD", "отпущено: среза нет", r == 0);
    r = g10bRearCam(1, 1, 4.f, 8.f, 0.f, 1.5f, ccp, clk, cp, lk);   // курс 0: нос в +Z, зад в −Z
    CK("REARVIEW_OLD", "бампер: глаз на 0,5 м позади центра (ось 1,5 − 1,0)", near_(cp[0], 4.f, 1e-4f) && near_(cp[2], 7.5f, 1e-4f));
    CK("REARVIEW_OLD", "бампер: высота переднего глаза − 0,3 м", near_(cp[1], 2.7f, 1e-4f));
    CK("REARVIEW_OLD", "бампер: взгляд на 2 м назад", near_(lk[2], cp[2] - 2.f, 1e-4f) && lk[1] == cp[1]); }
  // вид назад переключается только пока устойчиво: в воздухе / на боку / 0,2 с после — состояние держится
  { int st = 0; float dly = 0; float q0[4] = {1, 0, 0, 0}, qs[4] = {.7071f, .7071f, 0, 0}; float land[4] = {.1f, .1f, .1f, .1f}, air[4] = {0, 0, 0, 0};
    int un = g10bCamUnstable(air, q0, &dly, .03f);
    CK("REARVIEW_OLD", "в воздухе — неустойчиво", un == 1);
    CK("REARVIEW_OLD", "в воздухе кнопка не включает вид", g10bRearLook(&st, 1, 0, un) == 0);
    un = g10bCamUnstable(land, q0, &dly, .1f); CK("REARVIEW_OLD", "через 0,1 с после посадки ещё неустойчиво", un == 1);
    un = g10bCamUnstable(land, q0, &dly, .15f); CK("REARVIEW_OLD", "через 0,25 с — устойчиво", un == 0);
    CK("REARVIEW_OLD", "устойчиво: кнопка включает", g10bRearLook(&st, 1, 0, un) == 1);
    un = g10bCamUnstable(air, q0, &dly, .03f); CK("REARVIEW_OLD", "в полёте отпускание не выключает вид", g10bRearLook(&st, 0, 0, un) == 1);
    st = 0; dly = 0; un = g10bCamUnstable(land, qs, &dly, .03f); CK("REARVIEW_OLD", "на боку (90°) — неустойчиво", un == 1);
    CK("REARVIEW_OLD", "бампер: правило устойчивости не действует", g10bRearLook(&st, 1, 1, 1) == 1); }
#endif
  // №1130: вида назад нет — стрелка вниз (зажата, машина устойчива) камеру не меняет ни у дальнего вида, ни у бампера
  { float ccp[3] = {10.f, 3.f, 20.f}, clk[3] = {1.f, 2.f, 3.f}, cp[3] = {10.f, 3.f, 20.f}, lk[3] = {1.f, 2.f, 3.f}; int st = 1;
    CK("LOOKBACK1130_OLD", "дальний вид: зажатая стрелка вниз не включает вид назад", g10bRearCam(1, 0, 4.f, 8.f, 0.f, 1.5f, ccp, clk, cp, lk) == 0 && cp[0] == 10.f && cp[2] == 20.f && lk[2] == 3.f);
    CK("LOOKBACK1130_OLD", "бампер: то же", g10bRearCam(1, 1, 4.f, 8.f, 0.f, 1.5f, ccp, clk, cp, lk) == 0 && cp[1] == 3.f);
    CK("LOOKBACK1130_OLD", "устойчивая машина, кнопка зажата: состояние вида 0", g10bRearLook(&st, 1, 0, 0) == 0 && st == 0);
    CK("LOOKBACK1130_OLD", "бампер, кнопка зажата: 0", g10bRearLook(&st, 1, 1, 0) == 0); }
  // угол бампера: 90° по горизонтали → по вертикали 58,7° на 16:9, 73,7° на 4:3 (прежнее: тот же, что за машиной)
  CK("BUMPER_FOV_OLD", "16:9 = 58.7°", near_(g10bBumperFovY(45.f, 16.f / 9.f), 58.715f, .02f));
  CK("BUMPER_FOV_OLD", "4:3 = 73.7°", near_(g10bBumperFovY(45.f, 4.f / 3.f), 73.740f, .02f));
  CK("BUMPER_FOV_OLD", "не равен углу за машиной", fabsf(g10bBumperFovY(45.f, 16.f / 9.f) - 45.f) > 1.f);
  // дым: границы 59/60/97/98 %
  CK("SMOKE_D_OLD", "59% дыма нет", g10bSmokeStage(.59f, 0, 0, 0) == 0);
  CK("SMOKE_D_OLD", "61% лёгкий дым", g10bSmokeStage(.61f, 0, 0, 0) == 1);
  CK("SMOKE_D_OLD", "97% ещё лёгкий", g10bSmokeStage(.97f, 0, 0, 0) == 1);
  CK("SMOKE_D_OLD", "98% полный средний", g10bSmokeStage(.98f, 0, 0, 0) == 2);
  CK("SMOKE_D_OLD", "интенсивность 70% = 0.25", near_(g10bSmokeRate(.7f, 1), .25f, 1e-4f));
  CK("SMOKE_D_OLD", "интенсивность 60% = 0", near_(g10bSmokeRate(.6f, 1), 0.f, 1e-4f));
  CK("SMOKE_D_OLD", "интенсивность 100% = 1", near_(g10bSmokeRate(1.f, 1), 1.f, 1e-4f));
  CK("SMOKE_D_OLD", "на 98 % (средний дым) тоже по формуле: 0.95", near_(g10bSmokeRate(.98f, 2), .95f, 1e-4f));
  CK("SMOKE_D_OLD", "огонь/разбита не меняются", g10bSmokeStage(.5f, 0, 0, 1) == 3 && g10bSmokeStage(.5f, 0, 1, 1) == 4 && g10bSmokeStage(.9f, 1, 0, 0) == 0);
  // Turbo: ровно 5 с (в 60 кадрах/с — кадр 300), выход раньше — с нуля
  { float t = 0; int said = 0, at = -1; for (int f = 1; f <= 600; f++) if (g10bTurboStep(&t, &said, 1, 1.f / 60.f)) { at = f; break; }
    CK("TURBO_TIRED_OLD", "реплика на 5 с (кадр 300)", at >= 299 && at <= 301);
    t = 0; said = 0; int early = 0; for (int f = 0; f < 240; f++) early |= g10bTurboStep(&t, &said, 1, 1.f / 60.f);
    CK("TURBO_TIRED_OLD", "на 4 с реплики нет", !early);
    g10bTurboStep(&t, &said, 0, 1.f / 60.f); CK("TURBO_TIRED_OLD", "выход обнуляет", t == 0.f);
    int again = 0; for (int f = 0; f < 240; f++) again |= g10bTurboStep(&t, &said, 1, 1.f / 60.f); CK("TURBO_TIRED_OLD", "после выхода снова считает с нуля", !again);
    t = 0; said = 0; int once = 0; for (int f = 0; f < 1200; f++) once += g10bTurboStep(&t, &said, 1, 1.f / 60.f); CK("TURBO_TIRED_OLD", "реплика одна за забег", once == 1); }
  // искры: упор подвески после полёта (≤ 1 с), один раз за полёт; без полёта и на траве — нет
  { float since = 9.f; int n = 0;
    n += g10bBottomSparkStep(&since, .03f, 1, 0, 1);            // кадр полёта
    n += g10bBottomSparkStep(&since, .03f, 0, 1, 1);            // упор через кадр — искры
    CK("LAND_SPARK_OLD", "упор сразу после полёта — искры", n == 1);
    CK("LAND_SPARK_OLD", "второй упор того же полёта — нет", g10bBottomSparkStep(&since, .03f, 0, 1, 1) == 0);
    since = 9.f; g10bBottomSparkStep(&since, .03f, 1, 0, 1); int late = 0; for (int f = 0; f < 40; f++) late += g10bBottomSparkStep(&since, .03f, 0, f == 39, 1);
    CK("LAND_SPARK_OLD", "упор через 1,2 с после полёта — нет", late == 0);
    since = 9.f; CK("LAND_SPARK_OLD", "упор без полёта — нет", g10bBottomSparkStep(&since, .03f, 0, 1, 1) == 0);
    since = 9.f; g10bBottomSparkStep(&since, .03f, 1, 0, 1); CK("LAND_SPARK_OLD", "упор на траве — нет", g10bBottomSparkStep(&since, .03f, 0, 1, 0) == 0);
    float comp[4] = {.1f, .2f, .3f, .4f}; CK("LAND_SPARK_OLD", "упор = любое колесо на пределе хода", !g10bBottomOut(comp, .5f) && (comp[2] = .5f, g10bBottomOut(comp, .5f)));
    CK("LAND_SPARK_OLD", "число искр 30", G10B_SPARK_N == 30); }
  // локатор: из восьми углов остаются четыре (низ-сзади ×2, верх-спереди ×2)
  { int n = 0, lowRear = 0, hiFront = 0, hiRear = 0, lowFront = 0; for (int a = -1; a <= 1; a += 2) for (int u = 0; u < 2; u++) if (g10bCarZoneCorner(a, u)) { n++; if (a < 0 && !u) lowRear++; if (a > 0 && u) hiFront++; if (a < 0 && u) hiRear++; if (a > 0 && !u) lowFront++; }
    CK("CARZONE_PTS_OLD", "две пары (сзади-низ, спереди-верх) × лево-право = 4 угла", n == 2);
    CK("CARZONE_PTS_OLD", "низ-сзади и верх-спереди есть", lowRear == 1 && hiFront == 1);
    CK("CARZONE_PTS_OLD", "верх-сзади и низ-спереди нет", hiRear == 0 && lowFront == 0); }
  // SELECT: ближний → бампер → дальний → ближний; багги без бампера; под камерой зоны выбор игнорируется
  CK("SELECT_VIEW_OLD", "ближний → бампер", g10bViewNext(0, 0, 0) == 2);
  CK("SELECT_VIEW_OLD", "бампер → дальний", g10bViewNext(2, 0, 0) == 1);
  CK("SELECT_VIEW_OLD", "дальний → ближний", g10bViewNext(1, 0, 0) == 0);
  CK("SELECT_VIEW_OLD", "багги: ближний → дальний", g10bViewNext(0, 1, 0) == 1);
  CK("SELECT_VIEW_OLD", "камера зоны: вид не меняется", g10bViewNext(0, 0, 1) == 0);
  // вопрос «сохранить?»: в обычной игре есть, в туре и служебном запуске нет
  CK("LV_SAVEASK_OLD", "обычная игра — спрашивает", g10bLevelAsk(0, 0, 0, 1, 1, 0));
  CK("LV_SAVEASK_OLD", "тур — не спрашивает", !g10bLevelAsk(0, 1, 0, 1, 1, 0));
  CK("LV_SAVEASK_OLD", "без записи (svm выключен) — не спрашивает", !g10bLevelAsk(0, 0, 0, 0, 1, 0));
  // △: ранги и пауза в 3 кадра (между срабатываниями ≥ 4 кадров)
  CK("ACTION_RANK_OLD", "своя машина важнее будки", g10bActionPick(1u << G10B_ACT_OWNCAR | 1u << G10B_ACT_BOOTH) == G10B_ACT_OWNCAR);
  CK("ACTION_RANK_OLD", "будка важнее двери", g10bActionPick(1u << G10B_ACT_BOOTH | 1u << G10B_ACT_DOOR) == G10B_ACT_BOOTH);
  CK("ACTION_RANK_OLD", "дверь важнее выключателя", g10bActionPick(1u << G10B_ACT_DOOR | 1u << G10B_ACT_SWITCH) == G10B_ACT_DOOR);
  CK("ACTION_RANK_OLD", "выключатель важнее чужой машины", g10bActionPick(1u << G10B_ACT_SWITCH | 1u << G10B_ACT_FOREIGN) == G10B_ACT_SWITCH);
  CK("ACTION_RANK_OLD", "миссия важнее всего", g10bActionPick(0xffu) == G10B_ACT_MISSION);
  { float cool = 0.f; int nf = 0, frames[16];   // 30 кадр/с: зажатая △ срабатывает каждый 4-й кадр
    for (int f = 0; f < 14; f++) { if (g10bTriStep(&cool, 1, 1.f / 30.f)) { frames[nf++] = f; g10bTriFired(&cool); } }
    CK("ACTION_RANK_OLD", "30 кадр/с: срабатывает каждые 4 кадра", nf >= 3 && frames[1] - frames[0] == 4 && frames[2] - frames[1] == 4);
    cool = 0.f; nf = 0; for (int f = 0; f < 30; f++) { if (g10bTriStep(&cool, 1, 1.f / 60.f)) { frames[nf++] = f; g10bTriFired(&cool); } }
    CK("ACTION_RANK_OLD", "60 кадр/с: тот же промежуток 0,13 с (8 кадров)", nf >= 3 && frames[1] - frames[0] == 8); }
  // меню и загрузка
  CK("MENU_SNAP_OLD", "игра в памяти, записи на карте нет — RESUME виден", g10bMenuResumeShown(0, 1));
  CK("MENU_SNAP_OLD", "игры нет совсем — RESUME нет", !g10bMenuResumeShown(0, 0));
  CK("MENU_SNAP_OLD", "запись на карте — RESUME виден", g10bMenuResumeShown(1, 0));
  CK("MENU_SNAP_OLD", "курсор на RESUME при втором входе, если игра в памяти", g10bMenuCursorResume(0, 0, 1));
  CK("MENU_SNAP_OLD", "курсор на NEW GAME, если игры нет", !g10bMenuCursorResume(0, 0, 0) && !g10bMenuCursorResume(1, 0, 0));
  CK("MENU_SNAP_OLD", "SCRAP BOOK доступен без записи и без игры", g10bMenuScrapEnabled(0, 0));
  CK("LICENSE5S_OLD", "лицензия 5000 мс", g10bLicenseMs() == 5000);
  CK("LOAD_NOMIN_OLD", "минимум = влёт 1.8 + 0.2 = 2.0 с", near_(g10bLoadMinS(1.8f, 0), 2.0f, 1e-5f));
  CK("LOAD_NOMIN_OLD", "минимум следует за влётом (0.9 → 1.1 с)", near_(g10bLoadMinS(.9f, 0), 1.1f, 1e-5f));
  CK("TUT_PAUSE_OLD", "реплика звучит: START открывает паузу", g10bTutPauseOpens(1, 1, 1, 0, 1));
  CK("TUT_PAUSE_OLD", "реплика кончилась: START ничего", !g10bTutPauseOpens(1, 1, 1, 0, 0));
  CK("TUT_PAUSE_OLD", "без подсказки не срабатывает", !g10bTutPauseOpens(0, 1, 1, 0, 1));
  CK("NEWGAME_FMV_NOSKIP_OLD", "обычная игра: ролик не пропустить", g10bFmvNoSkip(0));
  CK("NEWGAME_FMV_NOSKIP_OLD", "служебный режим: можно", !g10bFmvNoSkip(1));
  // полосы разговора: выход за 0,5 с (не 0,25), схождение 0,5 с + пауза 0,5 с
  { float o = 1.f; for (int f = 0; f < 15; f++) o = fmaxf(0.f, o - (1.f / 30.f) / g10bLbExitSecs(.5f));   // 0,5 с на 30 кадрах/с
    CK("LB_EXIT_OLD", "через 0,5 с полосы ушли, а через 0,25 с — ещё нет", o < .02f);
    float o2 = 1.f; for (int f = 0; f < 7; f++) o2 = fmaxf(0.f, o2 - (1.f / 30.f) / g10bLbExitSecs(.5f)); CK("LB_EXIT_OLD", "на 0,23 с полосы ещё держат экран", o2 > .4f); }
  CK("LB_CONV_OLD", "схождение начинается при уходе разговора с защёлкой", g10bLbConvStart(1, 0, 1, -1.f, 0));
  CK("LB_CONV_OLD", "без защёлки (обычный разговор) — не начинается", !g10bLbConvStart(1, 0, 0, -1.f, 0));
  CK("LB_CONV_OLD", "в облёте — не начинается", !g10bLbConvStart(1, 0, 1, -1.f, 1));
  CK("LB_CONV_OLD", "половина пути: закрыто на 50 %", near_(g10bLbCover(.25f), .5f, 1e-5f));
  CK("LB_CONV_OLD", "0,5 с: закрыто полностью, 0,75 с: держится", g10bLbCover(.5f) == 1.f && g10bLbCover(.75f) == 1.f);
  CK("LB_CONV_OLD", "ирис не раньше 1,0 с", !g10bLbConvDone(.99f) && g10bLbConvDone(1.f));
  // камера в разговор: кривая u²(3−2u) за T даёт моменты оригинала (доля пути 50/90/95/99 % за 0,27/0,42/0,45/0,50 с, допуск 0,03 с)
  { float T = g10bTalkBlendSecs(), t50 = -1, t90 = -1, t95 = -1, t99 = -1; for (int i = 0; i <= 1000; i++) { float t = i * .001f, u = g10bSmooth(t, T);
      if (t50 < 0 && u >= .5f) t50 = t; if (t90 < 0 && u >= .9f) t90 = t; if (t95 < 0 && u >= .95f) t95 = t; if (t99 < 0 && u >= .99f) t99 = t; }
    CK("TALKCAM_DUR_OLD", "50 % за 0,27 с", near_(t50, .27f, .03f)); CK("TALKCAM_DUR_OLD", "90 % за 0,42 с", near_(t90, .42f, .03f));
    CK("TALKCAM_DUR_OLD", "95 % за 0,45 с", near_(t95, .45f, .03f)); CK("TALKCAM_DUR_OLD", "99 % за 0,50 с", near_(t99, .50f, .03f)); }
  printf("RESULT %s\n", bad ? "FAIL" : "PASS");
  return bad;
}
