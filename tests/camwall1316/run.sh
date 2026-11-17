#!/bin/bash
# 108 1316 К6: полосы разговора и дуга камеры сходят за срок независимо от длинного кадра. Стенд: каждый кадр +120 мс (flag stall1316 120, потолок dt 0,05 с), полосы держатся (flag lbforce1316), потом отпускаются; дуга камеры 1 с (flag camarc1316).
# Зелёный: полосы закрылись ≤ 0,9 с после отпускания (LB_SLIDE 0,5 с + кадр), дуга закончилась ≤ 1,5 с. Красный (flag camwall1316 0): полосы ≥ 1,0 с и дуга ≥ 2,0 с (счёт по dt с потолком). tests/camwall1316/run.sh EBOOT [SET]
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/camwall1316}; mkdir -p "$OUT"; rm -rf "$OUT"/out_wn "$OUT"/out_wo
for v in "wn:1" "wo:0"; do n=${v%%:*}; sed "s/FLAGVAL/${v##*:}/" tests/camwall1316/scene.txt > "$OUT/$n.txt"; FLAGS="" nice -n 20 tools/emu_run.sh "$OUT/$n.txt" $n "$EB" 240 > "$OUT/$n.log" 2>&1; done
python3 -I - "$OUT" <<'PY'
import re, sys
out = sys.argv[1]; bad = 0
def an(n):
    L = open(f'{out}/out_{n}/smoke.txt', errors='replace').read().splitlines()
    rel = next((float(re.search(r'w=([\d.]+)', l).group(1)) for l in L if l.startswith('LBREL1316')), None)
    lb = next((float(re.search(r'w=([\d.]+)', l).group(1)) for l in L if l.startswith('LBCLOSE1316')), None)
    a0 = next((float(re.search(r'w=([\d.]+)', l).group(1)) for l in L if l.startswith('CAMARC1316 start')), None)
    a1 = next((float(re.search(r'w=([\d.]+)', l).group(1)) for l in L if l.startswith('CAMARC1316 done')), None)
    return rel, lb, a0, a1
for n, want in (('wn', 1), ('wo', 0)):
    rel, lb, a0, a1 = an(n)
    if None in (rel, lb, a0, a1): print(n, 'FAIL: нет строк', rel, lb, a0, a1); bad = 1; continue
    d1, d2 = lb - rel, a1 - a0
    print('%s: полосы %.2f с после отпускания, дуга %.2f с' % (n, d1, d2))
    if want and not (d1 <= 0.9 and d2 <= 1.5): print('FAIL зелёный'); bad = 1
    if not want and not (d1 >= 1.0 and d2 >= 2.0): print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); sys.exit(bad)
PY
