// SHAR PSP v8: город клетками 40x40 м подгружается по мере езды, PVS, отсечение пирамидой видимости,
// swizzle текстур, HUD через GE, тест-прогон v2 (кнопка O).
#include "bisect.h"
#include <pspkernel.h>
#include <pspsuspend.h>   // исследование 03.10 П3: проба volatile
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <pspge.h>
#include "release.h"   // релизная сборка: служебные файлы и кнопки выключаются флагом SHAR_RELEASE (tools/final-build.sh --release)
#include "hud.h"
#ifndef C3_ENDING_OLD
#include "ending_view.h"
#endif
#include "col.h"
#include "camsafe.h"   // правка 331: камера никогда не внутри геометрии (ближняя плоскость, глаз)
#include "car.h"
#include "props.h"
#include "coins.h"
#include "stateprops.h"   // стейтпропы и обломки (подсекция SPR1 секции PRP2, docs/props_spec.md)
#include "fx.h"
#include "skid.h"     // правка 249: следы шин (viewer/skid.c)
#include "sound.h"
#include "carsnd.h"      // правка 135: звук удара машины за кадр (твёрдое / листва / машина)
#include "bootscreen.h"   // загрузочный экран GONE Makers    // этап 6: звук (viewer/sound.c + audio.c, пакет audio/ рядом с EBOOT)
#include "smoke.h"    // служебное: smokelog / scenario.txt для tools/smoke.sh
#include "carvis.h"   // машина: колёса, шарниры, урон, фонари, тень, остов (секция CAR1)
#include "levlight.h" // свет уровня для машин и персонажей: блок LGT1 в CAR1 v3 (29.09, рецепт §12)
#include "homer.h"    // этап 4: Гомер пешком (viewer/homer.c + skin.c, данные homer.shr)
#include "game.h"
#include "gag_skin.h"
#include "coll_stream.h"     // Б0: логика кадра без рисования — общая с ботами на Маке (tools/bots)
#include "rooms.h"    // 11.2: комнаты — коллизия и переходы через двери (общие с ботами), рисование — пакет SHG1 ниже
#include "gevb.h"    // кольцо вершин кадра (закон ≈1 мкс на слово списка GE)
#include "ui.h"       // этап 7: родной интерфейс Scrooby (build/ui.shr рядом с EBOOT, tools/ui_export.py)
#include "movie.h"    // интро оригинала: логотипы создателей и ролик до главного меню (movies/ рядом с EBOOT, tools/movie_export.py)
#include "frontend3d.h"   // 12а: 3D-гостиная главного меню оригинала (build/fe.shr, tools/frontend3d_export.py)
#include "fe_audio.h"
#include "audio.h"      // В5: поток звука гэга и петля жужжания осы (viewer/audio.h)
#include "police_fx.h"
#include "bonus_audio.h"
#include "msn_view.h"
#include "bonus.h"      // правка 420: бонус-игра — правила гонки (viewer/bonus.c), игра — viewer/bonus_play.h
#include "bonus_unlock.h"   // правка 421: трасса N открыта — собраны все карточки уровня N (одна правда; BONUS_UNLOCK_OLD=1 — как было)
#include "game_msn.h"      // машины миссий: дороги для их пути (gameMsnSetRoads)
#include <strings.h>
#include "gg_batch.h"
#include "ge_guard.h"    // правка 340: срез треугольников у камеры по защитной полосе GE (тени и земля у края кадра)
#include "render_fast.h" // 05.10 ускорение отрисовки: листы у камеры (О11), повтор клетки в dlCity (О4), стейтпропы вне кадра (О2); flag rfold
#include "padbuf.h"      // правка Роба 25: нажатия между кадрами (все отсчёты пульта с прошлого чтения)    // пункт 8: миссии уровня 1 (level01.msn, viewer/game_msn.c + msn_view.c, маркеры msnart.shr)
#include "ui_sfx.h"     // звуки интерфейса в игре, гейт анимаций HUD
#include "tutorial.h"   // подсказки обучения Барта (Tutorial.scr), 29.09
#include "transit.h"      // переходы оригинала: ирис (iris.shr, tools/iris_export.py) и журнал взрыва (BOOM/EJECT)
#include "msnart.h"     // служебная витрина предметов миссий (flag artgal): сверка рисования msnart.shr
#ifndef TICKET_CONTENT_C3_OLD
#define TICKET_CONTENT_C3_OLD 0
#endif
#if !TICKET_CONTENT_C3_OLD
#include "ticket_view.h"
#endif
#include "collectfx.h"   // 04.10: эффект цели миссии SetCollectibleEffect (флаг cfxold)
#include "blackbox.h"   // правка 103 (01.10): bbOp — самописец чёрного ящика (ниже, у blackBoxSecond)
#include <kubridge.h>   // аудит 01.10 22:35: ловушка вылета — модуль ядра sharexc.prx (viewer/kexc) грузится через kubridge ARK-4
static int kexcRc = 1;   // 0 — ловушка стоит; <0 — код ошибки загрузки/старта; 1 — файла нет (в шапке ящика «kexc N»)
#include "fpuguard.h"   // правка 252 (03.10): ловушки FPU потоков движка выключены, случаи 0/0, x/0, переполнения — в чёрный ящик
volatile int fpuKeep; volatile unsigned fpuSeenThr, fpuSeenFlg;
static unsigned fpuBoot = ~0u, fpuApplied = ~0u, fpuSeenTold;   // FCR31 главного потока до и после правила (шапка ящика «fpu»)
#include "traffic.h"      // этап 5: трафик, стоящие машины, пешеходы (build/traffic.shr, tools/traffic_export.py)
extern unsigned skinWProf, skinW[5], skinNB[2];   // №914: слова GE по стадиям skinDrawPal (skin.c)
extern unsigned trfProf, trfPoseUs, trfDrawUs, trfPoseN, trfWords;   // №914: разложение цены пешеходов (traffic.c)
#include "animobj.h"     // объекты уровня AnimCollision/Anim: грузовик Duff, ветка, платформы АЭС — действия и живая коллизия (ANM1)
#include "savemenu.h"     // сохранения оригинала на карте памяти PSP (viewer/savemenu.c, savegame.c; fsave.shr / psave.shr)
#include "msncam.h"       // правка 40: камеры старта миссий — «наезды» (msncam.shr, tools/msncam_export.py)
#include "convcam.h"      // правка 40: камеры разговора по данным этапа (SetConversationCam, SetCamBestSide)
#include "ui_anim.h"      // правка 51: UI_COIN_HOLD (счёт монет по замеру эталона PS2)
#include "placecode.h"    // правка 58: код места в счётчике («L1 D7-5»), tools/place.py — обратный ход
#include "wardrobe_view.h"   // правка 107: гардероб Гомера (wardrobe.shr, skins.txt, homer-<костюм>.shr)
#include "phone_view.h"    // правка 94: телефонная будка (PhoneBooth.scr): phone.shr + pcar-<k>.shr
#include "menu_access.h"   // пункт ДОБИВКИ 26: доступ к пунктам паузы по записям PS2 (ps2_access_*)
#include "tone.h"         // правка 144: кривая тона палитр мира, насыщенность из настроек (Settings паузы → Colour)
#include "shop_view.h"    // пункт 6: экран покупки у продавца (PurchaseRewards.scr): shop.shr + reward-<k>.shr
#include "zonegate.h"     // правка 181: ворота подгрузки города, как у оригинала (GAT1 секции REG1 v2)
#include "carcam.h"       // правка 198+: камеры за машиной всех машин диска (FollowCameraData, tools/carcam2h.py)
#include "groundshade.h"
#include "fog_tone.h"

PSP_MODULE_INFO("SHARVIEW", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-3072);   // правка 238 (Роб 03.10: «не могу сохраниться на PSP»): системной утилите сохранения (sceUtilitySavedata) нужна
                           // своя память вне кучи игры — было 1 МБ (эмулятору хватало, прошивке нет), теперь 3 МБ

