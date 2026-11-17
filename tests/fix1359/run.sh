#!/bin/bash
# Гейт 1359 (данные набора 108, один эмулятор за раз): пп. 1, 2, 3, 6 аудита «миссия против обычного мира». tests/fix1359/run.sh [SET]; EB — viewer/tfix/EBOOT.PBP (tour_build tfix, tools/gate_env.sh)
# п.1 m6.scn: L5 m6, cPolice гонится (chase) на этапе losetail; после этапа новое паркует (STAGEEND cPolice, режим не chase), красный stageendname1359old — гонится дальше.
# п.2 park.scn: L4 m1 этап 1.1.2, машина игрока на припаркованной cPolice; новое dmg 0.00, красный parkdmgname1359old — урон > 0.
# п.3 m1.scn: L4 m1 до конца миссии; новое — машина миссии cPolice уходит через очередь (MCDONE, MCREMOVE why=leaveQ), красный msndonepol1359old — MCREMOVE why=mission-done на глазах.
# п.6 wcar.scn: L4, стоящая полиция H&R в 5 м от места atv_v; новое — свободная машина не уходит (wcar-away 0), красный wcarpol1359old — уходит.
set -u; cd "$(dirname "$0")/../.."
EB=${EB:-viewer/tfix/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/fix1359; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-1}
run() { SMOKE_HEADLESS=1 FLAGS="$3" tools/emu_run.sh tests/fix1359/$1.scn $2 $EB 600 >/dev/null 2>&1; echo $O/out_$2/smoke.txt; }
fail=0; chk() { if [ "$2" = ok ]; then echo "  ok   $1"; else echo "  FAIL $1"; fail=1; fi; }
F=$(run m6 m6_new "ballast=5"); n=$(grep -c '^STAGEEND cPolice' $F); nm=$(grep '^MCAR.*cPolice' $F | tail -1 | grep -o 'mode=[a-zA-Z]*')
F=$(run m6 m6_old "ballast=5 stageendname1359old"); o=$(grep -c '^STAGEEND cPolice' $F); om=$(grep '^MCAR.*cPolice' $F | tail -1 | grep -o 'mode=[a-zA-Z]*')
echo "п.1 L5 m6: новое STAGEEND=$n $nm, прежнее STAGEEND=$o $om"; chk "п.1 новое паркует" $([ "$n" -ge 1 ] && [ "$nm" != "mode=chase" ] && echo ok); chk "п.1 красный: прежнее гонится" $([ "$o" -eq 0 ] && [ "$om" = "mode=chase" ] && echo ok)
F=$(run park park_new "ballast=5"); n=$(grep '^MCAR.*cPolice' $F | grep -o 'dmg=[0-9.]*' | sort -u | grep -vc 'dmg=0.00')
F=$(run park park_old "ballast=5 parkdmgname1359old"); o=$(grep '^MCAR.*cPolice' $F | grep -o 'dmg=[0-9.]*' | sort -u | grep -vc 'dmg=0.00')
echo "п.2 L4 m1: значений урона >0: новое $n, прежнее $o"; chk "п.2 новое без урона" $([ "$n" -eq 0 ] && echo ok); chk "п.2 красный: прежнее с уроном" $([ "$o" -ge 1 ] && echo ok)
F=$(run m1 m1_new "ballast=5"); n=$(grep -c 'MCREMOVE cPolice why=mission-done' $F); nq=$(grep -c 'MCREMOVE cPolice why=leaveQ' $F)
F=$(run m1 m1_old "ballast=5 msndonepol1359old"); o=$(grep -c 'MCREMOVE cPolice why=mission-done' $F)
echo "п.3 L4 m1: mission-done: новое $n (через очередь: $nq), прежнее $o"; chk "п.3 новое через очередь" $([ "$n" -eq 0 ] && [ "$nq" -ge 1 ] && echo ok); chk "п.3 красный: прежнее сносит на месте" $([ "$o" -ge 1 ] && echo ok)
F=$(run wcar wcar_new "ballast=5"); n=$(awk '/^TELEPORT.*kind=foot/{f=1} f&&/MCREMOVE atv_v why=wcar-away/{c++} END{print c+0}' $F)
F=$(run wcar wcar_old "ballast=5 wcarpol1359old"); o=$(awk '/^TELEPORT.*kind=foot/{f=1} f&&/MCREMOVE atv_v why=wcar-away/{c++} END{print c+0}' $F)
echo "п.6 L4: atv_v ушла от копа: новое $n, прежнее $o"; chk "п.6 новое стоит" $([ "$n" -eq 0 ] && echo ok); chk "п.6 красный: прежнее уходит" $([ "$o" -ge 1 ] && echo ok)
[ $fail = 0 ] && echo "ГЕЙТ tests/fix1359: PASS" || { echo "ГЕЙТ tests/fix1359: FAIL"; exit 1; }
