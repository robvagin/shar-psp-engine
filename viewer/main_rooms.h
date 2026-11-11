// ---------- 11.2: комнаты. Коллизия и переходы — пакет SHI1 (viewer/rooms.c, тот же код у ботов), рисование — SHG1
// (tools/interior_export.py): блоб комнаты (текстуры + куски в формате города) читается одним чтением при входе и
// освобождается при выходе. В комнате город не рисуется и не подгружается (клетки остаются в памяти: выход без дыр),
// неба и тумана нет; фон и свет — из данных комнаты (цвет вершин стен; LightGroup — Гомеру, skinLights). Нет пакета
// рисования, битый блоб или нет памяти — дверь не открывается, Гомер остаётся на улице
#define ROOM_NEAR .5f     // камера пешком в комнате бывает в 1.2 м от Гомера: ближняя плоскость города (1.5 м) резала бы его
// 16.10 (обсерватория L3/L6: в окнах и в куполе пустота): фон за окнами int_obv_background (панели на 40–120 м от комнаты, x 70…230 z 256…308) целиком за плоскостью 60 м.
// Комнаты до 30 м в поперечнике, но кадр за окном — это сцена за стенами: дальняя плоскость 300 м (счёт треугольников тот же, фон клеится видеочипом только в окнах;
// точность 16-битной глубины z²/(near·2^16) от far почти не зависит). -DROOM_FAR_OLD=1 — прежние 60 м
#ifndef ROOM_FAR_OLD
#define ROOM_FAR_OLD 0
#endif
#define ROOM_FAR (ROOM_FAR_OLD ? 60.f : 300.f)
static int roomPoint96(float x, float y, float z);
static Rooms rooms; static RoomState rs = {-1, -1, 1, 0, 1, 0}; static char roomsMsg[96]; static int roomN;   // rs: правка 41 — вход/выход по △ (needAction; смоук doorwalk — прежнее)
typedef struct { char name[32]; unsigned off, size; } RoomGfxEnt;
static RoomGfxEnt rgEnt[ROOMS_MAX]; static int rgN, rgBad[ROOMS_MAX];   // таблица SHG1; rgBad — блоб комнаты битый, больше не пробуем
static RoomCam rcam[ROOM_CAMS]; static int nrcam, roomCamOn, roomCamStickyOld, roomCamSwitchOld; static RoomCamState rcs = {-1}; static float roomFovY;   // камеры комнаты из данных (viewer/roomcam.h)
static struct { int room; unsigned char *mem; Piece *pc; unsigned npc, size, clear, amb, heap0, heap1; int nlit; float lit[4][3], ms; unsigned litc[4]; } rg = {-1};
static const char *roomName(void) { return roomsName(&rooms, &rs); }   // текущая комната для миссий (GameMsn.interior); NULL — город
static int inRoom(void) { return rg.mem && rs.room >= 0; }
// правка 181: кадр ворот подгрузки (viewer/zonegate.h). Игрок — машина или Гомер пешком; запас 1,5 м — кузов касается объёма
// раньше центра (близнец: чтение файла зоны начинается за 6–10 кадров до въезда центра машины)
// №733 (видео IMG_7280 9:43–10:33): выход из комнаты с переносом (тур: repair/goto, у игрока — миссия, будка) оставлял регион ворот комнаты —
// отсечение резало город до следующего «прыжка». После комнаты место сравнивается с последним до неё. ZONE_ROOM_JUMP_OLD=1 — прежнее
#ifndef ZONE_ROOM_JUMP_OLD
#define ZONE_ROOM_JUMP_OLD 0
#endif
#ifndef ZONE_STAND_ENTRY_OLD
#define ZONE_STAND_ENTRY_OLD 0
#endif
#ifndef ZONE_FOOT_EXACT_OLD
#define ZONE_FOOT_EXACT_OLD 0
#endif
// сверка g11b-collect круг 5 (P12-10, P12-16): старт, рестарт и выбор миссии ставят набор зон из команды миссии SetDynaLoadData (строка файлов
// «l3z1.p3d;l3r1.p3d;...»), а не ближайшего места телепорта. Строку отдаёт msn_view (mvDynaFiles), имена регионов уровня лежат в REG1 (regNm).
// Если место старта вне набора миссии (данные не сошлись), остаётся набор места. Перенос героя в течение 2 с после команды не перебивает набор
// миссии (игрока ставят на старт чуть позже команды). ZONE_MSN_SET_OLD=1 — как было: набор всегда по ближайшему месту
#ifndef ZONE_MSN_SET_OLD
#define ZONE_MSN_SET_OLD 0
#endif
// №1215 (Роб 05.11: «в тестах Апу появляется посреди пустоты, где ездят машины», L5): набор зон миссии принимался, если хоть один его регион есть
// в клетке игрока; у старта этапа «Get into your vehicle» (−155, 29, −482) клетка карты регионов — 3, 7, 8, набор этапа 0062 (2, 6, 7) — совпал один
// регион 7, и регионы 3 и 8 под ногами выключались: из 61 клетки вокруг рисовались 8, машины трафика (своя подгрузка) ездили по пустоте.
// Теперь к набору миссии добавляется набор места, где стоит игрок (тот, что игра ставит при переносе сюда): город под ногами не выключается. Все регионы клетки карты
// (regAt) брать нельзя: эмулятор, разговоры L5 — куча до 1,85 МБ, кусок 1,06 МБ. ZONEUNDER1215_OLD=1 — как было
#ifndef ZONEUNDER1215_OLD
#define ZONEUNDER1215_OLD 0
#endif
static unsigned zgMsnSeq, zgMsnMask; static int zgMsnFr;
static unsigned zoneMsnMask(const char *fl) {
  unsigned m = 0;
  for (const char *a = fl; *a;) {
    const char *e = a; while (*e && *e != ';' && *e != ':' && *e != '@') e++;
    char nm[24]; size_t n = (size_t)(e - a); if (n >= sizeof nm) n = sizeof nm - 1;
    memcpy(nm, a, n); nm[n] = 0;
    for (size_t k = 0; nm[k]; k++) if (nm[k] >= 'A' && nm[k] <= 'Z') nm[k] = (char)(nm[k] + 32);
    if (n > 4 && !strcmp(nm + n - 4, ".p3d")) nm[n - 4] = 0;
    if (*e == ';') for (int r = 0; r < regNn; r++) if (!strncmp(nm, regNm[r], 16) && nm[0]) m |= 1u << r;
    a = *e ? e + 1 : e;
  }
  return m;
}
static int zgRoomGap;
static void zoneGatesFrame(void) {
  if (!zgOn || !haveCol) return;
  if (inRoom()) { zgInit = 0; zgRoomGap = 1; return; }   // в комнате набор не меняется (вход — своя логика комнат по △, правило порта)
  int foot = onFoot(); float px = foot ? homer.x : car.x, py = foot ? homer.y : car.y, pz = foot ? homer.z : car.z;
  float dx = px - zgPx, dz = pz - zgPz; int jump = (zgInit || (zgRoomGap && !ZONE_ROOM_JUMP_OLD)) && dx * dx + dz * dz > 50.f * 50.f;
  zgRoomGap = 0;
  if (zgMsnFr > 0) zgMsnFr--;
  if (!ZONE_MSN_SET_OLD && regNn && !zgStart) {   // круг 5 (P12-10): команда миссии выполнилась (старт, рестарт, этап) — её набор зон
    unsigned sq; const char *fl = mvDynaFiles(&sq);
    if (sq != zgMsnSeq) {
      zgMsnSeq = sq; unsigned m = fl[0] ? zoneMsnMask(fl) : 0;
      if (m && (regAt(px, pz) & m)) { zgMsnMask = m; zgMsnFr = 120; regRes = m | (ZONEUNDER1215_OLD ? 0u : zonePlaceNear(zp, nzp, px, pz, 0xFFFFu)); zgInit = 0; }
      else zgMsnMask = 0;
      if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "ZONE msn \"%.60s\" mask %04x ok %d t=%.1f\n", fl, m, zgMsnMask != 0, smkT); smkWrite(s_); }
    }
  }
  if (zgStart || jump) {   // старт уровня или перенос: набор ближайшего места (оригинал при телепорте ставит набор места)
    unsigned b = regRes; regRes = zonePlaceNear(zp, nzp, px, pz, 0xFFFFu); zgStart = 0; zgInit = 0;
    if (!ZONE_MSN_SET_OLD && zgMsnFr > 0 && zgMsnMask && (regAt(px, pz) & zgMsnMask)) regRes = zgMsnMask | (ZONEUNDER1215_OLD ? 0u : regRes);   // №1215: regRes здесь — набор места (строка выше)   // круг 5 (P12-10): сразу после команды миссии её набор главнее места
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "ZONE %s res %04x -> %04x at %.0f %.0f t=%.1f\n", jump ? "jump" : "start", b, regRes, px, pz, smkT); smkWrite(s); }
  }
  if (!zgInit) {   // въезды считаются с этого места: объёмы, в которых игрок уже стоит, не срабатывают
    // сверка g11b, P12-16 / Д4 (СВЕРКА_ПОЛНАЯ/P12_загрузка_и_зоны.md): после старта, переноса героя и сброса объём, в котором игрок стоит, срабатывает на первом кадре, как вход (ZONE_STAND_ENTRY_OLD=1 — как TEST81)
    for (int i = 0; i < nzg; i++) zgWas[i] = ZONE_STAND_ENTRY_OLD ? (unsigned char)zoneGateIn(&zg[i], px, py, pz, 1.5f) : 0;
    zgInit = 1; zgPx = px; zgPz = pz; return;
  }
  int fired = -1; unsigned b = regRes;
  regRes = zoneGatesStep(zg, nzg, zgWas, regRes, px, py, pz, (foot && !ZONE_FOOT_EXACT_OLD) ? 0.f : 1.5f, &fired);   // сверка g11b P12-05: Гомер пешком — точка строго внутри объёма, запаса нет; у машины запас 1,5 м заменяет четыре угла кузова (нужна проверка по точкам в zonegate.h, передано)
  zgPx = px; zgPz = pz;
  if (regRes != b) { zgFired++;
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "ZONE gate %d res %04x -> %04x at %.0f %.0f t=%.1f\n", fired, b, regRes, px, pz, smkT); smkWrite(s); } }
}
// Правка Роба (запись PS2 ps2_m0_kwik_exit, ответ В77, 10.10): выход из интерьера ставит Гомера ровно в точку дверных ворот зоны — у Kwik-E-Mart
// (213,905; 4,93; 300,918 осей порта), ворота KwikEMart (213,91; 5,16; 300,92) загружают l1i01. Порт ставил его в локатор двери (213,905; 5,157;
// 302,744) — на 1,83 м южнее. ПРАВИЛО (общее для всех комнат): после выхода Гомер встаёт в центр ближайших к двери ворот подгрузки (ZoneGate) в
// пределах ROOM_GATE_NEAR; нет таких ворот — как было (локатор двери). ROOM_EXIT_GATE_OLD=1 — прежнее
#ifndef ROOM_EXIT_GATE_OLD
#define ROOM_EXIT_GATE_OLD 0
#endif
#define ROOM_GATE_NEAR 3.f
static void roomExitAtGate(void) {
  if (ROOM_EXIT_GATE_OLD || !haveHomer || !zg || nzg <= 0) return;
  int best = -1; float bd = ROOM_GATE_NEAR * ROOM_GATE_NEAR;
  for (int i = 0; i < nzg; i++) {
    float dx = zg[i].c[0] - homer.x, dz = zg[i].c[2] - homer.z, d = dx * dx + dz * dz;
    if (d < bd && fabsf(zg[i].c[1] - homer.y) < 3.f) { bd = d; best = i; }
  }
  if (best < 0) return;
  float h = homer.h; homerPlace(&homer, &world, zg[best].c[0], homer.y, zg[best].c[2], h);
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "ROOM exit-gate %d at=%.3f,%.3f,%.3f t=%.1f\n", best, homer.x, homer.y, homer.z, smkT); smkWrite(s); }
}
static unsigned rdU(const unsigned char *p) { unsigned v; memcpy(&v, p, 4); return v; }
static void roomsLoad(void) {   // один раз после уровня: SHI1 целиком (коллизия ~20 КБ, живёт до выхода) и таблица SHG1
  char fn[256]; levelFile(fn, "interiors-l%d.bin");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f < 0) { snprintf(roomsMsg, sizeof roomsMsg, "ROOMS pkg=none\n"); return; }
  int n = sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  void *b = n > 0 ? memalign(16, n) : 0; int e = -1;
  if (b && sceIoRead(f, b, n) == n) e = roomsParse(&rooms, b, (size_t)n);
  sceIoClose(f);
  if (e) { free(b); snprintf(roomsMsg, sizeof roomsMsg, "ROOMS pkg=bad err=%d\n", e); return; }
  levelFile(fn, "interiors-l%d-gfx.bin"); f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f >= 0) {
    static unsigned char h[16 + 48 * ROOMS_MAX]; int gn = sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
    int r = sceIoRead(f, h, sizeof h); unsigned k = r >= 16 ? rdU(h + 4) : 0;
    if (r >= 16 && !memcmp(h, "SHG1", 4) && rdU(h + 8) == 1 && k <= ROOMS_MAX && r >= (int)(16 + 48 * k))
      for (unsigned i = 0; i < k; i++) {   // всё или ничего: одна кривая запись — рисования нет ни у одной комнаты
        RoomGfxEnt *g = &rgEnt[i]; memcpy(g->name, h + 16 + 48 * i, 32); g->name[31] = 0;
        g->off = rdU(h + 16 + 48 * i + 32); g->size = rdU(h + 16 + 48 * i + 36);
        if ((g->off & 15) || g->size < 128 || g->off > (unsigned)gn || g->size > (unsigned)gn - g->off) { rgN = 0; break; }
        rgN = i + 1;
      }
    sceIoClose(f);
  }
  snprintf(roomsMsg, sizeof roomsMsg, "ROOMS pkg=ok rooms=%d doors=%d gfx=%d\n", rooms.nroom, rooms.ndoor, rgN);
  homerRoomPointHook = roomPoint96;   // Build 96: комнаты загружены — старт миссии в доме идёт видом roomplace, не hold
}
// сверка g11b-collect круг 5 (P16-11): анимация текстур комнаты (экран автомата Kwik-E-Mart, помехи телевизора Moe's, огни обсерватории) — блок ANI1 в хвосте
// блоба комнаты (tools/interior_export.py, подвал RAN1 в последних 16 байтах). Текстура-основа заменяется по часам: кадр = время x кадров/с по кругу длины записи,
// между ключами кадр держится. Блока нет (старый пакет) — комната статична. ROOM_ANIM_OLD=1 — не читать блок
#ifndef ROOM_ANIM_OLD
#define ROOM_ANIM_OLD 0
#endif
#define ROOM_ANIMS 4
#define ROOM_ANIM_KEYS 16
typedef struct { int base; int nk; float len, fps; unsigned short fr[ROOM_ANIM_KEYS], tx[ROOM_ANIM_KEYS]; } RoomAnim;
static RoomAnim roomAn[ROOM_ANIMS]; static int roomAnN; static unsigned roomAnT0; static int roomAnLog95;   // служебно: журнал смен ключа анимации текстур комнаты
static void roomAnimParse(const unsigned char *m, unsigned sz, unsigned ntx) {
  roomAnN = 0; if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "ROOMANIM parse sz=%u tail=%.4s\n", sz, sz >= 16 ? (const char *)(m + sz - 16) : "----"); smkWrite(l_); }
  if (ROOM_ANIM_OLD || sz < 16 + 128 || memcmp(m + sz - 16, "RAN1", 4)) return;
  unsigned off = rdU(m + sz - 12), n = rdU(m + sz - 8);
  if (off + 8 > sz - 16 || n > ROOM_ANIMS || memcmp(m + off, "ANI1", 4)) return;
  const unsigned char *q = m + off + 8; const unsigned char *end = m + sz - 16;
  for (unsigned i = 0; i < n; i++) {
    if (q + 16 > end) { roomAnN = 0; return; }
    RoomAnim *a = &roomAn[roomAnN]; unsigned nk = rdU(q + 4); float f2[2]; memcpy(f2, q + 8, 8);
    if (rdU(q) >= ntx || nk == 0 || nk > ROOM_ANIM_KEYS || q + 16 + 4 * nk > end || !(f2[0] > 0.f) || !(f2[1] > 0.f)) { roomAnN = 0; return; }
    a->base = (int)rdU(q); a->nk = (int)nk; a->len = f2[0]; a->fps = f2[1]; q += 16;
    for (unsigned k = 0; k < nk; k++, q += 4) { a->fr[k] = (unsigned short)(q[0] | q[1] << 8); a->tx[k] = (unsigned short)(q[2] | q[3] << 8); if (a->tx[k] >= ntx) { roomAnN = 0; return; } }
    roomAnN++;
  }
  roomAnT0 = gtUs95();
  if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "ROOMANIM parsed n=%d\n", roomAnN); smkWrite(l_); }
}
// номер текстуры куска комнаты с учётом анимации
static int roomAnimTex(int t) {
  if (!roomAnN || t < RTEX0) return t;
  float sec = (float)(gtUs95() - roomAnT0) * 1e-6f;
  if (smkOn && roomAnLog95) { static unsigned calls_, last_; static int lastSec_ = -1; calls_++; if ((int)sec != lastSec_) { lastSec_ = (int)sec; char l_[200]; snprintf(l_, sizeof l_, "ROOMANIM stat sec=%.2f smkT=%.2f lag=%.2f gt=%d calls=%u (+%u) n=%d\n", sec, smkT, smkT - sec, gtOn95, calls_, calls_ - last_, roomAnN); smkWrite(l_); last_ = calls_; } }
  for (int i = 0; i < roomAnN; i++) { const RoomAnim *a = &roomAn[i];
    if (t != RTEX0 + a->base) continue;
    float f = fmodf(sec * a->fps, a->len); int k = 0; for (int j = 0; j < a->nk; j++) if (a->fr[j] <= f) k = j;
    if (smkOn && roomAnLog95) { static int lastK[ROOM_ANIMS] = {-1, -1, -1, -1}; static unsigned lastT[ROOM_ANIMS]; if (lastK[i] != k) { unsigned now_ = sceKernelGetSystemTimeLow(); char l_[120]; snprintf(l_, sizeof l_, "ROOMANIM t=%.3f anim=%d key=%d frame=%.2f tex=%d dt_ms=%.1f\n", smkT, i, k, f, a->tx[k], lastT[i] ? (now_ - lastT[i]) / 1000.f : 0.f); smkWrite(l_); lastK[i] = k; lastT[i] = now_; } }   // №1233/Kwik L4 служебно: смены ключа и интервал, flag roomanimlog 1
    return RTEX0 + a->tx[k]; }
  return t;
}
// ТЕСТ90 П1 (ROOMIO_OLD / SYNCREAD_P1_OLD / файл syncreadp1old): блоб комнаты читается фоном заранее, когда Гомер пешком подошёл к двери ближе ROOM_PF_DIST;
// roomOpen под закрытым ирисом берёт готовый буфер (не готов — ждёт его, не больше 0,4 с; нет буфера — прежнее синхронное чтение). Кучи мало — не читает впрок
#ifndef ROOMIO_OLD
#define ROOMIO_OLD 0
#endif
#ifndef SYNCREAD_P1_OLD
#define SYNCREAD_P1_OLD 0
#endif
static int syncReadP1Old = SYNCREAD_P1_OLD;   // и для гэгов (main_collect.h); файл syncreadp1old / flag syncreadp1old 1
#ifndef ROOMIO_ON_DEFAULT
#define ROOMIO_ON_DEFAULT 1   // 0 — предчтение комнат по умолчанию выключено, включается файлом roomiop1on рядом с EBOOT (пара для тура); гэги от этого не зависят
#endif
static int roomPfOn = ROOMIO_ON_DEFAULT && !ROOMIO_OLD;   // файл roomiop1on / flag roomiop1on 1 включает, syncreadp1old выключает
#define ROOM_PF_DIST 14.f       // с какого расстояния до двери начинать читать комнату
#define ROOM_PF_KEEP 25.f       // дальше этого от двери буфер отпускается
#define ROOM_PF_READ_US 3000000u  // чтение впрок не пришло за 3 с — отмена, комната откроется синхронно под ирисом
#define ROOM_PF_IDLE_US 60000000u // комната прочитана, но Гомер не вошёл за 60 с — буфер отпускается
#define ROOM_PF_HEAP_MIN (3u << 20)  // впрок только при куче не меньше размер + 3 МБ; держим — отпускаем при куче < 1,5 МБ
static struct { int room, state; unsigned char *m; unsigned sz; SceUID fd; unsigned t0; } rpf = { -1, 0, 0, 0, -1, 0 };   // state: 0 нет, 1 читается, 2 готово
unsigned roomPfStarts, roomPfHits, roomPfWaits, roomPfDrops, roomPfTimeouts; static int rpfFail = -1;
static void roomPrefetchDrop(void) {
  if (rpf.state == 1) { SceInt64 r_; sceIoCancel(rpf.fd); sceIoWaitAsync(rpf.fd, &r_); }   // отмена, потом конец: буфер освобождается только после неё
  if (rpf.fd >= 0) sceIoClose(rpf.fd);
  free(rpf.m); rpf.m = 0; rpf.fd = -1; rpf.state = 0; rpf.room = -1; rpf.sz = 0;
}
static void rdlFree(void);   // листы комнат (РОВНЫЕ_30 шаг 2): готовые листы GE комнаты (ниже), освобождаются вместе с комнатой
static void roomClose1(void);
static void roomClose(void) { unsigned t0_ = sceKernelGetSystemTimeLow(); roomClose1(); xnEnd("room_out", t0_, (int)gameLevel); }   // №1206: время перехода
static void roomClose1(void) {   // блоб комнаты — обратно в кучу (GE прошлый кадр закончил: sceGuSync в конце кадра)
  rpfFail = -1; roomPrefetchDrop();   // ТЕСТ90 П1: незаконченное чтение впрок не переживает комнату и уровень
  if (rg.mem) bbOp("room-", rg.room >= 0 && rg.room < rooms.nroom ? rooms.room[rg.room].name : "-", 0);   // правка 103: самописец
  rdlFree();
  toneBlkDel(rg.mem); free(rg.pc); free(rgTex); free(rg.mem); nrcam = 0; rcs.cur = -1; roomCamOn = 0;
  roomAnN = 0;   // круг 5 (P16-11)
  rg.pc = 0; rgTex = 0; rgNtex = 0; rg.mem = 0; rg.npc = 0; rg.room = -1; bound = -2;
}
static void roomLeave(void) { roomClose(); roomStateInit(&rs); G.pHomerWorld = 0; e1002N = 14; }   // RESET CAR / NEW GAME: Гомер снова в городе
// canEnter для roomsStep: прочитать и разобрать блоб комнаты. 0 — не открыть (Гомер остаётся на улице)
// №1076: экран загрузки и тёмный экран двери ждут персонажей (main_frontend.h loadEnd, roomOpen ниже). NPC_WAIT1076_OLD=1 / flag npcwaitold 1 — как до правки
#ifndef NPC_WAIT1076_OLD
#define NPC_WAIT1076_OLD 0
#endif
static int npcWaitOld;
// №1209 (тур TEST90 на PSP): «foot X Z H Y» с Y под городом, когда Гомер ещё не в комнате, ставил его в город под землю — падение без конца. Теперь пропуск.
// FOOTROOM1209_OLD=1 / flag footroom1209old 1 — как было
#ifndef FOOTROOM1209_OLD
#define FOOTROOM1209_OLD 0
#endif
static int footRoom1209Old;
// ag/loadorder: пропуск foot по Y ниже −10 срабатывал и на низинах L3/L6 (город там лежит на −20…−77 м): точка вне комнаты тоже терялась без следа. Теперь пропуск — только если точка лежит на полу какой-нибудь комнаты
// (npcRoomAt). FOOTLOWLAND1209_OLD=1 / «flag footlowland1209old» — как в №1209 (любая точка ниже −10 м пропускается)
#ifndef FOOTLOWLAND1209_OLD
#define FOOTLOWLAND1209_OLD 0
#endif
static int footLowland1209Old = FOOTLOWLAND1209_OLD;
static int roomOpen1(void *u, int room);
// Будка-93 / правка Роба 1226 («вход — предсказуемая сцена»): раньше комната читалась (до 0,2 с на Маке, ×5 на PSP) при ещё стоящем кадре, и ирис схлопывался уже после — рывок до ириса.
// Теперь вход как у будки №1051/№1060: ирис закрывается первым (кадры ириса не ждут чтения), чтение комнаты, предметов и персонажей идёт под закрытым экраном, ирис раскрывается, когда готово
// (крючок держит его ещё, пока гэги комнаты дочитываются — main_state.h spawnHoldHook, причина «gag»). Не открылась — ирис снимается, Гомер остаётся на улице. DOORIRIS1226_OLD=1 / файл dooriris1226old / flag dooriris1226old 1 — как было
#ifndef DOORIRIS1226_OLD
#define DOORIRIS1226_OLD 0
#endif
static int doorIris1226Old = DOORIRIS1226_OLD;
static void **fbpG1226;          // кадровый буфер главного цикла (ставит main_loop.h)
static int door1226Gate;         // 1 — roomOpen зовёт roomsStep главного цикла (вход по двери); служебные вызовы ирис не трогают
static int irisPre1226;          // ирис «room_in» уже закрыт roomOpen: главный цикл второй раз его не закрывает
static int irisPre1226Frames;    // кадров развёртки, показанных этим закрытием (для счётчика кадров цикла)
static int roomOpenLegacy(void *u, int room) {   // прежний путь (TFSM95_OLD=1): ирис закрывается первым, потом roomOpen1 с лестницей
  { unsigned sz_ = 0; for (int i = 0; i < rgN; i++) if (!strcmp(rgEnt[i].name, rooms.room[room].name)) sz_ = rgEnt[i].size;
    transitPlan("room_in", sz_ + (64u << 10), rooms.room[room].start); }   // Будка-93 (1233): план до закрытия экрана
  if (door1226Gate && !doorIris1226Old && fbpG1226 && transitReady() && !transitBusy()) {
    unsigned t1_ = sceKernelGetSystemTimeLow();
    irisPre1226Frames = transitClose(fbpG1226, "room_in"); irisPre1226 = 1;
    if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "DOORIRIS1226 pre-close room=%s ms=%u t=%.1f\n", rooms.room[room].name, (sceKernelGetSystemTimeLow() - t1_) / 1000u, smkT); smkWrite(s_); }
  }
  unsigned t0_ = sceKernelGetSystemTimeLow(); int r_ = roomOpen1(u, room); xnEnd("room_in", t0_, room);
  if (!r_ && irisPre1226) { transitDrop(); irisPre1226 = 0; irisPre1226Frames = 0; if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "DOORIRIS1226 drop t=%.1f\n", smkT); smkWrite(s_); } }   // не открылась — не оставлять закрытый экран
  return r_;
}   // №1206: время перехода
// Build 95 П2 (transit_fsm.h): класс ROOM_IN идёт через ldrRun (PLAN, CLOSE, FREE, LOAD, FREE_MORE, SIMPLIFY, OPEN). Экран AUTO: ирис, если блок под комнату есть и ирис доступен; иначе прелоадер Itchy & Scratchy
// (решение Роба 2: комната не открылась, пересборки уровня нет: запомнить место, выбросить лишнее под прелоадером, грузить только комнату). Чтение комнаты roomOpen1 то же. TFSM95_OLD=1 — roomOpenLegacy
typedef struct { void *u; int room; } RoomLoadCtx;
static void levelCleanReload96(void **fbp, unsigned t0);   // mem1060.h
static int transitReload96(void *c, void **fbp) {   // Build 96: чистая перезагрузка перед SIMPLIFY — только вне миссии и вне комнаты (внутри миссии снимок не доказан)
  (void)c; if (mvInMission() || inRoom()) { if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "CLEANRELOAD96 skip why=%s t=%.1f\n", mvInMission() ? "mission" : "room", smkT); smkWrite(s_); } return 0; }
  levelCleanReload96(fbp, sceKernelGetSystemTimeLow());
  { int r0_ = uiScrReady(); if (r0_) uiScrClose(); if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "CLEANRELOAD96 scr ready %d->%d t=%.1f\n", r0_, uiScrReady(), smkT); smkWrite(s_); } }   // пакет экрана перезагрузки (газета/LoadingFE) закрыт: его держит только переход, а лавка и гардероб открывают свой и отказывают при открытом чужом
  if (!booth1225Old) camHold1225 = BOOTH_CAMHOLD1225_FRAMES;   // камера держится, наезда старта нет
  return 1;   // levelCleanReload96 доводит уровень до игры; отказ чтения уровня сам по себе её не останавливает (loadedLevel проверяет вызывающий)
}
static int transitReloadOverlay96(void *c, void **fbp) { return transitReload96(c, fbp) ? 2 : 0; }   // лавка и гардероб: слот экрана после перезагрузки свободен для их пакета
static int roomLoadCb(void *c) { RoomLoadCtx *x = (RoomLoadCtx *)c; return roomOpen1(x->u, x->room); }
static int roomOpen(void *u, int room) {
  if (tfsm95Old) return roomOpenLegacy(u, room);
  unsigned sz_ = 0; for (int i = 0; i < rgN; i++) if (!strcmp(rgEnt[i].name, rooms.room[room].name)) sz_ = rgEnt[i].size;
  RoomLoadCtx cx = {u, room}; Transit t; memset(&t, 0, sizeof t);
  t.name = "room_in"; t.screen = (door1226Gate && !doorIris1226Old) ? TFS_AUTO : TFS_NONE; t.need = sz_ + (64u << 10);
  t.at[0] = rooms.room[room].start[0]; t.at[1] = rooms.room[room].start[1]; t.at[2] = rooms.room[room].start[2];
  t.load = roomLoadCb; t.cleanReload = transitReload96; t.u = &cx; t.freeAllOnPreloader = 1; t.noReturn = 1; t.simplifyText = "PLEASE WAIT.";   // ближайшая строка библии оригинала (Данные-93)
  unsigned t0_ = sceKernelGetSystemTimeLow(); int ok = ldrRun(&t, fbpG1226); xnEnd("room_in", t0_, room);
  irisPre1226 = tfIrisClosed; irisPre1226Frames = tfIrisFrames;
  if (!ok && irisPre1226) { transitDrop(); irisPre1226 = 0; irisPre1226Frames = 0; if (smkOn) { char s_[80]; snprintf(s_, sizeof s_, "DOORIRIS1226 drop t=%.1f\n", smkT); smkWrite(s_); } }
  return ok;
}
static int roomOpen1(void *u, int room) {
  (void)u; bbOp("room+", rooms.room[room].name, 1);   // правка 103: самописец — ящик на карте ДО чтения комнаты
  unsigned t0 = sceKernelGetSystemTimeLow(), h0 = smkOn ? heapFreeBytes() : 0;
  const RoomGfxEnt *g = 0;
  for (int i = 0; i < rgN; i++) if (!strcmp(rgEnt[i].name, rooms.room[room].name)) g = &rgEnt[i];
  int err = !g ? 1 : rgBad[room] ? 2 : 0; unsigned char *m = 0; Tex *tx = 0; Piece *pp = 0;
  unsigned sz = g ? g->size : 0, ntx = 0, np = 0, to = 0, ro = 0;
  int fromPf = 0;
  if (zone3OnX() && smkOn) { char z_[460]; zone3StatLineX(z_, sizeof z_, smkT, "room_in"); smkWrite(z_); }   // П5 Зоны-95: свободное в областях перед входом
  if (!err && rpf.room == room && rpf.m && rpf.sz == sz && roomPfOn && !syncReadP1Old) {   // ТЕСТ90 П1: прочитано впрок (или читается — ждём здесь, под ирисом)
    unsigned w0 = sceKernelGetSystemTimeLow();
    while (rpf.state == 1 && sceKernelGetSystemTimeLow() - w0 < 400000u) {
      SceInt64 r_ = -1; int pr = sceIoPollAsync(rpf.fd, &r_);
      if (pr <= 0) { rpf.state = pr == 0 && r_ == (SceInt64)sz ? 2 : 0; if (pr < 0 || r_ != (SceInt64)sz) { free(rpf.m); rpf.m = 0; } break; }
      sceKernelDelayThread(2000); roomPfWaits++;
    }
    if (rpf.state == 1) { SceInt64 r2_; sceIoCancel(rpf.fd); sceIoWaitAsync(rpf.fd, &r2_); free(rpf.m); rpf.m = 0; }   // не успело за 0,4 с — отменить и читать по-старому
    if (rpf.state == 2 && rpf.m) { m = rpf.m; rpf.m = 0; fromPf = 1; roomPfHits++; }
    if (rpf.fd >= 0) { sceIoClose(rpf.fd); rpf.fd = -1; }
    rpf.state = 0; rpf.room = -1; rpf.sz = 0;
  }
  // TEST93 (Роб 07.11 вечер: «не могу зайти ни в дом, ни в Kwik-E-Mart» на L4 и дальше, на L1 можно): блоб комнаты брался мимо лестницы памяти (канон п.13),
  // а лестница с 435 держит отложенный блок 448 КБ под меню — на уровнях 2–7 цельного куска под комнату не хватало, вход молча отменялся (err 3).
  // Теперь при отказе: отложенный блок — в кучу (ляжет снова на ближайшем тике), потом ступени лестницы, потом повтор. ROOMMEM93_OLD=1 — как было
#ifndef ROOMMEM93_OLD
#define ROOMMEM93_OLD 0
#endif
#ifndef ROOMMEM93_CELLS_OLD
#define ROOMMEM93_CELLS_OLD 0   // 1 — без ступени «выгрузить все клетки города под закрытым ирисом» (бисекция обрыва после выхода из DMV, Опора-93)
#endif
  if (!err && !m && !(m = zone3AllocX(Z3A_PLACE, "room", sz, 16)) && !ROOMMEM93_OLD && memLadOn) {   // П5 Зоны-95: блоб комнаты — область «место»
    unsigned b0_ = memMaxBlock ? memMaxBlock() : 0, f0_ = heapFreeBytes();
    memLadResTake(); m = zone3AllocX(Z3A_PLACE, "room", sz, 16);
    if (!m && memLadderZ(LADZ_PLACE, "room", sz + (64u << 10), LAD_TOP)) m = zone3AllocX(Z3A_PLACE, "room", sz, 16);
    // куча раздроблена (свободно мегабайты, цельного куска нет): под закрытым ирисом двери город не виден — выгрузить клетки города, как ступень 4
    // при стоящем мире; при выходе их вернёт стриминг, а экран выхода держит Гомера, пока не будет пола (A1). Оригинал тоже грузит комнату отдельно от города
    if (!m && !ROOMMEM93_CELLS_OLD) { memLadWaitLoader(); unsigned f1_ = heapFreeBytes(); int n_ = cellsDropBeyond(-1.f); memLadQPush("cells", heapFreeBytes() > f1_ ? heapFreeBytes() - f1_ : 0); m = zone3AllocX(Z3A_PLACE, "room", sz, 16);
      memLadLog("WATCHDOG", "room", sz, 4, b0_, f0_, m ? "cells-ok" : "cells-none"); (void)n_; }
    memLadLog("WATCHDOG", "room", sz, m ? 0 : 5, b0_, f0_, m ? "ok" : "fail");
  }
  if (!err && !m) err = 3;   // нет памяти — не «битый»: отойдёт от двери и вернётся — новая попытка
  if (!err && !fromPf) {
    char fn[256]; levelFile(fn, "interiors-l%d-gfx.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    DCPRE96(m, sz);
    if (f < 0 || sceIoLseek32(f, g->off, PSP_SEEK_SET) != (int)g->off || sceIoRead(f, m, sz) != (int)sz) err = 4;
    if (f >= 0) sceIoClose(f);
  }
  if (!err) {
    ntx = rdU(m + 4); np = rdU(m + 8); to = rdU(m + 12); ro = rdU(m + 16);
    if (memcmp(m, "RMG1", 4) || ntx > 512 || np > 4096 || rdU(m + 28) > 4 || to < 128 || ro < to || ro > sz || np > (sz - ro) / 80) err = 5;
  }
  if (!err && !((tx = calloc(ntx ? ntx : 1, sizeof(Tex))) && (pp = calloc(np ? np : 1, sizeof(Piece))))) err = 3;
  size_t p = to;
  for (unsigned i = 0; !err && i < ntx; i++) {   // текстуры — как у города (SHR7): заголовок 16 Б, палитра 1 КБ, уровни
    if (p + 16 + 1024 > ro) { err = 6; break; }
    unsigned short *t2 = (unsigned short *)(m + p); Tex *t = &tx[i];
    t->w = t2[0]; t->h = t2[1]; t->levels = t2[2]; t->pre = m[p + 7] == 1; t->t4 = m[p + 6] == 4; t->pal = m + p + 16; palTone(m + p + 16); p += 16 + 1024;
    if ((t->t4 && !t->pre) || t->w < 1 || t->w > 512 || t->h < 1 || t->h > 512 || t->levels < 1 || t->levels > 8) { err = 6; break; }
    int lw = t->w, lh = t->h;
    for (int l = 0; l < t->levels; l++) {
      t->lv[l] = m + p; if (t->pre) t->slv[l] = m + p;
      p += (size_t)texRowBytes(t->t4, t->pre, lw) * (t->pre ? (lh + 7) & ~7 : lh); p = (p + 15) & ~(size_t)15; lw >>= 1; lh = lh > 1 ? lh >> 1 : 1;
      if (p > ro) { err = 6; break; }
    }
  }
  for (unsigned i = 0; !err && i < np; i++) {   // куски — записи Rec города, вершины без индексов (флаг 16), дальней версии нет
    Piece *d = &pp[i]; memcpy(&d->r, m + ro + 80 * i, 80);
    unsigned vs = (d->r.flags & F_V12) ? 12 : 16;
    if (!(d->r.flags & F_NOIDX) || d->r.nf || d->r.ni <= 0 || d->r.ni % 3 || d->r.nv != d->r.ni || (d->r.off & 15) || d->r.off > sz ||
        (unsigned)d->r.ni > (sz - d->r.off) / vs || d->r.tex < -1 || d->r.tex >= (int)ntx) { err = 7; break; }
    d->r.tex = d->r.tex >= 0 ? RTEX0 + d->r.tex : -1; d->r.owner = 0; d->r.cell = 0;
    d->v = (Vert *)(m + d->r.off); d->idx = d->fidx = 0;
  }
  if (!err && (nrcam = roomCamsParse(rcam, ROOM_CAMS, m, sz)) < 0) { nrcam = 0; err = 8; }   // таблица камер битая — пакет битый
  if (err) {
    free(pp); free(tx); free(m); if (err != 3) rgBad[room] = 1;
    char s[96]; snprintf(s, sizeof s, "ROOM ev=fail room=%s err=%d t=%.1f\n", rooms.room[room].name, err, smkT); smkWrite(s);
    return 0;
  }
  sceKernelDcacheWritebackRange(m, sz);
  toneBlkAdd(m, sz, TF_ROOM, g->off);   // правка 144: палитры комнаты — из interiors-lN-gfx.bin заново
  rg.room = room; rg.mem = m; rg.pc = pp; rg.npc = np; rg.size = sz; rgTex = tx; rgNtex = ntx; bound = -2;
  roomAnimParse(m, sz, ntx);   // круг 5 (P16-11)
  if (smkOn && roomAnN) { for (int q_ = 0; q_ < roomAnN; q_++) { int cnt_ = 0, tmin_ = 1 << 30, tmax_ = -1; float bb_[6] = {0}; for (unsigned i_ = 0; i_ < rg.npc; i_++) { int tx_ = rg.pc[i_].r.tex; if (tx_ < tmin_) tmin_ = tx_; if (tx_ > tmax_) tmax_ = tx_; if (tx_ == RTEX0 + roomAn[q_].base) { if (!cnt_) memcpy(bb_, rg.pc[i_].r.bb, sizeof bb_); cnt_++; } }
      char l_[240]; snprintf(l_, sizeof l_, "ROOMANIM pieces anim=%d base=%d key_tex=[%d %d %d %d] used_by=%d pieces_tex_range=%d..%d npc=%u bb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f)\n", q_, roomAn[q_].base, roomAn[q_].tx[0], roomAn[q_].nk > 1 ? roomAn[q_].tx[1] : -1, roomAn[q_].nk > 2 ? roomAn[q_].tx[2] : -1, roomAn[q_].nk > 3 ? roomAn[q_].tx[3] : -1, cnt_, tmin_ - RTEX0, tmax_ - RTEX0, rg.npc, bb_[0], bb_[1], bb_[2], bb_[3], bb_[4], bb_[5]); smkWrite(l_);
      { char t_[300]; int o_ = snprintf(t_, sizeof t_, "ROOMANIM keytex anim=%d", q_); for (int k_ = 0; k_ < roomAn[q_].nk && k_ < 4; k_++) { int id_ = roomAn[q_].tx[k_]; const Tex *x_ = (id_ >= 0 && (unsigned)id_ < ntx) ? &tx[id_] : 0; o_ += snprintf(t_ + o_, sizeof t_ - (size_t)o_, " [%d key@%u %dx%d lv%d t4=%d pre=%d pal=%d]", id_, roomAn[q_].fr[k_], x_ ? x_->w : -1, x_ ? x_->h : -1, x_ ? x_->levels : -1, x_ ? x_->t4 : -1, x_ ? x_->pre : -1, x_ && x_->pal); } snprintf(t_ + o_, sizeof t_ - (size_t)o_, "\n"); smkWrite(t_); } } }
  rg.clear = rdU(m + 20); rg.amb = rdU(m + 24); rg.nlit = (int)rdU(m + 28); rcs.cur = -1; rcs.in = 0; memset(rcs.inT, 0, sizeof rcs.inT); if (!CUT1085_OLD) roomCamRestart(&rcs);   /* №1085 (сверка g06): вход в помещение — первая камера срезом */ rcs.newWins = !roomCamStickyOld; rcs.switchOld = roomCamSwitchOld;   // правка 186; файл roomcamswitchold — задержка 172 до школы
  for (int i = 0; i < rg.nlit; i++) { memcpy(rg.lit[i], m + 32 + 16 * i, 12); rg.litc[i] = rdU(m + 32 + 16 * i + 12); }
  // №1076: комната читается при закрытом экране (дверь) — персонажи миссии и бонусные у её старта (Дедушка в доме L1) дочитываются здесь же, а не на глазах
  // после открытия; не дольше 1,5 с (тёмный экран держится чуть дольше). Служебное flag npcwaitold 1 — как до правки
  if (!NPC_WAIT1076_OLD && !npcWaitOld) { unsigned w0 = sceKernelGetSystemTimeLow(); int n0 = mvNpcPending() + mvAmbMissingAt(rooms.room[room].start[0], rooms.room[room].start[1], rooms.room[room].start[2]), nl = n0;
    while (nl > 0 && sceKernelGetSystemTimeLow() - w0 < 1500000u) { sceKernelDelayThread(5000); nl = mvNpcPending() + mvAmbMissingAt(rooms.room[room].start[0], rooms.room[room].start[1], rooms.room[room].start[2]); }
    if (smkOn && n0) { char s_[96]; snprintf(s_, sizeof s_, "NPCWAIT1076 room=%s pending0=%d left=%d ms=%u\n", rooms.room[room].name, n0, nl, (sceKernelGetSystemTimeLow() - w0) / 1000u); smkWrite(s_); } }
  rg.ms = (sceKernelGetSystemTimeLow() - t0) / 1000.f; rg.heap0 = h0;
  if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "ROOMPF room=%s fromPf=%d starts=%u hits=%u waits=%u drops=%u timeouts=%u ms=%.0f t=%.1f\n", rooms.room[room].name, fromPf, roomPfStarts, roomPfHits, roomPfWaits, roomPfDrops, roomPfTimeouts, rg.ms, smkT); smkWrite(s_); } rg.heap1 = smkOn ? heapFreeBytes() : 0;
  return 1;
}
// ТЕСТ90 П1: каждый кадр на улице пешком — не пора ли читать комнату ближайшей двери. Не в комнате, не в переходе, куча с запасом
static void roomPrefetchTick(void) {
  if (!roomPfOn || syncReadP1Old || !rooms.nroom || !rgN || !haveHomer || !onFoot() || rs.room >= 0 || rg.mem) { if (rpf.state && (rs.room >= 0 || rg.mem)) roomPrefetchDrop(); return; }
  unsigned now = sceKernelGetSystemTimeLow();
  // держим буфер, пока Гомер не дальше ROOM_PF_KEEP от двери этой комнаты, не дольше ROOM_PF_IDLE_US и пока в куче есть запас; чтение в пути — не дольше ROOM_PF_READ_US
  if (rpf.state) {
    float kd = 1e9f; for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == rpf.room) { float dx = rooms.door[d].p[0] - homer.x, dz = rooms.door[d].p[2] - homer.z, q = dx * dx + dz * dz; if (q < kd) kd = q; }
    if (kd > ROOM_PF_KEEP * ROOM_PF_KEEP || (rpf.state == 2 && (now - rpf.t0 > ROOM_PF_IDLE_US || heapFreeBytes() < (3u << 19)))) {
      if (rpf.state == 2 && kd <= ROOM_PF_KEEP * ROOM_PF_KEEP) rpfFail = rpf.room;   // простой или куча: рядом с дверью заново не читать, пока не отошёл
      roomPfDrops++; roomPrefetchDrop(); return;
    }
    if (rpf.state == 1) {
      SceInt64 r_ = -1; int pr = sceIoPollAsync(rpf.fd, &r_);
      if (pr <= 0) { if (pr == 0 && r_ == (SceInt64)rpf.sz) { rpf.state = 2; rpf.t0 = now; } else { sceIoClose(rpf.fd); rpf.fd = -1; free(rpf.m); rpf.m = 0; rpf.state = 0; rpfFail = rpf.room; rpf.room = -1; } }
      else if (now - rpf.t0 > ROOM_PF_READ_US) { roomPfTimeouts++; rpfFail = rpf.room; roomPrefetchDrop(); }   // не пришло: отмена, дескриптор закрыт, комната — синхронно под ирисом
      return;
    }
    return;
  }
  if (rpfFail >= 0) {   // отказавшая комната: снова только после того, как Гомер отошёл дальше ROOM_PF_KEEP от её двери
    float kd = 1e9f; for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == rpfFail) { float dx = rooms.door[d].p[0] - homer.x, dz = rooms.door[d].p[2] - homer.z, q = dx * dx + dz * dz; if (q < kd) kd = q; }
    if (kd > ROOM_PF_KEEP * ROOM_PF_KEEP) rpfFail = -1;
  }
  int best = -1; float bd = ROOM_PF_DIST * ROOM_PF_DIST;
  for (int d = 0; d < rooms.ndoor; d++) { float dx = rooms.door[d].p[0] - homer.x, dz = rooms.door[d].p[2] - homer.z, q = dx * dx + dz * dz; if (q < bd && rooms.door[d].room >= 0 && rooms.door[d].room < rooms.nroom) { bd = q; best = d; } }
  if (best < 0) return;
  int room = rooms.door[best].room;
  if (room == rpfFail) return;
  const RoomGfxEnt *g = 0;
  for (int i = 0; i < rgN; i++) if (!strcmp(rgEnt[i].name, rooms.room[room].name)) g = &rgEnt[i];
  if (!g || rgBad[room] || (zone3AreaOnX(Z3A_PLACE) ? zone3MaxFreeX(Z3A_PLACE) < g->size : heapFreeBytes() < g->size + ROOM_PF_HEAP_MIN)) return;   // П5 Зоны-95: предчтение комнаты — в область «место», куча не нужна
  unsigned char *m = zone3TryX(Z3A_PLACE, "roompf", g->size, 16); if (!m) return;
  char fn[256]; levelFile(fn, "interiors-l%d-gfx.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  DCPRE96(m, g->size);
  if (f < 0 || sceIoLseek32(f, g->off, PSP_SEEK_SET) != (int)g->off || sceIoReadAsync(f, m, g->size) < 0) { if (f >= 0) sceIoClose(f); free(m); rpfFail = room; return; }
  rpf.room = room; rpf.m = m; rpf.sz = g->size; rpf.fd = f; rpf.state = 1; rpf.t0 = now; roomPfStarts++;
}
// метка этапа buyskin (viewer/rooms.c roomsSpotTarget, game_msn.c shSkinSpot): ближайший гардероб skins.txt или дверь его комнаты
#include "dcache96.h"
#include "wardrobe_view.h"
static int skinTargetMain(float px, float py, float pz, float out[3]) { return roomsSpotTarget(&rooms, rs.room, wardrobeSpot, px, py, pz, out); }
static void roomLog(int ev);
// Сверка g11b, P05_ии_действия №16 / Д6, P16-12: после входа в комнату управление выключено ещё 0,5 с после раскрытия ириса, после выхода — 1,0 с («пока камера оглянется»).
// ROOM_HOLD_OLD=1 — управление сразу, как в TEST81. Поворот Гомера к двери и ходьба во время ириса — анимация перехода (transit.c, main_loop.h), передано.
#ifndef ROOM_HOLD_OLD
#define ROOM_HOLD_OLD 0
#endif
#define ROOM_HOLD_ENTER_S .5f
#define ROOM_HOLD_EXIT_S 1.f
static float roomHoldT;
static void roomHoldStart(int ev) { if (!ROOM_HOLD_OLD) roomHoldT = ev > 0 ? ROOM_HOLD_ENTER_S : ROOM_HOLD_EXIT_S; }
static int roomHoldActive(float dt) { if (roomHoldT <= 0.f) return 0; roomHoldT -= dt; return 1; }   // 1 — ввод игрока этого кадра обнулить
// Сценарий миссии ставит Гомера в комнату (данные диска: SetMissionResetPlayerOutCar с локатором в комнате и SetDynaLoadData
// ("…;l4i02.p3d@", "SimpsonsHouse") — L4 m1sd, L7 m1sd «Гомер дома», L7 m4sd «школа»; у уровня 1 таких стартов нет): движок
// входит в эту комнату (рисование, коллизия, камеры; выход — к её двери, как после входа с улицы). Какая комната — интерьер
// последнего SetDynaLoadData, иначе любая; «в комнате» — под точкой сценария пол этой комнаты (комнаты под городом, в стороне).
// Перенос сценарием из комнаты в город — выход из комнаты. Зовётся на старте уровня / загрузке и после переноса сценарием
static int roomPoint96(float x, float y, float z);
static int roomFloorAt(int i, float x, float y, float z) { float g, n[3]; return colGround(&rooms.room[i].col, x, z, y + 1.5f, y - 3.f, &g, n); }
// ag/loadorder: точка переноса сценария лежит в комнате -> дверь этой комнаты в городе (куда выйдет игрок; вокруг неё читается город). 1 — нашли; rk — номер комнаты
static int roomDoorOf(const float p[3], float out[3], int *rk) {
  for (int k = 0; k < rooms.nroom; k++) if (roomFloorAt(k, p[0], p[1], p[2]))
    for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == k) { memcpy(out, rooms.door[d].p, 3 * sizeof(float)); if (rk) *rk = k; return 1; }
  return 0;
}
static const char *npcRoomAt(float x, float y, float z) {
  for (int i = 0; i < rooms.nroom; i++) if (roomFloorAt(i, x, y, z)) return rooms.room[i].name;
  return NULL;
}
// служебное (06.11, бот «начало-конец миссий»): куда встать, чтобы попасть туда же, где цель p. 1 — out = дверь комнаты цели (игрок в городе), 2 — out = выход
// своей комнаты (игрок в комнате, цель не в ней), 0 — цель там же, где игрок. interior — этап «войти»: точка цели может стоять у двери снаружи, тогда комната — ближайшей двери (до 20 м)
static int svcRoomGo(const float p[3], float out[3], int interior) {
  int tr = -1; for (int k = 0; k < rooms.nroom && tr < 0; k++) if (roomFloorAt(k, p[0], p[1], p[2])) tr = k;
  if (rs.room >= 0) { if (tr == rs.room || rs.room >= rooms.nroom) return 0; memcpy(out, rooms.room[rs.room].exit, 3 * sizeof(float)); return 2; }
  if (tr < 0 && interior) { float bd = 20.f * 20.f; for (int d = 0; d < rooms.ndoor; d++) { float dx = rooms.door[d].p[0] - p[0], dz = rooms.door[d].p[2] - p[2], q = dx * dx + dz * dz; if (q < bd && rooms.door[d].room >= 0) { bd = q; tr = rooms.door[d].room; } } }
  if (tr < 0) return 0;
  for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == tr) { memcpy(out, rooms.door[d].p, 3 * sizeof(float)); return 1; }
  return 0;
}
// ag/loadorder (№1209, класс «поставили раньше, чем открылась комната»): сценарий миссии ставит Гомера в карман комнат, пока мир города (G.pHomerWorld = 0). Пола комнаты в городе нет: Гомер падает на 6 м (HOMER_VOID_DROP), срабатывает
// возврат на «надёжное место» (homer.c), и так пилой между y −20 и −26 с периодом 0,7 с, пока перенос ждёт (выезд карточки брифинга 1,0 с, ирис, MS_SETUP_CAP кадров). msnRoomSync искал пол комнаты только в окне
// от 1,5 м выше точки до 3 м ниже: в нижней части «пилы» (около половины периода) пол оказывался выше окна, комната не находилась, Гомер оставался в городе под землёй, как в туре №1209.
// Теперь, если окно не нашло комнату, а миссия назвала комнату (SetDynaLoadData), её пол ищется выше Гомера до 25 м: он провалился под собственный пол; Гомер встаёт на него и комната открывается.
// MSNROOMDEEP1209_OLD=1 / «flag msnroomdeep1209old 1» — как было
#ifndef MSNROOMDEEP1209_OLD
#define MSNROOMDEEP1209_OLD 0
#endif
static int msnRoomDeep1209Old = MSNROOMDEEP1209_OLD;
static int roomFloorAbove(int i, float x, float y, float z, float *gy) { float n[3]; return colGround(&rooms.room[i].col, x, z, y + 25.f, y - 1.f, gy, n); }
static int roomPoint96(float x, float y, float z) {   // Build 96: точка в комнате. Комнаты лежат под городом на y около −20; старт миссии в доме ставит героя ДО загрузки таблицы комнат нового уровня (nroom 0) — тогда по высоте
  if (!rooms.nroom) return y > -26.f && y < -14.f;
  for (int k = 0; k < rooms.nroom; k++) { const Room *r = &rooms.room[k];
    if (roomFloorAt(k, x, y, z)) return 1;
    if (fabsf(y - r->start[1]) < 4.f && hypotf(x - r->start[0], z - r->start[2]) < 12.f) return 1;
    if (fabsf(y - r->exit[1]) < 4.f && hypotf(x - r->exit[0], z - r->exit[2]) < 12.f) return 1; }
  return 0; }   // Build 96: точка на полу какой-то комнаты (homerRoomPointHook)
