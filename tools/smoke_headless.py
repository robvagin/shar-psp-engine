#!/usr/bin/env python3
"""Run the existing ship smoke scenario without a window or audible output."""
import os, shutil
from pathlib import Path
import subprocess
import sys
from PIL import Image

def main():
    run, out = (Path(p).resolve() for p in sys.argv[1:3])
    timeout = int(sys.argv[3])
    # $PPSSPP_HEADLESS (его выставляет tools/smoke.sh), build/twins/bin, общий эмулятор основной копии, PATH (Linux: свой PPSSPPHeadless; доработка noname8723, 06.11)
    binary = Path(os.environ['PPSSPP_HEADLESS']) if os.environ.get('PPSSPP_HEADLESS') else Path(__file__).resolve().parents[1] / 'build/twins/bin/PPSSPPHeadless'
    if not binary.is_file(): binary = Path.home() / 'Downloads/shar-psp/build/twins/bin/PPSSPPHeadless'   # рабочие копии (git worktree) без своего build/twins: общий безголовый эмулятор основной копии
    if not binary.is_file() and shutil.which('PPSSPPHeadless'): binary = Path(shutil.which('PPSSPPHeadless'))
    if not binary.is_file():
        raise SystemExit('smoke: missing headless emulator; no GUI fallback')
    env = dict(os.environ, TWINS_SMOKE=str(run/'smoke.txt'),
               TWINS_SHOTDIR=str(out), SDL_AUDIODRIVER='dummy',
               XDG_CONFIG_HOME=os.environ.get('SMOKE_XDG_CONFIG') or str(out/'config'), XDG_DATA_HOME=str(out/'data'))   # SMOKE_XDG_CONFIG — свой каталог конфигурации эмулятора (сохранения для проверки)
    cmd = [str(binary), '--graphics=software', f'--timeout={timeout}', str(run/'EBOOT.PBP')]
    if os.environ.get('SMOKE_HOME'):   # свой HOME эмулятора: сохранения этого прогона лежат там, личные карты не трогаются (гейт tests/levels/news_levels.sh)
        sh = Path(os.environ['SMOKE_HOME']); sh.mkdir(parents=True, exist_ok=True); env['HOME'] = str(sh)   # папку PSP эмулятор создаёт сам (заготовленная .config/ppsspp ломает запись: SAVEDATA_RW_MEMSTICK_FULL)
    if os.environ.get('SMOKE_PPSSPPLOG'): cmd.insert(1, '-l')
    with (out/'headless.log').open('w') as log:
        result = subprocess.run(cmd, cwd=run, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=timeout+30)
    for frame in sorted(out.glob('*.bmp')):
        # The emulator dumps PSP framebuffer stride (512px); visible display is 480px.
        with Image.open(frame) as image:
            image.convert('RGB').crop((0, 0, 480, 272)).save(frame.with_suffix('.png'))
    # smoke_report.py checks END, scenes, timings, reads, memory and pictures.
    # Exit status alone never certifies a completed smoke scenario.
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
