#!/bin/bash
# №1166: сторож данных камеры (tools/camera_data_gate.py) зелёный на полном наборе и красный на каждой порче: нет файла камеры, пустой файл, COL1 обнулена, level.shr обрезан.
#   tests/camera/camdata_gate.sh НАБОР_С_УРОВНЕМ_1 [level.shr для красных проб]      НАБОР — папка игры (EBOOT не нужен): level.shr, zonecam-l1.shr, carzone-l1.shr, msncam.shr, camcut-l1.shr, interiors-l1(.-gfx).bin
set -u; cd "$(dirname "$0")/../.."; S=$1; LV=${2:-$S/level.shr}; T=build/camdata_gate.$$; bad=0; PY=${PY:-python3}
$PY tools/camera_data_gate.py "$S" --levels 1 > /dev/null && echo "OK   полный набор: зелёный" || { echo "FAIL полный набор не прошёл"; $PY tools/camera_data_gate.py "$S" --levels 1; bad=1; }
red() {   # red ОПИСАНИЕ: набор в $T должен дать провал
  if $PY tools/camera_data_gate.py "$T" --levels 1 > /dev/null; then echo "FAIL красный не сработал: $1"; bad=1; else echo "OK   красный: $1"; fi; }
mk() { rm -rf $T; mkdir -p $T; for f in "$S"/*; do b=$(basename "$f"); ln -s "$(cd "$(dirname "$f")" && pwd -P)/$b" "$T/$b"; done; }
for n in zonecam-l1.shr carzone-l1.shr msncam.shr camcut-l1.shr interiors-l1.bin interiors-l1-gfx.bin level.shr; do mk; rm "$T/$n"; red "нет $n"; done
mk; rm "$T/zonecam-l1.shr"; : > "$T/zonecam-l1.shr"; red "пустой zonecam-l1.shr"
mk; rm "$T/level.shr"; $PY - "$LV" "$T/level.shr" <<'PYEOF'
import struct, sys
b = bytearray(open(sys.argv[1], 'rb').read()); off = struct.unpack_from('<I', b, 92)[0]; struct.pack_into('<I', b, off + 24, 0)   # ntri = 0
open(sys.argv[2], 'wb').write(b)
PYEOF
red "COL1 без треугольников"
mk; rm "$T/level.shr"; head -c 2000000 "$LV" > "$T/level.shr"; red "level.shr обрезан (COL1 вне файла)"
rm -rf $T; exit $bad
