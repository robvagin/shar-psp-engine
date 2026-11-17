// Эффект цели миссии SetCollectibleEffect — описание в collectfx.h. Чистый C, без GU (рисует fx.c через fxSprite).
#include <string.h>
#include <math.h>
#include "collectfx.h"
#include "collectfx_data.h"
#include "fx.h"

#define CFX_MAX 6           // одновременно горящих (точки гонки идут через 1–2 с, квады гаснут за 0.37 с)
typedef struct { const CfxDef *d; float t, p[3]; } CfxRun;
static CfxRun run[CFX_MAX];
int cfxOld;
// 1340 (Роб, 108 на PSP: «у предметов миссии, которые собираешь, нет блестяшки при подборе, блинк-блинк, как у гаечного ключа»): в оригинале у собираемой цели
// без SetCollectibleEffect, если у неё есть рисунок, в точке подбора играет эффект миссии по умолчанию mission_col (три квада свечения bluestarglow / halo + частицы
// mission_col_psShape, 0,7 с; эффект по умолчанию при подборе). У нас без команды эффекта не играло ничего (L1 m4 доставка, L2 m5 и m6 доставка, L3 m3 и m5 сброс и др.);
// «none» в скрипте — по-прежнему ничего. Эффект берётся из данных диска (tools/collectfx_export.py, missgen.p3d). CFX_DEFAULT1340_OLD=1 — как было
#ifndef CFX_DEFAULT1340_OLD
#define CFX_DEFAULT1340_OLD 0
#endif
int cfxDefault1340Old = CFX_DEFAULT1340_OLD;
unsigned cfxSprites;
// №463 (Роб; запись PS2, knowledge/facts/ps2-obs-video-m1-rings-wrench.yaml, В57): на въезде в точку этапа wrench_collect даёт не вспышку звёзд
// 0,37 с, а бледно-голубой полупрозрачный вертикальный столб со спиралью: растёт из машины, через 0,2 с 3-4 высоты машины, потом выше
// ~8 м (в порте 12 м), виден ~0,9 с. Столб — стопка мягких спрайтов glowsolid.bmp со спиральным сдвигом, поверх прежних звёзд.
// CFX_COLUMN_OLD=1 — как было (без столба)
#ifndef CFX_COLUMN_OLD
#define CFX_COLUMN_OLD 0
#endif
#define CFX_COL_MAX 3
#define CFX_COL_LIFE 0.9f
#define CFX_COL_H 12.f
typedef struct { float t, p[3]; int on; } CfxCol;
static CfxCol col[CFX_COL_MAX];

static const CfxDef *find(const char *name) {
  for (int i = 0; i < CFX_NDEF; i++) if (!strcmp(cfxDefs[i].name, name)) return &cfxDefs[i];
  return 0;
}

int cfxPlay(const char *name, const float pos[3]) {
  if (cfxOld || !name || !*name || !pos) return 0;
  const CfxDef *d = find(name); if (!d) return 0;   // "none" и неизвестные — ничего
  int n = 0;
  if (!CFX_COLUMN_OLD && !strcmp(name, "wrench_collect")) {
    int s = 0; float oldest = -1.f;
    for (int i = 0; i < CFX_COL_MAX; i++) { if (!col[i].on) { s = i; break; } if (col[i].t > oldest) { oldest = col[i].t; s = i; } }
    col[s].on = 1; col[s].t = 0.f; col[s].p[0] = pos[0]; col[s].p[1] = pos[1]; col[s].p[2] = pos[2];
  }
  for (int k = 0; k < d->nps; k++) {   // частицы — один раз (fx.c ведёт их сам до конца таймлайна системы)
    int id = fxFind(d->ps[k].sys); if (id < 0) continue;
    float p[3] = {pos[0] + d->ps[k].off[0], pos[1] + d->ps[k].off[1], pos[2] + d->ps[k].off[2]};
    if (fxBurst(id, p, 0)) n++;
  }
  if (d->nbill) {
    int s = 0; float oldest = -1.f;
    for (int i = 0; i < CFX_MAX; i++) { if (!run[i].d) { s = i; break; } if (run[i].t > oldest) { oldest = run[i].t; s = i; } }
    run[s].d = d; run[s].t = 0.f; run[s].p[0] = pos[0]; run[s].p[1] = pos[1]; run[s].p[2] = pos[2];
    n += d->nbill;
  }
  return n;
}

