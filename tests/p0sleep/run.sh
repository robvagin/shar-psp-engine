#!/bin/bash
# Гейт 107б P0 шаг 1: sfx.sab лечится после сна. tests/p0sleep/run.sh DIR_С_EBOOT_ТУРА_И_ДАННЫМИ
# Сцена: L1, flag simsleep 3 (сон, kill-хук закрывает sfx.sab, три чтения/открытия отказывают, пробуждение), затем четыре звука не из кэша (flag sndplay).
# Новое: строки PWR suspend/resume, epoch растёт, SNDB err=0 и heal ≥ 2; flag sbhealold 1 (красный контроль): err > 0 (файл банка мёртв до смены уровня).
set -u; cd "$(dirname "$0")/../.."; BAD=0
one() {  # $1 old(0/1)
  local SC; SC=$(mktemp /tmp/p0s.XXXXXX); printf 'scene P0_A\nwait 6\nflag repair\nwait 2\nflag sbhealold %s\nflag simsleep 3\nwait 2\nflag sndplay 60\nwait 1\nflag sndplay 70\nwait 1\nflag sndplay 80\nwait 1\nflag sndplay 90\nwait 3\nend\n' "$1" > "$SC"
  local L R; L=$(SMOKE_MAXT=200 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$SC" tools/smoke.sh "$2" 2>&1); rm -f "$SC"; R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  [ -n "$R" ] || { echo "FAIL: нет отчёта smoke"; BAD=1; return; }
  local err heal ep susp res; err=$(grep -h "^SNDB" "$R/smoke.txt" | tail -1 | sed -E 's/.* err=([0-9]+).*/\1/'); heal=$(grep -h "^SNDB" "$R/smoke.txt" | tail -1 | sed -E 's/.* heal=([0-9]+)\/.*/\1/')
  susp=$(grep -c "^PWR suspend" "$R/smoke.txt"); res=$(grep -c "^PWR resume " "$R/smoke.txt"); ep=$(grep -h "^PWR resume-complete" "$R/smoke.txt" | tail -1 | sed -E 's/.*epoch=([0-9]+).*/\1/')
  echo "old=$1: PWR suspend $susp, resume $res, epoch после сна $ep, SNDB err=$err heal=$heal"
  if [ "$1" = 0 ]; then [ "$err" = 0 ] && [ "${heal:-0}" -ge 2 ] && [ "$susp" = 1 ] && [ "$res" = 1 ] || { echo "FAIL новое: звук после сна не лечится"; BAD=1; }
  else [ "${err:-0}" -gt 0 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН: без лечения ошибок чтения нет"; BAD=1; }; fi
  rm -rf "${R:?}"
}
one 0 "$1"; one 1 "$1"; [ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
