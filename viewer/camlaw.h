// Законы камеры, сверенные с оригиналом (окно g06-camera, круг 6, 02.11.2026): чистые функции без игры вокруг, чтобы проверять на Маке (tests/camera/sverka_g06_host.c).
// Поведение и числа — своими словами; код свой. Каждая функция помечена строкой сверки и выключателем в вызывающем файле (*_SV_OLD / №).
#ifndef CAMLAW_H
#define CAMLAW_H
#include <math.h>

// Шаг «кубической» подгонки значения p со скоростью r к цели g за долю I (0..1): кривая Эрмита от (p, r) к (g, 0), скорость на конце нуль.
// Так оригинал подтягивает всё: курс, дальность, взгляд, переходы между камерами. I за кадр = запаздывание × (мс кадра / 16). Возвращает новое p, r обновляет
static inline float camHermite(float p, float *r, float g, float I) {
  float I2 = I * I, I3 = I2 * I, r0 = *r;
  float h00 = 2.f * I3 - 3.f * I2 + 1.f, h10 = I3 - 2.f * I2 + I, h01 = -2.f * I3 + 3.f * I2;
  float d00 = 6.f * I2 - 6.f * I, d10 = 3.f * I2 - 4.f * I + 1.f, d01 = -6.f * I2 + 6.f * I;
  *r = d00 * p + d10 * r0 + d01 * g;
  return h00 * p + h10 * r0 + h01 * g;
}

// ---- крен камеры пешком (СВЕРКА_КАМЕРА P08:20; №1084 и круг 6) ----
// Оригинал: крен-цель 0,176 рад × близость персонажа (0..1) на уровне 7, пока настоящая скорость Гомера (вектор целиком, с вертикалью) меньше трети максимальной скорости
// бега; иначе цель 0. К цели крен идёт тем же кубическим шагом с запаздыванием 0,01 за кадр в обе стороны (не рывком вверх и не медленнее вниз):
// 63% примерно за 1,3 с. Здесь тот же отклик вторым порядком (ω 1,5, ζ 0,8): подобран по тому же правилу, что остальные фильтры камеры
#define CAMLAW_TWIST_MAX .176f
#define CAMLAW_RUN 8.f         // максимальная скорость бега Гомера, м/с (HOMER_RUN): порог крена — треть
#define CAMLAW_TWIST_W 1.5f
static inline float camTwistWant(int level, float close, float spd3) { return level == 7 && close > 0.f && spd3 < CAMLAW_RUN / 3.f ? CAMLAW_TWIST_MAX * close : 0.f; }
static inline void camTwistStep(float *cur, float *vel, float want, float dt) {
  int n = (int)(dt / .004f) + 1; float h = dt / (float)n, e = *cur - want, v = *vel;
  for (int i = 0; i < n; i++) { v += (-CAMLAW_TWIST_W * CAMLAW_TWIST_W * e - 2.f * .8f * CAMLAW_TWIST_W * v) * h; e += v * h; }
  *cur = want + e; *vel = v;
}

// ---- автозум пешком на персонажа миссии (СВЕРКА_КАМЕРА 2.9, 34) ----
// Оригинал: ближайший ВИДИМЫЙ не-прохожий персонаж в 5 м (проверка раз в 1,5 с; видимость — пирамида кадра с узким тестовым углом 0,567 рад по горизонтали, шар радиуса 1 м,
// стены не в счёт); близость pct = 1 − расстояние/5 каждый кадр. Угол кадра пешком (по горизонтали, данные: 70°) уменьшается НА 0,49 рад × pct (до 42°, не до 28°), но не меньше 20°;
// к цели идёт с запаздыванием 0,022 за кадр
#define CAMLAW_WALK_FOV_MAX 1.2222f
#define CAMLAW_WALK_FOV_MIN .3487f
#define CAMLAW_PED_ZOOM .49f
#define CAMLAW_PED_FOV .567424f
#define CAMLAW_PED_W 3.3f
static inline float camWalkZoomWant(float pct) { float f = CAMLAW_WALK_FOV_MAX - CAMLAW_PED_ZOOM * pct; return f < CAMLAW_WALK_FOV_MIN ? CAMLAW_WALK_FOV_MIN : f; }
static inline float camWalkZoomK(float fov) { return tanf(fov * .5f) / tanf(CAMLAW_WALK_FOV_MAX * .5f); }   // множитель tan(FOV/2) кадра относительно обычного
// p — точка персонажа, e — глаз, l — точка взгляда; пирамида: по горизонтали и вертикали (кадр 4:3), шар 1 м добавляет угол asin(1/расстояние)
static inline int camWalkPedSeen(const float e[3], const float l[3], const float p[3]) {
  float f[3] = {l[0] - e[0], l[1] - e[1], l[2] - e[2]}, q[3] = {p[0] - e[0], p[1] - e[1], p[2] - e[2]};
  float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]), ql = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
  if (fl < 1e-4f || ql < 1e-4f) return 1;
  for (int k = 0; k < 3; k++) f[k] /= fl;
  float rx = f[2], rz = -f[0], rl = sqrtf(rx * rx + rz * rz);
  if (rl < 1e-4f) return 1;
  rx /= rl; rz /= rl;
  float u[3] = {f[1] * rz, f[2] * rx - f[0] * rz, -f[1] * rx};   // вверх кадра = f x r (r горизонтален: (rx, 0, rz))
  float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]); if (ul > 1e-4f) { u[0] /= ul; u[1] /= ul; u[2] /= ul; }
  float z = q[0] * f[0] + q[1] * f[1] + q[2] * f[2], x = q[0] * rx + q[2] * rz, y = q[0] * u[0] + q[1] * u[1] + q[2] * u[2];
  float m = ql > 1.f ? asinf(1.f / ql) : 1.5707f, hh = CAMLAW_PED_FOV * .5f, hv = atanf(tanf(hh) * .75f);
  if (z <= 0.f) return m >= 1.5707f;
  return atanf(fabsf(x) / z) <= hh + m && atanf(fabsf(y) / z) <= hv + m;
}

