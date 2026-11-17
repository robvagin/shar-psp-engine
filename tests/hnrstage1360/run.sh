#!/bin/bash
# Гейт 1360 (Fable Ж4, main_frontend.h hnrPursuit): шкала Hit & Run не сбрасывается на смене этапа внутри миссии. tests/hnrstage1360/run.sh [SET]; EB — viewer/tf2/EBOOT.PBP (tour_build tf2)
# Новое: после P0 и до P2 (пока миссия идёт) строк «HNR msnreset win» нет, погоня (latch=1) держится; красный (flag hnrstagepause1360old 1): win/start на каждой смене этапа, погоня снимается.
set -u; cd "$(dirname "$0")/../.."
EB=${EB:-viewer/tf2/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/hnrstage1360; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-1}
run() { SMOKE_HEADLESS=1 FLAGS="$2" tools/emu_run.sh tests/hnrstage1360/stage.scn $1 $EB 400 >/dev/null 2>&1; echo $O/out_$1/smoke.txt; }
stat() { LC_ALL=C awk '/^SCENE P0/{f=1} /^SCENE P2/{f=0} f&&/^HNR msnreset win/{w++} f&&/^HNRV/&&/police=[1-9]/{h++} END{print w+0, h+0}' $1; }
F=$(run new "ballast=5"); read rn hn <<< "$(stat $F)"
F=$(run old "ballast=5 hnrstagepause1360old"); read ro ho <<< "$(stat $F)"
echo "до сцены P2 (этапы 1.1.2 → 1.1.6): сбросов «win» новое $rn, прежнее $ro; секунд с полицией рядом (police>=1): новое $hn, прежнее $ho"
[ "$rn" -eq 0 ] && [ "$ro" -ge 2 ] && [ "$hn" -ge 5 ] && [ "$ho" -lt "$hn" ] && echo "ГЕЙТ tests/hnrstage1360: PASS" || { echo "ГЕЙТ tests/hnrstage1360: FAIL"; exit 1; }
