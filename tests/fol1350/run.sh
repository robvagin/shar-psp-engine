#!/bin/bash
# Гейт 1350: L4 m1, этап «FOLLOW THE POLICE TO LARD LAD'S»: игрок далеко от точки старта (дом/двор, не рядом с K9-6), цель слежки cPolice должна существовать и ехать.
# Причина: у погони Hit & Run и у машины миссии одно имя модели cPolice; снятие погони (этап 1.1.0 выключает Hit & Run) и сторожа погони убирали по имени и машину миссии.
# Зелёное: после старта этапа 1.1.1 в журнале есть MCAR cPolice с режимом evade не меньше 5 раз, MCREMOVE cPolice нет. Красное (файл hnrpolname1350old): cPolice снята (MCREMOVE ... hnr-off), MCAR cPolice после старта этапа нет.
set -u; cd "$(dirname "$0")/../.."
EB=${EB:-viewer/tfol/EBOOT.PBP}; export SET=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; O=${TMPDIR:-/tmp}/fol1350; mkdir -p $O; export OUT=$O EMU_MINE=${EMU_MINE:-2}
run() { SMOKE_HEADLESS=1 FLAGS="$2" tools/emu_run.sh tests/fol1350/follow.scn $1 $EB 150 >/dev/null 2>&1; f=$O/out_$1/smoke.txt
  echo "$(grep -c '^MCAR .*name=cPolice .*mode=evade' $f) $(grep -c '^MCREMOVE cPolice' $f)"; }
read n_ok n_rm <<< "$(run new "ballast=5")"; read o_ok o_rm <<< "$(run old "ballast=5 hnrpolname1350old")"
echo "новое: кадров cPolice evade=$n_ok, MCREMOVE cPolice=$n_rm; прежнее: evade=$o_ok, MCREMOVE cPolice=$o_rm"
[ "$n_ok" -ge 5 ] && [ "$n_rm" -eq 0 ] && [ "$o_ok" -le 1 ] && [ "$o_rm" -ge 1 ] && echo "ГЕЙТ tests/fol1350: PASS" || { echo "ГЕЙТ tests/fol1350: FAIL"; exit 1; }
