#!/bin/bash
# №1204 (Роб, 05.11.2026: «как оригинал»): изгородь-листва L7 J9-5 по записи PS2.
# x=145 (земля низко, верх изгороди высоко): в лоб упор на z ≈ 160,8 при 15/30/45 км/ч.
# x=135 (земля у верха изгороди): переезд поверх (дошла до z > 172), скорость у изгороди (минимум на z 157…172) не выше 85 % входной.
# Красный контроль: -DFOLIAGE_PS2_1154_OLD=1 (прежнее — проход насквозь) должен падать.
cd "$(dirname "$0")/../.."; LV=${LV:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15/l7/level.shr}
[ -f "$LV" ] || { echo "нет $LV"; exit 2; }
mkdir -p build/jtmp
build() { cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "$@" tests/airwalls/host.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o "$B" || exit 1; }
trace() { "$B" "$LV" scan $1 125 0 --y 5.8 --kmh $2:$2:1 --lat 0:0:1 --secs 16 -vv 2>&1 | python3 -c "
import sys,re,math
rows=[]
for l in sys.stdin:
    m=re.search(r't=([\d.]+) pos (\S+) (\S+) (\S+) vel (\S+) (\S+) (\S+)',l)
    if m: t,x,y,z,vx,vy,vz=map(float,m.groups()); rows.append((z,math.hypot(vx,vz)*3.6))
vin=max((v for z,v in rows if z<150),default=0)
mid=[v for z,v in rows if 157<=z<=172]; vout=min(mid) if mid else -1
if rows[-1][0]<172: vout=-1   # не переехала
print('%.0f %.0f %.1f'%(vin,vout,rows[-1][0]))"; }
run() { fail=0
  for k in 15 30 45; do read vin vout zend <<<"$(trace 145 $k)"
    ok=$(python3 -c "print(1 if 160.3<=$zend<=161.3 else 0)"); echo "  x=145 $k км/ч: конец z $zend ($([ $ok = 1 ] && echo упор || echo НЕ-упор))"; [ $ok = 1 ] || fail=1; done
  for k in 15 30 45; do read vin vout zend <<<"$(trace 135 $k)"
    ok=$(python3 -c "print(1 if $vout>=0 and $vout<=0.85*$vin else 0)"); echo "  x=135 $k км/ч: вход $vin, минимум у изгороди $vout ($([ $ok = 1 ] && echo 'переезд с потерей' || echo 'нет потери/не переехала'))"; [ $ok = 1 ] || fail=1; done
  return $fail; }
B=build/jtmp/f1204_new; build; echo "новое:"; run; n=$?
B=build/jtmp/f1204_old; build -DFOLIAGE_PS2_1154_OLD=1; echo "красный контроль (_OLD=1):"; run; o=$?
[ $n = 0 ] && [ $o != 0 ] && { echo "foliage1204: PASS (зелёный, красный пойман)"; exit 0; }
echo "foliage1204: FAIL (новое rc=$n, красный rc=$o)"; exit 1
