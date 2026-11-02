#!/bin/bash
# Правка Роба №1147 (g03-aicars): соперники бонус-гонки сталкиваются друг с другом (та же модель удара, что игрок-соперник) и ездят по трассе без застреваний.
# Бот: 4 машины × 7 трасс × оба хода, 3 круга, подборы / ловушки / DEATH включены (полигон tools/bonus/race_host, --ai4: четвёртая машина — стенд вместо игрока — на ходовых соперника).
# Зелёный: на каждом из 14 заездов все доехали, пересечений кузовов (перекрытие в плане глубже 0,75 м, не на крыше другой) 0, возвратов на путь ≤ 3 на круг (RESULT: PASS полигона).
# Красные (каждый обязан упасть на своей проверке):
#   -DAIAI1147_OLD=1        соперники сквозь друг друга: пересечений кузовов > 0 хотя бы на одной трассе
#   --noncollide            то же ключом полигона
#   -DMC_SPRINT_LEDGE_OLD=1 дорога с «уступом» +2000 к цене (b07 по ходу: машины кружат в обход трассы)
#   -DMC_SPRINT_JUMP_OLD=1  на трамплине не полный газ (b03 по ходу: падения)
#   -DMC_SHORTCUT_GOAL_OLD=1 срез перескакивает дорогу цели (b03 по ходу, b07 обратно)
# Заодно печатает возвраты и падения (DEATH) по заездам: справка, в проверку не входят.
cd "$(dirname "$0")/../.."
B=build/bonus; R=$B/bin
for f in b01 b02 b03 b04 b05 b06 b07; do [ -f $B/$f.shr ] && [ -f $B/$f.bns ] || { echo "нет $B/$f.shr/.bns: python tools/bonus/export.py --all"; exit 1; }; done
tools/bonus/build_host.sh || exit 1
for v in AIAI1147_OLD MC_SPRINT_LEDGE_OLD MC_SPRINT_JUMP_OLD MC_SHORTCUT_GOAL_OLD; do EXTRA="-D${v}=1" tools/bonus/build_host.sh $R/rh1147_$v || exit 1; done
fail=0
run() { local bin=$1 t=$2; shift 2; $bin $B/b0$t.shr $B/b0$t.bns $B/msncars-b.shr "$@" --ai4 2>&1; }
pass() { echo "$1" | grep -q "^RESULT: PASS"; }
ovl() { echo "$1" | grep -o 'КУЗОВОВ [0-9]*' | awk '{print $2+0}'; }
echo "== зелёный: 4 машины × 7 трасс × 2 хода"
for t in 1 2 3 4 5 6 7; do for r in "" --reverse; do
  out=$(run $R/race_host $t $r)
  rs=$(echo "$out" | grep -o 'возвраты [0-9]*' | awk '{s+=$2} END{print s+0}'); ds=$(echo "$out" | grep -o 'гибели [0-9]*' | awk '{s+=$2} END{print s+0}'); n=$(ovl "$out")
  if pass "$out" && [ "$n" = 0 ]; then echo "  b0$t ${r:-forward}: PASS (пересечений $n, возвратов $rs, падений $ds)"; else echo "  b0$t ${r:-forward}: $(echo "$out" | grep RESULT | cut -c1-110); пересечений $n"; fail=1; fi
done; done
echo "== красные"
red_ovl() { local name=$1 bin=$2 key=$3 caught=0; for t in 1 2 3 4 5 6 7; do out=$(run $bin $t $key); [ "$(ovl "$out")" -gt 0 ] && caught=1; done; [ $caught = 1 ] && echo "красный $name: пойман (пересечения)" || { echo "красный $name: НЕ пойман"; fail=1; }; }
red_ovl AIAI1147_OLD $R/rh1147_AIAI1147_OLD ""
red_ovl --noncollide $R/race_host --noncollide
red_fail() { local name=$1 bin=$2 caught=0; shift 2; for tr in "$@"; do set -- $tr; t=$1; shift; out=$(run $bin $t "$@"); pass "$out" || caught=1; done; [ $caught = 1 ] && echo "красный $name: пойман" || { echo "красный $name: НЕ пойман"; fail=1; }; }
red_fail LEDGE $R/rh1147_MC_SPRINT_LEDGE_OLD "7"
red_fail JUMP $R/rh1147_MC_SPRINT_JUMP_OLD "3"
red_fail SHORTCUT_GOAL $R/rh1147_MC_SHORTCUT_GOAL_OLD "3" "7 --reverse"
[ $fail = 0 ] && echo "ГЕЙТ tests/g03_1147/ai1147: PASS" || echo "ГЕЙТ tests/g03_1147/ai1147: FAIL"
exit $fail