static int roomSyncOff96;   // Build 96: 1 — msnRoomSync молчит (идёт чистая перезагрузка: старт уровня ставит игрока в комнату старта, входить в неё второй раз под тем же экраном не нужно)
static void msnRoomSync(void) {
  if (roomSyncOff96) return;
  char s[128];
  if (!rooms.nroom || !haveHomer || !onFoot()) return;
  const char *nm = mvDynaRoom(); int i = -1; float gDeep = 0.f; int deep = 0;
  for (int k = 0; k < rooms.nroom && i < 0; k++) if (nm[0] && !strcmp(rooms.room[k].name, nm) && roomFloorAt(k, homer.x, homer.y, homer.z)) i = k;
  for (int k = 0; k < rooms.nroom && i < 0; k++) if (roomFloorAt(k, homer.x, homer.y, homer.z)) i = k;
  if (i < 0 && !msnRoomDeep1209Old && nm[0] && !(rs.room >= 0 && rg.mem)) {   // Гомер провалился под пол названной комнаты (карман комнат в мире города)
    for (int k = 0; k < rooms.nroom && i < 0; k++) if (!strcmp(rooms.room[k].name, nm) && roomFloorAbove(k, homer.x, homer.y, homer.z, &gDeep)) i = k;
    if (i >= 0) { deep = 1; if (smkOn) { snprintf(s, sizeof s, "ROOM ev=msnrescue room=%s below=%.1f t=%.1f\n", rooms.room[i].name, gDeep - homer.y, smkT); smkWrite(s); } }
  }
  if (i < 0) { if (rs.room >= 0 || rg.mem) { roomLeave(); snprintf(s, sizeof s, "ROOM ev=msnout t=%.1f\n", smkT); smkWrite(s); } return; }
  if (rs.room == i && rg.mem) return;
  if (rs.room >= 0 || rg.mem) roomClose();
  mem1060Pre(1536u << 10, "room");   // №1060: пакет комнаты (до 0,8 МБ в туре TEST73) — цельным блоком; не хватает — далёкие клетки города
  if (!roomOpen(0, i)) { roomStateInit(&rs); G.pHomerWorld = 0; return; }
  rs.room = i; rs.exitArmed = 0; rs.doorArmed = 1; rs.lastDoor = -1;
  for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == i) { rs.lastDoor = d; break; }
  G.pHomerWorld = &rooms.room[i].col;
  homerPlace(&homer, G.pHomerWorld, homer.x, deep ? gDeep : homer.y + 1.f, homer.z, homer.h);   // точка сценария — на пол комнаты (deep — на пол, найденный глубже)
  chase.init = 0; roomLog(ROOM_EV_ENTER);
}
static void roomLog(int ev) {   // smoke.txt: вход/выход, куча до/после (гейт: 5 кругов без утечки), позиция Гомера
  if (!smkOn) return;
  char s[200]; const RoomDoor *d = &rooms.door[rs.lastDoor >= 0 ? rs.lastDoor : 0];
  if (ev == ROOM_EV_ENTER) snprintf(s, sizeof s, "ROOM ev=enter room=%s n=%d heap0=%u heap=%u bytes=%u tex=%u pieces=%u cams=%d ms=%.0f t=%.1f pos=%.2f,%.2f,%.2f h=%.3f\n",
                                    roomName(), ++roomN, rg.heap0, rg.heap1, rg.size, rgNtex, rg.npc, nrcam, rg.ms, smkT, homer.x, homer.y, homer.z, homer.h);
  else snprintf(s, sizeof s, "ROOM ev=exit room=%s n=%d heap=%u t=%.1f pos=%.2f,%.2f,%.2f h=%.3f\n",
                d->room >= 0 ? rooms.room[d->room].name : "?", roomN, heapFreeBytes(), smkT, homer.x, homer.y, homer.z, homer.h);
  smkWrite(s);
  if (ev == ROOM_EV_ENTER && rs.room >= 0) { char e_[120]; snprintf(e_, sizeof e_, "ROOMEXIT1002 room=%s exit=%.2f,%.2f,%.2f start=%.2f,%.2f,%.2f\n", rooms.room[rs.room].name, rooms.room[rs.room].exit[0], rooms.room[rs.room].exit[1], rooms.room[rs.room].exit[2], rooms.room[rs.room].start[0], rooms.room[rs.room].start[1], rooms.room[rs.room].start[2]); smkWrite(e_); }
}
static int rdTexKey(int tex) { if (tex >= RTEX0) tex -= RTEX0; return tex < 0 || tex > 510 ? 511 : tex; }   // правка 498: номер текстуры комнаты (RTEX0 + номер в блобе) -> ключ сортировки
// сверка g11b-collect круг 5 (P16-10): Moe's (moes) и BartRoom рисуются в два прохода. Первый — камера, отражённая через плоскость края комнаты
// (moes: x = 54,75; bartroom: z = 447, оси порта; экспорт хранит только половину комнаты), без отсечения по пирамиде, обход граней обратный, без
// полупрозрачного. Потом глубина очищается и идёт обычный проход поверх: отражение остаётся только там, где обычный кадр пуст (стена-зеркало). Физика,
// локаторы и свет не отражаются. Цена: комната дважды, только в этих двух. ROOM_MIRROR_OLD=1 — как было (один проход)
#ifndef TVGAP1083_OLD
#define TVGAP1083_OLD 0
#endif
#ifndef ROOM_MIRROR_OLD
#define ROOM_MIRROR_OLD 0
#endif
// №1106 (зеркала у Мо и в комнате Барта): отражённый проход тоже считается «от глаза», как №1083, чтобы на GE (float24) не было щелей между кусками. Вид без переноса умножается на
// линейное отражение (знак оси плоскости), перенос куска = центр - отражённый глаз (e' = отражение глаза через плоскость): V*Rf*(M) = Vrot*Rl*(M - e'), Rf = Rl + сдвиг. MIRROR1106_OLD=1 — отражённый
// проход с абсолютным переносом, как в круге 5 (с щелями на железе)
#ifndef MIRROR1106_OLD
#define MIRROR1106_OLD 0
#endif
static int roomMirrorOn;
static float rdMirE[3];   // №1106: глаз, отражённый через плоскость зеркала (ставит roomDrawMirrored)
// №1106, цена: в отражённый проход идут только куски, чей ОТРАЖЁННЫЙ ящик (ось плоскости зеркала перевёрнута) попадает в пирамиду видимости настоящей камеры (раньше шли все непрозрачные, 143 куска и
// 7871 треугольник у Moe's) и не мельче 1 м по диагонали ящика (BartRoom: 49 кусков из 118, треугольников 8023 из 9307; Moe's: 120 из 143; мелочь в отражении за стеной не читается).
// MIRRORCULL1106_OLD=1 — все непрозрачные куски, как в круге 5
#ifndef MIRRORCULL1106_OLD
#define MIRRORCULL1106_OLD 0
#endif
static int rdMirAx; static float rdMirPv;
static int rdMirVis(const float *b) {
  if (MIRRORCULL1106_OLD) return 1;
  { float dx = b[3] - b[0], dy = b[4] - b[1], dz = b[5] - b[2]; if (dx * dx + dy * dy + dz * dz < 1.f) return 0; }   // мельче 1 м по диагонали — не в отражение
  float r[6]; memcpy(r, b, sizeof r); r[rdMirAx] = 2.f * rdMirPv - b[rdMirAx + 3]; r[rdMirAx + 3] = 2.f * rdMirPv - b[rdMirAx];
  return visBox(r, 0, 0, 0);
}
static int roomNameIs(const char *n, const char *w) { if (!n) return 0; for (; *n && *w; n++, w++) if ((*n | 32) != *w) return 0; return !*n && !*w; }
// ТЕСТ90 (РОВНЫЕ_30 §3 п.2, «листы комнат»): комната — 263 куска дома Симпсонов каждый кадр отдельными вызовами (177–250 sceGuDrawArray по 25–60 мкс процессора: каждый — вызов ядра), листов не было.
// Теперь при входе в комнату куски, которые рисуются обычным непрозрачным путём (не полупрозрачные, не небо и не сложение, не полосы, текстура комнаты), запекаются в листы GE — по одному на группу
// «(флаги & 3, текстура)», как сортировка roomDraw; в листе у каждого куска те же команды, что дал бы drawPiece (привязка, флаги, матрица модели, рисование). Матрица модели — перенос = центр - глаз (№1083),
// он меняется каждый кадр: три слова переноса каждого куска перезаписываются перед вызовом листа теми же числами, что считал бы drawPiece (побитно); кусок, не прошедший пирамиду видимости, получает
// вместо команды рисования пустую команду (порядок и состав видимых кусков те же, что у прямого пути). Кадр: на видимую группу один вызов листа вместо вызова на кусок. Кусок под особым случаем в этом кадре
// (у стены: отсечение у камеры; анимированная текстура; срез по полосе ggOn/ggForce; диагностика) — вся группа идёт прямым путём, как раньше. Лист живёт, пока Гомер в комнате (rdlFree в roomClose).
// Два экземпляра листа (чётный и нечётный кадр): правятся слова листа, который видеочип в этом кадре ещё не читал. ROOMDL_OLD=1 / файл roomdlold рядом с EBOOT / «flag roomdl 0» — прямой путь, как до правки
#ifndef ROOM_TEXSORT_OLD
#define ROOM_TEXSORT_OLD 0
#endif
#ifndef ROOMDL_OLD
#define ROOMDL_OLD 1   // ПО УМОЛЧАНИЮ ВЫКЛЮЧЕНО до зелёного гейта tests/rooms/roomdl1190.py; включить сборкой -DROOMDL_OLD=0 или «flag roomdl 1» в сценарии
#endif
static int roomDlOn = !ROOMDL_OLD;
#define RDL_MAXG 160
#define RDL_MAXP 4096
#define RDL_MAXPRIM 6
typedef struct { unsigned short pi, np; unsigned mw, tris; unsigned pw[RDL_MAXPRIM], pv[RDL_MAXPRIM]; } RdlP;   // pi — номер куска комнаты; mw — слово первой из трёх команд переноса; pw/pv — слова и значения команд рисования
typedef struct { unsigned short p0, n; unsigned nw, off; unsigned *w[2]; } RdlG;   // off — начало листа группы (слов) в общем блоке rdlBase; w[0], w[1] — два экземпляра
static RdlP rdlP[RDL_MAXP]; static RdlG rdlG[RDL_MAXG]; static short rdlGOf[2048], rdlPOf[RDL_MAXP]; static int rdlN, rdlReady, rdlPar; static unsigned rdlBytes, rdlSig, rdlCalls, rdlDirect, rdlPcs;
static int rdlLo, rdlHi = 1 << 20; static unsigned rdlMask;   // служебно (flag roomdlmask N): куски с такими флагами в листы не идут (поиск причины различий)
static int rdlOnly = -1, rdlOnlyHi;   // lo, hi: куски lo..hi-1; hi = 0 — один кусок lo
static inline int rdlOnlyOk(int i) { return rdlOnly < 0 || (rdlOnlyHi ? i >= rdlOnly && i < rdlOnlyHi : i == rdlOnly); }   // служебно (flag roomdlonly N): в листы идёт только кусок N, остальные куски его группы — прямым путём после вызова листа
static int rdlDump;   // служебно (flag roomdldump 1): слова листа группы 0 в журнал
static int rdlBug;   // служебно (flag roomdlbug 1): перенос кусков в листе не обновляется — красный контроль гейта tests/rooms/roomdl1190.py
static int rdlCalledNow;   // в этом кадре хоть раз вызван лист комнаты
static unsigned *rdlBase; static unsigned char rdlVis[RDL_MAXP]; static const void *rdlFor;   // rdlBase — один блок под все листы комнаты (меньше дробления кучи)
   // rdlFor — для какой комнаты (rg.mem) собраны листы
