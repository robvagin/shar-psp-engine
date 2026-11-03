// №817 (Роб 18.10 после №798: «тем объектам, которые мы оставили, — гидрант, их почтовый ящик возле дома — тоже должен быть векторный контур; не надо прям каждый
// столб делать, но чтобы везде визуально учитывался векторный мультяшный стиль»): тонкий контур пропов (PRP1) и стейтпропов (SPR1) тем же пером, что у героев
// и машин (TOON_LINE_ABGR, вдали светлеет — skinToonAirMix), толщиной PROP_TOON_K от героя (skinToonWorldW по расстоянию).
// Правило отбора: обводим то, что читается предметом «в рост человека и ниже» — гидрант, ящик, урна, скамейка, автомат, будка, ящик с газетами, щит на ножке;
// высокие опоры и деревья обводкой превращаются в штриховку и шум:
//   экземпляр — не выше PROP_TOON_H м и полушириной не больше PROP_TOON_R м (фонари, столбы, светофоры, деревья, остановки, заборы — без контура), ближе PROP_TOON_D м,
//   в кадре и рисуется сам (то же правило у глаза, что у тела: clipGuardProp / clipGuardSphere, мелочь меньше spMinPx — нет);
//   кусок — непрозрачный, не мельче PROP_TOON_MIN м по наибольшей стороне (болты, ручки — без линий) и не «ножка» (поперёк тоньше PROP_TOON_POLE м при высоте
//   больше 1,2 м: у знака обводится щит, не стойка).
// Порядок как у героев (№797): оболочки всех годных экземпляров кадра — ДО тел пропов (листов клеток B1 и прежнего пути), без записи глубины, с тестом против
// уже нарисованного мира; тела потом закрывают внутреннее — линий внутри предмета нет, край остаётся. Оболочка — кусок, раздутый матрицей около центра своего
// бокса (бокс в сырых координатах вершин — кэш по адресу вершин). Даль — как у героев и машин (skinToonWorldW: дальше 40 м линий нет ни у кого).
// Не больше PROP_TOON_MAXP кусков за кадр. PROP_TOON_OLD=1 — без контура, как TEST34
#ifndef PROP_TOON_OLD
#define PROP_TOON_OLD 0
#endif
#define PROP_TOON_K .75f
#define PROP_TOON_H 3.f
#define PROP_TOON_R 1.6f
// №838 (тур TEST36 на PSP 19.10: x_props +2,65 мс на L2, +2,36 на L3, +1,0 на L1 против TEST34 — в эмуляторе было «+0,1»): обводим только ближнее, где
// линию видно, — до PROP_TOON_D_LITE м и не больше PROP_TOON_MAXP_LITE кусков за кадр; кадр перегружен (регулятор кадра) — без обводки пропов.
// PROP_TOON_LITE_OLD=1 — 40 м и 64 куска, как TEST36
#ifndef PROP_TOON_LITE_OLD
#define PROP_TOON_LITE_OLD 0
#endif
#define PROP_TOON_D (PROP_TOON_LITE_OLD ? 40.f : 18.f)
#define PROP_TOON_MIN .2f
#define PROP_TOON_POLE .3f
#define PROP_TOON_MAXP (PROP_TOON_LITE_OLD ? 64 : 24)
#define PROP_TOON_H_TONE 16.f   // №951б: потолок высоты экземпляра при Ink Tone (пальма, фонарь)
#define PROP_TOON_R_TONE 7.f
#define PROP_TOON_MAXP_TONE 48
// №1138 (Роб 02.11: «обводка только у персонажей и машин; у предметов — почтовых ящиков, гидрантов, урн, скамеек, всех сбиваемых и стейтпропов — убрать совсем»): проход контура пропов и стейтпропов выключен;
// остаётся телефонная будка PhoneBooth (не случайный предмет, пока Роб не скажет убрать): стейтпроп из 7 кусков, где кусок 1 — плоский восьмиугольник (18 вершин) или уже убранный boothBase1052 (0), кусок 6 — 42
// вершины или 0 (тот же признак, что в main_levels.h boothBase1052). Оболочки героя, пешеходов, персонажей миссий и машин рисуются в другом месте и не затронуты. Цена: контур пропов на кадр был до 2 экземпляров,
// 2 кусков, 357 вершин (по журналам смоука PROPTOON), теперь у будки только её куски; PROPINK1138_OLD=1 / «flag propink1138old 1» — как в TEST84
#include "propink1138.h"
static int propInk1138Old = PROPINK1138_OLD;
#include "propbooth1159.h"   // №1159: ptIsBooth и список будок уровня
typedef struct { const void *v; unsigned nv; float lo[3], hi[3]; } PtBox;
static PtBox ptBox[256]; static unsigned ptDraws, ptInst, ptVerts, ptUs, ptFrames; static int ptLogLv = -1, ptLogN; static unsigned ptLogK[24];
static void ptLog(int kind, unsigned i, float x, float y, float z, float h, float r, int got) {   // журнал смоука: первые 24 обведённых экземпляра уровня (где они, какого размера)
  if (!smkOn || !got) return; if (ptLogLv != gameLevel) { ptLogLv = gameLevel; ptLogN = 0; } if (ptLogN >= 24) return;
  unsigned key = (unsigned)kind << 31 | i; for (int k = 0; k < ptLogN; k++) if (ptLogK[k] == key) return; ptLogK[ptLogN++] = key;
  char l_[140]; snprintf(l_, sizeof l_, "PTINST lv=%d k=%d i=%u pos=%.1f,%.1f,%.1f h=%.2f r=%.2f\n", gameLevel, kind, i, x, y, z, h, r); smkWrite(l_);
}
static const PtBox *ptBoxOf(const void *v, unsigned nv, int flags) {
  PtBox *b = &ptBox[((unsigned)((uintptr_t)v >> 4) * 2654435761u) >> 24];
  if (b->v == v && b->nv == nv) return b;
  int vs = (flags & F_V12) ? 12 : 16, po = (flags & F_V12) ? 6 : 8;
  for (int a = 0; a < 3; a++) { b->lo[a] = 1e30f; b->hi[a] = -1e30f; }
  for (unsigned i = 0; i < nv; i++) { const short *q = (const short *)((const char *)v + i * vs + po);
    for (int a = 0; a < 3; a++) { float x = q[a] * (1.f / 32768.f); if (x < b->lo[a]) b->lo[a] = x; if (x > b->hi[a]) b->hi[a] = x; } }
  b->v = v; b->nv = nv; return b;
}
// оболочка куска с матрицей m: раздута на W м по каждой оси (плоскую ось не трогаем); 0 — кусок по правилу без контура
static int ptPoles, ptTone951; static unsigned ptToneN, ptToneTall;   // ptTone951 — служебное: Tone в кадре; ptToneTall — обведено высоких экземпляров (строка PROPTOON)
static int ptPolesDummy;   // №951б: Tone — обводятся и тонкие высокие (стволы пальм, столбы)
static unsigned ptPieceInk(int t, unsigned dflt) {   // №951б: Tone — тон среднего цвета текстуры куска (tstat); нет статистики — прежнее перо
  if ((unsigned)t >= ntex || !tex[t].tstatOk || (!tex[t].tstat[0] && !tex[t].tstat[1] && !tex[t].tstat[2])) return dflt;
  return skinInkTone(tex[t].tstat[0], tex[t].tstat[1], tex[t].tstat[2]); }
