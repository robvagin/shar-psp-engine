#!/bin/bash
# Гейт №1328: автомат с газировкой после удара машиной уходит (целый корпус скрыт, виден разбитый, коллизия снята, 15 монет), на всех уровнях.
#   tests/vend1328/gate.sh НАБОР_108 EBOOT.PBP ДАННЫЕ_НОВЫЕ ДАННЫЕ_СТАРЫЕ [УРОВНИ]
#   НАБОР_108 — набор с данными (как для tools/smoke.sh); ДАННЫЕ_* — папки, где лежат l<N>new/level.shr (экспорт с правкой №1328) и l<N>old/level.shr (тот же экспорт с VEND1328_OLD=1)
# 1) данные: tests/vend1328/probe.c на level.shr каждого уровня — удар машиной по автомату принят, целый корпус скрыт, коллизия снята, 15 монет, три удара кулака дают то же.
#    Уровни без своих сборок (1, 4–7) берутся из набора как есть (экспорт у них не менялся).
# 2) картинка: безголовый эмулятор, golevel, телепорт машины к автомату, снимок до, «flag vend1328 1» (удар машиной), снимок после; строки VEND1328 + число изменившихся пикселей вокруг автомата.
# Красный контроль: старые данные (VEND1328_OLD=1) — на L2 удар машиной не принят, на L3 корпус не уходит; гейт обязан их поймать.
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; NEWD="$3"; OLDD="$4"; shift 4; LV="${*:-2 3}"
OUT=build/vend1328/gate; mkdir -p "$OUT"; BAD=0
cc -std=c11 -O1 -w -I viewer tests/vend1328/probe.c viewer/props.c viewer/stateprops.c viewer/col.c viewer/car.c viewer/coins.c -lm -o "$OUT/probe" || { echo "FAIL: сборка пробы"; exit 1; }
lvl() { if [ "$1" = 1 ]; then echo "$BASE/level.shr"; else echo "$BASE/l$1/level.shr"; fi; }
# проба: veh=1, если удар машиной принят, состояние ушло дальше 0, коллизия снята, 15 монет, включённых вершин меньше, чем у целого; hit=1 — то же после трёх ударов кулака
probe_ok() {   # файл -> строка итога
  "$OUT/probe" "$1" > "$OUT/p.txt" 2>&1
  awk '/^RES START/{split($3,a,"="); v0=a[2]}
       /^RES VEH/{split($3,a,"="); split($4,b,"="); split($5,c,"="); split($6,d,"="); split($7,e,"="); veh=(a[2]==1 && b[2]>0 && c[2]==0 && d[2]==15 && e[2]<v0)}
       /^RES HIT/{split($3,a,"="); split($4,b,"="); split($5,c,"="); split($6,d,"="); split($7,e,"="); hit=(b[2]>0 && c[2]==0 && d[2]==15 && e[2]<v0)}
       END{printf "veh=%d hit=%d вершин_целый=%d\n", veh+0, hit+0, v0}' "$OUT/p.txt"
}
echo "== данные (проба на level.shr)"
for n in 1 2 3 4 5 6 7; do
  if [ "$n" = 2 ] || [ "$n" = 3 ]; then fn="$NEWD/l${n}new/level.shr"; fo="$OLDD/l${n}old/level.shr"; else fn=$(lvl $n); fo=""; fi
  rn=$(probe_ok "$fn"); echo "L$n новые: $rn"
  case "$rn" in veh=1\ hit=1*) ;; *) BAD=1; echo "  FAIL L$n";; esac
  if [ -n "$fo" ]; then ro=$(probe_ok "$fo"); echo "L$n старые (красный): $ro"; case "$ro" in veh=1\ hit=1*) echo "  КРАСНЫЙ НЕ ПОЙМАН L$n"; BAD=1;; *) echo "  красный пойман L$n";; esac; fi
