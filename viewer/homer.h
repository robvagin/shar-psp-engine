// Гомер пешком (этап 4): управление, анимация, коллизия с COL1, удары (HIT / STOMP), посадка в машину и выход.
// Модель и клипы — build/homer.shr (tools/homer_export.py, формат CHR1 из skin.h; скиннинг на GE, skin.c).
// Самостоятельный модуль: в main.c не встроен, патч — docs/homer_integration.md. Чистый C99 + skin.c, без malloc в кадре.
//
// ЧИСЛА (оригинал, PAL SLES_518.97 по разбору hit-and-run-web docs/MOVEMENT_PHYSICS.md; клипы и смеси — homer.cho):
//   ходьба 4 м/с (стик до упора), бег 8 м/с (○ зажат), разгон 20 м/с², торможение 10 м/с²; гравитация 25 м/с²;
//   раскладка пешком — диск (frontend.p3d ControllerPS2.pag, подписи «пешком»): X JUMP, □ ATTACK, ○ DASH / SPRINT,
//   △ GET IN CAR / TALK TO CHARACTER; подсказка обучения srr2 «USE LEFT ANALOG STICK TO MOVE AROUND / USE X TO JUMP /
//   PRESS AND HOLD ○ TO RUN»: без ○ Гомер идёт (клип hom_loco_run 3.4 м/с ближе к 4), с ○ бежит (hom_loco_dash 9.2 м/с).
//   Удар: □ на земле — пинок (hom_break), после прыжка — удар в прыжке (hom_jump_kick, «JUMP INTO THE AIR WITH THE X
//   BUTTON AND THEN PRESS THE □ BUTTON TO KICK THE WASP»), после второго прыжка — стомп (X X □)
//   прыжок 1.9 м, второй прыжок +1 м (ДОБИВКА 25: по записи PS2 вершина 1,84 / 2,71 м — см. HOMER_JUMP_PS2_OLD; доступен, пока вертикальная скорость в [-12, 2]); в воздухе 4 м/с, разгон 4.68 м/с²;
//   стомп: гравитация ×3.22; блендинг клипов 0.1 с (cho default), jump_idle 0.05/0.1, jump_dash_in_air 0.5/0.05;
//   локомоция walkerLoco: idle / walk / run / dash по скорости (скорости корня клипов: 0.87 / 3.4 / 9.2 м/с).
//   Наше (в данных нет): капсула r 0.3, ступенька 0.4 м, поворот 720°/с (закомментированное в cho), камера пешком.
//
// КООРДИНАТЫ — как у level.shr / main.c: метры, y вверх; курс h: вперёд = (sin h, 0, cos h). Позиция = ступни.
#ifndef SHAR_HOMER_H
#define SHAR_HOMER_H
#ifndef DRIVER_POSE_OLD
#define DRIVER_POSE_OLD 0   // B5: =1 — поза водителя каждый кадр полностью и без отсечения по кадру (homer.c homerPose, main_loop.h)
#endif
#ifndef DRIVER_BOX_OLD
#define DRIVER_BOX_OLD 0    // №520: =1 — рамка «водитель вне кадра» от прежнего place (после сброса машины водитель пропадал)
#endif
// ===== Внедрение сверки с исходником, окно g04-homer (02.11.2026, правки 1077–1081). Каждый выключатель: =1 — поведение TEST81 =====
// Ссылки «ГОМЕР x.y», «P02-n», «P05-n», «P08-n» — отчёты docs/handoff/СВЕРКА_ГОМЕР.md и СВЕРКА_ПОЛНАЯ/*.md
#ifndef HOMER_STICK_SQ_OLD
#define HOMER_STICK_SQ_OLD 0     // ГОМЕР 1.1: ходьба — скорость по (отклонение²)/0,69 до 1, ×4 м/с; 1 — линейно
#endif
#ifndef HOMER_RUN_FACE_OLD
#define HOMER_RUN_FACE_OLD 0     // ГОМЕР 1.3: бег — ход вдоль лица, лицо тянется к стику с темпом 2/с; 1 — ход в сторону стика, поворот 720°/с
#endif
#ifndef HOMER_DASH_SPEED_OLD
#define HOMER_DASH_SPEED_OLD 0   // P02-3, ГОМЕР 1.4: бег — желаемая скорость сразу 8 м/с при любом ненулевом стике, без разгона; 1 — как ходьба (отклонение×8, разгон 20)
#endif
#ifndef HOMER_TURN_EXP_OLD
#define HOMER_TURN_EXP_OLD 0     // P02-4, ГОМЕР 1.2/2.4: лицо догоняет по экспоненте с темпом 10 (ходьба) / 2 (бег) / 4 (воздух) в секунду; 1 — линейно 720°/с
#endif
#ifndef HOMER_BACKSTICK_OLD
#define HOMER_BACKSTICK_OLD 0    // P02-1, P17-10: стик в задней полуплоскости лица при отклонении < 0,6 гасится до 1 %; 1 — любое отклонение ведёт и разворачивает
#endif
#ifndef HOMER_AIR_PS2_OLD
#define HOMER_AIR_PS2_OLD 0      // ГОМЕР 2.2/2.3, P02-9/10, Д1: воздух — прибавка по стику (0,078 × желаемая скорость за кадр 1/30 с), без стика скорость не меняется, потолок max(4|8, скорость) − (○? 4 : 0); 1 — цель-скорость и потолок скорости отрыва
#endif
#ifndef HOMER_FRAME_PS2_OLD
#define HOMER_FRAME_PS2_OLD 0    // P02-2: направление стика от камеры каждый кадр; на смене камеры держится прежнее, пока стик в 15° от запомненного; 1 — схема №704/836/916/920/947б/1059
#endif
#ifndef HOMER_RUNSLIDE1230_OLD
#define HOMER_RUNSLIDE1230_OLD 0 // №1230 (Роб, L5 E3-5: бежит по косой вдоль перил — встаёт намертво): бег с ○ растил скорость от уже погашенной стеной (cur + 13 м/с² за кадр), а ход каждый кадр заново шёл по лицу, в стену — вдоль стены оставалось ~0,2 м/с вместо v·cos(угла); 1 — как было
#endif
#ifndef HOMER_JUMP_WALL_OLD
#define HOMER_JUMP_WALL_OLD 0    // P02-Д5: в отрыве (прыжок, сход с края) горизонталь 0, если в прошлом шаге касался твёрдого (стена, ящик, персонаж); 1 — горизонталь сохраняется
#endif
#ifndef HOMER_KICK_STOP_OLD
#define HOMER_KICK_STOP_OLD 0    // P02-17: удар на земле — стоп сразу, клип ничем не прерывается, нажатия теряются; 1 — торможение 10 м/с², отмена ходьбой/✕, повтор □
#endif
#ifndef HOMER_JKICK_ONCE_OLD
#define HOMER_JKICK_ONCE_OLD 0   // P02-11, P02-10: удар в прыжке один раз, кадр 6 (0,2 с), после него второй прыжок и стомп закрыты; после схода с края удара нет; 1 — как было
#endif
#ifndef HOMER_STOMP_VY_OLD
#define HOMER_STOMP_VY_OLD 0     // P02-12: стомп — вертикаль на нажатии сохраняется и продолжается после замаха (×3,22); 1 — 0 на замахе и −4 м/с после
#endif
#ifndef HOMER_LANDPOW_OLD
#define HOMER_LANDPOW_OLD 0      // P02-14, ГОМЕР 2.7: приземление — сила пыли в событии LAND (dx: 0,5 / 0,7 после второго прыжка / 1,0 после стомпа; dz: 1 — кольцо); 1 — dx = 0
#endif
#ifndef HOMER_AIR_ACT_OLD
#define HOMER_AIR_ACT_OLD 0      // P02-18: пока идёт действие (воздух, удар, посадка) △ не читается — homerCanEnter только на земле и стоя; 1 — как было
#endif
#ifndef HOMER_ENTER_BOX_OLD
#define HOMER_ENTER_BOX_OLD 0    // ГОМЕР 5.1/6.2: сесть можно внутри объёма «габарит кузова + 1 м во все стороны»; 1 — 5 м от двери / 3,5 м от кузова
#endif
#define HOMER_ENTER_PAD 1.0f
#ifndef HOMER_SLOPE30_OLD
#define HOMER_SLOPE30_OLD 0      // ГОМЕР 4.1: земля круче 30° (нормаль.y ≤ 0,866) — не опора: на неё не ступить; 1 — любой наклон
#endif
#define HOMER_SLOPE_NY 0.866f
#define HOMER_SLOPE_RISE 0.5f   // порт: бордюры и ступени с крутой гранью (до 0,5 м) берутся, сопка круче 30° — нет
#ifndef HOMER_MOUTHNAME_OLD
#define HOMER_MOUTHNAME_OLD 0    // круг 6, герой уровня (L2 Барт, L3 Лиза, L4 Мардж, L5 Апу): рот говорящего игрока берёт строку таблицы своего героя (homerCharName), а не всегда Гомера; 1 — всегда "homer" как в TEST83
#endif
#ifndef CHEERCAR1158_OLD
#define CHEERCAR1158_OLD 0       // правка Роба 1158 (сверено с оригиналом и кругом 5): радость за рулём (малая при этапе, большая при миссии, кайф) играется НА МЕСТЕ ВОДИТЕЛЯ, как весь набор ndr; в оригинале герой остаётся в машине и ни из каких дверей не выходит. Было: у клипов радости корень шёл по собственному движению клипа, и тело уезжало влево сквозь дверь и кузов, радовалось в воздухе сбоку и так же возвращалось. 1 — прежнее
#endif
#ifndef HOMER_ROCKOUT_OLD
#define HOMER_ROCKOUT_OLD 0      // круг 7+, P06:Д5 (сверено с оригиналом): команда миссии AllowRockOut действует ЗА РУЛЁМ, не пешком: если команда у текущего этапа, то через 3 с после посадки (один раз, считая с начала пребывания в машине) покой водителя меняется на кайф ndr_in_car_victory_small по кругу; жесты поверх него, потом снова он. Темп под бит музыки в порту не делается (бита нет): скорость 1. 1 — покой остаётся
#endif
#ifndef HOMER_HITCULL_OLD
#define HOMER_HITCULL_OLD 0      // круг 6, цена на PSP (урок №1086): сбивание машинами миссий проверяется только у машин рядом и быстрее 1 м/с (до: все до 6 машин миссии каждый кадр, с пересчётом матриц кузова); 1 — как в TEST83
#endif
#ifndef HOMER_AIR_TICK_HZ
// круг 7, сверено с оригиналом: прибавка воздуха 0,078 × желаемая скорость и доля поворота лица (темп × dt) считаются КАЖДЫЙ КАДР игры (шаг героя вызывается раз за кадр, на dt прибавка не умножается).
// Расчётный кадр оригинала 33,3 мс (порог размытия при просадке кадра), на PAL с вертикальной синхронизацией реальный кадр кратен 20 мс. 30 Гц — РАСЧЁТНОЕ число; реальная частота PS2 по записи не подтверждена
#define HOMER_AIR_TICK_HZ 30.f
#endif
#ifndef HOMER_RUNRAMP_OLD
#define HOMER_RUNRAMP_OLD 0     // GitHub #2 разбор tests/homer: запись PS2 gh2_dir_right/gh2_stop_all — бег с места разгоняется (путь за 0,6 с 2,3 м, за 1,2 с 7,1 м; мгновенные 8 м/с дали бы 4,8 и 9,6 м), ≈ 13 м/с²; 1 — сразу 8 м/с (сверка круга 3: скорость желаемая ставится сразу, реальный разгон вёл клип)
#endif
#define HOMER_RUN_ACCEL 13.0f    // м/с² разгона бега из ходьбы или с места (по записи PS2)
#ifndef MARGE_HAIR_OLD
#define MARGE_HAIR_OLD 0         // правка 1240 (Роб, PSP): клип сидения водителя ndr (общий, под закрытую кабину) кладёт причёску Мардж (hair_1..3) назад; в открытой машине (список tools/open_cars.txt -> carparams.h carOpenCons) причёска остаётся стоймя; 1 — всегда как в клипе
#endif
#ifndef HOMER_HIGHSEAT_OLD
#define HOMER_HIGHSEAT_OLD 0     // аудит 04.11 (решение дирижёра): высокое сиденье водителя (над началом машины выше 0,1 м, у 23 из 88 машин) — клипы посадки и выхода *_high_driver; 1 — всегда обычные клипы
#endif
#define HOMER_HIGH_SEAT_Y 0.1f   // порог высоты сиденья водителя, м (оригинал: 0,1)
#ifndef HOMER_BRAKE_SNAP_OLD
#define HOMER_BRAKE_SNAP_OLD 0   // GitHub #2 + запись PS2 04.11: конец бега (○ отпущен) режет скорость на земле до шага 4 м/с в тот же тик, дальше торможение 10 м/с²; 1 — 8 → 4 / 8 → 0 торможением 10 м/с²
#endif
#ifndef HOMER_TURNTICK_OLD
#define HOMER_TURNTICK_OLD 0     // круг 7, ГОМЕР 1.2/2.4/P02-4: доля поворота лица за кадр = темп × dt (линейно, как в оригинале), пересчитана на тик HOMER_AIR_TICK_HZ: эффективный темп ходьбы 12,2/с вместо 10; 1 — экспонента с темпом 10
#endif
#ifndef HOMER_CARHIT_ALL_OLD
#define HOMER_CARHIT_ALL_OLD 0   // ГОМЕР 5.7: на ногах сбивают только машины ИИ миссий (HomerCar.hostile), порог 1 м/с; 1 — любая быстрее 3 м/с
#endif
#ifndef HOMER_STUN_OLD
#define HOMER_STUN_OLD 0         // ГОМЕР 8.6: оса — оглушение 0,5 с с мерцанием (homerStun), без отлёта и падения; 1 — вызывающий зовёт homerHurt как раньше
#endif
#ifndef HOMER_STEPFX_OLD
#define HOMER_STEPFX_OLD 0       // P02-Д2: событие STEP на каждое касание ноги при желаемой скорости > 1 м/с (r = желаемая скорость); 1 — только бег
#endif
#ifndef HOMER_STEPSND_OLD
#define HOMER_STEPSND_OLD 0      // круг 6: было 1 — в TEST83 шаги Гомера были БЕЗ ЗВУКА совсем (main_state.h звук не играет, sndStepFoot никто не зовёт); теперь звук шагов играет отсюда один раз на касание ноги. P02-34: звук шага на каждое касание ноги (sndFootstep), набор по svSurfaceAt (бетон/металл/дерево; в помещении бетон); 1 — без звука шагов
#endif
#ifndef HOMER_IDLE_PS2_OLD
#define HOMER_IDLE_PS2_OLD 0     // P02-36, ГОМЕР 1.9: ожидание — 10–20 с случайно, клипы 5/10, 3/10, 2/10, подряд один и тот же не берётся; 1 — 8 с и по очереди
#endif
#ifndef HOMER_JUMPWIND_OLD
#define HOMER_JUMPWIND_OLD 0     // решение дирижёра 04.11 «запись PS2 главнее текста исходника»: заводка прыжка с места ≈ 0,04 с (запись ps2_homer_jump: первый кадр воздуха через 2 кадра после ✕; отрыв → земля 0,72–0,82 с; второй ✕ через 0,32 с срабатывает); 1 — заводка на весь клип (≈ 0,25 с)
#endif
#define HOMER_LAUNCH_S 0.02f
#define HOMER_JUMP2_DH_PS2 0.05f  // второй прыжок нажимается теперь при вертикали ≈ +2,5 (а не у вершины): добавка к высоте, вершина двойного по записи PS2 2,706 м
#define HOMER_JUMP2_UP_PS2 2.7f   // запись PS2: второй ✕ через 0,32 с после первого (вертикаль ≈ +2,1…2,5 м/с) срабатывает; прежний порог HOMER_JUMP2_UP 2,0 его отбрасывал (HOMER_JUMPWIND_OLD=1)
#ifndef HOMER_JUMP_LAUNCH_OLD
#define HOMER_JUMP_LAUNCH_OLD 0  // P02-7, ГОМЕР 2.8: прыжок с места — сначала клип заводки (без движения и гравитации), отрыв и звук после него; на ходу и при падении сразу; 1 — отрыв в тот же кадр
#endif
#ifndef HOMER_DOOR_WALK_OLD
#define HOMER_DOOR_WALK_OLD 0    // P02-32, P05-13: к двери машины пешком (не бежит), скорость min(расстояние/0,1 с, 4); таймаут 1,5 × путь / 4 м/с — ставит в точку у двери; 1 — бег 8 м/с, таймаут 4 с
#endif
#ifndef HOMER_DOOR_PTS_OLD
#define HOMER_DOOR_PTS_OLD 0     // P05-23, ГОМЕР 5.3: угловые точки обхода на 0,53 м по каждой оси от габарита кузова; 1 — на 0,8 / 0,7 м
#endif
#ifndef HOMER_ENTER_WAIT_OLD
#define HOMER_ENTER_WAIT_OLD 0   // P05-24, ГОМЕР 5.2, P02-31: △ у едущей машины — стоит без управления, пока скорость не упадёт до 1 км/ч (запас HOMER_ENTER_WAIT_MAX, затем ирис); 1 — садится на ходу
#endif
#define HOMER_ENTER_WAIT_MAX 3.0f
#ifndef HOMER_CTRL_EARLY_OLD
#define HOMER_CTRL_EARLY_OLD 0   // P02-27, P05-14, P05-Д8: посадка — управление и «в машине» по концу get_into_car, закрытие двери 0,2 с идёт за рулём; выход — закрытие двери обрывается стиком > 0,5 (дверь остаётся как была); 1 — всё до конца клипа
#endif
#ifndef HOMER_GESTURE_OLD
#define HOMER_GESTURE_OLD 0      // P02-29, P05-Д2: жесты водителя — пороги и держание оригинала (руль 0,5 и > 5 км/ч, газ > 8 м/с² и < 20 км/ч, тормоз > 10 м/с² и > 50 км/ч, удар > 25); 1 — пороги порта
#endif
#ifndef HOMER_SEATBOB_OLD
#define HOMER_SEATBOB_OLD 0      // P05-Д1: фигура за рулём качается по вертикали вслед за толчками машины (порог 1 м/с², k 0,003, предел 0,11 м); 1 — жёстко на якоре
#endif
#ifndef HOMER_EXIT_TOP_OLD
#define HOMER_EXIT_TOP_OLD 0     // P02-28: выход, когда у двери занято — ирис, Гомер на крыше на 0,2 м выше без подскока; 1 — подскок 0,5 м без ириса
#endif
#ifndef HOMER_PAD_ANY_OLD
#define HOMER_PAD_ANY_OLD 0      // P02-Д4, P02-16: подкидывающая точка срабатывает при входе в объём в любой фазе (стоя, бег, воздух); горизонталь не больше 25 м/с; 1 — только стоя
#endif
#ifndef HOMER_AIRCLIP_OLD
#define HOMER_AIRCLIP_OLD 0      // P02-Д6, P05-26: клип воздуха замедляется, чтобы дойти до конца к приземлению (скорость min(1, остаток/время до земли)); 1 — скорость 1
#endif
#ifndef HOMER_RIDE_OLD
#define HOMER_RIDE_OLD 0         // P02-22: на крыше едущей машины Гомер едет вместе с ней (положение и поворот), + сёрфинг (P02-23, P05-17); 1 — крыша лишь высота земли
#endif
#ifndef HOMER_STEP6_OLD
#define HOMER_STEP6_OLD 0        // ГОМЕР 4.2: ступенька до 0,6 м (луч вниз от прошлой высоты + 0,6); 1 — 0,4 м
#endif
#ifndef HOMER_LANDCLIP_OLD
#define HOMER_LANDCLIP_OLD 0     // P02-15, ГОМЕР 2.7: клип «приземление на месте» играет без стика и обрывается любым ненулевым стиком или ✕ (не только > 0,1); 1 — порог 0,1
#endif
#ifndef HOMER_SURF_OLD
#define HOMER_SURF_OLD 0         // P02-23, P05-17: сёрфинг на крыше едущей (> 10 м/с) машины без ввода — surf_in, surf_cycle, surf_out; 1 — без сёрфинга
#endif
#ifndef HOMER_CAMLAG_OLD
#define HOMER_CAMLAG_OLD 0       // P08-3/4 (+ поправка), 2.x: камера пешком — глаз догоняет Гомера с запаздыванием 0,08 за 16 мс (τ≈0,19 с), точка взгляда отдельным фильтром 0,044 за 16 мс (63 % за 0,29 с, вдвое быстрее при падении > 1 м/с); высота взгляда замирает, если камера не выше цели больше чем на 0,5 м; 1 — всё жёстко
#endif
#ifndef HOMER_CAMAIR_OLD
#define HOMER_CAMAIR_OLD 0       // P08-6, P08-50, 2.5: в воздухе (прыжок, отлёт, выброс) высота камеры замирает на высоте отрыва, курс — по закону 1059 с настоящей скоростью; 1 — +0,6 м и коэффициент 1,6
#endif
#ifndef HOMER_CAMFLIP_D_OLD
#define HOMER_CAMFLIP_D_OLD 0    // P08-5: после перескока у стены расстояние 4,26 м (минимум камеры из данных); 1 — 4,0 м
#endif
#ifndef HOMER_CAMGOAL_OLD
#define HOMER_CAMGOAL_OLD 0      // P08-Д1: пешком ближе 10 м к предмету цели миссии — камера становится за спину Гомера лицом к предмету (homerCamGoal); 1 — без притяжения
#endif
#ifndef HOMER_CAMLOOK_MIN_OLD
#define HOMER_CAMLOOK_MIN_OLD 0  // P08-Д10: расстояние от глаза до точки взгляда не меньше 1 м; 1 — без проверки
#endif
#ifndef HOMER_CEIL_OLD
#define HOMER_CEIL_OLD 0         // P02-13: голова о потолок — вертикаль в ноль, падение, позиция на 0,1 ниже; 1 — потолка нет
#endif
#ifndef HOMER_TALK_OLD
#define HOMER_TALK_OLD 0         // P15-15: новая реплика не прерывает играющий жест; 1 — заменяет
#endif
#ifndef HOMER_KICKSEG_OLD
#define HOMER_KICKSEG_OLD 0      // круг 2, ГОМЕР 3.1: пинок на земле — точка в 0,7 м вперёд и 0,7 м вверх, радиус 0,5 м, момент 0,25 с (отрезок 2 м под 45°); 1 — сфера 1 м в 0,85 вперёд и 0,45 вверх, 8/30 с
#endif
#ifndef HOMER_SWITCHCLIP_OLD
#define HOMER_SWITCHCLIP_OLD 0   // круг 2, P05:3: нажатие выключателя играет клип hit_switch; 1 — без клипа
#endif
#ifndef HOMER_DODGE_OLD
#define HOMER_DODGE_OLD 0        // круг 5, ГОМЕР 5.5 / P05-22 / P02-33: выход из уничтоженной машины — ждёт 0,5 с, поворот лицом наружу от водительской двери, на землю, клип уворота (npd_dodge) с его движением корня, потом управление; 1 — полёт головой вперёд по записи PS2 (правки 73, 147)
#endif
#ifndef HOMER_LOOKBACK_OLD
#define HOMER_LOOKBACK_OLD 0     // круг 5, P02-29: задний ход — взгляд назад (ndr_look_back), держится на кадре 25 и ещё 22 тика после конца реверса; 1 — без взгляда
#endif
#ifndef HOMER_INTERIOR_OLD
#define HOMER_INTERIOR_OLD 0     // P02-6, ГОМЕР 1.6/3.6: внутри интерьера бег и прыжок отключены, удар играет клип но не бьёт (homerIndoor); 1 — без ограничений
#endif
#ifndef HOMER_BODY_R_OLD
#define HOMER_BODY_R_OLD 0       // P02-21: тело — круг радиуса 0,35, центр на 0,05 назад от оси; 1 — 0,30 по оси
#endif
#include "skin.h"
#include "col.h"
#include "car.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- параметры (оригинал; см. шапку)
#define HOMER_WALK        4.0f
#define HOMER_RUN         8.0f
#define HOMER_ACCEL       20.0f
#define HOMER_DECEL       10.0f
#define HOMER_GRAVITY     25.0f
#ifndef HOMER_JUMP_PS2_OLD
#define HOMER_JUMP_PS2_OLD 0     // ДОБИВКА 25: 1 — прежние 1.9 / 1.0 (разбор web-порта); 0 — по записи PS2 (корень 0xa245b0)
#endif
#if HOMER_JUMP_PS2_OLD
#define HOMER_JUMP_H      1.9f
#define HOMER_JUMP2_H     1.0f
#else
// Запись PS2 09.10 (наблюдатель, корень SimState 0xa245b0): ~/Downloads/shar-psp-ps2-w/build/twins/o4/ps2_homer_jump —
// вершина стоя 1,8345 м выборочно / 1,8383 парабола (g 25,19); ps2_homer_double_jump (второй X через 0,32 с) — вершина
// 2,7062 / 2,7121. Значения ниже дают ту же вершину у порта с его шагом интегрирования 1/120 с (moveBody): 1/60 —
// 1,838 / 2,699, 1/30 — 1,838 / 2,723 (tests/homer_jump/host.sh). Разбор: tools/twins/homer_jump_ps2.py
#define HOMER_JUMP_H      1.878f
#define HOMER_JUMP2_H     0.945f
#endif
#define HOMER_JUMP2_UP    2.0f     // второй прыжок: вертикальная скорость не больше этого
#define HOMER_JUMP2_DOWN  12.0f    // ... и падение не быстрее этого
#define HOMER_AIR_SPEED   4.0f
#define HOMER_AIR_ACCEL   4.68f    // 0.078 * 60
#define HOMER_STOMP_G     3.22f
// ---- наше
#if HOMER_BODY_R_OLD
#define HOMER_RADIUS      0.30f    // капсула по xz
#define HOMER_BODY_BACK   0.f
#else
#define HOMER_RADIUS      0.35f    // сверка g04-homer P02-21: цилиндр тела радиуса 0,35 (было 0,30); центр на 0,05 назад от оси
#define HOMER_BODY_BACK   0.05f
#endif
#define HOMER_HEIGHT      1.80f
#ifndef HOMER_STEP
#if HOMER_STEP6_OLD
#define HOMER_STEP        0.40f    // ступенька/бордюр, на которые заходит сам (-DHOMER_STEP=0.004f — красный контроль гейта garage_ramp)
#else
#define HOMER_STEP        0.60f    // сверка g04-homer ГОМЕР 4.2: луч вниз с высоты прошлой позиции + 0,6 м — ступенька до 0,6 м
#endif
#endif
#ifndef HOMER_STAIR_UP
#define HOMER_STAIR_UP 0      // №гараж АЭС L1: лесенка из плит (уступы до HOMER_STAIR_MAX) — Гомер встаёт сам; 0 до эталона PS2
#endif
#define HOMER_STAIR_MAX 1.6f
#define HOMER_STAIR_CHAIN_T 1.2f   // с: после подъёма ровные площадки и дорога в ту же сторону тоже берутся
#define HOMER_STAIR_NY 0.966f  // верх опоры не круче ~15°
#define HOMER_SNAP        0.45f    // вниз по лестнице/с бордюра не отрываясь от земли
#define HOMER_TURN        12.57f   // рад/с (720°/с)
#define HOMER_KICK_REACH  1.0f     // радиус события HIT вокруг точки удара
#define HOMER_STOMP_R     2.0f     // радиус события STOMP вокруг ступней
#define HOMER_ENTER_DIST  5.0f     // △ работает, если до двери водителя не дальше (м)
#define HOMER_ENTER_BODY  3.5f     // ... или до кузова с любой стороны не дальше (м)
#ifndef HOMER_DOOR_STALL_OLD
#define HOMER_DOOR_STALL_OLD 0   // правка 285: 1 — прежнее (к недоступной двери 4 с на месте)
#endif
#ifndef HOMER_IRIS_PATH
#define HOMER_IRIS_PATH   4.5f     // правка 212: путь к двери водителя длиннее (м) — посадка под ирисом (homer.c HS_TO_DOOR); со стороны водителя (до 5 м) — пешком, в обход — ирисом
#endif
#ifndef HOMER_IRIS_CAR_OLD
#define HOMER_IRIS_CAR_OLD 0     // правка 333: 1 — прежнее (машины с SetIrisTransition садятся и выходят клипами двери); служебно — flag caririsold
#endif

