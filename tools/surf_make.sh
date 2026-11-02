#!/bin/sh
# Файл поверхности surf-l<N>.shr для набора (круг 6, g12-data; правило tools/ship_rules.txt): экспорт уровня N тем же рецептом, что у TEST81 (reissue_level.sh +
# флаги качества build_level.sh), из него таблица типов поверхности треугольников и отпечаток COL1 ГОТОВОГО level.shr набора (если таблицы треугольников
# разные — отказ, файл не пишется). Запуск: tools/surf_make.sh N BASE_LEVEL_SHR OUT_FILE. SURF_SIDE_MAKE_OLD=1 — ничего не писать.
set -e
cd "$(dirname "$0")/.."
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
[ "$SURF_SIDE_MAKE_OLD" = 1 ] && { echo "SURF_SIDE_MAKE_OLD=1: файл не пишется"; exit 1; }
N=$1; BASE=$2; OUT=$3; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
[ -f .venv/bin/activate ] && . .venv/bin/activate
env SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 SHR_T4=1 SHR_SIGN_CAP=1 SHR_PROP_OWNED_OLD=1 SHR_FENCE_ROOF_OLD=1 python tools/psp_export.py "$N" --out "$T/new.shr" >"$T/log" 2>&1 || { tail -5 "$T/log"; exit 1; }
python tools/sv_overlay.py --level "$N" --base "$BASE" --new "$T/new.shr" --surf-only --out "$OUT"
