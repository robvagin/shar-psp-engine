#!/bin/bash
# Гейт сохранений на Маке (пакет «Сохранения»): формат DATA.BIN и перенос прогресса через запись настоящими модулями движка.
# Сначала КРАСНЫЙ (обязан упасть): сборка без сверки CRC (-DSV_BROKEN_CRC) на испорченных записях; «сборка без записи»
# (--nowrite) — прогресс после перезапуска слетает; правка 334: прежнее поведение разовых предметов (-DSP_ONCE_OLD=1) и хвост
# записи без разовых (-DSV_BROKEN_ONCE) — ящик возвращается. Потом ЗЕЛЁНЫЙ. Плюс сверка порядка строк tools/save_export.py и savemenu.c.
#   tests/savegame/host.sh      -> первая строка итог; build/savegame/host_*.txt
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/savegame; mkdir -p "$OUT"
for f in build/level.shr build/homer.shr build/missions/level01.msn build/collect-l1.shr; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
. .venv/bin/activate
# порядок строк пакета (экран strings) = enum SVS_/SVM_MSG_* и ключи msgKey в viewer/savemenu.c
python3 tools/save_export.py --strings > "$OUT/strings_export.txt" || { echo "FAIL: save_export --strings"; exit 1; }
python3 - "$OUT/strings_export.txt" <<'EOF' > "$OUT/strings_check.txt"; SRC=$?
import re, sys
exp = open(sys.argv[1]).read().split()
c = open('viewer/savemenu.c').read()
keys = re.findall(r'"([A-Z_0-9()]+)"', c[c.index('msgKey[SVM_MSG_COUNT]'):c.index('#define ROW_MSG')])
svs = int(re.search(r'SVS_MSG = (\d+)', c).group(1)); lv1 = int(re.search(r'SVS_LEVEL1 = (\d+)', c).group(1))
ok = exp[:2] == ['EMPTY_SLOT', '#527255'] and exp[lv1:lv1 + 7] == ['LEVEL_%d' % i for i in range(1, 8)] and exp[svs:] == keys
print('strings', 'OK' if ok else 'FAIL', len(exp), 'строк; сообщений', len(keys))
sys.exit(0 if ok else 1)
EOF
V=viewer
. tools/bots/lib.sh; LIB="$BOTS_LIB $V/savegame.c"   # общий список исходников мира (tools/bots/lib.sh)
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots"
$CC tests/savegame/host.c $LIB -lm -o "$OUT/host" 2> "$OUT/build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/build.txt"; exit 1; }
$CC -DSV_BROKEN_CRC tests/savegame/host.c $LIB -lm -o "$OUT/host_nocrc" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного"; exit 1; }
$CC -DSV_BROKEN_OWNED tests/savegame/host.c $LIB -lm -o "$OUT/host_noowned" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного 3"; exit 1; }
# правка 334: красный 4 — прежнее поведение разовых предметов (фоновый возврат, в запись не идут); красный 5 — хвост без разовых
$CC -DSP_ONCE_OLD=1 tests/savegame/host.c $LIB -lm -o "$OUT/host_onceold" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного 4"; exit 1; }
$CC -DSV_BROKEN_ONCE tests/savegame/host.c $LIB -lm -o "$OUT/host_nooncetail" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного 5"; exit 1; }
$CC -DSV_BROKEN_CARHP tests/savegame/host.c $LIB -lm -o "$OUT/host_nocarhp" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного 6"; exit 1; }   # сверка g10-flow: прочность машин не пишется
$CC -DGAME_RESUME33_OLD=1 tests/savegame/host.c $LIB -lm -o "$OUT/host_resumeold" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного 7"; exit 1; }   # сверка g07 P14:16: прежнее — загрузка с этапа из записи
ARGS="build/level.shr build/homer.shr build/missions/level01.msn"
red=0
"$OUT/host_nocrc" $ARGS > "$OUT/host_red_nocrc.txt" 2>&1
if [ $? -ne 0 ] && grep -q "format.bitflips.*FAIL" "$OUT/host_red_nocrc.txt"; then red=$((red + 1))
  echo "красный 1 (без сверки CRC): упал, как должен — $(grep -m1 'format.bitflips' "$OUT/host_red_nocrc.txt" | cut -c1-120)"
