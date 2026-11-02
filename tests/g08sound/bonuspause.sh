#!/bin/bash
# №1144: пауза бонус-игры. Красный: BONUSPAUSEMUS1144_OLD=1 (музыка играет в паузе)
set -u; cd "$(dirname "$0")/../.."; export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"; O=build/g08sound; B=${B:-build/snd6/bonus/}
cc -std=c11 -O1 -w -DAUDIO_HOST -I viewer tests/g08sound/bonuspause.c viewer/audio.c -lm -o $O/bp_g || exit 2
cc -std=c11 -O1 -w -DAUDIO_HOST -DBONUSPAUSEMUS1144_OLD=1 -I viewer tests/g08sound/bonuspause.c viewer/audio.c -lm -o $O/bp_r || exit 2
$O/bp_g $B > $O/bp_g.txt || { grep FAIL $O/bp_g.txt; echo "FAIL зелёный"; exit 1; }
if $O/bp_r $B > $O/bp_r.txt; then echo "FAIL красный не пойман"; exit 1; else echo "красный BONUSPAUSEMUS1144_OLD=1: $(grep -m1 '^FAIL' $O/bp_r.txt)"; fi
echo "PASS: №1144 пауза бонус-игры"
