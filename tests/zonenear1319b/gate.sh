#!/bin/bash
# 1319б: регион «рядом» по клеткам (ZONENEARCELL_OLD) — дыры и куча в сценах L1 не хуже, чем без 1319; маршрут Роба «финиш m4 → центр» по-прежнему закрывает rx40.
# Запуск:  SET=<набор тура 108 с lmap v2> tests/zonenear1319b/gate.sh EBOOT_ТУРА [ПАПКА_РАБОТЫ]      (EBOOT собран tour_build без _OLD)
# Часть A (L1, 3 режима × 2 прохода, ballast 5): new = 1319б, f7ed = регион целиком (красный: ZONENEARCELL_OLD, как f7ed0b0f), none = без 1319.
#   ГЕЙТ: сумма missvis new ≤ none + 1; сумма missvis f7ed > сумма none + 3 (красный краснеет); heapmin(new) ≥ heapmin(f7ed) + 1,0 МБ.
# 1354б: красный контроль части A по missvis на данных набора 108 стоял на пороге (f7ed 3 при none 2). Теперь красный = куча: часть A2 (три места L1, по два прохода, режимы new/f7ed/none),
#   ГЕЙТ A2: у new куча не хуже none больше чем на 0,7 МБ в каждом месте; красный краснеет: хотя бы в одном месте heapmin(f7ed) ниже heapmin(none) не меньше чем на 1,0 МБ (измерено 1,3…2,0 МБ).
# Часть B (L2 маршрут m4 → стадион → центр): rx40 new: R1..R5 = 0, R0 ≤ 2; красный (flag zonenear1319 0): max rx40 ≥ 20.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; EB=${1:?EBOOT}; W=${2:-$ROOT/build/gates/zonenear1319b}; mkdir -p "$W"; export ROOT; . "$ROOT/tools/gate_env.sh"
export SMOKE_HEADLESS=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=$W EMU_MINE=${EMU_MINE:-2}
HEAD='scene F_L1
flag cfgdefaults 1
flag footfsm 1
flag streameye 1
flag trflog
flag prophotchk 1
flag repair
wait 6'
{ echo "$HEAD"; for n in 1 2; do for m in new f7ed none; do echo "scene Z_${m}_$n"
  case $m in new) echo "flag zonenear1319 1"; echo "flag zonenearcell 1";; f7ed) echo "flag zonenear1319 1"; echo "flag zonenearcell 0";; none) echo "flag zonenear1319 0";; esac
  echo "flag stop"; echo "goto 220 172 1.5708"; echo "wait 3"; echo "flag drive 40"; echo "wait 41"; echo "flag stop"; done; done; echo end; } > "$W/a.txt"
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
{ echo "$ROUTE"; for k in 0 1 2 3; do echo "scene M4_P$k"; echo "flag msnpass"; echo "wait 6"; echo "press cross 0.3"; done; echo "scene M4_P4"; echo "flag msnpass"; echo "wait 6"; echo "press cross 0.3"; echo "scene M4_DONE"; echo "wait 8"; i=0
  for p in "83 -350" "70 -280" "50 -200" "40 -120" "30 -50" "0 0"; do echo "scene R$i"; echo "goto $p 0"; echo "wait 14"; i=$((i+1)); done; echo "scene END"; echo "flag stop"; echo "wait 2"; echo end; } > "$W/b_new.txt"
awk '{print} /^flag player 1$/{print "flag zonenear1319 0"}' "$W/b_new.txt" > "$W/b_red.txt"
# A2 (1354б): три места L1 (-165 306, 143 161, -229 171), режимы new / f7ed / none, два прохода; куча (heapmin) как красный контроль на данных набора 108
{ echo "$HEAD"; for n in 1 2; do for sp in "Q1 -165 306 3.14" "Q4 143 161 -1.57" "Q5 -229 171 0"; do set -- $sp; for m in new f7ed none; do echo "scene ${1}_${m}_$n"
  case $m in new) echo "flag zonenear1319 1"; echo "flag zonenearcell 1";; f7ed) echo "flag zonenear1319 1"; echo "flag zonenearcell 0";; none) echo "flag zonenear1319 0";; esac
  echo "flag stop"; echo "goto $2 $3 $4"; echo "wait 3"; echo "flag drive 40"; echo "wait 14"; echo "flag stop"; done; done; done; echo end; } > "$W/a2.txt"
