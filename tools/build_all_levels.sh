#!/bin/bash
# Все уровни одной командой, локально, без ИИ: те же программы, что собирали уровень 1.
#   tools/build_all_levels.sh            -> уровни 1..7 в build/levels/lN/
#   tools/build_all_levels.sh 2 3        -> только уровни 2 и 3
# На каждый уровень: level.shr (город, коллизия, пропы, монеты, машина, PVS — tools/build_level.sh),
# audio/ (tools/sound_export.py), missions/ (tools/missions/compile.py). Общие для всех: homer.shr, ui.shr, fx.shr
# (fx_pack.py пока знает только l01_fx — эффекты уровней 2–7 отдельным шагом).
# Итог по каждому шагу — build/levels/report.txt (OK / FAIL + последняя строка ошибки). Упавший шаг не мешает остальным.
# Время: ~6–8 мин на уровень (PVS — самая долгая часть). Ресурсы игры никуда не отправляются.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
export PYTHONPATH="$ROOT/tools/_ci${PYTHONPATH:+:$PYTHONPATH}"   # Linux/WSL: поиск файлов диска без учёта регистра (tools/ci_fs.py); на macOS ничего не меняет
source .venv/bin/activate
export SHR_TEX_CAP256=${SHR_TEX_CAP256:-1} SHR_SMALL_MIPS=${SHR_SMALL_MIPS:-1} SHR_LAYERS=${SHR_LAYERS:-1}
LEVELS=${*:-1 2 3 4 5 6 7}
OUT=build/levels; mkdir -p "$OUT"; REP="$OUT/report.txt"
echo "сборка $(date '+%F %T'), уровни: $LEVELS" > "$REP"
step() {   # step УРОВЕНЬ ИМЯ команда...
  local L=$1 N=$2; shift 2; local LOG="$OUT/l$L/$N.log"
  if "$@" > "$LOG" 2>&1; then echo "l$L $N: OK" | tee -a "$REP"
  else echo "l$L $N: FAIL — $(grep -v '^\s*$' "$LOG" | tail -1)" | tee -a "$REP"; FAILED="$FAILED l$L:$N"; fi
}
for L in $LEVELS; do
  mkdir -p "$OUT/l$L"
  echo "== уровень $L"
  step "$L" level env LEVEL="$L" tools/build_level.sh "$ROOT/$OUT/l$L/level.shr"
  step "$L" sound python tools/sound_export.py --level "$L" --out "$OUT/l$L/audio"
  step "$L" missions python tools/missions/compile.py --level "$L" --out "$OUT/l$L/missions"
done
echo "готово: $REP"; cat "$REP"
if [ -n "$FAILED" ]; then echo "НЕ СОБРАНО:$FAILED"; exit 1; fi
