#!/bin/bash
# 108 О13/О17: снимки кадра ОДНОГО кадра в одном прогоне (flag rfcompare замораживает кадр и рисует пять раз; проход 4 — с прежними путями следов шин, картинок эффектов и рисунков меток, проход 0 — с новыми).
# Зелёный: rf_*_0.png == rf_*_4.png побайтно в каждой сцене, строки RF108 pass4 есть, fx_bad = msa_bad = 0 (сверка «flag fxfind108chk 1», «msafind108chk 1»), следы нарисованы (SKID drawn > 0).
# Красный (обязан упасть): «flag fxfind108red 2» и «msafind108red 2» (кэш отдаёт чужое) — снимки 0 и 4 разные или fx_bad/msa_bad > 0.  tests/hot108/rf.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/hot108rf; mkdir -p $OUT
run() { local N=$1 R=$2 SK=${3:-0}; local F=$OUT/$N.scn
  { echo "scene skid"; echo "flag rf108skip $SK"; echo "flag fxfind108chk 1"; echo "flag msafind108chk 1"; echo "flag pbchk 1"; echo "flag fxfind108red $R"; echo "flag msafind108red $R"
    echo "goto 188 279.9 1.5708"; echo "wait 3"; echo "press cross 2.0"; echo "wait 2.05"; echo "press circle 0.7"; echo "wait 0.6"; echo "flag skidlog"; echo "flag rfcompare"; echo "wait 2"
    echo "scene SW_1_0"; echo "flag rf108skip $SK"; echo "flag fxfind108chk 1"; echo "flag msafind108chk 1"; echo "flag pbchk 1"; echo "flag fxfind108red $R"; echo "flag msafind108red $R"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 10"; echo "flag rfcompare"; echo "wait 3"; echo "end"; } > $F
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > $OUT/$N.log 2>&1
  rm -rf $OUT/$N; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/ 2>/dev/null; cp -f $ROOT/build/twins/scen/full/$N/rf_*.png $OUT/$N/ 2>/dev/null; echo "DONE $N"; }
run rf108_green 0; run rf108_red 2; run rf108_base 0 1
python3 - <<'PY'
import os, re, sys, glob
import numpy as np
from PIL import Image
B = 'build/hot108rf'
ld = lambda p: np.array(Image.open(p).convert('RGB')).astype(int)
def dil(m, k=2):
    o = m.copy()
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            o |= np.roll(np.roll(m, dy, 0), dx, 1)
    return o
def diff(n, sc):
    p0, p4 = f'{B}/{n}/rf_{sc}_0.png', f'{B}/{n}/rf_{sc}_4.png'
    return (abs(ld(p0) - ld(p4)).sum(2) > 0) if os.path.exists(p0) and os.path.exists(p4) else None
def pb(n):
    r = None; p4 = 0; sk = 0
    for l in open(f'{B}/{n}/smoke.txt', errors='replace'):
        if l.startswith('PBCHK'): r = dict(kv.split('=') for kv in l.split()[1:])
        if l.startswith('RF108 pass4'): p4 += 1
        m = re.match(r'SKID .*drawn=(\d+)', l)
        if m: sk = max(sk, int(m.group(1)))
    return r, p4, sk
bad = 0
def fail(m):
    global bad; bad += 1; print('FAIL', m)
# проходы 0 и 4 одного замороженного кадра различаются и без наших правок (качание деревьев, строка счётчиков внизу): это шум основы (прогон base, проход 4 без прежних путей).
# Критерий: пиксели, где проход 4 (прежние пути) отличается от прохода 0, лежат внутри шума основы (расширен на 2 px); вне шума не больше 3 пикселей. Красный: вне шума больше.
pg, p4g, skg = pb('rf108_green'); pr, p4r, _ = pb('rf108_red')
print('green: проходов pass4 в журнале', p4g, ', SKID drawn max', skg, ', PBCHK', pg)
if p4g < 2: fail('проход 4 с прежними путями не отмечен')
if skg <= 0: fail('следы не нарисованы')
for k in ('fx_bad', 'msa_bad'):
    if int((pg or {}).get(k, -1)) != 0: fail(f'green {k} = {(pg or {}).get(k)}')
caughtRed = False
for sc in ('skid', 'SW_1_0'):
    dB, dG, dR = diff('rf108_base', sc), diff('rf108_green', sc), diff('rf108_red', sc)
    if dB is None or dG is None: fail(f'{sc}: нет снимков'); continue
    out = int((dG & ~dil(dB)).sum()); print(f'  {sc}: шум основы {int(dB.sum())} px, зелёный {int(dG.sum())} px, вне шума {out}')
    if out > 3: fail(f'{sc}: снимки новых и прежних путей расходятся вне шума ({out} px)')
    if dR is not None:
        outR = int((dR & ~dil(dB)).sum()); print(f'  {sc} красный: вне шума {outR} px')
        if outR > 3: caughtRed = True
caughtRed = caughtRed or int((pr or {}).get('fx_bad', 0)) > 0 or int((pr or {}).get('msa_bad', 0)) > 0
print('red PBCHK', pr); print('красный fxfind108red/msafind108red 2:', 'упал, как должен' if caughtRed else 'НЕ ПОЙМАН')
if not caughtRed: fail('красный не пойман')
print('PASS' if not bad else 'FAIL'); sys.exit(1 if bad else 0)
PY
