// TEST93 (Данные-93): штамп и полнота набора. Беда Роба 07.11: «я обновляю, а у людей не подгружается и переходит через уровень» — движок не знал, от какого
// выпуска лежат данные, и без файлов уровня молча уходил в меню / на уровень 1.
//   C1 setRevCheck(): рядом с EBOOT файл set.txt (tools/setrev.sh): rev=<число>. Нет файла — SETREV none, игра как раньше (наборы до 93). Номер не совпал с
//      SHAR_DATA_REV (viewer/datarev.h) — экран «Game files are from another build. Run the builder again.» (SETREV_OLD=1 — играет молча, как раньше).
//   C2 setCheckRun(): проход sceIoGetstat по обязательным файлам (viewer/setfiles.inc, генерирует tools/gen_setfiles.sh), итог в журнал и ящик;
//      есть недостающее — экран «Level N files are missing (lN/level.shr). Run the builder again.» с ✕ «играть всё равно» (SETCHECK_OLD=1 — молча, как раньше).
//   C3 setLevelMissingScreen(n): тот же честный экран в местах молчаливого ухода (уровень пройден, а следующего нет; запись на уровне без набора).
// 🔴 Служебный режим (смоук, тур, svcMode): экрана нет, только строка журнала SETREV/SETCHECK, игра идёт дальше (иначе тур на PSP и frame_gate повиснут).
#ifndef SHAR_SETCHECK_H
#define SHAR_SETCHECK_H
#include "datarev.h"
#include "setfiles.inc"   // setFiles[] / SETFILES_N: обязательные файлы набора (уровень 1 — в корне, 2–7 — в l<N>/)

static int setLevels;          // set.txt: levels=N — сколько уровней собрано в наборе (0 — set.txt нет или поле не указано)
static int setMissing = -1;    // итог последнего прохода (-1 — не проходили)
static int setFirstLevel;      // уровень первого недостающего файла
static char setFirstName[64];  // его имя («l3/level.shr»)