// ---- состояния
enum { HS_FOOT, HS_KICK, HS_STOMP, HS_HURT, HS_TO_DOOR, HS_GET_IN, HS_IN_CAR, HS_GET_OUT };

// ---- события наружу (homerPopEvent)
enum { HOMER_EV_HIT = 1,     // пинок / удар в прыжке: сфера (x,y,z,r), направление (dx,dz) — стейтпропам SP_EV_HIT, пропам — толчок
       HOMER_EV_STOMP,       // стомп: сфера вокруг ступней — стейтпропам SP_EV_STOMP (ящик сразу 30 монет)
       HOMER_EV_JUMP,        // прыжок (r = 1 первый, 2 второй) — звук/пыль
       HOMER_EV_LAND,        // приземление (r = скорость падения, м/с) — пыль, звук
       HOMER_EV_HURT,        // удар током / сбит: монеты разлетаются (coinsDrop в x,y,z)
       HOMER_EV_IN_CAR,      // сел: управление машиной
       HOMER_EV_ON_FOOT,     // вышел: управление Гомером
       HOMER_EV_EJECT,       // выброшен взрывом машины (r = сила, dx dz — куда летит); ON_FOOT приходит тоже
       HOMER_EV_BOUNCE,      // подкидывающая точка (правка Роба 46): взлёт с неё (x,y,z — точка на земле, r — высота до вершины, м;
                             // dx dz — куда летит) — звук trampoline, клуб пара/пыли
       HOMER_EV_STEP };      // шаг бега (правка Роба 45): на земле, клип бега hom_loco_dash преобладает — два шага на его цикл
                             // (фаза шага 0 и 0.5); x,y,z — ступни, r — скорость. Клуб пыли cloud.tga (слот 2 частиц оригинала
                             // «dust cloud when running/jumping»; запись PS2 18:13: клуб у ступни на каждый шаг бега)
