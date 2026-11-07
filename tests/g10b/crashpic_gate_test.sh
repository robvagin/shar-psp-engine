#!/bin/bash
# Красные контроли гейта tools/crashpic_gate.sh (№1208б): зелёный — модуль с настоящей картинкой; красные — старый модуль без картинки, подпорченная картинка, заглушка с --final.
set -u; T="$(cd "$(dirname "$0")/../.." && pwd)"; K="$T/viewer/kexc"; G="$T/tools/crashpic_gate.sh"; W=$(mktemp -d); ok=1
cp "$K/crashpic_placeholder.shx" "$K/crashpic.shx"   # временно: «настоящая» картинка
( cd "$K" && make clean >/dev/null 2>&1 && make >/dev/null 2>&1 ); cp "$K/sharexc.prx" "$W/good.prx"; rm -f "$K/crashpic.shx"; ( cd "$K" && make clean >/dev/null 2>&1 )
"$G" "$W/good.prx" --final | tail -1 | grep -q '^PASS' || { echo "ЗЕЛЁНЫЙ не прошёл"; ok=0; }
python3 - "$W/good.prx" "$W/bad.prx" <<'PY'
import sys; d=bytearray(open(sys.argv[1],'rb').read()); i=d.find(b'SHXC'); d[i+200]^=0xff; open(sys.argv[2],'wb').write(d)
PY
"$G" "$W/bad.prx" --final >/dev/null && { echo "КРАСНЫЙ (подпорчена картинка) не пойман"; ok=0; }
head -c 14446 /dev/zero > "$W/none.prx"; "$G" "$W/none.prx" --final >/dev/null && { echo "КРАСНЫЙ (нет картинки) не пойман"; ok=0; }
( cd "$K" && make SRCPNG=/nonexistent >/dev/null 2>&1 ); cp "$K/sharexc.prx" "$W/ph.prx"; ( cd "$K" && make clean >/dev/null 2>&1 )
"$G" "$W/ph.prx" --final >/dev/null && { echo "КРАСНЫЙ (заглушка при --final) не пойман"; ok=0; }
[ $ok = 1 ] && { echo "PASS: зелёный проходит, три красных падают"; exit 0; } || { echo FAIL; exit 1; }
