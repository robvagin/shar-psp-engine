#!/bin/bash
# 1367 (Роб, «сотый раз»: «если игрок долго обходит машину, чтобы сесть, он садится ирисом; у Гомера и Барта это есть, а Мардж свой джип обходит полтора часа»):
# ОДНО правило для всех героев (Гомер, Барт, Лиза, Мардж, Апу) и всех машин: путь к двери водителя с обходом дальше 4,5 м (≈1,1 с ходьбы) или упор на 0,6 с — посадка ирисом (viewer/homer.c HS_TO_DOOR, HOMERFAR1367_OLD).
#   tests/homer/enter1367.sh НАБОР [EMU=0|1]   НАБОР — папка с level.shr, homer.shr, lN/level.shr и lN/player.shr (build/test108tour/PSP/GAME/SHARTEST15)
# 1) без езды (host, один код с игрой): герой каждого уровня L1–L7 (L1 Гомер, L2 Барт, L3 Лиза, L4 Мардж, L5 Апу, L6–L7 Гомер) × все 89 машин × 8 мест вокруг (в пределах «габарит+1 м»: пассажирский борт у зада, середины, носа, сзади, спереди,
#    водитель у двери): посадка не дольше LIMIT = 3,5 с (N + дверь ≈ 1,1 + 2,4) — либо ирис. Красный: -DHOMERFAR1367_OLD=1 — обязан дать дольше LIMIT на каждом уровне.
# 2) эмулятор (если EMU не 0): Мардж на L4 у борта пассажира своего джипа marge_v (обход ~6 м): ирис «IRIS why=seat» в первую секунду после △; красный (flag homerfar1367old 1) — ирис не идёт, обходит пешком.
cd "$(dirname "$0")/../.."; S=${1:?набор}; EMU=${2:-1}; T=${TMPDIR:-/tmp}/en1367.$$; mkdir -p $T; fail=0; LIMIT=3.5
python3 - > $T/names.c <<'PY'
import re
t=open('viewer/carparams.h',encoding='utf-8').read(); m=re.search(r'carCons\[\]\s*=\s*\{(.*?)\};',t,re.S); n=re.findall(r'"([A-Za-z0-9_]+)"',m.group(1))
print('const char *const carNames1367[] = {'+','.join('"%s"'%x for x in n)+'};\nint carNamesN1367 = %d;'%len(n))
PY
for v in new red; do F=""; [ $v = red ] && F="-DHOMERFAR1367_OLD=1"
  cc -std=c99 -O2 -w -DSKIN_HOST $F -I viewer tests/homer/host_enter1367.c $T/names.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o $T/enter_$v || { echo "сборка $v не вышла"; exit 2; }; done
for L in 1 2 3 4 5 6 7; do if [ $L = 1 ]; then LV=$S/level.shr; SK=$S/homer.shr; else LV=$S/l$L/level.shr; SK=$S/l$L/player.shr; fi
  n=$($T/enter_new $LV $SK "" $LIMIT | tail -1 | sed 's/.*: //'); r=$($T/enter_red $LV $SK "" $LIMIT | tail -1 | sed 's/.*: //')
  echo "L$L: новое — дольше ${LIMIT} с или без посадки: $n; красное: $r"
  [ "$n" = 0 ] || { echo "  FAIL: новое правило дало долгие посадки"; fail=1; }; [ "$r" -gt 0 ] || { echo "  FAIL: красное не поймано (слепой гейт)"; fail=1; }
done
if [ "$EMU" != 0 ]; then
  for m in new red; do { echo "scene S_L4_load"; [ $m = red ] && echo "flag homerfar1367old 1"; cat <<'S'
flag golevel 4
wait 16
scene S_L4_out
foot 217.0 172.27 3.14
wait 3
press triangle
wait 1
shot s1
wait 5
shot s6
end
S
  } > $T/$m.scn
    SHIP_LEVELS="2 3 4 5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=$T/$m.scn SMOKE_MAXT=200 tools/smoke.sh "$S" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
    tp=$(grep -a "^TELEPORT" $D/smoke.txt | tail -1 | sed 's/.*t=\([0-9.]*\).*/\1/'); ti=$(grep -a "^IRIS why=seat phase=open" $D/smoke.txt | head -1 | sed 's/.*t=\([0-9.]*\)$/\1/')
    mkdir -p build/enter1367; cp -c $D/s1.png build/enter1367/L4_marge_${m}_s1.png 2>/dev/null; cp -c $D/s6.png build/enter1367/L4_marge_${m}_s6.png 2>/dev/null
    echo "L4 Мардж marge_v, $m: △ в $((${tp%.*}+3)) с, ирис посадки: ${ti:-нет}"
    if [ $m = new ]; then [ -n "$ti" ] && awk -v a="$ti" -v b="$tp" 'BEGIN{exit !(a-(b+3.0) < 1.5)}' || { echo "  FAIL: ирис не пошёл в первые 1,5 с"; fail=1; }
    else [ -z "$ti" ] || { echo "  FAIL: красный: ирис всё равно есть"; fail=1; }; fi
  done
fi
rm -rf $T
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
