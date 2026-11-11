#!/bin/bash
# Build 95 longgame (аудит Fable §7 п.4): tests/longgame/run.sh НАБОР [short|full] [nofrag] [ДОП. ФЛАГИ ДВИЖКА В НАБОРЕ УЖЕ СОБРАНЫ]
#   НАБОР — папка с EBOOT.PBP (сборка с SHAR_RELEASE_KEEP_SMOKE) и данными (уровни 2-7: SHIP_LEVELS); эмулятор один, безголовый (SMOKE_HEADLESS=1).
#   full: 15 минут игры L1 с заходами (3 двери, будка, пауза с MISSION SELECT, 2 смерти) + конец уровня как у игрока + 45 с L2, около 25 минут; short: один круг (около 6 минут).
#   Раздробление flag fragblk 0.6 (кусок <= 0,6 МБ, как у Роба на L4); nofrag — целая куча (зелёный контроль самой проверки).
#   Ожидание: на 94 при fragblk красный (MEMFAILSZ > 0: 16 за 15 минут, 12 из них одно место ra 088175f0 = комната Kwik-E-Mart 721 КБ), nofrag зелёный; на 95 (Z1/Z3) fragblk зелёный.
#   Красный контроль 95: тот же сценарий на сборке с -DZONE3_OLD=1 (когда появится) или -DNPCMEM_OLD=1 обязан дать MEMFAILSZ > 0 или npc!.  Последняя строка PASS/FAIL (код проверки).
set -u
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор}; MODE=${2:-short}; cd "$W"
export SHIP_LEVELS="${SHIP_LEVELS:-2 3 4 5 6 7}" SMOKE_HEADLESS=1; [ -x /Library/Developer/CommandLineTools/usr/bin/git ] && export DEVELOPER_DIR=/Library/Developer/CommandLineTools
[ -x .venv/bin/python ] && export PATH=$W/.venv/bin:$PATH
FR=0.6; EX=""; LVL=""; case " $* " in *" nofrag "*) FR=0; EX=--nofrag;; esac
for x in "$@"; do case "$x" in L[1-7]) LVL=$x;; esac; done   # L2|L5|L6: конец уровня с роликом (fmv3 / fmv5 / fmv6) -> следующий уровень
if [ "$MODE" = full ]; then ARGS="900 $FR $LVL"; MAXT=2400; elif [ "$MODE" = mid ]; then ARGS="300 $FR $LVL"; MAXT=1800; else ARGS="150 $FR short $LVL"; MAXT=1200; fi
[ -n "$LVL" ] && EX="$EX --$LVL"
OUT=${OUT:-$W/build/longgame}; mkdir -p "$OUT"; python3 -I "$H/gen_long.py" $ARGS > "$OUT/scenario.txt"
SMOKE_SCN="$OUT/scenario.txt" SMOKE_MAXT=$MAXT tools/smoke.sh "$SET" > "$OUT/smoke.out" 2>&1
D=$(grep -o "build/smoke/[0-9-]*" "$OUT/smoke.out" | head -1); [ -n "$D" ] || { echo "нет отчёта смоука: $OUT/smoke.out"; exit 2; }
echo "прогон: $D ($MODE $EX frag=$FR)"; python3 -I "$H/check_long.py" "$W/$D/smoke.txt" $EX