// №976 (Роб 25.10: «звёздочки спиралью как ДНК — красивая анимация, но слишком насыщенные: прозрачнее; доработать движение»; канон кинематики: скорость и фаза, не позиция, без рывков,
// мягкое появление и исчезание): прежний ход — высота линейно 25 м/с и обрыв на 12 м (излом скорости), звёзды появлялись в кадре, когда фронт проходил очередную ступень (щелчок),
// прозрачность линейными рампами. Теперь: скорость фронта плавно падает до нуля у вершины (h = H(1 − (1 − t/T)²)), каждая звезда входит и выходит по smoothstep (по расстоянию от
// фронта и по времени), радиус спирали раскрывается и закрывается мягко, общая прозрачность COL976_A. Фаза спирали — по-прежнему равномерная угловая скорость. GLINT976_OLD=1 — как было
#include "glintmotion976.h"
static void columnFrame(float dt) {
  int tex = fxTexFind("glowsolid.bmp");
  for (int i = 0; i < CFX_COL_MAX; i++) {
    CfxCol *c = &col[i]; if (!c->on) continue;
    float t = c->t, h, a, rad = .35f, k976 = 1.f;
    if (fxGlint976Old) { h = fminf(CFX_COL_H, 25.f * t); a = fminf(1.f, t / .1f) * (t > .65f ? fmaxf(0.f, (CFX_COL_LIFE - t) / (CFX_COL_LIFE - .65f)) : 1.f); }
    else { gm976Col(t, CFX_COL_H, CFX_COL_LIFE, &h, &a, &rad); k976 = COL976_A; }
    if (tex >= 0 && h > .2f && a > .01f) {
      float lvl = .42f * a; unsigned v = (unsigned)(lvl * 255.f + .5f);
      unsigned col32 = 0xff000000u | (v << 16) | ((unsigned)(lvl * .85f * 255.f + .5f) << 8) | (unsigned)(lvl * .6f * 255.f + .5f);   // abgr: бледно-голубой
      for (float y = 0.f; y < h + (fxGlint976Old ? 0.f : COL976_SOFT * .2f); y += .7f) {
        float ph = y * 1.8f - t * 9.f, sx = rad * cosf(ph), sz = rad * sinf(ph), ya = a;
        if (!fxGlint976Old) { ya = gm976Star(a, h, y); if (ya < .01f) continue; }
        if (fxGlowGlint && !GLOW_GLINT_OLD) { fxGlowGlint(c->p[0] + sx, c->p[1] + y, c->p[2] + sz, 1.05f, 0xFFFFD999u, ya * k976, y * .37f); cfxSprites++; continue; }   // №839: столб — бликами
        fxSprite(tex, c->p[0] + sx, c->p[1] + y, c->p[2] + sz, 1.5f, 1.5f, 0.f, col32, 1, 0);
        fxSprite(tex, c->p[0] - sx, c->p[1] + y + .35f, c->p[2] - sz, 1.5f, 1.5f, 0.f, col32, 1, 0);
        cfxSprites += 2;
      }
    }
    c->t += dt; if (c->t >= CFX_COL_LIFE) c->on = 0;
  }
}

void cfxFrame(float dt) {
  if (!CFX_COLUMN_OLD) columnFrame(dt);
  for (int i = 0; i < CFX_MAX; i++) {
    CfxRun *r = &run[i]; if (!r->d) continue;
    for (int k = 0; k < r->d->nbill; k++) {
      const CfxBill *b = &r->d->bill[k];
      float f = r->t * b->fps; int f0 = (int)f; if (f0 >= b->nfr - 1) continue;
      float u = f - f0; const float *A = b->tr[f0], *B = b->tr[f0 + 1]; float v[6];
      for (int c = 0; c < 6; c++) v[c] = A[c] + (B[c] - A[c]) * u;
      if (b->blend ? fmaxf(v[2], fmaxf(v[3], v[4])) <= .004f : v[5] <= .004f) continue;   // погасший квад не тратит заливку
      if (b->blend == 1 && fxGlowGlint && !GLOW_GLINT_OLD) {   // №839: звезда/вспышка сложением — векторным бликом
        unsigned c_ = 0; for (int c = 0; c < 3; c++) { float x = v[2 + c] * 255.f + .5f; c_ |= (x <= 0.f ? 0u : x >= 255.f ? 255u : (unsigned)x) << (8 * c); }
        fxGlowGlint(r->p[0] + b->off[0], r->p[1] + b->off[1], r->p[2] + b->off[2], fmaxf(v[0], v[1]) * .7f, c_, fxGlint976Old ? 1.f : COL976_A, (float)k * .53f); cfxSprites++; continue; }
      int tex = fxTexFind(b->tex); if (tex < 0) continue;
      unsigned c8[4]; for (int c = 0; c < 4; c++) { float x = v[2 + c] * 255.f + .5f; c8[c] = x <= 0.f ? 0u : x >= 255.f ? 255u : (unsigned)x; }
      fxSprite(tex, r->p[0] + b->off[0], r->p[1] + b->off[1], r->p[2] + b->off[2], v[0], v[1], 0.f,
               (c8[3] << 24) | (c8[2] << 16) | (c8[1] << 8) | c8[0], b->blend, 0);
      cfxSprites++;
    }
    r->t += dt;
    if (r->t >= r->d->dur) r->d = 0;
  }
}

void cfxReset(void) { memset(run, 0, sizeof run); memset(col, 0, sizeof col); }
int cfxActive(void) { int n = 0; for (int i = 0; i < CFX_MAX; i++) n += run[i].d != 0; for (int i = 0; i < CFX_COL_MAX; i++) n += col[i].on; return n; }
