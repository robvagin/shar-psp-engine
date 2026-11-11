// Опора-93: гейт пощады на последнем ударе (DMGMERCY_OLD). Инспектор, раздел 4 «механика», урон машины: проверка чисел порта против эталона Данных-93 (docs/state/inspector_ref/mechanics.json).
// Включает car.c целиком (статические функции dmgApply, addDamageK, effHP). Печатает строки NB (раздел 4, уровень "-").
#include <stdarg.h>
#include "../../viewer/car.c"
static int bad;
static void nb(const char *item, int ok, const char *fmt, ...) { char b[300]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); printf("%s %s: %s\n", ok ? "PASS" : "FAIL", item, b); bad += !ok; }
static Car mk(float hp, int player) { Car c; memset(&c, 0, sizeof c); c.p.hitPoints = hp; c.p.mass = 1500.f; c.svPlayer = player; c.huskDrive = 0; return c; }
static float closingFor(const Car *c, float D) { return D * 100000.f / (bodyMass(c) * (1.f + SV_DMG_E)); }
int main(void) {
  Car c = mk(3.f, 0); float cl;
  cl = closingFor(&c, 0.06f); addDamageK(&c, cl * 0.999f, 0.f, 1.f); int below = c.damage == 0.f;
  c = mk(3.f, 0); cl = closingFor(&c, 0.061f); addDamageK(&c, cl, 0.f, 1.f); int above = c.damage > 0.f;
  nb("порог урона от удара 0,06", below && above, "доля 0,06: урона %.4f (ждём 0); доля 0,061: урон %.4f (ждём больше 0)", below ? 0.f : 1.f, c.damage);
  c = mk(3.f, 0); cl = closingFor(&c, 0.3f); addDamageK(&c, cl, 0.f, 1.f);
  nb("сколько очков отнимает удар (доля / прочность)", fabsf(c.damage - 0.3f / 3.f) < 1e-3f, "удар 0,3 на прочности 3,0: damage %.4f (ждём 0,1000)", c.damage);
  { Car p = mk(3.f, 1); float s[4]; int boomAt = 0; for (int k = 0; k < 4; k++) { cl = closingFor(&p, 1.0f); p.noDmgT = 0.f; addDamageK(&p, cl, 0.f, 1.f); s[k] = p.damage; if (p.destroyed && !boomAt) boomAt = k + 1; }
    nb("сильнейшие удары: прочность 3,0 переживает три, взрыв на четвёртом", fabsf(s[0] - 1.f / 3.f) < 1e-3f && fabsf(s[1] - 2.f / 3.f) < 1e-3f && fabsf(s[2] - 0.999f) < 1e-3f && boomAt == 4, "damage по ударам %.3f %.3f %.3f %.3f, разбита на ударе %d (ждём 4)", s[0], s[1], s[2], s[3], boomAt); }
  { Car p = mk(0.5f, 1); int boomAt = 0; float s[3]; for (int k = 0; k < 3; k++) { cl = closingFor(&p, 1.0f); p.noDmgT = 0.f; addDamageK(&p, cl, 0.f, 1.f); s[k] = p.damage; if (p.destroyed && !boomAt) boomAt = k + 1; }
    nb("прочность 0,5: взрыв на втором ударе", boomAt == 2, "damage %.3f %.3f, разбита на ударе %d (ждём 2)", s[0], s[1], boomAt); }
  { Car p = mk(3.f, 1); p.damage = .9f; cl = closingFor(&p, 1.0f); addDamageK(&p, cl, 0.f, 1.f); int m1 = fabsf(p.damage - .999f) < 1e-3f && p.noDmgT >= .99f; float d1 = p.damage; addDamageK(&p, cl, 0.f, 1.f);
    nb("пощада: удар за 98 % оставляет 0,001 очка и окно 1 с без урона", m1 && p.damage == d1, "после удара damage %.4f noDmgT %.2f, второй удар в окне: damage %.4f (ждём без изменений)", d1, p.noDmgT, p.damage); }
  { Car p = mk(3.f, 1), o = mk(3.f, 0); p.damage = o.damage = .5f; p.noDmgT = o.noDmgT = 0.f; dmgApply(&p, 1.f, 1.f); dmgApply(&o, 1.f, 1.f);   // уже разбитые: время взрыва
    Car p2 = mk(0.3f, 1), o2 = mk(0.3f, 0); p2.damage = 0.999f; o2.damage = 0.999f; p2.noDmgT = o2.noDmgT = 0.f; p2.destroyed = o2.destroyed = 0;
    dmgApply(&p2, 1.f, 1.f); dmgApply(&o2, 1.f, 1.f);
    nb("взрыв: у машины игрока через 3,0 с, у остальных сразу", p2.destroyed && o2.destroyed && fabsf(p2.boomDelay - 3.f) < .01f && o2.boomDelay == 0.f, "игрок boomDelay %.2f (ждём 3,0), остальные %.2f (ждём 0)", p2.boomDelay, o2.boomDelay); }
  { Car n = mk(3.f, 0); n.x = 10.f; SvBoom b; memset(&b, 0, sizeof b); b.radius = 20.f; b.force = 20000.f; carBoomHit(&n, &b, 0); float near_ = n.damage; Car f = mk(3.f, 0); f.x = 30.f; carBoomHit(&f, &b, 0); Car p = mk(3.f, 1); p.x = 10.f; carBoomHit(&p, &b, 1);
    nb("взрыв соседа: радиус 20 м, отнимает 0,5 очка, игрок тоже", fabsf(near_ - 0.5f / 3.f) < 1e-3f && f.damage == 0.f && fabsf(p.damage - 0.5f / 3.f) < 1e-3f, "в 10 м: damage %.4f (ждём 0,1667); в 30 м: %.4f (ждём 0); игрок в 10 м: %.4f", near_, f.damage, p.damage); }
  return bad ? 1 : 0;
}
