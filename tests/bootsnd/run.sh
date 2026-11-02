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
# №1094: реплика продавца комиксов вместо нот. Клип берётся из tools/bootvo_clip.txt (нет имени — проверка на пробном клипе cbg/p_hitbyc_cbg_01.rsd: проверяется распаковка и
# включение, а не выбор клипа). Красные: писк (BOOTVO1094_OLD=1), глухая реплика (BOOT_VO_GAIN=0), испорченный файл (первый байт)
PY=${PY:-$HOME/Downloads/shar-psp/.venv/bin/python}
if [ -x "$PY" ] && [ -d hit-and-run-web/source/game/cbg ]; then
  CLIP=$(grep -v '^#' tools/bootvo_clip.txt | head -1); [ -n "$CLIP" ] || CLIP=cbg/p_hitbyc_cbg_01.rsd
  "$PY" tools/bootvo_export.py --out "$OUT/bootvo_t.bin" --ref "$OUT/bootvo_t.ref" --clip "$CLIP" > /dev/null || { echo "FAIL: bootvo_export"; exit 1; }
  cp "$OUT/bootvo_t.bin" "$OUT/bootvo_bad.bin"; printf 'X' | dd of="$OUT/bootvo_bad.bin" bs=1 count=1 conv=notrunc 2>/dev/null
  n1094() { cc -std=c99 -O1 -w -I viewer "$@" tests/bootsnd/host1094.c -o build/bootsnd1094-host -lm && build/bootsnd1094-host "$BIN" "$OUT/bootvo_t.ref"; }
  for plant in -DBOOTVO1094_OLD=1 -DBOOT_VO_GAIN=0.0f bad; do
    nred=$((nred + 1)); BIN="$OUT/bootvo_t.bin"; flag="$plant"; [ "$plant" = bad ] && { BIN="$OUT/bootvo_bad.bin"; flag=-DX=1; }
    n1094 "$flag" > "$OUT/red1094_${plant#-D}.txt" 2>&1; rc=$?
    if [ $rc -ne 0 ]; then red=$((red + 1)); echo "красный №1094 ($plant): упал, как должен — $(grep -m1 '^нет' "$OUT/red1094_${plant#-D}.txt" | cut -c1-100)"; else echo "КРАСНЫЙ НЕ ПОЙМАН: №1094 $plant"; fi
  done
  BIN="$OUT/bootvo_t.bin"; n1094 > "$OUT/green1094.txt" 2>&1; rc1094=$?; grep -E "клип:|RESULT" "$OUT/green1094.txt"
else echo "пропуск №1094: нет .venv или диска игры"; rc1094=0; fi
if [ $red -eq $nred ] && [ $rc1 -eq 0 ] && [ $rc0 -eq 0 ] && [ ${rc1094:-0} -eq 0 ]; then echo "PASS: красный $red/$nred пойман, зелёный OK (№1011 и прежняя партитура)"; exit 0; fi
echo "FAIL: красный $red/$nred, зелёный №1011 rc=$rc1, прежняя rc=$rc0"; exit 1
