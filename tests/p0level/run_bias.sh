#!/bin/bash
# 108 тур 108 (PSP): colring не ожил после сна на 3410 с работы приставки — сравнение (int)(сейчас − 0) < 0 при системном времени > 2147 с. tests/p0level/run_bias.sh EBOOT_ТУРА [SET] -> PASS/FAIL
# flag clockbias 1 — к времени таймеров повтора добавляется 2^31 (как после 36 минут); новое: ioerr после сна конечен, colring переоткрылся; красный: flag colringretryold 1 (прежнее сравнение) — ioerr не прекращается, переоткрытия нет.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/p0bias}; mkdir -p "$OUT"; rm -rf "$OUT"/out_bn "$OUT"/out_bo
FLAGS="ballast=5" nice -n 20 tools/emu_run.sh tests/p0level/scene_bias.txt bn "$EB" 200 > "$OUT/bn.log" 2>&1
FLAGS="ballast=5" nice -n 20 tools/emu_run.sh tests/p0level/scene_bias_old.txt bo "$EB" 200 > "$OUT/bo.log" 2>&1
OUT="$OUT" python3 -I - <<'PY'
import re,sys
def p(n):
    r=dict(ok=0,fail=0,ioerr_after=0); sc=None
    for ln in open(__import__('os').environ['OUT']+'/out_%s/smoke.txt'%n,errors='replace'):
        if ln.startswith('SCENE '): sc=ln.split()[1]
        if ln.startswith('COLRING reopen ok'): r['ok']+=1
        if 'colring reopen-fail' in ln: r['fail']+=1
        if 'WATCHDOG colring ioerr' in ln and sc in ('AFTER','AFTER_END'): r['ioerr_after']+=1
    return r
N=p('bn'); O=p('bo'); print('новое:',N); print('прежнее сравнение (colringretryold):',O); ok=True
if N['ok']<1 or N['ioerr_after']>5: print('FAIL новое: colring не ожил'); ok=False
if not (O['ok']==0 and O['ioerr_after']>=N['ioerr_after']+10): print('КРАСНЫЙ НЕ ПОЙМАН: прежнее сравнение тоже ожило'); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
PY
