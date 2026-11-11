#!/bin/bash
# Гейт №1245 «в последнем кадре наезда камеры на телевизор (RESUME GAME) нет заголовка меню со стрелками»: безголовый тур-EBOOT (RELEASE+KEEP_SMOKE), набор данных, сохранение (SMOKE_SAVEFILES).
#   tests/menu/tvleave1245_gate.sh EBOOT_tour.PBP DATA_DIR SAVE.bin [red]
# Снимок TV_flyend — последний кадр перелёта «туда» (он остаётся на экране на время загрузки). Жёлтых пикселей заголовка/надписи в верхних 90 строках: меню до нажатия F_00 > 300 (контроль чувствительности),
# TV_flyend < 40. Режим red: сценарий с «flag tvlastold 1» (прежнее поведение той же сборки) — гейт обязан упасть (TV_flyend > 300). EBOOT с -DTVLEAVE1245_OLD=1 в обычном режиме тоже обязан упасть
set -u; cd "$(dirname "$0")/../.."
EB=${1:?EBOOT}; SRC=${2:?DATA}; SAVE=${3:?SAVE}; MODE=${4:-green}; T=$PWD/build/tvgate1245_$$; trap 'rm -rf "$T"' EXIT
mkdir -p "$T"; cp -cR "$SRC" "$T/pkg" 2>/dev/null || cp -R "$SRC" "$T/pkg"; rm -f "$T/pkg/EBOOT.PBP"; cp "$EB" "$T/pkg/EBOOT.PBP"
if [ "$MODE" = red ]; then sed 's/^TVLASTOLD$/flag tvlastold 1/' tests/menu/tvleave1245_scenario.txt > "$T/scn.txt"; else grep -v '^TVLASTOLD$' tests/menu/tvleave1245_scenario.txt > "$T/scn.txt"; fi
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge 40 ]; do sleep 20; done   # свой один эмулятор, старт при нагрузке Мака < 40
SMOKE_HEADLESS=1 SMOKE_SAVEFILES="$SAVE=save00.bin" SMOKE_SCN=$T/scn.txt SMOKE_MAXT=150 nice -n 20 tools/smoke.sh "$T/pkg" > "$T/run.out" 2>&1
R=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$T/run.out"); [ -n "$R" ] || { echo "FAIL: нет отчёта: $(head -1 "$T/run.out")"; exit 1; }
python3 -I - "$R" "$MODE" <<'PY'
import sys
from PIL import Image
import numpy as np
R,MODE=sys.argv[1:3]
def yellow(n):
    a=np.asarray(Image.open(f'{R}/{n}.png').convert('RGB'),int)[:90]
    return int(((a[:,:,0]>200)&(a[:,:,1]>170)&(a[:,:,2]<90)).sum())
log=open(f'{R}/smoke.txt','rb').read().decode('utf-8','replace')
try: ye,y0=yellow('TV_flyend'),yellow('F_00')
except Exception as e: print('TVLEAVE 1245 GATE: FAIL нет снимка', e); sys.exit(1)
print(f'жёлтых px в верхних 90 строках: TV_flyend {ye}, F_00 (до нажатия) {y0}; режим {MODE}')
fails=[]
if y0<300: fails.append('гейт слеп: в меню до нажатия жёлтого заголовка нет')
if MODE=='red':
    if ye<300: fails.append('красный контроль не поймал: прежнее поведение, а заголовка в последнем кадре нет')
    print('TVLEAVE 1245 RED:', 'FAIL ' + '; '.join(fails) if fails else 'PASS (прежнее поведение поймано)')
else:
    if ye>=40: fails.append(f'в последнем кадре перелёта виден заголовок меню ({ye} px)')
    print('TVLEAVE 1245 GATE:', 'FAIL ' + '; '.join(fails) if fails else 'PASS')
sys.exit(1 if fails else 0)
PY
