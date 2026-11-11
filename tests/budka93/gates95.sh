#!/bin/bash
# Интегратор Build 95: все 18 гейтов переходов (15 + bonus, msnsel, menuload с красным контролем) на ОДНОМ EBOOT. tests/budka93/gates95.sh EBOOT [OUT]
# door (Kwik, дом), booth, t03 (double, fail), movie (heal, skip), wardrobe (heal, msg), fsm (N=0,2,5, nofit), end L1 (ДА) + resume. Эмулятор ОДИН за раз (пары убраны 08.11 по просьбе Роба/дирижёра). Последняя строка: ALL PASS / FAIL: список
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; cd $W
E=$(cd "$(dirname "${1:?EBOOT}")" && pwd)/$(basename "$1"); OUT=${2:-$W/build/gates95/$(date +%H%M)}; mkdir -p $OUT/s; export OUT
TOUR=$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15; GAME=$HOME/Downloads/shar-psp-t93/build/test96/PSP/GAME/SHARTEST15
export SHIP_LEVELS="2 3 4 5 6 7" MAXT=900 SMOKE_HEADLESS=1; S=$OUT/s; G=$H
py() { python3 "$@"; }

python3 $G/gen_door.py kwik 3 1.5 > $S/dk.txt; python3 $G/gen_door.py house 3 1.5 > $S/dh.txt; python3 $G/gen_door.py kwik 0 -1 x tffail=2 > $S/f2.txt; python3 $G/gen_door.py kwik 0 -1 x tffail=5 > $S/f5.txt; python3 $G/gen_door.py kwik 0 -1 x tffail=5 cleanold > $S/f5s.txt
python3 $G/gen_door.py kwik 0 -1 x tfnofit > $S/n0.txt; python3 $G/gen_door.py kwik 0 -1 > $S/f0.txt
python3 $G/gen_booth.py 3 1.5 pf=4 > $S/bo.txt; python3 $G/gen_msel.py 4 double > $S/dbl.txt; python3 $G/gen_msel.py 4 fail2 > $S/tf2.txt
python3 $G/gen_movie.py 1 > $S/mv1.txt; python3 $G/gen_movie.py 5 > $S/mv5.txt; python3 $G/gen_shop.py 1 > $S/w1.txt; python3 $G/gen_shop.py 5 cleanold > $S/w5.txt; python3 $G/gen_shop.py 5 > $S/w5r.txt
for c in bonus msnsel menuload; do python3 $G/gen_ext.py $c > $S/x_$c.txt; python3 $G/gen_ext.py $c old > $S/xo_$c.txt; done   # Build 96: газетные классы (18 гейтов)
for m in ok f2 f5 old; do case $m in ok) a="";; f2) a=tf=2;; f5) a=tf=5;; old) a=ldrold;; esac; python3 $G/gen_lvl.py 0 $a > $S/lv_$m.txt; done   # Build 96: LEVEL_NEXT на ldrRun
python3 $G/gen_end.py 1 > $S/e1.txt; sed 's/wait 40\nshot next//' $S/e1.txt > /dev/null
python3 - <<PY
a=open("$S/e1.txt").read().replace("wait 40\nshot next\nwait 20\nshot next2\nend","wait 10\nend"); open("$S/rsA.txt","w").write(a)
open("$S/rsB.txt","w").write("flag menu\nflag saves\nwait 1.5\nshot sv_menu\npress cross\nwait 8\nshot r1\nwait 20\nshot r2\nflag svlog\nend\n")
PY
run() { # имя сценарий набор
  SET=$3 bash $W/tests/budka93/run.sh $1 $2 $E > /dev/null 2>&1; }