done
[ "${DATA_ONLY:-0}" = 1 ] && { [ $BAD = 0 ] && echo "PASS (только данные)" || echo FAIL; exit $BAD; }
echo "== картинка (эмулятор)"
pos() { "$OUT/probe" "$1" | awk '/INST/{print $4, $5, $6, $8, $9; exit}'; }   # x y z fx fz первого автомата
shot_run() {   # n режим файл_уровня
  local n=$1 mode=$2 lv=$3 S="$OUT/set_${mode}_$n"; rm -rf "$S"; cp -Rc "$BASE" "$S"; cp "$EB" "$S/EBOOT.PBP"
  if [ "$n" = 1 ]; then cp "$lv" "$S/level.shr"; else cp "$lv" "$S/l$n/level.shr"; fi
  read X Y Z FX FZ < <(pos "$lv")
  local sc="$OUT/sc_${mode}_$n.txt"
  python3 - "$X" "$Y" "$Z" "$FX" "$FZ" "$n" > "$sc" <<'PY'
import sys, math
X, Y, Z, FX, FZ = map(float, sys.argv[1:6]); n = int(sys.argv[6])
cx, cz = X + 4.5 * FX, Z + 4.5 * FZ; h = math.atan2(-FX, -FZ)
ex, ey, ez = X + 7 * FX - 3 * FZ, Y + 2.6, Z + 7 * FZ + 3 * FX
lx, ly, lz = X - ex, Y + 1.2 - ey, Z - ez
L = ["flag nosave"]
if n != 1: L += [f"flag golevel {n}", "wait 40"]
L += ["scene a", "wait 3", f"goto {cx:.2f} {cz:.2f} {h:.4f}", "wait 5", f"cam {ex:.2f} {ey:.2f} {ez:.2f} {lx:.2f} {ly:.2f} {lz:.2f} 50",
      "wait 1.5", "flag vend1328 0", "shot vend_before", "flag vend1328 1", "wait 3", "flag vend1328 0", "shot vend_after", "wait 1", "end"]
print("\n".join(L))
PY
  local R; R=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=400 SMOKE_SCN="$sc" tools/smoke.sh "$S" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  case "$R" in /*) ;; *) R="$PWD/$R" ;; esac
  rm -rf "$S"
  grep -a '^VEND1328' "$R/smoke.txt" | tail -3 > "$OUT/vl_${mode}_$n.txt"
  for w in before after; do f=$(ls "$R"/*vend_$w*.png 2>/dev/null | head -1); [ -n "$f" ] && cp "$f" "$OUT/L${n}_${mode}_$w.png"; done
}
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
imgdiff() { python3 - "$1" "$2" <<'PY'
import sys
from PIL import Image, ImageChops
a, b = Image.open(sys.argv[1]).convert('RGB'), Image.open(sys.argv[2]).convert('RGB')
d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 24 else 0)
print(sum(d.histogram()[255:]))
PY
}
for n in $LV; do
  for mode in new old; do
    if [ $mode = new ]; then lv="$NEWD/l${n}new/level.shr"; else lv="$OLDD/l${n}old/level.shr"; fi
    shot_run $n $mode "$lv"
    b=$(grep -a 'hit=0' "$OUT/vl_${mode}_$n.txt" | head -1); a=$(tail -1 "$OUT/vl_${mode}_$n.txt")
    d=-1; [ -f "$OUT/L${n}_${mode}_before.png" ] && [ -f "$OUT/L${n}_${mode}_after.png" ] && d=$(imgdiff "$OUT/L${n}_${mode}_before.png" "$OUT/L${n}_${mode}_after.png")
    echo "L$n $mode: до: ${b:-нет строки}"; echo "L$n $mode: после: ${a:-нет строки}; пикселей изменилось $d"
    va=$(fld "$a" verts_on); vb=$(fld "$b" verts_on); sa=$(fld "$a" state); ca=$(fld "$a" colOn)
    if [ $mode = new ]; then
      [ "${sa:-0}" -gt 0 ] 2>/dev/null && [ "${ca:-1}" = 0 ] && [ "${va:-999}" -lt "${vb:-0}" ] 2>/dev/null && [ "$d" -gt 400 ] 2>/dev/null || { echo "  FAIL L$n новые данные"; BAD=1; }
    else
      if [ "${sa:-0}" -gt 0 ] 2>/dev/null && [ "${va:-999}" -lt "${vb:-0}" ] 2>/dev/null; then echo "  КРАСНЫЙ НЕ ПОЙМАН L$n (старые данные ведут себя как новые)"; BAD=1; else echo "  красный пойман L$n: на старых данных корпус не ушёл (вершин $vb -> $va, состояние $sa)"; fi
    fi
  done
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
