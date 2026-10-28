#!/bin/bash
# №705 приёмка (Роб 17.10: «L5 J4-5, пожарная лестница (как подъёмник): на трамплине невозможно прыгнуть, текстуры битые, машину ломает в текстуры и сразу бросает в сторону»).
# Факт PS2 (knowledge/facts/ps2-h82-fire-escape.yaml): на 25 м/с оригинал долетает до крыши и садится, забора над краем крыши у x 25,3 на высоте 16–20 м нет; на 20 м/с бьётся в фасад ниже кромки (как и порт).
# Правило экспорта (tools/psp_export.py, FENCE_ROOF, по умолчанию ВКЛ): внутренний забор по краю крыши/уступа (земля за ним выше земли с лица на 2 м и больше) — верх = земля за ним + 0,3 м, а не + 2,5…3 м.
# Откат: SHR_FENCE_ROOF_OLD=1 (пакеты побайтно как TEST73). Хост-проверка на реальном экспорте L1–L7 (без эмулятора: правило меняет только отрезки заборов COL1).
# 1) L5: звенья заборов у крыши J4-5 (окно x 20…32, z -12…20): верх над крышей (y1 > 18) был у 4 звеньев, стал 3 (боковой торец 659 опустился 20,2 -> 17,5, у 700 нижняя часть 17,5; звенья у стены дома 660, 698, 701 остаются)
# 2) рой tests/fence705/swarm.c (ядро car.c, зона прыжка, 135 заездов 20/25/30 м/с): на крышу садится не меньше, чем прежде (с правилом 36 против 33 на 30 м/с); ударов в воздухе не больше прежнего
# 3) семейство: на каждом из L1–L7 число кусков, получивших верх «земля за забором + 0,3 м» (строка №705 в журнале экспорта), и файл с правилом отличается от прежнего ровно в COL1 (размер секции) — в отчёт
# 4) сайдкары L5 совпадают (tools/check_sidecars.py: carflat flat912 noise949 edge983 zones1007) на новом и на прежнем экспорте
# Красный контроль: RED=1 tests/fence705/accept60.sh — «новым» объявляется прежний экспорт (SHR_FENCE_ROOF_OLD=1); гейт обязан дать FAIL
# Запуск: tests/fence705/accept60.sh [ПАПКА_ЭКСПОРТОВ]  (в ней new/lN/level.shr и old/lN/level.shr; по умолчанию build/f705, делается build/f705/exp.sh). Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
X=${1:-$W/build/f705}; rc=0; T=$(mktemp -d)
if [ "${RED:-0}" = 1 ]; then mkdir -p $T/red; for k in new old; do ln -s $X/old $T/red/$k; done; for k in sc_new sc_old set_new set_old; do ln -s $X/${k%_*}_old $T/red/$k 2>/dev/null; done; ln -sf $X/sc_old $T/red/sc_new; ln -sf $X/set_old $T/red/set_new; ln -sf $X/sc_old $T/red/sc_old; ln -sf $X/set_old $T/red/set_old; X=$T/red; echo "  КРАСНЫЙ КОНТРОЛЬ: «новый» экспорт = прежний (SHR_FENCE_ROOF_OLD=1)"; fi
CC="cc -std=c11 -O2 -w -DSKIN_HOST -include stdio.h -I $W/viewer"
$CC $H/segs.c $W/viewer/col.c $W/viewer/coins.c -lm -o $T/segs || { echo "segs не собрался"; echo FAIL; exit 1; }
$CC $H/swarm.c $W/viewer/car.c $W/viewer/col.c $W/viewer/coins.c -lm -o $T/swarm || { echo "swarm не собрался"; echo FAIL; exit 1; }
tall() { $T/segs $X/$1/l5/level.shr 20 32 -12 20 | awk '{for(i=1;i<=NF;i++) if($i ~ /^y1=/){v=substr($i,4)+0; if(v>18 && v<1000) n++}} END{print n+0}'; }
tn=$(tall new); to=$(tall old); echo "  1) L5 у крыши J4-5: звеньев с верхом над крышей (y1 > 18): прежнее $to -> новое $tn"
[ "$to" -gt "$tn" ] || { echo "     !! красный не красный или правило не сработало"; rc=1; }
sw() { $T/swarm $X/$1/l5/level.shr 4 | grep "^v=30" | sed 's/.*на крыше \([0-9]*\).*/\1/'; }
rn=$(sw new); ro=$(sw old); echo "  2) рой L5 30 м/с (45 заездов): садятся на крышу прежнее $ro -> новое $rn"
[ "$rn" -ge "$ro" ] || { echo "     !! садится хуже"; rc=1; }
echo "  2б) №705 ч.2, зона прыжка (рой L5, ядро car.c): на сколько заездов из 45 машина садится на крышу"
for sp in 20 25 30; do n=$($T/swarm $X/new/l5/level.shr 4 | grep "^v=$sp" | sed 's/.*на крыше \([0-9]*\).*/\1/'); o=$($T/swarm $X/new/l5/level.shr 4 old | grep "^v=$sp" | sed 's/.*на крыше \([0-9]*\).*/\1/')
  echo "     $sp м/с: новое $n, прежнее (carJumpZoneGateOld=1) $o   (PS2: 20 — нет, 25 — да)"; eval "n$sp=$n o$sp=$o"; done
