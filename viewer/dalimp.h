// Ориентиры дали (шаг Д1 разбора arch-fable 04.11, TEST90): таблица дали dal877.h режет город до 150–200 м там, где «даль занимает ≤ 1 % экрана».
// Для башен, высоких зданий и крупных щитов этого мало: они занимают меньше 1 %, но именно их глаз ищет вдали (жалоба Роба на даль автострады L5).
// Файл `dalimp.bin` в папке уровня (tools/far1177/dalimp.py): 'DIM1', u32 подпись таблицы кусков (pvs2Sig, как у dal877/PVS2), u32 n, n × u32 номер куска-ориентира.
// Кусок-ориентир, попавший в пирамиду между урезанной дальностью таблицы и полной (регулятор, 250 м), добавляется в список отсечения (cullPiece с полной дальностью),
// остальное режется как раньше. Город рисуется листом клетки целиком (дальние клетки — dlBuildFar), поэтому ориентир тянет за собой свою клетку 40 м, не больше.
// Нет файла, не та подпись, битые номера — ничего не меняется (побайтно прежняя игра). Цена: файл ≈ 1 КБ, кучи 1 КБ, кадр — обход списка (≤ 300 кусков, ящик и расстояние) и
// дальние листы клеток-ориентиров (модель tools/far1177/dalimp.py --cost: L5 +0,5 мс в среднем, p99 +2 мс; L4 +0,2 / +1,4). Строка DALIMP в смоуке раз в 2 с.
// Включение: файл `dalimpon` рядом с EBOOT или -DDALIMP_ON=1 / «flag dalimp 1». Прежнее: DALIMP_OLD=1 / файл `dalimpold` / «flag dalimp 0».
#ifndef DALIMP_H
#define DALIMP_H
#ifndef DALIMP_ON
#define DALIMP_ON 0   // выключен по умолчанию: на пяти точках L5 (башни и щиты дальше урезанной дали) снимок не отличается, цена +10…100 вызовов; включить — файл `dalimpon` рядом с EBOOT или -DDALIMP_ON=1
#endif
#ifndef DALIMP_OLD
#define DALIMP_OLD (!DALIMP_ON)
#endif
static int dimOn = DALIMP_ON;
static unsigned *dimK; static unsigned dimN; static float (*dimBB)[6]; static unsigned short *dimCell;   // номера кусков, их ящики и клетки подряд: проход не лезет в Piece (112 Б на кусок)
static unsigned dimFr, dimAdd, dimUs, dimPieces, dimMax;   // кадров с проходом, кусков добавлено, мкс, ориентиров на прогоне (счётчики строки DALIMP)
static void dimFree(void) { free(dimK); free(dimBB); free(dimCell); dimK = 0; dimBB = 0; dimCell = 0; dimN = 0; dimArm = 0; }
static void dimLoad(const unsigned char *tab, unsigned npcN) {
  dimFree(); if (!npcN) return;
  { char fo[256]; SceUID o; sibling(fo, "dalimpon"); if ((o = sceIoOpen(fo, PSP_O_RDONLY, 0)) >= 0) { sceIoClose(o); dimOn = 1; }
    sibling(fo, "dalimpold"); if ((o = sceIoOpen(fo, PSP_O_RDONLY, 0)) >= 0) { sceIoClose(o); dimOn = 0; } }   // данные грузятся всегда (1 КБ): «flag dalimp 1» в смоуке работает без пересборки
  char fn[256]; levelFile(fn, "dalimp.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned h[3]; int ok = 0;
  if (sceIoRead(f, h, 12) == 12 && !memcmp(h, "DIM1", 4) && h[1] == pvs2Sig(tab, npcN) && h[2] > 0 && h[2] <= 4096u) {
    unsigned *k = (unsigned *)malloc(h[2] * 4u);
    if (k && sceIoRead(f, k, (int)(h[2] * 4u)) == (int)(h[2] * 4u)) { ok = 1; for (unsigned i = 0; i < h[2]; i++) if (k[i] >= npcN) ok = 0; }
    if (ok) { dimBB = (float (*)[6])malloc(h[2] * sizeof *dimBB); dimCell = (unsigned short *)malloc(h[2] * 2u);
      if (dimBB && dimCell) { for (unsigned i = 0; i < h[2]; i++) { const unsigned char *r = tab + 80u * k[i]; memcpy(dimBB[i], r, 24); unsigned c = *(const unsigned *)(r + 68); dimCell[i] = c < 0xFFFFu ? (unsigned short)c : 0xFFFFu; } dimK = k; dimN = h[2]; }
      else { free(dimBB); free(dimCell); dimBB = 0; dimCell = 0; free(k); ok = 0; } }
    else free(k); }
  sceIoClose(f);
  if (smkOn) { char s_[100]; snprintf(s_, sizeof s_, "DALIMP load level=%d ok=%d n=%u\n", gameLevel, ok, ok ? dimN : 0u); smkWrite(s_); }
}
// из cullFinish (main_render.h): один раз за кадр, после основного отсечения и до сортировки; dimCapV — дальность после таблицы, dimKeepV — полная (ставит dalApply)
static void dimPass(void) {
  if (!dimArm) return; dimArm = 0;
  if (!dimOn || !dimN || dimKeepV <= dimCapV + 1.f) return;
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int nd0 = nd; float sv = drawDist, c2 = dimCapV * dimCapV, k2 = dimKeepV * dimKeepV; drawDist = dimKeepV;
  for (unsigned i = 0; i < dimN; i++) {
    float d2 = boxD2(dimBB[i]); if (d2 <= c2 || d2 > k2) continue;   // ближе урезанной дальности: уже в списке (или вне кадра); дальше полной — не рисуется
    unsigned ci = dimCell[i]; if (ci >= ncell || cl[ci].state != 2) continue;   // клетка не в памяти — рисовать нечего
    cullPiece(dimK[i]); }
  drawDist = sv;
  if (smkOn) { dimFr++; dimAdd += (unsigned)(nd - nd0); dimUs += sceKernelGetSystemTimeLow() - t0; if ((unsigned)(nd - nd0) > dimMax) dimMax = (unsigned)(nd - nd0); }
}
static void dimLine(float t) {   // раз в 2 с из dalApply (смоук)
  if (!dimN) return;
  char s_[160]; snprintf(s_, sizeof s_, "DALIMP t=%.1f on=%d pieces=%u frames=%u added_avg=%.1f added_max=%u pass_us_avg=%.1f\n", t, dimOn, dimN, dimFr, dimFr ? (double)dimAdd / dimFr : 0., dimMax, dimFr ? (double)dimUs / dimFr : 0.);
  smkWrite(s_); dimFr = dimAdd = dimUs = dimMax = 0;
}
#endif
