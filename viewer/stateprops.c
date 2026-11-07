// Стейтпропы и обломки: реализация (см. stateprops.h, docs/props_spec.md). Чистый C99, float, без malloc в кадре.
#include "stateprops.h"
#include "propbreak.h"
#include "sverka_shared.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

_Static_assert(sizeof(SpPiece) == 40, "SpPiece");
_Static_assert(sizeof(SpType) == 84, "SpType");
_Static_assert(sizeof(SpJoint) == 52, "SpJoint");
_Static_assert(sizeof(SpState) == 80, "SpState");
_Static_assert(sizeof(SpEvent) == 4, "SpEvent");
_Static_assert(sizeof(SpCall) == 8, "SpCall");
_Static_assert(sizeof(SpCol) == 64, "SpCol");
_Static_assert(sizeof(SpBill) == 40, "SpBill");
_Static_assert(sizeof(SpInst) == 32, "SpInst");

// Гипотезы (docs/props_spec.md §9, сверить по видео PS2):
#define SP_HIT_SPEED 2.f     // удар машиной засчитывается от 2 м/с сближения (как в веб-порте)
#define SP_COOL 0.3f         // новое событие от машины — только после 0.3 с без касания
#define SP_CAR_E 0.1f        // отскок машины от целого пропа
// №1101 (Роб 02.11, ответ на вопрос сверки по кустам-шарам cls 2: «усреднить между нашим, немножко проезжаемые, и тем, как они были в source файлах»): было два края.
//   наш (SPBUSH_SV_OLD=1, TEST83): куст не держит вовсе — насквозь на любой скорости, без торможения;
//   оригинал (SPBUSH_SV_OLD=0): твёрдое тело, как стена, на любой скорости, отскок SP_CAR_E 0,1.
// Середина (числа наши, не из оригинала): машина входит в куст медленнее BUSH_PASS_V (7 м/с, 25 км/ч) — куст держит как стена (отскок BUSH_E_MID = половина от целого пропа, 0,05, можно сдать назад);
// быстрее — продавливает насквозь, но скорость в момент входа падает на BUSH_BRAKE (60 %), без выталкивания и пружины (решение «проходит» держится, пока касание не кончилось).
// BUSHMID1101_OLD=1 — край «наш» (насквозь всегда); SPBUSH_SV_OLD=0 — край «оригинал».
#ifndef BUSHMID1101_OLD
#define BUSHMID1101_OLD 0
#endif
#define BUSH_PASS_V 7.0f
#define BUSH_BRAKE 0.6f
#define BUSH_PASS_T 1.0f        // с: после прохода куста следующий в ряду (кусты стоят рядами) тоже продавливается...
#define BUSH_PASS_V_CHAIN 3.0f  // ...если скорость входа не меньше 3 м/с,
#define BUSH_BRAKE_CHAIN 0.25f  // ...и тормозит слабее: 25 %
#define BUSH_E_MID (SP_CAR_E * .5f)
#define SP_SMASH_LOSS 0.3f   // потеря скорости машины при разрушении: 0.3 * m / (M + m)
#define SP_FX_RATE 20.f      // частиц в секунду, пока открыто окно EFX
#define SP_FX_BURST 8        // и столько сразу при входе в окно
// №1175 (Роб 03.11, L5 C1-6: «деревянные ограждения на автостраде при ударе, помимо звука, должны ещё разваливаться, как деревья»): у барьеров 0x3F0000F (donotenter, gatesmasha, railcross, туннельный забор —
// все AnimDynaPhys с вызовом BREAK) удар переводит предмет в состояние 1: вызов BREAK запускает обломок строки ATC (BreakableObject: у donotenter он без кусков, только система частиц
// l2_donotenter_fx_13Shape, у переезда — 4 куска и частицы). Частицы обломка (kind 1) при nativeBreakFx не слались никому: их должен был запустить вызывающий (fxBurstBreakable), но так делает только путь
// предметов (onPropKickFx), у стейтпропов вызывающего нет — оставался звук и 6 щепок запасного пресета. Теперь обломок, запущенный вызовом BREAK, играет свои частицы сам (ownFx).
// BARRIERFX1175_OLD=1 — как в TEST87: звук и 6 щепок пресета
static int breakOwnFx_;
#define SP_ZCONE 0.61f       // _Z-монеты: конус ±35° вокруг +Z пропа
// Правка Роба 297 (04.10, трамплин у L1 K2-6 к горе покрышек: «квадратные стоги сена — у оригинала разбиваются, у нас
// неразбиваемые»). Скорость удара о проп — сближение по оси, через которую кузов вошёл в объём последним (откат по скорости
// кузова: SAT по 4 осям в плане + вертикаль), а не по оси наименьшего перекрытия. Было: машина, слетая с трамплина на тюк сверху,
// входила в него вертикально (2.4–6.9 м/с вниз), перекрытие в плане выбирало боковую ось со сближением ~0 — тюк не разбивался,
// кузов отбрасывало вбок на 2–7 м (57 посадок из 60 на 5 тюках l1_haybale); краем/углом на скорости — 4–6 % ударов так же.
// -DSP_HIT_MINAXIS=1 — прежнее (красный гейта tests/smash/run.sh)
#ifndef SP_HIT_MINAXIS
#define SP_HIT_MINAXIS 0
#endif

// ---- сверка с исходником, окно g11-objects (02.11.2026): выключатели строк отчётов СВЕРКА_ПОЛНАЯ/P11_объекты_мира, P12, P16, P18. Все *_SV_OLD=1 — поведение TEST81
#ifndef SPIMP_SV_OLD
#define SPIMP_SV_OLD 0      // P16 Д6, P11-09: удар машины засчитывается по импульсу (масса машины x сближение > SP_HIT_IMPULSE), а не по скорости сближения 2 м/с
#endif
#define SP_HIT_IMPULSE 100.f
#ifndef SPFEATHER_SV_OLD
#define SPFEATHER_SV_OLD 0  // P11-09: касание не-ломаемого (типы 2, 3, 10) подаёт FEATHER_TOUCH до проверки силы
#endif
#ifndef SPBUSH_SV_OLD
#define SPBUSH_SV_OLD 1     // P11-08: куст-шар (тип 2) твёрдый, как стена. ВЫКЛЮЧЕНО до слова Роба: канон 27.09 п.4 «мягкая реакция с возможностью выехать», правка 173 (кусты насквозь). 0 — твёрдый
#endif
#ifndef SPKILL_SV_OLD
#define SPKILL_SV_OLD 0     // P11-20: вызов KILL_SPEED гасит скорость самого предмета, не машины (у стейтпропа скорости нет: машину не гасим)
#endif
#ifndef SPCOIN_SV_OLD
#define SPCOIN_SV_OLD 0     // P18 Д1: код 11 «монеты вперёд» = пять монет за вызов, имя SPAWN_25_COINS_Z только подпись; Hit & Run выключен — монет нет (P11-06)
#endif
#ifndef SPRAD_SV_OLD
#define SPRAD_SV_OLD 0      // P16-21, P18 Д6: вызов 8 RADIATE_FORCE (статуи Иедедайи) — толчок вверх 200 / масса в шаре 1,5 м
#endif
// смещение точки толчка от начала статуи Иедедайи (L2 и L5: начало (136,46; 8,04; −36,50), голова (135,03; 13,48; −37,39))
#define SP_RAD_DX -1.30f
#define SP_RAD_DY 5.16f
#define SP_RAD_DZ -1.41f
#ifndef SPLIE_SV_OLD
#define SPLIE_SV_OLD 0      // P11-07, P12-02/04, P11-17: сбитое лежит до перезагрузки зоны; разбитый ломаемый/двигаемый не возвращается; тип 10 после удара уходит из мира вне вида
#endif
#ifndef SPHNRHIT_SV_OLD
#define SPHNRHIT_SV_OLD 0   // P18 Д5: при выключенном H&R вместо монет — ещё одно событие HIT
#endif
#ifndef SPBOOM_SV_OLD
#define SPBOOM_SV_OLD 0     // P11-05, P11-Д1: взрыв машины (svBoom) бьёт стейтпропы в радиусе
#endif
#ifndef SPPHASE_SV_OLD
#define SPPHASE_SV_OLD 0    // P11-19, P12-11: каждый экземпляр стартует с случайным сдвигом анимации 0..10 с
#endif

static unsigned sprng = 777u;
static float sfr(void) { sprng = sprng * 1664525u + 1013904223u; return (sprng >> 8) * (1.f / 16777216.f); }

static void quatR(const float q[4], float R[9]) {   // R по столбцам
  float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0] = 1.f - 2.f * (y * y + z * z); R[1] = 2.f * (x * y + w * z); R[2] = 2.f * (x * z - w * y);
  R[3] = 2.f * (x * y - w * z); R[4] = 1.f - 2.f * (x * x + z * z); R[5] = 2.f * (y * z + w * x);
  R[6] = 2.f * (x * z + w * y); R[7] = 2.f * (y * z - w * x); R[8] = 1.f - 2.f * (x * x + y * y);
}
static void mulRv(const float R[9], const float v[3], float o[3]) {
  o[0] = R[0] * v[0] + R[3] * v[1] + R[6] * v[2];
  o[1] = R[1] * v[0] + R[4] * v[1] + R[7] * v[2];
  o[2] = R[2] * v[0] + R[5] * v[1] + R[8] * v[2];
}
static void mulRR(const float A[9], const float B[9], float o[9]) {
  for (int c = 0; c < 3; c++) mulRv(A, B + 3 * c, o + 3 * c);
}

#define ALIGN16(x) (((x) + 15u) & ~15u)

