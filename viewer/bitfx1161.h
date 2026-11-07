// №1161: какой спрайт берёт частица предмета без своей системы, по пресету SPFX_* (viewer/stateprops.h). Чистая таблица: хост-гейт tests/bitfx1161 сверяет её без эмулятора.
// BIT_SMOKE — дым машины car_damage3 (smoke3, серый, растёт до 3 раз, 3,3 с): для ударов и поломок не годится, читается как огонь. BITFX1161_OLD=1 — всё дымом, как в TEST86
#ifndef BITFX1161_H
#define BITFX1161_H
#include "stateprops.h"
#ifndef BITFX1161_OLD
#define BITFX1161_OLD 0
#endif
enum { BIT_SMOKE, BIT_DUST, BIT_WOOD, BIT_LEAF, BIT_HAY, BIT_TOMATO };
static inline int bit1161Sprite(int preset) {
  if (BITFX1161_OLD) return BIT_SMOKE;
  switch (preset) {
    case SPFX_WOOD: return BIT_WOOD;
    case SPFX_LEAVES: return BIT_LEAF;
    case SPFX_HAY: return BIT_HAY;
    case SPFX_TOMATO: return BIT_TOMATO;
    case SPFX_DUST: case SPFX_DEBRIS: default: return BIT_DUST;
  }
}
#endif
