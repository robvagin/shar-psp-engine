#!/bin/bash
# Гейт звука заставки. №1011 (Роб 26.10): по умолчанию — четыре ноты темы спикером, без щелчка и кулера (tests/bootsnd/host1011.c);
# прежняя партитура правки 128 (щелчок, кулер, бип POST) остаётся за BOOTSND1011_OLD=1 и проверяется прежним host.c.
# Сначала КРАСНЫЙ — проверки обязаны упасть на испорченной партитуре; потом ЗЕЛЁНЫЙ. Итог: build/bootsnd/*.txt, build/bootsnd1011.wav, build/bootsnd_A/B/C.wav
set -u
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}   # лицензия Xcode не принята
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/bootsnd; mkdir -p "$OUT"
n1011() { cc -std=c99 -O1 -Wall -I viewer "$@" tests/bootsnd/host1011.c -o build/bootsnd1011-host -lm && build/bootsnd1011-host; }
red=0; nred=0
for plant in -DBOOTSND1011_OLD=1 -DBOOT_TUNE_DIV=1193 -DBOOT_TUNE_LP=0.55f -DBOOT_SND_CHUNK=1000; do
  nred=$((nred + 1)); n1011 "$plant" > "$OUT/red1011_${plant#-D}.txt" 2>&1; rc=$?
  if [ $rc -ne 0 ]; then echo "красный №1011 ($plant): упал, как должен — $(grep -m1 -E ': нет|НЕТ' "$OUT/red1011_${plant#-D}.txt" | cut -c1-110)"; red=$((red + 1))
  else echo "КРАСНЫЙ НЕ ПОЙМАН: $plant прошёл гейт №1011"; fi
done
n1011 > "$OUT/green1011.txt" 2>&1; rc1=$?
grep -E "тишина|нот |нота |пик|RESULT" "$OUT/green1011.txt"
# прежняя партитура (правка 128) за выключателем: её проверки и красные на месте
for plant in -DBOOT_SND_CLICK=0 -DBOOT_SND_FAN_TAU=0 -DBOOT_BEEP_LP=0.55f -DBOOT_SND_CHUNK=1000; do
  nred=$((nred + 1)); bash tests/bootsnd/host.sh -DBOOTSND1011_OLD=1 "$plant" > "$OUT/red_${plant#-D}.txt" 2>&1; rc=$?
  if [ $rc -ne 0 ]; then red=$((red + 1)); else echo "КРАСНЫЙ НЕ ПОЙМАН (прежняя партитура): $plant"; fi
done
bash tests/bootsnd/host.sh -DBOOTSND1011_OLD=1 > "$OUT/green.txt" 2>&1; rc0=$?
echo "прежняя партитура (BOOTSND1011_OLD=1): $(grep RESULT "$OUT/green.txt")"
if [ $red -eq $nred ] && [ $rc1 -eq 0 ] && [ $rc0 -eq 0 ]; then echo "PASS: красный $red/$nred пойман, зелёный OK (№1011 и прежняя партитура)"; exit 0; fi
echo "FAIL: красный $red/$nred, зелёный №1011 rc=$rc1, прежняя rc=$rc0"; exit 1
