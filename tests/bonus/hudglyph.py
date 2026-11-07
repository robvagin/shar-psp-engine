#!/usr/bin/env python3
"""TEST90: грань bart_16bit@1 основного пакета (ui.shr) должна нести знаки, которые ставит HUD бонус-гонки на языке игры: заглавные с диакритикой и знаки места (2ÈME, 1º, 3ª).
В урезанном атласе их не было — вместо них рисовался «?». Читает таблицу граней пакета (viewer/ui.c UiFont: 16 байт заголовка + 256 знаков по 8 байт, флаг знака — последний байт).
  python3 tests/bonus/hudglyph.py ui.shr      код 1 — знака нет      (красный контроль: пакет, собранный с BONUSHUDCH_OLD=1, или ui.shr из TEST89)"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
v = struct.unpack_from('<19I', d, 4)
nfont, offFont, nstr, offStr = v[7], v[8], v[9], v[10]
NEED = {'È': 0xC8, 'É': 0xC9, 'º': 0xBA, 'ª': 0xAA, 'A': 0x41, '1': 0x31, '/': 0x2F}
bad = 0; found = False
for i in range(nfont):
    o = offFont + i * (16 + 256 * 8); tex, lh, base, pad, name, res = struct.unpack_from('<4HII', d, o)
    so = struct.unpack_from('<I', d, offStr + 4 * name)[0] if name < nstr else 0   # strOff[i] — смещение строки i в пакете
    nm = d[so: so + 40].split(b'\0')[0].decode('latin-1', 'replace')
    if not nm.startswith('bart_16bit'):
        continue
    found = True
    miss = [c for c, code in NEED.items() if not d[o + 16 + code * 8 + 7] & 1]
    print('%s %-16s нет знаков: %s' % ('FAIL' if miss else 'ok  ', nm, ''.join(miss) or '-')); bad |= bool(miss)
if not found:
    print('FAIL грань bart_16bit не найдена'); bad = 1
print('FAIL' if bad else 'PASS'); sys.exit(bad)
