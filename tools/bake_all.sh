#!/bin/bash
# №bake: печь загрузки одной командой. Данные кладутся в OUT так, как лежат рядом с EBOOT: OUT/colbake.shr, OUT/texbake.shr (уровень 1) и OUT/l<N>/colbake.shr, OUT/l<N>/texbake.shr.
#   tools/bake_all.sh LEVELS_DIR SET OUT [УРОВНИ]
#     LEVELS_DIR — папка с level.shr ИМЕННО того набора, что поедет на PSP (L1: LEVELS_DIR/level.shr, L<N>: LEVELS_DIR/l<N>/level.shr), обычно сам SET. Чужой level.shr (build/levels старше набора) даёт другой ключ входа: движок молча считает по-старому
#     SET        — набор для tools/smoke.sh с EBOOT.PBP текущего движка (печь texbake идёт в эмуляторе тем же бинарником; нужна, когда меняются палитры/тон/текстуры/ключ тона)
#   colbake (хост, секунды, CPU) пересчитывается всегда; texbake (эмулятор, ~1–2 мин на уровень) — если не задано BAKE_TEX=0.
#   Проверка: tools/colbake_gate.sh и tools/texbake_gate.sh (эмулятор, побайтно; красные контроли -n)
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; LD="$1"; SET="$2"; OUT="$3"; shift 3; LV="${*:-1 2 3 4 5 6 7}"
mkdir -p "$OUT"
for n in $LV; do
  L="$LD/l$n/level.shr"; [ -f "$L" ] || { [ "$n" = 1 ] && L="$LD/level.shr"; }
  if [ "$n" = 1 ]; then D="$OUT"; else D="$OUT/l$n"; fi; mkdir -p "$D"
  "$ROOT/tools/colbake.sh" "$L" "$D/colbake.shr"
done
if [ "${BAKE_TEX:-1}" = 1 ]; then
  T="$(mktemp -d)"; "$ROOT/tools/texbake.sh" "$SET" "$T" $LV
  for n in $LV; do if [ "$n" = 1 ]; then cp "$T/l1/texbake.shr" "$OUT/texbake.shr"; else mkdir -p "$OUT/l$n"; cp "$T/l$n/texbake.shr" "$OUT/l$n/texbake.shr"; fi; done
fi
echo "bake_all: готово в $OUT"