static unsigned rdlSigNow(void) { return (unsigned)opt * 2654435761u ^ (unsigned)(int)(mipBias * 64.f) * 40503u ^ (unsigned)layerBias * 97u ^ (unsigned)flatOn * 7919u ^ (unsigned)mipAuto * 31u; }   // что запечено в листе из настроек: сменилось — пересборка
static void rdlFree(void) {
  free(rdlBase); rdlBase = 0; for (int g = 0; g < rdlN; g++) rdlG[g].w[0] = rdlG[g].w[1] = 0;
  if (smkOn && rdlReady) { char l_[160]; snprintf(l_, sizeof l_, "ROOMDL ev=free groups=%d pieces=%u bytes=%u calls=%u direct=%u t=%.1f\n", rdlN, rdlPcs, rdlBytes, rdlCalls, rdlDirect, smkT); smkWrite(l_); }
  rdlN = 0; rdlReady = 0; rdlBytes = 0; rdlPcs = 0; rdlCalls = rdlDirect = 0; rdlFor = 0;
}
static int rdlPieceOk(const Piece *d) {   // кусок рисуется обычным непрозрачным путём и его команды не зависят от глаза
  if (d->r.flags & (2 | 4 | F_ADDITIVE | F_STRIP) || d->r.tex < RTEX0 || (d->r.flags & rdlMask)) return 0;
  return d->r.ni > 0 && d->r.ni <= 65535 && d->r.sc > 0.f;
}
static void rdlEmit(Piece *d) {   // те же команды, что drawPiece при ровном переносе от глаза: c = 1, s = 0, перенос по умолчанию 0 (перезаписывается перед вызовом)
  float c = 1.f, s = 0.f, tx = 0.f, ty = 0.f, tz = 0.f, k = d->r.sc, ox = d->r.ox, oy = d->r.oy, oz = d->r.oz;
  ScePspFMatrix4 m = {
    {c * k, 0, -s * k, 0}, {0, k, 0, 0}, {s * k, 0, c * k, 0},
    {tx + c * ox + s * oz, ty + oy, tz - s * ox + c * oz, 1}};
  phAllow = d->r.owner == 0; NZP(d); bindTexLean(d->r.tex); phAllow = 0; setFlags(d->r.flags);
  sceGuSetMatrix(GU_MODEL, &m); drawGeo(d, 0, d->r.ni);
}
static void rdlBuild(void) {
  rdlFree(); memset(rdlGOf, 0xff, sizeof rdlGOf); rdlSig = rdlSigNow();
  if (!roomDlOn || rg.npc == 0 || rg.npc > RDL_MAXP || diagOn || flatOn || ROOM_TEXSORT_OLD || TVGAP1083_OLD) return;
  unsigned t0 = sceKernelGetSystemTimeLow(); unsigned h0 = heapFreeBytes();
  static unsigned char bad[2048]; static unsigned short gn[RDL_MAXG]; memset(bad, 0, sizeof bad);
  for (unsigned i = 0; i < rg.npc; i++) { Piece *d = &rg.pc[i]; if (d->r.ni <= 0 || !pieceOk(d, 0)) continue;   // ничего не рисует — группе не мешает
    if (!rdlPieceOk(d) && rdlOnly < 0) bad[((d->r.flags & 3) << 9) | rdTexKey(d->r.tex)] = 1; }
  memset(gn, 0, sizeof gn);
  for (unsigned i = 0; i < rg.npc; i++) { Piece *d = &rg.pc[i]; rdlPOf[i] = -1; if (d->r.ni <= 0 || !pieceOk(d, 0) || !rdlPieceOk(d) || !rdlOnlyOk((int)i)) continue;
    int key = ((d->r.flags & 3) << 9) | rdTexKey(d->r.tex); if (bad[key]) continue;
    if (rdlGOf[key] < 0) { if (rdlN >= RDL_MAXG) { rdlFree(); return; } rdlGOf[key] = (short)rdlN; memset(&rdlG[rdlN], 0, sizeof rdlG[rdlN]); rdlN++; }
    gn[rdlGOf[key]]++; rdlPOf[i] = (short)rdlGOf[key]; }
  unsigned off = 0; for (int g = 0; g < rdlN; g++) { rdlG[g].p0 = (unsigned short)off; rdlG[g].n = 0; off += gn[g]; }
  static unsigned short fill[RDL_MAXG]; memset(fill, 0, sizeof fill);
  unsigned gcap = rg.npc * 160u + (unsigned)rdlN * 8u + 64u, goff = 0;   // gacc — листы всех групп подряд (временно), потом один блок на два экземпляра
  unsigned *tmpP = (unsigned *)memalign(16, 4096 * 4), *acc = 0, *gacc = (unsigned *)malloc(gcap * 4);   // tmpP — список одного куска; acc — список группы
  if (!tmpP || !gacc) { free(tmpP); free(gacc); rdlFree(); return; }
  dlLost(); int fail = 0; unsigned total = 0, npc = 0;
  for (int g = 0; g < rdlN && !fail; g++) {
    unsigned cap = (unsigned)gn[g] * 160u + 64u; acc = (unsigned *)malloc(cap * 4); if (!acc) { fail = 1; break; }
    unsigned nw = 0;
    for (unsigned i = 0; i < rg.npc && !fail; i++) { if (rdlPOf[i] != g) continue; Piece *d = &rg.pc[i];
      sceKernelDcacheWritebackInvalidateRange(tmpP, 4096 * 4); sceGuStart(GU_CALL, tmpP); if (!dlNoDepth) sceGuDepthRange(65535, 0); rdlEmit(d); DCALL96(); unsigned sz = (unsigned)sceGuFinish();   // у контекста листа свои границы глубины (нули): без этого sceGuDepthOffset слоёв (setBias) в листе портил границы глубины видеочипа для всего кадра (как у листов клеток и пропов) unsigned sz = (unsigned)sceGuFinish();
      unsigned n = sz / 4u; const volatile unsigned *u = (const volatile unsigned *)((unsigned)tmpP | 0x40000000u);
      if (n < 2 || n > 4000 || (u[n - 1] >> 24) != 0x0bu || nw + n > cap) { fail = 1; break; }   // последнее слово — возврат из листа (RET)
      RdlP *P = &rdlP[rdlG[g].p0 + fill[g]++]; memset(P, 0, sizeof *P); P->pi = (unsigned short)i; P->tris = (unsigned)d->r.ni / 3u; P->mw = 0xffffffffu;
      for (unsigned q = 0; q < n - 1; q++) { unsigned w = u[q], op = w >> 24; acc[nw + q] = w;
        if (op == 0x3a) { /* WORLDN */ if (q + 12 < n - 1 && (u[q + 1] >> 24) == 0x3b) P->mw = nw + q + 1 + 9; }   // перенос — последние три из двенадцати слов матрицы
        else if (op == 0x04) { if (P->np < RDL_MAXPRIM) { P->pw[P->np] = nw + q; P->pv[P->np] = w; P->np++; } else fail = 1; } }   // PRIM
      if (P->mw == 0xffffffffu || !P->np) fail = 1;
      nw += n - 1; npc++; }
    if (fail) { free(acc); acc = 0; break; }
    unsigned nw1_ = nw + 1;
    { unsigned nw1 = (nw + 1 + 3u) & ~3u; if (goff + nw1 > gcap) { free(acc); acc = 0; fail = 1; break; }
      memcpy(gacc + goff, acc, nw * 4u); for (unsigned q = nw; q < nw1; q++) gacc[goff + q] = q == nw ? 0x0b000000u : 0u;   // RET, добор до 16 байт нулями
      rdlG[g].off = goff; rdlG[g].nw = nw + 1; rdlG[g].n = gn[g]; goff += nw1; }
    free(acc); acc = 0;
    if (smkOn && rdlDump && g == 0) { for (unsigned q = 0; q < nw1_; q += 8) { char l_[200]; int o = snprintf(l_, sizeof l_, "RDLW %u:", q); for (unsigned r = q; r < q + 8 && r < nw1_; r++) o += snprintf(l_ + o, sizeof l_ - (size_t)o, " %08x", gacc[rdlG[g].off + r]); snprintf(l_ + o, sizeof l_ - (size_t)o, "\n"); smkWrite(l_); } }
    if (smkOn && g == 0) { char l_[400]; int o = snprintf(l_, sizeof l_, "ROOMDL ev=g0pieces"); for (unsigned q = 0; q < gn[g] && o < 360; q++) o += snprintf(l_ + o, sizeof l_ - (size_t)o, " %u", (unsigned)rdlP[rdlG[g].p0 + q].pi); snprintf(l_ + o, sizeof l_ - (size_t)o, "\n"); smkWrite(l_); }
    if (smkOn) { const Piece *f0_ = &rg.pc[rdlP[rdlG[g].p0].pi]; unsigned tr_ = 0; for (unsigned q = 0; q < gn[g]; q++) tr_ += rdlP[rdlG[g].p0 + q].tris; char l_[200]; snprintf(l_, sizeof l_, "ROOMDL ev=grp g=%d tex=%d fl=%x n=%u tris=%u words=%u ni0=%d sc=%.3f\n", g, f0_->r.tex, (unsigned)f0_->r.flags, (unsigned)gn[g], tr_, nw, f0_->r.ni, f0_->r.sc); smkWrite(l_); } }
  if (phTfx) { sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); phTfx = 0; }
  dlLost(); free(tmpP);
  if (!fail) {   // один блок: два экземпляра всех листов; запись только через некэшируемое зеркало, до неё — сброс строк кэша, оставшихся от прежнего хозяина памяти
    unsigned bytes = goff * 4u; rdlBase = (unsigned *)memalign(16, bytes * 2u);
    if (!rdlBase) fail = 1;
    else { sceKernelDcacheWritebackInvalidateRange(rdlBase, bytes * 2u); volatile unsigned *dst = (volatile unsigned *)((unsigned)rdlBase | 0x40000000u);
      for (unsigned q = 0; q < goff; q++) { dst[q] = gacc[q]; dst[goff + q] = gacc[q]; }
      for (int g = 0; g < rdlN; g++) { rdlG[g].w[0] = rdlBase + rdlG[g].off; rdlG[g].w[1] = rdlBase + goff + rdlG[g].off; }
      total = bytes * 2u; } }
  free(gacc);
  if (fail) { rdlFree(); if (smkOn) { char l_[120]; snprintf(l_, sizeof l_, "ROOMDL ev=fail room=%d t=%.1f\n", rg.room, smkT); smkWrite(l_); } return; }
  rdlBytes = total; rdlPcs = npc; rdlReady = 1; rdlPar = 0; rdlFor = rg.mem;
  if (smkOn) { unsigned hv[16], hn[16]; int nh = 0; for (unsigned i = 0; i < rg.npc; i++) { unsigned f = (unsigned)rg.pc[i].r.flags & 0xffffu; int q = 0; for (; q < nh; q++) if (hv[q] == f) break; if (q == nh && nh < 16) { hv[nh] = f; hn[nh] = 0; nh++; } if (q < nh) hn[q]++; }
    char l_[400]; int o = snprintf(l_, sizeof l_, "ROOMDL ev=flags"); for (int q = 0; q < nh; q++) o += snprintf(l_ + o, sizeof l_ - (size_t)o, " %x:%u", hv[q], hn[q]); snprintf(l_ + o, sizeof l_ - (size_t)o, "\n"); smkWrite(l_); }
  if (smkOn) { char l_[200]; int o_ = snprintf(l_, sizeof l_, "ROOMDL ev=anim n=%d", roomAnN); for (int q = 0; q < roomAnN; q++) o_ += snprintf(l_ + o_, sizeof l_ - (size_t)o_, " base=%d nk=%d fps=%.2f", roomAn[q].base, roomAn[q].nk, roomAn[q].fps); snprintf(l_ + o_, sizeof l_ - (size_t)o_, "\n"); smkWrite(l_); }
  if (smkOn) { char l_[200]; snprintf(l_, sizeof l_, "ROOMDL ev=build room=%d groups=%d pieces=%u bytes=%u heap=%u ms=%.1f t=%.1f\n", rg.room, rdlN, npc, total, heapFreeBytes(), (sceKernelGetSystemTimeLow() - t0) * .001f, smkT); smkWrite(l_); (void)h0; }
}
static int rdlChk; static unsigned rdlChkN, rdlChkBad;   // служебно (flag roomdlchk 1): сверка слов переноса с тем, что дал бы drawPiece (те же выражения, c и s — настоящие переменные)
__attribute__((noinline)) static void rdlRefMat(float tx, float ty, float tz, float c, float s, float k, float ox, float oy, float oz, float *w) {
  ScePspFMatrix4 m = {{c * k, 0, -s * k, 0}, {0, k, 0, 0}, {s * k, 0, c * k, 0}, {tx + c * ox + s * oz, ty + oy, tz - s * ox + c * oz, 1}};
  w[0] = m.w.x; w[1] = m.w.y; w[2] = m.w.z;
}
static inline unsigned rdlWord(float f) { unsigned u; memcpy(&u, &f, 4); return 0x3b000000u | (u >> 8); }   // как sendCommandf у libgu: 24 старших бита числа
// вызов листа группы g: перенос видимых кусков = центр - глаз (те же числа, что drawPiece), невидимым — пустая команда вместо рисования
static void rdlCall(int g, const float *E_) {
  RdlG *G = &rdlG[g]; volatile unsigned *u = (volatile unsigned *)((unsigned)G->w[rdlPar] | 0x40000000u); unsigned long tris = 0; unsigned nv = 0;
  for (unsigned q = 0; q < G->n; q++) { RdlP *P = &rdlP[G->p0 + q];
    if (rdlVis[P->pi]) { Piece *d = &rg.pc[P->pi]; float c = 1.f, s = 0.f, tx = -E_[0], ty = -E_[1], tz = -E_[2], ox = d->r.ox, oy = d->r.oy, oz = d->r.oz;
      if (rdlChk) { volatile float cv = 1.f, sv = 0.f; float w3[3]; rdlRefMat(tx, ty, tz, cv, sv, d->r.sc, ox, oy, oz, w3); rdlChkN++;
        if (rdlWord(w3[0]) != rdlWord(tx + c * ox + s * oz) || rdlWord(w3[1]) != rdlWord(ty + oy) || rdlWord(w3[2]) != rdlWord(tz - s * ox + c * oz)) { rdlChkBad++; if (smkOn && rdlChkBad <= 8) { char l_[160]; snprintf(l_, sizeof l_, "ROOMDL ev=chkbad pi=%u ref=%.6f,%.6f,%.6f mine=%.6f,%.6f,%.6f\n", P->pi, w3[0], w3[1], w3[2], tx + c * ox + s * oz, ty + oy, tz - s * ox + c * oz); smkWrite(l_); } }
        if (smkOn && !(rdlChkN & 1023u)) { char l_[100]; snprintf(l_, sizeof l_, "ROOMDL ev=chk n=%u bad=%u\n", rdlChkN, rdlChkBad); smkWrite(l_); } }
      if (!rdlBug) { u[P->mw] = rdlWord(tx + c * ox + s * oz); u[P->mw + 1] = rdlWord(ty + oy); u[P->mw + 2] = rdlWord(tz - s * ox + c * oz); }
      for (unsigned k = 0; k < P->np; k++) u[P->pw[k]] = P->pv[k];
      tris += P->tris; nv++; gateCount(d->r.ni); }
    else for (unsigned k = 0; k < P->np; k++) u[P->pw[k]] = 0u; }
  if (!nv) return;
  drawnTris += (long)tris; draws += (int)nv;
  sceGuCallList(G->w[rdlPar]); dlLost(); rdlCalls++; rdlCalledNow = 1;
}
static void roomPiece(unsigned i, int rel_, const float *E_) {   // кусок комнаты прямым путём (как до листов комнат)
  Piece *d = &rg.pc[i];
  if (trLeft == TR_FRAMES && trDetail) {   // след 259: кусок комнаты в первом кадре камеры миссии — до отправки на GE
    const Tex *x_ = texAt(d->r.tex); char t_[200];
    int l_ = snprintf(t_, sizeof t_, "TRR i=%u fl=%x ni=%d v=%p idx=%p tex=%d pal=%p lv0=%p slv0=%p lv=%d list=%d\n", i, d->r.flags, d->r.ni, (void *)d->v, (void *)d->idx,
                      d->r.tex, x_ ? (void *)x_->pal : 0, x_ ? (void *)x_->lv[0] : 0, x_ ? (void *)x_->slv[0] : 0, x_ ? x_->levels : -1, (int)sceGuCheckList());
    trLine(t_, l_); }
  int t0_ = d->r.tex; if (roomAnN) d->r.tex = roomAnimTex(t0_);   // круг 5 (P16-11): анимация текстуры
  if (rel_ && !(tqOn && (d->r.flags & 2) && !(d->r.flags & 4))) drawPiece(d, 0, -E_[0], -E_[1], -E_[2], 1.f, 0.f);   // №1083: перенос = центр - глаз (полупрозрачное идёт в очередь с обычным видом, как у города)
  else drawPiece(d, 0, 0.f, 0.f, 0.f, 1.f, 0.f);
  d->r.tex = t0_;
}
static void roomDraw(void) {   // куски комнаты: пирамида видимости, ближняя версия; полупрозрачное — в очередь tq
  // правка Роба 27: камеры комнат оригинала стоят за стенами — оболочка повёрнута лицом в комнату и рисуется с отсечением
  // изнанки (флаг 128, tools/roomorient.py), мебель, видимая изнутри с обеих сторон, — двусторонняя (флаг 8); старый пакет
  // без флага 128 — двусторонне, как раньше
  // Правка Роба 498 (замер TEST6, L7 «город 4,8 мс, 130–148 смен текстуры»: на деле это комната SimpsonsHouse, не город — Гомер в доме всю сцену):
  // куски комнаты шли в порядке таблицы — 294 куска, 48 текстур, 164 смены текстуры за кадр (SHG1: порядок по мешам). Теперь видимые куски
  // сортируются подсчётом по (прозрачность, текстура), как город (cullCity): те же куски, тот же результат, 48 смен вместо 164.
  // -DROOM_TEXSORT_OLD=1 — порядок таблицы
  #ifndef ROOM_TEXSORT_OLD
  #define ROOM_TEXSORT_OLD 0
  #endif
  static unsigned short rdOrd[4096], rdCnt[2048]; unsigned rdN = 0;
  if (!ROOM_TEXSORT_OLD && rg.npc <= 4096) {
    memset(rdCnt, 0, sizeof rdCnt);
    for (unsigned i = 0; i < rg.npc; i++) { Piece *d = &rg.pc[i]; if (roomMirrorOn ? (!(d->r.flags & 3) && rdMirVis(d->r.bb)) : visBox(d->r.bb, 0, 0, 0)) {
        int t = rdTexKey(d->r.tex); rdOrd[rdN++] = (unsigned short)i; rdCnt[((d->r.flags & 3) << 9) | t]++; } }
    unsigned s_ = 0; for (int k = 0; k < 2048; k++) { unsigned c_ = rdCnt[k]; rdCnt[k] = (unsigned short)s_; s_ += c_; }
    static unsigned short rdSorted[4096];
    for (unsigned j = 0; j < rdN; j++) { Piece *d = &rg.pc[rdOrd[j]]; int t = rdTexKey(d->r.tex); rdSorted[rdCnt[((d->r.flags & 3) << 9) | t]++] = rdOrd[j]; }
    for (unsigned j = 0; j < rdN; j++) rdOrd[j] = rdSorted[j];
  } else { for (unsigned i = 0; i < rg.npc; i++) rdOrd[rdN++ & 4095] = (unsigned short)i; if (rg.npc > 4096) rdN = 4096; }
  // №1083 (= №1075, щель у телевизора, фото PSP IMG_8110): в данных комнаты щели нет (спинка телевизора на z 400,73 двусторонняя и сходится с верхним скосом по общему
  // ребру; сверка tests/rooms/tvgap1083.py). Щель — точность GE: комната лежит в 500 м от начала, куски рисовались с переносом = центр куска в мировых координатах, а GE считает
  // вершины во float24 (шаг у 500 м ~ 4-8 мм): общая вершина двух кусков (скос и спинка — разные куски со своими центрами) округляется по-разному и ребро расходится на пиксель.
  // Так же город чинили правками 224/237/251: вид без переноса (глаз в начале), перенос куска = центр - глаз считает процессор во float32. Тот же приём для куска комнаты.
  // Общее правило для всех комнат (не координаты одного места). Отражённый проход Moe's/BartRoom (круг 5) остаётся прежним. TVGAP1083_OLD=1 — как было
  ScePspFMatrix4 vAbs_; int rel_ = !TVGAP1083_OLD && (!roomMirrorOn || !MIRROR1106_OLD), mv_ = rel_ && !roomMirrorOn;   // mv_: вид меняем здесь (в отражённом проходе его уже задал roomDrawMirrored)
  const float *E_ = roomMirrorOn ? rdMirE : ggE;
  if (mv_) { sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vAbs_); sceGumMatrixMode(GU_MODEL); ScePspFMatrix4 vr_ = vAbs_; vr_.w.x = vr_.w.y = vr_.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vr_); }
  int rdlUse = roomDlOn && rel_ && !roomMirrorOn && !ROOM_TEXSORT_OLD && rg.npc <= 4096 && !ggOn && !ggForce && !diagOn && !geOver(GE_HARD);   // листы комнат (РОВНЫЕ_30 шаг 2): листы комнаты (rdlBuild выше)
  if (rdlUse && (!rdlReady || rdlFor != rg.mem || rdlSig != rdlSigNow())) rdlBuild();
  rdlUse = rdlUse && rdlReady;
  rdlCalledNow = 0;
  if (rdlUse) { rdlPar ^= 1; memset(rdlVis, 0, rg.npc); for (unsigned j = 0; j < rdN; j++) rdlVis[rdOrd[j]] = 1; }
  unsigned j = 0;
  while (j < rdN) {
    unsigned e = j + 1; int g = -1;
    if (rdlUse) {   // серия кусков одной группы «(флаги & 3, текстура)»: все годные — один вызов листа; особый случай хоть у одного — вся серия прямым путём
      Piece *d0 = &rg.pc[rdOrd[j]]; int key = ((d0->r.flags & 3) << 9) | rdTexKey(d0->r.tex);
      while (e < rdN) { Piece *d1 = &rg.pc[rdOrd[e]]; if ((((d1->r.flags & 3) << 9) | rdTexKey(d1->r.tex)) != key) break; e++; }
      g = rdlGOf[key];
      if (g >= 0) for (unsigned k = j; k < e; k++) { Piece *dk = &rg.pc[rdOrd[k]]; if (rdlPOf[rdOrd[k]] < 0) continue;   // кусок, который ничего не рисует
        if (camClipNear(dk) || (roomAnN && roomAnimTex(dk->r.tex) != dk->r.tex)) { g = -1; break; } }
    }
    if (g >= 0 && (g < rdlLo || g >= rdlHi)) g = -1;   // служебно (flag roomdlrange): листы только у части групп
    if (g >= 0) { rdlCall(g, E_); if (rdlOnly >= 0) for (unsigned k = j; k < e; k++) if (rdlPOf[rdOrd[k]] < 0) roomPiece(rdOrd[k], rel_, E_); j = e; continue; }
    unsigned e2 = rdlUse ? e : j + 1; if (rdlUse) rdlDirect += e - j;
    for (unsigned k = j; k < e2; k++) { unsigned i = rdOrd[k]; Piece *d = &rg.pc[i]; if (ROOM_TEXSORT_OLD ? (roomMirrorOn ? (!(d->r.flags & 3) && rdMirVis(d->r.bb)) : visBox(d->r.bb, 0, 0, 0)) : 1) roomPiece(i, rel_, E_); }
    j = e2;
  }
  if (rdlUse && rdlCalledNow) {   // листы комнат (РОВНЫЕ_30 шаг 2): после листа в видеочипе остаётся состояние последнего куска ЛИСТА (он мог быть невидим), а прямой путь оставляет состояние последнего нарисованного куска; Гомер и прочее дальше полагаются на это состояние —
    for (int jj = (int)rdN - 1; jj >= 0; jj--) { Piece *dl_ = &rg.pc[rdOrd[jj]]; if (dl_->r.ni <= 0 || (tqOn && (dl_->r.flags & 2) && !(dl_->r.flags & 4)) || !pieceOk(dl_, 0)) continue;   // ставим флаги последнего нарисованного куска, как оставил бы прямой путь
      setFlags(dl_->r.flags | (camClipNear(dl_) ? F_LSIDE : 0)); break; } }
  if (mv_) sceGuSetMatrix(GU_VIEW, &vAbs_);
}
// №1118 (правка Роба: отражение без героя не нужно): отражённый проход Moe's и BartRoom выключен, вместо него на панели зеркала — холодный оттенок стекла и мягкий блик, медленно
// скользящий по стеклу вслед за положением камеры (векторный приём света psp-render §5: ровный цвет по вершинам, обычная прозрачность, без текстуры, один вызов сетки 14 x 3 клетки).
// Панели — по данным комнат (плоские куски у плоскости зеркала): Moe's x = 54,88 (перед панелью 54,91..54,98), z 198,4..202,6, y -19,1..-17,45; BartRoom z = 447,14 (панель 447,03..447,06),
// x 498,56..500,45, y -19,2..-18,1. Блик — полоса вдоль стекла со смещением по высоте (диагональ), плюс тонкая вторая полоса слабее. Положение полосы = смещение камеры вдоль стекла / расстояние
// до стекла (как отражение источника света: камера влево — блик по стеклу влево), плюс медленный дрейф +-0,06 за 12 с, чтобы стоящая камера не давала мёртвую картинку.
// MIRRORGLINT1118_OLD=1 — прежнее: отражённый проход (круг 5, №1106) без блика
#ifndef MIRRORGLINT1118_OLD
#define MIRRORGLINT1118_OLD 0
#endif
#include "mirrorglint.h"
typedef struct { unsigned c; float x, y, z; } GlintV;
typedef struct { int nx, la; float p, sg, l0, l1, y0, y1; } GlintPanel;
static void roomGlint(int k) {
  // ось нормали nx (0 = x, 2 = z), положение стекла p, сторона комнаты sg (-1/+1), боковая ось la, границы по боковой оси l0..l1 и по высоте y0..y1
  static const GlintPanel M_[2] = {
    { 0, 2, 54.88f, -1.f, 198.4f, 202.6f, -19.1f, -17.45f },       // Moe's
    { 2, 0, 447.14f, 1.f, 498.56f, 500.45f, -19.2f, -18.1f } };    // BartRoom
  const GlintPanel *m = &M_[k - 1];
  enum { NX = 14, NY = 3 };
  GlintV *q = (GlintV *)sceGuGetMemory(NX * NY * 6 * sizeof(GlintV)); if (!q) return;
  float cpos = glintPos(ggE[m->la], (m->l0 + m->l1) * .5f, ggE[m->nx], m->p, ggE[1], (m->y0 + m->y1) * .5f, (float)(gtUs95() & 0xFFFFFFu) * 1e-6f);
  GlintV g[(NX + 1) * (NY + 1)];
  for (int j = 0; j <= NY; j++) for (int i = 0; i <= NX; i++) {
    float u = (float)i / NX, w = (float)j / NY, h = glintH(u, w, cpos);
    float lat = m->l0 + (m->l1 - m->l0) * u, y = m->y0 + (m->y1 - m->y0) * w;
    GlintV *o = &g[j * (NX + 1) + i]; o->c = glintColor(h);
    float P[3]; P[m->nx] = m->p; P[m->la] = lat; P[1] = y;
    o->x = P[0] - ggE[0]; o->y = P[1] - ggE[1]; o->z = P[2] - ggE[2];
  }
  int n = 0;
  for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
    const GlintV *a_ = &g[j * (NX + 1) + i], *b_ = a_ + 1, *c_ = a_ + (NX + 1), *d_ = c_ + 1;
    q[n++] = *a_; q[n++] = *b_; q[n++] = *d_; q[n++] = *a_; q[n++] = *d_; q[n++] = *c_;
  }
  resetState();
  ScePspFMatrix4 vAbs_, vr_; sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vAbs_); sceGumMatrixMode(GU_MODEL);   // вид без переноса, координаты считаем от глаза (как №1083)
  vr_ = vAbs_; vr_.w.x = vr_.w.y = vr_.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vr_);
  ScePspFMatrix4 id; memset(&id, 0, sizeof id); id.x.x = id.y.y = id.z.z = id.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &id);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_CULL_FACE); sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE);
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, q);
  sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND); sceGuSetMatrix(GU_VIEW, &vAbs_);
  resetState();
}
static void roomDrawMirrored(void) {   // круг 5 (P16-10): вызывается вместо roomDraw
  const char *nm = roomName(); int k = ROOM_MIRROR_OLD ? 0 : roomNameIs(nm, "moes") ? 1 : roomNameIs(nm, "bartroom") ? 2 : 0;
  if (k && !MIRRORGLINT1118_OLD) { roomDraw(); roomGlint(k); return; }   // №1118: без отражённого прохода, блик на стекле
  if (k) {
    ScePspFMatrix4 v0, rf; memset(&rf, 0, sizeof rf); rf.x.x = rf.y.y = rf.z.z = rf.w.w = 1.f;
    if (k == 1) { rf.x.x = -1.f; rf.w.x = 2.f * 54.75f; } else { rf.z.z = -1.f; rf.w.z = 2.f * 447.f; }
    rdMirAx = k == 1 ? 0 : 2; rdMirPv = k == 1 ? 54.75f : 447.f;
    sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&v0);
    if (!TVGAP1083_OLD && !MIRROR1106_OLD) {   // №1106: вид без переноса x линейное отражение; глаз отражаем отдельно, перенос куска = центр - отражённый глаз
      ScePspFMatrix4 vr = v0, rl = rf; vr.w.x = vr.w.y = vr.w.z = 0.f; rl.w.x = rl.w.y = rl.w.z = 0.f;
      sceGumLoadMatrix(&vr); sceGumMultMatrix(&rl);
      rdMirE[0] = ggE[0]; rdMirE[1] = ggE[1]; rdMirE[2] = ggE[2];
      if (k == 1) rdMirE[0] = 2.f * 54.75f - ggE[0]; else rdMirE[2] = 2.f * 447.f - ggE[2];
    } else sceGumMultMatrix(&rf);
    sceGumMatrixMode(GU_MODEL);
    sceGuFrontFace(GU_CW); roomMirrorOn = 1; roomDraw(); roomMirrorOn = 0; sceGuFrontFace(GU_CCW);
    sceGumMatrixMode(GU_VIEW); sceGumLoadMatrix(&v0); sceGumMatrixMode(GU_MODEL);
    sceGuClear(GU_DEPTH_BUFFER_BIT);
  }
  roomDraw();
}

