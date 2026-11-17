#!/bin/bash
# Гейт 1264д (Бабл, 107): перезапуск основной части идёт на этап RESET_TO_HERE, даже если до него не дошли. tests/resetpt1264/gate.sh EBOOT [уровень миссия предпоследний-этап-вступления метка]
#   по умолчанию L1 m3 (метка на этапе 2 основной части). Сценарий: вступление с последнего этапа → msnpass (вход в основную часть) → flag msnretry (путь Restart/Retry).
#   Ждём: новое — после retry идёт этап mM.1.<метка>; красный контроль (flag resetpt1264old 1) — mM.1.0.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; L=${2:-1}; M=${3:-3}; LASTI=${4:-9}; CP=${5:-2}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
MN=${MNAME:-$M}   # номер миссии в журнале (на L2 индекс в списке на 1 меньше)
ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/rp1264; ok=1
for OLD in 0 1; do
  N=rp1264_${OLD}_L${L}_m${M}; F=build/rp1264/$N.scn
  { echo "scene SW_${L}_0"; echo "flag camdet 1"; [ "$L" != 1 ] && echo "flag golevel $L"; [ "$OLD" = 1 ] && echo "flag resetpt1264old 1"
    echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat -$((M*100+LASTI))"; echo "wait 3"; echo "press cross"; echo "wait 2"; echo "press cross"; echo "wait 4"
    echo "flag msnpass"; echo "wait ${W:-0.01}"; echo "flag msnretry"; echo "wait 8"; echo "end"; } > $F
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/rp1264/$N.log 2>&1
  cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/rp1264/$N.txt
  # этап, на который встали сразу после retry = последняя строка этапа основной части перед строкой "pause-restart" (она пишется после перезапуска)
  last=$(awk -v m=$MN '/^MSN stage m/{ if ($3 ~ "^m"m"\\.1\\.") st=$3 } /^MSN pause-restart/{print st; exit}' build/rp1264/$N.txt | sed -E 's/.*\.1\.([0-9]+)/\1/'); first=$(grep -E '^MSN stage m'$MN'\.1\.[0-9]+ ' build/rp1264/$N.txt | head -1 | sed -E 's/^MSN stage m[0-9]+\.1\.([0-9]+) .*/\1/')
  echo "OLD=$OLD: первый этап основной части $first, после retry $last (ждём новое $CP, старое 0)"
  if [ "$OLD" = 0 ]; then [ "$last" = "$CP" ] || ok=0; else [ "$last" = "0" ] || ok=0; fi
done
[ $ok = 1 ] && echo "ГЕЙТ ЗЕЛЁНЫЙ" || { echo "ГЕЙТ КРАСНЫЙ"; exit 1; }
