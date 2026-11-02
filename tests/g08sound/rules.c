// Круг 6, окно g08-sound: правила звука по строкам сверки. Подключает sound.c целиком, чтобы дотянуться до статических таблиц и очереди, и
// проверяет их настоящим микшером порта на Маке (AUDIO_HOST). Каждая проверка названа строкой сверки; run.sh собирает красные варианты
// с *_OLD=1 и требует, чтобы именно эти проверки краснели.
//   rules PACKDIR/    печатает OK/FAIL <строка> <что>, код 0 — всё прошло
#include "../../viewer/sound.c"
#define FR 512
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "OK" : "FAIL", n); fails += !ok; }
static short bufm[FR * 2];
static void run(float sec, float dt) {
  int frames = (int)(sec * 44100 / FR), per = (int)(dt * 44100 / FR); if (per < 1) per = 1;
  for (int k = 0; k < frames; k++) { audioHostMix(bufm, FR); if (k % per == 0) sndFrame(dt, 0, 3, 0, 1, 0, 0, 0, 0); }
}
static void freeDialog(void) { audioStream(AUDIO_ST_DIALOG, -1, 0.f); for (int k = 0; k < 8; k++) audioHostMix(bufm, FR); }
static float busOf(int bus) { return busNow[bus]; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  // --- без пакета: размеры удара (P09:25) -----------------------------------------------------------------------------------------------
  svCarMass = SV_MASS_KG;
  check("P09:25 удар 6,0 м/с (интенсивность 0,135) — small", crashSize(6.0f) == 0);
  check("P09:25 удар 7,0 м/с (0,158) — medium", crashSize(7.0f) == 1);
  check("P09:25 удар 10,5 м/с (0,236) — всё ещё medium", crashSize(10.5f) == 1);
  check("P09:25 удар 11,5 м/с (0,259) — large", crashSize(11.5f) == 2);
  { float v = 0.15f / (1500.f * 1.5f / 100000.f);   // скорость, где интенсивность ровно 0,15
    check("P09:25 круг 7: на границе 0,15 ещё small, на границе 0,25 ещё medium (строгое «больше»)", crashSize(v) == 0 && crashSize(0.25f / (1500.f * 1.5f / 100000.f)) == 1); }
  check("P09:25 события нет ниже 2,7 м/с (интенсивность 0,06)", crashMin() > 2.5f && crashMin() < 2.9f);
  check("P09:25 интенсивность = скорость × 2250 / 100000", fabsf(impI(10.f) - 0.225f) < 1e-4f);
  sndCarMass(1000.f);
  check("P09:25 лёгкая машина (1000 кг) на 7,0 м/с — small (интенсивность 0,105)", crashSize(7.0f) == 0);
  sndCarMass(2000.f);
  check("P09:25 тяжёлая машина (2000 кг) на 6,0 м/с — medium (0,18)", crashSize(6.0f) == 1);
  sndCarMass(0.f);
  check("P09:25 без массы — 1500 кг", fabsf(impI(10.f) - 0.225f) < 1e-4f);
  // --- выбор клипа подбора (P09:23) -----------------------------------------------------------------------------------------------------
  check("P09:23 L1 — level_1_pickup_sfx", !strcmp(sndPickupClip(1, 3), "level_1_pickup_sfx"));
  check("P09:23 L2 миссия 6 — monkey_collect", !strcmp(sndPickupClip(2, 6), "monkey_collect"));
  check("P09:23 L2 миссия 5 — общий клип уровня", !strcmp(sndPickupClip(2, 5), "level_2_pickup_sfx"));
  check("P09:23 L7 — nuclear_waste_collect", !strcmp(sndPickupClip(7, 2), "nuclear_waste_collect"));
  check("P09:23 L6 — level_6_pickup_sfx", !strcmp(sndPickupClip(6, 1), "level_6_pickup_sfx"));
  check("P09:23 L7 без банка не пустой, L8 нет", *sndPickupClip(7, 1) && !*sndPickupClip(8, 1));
  // --- с пакетом -------------------------------------------------------------------------------------------------------------------------
  if (sndInit(argv[1]) < 0) { printf("FAIL sndInit %s\n", sndError()); return 2; }
  sndUserVolumes(1, 1, 1, 1); audioListener(0, 3, 0, 1, 0); sndSeed(12345); sndLevelStart(0, 0, 0);
  run(1.f, 1.f / 30);
  { int okp = 1; for (int L = 1; L <= 7; L++) for (int m = 0; m < 8; m++) okp &= sndId(sndPickupClip(L, m)) >= 0;
    check("P09:23 на любом уровне и в любой миссии клип подбора есть в банке (иначе тишина)", okp); }
  // №1128: смена уровня играет Loading_screen (не Newspaper_Spin); выбор варианта по часам, все варианты выпадают
  check("№1128 смена уровня: событие Loading_screen (оригинал), Newspaper_Spin только из меню", !strcmp(sndLevelChangeStinger(), "loading_screen"));
  { int cnt3[3] = {0, 0, 0}, cnt2[2] = {0, 0}; for (unsigned k = 0; k < 3000; k++) { cnt3[sndVariantPick(3, 1000000u + k * 16667u)]++; cnt2[sndVariantPick(2, 7u + k * 33333u)]++; }
    check("№1128 из трёх loading_screen выпадают все, ни один не меньше 25 %", cnt3[0] > 750 && cnt3[1] > 750 && cnt3[2] > 750);
    check("№1128 из двух newspaper_spin выпадают оба, ни один не меньше 40 %", cnt2[0] > 1200 && cnt2[1] > 1200); }
  // P09:5 / P09:8: шансы реплик по таблице оригинала; Turbo разрешён, без общей паузы 5 с
  int poolOk = sayN[SND_SAY_BCRASH] && sayN[SND_SAY_MCRASH] && sayN[SND_SAY_BURN];
  check("P09:5 в пакете есть реплики Bcrash, Mcrash, Burn", poolOk);
  if (poolOk) {
    int N = 2000, okB = 0, okM = 0, okU = 0;
    for (int i = 0; i < N; i++) { freeDialog(); okB += sndSay(SND_SAY_BCRASH, 0); freeDialog(); okM += sndSay(SND_SAY_MCRASH, 0); freeDialog(); okU += sndSay(SND_SAY_BURN, 0); }
    printf("доли: Bcrash %.3f, Mcrash %.3f, Burn %.3f\n", okB / (float)N, okM / (float)N, okU / (float)N);
    check("P09:5 Bcrash без броска (все сказаны)", okB == N);
    check("P09:5 Mcrash 20 % (0,17–0,23)", okM > 0.17f * N && okM < 0.23f * N);
    check("P09:5 Burn 15 % (0,12–0,18)", okU > 0.12f * N && okU < 0.18f * N);
  }
  freeDialog();
  if (sayN[SND_SAY_TURBO]) {
    int r = sndSay(SND_SAY_TURBO, 0);
    check("P09:8 Turbo: реплика «устал» звучит (раньше отбрасывалась)", r == 1);
  } else printf("SKIP P09:8 Turbo: в пакете нет реплики\n");
  freeDialog();
  check("P09:8 MissP, Dodge, AIDestroy, LongJump молчат", !sndSay(SND_SAY_MISSP, 1) && !sndSay(SND_SAY_DODGE, 1) && !sndSay(SND_SAY_AIDESTROY, 1) && !sndSay(SND_SAY_LONGJUMP, 1));
  freeDialog();
  // P09:12: очередь по ступеням, жизнь 2,5 с, дубликат не ставят
  if (sayN[SND_SAY_BCRASH] && sayN[SND_SAY_MCRASH] && sayN[SND_SAY_MSTART]) {
    sqn = 0; sndSay(SND_SAY_BCRASH, 0);   // занимает поток диалогов
    sndSay(SND_SAY_MCRASH, 1); sndSay(SND_SAY_MCRASH, 1); sndSay(SND_SAY_MSTART, 0);
    check("P09:12 дубликат реплики в очередь второй раз не ставят (в очереди 2)", sqn == 2);
    check("P09:12 обязательная реплика встаёт впереди случайной", sqn == 2 && sq[0].ev == SND_SAY_MSTART && sq[1].ev == SND_SAY_MCRASH);
    check("P09:12 жизнь строки в очереди 2,5 с", sqn == 2 && fabsf(sq[1].life - 2.5f) < 1e-3f);
    run(3.f, 1.f / 30);
    check("P09:12 строки, не дождавшиеся за жизнь, пропали или выданы", sqn == 0);
  } else printf("SKIP P09:12: нет реплик Bcrash/Mcrash/Mstart\n");
  freeDialog(); sqn = 0;
  // P09:12 (круг 7): четыре ступени очереди как в таблице оригинала, Mstart без срока, переполнение отбрасывает новую, играющая реплика второй раз не ставится
  if (sayN[SND_SAY_BCRASH] && sayN[SND_SAY_TIME] && sayN[SND_SAY_MCRASH] && sayN[SND_SAY_MFAIL] && sayN[SND_SAY_MSTART]) {
    sndSay(SND_SAY_BCRASH, 0);   // занимает поток
    sndSay(SND_SAY_TIME, 1); sndSay(SND_SAY_MCRASH, 1); sndSay(SND_SAY_MFAIL, 0); sndSay(SND_SAY_MSTART, 0);
    check("P09:12 круг 7: порядок ступеней Mstart > Mfail > Mcrash > Time (обязательная, желательная, случайная, неизвестная)",
          sqn == 4 && sq[0].ev == SND_SAY_MSTART && sq[1].ev == SND_SAY_MFAIL && sq[2].ev == SND_SAY_MCRASH && sq[3].ev == SND_SAY_TIME);
    check("P09:12 круг 7: Mstart живёт без срока, Mfail 4 с, Time 2,5 с", sqn == 4 && sq[0].life > 1e6f && fabsf(sq[1].life - 4.f) < 1e-3f && fabsf(sq[3].life - 2.5f) < 1e-3f);
    check("P09:12 круг 7: играющая реплика (Bcrash) второй раз в очередь не ставится", (sndSay(SND_SAY_BCRASH, 1), sqn == 4));
    freeDialog(); sqn = 0;
    sndSay(SND_SAY_BCRASH, 0);
    for (int i = 0; i < SQ_N; i++) { sq[i].ev = (short)(100 + i); sq[i].tier = 0; sq[i].life = 2.5f; } sqn = SQ_N;
    sndSay(SND_SAY_MFAIL, 0);
    int has = 0; for (int i = 0; i < sqn; i++) has |= sq[i].ev == SND_SAY_MFAIL;
    check("P09:12 круг 7: очередь полна (16) — новая реплика отбрасывается, слабейшая не вытесняется", sqn == SQ_N && !has);
  } else printf("SKIP P09:12 круг 7: нет реплик Time/Mfail/Mstart в пакете\n");
  freeDialog(); sqn = 0;
  // P09:14/15 + M2:Д1 + M1:Д4: шины режимов
  sndOnFoot(0); sndMode(SND_MODE_NORMAL); run(1.f, 1.f / 30);
  float car0 = busOf(1), amb0 = busOf(3);
  sndStore(1); run(1.f, 1.f / 30);
  const XMode *st = modeRow("Store"), *nm = modeRow("Normal");
  check("M2:Д1 «магазин»: шина машины и фона падают по строке Store (данные диска)", st && nm && fabsf(busOf(1) - nm->v[1] * st->v[1]) < 0.02f && fabsf(busOf(3) - nm->v[3] * st->v[3]) < 0.02f && busOf(1) < car0 - 0.2f);
  float dlgStore = busOf(4);
  check("M2:Д1 в «магазине» реплика на полной громкости (строка Dialogue 1,0)", st && fabsf(busOf(4) - nm->v[4] * st->v[4]) < 0.02f && dlgStore > 0.9f * nm->v[4]);
  sndStore(0); run(1.f, 1.f / 30);
  { sndLetterbox(1); float m0 = busOf(2); run(0.375f, 1.f / 30); float mh = busOf(2); run(1.f, 1.f / 30); float mf = busOf(2);
    printf("полосы: музыка %.3f -> через 0,375 с %.3f -> через 1,4 с %.3f\n", m0, mh, mf);
    check("P09:15 круг 7: переход на часть размаха тоже идёт 750 мс (через 0,375 с — примерно половина пути)", mh > mf + 0.4f * (m0 - mf) && mh < mf + 0.6f * (m0 - mf));
    sndLetterbox(0); run(1.f, 1.f / 30); }
  check("M2:Д1 выход из «магазина»: шины вернулись", fabsf(busOf(1) - car0) < 0.02f && fabsf(busOf(3) - amb0) < 0.02f);
  { int wid = sndId("wasp_idle"); if (wid < 0) wid = sndId("car_jump");
    if (wid >= 0) {   // №1135: эффект мира (оса) во время открытой будки не слышен, после выхода возвращается
      sndStore(0); run(1.2f, 1.f / 30);
      sndPlay("wasp_idle", 2.f, 0.f, 2.f, 1.f); run(0.3f, 1.f / 30);
      int pk0 = 0; for (int k = 0; k < 40; k++) { audioHostMix(bufm, FR); for (int i = 0; i < 2 * FR; i++) { int a_ = bufm[i] < 0 ? -bufm[i] : bufm[i]; if (a_ > pk0) pk0 = a_; } }
      sndStore(1); run(1.0f, 1.f / 30); sndPlay("wasp_idle", 2.f, 0.f, 2.f, 1.f);
      int pk1 = 0; for (int k = 0; k < 40; k++) { audioHostMix(bufm, FR); for (int i = 0; i < 2 * FR; i++) { int a_ = bufm[i] < 0 ? -bufm[i] : bufm[i]; if (a_ > pk1) pk1 = a_; } }
      sndStore(0); run(1.2f, 1.f / 30); sndPlay("wasp_idle", 2.f, 0.f, 2.f, 1.f);
      int pk2 = 0; for (int k = 0; k < 40; k++) { audioHostMix(bufm, FR); for (int i = 0; i < 2 * FR; i++) { int a_ = bufm[i] < 0 ? -bufm[i] : bufm[i]; if (a_ > pk2) pk2 = a_; } }
      printf("осa: пик до будки %d, в будке %d, после %d\n", pk0, pk1, pk2);
      check("№1135 до будки эффект мира слышен (пик > 300)", pk0 > 300);
      check("№1135 в открытой будке эффект мира не слышен (пик < 3 % прежнего)", pk1 * 33 < pk0);
      check("№1135 после выхода эффект мира слышен снова (пик > 70 % прежнего)", pk2 * 10 > pk0 * 7);
    } else printf("SKIP №1135: нет wasp_idle и car_jump в пакете\n"); }
  sndStore(3); run(1.f, 1.f / 30);
  check("M2:Д1 гардероб (вид 3) состояния не включает", fabsf(busOf(1) - car0) < 0.02f);
  float mus0 = busOf(2);
  sndLoadDuck(1); run(1.f, 1.f / 30);
  check("M1:Д4 до загрузки шина музыки живая (> 0,2)", mus0 > 0.2f);
  check("M1:Д4 загрузка не из меню: музыка приглушена как в паузе (меньше половины)", busOf(2) < mus0 * 0.5f);
  sndLoadDuck(0); run(1.f, 1.f / 30);
  check("M1:Д4 после загрузки музыка вернулась", fabsf(busOf(2) - mus0) < 0.02f);
  // P09:1, P09:22, P09:17, Д9: фон улицы и комнаты
  sndOnFoot(1); sndMode(SND_MODE_ONFOOT); run(1.f, 1.f / 30);
  int t0 = ambTrack;
  if (audioTrack("amb_room_small_01") >= 0) {
    sndMissionAmbience(7, 3, 1);
    check("P09:22 L7 миссия 3 пешком: комнатная атмосфера", ambTrack == audioTrack("amb_room_small_01"));
    sndLevelStart(0, 0, 0); run(0.5f, 1.f / 30);
    sndMissionAmbience(7, 3, 0);
    check("P09:22 в машине атмосфера старта миссии не меняется", ambTrack != audioTrack("amb_room_small_01"));
    sndMissionAmbience(7, 1, 1);
    check("P09:22 L7 миссия 1 — по умолчанию (не комната)", ambTrack != audioTrack("amb_room_small_01"));
    sndLevelStart(0, 0, 0); run(0.5f, 1.f / 30);
    const char *r = sndRoom("Barts_Room");
    check("P09:17 комната вне списка играет amb_room_small_01", ambTrack == audioTrack("amb_room_small_01") && strcmp(r, "-") != 0);
    sndRoom(0); run(0.6f, 1.f / 30);
    check("P09:17 при выходе фон улицы вернулся", ambTrack != audioTrack("amb_room_small_01"));
  } else printf("SKIP P09:22/17: нет amb_room_small_01\n");
  (void)t0;
  // P09:18/31: куски музыки зон в миссиях
  mOwned = 1;
  int hasApu = 0, hasScary = 0;
  for (unsigned i = 0; i < nmevt; i++) { if (!strncmp(mevts[i].event, "OF_Apu_Oasis", 12)) hasApu = 1; if (!strncmp(mevts[i].event, "ScaryMusic01", 12)) hasScary = 1; }
  if (hasApu) check("P09:18 зона 11 (крыша Kwik-E-Mart) играет и в миссии", sndMusicEventIn("OF_Apu_Oasis", 0, 1) == 1);
  if (hasScary) check("P09:18 особая музыка Scary в миссии не вмешивается", sndMusicEventIn("ScaryMusic01", 0, 0) == 0);
  if (!hasApu && !hasScary) printf("SKIP P09:18: в пакете уровня нет событий зон\n");
  mOwned = 0;
  // P05:8: звуки подвижных объектов: короткие — из банка, длинные (лифт L2/L5) — дорожкой anim_* на позиционном потоке, петля излучателя ждёт
  if (audioTrack("anim_elevator") >= 0) {
    int h = sndAnimObj("elevator", 5.f, 0.f, 5.f);
    check("P05:8 лифт L2/L5: дорожка anim_elevator запущена на позиционном потоке", h == 1 && audioStreamTrack(AUDIO_ST_POSITIONAL) == audioTrack("anim_elevator"));
    run(1.f, 1.f / 30);
    check("P05:8 лифт: излучатель не перебивает дорожку объекта", audioStreamTrack(AUDIO_ST_POSITIONAL) == audioTrack("anim_elevator"));
    check("P05:8 повторный вызов с неизвестным именем ничего не делает", sndAnimObj("no_such_object", 0, 0, 0) == 0);
  } else if (sndId("trapdoor") >= 0) {
    check("P05:8 люк (короткий клип в банке) играет", sndAnimObj("TD", 1.f, 0.f, 1.f) != 0);
    check("P05:8 платформа играет по настройке платформы (14-20 м)", sndAnimObj("Splatform4Trans", 1.f, 0.f, 1.f) != 0);
  } else printf("SKIP P05:8: на этом уровне нет подвижных объектов со звуком\n");
  sndShutdown();
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails);
  return fails != 0;
}
