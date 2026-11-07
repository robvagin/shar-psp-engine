#!/bin/bash
# Гейт правки 433: красный (HNRMARKCAP_OLD=1, прежнее правило — погоня в кадре не кончается) обязан упасть, зелёный пройти. Мак, CPU.
cd "$(dirname "$0")/../.."; CC="/Library/Developer/CommandLineTools/usr/bin/clang -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"; cc --version >/dev/null 2>&1 && CC=cc
OUT=build/hnr; mkdir -p "$OUT"
$CC -std=c11 -O1 -I viewer -DHNRMARKCAP_OLD=0 tests/hnr/markcap.c -lm -o "$OUT/markcap_new" && $CC -std=c11 -O1 -I viewer -DHNRMARKCAP_OLD=1 tests/hnr/markcap.c -lm -o "$OUT/markcap_old" || exit 2
"$OUT/markcap_new" > "$OUT/markcap_new.txt"; g=$?; "$OUT/markcap_old" > "$OUT/markcap_old.txt"; o=$?
cat "$OUT/markcap_new.txt"; echo "--- прежнее правило (красный контроль, те же ожидания) ---"; grep "^FAIL" "$OUT/markcap_old.txt" | head -3; red=$(grep -c "^FAIL" "$OUT/markcap_old.txt")
[ $g = 0 ] && [ $o != 0 ] && [ "$red" -ge 3 ] && echo "PASS markcap: зелёный, красный пойман ($red строк)" && exit 0; echo "FAIL markcap g=$g old=$o red=$red"; exit 1
