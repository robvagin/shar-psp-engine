#!/bin/bash
# сцена загрузки уровня N и 20 с езды: tests/opcar108/scene_lv.sh N > файл
N=${1:?N}
cat <<S
flag menu
flag lvset $N
wait 1.5
press cross
waitfor LEVEL ev=load n=$N 300
wait 10
flag player
flag msn
wait 3
scene EQ
flag drive 100
wait 25
scene EQ_END
flag stop
wait 2
end
S
