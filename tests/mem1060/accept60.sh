#!/bin/bash
# №1060 приёмка (Роб 27.10: «у нас всегда была возможность кэша для будки, а если нет — прелоадер и переход; предусмотреть сценарии освобождения места и подгрузки обратно»).
# Проверяется: на каждом уровне L1–L7 при забитой и раздробленной куче будка открывается, выбранная машина приезжает, после выхода клетки и всё освобождённое на месте, утечки нет.
#   Куча забита: flag fragheap FRAG (наибольший свободный кусок ≤ FRAG МБ перед загрузкой уровня) + flag boothstarve LEFT (потребитель уровня забирает всё, что освобождают клетки, оставляя LEFT МБ;
#   отдаёт при выгрузке уровня — модель долгоживущих блоков кроме клеток). Один fragheap до отказа на прежнем коде не довёл ни при каких N (три ступени №1051 дроблением не обойти: пакет читается кусками по 256 КБ).
#   НОВЫЙ  (flag mem1060 1): сцена (газета, всё в кучу) -> не хватило -> пересборка уровня -> будка открыта -> машина -> возврат клеток под газетой; MEM1060 stat до/после.
#   КРАСНЫЙ (flag mem1060 0, выключатель): тот же прогон доходит до «TELEPHONE BOOTH UNAVAILABLE».
#   ОБЫЧНЫЙ (куча не тесная): ни сцены, ни перехода, ни выгрузки всех клеток — цена ноль.
# Запуск: tests/mem1060/accept60.sh [EBOOT] ; LEVELS="4 5" — только эти; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_m1060/EBOOT.PBP}; OUT=${OUT:-$W/build/m1060/acc}; export OUT; LV=${LEVELS:-1 2 3 4 5 6 7}; FRAG=${FRAG:-3}; LEFT=${LEFT:-1.5}
mkdir -p $OUT; rc=0; [ -n "$LV" ] || { echo "нет уровней"; echo FAIL; exit 1; }; [ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
jobs=""
[ "${REUSE:-0}" = 1 ] && [ -f $OUT/out_fered_L4/smoke.txt ] || jobs="fered_L4 4 $FRAG $LEFT 0 1 0"      # красный №1060б: flag mem1060fe 0 (шнур/газета, как в №1060)
for L in $LV; do
  for v in new red normal; do
    case $v in new) a="$FRAG $LEFT 0 1 1";; red) a="$FRAG $LEFT 0 0 1";; normal) a="0 -1 0 1 1";; esac
    [ "${REUSE:-0}" = 1 ] && [ -f $OUT/out_${v}_L$L/smoke.txt ] && continue
    jobs="$jobs\n$v"_L$L" $L $a"
  done
done
# эталон LoadingFE: тот же путь показа, что у перехода (flag mem1060fedemo), снимок ref1060 — в фоне, место в очереди общее
if [ "${REUSE:-0}" != 1 ] || [ ! -s $OUT/out_ref_fe/ref1060.png ]; then ( EMU_OWNER=${EMU_OWNER:-Оптимизация} EMU_MINE=3 SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74tour/PSP/GAME/SHARTEST15} OVL=${OVL:-$HOME/Downloads/shar-psp-t74-booth1056/build/b1056/acc/o_new} bash $W/tools/emu_run.sh $H/ref_fe.txt ref_fe $E 900 >/dev/null 2>&1 ) & fi
printf "$jobs" | grep . | xargs -P 3 -L 1 bash -c 'bash '$H'/run.sh "$0" "$1" "$2" "$3" "'$E'" "$4" "$5" "$6" >/dev/null 2>&1'
wait
export FE1060=1 REF1060=$OUT/out_ref_fe/ref1060.png
for L in $LV; do
  for v in new red normal; do
    echo "L$L $v:"; $W/.venv/bin/python3 -W ignore -I $H/check.py $OUT/out_${v}_L$L/smoke.txt $v $L || rc=1
  done
done
echo "красный №1060б (flag mem1060fe 0, L4): проверка LoadingFE обязана упасть:"
o=$($W/.venv/bin/python3 -W ignore -I $H/check.py $OUT/out_fered_L4/smoke.txt new 4); echo "$o" | grep -E "FAIL|PASS" | head -8
if echo "$o" | grep -qE "FAIL +(№1060б|шнур|возврат|кадр перехода)"; then echo "  ok   красный красный: проверки LoadingFE упали на выключателе"; else echo "  FAIL красный не красный"; rc=1; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
