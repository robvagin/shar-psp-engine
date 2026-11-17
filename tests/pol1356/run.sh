#!/bin/bash
# Гейт 1356: L4 m1, этап 1.1.2 «TALK TO CHIEF WIGGUM» (flag msnmainat 2, этап держится, без бота): игрок пешком в 3 м от машины миссии cPolice (m1_cop_stop) 8 с — арест (BUSTED / HNR busted) не должен срабатывать ни разу.
# Причина: арест и погоня H&R считали «полицией» любую машину с именем модели cPolice, в том числе стоящую цель этапа; рядом ближе 10 м и тише 30 км/ч дольше 0,75 с (пешком 1,5 с) — арест, потом по кругу.
# Красный: файл hnrbustname1356old (арест по имени, как было) — BUSTED повторяется. tests/pol1356/run.sh [SET]
set -u; cd "$(dirname "$0")/../.."
# собрать EBOOT: tour_build tpol (tools/gate_env.sh)
EB=${EB:-viewer/tpol/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/pol1356; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-1}
run() { SMOKE_HEADLESS=1 FLAGS="$2" tools/emu_run.sh tests/pol1356/hold.scn $1 $EB 600 >/dev/null 2>&1; grep -c '^HNR busted\|^BUSTED' $O/out_$1/smoke.txt; }
n=$(run new "ballast=5"); o=$(run old "ballast=5 hnrbustname1356old")
echo "арестов: новое $n, прежнее $o"
[ "$n" -eq 0 ] && [ "$o" -ge 2 ] && echo "ГЕЙТ tests/pol1356: PASS" || { echo "ГЕЙТ tests/pol1356: FAIL"; exit 1; }
