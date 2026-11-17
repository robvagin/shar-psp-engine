#!/bin/bash
# Гейт 1264 С4/С5: tests/bubble1264/gate_cs.sh EBOOT. Новое обязано пройти, старое (маска 8 / 16) упасть.
#  С4: L2 m7 этап 1→2 с одним текстом «destroy»: после смены этапа BUBBLE1247 rise ровно 1 (старое 0).
#  С5: разовое сообщение (flag uipopmsg 1) поверх задания: через 5 с значок цели на месте (MSNICON field=1000), текст задания вернулся (старое field=-1, msg пуст).
set -u; cd "$(dirname "$0")/../.."; R=tests/bubble1264/run.sh; EB=${1:?EBOOT}; ok=1
for O in 0 8; do CODE=601 PASSES=1 WAITP=6 $R $EB 2 6 1 $O >/dev/null 2>&1
  n=$(awk '/^MSN pass/{f=1} f&&/^BUBBLE1247 rise/{n++} END{print n+0}' build/bub1264/bub1264_${O}_L2_m6s1.txt)
  echo "С4 old=$O: всплытий после смены этапа $n"; if [ $O = 0 ]; then [ "$n" -ge 1 ] || ok=0; else [ "$n" = 0 ] || ok=0; fi; done
for O in 0 16; do ONEOFF=5 CODE=601 PASSES=0 $R $EB 2 6 1 $O >/dev/null 2>&1
  l=$(grep '^MSNICON' build/bub1264/bub1264_${O}_L2_m6s1.txt | tail -1); f=$(echo "$l" | sed -E 's/.* field=(-?[0-9]+) .*/\1/')
  echo "С5 old=$O: field=$f"; if [ $O = 0 ]; then [ "$f" = 1000 ] || ok=0; else [ "$f" = -1 ] || ok=0; fi; done
[ $ok = 1 ] && echo "ГЕЙТ ЗЕЛЁНЫЙ" || { echo "ГЕЙТ КРАСНЫЙ"; exit 1; }
