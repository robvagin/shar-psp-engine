#!/bin/bash
# 107б П10: урон машине от предмета по порогу оригинала (нормированный импульс = приведённая масса · скорость · (1+e) / 100000, порог 0,06). Фонарь 1000 кг (простой L1 и стейтпроп L5): без урона на 6 м/с, с уроном на 12 и 16 м/с;
# лёгкий предмет (до 150 кг) — урона нет даже на 16 м/с. Красный (-DCARDMGPROP107_OLD=1): урона от предметов нет нигде.   tests/cardmgprop107/gate.sh
set -u; cd "$(dirname "$0")/../.."; SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/cardmgprop107; mkdir -p $OUT
SRC=(tests/cardmgprop107/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c)
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "${SRC[@]}" -lm -o $OUT/g || { echo "FAIL: сборка"; exit 1; }; cc -std=c11 -O2 -w -DSKIN_HOST -DCARDMGPROP107_OLD=1 -I viewer "${SRC[@]}" -lm -o $OUT/r || exit 1
$OUT/g $SET/level.shr $SET/l5/level.shr > $OUT/g.txt; $OUT/r $SET/level.shr $SET/l5/level.shr > $OUT/r.txt
python3 - <<'PY'
import re
def rd(p):
    d = []
    for l in open(p):
        m = re.match(r'v\d (\S+) lamp (\S+) crate (\S+) sp (\S+)', l)
        if m: d.append(tuple(map(float, m.groups())))
    return d
g, r = rd('build/cardmgprop107/g.txt'), rd('build/cardmgprop107/r.txt'); bad = 0
for (v, l, c, s), (_, l2, c2, s2) in zip(g, r): print('v=%.0f м/с: фонарь простой %.3f, лёгкий %.3f, стейтпроп %.3f | красный: %.3f %.3f %.3f' % (v, l, c, s, l2, c2, s2))
ok = len(g) == 3 and g[0][1] <= 1e-6 and g[0][3] <= 1e-6 and g[1][1] > 0 and g[2][1] > 0 and g[1][3] > 0 and g[2][3] > 0 and all(x[2] <= 1e-6 for x in g)
red = all(x[1] <= 1e-6 and x[3] <= 1e-6 for x in r)
print('зелёный', 'OK' if ok else 'FAIL', '| красный (урона нет)', 'OK' if red else 'НЕ ПОЙМАН')
raise SystemExit(0 if ok and red else 1)
PY
[ $? = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
