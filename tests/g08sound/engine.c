// Круг 6, окно g08-sound: модель мотора игрока, мотор трафика и ИИ, затухание остановки голоса. Подключает audio.c целиком (статические
// состояния мотора и слотов), настоящий микшер порта на Маке (AUDIO_HOST). Названия проверок — строки сверки (P10-N); run6.sh собирает красные
// варианты с ENGINE_SV_OLD / WORLD_ENGINE_SV_OLD / SV_STOPFADE_OLD / SV_SKIDPITCH_OLD и требует, чтобы именно эти проверки краснели.
#include "../../viewer/audio.c"
#define FR 512
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "OK" : "FAIL", n); fails += !ok; }
static short bufm[FR * 2];
static int gearClicks;
static void noteHook(int id, const char *why) { (void)id; if (why && !strcmp(why, "gear")) gearClicks++; }
static const float TOP = 33.f * 3.6f;   // famil_v: 33 м/с
// кадр: состояние машины + мотор + один буфер микшера (11,6 мс)
static void frame(float kmh, float thr, int air, unsigned flags, float skid, int parked) {
  audioEngineEx(skid, 0, air, parked, 1.f); audioEngine(kmh, TOP, thr, flags); audioHostMix(bufm, FR);
}
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  if (audioInit(argv[1]) < 0) { printf("FAIL audioInit\n"); return 2; }
  audioNote = noteHook;
  const SabCar *c = 0; { int prof = audioPlayerCar("famil_v"); (void)prof; }
  // --- P10-3/P10-8: щелчка передачи нет, высота не быстрее 2 ед./с --------------------------------------------------------------------
  float last = -1, maxStep = 0; int gears = 0, lastG = -1;
  for (int k = 0; k < 500; k++) {
    float v = k * 0.07f * 3.6f; frame(v, 1.f, 0, AUDIO_ENGINE_ON, 0.f, 0);
    AudioStats st; audioStats(&st);
    if (k > 30 && last >= 0 && st.gear == lastG && fabsf(st.enginePitch - last) > maxStep) maxStep = fabsf(st.enginePitch - last);
    if (st.gear != lastG) { gears++; lastG = st.gear; } last = st.enginePitch;
  }
  printf("передач %d, щелчков %d, наибольший шаг высоты за буфер %.4f\n", gears, gearClicks, maxStep);
  check("P10-8 передачи сменяются на разгоне", gears >= 3);
  check("P10-8 высота не быстрее 2 ед./с (≤ 0,0232 за буфер 11,6 мс)", maxStep <= 0.0235f);
  if (audioFind("gearshift") >= 0) check("P10-3 щелчка передачи нет", gearClicks == 0);
  else printf("SKIP P10-3: клипа gearshift в пакете нет, щелчок невозможен ни в каком режиме (строка держится данными)\n");
  // --- P10-10: в воздухе высота от 0,5 до предела оборотов (у famil_v 1,82), отклик 500 мс на весь размах -------------------------------
  for (int k = 0; k < 500; k++) frame(20.f * 3.6f, 1.f, 1, AUDIO_ENGINE_ON, 0.f, 0);
  float hiAir = E.pitch;
  check("P10-10 воздух, полный газ: высота = предел оборотов 1,82 (не 2,0)", fabsf(hiAir - 1.82f) < 0.03f);
  int n0 = 0; float pStart = E.pitch;
  for (int k = 0; k < 500; k++) { frame(20.f * 3.6f, 0.f, 1, AUDIO_ENGINE_ON, 0.f, 0); n0++; if (fabsf(E.pitch - 0.91f) < 0.03f) break; }
  printf("воздух: с %.2f до нижней %.2f за %d буферов (%.2f с)\n", pStart, E.pitch, n0, n0 * 0.01161f);
  check("P10-10 воздух, газ отпущен: нижняя высота = 0,5 предела (0,91)", fabsf(E.pitch - 0.91f) < 0.03f);
  check("P10-10 весь размах 0,91 за 0,5 с, не быстрее (≥ 0,45 с)", n0 * 0.01161f >= 0.45f && n0 * 0.01161f <= 0.62f);
  // --- P10-29: задний ход при скорости > 5 % TopSpeed, высота сразу ---------------------------------------------------------------------
  for (int k = 0; k < 100; k++) frame(0.f, 0.f, 0, AUDIO_ENGINE_ON, 0.f, 0);
  float idleP = E.pitch;
  frame(0.2f * 3.6f * 33.f / 3.6f, 1.f, 0, AUDIO_ENGINE_ON | AUDIO_ENGINE_REVERSE, 0.f, 0);   // 20 % TopSpeed назад
  printf("задний ход: холостая %.3f -> %.3f за один кадр\n", idleP, E.pitch);
  check("P10-29 задний ход: высота выставляется сразу (скачок > 0,1 за кадр)", fabsf(E.pitch - idleP) > 0.1f);
  // --- P10-6: во время повышения внутренняя высота основного голоса не меняется ---------------------------------------------------------
  for (int k = 0; k < 100; k++) frame(0.f, 0.f, 0, AUDIO_ENGINE_ON, 0.f, 0);
  int caught = 0; float pBefore = 0, drift = 0;
  for (int k = 0; k < 900 && !caught; k++) {
    float v = k * 0.04f * 3.6f; int g0 = E.gear; float p0 = E.pitch; frame(v, 1.f, 0, AUDIO_ENGINE_ON, 0.f, 0);
    if (E.shOn && E.gear > g0 && g0 >= 1) { caught = 1; pBefore = p0; }
  }
  if (caught) { for (int k = 0; k < 10 && E.shOn; k++) { frame(E.prevSpd, 1.f, 0, AUDIO_ENGINE_ON, 0.f, 0); drift = fmaxf(drift, fabsf(E.pitch - pBefore)); } }
  printf("повышение: поймано %d, уход внутренней высоты за паузу %.4f\n", caught, drift);
  check("P10-6 повышение передачи ловится отдельным голосом", caught);
  check("P10-6 внутренняя высота за время повышения не уходит (< 0,05)", caught && drift < 0.05f);
  // --- P10-16: оставленная машина — только холостой, без передач и заноса -------------------------------------------------------------
  for (int k = 0; k < 200; k++) frame(5.f * 3.6f, 1.f, 0, AUDIO_ENGINE_ON, 0.f, 1);
  check("P10-16 оставленная машина (даже при скорости): передачи нет, только холостой", E.gear == 0);
  // --- P10 Д9: порядок режимов назад > воздух > занос --------------------------------------------------------------------------------
  for (int k = 0; k < 300; k++) frame(15.f * 3.6f, 1.f, 1, AUDIO_ENGINE_ON | AUDIO_ENGINE_REVERSE, 0.6f, 0);
  check("P10 Д9 назад главнее воздуха и заноса (передача -1)", E.gear == -1);
  for (int k = 0; k < 300; k++) frame(15.f * 3.6f, 1.f, 1, AUDIO_ENGINE_ON, 0.6f, 0);
  check("P10 Д9 воздух главнее заноса (передача 0, высота выше 1,0 при полном газе)", E.gear == 0 && E.pitch > 1.5f);
  // P10-11 (круг 7): занос — бернаут 0,5 + 0,8 · уровень, скольжение 0,5..0,9 предела по газу (предел у famil_v 1,82)
  audioEngineBurn(0.5f); for (int k = 0; k < 900; k++) frame(2.f * 3.6f, 1.f, 0, AUDIO_ENGINE_ON, 0.6f, 0);
  check("P10-11 круг 7: бернаут уровня 0,5 — высота 0,9", fabsf(E.pitch - 0.9f) < 0.03f);
  audioEngineBurn(0.f); for (int k = 0; k < 900; k++) frame(15.f * 3.6f, 1.f, 0, AUDIO_ENGINE_ON, 0.6f, 0);
  check("P10-11 круг 7: скольжение при полном газе — 0,9 предела (1,64)", fabsf(E.pitch - 1.638f) < 0.04f);
  for (int k = 0; k < 900; k++) frame(15.f * 3.6f, 0.5f, 0, AUDIO_ENGINE_ON, 0.6f, 0);
  check("P10-11 круг 7: скольжение при газе 0,5 — 0,7 предела (1,27)", fabsf(E.pitch - 1.274f) < 0.04f);
  frame(0.f, 0.f, 0, 0, 0.f, 0);   // мотор выключен
  // --- P10-1, P10-2: мотор трафика и ИИ: высота ставится сразу -----------------------------------------------------------------------
  lx = ly = lz = 0.f;
  int ready = audioSvReady();
  check("P10-1 клипы ferrari и mission_ai_vehicle есть, мотор мира идёт по новой схеме", ready);
  if (ready) {
    audioSvEngine(0, 0, 1, -1, 10.f, 0.f, 10.f, 0.f, 16.7f, 1.f, 0.016f);
    check("P10-1 трафик в момент появления: высота 0,5", fabsf(svT[0].pitch - 0.5f) < 1e-4f);
    audioSvEngine(0, 0, 1, -1, 10.f, 0.f, 10.f, 16.7f, 16.7f, 1.f, 0.016f);
    check("P10-1 трафик на 60 км/ч: следующий кадр сразу 1,0 (без предела 2 ед./с)", fabsf(svT[0].pitch - 1.0f) < 0.01f);
    audioSvEngine(0, 0, 1, -1, 10.f, 0.f, 10.f, 60.f, 16.7f, 1.2f, 0.016f);
    check("P10-1 трафик очень быстрый: не выше 1,5", fabsf(svT[0].pitch - 1.5f) < 1e-4f);
    audioSvEngine(1, 0, 1, -1, 12.f, 0.f, 12.f, 10.f, 20.f, 1.f, 0.016f);
    check("P10-2 ИИ: высота 0,3 + 0,7·v/TopSpeed = 0,65 сразу в первый кадр", fabsf(svA[0].pitch - 0.65f) < 1e-3f);
    audioSvEngine(1, 0, 1, -1, 12.f, 0.f, 12.f, 40.f, 20.f, 1.f, 0.016f);
    check("P10-2 ИИ: не выше 1,2", fabsf(svA[0].pitch - 1.2f) < 1e-4f);
    audioSvEngine(1, 0, 1, -1, 400.f, 0.f, 400.f, 10.f, 20.f, 1.f, 0.016f);
    check("P10-34 источник дальше 100 + 25 м: голос освобождён", svA[0].h == 0);
    audioSvStopAll();
  }
  // --- P10-12: высота клипа шин постоянна --------------------------------------------------------------------------------------------
  check("P10-12 высота клипа шин не зависит от уровня заноса", SKID_PITCH(0.1f) == SKID_PITCH(0.9f));
  // --- P10-25: остановка голоса затухает, а не обрывается -----------------------------------------------------------------------------
  int id = audioFind("horn_beep"); if (id < 0) id = audioFind("car_jump");
  if (id >= 0) {
    int h = audioPlay(id, 1.f, 0, 1.f); for (int k = 0; k < 4; k++) audioHostMix(bufm, FR);
    AudioStats a; audioStats(&a); int v0 = a.voices;
    audioStop(h); audioHostMix(bufm, FR); audioStats(&a); int v1 = a.voices;
    for (int k = 0; k < 12; k++) audioHostMix(bufm, FR); audioStats(&a); int v2 = a.voices;
    printf("остановка: голосов до %d, через 1 буфер %d, через 13 буферов %d\n", v0, v1, v2);
    check("P10-25 голос играл", v0 >= 1);
    check("P10-25 через один буфер после остановки голос ещё затухает (не оборван)", v1 >= v0);
    check("P10-25 через 13 буферов (≈150 мс) голос освобождён", v2 < v0);
    { for (int k = 0; k < 30; k++) audioHostMix(bufm, FR); AudioStats b0; audioStats(&b0);
      int h2 = audioPlay(id, 1.f, 0, 1.f); for (int k = 0; k < 4; k++) audioHostMix(bufm, FR); AudioStats b1; audioStats(&b1); audioStop(h2); int nb = 0;
      for (; nb < 40; nb++) { audioHostMix(bufm, FR); AudioStats q; audioStats(&q); if (q.voices <= b0.voices) break; }
      if (b1.voices <= b0.voices) printf("предупреждение: голос не стартовал\n");
      printf("затухание остановки: голос свободен после %d буферов (%.0f мс)\n", nb + 1, (nb + 1) * 11.61f);
      check("P10-25 круг 7: затухание ровно 0,1 с (9-10 буферов по 11,6 мс)", nb + 1 >= 9 && nb + 1 <= 11); }
  } else printf("SKIP P10-25: нет короткого клипа\n");
  (void)c;
  audioShutdown();
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails);
  return fails != 0;
}