// №1137/1154 (Роб на L7: «проваливаюсь в кусты» в J9-8 и «кусты полностью прозрачные и проезжаются насквозь» в J9-5): в данных уровней на каждом уровне 20–26 кустов-стейтпропов (класс 2, высота 0,4–3 м, не шире 2,5 м полуразмера:
// l*_shrub*, спящие качающиеся кусты и подобные) стоят без объёма: nCol = 0, машина проезжает сквозь них как через воздух (и идёт речь о телах «у видимых кустов» — в оригинале куст-статика с объёмом). Теперь такому типу при загрузке
// даётся один объём-ящик по ящику его кусков (горизонтальные полуразмеры × BUSH_BODY_K = 0,8: крона шире тела; число наше, не из оригинала), и машина встречает его по правилу №1101 (медленно — держит, быстро — продавливает
// с торможением). Пешком Гомер по-прежнему проходит класс 2 (homer.c). BUSHFALL1137_OLD=1 — без тел, как было
#ifndef BUSHFALL1137_OLD
#define BUSHFALL1137_OLD 0
#endif
#ifndef BUSHAIR1201_OLD
#define BUSHAIR1201_OLD 0
#endif
#define BUSH_BODY_K .8f
// №1201б (дирижёр 05.11: «если тип 78 не растение — правило №1137 к нему не применять»): тип 78 L1 (52 на L2, 58 на L3…) — подбираемый гаечный ключ (tools/prp2_export.py add_wrenches: класс 2, масса 0, ATC 0, 14–20 экземпляров на уровень на крышах и башнях),
// остальные безмассовые типы класса 2 без объёма — карточки и подборы. Тело куста получает только то, что в данных кустом и является: звук удара hit_shrub (ATC 10, масса 5: l*_shrubround). BUSHCLS1201_OLD=1 — правило №1137 по размеру, как в TEST85…90
#ifndef BUSHCLS1201_OLD
#define BUSHCLS1201_OLD 0
#endif
static int isBushType(const SpType *T) {
  return T->cls == 2 && !T->kind && !T->nCol && T->nPiece && 2.f * T->h[1] >= .4f && 2.f * T->h[1] <= 3.f && fmaxf(T->h[0], T->h[2]) <= 2.5f && (BUSHCLS1201_OLD || (T->atc == 10 && T->mass > 0.f));
}
static void bushBodies(StateProps *sp, unsigned ncol) {
  if (BUSHFALL1137_OLD || !sp->ntype) return;
  unsigned nc = 0;
  for (unsigned t = 0; t < sp->ntype; t++) { const SpType *T = &sp->type[t]; if (isBushType(T)) nc++; }
  if (!nc) return;
  SpCol *arr = (SpCol *)calloc(ncol + nc, sizeof(SpCol)); SpType *ty = (SpType *)malloc(sp->ntype * sizeof(SpType)); if (!arr || !ty) { free(arr); free(ty); return; }
  if (ncol) memcpy(arr, sp->col, ncol * sizeof(SpCol));
  memcpy(ty, sp->type, sp->ntype * sizeof(SpType)); sp->typeOwn = ty; sp->type = ty;
  unsigned k = 0;
  for (unsigned t = 0; t < sp->ntype; t++) { SpType *T = &ty[t]; if (!isBushType(T)) continue;
    SpCol *C = &arr[ncol + k]; C->kind = 0; C->second = 0; memcpy(C->c, T->c, sizeof C->c);
    C->a[0] = 1.f; C->a[4] = 1.f; C->a[8] = 1.f; C->h[0] = T->h[0] * BUSH_BODY_K; C->h[1] = T->h[1]; C->h[2] = T->h[2] * BUSH_BODY_K;
    T->firstCol = (uint16_t)(ncol + k); T->nCol = 1; k++; }
  sp->colOwn = arr; sp->col = arr;
}
int spInitResident(StateProps *sp, const void *section, unsigned size, int detachedKeys) {
  memset(sp, 0, sizeof *sp);
  if (!section || size < 64) return -1;
  if ((uintptr_t)section & 15) return -2;
  const unsigned char *h = (const unsigned char *)section;
  if (memcmp(h, "SPR1", 4)) return -3;
  const uint32_t *u = (const uint32_t *)(h + 4);
  unsigned ntype = u[0], ninst = u[1], npiece = u[2], njoint = u[3], nkey = u[4], nvis = u[5], nstate = u[6],
           nevent = u[7], ncall = u[8], ncol = u[9], nbill = u[10], vertBytes = u[11], idxBytes = u[12], ndyn = u[13],
           nname = u[14];   // @60: имена систем частиц (часть 15), в старых файлах 0
  unsigned sizes[15] = {vertBytes, idxBytes, npiece * 40u, ntype * 84u, njoint * 52u, nkey * 28u, nvis * 4u,
                        nstate * 80u, nevent * 4u, ncall * 8u, ncol * 64u, nbill * 40u, ninst * 32u, ndyn, nname * 32u};
  const unsigned char *ptr[15]; unsigned off = 64;
  for (int k = 0; k < 15; k++) {
    if (!(k == 5 && detachedKeys) && (uint64_t)off + sizes[k] > size) return -4;
    if (k == 5 && detachedKeys) { ptr[k] = NULL; continue; }
    ptr[k] = h + off; off = ALIGN16(off + sizes[k]);
  }
  sp->vertBase = ptr[0]; sp->idxBase = (const unsigned short *)ptr[1];
  sp->piece = (const SpPiece *)ptr[2]; sp->type = (const SpType *)ptr[3]; sp->joint = (const SpJoint *)ptr[4];
  sp->key = (const float *)ptr[5]; sp->vis = (const uint32_t *)ptr[6]; sp->state = (const SpState *)ptr[7];
  sp->event = (const SpEvent *)ptr[8]; sp->call = (const SpCall *)ptr[9]; sp->col = (const SpCol *)ptr[10];
  sp->bill = (const SpBill *)ptr[11]; sp->inst = (const SpInst *)ptr[12]; sp->dynBreak = ptr[13];
  sp->ntype = ntype; sp->ninst = ninst; sp->npiece = npiece; sp->ndyn = ndyn;
  sp->fxName = nname ? ptr[14] : NULL; sp->nfxName = nname;
  // правка Роба 76: хвост BQG1 — покадровые ширина/высота/цвет квадов свечения (анимации BQG_ оригинала); нет — квады из данных
  if ((uint64_t)off + 16 <= size && !memcmp(h + off, "BQG1", 4)) {
    const uint32_t *b = (const uint32_t *)(h + off + 4); unsigned nb = b[0], nr = b[1];
    if (nb == nbill && (uint64_t)off + 16 + nb * 8ull + nr * 12ull <= size) {
      sp->bqgIdx = (const uint32_t *)(h + off + 16); sp->bqgRow = (const uint32_t *)(h + off + 16 + nb * 8); sp->nbqgRow = nr;
    }
  }
  // проверки ссылок
  unsigned jtot = 0, nslot = 0;   // nslot: слотов всех типов (32 на слово маски) — таблицы fxSys/fxJoint
  for (unsigned t = 0; t < ntype; t++) {
    const SpType *T = &sp->type[t];
    if ((unsigned)T->firstPiece + T->nPiece > npiece || (unsigned)T->firstJoint + T->nJoint > njoint ||
        (unsigned)T->firstState + T->nState > nstate || (unsigned)T->firstCol + T->nCol > ncol ||
        (unsigned)T->firstBill + T->nBill > nbill || !T->nState) return -5;
    unsigned nfr = T->frames & 0xFFFF, ntr = T->frames >> 16, vw = T->visWords > 1 ? T->visWords : 1;
    if (vw > 8) return -5;   // 256 слотов — номер слота u8
    if ((uint64_t)T->keyOff + (uint64_t)nfr * ntr > nkey) return -5;
    if (vw > 1 && (uint64_t)T->pvisX + (vw - 1) > nvis) return -5;
    int pvis = T->pvisMask != 0;
    for (unsigned w = 1; w < vw; w++) pvis |= sp->vis[T->pvisX + w - 1] != 0;
    if (pvis && (uint64_t)T->visOff + (uint64_t)nfr * vw > nvis) return -5;
    for (unsigned s = 0; s < T->nState; s++) {
      const SpState *S = &sp->state[T->firstState + s];
      if ((unsigned)S->firstEvent + S->nEvent > nevent || (unsigned)S->firstCall + S->nCall > ncall || S->nFx > 4 || S->nCall > 16) return -5;
      if (vw > 1 && (uint64_t)S->visX + (vw - 1) > nvis) return -5;
      for (unsigned k = 0; k < S->nFx; k++) if (S->fx[k].slot >= 32 * vw) return -5;
      if (S->autoTo != 0xFF && S->autoTo >= T->nState) return -5;
      for (unsigned e = 0; e < S->nEvent; e++) if (sp->event[S->firstEvent + e].to >= T->nState) return -5;
    }
    for (unsigned k = 0; k < T->nPiece; k++) {
      const SpPiece *P = &sp->piece[T->firstPiece + k];
      if (((P->flags >> 8) & 0xFF) >= T->nJoint || ((unsigned)P->flags >> 16 & 0xFF) >= 32 * vw || (uint64_t)P->vOff + (uint64_t)P->nv * 16 > vertBytes ||
          (!(P->flags & 16) && (uint64_t)P->iOff + (uint64_t)P->ni * 2 > idxBytes)) return -5;   // флаг 16: без индексов (nv = ni)
    }
    for (unsigned b = 0; b < T->nBill; b++) if (sp->bill[T->firstBill + b].joint >= T->nJoint || sp->bill[T->firstBill + b].slot >= 32 * vw) return -5;
    nslot += 32 * vw;
    for (unsigned j = 0; j < T->nJoint; j++) {
      const SpJoint *J = &sp->joint[T->firstJoint + j];
      if (J->parent >= (int)j || (J->track >= 0 && (unsigned)J->track >= ntr)) return -5;
    }
  }
  for (unsigned i = 0; i < ninst; i++) { if (sp->inst[i].type >= ntype) return -6; jtot += sp->type[sp->inst[i].type].nJoint; }
  // память: экземпляры, суставы, слоты (fxSys 2 Б + fxJoint 1 Б на слот; до 29.09 — ntype × 32, у типов до 32 слотов так же),
  // начала слотов типов (ntype + 1)
  sp->touchInst = -1;
  sp->mem = calloc(1, (size_t)ninst * sizeof(SpRun) + (size_t)jtot * 12 * sizeof(float) + (size_t)(ntype + 1) * 4 + (size_t)nslot * 3 + 16);
  if (!sp->mem) return -7;
  sp->run = (SpRun *)sp->mem;
  float *jm = (float *)(sp->run + ninst);
  sp->slotBase = (uint32_t *)(jm + (size_t)jtot * 12);
  sp->fxSys = (int16_t *)(sp->slotBase + ntype + 1); sp->fxJoint = (uint8_t *)(sp->fxSys + nslot);
  for (unsigned t = 0, b = 0; t <= ntype; t++) { sp->slotBase[t] = b; if (t < ntype) b += 32 * (sp->type[t].visWords > 1 ? sp->type[t].visWords : 1); }
  for (unsigned k = 0; k < nslot; k++) sp->fxSys[k] = -1;
  for (unsigned i = 0; i < ninst; i++) {
    SpRun *r = &sp->run[i]; const SpInst *I = &sp->inst[i];
    r->jm = jm; jm += sp->type[I->type].nJoint * 12;
    r->x = I->x; r->y = I->y; r->z = I->z; quatR(I->q, r->R); r->fall = 0;
    r->alive = 1; r->colOn = r->col2On = 1; r->state = 0; r->t = 0; r->callDone = 0; r->burst = 1;
    const SpState *S = &sp->state[sp->type[I->type].firstState];
    r->poseF = S->poseMin;
    // сверка g11, P11-19 и P12-11: оригинал даёт каждому стейтпропу при создании случайный сдвиг анимации 0..10 с (качающиеся деревья, вывески, крутящиеся предметы идут в разных фазах).
    // Было: все с кадра 0, в такт. Только циклы без вызовов и автоперехода (одноразовые позы и окна частиц не трогаем); сдвиг — хеш номера экземпляра (детерминированно). SPPHASE_SV_OLD=1 — как было
    if (!SPPHASE_SV_OLD && S->poseMode == 2 && !S->nCall && S->autoTo == 0xFF && !I->tag) {
      unsigned hq = (i + 1u) * 2654435761u; hq ^= hq >> 15; hq *= 2246822519u; hq ^= hq >> 13;
      r->t = (float)(hq & 0xFFFFu) * (10.f / 65536.f);
    }
  }
  bushBodies(sp, ncol);   // №1137/1154
  return 0;
}

int spInit(StateProps *sp, const void *section, unsigned size) { return spInitResident(sp, section, size, 0); }

void spFree(StateProps *sp) { free(sp->colOwn); free(sp->typeOwn); free(sp->follow); free(sp->followOk); free(sp->followT0); if (sp->run) for (unsigned i = 0; i < sp->ninst; i++) free(sp->run[i].jm0); free(sp->mem); memset(sp, 0, sizeof *sp); }   // №923: jm0 — отдельные выделения

static const SpType *TY(const StateProps *sp, unsigned i) { return &sp->type[sp->inst[i].type]; }
static const SpState *ST(const StateProps *sp, unsigned i) { return &sp->state[TY(sp, i)->firstState + sp->run[i].state]; }
static unsigned VW(const SpType *T) { return T->visWords > 1 ? T->visWords : 1; }
// слово w маски состояния S типа T (слово 0 — S->vis, остальные — таблица vis с S->visX)
static uint32_t stWord(const StateProps *sp, const SpType *T, const SpState *S, unsigned w) {
  return w == 0 ? S->vis : w < VW(T) ? sp->vis[S->visX + w - 1] : 0u;
}
static int stSlot(const StateProps *sp, const SpType *T, const SpState *S, unsigned slot) {
  return (stWord(sp, T, S, slot >> 5) >> (slot & 31)) & 1u;
}
static int stAny(const StateProps *sp, const SpType *T, const SpState *S) {
  for (unsigned w = 0; w < VW(T); w++) if (stWord(sp, T, S, w)) return 1;
  return 0;
}
// слот под PVIS (маска PVIS типа) и его видимость на кадре f
static int pvSlot(const StateProps *sp, const SpType *T, unsigned slot) {
  unsigned w = slot >> 5; uint32_t m = w == 0 ? T->pvisMask : w < VW(T) ? sp->vis[T->pvisX + w - 1] : 0u;
  return (m >> (slot & 31)) & 1u;
}
int spSlotOn(const StateProps *sp, unsigned i, unsigned slot) {
  if (i >= sp->ninst) return 0;
  const SpType *T = TY(sp, i);
  return slot < 32 * VW(T) && stSlot(sp, T, ST(sp, i), slot);
}
unsigned spSlotCount(const StateProps *sp, unsigned type) { return type < sp->ntype ? 32 * VW(&sp->type[type]) : 0; }

static void enter(StateProps *sp, unsigned i, int to) {
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  if (to < 0 || to >= T->nState) return;
  r->state = (uint8_t)to; r->t = 0; r->callDone = 0; r->poseValid = 0; r->burst = 1; r->fxStarted = 0;
  r->slN = 0;   // №1000: смена состояния обнуляет время — накопленный сон не нужен
  r->fxAcc[0] = r->fxAcc[1] = r->fxAcc[2] = r->fxAcc[3] = 0;
  r->poseF = sp->state[T->firstState + to].poseMin;
}

