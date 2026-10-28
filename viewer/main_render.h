#include "xsub.h"
// ---------- карта высот и стены ----------
#include "bisect.h"
static int ground(float x, float z, float y, float step, float *out) {
  int gx = (int)((x - hox) / hcell), gz = (int)((z - hoz) / hcell);
  if (gx < 0 || gz < 0 || gx >= hw || gz >= hh) return 0;
  short *c = hm + ((size_t)gz * hw + gx) * 4; int found = 0; float best = -1e9f;
  for (int i = 0; i < 4; i++) {
    if (c[i] == -32768) continue;
    float h = c[i] * .01f;
    if (h <= y + step && h > best) { best = h; found = 1; }
  }
  if (found) *out = best;
  return found;
}
extern int roadArrowOld;   // viewer/msn_view.c, №606
static int mvGroundHook(float x, float z, float yHi, float yLo, float *out) { float n[3]; return haveCol && colGround(&world, x, z, yHi, yLo, out, n); }   // №606: высота коллизии под стрелкой маршрута
static int mvGroundNHook(float x, float z, float yHi, float yLo, float *y, float *n3) { return haveCol && colGround(&world, x, z, yHi, yLo, y, n3); }   // №1025: высота и нормаль под стрелкой
// стена в клетке перекрывает высоты кузова машины?
static int wallAt(float x, float z, float y) {
  int gx = (int)((x - hox) / hcell), gz = (int)((z - hoz) / hcell);
  if (gx < 0 || gz < 0 || gx >= hw || gz >= hh) return 1;
  short *w = wl + ((size_t)gz * hw + gx) * 2;
  if (w[0] > w[1]) return 0;
  float lo = w[0] * .01f, hi = w[1] * .01f;
  return hi > y + 0.45f && lo < y + 1.6f;
}

// ---------- текстуры ----------
static int ilog2(int v) { return 31 - __builtin_clz(v); }
// swizzle: блоки 16 байт x 8 строк, как любит кэш текстур GE; высота добивается до кратной 8
static void swizzleLevel(unsigned char *out, const unsigned char *in, int w, int h) {
  int hp = (h + 7) & ~7, rb = w / 16;
  for (int j = 0; j < hp; j++) {
    const unsigned char *row = in + (j < h ? j : h - 1) * w;
    for (int bx = 0; bx < rb; bx++)
      memcpy(out + ((j >> 3) * rb + bx) * 128 + (j & 7) * 16, row + bx * 16, 16);
  }
}
static void loadPump(void);   // правка 244: кадр газеты из долгих циклов загрузки (ниже, у loadFrame)
static void swizzleAll(void) {
  for (unsigned i = 0; i < ntex; i++) {
    loadPump();
    Tex *t = &tex[i]; size_t sz = 0; int lw = t->w, lh = t->h;
    if (t->pre) continue;                                          // уже в swizzle из экспортёра, копия не нужна
    for (int l = 0; l < t->levels; l++) { sz += (size_t)lw * ((lh + 7) & ~7); lw >>= 1; lh = lh > 1 ? lh >> 1 : 1; }
    unsigned char *b = memalign(16, sz); if (!b) return;           // нет памяти — останемся на линейных
    lw = t->w; lh = t->h;
    for (int l = 0; l < t->levels; l++) {
      t->slv[l] = b; swizzleLevel(b, t->lv[l], lw, lh);
      b += (size_t)lw * ((lh + 7) & ~7); lw >>= 1; lh = lh > 1 ? lh >> 1 : 1;
    }
  }
}

// ---------- состояние GE: шлём только то, что поменялось ----------
static int bound, texOn, curFlags, curFilt, binds;
static void setCull(int on); static void setBias(int b);
static float curSlope = -1.f; static int mipAuto, mipConst = -1, curLvlMode = -1;   // mipConst — служебное «flag mipconst N» (-1 выкл.): один уровень мипа у всех текстур, A/B арки J9-5 на железе static float curSlope = -1.f;   // правка 77/88: режим выбора мипа в GE (0 по треугольнику, 1 по глубине)
#define MIP_BIAS 0.f
// «Земля» (асфальт, разметка, тротуар, трава и т.п., tools/psp_export.py GROUND_RE; байт 9 заголовка текстуры level.shr = 1): наклон выбора мипа уже запечён с множителем 2^0,75 (SHR_GROUND_BIAS).
// Дополнительный множитель наклона в движке без пересборки данных: файл `groundbias` рядом с EBOOT (десятичное число 0,25 до 4, например 1.5; нет файла или число вне предела: 1, как раньше)
// и служебный флаг сценария `flag groundbias X`. Больше 1 — земля уходит на размытый мип раньше (меньше ряби вдали, мягче вблизи). GROUNDBIAS_RT_OLD=1 — файл и флаг игнорируются.
#ifndef GROUNDBIAS_RT_OLD
#define GROUNDBIAS_RT_OLD 0
#endif
static float groundMul = 1.f; static unsigned groundTexN;   // groundTexN: сколько текстур города помечено землёй (0 при старых данных: множитель ничего не делает)
#include "groundbias.h"
static inline float texSlope(const Tex *x) { return groundSlope(x->slope, x->ground, groundMul, GROUNDBIAS_RT_OLD); }
static float mipBias = MIP_BIAS;   // правка 416, служебное «flag mipbias X» (с dloff 1): сдвиг уровня мипа при выборе по глубине — замер мерцания дали
// сброс кэша состояний; заодно выключает отсечение граней (HUD, частицы, тень, фонари рассчитаны на выключенное) и сдвиг глубины слоёв
static void carToonHullFlush(void);
static void resetState(void) { flatOff(); carToonHullFlush(); curLvlMode = -1; bound = -2; texOn = -1; curFlags = -1; curFilt = -1; curSlope = -1.f; setCull(0); setBias(0); }   // 265: наклон мипа гостиной не переносится в город
static void texEnable(int on) {
  if (on == texOn) return; texOn = on;
  if (on) sceGuEnable(GU_TEXTURE_2D); else sceGuDisable(GU_TEXTURE_2D);
}
// номер текстуры куска: город/машина/пропы — tex[] уровня; комната (пункт 11.2) — RTEX0 + номер в блобе комнаты (rgTex)
#define RTEX0 0x4000
static Tex *rgTex; static unsigned rgNtex;
static Tex *cxTex; static unsigned cxNtex;   // В5: текстуры пакета collect-l1.shr (номер COLL_CTEX0 + k, viewer/collect.h)
static Tex *texAt(int t) {
  if (t == NOISE_TEX_ID) return noiseReady ? &noiseTex : 0;   // №912: процедурный шум земли
  if (t > NOISE_TEX_ID && t <= NOISE_TEX_ID + GD_NCLS) return gdReady[t - NOISE_TEX_ID] ? &gdTex[t - NOISE_TEX_ID] : 0;   // №969: рисунок земли по классам
  if (t >= COLL_CTEX0) { t -= COLL_CTEX0; return t < (int)cxNtex ? &cxTex[t] : 0; }
  if (t >= RTEX0) { t -= RTEX0; return t < (int)rgNtex ? &rgTex[t] : 0; }
  return t >= 0 && t < (int)ntex ? &tex[t] : 0;
}
// Правка 103: адреса, которые уходят в GE, — только VRAM или основная память. Пустой или битый адрес (пакет не прочитан, память
// отдана) эмулятор читает как нули и молчит, а GE настоящей PSP встаёт — и с ним вся приставка, HOME не отвечает. Кусок с таким
// адресом не рисуется, счётчик smkGeBad — в чёрный ящик (событие GEBAD) и в счётчик R+SELECT
static int geAddrOk(const void *p) {
  unsigned a = (unsigned)p & 0x1FFFFFFFu;
  return (a >= 0x04000000u && a < 0x04800000u) || (a >= 0x08000000u && a < 0x0C000000u);
}
static int pieceOk(const Piece *d, int far) {
  int ok = geAddrOk(d->v) && ((d->r.flags & F_NOIDX) || geAddrOk(far ? d->fidx : d->idx));
  if (ok && (opt & O_TEX)) { const Tex *x = texAt(d->r.tex);
    if (x) ok = geAddrOk(x->pal) && geAddrOk(x->pre || ((opt & O_SWZ) && x->slv[0]) ? x->slv[0] : x->lv[0]); }
  if (!ok) smkGeBad++;
  return ok;
}
// Правка 411: заглушка куска города, чей номер текстуры есть (>= 0), а самой текстуры в таблице нет (texAt пусто). Прежде GE
// рисовал его без текстуры цветом вершин (светлый, «палит»); теперь — ровным тоном горизонта уровня (fogCol), без дымки и без
// лишних проходов: 1×1 тексель, GU_TFX_REPLACE. Номер < 0 — кусок без текстуры по данным диска (тени, стёкла): цвет вершин, как был.
// Текстура с битым адресом (выгружена) не рисуется вовсе (pieceOk, правка 103) — за ней видно небо. «flag phold 1» — прежнее,
// «flag phplant 1» — служебно: у кусков города «нет текстуры» (замер заглушки, гейт tests/fartint)
#ifndef PH_OLD
#define PH_OLD 0
#endif
static int phAllow, phBoundAllow, phOld = PH_OLD, phPlant, phTfx; static unsigned phDraws;
static unsigned phTexel[4] __attribute__((aligned(16)));
static void phSetColor(unsigned rgb) { for (int i = 0; i < 4; i++) phTexel[i] = 0xff000000u | (rgb & 0xffffff); sceKernelDcacheWritebackRange(phTexel, sizeof phTexel); }
static unsigned tsBind, tsSame, tsDist, tsRebind, tsBytes, tsDistBytes, tsPh, tsSlope, tsConst, tsAutoN, tsSwz, tsLin, tsFrames, tsMaxDist, tsBadPrev;
// №657 «flag texvram N»: частые текстуры города (N КБ, 0 — выкл.) переезжают в свободный VRAM (после HUD и до истории jitter): GE читает их из видеопамяти быстрее, чем из основной.
// Набор выбирается по частоте: первые TV_WARM кадров считаем, в скольких кадрах текстура была в кадре, затем кладём по убыванию «кадров на байт». Меняется только slv[] (адрес для GE),
// lv[] остаётся в ОЗУ для чтения процессором (камера, пробы цвета). После переезда листы клеток и запечённые куски строятся заново (как flag groundbias). По умолчанию выкл.
#ifndef TEXVRAM_ON
#define TEXVRAM_ON 0
#endif
#define TV_WARM 150
#define TV_MIN 40
static int tvKB = TEXVRAM_ON; static unsigned tvBase, tvEnd, tvFrames, tvMovedN, tvMovedB, tvCand; static int tvDone; static const void *tvKey; static unsigned short tvUse[4096];
static void tvMigrate(void); static void tvRestore(void);
static unsigned tsPhB[BBP_N], tsPhR[BBP_N], tsPhD[BBP_N]; static int tsSite; static unsigned tsSiteB[8], tsSiteR[8];   // tsSite: источник привязок внутри фазы fx (1 маркеры цели, 2 кольца, 3 иконки, 4 прочее)   // по фазам кадра: привязки, повторы, разные текстуры
static int rbId = -9;   // id текстуры, чьи регистры оставила bindTexLean; заглушка phBind их перезаписывает (-9)
static void phBind(void) {
  rbId = -9; texEnable(1); binds++; phDraws++; if (smkOn) tsPh++;
  sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 1, 1, 4, phTexel); sceGuTexFlush();
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB); phTfx = 1;
  if (curFilt != GU_NEAREST) { curFilt = GU_NEAREST; sceGuTexFilter(GU_NEAREST, GU_NEAREST); }
  if (curLvlMode != 0) { sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvlMode = 0; }
}
// Правка «быстрый расчёт» (слово команды списка GE ≈ 0,5 мкс процессора, PHDRAW kw): привязка писала все регистры текстуры при каждой смене —
// режим, формат, размеры уровней, палитру — даже когда они совпадали с предыдущей текстурой. Теперь пропускаем неизменившиеся (режим, формат,
// размер уровня l, палитру), если с прошлой привязки через bindTexLean `bound` никто не сбрасывал (resetState/dlLost/другие рисовальщики ставят
// bound = -2, и тогда пишем всё). Адреса уровней, ширина строки и сброс кэша текстур — всегда. -DTEXREG_OLD=1 — прежнее
#ifndef TEXREG_OLD
#define TEXREG_OLD 1   // ВЫКЛЮЧЕНО по умолчанию (10.10, TEST8: «всё размыто»): правило держалось на том, что любой чужой код сбрасывает bound = -2 перед своей текстурой; раньше полная запись регистров защищала и от забытого сброса. Включить: -DTEXREG_OLD=0
#endif
// №657 (учёт текстур в кадре, только смоук/тур): привязок, повторов (текстура уже была привязана в этом кадре), разных текстур и их объём (уровень 0, байт), заглушек (текстуры нет — «выскакивание»),
// ветвей мипа. TEXSTAT раз в секунду в smoke.txt (на карту тура), по кадру: tsFrame() в начале кадра. Вне смоука счёт не идёт
static unsigned tsSeen[4096]; static unsigned tsStamp = 1; static unsigned tsFrBind, tsFrDist;
static void tsFrame(void) {
  tsSite = 0;
  { const void *key_ = (tex && ntex) ? tex[0].lv[0] : 0;   // новый уровень — таблица текстур другая: переезд и счёт заново
    if (key_ != tvKey) { tvKey = key_; tvDone = 0; tvFrames = 0; tvMovedN = tvMovedB = 0; memset(tvUse, 0, sizeof tvUse); }
    if (tvKB > 0 && !tvDone && key_ && ++tvFrames >= TV_WARM) tvMigrate(); }
  if (!smkOn) return;
  if (tsFrDist > tsMaxDist) tsMaxDist = tsFrDist;
  tsFrBind = tsFrDist = 0; tsStamp++; tsFrames++;
  static unsigned t0_; unsigned now_ = sceKernelGetSystemTimeLow(); if (!t0_) t0_ = now_;
  if (now_ - t0_ >= 1000000u && tsFrames) {
    float f_ = 1.f / (float)tsFrames; char l_[320];
    snprintf(l_, sizeof l_, "TEXSTAT t=%.1f frames=%u bind=%.1f same=%.1f distinct=%.1f maxdist=%u rebind=%.1f kb=%.0f distinct_kb=%.0f ph=%.2f bad=%u slope=%.1f const=%.1f auto=%.1f swz=%.1f lin=%.1f vram_tex=%u vram_kb=%u\n",
             smkT, tsFrames, tsBind * f_, tsSame * f_, tsDist * f_, tsMaxDist, tsRebind * f_, tsBytes * f_ / 1024.f, tsDistBytes * f_ / 1024.f, tsPh * f_, smkGeBad - tsBadPrev,
             tsSlope * f_, tsConst * f_, tsAutoN * f_, tsSwz * f_, tsLin * f_, tvMovedN, tvMovedB / 1024);
    smkWrite(l_); tsBadPrev = smkGeBad;
    { static const int ph_[] = {BBP_CITY, BBP_CAR3D, BBP_NPC3D, BBP_TRF3D, BBP_MCR3D, BBP_PROPS3D, BBP_FX3D, BBP_UI}; static const char *nm_[] = {"city", "car", "npc", "trf", "mcr", "props", "fx", "ui"};
      int n_ = snprintf(l_, sizeof l_, "TEXPH t=%.1f", smkT);
      for (int i = 0; i < 8 && n_ < (int)sizeof l_ - 24; i++) n_ += snprintf(l_ + n_, sizeof l_ - n_, " %s=%.1f/%.1f", nm_[i], tsPhB[ph_[i]] * f_, tsPhR[ph_[i]] * f_);
      for (int i = 0; i < 5; i++) n_ += snprintf(l_ + n_, sizeof l_ - n_, " fxsite%d=%.1f/%.1f", i, tsSiteB[i] * f_, tsSiteR[i] * f_);
      snprintf(l_ + n_, sizeof l_ - n_, " (привязок/повторов на кадр)\n"); smkWrite(l_); memset(tsPhB, 0, sizeof tsPhB); memset(tsPhR, 0, sizeof tsPhR); memset(tsSiteB, 0, sizeof tsSiteB); memset(tsSiteR, 0, sizeof tsSiteR); }
    tsBind = tsSame = tsDist = tsRebind = tsBytes = tsDistBytes = tsPh = tsSlope = tsConst = tsAutoN = tsSwz = tsLin = tsFrames = tsMaxDist = 0; t0_ = now_;
  }
}
static int texRegOld = TEXREG_OLD;   // №657: «flag texreg 1» — меньше слов GE на привязку (регистры, что не менялись, не пишутся); по умолчанию как в сборке (TEXREG_OLD=1 — полная запись)
static unsigned rbMode, rbFmt, rbSize[8], rbPal;
static void bindTexLean0(int t) {
  if (!(opt & O_TEX)) t = -1;
  if (phPlant && phAllow && t >= 0) t += 0x2000;   // служебное: номер вне таблицы (texAt пусто), не RTEX0/COLL_CTEX0
  if (t == bound && phAllow == phBoundAllow) { if (smkOn) tsSame++; return; } int prevBound = bound; bound = t; phBoundAllow = phAllow;
  Tex *x = texAt(t);
  if (!x) { if (t >= 0 && phAllow && !phOld) { phBind(); return; } texEnable(0); return; }
  if (phTfx) { sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); phTfx = 0; }
  int swz = x->pre || ((opt & O_SWZ) && x->slv[0]);
  { int ok = geAddrOk(x->pal) && x->levels >= 1 && x->levels <= 8;   // правка 103: пустая палитра или уровень (текстуры выгруженной модели,
    for (int l = 0; ok && l < x->levels; l++) ok = geAddrOk(swz ? x->slv[l] : x->lv[l]);   // mcrDrop) — GE железа встал бы целиком
    if (!ok) { smkGeBad++; if (phAllow && !phOld) phBind(); else texEnable(0); return; } }   // правка 411: город — тоном горизонта, не цветом вершин
  texEnable(1); binds++;
  if (smkOn || (tvKB > 0 && !tvDone)) { tsBind++; tsFrBind++; unsigned kb_ = (unsigned)(x->w * x->h) >> (x->t4 ? 1 : 0); tsBytes += kb_; swz ? tsSwz++ : tsLin++;
    int ph_ = (int)bbPhase; if (ph_ < 0 || ph_ >= BBP_N) ph_ = 0; tsPhB[ph_]++; if (ph_ == BBP_FX3D) { tsSiteB[tsSite & 7]++; } unsigned *sn_ = &tsSeen[(unsigned)t & 4095]; if (*sn_ == tsStamp) { tsRebind++; tsPhR[ph_]++; if (ph_ == BBP_FX3D) tsSiteR[tsSite & 7]++; } else { *sn_ = tsStamp; tsDist++; tsFrDist++; tsDistBytes += kb_; if ((unsigned)t < 4096 && tvUse[t] < 65535) tvUse[t]++; } }
  unsigned p = (unsigned)x->pal;
  int regOk = !texRegOld && prevBound == rbId && rbId != -9;   // регистры те же, что оставила прошлая привязка этой функции
  unsigned mode = ((x->levels - 1) << 16) | swz, fmt = x->t4 ? GU_PSM_T4 : GU_PSM_T8;
  if (!regOk) memset(rbSize, 0xff, sizeof rbSize);   // неизвестно, что в регистрах уровней
  if (!regOk || p != rbPal) { sceGuSendCommandi(0xb0, p & 0xffffff); sceGuSendCommandi(0xb1, (p >> 8) & 0xf0000); sceGuSendCommandi(0xc4, 32); rbPal = p; }
  if (!regOk || mode != rbMode) { sceGuSendCommandi(0xc2, mode); rbMode = mode; }       // TEX_MODE: уровни, swizzle
  if (!regOk || fmt != rbFmt) { sceGuSendCommandi(0xc3, fmt); rbFmt = fmt; }            // TEX_FORMAT (правка 129: T4 — 16 цветов CLUT)
  int lw = x->w, lh = x->h;
  for (int l = 0; l < x->levels; l++) {
    unsigned a = (unsigned)(swz ? x->slv[l] : x->lv[l]), sz = (ilog2(lh) << 8) | ilog2(lw);
    sceGuSendCommandi(0xa0 + l, a & 0xffffff);
    sceGuSendCommandi(0xa8 + l, ((a >> 8) & 0x0f0000) | texTbw(x->t4, lw));   // TBW >= 16 Б (мипы < 16 px, Ф2; T4 — 32 тексела)
    if (!regOk || sz != rbSize[l & 7]) { sceGuSendCommandi(0xb8 + l, sz); rbSize[l & 7] = sz; }
    lw >>= 1; lh = lh > 1 ? lh >> 1 : 1;
  }
  rbId = t;
  sceGuSendCommandi(0xcb, 0);                                    // один TEX_FLUSH на привязку
  int f = x->levels > 1 ? ((opt & O_TRI) ? GU_LINEAR_MIPMAP_LINEAR : GU_LINEAR_MIPMAP_NEAREST) : GU_LINEAR;
  if (f != curFilt) { curFilt = f; sceGuTexFilter(f, GU_LINEAR); }
#ifndef TRF_MIPBIAS_OLD
#define TRF_MIPBIAS_OLD 0
#endif
  if (smkOn && x->levels > 1) { if (mipConst >= 0) tsConst++; else if (x->slope > 0.f && !mipAuto) tsSlope++; else tsAutoN++; }
  if (mipConst >= 0 && x->levels > 1) { sceGuTexLevelMode(GU_TEXTURE_CONST, (float)(mipConst < x->levels ? mipConst : x->levels - 1)); curLvlMode = -1; }
  else if (x->slope > 0.f && x->levels > 1 && !mipAuto) {   // правка 77/88: мип по глубине пикселя (наклон из level.shr)
    if (curLvlMode != 1) { sceGuTexLevelMode(GU_TEXTURE_SLOPE, mipBias); curLvlMode = 1; }
    { float sl_ = texSlope(x); if (sl_ != curSlope) { sceGuTexSlope(sl_); curSlope = sl_; } }
  } else {
    float bias = 0.f;
#if !TRF_MIPBIAS_OLD
    if (haveTraffic && t >= (int)trTexBase) bias = -0.5f;   // №558: чёткие мипы для текстур машин трафика вдали
#endif
    int target = (bias != 0.f) ? 2 : 0;
    if (curLvlMode != target) { sceGuTexLevelMode(GU_TEXTURE_AUTO, bias); curLvlMode = target; }
  }
}
// №912: заливка вместо текстуры (toonflat.h). Свет GE без источников: окружающий цвет модели = заливка, материал ambient = цвет вершины (результат — заливка × цвет вершины)
static void flatLightOn(unsigned amb) {
  if (flatLit != 1) { sceGuColorMaterial(GU_AMBIENT); sceGuEnable(GU_LIGHTING); flatLit = 1; flatTouched = 1; flatAmbOk = 0; flatSwitches++; }
  if (!flatAmbOk || amb != flatAmb) { sceGuAmbient(amb); flatAmb = amb; flatAmbOk = 1; }
}
// №949: масштаб текстуры шума куска (toonflat.h nz*): тайл шума noisePitch метров. Код куска nzPend ставит NZP(d) перед привязкой; действует на одну привязку
static unsigned nzHash;   // хэш начала куска (NZP): вариант отображения земли №977
static inline void nzApply(int gc) {   // gc — класс земли №969 (0 — шум №912): у него свой тайл в метрах (gdPitch); у фрагмента асфальта (gdAsp) — свой вариант на кусок
  unsigned w = nzPend & 0xffffu; nzPend = 0; if (!nzOn) return; unsigned v = (gc && gdAsp[gc]) ? ((nzHash ^ (nzHash >> 15)) & 0x1ffu) : 0u; int key = (int)w | gc << 16 | (int)(v << 20); if (key == nzCur) return;
  if (!w) { nzReset(); return; }
  float ip = 128.f / (gc ? gdPitch[gc] : noisePitch), su = nzLut[w & 255] * ip, sv = nzLut[(w >> 8) & 255] * ip;
  if (!(w & 255)) su = 128.f; if (!((w >> 8) & 255)) sv = 128.f;
  if (v) { static const float zm[3] = { 1.f, .8f, .625f }; float z = zm[(v >> 2) % 3]; su *= z * ((v & 1) ? -1.f : 1.f); sv *= z * ((v & 2) ? -1.f : 1.f); sceGuTexOffset((float)((v >> 4) & 31) / 32.f, (float)((v * 7u + 3u) & 31) / 32.f); nzOffOn = 1; }
  else if (nzOffOn) { sceGuTexOffset(0.f, 0.f); nzOffOn = 0; }
  sceGuTexScale(su, sv); nzCur = key; nzBinds++;
}
#define NZP(d) (nzPend = (d)->nz, nzHash = ((unsigned)(int)((d)->r.ox * 8.f) * 73856093u) ^ ((unsigned)(int)((d)->r.oz * 8.f) * 19349663u) ^ ((unsigned)(d)->r.tex * 83492791u))
static void bindTexLean(int t) {
  int fk = (!flatOn || !(opt & O_TEX)) ? 0 : flatKind(t);
  if (!fk) { nzPend = 0; if (flatLit != 0 && flatTouched) flatOff(); else flatLit = 0; if (nzCur) nzReset(); bindTexLean0(t); return; }
  int gc = fk == 3 ? flatGd[t] : 0;   // №969: вид 3 — рисунок земли, gc — класс (газон, тротуар)
  if (fk >= 2) nzApply(gc); else nzPend = 0;
  int eff = (int)(flatExact[t] | (unsigned)fk | (unsigned)gc << 26);   // ключ состояния заливки: точный тон (24 бита), вид и класс рисунка; бит 30 — признак
  if (eff == bound && phAllow == phBoundAllow && flatLit == 1) { if (smkOn) tsSame++; return; }
  if (fk >= 2) { if (!(bound >= 0x40000000 && (bound & 3) == fk && ((bound >> 26) & 15) == gc)) { bound = -2; bindTexLean0(NOISE_TEX_ID + gc); }
    if (TOON_NOISE_WORLD && fk == 2) { if (flatMap != 1) { sceGuTexMapMode(GU_TEXTURE_MATRIX, 0, 0); sceGuTexProjMapMode(GU_POSITION); flatMap = 1; } if (!flatInList && flatMdlGen != flatTexGen) flatTexEmit(); }
    flatNoiseN++; } else { texEnable(0); rbId = -9; }
  if (fk >= 2 && noiseNoLight) { if (flatLit == 1) { sceGuDisable(GU_LIGHTING); sceGuColorMaterial(0); flatLit = 0; flatAmbOk = 0; } } else flatLightOn(flatTint[t]);
  bound = eff; phBoundAllow = phAllow; flatBinds++;
}
static int curCull;
static void setCull(int on) {
  if (on == curCull) return; curCull = on;
  if (on) sceGuEnable(GU_CULL_FACE); else sceGuDisable(GU_CULL_FACE);
}
// Ф2: кусок-«наклейка» (флаг 64, tools/psp_export.py mark_layers): лежит в плоскости другой непрозрачной
// поверхности; 16-бит глубина даёт им одно значение, и они мигают. Рисуем со сдвигом глубины к камере на LAYER_BIAS
// единиц (sceGuDepthOffset = сдвиг Z_CENTER, аналог polygon offset; при GEQUAL больше = ближе). curBias — что сейчас в GE.
// Правки Роба 37/43 (29.09): слоем считается зазор до 10 см (16-бит глубина не различает его на дальности ближних версий),
// наклейка на наклейке — сдвиг × этаж (флаги 256 = +1, 1024 = +2); слой со стороной (флаг 512) обёрнут экспортом лицом от подложки и рисуется
// с отсечением изнанки — со стороны подложки сдвиг не протаскивает его сквозь неё.
#define F_LAYER 64
#define F_SHELL 128   // кусок комнаты с обходом «лицом в комнату» (tools/roomorient.py, правка Роба 27): рисуется с отсечением изнанки
#define F_LAYER2 256  // этаж слоя +1: сдвиг LAYER_BIAS × этаж
#define F_LAYER3 1024 // этаж слоя +2 (стопки до 4 этажей)
#define F_LSIDE 512   // слой со стороной: отсечение изнанки (обход — лицом от подложки)
#define F_NEAR 2048   // правки 224/237/251: кусок рисуется ближней версией на любой дальности (tools/psp_export.py, дальней нет)
#define F_SKYV 4096   // №511: слой неба с v в [0, 1] (tools/psp_export.py F_SKYV): рисуется с GU_CLAMP по v, верх и низ слоя не смешиваются билинейно через край
// №511 (швы неба, слово Роба: «PNG плохо наслоен на небо, местами прямой шов»): слой холмов (верх непрозрачный, низ прозрачный) при GU_REPEAT по v на нижнем и
// верхнем крае смешивался с противоположной строкой — волосяная линия вдоль горизонта на всём круге, шире на мипах. Кайма края слоя (цвет невидимых пикселей PNG) —
// экспорт (tools/texq.py bleed_rgb). SKY_SEAM_OLD=1 — прежнее (REPEAT по v у всех слоёв); для старых данных (флага F_SKYV нет) поведение прежнее и так
#ifndef SKY_SEAM_OLD
#define SKY_SEAM_OLD 0
#endif
// №447: authored additive meshes (BLMD=2); SPR1 bits 8..23 remain joint/slot.
#define F_ADDITIVE (1 << 24)
// Слово Роба 15.10 («лучше дальность прорисовки, чем фонарики и неоны»; на причале L3/L6 неон ломается и мигает): свечения со сложением — квады-ореолы
// стейтпропов (фонари, неон, лучи) и небольшие аддитивные куски (световые пятна, неоновые вывески, до NEON_GLOW_MAXTRI треугольников) — по умолчанию
// не рисуются; большие (мерцание воды) остаются. NEON_GLOW_ON=1 при сборке или «flag neonglow 1» — как раньше. Мигание по кадрам PVIS (огни барьеров) не трогаем
#ifndef NEON_GLOW_ON
#define NEON_GLOW_ON 0
#endif
#ifndef NEON_GLOW_MAXTRI
#define NEON_GLOW_MAXTRI 64
#endif
static int neonGlowOn = NEON_GLOW_ON;
// №933 служебно (инвентарь светящегося текстурой/сложением): GLOWTOP раз в секунду — что ещё рисуется как свет не вектором. src: фаза кадра для кусков со сложением (A<фаза>), «B» — билборд-квад (текстура, режим BLMD, PVIS)
// №1052 (Роб 27.10: «в будке внизу осталось размытое мигание, у нас только векторное свечение»; «любое такое свечение удали»): размытые пятна с текстурой и сложением не рисуются нигде. Инвентарь (кадры 26.10, L1 будка, L5, L7; журнал GLOWTOP):
// билборды-ореолы mode 2 — уже блик или ничего (№839, neonGlowOn 0), таблица света lightsprite.h при slLightOn — ореолы и знаки mode 0/1 не рисуются, остаются конусы и лучи (вектор), аддитивные куски города до 64 треугольников — векторные звёзды (spritelight.h),
// остаётся одно место: аддитивный кусок с текстурой крупнее 64 треугольников (owner 0) рисовался прямо — теперь нет (конусы ламп и лучи идут таблицей света и вектором). GLOWOFF1052_OLD=1 / файл glowoff1052old / flag glowoff1052 0 — как в TEST73
#ifndef GLOWOFF1052_OLD
#define GLOWOFF1052_OLD 0
#endif
static int glowOff1052Old = GLOWOFF1052_OLD;
#define GLOWOFF1052_SKIP(d) (!glowOff1052Old && ((d)->r.flags & F_ADDITIVE) && (d)->r.owner == 0 && (d)->r.tex >= 0)
static int gloSite;   // №933 служебно: 1 пропы, 2 стейтпропы, 3 собираемое (collDraw), 0 прочее — откуда рисуется кусок (GLOWBIG)
static struct { char src; int a, b, c; unsigned n, tris; } gloT[24]; static int gloN; static float gloT0; static unsigned gloFr;
static void gloAdd(char src, int a, int b, int c, unsigned tris) {
  if (!smkOn) return;
  if (smkT - gloT0 >= 1.f || smkT < gloT0) { if (gloN) { char l[700]; int o = snprintf(l, sizeof l, "GLOWTOP t=%.1f sc=%s", smkT, smkScene);
      for (int i = 0; i < gloN; i++) for (int j = i + 1; j < gloN; j++) if (gloT[j].n > gloT[i].n) { __typeof__(gloT[0]) x = gloT[i]; gloT[i] = gloT[j]; gloT[j] = x; }
      for (int i = 0; i < gloN && i < 8 && o < 620; i++) o += snprintf(l + o, sizeof l - (size_t)o, " [%c tex=%d %d %d n=%u tris=%u]", gloT[i].src, gloT[i].a, gloT[i].b, gloT[i].c, gloT[i].n, gloT[i].tris);
      snprintf(l + o, sizeof l - (size_t)o, "\n"); smkWrite(l); } gloN = 0; gloT0 = smkT; }
  int k = 0; for (; k < gloN; k++) if (gloT[k].src == src && gloT[k].a == a && gloT[k].b == b && gloT[k].c == c) break;
  if (k == gloN) { if (gloN >= 24) return; gloT[k].src = src; gloT[k].a = a; gloT[k].b = b; gloT[k].c = c; gloT[k].n = gloT[k].tris = 0; gloN++; }
  gloT[k].n++; gloT[k].tris += tris;
}
static unsigned dbgAddN, dbgAddTri, dbgAddBig;   // №703 (диагностика): аддитивные куски города, прошедшие NEON_SKIP за кадры с прошлого «flag addstat»
static int slLightOn, slLightResetReq;   // №658 (viewer/spritelight.h): flag lightspr 1 — авторские световые куски города рисует таблица спрайтов; сброс таблицы при смене уровня
static int glowBias;   // TEST19 (диагностика мигания света): «flag glowbias N» — добавочный сдвиг глубины N единиц к камере для всех кусков и билбордов со сложением (световые пятна лежат в 8 см над землёй, 16-бит глубина их не различает); 0 — как есть
#define NEON_SKIP(d) ((!neonGlowOn || slLightOn) && ((d)->r.flags & F_ADDITIVE) && (d)->r.owner == 0 && (d)->r.ni <= 3 * NEON_GLOW_MAXTRI)
// №757 (Роб 17.10: «под объектами осталась облупленная тень, которая всё сжирает»; «нам не нужны тени деревьев, можем без них обойтись»): нарисованные тени
// деревьев уровня — наклейки на земле treeshadow.bmp (L1: рваная тёмная клякса, альфа до 167; 1040 треугольников, ~29 600 м² смешивания) и
// treeshad_overcast.bmp (L2: шумное пятно, 307 треугольников) — не рисуются. Текстура узнаётся при загрузке уровня по альфе (treeShScan, main_levels.h):
// по уровням TEST31 ровно одна на уровень (L1 №131, L2 №234), на L3–L7 нет. TREESH_OLD=1 / файл treeshold рядом с EBOOT / «flag treesh 1» — рисовать как было
#ifndef TREESH_OLD
#define TREESH_OLD 0
#endif
static unsigned char *texNoDraw; static unsigned texNoDrawN, treeShSkipped; static int treeShOn = TREESH_OLD;
#define TREESH_SKIP(d) (!treeShOn && (unsigned)(d)->r.tex < texNoDrawN && texNoDraw[(d)->r.tex] && (++treeShSkipped, 1))
#define F_FARP (1 << 25)   // правка 508: у куска города есть дальняя версия без нарезки (nf), её рисуют дальше farPlainD (файл far_plain)
#ifndef GLOW_BLEND_OLD
#define GLOW_BLEND_OLD 0
#endif
static int glowBlendOld = GLOW_BLEND_OLD;
static int glowFogBegin(int fl) {
  int fog = !glowBlendOld && (fl & F_ADDITIVE) && sceGuGetStatus(GU_FOG);
  if (fog) sceGuDisable(GU_FOG);
  return fog;
}
static void glowFogEnd(int fog) { if (fog) sceGuEnable(GU_FOG); }
#define LAYER_BIAS 6
static int curBias, layerBias = LAYER_BIAS;   // служебный файл nolayer рядом с EBOOT -> 0 (сравнение)
static void setBias(int b) { if (b != curBias) { curBias = b; sceGuDepthOffset(b); } }
static void setFlags(int fl) {
  fl &= 7 | F_ADDITIVE | F_DOUBLE | F_LAYER | F_SHELL | F_LAYER2 | F_LAYER3 | F_LSIDE; if (fl == curFlags) return; curFlags = fl;
  if (fl & 1) sceGuEnable(GU_ALPHA_TEST); else sceGuDisable(GU_ALPHA_TEST);
  if (fl & 2) {
    sceGuEnable(GU_BLEND);
    if (!glowBlendOld && (fl & F_ADDITIVE)) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
    else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  } else sceGuDisable(GU_BLEND);
  sceGuDepthMask((fl & 6) ? GU_TRUE : GU_FALSE);                 // прозрачное и небо глубину не пишут
  setCull((((opt & O_CULL) || (fl & F_SHELL)) && !(fl & (4 | F_DOUBLE))) || (fl & F_LSIDE));   // небо и двусторонние — без отсечения; комнаты (128) и слои со стороной (512) — с ним
  setBias(((fl & F_LAYER) ? layerBias * (1 + !!(fl & F_LAYER2) + 2 * !!(fl & F_LAYER3)) : 0) + ((fl & F_ADDITIVE) ? glowBias : 0));
}
// старый путь v7 (для сравнения в тесте: вариант LEGACY)
static void bindTexLegacy(int t) {
  if (!(opt & O_TEX)) t = -1;
  if (t == bound) return; bound = t;
  if (t < 0 || t >= (int)ntex) { sceGuDisable(GU_TEXTURE_2D); texOn = 0; return; }
  Tex *x = &tex[t]; int swz = x->pre || ((opt & O_SWZ) && x->slv[0]); binds++;
  sceGuEnable(GU_TEXTURE_2D); texOn = 1;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, x->pal);
  sceGuTexMode(x->t4 ? GU_PSM_T4 : GU_PSM_T8, x->levels - 1, 0, swz);
  int lw = x->w, lh = x->h;
  for (int l = 0; l < x->levels; l++) { sceGuTexImage(l, lw, lh, texTbw(x->t4, lw), swz ? x->slv[l] : x->lv[l]); lw >>= 1; lh = lh > 1 ? lh >> 1 : 1; }
  if (x->slope > 0.f && x->levels > 1 && !mipAuto) { sceGuTexLevelMode(GU_TEXTURE_SLOPE, mipBias); { float sl_ = texSlope(x); sceGuTexSlope(sl_); curSlope = sl_; } curLvlMode = 1; }   // правка 77/88: мип по глубине пикселя — без ступенек между треугольниками
  else { sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvlMode = 0; }
  sceGuTexFilter(x->levels > 1 ? ((opt & O_TRI) ? GU_LINEAR_MIPMAP_LINEAR : GU_LINEAR_MIPMAP_NEAREST) : GU_LINEAR, GU_LINEAR);
  sceGuTexOffset(0.f, 0.f); curFilt = -1;
}

// ---------- отрисовка кусков ----------
static long drawnTris; static int draws;
// far: 0 ближняя версия, 1 дальняя. Без индексов (флаг 16): поток вершин подряд, дальняя — после ni ближних
// гейт «КАДР ИГРОКА» (tools/frame_gate.sh): сколько геометрии РЕАЛЬНО ушло в GE по категориям кадра (фаза bbPhase), а не сколько вызовов начато:
// 16.10 drawPieceM считал draws++ и не рисовал ничего (кузова машин, динамические пропы пропадали, счётчики вызовов росли). Строка GATECAT раз в секунду (main_loop.h)
static unsigned gateGeo[24], gateTri[24], gateUntex[24];
static inline void gateCount(int cnt) { int p = bbPhase; if (p < 0 || p >= 24) p = 0; gateGeo[p]++; gateTri[p] += (unsigned)cnt / 3u; if (!texOn) gateUntex[p]++; }
#ifndef GG_LOW_H
#define GG_LOW_H 0.f   // по умолчанию выкл; в TEST29 — 2.5f после гейта
#endif
// №674 diaglog: журнал отрисовки диагональных вспышек (флаг сценария «diaglog», только smokeOn). Треугольники, которые уходят в GE БЕЗ среза ggCut и под риском защитной полосы
// (ggRisk: вершина ближе near или дальше GG_SAFE px от центра, треугольник касается кадра): GE выбрасывает такой треугольник целиком или рисует клин. Одна строка DIAGF на кадр с такими треугольниками
static int diagAbsM = -1;   // №921: 1 — матрица куска мировая (минус глаз), 0 — уже от глаза (город: drawPiece), -1 — угадать по переносу (прежнее правило diagGeo ошибалось у кусков города: перенос включает центр куска)
static float ggLowH = GG_LOW_H; static unsigned ggLowFrames; static int diagOn; static ScePspFMatrix4 diagM; static int diagMok;
static void diagGeo(const Piece *d, int far, int cnt);
#undef sceGuSetMatrix
#define sceGuSetMatrix(t, m) (diagOn && (t) == GU_MODEL ? (void)(diagM = *(const ScePspFMatrix4 *)(m), diagMok = 1) : (void)0, flatCapture(t, m), geSetMatrixSafe(t, m))
static void drawGeo(const Piece *d, int far, int cnt) {
  gateCount(cnt);
  if (diagOn) diagGeo(d, far, cnt);
  if (d->r.flags & F_STRIP) { stripDraw(d, far, (d->r.flags & F_V12) ? VT12 : VT16); return; }   // К4: полосы, одна команда на полосу (stripdraw.h)
  if (d->r.flags & F_NOIDX) {
    int vs = (d->r.flags & F_V12) ? 12 : 16, vt = (d->r.flags & F_V12) ? VT12 : VT16;
    const char *vb = (const char *)d->v + (far ? d->r.ni * vs : 0);
    for (int k = 0; k < cnt; k += 65535) {
      int n = cnt - k; if (n > 65535) n = 65535;
      sceGuDrawArray(GU_TRIANGLES, vt, n, 0, vb + k * vs);
    }
    return;
  }
  unsigned short *ix = far ? d->fidx : d->idx;
  for (int k = 0; k < cnt; k += 65532) {
    int n = cnt - k; if (n > 65532) n = 65532;
    sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | VT16, n, ix + k, d->v);
  }
}
// Правка Роба 340 (viewer/ge_guard.h): «по краям экрана иногда обламывается тень от деревьев на траве». Пешком у стены/дерева/машины
// ближняя плоскость кадра опускается до 0,25–0,3 м, защитная полоса GE у near сужается до ±2,3 м, и треугольник земли или тени
// (7 м), уходящий краем за камеру, GE выбрасывает целиком. В такие кадры (ggOn) кусок города, чей бокс не целиком в пирамиде
// полосы, рисуется по треугольникам: безопасные — подряд одним DrawArray, под риском — срезом на процессоре, веером.
// Прежнее — «flag ggold 1» (или сборка -DGG_OLD=1); «flag ggold 2» — правило в любом кадре (служебно, гейт)
#ifndef GG_OLD
#define GG_OLD 0
#endif
typedef struct { unsigned short u, v; unsigned c; float x, y, z; } GgVert;
#define GG_VT (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
static int ggBatchOld=GG_BATCH_OLD,ggBatchCapture;
static int ggMode = GG_OLD, ggOn, ggLog; static GgCam ggK; static float ggE[3], ggR[3], ggU[3], ggF[3];
// №912: текстурная матрица шума земли из матрицы модели (мир = M·p + глаз; u = мир.x / шаг, v = мир.z / шаг; целая часть сдвига отброшена — тайл повторяется)
static void flatTexEmit(void) {
  if (!flatMdlOk) return; flatTexGen = flatMdlGen; flatTexEmitN++; const float *m = flatMdl; const float ip = 1.f / NOISE_PITCH;
  float tu = (m[12] + ggE[0]) * ip, tv = (m[14] + ggE[2]) * ip; tu -= floorf(tu); tv -= floorf(tv);
  const float g = noiseUvG;   // множитель координат: у кадра sceGuTexScale(128, 128) — проверено замером, нужен ли делитель 128 («flag noiseg»)
  ScePspFMatrix4 T = {{m[0] * ip * g, m[2] * ip * g, 0, 0}, {m[4] * ip * g, m[6] * ip * g, 0, 0}, {m[8] * ip * g, m[10] * ip * g, 0, 0}, {tu * g, tv * g, 0, 1}};
  geSetMatrixSafe(GU_TEXTURE, &T);
}
static inline void flatCapture(int t, const void *mp) {
  if (t != GU_MODEL || !flatOn) return; memcpy(flatMdl, mp, 64); flatMdlOk = 1; flatMdlGen++;
  if (TOON_NOISE_WORLD && flatMap == 1 && !flatInList) flatTexEmit();
}

static int ggBigOnly; static unsigned ggBigFrames;   // №840: срез включён только правилом «крупные треугольники у камеры» — режутся лишь помеченные куски (main_state.h)
#define GG_PIECE_OK(e) (!ggBigOnly || ((e)->r.owner == 0 && (e)->r.pad[1] == 1u))
#define GG_CELL_OK(ci) (!ggBigOnly || (cellBigF && cellBigF[ci]))
static int ggTrisF, ggPiecesF, ggCellsF, ggExamF; static unsigned ggTrisAll, ggPiecesAll, ggCellsAll, ggFramesOn, ggExamAll, ggUsAll, ggUsMax, ggUsF; static float ggNearMin = 1e9f;
static int ggBoxRiskW(const float *bb) {   // бокс мира (мин xyz, макс xyz) -> ggBoxRisk (ge_guard.h) в осях камеры кадра
  return ggBoxRiskAx(bb, ggE, ggR, ggU, ggF, &ggK);
}
// render_fast О11 (viewer/render_fast.h): клетка «в риске» — есть ли у неё непрозрачный кусок, который drawGeoG резал бы (то же
// условие, что в начале drawGeoG). Нет — клетка идёт листом: drawGeoG все её куски и так отдал бы GE целиком
static int rfOld = RF_OLD; static unsigned rfGgListF, rfGgList, rfDlSkip, rfSpCut, rfSpPose, rfCullUs, rfSpUs, rfFrames;
static int ggCellCut(unsigned ci) {
  const Cell *c = &cl[ci];
  for (unsigned k = c->first; k < c->first + c->n; k++) { const Piece *e = &pc[k];
    if (e->r.ni <= 0 || ((e->r.flags & 2) && !(e->r.flags & 4))) continue;   // полупрозрачное — очередью tq, не листом
    if (e->r.owner == 0 && (e->r.flags & F_NOIDX) && e->r.ni <= 65535 && GG_PIECE_OK(e) && ggBoxRiskW(e->r.bb)) return 1; }
  return 0;
}
// render_fast О2: радиус сферы типа стейтпропа при любой позе (render_fast.h) — один раз на тип; сброс при новом SPR1 (rfSpReset)
static float *rfSpR; static unsigned rfSpN;
static void cellHybFreeFwd(void); static void ggcClear(void); static void spGridReset(void); static void pbResetFwd(void); static void crvResetFwd(void);
static void rfSpReset(void) { free(rfSpR); rfSpR = 0; rfSpN = 0; ggcClear(); spGridReset(); pbResetFwd(); crvResetFwd(); cellHybFreeFwd(); }
// 17.10 (терминал рендера, «облегчить процессору»): отсечение по размеру на экране, как Min Screen Radius / LOD cull ratio в больших движках. Стейтпроп, чья сфера
// на экране меньше spMinPx пикселей (радиус · фокус / дальность), не рисуется: знак или урна в 150 м меньше пикселя, а процессору стоит ≈17 мкс на кусок.
// Фокус по высоте кадра 272 px при угле обзора ≈55°. SP_MIN_PX_OLD=1 / «flag spminpx 0» — как было
#ifndef SP_MIN_PX_OLD
#define SP_MIN_PX_OLD 0
#endif
#define SP_FOCAL_PX 261.f
// Роб 17.10: «деревья и кусты вдали без качания»: стейтпроп с одним состоянием (зацикленная анимация) дальше SP_SWAY_FAR м позу не пересчитывает — замирает.
// SP_SWAY_FAR_OLD=1 / «flag swayfar 0» — качание на любой дальности, как было
#ifndef SP_SWAY_FAR_OLD
#define SP_SWAY_FAR_OLD 0
#endif
#define SP_SWAY_FAR 25.f
static int spSwayFar = !SP_SWAY_FAR_OLD;
static float spMinPx = SP_MIN_PX_OLD ? 0.f : 1.5f; static unsigned spSmallCut;
static float rfSpRadius(unsigned t) {
  if (!rfSpR && sprops.ntype) { rfSpR = malloc(sprops.ntype * sizeof *rfSpR); if (!rfSpR) return -1.f;
    rfSpN = sprops.ntype; for (unsigned i = 0; i < rfSpN; i++) rfSpR[i] = rfSpTypeRadius(&sprops, i); }
  return t < rfSpN ? rfSpR[t] : -1.f;
}
// кусок без индексов с матрицей модели m (absM: перенос в m мировой — вычесть глаз; иначе уже от глаза, город в осях камеры)
// «Быстрый расчёт» (L7 пешком: срез у камеры gg_ms 2,5 мс, ct_risky 3,4): вершины среза писались в список GE (sceGuGetMemory) — список пишется
// мимо кэша, слово ≈ 1 мкс (PHDRAW kw), а полигон среза — 5 слов на вершину. Кольцо вершин кадра в обычной (кэшируемой) памяти: пишем быстро,
// сброс кэша в память перед каждой командой рисования (№863: GE читает сразу, не после sceGuFinish); не влезло — как раньше (sceGuGetMemory). Кольцо
// обнуляется в начале кадра. -DGG_VB_OLD=1 — прежнее
#ifndef GG_VB_OLD
#define GG_VB_OLD 0
#endif
#define GGVB_BYTES (96u << 10)
static unsigned char ggVb[GGVB_BYTES] __attribute__((aligned(64))); static unsigned ggVbUsed;
static void *ggVbAlloc(unsigned bytes) {
  if (GG_VB_OLD || ggVbUsed + bytes > GGVB_BYTES) return sceGuGetMemory((int)bytes);
  void *p = ggVb + ggVbUsed; ggVbUsed += (bytes + 3u) & ~3u; return p;
}
// №863 (Роб 22.10, TEST46: «крутил камеру L/R — у стены или машины пол всё равно ломается: квадрат, в нём треугольник просвечивается пустотой»):
// sceGuDrawArray в режиме GU_DIRECT сразу сдвигает адрес остановки списка (pspsdk: sceGeListUpdateStallAddr) — GE, который ждёт процессор
// (ожидание GE 0,1 мс), читает вершины веера СРАЗУ, а сброс кэша кольца стоял в конце куска (c98788b0, 10.10). GE брал из памяти вершины
// прошлого кадра с того же места кольца: камера стоит — совпадают, камера крутится — веер среза пола пропадает или рисуется мусором.
// Эмулятор кэша не имеет — там всё чисто (поэтому №791/№848/№861 «чинились» в эмуляторе). Правило: перед КАЖДОЙ командой рисования
// несброшенные строки кольца уходят в память (ggVbSync, через макрос ниже — для всех мест кольца сразу, и будущих тоже). VB_SYNC863_OLD=1 — как было
#ifndef VB_SYNC863_OLD
#define VB_SYNC863_OLD 0
#endif
static unsigned ggVbClean, ggVbSyncN;   // ggVbClean — до этого места кольцо уже в памяти; ggVbSyncN — сбросов за секунду (строка GGSTAT)
static inline void ggVbSync(void) {
  if (VB_SYNC863_OLD || GG_VB_OLD) return;
  if (ggVbUsed < ggVbClean) ggVbClean = 0;   // кольцо обнулили (начало кадра)
  if (ggVbUsed > ggVbClean) { unsigned a = ggVbClean & ~63u; sceKernelDcacheWritebackRange(ggVb + a, ggVbUsed - a); ggVbClean = ggVbUsed; ggVbSyncN++; }
}
static void ggVbWriteback(unsigned from) {   // сброс кэша строк кольца, записанных с позиции from
  if (GG_VB_OLD || ggVbUsed <= from) return;
  unsigned a = from & ~63u; sceKernelDcacheWritebackRange(ggVb + a, ggVbUsed - a);
  if (a <= ggVbClean) ggVbClean = ggVbUsed;
}
#if !VB_SYNC863_OLD
#define sceGuDrawArray(p, vt, c, ix, v) (ggVbSync(), sceGuDrawArray(p, vt, c, ix, v))
#define sceGuDrawArrayN(p, vt, c, n, ix, v) (ggVbSync(), sceGuDrawArrayN(p, vt, c, n, ix, v))
#endif
// Кадр после взрыва (10.10, №500, отчёт 9D: L7 взрыв x_city 2 -> 7,5-8,7 мс, ct_risky 3-4 мс, gg_ms 2,7 мс, 13 кусков стабильно 12 с): взрыв не делает куски
// «рискованными» сам — он оставляет Гомера пешком у остова: ближняя плоскость падает до FOOT_CAR_NEAR (машина у глаза, frameNear), ниже NEAR_Z начинается срез
// города на процессоре (ggOn), и куски, чей бокс содержит глаз или выходит за полосу GE, режутся ВСЕ треугольники каждый кадр, хотя камера стоит. Стоящая камера:
// результат среза куска (куски обычных треугольников + веера) запоминается и в следующем кадре просто рисуется (команды те же, срез не повторяется); маска
// рискованных кусков клетки (ggCellMask) тоже. «Та же камера» — глаз в пределах 3 мм, оси 1e-4, ближняя плоскость и тангенсы не менялись (ggCamUpdate раз в
// кадр, счётчик ggCamEpoch); запись только когда камера уже стояла кадр (ggCamStill >= 1), при движении ничего не копится. -DBOOM_CITY_OLD=1 — как было
#ifndef BOOM_CITY_OLD
#define BOOM_CITY_OLD 0
#endif
static float ggCamKey[15]; static unsigned ggCamEpoch = 1, ggCamStill; static unsigned ggReplayF;
static void ggcGarbageFlush(void);
static void ggCamUpdate(void) {
  ggcGarbageFlush(); marksGarbageFlush();   // начало кадра: GE прошлого кадра отработал (sceGuSync), его память можно отдать
  float k[15] = {ggE[0], ggE[1], ggE[2], ggR[0], ggR[1], ggR[2], ggU[0], ggU[1], ggU[2], ggF[0], ggF[1], ggF[2], ggK.zn, ggK.tvx, ggK.tvy};
  int same = 1;
  for (int i = 0; i < 15 && same; i++) { float tol = i < 3 ? .003f : i < 13 ? 1e-4f : 1e-5f; if (fabsf(k[i] - ggCamKey[i]) > tol) same = 0; }
  if (same) { if (ggCamStill < 1000000u) ggCamStill++; } else { memcpy(ggCamKey, k, sizeof k); if (++ggCamEpoch == 0) ggCamEpoch = 1; ggCamStill = 0; }
}
typedef struct { unsigned char kind; unsigned a, b; } GgEnt;   // kind 0: куски обычных треугольников [вершина a, число b]; 1: веер, b вершин из буфера со смещения a
typedef struct { const Piece *d; const void *v; int cnt, far, absM; unsigned epoch, sum, nent, bytes; GgEnt *ent; unsigned char *vb; } GgCut;
#define GGC_N 48
#define GGC_CAP (256u << 10)
static GgCut ggcT[GGC_N]; static unsigned ggcBytes, ggcNext;
static struct { unsigned nent, vused; int ov; GgEnt ent[160]; unsigned char vb[24576] __attribute__((aligned(16))); } ggRec;
// Память записи кэша среза читает GE по списку этого кадра (веера ссылаются на gc->vb): освобождать её можно только после sceGuSync конца кадра — отложенно (ggcGarbageFlush в начале
// следующего кадра, ggCamUpdate), иначе вытеснение записи в том же кадре отдаёт GE чужую память (и зависание на мусорных вершинах)
static void *ggcGarbage[64]; static unsigned ggcGarbageN;
static void ggcGarbageFlush(void) { for (unsigned i = 0; i < ggcGarbageN; i++) free(ggcGarbage[i]); ggcGarbageN = 0; }
static void ggcFree(GgCut *c) { if (c->ent) { ggcBytes -= c->bytes; if (ggcGarbageN < 64) ggcGarbage[ggcGarbageN++] = c->ent; else free(c->ent); } memset(c, 0, sizeof *c); }
static void ggcClear(void) { for (int i = 0; i < GGC_N; i++) ggcFree(&ggcT[i]); ggcGarbageFlush(); ggcBytes = 0; ggcNext = 0; }
static unsigned ggcSum(const unsigned char *vb, int cnt, int vs) {   // 8 вершин по куску: отличает кусок, чей адрес заняли заново
  unsigned s = (unsigned)cnt; int n = cnt / 3;
  for (int i = 0; i < 8; i++) { const unsigned *p = (const unsigned *)(vb + ((size_t)((long long)n * i / 8) * 3) * vs); s = s * 31u + p[0] + p[1] * 3u + p[2]; }
  return s;
}
static GgCut *ggcFind(const Piece *d, int far, int cnt, int absM, unsigned sum) {
  for (int i = 0; i < GGC_N; i++) { GgCut *c = &ggcT[i];
    if (c->ent && c->d == d && c->epoch == ggCamEpoch && c->cnt == cnt && c->far == far && c->absM == absM && c->sum == sum) return c; }
  return 0;
}
static void ggcStore(const Piece *d, const void *v, int far, int cnt, int absM, unsigned sum) {
  if (ggRec.ov) return;
  unsigned bytes = ggRec.nent * (unsigned)sizeof(GgEnt) + ((ggRec.vused + 15u) & ~15u);
  for (int i = 0; i < GGC_N; i++) if (ggcT[i].ent && ggcT[i].d == d) ggcFree(&ggcT[i]);   // прежняя запись этого куска (чужая камера)
  while (ggcBytes + bytes > GGC_CAP) { GgCut *o = &ggcT[ggcNext++ % GGC_N]; if (!o->ent && ggcBytes == 0) return; if (o->ent) ggcFree(o); else if (ggcNext > 4 * GGC_N) return; }
  GgCut *c = &ggcT[ggcNext++ % GGC_N]; ggcFree(c);
  bytes += 16;   // запас на выравнивание начала вершин (раньше до 15 Б уходили за край блока)
  unsigned char *mem = (unsigned char *)memalign(16, bytes); if (!mem) return;
  c->ent = (GgEnt *)mem; c->vb = mem + ggRec.nent * sizeof(GgEnt); c->vb = (unsigned char *)(((uintptr_t)c->vb + 15u) & ~(uintptr_t)15u);
  if ((size_t)(c->vb - mem) + ggRec.vused > bytes) { free(mem); c->ent = 0; return; }
  memcpy(c->ent, ggRec.ent, ggRec.nent * sizeof(GgEnt)); memcpy(c->vb, ggRec.vb, ggRec.vused); sceKernelDcacheWritebackRange(c->vb, ggRec.vused ? ggRec.vused : 1);
  c->d = d; c->v = v; c->cnt = cnt; c->far = far; c->absM = absM; c->epoch = ggCamEpoch; c->sum = sum; c->nent = ggRec.nent; c->bytes = bytes; ggcBytes += bytes;
}
// Стейтпропы по клеткам (10.10, №500, отчёт 9D: sp_loop 3-5 мс при 160-310 экземплярах, видимых 40-80): цикл отрисовки обходил ВСЕ экземпляры, у каждого —
// spDrawable (живость, слова состояния), сфера типа, кадр. Теперь экземпляры раскладываются по решётке 48 м при первой отрисовке (позиции экземпляров не
// меняются, кроме обломков: их типы идут отдельным хвостом и обходятся всегда), в кадре идут только клетки внутри дальности и пирамиды обзора (сфера клетки);
// дальше та же проверка экземпляра, что и раньше. -DSPROP_GRID_OLD=1 — полный обход
#ifndef SPROP_GRID_OLD
#define SPROP_GRID_OLD 0
#endif
#define SPG_CELL 48.f
typedef struct { float cx, cy, cz, rad; unsigned first, n; } SpgCell;
static struct { const void *inst; unsigned ninst, ncell, ngrid; SpgCell *cell; unsigned *idx, *cand; } spg;
static void spGridReset(void) { free(spg.cell); free(spg.idx); free(spg.cand); memset(&spg, 0, sizeof spg); }
static void spGridBuild(void) {
  unsigned n = sprops.ninst; if (!n) return;
  unsigned char *deb = (unsigned char *)calloc(sprops.ntype + 1u, 1); if (!deb) return;
  for (unsigned p = 0; p < sprops.ndyn; p++) if (sprops.dynBreak && sprops.dynBreak[p] < sprops.ntype) deb[sprops.dynBreak[p]] = 1;
  float x0 = 1e30f, x1 = -1e30f, z0 = 1e30f, z1 = -1e30f; unsigned ng = 0;
  for (unsigned i = 0; i < n; i++) { if (sprops.inst[i].type < sprops.ntype && deb[sprops.inst[i].type]) continue; const SpRun *u = &sprops.run[i];
    if (u->x < x0) x0 = u->x; if (u->x > x1) x1 = u->x; if (u->z < z0) z0 = u->z; if (u->z > z1) z1 = u->z; ng++; }
  float G = SPG_CELL; unsigned nx = 1, nz = 1;
  if (ng) { for (;;) { nx = (unsigned)((x1 - x0) / G) + 1; nz = (unsigned)((z1 - z0) / G) + 1; if ((unsigned long long)nx * nz <= 4096ull) break; G *= 1.5f; } }
  unsigned nc = ng ? nx * nz : 0;
  SpgCell *cell = (SpgCell *)calloc(nc + 1u, sizeof *cell); unsigned *idx = (unsigned *)malloc(n * sizeof *idx), *cand = (unsigned *)malloc(n * sizeof *cand);
  unsigned *cof = (unsigned *)malloc(n * sizeof *cof);
  if (!cell || !idx || !cand || !cof) { free(cell); free(idx); free(cand); free(cof); free(deb); return; }
  for (unsigned i = 0; i < n; i++) {
    if (sprops.inst[i].type < sprops.ntype && deb[sprops.inst[i].type]) { cof[i] = 0xffffffffu; continue; }
    const SpRun *u = &sprops.run[i]; unsigned cx = (unsigned)((u->x - x0) / G), cz = (unsigned)((u->z - z0) / G);
    if (cx >= nx) cx = nx - 1; if (cz >= nz) cz = nz - 1; cof[i] = cx + cz * nx; cell[cof[i]].n++; }
  unsigned acc = 0; for (unsigned c = 0; c < nc; c++) { cell[c].first = acc; acc += cell[c].n; cell[c].n = 0; }
  for (unsigned i = 0; i < n; i++) if (cof[i] != 0xffffffffu) { SpgCell *c = &cell[cof[i]]; idx[c->first + c->n++] = i; }
  spg.ngrid = acc; unsigned dn = acc;
  for (unsigned i = 0; i < n; i++) if (cof[i] == 0xffffffffu) idx[dn++] = i;
  for (unsigned c = 0; c < nc; c++) { SpgCell *q = &cell[c]; if (!q->n) continue;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (unsigned k = 0; k < q->n; k++) { const SpRun *u = &sprops.run[idx[q->first + k]]; float p3[3] = {u->x, u->y, u->z};
      for (int a = 0; a < 3; a++) { if (p3[a] < lo[a]) lo[a] = p3[a]; if (p3[a] > hi[a]) hi[a] = p3[a]; } }
    q->cx = (lo[0] + hi[0]) * .5f; q->cy = (lo[1] + hi[1]) * .5f; q->cz = (lo[2] + hi[2]) * .5f; float rad = 0.f;
    for (unsigned k = 0; k < q->n; k++) { unsigned i = idx[q->first + k]; const SpRun *u = &sprops.run[i]; const SpType *T = &sprops.type[sprops.inst[i].type];
      float dx = u->x - q->cx, dy = u->y - q->cy, dz = u->z - q->cz;
      float rt = sqrtf(T->c[0] * T->c[0] + T->c[1] * T->c[1] + T->c[2] * T->c[2]) + sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + .5f;   // сфера типа при любой позе + запас
      float d3 = sqrtf(dx * dx + dy * dy + dz * dz) + rt; if (d3 > rad) rad = d3; }
    q->rad = rad; }
  free(cof); free(deb);
  spg.inst = sprops.inst; spg.ninst = n; spg.ncell = nc; spg.cell = cell; spg.idx = idx; spg.cand = cand;
}
// кандидаты кадра: экземпляры клеток, чья сфера в дальности maxD (по горизонтали) и, если pl, в пирамиде обзора (глаз e3), плюс хвост обломков; возвращает число
static unsigned spGridCollect(float maxD, const float e3[3], const float (*pl)[3], unsigned **out) {
  if (SPROP_GRID_OLD) return sprops.ninst + 0 * (unsigned)(uintptr_t)out;
  if (!spg.cell || spg.inst != (const void *)sprops.inst || spg.ninst != sprops.ninst) { spGridReset(); spGridBuild(); }
  if (!spg.cell) { *out = 0; return sprops.ninst; }
  unsigned n = 0;
  for (unsigned c = 0; c < spg.ncell; c++) { const SpgCell *q = &spg.cell[c]; if (!q->n) continue;
    float dx = q->cx - e3[0], dz = q->cz - e3[2], R = maxD + q->rad; if (dx * dx + dz * dz > R * R) continue;
    if (pl) { float cc[3] = {q->cx, q->cy, q->cz}; if (rfSphereOut(pl, e3, cc, q->rad)) continue; }
    memcpy(spg.cand + n, spg.idx + q->first, q->n * sizeof(unsigned)); n += q->n; }
  unsigned dn = spg.ninst - spg.ngrid; if (dn) { memcpy(spg.cand + n, spg.idx + spg.ngrid, dn * sizeof(unsigned)); n += dn; }
  *out = spg.cand; return n;
}
static int ggForce; static unsigned bigPiecesF;   // ggForce: срез куска без условий ggOn и бокса (№640, крупные куски пропов у глаза)
// №853: кузов своей машины у камеры (Барт вышел — глаз в 0,3–1 м от кузова, ближняя плоскость 0,25–0,35 м): длинные треугольники боковин и капота
// пересекают near, вершина за полосой — GE выбрасывает треугольник, в кузове мигает дыра (эмулятор, выход у будок L6: до 37 треугольников в секунду).
// Кусок машины у камеры (carNear853) рисуется как обычно, а выброшенные GE треугольники дорисовываются срезом веером (drawGeoPatch853). Цена (эмулятор,
// выход из машины у будок L6): у машины +0,05–0,1 мс на кадр, в обычной езде — отсев куска одной проверкой. GG_CAR853_OLD=1 — как было
#ifndef GG_CAR853_OLD
#define GG_CAR853_OLD 0
#endif
// №921 (боты 24.10, «выброс треугольников GE у глаза»: L1 P026, L3 P027, L4 P045/P063, L5 P000/P040/P041): заплатка №853 стояла только у машин (owner != 0). Куски пропов, стейтпропов и
// света (owner 0, рисуются drawPieceM) шли в GE как есть, если не «крупные» №640 (поперечник 10…150 м и глаз ближе размера куска к центру): проп с треугольником 4–5 м или конус света
// 14 м при ближней плоскости 0,3–0,7 м терял треугольник целиком (L5 P040: все 6 треугольников куска каждый кадр, 186 в секунду; конус L4 P045: 19 в секунду). Теперь для куска с owner 0
// действует та же проверка carNear853 (шар куска и правила (1)–(3)), выброшенные треугольники дорисовываются срезом веером; полупрозрачные куски из очереди — тем же путём (граница
// 2048 px у стекла). Цена — одна проверка шара на кусок; срез только у куска у глаза. DROPFIX921_OLD=1 / «flag dropfix921 0» / файл dropfix921old рядом с EBOOT — прежнее
#ifndef DROPFIX921_OLD
#define DROPFIX921_OLD 0
#endif
static int dropFix921 = !DROPFIX921_OLD;
static void drawGeoG(const Piece *d, int far, int cnt, const ScePspFMatrix4 *m, int absM) {
  diagAbsM = absM;   // №921 служебно: diagGeo из drawGeo ниже по этому куску
  if ((!ggOn && !ggForce) || d->r.owner != 0 || !(d->r.flags & F_NOIDX) || cnt > 65535 || (!ggForce && (!GG_PIECE_OK(d) || !ggBoxRiskW(d->r.bb)))) { drawGeo(d, far, cnt); return; }
  ggPiecesF++;ggExamF+=cnt/3;unsigned us0 = sceKernelGetSystemTimeLow();
  unsigned vb0_ = ggVbUsed;
  int v12 = (d->r.flags & F_V12) != 0, vs = v12 ? 12 : 16, vt = v12 ? VT12 : VT16;
  const unsigned char *vb = (const unsigned char *)d->v + (far ? d->r.ni * vs : 0);
  if (d->r.flags & F_STRIP) {   // К4: срезу нужен список — постоянная развёртка куска (stripdraw.h pieceListP); кэш срезов построен по ней же
    unsigned nl_; vb = pieceListP((Piece *)d, far, &nl_); if (!vb) return; if (nl_ > 65535) { drawGeo(d, far, cnt); return; } cnt = (int)nl_; if (cnt < 3) return; }
  float T0 = m->w.x - (absM ? ggE[0] : 0.f), T1 = m->w.y - (absM ? ggE[1] : 0.f), T2 = m->w.z - (absM ? ggE[2] : 0.f);
  // позиция куска (s16/32768) -> оси камеры одной матрицей 3×4: A = оси камеры × модель, B = оси камеры × перенос
  const float *ax[3] = {ggR, ggU, ggF}; float A[3][3], B[3];
  for (int r = 0; r < 3; r++) {
    A[r][0] = ax[r][0] * m->x.x + ax[r][1] * m->x.y + ax[r][2] * m->x.z; A[r][1] = ax[r][0] * m->y.x + ax[r][1] * m->y.y + ax[r][2] * m->y.z;
    A[r][2] = ax[r][0] * m->z.x + ax[r][1] * m->z.y + ax[r][2] * m->z.z; B[r] = ax[r][0] * T0 + ax[r][1] * T1 + ax[r][2] * T2;
  }
  const int po = v12 ? 6 : 8; const float S = 1.f / 32768.f;
  unsigned gsum_ = 0; int grec_ = 0;
  if (!BOOM_CITY_OLD && ggCamStill >= 1 && !ggForce) {   // стоящая камера: срез куска уже сделан в прошлый кадр — те же команды без пересчёта (ggcFind)
    gsum_ = ggcSum(vb, cnt, vs);
    GgCut *gc_ = ggcFind(d, far, cnt, absM, gsum_);
    if (gc_) {
      for (unsigned e = 0; e < gc_->nent; e++) { const GgEnt *en = &gc_->ent[e];
        if (en->kind == 0) sceGuDrawArray(GU_TRIANGLES, vt, (int)en->b, 0, vb + (size_t)en->a * vs);
        else sceGuDrawArray(GU_TRIANGLE_FAN, GG_VT, (int)en->b, 0, gc_->vb + en->a); }
      ggReplayF++; smkGgReplay++; ggUsF += sceKernelGetSystemTimeLow() - us0; return;
    }
    ggRec.nent = ggRec.vused = 0; ggRec.ov = 0; grec_ = 1;
  }
#define GG_FAN_PEND 48
#define GG_FAN_EMIT(w_, n_) do { sceGuDrawArray(GU_TRIANGLE_FAN, GG_VT, (n_), 0, (w_)); \
    if (grec_) { unsigned nb_ = (unsigned)(n_) * (unsigned)sizeof(GgVert); \
      if (ggRec.nent < 160 && ggRec.vused + nb_ <= sizeof ggRec.vb) { memcpy(ggRec.vb + ggRec.vused, (w_), nb_); ggRec.ent[ggRec.nent++] = (GgEnt){1, ggRec.vused, (unsigned)(n_)}; ggRec.vused += (nb_ + 15u) & ~15u; } else ggRec.ov = 1; } } while (0)
#define GG_FAN_FLUSH() do { for (int f_ = 0; f_ < fanN; f_++) GG_FAN_EMIT(fanW[f_], fanC[f_]); fanN = 0; } while (0)
  const GgVert *fanW[GG_FAN_PEND]; int fanC[GG_FAN_PEND], fanN = 0;   // №863: веера среза до одного сброса кэша (ggVbSync в первом sceGuDrawArray)
  const GgBatch *batch=(!ggBatchOld&&d->ggBatch)?d->ggBatch+(far?ggBatchStorage(d->r.ni):0):0;
  const GgBatch *leaf=batch?batch+ggBatchSupers(cnt):0;
  int run = 0;
  for (int t = 0; t + 2 < cnt; t += 3) {
    if(batch&&t%GG_BATCH_SUPER_VERTS==0&&!ggBatchMayRisk(&batch[t/GG_BATCH_SUPER_VERTS],(const float(*)[3])A,B,&ggK)){
      int n=cnt-t;if(n>GG_BATCH_SUPER_VERTS)n=GG_BATCH_SUPER_VERTS;ggExamF-=n/3;t+=n-3;continue;
    }
    if(leaf&&t%GG_BATCH_VERTS==0&&!ggBatchMayRisk(&leaf[t/GG_BATCH_VERTS],(const float(*)[3])A,B,&ggK)){
      int n=cnt-t;if(n>GG_BATCH_VERTS)n=GG_BATCH_VERTS;ggExamF-=n/3;t+=n-3;continue;
    }
    GgV g[3]; int nearz = 0;
    for (int i = 0; i < 3; i++) {   // сначала глубина: треугольник целиком дальше GG_ZFAR под риском быть не может (вершине за полосой
      const short *q = (const short *)(vb + (t + i) * vs + po);   // там нужно |x| > 7·z — сотни метров вбок от видимой части)
      float lx = q[0] * S, ly = q[1] * S, lz = q[2] * S;
      g[i].l[0] = lx; g[i].l[1] = ly; g[i].l[2] = lz; g[i].c[2] = A[2][0] * lx + A[2][1] * ly + A[2][2] * lz + B[2]; nearz |= g[i].c[2] < GG_ZFAR;
    }
    if (!nearz) continue;
    for (int i = 0; i < 3; i++) {   // оси камеры x, y — для риска; uv и цвет — лишь у треугольника под риском
      float lx = g[i].l[0], ly = g[i].l[1], lz = g[i].l[2];
      g[i].c[0] = A[0][0] * lx + A[0][1] * ly + A[0][2] * lz + B[0]; g[i].c[1] = A[1][0] * lx + A[1][1] * ly + A[1][2] * lz + B[1];
    }
    if (!ggRisk(&g[0], &g[1], &g[2], &ggK)) continue;
    for (int i = 0; i < 3; i++) {
      const unsigned char *p = vb + (t + i) * vs; const unsigned short *uv = (const unsigned short *)p;
      g[i].t[0] = uv[0]; g[i].t[1] = uv[1];
      if (v12) { unsigned c5 = *(const unsigned short *)(p + 4), r = c5 & 31, gg = (c5 >> 5) & 63, b = c5 >> 11;   // 5650 -> 8888, как GE
        g[i].k[0] = (float)((r << 3) | (r >> 2)); g[i].k[1] = (float)((gg << 2) | (gg >> 4)); g[i].k[2] = (float)((b << 3) | (b >> 2)); g[i].k[3] = 255.f; }
      else { unsigned c8 = *(const unsigned *)(p + 4); g[i].k[0] = (float)(c8 & 255); g[i].k[1] = (float)((c8 >> 8) & 255); g[i].k[2] = (float)((c8 >> 16) & 255); g[i].k[3] = (float)(c8 >> 24); }
    }
    if (t > run) { sceGuDrawArray(GU_TRIANGLES, vt, t - run, 0, vb + run * vs); if (grec_) { if (ggRec.nent < 160) ggRec.ent[ggRec.nent++] = (GgEnt){0, (unsigned)run, (unsigned)(t - run)}; else ggRec.ov = 1; } }
    run = t + 3; ggTrisF++;
    GgV o[GG_MAXV]; int n = ggCut(&g[0], &g[1], &g[2], &ggK, o);
    if (n < 3) continue;
    GgVert *w = (GgVert *)ggVbAlloc((unsigned)n * sizeof(GgVert));
    for (int i = 0; i < n; i++) {
      float u = o[i].t[0] + .5f, v = o[i].t[1] + .5f; unsigned ch[4];
      for (int j = 0; j < 4; j++) { float x = o[i].k[j] + .5f; ch[j] = x < 0.f ? 0u : x > 255.f ? 255u : (unsigned)x; }
      w[i].u = (unsigned short)(u < 0.f ? 0 : u > 65535.f ? 65535 : (int)u); w[i].v = (unsigned short)(v < 0.f ? 0 : v > 65535.f ? 65535 : (int)v);
      w[i].c = ch[0] | ch[1] << 8 | ch[2] << 16 | ch[3] << 24; w[i].x = o[i].l[0]; w[i].y = o[i].l[1]; w[i].z = o[i].l[2];
    }
    if (VB_SYNC863_OLD) { GG_FAN_EMIT(w, n); } else { fanW[fanN] = w; fanC[fanN] = n; if (++fanN == GG_FAN_PEND) GG_FAN_FLUSH(); }   // №863: веера куска — одним сбросом кэша
  }
  GG_FAN_FLUSH();
  if (cnt > run) { sceGuDrawArray(GU_TRIANGLES, vt, cnt - run, 0, vb + run * vs); if (grec_) { if (ggRec.nent < 160) ggRec.ent[ggRec.nent++] = (GgEnt){0, (unsigned)run, (unsigned)(cnt - run)}; else ggRec.ov = 1; } }
  if (grec_) ggcStore(d, d->v, far, cnt, absM, gsum_);
  ggVbWriteback(vb0_);
  ggUsF += sceKernelGetSystemTimeLow() - us0;   // процессор на срез по треугольникам (строка GGSTAT)
}

// Полупрозрачное (стекло, тени-пятна, листва с blend): пока tqOn, куски с флагом 2 не рисуются сразу, а
// откладываются в очередь и рисуются одним проходом после всего непрозрачного (город, машина, пропы, монеты),
// от дальнего к ближнему, с тестом глубины и без записи. Иначе стекло без записи глубины затирается тем, что
// рисуется после него (машина за витриной оказывалась поверх стекла), а стёкла друг за другом — в порядке текстур.
#define MAXTQ 256
typedef struct { Piece p; ScePspFMatrix4 m; int far, cnt; float key; int own; float cx, cy, cz, rad; } TQ;   // cx..rad — шар куска (rad 0 — границ нет), TQ_FAST: порядок по текстуре   // own — кусок своей машины (правка 413б, замер порядка)
static int cgOwnTq; static unsigned tqOwnSig, tqOwnFlips, tqOwnItems;   // 413б: подпись порядка кусков своей машины в очереди и её смены
static TQ tq[MAXTQ]; static TQ *tqp[MAXTQ]; static int ntq, tqOn; static float tqEx, tqEy, tqEz;
static int tqPush(const Piece *d, const ScePspFMatrix4 *m, int far, int cnt, float cx, float cy, float cz) {
  if (!tqOn || !(d->r.flags & 2) || (d->r.flags & 4) || ntq >= MAXTQ) return 0;
  TQ *q = &tq[ntq++]; q->p = *d; q->m = *m; q->far = far; q->cnt = cnt; q->own = cgOwnTq; cgOwnTq = 0;
  float dx = cx - tqEx, dy = cy - tqEy, dz = cz - tqEz; q->key = dx * dx + dy * dy + dz * dz;
  q->cx = cx; q->cy = cy; q->cz = cz; q->rad = d->rad > 0.f && d->rad < 200.f ? d->rad : 0.f;
  return 1;
}
// мир = T + R(угол) * (центр куска + масштаб * вершина); матрица модели сразу в GE, без стека gum
// Правки Роба 224/237/251 (швы мигают): с 03.10 у кусков города нет дальней версии (tools/psp_export.py SEAMS_OLD), флаг F_NEAR —
// кусок рисуется ближней на любой дальности; уровень, собранный по-старому (SHR_SEAMS_OLD=1), без флага — дальней, как раньше
// №634 (Роб: «проваливаешься в текстуры домов, это везде»): город рисуется двусторонне (O_CULL выключен: 0,5 % пикселей с изнанкой на других точках глаза, docs/handoff/noindex.md),
// и когда камера оказалась у/в стене дома, видна изнанка и грани у ближней плоскости — «текстура в лицо». Куски (непрозрачные), чей ящик ближе CAM_CLIP_R к глазу или содержит глаз,
// рисуются с отсечением задних граней (F_LSIDE), а клетка с таким куском — не листом, а кусками (лист пишет свои состояния сам). Остальной город прежний. CAM_CLIP_CITY_OLD=1 — как было
#ifndef CAM_CLIP_CITY_OLD
#define CAM_CLIP_CITY_OLD 0
#endif
#ifndef CAM_CLIP_R
#define CAM_CLIP_R 0.5f
#endif
#ifndef CAMCLIPFAST1060_OLD
#define CAMCLIPFAST1060_OLD 0
#endif
static int camClipFast1060 = !CAMCLIPFAST1060_OLD, camClipChk1060;   // №1060: служебный счётчик CAMCLIPSTAT без чтения далёких кусков
// №634 (арка J9-5, TEST19: стены навеса на железе бледные и просвечивают, у PS2 плотные): у PSP нет отсечения по ближней плоскости, GE выбрасывает треугольник целиком, если вершина
// за плоскостью; срез у камеры (ggOn, правка 340) раньше включался только при опущенной ближней плоскости. Теперь и в кадре, где непрозрачный кусок города ближе GG_NEAR_R к глазу.
// Рантайм «flag ggnear 0/1»; CAM_GG_NEAR_OLD=1 — как было
#ifndef CAM_GG_NEAR_OLD
#define CAM_GG_NEAR_OLD 0
#endif
#define GG_NEAR_R 1.5f
#ifndef GG_NEAR_DEFAULT
#define GG_NEAR_DEFAULT 0   // TEST22: №634 ggnear включает срез треугольников на процессоре для ВСЕХ клеток кадра, если глаз ближе 1,5 м к куску (на причале x_city в 13 раз, на улице в 3 раза); по умолчанию выкл, «flag ggnear 1» — рантайм-включение для A/B
#endif
static int ggNearOn = GG_NEAR_DEFAULT; static unsigned ggNearFrames;
static int ggNearAny(void); static int camWall05; static float camZn = 1.5f;   // camZn: ближняя плоскость кадра
static float wallFrontR;   // служебно «flag wallfrontr R»: длина луча wall_front вместо ближней плоскости (положительный контроль)
static unsigned camClipPieces; static int camClipLog;   // camClipLog — flag camclipdump   // служебно: сколько кусков в этом кадре нарисовано с отсечением у камеры
static inline int camClipNear(const Piece *d) {
  if (CAM_CLIP_CITY_OLD || !camWall05 || (d->r.flags & (2 | 4))) return 0;   // прозрачное и небо — не трогаем; без настоящей стены у глаза — как было
  float dx = fmaxf(fmaxf(d->r.bb[0] - eyeX, eyeX - d->r.bb[3]), 0.f), dy = fmaxf(fmaxf(d->r.bb[1] - eyeY, eyeY - d->r.bb[4]), 0.f), dz = fmaxf(fmaxf(d->r.bb[2] - eyeZ, eyeZ - d->r.bb[5]), 0.f);
  return dx * dx + dy * dy + dz * dz < CAM_CLIP_R * CAM_CLIP_R;
}
static void carToonHull(Piece *d, const float *m);
// №675: контур и у мелких предметов мира (пропы, столбы, знаки, урны): кусок не крупнее TOON_WORLD_EXT по любой оси, ближе TOON_WORLD_R к глазу; дома и крупные плиты города не трогаем (оболочка около центра бокса на 2,5 см у большого куска не видна)
#define TOON_WORLD_EXT 8.f
#define TOON_WORLD_R 45.f
// №1062 (Роб 27.10: «обводка только у героя, NPC, машин, будки и предметов, с которыми взаимодействуем; дома, дороги, край асфальта, линии мира, весь фон — без контура, ради скорости и гладкости»):
// линии окружения (toonlines.shr — дома и крупные предметы, roadlines.shr — край асфальта, линии троп, кромка) не читаются и не рисуются, контур мелких кусков города (toonWorldOk) выключен.
// Остаются оболочки героя, NPC, всех машин, пропов и стейтпропов (будка, предметы). INK1062_OLD=1 / файл ink1062old рядом с EBOOT (до загрузки уровня) / flag ink1062old 1 в начале сценария — как в TEST74
#ifndef INK1062_OLD
#define INK1062_OLD 0
#endif
static int ink1062Old = INK1062_OLD;
static unsigned worldToonDraws, hullToonCalls;   // №1062: оболочки мелких кусков города (входят в carToonDraws), строка INK1062 в журнале
static inline int toonWorldOk(const Piece *d) {
  if (!ink1062Old) return 0;   // №1062: контур мелких кусков города (пропы в мешах, столбы, знаки) — фон, без контура
  if (SKIN_TOON_V675_OLD || !skinToonEnabled() || (d->r.flags & (2 | 4)) || d->r.ni > 1500) return 0;
  float ex = d->r.bb[3] - d->r.bb[0], ey = d->r.bb[4] - d->r.bb[1], ez = d->r.bb[5] - d->r.bb[2];
  if (ex > TOON_WORLD_EXT || ey > TOON_WORLD_EXT || ez > TOON_WORLD_EXT) return 0;
  float dx = (d->r.bb[0] + d->r.bb[3]) * .5f - eyeX, dy = (d->r.bb[1] + d->r.bb[4]) * .5f - eyeY, dz = (d->r.bb[2] + d->r.bb[5]) * .5f - eyeZ;
  return dx * dx + dy * dy + dz * dz < TOON_WORLD_R * TOON_WORLD_R;
}
static unsigned char *opqFar; static unsigned opqFarUse, opqFarNear;   // №863: объявлено здесь, заполняет main_levels.h opq863Apply
static int skyDebug_;   // №701: служебный флаг сценария skydebug 1 (глубина после неба пурпуром), 2 — журнал дальних кусков кадра
static void drawPiece(Piece *d, int far, float tx, float ty, float tz, float c, float s) {
  RBC(rbcDraw);
  if (skyDebug_ == 2 && smkOn && d->r.owner == 0) {   // №701: дальние куски города кадра (центр ящика, расстояние до глаза, текстура, флаги)
    static int nlog_; float cx_ = (d->r.bb[0] + d->r.bb[3]) * .5f + tx, cy_ = (d->r.bb[1] + d->r.bb[4]) * .5f + ty, cz_ = (d->r.bb[2] + d->r.bb[5]) * .5f + tz;
    float dx_ = cx_ + ggE[0], dz_ = cz_ + ggE[2]; float dd_ = sqrtf(dx_ * dx_ + dz_ * dz_);
    if (dd_ > 90.f && nlog_ < 60) { nlog_++; char l_[200]; snprintf(l_, sizeof l_, "FARPIECE d=%.0f eye=%.0f,%.0f,%.0f c=%.0f,%.0f,%.0f ext=%.0f,%.0f,%.0f tex=%d fl=%x far=%d n=%d\n", dd_, ggE[0], ggE[1], ggE[2], cx_, cy_, cz_, d->r.bb[3] - d->r.bb[0], d->r.bb[4] - d->r.bb[1], d->r.bb[5] - d->r.bb[2], (int)d->r.tex, (unsigned)d->r.flags, far, far ? (int)d->r.nf : (int)d->r.ni); smkWrite(l_); }
  }
  if (opqFar && d >= pc && d < pc + npc && opqFar[d - pc] && tx == 0.f && ty == 0.f && tz == 0.f) { far = !ggBoxRiskW(d->r.bb); if (far) opqFarUse++; else opqFarNear++; }   // №863: море — крупной версией везде, где GE нарисует её целиком (main_levels.h opq863Apply)
  if ((d->r.flags & F_NEAR) && !(far && (farPlainD > 0.f || (opqFar && d >= pc && d < pc + npc && opqFar[d - pc])) && (d->r.flags & F_FARP))) far = 0;   // правка 508: F_FARP — дальняя версия без нарезки по far_plain
  int cnt = far ? (int)d->r.nf : d->r.ni;
  if (cnt <= 0 || geOver(GE_HARD) || !pieceOk(d, far) || NEON_SKIP(d) || GLOWOFF1052_SKIP(d) || TREESH_SKIP(d)) return;   // правка 103: список GE почти полон / битый адрес
  if (d->r.flags & F_ADDITIVE) { dbgAddN++; dbgAddTri += (unsigned)cnt / 3; if (cnt > 600) dbgAddBig++; gloAdd('c', d->r.tex, far, d->r.ni > 600, (unsigned)cnt / 3); }   // №933: город со сложением, прошедший NEON_SKIP (вода: ni > 192)
  float k = d->r.sc, ox = d->r.ox, oy = d->r.oy, oz = d->r.oz;
  ScePspFMatrix4 m = {
    {c * k, 0, -s * k, 0}, {0, k, 0, 0}, {s * k, 0, c * k, 0},
    {tx + c * ox + s * oz, ty + oy, tz - s * ox + c * oz, 1}};
  if (tqPush(d, &m, far, cnt, (d->r.bb[0] + d->r.bb[3]) * .5f + tx, (d->r.bb[1] + d->r.bb[4]) * .5f + ty, (d->r.bb[2] + d->r.bb[5]) * .5f + tz)) return;
  drawnTris += cnt / 3; draws++;
  if (!far && toonWorldOk(d)) { carToonHull(d, (const float *)&m); worldToonDraws++; }   // №675
  phAllow = d->r.owner == 0; NZP(d); bindTexLean(d->r.tex); phAllow = 0; { int ccn_ = camClipNear(d); camClipPieces += ccn_; setFlags(d->r.flags | (ccn_ ? F_LSIDE : 0)); }
  sceGuSetMatrix(GU_MODEL, &m);
  int glowFog = glowFogBegin(d->r.flags);
  drawGeoG(d, far, cnt, &m, !(tx == -ggE[0] && ty == -ggE[1] && tz == -ggE[2]));   // правка 340: у камеры — срез по полосе
  glowFogEnd(glowFog);
}
// Правка Роба 428 «мультяшный режим» (РАСШИРЕНИЕ ПОРТА): контур машины игрока — кусок ещё раз, раздутый по своим осям на
// SKIN_TOON_W с каждой стороны (растяжение около центра бокса куска; плоскую ось не трогаем) ДО куска и без записи глубины,
// чёрным: дымка 0..0,001 м цвета 0 — как маска lumaKey. Без новых данных: та же геометрия, +1 вызов на кусок
#define CAR_TOON_W .025f   // правило порта (вкус): как у персонажей (viewer/skin.c SKIN_TOON_W)
static unsigned carToonDraws;
// №779 (Роб 18.10 через дирижёра: «машинам NPC тоже контур, как у машины игрока и героев — внешний силуэт, толщина почти единая, вдали чуть тоньше и мягче»):
// вытяжка — skinToonWorldW по расстоянию до куска (цель на экране та же, что у героев), тушь вдали светлеет к цвету дали (skinToonAirMix), оболочка сдвинута по глубине
// назад на CAR_TOON_SIL_M (диапазон глубины sceGuDepthRange: с поправкой слоя libgu) — линии только по внешнему силуэту и там, где часть выступает сильнее.
// CAR_TOON_W779_OLD=1 — прежнее (2,5 см, полная тушь, без сдвига)
#ifndef CAR_TOON_W779_OLD
#define CAR_TOON_W779_OLD 0
#endif
// №901 (Роб 23.10: «внутри однотонного линий нет»): на капоте и крыше машины, у швов между кусками, тушь 15-сантиметровой оболочки проступала редкими точками в щелях (кусок позади шва — салон, моторный отсек —
// ближе 15 см). Сдвиг назад 30 см: точки уходят, внешний ободок против мира и линия у колёс остаются. CAR_TOON_DOT901_OLD=1 — 15 см, как TEST36
#ifndef CAR_TOON_DOT901_OLD
#define CAR_TOON_DOT901_OLD 0
#endif
#ifndef CAR_TOON_SIL_M
#define CAR_TOON_SIL_M (CAR_TOON_DOT901_OLD ? .15f : .3f)
#endif
// №870 (Роб 22.10: «проверь, что по разрешению для PSP чересчур детально»): контур мультика — второй проход каждого куска машины; у своей машины 33 куска,
// 15 из них — плоские детали на 2–11 треугольников (фары, накладки) и крошки меньше 30 см: их контура не видно, а вызов стоит как у кузова (контур машины
// ≈ 0,4 мс эмулятора ≈ 2 мс PSP). Такие куски контура не получают. TOON_HULL870_OLD=1 — контур у всех кусков
#ifndef TOON_HULL870_OLD
#define TOON_HULL870_OLD 0
#endif
static int thDiag; static unsigned thCalls, thWords, thUs;   // №912 диагностика: контур машин за секунду — вызовы, слова списка GE, мкс процессора (строка TOONHULL, только в смоуке)
// №912 (Роб 23.10: «мультик основа игры», окно «Оптимизация»: мультик ≈ 0,8 мс эмулятора ≈ 3,8–4,7 мс PSP на кадр, из них контур машин): разбор carToonHull по стадиям (L1, эмулятор, 28 вызовов на кадр, 8,5 мкс на вызов):
// матрица куска 38 % (полное умножение 4×4 при диагональной A), состояние + возврат после КАЖДОГО куска 30 % (пересброс текстуры, флагов, дымки — следующий настоящий кусок привязывает текстуру заново), рисование 19 %.
// Правило семейства: (1) матрица M = m·A считается по диагональной A (те же числа, 12 умножений вместо 64); (2) контур не рисуется между кусками, а копится в очередь и рисуется пачкой в конце фазы машин (flush в
// resetState и перед фазой пропов): состояние GE ставится один раз на пачку, текстура настоящих кусков не сбрасывается. Контур идёт без записи глубины и сдвинут назад (silD), поэтому внутри силуэта его закрывает
// сам кусок и соседи, снаружи остаётся ободок — порядок «контур, затем кусок» нужен был только для отбора, пачка после всех кусков закрывает больше, не меньше. HULL912_OLD=1 — как было (контур перед каждым куском)
#include "hullmat912.h"
#ifndef HULL912_OLD
#define HULL912_OLD 0
#endif
#ifndef HULL912_MAT_OLD
#define HULL912_MAT_OLD 0
#endif
// №930г (дирижёр 25.10: город +0,147 мс Мак ≈ +0,85 мс PSP на L4/L7 стоя даёт только слияние №930, флаг carfill930 0 не возвращает): виновата не заливка, а очередь контура. На L4 в кадре до 75 кусков контура (пары «машины, прохожие, пропы»);
// при 56 местах очередь выпускалась дважды за кадр — каждый выпуск сам по себе (состояние GE, сброс привязки) стоит ≈ 0,14 мс Мак. Проба: 160 -> x_city 1,199 (как до слияния), 56 -> 1,348, CARFILL930_OLD при 56 -> 1,354.
// Теперь 96 мест (≈ 9 КБ вместо 15 КБ у 160): пик 75 на L4 влезает. THQ930D_OLD=1 — 56, как test62
#ifndef THQ930D_OLD
#define THQ930D_OLD 0
#endif
#define THQ_MAX (CARFILL930_OLD ? 160 : THQ930D_OLD ? 56 : 96)   // №930 (память: компенсация кода и таблиц carfill930.h): очередь контура машин — 56 мест вместо 160 (≈ 9,6 КБ). Пик в прогонах: 23 почти везде, 68 один раз на L5 (на 4 места больше — один лишний выпуск пачки); полна — выпуск пачки и дальше, как было
typedef struct { float M[16]; unsigned ink; int silD, flags, ni; const Vert *v; const unsigned short *idx, *fidx; } ThQ;   // только то, что нужно drawGeo: копия куска целиком (memcpy) стоила больше самой матрицы
static ThQ thQ[THQ_MAX]; static int thQn, thQPeak; static unsigned thQFlush, thQItems, thUsPush, thUsFlush;   // thQPeak — наибольшая заполненность очереди (служебно, строка CARFILL)
// №922 (Роб: «у края кадра ничего не режется», закон объектива; боты 24.10, №921 п.в): обводка машины (оболочка куска, tex 0) у глаза теряла треугольники — кусок линии. Заплатка №853 теперь и у обводки:
// carNear853 по матрице оболочки, веера на месте выброшенных рисуются в том же состоянии (тушь дымкой, без глубины). DROPINK922_OLD=1 / «flag dropink922 0» / файл dropink922old — прежнее
#ifndef DROPINK922_OLD
#define DROPINK922_OLD 0
#endif
static int dropInk922 = !DROPINK922_OLD;
// цена: проверка обводки по матрице оболочки стоила +0,1…0,2 мс Мака на кадр у машины (замер A/B ik0/ik1, секунды с веерами), поэтому обводка латается только у куска, у кузова которого в этом кадре
// уже были веера (кузов и оболочка — одни вершины; кузов рисуется раньше выпуска очереди обводок). Список — до 8 кусков, обнуляется в начале кадра (main_loop.h)
static const void *carFanV[8]; static int carFanN;
static inline int carFanHas(const void *v) { for (int i = 0; i < carFanN; i++) if (carFanV[i] == v) return 1; return 0; }
static int carNear853(const Piece *d, const ScePspFMatrix4 *m); static void drawGeoPatch853(const Piece *d, const ScePspFMatrix4 *m);
// №991 (Роб 26.10 на PSP: «у машин NPC при некоторых ракурсах контур ломается — в три четверти сверху и с торца обводка сливается в одну большую толстую»): воспроизведено (L1, trfgo, глаз 8–30 м, высота 0,25–0,6 от расстояния; кадр с мультиком и без):
// оболочка плоской крыши, поднятая на вытяжку над самой крышей, проходит тест глубины и красит крышу целиком тушью — крыша «чёрный блок», бока вдвое толще. Сдвиг назад 30 см (№901) на этих дальностях и под скользящим углом крышу не прячет
// (лечит 60 см и больше). У NPC-машин сдвиг 80 см (больше 1,2 м уже съедает линию по нижнему краю бампера); машина игрока — как была (30 см). CARINK991_OLD=1 / «flag carink991 0» / файл carink991old — прежнее
#ifndef CARINK991_OLD
#define CARINK991_OLD 0
#endif
#define CAR_TOON_SIL_NPC .8f
static int carInk991 = !CARINK991_OLD;
// №1009 (Роб 26.10: «контур не должен сливаться: персонаж на фоне проезжающей машины NPC — разные объекты, у них разные контуры»): обводка героя и своей машины рисуется оболочкой без записи глубины, а тела машин
// трафика и стоящих рисовались ПОСЛЕ героя и своей машины: тело, нарисованное позже, закрывало ободок там, где за героем (или за своей машиной) стоит машина NPC — линия пропадала, силуэты слипались.
// Теперь машины трафика и стоящие рисуются раньше своей машины и героя (до их ободков; их собственные оболочки встают в пачку и выходят перед следующим слоем, как всегда): ободок ближнего ложится поверх тела
// дальней машины. Порядок кадров другой, число вызовов то же, цена 0. INKSEP1009_OLD=1 / файл inksep1009old / «flag inksep1009 0» — прежний порядок
#ifndef INKSEP1009_OLD
#define INKSEP1009_OLD 0
#endif
static int inkSep1009 = !INKSEP1009_OLD;
// №1028 (Роб 26.10, скриншот из эмулятора: «у розовой машины Гомера сзади-сбоку левый бок и торец залиты широкой чёрной полосой, торец всё равно ломается, даже на моей машине»): сдвиг оболочки назад 80 см, как у NPC-машин
// (№991), теперь и у машины игрока и машин миссий (carToonHull с carToonNpcNow = 0): на скользящем угле оболочка плоского бока и торца под сдвигом 30 см проходила тест глубины и красила их тушью. Толщина линии и перо те же.
// CARINK1028_OLD=1 / «flag carink1028 0» / файл carink1028old — как в TEST71
#ifndef CARINK1028_OLD
#define CARINK1028_OLD 0
#endif
static int carInk1028 = !CARINK1028_OLD;
static int carToonNpcNow;   // №931: рисуется контур NPC-машины (npcCarToon): тоньше и светлее с расстоянием, дальше TOON_NPC_N линии нет; машина игрока — прежнее
static void carToonHull(Piece *d, const float *m) {
  hullToonCalls++;   // №1062: все оболочки машин и мелких кусков города (счётчик журнала INK1062)
  if (d->r.ni <= 0 || geOver(GE_HARD) || d->r.sc <= 0.f) return;
  if (!TOON_HULL870_OLD && (d->r.ni < 16 * 3 || fmaxf(fmaxf(d->r.bb[3] - d->r.bb[0], d->r.bb[4] - d->r.bb[1]), d->r.bb[5] - d->r.bb[2]) < .3f)) return;
  unsigned th0_ = thDiag ? sceKernelGetSystemTimeLow() : 0, thw_ = thDiag ? (unsigned)sceGuCheckList() : 0;
  float dx_ = m[12] - eyeX, dy_ = m[13] - eyeY, dz_ = m[14] - eyeZ, dist_ = sqrtf(dx_ * dx_ + dy_ * dy_ + dz_ * dz_);
  // №823 (Роб 18.10: «своя машина сзади — задний бампер — контур массивный»): вытяжка по ближнему краю куска, а не по его началу (кузов — один кусок
  // в 4–5 м: при расстоянии до середины 5 м бампер в 2,5 м получал линию вдвое толще). TOON_RANGE823_OLD=1 — по началу куска, как TEST35
  float dW_ = dist_;
  if (!TOON_RANGE823_OLD) { float hx_ = (d->r.bb[3] - d->r.bb[0]) * .5f, hy_ = (d->r.bb[4] - d->r.bb[1]) * .5f, hz_ = (d->r.bb[5] - d->r.bb[2]) * .5f;
    dW_ = fmaxf(fmaxf(dist_ - sqrtf(hx_ * hx_ + hy_ * hy_ + hz_ * hz_), dist_ * .45f), 1.5f); }
  float W_ = CAR_TOON_W779_OLD ? CAR_TOON_W : carToonNpcNow ? skinToonWorldWNpc(dW_) : skinToonWorldW(dW_);
  if (W_ <= 0.f) return;
  unsigned ink_ = TOON_INK_OLD ? 0u : (CAR_TOON_W779_OLD ? TOON_INK_ABGR : carToonNpcNow ? skinToonAirMixNpc(skinInkAbgr(INK_CHAR), dist_) : skinToonAirMix(skinInkAbgr(INK_CHAR), dist_)) & 0xffffff;   // №796: перо линий
  int silD_ = (!CAR_TOON_W779_OLD && dist_ > .5f) ? (int)fminf(20000.f, ((carToonNpcNow || carInk1028) && carInk991 ? CAR_TOON_SIL_NPC : CAR_TOON_SIL_M) * 65535.f / dist_) : 0;
  float sc3[3], tr3[3], o[3] = {d->r.ox, d->r.oy, d->r.oz};   // оболочка: масштаб по осям вокруг центра куска и сдвиг (диагональная A)
  for (int i = 0; i < 3; i++) {
    float half = (d->r.bb[i + 3] - d->r.bb[i]) * .5f, c = (d->r.bb[i + 3] + d->r.bb[i]) * .5f;
    float sc = half > W_ ? 1.f + W_ / half : 1.f, cr = (c - o[i]) / d->r.sc;   // центр в сырых координатах вершин
    sc3[i] = sc; tr3[i] = cr * (1.f - sc);
  }
  int queued = !HULL912_OLD && !(d->r.flags & F_STRIP);
  if (queued && thQn >= THQ_MAX) carToonHullFlush();   // очередь полна — выпустить и копить дальше
  float Mloc[16], *M = queued ? thQ[thQn].M : Mloc;
  if (HULL912_MAT_OLD) {
    float A[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; for (int i = 0; i < 3; i++) { A[i * 5] = sc3[i]; A[12 + i] = tr3[i]; }
    hullMatFull(m, A, M);
  } else hullMatDiag(m, sc3, tr3, M);   // №912: A — диагональ и сдвиг, числа те же, знак нуля может отличаться (hullmat912.h)
  if (queued) {
    ThQ *q = &thQ[thQn]; q->ink = ink_; q->silD = silD_; q->flags = d->r.flags; q->ni = d->r.ni; q->v = d->v; q->idx = d->idx; q->fidx = d->fidx;
    thQn++; thQItems++; carToonDraws++; if (thQn > thQPeak) thQPeak = thQn;
    if (thDiag) { thCalls++; thUsPush += sceKernelGetSystemTimeLow() - th0_; thUs += sceKernelGetSystemTimeLow() - th0_; thWords += ((unsigned)sceGuCheckList() - thw_) / 4; }
    return;
  }
  texEnable(0); bound = -2; setFlags(d->r.flags & ~3);
  sceGuEnable(GU_FOG); sceGuFog(0.f, 1e-3f, ink_); fogK = -1.f;   // №754: дымка в цвет туши — ободок ровной тушью (№779: вдали светлее)
  sceGuDepthMask(GU_TRUE);   // без записи глубины: кусок ляжет поверх, снаружи силуэта останется ободок
  if (!SKIN_TOON_V675_OLD && TOON_INK_OLD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0, TOON_HULL_DST); }   // №675: контур 70 % чёрного (дымка даёт 0, фон проходит на 30 %)
  sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)M);
  if (silD_) sceGuDepthRange(65535 - silD_, 0);   // №779: только внешний силуэт
  if (dropInk922 && dropFix921 && carFanHas(d->v) && carNear853(d, (const ScePspFMatrix4 *)M)) drawGeoPatch853(d, (const ScePspFMatrix4 *)M);   // №922: обводка у глаза — с заплаткой, как кузов
  else drawGeo(d, 0, d->r.ni);   // №853: обводку у камеры не латали — выброс давал кадр без куска линии (№922: теперь латаем, см. выше)
  if (silD_) sceGuDepthRange(65535, 0);
  if (!SKIN_TOON_V675_OLD && TOON_INK_OLD) sceGuDisable(GU_BLEND);
  curFlags = -1; fogBase(); if (!groundCoverFog(&groundCover) && !fogCityOld) sceGuDisable(GU_FOG);   // параметры дымки — обратно (их включают carvis/fx)
  carToonDraws++;
}
// пачка контура: состояние один раз, на кусок — дымка (перо зависит от расстояния), матрица, сдвиг глубины, вызов; потом возврат как после одиночного
static void carToonHullFlush(void) {
  if (thQn <= 0) return;
  int n = thQn; thQn = 0; thQFlush++;
  unsigned th0_ = thDiag ? sceKernelGetSystemTimeLow() : 0, thw_ = thDiag ? (unsigned)sceGuCheckList() : 0;
  static Piece tp;   // для drawGeo нужны только флаги, число вершин, адреса данных; остальное нули (static)
  texEnable(0); bound = -2;
  sceGuEnable(GU_FOG); sceGuDepthMask(GU_TRUE);
  if (!SKIN_TOON_V675_OLD && TOON_INK_OLD) { sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0, TOON_HULL_DST); }
  unsigned lastInk = 0xffffffffu; int lastSil = 0;
  for (int i = 0; i < n; i++) {
    ThQ *q = &thQ[i]; if (geOver(GE_HARD)) break;
    setFlags(q->flags & ~3); sceGuDepthMask(GU_TRUE);   // setFlags пишет маску глубины по флагам куска — контур глубину не пишет
    if (q->ink != lastInk) { sceGuFog(0.f, 1e-3f, q->ink); lastInk = q->ink; }
    sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)q->M);
    if (q->silD != lastSil) { sceGuDepthRange(q->silD ? 65535 - q->silD : 65535, 0); lastSil = q->silD; }
    tp.r.flags = q->flags; tp.r.ni = q->ni; tp.v = (Vert *)q->v; tp.idx = (unsigned short *)q->idx; tp.fidx = (unsigned short *)q->fidx;
    if (dropInk922 && dropFix921 && carFanHas(tp.v) && carNear853(&tp, (const ScePspFMatrix4 *)q->M)) drawGeoPatch853(&tp, (const ScePspFMatrix4 *)q->M);   // №922
    else drawGeo(&tp, 0, q->ni);
  }
  if (lastSil) sceGuDepthRange(65535, 0);
  if (!SKIN_TOON_V675_OLD && TOON_INK_OLD) sceGuDisable(GU_BLEND);
  fogK = -1.f; curFlags = -1; fogBase(); if (!groundCoverFog(&groundCover) && !fogCityOld) sceGuDisable(GU_FOG);
  if (thDiag) { thUsFlush += sceKernelGetSystemTimeLow() - th0_; thUs += sceKernelGetSystemTimeLow() - th0_; thWords += ((unsigned)sceGuCheckList() - thw_) / 4; }
}
// №640 (диагональные линии на перекрёстках/в езде; слово дирижёра 16.10): ggCut режет по защитной полосе GE только куски города в кадре с ggOn. Крупные куски пропов и стейтпропов (щиты 12 м, плиты 13×18 м,
// конструкции 23 м; они без индексов, как город) шли в GE как есть: у ближней плоскости вершина уходит за ±2048 px, GE выбрасывает треугольник целиком или рисует клин. Правило: кусок с поперечником
// BIG_EXT_MIN…BIG_EXT_MAX (больше — небо/вода/купол, глаз они не пересекают) режется тем же drawGeoG (ggRisk/ggCut), но ТОЛЬКО пока глаз ближе размера куска к его центру (ggForce: без условия ggOn и бокса куска,
// без кэша стоящей камеры); иначе прежний drawGeo (цена — лишь в кадрах у куска). Куски с флагом 2 идут через очередь и не режутся. BIG_CUT_OLD=1 — прежнее
#ifndef BIG_CUT_OLD
#define BIG_CUT_OLD 0
#endif
#define BIG_EXT_MIN 10.f
#define BIG_EXT_MAX 150.f
#define BIG_EDGE_MIN 4.f   // ребро треугольника в мире: мельче (листва, мелкие грани) у ближней плоскости за полосу GE не уходит
typedef struct { const void *v; int ni, vs; float lo[3], hi[3], edge; unsigned char used; } BigInfo;   // edge — наибольшее ребро треугольника (локально, 0…2)
static BigInfo bigTab[512]; static int bigCutBlock; static int bigCutOld = BIG_CUT_OLD;

// ---- №674 diaglog: тело (после ggK/ggE/ggR/ggU/ggF) ----
typedef struct { unsigned frame; const void *v; int tris, risk, owner, ni, phase, tex, nearN, farN; float zmin, pxmax; } DiagEnt;
static unsigned diagListRisk, diagListCells, diagListDrop;   // №791: риск в клетках, ушедших листом (diagCellList)
static unsigned diagDropOwn[4];   // №853 служебно: выброшенные и видимые по владельцу куска (0 город, 1 машина, 2 небо, 3 прочее)
static unsigned diagDropReal, diagDropInk;   // №921: из выброшенных — без заплатки (настоящая потеря) и из них обводка машин (tex 0, фазы машин: по замыслу №853 не латается)
static unsigned diagCityDrop;   // №791: из «риска» кусков, нарисованных без среза, — точно выброшенные GE и видимые (ggDropExact)
static int ggDropExact(const GgV *a, const GgV *b, const GgV *c, const GgCam *k);
// №921 служебно (боты 24.10, выброс GE у глаза): какой кусок GE отбрасывает целиком и каким путём он нарисован — строка DIAGD раз в секунду (только diaglog)
static int diagPatch;   // 1 — кусок рисует drawGeoPatch853 (веера на месте выброшенных), 2 — drawGeo из очереди/среза с вееером, 0 — обычный drawGeo (потеря настоящая)
static struct { const void *v; int ni, tex, owner, ph, fl, patch, far, gon, bigf; unsigned n; float zmin, edge, dist, ext; } diagDt[16]; static int diagDtN;
static DiagEnt diagEnt[24]; static int diagN, diagFrRisk; static unsigned diagTotRisk, diagTotTri, diagLines, diagUs; static unsigned diagPh[24];
static const BigInfo *bigInfo(const void *v, int ni, int flags);
static void diagGeo(const Piece *d, int far, int cnt) {
  if (flatInList) return;   // №921: идёт сборка листа клетки — это не рисование (лист в кадр попадает, только если клетка не под риском: её меряет diagCellList, а куски под риском режутся и рисуются отдельно); раньше такие выбросы шли в DIAGS как «город»
  if (!diagMok || bigCutBlock || ggK.tvy <= 0.f) return;   // нет матрицы модели или бонусный уровень (оси камеры не заданы)
  unsigned us0 = sceKernelGetSystemTimeLow(); const ScePspFMatrix4 *m = &diagM;
  int absM = diagAbsM >= 0 ? diagAbsM : !(m->w.x == -ggE[0] && m->w.y == -ggE[1] && m->w.z == -ggE[2]); diagAbsM = -1;
  float T0 = m->w.x - (absM ? ggE[0] : 0.f), T1 = m->w.y - (absM ? ggE[1] : 0.f), T2 = m->w.z - (absM ? ggE[2] : 0.f);
  const float *ax[3] = {ggR, ggU, ggF}; float A[3][3], B[3];
  for (int r = 0; r < 3; r++) {
    A[r][0] = ax[r][0] * m->x.x + ax[r][1] * m->x.y + ax[r][2] * m->x.z; A[r][1] = ax[r][0] * m->y.x + ax[r][1] * m->y.y + ax[r][2] * m->y.z;
    A[r][2] = ax[r][0] * m->z.x + ax[r][1] * m->z.y + ax[r][2] * m->z.z; B[r] = ax[r][0] * T0 + ax[r][1] * T1 + ax[r][2] * T2;
  }
  int v12 = (d->r.flags & F_V12) != 0, noidx = (d->r.flags & F_NOIDX) != 0, vs = v12 ? 12 : 16, po = v12 ? 6 : 8; const float S = 1.f / 32768.f;
  const unsigned char *vb = (const unsigned char *)d->v + (noidx && far ? d->r.ni * vs : 0); const unsigned short *ix = far ? d->fidx : d->idx;
  int risk = 0, nearN = 0, farN = 0; float zmin = 1e30f, pxmax = 0.f;
  for (int t = 0; t + 2 < cnt; t += 3) {
    GgV g[3]; int bad = 0;
    for (int i = 0; i < 3; i++) {
      unsigned vi = noidx ? (unsigned)(t + i) : ix[t + i]; const short *q = (const short *)(vb + vi * vs + po);
      float lx = q[0] * S, ly = q[1] * S, lz = q[2] * S;
      for (int a = 0; a < 3; a++) g[i].c[a] = A[a][0] * lx + A[a][1] * ly + A[a][2] * lz + B[a];
    }
    if (g[0].c[2] >= GG_ZFAR && g[1].c[2] >= GG_ZFAR && g[2].c[2] >= GG_ZFAR) continue;
    if (!ggRisk(&g[0], &g[1], &g[2], &ggK)) continue;
    risk++; if (ggDropExact(&g[0], &g[1], &g[2], &ggK)) { diagCityDrop++; diagDropOwn[d->r.owner < 4 ? d->r.owner : 3]++; if (!diagPatch) { diagDropReal++; if (d->r.tex == 0 && bbPhase >= BBP_CAR3D && bbPhase <= BBP_MCR3D) diagDropInk++; }
      { int k_ = 0; for (; k_ < diagDtN; k_++) if (diagDt[k_].v == d->v && diagDt[k_].ni == d->r.ni && diagDt[k_].ph == bbPhase && diagDt[k_].patch == diagPatch) break;
        if (k_ == diagDtN && diagDtN < 16) { memset(&diagDt[k_], 0, sizeof diagDt[k_]); diagDt[k_].v = d->v; diagDt[k_].ni = d->r.ni; diagDt[k_].tex = d->r.tex; diagDt[k_].owner = d->r.owner; diagDt[k_].ph = bbPhase; diagDt[k_].fl = d->r.flags; diagDt[k_].patch = diagPatch; diagDt[k_].far = far; diagDt[k_].gon = ggOn + 2 * ggBigOnly; diagDt[k_].bigf = d->r.pad[1]; diagDt[k_].zmin = 1e30f; diagDtN++; }
        if (k_ < diagDtN) { float e2 = 0.f, zm = 1e30f; for (int i = 0; i < 3; i++) { const float *p0 = g[i].c, *p1 = g[(i + 1) % 3].c; float l2 = (p0[0] - p1[0]) * (p0[0] - p1[0]) + (p0[1] - p1[1]) * (p0[1] - p1[1]) + (p0[2] - p1[2]) * (p0[2] - p1[2]); if (l2 > e2) e2 = l2; if (p0[2] < zm) zm = p0[2]; }
          diagDt[k_].n++; if (zm < diagDt[k_].zmin) diagDt[k_].zmin = zm; if (sqrtf(e2) > diagDt[k_].edge) diagDt[k_].edge = sqrtf(e2);
          if (d->r.flags & F_NOIDX) { const BigInfo *bi_ = bigInfo(d->v, d->r.ni, d->r.flags); float ex_ = 0.f; for (int a_ = 0; a_ < 3; a_++) { float e_ = (bi_->hi[a_] - bi_->lo[a_]); if (e_ > ex_) ex_ = e_; } diagDt[k_].ext = ex_; }
          float cx_ = (g[0].c[0] + g[1].c[0] + g[2].c[0]) / 3.f, cy_ = (g[0].c[1] + g[1].c[1] + g[2].c[1]) / 3.f, cz_ = (g[0].c[2] + g[1].c[2] + g[2].c[2]) / 3.f; float dd_ = sqrtf(cx_ * cx_ + cy_ * cy_ + cz_ * cz_); if (diagDt[k_].dist == 0.f || dd_ < diagDt[k_].dist) diagDt[k_].dist = dd_; } } }   // №853: по владельцу (0 город, 1 машина, 2 небо)
    for (int i = 0; i < 3; i++) { float z = g[i].c[2]; if (z < zmin) zmin = z; if (z < ggK.zn) { nearN++; bad = 1; }
      if (z > 1e-3f) { float px = fabsf(g[i].c[0] / (ggK.tvx * z)) * 240.f; float py = fabsf(g[i].c[1] / (ggK.tvy * z)) * 136.f; if (px > pxmax) pxmax = px; if (py > pxmax) pxmax = py; if (px > 2048.f || py > 2048.f) { farN++; bad = 1; } } }
    (void)bad;
  }
  diagTotTri += (unsigned)(cnt / 3); diagTotRisk += (unsigned)risk;
  if (risk) { int p = bbPhase; if (p < 0 || p >= 24) p = 0; diagPh[p] += (unsigned)risk; diagFrRisk += risk;
    if (diagN < 24) diagEnt[diagN++] = (DiagEnt){smkFrameAbs, d->v, cnt / 3, risk, d->r.owner, d->r.ni, p, d->r.tex, nearN, farN, zmin, pxmax}; }
  diagUs += sceKernelGetSystemTimeLow() - us0;
}
// раз в кадр (BBP_VBLANK): строка DIAGF на кадр с треугольниками под риском (не больше 300 строк за прогон; остальное — счётчиками DIAGS раз в секунду)
static char diagScn[24]; static unsigned diagGndN, diagLowN, diagLowOff; static float diagGndMin = 99.f;
static void diagFrameEnd(void) {
  if (strcmp(diagScn, smkScene)) { memcpy(diagScn, smkScene, sizeof diagScn - 1); diagLines = 0; }   // не больше 40 строк DIAGF на сцену
  if (diagN && diagLines < 40) { char s[700]; int o = snprintf(s, sizeof s, "DIAGF t=%.2f sc=%s fr=%u eye=%.1f,%.1f,%.1f risk=%d", smkT, smkScene, smkFrameAbs, ggE[0], ggE[1], ggE[2], diagFrRisk);
    for (int i = 0; i < diagN && o < 600; i++) o += snprintf(s + o, sizeof s - (size_t)o, " | ph=%d own=%d tex=%d v=%p ni=%d tr=%d risk=%d near=%d far2048=%d zmin=%.2f px=%.0f", diagEnt[i].phase, diagEnt[i].owner, diagEnt[i].tex, diagEnt[i].v, diagEnt[i].ni, diagEnt[i].tris, diagEnt[i].risk, diagEnt[i].nearN, diagEnt[i].farN, diagEnt[i].zmin, diagEnt[i].pxmax);
    snprintf(s + o, sizeof s - (size_t)o, "\n"); smkWrite(s); diagLines++; }
  diagN = 0; diagFrRisk = 0;
}
static void svDiagSecond853(void);   // №853 служебно: тени (spritelight.h)
static void diagSecond(void) {
  char s[480]; snprintf(s, sizeof s, "DIAGS t=%.1f sc=%s fr=%u tris=%u risk=%u citydrop=%u drop_city=%u drop_car=%u drop_sky=%u listrisk=%u listdrop=%u listcells=%u city=%u car=%u npc=%u trf=%u mcr=%u props=%u fx=%u lines=%u diag_us=%u drop_real=%u drop_ink=%u\n", smkT, smkScene, smkFrameAbs, diagTotTri, diagTotRisk, diagCityDrop, diagDropOwn[0], diagDropOwn[1], diagDropOwn[2], diagListRisk, diagListDrop, diagListCells,
    diagPh[BBP_CITY], diagPh[BBP_CAR3D], diagPh[BBP_NPC3D], diagPh[BBP_TRF3D], diagPh[BBP_MCR3D], diagPh[BBP_PROPS3D], diagPh[BBP_FX3D], diagLines, diagUs, diagDropReal, diagDropInk);
  smkWrite(s);
  for (int k_ = 0; k_ < diagDtN; k_++) { char l_[300]; snprintf(l_, sizeof l_, "DIAGD t=%.1f sc=%s ph=%d own=%d tex=%d fl=%x ni=%d v=%p patch=%d far=%d gon=%d bigf=%d drops=%u zmin=%.2f edge=%.2f dist=%.2f ext=%.1f\n", smkT, smkScene, diagDt[k_].ph, diagDt[k_].owner, diagDt[k_].tex, diagDt[k_].fl, diagDt[k_].ni, diagDt[k_].v, diagDt[k_].patch, diagDt[k_].far, diagDt[k_].gon, diagDt[k_].bigf, diagDt[k_].n, diagDt[k_].zmin, diagDt[k_].edge, diagDt[k_].dist, diagDt[k_].ext); smkWrite(l_); }
  diagDtN = 0; diagDropReal = diagDropInk = 0; svDiagSecond853(); diagTotTri = diagTotRisk = diagUs = 0; diagListRisk = diagListCells = diagListDrop = diagCityDrop = 0; memset(diagDropOwn, 0, sizeof diagDropOwn); memset(diagPh, 0, sizeof diagPh);
}
// №791 (камера ломает пол/асфальт на PSP): diagGeo видит только куски, которые рисуются по одному. Клетка у камеры, ушедшая ЛИСТОМ (ggOn нет или клетка не в риске),
// мимо него: её треугольники под риском полосы GE (вершина за near или за ±2048 px) на PSP GE выбрасывает целиком, а эмулятор с окном рисует. Здесь — тот же ggRisk по
// мировым вершинам непрозрачных кусков клетки (как camGeomCollect). Только «flag diaglog 1»: в строку DIAGS (city, listrisk) и DIAGF (ph=99 — лист)
// модель GE для замера (psp-render §4, ge_guard.h): треугольник режется по near (z >= zn), каждая вершина результата проецируется; хоть одна за ±2048 px —
// GE выбрасывает треугольник целиком. 1 — выброшен и он был бы на экране (не целиком за одной гранью кадра)
static int ggDropExact(const GgV *a, const GgV *b, const GgV *c, const GgCam *k) {
  const GgV *v[3] = {a, b, c}; float p[6][3]; int n = 0;
  for (int i = 0; i < 3; i++) {
    const float *A = v[i]->c, *B = v[(i + 1) % 3]->c; int ia = A[2] >= k->zn, ib = B[2] >= k->zn;
    if (ia) { p[n][0] = A[0]; p[n][1] = A[1]; p[n][2] = A[2]; n++; }
    if (ia != ib) { float t = (k->zn - A[2]) / (B[2] - A[2]); p[n][0] = A[0] + (B[0] - A[0]) * t; p[n][1] = A[1] + (B[1] - A[1]) * t; p[n][2] = k->zn; n++; }
  }
  if (n < 3) return 0;
  int out[4] = {0, 0, 0, 0}, drop = 0;
  for (int i = 0; i < n; i++) { float z = p[i][2]; if (z < 1e-4f) z = 1e-4f; float sx = p[i][0] / (k->tvx * z) * 240.f, sy = p[i][1] / (k->tvy * z) * 136.f;
    if (sx > 2048.f || sx < -2048.f || sy > 2048.f || sy < -2048.f) drop = 1;
    out[0] += sx > 240.f; out[1] += sx < -240.f; out[2] += sy > 136.f; out[3] += sy < -136.f; }
  for (int i = 0; i < 4; i++) if (out[i] == n) return 0;   // целиком за одной гранью кадра — не на экране
  return drop;
}
static void diagCellList(unsigned ci) {
  if (bigCutBlock || ggK.tvy <= 0.f || ci >= ncell || !pc) return;
  unsigned us0 = sceKernelGetSystemTimeLow(); int riskC = 0, nearN = 0, farN = 0, worstR = 0; float zmin = 1e30f, pxmax = 0.f; const Piece *worst = 0; const float S = 1.f / 32768.f;
  for (unsigned k = cl[ci].first; k < cl[ci].first + cl[ci].n && k < npc; k++) {
    const Piece *d = &pc[k]; const Rec *r = &d->r;
    if (r->owner != 0 || (r->flags & (2 | 4)) || !d->v || r->ni < 3 || r->sc <= 0.f) continue;
    int v12 = (r->flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8, noidx = (r->flags & F_NOIDX) != 0, n = r->ni - r->ni % 3;
    if (!noidx && !d->idx) continue;
    const char *vbase = (const char *)d->v;
    if (r->flags & F_STRIP) { unsigned nl_; vbase = (const char *)pieceListT(d, 1, &nl_); if (!vbase) continue; n = (int)(nl_ - nl_ % 3); }
    int pr = 0;
    for (int t = 0; t + 2 < n; t += 3) {
      GgV g[3]; int allFar = 1;
      for (int i = 0; i < 3; i++) {
        unsigned vi = noidx ? (unsigned)(t + i) : d->idx[t + i]; const short *xy = (const short *)(vbase + vi * vs + xo);
        float w0 = r->ox + r->sc * S * xy[0] - ggE[0], w1 = r->oy + r->sc * S * xy[1] - ggE[1], w2 = r->oz + r->sc * S * xy[2] - ggE[2];
        g[i].c[0] = w0 * ggR[0] + w1 * ggR[1] + w2 * ggR[2]; g[i].c[1] = w0 * ggU[0] + w1 * ggU[1] + w2 * ggU[2]; g[i].c[2] = w0 * ggF[0] + w1 * ggF[1] + w2 * ggF[2];
        if (g[i].c[2] < GG_ZFAR) allFar = 0;
      }
      if (allFar || !ggRisk(&g[0], &g[1], &g[2], &ggK)) continue;
      pr++; if (ggDropExact(&g[0], &g[1], &g[2], &ggK)) diagListDrop++;
      for (int i = 0; i < 3; i++) { float z = g[i].c[2]; if (z < zmin) zmin = z; if (z < ggK.zn) nearN++;
        if (z > 1e-3f) { float px = fabsf(g[i].c[0] / (ggK.tvx * z)) * 240.f, py = fabsf(g[i].c[1] / (ggK.tvy * z)) * 136.f; if (px > pxmax) pxmax = px; if (py > pxmax) pxmax = py; if (px > 2048.f || py > 2048.f) farN++; } }
    }
    if (pr > worstR) { worstR = pr; worst = d; }
    riskC += pr;
  }
  if (riskC) { diagPh[BBP_CITY] += (unsigned)riskC; diagFrRisk += riskC; diagTotRisk += (unsigned)riskC; diagListRisk += (unsigned)riskC; diagListCells++;
    if (diagN < 24 && worst) diagEnt[diagN++] = (DiagEnt){smkFrameAbs, worst->v, worst->r.ni / 3, riskC, 0, worst->r.ni, 99, worst->r.tex, nearN, farN, zmin, pxmax}; }
  diagUs += sceKernelGetSystemTimeLow() - us0;
}
static unsigned bigInfoMiss853;
static const BigInfo *bigInfo(const void *v, int ni, int flags) {
  unsigned h = (unsigned)(((uintptr_t)v >> 4) ^ ((unsigned)ni * 2654435761u)) & 511u; BigInfo *b = &bigTab[h]; int probe = 0;
  for (; probe < 16; probe++, b = &bigTab[(h + probe) & 511u]) { if (b->used && b->v == v && b->ni == ni) return b; if (!b->used) break; }
  if (probe >= 16) b = &bigTab[h];   // таблица забита: вытеснить первую (редко: типов кусков ≪ 2048)
  bigInfoMiss853++;   // №853 служебно: пересчёт бокса и рёбер куска (промах таблицы)
  int v12 = (flags & F_V12) != 0, vs = v12 ? 12 : 16, po = v12 ? 6 : 8; float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  for (int i = 0; i < ni; i++) { const short *q = (const short *)((const char *)v + i * vs + po);
    for (int a = 0; a < 3; a++) { float x = q[a] * (1.f / 32768.f); if (x < lo[a]) lo[a] = x; if (x > hi[a]) hi[a] = x; } }
  float e2 = 0.f; for (int t = 0; t + 2 < ni; t += 3) for (int k = 0; k < 3; k++) { const short *p = (const short *)((const char *)v + (t + k) * vs + po), *q = (const short *)((const char *)v + (t + (k + 1) % 3) * vs + po);
    float dx = (p[0] - q[0]) * (1.f / 32768.f), dy = (p[1] - q[1]) * (1.f / 32768.f), dz = (p[2] - q[2]) * (1.f / 32768.f), l2 = dx * dx + dy * dy + dz * dz; if (l2 > e2) e2 = l2; }
  b->edge = sqrtf(e2); b->v = v; b->ni = ni; b->vs = vs; b->used = 1; memcpy(b->lo, lo, sizeof lo); memcpy(b->hi, hi, sizeof hi); return b;
}
// №984 (наряд «город в езде L3–L7»): проверки риска выброса GE на кусок — сначала дешёвый отсев без корней (по квадратам), точный расчёт только если отсев не отбросил. Отсев консервативен (запас 0,01 %):
// отбрасывает только то, что прежний расчёт тоже отбросил, поэтому результат тот же. TRFGEO984_OLD=1 / «flag trfgeo984 0» / файл trfgeo984old — прежний расчёт; «flag trfgeocheck 1» — каждую проверку
// считать обоими путями и считать расхождения (служебно, гейт tests/trfgeo984); -DTRFGEO984_BREAK=1 — красный контроль (отсев грубее, расхождения должны появиться)
#ifndef TRFGEO984_OLD
#define TRFGEO984_OLD 0
#endif
#ifdef TRFGEO984_BREAK
#define TRG_K 50.f
#else
#define TRG_K .9999f
#endif
static int trfGeoOld = TRFGEO984_OLD, trgCheck; static unsigned trgMis, trgChk;
static int trgIn; static unsigned trgN, trgBnUs, trgCnUs, trgFastN, trgDrawUs, trgPieces;
// крупный кусок без индексов с матрицей m: размер (м) и центр в мире; 0 — не крупный
static float bigSize(const void *v, int ni, int flags, const float m[16], float c[3]) {
  if (bigCutOld || !(flags & F_NOIDX) || ni < 3 || ni > 65535) return 0.f;
  const BigInfo *b = bigInfo(v, ni, flags); float mx = 0.f, ce[3];
  if (!trfGeoOld) { int small_ = 1; for (int a = 0; a < 3; a++) { const float *col = m + 4 * a; float e = b->hi[a] - b->lo[a]; if (e * e * (col[0] * col[0] + col[1] * col[1] + col[2] * col[2]) >= BIG_EXT_MIN * BIG_EXT_MIN * TRG_K) { small_ = 0; break; } } if (small_) return 0.f; }   // №984: ни одна сторона бокса не дотягивает до BIG_EXT_MIN — прежний расчёт (mx < BIG_EXT_MIN) тоже вернул бы 0, без шести корней
  for (int a = 0; a < 3; a++) { const float *col = m + 4 * a; float sc = sqrtf(col[0] * col[0] + col[1] * col[1] + col[2] * col[2]); float e = (b->hi[a] - b->lo[a]) * sc; if (e > mx) mx = e; ce[a] = (b->lo[a] + b->hi[a]) * .5f; }
  if (mx < BIG_EXT_MIN || mx >= BIG_EXT_MAX) return 0.f;
  { float s0 = 0.f; for (int a = 0; a < 3; a++) { const float *col = m + 4 * a; float sc = sqrtf(col[0] * col[0] + col[1] * col[1] + col[2] * col[2]); if (sc > s0) s0 = sc; } if (b->edge * s0 < BIG_EDGE_MIN) return 0.f; }
  for (int r = 0; r < 3; r++) c[r] = m[r] * ce[0] + m[4 + r] * ce[1] + m[8 + r] * ce[2] + m[12 + r];
  return mx;
}
// бокс куска в мире (оболочка восьми углов локального бокса): целиком в безопасной пирамиде или вне кадра — резать нечего (ggBoxRisk, как у города)
static void bigWorldBox(const void *v, int ni, int flags, const float *mm, float bb[6]) {
  const BigInfo *b = bigInfo(v, ni, flags); bb[0] = bb[1] = bb[2] = 1e30f; bb[3] = bb[4] = bb[5] = -1e30f;
  for (int k = 0; k < 8; k++) { float l0 = b->lo[0] + (b->hi[0] - b->lo[0]) * (k & 1), l1 = b->lo[1] + (b->hi[1] - b->lo[1]) * ((k >> 1) & 1), l2 = b->lo[2] + (b->hi[2] - b->lo[2]) * ((k >> 2) & 1);
    for (int r = 0; r < 3; r++) { float w = mm[r] * l0 + mm[4 + r] * l1 + mm[8 + r] * l2 + mm[12 + r]; if (w < bb[r]) bb[r] = w; if (w > bb[3 + r]) bb[3 + r] = w; } }
}
// №984 служебно (трафик, проверки риска выброса GE на кусок машины NPC): trgIn — идёт отрисовка куска трафика (trDrawPiece); мкс и число проверок за секунду — строка TRFGEO984

static int bigNear1(const Piece *d, const ScePspFMatrix4 *m) {
  if (bigCutBlock) return 0;   // бонусные уровни: оси камеры ggR/ggU/ggF/ggK там не заданы
  float c[3], sz = bigSize(d->v, d->r.ni, d->r.flags, (const float *)m, c); if (sz <= 0.f) return 0;
  float dx = c[0] - ggE[0], dy = c[1] - ggE[1], dz = c[2] - ggE[2]; if (dx * dx + dy * dy + dz * dz >= sz * sz) return 0;
  float bb[6]; bigWorldBox(d->v, d->r.ni, d->r.flags, (const float *)m, bb);
  return ggBoxRiskW(bb);
}
static int bigNear(const Piece *d, const ScePspFMatrix4 *m) {   // №984: то же для bigNear
  if (!trgCheck) return bigNear1(d, m);
  int sv = trfGeoOld; trfGeoOld = 1; int a = bigNear1(d, m); trfGeoOld = 0; int b = bigNear1(d, m); trfGeoOld = sv; trgChk++; if (a != b) trgMis++; return sv ? a : b;
}
// №853: кусок машины у камеры. Когда GE вообще может выбросить треугольник (модель ggDropExact, запас GG_SAFE: Gx = GG_SAFE/240, Gy = GG_SAFE/136):
// вершина q многоугольника после среза по near (z ≥ zn) лежит за полосой, |x| > Gx·tvx·z или |y| > Gy·tvy·z, а треугольник виден (точка p внутри кадра).
// Отсюда для треугольника с наибольшим ребром e: (1) e > K·zn, K = (Gx−1)·tvx/(1+tvx) (по вертикали Ky, берётся меньший); (2) у него есть вершина
// мельче e/K; (3) хоть одна вершина дальше Gx·tvx·zn вбок или Gy·tvy·zn по вертикали (q — выпуклая смесь вершин). У куска и групп треугольников
// (ggBatch) — то же по боксу: мелкие детали (колёса, салон) отсеиваются по (1) без расчёта, кузов вдали — по (2), кузов сбоку от взгляда — по (3)
typedef struct { float K, iK, lx, ly, gx, gy, zn, tvx, tvy; } CarDropK;
static CarDropK carDropK(void) {   // параметры полосы — пересчёт только при смене ближней плоскости или угла кадра
  static CarDropK c; if (c.zn == ggK.zn && c.tvx == ggK.tvx && c.tvy == ggK.tvy && c.K > 0.f) return c;
  float Gx = GG_SAFE / 240.f, Gy = GG_SAFE / 136.f, kx = (Gx - 1.f) * ggK.tvx / (1.f + ggK.tvx), ky = (Gy - 1.f) * ggK.tvy / (1.f + ggK.tvy);
  c.K = kx < ky ? kx : ky; c.iK = 1.f / c.K; c.gx = Gx * ggK.tvx; c.gy = Gy * ggK.tvy; c.lx = c.gx * ggK.zn; c.ly = c.gy * ggK.zn; c.zn = ggK.zn; c.tvx = ggK.tvx; c.tvy = ggK.tvy; return c;
}
static float matScale(const float *mm) { float s0 = 0.f; for (int a = 0; a < 3; a++) { const float *col = mm + 4 * a; float q = col[0] * col[0] + col[1] * col[1] + col[2] * col[2]; if (q > s0) s0 = q; } return sqrtf(s0); }
static unsigned carNearN, carNearCut, carPatchTri, carPatchFans, carPatchUs, carNearUs, cltBuilds, cltBuildUs, cltFailN;
// GE выбросит треугольник (модель ggDropExact без делений): вершина многоугольника после среза по near за полосой lim px; треугольник не целиком за гранью кадра
static int ggDropLim(const GgV *a, const GgV *b, const GgV *c, const GgCam *k, float lim) {
  const GgV *v[3] = {a, b, c}; float p[6][3]; int n = 0;
  for (int i = 0; i < 3; i++) {
    const float *A = v[i]->c, *B = v[(i + 1) % 3]->c; int ia = A[2] >= k->zn, ib = B[2] >= k->zn;
    if (ia) { p[n][0] = A[0]; p[n][1] = A[1]; p[n][2] = A[2]; n++; }
    if (ia != ib) { float t = (k->zn - A[2]) / (B[2] - A[2]); p[n][0] = A[0] + (B[0] - A[0]) * t; p[n][1] = A[1] + (B[1] - A[1]) * t; p[n][2] = k->zn; n++; }
  }
  if (n < 3) return 0;
  float gx = k->tvx * (lim / 240.f), gy = k->tvy * (lim / 136.f); int out[4] = {0, 0, 0, 0}, drop = 0;
  for (int i = 0; i < n; i++) { float x = p[i][0], y = p[i][1], z = p[i][2];
    if (fabsf(x) > gx * z || fabsf(y) > gy * z) drop = 1;
    out[0] += x > k->tvx * z; out[1] += -x > k->tvx * z; out[2] += y > k->tvy * z; out[3] += -y > k->tvy * z; }
  for (int i = 0; i < 4; i++) if (out[i] == n) return 0;
  return drop;
}
// Длинные треугольники куска машины (наибольшее ребро в мире больше CLT_EMIN = K·0,22 м — короче GE не выбросит при near от 0,22 м, правило (1)),
// по месту (код Мортона центра в боксе куска) и группами по CLT_G с боксом группы в осях куска: у камеры проходят единицы групп, не сотни треугольников.
// Строится один раз на кусок (ключ — вершины, число, сумма первых координат); память — свой участок, обнуляется при смене уровня (carLtReset)
#define CLT_EMIN .68f
#define CLT_G 8
#define CLT_N 96
#define CLT_POOL (24u << 10)
typedef struct { const void *v; int ni; unsigned sum; int nl, ng; unsigned short *t; float *gb; unsigned last, h0, h1; short bad, quiet; } CarLT;   // gb: по 7 чисел на группу — бокс (6) и наибольшее ребро в осях куска
static CarLT carLT[CLT_N]; static int carLTn; static unsigned char cltPool[CLT_POOL] __attribute__((aligned(16))); static unsigned cltUsed;
static unsigned cltDiagN;   // №940: сколько строк CLT940 записано на этом уровне
static void carLtReset(void) { carLTn = 0; cltUsed = 0; cltDiagN = 0; }
static unsigned cltLim940 = CLT_POOL;   // №940: предел участка (служебно «flag cltpool940»), по умолчанию весь
static void *cltAlloc(unsigned n) { n = (n + 15u) & ~15u; if (cltUsed + n > cltLim940) return 0; void *p = cltPool + cltUsed; cltUsed += n; return p; }
static unsigned cltMorton(unsigned x) { x &= 1023u; x = (x | x << 16) & 0x030000FFu; x = (x | x << 8) & 0x0300F00Fu; x = (x | x << 4) & 0x030C30C3u; x = (x | x << 2) & 0x09249249u; return x; }
static unsigned cltKey_[1024]; static unsigned short cltIdx_[1024];
// №940 (L3 набережная на PSP 28 -> 18 к/с, CARPATCH patch_us 287 мс в секунду, tri=0): участок CLT_POOL (24 КБ) вмещает ~12 кусков; не влезший кусок строился заново при КАЖДОМ вызове
// (сортировка вставками до 1024 треугольников = 3,4 мс PSP), потому что отказ нигде не запоминался, а недозаписанный кусок ещё и тёк участком. Теперь: (1) отказ запоминается в таблице
// на CLT_RETRY кадров; (2) недозапись откатывается; (3) не больше CLT_BUILDS_F построек за кадр, остальное в следующем кадре; (4) при нехватке места участок уплотняется (записи, не
// тронутые CLT_KEEP кадров, уходят, остальные сдвигаются по порядку); (5) сортировка Шелла по упакованному ключу на месте (O(n log n) вместо вставок O(n²), без доп. памяти); (6) тот же кадр и матрица, и прошлый обход выбросов не нашёл — обход пропускается (cltHash, стоянка). CLT_FIX940_OLD=1 /
// «flag cltfix940 0» / файл cltfix940old рядом с EBOOT — прежнее. Для гейта: «flag cltpool940 N» — предел участка N байт (имитация нехватки), «flag cltstress940 1» — ключ меняется каждый кадр
#ifndef CLT_FIX940_OLD
#define CLT_FIX940_OLD 0
#endif
#define CLT_BUILDS_F 2
#define CLT_RETRY 300
#define CLT_KEEP 120
static int cltFix940 = !CLT_FIX940_OLD, cltStress940; static unsigned cltFrame, cltGen, cltBuiltF, cltDeferN, cltCompactN, cltBadHit, cltQuietN;
static unsigned cltCompactBad, cltCmpF = ~0u;
static unsigned cltSumArr(const void *p, unsigned n) { const unsigned char *c = (const unsigned char *)p; unsigned h = 2166136261u; for (unsigned i = 0; i < n; i++) h = (h ^ c[i]) * 16777619u; return h; }
static void cltCompact(void) {
  unsigned cur = 0; int w = 0;
  for (int i = 0; i < carLTn; i++) { CarLT e = carLT[i]; if (e.bad ? cltFrame - e.last >= CLT_RETRY : cltFrame - e.last > CLT_KEEP) continue;   // отказ живёт CLT_RETRY кадров, запись — CLT_KEEP кадров без обращений
    unsigned s0 = 0, s1 = 0, nt = (unsigned)e.nl * 2u, ngb = (unsigned)e.ng * 28u;
    if (smkOn) { if (e.t) s0 = cltSumArr(e.t, nt); if (e.gb) s1 = cltSumArr(e.gb, ngb); }   // служебно (смоук): сверка содержимого до/после сдвига
    if (e.t) { if (cltPool + cur != (unsigned char *)e.t) memmove(cltPool + cur, e.t, nt); e.t = (unsigned short *)(cltPool + cur); cur += (nt + 15u) & ~15u; }
    if (e.gb) { if (cltPool + cur != (unsigned char *)e.gb) memmove(cltPool + cur, e.gb, ngb); e.gb = (float *)(cltPool + cur); cur += (ngb + 15u) & ~15u; }
#ifdef CLT_BREAK940
    if (e.gb && w == 0) ((unsigned char *)e.gb)[1] ^= 1;   // ТОЛЬКО красный контроль гейта: порча одного байта после сдвига (сверка cbad обязана заметить)
#endif

    if (smkOn && ((e.t && cltSumArr(e.t, nt) != s0) || (e.gb && cltSumArr(e.gb, ngb) != s1))) cltCompactBad++;
    carLT[w++] = e; }
  carLTn = w; cltUsed = cur; cltGen++; cltCompactN++;
}
static void cltCompactOnce(void) { if (cltCmpF != cltFrame) { cltCmpF = cltFrame; cltCompact(); } }   // не чаще раза в кадр: уплотнять нечего, если ничего не устарело
static void *cltAlloc940(unsigned n) { n = (n + 15u) & ~15u; if (cltUsed + n > cltLim940) return 0; void *p = cltPool + cltUsed; cltUsed += n; return p; }
static void cltSort940(int nl) {   // на месте (без доп. памяти): ключ Мортона 7 бит на ось << 10 | номер треугольника в куске; сортировка Шелла (шаги Циура), порядок равных — по номеру
  unsigned *k = cltKey_; static const int gap[8] = {701, 301, 132, 57, 23, 10, 4, 1};
  for (int gi = 0; gi < 8; gi++) { int g = gap[gi]; if (g >= nl) continue;
    for (int i = g; i < nl; i++) { unsigned v = k[i]; int j = i; while (j >= g && k[j - g] > v) { k[j] = k[j - g]; j -= g; } k[j] = v; } }
  for (int i = 0; i < nl; i++) cltIdx_[i] = (unsigned short)((k[i] & 1023u) * 3u);
}
static const CarLT *carLongTris_(const Piece *d, float sc, int *built) {
  int vs = (d->r.flags & F_V12) ? 12 : 16, po = (d->r.flags & F_V12) ? 6 : 8, ni = d->r.ni; const unsigned char *vb = (const unsigned char *)d->v;
  unsigned sum = 0; for (int i = 0; i < 4 && i < ni; i++) { const short *q = (const short *)(vb + i * vs + po); sum = sum * 31u + (unsigned short)q[0] + (unsigned)(unsigned short)q[1] * 7u + (unsigned)(unsigned short)q[2] * 13u; }
  for (int i = 0; i < carLTn; i++) if (carLT[i].v == d->v && carLT[i].ni == ni && carLT[i].sum == sum) return &carLT[i];
  if (carLTn >= CLT_N || ni / 3 > 1024 || sc <= 0.f) return 0;
  *built = 1; CarLT *L = &carLT[carLTn]; const float S = 1.f / 32768.f, e2min = (CLT_EMIN / sc) * (CLT_EMIN / sc);
  const BigInfo *b = bigInfo(d->v, ni, d->r.flags); float lo[3] = {b->lo[0], b->lo[1], b->lo[2]}, ex[3];
  for (int a = 0; a < 3; a++) { ex[a] = b->hi[a] - lo[a]; if (ex[a] < 1e-6f) ex[a] = 1e-6f; }
  int nl = 0;
  for (int t = 0; t + 2 < ni; t += 3) { float P[3][3], e2 = 0.f;
    for (int k = 0; k < 3; k++) { const short *q = (const short *)(vb + (t + k) * vs + po); P[k][0] = q[0] * S; P[k][1] = q[1] * S; P[k][2] = q[2] * S; }
    for (int k = 0; k < 3; k++) { float dx = P[k][0] - P[(k + 1) % 3][0], dy = P[k][1] - P[(k + 1) % 3][1], dz = P[k][2] - P[(k + 1) % 3][2], l2 = dx * dx + dy * dy + dz * dz; if (l2 > e2) e2 = l2; }
    if (e2 <= e2min) continue;
    unsigned c[3]; for (int a = 0; a < 3; a++) { float m = ((P[0][a] + P[1][a] + P[2][a]) * (1.f / 3.f) - lo[a]) / ex[a]; c[a] = (unsigned)(fmaxf(0.f, fminf(1.f, m)) * 1023.f); }
    cltKey_[nl] = cltMorton(c[0]) | cltMorton(c[1]) << 1 | cltMorton(c[2]) << 2; cltIdx_[nl] = (unsigned short)t; nl++; }
  for (int i = 1; i < nl; i++) { unsigned k = cltKey_[i]; unsigned short x = cltIdx_[i]; int j = i - 1; while (j >= 0 && cltKey_[j] > k) { cltKey_[j + 1] = cltKey_[j]; cltIdx_[j + 1] = cltIdx_[j]; j--; } cltKey_[j + 1] = k; cltIdx_[j + 1] = x; }
  int ng = (nl + CLT_G - 1) / CLT_G;
  L->t = nl ? (unsigned short *)cltAlloc((unsigned)nl * 2u) : 0; L->gb = ng ? (float *)cltAlloc((unsigned)ng * 7u * 4u) : 0;
  if (nl && (!L->t || !L->gb)) return 0;   // участок полон: кусок без заплатки
  for (int i = 0; i < nl; i++) L->t[i] = cltIdx_[i];
  for (int g = 0; g < ng; g++) { float *B = L->gb + 7 * g, e2g = 0.f; B[0] = B[1] = B[2] = 1e30f; B[3] = B[4] = B[5] = -1e30f;
    for (int i = g * CLT_G; i < nl && i < (g + 1) * CLT_G; i++) { float P[3][3];
      for (int k = 0; k < 3; k++) { const short *q = (const short *)(vb + (L->t[i] + k) * vs + po);
        for (int a = 0; a < 3; a++) { float x = q[a] * S; P[k][a] = x; if (x < B[a]) B[a] = x; if (x > B[3 + a]) B[3 + a] = x; } }
      for (int k = 0; k < 3; k++) { float dx = P[k][0] - P[(k + 1) % 3][0], dy = P[k][1] - P[(k + 1) % 3][1], dz = P[k][2] - P[(k + 1) % 3][2], l2 = dx * dx + dy * dy + dz * dz; if (l2 > e2g) e2g = l2; } }
    B[6] = sqrtf(e2g); }
  L->v = d->v; L->ni = ni; L->sum = sum; L->nl = nl; L->ng = ng; L->bad = 0; L->quiet = 0; carLTn++; return L;
}
static const CarLT *carLongTris940(const Piece *d, float sc, int *built) {
  int vs = (d->r.flags & F_V12) ? 12 : 16, po = (d->r.flags & F_V12) ? 6 : 8, ni = d->r.ni; const unsigned char *vb = (const unsigned char *)d->v;
  unsigned sum = 0; for (int i = 0; i < 4 && i < ni; i++) { const short *q = (const short *)(vb + i * vs + po); sum = sum * 31u + (unsigned short)q[0] + (unsigned)(unsigned short)q[1] * 7u + (unsigned)(unsigned short)q[2] * 13u; }
  if (cltStress940) sum ^= cltFrame * 2654435761u;   // служебно (гейт): ключ меняется каждый кадр — кэш не попадает
  for (int i = 0; i < carLTn; i++) if (carLT[i].v == d->v && carLT[i].ni == ni && carLT[i].sum == sum) { CarLT *E = &carLT[i];
    if (!E->bad) { E->last = cltFrame; return E; }
    if (cltFrame - E->last < CLT_RETRY) { cltBadHit++; return 0; }   // отказ запомнен: без пересчёта
    cltCompact(); break; }   // срок отказа вышел: уплотнить (эта запись уходит) и попробовать как новый
  if (ni / 3 > 1024 || sc <= 0.f) return 0;
  if (cltBuiltF >= CLT_BUILDS_F) { cltDeferN++; return 0; }   // предел построек на кадр; кусок получит заплатку в следующем кадре
  if (carLTn >= CLT_N) { cltCompactOnce(); if (carLTn >= CLT_N) return 0; }
  cltBuiltF++; *built = 1;
  const float S = 1.f / 32768.f, e2min = (CLT_EMIN / sc) * (CLT_EMIN / sc);
  const BigInfo *b = bigInfo(d->v, ni, d->r.flags); float lo[3] = {b->lo[0], b->lo[1], b->lo[2]}, ex[3];
  for (int a = 0; a < 3; a++) { ex[a] = b->hi[a] - lo[a]; if (ex[a] < 1e-6f) ex[a] = 1e-6f; }
  int nl = 0;
  for (int t = 0; t + 2 < ni; t += 3) { float P[3][3], e2 = 0.f;
    for (int k = 0; k < 3; k++) { const short *q = (const short *)(vb + (t + k) * vs + po); P[k][0] = q[0] * S; P[k][1] = q[1] * S; P[k][2] = q[2] * S; }
    for (int k = 0; k < 3; k++) { float dx = P[k][0] - P[(k + 1) % 3][0], dy = P[k][1] - P[(k + 1) % 3][1], dz = P[k][2] - P[(k + 1) % 3][2], l2 = dx * dx + dy * dy + dz * dz; if (l2 > e2) e2 = l2; }
    if (e2 <= e2min) continue;
    unsigned c[3]; for (int a = 0; a < 3; a++) { float m = ((P[0][a] + P[1][a] + P[2][a]) * (1.f / 3.f) - lo[a]) / ex[a]; c[a] = (unsigned)(fmaxf(0.f, fminf(1.f, m)) * 127.f); }
    cltKey_[nl] = (cltMorton(c[0]) | cltMorton(c[1]) << 1 | cltMorton(c[2]) << 2) << 10 | (unsigned)(t / 3); nl++; }
  cltSort940(nl);
  int ng = (nl + CLT_G - 1) / CLT_G; unsigned short *T = 0; float *GB = 0; int ok = 1;
  for (int pass = 0; pass < 2 && nl; pass++) {   // не влезло — один раз уплотнить участок и повторить; недозапись откатывается (не течёт)
    unsigned u0 = cltUsed; T = (unsigned short *)cltAlloc940((unsigned)nl * 2u); GB = T ? (float *)cltAlloc940((unsigned)ng * 7u * 4u) : 0;
    if (T && GB) { ok = 1; break; }
    cltUsed = u0; ok = 0; T = 0; GB = 0; if (pass == 0) cltCompactOnce();
  }
  if (!ok) {   // отказ запоминается: ни пересчёта, ни сортировки до CLT_RETRY кадров
    if (carLTn < CLT_N) { CarLT *B = &carLT[carLTn++]; memset(B, 0, sizeof *B); B->v = d->v; B->ni = ni; B->sum = sum; B->bad = 1; B->last = cltFrame; }
    return 0;
  }
  CarLT *L = &carLT[carLTn]; L->t = T; L->gb = GB;
  for (int i = 0; i < nl; i++) L->t[i] = cltIdx_[i];
  for (int g = 0; g < ng; g++) { float *B = L->gb + 7 * g, e2g = 0.f; B[0] = B[1] = B[2] = 1e30f; B[3] = B[4] = B[5] = -1e30f;
    for (int i = g * CLT_G; i < nl && i < (g + 1) * CLT_G; i++) { float P[3][3];
      for (int k = 0; k < 3; k++) { const short *q = (const short *)(vb + (L->t[i] + k) * vs + po);
        for (int a = 0; a < 3; a++) { float x = q[a] * S; P[k][a] = x; if (x < B[a]) B[a] = x; if (x > B[3 + a]) B[3 + a] = x; } }
      for (int k = 0; k < 3; k++) { float dx = P[k][0] - P[(k + 1) % 3][0], dy = P[k][1] - P[(k + 1) % 3][1], dz = P[k][2] - P[(k + 1) % 3][2], l2 = dx * dx + dy * dy + dz * dz; if (l2 > e2g) e2g = l2; } }
    B[6] = sqrtf(e2g); }
  L->v = d->v; L->ni = ni; L->sum = sum; L->nl = nl; L->ng = ng; L->bad = 0; L->quiet = 0; L->last = cltFrame; carLTn++;
  if (smkOn && cltDiagN < 40) { cltDiagN++; char l_[160]; snprintf(l_, sizeof l_, "CLT940 sc=%s ni=%d nl=%d ng=%d tex=%d own=%d used=%u/%u n=%d\n", smkScene, ni, nl, ng, d->r.tex, d->r.owner, cltUsed, cltLim940, carLTn); smkWrite(l_); }
  return L;
}
static const CarLT *carLongTris(const Piece *d, float sc) { int bl = 0; unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; const CarLT *r = cltFix940 ? carLongTris940(d, sc, &bl) : carLongTris_(d, sc, &bl); if (smkOn) { if (bl) { cltBuilds++; cltBuildUs += sceKernelGetSystemTimeLow() - t0; } if (!r) cltFailN++; } return r; }
// №940: тот же кадр камеры и матрица куска, и прошлый полный обход не нашёл ни одного выброса — обход повторять нечего (камера стоит: стоянка, пауза, съёмка). Два 32-битных хеша от битов 31 числа
static void cltHash(const ScePspFMatrix4 *m, unsigned *h0, unsigned *h1) {
  float f[31]; memcpy(f, m, 64); memcpy(f + 16, ggE, 12); memcpy(f + 19, ggR, 12); memcpy(f + 22, ggU, 12); memcpy(f + 25, ggF, 12); f[28] = ggK.zn; f[29] = ggK.tvx; f[30] = ggK.tvy;
  unsigned a = 2166136261u, b = 0x9E3779B9u;
  for (int i = 0; i < 31; i++) { unsigned u; memcpy(&u, &f[i], 4); a = (a ^ u) * 16777619u; a ^= a >> 13; b = (b + u) * 2654435761u; b ^= b << 7; }
  *h0 = a; *h1 = b;
}
static int cltBoxSkip(const float *bx, float A[3][3], const float B[3], const CarDropK *q, float zk) {   // 1 — в боксе GE не выбросит ни один длинный треугольник (правила (2), безопасная пирамида, вне кадра)
  float lo3[3], hi3[3], zlim = bx[6] * zk;   // (2) по наибольшему ребру
  for (int r = 0; r < 3; r++) { float l = B[r], h = B[r]; for (int a = 0; a < 3; a++) { float p0 = A[r][a] * bx[a], p1 = A[r][a] * bx[3 + a]; if (p0 < p1) { l += p0; h += p1; } else { l += p1; h += p0; } } lo3[r] = l; hi3[r] = h; }
  if (lo3[2] >= zlim || hi3[2] < ggK.zn) return 1;   // (2) и целиком за ближней плоскостью
  float zc = lo3[2] > ggK.zn ? lo3[2] : ggK.zn;   // часть перед near — внутри пирамиды полосы целиком: выброса нет
  if (hi3[0] <= q->gx * zc && -lo3[0] <= q->gx * zc && hi3[1] <= q->gy * zc && -lo3[1] <= q->gy * zc) return 1;
  if (lo3[0] > q->tvx * hi3[2] || -hi3[0] > q->tvx * hi3[2] || lo3[1] > q->tvy * hi3[2] || -hi3[1] > q->tvy * hi3[2]) return 1;   // вне кадра
  return 0;
}
// кусок рисуется целиком как обычно (один вызов); затем по группам длинных треугольников (carLongTris) только те, что GE выбросит (ggDropLim), —
// срез по пирамиде полосы, веером на их место. Остальное GE рисует сам. Стекло (флаг 2) — по точной полосе 2048: двойное наложение было бы видно
static void drawGeoPatch853(const Piece *d, const ScePspFMatrix4 *m) {
  unsigned f0_ = carPatchFans; int cnt = d->r.ni; diagPatch = 1; diagAbsM = 1; drawGeo(d, 0, cnt); diagPatch = 0;
  if ((d->r.flags & F_STRIP) || !(d->r.flags & F_NOIDX)) return;
  unsigned us0 = smkOn ? sceKernelGetSystemTimeLow() : 0, vb0_ = ggVbUsed;
  float sc = matScale((const float *)m); const CarLT *L = carLongTris(d, sc);
  if (!L || !L->nl) { if (smkOn) carPatchUs += sceKernelGetSystemTimeLow() - us0; return; }
  CarLT *Lm = (CarLT *)L; unsigned qh0 = 0, qh1 = 0;
  if (cltFix940) { cltHash(m, &qh0, &qh1); if (Lm->quiet && Lm->h0 == qh0 && Lm->h1 == qh1) { cltQuietN++; if (smkOn) carPatchUs += sceKernelGetSystemTimeLow() - us0; return; } }   // №940: ничего не изменилось, выбросов не было
  int hitN = 0;
  const float lim = (d->r.flags & 2) ? 2048.f : GG_SAFE;
  int v12 = (d->r.flags & F_V12) != 0, vs = v12 ? 12 : 16, po = v12 ? 6 : 8; const float S = 1.f / 32768.f;
  const unsigned char *vb = (const unsigned char *)d->v;
  float T0 = m->w.x - ggE[0], T1 = m->w.y - ggE[1], T2 = m->w.z - ggE[2]; const float *ax[3] = {ggR, ggU, ggF}; float A[3][3], B[3];
  for (int r = 0; r < 3; r++) {
    A[r][0] = ax[r][0] * m->x.x + ax[r][1] * m->x.y + ax[r][2] * m->x.z; A[r][1] = ax[r][0] * m->y.x + ax[r][1] * m->y.y + ax[r][2] * m->y.z;
    A[r][2] = ax[r][0] * m->z.x + ax[r][1] * m->z.y + ax[r][2] * m->z.z; B[r] = ax[r][0] * T0 + ax[r][1] * T1 + ax[r][2] * T2;
  }
  CarDropK q = carDropK(); float zk = sc * q.iK;
  for (int gi = 0; gi < L->ng; gi++) {
    const float *bx = L->gb + 7 * gi; float zlim = bx[6] * zk;   // (2) по наибольшему ребру группы
    if (cltBoxSkip(bx, A, B, &q, zk)) continue;
    for (int li = gi * CLT_G; li < L->nl && li < (gi + 1) * CLT_G; li++) { int t = L->t[li];
    GgV g[3]; float zmn = 1e30f, lxm = 0.f, lym = 0.f;
    for (int i = 0; i < 3; i++) { const short *qv = (const short *)(vb + (t + i) * vs + po); float lx = qv[0] * S, ly = qv[1] * S, lz = qv[2] * S;
      g[i].l[0] = lx; g[i].l[1] = ly; g[i].l[2] = lz; g[i].c[2] = A[2][0] * lx + A[2][1] * ly + A[2][2] * lz + B[2]; if (g[i].c[2] < zmn) zmn = g[i].c[2]; }
    carPatchTri++;
    if (zmn >= zlim) continue;   // (2)
    for (int i = 0; i < 3; i++) { float lx = g[i].l[0], ly = g[i].l[1], lz = g[i].l[2];
      g[i].c[0] = A[0][0] * lx + A[0][1] * ly + A[0][2] * lz + B[0]; g[i].c[1] = A[1][0] * lx + A[1][1] * ly + A[1][2] * lz + B[1];
      if (fabsf(g[i].c[0]) > lxm) lxm = fabsf(g[i].c[0]); if (fabsf(g[i].c[1]) > lym) lym = fabsf(g[i].c[1]); }
    if ((lxm <= q.lx && lym <= q.ly) || !ggDropLim(&g[0], &g[1], &g[2], &ggK, lim)) continue;   // (3) и точная модель
    hitN++;
    for (int i = 0; i < 3; i++) {
      const unsigned char *p = vb + (t + i) * vs; const unsigned short *uv = (const unsigned short *)p; g[i].t[0] = uv[0]; g[i].t[1] = uv[1];
      if (v12) { unsigned c5 = *(const unsigned short *)(p + 4), r = c5 & 31, gg = (c5 >> 5) & 63, b = c5 >> 11;
        g[i].k[0] = (float)((r << 3) | (r >> 2)); g[i].k[1] = (float)((gg << 2) | (gg >> 4)); g[i].k[2] = (float)((b << 3) | (b >> 2)); g[i].k[3] = 255.f; }
      else { unsigned c8 = *(const unsigned *)(p + 4); g[i].k[0] = (float)(c8 & 255); g[i].k[1] = (float)((c8 >> 8) & 255); g[i].k[2] = (float)((c8 >> 16) & 255); g[i].k[3] = (float)(c8 >> 24); }
    }
    GgV o[GG_MAXV]; int n = ggCut(&g[0], &g[1], &g[2], &ggK, o); if (n < 3) continue;
    GgVert *w = (GgVert *)ggVbAlloc((unsigned)n * sizeof(GgVert)); if (!w) continue;
    for (int i = 0; i < n; i++) {
      float u = o[i].t[0] + .5f, v = o[i].t[1] + .5f; unsigned ch[4];
      for (int j = 0; j < 4; j++) { float x = o[i].k[j] + .5f; ch[j] = x < 0.f ? 0u : x > 255.f ? 255u : (unsigned)x; }
      w[i].u = (unsigned short)(u < 0.f ? 0 : u > 65535.f ? 65535 : (int)u); w[i].v = (unsigned short)(v < 0.f ? 0 : v > 65535.f ? 65535 : (int)v);
      w[i].c = ch[0] | ch[1] << 8 | ch[2] << 16 | ch[3] << 24; w[i].x = o[i].l[0]; w[i].y = o[i].l[1]; w[i].z = o[i].l[2];
    }
    ggVbWriteback(vb0_); vb0_ = ggVbUsed;
    sceGuDrawArray(GU_TRIANGLE_FAN, GG_VT, n, 0, w); carPatchFans++;
    }
  }
  if (cltFix940) { Lm->h0 = qh0; Lm->h1 = qh1; Lm->quiet = !hitN; }
  if (carPatchFans != f0_ && carFanN < 8 && !carFanHas(d->v)) carFanV[carFanN++] = d->v;   // №922: у этого куска в кадре были веера — обводка проверяется
  if (smkOn) carPatchUs += sceKernelGetSystemTimeLow() - us0;
}
static int carNear8531(const Piece *d, const ScePspFMatrix4 *m) {
  if (GG_CAR853_OLD || bigCutBlock || ggK.tvy <= 0.f || (d->r.owner == 0 && !dropFix921) || !(d->r.flags & F_NOIDX) || d->r.ni < 3 || d->r.ni > 65535) return 0;   // №921: owner 0 — проп/свет (сюда город не попадает)
  if (smkOn) { static float cpT_; if (smkT - cpT_ >= 1.f || smkT < cpT_) { char l_[220];   // служебно: заплатка машин за секунду
      snprintf(l_, sizeof l_, "CARPATCH t=%.1f sc=%s tests=%u box=%u tri=%u fans=%u near_us=%u patch_us=%u bigmiss=%u clt=%d/%u/%u bld=%u bld_us=%u fail=%u defer=%u badhit=%u compact=%u quiet=%u cbad=%u\n", smkT, smkScene, carNearN, carNearCut, carPatchTri, carPatchFans, carNearUs, carPatchUs, bigInfoMiss853, carLTn, cltUsed, (unsigned)CLT_POOL, cltBuilds, cltBuildUs, cltFailN, cltDeferN, cltBadHit, cltCompactN, cltQuietN, cltCompactBad); smkWrite(l_);
      carNearN = carNearCut = carPatchTri = carPatchFans = carNearUs = carPatchUs = bigInfoMiss853 = cltBuilds = cltBuildUs = cltFailN = cltDeferN = cltBadHit = cltCompactN = cltQuietN = 0; cpT_ = smkT; } }
  if (d->r.owner == 0) { float dx_ = m->w.x - ggE[0], dy_ = m->w.y - ggE[1], dz_ = m->w.z - ggE[2]; if (dx_ * dx_ + dy_ * dy_ + dz_ * dz_ > 3600.f) return 0; }   // №923: проп/свет дальше 60 м от глаза (куски до 23 м, крупнее — bigNear) выброса у глаза не даёт: без поиска в таблице кусков (+0,02–0,03 мс Мака на кадр от №921)
  carNearN++; int k = 0;
  CarDropK q = carDropK(); const BigInfo *b = bigInfo(d->v, d->r.ni, d->r.flags); const float *mm = (const float *)m;
  if (!trfGeoOld) { float s0q = 0.f; for (int a = 0; a < 3; a++) { const float *col = mm + 4 * a; float qq = col[0] * col[0] + col[1] * col[1] + col[2] * col[2]; if (qq > s0q) s0q = qq; }
    float lim_ = q.K * ggK.zn; if (b->edge * b->edge * s0q <= lim_ * lim_ * TRG_K) return 0; }   // №984: правило (1) e > K·zn не выполнено (сравнение по квадратам, запас 0,01 %) — k = 0, как и в прежнем расчёте
  float sc = matScale(mm), e = b->edge * sc;
  if (e > q.K * ggK.zn) {   // (1)
    float lc[3] = {(b->lo[0] + b->hi[0]) * .5f, (b->lo[1] + b->hi[1]) * .5f, (b->lo[2] + b->hi[2]) * .5f}, h0 = b->hi[0] - lc[0], h1 = b->hi[1] - lc[1], h2 = b->hi[2] - lc[2];
    float rr = sqrtf(h0 * h0 + h1 * h1 + h2 * h2) * sc, c[3];
    for (int r_ = 0; r_ < 3; r_++) c[r_] = mm[r_] * lc[0] + mm[4 + r_] * lc[1] + mm[8 + r_] * lc[2] + mm[12 + r_];
    float e0 = c[0] - ggE[0], e1 = c[1] - ggE[1], e2 = c[2] - ggE[2], z = ggF[0] * e0 + ggF[1] * e1 + ggF[2] * e2;
    if (z - rr < e * q.iK && z + rr > ggK.zn) {   // (2), и не весь за ближней плоскостью
      float x = ggR[0] * e0 + ggR[1] * e1 + ggR[2] * e2, y = ggU[0] * e0 + ggU[1] * e1 + ggU[2] * e2;
      k = fabsf(x) + rr > q.lx || fabsf(y) + rr > q.ly;   // (3)
    }
  }
  carNearCut += (unsigned)k; return k;
}
static int carNear853(const Piece *d, const ScePspFMatrix4 *m) {   // №984: в служебном режиме trfgeocheck — оба пути, расхождения в счётчик
  if (!trgCheck) return carNear8531(d, m);
  int sv = trfGeoOld; trfGeoOld = 1; int a = carNear8531(d, m); trfGeoOld = 0; int b = carNear8531(d, m); trfGeoOld = sv; trgChk++; if (a != b) trgMis++; return sv ? a : b;
}
static unsigned dbgPM, dbgPMgeo;
// №913 (оптимизация предметов, срез у глаза: на L5 в езде режется ОДИН кусок пропа (текстура 63, 1884 вершины, подвижный, 16–28 м от глаза), каждый кадр целиком по треугольникам —
// 0,75 мс Мака, 0,8–1,0 мс «x_props»): у городских кусков срез идёт по иерархии групп (gg_batch.h: ящик 16 и 16×N треугольников, безопасные группы пропускаются без расчёта), у кусков
// пропов и стейтпропов (временный Piece без ggBatch) — нет. Теперь для куска, который режется (bigNear), иерархия строится лениво один раз по его вершинам (память пропов не двигается;
// ключ — адрес вершин, число, флаги; сброс при загрузке уровня) и передаётся срезу. Группы пропускаются только когда GE заведомо не выбросит ни один их треугольник (ggBatchMayRisk, как у города),
// результат тот же. GG_PROPBATCH_OLD=1 / «flag ggpropbatch 0|1» / файл ggpropbatchold рядом с EBOOT — прежний полный обход
#ifndef GG_PROPBATCH_OLD
#define GG_PROPBATCH_OLD 0
#endif
static int ggPropBatchOn = !GG_PROPBATCH_OLD;
typedef struct { const void *v; int ni, flags; GgBatch *b; } PropBatch;
static PropBatch pbat[24]; static int pbatN; static unsigned pbatBuilt;
static void propBatchReset(void) { for (int i = 0; i < pbatN; i++) free(pbat[i].b); pbatN = 0; }
static GgBatch *propBatchGet(const Piece *d) {
  for (int i = 0; i < pbatN; i++) if (pbat[i].v == d->v && pbat[i].ni == d->r.ni && pbat[i].flags == d->r.flags) return pbat[i].b;
  if (pbatN >= 24 || d->r.ni < 3 || d->r.ni > 65535 || (d->r.flags & F_STRIP) || !(d->r.flags & F_NOIDX)) return 0;
  GgBatch *b = (GgBatch *)malloc(ggBatchStorage((unsigned)d->r.ni) * sizeof(GgBatch)); if (!b) return 0;
  int v12 = (d->r.flags & F_V12) != 0; ggBatchCacheBuild(b, d->v, (unsigned)d->r.ni, v12 ? 12 : 16, v12 ? 6 : 8);
  pbat[pbatN].v = d->v; pbat[pbatN].ni = d->r.ni; pbat[pbatN].flags = d->r.flags; pbat[pbatN].b = b; pbatN++; pbatBuilt++; return b;
}
static struct { int tex, ni, ph, fl; unsigned n, cuts; double us; float p[3], dist; } ggfTab[12]; static unsigned smkBigNearUs, smkBigNearN, smkBigCutUs, smkBigCutN;   // №913 служебно: какие куски режет процессор (GGFORCE раз в 2 с)
// №930в (дирижёр 25.10, трафик +0,3 мс PSP в туре): замеры CFCOST читали системное время (вызов ядра) 4 раза на каждый кусок машины NPC — это цена самих замеров в туре, в игре без смоука её нет. Теперь выключены:
// включает «flag cfcost 1» в сценарии; CFCOST_OLD=1 — как test62 (читать всегда при смоуке)
#ifndef CFCOST_OLD
#define CFCOST_OLD 0
#endif
static int cfCostFlag;
#define cfCostOn (smkOn && (CFCOST_OLD || cfCostFlag))
static unsigned cfUsD[2], cfND[2], cfUsP, cfUsH;   // №930б служебно: мкс/число отрисовок кусков машин NPC [целиком, заливкой]
static const ScePspFMatrix4 *dpmKeep; static int dpmPlain;   // №930: dpmPlain — последний drawPieceM кончил обычным drawGeo (матрица модели осталась m); dpmKeep — матрица, которую второй вызов не пишет заново
static void drawPieceM(Piece *d, const ScePspFMatrix4 *m) {   // кусок с готовой матрицей модели
#if !SHR_REL_SMK
  unsigned xq_ = xsubCar ? XS_T() : 0, xqw_ = xsubCar ? XS_W() : 0;
#define XQ(i) do { if (xsubCar) XS_LAP(i, xq_, xqw_); } while (0)
#else
#define XQ(i) ((void)0)
#endif
  RBC(rbcDrawM);
  if (d->r.ni <= 0 || geOver(GE_HARD) || !pieceOk(d, 0) || NEON_SKIP(d) || GLOWOFF1052_SKIP(d) || TREESH_SKIP(d)) return;   // правка 103
  if (d->r.flags & F_ADDITIVE) { dbgAddN++; dbgAddTri += (unsigned)d->r.ni / 3; if (d->r.ni > 600) dbgAddBig++; gloAdd((char)('A' + (bbPhase < 0 || bbPhase > 20 ? 0 : bbPhase)), d->r.tex, d->r.owner, 0, (unsigned)d->r.ni / 3);
    if (smkOn && d->r.ni > 600) { static const void *seen_[12]; static int ns_; int k_ = 0; for (; k_ < ns_; k_++) if (seen_[k_] == d->v) break; if (k_ == ns_ && ns_ < 12) { seen_[ns_++] = d->v; char l_[260];
        snprintf(l_, sizeof l_, "GLOWBIG t=%.1f sc=%s site=%d phase=%d tex=%d ni=%d fl=%x own=%d at=%.1f,%.1f,%.1f sc=%.2f eye_d=%.1f\n", smkT, smkScene, gloSite, (int)bbPhase, d->r.tex, d->r.ni, d->r.flags, d->r.owner, m->w.x, m->w.y, m->w.z, matScale((const float *)m), sqrtf((m->w.x - eyeX) * (m->w.x - eyeX) + (m->w.y - eyeY) * (m->w.y - eyeY) + (m->w.z - eyeZ) * (m->w.z - eyeZ))); smkWrite(l_); } } }
  XQ(54); if (tqPush(d, m, 0, d->r.ni, m->w.x, m->w.y, m->w.z)) { XQ(58); return; }
  XQ(59); drawnTris += d->r.ni / 3; draws++;
  NZP(d); bindTexLean(d->r.tex); XQ(60); setFlags(d->r.flags); XQ(61);
  dpmPlain = 0; if (dpmKeep == m) dpmKeep = 0; else sceGuSetMatrix(GU_MODEL, m);   // №930: второй вызов того же куска (carFillDraw) — матрица модели уже в GE
  int glowFog = glowFogBegin(d->r.flags); XQ(62);
  // матрица мировая (absM=1), как у очереди прозрачного
  dbgPM++;   // счётчики гейта big_cut: кусков, дошедших до рисования / реально отданных в drawGeo (16.10: комментарий проглотил else — ни один кусок не рисовался, а счётчики вызовов росли)
#ifdef GATE_BREAK_PM
  return;   // отрицательный контроль гейта «КАДР ИГРОКА»: воспроизводит баг 44c9a048 (кусок посчитан, не нарисован)
#endif
  unsigned bn0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; int bn_ = bigNear(d, m); if (smkOn) { unsigned bn1_ = sceKernelGetSystemTimeLow(); smkBigNearUs += bn1_ - bn0_; smkBigNearN++; if (trgIn) { trgBnUs += bn1_ - bn0_; trgN++; } } XQ(55);   // №913: цена bigNear на кусок
  if (bn_) { unsigned bc0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; Piece pb_; const Piece *dg_ = d;
    if (ggPropBatchOn && !d->ggBatch && d->r.owner == 0 && !ggBatchOld) { GgBatch *gb_ = propBatchGet(d); if (gb_) { pb_ = *d; pb_.ggBatch = gb_; dg_ = &pb_; } }   // №913
    unsigned gt0_ = ggTrisF; ggForce = 1; drawGeoG(dg_, 0, d->r.ni, m, 1); ggForce = 0; bigPiecesF++; dbgPMgeo++;
    if (smkOn) { unsigned dt_ = sceKernelGetSystemTimeLow() - bc0_; smkBigCutUs += dt_; smkBigCutN++;
      int hit_ = -1; for (int q_ = 0; q_ < 12; q_++) if (ggfTab[q_].n && ggfTab[q_].tex == d->r.tex && ggfTab[q_].ni == d->r.ni && ggfTab[q_].ph == bbPhase) { hit_ = q_; break; }
      if (hit_ < 0) for (int q_ = 0; q_ < 12; q_++) if (!ggfTab[q_].n) { hit_ = q_; ggfTab[q_].tex = d->r.tex; ggfTab[q_].ni = d->r.ni; ggfTab[q_].ph = bbPhase; ggfTab[q_].us = 0; memcpy(ggfTab[q_].p, &m->w, sizeof ggfTab[q_].p); ggfTab[q_].dist = sqrtf((m->w.x - ggE[0]) * (m->w.x - ggE[0]) + (m->w.y - ggE[1]) * (m->w.y - ggE[1]) + (m->w.z - ggE[2]) * (m->w.z - ggE[2])); break; }
      if (hit_ >= 0) { ggfTab[hit_].n++; ggfTab[hit_].us += dt_; ggfTab[hit_].fl = d->r.flags; ggfTab[hit_].cuts += ggTrisF - gt0_; } } }
  else { unsigned cn0_ = (smkOn && trgIn) ? sceKernelGetSystemTimeLow() : 0; int cn_ = carNear853(d, m); if (smkOn && trgIn) trgCnUs += sceKernelGetSystemTimeLow() - cn0_; XQ(56); if (cn_) { drawGeoPatch853(d, m); dbgPMgeo++; }   // №853: кусок машины у камеры — целиком и веера на месте выброшенных
#ifndef GATE_DROP_DRAW
    else { diagAbsM = 1; drawGeo(d, 0, d->r.ni); dbgPMgeo++; dpmPlain = 1; }
#endif
    XQ(57); }   // GATE_DROP_DRAW=1 — ТОЛЬКО отрицательный контроль гейтов (tests/missions/drawn_gate.sh): куски не рисуются, как при баге 16.10
  glowFogEnd(glowFog); XQ(63);
}
// Правка «плавность» (10.10, место «ящики» L1: binds 230 против 160 у drive): пропы и стейтпропы рисовались в порядке экземпляров,
// каждый кусок — со своей сменой текстуры. Непрозрачные куски (флаг 2 не стоит) копятся и рисуются раз в кадре по (флаги, текстура);
// на глубине порядок не важен. Кусок с флагом 2 (очередь tq или смешивание без записи глубины) сперва выпускает накопленное, потом
// рисуется как раньше — его порядок относительно непрозрачного не меняется. Полна пачка — сброс и дальше как есть.
// -DPROP_SORT_OLD=1 — прежнее (каждый кусок сразу)
#ifndef PROP_SORT_OLD
#define PROP_SORT_OLD 0
#endif
#define MAXPB 512
#include "propsort913.h"
typedef struct { int tex, flags, ni; Vert *v; unsigned short *idx; float m[16]; } PBItem;
static PBItem pbq[MAXPB]; static int npb;
static int pbCmp(const void *a, const void *b) {
  const PBItem *x = a, *y = b; if (x->flags != y->flags) return x->flags < y->flags ? -1 : 1;
  return x->tex < y->tex ? -1 : x->tex > y->tex ? 1 : 0;
}
// qsort по самим элементам (≈100 Б каждый, до 512) переставлял их целиком; сортируем указатели, при равном ключе — порядок экземпляров
// (устойчиво: равные по флагам и текстуре идут как раньше без сортировки). -DPROP_SORT_PTR_OLD=1 — прежняя сортировка элементов
#ifndef PROP_SORT_PTR_OLD
#define PROP_SORT_PTR_OLD 0
#endif
static const PBItem *pbp[MAXPB];
static int pbCmpP(const void *a, const void *b) {
  const PBItem *x = *(const PBItem *const *)a, *y = *(const PBItem *const *)b; if (x->flags != y->flags) return x->flags < y->flags ? -1 : 1;
  if (x->tex != y->tex) return x->tex < y->tex ? -1 : 1;
  return x < y ? -1 : x > y;
}
static void propFlush(void) {
  if (!npb) return;
  if (!PROP_SORT_PTR_OLD) {
    int done_ = 0;
    if (!PROPSORT913_OLD) { static int kf_[MAXPB], kt_[MAXPB]; static unsigned short ord_[MAXPB]; for (int i = 0; i < npb; i++) { kf_[i] = pbq[i].flags; kt_[i] = pbq[i].tex; }   // разбор L3/L5: ключ и поразрядная сортировка вместо qsort с функцией (propsort913.h)
      if (pbSortKeys(kf_, kt_, npb, ord_)) { for (int i = 0; i < npb; i++) pbp[i] = &pbq[ord_[i]]; done_ = 1; } }
    if (!done_) { for (int i = 0; i < npb; i++) pbp[i] = &pbq[i]; qsort(pbp, npb, sizeof *pbp, pbCmpP); } }
  else qsort(pbq, npb, sizeof *pbq, pbCmp);
  for (int i = 0; i < npb; i++) { const PBItem *q = PROP_SORT_PTR_OLD ? &pbq[i] : pbp[i]; Piece tmp; memset(&tmp, 0, sizeof tmp);
    tmp.r.tex = q->tex; tmp.r.flags = q->flags; tmp.r.ni = q->ni; tmp.v = q->v; tmp.idx = q->idx;
    drawPieceM(&tmp, (const ScePspFMatrix4 *)q->m); }
  npb = 0;
}
// №923 служебно (разбор x_props железа L1/L3): SPTOP раз в секунду — типы стейтпропов, давшие больше всего нарисованных кусков (экземпляров в кадре, кусков на кадр, у типа: кусков, суставов, состояний, вид, билбордов, маска PVIS)
static unsigned pcTested, pcCulled, pcViol;
static struct { unsigned type, inst, drawn, nP, nJ, nS, kind, nB, pv; } spTop[24]; static int spTopN; static unsigned spTopFr; static float spTopT;
static void spTopAdd(unsigned type, unsigned drawn, unsigned nP, unsigned nJ, unsigned nS, unsigned kind, unsigned nB, unsigned pv) {
  int k = 0; for (; k < spTopN; k++) if (spTop[k].type == type) break;
  if (k == spTopN) { if (spTopN >= 24) return; spTop[k].type = type; spTop[k].inst = spTop[k].drawn = 0; spTop[k].nP = nP; spTop[k].nJ = nJ; spTop[k].nS = nS; spTop[k].kind = kind; spTop[k].nB = nB; spTop[k].pv = pv; spTopN++; }
  spTop[k].inst++; spTop[k].drawn += drawn;
}
static void spTopTick(void) {
  spTopFr++; if (smkT - spTopT < 1.f && smkT >= spTopT) return; spTopT = smkT;
  for (int i = 0; i < spTopN; i++) for (int j = i + 1; j < spTopN; j++) if (spTop[j].drawn > spTop[i].drawn) { __typeof__(spTop[0]) x = spTop[i]; spTop[i] = spTop[j]; spTop[j] = x; }
  char l[600]; int o = snprintf(l, sizeof l, "SPTOP t=%.1f sc=%s fr=%u", smkT, smkScene, spTopFr);
  for (int i = 0; i < spTopN && i < 6 && o < 520; i++) o += snprintf(l + o, sizeof l - (size_t)o, " [t%u inst=%.1f pcs=%.1f of %u j%u s%u k%u b%u pv%u]", spTop[i].type, spTop[i].inst / (float)spTopFr, spTop[i].drawn / (float)spTopFr, spTop[i].nP, spTop[i].nJ, spTop[i].nS, spTop[i].kind, spTop[i].nB, spTop[i].pv);
  o += snprintf(l + o, sizeof l - (size_t)o, " pcull=%.1f/%.1f viol=%u", pcTested / (float)spTopFr, pcCulled / (float)spTopFr, pcViol); pcTested = pcCulled = pcViol = 0;
  snprintf(l + o, sizeof l - (size_t)o, "\n"); smkWrite(l); spTopN = 0; spTopFr = 0;
}
static void propPiece(const Piece *tmp, const float *m) {   // кусок пропа/стейтпропа: в пачку или сразу
  if (PROP_SORT_OLD) { drawPieceM((Piece *)tmp, (const ScePspFMatrix4 *)m); return; }
  if (tmp->r.flags & 2) { propFlush(); drawPieceM((Piece *)tmp, (const ScePspFMatrix4 *)m); return; }
  if (npb >= MAXPB) propFlush();
  PBItem *q = &pbq[npb++]; q->tex = tmp->r.tex; q->flags = tmp->r.flags; q->ni = tmp->r.ni; q->v = tmp->v; q->idx = tmp->idx; memcpy(q->m, m, sizeof q->m);
}
// Правка Роба 413б («летающая машина в оригинале полупрозрачная, у нас становится прозрачной»): у frink_v (летающая машина Фринка)
// все 12 шейдеров диска с флагом translucent в заголовке шейдера (0x11000), но смешивание (BLMD) только у купола special_m
// (Windsheild.tga, альфа 191 = 0,75) и катушки frink_vcoil_m; у остальных 10 BLMD 0, ATST 0, альфа текстуры 255 (единственная
// такая машина диска: tests/hover/glass_census.py). Экспорт (tools/psp_export.py car_prims) ставит флаг 2 по translucent ИЛИ
// BLMD — весь кузов шёл в очередь полупрозрачного без записи глубины, порядок по центрам кусков: двигатель, салон и дальний борт
// рисуются поверх ближнего — машина «прозрачная» насквозь. Правило: кусок машины (своя CAR1, трафик TRF1, машины миссий MCR1;
// вершины машин всегда с альфой 255 — car_prims) с флагом 2, у которого вся палитра текстуры непрозрачна (альфа 255), рисуется
// непрозрачным, с записью глубины. Полупрозрачным остаётся только то, что прозрачно в данных (купол, призрачный ship L7).
// CARGLASS_OLD=1 / «flag carglassold 1» — прежнее (красный контроль tests/hover/frink_alpha.sh)
#ifndef CARGLASS_OLD
#define CARGLASS_OLD 0
#endif
static int carGlassOld = CARGLASS_OLD, cgOwn; static unsigned cgBlendTris, cgOpaqTris, cgFrames;   // служебное: треугольники СВОЕЙ машины (cgOwn) с прошлой строки CARGLASS
static int texOpaque(int t) {   // 1 — все цвета, на которые ссылаются точки верхнего уровня текстуры t, с альфой 255
  Tex *x = texAt(t); if (!x || !x->pal || !x->lv[0] || x->w <= 0 || x->h <= 0) return 0;
  if (x->opqPal != x->pal) {   // один раз на текстуру: палитры дополнены до 256 цветов нулями (альфа 0) — смотреть только занятые
    const unsigned *p = (const unsigned *)x->pal; const unsigned char *q = (const unsigned char *)x->lv[0];
    int bw = x->t4 ? (x->w + 1) / 2 : x->w, n = (x->pre && bw < 16 ? 16 : bw) * (x->pre ? (x->h + 7) & ~7 : x->h), o = 1;
    for (int i = 0; i < n && o; i++) o = x->t4 ? (p[q[i] & 15] >> 24) == 0xffu && (p[q[i] >> 4] >> 24) == 0xffu : (p[q[i]] >> 24) == 0xffu;
    x->opqPal = x->pal; x->opq = o;
  }
  return x->opq;
}
static void carGlass(Piece *d) {   // кусок машины перед drawPieceM
  if ((d->r.flags & 2) && !carGlassOld && texOpaque(d->r.tex)) d->r.flags &= ~2;
  cgOwnTq = cgOwn;
  if (cgOwn) { if (d->r.flags & 2) cgBlendTris += d->r.ni / 3; else cgOpaqTris += d->r.ni / 3; }
}
// №538: qsort нестабилен — у кусков с равным ключом (несколько конусов одного луча с одной точкой привязки) порядок мог зависеть от порядка вставки, а он меняется с набором экземпляров
// (запекание B1, сетка стейтпропов). Равный ключ — по порядку вставки (адрес в tq[]): порядок смешивания одинаков из кадра в кадр. TQ_STABLE_OLD=1 (бит 13 bisect) — прежнее
#ifndef TQ_STABLE_OLD
#define TQ_STABLE_OLD BIS(13)
#endif
static int tqCmp(const void *a, const void *b) {   // дальние раньше
  const TQ *qa = *(TQ *const *)a, *qb = *(TQ *const *)b; float ka = qa->key, kb = qb->key;
  if (ka != kb) return ka < kb ? 1 : -1;
  if (TQ_STABLE_OLD) return 0;
  return qa < qb ? -1 : qa > qb ? 1 : 0;
}
// Правка 313 (Роб 04.10: «делегируй Media Engine прозрачные элементы, свечение, музыку»): служебное «flag meprof 1» — замер на главном
// процессоре того, что можно отдать ME: сортировка отложенного полупрозрачного, частицы (обновление и сборка биллбордов), звук.
// Строка MEPROF раз в секунду в smoke.txt (на PSP — тур; в эмуляторе — время по счёту команд). Без флага — одна проверка за кадр
static int meProf; static unsigned mpFrames, mpSort, mpTqn, mpTqd, mpUpd, mpUpdMax, mpDraw, mpDrawMax, mpFx, mpFxMax;
// Правка «быстрый расчёт» (tqFlush 1,0–1,6 мс на взрывах; на 53 куска 44 смены текстуры по ≈ 22 слова списка GE, слово ≈ 1 мкс): очередь идёт строго
// от дальнего к ближнему, текстуры чередуются. Берём вперёд ближайший по порядку кусок с той же текстурой и флагами, ЕСЛИ он не пересекается на
// экране с пропускаемыми (шары кусков видны из глаза под углами, чьи круги не касаются): тогда порядок смешивания между ними ни на что не влияет.
// Куски без границ (rad 0) порядок держат (не переставляются и через них не прыгают). Окно просмотра 24. -DTQ_FAST_OLD=1 — прежний порядок
#ifndef TQ_FAST_OLD
#define TQ_FAST_OLD BIS(12)   // бит 12 файла bisect (bisect.h)
#endif
// По TEST14 (11.10) перегруппировка даёт мигание 1,98% против 0,45% без неё, кадр не легче: по умолчанию ВЫКЛЮЧЕНА. -DTQ_FAST_ON=1 возвращает
#ifndef TQ_FAST_ON
#define TQ_FAST_ON 0   // TEST17 (слово Роба/дирижёра 15.10: отрисовка как в 1721 f32c8c31): перегруппировка очереди полупрозрачного включена, как в 1721; 0 — выключена (d43e202c)
#endif
static int tqFastRt = TQ_FAST_ON;   // TEST19 (A/B на железе): «flag tqfast 0/1» — перегруппировка очереди полупрозрачного во время прогона
static void tqRegroup(void) {
  int n = ntq; if (!tqFastRt || TQ_FAST_OLD || n < 3) return;
  static float ux[MAXTQ], uy[MAXTQ], uz[MAXTQ], sa[MAXTQ], ca[MAXTQ]; static unsigned char valid[MAXTQ], used[MAXTQ]; static TQ *out[MAXTQ];
  for (int i = 0; i < n; i++) { const TQ *q = tqp[i]; used[i] = 0;
    float dx = q->cx - tqEx, dy = q->cy - tqEy, dz = q->cz - tqEz, d = sqrtf(dx * dx + dy * dy + dz * dz);
    valid[i] = q->rad > 0.f && d > q->rad * 1.05f;   // глаз внутри шара — считаем «пересекает всё»
    if (valid[i]) { float inv = 1.f / d; ux[i] = dx * inv; uy[i] = dy * inv; uz[i] = dz * inv; sa[i] = q->rad * inv; ca[i] = sqrtf(fmaxf(0.f, 1.f - sa[i] * sa[i])); } }
  int lastTex = -9999, lastFl = -1, first = 0;
  for (int o = 0; o < n; o++) {
    while (used[first]) first++;
    int pick = first;
    if (lastTex != -9999 && !(tqp[first]->p.r.tex == lastTex && tqp[first]->p.r.flags == lastFl) && valid[first]) {
      for (int j = first + 1; j < n && j < first + 24; j++) {
        if (used[j] || !valid[j] || tqp[j]->p.r.tex != lastTex || tqp[j]->p.r.flags != lastFl) continue;
        int ok = 1;
        for (int k = first; k < j && ok; k++) { if (used[k]) continue;
          if (!valid[k]) { ok = 0; break; }
          float cosA = ux[j] * ux[k] + uy[j] * uy[k] + uz[j] * uz[k], cosSum = ca[j] * ca[k] - sa[j] * sa[k];   // cos(α+β)
          if (cosA > cosSum - 0.002f) ok = 0; }   // круги касаются/пересекаются (запас) — порядок держим
        if (ok) { pick = j; break; } } }
    used[pick] = 1; out[o] = tqp[pick]; lastTex = out[o]->p.r.tex; lastFl = out[o]->p.r.flags; }
  for (int i = 0; i < n; i++) tqp[i] = out[i];
}
#ifndef TQTOP_ON
#define TQTOP_ON 1
#endif
static int tqSkipTex = -1, tqDumpTex = -1, tqDumpV = 2;   // tqdump N — построчная выгрузка кусков текстуры N в TQTOP-кадре   // TEST20: «flag tqskip N» — не рисовать полупрозрачные куски текстуры N (разбор, что в очереди); -1 — всё
static int tqPart; static float tqPartD2;   // №932б: 1 — рисуется только дальнее (key > tqPartD2), оно помечается key = -1 и не рисуется второй раз; очередь не сбрасывается
static void tqFlush(void) {
  int part_ = tqPart; int tqOn_ = tqOn;
  tqOn = 0;
  for (int i = 0; i < ntq; i++) tqp[i] = &tq[i];
  unsigned tq0_ = meProf ? sceKernelGetSystemTimeLow() : 0;
  qsort(tqp, ntq, sizeof *tqp, tqCmp);
  if (meProf) { mpSort += sceKernelGetSystemTimeLow() - tq0_; mpTqn += (unsigned)ntq; }
  { unsigned sig = 2166136261u, n = 0;   // 413б: порядок кусков своей машины после сортировки (смена подписи — куски поменялись местами)
    for (int i = 0; i < ntq; i++) if (tqp[i]->own) { sig = (sig ^ (unsigned)(tqp[i]->p.r.tex * 131 + tqp[i]->cnt)) * 16777619u; n++; }
    if (n && sig != tqOwnSig) tqOwnFlips++; tqOwnSig = sig; tqOwnItems += n; }
  tqRegroup();
  for (int i = 0; i < ntq; i++) {
    if (geOver(GE_HARD) || !pieceOk(&tqp[i]->p, tqp[i]->far) || tqp[i]->p.r.tex == tqSkipTex) continue;   // правка 103
    TQ *q = tqp[i]; if (q->key < 0.f || (part_ && q->key <= tqPartD2)) continue; if (part_) q->key = -1.f;
    drawnTris += q->cnt / 3; draws++;
    phAllow = q->p.r.owner == 0; NZP(&q->p); bindTexLean(q->p.r.tex); phAllow = 0; setFlags(q->p.r.flags);
    sceGuSetMatrix(GU_MODEL, &q->m);
    int glowFog = glowFogBegin(q->p.r.flags);
    if (dropFix921 && !q->far && q->p.r.owner == 0 && q->cnt == q->p.r.ni && !ggForce && carNear853(&q->p, &q->m)) { diagPatch = 1; drawGeoPatch853(&q->p, &q->m); diagPatch = 0; }   // №921: конус света/проп у глаза — заплатка, как у машин
    else drawGeoG(&q->p, q->far, q->cnt, &q->m, 1);   // правка 340: тени-наклейки у камеры — срез по полосе (матрица очереди мировая)
    glowFogEnd(glowFog);
  }
  if (part_) { tqOn = tqOn_; return; }
  if (smkOn && TQTOP_ON && ntq > 0) {   // TEST20 (разбор L3: что в fx на причале): раз в ~2 с строка TQTOP — топ текстур очереди полупрозрачного по треугольникам (индекс, размер, кусков, ктрис, ближний/дальний, м)
    static unsigned tqTopN; if ((tqTopN++ & 63) == 0) {
      struct { int tex, n; unsigned tris, ftris, fnear; float dmin, dmax; } a[8]; int na = 0;
      for (int i = 0; i < ntq; i++) { const TQ *q = &tq[i]; int t = q->p.r.tex; float d = sqrtf(q->key); int j = 0;
        for (; j < na; j++) if (a[j].tex == t) break;
        if (j == na) { if (na >= 8) continue; a[na].tex = t; a[na].n = 0; a[na].tris = 0; a[na].ftris = 0; a[na].fnear = 0; a[na].dmin = 1e9f; a[na].dmax = 0; na++; }
        a[j].n++; a[j].tris += (unsigned)q->cnt / 3; a[j].ftris += q->p.r.nf / 3; a[j].fnear += (q->p.r.flags & F_NEAR) != 0; if (d < a[j].dmin) a[j].dmin = d; if (d > a[j].dmax) a[j].dmax = d; }
      for (int i = 0; i < na; i++) for (int j = i + 1; j < na; j++) if (a[j].tris > a[i].tris) { __typeof__(a[0]) x = a[i]; a[i] = a[j]; a[j] = x; }
      char l[700]; int o = snprintf(l, sizeof l, "TQTOP t=%.1f scene=%s q=%d", smkT, smkScene, ntq);
      for (int i = 0; i < na && i < 5; i++) { int t = a[i].tex; o += snprintf(l + o, sizeof l - o, " [tex%d %dx%d n=%d kt=%.2f far_kt=%.2f nearflag=%u d=%.0f..%.0f]", t, t >= 0 && t < (int)ntex ? tex[t].w : 0, t >= 0 && t < (int)ntex ? tex[t].h : 0, a[i].n, a[i].tris / 1000.f, a[i].ftris / 1000.f, a[i].fnear, a[i].dmin, a[i].dmax);  if (o >= (int)sizeof l - 2) { o = (int)sizeof l - 2; break; } }   // аудит 16.10: строка не обрезается и не склеивается со следующей
      snprintf(l + o, sizeof l - o, "\n"); smkWrite(l);
      if (tqDumpTex >= 0) for (int i = 0; i < ntq; i++) { const TQ *q = &tq[i]; if (q->p.r.tex != tqDumpTex) continue; char m[200];
        if (tqDumpV > 0 && q->cnt >= 30) { tqDumpV--; for (int k = 0; k < q->cnt; k += (q->cnt / 30) / 3 * 3 ? (q->cnt / 30) / 3 * 3 : 3) { const Vert *w = &q->p.v[k]; snprintf(m, sizeof m, "TQV cell=%d k=%d u=%d v=%d c=%08x xyz=%d,%d,%d sc=%.4f o=%.1f,%.1f,%.1f\n", (int)q->p.r.cell, k, w->u, w->v, w->c, w->x, w->y, w->z, q->p.r.sc, q->p.r.ox, q->p.r.oy, q->p.r.oz); smkWrite(m); } }
        snprintf(m, sizeof m, "TQP tex=%d ni=%d cell=%d c=%.0f,%.0f,%.0f rad=%.0f d=%.0f bb=%.0f,%.0f,%.0f..%.0f,%.0f,%.0f fl=%x\n", q->p.r.tex, q->cnt, (int)q->p.r.cell, q->cx, q->cy, q->cz, q->rad, sqrtf(q->key), q->p.r.bb[0], q->p.r.bb[1], q->p.r.bb[2], q->p.r.bb[3], q->p.r.bb[4], q->p.r.bb[5], q->p.r.flags); smkWrite(m); } } }
  ntq = 0;
}
static void drawPieceLegacy(Piece *d, int far) {
  if (d->r.flags & F_NEAR) far = 0;   // правки 224/237/251: флаг F_NEAR — ближняя на любой дальности
  unsigned short *ix = far ? d->fidx : d->idx; int cnt = far ? (int)d->r.nf : d->r.ni;
  if (cnt <= 0) return; drawnTris += cnt / 3; draws++;
  bindTexLegacy(d->r.tex); sceGuTexScale(128.f, 128.f);
  if (d->r.flags & 1) sceGuEnable(GU_ALPHA_TEST); else sceGuDisable(GU_ALPHA_TEST);
  if (d->r.flags & 2) { sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); }
  else { sceGuDisable(GU_BLEND); sceGuDepthMask(GU_FALSE); }
  curFlags = -1;
  sceGumPushMatrix();
  ScePspFVector3 o = {d->r.ox, d->r.oy, d->r.oz}, sv = {d->r.sc, d->r.sc, d->r.sc};
  sceGumTranslate(&o); sceGumScale(&sv);
  if (d->r.flags & F_NOIDX) { sceGumUpdateMatrix(); diagMok = 0; drawGeo(d, far, cnt); }   // без индексов (шаг 3); путь GUM: матрицу diag не видит
  else for (int k = 0; k < cnt; k += 65532) {
    int n = cnt - k; if (n > 65532) n = 65532;
    sceGumDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D,
                    n, ix + k, d->v);
  }
  sceGumPopMatrix();
}

static int visBox(const float *b, float ox, float oy, float oz);
// небо: depthTest=0 — как раньше (первым, без теста глубины); 1 — последним: глубина неба сжата в 0..2 из 65535
// (sceGuDepthRange(2, 0)), поэтому оно проходит тест GEQUAL только там, где кадр пуст (очищен в 0), а город
// дальше ~124.7 м (глубина <= 2) — всё равно 100% туман. Запись глубины выключена флагом 4 (setFlags).
static void drawSky(float ex, float ey, float ez, int depthTest) {
  skyDomeTone();   // №785: купол неба в тоне мультика (сверка; пересчёт — только при смене настройки или перечитанной клетке)
  sceGuDisable(GU_FOG);
  setBias(0);   // сдвиг слоёв (флаг 64) иначе вошёл бы в DepthRange(2, 0) неба, и небо легло бы поверх дальнего города
  if (depthTest) sceGuDepthRange(2, 0); else sceGuDisable(GU_DEPTH_TEST);
  int skyClamp = 0; int skN_ = 0, skNotRes_ = 0, skCull_ = 0;
  for (int i = 0; i < nskyP; i++) {
    Piece *d = &pc[skyP[i]]; if (cl[d->r.cell].state != 2) { skNotRes_++; continue; }
    if ((opt & O_SKYCULL) && !visBox(d->r.bb, ex, ey, ez)) { skCull_++; continue; }
    int sv = !SKY_SEAM_OLD && (d->r.flags & F_SKYV) != 0;
    if (sv != skyClamp) { sceGuTexWrap(GU_REPEAT, sv ? GU_CLAMP : GU_REPEAT); skyClamp = sv; }
    drawPiece(d, 0, ex, ey, ez, 1, 0); skN_++;
  }
  if (sky954NStar > 0 && !sky954Old && !sky954StarsOld && !geOver(GE_HARD)) {   // №954: звёзды-точки купола (развёртка точек текстуры, main_camera.h sky954Dots): один вызов GE, 1 пиксель
    typedef struct { unsigned c; float x, y, z; } StV; StV *q = (StV *)sceGuGetMemory((unsigned)sky954NStar * sizeof(StV));
    for (int k = 0; k < sky954NStar; k++) q[k] = (StV){sky954Star[k].c, sky954Star[k].x, sky954Star[k].y, sky954Star[k].z};
    ScePspFMatrix4 m_ = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {ex, ey, ez, 1}};
    phAllow = 0; bindTexLean(-1); setFlags(4); sceGuSetMatrix(GU_MODEL, &m_);
    sceGuDrawArray(GU_POINTS, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, sky954NStar, 0, q);
  }
  { static unsigned s954T_; unsigned now_ = sceKernelGetSystemTimeLow(); if (smkOn && now_ - s954T_ > 1900000u) { s954T_ = now_; char l_[160]; snprintf(l_, sizeof l_, "SKY954 old=%d seen=%u conv=%u stars=%d starsold=%d depthtest=%d\n", sky954Old, sky954Seen, sky954Conv, sky954NStar, sky954StarsOld, depthTest); smkWrite(l_); } }
  if (skyDebug_ && depthTest) {   // №701 диагностика: где буфер глубины пуст (0) после неба — пурпур (там небо не нарисовано, хотя глубина свободна); серое при непустой глубине — это геометрия
    typedef struct { uint32_t c; int16_t x, y, z, pad; } DbgV; DbgV *q = (DbgV *)sceGuGetMemory(4 * sizeof(DbgV));
    q[0] = (DbgV){0xFFFF00FFu, 0, 0, 0, 0}; q[1] = (DbgV){0xFFFF00FFu, 480, 0, 0, 0}; q[2] = (DbgV){0xFFFF00FFu, 0, 272, 0, 0}; q[3] = (DbgV){0xFFFF00FFu, 480, 272, 0, 0};
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDepthMask(1);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, 0, q);
    sceGuDepthMask(0); sceGuEnable(GU_TEXTURE_2D);
  }
  { static unsigned skT_; unsigned now_ = sceKernelGetSystemTimeLow(); if (smkOn && now_ - skT_ > 900000u) { skT_ = now_; char l_[320]; snprintf(l_, sizeof l_, "SKYDRAW pieces=%d drawn=%d notres=%d culled=%d depthtest=%d eye=%.2f,%.2f,%.2f fwd=%.4f,%.4f,%.4f up=%.4f,%.4f,%.4f right=%.4f,%.4f,%.4f fov=%.2f\n", nskyP, skN_, skNotRes_, skCull_, depthTest, ggE[0], ggE[1], ggE[2], ggF[0], ggF[1], ggF[2], ggU[0], ggU[1], ggU[2], ggR[0], ggR[1], ggR[2], (float)FOVY); smkWrite(l_); } }
  if (skyClamp) sceGuTexWrap(GU_REPEAT, GU_REPEAT);
  if (depthTest) sceGuDepthRange(65535, 0); else sceGuEnable(GU_DEPTH_TEST);
}

// ---------- отсечение ----------
#include "clipguard.h"
#include "cullhier.h"
static float pl[4][3], eyeX, eyeY, eyeZ, cfX, cfY, cfZ, drawDist;
// №923: отсечение КУСКОВ крупных пропов и стейтпропов по боковым плоскостям кадра. Экземпляр с десятками кусков (дерево: 63 куска на 81 суставе, у дороги каждое качается и рисуется не запечённым) целиком
// проходит проверку сферы экземпляра, хотя половина его кусков вне кадра; каждый такой кусок стоит ≈16 слов списка GE и сотни тактов процессора (≈25 мкс на PSP). Бокс куска (по вершинам, один раз на
// адрес вершин) через матрицу куска -> сфера с запасом (полудиагональ × наибольший масштаб матрицы), та же проверка rfSphereOut, что у экземпляров. Только экземпляры с ≥ PCULL_MIN кусков.
// PIECECULL923_OLD=1 / «flag piececull923 0» / файл piececull923old — как было; «flag piececull923 3» — сверка по всем вершинам (viol в SPTOP), 4 — красный контроль: радиус ×0,3 со сверкой (viol > 0)
#ifndef PIECECULL923_OLD
#define PIECECULL923_OLD 0
#endif
#define PCULL_MIN 6
static int pieceCull923 = !PIECECULL923_OLD;
typedef struct { const void *v; float lo[3], hi[3]; } PcBox;
static PcBox pcbTab[512];
static int pcOut923(const void *v, unsigned nv, int flags, const float *m, const float e3[3]) {
  PcBox *b = &pcbTab[((uintptr_t)v >> 4) & 511u];
  if (b->v != v) {
    int v12 = (flags & F_V12) != 0, vs = v12 ? 12 : 16, po = v12 ? 6 : 8; float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    if (nv == 0 || nv > 65535) return 0;
    for (unsigned i = 0; i < nv; i++) { const short *q = (const short *)((const char *)v + i * vs + po); for (int a = 0; a < 3; a++) { float x = q[a] * (1.f / 32768.f); if (x < lo[a]) lo[a] = x; if (x > hi[a]) hi[a] = x; } }
    b->v = v; for (int a = 0; a < 3; a++) { b->lo[a] = lo[a]; b->hi[a] = hi[a]; }
  }
  float c0 = (b->lo[0] + b->hi[0]) * .5f, c1 = (b->lo[1] + b->hi[1]) * .5f, c2 = (b->lo[2] + b->hi[2]) * .5f, h0 = (b->hi[0] - b->lo[0]) * .5f, h1 = (b->hi[1] - b->lo[1]) * .5f, h2 = (b->hi[2] - b->lo[2]) * .5f;
  float wc[3]; for (int r = 0; r < 3; r++) wc[r] = m[r] * c0 + m[4 + r] * c1 + m[8 + r] * c2 + m[12 + r];
  float rw = sqrtf(h0 * h0 + h1 * h1 + h2 * h2) * matScale(m) * (pieceCull923 >= 4 ? .3f : 1.f);
  pcTested++; int out = rfSphereOut((const float (*)[3])pl, e3, wc, rw); pcCulled += (unsigned)out;
  if (out && pieceCull923 >= 3) {   // служебно (гейт): сверка по всем вершинам — отсечённый кусок не должен иметь вершины внутри всех четырёх плоскостей кадра
    int v12 = (flags & F_V12) != 0, vs = v12 ? 12 : 16, po = v12 ? 6 : 8; int bad = 0;
    for (unsigned i = 0; i < nv && !bad; i++) { const short *q = (const short *)((const char *)v + i * vs + po); float x = q[0] * (1.f / 32768.f), y = q[1] * (1.f / 32768.f), z = q[2] * (1.f / 32768.f), w[3]; int inside = 1;
      for (int r = 0; r < 3; r++) w[r] = m[r] * x + m[4 + r] * y + m[8 + r] * z + m[12 + r] - e3[r];
      for (int k = 0; k < 4; k++) if (w[0] * pl[k][0] + w[1] * pl[k][1] + w[2] * pl[k][2] < 0) { inside = 0; break; }
      if (inside) bad = 1; }
    pcViol += (unsigned)bad; }
  return out;
}
static float cullMg;   // B8: запас ящиков (м) только внутри cullCity: куски чуть за кромкой кадра попадают в список, чтобы он годился для почти той же позы (cullReuse)
static float curFovY = FOVY;   // вертикальный угол кадра: FOVY, в комнате — FOV её камеры из данных (11.2)
static void trSpawnLog(void) {   // гейт tests/traffic/spawnview_emu.sh: где родилась машина относительно кадра, который её рисует
  float tv = tanf(curFovY * (GU_PI / 360.f)), th = tv * ASPECT, hl = sqrtf(cfX * cfX + cfZ * cfZ); if (hl < 1e-4f) hl = 1e-4f;
  float rx = cfZ / hl, rz = -cfX / hl, ux = -rz * cfY, uy = rz * cfX - rx * cfZ, uz = rx * cfY;   // право и верх кадра
  for (int i = 0; i < TR_MAXCAR; i++) if (trSpawnPend & (1u << i)) {
    const Car *k = &traffic.car[i].car; float R = k->p.halfL + fabsf(k->p.bodyOfs) + .5f;
    float vx = k->x - eyeX, vy = k->y + 1.f - eyeY, vz = k->z - eyeZ, d = sqrtf(vx * vx + vy * vy + vz * vz);
    float z = vx * cfX + vy * cfY + vz * cfZ, x = vx * rx + vz * rz, y = vx * ux + vy * uy + vz * uz;
    int inf = d - R <= traffic.drawDist && z > -R && fabsf(x) - z * th <= R * sqrtf(1.f + th * th) && fabsf(y) - z * tv <= R * sqrtf(1.f + tv * tv);
    float px = onFoot() ? homer.x : car.x, pz = onFoot() ? homer.z : car.z, dp = hypotf(k->x - px, k->z - pz);
    char s[192]; snprintf(s, sizeof s, "TRFSPAWN t=%.2f car=%d d=%.1f ang=%.0f dp=%.1f inframe=%d old=%d at=%.1f,%.1f\n", smkT, i, d,
                          acosf(fmaxf(-1.f, fminf(1.f, z / fmaxf(d, 1e-3f)))) * 57.2958f, dp, inf, trfSpawnOld, k->x, k->z);
    smkWrite(s);
  }
  trSpawnPend = 0;
}
static void setupPlanes(float fx, float fy, float fz) {
  float rx = -fz, rz = fx, rl = sqrtf(rx * rx + rz * rz); if (rl < 1e-4f) { rx = 1; rz = 0; rl = 1; }
  rx /= rl; rz /= rl;                                   // вправо = взгляд x вверх
  float ux = -rz * fy, uy = rz * fx - rx * fz, uz = rx * fy;   // вверх камеры = вправо x взгляд
  float tv = tanf(curFovY * .5f * GU_PI / 180.f) * 1.03f, th = tv * ASPECT;
  float n[4][3] = {
    {fx * th - rx, fy * th, fz * th - rz}, {fx * th + rx, fy * th, fz * th + rz},
    {fx * tv - ux, fy * tv - uy, fz * tv - uz}, {fx * tv + ux, fy * tv + uy, fz * tv + uz}};
  memcpy(pl, n, sizeof pl);
}
// ящик целиком снаружи одной из 4 боковых плоскостей? (ox,oy,oz — сдвиг ящика, для неба = глаз)
static int visBox(const float *b, float ox, float oy, float oz) {
  RBC(rbcVis);
  for (int i = 0; i < 4; i++) {
    float px = (pl[i][0] > 0 ? b[3] + cullMg : b[0] - cullMg) + ox - eyeX;
    float py = (pl[i][1] > 0 ? b[4] + cullMg : b[1] - cullMg) + oy - eyeY;
    float pz = (pl[i][2] > 0 ? b[5] + cullMg : b[2] - cullMg) + oz - eyeZ;
    if (px * pl[i][0] + py * pl[i][1] + pz * pl[i][2] < 0) return 0;
  }
  return 1;
}
static int respawnSeen(void *user, float x, float y, float z, float r) {
  (void)user;
  float dx = x - eyeX, dz = z - eyeZ, distance = drawDist * .92f + r;
  if (dx * dx + dz * dz > distance * distance) return 0;
  float bb[6] = {x-r, y-r, z-r, x+r, y+r, z+r};
  return visBox(bb, 0, 0, 0);
}

// этап 5: отрисовка трафика — кусок TRF1 (формат Rec level.shr) через drawPieceM, текстуры с базы trTexBase
_Static_assert(sizeof(TrPiece) == sizeof(Rec), "TrPiece = Rec level.shr");
static int trVis(const float bb[6]) {   // + CLIP_GUARD (clipguard.h); у машин — по ориентированному ящику кузова (TR_CARBOX_OLD=1 — по грубому)
  int hid = (trVisCar && !TR_CARBOX_OLD) ? clipGuardCar(eyeX, eyeY, eyeZ, trVisCar, bb) : clipGuardBox(eyeX, eyeY, eyeZ, bb);
  return !hid && visBox(bb, 0, 0, 0); }   // №502: машина/пешеход у самого глаза не рисуется
// №465: блик стоп-сигнала / заднего хода машины трафика или миссии (кусок owner 3, trafficDrawModel зовёт его, только когда горит): место — перенос матрицы модели,
// вверх (режим LYAX) — её ось Y; resetState до и после — блик меняет состояние GE, остальной проход рисует кусками как обычно
static void flareDrawPiece(const TrPiece *p, const float m[16], int tex) {
  int mode = (p->flags >> 10) & 3; unsigned abgr = p->nf; float P3[3] = {m[12], m[13], m[14]}, U3[3] = {m[4], m[5], m[6]};
  resetState(); carFlareOne(P3, U3, mode, p->bb[0], p->bb[1], p->bb[2], abgr, tex, eyeX, eyeY, eyeZ, bindTexLean); resetState();
}
// №779: контур машин NPC (трафик, стоящие, машины миссий) — тот же carToonHull, что у машины игрока: кроме стекла (флаг 2), запечённого водителя (owner 4) и мелочи
// (фары, зеркала: наибольший размер куска меньше NPC_TOON_MIN_EXT); дальше 40 м skinToonWorldW даёт 0. NPC_CAR_TOON_OLD=1 — без контура, как TEST33
#ifndef NPC_CAR_TOON_OLD
#define NPC_CAR_TOON_OLD 0
#endif
#define NPC_TOON_MIN_EXT .4f
static unsigned npcToonDraws;
static void npcCarToon(Piece *d, const float *m) {
  if (NPC_CAR_TOON_OLD || !skinToonEnabled() || (d->r.flags & 2) || d->r.owner == 4) return;
  float ex = d->r.bb[3] - d->r.bb[0], ey = d->r.bb[4] - d->r.bb[1], ez = d->r.bb[5] - d->r.bb[2];
  if (ex < NPC_TOON_MIN_EXT && ey < NPC_TOON_MIN_EXT && ez < NPC_TOON_MIN_EXT) return;
  carToonNpcNow = 1; carToonHull(d, m); carToonNpcNow = 0; npcToonDraws++;
}
// №930: кусок машины NPC двумя вызовами: первые kf треугольников — без текстуры (цвет в вершинах), остальные — родной текстурой (фары, решётки, номер, швы). Оба вызова — те же carGlass и drawPieceM, что у целого куска;
// оболочка контура (npcCarToon) уже поставлена по целому куску. Веера у камеры хвоста записываются на целый кусок (carFanV сверяет адрес начала вершин куска)
// №930б (дирижёр 25.10: «две заливки на кусок дают +0,79 мс PSP в сценах машин миссий L2/L4 — сделать одну»): общий разбор куска один раз. Два вызова drawPieceM каждый раз заново считали pieceOk, tqPush, bigNear
// и carNear853 (последние два перебирают вершины и кэшируют отдельно для каждой половины куска по адресу и числу индексов), писали матрицу модели и дымку свечения. Быстрый путь: кусок дальше CF_FAST_R м от глаза (у глаза
// риск выброса треугольников GE проверяется один раз по целому куску; рискованный — как раньше, два вызова), непрозрачный (флаги 2, 4 и сложение — как раньше), годен целиком: проверки один раз, матрица и дымка один раз, дальше две отрисовки (без текстуры, с родной) —
// те же bindTexLean / setFlags / drawGeo, что в drawPieceM. CARFILL930B_OLD=1 — как ccf31797 (два вызова)
#ifndef CARFILL930B_OLD
#define CARFILL930B_OLD 0
#endif
#define CF_FAST_R 20.f
static void carFillDraw(Piece *d, const ScePspFMatrix4 *m, int kf) {
  int tex0 = d->r.tex, ni0 = d->r.ni, k3 = kf * 3; unsigned vs = (d->r.flags & F_V12) ? 12u : 16u; Vert *v0 = d->v;
  if (k3 >= ni0) { d->r.tex = -1; carGlass(d); drawPieceM(d, m); d->r.tex = tex0; return; }
  if (!CARFILL930B_OLD && !(d->r.flags & (2 | 4 | F_ADDITIVE)) && !geOver(GE_HARD) && !NEON_SKIP(d) && !TREESH_SKIP(d) && !bigCutBlock) {
    float dx = m->w.x - ggE[0], dy = m->w.y - ggE[1], dz = m->w.z - ggE[2];
    unsigned fg0_ = (smkOn && trgIn && !(dx * dx + dy * dy + dz * dz > CF_FAST_R * CF_FAST_R)) ? sceKernelGetSystemTimeLow() : 0; int fgn_ = dx * dx + dy * dy + dz * dz > CF_FAST_R * CF_FAST_R || (!bigNear(d, m) && !carNear853(d, m)); if (fg0_) { trgFastN++; trgBnUs += sceKernelGetSystemTimeLow() - fg0_; }
    if (fgn_ && pieceOk(d, 0)) {   // ближе CF_FAST_R: проверки риска выброса GE — один раз по целому куску (подмножество не рискованнее целого), иначе два вызова
      RBC(rbcDrawM);
      d->r.ni = k3; d->r.tex = -1; carGlass(d); drawnTris += k3 / 3; draws++; dbgPM++;
      NZP(d); bindTexLean(-1); setFlags(d->r.flags); sceGuSetMatrix(GU_MODEL, m);
      int gf_ = glowFogBegin(d->r.flags); diagAbsM = 1; drawGeo(d, 0, k3); dbgPMgeo++;
      d->r.ni = ni0 - k3; d->r.tex = tex0; d->v = (Vert *)((char *)v0 + (size_t)k3 * vs); carGlass(d); drawnTris += d->r.ni / 3; draws++; dbgPM++;
      NZP(d); bindTexLean(tex0); setFlags(d->r.flags); diagAbsM = 1; drawGeo(d, 0, d->r.ni); dbgPMgeo++;
      glowFogEnd(gf_);
      if (carFanN < 8 && carFanHas(d->v) && !carFanHas(v0)) carFanV[carFanN++] = v0;
      d->r.ni = ni0; d->v = v0; return;
    }
  }
  d->r.ni = k3; d->r.tex = -1; carGlass(d); dpmPlain = 0; drawPieceM(d, m); dpmKeep = dpmPlain ? m : 0;   // матрицу второму вызову не писать, если первый кончил обычной отрисовкой (без заплаток у камеры)
  d->r.ni = ni0 - k3; d->r.tex = tex0; d->v = (Vert *)((char *)v0 + (size_t)k3 * vs); carGlass(d); drawPieceM(d, m); dpmKeep = 0;
  if (carFanN < 8 && carFanHas(d->v) && !carFanHas(v0)) carFanV[carFanN++] = v0;
  d->r.ni = ni0; d->v = v0;
}
static void trDrawPiece(void *u, const TrPiece *p, const void *verts, const float m[16]) {
  if (p->owner == 3) { flareDrawPiece(p, m, p->tex >= 0 ? (int)trTexBase + p->tex : -1); return; }
  (void)u; Piece d; memset(&d, 0, sizeof d); memcpy(&d.r, p, sizeof d.r);
  d.r.tex = p->tex >= 0 ? (int)trTexBase + p->tex : -1; unsigned cfp0_ = cfCostOn ? sceKernelGetSystemTimeLow() : 0; int kf_ = carFillPick(p, &verts); d.v = (Vert *)verts; if (cfCostOn) cfUsP += sceKernelGetSystemTimeLow() - cfp0_;   // №930: треугольники куска с ровной областью текстуры — без текстуры (цвет уже в вершинах), с рисунком — родной
  unsigned tw0_ = smkOn ? (unsigned)sceGuCheckList() : 0, tt0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
  npcCarToon(&d, m);   // №779
  unsigned tw1_ = smkOn ? (unsigned)sceGuCheckList() : 0, tt1_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
  if (smkOn) cfUsH += tt1_ - tt0_;
  unsigned trg0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; trgIn = 1;
  { unsigned cfq_ = cfCostOn ? sceKernelGetSystemTimeLow() : 0; if (kf_ > 0) carFillDraw(&d, (const ScePspFMatrix4 *)m, kf_); else { carGlass(&d); drawPieceM(&d, (const ScePspFMatrix4 *)m); }
    if (cfCostOn) { cfUsD[kf_ > 0] += sceKernelGetSystemTimeLow() - cfq_; cfND[kf_ > 0]++; } }   // служебно (№930б): мкс и число отрисовок куска машины NPC — целиком / двумя заливками
  trgIn = 0; if (smkOn) { trgDrawUs += sceKernelGetSystemTimeLow() - trg0_; trgPieces++; }
  if (smkOn) { smkTrfPc[0] += 1.0; smkTrfPc[1] += (tw1_ - tw0_) / 4u; smkTrfPc[2] += ((unsigned)sceGuCheckList() - tw1_) / 4u; smkTrfPc[3] += (tt1_ - tt0_) / 1000.0; smkTrfPc[4] += (sceKernelGetSystemTimeLow() - tt1_) / 1000.0; }   // №914: кусков, слов оболочки / куска, мс оболочки / куска
}
// Машины миссий с ИИ (29.09, viewer/msncar.h — логика общая с ботами): рисунок — модели пакета msncars-l<N>.shr уровня
// (tools/msncar_export.py, формат MCR1: куски как у трафика TRF1), читаются с карты памяти по требованию, когда сценарий
// ставит машину (как LoadDisposableCar оригинала), и уходят в кучу, когда машины миссии больше нет. Машина, чья модель есть в
// трафике уровня (minivanA, schoolbu …), рисуется моделью трафика — второй копии в памяти нет. Разбитая — остовом трафика
// (huskA). Каталог пакета (кузов и колёса моделей) читается при загрузке уровня: физика машин миссий — по модели
#define MCR_MAX 6            // моделей машин миссии в памяти (данные: не больше 4 машин в миссии)
// №535 (11.10): wiggu_v и gramR_v несут 14 текстур — при пределе 12 блок помечался битым и машина не грузилась никогда (в будке приезжала основная).
// Предел 16; MCR_TEXPER_OLD=1 — прежние 12
#ifndef MCR_TEXPER_OLD
#define MCR_TEXPER_OLD 0
#endif
#define MCR_TEXPER (MCR_TEXPER_OLD ? 12 : 16)   // текстур на модель (у машин диска 2–14)
#ifndef TRF_BRAKELIGHT_OLD
#define TRF_BRAKELIGHT_OLD 0   // №465: стоп-сигналы трафика и машин миссий (блики добавляют до 2 текстур к модели; предел 16 общий с №535)
#endif
#ifndef POLICE_MODEL_WARM_OLD
#define POLICE_MODEL_WARM_OLD 0
#endif
static int policeModelWarmOld=POLICE_MODEL_WARM_OLD;
typedef struct { char name[16]; unsigned char *blob; const TrModel *M; const TrPiece *pc; const unsigned char *vt; int tex0; unsigned bytes; } McrModel;
static McrModel mcr[MCR_MAX]; static unsigned char *mcrDir; static unsigned mcrN; static char mcrPath[256];
static unsigned mcrTexBase; static int mcrTexOk; static McGeo mcrGeo[64];
static unsigned mcrLoads, mcrLoadBytes; static float mcrLoadMs;   // служебное (смоук: строка MCR)
static void mcrPreDropAll(void);
static unsigned char mcrBad[64];   // блок модели не прочитался (битый / обрезанный пакет): больше не пробовать до смены уровня
static void mcrDirLoad(void) {   // каталог пакета уровня: имена, TrModel (кузов, колёса), где блок модели
  mcrPreDropAll(); free(mcrDir); mcrDir = 0; mcrN = 0; mcSetGeo(0, 0); memset(mcrBad, 0, sizeof mcrBad);
  levelFile(mcrPath, "msncars-l%d.shr");
  SceUID f = sceIoOpen(mcrPath, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned h[8]; if (sceIoRead(f, h, 32) != 32 || memcmp(h, "MCR1", 4) || h[3] > 64) { sceIoClose(f); return; }
  unsigned n = h[3] * 176;   // каталог; у версии 2 за ним — таблица машин (h[6], по h[7] Б на машину)
  if (h[1] >= 2 && h[6] >= h[4] && h[7] >= 128 && h[7] <= 256) n = h[6] - h[4] + h[3] * h[7];
  mcrDir = malloc(n ? n : 1);
  sceIoLseek32(f, h[4], PSP_SEEK_SET);
  if (!mcrDir || sceIoRead(f, mcrDir, n) != (int)n) { free(mcrDir); mcrDir = 0; sceIoClose(f); return; }
  sceIoClose(f); mcrN = (unsigned)mcGeoFromPack((const unsigned char *)h, mcrDir, n, mcrGeo, 64);
  mcSetGeo(mcrGeo, (int)mcrN);
}
static void mcrDrop(int k) {   // модель k — в кучу, её текстуры — пустые
  if (!mcr[k].blob) return;
  bbOp("car-", mcr[k].name, 0);   // правка 103: самописец
  if (mcrTexOk) for (int i = 0; i < MCR_TEXPER; i++) memset(&tex[mcrTexBase + mcr[k].tex0 + i], 0, sizeof(Tex));
  carFillSetFree(1 + k); toneBlkDel(mcr[k].blob); geDeferFree(mcr[k].blob); memset(&mcr[k], 0, sizeof mcr[k]); bound = -2;   // №930: копия вершин куска — вместе с моделью
}
// самоаудит 16.10: список «держать 90 с» (№629) принадлежит уровню, при выгрузке всех машин миссий очищается (MCR_KEEP_CLEAR_OLD=1 — как было)
#ifndef MCR_KEEP_CLEAR_OLD
#define MCR_KEEP_CLEAR_OLD 0
#endif
static char mcrKeepN[6][24]; static unsigned mcrKeepT[6];
static void mcrFreeAll(void) { mcrPreDropAll(); for (int k = 0; k < MCR_MAX; k++) mcrDrop(k); if (!MCR_KEEP_CLEAR_OLD) { memset(mcrKeepN, 0, sizeof mcrKeepN); memset(mcrKeepT, 0, sizeof mcrKeepT); } }
static const MsnCars *mcrCars(void) { return bonusCarsPtr ? bonusCarsPtr : mvCars(); }   // правка 420: машины бонус-гонки
// №629: машины этапа, прочитанные впрок под газетой, до появления в списке миссии не выгружаются (mcrSync иначе выбросил бы их в первом кадре). Держим 90 с; MSN_READAHEAD_OLD=1 — не держим
static int mcrKept(const char *name) {
  unsigned now = sceKernelGetSystemTimeLow();
  for (int i = 0; i < 6; i++) if (mcrKeepN[i][0] && now - mcrKeepT[i] < 90000000u && !strcasecmp(mcrKeepN[i], name)) return 1;
  return 0;
}
static int mcrUsed(const char *name) {
  if (mcrKept(name)) return 1;
  const char *fc = mvForcedCar(); if (fc && !strcasecmp(fc, name)) return 1;   // машина миссии у игрока (SetForcedCar)
  if (bonusPlayerModel && !strcasecmp(bonusPlayerModel, name)) return 1;
  const MsnCars *m = mcrCars(); if (!m) return 0;
  for (int i = 0; i < m->n; i++) if (m->c[i].on && !strcasecmp(m->c[i].name, name)) return 1;
  return 0;
}
static int trModelOf(const char *name) {   // модель трафика уровня того же имени (-1 нет)
  if (!haveTraffic) return -1;
  for (unsigned i = 0; i < traffic.hd->nModel; i++) if (!strncasecmp(traffic.model[i].name, name, 16)) return (int)i;
  return -1;
}
// №538 (тур 13, 11.10): на смене этапа все новые машины миссии читались с карты в одном кадре (marge_v 40 мс + snake_v 42 + cArmor 33 …) — кадр 120–180 мс. Не больше одной
// новой модели за кадр (кроме машины игрока SetForcedCar): остальные приезжают на следующих кадрах, пока без рисунка (≤ 3 кадров на старте этапа, машины стоят). MCR_STAGGER_OLD=1 — как было
#ifndef MCR_STAGGER_OLD
#define MCR_STAGGER_OLD 0
#endif
static int mcrLoadedF, mcrDeferred, mcrForceLoad, mcrStaggerOn;   // StaggerOn — только между mcrSync и концом рисования машин миссии этого кадра (бонус-гонка и будка читают без очереди)   // за этот кадр прочитана новая модель / чья-то модель отложена / читать вне очереди

// №538: блок модели читается в фоне (sceIoReadAsync), как .chr персонажей (skinPreStart): машина миссии появляется, когда блок прочитан (кадр без 33–42 мс чтения);
// не успел за MCRPRE_MAX_AGE вызовов или это машина игрока — ждём остаток, как прежнее синхронное чтение. Запас кучи — memPrefetchRoom (тот же механизм, что у персонажей, msn_view.c).
// MCR_ASYNC_OLD=1 — прежнее чтение в кадре
#include "mcrpre.h"
#ifndef MCR_ASYNC_OLD
#define MCR_ASYNC_OLD 0
#endif
#ifndef NEAR_CAR_DRAW_OLD
#define NEAR_CAR_DRAW_OLD 0   // №544/546 включены по умолчанию (слово Роба 27.10: машины, что бьют игрока, видны всегда; грузовик не пропадает у края кадра; выключили без слова Роба 15.10); прежнее -DCULL_SPHERE_OLD=1 / -DNEAR_CAR_DRAW_OLD=1; TEST17 (слово Роба/дирижёра 15.10: отрисовка как в 1721 f32c8c31): машины ближе 30 м без принудительной отрисовки (как в 1721); 0 — №544/546
#endif
typedef struct { int e; SceUID fd; unsigned char *b; unsigned size; int age; } McrPre;
#define MCR_NPRE 2
static McrPre mcrPre[MCR_NPRE];
static McrPre *mcrPreFind(int e) { for (int i = 0; i < MCR_NPRE; i++) if (mcrPre[i].b && mcrPre[i].e == e) return &mcrPre[i]; return 0; }
static void mcrPreDrop(McrPre *q) { if (!q->b) return; SceInt64 r; sceIoWaitAsync(q->fd, &r); sceIoClose(q->fd); free(q->b); memset(q, 0, sizeof *q); }
static void mcrPreDropAll(void) { for (int i = 0; i < MCR_NPRE; i++) mcrPreDrop(&mcrPre[i]); }
static int mcrPreReady(const McrPre *q) { SceInt64 r; return sceIoPollAsync(q->fd, &r) == 0; }
static unsigned char *mcrPreTake(McrPre *q, unsigned size) {   // готовый (или дождаться остатка) блок -> владение буфером; 0 — чтение не удалось
  SceInt64 r = -1; sceIoWaitAsync(q->fd, &r); sceIoClose(q->fd);
  unsigned char *b = q->b; int ok = r == (SceInt64)size && q->size == size; memset(q, 0, sizeof *q);
  if (!ok) { free(b); return 0; }
  return b;
}
static const McrModel *mcrGet(const char *name) {   // модель машины миссии в памяти; нет — прочитать блок из пакета
  for (int k = 0; k < MCR_MAX; k++) if (mcr[k].blob && !strcasecmp(mcr[k].name, name)) return &mcr[k];
  if (!mcrDir || !mcrTexOk) return 0;
  int e = -1; for (unsigned i = 0; i < mcrN; i++) if (!strncasecmp((const char *)mcrDir + i * 176, name, 16)) { e = (int)i; break; }
  if (e < 0 || mcrBad[e]) return 0;   // блок уже не читался (битый пакет) — не пробовать каждый кадр
  McrPre *pq = MCR_ASYNC_OLD ? 0 : mcrPreFind(e);
  if (pq && mcrPreDecide(mcrPreReady(pq), pq->age, mcrForceLoad) == MCRPRE_DEFER) { pq->age++; mcrDeferred = 1; return 0; }   // читается в фоне — машина на следующем кадре
  if (!pq && !MCR_STAGGER_OLD && mcrStaggerOn && mcrLoadedF && !mcrForceLoad) { mcrDeferred = 1; return 0; }   // одна новая модель за кадр
  int k = -1; for (int j = 0; j < MCR_MAX && k < 0; j++) if (!mcr[j].blob) k = j;
  for (int j = 0; j < MCR_MAX && k < 0; j++) if (!mcrUsed(mcr[j].name)) { mcrDrop(j); k = j; }
  if (k < 0) return 0;
  const unsigned char *d = mcrDir + e * 176; unsigned u[8]; memcpy(u, d + 144, sizeof u);   // блок, размер, куски, вершины, текстуры
  if (u[7] > MCR_TEXPER) { mcrBad[e] = 1; if (pq) mcrPreDrop(pq); return 0; }
  { char nm[16]; memcpy(nm, d, 15); nm[15] = 0; bbOp("car+", nm, (BB_SYNC_ASYNC_OLD || !pq) ? 1 : 0); }   // блок уже прочитан в фоне (pq) — синхронного чтения нет, запись ящика не в кадре   // правка 103: самописец — ящик на карте ДО чтения модели
  unsigned t0 = sceKernelGetSystemTimeLow();
  unsigned char *b = pq ? mcrPreTake(pq, u[1]) : 0; int fromPre = b != 0;
  if (!b) { b = memalign(64, u[1]); if (!b) return 0; }
  SceUID f = fromPre ? 0 : sceIoOpen(mcrPath, PSP_O_RDONLY, 0);
  if (!fromPre && (f < 0 || sceIoLseek32(f, u[0], PSP_SEEK_SET) < 0 || sceIoRead(f, b, u[1]) != (int)u[1])) {
    if (f >= 0) sceIoClose(f); free(b); mcrBad[e] = 1;
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "MCR bad %.15s t=%.1f\n", (const char *)d, smkT); smkWrite(s); }
    return 0; }
  if (!fromPre) sceIoClose(f);
  sceKernelDcacheWritebackRange(b, u[1]); toneBlkAdd(b, u[1], TF_MCR, u[0]);   // правка 144: палитры — из пакета заново
  McrModel *M = &mcr[k]; memcpy(M->name, d, 15); M->name[15] = 0; M->blob = b; M->bytes = u[1];
  M->M = (const TrModel *)(d + 16); M->pc = (const TrPiece *)b; M->vt = b + u[3]; M->tex0 = k * MCR_TEXPER; unsigned cf0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; carFillModelLoad(k, M->name, M->pc, u[2], M->vt, mcrPath, u[0] + u[3]); if (smkOn) { char l_[112]; snprintf(l_, sizeof l_, "CARFILLM slot=%d name=%.15s fills=%d t=%.1f ms=%.2f\n", k, M->name, cfSet[1 + k].nbits, smkT, (sceKernelGetSystemTimeLow() - cf0_) / 1000.f); smkWrite(l_); }   // №930: разметка и перекраска модели при загрузке в слот (ms — цена кадра загрузки)
  const unsigned char *p = b + u[5], *end = p + u[6];   // текстуры: как у трафика (заголовок 16 Б, CLUT 1024 Б, уровни в swizzle)
  for (unsigned t = 0; t < u[7] && p + 16 + 1024 <= end; t++) {
    Tex *x = &tex[mcrTexBase + M->tex0 + t]; memset(x, 0, sizeof *x);
    const uint16_t *hh = (const uint16_t *)p; int tw = hh[0], th = hh[1], nl = hh[2], sw = p[7] == 1;
    x->w = tw; x->h = th; x->levels = nl > 8 ? 8 : nl; x->pre = sw; x->t4 = p[6] == 4 && sw; x->pal = (void *)(p + 16); palTone((void *)(p + 16));
    const unsigned char *q = p + 16 + 1024; int lw = tw, lh = th;
    for (int j = 0; j < nl; j++) {
      if (j < 8) x->lv[j] = (void *)q;
      q += (size_t)texRowBytes(x->t4, sw, lw) * (sw ? (lh + 7) & ~7 : lh); q = (const unsigned char *)(((uintptr_t)q + 15) & ~(uintptr_t)15);
      lw = lw > 1 ? lw >> 1 : 1; lh = lh > 1 ? lh >> 1 : 1;
    }
    if (x->pre) memcpy(x->slv, x->lv, sizeof x->lv);
    p = (const unsigned char *)(((uintptr_t)q + 15) & ~(uintptr_t)15);
  }
  if (!fromPre) mcrLoadedF = 1;   // готовый фоновый блок кадр не задерживает
  bound = -2; mcrLoads++; mcrLoadBytes += u[1]; mcrLoadMs = (sceKernelGetSystemTimeLow() - t0) / 1000.f;
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "MCR load %s bytes=%u ms=%.0f t=%.1f\n", M->name, u[1], mcrLoadMs, smkT); smkWrite(s); }
  return M;
}
// Правка Роба 474 (10.10): модель машины из будки читается с карты памяти в момент вызова (блок пакета msncars-lN, а не заранее с уровнем): пакет
// на карте, в ОЗУ — каталог (176 + 128 Б на машину) и не больше MCR_MAX моделей; прежняя вызванная уходит в mcrSync, когда не нужна. Чтение — до
// появления машины, пока экран будки гаснет (иначе кадр без машины и рывок). PHONE_PREFETCH_OLD=1 — как было: чтение при первом рисовании
#ifndef PHONE_PREFETCH_OLD
#define PHONE_PREFETCH_OLD 0
#endif
static void mcrPrefetch(const char *name) {
  if (PHONE_PREFETCH_OLD || !name || !*name || trModelOf(name) >= 0) return;
  mcrGet(name);
}
static void mcrReadahead(const char *name) {   // №629: машина этапа — в память под газетой (разбор блока 14–40 мс уходит из первых секунд после Continue)
  if (!name || !*name || trModelOf(name) >= 0 || !memPrefetchRoom(768u << 10)) return;
  int slot = 0; for (int i = 0; i < 6; i++) { if (!mcrKeepN[i][0] || sceKernelGetSystemTimeLow() - mcrKeepT[i] >= 90000000u) { slot = i; break; } slot = (i + 1) % 6; }
  strncpy(mcrKeepN[slot], name, 23); mcrKeepN[slot][23] = 0; mcrKeepT[slot] = sceKernelGetSystemTimeLow();
  int sv_ = mcrStaggerOn; mcrStaggerOn = 0; mcrGet(name); mcrStaggerOn = sv_;   // не откладывать до следующего кадра: списка машин миссии это имя ещё не знает
}
static void mcrPreStart(const char *name) {   // начать фоновое чтение блока модели name, если можно
  if (MCR_ASYNC_OLD || !mcrDir || !mcrTexOk) return;
  for (int k = 0; k < MCR_MAX; k++) if (mcr[k].blob && !strcasecmp(mcr[k].name, name)) return;
  int e = -1; for (unsigned i = 0; i < mcrN; i++) if (!strncasecmp((const char *)mcrDir + i * 176, name, 16)) { e = (int)i; break; }
  if (e < 0 || mcrBad[e] || mcrPreFind(e)) return;
  int qf = 0, ms = 0, pend = 0; McrPre *slot = 0;
  for (int i = 0; i < MCR_NPRE; i++) { if (!mcrPre[i].b) { qf++; if (!slot) slot = &mcrPre[i]; } else pend++; }
  for (int j = 0; j < MCR_MAX; j++) if (!mcr[j].blob || !mcrUsed(mcr[j].name)) ms++;
  const unsigned char *d = mcrDir + e * 176; unsigned u[8]; memcpy(u, d + 144, sizeof u);
  if (u[7] > MCR_TEXPER || !u[1]) return;
  int heapOk = memPrefetchRoom(u[1] + (64u << 10));
  if (!mcrPreMayStart(qf, ms - pend, heapOk, 0, 0)) return;
  unsigned char *b = memalign(64, u[1]); if (!b) return;
  SceUID f = sceIoOpen(mcrPath, PSP_O_RDONLY, 0);
  if (f < 0 || sceIoLseek32(f, u[0], PSP_SEEK_SET) < 0 || sceIoReadAsync(f, b, u[1]) < 0) { if (f >= 0) sceIoClose(f); free(b); return; }
  slot->e = e; slot->fd = f; slot->b = b; slot->size = u[1]; slot->age = 0;
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "MCR pre %.15s bytes=%u t=%.1f\n", (const char *)d, u[1], smkT); smkWrite(s); }
}
static void mcrSync(void) {   // кадр: модели машин, которых в миссии больше нет, — в кучу; новые — в память (до рисования)
  for (int k = 0; k < MCR_MAX; k++) if (mcr[k].blob && !mcrUsed(mcr[k].name) &&
      (policeModelWarmOld || !mvIsPolice(mcr[k].name))) mcrDrop(k);
  mcrLoadedF = 0; mcrDeferred = 0; mcrStaggerOn = 1;
  const char *fc = mvForcedCar(); if (fc) { mcrForceLoad = 1; mcrGet(fc); mcrForceLoad = 0; }
  const MsnCars *m = mcrCars(); if (!m) return;
  for (int i = 0; i < m->n; i++) if (m->c[i].on && trModelOf(m->c[i].name) < 0) {
    mcrPreStart(m->c[i].name);
#if !NEAR_CAR_DRAW_OLD
    // №544: если машина миссии близка к игроку/камере или в контакте — завершить чтение немедленно
    float px = onFoot() ? homer.x : car.x, pz = onFoot() ? homer.z : car.z;
    float dx = m->c[i].car.x - px, dz = m->c[i].car.z - pz;
    int nearP = (dx * dx + dz * dz <= 30.f * 30.f) || m->c[i].car.contacts > 0 || (!onFoot() && carNear2(&car, &m->c[i].car, 0.5f));
    if (nearP) mcrForceLoad = 1;
#endif
    mcrGet(m->c[i].name);
#if !NEAR_CAR_DRAW_OLD
    if (nearP) mcrForceLoad = 0;
#endif
  }
}
// Port I/O policy: read the native level's police model while NEW GAME is loading.
// The existing unused-slot eviction still releases this reservation if a mission needs it.
static void mcrWarmPolice(void) {
  if(policeModelWarmOld || !mvActive() || !mcrDir)return;
  for(unsigned i=0;i<mcrN;i++){
    char name[17];memcpy(name,mcrDir+i*176,16);name[16]=0;
    if(mvIsPolice(name)&&trModelOf(name)<0){mcrGet(name);break;}
  }
}
static void mcDrawPiece(void *u, const TrPiece *p, const void *verts, const float m[16]) {
  if (p->owner == 3) { flareDrawPiece(p, m, p->tex >= 0 ? *(const int *)u + p->tex : -1); return; }
  Piece d; memset(&d, 0, sizeof d); memcpy(&d.r, p, sizeof d.r);
  d.r.tex = p->tex >= 0 ? *(const int *)u + p->tex : -1; int kf_ = carFillPick(p, &verts); d.v = (Vert *)verts;   // №930: куски машин миссий — тоже по боковому файлу пакета
  npcCarToon(&d, m);   // №779: машины миссий
  { unsigned cfq_ = cfCostOn ? sceKernelGetSystemTimeLow() : 0; if (kf_ > 0) carFillDraw(&d, (const ScePspFMatrix4 *)m, kf_); else { carGlass(&d); drawPieceM(&d, (const ScePspFMatrix4 *)m); }
    if (cfCostOn) { cfUsD[kf_ > 0] += sceKernelGetSystemTimeLow() - cfq_; cfND[kf_ > 0]++; } }   // служебно (№930б): мкс и число отрисовок куска машины NPC — целиком / двумя заливками
}
// №867: кусок колеса своей машины (CAR1) — боковины с картинкой диска без качения, протектор с качением (wheelcap.h). 1 — нарисовано по частям;
// 0 — не колесо / делить нечего / полупрозрачное (очередь tq хранит указатель куска — копию на стеке туда не отдаём): рисует вызывающий, как было
#include "wheelcap.h"
static int carCapSplitDraw(const CarVis *v, const Car *c, Piece *d, int joint, int variant, const float m[16], int toon) {
  if (variant || (d->r.flags & (2 | F_STRIP)) || !(d->r.flags & F_NOIDX) || !carVisWheelJoint(v, joint) || !v->capStill) return 0;   // №878: диск без перелива крутится с колесом
  int vs = (d->r.flags & F_V12) ? 12 : 16, cap = wheelCapN((void *)d->v, d->r.ni, vs); if (cap <= 0) return 0;
  float m0[16]; carVisPieceMatrixEx(v, c, joint, variant, d->r.ox, d->r.oy, d->r.oz, d->r.sc, 1, m0);
  Piece t = *d; t.r.ni = cap; if (toon) carToonHull(&t, m0); drawPieceM(&t, (const ScePspFMatrix4 *)m0);
  t.v = (Vert *)((char *)d->v + (size_t)cap * vs); t.r.ni = d->r.ni - cap; if (toon) carToonHull(&t, m); drawPieceM(&t, (const ScePspFMatrix4 *)m);
  return 1;
}
static int mcrDrawn, mcrTris;
// Правка Роба 268 (04.10: «у ракетной машины стоит машина, которую не видно — въехал в пустоту и разбил»): машина миссии в кадре
// без модели (нет в пакете, все MCR_MAX мест заняты, не хватило памяти) — строка MCRNONE в журнал и в чёрный ящик, раз на имя
static char mcrNoneSeen[8][16]; static int mcrNoneN;
static void mcrNoModel(const char *name, const Car *k) {
  for (int i = 0; i < mcrNoneN; i++) if (!strncasecmp(mcrNoneSeen[i], name, 15)) return;
  if (mcrNoneN < 8) { strncpy(mcrNoneSeen[mcrNoneN], name, 15); mcrNoneSeen[mcrNoneN][15] = 0; mcrNoneN++; }
  int used = 0; for (int j = 0; j < MCR_MAX; j++) used += mcr[j].blob != 0;
  char s[160]; snprintf(s, sizeof s, "MCRNONE %.15s x=%.0f z=%.0f slots=%d/%d dir=%d t=%.1f\n", name, k->x, k->z, used, MCR_MAX, mcrDir != 0, smkT);
  if (smkOn) smkWrite(s);
  bbOp("car?", name, 0);
}
// SetForcedCar (L1 m6 marge_v): машина игрока — модель пакета msncars (без суставов CAR1: двери, урон по зонам, фонари — у машины
// уровня; правило порта). -> 1 — нарисована (CAR1-куски машины уровня не рисовать)
static int huskBurnOld = HUSK_BURN_OLD;   // №697: служебное «flag burnold 1» — остов горит постоянно (красный контроль)
static int huskOld;   // правка 286: служебное «flag huskold 1» — угнанная после взрыва целая (красный контроль)
static int mcrDrawPlayer2(const Car *k, float wheelAng, const char *fc);
static int mcrDrawPlayer(const Car *k, float wheelAng) {
  const char *fc = mvForcedCar(); if (!fc) return 0;
  trHideDriver = 1; carFillPlayer++; int r = mcrDrawPlayer2(k, wheelAng, fc); carFillPlayer--; trHideDriver = 0; return r;   // №930: машина игрока (в том числе после пересадки) — с текстурой   // №666: у машины игрока водителя нет
}
static int mcrDrawPlayer2(const Car *k, float wheelAng, const char *fc) {
  // правка Роба 286 (04.10: «школьный автобус горит и взрывается, но стоит целый»): угнанная машина / машина миссии у игрока после
  // взрыва — остов трафика (huskA), как машины миссий (mcrDraw) и трафик; прежде рисовалась целой моделью. «flag huskold 1» — прежнее
  { static int logged; if (smkOn && k->destroyed && !logged) { logged = 1; char s[96]; snprintf(s, sizeof s, "PLAYERHUSK car=%s husk=%d t=%.1f\n", fc, haveTraffic && !huskOld, smkT); smkWrite(s); } }
  if (carShowHusk(k) && haveTraffic && !huskOld) { int b = (int)trTexBase;
    trafficDrawModel(&traffic.model[traffic.hd->huskModel], traffic.piece, traffic.vert, k, wheelAng, 0, mcDrawPiece, &b, &mcrTris); return 1; }
  { int tm = trModelOf(fc); if (tm >= 0) { int b = (int)trTexBase;   // правка 63: угнанная машина — модель трафика уровня
      trafficDrawModel(&traffic.model[tm], traffic.piece, traffic.vert, k, wheelAng, 0, mcDrawPiece, &b, &mcrTris); return 1; } }
  mcrForceLoad = 1; const McrModel *M = mcrGet(fc); mcrForceLoad = 0; if (!M) return 1;   // модель дочитывается — кадр без машины, не машина уровня
  int b = (int)(mcrTexBase + M->tex0); trafficDrawModel(M->M, M->pc, M->vt, k, wheelAng, 0, mcDrawPiece, &b, &mcrTris);
  return 1;
}
static void mcrDraw(const float e3[3]) {   // машины миссии: модель пакета или трафика; разбитая — остов трафика
  const MsnCars *m = mcrCars(); mcrDrawn = mcrTris = 0; if (!m) return;
  for (int i = 0; i < m->n; i++) {
    const McCar *c = &m->c[i]; if (!c->on) continue;
    const Car *k = &c->car; float dx = k->x - e3[0], dy = k->y - e3[1], dz = k->z - e3[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
#if NEAR_CAR_DRAW_OLD
    float R = k->p.halfL + 1.5f;
    // Called only in the city (!inR). L3/L6 streets descend below -10 m.
    if (d - R > 110.f) continue;
    float bb[6] = {k->x - R, k->y - 1.f, k->z - R, k->x + R, k->y + 4.f, k->z + R};
    trVisCar = k; { int vv_ = trVis(bb); trVisCar = 0; if (!vv_) continue; }
    int tm = carShowHusk(k) && haveTraffic ? (int)traffic.hd->huskModel : trModelOf(c->name);
    float ldist = (tm >= 0 && haveTraffic && traffic.hd && tm < (int)traffic.hd->nModel) ? trLodDist(&traffic.model[tm]) : TR_LOD_DIST;
    int lod = d > ldist;
    if (tm >= 0) { int b = (int)trTexBase; trafficDrawModel(&traffic.model[tm], traffic.piece, traffic.vert, k, c->wheelAng, lod, mcDrawPiece, &b, &mcrTris); }
    else { const McrModel *M = mcrGet(c->name); if (!M) { if (!mcrDeferred) mcrNoModel(c->name, k); continue; }
      if (M->M) lod = d > trLodDist(M->M);
      int b = (int)(mcrTexBase + M->tex0); trafficDrawModel(M->M, M->pc, M->vt, k, c->wheelAng, lod, mcDrawPiece, &b, &mcrTris); }
#else
    // №544 & №546: машина в контакте или <= 30 м к игроку/камере рисуется всегда
    float px = onFoot() ? homer.x : car.x, py = onFoot() ? homer.y : car.y, pz = onFoot() ? homer.z : car.z;
    float dpx = k->x - px, dpy = k->y - py, dpz = k->z - pz;
    float dp = sqrtf(dpx * dpx + dpy * dpy + dpz * dpz);
    int inContact = (!onFoot() && (carNear2(&car, k, 0.5f) || k->contacts > 0 || k->lastHit > 0.f || k->carHitV > 0.f));
    int drawAlways = (d <= 30.f) || (dp <= 30.f) || inContact;
    float R = carSphereR(k);
    if (!drawAlways) {
      if (d - R > 110.f) continue;
      float bb[6] = {k->x - R, k->y - R, k->z - R, k->x + R, k->y + R, k->z + R};
      if (!trVis(bb)) continue;
    }
    int tm = carShowHusk(k) && haveTraffic ? (int)traffic.hd->huskModel : trModelOf(c->name);
    float ldist = (tm >= 0 && haveTraffic && traffic.hd && tm < (int)traffic.hd->nModel) ? trLodDist(&traffic.model[tm]) : TR_LOD_DIST;
    int lod = d > ldist;
    trHideDriver = bonusCarsPtr ? 0 : mvDriverShown(i);   // №666: водитель-персонаж рисуется отдельно — запечённого не рисуем
    if (tm >= 0) { int b = (int)trTexBase; trafficDrawModel(&traffic.model[tm], traffic.piece, traffic.vert, k, c->wheelAng, lod, mcDrawPiece, &b, &mcrTris); }
    else {
      // №544: убрать пропуск кадром позже от фонового чтения (MCR_ASYNC) для близких/контактных машин
      if (drawAlways) mcrForceLoad = 1;
      const McrModel *M = mcrGet(c->name);
      if (drawAlways) mcrForceLoad = 0;
      if (!M) { if (!mcrDeferred) mcrNoModel(c->name, k); continue; }
      if (M->M) lod = d > trLodDist(M->M);
      int b = (int)(mcrTexBase + M->tex0); trafficDrawModel(M->M, M->pc, M->vt, k, c->wheelAng, lod, mcDrawPiece, &b, &mcrTris);
    }
    trHideDriver = 0;
#endif
    if(policeFxDraw(c,c->car.t,e3))resetState();
    mcrDrawn++;
  }
}

static float boxD2(const float *b) {
  float dx = b[0] - cullMg - eyeX > 0 ? b[0] - cullMg - eyeX : (eyeX - b[3] - cullMg > 0 ? eyeX - b[3] - cullMg : 0);
  float dy = b[1] - cullMg - eyeY > 0 ? b[1] - cullMg - eyeY : (eyeY - b[4] - cullMg > 0 ? eyeY - b[4] - cullMg : 0);
  float dz = b[2] - cullMg - eyeZ > 0 ? b[2] - cullMg - eyeZ : (eyeZ - b[5] - cullMg > 0 ? eyeZ - b[5] - cullMg : 0);
  return dx * dx + dy * dy + dz * dz;
}
static int coneOut(float cx, float cy, float cz, float rad, float *dist) {   // старое отсечение v7
  float dx = cx - eyeX, dy = cy - eyeY, dz = cz - eyeZ, d2 = dx * dx + dy * dy + dz * dz, d = sqrtf(d2);
  *dist = d - rad; if (d - rad > drawDist) return 1;
  float f = dx * cfX + dy * cfY + dz * cfZ, l = sqrtf(fmaxf(0.f, d2 - f * f));
  return l * CONE_COS - f * CONE_SIN > rad;
}

#define MAXDRAW 2048
static unsigned short tk[MAXDRAW], tp[MAXDRAW], sp[MAXDRAW], cnt2k[2048]; static unsigned char tf[MAXDRAW], sf[MAXDRAW];
static unsigned short tcl[MAXDRAW], scl[MAXDRAW];   // render_fast О4: клетка куска (0xFFFF — полупрозрачный / вне u16) рядом с ключом
static int nd;
// настоящая стена ПЕРЕД глазом (TEST22: воксель с соседями со всех сторон давал ложные 30/30 на причале и у такси): занят воксель на луче взгляда от глаза до r м шагом 0,1 м
// в сетке негоризонтальных граней camgrid (клетка готова, cgs == 2); боковые и задние соседи не считаются
// самоаудит 16.10: перебор всех клеток каждый кадр (городские сотни) заменён списком клеток рядом с глазом: клетки, чья рамка ближе CW_BUILD_R, список пересчитывается при сдвиге глаза
// больше CW_MOVE, смене числа клеток или r > CW_BUILD_R - CW_MOVE; готовность клетки (state, cg, cgs) проверяется каждый раз, ответ тот же побитно. CAM_WALL_CACHE_OLD=1 — полный перебор
#ifndef CAM_WALL_CACHE_OLD
#define CAM_WALL_CACHE_OLD 0
#endif
#define CW_BUILD_R 3.f
#define CW_MOVE .5f
#define CW_MAX 48
static unsigned short cwCand[CW_MAX]; static int cwN = -1; static float cwX, cwY, cwZ; static unsigned cwNcell;
static int camRealWall(float r) {
  float fl_ = sqrtf(cfX * cfX + cfY * cfY + cfZ * cfZ); if (fl_ < 1e-4f) return 0;
  float fx_ = cfX / fl_, fy_ = cfY / fl_, fz_ = cfZ / fl_;
  int useC_ = !CAM_WALL_CACHE_OLD && r <= CW_BUILD_R - CW_MOVE;
  if (useC_) {
    if (cwN < 0 || cwNcell != ncell || fabsf(eyeX - cwX) > CW_MOVE || fabsf(eyeY - cwY) > CW_MOVE || fabsf(eyeZ - cwZ) > CW_MOVE) {
      cwN = 0; cwX = eyeX; cwY = eyeY; cwZ = eyeZ; cwNcell = ncell;
      for (unsigned ci_ = 0; ci_ < ncell; ci_++) { const Cell *c_ = &cl[ci_];
        if (eyeX < c_->bb[0] - CW_BUILD_R || eyeX > c_->bb[3] + CW_BUILD_R || eyeY < c_->bb[1] - CW_BUILD_R || eyeY > c_->bb[4] + CW_BUILD_R || eyeZ < c_->bb[2] - CW_BUILD_R || eyeZ > c_->bb[5] + CW_BUILD_R) continue;
        if (cwN >= CW_MAX) { cwN = -2; break; }
        cwCand[cwN++] = (unsigned short)ci_; }
    }
    if (cwN == -2) useC_ = 0;
  }
  unsigned nIt_ = useC_ ? (unsigned)cwN : ncell;
  for (unsigned ii_ = 0; ii_ < nIt_; ii_++) { unsigned ci_ = useC_ ? cwCand[ii_] : ii_; const Cell *c_ = &cl[ci_];
    if (c_->state != 2 || !c_->cg || c_->cgs != 2 || eyeX < c_->bb[0] - r || eyeX > c_->bb[3] + r || eyeY < c_->bb[1] - r || eyeY > c_->bb[4] + r || eyeZ < c_->bb[2] - r || eyeZ > c_->bb[5] + r) continue;
    for (float t_ = 0.f; t_ <= r; t_ += .1f) { float q_[3] = {eyeX + fx_ * t_, eyeY + fy_ * t_, eyeZ + fz_ * t_}; if (cgOcc(c_, q_)) return 1; } }
  return 0;
}
static int ggNearAny(void) { return camRealWall(GG_NEAR_R); }
static unsigned regAt(float x, float z) {   // маска регионов клетки (вне сетки, без REG1 или «flag noreg» — все)
  if (!regMask || regOff) return 0xFFFFu;
  int gx = (int)floorf((x - regOx) / regCell), gz = (int)floorf((z - regOz) / regCell);
  if (gx < 0 || gz < 0 || gx >= regW || gz >= regH) return 0xFFFFu;
  return regMask[gz * regW + gx];
}
static void cullPiece(unsigned k) {
  RBC(rbcCull);
  if (cr && (opt & O_PLANES) && !cr[k].pad && cr[k].cell != 0xFFFFu) {   // горячая запись: то же решение без чтения Piece
    const CullRec *h = &cr[k]; unsigned rg = h->rg;
    if (rg && rg <= 16 && !((regEye >> (rg - 1)) & 1u)) { regCut++; return; }
    float d2 = boxD2(h->bb);
    if (d2 > drawDist * drawDist || !visBox(h->bb, 0, 0, 0)) return;
    int far_ = (opt & O_FAR) ? d2 > LOD_DIST * LOD_DIST : 0;
    if (nd >= MAXDRAW) return;
    tk[nd] = h->tk; tp[nd] = k; tf[nd] = far_; tcl[nd] = h->tcl; nd++;
    return;
  }
  Piece *d = &pc[k]; int far;
  { unsigned rg = d->r.pad[0]; if (rg && rg <= 16 && !((regEye >> (rg - 1)) & 1u)) { regCut++; return; } }   // правка 95: регион не загружен
  if (opt & O_PLANES) {
    float d2 = boxD2(d->r.bb);
    if (d2 > drawDist * drawDist || !visBox(d->r.bb, 0, 0, 0)) return;
    far = d2 > LOD_DIST * LOD_DIST;
    if (farPlainD > 0.f && (d->r.flags & F_FARP)) far = d2 > farPlainD * farPlainD;   // правка 508: дальняя версия без нарезки — по своему порогу, не по LOD 50 м
  } else {
    float dist; if (coneOut(d->cx, d->cy, d->cz, d->rad, &dist)) return;
    far = dist > LOD_DIST;
    if (farPlainD > 0.f && (d->r.flags & F_FARP)) far = dist > farPlainD;
  }
  if (!(opt & O_FAR)) far = 0;
  if (nd >= MAXDRAW) return;
  int t = d->r.tex < 0 || d->r.tex > 510 ? 511 : d->r.tex;
  tk[nd] = ((d->r.flags & 3) << 9) | t; tp[nd] = k; tf[nd] = far;
  tcl[nd] = ((d->r.flags & 2) && !(d->r.flags & 4)) || d->r.cell >= 0xFFFFu ? 0xFFFFu : (unsigned short)d->r.cell; nd++;
}
// Иерархическое отсечение (cullhier.h): куски PVS по ящику клетки; crc — клетка куска отдельным массивом u16 (горячая запись куска не читается у отброшенных)
#ifndef CULL_HIER_OLD
#define CULL_HIER_OLD 0
#endif
static unsigned short *crc; static unsigned char *cceC; static unsigned *cceE; static unsigned cceN, cceEp;
static void ccLevelReset(void);
static void cullHierFree(void) { ccLevelReset(); free(crc); crc = 0; free(cceC); cceC = 0; free(cceE); cceE = 0; cceN = 0; }
static int cullHierReady(void) {   // таблицы по клеткам (лениво: после загрузки уровня)
  if (CULL_HIER_OLD || !cr || !npc || !ncell) return 0;
  if (!crc) { crc = (unsigned short *)malloc(npc * sizeof *crc); if (!crc) return 0; for (unsigned k = 0; k < npc; k++) crc[k] = cr[k].cell; }
  if (cceN != ncell) { free(cceC); free(cceE); cceC = (unsigned char *)calloc(ncell, 1); cceE = (unsigned *)calloc(ncell, sizeof *cceE); cceN = cceC && cceE ? ncell : 0; cceEp = 0; if (!cceN) return 0; }
  return 1;
}
static inline int cellClassC(unsigned ci) {
  if (cceE[ci] == cceEp) return cceC[ci];
  float bi[6]; memcpy(bi, cl[ci].bb, sizeof bi); if (cullMg > 0.f) { bi[0] -= cullMg; bi[1] -= cullMg; bi[2] -= cullMg; bi[3] += cullMg; bi[4] += cullMg; bi[5] += cullMg; }
  int c = cl[ci].state == 2 ? chClassify(bi, pl, eyeX, eyeY, eyeZ, drawDist) : 0;
  cceE[ci] = cceEp; cceC[ci] = (unsigned char)c; return c;
}
static void cullPieceIn(unsigned k, int inside) {   // горячая ветка cullPiece; inside — плоскости пройдены клеткой целиком
  RBC(rbcCull);
  const CullRec *h = &cr[k]; unsigned rg = h->rg;
  if (rg && rg <= 16 && !((regEye >> (rg - 1)) & 1u)) { regCut++; return; }
  float d2 = boxD2(h->bb);
  if (d2 > drawDist * drawDist || (!inside && !visBox(h->bb, 0, 0, 0))) return;
  int far_ = (opt & O_FAR) ? d2 > LOD_DIST * LOD_DIST : 0;
  if (nd >= MAXDRAW) return;
  tk[nd] = h->tk; tp[nd] = k; tf[nd] = far_; tcl[nd] = h->tcl; nd++;
}
static int pvsList(float x, float z, unsigned short **out) {
  if (!pvsEnt) return -1;
  int gx = (int)floorf((x - pvsOx) / pvsCell), gz = (int)floorf((z - pvsOz) / pvsCell);
  if (gx < 0 || gz < 0 || gx >= pvsW || gz >= pvsH) return -1;
  unsigned *e = pvsEnt + 2 * (gz * pvsW + gx);
  if (e[1] == 0xFFFFFFFFu) return -1;
  *out = pvsPool + e[0]; return (int)e[1];
}
// PVS по курсу (pvs2.h): для горячей клетки глаза — список сектора курса камеры вместо списка клетки; взгляд круто вниз/вверх (горизонтальная часть < 0,5) — полный список
static int pvsListS(float x, float z, const float *fn, unsigned short **out) {
  int n = pvsList(x, z, out);
  if (n < 0 || PVS_SECTOR_OLD || pv2Off || !pv2.ok) return n;
  int gx = (int)floorf((x - pvsOx) / pvsCell), gz = (int)floorf((z - pvsOz) / pvsCell); unsigned ci = (unsigned)(gz * pvsW + gx);
  int h = pvs2Find(&pv2, ci); if (h < 0) return n;
  if (fn[0] * fn[0] + fn[2] * fn[2] < .25f) return n;
  int s = pvs2Sector(atan2f(fn[0], fn[2]), pv2CurCell == (int)ci ? pv2Cur : -1); pv2Cur = s; pv2CurCell = (int)ci;
  const unsigned short *l; int m = pvs2List(&pv2, h, s, &l); if (m < 0) return n;
  *out = (unsigned short *)l; return m;
}
static int pvsUsed;
// Правка Роба 84 (место L1 F10-7, крыша заправки: «в сторону особняка Бёрнса — только купол деревьев»): PVS посчитан для глаза на
// 1.2–6.5 м над слоями карты высот клетки (tools/pvs.c). Глаз выше верхнего слоя клетки больше чем на PVS_EYE_MAX — PVS для него
// неверен (с крыши видно дальше): куски по пирамиде обзора, как за 115 м
#define PVS_EYE_MAX 7.f
static int pvsEyeOk(void) {
  if (!hm || hcell <= 0.f) return 1;
  int gx = (int)((eyeX - hox) / hcell), gz = (int)((eyeZ - hoz) / hcell);
  if (gx < 0 || gz < 0 || gx >= hw || gz >= hh) return 1;
  const short *c = hm + ((size_t)gz * hw + gx) * 4; int top = -32768;
  for (int k = 0; k < 4; k++) if (c[k] != -32768 && c[k] > top) top = c[k];
  return top == -32768 || eyeY <= top * .01f + PVS_EYE_MAX;
}
// B8 (план оптимизации, 10.10): при почти стоящей камере (после взрыва у остова, у перекрёстка) отсечение города повторять незачем: список прошлого кадра годится, если глаз сместился
// не более CR_EYE м, взгляд повернулся не более CR_DIR (разность единичных векторов), и те же дальность, PVS-список глаза, регионы, набор загруженных клеток, нет сброса списка (nd), не дольше
// CR_AGE кадров. Чтобы у кромки кадра не было «поп-ина», вычисление идёт с запасом ящиков cullMg (м) = поворот × (дальность + 30) / 1,4 + смещение + 0,1: любой кусок, видимый из сдвинутой позы,
// уже в списке (вывод: n_i · (x − e) в сдвинутой позе ≥ −|δn| · D − |n| · |δe|, нормали n = f·tv ± r имеют |n| ≈ 1,4–1,5). Запас ≈1,0–1,3 м: лишних кусков у кромки ≈1–2 %.
// -DCULL_REUSE_OLD=1 — как до B8 (без запаса и без повтора)
#ifndef CULL_REUSE_OLD
#define CULL_REUSE_OLD BIS(3)
#endif
#ifndef CR_EYE
#define CR_EYE .2f
#endif
#ifndef CR_DIR
#define CR_DIR .002f
#endif
#ifndef CR_AGE
#define CR_AGE 24
#endif
// №919 (наряд «езда L4/L7»): при езде глаз уходит на 0,5 м за кадр, поворот взгляда 0,5–2° — допуски B8 (0,2 м и ~0,1°) срабатывают 1 кадр из 30 (RFAST cull_reuse), отсечение города (ph7) стоит
// ~0,5 мс Мака за кадр. Допуски шире (CR_EYE_MV, CR_DIR_MV, возраст CR_AGE_MV) вместе с тем же запасом ящиков cullMg = поворот × (дальность + 30) / 1,4 + смещение + 0,1 (при допусках 1,6 м и 0,016 — около 3,4 м на 150 м дальности; кусков в списке больше на ~4 %): список, собранный для прежней
// позы, содержит всё, что видно из сдвинутой (вывод у B8). CR_MOVE_OLD=1 / «flag crmove 0|1» / файл crmoveold рядом с EBOOT — прежние допуски. «flag crchk 1» — сверка каждого повтора со свежим
// отсечением (строка CRCHK: viol — кусков свежего списка, которых нет в повторённом); «flag crmove 2» — отрицательный контроль: широкие допуски без запаса (viol > 0)
#ifndef CR_MOVE_OLD
#define CR_MOVE_OLD 0
#endif
#ifndef CR_EYE_MV
#define CR_EYE_MV 1.6f
#endif
#ifndef CR_DIR_MV
#define CR_DIR_MV .016f
#endif
#ifndef CR_AGE_MV
#define CR_AGE_MV 8
#endif
static int crMoveOn = !CR_MOVE_OLD;   // 1 — широкие допуски, 0 — B8, 2 — широкие без запаса (только отрицательный контроль)
static int crChkOn, crVerifyRun; static unsigned crChkFrames, crChkViol, crChkExtra, crChkCand;
#define CR_EYE_V (crMoveOn ? CR_EYE_MV : CR_EYE)
#define CR_DIR_V (crMoveOn ? CR_DIR_MV : CR_DIR)
#define CR_AGE_V (crMoveOn ? CR_AGE_MV : CR_AGE)
static struct { float e[3], f[3], dd; int fly; unsigned regEye, cellSig, opt, nd; const unsigned short *pvs; int pvsN; unsigned age; int valid; } crv;
static unsigned rfCullReuse, rfCullAll;
static void crvResetFwd(void) { crv.valid = 0; }   // новый уровень: список отсечения прошлого не годится
static unsigned cellSigNow(void) { unsigned h = 2166136261u; for (unsigned i = 0; i < ncell; i++) h = (h ^ (unsigned)(cl[i].state == 2 ? 2u + (i & 1u) : 1u)) * 16777619u; return h; }
static int ccNoMe, ccFromMe;   // №653: 1 — cullCity считает сам (запасной путь cullCityEnd / сверка)
static void ccMainSample(unsigned us);
static void cullFinish(int fly, const float *fn_, unsigned regE_, const unsigned short *pl0_, int n0_, unsigned us0);
static int ccBegin(int fly, const unsigned short *pl_, int n, float pr2, unsigned regE_, const float *fn_, const unsigned short *pl0_, int n0_, unsigned us0);
static void cullCity(int fly);
// №919: сверка повтора: свежее отсечение для нынешней позы без запаса (cullMg = 0) идёт тем же cullCity в режиме crVerifyRun; каждый его кусок обязан быть в повторённом списке sp[0..nd)
static void crVerify(int fly) {
  static unsigned char *bm; static unsigned bmN;
  if (bmN < npc) { free(bm); bm = (unsigned char *)calloc((npc + 7) / 8, 1); bmN = bm ? npc : 0; if (!bm) return; } else memset(bm, 0, (npc + 7) / 8);
  int nd0 = nd, pu0 = pvsUsed; unsigned rc0 = regCut, re0 = regEye;
  for (int i = 0; i < nd0; i++) bm[sp[i] >> 3] |= 1u << (sp[i] & 7);
  crVerifyRun = 1; cullCity(fly); crVerifyRun = 0;
  unsigned viol = 0; for (int i = 0; i < nd; i++) if (!(bm[tp[i] >> 3] >> (tp[i] & 7) & 1u)) viol++;
  crChkFrames++; crChkViol += viol; crChkCand += (unsigned)nd; crChkExtra += (unsigned)(nd0 > nd ? nd0 - nd : 0);
  nd = nd0; pvsUsed = pu0; regCut = rc0; regEye = re0;
}
static void cullCity(int fly) {
  unsigned us0 = sceKernelGetSystemTimeLow();   // render_fast: время отсечения (строка RFAST cull_us)
  float fl_ = sqrtf(cfX * cfX + cfY * cfY + cfZ * cfZ); if (fl_ < 1e-6f) fl_ = 1.f; float fn_[3] = {cfX / fl_, cfY / fl_, cfZ / fl_};
  unsigned regE_ = zgOn ? regRes : regAt(eyeX, eyeZ);
  int reuseOk = 0; unsigned short *pl0_ = 0; int n0_ = -1;
  if (!CULL_REUSE_OLD) {
    n0_ = (opt & O_PVS) && !fly && !regOff && pvsEyeOk() ? pvsListS(eyeX, eyeZ, fn_, &pl0_) : -1;
    float dx = eyeX - crv.e[0], dy = eyeY - crv.e[1], dz = eyeZ - crv.e[2], ax = fn_[0] - crv.f[0], ay = fn_[1] - crv.f[1], az = fn_[2] - crv.f[2];
    reuseOk = !crVerifyRun && crv.valid && crv.age < (unsigned)CR_AGE_V && crv.nd == (unsigned)nd && crv.fly == fly && crv.dd == drawDist && crv.regEye == regE_ && crv.opt == (unsigned)opt && crv.pvs == pl0_ && crv.pvsN == n0_
              && dx * dx + dy * dy + dz * dz <= CR_EYE_V * CR_EYE_V && ax * ax + ay * ay + az * az <= CR_DIR_V * CR_DIR_V && crv.cellSig == cellSigNow();
    rfCullAll++;
    if (reuseOk) { if (crChkOn) crVerify(fly); crv.age++; rfCullReuse++; rfCullUs += sceKernelGetSystemTimeLow() - us0; return; }   // список, сортировка, nd, pvsUsed, regCut — прошлого вычисления
    cullMg = crVerifyRun || crMoveOn == 2 ? 0.f : CR_DIR_V * (drawDist + 30.f) / 1.4f + CR_EYE_V + .1f;
  }
  nd = 0; pvsUsed = 0; regCut = 0; regEye = regE_;   // правка 181: набор ворот
  // «flag noreg» (отрицательный контроль правки 95): все регионы и без PVS (списки PVS уже без чужих регионов) — как до правки
  unsigned short *pl_; int n = (opt & O_PVS) && !fly && !regOff && pvsEyeOk() ? pvsListS(eyeX, eyeZ, fn_, &pl_) : -1;
  if (n >= 0 && pv2.ok && pl_ >= (unsigned short *)pv2.pool && pl_ < (unsigned short *)pv2.pool + pv2.npool) rfPv2Frames++;
  if (n >= 0) {                                        // PVS: только куски, видимые из этой клетки 20x20 м
    pvsUsed = 1;
    float pr2 = drawDist > pvsRange ? pvsRange * pvsRange : 1e30f;   // дальний режим: из PVS только до 115 м
    if (!crVerifyRun && ccBegin(fly, pl_, n, pr2, regE_, fn_, pl0_, n0_, us0)) return;   // №653: задание ушло на ME, результат берёт cullCityEnd
    if (cr && (opt & O_PLANES) && cullHierReady()) {   // А3: клетка один раз за кадр, куски вне пирамиды не читаются
      if (++cceEp == 0) { memset(cceE, 0, cceN * sizeof *cceE); cceEp = 1; }
      for (int i = 0; i < n; i++) { unsigned k = pl_[i]; if (k >= npc) continue; unsigned ci = crc[k];
        if (ci != 0xFFFFu && ci < ncell) { int c = cellClassC(ci); if (!c) continue;
          if (!cr[k].pad && boxD2(cr[k].bb) <= pr2) cullPieceIn(k, c == 2);
          else if (cr[k].pad && boxD2(cr[k].bb) <= pr2) cullPiece(k); }
        else if (cr[k].cell == 0xFFFFu && cl[pc[k].r.cell].state == 2 && boxD2(pc[k].r.bb) <= pr2) cullPiece(k); } }
    else
    if (cr) { for (int i = 0; i < n; i++) { unsigned k = pl_[i]; if (k < npc && cr[k].cell != 0xFFFFu && cl[cr[k].cell].state == 2 && boxD2(cr[k].bb) <= pr2) cullPiece(k);
                else if (k < npc && cr[k].cell == 0xFFFFu && cl[pc[k].r.cell].state == 2 && boxD2(pc[k].r.bb) <= pr2) cullPiece(k); } }
    else for (int i = 0; i < n; i++) { unsigned k = pl_[i]; if (k < npc && cl[pc[k].r.cell].state == 2 && boxD2(pc[k].r.bb) <= pr2) cullPiece(k); }
    if (drawDist > pvsRange)                            // за 115 м PVS нет: клетки по пирамиде, куски дальше 115 м (без двойников)
      for (unsigned i = 0; i < ncell; i++) {
        Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
        if (boxD2(c->bb) > drawDist * drawDist || !visBox(c->bb, 0, 0, 0)) continue;
        for (unsigned k = c->first; k < c->first + c->n; k++) if (boxD2(pc[k].r.bb) > pr2) cullPiece(k);
      }
  } else {
    for (unsigned i = 0; i < ncell; i++) {
      Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
      if (opt & O_PLANES) { if (boxD2(c->bb) > drawDist * drawDist || !visBox(c->bb, 0, 0, 0)) continue; }
      else { float dist; if (coneOut(c->cx, c->cy, c->cz, c->rad, &dist)) continue; }
      for (unsigned k = c->first; k < c->first + c->n; k++) cullPiece(k);
    }
  }
  cullFinish(fly, fn_, regE_, pl0_, n0_, us0);
}
// хвост cullCity: сортировка подсчётом, запись для повтора (B8), время отсечения. Зовут cullCity и cullCityEnd (отсечение на ME, №653)
static void cullFinish(int fly, const float *fn_, unsigned regE_, const unsigned short *pl0_, int n0_, unsigned us0) {
  if (crVerifyRun) { cullMg = 0.f; return; }   // №919: сверочное отсечение — свежий список остаётся в tp[0..nd) несортированным, повторённый sp[] не тронут
  // сортировка подсчётом по (прозрачность, текстура): меньше смен текстур, прозрачное после непрозрачного
  memset(cnt2k, 0, sizeof cnt2k);
  for (int i = 0; i < nd; i++) cnt2k[tk[i]]++;
  unsigned s = 0; for (int k = 0; k < 2048; k++) { unsigned c = cnt2k[k]; cnt2k[k] = s; s += c; }
  for (int i = 0; i < nd; i++) { int p = cnt2k[tk[i]]++; sp[p] = tp[i]; sf[p] = tf[i]; scl[p] = tcl[i]; }
  cullMg = 0.f;
  if (!CULL_REUSE_OLD) { crv.e[0] = eyeX; crv.e[1] = eyeY; crv.e[2] = eyeZ; memcpy(crv.f, fn_, sizeof crv.f); crv.dd = drawDist; crv.fly = fly; crv.regEye = regE_; crv.cellSig = cellSigNow(); crv.opt = (unsigned)opt;
    crv.nd = (unsigned)nd; crv.pvs = pl0_; crv.pvsN = n0_; crv.age = 0; crv.valid = 1; }
  rfCullUs += sceKernelGetSystemTimeLow() - us0;
  ccMainSample(sceKernelGetSystemTimeLow() - us0);
}

// ---------- №653: отсечение города на Media Engine (слово Роба, кандидат B) ----------
// Порядок: cullCity доходит до списка PVS и вместо цикла по кускам отдаёт задание ME (ccBegin: снимок состояния клеток, ящик, kick) и выходит;
// главный делает свою работу кадра (gameRespawn, collStreamPrepare), перед сборкой списка GE зовёт cullCityEnd: ответ есть — список из выхода ME
// (порядок и содержание те же; куски без горячей записи досчитывает главный по меткам), ответа нет к сроку CC_DEADLINE_US — cullCity целиком на
// главном (как до №653), кадр дальше не ждёт; CC_MISS_MAX промахов подряд — ME для города выключен до конца уровня. Звук первее: задание только
// в окне между заданиями звука (audMeCityWindow); звук, пришедший за городом, цепляется следом (mebox.h meChainAttach), на главный не уходит.
// Откат: -DME_CITY_OLD=1, файл-флаг mecityoff рядом с EBOOT, «flag mecity 0». Сверка с главным: «flag mecitycmp 1» (строка MECITYCMP)
#include "citycull.h"
#include "me/mebox.h"
#include "fpuguard.h"
_Static_assert(sizeof(CcRec) == sizeof(CullRec) && CC_MAXDRAW == MAXDRAW, "CcRec = CullRec, CC_MAXDRAW = MAXDRAW");
int meSvcScheme(void); int meSvcSubmit(int c, volatile MeBox *ubox, unsigned kind, void *job, unsigned words);
int meSvcPoll(int c, volatile MeBox *ubox, unsigned seq, unsigned *sum);
extern unsigned meSvcDbg[4]; int meSvcHeld(void); int meSvcSlept(void); void meSvcForget(int c);
int audMeCityWindow(unsigned needUs); int fxMeActive(void); void bbOp(const char *op, const char *arg, int sync);
#ifndef ME_CITY_OLD
#define ME_CITY_OLD 0
#endif
#ifndef ME_CITY_ON
#define ME_CITY_ON 0
#endif
#define CC_DEADLINE_US 2500u   // от отдачи до ответа; позже — считает главный
#define CC_MISS_MAX 3          // промахов подряд — ME для города выключен до конца уровня
static CcJob ccJob;
static MeBox ccBox __attribute__((aligned(64)));
static CcCell ccCells[CC_MAXCELL] __attribute__((aligned(64)));
static unsigned char ccState[CC_MAXCELL] __attribute__((aligned(64)));
static int ccFileOff, ccFlagOff, ccCmp, ccOn = ME_CITY_ON;   // магистраль 16.10 (слово дирижёра): отсечение города на ME по умолчанию выкл (ME_CITY_ON=1 при сборке или «flag mecity 1» в туре)   // файл mecityoff / flag mecity 0 / flag mecitycmp 1
static struct {
  int st;                                  // 0 не пробовали, 1 работает, 2 выключен до конца уровня (причина — why)
  char why[24];
  const void *keyCl, *keyCr; unsigned keyN, keyP; int tabOk, needInv;
  unsigned seq, t0, beginUs, emaMe, emaMain; int streak;
  unsigned tried, sent, ok, missed, busy, closed, ovf, bad, window;   // за всё время уровня
  unsigned sOk, sTried, sSaved, sT0;                                    // за 10 с
  unsigned cmpN, cmpBad, mism, rej[8], dq, dsent, ddone, dchain;   // rej: 0 выкл. 1 нет данных/PVS 2 нет службы 3 ролик/частицы 4 размер 5 завис 6 окно звука 7 ME занят
  int pend, fly; unsigned regE; float fn[3]; const unsigned short *pl0; int n0; unsigned us0; float mg, pr2; const unsigned short *pl; int n;
} cc = {0};
static void ccLevelReset(void) { memset(&cc, 0, sizeof cc); cc.emaMe = 2500; cc.emaMain = 1700; }
static void ccMainSample(unsigned us) {   // цена отсечения на главном (кадры, где ME не участвовал) — для «сэкономлено»
  if (ccFromMe) { ccFromMe = 0; return; }
  if (us < 20000u) cc.emaMain = (cc.emaMain * 7u + us) / 8u;
}
static void ccOff(const char *why) { cc.st = 2; snprintf(cc.why, sizeof cc.why, "%s", why); meSvcForget(1); }
static char ccAcct[96] = "off";   // строка ME для учёта ACCT (acct.h): ok/отдано/сэкономлено/отказы
static void ccReport(unsigned now) {   // раз в 10 с: доля кадров, где ME успел, и сэкономленные мкс (чёрный ящик blackbox.txt; в журнал смоука тоже)
  if (!cc.sT0) cc.sT0 = now;
  if (now - cc.sT0 < 10000000u) return;
  char s[200]; snprintf(s, sizeof s, "ok=%u/%u (%u%%) saved_us=%u/frame st=%d%s%s missed=%u busy=%u window=%u ovf=%u bad=%u me_us=%u main_us=%u cmp=%u/%u rej=%u/%u/%u/%u/%u/%u/%u/%u lastq=%d,%u,%u,%u", cc.sOk, cc.sTried, cc.sTried ? cc.sOk * 100 / cc.sTried : 0,
           cc.sOk ? cc.sSaved / cc.sOk : 0, cc.st, cc.st == 2 ? ":" : "", cc.st == 2 ? cc.why : "", cc.missed, cc.busy, cc.closed, cc.ovf, cc.bad, cc.emaMe, cc.emaMain, cc.cmpBad, cc.cmpN, cc.rej[0], cc.rej[1], cc.rej[2], cc.rej[3], cc.rej[4], cc.rej[5], cc.rej[6], cc.rej[7], (int)cc.dq, cc.dsent, cc.ddone, cc.dchain);
  snprintf(ccAcct, sizeof ccAcct, "ok=%u/%u saved_us=%u st=%d miss=%u busy=%u win=%u", cc.sOk, cc.sTried, cc.sOk ? cc.sSaved / cc.sOk : 0, cc.st, cc.missed, cc.busy, cc.closed);
  bbOp("mecity", s, 0);
  if (smkOn) { char l[240]; snprintf(l, sizeof l, "MECITY t=%.1f %s\n", smkT, s); smkWrite(l); }
  cc.sOk = cc.sTried = cc.sSaved = 0; cc.sT0 = now;
}
static int ccBegin(int fly, const unsigned short *pl_, int n, float pr2, unsigned regE_, const float *fn_, const unsigned short *pl0_, int n0_, unsigned us0) {
  ccReport(sceKernelGetSystemTimeLow());
  if (ME_CITY_OLD || !ccOn || ccNoMe || ccFileOff || ccFlagOff || cc.st == 2) { cc.rej[0]++; return 0; }
  if (!(cr && (opt & O_PLANES) && cullHierReady()) || n < 8) { cc.rej[1]++; return 0; }
  int sch = meSvcScheme(); if (sch < 0) { cc.rej[2]++; return 0; }                                  // служба ME не поднята (эмулятор без пустышки, nome)
  unsigned now = sceKernelGetSystemTimeLow();
  if (meSvcSlept()) { ccOff("sleep"); return 0; }
  if (meSvcHeld() || fxMeActive()) { cc.rej[3]++; return 0; }                                       // ролик / ME занят частицами: этот кадр — главный
  if (ncell > CC_MAXCELL || npc > 0xFFFFu) { ccOff("size"); return 0; }
  cc.tried++; cc.sTried++;
  volatile MeBox *ub = (volatile MeBox *)(0x40000000u | (unsigned)&ccBox);        // ящик пишет ME мимо кэша главного
  if (ub->sent != ub->done && cc.sent) {                                       // прошлое задание города ещё у ME (промах уже посчитан)
    cc.busy++; cc.rej[5]++; if (++cc.streak >= CC_MISS_MAX) ccOff("stuck"); return 0; }
  if (!audMeCityWindow(cc.emaMe)) { cc.closed++; cc.rej[6]++; return 0; }                       // звук первее: не в окне между заданиями звука
  if (cc.keyCl != cl || cc.keyCr != cr || cc.keyN != ncell || cc.keyP != npc) {    // таблица клеток и сверка адресов — раз на уровень
    cc.keyCl = cl; cc.keyCr = cr; cc.keyN = ncell; cc.keyP = npc; cc.tabOk = 1;
    for (unsigned i = 0; i < ncell; i++) { memcpy(ccCells[i].bb, cl[i].bb, sizeof ccCells[i].bb); ccCells[i].first = cl[i].first; ccCells[i].n = cl[i].n; }
    sceKernelDcacheWritebackInvalidateRange(ccCells, ncell * sizeof *ccCells);
    if ((sch & ME_MEM_LOW) && (meIsHigh((unsigned)cr + npc * sizeof *cr) || meIsHigh((unsigned)crc + npc * 2) || meIsHigh((unsigned)ccCells + sizeof ccCells) || meIsHigh((unsigned)&ccJob + sizeof ccJob))) {
      cc.tabOk = 0; ccOff("high"); return 0; }
    meSvcForget(1);                                                                 // очередь службы не должна помнить ящик, который сейчас обнулим
    cc.needInv = 1; memset(&ccBox, 0, sizeof ccBox); sceKernelDcacheWritebackInvalidateRange(&ccBox, sizeof ccBox);
  }
  if (!cc.tabOk) return 0;
  if ((sch & ME_MEM_LOW) && (meIsHigh((unsigned)pl_ + (unsigned)n * 2))) { ccOff("high"); return 0; }
  CcJob *J = &ccJob;
  for (unsigned i = 0; i < ncell; i++) { int r = cl[i].state == 2; ccState[i] = (unsigned char)(r | ((r && !cl[i].owner) << 1)); }
  sceKernelDcacheWritebackRange(ccState, (ncell + 63u) & ~63u);
  J->eye[0] = eyeX; J->eye[1] = eyeY; J->eye[2] = eyeZ; J->dd = drawDist; J->mg = cullMg; J->pr2 = pr2; J->lod = LOD_DIST;
  memcpy(J->pl, pl, sizeof J->pl);
  J->fcsr = fpuGet(); J->flags = ((opt & O_FAR) ? CC_F_FAR : 0u) | (drawDist > pvsRange ? CC_F_FARLOOP : 0u);
  J->n = (unsigned)n; J->ncell = ncell; J->npc = npc; J->regEye = regEye;
  J->list = (const unsigned short *)meUserAddr(sch, (unsigned)pl_); J->crc = (const unsigned short *)meUserAddr(sch, (unsigned)crc);
  J->cr = (const CcRec *)meUserAddr(sch, (unsigned)cr); J->cells = (const CcCell *)meUserAddr(sch, (unsigned)ccCells); J->state = (const unsigned char *)meUserAddr(sch, (unsigned)ccState);
  memset(J->inv, 0, sizeof J->inv);
  J->inv[0][0] = meUserAddr(sch, (unsigned)pl_); J->inv[0][1] = (unsigned)n * 2u;
  J->inv[1][0] = meUserAddr(sch, (unsigned)ccState); J->inv[1][1] = ncell;
  if (cc.needInv) { J->inv[2][0] = meUserAddr(sch, (unsigned)cr); J->inv[2][1] = npc * sizeof *cr;
    J->inv[3][0] = meUserAddr(sch, (unsigned)crc); J->inv[3][1] = npc * 2u;
    J->inv[4][0] = meUserAddr(sch, (unsigned)ccCells); J->inv[4][1] = ncell * sizeof *ccCells; }
  sceKernelDcacheWritebackInvalidateRange(J, (unsigned)((char *)&J->nOut - (char *)J));   // вход — в память; выход начинается с новой строки кэша
  int q = meSvcSubmit(1, &ccBox, ME_KIND_CITY, J, sizeof *J);
  if (q < 0) { cc.busy++; cc.rej[7]++; cc.dq = (unsigned)q; cc.dsent = meSvcDbg[0]; cc.ddone = meSvcDbg[1]; cc.dchain = meSvcDbg[2] | (meSvcDbg[3] << 8); return 0; }                                                // ME занят звуком / службе нет: главный считает
  cc.needInv = 0; cc.seq = (unsigned)q; cc.sent++; cc.t0 = sceKernelGetSystemTimeLow();
  cc.fly = fly; cc.regE = regE_; memcpy(cc.fn, fn_, sizeof cc.fn); cc.pl0 = pl0_; cc.n0 = n0_; cc.us0 = us0; cc.mg = cullMg; cc.pr2 = pr2; cc.pl = pl_; cc.n = n;
  cc.beginUs = cc.t0 - now; cc.pend = 1;
  cullMg = 0.f;                                                                      // до End запас нужен только ME (gameRespawn считает visBox без запаса)
  (void)now;
  return 1;
}
static int ccCompareKeep(int fly);
static void cullCityEnd(void) {
  if (!cc.pend) return;
  cc.pend = 0; unsigned e0 = sceKernelGetSystemTimeLow(), sum = 0, w = 0; int got = 0;
  for (;;) {
    if (meSvcPoll(1, &ccBox, cc.seq, &sum)) { got = 1; break; }
    w = sceKernelGetSystemTimeLow() - cc.t0;
    if (w >= CC_DEADLINE_US) break;
  }
  if (got && sum == ME_MIX_BADSIZE) { ccOff("layout"); got = 0; }
  if (got) {
    sceKernelDcacheInvalidateRange((char *)&ccJob.nOut, sizeof ccJob - ((char *)&ccJob.nOut - (char *)&ccJob));   // выход писал ME
    if (sum != 0 || ccJob.ovf) { cc.ovf++; got = 0; }                                // переполнение: главный считает сам, не промах
    else if (ccJob.magic != CC_MAGIC || ccJob.nOut > CC_MAXDRAW) { cc.bad++; got = 0; if (++cc.streak >= CC_MISS_MAX) ccOff("bad"); }
  }
  if (!got) {                                                                       // запасной путь: всё считает главный, кадр дальше не ждёт
    if (w >= CC_DEADLINE_US) { cc.missed++; if (++cc.streak >= CC_MISS_MAX) ccOff("miss"); }
    unsigned lost = sceKernelGetSystemTimeLow() - e0 + cc.beginUs;
    ccNoMe = 1; cullCity(cc.fly); ccNoMe = 0; rfCullUs += lost;
    ccReport(sceKernelGetSystemTimeLow()); return;
  }
  cc.streak = 0; cc.ok++; cc.sOk++;
  cc.emaMe = (cc.emaMe * 7u + (w ? w : 1u)) / 8u;
  cullMg = cc.mg;                                                                   // метки (cullPiece по Piece) считаются с тем же запасом
  nd = 0; regCut = ccJob.regCut;
  for (unsigned i = 0; i < ccJob.nOut; i++) {
    unsigned short t = ccJob.otk[i], k = ccJob.otp[i];
    if (t == CC_MK_CALL) cullPiece(k);
    else if (t == CC_MK_CHECK) { if (k < npc && cl[pc[k].r.cell].state == 2 && boxD2(pc[k].r.bb) <= cc.pr2) cullPiece(k); }
    else { if (nd >= MAXDRAW) break; tk[nd] = t; tp[nd] = k; tf[nd] = ccJob.otf[i]; tcl[nd] = ccJob.otcl[i]; nd++; }
  }
  unsigned mid = sceKernelGetSystemTimeLow();
  (void)mid; ccFromMe = 1;
  cullFinish(cc.fly, cc.fn, cc.regE, cc.pl0, cc.n0, e0 - cc.beginUs);   // время отсечения = отдача + ожидание + слияние
  unsigned spent = sceKernelGetSystemTimeLow() - e0 + cc.beginUs;
  cc.sSaved += cc.emaMain > spent ? cc.emaMain - spent : 0;
  if (ccCmp || (cc.ok & 63u) == 1u) {   // сверка с главным: по флагу — всегда, иначе каждый 64-й кадр с ответом ME; остаётся результат главного.
    if (ccCompareKeep(cc.fly) && !ccCmp) { cc.bad++; if (++cc.mism >= 3) ccOff("mismatch"); } }   // 3 расхождения (редко возможны от загрузки клетки в окне) — ME для города выключен до конца уровня
  ccReport(sceKernelGetSystemTimeLow());
}
// сверка с главным («flag mecitycmp 1», эмулятор с пустышкой ME): результат ME запоминаем, cullCity считаем по-старому и сравниваем; остаётся результат главного
static unsigned short cmpSp[MAXDRAW], cmpScl[MAXDRAW]; static unsigned char cmpSf[MAXDRAW];
static int ccCompareKeep(int fly) {
  int nd0 = nd; memcpy(cmpSp, sp, sizeof cmpSp); memcpy(cmpSf, sf, sizeof cmpSf); memcpy(cmpScl, scl, sizeof cmpScl);
  crv.valid = 0; ccNoMe = 1; cullCity(fly); ccNoMe = 0;
  int bad = nd != nd0;
  for (int i = 0; !bad && i < nd; i++) bad = sp[i] != cmpSp[i] || sf[i] != cmpSf[i] || scl[i] != cmpScl[i];
  cc.cmpN++; if (bad) cc.cmpBad++;
  if (smkOn && bad) { char l[120]; snprintf(l, sizeof l, "MECITYCMP BAD t=%.1f nd_me=%d nd_main=%d\n", smkT, nd0, nd); smkWrite(l); }
  return bad;
}

// ---------- правка 275: дисплей-листы клеток города ----------
// Отсечение — прежнее (cullCity по кускам: PVS, пирамида, регионы); меняется подача: видимая клетка вызывает свой лист целиком
// (все непрозрачные куски клетки — их у клетки в среднем 4; лишние треугольники при видеочипе, занятом на 33 %, дешевле команд
// процессора). У кусков клетки один центр и масштаб (правки швов 570ac3a: 1134 клетки из 1134) — матрица модели одна на клетку,
// в осях камеры, как у прежнего пути. Полупрозрачное — прежней очередью tq по кускам. Лист пишет все состояния сам (кэши
// bound/curFlags/... сброшены перед сборкой), после вызова кэши недействительны — dlLost. «flag dloff» — прежний путь по кускам
#define DL_BUILD_MAX 32       // листов за кадр: остальные клетки этого кадра — прежним путём (без рывка на въезде)
static unsigned farCellPieces;   // правка 508: кусков дальних клеток за кадр (дальняя версия без нарезки), в строку RFAST не входит — для отладки
static int dlOff, dlNoList, dlNoDepth;   // dlnodepth — красный контроль гейта tests/dl/run.py (лист без границ глубины: дома пропадают)
static void dlLost(void) { if (nzOn) { nzCur = -1; nzOffOn = 1; } flatLit = 2; flatAmbOk = 0; bound = -2; texOn = -1; curFlags = -1; curFilt = -1; curLvlMode = -1; curSlope = -1.f; curCull = -1; curBias = 0x7fffffff; }
// part = 0: полный лист клетки; part = 1 (правка «плавность»): лист без кусков из маски skip (бит — номер куска в клетке), в L->p*
// №912: разметка дорог — отдельный лист клетки (треугольники без текстуры, сдвиг глубины слоя, цвет вершин = тон разметки × свет), вызывается после основного листа клетки,
// если клетка ближе MARK_RANGE (м) к глазу; матрица модели та же (оси кусков клетки). Строится по первому вызову, уходит вместе с листами клетки (dlFree)
#ifndef MARK_RANGE_BIG
#define MARK_RANGE_BIG 55.f     // крупная разметка (полосы, зебра, стоп-линии): до этой дальности от глаза (по центру клетки, + полклетки)
#endif
#ifndef MARK_RANGE_SMALL
#define MARK_RANGE_SMALL 30.f   // мелочь (короткие штрихи, буквы): только ближе
#endif
static float markRangeBig = MARK_RANGE_BIG, markRangeSmall = MARK_RANGE_SMALL; static unsigned marksCalls, marksTrisDrawn;
static unsigned marksBuildList(unsigned *b, const MkVert *v, unsigned n) {   // один лист: состояние слоя + один DrawArray; -> размер, байт
  sceKernelDcacheWritebackInvalidateRange(b, 128u * 4u);
  dlLost(); sceGuStart(GU_CALL, b); flatOff(); flatInList = 1;
  if (!dlNoDepth) sceGuDepthRange(65535, 0);
  bindTexLean(-1); setFlags(F_LAYER);
  sceGuDrawArray(GU_TRIANGLES, MK_VT, (int)n, 0, v);
  flatOff(); flatInList = 0;
  unsigned sz = (unsigned)sceGuFinish(); dlLost(); return sz;
}
static void ldSignal(void);
static void marksWake(void) { ldSignal(); }
static void marksCall(unsigned ci, float ex, float ez) {
  marksLoaderOn = loaderUp;
  if (!flatOn || !marksOk || ci >= marksNcell || ci >= ncell) return; MkCell *m = &marksCell[ci];
  if (!m->ready) marksCellLoad2(ci, 1);
  if (!m->ready) return;
  const Cell *c = &cl[ci]; float dx = c->cx - ex, dz = c->cz - ez, d2 = dx * dx + dz * dz, rb = markRangeBig + 10.f, rs = markRangeSmall + 10.f, rf = rs + 30.f;
  int wantA = m->nbig && d2 <= rb * rb, wantS = m->ns && d2 <= rs * rs; CellDL *L = &cdl[ci];
  if (m->sready && d2 > rf * rf) { marksSmallDrop(ci); if (L->mbuf2) { dlBytesNow -= L->mbytes2; marksFreeLater(L->mbuf2); L->mbuf2 = 0; L->mok2 = 0; } }   // мелочь далеко — память назад (список GE не освобождаем под рукой GE)
  if (!wantA && !wantS) return;
  if (wantA) {
    if (!L->mok) {
      unsigned *b = (unsigned *)memalign(16, 128u * 4u); if (!b) return; marksColour(m->v, m->nbig); m->epoch = marksEpoch;
      if (marksBuildList(b, m->v, m->nbig) > 128u * 4u) { smkGeBad++; free(b); return; }
      L->mbuf = b; L->mbytes = 128u * 4u; L->mtrisA = (int)(m->nbig / 3u); L->mok = 1; dlBytesNow += L->mbytes; dlBuilt++; }
    else if (m->epoch != marksEpoch) { L->mok = 0; dlBytesNow -= L->mbytes; marksFreeLater(L->mbuf); L->mbuf = 0; return; }   // тон сменился (marksEpoch): лист пересоберём в следующем кадре
    sceGuCallList(L->mbuf); dlLost(); drawnTris += L->mtrisA; draws++; marksCalls++; marksTrisDrawn += (unsigned)L->mtrisA; }
  if (wantS) {
    if (!m->sready) { marksReqSmall(ci, marksWake); return; }   // мелочь ещё не прочитана: появится через кадр-другой
    if (!L->mok2) {
      unsigned *b = (unsigned *)memalign(16, 128u * 4u); if (!b) return; marksColour(m->sv, m->ns); m->sepoch = marksEpoch;
      if (marksBuildList(b, m->sv, m->ns) > 128u * 4u) { smkGeBad++; free(b); return; }
      L->mbuf2 = b; L->mbytes2 = 128u * 4u; L->mtrisB = (int)(m->ns / 3u); L->mok2 = 1; dlBytesNow += L->mbytes2; dlBuilt++; }
    else if (m->sepoch != marksEpoch) { L->mok2 = 0; dlBytesNow -= L->mbytes2; marksFreeLater(L->mbuf2); L->mbuf2 = 0; return; }
    sceGuCallList(L->mbuf2); dlLost(); drawnTris += L->mtrisB; draws++; marksCalls++; marksTrisDrawn += (unsigned)L->mtrisB; }
}
static int dlBuildM(unsigned ci, unsigned long long skip, int part) {
  Cell *c = &cl[ci]; CellDL *L = &cdl[ci]; unsigned short ord[64]; int n = 0; unsigned words = 16;
  if (part && c->n > 64) return -1;
  for (unsigned k = c->first; k < c->first + c->n && n < 64; k++) {
    const Piece *d = &pc[k];
    if (part && (skip >> (k - c->first) & 1ull)) continue;
    if (d->r.ni <= 0 || ((d->r.flags & 2) && !(d->r.flags & 4))) continue;   // полупрозрачное — очередью tq
    if (!(d->r.flags & F_NEAR)) return -1;                                   // уровень по-старому (дальние версии) — прежний путь
    int j = n++; while (j > 0) { const Piece *e = &pc[ord[j - 1]]; int ke = FLAT_SORTKEY(e), kd = FLAT_SORTKEY(d);
                                 if (ke <= kd) break; ord[j] = ord[j - 1]; j--; }
    ord[j] = (unsigned short)k; words += (d->r.flags & F_STRIP) ? stripDlWords(d, 0) + ((d->r.flags & F_FARP) && d->r.nf > 0 ? stripDlWords(d, 1) : 0) + 76 : 76 + 8 * (d->r.ni / 65532 + 1);   // №949: +4 слова на масштаб шума (2 команды TexScale + возврат)
  }
  if (c->n > 64) return -1;
  if (!n) { if (part) { L->pok = 2; L->pmask = skip; } else L->ok = 2; return 0; }
  unsigned bytes = words * 4; unsigned *b = (unsigned *)memalign(16, bytes); if (!b) return -1;
  sceKernelDcacheWritebackInvalidateRange(b, bytes);   // лист пишется мимо кэша (sceGuStart): грязные строки не должны лечь поверх
  dlLost(); sceGuStart(GU_CALL, b); flatOff(); unsigned nb0_ = binds + flatBinds, nn0_ = flatNoiseN; flatInList = 1;
  if (!dlNoDepth) sceGuDepthRange(65535, 0);   // у контекста листа свои границы глубины (нули): sceGuDepthOffset слоёв (setBias) считает от них — как у кадра
  int tris = 0, dr = 0;
  for (int i = 0; i < n; i++) { Piece *d = &pc[ord[i]]; if (!pieceOk(d, 0)) continue;
    phAllow = d->r.owner == 0; NZP(d); bindTexLean(d->r.tex); phAllow = 0; setFlags(d->r.flags); drawGeo(d, 0, d->r.ni); tris += d->r.ni / 3; dr++; }
  if (phTfx) { sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); phTfx = 0; }   // правка 411: лист кончается обычной модуляцией
  flatOff(); flatInList = 0;
  unsigned sz = (unsigned)sceGuFinish(); dlLost();
  if (sz > bytes) { smkGeBad++; free(b); return -1; }   // не должно быть: запас на кусок 72 слова
  if (part) { L->pnoise = flatNoiseN != nn0_; L->pnb = (unsigned short)(binds + flatBinds - nb0_); L->pbuf = b; L->pbytes = bytes; L->ptris = tris; L->pdraws = dr; L->pmask = skip; L->pok = 1; dlBuilt++; dlBytesNow += bytes; return 1; }
  L->noise = flatNoiseN != nn0_; L->nb = (unsigned short)(binds + flatBinds - nb0_); L->buf = b; L->bytes = bytes; L->tris = tris; L->draws = dr; L->ok = 1; dlBuilt++; dlBytesNow += bytes;
  return 1;
}
static int dlBuild(unsigned ci) { return dlBuildM(ci, 0, 0); }
// TEST23 (слово Роба «даль 200 м должна быть дешёвой»; разбор TEST21: дальние клетки шли кусками на CPU, drawpiece ×2): лист ДАЛЬНЕЙ версии клетки — все непрозрачные куски клетки,
// у кусков с флагом F_FARP (есть дальняя версия без нарезки, nf > 0) — дальние вершины, у остальных ближние; одна матрица клетки (как у листа ближних). Строится по требованию (в тот же
// бюджет DL_BUILD_MAX), не собрался — клетка идёт прежним путём по кускам. FAR_PLAIN_LIST_OLD=1 — прежнее
#ifndef FAR_PLAIN_LIST_OLD
#define FAR_PLAIN_LIST_OLD 0
#endif
static int dlBuildFar(unsigned ci) {
  Cell *c = &cl[ci]; CellDL *L = &cdl[ci]; unsigned short ord[64]; int n = 0; unsigned words = 16;
  if (c->n > 64) return -1;
  for (unsigned k = c->first; k < c->first + c->n && n < 64; k++) {
    const Piece *d = &pc[k];
    if (d->r.ni <= 0 || ((d->r.flags & 2) && !(d->r.flags & 4))) continue;   // полупрозрачное — очередью tq
    int j = n++; while (j > 0) { const Piece *e = &pc[ord[j - 1]]; int ke = FLAT_SORTKEY(e), kd = FLAT_SORTKEY(d);
                                 if (ke <= kd) break; ord[j] = ord[j - 1]; j--; }
    ord[j] = (unsigned short)k; words += (d->r.flags & F_STRIP) ? stripDlWords(d, 0) + ((d->r.flags & F_FARP) && d->r.nf > 0 ? stripDlWords(d, 1) : 0) + 76 : 76 + 8 * (d->r.ni / 65532 + 1);   // №949: +4 слова на масштаб шума (2 команды TexScale + возврат)
  }
  if (!n) { L->fok = 2; return 0; }
  unsigned bytes = words * 4; unsigned *b = (unsigned *)memalign(16, bytes); if (!b) return -1;
  sceKernelDcacheWritebackInvalidateRange(b, bytes);
  dlLost(); sceGuStart(GU_CALL, b); flatOff(); unsigned nb0_ = binds + flatBinds, nn0_ = flatNoiseN; flatInList = 1;
  if (!dlNoDepth) sceGuDepthRange(65535, 0);
  int tris = 0, dr = 0;
  for (int i = 0; i < n; i++) { Piece *d = &pc[ord[i]]; int fp = (d->r.flags & F_FARP) != 0 && d->r.nf > 0; if (!pieceOk(d, fp)) continue;
    phAllow = d->r.owner == 0; NZP(d); bindTexLean(d->r.tex); phAllow = 0; setFlags(d->r.flags); int cnt = fp ? d->r.nf : d->r.ni; drawGeo(d, fp, cnt); tris += cnt / 3; dr++; }
  if (phTfx) { sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); phTfx = 0; }
  flatOff(); flatInList = 0;
  unsigned sz = (unsigned)sceGuFinish(); dlLost();
  if (sz > bytes) { smkGeBad++; free(b); return -1; }
  L->fnoise = flatNoiseN != nn0_; L->fnb = (unsigned short)(binds + flatBinds - nb0_); L->fbuf = b; L->fbytes = bytes; L->ftris = tris; L->fdraws = dr; L->fok = 1; dlBuilt++; dlBytesNow += bytes;
  return 1;
}
static unsigned farListCalls;   // TEST23: вызовов листа дальней клетки за кадр (строка RFAST? нет — отладка, сбрасывается в dlCity)
// Куски клетки под риском среза у камеры (те же условия, что в ggCellCut и в начале drawGeoG), маска по номерам в клетке
#ifndef GG_FOOT_CUT_OLD
#define GG_FOOT_CUT_OLD 0
#endif
static unsigned long long ggCellMask(unsigned ci) {
  const Cell *c = &cl[ci]; unsigned long long m = 0;
  for (unsigned k = c->first; k < c->first + c->n && k - c->first < 64; k++) { const Piece *e = &pc[k];
    if (e->r.ni <= 0 || ((e->r.flags & 2) && !(e->r.flags & 4))) continue;
    if (e->r.owner == 0 && (e->r.flags & F_NOIDX) && e->r.ni <= 65535 && GG_PIECE_OK(e) && ggBoxRiskW(e->r.bb)) m |= 1ull << (k - c->first); }
  return m;
}
#include "propbake.h"
#include "proptoon.h"   // №817: контур пропов и стейтпропов (до тел пропов)
static void pbResetFwd(void) { pbReset(); }
static void tvRebuild(void) { for (unsigned i_ = 0; cdl && i_ < ncell; i_++) dlFree(i_); ggcClear(); pbReset(); resetState(); }
static int tvCmp(const void *a, const void *b) { float x = ((const float *)a)[1], y = ((const float *)b)[1]; return x < y ? 1 : x > y ? -1 : 0; }
static unsigned tvTexBytes(const Tex *x) {   // объём уровней как в загрузчике (main_levels.h), по 16 Б
  unsigned n = 0; int lw = x->w, lh = x->h;
  for (int l = 0; l < x->levels; l++) { n += (unsigned)texRowBytes(x->t4, 1, lw) * (unsigned)((lh + 7) & ~7); n = (n + 15) & ~15u; lw >>= 1; lh = lh > 1 ? lh >> 1 : 1; }
  return n;
}
static void tvMigrate(void) {
  tvDone = 1; tvMovedN = tvMovedB = 0;
  if (tvEnd <= tvBase || !tex || !ntex) { if (smkOn) smkWrite("TEXVRAM none: no free VRAM\n"); return; }
  unsigned budget = tvEnd - tvBase; if ((unsigned)tvKB * 1024u < budget) budget = (unsigned)tvKB * 1024u;
  float *c = (float *)malloc(ntex * 2 * sizeof(float)); if (!c) return;
  unsigned nc = 0, candB = 0;
  for (unsigned t = 0; t < ntex && t < 4096; t++) { const Tex *x = &tex[t];
    if (!x->pre || x->levels < 1 || !x->slv[0] || tvUse[t] < TV_MIN) continue;
    unsigned b = tvTexBytes(x); if (!b) continue;
    c[nc * 2] = (float)t; c[nc * 2 + 1] = (float)tvUse[t] / (float)b; nc++; candB += b; }
  qsort(c, nc, 2 * sizeof(float), tvCmp);
  unsigned used = 0; uintptr_t vr = (uintptr_t)sceGeEdramGetAddr();
  for (unsigned k = 0; k < nc; k++) { Tex *x = &tex[(unsigned)c[k * 2]]; unsigned b = tvTexBytes(x);
    if (used + b > budget) continue;
    int lw = x->w, lh = x->h; unsigned at = used;
    for (int l = 0; l < x->levels; l++) { unsigned n = (unsigned)texRowBytes(x->t4, 1, lw) * (unsigned)((lh + 7) & ~7);
      memcpy((void *)((vr + tvBase + at) | 0x40000000u), x->lv[l], n);   // через некэшируемый образ, как HUD (ui.c uiUseVram)
      x->slv[l] = (void *)(vr + tvBase + at); at = (at + n + 15) & ~15u; lw >>= 1; lh = lh > 1 ? lh >> 1 : 1; }
    used += b; tvMovedN++; }
  tvMovedB = used; tvCand = nc; free(c);
  tvRebuild();
  if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "TEXVRAM moved=%u of=%u kb=%u budget_kb=%u free_kb=%u cand_kb=%u t=%.1f\n", tvMovedN, nc, used / 1024, budget / 1024, (tvEnd - tvBase) / 1024, candB / 1024, smkT); smkWrite(l_); }
}
static void tvRestore(void) {   // «flag texvram 0»: адреса для GE снова в ОЗУ
  if (!tex) return;
  for (unsigned t = 0; t < ntex; t++) { Tex *x = &tex[t]; if (!x->pre) continue; for (int l = 0; l < x->levels; l++) x->slv[l] = x->lv[l]; }
  tvMovedN = tvMovedB = 0; tvDone = 0; tvFrames = 0; memset(tvUse, 0, sizeof tvUse); tvRebuild();
}
// Гибрид «лист клетки или нужные куски» (план плавности, 11.10, ночная оценка кадра PSP: город 60–70 тыс. треугольников в районе x −400…−500 L2/L5, 80 тыс. в худшем курсе): клетка
// вызывает лист ВСЕХ своих непрозрачных кусков, даже если из пирамиды и из PVS глаза нужен один (ночная карта: лист клетки целиком 33,8 тыс. треугольников в среднем против 22,9 тыс.
// нужных кусков, то есть 32 % лишнего; худший курс 49,4 против 34,5). Цена кадра PSP ≈0,32 мс на тысячу треугольников (по турам 9D/11A/11B), вызов листа ≈5,5 мкс, собственно
// кусок прежним путём ≈17 мкс (sp_loop). Клетка, у которой нужных кусков мало, а лишнее велико, рисуется нужными кусками: (нужные треугольники × C + число кусков × P) < (все треугольники
// клетки × C + L). Вид тот же: рисуются те же куски с теми же матрицами (клетки без листа и так идут кусками). -DCELL_HYB_OLD=1 — как до гибрида; C, P, L — макросы CELL_HYB_C/P/L
#ifndef CELL_HYB_OLD
#define CELL_HYB_OLD BIS(1)
#endif
#ifndef CELL_HYB_C
#define CELL_HYB_C 0.32f
#endif
#ifndef CELL_HYB_P
#define CELL_HYB_P 17.f
#endif
#ifndef CELL_HYB_L
#define CELL_HYB_L 5.5f
#endif
static unsigned *chEp, *chNeedT, *chNeedN, *chCellT; static unsigned char *chMode; static unsigned chN, chEpoch, rfHybCells, rfHybSaved;
// 17.10 (терминал рендера): «flag cellhyb 0» — гибрид выключен на ходу (все клетки листом) для замера на PSP: в езде видеочип ждёт 0,1 мс, процессор упирается,
// а гибрид меняет треугольники видеочипа на куски процессора (≈17 мкс на кусок). По умолчанию как при сборке
static int cellHybOn = 0;   // 17.10 замер PSP spd1017: гибрид выключен (город −0,7…−1,8 мс за кадр, рампа F12-6 работа 33,0 → 30,1 мс); «flag cellhyb 1» — включить
static void cellHybFree(void) { free(chEp); free(chNeedT); free(chNeedN); free(chCellT); free(chMode); chEp = chNeedT = chNeedN = chCellT = 0; chMode = 0; chN = 0; }
static void cellHybPrep(void) {
  if (!cellHybOn || CELL_HYB_OLD || !ncell) return;
  if (chN != ncell) { cellHybFree(); chEp = (unsigned *)calloc(ncell, sizeof(unsigned)); chNeedT = (unsigned *)calloc(ncell, sizeof(unsigned)); chNeedN = (unsigned *)calloc(ncell, sizeof(unsigned));
    chCellT = (unsigned *)calloc(ncell, sizeof(unsigned)); chMode = (unsigned char *)calloc(ncell, 1); if (!chEp || !chNeedT || !chNeedN || !chCellT || !chMode) { cellHybFree(); return; } chN = ncell; chEpoch = 0; }
  if (++chEpoch == 0) { memset(chEp, 0, chN * sizeof(unsigned)); chEpoch = 1; }
  for (int i = 0; i < nd; i++) { unsigned ci = scl[i]; if (ci >= ncell) continue; const Piece *d = &pc[sp[i]]; if ((d->r.flags & 2) && !(d->r.flags & 4)) continue;
    if (chEp[ci] != chEpoch) { chEp[ci] = chEpoch; chNeedT[ci] = chNeedN[ci] = 0; chMode[ci] = 0; }
    chNeedT[ci] += (unsigned)d->r.ni / 3u; chNeedN[ci]++; }
}
// 2 — клетка рисуется нужными кусками, 1 — листом
static int cellHybMode(unsigned ci, float ex, float ey, float ez) {
  if (!cellHybOn || CELL_HYB_OLD || !chN || ci >= chN || chEp[ci] != chEpoch) return 1;
  if (chMode[ci]) return chMode[ci];
  int m = 1; const Cell *c = &cl[ci];
  if (!dlNoList && c->n <= 64 && !(ggOn && GG_CELL_OK(ci) && ggBoxRiskW(c->bb))) {
    if (!chCellT[ci]) { unsigned t = 1; for (unsigned k = c->first; k < c->first + c->n; k++) { const Piece *e = &pc[k]; if (e->r.ni > 0 && !((e->r.flags & 2) && !(e->r.flags & 4))) t += (unsigned)e->r.ni / 3u; } chCellT[ci] = t; }
    float costList = chCellT[ci] * CELL_HYB_C + CELL_HYB_L, costPc = chNeedT[ci] * CELL_HYB_C + CELL_HYB_P * chNeedN[ci];   // мкс (C — мкс на треугольник)
    if (costPc < costList) { m = 2; rfHybCells++; rfHybSaved += chCellT[ci] > chNeedT[ci] ? chCellT[ci] - chNeedT[ci] : 0; } }
  (void)ex; (void)ey; (void)ez; chMode[ci] = (unsigned char)m; return m;
}
static void cellHybFreeFwd(void) { cellHybFree(); }
static unsigned long long ggCellMaskC(unsigned ci) {   // маска клетки: при стоящей камере — из прошлого кадра
  if (BOOM_CITY_OLD || ggCamStill < 1) return ggCellMask(ci);
  CellDL *P = &cdl[ci]; if (P->mkEpoch == ggCamEpoch) return P->mkVal;
  unsigned long long mk = ggCellMask(ci); P->mkEpoch = ggCamEpoch; P->mkVal = mk; return mk;
}
// Правка Роба 509 («пусть за щелью не будет контрастного света: под тёмным тёмное, под светлым светлое»): под каждой видимой клеткой города — квадрат цвета
// её средней земли (tools/psp_export.py cell_underlay, Cell.pad), ниже самой низкой земли квадрата 40 м. Щель в земле или стыке куска показывает цвет
// соседней земли, а не небо и не единый тон №437. Рисуется до города в осях камеры (вид без переноса), с записью глубины (небо ложится позади), туман как у города.
// Цена: 2 треугольника на видимую клетку, один вызов. -DCELL_UNDERLAY_OLD=1 — без подложки
#ifndef CELL_UNDERLAY_OLD
#define CELL_UNDERLAY_OLD 0
#endif
#include "horizon.h"
#include "hlod.h"   // горизонт: плоские силуэты дальнего города (flag horizon)
#include "silh.h"   // силуэты за дальностью (Загрузка 3)
#include "pano917.h"   // №917: кольцо-панорама дали за 150 м (pano917.bin уровня)
#include "road934.h"   // №934: дальние версии кусков дорог и мостов до 520 м (файл road934.shr уровня)
static unsigned cuDrawn;   // клеток подложки в последнем кадре (отладка)
static void cellUnderlayDraw(float ex, float ey, float ez) {
  cuDrawn = 0;
  if (CELL_UNDERLAY_OLD || nd <= 0 || clearChk) return;   // clearchk (замер-тур): щели меряются пурпуром — подложка их закрыла бы
  static unsigned short *mark; static unsigned markN; static unsigned short epoch;
  if (markN < ncell) { free(mark); mark = calloc(ncell ? ncell : 1, 2); markN = mark ? ncell : 0; epoch = 0; if (!mark) return; }
  if (++epoch == 0) { memset(mark, 0, markN * 2); epoch = 1; }
  enum { MAXQ = 192 };
  unsigned ids[MAXQ]; int nc = 0;
  for (int i = 0; i < nd && nc < MAXQ; i++) {
    unsigned ci = pc[sp[i]].r.cell; if (ci >= ncell || mark[ci] == epoch) continue; mark[ci] = epoch;
    if (!(cl[ci].pad >> 16) || cl[ci].owner || cl[ci].state != 2) continue;
    ids[nc++] = ci;
  }
  if (!nc || geOver(GE_SOFT)) return;
  typedef struct { unsigned c; float x, y, z; } UV;
  UV *v = (UV *)sceGuGetMemory(nc * 6 * sizeof(UV)); int n = 0;
  for (int k = 0; k < nc; k++) {
    const Cell *c = &cl[ids[k]]; unsigned pad = c->pad, c565 = pad >> 16;
    unsigned r = (c565 >> 11 & 31) * 255 / 31, g = (c565 >> 5 & 63) * 255 / 63, b = (c565 & 31) * 255 / 31, col = 0xff000000u | b << 16 | g << 8 | r;
    float cx = (c->bb[0] + c->bb[3]) * .5f, cz = (c->bb[2] + c->bb[5]) * .5f;
    float gx = floorf(cx / 40.f) * 40.f, gz = floorf(cz / 40.f) * 40.f;   // квадрат 40 м, в котором лежит центр ящика клетки (tools/psp_export.py CHUNK)
    float x0 = fmaxf(c->bb[0], gx), x1 = fminf(c->bb[3], gx + 40.f), z0 = fmaxf(c->bb[2], gz), z1 = fminf(c->bb[5], gz + 40.f);
    if (x1 <= x0 || z1 <= z0) continue;
    float y = c->bb[1] - (float)(pad >> 8 & 255) * .25f;
    float X0 = x0 - ex, X1 = x1 - ex, Z0 = z0 - ez, Z1 = z1 - ez, Y = y - ey;
    v[n++] = (UV){col, X0, Y, Z0}; v[n++] = (UV){col, X1, Y, Z0}; v[n++] = (UV){col, X1, Y, Z1};
    v[n++] = (UV){col, X0, Y, Z0}; v[n++] = (UV){col, X1, Y, Z1}; v[n++] = (UV){col, X0, Y, Z1};
    cuDrawn++;
  }
  if (!n) return;
  ScePspFMatrix4 id; memset(&id, 0, sizeof id); id.x.x = id.y.y = id.z.z = id.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &id);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, v);
  resetState();
}
// Прогрев под брифингом (№538, тур 13: первые секунды после снятия ириса — кадры 33–53 мс, листы клеток и первые запекания B1 строятся уже на глазах у игрока). Пока мир стоит и не рисуется
// (BF_HIDE / BF_BLIT, jitter.h), каждый кадр: до BRIEF_WARM_BUILDS листов целых клеток (те, что dlCity выбрал бы листом: загружены, не полупрозрачные, не дальние куски,
// не гибрид «только нужные куски») за не больше BRIEF_WARM_US мкс, и запекание B1 (pbFrameSet: у стоящего мира подписи не меняются, PB_STILL кадров хватает за полсекунды).
// BRIEF_WARM_OLD=1 (бит 11 файла bisect) — как до правки
#ifndef BRIEF_WARM_OLD
#define BRIEF_WARM_OLD BIS(11)
#endif
#define BRIEF_WARM_BUILDS 4
#define BRIEF_WARM_US 2500u
static unsigned rfWarmBuilt, rfWarmBaked;
static void briefWarm(float ex, float ey, float ez, int inR_) {
  if (BRIEF_WARM_OLD || inR_ || dlNoList) return;
  unsigned t0 = sceKernelGetSystemTimeLow(); int b = 0; cellHybPrep();
  for (int k = 0; k < nd && b < BRIEF_WARM_BUILDS; k++) {
    const Piece *d = &pc[tp[k]]; unsigned ci = d->r.cell;
    if (ci >= ncell || cl[ci].state != 2 || cdl[ci].ok) continue;
    if ((d->r.flags & 2) && !(d->r.flags & 4)) continue;
    if (farPlainD > 0.f && boxD2(cl[ci].bb) > farPlainD * farPlainD) continue;
    if (cellHybOn && !CELL_HYB_OLD && cellHybMode(ci, ex, ey, ez) == 2) continue;
    if (dlBuild(ci) >= 0) { b++; rfWarmBuilt++; }
    if (sceKernelGetSystemTimeLow() - t0 > BRIEF_WARM_US) break;
  }
  if (smkOn) { int left = 0; for (int k = 0; k < nd; k++) { const Piece *d = &pc[tp[k]]; unsigned ci = d->r.cell; if (ci < ncell && cl[ci].state == 2 && !cdl[ci].ok && !((d->r.flags & 2) && !(d->r.flags & 4))) left++; }
    char l_[96]; snprintf(l_, sizeof l_, "BWARM nd=%d left=%d b=%d us=%u t=%.2f\n", nd, left, b, sceKernelGetSystemTimeLow() - t0, smkT); smkWrite(l_); }
  if (!PROP_BAKE_OLD && (haveProps || haveSP)) {
    unsigned before = pbBuilt; pbEnsure(); int bl_[2] = {0, 0}; float md_ = drawDist * .92f + 4.f;
    pbFrameSet(&pbP, ex, ey, ez, md_, bl_); pbFrameSet(&pbS, ex, ey, ez, md_, bl_); rfWarmBaked += pbBuilt - before;
  }
}
// flag bisect HEX: та же маска, что файл bisect (bisect.h), на ходу; то, что зависит от выключателей, перестраивается (запекание B1, гибрид, повтор отсечения, дальность)
static void bisectSet(unsigned m) {
  unsigned ch = m ^ bisOld; bisOld = m;
  if (ch & (1u << 0)) { pbReset(); }                          // B1: наборы запекания заново (или сняты)
  if (ch & (1u << 1)) { cellHybFree(); }                      // гибрид листов клеток
  if (ch & (1u << 3)) { crv.valid = 0; }                      // повтор отсечения
  if (ch & ((1u << 2) | (1u << 8) | (1u << 10))) { ddScale = 1.f; }   // регулятор дальности: сначала 250 м
  if (ch & (1u << 7)) { pv2Cur = pv2CurCell = -1; }           // сектор PVS заново
  { char l[64]; snprintf(l, sizeof l, "BISECT mask=%x changed=%x t=%.1f\n", m, ch, smkT); smkWrite(l); }
}
// Сколько клеток вокруг спавна ещё не собрано в листы (те же условия, что у loadWarmStep): очередь прогрева. Нужна loadEnd — газета держится, пока очередь не пуста (LOAD_UNTIL_CELLS)
static int loadWarmPending(void) {
  if (LOAD_WARM_OLD || dlNoList || !cdl || !ncell || !ldWarmOn) return 0;
  const float *c = ldWarmC; int n = 0;
  for (unsigned i = 0; i < ncell; i++) {
    const Cell *k = &cl[i]; if (k->owner || k->state != 2 || cdl[i].ok || k->n > 64) continue;
    float dx = (k->bb[0] + k->bb[3]) * .5f - c[0], dz = (k->bb[2] + k->bb[5]) * .5f - c[2];
    if (dx * dx + dz * dz < 160.f * 160.f) n++;
  }
  return n;
}
static void loadWarmStep(void) {
  if (LOAD_WARM_OLD || dlNoList || !cdl || !ncell || !ldWarmOn) return;
  const float *c = ldWarmC; unsigned t0 = sceKernelGetSystemTimeLow(); int b = 0; int ggSave = ggOn; ggOn = 0;   // без среза у камеры: целые листы
  for (int it = 0; it < 4 && b < 4; it++) {
    int best = -1; float bd = 160.f * 160.f;
    for (unsigned i = 0; i < ncell; i++) {
      const Cell *k = &cl[i]; if (k->owner || k->state != 2 || cdl[i].ok || k->n > 64) continue;
      float dx = (k->bb[0] + k->bb[3]) * .5f - c[0], dz = (k->bb[2] + k->bb[5]) * .5f - c[2], d2 = dx * dx + dz * dz;
      if (d2 < bd) { bd = d2; best = (int)i; } }
    if (best < 0) break;
    if (dlBuild((unsigned)best) < 0) cdl[best].ok = 3;   // не собрался (память): не пробуем снова в этом проходе
    else { b++; rfLoadWarmBuilt++; }
    if (sceKernelGetSystemTimeLow() - t0 > 2500u) break; }
  ggOn = ggSave;
  if (!PROP_BAKE_OLD && (haveProps || haveSP) && sceKernelGetSystemTimeLow() - t0 < 4000u) {   // B1: четыре стороны по очереди, подписи созревают за PB_STILL вызовов
    float hh = (ldWarmStep_++ & 3) * 1.5707963f, f[3] = {sinf(hh), -.12f, cosf(hh)}, e[3] = {c[0], c[1] + 3.5f, c[2]};
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); f[0] /= fl; f[1] /= fl; f[2] /= fl;
    setupPlanes(f[0], f[1], f[2]);
    unsigned before = pbBuilt; pbEnsure(); int bl_[2] = {0, 0}; float md_ = drawDist * .92f + 4.f;
    pbFrameSet(&pbP, e[0], e[1], e[2], md_, bl_); pbFrameSet(&pbS, e[0], e[1], e[2], md_, bl_); rfLoadWarmBaked += pbBuilt - before; }
}
// TEST24 (№637, диагностика): кто в кадре города остаётся кусками (drawPiece), а не листом — счётчик и время (мкс) по причинам, только при смоуке; строка DCITY раз в секунду
static unsigned dcN[10], dcUs[10];
#define DPC(cat, stmt) do { unsigned t_ = smkOn ? sceKernelGetSystemTimeLow() : 0; stmt; if (smkOn) dcUs[cat] += sceKernelGetSystemTimeLow() - t_; dcN[cat]++; } while (0)
// №696: dfN здесь был ТЕМ ЖЕ static, что очередь отложенного освобождения в main_state.h (одна единица трансляции: два «static unsigned dfN» — одна переменная).
// Раз в секунду смоук-строка DLBUILD обнуляла dfN, и указатели msnart, стоявшие в очереди (до 1,2 МБ за уровень), терялись без free (утечка ~1,5 МБ за смену уровня);
// dfN++ счётчика листов, наоборот, добавлял в очередь мусорные указатели. Теперь счётчик свой — dlFarN; LVLEAK696_OLD=1 (или «flag lvleak696old 1») — прежняя общая переменная
#ifndef LVLEAK696_OLD
#define LVLEAK696_OLD 0
#endif
static int lvLeak696Old = LVLEAK696_OLD;
static unsigned dlFarN;
#define DLFARN (*(lvLeak696Old ? &dfN : &dlFarN))
static unsigned dfUs, dfFrame, dfFrameMax, dfFrames, dfT0, dnN, dnUs;   // аудит FPS: листы дальних/ближних клеток, собранные за секунду (число, мкс, максимум за кадр)
static int dlBuildFarT(unsigned ci) { unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int r = dlBuildFar(ci); if (smkOn) { DLFARN++; dfFrame++; dfUs += sceKernelGetSystemTimeLow() - t0; } return r; }
static int dlBuildT(unsigned ci) { unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int r = dlBuild(ci); if (smkOn) { dnN++; dnUs += sceKernelGetSystemTimeLow() - t0; } return r; }
// №932б (Роб: «фоны вдалеке мигают»; на L3/L5 причина — 16-битная глубина: дальние слои города 100–250 м делят между собой ≈ 270 ступеней, и слои с общим сдвигом глубины мерцают).
// Две области глубины: клетки дальше farPlainD (их рисует дальняя версия без нарезки) идут первым проходом со своей проекцией (ближняя плоскость 0,85·farPlainD, дальняя прежняя) —
// на те же 16 бит приходится не 0,7…260 м, а 85…260 м, ступеней глубины в ≈ 7 раз больше. Дальнее полупрозрачное из очереди tq — туда же (проекция та же); затем глубина очищается
// и всё остальное (ближние клетки, предметы, машины, люди, эффекты) — прежней проекцией: ближнее не теряет ничего. ZF932_OLD=1 или «flag zf932 0» — как раньше (один проход).
// Цена: очистка глубины и вторая матрица проекции за кадр. Небо рисуется первым без теста глубины (skyLast не трогаем): очистка его не задевает.
#ifndef ZF932_OLD
#define ZF932_OLD 0
#endif
#ifndef ZF932_NEAR_K
#define ZF932_NEAR_K .85f
#endif
static int zfUser = !ZF932_OLD; static float zfFov, zfNear;   // zfFov — угол кадра города (0 — не город: комната, служебные окна), zfNear — ближняя плоскость основного прохода
static ScePspFMatrix4 zfPmBase; static int zfCurSave; static unsigned zfFrames, zfTqFar;
static int zfBegin(void) { return zfUser && !ZF932_OLD && zfFov > 0.f && farPlainD >= 60.f && drawDist + 10.f > farPlainD * 1.3f ? 2 : 1; }
static int zfFarPiece(const Piece *d) { unsigned ci = d->r.cell; return ci < ncell && boxD2(cl[ci].bb) > farPlainD * farPlainD; }
static void zfPassStart(void) {
  ScePspFMatrix4 pm; zfCurSave = jitCur;
  sceGumMatrixMode(GU_PROJECTION); sceGumStoreMatrix(&zfPmBase); sceGumPushMatrix(); sceGumLoadIdentity();
  sceGumPerspective(zfFov, ASPECT, farPlainD * ZF932_NEAR_K, drawDist + 10.f); sceGumStoreMatrix(&pm); sceGumPopMatrix();
  if (jitActive) { pm.z.x += jitPmJ.z.x - jitPm0.z.x; pm.z.y += jitPmJ.z.y - jitPm0.z.y; }   // тот же сдвиг ¼ px, что у дальних клеток основного прохода
  sceGuSetMatrix(GU_PROJECTION, &pm); zfHold = 1; zfFrames++;
}
static void zfPassEnd(void) {
  dlLost(); resetState();
  tqPart = 1; tqPartD2 = farPlainD * farPlainD; tqFlush(); tqPart = 0; resetState();   // дальнее полупрозрачное — этой же проекцией, до очистки глубины
  sceGuDepthMask(GU_FALSE); sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuClearDepth(0); sceGuClear(GU_DEPTH_BUFFER_BIT); sceGuDepthRange(65535, 0);
  zfHold = 0; if (smkOn && (zfFrames % 120) == 1) { char l_[120]; snprintf(l_, sizeof l_, "ZF932 t=%.1f frames=%u far_d=%.0f near=%.2f\n", smkT, zfFrames, farPlainD, farPlainD * ZF932_NEAR_K); smkWrite(l_); }
  if (jitActive) { sceGuSetMatrix(GU_PROJECTION, zfCurSave ? &jitPmJ : &jitPm0); jitCur = zfCurSave; } else sceGuSetMatrix(GU_PROJECTION, &zfPmBase);
  sceGumMatrixMode(GU_MODEL);
}
static void dlCity(float ex, float ey, float ez) {
  unsigned dcT0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
  if (smkOn) { if (dfFrame > dfFrameMax) dfFrameMax = dfFrame; dfFrame = 0; dfFrames++; unsigned n_ = sceKernelGetSystemTimeLow(); if (!dfT0) dfT0 = n_;
    if (n_ - dfT0 >= 1000000u) { char l_[160]; snprintf(l_, sizeof l_, "DLBUILD t=%.1f frames=%u far_n=%u far_us=%u far_max_frame=%u near_n=%u near_us=%u\n", smkT, dfFrames, DLFARN, dfUs, dfFrameMax, dnN, dnUs); smkWrite(l_); dfT0 = n_; DLFARN = dfUs = dfFrameMax = dfFrames = dnN = dnUs = 0; } }
  if (++cdlEpoch == 0) { memset(cdlMark, 0, ncell * sizeof *cdlMark); cdlEpoch = 1; }
  int builds = 0; dlCallsFrame = dlFallFrame = 0; unsigned dcT1_ = 0;
  cellHybPrep();
  if (!CAM_CLIP_CITY_OLD) for (int i = 0; i < nd; i++) { Piece *d_ = &pc[sp[i]]; unsigned c_ = d_->r.cell; if (c_ < ncell && camClipNear(d_)) cdlMark[c_] = cdlEpoch | 0x80000000u; }   // №634: клетка с куском у камеры — кусками, с отсечением
  if (smkOn && camClipLog) { camClipLog = 0;   // служебно (flag camclipdump): один раз выписать куски в 4 м от глаза — текстура, флаги, ящик (разбор «голых» стен)
    for (int i = 0; i < nd; i++) { const Piece *d_ = &pc[sp[i]]; float dx = fmaxf(fmaxf(d_->r.bb[0] - eyeX, eyeX - d_->r.bb[3]), 0.f), dy = fmaxf(fmaxf(d_->r.bb[1] - eyeY, eyeY - d_->r.bb[4]), 0.f), dz = fmaxf(fmaxf(d_->r.bb[2] - eyeZ, eyeZ - d_->r.bb[5]), 0.f);
      if (dx * dx + dy * dy + dz * dz > 16.f) continue; char l_[280]; snprintf(l_, sizeof l_, "CAMDUMP piece=%u cell=%u tex=%d lv=%d slope=%.3f gnd=%d wh=%dx%d flags=%u far=%d box=%.1f,%.1f,%.1f..%.1f,%.1f,%.1f d=%.2f\n", (unsigned)sp[i], (unsigned)d_->r.cell, (int)d_->r.tex, texAt(d_->r.tex) ? texAt(d_->r.tex)->levels : -1, texAt(d_->r.tex) ? texAt(d_->r.tex)->slope : -1.f, texAt(d_->r.tex) ? texAt(d_->r.tex)->ground : -1, texAt(d_->r.tex) ? texAt(d_->r.tex)->w : 0, texAt(d_->r.tex) ? texAt(d_->r.tex)->h : 0, (unsigned)d_->r.flags, sf[i], d_->r.bb[0], d_->r.bb[1], d_->r.bb[2], d_->r.bb[3], d_->r.bb[4], d_->r.bb[5], sqrtf(dx * dx + dy * dy + dz * dz)); smkWrite(l_); } }
  unsigned dcS0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
  if (smkOn) {   // счётчик для смоука (Sonnet-Аудит-FPS): кадры с глазом внутри ящика непрозрачного куска города / ближе 0,3 м к его грани, раз в секунду
    static unsigned t0_, nF_, nIn_, nNear_, nMaxCl_, nWall_, nChk_, nFront_; int in_ = 0, near_ = 0, cl_ = 0, wall_ = 0, chk_ = 0, front_ = 0;
    {   // настоящая стена рядом с глазом (не ящик): воксель глаза или соседний в радиусе 0,3 м занят в сетке негоризонтальных граней camgrid (клетка готова, cgs == 2)
      static const float o_[7][3] = {{0, 0, 0}, {.3f, 0, 0}, {-.3f, 0, 0}, {0, .3f, 0}, {0, -.3f, 0}, {0, 0, .3f}, {0, 0, -.3f}};
      for (unsigned ci_ = 0; ci_ < ncell && !wall_; ci_++) { const Cell *c_ = &cl[ci_];
        if (c_->state != 2 || !c_->cg || c_->cgs != 2 || eyeX < c_->bb[0] - .3f || eyeX > c_->bb[3] + .3f || eyeY < c_->bb[1] - .3f || eyeY > c_->bb[4] + .3f || eyeZ < c_->bb[2] - .3f || eyeZ > c_->bb[5] + .3f) continue;
        chk_ = 1;
        for (int k_ = 0; k_ < 7; k_++) { float q_[3] = {eyeX + o_[k_][0], eyeY + o_[k_][1], eyeZ + o_[k_][2]}; if (cgOcc(c_, q_)) { wall_ = 1; break; } } } }
    front_ = camRealWall(wallFrontR > 0.f ? wallFrontR : fmaxf(camZn, .3f) + .2f);   // wall_front: стена по взгляду до max(znear, 0,3) + 0,2 м
    // №1060 (скорость 250: счётчик CAMCLIPSTAT — служебная цена тура 0,6–1,3 мс PSP за кадр на каждый кадр: обход всех видимых кусков с чтением Piece 112 Б; в игре без смоука его нет): кусок, чья клетка
    // дальше 0,5 м от глаза, не ближе 0,5 м сам (ящик куска внутри ящика клетки) — Piece не читается; строка CAMCLIPSTAT та же. CAMCLIPFAST1060_OLD=1 / файл camclipfast1060old / flag camclipfast1060 0 — полный обход;
    // flag camclipchk 1 — считать обоими способами и писать расхождения (CAMCLIPFAST mismatch)
    static unsigned ccEp1060[4096], ccVal1060[4096]; static unsigned ccEpoch1060; static unsigned ccBad1060;
    const int ccFast_ = camClipFast1060 && ncell <= 4096u; if (ccFast_) ccEpoch1060++;
    int in2_ = 0, near2_ = 0, cl2_ = 0;
    for (int i = 0; i < nd; i++) {
      if (ccFast_ && scl[i] < ncell) { unsigned c_ = scl[i]; if (ccEp1060[c_] != ccEpoch1060) { ccEp1060[c_] = ccEpoch1060; ccVal1060[c_] = boxD2(cl[c_].bb) < CAM_CLIP_R * CAM_CLIP_R; }
        if (!ccVal1060[c_] && !camClipChk1060) continue; }
      const Piece *d_ = &pc[sp[i]]; if (d_->r.flags & (2 | 4)) continue;
      float dx = fmaxf(fmaxf(d_->r.bb[0] - eyeX, eyeX - d_->r.bb[3]), 0.f), dy = fmaxf(fmaxf(d_->r.bb[1] - eyeY, eyeY - d_->r.bb[4]), 0.f), dz = fmaxf(fmaxf(d_->r.bb[2] - eyeZ, eyeZ - d_->r.bb[5]), 0.f);
      float q_ = dx * dx + dy * dy + dz * dz;
      if (camClipChk1060 && ccFast_ && scl[i] < ncell && !ccVal1060[scl[i]] && q_ < CAM_CLIP_R * CAM_CLIP_R) { ccBad1060++; if (smkOn && ccBad1060 <= 5) { char l_[160]; snprintf(l_, sizeof l_, "CAMCLIPFAST mismatch piece=%d cell=%u q=%.3f\n", i, (unsigned)scl[i], q_); smkWrite(l_); } }
      if (camClipChk1060 && ccFast_ && scl[i] < ncell && !ccVal1060[scl[i]]) continue;   // сверка: быстрый путь этот кусок пропустил бы
      if (q_ == 0.f) in_ = 1; if (q_ < .09f) near_ = 1; if (q_ < CAM_CLIP_R * CAM_CLIP_R) cl_++; }
    (void)in2_; (void)near2_; (void)cl2_;
    nF_++; nIn_ += in_; nNear_ += near_; nWall_ += wall_; nChk_ += chk_; nFront_ += front_; if ((unsigned)cl_ > nMaxCl_) nMaxCl_ = (unsigned)cl_;
    unsigned now_ = sceKernelGetSystemTimeLow(); if (!t0_) t0_ = now_;
    if (now_ - t0_ >= 1000000u) { char l_[220]; snprintf(l_, sizeof l_, "CAMCLIPSTAT t=%.1f frames=%u eye_inside=%u near03=%u clip_pieces_max=%u wall_vox=%u wall_grid=%u wall_front=%u ggnear_frames=%u ggnear=%d\n", smkT, nF_, nIn_, nNear_, nMaxCl_, nWall_, nChk_, nFront_, ggNearFrames, ggNearOn); ggNearFrames = 0; smkWrite(l_); t0_ = now_; nF_ = nIn_ = nNear_ = nMaxCl_ = nWall_ = nChk_ = nFront_ = 0; } }
  if (smkOn) dcUs[8] += sceKernelGetSystemTimeLow() - dcS0_;   // TEST24: цена самого счётчика CAMCLIPSTAT (только смоук)
  int zfN_ = zfBegin();   // №932б: 2 — дальние клетки отдельным проходом со своей проекцией, 1 — как раньше
  for (int zp_ = 0; zp_ < zfN_; zp_++) {
  if (zfN_ == 2 && zp_ == 0) zfPassStart();
  for (int i = 0; i < nd; i++) {
    if (zfN_ == 2 && zfFarPiece(&pc[sp[i]]) != (zp_ == 0)) continue;
    if (sf[i] && geOver(GE_SOFT)) continue;   // правка 103: дальние — первыми из кадра
    if (!(rfOld & RF_DL) && scl[i] < ncell && cdlMark[scl[i]] == cdlEpoch) { rfDlSkip++; continue; }   // О4: клетка уже вызвана — Piece не читаем
    Piece *d = &pc[sp[i]]; unsigned ci = d->r.cell;
    if (ci < ncell) jitterCell(cl[ci].bb, ex, ey, ez);   // №509: ближняя клетка (лист, куски, запасной путь) без сдвига проекции, дальняя со сдвигом
    if ((d->r.flags & 2) && !(d->r.flags & 4)) { DPC(0, fogRadial(d, sf[i]); drawPiece(d, sf[i], 0, 0, 0, 1, 0)); continue; }   // в очередь tq
    if (ci >= ncell) continue;
    if (farPlainD > 0.f && boxD2(cl[ci].bb) > farPlainD * farPlainD) {   // правка 508: дальняя клетка — дальней версией без нарезки; TEST23: листом клетки (dlBuildFar), иначе кусками
      if (!FAR_PLAIN_LIST_OLD && !dlNoList && cl[ci].n <= 64) {
        CellDL *FL = &cdl[ci];
        if (cdlMark[ci] == cdlEpoch) continue;                                    // лист этой клетки уже вызван в кадре
        if (!FL->fok && builds < DL_BUILD_MAX && ((builds++, dcN[6]++, (smkOn ? (dcT1_ = sceKernelGetSystemTimeLow()) : 0), dlBuildFar(ci) < 0))) FL->fok = 3;
        if (smkOn && dcT1_) { dcUs[6] += sceKernelGetSystemTimeLow() - dcT1_; dcT1_ = 0; }   // не собрался — по кускам (ниже)
        if (FL->fok == 1 && !geOver(GE_HARD)) {
          cdlMark[ci] = cdlEpoch; fogRadial(d, 1);
          float k_ = d->r.sc; ScePspFMatrix4 m_ = {{k_, 0, 0, 0}, {0, k_, 0, 0}, {0, 0, k_, 0}, {d->r.ox - ex, d->r.oy - ey, d->r.oz - ez, 1}};
          sceGuSetMatrix(GU_MODEL, &m_); if (TOON_NOISE_WORLD && FL->fnoise) flatTexEmit(); sceGuCallList(FL->fbuf); dlLost(); marksCall(ci, ex, ez); drawnTris += FL->ftris; draws += FL->fdraws; lstBinds += FL->fnb; dlCallsFrame++; farListCalls++; continue; }
        if (FL->fok == 2) continue;                                               // у клетки нет непрозрачных кусков
      }
      int fp_ = (d->r.flags & F_FARP) != 0; DPC(1, fogRadial(d, fp_); drawPiece(d, fp_, -ex, -ey, -ez, 1, 0)); farCellPieces++; continue; }
    if (cellHybOn && !CELL_HYB_OLD && cellHybMode(ci, ex, ey, ez) == 2) { DPC(2, fogRadial(d, sf[i]); drawPiece(d, sf[i], -ex, -ey, -ez, 1, 0)); continue; }   // гибрид: нужных кусков мало — только они
    if (cdlMark[ci] == cdlEpoch) continue;                 // клетка уже вызвана в этом кадре
    if (cdlMark[ci] == (cdlEpoch | 0x80000000u) || (!cdl[ci].ok && (builds >= DL_BUILD_MAX || (builds++, dlBuildT(ci) < 0)))) {
      cdlMark[ci] = cdlEpoch | 0x80000000u;               // лист не собран — куски этой клетки прежним путём
      DPC(3, fogRadial(d, sf[i]); drawPiece(d, sf[i], -ex, -ey, -ez, 1, 0)); dlFallFrame++; continue;
    }
    cdlMark[ci] = cdlEpoch;
    unsigned ct0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
    int ggCell_ = !dlNoList && ggOn && GG_CELL_OK(ci) && ggBoxRiskW(cl[ci].bb);   // правка 340: клетка у камеры в кадре с низкой ближней плоскостью; №840 — или клетка с крупными треугольниками
    // «быстрый расчёт»: ggCellCut (есть ли куски под риском) и ggCellMask (какие) обходили куски клетки дважды с одним предикатом; у клетки
    // до 64 кусков маска считается один раз и её «не ноль» — ответ ggCellCut. -DGG_MASK_ONCE_OLD=1 — прежнее
#ifndef GG_MASK_ONCE_OLD
#define GG_MASK_ONCE_OLD 0
#endif
    unsigned long long mkOnce_ = 0; int haveOnce_ = 0;
    if (ggCell_ && !(rfOld & RF_GG)) {
      int cut_;
      if (!GG_MASK_ONCE_OLD && !GG_FOOT_CUT_OLD && !sf[i] && cl[ci].n <= 64) { mkOnce_ = ggCellMaskC(ci); haveOnce_ = 1; cut_ = mkOnce_ != 0; }
      else cut_ = ggCellCut(ci);
      if (!cut_) { ggCell_ = 0; rfGgListF++; } }
    if (smkOn) smkCtMask += (sceKernelGetSystemTimeLow() - ct0_) / 1000.0;   // О11: ни один кусок не под риском — листом
    if (ggCell_ && !GG_FOOT_CUT_OLD && !sf[i] && cl[ci].n <= 64) {   // «плавность»: клетка у камеры — лист без рискованных кусков + срез только их
      CellDL *P_ = &cdl[ci]; unsigned mk0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; unsigned long long mk = haveOnce_ ? mkOnce_ : ggCellMaskC(ci); if (smkOn) smkCtMask += (sceKernelGetSystemTimeLow() - mk0_) / 1000.0;
      if (mk && (P_->pok == 0 || P_->pmask != mk)) {
        if (P_->pbuf) { dlBytesNow -= P_->pbytes; free(P_->pbuf); P_->pbuf = 0; }
        P_->pok = 0;
        if (builds < DL_BUILD_MAX) { unsigned bd_ = smkOn ? sceKernelGetSystemTimeLow() : 0; builds++; if (dlBuildM(ci, mk, 1) < 0) { P_->pok = 3; P_->pmask = mk; } if (smkOn) smkCtBuild += (sceKernelGetSystemTimeLow() - bd_) / 1000.0; }   // 3 — не собрался: до смены маски прежний путь
      }
      if (mk && P_->pok && P_->pok != 3 && P_->pmask == mk && !geOver(GE_HARD)) {
        fogRadial(d, sf[i]);
        if (P_->pok == 1) {
          float k = d->r.sc; ScePspFMatrix4 m = {{k, 0, 0, 0}, {0, k, 0, 0}, {0, 0, k, 0}, {d->r.ox - ex, d->r.oy - ey, d->r.oz - ez, 1}};
          { unsigned cl0_ = smkOn ? sceKernelGetSystemTimeLow() : 0; sceGuSetMatrix(GU_MODEL, &m); if (TOON_NOISE_WORLD && P_->pnoise) flatTexEmit(); sceGuCallList(P_->pbuf); dlLost(); marksCall(ci, ex, ez); if (smkOn) smkCtList += (sceKernelGetSystemTimeLow() - cl0_) / 1000.0; }
          drawnTris += P_->ptris; draws += P_->pdraws; lstBinds += P_->pnb; dlCallsFrame++;
        }
        ggCellsF++;
        { unsigned rk_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
        for (unsigned k = cl[ci].first; k < cl[ci].first + cl[ci].n; k++) { if (!(mk >> (k - cl[ci].first) & 1ull)) continue; Piece *e = &pc[k];
          DPC(4, fogRadial(e, sf[i]); drawPiece(e, sf[i], -ex, -ey, -ez, 1, 0)); }
        if (smkOn) smkCtRisky += (sceKernelGetSystemTimeLow() - rk_) / 1000.0; }
        continue; }
      if (!mk) { ggCell_ = 0; rfGgListF++; }
    }
    if (dlNoList || (ggCell_ && (ggCellsF++, 1))) {   // служебно: клетка целиком прежним путём; правка 340: клетка у камеры в кадре с низкой ближней плоскостью — по кускам (срез по полосе)
      for (unsigned k = cl[ci].first; k < cl[ci].first + cl[ci].n; k++) { Piece *e = &pc[k]; if (e->r.ni <= 0 || ((e->r.flags & 2) && !(e->r.flags & 4))) continue;
        DPC(5, fogRadial(e, sf[i]); drawPiece(e, sf[i], -ex, -ey, -ez, 1, 0)); }
      continue; }
    CellDL *L = &cdl[ci]; if (L->ok != 1) continue;
    if (diagOn) diagCellList(ci);   // №791: лист клетки без среза — треугольники под риском полосы GE (замер)
    if (geOver(GE_HARD)) continue;
    fogRadial(d, sf[i]);
    float k = d->r.sc; ScePspFMatrix4 m = {{k, 0, 0, 0}, {0, k, 0, 0}, {0, 0, k, 0}, {d->r.ox - ex, d->r.oy - ey, d->r.oz - ez, 1}};
    { unsigned cl1_ = smkOn ? sceKernelGetSystemTimeLow() : 0; sceGuSetMatrix(GU_MODEL, &m); if (TOON_NOISE_WORLD && L->noise) flatTexEmit(); sceGuCallList(L->buf); dlLost(); marksCall(ci, ex, ez); if (smkOn) smkCtList += (sceKernelGetSystemTimeLow() - cl1_) / 1000.0; }
    drawnTris += L->tris; draws += L->draws; lstBinds += L->nb; dlCallsFrame++;
  }
  if (zfN_ == 2 && zp_ == 0) zfPassEnd();
  }
  if (jitActive && !JITTER_NEAR_OLD && !jitCur) jitterUse(1);   // дальше (небо) — со сдвигом, как раньше
  if (smkOn) { dcUs[7] += sceKernelGetSystemTimeLow() - dcT0_; dcN[7]++; }
  dlLost(); resetState();   // после листов состояние GE — последнего куска листа: вернуть кадру его (сдвиг глубины 0, без отсечения)
}

