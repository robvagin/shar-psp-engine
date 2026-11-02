// №1138 (g11): какие стейтпропы обводятся в мультике — только телефонная будка (чистая арифметика, без PSP; читает proptoon.h и тест). PROPINK1138_OLD=1 — обводятся все годные, как в TEST84.
#ifndef PROPINK1138_H
#define PROPINK1138_H
#ifndef PROPINK1138_OLD
#define PROPINK1138_OLD 0
#endif
// будка: 7 кусков, кусок 1 — плоский восьмиугольник из 18 вершин (или убранный boothBase1052: 0), кусок 6 — 42 вершины (или 0)
static inline int propInkIsBooth(unsigned nPiece, int kind, unsigned nv1, unsigned nv6) { return nPiece == 7 && !kind && (nv1 == 18 || nv1 == 0) && (nv6 == 42 || nv6 == 0); }
// нужна ли обводка стейтпропа: будка — да; остальные — только в режиме TEST84
static inline int propInkWanted(int old, int isBooth) { return old ? 1 : isBooth; }
#endif
