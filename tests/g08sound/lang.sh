#!/bin/bash
# №1111: озвучка по языку. Пакет английского уровня 1 (build/snd6/l1) + dialog_<язык>.sap (tools/sound_export.py --lang, build/lang6/l1). Красные: LANGVO1111_OLD=1 (язык не применяется),
# повреждённый файл языка, язык без файла. Мак, CPU.
set -u; cd "$(dirname "$0")/../.."; export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"; O=build/g08sound; mkdir -p $O; bad=0
PY=${PY:-.venv/bin/python}; E=${E:-build/snd6/l1}; L=${L:-build/lang6/l1}; T=$O/langpack; rm -rf $T; mkdir -p $T
for f in sfx.sab music.sap; do ln -s "$PWD/$E/$f" $T/$f; done
ln -s "$PWD/$L/dialog_fr.sap" $T/dialog_fr.sap; cp "$L/dialog_ge.sap" $T/dialog_ge.sap; printf 'X' | dd of=$T/dialog_ge.sap bs=1 count=1 conv=notrunc 2>/dev/null   # немецкий файл испорчен, испанского нет
cc -std=c11 -O1 -w -DAUDIO_HOST -I viewer tests/g08sound/lang.c -lm -o $O/lang_g || exit 2
cc -std=c11 -O1 -w -DAUDIO_HOST -DLANGVO1111_OLD=1 -I viewer tests/g08sound/lang.c -lm -o $O/lang_old || exit 2
first=$($O/lang_g $T/ 1 | sed -n 's/.*FIRST=\([^ ]*\).*/\1/p'); [ -n "$first" ] && [ "$first" != "-" ] || { echo "FAIL: нет дорожки языка"; exit 1; }
$PY - "$first" > $O/lang_expect.bin <<'PYEOF'
import sys, os
sys.path.insert(0, 'tools'); import audio_export as AE
name = sys.argv[1]; idx = AE.lang_index('fr')
p = next(v for k, v in idx.items() if os.path.basename(k)[:-4][:27] == name)
rs = AE.read_rsd(p); d = rs['data']
if rs['ch'] == 1: d = b''.join(d[i:i + 16] * 2 for i in range(0, len(d), 16))
sys.stdout.buffer.write(d[:2048])
PYEOF
echo "дорожка языка: $first"
$O/lang_g $T/ 1 $O/lang_expect.bin > $O/lang_green.txt && grep -E "^(OK|FAIL)" $O/lang_green.txt | sed 's/^/французский: /' || { grep FAIL $O/lang_green.txt; bad=1; }
$O/lang_g $T/ 0 > $O/lang_en.txt && echo "English: $(grep -c '^OK' $O/lang_en.txt) проверка OK (дорожек языка нет)" || { cat $O/lang_en.txt; bad=1; }
$O/lang_g $T/ 2 none > $O/lang_ge.txt && echo "немецкий с повреждённым файлом: английская озвучка, дорожек языка нет" || { cat $O/lang_ge.txt; bad=1; }
$O/lang_g $T/ 3 none > $O/lang_sp.txt && echo "испанский без файла: английская озвучка" || { cat $O/lang_sp.txt; bad=1; }
if $O/lang_old $T/ 1 > $O/lang_red.txt; then echo "OK красный LANGVO1111_OLD=1: без перевода"; else echo "FAIL красный LANGVO1111_OLD=1 не сработал"; bad=1; fi
[ $bad = 0 ] && echo "PASS: №1111 озвучка по языку" || { echo FAIL; exit 1; }
