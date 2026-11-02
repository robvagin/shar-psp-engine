#!/bin/bash
# Гейт бонус-игры на Маке (правка 420): правила гонки и соперники ИИ на настоящих трассах (tools/bonus/race_host.c).
# Зелёный:  (1) на каждой из 7 трасс в оба хода хотя бы одна машина проходит все круги (финиш + чекпойнты по порядку данных);
#           (2) на b01 (оба хода), b02, b06, b07 по ходу — все 4 доезжают, возвратов на путь ≤ 3 на круг, круг ≥ 5 с.
# Красные (каждый обязан уронить свою проверку):
#   --break-cp     объёмы чекпойнтов подняты на 1 км — круги не засчитываются          -> (1)
#   --nopath       водитель едет прямо к остановкам (без пути по опоре трассы)          -> (2) на b01
#   --nogoal       водитель считает точки сам (пропуск чекпойнта = лишний круг)          -> (2) хотя бы на одной трассе
#   --oldroads     №1116: соперники по плотному пути порта, а не по дорогам трассы (BONUS_ROADS_OLD)  -> (2)
# Журнал: build/gates/bonus/race_<время>.txt
cd "$(dirname "$0")/../.."
B=build/bonus; mkdir -p build/gates/bonus; LOG=build/gates/bonus/race_$(date +%Y%m%d-%H%M%S).txt
tools/bonus/build_host.sh || exit 1
for f in b01 b02 b03 b04 b05 b06 b07; do [ -f $B/$f.shr ] && [ -f $B/$f.bns ] || { echo "нет $B/$f.shr/.bns: python tools/bonus/export.py --all"; exit 1; }; done
# 09.10 (часть «подборы, ловушки, огонь, DEATH»): этот гейт меряет водителя ИИ и правила гонки — без подборов, DEATH и ловушек
# (--noeffects): пороги (195 с на 3 круга, ≤ 3 возвратов на круг) подобраны до эффектов, нитро и DEATH трасс меняют ход гонки
# (b05 по ходу: с эффектами первый финиш 197 с). Эффекты — tests/bonus/pickups.sh; RACE_FX=1 — этот же гейт с эффектами (разбор)
FX="--noeffects --noncollide --ai4"; [ "${RACE_FX:-0}" = 1 ] && FX="--noncollide --ai4"   # №1147: этот гейт меряет водителя и правила гонки без ударов между соперниками (как до правки); удары и пересечения — tests/g03_1147
run() { $B/bin/race_host $B/b0$1.shr $B/b0$1.bns $B/msncars-b.shr "${@:2}" $FX; }
check1() {   # каждая трасса и ход: хоть одна машина доехала
  local bad=0
  for t in 1 2 3 4 5 6 7; do for r in "" --reverse; do
    out=$(run $t $r "$@"); echo "$out" >> "$LOG"
    echo "$out" | grep -q "доехал место" || { echo "  (1) b0$t ${r:-forward}: никто не доехал"; bad=1; }
  done; done; return $bad
}
check2() {
  local bad=0
  for tr in "1" "1 --reverse" "2" "6" "7"; do
    out=$(run $tr "$@"); echo "$out" >> "$LOG"
    echo "$out" | grep -q "^RESULT: PASS" || { echo "  (2) b0$tr: $(echo "$out" | grep RESULT | cut -c1-120)"; bad=1; }
  done; return $bad
}
fail=0
echo "== красные" | tee -a "$LOG"
check1 --break-cp > /dev/null && { echo "красный --break-cp: НЕ пойман"; fail=1; } || echo "красный --break-cp: пойман (1)"
check2 --nopath > /dev/null && { echo "красный --nopath: НЕ пойман"; fail=1; } || echo "красный --nopath: пойман (2)"
check2 --nogoal > /dev/null && { echo "красный --nogoal: НЕ пойман"; fail=1; } || echo "красный --nogoal: пойман (2)"
check2 --oldroads > /dev/null && { echo "красный --oldroads: НЕ пойман"; fail=1; } || echo "красный --oldroads: пойман (2)"
echo "== зелёный" | tee -a "$LOG"
check1 || fail=1
check2 || fail=1
echo "журнал: $LOG"
[ $fail = 0 ] && echo "ГЕЙТ tests/bonus/race: PASS" || echo "ГЕЙТ tests/bonus/race: FAIL"
exit $fail
