#!/bin/bash
# №927: △ на заднем ходу тормозит и Гомер выходит. Красный: -DGAME_EXIT_REV_OLD=1 (прежнее) обязан упасть (на 10 м/с назад выход за 2,15 с при пределе 1,5 с).
#   tests/controls/exit_rev.sh [LEVEL1.shr HOMER.shr level01.msn]
# Данные по умолчанию — ТЕКУЩИЙ набор: самый свежий по дате level.shr из LEVEL1 / build/levels/l1 / build/t*/data / build, у которого рядом лежит traffic.shr (то же для HOMER и MSN).
# Без traffic.shr рядом с level.shr игра без трафика, и красный не краснеет (на 10 м/с выход за 0,77 с вместо 2,15 с): гейт тогда падает с подсказкой, а не проходит молча.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/controls; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
SETD=${SET:-$HOME/Downloads/shar-psp-integ/build/test59tour/PSP/GAME/SHARTEST15}   # набор тура TEST59 (level.shr с traffic.shr, homer.shr, level01.msn) — запасной источник данных
[ -d /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk ] && export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
newest() { local f best=""; for f in "$@"; do [ -f "$f" ] || continue; case "$f" in */level.shr|level.shr) [ -f "$(dirname "$f")/traffic.shr" ] || continue;; esac; { [ -z "$best" ] || [ "$f" -nt "$best" ]; } && best="$f"; done; echo "$best"; }
L1=${1:-$(newest ${LEVEL1:-} build/levels/l1/level.shr build/t*/data/level.shr build/level.shr $SETD/level.shr)}
HM=${2:-$(newest ${HOMER:-} build/homer.shr build/t*/data/homer.shr $SETD/homer.shr)}
MSN=${3:-$(newest ${MSN1:-} build/missions/level01.msn build/t*/data/level01.msn build/level01.msn $SETD/level01.msn)}
for f in "$L1" "$HM" "$MSN"; do [ -n "$f" ] && [ -f "$f" ] || { echo "FAIL: нет данных (LEVEL1=$L1 HOMER=$HM MSN=$MSN): нужен level.shr текущего набора с traffic.shr рядом; задайте аргументами или LEVEL1/HOMER/MSN1"; exit 1; }; done
echo "данные: $L1 ($(date -r "$L1" '+%d.%m %H:%M')), $HM, $MSN"
ARGS="$L1 $HM $MSN ${EXIT_REV_SEC:-1.5}"
$CC -DGAME_EXIT_PS2_OLD=1 -DGAME_EXIT_REV_OLD=1 tests/controls/exit_rev_host.c $BOTS_LIB -lm -o "$OUT/exit_rev_red" || { echo "FAIL: сборка красного"; exit 1; }
"$OUT/exit_rev_red" $ARGS > "$OUT/exit_rev_red.txt" 2>&1; rr=$?
if grep -q "трафик: .* не читается" "$OUT/exit_rev_red.txt"; then echo "FAIL: рядом с $L1 нет traffic.shr (город без трафика, красный не краснеет): возьмите level.shr текущего набора с traffic.shr"; exit 1; fi
if [ $rr = 0 ]; then echo "КРАСНЫЙ НЕ ПОЙМАН: на 10 м/с $(grep -m1 ' 10.0 м/с' "$OUT/exit_rev_red.txt" | cut -c1-110); нужен level.shr текущего набора"; red=0
else red=1; echo "красный: $(grep -c 'CHECK.*FAIL' "$OUT/exit_rev_red.txt") провал(а): $(grep -m1 'CHECK.*FAIL' "$OUT/exit_rev_red.txt" | cut -c1-150)"; fi
$CC tests/controls/exit_rev_host.c $BOTS_LIB -lm -o "$OUT/exit_rev" || { echo "FAIL: сборка"; exit 1; }
echo "зелёный:"; "$OUT/exit_rev" $ARGS | grep -E "CHECK|RESULT"; green=${PIPESTATUS[0]}
[ $red = 1 ] && [ $green = 0 ] && echo "PASS (красный пойман, зелёный сошёлся)" || { echo "FAIL"; exit 1; }
