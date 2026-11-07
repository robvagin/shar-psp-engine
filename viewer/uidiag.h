// uidiag.h — №1186 (зависание «STALL phase=ui … SceGeQueue» у пользователя, PSP-3000, TEST88). Проверка каждого вызова рисования
// интерфейса: число вершин и площадь описывающего прямоугольника (экранные координаты, TRANSFORM_2D). Первый вызов с вершинами
// больше UIDIAG_VERTS или площадью больше экрана × UIDIAG_AREAK — запись UIDRAWBIG (функция, строка, число вершин, размер) для журнала
// и ящика; последний вызов и суммы кадра читают строка UIPEAK (main_loop.h) и stall.txt (сторож). Включение: #include "uidiag.h"
// ПОСЛЕ всех остальных include файла, где вызывается sceGuDrawArray. UIDIAG_OLD=1 — вызов как прежде, без проверки.
#ifndef UIDIAG_H
#define UIDIAG_H
#ifndef UIDIAG_OLD
#define UIDIAG_OLD 0
#endif
#define UIDIAG_VERTS 4096
#define UIDIAG_AREAK 4
#define UIDIAG_NBIG 8
typedef struct { const char *fn; int line, n, vt, w, h; unsigned cnt, seq; } UiBig;
typedef struct { const char *fn; int line, n, vt, w, h; unsigned seq; } UiLast;
extern UiBig uiBigRec[UIDIAG_NBIG]; extern unsigned uiBigN;   // первые вызовы (разные место+строка), cnt — повторы
extern UiLast uiLastDraw;                                      // последний вызов рисования интерфейса
extern unsigned uiDrawSeq, uiFrDraws, uiFrVerts, uiFrAreaPx, uiFrMaxVerts, uiFrMaxAreaPx;   // кадр (uiDiagFrame() — сброс)
extern unsigned uiSecDraws, uiSecVerts, uiSecMaxVerts, uiSecMaxAreaPx, uiSecAreaPx, uiSecFrames, uiSecFrAreaMax;
void uiDiagDraw(int prim, int vt, int n, const void *idx, const void *v, const char *fn, int line);
void uiDiagFrame(void);   // конец кадра интерфейса: кадровые суммы — в секундные
int uiDiagTakeBig(UiBig *out);   // забрать запись UIDRAWBIG (0 — нет новых)
#if !UIDIAG_OLD && !defined(UIDIAG_NOMACRO)
#define sceGuDrawArray(p, t, n, i, v) uiDiagDraw((p), (t), (n), (i), (v), __func__, __LINE__)
#endif
#endif
