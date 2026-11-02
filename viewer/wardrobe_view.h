// wardrobe_view.h — гардероб Гомера (правка Роба 107, 30.09): костюмы уровня (rewards.mfk BindReward "skin"), место выбора —
// ActionEvent PurchaseSkin в интерьерах (школа, Kwik-E-Mart); страница — вёрстка PhoneBooth.scr с картинками skins2d
// (build/wardrobe.shr, build/skins.txt — tools/skins_export.py). ◄ ► костюм, ✕ купить (монеты) и надеть, △ ○ назад
#ifndef WARDROBE_VIEW_H
#define WARDROBE_VIEW_H
#include "shop.h"

enum { WARDV_EV_NONE = 0, WARDV_EV_CLOSE = 2 };

int  wardrobeInit(const char *dir);                 // skins.txt уровня; -> костюмов (0 — нет)
int  wardrobeNear(float x, float y, float z);       // 1 — Гомер у места выбора костюма
int  wardrobeOpen(Shop *s, const char *dir, int worn);   // worn — номер надетого; 0 — открыт
int  wardrobeOn(void);
int  wardrobeTick(float dt);
void wardrobeInput(Shop *s, unsigned hit);
void wardrobeDraw3D(void);
void wardrobeDraw2D(Shop *s);
int  wardrobeEnd(void);                             // экран снят: номер костюма, который надеть (или -1)
const char *wardrobeName(int k);                    // сверка g07 круг 3: имя костюма k (h_fat, l_cool …)
const char *wardrobeFile(int k);                    // файл модели костюма k («homer.shr», «homer-h_fat_m.shr» …)
int  wardrobeSpot(int i, float out[4]);             // место выбора костюма i: x, y, z, радиус; 0 — больше мест нет
void wardrobeSetLog(void (*log)(const char *));     // строки WARDV open/sel/buy/deny/end в smoke.txt (гейт tests/shop/wardrobe_emu.sh)
#endif
