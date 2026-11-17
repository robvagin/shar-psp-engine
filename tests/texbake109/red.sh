#!/bin/bash
# Красные контроли запечённых признаков решётки и неба (109 З8): испорченное значение с ПЕРЕСЧИТАННЫМ хешем записи проходит проверку хеша, поэтому ловить его обязан режим сверки texbakecheck:
#   tests/texbake109/red.sh SET   SET — набор с EBOOT 109 и texbake.shr уровня 1 в корне (как для tools/texbake_gate.sh); уровень 1
# 1) запись 5: признак решётки fl[12] перевёрнут -> texbakecheck даёт LATBAKE bad>=1; 2) хвост SKY1: acc[0] += 1 -> SKYBAKE bad>=1. Файлы SET не меняются (работа в копии).
set -u; cd "$(dirname "$0")/../.."; SET="$(cd "$1" && pwd)"; BAD=0; T=$(mktemp -d /tmp/tb109.XXXXXX); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/set"; for f in "$SET"/*; do ln -s "$f" "$T/set/$(basename "$f")"; done; rm -f "$T/set/texbake.shr"
python3 - "$SET/texbake.shr" "$T/lat.shr" "$T/sky.shr" <<'PY'
import struct, sys
def fnv(d, h=2166136261):
    for (w,) in struct.iter_unpack('<I', bytes(d)): h = ((h ^ w) * 16777619) & 0xFFFFFFFF
    return h
b = bytearray(open(sys.argv[1], 'rb').read()); n = struct.unpack_from('<I', b, 8)[0]
o = 32 + 5 * 1056   # запись: in(4) out(4) ts(8) fl(16) pal(1024)
c = bytearray(b); c[o + 16 + 12] = 2 if c[o + 28] == 1 else 1
struct.pack_into('<I', c, o + 4, fnv(c[o + 16:o + 32], fnv(c[o + 8:o + 16], fnv(c[o + 32:o + 32 + 1024]))))
open(sys.argv[2], 'wb').write(c)
t = 32 + n * 1056; d = bytearray(b)
assert d[t:t + 4] == b'SKY1', 'нет хвоста SKY1'
v = struct.unpack_from('<f', d, t + 8)[0]; struct.pack_into('<f', d, t + 8, v + 1.0); struct.pack_into('<I', d, t + 44, fnv(d[t:t + 44]))
open(sys.argv[3], 'wb').write(d)
PY
for k in lat sky; do
  cp "$T/$k.shr" "$T/set/texbake.shr"
  printf 'flag level 1\nscene red\nwait 8\nend\n' > "$T/s.txt"
  EX=""; for f in flat912.bin marks912.bin notoon.txt zones1007.bin zones1007p.bin; do [ -f "$SET/$f" ] && EX="$EX $f"; done
  R=$(SMOKE_EXTRA="$EX texbake.shr" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_FLAGS=texbakecheck SMOKE_SCN="$T/s.txt" SMOKE_MAXT=300 tools/smoke.sh "$T/set" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p'); case "$R" in /*) ;; *) R="$PWD/$R" ;; esac
  if [ $k = lat ]; then L=$(grep -a '^LATBAKE' "$R/smoke.txt" | head -1); bad=$(echo "$L" | sed -n 's/.* bad=\([0-9]*\).*/\1/p'); else L=$(grep -a '^SKYBAKE' "$R/smoke.txt" | tail -1); bad=$(echo "$L" | sed -n 's/.* bad=\([0-9]*\).*/\1/p'); fi
  [ "${bad:-0}" -ge 1 ] 2>/dev/null && echo "красный контроль $k: пойман ($L)" || { echo "красный контроль $k: НЕ пойман ($L)"; BAD=1; }
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
