#!/bin/bash
# Сверка g02-traffic (02.11.2026): проверки на CPU Мака (tests/sverka_g02/g02.c). Зелёный прогон + красные контроли: для каждого правила его выключатель -DTRF_SV_*_OLD=1
# обязан уронить свою проверку.   tests/sverka_g02/g02.sh [level.shr] [traffic.shr]
set -u; cd "$(dirname "$0")/../.."
L=${1:-build/level.shr}; T=${2:-build/traffic.shr}; O=${TMPDIR:-/tmp}/g02.$$
SRC="tests/sverka_g02/g02.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c"
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $SRC -lm -o $O.green || exit 2
echo "зелёный:"; $O.green "$L" "$T" | tee $O.g.txt | grep -E "CHECK|RESULT"; gr=${PIPESTATUS[0]}
bad=0
red() {   # red ИМЯ_ВЫКЛЮЧАТЕЛЯ ПРЕФИКС_ПРОВЕРКИ...
  local sw=$1; shift
  cc -std=c11 -O2 -w -DSKIN_HOST -D$sw=1 -I viewer $SRC -lm -o $O.r || { echo "сборка $sw не удалась"; bad=1; return; }
  $O.r "$L" "$T" > $O.r.txt
  for chk in "$@"; do
    if grep -q "CHECK $chk .*FAIL" $O.r.txt; then echo "красный $sw: «${chk}» упала, как надо"; else echo "НЕ ПОКРАСНЕЛ $sw: «${chk}»"; bad=1; fi
  done
}
red TRF_SV_SPEED_OLD "42 разгон"
red TRF_SV_BRAKE_OLD "22 запас"
red TRF_SV_JUNC_OLD "29 стоп-знак"
red TRF_SV_PEDSPAWN_OLD "14 граница"
red TRF_SV_PANIC_OLD "6 паника"
red TR_PED_HIT_V_OLD "8 порог"
red TRF_SV_DMG2_OLD "11 2 процента"
red TRF_SV_HUSK_OLD "13 остов"
red TRF_SV_CHOOSE_OLD "31 полоса 0"
red TRF_SV_DODGE_OLD "9 уворот"
red TRF_SV_LANECHG_OLD "10 смена" "10 помеха 21 м"
red TRF_SV_PEDTALK_OLD "39 разговор"
red TRF_SV_PANIC_OLD "6 заражение"
red TRF_SV_HONK_OLD "16 гудок" "P09:3 пробка"
red TRF_SV_KICK_OLD "3.4 пинок"
red TRF_SV_ZPARK_OLD "P03-19 зона"
red TRF_SV_KICKF_OLD "3.3 пинок"
red TRF_SV_MOUTH_OLD "P15-06 рот"
red TRF_SV_PROPSCARS_OLD "P11-02 машины"
red TRF_SV_PEDTOL_OLD "7 допуск"
red TRF_SV_PEDRECT_OLD "7 допуск"
red TRF_SV_PEDDODGE_OLD "7 реакция"
red TRF_SV_PEDFACE_OLD "12 лицом"
red TRF_SV_PEDPATH_OLD "46 тропа"
red TRF_SV_PEDHIT_OLD "50 толчок" "8 отлёт трафика"
red TRF_SV_PEDFLY_OLD "8 отлёт трафика"
red TR_PED7_OLD "3 семь пешеходов"
red PEDLIE_PS2_OLD "7.7 лежание"
red TRF_REST1159_OLD "1159 засыпает"
red TRF_SV_PEDSPEED_OLD "5 скорость"
red TRF_SV_REMOVE_OLD "18 удаление"
red TRF_SV_BRAKE_OLD "23 коридор" "22 запас пешеход"
red TRF_SV_LANECHG_OLD "10 граница 21 м"
red TRF_SV_PEDTALK_OLD "39 границы"
red TRF_SV_CHOOSE_OLD "62 треть" "61 повтор 5 с"
red TRF_SV_BRAKEFIX_OLD "43 торможение"
red TRFSMOOTH1092_OLD "1092 поворот"
red OBSTACLE1098_OLD "1098 обход"
red PEDCAR_ALL_OLD "слой пеший-машина"
red TRF_SV_REJOIN_OLD "21 возврат 3 с"
red TRF_SV_REJOIN2_OLD "21 у конца дороги"
red TRF_SV_HONK_OLD "41 пешеходу"
red TRF_SV_SPAWN_OLD "55 скорость рождения" "20 кольцо" "34 плотность 0"
red TRF_SV_PEDSPAWN_OLD "58 частота"
red TRF_SV_RACE_OLD "15 гонка"
red TRF_SV_KICK_OLD "2 один пинок"
red TRF_SV_NAV_OLD "1 навигация"
rm -f $O.green $O.r $O.g.txt $O.r.txt
[ $gr = 0 ] && [ $bad = 0 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; }
