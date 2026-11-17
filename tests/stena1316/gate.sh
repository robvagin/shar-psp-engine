#!/bin/bash
# 1316 (Роб: L2 m3 победа → ирис → снова карточка и гонка → чёрный экран): гейт.   gate.sh ПАКЕТ_НОВЫЙ [ПАКЕТ_0aa2f88c]
# Б (путь Роба): ранний «yes» на провале m1 → m3 → msnpass гонки (финиш = SetFadeOut). Зелёное: после финиша идут stage timer/dialogue, повторной карточки m3 нет, WATCHDOG fade нет.
# Красное: flag fadeguard1316old 1 + rspend1304old 1 (и, если дан, настоящая 0aa2f88c): после финиша снова «MSN briefing m3».   Сторож: fade_stuck.txt — WATCHDOG fade-stuck и чёрный уходит; fade_stuck_old.txt — нет.
set -u; cd "$(dirname "$0")/../.."; PN=$1; PO=${2:-}
res() { MAXT=420 tests/stena1316/run.sh "$1" "$2"; }
chk() { awk '/^MSN pass m3 1.0/{f=1;next} f&&/^MSN briefing m3/{rb++} f&&/^MSN stage m3.1.1/{t++} /^WATCHDOG fade/{w++} END{printf "повтор карточки %d, stage timer %d, WATCHDOG fade %d\n", rb+0, t+0, w+0}' "$1/smoke.txt"; }
Rn=$(res tests/stena1316/b_new.txt $PN); Rr=$(res tests/stena1316/b_red.txt $PN)
echo "Б новое:  $(chk $Rn)"; echo "Б красное: $(chk $Rr)"
ok=1
read a b c <<< "$(chk $Rn | awk '{print $3,$6,$9}' | tr -d ',')"; [ "$a" = 0 ] && [ "$b" -ge 1 ] && [ "$c" = 0 ] || ok=0
read a b c <<< "$(chk $Rr | awk '{print $3,$6,$9}' | tr -d ',')"; [ "$a" -ge 1 ] || ok=0
if [ -n "$PO" ]; then Ro=$(res tests/stena1316/b_new.txt $PO); echo "Б на 0aa2f88c: $(chk $Ro)"; read a b c <<< "$(chk $Ro | awk '{print $3,$6,$9}' | tr -d ',')"; [ "$a" -ge 1 ] || ok=0; fi
Fn=$(res tests/stena1316/fade_stuck.txt $PN); Fo=$(res tests/stena1316/fade_stuck_old.txt $PN)
wn=$(grep -c "^WATCHDOG fade-stuck" $Fn/smoke.txt); wo=$(grep -c "^WATCHDOG fade-stuck" $Fo/smoke.txt)
echo "сторож: новое $wn срабатываний, старое $wo"; [ "$wn" -ge 1 ] && [ "$wo" = 0 ] || ok=0
[ $ok = 1 ] && echo PASS || { echo FAIL; exit 1; }
