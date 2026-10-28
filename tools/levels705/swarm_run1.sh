#!/bin/bash
N=$1; V=$2
if [ $N = 1 ]; then nw=$HOME/Downloads/shar-psp-t75-lv/build/t75lv/out905/level.shr; ol=$HOME/Downloads/shar-psp-integ/build/test74/PSP/GAME/SHARTEST15/level.shr; else nw=$HOME/Downloads/shar-psp-t75-lv/build/t75lv/out905/l$N/level.shr; ol=$HOME/Downloads/shar-psp-integ/build/test74/PSP/GAME/SHARTEST15/l$N/level.shr; fi
[ $V = new ] && f=$nw || f=$ol
$HOME/Downloads/shar-psp-t75-ramp95/build/lv75/pass_swarm $f $HOME/Downloads/shar-psp-t75-ramp95/build/lv75/segs/segs_l$N.txt > $HOME/Downloads/shar-psp-t75-lv/build/t75lv/sw/$V$N.txt 2>&1; echo done > $HOME/Downloads/shar-psp-t75-lv/build/t75lv/sw/$V$N.done