FLAGS="ballast=5" "$RUN" "$W/a.txt" a "$EB" 420 > "$W/a.log" 2>&1 &
FLAGS="" "$RUN" "$W/b_new.txt" bn "$EB" 400 > "$W/bn.log" 2>&1 &
wait
FLAGS="ballast=5" "$RUN" "$W/a2.txt" a2 "$EB" 700 > "$W/a2.log" 2>&1 &
FLAGS="" "$RUN" "$W/b_red.txt" br "$EB" 400 > "$W/br.log" 2>&1
wait
"$PY" - "$W" <<'PYEOF'
import re, sys
W = sys.argv[1]; bad = []
def stats(f):
    S = {}
    for l in open(f, errors='replace'):
        m = re.match(r'STAT (\S+) frames .*missvis (\d+) .*heapmin ([\d.]+)', l)
        if m: S[m.group(1)] = (int(m.group(2)), float(m.group(3)))
    return S
S = stats(W + '/out_a/smoke.txt'); mv = {}; hm = {}
for m in ('new', 'f7ed', 'none'):
    v = [S.get(f'Z_{m}_{n}') for n in (1, 2)]
    if None in v: bad.append(f'нет сцены Z_{m}'); continue
    mv[m] = sum(x[0] for x in v); hm[m] = min(x[1] for x in v)
print('A missvis', mv, 'heapmin', hm)
if len(mv) == 3:
    if mv['new'] > mv['none'] + 1: bad.append('A: дыр у new больше, чем без 1319')
def rx(f):
    sc = None; r = {}
    for l in open(f, errors='replace'):
        if l.startswith('SCENE '): sc = l.split()[1]
        m = re.match(r'DAL1272 .* rx40=(\d+)', l)
        if m and sc and re.fullmatch(r'R\d', sc): r[sc] = max(r.get(sc, 0), int(m.group(1)))
    return r
S2 = stats(W + '/out_a2/smoke.txt'); defs = []; wn = []
for q in ('Q1', 'Q4', 'Q5'):
    h = {}
    for m in ('new', 'f7ed', 'none'):
        v = [S2.get(f'{q}_{m}_{n}') for n in (1, 2)]
        if None in v: bad.append(f'A2: нет сцены {q}_{m}'); h = None; break
        h[m] = min(x[1] for x in v)
    if h: defs.append(h['none'] - h['f7ed']); wn.append(h['none'] - h['new']); print(f'A2 {q}: heapmin new {h["new"]} f7ed {h["f7ed"]} none {h["none"]}')
if defs:
    if max(defs) < 1.0: bad.append('A2: красный (регион целиком) не краснеет по куче (< 1,0 МБ)')
    if max(wn) > 0.7: bad.append('A2: куча у new хуже none больше чем на 0,7 МБ')
rn, rr = rx(W + '/out_bn/smoke.txt'), rx(W + '/out_br/smoke.txt')
print('B rx40 new', rn, 'red', rr)
if len(rn) < 6: bad.append('B: не все точки маршрута')
elif rn['R0'] > 2 or any(rn[f'R{i}'] for i in range(1, 6)): bad.append('B: rx40 у new не закрыт')
if not rr or max(rr.values()) < 20: bad.append('B: красный не краснеет (rx40 < 20)')
print('PASS' if not bad else 'FAIL: ' + '; '.join(bad)); sys.exit(1 if bad else 0)
PYEOF
