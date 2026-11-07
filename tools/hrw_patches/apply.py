#!/usr/bin/env python3
"""№1162: наши правки в разборщике диска hit-and-run-web (MIT, Vheissu; его ставят с GitHub отдельно, в нашем репозитории его нет).

Правка «alpha»: прозрачность вершин PS2 (4-й байт цвета, 0x80 = 1.0) уходит в атрибут alpha (tools/convert.py). Без неё стекло
(например окна Krusty, 51 = 40 %) выходит непрозрачным: tools/psp_export.py читает именно этот атрибут. Правка наша, три строки;
чужой код не копируется, текст замены задан здесь же.
  tools/hrw_patches/apply.py          накладывает недостающее (повтор ничего не меняет)
  tools/hrw_patches/apply.py --check  только проверить: выход 0 = наложено, 1 = нет, 2 = файл другой версии
HRW_PATCH1162_OLD=1 — ничего не делать (прежнее: чужой hit-and-run-web как есть)"""
import os, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONV = ROOT / 'hit-and-run-web/tools/convert.py'
EDITS = [   # (старый текст, новый текст); маркер «наложено» — новый текст целиком
    ("                current['color']=np.minimum(np.frombuffer(data,dtype=np.uint8).reshape(-1,4)[:,:3].astype(np.float32)/128,1)\n",
     "                rgba=np.frombuffer(data,dtype=np.uint8).reshape(-1,4)\n"
     "                current['color']=np.minimum(rgba[:,:3].astype(np.float32)/128,1)\n"
     "                current['alpha']=np.minimum(rgba[:,3:4].astype(np.float32)/128,1)   # PS2: 0x80 = 1.0 (стекло Krusty 51 = 40%)\n"),
    ("                    if all('normal' in part for part in parts):attrs['normal']=",
     "                    if any('alpha' in part for part in parts):attrs['alpha']=buffer(np.concatenate([part.get('alpha',np.ones((len(part['position']),1),dtype=np.float32)) for part in parts]).astype('<f4'))\n"
     "                    if all('normal' in part for part in parts):attrs['normal']="),
]


def main():
    if os.environ.get('HRW_PATCH1162_OLD') == '1': return 0
    check = '--check' in sys.argv
    if not CONV.exists():
        print(f'hrw_patches: нет {CONV} (поставь hit-and-run-web, см. README)'); return 2
    s = CONV.read_text(encoding='utf-8'); todo = 0
    for old, new in EDITS:
        if new in s: continue
        if s.count(old) != 1:
            print('hrw_patches: convert.py другой версии, место правки не найдено'); return 2
        s = s.replace(old, new); todo += 1
    if check: return 1 if todo else 0
    if todo: CONV.write_text(s, encoding='utf-8'); print(f'hrw_patches: convert.py — наложено правок: {todo}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
