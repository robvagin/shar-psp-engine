#!/bin/bash
# №1009 (починка теста, ветка t72/inksepfix): accept60.sh EBOOT [red]. Прежний сценарий tests/inksep1009 ставил Гомера по зашитым координатам машины трафика (257,3; 161,1), а `flag trfgo` берёт «первую активную машину»,
# которая теперь в другом месте (331,8; 161,2): Гомер не у машины, считать нечего (306 = 306 уже на исходном коммите 28dda2ac). Здесь сцена строится от положения машины (flag trfhome G).
# Мера та же: чёрных точек (все каналы < 70) в окне вокруг Гомера и бампера, flag inksep1009 1 против 0 на двух зазорах: стало >= было + 25. «red» — оба кадра с флагом 0: падает
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); export OUT=${OUT:-/tmp/inksepfix_out}; export EMU_OWNER=${EMU_OWNER:-Небо}; export EMU_MINE=${EMU_MINE:-2}
EB=$1; SC=$D/scenario_home.txt; [ "${2:-}" = red ] && SC=$D/scenario_home_red.txt
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc; bash $I/tools/emu_run.sh $SC acc "$EB" 300 >/dev/null 2>&1
python3 -I - "$OUT/out_acc" <<'PYEOF'
import sys
from PIL import Image
import numpy as np
d = sys.argv[1]; ok_all = True
def dark(nm):
    a = np.asarray(Image.open('%s/%s.bmp' % (d, nm)).convert('RGB'), int); return int((a[115:185, 215:280].max(axis=2) < 70).sum())
try:
    for g in ('h12', 'h22'):
        o, n = dark('o_' + g), dark('n_' + g); ok = n >= o + 25; ok_all &= ok
        print('  %s зазор %s: чёрных точек у Гомера и бампера было %d, стало %d (нужно стало >= было + 25)' % ('ok  ' if ok else 'FAIL', g, o, n))
except OSError: print('нет кадров'); print('FAIL'); sys.exit(1)
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
