#!/bin/bash
# П3: tests/pvb/run.sh НАБОР EBOOT — PVB1 (видимость битами, tools/pvs_bits.py): верность на Маке по всем семи уровням + загрузка в эмуляторе.
#   1) pvs_bits.py на каждом level.shr: независимое чтение файла как движком, ни один кусок PVS1 не потерян (надмножество); красный контроль — у каждой ячейки гасится один бит, проверка обязана упасть
#   2) эмулятор (безголовый, один): набор с pvb.shr -> строки PVBLOAD level=1 и level=4 ok=1 и END; красный контроль — в pvb.shr L4 испорчена подпись: PVBLOAD why=3, PVS1 читается, END
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор (SHARTEST15)}; EB=${2:?EBOOT}; cd $W; rc=0
export DEVELOPER_DIR=/Library/Developer/CommandLineTools; [ -x .venv/bin/python ] && export PATH=$W/.venv/bin:$PATH
T=${OUT:-$W/build/pvb}; rm -rf $T/set $T/setred; mkdir -p $T
for n in 1 2 3 4 5 6 7; do L=$SET/l$n/level.shr; [ $n = 1 ] && L=$SET/level.shr; P=$T/pvb$n.shr
  python3 tools/pvs_bits.py $L $P --quiet > $T/l$n.txt 2>&1 || { echo "L$n: FAIL (зелёный)"; cat $T/l$n.txt | tail -3; rc=1; continue; }
  python3 - "$L" "$P" <<'PY' || rc=1
import sys; sys.path.insert(0, 'tools'); import pvs_bits, pvs_check
L = pvs_check.load(sys.argv[1]); P = L['pvs']; city = L['rec']['owner'] == 0; ni = L['rec']['ni']
extra = pvs_bits.check_file(sys.argv[2], L, P, city, ni)
try: pvs_bits.check_file(sys.argv[2], L, P, city, ni, drop_bit=True); print('красный контроль НЕ упал'); sys.exit(1)
except AssertionError: pass
import os; print(f'ok: {sys.argv[1].split("/")[-2] if "/l" in sys.argv[1] else "l1"} {os.path.getsize(sys.argv[2])} Б, лишних кусков {extra}, красный контроль упал')
PY
done
cp -c -R $SET $T/set; for n in 2 3 4 5 6 7; do cp $T/pvb$n.shr $T/set/l$n/pvb.shr; done; cp $T/pvb1.shr $T/set/pvb.shr
cp -c -R $T/set $T/setred; rm -f $T/setred/l4/pvb.shr; python3 -I - $T/pvb4.shr $T/setred/l4/pvb.shr <<'PY'
import sys; b = bytearray(open(sys.argv[1], 'rb').read()); b[8] ^= 0xFF; open(sys.argv[2], 'wb').write(b)   # подпись (смещение 8) испорчена
PY
run() { SET=$1 OUT=$T EMU_OWNER=${EMU_OWNER:-Pvb} EMU_MINE=2 SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" FLAGS="ballast=9" bash tools/emu_run.sh $H/gate_load.txt $2 $EB 400 > $T/$2.out 2>&1; }
run $T/set green & sleep 2; run $T/setred red & wait
g=$T/out_green/smoke.txt; r=$T/out_red/smoke.txt
grep -q 'PVBLOAD level=1 ok=1' $g && grep -q 'PVBLOAD level=4 ok=1' $g && grep -q '^END' $g && echo "ok: эмулятор, PVB1 принят на L1 и L4, END" || { echo "FAIL: эмулятор (зелёный)"; rc=1; }
grep -q 'PVBLOAD level=4 ok=0 why=3' $r && grep -q 'PVSLOAD level=4 .* ok=1' $r && grep -q '^END' $r && echo "ok: красный набор (испорчена подпись) -> PVS1 как раньше, END" || { echo "FAIL: красный контроль подписи"; rc=1; }
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
