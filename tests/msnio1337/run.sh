#!/bin/bash
# Гейт 1337 (стенд перестроен 1366): tests/msnio1337/run.sh [SET]. Эмулятор безголовый, ballast 5, slowio 100 КБ/с (клетки города читаются долго), flag ioserial1337 1 (одна карта памяти),
# сценарий: телепорт далеко (круг клеток пуст и догружается) и сразу старт миссии. Время = от строки BBOP msn (старт миссии) до последней BBOP art= (все записи миссии прочитаны).
# Зелёное: новое не дольше 2,5 с и быстрее прежнего (файл msnioprio1337old) не меньше чем на 3 с; загрузчик уступал (ioprio > 0). Красное: прежнее ≥ 4 с (на 108-данных: 6,4 с против 1,0 с).
set -u; cd "$(dirname "$0")/../.."
EB=${EB:-viewer/t1337/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/msnio1337; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-2}
run() { SMOKE_HEADLESS=1 FLAGS="$2" tools/emu_run.sh tests/msnio1337/scenario.txt $1 $EB 200 >/dev/null 2>&1; }
span() { python3 -I - "$O/out_$1/smoke.txt" <<'PY'
import re, sys
L = list(open(sys.argv[1], errors='replace')); T = lambda l: float(re.search(r't=([\d.]+)', l).group(1))
m = [T(l) for l in L if l.startswith('BBOP msn ')]; a = [T(l) for l in L if l.startswith('BBOP art=')]
print(round(a[-1] - m[0], 2) if m and a else 'нет')
PY
}
run new "ballast=5 slowio=100"; run old "ballast=5 slowio=100 msnioprio1337old"
n=$(span new); o=$(span old); h=$(grep -o "ioprio=[0-9]*/[0-9]*ms" $O/out_new/smoke.txt | tail -1); h0=$(grep -o "ioprio=[0-9]*/[0-9]*ms" $O/out_old/smoke.txt | tail -1)
echo "чтение записей миссии (старт миссии до последнего art=), с: новое $n, прежнее $o; удержания загрузчика: новое $h, прежнее $h0"
case "$n$o" in *нет*) echo "ГЕЙТ tests/msnio1337: FAIL (нет строк BBOP msn / art=)"; exit 1;; esac
case "$h" in ioprio=0/*|"") echo "ГЕЙТ tests/msnio1337: FAIL (новое: загрузчик не уступал)"; exit 1;; esac
case "$h0" in ioprio=0/0ms) ;; *) echo "ГЕЙТ tests/msnio1337: FAIL (прежнее уступало)"; exit 1;; esac
awk -v o="$o" 'BEGIN { exit !(o >= 4.0) }' || { echo "ГЕЙТ tests/msnio1337: FAIL (красного нет: прежнее $o с < 4 — на стенде нет конкуренции)"; exit 1; }
awk -v n="$n" -v o="$o" 'BEGIN { exit !(n <= 2.5 && n <= o - 3.0) }' && echo "ГЕЙТ tests/msnio1337: PASS (новое $n с, прежнее $o с)" || { echo "ГЕЙТ tests/msnio1337: FAIL (новое $n с не быстрее прежнего $o с на 3 с или дольше 2,5 с)"; exit 1; }
