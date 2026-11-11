#!/bin/bash
# Р3 (тур Роба на 102): наезды старта миссий L1–L7 с диска — глаз не за стеной, не дальше FAR м от игрока. Мак, CPU, без эмулятора.
#   tests/camera/msncam_wall.sh [НАБОР] [FAR=40]    НАБОР — папка с level.shr, msncam.shr (L1) и l2..l7/ (по умолчанию build/test103tour/PSP/GAME/SHARTEST15 соседней копии mig102)
# Красный контроль: тот же путь БЕЗ сдвига от стены (MODE 0) обязан дать кадры за стеной (>0); зелёный: со сдвигом (MODE 1) их 0 на каждом уровне, и глаз ни разу дальше FAR + 2 м от игрока.
cd "$(dirname "$0")/../.." || exit 1
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
M="${1:-$HOME/Downloads/shar-psp-mig102/build/test103tour/PSP/GAME/SHARTEST15}"; FAR="${2:-40}"; mkdir -p build
cc -std=c11 -O1 -w -I viewer tests/camera/msncam_wall_host.c viewer/msncam.c viewer/col.c -lm -o build/mcw || { echo "FAIL: сборка"; exit 2; }
red=0; green=0; far_bad=0
for L in 1 2 3 4 5 6 7; do d=$M; [ $L != 1 ] && d=$M/l$L
  r=$(build/mcw $d/level.shr $d/msncam.shr $FAR 0 | tail -1); g=$(build/mcw $d/level.shr $d/msncam.shr $FAR 1)
  rb=$(echo "$r" | sed -n 's/.*до сдвига \([0-9]*\),.*/\1/p'); ga=$(echo "$g" | tail -1 | sed -n 's/.*после \([0-9]*\),.*/\1/p')
  mx=$(echo "$g" | sed -n 's/.*глаз от игрока до *\([0-9.]*\) м (гориз).*/\1/p' | sort -n | tail -1)
  echo "L$L: без сдвига кадров за стеной $rb (красное), со сдвигом $ga (зелёное), глаз до $mx м от игрока"
  red=$((red + rb)); green=$((green + ga)); awk -v m="$mx" -v f="$FAR" 'BEGIN{exit !(m > f + 2)}' && far_bad=$((far_bad + 1))
done
echo "ИТОГ: красное $red кадров за стеной, зелёное $green, уровней с глазом дальше FAR+2 м: $far_bad"
[ "$red" -gt 0 ] && [ "$green" = 0 ] && [ "$far_bad" = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
