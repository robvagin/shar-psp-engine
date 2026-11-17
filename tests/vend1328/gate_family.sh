#!/bin/bash
# Гейт №1328 (семейство): ВСЕ стейтпропы L1–L7 — видимость состояний по номеру записи (как в игре), блок машины состояний последний.
#   tests/vend1328/gate_family.sh НАБОР_108 EBOOT.PBP ДАННЫЕ(папка с fam/l<N>new, l3new) [картинки: img]
#   ORIG=папка с l<N>/level.shr до патча (если набор уже с патчем): красный берётся оттуда
#   PATCHED=папка_с_l<N>/level.shr — вместо полных перевыпусков брать патченные файлы набора (tools/vend1328_patch.py); SKIP_EXPORT=1 — без части 1 (экспорт на диске, долго); SKIP_DATA=1 — без частей 1–2
# 1) экспорт: tests/vend1328/vis_all.py — на диске, все типы и состояния; красный VEND1328_OLD=1 обязан не сойтись.
# 2) level.shr: у каждого уровня видимость состояний всех типов из файла (проба, режим all) == ожидаемому из диска по номеру; красный — level.shr набора 108 (выпущен до правки) на L2–L5, L7 обязан не сойтись.
# 3) картинка (img): удар машиной по колонке L2, воротам L2, киоску томатов L4, снимки до/после на новых и на старых данных; корпус/части должны уйти (verts_on меньше), на старых — нет.
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; D="$3"; MODE="${4:-data}"; PY=.venv/bin/python; OUT=build/vend1328/gatef; mkdir -p "$OUT"; BAD=0
cc -std=c11 -O1 -w -I viewer tests/vend1328/probe.c viewer/props.c viewer/stateprops.c viewer/col.c viewer/car.c viewer/coins.c -lm -o "$OUT/probe" || { echo "FAIL: сборка пробы"; exit 1; }
newf() { if [ -n "${PATCHED:-}" ]; then case $1 in 2|3|4|5|7) echo "$PATCHED/l$1/level.shr"; return;; esac; fi; case $1 in 2|4|5|7) echo "$D/fam/l${1}new/level.shr";; 3) echo "$D/l3new/level.shr";; 1) echo "$BASE/level.shr";; *) echo "$BASE/l$1/level.shr";; esac; }
oldf() { local d="${ORIG:-$BASE}"; if [ "$1" = 1 ]; then echo "$BASE/level.shr"; else echo "$d/l$1/level.shr"; fi; }
if [ "${SKIP_DATA:-0}" != 1 ]; then
if [ "${SKIP_EXPORT:-0}" != 1 ]; then
echo "== 1. экспорт, все типы L1–L7"
$PY tests/vend1328/vis_all.py > "$OUT/e_new.txt" 2>&1; rc=$?; head -1 "$OUT/e_new.txt"; [ $rc = 0 ] || { echo "  FAIL"; BAD=1; }
VEND1328_OLD=1 $PY tests/vend1328/vis_all.py > "$OUT/e_old.txt" 2>&1; rc=$?; echo "  красный (VEND1328_OLD=1): $(head -1 "$OUT/e_old.txt")"; [ $rc != 0 ] && echo "  красный пойман" || { echo "  КРАСНЫЙ НЕ ПОЙМАН"; BAD=1; }
fi
echo "== 2. level.shr (проба)"
for n in 1 2 3 4 5 6 7; do
  $PY tests/vend1328/vis_all.py --expect $n | sed 's/^TV [^ ]* //' | sort > "$OUT/exp$n.txt"
  "$OUT/probe" "$(newf $n)" all | sed 's/^TV [0-9]* //' | sort > "$OUT/new$n.txt"
  "$OUT/probe" "$(oldf $n)" all | sed 's/^TV [0-9]* //' | sort > "$OUT/old$n.txt"
  # проба видит ещё типы без машины состояний (статика с одним состоянием): сверяем, что каждый ожидаемый ряд видимости есть в файле
  miss=$(comm -23 "$OUT/exp$n.txt" "$OUT/new$n.txt" | wc -l | tr -d ' '); missold=$(comm -23 "$OUT/exp$n.txt" "$OUT/old$n.txt" | wc -l | tr -d ' ')
  echo "L$n: ожидаемых типов стейтпропов $(wc -l < "$OUT/exp$n.txt" | tr -d ' '), нет в файле (не по номеру): новые $miss, старые(набор 108) $missold"
  [ "$miss" = 0 ] || { echo "  FAIL L$n"; BAD=1; }
  case $n in 2|3|4|5|7) [ "$missold" != 0 ] && echo "  красный пойман L$n" || { echo "  КРАСНЫЙ НЕ ПОЙМАН L$n"; BAD=1; };; esac
