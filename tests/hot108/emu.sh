#!/bin/bash
# 108 О6/О7/О9/О11/О13: пара прогонов в безголовом эмуляторе (scen.py) на настоящем уровне 1, один сценарий, два режима:
#   new — все правки включены; old — «flag prophot108old 1», «pbver108old 1», «skiddense108old 1», «spidle108old 1» (прежние обходы). Машина по очереди заезжает в 5 предметов разных типов (удары, падение, засыпание),
#   жмёт тормоз (следы шин), в каждой точке снимок и строка PROPSTAT предмета. Сверки включены: «flag prophotchk 1» (горячий массив против PropState каждый кадр) и «flag pbchk 1» (строка PBCHK: ver_skip, ver_bad, hot_bad).
# Зелёный: снимки new == old побайтно (программный рендер), строки PROPSTAT равны, hot_bad 0, ver_bad 0, dyn_bad 0, vis_bad 0, ver_skip > 0. Красный (обязан упасть): «flag prophotred 1» — hot_bad > 0 или снимки/PROPSTAT другие.
#   tests/hot108/emu.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/hot108; mkdir -p $OUT
mk() { # имя, флаги в начале
  local N=$1; shift; local F=$OUT/$N.scn
  { echo "scene skid"; for f in "$@"; do echo "flag $f"; done; echo "flag prophotchk 1"; echo "flag pbchk 1"
    local k=0; while read -r x z a i; do k=$((k+1)); echo "goto $x $z $a"; echo "wait 2.5"; echo "press cross 2.6"; echo "wait 1.0"; echo "press circle 0.8"; echo "wait 0.6"; echo "flag propstat $i"; echo "flag skidlog"; echo "shot hot_$k"; echo "wait 0.5"; done < <(python3 tests/hot108/pick.py "$ROOT/build/level.shr" 5)
    echo "wait 3"; echo "shot hot_end"; echo "wait 1.2"; echo "end"; } > $F
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > $OUT/$N.log 2>&1
  mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/smoke.txt 2>/dev/null; cp -f $ROOT/build/twins/scen/full/$N/*.png $OUT/$N/ 2>/dev/null; echo "DONE $N"
}
# у всех прогонов одинаковое число строк «flag» (иначе сдвигается шкала кадров и снимки различаются дизерингом): prophotred, prophot108old, pbver108old, skiddense108old, spidle108old
FN="prophotred 0 prophot108old 0 pbver108old 0 skiddense108old 0 spidle108old 0"
case "${2:-all}" in
  new) mk hot108_new prophotred\ 0 prophot108old\ 0 pbver108old\ 0 skiddense108old\ 0 spidle108old\ 0;;
  new2) mk hot108_new2 prophotred\ 0 prophot108old\ 0 pbver108old\ 0 skiddense108old\ 0 spidle108old\ 0;;
  old) mk hot108_old prophotred\ 0 prophot108old\ 1 pbver108old\ 1 skiddense108old\ 1 spidle108old\ 1;;
  red) mk hot108_red prophotred\ 1 prophot108old\ 0 pbver108old\ 0 skiddense108old\ 0 spidle108old\ 0;;
  all) "$0" "$EB" new; "$0" "$EB" old; "$0" "$EB" red;;
esac
