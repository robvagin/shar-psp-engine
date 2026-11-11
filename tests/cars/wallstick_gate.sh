#!/bin/bash
# Гейт «скользящий удар не клинит и не заваливает» (Коллизия-96, тур 100: автопилот L2 застрял у стены (90; −210); allrisk на 80686fc3: крен до 70° на диагональной стене L3 C10-5).
# Повышенное трение стены после скольжения (a7c72281, CAR_SCRAPE_MU 0,6) разворачивало нос в стену: клин на 0,5–4 с при газе без руля (a), а с порогом скорости 6 м/с (cd49b631) — крен 67–71° на диагональной
# стене (b). Теперь скользящий удар гасит касательную по центру масс пропорционально прижатию (CAR_SCRAPE_MUX), без момента.
# РЕШЕНИЕ РОБА 10.11: физика как в 99 (CAR_SCRAPE_OLD=1 по умолчанию) — «новое» идёт без скольжения-правок, красные включают их через -DCAR_SCRAPE_OLD=0.
# (a) стена L2 z = −218 (x 71…106, нормаль +z), старт (80; −211) курсом 110/120/130° на 45 и 60 км/ч, газ, руль 0: почти стоя (< 5 км/ч) не больше 30 кадров (0,5 с). Красный: -DCAR_SCRAPE_MU=0.6f -DCAR_SCRAPE_DRAG=0.f (a7c72281: 34…239 кадров)
# (b) L3: старт (−438,1; 218,1) курсом 0°, держит 70 км/ч и курс (радиальный заезд allrisk n698, граница карты по диагонали у (−435; 266)): макс. крен не больше 35°. Красный: то же с -DCAR_SCRAPE_VMIN=6.f (cd49b631: 70°)
# Нужны каталоги данных уровней 2 и 3 (level.shr, player.shr). tests/cars/wallstick_gate.sh L2_DIR L3_DIR
set -u; ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; D2="${1:?каталог уровня 2}"; D3="${2:?каталог уровня 3}"; OUT="${OUT_DIR:-build/pk}"; mkdir -p "$OUT"
build() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $1 -I viewer -I tools/bots tools/bots/wallslide.c \$BOTS_LIB -lm -o $OUT/wallslide_gate" || exit 2; }
wedge() { build "$1"; bad=0; for kmh in 45 60; do for h in 110 120 130; do n=$(cd "$D2" && "$ROOT/$OUT/wallslide_gate" level.shr player.shr 80 -211 $h $kmh 0 1 300 </dev/null | sed -nE 's/.*\) ([0-9]+) кадров.*/\1/p')
    echo "  курс ${h}° ${kmh} км/ч: почти стоя ${n:-?} кадров (допуск ≤ 30)"; [ -z "$n" ] || [ "$n" -gt 30 ] && bad=1; done; done; return $bad; }
roll() { build "$1"; r=$(cd "$D3" && WS_Y=-65 WS_HOLD=70 "$ROOT/$OUT/wallslide_gate" level.shr player.shr -438.1 218.1 0 0 0 1 400 </dev/null | sed -nE 's/.*крен ([0-9]+)°.*/\1/p')
  echo "  L3 диагональная стена на 70 км/ч: макс. крен ${r:-?}° (допуск ≤ 35)"; [ -z "$r" ] || [ "$r" -gt 35 ]; }
echo "(a) клин, новое:"; wedge "" && echo "PASS" || { echo "FAIL"; exit 1; }
echo "(a) красный (a7c72281), обязан упасть:"; wedge "-DCAR_SCRAPE_OLD=0 -DCAR_SCRAPE_MU=0.6f -DCAR_SCRAPE_MUX=0.f" && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || echo "упал, как должен"
echo "(b) крен, новое:"; roll "" && { echo "FAIL"; exit 1; } || echo "PASS"
echo "(b) красный (cd49b631: порог 6 м/с), обязан упасть:"; roll "-DCAR_SCRAPE_OLD=0 -DCAR_SCRAPE_MU=0.6f -DCAR_SCRAPE_MUX=0.f -DCAR_SCRAPE_VMIN=6.f" && echo "упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; }