// правка 334: разовые стейтпропы (stateprops.h spOnce) — с монетами в машине состояний, не обломок, не миссионный
int spOnceOld = SP_ONCE_OLD;
int spOnce(const StateProps *sp, unsigned i) {
  if (i >= sp->ninst || sp->inst[i].tag) return 0;
  const SpType *T = TY(sp, i);
  if (T->kind) return 0;
  for (unsigned s = 0; s < T->nState; s++) {
    const SpState *S = &sp->state[T->firstState + s];
    for (unsigned k = 0; k < S->nCall; k++) { int op = sp->call[S->firstCall + k].op; if (op == SP_OP_COINS || op == SP_OP_COINS_Z) return 1; }
  }
  return 0;
}
void spRestore(StateProps *sp, unsigned i, int state) {
  if (i >= sp->ninst) return;
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  if (state < 0 || state >= T->nState) return;
  r->alive = 1; r->colOn = r->col2On = 1; r->cool = r->away = 0.f;
  for (int guard = 0; guard < 8; guard++) {   // цепочка авто-переходов — до состояния, где игра стоит
    enter(sp, i, state);
    const SpState *S = ST(sp, i);
    for (unsigned k = 0; k < S->nCall; k++) {   // действие вызовов без монет и счётчиков: только «убрать»
      const SpCall *C = &sp->call[S->firstCall + k];
      if (C->op == SP_OP_REMOVE) { r->alive = 0; r->colOn = r->col2On = 0; }
      else if (C->op == SP_OP_COLOFF) r->colOn = r->col2On = 0;
      else if (C->op == SP_OP_COL2OFF) r->col2On = 0;
    }
    r->callDone = 0xFFFFu;
    if (S->autoTo == 0xFF || S->autoTo == state) break;
    state = S->autoTo;
  }
  const SpState *S = ST(sp, i);
  r->t = 1000.f + S->autoTime;   // окна частиц и разовые позы — давно отыграли
  r->burst = 0; r->fxStarted = 0xFF;
  if (S->poseMode == 1) r->poseF = S->poseMax;
  r->poseValid = 0;
}

void spRespawnStep(StateProps *sp, float ex, float ez, float dt,
                   int (*seen)(void *, float, float, float, float), void *user) {
  if (dt <= 0.f || !seen) return;
  for (unsigned i = 0; i < sp->ninst; i++) {
    SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i); const SpInst *I = &sp->inst[i];
    const SpState *S = &sp->state[T->firstState];
    // сверка g11, P11-17 и P11-Д6 (Н): тип 10 (фонари, светофоры, столбы) после удара остаётся в физике и, как только выходит из пирамиды видимости игрока, удаляется из мира до
    // перезагрузки зоны (без таймера); P11-07 и P12-02/04 (С): разбитый ломаемый/двигаемый (типы 3 и 4) лежит разбитым до перезагрузки зоны, возврата «5 с вне вида дальше 60 м» нет
    // (бит в сохранение — savegame.c окна g10a, см. журнал). SPLIE_SV_OLD=1 — возврат через 5 с, как в TEST81
    if (!SPLIE_SV_OLD && !I->tag && !T->kind && !r->noRespawn) {
      if (T->cls == 10 && r->alive && (r->state != 0 || r->fall)) {
        float c0[3], rad0; spBounds(sp, i, c0, &rad0);
        if (!seen(user, c0[0], c0[1], c0[2], rad0)) { r->alive = 0; r->colOn = r->col2On = 0; r->noRespawn = 1; }
        r->away = 0.f; continue;
      }
      if ((T->cls == 3 || T->cls == 4) && r->state != 0 && (!r->alive || !r->colOn)) { r->away = 0.f; continue; }
    }
    // Миссионные теги, эффекты-обломки, самовосстанавливающиеся кусты и активные
    // начальные состояния не относятся к обычным разрушенным объектам улицы.
    if (r->noRespawn || I->tag || T->kind || T->cls == 2 || S->nCall ||
        (r->state == 0 && r->alive && r->colOn && r->col2On) || (!spOnceOld && spOnce(sp, i))) { r->away = 0.f; continue; }   // правка 334: разовые не возвращаются
    float c[3], radius; spBounds(sp, i, c, &radius);
    float dx = I->x - ex, dz = I->z - ez, R[9], home[3]; quatR(I->q, R);
    home[0] = I->x + R[0]*T->c[0] + R[3]*T->c[1] + R[6]*T->c[2];
    home[1] = I->y + R[1]*T->c[0] + R[4]*T->c[1] + R[7]*T->c[2];
    home[2] = I->z + R[2]*T->c[0] + R[5]*T->c[1] + R[8]*T->c[2];
    if (dx * dx + dz * dz < 60.f * 60.f || seen(user, c[0], c[1], c[2], radius) ||
        seen(user, home[0], home[1], home[2], radius)) { r->away = 0.f; continue; }
    if ((r->away += fminf(dt, .05f)) < 5.f) continue;
    r->x = I->x; r->y = I->y; r->z = I->z; quatR(I->q, r->R); r->fall = 0;
    r->alive = r->colOn = r->col2On = 1; r->cool = r->away = 0.f;
    enter(sp, i, 0);
  }
}

void spZoneReload(StateProps *sp) {
  for (unsigned i = 0; i < sp->ninst; i++) {
    SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i); const SpInst *I = &sp->inst[i];
    const SpState *S = &sp->state[T->firstState];
    int removed10 = T->cls == 10 && !r->alive && !I->tag;   // удалённый после удара высокий предмет (noRespawn ставило выпадение из вида)
    if (!removed10 && (r->noRespawn || I->tag || T->kind || T->cls == 2 || S->nCall ||
        (r->state == 0 && r->alive && r->colOn && r->col2On) || (!spOnceOld && spOnce(sp, i)))) continue;
    r->x = I->x; r->y = I->y; r->z = I->z; quatR(I->q, r->R); r->fall = 0;
    r->alive = r->colOn = r->col2On = 1; r->cool = r->away = 0.f; r->noRespawn = 0;
    enter(sp, i, 0);
  }
}

int spEvent(StateProps *sp, unsigned i, int ev) {
  if (i >= sp->ninst || !sp->run[i].alive) return 0;
  const SpState *S = ST(sp, i);
  for (unsigned k = 0; k < S->nEvent; k++) {
    const SpEvent *E = &sp->event[S->firstEvent + k];
    if (E->event == ev) {
      int st0 = sp->run[i].state;
      enter(sp, i, E->to);
      if (ev == SP_EV_HIT || ev == SP_EV_STOMP || ev == SP_EV_VEHICLE_HIT || ev == SP_EV_DESTROYED) {
        sp->hnrHits++;   // правка 112: удар игрока
        if (sp->hitInstN < 16) sp->hitInst[sp->hitInstN++] = (uint16_t)i;   // сверка g11 круг 2 (P03-11)
        // правка 135.1: звук объекта лежит в CollisionEffect (0x3000600) его сущности — звук столкновения, не анимации
        // (данные диска, art/l*.p3d); авто-переход SPR1 ударом не является. ПРАВИЛО ПОРТА по данным
        // (удар, не сменивший состояние, звука не давал и раньше)
        if (sp->run[i].state != st0 && sp->sndN < sizeof sp->sndQ / sizeof sp->sndQ[0]) sp->sndQ[sp->sndN++] = (uint16_t)i;
      }
      return 1; }
  }
  return 0;
}

int spSndPop(StateProps *sp, unsigned *i) {
  if (!sp->sndN) return 0;
  *i = sp->sndQ[0]; sp->sndN--;
  memmove(sp->sndQ, sp->sndQ + 1, sp->sndN * sizeof sp->sndQ[0]);
  return 1;
}

void spEnter(StateProps *sp, unsigned i, int state) {
  if (i >= sp->ninst) return;
  sp->run[i].alive = 1; sp->run[i].colOn = sp->run[i].col2On = 1; sp->run[i].cool = 0.f;
  enter(sp, i, state);
}

int spActivateTag(StateProps *sp, int tag) {
  int n = 0;
  for (unsigned i = 0; i < sp->ninst; i++)
    if (sp->inst[i].tag && (!tag || sp->inst[i].tag == tag) && sp->run[i].alive && sp->run[i].state == 0 && TY(sp, i)->nState > 1) {
      enter(sp, i, 1); sp->run[i].colOn = sp->run[i].col2On = 1; n++;
    }
  return n;
}

int spBreakOf(const StateProps *sp, unsigned prpType) {
  if (prpType >= sp->ndyn || sp->dynBreak[prpType] == 0xFF || sp->dynBreak[prpType] >= sp->ntype) return -1;
  return sp->dynBreak[prpType];
}

int spBreak(StateProps *sp, unsigned spType, const float pos[3], const float q[4]) { return spBreakDir(sp, spType, pos, q, NULL); }

// №923: средний горизонтальный ход кусков обломка за весь клип (мир): суставы в последнем кадре минус кадр 0; jm0 — суставы кадра 0 (выделяет spBreakDir)
#ifndef BREAK_DIR923_OLD
#define BREAK_DIR923_OLD 0
#endif
static int breakDirSetup(StateProps *sp, unsigned i, const float v[2]) {
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  free(r->jm0); r->jm0 = NULL; r->dirC = 1.f; r->dirS = 0.f;
  float vl = v ? sqrtf(v[0] * v[0] + v[1] * v[1]) : 0.f;
  unsigned nfr = T->frames & 0xFFFF;
  if (BREAK_DIR923_OLD || vl < .5f || !T->nPiece || nfr < 2 || !T->nJoint) return 0;
  float *j0 = (float *)malloc((size_t)T->nJoint * 12 * sizeof(float)); if (!j0) return 0;
  float keep = r->poseF;
  float S0[3] = {0, 0, 0}, S1[3] = {0, 0, 0}; unsigned n = 0;
  for (int pass = 0; pass < 2; pass++) {   // положения кусков (начало в мире, как в spPieceMatrix) в покое и в последнем кадре клипа; сумма по кускам
    r->poseF = pass ? (float)(nfr - 1) : 0.f; r->poseValid = 0; spPose(sp, i);
    if (!pass) memcpy(j0, r->jm, (size_t)T->nJoint * 12 * sizeof(float));
    for (unsigned k = 0; k < T->nPiece; k++) {
      const SpPiece *P = &sp->piece[T->firstPiece + k]; unsigned jn = (P->flags >> 8) & 0xFF; if (jn >= T->nJoint) continue;
      const float *J = r->jm + jn * 12; float o[3] = {P->ox, P->oy, P->oz}, jo[3], w[3];
      mulRv(J, o, jo); jo[0] += J[9]; jo[1] += J[10]; jo[2] += J[11]; mulRv(r->R, jo, w);
      float *S = pass ? S1 : S0; S[0] += w[0]; S[1] += w[1]; S[2] += w[2]; if (pass) n++;
    }
  }
  r->poseF = keep; r->poseValid = 0; spPose(sp, i);
  float dx = n ? (S1[0] - S0[0]) / (float)n : 0.f, dz = n ? (S1[2] - S0[2]) / (float)n : 0.f, dl = sqrtf(dx * dx + dz * dz);
  if (dl < .3f) { free(j0); return 0; }   // куски не уходят вбок (падают на месте / вверх): повернуть нечего
  float th = atan2f(v[0], v[1]) - atan2f(dx, dz);
  r->dirC = cosf(th); r->dirS = sinf(th); r->jm0 = j0;
  return 1;
}

int spBreakDir(StateProps *sp, unsigned spType, const float pos[3], const float q[4], const float v[2]) {
  if (spType >= sp->ntype) return 0;
  int best = -1; float bt = -1.f;
  for (unsigned i = 0; i < sp->ninst; i++) {
    if (sp->inst[i].type != spType) continue;
    SpRun *r = &sp->run[i];
    float age = r->state == 0 ? 1e9f : r->t;   // свободный слот — сразу, иначе самый старый
    if (age > bt) { bt = age; best = (int)i; }
  }
  if (best < 0) return 0;
  SpRun *r = &sp->run[best];
  r->x = pos[0]; r->y = pos[1]; r->z = pos[2]; quatR(q, r->R); r->alive = 1;
  enter(sp, best, 1);
  r->ownFx = (uint8_t)breakOwnFx_;
  breakDirSetup(sp, (unsigned)best, v);
  return sp->type[spType].nPiece > 0;
}

// центр коробки кусков в мире
static void centerW(const StateProps *sp, unsigned i, float c[3]) {
  const SpRun *r = &sp->run[i]; mulRv(r->R, TY(sp, i)->c, c); c[0] += r->x; c[1] += r->y; c[2] += r->z;
}

static void fxPush(StateProps *sp, int preset, const float p[3], float dx, float dz, int first) {
  if (sp->nfx >= SP_FXQ) return;
  SpFxOut *o = &sp->fxq[sp->nfx++];
  o->preset = (uint8_t)preset; o->first = (uint8_t)first; o->sys = -1; o->x = p[0]; o->y = p[1]; o->z = p[2]; o->dx = dx; o->dz = dz;
}

unsigned spFxNameCount(const StateProps *sp) { return sp->nfxName; }
const char *spFxName(const StateProps *sp, unsigned k) { return k < sp->nfxName ? (const char *)sp->fxName + k * 32 : ""; }
void spFxBind(StateProps *sp, unsigned k, int sys) {
  if (k >= sp->nfxName || !sp->fxSys) return;
  const uint8_t *e = sp->fxName + k * 32;
  unsigned t = (unsigned)e[28] | ((unsigned)e[29] << 8), slot = e[30];
  if (t >= sp->ntype || slot >= 32 * VW(&sp->type[t])) return;
  sp->fxSys[sp->slotBase[t] + slot] = (int16_t)(sys < 0 ? -1 : sys); sp->fxJoint[sp->slotBase[t] + slot] = e[31];
}

