// phone_view.h — телефонная будка (правка Роба 94, 30.09): экран PhoneBooth.scr оригинала — галерея машин уровня на подиуме
// (пакет build/phone.shr: страница и 2D-картинки машин cars2d, tools/shop_export.py), ◄ ► машина, ✕ вызвать (есть у игрока), △ ○ назад.
// Музыка экрана — событие композиции Store (Store_region -> Muzak), звуки — frontend.spt (scroll, accept, back, locked_out)
#ifndef PHONE_VIEW_H
#define PHONE_VIEW_H
#include "shop.h"

enum { PHONEV_EV_NONE = 0, PHONEV_EV_CLOSE = 2 };

int  phoneViewOpen(Shop *s, const char *dir);    // dir — папка пакетов уровня с '/'; 0 — открыт
void phoneViewWarm(const char *dir);
void phoneViewWarmOld(int old);
int  phoneViewOn(void);
int  phoneViewTick(Shop *s, float dt);           // -> PHONEV_EV_CLOSE, когда экран погас
void phoneViewInput(Shop *s, unsigned hit);
extern int boothLR1057Old;   // №1057: 1 — в будке зажатые L R не повторяются (как в TEST73)
extern int boothLR1038Old;   // №1038: 1 — выбор машины в будке только ◄ ► (как в TEST73); 0 — ещё L и R

void phoneViewDraw3D(void);
int phoneViewHusk(int *r);   // №557: прямоугольник картинки (px), если в будке сейчас показывается 3D-остов; 0 — нет
void phoneViewHusk3DForce(int on);   // №569: принудительный показ 3D-остова в будке (для тестов / flag phonehusk3d)
void phoneViewDraw2D(Shop *s);
const char *phoneViewEnd(Shop *s);               // экран снят: выбранная машина (имя) или NULL
void phoneViewSetCar(const char *name, float damage);   // перед открытием: машина игрока (NULL — своя уровня) и урон 0..1
void phoneViewSetProgress(int reached);   // №682: старший уровень, открытый в сохранении (svLvReached)
void phoneViewSetLog(void (*log)(const char *));   // строки PHONEV open/sel/deny/accept в smoke.txt
#endif
