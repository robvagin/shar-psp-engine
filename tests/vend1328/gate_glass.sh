#!/bin/bash
# Гейт №1352: стекло автомата с Красти без тона уровня (цвет вершин нейтральный, как на L1), кожа Красти на снимке близка к рисунку диска.
#   tests/vend1328/gate_glass.sh НАБОР EBOOT.PBP ИСХОДНЫЕ ПАТЧЕННЫЕ [img]    ИСХОДНЫЕ/l<N>/level.shr — файлы до патча (набор 107), ПАТЧЕННЫЕ/l<N>/level.shr — после tools/vend1328_patch.py, N = 2..7
# 1) данные: probe «glass» — цвет вершин стекла на патченных = FFFDFDFD на L2–L7 (на L1 так и было); красный: на исходных L2–L7 цвет другой.
# 2) отличие патченного от исходного: tools/vend1328_patch.py --diff печатает «вершин с новым цветом 6, с другими полями 0» (кроме таблиц состояний №1328).
# 3) пипетка (img): крупный снимок стекла в эмуляторе (glassshot.sh), tests/vend1328/glassmeasure.py: ΔE (CIE76) кожи Красти на снимке к рисунку диска. Зелёный: на новых ΔE <= 6 на каждом уровне с кадром;
#    красный: на исходных ΔE больше 12 хотя бы на трёх из L2, L3, L5, L6 (зелёный, оранжевый, жёлтый, синий), на новых ΔE меньше вдвое. L4 и L7 в безголовом эмуляторе рисуются чёрным (ночные, кадр пуст) — у них только данные. REUSE=1 — взять готовые снимки.
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; ORIG="$3"; PD="$4"; MODE="${5:-data}"; PY=.venv/bin/python; OUT=build/vend1328/gg; mkdir -p "$OUT"; BAD=0
cc -std=c11 -O1 -w -I viewer tests/vend1328/probe.c viewer/props.c viewer/stateprops.c viewer/col.c viewer/car.c viewer/coins.c -lm -o "$OUT/probe" || { echo "FAIL: сборка пробы"; exit 1; }
echo "== 1. данные: цвет вершин стекла (ABGR)"
L1=$("$OUT/probe" "$BASE/level.shr" glass | awk '/^GLASS/{print $NF}'); echo "L1 (набор): $L1"
for n in 2 3 4 5 6 7; do
  o=$("$OUT/probe" "$ORIG/l$n/level.shr" glass | awk '/^GLASS/{print $NF}'); p=$("$OUT/probe" "$PD/l$n/level.shr" glass | awk '/^GLASS/{print $NF}')
  echo "L$n: исходный $o -> патченный $p"
  [ "$p" = fffdfdfd ] || { echo "  FAIL L$n: цвет не нейтральный"; BAD=1; }
  [ "$o" != fffdfdfd ] && echo "  красный пойман L$n (исходный окрашен)" || { echo "  КРАСНЫЙ НЕ ПОЙМАН L$n"; BAD=1; }
done
echo "== 2. отличия файлов"
for n in 2 3 4 5 6 7; do $PY tools/vend1328_patch.py --diff "$ORIG/l$n/level.shr" "$PD/l$n/level.shr" | sed -n 2,3p | sed "s/^/L$n /" | cut -c1-330; [ ${PIPESTATUS[0]} = 0 ] || { echo "  FAIL L$n: отличия вне SPR1/цвета"; BAD=1; }; done
[ "$MODE" = img ] || { [ $BAD = 0 ] && echo "PASS (данные)" || echo FAIL; exit $BAD; }
echo "== 3. пипетка (ΔE кожи Красти к рисунку диска)"
declare -a NEWDE OLDDE
for n in 2 3 5 6; do
  for mode in new old; do
    if [ $mode = new ]; then lv="$PD/l$n/level.shr"; else lv="$ORIG/l$n/level.shr"; fi
    { [ "${REUSE:-0}" = 1 ] && [ -f "$OUT/L${n}_$mode.png" ]; } || LEVELFILE="$lv" GD=3 GF=22 tests/vend1328/glassshot.sh "$BASE" "$EB" $n "$OUT/L${n}_$mode.png" > /dev/null 2>&1
    r=$($PY tests/vend1328/glassmeasure.py "$OUT/L${n}_$mode.png" 2>&1 | tail -1)
    de=$(echo "$r" | sed -n 's/.*"dE_render_vs_disk": \([0-9.]*\).*/\1/p'); rt=$(echo "$r" | sed -n 's/.*"ratio_render_over_expected": \(\[[^]]*\]\).*/\1/p'); rg=$(echo "$r" | sed -n 's/.*"render": \(\[[^]]*\]\).*/\1/p'); cr=$(echo "$r" | sed -n 's/.*"corr": \([0-9.]*\).*/\1/p')
    echo "L$n $mode: кожа на снимке $rg, ΔE к диску ${de:-нет}, совпадение раскладки $cr, движок/ожидаемое $rt"
    eval "DE_${mode}_$n=${de:--1}"
  done
done
bigold=0
for n in 2 3 5 6; do
  eval "dn=\$DE_new_$n; do_=\$DE_old_$n"
  ok=$(python3 -c "import sys; dn=float('$dn'); do=float('$do_'); print(1 if dn>=0 and dn<=6 else 0)")
  [ "$ok" = 1 ] || { echo "  FAIL L$n: ΔE на новых $dn (нужно <= 6)"; BAD=1; }
  big=$(python3 -c "do=float('$do_'); dn=float('$dn'); print(1 if do>12 and dn<do/2 else 0)"); bigold=$((bigold + big))
done
[ $bigold -ge 3 ] && echo "красный пойман: на исходных ΔE больше 12 на $bigold из 4 уровней, на новых вдвое меньше" || { echo "КРАСНЫЙ НЕ ПОЙМАН ($bigold из 4)"; BAD=1; }
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
