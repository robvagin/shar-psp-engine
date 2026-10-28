// 17.10 монеты спрайтами одной пачкой. Вечер 17.10 (Роб, после TEST31): плоская картинка со сжатием по ширине («нет торца») заменена кадрами оборота
// настоящей 3D-монеты, запечёнными заранее на Маке в Blender (coinspr.bin рядом с EBOOT; см. ниже csLoad). Роб на гифке: «выглядит отлично, офигенно».
// Подключается из main.c после main_levels.h. COIN_SPRITE_OLD=1 / «flag coinspr 0» — 3D-модель
#ifndef COIN_SPRITE_OLD
#define COIN_SPRITE_OLD 0
#endif
#define COIN_SPR_MAX 192   // проверка 17.10: квад — 6 вершин, в ту же память (1152 вершины) влезает 192 монеты, сверх — 3D-модель
#define COIN_SPR_N 8
static float csRSign = 1.f;   // служебное: «flag coinmirror 1» — кадр зеркально по горизонтали (сверка ориентации знака)
static int coinSprite = !COIN_SPRITE_OLD; static unsigned coinSprCnt, coinSprFrames, coinSprSum;
typedef struct { float u, v; unsigned c; float x, y, z; } CsV;
static CsV coinSprV[COIN_SPR_MAX * 6] __attribute__((aligned(64)));
// Роб 17.10 (после TEST31): «это не спрайт — PNG-шку крутишь в пространстве, у неё нет торца… сам генерируешь заранее на макбуке, запекаешь красиво в гифку
// без фона… не крутится вокруг своей оси, а всё время направлена к нам… одну на всех». Монета — квадрат лицом к камере (вокруг вертикали) с кадром ленты
// coinspr.bin рядом с EBOOT: 32 кадра оборота настоящей 3D-монеты игры с торцом (Blender: tools/coin/coin_dump.py -> coin_blender.py / SHAR_coin.blend ->
// coin_pack.py). Кадр — общий для всех монет, по часам (скорость оборота как у монет игры, C_SPIN). Все монеты кадра — один вызов, 2 треугольника на монету.
// Нет файла — 3D-модель (как «flag coinspr 0»)
typedef struct { char magic[4]; unsigned short w, h, levels, frames, cols, rows, px, pad; float half, cy, fps; } CsHdr;   // ровно 32 Б, как пишет tools/coin/coin_pack.py
static CsHdr csH; static unsigned char *csMem; static const unsigned char *csPal, *csLv[2]; static int csState;   // 0 — не читали, 1 — лента есть, -1 — нет
static unsigned char csPalRaw[1024]; static int csRawOk;   // проверка 18.10: исходная палитра монеты — для смены тона (Cartoon, Saturation) без перезагрузки
static void coinSprRetone(void) { wsRetone();   // №803: и палитра ключа-спрайта (Cartoon, Saturation)
  if (!csRawOk || csState != 1 || !csPal) { if (smkOn) { char l_[100]; snprintf(l_, sizeof l_, "COINRETONE skip raw=%d state=%d pal=%d\n", csRawOk, csState, csPal != 0); smkWrite(l_); } return; } memcpy((void *)csPal, csPalRaw, 1024); palTone((void *)csPal);
  if (smkOn) { unsigned r_[3] = {0, 0, 0}, n_[3] = {0, 0, 0}, cnt_ = 0; for (int j = 0; j < 256; j++) if (csPal[j * 4 + 3] > 200) { cnt_++; for (int k = 0; k < 3; k++) { r_[k] += csPalRaw[j * 4 + k]; n_[k] += csPal[j * 4 + k]; } }
    char l_[160]; snprintf(l_, sizeof l_, "COINRETONE n=%u raw_sum=%u,%u,%u now_sum=%u,%u,%u sat=%d ct=%d\n", cnt_, r_[0], r_[1], r_[2], n_[0], n_[1], n_[2], toneSatPct(), toneContrastPct()); smkWrite(l_); } }
