#!/bin/bash
# 1304 (Роб: «Yes» на экране провала — повтора нет): НАСТОЯЩИЙ провал по таймеру на L2 m1 «DRIVE TO THE TRY-N-SAVE» (сценарий Гомера h2), ✕ на «yes». Причина: restartIris зовётся в кадре с dt = 0, а fadeTick обрывал затемнение «из прозрачного» (fadeA 0 → dir 0) — чёрного не будет, перезапуск ждёт вечно.
# Новое: после MSN retry — MSNEND why=cleanup и новый MSN stage за 2 с, без restart-force; красный (flag rspend1304old 1): ни одной строки MSN stage после retry.   retry_gate.sh PACK_L2
set -u; cd "$(dirname "$0")/../.."; P=$1
run() { SHIP_LEVELS=2 SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=500 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { awk -v want="$2" '/^MSN retry/{f=1;next} f&&/^MSN stage/{n++} f&&/restart-force/{r++} END{printf "%d %d\n", n+0, r+0}' "$1/smoke.txt"; }
Rn=$(run tests/stena1296/h2_real.txt); Ro=$(run tests/stena1296/h2_real_old.txt)
read sn fn <<< "$(chk $Rn x)"; read so fo <<< "$(chk $Ro x)"
echo "  новое: этапов после retry $sn, принудительных $fn;  прежнее: этапов $so, принудительных $fo"
[ "$sn" -ge 1 ] && [ "$fn" = 0 ] && [ "$so" = 0 ] && echo PASS || { echo FAIL; exit 1; }