#define BUF_W 512
#define SCR_W 480
#define SCR_H 272
// Режим дальности (docs/ДОП_АУДИТ_2026-09-27.md), SELECT переключает: 1 = ровные 30 FPS и город до DIST_LONG,
// 0 = 60 FPS и 115 м (как v8). Файл view60 рядом с EBOOT — старт в режиме 60. Физика машины — подшаги 1/120 в любом.
#define DIST_LONG 250.f
#define DIST_SHORT 115.f
// План плавности (11.10): бюджет кадра по дальности. Если средняя работа кадра (workMs) выше бюджета DD_BUDGET_* мс, дальность города плавно уменьшается (DD_DOWN м/с) до пола DD_FLOOR_*; когда запас
// больше DD_HYST мс — так же плавно возвращается (DD_UP м/с). Дымка и дальняя плоскость идут от drawDist, поэтому край — 100 % дымки без скачка и «выскакивания»; радиусы подгрузки клеток от
// неё не зависят. Работает только в городе (не комната, не пауза). -DDD_ADAPT_OLD=1 — дальность фиксирована, как раньше
#ifndef DD_ADAPT_OLD
#define DD_ADAPT_OLD BIS(2)
#endif
#ifndef DD_BUDGET_LONG
#define DD_BUDGET_LONG 31.2f
#endif
#ifndef DD_BUDGET_SHORT
#define DD_BUDGET_SHORT 15.2f
#endif
#define DD_FLOOR_LONG 150.f
#define DD_FLOOR_SHORT 100.f
#define DD_DOWN 20.f
#ifndef DD_FAST_OLD
#define DD_FAST_OLD BIS(8)   // 1 — спуск дальности всегда DD_DOWN м/с (как в сборке 0516)
#endif
#define DD_DOWN_X 3.5f   // потолок ускорения спуска (70 м/с) при превышении бюджета на DD_DOWN_K мс на каждую долю
#define DD_DOWN_K 6.f
// №553 (TEST14): регулятор «среднее + 1,5 × отклонение» держал дальность у пола 150 м — Роб видит подгрузку текстур и света на глазах. По умолчанию ВЫКЛЮЧЕН (регулятор по среднему, как до №538); -DDD_SIGMA_ON=1 возвращает новый
#ifndef DD_SIGMA_ON
#define DD_SIGMA_ON 0   // TEST25 (слово дирижёра по №553 и приоритету Роба «дальность»): выключен, регулятор по среднему; прежнее -DDD_SIGMA_ON=1. TEST17 (слово Роба/дирижёра 15.10: отрисовка как в 1721 f32c8c31): регулятор «среднее + 1,5 × отклонение» включён, как в 1721; 0 — по среднему (e616be6c)
#endif
#ifndef DD_SIGMA_OLD
#define DD_SIGMA_OLD (BIS(10) || !DD_SIGMA_ON)   // 1 — регулятор по среднему, как до №538
#endif
#define DD_SIGMA_K 1.5f      // среднее + K x среднее отклонение кадра = оценка p90–95
#define DD_UP 6.f
// Видео TEST17 (Карта review2, D_L1_ramp9_b0 t950,07): за три кадра в небе справа «проступают» дальние деревья — дальность города «дышит» (по логу железа dd на ramp9 182–249 м, откл. 15,5 м, на
// D_L1_city 150–250 м, откл. 33 м; у TEST16 без сигма-регулятора на ramp9 стояло ровно 250 м): регулятор двигает её почти каждый кадр, узкий гистерезис, спуск до 70 м/с.
// «Ровный» режим: вниз только при превышении бюджета больше чем на DD_STEADY_DOWN мс, вверх только когда запас больше DD_STEADY_UP мс, потолок спуска ×DD_STEADY_X, подъём DD_STEADY_RISE м/с.
// DD_STEADY_OLD=1 / «flag ddsteadyold 1» — как раньше
#ifndef DD_STEADY_OLD
#define DD_STEADY_OLD 0
#endif
#define DD_STEADY_DOWN 1.5f
#define DD_STEADY_UP 6.f
#define DD_STEADY_X 2.f
#define DD_STEADY_RISE 4.f
static int ddSteadyOld = DD_STEADY_OLD;
#define DD_HYST 2.5f
// Слово Роба 15.10 («чужие текстуры на свете, подгружаются по ходу езды»): память, на которую уже записан вызов рисования этого кадра (картинки миссий msnart,
// модели машин миссий), освобождается не сразу, а после sceGuSync конца кадра (geDeferFlush): GE читает список после sceGuFinish, и освобождённое, но уже занятое
// другим чтением место подменило бы текстуру. Очередь на DF_MAX указателей; переполнение — освободить сразу (счётчик dfOverflow). DEFER_FREE_OLD=1 — как раньше
#ifndef DEFER_FREE_OLD
#define DEFER_FREE_OLD 0
#endif
#define DF_MAX 192
static void *dfQ[DF_MAX]; static unsigned dfN, dfOverflow, dfTotal, dfMax; static int dfOld = DEFER_FREE_OLD;
void geDeferFree(void *p) {
  if (!p) return;
  if (dfOld || dfN >= DF_MAX) { if (!dfOld) dfOverflow++; free(p); return; }
  dfQ[dfN++] = p; dfTotal++; if (dfN > dfMax) dfMax = dfN;
}
int lt696Pending(void *p) { for (unsigned i = 0; i < dfN; i++) if (dfQ[i] == p) return 1; return 0; }
static void geDeferFlush(void) { for (unsigned i = 0; i < dfN; i++) free(dfQ[i]); dfN = 0; }
// Слово Роба 15.10: дальность города одной величиной — множитель к базовой (DIST_LONG 250 / DIST_SHORT 115 м); по умолчанию +17 % (292 / 135 м). «flag drawdist N» (N — проценты,
// 50..200, 100 — как до правки) в сценарии; DRAWDIST_MUL=1.0f при сборке — прежняя. Адаптация под бюджет кадра (ddScale) работает поверх, пол — от базовой дальности
#ifndef DRAWDIST_MUL
#define DRAWDIST_MUL 1.0f   // TEST17 (Роб 15.10, 16D «сильно лагает»: недогрузка клеток 468 → 7383): дальность как была; 1.17f — +17 %
#endif
static float drawDistMul = DRAWDIST_MUL;
#ifndef TEST_OVERLAY_OFF
#define TEST_OVERLAY_OFF 0
#endif
static int testOverlayOn = !TEST_OVERLAY_OFF;   // TEST16E: оверлей видео-теста (только при сценарии smkOn)
static float ddScale = 1.f, ddAvg;
// Слово Роба 15.10 (видео TEST19: «на причале вдалеке ничего не видно», «в погоне город вдали не появляется»): регулятор дальности стоял на полу 150 м почти весь тур.
// У уровня с дальними версиями кусков (файл far_plain, SHR_FAR_PLAIN=1) пол выше: DD_FLOOR_FAR (200 м) — дальше 140 м клетки рисуются дальней версией без нарезки (−39 % треугольников).
// DD_FLOOR_FAR_OLD=1 — прежний пол 150 для всех; «flag ddfloor N» — пол в метрах в сценарии (замер A/B)
#ifndef DD_FLOOR_FAR_OLD
#define DD_FLOOR_FAR_OLD 0
#endif
#ifndef DD_FLOOR_FAR
#define DD_FLOOR_FAR 200.f
#endif
static float ddFloorOver;
#define PVS_RANGE 115.f   // PVS (tools/pvs.c DRAW) посчитан до 115 м; дальше куски берём по пирамиде клетками
static int longView = 1;
#define LOAD_DIST (drawDist + 75.f)    // запас над дальностью: клетка успевает прочитаться до того, как войдёт в кадр (115 -> 190 м)
#define DROP_DIST (drawDist + 145.f)   // выгрузка с гистерезисом (115 -> 260 м ~3.3 МБ; 250 -> 395 м ~5.4 МБ, tools/budget.py --sweep)
#define LOD_DIST 50.f
#define CONE_COS 0.60f
#define CONE_SIN 0.80f
#ifndef CAM_554_OLD
#define CAM_554_OLD 0
#endif
// №650 (Роб: «широкий экран остаётся, но линза как у оригинала»): широкий режим PS2 держит ГОРИЗОНТАЛЬ — машина 78,1° (вертикаль 16:9 48,8°), пешком 70,0° (43,1°) — настоящая камера PS2 (ps2-camera-lens-angles), запись КАМЕРА_СВЕРКА_PS2 / ps2-camera-fov.
// CAM_LENS_OLD=1 / flag camlensold 1 — усреднение №554 (55° и 48°); CAM_554_OLD=1 — ещё раньше (62° и 54°)
#ifndef CAM_LENS_OLD
#define CAM_LENS_OLD 0
#endif
#if CAM_554_OLD
#define FOVY 62.f
#elif CAM_LENS_OLD
#define FOVY 55.f
#else
#define FOVY 48.8f
#endif
#define ASPECT (16.f / 9.f)
// Ф2: ближняя плоскость 0.7 -> 1.5 м: шаг 16-бит глубины ~ d^2/(65535*near) — в 2.1 раза мельче везде
// (100 м: 22 -> 10 см; 250 м: 1.36 -> 0.64 м). Камера за машиной 6.8 м сзади и 2.4 м выше — кузов дальше 4 м.
// Служебный файл near07 рядом с EBOOT -> 0.7 (сравнение).
#define NEAR_Z 1.5f
static float nearZ = NEAR_Z;
// B7 «потолок кадра» (ресёрч процессора 10.10): если процессор прошлых кадров не уложился в ~30 мс, на полсекунды откладываем то, что на глаз
// не видно: дальние позы (стейтпропы дальше 15/30 м вместо 30/60, пешеходы дальше 25 м раз в 4 кадра), блёстки монет дальше 20 м. Уровни: 0 — как
// есть, 1 — перегрузка. Гистерезис: два кадра подряд > FRAME_CAP_MS включают, полсекунды без перегрузки выключают. -DFRAME_CAP_OLD=1 — выключено
#ifndef FRAME_CAP_OLD
#define FRAME_CAP_OLD 0
#endif
#ifndef FRAME_CAP_MS
#define FRAME_CAP_MS 30.0f
#endif
static int frameGov;   // 0 / 1
int trafficFrameGov;   // то же для traffic.c (пешеходы)
static void frameGovStep(float cpuMs, float dt) {
  static int bad; static float hold;
  if (FRAME_CAP_OLD) { frameGov = trafficFrameGov = 0; return; }
  if (cpuMs > FRAME_CAP_MS) bad++; else bad = 0;
  if (bad >= 2) { frameGov = 1; hold = .5f; } else if (hold > 0.f) { hold -= dt; if (hold <= 0.f) frameGov = 0; }
  trafficFrameGov = frameGov;
}
#define SKY 0xffe0b070
#define FOG_LUM .85f   // яркость дымки от яркости горизонта неба
#define FOG_SAT .20f   // сколько оттенка неба оставить в дымке (0 — чисто серый)

// переключатели: всё, что влияет на скорость, можно выключить по отдельности (тест P2 так и меряет)
enum {
  O_SWZ = 1, O_TEX = 2, O_TRI = 4, O_PVS = 8, O_PLANES = 16, O_LEAN = 32, O_FAR = 64, O_SKY = 128,
  O_SKYCULL = 256, O_QVP = 512, O_D100 = 1024, O_HUD = 2048
};
#define O_DEFAULT (O_SWZ | O_TEX | O_TRI | O_PVS | O_PLANES | O_LEAN | O_FAR | O_SKY | O_SKYCULL | O_HUD | (1u << 17))   // 1<<17 O_SKYLAST; отсечение O_CULL (1<<16) ПОКА ВЫКЛ: tools/wind на других точках глаза даёт 0.5% пикселей с изнанки (docs/handoff/noindex.md)
static unsigned opt = O_DEFAULT;

