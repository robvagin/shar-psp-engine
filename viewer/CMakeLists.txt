cmake_minimum_required(VERSION 3.10)
project(sharview C)
add_executable(sharview load_fuse_io.c msio.c geguard.c main.c game.c hud.c col.c car.c props.c coins.c fx.c skid.c groundshade.c carvis.c stateprops.c ui.c ui_sfx.c tone.c surface.c svdata.c skin.c homer.c watchdog.c watchdog_game.c sound.c audio.c movie.c frontend3d.c police_fx.c msncar.c game_msn.c shop.c shop_view.c phone_view.c wardrobe_view.c msn_view.c msnart.c mission/msn_file.c mission/msn_run.c mission/msn_ops.c mission/msn_game.c rooms.c roomcam.c carzone.c traffic.c collect.c gag_skin.c coll_stream.c transit.c msn_audio.c animobj.c savegame.c savemenu.c lang.c savefile.c msncam.c convcam.c tutorial.c memwatch.c gevb.c bonus.c bonus_audio.c ticket.c ticket_view.c fe_audio.c ending_view.c reliab.c uidiag.c bootmark.c)

# TEST93 (Данные-93, C2): список обязательных файлов набора для проверки полноты при старте (viewer/setcheck.h) генерируется из tools/ship_files.sh
# (tools/gen_setfiles.sh -> viewer/setfiles.inc); пересчёт — когда изменились правила выпуска или сам скрипт. Нет bash/ошибка — остаётся закоммиченный файл
add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/setfiles.inc"
  COMMAND sh -c "bash '${CMAKE_SOURCE_DIR}/../tools/gen_setfiles.sh' '${CMAKE_SOURCE_DIR}/setfiles.inc' || touch '${CMAKE_SOURCE_DIR}/setfiles.inc'"
  DEPENDS "${CMAKE_SOURCE_DIR}/../tools/ship_rules.txt" "${CMAKE_SOURCE_DIR}/../tools/gen_setfiles.sh"
  COMMENT "setfiles.inc: обязательные файлы набора" VERBATIM)
add_custom_target(setfiles_inc DEPENDS "${CMAKE_SOURCE_DIR}/setfiles.inc")
add_dependencies(sharview setfiles_inc)

