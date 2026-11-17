#!/bin/bash
# 1332 (Роб, 108 на PSP: «не могу запрыгнуть на машины сверху: Mr. Plow, школьный автобус»; обе куплены: своя машина игрока).
# (1) Чужие машины (трафик, миссий) Гомер пускает на крышу со ступенькой HOMER_STEP (0,6 м) от стоп, своя (homer.c pushFromCar) выталкивала, пока стопы не выше крыши минус 0,15: крыши Mr. Plow (2,67) и автобуса
#     (2,84) не брались ни разу из 135 попыток стенда. Правка: та же ступенька. HOMER_OWNROOF1332_OLD=1 — как было.
# (2) «Этажи» верха: у высоких машин верх не плоский (в p3d оригинала несколько коллизионных объёмов: у школьного автобуса передний блок 3,39 м и задний 2,80 м, у Plow King кабина и кузов); у нас одна плоская крыша на верхе
#     всей модели (3,2–3,4 м у Plow King и автобуса: на пределе прыжка). tools/car_decks_export.py пишет cardecks.txt (данные набора), homer.c homerDecksParse читает, homerCarDecks даёт этажи Гомеру, трафику и машинам
#     миссий. HOMER_DECKS1332_OLD=1 — как было.
# Гейт без езды (хост, реальные homer.c / col.c / car.c / msncar.c, ровное место L2, стоящая машина, 135 вариантов подхода (у середины, к носу, к корме), времени и двойного прыжка):
#   (1) своя mrplo_v и otto_v на машине хотя бы в одном варианте, старое (оба выключателя) — ни в одном;
#   (2) своя plowk_v и школьный автобус трафика schoolbu: с этажами хоть раз, без этажей (только ступенька) ни разу (свои); у чужих (mcSolid) с этажами больше попыток, чем без;
#   седаны (нет в списке этажей) — как было, ходьба в борт без прыжка не поднимает на крышу.   tests/roof1332/run.sh [SET_DIR]   (PY — python с hit-and-run-web/tools, по умолчанию venv Дали)
set -u; cd "$(dirname "$0")/../.."
S=${1:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}; T=${TMPDIR:-/tmp}/roof1332.$$; PY=${PY:-$HOME/Downloads/shar-psp-dal103/.venv/bin/python}
cc -std=c99 -O2 -w -DSKIN_HOST -I viewer tests/roof1332/probe.c viewer/msncar.c viewer/traffic.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o $T || { echo "FAIL: не собралось"; exit 2; }
mkdir -p build; DK=${TMPDIR:-/tmp}/cardecks1332.$$.txt; $PY tools/car_decks_export.py $DK "$S" > /dev/null || { echo "FAIL: cardecks.txt не собран"; exit 2; }
A="$S/l2/level.shr $S/homer.shr $S/l2/msncars-l2.shr $S/traffic.shr"; bad=0
run() { env "$@" $T $A; }
NEW_O=$(run OWN=1 DECKS=$DK); STEP_O=$(run OWN=1 OLDD=1 DECKS=$DK); OLD_O=$(run OWN=1 OLD=1 OLDD=1 DECKS=$DK)
NEW_M=$(run DECKS=$DK); OLD_M=$(run OLDD=1 DECKS=$DK); rm -f $T $DK
n() { echo "$1" | grep -a -B1 "^$2 " | grep -a "на машине" | sed 's/.*на машине \([0-9]*\),.*/\1/'; }
for c in mrplo_v otto_v; do a=$(n "$NEW_O" $c); o=$(n "$OLD_O" $c); echo "(1) своя $c: новое $a, старое $o"; [ "$a" -ge 1 ] || { echo "FAIL зелёный $c"; bad=1; }; [ "$o" = 0 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН $c"; bad=1; }; done
for c in plowk_v schoolbu; do a=$(n "$NEW_O" $c); o=$(n "$STEP_O" $c); echo "(2) своя $c: с этажами $a, без этажей $o"; [ "$a" -ge 1 ] || { echo "FAIL зелёный этажи $c"; bad=1; }; [ "$o" = 0 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН этажи $c"; bad=1; }
  a=$(n "$NEW_M" $c); o=$(n "$OLD_M" $c); echo "(2) чужая $c (mcSolid): с этажами $a, без этажей $o"; [ "$a" -gt "$o" ] || { echo "FAIL чужая $c"; bad=1; }; done
for c in famil_v cSedan; do a=$(n "$NEW_O" $c); o=$(n "$STEP_O" $c); b=$(n "$NEW_M" $c); p=$(n "$OLD_M" $c); echo "$c (не сломать, с этажами/без): своя $a/$o, чужая $b/$p"; [ "$a" = "$o" ] && [ "$b" = "$p" ] || { echo "FAIL седан $c"; bad=1; }; done
echo "$NEW_O$NEW_M" | grep -a "ВЗОБРАЛСЯ" && { echo "FAIL: ходьба в борт подняла на крышу"; bad=1; }
[ $bad = 0 ] && echo "roof1332: PASS" || echo "roof1332: FAIL"; exit $bad
