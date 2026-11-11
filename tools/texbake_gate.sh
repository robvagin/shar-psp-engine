#!/bin/bash
# №bake-3А: гейт печёной таблицы текстур, побайтно на уровнях. Каждый уровень — два прогона эмулятора на Маке:
#   «use»   — движок берёт texbake.shr: ждём used=ntex, live=0, miss=0, bad=0;
#   «check» — считает живое и сравнивает с печёным: ждём cmp=ntex, mismatch=0.
# Красные контроли (уровень 1, ключ -n): corrupt=1 (испорчен байт палитры в записи) — запись отвергнута хешем (bad=1), остальные использованы, итог = живой расчёт;
#   corrupt=2 (испорчен байт и пересчитан хеш записи) + check — сверка ОБЯЗАНА дать mismatch>=1 (иначе гейт не умеет краснеть).
#   tools/texbake_gate.sh SET [-n] [УРОВНИ]   SET как у tools/texbake.sh, но с texbake.shr рядом с уровнями (для L1 — в корне SET)
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; SET="$1"; shift; NEG=0; [ "${1:-}" = -n ] && { NEG=1; shift; }; LV="${*:-1 2 3 4 5 6 7}"; SCN="$(mktemp -d)"; FAIL=0
one() { # level flags filedata tag -> строка TEXBAKE
  printf 'flag level %s\nscene g%s\nwait 14\nend\n' "$1" "$1" > "$SCN/s$1${4:-}.txt"
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_EXTRA="texbake.shr l2/texbake.shr l3/texbake.shr l4/texbake.shr l5/texbake.shr l6/texbake.shr l7/texbake.shr $(for f in flat912.bin marks912.bin notoon.txt zones1007.bin zones1007p.bin; do for d in "" l2/ l3/ l4/ l5/ l6/ l7/; do [ -f "$SET/$d$f" ] && printf "%s " "$d$f"; done; done)" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3 $3" SMOKE_FLAGS="$2" SMOKE_SCN="$SCN/s$1${4:-}.txt" SMOKE_MAXT="${SMOKE_MAXT:-600}" "$ROOT/tools/smoke.sh" "$SET" >"$SCN/run$1${4:-}.log" 2>&1
  d=$(sed -n 's/^отчёт: \(.*\)\/summary.txt.*/\1/p' "$SCN/run$1${4:-}.log" | tail -1); case "$d" in /*) ;; *) d="$ROOT/$d" ;; esac; grep -a '^TEXBAKE ' "$d/smoke.txt" | head -1
}
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
for n in $LV; do
  u=$(one $n "" "" u); c=$(one $n texbakecheck "" c)
  nt=$(fld "$u" ntex); ok=1
  [ -n "$u" ] && [ "$(fld "$u" used)" = "$nt" ] && [ "$(fld "$u" live)" = 0 ] && [ "$(fld "$u" miss)" = 0 ] && [ "$(fld "$u" bad)" = 0 ] || ok=0
  [ -n "$c" ] && [ "$(fld "$c" cmp)" = "$nt" ] && [ "$(fld "$c" mismatch)" = 0 ] || ok=0
  [ $ok = 1 ] && echo "L$n PASS  use: $u" || { echo "L$n FAIL  use: $u | check: $c"; FAIL=1; }
  [ $ok = 1 ] && echo "      check: $c"
done
if [ $NEG = 1 ]; then
  a=$(one 1 "" "texbakecorrupt=1" a); [ "$(fld "$a" bad)" = 1 ] && echo "КОНТРОЛЬ corrupt=1 OK (запись отвергнута, откат на живое): $a" || { echo "КОНТРОЛЬ corrupt=1 НЕ СРАБОТАЛ: $a"; FAIL=1; }
  b=$(one 1 texbakecheck "texbakecorrupt=2" b); [ "${b:+x}" = x ] && [ "$(fld "$b" mismatch)" -ge 1 ] 2>/dev/null && echo "КОНТРОЛЬ corrupt=2 OK (сверка покраснела): $b" || { echo "КОНТРОЛЬ corrupt=2 НЕ СРАБОТАЛ (гейт не умеет краснеть): $b"; FAIL=1; }
fi
exit $FAIL
