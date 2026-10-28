#!/bin/bash
# Стенд езды по городу L3–L7 (№984, окно «Оптимизация»): боты на CPU Мака, настоящие модули движка, без AI/API.
#   tools/bots/city984.sh run  ИМЯ [--levels "3 4 5 6 7"] [--routes 6] [--secs 60] [--seed 1] [--prof]   бот-автопилот по маршрутам: след машины и трафика, фазы кадра -> build/city984/ИМЯ/
#   tools/bots/city984.sh cmp  A B                         сравнение двух прогонов «до/после»: след и трафик побитно (код 0 — поведение не изменилось), иначе первый кадр расхождения и наибольшее отклонение; фазы мкс/кадр рядом
#   tools/bots/city984.sh emu  ИМЯ EBOOT [--levels "…"] [--routes 3] [--secs 25]   те же стартовые точки в эмуляторе (flag drive, тур-EBOOT): PHSEC по маршрутам (build, x_city, x_props, x_trf, ph1…) -> build/city984/ИМЯ_emu/, таблица
#   tools/bots/city984.sh emucmp A B                       таблица средних PHSEC двух прогонов эмулятора, разница ×5,8 (≈ PSP) по участкам кадра
# Окружение: DATA — набор данных (по умолчанию тур test68: ~/Downloads/shar-psp-integ/build/test68tour/PSP/GAME/SHARTEST15; L1 в корне, l2…l7 в подпапках); V_DIR — папка viewer/ другого дерева («после»: сборка бота
# из дерева окна, стенд и данные те же): V_DIR=~/Downloads/shar-psp-t68-camray/viewer tools/bots/city984.sh run after. SDKROOT на Маке без принятой лицензии Xcode:
# export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk и PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH. Цена бота — мкс на Маке нативно (сотни раз быстрее PSP): годится для сравнения «до/после»,
# цену кадра PSP даёт строка emu (×5,8 по PHSEC). Бот не рисует: камера, список GE, стейтпропы-цикл рисования — только в режиме emu.
set -e; cd "$(dirname "$0")/../.."; ROOT=$PWD; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
D=${DATA:-$HOME/Downloads/shar-psp-integ/build/test68tour/PSP/GAME/SHARTEST15}; OUTR=${OUTR:-$ROOT/build/city984}; mkdir -p "$OUTR"
cmd=$1; shift || true
opt() { local k=$1 def=$2; shift 2; while [ $# -gt 0 ]; do [ "$1" = "$k" ] && { echo "$2"; return; }; shift; done; echo "$def"; }
lvfiles() { local L=$1; if [ "$L" = 1 ]; then echo "$D/level.shr $D/homer.shr $D/level01.msn"; else echo "$D/l$L/level.shr $D/l$L/player.shr $D/l$L/level0$L.msn"; fi; }
case "$cmd" in
run)
  N=$1; shift; LV=$(opt --levels "3 4 5 6 7" "$@"); RT=$(opt --routes 6 "$@"); SC=$(opt --secs 60 "$@"); SD=$(opt --seed 1 "$@"); PR=""; [[ " $* " == *" --prof "* ]] && PR="--prof"
  O=$OUTR/$N; rm -rf "$O"; mkdir -p "$O"; BIN=$O/bin; mkdir -p "$BIN"
  V_DIR=${V_DIR:-viewer} OUT_DIR=$BIN BINS=city984 bash tools/bots/build.sh >/dev/null 2>"$O/build.log" || { echo "сборка не прошла: $O/build.log"; tail -5 "$O/build.log"; exit 1; }
  : > "$O/summary.txt"
  for L in $LV; do BW_LEVEL=$L "$BIN/city984" $(lvfiles $L) --routes $RT --secs $SC --seed $SD --trace "$O/trace" $PR | tee -a "$O/summary.txt" | grep -E "ИТОГ|FAIL|нет "; done
  echo "-> $O (summary.txt, trace/L<N>_r<K>.tsv)";;
cmp)
  A=$OUTR/$1; B=$OUTR/$2; python3 - "$A" "$B" <<'PYEOF'
import sys, re, glob, os
A, B = sys.argv[1], sys.argv[2]; bad = 0; n = 0; skipped = 0
def load(fn): return [l.split('\t') for l in open(fn)]
for fa in sorted(glob.glob(A + '/trace/*.tsv')):
    name = os.path.basename(fa); fb = B + '/trace/' + name; n += 1
    if not os.path.exists(fb): skipped += 1; n -= 1; continue   # второй прогон был по другим уровням: сравниваем общие маршруты
    a, b = load(fa), load(fb)
    if a == b: continue
    k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
    dm = max((max(abs(float(x) - float(y)) for x, y in zip(a[i][:3], b[i][:3])) for i in range(min(len(a), len(b)))), default=0)
    print('  %-12s РАЗНЫЕ: первый кадр %d, наибольшее отклонение положения %.4f м' % (name, k, dm)); bad += 1
