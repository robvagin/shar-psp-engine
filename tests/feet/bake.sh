#!/bin/bash
# Модели-96: гейт «поправка стоп запечена в данные» (viewer/skin.c skinFootLift читает таблицу модели, tools/footbake.c её пишет при экспорте).
#   tests/feet/bake.sh [DATA]   DATA — набор с моделями (корень, chars/, l{N}/); без аргумента модели собираются заново (tests/feet/gen_data.py, ~1,5 мин)
# Проверяет: (1) у каждой модели и клипа есть число, skinFootLift отдаёт его и оно равно счёту на месте, вершины не обходятся; (2) посадка стоп прежним гейтом (run_all.py): никто не ниже 1 см под землёй и не выше 2 см;
# (3) красный контроль: те же модели без запекания (FOOTBAKE_OFF=1) — гейт (1) обязан упасть, и показывает цену первого вызова на клип.
set -e
cd "$(dirname "$0")/../.."
PY=${PYTHON:-.venv/bin/python}; T=${TMPDIR:-/tmp}/feetbake.$$; mkdir -p "$T"
D=${1:-$T/data}
[ -n "$1" ] || $PY tests/feet/gen_data.py "$D"
run() { local out rc=0; out=$("$@") || rc=$?; echo "$out" | tail -3; return $rc; }
echo "== (1) запечённое число"; run $PY tests/feet/run_all.py "$D" --bake
echo "== (2) посадка стоп"; run $PY tests/feet/run_all.py "$D"
echo "== (3) красный контроль: без запекания (L4)"; FOOTBAKE_OFF=1 $PY tests/feet/gen_data.py "$T/red" --levels 4 | tail -1
if run $PY tests/feet/run_all.py "$T/red" --bake; then echo "ОШИБКА: красный контроль не упал"; exit 1; else echo "красный контроль упал, как и должен"; fi
echo "ИТОГ: PASS"
[ -n "$KEEP" ] || rm -rf "$T"
