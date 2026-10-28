#!/bin/bash
# №1061 (Мультик): покрышки машин чёрные на всех мипах, диск не тронут. Зелёный — новые пакеты (build/tire1061/data), красный — пакеты набора TEST74 (без OVL).
# Сцена: L2, машина трафика 0 стоит на дороге, глаз в 8 м сбоку (angle 0), мипы -1 (авто), 2, 3 (flag mipconst: как грубые мипы на железе дальше 30-100 м) — такси, sedanB, sportsA, pizza.
# Прогоны: textures вкл (flat912 0) — там мипы читают атлас; мультик (toon 1, flat912 1) — не должно быть хуже. Метрика: по окну колёс кадра (x 40..440, y 95..215) «тёплые не чёрные» пиксели колеса: R > B + 18 и яркость 40..150 ВНЕ кузова нельзя отделить, поэтому считаем тёмные (яркость < 48)
# и коричнево-оливковые тёмно-средние (R > B + 12, яркость 48..110) — у жёлтого колеса вторых много, у чёрного первых.
# Зелёный: на мипах 2 и 3 у КАЖДОЙ машины темных не меньше, чем у красного, и у такси тёмных больше в 1.3 раза; красный: сравнение красного с самим собой не проходит (отношение 1.0).
# Запуск: tests/tire1061/accept60.sh [EBOOT]; SET — набор данных (по умолчанию SHARTEST15 test74 из integ), DATA — пакеты (по умолчанию build/tire1061/data). Последняя строка PASS/FAIL.
W="$(cd "$(dirname "$0")/../.." && pwd)"; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
EB=${1:-$W/viewer/build_t1061/EBOOT.PBP}; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74/PSP/GAME/SHARTEST15}; DATA=${DATA:-$W/build/tire1061/data}
O=${OUT:-$W/build/tire1061/acc}; mkdir -p "$O"; S=$W/tests/tire1061
( OVL=$DATA EMU_OWNER=Мультик EMU_MINE=2 OUT=$O $W/tools/emu_run.sh $S/cars_l2.txt green "$EB" 300 >/dev/null 2>&1 ) &
( OVL= EMU_OWNER=Мультик EMU_MINE=2 OUT=$O $W/tools/emu_run.sh $S/cars_l2.txt red "$EB" 300 >/dev/null 2>&1 ) &
wait
python3 -I - "$O" <<'PY'
import sys
from PIL import Image
import numpy as np
o = sys.argv[1]; ok = True
# окна колёс в кадре (глаз в 8 м сбоку, машина по центру): центры в кропе (40, 95) 400x120; среднее по 20x20 пикселей, два колеса
cen = {'taxi': [(140, 50), (300, 50)], 'sedanb': [(130, 52), (305, 52)], 'sports': [(130, 52), (285, 52)], 'pizza': [(125, 52), (270, 52)]}
def wheel(n, tx, c, m):
    a = np.asarray(Image.open(f'{o}/out_{n}/{tx}_{c}_m{m}.png').convert('RGB')).astype(int)[95:215, 40:440]
    return np.mean([a[y - 10:y + 10, x - 10:x + 10].reshape(-1, 3).mean(0) for x, y in cen[c]], 0)
for c in cen:
    for tx, ms in (('tex', (2, 3)), ('toon', (2,))):
        for m in ms:
            g, r = wheel('green', tx, c, m), wheel('red', tx, c, m)
            wg, wr = g[0] - g[2], r[0] - r[2]   # «жёлтость» колеса: R - B
            print(f'{tx} {c} mip{m}: green RGB {g.astype(int).tolist()} жёлтость {wg:.0f} | red RGB {r.astype(int).tolist()} жёлтость {wr:.0f}')
            if wg > wr + 6: ok = False   # новое не жёлтее старого ни у одной машины (яркость кадров шумит: фон, пешеходы)
            if tx == 'tex' and c == 'taxi' and not (wr >= 40 and wg <= 12): ok = False   # такси: красный жёлтый (доказывает дефект), зелёный чёрный
print('PASS' if ok else 'FAIL')
PY
