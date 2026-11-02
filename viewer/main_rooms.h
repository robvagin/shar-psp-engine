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
      if (m && (regAt(px, pz) & m)) { zgMsnMask = m; zgMsnFr = 120; regRes = m; zgInit = 0; }
      else zgMsnMask = 0;
      if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "ZONE msn \"%.60s\" mask %04x ok %d t=%.1f\n", fl, m, zgMsnMask != 0, smkT); smkWrite(s_); }
    }
  }
  if (zgStart || jump) {   // старт уровня или перенос: набор ближайшего места (оригинал при телепорте ставит набор места)
    unsigned b = regRes; regRes = zonePlaceNear(zp, nzp, px, pz, 0xFFFFu); zgStart = 0; zgInit = 0;
    if (!ZONE_MSN_SET_OLD && zgMsnFr > 0 && zgMsnMask && (regAt(px, pz) & zgMsnMask)) regRes = zgMsnMask;   // круг 5 (P12-10): сразу после команды миссии её набор главнее места
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
static RoomAnim roomAn[ROOM_ANIMS]; static int roomAnN; static unsigned roomAnT0;
static void roomAnimParse(const unsigned char *m, unsigned sz, unsigned ntx) {
  roomAnN = 0; if (ROOM_ANIM_OLD || sz < 16 + 128 || memcmp(m + sz - 16, "RAN1", 4)) return;
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
  roomAnT0 = sceKernelGetSystemTimeLow();
}
// номер текстуры куска комнаты с учётом анимации
static int roomAnimTex(int t) {
  if (!roomAnN || t < RTEX0) return t;
  float sec = (float)(sceKernelGetSystemTimeLow() - roomAnT0) * 1e-6f;
  for (int i = 0; i < roomAnN; i++) { const RoomAnim *a = &roomAn[i];
    if (t != RTEX0 + a->base) continue;
    float f = fmodf(sec * a->fps, a->len); int k = 0; for (int j = 0; j < a->nk; j++) if (a->fr[j] <= f) k = j;
    return RTEX0 + a->tx[k]; }
  return t;
}
static void roomClose(void) {   // блоб комнаты — обратно в кучу (GE прошлый кадр закончил: sceGuSync в конце кадра)
  if (rg.mem) bbOp("room-", rg.room >= 0 && rg.room < rooms.nroom ? rooms.room[rg.room].name : "-", 0);   // правка 103: самописец
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
static int roomOpen(void *u, int room) {
  (void)u; bbOp("room+", rooms.room[room].name, 1);   // правка 103: самописец — ящик на карте ДО чтения комнаты
  unsigned t0 = sceKernelGetSystemTimeLow(), h0 = smkOn ? heapFreeBytes() : 0;
  const RoomGfxEnt *g = 0;
  for (int i = 0; i < rgN; i++) if (!strcmp(rgEnt[i].name, rooms.room[room].name)) g = &rgEnt[i];
  int err = !g ? 1 : rgBad[room] ? 2 : 0; unsigned char *m = 0; Tex *tx = 0; Piece *pp = 0;
  unsigned sz = g ? g->size : 0, ntx = 0, np = 0, to = 0, ro = 0;
  if (!err && !(m = memalign(16, sz))) err = 3;   // нет памяти — не «битый»: отойдёт от двери и вернётся — новая попытка
  if (!err) {
    char fn[256]; levelFile(fn, "interiors-l%d-gfx.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
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
  rg.clear = rdU(m + 20); rg.amb = rdU(m + 24); rg.nlit = (int)rdU(m + 28); rcs.cur = -1; rcs.in = 0; memset(rcs.inT, 0, sizeof rcs.inT); if (!CUT1085_OLD) roomCamRestart(&rcs);   /* №1085 (сверка g06): вход в помещение — первая камера срезом */ rcs.newWins = !roomCamStickyOld; rcs.switchOld = roomCamSwitchOld;   // правка 186; файл roomcamswitchold — задержка 172 до школы
  for (int i = 0; i < rg.nlit; i++) { memcpy(rg.lit[i], m + 32 + 16 * i, 12); rg.litc[i] = rdU(m + 32 + 16 * i + 12); }
  // №1076: комната читается при закрытом экране (дверь) — персонажи миссии и бонусные у её старта (Дедушка в доме L1) дочитываются здесь же, а не на глазах
  // после открытия; не дольше 1,5 с (тёмный экран держится чуть дольше). Служебное flag npcwaitold 1 — как до правки
  if (!NPC_WAIT1076_OLD && !npcWaitOld) { unsigned w0 = sceKernelGetSystemTimeLow(); int n0 = mvNpcPending() + mvAmbMissingAt(rooms.room[room].start[0], rooms.room[room].start[1], rooms.room[room].start[2]), nl = n0;
    while (nl > 0 && sceKernelGetSystemTimeLow() - w0 < 1500000u) { sceKernelDelayThread(5000); nl = mvNpcPending() + mvAmbMissingAt(rooms.room[room].start[0], rooms.room[room].start[1], rooms.room[room].start[2]); }
    if (smkOn && n0) { char s_[96]; snprintf(s_, sizeof s_, "NPCWAIT1076 room=%s pending0=%d left=%d ms=%u\n", rooms.room[room].name, n0, nl, (sceKernelGetSystemTimeLow() - w0) / 1000u); smkWrite(s_); } }
  rg.ms = (sceKernelGetSystemTimeLow() - t0) / 1000.f; rg.heap0 = h0; rg.heap1 = smkOn ? heapFreeBytes() : 0;
  return 1;
}
// метка этапа buyskin (viewer/rooms.c roomsSpotTarget, game_msn.c shSkinSpot): ближайший гардероб skins.txt или дверь его комнаты
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
static int roomFloorAt(int i, float x, float y, float z) { float g, n[3]; return colGround(&rooms.room[i].col, x, z, y + 1.5f, y - 3.f, &g, n); }
static const char *npcRoomAt(float x, float y, float z) {
  for (int i = 0; i < rooms.nroom; i++) if (roomFloorAt(i, x, y, z)) return rooms.room[i].name;
  return NULL;
}
static void msnRoomSync(void) {
  char s[128];
  if (!rooms.nroom || !haveHomer || !onFoot()) return;
  const char *nm = mvDynaRoom(); int i = -1;
  for (int k = 0; k < rooms.nroom && i < 0; k++) if (nm[0] && !strcmp(rooms.room[k].name, nm) && roomFloorAt(k, homer.x, homer.y, homer.z)) i = k;
  for (int k = 0; k < rooms.nroom && i < 0; k++) if (roomFloorAt(k, homer.x, homer.y, homer.z)) i = k;
  if (i < 0) { if (rs.room >= 0 || rg.mem) { roomLeave(); snprintf(s, sizeof s, "ROOM ev=msnout t=%.1f\n", smkT); smkWrite(s); } return; }
  if (rs.room == i && rg.mem) return;
  if (rs.room >= 0 || rg.mem) roomClose();
  mem1060Pre(1536u << 10, "room");   // №1060: пакет комнаты (до 0,8 МБ в туре TEST73) — цельным блоком; не хватает — далёкие клетки города
  if (!roomOpen(0, i)) { roomStateInit(&rs); G.pHomerWorld = 0; return; }
  rs.room = i; rs.exitArmed = 0; rs.doorArmed = 1; rs.lastDoor = -1;
  for (int d = 0; d < rooms.ndoor; d++) if (rooms.door[d].room == i) { rs.lastDoor = d; break; }
  G.pHomerWorld = &rooms.room[i].col;
  homerPlace(&homer, G.pHomerWorld, homer.x, homer.y + 1.f, homer.z, homer.h);   // точка сценария — на пол комнаты
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
static int roomMirrorOn;
static int roomNameIs(const char *n, const char *w) { if (!n) return 0; for (; *n && *w; n++, w++) if ((*n | 32) != *w) return 0; return !*n && !*w; }
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
    for (unsigned i = 0; i < rg.npc; i++) { Piece *d = &rg.pc[i]; if (roomMirrorOn ? !(d->r.flags & 3) : visBox(d->r.bb, 0, 0, 0)) {
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
  ScePspFMatrix4 vAbs_; int rel_ = !TVGAP1083_OLD && !roomMirrorOn;
  if (rel_) { sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vAbs_); sceGumMatrixMode(GU_MODEL); ScePspFMatrix4 vr_ = vAbs_; vr_.w.x = vr_.w.y = vr_.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vr_); }
  for (unsigned j = 0; j < rdN; j++) { unsigned i = rdOrd[j]; Piece *d = &rg.pc[i]; if (ROOM_TEXSORT_OLD ? (roomMirrorOn ? !(d->r.flags & 3) : visBox(d->r.bb, 0, 0, 0)) : 1) {
      if (trLeft == TR_FRAMES && trDetail) {   // след 259: кусок комнаты в первом кадре камеры миссии — до отправки на GE
        const Tex *x_ = texAt(d->r.tex); char t_[200];
        int l_ = snprintf(t_, sizeof t_, "TRR i=%u fl=%x ni=%d v=%p idx=%p tex=%d pal=%p lv0=%p slv0=%p lv=%d list=%d\n", i, d->r.flags, d->r.ni, (void *)d->v, (void *)d->idx,
                          d->r.tex, x_ ? (void *)x_->pal : 0, x_ ? (void *)x_->lv[0] : 0, x_ ? (void *)x_->slv[0] : 0, x_ ? x_->levels : -1, (int)sceGuCheckList());
        trLine(t_, l_); }
      int t0_ = d->r.tex; if (roomAnN) d->r.tex = roomAnimTex(t0_);   // круг 5 (P16-11): анимация текстуры
      if (rel_ && !(tqOn && (d->r.flags & 2) && !(d->r.flags & 4))) drawPiece(d, 0, -ggE[0], -ggE[1], -ggE[2], 1.f, 0.f);   // №1083: перенос = центр - глаз (полупрозрачное идёт в очередь с обычным видом, как у города)
      else drawPiece(d, 0, 0.f, 0.f, 0.f, 1.f, 0.f);
      d->r.tex = t0_; } }
  if (rel_) sceGuSetMatrix(GU_VIEW, &vAbs_);
}
static void roomDrawMirrored(void) {   // круг 5 (P16-10): вызывается вместо roomDraw
  const char *nm = roomName(); int k = ROOM_MIRROR_OLD ? 0 : roomNameIs(nm, "moes") ? 1 : roomNameIs(nm, "bartroom") ? 2 : 0;
  if (k) {
    ScePspFMatrix4 v0, rf; memset(&rf, 0, sizeof rf); rf.x.x = rf.y.y = rf.z.z = rf.w.w = 1.f;
    if (k == 1) { rf.x.x = -1.f; rf.w.x = 2.f * 54.75f; } else { rf.z.z = -1.f; rf.w.z = 2.f * 447.f; }
    sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&v0); sceGumMultMatrix(&rf); sceGumMatrixMode(GU_MODEL);
    sceGuFrontFace(GU_CW); roomMirrorOn = 1; roomDraw(); roomMirrorOn = 0; sceGuFrontFace(GU_CCW);
    sceGumMatrixMode(GU_VIEW); sceGumLoadMatrix(&v0); sceGumMatrixMode(GU_MODEL);
    sceGuClear(GU_DEPTH_BUFFER_BIT);
  }
  roomDraw();
}

