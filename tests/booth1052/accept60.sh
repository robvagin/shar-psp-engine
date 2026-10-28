#!/bin/bash
# №1052 (Управление): пятно под будкой (phone_icon_shadow, сложение) не рисуется. Зелёный — по умолчанию, красный — flag boothdecal1052 0 (как в TEST73).
# Метрика: число сиреневых пикселей вокруг основания будки L1 (кадр c02 сценария view2.txt). Зелёный < 100, красный > 1000. Последняя строка PASS/FAIL.
# Запуск: tests/booth1052/accept60.sh [EBOOT] ; SET — набор данных (по умолчанию SHARTEST15 из integ)
W="$(cd "$(dirname "$0")/../.." && pwd)"; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
EB=${1:-$W/viewer/build_dec/EBOOT.PBP}; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test72/PSP/GAME/SHARTEST15}
O=${OUT:-$W/build/b1052/acc}; mkdir -p "$O"; S=$W/tests/booth1052
sed 's/^flag msn$/flag msn\nflag boothdecal1052 0/' $S/view2.txt > "$O/red.txt"
EMU_OWNER=Управление EMU_MINE=2 OUT=$O $W/tools/emu_run.sh $S/view2.txt green "$EB" 60 >/dev/null 2>&1
EMU_OWNER=Управление EMU_MINE=2 OUT=$O $W/tools/emu_run.sh "$O/red.txt" red "$EB" 60 >/dev/null 2>&1
python3 -I - "$O" <<'PY'
import sys
from PIL import Image
import numpy as np
o = sys.argv[1]; res = {}
for n in ('green', 'red'):
    a = np.asarray(Image.open(f'{o}/out_{n}/c02.png').convert('RGB')).astype(int)[120:200, 120:360]
    R, G, B = a[..., 0], a[..., 1], a[..., 2]
    res[n] = int(((B > G + 25) & (B > R + 5) & (R > 110) & (B > 170)).sum())
print('lavender px: green %d red %d' % (res['green'], res['red']))
print('PASS' if res['green'] < 100 and res['red'] > 1000 else 'FAIL')
PY