int spFxPop(StateProps *sp, SpFxOut *o) {
  if (!sp->nfx) return 0;
  *o = sp->fxq[--sp->nfx];
  return 1;
}

static void doCall(StateProps *sp, unsigned i, const SpCall *C, const ColWorld *world, Coins *coins) {
  SpRun *r = &sp->run[i];
  switch (C->op) {
    case SP_OP_REMOVE: r->alive = 0; r->colOn = r->col2On = 0; break;
    case SP_OP_COLOFF: r->colOn = r->col2On = 0; break;
    case SP_OP_COL2OFF: r->col2On = 0; break;
    case SP_OP_DESTROYED: sp->destroyed++; sp->destroyedTags |= 1u << (sp->inst[i].tag & 31); break;
    case SP_OP_COLA: sp->cola++; break;
    case SP_OP_KILLSPEED: sp->killSpeed = 1; break;
    case SP_OP_COINS: case SP_OP_COINS_Z: {
      // сверка g11, P18-Д1 (С), P11-06 (С): колбэки оригинал выбирает по числовому коду, имя в данных — подпись: код 11 («монеты вперёд») — пять монет за вызов, даже если имя
      // SPAWN_25_COINS_Z (автоматы l2_vending: 3 вызова x 5 = 15, а не 75); вызовы 1, 5, 10, 15, 20 монет при выключенном Hit & Run (sp->hnrOff) монет не дают, код 11 даёт всегда.
      // Ящик (звук car_hit_crate: +1 монета к каждому вызову при выключенном H&R) — признака в данных SPR1 нет, см. журнал. SPCOIN_SV_OLD=1 — как было
      int n = C->arg;
      if (!SPCOIN_SV_OLD) {
        if (C->op == SP_OP_COINS_Z) n = 5;
        else if (sp->hnrOff) n = (sp->crateLike && sp->crateLike(sp->radUser, i)) ? C->arg + 1 : 0;   // круг 3 (g11): ящик при выключенном Hit & Run даёт на монету больше записанного
      }
      float c[3]; centerW(sp, i, c);
      float fl = r->y, g;
      if (world && colGround(world, c[0], c[2], c[1] + 1.f, r->y - 4.f, &g, 0)) fl = g;
      c[1] += 0.3f;
      sp->coins += n;
      // сверка g11 круг 2 (P18 Д5): при выключенном Hit & Run вызов монет (кроме кода 11) вместо монет подаёт предмету ещё одно событие HIT (состояние идёт по цепочке дальше); ящикам +1 монета —
      // признака ящика в SPR1 нет (см. журнал круга 1). SPHNRHIT_SV_OLD=1 — только без монет
      if (!SPHNRHIT_SV_OLD && !SPCOIN_SV_OLD && sp->hnrOff && C->op == SP_OP_COINS && !(sp->crateLike && sp->crateLike(sp->radUser, i))) r->hitPend = 1;
      if (!coins || n <= 0) break;
      if (C->op == SP_OP_COINS) coinsDrop(coins, n, c[0], c[1], c[2], fl);
      else {   // вдоль локальной +Z пропа (лицо автомата: стекло l1_vendingShape3 на +Z в осях объекта)
        float dx = r->R[6], dz = r->R[8], l = sqrtf(dx * dx + dz * dz);
        if (l < 1e-3f) { dx = 0; dz = 1; l = 1; }
        coinsDropDir(coins, n, c[0], c[1], c[2], fl, dx / l, dz / l, SP_ZCONE);
      }
    } break;
    case SP_OP_BREAK: {   // барьер 0x3F0000F (tools/prp2_export.py _add_animphys): обломок строки ATC — тип kind 1 с этим номером
      float p[3] = {r->x, r->y, r->z};
      int broken = 0;
      for (unsigned t = 0; t < sp->ntype; t++)
        if (sp->type[t].kind == 1 && sp->type[t].bindex == C->arg) {
          breakOwnFx_ = !BARRIERFX1175_OLD;   // №1175: частицы обломка играет он сам
          broken = spBreakDir(sp, t, p, sp->inst[i].q, r->hv);
          breakOwnFx_ = 0;   // №923: осколки по ходу машины (hv — ноль, если машина предмет не била)
          sp->breaks++;
          break;
        }
#ifndef FENCE_BREAK_OLD
      // №588 (метка #19): у деревянных заборчиков (arg=16 eBigBarrierBreaking) при разрушении
      // спавним частицы разлетающихся деревянных щепок (анимация разваливания)
      if (!broken && C->arg == 16 && BARRIERFX1175_OLD) {   // №1175: теперь частицы даёт сам обломок (родная система или пресет его состояния); 6 щепок — только как в TEST87
        for (int k = 0; k < 6; k++) {
          float pt[3] = {p[0] + (sfr() - .5f) * 1.5f, p[1] + (sfr() - .2f) * 0.8f, p[2] + (sfr() - .5f) * 1.5f};
          fxPush(sp, SPFX_WOOD, pt, r->R[6], r->R[8], k == 0);
        }
      }
#endif
    } break;
    case SP_OP_RADIATE:
      // сверка g11, P16-21 и P18-Д6 (Н): вызов 8 стоит в состоянии 1 статуй Иедедайи (l2z1, l5z1) и срабатывает при входе в него (после удара): всем подвижным предметам, кроме игрока, в шаре 1,5 м от
      // фиксированной точки мира (135,16; 13,20; 37,91 в осях оригинала, у нас z зеркален) — скорость вверх 200 / масса строки ATC (оторвать голову статуи, скамейки остаются). Точка в данных
      // вызова не лежит (оригинал держит её в коде), числа взяты из отчёта. Величина — по записи PS2 не проверена (масса 1000 даёт 0,2 м/с, единицы могут быть другие). SPRAD_SV_OLD=1 — ничего
      // Круг 3 (g11): точка толчка — относительно самой статуи (смещение от её начала, вверх к голове), а не абсолютные координаты мира; полностью в данные (рядом с вызовом 8) её переносит экспорт — окно g12
      if (!SPRAD_SV_OLD && sp->radiate) sp->radiate(sp->radUser, r->x + SP_RAD_DX, r->y + SP_RAD_DY, r->z + SP_RAD_DZ, 1.5f, 200.f);
      break;
    default: break;   // FIRE_ENERGY_BOLT — на уровне 1 не встречаются
  }
}

// вызовы текущего состояния, время которых наступило
static void runCalls(StateProps *sp, unsigned i, const ColWorld *world, Coins *coins) {
  for (int guard = 0; guard < 4; guard++) {
    SpRun *r = &sp->run[i]; const SpState *S = ST(sp, i); int st = r->state;
    for (unsigned k = 0; k < S->nCall; k++) {
      if (r->callDone & (1u << k)) continue;
      const SpCall *C = &sp->call[S->firstCall + k];
      if (C->time > r->t) continue;
      r->callDone |= (uint16_t)(1u << k);
      doCall(sp, i, C, world, coins);
      if (!r->alive) return;
      if (r->hitPend) { r->hitPend = 0; spEvent(sp, i, SP_EV_HIT); st = -1; break; }   // сверка g11 круг 2 (P18 Д5): состояние сменилось, продолжаем с нового
    }
    if (st < 0) continue;   // сверка g11 круг 2: состояние сменилось событием HIT — разбор с начала
    // авто-переход
    if (S->autoTo != 0xFF && r->t >= S->autoTime) { enter(sp, i, S->autoTo); if (r->state != st) continue; }
    return;
  }
}

static float ctrlFrame(float t, float fps, uint8_t mode, float mn, float mx, float speed, unsigned cycles, int *active) {
  float f = t * fps * speed, len = mx - mn;
  *active = 1;
  if (mode == 0 || len <= 0.f) { *active = mode != 0 && f <= 0.f; return mn; }
  if (mode == 1) { if (f >= len) { *active = 0; return mx; } return mn + f; }
  if (cycles && f >= len * cycles) { *active = 0; return mx; }
  return mn + fmodf(f, len);
}

