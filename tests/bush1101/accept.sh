#!/bin/bash
# Гейт №1101 (кусты-шары: середина между «насквозь» и «твёрдые»), хост на Маке. Зелёный: на 3 и 5 м/с куст держит (проходят насквозь не чаще, чем у твёрдого варианта, плюс 5 %), с него можно сдать назад (все заезды без прохода),
# отскока больше 25 % скорости входа не чаще, чем у твёрдого варианта; на 10 и 14 м/с проходят насквозь не реже 80 % от варианта «наш», сразу после входа остаётся не больше 65 % скорости входа. Красные (обязаны упасть): -DBUSHMID1101_OLD=1 (насквозь
# даже медленно, как TEST83) и -DSPBUSH_SV_OLD=0 (твёрдые на любой скорости, как оригинал: быстро не проходят).
#   tests/bush1101/accept.sh [LEVEL.shr ...]   (по умолчанию L1 и l2..l7, если есть)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
LV=("$@"); [ ${#LV[@]} -eq 0 ] && { LV=(build/level.shr); for n in 2 3 4 5 6 7; do for d in build/levels/l$n $HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15/l$n; do [ -f $d/level.shr ] && { LV+=($d/level.shr); break; }; done; done; }
OUT=build/bush1101; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
SRC="tests/bush1101/host.c $V/props.c $V/stateprops.c $V/col.c $V/car.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head -5; exit 1; }; : > "$OUT/$NAME.txt"; for l in "${LV[@]}"; do echo "# $l" >> "$OUT/$NAME.txt"; "$OUT/$NAME" "$l" >> "$OUT/$NAME.txt"; done; }
NAME=green; b; NAME=through; b -DBUSHMID1101_OLD=1; NAME=hard; b -DSPBUSH_SV_OLD=0
# итог по всем уровням: суммы n, crossed, back, backok, spring по скоростям; outmax, exitmax — максимум
agg() { awk -v v="$2" -v key="$3" '$1==v { for (i=2;i<=NF;i+=2) if ($i==key) { if (key=="outmax"||key=="exitmax") { if ($(i+1)>m) m=$(i+1) } else s+=$(i+1) } } END { print (key=="outmax"||key=="exitmax") ? m+0 : s+0 }' "$OUT/$1.txt"; }
# числа по режиму: медленно проходят / всего, сдать назад удалось / не прошли, пружина, быстро проходят / всего, выход
stat() { N=$1
  S3N=$(agg $N v3 n); S3C=$(agg $N v3 crossed); S5N=$(agg $N v5 n); S5C=$(agg $N v5 crossed); F10N=$(agg $N v10 n); F10C=$(agg $N v10 crossed); F14N=$(agg $N v14 n); F14C=$(agg $N v14 crossed)
  SLN=$((S3N+S5N)); SLC=$((S3C+S5C)); FN=$((F10N+F14N)); FC=$((F10C+F14C))
  BK=$(( $(agg $N v3 back) + $(agg $N v5 back) )); BKO=$(( $(agg $N v3 backok) + $(agg $N v5 backok) )); SPR=$(( $(agg $N v3 spring) + $(agg $N v5 spring) + $(agg $N v10 spring) + $(agg $N v14 spring) ))
  EX=$(awk -v s="$(agg $N v10 exitsum)" -v t="$(agg $N v14 exitsum)" -v n=$((F10C+F14C)) 'BEGIN{printf "%.2f", (n>0 ? (s+t)/n : 0)}')
  echo "$N: медленно проходят $SLC из $SLN, сдать назад удалось $BKO из $BK, пружина/отскок>25%: $SPR, быстро проходят $FC из $FN, сразу после входа остаётся в среднем $EX скорости входа"
}
# быстрых проходов «насквозь» у режима «наш» (свободный) меньше 100 %: кусты стоят рядами и рядом бывают стены мира — меряем в долях от него
stat through; TFC=$FC; TFN=$FN
stat green; GFC=$FC; GFN=$FN; GSLC=$SLC; GSLN=$SLN; GBK=$BK; GBKO=$BKO; GSPR=$SPR; GEX=$EX
stat hard; HFC=$FC; HSLC=$SLC; HSPR=$SPR
greenok() { awk -v hsl=$HSLC -v hsp=$HSPR -v gfc=$GFC -v tfc=$TFC -v gsl=$GSLC -v gsn=$GSLN -v bk=$GBK -v bo=$GBKO -v sp=$GSPR -v ex="$GEX" 'BEGIN{ exit !(gsn>0 && tfc>0 && gsl<=hsl+.05*gsn && gfc>=.8*tfc && bo>=.9*bk && sp<=hsp && ex+0>0 && ex<=.65) }'; }
fail=0
greenok || { echo "FAIL зелёный: медленно должны не проходить, быстро проходить не реже 80 % от режима «наш», назад выезжать, без пружины"; fail=1; }
# красные: «наш» (насквозь всегда) проходит и медленно; «оригинал» (твёрдый) быстро проходит заметно реже, чем середина
awk -v a=$(agg through v3 crossed) -v an=$(agg through v3 n) 'BEGIN{exit !(an>0 && a>.1*an)}' && echo "красный BUSHMID1101_OLD=1: упал, как должен — медленно проходят $(( $(agg through v3 crossed) + $(agg through v5 crossed) )) из $(( $(agg through v3 n) + $(agg through v5 n) ))" || { echo "КРАСНЫЙ BUSHMID1101_OLD НЕ ПОЙМАН"; fail=1; }
awk -v h=$HFC -v g=$GFC 'BEGIN{exit !(g>0 && h<=.5*g)}' && echo "красный SPBUSH_SV_OLD=0 (твёрдые, как оригинал): упал, как должен — быстро проходят $HFC против $GFC у середины" || { echo "КРАСНЫЙ SPBUSH_SV_OLD=0 НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