// Правка Роба 103 (30.09, «у школы … выбило всю приставку»): sceGu* пишут в list без проверки границы. Переполнение портит данные
// движка за массивом, и GE исполняет мусор — встаёт вся PSP. Заполнение (sceGuCheckList — байт от начала кадра) проверяется на
// каждом куске: выше GE_SOFT % бюджета не рисуются дальние куски города, пропы, монеты, частицы; выше GE_HARD % — всё 3D. Остаток —
// HUD, экраны, ирис. Пропуски — в счётчик (smkGeSkip, R+SELECT, STAT, чёрный ящик), память не портится.
// Повтор 01.10 (7dec8b30, «спортивная площадка у школы»): 30.09 остановили только куски города, а персонажи (фоновые, миссий,
// пешеходы — skinDraw), пропы гонок (сотни мешей) и значки миссий (msnart) рисовались после города без проверки — и могли дописать
// список за конец массива. Теперь (общее правило, все уровни): всё 3D — в бюджете GE_BUDGET с теми же порогами (skinGeRoom,
// msnartGeRoom -> geRoomHard), а за бюджетом у массива запас GE_TAIL: интерфейс, HUD и ирис после остановленного на 90 % 3D до
// конца массива не дойдут (их максимум — десятки КБ; весь кадр на железе — до 139 КБ, замер 01.10 00:34). Цена — 64 КБ кучи.
// Замер по данным (01.10): город L1 без PVS с глаза в прыжке (+12 м над крышами) — не больше ~860 кусков в пирамиде, ~120 КБ;
// список за 256 КБ в обычной игре не уходит — это защита на крайний случай, не найденная причина вылета 103
#define GE_BUDGET (256u << 10)
#define GE_TAIL (64u << 10)
static unsigned int __attribute__((aligned(64))) list[(GE_BUDGET + GE_TAIL) / sizeof(unsigned int)];
#define GE_SOFT 80
#define GE_HARD 90
static unsigned geCap = GE_BUDGET;   // служебное «flag gelist N» (КБ): искусственно маленький бюджет — гейт защиты
static unsigned gePeakSec, gePeakShown, geNow, geOverBudget;   // пик заполнения за секунду и в последнем кадре, байт; кадров за бюджетом
static int geOver(int pct) { if ((unsigned)sceGuCheckList() <= geCap / 100u * (unsigned)pct) return 0; smkGeSkip++; return 1; }
static int geRoom(unsigned bytes) { return (unsigned)sceGuCheckList() + bytes <= geCap / 100u * GE_SOFT; }   // частицы (fx.c)
static int geRoomHard(unsigned bytes) {   // персонажи (skin.c skinDraw) и рисунки миссий (msnart.c): тот же предел, что у кусков города
  if ((unsigned)sceGuCheckList() + bytes <= geCap / 100u * GE_HARD) return 1;
  smkGeSkip++; return 0;
}
// Правка 103 (повтор 01.10): сторож «bbdog» чёрного ящика (ниже, у blackBoxSecond). bbBeat — показанные кадры (swapBuf здесь и
// bbBeatTick ириса transit.c), bbPhase — где главный поток внутри кадра. Кадров нет BB_STALL_MS — сторож (выше главного по приоритету)
// сам пишет ящик «STALL … phase=…»: завис ли GE (gesync), логика (car, msn, traffic …) или сборка списка (city, npc3d …)
// (enum BBP_* перенесён в smoke.h: таймеры фаз PHSEC)
static volatile unsigned bbBeat; static volatile int bbPhase;
// №775 (STALL op=player на PSP: TEST27 L6, TEST33 L5; эмулятор не повторяет): op остаётся «player» всю загрузку уровня — после модели игрока bbOp нет.
// bbStep — последний законченный шаг загрузки (ldStep, phMark и шаги внутри levelUiSnd/playerLoad), только указатель: сторож пишет его в ящик и на экран
// вместе с тем, чего ждёт каждый поток (вид ожидания и имя объекта). ldSlowUs — служебное «flag ldslow N»: каждое чтение главного на экране загрузки
// не короче N мкс, кадры газеты идут посреди чтения, как на настоящей карте (в эмуляторе чтение мгновенное)
static const char *volatile bbStep = "-";
static unsigned ldSlowUs;
static unsigned smkFreezeMs;   // служебное «flag freeze N»: главный поток стоит N с в фазе gesync (гейт сторожа tests/crash_school)
// След зависания (правки 252/259, 03.10 22:2x: тур трижды вставал в первом кадре камеры миссии m0, сторож молчал): после старта
// камеры миссии в замер-туре на TR_FRAMES кадров каждая фаза кадра пишется строкой в trace.txt рядом с EBOOT сразу, с закрытием
// файла и мимо очереди карты (msioWriteRaw) — последняя строка на карте покажет, где встала PSP
static volatile int trLeft, trDetail, trDetailOn, trOn; static void trPhase(int n);   // trOn: служебное «flag trace 1» — след включается только им (04.10: причина 259 найдена, след тормозил смоук)   // trDetailOn: служебное «flag trdetail 1» — куски по строке
static void trLine(const char *ln, int l);
#define TR_FRAMES 3   // после старта камеры миссии: 3 кадра по фазам (зависание 259 — в первом), не тормозить маршрут дальше
// Правка Роба 500, шаг А1 (ресёрч оптимизации, docs/handoff/ОПТИМИЗАЦИЯ_PSP_РЕСЁРЧ_2026-10-10.md): «сборка списка GE» (11–22 мс) без разбивки.
// Те же отметки BBP, что и у сторожа, копят мс на кадр по подфазам сборки: city (небо, листы клеток, очередь), car3d, npc3d (Гомер, люди),
// trf3d, mcr3d, props3d (пропы, стейтпропы, билборды, монеты), fx3d (частицы, прозрачное), ui (интерфейс, счётчик) — строка RBLD раз в
// секунду в smoke.txt (main_loop.h). Цена: один вызов таймера на отметку. RBLD_OLD=1 — без счёта
#ifndef RBLD_OLD
#define RBLD_OLD 0
#endif
static double rbldSum[BBP_N + 1]; static unsigned rbldN, rbldT;
// Подфазы внутри BBP_UI (слово дирижёра 15.10: x_ui 3,5–4,7 мс в погонях, что именно?): метки UIPH(k) между кусками кода интерфейса в main_loop.h, мкс накапливаются, раз в
// секунду строка UIPH (мс на кадр). Только тест-апка (smkOn), UIPH_OLD=1 — без меток. Названия — в UIPH_NAMES; сумма фаз = x_ui без дрожи таймера.
#ifndef UIPH_OLD
#define UIPH_OLD 0
#endif
enum { UIPH_PRE, UIPH_HUDSET, UIPH_MVHUD, UIPH_RADARADD, UIPH_MISC, UIPH_COLL, UIPH_FADE, UIPH_SCREEN, UIPH_EXTRA, UIPH_TAIL, UIPH_TRANSIT, UIPH_HUDDBG, UIPH_OVERLAY, UIPH_GECHECK, UIPH_N };
static const char *const UIPH_NAMES[UIPH_N] = {"pre", "hudset", "mvhud", "radaradd", "misc", "coll", "fade", "screen", "extra", "tail", "transit", "huddbg", "overlay", "gecheck"};
static unsigned uiPhUs[UIPH_N], uiPhN, uiPhT;
#define UIPH_START() do { if (smkOn && !UIPH_OLD) { uiPhT = sceKernelGetSystemTimeLow(); uiPhN++; } } while (0)
#define UIPH(k) do { if (smkOn && !UIPH_OLD) { unsigned n_ = sceKernelGetSystemTimeLow(); uiPhUs[k] += n_ - uiPhT; uiPhT = n_; } } while (0)
static unsigned rbcCull, rbcVis, rbcDraw, rbcDrawM;   // счётчики вызовов за секунду (не таймеры): cullPiece, visBox, drawPiece, drawPieceM — в строке RBLD
#define RBC(x) do { if (!RBLD_OLD) (x)++; } while (0)
static void rbldTick(int n) {
  if (RBLD_OLD) return;
  unsigned now = sceKernelGetSystemTimeLow(); int p = bbPhase;
  if (p >= BBP_CITY && p <= BBP_UI && n > p && n <= BBP_GESYNC) rbldSum[p] += (now - rbldT) / 1000.0;
  if (n == BBP_CITY) rbldN++;
  rbldT = now;
}
#define BBP(n) (flatOff(), rbldTick(n), smkBbp(n, draws, drawnTris, binds, (int)sceGuCheckList()), bbPhase = (n), trLeft > 0 ? trPhase(n) : (void)0)   // RBLD (подфазы сборки) + PHDRAW (вызовы по фазам)
void bbBeatTick(void) { bbBeat++; }
// Правка Роба 293 (04.10, «в самом начале наезда к газете мигает пол»; повторы 244, 265 — каждый раз в другой момент перелёта).
// Смена буфера кадра должна попасть в обратный ход луча: sceGuSwapBuffers ставит новый буфер «со следующего кадрового импульса»
// (PSP_DISPLAY_SETBUF_NEXTFRAME). Если главный поток опоздал (его может вытеснить поток повыше: звук 0x12, загрузчик 0x18), экран,
// по гипотезе, ещё целый кадр показывает ПРЕЖНИЙ буфер, а следующий кадр сразу его чистит и рисует: sceGuClear и каждый
// sceGuDrawArray тут же двигают границу исполнения GE (libpspgu: sceGeListUpdateStallAddr — проверено по libpspgu.a). Луч дочитывает
// низ экрана — пол гостиной — из полу-нарисованного кадра (пол рисуется рано, текстура №8; ковёр, трек, газета — позже). Эмулятор показывает буфер мгновенно в обратный ход —
// этого не видит; покадровый тест 265 (кадр снят после конца кадра) — тоже. Правило порта (экраны, ждавшие обратный ход: меню и
// гостиная, ролики, загрузка; город — пока только счёт, ниже): после смены обратного хода уже нет (sceDisplayIsVblank() == 0) —
// ждём следующего и только потом рисуем: повтор кадра вместо рваного.
// ГИПОТЕЗА, проверка — тур TEST 10 (tests/fe_guard/psp_test10_menu.txt). Прежнее: сборка -DSWAP_GUARD_OLD=1 или «flag swapold 1»
#ifndef SWAP_GUARD_OLD
#define SWAP_GUARD_OLD 0
#endif
static int swapOld = SWAP_GUARD_OLD;
static unsigned swapDelayUs;   // служебное «flag swaplate N»: опоздание N мс перед сменой (имитация вытеснения: гейт и тур TEST 10)
static unsigned swapN, swapLateN, swapWaitN, swapWinN, swapWinLate, swapGameLateN; static int swapOff;   // счётчики; swapOff — страховка ниже
static int swapLine = -1, swapLate, swapWait, swapGuard;   // последняя смена: строка луча после неё, опоздала, ждали обратный ход, guard
// guard 1 — правило; 0 — кадр без ожидания обратного хода (загрузка с vsync 0): как было; -1 — главный цикл игры (город): как было,
// опоздания только считаются (swapGameLateN) — туда правило пока не распространено, сначала замер туром TEST 10 (у города между
// ожиданием обратного хода и сменой стоит smokeFrame)
// №663 лист «PS2 | порт»: «flag framesheet 0.2» — служебный снимок показанного кадра не чаще раза в N секунд (имя fs<номер>_<мс от начала>), от нажатия ✕ до первого кадра уровня; только при smkOn
static float fsInt; static unsigned fsT0, fsLast; static int fsN;
static void framesheetTick(void) {
  if (!smkOn || fsInt <= 0.f) return;
  unsigned now = sceKernelGetSystemTimeLow(); if (!fsT0) { fsT0 = fsLast = now - (unsigned)(fsInt * 1e6f); }
  if (now - fsLast < (unsigned)(fsInt * 1e6f) || fsN >= 99) return;
  char nm[32]; snprintf(nm, sizeof nm, "fs%02d_%05u", fsN++, (now - fsT0) / 1000u); fsLast = now; smokeShotNow(nm);
}
static void *swapBufG(int guard) {
  bbBeat++;
  if (swapDelayUs && guard > 0) sceKernelDelayThread(swapDelayUs);
  void *r = sceGuSwapBuffers();
  int vb = sceDisplayIsVblank(); swapLine = sceDisplayGetCurrentHcount(); swapLate = !vb; swapWait = 0; swapGuard = guard; swapN++;
  if (swapLate && guard > 0) {
    swapLateN++; swapWinLate++;
    if (!swapOld && !swapOff) { sceDisplayWaitVblankStart(); swapWait = 1; swapWaitN++; }
  }
  // страховка: если «опаздывает» больше половины смен из 64 сами по себе (иная прошивка — обратный ход не виден), не ждать до
  // следующего окна — иначе вдвое меньше кадров везде. Имитация swaplate не в счёт
  if (guard > 0 && ++swapWinN >= 64) { swapOff = !swapDelayUs && swapWinLate > 32; swapWinN = swapWinLate = 0; }
  if (guard < 0 && swapLate) swapGameLateN++;
  framesheetTick();
  return r;
}
static void *swapBuf(void) { return swapBufG(1); }   // экраны, ждавшие обратный ход: меню, гостиная, ролики, загрузка с vsync

