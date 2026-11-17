#!/bin/bash
# 108: ВТОРОЙ сон на том же уровне через >2147 с после первого (Fable-2): tests/p0level/run_two.sh EBOOT [SET] -> PASS/FAIL. Сон 1 при обычных часах (переоткрытие), потом flag clockbias 1 (+2^31), сон 2.
# Новое: COLRING reopen ok ≥ 2; красный colringretrymid (сравнение «срока» 74fa310e): второе переоткрытие не случается (ok = 1), ioerr после сна 2 не прекращается.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/p0two}; mkdir -p "$OUT"; rm -rf "$OUT"/out_tn "$OUT"/out_tm
FLAGS="ballast=5" nice -n 20 tools/emu_run.sh tests/p0level/scene_two.txt tn "$EB" 200 > "$OUT/tn.log" 2>&1
FLAGS="ballast=5" nice -n 20 tools/emu_run.sh tests/p0level/scene_two_mid.txt tm "$EB" 200 > "$OUT/tm.log" 2>&1
OUT="$OUT" python3 -I - <<'PY'
import os,sys
def p(n):
    r=dict(ok=0,ioerr_after=0); sc=None
    for ln in open(os.environ['OUT']+'/out_%s/smoke.txt'%n,errors='replace'):
        if ln.startswith('SCENE '): sc=ln.split()[1]
        if ln.startswith('COLRING reopen ok'): r['ok']+=1
        if 'WATCHDOG colring ioerr' in ln and sc in ('AFTER','AFTER_END'): r['ioerr_after']+=1
    return r
N=p('tn'); M=p('tm'); print('новое:',N); print('сравнение 74fa310e (colringretrymid):',M); ok=True
if N['ok']<2 or N['ioerr_after']>5: print('FAIL новое: второй сон не переоткрыл кольцо'); ok=False
if not (M['ok']<2 and M['ioerr_after']>=N['ioerr_after']+5): print('КРАСНЫЙ НЕ ПОЙМАН: прежнее сравнение тоже переоткрыло'); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
PY
