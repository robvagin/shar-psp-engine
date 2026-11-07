#!/bin/bash
# Гейт «КАДР ИГРОКА» (слово Роба 16.10: «поставь себе проверку»): безголовый путь игрока на сохранении Роба и проверка, что кадр ВИДЕН, а не только сосчитан.
#   tools/frame_gate.sh EBOOT.PBP НАБОР [ВЫХОД]
#   EBOOT — тур-конфигурация сборки (SHAR_RELEASE + KEEP_DIAG + KEEP_SMOKE: сценарий и журнал смоука); НАБОР — папка с данными (как build/test23_emu: level.shr, l2…l7, звук, интерфейс)
# Путь 1 (tools/frame_gate_scenario.txt): RESUME (сохранение Роба в ~/.ppsspp/PSP/SAVEDATA, flag nosave — тур ничего не пишет) → гараж у АЭС → посадка (flag repair) →
# езда по улице у дома с прохожими и трафиком. Путь 2 (tools/frame_gate_new.txt): NEW GAME, 35 с игры. Проверки и пороги — tools/frame_gate.py.
# Итог — одна строка «КАДР ИГРОКА: PASS|FAIL …» и лист кадров ВЫХОД/frame_gate_sheet.png; код выхода 0 только при PASS. Эмулятор только безголовый, без звука.
# 04.11 (gates): TIMEOUT эмулятора под нагрузкой Мака больше не «упал/завис». Пределы времени растут с load (FG_LOAD_SCALE=1|2|3 вручную; по умолчанию load/ядра ≥ 2 → ×3, ≥ 1 → ×2),
#   исход каждого прогона определяет tools/run_outcome.py: ENDED / TIMEOUT / HANG / NOSTART / EXITED. TIMEOUT повторяется один раз с пределом ×2; если и тогда END нет, а игра шла до обрыва —
#   строка «КАДР ИГРОКА: TIMEOUT, не зависание · не измерено» и код выхода 3 (не PASS и не FAIL: набор не засчитан, повторить при меньшей нагрузке). HANG/NOSTART/EXITED — FAIL (код 1).
# GATE_EXPECT_FAIL="а,б" — отрицательный контроль: строка PASS, если упали именно эти проверки (EBOOT TEST25 f0551e4b обязан упасть по а и б).
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
set -u
cd "$(dirname "$0")/.."; ROOT=$PWD
EB=${1:?EBOOT.PBP}; PK=${2:?НАБОР (папка данных)}; OUT=${3:-$ROOT/build/frame_gate/$(date +%H%M%S)}
[ -f "$EB" ] || { echo "КАДР ИГРОКА: FAIL нет EBOOT $EB"; exit 2; }; [ -d "$PK" ] || { echo "КАДР ИГРОКА: FAIL нет набора $PK"; exit 2; }
mkdir -p "$OUT" "$ROOT/build"; TMP="$ROOT/build/gate_pkg_$$"
cleanup() { [ -n "${TMP:-}" ] && [ -d "$TMP" ] && rm -rf "${TMP:?}"; }; trap cleanup EXIT
SAV="$HOME/.ppsspp/PSP/SAVEDATA"
if [ "${FG_POST_ONLY:-0}" != 1 ]; then
cp -cR "$PK" "$TMP" 2>/dev/null || cp -R "$PK" "$TMP"; cp "$EB" "$TMP/EBOOT.PBP"
[ -d "$SAV" ] && find "$SAV" -type f -exec shasum {} \; | sort > "$OUT/savedata_before.sha"
fi
# №1114 (перевод записей): плоские save??.bin в наборе, если есть, переводятся в SHV3; записи эмулятора (SAVEDATA, шифр PPSSPP) не трогаем — игра переводит прежнюю запись сама при чтении (SAVEV3CONV_OLD=0), в журнале строка SVCONV
SAVEV3CONV_GATE=${SAVEV3CONV_GATE:-1}
[ "$SAVEV3CONV_GATE" = 1 ] && [ "${FG_POST_ONLY:-0}" != 1 ] && python3 "$ROOT/tools/save_v3_convert.py" --dir "$TMP" > "$OUT/save_convert.txt" 2>&1
LOAD1=$(sysctl -n vm.loadavg | tr -d '{}' | awk '{print $1}'); NCPU=$(sysctl -n hw.ncpu)
FG_LOAD_SCALE=${FG_LOAD_SCALE:-$(awk -v l="$LOAD1" -v n="$NCPU" 'BEGIN{r=l/n; print (r>=2)?3:((r>=1)?2:1)}')}
echo "load1=$LOAD1 ncpu=$NCPU масштаб пределов x$FG_LOAD_SCALE" > "$OUT/load.txt"
# предел: FG_*_MAXT, заданный вручную, берётся как есть; иначе 220/160 с x масштаб нагрузки (24.10: боты, load 50 — TIMEOUT на сцене, а не зависание)
run1() {   # run1 ИМЯ СЦЕНАРИЙ МАКС_С
  ( SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$2" SMOKE_MAXT=$3 nice -n 20 tools/smoke.sh "$TMP" ) > "$OUT/$1.out" 2>&1
  D="$ROOT/build/smoke/$(ls -t "$ROOT/build/smoke" | head -1)"; rm -rf "${OUT:?}/$1"; cp -R "$D" "$OUT/$1"   # 24.10: без rm повторный запуск клал прогон ВНУТРЬ старой папки, а проверялись файлы прошлой попытки
  python3 "$ROOT/tools/run_outcome.py" "$OUT/$1" > "$OUT/$1.outcome"; echo $? > "$OUT/$1.outcome.rc"
  echo "$1 $(sed -n 's/.*t0=\([0-9]*\) t1=\([0-9]*\).*/\1 \2/p' "$OUT/$1/run_meta.txt")" >> "$OUT/windows.txt"   # окно эмулятора этого прогона (эпоха, с): кто писал в SAVEDATA, видно по времени записи
}
run() {   # run ИМЯ СЦЕНАРИЙ МАКС_С: при TIMEOUT — один повтор с пределом x2
  run1 "$1" "$2" "$3"; rc=$(cat "$OUT/$1.outcome.rc")
  if [ "$rc" = 3 ]; then   # предел повтора — по измеренной скорости эмуляции (игровых секунд на секунду), не меньше x2
    R2=$(python3 "$ROOT/tools/run_outcome.py" "$OUT/$1" --suggest "$2"); echo "$1: $(cat "$OUT/$1.outcome") — повтор с пределом $R2 с" >> "$OUT/retries.txt"; run1 "$1" "$2" "$R2"; fi
}
[ "${FG_POST_ONLY:-0}" = 1 ] || {   # FG_POST_ONLY=1 — не запускать эмулятор, а разобрать готовую папку ВЫХОД (проверка разбора на старых прогонах)
run resume "$ROOT/tools/frame_gate_scenario.txt" ${FG_RESUME_MAXT:-$((220 * FG_LOAD_SCALE))}   # FG_*_MAXT — больше при загруженном Маке (24.10: боты, load 50 — TIMEOUT на сцене, а не зависание)
grep -rhE "SVCONV|SVSCAN|SVFILE" "$OUT/resume" 2>/dev/null | sort -u | head -8 > "$OUT/save_conv_log.txt"   # доказательство: прежняя запись Роба прочитана (O/E/D в SVSCAN, строка SVCONV)
run new "$ROOT/tools/frame_gate_new.txt" ${FG_NEW_MAXT:-$((160 * FG_LOAD_SCALE))}
}
[ "${FG_POST_ONLY:-0}" = 1 ] || { [ -d "$SAV" ] && find "$SAV" -type f -exec shasum {} \; | sort > "$OUT/savedata_after.sha"; }
# исходы прогонов: не ENDED — проверки не запускаем (на обрубленном журнале они бы мерили не игру)
for n in resume new; do
  k=$(cat "$OUT/$n.outcome.rc"); [ "$k" = 0 ] && continue
  if [ "$k" = 3 ]; then echo "КАДР ИГРОКА: TIMEOUT, не зависание · не измерено ($n: $(cat "$OUT/$n.outcome"); load $LOAD1/$NCPU, предел x$FG_LOAD_SCALE, повтор был) · набор не засчитан, повторить при меньшей нагрузке" | tee "$OUT/frame_gate.txt"; exit 3; fi
  echo "КАДР ИГРОКА: FAIL · игра не дошла до конца ($n: $(cat "$OUT/$n.outcome"))" | tee "$OUT/frame_gate.txt"; exit 1
done
# 04.11: звук путём игрока действительно играл (tools/audio_alive.py по AUDV/SNDV журнала); при FG_AUDIO_OLD=1 — прежнее (звук не смотрим)
python3 "$ROOT/tools/frame_gate.py" "$OUT" ${GATE_EXPECT_FAIL:+--expect-fail "$GATE_EXPECT_FAIL"}; GRC=$?
ARC=0
if [ "${FG_AUDIO_OLD:-0}" != 1 ]; then
  python3 "$ROOT/tools/audio_alive.py" "$OUT/resume" --scenes FG_garage,FG_seat,FG_street,FG_static > "$OUT/audio.txt" 2>&1; ARC=$?
  head -1 "$OUT/audio.txt"; [ -z "${GATE_EXPECT_FAIL:-}" ] && [ $ARC != 0 ] && { echo "КАДР ИГРОКА: FAIL · звук не играл ($(head -1 "$OUT/audio.txt")); подробно $OUT/audio.txt"; GRC=1; }
fi
# 04.11: SAVEDATA общий у всех сессий на этом Маке (~/.ppsspp): запись чужого эмулятора не должна краснить гейт «сохранение Роба цело». Изменённый файл с временем записи ВНЕ окон эмуляторов этого прогона
# (windows.txt) — чужая запись: гейт не может ни засчитать, ни забраковать набор — «не измерено», код 3 (повторить). FG_FOREIGN_OLD=1 — прежнее (любое различие = FAIL е)
if [ $GRC != 0 ] && [ "${FG_FOREIGN_OLD:-0}" != 1 ] && [ -f "$OUT/windows.txt" ] && [ $ARC = 0 ] && [ "$(head -1 "$OUT/frame_gate.txt" | grep -oE '(а|б|в|г|д|е) FAIL' | tr -d ' \n')" = "еFAIL" ]; then
  FOREIGN=0; OWN=0
  for f in $(diff "$OUT/savedata_before.sha" "$OUT/savedata_after.sha" | sed -n 's/^[<>] [0-9a-f]*  //p' | sort -u); do
    [ -e "$f" ] || { OWN=1; continue; }; mt=$(stat -f %m "$f"); in=0
    while read -r _ a b; do [ -n "$a" ] && [ "$mt" -ge "$a" ] && [ "$mt" -le "$b" ] && in=1; done < "$OUT/windows.txt"
    [ $in = 1 ] && OWN=1 || FOREIGN=1
  done
  if [ $FOREIGN = 1 ] && [ $OWN = 0 ]; then echo "КАДР ИГРОКА: SAVEDATA изменена ЧУЖОЙ записью (время записи вне окон эмуляторов этого прогона), остальные проверки ok · не измерено, повторить" | tee "$OUT/frame_gate.txt"; exit 3; fi
fi
exit $GRC
