#!/bin/bash
# Сверка g09b-menu, круг 6 (02.11.2026): правила меню (viewer/menu_rules.h) на CPU Мака. Зелёный прогон + красные контроли: для каждого правила его выключатель
# -D*_OLD=1 обязан уронить свою проверку. Плюс проводка: каждую функцию mr* из заголовка должна звать игра (иначе «внедрено, а вызова нет»).   tests/sverka_g09b/g09b.sh
set -u; cd "$(dirname "$0")/../.."
O=${TMPDIR:-/tmp}/g09b.$$; SRC=tests/sverka_g09b/g09b.c
cc -std=c11 -O1 -w -I viewer $SRC -lm -o $O.green || exit 2
echo "зелёный:"; $O.green | tee $O.g.txt | grep -E "FAIL|RESULT"; gr=${PIPESTATUS[0]}
bad=0
red() {   # red ВЫКЛЮЧАТЕЛЬ ИМЯ_ПРОВЕРКИ...
  local sw=$1; shift
  cc -std=c11 -O1 -w -D$sw=1 -I viewer $SRC -lm -o $O.r || { echo "сборка $sw не удалась"; bad=1; return; }
  $O.r > $O.r.txt
  for chk in "$@"; do
    if grep -q "CHECK $chk .*FAIL" $O.r.txt; then echo "красный $sw: «${chk}» упала, как надо"; else echo "НЕ ПОКРАСНЕЛ $sw: «${chk}»"; bad=1; fi
  done
}
red SVM_DEFNO_OLD "курсор запроса"
red SVM_START3_OLD "START вопрос выхода"
red SVM_QUITASK_OLD "вопросы выхода"
red SVM_TUTLABEL_OLD "подпись обучения"
red DEMOBLINK_OLD "мигание DEMO"
red DEMO30_OLD "простой 30 с"
red DEMOIDLE_HELD_OLD "кнопка не сбрасывает простой"
red CAMBLEND_OLD "посадка 50/90/95/99" "старт с нулевой скорости" "конец наезда 95 %" "форма по пересчёту"
red LBSLIDE_OLD "полосы разговора"
red OPTFLY_OLD "перелёты OPTIONS"
red JACKSPEED_OLD "угон по скорости"
red TUTGIVER_OLD "подсказка от выдавальщика"
red HNRMSN3_OLD "миссия: отмена не победа" "миссия: повтор сбрасывает"
red HNRONE_OLD "проходы погони"
red HNRSHOW_OLD "хвост показа после ареста"
red TRIG3D_OLD "будка: шар"
red SHADOWLIN_OLD "тень от высоты"
red MSNREPAIR2_OLD "ремонт на старте"
red MENUMUTE_ALL_OLD "глушение при старте"
red HNRMARK_OLD "5 с вне кадра"
red HNRSUSP_OLD "погоня в запас"
red NEWCAM_OLD "NEW GAME: умолчания"
red MINIFLY1143_OLD "MINI GAME: один наезд"
red CAMMENU1151_OLD "камера: старый файл"
red LANG1111_OLD "язык системы" "файл на языке"
# проводка: функция заголовка вызывается где-то кроме самого заголовка (и теста)
for f in $(grep -oE "static inline [a-z]+ (mr[A-Za-z]+)" viewer/menu_rules.h | awk '{print $4}'); do
  n=$(grep -lE "\b$f\(" viewer/*.c viewer/*.h | grep -vc "menu_rules.h")
  if [ "$n" -ge 1 ]; then :; else echo "НЕ ВЫЗЫВАЕТСЯ игрой: $f"; bad=1; fi
done
rm -f $O.green $O.r $O.g.txt $O.r.txt
[ $gr = 0 ] && [ $bad = 0 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; }
