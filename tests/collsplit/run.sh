#!/bin/bash
# Пропечка-96 (Fable шаг 11, COLLSPLIT): tests/collsplit/run.sh НАБОР   (папка с EBOOT.PBP новой сборки и данными набора 96; уровни 2-7 - SHIP_LEVELS)
#   1) tools/collect_check.py: вершины каждого типа гэга - сплошной отрезок, типы не пересекаются (красный контроль --selftest)
#   2) L4, ballast 5, «куча свободно после загрузки» (LEVEL ev=load heap=): NEW (вершины гэгов по типу) против OLD (файл collsplitold, как Build 95); NEW обязан дать больше на >= 500 КБ, COLL ok split=1
#   3) гэги играют: сценарий tests/collect/emu_scenario.txt на L1 (оса, барбекю во дворе, ролик в Kwik-E-Mart): снимок gag_bbq NEW против OLD - разница только в частицах (<= 400 точек), нет WATCHDOG mem / MEMFAIL / COLL error
# Эмулятор один, nice 20. Последняя строка PASS/FAIL.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор}; cd $W; export SHIP_LEVELS="2 3 4 5 6 7" PATH=$W/.venv/bin:$PATH
[ -x /Library/Developer/CommandLineTools/usr/bin/git ] && export DEVELOPER_DIR=/Library/Developer/CommandLineTools
OUT=${OUT:-$W/build/collsplit}; mkdir -p $OUT; rc=0
python3 tools/collect_check.py --selftest | tail -1 | grep -q PASS || { echo "collect_check selftest: FAIL"; rc=1; }
run() { # имя сценарий maxt флаги
  SMOKE_FLAGS="camdet $4" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN=$2 SMOKE_MAXT=$3 nice -n 20 tools/smoke.sh $SET > $OUT/$1.out 2>&1; ls -d build/smoke/* | tail -1 > $OUT/$1.dir; }
run l4new $H/l4_idle.txt 240 ""; run l4old $H/l4_idle.txt 240 collsplitold
hp() { grep -a 'LEVEL ev=load n=4' $(cat $OUT/$1.dir)/smoke.txt | sed -n 's/.* heap=\([0-9]*\) .*/\1/p' | head -1; }
HN=$(hp l4new); HO=$(hp l4old); echo "L4 ballast 5, свободно после загрузки: OLD $HO Б, NEW $HN Б, выигрыш $((HN-HO)) Б"
grep -a 'COLL ok' $(cat $OUT/l4new.dir)/smoke.txt | tail -1 | grep -q 'split=1' || { echo "NEW: split=1 нет"; rc=1; }
grep -a 'COLL ok' $(cat $OUT/l4old.dir)/smoke.txt | tail -1 | grep -q 'split=0' || { echo "OLD: split=0 нет (красный контроль не отличается)"; rc=1; }
[ -n "$HN" ] && [ -n "$HO" ] && [ $((HN-HO)) -ge 512000 ] || { echo "выигрыш < 500 КБ"; rc=1; }
run gnew $W/tests/collect/emu_scenario.txt 200 ""; run gold $W/tests/collect/emu_scenario.txt 200 collsplitold
N=$(cat $OUT/gnew.dir); O=$(cat $OUT/gold.dir)
python3 -I - $N $O <<'P' || rc=1
import sys
from PIL import Image, ImageChops
n, o = sys.argv[1:3]; ok = True
for s in ('wasp_alive', 'gag_bbq', 'loot_fmv'):   # card_got не сравниваем: карточка - не гэг, разница только в фазе позы Гомера (сдвиг кадра от фонового чтения)
    try:
        a, b = Image.open(f'{n}/{s}.png').convert('RGB'), Image.open(f'{o}/{s}.png').convert('RGB')
        if s == 'wasp_alive': a, b = a.crop((200, 50, 300, 130)), b.crop((200, 50, 300, 130))   # оса и машина: слева случайный трафик (автобус против фургона), его не сравниваем
        d = ImageChops.difference(a, b)
    except Exception as e: print(s, 'нет снимка', e); ok = False; continue
    px = sum(1 for p in d.getdata() if max(p) > 8); print(f'{s}: разница {px} точек', 'OK' if px <= 400 else 'FAIL'); ok = ok and px <= 400
sys.exit(0 if ok else 1)
P
# 4) полёт 30 м/с по кругу L1 (в 2 раза быстрее машины) при медленном чтении карты: ни одного гэга ближе 100 м, не готового дольше 1,5 с
run fly $W/tests/load2/fly_l1.txt 200 "slowio flypath"; FL=$(cat $OUT/fly.dir); echo "полёт: гэгов 'COLL ok'/stream: $(grep -a -c 'GAGSTREAM' $FL/smoke.txt) строк GAGSTREAM"
for d in $N $(cat $OUT/l4new.dir) $FL; do nr=$(grep -a -c 'COLLSPLIT notready' $d/smoke.txt); echo "$d: гэгов ближе 100 м не готово: $nr"; [ $nr = 0 ] || rc=1; bad=$(grep -a -E 'WATCHDOG mem|MEMFAIL|COLL error|COLL stream error' $d/smoke.txt | wc -l); echo "$d: WATCHDOG mem / MEMFAIL / COLL error: $bad"; [ $bad = 0 ] || rc=1; done
# 5) красный контроль «не готов»: чтение гэга задержано (flag collhold 2500) - в радиусе ждём: notready >= 1, гэг не запускается от первых двух △, запускается после готовности; без задержки - сразу и notready = 0
run hold $H/hold.txt 300 ""; run ctl $H/control.txt 200 ""
HD=$(cat $OUT/hold.dir); CD=$(cat $OUT/ctl.dir)
t0=$(grep -a '^SCENE gag' $HD/smoke.txt | awk '{print $3}'); th=$(grep -a 'COLLEV gag_start' $HD/smoke.txt | head -1 | sed -n 's/.* t=\([0-9.]*\)$/\1/p'); tc=$(grep -a 'COLLEV gag_start' $CD/smoke.txt | head -1 | sed -n 's/.* t=\([0-9.]*\)$/\1/p')
nh=$(grep -a -c 'COLLSPLIT notready' $HD/smoke.txt); nc=$(grep -a -c 'COLLSPLIT notready' $CD/smoke.txt)
echo "задержка: notready $nh, гэг запущен через $(python3 -c "print(round($th-$t0,1))" 2>/dev/null) с после появления; без задержки: notready $nc, запуск через $(python3 -c "print(round($tc-$t0,1))" 2>/dev/null) с"
[ "$nh" -ge 1 ] && python3 -c "import sys;sys.exit(0 if $th-$t0>=5 else 1)" || { echo "красный контроль не покраснел: гэг запустился до готовности или notready нет"; rc=1; }
[ "$nc" = 0 ] && python3 -c "import sys;sys.exit(0 if $tc-$t0<=3 else 1)" || { echo "контроль без задержки: notready или запуск поздно"; rc=1; }
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