export EMU_OWNER=Gates95
run dk $S/dk.txt $TOUR; run dh $S/dh.txt $TOUR
run bo $S/bo.txt $TOUR; run f0 $S/f0.txt $TOUR
run f2 $S/f2.txt $TOUR; run f5 $S/f5.txt $TOUR; run f5s $S/f5s.txt $TOUR
run n0 $S/n0.txt $TOUR; SHIP_LEVELS=5 run dbl $S/dbl.txt $TOUR
SHIP_LEVELS=5 run tf2 $S/tf2.txt $TOUR; run mv1 $S/mv1.txt $TOUR
run mv5 $S/mv5.txt $TOUR; run w1 $S/w1.txt $TOUR
run w5 $S/w5.txt $TOUR; run w5r $S/w5r.txt $TOUR; run e1 $S/e1.txt $GAME
run rs $S/rsA.txt $GAME; run rs $S/rsB.txt $GAME
for m in ok f2 f5 old; do MAXT=900 run lv_$m $S/lv_$m.txt $TOUR; done
for c in bonus msnsel menuload; do MAXT=240 run x_$c $S/x_$c.txt $TOUR; FLAGS="tfsm95old ${FLAGS:-}" MAXT=240 run xo_$c $S/xo_$c.txt $TOUR; done
fail=""; chk() { local n=$1; shift; if py "$@" > $OUT/$n.chk 2>&1; then echo "PASS $n"; else echo "FAIL $n"; fail="$fail $n"; fi; }
O=$OUT
chk door_kwik $G/check_door.py $O/out_dk/smoke.txt new KwikEMart; chk door_house $G/check_door.py $O/out_dh/smoke.txt new SimpsonsHouse
chk booth $G/check_booth.py $O/out_bo/smoke.txt new
chk t03_double $G/check_t03.py $O/out_dbl/smoke.txt double new; chk t03_fail $G/check_t03.py $O/out_tf2/smoke.txt fail new
chk movie_heal $G/check_movie.py $O/out_mv1/smoke.txt heal; chk movie_skip $G/check_movie.py $O/out_mv5/smoke.txt skip
chk ward_heal $G/check_shop.py $O/out_w1/smoke.txt heal; chk ward_msg $G/check_shop.py $O/out_w5/smoke.txt msg; chk ward_reload $G/check_shop.py $O/out_w5r/smoke.txt reload; if py $G/check_shop.py $O/out_w5/smoke.txt reload > $OUT/red_ward.chk 2>&1; then echo "FAIL красный контроль ward_reload не покраснел"; fail="$fail red_ward"; else echo "RED ok ward_reload (без ступени краснеет)"; fi
chk fsm0 $G/check_fsm.py $O/out_f0/smoke.txt KwikEMart 0; chk fsm2 $G/check_fsm.py $O/out_f2/smoke.txt KwikEMart 2; chk fsm5 $G/check_fsm.py $O/out_f5/smoke.txt KwikEMart 5; chk fsm5_simplify $G/check_fsm.py $O/out_f5s/smoke.txt KwikEMart 5 simpl; if py $G/check_fsm.py $O/out_f5s/smoke.txt KwikEMart 5 > $OUT/red_reload.chk 2>&1; then echo "FAIL красный контроль reload не покраснел"; fail="$fail red_reload"; else echo "RED ok reload (без ступени краснеет)"; fi; chk fsm_nofit $G/check_fsm.py $O/out_n0/smoke.txt KwikEMart 0 nofit
chk end1 $G/check_end.py $O/out_e1/smoke.txt 1; chk resume $G/check_resume.py $O/out_rs/smoke.txt new
for c in bonus msnsel menuload; do chk ext_$c $G/check_ext.py $O/out_x_$c/smoke.txt $c new; if py $G/check_ext.py $O/out_xo_$c/smoke.txt $c old > $OUT/red_$c.chk 2>&1; then echo "FAIL красный контроль ext_$c не покраснел"; fail="$fail red_$c"; else echo "RED ok ext_$c (старое краснеет)"; fi; done
for m in ok f2 f5; do chk lvfsm_$m $G/check_lvlfsm.py $O/out_lv_$m/smoke.txt $m; done; if py $G/check_lvlfsm.py $O/out_lv_old/smoke.txt old > $OUT/red_lv.chk 2>&1; then echo "FAIL красный контроль lvfsm не покраснел"; fail="$fail red_lv"; else echo "RED ok lvfsm (прежнее тело краснеет)"; fi
[ -z "$fail" ] && echo "ALL PASS" || echo "FAIL:$fail"
# Правило Роба по диску 08.11: после итога копии и большие журналы не держим (summary, check-файлы, png остаются)
for d in $OUT/emu_home_* $OUT/rr_*; do [ -e "$d" ] && rm -rf "$d"; done
for f in $OUT/out_*/smoke.txt; do [ -f "$f" ] && [ "$(stat -f %z "$f")" -gt 1048576 ] && gzip -9f "$f"; done
rm -f $OUT/out_*/*.bmp