[ "$n20" = 0 ] || { echo "     !! на 20 м/с садится (PS2: бьётся в фасад)"; rc=1; }
[ "$n25" -ge 10 ] || { echo "     !! на 25 м/с не садится (PS2: садится)"; rc=1; }
[ "$o25" = 0 ] || { echo "     !! красный: на 25 м/с прежнее правило тоже садится"; rc=1; }
echo "     twin (tests/jumps/twin.sh --gate, L1 нового экспорта): $(bash $W/tests/jumps/twin.sh --gate $X/new/l1/level.shr 2>&1 | tail -1)"
bash $W/tests/jumps/twin.sh --gate $X/new/l1/level.shr 2>&1 | tail -1 | grep -q "twin gate: PASS" || { echo "     !! twin не PASS"; rc=1; }
echo "  2в) J9-5 (L1 jumpzone3, запись PS2 tests/jumps/ramps_ps2_jz3.tsv): правило не должно менять порт (PS2 его не подтвердила): twin прежний == новый"
$CC $W/tests/jumps/twin.c $W/viewer/car.c $W/viewer/col.c $W/viewer/props.c $W/viewer/stateprops.c $W/viewer/coins.c -lm -o $T/twin_n && $CC -DJUMP_ZONE_GATE_OLD=1 $W/tests/jumps/twin.c $W/viewer/car.c $W/viewer/col.c $W/viewer/props.c $W/viewer/stateprops.c $W/viewer/coins.c -lm -o $T/twin_o || { echo "twin не собрался"; rc=1; }
$T/twin_n $X/new/l1/level.shr --ref $W/tests/jumps/ramps_ps2_jz3.tsv --known /dev/null > $T/jz3_n.txt; $T/twin_o $X/new/l1/level.shr --ref $W/tests/jumps/ramps_ps2_jz3.tsv --known /dev/null > $T/jz3_o.txt
cmp -s $T/jz3_n.txt $T/jz3_o.txt && echo "     J9-5: вывод twin прежнего и нового правила одинаков" || { echo "     !! J9-5 изменился под правилом"; rc=1; }
echo "  3) семейство (куски с верхом по правилу / размер level.shr: прежний -> новый):"
for L in 1 2 3 4 5 6 7; do n=$(grep -h '№705' $X/new/l$L/exp.log | sed 's/.*у \([0-9]*\) кусков.*/\1/'); o=$(grep -c '№705' $X/old/l$L/exp.log)
  printf "     L%s: кусков %s, в прежнем экспорте правило %s; level.shr %s -> %s\n" $L "${n:-0}" "$([ $o = 0 ] && echo не сработало || echo РАБОТАЕТ)" $(stat -f %z $X/old/l$L/level.shr) $(stat -f %z $X/new/l$L/level.shr)
  [ "$o" = 0 ] || rc=1; done
