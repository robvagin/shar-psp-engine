#!/bin/bash
# Аудит полноты п.10 (g04-homer): «10 секунд» — реплика один раз при пересечении 10 с вниз. Красный: MSN_TEN_RED=1 (-DMSN_AUDIO_SV_OLD=1)
set -eu; cd "$(dirname "$0")/../.."; D="${TMPDIR:-/tmp}/msnten.$$"; mkdir -p "$D"
RED=""; [ "${MSN_TEN_RED:-0}" = 1 ] && RED="-DMSN_AUDIO_SV_OLD=1"
cc -std=c11 -O1 $RED -Iviewer -Iviewer/mission tests/msn_ten/host.c viewer/msn_audio.c viewer/mission/msn_file.c viewer/mission/msn_ops.c viewer/mission/msn_run.c viewer/mission/msn_game.c -lm -Wl,-undefined,dynamic_lookup -o "$D/check"
"$D/check"; r=$?; rm -rf "${D:?}"; exit $r
