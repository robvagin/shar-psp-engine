#!/bin/bash
# Гейт 1360 (Fable Ж5, traffic.c retDrive1345): возврат ударенной машины трафика на полосу идёт с отпущенным ручником. Данные набора 108 (level.shr, traffic.shr). Красный: -DTRF_RETHB1360_OLD=1.
# Новое: в кадрах возврата (retOn) ручник отпущен (остаток защёлки SV_HBLATCH_T первые кадры, не более 20; занос ручника 0), машина вернулась на полосу; прежнее: ручник и занос ручника почти во всех кадрах возврата.
cd "$(dirname "$0")/../.."; V=viewer; S=${1:-${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15}}; L=$S/level.shr; T=$S/traffic.shr; O=${TMPDIR:-/tmp}/trf1360; mkdir -p $O
SRC="tests/trf1360/s.c $V/traffic.c $V/homer.c $V/skin.c $V/col.c $V/car.c"
cc -std=c11 -O2 -w -DSKIN_HOST -I $V $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/new || exit 1
cc -std=c11 -O2 -w -DSKIN_HOST -DTRF_RETHB1360_OLD=1 -I $V $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/old || exit 1
n=$($O/new $L $T vis 2>&1 | grep "^trf1360"); o=$($O/old $L $T vis 2>&1 | grep "^trf1360"); echo "новое:   $n"; echo "прежнее: $o"
g() { echo "$1" | sed -n "s/.*$2 \([-0-9.]*\).*/\1/p" | head -1; }
nh=$(g "$n" "кадров с ручником"); oh=$(g "$o" "кадров с ручником"); ns=$(g "$n" "кадров в заносе"); os=$(g "$o" "кадров в заносе"); nb=$(g "$n" "вернулась через")
awk -v nh="$nh" -v oh="$oh" -v ns="$ns" -v os="$os" -v nb="$nb" 'BEGIN { exit !(nh <= 20 && oh >= 100 && ns == 0 && os >= 50 && nb > 0) }' && echo "ГЕЙТ tests/trf1360: PASS" || { echo "ГЕЙТ tests/trf1360: FAIL (кадров с ручником: новое $nh, прежнее $oh; в заносе: новое $ns, прежнее $os; вернулась новое $nb)"; exit 1; }