echo "  3б) вне секции COL1 новый level.shr равен прежнему (допуск: поля смещений, не больше 8 байт):"
python3 -W ignore -I - $X <<'PY' || rc=1
import struct, sys
x = sys.argv[1]; ok = True
for L in range(1, 8):
    a = open(f'{x}/old/l{L}/level.shr', 'rb').read(); b = open(f'{x}/new/l{L}/level.shr', 'rb').read()
    co, po = struct.unpack_from('<I', a, 92)[0], struct.unpack_from('<I', a, 96)[0]; cb, pb = struct.unpack_from('<I', b, 92)[0], struct.unpack_from('<I', b, 96)[0]
    d = sum(1 for i in range(104, co) if a[i] != b[i]) + (0 if co == cb else 999) + sum(1 for p, q in zip(a[po:], b[pb:]) if p != q) + abs(len(a) - po - (len(b) - pb))
    d += sum(1 for i in range(104) if i not in (96, 97, 98, 99, 100, 101, 102, 103) and a[i] != b[i])
    print(f'     L{L}: байт вне COL1 отличается {d}{"" if d <= 8 else "   <<<"}'); ok &= d <= 8
sys.exit(0 if ok else 1)
PY
echo "  4) сайдкары L5 (check_sidecars): новый экспорт / прежний"
echo "     (набор: файлы TEST73 с подменой l5/level.shr на сырой экспорт; python3 -I tools/check_sidecars.py $X/set_new --only carflat,flat912,noise949,edge983,zones1007 --out $X/sc_new; то же set_old -> sc_old)"
for f in carflat930.bin carflatm930.bin edge983.bin flat912.bin noise949.bin zones1007p.bin; do
  if cmp -s $X/sc_new/l5/$f $X/sc_old/l5/$f; then echo "     $f: выход генератора на новом экспорте == на прежнем"; else echo "     !! $f: РАЗНЫЕ"; rc=1; fi; done
echo "     (из набора TEST73 генераторы сходятся и на сыром прежнем экспорте, кроме edge983/zones1007p: сырой экспорт без PVS/wind; l3 noise949 устарел в самом TEST73 — не от этой правки)"
if [ -n "${EBOOT:-}" ] && [ -f "$EBOOT" ]; then
  echo "  5) реальный порт в эмуляторе (EBOOT $EBOOT, набор тура TEST73): машина с газом из x=78 и x=70 на L5 J4-5; крыша = снимок с y > 17 при x < 26"
  SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test73tour/PSP/GAME/SHARTEST15}; export SET EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-3}; O=$X/emu; mkdir -p $O
  sed 's/^scene RAMP$/scene RAMP\nflag jzgate705 0/' $H/ramp78.txt > $O/ramp78_old.txt
  for p in "$H/ramp78.txt r78n" "$O/ramp78_old.txt r78o" "$H/ramp70.txt r70n"; do set -- $p; ( OUT=$O bash $W/tools/emu_run.sh $1 $2 "$EBOOT" 200 >/dev/null 2>&1 ) & done; wait
  roof() { grep '^SHOT' $O/out_$1/smoke.txt | awk '{ if ($6+0 > 17 && $5+0 < 26) r=1 } END { print r+0 }'; }
  a=$(roof r78n); b=$(roof r78o); c=$(roof r70n)
  echo "     x=78: новое садится на крышу $a, прежнее $b (ждём 1 и 0); x=70: новое $c (ждём 0, как PS2 на 20 м/с)"
  [ "$a" = 1 ] && [ "$b" = 0 ] && [ "$c" = 0 ] || { echo "     !! эмулятор не подтвердил"; rc=1; }
else echo "  5) эмулятор пропущен (EBOOT=viewer/build_705/EBOOT.PBP tests/fence705/accept60.sh — реальный порт, 3 прогона)"; fi
rm -rf $T; [ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
