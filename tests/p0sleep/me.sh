#!/bin/bash
# Гейт 107б P0: звук возвращается на ME после сна. tests/p0sleep/me.sh DIR_С_EBOOT_ТУРА_И_ДАННЫМИ
# ЧЕСТНО: настоящий ME после сна и новый модуль sharme.prx проверяет только железо (тур 107 + сон Роба). Здесь — пустышка ME на главном (файл meaudio=3), которая после flag simsleep 5 с не отвечает и держит «занято»
# (как ME, перезапущенный сном); пустышка повторяет правило нового модуля (устаревшее «занято» > 500 мс свободно); с откатом модуля (rollback.txt: SHARME_STALE107B_OLD) правила нет.
# A новое: в последние 5 с ME получает ≥ 60 буферов в секунду, главный считает ≤ 5, строка ME WAKE ok; B откат модуля (нет правила «устаревшее занято»): ME не вернулся, ok нет; C flag mewakeold 1: ME не вернулся.
set -u; cd "$(dirname "$0")/../.."; BAD=0; SRC="$1"; T=$(mktemp -d /tmp/mewake.XXXXXX)
mk() { local D="$T/$1"; mkdir -p "$D"; for f in "$SRC"/*; do ln -s "$(readlink "$f" 2>/dev/null || echo "$f")" "$D/$(basename "$f")" 2>/dev/null; done; rm -f "$D/EBOOT.PBP" "$D/meaudio" "$D/rollback.txt"; cp "$SRC/EBOOT.PBP" "$D/EBOOT.PBP"; printf 3 > "$D/meaudio"; [ -n "${2:-}" ] && printf '%s\n' "$2" > "$D/rollback.txt"; echo "$D"; }
one() {  # $1 имя, $2 каталог, $3 строка flag перед сном
  local SC; SC=$(mktemp /tmp/mew.XXXXXX); printf 'scene ME_A\nwait 8\nflag repair\nwait 3\n%s\nflag simsleep 0\nwait 34\nend\n' "$3" > "$SC"
  local L R; L=$(SMOKE_EXTRA="meaudio rollback.txt" SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$SC" tools/smoke.sh "$2" 2>&1); rm -f "$SC"; R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  [ -n "$R" ] || { echo "FAIL $1: нет отчёта smoke"; BAD=1; return; }
  local cpu kick ok gu; cpu=$(grep "^AUDQ" "$R/smoke.txt" | tail -5 | sed -E 's/.* cpu=([0-9]+) .*/\1/' | awk '{s+=$1} END {printf "%d", s/NR}'); kick=$(grep "^AUDQ" "$R/smoke.txt" | tail -5 | sed -E 's/.* kick=([0-9]+) .*/\1/' | awk '{s+=$1} END {printf "%d", s/NR}'); ok=$(grep -c "ME WAKE ok" "$R/smoke.txt"); gu=$(grep -c "ME WAKE give-up" "$R/smoke.txt")
  echo "$1: за последние 5 с на ME отдано $kick буферов из 87 (из них посчитал главный: $cpu), ME WAKE ok $ok, give-up $gu"; echo "$cpu $kick $ok" > "$T/$1.res"; grep -h "ME WAKE" "$R/smoke.txt" | head -8 | sed 's/^/    /'; rm -rf "${R:?}"
}
one A "$(mk a)" ""
one B "$(mk b SHARME_STALE107B_OLD)" ""
one C "$(mk c)" "flag mewakeold 1"
read -r ca ka oa < "$T/A.res"; read -r cb kb ob < "$T/B.res"; read -r cc kc oc < "$T/C.res"
# «работает на ME» = отдано ≥ 60 буферов в секунду и почти все посчитал ME (cpu ≤ 5)
[ "$ka" -ge 60 ] && [ "$ca" -le 5 ] && [ "$oa" -ge 1 ] || { echo "FAIL A: ME не вернулся после сна"; BAD=1; }
{ [ "$kb" -lt 60 ] || [ "$cb" -gt 5 ]; } && [ "$ob" -eq 0 ] || { echo "КРАСНЫЙ B НЕ ПОЙМАН: с прежним модулем звук вернулся"; BAD=1; }
{ [ "$kc" -lt 60 ] || [ "$cc" -gt 5 ]; } && [ "$oc" -eq 0 ] || { echo "КРАСНЫЙ C НЕ ПОЙМАН: без возврата звук вернулся"; BAD=1; }
rm -rf "${T:?}"; [ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
