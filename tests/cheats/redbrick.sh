#!/bin/bash
# №1104, код Red Brick: пакет машин уровня 1 (tools/msncar_export.py) содержит машину redbrick; с REDBRICK_PACK_OLD=1 — нет (красный). Нужны диск игры и .venv проекта (~35 с на пакет).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/cheats; mkdir -p "$OUT"
PY=python3; for c in "$ROOT/.venv/bin/python" "$HOME/Downloads/shar-psp/.venv/bin/python"; do [ -x "$c" ] && PY="$c" && break; done
mklink=0; [ -e donut ] || { ln -s "$HOME/Downloads/shar-psp/donut" donut 2>/dev/null && mklink=1; }
has() { "$PY" - "$1" <<'P'
import struct, sys
b = open(sys.argv[1], 'rb').read(); m, v, lv, n, cat = struct.unpack_from('<4sIIII', b, 0)
names = [b[cat + 176 * i:cat + 176 * i + 16].split(b'\0')[0].decode().lower() for i in range(n)]
print('redbrick' in names)
P
}
"$PY" tools/msncar_export.py --level 1 --out $OUT/msncars-l1-new.shr >/dev/null 2>&1; g=$(has $OUT/msncars-l1-new.shr 2>/dev/null)
REDBRICK_PACK_OLD=1 "$PY" tools/msncar_export.py --level 1 --out $OUT/msncars-l1-old.shr >/dev/null 2>&1; r=$(has $OUT/msncars-l1-old.shr 2>/dev/null)
[ $mklink = 1 ] && rm -f donut
echo "зелёный: redbrick в пакете = $g (ждём True); красный REDBRICK_PACK_OLD=1: $r (ждём False)"
[ "$g" = True ] && [ "$r" = False ] && echo PASS || { echo FAIL; exit 1; }
