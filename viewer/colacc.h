// Коллизия-96: доступ к треугольникам и спискам клеток мира (col.h). COLRING_OLD=1 (по умолчанию) — прямое чтение, как было; 0 — через кольцо (viewer/colring.h).
#ifndef SHAR_COLACC_H
#define SHAR_COLACC_H
#include "col.h"
#if COLRING_OLD
#define CELL_SPAN(w, gx, gz, c, kb, ke) do { (kb) = (w)->refs + (w)->cellStart[c]; (ke) = (w)->refs + (w)->cellStart[(c) + 1]; } while (0)
#define TRI_AT(w, i) (&(w)->tri[i])
#else
#include "colring.h"
#define CELL_SPAN(w, gx, gz, c, kb, ke) do { if ((w)->ring) colRingCell((w)->ring, (uint32_t)(gx), (uint32_t)(gz), &(kb), &(ke)); else { (kb) = (w)->refs + (w)->cellStart[c]; (ke) = (w)->refs + (w)->cellStart[(c) + 1]; } } while (0)
#define TRI_AT(w, i) ((w)->ring ? (const ColTri *)colRingTri((w)->ring, (i)) : &(w)->tri[i])
#endif
#endif
