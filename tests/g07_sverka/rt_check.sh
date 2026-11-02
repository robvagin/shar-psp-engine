#!/bin/bash
# Окно g07-missions, сверка 02.11.2026: рантайм миссий на настоящих данных. Красный контроль — все выключатели *_OLD=1 (прежнее поведение) обязаны упасть.
#   tests/g07_sverka/rt_check.sh [LEVEL.msn ...]  -> PASS/FAIL (данные: build/missions/level0N.msn)
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07_sverka}; mkdir -p "$O"
SRC="tests/g07_sverka/rt_check.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
OLDF="-DMSN_STAGEDELAY_OLD=1 -DMSN_LOCKED_OLD=1 -DMSN_PAR_OLD=1 -DMSN_OOV_OLD=1 -DMSN_TIMERPAUSE_OLD=1 -DMSN_ORDER_OLD=1 -DMSN_DAMAGE_OLD=1 -DMSN_FOLLOW_OLD=1 -DMSN_LOSETAIL_OLD=1 -DMSN_GOOUTSIDE_OLD=1 -DMSN_INTERIOR_ENTER_OLD=1 -DMSN_COINS_OLD=1 -DMSN_DUMP_OLD=1 -DMSN_HINT_OLD=1 -DMSN_ABORT_OLD=1 -DMSN_IRIS_OLD=1 -DMSN_LEVELOVER_OLD=1 -DMSN_TALKORDER_OLD=1 -DMSN_C3_OLD=1 -DMSN_BUYCUR_OLD=1 -DMSN_CP5_OLD=1 -DMSN_RETRYTIME_OLD=1 -DMSN_DUMPFAR_OLD=1 -DMSN_DUMPHIT_OLD=1 -DMSN_FADE_OLD=1 -DMSN_BONUSINTRO_OLD=1 -DMSN_BONUSREPEAT_OLD=1 -DMSN_BARREL_OLD=1 -DMSN_BUYMARK_OLD=1"
cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission $SRC -lm -o "$O/rt_new" || exit 1
cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission $OLDF $SRC -lm -o "$O/rt_old" || exit 1
MSNS=${@:-build/missions/level01.msn}
POS=0; : > "$O/rt_green_all.txt"; : > "$O/rt_red_all.txt"
for m in $MSNS; do
  echo "== $m (как в оригинале):"; "$O/rt_new" "$m" > "$O/rt_g.txt" || POS=1; grep -v ' OK ' "$O/rt_g.txt" | grep -v SKIP
  "$O/rt_old" "$m" > "$O/rt_r.txt"
  sed "s|^CHECK |$(basename $m) |" "$O/rt_g.txt" | grep '^level' >> "$O/rt_green_all.txt"; sed "s|^CHECK |$(basename $m) |" "$O/rt_r.txt" | grep '^level' >> "$O/rt_red_all.txt"
done
# круг 6: красный по КАЖДОЙ проверке — прошедшая в зелёном (на каком-то уровне) обязана упасть в красном (хотя бы на одном уровне)
MISS=0
for NAME in $(grep ' OK  ' "$O/rt_green_all.txt" | awk '{print $2}' | sort -u); do
  # контрольные стороны: старое поведение их тоже проходит (закрепляют, что новое не сломало общее); строку сверки ловит соседняя проверка той же темы
  case "$NAME" in *_info|abort_re|arrest|buy_cur|lockok|lose_near|oov_back|oov_in|oov_wait|outside_in|par_fail|pause_go|retry_set|bonus_once) continue;; esac   # retry_*: «уже так» (строки 55, 66), старого выключателя нет
  grep -q "^[^ ]* $NAME .*FAIL" "$O/rt_red_all.txt" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $NAME"; MISS=$((MISS+1)); }
done
echo "== прежнее поведение: упало $(grep -c ' FAIL ' "$O/rt_red_all.txt") проверок, не поймано $MISS"
if [ $POS = 0 ] && [ $MISS = 0 ]; then echo PASS; else echo "FAIL (новое: код $POS, красных не поймано $MISS)"; exit 1; fi
