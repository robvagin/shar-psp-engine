#!/bin/bash
# №1061 (Мультик): колёса машины игрока по умолчанию в level.shr (CAR) после tools/car_tire_splice1061.py. Красный — файлы TEST75 (out905), зелёный — подмена CAR (build/tire1061/car + lmap) с mcpr TEST75.
# Сцена: уровень N, машина игрока на старте, глаз в 4 м сбоку (camrel), мипы -1 (авто) и 3, текстуры вкл. Окно колёс кадра x 40..230, y 100..240.
# Зелёный: пиксели, изменившиеся на мипе 3 (|Δ| > 30 по сумме каналов) — это колёса: у L1, L3, L4, L5, L7 их не меньше 100 и они темнее красного не меньше чем на 3; вблизи (мип -1) в этих же пикселях |Δ| не больше 0,6 от мипа 3 (диск на месте).
# Красный контроль: красный против красного (n3 = 0) не даёт «изменилось ≥ 100» -> FAIL. Сцена: глаз в 6 м сбоку от героя у стартовой машины (camrel 6 1.3 0).
# Запуск: tests/tire1061/player_gate.sh [EBOOT] [уровни]; SET — набор данных; SRC — out905 (по умолчанию ~/Downloads/shar-psp-t75-lv/build/t75lv/out905); CAR — build/tire1061/car. Последняя строка PASS/FAIL
W="$(cd "$(dirname "$0")/../.." && pwd)"; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
EB=${1:-$W/viewer/build_t1061/EBOOT.PBP}; LEVELS=${2:-"1 3 4 5 6 7"}; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74/PSP/GAME/SHARTEST15}
SRC=${SRC:-$HOME/Downloads/shar-psp-t75-lv/build/t75lv/out905}; CAR=${CAR:-$W/build/tire1061/car}; O=${OUT:-$W/build/tire1061/pacc}; mkdir -p "$O"
for L in $LEVELS; do
  [ $L = 1 ] && r="" || r="/l$L"
  rm -rf "$O/ovl_r$L" "$O/ovl_g$L"; mkdir -p "$O/ovl_r$L$r" "$O/ovl_g$L$r"
  cp "$SRC$r/level.shr" "$SRC$r/lmap.shr" "$SRC$r/mcpr.shr" "$O/ovl_r$L$r/"
  cp "$CAR$r/level.shr" "$CAR$r/lmap.shr" "$SRC$r/mcpr.shr" "$O/ovl_g$L$r/"
  sed "s/@N@/$L/" $W/tests/tire1061/player_level.txt > "$O/scn$L.txt"
  ( OVL="$O/ovl_r$L" EMU_OWNER=Мультик EMU_MINE=2 OUT=$O $W/tools/emu_run.sh "$O/scn$L.txt" r$L "$EB" 300 >/dev/null 2>&1 ) &
  ( OVL="$O/ovl_g$L" EMU_OWNER=Мультик EMU_MINE=2 OUT=$O $W/tools/emu_run.sh "$O/scn$L.txt" g$L "$EB" 300 >/dev/null 2>&1 ) &
  wait; rm -rf "$O/rr_r$L" "$O/rr_g$L"
done
python3 -I - "$O" $LEVELS <<'PY'
import sys
from PIL import Image
import numpy as np
o = sys.argv[1]; lv = sys.argv[2:]; ok = True; need = {'1', '3', '4', '5', '7'}   # где перепись нашла не чёрный протектор хотя бы на одном мипе (L2: honor_v, колёса не рисуются; L6: bart_v чёрный)
def fr(n, L, m): return np.asarray(Image.open(f'{o}/out_{n}{L}/m{m}a.png').convert('RGB')).astype(int)[60:272]   # без строк счётчиков
lum = lambda a: (a * [.299, .587, .114]).sum(-1)
for L in lv:
    r3, g3, r1, g1 = fr('r', L, 3), fr('g', L, 3), fr('r', L, -1), fr('g', L, -1)
    d3 = np.abs(g3 - r3).sum(-1) > 30; n3 = int(d3.sum())
    dl = float((lum(g3) - lum(r3))[d3].mean()) if n3 else 0.0
    a3 = float(np.abs(g3 - r3).sum(-1)[d3].mean() / 3) if n3 else 0.0; a1 = float(np.abs(g1 - r1).sum(-1)[d3].mean() / 3) if n3 else 0.0
    print(f'L{L}: мип3 изменилось пикселей {n3}, яркость изменённых green−red {dl:.1f}, |Δ| {a3:.1f}; на мипе -1 в тех же пикселях |Δ| {a1:.1f}')
    if n3 >= 400 and dl > 2: ok = False                         # новое не светлее старого (горстка пикселей диска у L6 не в счёт)
    if L in need and not (n3 >= 100 and dl <= -3): ok = False    # на мипе 3 колесо заметно темнее
    if n3 and a1 > 0.6 * a3: ok = False                          # на мипе -1 (вблизи) колесо то же: картинка диска на месте, меняется вдали
print('PASS' if ok else 'FAIL')
PY