# правки 252/259 (03.10): кэш карты ARK-4 — все sceIoRead/sceIoWrite движка идут через viewer/msio.c (по одному, сброс кэша)
target_compile_options(sharview PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/msio.h")   # SHELL: — CMake склеивает повторы «-include»
target_compile_options(sharview PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/geguard.h")   # правки 259/252: матрицы в GE — только конечные
# -O2 + аппаратный sqrt (без errno) + float-константы: раньше собиралось вообще без оптимизаций (-O0)
target_compile_options(sharview PRIVATE -O2 -g -Wall -Wno-misleading-indentation -fno-math-errno -fsingle-precision-constant)
# -g (аудит 01.10 22:35): строки кода в ELF для tools/crash_addr.py; код PRX не меняется (prxgen убирает отладку)
# pspjpeg + psputility: интро (viewer/movie.c) — аппаратный JPEG из модуля avcodec прошивки
target_link_libraries(sharview pspkubridge pspgum pspgu pspdebug pspdisplay pspctrl psprtc psppower pspge pspaudio pspjpeg psputility m)

# релиз (viewer/release.h, tools/final-build.sh --release): служебные файлы-флаги, журналы, SELECT=RESET CAR и счётчик выключаются; по умолчанию всё OFF
# оптимизация по файлам (docs/handoff/ОПТИМИЗАЦИЯ_НЕСТАНДАРТНАЯ_2026-10-10.md, п. 6): -DSHAR_OPT="-Os" добавляет флаги ПОСЛЕ -O2 (последний побеждает) для всех единиц;
# -DSHAR_OPT_COLD="-Os" только для холодного кода (интерфейс, меню, сохранения, ролики, рамки) — кэш команд 16 КиБ. По умолчанию пусто: сборка прежняя
set(SHAR_OPT "" CACHE STRING "extra compile options after -O2 for all sources")
set(SHAR_OPT_COLD "" CACHE STRING "extra compile options for cold sources (ui, menus, saves, movie, shop views)")
if(SHAR_OPT)
  separate_arguments(_shr_opt UNIX_COMMAND "${SHAR_OPT}")
  target_compile_options(sharview PRIVATE ${_shr_opt})
endif()
if(SHAR_OPT_COLD)
  separate_arguments(_shr_cold UNIX_COMMAND "${SHAR_OPT_COLD}")
  set(_shr_cold_files ui.c ui_sfx.c savemenu.c savegame.c savefile.c movie.c shop.c shop_view.c phone_view.c wardrobe_view.c msn_view.c ticket.c ticket_view.c ending_view.c tutorial.c memwatch.c)
  foreach(f ${_shr_cold_files})
    set_source_files_properties(${f} PROPERTIES COMPILE_OPTIONS "${_shr_cold}")
  endforeach()
endif()

foreach(v SHAR_JITTER_DEFAULT SHAR_RELEASE SHAR_RELEASE_KEEP_DIAG SHAR_RELEASE_KEEP_HUD SHAR_RELEASE_KEEP_RESET SHAR_RELEASE_KEEP_SMOKE)
  if(${v})
    target_compile_definitions(sharview PRIVATE ${v}=1)
  endif()
endforeach()
# правка 103: учёт отказов памяти во всех модулях (viewer/memwatch.c)
target_link_libraries(sharview "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=memalign,--wrap=free,--wrap=sceIoClose")   # sceIoClose: ловушка 05.10 (main.c __wrap_sceIoClose)
# правки 269/273/276, ступень 0 (проба Media Engine): сторона игры viewer/me/meprobe.c — только по флагу сценария «meprobe»;
# модуль ядра sharme.prx (код ME, метод MIST, viewer/me/vendor — MIT) собирается отдельно: make -C viewer/me, правило в
# tools/ship_rules.txt — как ловушка вылетов viewer/kexc
target_sources(sharview PRIVATE me/meprobe.c)
# правка 313: частицы и свечение (viewer/fxdsp.c) — с ключами модуля ME (viewer/me/Makefile): -G0, циклы не в memset/memcpy, деление
# без ловушки; тогда машинный код счёта у главного процессора и у ME один (сверка — tests/me/run.py)
target_sources(sharview PRIVATE fxdsp.c)
set_source_files_properties(fxdsp.c PROPERTIES COMPILE_OPTIONS "-G0;-fno-pic;-fno-tree-loop-distribute-patterns;-mno-check-zero-division")
# TEST 15: функция для ME в памяти игры (viewer/me_gametask.c) — те же ключи, что у кода ME
target_sources(sharview PRIVATE me_gametask.c)
target_sources(sharview PRIVATE me_math.c)
set_source_files_properties(me_math.c PROPERTIES COMPILE_OPTIONS "-G0;-fno-pic;-fno-tree-loop-distribute-patterns;-mno-check-zero-division")
set_source_files_properties(me_gametask.c PROPERTIES COMPILE_OPTIONS "-G0;-fno-pic;-fno-tree-loop-distribute-patterns;-mno-check-zero-division")
target_sources(sharview PRIVATE collectfx.c)   # 04.10: эффект цели миссии SetCollectibleEffect (данные viewer/collectfx_data.h, tools/collectfx_export.py)
target_sources(sharview PRIVATE camsafe.c)   # правка 331: камера никогда не внутри геометрии (viewer/camsafe.h)
# фон/иконка в меню PSP — только локальные файлы (официальный арт, в git не попадает: build/ в .gitignore)
set(PBP_EXTRA "")
if(EXISTS "${CMAKE_SOURCE_DIR}/../build/assets/pic1.png")
  list(APPEND PBP_EXTRA BACKGROUND_PATH "${CMAKE_SOURCE_DIR}/../build/assets/pic1.png")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/../build/assets/icon0.png")
  list(APPEND PBP_EXTRA ICON_PATH "${CMAKE_SOURCE_DIR}/../build/assets/icon0.png")
endif()
# правка Роба 110: живой значок XMB при наведении (ICON1.PMF 144x80, ~5 с из роликов диска; tools/icon1.py)
if(EXISTS "${CMAKE_SOURCE_DIR}/../build/assets/icon1.pmf")
  list(APPEND PBP_EXTRA ANIM_PATH "${CMAKE_SOURCE_DIR}/../build/assets/icon1.pmf")
endif()
# правка Роба 130: музыка в XMB при наведении (SND0.AT3, ATRAC3: отрывок мелодии телефонной будки muzak; tools/snd0.py)
if(EXISTS "${CMAKE_SOURCE_DIR}/../build/assets/snd0.at3")
  list(APPEND PBP_EXTRA MUSIC_PATH "${CMAKE_SOURCE_DIR}/../build/assets/snd0.at3")
endif()
# правка Роба 146: подпись игры в XMB «как родное» (прежде «SHAR Springfield Viewer»; tools/pbp_gate.py TITLE).
# PARAM.SFO пишем сами с VERBATIM: create_pbp_file отдаёт TITLE в shell без кавычек, и «&» рвёт команду
# (те же ключи mksfoex, что у create_pbp_file: MEMSIZE=1, APP_VER пустой); команда стоит раньше — POST_BUILD по порядку
set(SHAR_SFO "${CMAKE_BINARY_DIR}/PARAM.SFO")
add_custom_command(TARGET sharview POST_BUILD
  COMMAND "${PSPDEV}/bin/mksfoex" "-d" "MEMSIZE=1" "-s" "APP_VER=" "The Simpsons Hit & Run" "${SHAR_SFO}"
  COMMENT "PARAM.SFO: The Simpsons Hit & Run" VERBATIM)
create_pbp_file(TARGET sharview TITLE "The Simpsons Hit & Run" SFO_PATH "${SHAR_SFO}" BUILD_PRX ${PBP_EXTRA})
# новая иконка/фон/живой значок/музыка (tools/icon0.py, tools/pic1.py, tools/icon1.py, tools/snd0.py) -> перелинковка -> EBOOT перепаковывается (иначе в нём осталась бы старая; tools/pbp_gate.py)
foreach(art pic1.png icon0.png icon1.pmf snd0.at3)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../build/assets/${art}")
    set_property(TARGET sharview APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/../build/assets/${art}")
  endif()
endforeach()
# правка 252 (03.10): служебная сборка «грязной памяти» — cmake -DMEM_SCRIBBLE=ON: новые и освобождённые выделения заливаются
# 0xFF (viewer/memwatch.c), как неинициализированная память настоящей PSP; прогон пути F10 в проверяльщике (tests/crash_f10/hw.sh
# с EBOOT этой сборки) 03.10 чистый. В выпуске выключено
option(MEM_SCRIBBLE "fill malloc/free blocks with 0xFF (diagnostic build)" OFF)
option(LEAK696_TRACE "per-caller live allocation table, dumped after level unload (diagnostic build, task 696)" OFF)
if(LEAK696_TRACE)
  target_compile_definitions(sharview PRIVATE LEAK696_TRACE=1)
  target_link_options(sharview PRIVATE "-Wl,--wrap=free")
endif()
if(MEM_SCRIBBLE)
  target_compile_definitions(sharview PRIVATE MEM_SCRIBBLE=1)
  target_link_options(sharview PRIVATE "-Wl,--wrap=free")
endif()
