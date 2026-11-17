#!/bin/bash
# Правка 1265 (окно Стена, 107): машина этапа без модели не бывает невидимой твёрдой стеной (L1 m3, финал «destroy smith_v»; служебное «flag mcrfail N»).
# Нужны два EBOOT: новый (PACK_NEW) и собранный с -DMCRFIX1265_OLD=1 (PACK_OLD); рядом лежат данные TEST106 (папки-пакеты с ссылками, как build/st107/pack*).
#   зелёный:  mcrfail 12     — отказы чтения проходят: своя модель встаёт (в 107b лестница делает по одному шагу в кадр, заглушка только после исчерпания; MCRNONE в первые кадры допустим)
#             mcrfail 100000 — лестница исчерпана: WATCHDOG mcr standin (кузов трафика), невидимой нет, MCRNONE записан (для ботов — дыра)
#   красный:  OLD=1, оба сценария — MCRNONE и машина невидима (stand-in нет)
# Безголовый эмулятор, один за раз. Использование: tests/stena107/gate.sh PACK_NEW PACK_OLD
set -u; cd "$(dirname "$0")/../.."; N="${1:-build/st107/pack}"; O="${2:-build/st107/pack_old}"; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$2" SMOKE_MAXT=300 tools/smoke.sh "$1" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { local R="$1" want="$2" pat="$3" what="$4" n; n=$(grep -cE "$pat" "$R/smoke.txt"); if { [ "$want" = yes ] && [ "$n" -gt 0 ]; } || { [ "$want" = no ] && [ "$n" = 0 ]; }; then echo "  ok   $what"; else echo "  FAIL $what (строк $n)"; fail=1; fi; }
echo "новый, mcrfail 12:";     R=$(run "$N" tests/stena107/m3_mcrfail9.txt);    chk "$R" yes "MCR load smith_v" "своя модель прочитана (107b: лестница по шагу в кадр, заглушка — только после исчерпания, потом своя)"; chk "$R" no "MCR bad .*perm=1" "битым навсегда не стал"
echo "новый, mcrfail 100000:"; R=$(run "$N" tests/stena107/m3_mcrfail_all.txt); chk "$R" yes "WATCHDOG mcr standin smith_v" "кузов-заглушка нарисован"; chk "$R" yes "WATCHDOG mcr ladder-exhausted smith_v" "лестница исчерпана записана"
echo "OLD=1, mcrfail 9:";      R=$(run "$O" tests/stena107/m3_mcrfail9.txt);    chk "$R" yes "MCRNONE smith_v" "КРАСНЫЙ: MCRNONE (невидимая)"; chk "$R" no "mcr standin" "заглушки нет"
echo "OLD=1, mcrfail 100000:"; R=$(run "$O" tests/stena107/m3_mcrfail_all.txt); chk "$R" yes "MCRNONE smith_v" "КРАСНЫЙ: MCRNONE (невидимая)"; chk "$R" no "mcr standin" "заглушки нет"
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
