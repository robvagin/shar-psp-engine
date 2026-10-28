#!/bin/bash
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
cd ~/Downloads/shar-psp-t73-pano
export SET=~/Downloads/shar-psp-integ/build/test73tour/PSP/GAME/SHARTEST15 EMU_OWNER=Оптимизация EMU_MINE=2 OUT=/tmp/claude-501/p917b/bake
B=/tmp/claude-501/p917b/bake
one() { n=$1; tools/emu_run.sh $B/sc/$n.txt cap_$n /tmp/claude-501/p917b/EBOOT_cap.PBP 1500 > $B/cap_$n.log 2>&1; want=$(grep -c "^shot " $B/sc/$n.txt); got=$(ls $B/out_cap_$n/*.png 2>/dev/null | wc -l); echo "$(date +%H:%M:%S) $n $got/$want" >> $B/progress.log; }
export -f one; export B
cat $B/sc/manifest.txt | xargs -P 2 -I{} bash -c 'one {}'
echo ALLDONE >> $B/progress.log