// Г4 / правка Роба 142 (01.10, решено терминалом): «край без дна» — стена и для Гомера, как у машины (viewer/car.c, «край
// мира» why 16/32). Ночь 30.09→01.10: все 20 верхних мест провала под мир — Гомер пешком; он уходил туда, где под ступнями
// нет никакой опоры коллизии до самого низа: за край земли Intersect (дорога у склона холма, обрыв у забора), под
// нависающую землю (склон холма над дорогой — опора только выше ступенек) и на уступ выше HOMER_STEP без стенки (l1z2, 0.6 м).
// Оракул PS2 (build/ps2ref/falls, 30.09 22:59, те же места прошлой ночи): тело оригинала там стоит или падает на землю ниже,
// под мир не уходит нигде, кроме зон DEATH (там возвращает зона). Правило порта: шаг в точку, под которой нет опоры не выше
// ступенек, отменяется (как стена); исключения — щель до HOMER_CRACK с опорой по обе стороны и следы зон DEATH (туда можно,
// возвращает deathCheck, как в оригинале). Геометрию не добавляет и не вырезает. 0 — как до правки (отрицательный контроль)
#ifndef HOMER_VOID_WALL
#define HOMER_VOID_WALL 1
#endif
// Разбор ночи 11.10 (выпадения за мир, «КОЛЛИЗИИ_НОЧЬ_РАЗБОР»): Гомер стоит на крыше машины у обрыва/под нависающей землёй (крыша — не
// опора мира), прыгает, сходит с крыши — под ним по всей колонке нет опоры коллизии: падал 60 м до сброса airFrom - 60, а «надёжное
// место» (safe) запоминалось и с крыши — возврат туда снова над пустотой. Правило: колонка без опоры мира ниже тела (не след зоны DEATH) —
// возврат на «надёжное место» после HOMER_VOID_DROP м падения (≈ 1 с), а safe хранит только опору мира. Геометрию не меняет.
// 0 — как до правки (отрицательный контроль tests/falls, tools/bots/fallhunt)
#ifndef HOMER_VOID_RECOVER
#define HOMER_VOID_RECOVER 1
#endif
#define HOMER_VOID_DROP 6.0f
// Правка 431: постановка Гомера обязана пройти проверку «опора под точкой» (homerPlace), см. поле spawnWait. HOMER_SPAWNWAIT_OLD=1 — как было (ставит и отпускает на любой точке)
#ifndef HOMER_SPAWNWAIT_OLD
#define HOMER_SPAWNWAIT_OLD 0
#endif
#ifndef HOMER_SPAWN_WAIT_T
#define HOMER_SPAWN_WAIT_T 6.0f    // сколько секунд ждать опору, прежде чем искать рядом / возвращать: перенос миссии в комнату ждёт карточку брифинга (~1 с), подготовку фазы (до 90 кадров) и ирис — запас; дольше — данные не те
#endif
#define HOMER_SPAWN_RING_R 12.f    // радиус поиска ближайшей опоры при тайм-ауте, м
#define HOMER_SPAWN_PROBE 100.f    // как далеко вниз от точки homerPlace искал опору всегда (ниже — не опора)
#define HOMER_CRACK 1.0f     // = CAR_CRACK (viewer/car.c): щель уже 2 м с опорой по обе стороны — не край
#ifndef HOMER_STEP_DUST
#define HOMER_STEP_DUST 1    // отрицательный контроль tests/jumps (--gate): 0 — шагов бега нет (как до правки 45)
#endif
typedef struct { int type; float x, y, z, r, dx, dz; } HomerEvent;