static int setUi;   // flag setcheckui (сценарий смоука): экраны как у игрока, кнопку на экране «нажимает» сам тест (smkTap) — гейт tests/smoke_scenes/d93_*
static int setQuiet(void) { return (smkOn || svcMode) && !setUi; }   // служебный режим: только журнал
static void setNote(const char *line) {   // журнал смоука + чёрный ящик по событию
  char b[160]; snprintf(b, sizeof b, "%s\n", line); smkWrite(b); bbNote(b);
}
static int setStat(const char *rel, int dir) {   // файл (папка) рядом с EBOOT есть? Путь строим сами (не через sibling: язык и служебные имена тут ни при чём)
  char fn[256]; strcpy(fn, path); char *sl = strrchr(fn, '/'); size_t o = sl ? (size_t)(sl + 1 - fn) : 0;
  if (o + strlen(rel) + 1 > sizeof fn) return 0;
  strcpy(fn + o, rel); SceIoStat st; if (sceIoGetstat(fn, &st) < 0) return 0;
  return dir ? (st.st_mode & FIO_S_IFDIR) != 0 : 1;
}
static int setFileLevel(const char *nm) { return (nm[0] == 'l' && nm[1] >= '2' && nm[1] <= '7' && nm[2] == '/') ? nm[1] - '0' : 1; }
static int setLevelExpected(int n) {   // уровень входит в набор? 1 — всегда; 2–7: по set.txt (levels=N), без него — если есть папка l<N>/ (набор до 93 без уровней — не тревога)
  if (n <= 1) return 1;
  if (setLevels > 0) return n <= setLevels;
  char d[8]; snprintf(d, sizeof d, "l%d", n); return setStat(d, 1);
}
static void setReadTxt(int *rev) {   // set.txt: rev=… levels=…; нет файла — *rev = -1
  *rev = -1; setLevels = 0;
  char fn[256]; strcpy(fn, path); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "set.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  char b[160]; int n = sceIoRead(f, b, sizeof b - 1); sceIoClose(f); if (n <= 0) { *rev = 0; return; }
  b[n] = 0; *rev = 0;
  for (char *p = b; p && *p; ) {
    if (!strncmp(p, "rev=", 4)) *rev = atoi(p + 4); else if (!strncmp(p, "levels=", 7)) setLevels = atoi(p + 7);
    p = strchr(p, '\n'); if (p) p++;
  }
}
extern unsigned char msx[];   // шрифт PSPSDK 8x8 (libpspdebug), тот же, что у экрана fail()
static void setPx(unsigned char *base, int bw, int fmt, int x, int y, unsigned rgb) {   // rgb 0xRRGGBB -> пиксель буфера показа
  unsigned r = (rgb >> 16) & 255u, g = (rgb >> 8) & 255u, b = rgb & 255u;
  if (fmt == 3) ((unsigned *)base)[y * bw + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
  else { unsigned short v = fmt == 0 ? (unsigned short)(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3)) : fmt == 1 ? (unsigned short)(0x8000u | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)) : (unsigned short)(0xF000u | ((b >> 4) << 8) | ((g >> 4) << 4) | (r >> 4)); ((unsigned short *)base)[y * bw + x] = v; }
}
static void setDrawText(void *top, int bw, int fmt, const char *msg, const char *label) {
  unsigned char *base = (unsigned char *)(((unsigned)top & 0x0FFFFFFFu) | 0x40000000u);   // тот же приём, что у экрана сторожа (bbDogScreen)
  for (int y = 0; y < 272; y++) for (int x = 0; x < 480; x++) setPx(base, bw, fmt, x, y, 0);
  char ln[16][72]; int n = 0; memset(ln, 0, sizeof ln);
  snprintf(ln[n++], 72, "SHAR PSP"); n++;
  for (const char *p = msg; *p && n < 10;) { int w = (int)strlen(p) > 68 ? 68 : (int)strlen(p); if (w == 68) { while (w > 40 && p[w] != ' ') w--; } memcpy(ln[n], p, (size_t)w); n++; p += w; while (*p == ' ') p++; }
  n++; snprintf(ln[n++], 72, "%s", label); snprintf(ln[n++], 72, "Press HOME to exit.");
  for (int r = 0; r < n; r++) for (int c = 0; ln[r][c]; c++) {
    const unsigned char *gl = msx + (unsigned char)ln[r][c] * 8;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gl[y] & (0x80 >> x)) { int px = c * 7 + x, py = r * 8 + y; if (px < 480 && py < 272) setPx(base, bw, fmt, px, py, 0xFFFFFF); }
  }
}
#ifndef SETRESTORE_OLD
#define SETRESTORE_OLD 0   // 1 — после экрана дисплей не возвращается (кадр игры в чужом формате)
#endif
#define SET_BTN (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_START)   // ✕ по тексту, но годится любая кнопка действия: экран не должен запереть игру
// Экран с ожиданием ✕ (отладочный шрифт PSP, как экраны fail() и сторожа). Вызывать не из служебного режима. Возвращает после ✕ (или просьбы выйти)
static void setModal(const char *msg, const char *label) {
    // TEST93 (дирижёр: после «play anyway» кадр игры в чужом формате): pspDebugScreenInit переставляет дисплей на 32 бита и адрес 0, игра после него рисует в 5650 — кадр 2x2 и чужие цвета.
  // Поэтому текст пишется ПРЯМО в показанный буфер, в его собственном формате и ширине строки; дисплей и GU не трогаются (следующий кадр игры затирает текст). SETRESTORE_OLD=1 — прежний путь
  void *dTop = 0; int dBw = 0, dFmt = 0; int dOk = !SETRESTORE_OLD && sceDisplayGetFrameBuf(&dTop, &dBw, &dFmt, PSP_DISPLAY_SETBUF_IMMEDIATE) >= 0 && dTop && dBw > 0;
  if (dOk) setDrawText(dTop, dBw, dFmt, msg, label);
  else {
    pspDebugScreenInit();   // как fail()
    pspDebugScreenSetBackColor(0xFF000000u); pspDebugScreenSetTextColor(0xFFFFFFFFu); pspDebugScreenClear(); pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("SHAR PSP\n\n%s\n\n%s\nPress HOME to exit.", msg, label);
  }
  sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  SceCtrlData pd; int armed = 0, tf = -1;   // сперва дождаться, пока ✕ отпущена (нажатие, которое привело сюда, не считается)
  if (setUi && smkOn) {   // тест: снимок экрана, дальше «нажатие» кнопки через тот же smokePad, что и у игры
    static int shotN; char nm[24], l[96], fn[256]; snprintf(nm, sizeof nm, "setui%d", ++shotN);
    snprintf(l, sizeof l, "SHOT %s %.1f pos 0.00 0.00 0.00 0.0000\n", nm, smkT); smkWrNow = 1; smkWrite(l);
    strcpy(fn, smkFile); char *sl = strrchr(fn, '/'); snprintf(sl ? sl + 1 : fn, 40, "shot_%s.ok", nm);
    for (int k = 0; k < 1500; k++) { SceIoStat st; if (sceIoGetstat(fn, &st) >= 0) break; sceDisplayWaitVblankStart(); }
    tf = 0;
  }
  while (!exitReq) {
    if (tf >= 0) { tf++; smkTap = tf == 20 ? PSP_CTRL_CROSS : tf == 30 ? 0u : smkTap; }
    sceCtrlPeekBufferPositive(&pd, 1); pd.Buttons |= smokePad();
    if (!(pd.Buttons & SET_BTN)) armed = 1; else if (armed) break;
    sceDisplayWaitVblankStart();
  }
  smkTap = 0;
  for (int k = 0; k < 30; k++) { sceCtrlPeekBufferPositive(&pd, 1); if (!(pd.Buttons & SET_BTN)) break; sceDisplayWaitVblankStart(); }   // отпустить ✕, чтобы игра её не получила
  setNote("SETCHECK continue");
}
// C1
static void setRevCheck(void) {
  int rev; setReadTxt(&rev);
  char l[96];
  if (rev < 0) { setNote("SETREV none"); return; }
  if (rev == SHAR_DATA_REV) { snprintf(l, sizeof l, "SETREV ok rev=%d levels=%d", rev, setLevels); setNote(l); return; }
  snprintf(l, sizeof l, "SETREV want=%d have=%d", SHAR_DATA_REV, rev); setNote(l);
  if (SETREV_OLD || setQuiet()) return;
  fail("Game files are from another build. Run the builder again.");
}
// C2: проход по обязательным файлам; итог в setMissing / setFirstLevel / setFirstName
static void setCheckRun(void) {
  unsigned t0 = sceKernelGetSystemTimeLow(); int miss = 0, seen = 0; setFirstName[0] = 0; setFirstLevel = 0;
  for (int n = 1; n <= 7; n++) {
    if (!setLevelExpected(n)) continue;
    for (int i = 0; i < (int)SETFILES_N; i++) {
      if (setFileLevel(setFiles[i]) != n) continue;
      seen++;
      if (!setStat(setFiles[i], 0)) { miss++; if (!setFirstName[0]) { snprintf(setFirstName, sizeof setFirstName, "%s", setFiles[i]); setFirstLevel = n; } }
    }
  }
  setMissing = miss;
  char l[160]; snprintf(l, sizeof l, "SETCHECK missing=%d first=%s files=%d us=%u", miss, miss ? setFirstName : "-", seen, sceKernelGetSystemTimeLow() - t0); setNote(l);
}
static void setMissingText(char *out, size_t cap, int n, const char *first) {
  snprintf(out, cap, "Level %d files are missing (%s). Run the builder again.", n, first);
}
// №1237 (дирижёр, видео тестера: модель NPC не ложится): куча игры = вся память пользователя минус 3 МБ (PSP_HEAP_SIZE_KB). Без «Use Extra Memory» (ARK: Off / Default) разделу отдано ~24 МБ,
// куча на старте ~18 МБ, и город, модели и ролики не помещаются — «пустые места». С Auto / MAX — ~44 МБ. Ниже MEMCHK1237_MIN_MB — честный экран, ✕ «играть всё равно»; служебный режим — только строка MEMCHK.
// MEMCHK1237_OLD=1 / файл memchk1237old — без проверки. Для теста: flag memchksim — «замер» 18 МБ (эмулятор всегда с полной памятью), вместе с flag setcheckui — экран и ✕ сценария
#ifndef MEMCHK1237_OLD
#define MEMCHK1237_OLD 0
#endif
#define MEMCHK1237_MIN_MB 32
static int memChk1237 = !MEMCHK1237_OLD;
static void memBootCheck(void) {
  if (smkOn && smokeWantsFlag(path, "memchk1237old")) memChk1237 = 0;   // красный контроль сценария
  if (!memChk1237) return;
  float mb = heapFreeBytes() / 1048576.f; int sim = smkOn && smokeWantsFlag(path, "memchksim"); if (sim) mb = 18.f;
  int low = mb < (float)MEMCHK1237_MIN_MB; char l[96]; snprintf(l, sizeof l, "MEMCHK heap=%.1f min=%d low=%d%s", mb, MEMCHK1237_MIN_MB, low, sim ? " sim=1" : ""); setNote(l);
  if (low && !setQuiet()) setModal("Turn on Use Extra Memory (Auto or MAX) in ARK settings.", "Press X to play anyway.");
}
static void setBootCheck(void) {   // при старте, до меню (служебный режим уже известен)
  setUi = smkOn && smokeWantsFlag(path, "setcheckui");
  memBootCheck();
  if (!SETREV_OLD) setRevCheck();
  if (SETCHECK_OLD) return;
  setCheckRun();
  if (setMissing > 0 && !setQuiet()) {
    char m[200]; setMissingText(m, sizeof m, setFirstLevel, setFirstName); setModal(m, "Press X to play anyway.");
  }
}
// C3: уровень n нужен, а его набора нет (уровень пройден → следующий; запись/выбор миссии на уровне без набора). Имя недостающего — по списку, иначе lN/level.shr
static void setLevelMissingScreen(int n) {
  char first[64]; snprintf(first, sizeof first, n > 1 ? "l%d/level.shr" : "level.shr", n);
  for (int i = 0; i < (int)SETFILES_N; i++) if (setFileLevel(setFiles[i]) == n && !setStat(setFiles[i], 0)) { snprintf(first, sizeof first, "%s", setFiles[i]); break; }
  char l[160]; snprintf(l, sizeof l, "SETCHECK nolevel n=%d first=%s", n, first); setNote(l);
  if (setQuiet()) return;
  char m[200]; setMissingText(m, sizeof m, n, first); setModal(m, "Press X to continue.");
}
// C3: запись/загрузка на уровне без набора: 1 — идти дальше, 0 — назад в меню (экран уже показан). Служебный режим и SETCHECK_OLD — 1 (прежний уход на уровень 1)
static int setSaveGuard(int level) {
  if (SETCHECK_OLD || levelAvailable(level)) return 1;
  setLevelMissingScreen(level);
  return setQuiet() ? 1 : 0;
}
#endif
