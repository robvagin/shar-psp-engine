#!/bin/bash
# ps2-twin P05:25: «занято» у будки — хост-тест на Маке + проверка проводки (вызовы есть в сведённом коде) + банк звука
cd "$(dirname "$0")/../.."
export PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_phonebusy}; mkdir -p "$O"; D=${MSN_DIR:-$HOME/Downloads/shar-psp/build/missions}
M=""; for n in 1 2 3 4 5 6 7; do M="$M $D/level0$n.msn"; done
B="cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission -Iviewer"; S="tests/g07b_phonebusy/phonebusy_check.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
$B $S -lm -o "$O/pb_new" || exit 1; $B -DPHONEBUSY_OLD=1 $S -lm -o "$O/pb_old" || exit 1
"$O/pb_new" $M | grep -E "FAIL|OK:|coverage"; POS=${PIPESTATUS[0]}
"$O/pb_old" $M >/dev/null && { echo "красный контроль НЕ упал"; exit 1; } || echo "красный (PHONEBUSY_OLD=1): упал, как должен"
# проводка: без вызывающего API не считается (ошибка 7 навыка)
w=0
grep -q 'pbPress(&pbArm' viewer/main_actbooth.h && grep -q 'uiSfxPlayNamed("phonebusy")' viewer/main_actbooth.h || { echo "FAIL: нет вызова гудка в main_actbooth.h"; w=1; }
grep -q 'pbStep(&pbArm' viewer/main_loop.h && grep -q 'pbHint(&pbArm' viewer/main_loop.h || { echo "FAIL: нет pbStep/pbHint в main_loop.h"; w=1; }
grep -q 'gameMsnBoothsOff' viewer/msn_view.c && grep -q 'pbMissionOff' viewer/game_msn.c || { echo "FAIL: нет пути миссия -> будки выключены"; w=1; }
grep -q "if os.environ.get('PHONEBUSY_OLD') == '1': drop.add('phonebusy')" tools/sound_export.py || { echo "FAIL: клип phonebusy всё ещё убран из банка"; w=1; }
[ $w = 0 ] && echo "проводка: вызовы на месте"
[ "$POS" = 0 ] && [ $w = 0 ] && echo PASS || { echo FAIL; exit 1; }
