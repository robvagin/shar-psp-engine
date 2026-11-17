#!/bin/bash
# 109 (а) по списку Запекания (msnres-l<N>.shr): карточка миссии заказывает ВСЕ записи миссии у msnart в начале и открывается, когда они прочитаны;
# после неё чтений записей этой миссии нет. Стенд: L1, миссия m3 (flag msnmainat 300). Список собирается из набора (tools/missions/msnres.py) в build/msnres109/.
# Зелёный (loadlist109old 0): каждая запись миссии (art/картинка, не «движок», не «нет в наборе») прочитана до строки «MSN briefing-loaded» и ни одной после неё до следующей миссии.
# Красный (loadlist109old 1): часть записей миссии к карточке не прочитана (просятся позже, по ходу этапов) или читается после неё.   tests/load109/list.sh EBOOT [ГОТОВЫЙ_msnres-l1.shr]
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; LST=${2:-build/msnres109/msnres-l1.shr}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag loadlist109old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 300"; echo "wait 24"; echo "flag msnmainat 400"; echo "wait 14"; echo "end"; } > $OUT/$N.scn
  SCEN_OVERRIDE="msnres-l1.shr=$LST" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run list109_new 0; run list109_old 1
LST=$LST python3 - <<'PY'
import os, re, struct
f = open(os.environ['LST'], 'rb').read(); h = struct.unpack_from('<4sIIIIIII', f, 0); nm, off = h[3], h[5]
def keys(mid):
    for i in range(nm):
        e = f[32 + i * 40: 32 + i * 40 + 40]
        if e[:8].split(b'\0')[0].decode() != mid: continue
        first, n = struct.unpack_from('<II', e, 8); out = []
        for k in range(n):
            r = f[off + (first + k) * 96: off + (first + k + 1) * 96]
            if r[0] in (1, 4) and not (r[3] & 3): out.append(r[8:].split(b'\0')[0].decode())
        return out
    return []
def scan(n, mid):
    L = open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines(); ks = set(keys(mid))
    s = next((i for i, l in enumerate(L) if l.startswith('MSN mission %s ' % mid)), -1)
    if s < 0: return None
    e = next((i for i in range(s + 1, len(L)) if L[i].startswith('MSN mission ')), len(L))
    b = next((i for i in range(s, e) if L[i].startswith('MSN briefing-loaded')), -1)
    req = lambda lo, hi: [m.group(1) for l in L[lo:hi] if (m := re.match(r'MSNREQ art (\S+)', l)) and m.group(1) in ks]
    before = set(req(s, b if b >= 0 else e)); after = req(b if b >= 0 else e, e); early = set(m.group(1) for l in L[:s] if (m := re.match(r'MSNREQ art (\S+)', l)))
    miss = sorted(k for k in ks if k not in before and k not in early)
    ordered = [l for l in L[s:s + 40] if l.startswith('MSN reslist %s ' % mid)]
    return len(ks), len(before), after, miss, ordered[:1], b
bad = 0
for n, want in (('list109_new', 1), ('list109_old', 0)):
    r = scan(n, 'm3')
    if r is None: print(n, 'FAIL: нет миссии m3'); bad = 1; continue
    nk, nb, after, miss, ordered, b = r
    print('%s: записей в списке %d, прочитано до карточки %d, после карточки %d %s, не прочитано вовсе %s %s' % (n, nk, nb, len(after), sorted(set(after))[:3], miss[:3], ordered))
    if want and (b < 0 or after or miss or not ordered): print('FAIL зелёный'); bad = 1
    if not want and not after and not miss: print('КРАСНЫЙ НЕ ПОЙМАН: на старом пути всё прочитано до карточки'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
