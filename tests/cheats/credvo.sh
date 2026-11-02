#!/bin/bash
# №1104: пакет fe_audio.sap с репликами титров (tools/credvo_export.py + fe_audio_export.py). Проверка структуры и красный CREDVO_OLD=1 (без реплик). Нужен диск игры (hit-and-run-web/source/game).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/cheats; mkdir -p "$OUT"
check() { python3 - "$1" "$2" <<'P'
import struct, sys
b = open(sys.argv[1], 'rb').read(); want = int(sys.argv[2]); bad = []
if b[:4] != b'SAP1': bad.append('нет SAP1')
n = struct.unpack_from('<I', b, 8)[0]
rows = [struct.unpack_from('<28sIIIIIHBBhh4h', b, 64 + 64 * i) for i in range(n)]
names = [r[0].split(b'\0')[0].decode() for r in rows]
kk = [x for x in names if x.startswith('KK_')]
if n != want: bad.append('дорожек %d, ждали %d' % (n, want))
if len(kk) != (31 if want > 12 else 0): bad.append('реплик титров %d' % len(kk))
if len(set(names)) != n: bad.append('повтор имён')
for r in rows:
    nm = r[0].split(b'\0')[0].decode()
    if r[7] != 2 or r[8] != 0 or r[9] != -1 or r[10] != 2 or r[6] < 8000 or r[2] * 32 + r[1] > len(b) or r[1] % 2048 or r[3] != r[2] * 28 or r[1] < 64 + 64 * n: bad.append('дорожка ' + nm + ' не по правилам движка')
print('OK %d дорожек, реплик титров %d, %d Б' % (n, len(kk), len(b)) if not bad else 'FAIL ' + '; '.join(bad[:4]))
sys.exit(1 if bad else 0)
P
}
python3 tools/fe_audio_export.py --out $OUT/fe_audio_new.sap >/dev/null || { echo "FAIL: экспорт"; exit 1; }
CREDVO_OLD=1 python3 tools/fe_audio_export.py --out $OUT/fe_audio_old.sap >/dev/null || { echo "FAIL: экспорт (старый)"; exit 1; }
echo -n "зелёный: "; check $OUT/fe_audio_new.sap 43; g=$?
echo -n "красный CREDVO_OLD=1 (12 дорожек, а ждём 43): "; if check $OUT/fe_audio_old.sap 43 >/dev/null; then echo "НЕ ПОЙМАН"; r=1; else echo "упал, как должен"; r=0; fi
[ $g = 0 ] && [ $r = 0 ] && echo PASS || { echo FAIL; exit 1; }