typedef struct { short u, v; unsigned int c; short x, y, z, pad; } Vert;
#include "groundcover.h"
static GroundCover groundCover;
// Шаг 3 аудита (gebench на PSP: без индексов 1.83x быстрее): флаги куска (младший байт Rec.flags)
//   8 двусторонний (без отсечения задних граней), 16 без индексов (вершины по 3 на треугольник: ближняя версия,
//   сразу за ней дальняя, nv = ni + nf), 32 вершина 12 Б (u,v, цвет 5650, x,y,z; альфа 255)
#define F_DOUBLE 8
#define F_NOIDX 16
#define F_V12 32
#define VT16 (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
#define VT12 (GU_TEXTURE_16BIT | GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
#define O_CULL (1u << 16)     // отсечение задних граней (лицевой обход CCW, tools/wind.c)
#define O_SKYLAST (1u << 17)  // небо после непрозрачного, с тестом глубины; цвет кадра не чистим
static int fb16 = 1;          // кадр 5650 + дизеринг (служебный файл fb32 рядом с EBOOT -> 8888, для сравнения)
#include "jitter.h"   // эксперимент: временное сглаживание полпикселя (по умолчанию выкл., файл jitter)
static int clearChk;          // служебный файл clearchk: кадр чистится пурпурным, P1 считает непокрытые пиксели
static int pano917Cap; static float pano917Near, pano917Far;   // №917б: служебный кадр бота («flag pano917cap БЛИЗКО ДАЛЬНО»): город только дальше БЛИЗКО м (ближняя плоскость) до ДАЛЬНО м, пурпурный фон, без неба, ленты, интерфейса
static int noSkyChk;          // замер-тур (03.10, «flag nosky 1»): небо не рисуется — с clearchk щель остаётся пурпурной, а не цветом неба
static int seamsOld;          // служебный файл seamsold: город с абсолютными матрицами, как до правок 224/237/251 (красный контроль)
static int seamChk;           // служебный файл seamchk (с clearchk): город без альфа-теста и полупрозрачного — пурпур остаётся только
                              // в щелях между непрозрачными поверхностями (правки 224/237/251, tools/seams_emu.py; листва иначе — тысячи «швов»)
static int ccViews, ccMax; static long ccTot;
typedef struct { int w, h, levels; void *pal; void *lv[8]; void *slv[8]; int pre; float slope; int ground; int t4; const void *opqPal; int opq; unsigned char tstat[5], tstatOk; int levelsAll; unsigned char lat; unsigned flatC; unsigned char flatK, flatE, flatS, flatR, flatEd, flatDone, curb, asph; unsigned short xdon; } Tex;   // №904: levelsAll — уровней мипа в файле, lat — решётка (latticeScan, main_levels.h)   // tstat[4] (№786): 1 — текстура без шагов мультика (облака неба)   // t4: правка 129 — 4 бита (байт 6 = 4); opq/opqPal — правка 413 (carGlass)
// строка уровня в файле, байт: T8 — ширина (в swizzle не меньше 16 Б, Ф2); T4 (правка 129, только в swizzle) — полширины, не меньше 16 Б
static inline int texRowBytes(int t4, int pre, int lw) { return t4 ? ((lw + 1) >> 1 < 16 ? 16 : (lw + 1) >> 1) : (pre && lw < 16 ? 16 : lw); }
// ширина буфера GE (TBW) в текселах: T8 не меньше 16, T4 не меньше 32 (блок swizzle 16 Б = 32 тексела по 4 бита)
static inline int texTbw(int t4, int lw) { return t4 ? (lw < 32 ? 32 : lw) : (lw < 16 ? 16 : lw); }   // slope: правка 77/88 — мип по глубине (0 — по треугольнику)   // pre: уровни в файле уже в swizzle (lv = slv)
typedef struct {
  float bb[6]; int tex, flags, nv, ni, owner; float ox, oy, oz, sc; unsigned off, nf, cell, pad[2];
} Rec;
typedef struct { Rec r; float cx, cy, cz, rad; Vert *v; unsigned short *idx, *fidx; GgBatch *ggBatch; void *unr; unsigned nz; } Piece;   // unr — развёртка полос в списки (stripdraw.h), только у кусков с F_STRIP, по требованию; nz — №949: код масштаба шума земли (u | v << 8, toonflat.h), 0 — нет
typedef struct {
  float bb[6]; unsigned first, n, owner, off, size, pad;
  float cx, cy, cz, rad;
  volatile int state;           // 0 нет, 1 грузится, 2 готов, 3 нет памяти
  void *mem;
  volatile unsigned reqT;       // исследование 03.10 П2: время первого запроса клетки (мкс | 1), 0 — не запрошена
  void *cg; unsigned char cgn[3], cgs; unsigned short cgk; int cgi; unsigned cgbytes;   // №620: сетка занятости видимой геометрии (camgrid.h), 0 — нет
} Cell;

