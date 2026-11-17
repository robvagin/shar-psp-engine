#!/bin/bash
# 1337: машина цели этапа не прочиталась (flag mcrreadfail 15: первые отказы, потом успех на повторе), карточка вышла по сроку, этап 1 L4 m1 («FOLLOW THE POLICE») стартует без cPolice.
# Новое: TARGET car=0, метка на пустом месте скрыта (MSNHUD marker=0), логика миссии стоит; когда машина прочиталась — TARGETWAIT car=1 waited>3 с (без cap), метка вернулась, провал не раньше прихода машины.
# Красное (flag mcrwait1337old 1): ожидания нет, этап идёт без машины (TARGET car=0, провал сразу), гейт обязан упасть.   tests/stena1338/wait.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS="4" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
N=$(run tests/stena1338/wait.scn); R=$(run tests/stena1338/wait_red.scn)
mk() { grep -a '^MSNHUD' $1/smoke.txt | grep -a 'stage=1 ' | head -1 | sed -n 's/.* marker=\([0-9]\).*/\1/p'; }
tw() { grep -a '^TARGETWAIT1337' $1/smoke.txt | head -1; }
c0() { grep -a '^TARGET stage=1.1' $1/smoke.txt | head -1 | sed -n 's/.* car=\([0-9]\).*/\1/p'; }
nm=$(mk $N); rm=$(mk $R); nt=$(tw $N); rt=$(tw $R); nc=$(c0 $N); rc=$(c0 $R)
echo "новое: car при старте=$nc, метка во время ожидания=$nm, $nt ($N)"; echo "красное: car при старте=$rc, метка=$rm, ожидание: ${rt:-нет} ($R)"
w=$(echo "$nt" | sed -n 's/.*waited=\([0-9]*\)ms.*/\1/p'); cap=$(echo "$nt" | grep -c ' cap')
tw_t=$(echo "$nt" | sed -n 's/.* t=\([0-9.]*\)$/\1/p'); nf_t=$(grep -a '^MSN failed' $N/smoke.txt | head -1 | sed -n 's/.* t=\([0-9.]*\) .*/\1/p')
ok=$(awk -v nc="$nc" -v nm="$nm" -v w="${w:-0}" -v cap="$cap" -v a="${tw_t:-0}" -v f="${nf_t:-0}" 'BEGIN{print (nc==0 && nm==0 && w>3000 && cap==0 && (f==0 || f>=a)) ? 1 : 0}')   # провал (если есть) не раньше прихода машины
red=$(awk -v rc="$rc" -v rt="${rt:-}" 'BEGIN{print (rc==0 && rt=="") ? 1 : 0}')   # красное: этап пошёл без машины и без ожидания
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
