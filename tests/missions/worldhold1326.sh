#!/bin/bash
# 1326: после ✕ карточки нет кадров, где интерфейс виден, а мир чёрный (доля тёмных точек в центре кадра > 0,9 при ярких точках радара/подписи). Чёрный слой на 4 с задаёт flag worldblack 4 (как на PSP).
# Новое: плохих 0 и строка WORLDHOLD1326 waited≈4000 мс. Красное (flag worldhold1326old 1): плохих ≥ 2, гейт обязан упасть.
#   tests/missions/worldhold1326.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS="5" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=120 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
N=$(run tests/missions/worldhold1326.scn); R=$(run tests/missions/worldhold1326_red.scn)
echo "новое ($N):"; python3 tests/missions/worldhold1326.py $N; nok=$?; grep -a '^WORLDHOLD1326' $N/smoke.txt | head -3
echo "красное ($R):"; python3 tests/missions/worldhold1326.py $R; rok=$?
[ $nok = 0 ] && [ $rok = 1 ] && { echo PASS; exit 0; }; [ $rok = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