done
fi
[ "$MODE" = img ] || { [ $BAD = 0 ] && echo "PASS (данные)" || echo FAIL; exit $BAD; }
echo "== 3. картинка (эмулятор)"
typeof() { "$OUT/probe" "$1" all | awk -v want="$2" '{s=$0; sub(/^TV [0-9]+ /,"",s); if (s==want) {print $2; exit}}'; }
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
imgdiff() { $PY - "$1" "$2" <<'PY'
import sys
from PIL import Image, ImageChops
a, b = Image.open(sys.argv[1]).convert('RGB'), Image.open(sys.argv[2]).convert('RGB')
d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 24 else 0)
print(sum(d.histogram()[255:]))
PY
}
one() {   # n режим файл тип метка
  local n=$1 mode=$2 lv=$3 T=$4 tag=$5 S="$OUT/set_${tag}_${mode}"; rm -rf "$S"; cp -Rc "$BASE" "$S"; cp "$EB" "$S/EBOOT.PBP"
  if [ "$n" = 1 ]; then cp "$lv" "$S/level.shr"; else cp "$lv" "$S/l$n/level.shr"; fi
  local line; line=$("$OUT/probe" "$(newf $n)" types | awk -v t="$T" '$2==t'); local X Y Z FX FZ
  read X Y Z FX FZ < <(echo "$line" | sed 's/.* at //' | awk '{print $1,$2,$3,$5,$6}')
  local sc="$OUT/sc_${tag}_${mode}.txt"
  $PY - "$X" "$Y" "$Z" "$FX" "$FZ" "$n" "$T" > "$sc" <<'PY'
import sys, math
X, Y, Z, FX, FZ = map(float, sys.argv[1:6]); n = int(sys.argv[6]); T = int(sys.argv[7])
cx, cz = X + 12 * FX, Z + 12 * FZ; h = math.atan2(-FX, -FZ)
ex, ey, ez = X + 6 * FX - 4 * FZ, Y + 2.6, Z + 6 * FZ + 4 * FX
lx, ly, lz = X - ex, Y + 1.2 - ey, Z - ez
L = ["flag nosave"]
if n != 1: L += [f"flag golevel {n}", "wait %d" % (90 if n in (4, 7) else 40)]
L += ["scene a", "wait 3", f"goto {cx:.2f} {cz:.2f} {h:.4f}", "wait 5", f"cam {ex:.2f} {ey:.2f} {ez:.2f} {lx:.2f} {ly:.2f} {lz:.2f} 55",
      "wait 1.5", f"flag sphit1328 {T} 0", "shot sp_before", f"flag sphit1328 {T}", "wait 0.5", f"flag sphit1328 {T} 0", "shot sp_mid", "wait 3", f"flag sphit1328 {T} 0", "shot sp_after", "wait 1", "end"]
print("\n".join(L))
PY
  local R; R=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=500 SMOKE_SCN="$sc" tools/smoke.sh "$S" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  case "$R" in /*) ;; *) R="$PWD/$R" ;; esac; rm -rf "$S"
  grep -a '^SPHIT1328' "$R/smoke.txt" | tail -4 > "$OUT/sl_${tag}_${mode}.txt"
  for w in before mid after; do f=$(ls "$R"/*sp_$w*.png 2>/dev/null | head -1); [ -n "$f" ] && cp "$f" "$OUT/${tag}_${mode}_$w.png"; done
}
# метка:уровень:отпечаток видимости новых данных (состояния 0 и 1)
for spec in "gaspump_L2:2:0000007b 00000074" "gatesmasha_L2:2:000001ff 00000180" "tomaccostand_L4:4:00000001 0000fffe"; do
  tag=${spec%%:*}; r=${spec#*:}; n=${r%%:*}; fp=${r#*:}
  T=$(typeof "$(newf $n)" "$fp"); [ -n "$T" ] || { echo "  FAIL $tag: тип не найден"; BAD=1; continue; }
  for mode in new old; do
    if [ $mode = new ]; then lv=$(newf $n); else lv=$(oldf $n); fi
    one $n $mode "$lv" "$T" "${tag}"
    b=$(sed -n 1p "$OUT/sl_${tag}_${mode}.txt"); m=$(sed -n 3p "$OUT/sl_${tag}_${mode}.txt"); a=$(sed -n 4p "$OUT/sl_${tag}_${mode}.txt")
    eval "VB_$mode=$(fld "$b" verts_on) VM_$mode=$(fld "$m" verts_on) VA_$mode=$(fld "$a" verts_on)"
    d=-1; [ -f "$OUT/${tag}_${mode}_before.png" ] && [ -f "$OUT/${tag}_${mode}_mid.png" ] && d=$(imgdiff "$OUT/${tag}_${mode}_before.png" "$OUT/${tag}_${mode}_mid.png")
    echo "$tag (тип $T) $mode: до: ${b:-нет}"; echo "$tag $mode: через 0,5 с: ${m:-нет}; пикселей изменилось против «до» $d"; echo "$tag $mode: через 3,5 с: ${a:-нет}"
  done
  # новые данные: рисуется меньше вершин, чем на старых (старые держат все рисунки включёнными сразу после удара); красный — старые, где не меньше
  vnm=${VM_new:-0}; vom=${VM_old:-0}; vna=${VA_new:-0}; voa=${VA_old:-0}
  if [ "$vnm" -lt "$vom" ] 2>/dev/null || [ "$vna" -lt "$voa" ] 2>/dev/null; then echo "  $tag: новые данные рисуют меньше (0,5 с: $vnm против $vom; 3,5 с: $vna против $voa) — красный на старых пойман"; else echo "  FAIL $tag: новые данные не отличаются от старых"; BAD=1; fi
  cp "$OUT/${tag}_new_mid.png" "$OUT/final_${tag}_new.png" 2>/dev/null
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
