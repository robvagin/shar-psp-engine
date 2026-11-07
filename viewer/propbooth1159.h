// №1159 (g11, регресс скорости TEST83→85): после №1138 контур остался только у телефонной будки, но проход контура каждый кадр собирал стейтпропы у глаза из сетки (spGridCollect) и для каждого
// кандидата заново проверял «это будка?» (чтение 7 записей кусков). До №1096 регулятор кадра при перегрузке пропускал проход целиком, теперь он идёт всегда. Список будок уровня не меняется:
// строим его один раз на уровень (по указателю на экземпляры и их числу) и обходим только его (0–2 записи на уровень вместо десятков кандидатов). Состав тот же: будка = ptIsBooth.
// PROPBOOTH1159_OLD=1 — сетка и проверка каждого кандидата, как в TEST85. Чистая арифметика над StateProps, без PSP (читает proptoon.h и тест)
#ifndef PROPBOOTH1159_H
#define PROPBOOTH1159_H
#include <stdlib.h>
#include "stateprops.h"
#include "propink1138.h"
#ifndef PROPBOOTH1159_OLD
#define PROPBOOTH1159_OLD 0
#endif
// №1159 п.2 (разбор TEST86: наибольший кусок кучи к концу тура 1,5–2,3 МБ против 2,3–5,3 в TEST85, на 11-й загрузке пакет предметов 1,4 МБ не влез): список будок брал по 4 байта на каждый экземпляр
// уровня (тысячи) и не освобождался при выгрузке — оставался посреди кучи между уровнями. Теперь: два прохода — считаем будки (0–2 на уровень), берём ровно столько, и ptBoothFree() отдаёт
// память при выгрузке уровня (levelUnload). PROPBOOTHMEM1159_OLD=1 — как в TEST86: размер по числу экземпляров, не освобождается
#ifndef PROPBOOTHMEM1159_OLD
#define PROPBOOTHMEM1159_OLD 0
#endif
static int ptIsBooth(const StateProps *sp, const SpType *T) {
  if (T->nPiece != 7) return 0;
  const SpPiece *P = &sp->piece[T->firstPiece];
  return propInkIsBooth(T->nPiece, T->kind, P[1].nv, P[6].nv);
}
static unsigned *ptBoothL, ptBoothN, ptBoothCap; static const void *ptBoothKey; static unsigned ptBoothNi;
// список номеров экземпляров-будок; пересобирается при смене уровня (другие экземпляры или их число)
static unsigned ptBoothEnsure(const StateProps *sp) {
  if (ptBoothKey == (const void *)sp->inst && ptBoothNi == sp->ninst && ptBoothKey) return ptBoothN;
  ptBoothKey = sp->inst; ptBoothNi = sp->ninst; ptBoothN = 0;
  unsigned need = sp->ninst ? sp->ninst : 1;
  if (!PROPBOOTHMEM1159_OLD) { need = 0; for (unsigned i = 0; i < sp->ninst; i++) if (ptIsBooth(sp, &sp->type[sp->inst[i].type])) need++; if (!need) need = 1; if (ptBoothCap != need) { free(ptBoothL); ptBoothL = 0; ptBoothCap = 0; } }
  if (ptBoothCap < need) { free(ptBoothL); ptBoothL = (unsigned *)malloc(sizeof(unsigned) * need); ptBoothCap = ptBoothL ? need : 0; }
  if (!ptBoothL) return 0;
  for (unsigned i = 0; i < sp->ninst; i++) if (ptIsBooth(sp, &sp->type[sp->inst[i].type])) ptBoothL[ptBoothN++] = i;
  return ptBoothN;
}
// выгрузка уровня: память списка обратно в кучу, ключ сброшен (следующий уровень строит список заново)
static void ptBoothFree(void) {
  if (PROPBOOTHMEM1159_OLD) return;
  free(ptBoothL); ptBoothL = 0; ptBoothCap = 0; ptBoothN = 0; ptBoothKey = 0; ptBoothNi = 0;
}
#endif
