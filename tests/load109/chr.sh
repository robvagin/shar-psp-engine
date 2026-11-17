#!/bin/bash
# 109 / правка Роба 1293: последняя миссия L1 (m7): Бёрнс (и Смитерс, Карл) не виден, пока не заговоришь — модель читалась по ходу этапов.
# Теперь карточка заказывает модели ВСЕХ персонажей миссии из списка msnres (kind CHR) и открывается, когда они в памяти.
# Зелёный (loadchr109old 0): каждая модель персонажа m7 прочитана (MSNNPC load) до строки «MSN briefing-loaded», после неё чтений персонажей m7 (load / prefetch / lookahead) нет.
# Красный (loadchr109old 1): burns читается после карточки.   tests/load109/chr.sh EBOOT [msnres-l1.shr]
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; LST=${2:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15/msnres-l1.shr}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag loadchr109old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 700"; echo "wait 40"; echo "end"; } > $OUT/$N.scn
  SCEN_OVERRIDE="msnres-l1.shr=$LST" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
[ -n "${CHR_SKIP:-}" ] || { run chr109_new 0; run chr109_old 1; }
LST=$LST python3 - <<'PY'
import os, re, struct
f = open(os.environ['LST'], 'rb').read(); h = struct.unpack_from('<4sIIIIIII', f, 0); nm, off = h[3], h[5]
names = []
for i in range(nm):
    e = f[32 + i * 40: 72 + i * 40]
    if e[:8].split(b'\0')[0].decode() != 'm7': continue
    first, n = struct.unpack_from('<II', e, 8)
    for k in range(n):
        r = f[off + (first + k) * 96: off + (first + k + 1) * 96]
        if r[0] == 3 and not (r[3] & 3): names.append(r[8:].split(b'\0')[0].decode())
print('персонажи m7 по списку:', names)
def scan(n):
    L = open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines()
    s = next((i for i, l in enumerate(L) if l.startswith('MSN mission m7 ')), -1)
    if s < 0: return None
    e = next((i for i in range(s + 1, len(L)) if L[i].startswith('MSN mission ')), len(L))
    b = next((i for i in range(s, e) if L[i].startswith('MSN briefing-loaded')), -1)
    ld = lambda lo, hi, nm_: any(re.match(r'MSNNPC load %s ' % nm_, l) for l in L[lo:hi])
    pend = lambda lo, hi: [m.group(1) for l in L[lo:hi] if (m := re.match(r'MSNNPC (?:load|prefetch|lookahead) (\S+)', l)) and m.group(1) in names]
    pre = set(m.group(1) for l in L[:s] if (m := re.match(r'MSNNPC load (\S+)', l)))
    before = [x for x in names if ld(s, b if b >= 0 else e, x) or x in pre]; after = pend(b if b >= 0 else e, e)
    return b, before, after
bad = 0
for n, want in (('chr109_new', 1), ('chr109_old', 0)):
    r = scan(n)
    if r is None: print(n, 'FAIL: нет миссии m7'); bad = 1; continue
    b, before, after = r
    print('%s: карточка %s, моделей до неё %d из %d, чтений персонажей после неё %d %s' % (n, 'есть' if b >= 0 else 'НЕТ', len(before), len(names), len(after), sorted(set(after))))
    if want and (b < 0 or len(before) != len(names) or after): print('FAIL зелёный'); bad = 1
    if not want and not after and len(before) == len(names): print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