// Правка Роба 509 (подложка по клеткам): Cell.pad = байт 0 регион+1 (правка 181), байт 1 глубина плоскости под низом ящика клетки в шагах 0,25 м,
// байты 2–3 цвет RGB565 средней земли клетки (0 — подложки нет). Регион — только младший байт
#define CELL_REG(c) ((c)->pad & 0xFFu)
static Tex *tex; static unsigned ntex, npc, ncell;
static Piece *pc; static Cell *cl;
// №840 (Роб 19.10, TEST38: «газон под Гомером при выходе из машины пропадает кусками; пол не должен пропадать никогда»): GE PSP выбрасывает треугольник
// целиком, если его вершина (после среза по ближней плоскости) уходит за ±2048 px; при ближней плоскости 1,5 м это треугольник с ребром больше ~10 м
// у камеры (газон, полоса дороги). Куски города с ребром больше GG_BIG_EDGE помечаются при загрузке клетки (Rec.pad[1] = 1 у owner 0; у города он 0),
// клетка с такими кусками — cellBigF[клетка] = 1; срез на процессоре у камеры для них включён всегда (ggBigOnly). GG_BIG840_OLD=1 — как было
#ifndef GG_BIG840_OLD
#define GG_BIG840_OLD 0
#endif
// №854 (Роб 20.10 после тура TEST43: «куда лучше, ~20 % проломов осталось — капельку добавить объёма»): тур TEST43 в эмуляторе со счётчиком — 2 выброса листом
// на холме L3, в повторе — 34 за 2 с в конце L1: глаз ровно в 4 м над землёй, ближняя плоскость 1,5 м, треугольник города в 0,5 м от глаза (правило №840 не
// включилось: порог 3,2 м). Запас — по высоте глаза: 4,5 м. Порог ребра 5 м пробовал (−80 % риска в листах), но на ходьбе +0,1–0,34 мс эмулятора на кадр —
// оставлен 8 м. GG_BIG854_OLD=1 — как TEST43
#ifndef GG_BIG854_OLD
#define GG_BIG854_OLD 0
#endif
#define GG_BIG_EDGE 8.f
#define GG_BIG_H (GG_BIG854_OLD ? 3.2f : 4.5f)   // №840: глаз ниже этого над землёй (или земли под глазом нет) — срез крупных треугольников у камеры (№854: 4,5 м)
static unsigned char *cellBigF;
#include "cellpool.h"
#include "stripdraw.h"   // К4: куски города полосами (F_STRIP), ПЛАН_ОПТИМИЗАЦИЙ 17.10 п.1
#include "camgrid.h"
// Правка «быстрый расчёт» (ТОП-3 тура SHARTEST7: отсечение ph7 2,7–3,3 мс при 1000–1200 вызовах cullPiece, ~2,7 мкс на вызов — промахи кэша):
// отсечение читало из 112-байтного Piece две строки кэша (бокс, регион, флаги, текстуру, клетку). Горячая запись 32 Б на кусок, собранная
// при загрузке уровня (метаданные городских кусков не меняются: меняет флаги/текстуру только отрисовка машин, а они не в городе), и клетка
// с её состоянием. -DCULL_HOT_OLD=1 — прежний путь по Piece
#ifndef CULL_HOT_OLD
#define CULL_HOT_OLD 0
#endif
typedef struct { float bb[6]; unsigned short tk, tcl, cell; unsigned char rg, pad; } CullRec;   // tk — ключ сортировки (флаги&3 << 9 | текстура), tcl — клетка для листа (0xFFFF — полупрозрачный/вне u16)
static CullRec *cr;
// Правка 275 (план ведущего 04.10 §2): дисплей-лист GE на клетку города — команды всех непрозрачных кусков клетки (текстура, флаги,
// DrawArray) собираются один раз, когда клетка впервые видна; в кадре на клетку — матрица модели и один sceGuCallList. Лист живёт,
// пока клетка в памяти (unloadCell освобождает). ok: 0 — не собран, 1 — собран, 2 — у клетки нет непрозрачных кусков
// Правка «плавность» (10.10, M313_boom пешком у остова): второй лист клетки — без кусков под риском среза (правка 340), собранный под маску
// этих кусков (pmask, бит — номер куска в клетке); пока маска та же, клетка у камеры — один вызов листа + срез только рискованных кусков
typedef struct { unsigned *buf; int tris, draws; unsigned bytes; unsigned char ok; unsigned short nb, pnb, fnb; unsigned char noise, pnoise, fnoise;   // №912: смен состояния текстуры (привязка или заливка) внутри листа
                 unsigned *pbuf; int ptris, pdraws; unsigned pbytes; unsigned long long pmask; unsigned char pok;
                 unsigned long long mkVal; unsigned mkEpoch;
                 unsigned *fbuf; int ftris, fdraws; unsigned fbytes; unsigned char fok; unsigned *mbuf, *mbuf2; unsigned mbytes, mbytes2; int mtrisA, mtrisB; unsigned char mok, mok2; } CellDL;   // mbuf/mok (№912): отдельный лист разметки клетки (рисуется ближе MARK_RANGE)   // fbuf/fok: лист дальней версии клетки (far_plain, TEST23); fok 1 — собран, 3 — не собрался   // mkVal/mkEpoch: маска рискованных кусков для стоящей камеры (BOOM_CITY_OLD)
