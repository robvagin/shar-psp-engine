#!/bin/bash
# №1111: языки на CPU Мака. Покрытие знаков (coverage.py), ни одной надписи поверх соседней на F, G, S (overlap.py), красный контроль: с LANGFIT1111_OLD=1 пересечения обязаны появиться.
# Ширины относительно собственной рамки (fit.py) — отчёт: остаток многострочных сообщений без пересечений. tests/lang1111/run.sh   (около 5 минут)
set -u; cd "$(dirname "$0")/../.."; PY=${PY:-python3}; bad=0
$PY tests/lang1111/coverage.py | tail -1 | grep -q PASS && echo "покрытие знаков: PASS" || { echo "покрытие знаков: FAIL"; bad=1; }
for L in F G S; do
  LANG_BIBLE=$L $PY tests/lang1111/overlap.py | tail -1 | grep -q "новых против английского 0" && echo "пересечения $L: 0" || { echo "пересечения $L: ЕСТЬ"; bad=1; }
done
LANGFIT1111_OLD=1 LANG_BIBLE=F $PY tests/lang1111/overlap.py | tail -1 | grep -q "новых против английского 0" && { echo "КРАСНЫЙ НЕ ПОЙМАН: без подгонки пересечений нет"; bad=1; } || echo "красный (подгонка выключена): пересечения появились, как надо"
[ $bad = 0 ] && echo PASS || { echo FAIL; exit 1; }
