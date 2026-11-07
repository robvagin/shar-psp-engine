#!/bin/sh
# Сборка уровня целиком: экспорт с диска (tools/psp_export.py) + предрасчёт видимости PVS (tools/pvs)
# + обход треугольников под отсечение задних граней (tools/wind).
#   tools/build_level.sh              -> build/level.shr
#   tools/build_level.sh /path/x.shr  -> /path/x.shr (build/level.shr не трогается)
# Параметры PVS через переменную окружения (по умолчанию "--samples 9": 9x9 точек глаза на клетку вместо 5x5,
# в проверке ноль пропусков ближе 60 м, списки на ~2% длиннее, считается ~3 раза дольше), например:
#   PVS_ARGS="" tools/build_level.sh               -> 5x5, быстрее
#   PVS_ARGS="--samples 9 --dilate" ...            -> + объединение с 8 соседними клетками
# Уровень: LEVEL=2 tools/build_level.sh (по умолчанию 1).
# Качество картинки (закон 1 docs/handoff/LOOP_TERMINAL.md): по умолчанию SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1,
# как в tools/build_all_levels.sh; три флага печатаются в лог. Снижать только по слову Роба.
# Проверка результата: python tools/pvs_check.py OUT
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/tools/_ci${PYTHONPATH:+:$PYTHONPATH}"   # Linux/WSL: поиск файлов диска без учёта регистра (tools/ci_fs.py); на macOS ничего не меняет
OUT="${1:-}"
case "$OUT" in ""|/*) ;; *) OUT="$PWD/$OUT" ;; esac
cd "$ROOT"
export SHR_TEX_CAP256="${SHR_TEX_CAP256:-1}" SHR_SMALL_MIPS="${SHR_SMALL_MIPS:-1}" SHR_LAYERS="${SHR_LAYERS:-1}"
# правка Роба 129 (01.10): SHR_T4=1 — текстуры в 16 цветов 4 битами (уровень 0 без потерь), SHR_SIGN_CAP=1 — вывески 128 px
export SHR_T4="${SHR_T4:-1}" SHR_SIGN_CAP="${SHR_SIGN_CAP:-1}"
# №1162/№1166 («чужой экспорт = наш»): уровни нашего набора (TEST81 + правки поверх) собраны рецептом reissue_level.sh: дальняя версия без нарезки (SHR_FAR_PLAIN=1),
# правило №488 выключено (SHR_PROP_OWNED_OLD=1), тип поверхности земли — отдельным файлом surf-l<N>.shr
# (SHR_SURF_OLD=1: блока SRF1 в COL1 нет, хеш проб машин миссий mcpr.shr и lmap остаются верными). Публичная сборка по умолчанию давала другой level.shr
# (COL1, PRP2, CAR1, PVS1 отличались: стены и коллизия, на которых стоит камера). Теперь те же значения по умолчанию; явно заданная переменная сильнее
# (reissue_level.sh и прежние вызовы не меняются). BUILD_LEVEL1162_OLD=1 — прежние умолчания (всё выключено)
if [ "${BUILD_LEVEL1162_OLD:-0}" != 1 ]; then
  export SHR_FAR_PLAIN="${SHR_FAR_PLAIN-1}" SHR_PROP_OWNED_OLD="${SHR_PROP_OWNED_OLD-1}" SHR_SURF_OLD="${SHR_SURF_OLD-1}"
fi
echo "качество: SHR_TEX_CAP256=$SHR_TEX_CAP256 SHR_SMALL_MIPS=$SHR_SMALL_MIPS SHR_LAYERS=$SHR_LAYERS; память: SHR_T4=$SHR_T4 SHR_SIGN_CAP=$SHR_SIGN_CAP"

# pvs: пересобрать, если бинарника нет или исходник новее
if [ ! -x tools/pvs ] || [ tools/pvs.c -nt tools/pvs ]; then
  cc -O3 -o tools/pvs tools/pvs.c -lm
fi
. .venv/bin/activate
# №1162: наши правки в чужом разборщике диска (прозрачность вершин PS2); HRW_PATCH1162_OLD=1 — как было
python tools/hrw_patches/apply.py || exit 1

if [ -n "$OUT" ]; then
  # без --out экспортёр молча записал бы build/level.shr
  grep -q -- "--out" tools/psp_export.py || { echo "psp_export.py не знает --out: не пишу в $OUT" >&2; exit 1; }
  python tools/psp_export.py "${LEVEL:-1}" --out "$OUT"
else
  OUT="$ROOT/build/level.shr"
  python tools/psp_export.py "${LEVEL:-1}"
fi
# shellcheck disable=SC2086
# правка 497: списки PVS до 250 м (режим 30 FPS), а не до 115 — без них за 115 м город идёт по пирамиде без закрытия домами (L2 на PSP
# 51 тыс. треугольников против 30 на L1). PVS_RANGE_ARG="" — прежние списки до 115 м (движок читает дальность из заголовка PVS1)
tools/pvs "$OUT" ${PVS_ARGS---samples 9} ${PVS_RANGE_ARG---range 250}
# обход треугольников под отсечение задних граней (голосование видами, ~10 с): разворот + флаг 8 «двусторонний»
if [ ! -x tools/wind ] || [ tools/wind.c -nt tools/wind ]; then
  cc -O3 -o tools/wind tools/wind.c -lm -lpthread
fi
tools/wind "$OUT" ${WIND_ARGS-}
# правка 508: SHR_FAR_PLAIN=1 — у кусков есть дальняя версия без нарезки (флаг F_FARP); второй проход обхода голосует ТОЛЬКО за неё (ближние версии, а с
# ними результат выключенного far_plain, не меняются), с глаз дальше FAR_PLAIN_WIND_D (по умолчанию 60 м; движок берёт её дальше D из файла far_plain, D >= 60)
if [ "${SHR_FAR_PLAIN:-0}" = "1" ]; then tools/wind "$OUT" --far-d "${FAR_PLAIN_WIND_D:-60}" --far-only; fi
# загрузка 2, п.3: порядок секций «одним проходом» — резидент, COL1, PRP2, CAR1, REG1, PVS1, потом блобы клеток (tools/shr_order.py).
# Последним шагом: tools/pvs читает PVS1 в конце файла. SHR_ORDER_OLD=1 — прежний порядок (клетки перед COL1)
if [ "${SHR_ORDER_OLD:-0}" != 1 ]; then python3 tools/shr_order.py "$OUT"; fi
# К4 (ПЛАН_ОПТИМИЗАЦИЙ 17.10 п.1): SHR_STRIPS=1 — куски города неиндексными полосами (tools/strips.c, последним шагом, после порядка секций; нужен shr_order). Без флага файл побайтно прежний.
# Проверка: python3 tools/strips_check.py СПИСОК.shr ПОЛОСЫ.shr (те же треугольники и обход). Движок читает оба вида (флаг куска 1<<26).
if [ "${SHR_STRIPS:-0}" = "1" ]; then
  [ "${SHR_ORDER_OLD:-0}" != 1 ] || { echo "SHR_STRIPS=1 требует порядок секций shr_order (SHR_ORDER_OLD=0)" >&2; exit 1; }
  tools/strips.sh >/dev/null
  cp "$OUT" "$OUT.list"; tools/strips "$OUT.list" "$OUT"; rm -f "$OUT.list"
fi
# №1166: build_level.sh собирает ТОЛЬКО level.shr (город и коллизия). Для игры нужен весь набор (камеры зон, комнаты, трафик, Гомер, звук, интерфейс): tools/ship.sh
echo "build_level: готов только level.shr; полный набор для PSP (камеры, комнаты, трафик, интерфейс, звук) собирает tools/ship.sh" >&2