static CellDL *cdl; static unsigned *cdlMark, cdlEpoch; static unsigned dlBuilt, dlBytesNow, dlCallsFrame, dlFallFrame;
static void dlFree(unsigned ci) {
  if (!cdl || ci >= ncell) return;
  if (cdl[ci].buf) { dlBytesNow -= cdl[ci].bytes; free(cdl[ci].buf); }
  if (cdl[ci].pbuf) { dlBytesNow -= cdl[ci].pbytes; free(cdl[ci].pbuf); }
  if (cdl[ci].fbuf) { dlBytesNow -= cdl[ci].fbytes; free(cdl[ci].fbuf); }
  if (cdl[ci].mbuf) { dlBytesNow -= cdl[ci].mbytes; free(cdl[ci].mbuf); }
  if (cdl[ci].mbuf2) { dlBytesNow -= cdl[ci].mbytes2; free(cdl[ci].mbuf2); }
  memset(&cdl[ci], 0, sizeof cdl[ci]);
}
static unsigned short *carP, *skyP; static int ncarP, nskyP;
static short *hm, *wl; static float hox, hoz, hcell; static int hw, hh;
static Car oldCar; static int oldCarOn;   // правка 63: своя машина осталась стоять, пока игрок на угнанной
// Правка Роба 497 (L2 на PSP: 51 тыс. треугольников и 22 мс сборки списка против 30 тыс. и 12,6 на L1): PVS считался только до 115 м, за ними
// (в режиме 30 FPS город до 250 м) куски брались по пирамиде обзора, без закрытия домами — это 60 % треугольников кадра. Дальность списков
// пишет tools/pvs --range N в pad заголовка PVS1 (0 — 115, как раньше); с ней весь режим 250 м идёт по спискам. PVS_FAR_OLD=1 — прежнее.
#ifndef PVS_FAR_OLD
#define PVS_FAR_OLD 0
#endif
// Правка Роба 508 (L2: треугольников в кадре ×1,7 к L1 из-за нарезки города 14/7 м): дальняя версия куска БЕЗ нарезки (исходные треугольники диска,
// режутся рёбра > 28 м; tools/psp_export.py SHR_FAR_PLAIN=1, флаг куска F_FARP). Включается ФАЙЛОМ far_plain рядом с EBOOT (в нём число D, м, например 100;
// пустой файл — 100): дальше D клетка рисуется кусками дальней версии, ближе — листом клетки как раньше. Файла нет — farPlainD = 0, всё как раньше.
// Шов ближний/дальний закрыт юбками дальней версии по границе клеток (skirts.py, SHR_FAR_PLAIN). FARPLAIN_OLD=1 — игнорировать файл
#ifndef FARPLAIN_OLD
#define FARPLAIN_OLD 0
#endif
static float farPlainD;
#ifndef DD_SOFT_FLOOR_OLD
#define DD_SOFT_FLOOR_OLD 0
#endif
#ifndef DD_SOFT_OVER
#define DD_SOFT_OVER 2.f   // мс сверх бюджета (оценка p90–95), с которых мягкий пол отпускает дальность до жёсткого (DD_FLOOR_LONG 150 м)
#endif
// Слово Роба 16.10 (~16:55): «сделай дальше прорисовку: 200 м». Пол дальности 200 м на всех уровнях (и мягкий, и жёсткий), регулятор работает только выше;
// «flag ddfloor N» по-прежнему переопределяет (A/B 150/200 в туре). DD_FLOOR200_OLD=1 — прежние полы (мягкий 200 только с far_plain, жёсткий 150)
#ifndef DD_FLOOR200_OLD
#define DD_FLOOR200_OLD 0
#endif
static float ddFloorHard(void) { return ddFloorOver > 0.f ? ddFloorOver : (DD_FLOOR200_OLD ? DD_FLOOR_LONG : DD_FLOOR_FAR); }   // нижний предел дальности: 150 м; «flag ddfloor N» поднимает его до N (замер A/B жёсткого пола)
static float ddFloorLong(void) { if (ddFloorOver > 0.f) return ddFloorOver; return (!DD_FLOOR_FAR_OLD && (farPlainD > 0.f || !DD_FLOOR200_OLD)) ? DD_FLOOR_FAR : DD_FLOOR_LONG; }   // пол регулятора дальности: у уровня с дальними версиями выше
static float pvsRange = PVS_RANGE;   // дальность, до которой списки PVS1 уровня полны (читается из файла)
static unsigned *pvsEnt; static unsigned short *pvsPool; static float pvsOx, pvsOz, pvsCell; static int pvsW, pvsH;
static unsigned rfPbUs, rfUiUs;   // RFAST: время блока запекания B1 (b1_us), интерфейса (ui_us)
// Прогрев на хвосте экрана загрузки (№538, ЗАДАНИЕ_ЗАГРУЗКА_ВПРОК): показ идёт не меньше LOAD_MIN_S (3,5 с), чтение ~0,64 с — ~3 с простоя. Вокруг точки старта (ldWarmC) строятся листы целых клеток
// и запекание B1, чтобы после снятия ириса этого не делать на глазах. LOAD_WARM_OLD=1 (бит 14 bisect) — как до правки
#ifndef LOAD_WARM_OLD
#define LOAD_WARM_OLD BIS(14)
#endif
static float ldWarmC[3]; static int ldWarmOn, ldWarmStep_; static unsigned rfLoadWarmBuilt, rfLoadWarmBaked;
#include "pvs2.h"   // PVS по курсу в горячих клетках (PV2S после PVS1; PVS_SECTOR_OLD=1 — выкл.)
static int pv2Off;   // «flag pv2off 1» (смоук): PVS по курсу выключен на лету, сравнение кадров «с / без»
static Pvs2 pv2; static void *pv2Mem; static int pv2Cur = -1, pv2CurCell = -1; static unsigned rfPv2Frames, rfPv2Cut;
// правка Роба 95/83/84 (L1 F11-2 и крыша заправки F10-7): регионы оригинала — секция REG1 (tools/regions_export.py). Оригинал
// держит 3–4 файла-региона из 13 (ZoneEvent, телепорты, миссии); в регионе лежат грубые «декорации издали» соседних районов
// (у заправки — холм с дорогой, дома и стена деревьев из l1r1). Кусок города (Rec.pad[0] = регион + 1) рисуется, только если
// оригинал держит его регион с клетки глаза (маска u16 по клеткам 20 м); 0 — без региона, рисуется всегда
static unsigned short *regMask; static float regOx, regOz, regCell; static int regW, regH, regOff, regCut;
static unsigned regEye = 0xFFFFu;
// Правка 181 (слово Роба 02.10: «ближе к оригиналу, других путей нет»): регионы города в памяти — как у оригинала, воротами
// (viewer/zonegate.h, блок GAT1 секции REG1 v2). regRes — набор регионов: старт уровня — набор ближайшего места (AddTeleportDest),
// въезд в ворота — выгрузить/загрузить, перенос дальше 50 м за кадр (goto, сброс, сценарий) — набор места; в комнате набор не
// меняется. stream() держит в памяти только клетки регионов набора, рисуются тоже только они. Нет GAT1 — как раньше (REG1 по месту)
static ZoneGate *zg; static ZonePlace *zp; static int nzg, nzp, zgOn, zgInit, zgStart; static unsigned regRes = 0xFFFFu;
static unsigned char zgWas[ZG_MAX]; static float zgPx, zgPz; static unsigned zgFired, zgUnloads;
static char path[256];
static ColWorld world; static int haveCol, autodrive, slowmo; static float adT, adStop; static Car car;
// №914: служебный автопилот (flag drive / walk). AD_CLOCK_OLD=1 — часы adT шли дважды за кадр (строка «if (autodrive)» перед шагом машины
// плюс свой шаг режима 4 и 5): срок вдвое короче заданного. AD_SEAT_OLD=1 — flag drive пешком не сажал Гомера в машину (в L2/L4/L6/L7 она стоит
// в координатах L1, путь 0 м). Красные контроли гейта tests/tour/drive914.sh: flag adclockold 1 / flag adseatold 1
#ifndef AD_CLOCK_OLD
#define AD_CLOCK_OLD 0
#endif
#ifndef AD_SEAT_OLD
#define AD_SEAT_OLD 0
#endif
static int adClockOld = AD_CLOCK_OLD, adSeatOld = AD_SEAT_OLD;
static int carExplosionOld;
static int radarPhonesOld;
static void carExplosionAt(float x, float y, float z) {
  float p[3]={x,y,z}, n[3]={0,1,0}, gy;
  if (!carExplosionOld && haveCol && colGround(&world,x,z,y+2.f,y-20.f,&gy,n)) p[1]=gy;
  fxBurstBreakable(24,p,n);
}
// Аудит 01.10 22:35 (обрыв F10-5): служебный автопилот по путевым точкам — «flag wp X Z» добавить точку, «flag wpv V» держать
// V м/с, «flag wpgo» ехать (autodrive 3): руль на точку, точка взята в 8 м (строка WP в smoke.txt), после последней — тормоз.
// Повтор пути из чёрного ящика с той же скоростью (tests/crash_f10)
#ifndef WP_STEER_SIGN
#define WP_STEER_SIGN -1.f   // проверено прогоном: при +1 машина уходит от точки
#endif
static float wpX[32], wpZ[32], wpV = 22.f; static int wpN, wpI;   // этап 2: честная коллизия (секция COL1)
static int uiReady;   // этап 7: ui.shr загружен -> родной HUD, START = пауза с RESET CAR
static void palTone(void *pal);   // правка 29: кривая тона палитр (ниже)
enum { TF_LEVEL = 0, TF_TRAFFIC = 1, TF_MCR = 2, TF_ROOM = 3 };   // правка 144: файлы-источники палитр (toneBlkAdd, toneApplyAll)
static Props props; static int haveProps; static Coins coins; static int haveCoins; static float hnrHeat, hnrCalm;  /* шкала Hit & Run 0..1 */ static float prevDmg; static int wasDestroyed;   // этап 3б: сбиваемые пропы (секция PRP1)
static StateProps sprops; static int haveSP;   // SPR1: ящики, автоматы, будки, колонки, кусты-шары, щитки m4 + обломки деревьев/стекла/знаков
static int artGal;   // служебная витрина предметов миссий (flag artgal)
static AnimObjs ao; static int haveAo, aoAct;   // ANM1 (viewer/animobj.c): действия объектов уровня, живые объёмы COL1; aoAct — △ этого кадра объекту
// сбитый DynaPhys с обломком BreakableObject: проиграть обломок в его исходной точке; 1 — у обломка есть меши, проп спрятать
// + частицы удара по таблице ATC типа (PRP2 @44): обломок (вода гидранта, листья/щепа деревьев, стекло Krusty) и
// система частиц (3 куст, 4 мусор, 5 листья дуба, 6 почта) — родные из fx.shr; eSpark (фонари) — искры кодом
#ifndef BREAK_DIR923_OLD
#define BREAK_DIR923_OLD 0   // №923: 1 — осколки по осям мира / предмета, как было
#endif
static void onPropKickFx(void *u, unsigned i, const float pos[3], float carV, float dx, float dz) {   // звук и частицы удара (логика — gamePropKick)
  (void)u;
  sndObjectHit(SND_SET_PROPS, i, pos[0], pos[1], pos[2], carV);   // родной звук предмета
  int part, brk; propsTypeFx(&props, props.def[i].type, &part, &brk);
  if (brk >= 0) tutEvent(TUT_BREAK);   // подсказка Барта «ломай предметы — будут монеты» (запись PS2 2:20)
  if (brk >= 0) {
    // №923: осколки по ходу машины (dx, dz — направление, gamePropKick); нет направления или BREAK_DIR923_OLD=1 — по осям мира, как было
    if (!BREAK_DIR923_OLD && dx * dx + dz * dz > .25f) fxBurstBreakableYaw(brk, pos, atan2f(dx, dz));
    else fxBurstBreakable(brk, pos, NULL);
  }
  // Правка Роба 493 (10.10): стеклянный щит Krusty (l1_krustyglass_Shape, обломок BreakableObject 19 = eKrustyGlassBreaking) при разбитии даёт монеты — тем
  // же разлётом, что ящик (штатный coinsDrop, пол — земля под щитом). Числа на диске НЕТ (ATC/BreakableObject без монет, разбор 10.10), N — ПРАВИЛО ПОРТА по
  // видео PS2 (27:18, 40:49, 41:00, 41:28), ответ Наблюдателя: счётчик +5 на щит ровно (267→272, 273→278, 281→286), вылетает видимо 5–8, несобранных не осталось
  // → KRUSTY_GLASS_COINS 5; разлёт низкий вокруг и вперёд по ходу машины, без фонтана вверх (полусфера ±90° от направления удара, coinsDropDir).
  // Оговорка: на 25–30 м/с кузов подбирает на лету, на малой скорости собранное может отличаться. KRUSTY_GLASS_COINS_OLD=1 — без монет
#ifndef KRUSTY_GLASS_COINS_OLD
#define KRUSTY_GLASS_COINS_OLD 0
#endif
#ifndef KRUSTY_GLASS_COINS
#define KRUSTY_GLASS_COINS 5
#endif
#define BREAK_KRUSTY_GLASS 19
  if (!KRUSTY_GLASS_COINS_OLD && brk == BREAK_KRUSTY_GLASS && haveCoins) {
    float gy, fl = pos[1];
    if (haveCol && colGround(&world, pos[0], pos[2], pos[1] + 1.f, pos[1] - 40.f, &gy, 0)) fl = gy;
    float dl = sqrtf(dx * dx + dz * dz);
    if (dl > 1e-3f) coinsDropDir(&coins, KRUSTY_GLASS_COINS, pos[0], pos[1], pos[2], fl, dx / dl, dz / dl, 0.f);
    else coinsDrop(&coins, KRUSTY_GLASS_COINS, pos[0], pos[1], pos[2], fl);
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "COINS krusty-glass n=%d at=%.1f,%.1f,%.1f floor=%.1f t=%.1f\n", KRUSTY_GLASS_COINS, pos[0], pos[1], pos[2], fl, smkT); smkWrite(s); }
  }
  if (part >= 3) fxBurst(fxFindInst(part), pos, NULL);
  else if (part == 1)
    for (int k = 0; k < 14; k++) {
      float r1 = rand() * (1.f / RAND_MAX) - .5f, r2 = rand() * (1.f / RAND_MAX), r3 = rand() * (1.f / RAND_MAX) - .5f;
      fxSpawn(FX_SPARK, pos[0], pos[1] + .8f, pos[2], r1 * 8.f + dx * 3.f, r2 * 5.f + 1.f, r3 * 8.f + dz * 3.f);
    }
}
static CarVis cv, oldCv; static int haveCarVis, carSeq; static float camAng;   // carSeq/camAng — служебное (carseq.txt, camang.txt)   // CAR1: визуальный слой машины (viewer/carvis.c)
static CarParams lvlCar; static int lvlCarOk = -9;   // машина уровня из CAR1 v2 (car.h carParamsFromCar1): 1 — прочитана; иначе famil_v
// свет уровня (LGT1 в CAR1 v3): персонажам — солнце и фон skin.h × множители группы sun уровня; машины запечены экспортом.
// lumaKey (служебный «flag lumakey 1», гейт tests/light): машина и Гомер — сплошным цветом-ключом (маска для замера яркости)
static LevLight lvlLight; static int lvlLightOk = -9, lumaKey;
#ifndef SKIN_DATA_LIT
#define SKIN_DATA_LIT 0   // 1 — персонажей на улице освещает вся группа "sun" уровня (3 направленных + фон из данных); 0 — форма порта (skinLightLevel). «flag skindata 1» без пересборки
#endif
static int skinDataLit = SKIN_DATA_LIT;
#define LUMA_KEY_CAR 0xffff00ffu     // ABGR: пурпурный
#define LUMA_KEY_HOMER 0xff00ff00u   // ABGR: зелёный
static void levelLightApply(int ok) {
  unsigned sun, amb;
  if (ok == 1) skinLightLevel(lvlLight.ambS, lvlLight.dirS, &sun, &amb); else skinLightLevel(0, 0, &sun, &amb);
  if (smkOn) { char s[200];
    snprintf(s, sizeof s, "LIGHT level=%u src=%s group=%s ndir=%u amb=%06x ambS=%.3f,%.3f,%.3f dirS=%.3f,%.3f,%.3f skinSun=%08x skinAmb=%08x\n",
             ok == 1 ? lvlLight.level : 0, levLightError(ok), ok == 1 ? lvlLight.group : "-", lvlLight.ndir, lvlLight.amb & 0xffffff,
             lvlLight.ambS[0], lvlLight.ambS[1], lvlLight.ambS[2], lvlLight.dirS[0], lvlLight.dirS[1], lvlLight.dirS[2], sun, amb); smkWrite(s); }
}
static SkinModel *homerM; static Homer homer; static int haveHomer, doorHinge = -1;   // этап 4: Гомер (homer.shr рядом с EBOOT)
#include "smkpilot.h"   // автопилот теста: flag drive SECS (main_loop.h, autodrive == 4)
static SmkPilot smkPilot; static SmkWalk smkWalk; static float smkPilotLogT;
static Traffic traffic; static int haveTraffic; static unsigned trTexBase;   // этап 5: трафик и пешеходы (traffic.shr рядом с EBOOT)
static float trPedMin = -1.f;   // служебное (flag pedtest): ближе всего Гомер подходил к служебному пешеходу, м
static int trLog; static float trLogT = .5f;   // служебное (flag trflog): строка TRF раз в секунду в smoke.txt
static unsigned trSpawnPend;   // правка 328 (flag trflog): машины трафика, рождённые в этот кадр — строка TRFSPAWN по камере кадра
// твёрдые тела для Гомера вне COL1 — цепочка: стейтпропы, трафик (новые системы добавляются сюда же)
static int propsSoftHomer;   // правка 290: служебное «flag propsoftold 1» — пропы насквозь, как было (красный контроль)
static int homerSolidAll(void *u, float *x, float *z, float y, float r, float *top, float *nx, float *nz) {
  (void)u; int hit = 0;
  if (haveSP) hit |= homerSolidSprops(&sprops, x, z, y, r, top, nx, nz);
  if (haveProps && !propsSoftHomer) hit |= propsSolidHomer(&props, x, z, y, r, HOMER_STEP, HOMER_HEIGHT, top, nx, nz);   // правка 290: ящики, гидранты
  if (haveTraffic) hit |= trafficSolid(&traffic, x, z, y, r, top, nx, nz);
  return hit;
}
static void homerDoor(void *u, float open) {   // дверь водителя из анимации Гомера (шарнир CAR1, пока замок цел)
  (void)u; if (doorHinge >= 0 && cv.latched[doorHinge]) cv.th[doorHinge] = open * cv.hinge[doorHinge].max;
}
static int homerDoorCanFn(void) { return doorHinge >= 0 && !mvForcedCar() && cv.latched[doorHinge]; }   // №667: дверь есть только у машины уровня (carvis), и пока замок цел
static int onFoot(void) { return haveHomer && !homerDriving(&homer); }
// №847 (Роб 19.10, TEST39: «когда быстро бегают, из-под ног летит мультяшная пыль маленькими родными спрайтами — в оригинальных файлах»): система
// dashShape из l0N_fx.p3d (dust1.bmp, серые клубы растут 0,3 → 1,2 м и тают за 2 с, ~1 частица на кадр) идёт за ногами, пока герой бежит
// по земле быстрее 6 м/с (бег — клип dash, locoV 9,2 м/с); не бежит — рождение 0, клубы доживают. Прежний клуб на шаг (code_dust, правка 45)
// остаётся только без dashShape в пакете. DASH847_OLD=1 — как было
#ifndef DASH847_OLD
#define DASH847_OLD 0
#endif
static int dashH847;
static void dashDust847(void) {
  if (DASH847_OLD) return;
  int run_ = onFoot() && homer.grounded && homer.speed > 6.f;
  float p_[3] = {homer.x, homer.y + .05f, homer.z}, d_[3] = {sinf(homer.h), 0.f, cosf(homer.h)};
  if (run_) {
    if (!dashH847 || !fxAlive(dashH847)) { int id_ = fxFind("dashShape"); dashH847 = id_ >= 0 ? fxStartSystem(id_, p_, d_) : 0; }
    else { fxMove(dashH847, p_, d_); fxSetRate(dashH847, 1.f); }
  } else if (dashH847 && fxAlive(dashH847)) fxSetRate(dashH847, 0.f);
  if (smkOn) { static float lt_; if (smkT - lt_ >= 1.f || smkT < lt_) { lt_ = smkT; char l_[128]; snprintf(l_, sizeof l_, "DASH847 run=%d speed=%.1f id=%d h=%d alive=%d live=%d t=%.1f\n", run_, homer.speed, fxFind("dashShape"), dashH847, dashH847 ? fxAlive(dashH847) : 0, fxCount(), smkT); smkWrite(l_); } }
}
static void spFxOut(void *u, const SpFxOut *o0) {   // частицы стейтпропов: родная система окна EFX или пресет
  (void)u; SpFxOut o = *o0;
  if (o.sys >= 0) { float p3[3] = {o.x, o.y, o.z}; fxBurstAt(o.sys, p3, NULL, o.t0); return; }   // правка 80: с кадра окна EFX
  float r1 = rand() * (1.f / RAND_MAX) - .5f, r2 = rand() * (1.f / RAND_MAX), r3 = rand() * (1.f / RAND_MAX) - .5f;
  switch (o.preset) {
    case SPFX_GLASS: case SPFX_SPARKS: fxSpawn(FX_SPARK, o.x, o.y, o.z, r1 * 8.f, r2 * 5.f + 1.f, r3 * 8.f); break;
    case SPFX_FIRE: fxSpawn(FX_FIRE, o.x, o.y, o.z, r1 * 1.5f, r2 * 2.f + 1.f, r3 * 1.5f); break;
    case SPFX_SMOKE: fxSpawn(FX_DARK, o.x, o.y, o.z, r1, r2 + .5f, r3); break;
    case SPFX_WATER: fxSpawn(FX_SMOKE, o.x, o.y, o.z, r1 * 1.5f, r2 * 3.f + 5.f, r3 * 1.5f); break;
    default: fxSpawn(FX_SMOKE, o.x, o.y, o.z, r1 * 4.f, r2 * 3.f + .5f, r3 * 4.f); break;   // пыль, щепа, листья, сено, томаты
  }
}
static void spChangeSnd(void *u, unsigned i, float speed) { (void)u; sndObjectHit(SND_SET_SPROPS, i, sprops.run[i].x, sprops.run[i].y, sprops.run[i].z, speed); }
static unsigned hmHits, hmStomps, hmJumps; static float hmVmax;   // служебное: удары, стомпы, прыжки, скорость (flag homerv, tests/controls)
static void homerEvFx(void *u, const HomerEvent *ep) {   // звук событий Гомера (ревью Codex §14), пыль
  (void)u; HomerEvent e = *ep;
  if (e.type == HOMER_EV_HIT) hmHits++; else if (e.type == HOMER_EV_STOMP) hmStomps++; else if (e.type == HOMER_EV_JUMP) hmJumps++;
  if (e.type == HOMER_EV_STOMP) sndStomp(e.x, e.y, e.z);
  else if (e.type == HOMER_EV_JUMP) sndJump(e.x, e.y, e.z);
  else if (e.type == HOMER_EV_LAND) sndLand(0, e.x, e.y, e.z);
  else if (e.type == HOMER_EV_ON_FOOT) { sndOnFoot(1); sndMusic(SND_MUS_ONFOOT); sndMode(SND_MODE_ONFOOT); }
  else if (e.type == HOMER_EV_IN_CAR) { sndOnFoot(0); sndMusic(SND_MUS_FREEROAM); sndMode(SND_MODE_NORMAL); tutEvent(TUT_GAS); }   // подсказка «газ» (PS2 1:50)
  else if (e.type == HOMER_EV_HIT && haveTraffic) trafficKick(&traffic, e.x, e.y, e.z, e.r, e.dx, e.dz);   // пинок пешехода (шкала — событием трафика)
  if (e.type == HOMER_EV_HIT) { int k_ = mvKick(e.x, e.y, e.z, e.r, e.dx, e.dz); if (k_ && smkOn) { char s_[96]; snprintf(s_, sizeof s_, "MSNKICK n=%d t=%.1f\n", k_, smkT); smkWrite(s_); } }   // №843: и персонажей миссии
  // №467 (Роб: выкрики героя пешком при наезде, ускорении, длинном прыжке — фразы диска, случайный вариант): события и реплики уже есть
  // в sound.c (hitbyc / turbo / longjump), но их никто не вызывал. Наезд — HOMER_EV_HURT; ускорение — первый шаг бега после паузы
  // 2 с без шагов; длинный прыжок — второй прыжок в воздухе (jump_dash_in_air). «Уклонение» (dodge) без своего события — не подключено.
  // HOMER_VO_OLD=1 — как было (молчит)
#ifndef HOMER_VO_OLD
#define HOMER_VO_OLD 0
#endif
  if (!HOMER_VO_OLD) {
    static unsigned lastStepUs;
    unsigned nowUs = sceKernelGetSystemTimeLow();
    if (e.type == HOMER_EV_HURT) sndSay(SND_SAY_HITBYC, 0);
    else if (e.type == HOMER_EV_JUMP && e.r > 1.5f) sndSay(SND_SAY_LONGJUMP, 0);
    else if (e.type == HOMER_EV_STEP) { if (nowUs - lastStepUs > 2000000u) sndSay(SND_SAY_TURBO, 0); lastStepUs = nowUs; }
  }
  if (e.type == HOMER_EV_LAND) fxHomerDust(FXD_LAND, e.x, e.y, e.z, e.r);   // правка 45: клуб пыли cloud.tga, не дым машины
  if (e.type == HOMER_EV_STEP && (DASH847_OLD || fxFind("dashShape") < 0)) fxHomerDust(FXD_STEP, e.x, e.y, e.z, e.r);   // №847: есть dashShape — пыль бега она   // правка 45: клуб пыли на шаг бега, как в записи PS2
  if (e.type == HOMER_EV_BOUNCE) { fxHomerDust(FXD_BOUNCE, e.x, e.y, e.z, e.r); sndPlay("trampoline", e.x, e.y, e.z, 1.f);   // правка 46
    sndSay(SND_SAY_SPRINGBOARD, 0); }   // аудит 09.10: W_Springboard «Bouncy bouncy!» — подкидывающая точка (шанс — правило порта)
}
// Аудит 09.10 п. 12/58: W_Fall («AHHH! I'm gonna go boom!») — Гомер пешком падает. ПРАВИЛО ПОРТА: ниже точки отрыва больше
// FALL_SAY_H и всё ещё летит вниз — одна реплика на полёт (высоту, с которой оригинал кричит, запись PS2 не дала)
#define FALL_SAY_H 8.f
static int fallSaid;
static void afterHomerHud(void *u, const HomerCar *hc) { (void)u; int can = onFoot() && homerCanEnter(&homer, hc);
  if (onFoot() && !homer.grounded && homer.vy < 0.f && homer.airFrom - homer.y > FALL_SAY_H) { if (!fallSaid) fallSaid = sndSay(SND_SAY_FALL, 0) | 1; }
  else if (homer.grounded || !onFoot()) fallSaid = 0;
  if (uiReady) uiHudShow(UI_T_ACTION, !uiPaused() && can); if (can) { tutEvent(TUT_GETIN); uiActionText("Get Into Car"); } }   // правка 206: TUTORIAL «PRESS @ TO GET INTO CAR»   // подсказка «△ — сесть» (PS2 1:45)
