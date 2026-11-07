#!/bin/bash
# ME-доля (слово Роба 492, 10.10): две тестовые апки рядом с SHARVIEW, SHARVIEW и nullDC-PSP не трогает.
#   SHARTEST4 «SHAR TEST4 CPU»   — всё на главном процессоре, ME выключен; разрез PHSEC по подсистемам и ожидание видеочипа; счётчик audio_mix исправлен
#   SHARTEST5 «SHAR TEST5 PEAK»  — адаптивный помощник: по умолчанию всё на главном, ME включается в тяжёлые моменты (mepeak): прошлый кадр > 13 мс,
#                                   живых частиц > 100 или погоня; выключается через 1 с спокойствия. Звук и частицы на ME (meaudio=u, mefx=u),
#                                   частицы кусками mefxmax=48 элементов (около 0,4 мс на ME)
# Данные берутся из пакета TOUR_SRC (по умолчанию build/build3 рядом с этим деревом) с EBOOT из viewer/build; дорога — tests/me/psp_tour_me492.txt.
#   tools/tour_me492.sh --list      показать, что будет сделано
#   tools/tour_me492.sh --deploy    собрать пакет-источник и положить обе апки на PSP (PSP по USB, сверка cmp)
#   tools/tour_me492.sh --fetch     забрать отчёты обеих апок в build/psp-tour/
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
set -u
cd "$(dirname "$0")/.."
SRC="${TOUR_SRC_BASE:-build/build3}"; STG=build/tour-src-me492
case "${1:-}" in
  --list) echo "SHARTEST4: TOUR_SRC=$STG (копия $SRC + EBOOT viewer/build), без флаг-файлов ME"
          echo "SHARTEST5: те же данные, флаг-файлы meaudio=u mefx=u mefxmax=48 mepeak"; echo "дорога: tests/me/psp_tour_me492.txt" ;;
  --deploy)
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build/EBOOT.PBP "$STG/EBOOT.PBP" || exit 1
    export SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt"
    TOUR_DIR=SHARTEST4 TOUR_TITLE="SHAR TEST4 CPU" tools/tour_psp.sh --deploy --all-levels || exit 1
    : > /Volumes/PSP/PSP/GAME/SHARTEST4/meaudiooff   # звук на ME включён в коде по умолчанию (слово Роба 500): для базы «всё на главном» его выключаем
    TOUR_DIR=SHARTEST5 TOUR_TITLE="SHAR TEST5 PEAK" TOUR_MEAUDIO=u TOUR_MEFX=u tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST5; printf 48 > "$D/mefxmax"; : > "$D/mepeak"; sync
    [ -f "$D/mepeak" ] && [ -f "$D/meaudio" ] && [ -f "$D/mefx" ] || { echo "флаг-файлы SHARTEST5 не легли"; exit 1; }
    echo "готово: SHARTEST4 и SHARTEST5 на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy6)   # SHARTEST6 = конфигурация финала (звук на ME по умолчанию в коде, частицы на главном) + все таймеры фаз (PHSEC x_city.. и PHDRAW)
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build/EBOOT.PBP "$STG/EBOOT.PBP" || exit 1
    export SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt"
    TOUR_DIR=SHARTEST6 TOUR_TITLE="SHAR TEST6 FINAL" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST6; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax"; sync   # без ручных файлов: как у игрока
    echo "готово: SHARTEST6 на PSP (конфигурация финала + таймеры); SHARVIEW, SHARTEST4/5 и nullDC-PSP не тронуты" ;;
  --deploy7)   # SHARTEST7 = финал + PVS 250 м + мипы + jitter 40; SHARTEST7S = то же, но EBOOT с -DSHAR_OPT=-Os (viewer/build-os, опция Трасс debecc78)
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    [ -f viewer/build-os/EBOOT.PBP ] || { echo "нет viewer/build-os/EBOOT.PBP (psp-cmake -S viewer -B viewer/build-os -DSHAR_OPT=-Os)"; exit 1; }
    for L in 1 2 7; do [ -f build/t7-levels/l$L/level.shr ] || { echo "нет build/t7-levels/l$L/level.shr (пересборка уровней не закончена)"; exit 1; }; done
    cp viewer/build/EBOOT.PBP build/EBOOT_o2.PBP; trap 'cp build/EBOOT_o2.PBP viewer/build/EBOOT.PBP' EXIT   # tour_psp.sh берёт EBOOT из viewer/build: на время -Os подменяем и возвращаем
    dep7() {   # dep7 ПАПКА ЗАГОЛОВОК EBOOT
      rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp "$3" "$STG/EBOOT.PBP" && cp "$3" viewer/build/EBOOT.PBP || exit 1
      cp build/t7-levels/l1/level.shr "$STG/level.shr"; cp build/t7-levels/l2/level.shr "$STG/l2/level.shr"; cp build/t7-levels/l7/level.shr "$STG/l7/level.shr"
      for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"; [ -f build/t7-levels/l$L/level.shr.texsrc.json ] && cp build/t7-levels/l$L/level.shr.texsrc.json "$d/"; done
      SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR="$1" TOUR_TITLE="$2" tools/tour_psp.sh --deploy --all-levels || exit 1
      D=/Volumes/PSP/PSP/GAME/$1; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax"; printf 40 > "$D/jitter"; sync   # jitter 40 включён со старта (Трассы 70b1a5cf); удалить файл = выключено
    }
    dep7 SHARTEST7 "SHAR TEST7" build/EBOOT_o2.PBP
    dep7 SHARTEST7S "SHAR TEST7 OS" viewer/build-os/EBOOT.PBP
    echo "готово: SHARTEST7 («SHAR TEST7», -O2) и SHARTEST7S («SHAR TEST7 OS», -Os) на PSP; SHARVIEW, SHARTEST4/5/6 и nullDC-PSP не тронуты" ;;
  --deploy8)   # SHARTEST8 = финал + PVS 250 + мипы (уровни build/t7-levels) + джиттер полосами 64 px «40 80» (Трассы ecacfabc); один значок, -O2
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    DATA=build/t7-levels; if [ -f build/t8-data/l7/msncars-l7.shr ] && tail -1 build/t8-data/status.txt | grep -q ALLDONE; then DATA=build/t8-data; fi   # t8-data: уровни + traffic + msncars с щелями машин (№506); иначе уровни TEST7
    for L in 1 2 7; do [ -f $DATA/l$L/level.shr ] || { echo "нет $DATA/l$L/level.shr"; exit 1; }; done; echo "данные тура: $DATA"
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build/EBOOT.PBP "$STG/EBOOT.PBP" || exit 1
    for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"
      cp $DATA/l$L/level.shr "$d/level.shr"; [ -f $DATA/l$L/level.shr.texsrc.json ] && cp $DATA/l$L/level.shr.texsrc.json "$d/"
      [ -f $DATA/l$L/traffic.shr ] && cp $DATA/l$L/traffic.shr "$d/traffic.shr"; [ -f $DATA/l$L/msncars-l$L.shr ] && cp $DATA/l$L/msncars-l$L.shr "$d/msncars-l$L.shr"; done
    SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR=SHARTEST8 TOUR_TITLE="SHAR TEST8" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST8; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax"; printf "40 80" > "$D/jitter"; sync
    echo "готово: SHARTEST8 («SHAR TEST8») на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy9)   # SHARTEST9A = TEST8 с -DTEXREG_OLD=1 (бинды текстур как раньше); SHARTEST9B = + -DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 (джиттер как в TEST7: один спрайт, машины со сдвигом)
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }
    for e in build-9a build-9b; do [ -f viewer/$e/EBOOT.PBP ] || { echo "нет viewer/$e/EBOOT.PBP"; exit 1; }; done
    DATA=build/t7-levels; if [ -f build/t8-data/l7/msncars-l7.shr ] && tail -1 build/t8-data/status.txt | grep -q ALLDONE; then DATA=build/t8-data; fi
    for L in 1 2 7; do [ -f $DATA/l$L/level.shr ] || { echo "нет $DATA/l$L/level.shr"; exit 1; }; done; echo "данные тура: $DATA"
    cp viewer/build/EBOOT.PBP build/EBOOT_o2.PBP; trap 'cp build/EBOOT_o2.PBP viewer/build/EBOOT.PBP' EXIT
    dep9() {   # dep9 ПАПКА ЗАГОЛОВОК EBOOT JITTER
      rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp "$3" "$STG/EBOOT.PBP" && cp "$3" viewer/build/EBOOT.PBP || exit 1
      for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"
        cp $DATA/l$L/level.shr "$d/level.shr"; [ -f $DATA/l$L/level.shr.texsrc.json ] && cp $DATA/l$L/level.shr.texsrc.json "$d/"
        [ -f $DATA/l$L/traffic.shr ] && cp $DATA/l$L/traffic.shr "$d/traffic.shr"; [ -f $DATA/l$L/msncars-l$L.shr ] && cp $DATA/l$L/msncars-l$L.shr "$d/msncars-l$L.shr"; done
      SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR="$1" TOUR_TITLE="$2" tools/tour_psp.sh --deploy --all-levels || exit 1
      D=/Volumes/PSP/PSP/GAME/$1; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax"; printf "%s" "$4" > "$D/jitter"; sync
    }
    dep9 SHARTEST9A "SHAR TEST9A" viewer/build-9a/EBOOT.PBP "40 80"
    dep9 SHARTEST9B "SHAR TEST9B" viewer/build-9b/EBOOT.PBP "40"
    echo "готово: SHARTEST9A и SHARTEST9B на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --list-final|--deploy-final)   # слово Роба 10.10 вечер: SHARTEST10 (8 худших точек, tests/me/psp_tour_final_points.txt) и SHARTEST10B (b02/b05, psp_tour_final_bonus.txt) рядом с финальной SHARVIEW
    # Данные и EBOOT берутся из пакета tools/final-build.sh (FINAL_PKG или последний build/final-<время> без -release); EBOOT пакета должен быть тот же, что viewer/build/EBOOT.PBP.
    # В папки тура кладутся nojitter и smokelog (PHSUB near_why); clearchk/seamchk файлами НЕ кладутся (замер скорости чистый): счётчик щелей включается на лету хвостом сценария (сцены C_*). SHARVIEW и nullDC-PSP не трогаются. Гейт: tools/tour_final_gate.py build/psp-tour/<папка>.
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"
    [ -n "$FP" ] && [ -d "$FP" ] || { echo "нет пакета tools/final-build.sh (build/final-*)"; exit 1; }
    if [ "$1" = --list-final ]; then
      echo "пакет: $FP"; echo "SHARTEST10  «SHAR TEST10»  — tests/me/psp_tour_final_points.txt, уровни 1-7 (точки 3 4 5 6, хвост 1 2 7), флаги nojitter smokelog, хвост C_L1/C_L2/C_L7 со счётчиком щелей на лету"
      echo "SHARTEST10B «SHAR TEST10B» — tests/me/psp_tour_final_bonus.txt, бонус-трассы b02 b05, флаги nojitter smokelog"; echo "гейт: ноль пропусков, p95 <= 31 мс, MSN mainat совпал, near_why в PHSUB (tools/tour_final_gate.py)"
      SHIP_LEVELS="3 4 5 6" TOUR_SRC="$FP" bash tools/ship_files.sh --pairs | awk -F'|' -v p="$FP" '{n++} END{print "файлов в пакете тура: " n}'; exit 0; fi
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP: собери финал заново"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете $FP джиттер не выключен (нужен nojitter, без jitter)"; exit 1; }
    for T in "10|SHAR TEST10|psp_tour_final_points.txt|2 3 4 5 6 7|20" "10B|SHAR TEST10B|psp_tour_final_bonus.txt|2|22"; do
      IFS='|' read -r N TT SC LV MS <<< "$T"   # MS — длина музыки значка, с (слово Роба: предел SND0; SHARVIEW остаётся 16 с)
      [ -f "build/snd0-test/snd0_${MS}s.at3" ] || { echo "нет build/snd0-test/snd0_${MS}s.at3 (tools/snd0.py --track sunday_drive --start 0 --dur $MS --fade-out 2 --out ...)"; exit 1; }
      SHIP_LEVELS="$LV" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$PWD/build/snd0-test/snd0_${MS}s.at3" TOUR_SCENARIO="$PWD/tests/me/$SC" TOUR_DIR=SHARTEST$N TOUR_TITLE="$TT" tools/tour_psp.sh --deploy --all-levels || exit 1
      D=/Volumes/PSP/PSP/GAME/SHARTEST$N; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"
      : > "$D/nojitter"; : > "$D/smokelog"; sync
    done
    echo "готово: SHARTEST10 и SHARTEST10B на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy-11)   # слово Роба 10.10 ночь: SHARTEST11A (все приёмы) и SHARTEST11B (приёмы отрисовки выключены) для разбора зависания L3; данные из FINAL_PKG, меняется только EBOOT
    # 11A = viewer/build как есть; 11B = viewer/build-11b с -DSHAR_OPT="-DPROP_BAKE_OLD=1 -DCULL_REUSE_OLD=1 -DBOOM_CITY_OLD=1 -DNEARPASS_OLD=1". Сценарий tests/me/psp_tour_11.txt: L3 60 с простоя, 60 с езды, остальные точки.
    export PSPDEV="$PWD/pspdev" PATH="$PWD/pspdev/bin:$PATH"
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f viewer/build-11b/CMakeCache.txt ] || psp-cmake -S viewer -B viewer/build-11b -DSHAR_OPT="-DPROP_BAKE_OLD=1 -DCULL_REUSE_OLD=1 -DBOOM_CITY_OLD=1 -DNEARPASS_OLD=1" -DSHAR_JITTER_DEFAULT=OFF >/dev/null || exit 1
    cmake --build viewer/build -j3 >/dev/null || exit 1; cmake --build viewer/build-11b -j3 >/dev/null || exit 1
    cp viewer/build/EBOOT.PBP build/eboot-11A.pbp; cp viewer/build-11b/EBOOT.PBP build/eboot-11B.pbp
    cmp -s build/eboot-11A.pbp build/eboot-11B.pbp && { echo "11A и 11B одинаковы: флаги не сработали"; exit 1; }
    for X in A B; do
      STG11=build/tour-src-11$X; rm -rf "$STG11"; cp -cR "$FP" "$STG11" && cp build/eboot-11$X.pbp "$STG11/EBOOT.PBP" && cp build/eboot-11$X.pbp viewer/build/EBOOT.PBP || exit 1
      SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$STG11" TOUR_SCENARIO="$PWD/tests/me/psp_tour_11.txt" TOUR_DIR=SHARTEST11$X TOUR_TITLE="SHAR TEST11$X" tools/tour_psp.sh --deploy --all-levels || { cp build/eboot-11A.pbp viewer/build/EBOOT.PBP; exit 1; }
      D=/Volumes/PSP/PSP/GAME/SHARTEST11$X; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    done
    cp build/eboot-11A.pbp viewer/build/EBOOT.PBP; echo "готово: SHARTEST11A и SHARTEST11B на PSP; SHARVIEW, SHARTEST10/10B и nullDC-PSP не тронуты" ;;
  --deploy-12)   # 11.10 утро: SHARTEST12 = сценарий 11A с поправкой (брифинг: press cross), музыка значка 22 с и ICON1 игры; данные и EBOOT из пакета final-build
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете джиттер не выключен"; exit 1; }
    [ -f build/snd0-test/snd0_22s.at3 ] || { echo "нет build/snd0-test/snd0_22s.at3"; exit 1; }
    SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$PWD/build/snd0-test/snd0_22s.at3" TOUR_SCENARIO="$PWD/tests/me/psp_tour_12.txt" TOUR_DIR=SHARTEST12 TOUR_TITLE="SHAR TEST12" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST12; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    echo "готово: SHARTEST12 на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy-13)   # 11.10 утро: SHARTEST13 = старт миссии отдельно от езды, 5 худших мест дневных ботов, смена этапа L5 m6, хвост щелей, музыка значка 22 с и ICON1 игры; данные и EBOOT из пакета final-build
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете джиттер не выключен"; exit 1; }
    [ -f build/snd0-test/snd0_22s.at3 ] || { echo "нет build/snd0-test/snd0_22s.at3"; exit 1; }
    SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$PWD/build/snd0-test/snd0_22s.at3" TOUR_SCENARIO="$PWD/tests/me/psp_tour_13.txt" TOUR_DIR=SHARTEST13 TOUR_TITLE="SHAR TEST13" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST13; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    echo "готово: SHARTEST13 на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy-14)   # 11.10 утро: SHARTEST14 = старт миссии отдельно от езды, 5 худших мест дневных ботов, смена этапа L5 m6, хвост щелей, музыка значка 22 с и ICON1 игры; данные и EBOOT из пакета final-build
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете джиттер не выключен"; exit 1; }
    [ -f build/snd0-test/snd0_22s.at3 ] || { echo "нет build/snd0-test/snd0_22s.at3"; exit 1; }
    SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$PWD/build/snd0-test/snd0_22s.at3" TOUR_SCENARIO="$PWD/tests/me/psp_tour_14.txt" TOUR_DIR=SHARTEST14 TOUR_TITLE="SHAR TEST14" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST14; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    echo "готово: SHARTEST14 на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy-15)   # 12.10: SHARTEST15 = сценарий psp_tour_15 (TEST15: гейты №540, 545, 547/550, 546, 544, 23,4 мин); данные и EBOOT из пакета final-build; в ПАКЕТ ТУРА build/tour-pkg-15 (структура PSP/GAME/SHARTEST15), на PSP не кладёт
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете джиттер не выключен"; exit 1; }
    SND="${SND0_FILE:-$HOME/Downloads/shar-psp-sonnet-runs-w/build/snd0-test/snd0_22s.at3}"; [ -f "$SND" ] || { echo "нет $SND"; exit 1; }
    OUT="${TOUR_PKG_ROOT:-$PWD/build/tour-pkg-15}"; rm -rf "$OUT"; mkdir -p "$OUT/PSP/GAME"
    TOUR_PSP_ROOT="$OUT" SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$SND" TOUR_SCENARIO="$PWD/tests/me/psp_tour_15.txt" TOUR_DIR=SHARTEST15 TOUR_TITLE="SHAR TEST15" tools/tour_psp.sh --deploy --all-levels || exit 1
    D="$OUT/PSP/GAME/SHARTEST15"; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    echo "готово: пакет тура $OUT/PSP/GAME/SHARTEST15 (на PSP не клали)" ;;
  --deploy-16)   # 13.10: SHARTEST16 = сценарий psp_tour_16 (TEST16: починены серии b538/gb1, сцены света АЭС L1, улица L1, фонари L7); в ПАКЕТ ТУРА build/tour-pkg-16 (на PSP не кладёт)
    FP="${FINAL_PKG:-$(for d in build/final-2*; do [ -d "$d" ] && echo "$d"; done | grep -v -- '-release$' | sort | tail -1)}"; [ -d "$FP" ] || { echo "нет пакета final-build"; exit 1; }
    [ -f "$FP/EBOOT.PBP" ] && cmp -s "$FP/EBOOT.PBP" viewer/build/EBOOT.PBP || { echo "EBOOT пакета $FP не равен viewer/build/EBOOT.PBP"; exit 1; }
    [ -f "$FP/nojitter" ] && [ ! -f "$FP/jitter" ] || { echo "в пакете джиттер не выключен"; exit 1; }
    SND="${SND0_FILE:-$HOME/Downloads/shar-psp-sonnet-runs-w/build/snd0-test/snd0_22s.at3}"; [ -f "$SND" ] || { echo "нет $SND"; exit 1; }
    OUT="${TOUR_PKG_ROOT:-$PWD/build/tour-pkg-16}"; rm -rf "$OUT"; mkdir -p "$OUT/PSP/GAME"
    TOUR_PSP_ROOT="$OUT" SHIP_LEVELS="2 3 4 5 6 7" TOUR_SRC="$FP" TOUR_ICON1=1 TOUR_SND0="$SND" TOUR_SCENARIO="$PWD/tests/me/psp_tour_16.txt" TOUR_DIR=SHARTEST16 TOUR_TITLE="SHAR TEST16" tools/tour_psp.sh --deploy --all-levels || exit 1
    D="$OUT/PSP/GAME/SHARTEST16"; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/clearchk" "$D/seamchk"; : > "$D/nojitter"; : > "$D/smokelog"; sync
    echo "готово: пакет тура $OUT/PSP/GAME/SHARTEST16 (на PSP не клали)" ;;
  --deploy10)   # SHARTEST10 = build31 + TEXREG выключен + подмес мира до машин (Трассы) + типографика (ui/load/mini) + плавная загрузка #508; данные: build/t8-data (уровни, traffic, msncars с щелями) + build/t10-data (ui, load, mini)
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    DATA=build/t8-data; tail -1 $DATA/status.txt | grep -q ALLDONE || { echo "build/t8-data не досчитан"; exit 1; }
    for f in ui.shr l2/ui.shr l7/ui.shr load.shr bonus/mini.shr; do [ -f build/t10-data/$f ] || { echo "нет build/t10-data/$f"; exit 1; }; done
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build/EBOOT.PBP "$STG/EBOOT.PBP" || exit 1
    for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"
      cp $DATA/l$L/level.shr "$d/level.shr"; [ -f $DATA/l$L/level.shr.texsrc.json ] && cp $DATA/l$L/level.shr.texsrc.json "$d/"
      cp $DATA/l$L/traffic.shr "$d/traffic.shr"; cp $DATA/l$L/msncars-l$L.shr "$d/msncars-l$L.shr"; done
    cp build/t10-data/ui.shr "$STG/ui.shr"; cp build/t10-data/l2/ui.shr "$STG/l2/ui.shr"; cp build/t10-data/l7/ui.shr "$STG/l7/ui.shr"
    cp build/t10-data/load.shr "$STG/load.shr"; mkdir -p "$STG/bonus"; cp build/t10-data/bonus/mini.shr "$STG/bonus/mini.shr"
    SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR=SHARTEST10 TOUR_TITLE="SHAR TEST10" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST10; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax"; printf "40 80" > "$D/jitter"; sync
    echo "готово: SHARTEST10 на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --deploy9c)   # SHARTEST9C = EBOOT 9A (TEST8 с TEXREG_OLD=1), джиттер ВЫКЛ (файла jitter нет, подмеса нет): дальний план только на мипах
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build-9a/EBOOT.PBP ] || { echo "нет viewer/build-9a/EBOOT.PBP"; exit 1; }
    DATA=build/t7-levels; for L in 1 2 7; do [ -f $DATA/l$L/level.shr ] || { echo "нет $DATA/l$L/level.shr"; exit 1; }; done; echo "данные тура: $DATA (как у 9A)"
    rm -rf /Volumes/PSP/PSP/GAME/SHARTEST9A /Volumes/PSP/PSP/GAME/SHARTEST9B   # 9A/9B отыграли, отчёты сняты (слово дирижёра)
    cp viewer/build/EBOOT.PBP build/EBOOT_o2.PBP; trap 'cp build/EBOOT_o2.PBP viewer/build/EBOOT.PBP' EXIT
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build-9a/EBOOT.PBP "$STG/EBOOT.PBP" && cp viewer/build-9a/EBOOT.PBP viewer/build/EBOOT.PBP || exit 1
    for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"; cp $DATA/l$L/level.shr "$d/level.shr"; [ -f $DATA/l$L/level.shr.texsrc.json ] && cp $DATA/l$L/level.shr.texsrc.json "$d/"; done
    SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR=SHARTEST9C TOUR_TITLE="SHAR TEST9C" tools/tour_psp.sh --deploy --all-levels || exit 1
    D=/Volumes/PSP/PSP/GAME/SHARTEST9C; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter"; : > "$D/nojitter"; sync   # nojitter: явно выключено
    echo "готово: SHARTEST9C (9A без джиттера) на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --fetch9c) TOUR_DIR=SHARTEST9C tools/tour_psp.sh --fetch ;;
  --deploy9cd)   # 9C и 9D = один и тот же EBOOT (вершина build31: TEXREG выкл, исправленный джиттер Трасс 14e659af, №508 плавная загрузка, типографика, данные TEST10); 9C без джиттера (nojitter), 9D с файлом jitter «40 80»
    [ -d "$SRC" ] || { echo "нет $SRC"; exit 1; }; [ -f viewer/build/EBOOT.PBP ] || { echo "нет viewer/build/EBOOT.PBP"; exit 1; }
    DATA=build/t8-data; tail -1 $DATA/status.txt | grep -q ALLDONE || { echo "build/t8-data не досчитан"; exit 1; }
    for f in ui.shr l2/ui.shr l7/ui.shr load.shr bonus/mini.shr; do [ -f build/t10-data/$f ] || { echo "нет build/t10-data/$f"; exit 1; }; done
    rm -rf /Volumes/PSP/PSP/GAME/SHARTEST9A /Volumes/PSP/PSP/GAME/SHARTEST9B /Volumes/PSP/PSP/GAME/SHARTEST9C /Volumes/PSP/PSP/GAME/SHARTEST9D
    rm -rf "$STG"; cp -cR "$SRC" "$STG" && cp viewer/build/EBOOT.PBP "$STG/EBOOT.PBP" || exit 1
    for L in 1 2 7; do d="$STG"; [ $L != 1 ] && d="$STG/l$L"
      cp $DATA/l$L/level.shr "$d/level.shr"; [ -f $DATA/l$L/level.shr.texsrc.json ] && cp $DATA/l$L/level.shr.texsrc.json "$d/"
      cp $DATA/l$L/traffic.shr "$d/traffic.shr"; cp $DATA/l$L/msncars-l$L.shr "$d/msncars-l$L.shr"; done
    cp build/t10-data/ui.shr "$STG/ui.shr"; cp build/t10-data/l2/ui.shr "$STG/l2/ui.shr"; cp build/t10-data/l7/ui.shr "$STG/l7/ui.shr"
    cp build/t10-data/load.shr "$STG/load.shr"; mkdir -p "$STG/bonus"; cp build/t10-data/bonus/mini.shr "$STG/bonus/mini.shr"
    for v in C D; do
      SHIP_LEVELS="2 7" TOUR_SRC="$STG" TOUR_SCENARIO="$PWD/tests/me/psp_tour_me492.txt" TOUR_DIR=SHARTEST9$v TOUR_TITLE="SHAR TEST9$v" tools/tour_psp.sh --deploy --all-levels || exit 1
      D=/Volumes/PSP/PSP/GAME/SHARTEST9$v; rm -f "$D/meaudiooff" "$D/meaudio" "$D/mefx" "$D/mepeak" "$D/mefxmax" "$D/jitter" "$D/nojitter"
      if [ $v = C ]; then : > "$D/nojitter"; else printf "40 80" > "$D/jitter"; fi; sync
    done
    echo "готово: SHARTEST9C (джиттер выкл) и SHARTEST9D (джиттер «40 80») на PSP; SHARVIEW и nullDC-PSP не тронуты" ;;
  --fetch9cd) for d in SHARTEST9C SHARTEST9D; do TOUR_DIR=$d tools/tour_psp.sh --fetch; done ;;
  --fetch10) TOUR_DIR=SHARTEST10 tools/tour_psp.sh --fetch ;;
  --fetch9) for d in SHARTEST9A SHARTEST9B; do TOUR_DIR=$d tools/tour_psp.sh --fetch; done ;;
  --fetch8) TOUR_DIR=SHARTEST8 tools/tour_psp.sh --fetch ;;
  --fetch7) for d in SHARTEST7 SHARTEST7S; do TOUR_DIR=$d tools/tour_psp.sh --fetch; done ;;
  --fetch6) TOUR_DIR=SHARTEST6 tools/tour_psp.sh --fetch ;;
  --fetch) for d in SHARTEST4 SHARTEST5; do TOUR_DIR=$d tools/tour_psp.sh --fetch; done ;;
  *) echo "tools/tour_me492.sh --list | --deploy | --deploy6 | --deploy7 | --fetch | --fetch6 | --fetch7"; exit 2 ;;
esac
