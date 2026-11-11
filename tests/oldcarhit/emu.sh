#!/bin/bash
# Гейт правки 1241 в эмуляторе (когда Мак свободен): tests/oldcarhit/emu.sh [EBOOT]. Красный: flag oldcarhitold. Один эмулятор, nice 20.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:-viewer/build_p96/EBOOT.PBP}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
run() { local n=$1 old=$2; sed "s/^flag oldcarram\$/flag oldcarram\n$( [ $old = 1 ] && echo 'flag oldcarhitold' )/" tests/oldcarhit/emu_scenario.txt > build/oldcarhit_$n.txt
  SCEN_EBOOT=$EB nice -n 20 python3 "$S" $n build/oldcarhit_$n.txt >/dev/null 2>&1; f=${SCEN_ROOT:-$HOME/Downloads/shar-psp}/build/twins/scen/full/$n/smoke.txt
  [ -f $f ] && grep -a "^OLDCAR" $f | sort -t= -k2 -n | head -3; python3 - "$f" <<'PY'
import re,sys
t=open(sys.argv[1],errors='replace').read() if len(sys.argv)>1 else ''
d=[float(x) for x in re.findall(r'OLDCARDIST d=([\d.]+)',t)]
print('min d = %.2f' % min(d) if d else 'нет строк OLDCARDIST')
PY
}
echo "красный:"; run oldhit_red 1; echo "зелёный:"; run oldhit_green 0
