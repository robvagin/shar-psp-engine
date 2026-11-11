// Issue #2 / #4 (автор измерял PC): трек нашего Гомера пешком в формате его analyse.py (poll,ms,x,z) — тот же метод для обоих, его смещения убираются.
//   onfoot_trace LEVEL HOMER OUT.csv [--hz 30|60]          — настоящий homerUpdate (хост-стенд, как turnrun.c)
//   onfoot_trace --src OUT.csv [--hz 60]                   — модель «по исходнику» (константы оригинала, идеальный привод скорости): для столбца «оригинал» таблицы
// Сценарий (как у автора: ничего не размечено, события находит analyse.py по самой скорости): 4 повторения
//   A: стоять 1 с, бег ○+стик 2 с, всё отпущено (остановка), стоять 1 с
//   B: бег 2 с, ○ отпущен при удержанном стике (бег -> шаг) 1,2 с, всё отпущено, стоять 1 с
//   C: бег 2 с, стик назад при ○ (разворот на 180° на бегу) 3 с, всё отпущено, стоять 1 с
//   D: бег 2 с, стик по кругу 110 °/с при ○ 3,3 с (устойчивый поворот), всё отпущено, стоять 1 с
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W; static float DT = 1.f / 60.f, SX = 40.f, SZ = -300.f, HD = 1.5708f;
static FILE *OUT; static long poll_; static double T_;
static float wrapA(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
// ---- модель по исходнику: константы оригинала (ходьба 4, рывок +4, ускорение желаемой скорости 20, торможение 10; темп лица 10/с ходьба, 2/с бег; доля = темп × dt за кадр;
// при отпускании ○ желаемая скорость режется до 4 в тот же кадр; на беге ход вдоль лица, иначе вдоль стика); привод скорости идеальный (скорость = желаемая)
static struct { float x, z, h, ds; } S;
static void srcStep(float mx, float my, int dash) {
  float mag = hypotf(mx, my); if (mag > 1.f) mag = 1.f; float dir = atan2f(mx, my);
  float in = mag > 0.f ? mag * mag / .69f : 0.f; if (in > 1.f) in = 1.f; in *= dash ? 8.f : 4.f;   // желаемая скорость стика (× максимум: 8 при ○, 4 без)
  float maxs = dash ? 8.f : 4.f;
  if (mag > .05f) { float d = wrapA(dir - S.h); float r = (dash ? 2.f : 10.f) * DT; if (r > 1.f) r = 1.f; S.h = wrapA(S.h + d * r); }
  if (dash && in > 0.f) S.ds = maxs;
  else { if (in > S.ds) { S.ds += 20.f * DT; if (S.ds > in) S.ds = in; } else { S.ds -= 10.f * DT; if (S.ds < in) S.ds = in; } if (S.ds > maxs) S.ds = maxs; }
  float ma = dash ? S.h : (mag > .05f ? dir : S.h);
  S.x += sinf(ma) * S.ds * DT; S.z += cosf(ma) * S.ds * DT;
}
static void emit(float x, float z) { poll_++; T_ += DT; fprintf(OUT, "%ld,%ld,%.5f,%.5f\n", poll_, (long)(T_ * 1000.0 + .5), x, z); }
static int SRC;
static void respawn(void) { if (SRC) { S.x = SX; S.z = SZ; S.h = HD; S.ds = 0.f; } else { float g_ = 0.f; colGround(bwCol(), SX, SZ, 30.f, -50.f, &g_, 0); homerPlace(&W.homer, bwCol(), SX, g_ + .3f, SZ, HD); } }
static void step(float mx, float my, int dash) {
  if (SRC) { srcStep(mx, my, dash); emit(S.x, S.z); return; }
  HomerInput in; memset(&in, 0, sizeof in); in.mx = mx; in.my = my; in.camYaw = 0.f; in.dash = (unsigned char)dash; homerUpdate(&W.homer, bwCol(), NULL, &in, DT); emit(W.homer.x, W.homer.z);
}
static void hold(float secs, float mx, float my, int dash) { int n = (int)(secs / DT + .5f); for (int i = 0; i < n; i++) step(mx, my, dash); }
int main(int argc, char **argv) {
  int a = 1; const char *lv = 0, *hm = 0, *out; float hz = 60.f;
  if (argc > 2 && !strcmp(argv[1], "--src")) { SRC = 1; out = argv[2]; a = 3; } else { if (argc < 4) return 2; lv = argv[1]; hm = argv[2]; out = argv[3]; a = 4; }
  for (; a + 1 < argc; a++) if (!strcmp(argv[a], "--hz")) hz = (float)atof(argv[a + 1]);
  DT = 1.f / hz;
  if (!SRC) { if (bwData(lv, hm, 0)) return 2; memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
    float g_ = 0.f; colGround(bwCol(), SX, SZ, 30.f, -50.f, &g_, 0); homerPlace(&W.homer, bwCol(), SX, g_ + .3f, SZ, HD); for (int i = 0; i < 60; i++) { HomerInput in; memset(&in, 0, sizeof in); homerUpdate(&W.homer, bwCol(), NULL, &in, DT); } }
  else { S.x = SX; S.z = SZ; S.h = HD; S.ds = 0.f; }
  OUT = fopen(out, "w"); if (!OUT) return 2; fprintf(OUT, "poll,ms,x,z\n");
  float sx = sinf(HD), sz = cosf(HD); if (!SRC) { sx = -sx; sz = -sz; }   // в хосте homerUpdate ось x стика зеркальна миру: «вперёд» по стику (-1; 0) идёт на +x (в turnrun важен только относительный поворот, тут нужна свободная полоса)   // направление «вперёд»: на +x (площадка свободна на 40 м по 8 направлениям; каждый отрезок короче — сброс позиции между повторениями не нужен: идём кольцом)
  for (int rep = 0; rep < 4; rep++) {
    // каждый тест с точки старта (площадка свободна 40 м); возврат = телепорт, analyse.py режет запись на скачках > 1 м, как на загрузках у автора
    respawn(); hold(1.0f, 0, 0, 0); hold(2.0f, sx, sz, 1); hold(1.2f, 0, 0, 0); hold(0.8f, 0, 0, 0);                                         // A: стоп с бега
    respawn(); hold(1.0f, 0, 0, 0); hold(2.0f, sx, sz, 1); hold(1.2f, sx, sz, 0); hold(1.0f, 0, 0, 0);                                        // B: бег -> шаг, дальше стоп
    respawn(); hold(1.0f, 0, 0, 0); hold(2.5f, sx, sz, 1); hold(3.0f, -sx, -sz, 1); hold(1.2f, 0, 0, 0); hold(1.0f, 0, 0, 0);                // C: разворот на 180° на бегу
    respawn(); hold(1.0f, 0, 0, 0); hold(2.5f, sx, sz, 1); { float ang = SRC ? HD : HD + 3.14159265f; int n = (int)(3.3f / DT + .5f); for (int i = 0; i < n; i++) { ang += 110.f * 3.14159265f / 180.f * DT; step(sinf(ang), cosf(ang), 1); } } hold(1.2f, 0, 0, 0); hold(1.0f, 0, 0, 0);   // D: круг
  }
  fclose(OUT); return 0;
}
