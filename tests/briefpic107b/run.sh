#!/bin/bash
# Гейт 107б (генплан Fable, 1269 не закрыта в сценарии «мёртвый файл»): картинка карточки миссии встаёт, когда дескриптор msnart.shr залип (flag msnartiostuck 1: асинхронное чтение отказывает, пока не переоткрыт)
# и первые переоткрытия отказывают (flag msnartreopenfail 2: afd = -1). tests/briefpic107b/run.sh DIR_С_EBOOT_И_ДАННЫМИ [OLD]; OLD=1 → «flag msnartio107bold 1» (поведение после 108, красный: lost=1).
set -u; cd "$(dirname "$0")/../.."; OLD="${2:-0}"; SC=$(mktemp /tmp/bp107b.XXXXXX)
cat > "$SC" <<EOS
scene m0
wait 3
flag msn
wait 4
flag msngo
wait 2
flag msnartio107bold $OLD
flag msnartiostuck 1
flag msnartreopenfail 2
wait 1
press triangle
wait 60
shot brief_card
end
EOS
L=$(SMOKE_MAXT=400 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$SC" tools/smoke.sh "$1" 2>&1); rm -f "$SC"; R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
[ -n "$R" ] || { echo "FAIL: нет отчёта smoke"; exit 1; }
.venv/bin/python tests/briefpic107b/check.py "$R" "$OLD"; rc=$?; rm -rf "$R"; exit $rc
