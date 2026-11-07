#!/bin/bash
# Правка Роба 1205 (05.11: «в уличных гонках проверить соперничество — порядок соперников не менялся, никто не доехал, на L5 sr3 bart_v не едет»). Безголовый эмулятор, два дефекта игры, красный контроль у каждого:
#  seat — уличные гонки УРОВНЯ 5 (sr1–sr3): в данных уровня нет PutMFPlayerInCar (у уровней 1–4, 6, 7 есть), а у оригинала на загрузке миссии sr* зовётся сброс игрока при загрузке миссии — игрок садится в машину на
#         SetMissionResetPlayerInCar. У нас основная фаза гонки этот сброс пропускала (game_msn.c plReset: не startArmed): Гомер пешком там, где был до разговора, соперник sr3 (bart_v) дальше 300 м от него
#         стоит на ручнике (так же у оригинала, правило ожидания игрока у ИИ-гонщика). Красный: flag raceresetold 1 (RACE_RESET_OLD=1). Проверка: L5 sr3, MISSION SELECT с места старта уровня.
#  jam  — L7 sr2: cHears срезает поворот, встаёт в забор трассы и качается у его угла ±3 м больше 190 с (после сверки 02.11 правило «10 с без приближения — на свой путь» убрано). Красный: flag racejamold 1
#         (RACE_JAM_OLD=1). Проверка: игрок стоит на старте, cHears обязан набрать хотя бы одну точку пути за 110 с гонки.
# Один EBOOT (viewer/build_rc/EBOOT.PBP, с SHAR_RELEASE_KEEP_SMOKE) и набор данных (RACE_SET, по умолчанию набор тура TEST90); красный и зелёный прогоны каждой пары идут одновременно (два эмулятора).   tests/missions/race_rivals.sh
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
SRC="${RACE_SET:-$HOME/Downloads/shar-psp-integ/build/test90tour/PSP/GAME/SHARTEST15}"; EB="${EBOOT:-$ROOT/viewer/build_rc/EBOOT.PBP}"
OUT=build/race_rivals; mkdir -p "$OUT"; SET="$OUT/set"; rm -rf "$SET"; cp -c -R "$SRC" "$SET" 2>/dev/null || cp -R "$SRC" "$SET"; cp "$EB" "$SET/EBOOT.PBP" || exit 2
export SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_MAXT=500
scn() {   # имя уровень миссия флаг-красного гонка_секунд
  { echo "flag nosave"; echo "scene RIV_$1"; [ "$2" != 1 ] && { echo "flag golevel $2"; echo "wait 20"; } || echo "wait 3"; echo "flag player 1"; [ -n "$4" ] && echo "flag $4 1"
    echo "flag msn"; echo "wait 8"; echo "flag msnsel $3"; echo "wait 1.5"; echo "press cross 0.2"; echo "wait 5"; echo "press cross 0.2"; echo "wait $5"; echo "end"; } > "$OUT/$1.txt"
}
run() { SMOKE_SCN="$ROOT/$OUT/$1.txt" nice -n 10 tools/smoke.sh "$SET" > "$OUT/$1.log" 2>&1; sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$OUT/$1.log" > "$OUT/$1.dir"; }
pair() {   # проверка аргументы...: seat/jam
  kind=$1; shift; nm=$1; lv=$2; k=$3; flag=$4; secs=$5; shift 5
  scn "${nm}_red" $lv $k $flag $secs; scn "${nm}_green" $lv $k "" $secs
  run "${nm}_red" & sleep 2; run "${nm}_green" & wait
  for v in red green; do d=$(cat "$OUT/${nm}_$v.dir"); [ -n "$d" ] && [ -f "$d/smoke.txt" ] || { echo "FAIL: нет отчёта прогона ${nm}_$v"; tail -3 "$OUT/${nm}_$v.log"; exit 2; }
    python3 tests/missions/race_rivals.py $kind "$d/smoke.txt" "$@" > "$OUT/${nm}_$v.res"; rc=$?; echo "  [${nm} $v] $(cat "$OUT/${nm}_$v.res")  (журнал $d/smoke.txt)"; eval "${nm}_${v}_rc=$rc"; done
}
pair seat seat 5 10 raceresetold 50 147.7 9.2
pair jam jam 7 9 racejamold 110 cHears 1
ok=1; [ "${seat_red_rc:-0}" = 1 ] && [ "${seat_green_rc:-1}" = 0 ] && [ "${jam_red_rc:-0}" = 1 ] && [ "${jam_green_rc:-1}" = 0 ] && ok=0
[ $ok = 0 ] && { echo "PASS (красные упали, зелёные сошлись)"; exit 0; }
echo "FAIL (seat красный $seat_red_rc зелёный $seat_green_rc; jam красный $jam_red_rc зелёный $jam_green_rc; нужно красный 1, зелёный 0)"; exit 1