// вход за кадр
typedef struct {
  float mx, my;                // стик: вправо +, вперёд (вверх) +; -1..1, мёртвая зона уже вычтена
  float camYaw;                // курс камеры (вперёд = (sin, cos)); движение — относительно камеры
  unsigned char jump, attack;  // нажатия В ЭТОМ кадре: X прыжок, □ удар (пинок / в прыжке / стомп после второго прыжка);
                               // △ — main.c: homerEnterCar/ExitCar, двери, разговор
  unsigned char dash;          // ○ зажат: бег HOMER_RUN (DASH / SPRINT); без него стик — ходьба HOMER_WALK
  unsigned char stomp;         // служебное: стомп сразу в воздухе — прежняя раскладка порта (○ до 29.09), только
                               // отрицательный контроль tests/controls (Game.footPadOld)
} HomerInput;

// машина для посадки/выхода и толкания (homerCarFrom заполняет из car.h)
typedef struct {
  float m[16];                 // модель машины -> мир (carPieceMatrix(c, lift, 0,0,0, 1)): смотрит в -Z, +X вправо
  float lift;                  // модель на lift над низом колёс
  float x, y, z, h, v;         // низ колёс, курс, скорость
  CarBox box;                  // кузов (carBox)
  float roof;                  // высота крыши (на неё можно запрыгнуть)
  int ok;
  int dead;                    // машина взорвана (остов): сесть нельзя (txtbible: «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A PHONEBOOTH»)
  float door[2];               // точка у двери водителя (x, z модели; car.p.doorX/Z): отсюда начинается посадка
  int high;                    // аудит 04.11: сиденье водителя выше 0,1 м над началом машины (car.p.seat[1], сустав dl) — посадка и выход клипами *_high
  float seatY; int seatData;   // №666: высота сиденья над землёй (car.p.seat[1] + lift, сустав dl) и признак «из данных машины»
  float seatA[2];              // якорь посадки (car.p.seatAx/Az): конец клипа посадки ставит Гомера на сиденье; у машины, где
                               // дверь дальше от борта (коробка кузова шире кабины), якорь доезжает от двери за время клипа посадки
  int husk;                    // правка 355: остов после взрыва — посадка/выход без двери и ириса (HOMER_HUSK_SEAT)
  int hostile;                 // сверка g04-homer ГОМЕР 5.7: машина ИИ миссии (погоня, цель) — только такая сбивает пешего (ставит вызывающий после homerCarFrom)
  int openTop;                 // правка 1240: машина без верха над сиденьем (carOpenFor, список tools/open_cars.txt) — причёска Мардж за рулём стоймя
  int iris;                    // правка 333: в .con машины SetIrisTransition(1) (car.c carIrisFor) — посадка и выход ирисом, без клипов двери
} HomerCar;
void homerCarFrom(HomerCar *hc, const Car *c, float lift);
void homerSetDoorOut(float m);   // Роб 17.10: насколько дальше от борта стоит у двери, м (0,12 по умолчанию; 0 — как было) — рука не уходит в дверь

