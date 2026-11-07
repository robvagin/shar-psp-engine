#!/bin/bash
# №1178: таблицы вывесок всех семи уровней: tests/sign1178/all_levels.sh НАБОР [ВЫХОД]   (НАБОР = папка с level.shr и l2..l7/level.shr; ВЫХОД по умолчанию build/g13_1178/data)
# Проверка: для каждого уровня таблица собирается (все подписи найдены), ntex в заголовке равен числу текстур файла, записей не меньше списка; красный контроль: испорченный список отвергается.
set -u; ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; S="${1:?набор}"; O="${2:-$ROOT/build/g13_1178/data}"; fail=0
for n in 1 2 3 4 5 6 7; do
  if [ $n = 1 ]; then lv="$S/level.shr"; out="$O/sign1178.bin"; else lv="$S/l$n/level.shr"; out="$O/l$n/sign1178.bin"; fi
  mkdir -p "$(dirname "$out")"
  python3 -I "$ROOT/tools/far1177/signs.py" "$lv" "$out" --list "$ROOT/tools/far1177/signs_L$n.json" > /tmp/sign1178.$$ 2>&1 && echo "  L$n ok: $(cat /tmp/sign1178.$$)" || { echo "  L$n FAIL: $(cat /tmp/sign1178.$$)"; fail=1; }
done; rm -f /tmp/sign1178.$$
python3 -I - "$ROOT/tools/far1177/signs_L3.json" /tmp/bad3.$$.json <<'P'
import json,sys; d=json.load(open(sys.argv[1])); d['signs'][0]['sig']='0'*16; json.dump(d,open(sys.argv[2],'w'))
P
if python3 -I "$ROOT/tools/far1177/signs.py" "$S/l3/level.shr" /tmp/bad3.$$.bin --list /tmp/bad3.$$.json >/dev/null 2>&1; then echo "  FAIL: красный контроль не сработал"; fail=1; else echo "  ok: красный контроль (испорченный список L3 отвергнут)"; fi
rm -f /tmp/bad3.$$.json /tmp/bad3.$$.bin; [ $fail = 0 ] && echo "sign1178 все уровни: PASS" || { echo "sign1178 все уровни: FAIL"; exit 1; }
