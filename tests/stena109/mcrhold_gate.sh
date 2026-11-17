#!/bin/bash
# 109 (в, Fable-2): машины всей миссии заняты до конца миссии — ни выгрузки по истечении 90 с, ни синхронного перечитывания (рывок 33–42 мс каждые ~91 с).
# Новое: после 20-й секунды (миссия давно началась) за 130 с «MCR load <машина>» нет вовсе; красный: flag mcrhold109old 1 — повтор есть.   mcrhold_gate.sh PACK
set -u; cd "$(dirname "$0")/../.."; P=$1
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=400 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
rep() { grep -E "^MCR load " "$1/smoke.txt" | awk '{ t=$NF; sub(/^t=/,"",t); if (t+0 >= 20) { n[$3]++ } } END {m=0; for (k in n) { printf "%s=%d ", k, n[k]; if (n[k]>m) m=n[k] } printf "| max=%d\n", m}'; }
Rn=$(run tests/stena109/mcrhold_new.txt); Ro=$(run tests/stena109/mcrhold_old.txt)
a=$(rep $Rn); b=$(rep $Ro); echo "  новое: $a"; echo "  _OLD:  $b"
mn=${a##*max=}; mo=${b##*max=}
[ "${mn:-9}" -le 0 ] && [ "${mo:-0}" -ge 1 ] && echo PASS || { echo FAIL; exit 1; }