// дверь водителя: open 0 закрыта .. 1 открыта (main.c крутит шарнир двери CarVis); done: 1 = сел, 0 = вышел
typedef struct {
  void (*door)(void *user, float open);
  void (*done)(void *user, int inCar);
  void *user;
} HomerCarCb;

// Дополнительные твёрдые тела (не в COL1): стейтпропы и т.п. Вытолкнуть круг (x, z, радиус r) из всего, что выше
// ступеньки над ступнями y; в *top — самая высокая опора под (x, z) не выше y + HOMER_STEP (можно стоять), иначе не трогать.
// Возврат: 1 — вытолкнули (нормаль в *nx, *nz). Готовая реализация для стейтпропов — homerSolidSprops (user = StateProps*).
typedef int (*HomerSolidFn)(void *user, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
struct StateProps;
extern unsigned homerSolidFrame;   // != 0 только внутри homerUpdate: твёрдость предметов считает список «кто рядом» раз за вызов (HOMER_SOLID_CACHE_OLD=1 — выкл.)
int homerSolidSprops(void *stateProps, float *x, float *z, float y, float r, float *top, float *nx, float *nz);

#define HOMER_MAXEV 8
typedef struct {
  const SkinModel *m;
  int state;
  // --- тело
  float x, y, z, h;            // ступни, курс модели
  float vx, vy, vz;            // скорость, м/с
  int grounded, jumps;         // на земле; прыжков с земли (0, 1, 2)
  float camTurn;               // правка Роба 75: L/R пешком — довернуть камеру (-1 влево, +1 вправо), main.c каждый кадр
  float groundY, airFrom;      // высота земли под ногами; с какой высоты начался полёт
  float speed;                 // горизонтальная скорость, м/с
  float st;                    // время в текущем состоянии, с
  int phase;                   // подфаза состояния (стомп: 0 замах, 1 падение, 2 приземление; машина: 0 открыть, 1 сесть/выйти, 2 закрыть)
  int hitSent;
  float idleT;                 // сколько стоит без дела (длинные idle)
  // --- машина
  HomerCar car; HomerCarCb cb; float door;
  float anchor[3];             // точка у двери в осях модели машины (ступни в начале посадки)
  // --- анимация
  int clip, clipLoop; float clipT, clipRate, clipFrom;   // одиночный клип (не локомоция); clip < 0 — локомоция
  int rootMode;                // 1 — поза с переносом корня (посадка/выход/в машине), в системе машины
  float phaseLoco, idleClipT;  // фаза шага (0..1), время idle
  float fade, fadeLen;         // переход из снимка позы snap
  int cLoco[4], cTake, cAir, cLand, cDash2, cBreak, cJKick, cStA, cStAir, cStLand,
      cOpenIn, cIn, cCloseIn, cOpenOut, cOut, cCloseOut, cInCar, cFlail, cGetUp, cIdle[3];
  int hInOpen, hIn, hOut, hCloseOut;   // аудит 04.11: клипы *_high_driver (-1 — нет в пакете, тогда обычные); активные на текущую посадку/выход — aIn, aOpenIn, aOut, aCloseOut
  int aOpenIn, aIn, aOut, aCloseOut; float hiOfs[3][2];   // сдвиг корня высоких клипов (x, z): каждый клип начинается там, где кончился предыдущий
  // правки Роба 220/231: за рулём — набор водителя ndr оригинала (cInCar = ndr_in_car_idle; нет в пакете — hom_in_car_idle):
  // жесты поворота руля влево/вправо, газа, тормоза, удара (-1 — нет клипа). Вход — homerDriveInput (main.c / game.c каждый кадр)
  int cDrvTurn[2], cDrvGas, cDrvBrake, cDrvCrash;
  float drvSteer, drvGas, drvBrake, drvSpeed, drvPrevSteer, drvPrevGas, drvPrevBrake; int drvCrash, drvGestures;
  float locoV[4], locoStride[4];
  float seatRoot[3];           // корень позы «сидит» (конец get_into_car_close_door_driver)
  SkinXf xf[SKIN_MAXJ], tmp[SKIN_MAXJ], snap[SKIN_MAXJ];
  ScePspFMatrix4 pal[SKIN_MAXJ] __attribute__((aligned(16)));
  ScePspFMatrix4 palFresh[SKIN_MAXJ] __attribute__((aligned(16))), palPrev[SKIN_MAXJ] __attribute__((aligned(16)));   // B5: последние две полные позы водителя (homerPose)
  int drvPoseValid, drvPoseTick;   // B5: палитры palFresh/palPrev годны; чётность кадра
  ScePspFMatrix4 world[SKIN_MAXJ] __attribute__((aligned(16)));
  ScePspFMatrix4 place __attribute__((aligned(16)));       // модель Гомера -> мир (для skinDrawPal)
  SkinPlayer blink;            // только моргание (clip -1)
  // --- твёрдые тела вне COL1 (необязательно)
  HomerSolidFn solid; void *solidUser;
  // --- события
  HomerEvent ev[HOMER_MAXEV]; int nev;
  // --- камера пешком
  float camYaw, camY, frame, heldX, heldY, neutralT, steerGap, sweepT; int camInit, flipSeen, flipLatch; float flipLatchT;   // №920: neutralT — сколько стик уже в нуле (кадр управления держится STEER_NEUTRAL_T); steerGap — сколько steer не вызывался (после машины, пинка, падения кадр не считается свежим)
  float safe[3], launchV; int idleNext;
  // правка 431 (канон Роба 06.11, «никто не появляется раньше сцены»): постановка без опоры под ногами не отпускает тело в физику.
  // spawnWait: 0 — нет; 1 — homerPlace не нашёл опору в своём мире; 2 — мир при постановке не был известен (w = NULL), проверка в первом кадре.
  // Пока ожидание: тело не двигается, не падает, не рисуется (homerSpawnPending); опора появилась (в мире кадра) — встаёт на неё. Не появилась за HOMER_SPAWN_WAIT_T —
  // ближайшая опора в HOMER_SPAWN_RING_R, иначе последнее надёжное место (safe), иначе spawnLost = 1 (вызывающий ставит от машины / старта)
  int spawnWait, safeOk, spawnLost; float spawnT, spawnX, spawnY, spawnZ;
  int spawnHolds, spawnFixes, spawnRings, spawnSafes;   // счётчики для гейта и журнала
  // --- отладка / тесты
  float maxY, lastLandV; int landings;
  int voidRecovers;            // сколько раз возвращён с колонки без опоры (HOMER_VOID_RECOVER), для гейта fallhunt
  int stairUps;                // лесенка: сколько раз встал на уступ (HOMER_STAIR_UP)
  float stairChain, stairDx, stairDz;   // лесенка: остаток секунд цепочки подъёмов и её направление
  int voidStops;               // Г4: сколько подшагов отменено правилом «край без дна» (HOMER_VOID_WALL), для гейта tests/falls
  int voidFloors;              // правка 142 / ДОБИВКА 13: подшагов на полу над пустотой (voidFloor), для гейта tests/falls/void_floor
  // --- камера пешком и машина рядом (Роб 28.09: «подхожу к машине — вижу внутренности»): машина из последнего homerUpdate,
  // дистанция камеры и ближняя плоскость для main.c (камера у кузова/стены не режет кузов и Гомера)
  HomerCar camCar; float camDist, camNear;
  float camFlipT;              // CAM_WALL_PS2: с последнего перескока камеры на другую сторону (с), не чаще раза в 2 с
  float camPullD, camRise;     // правка 40: допустимая дистанция камеры (стена за спиной — сразу ближе, отпускает плавно) и подъём
  float camFx, camFz;          // правка 223: точка, за которой идёт камера (на посадке — держится, не дёргается с телом)
  float camHoldD, camHoldE;    // правка 349 (В86): поза камеры после переноса зоной DEATH — дистанция и глаз (0 — обычная камера)
  int camHits;                 // сколько лучей камеры упёрлось в коллизию в последнем кадре (журнал, гейт tests/camera)
  int overTop;                 // последний выход из машины — через верх (у двери было занято), для тестов и журнала
  // --- переходы (viewer/transit.c, пакет «переходы как в оригинале»): счётчики для ириса и журнала
  int irisOn;                  // 1 — вызывающий покажет ирис: посадка при занятой двери — сразу на сиденье (под ирисом)
  float stallD, stallT;        // правка 285: путь к двери — лучший остаток и время без продвижения (упёрся — ирис)
  int irisSeat;                // ++ при такой посадке (main.c по смене счётчика закрывает/открывает ирис)
  int huskIn, huskOut;         // правка 355: ++ при посадке / выходе у остова (без двери и ириса)
  int irisCarIn, irisCarOut;   // правка 333: ++ при посадке / выходе ирисом у машины с SetIrisTransition (transitHomerWhy: car_in / car_out)
  int ejected;                 // ++ при выбросе взрывом (homerEject)
  int carHits; float carHitCool;   // №709: ++ при ударе машины по Гомеру пешком (падает и встаёт); пауза до следующего удара, с
  // --- правка Роба 147: выброс взрывом как в оригинале — задержка в горящем остове, полёт головой вперёд (тело наклонено),
  // касание грудью, лёжа, hom_get_up. pitch — наклон тела вокруг оси X модели (0 стоит, pi/2 ничком, голова по курсу)
  float pitch, ejWait, ejPow, ejFromX, ejFromZ; int ejPh, jExtra; int hair[4], nHair;   // hair[] — суставы причёски hair_* (правка 1240)
  // jExtra — сустав Extra_Attributes (не часть тела)   // ejPh: 0 нет, 1 полёт, 2 касание/скольжение, 3 встаёт
  // --- разговор миссии (правка Роба 38): жест реплики (клип dialogue_* из homer.shr, AddAmbientPcAnimation) и рот
  int cTalk, jaw; float jawOpen;
  // --- подкидывающие точки (правка Роба 46; блок JMP1 COL1, tools/jumps_export.py): последняя точка (-1 — вне точек;
  // пока стоит в той же, второй раз не подбрасывает), полёт с неё (без стика воздух скорость не гасит), счётчик для тестов
  int padLast, bouncing, bounces; float padApex[3];
  // --- сверка g04-homer (02.11.2026)
  float supY;                  // ГОМЕР 4.1: высота последней опоры не круче 30° (подъём по круче больше чем на HOMER_SLOPE_RISE над ней — стена)
  float wantV;                 // желаемая скорость от стика в этом кадре, м/с (событие шага, звук шага)
  int wallHit;                 // в прошлом подшаге касался твёрдого (стена, ящик, персонаж) — P02-Д5
  float wallSpd;               // №1230: скорость хода (скаляр) до гашения стеной в последнем касании этого кадра; 0 — касания не было (сбрасывается в moveBody)
  int jkicked, edgeFall;       // удар в прыжке был (до приземления); сошёл с края без прыжка — P02-11 / P02-10
  float stompVy;               // вертикаль в момент стомпа — P02-12
  float stunT;                 // оглушение осой, с (homerStun)
  float frameHold, frameStick; int frameLatch; float prevCamYaw, frameOffT;   // P02-2: держание направления на смене камеры
  float launchWait;            // P02-7: сколько ещё идёт заводка прыжка с места, с
  float idleWait; int idleLast;   // P02-36: случайная пауза до длинного idle и прошлый выбор
  float lkX, lkY, lkZ; int lkInit;   // точка взгляда камеры пешком (фильтр, P08-3/4)
  float stepPh; int indoor;    // шаги: фаза; признак интерьера (homerIndoor)
  float airCap; int airRun, dashHeld;   // круг 3: потолок горизонтальной скорости в полёте и бег на отрыве (запоминаются при отрыве)
  int cDodge, cDrvBack, cCarVic[2]; float gRevT;   // круг 5: клипы уворота, взгляда назад, радости за рулём (малый, большой); время после реверса
  float inCarT; int rockOn;    // круг 7+: время в машине с посадки, с; кайф включён (3 с с флагом rockOut)
  int rockOut;                 // круг 2: этап с AllowRockOut. Круг 7+: ставит game_msn.c по команде этапа; кайф — за рулём, клип ndr_in_car_victory_small (см. HOMER_ROCKOUT_OLD)
  float celT; int celKind;     // праздничный клип: 0 нет, 1 малый (стик прерывает), 2 большой (не прерывает)
  int celClip;                 // клип праздника, который играет
  int cSurf[3], surfSt;        // сёрфинг на крыше едущей машины: surf_in / surf_cycle / surf_out; 0 нет, 1 вход, 2 цикл, 3 выход
  int onCar, carPrevOk; float carPrevX, carPrevZ, carPrevAng, carPrevRoof;   // стоит на крыше машины и её прошлое положение (едет вместе с ней)
  float doorT, doorBudget; int doorHold;   // посадка: время подхода к двери и его бюджет, дверь остаётся открытой при выходе (круг 6: поле ожидания остановки машины waitCar убрано — ожидания нет, посадка идёт сразу ирисом)
  int gKind; float drvPrevSpd;                      // жест водителя: 1 руль, 2 газ, 3 тормоз; прошлая скорость машины (круг 6: неиспользуемое gHold убрано)
  float bobOff, bobPrevY, bobPrevVy; int bobInit;   // качание фигуры водителя
} Homer;

// модель — skinLoad("homer.shr"); 0 = ок, <0 — в модели нет нужных клипов
int homerInit(Homer *H, const SkinModel *m);
// правка 352: префикс клипов последней homerInit (hom_ / brt_ / lsa_ / mrg_ / apu_ — игрок уровня, пакет l<N>/player.shr)
const char *homerClipPrefix(void);
// круг 6: имя героя игрока по префиксу клипов (hom_ homer, brt_ bart, lsa_ lisa, mrg_ marge, apu_ apu) — для таблицы рта; HOMER_MOUTHNAME_OLD=1 — всегда "homer"
const char *homerCharName(void);
// поставить на землю под (x, y + 2, z) с курсом h, пешком
extern int homerRoomPlace96; extern int (*homerRoomPointHook)(float x, float y, float z);   // Build 96: roomplace вместо hold для постановки на пол комнаты (hook ставит движок и бот)
void homerPlace(Homer *H, const ColWorld *w, float x, float y, float z, float h);
// Правка 431, вторая половина канона: «геометрия» под точкой постановки. Крючки ставит движок (main_loop.h): готова ли сцена города под точкой (клетки в памяти) и «нужна клетка под точкой»
// (лестница памяти окна «Меню»). NULL (боты, хост) — сцена всегда готова. failed = 1 — клетка не загрузилась вообще (state 3, нет памяти), а не «ещё грузится».
// Пока сцены нет, а опора есть, герой ждёт так же (не рисуется, не двигается), до HOMER_SPAWN_WAIT_T; потом отпускается со строкой WATCHDOG cellfail. HOMER_SCENEGATE_OLD=1 — без проверки сцены
#ifndef HOMER_SCENEGATE_OLD
#define HOMER_SCENEGATE_OLD 0
#endif
extern int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
extern void (*sceneWantHook)(const ColWorld *w, float x, float z);
static inline int homerSpawnPending(const Homer *H) { return H->spawnWait != 0; }   // правка 431: ждёт опору — не двигается, не рисуется (main_loop.h)
// шаг кадра. car — машина игрока (NULL — нет): посадка/выход/в машине и толкание кузовом
void homerUpdate(Homer *H, const ColWorld *w, const HomerCar *car, const HomerInput *in, float dt);
// посадка: △ у машины. 1 = пошёл к двери (дальше сам: подойти, открыть, сесть, закрыть -> cb.done(1)), 0 = далеко/занят
int homerEnterCar(Homer *H, const HomerCar *car, const HomerCarCb *cb);
// выход: △ в машине. 1 = начал (открыть, выйти, закрыть -> cb.done(0)), 0 = не в машине
int homerExitCar(Homer *H, const HomerCar *car, const HomerCarCb *cb);
// сесть сразу (старт уровня в машине), без анимации
void homerSetInCar(Homer *H, const HomerCar *car);
// удар током / сбит машиной: откинуть от (fromX, fromZ), флейл, встаёт. Событие HURT
void homerHurt(Homer *H, float fromX, float fromZ, float power);
// сверка g04-homer ГОМЕР 8.6: попадание осы — оглушение на sec (0,5) с мерцанием модели, без отлёта и падения; событий не шлёт (5 монет и размытие экрана — у вызывающего, collect.c / game.c)
void homerStun(Homer *H, float sec);
// сверка g04-homer ГОМЕР 5.7: машина ИИ миссии (c->hostile = 1) наехала на пешего — сбить (падает и встаёт); 1 — сбит. Зовёт traffic.c / car.c, передано g02-traffic / g03-aicars
int homerKnockedByCar(Homer *H, const HomerCar *c);
// круг 6 (цена на PSP): стоит ли вообще строить HomerCar для этой машины и звать homerKnockedByCar — рядом (кузов + 1 м) и быстрее 1 м/с по модулю. HOMER_HITCULL_OLD=1 — всегда 1
int homerCarWorth(const Homer *H, const Car *c);
// круг 2, P05:3: Гомер нажимает выключатель (клип hom_hit_switch, стоя). yaw — курс лицом по локатору (NAN — не поворачивать). 1 — клип запущен
int homerPressButton(Homer *H, float yaw);
// круг 2, P06:Д5: AllowRockOut — пока этап идёт, в покое играет рок-стойка (клипа в homer.shr нет: флаг хранится, играет, когда клип выгрузят)
void homerRockOut(Homer *H, int on);
// сверка g04-homer P02-6: игрок внутри интерьера (комната, магазин) — бег и прыжок отключены, удар играет клип, но по предметам не бьёт; ставит game.c каждый кадр до homerUpdate
static inline void homerSetIndoor(Homer *H, int on) { H->indoor = on; }
// сверка g04-homer P05-1 / P02-24: праздничный клип — big=0 малый (конец этапа; обрывается стиком), big=1 большой (карточка, миссия пройдена; ничем не прерывается). В машине водителя — in_car_victory_*_driver. 1 — запущен
int homerCelebrate(Homer *H, int big);
// взрыв машины (Роб 28.09: «в оригинале при взрыве персонаж вылетал из машины»): Гомер в машине / садится / выходит —
// вылетает от центра взрыва (fromX, fromZ) со стороны двери водителя (занята — с крыши в другую сторону), флейл, встаёт.
// Пешком к двери — просто останавливается. События ON_FOOT и EJECT (без HURT: монеты не разлетаются). 1 — выброшен
#define HOMER_EJECT_POWER 0.8f     // правка Роба 73 (запись PS2 33:36: Гомер падает рядом с остовом): «не так сильно, в сторону» — ×0.8 (3.2 м/с вбок, 4 вверх)
#define HOMER_EJECT_Y     .5f      // правило порта: старт над землёй у двери (высота сиденья)
// Правка Роба 147 (01.10: «летят руками вперёд, и потом стоят, очухиваются; летят вперёд, а не сразу на ноги»).
// Данные диска: у Гомера для этого только hom_flail (17 кадров) и hom_get_up (31 кадр, кадр 0 — ничком, голова 0.22 м над землёй
// и 0.65 м впереди ступней). Запись PS2 (33:33.43 удар → 33:34.23 у двери → 33:34.30 летит горизонтально головой вперёд →
// 33:34.50 касание грудью, ноги вверх → 33:34.83 лежит → 33:34.90 get_up → 33:35.63 бежит). Числа ниже — правило порта,
// подобраны по записи (дальность ≈3–5 м от двери, ±30% — рост Гомера в кадре)
#define HOMER_HUSK_SEAT .7f        // правка 355 [PS2 близнец]: △ у остова -> за рулём через ~0,7 с, без двери и ириса
#define HOMER_EJECT_DELAY .7f      // запись: от взрыва до выброса Гомер ещё в горящем остове
#define HOMER_EJECT_VH    10.f     // ×power (0.8) = 8 м/с вбок: низко и быстро
#define HOMER_EJECT_VY    2.5f     // ×power = 2 м/с вверх
#define HOMER_EJECT_TILT  .13f     // с: корпус до горизонтали (33:34.23 стоит → 33:34.30 горизонтально)
#define HOMER_EJECT_SLIDE .3f      // с: касание грудью и кувырок ног (33:34.50–33:34.77)
#define HOMER_EJECT_ROCK  .6f      // рад (≈35°): ноги взлетают выше горизонтали при касании
#define HOMER_EJECT_REST  .35f     // рад (≈20°): лежит чуть головой вниз — на груди и кистях, как кадр 0 get_up (голова 0.22 м)
#define HOMER_EJECT_FRIC  26.f     // м/с²: скольжение по земле гаснет за время касания
#define HOMER_EJECT_PIV   .65f     // м: ось наклона на этой высоте модели (голова лежащего — на 0.65 м впереди ступней, как кадр 0 get_up)
#define HOMER_EJECT_CLEAR .03f     // м: суставы наклонённого тела не ниже опоры + это (кисти, носки — не в землю; касание грудью и кувырок)
#define HOMER_EJECT_LIE   .3f      // м: ось наклона над землёй, когда лежит (голова ≈0.22 м над землёй, как кадр 0 get_up)
// правка 147: в машине вызов только взводит выброс, сам выброс — в homerUpdate через HOMER_EJECT_DELAY (ejected++ тогда же);
// 1 — выброс взведён (или, с -DHOMER_EJECT_V1, прежний мгновенный выброс спиной вперёд — отрицательный контроль гейта)
int homerEject(Homer *H, const ColWorld *w, const HomerCar *car, float fromX, float fromZ, float power);
// забрать событие (0 = пусто)
int homerPopEvent(Homer *H, HomerEvent *e);
// попадает ли событие HIT/STOMP в объект со сферой (cx,cy,cz,r)
int homerHitTest(const HomerEvent *e, float cx, float cy, float cz, float r);
// точка подбора монет (coinsUpdate(..., inCar = 0)); в машине — центр машины
void homerCoinPoint(const Homer *H, float p[3]);
static inline int homerOnFoot(const Homer *H) { return H->state != HS_IN_CAR && H->state != HS_GET_IN && H->state != HS_GET_OUT && H->state != HS_TO_DOOR; }
static inline int homerDriving(const Homer *H) { return H->state == HS_IN_CAR; }
// сверка g04-homer P10-Д7: кабинный мотор (звук) выключается и позиционный холостой ход включается в КОНЦЕ анимации выхода, а не в начале. Для звука main_loop.h (передано g08-sound / g10-flow); управление по-прежнему homerDriving
static inline int homerCabEngine(const Homer *H) { return H->state == HS_IN_CAR || H->state == HS_GET_OUT; }
// можно ли сесть (у двери водителя ближе HOMER_ENTER_DIST или у кузова ближе HOMER_ENTER_BODY)
int homerCanEnter(const Homer *H, const HomerCar *car);
// правки 220/231: руль (−1 влево .. 1 вправо), газ, тормоз 0..1, скорость машины м/с, удар в этом кадре — для жестов водителя
void homerDriveInput(Homer *H, float steer, float gas, float brake, float speed, int crash);
// камера пешком: плавно за спиной (своя, в данных оригинала её нет). Выход: глаз и точка взгляда
extern float (*homerCamRayExtra)(const float a[3], const float b[3]);   // правка 284: доля луча до кузова машины (1 — свободно); NULL — только мир
extern int homerCamHeroOld;   // правка 361: 1 — прежнее: после выброса камера по курсу машины, в полёте курс не догоняет (красный контроль)
void homerCamera(Homer *H, const ColWorld *w, float dt, float eye[3], float look[3]);
// Правка 349 / ДОБИВКА 18 ([PS2] В86, knowledge/facts/ps2-r6-foot-death.yaml; запись ps2-w build/twins/o4/
// ps2_v86_r6_foot_death_physics/trace.tsv, кадры 606–906): после переноса пешком зоной DEATH активной остаётся камера пешком оригинала, её глаз
// переставляется сразу (кадр 608, первый после переноса) — по горизонтали 4,165 м от Гомера С ЛИЦА (по его курсу), на 1,891 м выше
// ступней, взгляд в точку 1,0 м над ступнями (= HOMER_CAM_LOOK_H); стоя так и остаётся (до кадра 906). Здесь: камера сразу в эту
// позу; пошёл / повернул камеру — дистанция и высота уходят к обычным 2,5 м/с (правило порта, как отпуск стены — правка 40)
void homerCamDeathPose(Homer *H);
#define HOMER_CAM_DEATH_DIST 4.165f   // [PS2] В86 trace.tsv кадр 608: |cam − Гомер| по горизонтали
#define HOMER_CAM_DEATH_EYE 1.891f    // [PS2] В86 trace.tsv кадр 608: cam y − Гомер y
int camOverGround(const ColWorld *w, float x, float z, float y);   // правка 40: под глазом камеры есть опора коллизии
extern int homerSpawnWait431;   // правка 431: 1 — ожидание опоры под точкой постановки (HOMER_SPAWNWAIT_OLD=1 / flag spawnhold 0 — как было)
extern int homerSceneGate431;   // правка 431: 1 — герой ждёт сцену под точкой (HOMER_SCENEGATE_OLD=1 / файл spawnholdold / flag spawnhold 0 — как было)
extern int homerRunSlide1230;   // №1230: 1 — бег вдоль стены с косым касанием держит скорость v·cos(угла), скользит; HOMER_RUNSLIDE1230_OLD=1 / файл runslide1230old / flag runslide1230 0 — как было (красный контроль tests/rail1230)
extern int homerIrisCarOld;   // правка 333: 1 — прежнее (клипы двери и у машин с SetIrisTransition); красный контроль tests/homer/iris_car.py
extern int homerWalkCam1059Old, homerViewOk; extern float homerWalkCamTau, homerWalkCamRate, homerViewYaw;   // №1059
extern int homerCtrl916Old, homerExitYawOld, homerCtrlFreezeOld, homerCtrlOn, homerCamBusy; extern float homerCtrlYaw;   // homerCamBusy — №836: камера в переходе/вращается (ставит main_loop.h)   // №704: курс управления, замороженный на время облёта камеры
extern int homerFlipStandOld;   // HOMER_FLIP_STAND_OLD=1 / flag homerflipstandold 1 — перескок камеры у стены и у стоящего Гомера (как было)
extern int homerCamCarOld;   // №982: 1 — машина у спины Гомера, как стена: перескок камеры через героя и глаз в кузове (flag camcar982old)
extern float homerCamCarTop;   // №982: верх кузова машины, задержавшей луч камеры в последнем вызове homerCamRayExtra (-1e9 — не было)
extern int homerCamFlipOld, homerCamFlipN, homerFlipBusyOld, homerFlipHold;   // №916в: перескок камеры у стены одним кадром (CAM_FLIP1_OLD=1 / flag camflip1old 1 — как было); счётчик перескоков
extern int homerCamPs2Old;   // CAM_WALL_PS2_OLD=1 / flag camwallps2old 1 — стена за камерой пешком как раньше (подъезд + подъём), не как у PS2
extern float homerCamDbg[8];
extern int homerCarPinOld;   // 1 — как в TEST89 (упор луча в кузов подтягивает камеру пешком к герою)
extern int homerCamWallOff;   // 1 — камера пешком без обхода стен (служебное, красный контроль tests/camera)
// №996 (Роб 26.10: «настройка высоты и дальности камеры в меню, и в машине, и пешком»): множители страницы CAMERA паузы (Height, Distance; 1 — как было): камера пешком и камера за машиной
// (main_camera.h chaseCamUpdate). Задаёт cfgApply (viewer/main_save.h). CAMCFG996_OLD=1 / файл camcfg996old рядом с EBOOT / «flag camcfg996 0» — множители всегда 1
extern float homerCamUserD, homerCamUserH;
// №1231 (Роб, TEST93: «пешком зажать L и R вместе и двигать стик — камера облетает Гомера и наклоняется, Гомер стоит»): main_loop.h ставит homerCamLR = 1, пока пешком зажаты обе кнопки, и кладёт стик
// в homerCamLRx/y (−1..1, мёртвая зона снята; бег при этом выключен). Влево-вправо — курс камеры (homer.camTurn, как у одиночных L/R, но с аналоговой скоростью), вверх-вниз — высота глаза
// (наклон, homer.c homerCameraAt); отпустил L или R — наклон возвращается за HOMER_CAMLR_RETURN_S, курс остаётся (дальше камеру ведёт обычная погоня за спину при ходьбе). CAMLR1231_OLD=1 / flag camlr1231 0 / файл camlr1231old — как было
extern int homerCamLR, camLR1231; extern float homerCamLRx, homerCamLRy, homerCamLRpitch;
extern float homerCamDist, homerCamEyeH, homerCamLookH;   // камера пешком (HOMER_CAM_DIST / _EYE_H / _LOOK_H; служебное — красный контроль)
#ifndef HOMER_CAM_SEAT_K
#define HOMER_CAM_SEAT_K (1.f / 3.8f)   // правка 223: на посадке камера догоняет Гомера с постоянной 3,8 с (близнец В23); 0 (-D) — жёстко, как было
#endif
#define HOMER_CAM_NEAR 1.5f          // ближняя плоскость камеры, когда рядом ничего нет (= NEAR_Z main.c)
#define HOMER_CAR_CLEAR .6f          // камера не ближе к кузову машины (порт: иначе ближняя плоскость режет кузов)
// разговор (msn_view.c): жест реплики — клип «dialogue_...» (суффикс имени; NULL / "none" / нет клипа — стоит как стоял),
// играет один раз стоя (движение — отмена); рот — открытие челюсти, рад (0 — закрыт; skinJawOpen)
void homerTalk(Homer *H, const char *gesture);
static inline void homerJaw(Homer *H, float rad) { H->jawOpen = rad; }
// Камера пешком как у оригинала (близнец 02.10, камера PS2 по кадрам cam_pos/cam_fwd + Гомер 0xa24a70: build/twins/o4/
// v23_exit_cam/walk.tsv, v12_booth_cam/walk.tsv): стоя — 6,45 м за спиной по горизонтали, глаз на 2,38 м над ступнями, взгляд вниз
// 12° — в точку ~1,0 м над ступнями (на ходу отстаёт до 7 м). Было своё правило порта: 4,2 м, глаз 1,9 м, взгляд в 1,2 м
#ifndef HOMER_CAM_DIST
#define HOMER_CAM_DIST 6.45f         // камера пешком: расстояние за спиной без препятствий
#endif
#ifndef HOMER_CAM_EYE_H
#define HOMER_CAM_EYE_H 2.38f        // глаз над ступнями
#endif
#ifndef HOMER_CAM_LOOK_H
#define HOMER_CAM_LOOK_H 1.0f        // точка взгляда над ступнями
#endif
// Угол кадра пешком: у оригинала у́же, чем у камеры машины (близнец 02.10 build/twins/scen/full/camfov: кадр порта в позе камеры
// PS2 против кадра PS2; PS2 в своём широком режиме держит горизонтальный угол: пешком ~68,6°, машина ~77°, будка 80° — данные).
// Порт держит вертикаль кадра 4:3 оригинала (Hor+, FOVY 62 у машины = 4:3 оригинала): пешком 4:3 — 54°
#ifndef HOMER_CAM_FOVY
#if CAM_554_OLD
#define HOMER_CAM_FOVY 54.f
#elif CAM_LENS_OLD
#define HOMER_CAM_FOVY 48.f
#else
#define HOMER_CAM_FOVY 43.1f
#endif
#endif
#define HOMER_CAM_MIN 1.2f           // ближе к Гомеру камера не подъезжает — поднимается (правка 40)
#ifndef HOMER_CAM_SIDE
#define HOMER_CAM_SIDE .6f           // правка 160: полуширина ближней плоскости 16:9 с запасом (тест: -DHOMER_CAM_SIDE=.3f — прежнее)
#endif
#define HOMER_CAM_UPDN .35f
#define HOMER_CAM_MARGIN .35f        // зазор от стены до глаза (ближняя плоскость 0.3 м — стена не режет кадр)
// сверка g04-homer P08-Д1: предмет цели миссии, к которому пешком тянется камера (ближе 10 м). on=0 — цели нет. Ставит game_msn.c каждый кадр по этапу «дойти» с рисуемым предметом
void homerCamGoal(int on, float x, float y, float z);
// поза для кадра (после homerUpdate; для невидимого — можно не звать)
void homerPose(Homer *H);
extern int (*homerDoorCan)(void);   // №667: 1 — у рисуемой машины водительская дверь может открыться (NULL — да, как прежде)
int homerSeatProbe(const Homer *H, float out[9]);   // №666: замер посадки для журнала (homer.c)
// рамка водителя в машине для отсечения по кадру (мир): 1 — рамка в bb[6] (xmin ymin zmin xmax ymax zmax), 0 — не за рулём; от текущей матрицы машины (№520)
int homerDriverBox(Homer *H, float bb[6]);
#ifndef SKIN_HOST
// нарисовать (внутри открытого списка GE; меняет состояние — см. skin.h, после — resetState() в main.c)
void homerDraw(const Homer *H);
#endif

#ifdef __cplusplus
}
#endif
#endif
