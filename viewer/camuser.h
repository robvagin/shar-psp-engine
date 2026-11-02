// №1151: настройки камеры игрока (раздел CAMERA в паузе). Одно место для меню и камеры: меню пишет шесть значений (ступени ниже), камера читает их через зажим в безопасные пределы.
// Значения по умолчанию = нынешняя камера. Применение сразу (живой кадр под паузой): меню ставит camCfgSnap996 = 1 при смене — сглаживание камеры сбрасывается (main_loop.h).
// Закон объектива и ближняя плоскость (camsafe, camSafeEye, camSafeFrame) работают при любых значениях: глаз не входит в стены, ничего не обрезается у краёв кадра.
// CAMSET1151_OLD=1 — как до №1151 (высота и дальность из homerCamUserH/D на обе камеры, наклона и множителя угла нет).
// Чистые законы (хост-тест tests/camera/camset1151_host.c): зажим и плавное приближение наклона / множителя угла.
#ifndef SHAR_CAMUSER_H
#define SHAR_CAMUSER_H
#include <math.h>
#ifndef CAMSET1151_OLD
#define CAMSET1151_OLD 0
#endif
// Ступени меню (g09b-menu): высота 0,5 0,75 1 1,25 1,5; дальность 0,7 0,85 1 1,15 1,3; наклон −10 −5 0 5 10°; угол обзора ×0,85 0,92 1 1,08 1,15. Пешком — высота и дальность как у машины.
#define CAMU_H_LO .5f
#define CAMU_H_HI 1.5f
#define CAMU_D_LO .7f
#define CAMU_D_HI 1.3f
#define CAMU_TILT_MAX 10.f
#define CAMU_FOV_LO .85f
#define CAMU_FOV_HI 1.15f
#define CAMU_FOVY_MIN 30.f   // ° по вертикали: абсолютные пределы угла кадра (узкий — как автозум пешком, широкий — не шире края полосы GE)
#define CAMU_FOVY_MAX 80.f
#define CAMU_TILT_RATE 40.f  // °/с: плавное приближение наклона к заданному (без рывка; под паузой и при смене настроек — сразу)
#define CAMU_FOV_RATE 0.6f   // 1/с: то же для множителя угла
static float camUserCarH = 1.f, camUserCarD = 1.f, camUserCarTilt = 0.f, camUserCarFov = 1.f, camUserFootH = 1.f, camUserFootD = 1.f;
static float camUserTiltNow, camUserFovNow = 1.f;   // то, что камера применяет в этом кадре (плавно идёт к заданному)
static inline float camUserClamp(float v, float lo, float hi) { if (!(v == v)) v = 1.f; return v < lo ? lo : v > hi ? hi : v; }   // не число — 1 (по умолчанию), потом пределы
static inline float camUserH(float v) { return camUserClamp(v, CAMU_H_LO, CAMU_H_HI); }
static inline float camUserD(float v) { return camUserClamp(v, CAMU_D_LO, CAMU_D_HI); }
static inline float camUserTiltWant(float v) { return !(v == v) ? 0.f : camUserClamp(v, -CAMU_TILT_MAX, CAMU_TILT_MAX); }
static inline float camUserFovWant(float v) { return camUserClamp(v, CAMU_FOV_LO, CAMU_FOV_HI); }
// шаг приближения: now → want, не быстрее rate за секунду; dt = 0 или snap — сразу
static inline float camUserEase(float now, float want, float rate, float dt, int snap) {
  if (snap || dt <= 0.f) return want;
  float d = want - now, s = rate * dt; return d > s ? now + s : d < -s ? now - s : want;
}
static inline float camUserCarHe(float old) { return CAMSET1151_OLD ? old : camUserH(camUserCarH); }
static inline float camUserCarDe(float old) { return CAMSET1151_OLD ? old : camUserD(camUserCarD); }
static inline float camUserFootHe(float old) { return CAMSET1151_OLD ? old : camUserH(camUserFootH); }
static inline float camUserFootDe(float old) { return CAMSET1151_OLD ? old : camUserD(camUserFootD); }
// угол кадра машины с множителем (без бампера — у него свой угол)
static inline float camUserFovY(float base, float mul) { float v = base * mul; return v < CAMU_FOVY_MIN ? (base < CAMU_FOVY_MIN ? base : CAMU_FOVY_MIN) : v > CAMU_FOVY_MAX ? (base > CAMU_FOVY_MAX ? base : CAMU_FOVY_MAX) : v; }
// наклон взгляда: поворачивает вектор «глаз → цель» вокруг горизонтальной оси на tilt° вверх, длина прежняя; почти вертикальный взгляд (|dir.y| > 0,98) не трогаем
static inline void camUserTilt(const float eye[3], float look[3], float tiltDeg) {
  if (tiltDeg == 0.f) return;
  float d[3] = {look[0] - eye[0], look[1] - eye[1], look[2] - eye[2]}, h = sqrtf(d[0] * d[0] + d[2] * d[2]), L = sqrtf(h * h + d[1] * d[1]);
  if (L < 1e-3f || h < 1e-3f) return;
  float el = atan2f(d[1], h) + tiltDeg * (3.14159265f / 180.f);
  if (el > 1.4f) el = 1.4f; if (el < -1.4f) el = -1.4f;   // не больше 80° от горизонта
  float nh = L * cosf(el); look[0] = eye[0] + d[0] / h * nh; look[2] = eye[2] + d[2] / h * nh; look[1] = eye[1] + L * sinf(el);
}
#endif
