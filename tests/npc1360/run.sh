#!/bin/bash
# Гейт 1360 п.10 (аудит 1359): AddNPC миссии с именем фонового персонажа не стирает фонового. L4: street race sr3 (в ней Сельма и Пэтти) → следующая миссия gr1 (чистка персонажей миссии) → фоновая Сельма (kind=1, 232,510) на месте.
# Красный: flag npcslot1360old 1 — Сельма пропадает. tests/npc1360/run.sh [SET]; EB — viewer/tnpc/EBOOT.PBP (tour_build tnpc, tools/gate_env.sh)
set -u; cd "$(dirname "$0")/../.."
EB=${EB:-viewer/tnpc/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/npc1360; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-1}
run() { SMOKE_HEADLESS=1 FLAGS="$2" tools/emu_run.sh tests/npc1360/probe.scn $1 $EB 400 >/dev/null 2>&1; echo $O/out_$1/smoke.txt; }
cnt() { LC_ALL=C awk -v sc="$2" '/^SCENE /{f=($2==sc)} f&&/^NPC1360/&&/ selma kind=1 /{n++} END{print n+0}' $1; }
F=$(run new "ballast=5"); n1=$(cnt $F N1); n2=$(cnt $F N2)
F=$(run old "ballast=5 npcslot1360old"); o1=$(cnt $F N1); o2=$(cnt $F N2)
echo "фоновая Сельма (kind=1): во время sr3 новое $n1 прежнее $o1; после миссии новое $n2 прежнее $o2"
[ "$n1" -eq 1 ] && [ "$n2" -eq 1 ] && [ "$o1" -eq 0 ] && [ "$o2" -eq 0 ] && echo "ГЕЙТ tests/npc1360: PASS" || { echo "ГЕЙТ tests/npc1360: FAIL"; exit 1; }
