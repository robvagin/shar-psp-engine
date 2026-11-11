// Служебно (окно «Оптимизация», 24.10): мкс, вызовы и слова GE по участкам кадра, строка XSUB. Включается только «flag xsub 1» в сценарии тура (нужен smokelog);
// Участки mvDraw3D (msn_view.c, персонажи миссий, x_npc): 50 шапка кадра, 51 выбор клипа/разговор, 52 skinPlay, 53 проверки видимости, 54 поза (выборка, палитра), 55 поправка стоп, 56 skinPlace+skinDraw, 57 — число персонажей в позе.
// в сборке без смоука (SHR_REL_SMK) макросы пусты: кода нет. Цена отсечки ≈ 0,5 мкс эмулятора (sceKernelGetSystemTimeLow + sceGuCheckList), ≈90 отсечек на кадр.
#ifndef XSUB_H
#define XSUB_H
#include "release.h"
#if SHR_REL_SMK || !defined(__psp__)   // хост-гейты (tests/toon/hull901.py и др.) — пусто
#define XS_N 64   // 24.10 (окно «Меню», боты Мака): game.c объявляет xsubU[XS_N] и в хост-сборке — без размера ломались все tools/bots (missioner, chase, explorer, nav), «use of undeclared identifier XS_N»
#define XS_T() 0u
#define XS_W() 0u
#define XS_LAP(i, t0, w0) ((void)0)
#define XS_ADD(i, n) ((void)0)
#define XS_ADD_SK(i, n) ((void)0)
#define XS_SKIN(v) ((void)0)
#define XS_CAR(v) ((void)0)
#define XS_FLAG(v) ((void)0)
#define XS_FRAME(on) ((void)0)
#else
#include <pspkernel.h>
#include <pspgu.h>
#define XS_N 64
extern unsigned xsubU[XS_N], xsubC[XS_N], xsubW[XS_N]; extern int xsubOn, xsubSkin, xsubCar;
#define XS_T() (xsubOn ? sceKernelGetSystemTimeLow() : 0u)
#define XS_W() (xsubOn ? (unsigned)sceGuCheckList() : 0u)
// отсечка: прошедшее с t0 и слова списка с w0 — в участок i; t0, w0 = сейчас
#define XS_LAP(i, t0, w0) do { if (xsubOn) { unsigned n_ = sceKernelGetSystemTimeLow(), v_ = (unsigned)sceGuCheckList(); xsubU[i] += n_ - (t0); xsubC[i]++; xsubW[i] += v_ >= (w0) ? v_ - (w0) : 0; (t0) = n_; (w0) = v_; } } while (0)
#define XS_ADD(i, n) do { if (xsubOn) xsubC[i] += (n); } while (0)
#define XS_ADD_SK(i, n) do { if (xsubOn && xsubSkin) xsubC[i] += (n); } while (0)   // только пока рисуется игрок (XS_SKIN)
extern int xsubFlag;
#define XS_FLAG(v) (xsubFlag = (int)(v))   // flag xsub 1 - участки кадра; flag xsub 2 - ещё и стадии каждого куска машины в drawPieceM (дороже втрое)
#define XS_FRAME(on) (xsubOn = (on) && xsubFlag > 0)   // on = smkOn
#define XS_SKIN(v) (xsubSkin = (v))
#define XS_CAR(v) (xsubCar = (v) && xsubFlag >= 2)
#endif
#endif
