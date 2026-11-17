#!/bin/bash
# 109 (Fable-2 по LOADLIST109): мест под записи msnart 32, худшая миссия по спискам занимает 23; при переполнении вытеснялась самая ранняя запись миссии (значок первого этапа).
# Статика: по msnres-l1..l7 самая тяжёлая миссия (свои art+картинка + уровневые art) не больше 28 из 32 (запас 4). Эмулятор L1 m2 (17+6): новое — вытеснений (BBOP art-) 0;
# красный (flag msnartcap109 20, тесное место) — вытеснения есть.   tests/load109/cap.sh EBOOT [msnres-dir]   (dir: msnres-l1.shr в корне, l<N>/msnres-l<N>.shr)
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; DIR=${2:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
LST=$DIR/msnres-l1.shr; [ -f $LST ] || LST=$DIR/msnres-l1.shr
run() { local N=$1 C=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag msnartcap109 $C"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 200"; echo "wait 24"; echo "end"; } > $OUT/$N.scn
  SCEN_OVERRIDE="msnres-l1.shr=$LST" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
[ -n "${CAP_SKIP:-}" ] || { run cap109_new 0; run cap109_red 20; }
DIR=$DIR python3 - <<'PY'
import os, struct, re
mx = (0, '')
for L in range(1, 8):
    p = os.environ['DIR'] + ('/msnres-l1.shr' if L == 1 else f'/l{L}/msnres-l{L}.shr'); f = open(p, 'rb').read(); h = struct.unpack_from('<4sIIIIIII', f, 0); nm, off = h[3], h[5]; ms = {}
    for i in range(nm):
        e = f[32 + i * 40: 72 + i * 40]; first, n = struct.unpack_from('<II', e, 8); ms[e[:8].split(b'\0')[0].decode()] = [f[off + (first + k) * 96: off + (first + k + 1) * 96] for k in range(n)]
    lv = set(r[8:].split(b'\0')[0] for r in ms['L'] if r[0] == 1)
    for mid, rs in ms.items():
        if mid == 'L': continue
        m = set(r[8:].split(b'\0')[0] for r in rs if r[0] in (1, 4) and not (r[3] & 3)); t = len(m | lv)
        if t > mx[0]: mx = (t, f'L{L} {mid}')
print('статика: самая тяжёлая миссия', mx, 'из 32')
bad = 0
if mx[0] > 28: print('FAIL статика'); bad = 1
def ev(n): return sum(1 for l in open(f'build/load109/{n}/smoke.txt', errors='replace') if l.startswith('BBOP art- '))
def got(n): return any(l.startswith('MSN briefing-loaded') for l in open(f'build/load109/{n}/smoke.txt', errors='replace'))
a, b = ev('cap109_new'), ev('cap109_red'); print('вытеснений: новое', a, 'красный (предел 20)', b, '| карточка вышла: ', got('cap109_new'), got('cap109_red'))
if a != 0 or not got('cap109_new'): print('FAIL зелёный'); bad = 1
if b < 1: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
