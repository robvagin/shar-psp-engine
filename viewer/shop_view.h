// Экран покупки у продавца машин — Scrooby PurchaseRewards.scr оригинала (пункт 6 ведущего 29.09). Логика лавки (каталог,
// кошелёк, принадлежность, продавцы) — viewer/shop.c + viewer/game_msn.c, общая с ботами; здесь то, что видит игрок на PSP:
//   сцена подиума reward-<k>.shr (k — номер машины среди машин на продажу уровня; FE1, tools/shop_export.py; рисует viewer/frontend3d.c): шторы, колонны, подиум,
//     машина крутится (оборот 121 кадр / 30 к/с из данных rewardbg.p3d)
//   страница shop.shr (UIP1, tools/shop_export.py; рисует viewer/ui.c uiScr*): имя, «150 coins / to purchase», звёзды
//     (SetCarAttributes), «□ Show Stats», «✕ Purchase», «△ Cancel», ◄ ►, счёт монет
// Нет пакетов рядом с EBOOT — экрана нет (shopViewOn 0), лавку показывает прежняя карточка брифинга (msn_view.c).
//
// Ход экрана (запись PS2 14:03.6–14:09.5, кадры 10/с; правила порта по записи, данных о времени нет):
//   △ у продавца -> ирис закрывается над городом -> сцена тёмная 0.5 с (шторка PreviewLightCover, текст уже виден)
//   -> свет 0.4 с, с ним машина и счёт монет -> ✕: цена исчезает, монеты убывают 1 с, машина уходит с подиума вместе с
//   последней монетой -> 0.3 с пустой подиум -> свет гаснет 0.3 с (и счёт) -> ирис -> город. △/○ — сразу гаснет свет.
//
// Порядок в main.c: после mvTick —   ev = shopViewTick(mvShop(), dir, wallDt);  ev: ирис (transitClose) над последним
//   кадром; SHOPV_EV_CLOSE — после ириса shopViewEnd(mvShop()). Кнопки лавки — shopViewInput (msn_view.c, ветка брифинга).
//   3D: shopViewDraw3D() последним в 3D (чистит цвет и глубину); 2D: shopViewDraw2D() вместо HUD.
#ifndef SHOP_VIEW_H
#define SHOP_VIEW_H
#include "shop.h"

enum { SHOPV_EV_NONE = 0, SHOPV_EV_OPEN = 1, SHOPV_EV_CLOSE = 2 };
enum { SHOPV_IN = 0, SHOPV_IDLE, SHOPV_BOUGHT, SHOPV_OUT, SHOPV_DONE };

int  shopViewTick(Shop *s, const char *dir, float dt);   // dir — папка пакетов уровня с '/' на конце; -> SHOPV_EV_*
int  shopViewOn(void);                                   // 1 — экран покупки на экране (город не рисуется)
void shopViewInput(Shop *s, unsigned hit);               // нажатия кадра (PSP_CTRL_*): ◄ ► ✕ □ △ ○
void shopViewEnd(Shop *s);                               // после ириса выхода: пакеты освобождены, лавка закрыта
void shopViewDraw3D(void);                               // в открытом списке GE, последним в 3D
void shopViewDraw2D(Shop *s);                            // шторка света + страница (вместо uiDraw)
extern int (*shopViewOpenFn)(const char *fn);   // Build 95 П2: открытие пакета ведёт ldrRun (0 — открыт)
extern int (*shopViewHeal)(unsigned need);   // Будка-93 (1233): ступени лестницы под пакет лавки; 1 — место есть
extern void (*shopViewFailNote)(const char *what);   // сообщение игроку при отказе
extern int shopViewFailTest;   // служебное: flag shopfail N
void shopViewSetLog(void (*log)(const char *line));      // строки SHOPV (smoke.txt)
int  shopViewPhase(void);                                // SHOPV_* (служебное)
void shopViewPlant(int off);                             // отрицательный контроль гейта: 1 — экрана нет (прежняя карточка)
#endif
