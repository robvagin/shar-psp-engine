// SHAR main assembly: included modules share the original static state and order.
// One translation unit; no gameplay/ABI/optimizer boundary changes.
#ifndef MAIN_MODULES_OLD
#define MAIN_MODULES_OLD 0
#endif
#if MAIN_MODULES_OLD
#include "../build/main278/legacy.c" // prepared by tests/main_split/check.py
#else
#include "main_state.h"
#include "toonflat.h"
#include "carfill930.h"   // №930: куски машин трафика заливкой (мультик)
#include "pfetch.h"
#include "flatdiag.h"
#include "pano917io.h"   // №917: кольцо-панорама дали — загрузка (читает загрузчик клеток); рисует pano917.h после ленты
#include "main_stream.h"
#include "main_camera.h"
#include "main_render.h"
#include "main_probe.h"
#include "stripbench.h"   // тур полос (служебный файл stripbench): без файла не исполняется
#include "main_rooms.h"
#include "main_collect.h"
#include "main_frontend.h"
#include "main_save.h"
#include "dal877.h"   // №877: таблица дали бота-репортёра
#include "drip770.h"   // №770: капли из труб — векторным столбом света
#include "main_levels.h"
#include "halftone.h"     // полутон теней и света (слово Роба 17.10)
#include "lightsprite.h"   // №599: постоянный свет таблицей спрайтов (LIGHT_SPRITE_OLD=1 — прежний путь)
#include "spritelight.h"
#include "oldtv.h"       // №800: «старый телевизор» (строка Old TV на странице CARTOON)
#include "coinspr.h"   // монеты спрайтами одной пачкой (Роб 17.10)    // №658: тени спрайтами одним модулем (flag shadowspr 1)
#include "mem1060.h"   // №1060: будка всегда (сцена, пересборка кучи, возврат)
#include "main_loop.h"
#endif
