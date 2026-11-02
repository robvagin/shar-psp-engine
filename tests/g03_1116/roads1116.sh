#!/bin/bash
# Правка Роба №1116 (g03-aicars): соперники бонус-гонки едут по дорогам трассы (Road/RoadSegment/Intersection файла b0N.p3d) от точки WPR к следующей.
# Зелёный: (1) у каждой из 7 трасс в .bns есть блок дорог и его принимает загрузчик трафика; (2) гонка 4 машин (путь по дорогам, без подборов и ловушек):
#          b01 по ходу и обратно, b02 по ходу и обратно, b05 по ходу, b06 по ходу, b07 по ходу — все доехали, возвратов на путь ≤ 3 на круг.
# Красные (каждый обязан упасть на своей проверке):
#   --oldroads              прежний путь порта по опоре (BONUS_ROADS_OLD)               -> b01 обратно, b02, b06, b07
#   -DMC_SPRINT_GOALY_OLD=1 цель-точка с отсевом по высоте 3,5 м (WPL02/05/06 b02 лягут на ярус моста) -> b02 обратно, b05 по ходу
#   -DMC_SPRINT_CURVE_OLD=1 без ограничения скорости по кривизне пути (зигзаги и шпильки)    -> b04 по ходу
cd "$(dirname "$0")/../.."
B=build/bonus; R=$B/bin
for f in b01 b02 b03 b04 b05 b06 b07; do [ -f $B/$f.shr ] && [ -f $B/$f.bns ] || { echo "нет $B/$f.shr/.bns: python tools/bonus/export.py --all"; exit 1; }; done
tools/bonus/build_host.sh || exit 1
EXTRA="-DMC_SPRINT_GOALY_OLD=1" tools/bonus/build_host.sh $R/rh1116_goaly || exit 1
EXTRA="-DMC_SPRINT_CURVE_OLD=1" tools/bonus/build_host.sh $R/rh1116_curve || exit 1
fail=0
run() { local bin=$1 t=$2; shift 2; $bin $B/b0$t.shr $B/b0$t.bns $B/msncars-b.shr "$@" --noeffects --noncollide --ai4 2>&1; }   # №1147: без ударов между соперниками (удары — tests/g03_1147)
pass() { echo "$1" | grep -q "^RESULT: PASS"; }
echo "== (1) блок дорог в .bns"
for t in 1 2 3 4 5 6 7; do out=$(run $R/race_host $t --info); echo "$out" | head -1; pass "$out" || { echo "  b0$t: блок дорог не принят"; fail=1; }; done
echo "== (2) зелёный: гонка по дорогам"
for tr in "1" "1 --reverse" "2" "2 --reverse" "5" "6" "7"; do set -- $tr; t=$1; shift
  out=$(run $R/race_host $t "$@"); if pass "$out"; then echo "  b0$t ${*:-forward}: PASS"; else echo "  b0$t ${*:-forward}: $(echo "$out" | grep RESULT | cut -c1-110)"; fail=1; fi; done
red() {   # имя, бинарь, ключ, список «трасса [ключи]» — хотя бы одна обязана упасть
  local name=$1 bin=$2 key=$3; shift 3; local caught=0
  for tr in "$@"; do set -- $tr; t=$1; shift; out=$(run $bin $t "$@" $key); pass "$out" || caught=1; done
  [ $caught = 1 ] && echo "красный $name: пойман" || { echo "красный $name: НЕ пойман"; fail=1; }
}
echo "== красные"
red --oldroads $R/race_host --oldroads "1 --reverse" "2" "6" "7"
red goaly $R/rh1116_goaly "" "2 --reverse" "5"
red curve $R/rh1116_curve "" "4"
[ $fail = 0 ] && echo "ГЕЙТ tests/g03_1116/roads1116: PASS" || echo "ГЕЙТ tests/g03_1116/roads1116: FAIL"
exit $fail
