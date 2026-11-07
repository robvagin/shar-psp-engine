// №1178 (Роб 03.11, L5 B3-4: «билборды вдалеке прорисовываются фиговые, некоторые важные и прикольные»): вывески и щиты читаются вдали.
// Причина (замер в эмуляторе с программным рендером, tests/sign1178): мип по глубине (GU_TEXTURE_SLOPE) берётся по плотности текселей текстуры,
// у щита 25 м на 100 м от глаза это 55 px шириной и мип в 1 px текста: кот у щита 153 и надпись — серая каша; на один уровень мипа резче (наклон ×0,5)
// видны рисунок и надпись. Лечится одним числом на текстуру: наклон текстуры в level.shr умножается на множитель из боковой таблицы.
// Файл `sign1178.bin` в папке уровня (tools/far1177/signs.py, список вывесок уровня — tools/far1177/signs_L<N>.json по подписи рисунка, не по номеру):
//   'SG78', u32 версия (1), u32 число текстур уровня (ntex), u32 число записей n, потом n × { u16 номер текстуры, u16 множитель наклона в 1/4096 }.
// Нет файла, не тот ntex, битый заголовок — ничего не меняется (побайтно прежняя игра). Цена: ≈1 КБ файла, открытие 15 мс при загрузке уровня, 0 КБ кучи, 0 мс кадра
// (наклон берётся из той же структуры; листы клеток собираются после загрузки и несут новое значение).
// SIGN1178_OLD=1 — выключатель сборки, файл-флаг `sign1178old` рядом с EBOOT — выключатель без пересборки.
#ifndef SIGN1178_H
#define SIGN1178_H
#ifndef SIGN1178_OLD
#define SIGN1178_OLD 0
#endif
static unsigned sign1178N;   // сколько текстур уровня получили множитель (для журнала)
static void signBiasLoad(void) {
  sign1178N = 0;
  if (SIGN1178_OLD || !tex || !ntex) return;
  char fo[256]; sibling(fo, "sign1178old"); { SceUID o = sceIoOpen(fo, PSP_O_RDONLY, 0); if (o >= 0) { sceIoClose(o); return; } }
  char fn[256]; levelFile(fn, "sign1178.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned h[4]; unsigned short *e = 0; int ok = 0;
  if (sceIoRead(f, h, 16) == 16 && !memcmp(h, "SG78", 4) && h[1] == 1 && h[2] == ntex && h[3] > 0 && h[3] <= 1024u) {
    e = (unsigned short *)malloc(h[3] * 4u);
    if (e && sceIoRead(f, e, (int)(h[3] * 4u)) == (int)(h[3] * 4u)) ok = 1; }
  sceIoClose(f);
  if (ok) for (unsigned i = 0; i < h[3]; i++) {
    unsigned t = e[2 * i], k = e[2 * i + 1];
    if (t < ntex && k >= 512u && k <= 8192u && tex[t].slope > 0.f) { tex[t].slope *= (float)k * (1.f / 4096.f); sign1178N++; } }   // 0,125…2: вне предела запись пропускается
  free(e);
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "SIGN1178 level=%d ok=%d n=%u of %u\n", gameLevel, ok, sign1178N, ok ? h[3] : 0u); smkWrite(s_); }
}
#endif
