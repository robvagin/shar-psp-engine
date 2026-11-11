// №1261 (Роб 09.11): мягкое векторное свечение для дверей-входов и интерактивных объектов, как в оригинале (кадры PS2 от Публичного-93: build/ps2_glow_1261, L1).
// Что видно у PS2: у двери дома — светло-фиолетовый зубчатый «взрыв» по контуру двери, пульсирует размером и яркостью, виден уже с 20–30 м; у входа Kwik-E-Mart — прозрачная
// фиолетовая арка во весь проём (в минимуме пульса почти пропадает); у будки-телефона — синий диск на земле у основания и ореол над будкой (плавающая трубка).
// Здесь всё процедурно, треугольники с цветом и прозрачностью по вершинам (без текстур диска), одним вызовом на кадр, в той же пачке билбордов, что свет таблицей (main_loop.h, BBT 5):
//  • дверь: данные комнат (rooms.door, объём-триггер); широкий проём (≥ 2,4 м) — арка, узкий — зубчатый «взрыв» лицом к камере;
//  • будка: world.phone (точка и радиус триггера) — диск у земли + мягкий ореол над будкой.
// Пульс: период GLOW1261_PERIOD с, яркость 0…1, размер ±6 %. Дальность GLOW1261_RANGE м (с запасом 10 м плавное угасание). GLOW1261_OLD=1 / файл glow1261old / «flag glow1261old 1» — как было (без свечения).
#ifndef GLOW1261_H
#define GLOW1261_H
#include "glintshape941.h"
#ifndef GLOW1261_OLD
#define GLOW1261_OLD 0
#endif
#define GLOW1261_RANGE 55.f
#define GLOW1261_PERIOD 1.6f
#define GLOW1261_ARCH_W 3.0f   // полуширина триггера, м: ≥ — арка (Kwik-E-Mart 4,07), меньше — «взрыв» (дом Симпсонов 1,77: триггер шире самой двери)
static int glow1261On = !GLOW1261_OLD; static float glow1261Phase = -1.f;   // >= 0 — фаза пульса зафиксирована (служебно: flag glowphase, гейт кадрами)
static unsigned glow1261Quads, glow1261Draws, glow1261Doors, glow1261Booths;
typedef struct { unsigned c; float x, y, z; } G61V;
#define G61_SMALL __attribute__((optimize("Os"), noinline))   // свечение — холодный код (раз в кадр, 2–3 двери): размер важнее скорости, кэш команд 16 КиБ
static inline unsigned g61Col(float r, float g, float b, float a) {
  if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
  return ((unsigned)(a * 255.f + .5f) << 24) | ((unsigned)(b * 255.f + .5f) << 16) | ((unsigned)(g * 255.f + .5f) << 8) | (unsigned)(r * 255.f + .5f);
}
// «взрыв» лицом к камере = блик ключа (№941, glintshape941.h): ТА ЖЕ функция контура gsOutline — лучи с острыми внешними кончиками, стороны между кончиками — гладкие вогнутые дуги Безье (внутренние углы, впадины, скруглены).
// Слово Роба 09.11: «у нас уже есть готовый блик на гаечном ключе — повтори его». Отличие только в числе лучей (10 по контуру двери вместо 4–5) и в их неравенстве: углы, длины лучей псевдослучайные, но один и тот же узор на всех кадрах и дверях.
// Заливка от прозрачного ядра к краю, светлый мягкий ободок, ореол наружу в ноль. X, Y — оси плоскости (полуразмеры), A — яркость пульса
#define G61_RAYS 10
#define G61_SEG 5
#define G61_N (G61_RAYS * G61_SEG)
static float g61Px[G61_N], g61Py[G61_N]; static int g61Ready;
G61_SMALL static void g61Shape(void) {
  if (g61Ready) return;
  unsigned seed = 0x6d2b79f5u; static GsDef def; float gap[G61_RAYS], sum = 0.f;
#define RND() (seed = seed * 1664525u + 1013904223u, (float)(seed >> 8) / 16777216.f)
  for (int k = 0; k < G61_RAYS; k++) { gap[k] = .65f + .70f * RND(); sum += gap[k]; }   // неравный шаг лучей
  float a = .35f; def.n = G61_RAYS; def.k = .50f;   // k как у блика ключа (.5): впадина на ≈70 % радиуса кончика, дуга вогнута, без излома
  for (int k = 0; k < G61_RAYS; k++) { def.ang[k] = a; def.len[k] = .74f + .26f * RND(); a += gap[k] / sum * 6.2831853f; }   // длина луча .74…1.0
#undef RND
  int m = gsOutline(&def, G61_SEG, 1.f, g61Px, g61Py); (void)m;
  g61Ready = 1;
}
G61_SMALL static G61V *g61Put(G61V *o, unsigned col, const float c[3], const float X[3], const float Y[3], float rr, int i) {   // вершина контура i на доле радиуса rr (общая для всех полос, чтобы не раскрывать 12 копий в цикле)
  float u_ = g61Px[i] * rr, v_ = g61Py[i] * rr;
  o->c = col; o->x = c[0] + X[0] * u_ + Y[0] * v_; o->y = c[1] + X[1] * u_ + Y[1] * v_; o->z = c[2] + X[2] * u_ + Y[2] * v_; return o + 1;
}
G61_SMALL static unsigned g61Burst(G61V *o, const float c[3], const float X[3], const float Y[3], float A, float rgb[3]) {
  g61Shape();
  G61V *o0 = o;
  // четыре полосы: ядро (прозрачное) → заливка → светлый ободок → контур → ореол наружу в ноль; у каждой полосы цвет и доля радиуса внутренней и внешней кромки
  const unsigned col[5] = {g61Col(rgb[0], rgb[1], rgb[2], .20f * A), g61Col(rgb[0], rgb[1], rgb[2], .68f * A), g61Col(.93f, .88f, 1.f, .80f * A), g61Col(rgb[0], rgb[1], rgb[2], .34f * A), g61Col(rgb[0], rgb[1], rgb[2], 0.f)};
  static const float rad[5] = {.30f, .78f, .96f, 1.04f, 1.20f};
  for (int i = 0; i < G61_N; i++) { int j = (i + 1) % G61_N;
    for (int b = 0; b < 4; b++) {   // два треугольника полосы: A(i) B(i) B(j) и A(i) B(j) A(j)
      unsigned ca = col[b], cb = col[b + 1]; float ra = rad[b], rb = rad[b + 1];
      o = g61Put(o, ca, c, X, Y, ra, i); o = g61Put(o, cb, c, X, Y, rb, i); o = g61Put(o, cb, c, X, Y, rb, j);
      o = g61Put(o, ca, c, X, Y, ra, i); o = g61Put(o, cb, c, X, Y, rb, j); o = g61Put(o, ca, c, X, Y, ra, j);
    }
  }
  return (unsigned)(o - o0);
}
// диск на земле: центр → яркое кольцо (.85 радиуса) → край в ноль; плоскость XZ
G61_SMALL static unsigned g61Disc(G61V *o, const float c[3], float R, float A, float rgb[3]) {
  enum { N = 24 };
  unsigned n = 0; unsigned cC = g61Col(rgb[0], rgb[1], rgb[2], .16f * A), cR = g61Col(rgb[0], rgb[1], rgb[2], .62f * A), cE = g61Col(rgb[0], rgb[1], rgb[2], 0.f);
  static const unsigned char ord[9][2] = {{0, 0}, {1, 0}, {1, 1}, {1, 0}, {2, 0}, {2, 1}, {1, 0}, {2, 1}, {1, 1}};   // вершина: (кольцо 0 центр / 1 яркое .82 / 2 край, угол a0 или a1)
  for (int i = 0; i < N; i++) { float a0 = 6.2831853f * (float)i / (float)N, a1 = 6.2831853f * (float)(i + 1) / (float)N, cs[2] = {cosf(a0), cosf(a1)}, sn[2] = {sinf(a0), sinf(a1)};
    for (int k = 0; k < 9; k++) { int ring = ord[k][0], ai = ord[k][1]; float r = ring == 0 ? 0.f : (ring == 1 ? R * .82f : R);
      o[n].c = ring == 0 ? cC : (ring == 1 ? cR : cE); o[n].x = c[0] + cs[ai] * r; o[n].y = c[1]; o[n].z = c[2] + sn[ai] * r; n++; }
  }
  return n;
}
// мягкий шар лицом к камере (ореол): центр → край в ноль
G61_SMALL static unsigned g61Orb(G61V *o, const float c[3], const float X[3], const float Y[3], float R, float A, float rgb[3]) {
  enum { N = 14 };
  unsigned n = 0; unsigned cC = g61Col(rgb[0], rgb[1], rgb[2], .50f * A), cE = g61Col(rgb[0], rgb[1], rgb[2], 0.f);
  for (int i = 0; i < N; i++) {
    o[n].c = cC; o[n].x = c[0]; o[n].y = c[1]; o[n].z = c[2]; n++;
    for (int e = 0; e < 2; e++) { float a = 6.2831853f * (float)(i + e) / (float)N, ca_ = cosf(a), sa_ = sinf(a);
      o[n].c = cE; o[n].x = c[0] + (X[0] * ca_ + Y[0] * sa_) * R; o[n].y = c[1] + (X[1] * ca_ + Y[1] * sa_) * R; o[n].z = c[2] + (X[2] * ca_ + Y[2] * sa_) * R; n++; }
  }
  return n;
}
// трубка телефона над будкой (у PS2 плавает над будкой синей моделью с ореолом): тёмно-синий силуэт лицом к камере — перекладина и два блока на концах, повёрнут на 35°
G61_SMALL static unsigned g61Handset(G61V *o, const float c[3], const float X[3], const float Y[3], float s, float A) {
  const float ca = .8192f, sa = .5736f;   // cos/sin 35°
  static const float R[3][4] = {{-.34f, -.06f, .34f, .06f}, {-.40f, -.20f, -.22f, .20f}, {.22f, -.20f, .40f, .20f}};   // x0 y0 x1 y1 каждого блока (перекладина и два конца)
  unsigned n = 0, col = g61Col(.06f, .10f, .55f, .95f * A);
  for (int b = 0; b < 3; b++) {
    float px[4][2] = {{R[b][0], R[b][1]}, {R[b][2], R[b][1]}, {R[b][2], R[b][3]}, {R[b][0], R[b][3]}};
    static const int ix[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) { float u = px[ix[k]][0] * s, v = px[ix[k]][1] * s, ru = u * ca - v * sa, rv = u * sa + v * ca;
      o[n].c = col; o[n].x = c[0] + X[0] * ru + Y[0] * rv; o[n].y = c[1] + X[1] * ru + Y[1] * rv; o[n].z = c[2] + X[2] * ru + Y[2] * rv; n++; }
  }
  return n;
}
G61_SMALL static void glow1261Draw(float ex, float ey, float ez, int inR) {
  if (!glow1261On || inR) return;
  unsigned nd = (unsigned)rooms.ndoor, nb = haveCol ? (unsigned)world.nphone : 0u;
#if !SHR_REL_SMK   // журнал свечения для смоука: в релизе без смоука не нужен
  if (smkOn) { static float lg_; if (smkT - lg_ >= 3.f || smkT < lg_) { lg_ = smkT; char s_[240]; snprintf(s_, sizeof s_, "GLOW1261 t=%.1f nd=%u nb=%u doors=%u booths=%u quads=%u\n", smkT, nd, nb, glow1261Doors, glow1261Booths, glow1261Quads); smkWrite(s_);
      for (unsigned k = 0; k < nd && k < 6; k++) { const RoomDoor *d_ = &rooms.door[k]; snprintf(s_, sizeof s_, "GLOW1261D %s p=%.1f,%.1f,%.1f kind=%d c=%.1f,%.1f,%.1f half=%.2f,%.2f,%.2f\n", d_->name, d_->p[0], d_->p[1], d_->p[2], d_->trig.kind, d_->trig.c[0], d_->trig.c[1], d_->trig.c[2], d_->trig.half[0], d_->trig.half[1], d_->trig.half[2]); smkWrite(s_); }
      for (unsigned k = 0; k < nb && k < 4; k++) { snprintf(s_, sizeof s_, "GLOW1261B %s p=%.1f,%.1f,%.1f r=%.2f\n", world.phone[k].name, world.phone[k].p[0], world.phone[k].p[1], world.phone[k].p[2], world.phone[k].r); smkWrite(s_); } glow1261Doors = glow1261Booths = glow1261Quads = 0; } }
#endif
  if (!nd && !nb) return;
  float t = (float)(sceKernelGetSystemTimeLow() % 1000000000u) * 1e-6f, ph = glow1261Phase >= 0.f ? glow1261Phase : 6.2831853f * t / GLOW1261_PERIOD;
  float A0 = .5f + .5f * cosf(ph), sz = 1.f + .06f * sinf(ph);   // яркость 0…1 (у PS2 в минимуме пульса свечение почти исчезает), размер ±6 %
  const float R2 = (GLOW1261_RANGE + 10.f) * (GLOW1261_RANGE + 10.f);
  unsigned b0 = ggVbUsed, cap = 4400; G61V *q = (G61V *)ggVbAlloc(cap * sizeof(G61V)); if (!q) return;
  unsigned nv = 0, nvD = 0; float e3[3] = {ex, ey, ez};
  for (unsigned k = 0; k < nd; k++) {
    const RoomDoor *d = &rooms.door[k];
    float c[3] = {d->p[0], d->p[1] + 1.1f, d->p[2]};   // точка локатора двери (центр триггера уходит вглубь комнаты)
    float dx = c[0] - ex, dz = c[2] - ez, d2 = dx * dx + dz * dz; if (d2 > R2) continue;
    { float l_ = sqrtf(d2); if (l_ > 2.5f) { c[0] -= dx / l_ * 1.4f; c[2] -= dz / l_ * 1.4f; dx = c[0] - ex; dz = c[2] - ez; d2 = dx * dx + dz * dz; } }   // плоскость свечения на 1,4 м к камере от локатора: иначе наклонная плоскость уходит за стену и половина «взрыва» не видна (глубина проверяется)
    float dist = sqrtf(d2), f = dist <= GLOW1261_RANGE ? 1.f : (GLOW1261_RANGE + 10.f - dist) / 10.f; if (f <= 0.f) continue;
    float wide = d->trig.kind == 1 ? fmaxf(d->trig.half[0], d->trig.half[2]) : (d->trig.kind == 0 ? d->trig.half[0] : 0.5f);   // полуширина проёма по триггеру
    float hwid = wide < 0.5f ? 0.5f : (wide > 6.f ? 6.f : wide);
    if (rfSphereOut((const float (*)[3])pl, e3, c, 4.f)) continue;
    float rgb[3] = {.66f, .56f, 1.f};   // лиловый (по пикселям PS2 средний цвет ореола ≈ (179,168,234) вместе с белым ободком)
    float xl = sqrtf(dx * dx + dz * dz); float Xc[3] = {xl > 1e-3f ? dz / xl : 1.f, 0.f, xl > 1e-3f ? -dx / xl : 0.f};   // ось «вправо» лицом к камере вокруг вертикали
    if (nv + 1500 > cap) break;
    if (hwid >= GLOW1261_ARCH_W) {   // широкий проём (магазин): арка в плоскости двери — лицом к камере (плоскость двери не нужна: вид снаружи)
      float X[3] = {Xc[0], 0.f, Xc[2]}, hw_ = hwid * .36f; hw_ = hw_ < 1.1f ? 1.1f : (hw_ > 1.8f ? 1.8f : hw_); c[1] = d->p[1] + 1.5f;   // арка по размеру стеклянной двери, а не триггера
      float Xb = hw_ * 2.7f; Xb = Xb < 3.6f ? 3.6f : (Xb > 5.f ? 5.f : Xb);   // №1261 v5: широкий вход — тот же блик ключа, что у дома, чуть шире по размеру стеклянной двери (Роб: «сделай тем же бликом Kwik-E-Mart вместо арки»)
      float Xs[3] = {Xc[0] * Xb * sz, 0.f, Xc[2] * Xb * sz}, Ys[3] = {0.f, 3.6f * sz, 0.f}; (void)X; nv += g61Burst(q + nv, c, Xs, Ys, A0 * f, rgb); glow1261Quads += 2; }
    else {
      float X[3] = {Xc[0] * 3.6f * sz, 0.f, Xc[2] * 3.6f * sz}, Y[3] = {0.f, 3.5f * sz, 0.f}; nv += g61Burst(q + nv, c, X, Y, A0 * f, rgb); glow1261Quads += 2; }   // у PS2 «взрыв» ≈ 3,4 × 3,5 м
    glow1261Doors++;
  }
  nvD = nv;   // дальше — будки (сложение), раньше — двери (обычное смешивание)
  for (unsigned k = 0; k < nb; k++) {
    const ColWrench *P = &world.phone[k];
    float dx = P->p[0] - ex, dz = P->p[2] - ez, d2 = dx * dx + dz * dz; if (d2 > R2) continue;
    float dist = sqrtf(d2), f = dist <= GLOW1261_RANGE ? 1.f : (GLOW1261_RANGE + 10.f - dist) / 10.f; if (f <= 0.f) continue;
    float c[3] = {P->p[0], P->p[1], P->p[2]};
    if (rfSphereOut((const float (*)[3])pl, e3, c, 3.f)) continue;
    if (nv + 400 > cap) break;
    float rgb[3] = {.22f, .50f, 1.f};   // синий, как диск у PS2
    float gy = c[1]; if (haveCol) { float g_; if (colGround(&world, c[0], c[2], c[1] + 1.5f, c[1] - 6.f, &g_, NULL)) gy = g_; }
    float cd[3] = {c[0], gy + .06f, c[2]};
    nv += g61Disc(q + nv, cd, 1.45f * sz, A0 * f * 1.1f, rgb);
    float xl = sqrtf(dx * dx + dz * dz); float Xc[3] = {xl > 1e-3f ? dz / xl : 1.f, 0.f, xl > 1e-3f ? -dx / xl : 0.f}, Yc[3] = {0.f, 1.f, 0.f};
    float co[3] = {c[0], gy + 2.35f + .08f * sinf(ph * 1.5f), c[2]};
    float rgb2[3] = {.55f, .62f, 1.f};
    nv += g61Orb(q + nv, co, Xc, Yc, 1.05f * sz, A0 * f * .9f, rgb2); nv += g61Handset(q + nv, co, Xc, Yc, 1.1f * sz, f);
    glow1261Booths++; glow1261Quads += 2;
  }
  if (!nv) return;
  ggVbWriteback(b0);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND);
  sceGuDepthMask(GU_TRUE); setCull(0);
  unsigned nDoor = nv; (void)nvD;   // всё обычным смешиванием: цветное свечение поверх сцены (сложение на сером асфальте даёт бледное белое)
  if (nDoor) { sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)nDoor, 0, q); }
  sceGuEnable(GU_TEXTURE_2D); resetState(); glow1261Draws++;
}
#endif
