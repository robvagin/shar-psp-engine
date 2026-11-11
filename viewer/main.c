// SHAR main assembly: included modules share the original static state and order.
// One translation unit; no gameplay/ABI/optimizer boundary changes.
#ifndef MAIN_MODULES_OLD
#define MAIN_MODULES_OLD 0
#endif
#if MAIN_MODULES_OLD
#include "../build/main278/legacy.c" // prepared by tests/main_split/check.py
#else
#include "zone3_api.h"   // П5 Зоны-95: области «миссия» и «место» (zone3.c)
#include "main_state.h"
#include "toonflat.h"
#include "carfill930.h"   // №930: куски машин трафика заливкой (мультик)
#include "pfetch.h"
#include "flatdiag.h"
#include "pano917io.h"   // №917: кольцо-панорама дали — загрузка (читает загрузчик клеток); рисует pano917.h после ленты
#include "main_stream.h"
#include "transit_fsm.h"   // Build 95 П2: машина состояний переходов (типы и ldrRun)
#include "main_camera.h"
#include "main_render.h"
#include "memladder.h"   // правка 435: лестница памяти (канон п.13)
#define ZONEALLOC_IMPL
#include "zonealloc.h"   // П1-95: интерфейс зон памяти (заглушка поверх кучи, реализация здесь; viewer/zonealloc.h)
#include "main_probe.h"
#include "stripbench.h"   // тур полос (служебный файл stripbench): без файла не исполняется
#include "main_rooms.h"
#include "main_collect.h"
#include "main_frontend.h"
#include "main_save.h"
#include "main_scrap.h"   // №1105: SCRAP BOOK целиком
#include "dalimp.h"   // ориентиры дали (Д1 разбора arch-fable): башни и щиты вдали не режутся таблицей дали
#include "dal877.h"   // №877: таблица дали бота-репортёра
#include "sign1178.h"   // №1178: вывески и щиты вдали резче (боковая таблица sign1178.bin)
#include "drip770.h"   // №770: капли из труб — векторным столбом света
#include "main_levels.h"
#include "halftone.h"     // полутон теней и света (слово Роба 17.10)
#include "lightsprite.h"   // №599: постоянный свет таблицей спрайтов (LIGHT_SPRITE_OLD=1 — прежний путь)
#include "spritelight.h"
#include "glow1261.h"   // №1261: свечение дверей и будок (процедурно)
#include "oldtv.h"       // №800: «старый телевизор» (строка Old TV на странице CARTOON)
#include "coinspr.h"   // монеты спрайтами одной пачкой (Роб 17.10)    // №658: тени спрайтами одним модулем (flag shadowspr 1)
#include "mem1060.h"   // №1060: будка всегда (сцена, пересборка кучи, возврат)
#define TFSM_IMPL
#include "transit_fsm.h"   // ldrRun: реализация после mem1060.h (прелоадер, шкала, возврат клеток)
#include "transit_booth.h"   // Build 95 П2: класс BOOTH на ldrRun
#include "main_loop.h"
#endif