// №975 (Роб 25.10, L5 F3-2: «красные фонари не сбиваются»): фонарь стейтпроп (cls 10: streetlamp, streetlampbanner, streetlampsquare, parkinglight…) по данным диска при ударе только гасит
// свет (состояние 1: без луча и пятна, кадры клипа 0..0): падение у оригинала — физика тела AnimDynaPhys, в порте у стейтпропов её не было, столб стоял с погасшим светом (PRP1-фонари L1
// падали по №968). Теперь высокий предмет cls 10 после удара валится на шарнире у основания по тому же закону, что №968 (замер PS2: θ'' = 0,5·sin θ, ω0 = 0,2 + 0,07·max(0, v − 12) рад/с).
// Только если у состояния после удара клип не двигается (иначе падение рисует сам клип). SP_POLEFALL975_OLD=1 — как было
#ifndef SP_POLEFALL975_OLD
#define SP_POLEFALL975_OLD 0
#endif
int spPoleFallOld = SP_POLEFALL975_OLD;   // flag polefall975old 1 — как было
// №975б (список сомнений: L4 тип 1, L4 тип 9, L6 тип 5, L7 тип 7…): у высокого фонаря cls 10 вертикальный цилиндр ствола (объём «второго» набора) начинается на 1,8–2 м над основанием
// (L4 тип 1: 2,05…6,57 м), основания в стене мира (COL1) тут нет — машина проезжает сквозь основание как призрак (в переписи 13 из 16 заездов «не дошли», в игре v не падает). На PS2 у PRP1-фонаря
// (L1) тело стоит от земли (замер №968), так что ствол тянем вниз до земли. SP_POLEBASE975_OLD=1 / flag polebase975old 1 — как было
#ifndef SP_POLEBASE975_OLD
#define SP_POLEBASE975_OLD 0
#endif
int spPoleBaseOld = SP_POLEBASE975_OLD;
#ifndef POLE998_OLD
#define POLE998_OLD 0
#endif
int spPole998Old = POLE998_OLD;   // flag pole998 0 — как было
#ifndef SP_POLEWALL975_OLD
#define SP_POLEWALL975_OLD 0
#endif
int spPoleWallOld = SP_POLEWALL975_OLD;   // flag polewall975old 1 — как было
#define SPF_A 0.5f
#define SPF_W0 0.2f
#define SPF_WK 0.07f
#define SPF_V0 12.f
float propsPoleKick1046(float v, float w0, float wk, float v0);   // №1046 (props.c)
static void poleFallStart(StateProps *sp, unsigned i, const Car *car, const float bn[2]) {
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  if (spPoleFallOld || r->fall || T->cls != 10 || T->h[1] < 2.f || !r->alive) return;
  const SpState *S = ST(sp, i); if (S->poseMode != 0 && S->poseMax > S->poseMin) return;
  float vx = car->velPre[0], vz = car->velPre[2], v = sqrtf(vx * vx + vz * vz);
  if (v < 0.5f) { vx = -bn[0]; vz = -bn[1]; v = sqrtf(vx * vx + vz * vz); if (v < 1e-3f) return; v = 2.f; }
  float dx = vx / sqrtf(vx * vx + vz * vz), dz = vz / sqrtf(vx * vx + vz * vz);
  r->fall = 1; r->fallTh = 0.0174533f; r->fallW = propsPoleKick1046(v, SPF_W0, SPF_WK, SPF_V0); r->fallD[0] = dx; r->fallD[1] = dz; memcpy(r->fallR0, r->R, sizeof r->fallR0);
}
static void poleFallStep(SpRun *r, float dt) {
  float th = r->fallTh, w = r->fallW, h = dt / 4.f; int done = 0;
  for (int k = 0; k < 4 && !done; k++) { w += SPF_A * sinf(th) * h; th += w * h; if (th >= 1.5707963f) { th = 1.5707963f; done = 1; } }
  r->fallTh = th; r->fallW = done ? 0.f : w; if (done) r->fall = 2;
  float ax = r->fallD[1], az = -r->fallD[0], c = cosf(th), s = sinf(th), t = 1.f - c;   // ось = вверх × направление = (dz, 0, −dx); Родриг, R по столбцам
  float Rr[9] = { c + ax * ax * t, az * s, ax * az * t,        // столбец 0
                  -az * s,           c,      ax * s,            // столбец 1
                  ax * az * t,      -ax * s, c + az * az * t };  // столбец 2
  mulRR(Rr, r->fallR0, r->R);
}
// сверка g11, P11_объекты_мира Д1 (С) и P11-05: не ломаемые стейтпропы типов 3 и 10 (подвижные, падающие столбы и фонари) от пинка героя (сила 400, 45° вверх от курса), стомпа (800, радиус 2 м)
// и взрыва машины (20000, радиус 20 м) получают, кроме событий, скорость сила / масса (масса из ATC, 100 если нуль) и входят в физику. У ломаемых (4) и статики (2) скорости нет. В движке у
// стейтпропов нет тела с физикой: тип 10 (высокий) валится на шарнире у основания (poleFallStep, замер PS2 №968) с начальной скоростью от этого толчка; тип 3 без клипа остаётся на месте
// (нужно тело — см. журнал). (dx, dz) — куда толкают. Возврат 1 — столб пошёл вниз. SPBOOM_SV_OLD=1 — функция ничего не делает
int spImpulse(StateProps *sp, unsigned i, float dx, float dz, float force) {
  if (SPBOOM_SV_OLD || i >= sp->ninst) return 0;
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  if (r->fall || T->cls != 10 || T->kind || T->h[1] < 2.f || !r->alive || !r->colOn) return 0;
  const SpState *S = ST(sp, i); if (S->poseMode != 0 && S->poseMax > S->poseMin) return 0;   // падение рисует сам клип
  float l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) return 0;
  float m = T->mass > 0.f ? T->mass : 100.f, v = force / m;
  r->fall = 1; r->fallTh = 0.0174533f; r->fallW = propsPoleKick1046(v, SPF_W0, SPF_WK, SPF_V0); r->fallD[0] = dx / l; r->fallD[1] = dz / l; memcpy(r->fallR0, r->R, sizeof r->fallR0);
  return 1;
}
int spFollowAdopt(StateProps *sp, const unsigned char *rec, unsigned n) {
  free(sp->follow); free(sp->followOk); free(sp->followT0); sp->follow = sp->followOk = NULL; sp->followT0 = NULL; sp->nfollow = 0;
  if (!rec || !n || !sp->type) return -1;
  unsigned m = n < sp->ntype ? n : sp->ntype;   // экспорт без рисунка собирает то же начало списка типов; типы в конце (ключи, бочки, подборы) в файле отсутствуют
  for (unsigned t = 0; t < m; t++) {
    const unsigned char *r = rec + 16 * t; unsigned nj = r[1], nc = r[2] | (r[3] << 8), nfr = r[4] | (r[5] << 8);
    if (nj != (unsigned)sp->type[t].nJoint || nc != (unsigned)sp->type[t].nCol || nfr != (sp->type[t].frames & 0xFFFFu)) return -1;
  }
  sp->follow = (uint8_t *)calloc(m, 1); sp->followOk = (uint8_t *)calloc(m, 1); sp->followT0 = (float *)calloc(m * 3, sizeof(float));
  if (!sp->follow || !sp->followOk || !sp->followT0) { free(sp->follow); free(sp->followOk); free(sp->followT0); sp->follow = sp->followOk = NULL; sp->followT0 = NULL; return -1; }
  int k = 0; sp->nfollow = m;
  for (unsigned t = 0; t < m; t++) { sp->follow[t] = rec[16 * t]; k += sp->follow[t] != 0; }
  return k;
}
void spLoop(StateProps *sp, unsigned i, float a, float b) {
  if (i >= sp->ninst) return;
  SpRun *r = &sp->run[i];
  if (b > a) { r->loopOn = 1; r->loopA = a; r->loopB = b; } else r->loopOn = 0;
  r->poseValid = 0;
}
static void stepInst(StateProps *sp, unsigned i, float dt, const ColWorld *world, Coins *coins) {
  SpRun *r = &sp->run[i];
  if (!r->alive) return;
  r->t += dt;
  if (r->fall == 1) poleFallStep(r, dt);
  if (r->cool > 0.f) r->cool -= dt;
#ifndef SP_SCAN_OLD
#define SP_SCAN_OLD 0
#endif
  if (!SP_SCAN_OLD) { const SpState *S0 = ST(sp, i);   // состояние без вызовов, автоперехода, позы и частиц: ниже делать нечего, кроме сброса burst
    if (!S0->nCall && S0->autoTo == 0xFF && !S0->poseMode && !S0->nFx && !r->loopOn) { r->burst = 0; return; } }
  runCalls(sp, i, world, coins);
  if (!r->alive) return;
  const SpType *T = TY(sp, i); const SpState *S = ST(sp, i);
  float fps = T->fps ? (float)T->fps : 30.f;
  if (r->loopOn) {   // сверка g11 круг 2 (P15-19): цикл вступления или окончания гэга
    float f = r->loopA + fmodf(r->t * fps, r->loopB - r->loopA);
    if (f != r->poseF) { r->poseF = f; r->poseValid = 0; }
  } else
  if (S->poseMode) {
    int act; float f = ctrlFrame(r->t, fps, S->poseMode, S->poseMin, S->poseMax, S->poseSpeed, S->poseCycles, &act);
    if (f != r->poseF) { r->poseF = f; r->poseValid = 0; }
  }
  // частицы: окно кадров контроллера EFX = время работы эмиттера
  int first = r->burst; r->burst = 0;
  if (T->kind == 1 && sp->nativeBreakFx && !r->ownFx) return;   // обломок: родные частицы уже запустил вызывающий (fxBurstBreakable)
  for (unsigned k = 0; k < S->nFx; k++) {
    const SpFx *F = &S->fx[k];
    if (!stSlot(sp, T, S, F->slot)) continue;
    int act; ctrlFrame(r->t, fps, F->mode, F->min, F->max, F->speed, F->cycles, &act);
    if (!act) continue;
    int sys = F->slot < 32 * VW(T) ? sp->fxSys[sp->slotBase[sp->inst[i].type] + F->slot] : -1;
    if (sys >= 0) {   // родная система: один раз при открытии окна EFX, в точке сустава эффекта
      if (r->fxStarted & (1u << k)) continue;
      r->fxStarted |= (uint8_t)(1u << k);
      if (sp->nfx >= SP_FXQ) continue;
      unsigned j = sp->fxJoint[sp->slotBase[sp->inst[i].type] + F->slot];
      float lp[3] = {0, 0, 0}, p[3];
      if (j < T->nJoint) { spPose(sp, i); lp[0] = r->jm[j * 12 + 9]; lp[1] = r->jm[j * 12 + 10]; lp[2] = r->jm[j * 12 + 11]; }
      mulRv(r->R, lp, p); p[0] += r->x; p[1] += r->y; p[2] += r->z;
      SpFxOut *o = &sp->fxq[sp->nfx++];
      o->preset = F->preset; o->first = 1; o->sys = (int16_t)sys; o->x = p[0]; o->y = p[1]; o->z = p[2]; o->dx = r->R[6]; o->dz = r->R[8]; o->t0 = (float)F->min;
      continue;
    }
    int n = first ? SP_FX_BURST : 0;
    r->fxAcc[k] += dt * SP_FX_RATE;
    if (!(r->fxAcc[k] < 64.f)) r->fxAcc[k] = 0.f;   // правка 252: NaN/бесконечность не крутят цикл вечно (обычно < 1)
    while (r->fxAcc[k] >= 1.f) { r->fxAcc[k] -= 1.f; n++; }
    if (!n) continue;
    float c[3]; centerW(sp, i, c);
    for (int m = 0; m < n; m++) {
      float p[3] = {c[0] + (sfr() - .5f) * T->h[0], c[1] + (sfr() - .2f) * T->h[1], c[2] + (sfr() - .5f) * T->h[2]};
      fxPush(sp, F->preset, p, r->R[6], r->R[8], first && m == 0);
    }
  }
}

// ---- машина против объёмов пропа (срез слоем кузова, проекция на xz, SAT: прямоугольник кузова против
// прямоугольника коробки / круга сферы и вертикального цилиндра). Нормаль — от пропа к машине.
// правка 297: ось последнего входа. ov — перекрытие по оси, a — сближение по ней (> 0 — кузов едет в проп): по оси, вошедшей в
// перекрытие позже всех (наименьшее ov / a), кузов и ударил; его сближение — скорость удара
static void lastEntry(float ov, float a, float *bestT, float *impact) {
  if (a <= 1e-3f) return;
  float t = ov / a; if (t < *bestT) { *bestT = t; *impact = a; }
}
static float spColOff[3];   // круг 5 (g11): сдвиг объёма за суставом, в осях объекта; ноль — объём стоит в позе кадра 0
static float spColOffW[3];  // круг 7 (g11): сдвиг объёма за суставом в осях МИРА, без поворота экземпляра (положение сустава целиком; см. FOLLOWABS1107_OLD)
static int spVRelax;   // №975: 1 — объём считается от земли (без проверки «над кузовом»): столб с базой в стене мира (см. carContact)
static int spBaseExt;   // №975: 1 — вертикальный цилиндр (ствол) высокого предмета cls 10 тянется вниз до земли (см. carContact)
static int hitVolume(const SpRun *r, const SpCol *C, const CarBox *cb, const float vel[3], float *nx, float *nz, float *depth, float *impact) {
  float c[3], a[3][3];
  { float cc[3] = {C->c[0] + spColOff[0], C->c[1] + spColOff[1], C->c[2] + spColOff[2]}; mulRv(r->R, cc, c); }
  c[0] += r->x + spColOffW[0]; c[1] += r->y + spColOffW[1]; c[2] += r->z + spColOffW[2];
  for (int k = 0; k < 3; k++) mulRv(r->R, C->a + 3 * k, a[k]);
  float ey = 0.f;
  int circle = C->kind == 1 || (C->kind == 2 && fabsf(a[1][1]) > 0.9f);
  if (C->kind == 1) ey = C->h[0];
  else for (int k = 0; k < 3; k++) ey += C->h[k] * fabsf(a[k][1]);
  if (!spVRelax && !(spBaseExt && C->kind == 2 && fabsf(a[1][1]) > 0.9f) && c[1] - ey > cb->y + cb->hi) return 0;
  if (c[1] + ey < cb->y + cb->lo) return 0;
  // вертикаль (правка 297): перекрытие слоя кузова и пропа по высоте, сближение — скорость кузова к пропу по y
  float bestT = 3e38f, imp = 0.f;
  { float ylo = cb->y + cb->lo, yhi = cb->y + cb->hi, ov = fminf(yhi, c[1] + ey) - fmaxf(ylo, c[1] - ey);
    float above = (ylo + yhi) * .5f > c[1] ? 1.f : -1.f;   // кузов выше центра пропа — входил сверху (сближение — падение)
    lastEntry(ov, -vel[1] * above, &bestT, &imp); }
  float fx = cb->fx, fz = cb->fz, rx = -fz, rz = fx;
  float dx = cb->x - c[0], dz = cb->z - c[2];
  if (circle) {
    float rad = C->h[0];
    float lf = dx * fx + dz * fz, lr = dx * rx + dz * rz;   // центр кузова относительно пропа, в осях кузова
    // ближайшая точка кузова к центру пропа
    float qf = fmaxf(-cb->hl, fminf(cb->hl, -lf)), qr = fmaxf(-cb->hw, fminf(cb->hw, -lr));
    float ex = -lf - qf, ez = -lr - qr, d2 = ex * ex + ez * ez;
    if (d2 >= rad * rad) return 0;
    if (d2 > 1e-8f) {
      float d = sqrtf(d2), ux = -ex / d, uz = -ez / d;   // от пропа к кузову в осях кузова
      *nx = ux * fx + uz * rx; *nz = ux * fz + uz * rz; *depth = rad - d;
    } else {   // центр пропа внутри кузова — выталкиваем по ближней грани
      float pf = cb->hl - fabsf(lf), pr = cb->hw - fabsf(lr);
      if (pf < pr) { float s = lf > 0 ? 1.f : -1.f; *nx = fx * s; *nz = fz * s; *depth = pf + rad; }
      else { float s = lr > 0 ? 1.f : -1.f; *nx = rx * s; *nz = rz * s; *depth = pr + rad; }
    }
    lastEntry(*depth, -(vel[0] * *nx + vel[2] * *nz), &bestT, &imp);   // круг: по радиусу (как было) или сверху
    *impact = imp;
    return 1;
  }
  // след коробки на xz: ось u1 — самая горизонтальная из трёх, u2 — перпендикуляр
  int hk = 0; for (int k = 1; k < 3; k++) if (fabsf(a[k][1]) < fabsf(a[hk][1])) hk = k;
  float ux = a[hk][0], uz = a[hk][2], ul = sqrtf(ux * ux + uz * uz);
  if (ul < 1e-4f) { ux = 1; uz = 0; } else { ux /= ul; uz /= ul; }
  float vx = -uz, vz = ux, e1 = 0.f, e2 = 0.f;
  for (int k = 0; k < 3; k++) {
    e1 += C->h[k] * fabsf(a[k][0] * ux + a[k][2] * uz);
    e2 += C->h[k] * fabsf(a[k][0] * vx + a[k][2] * vz);
  }
  float ax[4][2] = {{fx, fz}, {rx, rz}, {ux, uz}, {vx, vz}}, best = 1e9f;
  for (int k = 0; k < 4; k++) {
    float px = ax[k][0], pz = ax[k][1];
    float ea = cb->hl * fabsf(fx * px + fz * pz) + cb->hw * fabsf(rx * px + rz * pz);
    float eb = e1 * fabsf(ux * px + uz * pz) + e2 * fabsf(vx * px + vz * pz);
    float o = dx * px + dz * pz, ov = ea + eb - fabsf(o);
    if (ov <= 0.f) return 0;
    if (ov < best) { best = ov; *nx = o < 0 ? -px : px; *nz = o < 0 ? -pz : pz; }
    float sg = o < 0 ? -1.f : 1.f;   // ось от пропа к кузову: сближение — скорость против неё
    lastEntry(ov, -(vel[0] * px + vel[2] * pz) * sg, &bestT, &imp);
  }
  *depth = best;
  *impact = imp;
  return 1;
}

