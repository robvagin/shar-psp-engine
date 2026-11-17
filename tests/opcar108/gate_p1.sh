#!/bin/bash
# 108 П1: гейт страниц карты высот и стен. tests/opcar108/gate_p1.sh EBOOT [SET с lmap.shr версии 2]  ->  PASS/FAIL
#  1) L1..L7: lmapverify — все ячейки сетки, страницы против плотного массива level.shr, расхождений 0 (строка LMAPVERIFY);
#  2) красный контроль: с файлом lmapbreak (одна высота испорчена) расхождения > 0 на L1;
#  3) куча: L1 и L4, ballast 5, езда 25 с — минимум кучи новое больше прежнего (p1pagesold) не меньше чем на 1,5 МБ; красный: с p1pagesold строки LMAPPG нет.
#  Прогоны по одному (Мак на батарее): EMU_MINE=1
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$PWD/build/opcar108/set108} SMOKE_HEADLESS=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/opcar108} EMU_MINE=1; mkdir -p "$OUT"
sc() { tests/opcar108/scene_lv.sh $1 | sed 's/^wait 25$/wait '$2'/; s/^wait 10$/wait 6/' > "$OUT/g$1_$2.txt"; echo "$OUT/g$1_$2.txt"; }
run() { n=$1; L=$2; F="$3"; W=$4; FLAGS="$F" nice -n 20 tools/emu_run.sh "$(sc $L $W)" $n "$EB" 200 > "$OUT/$n.log" 2>&1; echo "$OUT/out_$n/smoke.txt"; }
fail=0; res=""
for L in 1 2 3 4 5 6 7; do f=$(run gv$L $L "lmapverify" 2); l=$(grep -h '^LMAPVERIFY' $f); b=$(echo "$l" | sed -n 's/.*hm_bad=\([0-9]*\) wall_bad=\([0-9]*\).*/\1 \2/p'); res="$res\nL$L $l"; [ "$b" = "0 0" ] || fail=1; done
f=$(run gred 1 "lmapverify lmapbreak" 2); l=$(grep -h '^LMAPVERIFY' $f); res="$res\nRED $l"; hb=$(echo "$l" | sed -n 's/.*hm_bad=\([0-9]*\).*/\1/p'); [ "${hb:-0}" -gt 0 ] || fail=1
for L in 1 4; do fn=$(run gmn$L $L "ballast=5" 25); fo=$(run gmo$L $L "ballast=5 p1pagesold" 25)
  mn=$(grep -h 'STAT EQ ' $fn | grep -o 'heapmin [0-9.]*' | cut -d' ' -f2); mo=$(grep -h 'STAT EQ ' $fo | grep -o 'heapmin [0-9.]*' | cut -d' ' -f2)
  res="$res\nL$L heapmin новое=$mn прежнее=$mo, LMAPPG при p1pagesold: $(grep -c '^LMAPPG' $fo)"
  awk -v a="$mn" -v b="$mo" 'BEGIN{exit !(a-b>=1.5)}' || fail=1; [ "$(grep -c '^LMAPPG' $fo)" = 0 ] || fail=1; done
printf "$res\n"; [ $fail = 0 ] && echo "P1 GATE PASS" || echo "P1 GATE FAIL"; rm -rf "$OUT"/rr_* "$OUT"/emu_home_*; exit $fail
