#!/bin/bash
# №1192 гейт: естественные переходы уровней цепочкой L1->L7 как у игрока (flag realend 1 + flag lvwin): tests/smoke_scenes/lv1192_chain.txt.
# Использование: SET=<набор с EBOOT.PBP и данными, EBOOT с flag realend> [BALLAST=7] tests/g10b/lv1192.sh ; последняя строка PASS/FAIL.
# LOGOS=1 — с логотипами Fox/Gracie (файл lvlogoson, как у игрока TEST88), по умолчанию (№1199) логотипов нет: ролики L1 сыграны 0/2.
# PASS: 6 переходов LEVELUP from=N to=N+1 и загрузка уровня N+1, на L1 сыграны foxlogo и gracie (MSNFMV played=1), STALL 0, GE NESTED 0, MEMFAILSZ 0 при куче как у PSP (BALLAST=7), END достигнут.
# Эмулятор ME и прошивку роликов не исполняет: зелёный здесь не доказывает PSP (№1192: зависание всей приставки на L2 после роликов). Папка прогона берётся из вывода smoke.sh, не «последняя».
set -u; T="$(cd "$(dirname "$0")/../.." && pwd)"; SET=${SET:?нужен набор}
export SHIP_LEVELS="2 3 4 5 6 7"; EXP=0; [ -n "${LOGOS:-}" ] && EXP=2
SMOKE_FLAGS="${LOGOS:+lvlogoson}" SMOKE_BALLAST=${BALLAST:-7} SMOKE_HEADLESS=1 SMOKE_MAXT=${SMOKE_MAXT:-2400} SMOKE_SCN="$T/tests/smoke_scenes/lv1192_chain.txt" "$T/tools/smoke.sh" "$SET" > /tmp/lv1192_$$.log 2>&1
D=$(sed -n 's/^отчёт: \(.*\)\/summary.txt, листы.*/\1/p' /tmp/lv1192_$$.log | tail -1); case "$D" in /*) ;; *) D="$T/$D" ;; esac; S="$D/smoke.txt"
up=$(grep -ac "^LEVELUP from=[1-6] to=[2-7]" "$S"); ld=$(grep -a "^LEVEL ev=load n=[2-7]" "$S" | grep -ac "loaded=[2-7]"); fmv=$(grep -a "^MSNFMV name=\(foxlogo\|gracie\) " "$S" | grep -ac "played=1")
st=$(grep -ac "^BBSTALL" "$S"); ne=$(grep -ac "^GE NESTED" "$S"); mf=$(grep -ac "^MEMFAILSZ" "$S"); en=$(grep -ac "^END" "$S")
echo "LEVELUP: $up/6; загрузок L2..L7: $ld/6; ролики L1 сыграны: $fmv (ждём $EXP); STALL: $st; GE NESTED: $ne; MEMFAILSZ: $mf; END: $en ($D)"
[ "$up" = 6 ] && [ "$ld" = 6 ] && [ "$fmv" = "$EXP" ] && [ "$st" = 0 ] && [ "$ne" = 0 ] && [ "$mf" = 0 ] && [ "$en" -ge 1 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; }
