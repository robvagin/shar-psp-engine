#!/bin/bash
# №1208б гейт: в sharexc.prx вшита картинка экрана вылета. Использование: tools/crashpic_gate.sh <sharexc.prx> [--final]
#   проверяет: метка SHXPIC1:<хеш> в модуле, картинка SHXC целиком в модуле (размеры, палитра, длина), хеш картинки = метка; --final (публичная сборка и набор TEST для Роба): заглушка (PLACEHOLDER) не допускается.
#   Код 0 — PASS; иначе первая строка — причина. Красные контроли: tests/g10b/crashpic_gate_test.sh
set -u; PRX=${1:?нужен sharexc.prx}; FINAL=0; [ "${2:-}" = --final ] && FINAL=1
[ -f "$PRX" ] || { echo "FAIL: нет файла $PRX"; exit 1; }
python3 - "$PRX" "$FINAL" <<'PY'
import hashlib, re, struct, sys
d = open(sys.argv[1], 'rb').read(); final = sys.argv[2] == '1'
m = re.search(rb'SHXPIC1:([0-9a-fA-F]{12}|PLACEHOLDER)\x00', d)
if not m: print('FAIL: в модуле нет метки SHXPIC1 (картинка не вшита)'); sys.exit(1)
tag = m.group(1).decode()
i = d.find(b'SHXC')
if i < 0 or i + 12 > len(d): print('FAIL: в модуле нет картинки SHXC'); sys.exit(1)
w, h, nc, z = struct.unpack('<HHHH', d[i + 4:i + 12]); n = 12 + nc * 4 + (w * h if z == 0 else 4 + (struct.unpack('<I', d[i + 12 + nc * 4:i + 16 + nc * 4])[0] if z == 1 and i + 16 + nc * 4 <= len(d) else 10**9))
if not (0 < w <= 480 and 0 < h <= 272 and 0 < nc <= 256) or i + n > len(d): print('FAIL: картинка в модуле битая (%dx%d, %d цветов)' % (w, h, nc)); sys.exit(1)
if tag == 'PLACEHOLDER':
    if final: print('FAIL: в модуле заглушка картинки (PLACEHOLDER); нужен crashpic.shx (viewer/kexc/crashpic.shx)'); sys.exit(1)
    print('PASS (заглушка, не для выпуска): %dx%d, %d цветов, модуль %d байт' % (w, h, nc, len(d))); sys.exit(0)
hh = hashlib.sha256(d[i:i + n]).hexdigest()[:12]
if hh != tag.lower(): print('FAIL: хеш картинки %s не равен метке %s' % (hh, tag)); sys.exit(1)
print('PASS: %dx%d, %d цветов, метка %s, модуль %d байт' % (w, h, nc, tag, len(d)))
PY
