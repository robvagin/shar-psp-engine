#!/bin/bash
# Гейты регулятора дальности (1244), возврата после сторожа круга, потолка «места» и круга клеток (99). tests/ddreg1244/run.sh EBOOT [ИМЯ_ГЕЙТА...]
# Окружение как у tools/emu_run.sh (SET набор данных, OUT рабочая папка), безголово, nice 20, эмулятор один. Результат: $OUT/ddreg_<гейт>.txt, последняя строка PASS/FAIL; для красных - ожидается FAIL.
# ВАЖНО (99): после правки STREAMTIGHT99 кругов клеток на L2 больше нет, поэтому гейты возврата и потолка ОБЯЗАНЫ идти с файлом streamtight99old (старый счёт загрузки/отдачи воспроизводит круг, на котором
# эти гейты проверяют логику возврата и потолка). Гейт nocircle - наоборот, без этого файла (проверяет, что круга нет), красный ему - с файлом.
#   reg      регулятор по кадру: FLAGS cellthrashold (сторож круга выключен, память не держит дальность)   scene.txt          check.py            красный: ddreg1244old
#   thrash   возврат после круга: streamtight99old cellthrashceilold                                       scene.txt          check.py --thrash   красный: + cellthrashrestold
#   ceil     потолок места: streamtight99old                                                               scene_ceil.txt     check.py --ceil     красный: + cellthrashceilold
#   nocircle круга нет: cellthrashceilold                                                                  scene_ceil.txt     check.py --nocircle красный: + streamtight99old
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; shift; GATES=${*:-reg thrash ceil nocircle}
OUT=${OUT:-$W/build/ddreg_gates}; export OUT; mkdir -p "$OUT"; cd "$W"; export SMOKE_HEADLESS=1; rc=0
one() { # имя сценарий ключ_проверки флаги ожидание(G/R)
  local n=$1 sc=$2 ck=$3 fl=$4 ex=$5 o=$OUT/out_$1
  FLAGS="$fl" nice -n 20 tools/emu_run.sh "$H/$sc" "$n" "$EB" 800 > "$OUT/$n.log" 2>&1
  grep -E "^(DDREG|SCENE|END|WATCHDOG cell)" "$o/smoke.txt" > "$o/key.txt"; python3 -I "$H/check.py" "$o/key.txt" $ck > "$OUT/ddreg_$n.txt" 2>&1; local c=$?
  find "$o" -type f \( -name smoke.txt -o -name blackbox.txt -o -name headless.log -o -name '*.bmp' \) -delete; rm -rf "$OUT/emu_home_$n"
  echo "$n: $(tail -1 "$OUT/ddreg_$n.txt") (ожидается $([ $ex = G ] && echo PASS || echo FAIL))"
  if [ $ex = G ] && [ $c != 0 ]; then rc=1; fi; if [ $ex = R ] && [ $c = 0 ]; then echo "  красный контроль НЕ покраснел"; rc=1; fi
}
for g in $GATES; do case $g in
  reg)      one reg_g scene.txt "" "cellthrashold" G; one reg_r scene.txt "" "cellthrashold ddreg1244old" R;;
  thrash)   one thr_g scene.txt --thrash "streamtight99old cellthrashceilold citycap100old" G; one thr_r scene.txt --thrash "streamtight99old cellthrashceilold citycap100old cellthrashrestold" R;;
  ceil)     one ceil_g scene_ceil.txt --ceil "streamtight99old citycap100old" G; one ceil_r scene_ceil.txt --ceil "streamtight99old citycap100old cellthrashceilold" R;;
  nocircle) one noc_g scene_ceil.txt --nocircle "cellthrashceilold" G; one noc_r scene_ceil.txt --nocircle "cellthrashceilold streamtight99old" R;;
esac; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
