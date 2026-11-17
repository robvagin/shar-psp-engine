#!/bin/bash
# Гейт №1271б: ролики конца уровня fmv3/5/6/7 (переходная миссия m8, в игре через цель «fmv», не из VIEW MOVIES) идут в рамке-телевизоре.
# tests/movietv1271/fmv_end.sh DIR_С_EBOOT [FAIL] [BALLAST] [УРОВНИ]: FAIL — сколько первых открытий рамки отказывает (flag tvfail1271: 0 обычный ход, 1 лестница памяти, 2 клетки в кучу), BALLAST по умолчанию 5 (9 — стресс).
# Уровни по умолчанию «2 5 6 7». Проверка: строка MOVIETV1271 who=fmvN ... frame=1 / opened=1 rc=0 и периметр снимка (check.py). Новый EBOOT зелёный при любом FAIL; сборка -DMOVIETV1271B_OLD=1 даёт красный при FAIL>=1.
set -u; cd "$(dirname "$0")/../.."; DIR="$1"; FAIL="${2:-0}"; BAL="${3:-5}"; LV="${4:-2 5 6 7}"; BAD=0
for L in $LV; do
  SC=$(mktemp /tmp/fmvend.XXXXXX); cat > "$SC" <<EOS
scene FMVEND_L$L
flag golevel $L
wait 40
flag repair
wait 3
flag tvfail1271 $FAIL
flag msnsel 7
wait 8
shot mid
wait 1
end
EOS
  O=$(SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=$BAL SHIP_LEVELS="1 2 3 4 5 6 7" SMOKE_SCN="$SC" tools/smoke.sh "$DIR" 2>&1); rm -f "$SC"
  R=$(echo "$O" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  [ -n "$R" ] || { echo "FAIL L$L: нет отчёта smoke"; BAD=1; continue; }
  echo -n "L$L fail=$FAIL ballast=$BAL: "; .venv/bin/python tests/movietv1271/check.py "$R/mid.png" || BAD=1
  grep -h "MOVIETV1271" "$R/smoke.txt" | head -3; rm -rf "${R:?}"
done
exit $BAD
