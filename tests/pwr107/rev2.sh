#!/bin/bash
# 107в (ревью Fable, часть 2): три пункта. EBOOT_NEW — с правками, EBOOT_OLD — до них (сборка bubl109 4d7dd839+109, те же ошибки).
#  1) отмена чтения гэга без предела: «flag collstall107 1» (чтение висит), «flag collcancelhang107 1» (отмена не возвращается), сон «flag simsleep 1» — новое: строка COLLCANCEL timeout (дескриптор брошен за 200 мс), игра идёт дальше (кадры после сна есть).
#  2) msnart: первый кадр после СМЕНЫ уровня (flag golevel 2) не закрывает здоровый msnart.shr: строк «BBOP art-pwr» без сна нет (новое 0, прежнее ≥ 1).
#  3) гэги: умер ТОЛЬКО фоновый дескриптор («flag collkillasync107») — повторное открытие за 1 с: COLLREOPEN есть (новое), нет (прежнее «&&»).
#   tests/pwr107/rev2.sh EBOOT_NEW EBOOT_OLD
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
NEW=${1:?EBOOT_NEW}; OLD=${2:?EBOOT_OLD}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/pwr107r; mkdir -p $OUT
go() { local N=$1 EB=$2; SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
printf 'scene gag\nwait 3\nfoot 407.99 -413.09 0 4.6\nwait 3\nflag collstall107 1\nflag collcancelhang107 1\nfoot 228.567 201.097 0 5\nwait 4\nflag simsleep 1\nwait 8\nend\n' > $OUT/c1.scn; cp $OUT/c1.scn $OUT/c1new.scn; go c1new $NEW
printf 'scene SW_1_0\nwait 20\nflag player 1\nflag msn\nwait 8\nflag golevel 2\nwait 25\nflag player 1\nflag msn\nwait 6\nend\n' > $OUT/c2new.scn; go c2new $NEW; cp $OUT/c2new.scn $OUT/c2old.scn; go c2old $OLD
printf 'scene gag\nwait 3\nflag collkillasync107\nfoot 407.99 -413.09 0 4.6\nwait 2\nfoot 228.567 201.097 0 5\nwait 6\nend\n' > $OUT/c3new.scn; go c3new $NEW; cp $OUT/c3new.scn $OUT/c3old.scn; go c3old $OLD
python3 - <<'PY'
import re
def rd(n): return open(f'build/pwr107r/{n}/smoke.txt', errors='replace').read().splitlines()
bad = 0
L = rd('c1new'); sl = [i for i, l in enumerate(L) if l.startswith('PWR sim sleep')]; to = [i for i, l in enumerate(L) if l.startswith('COLLCANCEL timeout')]
after = sum(1 for l in L[sl[0]:] if l.startswith('GAGSTREAM') or l.startswith('BBOP') or l.startswith('BB ')) if sl else 0
print('1) отмена: сон на строке', sl[:1], 'COLLCANCEL timeout', len(to), 'строк игры после сна', after)
if not sl or not to or after < 5: print('FAIL 1'); bad = 1
a = sum(1 for l in rd('c2new') if l.startswith('BBOP art-pwr')); b = sum(1 for l in rd('c2old') if l.startswith('BBOP art-pwr'))
print('2) art-pwr без сна: новое', a, 'прежнее', b)
if a != 0 or b < 1: print('FAIL 2'); bad = 1
a = sum(1 for l in rd('c3new') if l.startswith('COLLREOPEN')); b = sum(1 for l in rd('c3old') if l.startswith('COLLREOPEN'))
print('3) умер только фоновый: COLLREOPEN новое', a, 'прежнее', b)
if a < 1 or b != 0: print('FAIL 3'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