#ifndef FOLLOWABS1107_OLD
#define FOLLOWABS1107_OLD 0
#endif
#ifndef SPFOLLOW_SV_OLD
#define SPFOLLOW_SV_OLD 0
#endif
static void followOffsets(StateProps *sp, unsigned i) {
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  spColOff[0] = spColOff[1] = spColOff[2] = 0.f; spColOffW[0] = spColOffW[1] = spColOffW[2] = 0.f;
  if (!SPFOLLOW_SV_OLD && sp->follow && sp->inst[i].type < sp->nfollow && sp->follow[sp->inst[i].type]) {
    unsigned t_ = sp->inst[i].type, j_ = sp->follow[t_] - 1u;
    if (j_ < T->nJoint) {
      if (!sp->followOk[t_]) {   // сдвиг сустава в кадре 0: один раз по первому подошедшему экземпляру (ключи могут быть ещё не подгружены — тогда повтор позже)
        float keep = r->poseF; r->poseF = 0.f; r->poseValid = 0; spPose(sp, i);
        if (r->poseValid) { memcpy(&sp->followT0[3 * t_], r->jm + j_ * 12 + 9, 12); sp->followOk[t_] = 1; }
        r->poseF = keep; r->poseValid = 0;
      }
      if (sp->followOk[t_]) { spPose(sp, i); if (r->poseValid) for (int q_ = 0; q_ < 3; q_++) { if (FOLLOWABS1107_OLD) spColOff[q_] = r->jm[j_ * 12 + 9 + q_] - sp->followT0[3 * t_ + q_]; else spColOffW[q_] = r->jm[j_ * 12 + 9 + q_]; } }
    }
  }
}
// Круг 7: центр объёма k экземпляра i в мире с учётом следования за суставом (для теста и отладки); 0 — нет такого объёма
int spVolumeCenter(StateProps *sp, unsigned i, unsigned k, float out[3]) {
  if (i >= sp->ninst) return 0;
  const SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i); if (k >= T->nCol) return 0;
  followOffsets(sp, i);
  const SpCol *C = &sp->col[T->firstCol + k]; float cc[3] = {C->c[0] + spColOff[0], C->c[1] + spColOff[1], C->c[2] + spColOff[2]}, c[3];
  mulRv(r->R, cc, c); out[0] = c[0] + r->x + spColOffW[0]; out[1] = c[1] + r->y + spColOffW[1]; out[2] = c[2] + r->z + spColOffW[2]; return 1;
}
static void carContact(StateProps *sp, unsigned i, const ColWorld *world, Car *car, Coins *coins, CarBox *cb) {
  SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
  if (!r->alive || !r->colOn || !T->nCol) return;
  float c[3]; centerW(sp, i, c);
  float rad = sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + 1.f;
  float dx = c[0] - cb->x, dz = c[2] - cb->z, rr = rad + cb->hl + cb->hw;
  if (dx * dx + dz * dz > rr * rr) return;
  int touched = 0; float bn[2] = {0, 0}, bd = 0.f, bimp = 0.f;
  // круг 5 (g11, P11 Д2 / P12-12): объём типа со следованием едет за суставом (только сдвиг, без поворота): текущая позиция сустава минус позиция в кадре 0. SPFOLLOW_SV_OLD=1 — объёмы стоят
#ifndef SPFOLLOW_SV_OLD
#define SPFOLLOW_SV_OLD 0
#endif
  // круг 7 (g11, по оригиналу): к положению экземпляра добавляется положение сустава ЦЕЛИКОМ (не сдвиг от кадра 0) и без поворота экземпляра; поворот у объёма — экземпляра, сустав его не вращает.
  // У ящика сустав 3 в покое в нуле (разницы нет, кроме поворота экземпляра), у автомата сустав 1 в покое на 0,261 м по оси Z, у паука L7 сустав 1 в покое в (0,92; −0,52; 0) и за клип уходит на 5,1 м.
  // FOLLOWABS1107_OLD=1 — сдвиг от кадра 0 в осях экземпляра (круг 5)
  followOffsets(sp, i);
  spBaseExt = !spPoleBaseOld && T->cls == 10 && T->h[1] >= 2.f;
  for (unsigned k = 0; k < T->nCol; k++) {
    const SpCol *C = &sp->col[T->firstCol + k];
    if (C->second && !r->col2On) continue;
    float nx, nz, d, imp = 0.f;
    if (hitVolume(r, C, cb, car->vel, &nx, &nz, &d, &imp)) {
      if (imp > bimp) bimp = imp;
      if (d > bd) { bd = d; bn[0] = nx; bn[1] = nz; touched = 1; }
    }
  }
  spBaseExt = 0;
  // №902 (Роб 23.10: «разбивание стекла иногда отстаёт от удара — то вовремя, то поздно; с деревьями так же; всегда в тот же кадр, что удар»):
  // стекло витрины, будка, забор часто стоят в стене мира (COL1): заезд, где стена касается раньше объёма предмета, гасит машину о стену —
  // предмет бился позже (дожим, 2–40 кадров) или никогда (L1: 188 и 32 из 251 таких заездов, tests/smash902). Удар машины о твёрдое в этом
  // кадре (hardV ≥ SP_HIT_SPEED) засчитывается и предмету, если его объём в SP_WALL_PAD от кузова; машину он не выталкивает (уже стена).
  // SP_WALL902_OLD=1 — как было
#ifndef SP_WALL902_OLD
#define SP_WALL902_OLD 0
#endif
#define SP_WALL_PAD .35f
  int viaWall = 0;
  if (!touched && !SP_WALL902_OLD && car->hardV >= SP_HIT_SPEED) {
    CarBox big = *cb; big.hl += SP_WALL_PAD; big.hw += SP_WALL_PAD;
    for (unsigned k = 0; k < T->nCol && !touched; k++) {
      const SpCol *C = &sp->col[T->firstCol + k];
      if (C->second && !r->col2On) continue;
      float nx, nz, d, imp = 0.f;
      if (hitVolume(r, C, &big, car->vel, &nx, &nz, &d, &imp)) { bn[0] = nx; bn[1] = nz; bd = 0.f; bimp = car->hardV; touched = viaWall = 1; }
    }
  }
  // №975 (Роб 25.10, L5 F3-2: «на перекрёстке красные фонари не сбиваются, ни один»): у высокого предмета (столб, фонарь, светофор: cls 10) объёмы по данным лежат на 1,8–3 м выше земли
  // (нижний цилиндр 1,8…6,5 м над основанием), а основание стоит стеной мира (COL1): машина упирается в стену и ни один объём не достаёт до кузова по высоте, №902 не засчитывает
  // удар — столб стоит с машиной у основания. Удар о твёрдое в этом кадре (hardV ≥ SP_HIT_SPEED) засчитывается и высокому предмету, если его объём по плану в SP_WALL_PAD от
  // кузова (высота не проверяется: основание под ним и есть стена). Реакция — состояние из данных диска, падение — шарнир (poleFallStart). SP_POLEWALL975_OLD=1 / flag polewall975old 1 — как было
#ifndef SP_POLEWALL975_OLD
#define SP_POLEWALL975_OLD 0
#endif
  if (!touched && !spPoleWallOld && !SP_WALL902_OLD && car->hardV >= SP_HIT_SPEED && T->cls == 10 && T->h[1] >= 2.f) {
    CarBox big = *cb; big.hl += SP_WALL_PAD; big.hw += SP_WALL_PAD; spVRelax = 1;
    for (unsigned k = 0; k < T->nCol && !touched; k++) {
      const SpCol *C = &sp->col[T->firstCol + k];
      if (C->second && !r->col2On) continue;
      float nx, nz, d, imp = 0.f;
      if (hitVolume(r, C, &big, car->vel, &nx, &nz, &d, &imp)) { bn[0] = nx; bn[1] = nz; bd = 0.f; bimp = car->hardV; touched = viaWall = 1; }
    }
    spVRelax = 0;
  }
  if (!touched) return;
  sp->touchInst = (int)i; sp->touchSpeed = 0.f;   // сверка g11 круг 2 (P06-23)
  // сверка g11, P11-09 (Н): первое касание не-ломаемого стейтпропа (типы 2, 3, 10) подаёт FEATHER_TOUCH до проверки силы, на каждое срабатывание контакта (кулдаун SP_COOL);
  // HIT и VEHICLE_HIT — только при импульсе выше SP_HIT_IMPULSE. SPFEATHER_SV_OLD=1 — как было (FEATHER не подаётся)
  if (!SPFEATHER_SV_OLD && r->cool <= 0.f && (T->cls == 2 || T->cls == 3 || T->cls == 10)) spEvent(sp, i, SP_EV_FEATHER);
  const int bushEntry = r->cool <= 0.f;   // №1101: первое касание этого заезда (до взведения остывания ниже)
  float vn = car->vel[0] * bn[0] + car->vel[2] * bn[1];   // < 0 — едем в проп
#if SP_HIT_MINAXIS
  float closing = -vn;     // прежнее: сближение по оси наименьшего перекрытия в плане
#else
  float closing = fmaxf(bimp, -vn);   // правка 297: по оси последнего входа (в том числе сверху); не меньше прежнего
#endif
  // №998 (Роб 26.10, TEST71 на PSP, L5 F3-2: «красные столбы не сбиваются, сколько можно», повтор №975): прямой удар в ствол работал (№975), а касание вскользь — бортом или углом кузова на ходу —
  // нет: у вертикального цилиндра ствола нормаль SAT лежит поперёк движения, сближение по ней ~0 (< SP_HIT_SPEED), касание есть, реакции нет (перепись tests/pole998: у фонаря типа 8 при боковом смещении
  // заезда 1,25 м от ствола 3–5 из 16 заездов «дыра»; на перекрёстке углы и борта задевают столбы чаще, чем бьют в лоб). Теперь у высокого предмета cls 10 касание на ходу засчитывается ударом:
  // скорость удара — не меньше скорости кузова до касания (потеря скорости машины — по доле сближения от скорости, но не меньше четверти). POLE998_OLD=1 / flag pole998 0 — как было
  float vPol = 1.f;
  if (!spPole998Old && T->cls == 10 && T->h[1] >= 2.f) {
    float vp = sqrtf(car->velPre[0] * car->velPre[0] + car->velPre[2] * car->velPre[2]);
    if (closing < vp && vp >= SP_HIT_SPEED) { vPol = fmaxf(.25f, closing / vp); closing = vp; }
  }
  sp->lastImpact = closing;
  // №564: Деревянные ворота у поля/огорода (L1 J3-6, l1/l4/l7_fence_Shape) — в оригинале PS2 сбиваются
  // любой машиной с обеих сторон (AnimDynaPhys cls 4, physProp 1, 4 доски fencebarShape). Ошибочный cls 11
  // (SP_CLS_HARDFENCE) на 10.10 требовал массу машины >= 3750 кг, из-за чего лёгкие машины (Гомер 1500 кг) бились
  // в них как в каменную стену. По умолчанию GATE_BREAK_OLD=0 сбивает ворота любой машиной при closing >= SP_HIT_SPEED.
  // Флаг отката: -DGATE_BREAK_OLD=1 (или SP_HARDFENCE_OLD=0) оставляет блокировку для машин легче 3750 кг.
#ifndef GATE_BREAK_OLD
#define GATE_BREAK_OLD 0
#endif
#ifndef SP_HARDFENCE_OLD
#define SP_HARDFENCE_OLD (!GATE_BREAK_OLD)
#endif
#ifndef SP_FENCE_MASS
#define SP_FENCE_MASS 3750.f
#endif
  // сверка g11, P16 Д6 (Н): порог удара машины — по импульсу (масса машины x сближение > SP_HIT_IMPULSE, около 0,07 м/с для 1500 кг), а не по скорости сближения 2 м/с (SP_HIT_SPEED).
  // Запасной DESTROYED у ударов машины оставлен: отчёт P18 Д11 прямо называет цепочку DESTROYED, STOMP, HIT для ломающихся, а P16 Д6 просит его убрать — строки противоречат, см. журнал.
  // SPIMP_SV_OLD=1 — порог 2 м/с
  // Круг 3 (g11, проверка по исходнику): ломающийся предмет (тип 4) отвечает на любое касание машиной, без порога; без обработчика «удар машиной» ему подряд подаются разрушение, стомп и удар (каждое
  // событие меняет состояние по очереди). У остальных порог по импульсу, цепочка «удар машиной, иначе обычный удар», запасного разрушения нет. SPIMP_SV_OLD=1 — общая цепочка с порогом 2 м/с
  const int breakable = T->cls == 4;
  int impOk = SPIMP_SV_OLD ? closing >= SP_HIT_SPEED : (breakable || closing * car->p.mass > SP_HIT_IMPULSE);
  if (r->cool <= 0.f && impOk && (SP_HARDFENCE_OLD || T->cls != 11 || car->p.mass >= SP_FENCE_MASS)) {
    // событие машины: первое, что обрабатывает текущее состояние (гипотеза §6: VEHICLE_HIT -> HIT -> DESTROYED)
    r->hv[0] = car->velPre[0]; r->hv[1] = car->velPre[2];   // №923: куда ехала машина к удару (до стены кадра) — туда летят осколки
    int evAny;
    if (SPIMP_SV_OLD) evAny = spEvent(sp, i, SP_EV_VEHICLE_HIT) || spEvent(sp, i, SP_EV_HIT) || spEvent(sp, i, SP_EV_DESTROYED);
    else if (breakable) { evAny = spEvent(sp, i, SP_EV_VEHICLE_HIT); if (!evAny) { evAny = spEvent(sp, i, SP_EV_DESTROYED); evAny |= spEvent(sp, i, SP_EV_STOMP); evAny |= spEvent(sp, i, SP_EV_HIT); } }
    else evAny = spEvent(sp, i, SP_EV_VEHICLE_HIT) || spEvent(sp, i, SP_EV_HIT);
    if (evAny) {
      sp->hits++;
      runCalls(sp, i, world, coins);   // вызовы с временем 0 — сразу (коллизия уходит в этом же кадре)
      poleFallStart(sp, i, car, bn);   // №975
      int broken = !r->alive || !r->colOn;
      if (PROP_BREAK_PASS_OLD || !broken) {
        float k = SP_SMASH_LOSS * T->mass / (car->p.mass + T->mass) * vPol;
        if (T->cls != 2 || !SPBUSH_SV_OLD) { car->vel[0] *= 1.f - k; car->vel[2] *= 1.f - k; }
        if (sp->killSpeed && SPKILL_SV_OLD) car->vel[0] = car->vel[2] = 0.f;   // сверка g11 P11-20: KILL_SPEED гасит скорость предмета, не машины
      }
      sp->killSpeed = 0;   // consume this object's callback, never the next impact
    }
  }
  r->cool = SP_COOL;
  if (viaWall) return;   // №902: машину остановила стена мира, предмет не выталкивает
  // Правка «несбиваемое должно сбиваться» (10.10, боты L1–L7: фонари/автоматы/гидранты L7 400 из 428 заездов на 50 км/ч — объект
  // ответил (state != 0), но коллизия на время падения/тряски оставалась и гасила 12.6 из 13.9 м/с, как стена). Объект, уже
  // ответивший на удар (состояние не начальное), машину больше не выталкивает: бьём один раз, проезжаем. Прежнее: -DSP_REACT_PASS_OLD=1
#ifndef SP_REACT_PASS_OLD
#define SP_REACT_PASS_OLD 0
#endif
  // сверка g11, P11-08 (С): куст-шар (тип 2) — жёсткое неподвижное тело, машина сталкивается с ним как со стеной, контакт не отменяется (удар с импульсом больше 100 даёт HIT/VEHICLE_HIT — куст трясётся и
  // возвращается через 1,3 с). SPBUSH_SV_OLD=1 (по умолчанию, ждём слова Роба) — насквозь с тряской
  // №1201 (Роб на TEST89: «некоторые трамплины воспринимаются по-другому»; бисекция tests/jumps/twin.sh по версиям): с TEST85 кусты-стейтпропы получили тело (№1137), и машина в ПОЛЁТЕ, задев такое тело (L1 F12-6: предмет типа 78 на башне, y 11,9; jumpzone4, 100 км/ч),
  // теряла 60 % скорости (BUSH_BRAKE) и падала на 14,8 м вместо 39,9 м (запись PS2 32,7 м, итог 101,5 м; TEST84 110,5 м). Куст-тело работает только пока хоть одно колесо стоит на земле или кузов лежит на опоре: в воздухе машина проходит сквозь.
  // BUSHAIR1201_OLD=1 — как в TEST85…TEST90 (куст тормозит и в полёте)
  if (!BUSHAIR1201_OLD && T->cls == 2 && SPBUSH_SV_OLD && car->wheels == 0 && !car->grounded) return;
  const int bushMid = T->cls == 2 && SPBUSH_SV_OLD && !BUSHMID1101_OLD;   // №1101: середина между «насквозь» и «стена»
  int bushSolid = (!SPBUSH_SV_OLD || bushMid) && T->cls == 2;
  if (bushMid && r->alive && r->colOn) {
    if (bushEntry) {
      const int chain = car->bushPassT > 0.f;
      r->bushPass = (hypotf(car->vel[0], car->vel[2]) >= (chain ? BUSH_PASS_V_CHAIN : BUSH_PASS_V));   // по скорости машины, а не по нормальной составляющей: бортом скорость та же
      if (r->bushPass) { const float kb = chain ? BUSH_BRAKE_CHAIN : BUSH_BRAKE; car->vel[0] *= 1.f - kb; car->vel[2] *= 1.f - kb; car->v *= 1.f - kb; car->bushPassT = BUSH_PASS_T; }
    }
    if (r->bushPass) return;   // продавливает: без выталкивания и отскока
  }
  if ((T->cls == 2 && SPBUSH_SV_OLD && !bushMid) || !r->alive || !r->colOn || (!SP_REACT_PASS_OLD && r->state != 0 && !bushSolid)) return;   // куст-шар (cls 2) — насквозь, с тряской (гипотеза §9.3)
  // целый проп — как стена: вытолкнуть кузов и погасить скорость в проп
  car->x += bn[0] * bd; car->z += bn[1] * bd; cb->x += bn[0] * bd; cb->z += bn[1] * bd;
  vn = car->vel[0] * bn[0] + car->vel[2] * bn[1];
  const float eCar = bushMid ? BUSH_E_MID : SP_CAR_E;
  if (vn < 0.f) { car->vel[0] -= (1.f + eCar) * vn * bn[0]; car->vel[2] -= (1.f + eCar) * vn * bn[1]; }
}