static void csLoad(void) {   // №877: повторный вызов не теряет прежнюю ленту
  if (csMem) { free(csMem); csMem = 0; csPal = 0; }
  csState = -1; char fn[256]; sibling(fn, "coinspr.bin"); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fd < 0) return;
  int n = (int)sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET);
  if (n > 32 + 1024 && n < 1 << 20 && (csMem = (unsigned char *)memalign(16, (size_t)n)) && sceIoRead(fd, csMem, (SceSize)n) == n) {
    memcpy(&csH, csMem, sizeof csH);
    unsigned l0 = (unsigned)csH.w * csH.h, l1 = l0 / 4;
    if (!memcmp(csH.magic, "CSP1", 4) && csH.w == 512 && csH.h == 256 && csH.levels == 2 && csH.frames >= 8 && csH.frames <= csH.cols * csH.rows && csH.cols * csH.px == csH.w &&
        csH.rows * csH.px == csH.h && 32u + 1024u + l0 + l1 <= (unsigned)n && csH.half > .05f && csH.half < 2.f && csH.cy > -2.f && csH.cy < 2.f && csH.fps > 1.f && csH.fps < 120.f) {   // сравнения отсекают и NaN
      csPal = csMem + 32; csLv[0] = csPal + 1024; csLv[1] = csLv[0] + l0; memcpy(csPalRaw, csPal, 1024); csRawOk = 1; palTone((void *)csPal);
      sceKernelDcacheWritebackRange(csMem, (unsigned)n); csState = 1; }
  }
  sceIoClose(fd);
  if (csState < 0 && csMem) { free(csMem); csMem = 0; }
  if (smkOn) { char l_[160]; snprintf(l_, sizeof l_, "COINSPR atlas=%d frames=%u px=%u half=%.3f cy=%.3f fps=%.1f\n", csState, csH.frames, csH.px, csH.half, csH.cy, csH.fps); smkWrite(l_); }
}
static unsigned csFrame;   // кадр ленты этого кадра игры (один на все монеты)
static void coinSprPreload(void) { if (!csState && coinSprite) csLoad(); }   // при загрузке уровня (main_levels.h levelLoadAfter)
// №1020: кадр оборота монеты сдвинут по монете на COIN1020_PH кадров (номер монеты -> хэш, 0..COIN1020_PH − 1): монеты не вращаются синхронно, но видно, что они одной ленты
#ifndef COIN1020_PH
#define COIN1020_PH 6u
#endif
static unsigned coinPhMask1020;   // служебно: какие сдвиги кадра встретились
static unsigned coinPh1020(unsigned i) { unsigned s = i * 0x9e3779b1u + 0x1234567u; s ^= s >> 16; s *= 0x7feb352du; s ^= s >> 15; s *= 0x846ca68bu; s ^= s >> 16; return s % COIN1020_PH; }
static int coinSprAdd(const Coin *c, float a, float ex, float ez, unsigned idx) {   // 1 — монета ушла в пачку
  if (!csState) csLoad();
  if (csState < 0 || coinSprCnt >= COIN_SPR_MAX) return 0;
  if (!coinSprCnt) csFrame = (sceKernelGetSystemTimeLow() / (unsigned)(1e6f / csH.fps)) % csH.frames;
  float dx = c->x - ex, dz = c->z - ez, l = sqrtf(dx * dx + dz * dz); if (l < .05f) return 0;
  float Rx = -dz / l * csRSign, Rz = dx / l * csRSign, hs = csH.half * a, C[3] = {c->x, c->y + csH.cy * a, c->z};   // R — вправо по экрану
  unsigned ph_ = fxCoin1020 ? coinPh1020(idx) : 0u; coinPhMask1020 |= 1u << ph_;
  unsigned cf_ = (csFrame + ph_) % csH.frames;   // №1020
  unsigned fc = cf_ % csH.cols, fr = cf_ / csH.cols;
  float u0 = (fc * csH.px + .5f) / csH.w, u1 = ((fc + 1) * csH.px - .5f) / csH.w, v0 = (fr * csH.px + .5f) / csH.h, v1 = ((fr + 1) * csH.px - .5f) / csH.h;
  const float X[4] = {-1.f, 1.f, -1.f, 1.f}, Y[4] = {1.f, 1.f, -1.f, -1.f}, U[4] = {u0, u1, u0, u1}, V[4] = {v0, v0, v1, v1}; static const unsigned char ord[6] = {0, 2, 1, 1, 2, 3};
  CsV *o = coinSprV + coinSprCnt * 6;
  for (int m = 0; m < 6; m++) { int k = ord[m]; o->u = U[k]; o->v = V[k]; o->c = 0xFFFFFFFFu; o->x = C[0] + Rx * hs * X[k]; o->y = C[1] + hs * Y[k]; o->z = C[2] + Rz * hs * X[k]; o++; }
  coinSprCnt++; return 1;
}
static void coinSprFlush(int ctex) {
  (void)ctex; if (!coinSprCnt) return;
  sceKernelDcacheWritebackRange(coinSprV, coinSprCnt * 6 * sizeof(CsV));
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuEnable(GU_TEXTURE_2D); texOn = -1;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, csPal);
  sceGuTexMode(GU_PSM_T8, 1, 0, 1); sceGuTexImage(0, csH.w, csH.h, csH.w, csLv[0]); sceGuTexImage(1, csH.w / 2, csH.h / 2, csH.w / 2, csLv[1]);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_LINEAR); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); sceGuTexFlush();
  sceGuDisable(GU_BLEND); sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0x60, 0xff); sceGuDepthMask(GU_FALSE); setCull(0); curFlags = -1;
  sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)(coinSprCnt * 6), 0, coinSprV);
  sceGuAlphaFunc(GU_GREATER, 0x40, 0xff);   // проверка 17.10: вернуть порог вырезки, как у всего мира (иначе листва и заборы после монет режутся строже)
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexScale(128.f, 128.f); rbId = -9; rbPal = 0; resetState(); coinSprSum += coinSprCnt; coinSprFrames++; coinSprCnt = 0;
}
// №802 (Роб 18.10 после TEST34: «в меню монетка по-прежнему генерируется 3D-шная — поставь везде наш спрайт, как в мире»): монета у счёта (HUD, пауза,
// будка, лавка, гардероб — все через hudCoin3D) — тот же кадр ленты coinspr.bin и те же часы, что у монет мира; квадрат на месте значка: монета ростом
// со значок (как 3D-монета hudCoin3D: k = h / 0,62 пикс на метр, центр монеты 0,31 м над низом — в центр значка), прозрачность значка. Квадрат до 40 пикс —
// уровень 1 ленты (кадр 32 пикс), крупнее — уровень 0. Нет ленты — 3D-монета, как было. HUD_COIN_SPRITE_OLD=1 — 3D-монета
#ifndef HUD_COIN_SPRITE_OLD
#define HUD_COIN_SPRITE_OLD 0
#endif
static unsigned coinSprHudN;   // служебное: кадров со спрайтом у счёта
static int coinSprHud(float cx, float cy, float hh, float al) {
  if (HUD_COIN_SPRITE_OLD || !coinSprite) return 0;
  if (!csState) csLoad();
  if (csState < 0 || hh < 2.f) return 0;
  float k = hh / .62f, hs = csH.half * k, ccy = cy - (csH.cy - .31f) * k;
  unsigned fr = (sceKernelGetSystemTimeLow() / (unsigned)(1e6f / csH.fps)) % csH.frames;
  int lv = hs * 2.f <= 40.f ? 1 : 0; unsigned px = lv ? csH.px / 2u : csH.px, w = lv ? csH.w / 2u : csH.w, h = lv ? csH.h / 2u : csH.h;
  float u0 = (float)((fr % csH.cols) * px), v0 = (float)((fr / csH.cols) * px);
  typedef struct { float u, v; unsigned c; float x, y, z; } HV;
  HV *v = (HV *)sceGuGetMemory(2 * sizeof(HV));
  unsigned a = (unsigned)(al * 255.f + .5f); if (a > 255u) a = 255u; unsigned col = (a << 24) | 0x00FFFFFFu;
  v[0] = (HV){u0, v0, col, cx - hs, ccy - hs, 0.f}; v[1] = (HV){u0 + (float)px, v0 + (float)px, col, cx + hs, ccy + hs, 0.f};
  sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_TEXTURE_2D); texOn = -1;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(32, csPal);
  sceGuTexMode(GU_PSM_T8, 0, 0, 1); sceGuTexImage(0, (int)w, (int)h, (int)w, csLv[lv]);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); sceGuTexFlush();
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuEnable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0, 0xff);
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
  sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexScale(128.f, 128.f); rbId = -9; rbPal = 0;
  sceGuEnable(GU_DEPTH_TEST); resetState(); coinSprHudN++;
  return 1;
}