static void coinSnd(void *u) { (void)u; sndCoin(); tutEvent(TUT_COINS); }   // подсказка «монеты — на машины и одежду» (PS2 2:30)
static void resetMusic(void *u, int wasOnFoot) { (void)u; if (wasOnFoot) { sndMusic(SND_MUS_FREEROAM); sndMode(SND_MODE_NORMAL); } }
static unsigned char spSt[512]; static int spStInit;   // прошлые состояния стейтпропов (звук, шкала H&R); NEW GAME сбрасывает spStInit
static Game G = {&world, &car, &props, &sprops, &coins, &homer, &haveCol, &haveProps, &haveSP, &haveCoins, &haveHomer,
                 &hnrHeat, &hnrCalm, &wasDestroyed, spSt, &spStInit, &cv.lift,
                 {onPropKickFx, spChangeSnd, spFxOut, homerEvFx, afterHomerHud, coinSnd, homerDoor, resetMusic, 0}};
// гаечный ключ взят (пункт 4, viewer/game.c gameWrenchStep): звук wrench_collect (world.spt), частицы wrench_collect2Shape (art/wrench.p3d)
static void onWrench(void *u, const float pos[3], int repaired) {
  (void)u; (void)repaired; sndPlay("wrench_collect", pos[0], pos[1], pos[2], 1.f); tutEvent(TUT_WRENCH);   // подсказка «ключи чинят машину» (PS2 4:15)
  int s = fxFind("wrench_collect2Shape"); if (s >= 0) { float p[3] = {pos[0], pos[1] + .5f, pos[2]}; fxBurst(s, p, NULL); }
}
static SceUID sema; static volatile int req = -1; static volatile unsigned loadedBytes = 0;