// Правка «быстрый расчёт» (слово Роба 10.10): машина против ВСЕХ экземпляров каждый кадр — у каждого центр коробки через поворот и корень
// радиуса типа. Прежде чем звать carContact, отсекаем по положению экземпляра: центр отстоит от положения не дальше |T->c|, значит, если
// положение дальше rad(тип) + кузов + |T->c|, прежний тест carContact тоже вернул бы «далеко». Радиусы типов — таблица на вызов.
// -DSP_SCAN_OLD=1 — прежний обход
#ifndef SP_SCAN_OLD
#define SP_SCAN_OLD 0
#endif
unsigned spHitStat[3];
void spHitCar(StateProps *sp, const ColWorld *world, Car *car, Coins *coins) {
  if (car) {
    CarBox cb; carBox(car, &cb);
    static float *lim; static unsigned limN;
    if (!SP_SCAN_OLD && sp->ntype && (limN < sp->ntype || !lim)) { free(lim); lim = (float *)malloc(sizeof(float) * sp->ntype); limN = lim ? sp->ntype : 0; }
    if (SP_SCAN_OLD || !lim) { for (unsigned i = 0; i < sp->ninst; i++) carContact(sp, i, world, car, coins, &cb); return; }
    for (unsigned t = 0; t < sp->ntype; t++) { const SpType *T = &sp->type[t];
      lim[t] = sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + 1.f + sqrtf(T->c[0] * T->c[0] + T->c[1] * T->c[1] + T->c[2] * T->c[2]) + cb.hl + cb.hw + .05f; }
    for (unsigned i = 0; i < sp->ninst; i++) {
      const SpRun *r = &sp->run[i]; float dx = r->x - cb.x, dz = r->z - cb.z, l = lim[sp->inst[i].type]; spHitStat[0]++;
      if (dx * dx + dz * dz > l * l) continue;
      spHitStat[1]++; if (sp->type[sp->inst[i].type].nCol) spHitStat[2]++;   // №1159: счётчики для теста — просмотрено / дошло до carContact / из них с объёмом (дальше идёт дорогая часть)
      carContact(sp, i, world, car, coins, &cb);
    }
  }
}

// №1000 (стейтпропы: «каждый кадр перебираются все ~200, видимых 13–18»; рисование уже идёт по сетке 48 м, а шаг логики stepInst гонял все экземпляры): качающиеся деревья и кусты
// (одно состояние, только поза по времени, без вызовов/частиц/билбордов/масок видимости; ≈ 60 % экземпляров на L3–L7) дальше SPN_R от глаза, машины и Гомера не шагают: рисование
// всё равно держит их позу замершей дальше 25 м (main_loop.h SP_SWAY_FAR). Сон копит dt; при приближении время догоняется теми же сложениями (одинаковые dt — побитно как без сна),
// потом обычный шаг. Состояние меняют только удары и события рядом с машиной/Гомером или вызовы миссии: enter() обнуляет сон. SPNEAR1000_OLD=1 / «flag spnear1000 0» / файл spnear1000old — как было
#ifndef SPNEAR1000_OLD
#define SPNEAR1000_OLD 0
#endif
#define SPN_R 45.f
#define SPN_MAXN 3000
int spNear1000 = !SPNEAR1000_OLD;
unsigned spNearStat[2];   // отчёт smoke SPNEAR: спящих в последнем кадре, экземпляров
static int spSleepable(const StateProps *sp, unsigned i) {
  const SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i); const SpState *S = ST(sp, i);
  return r->alive && T->kind == 0 && T->nState <= 1 && !T->nBill && !T->pvisMask && S->poseMode && !S->nCall && S->autoTo == 0xFF && !S->nFx
      && !r->fall && !r->burst && !r->loopOn && r->cool <= 0.f && !r->jm0 && r->liftK == 0.f && !sp->inst[i].tag;
}
static void spNearWake(SpRun *r) {
#ifdef SPNEAR1000_RED
  r->slN = 0; return;   // красный контроль гейта tests/spnear1000: сон без догона времени
#endif
  if (!r->slMix) for (unsigned k = 0; k < r->slN; k++) r->t += r->slD0; else r->t += r->slSum;
  r->slN = 0;
}
// сверка g11, P11-05 и P11-Д1: взрыв машины (sverka_shared.h svBoom, заполняет car.c окна g01) бьёт стейтпропы, чья рамка пересекает сферу взрыва: события по силе (больше 400 — DESTROYED и HIT,
// больше 800 ещё STOMP; цепочка до первого принявшего состояния), у типа 10 ещё падение от центра взрыва. Читатель хранит свой последний seq
static unsigned boomSeqSp;
static void boomStep(StateProps *sp, const ColWorld *world, Coins *coins) {
  if (SPBOOM_SV_OLD || svBoom.seq == boomSeqSp) return;
  boomSeqSp = svBoom.seq;
  if (svBoom.radius <= 0.f) return;
  unsigned h0 = sp->hnrHits;
  for (unsigned i = 0; i < sp->ninst; i++) {
    SpRun *r = &sp->run[i]; const SpType *T = TY(sp, i);
    if (!r->alive || !r->colOn || T->kind) continue;
    float c[3], rad; spBounds(sp, i, c, &rad);
    float dx = c[0] - svBoom.x, dy = c[1] - svBoom.y, dz = c[2] - svBoom.z, rr = svBoom.radius + rad;
    if (dx * dx + dy * dy + dz * dz > rr * rr) continue;
    int hit = 0;
    if (svBoom.force > 800.f) hit = spEvent(sp, i, SP_EV_STOMP);
    if (!hit && svBoom.force > 400.f) hit = spEvent(sp, i, SP_EV_DESTROYED);
    if (!hit) hit = spEvent(sp, i, SP_EV_HIT);
    if (hit) runCalls(sp, i, world, coins);
    spImpulse(sp, i, dx, dz, svBoom.force);
  }
  if (!svBoom.byPlayer) { sp->hnrHits = h0; if (sp->hitInstN > h0) sp->hitInstN = h0 < 16 ? h0 : 16; }   // шкала Hit & Run — только за взрыв, устроенный игроком
}
void spUpdate(StateProps *sp, const ColWorld *world, Car *car, Coins *coins, float dt) {
  if (dt <= 0.f) return;
  if (dt > 0.1f) dt = 0.1f;
  sp->touchInst = -1;
  boomStep(sp, world, coins);
  if (spNear1000 && sp->nearOn && car) {
    float p[3][2] = {{sp->nearP[0][0], sp->nearP[0][1]}, {sp->nearP[1][0], sp->nearP[1][1]}, {car->x, car->z}}; unsigned ns = 0;
    for (unsigned i = 0; i < sp->ninst; i++) { SpRun *r = &sp->run[i];
      if (spSleepable(sp, i) && r->slN < SPN_MAXN) {
        int far = 1;
        for (int k = 0; k < 3 && far; k++) { float dx = r->x - p[k][0], dz = r->z - p[k][1]; if (dx * dx + dz * dz <= SPN_R * SPN_R) far = 0; }
        if (far) { if (!r->slN) { r->slD0 = dt; r->slMix = 0; r->slSum = 0.f; } else if (dt != r->slD0) r->slMix = 1; r->slSum += dt; r->slN++; ns++; continue; } }
      if (r->slN) spNearWake(r);
      stepInst(sp, i, dt, world, coins); }
    sp->nearSleep = ns; spNearStat[0] = ns; spNearStat[1] = sp->ninst;
  } else for (unsigned i = 0; i < sp->ninst; i++) { if (sp->run[i].slN) spNearWake(&sp->run[i]); stepInst(sp, i, dt, world, coins); }
  spHitCar(sp, world, car, coins);
}

