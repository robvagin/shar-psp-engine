// Хост-проверка кривых сверки g09a-hud (ui_anim.h п. 2д): этап пройден, MISSION COMPLETE, отсчёт; отрицательный контроль — прежняя пружина.
// cc -I viewer -o /tmp/anim_sverka tests/g09a/anim_sverka.c -lm && /tmp/anim_sverka
#include <stdio.h>
#include <math.h>
int uiBubble992Old, uiBubble1040; float uiCountdownDur = 1.0f;
#include "ui_anim.h"
static int fails;
#define CHK(c, msg) do { if (!(c)) { printf("FAIL %s\n", msg); fails++; } } while (0)
int main(void) {
  float s, a;
  // этап пройден: 1,0 с, √доли ·1,25, виден до 64 %
  CHK(uiPopState(UI_POP_STAGE, 1, 0.f, &s, &a) && s == 0.f && a == 1.f, "stage t=0");
  uiPopState(UI_POP_STAGE, 1, .25f, &s, &a); CHK(fabsf(s - 1.25f * .5f) < 1e-4f && a == 1.f, "stage t=.25");
  uiPopState(UI_POP_STAGE, 1, .64f, &s, &a); CHK(fabsf(s - 1.25f * .8f) < 1e-4f && a == 1.f, "stage t=.64 виден");
  uiPopState(UI_POP_STAGE, 1, .8f, &s, &a); CHK(fabsf(a - .528f) < 2e-3f, "stage t=.8: альфа по размеру 0,53 (проверка по исходнику)");
  CHK(!uiPopState(UI_POP_STAGE, 1, 1.0f, &s, &a) && a == 0.f, "stage конец 1,0 с");
  // MISSION COMPLETE: 2,5 с, 1,25·доля^(1/8), к 72 % размер ≈1,2
  uiPopState(UI_POP_MCOMP, 1, .72f * 2.5f, &s, &a); CHK(fabsf(s - 1.25f * powf(.72f, .125f)) < 1e-4f && s > 1.19f && s < 1.21f && a == 1.f, "mcomp 72%");
  uiPopState(UI_POP_MCOMP, 1, .05f, &s, &a); CHK(s > .6f && s < .85f, "mcomp почти сразу около 0,7");
  CHK(!uiPopState(UI_POP_MCOMP, 1, 2.5f, &s, &a), "mcomp конец 2,5 с");
  // отсчёт: 3,0·доля^(1/4), виден до 48 %
  uiPopState(UI_POP_CDOWN, 1, .48f, &s, &a); CHK(fabsf(s - 3.f * powf(.48f, .25f)) < 1e-4f && s > 2.49f && s < 2.51f && a == 1.f, "cdown 48%: размер 2,5");
  uiPopState(UI_POP_CDOWN, 1, .74f, &s, &a); CHK(fabsf(a - (1.f - (3.f * powf(.74f, .25f) - 2.5f) / .5f)) < 1e-4f, "cdown 74%: альфа по размеру");
  uiCountdownDur = 2.f; uiPopState(UI_POP_CDOWN, 1, 1.f, &s, &a); CHK(fabsf(s - 3.f * powf(.5f, .25f)) < 1e-4f, "cdown своя длительность позиции");
  // снятый баннер пропадает сразу (в оригинале группа выключена)
  CHK(!uiPopState(UI_POP_STAGE, 0, 0.f, &s, &a), "stage снят");
  // отрицательный контроль: прежняя пружина BANNER держится дольше и начинает с 0,4, а не с нуля
  uiPopState(UI_POP_BANNER, 1, 0.f, &s, &a); CHK(s > .39f && s < .41f, "контроль: прежняя пружина начинается с 0,4");
  // газета: 1,8 с, масштаб 0,13…1, прозрачность = доля, 10 оборотов к 0,9 с и ровно 15 к 1,8 с, потом неподвижна и прямая
  { float ang, k, al; uiNewsPose(0.f, &ang, &k, &al); CHK(fabsf(k - .25f / 1.95f) < 1e-4f && al == 0.f, "news t=0");
    uiNewsPose(.9f, &ang, &k, &al); CHK(al == 1.f && fabsf(k - (.25f + .85f) / 1.95f) < 1e-4f, "news t=.9");
    float revs = .9f / (.06f * 1.5f); CHK(fabsf(revs - 10.f) < 1e-3f, "news 10 оборотов к 0,9 с"); CHK(fabsf(remainderf(ang - 2.f * UI_PI * (revs - 15.f), 2.f * UI_PI)) < 1e-3f, "news угол .9 (с точностью до целых оборотов)");
    uiNewsPose(1.8f, &ang, &k, &al); CHK(ang == 0.f && k == 1.f && al == 1.f, "news конец: прямая");
    uiNewsPose(5.f, &ang, &k, &al); CHK(ang == 0.f && k == 1.f && al == 1.f, "news после конца неподвижна"); }
  // №1088: мягкая посадка газеты. Таблица последних кадров (60 к/с): масштаб, угол, угловая скорость об/с; скорость монотонно к нулю, ускорение без скачка, без перелёта
  { const float dt = 1.f / 60.f; float pa = 0, ps = 0, pw = 0, pacc = 0; int n = 0, mono = 1, over = 0, jerk = 0; float wEnd = 0, wMax = 0;
    printf("кадр  t     масштаб  угол(об)  скорость(об/с)\n");
    for (float t = 1.8f - 14 * dt; t <= 1.8f + 1e-4f; t += dt, n++) {
      float ang, k, al; uiNewsPose(t, &ang, &k, &al); float w = n ? (ang - pa) / dt / (2.f * UI_PI) : 0.f, acc = n > 1 ? (w - pw) / dt : 0.f;
      if (n > 3 && w > pw + 1e-3f) mono = 0;
      if (k > 1.0001f || (n && k < ps - 1e-5f)) over = 1;
      if (n > 4 && fabsf(acc - pacc) > 150.f) jerk = 1;   // скачок ускорения больше 150 об/с² за кадр
      printf("%2d  %.3f  %.4f  %8.4f  %7.3f\n", n, t, k, ang / (2.f * UI_PI), w);
      pa = ang; ps = k; pw = w; pacc = acc; wEnd = w; if (fabsf(w) > wMax) wMax = fabsf(w); }
    CHK(mono, "news посадка: скорость вращения только убывает"); CHK(!over, "news посадка: масштаб не перелетает единицу и не откатывается"); CHK(!jerk, "news посадка: ускорение без скачка");
    CHK(fabsf(wEnd) < 0.25f, "news посадка: к последнему кадру скорость около нуля"); uiNewsPose(1.8f, &pa, &ps, &pw); CHK(pa == 0.f && ps == 1.f, "news посадка: встала прямой, масштаб 1");
    // влёт тот же: до 65 % времени масштаб и прозрачность как у оригинала, угол — до целых оборотов
    for (float t = .05f; t < 1.17f; t += .05f) { float a1, k1, l1, a2, k2, l2; uiNewsPose(t, &a1, &k1, &l1); uiNewsPoseOld(t, &a2, &k2, &l2);
      CHK(fabsf(k1 - k2) < 1e-6f && fabsf(remainderf(a1 - a2, 2.f * UI_PI)) < 1e-3f, "news влёт как у оригинала до 65 %"); }
    // стык: скорость вращения не прыгает (разница < 3 %)
    { float a1, k1, l1, a2, k2, l2, e = .002f; uiNewsPose(1.17f - e, &a1, &k1, &l1); uiNewsPose(1.17f, &a2, &k2, &l2); float w1 = (a2 - a1) / e;
      float b1, b2; uiNewsPose(1.17f + e, &b1, &k1, &l1); float w2 = (b1 - a2) / e; CHK(fabsf(w2 - w1) < .03f * fabsf(w1), "news стык: скорость вращения не прыгает"); }
    // отрицательный контроль: прежний закон обрывается на 4,2 об/с
    { float a1, a2, k, l; uiNewsPoseOld(1.8f - dt, &a1, &k, &l); uiNewsPoseOld(1.8f, &a2, &k, &l); float w = fabsf(a2 - a1) / dt / (2.f * UI_PI);
      printf("контроль (TEST83): скорость в конце %.2f об/с\n", w); CHK(w > 3.f, "контроль: старая газета встаёт на скорости ~4 об/с"); } }
  // №1093: значок цели с пульсом и клин не выходят за круг. Цель далеко за краем: центр на пределе, углы значка (худший случай квадрата) внутри круга на всех фазах пульса
  { const float clip = 60.f, w = 16.f, h = 14.f; int outNew = 0, outOld = 0; float worst = 0.f;
    for (int ph = 0; ph < 100; ph++) { float ks = 1.25f + .25f * sinf(2.f * UI_PI * (ph / 100.f * .5f) / .5f);
      float limN = uiRadarIconLim(clip, w, h, ks), limO = clip - (w > h ? w : h) * 0.35f, hd = .5f * ks * sqrtf(w * w + h * h);
      float reachN = floorf(limN + .5f) + hd, reachO = limO + hd;   // дальний угол значка от центра круга (центр по оси, округление до пикселя)
      if (reachN > clip) outNew++; if (reachO > clip) outOld++; if (reachN > worst) worst = reachN; }
    printf("значок: худший вылет нового %.2f при круге %.0f; пульс с вылетом: новый %d, TEST83 %d фаз из 100\n", worst, clip, outNew, outOld);
    CHK(outNew == 0, "№1093: новый значок целиком в круге на всех фазах"); CHK(outOld > 0, "контроль: у TEST83 значок вылезает за круг");
    float x = 100.f, y = 3.f; uiRadarClampPt(0.f, 0.f, 59.f, &x, &y); CHK(sqrtf(x * x + y * y) <= 59.001f && x > 58.f, "клин: вершина подтянута к краю");
    x = 10.f; y = 20.f; uiRadarClampPt(0.f, 0.f, 59.f, &x, &y); CHK(x == 10.f && y == 20.f, "клин: вершина внутри не тронута"); }
  // №1123: газета начинает невидимой и проявляется за первые 45 % влёта; потом непрозрачна; контроль TEST84 — прозрачность равна доле времени
  { float a0, k0, l0, a1, k1, l1; uiNewsPose(0.f, &a0, &k0, &l0); uiNewsPose(1e-4f, &a1, &k1, &l1); CHK(l0 == 0.f && l1 < 1e-4f, "news проявление: первый кадр прозрачен");
    int mono = 1; float pl = -1.f; for (float t = 0.f; t <= 1.8f; t += 1.8f / 120.f) { float a, k, l; uiNewsPose(t, &a, &k, &l); if (l < pl - 1e-6f || l < 0.f || l > 1.f) mono = 0; pl = l; }
    CHK(mono, "news проявление: прозрачность только растёт, 0…1");
    float a, k, l10, l50, l90, ll; uiNewsPose(.18f, &a, &k, &l10); uiNewsPose(.9f, &a, &k, &l50); uiNewsPose(1.62f, &a, &k, &l90); uiNewsPose(.81f, &a, &k, &ll);
    printf("газета: прозрачность на 0 / 10 / 50 / 90 %%: 0.00 %.3f %.3f %.3f (TEST84: 0.00 0.100 0.500 0.900)\n", l10, l50, l90);
    CHK(l10 < .2f && l50 == 1.f && l90 == 1.f && fabsf(ll - 1.f) < 1e-5f, "news проявление: к 10 % мало, с 45 % полная"); }
  // №1129: обрезка по кругу. Клин (острие в центре) и значки далеко за краем: ни одной вершины за кругом, внутри круга то же, текстурные координаты линейны
  { const float cx = 80.f, cy = 70.f, R = 59.f; int bad = 0, cases = 0; float worstD = 0.f;
    for (int ang = 0; ang < 360; ang += 5) for (int far = 0; far < 3; far++) {
      float th = (float)ang * UI_PI / 180.f, L = R * (1.f + .6f * (float)(far + 1)), wd = 9.f;
      UiCV tri[3] = {{cx, cy, 0, 0, {255, 255, 0, 200}}, {cx + L * sinf(th) - wd * cosf(th), cy - L * cosf(th) - wd * sinf(th), 0, 0, {255, 255, 0, 200}}, {cx + L * sinf(th) + wd * cosf(th), cy - L * cosf(th) + wd * sinf(th), 0, 0, {255, 255, 0, 200}}};
      UiCV out[UI_CLIP_CAP]; int m = uiClipCircle(tri, 3, out, cx, cy, R); cases++;
      if (m < 3) { bad++; continue; }
      for (int i = 0; i < m; i++) { float d = sqrtf((out[i].x - cx) * (out[i].x - cx) + (out[i].y - cy) * (out[i].y - cy)); if (d > worstD) worstD = d; if (d > R + 1e-3f) bad++; }
      // острие осталось на месте, клин указывает на цель: среднее направление результата близко к направлению на цель
      float mx = 0.f, my = 0.f; for (int i = 0; i < m; i++) { mx += out[i].x - cx; my += out[i].y - cy; }
      float dir = atan2f(mx, -my), dd = remainderf(dir - th, 2.f * UI_PI); if (fabsf(dd) > .25f) bad++; }
    printf("клин: %d случаев, дальняя вершина после обрезки %.3f при круге %.0f, ошибок %d\n", cases, worstD, R, bad);
    CHK(bad == 0, "клин: за кругом ни одной вершины, направление сохранено");
    // отрицательный контроль: без обрезки вершины клина далеко за кругом
    CHK(sqrtf((float)((cx + R * 2.8f - cx) * (cx + R * 2.8f - cx))) > R, "клин: без обрезки был бы за кругом");
    // значок 16x14 с пульсом 1,5 на самом краю и за ним: углы режутся, текстурная координата u линейна по x
    int bad2 = 0, cut = 0; float worstI = 0.f;
    for (int ang = 0; ang < 360; ang += 3) for (float ks = 1.f; ks <= 1.5f; ks += .125f) for (float off = 0.f; off <= 1.f; off += .5f) {
      float th = (float)ang * UI_PI / 180.f, ox = cx + (R - 4.f + off * 12.f) * sinf(th), oy = cy - (R - 4.f + off * 12.f) * cosf(th), hw = 8.f * ks, hh = 7.f * ks;
      UiCV q[4] = {{ox - hw, oy - hh, 0.f, 0.f, {0}}, {ox + hw, oy - hh, 16.f, 0.f, {0}}, {ox + hw, oy + hh, 16.f, 14.f, {0}}, {ox - hw, oy + hh, 0.f, 14.f, {0}}};
      UiCV out[UI_CLIP_CAP]; int m = uiClipCircle(q, 4, out, cx, cy, R);
      if (m == 0) continue;
      if (m != 4) cut++;
      for (int i = 0; i < m; i++) { float d = sqrtf((out[i].x - cx) * (out[i].x - cx) + (out[i].y - cy) * (out[i].y - cy)); if (d > worstI) worstI = d; if (d > R + 1e-3f) bad2++;
        float eu = (out[i].x - (ox - hw)) / (2.f * hw) * 16.f, ev = (out[i].y - (oy - hh)) / (2.f * hh) * 14.f; if (fabsf(out[i].u - eu) > .01f || fabsf(out[i].v - ev) > .01f) bad2++; } }
    printf("значок: обрезан в %d случаях, дальняя вершина %.3f при круге %.0f, ошибок %d\n", cut, worstI, R, bad2);
    CHK(bad2 == 0 && cut > 0, "значок: за кругом ни одной вершины, текстурные координаты верны");
    // значок целиком внутри не тронут: те же 4 вершины
    UiCV in4[4] = {{cx - 8, cy - 7, 0, 0, {0}}, {cx + 8, cy - 7, 16, 0, {0}}, {cx + 8, cy + 7, 16, 14, {0}}, {cx - 8, cy + 7, 0, 14, {0}}}; UiCV o4[UI_CLIP_CAP];
    CHK(uiClipCircle(in4, 4, o4, cx, cy, R) == 4, "значок внутри круга не режется"); }
  // счёт монет: въезд и выезд 0,2 с
  CHK(fabsf(UI_COIN_IN - .2f) < 1e-6f && fabsf(UI_COIN_OUT - .2f) < 1e-6f, "coins 0,2 с");
  printf(fails ? "ПРОВАЛ %d\n" : "OK\n", fails);
  return fails != 0;
}
