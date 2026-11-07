#!/bin/bash
# №1168: переходы бонус-игры в безголовом эмуляторе: главное меню -> MINI GAME -> трасса 4, 1, 7 подряд (по 20 с гонки, выход из гонки) -> меню, сцена тура TEST88 (tests/smoke_scenes/bonus89_block.txt).
#   tests/bonus1167/menu_tracks.sh EBOOT [BALLAST_MB] [FRAG_MB]    BALLAST_MB — файл ballast (куча тесней), FRAG_MB — «flag bonusfrag N» (наибольший свободный блок ≤ N МБ)
# Проверяет по smoke.txt: экран загрузки (ev=loadscr fe=1 — LoadingFE) на каждой трассе, ev=load, ev=quit с кучей, ev=exit ok=1, снимки bonus_load (экран загрузки) и три пары race/back не пустые;
# эмулятор не упал (rc 0). КРАСНЫЙ: сборка с -DBONUSLOAD1168_OLD=1 — строк loadscr нет, проверка обязана упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="${1:?EBOOT}"; BAL="${2:-0}"; FRAG="${3:-0}"
SET="${SHAR_SET:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15}"; W="${SHAR_WORK:-$ROOT/build/bonus1168}"
BIN="$HOME/Downloads/shar-psp/build/twins/bin/PPSSPPHeadless"
[ -f "$EB" ] && [ -d "$SET/bonus" ] && [ -x "$BIN" ] || { echo "FAIL: нет EBOOT, набора или эмулятора"; exit 2; }
tag="$(shasum -a 256 "$EB" | cut -c1-8)-b$BAL-f$FRAG"; d="$W/$tag"; rm -rf "${d:?}"; mkdir -p "$d/run" "$d/out"
for f in "$SET"/*; do b="$(basename "$f")"; case "$b" in EBOOT.PBP|scenario.txt|smokelog) ;; *) ln -s "$f" "$d/run/$b";; esac; done
cp "$EB" "$d/run/EBOOT.PBP"; touch "$d/run/smokelog"; printf 3 > "$d/run/meaudio"; [ "$BAL" != 0 ] && printf "%s" "$BAL" > "$d/run/ballast"
{ echo "flag menu"; [ "$FRAG" != 0 ] && echo "flag bonusfrag $FRAG"; echo "flag bonusshots 1"; grep -v "^#" tests/bonus1167/scn_menu_tracks.txt | grep -v "^flag menu" | grep -v "^end"; echo "wait 2"; echo "end"; } > "$d/run/scenario.txt"
( cd "$d/run" && env TWINS_SMOKE="$d/run/smoke.txt" TWINS_SHOTDIR="$d/out" SDL_AUDIODRIVER=dummy XDG_CONFIG_HOME="$d/out/config" XDG_DATA_HOME="$d/out/data" \
    "$BIN" --graphics=software --timeout=2400 "$d/run/EBOOT.PBP" > "$d/out/log.txt" 2>&1 ); rc=$?
sm="$d/run/smoke.txt"; fail=0
chk() { if [ "$2" -ge "$3" ]; then echo "  ok   $1: $2"; else echo "  FAIL $1: $2 (нужно >= $3)"; fail=$((fail+1)); fi; }
echo "EBOOT $tag, rc=$rc, журнал $sm"
chk "эмулятор не упал (rc=0)" $([ $rc -eq 0 ] && echo 1 || echo 0) 1
chk "загрузка трассы (ev=load)" "$(grep -a -c '^BONUS ev=load track' "$sm")" 3
chk "экран загрузки LoadingFE (ev=loadscr fe=1)" "$(grep -a -c '^BONUS ev=loadscr fe=1' "$sm")" 3
chk "гонка дошла до итогов (ev=done)" "$(grep -a -c '^BONUS ev=done' "$sm")" 3
chk "круг игрока засчитан (ev=finish car=0)" "$(grep -a -c '^BONUS ev=finish car=0 ' "$sm")" 3
chk "выход в меню (ev=exit ok=1)" "$(grep -a -c '^BONUS ev=exit ok=1' "$sm")" 3
chk "выбор персонажа кнопкой (ev=confirm kind=character)" "$(grep -a -c '^BONUS ev=confirm kind=character' "$sm")" 3
chk "выбор машины кнопкой (ev=confirm kind=vehicle)" "$(grep -a -c '^BONUS ev=confirm kind=vehicle' "$sm")" 3
chk "ожидания других игроков нет (uiscreen waitplayers)" "$([ "$(grep -a -c 'name=waitplayers' "$sm")" = 0 ] && echo 1 || echo 0)" 1
ls "$d/out"/*.bmp >/dev/null 2>&1 && chk "снимков экрана" "$(ls "$d/out"/*.bmp | wc -l | tr -d ' ')" 7 || chk "снимков экрана" 0 7
grep -a -E '^BONUS ev=(load track|loadscr|quit|exit)' "$sm" | cut -c1-170
[ $fail -eq 0 ] && echo "ИТОГ: PASS" || echo "ИТОГ: FAIL ($fail)"; exit $((fail>0))
