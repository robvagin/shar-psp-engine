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
    uiNewsPose(.9f, &ang, &k, &al); CHK(fabsf(al - .5f) < 1e-4f && fabsf(k - (.25f + .85f) / 1.95f) < 1e-4f, "news t=.9");
    float revs = .9f / (.06f * 1.5f); CHK(fabsf(revs - 10.f) < 1e-3f, "news 10 оборотов к 0,9 с"); CHK(fabsf(ang - 2.f * UI_PI * (revs - 15.f)) < 1e-3f, "news угол .9");
    uiNewsPose(1.8f, &ang, &k, &al); CHK(ang == 0.f && k == 1.f && al == 1.f, "news конец: прямая");
    uiNewsPose(5.f, &ang, &k, &al); CHK(ang == 0.f && k == 1.f && al == 1.f, "news после конца неподвижна"); }
  // счёт монет: въезд и выезд 0,2 с
  CHK(fabsf(UI_COIN_IN - .2f) < 1e-6f && fabsf(UI_COIN_OUT - .2f) < 1e-6f, "coins 0,2 с");
  printf(fails ? "ПРОВАЛ %d\n" : "OK\n", fails);
  return fails != 0;
}
