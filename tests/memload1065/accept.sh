#!/bin/bash
# №1065 приёмка (окно Оптимизация, 28.10; журналы TEST74–77: TEST77 L5 без звука — банк sfx.sab не выделился; load.shr без проверки результата — чёрный кадр без газеты).
# Смена уровней подряд через golevel (levelAdvance, как LEVELUP в игре); на каждой загрузке: звук открыт (snd=1/N в LEVEL ev=load) и газета в кадре (SCRFILL после LEVELUP).
#   new      куча обычная (fragheap 3 перед каждой загрузкой), L1..L7: все ok, строк SCROPEN1065 / SNDRETRY1065 нет (цена ноль)
#   newsim1  имитация: open load.shr и банк звука отказывают один раз на каждой смене (flag scrfail1065 1, sndfail1065 1) -> место и повтор: все ok
#   newsim2  load.shr отказывает дважды -> предыдущий уровень выгружается до газеты и открытие повторяется: все ok
#   natnew   НАСТОЯЩАЯ тесная куча (flag boothstarve 0.8 МБ перед сменой, без имитации) -> газета открыта после выгрузки уровня: все ok, в журнале rc=-3
#   КРАСНЫЕ  на выключателе flag memload1065 0: redsim2 (те же отказы, прежний код) и natred (та же тесная куча) обязаны покраснеть (нет звука / нет газеты)
# Звук на настоящей тесной куче в PPSSPP не воспроизведён (в эмуляторе кучи вдвое больше, отказ на железе — лотерея раскладки): порядок загрузки (ui+sfx первыми) проверяется по LOADPH2, повтор — имитацией.
# Запуск: tests/memload1065/accept.sh [EBOOT] ; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_ml1065/EBOOT.PBP}; OUT=${OUT:-$W/build/ml1065}; export OUT_RUN=$OUT/run; mkdir -p $OUT; rc=0
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
PY="$W/.venv/bin/python3 -W ignore -I"
ALL="1 2 3 4 5 6 7"; NAT="1 2 3 4"
jobs=""
add() { n=$1; f=$2; shift 2; [ "${REUSE:-0}" = 1 ] && [ -f $OUT/run/out_$n/smoke.txt ] && return; python3 $H/gen.py $f $@ > $OUT/$n.txt; jobs="$jobs\n$n"; }
add new 3 new $ALL; add newsim1 3 newsim1 $ALL; add newsim2 3 newsim2 $ALL; add redsim2 3 redsim2 $ALL; add natnew 0 natnew $NAT; add natred 0 natred $NAT
export H OUT E; printf "$jobs" | grep . | xargs -P 3 -n 1 bash -c 'OUT=$OUT/run bash $H/run.sh "$0" "$OUT/$0.txt" "$E" >"$OUT/$0.log" 2>&1'
for m in new newsim1 newsim2 natnew; do echo "$m:"; o=$($PY $H/check.py $OUT/run/out_$m/smoke.txt new); r=$?; echo "$o" | cut -c1-170; [ $r = 0 ] || rc=1; done
for m in redsim2 natred; do echo "красный $m (flag memload1065 0):"; $PY $H/check.py $OUT/run/out_$m/smoke.txt red | tail -2 | grep -q "OK красный краснеет" && echo "  ok красный краснеет" || { echo "  FAIL красный не покраснел"; rc=1; }; done
q=$(grep -cE "^(SCROPEN1065|SNDRETRY1065)" $OUT/run/out_new/smoke.txt); [ "$q" = 0 ] && echo "ok обычная куча: ни одного повтора (цена ноль)" || { echo "FAIL обычная куча: строк повтора $q"; rc=1; }
o=$(awk '/^LEVELUP from=2 to=3/{f=1} f&&/^LOADPH2/{printf "%s ",$2} /^LEVEL ev=load n=3/{exit}' $OUT/run/out_new/smoke.txt); case "$o" in "ui+sfx player"*) echo "ok порядок загрузки: $o";; *) echo "FAIL порядок загрузки: $o"; rc=1;; esac
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
