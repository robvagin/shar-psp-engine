#!/bin/bash
# Гейт 1264 Б.8 (Бабл, 107): каждый выход из уличной гонки снимает заборы, признак гонки возвращает пешеходов. tests/raceexit1264/gate.sh EBOOT [выходы: switch skip level]
#   гонка L1 sr2 (индекс 9) → ждём race=1 и заборы → выход → в последней строке TRF race=0 fences=0. Красный контроль: flag racexitplant 1 (уборка гонку не снимает) обязан упасть.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; EXITS=${2:-"switch skip level"}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/rx1264; ok=1
for E in $EXITS; do for P in 0 1; do
  N=rx1264_${E}_p$P; F=build/rx1264/$N.scn
  case $E in switch) X="flag msnsel 0";; skip) X="flag msnskip";; level) X="flag golevel 2";; esac
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag trflog"; [ "$P" = 1 ] && echo "flag racexitplant 1"
    echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmain 9"; echo "wait 3"; echo "press cross"; echo "wait 12"
    echo "$X"; echo "wait 12"; echo "end"; } > $F
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/rx1264/$N.log 2>&1
  cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/rx1264/$N.txt
  python3 - "$N" "$E" "$P" <<'PY' || ok=0
import re,sys,pathlib
n,e,p=sys.argv[1:4]; L=[l for l in pathlib.Path('build/rx1264/%s.txt'%n).read_text(errors='replace').splitlines() if l.startswith('TRF t=')]
g=lambda l,k:int(re.search(k+r'=(\d+)',l).group(1))
raced=any(g(l,'race')==1 and g(l,'fences')>0 for l in L); last=L[-1] if L else ''
fin=bool(last) and g(last,'race')==0 and g(last,'fences')==0
print(('новое  ' if p=='0' else 'красный'),e,'гонка была(race=1,заборы>0):',raced,'| после выхода race=%s fences=%s'%(g(last,'race'),g(last,'fences')) if last else '| нет TRF')
sys.exit(0 if (raced and (fin if p=='0' else not fin)) else 1)
PY
done; done
[ $ok = 1 ] && echo "ГЕЙТ ЗЕЛЁНЫЙ" || { echo "ГЕЙТ КРАСНЫЙ"; exit 1; }
