#!/bin/sh
# №1141 / №1159: прогон проверки окна старта камеры + красные контроли. Из корня копии: sh tests/camera/startcam1141_run.sh
# Норма обязана пройти; прежние режимы обязаны не пройти (иначе проверка не свидетель). Выход 0 только если всё так.
SDK=${SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk}; T=${TMPDIR:-/tmp}; rc=0
run() { cc -isysroot "$SDK" -std=c11 -w $1 -I viewer tests/camera/startcam1141_host.c viewer/camsafe.c viewer/col.c -lm -o "$T/s1141_x" || exit 2; "$T/s1141_x" >"$T/s1141_x.txt" 2>&1; return $?; }
run ""; if [ $? -eq 0 ]; then echo "норма: PASS (ждали)"; else echo "норма: FAIL (ОШИБКА)"; cat "$T/s1141_x.txt"; rc=1; fi
for f in -DSTARTCAM1141_OLD=1 -DCAMSTARTGEN1159_OLD=1 -DCAMWINSPLIT_OLD=1; do
  run "$f"; if [ $? -ne 0 ]; then echo "красный $f: упал (ждали): $(grep -c 'FAIL [0-9]' "$T/s1141_x.txt") условий"; else echo "красный $f: ПРОШЁЛ (ОШИБКА: проверка не ловит прежнее)"; rc=1; fi
done
run "-DSTARTCAM1159_OLD=1"; if [ $? -eq 0 ]; then echo "STARTCAM1159_OLD=1 (тот же результат, только цена): PASS"; else echo "STARTCAM1159_OLD=1: FAIL"; rc=1; fi
exit $rc
