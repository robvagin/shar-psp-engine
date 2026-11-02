#!/usr/bin/env python3
"""№1111: таблица «подпись на языке -> английская подпись» для строк меню, которые движок находит по английской подписи (viewer/lang.c langKey, main_save.h rowOf, main_levels.h главное меню).
Пары берутся из готовых экранов пакетов (menu_export, MENU_ROWS_OUT): одна и та же строка (экран, номер) на английском и на языке. Строки порта (Height, Car Shadows, Curbs …) английские на всех языках и в таблицу не входят.
  LANG_BIBLE=F python3 tools/lang_rowkeys.py --out build/lang/F/rowkeys.txt --rows E.json F.json ...   (строка файла: ПОДПИСЬ_НА_ЯЗЫКЕ<TAB>ENGLISH, заглавными)"""
import struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_export as U

MAIN_LABELS = ['NEW GAME', 'OPTIONS', 'LOAD GAME', 'RESUME GAME', 'BONUS GAME', 'SCRAP BOOK']   # главное меню: строки ищутся целиком (main_levels.h)

def table(lang):
    root = U.read(U.SCROOBY / 'resource/txtbible/srr2.p3d')
    for c in U.walk(root):
        if c.id == 0x1800E:
            _, p = U.string(c.data)
            if c.data[p:p + 1].decode() != lang: continue
            n, mod, size = struct.unpack_from('<III', c.data, p + 1); p += 13
            hs = struct.unpack_from('<%dI' % n, c.data, p); of = struct.unpack_from('<%dI' % n, c.data, p + 4 * n); buf = c.data[p + 8 * n:]
            return {h: buf[o:].decode('utf-16-le', errors='replace').split('\0')[0] for h, o in zip(hs, of)}

def main():
    """--out rowkeys.txt --rows E.json L.json [E2.json L2.json ...]: пары подписей одних и тех же строк (экран, номер строки) на английском и на языке; плюс пункты главного меню по тексту диска"""
    import json, accents
    out = sys.argv[sys.argv.index('--out') + 1]; args = sys.argv[sys.argv.index('--rows') + 1:] if '--rows' in sys.argv else []
    pairs = {}
    for e, l in zip(args[0::2], args[1::2]):
        E = json.loads(Path(e).read_text(encoding='utf-8')); L = json.loads(Path(l).read_text(encoding='utf-8'))
        for sc, rows in E.items():
            for r, en in rows.items():
                loc = (L.get(sc) or {}).get(r)
                if loc and en and loc.upper() != en.upper(): pairs.setdefault(loc, en.upper())   # подпись на языке — как в пакете (регистр знаков не меняем: строка байт сравнивается без регистра только для ASCII)
    Eb, Xb = table('E'), table(U.LANG)
    for lab in MAIN_LABELS:
        hs = [h for h, v in Eb.items() if v.upper() == lab]
        if hs and Xb.get(hs[0]) and Xb[hs[0]].upper() != lab: pairs.setdefault(Xb[hs[0]], lab)
    lines = [accents.font_bytes(k) + b'\t' + v.encode('latin-1', 'replace') for k, v in sorted(pairs.items())]
    Path(out).parent.mkdir(parents=True, exist_ok=True); Path(out).write_bytes(b'\n'.join(lines) + b'\n')
    print(len(lines), 'строк', out)

if __name__ == '__main__':
    main()
