#!/bin/bash
# Общий безголовый раннер сценария (TEST60, 24.10): вместо раннеров окон ~/.claude/jobs/shar1023/tmp/agentN/run/run5x.sh (папка удалена при уборке 24.10).
# Подпись та же, что у них и у tests/ctrl916/run.sh:  tools/emu_run.sh СЦЕНАРИЙ ИМЯ EBOOT [МАКС_С]  ->  $OUT/out_ИМЯ/smoke.txt (+ снимки, blackbox*.txt)
#   гейты с RUN=${RUN:-$A/run/run56.sh}: запускать с RUN=<дерево>/tools/emu_run.sh OUT=$A/run (итог там же, где гейт его ищет: $A/run/out_ИМЯ)
# Окружение:
#   SET        набор данных (по умолчанию тур TEST59: ~/Downloads/shar-psp-integ/build/test59tour/PSP/GAME/SHARTEST15), копируется клоном APFS
#   OVL        папка-наложение данных поверх набора, те же относительные пути (horizon.shr, l3/horizon.shr, flat912.bin …); данные игры в git не кладём
#   OUT        рабочая папка (по умолчанию $PWD/emu_out); в ней свой HOME эмулятора emu_home_ИМЯ — сейвы прогона не трогают карты Роба
#   FLAGS      служебные файлы рядом с EBOOT: "a b" — пустые, "a=1" — с содержимым
#   EMU_SLOTS  общий предел эмуляторов на Мак (по умолчанию 10; замки /tmp/shar_emu_slots/N), EMU_MINE — предел одного хозяина (по умолчанию 2),
#   EMU_OWNER  имя хозяина (по умолчанию имя папки OUT). Лимит Роба 24.10: не больше 6 на окно и около 12 на Мак, иначе каждый прогон в 5–8 раз дольше
#   KEEP_RR=1  не удалять рабочую копию набора после прогона
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
set -u
[ $# -ge 3 ] || { echo "emu_run.sh СЦЕНАРИЙ ИМЯ EBOOT [МАКС_С]" >&2; exit 2; }
SCN=$1; NAME=$2; EB=$3; MAXT=${4:-700}
I="$(cd "$(dirname "$0")/.." && pwd)"
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test59tour/PSP/GAME/SHARTEST15}
OUT=${OUT:-$PWD/emu_out}; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
OWNER=${EMU_OWNER:-$(basename "$OUT")}; SLOTS=${EMU_SLOTS:-10}; MINE=${EMU_MINE:-2}
[ -f "$SET/EBOOT.PBP" ] || { echo "нет набора данных $SET (SET=...)" >&2; exit 2; }
[ -f "$SCN" ] || { echo "нет сценария $SCN" >&2; exit 2; }
[ -f "$EB" ] || { echo "нет EBOOT $EB" >&2; exit 2; }
# место в общей очереди: замок-папка /tmp/shar_emu_slots/N с PID и хозяином; мёртвый PID освобождает место
SD=/tmp/shar_emu_slots; mkdir -p "$SD"; MY=""
trap '[ -n "$MY" ] && rm -rf "$MY"' EXIT
while :; do
  tot=0; mine=0
  for s in "$SD"/*; do [ -d "$s" ] || continue
    p=$(cat "$s/pid" 2>/dev/null); if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then tot=$((tot + 1)); [ "$(cat "$s/owner" 2>/dev/null)" = "$OWNER" ] && mine=$((mine + 1))
    elif [ -n "$p" ] || [ $(( $(date +%s) - $(stat -f %m "$s") )) -gt 10 ]; then rm -rf "$s"; fi   # без pid дольше 10 с — брошенный замок
  done
  if [ $tot -lt "$SLOTS" ] && [ $mine -lt "$MINE" ]; then
    for n in $(seq 1 "$SLOTS"); do if mkdir "$SD/$n" 2>/dev/null; then echo $$ > "$SD/$n/pid"; echo "$OWNER" > "$SD/$n/owner"; MY="$SD/$n"; break; fi; done
    [ -n "$MY" ] && break
  fi
  sleep 3
done
R=$OUT/rr_$NAME; O=$OUT/out_$NAME; rm -rf "$R" "$O"; mkdir -p "$O" "$OUT/emu_home_$NAME"
cp -c -R "$SET" "$R" || { echo "не смог скопировать набор" >&2; exit 1; }
[ -n "${OVL:-}" ] && { cp -c -R "$OVL"/. "$R"/ || { echo "не смог наложить $OVL" >&2; exit 1; }; }
cp "$EB" "$R/EBOOT.PBP"; rm -f "$R"/smoke.txt "$R"/blackbox*.txt; cp "$SCN" "$R/scenario.txt"; touch "$R/smokelog"
for f in ${FLAGS:-}; do case "$f" in *=*) printf '%s' "${f#*=}" > "$R/${f%%=*}";; *) : > "$R/$f";; esac; done
SMOKE_HOME=$OUT/emu_home_$NAME python3 "$I/tools/smoke_headless.py" "$R" "$O" "$MAXT" > /dev/null 2>&1
[ -s "$R/smoke.txt" ] || { echo "прогон $NAME не дал smoke.txt" >&2; [ "${KEEP_RR:-0}" = 1 ] || rm -rf "$R"; exit 1; }
cp "$R/smoke.txt" "$O/"; cp "$R"/blackbox*.txt "$O/" 2>/dev/null
[ "${KEEP_RR:-0}" = 1 ] || rm -rf "$R"
grep -q "^END" "$O/smoke.txt" && echo "done $NAME" || { echo "done $NAME (без END: зависание или предел времени)"; exit 3; }
