#!/bin/bash
# 107б P0 (Бабл, часть msnart + гэги; сон и пробуждение, слово Роба 1273): эмулятор L1, flag simsleep N Будки (power.h: kill-хуки закрывают дескрипторы msnart.shr и пакета гэгов в −1, N открытий/чтений отказывают, пробуждение двигает pwrEpoch).
#  A) значок задания: сон (simsleep 3), сразу старт m5 (с отключённым самолечением 108: msnartio108old 1). Зелёный: после пробуждения art= по значку (msnic) есть, строка art-pwr. Красный (pwr107old 1): art= нет, art+ повторяется.
#  B) гэг барбекю (228,6; 201,1): Гомер стоит у него при мёртвых дескрипторах пакета collect, потом пробуждение. Зелёный: COLLREOPEN и GAGSTREAM resident > 0 после пробуждения. Красный (pwr107old 1, collheal107old 1): гэг не загружен.
#   tests/pwr107/emu.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/pwr107; mkdir -p $OUT
go() { local N=$1; SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
a() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag msnartio108old 1"; echo "flag pwr107old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag simsleep 3"; echo "flag msnmainat 500"; echo "wait 14"; echo "end"; } > $OUT/$N.scn; go $N; }
b() { local N=$1 O=$2; { echo "scene gag"; echo "flag pwr107old $O"; echo "flag collheal107old $O"; echo "wait 4"; echo "foot 407.99 -413.09 0 4.6"; echo "wait 4"; echo "flag simsleep 3"; echo "foot 228.567 201.097 0 5"; echo "wait 12"; echo "end"; } > $OUT/$N.scn; go $N; }
a pwrA_new 0; a pwrA_old 1; b pwrB_new 0; b pwrB_old 1
python3 - <<'PY'
import re
B = 'build/pwr107'
def rd(n): return open(f'{B}/{n}/smoke.txt', errors='replace').read().splitlines()
bad = 0
def A(n):   # art= по значку ПОСЛЕ строки пробуждения; art+ до и после
    L = rd(n); woke = 0; before = after = eqAfter = pw = 0
    for l in L:
        if l.startswith('PWR sim sleep'): woke = 1
        if l.startswith('BBOP art+') and 'msnic' in l: after += woke; before += 1 - woke
        if l.startswith('BBOP art=') and 'msnic' in l: eqAfter += woke
        if l.startswith('BBOP art-pwr'): pw += 1
    return before, after, eqAfter, pw
def Bq(n):   # гэги: сколько чтений дошло ПОСЛЕ пробуждения (async done), переоткрытий
    L = rd(n); woke = 0; d0 = d1 = 0; re_ = 0
    for l in L:
        if l.startswith('PWR sim sleep'): woke = 1
        if l.startswith('COLLREOPEN'): re_ += 1
        m = re.match(r'GAGSTREAM resident=(\d+) tracks=(\d+) skins=(\d+).*async=(\d+)/(\d+)', l)
        if m:
            if woke: d1 = max(d1, int(m.group(5)))
            else: d0 = max(d0, int(m.group(5)))
    return re_, d0, d1
an, ao, bn, bo = A('pwrA_new'), A('pwrA_old'), Bq('pwrB_new'), Bq('pwrB_old')
print('A значок (art+ до пробуждения, art+ после, art= после, art-pwr): new %d %d %d %d | old %d %d %d %d' % (an + ao))
print('B гэг (переоткрытий, чтений дошло до пробуждения, максимум после): new %d %d %d | old %d %d %d' % (bn + bo))
if not (an[2] >= 1 and an[3] >= 1): print('FAIL A зелёный: после пробуждения значок не прочитан'); bad = 1
if not (ao[2] == 0): print('КРАСНЫЙ A НЕ ПОЙМАН'); bad = 1
else: print('красный A (pwr107old 1): значок после пробуждения не пришёл, как должен')
if not (bn[0] >= 1 and bn[2] > bn[1]): print('FAIL B зелёный: после пробуждения гэги не читаются'); bad = 1
if not (bo[0] == 0 and bo[2] <= bo[1]): print('КРАСНЫЙ B НЕ ПОЙМАН'); bad = 1
else: print('красный B (pwr107old 1, collheal107old 1): после пробуждения гэги не читаются, как должен')
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
