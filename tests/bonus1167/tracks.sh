#!/bin/bash
# №1167 (вылет бонус-игры на PSP): все 7 трасс в безголовом эмуляторе: меню MINI GAME -> старт -> гонка 1 круг (водитель ИИ за игрока) -> выход в меню.
#   tests/bonus1167/tracks.sh EBOOT [трассы, по умолчанию "1 2 3 4 5 6 7"]     BONUS_FPS30=1 — 30 к/с, как в туре и на PSP (flag swaplate 17: пауза 17 мс на смену кадра); BONUS_VEH=5 — машина игрока carhom_v (flag bonusveh): водитель ИИ за игрока проходит круг, финиш игрока обязателен (с famil_v на повороте у старта он застревает — не вина трассы)
# Набор данных: SHAR_SET (папка SHARTEST15 тура; по умолчанию ~/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15), рабочая папка SHAR_WORK (по умолчанию build/bonus1167).
# Критерий трассы: эмулятор не упал (rc != 139), в журнале есть BONUS ev=load, ev=start, ev=exit ok=1 и нет обращений по адресу ниже 64 КБ (режим -i не нужен: JIT на таком обращении падает сам).
# КРАСНЫЙ: EBOOT, собранный с -DBONUSPRP1167_OLD=1, обязан упасть на каждой трассе (запись в таблицу текстур до её выделения). ЗЕЛЁНЫЙ: все трассы проходят.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="${1:?EBOOT}"; shift; TRACKS="${*:-1 2 3 4 5 6 7}"
SET="${SHAR_SET:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15}"; W="${SHAR_WORK:-$ROOT/build/bonus1167}"
BIN="$HOME/Downloads/shar-psp/build/twins/bin/PPSSPPHeadless"
[ -f "$EB" ] && [ -d "$SET/bonus" ] && [ -x "$BIN" ] || { echo "FAIL: нет EBOOT, набора ($SET/bonus) или эмулятора"; exit 2; }
tag="$(shasum -a 256 "$EB" | cut -c1-8)"; fail=0
one() {   # трасса -> строка итога, 0 = прошла
  local t="$1" d="$W/$tag/t$t"; rm -rf "$d"; mkdir -p "$d/run" "$d/out"
  for f in "$SET"/*; do b="$(basename "$f")"; case "$b" in EBOOT.PBP|scenario.txt|smokelog) ;; *) ln -s "$f" "$d/run/$b";; esac; done
  cp "$EB" "$d/run/EBOOT.PBP"; touch "$d/run/smokelog"; printf 3 > "$d/run/meaudio"
  { echo "flag menu"; echo "flag bonusopen"; echo "flag bonuslaps 1"; echo "flag bonusauto 1"; [ -n "${BONUS_FPS30:-}" ] && echo "flag swaplate 17"; [ -n "${BONUS_VEH:-}" ] && echo "flag bonusveh $BONUS_VEH"; echo "flag bonustrack $t"; echo "wait 2"; echo "press left"; echo "wait .6"; echo "press cross"; echo "wait 2"
    echo "flag bonusgo"; echo "wait 75"; echo "press triangle"; echo "wait 6"; echo "end"; } > "$d/run/scenario.txt"
  ( cd "$d/run" && env TWINS_SMOKE="$d/run/smoke.txt" TWINS_SHOTDIR="$d/out" SDL_AUDIODRIVER=dummy XDG_CONFIG_HOME="$d/out/config" XDG_DATA_HOME="$d/out/data" \
      "$BIN" --graphics=software --timeout=1500 "$d/run/EBOOT.PBP" > "$d/out/log.txt" 2>&1 ); local rc=$?
  local sm="$d/run/smoke.txt" ld st ex; ld=$(grep -a -c "^BONUS ev=load track=$t " "$sm" 2>/dev/null); st=$(grep -a -c "^BONUS ev=start track=$t " "$sm" 2>/dev/null); ex=$(grep -a -c "^BONUS ev=exit ok=1" "$sm" 2>/dev/null)
  local pl=1; if [ -n "${BONUS_VEH:-}${BONUS_FINISH:-}" ]; then pl=$(grep -a -c "^BONUS ev=finish car=0 " "$sm" 2>/dev/null); fi   # с BONUS_VEH (маневренная машина игрока за водителем ИИ) круг игрока обязан быть засчитан
  if [ $rc -eq 0 ] && [ "${ld:-0}" -ge 1 ] && [ "${st:-0}" -ge 1 ] && [ "${ex:-0}" -ge 1 ] && [ "${pl:-0}" -ge 1 ]; then echo "трасса $t: ПРОШЛА (load=$ld start=$st exit=$ex finish_player=$pl)"; return 0; fi
  echo "трасса $t: УПАЛА rc=$rc load=${ld:-0} start=${st:-0} exit=${ex:-0} finish_player=${pl:-0}"; return 1
}
MAXJ="${BONUS_JOBS:-2}"   # одновременно не больше MAXJ своих эмуляторов (общая машина: всего не больше трёх)
for t in $TRACKS; do while [ "$(jobs -rp | wc -l | tr -d " ")" -ge "$MAXJ" ]; do sleep 3; done; ( one "$t" > "$W.$tag.t$t.txt" 2>&1 ) & done; wait
for t in $TRACKS; do cat "$W.$tag.t$t.txt"; grep -q "ПРОШЛА" "$W.$tag.t$t.txt" || fail=$((fail+1)); rm -f "$W.$tag.t$t.txt"; done
[ $fail -eq 0 ] && echo "ИТОГ: PASS (EBOOT $tag, трассы: $TRACKS)" || echo "ИТОГ: FAIL ($fail из $(echo $TRACKS | wc -w), EBOOT $tag)"
exit $((fail>0))
