#!/usr/bin/env python3
"""Снимок одного экрана интерфейса (макет на Маке, как ui_export mock) для листа: python3 tests/lang1111/shot.py ЭКРАН ВЫХОД.png [--hide-reset]. Язык — LANG_BIBLE; --hide-reset: без служебной строки RESET CAR (как в игре)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import ui_export as U

def main():
    screen, out = sys.argv[1], sys.argv[2]
    items, icons = U.build_items(screen); state = dict(U.DEMO_STATE); sel = 0 if screen in ('pause', 'pausemsn') else -1
    if screen in ('pause', 'pausemsn'): state['show'] = {'COINS', 'OBJECTIVE'}
    rmap = U.bake_radar_map(); rmap['rgba'] = U.radar_map_rgba(rmap)
    pa = U.Assets(); pf = U.FontCache('psp'); dl, _ = U.adapt(items, icons, 'psp', pa, pf)
    if '--hide-reset' in sys.argv:
        dl = [t for t in dl if not (t.get('name') == 'ResetCar' or t.get('path', '').endswith('ResetCar') or 'ResetCar' in t.get('path', ''))]
    U.render(dl, U.DST_W, U.DST_H, state, pa.bitmaps, U.backdrop(U.DST_W, U.DST_H, screen), rmap, sel).save(out)

if __name__ == '__main__':
    main()
