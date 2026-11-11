#!/bin/bash
# Гейт правки 1247: старт миссии каждого уровня по одному (эмулятор один, nice 20, ballast 5), с играющим наездом.
#   tests/bubble1247/run.sh EBOOT [уровни] [этап] [старый:0|1]   → build/bub1247/<имя>/smoke.txt копируется в build/bub1247/<имя>.txt
# Лист кадров: SHOTN=24 SKIP=128 (снимок каждого кадра после подъёма пузыря, bs_<кадр>.png рядом с журналом).
# AVD=1.0 — значок задания приходит через 1 с после запроса (медленная карта PSP).
# №1262: 4-й аргумент 3 — «flag bubble1262old 1» (без выезда: гейт check.py обязан упасть по сдвигу).
# Красный контроль — 4-й аргумент 1: «flag bubble1247old 1» (прежнее поведение).
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; LV=${2:-1 2 3 4 5 6 7}; K=${3:-0}; OLD=${4:-0}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/bub1247
for L in $LV; do
  N=bub1251_${OLD}_L${L}_k$K; F=build/bub1247/$N.scn
  { echo "scene SW_${L}_$K"; echo "flag camdet 1"; echo "flag camlog"; echo "flag startcam 1"; [ "$L" != 1 ] && echo "flag golevel $L"
    echo "flag bubtrace"; [ "${AVD:-0}" != 0 ] && echo "flag avdelay $AVD"; [ "$OLD" = 1 ] && echo "flag bubble1247old 1"; [ "$OLD" = 2 ] && echo "flag bubble1251old 1"; [ "$OLD" = 3 ] && echo "flag bubble1262old 1"; [ "${SHOTN:-0}" -gt 0 ] && { echo "flag bubskip ${SKIP:-0}"; echo "flag bubshots $SHOTN"; }
    echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnsel $K"; echo "wait 2"; echo "shot a"; echo "wait 2"; echo "shot b"; echo "wait 4"; echo "end"; } > $F
  # данные выпуска: при RELDATA=папка (например t97/data) подменяются и корневые, и УРОВНЕВЫЕ пакеты интерфейса (движок на уровне N открывает lN/ui.shr, не корневой)
  if [ -n "${RELDATA:-}" ]; then OV="ui.shr=$RELDATA/ui.shr,ui.shr.fonts=$RELDATA/ui.shr.fonts,pmenu.shr=$RELDATA/pmenu.shr,pmenu.shr.fonts=$RELDATA/pmenu.shr.fonts"; [ -f "$RELDATA/l$L/ui.shr" ] && OV="$OV,l$L/ui.shr=$RELDATA/l$L/ui.shr"; [ -f "$RELDATA/l$L/pmenu.shr" ] && OV="$OV,l$L/pmenu.shr=$RELDATA/l$L/pmenu.shr,l$L/pmenu.shr.fonts=$RELDATA/l$L/pmenu.shr.fonts"; export SCEN_OVERRIDE="$OV"; fi
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/bub1247/$N.log 2>&1
  cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/bub1247/$N.txt 2>/dev/null || echo "L$L: нет smoke.txt"
done
echo ALLDONE