def ph(d):
    r = {}
    for l in open(d + '/summary.txt'):
        m = re.match(r'ИТОГ L(\d+): кадров \d+, ([\d.]+) мкс/кадр(?: \| фазы мкс/кадр: машина ([\d.]+), мир ([\d.]+), респавн/комнаты ([\d.]+), трафик ([\d.]+), H&R ([\d.]+))?', l)
        if m: r[m.group(1)] = [float(x) for x in m.groups()[1:] if x]
    return r
pa, pb = ph(A), ph(B)
print('маршрутов сравнено %d (пропущено без пары %d), расхождений %d -> %s' % (n, skipped, bad, 'ПОВЕДЕНИЕ НЕ ИЗМЕНИЛОСЬ (побитно)' if not bad and n else 'ИЗМЕНИЛОСЬ'))
for L in sorted(pa):
    if L in pb: print('  L%s мкс/кадр (всего, [машина, мир, респавн, трафик, H&R]): до %s, после %s' % (L, pa[L], pb[L]))
sys.exit(1 if bad or not n else 0)
PYEOF
  ;;
emu)
  N=$1; EB=$2; shift 2; LV=$(opt --levels "3 4 5 6 7" "$@"); RT=$(opt --routes 3 "$@"); SC=$(opt --secs 25 "$@"); O=$OUTR/${N}_emu; rm -rf "$O"; mkdir -p "$O"
  BINB=$OUTR/_emu_bin; mkdir -p "$BINB"; V_DIR=viewer OUT_DIR=$BINB BINS=city984 bash tools/bots/build.sh >/dev/null 2>&1
  for L in $LV; do BW_LEVEL=$L "$BINB/city984" $(lvfiles $L) --routes $RT --secs 1 > "$O/starts_L$L.txt" || true
    python3 - "$O" "$L" "$SC" <<'PYEOF'
import re, sys
O, L, SC = sys.argv[1], sys.argv[2], sys.argv[3]
import os
o = ['scene C_BOOT', 'flag cfgdefaults 1'] + [x for x in os.environ.get('EXTRA', '').split(';') if x] + ['flag repair', 'wait 6', 'flag toon 1', 'wait 3', 'scene C_GO', 'press circle 0.3', 'wait 0.5', 'press circle 0.3', 'wait 0.5', 'flag golevel %s' % L, 'wait 40', 'press triangle 0.2', 'wait 3']
for l in open('%s/starts_L%s.txt' % (O, L)):
    m = re.match(r'ROUTE L(\d+) r(\d+): старт \((-?\d+),(-?\d+)\) курс (-?[\d.]+)', l)
    if not m: continue
    k = m.group(2); o += ['flag stop', 'scene CITY_L%s_r%s' % (L, k), 'goto %s %s %s' % (m.group(3), m.group(4), m.group(5)), 'wait 3', 'flag drive %d' % int(SC), 'wait %d' % (int(SC) + 1)]
o += ['flag stop', 'end']; open('%s/scn_L%s.txt' % (O, L), 'w').write('\n'.join(o) + '\n')
PYEOF
  done
  export EMU_OWNER=${EMU_OWNER:-Оптимизация} EMU_MINE=${EMU_MINE:-2} OUT=$O SET=${SET:-$D}
  set -- $LV; while [ $# -gt 0 ]; do A=$1; B=${2:-}; ( tools/emu_run.sh "$O/scn_L$A.txt" L$A "$EB" 900 >/dev/null 2>&1 & [ -n "$B" ] && tools/emu_run.sh "$O/scn_L$B.txt" L$B "$EB" 900 >/dev/null 2>&1 & wait ); shift; [ $# -gt 0 ] && shift; done
  cat "$O"/out_L*/smoke.txt > "$O/smoke_all.txt" 2>/dev/null; python3 tools/bots/city984_emu.py "$O/smoke_all.txt";;
emucmp)
  python3 tools/bots/city984_emu.py "$OUTR/$1_emu/smoke_all.txt" "$OUTR/$2_emu/smoke_all.txt";;
*) sed -n 2,12p "$0"; exit 2;;
esac
