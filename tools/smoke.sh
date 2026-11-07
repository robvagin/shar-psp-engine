#!/bin/bash
# Смоук-тест в эмуляторе одним запуском PPSSPP (через общую очередь tools/ppsspp_run.sh).
#   tools/smoke.sh [--negative] [DIR]   DIR = где лежат EBOOT.PBP и данные (по умолчанию build/)
#   --negative                отрицательные контроли (tools/smoke_negative.txt): длинный кадр, задержанная клетка,
#                             испорченное чтение — проходит, только если проверки поймали КАЖДУЮ подброшенную ошибку
#   SMOKE_SCN=файл            другой сценарий (по умолчанию tools/smoke_scenario.txt)
#   SMOKE_FLAGS="a b"         положить рядом с EBOOT пустые служебные файлы-флаги a, b (например nolayer)
#   SMOKE_FILEDATA="a=1 b=u"  то же с содержимым (первый байт читают флаг-файлы ME: meaudio=3 — пустышка ME в эмуляторе, правка 410)
#   SMOKE_OPTS=hex            opts.txt рядом с EBOOT — маска переключателей движка (viewer/main.c O_*; 2017f — без неба, HUD; правка 106)
#   SMOKE_BALLAST=N           файл ballast рядом с EBOOT: движок занимает N МБ до игры — свободной кучи как у настоящей PSP
#                             (эмулятор отдаёт больше; правка 103, viewer/smoke.h smokeInit)
#   SMOKE_PPSSPPLOG=1         журнал PPSSPP в build/smoke/<время>/ppsspp.log (--log): там «Bad memory access … ignored» —
#                             обращения по плохому адресу, которые эмулятор молча пропускает, а PSP на них падает (правка 103)
#   SMOKE_HEADLESS=1          тот же сценарий и smoke_report, без окна/звука: PPSSPPHeadless, software render
#   SMOKE_MAXT=с              предел времени прогона вместо «сумма wait + 90 с» (уровни 2–7 грузятся дольше)
#   blackbox*.txt и sfxlog.txt движка после прогона копируются в build/smoke/<время>/ (правка 103)
# Что делает: копирует EBOOT + нужные данные в свою чистую папку build/emu/smoke/<время>-<pid>/ (без чужих служебных
# файлов; параллельные прогоны не мешают друг другу; после прогона папка удаляется),
# кладёт рядом scenario.txt + smokelog; движок (viewer/smoke.h) сам проходит сцены (спавн, езда прямо, дым,
# остов, ящик, дальний вид 30 FPS, 60 FPS), пишет раз в секунду FPS/work/cpu/tris/draws/RAM в smoke.txt,
# а на строках "SHOT имя" ppsspp_run.sh снимает окно. Итог: build/smoke/<время>/ — summary.txt, contact.png,
# снимки, smoke.txt. Код выхода 0 = OK; иначе первая строка вывода — причина.
# Провал (подробно — tools/smoke_report.py): упала/зависла, FPS ниже цели (60 в view60, 30 в long; допуск 2%), кадр длиннее
# целевого периода, пропуски периода, видимые недогруженные клетки, ошибки чтения/памяти.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/portable.sh"
# эмулятор (доработка noname8723 для Ubuntu, 06.11): macOS — окно PPSSPPSDL.app (tools/ppsspp_run.sh); Linux и SMOKE_HEADLESS=1 — PPSSPPHeadless:
# $PPSSPP_HEADLESS, build/twins/bin, общий build/twins/bin основной копии или PATH. На Linux безголовый режим включается сам
find_headless() {
  local c
  for c in "${PPSSPP_HEADLESS:-}" "$ROOT/build/twins/bin/PPSSPPHeadless" "$HOME/Downloads/shar-psp/build/twins/bin/PPSSPPHeadless" "$(command -v PPSSPPHeadless 2>/dev/null)"; do
    [ -n "$c" ] && [ -x "$c" ] && { echo "$c"; return 0; }
  done; return 1
}
[ "$(uname -s)" = Darwin ] || SMOKE_HEADLESS=1
if [ "${1:-}" = --have-emulator ]; then   # ship.sh: есть ли чем гнать смоук на этой машине (код 0 — да)
  if [ "${SMOKE_HEADLESS:-0}" = 1 ]; then find_headless >/dev/null; exit $?; else [ -d /Applications/PPSSPPSDL.app ]; exit $?; fi
