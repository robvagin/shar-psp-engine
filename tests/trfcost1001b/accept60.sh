#!/bin/bash
# №1001б приёмка одной командой (окно Глюки): pathGap (просвет на пути впереди) отбрасывает далёкие препятствия заранее и не зовёт routeAt, когда впереди никого: поведение трафика побитно то же, цена ниже.
#   tests/trfcost1001b/accept60.sh — стенд tools/bots/city984.sh на L3–L7 (6 маршрутов × 60 с):
#     1) новое побитно как прежнее (TRFCOST1001B=0): след игрока и всего трафика на 30 маршрутах, cmp код 0
#     2) цена: фаза «трафик» стенда новая не выше прежней (медиана трёх чередующихся прогонов, по уровням и в сумме)
#     3) красный контроль: сборка -DTRFCOST1001B_BREAK=1 (слишком тесный отбор) на L5 и L7 обязана разойтись с прежним
#   Цену кадра PSP даёт tools/bots/city984.sh emu / emucmp (эмулятор), здесь её нет. DATA — набор данных (по умолчанию тур test71). Последняя строка: PASS или FAIL
set -u
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
cd "$(dirname "$0")/../.."; export DATA=${DATA:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
S=tools/bots/city984.sh; O=$PWD/build/city984; bad=0
for i in 1 2 3; do TRFCOST1001B=0 $S run acc_o$i --prof > /dev/null 2>&1; $S run acc_n$i --prof > /dev/null 2>&1; done
echo "-- 1) побитно то же (новое против прежнего, 30 маршрутов):"; r=$($S cmp acc_o1 acc_n1 | head -1); echo "  $r"; echo "$r" | grep -q "НЕ ИЗМЕНИЛОСЬ" || { echo "  !! поведение изменилось"; bad=1; }
python3 - <<'PYEOF' || bad=1
import re, statistics, sys, os
O = os.path.expanduser('~/Downloads/shar-psp-t72-trfcost1001b/build/city984')
def ph(n):
    d = {}
    for i in (1, 2, 3):
        for l in open('%s/acc_%s%d/summary.txt' % (O, n, i)):
            m = re.match(r'ИТОГ L(\d): кадров \d+, [\d.]+ мкс/кадр \| фазы мкс/кадр: машина [\d.]+, мир [\d.]+, респавн/комнаты [\d.]+, трафик ([\d.]+)', l)
            if m: d.setdefault(m[1], []).append(float(m[2]))
    return {k: statistics.median(v) for k, v in d.items()}
o, n = ph('o'), ph('n'); to = sum(o.values()); tn = sum(n.values())
print('-- 2) цена, фаза «трафик» стенда, мкс/кадр Мака (медиана 3 прогонов) до → после: ' + ', '.join('L%s %.2f → %.2f' % (k, o[k], n[k]) for k in sorted(o)) + '; сумма %.2f → %.2f (×5,8 = %+.3f мс PSP)' % (to, tn, 5.8 * (tn - to) / 1000))
sys.exit(0 if tn <= to * 1.02 else 1)
PYEOF
echo "-- 3) красный контроль: сборка -DTRFCOST1001B_BREAK=1 (L5, L7) обязана разойтись с прежним:"
BINS=city984 SUF=_brk tools/bots/build.sh -DTRFCOST1001B_BREAK=1 > /dev/null 2>&1 || { echo "  !! сборка красного не вышла"; bad=1; }
rm -rf $O/acc_brk; mkdir -p $O/acc_brk/trace; : > $O/acc_brk/summary.txt
for L in 5 7; do BW_LEVEL=$L build/bots/city984_brk $DATA/l$L/level.shr $DATA/l$L/player.shr $DATA/l$L/level0$L.msn --routes 6 --secs 60 --seed 1 --trace $O/acc_brk/trace >> $O/acc_brk/summary.txt 2>&1; done
rb=$($S cmp acc_o1 acc_brk | grep -c "РАЗНЫЕ"); echo "  маршрутов с расхождением: $rb"; [ "$rb" -gt 0 ] || { echo "  !! красный не разошёлся: гейт ничего не доказывает"; bad=1; }
if [ $bad -eq 0 ]; then echo PASS; else echo FAIL; fi
