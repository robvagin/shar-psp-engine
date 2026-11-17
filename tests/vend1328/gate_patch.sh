#!/bin/bash
# Гейт №1328: патч таблиц состояний стейтпропов поверх готовых level.shr набора (tools/vend1328_patch.py).
#   [ORIG=папка с исходными l<N>/level.shr, если НАБОР уже с патчем]  tests/vend1328/gate_patch.sh НАБОР_108 EBOOT.PBP ПАПКА_ПАТЧЕННЫХ [img]      ПАПКА_ПАТЧЕННЫХ/l<N>/level.shr для N = 2 3 4 5 7
# 1) сверка (--verify): начало таблиц файла равно таблицам экспортёра по-старому, значит подмена безопасна;
# 2) отличие от файла набора только внутри SPR1 (таблицы состояний), плюс при росте — заголовок/клеточная таблица/REGF (печатаются диапазоны), прочих отличий 0; L3, L4, L5, L7 — тот же размер;
# 3) подпись notoon (ntex, FNV по таблице текстур) та же, что у файла набора, notoon.txt набора подходит;
# 4) красный: VEND1328_OLD=1 — патч не выполняется (копия), и gate_family на копиях краснеет; зелёный: gate_family на патченных (данные L1–L7 [+ картинка]).
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; PD="$3"; MODE="${4:-data}"; PY=.venv/bin/python; BAD=0; RED=build/vend1328/patched_red; mkdir -p "$RED"
for n in 2 3 4 5 7; do
  f="${ORIG:-$BASE}/l$n/level.shr"; g="$PD/l$n/level.shr"
  echo "== L$n"; $PY tools/vend1328_patch.py $n "$f" --verify | sed 's/^/  /' | tail -3; [ ${PIPESTATUS[0]} = 0 ] || { echo "  FAIL verify"; BAD=1; }
  $PY tools/vend1328_patch.py --diff "$f" "$g" | sed 's/^/  /'; [ ${PIPESTATUS[0]} = 0 ] || { echo "  FAIL: отличия вне SPR1"; BAD=1; }
  a=$(stat -f %z "$f"); b=$(stat -f %z "$g"); echo "  размер $a -> $b"; { [ $n = 2 ] || [ $a = $b ]; } || { echo "  FAIL: размер изменился"; BAD=1; }
  $PY - "$f" "$g" "$BASE/l$n/notoon.txt" <<'PY' || { echo "  FAIL: подпись notoon"; BAD=1; }
import sys, struct; sys.path.insert(0, 'tools')
from notoon_check import shr_sig
a, b = shr_sig(sys.argv[1]), shr_sig(sys.argv[2]); head = open(sys.argv[3]).readline().split()
ok = a == b and head[:2] == ['ntex', str(b[0])] and int(head[3], 16) == b[1]
print('  notoon: ntex %d sig %08x (файл набора %s) -> %s' % (b[0], b[1], ' '.join(head[:4]), 'сходится' if ok else 'НЕ СХОДИТСЯ')); sys.exit(0 if ok else 1)
PY
  mkdir -p "$RED/l$n"; VEND1328_OLD=1 GLASS1352_OLD=1 $PY tools/vend1328_patch.py $n "$f" "$RED/l$n/level.shr" > /dev/null; cmp -s "$f" "$RED/l$n/level.shr" && echo "  VEND1328_OLD=1 и GLASS1352_OLD=1: файл не меняется (копия)" || { echo "  FAIL: выключатель не вернул файл"; BAD=1; }
done
echo "== красный: копии без патча (VEND1328_OLD=1)"
PATCHED="$RED" ORIG="${ORIG:-$BASE}" SKIP_EXPORT=1 tests/vend1328/gate_family.sh "$BASE" "$EB" build/vend1328 > build/vend1328/gp_red.txt 2>&1; rc=$?
grep -E "^L[0-9]:" build/vend1328/gp_red.txt; [ $rc != 0 ] && echo "  красный пойман (gate_family на копиях: FAIL)" || { echo "  КРАСНЫЙ НЕ ПОЙМАН"; BAD=1; }
echo "== зелёный: патченные файлы"
PATCHED="$PD" ORIG="${ORIG:-$BASE}" SKIP_EXPORT=1 tests/vend1328/gate_family.sh "$BASE" "$EB" build/vend1328 $MODE > build/vend1328/gp_green.txt 2>&1; rc=$?
grep -v "^$" build/vend1328/gp_green.txt | cut -c1-230; [ $rc = 0 ] || BAD=1
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
