#!/bin/bash
# №1185: лист-карта секторов каждой из 7 трасс: снимок вида сверху из эмулятора (камера гонки) + границы секторов и номера по пути ИИ (tests/bonus1167/sector_sheet.py).
#   tests/bonus1167/sheet.sh EBOOT [ВЫХОД=build/bonus1167/sheets]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="${1:?EBOOT}"; OUT="${2:-$ROOT/build/bonus1167/sheets}"; mkdir -p "$OUT"
SET="${SHAR_SET:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15}"; BIN="$HOME/Downloads/shar-psp/build/twins/bin/PPSSPPHeadless"
BNS="${BNS_DIR:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15/bonus}"
one() { local t="$1" d="$OUT/run$t"; rm -rf "${d:?}"; mkdir -p "$d/run" "$d/out"
  for f in "$SET"/*; do b="$(basename "$f")"; case "$b" in EBOOT.PBP|scenario.txt|smokelog) ;; *) ln -s "$f" "$d/run/$b";; esac; done
  cp "$EB" "$d/run/EBOOT.PBP"; touch "$d/run/smokelog"; printf 3 > "$d/run/meaudio"
  rm -f "$d/run/bonus"; mkdir -p "$d/run/bonus"; for f in "$SET"/bonus/*; do ln -s "$f" "$d/run/bonus/$(basename "$f")"; done   # bonus/ — своя папка: рядом кладутся линии секторов .sec (набор integ не трогаем)
  for f in "${SEC_DIR:-$ROOT/build/secdata}"/b0?.sec; do cp "$f" "$d/run/bonus/$(basename "$f")"; done
  { echo "flag menu"; echo "flag bonusopen"; echo "flag bonuslaps 1"; echo "flag bonusauto 1"; echo "flag bonustrack $t"; echo "wait 2"; echo "press left"; echo "wait .6"; echo "press cross"; echo "wait 8"
    echo "flag bonusgo"; echo "wait 9"; echo "shot sector_b$t"; echo "wait 1"; echo "flag bonuscode 1"; echo "wait 1"; echo "shot code_b$t"; echo "end"; } > "$d/run/scenario.txt"
  ( cd "$d/run" && env TWINS_SMOKE="$d/run/smoke.txt" TWINS_SHOTDIR="$d/out" SDL_AUDIODRIVER=dummy XDG_CONFIG_HOME="$d/out/config" XDG_DATA_HOME="$d/out/data" "$BIN" --graphics=software --timeout=1200 "$d/run/EBOOT.PBP" > "$d/out/log.txt" 2>&1 ); }
for t in 1 2 3 4 5 6 7; do while [ "$(jobs -rp | wc -l | tr -d ' ')" -ge "${BONUS_JOBS:-2}" ]; do sleep 3; done; one "$t" & done; wait
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
[ -x build/bonus1167/bin/sector_host ] || tests/bonus1167/sector.sh >/dev/null 2>&1
SEC_DIR="${SEC_DIR:-$ROOT/build/secdata}" build/bonus1167/bin/sector_host -d "$BNS"/b0{1,2,3,4,5,6,7}.bns > "$OUT/boundaries.txt"
"$ROOT/.venv/bin/python" -I tests/bonus1167/sector_sheet.py "$OUT" || python3 -I tests/bonus1167/sector_sheet.py "$OUT"