else echo "КРАСНЫЙ 1 НЕ ПОЙМАН: сборка без CRC прошла проверку порчи"; fi
"$OUT/host" $ARGS --nowrite > "$OUT/host_red_nowrite.txt" 2>&1
if [ $? -ne 0 ] && grep -q "game.progress_same.*FAIL" "$OUT/host_red_nowrite.txt"; then red=$((red + 1))
  echo "красный 2 (без записи): упал, как должен — $(grep -m1 'game.progress_same' "$OUT/host_red_nowrite.txt" | cut -c1-120)"
else echo "КРАСНЫЙ 2 НЕ ПОЙМАН: без записи прогресс «сохранился»"; fi
"$OUT/host_noowned" $ARGS > "$OUT/host_red_noowned.txt" 2>&1   # v2: купленное не пишется в запись — после загрузки Plow King не наш
if [ $? -ne 0 ] && grep -q "game.owned_plowk.*FAIL" "$OUT/host_red_noowned.txt"; then red=$((red + 1))
  echo "красный 3 (без купленного в записи): упал, как должен — $(grep -m1 'game.owned_plowk' "$OUT/host_red_noowned.txt" | cut -c1-120)"
else echo "КРАСНЫЙ 3 НЕ ПОЙМАН: купленное «сохранилось» без записи"; fi
"$OUT/host_onceold" $ARGS > "$OUT/host_red_onceold.txt" 2>&1   # правка 334: прежнее — ящик возвращается вне вида и после загрузки
if [ $? -ne 0 ] && grep -q "game.once_in_game.*FAIL" "$OUT/host_red_onceold.txt" && grep -q "game.once_after_load.*FAIL" "$OUT/host_red_onceold.txt"; then red=$((red + 1))
  echo "красный 4 (прежние разовые): упал, как должен — $(grep -m1 'game.once_in_game' "$OUT/host_red_onceold.txt" | cut -c1-150)"
else echo "КРАСНЫЙ 4 НЕ ПОЙМАН: ящик не вернулся при прежнем поведении"; fi
"$OUT/host_nooncetail" $ARGS > "$OUT/host_red_nooncetail.txt" 2>&1   # правка 334: хвост без разовых — после загрузки ящик снова цел
if [ $? -ne 0 ] && grep -q "game.once_after_load.*FAIL" "$OUT/host_red_nooncetail.txt"; then red=$((red + 1))
  echo "красный 5 (разовые не пишутся в хвост): упал, как должен — $(grep -m1 'game.once_after_load' "$OUT/host_red_nooncetail.txt" | cut -c1-150)"
else echo "КРАСНЫЙ 5 НЕ ПОЙМАН: разбитый ящик «сохранился» без записи"; fi
"$OUT/host_nocarhp" $ARGS > "$OUT/host_red_nocarhp.txt" 2>&1   # сверка g10-flow P01 Д1: прочность машин не переживает запись
if [ $? -ne 0 ] && grep -q "carhp.roundtrip.*FAIL" "$OUT/host_red_nocarhp.txt"; then red=$((red + 1))
  echo "красный 6 (прочность машин не пишется): упал, как должен — $(grep -m1 'carhp.roundtrip' "$OUT/host_red_nocarhp.txt" | cut -c1-120)"
else echo "КРАСНЫЙ 6 НЕ ПОЙМАН: прочность машин «сохранилась» без записи"; fi
"$OUT/host_resumeold" $ARGS > "$OUT/host_red_resumeold.txt" 2>&1   # сверка g07 P14:16: загрузка с чекпоинта записи — не как в оригинале
if [ $? -ne 0 ] && grep -q "resume.checkpoint.*FAIL" "$OUT/host_red_resumeold.txt"; then red=$((red + 1))
  echo "красный 7 (загрузка с этапа записи): упал, как должен — $(grep -m1 'resume.checkpoint' "$OUT/host_red_resumeold.txt" | cut -c1-120)"
else echo "КРАСНЫЙ 7 НЕ ПОЙМАН: загрузка с этапа записи прошла проверку «с начала миссии»"; fi
"$OUT/host" $ARGS > "$OUT/host_green.txt" 2>&1; rc=$?
grep "^CHECK\|^  до\|^  после\|^RESULT" "$OUT/host_green.txt"; cat "$OUT/strings_check.txt"
if [ $red -eq 7 ] && [ $rc -eq 0 ] && [ $SRC -eq 0 ]; then echo "PASS: красный 7/7 пойман, зелёный $(grep -m1 RESULT "$OUT/host_green.txt")"; exit 0; fi
echo "FAIL: красный $red/7, зелёный rc=$rc, строки rc=$SRC"; exit 1
