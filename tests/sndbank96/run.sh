#!/bin/bash
# Гейт Звука-96 (банк sfx.sab по требованию): tests/sndbank96/run.sh [каталог_audio ...]
#  зелёный: каждый файл банка звучит бит в бит как при полном банке (3 круга случайно с вытеснениями), данные живых голосов на месте при 40 голосах,
#           банк в памяти меньше полного не менее чем на 0,9 МБ, вытеснения были; красный контроль: «break» (загрузка отключена) обязан дать расхождения (код 1)
#  Без аргументов: наборы уровней из tests/sndbank96/packs.txt или SB_PACKS="путь1 путь2"
set -u; H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
make -C "$W/tests/audio/host" >/dev/null 2>&1 || { echo "сборка audio_host не вышла"; exit 2; }
PACKS=${*:-${SB_PACKS:-$(for l in 2 3 4 5 6 7; do echo $HOME/Downloads/shar-psp-integ/build/lo_set/l$l/audio/; done)}}
RC=0
for P in $PACKS; do
  OUT=$("$W/tests/audio/host/audio_host" sbtest "$P" 2>&1); G=$?
  "$W/tests/audio/host/audio_host" sbtest "$P" break >/dev/null 2>&1; B=$?
  echo "$P  зелёный rc=$G  красный контроль rc=$B"; echo "$OUT" | grep -E 'банк в памяти|нагрузка|FAIL|SNDBANK96'
  [ $G = 0 ] && [ $B != 0 ] || RC=1
done
[ $RC = 0 ] && echo "SNDBANK96 GATE OK" || echo "SNDBANK96 GATE FAIL"; exit $RC
