#!/bin/bash
# 1329 (Роб, 108 на PSP: «только что купил школьный автобус, не было звука покупки»). Оригинал: на ✕ в меню покупки наград играет один звук «accept» (событие выбора пункта меню при выборе пункта);
# кассового звука нет (монеты списываются без события). У нас экран лавки машин держит режим «Store» с шиной эффектов в нуле (№1135), а звуки лавки шли на шину эффектов: accept молчал.
# Теперь звуки лавки на шине меню (SHOPSND1329_OLD=1 / flag shopsnd1329old 1 — как было).
# Стенд: L3, Отто (школьный автобус otto_v, flag sellergo 1), △, ✕. Критерий по ЖУРНАЛУ звука: строка «SHOPV snd accept bus=ui lvl=…» после «SHOPV buy», уровень шины > 0,5 (слышно);
# красный (старое): «bus=sfx lvl=0.00». Костюм (гардероб Kwik-E-Mart, шина эффектов в гардеробе жива): после «WARDV buy» строка «SFX accept». Запуск: tests/shop1329/run.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/shop1329g}; mkdir -p $OUT
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2 SHIP_LEVELS="1 2 3 4 5 6 7"
for V in 0 1; do
  { echo "scene start"; echo "flag msn"; echo "flag sfxlog"; echo "wait 3"; echo "scene L3_load"; echo "flag golevel 3"; echo "wait 30"; echo "scene L3_shop"; echo "flag shopsnd1329old $V"; echo "flag coinadd 2000"; echo "wait 6"
    echo "flag sellergo 1"; echo "wait 0.4"; echo "press triangle"; echo "wait 3"; echo "press cross"; echo "wait 5"; echo "end"; } > $OUT/c_$V.scn
  (cd $R && OUT=$OUT FLAGS="" nice -n 20 tools/emu_run.sh $OUT/c_$V.scn c$V "$EB" 400 > $OUT/c$V.log 2>&1) & done
awk '/^scene wardrobe/ { print; print "flag sfxlog"; next } { print }' tests/shop/wardrobe_scenario.txt > $OUT/w.scn
(cd $R && OUT=$OUT FLAGS="" nice -n 20 tools/emu_run.sh $OUT/w.scn w "$EB" 400 > $OUT/w.log 2>&1) &
wait
OUT=$OUT python3 - <<'PY'
import os, re
O = os.environ['OUT']
def lines(n): return open(f'{O}/out_{n}/smoke.txt', errors='replace').read().splitlines() if os.path.exists(f'{O}/out_{n}/smoke.txt') else []
bad = 0
for v, want in ((0, 1), (1, 0)):
    L = lines(f'c{v}'); bi = next((i for i, l in enumerate(L) if l.startswith('SHOPV buy') and 'otto_v' in l), -1)
    if bi < 0: print('FAIL: покупка школьного автобуса не прошла (нет SHOPV buy ... otto_v), v=%d' % v); bad = 1; continue
    s = next((l for l in L[bi - 3:bi + 40] if l.startswith('SHOPV snd accept')), None)
    m = re.search(r'bus=(\w+) lvl=([\d.]+)', s or ''); lvl = float(m.group(2)) if m else -1
    print('%s: школьный автобус, звук покупки: %s' % ('новое' if v == 0 else 'старое', s or 'нет строки'))
    if want and not (m and m.group(1) == 'ui' and lvl > 0.5): print('FAIL зелёный'); bad = 1
    if not want and not (m and lvl < 0.05): print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
W = lines('w'); bi = next((i for i, l in enumerate(W) if l.startswith('WARDV buy')), -1)
a = next((l for l in W[bi:bi + 60] if l.startswith('SFX accept')), None) if bi >= 0 else None
print('костюм: %s / %s' % (W[bi][:60] if bi >= 0 else 'нет WARDV buy', a[:40] if a else 'нет SFX accept'))
if not a: print('FAIL костюм'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