// ---- наклон камеры пешком на склоне (СВЕРКА_КАМЕРА 2.6, 33) ----
// Угол над героем меньше на 0,3 рад, если нормаль земли под ногами круче 14° (y < 0,97); иначе на 0,2 рад, если стик камеры не тронут, верхний луч
// (от глаза вверх на 1,6 м и вперёд на ближнюю плоскость к точке над головой героя) свободен, а нижний (от точки у пояса героя назад на дальность + 0,5 м) закрыт
static inline float camSlopeWant(int steep, int still, int topFree, int botFree) { return steep ? .3f : (still && topFree && !botFree ? .2f : 0.f); }

// ---- тряска камеры (СВЕРКА_КАМЕРА 1.17, P08:1) ----
// 0,18 с; за 16 мс фаза растёт на 0,942 рад (58,9 рад/с), начинается с 0,87 рад и первое значение берётся уже после первого шага; размах 1,10 м линейно к нулю
#define CAMLAW_SHAKE_LEN .18f
#define CAMLAW_SHAKE_AMP 1.1f
static inline float camShakeAt(float t) { return t >= 0.f && t < CAMLAW_SHAKE_LEN ? CAMLAW_SHAKE_AMP * (1.f - t / CAMLAW_SHAKE_LEN) * sinf(.87f + 58.9f * (t + .016f)) : 0.f; }

// ---- камера заднего хода (СВЕРКА_КАМЕРА 1.11; P08:10) ----
// Включается, если машина не в воздухе и не опрокинута (устойчива), едет назад (тормоз) и v² > 45 (6,7 м/с) или есть контакт со стеной; выключается, когда не едет назад и газ ≥ 0,5.
static inline int camRevOn(int stable, int brake, int back, float v, int contact) { return stable && brake && back && (v * v > 45.f || contact); }
static inline int camRevOff(int back, float gas) { return !back && gas >= .5f; }
// Желаемое место: от текущего положения камеры; дальше дистанции включения — на дистанцию, высота = высоте включения над машиной. Ближе 7 м, если машина едет в сторону камеры и камера
// позади носа, — сдвиг вбок на (дистанция / 2) × запаздывание (0,05) за 16 мс (не рывок). d3 — камера минус машина, h — курс (x, z), dist — дистанция включения, yH — высота включения,
// v — скорость машины, out — желаемое место (смещение от машины), dt — с
static inline void camRevDesired(const float d3[3], float dist, float yH, const float v[3], float hx, float hz, float dt, float out[3]) {
  float s2 = d3[0] * d3[0] + d3[1] * d3[1] + d3[2] * d3[2];
  out[0] = d3[0]; out[1] = d3[1]; out[2] = d3[2];
  if (s2 > dist * dist) { float s = sqrtf(s2); for (int k = 0; k < 3; k++) out[k] = d3[k] / s * dist; out[1] = yH; }
  else if (s2 < 49.f) {
    float vd = v[0] * d3[0] + v[2] * d3[2], hd = hx * d3[0] + hz * d3[2];
    if (vd > 0.f && hd < 0.f) { float px = -hz, pz = hx, pl = sqrtf(px * px + pz * pz); if (pl > 1e-4f) { px /= pl; pz /= pl; }
      float sg = (px * d3[0] + pz * d3[2]) < 0.f ? -1.f : 1.f, st = dist * .5f * (.05f * dt * 60.f > 1.f ? 1.f : .05f * dt * 60.f);
      out[0] += px * sg * st; out[2] += pz * sg * st; }
  }
}

