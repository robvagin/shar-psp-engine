#!/bin/bash
# Гейт правки 1230 (Роб, TEST93 L5 E3-5: Гомер бежит по косой вдоль перил, перила останавливают намертво; должен скользить).
#   tests/rail1230/gate.sh [LVL_ROOT]  -> PASS/FAIL. LVL_ROOT — папка с L1..L7/{level.shr,player.shr} (по умолчанию последняя build/bots/missions7/*/in основной копии)
# Красный: сборка с -DHOMER_RUNSLIDE1230_OLD=1 (бег гасит скорость хода стеной, как было). Зелёный: по умолчанию.
# Метрика: Гомер бежит (○) на звено забора под углом к плоскости стены, 3.5 с; «скольжение» = скорость вдоль стены в 8..38 кадрах после касания / (8·cos угла).
# Берутся контакты, начатые на 15..140 кадре (старт не в стене). Звенья: E3-5 L5 (546 и 598 — поручни площадки Monorail; 547 и 549 упираются в угол с третьим звеном, коридор для замера закрыт) и по --auto-seg N на каждом из L1..L7.
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
N=${RAIL_N:-60}; O=build/tmp/rail1230; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/rail1230/rail_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green ""; bld $O/red "-DHOMER_RUNSLIDE1230_OLD=1"
pl() { [ -f $LVL/L$1/player.shr ] && echo player.shr || echo homer.shr; }   # у L1 герой — homer.shr
run() { # $1 бинарь, $2 метка: все семь уровней + E3-5
  for L in 1 2 3 4 5 6 7; do (cd $LVL/L$L && $ROOT/$1 level.shr $(pl $L) --auto-seg $N); done > $O/$2.txt
  (cd $LVL/L5 && for s in 546 598; do $ROOT/$1 level.shr $(pl 5) --seg $s --dist 1.5; done) > $O/$2_e35.txt
}
run $O/green green; run $O/red red
stat() { # файл -> строки "угол n среднее_отношение скольжение скв"
  awk -F'\t' '$1=="R"{split($4,a," "); split($5,c," "); split($6,s," "); split($7,i," "); if (c[2]>=15 && c[2]<=140) { A=a[2]; n[A]++; if (A<=60) m[A]+=s[2]/i[2]; else m[A]+=s[2]; x[A]+=($8=="crossed 1") }}
       END{for (A in n) printf "%d %d %.3f %d\n", A, n[A], m[A]/n[A], x[A]}' "$1" | sort -n
}
bad=0
for t in green red green_e35 red_e35; do echo "== $t"; stat $O/$t.txt | awk '{printf "  угол %2d: контактов %3d  %s %.2f  сквозь %d\n",$1,$2,($1<=60?"скольжение (доля идеала)":"скорость вдоль, м/с"),$3,$4}'; done
chk() { # метка файл угол условие(awk по $3)
  v=$(stat $2 | awk -v A=$3 '$1==A{print $3}'); [ -n "$v" ] || { echo "FAIL: $1: нет контактов под $3°"; bad=1; return; }
  awk -v v=$v "BEGIN{exit !($4)}" || { echo "FAIL: $1 под $3°: $v (ждали $4)"; bad=1; }
}
for f in green green_e35; do for A in 30 60; do chk "зелёный $f" $O/$f.txt $A "v>=0.8"; done; done
chk "красный (OLD) стоит" $O/red.txt 30 "v<=0.5"; chk "красный (OLD) стоит" $O/red.txt 60 "v<=0.3"
chk "упор 90° зелёный" $O/green.txt 90 "v<=1.2"; chk "упор 85° зелёный" $O/green.txt 85 "v<=2.0"   # у забора, скошенного к осям, выталкивание по оси тела даёт малый увод вдоль (ниже 1,2 м/с из 8)
for f in green green_e35; do s=$(stat $O/$f.txt | awk '{x+=$4} END{print x+0}'); r=$(stat $O/${f/green/red}.txt | awk '{x+=$4} END{print x+0}'); [ "$s" -le "$r" ] || { echo "FAIL: $f: сквозь стену $s раз при $r у красного"; bad=1; }; done
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
