#!/bin/bash
# Круг 6, g08-sound: правила звука по строкам. Зелёный вариант проходит, каждый красный (один *_OLD=1) краснеет именно на своих строках.
# Мак, CPU, без эмулятора. Нужен пакет звука уровня: PACK=build/snd6/l1/ (экспорт tools/sound_export.py --level 1 --out build/snd6/l1)
set -u
cd "$(dirname "$0")/../.."
export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"
PACK="${PACK:-build/snd6/l1/}"; O=build/g08sound; mkdir -p "$O"
build() { cc -std=c11 -O1 -w -DAUDIO_HOST $2 -I viewer tests/g08sound/rules.c viewer/audio.c -lm -o "$O/$1" || exit 2; }
build green ""
"$O/green" "$PACK" > "$O/green.txt"; green=$?
grep -c '^OK' "$O/green.txt" | sed 's/^/зелёный: проверок OK = /'
[ $green -eq 0 ] || { grep '^FAIL' "$O/green.txt"; echo "FAIL: зелёный вариант не прошёл"; exit 1; }
bad=0
# флаг : часть имени проверки, которая обязана покраснеть
while IFS=: read -r flag mask; do
  build "red_$flag" "-D$flag=1"
  "$O/red_$flag" "$PACK" > "$O/red_$flag.txt"
  n=$(grep '^FAIL' "$O/red_$flag.txt" | grep -cE -- "$mask")
  if [ "$n" -ge 1 ]; then echo "красный $flag: покраснело $n по «${mask}»"; else echo "FAIL: красный $flag не покраснел по «${mask}»"; bad=1; fi
done <<'LIST'
SV_CRASH_OLD:P09:25
NEWSMUS1128_OLD:№1128
BOOTHMUTE1135_OLD:№1135
SV_MASS_OLD:P09:25 (лёгкая|тяжёлая)
SV_PICKUP_OLD:P09:23
SV_SAYTAB_OLD:P09:8 Turbo
SV_SAYTAB_OLD:P09:12
SV_SAYQ7_OLD:P09:12 круг 7
SV_CRASH7_OLD:P09:25 круг 7
SV_SAYTAB_OLD:P09:5 Mcrash
SV_MODEMIX_OLD:M2:Д1
SV_MODEMIX_OLD:M1:Д4
SV_FADE7_OLD:P09:15 круг 7
SV_AMBFOOT_OLD:P09:22
SV_ROOM_OLD:P09:17
LIST
# подвижные объекты: лифт идёт потоком на L2, люк и платформы — из банка на L1
build green2 ""
"$O/green2" build/snd6/l2/ > "$O/green_l2.txt" || { grep '^FAIL' "$O/green_l2.txt"; echo "FAIL: зелёный L2"; bad=1; }
grep -c '^OK' "$O/green_l2.txt" | sed 's/^/зелёный L2: проверок OK = /'
build red_anim "-DANIMOBJ_RANGE_OLD=1"
"$O/red_anim" build/snd6/l2/ > "$O/red_anim.txt"; n=$(grep '^FAIL' "$O/red_anim.txt" | grep -c "P05:8"); if [ "$n" -ge 1 ]; then echo "красный ANIMOBJ_RANGE_OLD (L2): покраснело $n по «P05:8»"; else echo "FAIL: красный ANIMOBJ_RANGE_OLD не покраснел"; bad=1; fi
# мотор и затухание (engine.c подключает audio.c)
buildE() { cc -std=c11 -O1 -w -DAUDIO_HOST $2 -I viewer tests/g08sound/engine.c -lm -o "$O/$1" || exit 2; }
buildE eng_green ""
"$O/eng_green" "$PACK" > "$O/eng_green.txt" || { grep '^FAIL' "$O/eng_green.txt"; echo "FAIL: зелёный мотор не прошёл"; exit 1; }
grep -c '^OK' "$O/eng_green.txt" | sed 's/^/зелёный мотор: проверок OK = /'
while IFS=: read -r flag mask; do
  buildE "eng_$flag" "-D$flag=1"
  "$O/eng_$flag" "$PACK" > "$O/eng_$flag.txt"
  n=$(grep '^FAIL' "$O/eng_$flag.txt" | grep -cE -- "$mask")
  if [ "$n" -ge 1 ]; then echo "красный $flag: покраснело $n по «${mask}»"; else echo "FAIL: красный $flag не покраснел по «${mask}»"; bad=1; fi
done <<'LIST2'
ENGINE_SV_OLD:P10-10
ENGINE_SV_OLD:P10-29
ENGINE_SV_OLD:P10-6
ENGINE_SV_OLD:P10-16
ENGINE_SV_OLD:P10 Д9
WORLD_ENGINE_SV_OLD:P10-1
SV_STOPFADE_OLD:P10-25
SV_SKIDPITCH_OLD:P10-12
SV_BURNPITCH_OLD:P10-11 круг 7
SV_STOPFADE7_OLD:P10-25 круг 7
LIST2
[ $bad -eq 0 ] && echo "PASS: g08sound круг 6 — зелёный проходит, красные краснеют" || { echo "FAIL"; exit 1; }
