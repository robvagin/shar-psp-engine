#!/bin/bash
# Гейт 1360 (оценка Fable Ж2, main_rooms.h:161): при тесной куче (ballast 9, memTight) регион «рядом» не отключает сам себя; маршрут Роба «финиш m4 → центр» (L2, как часть B tests/zonenear1319b) закрывает rx40.
# Тесная куча на Маке не наступает сама (ballast 9 и 13 не дали memTight в нужный момент) — после M4_DONE служебный flag memtight1360 1 включает её принудительно.
# Запуск: SET=<набор 108> tests/zonenear1360/run.sh EBOOT_ТУРА [ПАПКА]. Красный: flag zoneneartight1360old 1 (прежнее) — rx40 открыт на тесной куче.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; EB=${1:?EBOOT}; W=${2:-$ROOT/build/gates/zonenear1360}; mkdir -p "$W"; export ROOT; . "$ROOT/tools/gate_env.sh"
export SMOKE_HEADLESS=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=$W EMU_MINE=${EMU_MINE:-2}
ROUTE='flag menu
flag lvset 2
scene lv_boot
wait 1.5
press cross
waitfor LEVEL ev=load n=2 300
wait 10
flag player 1
flag msn
wait 6
scene M4_AT
flag msnmainat 305
wait 3
press cross 0.3
wait 10'
mk() { { echo "$ROUTE"; [ -n "$1" ] && echo "flag zoneneartight1360old 1"; for k in 0 1 2 3; do echo "scene M4_P$k"; echo "flag msnpass"; echo "wait 6"; echo "press cross 0.3"; done; echo "scene M4_P4"; echo "flag msnpass"; echo "wait 6"; echo "press cross 0.3"; echo "scene M4_DONE"; echo "wait 8"; echo "flag memtight1360 1"; echo "wait 3"; i=0
  for p in "83 -350" "70 -280" "50 -200" "40 -120" "30 -50" "0 0"; do echo "scene R$i"; echo "goto $p 0"; echo "wait 14"; i=$((i+1)); done; echo "scene END"; echo "flag stop"; echo "wait 2"; echo end; } > "$2"; }
mk "" "$W/new.txt"; mk 1 "$W/old.txt"
FLAGS="ballast=${BALLAST:-5}" "$RUN" "$W/new.txt" new "$EB" 400 > "$W/new.log" 2>&1
FLAGS="ballast=${BALLAST:-5}" "$RUN" "$W/old.txt" old "$EB" 400 > "$W/old.log" 2>&1
"$PY" - "$W" <<'PYEOF'
import re, sys
W = sys.argv[1]
def rx(f):
    sc = None; r = {}; tight = 0
    for l in open(f, errors='replace'):
        if l.startswith('SCENE '): sc = l.split()[1]
        if 'tight=1' in l: tight += 1
        m = re.match(r'DAL1272 .* rx40=(\d+)', l)
        if m and sc and re.fullmatch(r'R\d', sc): r[sc] = max(r.get(sc, 0), int(m.group(1)))
    return r, tight
rn, tn = rx(W + '/out_new/smoke.txt'); ro, to = rx(W + '/out_old/smoke.txt')
print('rx40 новое', rn, 'tight-строк', tn); print('rx40 прежнее', ro, 'tight-строк', to)
bad = []
if len(rn) < 6 or len(ro) < 6: bad.append('не все точки маршрута')
if tn == 0: bad.append('память не тесная (tight=1 нет): гейт ничего не проверяет')
if rn and (rn.get('R0', 0) > 2 or any(rn.get(f'R{i}', 0) for i in range(1, 6))): bad.append('новое: rx40 не закрыт при тесной куче')
if ro and max(ro.values()) < 20: bad.append('красный не краснеет (rx40 прежнего < 20)')
print('PASS' if not bad else 'FAIL: ' + '; '.join(bad)); sys.exit(1 if bad else 0)
PYEOF
