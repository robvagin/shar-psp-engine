#!/bin/bash
# №1248б (Кольцо-96): мягкий куст на малой скорости (принцип доводки п.4). L7 G5-2: четыре куста-шара на оси съезда двора. Хост-стенд tools/bots/pocket.c, режим thru.
#   tests/pocket1248/bush_gate.sh [DIR с homer.shr, l5/level.shr, l7/level.shr]
# 1) малый ход (старт в 4 м от куста, газ): новое — проезжает кусты в обе стороны (потолок 3,5 м/с у точки), прежнее (сборка -DBUSHSOFT1248_OLD=1) — встаёт у кустов (красное);
# 2) удар на скорости (с востока, старт в 14 м, на кусте > 7 м/с): новое = прежнее строка в строку (правило №1101: −60 % / −25 %);
# (с №1248е объёмы типа 76 во дворе не рисуются и не твёрдые; этот гейт проверяет мягкость на них с -DSPINVIS1248_OLD=1, на обеих сборках)
# 3) твёрдое осталось твёрдым: мировая стена двора (x = −44,9) — не проходят ни новое, ни прежнее; коробка L5 тип 9 — новое = прежнее.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF -DSPINVIS1248_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket" && cc $CF -DSPINVIS1248_OLD=1 -DBUSHSOFT1248_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_bold" || { echo "FAIL: сборка"; exit 1; }
H="$D/homer.shr"; L7="$D/l7/level.shr"; bad=0
t() { "$O/$1" "$2" "$H" thru "${@:3}" 2>&1 | head -1; }
for h in 90 270; do
  n=$(t pocket "$L7" -31.4 -297 $h 4 6); o=$(t pocket_bold "$L7" -31.4 -297 $h 4 6); echo "малый ход курс $h: новое: ${n:0:90}"; echo "                  прежнее: ${o:0:90}"
  case "$n" in *"ПРОШЛА"*) ;; *) echo "FAIL: новое не проехало куст (курс $h)"; bad=1;; esac
  case "$o" in *"НЕ ПРОШЛА"*) ;; *) echo "FAIL: красный контроль не поймал (курс $h): прежнее проехало"; bad=1;; esac
  [ $h = 270 ] || continue   # с запада до куста всего 4 м (стена двора): разгона на удар нет
  n=$(t pocket "$L7" -31.4 -297 $h 14 6); o=$(t pocket_bold "$L7" -31.4 -297 $h 14 6); case "$n" in *"v у точки"*) ;; *) echo "FAIL: нет данных удара: $n"; bad=1;; esac
  [ "$n" = "$o" ] && echo "удар на скорости курс $h: одинаково (${n:0:110})" || { echo "FAIL: на скорости разно: $n | $o"; bad=1; }
done
for b in pocket pocket_bold; do w=$(t $b "$L7" -44.9 -297 270 6 5); case "$w" in *"НЕ ПРОШЛА"*) ;; *) echo "FAIL: стена двора проходима ($b)"; bad=1;; esac; done; echo "стена двора: не проходят ни новое, ни прежнее"
for h in 0 90 180 270; do n=$(PK_Y=8 t pocket "$D/l5/level.shr" 234.6 -107.5 $h 5 4); o=$(PK_Y=8 t pocket_bold "$D/l5/level.shr" 234.6 -107.5 $h 5 4); [ "$n" = "$o" ] || { echo "FAIL: коробка L5 разно (курс $h)"; bad=1; }; done; echo "коробка L5 тип 9: новое = прежнее на 4 курсах"
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
