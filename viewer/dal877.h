// №877 (Роб 23.10 утро: «пропекай на все уровни, получай максимальный прок от ночных тестов»): таблица дали от бота-репортёра.
// За 115 м PVS нет — отсечение берёт всё в пирамиде до drawDist (250 м), даже закрытое домами. Бот (tools/scout/scout.c) снял каждую точку земли
// через 10,5 м в 8 сторон на двух высотах глаза; tools/scout/dal877.py записал по точке и стороне, видна ли даль за 140/200 м (> 1 % экрана).
// Файл dal877.bin в папке уровня (подпись таблицы кусков как у PVS2). Отсечение города (cullCity) берёт min(drawDist, нужная даль): худшее из 4 точек
// вокруг глаза и из ближайшей стороны с двумя соседними (кадр шире стороны бота). Туман, лента горизонта, ориентиры, подгрузка клеток — от прежней
// drawDist (картинка дальше не меняется, рисуется только видимое). Меньше двух точек бота рядом, глаз выше самой высокой точки бота на 4 м, полёт, комната — без ограничения.
// Ограничение поднимается сразу, опускается ступенькой через DAL_HOLD с. DAL877_OLD=1 / «flag dal 0» — как было; строка DAL877 в смоуке раз в 2 с
#ifndef DAL877_OLD
#define DAL877_OLD 0
#endif
#define DAL_HOLD 1.f   // с
// Рампа границы дали (TEST90, слово Роба: «убрать дыхание пояса 150–250 м»): раньше цель таблицы (150 / 200 / полная) переключалась ступенькой и пояс 50–100 м домов
// появлялся или пропадал целиком за один кадр (в езде L5 в 26 % окон по 2 с средняя дальность прыгала на ≥ 25 м). Теперь дальность идёт к цели с ограниченной скоростью
// (растёт DAL_UP м/с, убирается DAL_DN м/с) шагами по DAL_Q м (шаг реже кадра: отсечение города переиспользуется между шагами, цена ≈ 0). Гистерезис прежний (вниз только
// после DAL_HOLD с). В покое дальность доходит ровно до цели таблицы: картинка та же, что без рампы. DALRAMP_OLD=1 / файл `dalrampold` / «flag dalramp 0» — прежняя ступенька.
#ifndef DALRAMP_OLD
#define DALRAMP_OLD 0
#endif
#define DAL_UP 40.f
#define DAL_DN 25.f
#define DAL_Q 10.f
static int dalRampOn = !DALRAMP_OLD; static float dalOut = 1e9f, dalAcc, dalPrev = -1.f; static unsigned dalJump, dalChg, dalJumpAll, dalChgAll;   // dalOut — дальность после рампы
static int dalOn = !DAL877_OLD;
static struct { float x0, z0, st; unsigned w, h; unsigned short *pk; short *yt; } dal;
static float dalCur = 1e9f; static unsigned dalN, dalCapN, dalSum, dalWhy[5];   // dalWhy: вне сетки, глаз выше, нет точки, 250 м, пропуск кадра   // dalCur — текущее ограничение, м; счётчики для строки DAL877
static void dalFree(void) { free(dal.pk); memset(&dal, 0, sizeof dal); dalCur = 1e9f; }
static void dalLoad(const unsigned char *tab, unsigned npcN) {
  dalFree(); if (DAL877_OLD || !npcN) return;
  { char fo_[256]; sibling(fo_, "dalrampold"); SceUID o_ = sceIoOpen(fo_, PSP_O_RDONLY, 0); if (o_ >= 0) { sceIoClose(o_); dalRampOn = 0; } }   // рампа границы дали: файл dalrampold — прежняя ступенька
  char fn[256]; levelFile(fn, "dal877.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned h[7]; int ok = 0;
  if (sceIoRead(f, h, 28) == 28 && !memcmp(h, "DAL1", 4) && h[1] == pvs2Sig(tab, npcN) && h[5] > 0 && h[6] > 0 && h[5] * h[6] <= 65536u) {
    unsigned n = h[5] * h[6];
    if ((dal.pk = (unsigned short *)malloc(n * 4u))) {
      if (sceIoRead(f, dal.pk, (int)(n * 4u)) == (int)(n * 4u)) {
        memcpy(&dal.x0, &h[2], 4); memcpy(&dal.z0, &h[3], 4); memcpy(&dal.st, &h[4], 4); dal.w = h[5]; dal.h = h[6]; dal.yt = (short *)(dal.pk + n);
        ok = dal.st > 1.f && dal.st < 100.f; }
      if (!ok) dalFree(); } }
  sceIoClose(f);
  if (smkOn) { char s_[120]; snprintf(s_, sizeof s_, "DAL877 load level=%d ok=%d w=%u h=%u step=%.1f\n", gameLevel, ok, dal.w, dal.h, dal.st); smkWrite(s_); }
}
static float dalNeed(float ex, float ey, float ez, float fx, float fz) {   // нужная даль, м (1e9 — без ограничения)
  if (!dal.pk) return 1e9f;
  float gx = (ex - dal.x0) / dal.st, gz = (ez - dal.z0) / dal.st; int i0 = (int)floorf(gx), j0 = (int)floorf(gz);
  if (i0 < 0 || j0 < 0 || i0 + 1 >= (int)dal.w || j0 + 1 >= (int)dal.h) { dalWhy[0]++; return 1e9f; }
  float yaw = atan2f(fx, fz); int d = (int)floorf(yaw * (4.f / 3.14159265f) + .5f); d = ((d % 8) + 8) % 8;
  int need = 0, have = 0; float yTop = -1e9f;
  for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) { unsigned k = (unsigned)(j0 + a) * dal.w + (unsigned)(i0 + b); unsigned v = dal.pk[k];
    if (v == 0xFFFFu) continue;   // точки нет (дом, вода, край карты) — не в счёт
    have++; if (dal.yt[k] * .1f > yTop) yTop = dal.yt[k] * .1f;
    for (int e = -1; e <= 1; e++) { int c = (int)(v >> (2 * ((d + e + 8) % 8)) & 3u); if (c == 3) c = 2; if (c > need) need = c; } }
  if (have < 2) { dalWhy[2]++; return 1e9f; }   // меньше двух точек бота вокруг глаза — без ограничения
  if (ey > yTop + 4.f) { dalWhy[1]++; return 1e9f; }   // глаз выше самого высокого глаза бота в 4 точках на 4 м (крыша, прыжок, камера сверху); на склоне — по верхней точке
  if (need == 2) dalWhy[3]++;
  return need == 0 ? 150.f : need == 1 ? 200.f : 1e9f;
}
// кадр: ограничение отсечения города; возвращает прежнюю drawDist (вернуть после cullCity)
static float dalApply(float dt, int skip) {
  float keep = drawDist; dimArm = 0;
  if (!dalOn || skip || !dal.pk) { dalCur = 1e9f; dalOut = 1e9f; dalAcc = 0.f; dalPrev = -1.f; if (dal.pk) dalWhy[4]++; return keep; }
  float nd = dalNeed(eyeX, eyeY, eyeZ, cfX, cfZ);
  // ступенькой (150 / 200 / полная): вверх сразу, вниз — когда меньшая нужда держится DAL_HOLD с; ровная дальность — отсечение города переиспользуется
  // (cullCity crv: плавный спуск по DAL_DOWN м/с менял дальность каждый кадр и сбрасывал переиспользование)
  static float lowT; if (nd >= dalCur || nd >= keep) { dalCur = nd; lowT = 0.f; } else if ((lowT += dt) >= DAL_HOLD) { dalCur = nd; lowT = 0.f; }
  float cap = dalCur < keep ? dalCur : keep;
  if (dalRampOn) {   // рампа: к цели с ограниченной скоростью, шагами по DAL_Q (вверх DAL_UP м/с, вниз DAL_DN м/с)
    static int dir_; if (dalOut > keep) dalOut = keep;
    if (cap > dalOut) { if (dir_ != 1) { dir_ = 1; dalAcc = 0.f; } dalAcc += DAL_UP * dt; while (dalAcc >= DAL_Q && dalOut < cap) { dalOut += DAL_Q; dalAcc -= DAL_Q; } if (dalOut > cap) dalOut = cap; }
    else if (cap < dalOut) { if (dir_ != -1) { dir_ = -1; dalAcc = 0.f; } dalAcc += DAL_DN * dt; while (dalAcc >= DAL_Q && dalOut > cap) { dalOut -= DAL_Q; dalAcc -= DAL_Q; } if (dalOut < cap) dalOut = cap; }
    else { dalAcc = 0.f; dir_ = 0; }
    cap = dalOut; }
  { float now_ = cap < keep ? cap : keep; if (dalPrev >= 0.f) { float dd_ = now_ - dalPrev; if (dd_ < 0.f) dd_ = -dd_; if (dd_ > .5f) dalChg++; if (dd_ >= 25.f) dalJump++; } dalPrev = now_; }   // счётчики: кадров со сменой дальности, со сменой на ≥ 25 м
  dalN++; if (cap < drawDist) { drawDist = cap; dalCapN++; dimCapV = drawDist; dimKeepV = keep; dimArm = dimOn && dimN; } dalSum += (unsigned)drawDist;   // dimArm: ориентиры дали (dalimp.h)
  if (smkOn) { static unsigned t0_; unsigned t_ = sceKernelGetSystemTimeLow(); if (!t0_) t0_ = t_;
    if (t_ - t0_ > 2000000u && dalN) { char s_[280]; snprintf(s_, sizeof s_, "DAL877 frames=%u capped=%u avg_dd=%u full_dd=%.0f out=%u high=%u nodata=%u far=%u skip=%u ey=%.1f t=%.1f chg=%u jump25=%u ramp=%d\n", dalN, dalCapN, dalSum / dalN, keep, dalWhy[0], dalWhy[1], dalWhy[2], dalWhy[3], dalWhy[4], eyeY, smkT, dalChg, dalJump, dalRampOn); smkWrite(s_); dimLine(smkT); dalJumpAll += dalJump; dalChgAll += dalChg; dalChg = dalJump = 0; dalN = dalCapN = dalSum = 0; memset(dalWhy, 0, sizeof dalWhy); t0_ = t_; } }
  return keep;
}