static int ptHull(const void *v, const unsigned short *idx, int ni, unsigned nv, int flags, const float *m, float W, unsigned ink) {
  if (ni <= 0 || !v) return 0;
  const PtBox *b = ptBoxOf(v, nv ? nv : (unsigned)ni, flags);
  float A[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, ext[3];
  for (int a = 0; a < 3; a++) {
    const float *col = m + 4 * a; float s = sqrtf(col[0] * col[0] + col[1] * col[1] + col[2] * col[2]);
    float half = (b->hi[a] - b->lo[a]) * .5f * s, c = (b->hi[a] + b->lo[a]) * .5f;   // half — полуразмер в мире, м; c — центр в сырых координатах
    ext[a] = 2.f * half;
    float sc = half > W ? 1.f + W / half : 1.f; A[a * 5] = sc; A[12 + a] = c * (1.f - sc);
  }
  float mx = fmaxf(ext[0], fmaxf(ext[1], ext[2]));
  if (mx < PROP_TOON_MIN || (!ptPoles && ext[1] > 1.2f && fmaxf(ext[0], ext[2]) < PROP_TOON_POLE)) return 0;   // мелочь и ножки знаков — без линий
  float M[16];
  for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) {   // M = m · A (столбцы подряд, как ScePspFMatrix4)
    float x = 0.f; for (int k = 0; k < 4; k++) x += m[k * 4 + r] * A[c * 4 + k]; M[c * 4 + r] = x; }
  Piece p; memset(&p, 0, sizeof p); p.r.tex = -1; p.r.flags = flags & ~3; p.r.ni = ni; p.v = (Vert *)v; p.idx = (unsigned short *)idx;
  setFlags(flags & ~3); sceGuDepthMask(GU_TRUE);   // без записи глубины: тело ляжет поверх, снаружи силуэта останется ободок
  sceGuFog(0.f, 1e-3f, ink);   // дымка в цвет пера — ободок ровной линией
  sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)M);
  drawGeo(&p, 0, ni); ptVerts += (unsigned)ni;
  return 1;
}
// проход оболочек пропов и стейтпропов кадра — вызывать ДО тел пропов (main_loop.h, перед листами клеток B1)
// №844 (Роб 19.10, TEST38: «от гаечного ключа бледный след, как отстающий кадр тени — дырка запоздалой анимации»): ключ с №803 рисуется спрайтом
// из кадров модели, а обводка (оболочка) — по 3D-модели, которая крутится плавно; её тёмный контур отставал от кадров спрайта. Ключ-спрайт не обводится
// (у кадров спрайта свой контур). PT_WRSPR844_OLD=1 — как было
#ifndef PT_WRSPR844_OLD
#define PT_WRSPR844_OLD 0
#endif
static int wrSprOn(void); static int wrSprInst(unsigned i);   // spritelight.h (подключается позже)
#ifndef PT_TYPE852_OLD
#define PT_TYPE852_OLD 0   // №852: 1 — отбор по размеру типа каждый кадр, а не при сборке списка
#endif
#ifndef PT_NEAR845_OLD
#define PT_NEAR845_OLD 0
#endif
static void propToonPass(float ex, float ey, float ez) {
  if (PROP_TOON_OLD || !skinToonEnabled() || geOver(GE_SOFT) || (!PROP_TOON_LITE_OLD && propsInkGov(trafficFrameGov))) return;   // №1096: регулятор кадра контур предметов больше не гасит (мигание)   // №838: перегрузка кадра — без обводки пропов
  unsigned t0 = smkOn ? sceKernelGetSystemTimeLow() : 0; int np = 0, on = 0; const float e3[3] = {ex, ey, ez}, D2 = PROP_TOON_D * PROP_TOON_D;
  const int tone = skinInkToneOn(); ptPoles = tone;   // №951б: Tone — и высокие предметы (деревья, пальмы, столбы), перо по тону текстуры куска
  const float tH = tone ? PROP_TOON_H_TONE : PROP_TOON_H, tR = tone ? PROP_TOON_R_TONE : PROP_TOON_R; const int tMaxP = tone ? PROP_TOON_MAXP_TONE : PROP_TOON_MAXP;
  // №845 (тур TEST38: обводка 0,2–0,4 мс кадра даже без обведённых — каждый кадр обход ВСЕХ пропов уровня): пропы ближе PROP_TOON_D + 12 м —
  // список, пересчёт раз в 8 кадров или после сдвига глаза на 6 м; стейтпропы — из сетки (spGridCollect). PT_NEAR845_OLD=1 — полный обход
  unsigned npi_ = haveProps ? props.ninst : 0; const unsigned *pi_ = 0;
  if (!PT_NEAR845_OLD && haveProps && propInk1138Old) {   // №1138: список предметов нужен, только если их обводим
    static unsigned *near_, nearN_, nearCap_, nearFr_, nearNi_; static float nearE_[3]; static const void *nearKey_; static int nearTone_;
    float mx_ = ex - nearE_[0], mz_ = ez - nearE_[2];
    if (nearKey_ != (const void *)props.st || nearNi_ != props.ninst || nearCap_ < props.ninst || nearTone_ != tone || !(nearFr_++ & 7) || mx_ * mx_ + mz_ * mz_ > 36.f) {
      nearTone_ = tone;
      if (nearCap_ < props.ninst) { free(near_); near_ = (unsigned *)malloc(props.ninst * sizeof(unsigned)); nearCap_ = near_ ? props.ninst : 0; }
      nearN_ = 0; nearKey_ = props.st; nearNi_ = props.ninst; nearE_[0] = ex; nearE_[2] = ez; const float R2_ = (PROP_TOON_D + 12.f) * (PROP_TOON_D + 12.f);
      for (unsigned i = 0; near_ && i < props.ninst; i++) { const PropState *st = &props.st[i]; float x = pbP.on ? pbP.px[i] : st->x, z = pbP.on ? pbP.pz[i] : st->z;
        if ((x - ex) * (x - ex) + (z - ez) * (z - ez) > R2_) continue;
        if (!PT_TYPE852_OLD) { const PropType *ty_ = &props.types[props.def[i].type]; if (2.f * ty_->halfH > tH || ty_->radius > tR) continue; }   // №852: крупные не обводятся — не в список
        near_[nearN_++] = i; } }
    if (near_) { pi_ = near_; npi_ = nearN_; } }
  const int inkX_ = INK1096_OLD ? 0 : 8;   // №1096: предметы, тронутые игроком, сверх потолка кусков (не выпадают при толпе предметов)
  if (haveProps && propInk1138Old) for (unsigned j_ = 0; j_ < npi_ && np < tMaxP + inkX_; j_++) { unsigned i = pi_ ? pi_[j_] : j_; if (i >= props.ninst) continue;   // №845: индекс из списка — в пределах текущих пропов
    const PropState *st = &props.st[i];
    if (np >= tMaxP && !st->inkTouch) continue;
    float x = pbP.on ? pbP.px[i] : st->x, z = pbP.on ? pbP.pz[i] : st->z, dx = x - ex, dz = z - ez;   // B1: положения подряд (кэш), иначе — состояние
    if (dx * dx + dz * dz > D2 || st->hidden) continue;
    const PropType *ty = &props.types[props.def[i].type];
    if (2.f * ty->halfH > tH || ty->radius > tR) continue;
    { float c[3] = {st->x, st->y, st->z}; if (rfSphereOut((const float (*)[3])pl, e3, c, ty->radius + 2.f * ty->halfH)) continue; }
    if (clipGuardProp(ex, ey, ez, st->x, st->y, st->z, ty->radius, ty->halfH, ty->radius <= 1.5f && camNearSphere(ex, ey, ez, st->x, st->y, st->z, ty->radius))) continue;   // тела у глаза нет — и оболочки
    float dy = st->y - ey, dist = sqrtf(dx * dx + dy * dy + dz * dz), W = PROP_TOON_K * skinToonWorldW(dist); if (W <= 0.f) continue;
    unsigned ink = skinToonAirMix(skinInkAbgr(INK_PROP), dist) & 0xffffffu; int got = 0;
    if (!on) { texEnable(0); bound = -2; sceGuEnable(GU_FOG); fogK = -1.f; on = 1; }
    for (unsigned k = 0; k < ty->nPiece && np < tMaxP + inkX_; k++) {
      const PropPiece *pp = &props.pieces[ty->firstPiece + k];
      if (pp->ni <= 0 || (pp->flags & (2 | F_ADDITIVE))) continue;
      float m[16]; propPieceMatrix(st, pp->ox, pp->oy, pp->oz, pp->sc, m);
      if (ptHull((const char *)props.vertBase + pp->vOff, (const unsigned short *)((const char *)props.idxBase + pp->iOff), pp->ni, pp->nv, pp->flags, m, W, tone ? (skinToonAirMix(ptPieceInk(pp->tex, skinInkAbgr(INK_PROP)), dist) & 0xffffffu) : ink)) { np++; got = 1; }
    }
    ptInst += (unsigned)got; if (tone && got && (2.f * ty->halfH > PROP_TOON_H || ty->radius > PROP_TOON_R)) ptToneTall++; ptLog(0, i, st->x, st->y, st->z, 2.f * ty->halfH, ty->radius, got);
  }
  ptTone951 = tone; unsigned *cs_ = 0, ncs_ = 0;
  if (haveSP) {
    if (!PROPBOOTH1159_OLD && !propInk1138Old && !PT_NEAR845_OLD) { ncs_ = ptBoothEnsure(&sprops); cs_ = ptBoothL; }   // №1159: обводится только будка — обход её списка, без сетки и проверки каждого кандидата
    else ncs_ = PT_NEAR845_OLD ? sprops.ninst : spGridCollect(PROP_TOON_D, e3, 0, &cs_);   // №845: стейтпропы у глаза — из сетки
  }
  if (haveSP) for (unsigned j_ = 0; j_ < ncs_ && np < tMaxP; j_++) { unsigned i = cs_ ? cs_[j_] : j_;
    SpRun *r = &sprops.run[i];
    if (!propInkWanted(propInk1138Old, ptIsBooth(&sprops, &sprops.type[sprops.inst[i].type]))) continue;   // №1138: у предметов контура нет, только у будки
    if (!PT_WRSPR844_OLD && wrSprOn() && wrSprInst(i)) continue;   // №844: ключ рисуется спрайтом (кадры модели) — оболочка 3D-модели (крутится плавно) отставала от кадров спрайта бледным следом
    float x = pbS.on ? pbS.px[i] : r->x, z = pbS.on ? pbS.pz[i] : r->z, dx = x - ex, dz = z - ez;
    if (dx * dx + dz * dz > D2 || !r->alive) continue;
    const SpType *T = &sprops.type[sprops.inst[i].type];
    if (T->kind != 0 || 2.f * T->h[1] > tH || fmaxf(T->h[0], T->h[2]) > tR) continue;
    if (!spDrawable(&sprops, i, ex, ez, PROP_TOON_D)) continue;
    float c[3], rr; spBounds(&sprops, i, c, &rr);
    if (rfSphereOut((const float (*)[3])pl, e3, c, rr)) continue;
    float cx = c[0] - ex, cy = c[1] - ey, cz = c[2] - ez, d2 = cx * cx + cy * cy + cz * cz;
    if (rr <= 2.5f && clipGuardSphere(ex, ey, ez, c[0], c[1], c[2], rr, camNearSphere(ex, ey, ez, c[0], c[1], c[2], rr))) continue;   // тела у глаза нет — и оболочки
    { float r_ = rfSpRadius(sprops.inst[i].type), k_ = r_ * SP_FOCAL_PX; if (spMinPx > 0.f && r_ >= 0.f && k_ * k_ < spMinPx * spMinPx * d2) continue; }   // мелочь, которую не рисуем
    float dist = sqrtf(d2), W = PROP_TOON_K * skinToonWorldW(dist); if (W <= 0.f) continue;
    unsigned ink = skinToonAirMix(skinInkAbgr(INK_PROP), dist) & 0xffffffu; int got = 0;
    if (T->nJoint) { spPose(&sprops, i); if (!r->poseValid) continue; }
    if (!on) { texEnable(0); bound = -2; sceGuEnable(GU_FOG); fogK = -1.f; on = 1; }
    for (unsigned k = 0; k < T->nPiece && np < tMaxP; k++) {
      if (!spPieceOn(&sprops, i, k)) continue;
      const SpPiece *pp = &sprops.piece[T->firstPiece + k];
      if (!pp->ni || (pp->flags & (2 | F_ADDITIVE))) continue;
      float m[16]; spPieceMatrix(&sprops, i, k, m);
      if (ptHull((const char *)sprops.vertBase + pp->vOff, (const unsigned short *)((const char *)sprops.idxBase + pp->iOff), (int)pp->ni, pp->nv, pp->flags & (0xFF | F_ADDITIVE), m, W, tone ? (skinToonAirMix(ptPieceInk(pp->tex, skinInkAbgr(INK_PROP)), dist) & 0xffffffu) : ink)) { np++; got = 1; }
    }
    ptInst += (unsigned)got; if (tone && got && (2.f * T->h[1] > PROP_TOON_H || fmaxf(T->h[0], T->h[2]) > PROP_TOON_R)) ptToneTall++; ptLog(1, i, c[0], c[1], c[2], 2.f * T->h[1], fmaxf(T->h[0], T->h[2]), got);
  }
  if (on) { curFlags = -1; fogBase(); if (!groundCoverFog(&groundCover) && !fogCityOld) sceGuDisable(GU_FOG); resetState(); }   // параметры дымки — обратно, как после carToonHull
  ptDraws += (unsigned)np; ptFrames++;
  if (smkOn) { ptUs += sceKernelGetSystemTimeLow() - t0; static unsigned tl_; unsigned now_ = sceKernelGetSystemTimeLow();
    if (now_ - tl_ > 1000000u && ptFrames) { tl_ = now_; char l_[160]; snprintf(l_, sizeof l_, "PROPTOON t=%.1f inst=%.1f pieces=%.1f verts=%.0f us=%.0f tone=%d tall=%.1f (на кадр)\n", smkT, (float)ptInst / ptFrames, (float)ptDraws / ptFrames, (float)ptVerts / ptFrames, (float)ptUs / ptFrames, ptTone951, (float)ptToneTall / ptFrames); smkWrite(l_);
      ptInst = ptDraws = ptVerts = ptUs = ptFrames = 0; ptToneTall = 0; } }
}
