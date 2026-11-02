// phonebusy.h — «занято» у телефонной будки (ps2-twin P05:25, сверка g07b-msnview): чистые функции для main_actbooth.h / main_loop.h и хост-теста tests/g07b_phonebusy.
// Поведение оригинала своими словами:
//  - будки выключены, пока идёт миссия с принудительной машиной (от запуска миссии до её закрытия); флаг один на всех будок уровня;
//  - будка «берёт» кнопку △, только если в момент ВХОДА в её объём будки были включены; вошёл при выключенных — подсказки нет и △ молчит, даже если будки включились,
//    пока стоишь внутри (нужно выйти и войти);
//  - вошёл при включённых, потом будки выключились (миссия с машиной началась, пока стоишь в будке) — подсказка гаснет, △ даёт гудок «занято» один раз за нажатие и ничего не открывает;
//  - открывающаяся или открытая пауза, проваленная миссия, арест: △ молча игнорируется (гудка нет) — это условия снаружи (mvBoothBlocked, uiPaused), здесь не считаются.
// PHONEBUSY_OLD=1 — прежнее: будки всегда включены, гудка нет, подсказка у любой будки.
#ifndef PHONEBUSY_H
#define PHONEBUSY_H
#ifndef PHONEBUSY_OLD
#define PHONEBUSY_OLD 0
#endif
// миссия с принудительной машиной в ходе (cur >= 0, флаг FORCEDCAR у миссии, состояние — идёт / отсчёт / между этапами); состояния подаёт вызывающий
static inline int pbMissionOff(int cur, int forcedFlag, int running) { return !PHONEBUSY_OLD && cur >= 0 && forcedFlag && running; }
typedef struct { int k; int armed; } PbArm;   // k — будка, в объёме которой игрок (-1 — ни одной); armed — вошёл при включённых
static inline void pbStep(PbArm *a, int pk, int enabled) {   // каждый кадр: pk — номер будки под игроком или -1
  if (pk < 0) { a->k = -1; a->armed = 0; return; }
  if (pk != a->k) { a->k = pk; a->armed = enabled; }   // вход (или переход в объём другой будки): запоминаем, были ли включены
}
enum { PB_NONE = 0, PB_OPEN, PB_BUSY };
static inline int pbPress(const PbArm *a, int pk, int enabled) {   // △ у будки, остальные условия (пауза, провал) уже пропущены
  if (PHONEBUSY_OLD) return pk >= 0 ? PB_OPEN : PB_NONE;
  if (pk < 0 || a->k != pk || !a->armed) return PB_NONE;
  return enabled ? PB_OPEN : PB_BUSY;
}
static inline int pbHint(const PbArm *a, int pk, int enabled) {   // подсказка «Use Telephone Booth»
  if (PHONEBUSY_OLD) return pk >= 0;
  return pk >= 0 && a->k == pk && a->armed && enabled;
}
#endif
