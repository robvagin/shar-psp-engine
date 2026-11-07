#!/bin/bash
# Гейт №1169 «после подлёта к ТВ по RESUME GAME на экране нет заголовка меню со стрелками»: безголовый тур-EBOOT (RELEASE+KEEP_SMOKE), набор данных, сохранение (SMOKE_SAVEFILES).
#   tests/menu/tvleave1169_gate.sh EBOOT_tour.PBP DATA_DIR SAVE.bin
# 1) кадр TV_leave (кадр, что висит на экране на время загрузки): жёлтых пикселей заголовка/надписи в верхних 90 строках < 40 (в меню их > 300);
# 2) контроль чувствительности: кадр F_00 (меню до нажатия) — жёлтых > 300, иначе гейт слеп; 3) журнал: MENUHOLD hold=1.
# EBOOT, собранный с -DTVLEAVE1169_OLD=1, обязан упасть на пункте 1
set -u; cd "$(dirname "$0")/../.."
EB=${1:?EBOOT}; SRC=${2:?DATA}; SAVE=${3:?SAVE}; SCN=$PWD/tests/menu/tvleave1169_scenario.txt; T=$PWD/build/tvgate1169_$$; trap 'rm -rf "$T"' EXIT
mkdir -p "$T"; cp -cR "$SRC" "$T/pkg" 2>/dev/null || cp -R "$SRC" "$T/pkg"; rm -f "$T/pkg/EBOOT.PBP"; cp "$EB" "$T/pkg/EBOOT.PBP"
while [ "$(pgrep -x PPSSPPHeadless | wc -l)" -ge 5 ]; do sleep 10; done
SMOKE_HEADLESS=1 SMOKE_SAVEFILES="$SAVE=save00.bin" SMOKE_SCN=$SCN SMOKE_MAXT=120 tools/smoke.sh "$T/pkg" > "$T/run.out" 2>&1
R=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$T/run.out"); [ -n "$R" ] || { echo "FAIL: нет отчёта: $(head -1 "$T/run.out")"; exit 1; }
python3 -I - "$R" <<'PY'
import sys
from PIL import Image
import numpy as np
R=sys.argv[1]
def yellow(n):
    a=np.asarray(Image.open(f'{R}/{n}.png').convert('RGB'),int)[:90]
    return int(((a[:,:,0]>200)&(a[:,:,1]>170)&(a[:,:,2]<90)).sum())
log=open(f'{R}/smoke.txt','rb').read().decode('utf-8','replace')
try: yl,y0=yellow('TV_leave'),yellow('F_00')
except Exception as e: print('HUD TVLEAVE GATE: FAIL нет снимка', e); sys.exit(1)
hold='MENUHOLD hold=1' in log
print(f'жёлтых px в верхних 90 строках: TV_leave {yl}, F_00 (до нажатия) {y0}; MENUHOLD в журнале: {hold}')
fails=[]
if y0<300: fails.append('гейт слеп: в меню до нажатия жёлтого заголовка нет')
if yl>=40: fails.append(f'в кадре после подлёта виден заголовок меню ({yl} px)')
if not hold: fails.append('нет MENUHOLD hold=1')
print('TVLEAVE 1169 GATE:', 'FAIL ' + '; '.join(fails) if fails else 'PASS'); sys.exit(1 if fails else 0)
PY