// ---- бампер (СВЕРКА_КАМЕРА 1.20, P08:8) ----
// Глаз над кузовом: вперёд от центра на половину колёсной базы минус 1,0 м (ближняя плоскость) = переднюю ось (от начала машины, wheelF) минус 1,0 м; высота верха коробки + 0,3 м
static inline float camBumperFwd(float wheelF) { return wheelF - 1.f; }

// ---- камера бонус-игры (№1109; СВЕРКА: камера «гонки на очки») ----
// Оригинал: камера неподвижная, на каждый уровень (трасса 1..7) своё место, цель (0, 0, 0), угол кадра по горизонтали (кадр 4:3), ближняя 150 (на трассе 6 — 250), дальняя 800;
// никакого сглаживания (камера просто стоит). Верх кадра — по обычному правилу «вертикаль мира наклонена к взгляду». Места даны в осях оригинала (z зеркален к нашим)
static const float CAMLAW_BONUS_POS[7][3] = {{26.88f, 157.87f, -206.23f}, {35.67f, 195.66f, -207.82f}, {-27.78f, 146.18f, -228.17f}, {32.92f, 305.26f, -292.99f},
                                             {26.73f, 128.08f, -208.55f}, {30.338f, 208.018f, -358.739f}, {30.62f, 157.05f, -213.99f}};
static const float CAMLAW_BONUS_FOV[7] = {.4f, .4f, .35f, .26f, .4f, .2618f, .4f};
// -> 1, если трасса 1..7. fovyDeg — вертикаль для 16:9 по тому же вертикальному полю, что у оригинала на 4:3 (как у трассы 1, измеренной на PS2); dist — расстояние до цели
static inline int camBonusTop(int track, float eye[3], float at[3], float up[3], float *fovyDeg, float *nearZ, float *farZ, float *dist) {
  if (track < 1 || track > 7) return 0;
  const float *e = CAMLAW_BONUS_POS[track - 1];
  float cz[3] = {-e[0], -e[1], -e[2]}, cx[3] = {cz[2], 0.f, -cz[0]};
  float cy[3] = {cz[1] * cx[2] - cz[2] * cx[1], cz[2] * cx[0] - cz[0] * cx[2], cz[0] * cx[1] - cz[1] * cx[0]}, l = sqrtf(cy[0] * cy[0] + cy[1] * cy[1] + cy[2] * cy[2]);
  eye[0] = e[0]; eye[1] = e[1]; eye[2] = -e[2]; at[0] = at[1] = at[2] = 0.f;
  up[0] = cy[0] / l; up[1] = cy[1] / l; up[2] = -cy[2] / l;
  *fovyDeg = 2.f * atanf(tanf(CAMLAW_BONUS_FOV[track - 1] * .5f) * .75f) * (180.f / 3.14159265f);
  *nearZ = track == 6 ? 250.f : 150.f; *farZ = 800.f; *dist = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
  return 1;
}
// Select в гонке: вид сверху (0) → те же виды, что в машине в городе (1 — вид города 0, 2 — вид города 2 (от первого лица, бампер), 3 — вид города 1), и снова сверху.
// cvNext — следующий вид города после текущего (g10bViewNext); когда город вернулся бы к виду 0, идём на вид сверху
static inline int camBonusCv(int cam) { return cam == 1 ? 0 : cam == 2 ? 2 : 1; }
static inline int camBonusOf(int cv) { return cv == 0 ? 1 : cv == 2 ? 2 : 3; }
static inline int camBonusNext(int cam, int cvNext) { if (cam == 0) return 1; if (cvNext == 0) return 0; return camBonusOf(cvNext); }

// ---- №1127: проезжающие машины и камера ----
// Чужая машина считается движущейся, если её скорость по земле больше CAMLAW_CARMOVE_V, м/с. От движущейся машины положение камеры не уворачивается (ни притяжением к опоре, ни срезом на
// идеальное место): в оригинале линия «машина → камера» проверяется только по статике мира, а сфера камеры отталкивается от машины с запаздыванием 0,2 за 16 мс (рывка нет); у нас тело
// движущейся машины видит только ближняя плоскость (закон Роба: ничего не обрезается у краёв кадра).
#define CAMLAW_CARMOVE_V 1.0f
static inline int camCarMoving(float vx, float vz) { return vx * vx + vz * vz > CAMLAW_CARMOVE_V * CAMLAW_CARMOVE_V; }
// Ближняя плоскость при движущихся машинах рядом: сужается не быстрее, чем в CAMLAW_NEAR_FALL раз за 1/30 с (быстрее, чем машина успевает войти в кадр: 20 м/с — 0,67 м за кадр),
// расширяется как раньше (CS_NEAR_RISE). prev — прошлый кадр, want — желаемая, dt — с; не уже minN
#define CAMLAW_NEAR_FALL 0.6f
static inline float camCarNearStep(float prev, float want, float dt, float minN) {
  float lo = prev * powf(CAMLAW_NEAR_FALL, dt * 30.f);
  float v = want < lo ? lo : want;
  return v < minN ? minN : v;
}

#endif