fi
[ "${SMOKE_HEADLESS:-0}" != 1 ] || { PPSSPP_HEADLESS="$(find_headless)" || { echo "smoke: нет PPSSPPHeadless (PPSSPP_HEADLESS=/путь/к/PPSSPPHeadless)"; exit 1; }; export PPSSPP_HEADLESS; }
NEG=""; if [ "${1:-}" = --negative ]; then NEG=--negative; shift; SMOKE_SCN="${SMOKE_SCN:-$ROOT/tools/smoke_negative.txt}"; fi
SRC="${1:-$ROOT/build}"; case "$SRC" in /*) ;; *) SRC="$PWD/$SRC" ;; esac
SCN="${SMOKE_SCN:-$ROOT/tools/smoke_scenario.txt}"
STAMP=$(date +%Y%m%d-%H%M%S)
RUN="$ROOT/build/emu/smoke/$STAMP-$$"
OUT="$ROOT/build/smoke/$STAMP"; [ -e "$OUT" ] && OUT="$OUT-$$"
[ -f "$SRC/EBOOT.PBP" ] || { echo "smoke: нет $SRC/EBOOT.PBP"; exit 1; }

mkdir -p "$RUN" "$OUT"; trap 'rm -rf "$RUN"' EXIT
cp -c "$SRC/EBOOT.PBP" "$RUN/" 2>/dev/null || cp "$SRC/EBOOT.PBP" "$RUN/"
# Наборы уровней 2–7 (l<N>/…) и ролики целей fmv (movies/fmv*) — из DIR, иначе с места сборки (tools/ship_files.sh --pairs:
# build/levels/l<N>/…); нет нигде — уровень в этом прогоне недоступен (меню его пропускает), смоук уровня 1 идёт. Прочие — обязательны
while IFS='|' read -r f p; do
  s="$SRC/$f"
  if [ ! -f "$s" ]; then
    case "$f" in
      l[0-9]/*|movies/fmv*|bonus/*|far_plain|mcpr.shr|lmap.shr|colbake.shr|pv2s.shr|horizon.shr) s="$ROOT/$p"; [ -f "$s" ] || continue ;;   # horizon.shr (Горизонт) — по желанию: нет файла — горизонт тихо выкл   # правка 420: трассы бонус-игры — по желанию (нет — MINI GAME серый)
      *) s="$ROOT/$p"; [ -f "$s" ] || { echo "smoke: нет $SRC/$f"; exit 1; } ;;   # 30.09: иначе с места сборки (level01.msn — build/missions/)
    esac
  fi
  mkdir -p "$RUN/$(dirname "$f")" || exit 1
  cp -c "$s" "$RUN/$f" 2>/dev/null || cp "$s" "$RUN/$f" || exit 1
done < <("$ROOT/tools/ship_files.sh" --pairs)
for q in stripbench.bin level_toon.shr toonlines.shr pv2s.shr l1/pv2s.shr l2/pv2s.shr l3/pv2s.shr l4/pv2s.shr l5/pv2s.shr l6/pv2s.shr l7/pv2s.shr horizon.shr l1/horizon.shr l2/horizon.shr l3/horizon.shr l4/horizon.shr l5/horizon.shr l6/horizon.shr l7/horizon.shr hlod.shr l1/hlod.shr l2/hlod.shr l3/hlod.shr l4/hlod.shr l5/hlod.shr l6/hlod.shr l7/hlod.shr; do [ -f "$SRC/$q" ] && { mkdir -p "$RUN/$(dirname "$q")"; cp "$SRC/$q" "$RUN/$q"; }; done   # TEST24: PVS по курсу (tools/pvs2.py --section) — по желанию, как far_plain: есть в наборе — кладём
if [ -n "$NEG" ]; then   # полный сценарий + отрицательные контроли одним запуском
  { grep -v '^end' "$ROOT/tools/smoke_scenario.txt"; cat "$SCN"; } > "$OUT/scenario_full.txt"; SCN="$OUT/scenario_full.txt"
fi
for f in ${SMOKE_EXTRA:-}; do [ -f "$SRC/$f" ] && { mkdir -p "$RUN/$(dirname "$f")"; cp "$SRC/$f" "$RUN/$f"; }; done   # №679: SMOKE_EXTRA="toonlines.shr l2/toonlines.shr" — необязательные файлы данных из DIR (контур мультика)
cp "$SCN" "$RUN/scenario.txt"; touch "$RUN/smokelog"
for f in ${SMOKE_FLAGS:-}; do touch "$RUN/$f"; done   # SMOKE_FLAGS="nolayer near07 …" — служебные файлы-флаги рядом с EBOOT (сравнение до/после)
for kv in ${SMOKE_FILEDATA:-}; do printf "%s" "${kv#*=}" > "$RUN/${kv%%=*}"; done   # правка 410: SMOKE_FILEDATA="meaudio=3 mescheme=u" — файл с содержимым
for kv in ${SMOKE_SAVEFILES:-}; do cp "${kv%%=*}" "$RUN/${kv#*=}"; done   # №682: SMOKE_SAVEFILES="/путь/save03.bin=save00.bin" — готовое сохранение в слот рядом с EBOOT (запись файлом, без утилиты)
[ -n "${SMOKE_OPTS:-}" ] && printf "%s" "$SMOKE_OPTS" > "$RUN/opts.txt"   # правка 106: замер clearchk без неба (tools/seams_emu.py)
[ -n "${SMOKE_BALLAST:-}" ] && printf "%s" "$SMOKE_BALLAST" > "$RUN/ballast"   # правка 103: куча как у PSP
LOGARG=(); [ -n "${SMOKE_PPSSPPLOG:-}" ] && LOGARG=("" "--log=$OUT/ppsspp.log")   # пустой 3-й аргумент — без снимка окна в конце
# максимум времени: сумма wait + 90 с на загрузку и снимки
MAXT=$(awk '$1=="wait"{s+=$2} $1=="shot"{s+=1} END{print int(s)+90}' "$SCN")
[ -n "${SMOKE_MAXT:-}" ] && MAXT=$SMOKE_MAXT   # SMOKE_MAXT=с — свой предел (загрузка уровней 2–7 в безголовом эмуляторе дольше 90 с)
T0=$(date +%s)
if [ "${SMOKE_HEADLESS:-0}" = 1 ]; then
  python3 "$ROOT/tools/smoke_headless.py" "$RUN" "$OUT" "$MAXT"
else
  PPSSPP_SHOTS="$RUN/smoke.txt" PPSSPP_SHOTDIR="$OUT" "$ROOT/tools/ppsspp_run.sh" "$RUN/EBOOT.PBP" "$MAXT" ${LOGARG[@]+"${LOGARG[@]}"}   # bash 3.2 + set -u: пустой массив без ошибки
fi
# 04.11 (gates): паспорт прогона для tools/run_outcome.py — TIMEOUT эмулятора под нагрузкой Мака отличается от зависания игры (wall_s против maxt, gap_s = сколько секунд журнал не рос до обрыва)
T1=$(date +%s); MT=$(file_mtime "$RUN/smoke.txt" 2>/dev/null || echo "$T0")
echo "wall_s=$((T1 - T0)) maxt=$MAXT gap_s=$((T1 - MT)) t0=$T0 t1=$T1 load1=$(sysctl -n vm.loadavg | tr -d '{}' | awk '{print $1}') ncpu=$(sysctl -n hw.ncpu)" > "$OUT/run_meta.txt"
cp "$RUN/stall.txt" "$OUT/" 2>/dev/null
cp "$RUN/smoke.txt" "$OUT/" 2>/dev/null; cp "$RUN/acct.txt" "$OUT/" 2>/dev/null   # учёт ACCT (viewer/acct.h) — рядом с журналом
for f in blackbox.txt blackbox_prev.txt sfxlog.txt stall.txt shar_report.txt bootmark.txt texbake_L1.out texbake_L2.out texbake_L3.out texbake_L4.out texbake_L5.out texbake_L6.out texbake_L7.out colbake_L1.out colbake_L2.out colbake_L3.out colbake_L4.out colbake_L5.out colbake_L6.out colbake_L7.out; do [ -f "$RUN/$f" ] && cp "$RUN/$f" "$OUT/"; done   # правка 103: чёрный ящик, журнал звуков
for f in "$RUN"/rfsnap_*.bin; do [ ! -f "$f" ] || cp "$f" "$OUT/" || exit 1; done   # retained RF frame evidence, before owned RUN cleanup
. "$ROOT/.venv/bin/activate"
python "$ROOT/tools/smoke_report.py" "$OUT" "$SCN" $NEG; RC=$?
# 04.11 (gates): SMOKE_AUDIO=1 (или список сцен «A,B») — звук действительно играл (tools/audio_alive.py: AUDV/SNDV самого движка, не флаги); провал звука = провал смоука. Без переменной — прежнее
if [ -n "${SMOKE_AUDIO:-}" ]; then
  python3 "$ROOT/tools/audio_alive.py" "$OUT" ${SMOKE_AUDIO_SCENES:+--scenes "$SMOKE_AUDIO_SCENES"} > "$OUT/audio.txt" 2>&1; ARC=$?; cat "$OUT/audio.txt"
  [ $ARC = 0 ] || { echo "смоук: звук не доказан (audio_alive код $ARC)" >> "$OUT/summary.txt"; [ $RC = 0 ] && RC=1; }
fi
[ "$SCN" = "$OUT/scenario_full.txt" ] || cp "$SCN" "$OUT/scenario.txt"
echo "отчёт: ${OUT#$ROOT/}/summary.txt, листы: ${OUT#$ROOT/}/contact.png"
exit $RC
