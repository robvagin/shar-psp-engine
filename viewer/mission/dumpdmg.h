// A1.2 (сверка Fable 13.11, §4): кто неуязвим на этапе dump. В оригинале режим «бамперные машины» = у этапа нет привязок предметов к точкам пути.
//  - без привязок: машина игрока неуязвима весь этап при любом числе предметов; цель при одном предмете бьётся (шкала урона), при нескольких неуязвима;
//  - с привязками: неуязвима только цель, машина игрока получает урон как обычно.
// Было (круг 2, g07b Д6): машина игрока и цель неуязвимы, если предметов больше одного, в любом режиме. GAME_A12_DUMPDMG_OLD=1 — прежнее.
#ifndef MISSION_DUMPDMG_H
#define MISSION_DUMPDMG_H
#ifndef GAME_A12_DUMPDMG_OLD
int rb_GAME_A12_DUMPDMG_OLD __attribute__((weak));   /* 107б rollback.txt (viewer/rollback.h): прежний путь без пересборки */
#define GAME_A12_DUMPDMG_OLD rb_GAME_A12_DUMPDMG_OLD
#endif
static inline int dumpPlayerImmune(int nbind, int nitems) { return GAME_A12_DUMPDMG_OLD ? nitems > 1 : nbind == 0; }
static inline int dumpTargetImmune(int nbind, int nitems) { return GAME_A12_DUMPDMG_OLD ? nitems > 1 : (nbind > 0 || nitems > 1); }
#endif