static void blackBox(int exitNow);
#include "exit739.h"   // №739: выход через главный поток
static int exitCb(int a, int b, void *c) { blackBox(1); if (EXIT739_OLD) sceKernelExitGame(); else exitRequestWait(); return 0; }   // правка 103: ящик — EXIT
static int cbThread(SceSize a, void *b) {
  int id = sceKernelCreateCallback("exit", exitCb, NULL);
  sceKernelRegisterExitCallback(id); sceKernelSleepThreadCB(); return 0;
}
static void fail(const char *msg) {
  // №775: сторож через 5 с печатает STALL поверх этого экрана и закрывает текст ошибки — текст уходит в шаг (stall.txt: STALLSTEP FAIL …, экран: step=FAIL …)
  static char failStep[96]; snprintf(failStep, sizeof failStep, "FAIL %s", msg ? msg : "-"); bbStep = failStep;
  pspDebugScreenInit(); pspDebugScreenPrintf("SHAR PSP\n\n%s\n\nPress HOME to exit.", msg);
  sceKernelSleepThread();
}
static void sibling(char *out, const char *name) {   // путь к файлу рядом с EBOOT
  if (SHR_REL && shrDevName(name)) { strcpy(out, SHR_OFF_DIR); strncat(out, name, 200); return; }   // релиз (viewer/release.h): служебное недоступно
  strcpy(out, path); char *sl = strrchr(out, '/'); strcpy(sl ? sl + 1 : out, name);
}
// Номер уровня (1..7) и файлы уровня рядом с EBOOT (docs/РЕЦЕПТ_УРОВНЯ.md §3.2, §9). Имя с %d — по номеру уровня
// (interiors-l%d.bin, collect-l%d.shr, zonecam-l%d.shr; миссии level%02d.msn — msn_view.c; газета загрузки LEVEL_IDX).
// Уровень 1 — в корне SHARVIEW, как выложен; уровни 2–7 — весь набор в папке l<N>/ (level.shr, traffic.shr, audio/, ui.shr,
// msnart.shr, fx.shr, msncam.shr и имена с номером), как build/levels/l<N>/ сборки и правила {N} tools/ship_rules.txt.
// Какой уровень: NEW GAME — 1 (служебный режим: выбор L/R в меню), LOAD/RESUME — уровень записи, уровень пройден — следующий
static int gameLevel = 1;
static char levelShr[256];   // level.shr уровня gameLevel (у уровня 1 — тот же файл, что path)
// правка 420 (бонус-игра, viewer/bonus_play.h): пока bonusTrack > 0, набор «уровня» — трасса мини-игры: level.shr -> bonus/b0N.shr,
// машины -> bonus/msncars-b.shr, интерфейс и звук — уровня 1; прочих пакетов уровня (трафик, миссии, карточки, комнаты, камеры зон)
// у трассы нет — имя без файла, модули молчат, как при отсутствии пакета. BONUS_OLD=1 — пункт MINI GAME серый, как было
#ifndef BONUS_OLD
#define BONUS_OLD 0
#endif
static int msnBot; static float msnBotT;   // №бот миссий (flag msnbot 1, MSNBOT_OLD): помощник игрока для ночного бота миссий — пропуск реплик и брифинга, △ и посадка по этапу
static int bonusTrack, mainMini = -1, mainScrap = -1, endingPending;   // endingPending — №860: титры игры ждут главного меню (menuRun)   // mainScrap — №862: SCRAP BOOK главного меню (альбом карточек)
static const MsnCars *bonusCarsPtr; static const char *bonusPlayerModel;   // машины гонки (рисование моделей пакета msncars)
// правка 421: открытые трассы игры в памяти (бит N-1 — трасса N, viewer/bonus_unlock.h). До первой игры — по самому свежему
// сохранению (как автозагрузка оригинала: RESUME GAME), в игре — по карточкам Collect (сбор последней карточки уровня, загрузка, DEV)
static unsigned bonusOpen; static int bonusSession, bonusSvcAll;   // bonusSvcAll — служебное «flag bonusopen» (гейты гонки: все открыты)
static unsigned bonusOpenMask(void) { return bonusSvcAll || BONUS_UNLOCK_OLD ? BONUS_ALL_TRACKS : bonusOpen; }
static int bonusTrackIsOpen(int n) { return bonusTrackOpen(bonusOpenMask(), n); }   // «трасса N открыта?» — для выбора трассы и экранов MiniMenu
static void bonusOpenFromColl(const char *src);   // ниже, у сохранений (нужен Collect coll)
static int bonusAvailable(void) {
  if (BONUS_OLD) return 0;
  if (!bonusOpenMask()) return 0;   // правка 421: ни одной открытой трассы — BONUS GAME серый (запись PS2 ps2_bonus_main_selection)
  char fn[256]; SceIoStat st; sibling(fn, "bonus/b01.shr");
  if (sceIoGetstat(fn, &st) < 0) return 0;
  sibling(fn, BONUS_CARS); return sceIoGetstat(fn, &st) >= 0;
}
static void levelFile(char *out, const char *name) {
  char nm[96], nn[64];
  if (bonusTrack) {
    if (!strcmp(name, "level.shr")) snprintf(nm, sizeof nm, BONUS_SHR, bonusTrack);
    else if (!strcmp(name, "msncars-l%d.shr")) snprintf(nm, sizeof nm, "%s", BONUS_CARS);
    else if (!strcmp(name, "audio/")) snprintf(nm,sizeof nm,"%s",BONUS_SOUND_OLD?"audio/":"bonus/audio/");
    else if (!strcmp(name, "ui.shr") || !name[0]) snprintf(nm, sizeof nm, "%s", name);
    else snprintf(nm, sizeof nm, "bonus/-");
    sibling(out, nm); return;
  }
  if (strchr(name, '%')) snprintf(nn, sizeof nn, name, gameLevel); else snprintf(nn, sizeof nn, "%s", name);
  if (gameLevel > 1) snprintf(nm, sizeof nm, "l%d/%s", gameLevel, nn); else snprintf(nm, sizeof nm, "%s", nn);
  sibling(out, nm);
}
// Codex-3 menus: PauseBgd and the seven album cards belong to the active level.
static void pausePackFile(char *out, const char *name) {
#ifndef C3_LEVEL_MENUS_OLD
  levelFile(out, name);
#else
  sibling(out, name);
#endif
}