// ---- отрисовка
int spDrawable(const StateProps *sp, unsigned i, float ex, float ez, float maxD) {
  const SpRun *r = &sp->run[i];
  if (!r->alive || !stAny(sp, TY(sp, i), ST(sp, i))) return 0;
  float dx = r->x - ex, dz = r->z - ez;
  return dx * dx + dz * dz <= maxD * maxD;
}

void spBounds(const StateProps *sp, unsigned i, float c[3], float *r) {
  const SpType *T = TY(sp, i); centerW(sp, i, c);
  *r = sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + 0.5f;
}

void spPose(StateProps *sp, unsigned i) {
  SpRun *r = &sp->run[i];
  if (r->poseValid) return;
  const SpType *T = TY(sp, i);
  unsigned nfr = T->frames & 0xFFFF, ntr = T->frames >> 16;
  float f = r->poseF; if (f < 0) f = 0; if (nfr && f > (float)(nfr - 1)) f = (float)(nfr - 1);
  unsigned f0 = (unsigned)f, f1 = f0 + 1 < nfr ? f0 + 1 : f0; float u = f - (float)f0;
  const float *keys = sp->typeKey ? sp->typeKey[sp->inst[i].type] : (sp->key ? sp->key + (size_t)T->keyOff * 7 : NULL);
  if(ntr && !keys && sp->ensureKey && !sp->ensureKey(sp->inst[i].type))keys=sp->typeKey[sp->inst[i].type];
  if (ntr && !keys) return; // streamed pose is never fabricated from an unloaded track
  for (unsigned j = 0; j < T->nJoint; j++) {
    const SpJoint *J = &sp->joint[T->firstJoint + j];
    float q[4], t[3], L[9];
    if (J->track >= 0 && nfr) {
      const float *a = keys + ((size_t)f0 * ntr + J->track) * 7,   // keyOff — в записях по 7 float
                  *b = keys + ((size_t)f1 * ntr + J->track) * 7;
      float d = a[3] * b[3] + a[4] * b[4] + a[5] * b[5] + a[6] * b[6], sb = d < 0 ? -u : u, n = 0;
      for (int k = 0; k < 3; k++) t[k] = a[k] + (b[k] - a[k]) * u;
      if (r->liftK > 1.f) { const float *a0 = keys + (size_t)J->track * 7; t[1] = a0[1] + (t[1] - a0[1]) * r->liftK; }   // №519: ход вверх от кадра 0 растянут (дорожка суставов подъёмника)
      for (int k = 0; k < 4; k++) { q[k] = a[3 + k] * (1.f - u) + b[3 + k] * sb; n += q[k] * q[k]; }
      n = n > 1e-12f ? 1.f / sqrtf(n) : 1.f; for (int k = 0; k < 4; k++) q[k] *= n;
    } else { memcpy(q, J->q, sizeof q); memcpy(t, J->t, sizeof t); }
    quatR(q, L);
    for (int c = 0; c < 3; c++) { L[3 * c] *= J->s[c]; L[3 * c + 1] *= J->s[c]; L[3 * c + 2] *= J->s[c]; }
    float *W = r->jm + j * 12;
    if (J->parent < 0) { memcpy(W, L, sizeof L); memcpy(W + 9, t, sizeof t); }
    else {
      const float *P = r->jm + J->parent * 12;
      mulRR(P, L, W); mulRv(P, t, W + 9); W[9] += P[9]; W[10] += P[10]; W[11] += P[11];
    }
  }
  r->poseValid = 1; r->posed = 1;
}

int spPieceOn(const StateProps *sp, unsigned i, unsigned k) {
  const SpType *T = TY(sp, i); const SpRun *r = &sp->run[i];
  if (k >= T->nPiece || !r->alive) return 0;
  unsigned slot = ((unsigned)sp->piece[T->firstPiece + k].flags >> 16) & 0xFF, vw = VW(T);
  if (!stSlot(sp, T, ST(sp, i), slot)) return 0;
  if (pvSlot(sp, T, slot)) {
    unsigned nfr = T->frames & 0xFFFF, f = r->poseF > 0 ? (unsigned)r->poseF : 0;
    if (f >= nfr) f = nfr - 1;
    if (!((sp->vis[T->visOff + (size_t)f * vw + (slot >> 5)] >> (slot & 31)) & 1u)) return 0;
  }
  return 1;
}

void spPieceMatrix(const StateProps *sp, unsigned i, unsigned k, float m[16]) {
  const SpType *T = TY(sp, i); const SpRun *r = &sp->run[i]; const SpPiece *P = &sp->piece[T->firstPiece + k];
  const float *J = r->jm + ((P->flags >> 8) & 0xFF) * 12;
  float A[9], o[3] = {P->ox, P->oy, P->oz}, jo[3], b[3];
  mulRR(r->R, J, A);
  mulRv(J, o, jo); jo[0] += J[9]; jo[1] += J[10]; jo[2] += J[11];
  mulRv(r->R, jo, b);
  if (r->jm0) {   // №923: смещение куска от места покоя поворачивается вокруг вертикали (по ходу машины); место покоя и поворот куска — прежние
    const float *J0 = r->jm0 + ((P->flags >> 8) & 0xFF) * 12; float j0[3], b0[3];
    mulRv(J0, o, j0); j0[0] += J0[9]; j0[1] += J0[10]; j0[2] += J0[11];
    mulRv(r->R, j0, b0);
    float dx = b[0] - b0[0], dz = b[2] - b0[2];
    b[0] = b0[0] + r->dirC * dx + r->dirS * dz; b[2] = b0[2] + r->dirC * dz - r->dirS * dx;   // b[1] (высота) прежняя: b0[1] + (b[1] - b0[1])
  }
  float s = P->sc;
  m[0] = A[0] * s; m[1] = A[1] * s; m[2] = A[2] * s; m[3] = 0;
  m[4] = A[3] * s; m[5] = A[4] * s; m[6] = A[5] * s; m[7] = 0;
  m[8] = A[6] * s; m[9] = A[7] * s; m[10] = A[8] * s; m[11] = 0;
  m[12] = b[0] + r->x; m[13] = b[1] + r->y; m[14] = b[2] + r->z; m[15] = 1;
}

static int billAxesOld;
void spBillAxesOld(int old) { billAxesOld = old != 0; }
int spBillQuad(const StateProps *sp, unsigned i, unsigned bi, float ex, float ey, float ez, float v[12], uint32_t *abgr, int *tex, uint32_t *flags) {
  const SpType *T = TY(sp, i); const SpRun *r = &sp->run[i];
  if (bi >= T->nBill || !r->alive) return 0;
  const SpBill *B = &sp->bill[T->firstBill + bi];
  if (!stSlot(sp, T, ST(sp, i), B->slot)) return 0;
  if (SP_BILL_PVIS_ON && (B->flags & SP_BILL_PVIS) && pvSlot(sp, T, B->slot)) {   // огонь барьера 0x3F0000F: мигает по кадрам PVIS
    unsigned nfr = T->frames & 0xFFFF, vw = VW(T), f = r->poseF > 0 ? (unsigned)r->poseF : 0;
    if (nfr && f >= nfr) f = nfr - 1;
    if (nfr && !((sp->vis[T->visOff + (size_t)f * vw + (B->slot >> 5)] >> (B->slot & 31)) & 1u)) return 0;
  }
  const float *J = r->jm + B->joint * 12;
  float jo[3], p[3], A[9];
  mulRv(J, B->t, jo); jo[0] += J[9]; jo[1] += J[10]; jo[2] += J[11];
  mulRv(r->R, jo, p); p[0] += r->x; p[1] += r->y; p[2] += r->z;
  float X[3], Y[3];
  if (B->mode == 0) {   // NOAX: квадрат в плоскости XY сустава (тень ящика и свет автомата лежат на земле)
    mulRR(r->R, J, A);
    for (int k = 0; k < 3; k++) { X[k] = A[k] * B->w * 0.5f; Y[k] = A[3 + k] * B->h * 0.5f; }
  } else if (!billAxesOld && (B->mode == 2 || B->mode == 3)) {   // №371: конус остаётся вдоль заданной оси, вращается только вокруг неё
    if (B->mode == 3) { mulRR(r->R, J, A); for (int k = 0; k < 3; k++) Y[k] = A[3 + k]; }
    else { Y[0] = 0.f; Y[1] = 1.f; Y[2] = 0.f; }
    float yl = sqrtf(Y[0] * Y[0] + Y[1] * Y[1] + Y[2] * Y[2]); if (yl < 1e-6f) return 0;
    for (int k = 0; k < 3; k++) Y[k] /= yl;
    float D[3] = {ex - p[0], ey - p[1], ez - p[2]};
    X[0] = Y[1] * D[2] - Y[2] * D[1]; X[1] = Y[2] * D[0] - Y[0] * D[2]; X[2] = Y[0] * D[1] - Y[1] * D[0];
    float xl = sqrtf(X[0] * X[0] + X[1] * X[1] + X[2] * X[2]);
    if (xl < 1e-6f) {   // глаз на оси: детерминированный перпендикуляр, без деления на ноль
      float D0[3] = {0.f, 0.f, 1.f}; if (fabsf(Y[2]) > .9f) { D0[0] = 1.f; D0[2] = 0.f; }
      X[0] = Y[1] * D0[2] - Y[2] * D0[1]; X[1] = Y[2] * D0[0] - Y[0] * D0[2]; X[2] = Y[0] * D0[1] - Y[1] * D0[0];
      xl = sqrtf(X[0] * X[0] + X[1] * X[1] + X[2] * X[2]);
    }
    for (int k = 0; k < 3; k++) { X[k] *= B->w * .5f / xl; Y[k] *= B->h * .5f; }
  } else {              // AAX к камере: прежнее поведение
    float dx = p[0] - ex, dy = p[1] - ey, dz = p[2] - ez, l = sqrtf(dx * dx + dz * dz);
    if (l < 1e-3f) return 0;
    X[0] = dz / l; X[1] = 0; X[2] = -dx / l;
    float dl = sqrtf(dx * dx + dy * dy + dz * dz);
    Y[0] = -(dy / dl) * (dx / l); Y[1] = l / dl; Y[2] = -(dy / dl) * (dz / l);
    for (int k = 0; k < 3; k++) { X[k] *= B->w * 0.5f; Y[k] *= B->h * 0.5f; }
  }
  for (int k = 0; k < 3; k++) {
    v[k] = p[k] - X[k] - Y[k]; v[3 + k] = p[k] + X[k] - Y[k]; v[6 + k] = p[k] - X[k] + Y[k]; v[9 + k] = p[k] + X[k] + Y[k];
  }
  uint32_t c = B->rgba;   // ARGB (Pure3D) -> ABGR (GE)
  if (sp->bqgIdx) {   // правка 76: кадр анимации BQG — ширина, высота, цвет (видимость — альфа 0)
    unsigned gi = T->firstBill + bi, first = sp->bqgIdx[2 * gi], n = sp->bqgIdx[2 * gi + 1];
    if (n && first + n <= sp->nbqgRow) {
      const uint32_t *row = sp->bqgRow + 3 * (first + (unsigned)(r->poseF > 0.f ? r->poseF : 0.f) % n);
      float sw, sh; memcpy(&sw, &row[0], 4); memcpy(&sh, &row[1], 4); c = row[2];
      float kw = B->w > 1e-4f ? sw / B->w : 1.f, kh = B->h > 1e-4f ? sh / B->h : 1.f;
      for (int k = 0; k < 3; k++) { float cx = (v[k] + v[3 + k] + v[6 + k] + v[9 + k]) * .25f;
        float ex_ = (v[3 + k] - v[k]) * .5f, ey_ = (v[6 + k] - v[k]) * .5f;
        v[k] = cx - ex_ * kw - ey_ * kh; v[3 + k] = cx + ex_ * kw - ey_ * kh; v[6 + k] = cx - ex_ * kw + ey_ * kh; v[9 + k] = cx + ex_ * kw + ey_ * kh; }
    }
  }
  *abgr = (c & 0xFF00FF00u) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
  *tex = B->tex; *flags = B->flags;
  return 1;
}
