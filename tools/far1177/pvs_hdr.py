"""Дальность PVS в заголовке level.shr (проверка набора перед выпуском, п. 7 разбора arch-fable).
  python3 -I tools/far1177/pvs_hdr.py ПАПКА [ПАПКА ...]      ПАПКА: набор (level.shr + l2..l7/level.shr) или build/levels (l1..l7/level.shr)
Заголовок PVS1 (32 Б, @88 файла указывает на него): 'PVS1', 7 слов; последнее слово — дальность списков, м (0 — 115, правка 497; движок читает pvsRange из него).
Выход 1, если у какого-то уровня дальность < 250 (такой набор рисует за 115 м всё в пирамиде без закрытия домами). Размер пула PVS — слово 6."""
import sys, struct, os, time
def hdr(p):
    d = open(p, 'rb').read(1 << 20) if False else open(p, 'rb').read(); off, = struct.unpack_from('<I', d, 88)
    if not off or d[off:off + 4] != b'PVS1': return None
    return struct.unpack_from('<4s7I', d, off)
bad = 0
for root in sys.argv[1:]:
    print(root)
    for n in range(1, 8):
        for p in (root + ('/level.shr' if n == 1 else '/l%d/level.shr' % n), root + '/l%d/level.shr' % n):
            if os.path.exists(p): break
        else: print('  L%d нет файла' % n); continue
        h = hdr(p); mt = time.strftime('%d.%m %H:%M', time.localtime(os.stat(p).st_mtime))
        if not h: print('  L%d без PVS1 (%s)' % (n, mt)); bad += 1; continue
        rng = h[7] if h[7] else 115
        print('  L%d дальность PVS %3d м%s, пул %.2f МБ, файл от %s' % (n, rng, '' if h[7] else ' (поле 0)', h[6] / 1048576, mt)); bad += rng < 250
print('PASS' if not bad else 'FAIL: уровней с дальностью PVS < 250 м: %d' % bad); sys.exit(1 if bad else 0)
